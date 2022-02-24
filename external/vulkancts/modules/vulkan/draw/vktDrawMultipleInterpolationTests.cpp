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
#include "tcuVectorUtil.hpp"

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
	CENTROID		= 3,
	SAMPLE			= 4,
	COUNT			= 5,
};

struct DrawParams
{
	vk::VkFormat				format;
	tcu::UVec2					size;
	vk::VkSampleCountFlagBits	samples;
	// From the SPIR-V point of view, structured test variants will allow us to test interpolation decorations on struct members
	// instead of plain ids.
	bool						useStructure;
	bool						includeSampleDecoration;
	bool						useDynamicRendering;
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
		case SAMPLE:
			return "sample";
		default:
			DE_FATAL("Invalid interpolation enum");
	}

	return "";
}

class DrawTestInstance : public TestInstance
{
public:
					DrawTestInstance	(Context& context, DrawParams params);
	void			render				(de::SharedPtr<Image>& colorTargetImage,
										 tcu::ConstPixelBufferAccess* frame,
										 const char* vsName,
										 const char* fsName,
										 Interpolation interpolation,
										 bool sampleRateShading);
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
	const std::string							blockName		= "ifb";
	const std::map<std::string, std::string>	replacements	=
	{
		std::pair<std::string, std::string>{"blockOpeningOut"	, (m_params.useStructure ? "layout(location = 0) out InterfaceBlock {\n" : "")},
		std::pair<std::string, std::string>{"blockOpeningIn"	, (m_params.useStructure ? "layout(location = 0) in InterfaceBlock {\n" : "")},
		std::pair<std::string, std::string>{"blockClosure"		, (m_params.useStructure ? "} " + blockName + ";\n" : "")},
		std::pair<std::string, std::string>{"extensions"		, (m_params.useStructure ? "#extension GL_ARB_enhanced_layouts : require\n" : "")},
		std::pair<std::string, std::string>{"accessPrefix"		, (m_params.useStructure ? blockName + "." : "")},
		std::pair<std::string, std::string>{"outQual"			, (m_params.useStructure ? "" : "out ")},
		std::pair<std::string, std::string>{"inQual"			, (m_params.useStructure ? "" : "in ")},
		std::pair<std::string, std::string>{"indent"			, (m_params.useStructure ? "    " : "")},
	};

	std::ostringstream vertShaderMultiStream;
	vertShaderMultiStream
		<< "#version 430\n"
		<< "${extensions}"
		<< "\n"
		<< "layout(location = 0) in vec4 in_position;\n"
		<< "layout(location = 1) in vec4 in_color;\n"
		<< "\n"
		<< "${blockOpeningOut}"
		<< "${indent}layout(location = 0) ${outQual}vec4 out_color_smooth;\n"
		<< "${indent}layout(location = 1) ${outQual}flat vec4 out_color_flat;\n"
		<< "${indent}layout(location = 2) ${outQual}noperspective vec4 out_color_noperspective;\n"
		<< "${indent}layout(location = 3) ${outQual}centroid vec4 out_color_centroid;\n"
		<< (m_params.includeSampleDecoration ? "${indent}layout(location = 4) ${outQual}sample vec4 out_color_sample;\n" : "")
		<< "${blockClosure}"
		<< "\n"
		<< "void main()\n"
		<< "{\n"
		<< "    ${accessPrefix}out_color_smooth = in_color;\n"
		<< "    ${accessPrefix}out_color_flat = in_color;\n"
		<< "    ${accessPrefix}out_color_noperspective = in_color;\n"
		<< "    ${accessPrefix}out_color_centroid = in_color;\n"
		<< (m_params.includeSampleDecoration ? "    ${accessPrefix}out_color_sample = in_color;\n" : "")
		<< "    gl_Position = in_position;\n"
		<< "}\n"
		;
	const tcu::StringTemplate vertShaderMulti(vertShaderMultiStream.str());

	const auto colorCount = (m_params.includeSampleDecoration ? COUNT : (COUNT - 1));

