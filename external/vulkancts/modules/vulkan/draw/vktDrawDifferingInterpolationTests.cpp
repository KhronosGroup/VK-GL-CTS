/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2017 The Khronos Group Inc.
 * Copyright (c) 2017 Google Inc.
 * Copyright (c) 2017 Samsung Electronics Co., Ltd.
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
 * \brief Differing iterpolation decorations tests
 *//*--------------------------------------------------------------------*/

#include "vktDrawDifferingInterpolationTests.hpp"

#include "vktDrawBaseClass.hpp"
#include "vkQueryUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vktTestGroupUtil.hpp"

#include "deDefs.h"
#include "deRandom.hpp"
#include "deString.h"

#include "tcuTestCase.hpp"
#include "tcuRGBA.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuImageCompare.hpp"
#include "tcuStringTemplate.hpp"

#include "rrRenderer.hpp"

#include <string>
#include <sstream>

namespace vkt
{
namespace Draw
{
namespace
{
using namespace vk;
using namespace std;

struct DrawParams
{
	string					vertShader;
	string					fragShader;
	string					refVertShader;
	string					refFragShader;
	const SharedGroupParams	groupParams;
};

class DrawTestInstance : public TestInstance
{
public:
						DrawTestInstance			(Context& context, const DrawParams& data);
						~DrawTestInstance			(void);
	tcu::TestStatus		iterate						(void);

protected:
	void				preRenderCommands			(VkCommandBuffer cmdBuffer, VkImage colorTargetImage, const VkClearValue& clearColor);
	void				draw						(VkCommandBuffer cmdBuffer, VkPipeline pipeline, VkBuffer vertexBuffer);

#ifndef CTS_USES_VULKANSC
	void				beginSecondaryCmdBuffer(VkCommandBuffer cmdBuffer, VkFormat colorAttachmentFormat, VkRenderingFlagsKHR renderingFlags = 0u);
#endif // CTS_USES_VULKANSC

private:
	DrawParams			m_data;

