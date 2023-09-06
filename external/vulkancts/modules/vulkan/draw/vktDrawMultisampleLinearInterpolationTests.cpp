/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2022 The Khronos Group Inc.
 * Copyright (c) 2022 Google Inc.
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
 * \file vktDrawMultisampleLinearInterpolationTests.cpp
 * \brief InterpolateAt tests with linear interpolation
 *//*--------------------------------------------------------------------*/

#include "vktDrawMultisampleLinearInterpolationTests.hpp"

#include "vktDrawBaseClass.hpp"
#include "vkQueryUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vktTestGroupUtil.hpp"

namespace vkt
{
namespace Draw
{
namespace
{
using namespace vk;

class MultisampleLinearInterpolationTestInstance : public TestInstance
{
public:
						MultisampleLinearInterpolationTestInstance	(Context&						context,
																	 const tcu::IVec2				renderSize,
																	 const float					interpolationRange,
																	 const VkSampleCountFlagBits	sampleCountFlagBits,
																	 const SharedGroupParams		groupParams)
						: vkt::TestInstance		(context)
						, m_renderSize			(renderSize)
						, m_interpolationRange	(interpolationRange)
						, m_sampleCountFlagBits	(sampleCountFlagBits)
						, m_groupParams			(groupParams)
						{}

						~MultisampleLinearInterpolationTestInstance	(void)
						{}

