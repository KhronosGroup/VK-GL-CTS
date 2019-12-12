/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2019 The Khronos Group Inc.
 * Copyright (c) 2019 Google LLC.
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
 * \brief Tests for provoking vertex
 *//*--------------------------------------------------------------------*/

#include "vktRasterizationProvokingVertexTests.hpp"

#include "vkCmdUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkRefUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vktTestGroupUtil.hpp"
#include "tcuRGBA.hpp"
#include "tcuSurface.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuTestLog.hpp"

using namespace vk;

namespace vkt
{
namespace rasterization
{
namespace
{

enum ProvokingVertexMode
{
	PROVOKING_VERTEX_DEFAULT,
	PROVOKING_VERTEX_FIRST,
	PROVOKING_VERTEX_LAST,
	PROVOKING_VERTEX_PER_PIPELINE
};

struct Params
{
	VkFormat			format;
	tcu::UVec2			size;
	VkPrimitiveTopology	primitiveTopology;
	bool				requireGeometryShader;
	ProvokingVertexMode	provokingVertexMode;
};

class ProvokingVertexTestInstance : public TestInstance
{
public:
						ProvokingVertexTestInstance	(Context& context, Params params);
	tcu::TestStatus		iterate						(void);
private:
	Params	m_params;
};

ProvokingVertexTestInstance::ProvokingVertexTestInstance (Context& context, Params params)
	: TestInstance	(context)
	, m_params		(params)
{
}

class ProvokingVertexTestCase : public TestCase
{
public:
							ProvokingVertexTestCase	(tcu::TestContext&	testCtx,
													 const std::string&	name,
													 const std::string&	description,
													 const Params	params);
	virtual void			initPrograms			(SourceCollections& programCollection) const;
	virtual void			checkSupport			(Context& context) const;
	virtual TestInstance*	createInstance			(Context& context) const;
private:
	const Params			m_params;
};

ProvokingVertexTestCase::ProvokingVertexTestCase (tcu::TestContext& testCtx,
												  const std::string& name,
												  const std::string& description,
												  const Params params)
	: TestCase	(testCtx, name, description)
	, m_params	(params)
{
}

void ProvokingVertexTestCase::initPrograms (SourceCollections& programCollection) const
{
	const std::string	vertShader (
		"#version 430\n"
		"layout(location = 0) in vec4 in_position;\n"
		"layout(location = 1) in vec4 in_color;\n"
		"layout(location = 0) flat out vec4 out_color;\n"
		"void main()\n"
		"{\n"
		"    out_color = in_color;\n"
		"    gl_Position = in_position;\n"
		"}\n");

	const std::string	fragShader (
		"#version 430\n"
		"layout(location = 0) flat in vec4 in_color;\n"
		"layout(location = 0) out vec4 out_color;\n"
		"void main()\n"
		"{\n"
		"    out_color = in_color;\n"
		"}\n");

	programCollection.glslSources.add("vert") << glu::VertexSource(vertShader);
	programCollection.glslSources.add("frag") << glu::FragmentSource(fragShader);
}

void ProvokingVertexTestCase::checkSupport (Context& context) const
{
	if (m_params.requireGeometryShader)
		context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_GEOMETRY_SHADER);

