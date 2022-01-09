/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2015 Google Inc.
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
 * \brief Simple Smoke Tests
 *//*--------------------------------------------------------------------*/

#include "vktApiTests.hpp"

#include "vktTestCaseUtil.hpp"

#include "vkDefs.hpp"
#include "vkPlatform.hpp"
#include "vkStrUtil.hpp"
#include "vkRef.hpp"
#include "vkRefUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkDeviceUtil.hpp"
#include "vkPrograms.hpp"
#include "vkTypeUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"

#include "tcuTestLog.hpp"
#include "tcuFormatUtil.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuImageCompare.hpp"

#include "rrRenderer.hpp"

#include "deUniquePtr.hpp"

namespace vkt
{
namespace api
{

namespace
{

using namespace vk;
using std::vector;
using tcu::TestLog;
using de::UniquePtr;

tcu::TestStatus createSamplerTest (Context& context)
{
	const VkDevice			vkDevice	= context.getDevice();
	const DeviceInterface&	vk			= context.getDeviceInterface();

	{
		const struct VkSamplerCreateInfo		samplerInfo	=
		{
			VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,		// sType
			DE_NULL,									// pNext
			0u,											// flags
			VK_FILTER_NEAREST,							// magFilter
			VK_FILTER_NEAREST,							// minFilter
			VK_SAMPLER_MIPMAP_MODE_NEAREST,				// mipmapMode
			VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,		// addressModeU
			VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,		// addressModeV
			VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,		// addressModeW
			0.0f,										// mipLodBias
			VK_FALSE,									// anisotropyEnable
			1.0f,										// maxAnisotropy
			DE_FALSE,									// compareEnable
			VK_COMPARE_OP_ALWAYS,						// compareOp
			0.0f,										// minLod
			0.0f,										// maxLod
			VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,	// borderColor
			VK_FALSE,									// unnormalizedCoords
		};

		Move<VkSampler>			tmpSampler	= createSampler(vk, vkDevice, &samplerInfo);
		Move<VkSampler>			tmp2Sampler;

		tmp2Sampler = tmpSampler;

		const Unique<VkSampler>	sampler		(tmp2Sampler);
	}

	return tcu::TestStatus::pass("Creating sampler succeeded");
}

void createShaderProgs (SourceCollections& dst)
{
	dst.glslSources.add("test") << glu::VertexSource(
		"#version 310 es\n"
		"layout(location = 0) in highp vec4 a_position;\n"
		"void main (void) { gl_Position = a_position; }\n");
}

tcu::TestStatus createShaderModuleTest (Context& context)
{
	const VkDevice					vkDevice	= context.getDevice();
	const DeviceInterface&			vk			= context.getDeviceInterface();
	const Unique<VkShaderModule>	shader		(createShaderModule(vk, vkDevice, context.getBinaryCollection().get("test"), 0));

	return tcu::TestStatus::pass("Creating shader module succeeded");
}

void createTriangleAsmProgs (SourceCollections& dst)
{
	dst.spirvAsmSources.add("vert") <<
		"		 OpCapability Shader\n"
		"%1 =	 OpExtInstImport \"GLSL.std.450\"\n"
		"		 OpMemoryModel Logical GLSL450\n"
		"		 OpEntryPoint Vertex %4 \"main\" %10 %12 %16 %17\n"
		"		 OpSource ESSL 300\n"
		"		 OpName %4 \"main\"\n"
		"		 OpName %10 \"gl_Position\"\n"
		"		 OpName %12 \"a_position\"\n"
		"		 OpName %16 \"gl_VertexIndex\"\n"
		"		 OpName %17 \"gl_InstanceIndex\"\n"
		"		 OpDecorate %10 BuiltIn Position\n"
		"		 OpDecorate %12 Location 0\n"
		"		 OpDecorate %16 BuiltIn VertexIndex\n"
		"		 OpDecorate %17 BuiltIn InstanceIndex\n"
		"%2 =	 OpTypeVoid\n"
		"%3 =	 OpTypeFunction %2\n"
		"%7 =	 OpTypeFloat 32\n"
		"%8 =	 OpTypeVector %7 4\n"
		"%9 =	 OpTypePointer Output %8\n"
		"%10 =	 OpVariable %9 Output\n"
		"%11 =	 OpTypePointer Input %8\n"
		"%12 =	 OpVariable %11 Input\n"
		"%14 =	 OpTypeInt 32 1\n"
		"%15 =	 OpTypePointer Input %14\n"
		"%16 =	 OpVariable %15 Input\n"
		"%17 =	 OpVariable %15 Input\n"
		"%4 =	 OpFunction %2 None %3\n"
		"%5 =	 OpLabel\n"
		"%13 =	 OpLoad %8 %12\n"
		"		 OpStore %10 %13\n"
		"		 OpBranch %6\n"
		"%6 =	 OpLabel\n"
		"		 OpReturn\n"
		"		 OpFunctionEnd\n";
	dst.spirvAsmSources.add("frag") <<
		"		OpCapability Shader\n"
		"%1 =	OpExtInstImport \"GLSL.std.450\"\n"
		"		OpMemoryModel Logical GLSL450\n"
		"		OpEntryPoint Fragment %4 \"main\" %10\n"
		"		OpExecutionMode %4 OriginUpperLeft\n"
		"		OpSource ESSL 300\n"
		"		OpName %4 \"main\"\n"
		"		OpName %10 \"o_color\"\n"
		"		OpDecorate %10 RelaxedPrecision\n"
		"		OpDecorate %10 Location 0\n"
		"%2 =	OpTypeVoid\n"
		"%3 =	OpTypeFunction %2\n"
		"%7 =	OpTypeFloat 32\n"
		"%8 =	OpTypeVector %7 4\n"
		"%9 =	OpTypePointer Output %8\n"
		"%10 =	OpVariable %9 Output\n"
		"%11 =	OpConstant %7 1065353216\n"
		"%12 =	OpConstant %7 0\n"
		"%13 =	OpConstantComposite %8 %11 %12 %11 %11\n"
		"%4 =	OpFunction %2 None %3\n"
		"%5 =	OpLabel\n"
		"		OpStore %10 %13\n"
		"		OpBranch %6\n"
		"%6 =	OpLabel\n"
		"		OpReturn\n"
		"		OpFunctionEnd\n";
}

void createTriangleProgs (SourceCollections& dst)
{
	dst.glslSources.add("vert") << glu::VertexSource(
		"#version 310 es\n"
		"layout(location = 0) in highp vec4 a_position;\n"
		"void main (void) { gl_Position = a_position; }\n");
	dst.glslSources.add("frag") << glu::FragmentSource(
		"#version 310 es\n"
		"layout(location = 0) out lowp vec4 o_color;\n"
		"void main (void) { o_color = vec4(1.0, 0.0, 1.0, 1.0); }\n");
}

void createProgsNoOpName (SourceCollections& dst)
{
	dst.spirvAsmSources.add("vert") <<
		"OpCapability Shader\n"
		"%1 = OpExtInstImport \"GLSL.std.450\"\n"
		"OpMemoryModel Logical GLSL450\n"
		"OpEntryPoint Vertex %4 \"main\" %20 %22 %26\n"
		"OpSource ESSL 310\n"
		"OpMemberDecorate %18 0 BuiltIn Position\n"
		"OpMemberDecorate %18 1 BuiltIn PointSize\n"
		"OpDecorate %18 Block\n"
		"OpDecorate %22 Location 0\n"
		"OpDecorate %26 Location 2\n"
		"%2 = OpTypeVoid\n"
		"%3 = OpTypeFunction %2\n"
		"%6 = OpTypeFloat 32\n"
		"%7 = OpTypeVector %6 4\n"
		"%8 = OpTypeStruct %7\n"
		"%9 = OpTypePointer Function %8\n"
		"%11 = OpTypeInt 32 1\n"
		"%12 = OpConstant %11 0\n"
		"%13 = OpConstant %6 1\n"
		"%14 = OpConstant %6 0\n"
		"%15 = OpConstantComposite %7 %13 %14 %13 %13\n"
		"%16 = OpTypePointer Function %7\n"
		"%18 = OpTypeStruct %7 %6\n"
		"%19 = OpTypePointer Output %18\n"
		"%20 = OpVariable %19 Output\n"
		"%21 = OpTypePointer Input %7\n"
		"%22 = OpVariable %21 Input\n"
		"%24 = OpTypePointer Output %7\n"
		"%26 = OpVariable %24 Output\n"
		"%4 = OpFunction %2 None %3\n"
		"%5 = OpLabel\n"
		"%10 = OpVariable %9 Function\n"
		"%17 = OpAccessChain %16 %10 %12\n"
		"OpStore %17 %15\n"
		"%23 = OpLoad %7 %22\n"
		"%25 = OpAccessChain %24 %20 %12\n"
		"OpStore %25 %23\n"
		"%27 = OpAccessChain %16 %10 %12\n"
		"%28 = OpLoad %7 %27\n"
		"OpStore %26 %28\n"
		"OpReturn\n"
		"OpFunctionEnd\n";
	dst.spirvAsmSources.add("frag") <<
		"OpCapability Shader\n"
		"%1 = OpExtInstImport \"GLSL.std.450\"\n"
		"OpMemoryModel Logical GLSL450\n"
		"OpEntryPoint Fragment %4 \"main\" %9 %11\n"
		"OpExecutionMode %4 OriginUpperLeft\n"
		"OpSource ESSL 310\n"
		"OpDecorate %9 RelaxedPrecision\n"
		"OpDecorate %9 Location 0\n"
		"OpDecorate %11 Location 2\n"
		"%2 = OpTypeVoid\n"
		"%3 = OpTypeFunction %2\n"
		"%6 = OpTypeFloat 32\n"
		"%7 = OpTypeVector %6 4\n"
		"%8 = OpTypePointer Output %7\n"
		"%9 = OpVariable %8 Output\n"
		"%10 = OpTypePointer Input %7\n"
		"%11 = OpVariable %10 Input\n"
		"%4 = OpFunction %2 None %3\n"
		"%5 = OpLabel\n"
		"%12 = OpLoad %7 %11\n"
		"OpStore %9 %12\n"
		"OpReturn\n"
		"OpFunctionEnd\n";
}

class RefVertexShader : public rr::VertexShader
{
public:
	RefVertexShader (void)
		: rr::VertexShader(1, 0)
	{
		m_inputs[0].type = rr::GENERICVECTYPE_FLOAT;
	}

