/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2015 The Khronos Group Inc.
 * Copyright (c) 2015 Imagination Technologies Ltd.
 * Copyright (c) 2023 LunarG, Inc.
 * Copyright (c) 2023 Nintendo
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
#include "vkTypeUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"
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

enum class DepthClipControlCase
{
	DISABLED			= 0,	// No depth clip control.
	NORMAL				= 1,	// Depth clip control with static viewport.
	NORMAL_W			= 2,	// Depth clip control with static viewport and .w different from 1.0f
	BEFORE_STATIC		= 3,	// Set dynamic viewport state, then bind a static pipeline.
	BEFORE_DYNAMIC		= 4,	// Set dynamic viewport state, bind dynamic pipeline.
	BEFORE_TWO_DYNAMICS	= 5,	// Set dynamic viewport state, bind dynamic pipeline with [0,1] view volume, then bind dynamic pipeline with [-1,1] view volume.
	AFTER_DYNAMIC		= 6,	// Bind dynamic pipeline, then set dynamic viewport state.
};

bool isSupportedDepthStencilFormat (const InstanceInterface& instanceInterface, VkPhysicalDevice device, VkFormat format)
{
	VkFormatProperties formatProps;

	instanceInterface.getPhysicalDeviceFormatProperties(device, format, &formatProps);

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
	static const float					quadDepthsMinusOneToOne[QUAD_COUNT];
	static const float					quadWs[QUAD_COUNT];

										DepthTest				(tcu::TestContext&					testContext,
																 const std::string&					name,
																 const std::string&					description,
																 const PipelineConstructionType		pipelineConstructionType,
																 const VkFormat						depthFormat,
																 const VkCompareOp					depthCompareOps[QUAD_COUNT],
																 const bool							separateDepthStencilLayouts,
																 const VkPrimitiveTopology			primitiveTopology				= VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
																 const bool							depthBoundsTestEnable			= false,
																 const float						depthBoundsMin					= 0.0f,
																 const float						depthBoundsMax					= 1.0f,
																 const bool							depthTestEnable					= true,
																 const bool							stencilTestEnable				= false,
																 const bool							colorAttachmentEnable			= true,
																 const bool							hostVisible						= false,
																 const tcu::UVec2					renderSize						= tcu::UVec2(32, 32),
																 const DepthClipControlCase			depthClipControl				= DepthClipControlCase::DISABLED);
	virtual								~DepthTest				(void);
	virtual void						initPrograms			(SourceCollections& programCollection) const;
	virtual void						checkSupport			(Context& context) const;
	virtual TestInstance*				createInstance			(Context& context) const;

private:
	const PipelineConstructionType		m_pipelineConstructionType;
	const VkFormat						m_depthFormat;
	const bool							m_separateDepthStencilLayouts;
	VkPrimitiveTopology					m_primitiveTopology;
	const bool							m_depthBoundsTestEnable;
	const float							m_depthBoundsMin;
	const float							m_depthBoundsMax;
	const bool							m_depthTestEnable;
	const bool							m_stencilTestEnable;
	const bool							m_colorAttachmentEnable;
	const bool							m_hostVisible;
	const tcu::UVec2					m_renderSize;
	const DepthClipControlCase			m_depthClipControl;
	VkCompareOp							m_depthCompareOps[QUAD_COUNT];
};

class DepthTestInstance : public vkt::TestInstance
{
public:
										DepthTestInstance		(Context&							context,
																 const PipelineConstructionType		pipelineConstructionType,
																 const VkFormat						depthFormat,
																 const VkCompareOp					depthCompareOps[DepthTest::QUAD_COUNT],
																 const bool							separateDepthStencilLayouts,
																 const VkPrimitiveTopology			primitiveTopology,
																 const bool							depthBoundsTestEnable,
																 const float						depthBoundsMin,
																 const float						depthBoundsMax,
																 const bool							depthTestEnable,
																 const bool							stencilTestEnable,
																 const bool							colorAttachmentEnable,
																 const bool							hostVisible,
																 const tcu::UVec2					renderSize,
																 const DepthClipControlCase			depthClipControl);

	virtual								~DepthTestInstance		(void);
	virtual tcu::TestStatus				iterate					(void);

private:
	tcu::TestStatus						verifyImage				(void);

private:
	VkCompareOp							m_depthCompareOps[DepthTest::QUAD_COUNT];
	const tcu::UVec2					m_renderSize;
	const VkFormat						m_colorFormat;
	const VkFormat						m_depthFormat;
	const bool							m_separateDepthStencilLayouts;
	VkPrimitiveTopology					m_primitiveTopology;
	const bool							m_depthBoundsTestEnable;
	const float							m_depthBoundsMin;
	const float							m_depthBoundsMax;
	const bool							m_depthTestEnable;
	const bool							m_stencilTestEnable;
	const bool							m_colorAttachmentEnable;
	const bool							m_hostVisible;
	const DepthClipControlCase			m_depthClipControl;
	VkImageSubresourceRange				m_depthImageSubresourceRange;

	Move<VkImage>						m_colorImage;
	de::MovePtr<Allocation>				m_colorImageAlloc;
	Move<VkImage>						m_depthImage;
	de::MovePtr<Allocation>				m_depthImageAlloc;
	Move<VkImageView>					m_colorAttachmentView;
	Move<VkImageView>					m_depthAttachmentView;
	RenderPassWrapper					m_renderPass;
	Move<VkFramebuffer>					m_framebuffer;

	ShaderWrapper						m_vertexShaderModule;
	ShaderWrapper						m_fragmentShaderModule;

	Move<VkBuffer>						m_vertexBuffer;
	std::vector<Vertex4RGBA>			m_vertices;
	de::MovePtr<Allocation>				m_vertexBufferAlloc;

	Move<VkBuffer>						m_altVertexBuffer;
	std::vector<Vertex4RGBA>			m_altVertices;
	de::MovePtr<Allocation>				m_altVertexBufferAlloc;

	PipelineLayoutWrapper				m_pipelineLayout;
	GraphicsPipelineWrapper				m_graphicsPipelines[DepthTest::QUAD_COUNT];
	GraphicsPipelineWrapper				m_altGraphicsPipelines[DepthTest::QUAD_COUNT];

	Move<VkCommandPool>					m_cmdPool;
	Move<VkCommandBuffer>				m_cmdBuffer;
};

const float DepthTest::quadDepths[QUAD_COUNT] =
{
	0.1f,
	0.0f,
	0.3f,
	0.2f
};

// Depth values suitable for the depth range of -1..1.
const float DepthTest::quadDepthsMinusOneToOne[QUAD_COUNT] =
{
	-0.8f,
	-1.0f,
	 0.6f,
	 0.2f
};

const float DepthTest::quadWs[QUAD_COUNT] =
{
	2.0f,
	1.25f,
	0.5f,
	0.25f
};

DepthTest::DepthTest (tcu::TestContext&					testContext,
					  const std::string&				name,
					  const std::string&				description,
					  const PipelineConstructionType	pipelineConstructionType,
					  const VkFormat					depthFormat,
					  const VkCompareOp					depthCompareOps[QUAD_COUNT],
					  const bool						separateDepthStencilLayouts,
					  const VkPrimitiveTopology			primitiveTopology,
					  const bool						depthBoundsTestEnable,
					  const float						depthBoundsMin,
					  const float						depthBoundsMax,
					  const bool						depthTestEnable,
					  const bool						stencilTestEnable,
					  const bool						colorAttachmentEnable,
					  const bool						hostVisible,
						const tcu::UVec2				renderSize,
					  const DepthClipControlCase		depthClipControl)
	: vkt::TestCase					(testContext, name, description)
	, m_pipelineConstructionType	(pipelineConstructionType)
	, m_depthFormat					(depthFormat)
	, m_separateDepthStencilLayouts	(separateDepthStencilLayouts)
	, m_primitiveTopology			(primitiveTopology)
	, m_depthBoundsTestEnable		(depthBoundsTestEnable)
	, m_depthBoundsMin				(depthBoundsMin)
	, m_depthBoundsMax				(depthBoundsMax)
	, m_depthTestEnable				(depthTestEnable)
	, m_stencilTestEnable			(stencilTestEnable)
	, m_colorAttachmentEnable		(colorAttachmentEnable)
	, m_hostVisible					(hostVisible)
	, m_renderSize					(renderSize)
	, m_depthClipControl			(depthClipControl)
{
	deMemcpy(m_depthCompareOps, depthCompareOps, sizeof(VkCompareOp) * QUAD_COUNT);
}

DepthTest::~DepthTest (void)
{
}

void DepthTest::checkSupport (Context& context) const
{
	if (m_depthBoundsTestEnable)
		context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_DEPTH_BOUNDS);

	if (!isSupportedDepthStencilFormat(context.getInstanceInterface(), context.getPhysicalDevice(), m_depthFormat))
		throw tcu::NotSupportedError(std::string("Unsupported depth/stencil format: ") + getFormatName(m_depthFormat));

	if (m_separateDepthStencilLayouts && !context.isDeviceFunctionalitySupported("VK_KHR_separate_depth_stencil_layouts"))
		TCU_THROW(NotSupportedError, "VK_KHR_separate_depth_stencil_layouts is not supported");

	checkPipelineConstructionRequirements(context.getInstanceInterface(), context.getPhysicalDevice(), m_pipelineConstructionType);

#ifndef CTS_USES_VULKANSC
	if (m_depthClipControl != DepthClipControlCase::DISABLED && !context.isDeviceFunctionalitySupported("VK_EXT_depth_clip_control"))
		TCU_THROW(NotSupportedError, "VK_EXT_depth_clip_control is not supported");
#endif // CTS_USES_VULKANSC
}

