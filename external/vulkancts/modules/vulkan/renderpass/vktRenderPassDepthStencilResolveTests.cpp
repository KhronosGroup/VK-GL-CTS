/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2018 The Khronos Group Inc.
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
 * \brief VK_KHR_depth_stencil_resolve tests.
 *//*--------------------------------------------------------------------*/

#include "vktRenderPassDepthStencilResolveTests.hpp"
#include "vktRenderPassTestsUtil.hpp"

#include "vktTestCaseUtil.hpp"
#include "vktTestGroupUtil.hpp"

#include "vkDefs.hpp"
#include "vkDeviceUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkPlatform.hpp"
#include "vkPrograms.hpp"
#include "vkQueryUtil.hpp"
#include "vkRef.hpp"
#include "vkRefUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"

#include "tcuImageCompare.hpp"
#include "tcuFormatUtil.hpp"
#include "tcuResultCollector.hpp"
#include "tcuTestLog.hpp"
#include "tcuTextureUtil.hpp"

#include "deUniquePtr.hpp"
#include "deSharedPtr.hpp"
#include "deMath.h"
#include <limits>

using namespace vk;

using tcu::Vec4;
using tcu::TestLog;

typedef de::SharedPtr<vk::Unique<VkImage> >		VkImageSp;
typedef de::SharedPtr<vk::Unique<VkImageView> >	VkImageViewSp;
typedef de::SharedPtr<vk::Unique<VkBuffer> >	VkBufferSp;
typedef de::SharedPtr<vk::Unique<VkPipeline> >	VkPipelineSp;
typedef de::SharedPtr<Allocation>				AllocationSp;

namespace vkt
{
namespace
{

using namespace renderpass;

template<typename T>
de::SharedPtr<T> safeSharedPtr (T* ptr)
{
	try
	{
		return de::SharedPtr<T>(ptr);
	}
	catch (...)
	{
		delete ptr;
		throw;
	}
}

enum VerifyBuffer
{
	VB_DEPTH = 0,
	VB_STENCIL,
};

struct TestConfig
{
	TestConfig(VkFormat format_,
			   deUint32 width_,
			   deUint32 height_,
			   deUint32 depth_,
			   VkImageAspectFlags aspectFlag_,
			   deUint32 sampleCount_,
			   VkResolveModeFlagBitsKHR depthResolveMode_,
			   VkResolveModeFlagBitsKHR stencilResolveMode_,
			   VerifyBuffer verifyBuffer_,
			   float expectedValue_)
		: format(format_)
		, width(width_)
		, height(height_)
		, depth(depth_)
		, aspectFlag(aspectFlag_)
		, sampleCount(sampleCount_)
		, depthResolveMode(depthResolveMode_)
		, stencilResolveMode(stencilResolveMode_)
		, verifyBuffer(verifyBuffer_)
		, expectedValue(expectedValue_)
	{
	}

	VkFormat					format;
	deUint32					width;
	deUint32					height;
	deUint32					depth;
	VkImageAspectFlags			aspectFlag;
	deUint32					sampleCount;
	VkResolveModeFlagBitsKHR	depthResolveMode;
	VkResolveModeFlagBitsKHR	stencilResolveMode;
	VerifyBuffer				verifyBuffer;
	float						expectedValue;
};

class DepthStencilRenderPassTestInstance : public TestInstance
{
public:
								DepthStencilRenderPassTestInstance	(Context& context, TestConfig config);
								~DepthStencilRenderPassTestInstance	(void);

	tcu::TestStatus				iterate (void);

private:
	bool						isFeaturesSupported				(void);
	VkImageSp					createImage						(deUint32 sampleCount, VkImageUsageFlags additionalUsage = 0u);
	VkImageViewSp				createImageView					(VkImageSp image);
	AllocationSp				createImageMemory				(VkImageSp image);
	Move<VkRenderPass>			createRenderPass				(void);
	Move<VkFramebuffer>			createFramebuffer				(void);
	Move<VkPipelineLayout>		createRenderPipelineLayout		(void);
	Move<VkPipeline>			createRenderPipeline			(void);
	AllocationSp				createBufferMemory				(void);
	VkBufferSp					createBuffer					(void);

	VkSampleCountFlagBits		sampleCountBitFromSampleCount	(deUint32 count) const;

	void						submit							(void);
	bool						verify							(void);

private:
	const VkFormat					m_format;
	const VkImageAspectFlags		m_aspectFlag;
	const VkResolveModeFlagBitsKHR	m_depthResolveMode;
	const VkResolveModeFlagBitsKHR	m_stencilResolveMode;
	VerifyBuffer					m_verifyBuffer;
	float							m_expectedValue;

	const deUint32					m_sampleCount;
	const deUint32					m_width;
	const deUint32					m_height;
	const deUint32					m_depth;

	const bool						m_featureSupported;
	const InstanceInterface&		m_vki;
	const DeviceInterface&			m_vkd;
	VkDevice						m_device;
	VkPhysicalDevice				m_physicalDevice;

	VkImageSp						m_multisampleImage;
	AllocationSp					m_multisampleImageMemory;
	VkImageViewSp					m_multisampleImageView;

	VkImageSp						m_singlesampleImage;
	AllocationSp					m_singlesampleImageMemory;
	VkImageViewSp					m_singlesampleImageView;

	const Unique<VkRenderPass>		m_renderPass;
	const Unique<VkFramebuffer>		m_framebuffer;

	const Unique<VkPipelineLayout>	m_renderPipelineLayout;
	const Unique<VkPipeline>		m_renderPipeline;

	VkBufferSp						m_buffer;
	AllocationSp					m_bufferMemory;