	void shadeVertices (const rr::VertexAttrib* inputs, rr::VertexPacket* const* packets, const int numPackets) const
	{
		for (int packetNdx = 0; packetNdx < numPackets; ++packetNdx)
		{
			packets[packetNdx]->position = rr::readVertexAttribFloat(inputs[0],
																	 packets[packetNdx]->instanceNdx,
																	 packets[packetNdx]->vertexNdx);
		}
	}
};

class RefFragmentShader : public rr::FragmentShader
{
public:
	RefFragmentShader (void)
		: rr::FragmentShader(0, 1)
	{
		m_outputs[0].type = rr::GENERICVECTYPE_FLOAT;
	}

	void shadeFragments (rr::FragmentPacket*, const int numPackets, const rr::FragmentShadingContext& context) const
	{
		for (int packetNdx = 0; packetNdx < numPackets; ++packetNdx)
		{
			for (int fragNdx = 0; fragNdx < rr::NUM_FRAGMENTS_PER_PACKET; ++fragNdx)
			{
				rr::writeFragmentOutput(context, packetNdx, fragNdx, 0, tcu::Vec4(1.0f, 0.0f, 1.0f, 1.0f));
			}
		}
	}
};

void renderReferenceTriangle (const tcu::PixelBufferAccess& dst, const tcu::Vec4 (&vertices)[3], const int subpixelBits)
{
	const RefVertexShader					vertShader;
	const RefFragmentShader					fragShader;
	const rr::Program						program			(&vertShader, &fragShader);
	const rr::MultisamplePixelBufferAccess	colorBuffer		= rr::MultisamplePixelBufferAccess::fromSinglesampleAccess(dst);
	const rr::RenderTarget					renderTarget	(colorBuffer);
	const rr::RenderState					renderState		((rr::ViewportState(colorBuffer)), subpixelBits, rr::VIEWPORTORIENTATION_UPPER_LEFT);
	const rr::Renderer						renderer;
	const rr::VertexAttrib					vertexAttribs[]	=
	{
		rr::VertexAttrib(rr::VERTEXATTRIBTYPE_FLOAT, 4, sizeof(tcu::Vec4), 0, vertices[0].getPtr())
	};

	renderer.draw(rr::DrawCommand(renderState,
								  renderTarget,
								  program,
								  DE_LENGTH_OF_ARRAY(vertexAttribs),
								  &vertexAttribs[0],
								  rr::PrimitiveList(rr::PRIMITIVETYPE_TRIANGLES, DE_LENGTH_OF_ARRAY(vertices), 0)));
}

tcu::TestStatus renderTriangleTest (Context& context)
{
	const VkDevice							vkDevice				= context.getDevice();
	const DeviceInterface&					vk						= context.getDeviceInterface();
	const VkQueue							queue					= context.getUniversalQueue();
	const deUint32							queueFamilyIndex		= context.getUniversalQueueFamilyIndex();
	SimpleAllocator							memAlloc				(vk, vkDevice, getPhysicalDeviceMemoryProperties(context.getInstanceInterface(), context.getPhysicalDevice()));
	const tcu::IVec2						renderSize				(256, 256);
	const VkFormat							colorFormat				= VK_FORMAT_R8G8B8A8_UNORM;
	const tcu::Vec4							clearColor				(0.125f, 0.25f, 0.75f, 1.0f);

	const tcu::Vec4							vertices[]				=
	{
		tcu::Vec4(-0.5f, -0.5f, 0.0f, 1.0f),
		tcu::Vec4(+0.5f, -0.5f, 0.0f, 1.0f),
		tcu::Vec4( 0.0f, +0.5f, 0.0f, 1.0f)
	};

	const VkBufferCreateInfo				vertexBufferParams		=
	{
		VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,	// sType
		DE_NULL,								// pNext
		0u,										// flags
		(VkDeviceSize)sizeof(vertices),			// size
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,		// usage
		VK_SHARING_MODE_EXCLUSIVE,				// sharingMode
		1u,										// queueFamilyIndexCount
		&queueFamilyIndex,						// pQueueFamilyIndices
	};
	const Unique<VkBuffer>					vertexBuffer			(createBuffer(vk, vkDevice, &vertexBufferParams));
	const UniquePtr<Allocation>				vertexBufferMemory		(memAlloc.allocate(getBufferMemoryRequirements(vk, vkDevice, *vertexBuffer), MemoryRequirement::HostVisible));

	VK_CHECK(vk.bindBufferMemory(vkDevice, *vertexBuffer, vertexBufferMemory->getMemory(), vertexBufferMemory->getOffset()));

	const VkDeviceSize						imageSizeBytes			= (VkDeviceSize)(sizeof(deUint32)*renderSize.x()*renderSize.y());
	const VkBufferCreateInfo				readImageBufferParams	=
	{
		VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,		// sType
		DE_NULL,									// pNext
		(VkBufferCreateFlags)0u,					// flags
		imageSizeBytes,								// size
		VK_BUFFER_USAGE_TRANSFER_DST_BIT,			// usage
		VK_SHARING_MODE_EXCLUSIVE,					// sharingMode
		1u,											// queueFamilyIndexCount
		&queueFamilyIndex,							// pQueueFamilyIndices
	};
	const Unique<VkBuffer>					readImageBuffer			(createBuffer(vk, vkDevice, &readImageBufferParams));
	const UniquePtr<Allocation>				readImageBufferMemory	(memAlloc.allocate(getBufferMemoryRequirements(vk, vkDevice, *readImageBuffer), MemoryRequirement::HostVisible));

	VK_CHECK(vk.bindBufferMemory(vkDevice, *readImageBuffer, readImageBufferMemory->getMemory(), readImageBufferMemory->getOffset()));

	const VkImageCreateInfo					imageParams				=
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,									// sType
		DE_NULL,																// pNext
		0u,																		// flags
		VK_IMAGE_TYPE_2D,														// imageType
		VK_FORMAT_R8G8B8A8_UNORM,												// format
		{ (deUint32)renderSize.x(), (deUint32)renderSize.y(), 1 },				// extent
		1u,																		// mipLevels
		1u,																		// arraySize
		VK_SAMPLE_COUNT_1_BIT,													// samples
		VK_IMAGE_TILING_OPTIMAL,												// tiling
		VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT,	// usage
		VK_SHARING_MODE_EXCLUSIVE,												// sharingMode
		1u,																		// queueFamilyIndexCount
		&queueFamilyIndex,														// pQueueFamilyIndices
		VK_IMAGE_LAYOUT_UNDEFINED,												// initialLayout
	};

	const Unique<VkImage>					image					(createImage(vk, vkDevice, &imageParams));
	const UniquePtr<Allocation>				imageMemory				(memAlloc.allocate(getImageMemoryRequirements(vk, vkDevice, *image), MemoryRequirement::Any));

	VK_CHECK(vk.bindImageMemory(vkDevice, *image, imageMemory->getMemory(), imageMemory->getOffset()));

	const Unique<VkRenderPass>				renderPass				(makeRenderPass(vk, vkDevice, VK_FORMAT_R8G8B8A8_UNORM));