	if (m_params.provokingVertexMode != PROVOKING_VERTEX_DEFAULT)
	{
		context.requireDeviceFunctionality("VK_EXT_provoking_vertex");

		if (m_params.provokingVertexMode != PROVOKING_VERTEX_FIRST)
		{
			const InstanceInterface&					instanceDriver			= context.getInstanceInterface();
			VkPhysicalDeviceFeatures2					physicalDeviceFeatures;

			VkPhysicalDeviceProvokingVertexFeaturesEXT	provokingVertexFeatures	=
			{
				VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROVOKING_VERTEX_FEATURES_EXT,	// sType
				DE_NULL,															// pNext
				VK_FALSE,															// provokingVertexLast
				VK_FALSE															// transformFeedbackPreservesProvokingVertex
			};

			deMemset(&physicalDeviceFeatures, 0, sizeof(physicalDeviceFeatures));
			physicalDeviceFeatures.sType	= VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
			physicalDeviceFeatures.pNext	= &provokingVertexFeatures;
			instanceDriver.getPhysicalDeviceFeatures2(context.getPhysicalDevice(), &physicalDeviceFeatures);

			if (provokingVertexFeatures.provokingVertexLast != VK_TRUE)
				TCU_THROW(NotSupportedError, "VK_PROVOKING_VERTEX_MODE_LAST_VERTEX_EXT not supported");

			if (m_params.provokingVertexMode == PROVOKING_VERTEX_PER_PIPELINE)
			{
				VkPhysicalDeviceProvokingVertexPropertiesEXT	provokingVertexProperties	=
				{
					VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROVOKING_VERTEX_PROPERTIES_EXT,	// sType
					DE_NULL,															// pNext
					VK_FALSE,															// provokingVertexModePerPipeline
					VK_FALSE															// transformFeedbackPreservesProvokingVertex
				};

				VkPhysicalDeviceProperties2						physicalDeviceProperties;

				deMemset(&physicalDeviceProperties, 0, sizeof(physicalDeviceProperties));
				physicalDeviceProperties.sType	= VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
				physicalDeviceProperties.pNext	= &provokingVertexProperties;
				instanceDriver.getPhysicalDeviceProperties2(context.getPhysicalDevice(), &physicalDeviceProperties);

				if (provokingVertexProperties.provokingVertexModePerPipeline != VK_TRUE)
					TCU_THROW(NotSupportedError, "provokingVertexModePerPipeline not supported");
			}
		}
	}
}

TestInstance* ProvokingVertexTestCase::createInstance (Context& context) const
{
	return new ProvokingVertexTestInstance(context, m_params);
}

tcu::TestStatus ProvokingVertexTestInstance::iterate (void)
{
	const bool					useProvokingVertexExt	= (m_params.provokingVertexMode != PROVOKING_VERTEX_DEFAULT);
	const DeviceInterface&		vk						= m_context.getDeviceInterface();
	const VkDevice				device					= m_context.getDevice();
	const deUint32				queueFamilyIndex		= m_context.getUniversalQueueFamilyIndex();
	const tcu::TextureFormat	textureFormat			= vk::mapVkFormat(m_params.format);
	Allocator&					allocator				= m_context.getDefaultAllocator();
	Move<VkCommandPool>			commandPool;
	Move<VkCommandBuffer>		commandBuffer;
	Move<VkImage>				image;
	Move<VkImageView>			imageView;
	de::MovePtr<Allocation>		imageMemory;
	Move<VkBuffer>				resultBuffer;
	de::MovePtr<Allocation>		resultBufferMemory;
	Move<VkBuffer>				vertexBuffer;
	de::MovePtr<Allocation>		vertexBufferMemory;
	Move<VkRenderPass>			renderPass;
	Move<VkFramebuffer>			framebuffer;
	Move<VkPipeline>			pipeline;
	Move<VkPipeline>			altPipeline;
	deUint32					vertexCount				= 0;

	// Image
	{
		const VkExtent3D		extent		=
		{
			m_params.size.x(),	// width
			m_params.size.y(),	// height
			1					// depth
		};

		const VkImageUsageFlags	usage		= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
											  VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

		const VkImageCreateInfo	createInfo	=
		{
			VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,	// sType
			DE_NULL,								// pNext
			0u,										// flags
			VK_IMAGE_TYPE_2D,						// imageType
			m_params.format,						// format
			extent,									// extent
			1u,										// mipLevels
			1u,										// arrayLayers
			VK_SAMPLE_COUNT_1_BIT,					// samples
			VK_IMAGE_TILING_OPTIMAL,				// tiling
			usage,									// usage
			VK_SHARING_MODE_EXCLUSIVE,				// sharingMode
			1u,										// queueFamilyIndexCount
			&queueFamilyIndex,						// pQueueFamilyIndices
			VK_IMAGE_LAYOUT_UNDEFINED				// initialLayout
		};

		image = createImage(vk, device, &createInfo, DE_NULL);

		imageMemory	= allocator.allocate(getImageMemoryRequirements(vk, device, *image), MemoryRequirement::Any);
		VK_CHECK(vk.bindImageMemory(device, *image, imageMemory->getMemory(), imageMemory->getOffset()));
	}

	// Image view
	{
		const VkImageSubresourceRange	subresourceRange	=
		{
			VK_IMAGE_ASPECT_COLOR_BIT,	// aspectMask
			0u,							// baseMipLevel
			1u,							// mipLevels
			0u,							// baseArrayLayer
			1u							// arraySize
		};

		const VkImageViewCreateInfo		createInfo			=
		{
			VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,	// sType
			DE_NULL,									// pNext
			0u,											// flags
			*image,										// image
			VK_IMAGE_VIEW_TYPE_2D,						// viewType
			m_params.format,							// format
			makeComponentMappingRGBA(),					// components
			subresourceRange							// subresourceRange
		};

		imageView = createImageView(vk, device, &createInfo, DE_NULL);
	}

	// Result Buffer
	{
		const VkDeviceSize			bufferSize		= textureFormat.getPixelSize() * m_params.size.x() * m_params.size.y();

		const VkBufferCreateInfo	createInfo		=
		{
			VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,	// sType
			DE_NULL,								// pNext
			0u,										// flags
			bufferSize,								// size
			VK_BUFFER_USAGE_TRANSFER_DST_BIT,		// usage
			VK_SHARING_MODE_EXCLUSIVE,				// sharingMode
			1u,										// queueFamilyIndexCount
			&queueFamilyIndex						// pQueueFamilyIndices
		};

		resultBuffer		= createBuffer(vk, device, &createInfo);
		resultBufferMemory	= allocator.allocate(getBufferMemoryRequirements(vk, device, *resultBuffer), MemoryRequirement::HostVisible);

		VK_CHECK(vk.bindBufferMemory(device, *resultBuffer, resultBufferMemory->getMemory(), resultBufferMemory->getOffset()));
	}

	// Render pass
	{
		const VkAttachmentDescription	attachmentDesc	=
		{
			0u,											// flags
			m_params.format,							// format
			VK_SAMPLE_COUNT_1_BIT,						// samples
			VK_ATTACHMENT_LOAD_OP_CLEAR,				// loadOp
			VK_ATTACHMENT_STORE_OP_STORE,				// storeOp
			VK_ATTACHMENT_LOAD_OP_DONT_CARE,			// stencilLoadOp
			VK_ATTACHMENT_STORE_OP_DONT_CARE,			// stencilStoreOp
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,	// initialLayout
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL	// finalLayout
		};

		const VkAttachmentReference		attachmentRef	=
		{
			0u,											// attachment
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,	// layout
		};

		const VkSubpassDescription		subpassDesc		=
		{
			0u,									// flags
			VK_PIPELINE_BIND_POINT_GRAPHICS,	// pipelineBindPoint
			0u,									// inputAttachmentCount
			DE_NULL,							// pInputAttachments
			1u,									// colorAttachmentCount
			&attachmentRef,						// pColorAttachments
			DE_NULL,							// pResolveAttachments
			DE_NULL,							// pDepthStencilAttachment
			0u,									// preserveAttachmentCount
			DE_NULL								// pPreserveAttachments
		};

		const VkRenderPassCreateInfo	createInfo		=
		{
			VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,	// sType
			DE_NULL,									// pNext
			0u,											// flags
			1u,											// attachmentCount
			&attachmentDesc,							// pAttachments
			1u,											// subpassCount
			&subpassDesc,								// pSubpasses
			0u,											// dependencyCount
			DE_NULL									// pDependencies
		};

		renderPass = createRenderPass(vk, device, &createInfo, DE_NULL);
	}

	// Framebuffer
	{
		const VkFramebufferCreateInfo	createInfo	=
		{
			VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,	// sType
			DE_NULL,									// pNext
			0u,											// flags
			*renderPass,								// renderPass
			1u,											// attachmentCount
			&*imageView,								// pAttachments
			m_params.size.x(),							// width
			m_params.size.y(),							// height
			1u											// layers
		};

		framebuffer = createFramebuffer(vk, device, &createInfo, DE_NULL);
	}

	// Pipelines
	{
		const Unique<VkShaderModule>									vertexShader					(createShaderModule(vk, device, m_context.getBinaryCollection().get("vert"), 0));
		const Unique<VkShaderModule>									fragmentShader					(createShaderModule(vk, device, m_context.getBinaryCollection().get("frag"), 0));
		const std::vector<VkViewport>									viewports						(1, makeViewport(tcu::UVec2(m_params.size)));
		const std::vector<VkRect2D>										scissors						(1, makeRect2D(tcu::UVec2(m_params.size)));;

		const VkVertexInputBindingDescription							vertexInputBindingDescription	=
		{
			0,							// binding
			sizeof(tcu::Vec4) * 2,		// strideInBytes
			VK_VERTEX_INPUT_RATE_VERTEX	// stepRate
		};

		const VkPipelineLayoutCreateInfo								pipelineLayoutCreateInfo		=
		{
			VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,	// sType
			DE_NULL,										// pNext
			0u,												// flags
			0u,												// descriptorSetCount
			DE_NULL,										// pSetLayouts
			0u,												// pushConstantRangeCount
			DE_NULL											// pPushConstantRanges
		};

		const Move<VkPipelineLayout>									pipelineLayout					= createPipelineLayout(vk, device, &pipelineLayoutCreateInfo);

		const VkVertexInputAttributeDescription							vertexAttributeDescriptions[2]	=
		{
			// Position
			{
				0u,								// location
				0u,								// binding
				VK_FORMAT_R32G32B32A32_SFLOAT,	// format
				0u								// offsetInBytes
			},
			// Color
			{
				1u,								// location
				0u,								// binding
				VK_FORMAT_R32G32B32A32_SFLOAT,	// format
				sizeof(tcu::Vec4)				// offsetInBytes
			}
		};

		const VkPipelineVertexInputStateCreateInfo						vertexInputStateParams			=
		{
			VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,	// sType
			DE_NULL,													// pNext
			0,															// flags
			1u,															// bindingCount
			&vertexInputBindingDescription,								// pVertexBindingDescriptions
			2u,															// attributeCount
			vertexAttributeDescriptions									// pVertexAttributeDescriptions
		};

		const VkPipelineMultisampleStateCreateInfo						multisampleStateParams			=
		{
			VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,	// sType
			DE_NULL,													// pNext
			0u,															// flags
			VK_SAMPLE_COUNT_1_BIT,										// rasterizationSamples
			VK_FALSE,													// sampleShadingEnable
			0.0f,														// minSampleShading
			DE_NULL,													// SampleMask
			VK_FALSE,													// alphaToCoverageEnable
			VK_FALSE													// alphaToOneEnable
		};

		const VkProvokingVertexModeEXT									provokingVertexMode				= m_params.provokingVertexMode == PROVOKING_VERTEX_LAST
																										? VK_PROVOKING_VERTEX_MODE_LAST_VERTEX_EXT
																										: VK_PROVOKING_VERTEX_MODE_FIRST_VERTEX_EXT;

		const VkPipelineRasterizationProvokingVertexStateCreateInfoEXT	provokingVertexCreateInfo		=
		{
			VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_PROVOKING_VERTEX_STATE_CREATE_INFO_EXT,	// sType
			DE_NULL,																			// pNext
			provokingVertexMode																	// provokingVertexMode
		};

		const VkPipelineRasterizationStateCreateInfo					rasterizationStateCreateInfo	=
		{
			VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,		// sType
			useProvokingVertexExt ? &provokingVertexCreateInfo : DE_NULL,	// pNext
			0,																// flags
			false,															// depthClipEnable
			false,															// rasterizerDiscardEnable
			VK_POLYGON_MODE_FILL,											// fillMode
			VK_CULL_MODE_NONE,												// cullMode
			VK_FRONT_FACE_COUNTER_CLOCKWISE,								// frontFace
			VK_FALSE,														// depthBiasEnable
			0.0f,															// depthBias
			0.0f,															// depthBiasClamp
			0.0f,															// slopeScaledDepthBias
			1.0f															// lineWidth
		};

		const VkColorComponentFlags										channelWriteMask				= VK_COLOR_COMPONENT_R_BIT |
																										  VK_COLOR_COMPONENT_G_BIT |
																										  VK_COLOR_COMPONENT_B_BIT |
																										  VK_COLOR_COMPONENT_A_BIT;

		const VkPipelineColorBlendAttachmentState						colorBlendAttachmentState		=
		{
			false,					// blendEnable
			VK_BLEND_FACTOR_ONE,	// srcBlendColor
			VK_BLEND_FACTOR_ZERO,	// destBlendColor
			VK_BLEND_OP_ADD,		// blendOpColor
			VK_BLEND_FACTOR_ONE,	// srcBlendAlpha
			VK_BLEND_FACTOR_ZERO,	// destBlendAlpha
			VK_BLEND_OP_ADD,		// blendOpAlpha
			channelWriteMask		// channelWriteMask
		};

		const VkPipelineColorBlendStateCreateInfo						colorBlendStateParams			=
		{
			VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,	// sType
			DE_NULL,													// pNext
			0,															// flags
			false,														// logicOpEnable
			VK_LOGIC_OP_COPY,											// logicOp
			1u,															// attachmentCount
			&colorBlendAttachmentState,									// pAttachments
			{ 0.0f, 0.0f, 0.0f, 0.0f }									// blendConst[4]
		};

		const VkPipelineDynamicStateCreateInfo							dynamicStateCreateInfo			=
		{
			VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,	// sType
			DE_NULL,												// pNext
			0u,														// flags
			0u,														// dynamicStateCount
			DE_NULL													// pDynamicStates
		};

		pipeline = makeGraphicsPipeline(vk,
										device,
										*pipelineLayout,
										*vertexShader,
										DE_NULL,						// tessellationControlShaderModule
										DE_NULL,						// tessellationEvalShaderModule
										DE_NULL,						// geometryShaderModule
										*fragmentShader,
										*renderPass,
										viewports,
										scissors,
										m_params.primitiveTopology,
										0u,								// subpass
										0u,								// patchControlPoints
										&vertexInputStateParams,
										&rasterizationStateCreateInfo,
										&multisampleStateParams,
										DE_NULL,						// depthStencilStateCreateInfo,
										&colorBlendStateParams,
										&dynamicStateCreateInfo);

		if (m_params.provokingVertexMode == PROVOKING_VERTEX_PER_PIPELINE)
		{
			const VkPipelineRasterizationProvokingVertexStateCreateInfoEXT	altProvokingVertexCreateInfo	=
			{
				VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_PROVOKING_VERTEX_STATE_CREATE_INFO_EXT,	// sType
				DE_NULL,																			// pNext
				VK_PROVOKING_VERTEX_MODE_LAST_VERTEX_EXT											// provokingVertexMode
			};

			const VkPipelineRasterizationStateCreateInfo					altRasterizationStateCreateInfo	=
			{
				VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,	// sType
				&altProvokingVertexCreateInfo,								// pNext
				0,															// flags
				false,														// depthClipEnable
				false,														// rasterizerDiscardEnable
				VK_POLYGON_MODE_FILL,										// fillMode
				VK_CULL_MODE_NONE,											// cullMode
				VK_FRONT_FACE_COUNTER_CLOCKWISE,							// frontFace
				VK_FALSE,													// depthBiasEnable
				0.0f,														// depthBias
				0.0f,														// depthBiasClamp
				0.0f,														// slopeScaledDepthBias
				1.0f,														// lineWidth
			};

			altPipeline = makeGraphicsPipeline(vk,
											   device,
											   *pipelineLayout,
											   *vertexShader,
											   DE_NULL,							// tessellationControlShaderModule
											   DE_NULL,							// tessellationEvalShaderModule
											   DE_NULL,							// geometryShaderModule
											   *fragmentShader,
											   *renderPass,
											   viewports,
											   scissors,
											   m_params.primitiveTopology,
											   0u,								// subpass
											   0u,								// patchControlPoints
											   &vertexInputStateParams,
											   &altRasterizationStateCreateInfo,
											   &multisampleStateParams,
											   DE_NULL,							// depthStencilStateCreateInfo,
											   &colorBlendStateParams,
											   &dynamicStateCreateInfo);
		}
	}

	// Vertex buffer
	{
		std::vector<tcu::Vec4>	vertices;

		switch (m_params.primitiveTopology)
		{
			case VK_PRIMITIVE_TOPOLOGY_LINE_LIST:
				// Position												//Color
				vertices.push_back(tcu::Vec4(-1.0f,-0.5f, 0.0f, 1.0f));	vertices.push_back(tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f));	// line 0
				vertices.push_back(tcu::Vec4( 1.0f,-0.5f, 0.0f, 1.0f));	vertices.push_back(tcu::Vec4(0.0f, 0.0f, 1.0f, 1.0f));
				vertices.push_back(tcu::Vec4(-1.0f, 0.5f, 0.0f, 1.0f));	vertices.push_back(tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f));	// line 1
				vertices.push_back(tcu::Vec4( 1.0f, 0.5f, 0.0f, 1.0f));	vertices.push_back(tcu::Vec4(0.0f, 0.0f, 1.0f, 1.0f));

				vertices.push_back(tcu::Vec4(-0.5f,-1.0f, 0.0f, 1.0f));	vertices.push_back(tcu::Vec4(0.0f, 0.0f, 1.0f, 1.0f));	// line 1 reverse
				vertices.push_back(tcu::Vec4(-0.5f, 1.0f, 0.0f, 1.0f));	vertices.push_back(tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f));
				vertices.push_back(tcu::Vec4( 0.5f,-1.0f, 0.0f, 1.0f));	vertices.push_back(tcu::Vec4(0.0f, 0.0f, 1.0f, 1.0f));	// line 0 reverse
				vertices.push_back(tcu::Vec4( 0.5f, 1.0f, 0.0f, 1.0f));	vertices.push_back(tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f));
				break;
			case VK_PRIMITIVE_TOPOLOGY_LINE_STRIP:
				// Position												// Color
				vertices.push_back(tcu::Vec4(-1.0f,-0.5f, 0.0f, 1.0f));	vertices.push_back(tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f)); // line strip
				vertices.push_back(tcu::Vec4( 1.0f,-0.5f, 0.0f, 1.0f));	vertices.push_back(tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f));
				vertices.push_back(tcu::Vec4(-1.0f, 0.5f, 0.0f, 1.0f));	vertices.push_back(tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f));
				vertices.push_back(tcu::Vec4( 1.0f, 0.5f, 0.0f, 1.0f));	vertices.push_back(tcu::Vec4(0.0f, 1.0f, 0.0f, 1.0f));

				vertices.push_back(tcu::Vec4(-0.5f,-1.0f, 0.0f, 1.0f));	vertices.push_back(tcu::Vec4(0.0f, 1.0f, 0.0f, 1.0f)); // line strip reverse
				vertices.push_back(tcu::Vec4(-0.5f, 1.0f, 0.0f, 1.0f));	vertices.push_back(tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f));
				vertices.push_back(tcu::Vec4( 0.5f,-1.0f, 0.0f, 1.0f));	vertices.push_back(tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f));
				vertices.push_back(tcu::Vec4( 0.5f, 1.0f, 0.0f, 1.0f));	vertices.push_back(tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f));
				break;
			case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST:
				// Position												// Color
				vertices.push_back(tcu::Vec4( 1.0f, 1.0f, 0.0f, 1.0f));	vertices.push_back(tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f));	// triangle 0
				vertices.push_back(tcu::Vec4(-0.6f,-1.0f, 0.0f, 1.0f));	vertices.push_back(tcu::Vec4(0.0f, 1.0f, 0.0f, 1.0f));
				vertices.push_back(tcu::Vec4(-0.2f, 1.0f, 0.0f, 1.0f));	vertices.push_back(tcu::Vec4(0.0f, 0.0f, 1.0f, 1.0f));
				vertices.push_back(tcu::Vec4( 0.2f, 1.0f, 0.0f, 1.0f));	vertices.push_back(tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f));	// triangle 1
				vertices.push_back(tcu::Vec4( 0.6f,-1.0f, 0.0f, 1.0f));	vertices.push_back(tcu::Vec4(0.0f, 1.0f, 0.0f, 1.0f));
				vertices.push_back(tcu::Vec4( 1.0f, 1.0f, 0.0f, 1.0f));	vertices.push_back(tcu::Vec4(0.0f, 0.0f, 1.0f, 1.0f));

				vertices.push_back(tcu::Vec4(-1.0f,-1.0f, 0.0f, 1.0f));	vertices.push_back(tcu::Vec4(0.0f, 0.0f, 1.0f, 1.0f));	// triangle 1 reverse
				vertices.push_back(tcu::Vec4(-0.6f, 1.0f, 0.0f, 1.0f));	vertices.push_back(tcu::Vec4(0.0f, 1.0f, 0.0f, 1.0f));
				vertices.push_back(tcu::Vec4(-0.2f,-1.0f, 0.0f, 1.0f));	vertices.push_back(tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f));
				vertices.push_back(tcu::Vec4( 0.2f,-1.0f, 0.0f, 1.0f));	vertices.push_back(tcu::Vec4(0.0f, 0.0f, 1.0f, 1.0f));	// triangle 0 reverse
				vertices.push_back(tcu::Vec4( 0.6f, 1.0f, 0.0f, 1.0f));	vertices.push_back(tcu::Vec4(0.0f, 1.0f, 0.0f, 1.0f));
				vertices.push_back(tcu::Vec4(-1.0f,-1.0f, 0.0f, 1.0f));	vertices.push_back(tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f));
				break;
			case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP:
				// Position												// Color
				vertices.push_back(tcu::Vec4(-1.0f, 1.0f, 0.0f, 1.0f));	vertices.push_back(tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f));	// triangle strip
				vertices.push_back(tcu::Vec4(-0.5f,-1.0f, 0.0f, 1.0f));	vertices.push_back(tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f));
				vertices.push_back(tcu::Vec4( 0.0f, 1.0f, 0.0f, 1.0f));	vertices.push_back(tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f));
				vertices.push_back(tcu::Vec4( 0.5f,-1.0f, 0.0f, 1.0f));	vertices.push_back(tcu::Vec4(0.0f, 1.0f, 0.0f, 1.0f));
				vertices.push_back(tcu::Vec4( 1.0f, 1.0f, 0.0f, 1.0f));	vertices.push_back(tcu::Vec4(0.0f, 0.0f, 1.0f, 1.0f));

				vertices.push_back(tcu::Vec4(-1.0f,-1.0f, 0.0f, 1.0f));	vertices.push_back(tcu::Vec4(0.0f, 0.0f, 1.0f, 1.0f));	// triangle strip reverse
				vertices.push_back(tcu::Vec4(-0.5f, 1.0f, 0.0f, 1.0f));	vertices.push_back(tcu::Vec4(0.0f, 1.0f, 0.0f, 1.0f));
				vertices.push_back(tcu::Vec4( 0.0f,-1.0f, 0.0f, 1.0f));	vertices.push_back(tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f));
				vertices.push_back(tcu::Vec4( 0.5f, 1.0f, 0.0f, 1.0f));	vertices.push_back(tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f));
				vertices.push_back(tcu::Vec4( 1.0f,-1.0f, 0.0f, 1.0f));	vertices.push_back(tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f));
				break;
			case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN:
				// Position												// Color
				vertices.push_back(tcu::Vec4( 0.0f, 1.0f, 0.0f, 1.0f));	vertices.push_back(tcu::Vec4(0.0f, 1.0f, 0.0f, 1.0f));	// triangle fan
				vertices.push_back(tcu::Vec4(-1.0f, 1.0f, 0.0f, 1.0f));	vertices.push_back(tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f));
				vertices.push_back(tcu::Vec4(-0.5f,-1.0f, 0.0f, 1.0f));	vertices.push_back(tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f));
				vertices.push_back(tcu::Vec4( 0.5f,-1.0f, 0.0f, 1.0f));	vertices.push_back(tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f));
				vertices.push_back(tcu::Vec4( 1.0f, 1.0f, 0.0f, 1.0f));	vertices.push_back(tcu::Vec4(0.0f, 0.0f, 1.0f, 1.0f));

				vertices.push_back(tcu::Vec4( 0.0f,-1.0f, 0.0f, 1.0f));	vertices.push_back(tcu::Vec4(0.0f, 1.0f, 0.0f, 1.0f)); // triangle fan reverse
				vertices.push_back(tcu::Vec4(-1.0f,-1.0f, 0.0f, 1.0f));	vertices.push_back(tcu::Vec4(0.0f, 0.0f, 1.0f, 1.0f));
				vertices.push_back(tcu::Vec4(-0.5f, 1.0f, 0.0f, 1.0f));	vertices.push_back(tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f));
				vertices.push_back(tcu::Vec4( 0.5f, 1.0f, 0.0f, 1.0f));	vertices.push_back(tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f));
				vertices.push_back(tcu::Vec4( 1.0f,-1.0f, 0.0f, 1.0f));	vertices.push_back(tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f));
				break;
			case VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY:
				// Position												// Color
				vertices.push_back(tcu::Vec4(-1.0f,-0.5f, 0.0f, 1.0f));	vertices.push_back(tcu::Vec4(0.0f, 1.0f, 0.0f, 1.0f));	// line 0
				vertices.push_back(tcu::Vec4(-0.5f,-0.5f, 0.0f, 1.0f));	vertices.push_back(tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f));
				vertices.push_back(tcu::Vec4( 0.5f,-0.5f, 0.0f, 1.0f));	vertices.push_back(tcu::Vec4(0.0f, 0.0f, 1.0f, 1.0f));
				vertices.push_back(tcu::Vec4( 1.0f,-0.5f, 0.0f, 1.0f));	vertices.push_back(tcu::Vec4(1.0f, 1.0f, 0.0f, 1.0f));
				vertices.push_back(tcu::Vec4(-1.0f, 0.5f, 0.0f, 1.0f));	vertices.push_back(tcu::Vec4(0.0f, 1.0f, 0.0f, 1.0f));	// line 1
				vertices.push_back(tcu::Vec4(-0.5f, 0.5f, 0.0f, 1.0f));	vertices.push_back(tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f));
				vertices.push_back(tcu::Vec4( 0.5f, 0.5f, 0.0f, 1.0f));	vertices.push_back(tcu::Vec4(0.0f, 0.0f, 1.0f, 1.0f));
				vertices.push_back(tcu::Vec4( 1.0f, 0.5f, 0.0f, 1.0f));	vertices.push_back(tcu::Vec4(1.0f, 1.0f, 0.0f, 1.0f));

				vertices.push_back(tcu::Vec4(-0.5f,-1.0f, 0.0f, 1.0f));	vertices.push_back(tcu::Vec4(1.0f, 1.0f, 0.0f, 1.0f));	// line 1 reverse
				vertices.push_back(tcu::Vec4(-0.5f,-0.5f, 0.0f, 1.0f));	vertices.push_back(tcu::Vec4(0.0f, 0.0f, 1.0f, 1.0f));
				vertices.push_back(tcu::Vec4(-0.5f, 0.5f, 0.0f, 1.0f));	vertices.push_back(tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f));
				vertices.push_back(tcu::Vec4(-0.5f, 1.0f, 0.0f, 1.0f));	vertices.push_back(tcu::Vec4(0.0f, 1.0f, 0.0f, 1.0f));
				vertices.push_back(tcu::Vec4( 0.5f,-1.0f, 0.0f, 1.0f));	vertices.push_back(tcu::Vec4(1.0f, 1.0f, 0.0f, 1.0f));	// line 0 reverse
				vertices.push_back(tcu::Vec4( 0.5f,-0.5f, 0.0f, 1.0f));	vertices.push_back(tcu::Vec4(0.0f, 0.0f, 1.0f, 1.0f));
				vertices.push_back(tcu::Vec4( 0.5f, 0.5f, 0.0f, 1.0f));	vertices.push_back(tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f));
				vertices.push_back(tcu::Vec4( 0.5f, 1.0f, 0.0f, 1.0f));	vertices.push_back(tcu::Vec4(0.0f, 1.0f, 0.0f, 1.0f));
				break;
			case VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY:
				// Position												// Color
				vertices.push_back(tcu::Vec4(-1.0f,-0.5f, 0.0f, 1.0f));	vertices.push_back(tcu::Vec4(0.0f, 1.0f, 0.0f, 1.0f));	// line strip
				vertices.push_back(tcu::Vec4(-0.5f,-0.5f, 0.0f, 1.0f));	vertices.push_back(tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f));
				vertices.push_back(tcu::Vec4( 0.5f,-0.5f, 0.0f, 1.0f));	vertices.push_back(tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f));
				vertices.push_back(tcu::Vec4(-0.5f, 0.5f, 0.0f, 1.0f));	vertices.push_back(tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f));
				vertices.push_back(tcu::Vec4( 0.5f, 0.5f, 0.0f, 1.0f));	vertices.push_back(tcu::Vec4(0.0f, 0.0f, 1.0f, 1.0f));
				vertices.push_back(tcu::Vec4( 1.0f, 0.5f, 0.0f, 1.0f));	vertices.push_back(tcu::Vec4(1.0f, 1.0f, 0.0f, 1.0f));

				vertices.push_back(tcu::Vec4(-0.5f,-1.0f, 0.0f, 1.0f));	vertices.push_back(tcu::Vec4(1.0f, 1.0f, 0.0f, 1.0f));	// line strip reverse
				vertices.push_back(tcu::Vec4(-0.5f,-0.5f, 0.0f, 1.0f));	vertices.push_back(tcu::Vec4(0.0f, 0.0f, 1.0f, 1.0f));
				vertices.push_back(tcu::Vec4(-0.5f, 0.5f, 0.0f, 1.0f));	vertices.push_back(tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f));
				vertices.push_back(tcu::Vec4( 0.5f,-0.5f, 0.0f, 1.0f));	vertices.push_back(tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f));
				vertices.push_back(tcu::Vec4( 0.5f, 0.5f, 0.0f, 1.0f));	vertices.push_back(tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f));
				vertices.push_back(tcu::Vec4( 0.5f, 1.0f, 0.0f, 1.0f));	vertices.push_back(tcu::Vec4(0.0f, 1.0f, 0.0f, 1.0f));
				break;
			case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY:
				// Position												// Color
				vertices.push_back(tcu::Vec4(-1.0f, 1.0f, 0.0f, 1.0f));	vertices.push_back(tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f));	// triangle 0
				vertices.push_back(tcu::Vec4( 0.0f, 0.0f, 0.0f, 1.0f));	vertices.push_back(tcu::Vec4(1.0f, 1.0f, 1.0f, 1.0f));
				vertices.push_back(tcu::Vec4(-0.6f,-1.0f, 0.0f, 1.0f));	vertices.push_back(tcu::Vec4(0.0f, 1.0f, 0.0f, 1.0f));
				vertices.push_back(tcu::Vec4( 0.0f, 0.0f, 0.0f, 1.0f));	vertices.push_back(tcu::Vec4(1.0f, 1.0f, 1.0f, 1.0f));
				vertices.push_back(tcu::Vec4(-0.2f, 1.0f, 0.0f, 1.0f));	vertices.push_back(tcu::Vec4(0.0f, 0.0f, 1.0f, 1.0f));
				vertices.push_back(tcu::Vec4( 0.0f, 0.0f, 0.0f, 1.0f));	vertices.push_back(tcu::Vec4(1.0f, 1.0f, 1.0f, 1.0f));
				vertices.push_back(tcu::Vec4( 0.2f, 1.0f, 0.0f, 1.0f));	vertices.push_back(tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f));	// triangle 1
				vertices.push_back(tcu::Vec4( 0.0f, 0.0f, 0.0f, 1.0f));	vertices.push_back(tcu::Vec4(1.0f, 1.0f, 1.0f, 1.0f));
				vertices.push_back(tcu::Vec4( 0.6f,-1.0f, 0.0f, 1.0f));	vertices.push_back(tcu::Vec4(0.0f, 1.0f, 0.0f, 1.0f));
				vertices.push_back(tcu::Vec4( 0.0f, 0.0f, 0.0f, 1.0f));	vertices.push_back(tcu::Vec4(1.0f, 1.0f, 1.0f, 1.0f));
				vertices.push_back(tcu::Vec4( 1.0f, 1.0f, 0.0f, 1.0f));	vertices.push_back(tcu::Vec4(0.0f, 0.0f, 1.0f, 1.0f));
				vertices.push_back(tcu::Vec4( 0.0f, 0.0f, 0.0f, 1.0f));	vertices.push_back(tcu::Vec4(1.0f, 1.0f, 1.0f, 1.0f));

				vertices.push_back(tcu::Vec4(-1.0f,-1.0f, 0.0f, 1.0f));	vertices.push_back(tcu::Vec4(0.0f, 0.0f, 1.0f, 1.0f));	// triangle 1 reverse
				vertices.push_back(tcu::Vec4( 0.0f, 0.0f, 0.0f, 1.0f));	vertices.push_back(tcu::Vec4(1.0f, 1.0f, 1.0f, 1.0f));
				vertices.push_back(tcu::Vec4(-0.6f, 1.0f, 0.0f, 1.0f));	vertices.push_back(tcu::Vec4(0.0f, 1.0f, 0.0f, 1.0f));
				vertices.push_back(tcu::Vec4( 0.0f, 0.0f, 0.0f, 1.0f));	vertices.push_back(tcu::Vec4(1.0f, 1.0f, 1.0f, 1.0f));
				vertices.push_back(tcu::Vec4(-0.2f,-1.0f, 0.0f, 1.0f));	vertices.push_back(tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f));
				vertices.push_back(tcu::Vec4( 0.0f, 0.0f, 0.0f, 1.0f));	vertices.push_back(tcu::Vec4(1.0f, 1.0f, 1.0f, 1.0f));
				vertices.push_back(tcu::Vec4( 0.2f,-1.0f, 0.0f, 1.0f));	vertices.push_back(tcu::Vec4(0.0f, 0.0f, 1.0f, 1.0f));	// triangle 0 reverse
				vertices.push_back(tcu::Vec4( 0.0f, 0.0f, 0.0f, 1.0f));	vertices.push_back(tcu::Vec4(1.0f, 1.0f, 1.0f, 1.0f));
				vertices.push_back(tcu::Vec4( 0.6f, 1.0f, 0.0f, 1.0f));	vertices.push_back(tcu::Vec4(0.0f, 1.0f, 0.0f, 1.0f));
				vertices.push_back(tcu::Vec4( 0.0f, 0.0f, 0.0f, 1.0f));	vertices.push_back(tcu::Vec4(1.0f, 1.0f, 1.0f, 1.0f));
				vertices.push_back(tcu::Vec4( 1.0f,-1.0f, 0.0f, 1.0f));	vertices.push_back(tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f));
				vertices.push_back(tcu::Vec4( 0.0f, 0.0f, 0.0f, 1.0f));	vertices.push_back(tcu::Vec4(1.0f, 1.0f, 1.0f, 1.0f));
				break;
			case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY:
				// Position												// Color
				vertices.push_back(tcu::Vec4(-1.0f, 1.0f, 0.0f, 1.0f));	vertices.push_back(tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f));	// triangle strip
				vertices.push_back(tcu::Vec4( 0.0f, 0.0f, 0.0f, 1.0f));	vertices.push_back(tcu::Vec4(1.0f, 1.0f, 1.0f, 1.0f));
				vertices.push_back(tcu::Vec4(-0.5f,-1.0f, 0.0f, 1.0f));	vertices.push_back(tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f));
				vertices.push_back(tcu::Vec4( 0.0f, 0.0f, 0.0f, 1.0f));	vertices.push_back(tcu::Vec4(1.0f, 1.0f, 1.0f, 1.0f));
				vertices.push_back(tcu::Vec4( 0.0f, 1.0f, 0.0f, 1.0f));	vertices.push_back(tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f));
				vertices.push_back(tcu::Vec4( 0.0f, 0.0f, 0.0f, 1.0f));	vertices.push_back(tcu::Vec4(1.0f, 1.0f, 1.0f, 1.0f));
				vertices.push_back(tcu::Vec4( 0.5f,-1.0f, 0.0f, 1.0f));	vertices.push_back(tcu::Vec4(0.0f, 1.0f, 0.0f, 1.0f));
				vertices.push_back(tcu::Vec4( 0.0f, 0.0f, 0.0f, 1.0f));	vertices.push_back(tcu::Vec4(1.0f, 1.0f, 1.0f, 1.0f));
				vertices.push_back(tcu::Vec4( 1.0f, 1.0f, 0.0f, 1.0f));	vertices.push_back(tcu::Vec4(0.0f, 0.0f, 1.0f, 1.0f));
				vertices.push_back(tcu::Vec4( 0.0f, 0.0f, 0.0f, 1.0f));	vertices.push_back(tcu::Vec4(1.0f, 1.0f, 1.0f, 1.0f));

				vertices.push_back(tcu::Vec4(-1.0f,-1.0f, 0.0f, 1.0f));	vertices.push_back(tcu::Vec4(0.0f, 0.0f, 1.0f, 1.0f));	// triangle strip reverse
				vertices.push_back(tcu::Vec4( 0.0f, 0.0f, 0.0f, 1.0f));	vertices.push_back(tcu::Vec4(1.0f, 1.0f, 1.0f, 1.0f));
				vertices.push_back(tcu::Vec4(-0.5f, 1.0f, 0.0f, 1.0f));	vertices.push_back(tcu::Vec4(0.0f, 1.0f, 0.0f, 1.0f));
				vertices.push_back(tcu::Vec4( 0.0f, 0.0f, 0.0f, 1.0f));	vertices.push_back(tcu::Vec4(1.0f, 1.0f, 1.0f, 1.0f));
				vertices.push_back(tcu::Vec4( 0.0f,-1.0f, 0.0f, 1.0f));	vertices.push_back(tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f));
				vertices.push_back(tcu::Vec4( 0.0f, 0.0f, 0.0f, 1.0f));	vertices.push_back(tcu::Vec4(1.0f, 1.0f, 1.0f, 1.0f));
				vertices.push_back(tcu::Vec4( 0.5f, 1.0f, 0.0f, 1.0f));	vertices.push_back(tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f));
				vertices.push_back(tcu::Vec4( 0.0f, 0.0f, 0.0f, 1.0f));	vertices.push_back(tcu::Vec4(1.0f, 1.0f, 1.0f, 1.0f));
				vertices.push_back(tcu::Vec4( 1.0f,-1.0f, 0.0f, 1.0f));	vertices.push_back(tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f));
				vertices.push_back(tcu::Vec4( 0.0f, 0.0f, 0.0f, 1.0f));	vertices.push_back(tcu::Vec4(1.0f, 1.0f, 1.0f, 1.0f));
				break;
			default:
				DE_FATAL("Unknown primitive topology");
		}

		const size_t				bufferSize	= vertices.size() * sizeof(tcu::Vec4);

		const VkBufferCreateInfo	createInfo	=
		{
			VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,	// sType
			DE_NULL,								// pNext
			0u,										// flags
			(VkDeviceSize)bufferSize,				// size
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,		// usage
			VK_SHARING_MODE_EXCLUSIVE,				// sharingMode
			1u,										// queueFamilyCount
			&queueFamilyIndex						// pQueueFamilyIndices
		};

		vertexCount			= (deUint32)vertices.size() / 4;
		vertexBuffer		= createBuffer(vk, device, &createInfo);
		vertexBufferMemory	= allocator.allocate(getBufferMemoryRequirements(vk, device, *vertexBuffer), MemoryRequirement::HostVisible);
		VK_CHECK(vk.bindBufferMemory(device, *vertexBuffer, vertexBufferMemory->getMemory(), vertexBufferMemory->getOffset()));
		deMemcpy(vertexBufferMemory->getHostPtr(), &vertices[0], bufferSize);
		flushAlloc(vk, device, *vertexBufferMemory);
	}

	// Command buffer
	{
		const VkCommandPoolCreateInfo	commandPoolCreateInfo	=
		{
			VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,			// sType
			DE_NULL,											// pNext
			VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,	// flags
			m_context.getUniversalQueueFamilyIndex()			// queueFamilyIndex
		};

		commandPool		= createCommandPool(vk, device, &commandPoolCreateInfo);
		commandBuffer	= allocateCommandBuffer(vk, device, *commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
	}

	// Clear the color buffer to red and check the drawing doesn't add any
	// other colors from non-provoking vertices
	{
		const VkQueue					queue				= m_context.getUniversalQueue();
		const VkRect2D					renderArea			= makeRect2D(m_params.size.x(), m_params.size.y());
		const VkDeviceSize				vertexBufferOffset	= 0;
		const VkClearValue				clearValue			= makeClearValueColor(tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f));
		const VkPipelineStageFlags		srcStageMask		= VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		const VkPipelineStageFlags		dstStageMask		= VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;

		const VkImageSubresourceRange	subResourcerange	=
		{
			VK_IMAGE_ASPECT_COLOR_BIT,	// aspectMask
			0,							// baseMipLevel
			1,							// levelCount
			0,							// baseArrayLayer
			1							// layerCount
		};

		const VkImageMemoryBarrier		imageBarrier		=
		{
			VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,		// sType
			DE_NULL,									// pNext
			0,											// srcAccessMask
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,		// dstAccessMask
			VK_IMAGE_LAYOUT_UNDEFINED,					// oldLayout
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,	// newLayout
			VK_QUEUE_FAMILY_IGNORED,					// srcQueueFamilyIndex
			VK_QUEUE_FAMILY_IGNORED,					// destQueueFamilyIndex
			*image,										// image
			subResourcerange							// subresourceRange
		};

		// The first half of the vertex buffer is for PROVOKING_VERTEX_FIRST,
		// the second half for PROVOKING_VERTEX_LAST
		const deUint32					firstVertex			= m_params.provokingVertexMode == PROVOKING_VERTEX_LAST
															? vertexCount
															: 0u;

		beginCommandBuffer(vk, *commandBuffer, 0u);

		vk.cmdPipelineBarrier(*commandBuffer, srcStageMask, dstStageMask, 0, 0, DE_NULL, 0, DE_NULL, 1, &imageBarrier);

		beginRenderPass(vk, *commandBuffer, *renderPass, *framebuffer, renderArea, 1, &clearValue);
		vk.cmdBindVertexBuffers(*commandBuffer, 0, 1, &*vertexBuffer, &vertexBufferOffset);
		vk.cmdBindPipeline(*commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline);
		vk.cmdDraw(*commandBuffer, vertexCount, 1u, firstVertex, 0u);

		if (m_params.provokingVertexMode == PROVOKING_VERTEX_PER_PIPELINE)
		{
			vk.cmdBindPipeline(*commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *altPipeline);
			vk.cmdDraw(*commandBuffer, vertexCount, 1u, vertexCount, 0u);
		}

		endRenderPass(vk, *commandBuffer);
		copyImageToBuffer(vk, *commandBuffer, *image, *resultBuffer, tcu::IVec2(m_params.size.x(), m_params.size.y()));
		endCommandBuffer(vk, *commandBuffer);
		submitCommandsAndWait(vk, device, queue, commandBuffer.get());
		invalidateAlloc(vk, device, *resultBufferMemory);
	}

	// Verify result
	{
		const size_t				bufferSize			= textureFormat.getPixelSize() * m_params.size.x() * m_params.size.y();
		tcu::Surface				referenceSurface	(m_params.size.x(), m_params.size.y());
		tcu::ConstPixelBufferAccess	referenceAccess		= referenceSurface.getAccess();
		tcu::Surface				resultSurface		(m_params.size.x(), m_params.size.y());
		tcu::ConstPixelBufferAccess	resultAccess		(textureFormat,
														 tcu::IVec3(m_params.size.x(), m_params.size.y(), 1),
														 resultBufferMemory->getHostPtr());

		// Create reference
		for (deUint32 y = 0; y < m_params.size.y(); y++)
		for (deUint32 x = 0; x < m_params.size.x(); x++)
			referenceSurface.setPixel(x, y, tcu::RGBA::red());

		// Copy result
		tcu::copy(resultSurface.getAccess(), resultAccess);

		// Compare
		if (deMemCmp(referenceAccess.getDataPtr(), resultAccess.getDataPtr(), bufferSize) != 0)
		{
			m_context.getTestContext().getLog()	<< tcu::TestLog::ImageSet("Result of rendering", "Result of rendering")
												<< tcu::TestLog::Image("Result", "Result", resultSurface)
												<< tcu::TestLog::EndImageSet;
			return tcu::TestStatus::fail("Incorrect rendering");
		}
	}

	return tcu::TestStatus::pass("Solid red");
}

