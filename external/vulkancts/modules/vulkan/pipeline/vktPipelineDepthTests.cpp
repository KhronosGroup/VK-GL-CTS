/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2015 The Khronos Group Inc.
 * Copyright (c) 2015 Imagination Technologies Ltd.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and/or associated documentation files (the
 * "Materials"), to deal in the Materials without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Materials, and to
 * permit persons to whom the Materials are furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice(s) and this permission notice shall be included
 * in all copies or substantial portions of the Materials.
 *
 * The Materials are Confidential Information as defined by the
 * Khronos Membership Agreement until designated non-confidential by Khronos,
 * at which point this condition clause shall be removed.
 *
 * THE MATERIALS ARE PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * MATERIALS OR THE USE OR OTHER DEALINGS IN THE MATERIALS.
 *
 *//*!
 * \file
 * \brief Depth Tests
 *//*--------------------------------------------------------------------*/

#include "vktPipelineDepthTests.hpp"
#include "vktPipelineClearUtil.hpp"
#include "vktPipelineImageUtil.hpp"
#include "vktPipelineVertexUtil.hpp"
#include "vktPipelineReferenceRenderer.hpp"
#include "vktTestCase.hpp"
#include "vktTestCaseUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkPrograms.hpp"
#include "vkQueryUtil.hpp"
#include "vkRef.hpp"
#include "vkRefUtil.hpp"
#include "tcuImageCompare.hpp"
#include "deUniquePtr.hpp"
#include "deStringUtil.hpp"
#include "deMemory.h"

#include <sstream>
#include <vector>