	const VkImageViewCreateInfo				colorAttViewParams		=
	{
		VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,		// sType
		DE_NULL,										// pNext
		0u,												// flags
		*image,											// image
		VK_IMAGE_VIEW_TYPE_2D,							// viewType
		VK_FORMAT_R8G8B8A8_UNORM,						// format
		{
			VK_COMPONENT_SWIZZLE_R,
			VK_COMPONENT_SWIZZLE_G,
			VK_COMPONENT_SWIZZLE_B,
			VK_COMPONENT_SWIZZLE_A
		},												// components
		{
			VK_IMAGE_ASPECT_COLOR_BIT,						// aspectMask
			0u,												// baseMipLevel
			1u,												// levelCount
			0u,												// baseArrayLayer
			1u,												// layerCount
		},												// subresourceRange
	};
	const Unique<VkImageView>				colorAttView			(createImageView(vk, vkDevice, &colorAttViewParams));

	// Pipeline layout
	const VkPipelineLayoutCreateInfo		pipelineLayoutParams	=
	{
		VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,			// sType
		DE_NULL,												// pNext
		(vk::VkPipelineLayoutCreateFlags)0,
		0u,														// setLayoutCount
		DE_NULL,												// pSetLayouts
		0u,														// pushConstantRangeCount
		DE_NULL,												// pPushConstantRanges
	};
	const Unique<VkPipelineLayout>			pipelineLayout			(createPipelineLayout(vk, vkDevice, &pipelineLayoutParams));

	// Shaders
	const Unique<VkShaderModule>			vertShaderModule		(createShaderModule(vk, vkDevice, context.getBinaryCollection().get("vert"), 0));
	const Unique<VkShaderModule>			fragShaderModule		(createShaderModule(vk, vkDevice, context.getBinaryCollection().get("frag"), 0));

	// Pipeline
	const std::vector<VkViewport>			viewports				(1, makeViewport(renderSize));
	const std::vector<VkRect2D>				scissors				(1, makeRect2D(renderSize));

	const Unique<VkPipeline>				pipeline				(makeGraphicsPipeline(vk,					// const DeviceInterface&            vk
																						  vkDevice,				// const VkDevice                    device
																						  *pipelineLayout,		// const VkPipelineLayout            pipelineLayout
																						  *vertShaderModule,	// const VkShaderModule              vertexShaderModule
																						  DE_NULL,				// const VkShaderModule              tessellationControlModule
																						  DE_NULL,				// const VkShaderModule              tessellationEvalModule
																						  DE_NULL,				// const VkShaderModule              geometryShaderModule
																						  *fragShaderModule,	// const VkShaderModule              fragmentShaderModule
																						  *renderPass,			// const VkRenderPass                renderPass
																						  viewports,			// const std::vector<VkViewport>&    viewports
																						  scissors));			// const std::vector<VkRect2D>&      scissors

	// Framebuffer
	const VkFramebufferCreateInfo			framebufferParams		=
	{
		VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,				// sType
		DE_NULL,												// pNext
		0u,														// flags
		*renderPass,											// renderPass
		1u,														// attachmentCount
		&*colorAttView,											// pAttachments
		(deUint32)renderSize.x(),								// width
		(deUint32)renderSize.y(),								// height
		1u,														// layers
	};
	const Unique<VkFramebuffer>				framebuffer				(createFramebuffer(vk, vkDevice, &framebufferParams));

	const VkCommandPoolCreateInfo			cmdPoolParams			=
	{
		VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,					// sType
		DE_NULL,													// pNext
		VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,			// flags
		queueFamilyIndex,											// queueFamilyIndex
	};
	const Unique<VkCommandPool>				cmdPool					(createCommandPool(vk, vkDevice, &cmdPoolParams));

	// Command buffer
	const VkCommandBufferAllocateInfo		cmdBufParams			=
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,			// sType
		DE_NULL,												// pNext
		*cmdPool,												// pool
		VK_COMMAND_BUFFER_LEVEL_PRIMARY,						// level
		1u,														// bufferCount
	};
	const Unique<VkCommandBuffer>			cmdBuf					(allocateCommandBuffer(vk, vkDevice, &cmdBufParams));

	// Record commands
	beginCommandBuffer(vk, *cmdBuf);

	{
		const VkMemoryBarrier		vertFlushBarrier	=
		{
			VK_STRUCTURE_TYPE_MEMORY_BARRIER,			// sType
			DE_NULL,									// pNext
			VK_ACCESS_HOST_WRITE_BIT,					// srcAccessMask
			VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT,		// dstAccessMask
		};
		const VkImageMemoryBarrier	colorAttBarrier		=
		{
			VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,		// sType
			DE_NULL,									// pNext
			0u,											// srcAccessMask
			(VK_ACCESS_COLOR_ATTACHMENT_READ_BIT|
			 VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT),		// dstAccessMask
			VK_IMAGE_LAYOUT_UNDEFINED,					// oldLayout
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,	// newLayout
			queueFamilyIndex,							// srcQueueFamilyIndex
			queueFamilyIndex,							// dstQueueFamilyIndex
			*image,										// image
			{
				VK_IMAGE_ASPECT_COLOR_BIT,					// aspectMask
				0u,											// baseMipLevel
				1u,											// levelCount
				0u,											// baseArrayLayer
				1u,											// layerCount
			}											// subresourceRange
		};
		vk.cmdPipelineBarrier(*cmdBuf, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, (VkDependencyFlags)0, 1, &vertFlushBarrier, 0, (const VkBufferMemoryBarrier*)DE_NULL, 1, &colorAttBarrier);
	}

	beginRenderPass(vk, *cmdBuf, *renderPass, *framebuffer, makeRect2D(0, 0, renderSize.x(), renderSize.y()), clearColor);

	vk.cmdBindPipeline(*cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline);
	{
		const VkDeviceSize bindingOffset = 0;
		vk.cmdBindVertexBuffers(*cmdBuf, 0u, 1u, &vertexBuffer.get(), &bindingOffset);
	}
	vk.cmdDraw(*cmdBuf, 3u, 1u, 0u, 0u);
	endRenderPass(vk, *cmdBuf);
	copyImageToBuffer(vk, *cmdBuf, *image, *readImageBuffer, renderSize);
	endCommandBuffer(vk, *cmdBuf);

	// Upload vertex data
	deMemcpy(vertexBufferMemory->getHostPtr(), &vertices[0], sizeof(vertices));
	flushAlloc(vk, vkDevice, *vertexBufferMemory);

	// Submit & wait for completion
	submitCommandsAndWait(vk, vkDevice, queue, cmdBuf.get());

	// Read results, render reference, compare
	{
		const tcu::TextureFormat			tcuFormat		= vk::mapVkFormat(colorFormat);
		const tcu::ConstPixelBufferAccess	resultAccess	(tcuFormat, renderSize.x(), renderSize.y(), 1, readImageBufferMemory->getHostPtr());

		invalidateAlloc(vk, vkDevice, *readImageBufferMemory);

		{
			tcu::TextureLevel	refImage		(tcuFormat, renderSize.x(), renderSize.y());
			const tcu::UVec4	threshold		(0u);
			const tcu::IVec3	posDeviation	(1,1,0);

			tcu::clear(refImage.getAccess(), clearColor);
			renderReferenceTriangle(refImage.getAccess(), vertices, context.getDeviceProperties().limits.subPixelPrecisionBits);

			if (tcu::intThresholdPositionDeviationCompare(context.getTestContext().getLog(),
														  "ComparisonResult",
														  "Image comparison result",
														  refImage.getAccess(),
														  resultAccess,
														  threshold,
														  posDeviation,
														  false,
														  tcu::COMPARE_LOG_RESULT))
				return tcu::TestStatus::pass("Rendering succeeded");
			else
				return tcu::TestStatus::fail("Image comparison failed");
		}
	}

