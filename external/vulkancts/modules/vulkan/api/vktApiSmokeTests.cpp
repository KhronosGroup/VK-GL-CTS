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
	addFunctionCaseWithPrograms	(smokeTests.get(), "asm_triangle",				"", createTriangleAsmProgs,	renderTriangleTest);
	addFunctionCaseWithPrograms	(smokeTests.get(), "asm_triangle_no_opname",	"", createProgsNoOpName,	renderTriangleTest);
	addFunctionCaseWithPrograms	(smokeTests.get(), "unused_resolve_attachment",	"", createTriangleProgs,	renderTriangleUnusedResolveAttachmentTest);

	return smokeTests.release();
}

} // api
} // vkt