	std::ostringstream fragShaderMultiStream;
	fragShaderMultiStream
		<< "#version 430\n"
		<< "${extensions}"
		<< "\n"
		<< "${blockOpeningIn}"
		<< "${indent}layout(location = 0) ${inQual}vec4 in_color_smooth;\n"
		<< "${indent}layout(location = 1) ${inQual}flat vec4 in_color_flat;\n"
		<< "${indent}layout(location = 2) ${inQual}noperspective vec4 in_color_noperspective;\n"
		<< "${indent}layout(location = 3) ${inQual}centroid vec4 in_color_centroid;\n"
		<< (m_params.includeSampleDecoration ? "${indent}layout(location = 4) ${inQual}sample vec4 in_color_sample;\n" : "")
		<< "${blockClosure}"
		<< "\n"
		<< "layout(push_constant, std430) uniform PushConstants {\n"
		<< "    uint interpolationIndex;\n"
		<< "} pc;\n"
		<< "\n"
		<< "layout(location=0) out vec4 out_color;\n"
		<< "\n"
		<< "void main()\n"
		<< "{\n"
		<< "    const vec4 in_colors[" + de::toString(colorCount) + "] = vec4[](\n"
		<< "        ${accessPrefix}in_color_smooth,\n"
		<< "        ${accessPrefix}in_color_flat,\n"
		<< "        ${accessPrefix}in_color_noperspective,\n"
		<< "        ${accessPrefix}in_color_centroid" << (m_params.includeSampleDecoration ? "," : "") << "\n"
		<< (m_params.includeSampleDecoration ? "        ${accessPrefix}in_color_sample\n" : "")
		<< "    );\n"
		<< "    out_color = in_colors[pc.interpolationIndex];\n"
		<< "}\n"
		;
	const tcu::StringTemplate fragShaderMulti(fragShaderMultiStream.str());

	const tcu::StringTemplate vertShaderSingle
	{
		"#version 430\n"
		"${extensions}"
		"\n"
		"layout(location = 0) in vec4 in_position;\n"
		"layout(location = 1) in vec4 in_color;\n"
		"\n"
		"${blockOpeningOut}"
		"${indent}layout(location = 0) ${outQual}${qualifier:opt}vec4 out_color;\n"
		"${blockClosure}"
		"\n"
		"void main()\n"
		"{\n"
		"    ${accessPrefix}out_color = in_color;\n"
		"    gl_Position = in_position;\n"
		"}\n"
	};

	const tcu::StringTemplate fragShaderSingle
	{
		"#version 430\n"
		"${extensions}"
		"\n"
		"${blockOpeningIn}"
		"${indent}layout(location = 0) ${inQual}${qualifier:opt}vec4 in_color;\n"
		"${blockClosure}"
		"\n"
		"layout(location = 0) out vec4 out_color;\n"
		"\n"
		"void main()\n"
		"{\n"
		"    out_color = ${accessPrefix}in_color;\n"
		"}\n"
	};

	std::map<std::string, std::string>	smooth			= replacements;
	std::map<std::string, std::string>	flat			= replacements;
	std::map<std::string, std::string>	noperspective	= replacements;
	std::map<std::string, std::string>	centroid		= replacements;
	std::map<std::string, std::string>	sample			= replacements;

	flat["qualifier"]			= "flat ";
	noperspective["qualifier"]	= "noperspective ";
	centroid["qualifier"]		= "centroid ";
	sample["qualifier"]			= "sample ";

	programCollection.glslSources.add("vert_multi")			<< glu::VertexSource(vertShaderMulti.specialize(replacements));
	programCollection.glslSources.add("frag_multi")			<< glu::FragmentSource(fragShaderMulti.specialize(replacements));
	programCollection.glslSources.add("vert_smooth")		<< glu::VertexSource(vertShaderSingle.specialize(smooth));
	programCollection.glslSources.add("frag_smooth")		<< glu::FragmentSource(fragShaderSingle.specialize(smooth));
	programCollection.glslSources.add("vert_flat")			<< glu::VertexSource(vertShaderSingle.specialize(flat));
	programCollection.glslSources.add("frag_flat")			<< glu::FragmentSource(fragShaderSingle.specialize(flat));
	programCollection.glslSources.add("vert_noperspective")	<< glu::VertexSource(vertShaderSingle.specialize(noperspective));
	programCollection.glslSources.add("frag_noperspective")	<< glu::FragmentSource(fragShaderSingle.specialize(noperspective));
	programCollection.glslSources.add("vert_centroid")		<< glu::VertexSource(vertShaderSingle.specialize(centroid));
	programCollection.glslSources.add("frag_centroid")		<< glu::FragmentSource(fragShaderSingle.specialize(centroid));