	const Unique<VkCommandPool>		m_commandPool;
};

DepthStencilRenderPassTestInstance::DepthStencilRenderPassTestInstance (Context& context, TestConfig config)
	: TestInstance				(context)
	, m_format					(config.format)
	, m_aspectFlag				(config.aspectFlag)
	, m_depthResolveMode		(config.depthResolveMode)
	, m_stencilResolveMode		(config.stencilResolveMode)
	, m_verifyBuffer			(config.verifyBuffer)
	, m_expectedValue			(config.expectedValue)
	, m_sampleCount				(config.sampleCount)
	, m_width					(config.width)
	, m_height					(config.height)
	, m_depth					(config.depth)
	, m_featureSupported		(isFeaturesSupported())
	, m_vki						(context.getInstanceInterface())
	, m_vkd						(context.getDeviceInterface())
	, m_device					(context.getDevice())
	, m_physicalDevice			(context.getPhysicalDevice())

	, m_multisampleImage		(createImage(m_sampleCount))
	, m_multisampleImageMemory	(createImageMemory(m_multisampleImage))
	, m_multisampleImageView	(createImageView(m_multisampleImage))

	, m_singlesampleImage		(createImage(1, VK_IMAGE_USAGE_TRANSFER_SRC_BIT))
	, m_singlesampleImageMemory	(createImageMemory(m_singlesampleImage))
	, m_singlesampleImageView	(createImageView(m_singlesampleImage))

	, m_renderPass				(createRenderPass())
	, m_framebuffer				(createFramebuffer())

	, m_renderPipelineLayout	(createRenderPipelineLayout())
	, m_renderPipeline			(createRenderPipeline())

	, m_buffer					(createBuffer())
	, m_bufferMemory			(createBufferMemory())

	, m_commandPool				(createCommandPool(context.getDeviceInterface(), context.getDevice(), VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, context.getUniversalQueueFamilyIndex()))
{
}

DepthStencilRenderPassTestInstance::~DepthStencilRenderPassTestInstance (void)
{
}

bool DepthStencilRenderPassTestInstance::isFeaturesSupported()
{
	m_context.requireDeviceExtension("VK_KHR_depth_stencil_resolve");

	VkPhysicalDeviceDepthStencilResolvePropertiesKHR dsResolveProperties;
	deMemset(&dsResolveProperties, 0, sizeof(VkPhysicalDeviceDepthStencilResolvePropertiesKHR));
	dsResolveProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEPTH_STENCIL_RESOLVE_PROPERTIES_KHR;
	dsResolveProperties.pNext = DE_NULL;

	VkPhysicalDeviceProperties2 deviceProperties;
	deviceProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
	deviceProperties.pNext = &dsResolveProperties;

	// perform query to get supported float control properties
	const VkPhysicalDevice			physicalDevice		= m_context.getPhysicalDevice();
	const vk::InstanceInterface&	instanceInterface	= m_context.getInstanceInterface();
	instanceInterface.getPhysicalDeviceProperties2(physicalDevice, &deviceProperties);

	// check if both modes are supported
	if ((m_depthResolveMode != VK_RESOLVE_MODE_NONE_KHR) &&
		!(m_depthResolveMode & dsResolveProperties.supportedDepthResolveModes))
		TCU_THROW(NotSupportedError, "Depth resolve mode not supported");
	if ((m_stencilResolveMode != VK_RESOLVE_MODE_NONE_KHR) &&
		!(m_stencilResolveMode & dsResolveProperties.supportedStencilResolveModes))
		TCU_THROW(NotSupportedError, "Stencil resolve mode not supported");

	// check if the implementation supports setting the depth and stencil resolve
	// modes to different values when one of those modes is VK_RESOLVE_MODE_NONE_KHR
	if (dsResolveProperties.independentResolveNone)
	{
		if ((!dsResolveProperties.independentResolve) &&
			(m_depthResolveMode != m_stencilResolveMode) &&
			(m_depthResolveMode != VK_RESOLVE_MODE_NONE_KHR) &&
			(m_stencilResolveMode != VK_RESOLVE_MODE_NONE_KHR))
			TCU_THROW(NotSupportedError, "Implementation doesn't support diferent resolve modes");
	}
	else if (m_depthResolveMode != m_stencilResolveMode)
	{
		// when independentResolveNone is VK_FALSE then both modes must be the same
		TCU_THROW(NotSupportedError, "Implementation doesn't support diferent resolve modes");
	}

	// check if the implementation supports all combinations of the supported depth and stencil resolve modes
	if (!dsResolveProperties.independentResolve && (m_depthResolveMode != m_stencilResolveMode))
		TCU_THROW(NotSupportedError, "Implementation doesn't support diferent resolve modes");

	return true;
}

VkImageSp DepthStencilRenderPassTestInstance::createImage (deUint32 sampleCount, VkImageUsageFlags additionalUsage)
{
	const tcu::TextureFormat	format(mapVkFormat(m_format));
	const VkImageTiling			imageTiling(VK_IMAGE_TILING_OPTIMAL);
	VkSampleCountFlagBits		sampleCountBit(sampleCountBitFromSampleCount(sampleCount));
	VkImageUsageFlags			usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | additionalUsage;

	VkImageFormatProperties imageFormatProperties;
	if (m_vki.getPhysicalDeviceImageFormatProperties(m_physicalDevice, m_format, VK_IMAGE_TYPE_2D, imageTiling,
													 usage, 0u, &imageFormatProperties) == VK_ERROR_FORMAT_NOT_SUPPORTED)
	{
		TCU_THROW(NotSupportedError, "Format not supported");
	}
	if (imageFormatProperties.sampleCounts < sampleCount)
	{
		TCU_THROW(NotSupportedError, "Sample count not supported");
	}

	const VkExtent3D imageExtent =
	{
		m_width,
		m_height,
		m_depth
	};

	if (!(tcu::hasDepthComponent(format.order) || tcu::hasStencilComponent(format.order)))
		TCU_THROW(NotSupportedError, "Format can't be used as depth/stencil attachment");

	if (imageFormatProperties.maxExtent.width < imageExtent.width
		|| imageFormatProperties.maxExtent.height < imageExtent.height
		|| imageFormatProperties.maxExtent.depth < imageExtent.depth
		|| ((imageFormatProperties.sampleCounts & sampleCountBit) == 0))
	{
		TCU_THROW(NotSupportedError, "Image type not supported");
	}

	const VkImageCreateInfo pCreateInfo =
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		DE_NULL,
		0u,
		VK_IMAGE_TYPE_2D,
		m_format,
		imageExtent,
		1u,
		1u,
		sampleCountBit,
		imageTiling,
		usage,
		VK_SHARING_MODE_EXCLUSIVE,
		0u,
		DE_NULL,
		VK_IMAGE_LAYOUT_UNDEFINED
	};