	return tcu::TestStatus::pass("Rendering succeeded");
}

struct VoidVulkanStruct
{
	VkStructureType sType;
	const void*		pNext;
};

tcu::TestStatus renderTriangleUnusedExtStructTest (Context& context)
{
	const VkDevice									vkDevice						= context.getDevice();
	const DeviceInterface&							vk								= context.getDeviceInterface();
	const VkQueue									queue							= context.getUniversalQueue();
	const deUint32									queueFamilyIndex				= context.getUniversalQueueFamilyIndex();
	SimpleAllocator									memAlloc						(vk, vkDevice, getPhysicalDeviceMemoryProperties(context.getInstanceInterface(), context.getPhysicalDevice()));
	const tcu::IVec2								renderSize						(256, 256);
	const VkFormat									colorFormat						= VK_FORMAT_R8G8B8A8_UNORM;
	const tcu::Vec4									clearColor						(0.125f, 0.25f, 0.75f, 1.0f);

	// This structure will stand in as an unknown extension structure that must be ignored by implementations.
	VoidVulkanStruct								unusedExtStruct					=
	{
		VK_STRUCTURE_TYPE_MAX_ENUM,		// sType
		DE_NULL							// pNext
	};

	const tcu::Vec4									vertices[]						=
	{
		tcu::Vec4(-0.5f, -0.5f, 0.0f, 1.0f),
		tcu::Vec4(+0.5f, -0.5f, 0.0f, 1.0f),
		tcu::Vec4( 0.0f, +0.5f, 0.0f, 1.0f)
	};

	const VkBufferCreateInfo						vertexBufferParams				=
	{
		VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,	// sType
		&unusedExtStruct,						// pNext
		0u,										// flags
		(VkDeviceSize)sizeof(vertices),			// size
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,		// usage
		VK_SHARING_MODE_EXCLUSIVE,				// sharingMode
		1u,										// queueFamilyIndexCount
		&queueFamilyIndex,						// pQueueFamilyIndices
	};
	const Unique<VkBuffer>							vertexBuffer					(createBuffer(vk, vkDevice, &vertexBufferParams));
	const UniquePtr<Allocation>						vertexBufferMemory				(memAlloc.allocate(getBufferMemoryRequirements(vk, vkDevice, *vertexBuffer), MemoryRequirement::HostVisible));

	VK_CHECK(vk.bindBufferMemory(vkDevice, *vertexBuffer, vertexBufferMemory->getMemory(), vertexBufferMemory->getOffset()));

	const VkDeviceSize								imageSizeBytes					= (VkDeviceSize)(sizeof(deUint32)*renderSize.x()*renderSize.y());
	const VkBufferCreateInfo						readImageBufferParams			=
	{
		VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,	// sType
		&unusedExtStruct,						// pNext
		(VkBufferCreateFlags)0u,				// flags
		imageSizeBytes,							// size
		VK_BUFFER_USAGE_TRANSFER_DST_BIT,		// usage
		VK_SHARING_MODE_EXCLUSIVE,				// sharingMode
		1u,										// queueFamilyIndexCount
		&queueFamilyIndex,						// pQueueFamilyIndices
	};
	const Unique<VkBuffer>							readImageBuffer					(createBuffer(vk, vkDevice, &readImageBufferParams));
	const UniquePtr<Allocation>						readImageBufferMemory			(memAlloc.allocate(getBufferMemoryRequirements(vk, vkDevice, *readImageBuffer), MemoryRequirement::HostVisible));

	VK_CHECK(vk.bindBufferMemory(vkDevice, *readImageBuffer, readImageBufferMemory->getMemory(), readImageBufferMemory->getOffset()));

	const VkImageCreateInfo							imageParams						=
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,									// sType
		&unusedExtStruct,														// pNext
		0u,																		// flags
		VK_IMAGE_TYPE_2D,														// imageType
		VK_FORMAT_R8G8B8A8_UNORM,												// format
		{ (deUint32)renderSize.x(), (deUint32)renderSize.y(), 1 },				// extent
		1u,																		// mipLevels
		1u,																		// arraySize
		VK_SAMPLE_COUNT_1_BIT,													// samples
		VK_IMAGE_TILING_OPTIMAL,												// tiling
		VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT,	// usage
		VK_SHARING_MODE_EXCLUSIVE,												// sharingMode
		1u,																		// queueFamilyIndexCount
		&queueFamilyIndex,														// pQueueFamilyIndices
		VK_IMAGE_LAYOUT_UNDEFINED,												// initialLayout
	};

	const Unique<VkImage>							image							(createImage(vk, vkDevice, &imageParams));
	const UniquePtr<Allocation>						imageMemory						(memAlloc.allocate(getImageMemoryRequirements(vk, vkDevice, *image), MemoryRequirement::Any));

	VK_CHECK(vk.bindImageMemory(vkDevice, *image, imageMemory->getMemory(), imageMemory->getOffset()));

	// Render pass
	const VkAttachmentDescription					colorAttachmentDescription			=
	{
		(VkAttachmentDescriptionFlags)0,			// VkAttachmentDescriptionFlags    flags
		VK_FORMAT_R8G8B8A8_UNORM,					// VkFormat                        format
		VK_SAMPLE_COUNT_1_BIT,						// VkSampleCountFlagBits           samples
		VK_ATTACHMENT_LOAD_OP_CLEAR,				// VkAttachmentLoadOp              loadOp
		VK_ATTACHMENT_STORE_OP_STORE,				// VkAttachmentStoreOp             storeOp
		VK_ATTACHMENT_LOAD_OP_DONT_CARE,			// VkAttachmentLoadOp              stencilLoadOp
		VK_ATTACHMENT_STORE_OP_DONT_CARE,			// VkAttachmentStoreOp             stencilStoreOp
		VK_IMAGE_LAYOUT_UNDEFINED,					// VkImageLayout                   initialLayout
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL	// VkImageLayout                   finalLayout
	};

	const VkAttachmentReference						colorAttachmentRef					=
	{
		0u,											// deUint32         attachment
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL	// VkImageLayout    layout
	};

	const VkSubpassDescription						subpassDescription					=
	{
		(VkSubpassDescriptionFlags)0,		// VkSubpassDescriptionFlags       flags
		VK_PIPELINE_BIND_POINT_GRAPHICS,	// VkPipelineBindPoint             pipelineBindPoint
		0u,									// deUint32                        inputAttachmentCount
		DE_NULL,							// const VkAttachmentReference*    pInputAttachments
		1u,									// deUint32                        colorAttachmentCount
		&colorAttachmentRef,				// const VkAttachmentReference*    pColorAttachments
		DE_NULL,							// const VkAttachmentReference*    pResolveAttachments
		DE_NULL,							// const VkAttachmentReference*    pDepthStencilAttachment
		0u,									// deUint32                        preserveAttachmentCount
		DE_NULL								// const deUint32*                 pPreserveAttachments
	};

	const VkRenderPassCreateInfo					renderPassInfo						=
	{
		VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,	// VkStructureType                   sType
		&unusedExtStruct,							// const void*                       pNext
		(VkRenderPassCreateFlags)0,					// VkRenderPassCreateFlags           flags
		1u,											// deUint32                          attachmentCount
		&colorAttachmentDescription,				// const VkAttachmentDescription*    pAttachments
		1u,											// deUint32                          subpassCount
		&subpassDescription,						// const VkSubpassDescription*       pSubpasses
		0u,											// deUint32                          dependencyCount
		DE_NULL										// const VkSubpassDependency*        pDependencies
	};

	const Unique<VkRenderPass>						renderPass							(createRenderPass(vk, vkDevice, &renderPassInfo, DE_NULL));

	// Color attachment view
	const VkImageViewCreateInfo						colorAttViewParams					=
	{
		VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,	// sType
		&unusedExtStruct,							// pNext
		0u,											// flags
		*image,										// image
		VK_IMAGE_VIEW_TYPE_2D,						// viewType
		VK_FORMAT_R8G8B8A8_UNORM,					// format
		{
			VK_COMPONENT_SWIZZLE_R,
			VK_COMPONENT_SWIZZLE_G,
			VK_COMPONENT_SWIZZLE_B,
			VK_COMPONENT_SWIZZLE_A
		},											// components
		{
			VK_IMAGE_ASPECT_COLOR_BIT,				// aspectMask
			0u,										// baseMipLevel
			1u,										// levelCount
			0u,										// baseArrayLayer
			1u,										// layerCount
		},											// subresourceRange
	};
	const Unique<VkImageView>						colorAttView						(createImageView(vk, vkDevice, &colorAttViewParams));

	// Pipeline layout
	const VkPipelineLayoutCreateInfo				pipelineLayoutParams				=
	{
		VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,	// sType
		&unusedExtStruct,								// pNext
		(vk::VkPipelineLayoutCreateFlags)0,
		0u,												// setLayoutCount
		DE_NULL,										// pSetLayouts
		0u,												// pushConstantRangeCount
		DE_NULL,										// pPushConstantRanges
	};
	const Unique<VkPipelineLayout>					pipelineLayout						(createPipelineLayout(vk, vkDevice, &pipelineLayoutParams));

	// Shader modules
	const ProgramBinary								vertBin								= context.getBinaryCollection().get("vert");
	const ProgramBinary								fragBin								= context.getBinaryCollection().get("frag");

	const struct VkShaderModuleCreateInfo			vertModuleInfo						=
	{
		VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		&unusedExtStruct,
		0,
		(deUintptr)vertBin.getSize(),
		(const deUint32*)vertBin.getBinary(),
	};

	const struct VkShaderModuleCreateInfo			fragModuleInfo						=
	{
		VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		&unusedExtStruct,
		0,
		(deUintptr)fragBin.getSize(),
		(const deUint32*)fragBin.getBinary(),
	};

	const Unique<VkShaderModule>					vertShaderModule					(createShaderModule(vk, vkDevice, &vertModuleInfo));
	const Unique<VkShaderModule>					fragShaderModule					(createShaderModule(vk, vkDevice, &fragModuleInfo));

	// Pipeline
	const std::vector<VkViewport>					viewports							(1, makeViewport(renderSize));
	const std::vector<VkRect2D>						scissors							(1, makeRect2D(renderSize));