TestInstance* DepthTest::createInstance (Context& context) const
{
	return new DepthTestInstance(context, m_pipelineConstructionType, m_depthFormat, m_depthCompareOps, m_separateDepthStencilLayouts, m_primitiveTopology, m_depthBoundsTestEnable, m_depthBoundsMin, m_depthBoundsMax, m_depthTestEnable, m_stencilTestEnable, m_colorAttachmentEnable, m_hostVisible, m_renderSize, m_depthClipControl);
}

void DepthTest::initPrograms (SourceCollections& programCollection) const
{
	if (m_colorAttachmentEnable)
	{
		programCollection.glslSources.add("color_vert") << glu::VertexSource(
			"#version 310 es\n"
			"layout(location = 0) in vec4 position;\n"
			"layout(location = 1) in vec4 color;\n"
			"layout(location = 0) out highp vec4 vtxColor;\n"
			"void main (void)\n"
			"{\n"
			"	gl_Position = position;\n"
			"	gl_PointSize = 1.0f;\n"
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
	else
	{
		programCollection.glslSources.add("color_vert") << glu::VertexSource(
			"#version 310 es\n"
			"layout(location = 0) in vec4 position;\n"
			"layout(location = 1) in vec4 color;\n"
			"void main (void)\n"
			"{\n"
			"	gl_Position = position;\n"
			"	gl_PointSize = 1.0f;\n"
			"}\n");
	}
}

DepthTestInstance::DepthTestInstance (Context&							context,
									  const PipelineConstructionType	pipelineConstructionType,
									  const VkFormat					depthFormat,
									  const VkCompareOp					depthCompareOps[DepthTest::QUAD_COUNT],
									  const bool						separateDepthStencilLayouts,
									  const VkPrimitiveTopology			primitiveTopology,
									  const bool						depthBoundsTestEnable,
									  const float						depthBoundsMin,
									  const float						depthBoundsMax,
									  const bool						depthTestEnable,
									  const bool						stencilTestEnable,
									  const bool						colorAttachmentEnable,
									  const bool						hostVisible,
									  const tcu::UVec2					renderSize,
									  const DepthClipControlCase		depthClipControl)
	: vkt::TestInstance				(context)
	, m_renderSize					(renderSize)
	, m_colorFormat					(colorAttachmentEnable ? VK_FORMAT_R8G8B8A8_UNORM : VK_FORMAT_UNDEFINED)
	, m_depthFormat					(depthFormat)
	, m_separateDepthStencilLayouts	(separateDepthStencilLayouts)
	, m_primitiveTopology			(primitiveTopology)
	, m_depthBoundsTestEnable		(depthBoundsTestEnable)
	, m_depthBoundsMin				(depthBoundsMin)
	, m_depthBoundsMax				(depthBoundsMax)
	, m_depthTestEnable				(depthTestEnable)
	, m_stencilTestEnable			(stencilTestEnable)
	, m_colorAttachmentEnable		(colorAttachmentEnable)
	, m_hostVisible					(hostVisible)
	, m_depthClipControl			(depthClipControl)
	, m_graphicsPipelines
	{
		{ context.getInstanceInterface(), context.getDeviceInterface(), context.getPhysicalDevice(), context.getDevice(), context.getDeviceExtensions(), pipelineConstructionType },
		{ context.getInstanceInterface(), context.getDeviceInterface(), context.getPhysicalDevice(), context.getDevice(), context.getDeviceExtensions(), pipelineConstructionType },
		{ context.getInstanceInterface(), context.getDeviceInterface(), context.getPhysicalDevice(), context.getDevice(), context.getDeviceExtensions(), pipelineConstructionType },
		{ context.getInstanceInterface(), context.getDeviceInterface(), context.getPhysicalDevice(), context.getDevice(), context.getDeviceExtensions(), pipelineConstructionType }
	}
	, m_altGraphicsPipelines
	{
		{ context.getInstanceInterface(), context.getDeviceInterface(), context.getPhysicalDevice(), context.getDevice(), m_context.getDeviceExtensions(), pipelineConstructionType },
		{ context.getInstanceInterface(), context.getDeviceInterface(), context.getPhysicalDevice(), context.getDevice(), m_context.getDeviceExtensions(), pipelineConstructionType },
		{ context.getInstanceInterface(), context.getDeviceInterface(), context.getPhysicalDevice(), context.getDevice(), m_context.getDeviceExtensions(), pipelineConstructionType },
		{ context.getInstanceInterface(), context.getDeviceInterface(), context.getPhysicalDevice(), context.getDevice(), m_context.getDeviceExtensions(), pipelineConstructionType }
	}
{
	const DeviceInterface&		vk						= context.getDeviceInterface();
	const VkDevice				vkDevice				= context.getDevice();
	const deUint32				queueFamilyIndex		= context.getUniversalQueueFamilyIndex();
	SimpleAllocator				memAlloc				(vk, vkDevice, getPhysicalDeviceMemoryProperties(context.getInstanceInterface(), context.getPhysicalDevice()));
	const VkComponentMapping	componentMappingRGBA	= { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
	const bool					hasDepthClipControl		= (m_depthClipControl != DepthClipControlCase::DISABLED);
	const bool					useAltGraphicsPipelines	= (m_depthClipControl == DepthClipControlCase::BEFORE_TWO_DYNAMICS ||
														   m_depthClipControl == DepthClipControlCase::NORMAL_W);
	const bool					useAltVertices			= m_depthClipControl == DepthClipControlCase::NORMAL_W;

	// Copy depth operators
	deMemcpy(m_depthCompareOps, depthCompareOps, sizeof(VkCompareOp) * DepthTest::QUAD_COUNT);

	// Create color image
	if (m_colorAttachmentEnable)
	{
		const VkImageCreateInfo colorImageParams =
		{
			VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,										// VkStructureType			sType;
			DE_NULL,																	// const void*				pNext;
			0u,																			// VkImageCreateFlags		flags;
			VK_IMAGE_TYPE_2D,															// VkImageType				imageType;
			m_colorFormat,																// VkFormat					format;
			{ m_renderSize.x(), m_renderSize.y(), 1u },									// VkExtent3D				extent;
			1u,																			// deUint32					mipLevels;
			1u,																			// deUint32					arrayLayers;
			VK_SAMPLE_COUNT_1_BIT,														// VkSampleCountFlagBits	samples;
			VK_IMAGE_TILING_OPTIMAL,													// VkImageTiling			tiling;
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,		// VkImageUsageFlags		usage;
			VK_SHARING_MODE_EXCLUSIVE,													// VkSharingMode			sharingMode;
			1u,																			// deUint32					queueFamilyIndexCount;
			&queueFamilyIndex,															// const deUint32*			pQueueFamilyIndices;
			VK_IMAGE_LAYOUT_UNDEFINED,													// VkImageLayout			initialLayout;
		};

		m_colorImage			= createImage(vk, vkDevice, &colorImageParams);

		// Allocate and bind color image memory
		m_colorImageAlloc		= memAlloc.allocate(getImageMemoryRequirements(vk, vkDevice, *m_colorImage), MemoryRequirement::Any);
		VK_CHECK(vk.bindImageMemory(vkDevice, *m_colorImage, m_colorImageAlloc->getMemory(), m_colorImageAlloc->getOffset()));
	}

	// Create depth image
	{
		const VkImageCreateInfo depthImageParams =
		{
			VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,			// VkStructureType			sType;
			DE_NULL,										// const void*				pNext;
			0u,												// VkImageCreateFlags		flags;
			VK_IMAGE_TYPE_2D,								// VkImageType				imageType;
			m_depthFormat,									// VkFormat					format;
			{ m_renderSize.x(), m_renderSize.y(), 1u },		// VkExtent3D				extent;
			1u,												// deUint32					mipLevels;
			1u,												// deUint32					arrayLayers;
			VK_SAMPLE_COUNT_1_BIT,							// VkSampleCountFlagBits	samples;
			VK_IMAGE_TILING_OPTIMAL,						// VkImageTiling			tiling;
			VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
			VK_IMAGE_USAGE_TRANSFER_SRC_BIT,				// VkImageUsageFlags		usage;
			VK_SHARING_MODE_EXCLUSIVE,						// VkSharingMode			sharingMode;
			1u,												// deUint32					queueFamilyIndexCount;
			&queueFamilyIndex,								// const deUint32*			pQueueFamilyIndices;
			VK_IMAGE_LAYOUT_UNDEFINED,						// VkImageLayout			initialLayout;
		};

		m_depthImage = createImage(vk, vkDevice, &depthImageParams);

		// Allocate and bind depth image memory
		auto memReqs = MemoryRequirement::Local | MemoryRequirement::HostVisible;
		m_depthImageAlloc = memAlloc.allocate(getImageMemoryRequirements(vk, vkDevice, *m_depthImage), m_hostVisible ? memReqs : MemoryRequirement::Any);
		VK_CHECK(vk.bindImageMemory(vkDevice, *m_depthImage, m_depthImageAlloc->getMemory(), m_depthImageAlloc->getOffset()));

		const VkImageAspectFlags aspect = (mapVkFormat(m_depthFormat).order == tcu::TextureFormat::DS ? VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT
																									  : VK_IMAGE_ASPECT_DEPTH_BIT);
		m_depthImageSubresourceRange    = makeImageSubresourceRange(aspect, 0u, depthImageParams.mipLevels, 0u, depthImageParams.arrayLayers);
	}

	// Create color attachment view
	if (m_colorAttachmentEnable)
	{
		const VkImageViewCreateInfo colorAttachmentViewParams =
		{
			VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,		// VkStructureType			sType;
			DE_NULL,										// const void*				pNext;
			0u,												// VkImageViewCreateFlags	flags;
			*m_colorImage,									// VkImage					image;
			VK_IMAGE_VIEW_TYPE_2D,							// VkImageViewType			viewType;
			m_colorFormat,									// VkFormat					format;
			componentMappingRGBA,							// VkComponentMapping		components;
			{ VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u }	// VkImageSubresourceRange	subresourceRange;
		};

		m_colorAttachmentView = createImageView(vk, vkDevice, &colorAttachmentViewParams);
	}

	// Create depth attachment view
	{
		const VkImageViewCreateInfo depthAttachmentViewParams =
		{
			VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,		// VkStructureType			sType;
			DE_NULL,										// const void*				pNext;
			0u,												// VkImageViewCreateFlags	flags;
			*m_depthImage,									// VkImage					image;
			VK_IMAGE_VIEW_TYPE_2D,							// VkImageViewType			viewType;
			m_depthFormat,									// VkFormat					format;
			componentMappingRGBA,							// VkComponentMapping		components;
			m_depthImageSubresourceRange,					// VkImageSubresourceRange	subresourceRange;
		};

		m_depthAttachmentView = createImageView(vk, vkDevice, &depthAttachmentViewParams);
	}

	// Create render pass
	m_renderPass = RenderPassWrapper(pipelineConstructionType, vk, vkDevice, m_colorFormat, m_depthFormat);

	// Create framebuffer
	{
		std::vector<VkImage>			images;
		std::vector<VkImageView>		attachmentBindInfos;

		if (m_colorAttachmentEnable)
		{
			images.push_back(*m_colorImage);
			attachmentBindInfos.push_back(*m_colorAttachmentView);
		}

		images.push_back(*m_depthImage);
		attachmentBindInfos.push_back(*m_depthAttachmentView);

		const VkFramebufferCreateInfo	framebufferParams	=
		{
			VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,			// VkStructureType				sType;
			DE_NULL,											// const void*					pNext;
			0u,													// VkFramebufferCreateFlags		flags;
			*m_renderPass,										// VkRenderPass					renderPass;
			(deUint32)attachmentBindInfos.size(),				// deUint32						attachmentCount;
			attachmentBindInfos.data(),							// const VkImageView*			pAttachments;
			(deUint32)m_renderSize.x(),							// deUint32						width;
			(deUint32)m_renderSize.y(),							// deUint32						height;
			1u													// deUint32						layers;
		};

		m_renderPass.createFramebuffer(vk, vkDevice, &framebufferParams, images);
	}

	// Create pipeline layout
	{
		const VkPipelineLayoutCreateInfo pipelineLayoutParams =
		{
			VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,		// VkStructureType					sType;
			DE_NULL,											// const void*						pNext;
			0u,													// VkPipelineLayoutCreateFlags		flags;
			0u,													// deUint32							setLayoutCount;
			DE_NULL,											// const VkDescriptorSetLayout*		pSetLayouts;
			0u,													// deUint32							pushConstantRangeCount;
			DE_NULL												// const VkPushConstantRange*		pPushConstantRanges;
		};

		m_pipelineLayout = PipelineLayoutWrapper(pipelineConstructionType, vk, vkDevice, &pipelineLayoutParams);
	}

	// Shader modules
	m_vertexShaderModule		= ShaderWrapper(vk, vkDevice, m_context.getBinaryCollection().get("color_vert"), 0);
	if (m_colorAttachmentEnable)
		m_fragmentShaderModule	= ShaderWrapper(vk, vkDevice, m_context.getBinaryCollection().get("color_frag"), 0);

	const std::vector<VkViewport>	viewports		{ makeViewport(m_renderSize) };
	const std::vector<VkViewport>	badViewports	{ makeViewport(0.0f, 0.0f, static_cast<float>(m_renderSize.x()) / 2.0f, static_cast<float>(m_renderSize.y()) / 2.0f, 1.0f, 0.0f) };
	const std::vector<VkRect2D>		scissors		{ makeRect2D(m_renderSize) };
	const bool						dynamicViewport	= (static_cast<int>(m_depthClipControl) > static_cast<int>(DepthClipControlCase::BEFORE_STATIC));

	// Create pipeline
	{
		const VkVertexInputBindingDescription				vertexInputBindingDescription
		{
			0u,							// deUint32					binding;
			sizeof(Vertex4RGBA),		// deUint32					strideInBytes;
			VK_VERTEX_INPUT_RATE_VERTEX	// VkVertexInputStepRate	inputRate;
		};

		const VkVertexInputAttributeDescription				vertexInputAttributeDescriptions[2]
		{
			{
				0u,									// deUint32	location;
				0u,									// deUint32	binding;
				VK_FORMAT_R32G32B32A32_SFLOAT,		// VkFormat	format;
				0u									// deUint32	offset;
			},
			{
				1u,									// deUint32	location;
				0u,									// deUint32	binding;
				VK_FORMAT_R32G32B32A32_SFLOAT,		// VkFormat	format;
				DE_OFFSET_OF(Vertex4RGBA, color),	// deUint32	offset;
			}
		};

		const VkPipelineVertexInputStateCreateInfo			vertexInputStateParams
		{
			VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,		// VkStructureType							sType;
			DE_NULL,														// const void*								pNext;
			0u,																// VkPipelineVertexInputStateCreateFlags	flags;
			1u,																// deUint32									vertexBindingDescriptionCount;
			&vertexInputBindingDescription,									// const VkVertexInputBindingDescription*	pVertexBindingDescriptions;
			2u,																// deUint32									vertexAttributeDescriptionCount;
			vertexInputAttributeDescriptions								// const VkVertexInputAttributeDescription*	pVertexAttributeDescriptions;
		};

		const VkPipelineInputAssemblyStateCreateInfo		inputAssemblyStateParams
		{
			VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,	// VkStructureType								sType
			DE_NULL,														// const void*									pNext
			0u,																// VkPipelineInputAssemblyStateCreateFlags		flags
			m_primitiveTopology,											// VkPrimitiveTopology							topology
			VK_FALSE														// VkBool32										primitiveRestartEnable
		};

		VkPipelineDepthStencilStateCreateInfo				depthStencilStateParams
		{
			VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,	// VkStructureType							sType;
			DE_NULL,													// const void*								pNext;
			0u,															// VkPipelineDepthStencilStateCreateFlags	flags;
			m_depthTestEnable,											// VkBool32									depthTestEnable;
			true,														// VkBool32									depthWriteEnable;
			VK_COMPARE_OP_LESS,											// VkCompareOp								depthCompareOp;
			m_depthBoundsTestEnable,									// VkBool32									depthBoundsTestEnable;
			m_stencilTestEnable,										// VkBool32									stencilTestEnable;
			// VkStencilOpState	front;
			{
				VK_STENCIL_OP_KEEP,		// VkStencilOp	failOp;
				VK_STENCIL_OP_KEEP,		// VkStencilOp	passOp;
				VK_STENCIL_OP_KEEP,		// VkStencilOp	depthFailOp;
				VK_COMPARE_OP_NEVER,	// VkCompareOp	compareOp;
				0u,						// deUint32		compareMask;
				0u,						// deUint32		writeMask;
				0u,						// deUint32		reference;
			},
			// VkStencilOpState	back;
			{
				VK_STENCIL_OP_KEEP,		// VkStencilOp	failOp;
				VK_STENCIL_OP_KEEP,		// VkStencilOp	passOp;
				VK_STENCIL_OP_KEEP,		// VkStencilOp	depthFailOp;
				VK_COMPARE_OP_NEVER,	// VkCompareOp	compareOp;
				0u,						// deUint32		compareMask;
				0u,						// deUint32		writeMask;
				0u,						// deUint32		reference;
			},
			m_depthBoundsMin,											// float									minDepthBounds;
			m_depthBoundsMax,											// float									maxDepthBounds;
		};

		// Make sure rasterization is not disabled when the fragment shader is missing.
		const vk::VkPipelineRasterizationStateCreateInfo rasterizationStateParams
		{
			vk::VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,	//	VkStructureType							sType;
			nullptr,														//	const void*								pNext;
			0u,																//	VkPipelineRasterizationStateCreateFlags	flags;
			VK_FALSE,														//	VkBool32								depthClampEnable;
			VK_FALSE,														//	VkBool32								rasterizerDiscardEnable;
			vk::VK_POLYGON_MODE_FILL,										//	VkPolygonMode							polygonMode;
			vk::VK_CULL_MODE_NONE,											//	VkCullModeFlags							cullMode;
			vk::VK_FRONT_FACE_COUNTER_CLOCKWISE,							//	VkFrontFace								frontFace;
			VK_FALSE,														//	VkBool32								depthBiasEnable;
			0.0f,															//	float									depthBiasConstantFactor;
			0.0f,															//	float									depthBiasClamp;
			0.0f,															//	float									depthBiasSlopeFactor;
			1.0f,															//	float									lineWidth;
		};

		PipelineViewportDepthClipControlCreateInfoWrapper depthClipControlWrapper;
		PipelineViewportDepthClipControlCreateInfoWrapper depthClipControl01Wrapper;

#ifndef CTS_USES_VULKANSC
		VkPipelineViewportDepthClipControlCreateInfoEXT depthClipControlCreateInfo
		{
			VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_DEPTH_CLIP_CONTROL_CREATE_INFO_EXT,	// VkStructureType	sType;
			DE_NULL,																// const void*		pNext;
			VK_TRUE,																// VkBool32			negativeOneToOne;
		};
		if (hasDepthClipControl)
			depthClipControlWrapper.ptr = &depthClipControlCreateInfo;

		// Using the range 0,1 in the structure.
		VkPipelineViewportDepthClipControlCreateInfoEXT depthClipControlCreateInfo01
		{
			VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_DEPTH_CLIP_CONTROL_CREATE_INFO_EXT,	// VkStructureType	sType;
			DE_NULL,																// const void*		pNext;
			VK_FALSE,																// VkBool32			negativeOneToOne;
		};
		depthClipControl01Wrapper.ptr = &depthClipControlCreateInfo01;
#endif // CTS_USES_VULKANSC

		// Dynamic viewport if needed.
		std::vector<VkDynamicState> dynamicStates;

		if (m_depthClipControl == DepthClipControlCase::BEFORE_DYNAMIC
			|| m_depthClipControl == DepthClipControlCase::BEFORE_TWO_DYNAMICS
			|| m_depthClipControl == DepthClipControlCase::AFTER_DYNAMIC)
		{
			dynamicStates.push_back(VK_DYNAMIC_STATE_VIEWPORT);
		}

		const VkPipelineDynamicStateCreateInfo					dynamicStateCreateInfo			=
		{
			VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,	//	VkStructureType						sType;
			nullptr,												//	const void*							pNext;
			0u,														//	VkPipelineDynamicStateCreateFlags	flags;
			static_cast<uint32_t>(dynamicStates.size()),			//	uint32_t							dynamicStateCount;
			de::dataOrNull(dynamicStates),							//	const VkDynamicState*				pDynamicStates;
		};

		const vk::VkPipelineColorBlendAttachmentState blendState
		{
			VK_FALSE,
			VK_BLEND_FACTOR_ONE,
			VK_BLEND_FACTOR_ONE,
			VK_BLEND_OP_ADD,
			VK_BLEND_FACTOR_ONE,
			VK_BLEND_FACTOR_ONE,
			VK_BLEND_OP_ADD,
			VK_COLOR_COMPONENT_R_BIT |
			VK_COLOR_COMPONENT_G_BIT |
			VK_COLOR_COMPONENT_B_BIT |
			VK_COLOR_COMPONENT_A_BIT,
		};

		deUint32 colorAttachmentCount = (m_colorFormat != VK_FORMAT_UNDEFINED) ? 1u : 0u;

		const VkPipelineColorBlendStateCreateInfo colorBlendStateCreateInfo
		{
			VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,		// VkStructureType								sType
			DE_NULL,														// const void*									pNext
			0u,																// VkPipelineColorBlendStateCreateFlags			flags
			VK_FALSE,														// VkBool32										logicOpEnable
			VK_LOGIC_OP_CLEAR,												// VkLogicOp									logicOp
			colorAttachmentCount,											// deUint32										attachmentCount
			&blendState,													// const VkPipelineColorBlendAttachmentState*	pAttachments
			{ 0.0f, 0.0f, 0.0f, 0.0f }										// float										blendConstants[4]
		};

		for (int quadNdx = 0; quadNdx < DepthTest::QUAD_COUNT; quadNdx++)
		{
			depthStencilStateParams.depthCompareOp	= depthCompareOps[quadNdx];

			m_graphicsPipelines[quadNdx].setDefaultMultisampleState()
										.setDefaultColorBlendState()
										.setViewportStatePnext(depthClipControlWrapper.ptr)
										.setDynamicState(&dynamicStateCreateInfo)
										.setupVertexInputState(&vertexInputStateParams, &inputAssemblyStateParams)
										.setupPreRasterizationShaderState((dynamicViewport ? badViewports : viewports),
																	scissors,
																	m_pipelineLayout,
																	*m_renderPass,
																	0u,
																	m_vertexShaderModule,
																	&rasterizationStateParams)
										.setupFragmentShaderState(m_pipelineLayout,
																	*m_renderPass,
																	0u,
																	m_fragmentShaderModule,
																	&depthStencilStateParams)
										.setupFragmentOutputState(*m_renderPass, 0u, &colorBlendStateCreateInfo)
										.setMonolithicPipelineLayout(m_pipelineLayout)
										.buildPipeline();

			if (useAltGraphicsPipelines)
			{
				if (m_depthClipControl == DepthClipControlCase::NORMAL_W)
				{
					m_altGraphicsPipelines[quadNdx].setDefaultMultisampleState()
												   .setDefaultColorBlendState()
												   .setViewportStatePnext(depthClipControl01Wrapper.ptr)
												   .setDynamicState(&dynamicStateCreateInfo)
												   .setupVertexInputState(&vertexInputStateParams, &inputAssemblyStateParams)
												   .setupPreRasterizationShaderState((dynamicViewport ? badViewports : viewports),
																				scissors,
																				m_pipelineLayout,
																				*m_renderPass,
																				0u,
																				m_vertexShaderModule,
																				&rasterizationStateParams)
												   .setupFragmentShaderState(m_pipelineLayout,
																		*m_renderPass,
																		0u,
																		m_fragmentShaderModule,
																		&depthStencilStateParams)
												   .setupFragmentOutputState(*m_renderPass, 0u, &colorBlendStateCreateInfo)
												   .setMonolithicPipelineLayout(m_pipelineLayout)
												   .buildPipeline();
				}
				else
				{
					m_altGraphicsPipelines[quadNdx].setDefaultMultisampleState()
												   .setDefaultColorBlendState()
												   .setViewportStatePnext(depthClipControl01Wrapper.ptr)
												   .setDynamicState(&dynamicStateCreateInfo)
												   .setupVertexInputState(&vertexInputStateParams)
												   .setupPreRasterizationShaderState((dynamicViewport ? badViewports : viewports),
																					scissors,
																					m_pipelineLayout,
																					*m_renderPass,
																					0u,
																					m_vertexShaderModule,
																					&rasterizationStateParams)
												   .setupFragmentShaderState(m_pipelineLayout,
																					*m_renderPass,
																					0u,
																					m_fragmentShaderModule,
																					&depthStencilStateParams)
												   .setupFragmentOutputState(*m_renderPass, 0u, &colorBlendStateCreateInfo)
												   .setMonolithicPipelineLayout(m_pipelineLayout)
												   .buildPipeline();
				}
			}
		}
	}

	// Create vertex buffer
	{
		const VkBufferCreateInfo vertexBufferParams =
		{
			VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,		// VkStructureType		sType;
			DE_NULL,									// const void*			pNext;
			0u,											// VkBufferCreateFlags	flags;
			1024u,										// VkDeviceSize			size;
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,			// VkBufferUsageFlags	usage;
			VK_SHARING_MODE_EXCLUSIVE,					// VkSharingMode		sharingMode;
			1u,											// deUint32				queueFamilyIndexCount;
			&queueFamilyIndex							// const deUint32*		pQueueFamilyIndices;
		};

		m_vertices			= createOverlappingQuads();
		m_vertexBuffer		= createBuffer(vk, vkDevice, &vertexBufferParams);
		m_vertexBufferAlloc	= memAlloc.allocate(getBufferMemoryRequirements(vk, vkDevice, *m_vertexBuffer), MemoryRequirement::HostVisible);

		VK_CHECK(vk.bindBufferMemory(vkDevice, *m_vertexBuffer, m_vertexBufferAlloc->getMemory(), m_vertexBufferAlloc->getOffset()));

		if (useAltVertices) {
			m_altVertices			= createOverlappingQuads();
			m_altVertexBuffer		= createBuffer(vk, vkDevice, &vertexBufferParams);
			m_altVertexBufferAlloc	= memAlloc.allocate(getBufferMemoryRequirements(vk, vkDevice, *m_altVertexBuffer), MemoryRequirement::HostVisible);

			VK_CHECK(vk.bindBufferMemory(vkDevice, *m_altVertexBuffer, m_altVertexBufferAlloc->getMemory(), m_altVertexBufferAlloc->getOffset()));
		}

		// Adjust depths
		for (int quadNdx = 0; quadNdx < DepthTest::QUAD_COUNT; quadNdx++)
			for (int vertexNdx = 0; vertexNdx < 6; vertexNdx++)
			{
				m_vertices[quadNdx * 6 + vertexNdx].position.z() = (hasDepthClipControl ? DepthTest::quadDepthsMinusOneToOne[quadNdx] : DepthTest::quadDepths[quadNdx]);
				if (m_depthClipControl == DepthClipControlCase::NORMAL_W)
				{
					const float w = DepthTest::quadWs[quadNdx];
					m_vertices[quadNdx * 6 + vertexNdx].position.x() *= w;
					m_vertices[quadNdx * 6 + vertexNdx].position.y() *= w;
					m_vertices[quadNdx * 6 + vertexNdx].position.z() *= w;
					m_vertices[quadNdx * 6 + vertexNdx].position.w() = w;
				}
				if (useAltVertices)
				{
					m_altVertices[quadNdx * 6 + vertexNdx].position = m_vertices[quadNdx * 6 + vertexNdx].position;
					float z = m_altVertices[quadNdx * 6 + vertexNdx].position.z();
					float w = m_altVertices[quadNdx * 6 + vertexNdx].position.w();
					if (depthCompareOps[quadNdx] == vk::VK_COMPARE_OP_NOT_EQUAL ||
						depthCompareOps[quadNdx] == vk::VK_COMPARE_OP_LESS ||
						depthCompareOps[quadNdx] == vk::VK_COMPARE_OP_LESS_OR_EQUAL)
					{
						z += 0.01f;
					}
					else if (depthCompareOps[quadNdx] == vk::VK_COMPARE_OP_GREATER ||
							 depthCompareOps[quadNdx] == vk::VK_COMPARE_OP_GREATER_OR_EQUAL)
					{
						z -= 0.01f;
					}
					m_altVertices[quadNdx * 6 + vertexNdx].position.z() = (z + w) * 0.5f;
				}
			}

		// Load vertices into vertex buffer
		deMemcpy(m_vertexBufferAlloc->getHostPtr(), m_vertices.data(), m_vertices.size() * sizeof(Vertex4RGBA));
		flushAlloc(vk, vkDevice, *m_vertexBufferAlloc);

		if (useAltVertices) {
			deMemcpy(m_altVertexBufferAlloc->getHostPtr(), m_altVertices.data(), m_altVertices.size() * sizeof(Vertex4RGBA));
			flushAlloc(vk, vkDevice, *m_altVertexBufferAlloc);
		}
	}

	// Create command pool
	m_cmdPool = createCommandPool(vk, vkDevice, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, queueFamilyIndex);

	// Create command buffer
	{
		std::vector<VkClearValue>			attachmentClearValues;

		if (m_colorAttachmentEnable)
			attachmentClearValues.push_back(defaultClearValue(m_colorFormat));

		attachmentClearValues.push_back(defaultClearValue(m_depthFormat));

		const VkImageMemoryBarrier			colorBarrier					=
		{
			VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,									// VkStructureType            sType;
			DE_NULL,																// const void*                pNext;
			(VkAccessFlags)0,														// VkAccessFlags              srcAccessMask;
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,									// VkAccessFlags              dstAccessMask;
			VK_IMAGE_LAYOUT_UNDEFINED,												// VkImageLayout              oldLayout;
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,								// VkImageLayout              newLayout;
			VK_QUEUE_FAMILY_IGNORED,												// uint32_t                   srcQueueFamilyIndex;
			VK_QUEUE_FAMILY_IGNORED,												// uint32_t                   dstQueueFamilyIndex;
			*m_colorImage,															// VkImage                    image;
			{ VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u }							// VkImageSubresourceRange    subresourceRange;
		};

		VkImageSubresourceRange				depthBarrierSubresourceRange	= m_depthImageSubresourceRange;
		VkImageLayout						newLayout						= VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		if (m_separateDepthStencilLayouts)
		{
			depthBarrierSubresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
			newLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
		}

		const VkImageMemoryBarrier			depthBarrier					=
		{
			VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,									// VkStructureType            sType;
			DE_NULL,																// const void*                pNext;
			(VkAccessFlags)0,														// VkAccessFlags              srcAccessMask;
			VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,							// VkAccessFlags              dstAccessMask;
			VK_IMAGE_LAYOUT_UNDEFINED,												// VkImageLayout              oldLayout;
			newLayout										,						// VkImageLayout              newLayout;
			VK_QUEUE_FAMILY_IGNORED,												// uint32_t                   srcQueueFamilyIndex;
			VK_QUEUE_FAMILY_IGNORED,												// uint32_t                   dstQueueFamilyIndex;
			*m_depthImage,															// VkImage                    image;
			depthBarrierSubresourceRange,											// VkImageSubresourceRange    subresourceRange;
		};

		std::vector<VkImageMemoryBarrier>	imageLayoutBarriers;

		if (m_colorAttachmentEnable)
			imageLayoutBarriers.push_back(colorBarrier);

		imageLayoutBarriers.push_back(depthBarrier);

		m_cmdBuffer = allocateCommandBuffer(vk, vkDevice, *m_cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

		beginCommandBuffer(vk, *m_cmdBuffer, 0u);

		vk.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
			VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, (VkDependencyFlags)0,
			0u, DE_NULL, 0u, DE_NULL, (deUint32)imageLayoutBarriers.size(), imageLayoutBarriers.data());

		m_renderPass.begin(vk, *m_cmdBuffer, makeRect2D(0, 0, m_renderSize.x(), m_renderSize.y()), (deUint32)attachmentClearValues.size(), attachmentClearValues.data());

		const VkDeviceSize quadOffset = (m_vertices.size() / DepthTest::QUAD_COUNT) * sizeof(Vertex4RGBA);

		for (int quadNdx = 0; quadNdx < DepthTest::QUAD_COUNT; quadNdx++)
		{
			VkDeviceSize vertexBufferOffset = quadOffset * quadNdx;

			if (m_depthClipControl == DepthClipControlCase::NORMAL_W && depthCompareOps[quadNdx] != vk::VK_COMPARE_OP_NEVER)
			{
				m_altGraphicsPipelines[quadNdx].bind(*m_cmdBuffer);
				vk.cmdBindVertexBuffers(*m_cmdBuffer, 0, 1, &m_altVertexBuffer.get(), &vertexBufferOffset);
				vk.cmdDraw(*m_cmdBuffer, (deUint32)(m_altVertices.size() / DepthTest::QUAD_COUNT), 1, 0, 0);
			}

			if (m_depthClipControl == DepthClipControlCase::BEFORE_STATIC
				|| m_depthClipControl == DepthClipControlCase::BEFORE_DYNAMIC
				|| m_depthClipControl == DepthClipControlCase::BEFORE_TWO_DYNAMICS)
			{
				if (vk::isConstructionTypeShaderObject(pipelineConstructionType))
				{
#ifndef CTS_USES_VULKANSC
					vk.cmdSetViewportWithCount(*m_cmdBuffer, 1u, viewports.data());
#else
					vk.cmdSetViewportWithCountEXT(*m_cmdBuffer, 1u, viewports.data());
#endif
				}
				else
				{
					vk.cmdSetViewport(*m_cmdBuffer, 0u, 1u, viewports.data());
				}
			}

			if (m_depthClipControl == DepthClipControlCase::BEFORE_TWO_DYNAMICS)
				m_altGraphicsPipelines[quadNdx].bind(*m_cmdBuffer);
			m_graphicsPipelines[quadNdx].bind(*m_cmdBuffer);

			if (m_depthClipControl == DepthClipControlCase::AFTER_DYNAMIC)
			{
				if (vk::isConstructionTypeShaderObject(pipelineConstructionType))
				{
#ifndef CTS_USES_VULKANSC
					vk.cmdSetViewportWithCount(*m_cmdBuffer, 1u, viewports.data());
#else
					vk.cmdSetViewportWithCountEXT(*m_cmdBuffer, 1u, viewports.data());
#endif
				}
				else
				{
					vk.cmdSetViewport(*m_cmdBuffer, 0u, 1u, viewports.data());
				}
			}

			vk.cmdBindVertexBuffers(*m_cmdBuffer, 0, 1, &m_vertexBuffer.get(), &vertexBufferOffset);
			vk.cmdDraw(*m_cmdBuffer, (deUint32)(m_vertices.size() / DepthTest::QUAD_COUNT), 1, 0, 0);
		}

		m_renderPass.end(vk, *m_cmdBuffer);
		endCommandBuffer(vk, *m_cmdBuffer);
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

	submitCommandsAndWait(vk, vkDevice, queue, m_cmdBuffer.get());

	return verifyImage();
}

tcu::TestStatus DepthTestInstance::verifyImage (void)
{
	const tcu::TextureFormat	tcuColorFormat	= mapVkFormat(VK_FORMAT_R8G8B8A8_UNORM);
	const tcu::TextureFormat	tcuDepthFormat	= mapVkFormat(m_depthFormat);
	const ColorVertexShader		vertexShader;
	const ColorFragmentShader	fragmentShader	(tcuColorFormat, tcuDepthFormat, (m_depthClipControl != DepthClipControlCase::DISABLED));
	const rr::Program			program			(&vertexShader, &fragmentShader);
	ReferenceRenderer			refRenderer		(m_renderSize.x(), m_renderSize.y(), 1, tcuColorFormat, tcuDepthFormat, &program);
	bool						colorCompareOk	= false;
	bool						depthCompareOk	= false;

	// Render reference image
	{
		for (int quadNdx = 0; quadNdx < DepthTest::QUAD_COUNT; quadNdx++)
		{
			// Set depth state
			rr::RenderState renderState(refRenderer.getViewportState(), m_context.getDeviceProperties().limits.subPixelPrecisionBits);
			renderState.fragOps.depthTestEnabled = m_depthTestEnable;
			renderState.fragOps.depthFunc = mapVkCompareOp(m_depthCompareOps[quadNdx]);
			if (m_depthBoundsTestEnable)
			{
				renderState.fragOps.depthBoundsTestEnabled = true;
				renderState.fragOps.minDepthBound = m_depthBoundsMin;
				renderState.fragOps.maxDepthBound = m_depthBoundsMax;
			}

			refRenderer.draw(renderState,
							 mapVkPrimitiveTopology(m_primitiveTopology),
							 std::vector<Vertex4RGBA>(m_vertices.begin() + quadNdx * 6,
													  m_vertices.begin() + (quadNdx + 1) * 6));
		}
	}

	// Compare color result with reference image
	if (m_colorAttachmentEnable)
	{
		const DeviceInterface&			vk					= m_context.getDeviceInterface();
		const VkDevice					vkDevice			= m_context.getDevice();
		const VkQueue					queue				= m_context.getUniversalQueue();
		const deUint32					queueFamilyIndex	= m_context.getUniversalQueueFamilyIndex();
		SimpleAllocator					allocator			(vk, vkDevice, getPhysicalDeviceMemoryProperties(m_context.getInstanceInterface(), m_context.getPhysicalDevice()));
		de::MovePtr<tcu::TextureLevel>	result				= readColorAttachment(vk, vkDevice, queue, queueFamilyIndex, allocator, *m_colorImage, m_colorFormat, m_renderSize);

		colorCompareOk = tcu::intThresholdPositionDeviationCompare(m_context.getTestContext().getLog(),
															  "IntImageCompare",
															  "Image comparison",
															  refRenderer.getAccess(),
															  result->getAccess(),
															  tcu::UVec4(2, 2, 2, 2),
															  tcu::IVec3(1, 1, 0),
															  true,
															  tcu::COMPARE_LOG_RESULT);
	}
	else
	{
		colorCompareOk = true;
	}

	// Compare depth result with reference image
	{
		const DeviceInterface&			vk					= m_context.getDeviceInterface();
		const VkDevice					vkDevice			= m_context.getDevice();
		const VkQueue					queue				= m_context.getUniversalQueue();
		const deUint32					queueFamilyIndex	= m_context.getUniversalQueueFamilyIndex();
		SimpleAllocator					allocator			(vk, vkDevice, getPhysicalDeviceMemoryProperties(m_context.getInstanceInterface(), m_context.getPhysicalDevice()));
		de::MovePtr<tcu::TextureLevel>	result				= readDepthAttachment(vk, vkDevice, queue, queueFamilyIndex, allocator, *m_depthImage, m_depthFormat, m_renderSize);

		{
			de::MovePtr<tcu::TextureLevel>	convertedReferenceLevel;
			tcu::Maybe<tcu::TextureFormat>	convertedFormat;

			if (refRenderer.getDepthStencilAccess().getFormat().type == tcu::TextureFormat::UNSIGNED_INT_24_8_REV)
			{
				convertedFormat = tcu::TextureFormat(tcu::TextureFormat::D, tcu::TextureFormat::UNORM_INT24);
			}
			else if (refRenderer.getDepthStencilAccess().getFormat().type == tcu::TextureFormat::UNSIGNED_INT_16_8_8)
			{
				convertedFormat = tcu::TextureFormat(tcu::TextureFormat::D, tcu::TextureFormat::UNORM_INT16);
			}
			else if (refRenderer.getDepthStencilAccess().getFormat().type == tcu::TextureFormat::FLOAT_UNSIGNED_INT_24_8_REV)
			{
				convertedFormat = tcu::TextureFormat(tcu::TextureFormat::D, tcu::TextureFormat::FLOAT);
			}

			if (convertedFormat)
			{
				convertedReferenceLevel = de::MovePtr<tcu::TextureLevel>(new tcu::TextureLevel(*convertedFormat, refRenderer.getDepthStencilAccess().getSize().x(), refRenderer.getDepthStencilAccess().getSize().y()));
				tcu::copy(convertedReferenceLevel->getAccess(), refRenderer.getDepthStencilAccess());
			}

			float depthThreshold = 0.0f;

			if (tcu::getTextureChannelClass(result->getFormat().type) == tcu::TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT)
			{
				const tcu::IVec4	formatBits = tcu::getTextureFormatBitDepth(result->getFormat());
				depthThreshold = 1.0f / static_cast<float>((1 << formatBits[0]) - 1);
			}
			else if (tcu::getTextureChannelClass(result->getFormat().type) == tcu::TEXTURECHANNELCLASS_FLOATING_POINT)
			{
				depthThreshold = 0.0000001f;
			}
			else
				TCU_FAIL("unrecognized format type class");

			depthCompareOk = tcu::floatThresholdCompare(m_context.getTestContext().getLog(),
														"DepthImageCompare",
														"Depth image comparison",
														convertedReferenceLevel ? convertedReferenceLevel->getAccess() : refRenderer.getDepthStencilAccess(),
														result->getAccess(),
														tcu::Vec4(depthThreshold, 0.0f, 0.0f, 0.0f),
														tcu::COMPARE_LOG_RESULT);
		}
	}

	if (colorCompareOk && depthCompareOk)
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

std::string getTopologyName (const VkPrimitiveTopology topology) {
	const std::string	fullName	= getPrimitiveTopologyName(topology);

	DE_ASSERT(de::beginsWith(fullName, "VK_PRIMITIVE_TOPOLOGY_"));

	return de::toLower(fullName.substr(22));
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

tcu::TestCaseGroup* createDepthTests (tcu::TestContext& testCtx, PipelineConstructionType pipelineConstructionType)
{
	const VkFormat			depthFormats[]						=
	{
		VK_FORMAT_D16_UNORM,
		VK_FORMAT_X8_D24_UNORM_PACK32,
		VK_FORMAT_D32_SFLOAT,
		VK_FORMAT_D16_UNORM_S8_UINT,
		VK_FORMAT_D24_UNORM_S8_UINT,
		VK_FORMAT_D32_SFLOAT_S8_UINT
	};

	// Each entry configures the depth compare operators of QUAD_COUNT quads.
	// All entries cover pair-wise combinations of compare operators.
	const VkCompareOp		depthOps[][DepthTest::QUAD_COUNT]	=
	{
		{ VK_COMPARE_OP_NOT_EQUAL,			VK_COMPARE_OP_NOT_EQUAL,		VK_COMPARE_OP_NOT_EQUAL,		VK_COMPARE_OP_NOT_EQUAL },
		{ VK_COMPARE_OP_NOT_EQUAL,			VK_COMPARE_OP_EQUAL,			VK_COMPARE_OP_EQUAL,			VK_COMPARE_OP_GREATER },
		{ VK_COMPARE_OP_NOT_EQUAL,			VK_COMPARE_OP_GREATER,			VK_COMPARE_OP_GREATER,			VK_COMPARE_OP_LESS_OR_EQUAL },
		{ VK_COMPARE_OP_NOT_EQUAL,			VK_COMPARE_OP_GREATER_OR_EQUAL,	VK_COMPARE_OP_GREATER_OR_EQUAL,	VK_COMPARE_OP_GREATER_OR_EQUAL },
		{ VK_COMPARE_OP_NOT_EQUAL,			VK_COMPARE_OP_LESS_OR_EQUAL,	VK_COMPARE_OP_LESS_OR_EQUAL,	VK_COMPARE_OP_ALWAYS },
		{ VK_COMPARE_OP_NOT_EQUAL,			VK_COMPARE_OP_LESS,				VK_COMPARE_OP_LESS,				VK_COMPARE_OP_LESS },
		{ VK_COMPARE_OP_NOT_EQUAL,			VK_COMPARE_OP_NEVER,			VK_COMPARE_OP_NEVER,			VK_COMPARE_OP_NEVER },
		{ VK_COMPARE_OP_EQUAL,				VK_COMPARE_OP_NOT_EQUAL,		VK_COMPARE_OP_EQUAL,			VK_COMPARE_OP_EQUAL },
		{ VK_COMPARE_OP_EQUAL,				VK_COMPARE_OP_EQUAL,			VK_COMPARE_OP_NOT_EQUAL,		VK_COMPARE_OP_LESS },
		{ VK_COMPARE_OP_EQUAL,				VK_COMPARE_OP_GREATER,			VK_COMPARE_OP_GREATER_OR_EQUAL,	VK_COMPARE_OP_NOT_EQUAL },
		{ VK_COMPARE_OP_EQUAL,				VK_COMPARE_OP_GREATER_OR_EQUAL,	VK_COMPARE_OP_GREATER,			VK_COMPARE_OP_GREATER },
		{ VK_COMPARE_OP_EQUAL,				VK_COMPARE_OP_LESS_OR_EQUAL,	VK_COMPARE_OP_LESS,				VK_COMPARE_OP_LESS_OR_EQUAL },
		{ VK_COMPARE_OP_NOT_EQUAL,			VK_COMPARE_OP_ALWAYS,			VK_COMPARE_OP_ALWAYS,			VK_COMPARE_OP_EQUAL },
		{ VK_COMPARE_OP_EQUAL,				VK_COMPARE_OP_LESS,				VK_COMPARE_OP_NEVER,			VK_COMPARE_OP_ALWAYS },
		{ VK_COMPARE_OP_EQUAL,				VK_COMPARE_OP_NEVER,			VK_COMPARE_OP_LESS_OR_EQUAL,	VK_COMPARE_OP_GREATER_OR_EQUAL },
		{ VK_COMPARE_OP_GREATER,			VK_COMPARE_OP_NOT_EQUAL,		VK_COMPARE_OP_GREATER,			VK_COMPARE_OP_LESS },
		{ VK_COMPARE_OP_GREATER,			VK_COMPARE_OP_EQUAL,			VK_COMPARE_OP_GREATER_OR_EQUAL,	VK_COMPARE_OP_ALWAYS },
		{ VK_COMPARE_OP_GREATER,			VK_COMPARE_OP_GREATER,			VK_COMPARE_OP_NOT_EQUAL,		VK_COMPARE_OP_GREATER },
		{ VK_COMPARE_OP_GREATER,			VK_COMPARE_OP_GREATER_OR_EQUAL,	VK_COMPARE_OP_LESS_OR_EQUAL,	VK_COMPARE_OP_NOT_EQUAL },
		{ VK_COMPARE_OP_GREATER,			VK_COMPARE_OP_LESS_OR_EQUAL,	VK_COMPARE_OP_NEVER,			VK_COMPARE_OP_GREATER_OR_EQUAL },
		{ VK_COMPARE_OP_GREATER,			VK_COMPARE_OP_LESS,				VK_COMPARE_OP_EQUAL,			VK_COMPARE_OP_NEVER },
		{ VK_COMPARE_OP_GREATER_OR_EQUAL,	VK_COMPARE_OP_NOT_EQUAL,		VK_COMPARE_OP_GREATER_OR_EQUAL,	VK_COMPARE_OP_GREATER },
		{ VK_COMPARE_OP_GREATER,			VK_COMPARE_OP_NEVER,			VK_COMPARE_OP_ALWAYS,			VK_COMPARE_OP_LESS_OR_EQUAL },
		{ VK_COMPARE_OP_GREATER_OR_EQUAL,	VK_COMPARE_OP_EQUAL,			VK_COMPARE_OP_GREATER,			VK_COMPARE_OP_NOT_EQUAL },
		{ VK_COMPARE_OP_GREATER_OR_EQUAL,	VK_COMPARE_OP_GREATER,			VK_COMPARE_OP_EQUAL,			VK_COMPARE_OP_GREATER_OR_EQUAL },
		{ VK_COMPARE_OP_GREATER_OR_EQUAL,	VK_COMPARE_OP_GREATER_OR_EQUAL,	VK_COMPARE_OP_NOT_EQUAL,		VK_COMPARE_OP_LESS_OR_EQUAL },
		{ VK_COMPARE_OP_GREATER_OR_EQUAL,	VK_COMPARE_OP_LESS_OR_EQUAL,	VK_COMPARE_OP_ALWAYS,			VK_COMPARE_OP_LESS },
		{ VK_COMPARE_OP_GREATER_OR_EQUAL,	VK_COMPARE_OP_LESS,				VK_COMPARE_OP_LESS_OR_EQUAL,	VK_COMPARE_OP_EQUAL },
		{ VK_COMPARE_OP_GREATER_OR_EQUAL,	VK_COMPARE_OP_ALWAYS,			VK_COMPARE_OP_LESS,				VK_COMPARE_OP_NEVER },
		{ VK_COMPARE_OP_LESS_OR_EQUAL,		VK_COMPARE_OP_NOT_EQUAL,		VK_COMPARE_OP_LESS_OR_EQUAL,	VK_COMPARE_OP_LESS_OR_EQUAL },
		{ VK_COMPARE_OP_LESS_OR_EQUAL,		VK_COMPARE_OP_EQUAL,			VK_COMPARE_OP_LESS,				VK_COMPARE_OP_EQUAL },
		{ VK_COMPARE_OP_LESS_OR_EQUAL,		VK_COMPARE_OP_GREATER,			VK_COMPARE_OP_NEVER,			VK_COMPARE_OP_LESS },
		{ VK_COMPARE_OP_LESS_OR_EQUAL,		VK_COMPARE_OP_GREATER_OR_EQUAL,	VK_COMPARE_OP_EQUAL,			VK_COMPARE_OP_ALWAYS },
		{ VK_COMPARE_OP_LESS_OR_EQUAL,		VK_COMPARE_OP_LESS,				VK_COMPARE_OP_NOT_EQUAL,		VK_COMPARE_OP_GREATER_OR_EQUAL },
		{ VK_COMPARE_OP_LESS_OR_EQUAL,		VK_COMPARE_OP_LESS_OR_EQUAL,	VK_COMPARE_OP_GREATER_OR_EQUAL,	VK_COMPARE_OP_NEVER },
		{ VK_COMPARE_OP_LESS,				VK_COMPARE_OP_NOT_EQUAL,		VK_COMPARE_OP_LESS,				VK_COMPARE_OP_GREATER_OR_EQUAL },
		{ VK_COMPARE_OP_LESS,				VK_COMPARE_OP_EQUAL,			VK_COMPARE_OP_NEVER,			VK_COMPARE_OP_LESS_OR_EQUAL },
		{ VK_COMPARE_OP_LESS,				VK_COMPARE_OP_GREATER,			VK_COMPARE_OP_LESS_OR_EQUAL,	VK_COMPARE_OP_NEVER },
		{ VK_COMPARE_OP_LESS,				VK_COMPARE_OP_LESS_OR_EQUAL,	VK_COMPARE_OP_GREATER,			VK_COMPARE_OP_EQUAL },
		{ VK_COMPARE_OP_LESS,				VK_COMPARE_OP_LESS,				VK_COMPARE_OP_ALWAYS,			VK_COMPARE_OP_NOT_EQUAL },
		{ VK_COMPARE_OP_LESS,				VK_COMPARE_OP_NEVER,			VK_COMPARE_OP_NOT_EQUAL,		VK_COMPARE_OP_ALWAYS },
		{ VK_COMPARE_OP_NEVER,				VK_COMPARE_OP_NOT_EQUAL,		VK_COMPARE_OP_ALWAYS,			VK_COMPARE_OP_ALWAYS },
		{ VK_COMPARE_OP_LESS,				VK_COMPARE_OP_ALWAYS,			VK_COMPARE_OP_GREATER_OR_EQUAL,	VK_COMPARE_OP_LESS },
		{ VK_COMPARE_OP_NEVER,				VK_COMPARE_OP_GREATER_OR_EQUAL,	VK_COMPARE_OP_NEVER,			VK_COMPARE_OP_EQUAL },
		{ VK_COMPARE_OP_NEVER,				VK_COMPARE_OP_NEVER,			VK_COMPARE_OP_LESS,				VK_COMPARE_OP_GREATER },
		{ VK_COMPARE_OP_NEVER,				VK_COMPARE_OP_LESS_OR_EQUAL,	VK_COMPARE_OP_EQUAL,			VK_COMPARE_OP_NOT_EQUAL },
		{ VK_COMPARE_OP_NEVER,				VK_COMPARE_OP_LESS,				VK_COMPARE_OP_GREATER_OR_EQUAL,	VK_COMPARE_OP_LESS_OR_EQUAL },
		{ VK_COMPARE_OP_NEVER,				VK_COMPARE_OP_ALWAYS,			VK_COMPARE_OP_GREATER,			VK_COMPARE_OP_GREATER_OR_EQUAL },
		{ VK_COMPARE_OP_ALWAYS,				VK_COMPARE_OP_EQUAL,			VK_COMPARE_OP_ALWAYS,			VK_COMPARE_OP_NEVER },
		{ VK_COMPARE_OP_ALWAYS,				VK_COMPARE_OP_NEVER,			VK_COMPARE_OP_EQUAL,			VK_COMPARE_OP_LESS },
		{ VK_COMPARE_OP_ALWAYS,				VK_COMPARE_OP_GREATER,			VK_COMPARE_OP_LESS,				VK_COMPARE_OP_ALWAYS },
		{ VK_COMPARE_OP_ALWAYS,				VK_COMPARE_OP_ALWAYS,			VK_COMPARE_OP_NEVER,			VK_COMPARE_OP_GREATER },
		{ VK_COMPARE_OP_ALWAYS,				VK_COMPARE_OP_LESS_OR_EQUAL,	VK_COMPARE_OP_NOT_EQUAL,		VK_COMPARE_OP_EQUAL },
		{ VK_COMPARE_OP_LESS_OR_EQUAL,		VK_COMPARE_OP_NEVER,			VK_COMPARE_OP_GREATER,			VK_COMPARE_OP_NOT_EQUAL },
		{ VK_COMPARE_OP_NEVER,				VK_COMPARE_OP_EQUAL,			VK_COMPARE_OP_LESS_OR_EQUAL,	VK_COMPARE_OP_LESS },
		{ VK_COMPARE_OP_EQUAL,				VK_COMPARE_OP_GREATER_OR_EQUAL,	VK_COMPARE_OP_ALWAYS,			VK_COMPARE_OP_NEVER },
		{ VK_COMPARE_OP_GREATER,			VK_COMPARE_OP_ALWAYS,			VK_COMPARE_OP_LESS,				VK_COMPARE_OP_NOT_EQUAL },
		{ VK_COMPARE_OP_GREATER,			VK_COMPARE_OP_NEVER,			VK_COMPARE_OP_GREATER_OR_EQUAL,	VK_COMPARE_OP_EQUAL },
		{ VK_COMPARE_OP_EQUAL,				VK_COMPARE_OP_ALWAYS,			VK_COMPARE_OP_EQUAL,			VK_COMPARE_OP_LESS_OR_EQUAL },
		{ VK_COMPARE_OP_LESS_OR_EQUAL,		VK_COMPARE_OP_GREATER,			VK_COMPARE_OP_ALWAYS,			VK_COMPARE_OP_GREATER },
		{ VK_COMPARE_OP_NEVER,				VK_COMPARE_OP_NOT_EQUAL,		VK_COMPARE_OP_NOT_EQUAL,		VK_COMPARE_OP_NEVER },
		{ VK_COMPARE_OP_ALWAYS,				VK_COMPARE_OP_LESS,				VK_COMPARE_OP_GREATER,			VK_COMPARE_OP_GREATER },
		{ VK_COMPARE_OP_ALWAYS,				VK_COMPARE_OP_NOT_EQUAL,		VK_COMPARE_OP_NEVER,			VK_COMPARE_OP_NOT_EQUAL },
		{ VK_COMPARE_OP_GREATER_OR_EQUAL,	VK_COMPARE_OP_ALWAYS,			VK_COMPARE_OP_NOT_EQUAL,		VK_COMPARE_OP_ALWAYS },
		{ VK_COMPARE_OP_LESS_OR_EQUAL,		VK_COMPARE_OP_ALWAYS,			VK_COMPARE_OP_LESS_OR_EQUAL,	VK_COMPARE_OP_GREATER },
		{ VK_COMPARE_OP_LESS,				VK_COMPARE_OP_GREATER_OR_EQUAL,	VK_COMPARE_OP_LESS,				VK_COMPARE_OP_GREATER },
		{ VK_COMPARE_OP_ALWAYS,				VK_COMPARE_OP_EQUAL,			VK_COMPARE_OP_LESS_OR_EQUAL,	VK_COMPARE_OP_GREATER_OR_EQUAL },
		{ VK_COMPARE_OP_ALWAYS,				VK_COMPARE_OP_GREATER_OR_EQUAL,	VK_COMPARE_OP_GREATER_OR_EQUAL,	VK_COMPARE_OP_LESS_OR_EQUAL },
		{ VK_COMPARE_OP_GREATER_OR_EQUAL,	VK_COMPARE_OP_GREATER_OR_EQUAL,	VK_COMPARE_OP_NEVER,			VK_COMPARE_OP_LESS },
		{ VK_COMPARE_OP_GREATER_OR_EQUAL,	VK_COMPARE_OP_NEVER,			VK_COMPARE_OP_GREATER,			VK_COMPARE_OP_NEVER },
		{ VK_COMPARE_OP_LESS,				VK_COMPARE_OP_GREATER,			VK_COMPARE_OP_EQUAL,			VK_COMPARE_OP_EQUAL },
		{ VK_COMPARE_OP_NEVER,				VK_COMPARE_OP_GREATER,			VK_COMPARE_OP_ALWAYS,			VK_COMPARE_OP_GREATER_OR_EQUAL },
		{ VK_COMPARE_OP_NOT_EQUAL,			VK_COMPARE_OP_NOT_EQUAL,		VK_COMPARE_OP_GREATER,			VK_COMPARE_OP_ALWAYS },
		{ VK_COMPARE_OP_NOT_EQUAL,			VK_COMPARE_OP_LESS_OR_EQUAL,	VK_COMPARE_OP_NOT_EQUAL,		VK_COMPARE_OP_GREATER }
	};

	const bool						colorAttachmentEnabled[]	= { true, false };

	const VkPrimitiveTopology		primitiveTopologies[]		= { VK_PRIMITIVE_TOPOLOGY_POINT_LIST, VK_PRIMITIVE_TOPOLOGY_LINE_LIST, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST };

	de::MovePtr<tcu::TestCaseGroup>	depthTests					(new tcu::TestCaseGroup(testCtx, "depth", "Depth tests"));
	de::MovePtr<tcu::TestCaseGroup>	noColorAttachmentTests		(new tcu::TestCaseGroup(testCtx, "nocolor", "Depth tests with no color attachment"));

	// Tests for format features
	if (!isConstructionTypeLibrary(pipelineConstructionType))
	{
		de::MovePtr<tcu::TestCaseGroup> formatFeaturesTests (new tcu::TestCaseGroup(testCtx, "format_features", "Checks depth format features"));

		// Formats that must be supported in all implementations
		addFunctionCase(formatFeaturesTests.get(),
				"support_d16_unorm",
				"Tests if VK_FORMAT_D16_UNORM is supported as depth/stencil attachment format",
				testSupportsDepthStencilFormat,
				VK_FORMAT_D16_UNORM);

		// Sets where at least one of the formats must be supported
		const VkFormat	depthOnlyFormats[]		= { VK_FORMAT_X8_D24_UNORM_PACK32, VK_FORMAT_D32_SFLOAT };
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

	for (deUint32 colorAttachmentEnabledIdx = 0; colorAttachmentEnabledIdx < DE_LENGTH_OF_ARRAY(colorAttachmentEnabled); colorAttachmentEnabledIdx++)
	{
		const bool colorEnabled = colorAttachmentEnabled[colorAttachmentEnabledIdx];

		// Tests for format and compare operators
		{
			de::MovePtr<tcu::TestCaseGroup> formatTests (new tcu::TestCaseGroup(testCtx, "format", "Uses different depth formats"));

			for (size_t formatNdx = 0; formatNdx < DE_LENGTH_OF_ARRAY(depthFormats); formatNdx++)
			{
				const bool		hasDepth					= tcu::hasDepthComponent(mapVkFormat(depthFormats[formatNdx]).order);
				const bool		hasStencil					= tcu::hasStencilComponent(mapVkFormat(depthFormats[formatNdx]).order);
				const int		separateLayoutsLoopCount	= (hasDepth && hasStencil) ? 2 : 1;

				for (int separateDepthStencilLayouts = 0; separateDepthStencilLayouts < separateLayoutsLoopCount; ++separateDepthStencilLayouts)
				{
					const bool			useSeparateDepthStencilLayouts	= bool(separateDepthStencilLayouts);

					de::MovePtr<tcu::TestCaseGroup>	formatTest		(new tcu::TestCaseGroup(testCtx,
								(getFormatCaseName(depthFormats[formatNdx]) + ((useSeparateDepthStencilLayouts) ? "_separate_layouts" : "")).c_str(),
								(std::string("Uses format ") + getFormatName(depthFormats[formatNdx]) + ((useSeparateDepthStencilLayouts) ? " with separate depth/stencil layouts" : "")).c_str()));
					de::MovePtr<tcu::TestCaseGroup>	compareOpsTests	(new tcu::TestCaseGroup(testCtx, "compare_ops", "Combines depth compare operators"));

					for (size_t topologyNdx = 0; topologyNdx < DE_LENGTH_OF_ARRAY(primitiveTopologies); topologyNdx++) {
						const std::string topologyName = getTopologyName(primitiveTopologies[topologyNdx]) + "_";
						for (size_t opsNdx = 0; opsNdx < DE_LENGTH_OF_ARRAY(depthOps); opsNdx++)
						{
							compareOpsTests->addChild(new DepthTest(testCtx,
										topologyName + getCompareOpsName(depthOps[opsNdx]),
										getCompareOpsDescription(depthOps[opsNdx]),
										pipelineConstructionType,
										depthFormats[formatNdx],
										depthOps[opsNdx],
										useSeparateDepthStencilLayouts,
										primitiveTopologies[topologyNdx],
										false,
										0.0f,
										1.0f));

							compareOpsTests->addChild(new DepthTest(testCtx,
										topologyName + getCompareOpsName(depthOps[opsNdx]) + "_depth_bounds_test",
										getCompareOpsDescription(depthOps[opsNdx]) + " with depth bounds test enabled",
										pipelineConstructionType,
										depthFormats[formatNdx],
										depthOps[opsNdx],
										useSeparateDepthStencilLayouts,
										primitiveTopologies[topologyNdx],
										true,
										0.1f,
										0.25f,
										true,
										false,
										colorEnabled));
						}
					}
					// Special VkPipelineDepthStencilStateCreateInfo known to have issues
					{
						const VkCompareOp depthOpsSpecial[DepthTest::QUAD_COUNT] = { VK_COMPARE_OP_NEVER, VK_COMPARE_OP_NEVER, VK_COMPARE_OP_NEVER, VK_COMPARE_OP_NEVER };

						compareOpsTests->addChild(new DepthTest(testCtx,
									"never_zerodepthbounds_depthdisabled_stencilenabled",
									"special VkPipelineDepthStencilStateCreateInfo",
									pipelineConstructionType,
									depthFormats[formatNdx],
									depthOpsSpecial,
									useSeparateDepthStencilLayouts,
									VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
									true,
									0.0f,
									0.0f,
									false,
									true,
									colorEnabled));
					}
					formatTest->addChild(compareOpsTests.release());

					// Test case with depth test enabled, but depth write disabled
					de::MovePtr<tcu::TestCaseGroup>	depthTestDisabled(new tcu::TestCaseGroup(testCtx, "depth_test_disabled", "Test for disabled depth test"));
					{
						const VkCompareOp depthOpsDepthTestDisabled[DepthTest::QUAD_COUNT] = { VK_COMPARE_OP_NEVER, VK_COMPARE_OP_LESS, VK_COMPARE_OP_GREATER, VK_COMPARE_OP_ALWAYS };
						depthTestDisabled->addChild(new DepthTest(testCtx,
									"depth_write_enabled",
									"Depth writes should not occur if depth test is disabled",
									pipelineConstructionType,
									depthFormats[formatNdx],
									depthOpsDepthTestDisabled,
									useSeparateDepthStencilLayouts,
									VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
									false,			/* depthBoundsTestEnable */
									0.0f,			/* depthBoundMin*/
									1.0f,			/* depthBoundMax*/
									false,			/* depthTestEnable */
									false,			/* stencilTestEnable */
									colorEnabled	/* colorAttachmentEnable */));
					}
					formatTest->addChild(depthTestDisabled.release());

					// Test case with depth buffer placed in local memory
					de::MovePtr<tcu::TestCaseGroup>	hostVisibleTests(new tcu::TestCaseGroup(testCtx, "host_visible", "Test for disabled depth test"));
					{
						const VkCompareOp hostVisibleOps[DepthTest::QUAD_COUNT] = { VK_COMPARE_OP_NEVER, VK_COMPARE_OP_LESS, VK_COMPARE_OP_GREATER, VK_COMPARE_OP_ALWAYS };

						hostVisibleTests->addChild(new DepthTest(testCtx,
									"local_memory_depth_buffer",
									"Depth buffer placed in local memory",
									pipelineConstructionType,
									depthFormats[formatNdx],
									hostVisibleOps,
									useSeparateDepthStencilLayouts,
									VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
									false,					/* depthBoundsTestEnable */
									0.0f,					/* depthBoundMin*/
									1.0f,					/* depthBoundMax*/
									true,					/* depthTestEnable */
									false,					/* stencilTestEnable */
									colorEnabled,			/* colorAttachmentEnable */
									true,					/* hostVisible */
									tcu::UVec2(256, 256)	/*renderSize*/));
					}

					formatTest->addChild(hostVisibleTests.release());
					formatTests->addChild(formatTest.release());
				}
			}
			if (colorEnabled)
				depthTests->addChild(formatTests.release());
			else
				noColorAttachmentTests->addChild(formatTests.release());
		}
	}
	depthTests->addChild(noColorAttachmentTests.release());

#ifndef CTS_USES_VULKANSC
	de::MovePtr<tcu::TestCaseGroup>	depthClipControlTests		(new tcu::TestCaseGroup(testCtx, "depth_clip_control", "Depth tests with depth clip control enabled"));
	{
		const VkCompareOp compareOps[] = { VK_COMPARE_OP_ALWAYS, VK_COMPARE_OP_LESS };

		const struct
		{
			const DepthClipControlCase	viewportCase;
			const std::string			suffix;
		} kViewportCases[] =
		{
			{ DepthClipControlCase::NORMAL,					""								},
			{ DepthClipControlCase::NORMAL_W,				"_different_w"					},
			{ DepthClipControlCase::BEFORE_STATIC,			"_viewport_before_static"		},
			{ DepthClipControlCase::BEFORE_DYNAMIC,			"_viewport_before_dynamic"		},
			{ DepthClipControlCase::BEFORE_TWO_DYNAMICS,	"_viewport_before_two_dynamic"	},
			{ DepthClipControlCase::AFTER_DYNAMIC,			"_viewport_after_dynamic"		},
		};

		for (const auto& viewportCase : kViewportCases)
			for (const auto& format : depthFormats)
				for (const auto& compareOp : compareOps)
				{
					std::string testName = getFormatCaseName(format) + "_" + de::toLower(std::string(getCompareOpName(compareOp)).substr(14)) + viewportCase.suffix;

					const VkCompareOp ops[DepthTest::QUAD_COUNT] = { compareOp, compareOp, compareOp, compareOp };
					depthClipControlTests->addChild(new DepthTest(testCtx, testName, "", pipelineConstructionType, format, ops,
													false, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, false, 0.0f, 1.0f, true, false, true, false, tcu::UVec2(32,32), viewportCase.viewportCase));
				}
	}
	depthTests->addChild(depthClipControlTests.release());
#endif // CTS_USES_VULKANSC

	return depthTests.release();
}

} // pipeline
} // vkt