	tcu::TestStatus		iterate										(void);

private:
	const tcu::IVec2			m_renderSize;
	const float					m_interpolationRange;
	const VkSampleCountFlagBits	m_sampleCountFlagBits;
	const SharedGroupParams		m_groupParams;
};

tcu::TestStatus MultisampleLinearInterpolationTestInstance::iterate (void)
{
	const DeviceInterface&						vk					= m_context.getDeviceInterface();
	const VkDevice								device				= m_context.getDevice();

	tcu::ConstPixelBufferAccess					resultPixelBufferAccesses[2];
	de::SharedPtr<Image>						colorTargetImages[2];
	de::SharedPtr<Image>						multisampleImages[2];

	const VkFormat								imageColorFormat	= VK_FORMAT_R8G8B8A8_UNORM;

	const std::string							vertShadernames[2]	= { "vertRef", "vertNoPer" };
	const std::string							fragShadernames[2]	= { "fragRef", "fragNoPer" };

	tcu::TestLog&								log					= m_context.getTestContext().getLog();

	const bool									useMultisampling	= m_sampleCountFlagBits == VK_SAMPLE_COUNT_1_BIT ? false : true;

	for (int draw = 0; draw < 2; draw++)
	{
		const Unique<VkShaderModule>					vs					(createShaderModule(vk, device, m_context.getBinaryCollection().get(vertShadernames[draw].c_str()), 0));
		const Unique<VkShaderModule>					fs					(createShaderModule(vk, device, m_context.getBinaryCollection().get(fragShadernames[draw].c_str()), 0));

		de::SharedPtr<Buffer>							vertexBuffer;

		const CmdPoolCreateInfo							cmdPoolCreateInfo	(m_context.getUniversalQueueFamilyIndex());
		Move<VkCommandPool>								cmdPool				= createCommandPool(vk, device, &cmdPoolCreateInfo);
		Move<VkCommandBuffer>							cmdBuffer			= allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
		Move<VkCommandBuffer>							secCmdBuffer;

		Move<VkRenderPass>								renderPass;

		std::vector<Move<VkImageView>>					colorTargetViews;
		std::vector<Move<VkImageView>>					multisampleViews;

		Move<VkFramebuffer>								framebuffer;

		Move<VkPipeline>								pipeline;
		const PipelineLayoutCreateInfo					pipelineLayoutCreateInfo;
		Move<VkPipelineLayout>							pipelineLayout		= createPipelineLayout(vk, device, &pipelineLayoutCreateInfo);

		const VkVertexInputAttributeDescription			vertInAttrDescs[2]	=
		{
			{ 0u, 0u, VK_FORMAT_R32G32B32A32_SFLOAT, 0u },
			{ 1u, 0u, VK_FORMAT_R32G32B32A32_SFLOAT, static_cast<deUint32>(sizeof(float) * 4) }
		};

		// Create color buffer images
		{
			const VkExtent3D		targetImageExtent		= { static_cast<deUint32>(m_renderSize.x()), static_cast<deUint32>(m_renderSize.y()), 1u };
			const VkImageUsageFlags	usage					= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
			const ImageCreateInfo	targetImageCreateInfo	(VK_IMAGE_TYPE_2D,
															 imageColorFormat,
															 targetImageExtent,
															 1u,
															 1u,
															 VK_SAMPLE_COUNT_1_BIT,
															 VK_IMAGE_TILING_OPTIMAL,
															 usage);

			colorTargetImages[draw] = Image::createAndAlloc(vk, device, targetImageCreateInfo,
															m_context.getDefaultAllocator(),
															m_context.getUniversalQueueFamilyIndex());

			if (useMultisampling)
			{
				const ImageCreateInfo	multisampleImageCreateInfo	(VK_IMAGE_TYPE_2D,
																	 imageColorFormat,
																	 targetImageExtent,
																	 1u,
																	 1u,
																	 m_sampleCountFlagBits,
																	 VK_IMAGE_TILING_OPTIMAL,
																	 usage);

				multisampleImages[draw] = Image::createAndAlloc(vk, device, multisampleImageCreateInfo,
																m_context.getDefaultAllocator(),
																m_context.getUniversalQueueFamilyIndex());
			}
		}

		{
			const ImageViewCreateInfo colorTargetViewInfo(colorTargetImages[draw]->object(),
														  VK_IMAGE_VIEW_TYPE_2D,
														  imageColorFormat);

			colorTargetViews.push_back(createImageView(vk, device, &colorTargetViewInfo));

			if (useMultisampling)
			{
				const ImageViewCreateInfo multisamplingTargetViewInfo(multisampleImages[draw]->object(),
																	  VK_IMAGE_VIEW_TYPE_2D,
																	  imageColorFormat);

				multisampleViews.push_back(createImageView(vk, device, &multisamplingTargetViewInfo));
			}
		}

		// Create render pass and frame buffer.
		if (!m_groupParams->useDynamicRendering)
		{
			RenderPassCreateInfo				renderPassCreateInfo;
			std::vector<VkImageView>			attachments;
			std::vector<VkAttachmentReference>	colorAttachmentRefs;
			std::vector<VkAttachmentReference>	multisampleAttachmentRefs;
			deUint32							attachmentNdx		= 0;

			{
				const VkAttachmentReference	colorAttachmentReference	=
				{
					attachmentNdx++,							// uint32_t			attachment;
					VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL	// VkImageLayout	layout;
				};

				colorAttachmentRefs.push_back(colorAttachmentReference);

				renderPassCreateInfo.addAttachment(AttachmentDescription(imageColorFormat,
																		 VK_SAMPLE_COUNT_1_BIT,
																		 VK_ATTACHMENT_LOAD_OP_CLEAR,
																		 VK_ATTACHMENT_STORE_OP_STORE,
																		 VK_ATTACHMENT_LOAD_OP_DONT_CARE,
																		 VK_ATTACHMENT_STORE_OP_DONT_CARE,
																		 VK_IMAGE_LAYOUT_UNDEFINED,
																		 VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL));

				if (useMultisampling)
				{
					const VkAttachmentReference	multiSampleAttachmentReference	=
					{
						attachmentNdx++,							// uint32_t			attachment;
						VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL	// VkImageLayout	layout;
					};

					multisampleAttachmentRefs.push_back(multiSampleAttachmentReference);

					renderPassCreateInfo.addAttachment(AttachmentDescription(imageColorFormat,
																			 m_sampleCountFlagBits,
																			 VK_ATTACHMENT_LOAD_OP_CLEAR,
																			 VK_ATTACHMENT_STORE_OP_DONT_CARE,
																			 VK_ATTACHMENT_LOAD_OP_DONT_CARE,
																			 VK_ATTACHMENT_STORE_OP_DONT_CARE,
																			 VK_IMAGE_LAYOUT_UNDEFINED,
																			 VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL));
				}
			}

			renderPassCreateInfo.addSubpass(SubpassDescription(VK_PIPELINE_BIND_POINT_GRAPHICS,
															   0,
															   0,
															   DE_NULL,
															   (deUint32)colorAttachmentRefs.size(),
															   useMultisampling ? &multisampleAttachmentRefs[0] : &colorAttachmentRefs[0],
															   useMultisampling ? &colorAttachmentRefs[0] : DE_NULL,
															   AttachmentReference(),
															   0,
															   DE_NULL));

			renderPass = createRenderPass(vk, device, &renderPassCreateInfo);

			for (deUint32 frameNdx = 0; frameNdx < colorTargetViews.size(); frameNdx++)
			{
				attachments.push_back(*colorTargetViews[frameNdx]);

				if (useMultisampling)
					attachments.push_back(*multisampleViews[frameNdx]);
			}

			const VkFramebufferCreateInfo	framebufferCreateInfo	=
			{
				VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,	// VkStructureType			sType;
				DE_NULL,									// const void*				pNext;
				0u,											// VkFramebufferCreateFlags	flags;
				*renderPass,								// VkRenderPass				renderPass;
				static_cast<deUint32>(attachments.size()),	// uint32_t					attachmentCount;
				&attachments[0],							// const VkImageView*		pAttachments;
				static_cast<deUint32>(m_renderSize.x()),	// uint32_t					width;
				static_cast<deUint32>(m_renderSize.y()),	// uint32_t					height;
				1u											// uint32_t					layers;
			};

			framebuffer = createFramebuffer(vk, device, &framebufferCreateInfo);
		}

		// Create vertex buffer.
		{
			const PositionColorVertex	vertices[]		=
			{
				// The first draw is for reference image.
				/*     ____            ____   */
				/*    /    \          |    |  */
				/*   /      \         |____|  */
				/*  /        \                */
				/* /__________\               */
				/*                            */
				/*    result        reference */
				/*                            */
				// In result shape the bottom vertices are deeper. When the drawn result image is a perfect square,
				// and color comparison with reference image is easy to make.
				PositionColorVertex(tcu::Vec4(	1.0f,						-1.0f,						0.0f,	1.0f),						tcu::Vec4(0.0f, m_interpolationRange, 0.0f, m_interpolationRange)),	// Top Right
				PositionColorVertex(tcu::Vec4(	-1.0f,						-1.0f,						0.0f,	1.0f),						tcu::Vec4(m_interpolationRange * 0.5f, m_interpolationRange * 0.5f, 0.0f, m_interpolationRange)),	// Top Left
				PositionColorVertex(tcu::Vec4(	draw == 0 ? 1.0f : 2.0f,	draw == 0 ? 1.0f : 2.0f,	0.0f,	draw == 0 ? 1.0f : 2.0f),	tcu::Vec4(m_interpolationRange * 0.5f, m_interpolationRange * 0.5f, 0.0f, m_interpolationRange)),	// Bottom Right
				PositionColorVertex(tcu::Vec4(	draw == 0 ? -1.0f : -2.0f,	draw == 0 ? 1.0f : 2.0f,	0.0f,	draw == 0 ? 1.0f : 2.0f),	tcu::Vec4(m_interpolationRange, 0.0f, 0.0f, m_interpolationRange)),	// Bottom Left
				PositionColorVertex(tcu::Vec4(	draw == 0 ? 1.0f : 2.0f,	draw == 0 ? 1.0f : 2.0f,	0.0f,	draw == 0 ? 1.0f : 2.0f),	tcu::Vec4(m_interpolationRange * 0.5f, m_interpolationRange * 0.5f, 0.0f, m_interpolationRange)),	// Bottom Right
				PositionColorVertex(tcu::Vec4(	-1.0f,						-1.0f,						0.0f,	1.0f),						tcu::Vec4(m_interpolationRange * 0.5f, m_interpolationRange * 0.5f, 0.0f, m_interpolationRange))	// Top Left
			};

			const VkDeviceSize			dataSize		= sizeof(vertices);
										vertexBuffer	= Buffer::createAndAlloc(vk, device, BufferCreateInfo(dataSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT), m_context.getDefaultAllocator(), MemoryRequirement::HostVisible);
			deUint8*					ptr				= static_cast<deUint8*>(vertexBuffer->getBoundMemory().getHostPtr());

			deMemcpy(ptr, vertices, static_cast<size_t>(dataSize));
			flushMappedMemoryRange(vk, device, vertexBuffer->getBoundMemory().getMemory(), vertexBuffer->getBoundMemory().getOffset(), VK_WHOLE_SIZE);
		}

		// Create pipeline.
		{
			const PipelineCreateInfo::ColorBlendState::Attachment	vkCbAttachmentState;

			VkViewport												viewport						= makeViewport(m_renderSize.x(), m_renderSize.y());
			VkRect2D												scissor							= makeRect2D(m_renderSize.x(), m_renderSize.y());

			const std::vector<deUint32>								sampleMask						= { 0xfffffff, 0xfffffff };

			const VkVertexInputBindingDescription					vertexInputBindingDescription	= { 0, (deUint32)sizeof(tcu::Vec4) * 2, VK_VERTEX_INPUT_RATE_VERTEX };
			PipelineCreateInfo::VertexInputState					vertexInputState				= PipelineCreateInfo::VertexInputState(1, &vertexInputBindingDescription, 2, vertInAttrDescs);

			PipelineCreateInfo										pipelineCreateInfo(*pipelineLayout, *renderPass, 0, 0);

			pipelineCreateInfo.addShader(PipelineCreateInfo::PipelineShaderStage(*vs, "main", VK_SHADER_STAGE_VERTEX_BIT));
			pipelineCreateInfo.addShader(PipelineCreateInfo::PipelineShaderStage(*fs, "main", VK_SHADER_STAGE_FRAGMENT_BIT));
			pipelineCreateInfo.addState(PipelineCreateInfo::VertexInputState(vertexInputState));
			pipelineCreateInfo.addState(PipelineCreateInfo::InputAssemblerState(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST));
			pipelineCreateInfo.addState(PipelineCreateInfo::ColorBlendState(1, &vkCbAttachmentState));
			pipelineCreateInfo.addState(PipelineCreateInfo::ViewportState(1, std::vector<VkViewport>(1, viewport), std::vector<VkRect2D>(1, scissor)));
			pipelineCreateInfo.addState(PipelineCreateInfo::DepthStencilState());
			pipelineCreateInfo.addState(PipelineCreateInfo::RasterizerState());
			pipelineCreateInfo.addState(PipelineCreateInfo::MultiSampleState(m_sampleCountFlagBits, false, 0.0f, sampleMask));

#ifndef CTS_USES_VULKANSC
			VkPipelineRenderingCreateInfo							renderingCreateInfo
			{
				VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR,
				DE_NULL,
				0u,
				1u,
				&imageColorFormat,
				VK_FORMAT_UNDEFINED,
				VK_FORMAT_UNDEFINED
			};

			if (m_groupParams->useDynamicRendering)
				pipelineCreateInfo.pNext = &renderingCreateInfo;
#endif // CTS_USES_VULKANSC

			pipeline = createGraphicsPipeline(vk, device, DE_NULL, &pipelineCreateInfo);
		}

		// Draw quad and read results.
		{
			const VkQueue					queue				= m_context.getUniversalQueue();
			const VkClearValue				clearColor			= { { { 0.0f, 0.0f, 0.0f, 1.0f } } };
			const ImageSubresourceRange		subresourceRange	(VK_IMAGE_ASPECT_COLOR_BIT);
			const VkRect2D					renderArea			= makeRect2D(m_renderSize.x(), m_renderSize.y());
			const VkBuffer					buffer				= vertexBuffer->object();
			const VkOffset3D				zeroOffset			= { 0, 0, 0 };

			std::vector<VkClearValue>		clearValues			(2, clearColor);

			auto drawCommands = [&](VkCommandBuffer cmdBuff)
			{
				const VkDeviceSize vertexBufferOffset = 0;
				vk.cmdBindVertexBuffers(cmdBuff, 0, 1, &buffer, &vertexBufferOffset);
				vk.cmdBindPipeline(cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline);
				vk.cmdDraw(cmdBuff, 6u, 1u, 0u, 0u);
			};

			clearColorImage(vk, device, queue, m_context.getUniversalQueueFamilyIndex(),
				colorTargetImages[draw]->object(), tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f),
				VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
				VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0u, 1u);

#ifndef CTS_USES_VULKANSC
			auto preRenderBarriers = [&]()
			{
				// Transition Images
				initialTransitionColor2DImage(vk, *cmdBuffer, colorTargetImages[draw]->object(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
					VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

				if (useMultisampling)
				{
					initialTransitionColor2DImage(vk, *cmdBuffer, multisampleImages[draw]->object(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
						VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
				}
			};

			if (m_groupParams->useDynamicRendering)
			{
				const deUint32 imagesCount = static_cast<deUint32>(colorTargetViews.size());

				std::vector<VkRenderingAttachmentInfo> colorAttachments(imagesCount,
				{
					VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR,	// VkStructureType			sType;
					DE_NULL,											// const void*				pNext;
					DE_NULL,											// VkImageView				imageView;
					VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,			// VkImageLayout			imageLayout;
					VK_RESOLVE_MODE_NONE,								// VkResolveModeFlagBits	resolveMode;
					DE_NULL,											// VkImageView				resolveImageView;
					VK_IMAGE_LAYOUT_GENERAL,							// VkImageLayout			resolveImageLayout;
					VK_ATTACHMENT_LOAD_OP_CLEAR,						// VkAttachmentLoadOp		loadOp;
					VK_ATTACHMENT_STORE_OP_STORE,						// VkAttachmentStoreOp		storeOp;
					clearColor											// VkClearValue				clearValue;
				});

				for (deUint32 i = 0; i < imagesCount; ++i)
				{
					if (useMultisampling)
					{
						colorAttachments[i].imageView			= *multisampleViews[i];
						colorAttachments[i].resolveMode			= VK_RESOLVE_MODE_AVERAGE_BIT;
						colorAttachments[i].resolveImageView	= *colorTargetViews[i];
					}
					else
					{
						colorAttachments[i].imageView			= *colorTargetViews[i];
					}
				}

				VkRenderingInfo renderingInfo
				{
					VK_STRUCTURE_TYPE_RENDERING_INFO,	// VkStructureType						sType;
					DE_NULL,							// const void*							pNext;
					0,									// VkRenderingFlagsKHR					flags;
					renderArea,							// VkRect2D								renderArea;
					1u,									// deUint32								layerCount;
					0u,									// deUint32								viewMask;
					imagesCount,						// deUint32								colorAttachmentCount;
					colorAttachments.data(),			// const VkRenderingAttachmentInfoKHR*	pColorAttachments;
					DE_NULL,							// const VkRenderingAttachmentInfoKHR*	pDepthAttachment;
					DE_NULL,							// const VkRenderingAttachmentInfoKHR*	pStencilAttachment;
				};

				if (m_groupParams->useSecondaryCmdBuffer)
				{
					VkCommandBufferInheritanceRenderingInfoKHR inheritanceRenderingInfo
					{
						VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_RENDERING_INFO_KHR,		// VkStructureType						sType;
						DE_NULL,																// const void*							pNext;
						0u,																		// VkRenderingFlagsKHR					flags;
						0u,																		// uint32_t								viewMask;
						1u,																		// uint32_t								colorAttachmentCount;
						&imageColorFormat,														// const VkFormat*						pColorAttachmentFormats;
						VK_FORMAT_UNDEFINED,													// VkFormat								depthAttachmentFormat;
						VK_FORMAT_UNDEFINED,													// VkFormat								stencilAttachmentFormat;
						m_sampleCountFlagBits													// VkSampleCountFlagBits				rasterizationSamples;
					};

					const VkCommandBufferInheritanceInfo	bufferInheritanceInfo = initVulkanStructure(&inheritanceRenderingInfo);
					VkCommandBufferBeginInfo				commandBufBeginParams
					{
						VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,							// VkStructureType					sType;
						DE_NULL,																// const void*						pNext;
						VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,							// VkCommandBufferUsageFlags		flags;
						&bufferInheritanceInfo
					};

					secCmdBuffer = allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_SECONDARY);

					// record secondary command buffer
					if (m_groupParams->secondaryCmdBufferCompletelyContainsDynamicRenderpass)
					{
						inheritanceRenderingInfo.flags = VK_RENDERING_CONTENTS_SECONDARY_COMMAND_BUFFERS_BIT;
						VK_CHECK(vk.beginCommandBuffer(*secCmdBuffer, &commandBufBeginParams));
						vk.cmdBeginRendering(*secCmdBuffer, &renderingInfo);
					}
					else
					{
						commandBufBeginParams.flags |= VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;
						VK_CHECK(vk.beginCommandBuffer(*secCmdBuffer, &commandBufBeginParams));
					}

					drawCommands(*secCmdBuffer);

					if (m_groupParams->secondaryCmdBufferCompletelyContainsDynamicRenderpass)
						endRendering(vk, *secCmdBuffer);

					endCommandBuffer(vk, *secCmdBuffer);

					// record primary command buffer
					beginCommandBuffer(vk, *cmdBuffer, 0u);

					preRenderBarriers();

					if (!m_groupParams->secondaryCmdBufferCompletelyContainsDynamicRenderpass)
					{
						renderingInfo.flags = VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS;
						vk.cmdBeginRendering(*cmdBuffer, &renderingInfo);
					}
					vk.cmdExecuteCommands(*cmdBuffer, 1u, &*secCmdBuffer);

					if (!m_groupParams->secondaryCmdBufferCompletelyContainsDynamicRenderpass)
						endRendering(vk, *cmdBuffer);
					endCommandBuffer(vk, *cmdBuffer);
				}
				else
				{
					beginCommandBuffer(vk, *cmdBuffer, 0u);
					preRenderBarriers();

					vk.cmdBeginRendering(*cmdBuffer, &renderingInfo);
					drawCommands(*cmdBuffer);
					endRendering(vk, *cmdBuffer);

					endCommandBuffer(vk, *cmdBuffer);
				}
			}
			else
#endif // CTS_USES_VULKANSC
			{
				beginCommandBuffer(vk, *cmdBuffer, 0u);

				const deUint32 imagesCount = static_cast<deUint32>(colorTargetViews.size() + multisampleViews.size());

				beginRenderPass(vk, *cmdBuffer, *renderPass, *framebuffer, renderArea, imagesCount, &clearValues[0]);
				drawCommands(*cmdBuffer);
				endRenderPass(vk, *cmdBuffer);

				endCommandBuffer(vk, *cmdBuffer);
			}

			submitCommandsAndWait(vk, device, queue, cmdBuffer.get());

			resultPixelBufferAccesses[draw] = colorTargetImages[draw]->readSurface(queue, m_context.getDefaultAllocator(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, zeroOffset, m_renderSize.x(), m_renderSize.y(), VK_IMAGE_ASPECT_COLOR_BIT);
		}
	}

	if (!tcu::floatThresholdCompare(log, "Result", "Image comparison result", resultPixelBufferAccesses[0], resultPixelBufferAccesses[1], tcu::Vec4(0.005f), tcu::COMPARE_LOG_RESULT))
		return tcu::TestStatus::fail("Rendered color image is not correct");

	return tcu::TestStatus::pass("Success");
}

class MultisampleLinearInterpolationTestCase : public TestCase
{
	public:
								MultisampleLinearInterpolationTestCase	(tcu::TestContext&				context,
																		 const char*					name,
																		 const char*					desc,
																		 const tcu::IVec2				renderSize,
																		 const float					interpolationRange,
																		 const tcu::Vec2				offset,
																		 const VkSampleCountFlagBits	sampleCountFlagBits,
																		 const SharedGroupParams		groupParams)
								: vkt::TestCase(context, name, desc)
								, m_renderSize			(renderSize)
								, m_interpolationRange	(interpolationRange)
								, m_offset				(offset)
								, m_sampleCountFlagBits	(sampleCountFlagBits)
								, m_groupParams			(groupParams)
								{}

								~MultisampleLinearInterpolationTestCase	(void)
								{}

	virtual	void				initPrograms		(SourceCollections& programCollection) const;
	virtual void				checkSupport		(Context& context) const;
	virtual TestInstance*		createInstance		(Context& context) const;

private:
	const tcu::IVec2			m_renderSize;
	const float					m_interpolationRange;
	const tcu::Vec2				m_offset;
	const VkSampleCountFlagBits	m_sampleCountFlagBits;
	const SharedGroupParams		m_groupParams;
};

void MultisampleLinearInterpolationTestCase::initPrograms (SourceCollections& programCollection) const
{
	// Reference vertex shader.
	{
		std::ostringstream vrt;

		vrt << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
			<< "\n"
			<< "layout(location = 0) in vec4 in_position;\n"
			<< "layout(location = 1) in vec4 in_color;\n"
			<< "layout(location = 0) out vec4 out_color;\n"
			<< "\n"
			<< "void main()\n"
			<< "{\n"
			<< "    gl_PointSize = 1.0;\n"
			<< "    gl_Position  = in_position;\n"
			<< "    out_color    = in_color;\n"
			<< "}\n";

		programCollection.glslSources.add("vertRef") << glu::VertexSource(vrt.str());
	}

	// Noperspective vertex shader.
	{
		std::ostringstream vrt;

		vrt << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
			<< "\n"
			<< "layout(location = 0) in vec4 in_position;\n"
			<< "layout(location = 1) in vec4 in_color;\n"
			<< "layout(location = 0) noperspective out vec4 out_color;\n"
			<< "\n"
			<< "void main()\n"
			<< "{\n"
			<< "    gl_PointSize = 1.0;\n"
			<< "    gl_Position  = in_position;\n"
			<< "    out_color    = in_color;\n"
			<< "}\n";

		programCollection.glslSources.add("vertNoPer") << glu::VertexSource(vrt.str());
	}

	// Reference fragment shader.
	{
		std::ostringstream frg;

		frg << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
			<< "layout(location = 0) in vec4 in_color;\n"
			<< "layout(location = 0) out vec4 out_color;\n"
			<< "void main()\n"
			<< "{\n"
			<< "    vec4 out_color_y = mix(vec4(0.0, 1.0, 0.0, 1.0), vec4(1.0, 0.0, 0.0, 1.0), gl_FragCoord.y / " << static_cast<float>(m_renderSize.y()) << " + " << m_offset.y() / static_cast<float>(m_renderSize.y()) << ");\n"
			<< "    vec4 out_color_x = mix(vec4(1.0, 0.0, 0.0, 1.0), vec4(0.0, 1.0, 0.0, 1.0), gl_FragCoord.x / " << static_cast<float>(m_renderSize.x()) << " + " << m_offset.x() / static_cast<float>(m_renderSize.x()) << ");\n"
			<< "    out_color = 0.5 * (out_color_y + out_color_x);\n"
			<< "}\n";

		programCollection.glslSources.add("fragRef") << glu::FragmentSource(frg.str());
	}

	// Noperspective fragment shader.
	{
		std::ostringstream frg;

		frg << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
			<< "layout(location = 0) noperspective in vec4 in_color;\n"
			<< "layout(location = 0) out vec4 out_color;\n"
			<< "void main()\n"
			<< "{\n"
			<< "    vec4 out_color_offset = interpolateAtOffset(in_color, vec2(" << m_offset.x() << ", " << m_offset.y() << "));\n"
			<< "    vec4 out_color_sample = interpolateAtSample(in_color, gl_SampleID);\n"
			<< "    out_color = (0.5 * (out_color_offset + out_color_sample));\n"
			<< "    out_color /= " << m_interpolationRange << ";\n";

		// Run additional sample comparison test. If it fails, we write 1.0 to blue color channel.
		frg << "    vec4 diff = out_color_sample - interpolateAtOffset(in_color, gl_SamplePosition - vec2(0.5));"
			<< "    float min_precision = 0.000001;\n"
			<< "    if (diff.x > min_precision && diff.y > min_precision && diff.z > min_precision && diff.w > min_precision)\n"
			<< "    {\n"
			<< "        out_color.z = 1.0;\n"
			<< "    }\n";

		frg << "}\n";

		programCollection.glslSources.add("fragNoPer") << glu::FragmentSource(frg.str());
	}
}

void MultisampleLinearInterpolationTestCase::checkSupport (Context& context) const
{
	context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_SAMPLE_RATE_SHADING);

	if (!(m_sampleCountFlagBits & context.getDeviceProperties().limits.framebufferColorSampleCounts))
		TCU_THROW(NotSupportedError, "Multisampling with " + de::toString(m_sampleCountFlagBits) + " samples not supported");

#ifndef CTS_USES_VULKANSC
	if (m_groupParams->useDynamicRendering)
		context.requireDeviceFunctionality("VK_KHR_dynamic_rendering");

	if (context.isDeviceFunctionalitySupported("VK_KHR_portability_subset") &&
		!context.getPortabilitySubsetFeatures().shaderSampleRateInterpolationFunctions)
	{
		TCU_THROW(NotSupportedError, "VK_KHR_portability_subset: Shader sample rate interpolation functions are not supported by this implementation");
	}
#endif // CTS_USES_VULKANSC
}

TestInstance* MultisampleLinearInterpolationTestCase::createInstance (Context& context) const
{
	return new MultisampleLinearInterpolationTestInstance(context, m_renderSize, m_interpolationRange, m_sampleCountFlagBits, m_groupParams);
}

void createTests (tcu::TestCaseGroup* testGroup, const SharedGroupParams groupParams)
{
	tcu::TestContext&	testCtx	= testGroup->getTestContext();

	struct
	{
		const std::string	name;
		const tcu::Vec2		value;
	} offsets[]	=
	{
		{ "no_offset",	tcu::Vec2(0.0f, 0.0f) },
		{ "offset_min",	tcu::Vec2(-0.5f, -0.5f) },
		{ "offset_max",	tcu::Vec2(0.4375f, 0.4375f) }
	};

	struct
	{
		const std::string		name;
		VkSampleCountFlagBits	value;
	} flagBits[] =
	{
		{ "1_sample",	VK_SAMPLE_COUNT_1_BIT },
		{ "2_samples",	VK_SAMPLE_COUNT_2_BIT },
		{ "4_samples",	VK_SAMPLE_COUNT_4_BIT },
		{ "8_samples",	VK_SAMPLE_COUNT_8_BIT },
		{ "16_samples",	VK_SAMPLE_COUNT_16_BIT },
		{ "32_samples",	VK_SAMPLE_COUNT_32_BIT },
		{ "64_samples",	VK_SAMPLE_COUNT_64_BIT }
	};

	for (const auto& offset : offsets)
	{
		for (const auto& flagBit : flagBits)
		{
			// reduce number of tests for dynamic rendering cases where secondary command buffer is used
			if (groupParams->useSecondaryCmdBuffer && (flagBit.value > VK_SAMPLE_COUNT_4_BIT))
				break;

			testGroup->addChild(new MultisampleLinearInterpolationTestCase(testCtx, (offset.name + "_" + flagBit.name).c_str(), ".", tcu::IVec2(16, 16), 1.0f, offset.value, flagBit.value, groupParams));
		}
	}
}

}	// anonymous

tcu::TestCaseGroup*	createMultisampleLinearInterpolationTests (tcu::TestContext& testCtx, const SharedGroupParams groupParams)
{
	return createTestGroup(testCtx, "linear_interpolation", "Tests for linear interpolation decorations.", createTests, groupParams);
}

}	// Draw
}	// vkt