void createTests (tcu::TestCaseGroup* testGroup)
{
	tcu::TestContext&	testCtx	= testGroup->getTestContext();

	const struct
	{
		const char*			name;
		const char*			desc;
		ProvokingVertexMode	mode;
	} provokingVertexModes[] =
	{
		{ "default",		"Default provoking vertex convention",				PROVOKING_VERTEX_DEFAULT,		},
		{ "first",			"VK_PROVOKING_VERTEX_MODE_FIRST_VERTEX_EXT",		PROVOKING_VERTEX_FIRST,			},
		{ "last",			"VK_PROVOKING_VERTEX_MODE_LAST_VERTEX_EXT",			PROVOKING_VERTEX_LAST,			},
		{ "per_pipeline",	"Pipelines with different provokingVertexModes",	PROVOKING_VERTEX_PER_PIPELINE	}
	};

	const struct
	{
		std::string			name;
		VkPrimitiveTopology	type;
		bool				requiresGeometryShader;
	} topologies[] =
	{
		{ "line_list",						VK_PRIMITIVE_TOPOLOGY_LINE_LIST,						false	},
		{ "line_strip",						VK_PRIMITIVE_TOPOLOGY_LINE_STRIP,						false	},
		{ "triangle_list",					VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,					false	},
		{ "triangle_strip",					VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,					false	},
		{ "triangle_fan",					VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN,						false	},
		{ "line_list_with_adjacency",		VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY,			true	},
		{ "line_strip_with_adjacency",		VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY,		true	},
		{ "triangle_list_with_adjacency",	VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY,		true	},
		{ "triangle_strip_with_adjacency",	VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY,	true	}
	};

	for (deUint32 provokingVertexIdx = 0; provokingVertexIdx < DE_LENGTH_OF_ARRAY(provokingVertexModes); provokingVertexIdx++)
	{
		const char*					groupName	= provokingVertexModes[provokingVertexIdx].name;
		const char*					groupDesc	= provokingVertexModes[provokingVertexIdx].desc;
		tcu::TestCaseGroup* const	subGroup	= new tcu::TestCaseGroup(testCtx, groupName, groupDesc);

		for (deUint32 topologyIdx = 0; topologyIdx < DE_LENGTH_OF_ARRAY(topologies); topologyIdx++)
		{
			const std::string	caseName	= topologies[topologyIdx].name;
			const std::string	caseDesc	= "VK_PRIMITIVE_TOPOLOGY_" + de::toUpper(topologies[topologyIdx].name);

			const Params		params =
			{
				VK_FORMAT_R8G8B8A8_UNORM,						// format
				tcu::UVec2(32, 32),								// size
				topologies[topologyIdx].type,					// primitiveTopology
				topologies[topologyIdx].requiresGeometryShader,	// requireGeometryShader
				provokingVertexModes[provokingVertexIdx].mode	// provokingVertexMode
			};

			subGroup->addChild(new ProvokingVertexTestCase(testCtx, caseName, caseDesc, params));
		}

		testGroup->addChild(subGroup);
	}
}

}	// anonymous

tcu::TestCaseGroup* createProvokingVertexTests (tcu::TestContext& testCtx)
{
	return createTestGroup(testCtx,
						   "provoking_vertex",
						   "Tests for provoking vertex",
						   createTests);
}

}	// rasterization
}	// vkt