	return safeSharedPtr(new Unique<VkImage>(vk::createImage(m_vkd, m_device, &pCreateInfo)));
}

AllocationSp DepthStencilRenderPassTestInstance::createImageMemory (VkImageSp image)
{
	Allocator& allocator = m_context.getDefaultAllocator();

	de::MovePtr<Allocation> allocation (allocator.allocate(getImageMemoryRequirements(m_vkd, m_device, **image), MemoryRequirement::Any));
	VK_CHECK(m_vkd.bindImageMemory(m_device, **image, allocation->getMemory(), allocation->getOffset()));
	return safeSharedPtr(allocation.release());
}

VkImageViewSp DepthStencilRenderPassTestInstance::createImageView (VkImageSp image)
{
	const VkImageSubresourceRange	range =
	{
		m_aspectFlag,
		0u,
		1u,
		0u,
		1u
	};

	const VkImageViewCreateInfo pCreateInfo =
	{
		VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		DE_NULL,
		0u,
		**image,
		VK_IMAGE_VIEW_TYPE_2D,
		m_format,
		makeComponentMappingRGBA(),
		range,
	};
	return safeSharedPtr(new Unique<VkImageView>(vk::createImageView(m_vkd, m_device, &pCreateInfo)));
}

Move<VkRenderPass> DepthStencilRenderPassTestInstance::createRenderPass (void)
{
	const VkSampleCountFlagBits samples(sampleCountBitFromSampleCount(m_sampleCount));

	const AttachmentDescription2 multisampleAttachment		// VkAttachmentDescription2KHR
	(
															// VkStructureType					sType;
		DE_NULL,											// const void*						pNext;
		0u,													// VkAttachmentDescriptionFlags		flags;
		m_format,											// VkFormat							format;
		samples,											// VkSampleCountFlagBits			samples;
		VK_ATTACHMENT_LOAD_OP_CLEAR,						// VkAttachmentLoadOp				loadOp;
		VK_ATTACHMENT_STORE_OP_DONT_CARE,					// VkAttachmentStoreOp				storeOp;
		VK_ATTACHMENT_LOAD_OP_CLEAR,						// VkAttachmentLoadOp				stencilLoadOp;
		VK_ATTACHMENT_STORE_OP_DONT_CARE,					// VkAttachmentStoreOp				stencilStoreOp;
		VK_IMAGE_LAYOUT_UNDEFINED,							// VkImageLayout					initialLayout;
		VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL				// VkImageLayout					finalLayout;
	);
	const AttachmentReference2 multisampleAttachmentRef		// VkAttachmentReference2KHR
	(
															// VkStructureType					sType;
		DE_NULL,											// const void*						pNext;
		0u,													// deUint32							attachment;
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,	// VkImageLayout					layout;
		0u													// VkImageAspectFlags				aspectMask;
	);

	const AttachmentDescription2 singlesampleAttachment		// VkAttachmentDescription2KHR
	(
															// VkStructureType					sType;
		DE_NULL,											// const void*						pNext;
		0u,													// VkAttachmentDescriptionFlags		flags;
		m_format,											// VkFormat							format;
		VK_SAMPLE_COUNT_1_BIT,								// VkSampleCountFlagBits			samples;
		VK_ATTACHMENT_LOAD_OP_CLEAR,						// VkAttachmentLoadOp				loadOp;
		VK_ATTACHMENT_STORE_OP_STORE,						// VkAttachmentStoreOp				storeOp;
		VK_ATTACHMENT_LOAD_OP_CLEAR,						// VkAttachmentLoadOp				stencilLoadOp;
		VK_ATTACHMENT_STORE_OP_STORE,						// VkAttachmentStoreOp				stencilStoreOp;
		VK_IMAGE_LAYOUT_UNDEFINED,							// VkImageLayout					initialLayout;
		VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL				// VkImageLayout					finalLayout;
	);
	AttachmentReference2 singlesampleAttachmentRef			// VkAttachmentReference2KHR
	(
															// VkStructureType					sType;
		DE_NULL,											// const void*						pNext;
		1u,													// deUint32							attachment;
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,	// VkImageLayout					layout;
		0u													// VkImageAspectFlags				aspectMask;
	);

	std::vector<AttachmentDescription2> attachments;
	attachments.push_back(multisampleAttachment);
	attachments.push_back(singlesampleAttachment);

	VkSubpassDescriptionDepthStencilResolveKHR dsResolveDescription =
	{
		VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_DEPTH_STENCIL_RESOLVE_KHR,
		DE_NULL,																// const void*						pNext;
		m_depthResolveMode,														// VkResolveModeFlagBitsKHR			depthResolveMode;
		m_stencilResolveMode,													// VkResolveModeFlagBitsKHR			stencilResolveMode;
		&singlesampleAttachmentRef												// VkAttachmentReference2KHR		pDepthStencilResolveAttachment;
	};

	const SubpassDescription2 subpass					// VkSubpassDescription2KHR
	(
														// VkStructureType						sType;
		&dsResolveDescription,							// const void*							pNext;
		(VkSubpassDescriptionFlags)0,					// VkSubpassDescriptionFlags			flags;
		VK_PIPELINE_BIND_POINT_GRAPHICS,				// VkPipelineBindPoint					pipelineBindPoint;
		0u,												// deUint32								viewMask;
		0u,												// deUint32								inputAttachmentCount;
		DE_NULL,										// const VkAttachmentReference2KHR*		pInputAttachments;
		0u,												// deUint32								colorAttachmentCount;
		DE_NULL,										// const VkAttachmentReference2KHR*		pColorAttachments;
		DE_NULL,										// const VkAttachmentReference2KHR*		pResolveAttachments;
		&multisampleAttachmentRef,						// const VkAttachmentReference2KHR*		pDepthStencilAttachment;
		0u,												// deUint32								preserveAttachmentCount;
		DE_NULL											// const deUint32*						pPreserveAttachments;
	);

	const RenderPassCreateInfo2 renderPassCreator		// VkRenderPassCreateInfo2KHR
	(
														// VkStructureType						sType;
		DE_NULL,										// const void*							pNext;
		(VkRenderPassCreateFlags)0u,					// VkRenderPassCreateFlags				flags;
		(deUint32)attachments.size(),					// deUint32								attachmentCount;
		&attachments[0],								// const VkAttachmentDescription2KHR*	pAttachments;
		1u,												// deUint32								subpassCount;
		&subpass,										// const VkSubpassDescription2KHR*		pSubpasses;
		0u,												// deUint32								dependencyCount;
		DE_NULL,										// const VkSubpassDependency2KHR*		pDependencies;
		0u,												// deUint32								correlatedViewMaskCount;
		DE_NULL											// const deUint32*						pCorrelatedViewMasks;
	);

	return renderPassCreator.createRenderPass(m_vkd, m_device);
}

Move<VkFramebuffer> DepthStencilRenderPassTestInstance::createFramebuffer (void)
{
	std::vector<VkImageView> attachments;
	attachments.push_back(**m_multisampleImageView);
	attachments.push_back(**m_singlesampleImageView);

	const VkFramebufferCreateInfo createInfo =
	{
		VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
		DE_NULL,
		0u,

		*m_renderPass,
		(deUint32)attachments.size(),
		&attachments[0],

		m_width,
		m_height,
		1u
	};

	return vk::createFramebuffer(m_vkd, m_device, &createInfo);
}

Move<VkPipelineLayout> DepthStencilRenderPassTestInstance::createRenderPipelineLayout (void)
{
	const VkPushConstantRange pushConstant =
	{
		VK_SHADER_STAGE_FRAGMENT_BIT,
		0u,
		4u
	};
	bool constantNeeded = (m_verifyBuffer == VB_STENCIL);
	const VkPipelineLayoutCreateInfo createInfo	=
	{
		VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		DE_NULL,
		(vk::VkPipelineLayoutCreateFlags)0,

		0u,
		DE_NULL,

		constantNeeded ? 1u : 0u,
		constantNeeded ? &pushConstant : DE_NULL
	};

	return vk::createPipelineLayout(m_vkd, m_device, &createInfo);
}

Move<VkPipeline> DepthStencilRenderPassTestInstance::createRenderPipeline (void)
{
	const bool testingStencil = (m_verifyBuffer == VB_STENCIL);
	const vk::BinaryCollection& binaryCollection = m_context.getBinaryCollection();

	const Unique<VkShaderModule> vertexShaderModule(createShaderModule(m_vkd, m_device, binaryCollection.get("quad-vert"), 0u));
	const Unique<VkShaderModule> fragmentShaderModule(createShaderModule(m_vkd, m_device, binaryCollection.get("quad-frag"), 0u));

	const VkPipelineVertexInputStateCreateInfo vertexInputState =
	{
		VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
		DE_NULL,
		(VkPipelineVertexInputStateCreateFlags)0u,

		0u,
		DE_NULL,

		0u,
		DE_NULL
	};
	const tcu::UVec2				renderArea	(m_width, m_height);
	const std::vector<VkViewport>	viewports	(1, makeViewport(renderArea));
	const std::vector<VkRect2D>		scissors	(1, makeRect2D(renderArea));

	const VkPipelineMultisampleStateCreateInfo multisampleState =
	{
		VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
		DE_NULL,
		(VkPipelineMultisampleStateCreateFlags)0u,

		sampleCountBitFromSampleCount(m_sampleCount),
		VK_FALSE,
		0.0f,
		DE_NULL,
		VK_FALSE,
		VK_FALSE,
	};
	const VkPipelineDepthStencilStateCreateInfo depthStencilState =
	{
		VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
		DE_NULL,
		(VkPipelineDepthStencilStateCreateFlags)0u,

		VK_TRUE,							// depthTestEnable
		VK_TRUE,
		VK_COMPARE_OP_ALWAYS,
		VK_FALSE,
		testingStencil,						// stencilTestEnable
		{
			VK_STENCIL_OP_REPLACE,			// failOp
			VK_STENCIL_OP_REPLACE,			// passOp
			VK_STENCIL_OP_REPLACE,			// depthFailOp
			VK_COMPARE_OP_ALWAYS,			// compareOp
			0xFFu,							// compareMask
			0xFFu,							// writeMask
			1								// reference
		},
		{
			VK_STENCIL_OP_REPLACE,
			VK_STENCIL_OP_REPLACE,
			VK_STENCIL_OP_REPLACE,
			VK_COMPARE_OP_ALWAYS,
			0xFFu,
			0xFFu,
			1
		},
		0.0f,
		1.0f
	};

	std::vector<VkDynamicState> dynamicState;
	dynamicState.push_back(VK_DYNAMIC_STATE_STENCIL_REFERENCE);
	const VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,	// VkStructureType                      sType;
		DE_NULL,												// const void*                          pNext;
		(VkPipelineDynamicStateCreateFlags)0u,					// VkPipelineDynamicStateCreateFlags    flags;
		static_cast<deUint32>(dynamicState.size()),				// deUint32                             dynamicStateCount;
		&dynamicState[0]										// const VkDynamicState*                pDynamicStates;
	};