namespace vkt
{
namespace pipeline
{

using namespace vk;

namespace
{

bool isSupportedDepthStencilFormat (const InstanceInterface& instanceInterface, VkPhysicalDevice device, VkFormat format)
{
	VkFormatProperties formatProps;

	VK_CHECK(instanceInterface.getPhysicalDeviceFormatProperties(device, format, &formatProps));

	return (formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0u;
}

tcu::TestStatus testSupportsDepthStencilFormat (Context& context, VkFormat format)
{
	DE_ASSERT(vk::isDepthStencilFormat(format));

	if (isSupportedDepthStencilFormat(context.getInstanceInterface(), context.getPhysicalDevice(), format))
		return tcu::TestStatus::pass("Format can be used in depth/stencil attachment");
	else
		return tcu::TestStatus::fail("Unsupported depth/stencil attachment format");
}

tcu::TestStatus testSupportsAtLeastOneDepthStencilFormat (Context& context, const std::vector<VkFormat> formats)
{
	std::ostringstream	supportedFormatsMsg;
	bool				pass					= false;

	DE_ASSERT(!formats.empty());

	for (size_t formatNdx = 0; formatNdx < formats.size(); formatNdx++)
	{
		const VkFormat format = formats[formatNdx];

		DE_ASSERT(vk::isDepthStencilFormat(format));

		if (isSupportedDepthStencilFormat(context.getInstanceInterface(), context.getPhysicalDevice(), format))
		{
			pass = true;
			supportedFormatsMsg << vk::getFormatName(format);

			if (formatNdx < formats.size() - 1)
				supportedFormatsMsg << ", ";
		}
	}

	if (pass)
		return tcu::TestStatus::pass(std::string("Supported depth/stencil formats: ") + supportedFormatsMsg.str());
	else
		return tcu::TestStatus::fail("All depth/stencil formats are unsupported");
}

class DepthTest : public vkt::TestCase
{
public:
	enum
	{
		QUAD_COUNT = 4
	};

	static const float					quadDepths[QUAD_COUNT];

										DepthTest				(tcu::TestContext&		testContext,
																 const std::string&		name,
																 const std::string&		description,
																 const VkFormat			depthFormat,
																 const VkCompareOp		depthCompareOps[QUAD_COUNT]);
	virtual								~DepthTest				(void);
	virtual void						initPrograms			(SourceCollections& programCollection) const;
	virtual TestInstance*				createInstance			(Context& context) const;

private:
	const VkFormat						m_depthFormat;
	VkCompareOp							m_depthCompareOps[QUAD_COUNT];
};

class DepthTestInstance : public vkt::TestInstance
{
public:
										DepthTestInstance		(Context& context, const VkFormat depthFormat, const VkCompareOp depthCompareOps[DepthTest::QUAD_COUNT]);
	virtual								~DepthTestInstance		(void);
	virtual tcu::TestStatus				iterate					(void);

private:
	tcu::TestStatus						verifyImage				(void);

private:
	VkCompareOp							m_depthCompareOps[DepthTest::QUAD_COUNT];
	const tcu::IVec2					m_renderSize;
	const VkFormat						m_colorFormat;
	const VkFormat						m_depthFormat;

	Move<VkImage>						m_colorImage;
	de::MovePtr<Allocation>				m_colorImageAlloc;
	Move<VkImage>						m_depthImage;
	de::MovePtr<Allocation>				m_depthImageAlloc;
	Move<VkImageView>					m_colorAttachmentView;
	Move<VkImageView>					m_depthAttachmentView;
	Move<VkRenderPass>					m_renderPass;
	Move<VkFramebuffer>					m_framebuffer;

	Move<VkShaderModule>				m_vertexShaderModule;
	Move<VkShaderModule>				m_fragmentShaderModule;
	Move<VkShader>						m_vertexShader;
	Move<VkShader>						m_fragmentShader;

	Move<VkBuffer>						m_vertexBuffer;
	std::vector<Vertex4RGBA>			m_vertices;
	de::MovePtr<Allocation>				m_vertexBufferAlloc;

	Move<VkPipelineLayout>				m_pipelineLayout;
	Move<VkPipeline>					m_graphicsPipelines[DepthTest::QUAD_COUNT];

	Move<VkCmdPool>						m_cmdPool;
	Move<VkCmdBuffer>					m_cmdBuffer;

	Move<VkFence>						m_fence;
};

const float DepthTest::quadDepths[QUAD_COUNT] =
{
	0.1f,
	0.0f,
	0.3f,
	0.2f
};

DepthTest::DepthTest (tcu::TestContext&		testContext,
					  const std::string&	name,
					  const std::string&	description,
					  const VkFormat		depthFormat,
					  const VkCompareOp		depthCompareOps[QUAD_COUNT])
	: vkt::TestCase	(testContext, name, description)
	, m_depthFormat	(depthFormat)
{
	deMemcpy(m_depthCompareOps, depthCompareOps, sizeof(VkCompareOp) * QUAD_COUNT);
}

DepthTest::~DepthTest (void)
{
}

TestInstance* DepthTest::createInstance (Context& context) const
{
	return new DepthTestInstance(context, m_depthFormat, m_depthCompareOps);
}

void DepthTest::initPrograms (SourceCollections& programCollection) const
{
	programCollection.glslSources.add("color_vert") << glu::VertexSource(
		"#version 310 es\n"
		"layout(location = 0) in vec4 position;\n"
		"layout(location = 1) in vec4 color;\n"
		"layout(location = 0) out highp vec4 vtxColor;\n"
		"void main (void)\n"
		"{\n"
		"	gl_Position = position;\n"
		"	vtxColor = color;\n"
		"}\n");

	programCollection.glslSources.add("color_frag") << glu::FragmentSource(
		"#version 310 es\n"
		"layout(location = 0) in highp vec4 vtxColor;\n"
		"layout(location = 0) out highp vec4 fragColor;\n"
		"void main (void)\n"
		"{\n"
		"	fragColor = vtxColor;\n"
		"}\n");
}

DepthTestInstance::DepthTestInstance (Context&				context,
									  const VkFormat		depthFormat,
									  const VkCompareOp		depthCompareOps[DepthTest::QUAD_COUNT])
	: vkt::TestInstance	(context)
	, m_renderSize		(32, 32)
	, m_colorFormat		(VK_FORMAT_R8G8B8A8_UNORM)
	, m_depthFormat		(depthFormat)
{
	const DeviceInterface&		vk					= context.getDeviceInterface();
	const VkDevice				vkDevice			= context.getDevice();
	const deUint32				queueFamilyIndex	= context.getUniversalQueueFamilyIndex();
	SimpleAllocator				memAlloc			(vk, vkDevice, getPhysicalDeviceMemoryProperties(context.getInstanceInterface(), context.getPhysicalDevice()));
	const VkChannelMapping		channelMappingRGBA	= { VK_CHANNEL_SWIZZLE_R, VK_CHANNEL_SWIZZLE_G, VK_CHANNEL_SWIZZLE_B, VK_CHANNEL_SWIZZLE_A };

	// Copy depth operators
	deMemcpy(m_depthCompareOps, depthCompareOps, sizeof(VkCompareOp) * DepthTest::QUAD_COUNT);

	// Create color image
	{
		const VkImageCreateInfo colorImageParams =
		{
			VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,										// VkStructureType		sType;
			DE_NULL,																	// const void*			pNext;
			VK_IMAGE_TYPE_2D,															// VkImageType			imageType;
			m_colorFormat,																// VkFormat				format;
			{ m_renderSize.x(), m_renderSize.y(), 1u },									// VkExtent3D			extent;
			1u,																			// deUint32				mipLevels;
			1u,																			// deUint32				arraySize;
			1u,																			// deUint32				samples;
			VK_IMAGE_TILING_OPTIMAL,													// VkImageTiling		tiling;
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SOURCE_BIT,	// VkImageUsageFlags	usage;
			0u,																			// VkImageCreateFlags	flags;
			VK_SHARING_MODE_EXCLUSIVE,													// VkSharingMode		sharingMode;
			1u,																			// deUint32				queueFamilyCount;
			&queueFamilyIndex,															// const deUint32*		pQueueFamilyIndices;
			VK_IMAGE_LAYOUT_UNDEFINED,													// VkImageLayout		initialLayout;
		};

		m_colorImage			= createImage(vk, vkDevice, &colorImageParams);

		// Allocate and bind color image memory
		m_colorImageAlloc		= memAlloc.allocate(getImageMemoryRequirements(vk, vkDevice, *m_colorImage), MemoryRequirement::Any);
		VK_CHECK(vk.bindImageMemory(vkDevice, *m_colorImage, m_colorImageAlloc->getMemory(), m_colorImageAlloc->getOffset()));
	}

	// Create depth image
	{
		// Check format support
		if (!isSupportedDepthStencilFormat(context.getInstanceInterface(), context.getPhysicalDevice(), m_depthFormat))
			throw tcu::NotSupportedError(std::string("Unsupported depth/stencil format: ") + getFormatName(m_depthFormat));

		const VkImageCreateInfo depthImageParams =
		{
			VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,			// VkStructureType		sType;
			DE_NULL,										// const void*			pNext;
			VK_IMAGE_TYPE_2D,								// VkImageType			imageType;
			m_depthFormat,									// VkFormat				format;
			{ m_renderSize.x(), m_renderSize.y(), 1u },		// VkExtent3D			extent;
			1u,												// deUint32				mipLevels;
			1u,												// deUint32				arraySize;
			1u,												// deUint32				samples;
			VK_IMAGE_TILING_OPTIMAL,						// VkImageTiling		tiling;
			VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,	// VkImageUsageFlags	usage;
			0u,												// VkImageCreateFlags	flags;
			VK_SHARING_MODE_EXCLUSIVE,						// VkSharingMode		sharingMode;
			1u,												// deUint32				queueFamilyCount;
			&queueFamilyIndex,								// const deUint32*		pQueueFamilyIndices;
			VK_IMAGE_LAYOUT_UNDEFINED,						// VkImageLayout		initialLayout;
		};

		m_depthImage = createImage(vk, vkDevice, &depthImageParams);

		// Allocate and bind depth image memory
		m_depthImageAlloc = memAlloc.allocate(getImageMemoryRequirements(vk, vkDevice, *m_depthImage), MemoryRequirement::Any);
		VK_CHECK(vk.bindImageMemory(vkDevice, *m_depthImage, m_depthImageAlloc->getMemory(), m_depthImageAlloc->getOffset()));
	}

	// Create color attachment view
	{
		const VkImageViewCreateInfo colorAttachmentViewParams =
		{
			VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,		// VkStructureType			sType;
			DE_NULL,										// const void*				pNext;
			*m_colorImage,									// VkImage					image;
			VK_IMAGE_VIEW_TYPE_2D,							// VkImageViewType			viewType;
			m_colorFormat,									// VkFormat					format;
			channelMappingRGBA,							 	// VkChannelMapping			channels;
			{ VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u },  // VkImageSubresourceRange	subresourceRange;
			0u												// VkImageViewCreateFlags	flags;
		};

		m_colorAttachmentView = createImageView(vk, vkDevice, &colorAttachmentViewParams);
	}

	// Create depth attachment view
	{
		const VkImageViewCreateInfo depthAttachmentViewParams =
		{
			VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,		// VkStructureType			sType;
			DE_NULL,										// const void*				pNext;
			*m_depthImage,									// VkImage					image;
			VK_IMAGE_VIEW_TYPE_2D,							// VkImageViewType			viewType;
			m_depthFormat,									// VkFormat					format;
			channelMappingRGBA,							 	// VkChannelMapping			channels;
			{ VK_IMAGE_ASPECT_DEPTH_BIT, 0u, 1u, 0u, 1u },  // VkImageSubresourceRange	subresourceRange;
			0u												// VkImageViewCreateFlags	flags;
		};

		m_depthAttachmentView = createImageView(vk, vkDevice, &depthAttachmentViewParams);
	}

	// Create render pass
	{
		const VkAttachmentDescription colorAttachmentDescription =
		{
			VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION,			// VkStructureType				sType;
			DE_NULL,											// const void*					pNext;
			m_colorFormat,										// VkFormat						format;
			1u,													// deUint32						samples;
			VK_ATTACHMENT_LOAD_OP_CLEAR,						// VkAttachmentLoadOp			loadOp;
			VK_ATTACHMENT_STORE_OP_STORE,						// VkAttachmentStoreOp			storeOp;
			VK_ATTACHMENT_LOAD_OP_DONT_CARE,					// VkAttachmentLoadOp			stencilLoadOp;
			VK_ATTACHMENT_STORE_OP_DONT_CARE,					// VkAttachmentStoreOp			stencilStoreOp;
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,			// VkImageLayout				initialLayout;
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,			// VkImageLayout				finalLayout;
			0u,													// VkAttachmentDescriptionFlags	flags;
		};

		const VkAttachmentDescription depthAttachmentDescription =
		{
			VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION,			// VkStructureType				sType;
			DE_NULL,											// const void*					pNext;
			m_depthFormat,										// VkFormat						format;
			1u,													// deUint32						samples;
			VK_ATTACHMENT_LOAD_OP_CLEAR,						// VkAttachmentLoadOp			loadOp;
			VK_ATTACHMENT_STORE_OP_DONT_CARE,					// VkAttachmentStoreOp			storeOp;
			VK_ATTACHMENT_LOAD_OP_DONT_CARE,					// VkAttachmentLoadOp			stencilLoadOp;
			VK_ATTACHMENT_STORE_OP_DONT_CARE,					// VkAttachmentStoreOp			stencilStoreOp;
			VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,	// VkImageLayout				initialLayout;
			VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,	// VkImageLayout				finalLayout;
			0u,													// VkAttachmentDescriptionFlags	flags;
		};

		const VkAttachmentDescription attachments[2] =
		{
			colorAttachmentDescription,
			depthAttachmentDescription
		};

		const VkAttachmentReference colorAttachmentReference =
		{
			0u,													// deUint32			attachment;
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL			// VkImageLayout	layout;
		};

		const VkAttachmentReference depthAttachmentReference =
		{
			1u,													// deUint32			attachment;
			VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL	// VkImageLayout	layout;
		};

		const VkSubpassDescription subpassDescription =
		{
			VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION,				// VkStructureType					sType;
			DE_NULL,											// const void*						pNext;
			VK_PIPELINE_BIND_POINT_GRAPHICS,					// VkPipelineBindPoint				pipelineBindPoint;
			0u,													// VkSubpassDescriptionFlags		flags;
			0u,													// deUint32							inputCount;
			DE_NULL,											// const VkAttachmentReference*		pInputAttachments;
			1u,													// deUint32							colorCount;
			&colorAttachmentReference,							// const VkAttachmentReference*		pColorAttachments;
			DE_NULL,											// const VkAttachmentReference*		pResolveAttachments;
			depthAttachmentReference,							// VkAttachmentReference			depthStencilAttachment;
			0u,													// deUint32							preserveCount;
			DE_NULL												// const VkAttachmentReference*		pPreserveAttachments;
		};

		const VkRenderPassCreateInfo renderPassParams =
		{
			VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,			// VkStructureType					sType;
			DE_NULL,											// const void*						pNext;
			2u,													// deUint32							attachmentCount;
			attachments,										// const VkAttachmentDescription*	pAttachments;
			1u,													// deUint32							subpassCount;
			&subpassDescription,								// const VkSubpassDescription*		pSubpasses;
			0u,													// deUint32							dependencyCount;
			DE_NULL												// const VkSubpassDependency*		pDependencies;
		};

		m_renderPass = createRenderPass(vk, vkDevice, &renderPassParams);
	}

	// Create framebuffer
	{
		const VkImageView attachmentBindInfos[2] =
		{
			*m_colorAttachmentView,
			*m_depthAttachmentView,
		};

		const VkFramebufferCreateInfo framebufferParams =
		{
			VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,			// VkStructureType				sType;
			DE_NULL,											// const void*					pNext;
			*m_renderPass,										// VkRenderPass					renderPass;
			2u,													// deUint32						attachmentCount;
			attachmentBindInfos,								// const VkImageView*			pAttachments;
			(deUint32)m_renderSize.x(),							// deUint32						width;
			(deUint32)m_renderSize.y(),							// deUint32						height;
			1u													// deUint32						layers;
		};

		m_framebuffer = createFramebuffer(vk, vkDevice, &framebufferParams);
	}

	// Create pipeline layout
	{
		const VkPipelineLayoutCreateInfo pipelineLayoutParams =
		{
			VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,		// VkStructureType				sType;
			DE_NULL,											// const void*					pNext;
			0u,													// deUint32						descriptorSetCount;
			DE_NULL,											// const VkDescriptorSetLayout*	pSetLayouts;
			0u,													// deUint32						pushConstantRangeCount;
			DE_NULL												// const VkPushConstantRange*	pPushConstantRanges;
		};

		m_pipelineLayout = createPipelineLayout(vk, vkDevice, &pipelineLayoutParams);
	}

	// Create shaders
	{
		m_vertexShaderModule	= createShaderModule(vk, vkDevice, m_context.getBinaryCollection().get("color_vert"), 0);
		m_fragmentShaderModule	= createShaderModule(vk, vkDevice, m_context.getBinaryCollection().get("color_frag"), 0);

		const VkShaderCreateInfo vertexShaderParams =
		{
			VK_STRUCTURE_TYPE_SHADER_CREATE_INFO,			// VkStructureType		sType;
			DE_NULL,										// const void*			pNext;
			*m_vertexShaderModule,							// VkShaderModule		module;
			"main",											// const char*			pName;
			0u,												// VkShaderCreateFlags	flags;
			VK_SHADER_STAGE_VERTEX,							// VkShaderStage		stage;
		};

		const VkShaderCreateInfo fragmentShaderParams =
		{
			VK_STRUCTURE_TYPE_SHADER_CREATE_INFO,			// VkStructureType		sType;
			DE_NULL,										// const void*			pNext;
			*m_fragmentShaderModule,						// VkShaderModule		module;
			"main",											// const char*			pName;
			0u,												// VkShaderCreateFlags	flags;
			VK_SHADER_STAGE_FRAGMENT,						// VkShaderStage		stage;
		};

		m_vertexShader		= createShader(vk, vkDevice, &vertexShaderParams);
		m_fragmentShader	= createShader(vk, vkDevice, &fragmentShaderParams);
	}

	// Create pipeline
	{
		const VkPipelineShaderStageCreateInfo shaderStageParams[2] =
		{
			{
				VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,		// VkStructureType				sType;
				DE_NULL,													// const void*					pNext;
				VK_SHADER_STAGE_VERTEX,										// VkShaderStage				stage;
				*m_vertexShader,											// VkShader						shader;
				DE_NULL														// const VkSpecializationInfo*	pSpecializationInfo;
			},
			{
				VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,		// VkStructureType				sType;
				DE_NULL,													// const void*					pNext;
				VK_SHADER_STAGE_FRAGMENT,									// VkShaderStage				stage;
				*m_fragmentShader,											// VkShader						shader;
				DE_NULL														// const VkSpecializationInfo*	pSpecializationInfo;
			}
		};

		const VkVertexInputBindingDescription vertexInputBindingDescription =
		{
			0u,									// deUint32					binding;
			sizeof(Vertex4RGBA),				// deUint32					strideInBytes;
			VK_VERTEX_INPUT_STEP_RATE_VERTEX	// VkVertexInputStepRate	stepRate;
		};

		const VkVertexInputAttributeDescription vertexInputAttributeDescriptions[2] =
		{
			{
				0u,									// deUint32	location;
				0u,									// deUint32	binding;
				VK_FORMAT_R32G32B32A32_SFLOAT,		// VkFormat	format;
				0u									// deUint32	offsetInBytes;
			},
			{
				1u,									// deUint32	location;
				0u,									// deUint32	binding;
				VK_FORMAT_R32G32B32A32_SFLOAT,		// VkFormat	format;
				DE_OFFSET_OF(Vertex4RGBA, color),	// deUint32	offsetInBytes;
			}
		};

		const VkPipelineVertexInputStateCreateInfo vertexInputStateParams =
		{
			VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,		// VkStructureType							sType;
			DE_NULL,														// const void*								pNext;
			1u,																// deUint32									bindingCount;
			&vertexInputBindingDescription,									// const VkVertexInputBindingDescription*	pVertexBindingDescriptions;
			2u,																// deUint32									attributeCount;
			vertexInputAttributeDescriptions								// const VkVertexInputAttributeDescription*	pVertexAttributeDescriptions;
		};

		const VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateParams =
		{
			VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,	// VkStructureType		sType;
			DE_NULL,														// const void*			pNext;
			VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,							// VkPrimitiveTopology	topology;
			false															// VkBool32				primitiveRestartEnable;
		};

		const VkViewport viewport =
		{
			0.0f,						// float	originX;
			0.0f,						// float	originY;
			(float)m_renderSize.x(),	// float	width;
			(float)m_renderSize.y(),	// float	height;
			0.0f,						// float	minDepth;
			1.0f						// float	maxDepth;
		};
		const VkRect2D scissor =
		{
			{ 0, 0 },												// VkOffset2D  offset;
			{ m_renderSize.x(), m_renderSize.y() }					// VkExtent2D  extent;
		};
		const VkPipelineViewportStateCreateInfo viewportStateParams =
		{
			VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,			// VkStructureType		sType;
			DE_NULL,														// const void*			pNext;
			1u,																// deUint32				viewportCount;
			&viewport,														// const VkViewport*	pViewports;
			1u,																// deUint32				scissorCount;
			&scissor														// const VkRect2D*		pScissors;
		};

		const VkPipelineRasterStateCreateInfo rasterStateParams =
		{
			VK_STRUCTURE_TYPE_PIPELINE_RASTER_STATE_CREATE_INFO,			// VkStructureType	sType;
			DE_NULL,														// const void*		pNext;
			false,															// VkBool32			depthClipEnable;
			false,															// VkBool32			rasterizerDiscardEnable;
			VK_FILL_MODE_SOLID,												// VkFillMode		fillMode;
			VK_CULL_MODE_NONE,												// VkCullMode		cullMode;
			VK_FRONT_FACE_CCW,												// VkFrontFace		frontFace;
			VK_FALSE,														// VkBool32			depthBiasEnable;
			0.0f,															// float			depthBias;
			0.0f,															// float			depthBiasClamp;
			0.0f,															// float			slopeScaledDepthBias;
			1.0f,															// float			lineWidth;
		};

		const VkPipelineColorBlendAttachmentState colorBlendAttachmentState =
		{
			false,																		// VkBool32			blendEnable;
			VK_BLEND_ONE,																// VkBlend			srcBlendColor;
			VK_BLEND_ZERO,																// VkBlend			destBlendColor;
			VK_BLEND_OP_ADD,															// VkBlendOp		blendOpColor;
			VK_BLEND_ONE,																// VkBlend			srcBlendAlpha;
			VK_BLEND_ZERO,																// VkBlend			destBlendAlpha;
			VK_BLEND_OP_ADD,															// VkBlendOp		blendOpAlpha;
			VK_CHANNEL_R_BIT | VK_CHANNEL_G_BIT | VK_CHANNEL_B_BIT | VK_CHANNEL_A_BIT	// VkChannelFlags	channelWriteMask;
		};

		const VkPipelineColorBlendStateCreateInfo colorBlendStateParams =
		{
			VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,	// VkStructureType								sType;
			DE_NULL,													// const void*									pNext;
			false,														// VkBool32										alphaToCoverageEnable;
			false,														// VkBool32										alphaToOneEnable;
			false,														// VkBool32										logicOpEnable;
			VK_LOGIC_OP_COPY,											// VkLogicOp									logicOp;
			1u,															// deUint32										attachmentCount;
			&colorBlendAttachmentState,									// const VkPipelineColorBlendAttachmentState*	pAttachments;
			{ 0.0f, 0.0f, 0.0f, 0.0f },									// float										blendConst[4];
		};

		const VkPipelineMultisampleStateCreateInfo	multisampleStateParams	=
		{
			VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,	// VkStructureType		sType;
			DE_NULL,													// const void*			pNext;
			1u,															// deUint32				rasterSamples;
			false,														// VkBool32				sampleShadingEnable;
			0.0f,														// float				minSampleShading;
			DE_NULL														// const VkSampleMask*	pSampleMask;
		};

		const VkPipelineDynamicStateCreateInfo	dynamicStateParams		=
		{
			VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,		// VkStructureType			sType;
			DE_NULL,													// const void*				pNext;
			0u,															// deUint32					dynamicStateCount;
			DE_NULL														// const VkDynamicState*	pDynamicStates;
		};

		VkPipelineDepthStencilStateCreateInfo depthStencilStateParams =
		{
			VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,	// VkStructureType	sType;
			DE_NULL,													// const void*		pNext;
			true,														// VkBool32			depthTestEnable;
			true,														// VkBool32			depthWriteEnable;
			VK_COMPARE_OP_LESS,											// VkCompareOp		depthCompareOp;
			false,														// VkBool32			depthBoundsTestEnable;
			false,														// VkBool32			stencilTestEnable;
			// VkStencilOpState	front;
			{
				VK_STENCIL_OP_KEEP,		// VkStencilOp	stencilFailOp;
				VK_STENCIL_OP_KEEP,		// VkStencilOp	stencilPassOp;
				VK_STENCIL_OP_KEEP,		// VkStencilOp	stencilDepthFailOp;
				VK_COMPARE_OP_NEVER,	// VkCompareOp	stencilCompareOp;
				0u,						// deUint32		stencilCompareMask;
				0u,						// deUint32		stencilWriteMask;
				0u,						// deUint32		stencilReference;
			},
			// VkStencilOpState	back;
			{
				VK_STENCIL_OP_KEEP,		// VkStencilOp	stencilFailOp;
				VK_STENCIL_OP_KEEP,		// VkStencilOp	stencilPassOp;
				VK_STENCIL_OP_KEEP,		// VkStencilOp	stencilDepthFailOp;
				VK_COMPARE_OP_NEVER,	// VkCompareOp	stencilCompareOp;
				0u,						// deUint32		stencilCompareMask;
				0u,						// deUint32		stencilWriteMask;
				0u,						// deUint32		stencilReference;
			},
			-1.0f,														// float			minDepthBounds;
			+1.0f,														// float			maxDepthBounds;
		};

		const VkGraphicsPipelineCreateInfo graphicsPipelineParams =
		{
			VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,	// VkStructureType									sType;
			DE_NULL,											// const void*										pNext;
			2u,													// deUint32											stageCount;
			shaderStageParams,									// const VkPipelineShaderStageCreateInfo*			pStages;
			&vertexInputStateParams,							// const VkPipelineVertexInputStateCreateInfo*		pVertexInputState;
			&inputAssemblyStateParams,							// const VkPipelineInputAssemblyStateCreateInfo*	pInputAssemblyState;
			DE_NULL,											// const VkPipelineTessellationStateCreateInfo*		pTessellationState;
			&viewportStateParams,								// const VkPipelineViewportStateCreateInfo*			pViewportState;
			&rasterStateParams,									// const VkPipelineRasterStateCreateInfo*			pRasterState;
			&multisampleStateParams,							// const VkPipelineMultisampleStateCreateInfo*		pMultisampleState;
			&depthStencilStateParams,							// const VkPipelineDepthStencilStateCreateInfo*		pDepthStencilState;
			&colorBlendStateParams,								// const VkPipelineColorBlendStateCreateInfo*		pColorBlendState;
			&dynamicStateParams,								// const VkPipelineDynamicStateCreateInfo*			pDynamicState;
			0u,													// VkPipelineCreateFlags							flags;
			*m_pipelineLayout,									// VkPipelineLayout									layout;
			*m_renderPass,										// VkRenderPass										renderPass;
			0u,													// deUint32											subpass;
			0u,													// VkPipeline										basePipelineHandle;
			0u													// deInt32											basePipelineIndex;
		};

		for (int quadNdx = 0; quadNdx < DepthTest::QUAD_COUNT; quadNdx++)
		{
			depthStencilStateParams.depthCompareOp	= depthCompareOps[quadNdx];
			m_graphicsPipelines[quadNdx]			= createGraphicsPipeline(vk, vkDevice, DE_NULL, &graphicsPipelineParams);
		}
	}

	// Create vertex buffer
	{
		const VkBufferCreateInfo vertexBufferParams =
		{
			VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,		// VkStructureType		sType;
			DE_NULL,									// const void*			pNext;
			1024u,										// VkDeviceSize			size;
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,			// VkBufferUsageFlags	usage;
			0u,											// VkBufferCreateFlags	flags;
			VK_SHARING_MODE_EXCLUSIVE,					// VkSharingMode		sharingMode;
			1u,											// deUint32				queueFamilyCount;
			&queueFamilyIndex							// const deUint32*		pQueueFamilyIndices;
		};

		m_vertices			= createOverlappingQuads();
		m_vertexBuffer		= createBuffer(vk, vkDevice, &vertexBufferParams);
		m_vertexBufferAlloc	= memAlloc.allocate(getBufferMemoryRequirements(vk, vkDevice, *m_vertexBuffer), MemoryRequirement::HostVisible);

		VK_CHECK(vk.bindBufferMemory(vkDevice, *m_vertexBuffer, m_vertexBufferAlloc->getMemory(), m_vertexBufferAlloc->getOffset()));

		// Adjust depths
		for (int quadNdx = 0; quadNdx < DepthTest::QUAD_COUNT; quadNdx++)
			for (int vertexNdx = 0; vertexNdx < 6; vertexNdx++)
				m_vertices[quadNdx * 6 + vertexNdx].position.z() = DepthTest::quadDepths[quadNdx];

		// Load vertices into vertex buffer
		deMemcpy(m_vertexBufferAlloc->getHostPtr(), m_vertices.data(), m_vertices.size() * sizeof(Vertex4RGBA));
		flushMappedMemoryRange(vk, vkDevice, m_vertexBufferAlloc->getMemory(), m_vertexBufferAlloc->getOffset(), vertexBufferParams.size);
	}

	// Create command pool
	{
		const VkCmdPoolCreateInfo cmdPoolParams =
		{
			VK_STRUCTURE_TYPE_CMD_POOL_CREATE_INFO,		// VkStructureType		sType;
			DE_NULL,									// const void*			pNext;
			queueFamilyIndex,							// deUint32				queueFamilyIndex;
			VK_CMD_POOL_CREATE_TRANSIENT_BIT			// VkCmdPoolCreateFlags	flags;
		};

		m_cmdPool = createCommandPool(vk, vkDevice, &cmdPoolParams);
	}

	// Create command buffer
	{
		const VkCmdBufferCreateInfo cmdBufferParams =
		{
			VK_STRUCTURE_TYPE_CMD_BUFFER_CREATE_INFO,	// VkStructureType			sType;
			DE_NULL,									// const void*				pNext;
			*m_cmdPool,									// VkCmdPool				cmdPool;
			VK_CMD_BUFFER_LEVEL_PRIMARY,				// VkCmdBufferLevel			level;
			0u											// VkCmdBufferCreateFlags	flags;
		};

		const VkCmdBufferBeginInfo cmdBufferBeginInfo =
		{
			VK_STRUCTURE_TYPE_CMD_BUFFER_BEGIN_INFO,	// VkStructureType			sType;
			DE_NULL,									// const void*				pNext;
			0u,											// VkCmdBufferOptimizeFlags	flags;
			DE_NULL,									// VkRenderPass				renderPass;
			0u,											// deUint32					subpass;
			DE_NULL										// VkFramebuffer			framebuffer;
		};

		const VkClearValue attachmentClearValues[2] =
		{
			defaultClearValue(m_colorFormat),
			defaultClearValue(m_depthFormat),
		};

		const VkRenderPassBeginInfo renderPassBeginInfo =
		{
			VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,				// VkStructureType		sType;
			DE_NULL,												// const void*			pNext;
			*m_renderPass,											// VkRenderPass			renderPass;
			*m_framebuffer,											// VkFramebuffer		framebuffer;
			{ { 0, 0 }, { m_renderSize.x(), m_renderSize.y() } },	// VkRect2D				renderArea;
			2,														// deUint32				clearValueCount;
			attachmentClearValues									// const VkClearValue*	pClearValues;
		};

		m_cmdBuffer = createCommandBuffer(vk, vkDevice, &cmdBufferParams);

		VK_CHECK(vk.beginCommandBuffer(*m_cmdBuffer, &cmdBufferBeginInfo));
		vk.cmdBeginRenderPass(*m_cmdBuffer, &renderPassBeginInfo, VK_RENDER_PASS_CONTENTS_INLINE);

		const VkDeviceSize		quadOffset		= (m_vertices.size() / DepthTest::QUAD_COUNT) * sizeof(Vertex4RGBA);

		for (int quadNdx = 0; quadNdx < DepthTest::QUAD_COUNT; quadNdx++)
		{
			VkDeviceSize vertexBufferOffset = quadOffset * quadNdx;

			vk.cmdBindPipeline(*m_cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_graphicsPipelines[quadNdx]);
			vk.cmdBindVertexBuffers(*m_cmdBuffer, 0, 1, &m_vertexBuffer.get(), &vertexBufferOffset);
			vk.cmdDraw(*m_cmdBuffer, (deUint32)(m_vertices.size() / DepthTest::QUAD_COUNT), 1, 0, 0);
		}

		vk.cmdEndRenderPass(*m_cmdBuffer);
		VK_CHECK(vk.endCommandBuffer(*m_cmdBuffer));
	}

	// Create fence
	{
		const VkFenceCreateInfo fenceParams =
		{
			VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,	// VkStructureType		sType;
			DE_NULL,								// const void*			pNext;
			0u										// VkFenceCreateFlags	flags;
		};

		m_fence = createFence(vk, vkDevice, &fenceParams);
	}
}

DepthTestInstance::~DepthTestInstance (void)
{
}

tcu::TestStatus DepthTestInstance::iterate (void)
{
	const DeviceInterface&		vk			= m_context.getDeviceInterface();
	const VkDevice				vkDevice	= m_context.getDevice();
	const VkQueue				queue		= m_context.getUniversalQueue();

	VK_CHECK(vk.resetFences(vkDevice, 1, &m_fence.get()));
	VK_CHECK(vk.queueSubmit(queue, 1, &m_cmdBuffer.get(), *m_fence));
	VK_CHECK(vk.waitForFences(vkDevice, 1, &m_fence.get(), true, ~(0ull) /* infinity*/));

	return verifyImage();
}

tcu::TestStatus DepthTestInstance::verifyImage (void)
{
	const tcu::TextureFormat	tcuColorFormat	= mapVkFormat(m_colorFormat);
	const tcu::TextureFormat	tcuDepthFormat	= mapVkFormat(m_depthFormat);
	const ColorVertexShader		vertexShader;
	const ColorFragmentShader	fragmentShader	(tcuColorFormat, tcuDepthFormat);
	const rr::Program			program			(&vertexShader, &fragmentShader);
	ReferenceRenderer			refRenderer		(m_renderSize.x(), m_renderSize.y(), 1, tcuColorFormat, tcuDepthFormat, &program);
	bool						compareOk		= false;

	// Render reference image
	{
		for (int quadNdx = 0; quadNdx < DepthTest::QUAD_COUNT; quadNdx++)
		{
			// Set depth state
			rr::RenderState renderState(refRenderer.getViewportState());
			renderState.fragOps.depthTestEnabled = true;
			renderState.fragOps.depthFunc = mapVkCompareOp(m_depthCompareOps[quadNdx]);

			refRenderer.draw(renderState,
							 rr::PRIMITIVETYPE_TRIANGLES,
							 std::vector<Vertex4RGBA>(m_vertices.begin() + quadNdx * 6,
													  m_vertices.begin() + (quadNdx + 1) * 6));
		}
	}

	// Compare result with reference image
	{
		const DeviceInterface&			vk					= m_context.getDeviceInterface();
		const VkDevice					vkDevice			= m_context.getDevice();
		const VkQueue					queue				= m_context.getUniversalQueue();
		const deUint32					queueFamilyIndex	= m_context.getUniversalQueueFamilyIndex();
		SimpleAllocator					allocator			(vk, vkDevice, getPhysicalDeviceMemoryProperties(m_context.getInstanceInterface(), m_context.getPhysicalDevice()));
		de::MovePtr<tcu::TextureLevel>	result				= readColorAttachment(vk, vkDevice, queue, queueFamilyIndex, allocator, *m_colorImage, m_colorFormat, m_renderSize);

		compareOk = tcu::intThresholdPositionDeviationCompare(m_context.getTestContext().getLog(),
															  "IntImageCompare",
															  "Image comparison",
															  refRenderer.getAccess(),
															  result->getAccess(),
															  tcu::UVec4(2, 2, 2, 2),
															  tcu::IVec3(1, 1, 0),
															  true,
															  tcu::COMPARE_LOG_RESULT);
	}

	if (compareOk)
		return tcu::TestStatus::pass("Result image matches reference");
	else
		return tcu::TestStatus::fail("Image mismatch");
}

std::string getFormatCaseName (const VkFormat format)
{
	const std::string	fullName	= getFormatName(format);

	DE_ASSERT(de::beginsWith(fullName, "VK_FORMAT_"));

	return de::toLower(fullName.substr(10));
}

std::string	getCompareOpsName (const VkCompareOp quadDepthOps[DepthTest::QUAD_COUNT])
{
	std::ostringstream name;

	for (int quadNdx = 0; quadNdx < DepthTest::QUAD_COUNT; quadNdx++)
	{
		const std::string	fullOpName	= getCompareOpName(quadDepthOps[quadNdx]);

		DE_ASSERT(de::beginsWith(fullOpName, "VK_COMPARE_OP_"));

		name << de::toLower(fullOpName.substr(14));

		if (quadNdx < DepthTest::QUAD_COUNT - 1)
			name << "_";
	}

	return name.str();
}

std::string	getCompareOpsDescription (const VkCompareOp quadDepthOps[DepthTest::QUAD_COUNT])
{
	std::ostringstream desc;
	desc << "Draws " << DepthTest::QUAD_COUNT << " quads with depth compare ops: ";

	for (int quadNdx = 0; quadNdx < DepthTest::QUAD_COUNT; quadNdx++)
	{
		desc << getCompareOpName(quadDepthOps[quadNdx]) << " at depth " << DepthTest::quadDepths[quadNdx];

		if (quadNdx < DepthTest::QUAD_COUNT - 1)
			desc << ", ";
	}
	return desc.str();
}


} // anonymous

tcu::TestCaseGroup* createDepthTests (tcu::TestContext& testCtx)
{
	const VkFormat depthFormats[] =
	{
		VK_FORMAT_D16_UNORM,
		VK_FORMAT_D24_UNORM_X8,
		VK_FORMAT_D32_SFLOAT,
		VK_FORMAT_D16_UNORM_S8_UINT,
		VK_FORMAT_D24_UNORM_S8_UINT,
		VK_FORMAT_D32_SFLOAT_S8_UINT
	};

	// Each entry configures the depth compare operators of QUAD_COUNT quads.
	// All entries cover pair-wise combinations of compare operators.
	const VkCompareOp depthOps[][DepthTest::QUAD_COUNT] =
	{
		{ VK_COMPARE_OP_NOT_EQUAL,		VK_COMPARE_OP_NOT_EQUAL,		VK_COMPARE_OP_NOT_EQUAL,		VK_COMPARE_OP_NOT_EQUAL },
		{ VK_COMPARE_OP_NOT_EQUAL,		VK_COMPARE_OP_EQUAL,			VK_COMPARE_OP_EQUAL,			VK_COMPARE_OP_GREATER },
		{ VK_COMPARE_OP_NOT_EQUAL,		VK_COMPARE_OP_GREATER,			VK_COMPARE_OP_GREATER,			VK_COMPARE_OP_LESS_EQUAL },
		{ VK_COMPARE_OP_NOT_EQUAL,		VK_COMPARE_OP_GREATER_EQUAL,	VK_COMPARE_OP_GREATER_EQUAL,	VK_COMPARE_OP_GREATER_EQUAL },
		{ VK_COMPARE_OP_NOT_EQUAL,		VK_COMPARE_OP_LESS_EQUAL,		VK_COMPARE_OP_LESS_EQUAL,		VK_COMPARE_OP_ALWAYS },
		{ VK_COMPARE_OP_NOT_EQUAL,		VK_COMPARE_OP_LESS,				VK_COMPARE_OP_LESS,				VK_COMPARE_OP_LESS },
		{ VK_COMPARE_OP_NOT_EQUAL,		VK_COMPARE_OP_NEVER,			VK_COMPARE_OP_NEVER,			VK_COMPARE_OP_NEVER },
		{ VK_COMPARE_OP_EQUAL,			VK_COMPARE_OP_NOT_EQUAL,		VK_COMPARE_OP_EQUAL,			VK_COMPARE_OP_EQUAL },
		{ VK_COMPARE_OP_EQUAL,			VK_COMPARE_OP_EQUAL,			VK_COMPARE_OP_NOT_EQUAL,		VK_COMPARE_OP_LESS },
		{ VK_COMPARE_OP_EQUAL,			VK_COMPARE_OP_GREATER,			VK_COMPARE_OP_GREATER_EQUAL,	VK_COMPARE_OP_NOT_EQUAL },
		{ VK_COMPARE_OP_EQUAL,			VK_COMPARE_OP_GREATER_EQUAL,	VK_COMPARE_OP_GREATER,			VK_COMPARE_OP_GREATER },
		{ VK_COMPARE_OP_EQUAL,			VK_COMPARE_OP_LESS_EQUAL,		VK_COMPARE_OP_LESS,				VK_COMPARE_OP_LESS_EQUAL },
		{ VK_COMPARE_OP_NOT_EQUAL,		VK_COMPARE_OP_ALWAYS,			VK_COMPARE_OP_ALWAYS,			VK_COMPARE_OP_EQUAL },
		{ VK_COMPARE_OP_EQUAL,			VK_COMPARE_OP_LESS,				VK_COMPARE_OP_NEVER,			VK_COMPARE_OP_ALWAYS },
		{ VK_COMPARE_OP_EQUAL,			VK_COMPARE_OP_NEVER,			VK_COMPARE_OP_LESS_EQUAL,		VK_COMPARE_OP_GREATER_EQUAL },
		{ VK_COMPARE_OP_GREATER,		VK_COMPARE_OP_NOT_EQUAL,		VK_COMPARE_OP_GREATER,			VK_COMPARE_OP_LESS },
		{ VK_COMPARE_OP_GREATER,		VK_COMPARE_OP_EQUAL,			VK_COMPARE_OP_GREATER_EQUAL,	VK_COMPARE_OP_ALWAYS },
		{ VK_COMPARE_OP_GREATER,		VK_COMPARE_OP_GREATER,			VK_COMPARE_OP_NOT_EQUAL,		VK_COMPARE_OP_GREATER },
		{ VK_COMPARE_OP_GREATER,		VK_COMPARE_OP_GREATER_EQUAL,	VK_COMPARE_OP_LESS_EQUAL,		VK_COMPARE_OP_NOT_EQUAL },
		{ VK_COMPARE_OP_GREATER,		VK_COMPARE_OP_LESS_EQUAL,		VK_COMPARE_OP_NEVER,			VK_COMPARE_OP_GREATER_EQUAL },
		{ VK_COMPARE_OP_GREATER,		VK_COMPARE_OP_LESS,				VK_COMPARE_OP_EQUAL,			VK_COMPARE_OP_NEVER },
		{ VK_COMPARE_OP_GREATER_EQUAL,	VK_COMPARE_OP_NOT_EQUAL,		VK_COMPARE_OP_GREATER_EQUAL,	VK_COMPARE_OP_GREATER },
		{ VK_COMPARE_OP_GREATER,		VK_COMPARE_OP_NEVER,			VK_COMPARE_OP_ALWAYS,			VK_COMPARE_OP_LESS_EQUAL },
		{ VK_COMPARE_OP_GREATER_EQUAL,	VK_COMPARE_OP_EQUAL,			VK_COMPARE_OP_GREATER,			VK_COMPARE_OP_NOT_EQUAL },
		{ VK_COMPARE_OP_GREATER_EQUAL,	VK_COMPARE_OP_GREATER,			VK_COMPARE_OP_EQUAL,			VK_COMPARE_OP_GREATER_EQUAL },
		{ VK_COMPARE_OP_GREATER_EQUAL,	VK_COMPARE_OP_GREATER_EQUAL,	VK_COMPARE_OP_NOT_EQUAL,		VK_COMPARE_OP_LESS_EQUAL },
		{ VK_COMPARE_OP_GREATER_EQUAL,	VK_COMPARE_OP_LESS_EQUAL,		VK_COMPARE_OP_ALWAYS,			VK_COMPARE_OP_LESS },
		{ VK_COMPARE_OP_GREATER_EQUAL,	VK_COMPARE_OP_LESS,				VK_COMPARE_OP_LESS_EQUAL,		VK_COMPARE_OP_EQUAL },
		{ VK_COMPARE_OP_GREATER_EQUAL,	VK_COMPARE_OP_ALWAYS,			VK_COMPARE_OP_LESS,				VK_COMPARE_OP_NEVER },
		{ VK_COMPARE_OP_LESS_EQUAL,		VK_COMPARE_OP_NOT_EQUAL,		VK_COMPARE_OP_LESS_EQUAL,		VK_COMPARE_OP_LESS_EQUAL },
		{ VK_COMPARE_OP_LESS_EQUAL,		VK_COMPARE_OP_EQUAL,			VK_COMPARE_OP_LESS,				VK_COMPARE_OP_EQUAL },
		{ VK_COMPARE_OP_LESS_EQUAL,		VK_COMPARE_OP_GREATER,			VK_COMPARE_OP_NEVER,			VK_COMPARE_OP_LESS },
		{ VK_COMPARE_OP_LESS_EQUAL,		VK_COMPARE_OP_GREATER_EQUAL,	VK_COMPARE_OP_EQUAL,			VK_COMPARE_OP_ALWAYS },
		{ VK_COMPARE_OP_LESS_EQUAL,		VK_COMPARE_OP_LESS,				VK_COMPARE_OP_NOT_EQUAL,		VK_COMPARE_OP_GREATER_EQUAL },
		{ VK_COMPARE_OP_LESS_EQUAL,		VK_COMPARE_OP_LESS_EQUAL,		VK_COMPARE_OP_GREATER_EQUAL,	VK_COMPARE_OP_NEVER },
		{ VK_COMPARE_OP_LESS,			VK_COMPARE_OP_NOT_EQUAL,		VK_COMPARE_OP_LESS,				VK_COMPARE_OP_GREATER_EQUAL },
		{ VK_COMPARE_OP_LESS,			VK_COMPARE_OP_EQUAL,			VK_COMPARE_OP_NEVER,			VK_COMPARE_OP_LESS_EQUAL },
		{ VK_COMPARE_OP_LESS,			VK_COMPARE_OP_GREATER,			VK_COMPARE_OP_LESS_EQUAL,		VK_COMPARE_OP_NEVER },
		{ VK_COMPARE_OP_LESS,			VK_COMPARE_OP_LESS_EQUAL,		VK_COMPARE_OP_GREATER,			VK_COMPARE_OP_EQUAL },
		{ VK_COMPARE_OP_LESS,			VK_COMPARE_OP_LESS,				VK_COMPARE_OP_ALWAYS,			VK_COMPARE_OP_NOT_EQUAL },
		{ VK_COMPARE_OP_LESS,			VK_COMPARE_OP_NEVER,			VK_COMPARE_OP_NOT_EQUAL,		VK_COMPARE_OP_ALWAYS },
		{ VK_COMPARE_OP_NEVER,			VK_COMPARE_OP_NOT_EQUAL,		VK_COMPARE_OP_ALWAYS,			VK_COMPARE_OP_ALWAYS },
		{ VK_COMPARE_OP_LESS,			VK_COMPARE_OP_ALWAYS,			VK_COMPARE_OP_GREATER_EQUAL,	VK_COMPARE_OP_LESS },
		{ VK_COMPARE_OP_NEVER,			VK_COMPARE_OP_GREATER_EQUAL,	VK_COMPARE_OP_NEVER,			VK_COMPARE_OP_EQUAL },
		{ VK_COMPARE_OP_NEVER,			VK_COMPARE_OP_NEVER,			VK_COMPARE_OP_LESS,				VK_COMPARE_OP_GREATER },
		{ VK_COMPARE_OP_NEVER,			VK_COMPARE_OP_LESS_EQUAL,		VK_COMPARE_OP_EQUAL,			VK_COMPARE_OP_NOT_EQUAL },
		{ VK_COMPARE_OP_NEVER,			VK_COMPARE_OP_LESS,				VK_COMPARE_OP_GREATER_EQUAL,	VK_COMPARE_OP_LESS_EQUAL },
		{ VK_COMPARE_OP_NEVER,			VK_COMPARE_OP_ALWAYS,			VK_COMPARE_OP_GREATER,			VK_COMPARE_OP_GREATER_EQUAL },
		{ VK_COMPARE_OP_ALWAYS,			VK_COMPARE_OP_EQUAL,			VK_COMPARE_OP_ALWAYS,			VK_COMPARE_OP_NEVER },
		{ VK_COMPARE_OP_ALWAYS,			VK_COMPARE_OP_NEVER,			VK_COMPARE_OP_EQUAL,			VK_COMPARE_OP_LESS },
		{ VK_COMPARE_OP_ALWAYS,			VK_COMPARE_OP_GREATER,			VK_COMPARE_OP_LESS,				VK_COMPARE_OP_ALWAYS },
		{ VK_COMPARE_OP_ALWAYS,			VK_COMPARE_OP_ALWAYS,			VK_COMPARE_OP_NEVER,			VK_COMPARE_OP_GREATER },
		{ VK_COMPARE_OP_ALWAYS,			VK_COMPARE_OP_LESS_EQUAL,		VK_COMPARE_OP_NOT_EQUAL,		VK_COMPARE_OP_EQUAL },
		{ VK_COMPARE_OP_LESS_EQUAL,		VK_COMPARE_OP_NEVER,			VK_COMPARE_OP_GREATER,			VK_COMPARE_OP_NOT_EQUAL },
		{ VK_COMPARE_OP_NEVER,			VK_COMPARE_OP_EQUAL,			VK_COMPARE_OP_LESS_EQUAL,		VK_COMPARE_OP_LESS },
		{ VK_COMPARE_OP_EQUAL,			VK_COMPARE_OP_GREATER_EQUAL,	VK_COMPARE_OP_ALWAYS,			VK_COMPARE_OP_NEVER },
		{ VK_COMPARE_OP_GREATER,		VK_COMPARE_OP_ALWAYS,			VK_COMPARE_OP_LESS,				VK_COMPARE_OP_NOT_EQUAL },
		{ VK_COMPARE_OP_GREATER,		VK_COMPARE_OP_NEVER,			VK_COMPARE_OP_GREATER_EQUAL,	VK_COMPARE_OP_EQUAL },
		{ VK_COMPARE_OP_EQUAL,			VK_COMPARE_OP_ALWAYS,			VK_COMPARE_OP_EQUAL,			VK_COMPARE_OP_LESS_EQUAL },
		{ VK_COMPARE_OP_LESS_EQUAL,		VK_COMPARE_OP_GREATER,			VK_COMPARE_OP_ALWAYS,			VK_COMPARE_OP_GREATER },
		{ VK_COMPARE_OP_NEVER,			VK_COMPARE_OP_NOT_EQUAL,		VK_COMPARE_OP_NOT_EQUAL,		VK_COMPARE_OP_NEVER },
		{ VK_COMPARE_OP_ALWAYS,			VK_COMPARE_OP_LESS,				VK_COMPARE_OP_GREATER,			VK_COMPARE_OP_GREATER },
		{ VK_COMPARE_OP_ALWAYS,			VK_COMPARE_OP_NOT_EQUAL,		VK_COMPARE_OP_NEVER,			VK_COMPARE_OP_NOT_EQUAL },
		{ VK_COMPARE_OP_GREATER_EQUAL,	VK_COMPARE_OP_ALWAYS,			VK_COMPARE_OP_NOT_EQUAL,		VK_COMPARE_OP_ALWAYS },
		{ VK_COMPARE_OP_LESS_EQUAL,		VK_COMPARE_OP_ALWAYS,			VK_COMPARE_OP_LESS_EQUAL,		VK_COMPARE_OP_GREATER },
		{ VK_COMPARE_OP_LESS,			VK_COMPARE_OP_GREATER_EQUAL,	VK_COMPARE_OP_LESS,				VK_COMPARE_OP_GREATER },
		{ VK_COMPARE_OP_ALWAYS,			VK_COMPARE_OP_EQUAL,			VK_COMPARE_OP_LESS_EQUAL,		VK_COMPARE_OP_GREATER_EQUAL },
		{ VK_COMPARE_OP_ALWAYS,			VK_COMPARE_OP_GREATER_EQUAL,	VK_COMPARE_OP_GREATER_EQUAL,	VK_COMPARE_OP_LESS_EQUAL },
		{ VK_COMPARE_OP_GREATER_EQUAL,	VK_COMPARE_OP_GREATER_EQUAL,	VK_COMPARE_OP_NEVER,			VK_COMPARE_OP_LESS },
		{ VK_COMPARE_OP_GREATER_EQUAL,	VK_COMPARE_OP_NEVER,			VK_COMPARE_OP_GREATER,			VK_COMPARE_OP_NEVER },
		{ VK_COMPARE_OP_LESS,			VK_COMPARE_OP_GREATER,			VK_COMPARE_OP_EQUAL,			VK_COMPARE_OP_EQUAL },
		{ VK_COMPARE_OP_NEVER,			VK_COMPARE_OP_GREATER,			VK_COMPARE_OP_ALWAYS,			VK_COMPARE_OP_GREATER_EQUAL },
		{ VK_COMPARE_OP_NOT_EQUAL,		VK_COMPARE_OP_NOT_EQUAL,		VK_COMPARE_OP_GREATER,			VK_COMPARE_OP_ALWAYS },
		{ VK_COMPARE_OP_NOT_EQUAL,		VK_COMPARE_OP_LESS_EQUAL,		VK_COMPARE_OP_NOT_EQUAL,		VK_COMPARE_OP_GREATER }
	};

	de::MovePtr<tcu::TestCaseGroup> depthTests (new tcu::TestCaseGroup(testCtx, "depth", "Depth tests"));

	// Tests for format features
	{
		de::MovePtr<tcu::TestCaseGroup> formatFeaturesTests (new tcu::TestCaseGroup(testCtx, "format_features", "Checks depth format features"));

		// Formats that must be supported in all implementations
		addFunctionCase(formatFeaturesTests.get(),
						"support_d16_unorm",
						"Tests if VK_FORMAT_D16_UNORM is supported as depth/stencil attachment format",
						testSupportsDepthStencilFormat,
						VK_FORMAT_D16_UNORM);

		// Sets where at least one of the formats must be supported
		const VkFormat	depthOnlyFormats[]		= { VK_FORMAT_D24_UNORM_X8, VK_FORMAT_D32_SFLOAT };
		const VkFormat	depthStencilFormats[]	= { VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D32_SFLOAT_S8_UINT };

		addFunctionCase(formatFeaturesTests.get(),
						"support_d24_unorm_or_d32_sfloat",
						"Tests if any of VK_FORMAT_D24_UNORM_X8 or VK_FORMAT_D32_SFLOAT are supported as depth/stencil attachment format",
						testSupportsAtLeastOneDepthStencilFormat,
						std::vector<VkFormat>(depthOnlyFormats, depthOnlyFormats + DE_LENGTH_OF_ARRAY(depthOnlyFormats)));

		addFunctionCase(formatFeaturesTests.get(),
						"support_d24_unorm_s8_uint_or_d32_sfloat_s8_uint",
						"Tests if any of VK_FORMAT_D24_UNORM_S8_UINT or VK_FORMAT_D32_SFLOAT_S8_UINT are supported as depth/stencil attachment format",
						testSupportsAtLeastOneDepthStencilFormat,
						std::vector<VkFormat>(depthStencilFormats, depthStencilFormats + DE_LENGTH_OF_ARRAY(depthStencilFormats)));

		depthTests->addChild(formatFeaturesTests.release());
	}

	// Tests for format and compare operators
	{
		de::MovePtr<tcu::TestCaseGroup> formatTests (new tcu::TestCaseGroup(testCtx, "format", "Uses different depth formats"));

		for (size_t formatNdx = 0; formatNdx < DE_LENGTH_OF_ARRAY(depthFormats); formatNdx++)
		{
			de::MovePtr<tcu::TestCaseGroup>	formatTest		(new tcu::TestCaseGroup(testCtx,
																					getFormatCaseName(depthFormats[formatNdx]).c_str(),
																					(std::string("Uses format ") + getFormatName(depthFormats[formatNdx])).c_str()));
			de::MovePtr<tcu::TestCaseGroup>	compareOpsTests	(new tcu::TestCaseGroup(testCtx, "compare_ops", "Combines depth compare operators"));

			for (size_t opsNdx = 0; opsNdx < DE_LENGTH_OF_ARRAY(depthOps); opsNdx++)
			{
				compareOpsTests->addChild(new DepthTest(testCtx,
														getCompareOpsName(depthOps[opsNdx]),
														getCompareOpsDescription(depthOps[opsNdx]),
														depthFormats[formatNdx],
														depthOps[opsNdx]));
			}
			formatTest->addChild(compareOpsTests.release());
			formatTests->addChild(formatTest.release());
		}
		depthTests->addChild(formatTests.release());
	}

	return depthTests.release();
}

} // pipeline
} // vkt
