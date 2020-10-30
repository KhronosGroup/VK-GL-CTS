/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2016 The Khronos Group Inc.
 * Copyright (c) 2017 Google Inc.
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
 * \brief Depth clamp tests.
 *//*--------------------------------------------------------------------*/

#include "vkDefs.hpp"
#include "vktDrawDepthClampTests.hpp"
#include "vktTestGroupUtil.hpp"
#include "vktTestCaseUtil.hpp"
#include "vktDrawCreateInfoUtil.hpp"
#include "vktDrawBufferObjectUtil.hpp"
#include "vktDrawImageObjectUtil.hpp"
#include "vkPrograms.hpp"
#include "vkTypeUtil.hpp"
#include "vkRefUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkQueryUtil.hpp"
#include "tcuTextureUtil.hpp"

#include <cmath>

namespace vkt
{
namespace Draw
{
namespace {
using namespace vk;
using namespace de;
using std::string;
using tcu::Vec4;

static const int					WIDTH				= 256;
static const int					HEIGHT				= 256;

struct TestParams
{
	string							testNameSuffix;
	float							depthValue;
	float							expectedValue;
	bool							enableDepthBias;
	float							depthBiasConstantFactor;
	bool							skipUNorm;
	bool							skipSNorm;
	std::vector<const char*>		requiredExtensions;
};

const VkFormat		depthStencilImageFormatsToTest[]	=
{
	VK_FORMAT_D16_UNORM,
	VK_FORMAT_X8_D24_UNORM_PACK32,
	VK_FORMAT_D32_SFLOAT,
	VK_FORMAT_D16_UNORM_S8_UINT,
	VK_FORMAT_D24_UNORM_S8_UINT,
	VK_FORMAT_D32_SFLOAT_S8_UINT
};
const float			depthEpsilonValuesByFormat[]		=
{
    1e-5f,
	std::numeric_limits<float>::epsilon(),
	std::numeric_limits<float>::epsilon(),
	1e-5f,
	std::numeric_limits<float>::epsilon(),
	std::numeric_limits<float>::epsilon()
};

const float			initialClearDepth				    = 0.5f;
const TestParams	depthClearValuesToTest[]			=
{
	{
		"",											// testNameSuffix
		0.3f,										// depthValue
		0.3f,										// expectedValue
		false,										// enableDepthBias
		0.0f,										// depthBiasConstantFactor
		false,										// skipUNorm
		false,										// skipSNorm
		{},											// requiredExtensions
	},
	{
		"_clamp_input_negative",					// testNameSuffix
		-1e6f,										// depthValue
		0.0f,										// expectedValue
		false,										// enableDepthBias
		0.0f,										// depthBiasConstantFactor
		false,										// skipUNorm
		false,										// skipSNorm
		{},											// requiredExtensions
	},
	{
		"_clamp_input_positive",					// testNameSuffix
		1.e6f,										// depthValue
		1.0f,										// expectedValue
		false,										// enableDepthBias
		0.0f,										// depthBiasConstantFactor
		false,										// skipUNorm
		false,										// skipSNorm
		{},											// requiredExtensions
	},
	{
		"_depth_bias_clamp_input_negative",			// testNameSuffix
		0.3f,										// depthValue
		0.0f,										// expectedValue
		true,										// enableDepthBias
		-2e11f,										// depthBiasConstantFactor
		false,										// skipUNorm
		false,										// skipSNorm
		{},											// requiredExtensions
	},
	{
		"_depth_bias_clamp_input_positive",			// testNameSuffix
		0.7f,										// depthValue
		1.0f,										// expectedValue
		true,										// enableDepthBias
		2e11f,										// depthBiasConstantFactor
		false,										// skipUNorm
		false,										// skipSNorm
		{},											// requiredExtensions
	},
	{
		"_depth_range_unrestricted_negative",		// testNameSuffix
		-1.5f,										// depthValue
		-1.5f,										// expectedValue
		false,										// enableDepthBias
		0.0f,										// depthBiasConstantFactor
		true,										// skipUNorm
		true,										// skipSNorm
		{
			"VK_EXT_depth_range_unrestricted"		// requiredExtensions[0]
		},
	},
	{
		"_depth_range_unrestricted_positive",		// testNameSuffix
		1.5f,										// depthValue
		1.5f,										// expectedValue
		false,										// enableDepthBias
		0.0f,										// depthBiasConstantFactor
		true,										// skipUNorm
		true,										// skipSNorm
		{
			"VK_EXT_depth_range_unrestricted"		// requiredExtensions[0]
		},
	}
};

bool isUnormDepthFormat(VkFormat format)
{
	switch (format)
	{
		case VK_FORMAT_D24_UNORM_S8_UINT:
		case VK_FORMAT_X8_D24_UNORM_PACK32:
		case VK_FORMAT_D16_UNORM_S8_UINT:
			/* Special case for combined depth-stencil-unorm modes for which tcu::getTextureChannelClass()
			   returns TEXTURECHANNELCLASS_LAST */
			return true;
		default:
			return vk::isUnormFormat(format);
	}
}

class DepthClampTestInstance : public TestInstance {
public:
								DepthClampTestInstance	(Context& context, const TestParams& params, const VkFormat format, const float epsilon);
	tcu::TestStatus				iterate					();

private:
	tcu::ConstPixelBufferAccess draw					(const VkViewport viewport);