	return makeGraphicsPipeline(m_vkd,															// const DeviceInterface&                        vk
								m_device,														// const VkDevice                                device
								*m_renderPipelineLayout,										// const VkPipelineLayout                        pipelineLayout
								*vertexShaderModule,											// const VkShaderModule                          vertexShaderModule
								DE_NULL,														// const VkShaderModule                          tessellationControlShaderModule
								DE_NULL,														// const VkShaderModule                          tessellationEvalShaderModule
								DE_NULL,														// const VkShaderModule                          geometryShaderModule
								*fragmentShaderModule,											// const VkShaderModule                          fragmentShaderModule
								*m_renderPass,													// const VkRenderPass                            renderPass
								viewports,														// const std::vector<VkViewport>&                viewports
								scissors,														// const std::vector<VkRect2D>&                  scissors
								VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,							// const VkPrimitiveTopology                     topology
								0u,																// const deUint32                                subpass
								0u,																// const deUint32                                patchControlPoints
								&vertexInputState,												// const VkPipelineVertexInputStateCreateInfo*   vertexInputStateCreateInfo
								DE_NULL,														// const VkPipelineRasterizationStateCreateInfo* rasterizationStateCreateInfo
								&multisampleState,												// const VkPipelineMultisampleStateCreateInfo*   multisampleStateCreateInfo
								&depthStencilState,												// const VkPipelineDepthStencilStateCreateInfo*  depthStencilStateCreateInfo
								DE_NULL,														// const VkPipelineColorBlendStateCreateInfo*    colorBlendStateCreateInfo
								testingStencil ? &dynamicStateCreateInfo : DE_NULL);			// const VkPipelineDynamicStateCreateInfo*       dynamicStateCreateInfo
}