	enum
	{
		WIDTH = 256,
		HEIGHT = 256
	};
};

DrawTestInstance::DrawTestInstance (Context& context, const DrawParams& data)
	: vkt::TestInstance		(context)
	, m_data				(data)
{
}

DrawTestInstance::~DrawTestInstance (void)
{
}

class DrawTestCase : public TestCase
{
	public:
								DrawTestCase		(tcu::TestContext& context, const char* name, const char* desc, const DrawParams data);
								~DrawTestCase		(void);
	virtual	void				initPrograms		(SourceCollections& programCollection) const;
	virtual void				checkSupport		(Context& context) const;
	virtual TestInstance*		createInstance		(Context& context) const;

private:
	DrawParams					m_data;
};

DrawTestCase::DrawTestCase (tcu::TestContext& context, const char* name, const char* desc, const DrawParams data)
	: vkt::TestCase	(context, name, desc)
	, m_data		(data)
{
}

DrawTestCase::~DrawTestCase	(void)
{
}

void DrawTestCase::initPrograms (SourceCollections& programCollection) const
{
	const tcu::StringTemplate	vertShader	(string(
		"#version 430\n"
		"layout(location = 0) in vec4 in_position;\n"
		"layout(location = 1) in vec4 in_color;\n"
		"layout(location = 0) ${qualifier:opt} out vec4 out_color;\n"
		"out gl_PerVertex {\n"
		"    vec4  gl_Position;\n"
		"    float gl_PointSize;\n"
		"};\n"
		"void main() {\n"
		"    gl_PointSize = 1.0;\n"
		"    gl_Position  = in_position;\n"
		"    out_color    = in_color;\n"
		"}\n"));

	const tcu::StringTemplate	fragShader	(string(
		"#version 430\n"
		"layout(location = 0) ${qualifier:opt} in vec4 in_color;\n"
		"layout(location = 0) out vec4 out_color;\n"
		"void main()\n"
		"{\n"
		"    out_color = in_color;\n"
		"}\n"));

	map<string, string> empty;
	map<string, string> flat;
	flat["qualifier"] = "flat";
	map<string, string> noPerspective;
	noPerspective["qualifier"] = "noperspective";

	programCollection.glslSources.add("vert") << glu::VertexSource(vertShader.specialize(empty));
	programCollection.glslSources.add("vertFlatColor") << glu::VertexSource(vertShader.specialize(flat));
	programCollection.glslSources.add("vertNoPerspective") << glu::VertexSource(vertShader.specialize(noPerspective));
	programCollection.glslSources.add("frag") << glu::FragmentSource(fragShader.specialize(empty));
	programCollection.glslSources.add("fragFlatColor") << glu::FragmentSource(fragShader.specialize(flat));
	programCollection.glslSources.add("fragNoPerspective") << glu::FragmentSource(fragShader.specialize(noPerspective));
}

void DrawTestCase::checkSupport(Context& context) const
{
	if (m_data.groupParams->useDynamicRendering)
		context.requireDeviceFunctionality("VK_KHR_dynamic_rendering");
}

TestInstance* DrawTestCase::createInstance (Context& context) const
{
	return new DrawTestInstance(context, m_data);
}

tcu::TestStatus DrawTestInstance::iterate (void)
{
	tcu::ConstPixelBufferAccess	frames[2];
	de::SharedPtr<Image>		colorTargetImages[2];
	const string				vertShaderNames[2]	= { m_data.vertShader, m_data.refVertShader };
	const string				fragShaderNames[2]	= { m_data.fragShader, m_data.refFragShader };
	const DeviceInterface&		vk					= m_context.getDeviceInterface();
	const VkDevice				device				= m_context.getDevice();
	tcu::TestLog				&log				= m_context.getTestContext().getLog();

	// Run two iterations with shaders that have different interpolation decorations. Images should still match.
	for (deUint32 frameIdx = 0; frameIdx < DE_LENGTH_OF_ARRAY(frames); frameIdx++)
	{
		const CmdPoolCreateInfo			cmdPoolCreateInfo		(m_context.getUniversalQueueFamilyIndex());
		Move<VkCommandPool>				cmdPool					= createCommandPool(vk, device, &cmdPoolCreateInfo);
		Move<VkCommandBuffer>			cmdBuffer				= allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
		Move<VkCommandBuffer>			secCmdBuffer;
		const Unique<VkShaderModule>	vs						(createShaderModule(vk, device, m_context.getBinaryCollection().get(vertShaderNames[frameIdx].c_str()), 0));
		const Unique<VkShaderModule>	fs						(createShaderModule(vk, device, m_context.getBinaryCollection().get(fragShaderNames[frameIdx].c_str()), 0));
		const VkFormat					targetImageFormat		= VK_FORMAT_R8G8B8A8_UNORM;
		de::SharedPtr<Buffer>			vertexBuffer;
		Move<VkRenderPass>				renderPass;
		Move<VkImageView>				colorTargetView;
		Move<VkFramebuffer>				framebuffer;
		Move<VkPipeline>				pipeline;

		// Create color buffer image.
		{
			const VkExtent3D				targetImageExtent		= { WIDTH, HEIGHT, 1 };
			const ImageCreateInfo			targetImageCreateInfo	(VK_IMAGE_TYPE_2D, targetImageFormat, targetImageExtent, 1, 1, VK_SAMPLE_COUNT_1_BIT,
				VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
			colorTargetImages[frameIdx]								= Image::createAndAlloc(vk, device, targetImageCreateInfo, m_context.getDefaultAllocator(), m_context.getUniversalQueueFamilyIndex());
		}

		const ImageViewCreateInfo colorTargetViewInfo(colorTargetImages[frameIdx]->object(), VK_IMAGE_VIEW_TYPE_2D, targetImageFormat);
		colorTargetView = createImageView(vk, device, &colorTargetViewInfo);

		// Create render pass and frame buffer.
		if (!m_data.groupParams->useDynamicRendering)
		{
			RenderPassCreateInfo			renderPassCreateInfo;
			renderPassCreateInfo.addAttachment(AttachmentDescription(targetImageFormat,
																	 VK_SAMPLE_COUNT_1_BIT,
																	 VK_ATTACHMENT_LOAD_OP_LOAD,
																	 VK_ATTACHMENT_STORE_OP_STORE,
																	 VK_ATTACHMENT_LOAD_OP_DONT_CARE,
																	 VK_ATTACHMENT_STORE_OP_STORE,
																	 VK_IMAGE_LAYOUT_GENERAL,
																	 VK_IMAGE_LAYOUT_GENERAL));

			const VkAttachmentReference		colorAttachmentRef		= { 0, VK_IMAGE_LAYOUT_GENERAL };
			renderPassCreateInfo.addSubpass(SubpassDescription(VK_PIPELINE_BIND_POINT_GRAPHICS,
															   0,
															   0,
															   DE_NULL,
															   1,
															   &colorAttachmentRef,
															   DE_NULL,
															   AttachmentReference(),
															   0,
															   DE_NULL));

			renderPass = createRenderPass(vk, device, &renderPassCreateInfo);

			vector<VkImageView>				colorAttachments		{ *colorTargetView };
			const FramebufferCreateInfo		framebufferCreateInfo	(*renderPass, colorAttachments, WIDTH, HEIGHT, 1);
			framebuffer	= createFramebuffer(vk, device, &framebufferCreateInfo);
		}

		// Create vertex buffer.
		{
			const PositionColorVertex	vertices[]	=
			{
				PositionColorVertex(
					tcu::Vec4(-0.8f, -0.7f, 1.0f, 1.0f),	// Coord
					tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f)),		// Color

				PositionColorVertex(
					tcu::Vec4(0.0f, 0.4f, 0.5f, 0.5f),		// Coord
					tcu::Vec4(0.0f, 1.0f, 0.0f, 1.0f)),		// Color

				PositionColorVertex(
					tcu::Vec4(0.8f, -0.5f, 1.0f, 1.0f),		// Coord
					tcu::Vec4(0.0f, 0.0f, 1.0f, 1.0f))		// Color
			};

			const VkDeviceSize			dataSize	= DE_LENGTH_OF_ARRAY(vertices) * sizeof(PositionColorVertex);
			vertexBuffer							= Buffer::createAndAlloc(vk, device, BufferCreateInfo(dataSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT), m_context.getDefaultAllocator(), MemoryRequirement::HostVisible);
			deUint8*					ptr			= reinterpret_cast<deUint8*>(vertexBuffer->getBoundMemory().getHostPtr());

			deMemcpy(ptr, vertices, static_cast<size_t>(dataSize));
			flushMappedMemoryRange(vk, device, vertexBuffer->getBoundMemory().getMemory(), vertexBuffer->getBoundMemory().getOffset(), VK_WHOLE_SIZE);
		}

		const PipelineLayoutCreateInfo	pipelineLayoutCreateInfo;
		Move<VkPipelineLayout>			pipelineLayout = createPipelineLayout(vk, device, &pipelineLayoutCreateInfo);

		// Create pipeline
		{
			const PipelineCreateInfo::ColorBlendState::Attachment vkCbAttachmentState;

			VkViewport	viewport	= makeViewport(WIDTH, HEIGHT);
			VkRect2D	scissor		= makeRect2D(WIDTH, HEIGHT);

			const VkVertexInputBindingDescription vertexInputBindingDescription = { 0, (deUint32)sizeof(tcu::Vec4) * 2, VK_VERTEX_INPUT_RATE_VERTEX };

			const VkVertexInputAttributeDescription vertexInputAttributeDescriptions[2] =
			{
				{ 0u, 0u, VK_FORMAT_R32G32B32A32_SFLOAT, 0u },
				{ 1u, 0u, VK_FORMAT_R32G32B32A32_SFLOAT, (deUint32)(sizeof(float)* 4) }
			};

			PipelineCreateInfo::VertexInputState vertexInputState	= PipelineCreateInfo::VertexInputState(1, &vertexInputBindingDescription, 2, vertexInputAttributeDescriptions);

			PipelineCreateInfo pipelineCreateInfo(*pipelineLayout, *renderPass, 0, 0);
			pipelineCreateInfo.addShader(PipelineCreateInfo::PipelineShaderStage(*vs, "main", VK_SHADER_STAGE_VERTEX_BIT));
			pipelineCreateInfo.addShader(PipelineCreateInfo::PipelineShaderStage(*fs, "main", VK_SHADER_STAGE_FRAGMENT_BIT));
			pipelineCreateInfo.addState(PipelineCreateInfo::VertexInputState(vertexInputState));
			pipelineCreateInfo.addState(PipelineCreateInfo::InputAssemblerState(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST));
			pipelineCreateInfo.addState(PipelineCreateInfo::ColorBlendState(1, &vkCbAttachmentState));
			pipelineCreateInfo.addState(PipelineCreateInfo::ViewportState(1, vector<VkViewport>(1, viewport), vector<VkRect2D>(1, scissor)));
			pipelineCreateInfo.addState(PipelineCreateInfo::DepthStencilState());
			pipelineCreateInfo.addState(PipelineCreateInfo::RasterizerState());
			pipelineCreateInfo.addState(PipelineCreateInfo::MultiSampleState());

#ifndef CTS_USES_VULKANSC
			VkPipelineRenderingCreateInfoKHR renderingCreateInfo
			{
				VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR,
				DE_NULL,
				0u,
				1u,
				&targetImageFormat,
				VK_FORMAT_UNDEFINED,
				VK_FORMAT_UNDEFINED
			};

			if (m_data.groupParams->useDynamicRendering)
				pipelineCreateInfo.pNext = &renderingCreateInfo;
#endif // CTS_USES_VULKANSC

			pipeline = createGraphicsPipeline(vk, device, DE_NULL, &pipelineCreateInfo);
		}

		const VkRect2D		renderArea	= makeRect2D(WIDTH, HEIGHT);
		const VkClearValue	clearColor	{ { { 0.0f, 0.0f, 0.0f, 1.0f } } };
		const VkBuffer		buffer		= vertexBuffer->object();

		// Record commands
#ifndef CTS_USES_VULKANSC
		if (m_data.groupParams->useSecondaryCmdBuffer)
		{
			secCmdBuffer = allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_SECONDARY);

			// record secondary command buffer
			if (m_data.groupParams->secondaryCmdBufferCompletelyContainsDynamicRenderpass)
			{
				beginSecondaryCmdBuffer(*secCmdBuffer, targetImageFormat, VK_RENDERING_CONTENTS_SECONDARY_COMMAND_BUFFERS_BIT);
				beginRendering(vk, *secCmdBuffer, *colorTargetView, renderArea, clearColor, VK_IMAGE_LAYOUT_GENERAL, VK_ATTACHMENT_LOAD_OP_LOAD, 0u);
			}
			else
				beginSecondaryCmdBuffer(*secCmdBuffer, targetImageFormat);

			draw(*secCmdBuffer, *pipeline, buffer);

			if (m_data.groupParams->secondaryCmdBufferCompletelyContainsDynamicRenderpass)
				endRendering(vk, *secCmdBuffer);

			endCommandBuffer(vk, *secCmdBuffer);

			// record primary command buffer
			beginCommandBuffer(vk, *cmdBuffer, 0u);

			preRenderCommands(*cmdBuffer, colorTargetImages[frameIdx]->object(), clearColor);

			if (!m_data.groupParams->secondaryCmdBufferCompletelyContainsDynamicRenderpass)
				beginRendering(vk, *cmdBuffer, *colorTargetView, renderArea, clearColor, VK_IMAGE_LAYOUT_GENERAL, VK_ATTACHMENT_LOAD_OP_LOAD, VK_RENDERING_CONTENTS_SECONDARY_COMMAND_BUFFERS_BIT);

			vk.cmdExecuteCommands(*cmdBuffer, 1u, &*secCmdBuffer);

			if (!m_data.groupParams->secondaryCmdBufferCompletelyContainsDynamicRenderpass)
				endRendering(vk, *cmdBuffer);

			endCommandBuffer(vk, *cmdBuffer);
		}
		else if (m_data.groupParams->useDynamicRendering)
		{
			beginCommandBuffer(vk, *cmdBuffer);

			preRenderCommands(*cmdBuffer, colorTargetImages[frameIdx]->object(), clearColor);
			beginRendering(vk, *cmdBuffer, *colorTargetView, renderArea, clearColor, VK_IMAGE_LAYOUT_GENERAL, VK_ATTACHMENT_LOAD_OP_LOAD);
			draw(*cmdBuffer, *pipeline, buffer);
			endRendering(vk, *cmdBuffer);

			endCommandBuffer(vk, *cmdBuffer);
		}
#endif // CTS_USES_VULKANSC

		if (!m_data.groupParams->useDynamicRendering)
		{
			beginCommandBuffer(vk, *cmdBuffer);

			preRenderCommands(*cmdBuffer, colorTargetImages[frameIdx]->object(), clearColor);
			beginRenderPass(vk, *cmdBuffer, *renderPass, *framebuffer, renderArea);
			draw(*cmdBuffer, *pipeline, buffer);
			endRenderPass(vk, *cmdBuffer);

			endCommandBuffer(vk, *cmdBuffer);
		}

		// Submit and read results.
		const VkQueue		queue		= m_context.getUniversalQueue();
		const VkOffset3D	zeroOffset	= { 0, 0, 0 };
		submitCommandsAndWait(vk, device, queue, cmdBuffer.get());
		frames[frameIdx] = colorTargetImages[frameIdx]->readSurface(queue, m_context.getDefaultAllocator(), VK_IMAGE_LAYOUT_GENERAL, zeroOffset, WIDTH, HEIGHT, VK_IMAGE_ASPECT_COLOR_BIT);
	}