	const TestParams									m_params;
	const VkFormat										m_format;
	const float											m_epsilon;
	SharedPtr<Image>									m_depthTargetImage;
    Move<VkImageView>									m_depthTargetView;
	SharedPtr<Buffer>									m_vertexBuffer;
	Move<VkRenderPass>									m_renderPass;
	Move<VkFramebuffer>									m_framebuffer;
	Move<VkPipelineLayout>								m_pipelineLayout;
	Move<VkPipeline>									m_pipeline;
};

static const Vec4					vertices[]			= {
	Vec4(-1.0f, -1.0f,  0.5f, 1.0f),	// 0 -- 2
	Vec4(-1.0f,  1.0f,  0.5f, 1.0f),	// |  / |
	Vec4( 1.0f, -1.0f,  0.5f, 1.0f),	// | /  |
	Vec4( 1.0f,  1.0f,  0.5f, 1.0f)		// 1 -- 3
};
static const VkPrimitiveTopology    verticesTopology	= VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;

DepthClampTestInstance::DepthClampTestInstance (Context& context, const TestParams& params, const VkFormat format, const float epsilon)
	: TestInstance(context)
	, m_params(params)
	, m_format(format)
	, m_epsilon(epsilon)
{
	const DeviceInterface&		vk								= m_context.getDeviceInterface();
	const VkDevice				device							= m_context.getDevice();
	const deUint32			    queueFamilyIndex				= m_context.getUniversalQueueFamilyIndex();

	DescriptorPoolBuilder		descriptorPoolBuilder;
	DescriptorSetLayoutBuilder	descriptorSetLayoutBuilder;
	// Vertex data
	{
		const size_t			verticesCount					= DE_LENGTH_OF_ARRAY(vertices);
		const VkDeviceSize		dataSize					    = verticesCount * sizeof(Vec4);
		m_vertexBuffer											= Buffer::createAndAlloc(vk, device, BufferCreateInfo(dataSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT),
																m_context.getDefaultAllocator(), MemoryRequirement::HostVisible);

		Vec4 testVertices[verticesCount];
		deMemcpy(testVertices, vertices, dataSize);
		for(size_t i = 0; i < verticesCount; ++i)
			testVertices[i][2] = params.depthValue;
		deMemcpy(m_vertexBuffer->getBoundMemory().getHostPtr(), testVertices, static_cast<std::size_t>(dataSize));
		flushMappedMemoryRange(vk, device, m_vertexBuffer->getBoundMemory().getMemory(), m_vertexBuffer->getBoundMemory().getOffset(), VK_WHOLE_SIZE);
	}
	// Render pass
	{
		const VkImageUsageFlags		targetImageUsageFlags						= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		const ImageCreateInfo		targetImageCreateInfo						(VK_IMAGE_TYPE_2D, m_format, { WIDTH, HEIGHT, 1u }, 1u,	1u,	VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_TILING_OPTIMAL, targetImageUsageFlags);
		m_depthTargetImage														= Image::createAndAlloc(vk, device, targetImageCreateInfo, m_context.getDefaultAllocator(), queueFamilyIndex);

		RenderPassCreateInfo		renderPassCreateInfo;
		renderPassCreateInfo.addAttachment(AttachmentDescription(
			m_format,												// format
			VK_SAMPLE_COUNT_1_BIT,									// samples
			VK_ATTACHMENT_LOAD_OP_LOAD,								// loadOp
			VK_ATTACHMENT_STORE_OP_STORE,							// storeOp
			VK_ATTACHMENT_LOAD_OP_DONT_CARE,						// stencilLoadOp
			VK_ATTACHMENT_STORE_OP_DONT_CARE,						// stencilStoreOp
			VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,		// initialLayout
			VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL));		// finalLayout
		const VkAttachmentReference depthAttachmentReference					= makeAttachmentReference(0u, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
		renderPassCreateInfo.addSubpass(SubpassDescription(
			VK_PIPELINE_BIND_POINT_GRAPHICS,	// pipelineBindPoint
			(VkSubpassDescriptionFlags)0,		// flags
			0u,									// inputAttachmentCount
			DE_NULL,							// inputAttachments
			0u,									// colorAttachmentCount
			DE_NULL,							// colorAttachments
			DE_NULL,							// resolveAttachments
			depthAttachmentReference,			// depthStencilAttachment
			0u,									// preserveAttachmentCount
			DE_NULL));							// preserveAttachments
		m_renderPass														    = createRenderPass(vk, device, &renderPassCreateInfo);
	}

	const ImageViewCreateInfo					depthTargetViewInfo				(m_depthTargetImage->object(), VK_IMAGE_VIEW_TYPE_2D, m_format);
	m_depthTargetView															= createImageView(vk, device, &depthTargetViewInfo);

	const std::vector<VkImageView>				depthAttachments				{ *m_depthTargetView };
	FramebufferCreateInfo						framebufferCreateInfo			(*m_renderPass, depthAttachments, WIDTH, HEIGHT, 1);

	m_framebuffer																= createFramebuffer(vk, device, &framebufferCreateInfo);

	// Vertex input
	const VkVertexInputBindingDescription		vertexInputBindingDescription	=
	{
		0u,										// uint32_t             binding;
		sizeof(Vec4),							// uint32_t             stride;
		VK_VERTEX_INPUT_RATE_VERTEX,			// VkVertexInputRate    inputRate;
	};

	const VkVertexInputAttributeDescription		vertexInputAttributeDescription =
	{
		0u,										// uint32_t    location;
		0u,										// uint32_t    binding;
		VK_FORMAT_R32G32B32A32_SFLOAT,			// VkFormat    format;
		0u										// uint32_t    offset;
	};

	const PipelineCreateInfo::VertexInputState	vertexInputState				= PipelineCreateInfo::VertexInputState(1, &vertexInputBindingDescription,
																													   1, &vertexInputAttributeDescription);

	// Graphics pipeline
	const Unique<VkShaderModule>	vertexModule								(createShaderModule(vk, device, m_context.getBinaryCollection().get("vert"), 0));
	const Unique<VkShaderModule>	fragmentModule								(createShaderModule(vk, device, m_context.getBinaryCollection().get("frag"), 0));

	const PipelineLayoutCreateInfo	pipelineLayoutCreateInfo					(0u, DE_NULL, 0u, DE_NULL);
	m_pipelineLayout															= createPipelineLayout(vk, device, &pipelineLayoutCreateInfo);

	const VkRect2D					scissor										= makeRect2D(WIDTH, HEIGHT);
	const VkViewport				viewport									= makeViewport(WIDTH, HEIGHT);
	std::vector<VkDynamicState>		dynamicStates								(1, VK_DYNAMIC_STATE_VIEWPORT);

	PipelineCreateInfo pipelineCreateInfo(*m_pipelineLayout, *m_renderPass, 0, (VkPipelineCreateFlags)0);
	pipelineCreateInfo.addShader(PipelineCreateInfo::PipelineShaderStage(*vertexModule,   "main", VK_SHADER_STAGE_VERTEX_BIT));
	pipelineCreateInfo.addShader(PipelineCreateInfo::PipelineShaderStage(*fragmentModule, "main", VK_SHADER_STAGE_FRAGMENT_BIT));
	pipelineCreateInfo.addState (PipelineCreateInfo::VertexInputState	(vertexInputState));
	pipelineCreateInfo.addState (PipelineCreateInfo::InputAssemblerState(verticesTopology));
	pipelineCreateInfo.addState (PipelineCreateInfo::ViewportState		(1, std::vector<VkViewport>(1, viewport), std::vector<VkRect2D>(1, scissor)));
	pipelineCreateInfo.addState (PipelineCreateInfo::DepthStencilState	(VK_TRUE, VK_TRUE, VK_COMPARE_OP_ALWAYS, VK_FALSE, VK_FALSE));
	pipelineCreateInfo.addState (PipelineCreateInfo::RasterizerState	(
		VK_TRUE,										// depthClampEnable
		VK_FALSE,										// rasterizerDiscardEnable
		VK_POLYGON_MODE_FILL,							// polygonMode
		VK_CULL_MODE_NONE,								// cullMode
		VK_FRONT_FACE_CLOCKWISE,						// frontFace
		m_params.enableDepthBias ? VK_TRUE : VK_FALSE,	// depthBiasEnable
		m_params.depthBiasConstantFactor,				// depthBiasConstantFactor
		0.0f,											// depthBiasClamp
		0.0f,											// depthBiasSlopeFactor
		1.0f));											// lineWidth
	pipelineCreateInfo.addState (PipelineCreateInfo::MultiSampleState	());
	pipelineCreateInfo.addState (PipelineCreateInfo::DynamicState		(dynamicStates));
	m_pipeline																	= createGraphicsPipeline(vk, device, DE_NULL, &pipelineCreateInfo);
}

tcu::ConstPixelBufferAccess DepthClampTestInstance::draw (const VkViewport viewport)
{
	const DeviceInterface&				vk					= m_context.getDeviceInterface();
	const VkDevice						device				= m_context.getDevice();
	const VkQueue						queue				= m_context.getUniversalQueue();
	const deUint32						queueFamilyIndex	= m_context.getUniversalQueueFamilyIndex();

	const CmdPoolCreateInfo				cmdPoolCreateInfo	(queueFamilyIndex);
	const Unique<VkCommandPool>			cmdPool				(createCommandPool(vk, device, &cmdPoolCreateInfo));
	const Unique<VkCommandBuffer>		cmdBuffer			(allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

	const bool							isCombinedType		= tcu::isCombinedDepthStencilType(mapVkFormat(m_format).type) && m_format != VK_FORMAT_X8_D24_UNORM_PACK32;

	beginCommandBuffer(vk, *cmdBuffer);
	vk.cmdSetViewport(*cmdBuffer, 0u, 1u, &viewport);
	if (isCombinedType)
		initialTransitionDepthStencil2DImage(vk, *cmdBuffer, m_depthTargetImage->object(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
	else
		initialTransitionDepth2DImage(vk, *cmdBuffer, m_depthTargetImage->object(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

	const VkImageAspectFlagBits			aspectBits			= (VkImageAspectFlagBits)(isCombinedType ? VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT : VK_IMAGE_ASPECT_DEPTH_BIT);
	const ImageSubresourceRange			subresourceRange	(aspectBits);
	const VkClearDepthStencilValue		clearDepth		    = makeClearDepthStencilValue(initialClearDepth, 0u);

	vk.cmdClearDepthStencilImage(*cmdBuffer, m_depthTargetImage->object(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearDepth, 1, &subresourceRange);

	transition2DImage(vk, *cmdBuffer, m_depthTargetImage->object(), aspectBits,
					  VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
					  VK_ACCESS_TRANSFER_WRITE_BIT		  , VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
					  VK_PIPELINE_STAGE_TRANSFER_BIT	  , VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT);

	{
		const VkMemoryBarrier memBarrier					= { VK_STRUCTURE_TYPE_MEMORY_BARRIER, DE_NULL, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT };
		vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 1, &memBarrier, 0, DE_NULL, 0, DE_NULL);
	}

	beginRenderPass(vk, *cmdBuffer, *m_renderPass, *m_framebuffer, makeRect2D(0, 0, WIDTH, HEIGHT));
	vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipeline);
	const VkDeviceSize		offset							= 0;
	const VkBuffer			buffer							= m_vertexBuffer->object();
	vk.cmdBindVertexBuffers(*cmdBuffer, 0, 1, &buffer, &offset);
	vk.cmdDraw(*cmdBuffer, DE_LENGTH_OF_ARRAY(vertices), 1, 0, 0);
	endRenderPass(vk, *cmdBuffer);

	transition2DImage(vk, *cmdBuffer, m_depthTargetImage->object(), aspectBits,
					  VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
					  VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT	  , VK_ACCESS_MEMORY_READ_BIT,
					  VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT		  , VK_PIPELINE_STAGE_HOST_BIT);

	endCommandBuffer(vk, *cmdBuffer);
	submitCommandsAndWait(vk, device, queue, *cmdBuffer);

	VK_CHECK(vk.queueWaitIdle(queue));

	return m_depthTargetImage->readDepth(queue, m_context.getDefaultAllocator(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, { 0, 0, 0 }, WIDTH, HEIGHT, VK_IMAGE_ASPECT_DEPTH_BIT);
}

tcu::TestStatus DepthClampTestInstance::iterate (void)
{
	// Set up the viewport and draw
	const VkViewport viewport						=
	{
		0.0f,															// float    x;
		0.0f,															// float    y;
		WIDTH,															// float    width;
		HEIGHT,															// float    height;
		m_params.expectedValue < 0.0f ? m_params.expectedValue : 0.0f,	// float    minDepth;
		m_params.expectedValue > 1.0f ? m_params.expectedValue : 1.0f,	// float    maxDepth;
	};
	const tcu::ConstPixelBufferAccess	resultImage	= draw(viewport);

	DE_ASSERT((isUnormDepthFormat(m_format) == false) ||
		(m_params.expectedValue >= 0.0f && m_params.expectedValue <= 1.0f));

	for(int z = 0; z < resultImage.getDepth(); ++z)
	for(int y = 0; y < resultImage.getHeight(); ++y)
	for(int x = 0; x < resultImage.getWidth(); ++x)
	{
		if (std::abs(m_params.expectedValue - resultImage.getPixDepth(x,y,z)) >= m_epsilon)
		{
			std::ostringstream msg;
			msg << "Depth value mismatch, expected: " << m_params.expectedValue << ", got: " << resultImage.getPixDepth(x,y,z) << " at " << "(" << x << ", " << y << ", " << z << ")";
			return tcu::TestStatus::fail(msg.str());
		}
	}
	return tcu::TestStatus::pass("Pass");
}

class DepthClampTest : public TestCase
{
public:
    DepthClampTest (tcu::TestContext &testCtx, const string& name, const string& description, const TestParams &params, const VkFormat format, const float epsilon)
		: TestCase	(testCtx, name, description)
		, m_params(params)
		, m_format(format)
		, m_epsilon(epsilon)
	{
	}

	virtual void initPrograms (SourceCollections& programCollection) const
	{
		{
			std::ostringstream src;
			src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
				<< "\n"
				<< "layout(location = 0) in vec4 in_position;\n"
				<< "\n"
				<< "out gl_PerVertex {\n"
				<< "    vec4  gl_Position;\n"
				<< "};\n"
				<< "\n"
				<< "void main(void)\n"
				<< "{\n"
				<< "    gl_Position = in_position;\n"
				<< "}\n";
			programCollection.glslSources.add("vert") << glu::VertexSource(src.str());
		}
		{
			std::ostringstream src;
			src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
				<< "\n"
				<< "void main(void)\n"
				<< "{\n"
				<< "}\n";
			programCollection.glslSources.add("frag") << glu::FragmentSource(src.str());
		}
	}

	virtual void checkSupport (Context& context) const
	{
		context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_DEPTH_CLAMP);
		for(const auto& extensionName : m_params.requiredExtensions)
		{
			context.requireDeviceFunctionality(extensionName);
		}
		VkImageFormatProperties imageFormatProperties;
		const auto&	vki		= context.getInstanceInterface();
		const auto&	vkd		= context.getPhysicalDevice();
		const auto	usage	= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		if (vki.getPhysicalDeviceImageFormatProperties(vkd, m_format, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL, usage, 0u, &imageFormatProperties) == VK_ERROR_FORMAT_NOT_SUPPORTED)
		{
			TCU_THROW(NotSupportedError, "Format not supported");
		}
	}

	virtual TestInstance* createInstance (Context& context) const
	{
		return new DepthClampTestInstance(context, m_params, m_format, m_epsilon);
	}

private:
	const TestParams	m_params;
	const VkFormat		m_format;
	const float			m_epsilon;
};

std::string getFormatCaseName (VkFormat format)
{
	return de::toLower(de::toString(getFormatStr(format)).substr(10));
}

void createTests (tcu::TestCaseGroup* testGroup)
{
	for(int i = 0; i < DE_LENGTH_OF_ARRAY(depthStencilImageFormatsToTest); ++i)
	{
		const auto		format			= depthStencilImageFormatsToTest[i];
		const float		epsilon			= depthEpsilonValuesByFormat[i];
		const auto		formatCaseName	= getFormatCaseName(format);
		for(const auto& params : depthClearValuesToTest)
		{
			if ((params.skipSNorm && vk::isSnormFormat(format)) || (params.skipUNorm && isUnormDepthFormat(format)))
				continue;
			const auto	testCaseName	= formatCaseName + params.testNameSuffix;
			testGroup->addChild(new DepthClampTest(testGroup->getTestContext(), testCaseName, "Depth clamp", params, format, epsilon));
		}
	}
}
}	// anonymous

tcu::TestCaseGroup*	createDepthClampTests (tcu::TestContext& testCtx)
{
	return createTestGroup(testCtx, "depth_clamp", "Depth Clamp Tests", createTests);
}
}	// Draw
}	// vkt