AllocationSp DepthStencilRenderPassTestInstance::createBufferMemory (void)
{
	Allocator&				allocator = m_context.getDefaultAllocator();
	de::MovePtr<Allocation> allocation(allocator.allocate(getBufferMemoryRequirements(m_vkd, m_device, **m_buffer), MemoryRequirement::HostVisible));
	VK_CHECK(m_vkd.bindBufferMemory(m_device, **m_buffer, allocation->getMemory(), allocation->getOffset()));
	return safeSharedPtr(allocation.release());
}

VkBufferSp DepthStencilRenderPassTestInstance::createBuffer (void)
{
	const VkBufferUsageFlags	bufferUsage			(VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
	const tcu::TextureFormat	textureFormat		(mapVkFormat(m_format));
	const VkDeviceSize			pixelSize			(textureFormat.getPixelSize());
	const VkBufferCreateInfo	createInfo			=
	{
		VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		DE_NULL,
		0u,

		m_width * m_height * pixelSize,
		bufferUsage,

		VK_SHARING_MODE_EXCLUSIVE,
		0u,
		DE_NULL
	};
	return safeSharedPtr(new Unique<VkBuffer>(vk::createBuffer(m_vkd, m_device, &createInfo)));
}

VkSampleCountFlagBits DepthStencilRenderPassTestInstance::sampleCountBitFromSampleCount (deUint32 count) const
{
	switch (count)
	{
		case 1:  return VK_SAMPLE_COUNT_1_BIT;
		case 2:  return VK_SAMPLE_COUNT_2_BIT;
		case 4:  return VK_SAMPLE_COUNT_4_BIT;
		case 8:  return VK_SAMPLE_COUNT_8_BIT;
		case 16: return VK_SAMPLE_COUNT_16_BIT;
		case 32: return VK_SAMPLE_COUNT_32_BIT;
		case 64: return VK_SAMPLE_COUNT_64_BIT;

		default:
			DE_FATAL("Invalid sample count");
			return (VkSampleCountFlagBits)0x0;
	}
}

void DepthStencilRenderPassTestInstance::submit (void)
{
	const DeviceInterface&						vkd					(m_context.getDeviceInterface());
	const VkDevice								device				(m_context.getDevice());
	const Unique<VkCommandBuffer>				commandBuffer		(allocateCommandBuffer(vkd, device, *m_commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));
	const RenderpassSubpass2::SubpassBeginInfo	subpassBeginInfo	(DE_NULL, VK_SUBPASS_CONTENTS_INLINE);
	const RenderpassSubpass2::SubpassEndInfo	subpassEndInfo		(DE_NULL);

	beginCommandBuffer(vkd, *commandBuffer);

	{
		VkClearValue clearValues[2];
		clearValues[0].depthStencil = makeClearDepthStencilValue(0.0f, 0x0);
		clearValues[1].depthStencil = clearValues[0].depthStencil;

		const VkRenderPassBeginInfo beginInfo =
		{
			VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
			DE_NULL,

			*m_renderPass,
			*m_framebuffer,

			{
				{ 0u, 0u },
				{ m_width, m_height }
			},

			2u,
			clearValues
		};
		RenderpassSubpass2::cmdBeginRenderPass(vkd, *commandBuffer, &beginInfo, &subpassBeginInfo);
	}

	// Render
	if (m_verifyBuffer == VB_DEPTH)
	{
		vkd.cmdBindPipeline(*commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_renderPipeline);
		vkd.cmdDraw(*commandBuffer, 6u, 1u, 0u, 0u);
	}
	else
	{
		// For stencil we do two passes to have different value for just first and last sample
		deInt32 sampleID = 0;
		for (deUint32 renderPass = 0 ; renderPass < 2 ; renderPass++)
		{
			deUint32 stencilReference = renderPass + 1;
			vkd.cmdBindPipeline(*commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_renderPipeline);
			vkd.cmdPushConstants(*commandBuffer, *m_renderPipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0u, sizeof(sampleID), &sampleID);
			vkd.cmdSetStencilReference(*commandBuffer, VK_STENCIL_FRONT_AND_BACK, stencilReference);
			vkd.cmdDraw(*commandBuffer, 6u, 1u, 0u, 0u);
			sampleID = deInt32(m_sampleCount) - 1;
		}
	}

	RenderpassSubpass2::cmdEndRenderPass(vkd, *commandBuffer, &subpassEndInfo);

	// Memory barriers between rendering and copying
	{
		const VkImageMemoryBarrier barrier =
		{
			VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
			DE_NULL,

			VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
			VK_ACCESS_TRANSFER_READ_BIT,

			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,

			VK_QUEUE_FAMILY_IGNORED,
			VK_QUEUE_FAMILY_IGNORED,

			**m_singlesampleImage,
			{
				m_aspectFlag,
				0u,
				1u,
				0u,
				1u
			}
		};

		vkd.cmdPipelineBarrier(*commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u, DE_NULL, 0u, DE_NULL, 1u, &barrier);
	}

	// Copy image memory to buffers
	const VkBufferImageCopy region =
	{
		0u,
		0u,
		0u,
		{
			(m_verifyBuffer == VB_DEPTH) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_STENCIL_BIT,
			0u,
			0u,
			1u,
		},
		{ 0u, 0u, 0u },
		{ m_width, m_height, 1u }
	};

	vkd.cmdCopyImageToBuffer(*commandBuffer, **m_singlesampleImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, **m_buffer, 1u, &region);

	// Memory barriers between copies and host access
	{
		const VkBufferMemoryBarrier barrier =
		{
			VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
			DE_NULL,

			VK_ACCESS_TRANSFER_WRITE_BIT,
			VK_ACCESS_HOST_READ_BIT,

			VK_QUEUE_FAMILY_IGNORED,
			VK_QUEUE_FAMILY_IGNORED,

			**m_buffer,
			0u,
			VK_WHOLE_SIZE
		};

		vkd.cmdPipelineBarrier(*commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0u, 0u, DE_NULL, 1u, &barrier, 0u, DE_NULL);
	}

	endCommandBuffer(vkd, *commandBuffer);

	submitCommandsAndWait(vkd, device, m_context.getUniversalQueue(), *commandBuffer);
}

bool DepthStencilRenderPassTestInstance::verify (void)
{
	deUint8*	pixelPtr	= static_cast<deUint8*>(m_bufferMemory->getHostPtr());
	float		epsilon		= 0.002f;

	if (m_verifyBuffer == VB_DEPTH)
	{
		float depth = 0.0f;
		for (deUint32 y = 0; y < m_height; y++)
		{
			for (deUint32 x = 0; x < m_width; x++)
			{
				// depth data in buffer is tightly packed, ConstPixelBufferAccess
				// coludn't be used for depth value extraction as it cant interpret
				// formats containing just depth component
				switch (m_format)
				{
				case VK_FORMAT_D16_UNORM:
				case VK_FORMAT_D16_UNORM_S8_UINT:
				{
					deUint16* value = reinterpret_cast<deUint16*>(pixelPtr);
					depth = (static_cast<float>(*value)) / 65535.0f;
					pixelPtr += 2;
					break;
				}
				case VK_FORMAT_X8_D24_UNORM_PACK32:
				case VK_FORMAT_D24_UNORM_S8_UINT:
				{
#if (DE_ENDIANNESS == DE_LITTLE_ENDIAN)
					deUint32 value = (((deUint32)pixelPtr[0]) <<  0u) |
									 (((deUint32)pixelPtr[1]) <<  8u) |
									 (((deUint32)pixelPtr[2]) << 16u);
#else
					deUint32 value = (((deUint32)pixelPtr[0]) << 16u) |
									 (((deUint32)pixelPtr[1]) <<  8u) |
									 (((deUint32)pixelPtr[2]) <<  0u);
#endif
					depth = static_cast<float>(value) / 16777215.0f;
					pixelPtr += 4;
					break;
				}
				case VK_FORMAT_D32_SFLOAT:
				case VK_FORMAT_D32_SFLOAT_S8_UINT:
					depth = *(reinterpret_cast<float*>(pixelPtr));
					pixelPtr += 4;
					break;
				default:
					DE_ASSERT(true);
				}

				float error = deFloatAbs(depth-m_expectedValue);
				if (error > epsilon)
				{
					m_context.getTestContext().getLog() << TestLog::Message
						<< "At (" << x << ", " << y << ") depth value is: " << depth
						<< " expected: " << m_expectedValue << TestLog::EndMessage;
					return false;
				}
			}
		}
	}
	else
	{
		// when stencil is tested we are discarding invocations and
		// because of that depth and stencil need to be tested separately

		deUint8 expectedValue = static_cast<deUint8>(m_expectedValue);
		for (deUint32 y = 0; y < m_height; y++)
		{
			for (deUint32 x = 0; x < m_width; x++)
			{
				deUint8 stencil = *pixelPtr++;
				if (stencil != expectedValue)
				{
					m_context.getTestContext().getLog() << TestLog::Message
						<< "At (" << x << ", " << y << ") stencil value is: "
						<< static_cast<deUint32>(stencil)
						<< " expected: " << static_cast<deUint32>(expectedValue)
						<< TestLog::EndMessage;
					return false;
				}
			}
		}
		m_context.getTestContext().getLog() << TestLog::Message
			<< "Stencil value is "
			<< static_cast<deUint32>(expectedValue)
			<< TestLog::EndMessage;
	}

	return true;
}

tcu::TestStatus DepthStencilRenderPassTestInstance::iterate (void)
{
	submit();
	return verify() ? tcu::TestStatus::pass("Pass") : tcu::TestStatus::fail("Fail");
}

struct Programs
{
	void init (vk::SourceCollections& dst, TestConfig config) const
	{
		dst.glslSources.add("quad-vert") << glu::VertexSource(
			"#version 450\n"
			"out gl_PerVertex {\n"
			"\tvec4 gl_Position;\n"
			"};\n"
			"highp float;\n"
			"void main (void) {\n"
			"\tgl_Position = vec4(((gl_VertexIndex + 2) / 3) % 2 == 0 ? -1.0 : 1.0,\n"
			"\t                   ((gl_VertexIndex + 1) / 3) % 2 == 0 ? -1.0 : 1.0, 0.0, 1.0);\n"
			"}\n");

		if (config.verifyBuffer == VB_DEPTH)
		{
			dst.glslSources.add("quad-frag") << glu::FragmentSource(
				"#version 450\n"
				"precision highp float;\n"
				"precision highp int;\n"
				"void main (void)\n"
				"{\n"
				"  float sampleIndex = float(gl_SampleID);\n"				// sampleIndex is integer in range <0, 63>
				"  float valueIndex = round(mod(sampleIndex, 4.0));\n"		// limit possible depth values - count to 4
				"  float value = valueIndex + 2.0;\n"						// value is one of [2, 3, 4, 5]
				"  value = round(exp2(value));\n"							// value is one of [4, 8, 16, 32]
				"  bool condition = (int(value) == 8);\n"					// select second sample value (to make it smallest)
				"  value = round(value - float(condition) * 6.0);\n"		// value is one of [4, 2, 16, 32]
				"  gl_FragDepth = value / 100.0;\n"							// sample depth is one of [0.04, 0.02, 0.16, 0.32]
				"}\n");
		}
		else
		{
			dst.glslSources.add("quad-frag") << glu::FragmentSource(
				"#version 450\n"
				"precision highp float;\n"
				"precision highp int;\n"
				"layout(push_constant) uniform PushConstant {\n"
				"  highp int sampleID;\n"
				"} pushConstants;\n"
				"void main (void)\n"
				"{\n"
				"  if(gl_SampleID != pushConstants.sampleID)\n"
				"    discard;\n"
				"  gl_FragDepth = 0.5;\n"
				"}\n");
		}
	}
};

void initTests (tcu::TestCaseGroup* group)
{
	struct FormatData
	{
		VkFormat		format;
		const char*		name;
		bool			hasDepth;
		bool			hasStencil;
	};
	FormatData formats[] =
	{
		{ VK_FORMAT_D16_UNORM,				"d16_unorm",			true,	false },
		{ VK_FORMAT_X8_D24_UNORM_PACK32,	"x8_d24_unorm_pack32",	true,	false },
		{ VK_FORMAT_D32_SFLOAT,				"d32_sfloat",			true,	false },
		{ VK_FORMAT_S8_UINT,				"s8_uint",				false,	true },
		{ VK_FORMAT_D16_UNORM_S8_UINT,		"d16_unorm_s8_uint",	true,	true },
		{ VK_FORMAT_D24_UNORM_S8_UINT,		"d24_unorm_s8_uint",	true,	true },
		{ VK_FORMAT_D32_SFLOAT_S8_UINT,		"d32_sfloat_s8_uint",	true,	true },
	};

	struct ResolveModeData
	{
		VkResolveModeFlagBitsKHR	flag;
		std::string					name;
	};
	ResolveModeData resolveModes[] =
	{
		{ VK_RESOLVE_MODE_NONE_KHR,				"none" },
		{ VK_RESOLVE_MODE_SAMPLE_ZERO_BIT_KHR,	"zero" },
		{ VK_RESOLVE_MODE_AVERAGE_BIT_KHR,		"average" },
		{ VK_RESOLVE_MODE_MIN_BIT_KHR,			"min" },
		{ VK_RESOLVE_MODE_MAX_BIT_KHR,			"max" },
	};
	struct ImageTestData
	{
		const char*		groupName;
		deUint32		width;
		deUint32		height;
		deUint32		depth;
	};
	ImageTestData imagesTestData[] =
	{
		{ "image_2d_32_32", 32, 32, 1 },
		{ "image_2d_49_13", 49, 13, 1 },
	};
	const deUint32 sampleCounts[] =
	{
		2u, 4u, 8u, 16u, 32u, 64u
	};
	const float depthExpectedValue[][6] =
	{
		// 2 samples	4			8			16			32			64
		{ 0.0f,			0.0f,		0.0f,		0.0f,		0.0f,		0.0f },		// RESOLVE_MODE_NONE
		{ 0.04f,		0.04f,		0.04f,		0.04f,		0.04f,		0.04f },	// RESOLVE_MODE_SAMPLE_ZERO_BIT
		{ 0.03f,		0.135f,		0.135f,		0.135f,		0.135f,		0.135f },	// RESOLVE_MODE_AVERAGE_BIT
		{ 0.02f,		0.02f,		0.02f,		0.02f,		0.02f,		0.02f },	// RESOLVE_MODE_MIN_BIT
		{ 0.04f,		0.32f,		0.32f,		0.32f,		0.32f,		0.32f },	// RESOLVE_MODE_MAX_BIT
	};
	const float stencilExpectedValue[][6] =
	{
		// 2 samples	4			8	,		16			32			64
		{ 0.0f,			0.0f,		0.0f,		0.0f,		0.0f,		0.0f },		// RESOLVE_MODE_NONE
		{ 1.0f,			1.0f,		1.0f,		1.0f,		1.0f,		1.0f },		// RESOLVE_MODE_SAMPLE_ZERO_BIT
		{ 0.0f,			0.0f,		0.0f,		0.0f,		0.0f,		0.0f },		// RESOLVE_MODE_AVERAGE_BIT - not supported
		{ 1.0f,			0.0f,		0.0f,		0.0f,		0.0f,		0.0f },		// RESOLVE_MODE_MIN_BIT
		{ 2.0f,			2.0f,		2.0f,		2.0f,		2.0f,		2.0f },		// RESOLVE_MODE_MAX_BIT
	};

	tcu::TestContext& testCtx(group->getTestContext());

	// iterate over image data
	for	 (deUint32 imageDataNdx = 0; imageDataNdx < DE_LENGTH_OF_ARRAY(imagesTestData); imageDataNdx++)
	{
		ImageTestData imageData = imagesTestData[imageDataNdx];

		// create test group for image data
		de::MovePtr<tcu::TestCaseGroup> imageGroup(new tcu::TestCaseGroup(testCtx, imageData.groupName, imageData.groupName));

		// iterate over sampleCounts
		for (size_t sampleCountNdx = 0; sampleCountNdx < DE_LENGTH_OF_ARRAY(sampleCounts); sampleCountNdx++)
		{
			const deUint32		sampleCount	(sampleCounts[sampleCountNdx]);
			const std::string	sampleName	("samples_" + de::toString(sampleCount));

			// create test group for sample count
			de::MovePtr<tcu::TestCaseGroup> sampleGroup(new tcu::TestCaseGroup(testCtx, sampleName.c_str(), sampleName.c_str()));

			// iterate over depth/stencil formats
			for (size_t formatNdx = 0; formatNdx < DE_LENGTH_OF_ARRAY(formats); formatNdx++)
			{
				const FormatData&			formatData	= formats[formatNdx];
				VkFormat					format		= formatData.format;
				const char*					formatName	= formatData.name;
				const bool					hasDepth	= formatData.hasDepth;
				const bool					hasStencil	= formatData.hasStencil;
				VkImageAspectFlags			aspectFlags	= (hasDepth * VK_IMAGE_ASPECT_DEPTH_BIT) |
														  (hasStencil * VK_IMAGE_ASPECT_STENCIL_BIT);

				// create test group for format
				de::MovePtr<tcu::TestCaseGroup> formatGroup(new tcu::TestCaseGroup(testCtx, formatName, formatName));

				// iterate over depth resolve modes
				for (size_t depthResolveModeNdx = 0; depthResolveModeNdx < DE_LENGTH_OF_ARRAY(resolveModes); depthResolveModeNdx++)
				{
					// iterate over stencil resolve modes
					for (size_t stencilResolveModeNdx = 0; stencilResolveModeNdx < DE_LENGTH_OF_ARRAY(resolveModes); stencilResolveModeNdx++)
					{
						// there is no average resolve mode for stencil - go to next iteration
						ResolveModeData& sResolve = resolveModes[stencilResolveModeNdx];
						if (sResolve.flag == VK_RESOLVE_MODE_AVERAGE_BIT_KHR)
							continue;

						// if pDepthStencilResolveAttachment is not NULL and does not have the value VK_ATTACHMENT_UNUSED,
						// depthResolveMode and stencilResolveMode must not both be VK_RESOLVE_MODE_NONE_KHR
						ResolveModeData& dResolve = resolveModes[depthResolveModeNdx];
						if ((dResolve.flag == VK_RESOLVE_MODE_NONE_KHR) && (sResolve.flag == VK_RESOLVE_MODE_NONE_KHR))
							continue;

						// if there is no depth or stencil component then resolve mode for both should be same
						if ((!hasDepth || !hasStencil) && (dResolve.flag != sResolve.flag))
							continue;

						std::string baseName = "depth_" + dResolve.name + "_stencil_" + sResolve.name;

						typedef InstanceFactory1<DepthStencilRenderPassTestInstance, TestConfig, Programs> DSResolveTestInstance;
						if (hasDepth)
						{
							std::string	name			= baseName + "_testing_depth";
							const char*	testName		= name.c_str();
							float		expectedValue	= depthExpectedValue[depthResolveModeNdx][sampleCountNdx];
							const TestConfig testConfig(format,
														imageData.width,
														imageData.height,
														imageData.depth,
														aspectFlags,
														sampleCount,
														dResolve.flag,
														sResolve.flag,
														VB_DEPTH,
														expectedValue);
							formatGroup->addChild(new DSResolveTestInstance(testCtx, tcu::NODETYPE_SELF_VALIDATE, testName, testName, testConfig));
						}
						if (hasStencil)
						{
							std::string	name			= baseName + "_testing_stencil";
							const char*	testName		= name.c_str();
							float		expectedValue	= stencilExpectedValue[stencilResolveModeNdx][sampleCountNdx];
							const TestConfig testConfig(format,
														imageData.width,
														imageData.height,
														imageData.depth,
														aspectFlags,
														sampleCount,
														dResolve.flag,
														sResolve.flag,
														VB_STENCIL,
														expectedValue);
							formatGroup->addChild(new DSResolveTestInstance(testCtx, tcu::NODETYPE_SELF_VALIDATE, testName, testName, testConfig));
						}
					}
				}

				sampleGroup->addChild(formatGroup.release());
			}

			imageGroup->addChild(sampleGroup.release());
		}

		group->addChild(imageGroup.release());
	}
}

} // anonymous

tcu::TestCaseGroup* createRenderPass2DepthStencilResolveTests (tcu::TestContext& testCtx)
{
	return createTestGroup(testCtx, "depth_stencil_resolve", "Depth/stencil resolve tests", initTests);
}

} // vkt