	VkPipelineShaderStageCreateInfo					stageCreateInfo						=
	{
		VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,	// VkStructureType                     sType
		&unusedExtStruct,										// const void*                         pNext
		0u,														// VkPipelineShaderStageCreateFlags    flags
		VK_SHADER_STAGE_VERTEX_BIT,								// VkShaderStageFlagBits               stage
		DE_NULL,												// VkShaderModule                      module
		"main",													// const char*                         pName
		DE_NULL													// const VkSpecializationInfo*         pSpecializationInfo
	};

	std::vector<VkPipelineShaderStageCreateInfo>	pipelineShaderStageParams;

	stageCreateInfo.stage	= VK_SHADER_STAGE_VERTEX_BIT;
	stageCreateInfo.module	= *vertShaderModule;
	pipelineShaderStageParams.push_back(stageCreateInfo);

	stageCreateInfo.stage	= VK_SHADER_STAGE_FRAGMENT_BIT;
	stageCreateInfo.module	= *fragShaderModule;
	pipelineShaderStageParams.push_back(stageCreateInfo);

	const VkVertexInputBindingDescription			vertexInputBindingDescription		=
	{
		0u,								// deUint32             binding
		sizeof(tcu::Vec4),				// deUint32             stride
		VK_VERTEX_INPUT_RATE_VERTEX,	// VkVertexInputRate    inputRate
	};

	const VkVertexInputAttributeDescription			vertexInputAttributeDescription		=
	{
		0u,								// deUint32    location
		0u,								// deUint32    binding
		VK_FORMAT_R32G32B32A32_SFLOAT,	// VkFormat    format
		0u								// deUint32    offset
	};

	const VkPipelineVertexInputStateCreateInfo		vertexInputStateCreateInfo			=
	{
		VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,	// VkStructureType                             sType
		&unusedExtStruct,											// const void*                                 pNext
		(VkPipelineVertexInputStateCreateFlags)0,					// VkPipelineVertexInputStateCreateFlags       flags
		1u,															// deUint32                                    vertexBindingDescriptionCount
		&vertexInputBindingDescription,								// const VkVertexInputBindingDescription*      pVertexBindingDescriptions
		1u,															// deUint32                                    vertexAttributeDescriptionCount
		&vertexInputAttributeDescription							// const VkVertexInputAttributeDescription*    pVertexAttributeDescriptions
	};

	const VkPipelineInputAssemblyStateCreateInfo	inputAssemblyStateCreateInfo		=
	{
		VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,	// VkStructureType                            sType
		&unusedExtStruct,												// const void*                                pNext
		0u,																// VkPipelineInputAssemblyStateCreateFlags    flags
		VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,							// VkPrimitiveTopology                        topology
		VK_FALSE														// VkBool32                                   primitiveRestartEnable
	};

	const VkPipelineViewportStateCreateInfo			viewportStateCreateInfo				=
	{
		VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,	// VkStructureType                             sType
		&unusedExtStruct,										// const void*                                 pNext
		(VkPipelineViewportStateCreateFlags)0,					// VkPipelineViewportStateCreateFlags          flags
		(deUint32)viewports.size(),								// deUint32                                    viewportCount
		viewports.data(),										// const VkViewport*                           pViewports
		(deUint32)scissors.size(),								// deUint32                                    scissorCount
		scissors.data()											// const VkRect2D*                             pScissors
	};

	const VkPipelineRasterizationStateCreateInfo	rasterizationStateCreateInfo		=
	{
		VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,	// VkStructureType                            sType
		&unusedExtStruct,											// const void*                                pNext
		0u,															// VkPipelineRasterizationStateCreateFlags    flags
		VK_FALSE,													// VkBool32                                   depthClampEnable
		VK_FALSE,													// VkBool32                                   rasterizerDiscardEnable
		VK_POLYGON_MODE_FILL,										// VkPolygonMode                              polygonMode
		VK_CULL_MODE_NONE,											// VkCullModeFlags                            cullMode
		VK_FRONT_FACE_COUNTER_CLOCKWISE,							// VkFrontFace                                frontFace
		VK_FALSE,													// VkBool32                                   depthBiasEnable
		0.0f,														// float                                      depthBiasConstantFactor
		0.0f,														// float                                      depthBiasClamp
		0.0f,														// float                                      depthBiasSlopeFactor
		1.0f														// float                                      lineWidth
	};

	const VkPipelineMultisampleStateCreateInfo		multisampleStateCreateInfo			=
	{
		VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,	// VkStructureType                          sType
		&unusedExtStruct,											// const void*                              pNext
		0u,															// VkPipelineMultisampleStateCreateFlags    flags
		VK_SAMPLE_COUNT_1_BIT,										// VkSampleCountFlagBits                    rasterizationSamples
		VK_FALSE,													// VkBool32                                 sampleShadingEnable
		1.0f,														// float                                    minSampleShading
		DE_NULL,													// const VkSampleMask*                      pSampleMask
		VK_FALSE,													// VkBool32                                 alphaToCoverageEnable
		VK_FALSE													// VkBool32                                 alphaToOneEnable
	};

	const VkStencilOpState							stencilOpState						=
	{
		VK_STENCIL_OP_KEEP,		// VkStencilOp    failOp
		VK_STENCIL_OP_KEEP,		// VkStencilOp    passOp
		VK_STENCIL_OP_KEEP,		// VkStencilOp    depthFailOp
		VK_COMPARE_OP_NEVER,	// VkCompareOp    compareOp
		0,						// deUint32       compareMask
		0,						// deUint32       writeMask
		0						// deUint32       reference
	};

	const VkPipelineDepthStencilStateCreateInfo		depthStencilStateCreateInfo			=
	{
		VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,	// VkStructureType                          sType
		&unusedExtStruct,											// const void*                              pNext
		0u,															// VkPipelineDepthStencilStateCreateFlags   flags
		VK_FALSE,													// VkBool32                                 depthTestEnable
		VK_FALSE,													// VkBool32                                 depthWriteEnable
		VK_COMPARE_OP_LESS_OR_EQUAL,								// VkCompareOp                              depthCompareOp
		VK_FALSE,													// VkBool32                                 depthBoundsTestEnable
		VK_FALSE,													// VkBool32                                 stencilTestEnable
		stencilOpState,												// VkStencilOpState                         front
		stencilOpState,												// VkStencilOpState                         back
		0.0f,														// float                                    minDepthBounds
		1.0f,														// float                                    maxDepthBounds
	};

	const VkPipelineColorBlendAttachmentState		colorBlendAttachmentState			=
	{
		VK_FALSE,					// VkBool32                 blendEnable
		VK_BLEND_FACTOR_ZERO,		// VkBlendFactor            srcColorBlendFactor
		VK_BLEND_FACTOR_ZERO,		// VkBlendFactor            dstColorBlendFactor
		VK_BLEND_OP_ADD,			// VkBlendOp                colorBlendOp
		VK_BLEND_FACTOR_ZERO,		// VkBlendFactor            srcAlphaBlendFactor
		VK_BLEND_FACTOR_ZERO,		// VkBlendFactor            dstAlphaBlendFactor
		VK_BLEND_OP_ADD,			// VkBlendOp                alphaBlendOp
		VK_COLOR_COMPONENT_R_BIT	// VkColorComponentFlags    colorWriteMask
		| VK_COLOR_COMPONENT_G_BIT
		| VK_COLOR_COMPONENT_B_BIT
		| VK_COLOR_COMPONENT_A_BIT
	};

	const VkPipelineColorBlendStateCreateInfo		colorBlendStateCreateInfo			=
	{
		VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,	// VkStructureType                               sType
		&unusedExtStruct,											// const void*                                   pNext
		0u,															// VkPipelineColorBlendStateCreateFlags          flags
		VK_FALSE,													// VkBool32                                      logicOpEnable
		VK_LOGIC_OP_CLEAR,											// VkLogicOp                                     logicOp
		1u,															// deUint32                                      attachmentCount
		&colorBlendAttachmentState,									// const VkPipelineColorBlendAttachmentState*    pAttachments
		{ 0.0f, 0.0f, 0.0f, 0.0f }									// float                                         blendConstants[4]
	};