	qpTestResult res = QP_TEST_RESULT_PASS;

	if (!tcu::intThresholdCompare(log, "Result", "Image comparison result", frames[0], frames[1], tcu::UVec4(0), tcu::COMPARE_LOG_RESULT))
		res = QP_TEST_RESULT_FAIL;

	return tcu::TestStatus(res, qpGetTestResultName(res));
}

void DrawTestInstance::preRenderCommands (VkCommandBuffer cmdBuffer, VkImage colorTargetImage, const VkClearValue& clearColor)
{
	const DeviceInterface&			vk					= m_context.getDeviceInterface();
	const ImageSubresourceRange		subresourceRange	(VK_IMAGE_ASPECT_COLOR_BIT);
	const VkMemoryBarrier			memBarrier
	{
		VK_STRUCTURE_TYPE_MEMORY_BARRIER,
		DE_NULL,
		VK_ACCESS_TRANSFER_WRITE_BIT,
		VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
	};

	initialTransitionColor2DImage(vk, cmdBuffer, colorTargetImage, VK_IMAGE_LAYOUT_GENERAL,
								  vk::VK_ACCESS_TRANSFER_WRITE_BIT, vk::VK_PIPELINE_STAGE_TRANSFER_BIT);

	vk.cmdClearColorImage(cmdBuffer, colorTargetImage, VK_IMAGE_LAYOUT_GENERAL, &clearColor.color, 1, &subresourceRange);

	vk.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
						  0, 1, &memBarrier, 0, DE_NULL, 0, DE_NULL);
}

void DrawTestInstance::draw (VkCommandBuffer cmdBuffer, VkPipeline pipeline, VkBuffer vertexBuffer)
{
	const DeviceInterface&	vk					= m_context.getDeviceInterface();
	const VkDeviceSize		vertexBufferOffset	= 0;

	vk.cmdBindVertexBuffers(cmdBuffer, 0, 1, &vertexBuffer, &vertexBufferOffset);
	vk.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
	vk.cmdDraw(cmdBuffer, 3u, 1u, 0u, 0u);
}

#ifndef CTS_USES_VULKANSC
void DrawTestInstance::beginSecondaryCmdBuffer(VkCommandBuffer cmdBuffer, VkFormat colorAttachmentFormat, VkRenderingFlagsKHR renderingFlags)
{
	VkCommandBufferInheritanceRenderingInfoKHR inheritanceRenderingInfo
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_RENDERING_INFO_KHR,		// VkStructureType					sType;
		DE_NULL,																// const void*						pNext;
		renderingFlags,															// VkRenderingFlagsKHR				flags;
		0u,																		// uint32_t							viewMask;
		1u,																		// uint32_t							colorAttachmentCount;
		&colorAttachmentFormat,													// const VkFormat*					pColorAttachmentFormats;
		VK_FORMAT_UNDEFINED,													// VkFormat							depthAttachmentFormat;
		VK_FORMAT_UNDEFINED,													// VkFormat							stencilAttachmentFormat;
		VK_SAMPLE_COUNT_1_BIT,													// VkSampleCountFlagBits			rasterizationSamples;
	};
	const VkCommandBufferInheritanceInfo bufferInheritanceInfo = initVulkanStructure(&inheritanceRenderingInfo);