	if (m_params.includeSampleDecoration)
	{
		programCollection.glslSources.add("vert_sample")		<< glu::VertexSource(vertShaderSingle.specialize(sample));
		programCollection.glslSources.add("frag_sample")		<< glu::FragmentSource(fragShaderSingle.specialize(sample));
	}
}

void DrawTestCase::checkSupport (Context& context) const
{
	if (!(m_params.samples & context.getDeviceProperties().limits.framebufferColorSampleCounts))
		TCU_THROW(NotSupportedError, "Multisampling with " + de::toString(m_params.samples) + " samples not supported");

	if (m_params.includeSampleDecoration && !context.getDeviceFeatures().sampleRateShading)
		TCU_THROW(NotSupportedError, "Sample rate shading not supported");

	if (m_params.useDynamicRendering)
		context.requireDeviceFunctionality("VK_KHR_dynamic_rendering");
}

TestInstance* DrawTestCase::createInstance (Context& context) const
{
	return new DrawTestInstance(context, m_params);
}

void DrawTestInstance::render (de::SharedPtr<Image>& colorTargetImage,
							   tcu::ConstPixelBufferAccess* frame,
							   const char* vsName,
							   const char* fsName,
							   Interpolation interpolation,
							   bool sampleRateShading)
{
	const deUint32											pcData				= static_cast<deUint32>(interpolation);
	const deUint32											pcDataSize			= static_cast<deUint32>(sizeof(pcData));
	const bool												useMultisampling	= (m_params.samples != vk::VK_SAMPLE_COUNT_1_BIT);
	const vk::VkBool32										sampleShadingEnable	= (sampleRateShading ? VK_TRUE : VK_FALSE);
	const vk::DeviceInterface&								vk					= m_context.getDeviceInterface();
	const vk::VkDevice										device				= m_context.getDevice();
	const vk::Unique<vk::VkShaderModule>					vs					(createShaderModule(vk, device, m_context.getBinaryCollection().get(vsName), 0));
	const vk::Unique<vk::VkShaderModule>					fs					(createShaderModule(vk, device, m_context.getBinaryCollection().get(fsName), 0));
	const CmdPoolCreateInfo									cmdPoolCreateInfo	= m_context.getUniversalQueueFamilyIndex();
	vk::Move<vk::VkCommandPool>								cmdPool				= createCommandPool(vk, device, &cmdPoolCreateInfo);
	vk::Move<vk::VkCommandBuffer>							cmdBuffer			= vk::allocateCommandBuffer(vk, device, *cmdPool, vk::VK_COMMAND_BUFFER_LEVEL_PRIMARY);
	de::SharedPtr<Image>									multisampleImage;
	std::vector<de::SharedPtr<vk::Move<vk::VkImageView> > >	colorTargetViews;
	std::vector<de::SharedPtr<vk::Move<vk::VkImageView> > >	multisampleViews;
	de::SharedPtr<Buffer>									vertexBuffer;
	vk::Move<vk::VkRenderPass>								renderPass;
	vk::Move<vk::VkFramebuffer>								framebuffer;
	vk::Move<vk::VkPipelineLayout>							pipelineLayout;
	vk::Move<vk::VkPipeline>								pipeline;

	// Create color buffer images
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

		colorTargetImage = Image::createAndAlloc(vk, device, targetImageCreateInfo,
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

			multisampleImage = Image::createAndAlloc(vk, device, multisampleImageCreateInfo,
													 m_context.getDefaultAllocator(),
													 m_context.getUniversalQueueFamilyIndex());
		}
	}

	{
		const ImageViewCreateInfo colorTargetViewInfo(colorTargetImage->object(),
													  vk::VK_IMAGE_VIEW_TYPE_2D,
													  m_params.format);

		colorTargetViews.push_back(makeSharedPtr(createImageView(vk, device, &colorTargetViewInfo)));

		if (useMultisampling)
		{
			const ImageViewCreateInfo multisamplingTargetViewInfo(multisampleImage->object(),
																  vk::VK_IMAGE_VIEW_TYPE_2D,
																  m_params.format);

			multisampleViews.push_back(makeSharedPtr(createImageView(vk, device, &multisamplingTargetViewInfo)));
		}
	}

	// Create render pass and framebuffer
	if (!m_params.useDynamicRendering)
	{
		RenderPassCreateInfo					renderPassCreateInfo;
		std::vector<vk::VkImageView>			attachments;
		std::vector<vk::VkAttachmentReference>	colorAttachmentRefs;
		std::vector<vk::VkAttachmentReference>	multisampleAttachmentRefs;
		deUint32								attachmentNdx				= 0;

		{
			const vk::VkAttachmentReference	colorAttachmentReference	=
			{
				attachmentNdx++,
				vk::VK_IMAGE_LAYOUT_GENERAL
			};

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
				const vk::VkAttachmentReference	multiSampleAttachmentReference	=
				{
					attachmentNdx++,
					vk::VK_IMAGE_LAYOUT_GENERAL
				};

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
		const auto														pcRange								= vk::makePushConstantRange(vk::VK_SHADER_STAGE_FRAGMENT_BIT, 0u, pcDataSize);
		const std::vector<vk::VkPushConstantRange>						pcRanges							(1u, pcRange);
		const PipelineLayoutCreateInfo									pipelineLayoutCreateInfo			(0u, nullptr, static_cast<deUint32>(pcRanges.size()), pcRanges.data());

		pipelineLayout = createPipelineLayout(vk, device, &pipelineLayoutCreateInfo);

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

		std::vector<PipelineCreateInfo::ColorBlendState::Attachment>	vkCbAttachmentStates				(1u);
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
		pipelineCreateInfo.addState(PipelineCreateInfo::MultiSampleState(m_params.samples, sampleShadingEnable, 1.0f));

		std::vector<vk::VkFormat> colorAttachmentFormats(colorTargetViews.size(), m_params.format);
		vk::VkPipelineRenderingCreateInfoKHR renderingCreateInfo
		{
			vk::VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR,
			DE_NULL,
			0u,
			static_cast<deUint32>(colorAttachmentFormats.size()),
			colorAttachmentFormats.data(),
			vk::VK_FORMAT_UNDEFINED,
			vk::VK_FORMAT_UNDEFINED
		};

		if (m_params.useDynamicRendering)
			pipelineCreateInfo.pNext = &renderingCreateInfo;

		pipeline = createGraphicsPipeline(vk, device, DE_NULL, &pipelineCreateInfo);
	}

	// Queue draw and read results.
	{
		const vk::VkQueue				queue				= m_context.getUniversalQueue();
		const vk::VkRect2D				renderArea			= vk::makeRect2D(m_params.size.x(), m_params.size.y());
		const vk::VkDeviceSize			vertexBufferOffset	= 0;
		const vk::VkBuffer				buffer				= vertexBuffer->object();
		const vk::VkOffset3D			zeroOffset			= { 0, 0, 0 };
		const auto						clearValueColor		= vk::makeClearValueColor(tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f));
		std::vector<vk::VkClearValue>	clearValues			(2, clearValueColor);

		beginCommandBuffer(vk, *cmdBuffer, 0u);

		if (m_params.useDynamicRendering)
		{
			const deUint32 imagesCount = static_cast<deUint32>(colorTargetViews.size());
			std::vector<vk::VkRenderingAttachmentInfoKHR> colorAttachments(imagesCount,
			{
				vk::VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR,	// VkStructureType						sType;
				DE_NULL,												// const void*							pNext;
				DE_NULL,												// VkImageView							imageView;
				vk::VK_IMAGE_LAYOUT_GENERAL,							// VkImageLayout						imageLayout;
				vk::VK_RESOLVE_MODE_NONE,								// VkResolveModeFlagBits				resolveMode;
				DE_NULL,												// VkImageView							resolveImageView;
				vk::VK_IMAGE_LAYOUT_GENERAL,							// VkImageLayout						resolveImageLayout;
				vk::VK_ATTACHMENT_LOAD_OP_CLEAR,						// VkAttachmentLoadOp					loadOp;
				vk::VK_ATTACHMENT_STORE_OP_STORE,						// VkAttachmentStoreOp					storeOp;
				clearValueColor											// VkClearValue							clearValue;
			});

			for (deUint32 i = 0; i < imagesCount; ++i)
			{
				if (useMultisampling)
				{
					colorAttachments[i].imageView			= **multisampleViews[i];
					colorAttachments[i].resolveMode			= vk::VK_RESOLVE_MODE_AVERAGE_BIT;
					colorAttachments[i].resolveImageView	= **colorTargetViews[i];
				}
				else
					colorAttachments[i].imageView = **colorTargetViews[i];
			}

			vk::VkRenderingInfoKHR renderingInfo
			{
				vk::VK_STRUCTURE_TYPE_RENDERING_INFO_KHR,
				DE_NULL,
				0,														// VkRenderingFlagsKHR					flags;
				renderArea,												// VkRect2D								renderArea;
				1u,														// deUint32								layerCount;
				0u,														// deUint32								viewMask;
				imagesCount,											// deUint32								colorAttachmentCount;
				colorAttachments.data(),								// const VkRenderingAttachmentInfoKHR*	pColorAttachments;
				DE_NULL,												// const VkRenderingAttachmentInfoKHR*	pDepthAttachment;
				DE_NULL,												// const VkRenderingAttachmentInfoKHR*	pStencilAttachment;
			};

			// Transition Images
			initialTransitionColor2DImage(vk, *cmdBuffer, colorTargetImage->object(), vk::VK_IMAGE_LAYOUT_GENERAL,
				vk::VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, vk::VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

			if (useMultisampling)
			{
				initialTransitionColor2DImage(vk, *cmdBuffer, multisampleImage->object(), vk::VK_IMAGE_LAYOUT_GENERAL,
					vk::VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, vk::VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
			}

			vk.cmdBeginRendering(*cmdBuffer, &renderingInfo);
		}
		else
		{
			const deUint32 imagesCount = static_cast<deUint32>(colorTargetViews.size() + multisampleViews.size());
			beginRenderPass(vk, *cmdBuffer, *renderPass, *framebuffer, renderArea, imagesCount, &clearValues[0]);
		}

		vk.cmdBindVertexBuffers(*cmdBuffer, 0, 1, &buffer, &vertexBufferOffset);
		vk.cmdBindPipeline(*cmdBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline);
		vk.cmdPushConstants(*cmdBuffer, *pipelineLayout, vk::VK_SHADER_STAGE_FRAGMENT_BIT, 0u, pcDataSize, &pcData);
		vk.cmdDraw(*cmdBuffer, 3u, 1u, 0u, 0u);

		if (m_params.useDynamicRendering)
			endRendering(vk, *cmdBuffer);
		else
			endRenderPass(vk, *cmdBuffer);

		endCommandBuffer(vk, *cmdBuffer);
		submitCommandsAndWait(vk, device, queue, cmdBuffer.get());

		*frame = colorTargetImage->readSurface(queue,
											   m_context.getDefaultAllocator(),
											   vk::VK_IMAGE_LAYOUT_GENERAL,
											   zeroOffset,
											   (int)m_params.size.x(),
											   (int)m_params.size.y(),
											   vk::VK_IMAGE_ASPECT_COLOR_BIT);
	}
}

bool DrawTestInstance::compare (const tcu::ConstPixelBufferAccess& result, const tcu::ConstPixelBufferAccess& reference)
{
	DE_ASSERT(result.getSize() == reference.getSize());

	const tcu::IVec4	threshold	(1u, 1u, 1u, 1u);

	for (int y = 0; y < result.getHeight(); y++)
	{
		for (int x = 0; x < result.getWidth(); x++)
		{
			tcu::IVec4	refPix	= reference.getPixelInt(x, y);
			tcu::IVec4	cmpPix	= result.getPixelInt(x, y);
			tcu::IVec4	diff	= tcu::abs(refPix - cmpPix);

			if (!tcu::boolAll(tcu::lessThanEqual(diff, threshold)))
				return false;
		}
	}

	return true;
}

tcu::TestStatus DrawTestInstance::iterate (void)
{
	tcu::TestLog&						log						= m_context.getTestContext().getLog();
	const bool							useMultisampling		= (m_params.samples != vk::VK_SAMPLE_COUNT_1_BIT);
	const deUint32						frameCount				= static_cast<deUint32>(COUNT);
	std::vector<de::SharedPtr<Image> >	resImages				(frameCount);
	de::SharedPtr<Image>				smoothImage[2];
	de::SharedPtr<Image>				flatImage[2];
	de::SharedPtr<Image>				noperspectiveImage[2];
	de::SharedPtr<Image>				centroidImage[2];
	de::SharedPtr<Image>				sampleImage[2];
	tcu::ConstPixelBufferAccess			resFrames[frameCount];
	tcu::ConstPixelBufferAccess			refFrames[frameCount];
	tcu::ConstPixelBufferAccess			refSRSFrames[frameCount]; // Using sample rate shading.

	for (int interpolationType = 0; interpolationType < COUNT; ++interpolationType)
	{
		// Avoid generating a result image for the sample decoration if we're not using it.
		if (!m_params.includeSampleDecoration && interpolationType == Interpolation::SAMPLE)
			continue;

		render(resImages[interpolationType], &resFrames[interpolationType], "vert_multi", "frag_multi", static_cast<Interpolation>(interpolationType), false);
	}

	for (int i = 0; i < 2; ++i)
	{
		const bool useSampleRateShading = (i > 0);

		// Sample rate shading is an alternative good result for cases using the sample decoration.
		if (useSampleRateShading && !m_params.includeSampleDecoration)
			continue;

		tcu::ConstPixelBufferAccess *framesArray = (useSampleRateShading ? refSRSFrames : refFrames);

		render(smoothImage[i],			&framesArray[SMOOTH],			"vert_smooth",			"frag_smooth",			SMOOTH,			useSampleRateShading);
		render(flatImage[i],			&framesArray[FLAT],				"vert_flat",			"frag_flat",			FLAT,			useSampleRateShading);
		render(noperspectiveImage[i],	&framesArray[NOPERSPECTIVE],	"vert_noperspective",	"frag_noperspective",	NOPERSPECTIVE,	useSampleRateShading);
		render(centroidImage[i],		&framesArray[CENTROID],			"vert_centroid",		"frag_centroid",		CENTROID,		useSampleRateShading);

		// Avoid generating a reference image for the sample interpolation if we're not using it.
		if (m_params.includeSampleDecoration)
			render(sampleImage[i],		&framesArray[SAMPLE],			"vert_sample",			"frag_sample",			SAMPLE,			useSampleRateShading);
	}

	for (deUint32 resNdx = 0; resNdx < frameCount; resNdx++)
	{
		if (!m_params.includeSampleDecoration && resNdx == SAMPLE)
			continue;

		const std::string resName = interpolationToString((Interpolation)resNdx);

		log	<< tcu::TestLog::ImageSet(resName, resName)
			<< tcu::TestLog::Image("Result", "Result", resFrames[resNdx])
			<< tcu::TestLog::Image("Reference", "Reference", refFrames[resNdx]);
		if (m_params.includeSampleDecoration)
			log << tcu::TestLog::Image("ReferenceSRS", "Reference with sample shading", refSRSFrames[resNdx]);
		log	<< tcu::TestLog::EndImageSet;

		for (deUint32 refNdx = 0; refNdx < frameCount; refNdx++)
		{
			if (!m_params.includeSampleDecoration && refNdx == SAMPLE)
				continue;

			const std::string refName = interpolationToString((Interpolation)refNdx);

			if (resNdx == refNdx)
			{
				if (!compare(resFrames[resNdx], refFrames[refNdx]) && (!m_params.includeSampleDecoration || !compare(resFrames[resNdx], refSRSFrames[refNdx])))
					return tcu::TestStatus::fail(resName + " produced different results");
			}
			else if (!useMultisampling &&
				((resNdx == SMOOTH && refNdx == CENTROID) ||
				 (resNdx == CENTROID && refNdx == SMOOTH) ||
				 (resNdx == SMOOTH && refNdx == SAMPLE)   ||
				 (resNdx == SAMPLE && refNdx == SMOOTH)   ||
				 (resNdx == CENTROID && refNdx == SAMPLE) ||
				 (resNdx == SAMPLE && refNdx == CENTROID)))
			{
				if (!compare(resFrames[resNdx], refFrames[refNdx]))
					return tcu::TestStatus::fail(resName + " and " + refName + " produced different results without multisampling");
			}
			else
			{
				// "smooth" means lack of centroid and sample.
				// Spec does not specify exactly what "smooth" should be, so it can match centroid or sample.
				// "centroid" and "sample" may also produce the same results.
				if (!((resNdx == SMOOTH && refNdx == CENTROID)   ||
					  (resNdx == CENTROID && refNdx == SMOOTH)   ||
					  (resNdx == SMOOTH && refNdx == SAMPLE)     ||
					  (resNdx == SAMPLE && refNdx == SMOOTH)     ||
					  (resNdx == CENTROID && refNdx == SAMPLE)   ||
					  (resNdx == SAMPLE && refNdx == CENTROID)   ))
				{
					if (compare(resFrames[resNdx], refFrames[refNdx]))
						return tcu::TestStatus::fail(resName + " and " + refName + " produced same result");
				}
			}
		}
	}

	return tcu::TestStatus::pass("Results differ and references match");
}

void createTests (tcu::TestCaseGroup* testGroup, bool useDynamicRendering)
{
	tcu::TestContext&	testCtx	= testGroup->getTestContext();
	const vk::VkFormat	format	= vk::VK_FORMAT_R8G8B8A8_UNORM;
	const tcu::UVec2	size	(128, 128);

	struct TestVariant
	{
		const std::string				name;
		const std::string				desc;
		const vk::VkSampleCountFlagBits	samples;
	};

	static const std::vector<TestVariant> testVariants =
	{
		{ "1_sample",	"Without multisampling",	vk::VK_SAMPLE_COUNT_1_BIT	},
		{ "2_samples",	"2 samples",				vk::VK_SAMPLE_COUNT_2_BIT	},
		{ "4_samples",	"4 samples",				vk::VK_SAMPLE_COUNT_4_BIT	},
		{ "8_samples",	"8 samples",				vk::VK_SAMPLE_COUNT_8_BIT	},
		{ "16_samples",	"16 samples",				vk::VK_SAMPLE_COUNT_16_BIT	},
		{ "32_samples",	"32 samples",				vk::VK_SAMPLE_COUNT_32_BIT	},
		{ "64_samples",	"64 samples",				vk::VK_SAMPLE_COUNT_64_BIT	},
	};

	struct GroupVariant
	{
		const bool			useStructure;
		const std::string	groupName;
	};

	static const std::vector<GroupVariant> groupVariants =
	{
		{ false,	"separate"		},
		{ true,		"structured"	},
	};

	const struct
	{
		const bool			includeSampleDecoration;
		const std::string	groupName;
	} sampleVariants[] =
	{
		{ false,	"no_sample_decoration"		},
		{ true,		"with_sample_decoration"	},
	};

	for (const auto& grpVariant : groupVariants)
	{
		de::MovePtr<tcu::TestCaseGroup> group {new tcu::TestCaseGroup{testCtx, grpVariant.groupName.c_str(), ""}};

		for (const auto& sampleVariant : sampleVariants)
		{
			de::MovePtr<tcu::TestCaseGroup> sampleGroup {new tcu::TestCaseGroup{testCtx, sampleVariant.groupName.c_str(), ""}};

			for (const auto& testVariant : testVariants)
			{
				const DrawParams params {format, size, testVariant.samples, grpVariant.useStructure, sampleVariant.includeSampleDecoration, useDynamicRendering};
				sampleGroup->addChild(new DrawTestCase(testCtx, testVariant.name, testVariant.desc, params));
			}

			group->addChild(sampleGroup.release());
		}

		testGroup->addChild(group.release());
	}
}

}	// anonymous

tcu::TestCaseGroup* createMultipleInterpolationTests (tcu::TestContext& testCtx, bool useDynamicRendering)
{
	return createTestGroup(testCtx,
						   "multiple_interpolation",
						   "Tests for multiple interpolation decorations in a shader stage.",
						   createTests,
						   useDynamicRendering);
}

}	// Draw
}	// vkt