	const VkGraphicsPipelineCreateInfo				pipelineCreateInfo					=
	{
		VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,	// VkStructureType                                  sType
		&unusedExtStruct,									// const void*                                      pNext
		0u,													// VkPipelineCreateFlags                            flags
		(deUint32)pipelineShaderStageParams.size(),			// deUint32                                         stageCount
		&pipelineShaderStageParams[0],						// const VkPipelineShaderStageCreateInfo*           pStages
		&vertexInputStateCreateInfo,						// const VkPipelineVertexInputStateCreateInfo*      pVertexInputState
		&inputAssemblyStateCreateInfo,						// const VkPipelineInputAssemblyStateCreateInfo*    pInputAssemblyState
		DE_NULL,											// const VkPipelineTessellationStateCreateInfo*     pTessellationState
		&viewportStateCreateInfo,							// const VkPipelineViewportStateCreateInfo*         pViewportState
		&rasterizationStateCreateInfo,						// const VkPipelineRasterizationStateCreateInfo*    pRasterizationState
		&multisampleStateCreateInfo,						// const VkPipelineMultisampleStateCreateInfo*      pMultisampleState
		&depthStencilStateCreateInfo,						// const VkPipelineDepthStencilStateCreateInfo*     pDepthStencilState
		&colorBlendStateCreateInfo,							// const VkPipelineColorBlendStateCreateInfo*       pColorBlendState
		DE_NULL,											// const VkPipelineDynamicStateCreateInfo*          pDynamicState
		*pipelineLayout,									// VkPipelineLayout                                 layout
		*renderPass,										// VkRenderPass                                     renderPass
		0u,													// deUint32                                         subpass
		DE_NULL,											// VkPipeline                                       basePipelineHandle
		0													// deInt32                                          basePipelineIndex
	};

	const Unique<VkPipeline>						pipeline							(createGraphicsPipeline(vk, vkDevice, DE_NULL, &pipelineCreateInfo));

	// Framebuffer
	const VkFramebufferCreateInfo					framebufferParams					=
	{
		VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,	// sType
		&unusedExtStruct,							// pNext
		0u,											// flags
		*renderPass,								// renderPass
		1u,											// attachmentCount
		&*colorAttView,								// pAttachments
		(deUint32)renderSize.x(),					// width
		(deUint32)renderSize.y(),					// height
		1u,											// layers
	};
	const Unique<VkFramebuffer>						framebuffer							(createFramebuffer(vk, vkDevice, &framebufferParams));

	// Command buffer
	const VkCommandPoolCreateInfo					cmdPoolParams						=
	{
		VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,			// sType
		&unusedExtStruct,									// pNext
		VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,	// flags
		queueFamilyIndex,									// queueFamilyIndex
	};
	const Unique<VkCommandPool>						cmdPool								(createCommandPool(vk, vkDevice, &cmdPoolParams));

	const VkCommandBufferAllocateInfo				cmdBufParams						=
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,		// sType
		&unusedExtStruct,									// pNext
		*cmdPool,											// pool
		VK_COMMAND_BUFFER_LEVEL_PRIMARY,					// level
		1u,													// bufferCount
	};
	const Unique<VkCommandBuffer>					cmdBuf								(allocateCommandBuffer(vk, vkDevice, &cmdBufParams));

	// Record commands
	const VkCommandBufferBeginInfo					commandBufBeginParams				=
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,	// VkStructureType                  sType;
		&unusedExtStruct,								// const void*                      pNext;
		VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,	// VkCommandBufferUsageFlags        flags;
		(const VkCommandBufferInheritanceInfo*)DE_NULL,
	};
	VK_CHECK(vk.beginCommandBuffer(*cmdBuf, &commandBufBeginParams));

	const VkMemoryBarrier							vertFlushBarrier					=
	{
		VK_STRUCTURE_TYPE_MEMORY_BARRIER,		// sType
		&unusedExtStruct,						// pNext
		VK_ACCESS_HOST_WRITE_BIT,				// srcAccessMask
		VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT,	// dstAccessMask
	};

	const VkImageMemoryBarrier						colorAttBarrier						=
	{
		VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,		// sType
		&unusedExtStruct,							// pNext
		0u,											// srcAccessMask
		(VK_ACCESS_COLOR_ATTACHMENT_READ_BIT|
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT),	// dstAccessMask
		VK_IMAGE_LAYOUT_UNDEFINED,					// oldLayout
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,	// newLayout
		queueFamilyIndex,							// srcQueueFamilyIndex
		queueFamilyIndex,							// dstQueueFamilyIndex
		*image,										// image
		{
			VK_IMAGE_ASPECT_COLOR_BIT,				// aspectMask
			0u,										// baseMipLevel
			1u,										// levelCount
			0u,										// baseArrayLayer
			1u,										// layerCount
		}											// subresourceRange
	};
	vk.cmdPipelineBarrier(*cmdBuf, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, (VkDependencyFlags)0, 1, &vertFlushBarrier, 0, (const VkBufferMemoryBarrier*)DE_NULL, 1, &colorAttBarrier);

	const VkClearValue								clearValue							= makeClearValueColor(clearColor);
	const VkRenderPassBeginInfo						renderPassBeginInfo					=
	{
		VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,			// VkStructureType         sType;
		&unusedExtStruct,									// const void*             pNext;
		*renderPass,										// VkRenderPass            renderPass;
		*framebuffer,										// VkFramebuffer           framebuffer;
		makeRect2D(0, 0, renderSize.x(), renderSize.y()),	// VkRect2D                renderArea;
		1u,													// deUint32                clearValueCount;
		&clearValue,										// const VkClearValue*     pClearValues;
	};

	vk.cmdBeginRenderPass(*cmdBuf, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

	vk.cmdBindPipeline(*cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline);

	const VkDeviceSize								bindingOffset						= 0;
	vk.cmdBindVertexBuffers(*cmdBuf, 0u, 1u, &vertexBuffer.get(), &bindingOffset);

	vk.cmdDraw(*cmdBuf, 3u, 1u, 0u, 0u);
	endRenderPass(vk, *cmdBuf);

	const VkImageMemoryBarrier						imageBarrier						=
	{
		VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,									// VkStructureType			sType;
		&unusedExtStruct,														// const void*				pNext;
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,									// VkAccessFlags			srcAccessMask;
		VK_ACCESS_TRANSFER_READ_BIT,											// VkAccessFlags			dstAccessMask;
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,								// VkImageLayout			oldLayout;
		VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,									// VkImageLayout			newLayout;
		VK_QUEUE_FAMILY_IGNORED,												// deUint32					srcQueueFamilyIndex;
		VK_QUEUE_FAMILY_IGNORED,												// deUint32					destQueueFamilyIndex;
		*image,																	// VkImage					image;
		makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0, 1u)		// VkImageSubresourceRange	subresourceRange;
	};

	vk.cmdPipelineBarrier(*cmdBuf, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u,
						  0u, DE_NULL, 0u, DE_NULL, 1u, &imageBarrier);

	const VkImageSubresourceLayers					subresource							=
	{
		VK_IMAGE_ASPECT_COLOR_BIT,	// VkImageAspectFlags	aspectMask;
		0u,							// deUint32				mipLevel;
		0u,							// deUint32				baseArrayLayer;
		1u							// deUint32				layerCount;
	};

	const VkBufferImageCopy							region								=
	{
		0ull,												// VkDeviceSize					bufferOffset;
		0u,													// deUint32						bufferRowLength;
		0u,													// deUint32						bufferImageHeight;
		subresource,										// VkImageSubresourceLayers		imageSubresource;
		makeOffset3D(0, 0, 0),								// VkOffset3D					imageOffset;
		makeExtent3D(renderSize.x(), renderSize.y(), 1u)	// VkExtent3D					imageExtent;
	};

	vk.cmdCopyImageToBuffer(*cmdBuf, *image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, *readImageBuffer, 1u, &region);

	const VkBufferMemoryBarrier						bufferBarrier						=
	{
		VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,	// VkStructureType	sType;
		&unusedExtStruct,							// const void*		pNext;
		VK_ACCESS_TRANSFER_WRITE_BIT,				// VkAccessFlags	srcAccessMask;
		VK_ACCESS_HOST_READ_BIT,					// VkAccessFlags	dstAccessMask;
		VK_QUEUE_FAMILY_IGNORED,					// deUint32			srcQueueFamilyIndex;
		VK_QUEUE_FAMILY_IGNORED,					// deUint32			dstQueueFamilyIndex;
		*readImageBuffer,							// VkBuffer			buffer;
		0ull,										// VkDeviceSize		offset;
		VK_WHOLE_SIZE								// VkDeviceSize		size;
	};

	vk.cmdPipelineBarrier(*cmdBuf, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0u,
						  0u, DE_NULL, 1u, &bufferBarrier, 0u, DE_NULL);

	endCommandBuffer(vk, *cmdBuf);

	// Upload vertex data
	const VkMappedMemoryRange						flushRange							=
	{
		VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
		&unusedExtStruct,
		vertexBufferMemory->getMemory(),
		vertexBufferMemory->getOffset(),
		VK_WHOLE_SIZE
	};
	deMemcpy(vertexBufferMemory->getHostPtr(), &vertices[0], sizeof(vertices));
	VK_CHECK(vk.flushMappedMemoryRanges(vkDevice, 1u, &flushRange));

	// Submit & wait for completion
	const VkFenceCreateInfo							createInfo							=
	{
		VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
		&unusedExtStruct,
		0u
	};

	const Unique<VkFence>							fence								(createFence(vk, vkDevice, &createInfo, DE_NULL));

	const VkSubmitInfo								submitInfo							=
	{
		VK_STRUCTURE_TYPE_SUBMIT_INFO,			// VkStructureType				sType;
		&unusedExtStruct,						// const void*					pNext;
		0u,										// deUint32						waitSemaphoreCount;
		DE_NULL,								// const VkSemaphore*			pWaitSemaphores;
		(const VkPipelineStageFlags*)DE_NULL,	// const VkPipelineStageFlags*	pWaitDstStageMask;
		1u,										// deUint32						commandBufferCount;
		&*cmdBuf,								// const VkCommandBuffer*		pCommandBuffers;
		0u,										// deUint32						signalSemaphoreCount;
		DE_NULL,								// const VkSemaphore*			pSignalSemaphores;
	};

	VK_CHECK(vk.queueSubmit(queue, 1u, &submitInfo, *fence));
	VK_CHECK(vk.waitForFences(vkDevice, 1u, &fence.get(), DE_TRUE, ~0ull));

	// Read results, render reference, compare
	{
		const tcu::TextureFormat					tcuFormat							= vk::mapVkFormat(colorFormat);
		const tcu::ConstPixelBufferAccess			resultAccess						(tcuFormat, renderSize.x(), renderSize.y(), 1, readImageBufferMemory->getHostPtr());

		invalidateAlloc(vk, vkDevice, *readImageBufferMemory);

		{
			tcu::TextureLevel						refImage							(tcuFormat, renderSize.x(), renderSize.y());
			const tcu::UVec4						threshold							(0u);
			const tcu::IVec3						posDeviation						(1,1,0);

			tcu::clear(refImage.getAccess(), clearColor);
			renderReferenceTriangle(refImage.getAccess(), vertices, context.getDeviceProperties().limits.subPixelPrecisionBits);

			if (tcu::intThresholdPositionDeviationCompare(context.getTestContext().getLog(),
														  "ComparisonResult",
														  "Image comparison result",
														  refImage.getAccess(),
														  resultAccess,
														  threshold,
														  posDeviation,
														  false,
														  tcu::COMPARE_LOG_RESULT))
				return tcu::TestStatus::pass("Rendering succeeded");
			else
				return tcu::TestStatus::fail("Image comparison failed");
		}
	}

	return tcu::TestStatus::pass("Rendering succeeded");
}

