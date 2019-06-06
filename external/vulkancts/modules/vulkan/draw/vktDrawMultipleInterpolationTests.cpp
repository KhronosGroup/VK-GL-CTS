/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2019 The Khronos Group Inc.
 * Copyright (c) 2019 Google Inc.
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
 * \brief Tests for multiple interpolation decorations in a shader stage
 *//*--------------------------------------------------------------------*/

#include "vktDrawMultipleInterpolationTests.hpp"

#include "tcuStringTemplate.hpp"
#include "vkCmdUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vktDrawBaseClass.hpp"
#include "vktTestGroupUtil.hpp"

namespace vkt
{
namespace Draw
{
namespace
{

enum Interpolation
{
	SMOOTH			= 0,
	FLAT			= 1,
	NOPERSPECTIVE	= 2,
	CENTROID		= 3
};

struct DrawParams
{
	vk::VkFormat				format;
	tcu::UVec2					size;
	vk::VkSampleCountFlagBits	samples;
};

template<typename T>
inline de::SharedPtr<vk::Move<T> > makeSharedPtr(vk::Move<T> move)
{
	return de::SharedPtr<vk::Move<T> >(new vk::Move<T>(move));
}

const char* interpolationToString (Interpolation interpolation)
{
	switch (interpolation)
	{
		case SMOOTH:
			return "smooth";
		case FLAT:
			return "flat";
		case NOPERSPECTIVE:
			return "noperspective";
		case CENTROID:
			return "centroid";
		default:
			DE_FATAL("Invalid interpolation enum");
	}

	return "";
}

class DrawTestInstance : public TestInstance
{
public:
					DrawTestInstance	(Context& context, DrawParams params);
	void			render				(std::vector<de::SharedPtr<Image> >& colorTargetImages,
										 tcu::ConstPixelBufferAccess* frames,
										 const char* vsName,
										 const char* fsName);
	bool			compare				(const tcu::ConstPixelBufferAccess& result,
										 const tcu::ConstPixelBufferAccess& reference);
	tcu::TestStatus	iterate				(void);
private:
	DrawParams		m_params;
};

DrawTestInstance::DrawTestInstance (Context& context, DrawParams params)
	: TestInstance	(context)
	, m_params		(params)
{
}

class DrawTestCase : public TestCase
{
public:
							DrawTestCase	(tcu::TestContext&	testCtx,
											 const std::string&	name,
											 const std::string&	description,
											 const DrawParams	params);
							~DrawTestCase	(void);
	virtual void			initPrograms	(vk::SourceCollections& programCollection) const;
	virtual void			checkSupport	(Context& context) const;
	virtual TestInstance*	createInstance	(Context& context) const;
private:
	const DrawParams		m_params;
};

DrawTestCase::DrawTestCase (tcu::TestContext& testCtx,
							const std::string& name,
							const std::string& description,
							const DrawParams params)
	: TestCase	(testCtx, name, description)
	, m_params	(params)
{
}

DrawTestCase::~DrawTestCase (void)
{
}

void DrawTestCase::initPrograms (vk::SourceCollections& programCollection) const
{
	const std::string					vertShader_multi	(
		"#version 430\n"
		"layout(location = 0) in vec4 in_position;\n"
		"layout(location = 1) in vec4 in_color;\n"
		"layout(location = 0) out vec4 out_color_smooth;\n"
		"layout(location = 1) flat out vec4 out_color_flat;\n"
		"layout(location = 2) noperspective out vec4 out_color_noperspective;\n"
		"layout(location = 3) centroid out vec4 out_color_centroid;\n"
		"void main()\n"
		"{\n"
		"    out_color_smooth = in_color;\n"
		"    out_color_flat = in_color;\n"
		"    out_color_noperspective = in_color;\n"
		"    out_color_centroid = in_color;\n"
		"    gl_Position = in_position;\n"
		"}\n");

	const std::string					fragShader_multi	(
		"#version 430\n"
		"layout(location = 0) in vec4 in_color_smooth;\n"
		"layout(location = 1) flat in vec4 in_color_flat;\n"
		"layout(location = 2) noperspective in vec4 in_color_noperspective;\n"
		"layout(location = 3) centroid in vec4 in_color_centroid;\n"
		"layout(location = " + de::toString(SMOOTH) + ") out vec4 out_color_smooth;\n"
		"layout(location = " + de::toString(FLAT) + ") out vec4 out_color_flat;\n"
		"layout(location = " + de::toString(NOPERSPECTIVE) + ") out vec4 out_color_noperspective;\n"
		"layout(location = " + de::toString(CENTROID) + ") out vec4 out_color_centroid;\n"
		"void main()\n"
		"{\n"
		"    out_color_smooth = in_color_smooth;\n"
		"    out_color_flat = in_color_flat;\n"
		"    out_color_noperspective = in_color_noperspective;\n"
		"    out_color_centroid = in_color_centroid;\n"
		"}\n");

	const tcu::StringTemplate			vertShader_single	(std::string(
		"#version 430\n"
		"layout(location = 0) in vec4 in_position;\n"
		"layout(location = 1) in vec4 in_color;\n"
		"layout(location = 0) ${qualifier:opt} out vec4 out_color;\n"
		"void main()\n"
		"{\n"
		"    out_color = in_color;\n"
		"    gl_Position = in_position;\n"
		"}\n"));

	const tcu::StringTemplate			fragShader_single	(std::string(
		"#version 430\n"
		"layout(location = 0) ${qualifier:opt} in vec4 in_color;\n"
		"layout(location = 0) out vec4 out_color;\n"
		"void main()\n"
		"{\n"
		"    out_color = in_color;\n"
		"}\n"));

	std::map<std::string, std::string>	smooth;
	std::map<std::string, std::string>	flat;
	std::map<std::string, std::string>	noperspective;
	std::map<std::string, std::string>	centroid;

	flat["qualifier"]			= "flat";
	noperspective["qualifier"]	= "noperspective";
	centroid["qualifier"]		= "centroid";

	programCollection.glslSources.add("vert_multi")			<< glu::VertexSource(vertShader_multi);
	programCollection.glslSources.add("frag_multi")			<< glu::FragmentSource(fragShader_multi);
	programCollection.glslSources.add("vert_smooth")		<< glu::VertexSource(vertShader_single.specialize(smooth));
	programCollection.glslSources.add("frag_smooth")		<< glu::FragmentSource(fragShader_single.specialize(smooth));
	programCollection.glslSources.add("vert_flat")			<< glu::VertexSource(vertShader_single.specialize(flat));
	programCollection.glslSources.add("frag_flat")			<< glu::FragmentSource(fragShader_single.specialize(flat));
	programCollection.glslSources.add("vert_noperspective")	<< glu::VertexSource(vertShader_single.specialize(noperspective));
	programCollection.glslSources.add("frag_noperspective")	<< glu::FragmentSource(fragShader_single.specialize(noperspective));
	programCollection.glslSources.add("vert_centroid")		<< glu::VertexSource(vertShader_single.specialize(centroid));
	programCollection.glslSources.add("frag_centroid")		<< glu::FragmentSource(fragShader_single.specialize(centroid));
}

void DrawTestCase::checkSupport (Context& context) const
{
	if (!(m_params.samples & context.getDeviceProperties().limits.framebufferColorSampleCounts))
		throw tcu::NotSupportedError("Multisampling with " + de::toString(m_params.samples) + " samples not supported");
}

TestInstance* DrawTestCase::createInstance (Context& context) const
{
	return new DrawTestInstance(context, m_params);
}

void DrawTestInstance::render (std::vector<de::SharedPtr<Image> >& colorTargetImages,
							   tcu::ConstPixelBufferAccess* frames,
							   const char* vsName,
							   const char* fsName)
{
	const bool												useMultisampling	= (m_params.samples != vk::VK_SAMPLE_COUNT_1_BIT);
	const vk::DeviceInterface&								vk					= m_context.getDeviceInterface();
	const vk::VkDevice										device				= m_context.getDevice();
	const vk::Unique<vk::VkShaderModule>					vs					(createShaderModule(vk, device, m_context.getBinaryCollection().get(vsName), 0));
	const vk::Unique<vk::VkShaderModule>					fs					(createShaderModule(vk, device, m_context.getBinaryCollection().get(fsName), 0));
	const CmdPoolCreateInfo									cmdPoolCreateInfo	= m_context.getUniversalQueueFamilyIndex();
	vk::Move<vk::VkCommandPool>								cmdPool				= createCommandPool(vk, device, &cmdPoolCreateInfo);
	vk::Move<vk::VkCommandBuffer>							cmdBuffer			= vk::allocateCommandBuffer(vk, device, *cmdPool, vk::VK_COMMAND_BUFFER_LEVEL_PRIMARY);
	std::vector<de::SharedPtr<Image> >						multisampleImages;
	std::vector<de::SharedPtr<vk::Move<vk::VkImageView> > >	colorTargetViews;
	std::vector<de::SharedPtr<vk::Move<vk::VkImageView> > >	multisampleViews;
	de::SharedPtr<Buffer>									vertexBuffer;
	vk::Move<vk::VkRenderPass>								renderPass;
	vk::Move<vk::VkFramebuffer>								framebuffer;
	vk::Move<vk::VkPipeline>								pipeline;

	// Create color buffer images
	for (deUint32 frameNdx = 0; frameNdx < colorTargetImages.size(); frameNdx++)
	{
		const vk::VkExtent3D		targetImageExtent		= { m_params.size.x(), m_params.size.y(), 1 };
		const vk::VkImageUsageFlags	usage					= vk::VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | vk::VK_IMAGE_USAGE_TRANSFER_SRC_BIT | vk::VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		const ImageCreateInfo		targetImageCreateInfo	(vk::VK_IMAGE_TYPE_2D,
															 m_params.format,
															 targetImageExtent,
															 1,
															 1,
															 vk::VK_SAMPLE_COUNT_1_BIT,
															 vk::VK_IMAGE_TILING_OPTIMAL,
															 usage);

		colorTargetImages[frameNdx] = Image::createAndAlloc(vk, device, targetImageCreateInfo,
															m_context.getDefaultAllocator(),
															m_context.getUniversalQueueFamilyIndex());

		if (useMultisampling)
		{
			const ImageCreateInfo multisampleImageCreateInfo (vk::VK_IMAGE_TYPE_2D,
															  m_params.format,
															  targetImageExtent,
															  1,
															  1,
															  m_params.samples,
															  vk::VK_IMAGE_TILING_OPTIMAL,
															  usage);

			multisampleImages.push_back(Image::createAndAlloc(vk, device, multisampleImageCreateInfo,
															  m_context.getDefaultAllocator(),
															  m_context.getUniversalQueueFamilyIndex()));
		}
	}

	// Create render pass and framebuffer
	{
		RenderPassCreateInfo					renderPassCreateInfo;
		std::vector<vk::VkImageView>			attachments;
		std::vector<vk::VkAttachmentReference>	colorAttachmentRefs;
		std::vector<vk::VkAttachmentReference>	multisampleAttachmentRefs;
		deUint32								attachmentNdx				= 0;

		for (deUint32 frameNdx = 0; frameNdx < colorTargetImages.size(); frameNdx++)
		{
			const ImageViewCreateInfo		colorTargetViewInfo			(colorTargetImages[frameNdx]->object(),
																		 vk::VK_IMAGE_VIEW_TYPE_2D,
																		 m_params.format);

			const vk::VkAttachmentReference	colorAttachmentReference	=
			{
				attachmentNdx++,
				vk::VK_IMAGE_LAYOUT_GENERAL
			};

			colorTargetViews.push_back(makeSharedPtr(createImageView(vk, device, &colorTargetViewInfo)));
			colorAttachmentRefs.push_back(colorAttachmentReference);

			renderPassCreateInfo.addAttachment(AttachmentDescription(m_params.format,
																	 vk::VK_SAMPLE_COUNT_1_BIT,
																	 vk::VK_ATTACHMENT_LOAD_OP_CLEAR,
																	 vk::VK_ATTACHMENT_STORE_OP_STORE,
																	 vk::VK_ATTACHMENT_LOAD_OP_DONT_CARE,
																	 vk::VK_ATTACHMENT_STORE_OP_DONT_CARE,
																	 vk::VK_IMAGE_LAYOUT_UNDEFINED,
																	 vk::VK_IMAGE_LAYOUT_GENERAL));

			if (useMultisampling)
			{
				const ImageViewCreateInfo		multisamplingTargetViewInfo		(multisampleImages[frameNdx]->object(),
																				 vk::VK_IMAGE_VIEW_TYPE_2D,
																				 m_params.format);

				const vk::VkAttachmentReference	multiSampleAttachmentReference	=
				{
					attachmentNdx++,
					vk::VK_IMAGE_LAYOUT_GENERAL
				};

				multisampleViews.push_back(makeSharedPtr(createImageView(vk, device, &multisamplingTargetViewInfo)));
				multisampleAttachmentRefs.push_back(multiSampleAttachmentReference);

				renderPassCreateInfo.addAttachment(AttachmentDescription(m_params.format,
																		 m_params.samples,
																		 vk::VK_ATTACHMENT_LOAD_OP_CLEAR,
																		 vk::VK_ATTACHMENT_STORE_OP_STORE,
																		 vk::VK_ATTACHMENT_LOAD_OP_DONT_CARE,
																		 vk::VK_ATTACHMENT_STORE_OP_DONT_CARE,
																		 vk::VK_IMAGE_LAYOUT_UNDEFINED,
																		 vk::VK_IMAGE_LAYOUT_GENERAL));
			}
		}

		renderPassCreateInfo.addSubpass(SubpassDescription(vk::VK_PIPELINE_BIND_POINT_GRAPHICS,
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
			attachments.push_back(**colorTargetViews[frameNdx]);

			if (useMultisampling)
				attachments.push_back(**multisampleViews[frameNdx]);
		}

		const vk::VkFramebufferCreateInfo framebufferCreateInfo =
		{
			vk::VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
			DE_NULL,
			0u,
			*renderPass,
			(deUint32)attachments.size(),
			&attachments[0],
			m_params.size.x(),
			m_params.size.y(),
			1
		};

		framebuffer = createFramebuffer(vk, device, &framebufferCreateInfo);
	}

	// Create vertex buffer.
	{
		const PositionColorVertex	vertices[]		=
		{
			PositionColorVertex(
				tcu::Vec4(-1.5f, -0.4f, 1.0f, 2.0f),	// Coord
				tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f)),		// Color

			PositionColorVertex(
				tcu::Vec4(0.4f, -0.4f, 0.5f, 0.5f),		// Coord
				tcu::Vec4(0.0f, 1.0f, 0.0f, 1.0f)),		// Color

			PositionColorVertex(
				tcu::Vec4(0.3f, 0.8f, 0.0f, 1.0f),		// Coord
				tcu::Vec4(0.0f, 0.0f, 1.0f, 1.0f))		// Color
		};

		const vk::VkDeviceSize		dataSize		= DE_LENGTH_OF_ARRAY(vertices) * sizeof(PositionColorVertex);
									vertexBuffer	= Buffer::createAndAlloc(vk,
																			 device,
																			 BufferCreateInfo(dataSize, vk::VK_BUFFER_USAGE_VERTEX_BUFFER_BIT),
																			 m_context.getDefaultAllocator(),
																			 vk::MemoryRequirement::HostVisible);
		deUint8*					ptr				= reinterpret_cast<deUint8*>(vertexBuffer->getBoundMemory().getHostPtr());

		deMemcpy(ptr, vertices, static_cast<size_t>(dataSize));
		flushMappedMemoryRange(vk,
							   device,
							   vertexBuffer->getBoundMemory().getMemory(),
							   vertexBuffer->getBoundMemory().getOffset(),
							   VK_WHOLE_SIZE);
	}

	// Create pipeline
	{
		const vk::VkViewport											viewport							= vk::makeViewport(m_params.size.x(), m_params.size.y());
		const vk::VkRect2D												scissor								= vk::makeRect2D(m_params.size.x(), m_params.size.y());
		const PipelineLayoutCreateInfo									pipelineLayoutCreateInfo;
		const vk::Move<vk::VkPipelineLayout>							pipelineLayout						= createPipelineLayout(vk, device, &pipelineLayoutCreateInfo);
		PipelineCreateInfo												pipelineCreateInfo					(*pipelineLayout, *renderPass, 0, 0);

		const vk::VkVertexInputBindingDescription						vertexInputBindingDescription		=
		{
			0,
			(deUint32)sizeof(tcu::Vec4) * 2,
			vk::VK_VERTEX_INPUT_RATE_VERTEX
		};

		const vk::VkVertexInputAttributeDescription						vertexInputAttributeDescriptions[2]	=
		{
			{ 0u, 0u, vk::VK_FORMAT_R32G32B32A32_SFLOAT, 0u },
			{ 1u, 0u, vk::VK_FORMAT_R32G32B32A32_SFLOAT, (deUint32)(sizeof(float) * 4) }
		};

		std::vector<PipelineCreateInfo::ColorBlendState::Attachment>	vkCbAttachmentStates				(colorTargetImages.size());
		PipelineCreateInfo::VertexInputState							vertexInputState					= PipelineCreateInfo::VertexInputState(1,
																																				   &vertexInputBindingDescription,
																																				   2,
																																				   vertexInputAttributeDescriptions);

		pipelineCreateInfo.addShader(PipelineCreateInfo::PipelineShaderStage(*vs, "main", vk::VK_SHADER_STAGE_VERTEX_BIT));
		pipelineCreateInfo.addShader(PipelineCreateInfo::PipelineShaderStage(*fs, "main", vk::VK_SHADER_STAGE_FRAGMENT_BIT));
		pipelineCreateInfo.addState(PipelineCreateInfo::VertexInputState(vertexInputState));
		pipelineCreateInfo.addState(PipelineCreateInfo::InputAssemblerState(vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST));
		pipelineCreateInfo.addState(PipelineCreateInfo::ColorBlendState((deUint32)vkCbAttachmentStates.size(), &vkCbAttachmentStates[0]));
		pipelineCreateInfo.addState(PipelineCreateInfo::ViewportState(1, std::vector<vk::VkViewport>(1, viewport), std::vector<vk::VkRect2D>(1, scissor)));
		pipelineCreateInfo.addState(PipelineCreateInfo::DepthStencilState());
		pipelineCreateInfo.addState(PipelineCreateInfo::RasterizerState());
		pipelineCreateInfo.addState(PipelineCreateInfo::MultiSampleState(m_params.samples));

		pipeline = createGraphicsPipeline(vk, device, DE_NULL, &pipelineCreateInfo);
	}

	// Queue draw and read results.
	{
		const vk::VkQueue				queue				= m_context.getUniversalQueue();
		const vk::VkRect2D				renderArea			= vk::makeRect2D(m_params.size.x(), m_params.size.y());
		const vk::VkDeviceSize			vertexBufferOffset	= 0;
		const vk::VkBuffer				buffer				= vertexBuffer->object();
		const vk::VkOffset3D			zeroOffset			= { 0, 0, 0 };
		std::vector<vk::VkClearValue>	clearValues;

		for (deUint32 i = 0; i < colorTargetImages.size() + multisampleImages.size(); i++)
			clearValues.push_back(vk::makeClearValueColor(tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f)));

		beginCommandBuffer(vk, *cmdBuffer, 0u);
		beginRenderPass(vk, *cmdBuffer, *renderPass, *framebuffer, renderArea, (deUint32)clearValues.size(), &clearValues[0]);
		vk.cmdBindVertexBuffers(*cmdBuffer, 0, 1, &buffer, &vertexBufferOffset);
		vk.cmdBindPipeline(*cmdBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline);
		vk.cmdDraw(*cmdBuffer, 3u, 1u, 0u, 0u);
		endRenderPass(vk, *cmdBuffer);

		endCommandBuffer(vk, *cmdBuffer);
		submitCommandsAndWait(vk, device, queue, cmdBuffer.get());

		for (deUint32 frameNdx = 0; frameNdx < colorTargetImages.size(); frameNdx++)
		{
			frames[frameNdx] = colorTargetImages[frameNdx]->readSurface(queue,
																		m_context.getDefaultAllocator(),
																		vk::VK_IMAGE_LAYOUT_GENERAL,
																		zeroOffset,
																		(int)m_params.size.x(),
																		(int)m_params.size.y(),
																		vk::VK_IMAGE_ASPECT_COLOR_BIT);
		}
	}
}

bool DrawTestInstance::compare (const tcu::ConstPixelBufferAccess& result, const tcu::ConstPixelBufferAccess& reference)
{
	DE_ASSERT(result.getSize() == reference.getSize());

	const size_t	size	= result.getWidth() * result.getHeight() * vk::mapVkFormat(m_params.format).getPixelSize();
	const int		res		= deMemCmp(result.getDataPtr(), reference.getDataPtr(), size);

	return (res == 0);
}

tcu::TestStatus DrawTestInstance::iterate (void)
{
	tcu::TestLog&						log						= m_context.getTestContext().getLog();
	const bool							useMultisampling		= (m_params.samples != vk::VK_SAMPLE_COUNT_1_BIT);
	const deUint32						frameCount				= 4;
	std::vector<de::SharedPtr<Image> >	resImages				(frameCount);
	std::vector<de::SharedPtr<Image> >	smoothImage				(1);
	std::vector<de::SharedPtr<Image> >	flatImage				(1);
	std::vector<de::SharedPtr<Image> >	noperspectiveImage		(1);
	std::vector<de::SharedPtr<Image> >	centroidImage			(1);
	tcu::ConstPixelBufferAccess			resFrames[frameCount];
	tcu::ConstPixelBufferAccess			refFrames[frameCount];

	render(resImages,			resFrames,					"vert_multi",			"frag_multi");
	render(smoothImage,			&refFrames[SMOOTH],			"vert_smooth",			"frag_smooth");
	render(flatImage,			&refFrames[FLAT],			"vert_flat",			"frag_flat");
	render(noperspectiveImage,	&refFrames[NOPERSPECTIVE],	"vert_noperspective",	"frag_noperspective");
	render(centroidImage,		&refFrames[CENTROID],		"vert_centroid",		"frag_centroid");

	for (deUint32 resNdx = 0; resNdx < frameCount; resNdx++)
	{
		const std::string resName = interpolationToString((Interpolation)resNdx);

		log	<< tcu::TestLog::ImageSet(resName, resName)
			<< tcu::TestLog::Image("Result", "Result", resFrames[resNdx])
			<< tcu::TestLog::Image("Reference", "Reference", refFrames[resNdx])
			<< tcu::TestLog::EndImageSet;

		for (deUint32 refNdx = 0; refNdx < frameCount; refNdx++)
		{
			const std::string refName = interpolationToString((Interpolation)refNdx);

			if (resNdx == refNdx)
			{
				if (!compare(resFrames[resNdx], refFrames[refNdx]))
					return tcu::TestStatus::fail(resName + " produced different results");
			}
			else if (!useMultisampling && ((resNdx == SMOOTH && refNdx == CENTROID) || (resNdx == CENTROID && refNdx == SMOOTH)))
			{
				if (!compare(resFrames[resNdx], refFrames[refNdx]))
					return tcu::TestStatus::fail(resName + " and " + refName + " produced different results without multisampling");
			}
			else
			{
				if (compare(resFrames[resNdx], refFrames[refNdx]))
					return tcu::TestStatus::fail(resName + " and " + refName + " produced same result");
			}
		}
	}

	return tcu::TestStatus::pass("Results differ and references match");
}

void createTests (tcu::TestCaseGroup* testGroup)
{
	tcu::TestContext&	testCtx	= testGroup->getTestContext();
	const vk::VkFormat	format	= vk::VK_FORMAT_R8G8B8A8_UNORM;
	const tcu::UVec2	size	(128, 128);

	const struct
	{
		const std::string	name;
		const std::string	desc;
		const DrawParams	params;
	} tests[] =
	{
		{ "1_sample",	"Without multisampling",	{ format, size, vk::VK_SAMPLE_COUNT_1_BIT	} },
		{ "2_samples",	"2 samples",				{ format, size, vk::VK_SAMPLE_COUNT_2_BIT	} },
		{ "4_samples",	"4 samples",				{ format, size, vk::VK_SAMPLE_COUNT_4_BIT	} },
		{ "8_samples",	"8 samples",				{ format, size, vk::VK_SAMPLE_COUNT_8_BIT	} },
		{ "16_samples",	"16 samples",				{ format, size, vk::VK_SAMPLE_COUNT_16_BIT	} },
		{ "32_samples",	"32 samples",				{ format, size, vk::VK_SAMPLE_COUNT_32_BIT	} },
		{ "64_samples",	"64 samples",				{ format, size, vk::VK_SAMPLE_COUNT_64_BIT	} },
	};

	for (int i = 0; i < DE_LENGTH_OF_ARRAY(tests); i++)
		testGroup->addChild(new DrawTestCase(testCtx, tests[i].name, tests[i].desc, tests[i].params));
}

}	// anonymous

tcu::TestCaseGroup* createMultipleInterpolationTests (tcu::TestContext& testCtx)
{
	return createTestGroup(testCtx,
						   "multiple_interpolation",
						   "Tests for multiple interpolation decorations in a shader stage.",
						   createTests);
}

}	// Draw
}	// vkt