	VkCommandBufferUsageFlags usageFlags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	if (!m_data.groupParams->secondaryCmdBufferCompletelyContainsDynamicRenderpass)
		usageFlags |= VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;

	const VkCommandBufferBeginInfo commandBufBeginParams
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,							// VkStructureType					sType;
		DE_NULL,																// const void*						pNext;
		usageFlags,																// VkCommandBufferUsageFlags		flags;
		&bufferInheritanceInfo
	};

	const DeviceInterface& vk = m_context.getDeviceInterface();
	VK_CHECK(vk.beginCommandBuffer(cmdBuffer, &commandBufBeginParams));
}
#endif // CTS_USES_VULKANSC

void createTests (tcu::TestCaseGroup* testGroup, const SharedGroupParams groupParams)
{
	tcu::TestContext&	testCtx	= testGroup->getTestContext();
	const DrawParams	paramsFlat0	= { "vert", "fragFlatColor", "vertFlatColor", "fragFlatColor", groupParams };
	const DrawParams	paramsFlat1	= { "vertFlatColor", "frag", "vert", "frag", groupParams };

	const DrawParams	paramsNoPerspective0	= { "vert", "fragNoPerspective", "vertNoPerspective", "fragNoPerspective", groupParams };
	const DrawParams	paramsNoPerspective1	= { "vertNoPerspective", "frag", "vert", "frag", groupParams };

	testGroup->addChild(new DrawTestCase(testCtx, "flat_0", "Mismatching flat interpolation testcase 0.", paramsFlat0));
	testGroup->addChild(new DrawTestCase(testCtx, "flat_1", "Mismatching flat interpolation testcase 1.", paramsFlat1));

	testGroup->addChild(new DrawTestCase(testCtx, "noperspective_0", "Mismatching noperspective interpolation testcase 0.", paramsNoPerspective0));
	testGroup->addChild(new DrawTestCase(testCtx, "noperspective_1", "Mismatching noperspective interpolation testcase 1.", paramsNoPerspective1));
}

}	// anonymous

tcu::TestCaseGroup*	createDifferingInterpolationTests (tcu::TestContext& testCtx, const SharedGroupParams groupParams)
{
	return createTestGroup(testCtx, "differing_interpolation", "Tests for mismatched interpolation decorations.", createTests, groupParams);
}

}	// Draw
}	// vkt