tcu::TestStatus renderTriangleUnusedResolveAttachmentTest (Context& context)
{
	const VkDevice							vkDevice				= context.getDevice();
	const DeviceInterface&					vk						= context.getDeviceInterface();
	const VkQueue							queue					= context.getUniversalQueue();
	const deUint32							queueFamilyIndex		= context.getUniversalQueueFamilyIndex();
	SimpleAllocator							memAlloc				(vk, vkDevice, getPhysicalDeviceMemoryProperties(context.getInstanceInterface(), context.getPhysicalDevice()));
	const tcu::IVec2						renderSize				(256, 256);
	const VkFormat							colorFormat				= VK_FORMAT_R8G8B8A8_UNORM;
	const tcu::Vec4							clearColor				(0.125f, 0.25f, 0.75f, 1.0f);

	const tcu::Vec4							vertices[]				=
	{
		tcu::Vec4(-0.5f, -0.5f, 0.0f, 1.0f),
		tcu::Vec4(+0.5f, -0.5f, 0.0f, 1.0f),
		tcu::Vec4( 0.0f, +0.5f, 0.0f, 1.0f)
	};

	const VkBufferCreateInfo				vertexBufferParams		=
	{
		VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,	// sType
		DE_NULL,								// pNext
		0u,										// flags
		(VkDeviceSize)sizeof(vertices),			// size
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,		// usage
		VK_SHARING_MODE_EXCLUSIVE,				// sharingMode
		1u,										// queueFamilyIndexCount
		&queueFamilyIndex,						// pQueueFamilyIndices
	};
	const Unique<VkBuffer>					vertexBuffer			(createBuffer(vk, vkDevice, &vertexBufferParams));
	const UniquePtr<Allocation>				vertexBufferMemory		(memAlloc.allocate(getBufferMemoryRequirements(vk, vkDevice, *vertexBuffer), MemoryRequirement::HostVisible));

	VK_CHECK(vk.bindBufferMemory(vkDevice, *vertexBuffer, vertexBufferMemory->getMemory(), vertexBufferMemory->getOffset()));

	const VkDeviceSize						imageSizeBytes			= (VkDeviceSize)(sizeof(deUint32)*renderSize.x()*renderSize.y());
	const VkBufferCreateInfo				readImageBufferParams	=
	{
		VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,		// sType
		DE_NULL,									// pNext
		(VkBufferCreateFlags)0u,					// flags
		imageSizeBytes,								// size
		VK_BUFFER_USAGE_TRANSFER_DST_BIT,			// usage
		VK_SHARING_MODE_EXCLUSIVE,					// sharingMode
		1u,											// queueFamilyIndexCount
		&queueFamilyIndex,							// pQueueFamilyIndices
	};
	const Unique<VkBuffer>					readImageBuffer			(createBuffer(vk, vkDevice, &readImageBufferParams));
	const UniquePtr<Allocation>				readImageBufferMemory	(memAlloc.allocate(getBufferMemoryRequirements(vk, vkDevice, *readImageBuffer), MemoryRequirement::HostVisible));

	VK_CHECK(vk.bindBufferMemory(vkDevice, *readImageBuffer, readImageBufferMemory->getMemory(), readImageBufferMemory->getOffset()));

	const VkImageCreateInfo					imageParams				=
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,									// sType
		DE_NULL,																// pNext
		0u,																		// flags
		VK_IMAGE_TYPE_2D,														// imageType
		VK_FORMAT_R8G8B8A8_UNORM,												// format
		{ (deUint32)renderSize.x(), (deUint32)renderSize.y(), 1 },				// extent
		1u,																		// mipLevels
		1u,																		// arraySize
		VK_SAMPLE_COUNT_1_BIT,													// samples
		VK_IMAGE_TILING_OPTIMAL,												// tiling
		VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT,	// usage
		VK_SHARING_MODE_EXCLUSIVE,												// sharingMode
		1u,																		// queueFamilyIndexCount
		&queueFamilyIndex,														// pQueueFamilyIndices
		VK_IMAGE_LAYOUT_UNDEFINED,												// initialLayout
	};

	const Unique<VkImage>					image					(createImage(vk, vkDevice, &imageParams));
	const UniquePtr<Allocation>				imageMemory				(memAlloc.allocate(getImageMemoryRequirements(vk, vkDevice, *image), MemoryRequirement::Any));

	VK_CHECK(vk.bindImageMemory(vkDevice, *image, imageMemory->getMemory(), imageMemory->getOffset()));

	const VkAttachmentDescription			colorAttDesc			=
	{
		0u,												// flags
		VK_FORMAT_R8G8B8A8_UNORM,						// format
		VK_SAMPLE_COUNT_1_BIT,							// samples
		VK_ATTACHMENT_LOAD_OP_CLEAR,					// loadOp
		VK_ATTACHMENT_STORE_OP_STORE,					// storeOp
		VK_ATTACHMENT_LOAD_OP_DONT_CARE,				// stencilLoadOp
		VK_ATTACHMENT_STORE_OP_DONT_CARE,				// stencilStoreOp
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,		// initialLayout
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,		// finalLayout
	};
	const VkAttachmentReference				colorAttRef				=
	{
		0u,												// attachment
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,		// layout
	};
	const VkAttachmentReference				resolveAttRef			=
	{
		VK_ATTACHMENT_UNUSED,
		VK_IMAGE_LAYOUT_GENERAL
	};
	const VkSubpassDescription				subpassDesc				=
	{
		(VkSubpassDescriptionFlags)0u,					// flags
		VK_PIPELINE_BIND_POINT_GRAPHICS,				// pipelineBindPoint
		0u,												// inputAttachmentCount
		DE_NULL,										// pInputAttachments
		1u,												// colorAttachmentCount
		&colorAttRef,									// pColorAttachments
		&resolveAttRef,									// pResolveAttachments
		DE_NULL,										// depthStencilAttachment
		0u,												// preserveAttachmentCount
		DE_NULL,										// pPreserveAttachments
	};
	const VkRenderPassCreateInfo			renderPassParams		=
	{
		VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,		// sType
		DE_NULL,										// pNext
		0u,												// flags
		1u,												// attachmentCount
		&colorAttDesc,									// pAttachments
		1u,												// subpassCount
		&subpassDesc,									// pSubpasses
		0u,												// dependencyCount
		DE_NULL,										// pDependencies
	};
	const Unique<VkRenderPass>				renderPass				(createRenderPass(vk, vkDevice, &renderPassParams));

	const VkImageViewCreateInfo				colorAttViewParams		=
	{
		VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,		// sType
		DE_NULL,										// pNext
		0u,												// flags
		*image,											// image
		VK_IMAGE_VIEW_TYPE_2D,							// viewType
		VK_FORMAT_R8G8B8A8_UNORM,						// format
		{
			VK_COMPONENT_SWIZZLE_R,
			VK_COMPONENT_SWIZZLE_G,
			VK_COMPONENT_SWIZZLE_B,
			VK_COMPONENT_SWIZZLE_A
		},												// components
		{
			VK_IMAGE_ASPECT_COLOR_BIT,						// aspectMask
			0u,												// baseMipLevel
			1u,												// levelCount
			0u,												// baseArrayLayer
			1u,												// layerCount
		},												// subresourceRange
	};
	const Unique<VkImageView>				colorAttView			(createImageView(vk, vkDevice, &colorAttViewParams));

	// Pipeline layout
	const VkPipelineLayoutCreateInfo		pipelineLayoutParams	=
	{
		VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,			// sType
		DE_NULL,												// pNext
		(vk::VkPipelineLayoutCreateFlags)0,
		0u,														// setLayoutCount
		DE_NULL,												// pSetLayouts
		0u,														// pushConstantRangeCount
		DE_NULL,												// pPushConstantRanges
	};
	const Unique<VkPipelineLayout>			pipelineLayout			(createPipelineLayout(vk, vkDevice, &pipelineLayoutParams));

	// Shaders
	const Unique<VkShaderModule>			vertShaderModule		(createShaderModule(vk, vkDevice, context.getBinaryCollection().get("vert"), 0));
	const Unique<VkShaderModule>			fragShaderModule		(createShaderModule(vk, vkDevice, context.getBinaryCollection().get("frag"), 0));

	// Pipeline
	const std::vector<VkViewport>			viewports				(1, makeViewport(renderSize));
	const std::vector<VkRect2D>				scissors				(1, makeRect2D(renderSize));

	const Unique<VkPipeline>				pipeline				(makeGraphicsPipeline(vk,					// const DeviceInterface&            vk
																						  vkDevice,				// const VkDevice                    device
																						  *pipelineLayout,		// const VkPipelineLayout            pipelineLayout
																						  *vertShaderModule,	// const VkShaderModule              vertexShaderModule
																						  DE_NULL,				// const VkShaderModule              tessellationControlShaderModule
																						  DE_NULL,				// const VkShaderModule              tessellationEvalShaderModule
																						  DE_NULL,				// const VkShaderModule              geometryShaderModule
																						  *fragShaderModule,	// const VkShaderModule              fragmentShaderModule
																						  *renderPass,			// const VkRenderPass                renderPass
																						  viewports,			// const std::vector<VkViewport>&    viewports
																						  scissors));			// const std::vector<VkRect2D>&      scissors

	// Framebuffer
	const VkFramebufferCreateInfo			framebufferParams		=
	{
		VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,				// sType
		DE_NULL,												// pNext
		0u,														// flags
		*renderPass,											// renderPass
		1u,														// attachmentCount
		&*colorAttView,											// pAttachments
		(deUint32)renderSize.x(),								// width
		(deUint32)renderSize.y(),								// height
		1u,														// layers
	};
	const Unique<VkFramebuffer>				framebuffer				(createFramebuffer(vk, vkDevice, &framebufferParams));

	const VkCommandPoolCreateInfo			cmdPoolParams			=
	{
		VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,					// sType
		DE_NULL,													// pNext
		VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,			// flags
		queueFamilyIndex,											// queueFamilyIndex
	};
	const Unique<VkCommandPool>				cmdPool					(createCommandPool(vk, vkDevice, &cmdPoolParams));

	// Command buffer
	const VkCommandBufferAllocateInfo		cmdBufParams			=
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,			// sType
		DE_NULL,												// pNext
		*cmdPool,												// pool
		VK_COMMAND_BUFFER_LEVEL_PRIMARY,						// level
		1u,														// bufferCount
	};
	const Unique<VkCommandBuffer>			cmdBuf					(allocateCommandBuffer(vk, vkDevice, &cmdBufParams));

	// Record commands
	beginCommandBuffer(vk, *cmdBuf);

	{
		const VkMemoryBarrier		vertFlushBarrier	=
		{
			VK_STRUCTURE_TYPE_MEMORY_BARRIER,			// sType
			DE_NULL,									// pNext
			VK_ACCESS_HOST_WRITE_BIT,					// srcAccessMask
			VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT,		// dstAccessMask
		};
		const VkImageMemoryBarrier	colorAttBarrier		=
		{
			VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,		// sType
			DE_NULL,									// pNext
			0u,											// srcAccessMask
			(VK_ACCESS_COLOR_ATTACHMENT_READ_BIT|
			 VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT),		// dstAccessMask
			VK_IMAGE_LAYOUT_UNDEFINED,					// oldLayout
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,	// newLayout
			queueFamilyIndex,							// srcQueueFamilyIndex
			queueFamilyIndex,							// dstQueueFamilyIndex
			*image,										// image
			{
				VK_IMAGE_ASPECT_COLOR_BIT,					// aspectMask
				0u,											// baseMipLevel
				1u,											// levelCount
				0u,											// baseArrayLayer
				1u,											// layerCount
			}											// subresourceRange
		};
		vk.cmdPipelineBarrier(*cmdBuf, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, (VkDependencyFlags)0, 1, &vertFlushBarrier, 0, (const VkBufferMemoryBarrier*)DE_NULL, 1, &colorAttBarrier);
	}

	beginRenderPass(vk, *cmdBuf, *renderPass, *framebuffer, makeRect2D(0, 0, renderSize.x(), renderSize.y()), clearColor);

	vk.cmdBindPipeline(*cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline);
	{
		const VkDeviceSize bindingOffset = 0;
		vk.cmdBindVertexBuffers(*cmdBuf, 0u, 1u, &vertexBuffer.get(), &bindingOffset);
	}
	vk.cmdDraw(*cmdBuf, 3u, 1u, 0u, 0u);
	endRenderPass(vk, *cmdBuf);
	copyImageToBuffer(vk, *cmdBuf, *image, *readImageBuffer, renderSize);
	endCommandBuffer(vk, *cmdBuf);

	// Upload vertex data
	deMemcpy(vertexBufferMemory->getHostPtr(), &vertices[0], sizeof(vertices));
	flushAlloc(vk, vkDevice, *vertexBufferMemory);

	// Submit & wait for completion
	submitCommandsAndWait(vk, vkDevice, queue, cmdBuf.get());

	// Read results, render reference, compare
	{
		const tcu::TextureFormat			tcuFormat		= vk::mapVkFormat(colorFormat);
		const tcu::ConstPixelBufferAccess	resultAccess	(tcuFormat, renderSize.x(), renderSize.y(), 1, readImageBufferMemory->getHostPtr());

		invalidateAlloc(vk, vkDevice, *readImageBufferMemory);

		{
			tcu::TextureLevel	refImage		(tcuFormat, renderSize.x(), renderSize.y());
			const tcu::UVec4	threshold		(0u);
			const tcu::IVec3	posDeviation	(1,1,0);

			tcu::clear(refImage.getAccess(), clearColor);
			renderReferenceTriangle(refImage.getAccess(), vertices, context.getDeviceProperties().limits.subPixelPrecisionBits);

			if (tcu::intThresholdPositionDeviationCompare(context.getTestContext().getLog(),
														  "ComparisonResult",
														  "Image comparison result",
														  refImage.getAccess(),
														  resultAccess,
														  threshold,
														  posDeviation,
														  false,
														  tcu::COMPARE_LOG_RESULT))
				return tcu::TestStatus::pass("Rendering succeeded");
			else
				return tcu::TestStatus::fail("Image comparison failed");
		}
	}

	return tcu::TestStatus::pass("Rendering succeeded");
}

} // anonymous

tcu::TestCaseGroup* createSmokeTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup>	smokeTests	(new tcu::TestCaseGroup(testCtx, "smoke", "Smoke Tests"));

	addFunctionCase				(smokeTests.get(), "create_sampler",			"",	createSamplerTest);
	addFunctionCaseWithPrograms	(smokeTests.get(), "create_shader",				"", createShaderProgs,		createShaderModuleTest);
	addFunctionCaseWithPrograms	(smokeTests.get(), "triangle",					"", createTriangleProgs,	renderTriangleTest);
	addFunctionCaseWithPrograms	(smokeTests.get(), "triangle_ext_structs",		"", createTriangleProgs,	renderTriangleUnusedExtStructTest);
	addFunctionCaseWithPrograms	(smokeTests.get(), "asm_triangle",				"", createTriangleAsmProgs,	renderTriangleTest);
	addFunctionCaseWithPrograms	(smokeTests.get(), "asm_triangle_no_opname",	"", createProgsNoOpName,	renderTriangleTest);
	addFunctionCaseWithPrograms	(smokeTests.get(), "unused_resolve_attachment",	"", createTriangleProgs,	renderTriangleUnusedResolveAttachmentTest);

	return smokeTests.release();
}

} // api
} // vkt
