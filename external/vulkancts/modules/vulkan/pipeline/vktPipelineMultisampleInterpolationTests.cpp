/*------------------------------------------------------------------------
* Vulkan Conformance Tests
* ------------------------
*
* Copyright (c) 2016 The Khronos Group Inc.
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
*//*
* \file vktPipelineMultisampleInterpolationTests.cpp
* \brief Multisample Interpolation Tests
*//*--------------------------------------------------------------------*/

#include "vktPipelineMultisampleInterpolationTests.hpp"
#include "vktPipelineMultisampleTestsUtil.hpp"
#include "vktPipelineMakeUtil.hpp"
#include "vkQueryUtil.hpp"
#include "tcuTestLog.hpp"

#include <set>

namespace vkt
{
namespace pipeline
{
namespace multisample
{

using namespace vk;

struct ImageMSParams
{
	ImageMSParams(const VkSampleCountFlagBits samples, const tcu::UVec3& size) : numSamples(samples), imageSize(size) {}

	VkSampleCountFlagBits	numSamples;
	tcu::UVec3				imageSize;
};

class MSInterpolationCaseBase : public TestCase
{
public:
	MSInterpolationCaseBase	(tcu::TestContext&		testCtx,
							 const std::string&		name,
							 const ImageMSParams&	imageMSParams)
		: TestCase(testCtx, name, "")
		, m_imageMSParams(imageMSParams)
	{}

protected:
	const ImageMSParams m_imageMSParams;
};

typedef MSInterpolationCaseBase* (*MSInterpolationCaseFuncPtr)(tcu::TestContext& testCtx, const std::string& name, const ImageMSParams& imageMSParams);

class MSInterpolationInstanceBase : public TestInstance
{
public:
								MSInterpolationInstanceBase	(Context&							context,
															 const ImageMSParams&				imageMSParams)
		: TestInstance		(context)
		, m_imageMSParams	(imageMSParams)
		, m_imageType		(IMAGE_TYPE_2D)
		, m_imageFormat		(tcu::TextureFormat(tcu::TextureFormat::RG, tcu::TextureFormat::UNORM_INT8))
	{}

	tcu::TestStatus				iterate						(void);

protected:

	typedef std::vector<VkVertexInputAttributeDescription> VertexAttribDescVec;

	struct VertexDataDesc
	{
		VkPrimitiveTopology	primitiveTopology;
		deUint32			verticesCount;
		deUint32			dataStride;
		VkDeviceSize		dataSize;
		VertexAttribDescVec	vertexAttribDescVec;
	};

	void						validateImageSize			(const InstanceInterface&			instance,
															 const VkPhysicalDevice				physicalDevice,
															 const ImageType					imageType,
															 const tcu::UVec3&					imageSize) const;

	void						validateImageFeatureFlags	(const InstanceInterface&			instance,
															 const VkPhysicalDevice				physicalDevice,
															 const VkFormat						format,
															 const VkFormatFeatureFlags			featureFlags) const;

	void						validateImageInfo			(const InstanceInterface&			instance,
															 const VkPhysicalDevice				physicalDevice,
															 const VkImageCreateInfo&			imageInfo) const;

	virtual VertexDataDesc		getVertexDataDescripton		(void) const = 0;

	virtual void				uploadVertexData			(const Allocation&					vertexBufferAllocation,
															 const VertexDataDesc&				vertexDataDescripton) const = 0;

	virtual tcu::TestStatus		verifyResolvedImage			(const tcu::ConstPixelBufferAccess&	imageData) const = 0;
protected:
	const ImageMSParams			m_imageMSParams;
	const ImageType				m_imageType;
	const tcu::TextureFormat	m_imageFormat;
};

void MSInterpolationInstanceBase::validateImageSize (const InstanceInterface&	instance,
													 const VkPhysicalDevice		physicalDevice,
													 const ImageType			imageType,
													 const tcu::UVec3&			imageSize) const
{
	const VkPhysicalDeviceProperties deviceProperties = getPhysicalDeviceProperties(instance, physicalDevice);

	bool isImageSizeValid = true;

	switch (imageType)
	{
		case IMAGE_TYPE_1D:
			isImageSizeValid =	imageSize.x() <= deviceProperties.limits.maxImageDimension1D;
			break;
		case IMAGE_TYPE_1D_ARRAY:
			isImageSizeValid =	imageSize.x() <= deviceProperties.limits.maxImageDimension1D &&
								imageSize.z() <= deviceProperties.limits.maxImageArrayLayers;
			break;
		case IMAGE_TYPE_2D:
			isImageSizeValid =	imageSize.x() <= deviceProperties.limits.maxImageDimension2D &&
								imageSize.y() <= deviceProperties.limits.maxImageDimension2D;
			break;
		case IMAGE_TYPE_2D_ARRAY:
			isImageSizeValid =	imageSize.x() <= deviceProperties.limits.maxImageDimension2D &&
								imageSize.y() <= deviceProperties.limits.maxImageDimension2D &&
								imageSize.z() <= deviceProperties.limits.maxImageArrayLayers;
			break;
		case IMAGE_TYPE_CUBE:
			isImageSizeValid =	imageSize.x() <= deviceProperties.limits.maxImageDimensionCube &&
								imageSize.y() <= deviceProperties.limits.maxImageDimensionCube;
			break;
		case IMAGE_TYPE_CUBE_ARRAY:
			isImageSizeValid =	imageSize.x() <= deviceProperties.limits.maxImageDimensionCube &&
								imageSize.y() <= deviceProperties.limits.maxImageDimensionCube &&
								imageSize.z() <= deviceProperties.limits.maxImageArrayLayers;
			break;
		case IMAGE_TYPE_3D:
			isImageSizeValid =	imageSize.x() <= deviceProperties.limits.maxImageDimension3D &&
								imageSize.y() <= deviceProperties.limits.maxImageDimension3D &&
								imageSize.z() <= deviceProperties.limits.maxImageDimension3D;
			break;
		default:
			DE_FATAL("Unknown image type");
	}

	if (!isImageSizeValid)
	{
		std::ostringstream	notSupportedStream;

		notSupportedStream << "Image type (" << getImageTypeName(imageType) << ") with size (" << imageSize.x() << ", " << imageSize.y() << ", " << imageSize.z() << ") not supported by device" << std::endl;

		const std::string notSupportedString = notSupportedStream.str();

		TCU_THROW(NotSupportedError, notSupportedString.c_str());
	}
}

void MSInterpolationInstanceBase::validateImageFeatureFlags	(const InstanceInterface&	instance,
															 const VkPhysicalDevice		physicalDevice,
															 const VkFormat				format,
															 const VkFormatFeatureFlags	featureFlags) const
{
	const VkFormatProperties formatProperties = getPhysicalDeviceFormatProperties(instance, physicalDevice, format);

	if ((formatProperties.optimalTilingFeatures & featureFlags) != featureFlags)
	{
		std::ostringstream	notSupportedStream;

		notSupportedStream << "Device does not support image format " << format << " for feature flags " << featureFlags << std::endl;

		const std::string notSupportedString = notSupportedStream.str();

		TCU_THROW(NotSupportedError, notSupportedString.c_str());
	}
}

void MSInterpolationInstanceBase::validateImageInfo	(const InstanceInterface&	instance,
													 const VkPhysicalDevice		physicalDevice,
													 const VkImageCreateInfo&	imageInfo) const
{
	VkImageFormatProperties imageFormatProps;
	instance.getPhysicalDeviceImageFormatProperties(physicalDevice, imageInfo.format, imageInfo.imageType, imageInfo.tiling, imageInfo.usage, imageInfo.flags, &imageFormatProps);

	if (imageFormatProps.maxExtent.width  < imageInfo.extent.width  ||
		imageFormatProps.maxExtent.height < imageInfo.extent.height ||
		imageFormatProps.maxExtent.depth  < imageInfo.extent.depth)
	{
		std::ostringstream	notSupportedStream;

		notSupportedStream	<< "Image extent ("
							<< imageInfo.extent.width  << ", "
							<< imageInfo.extent.height << ", "
							<< imageInfo.extent.depth
							<< ") exceeds allowed maximum ("
							<< imageFormatProps.maxExtent.width <<  ", "
							<< imageFormatProps.maxExtent.height << ", "
							<< imageFormatProps.maxExtent.depth
							<< ")"
							<< std::endl;

		const std::string notSupportedString = notSupportedStream.str();

		TCU_THROW(NotSupportedError, notSupportedString.c_str());
	}

	if (imageFormatProps.maxArrayLayers < imageInfo.arrayLayers)
	{
		std::ostringstream	notSupportedStream;

		notSupportedStream << "Image layers count of " << imageInfo.arrayLayers << " exceeds allowed maximum which is " << imageFormatProps.maxArrayLayers << std::endl;

		const std::string notSupportedString = notSupportedStream.str();

		TCU_THROW(NotSupportedError, notSupportedString.c_str());
	}

	if (!(imageFormatProps.sampleCounts & imageInfo.samples))
	{
		std::ostringstream	notSupportedStream;

		notSupportedStream << "Samples count of " << imageInfo.samples << " not supported for image" << std::endl;

		const std::string notSupportedString = notSupportedStream.str();

		TCU_THROW(NotSupportedError, notSupportedString.c_str());
	}
}

tcu::TestStatus MSInterpolationInstanceBase::iterate (void)
{
	const InstanceInterface&		instance			= m_context.getInstanceInterface();
	const DeviceInterface&			deviceInterface		= m_context.getDeviceInterface();
	const VkDevice					device				= m_context.getDevice();
	const VkPhysicalDevice			physicalDevice		= m_context.getPhysicalDevice();
	const VkPhysicalDeviceFeatures&	features			= m_context.getDeviceFeatures();
	Allocator&						allocator			= m_context.getDefaultAllocator();
	const VkQueue					queue				= m_context.getUniversalQueue();
	const deUint32					queueFamilyIndex	= m_context.getUniversalQueueFamilyIndex();

	VkImageCreateInfo				imageMSInfo;
	VkImageCreateInfo				imageRSInfo;

	// Check if image size does not exceed device limits
	validateImageSize(instance, physicalDevice, m_imageType, m_imageMSParams.imageSize);

	// Check if device supports image format as color attachment
	validateImageFeatureFlags(instance, physicalDevice, mapTextureFormat(m_imageFormat), VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT);

	imageMSInfo.sType					= VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageMSInfo.pNext					= DE_NULL;
	imageMSInfo.flags					= 0u;
	imageMSInfo.imageType				= mapImageType(m_imageType);
	imageMSInfo.format					= mapTextureFormat(m_imageFormat);
	imageMSInfo.extent					= makeExtent3D(getLayerSize(m_imageType, m_imageMSParams.imageSize));
	imageMSInfo.arrayLayers				= getNumLayers(m_imageType, m_imageMSParams.imageSize);
	imageMSInfo.mipLevels				= 1u;
	imageMSInfo.samples					= m_imageMSParams.numSamples;
	imageMSInfo.tiling					= VK_IMAGE_TILING_OPTIMAL;
	imageMSInfo.initialLayout			= VK_IMAGE_LAYOUT_UNDEFINED;
	imageMSInfo.usage					= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	imageMSInfo.sharingMode				= VK_SHARING_MODE_EXCLUSIVE;
	imageMSInfo.queueFamilyIndexCount	= 0u;
	imageMSInfo.pQueueFamilyIndices		= DE_NULL;

	if (m_imageType == IMAGE_TYPE_CUBE || m_imageType == IMAGE_TYPE_CUBE_ARRAY)
	{
		imageMSInfo.flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
	}

	validateImageInfo(instance, physicalDevice, imageMSInfo);

	const de::UniquePtr<Image> imageMS(new Image(deviceInterface, device, allocator, imageMSInfo, MemoryRequirement::Any));

	imageRSInfo			= imageMSInfo;
	imageRSInfo.samples	= VK_SAMPLE_COUNT_1_BIT;

	validateImageInfo(instance, physicalDevice, imageRSInfo);

	const de::UniquePtr<Image> imageRS(new Image(deviceInterface, device, allocator, imageRSInfo, MemoryRequirement::Any));

	// Create render pass
	const VkAttachmentDescription attachmentMSDesc =
	{
		(VkAttachmentDescriptionFlags)0u,			// VkAttachmentDescriptionFlags		flags;
		imageMSInfo.format,							// VkFormat							format;
		imageMSInfo.samples,						// VkSampleCountFlagBits			samples;
		VK_ATTACHMENT_LOAD_OP_CLEAR,				// VkAttachmentLoadOp				loadOp;
		VK_ATTACHMENT_STORE_OP_STORE,				// VkAttachmentStoreOp				storeOp;
		VK_ATTACHMENT_LOAD_OP_DONT_CARE,			// VkAttachmentLoadOp				stencilLoadOp;
		VK_ATTACHMENT_STORE_OP_DONT_CARE,			// VkAttachmentStoreOp				stencilStoreOp;
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,	// VkImageLayout					initialLayout;
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL	// VkImageLayout					finalLayout;
	};

	const VkAttachmentDescription attachmentRSDesc =
	{
		(VkAttachmentDescriptionFlags)0u,			// VkAttachmentDescriptionFlags		flags;
		imageRSInfo.format,							// VkFormat							format;
		imageRSInfo.samples,						// VkSampleCountFlagBits			samples;
		VK_ATTACHMENT_LOAD_OP_CLEAR,			// VkAttachmentLoadOp				loadOp;
		VK_ATTACHMENT_STORE_OP_STORE,				// VkAttachmentStoreOp				storeOp;
		VK_ATTACHMENT_LOAD_OP_DONT_CARE,			// VkAttachmentLoadOp				stencilLoadOp;
		VK_ATTACHMENT_STORE_OP_DONT_CARE,			// VkAttachmentStoreOp				stencilStoreOp;
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,	// VkImageLayout					initialLayout;
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL	// VkImageLayout					finalLayout;
	};

	const VkAttachmentDescription attachments[] = { attachmentMSDesc, attachmentRSDesc };

	const VkAttachmentReference attachmentMSRef =
	{
		0u,											// deUint32			attachment;
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL	// VkImageLayout	layout;
	};

	const VkAttachmentReference attachmentRSRef =
	{
		1u,											// deUint32			attachment;
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL	// VkImageLayout	layout;
	};

	const VkAttachmentReference* resolveAttachment = m_imageMSParams.numSamples == VK_SAMPLE_COUNT_1_BIT ? DE_NULL : &attachmentRSRef;

	const VkSubpassDescription subpassDescription =
	{
		(VkSubpassDescriptionFlags)0u,						// VkSubpassDescriptionFlags		flags;
		VK_PIPELINE_BIND_POINT_GRAPHICS,					// VkPipelineBindPoint				pipelineBindPoint;
		0u,													// deUint32							inputAttachmentCount;
		DE_NULL,											// const VkAttachmentReference*		pInputAttachments;
		1u,													// deUint32							colorAttachmentCount;
		&attachmentMSRef,									// const VkAttachmentReference*		pColorAttachments;
		resolveAttachment,								// const VkAttachmentReference*		pResolveAttachments;
		DE_NULL,											// const VkAttachmentReference*		pDepthStencilAttachment;
		0u,													// deUint32							preserveAttachmentCount;
		DE_NULL												// const deUint32*					pPreserveAttachments;
	};

	const VkRenderPassCreateInfo renderPassInfo =
	{
		VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,			// VkStructureType					sType;
		DE_NULL,											// const void*						pNext;
		(VkRenderPassCreateFlags)0u,						// VkRenderPassCreateFlags			flags;
		2u,													// deUint32							attachmentCount;
		attachments,										// const VkAttachmentDescription*	pAttachments;
		1u,													// deUint32							subpassCount;
		&subpassDescription,								// const VkSubpassDescription*		pSubpasses;
		0u,													// deUint32							dependencyCount;
		DE_NULL												// const VkSubpassDependency*		pDependencies;
	};

	const Unique<VkRenderPass> renderPass(createRenderPass(deviceInterface, device, &renderPassInfo));

	const VkImageSubresourceRange fullImageRange = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, imageMSInfo.mipLevels, 0u, imageMSInfo.arrayLayers);

	// Create color attachments image views
	const Unique<VkImageView> imageMSView(makeImageView(deviceInterface, device, **imageMS, mapImageViewType(m_imageType), imageMSInfo.format, fullImageRange));
	const Unique<VkImageView> imageRSView(makeImageView(deviceInterface, device, **imageRS, mapImageViewType(m_imageType), imageMSInfo.format, fullImageRange));

	const VkImageView attachmentsViews[] = { *imageMSView, *imageRSView };

	// Create framebuffer
	const VkFramebufferCreateInfo framebufferInfo =
	{
		VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,	// VkStructureType                             sType;
		DE_NULL,									// const void*                                 pNext;
		(VkFramebufferCreateFlags)0u,				// VkFramebufferCreateFlags                    flags;
		*renderPass,								// VkRenderPass                                renderPass;
		2u,											// uint32_t                                    attachmentCount;
		attachmentsViews,							// const VkImageView*                          pAttachments;
		imageMSInfo.extent.width,					// uint32_t                                    width;
		imageMSInfo.extent.height,					// uint32_t                                    height;
		imageMSInfo.arrayLayers,					// uint32_t                                    layers;
	};

	const Unique<VkFramebuffer> framebuffer(createFramebuffer(deviceInterface, device, &framebufferInfo));

	// Create pipeline layout
	const VkPipelineLayoutCreateInfo pipelineLayoutParams =
	{
		VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,		// VkStructureType					sType;
		DE_NULL,											// const void*						pNext;
		(VkPipelineLayoutCreateFlags)0u,					// VkPipelineLayoutCreateFlags		flags;
		0u,													// deUint32							setLayoutCount;
		DE_NULL,											// const VkDescriptorSetLayout*		pSetLayouts;
		0u,													// deUint32							pushConstantRangeCount;
		DE_NULL,											// const VkPushConstantRange*		pPushConstantRanges;
	};

	const Unique<VkPipelineLayout> pipelineLayout(createPipelineLayout(deviceInterface, device, &pipelineLayoutParams));

	// Create vertex attributes data
	const VertexDataDesc vertexDataDesc = getVertexDataDescripton();

	de::SharedPtr<Buffer> vertexBuffer = de::SharedPtr<Buffer>(new Buffer(deviceInterface, device, allocator, makeBufferCreateInfo(vertexDataDesc.dataSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT), MemoryRequirement::HostVisible));
	const Allocation& vertexBufferAllocation = vertexBuffer->getAllocation();

	uploadVertexData(vertexBufferAllocation, vertexDataDesc);

	flushMappedMemoryRange(deviceInterface, device, vertexBufferAllocation.getMemory(), vertexBufferAllocation.getOffset(), vertexDataDesc.dataSize);

	const VkVertexInputBindingDescription vertexBinding =
	{
		0u,							// deUint32				binding;
		vertexDataDesc.dataStride,	// deUint32				stride;
		VK_VERTEX_INPUT_RATE_VERTEX	// VkVertexInputRate	inputRate;
	};

	const VkPipelineVertexInputStateCreateInfo vertexInputStateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,			// VkStructureType                             sType;
		DE_NULL,															// const void*                                 pNext;
		(VkPipelineVertexInputStateCreateFlags)0u,							// VkPipelineVertexInputStateCreateFlags       flags;
		1u,																	// uint32_t                                    vertexBindingDescriptionCount;
		&vertexBinding,														// const VkVertexInputBindingDescription*      pVertexBindingDescriptions;
		static_cast<deUint32>(vertexDataDesc.vertexAttribDescVec.size()),	// uint32_t                                    vertexAttributeDescriptionCount;
		dataPointer(vertexDataDesc.vertexAttribDescVec),					// const VkVertexInputAttributeDescription*    pVertexAttributeDescriptions;
	};

	const VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,	// VkStructureType                             sType;
		DE_NULL,														// const void*                                 pNext;
		(VkPipelineInputAssemblyStateCreateFlags)0u,					// VkPipelineInputAssemblyStateCreateFlags     flags;
		vertexDataDesc.primitiveTopology,								// VkPrimitiveTopology                         topology;
		VK_FALSE,														// VkBool32                                    primitiveRestartEnable;
	};

	const VkViewport viewport =
	{
		0.0f, 0.0f,
		static_cast<float>(imageMSInfo.extent.width), static_cast<float>(imageMSInfo.extent.height),
		0.0f, 1.0f
	};

	const VkRect2D scissor =
	{
		makeOffset2D(0, 0),
		makeExtent2D(imageMSInfo.extent.width, imageMSInfo.extent.height),
	};

	const VkPipelineViewportStateCreateInfo viewportStateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,			// VkStructureType                             sType;
		DE_NULL,														// const void*                                 pNext;
		(VkPipelineViewportStateCreateFlags)0u,							// VkPipelineViewportStateCreateFlags          flags;
		1u,																// uint32_t                                    viewportCount;
		&viewport,														// const VkViewport*                           pViewports;
		1u,																// uint32_t                                    scissorCount;
		&scissor,														// const VkRect2D*                             pScissors;
	};

	const VkPipelineRasterizationStateCreateInfo rasterizationStateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,		// VkStructureType                          sType;
		DE_NULL,														// const void*                              pNext;
		(VkPipelineRasterizationStateCreateFlags)0u,					// VkPipelineRasterizationStateCreateFlags  flags;
		VK_FALSE,														// VkBool32                                 depthClampEnable;
		VK_FALSE,														// VkBool32                                 rasterizerDiscardEnable;
		VK_POLYGON_MODE_FILL,											// VkPolygonMode							polygonMode;
		VK_CULL_MODE_NONE,												// VkCullModeFlags							cullMode;
		VK_FRONT_FACE_COUNTER_CLOCKWISE,								// VkFrontFace								frontFace;
		VK_FALSE,														// VkBool32									depthBiasEnable;
		0.0f,															// float									depthBiasConstantFactor;
		0.0f,															// float									depthBiasClamp;
		0.0f,															// float									depthBiasSlopeFactor;
		1.0f,															// float									lineWidth;
	};

	const VkPipelineMultisampleStateCreateInfo multisampleStateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,		// VkStructureType							sType;
		DE_NULL,														// const void*								pNext;
		(VkPipelineMultisampleStateCreateFlags)0u,						// VkPipelineMultisampleStateCreateFlags	flags;
		imageMSInfo.samples,											// VkSampleCountFlagBits					rasterizationSamples;
		features.sampleRateShading,										// VkBool32									sampleShadingEnable;
		1.0f,															// float									minSampleShading;
		DE_NULL,														// const VkSampleMask*						pSampleMask;
		VK_FALSE,														// VkBool32									alphaToCoverageEnable;
		VK_FALSE,														// VkBool32									alphaToOneEnable;
	};

	const VkStencilOpState stencilOpState = makeStencilOpState
	(
		VK_STENCIL_OP_KEEP,		// stencil fail
		VK_STENCIL_OP_KEEP,		// depth & stencil pass
		VK_STENCIL_OP_KEEP,		// depth only fail
		VK_COMPARE_OP_ALWAYS,	// compare op
		0u,						// compare mask
		0u,						// write mask
		0u						// reference
	);

	const VkPipelineDepthStencilStateCreateInfo depthStencilStateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,		// VkStructureType							sType;
		DE_NULL,														// const void*								pNext;
		(VkPipelineDepthStencilStateCreateFlags)0u,						// VkPipelineDepthStencilStateCreateFlags	flags;
		VK_FALSE,														// VkBool32									depthTestEnable;
		VK_FALSE,														// VkBool32									depthWriteEnable;
		VK_COMPARE_OP_LESS,												// VkCompareOp								depthCompareOp;
		VK_FALSE,														// VkBool32									depthBoundsTestEnable;
		VK_FALSE,														// VkBool32									stencilTestEnable;
		stencilOpState,													// VkStencilOpState							front;
		stencilOpState,													// VkStencilOpState							back;
		0.0f,															// float									minDepthBounds;
		1.0f,															// float									maxDepthBounds;
	};

	const VkColorComponentFlags colorComponentsAll = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

	const VkPipelineColorBlendAttachmentState colorBlendAttachmentState =
	{
		VK_FALSE,														// VkBool32					blendEnable;
		VK_BLEND_FACTOR_SRC_ALPHA,										// VkBlendFactor			srcColorBlendFactor;
		VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,							// VkBlendFactor			dstColorBlendFactor;
		VK_BLEND_OP_ADD,												// VkBlendOp				colorBlendOp;
		VK_BLEND_FACTOR_SRC_ALPHA,										// VkBlendFactor			srcAlphaBlendFactor;
		VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,							// VkBlendFactor			dstAlphaBlendFactor;
		VK_BLEND_OP_ADD,												// VkBlendOp				alphaBlendOp;
		colorComponentsAll,												// VkColorComponentFlags	colorWriteMask;
	};

	const VkPipelineColorBlendStateCreateInfo colorBlendStateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,		// VkStructureType								sType;
		DE_NULL,														// const void*									pNext;
		(VkPipelineColorBlendStateCreateFlags)0u,						// VkPipelineColorBlendStateCreateFlags			flags;
		VK_FALSE,														// VkBool32										logicOpEnable;
		VK_LOGIC_OP_COPY,												// VkLogicOp									logicOp;
		1u,																// deUint32										attachmentCount;
		&colorBlendAttachmentState,										// const VkPipelineColorBlendAttachmentState*	pAttachments;
		{ 0.0f, 0.0f, 0.0f, 0.0f },										// float										blendConstants[4];
	};

	const Unique<VkShaderModule> vsModule(createShaderModule(deviceInterface, device, m_context.getBinaryCollection().get("vertex_shader"), (VkShaderModuleCreateFlags)0));

	const VkPipelineShaderStageCreateInfo vsShaderStageInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,			// VkStructureType						sType;
		DE_NULL,														// const void*							pNext;
		(VkPipelineShaderStageCreateFlags)0u,							// VkPipelineShaderStageCreateFlags		flags;
		VK_SHADER_STAGE_VERTEX_BIT,										// VkShaderStageFlagBits				stage;
		*vsModule,														// VkShaderModule						module;
		"main",															// const char*							pName;
		DE_NULL,														// const VkSpecializationInfo*			pSpecializationInfo;
	};

	const Unique<VkShaderModule> fsModule(createShaderModule(deviceInterface, device, m_context.getBinaryCollection().get("fragment_shader"), (VkShaderModuleCreateFlags)0));

	const VkPipelineShaderStageCreateInfo fsShaderStageInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,			// VkStructureType						sType;
		DE_NULL,														// const void*							pNext;
		(VkPipelineShaderStageCreateFlags)0u,							// VkPipelineShaderStageCreateFlags		flags;
		VK_SHADER_STAGE_FRAGMENT_BIT,									// VkShaderStageFlagBits				stage;
		*fsModule,														// VkShaderModule						module;
		"main",															// const char*							pName;
		DE_NULL,														// const VkSpecializationInfo*			pSpecializationInfo;
	};

	const VkPipelineShaderStageCreateInfo shaderStageInfos[] = { vsShaderStageInfo, fsShaderStageInfo };

	const VkGraphicsPipelineCreateInfo graphicsPipelineInfo =
	{
		VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,				// VkStructureType									sType;
		DE_NULL,														// const void*										pNext;
		(VkPipelineCreateFlags)0,										// VkPipelineCreateFlags							flags;
		2u,																// deUint32											stageCount;
		shaderStageInfos,												// const VkPipelineShaderStageCreateInfo*			pStages;
		&vertexInputStateInfo,											// const VkPipelineVertexInputStateCreateInfo*		pVertexInputState;
		&inputAssemblyStateInfo,										// const VkPipelineInputAssemblyStateCreateInfo*	pInputAssemblyState;
		DE_NULL,														// const VkPipelineTessellationStateCreateInfo*		pTessellationState;
		&viewportStateInfo,												// const VkPipelineViewportStateCreateInfo*			pViewportState;
		&rasterizationStateInfo,										// const VkPipelineRasterizationStateCreateInfo*	pRasterizationState;
		&multisampleStateInfo,											// const VkPipelineMultisampleStateCreateInfo*		pMultisampleState;
		&depthStencilStateInfo,											// const VkPipelineDepthStencilStateCreateInfo*		pDepthStencilState;
		&colorBlendStateInfo,											// const VkPipelineColorBlendStateCreateInfo*		pColorBlendState;
		DE_NULL,														// const VkPipelineDynamicStateCreateInfo*			pDynamicState;
		*pipelineLayout,												// VkPipelineLayout									layout;
		*renderPass,													// VkRenderPass										renderPass;
		0u,																// deUint32											subpass;
		DE_NULL,														// VkPipeline										basePipelineHandle;
		0u,																// deInt32											basePipelineIndex;
	};

	// Create graphics pipeline
	const Unique<VkPipeline> graphicsPipeline(createGraphicsPipeline(deviceInterface, device, DE_NULL, &graphicsPipelineInfo));

	// Create command buffer for compute and transfer oparations
	const Unique<VkCommandPool>	  commandPool(createCommandPool(deviceInterface, device, (VkCommandPoolCreateFlags)VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex));
	const Unique<VkCommandBuffer> commandBuffer(makeCommandBuffer(deviceInterface, device, *commandPool));

	// Start recording commands
	beginCommandBuffer(deviceInterface, *commandBuffer);

	{
		VkImageMemoryBarrier imageOutputAttachmentBarriers[2];

		imageOutputAttachmentBarriers[0] = makeImageMemoryBarrier
		(
			0u,
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			**imageMS,
			fullImageRange
		);

		imageOutputAttachmentBarriers[1] = makeImageMemoryBarrier
		(
			0u,
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			**imageRS,
			fullImageRange
		);

		deviceInterface.cmdPipelineBarrier(*commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0u, 0u, DE_NULL, 0u, DE_NULL, 2u, imageOutputAttachmentBarriers);
	}

	{
		const VkDeviceSize vertexStartOffset = 0u;

		std::vector<VkClearValue> clearValues;
		clearValues.push_back(makeClearValueColor(tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f)));
		clearValues.push_back(makeClearValueColor(tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f)));

		const vk::VkRect2D renderArea =
		{
			makeOffset2D(0u, 0u),
			makeExtent2D(imageMSInfo.extent.width, imageMSInfo.extent.height),
		};

		// Begin render pass
		const VkRenderPassBeginInfo renderPassBeginInfo =
		{
			VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,		// VkStructureType         sType;
			DE_NULL,										// const void*             pNext;
			*renderPass,										// VkRenderPass            renderPass;
			*framebuffer,									// VkFramebuffer           framebuffer;
			renderArea,										// VkRect2D                renderArea;
			static_cast<deUint32>(clearValues.size()),		// deUint32                clearValueCount;
			&clearValues[0],								// const VkClearValue*     pClearValues;
		};

		deviceInterface.cmdBeginRenderPass(*commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

		// Bind graphics pipeline
		deviceInterface.cmdBindPipeline(*commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *graphicsPipeline);

		// Bind vertex buffer
		deviceInterface.cmdBindVertexBuffers(*commandBuffer, 0u, 1u, &vertexBuffer->get(), &vertexStartOffset);

		// Draw full screen quad
		deviceInterface.cmdDraw(*commandBuffer, vertexDataDesc.verticesCount, 1u, 0u, 0u);

		// End render pass
		deviceInterface.cmdEndRenderPass(*commandBuffer);
	}

	const VkImage sourceImage = m_imageMSParams.numSamples == VK_SAMPLE_COUNT_1_BIT ? **imageMS : **imageRS;

	{
		const VkImageMemoryBarrier imageTransferSrcBarrier = makeImageMemoryBarrier
		(
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			VK_ACCESS_TRANSFER_READ_BIT,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			sourceImage,
			fullImageRange
		);

		deviceInterface.cmdPipelineBarrier(*commandBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u, DE_NULL, 0u, DE_NULL, 1u, &imageTransferSrcBarrier);
	}

	// Copy data from resolve image to buffer
	const deUint32				imageRSSizeInBytes = getImageSizeInBytes(imageRSInfo.extent, imageRSInfo.arrayLayers, m_imageFormat, imageRSInfo.mipLevels);

	const VkBufferCreateInfo	bufferRSInfo = makeBufferCreateInfo(imageRSSizeInBytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
	const de::UniquePtr<Buffer>	bufferRS(new Buffer(deviceInterface, device, allocator, bufferRSInfo, MemoryRequirement::HostVisible));

	{
		const VkBufferImageCopy bufferImageCopy =
		{
			0u,																						//	VkDeviceSize				bufferOffset;
			0u,																						//	deUint32					bufferRowLength;
			0u,																						//	deUint32					bufferImageHeight;
			makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, imageRSInfo.arrayLayers),	//	VkImageSubresourceLayers	imageSubresource;
			makeOffset3D(0, 0, 0),																	//	VkOffset3D					imageOffset;
			imageRSInfo.extent,																		//	VkExtent3D					imageExtent;
		};

		deviceInterface.cmdCopyImageToBuffer(*commandBuffer, sourceImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, bufferRS->get(), 1u, &bufferImageCopy);
	}

	{
		const VkBufferMemoryBarrier bufferRSHostReadBarrier = makeBufferMemoryBarrier
		(
			VK_ACCESS_TRANSFER_WRITE_BIT,
			VK_ACCESS_HOST_READ_BIT,
			bufferRS->get(),
			0u,
			imageRSSizeInBytes
		);

		deviceInterface.cmdPipelineBarrier(*commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0u, 0u, DE_NULL, 1u, &bufferRSHostReadBarrier, 0u, DE_NULL);
	}

	// End recording commands
	VK_CHECK(deviceInterface.endCommandBuffer(*commandBuffer));

	// Submit commands for execution and wait for completion
	submitCommandsAndWait(deviceInterface, device, queue, *commandBuffer);

	// Retrieve data from buffer to host memory
	const Allocation& bufferRSAllocation = bufferRS->getAllocation();

	invalidateMappedMemoryRange(deviceInterface, device, bufferRSAllocation.getMemory(), bufferRSAllocation.getOffset(), imageRSSizeInBytes);

	const tcu::ConstPixelBufferAccess bufferRSData (m_imageFormat,
													imageRSInfo.extent.width,
													imageRSInfo.extent.height,
													imageRSInfo.extent.depth * imageRSInfo.arrayLayers,
													bufferRSAllocation.getHostPtr());

	std::stringstream imageName;
	imageName << getImageTypeName(m_imageType) << "_" << bufferRSData.getWidth() << "_" << bufferRSData.getHeight() << "_" << bufferRSData.getDepth() << std::endl;

	m_context.getTestContext().getLog()
		<< tcu::TestLog::Section(imageName.str(), imageName.str())
		<< tcu::LogImage("image", "", bufferRSData)
		<< tcu::TestLog::EndSection;

	return verifyResolvedImage(bufferRSData);
}

class MSInstanceDistinctValues : public MSInterpolationInstanceBase
{
public:
					MSInstanceDistinctValues(Context&				context,
											 const ImageMSParams&	imageMSParams)
	: MSInterpolationInstanceBase(context, imageMSParams) {}

	VertexDataDesc	getVertexDataDescripton	(void) const;
	void			uploadVertexData		(const Allocation& vertexBufferAllocation, const VertexDataDesc& vertexDataDescripton) const;
	tcu::TestStatus	verifyResolvedImage		(const tcu::ConstPixelBufferAccess&	imageData) const;

protected:
	struct VertexData
	{
		VertexData(const tcu::Vec4& posNdc) : positionNdc(posNdc) {}

		tcu::Vec4 positionNdc;
	};
};

MSInterpolationInstanceBase::VertexDataDesc MSInstanceDistinctValues::getVertexDataDescripton (void) const
{
	VertexDataDesc vertexDataDesc;

	vertexDataDesc.verticesCount		= 3u;
	vertexDataDesc.dataStride			= sizeof(VertexData);
	vertexDataDesc.dataSize				= vertexDataDesc.verticesCount * vertexDataDesc.dataStride;
	vertexDataDesc.primitiveTopology	= VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

	const VkVertexInputAttributeDescription vertexAttribPositionNdc =
	{
		0u,										// deUint32	location;
		0u,										// deUint32	binding;
		VK_FORMAT_R32G32B32A32_SFLOAT,			// VkFormat	format;
		DE_OFFSET_OF(VertexData, positionNdc),	// deUint32	offset;
	};

	vertexDataDesc.vertexAttribDescVec.push_back(vertexAttribPositionNdc);

	return vertexDataDesc;
}

void MSInstanceDistinctValues::uploadVertexData (const Allocation& vertexBufferAllocation, const VertexDataDesc& vertexDataDescripton) const
{
	std::vector<VertexData> vertices;

	vertices.push_back(VertexData(tcu::Vec4(-1.0f,-1.0f, 0.0f, 1.0f)));
	vertices.push_back(VertexData(tcu::Vec4(-1.0f, 4.0f, 0.0f, 1.0f)));
	vertices.push_back(VertexData(tcu::Vec4( 4.0f,-1.0f, 0.0f, 1.0f)));

	deMemcpy(vertexBufferAllocation.getHostPtr(), dataPointer(vertices), static_cast<std::size_t>(vertexDataDescripton.dataSize));
}

tcu::TestStatus	MSInstanceDistinctValues::verifyResolvedImage (const tcu::ConstPixelBufferAccess& imageData) const
{
	const deUint32 distinctValuesExpected = static_cast<deUint32>(m_imageMSParams.numSamples) + 1u;

	std::vector<tcu::IVec4> distinctValues;

	for (deInt32 z = 0u; z < imageData.getDepth();  ++z)
	for (deInt32 y = 0u; y < imageData.getHeight(); ++y)
	for (deInt32 x = 0u; x < imageData.getWidth();  ++x)
	{
		const tcu::IVec4 pixel = imageData.getPixelInt(x, y, z);

		if (std::find(distinctValues.begin(), distinctValues.end(), pixel) == distinctValues.end())
			distinctValues.push_back(pixel);
	}

	if (distinctValues.size() >= distinctValuesExpected)
		return tcu::TestStatus::pass("Passed");
	else
		return tcu::TestStatus::fail("Failed");
}

class MSCaseSampleQualifierDistinctValues : public MSInterpolationCaseBase
{
public:
					MSCaseSampleQualifierDistinctValues	(tcu::TestContext&		testCtx,
														 const std::string&		name,
														 const ImageMSParams&	imageMSParams)
	: MSInterpolationCaseBase(testCtx, name, imageMSParams) {}

	void			init								(void);
	void			initPrograms						(vk::SourceCollections& programCollection) const;
	TestInstance*	createInstance						(Context&				context) const;
};

MSInterpolationCaseBase* createMSCaseSampleQualifierDistinctValues (tcu::TestContext& testCtx, const std::string& name, const ImageMSParams& imageMSParams)
{
	return new MSCaseSampleQualifierDistinctValues(testCtx, name, imageMSParams);
}

void MSCaseSampleQualifierDistinctValues::init (void)
{
	m_testCtx.getLog()
		<< tcu::TestLog::Message
		<< "Verifying that a sample qualified varying is given different values for different samples.\n"
		<< "	Render full screen traingle with quadratic function defining red/green color pattern division.\n"
		<< "	=> Resulting image should contain n+1 different colors, where n = sample count.\n"
		<< tcu::TestLog::EndMessage;

	MSInterpolationCaseBase::init();
}

void MSCaseSampleQualifierDistinctValues::initPrograms (vk::SourceCollections& programCollection) const
{
	// Create vertex shader
	std::ostringstream vs;

	vs << "#version 440\n"
		<< "layout(location = 0) in vec4 vs_in_position_ndc;\n"
		<< "\n"
		<< "layout(location = 0) out vec4 vs_out_position_ndc;\n"
		<< "\n"
		<< "out gl_PerVertex {\n"
		<< "	vec4  gl_Position;\n"
		<< "};\n"
		<< "void main (void)\n"
		<< "{\n"
		<< "	gl_Position			= vs_in_position_ndc;\n"
		<< "	vs_out_position_ndc = vs_in_position_ndc;\n"
		<< "}\n";

	programCollection.glslSources.add("vertex_shader") << glu::VertexSource(vs.str());

	// Create fragment shader
	std::ostringstream fs;

	fs << "#version 440\n"
		<< "layout(location = 0) sample in vec4 fs_in_position_ndc;\n"
		<< "\n"
		<< "layout(location = 0) out vec2 fs_out_color;\n"
		<< "\n"
		<< "void main (void)\n"
		<< "{\n"
		<< "	if(fs_in_position_ndc.y < -2.0*pow(0.5*(fs_in_position_ndc.x + 1.0), 2.0) + 1.0)\n"
		<< "		fs_out_color = vec2(1.0, 0.0);\n"
		<< "	else\n"
		<< "		fs_out_color = vec2(0.0, 1.0);\n"
		<< "}\n";

	programCollection.glslSources.add("fragment_shader") << glu::FragmentSource(fs.str());
}

TestInstance* MSCaseSampleQualifierDistinctValues::createInstance (Context& context) const
{
	if (!context.getDeviceFeatures().sampleRateShading)
		TCU_THROW(NotSupportedError, "sampleRateShading support required");
	return new MSInstanceDistinctValues(context, m_imageMSParams);
}

class MSCaseInterpolateAtSampleDistinctValues : public MSInterpolationCaseBase
{
public:
					MSCaseInterpolateAtSampleDistinctValues	(tcu::TestContext&		testCtx,
															 const std::string&		name,
															 const ImageMSParams&	imageMSParams)
	: MSInterpolationCaseBase(testCtx, name, imageMSParams) {}

	void			init									(void);
	void			initPrograms							(vk::SourceCollections& programCollection) const;
	TestInstance*	createInstance							(Context&				context) const;
};

MSInterpolationCaseBase* createMSCaseInterpolateAtSampleDistinctValues (tcu::TestContext& testCtx, const std::string& name, const ImageMSParams& imageMSParams)
{
	return new MSCaseInterpolateAtSampleDistinctValues(testCtx, name, imageMSParams);
}

void MSCaseInterpolateAtSampleDistinctValues::init (void)
{
	m_testCtx.getLog()
		<< tcu::TestLog::Message
		<< "Verifying that a interpolateAtSample returns different values for different samples.\n"
		<< "	Render full screen traingle with quadratic function defining red/green color pattern division.\n"
		<< "	=> Resulting image should contain n+1 different colors, where n = sample count.\n"
		<< tcu::TestLog::EndMessage;

	MSInterpolationCaseBase::init();
}

void MSCaseInterpolateAtSampleDistinctValues::initPrograms (vk::SourceCollections& programCollection) const
{
	// Create vertex shader
	std::ostringstream vs;

	vs << "#version 440\n"
		<< "layout(location = 0) in vec4 vs_in_position_ndc;\n"
		<< "\n"
		<< "layout(location = 0) out vec4 vs_out_position_ndc;\n"
		<< "\n"
		<< "out gl_PerVertex {\n"
		<< "	vec4  gl_Position;\n"
		<< "};\n"
		<< "void main (void)\n"
		<< "{\n"
		<< "	gl_Position			= vs_in_position_ndc;\n"
		<< "	vs_out_position_ndc = vs_in_position_ndc;\n"
		<< "}\n";

	programCollection.glslSources.add("vertex_shader") << glu::VertexSource(vs.str());

	// Create fragment shader
	std::ostringstream fs;

	fs << "#version 440\n"
		<< "layout(location = 0) in vec4 fs_in_position_ndc;\n"
		<< "\n"
		<< "layout(location = 0) out vec2 fs_out_color;\n"
		<< "\n"
		<< "void main (void)\n"
		<< "{\n"
		<< "	const vec4 position_ndc_at_sample = interpolateAtSample(fs_in_position_ndc, gl_SampleID);\n"
		<< "	if(position_ndc_at_sample.y < -2.0*pow(0.5*(position_ndc_at_sample.x + 1.0), 2.0) + 1.0)\n"
		<< "		fs_out_color = vec2(0.0, 1.0);\n"
		<< "	else\n"
		<< "		fs_out_color = vec2(1.0, 0.0);\n"
		<< "}\n";

	programCollection.glslSources.add("fragment_shader") << glu::FragmentSource(fs.str());
}

TestInstance* MSCaseInterpolateAtSampleDistinctValues::createInstance (Context& context) const
{
	if (!context.getDeviceFeatures().sampleRateShading)
		TCU_THROW(NotSupportedError, "sampleRateShading support required");

	return new MSInstanceDistinctValues(context, m_imageMSParams);
}

class MSInstanceInterpolateScreenPosition : public MSInterpolationInstanceBase
{
public:
					MSInstanceInterpolateScreenPosition	(Context&				context,
														 const ImageMSParams&	imageMSParams)
	: MSInterpolationInstanceBase(context, imageMSParams) {}

	VertexDataDesc	getVertexDataDescripton				(void) const;
	void			uploadVertexData					(const Allocation& vertexBufferAllocation, const VertexDataDesc& vertexDataDescripton) const;
	tcu::TestStatus	verifyResolvedImage					(const tcu::ConstPixelBufferAccess&	imageData) const;

protected:
	struct VertexData
	{
		VertexData(const tcu::Vec4& posNdc, const tcu::Vec2& posScreen) : positionNdc(posNdc), positionScreen(posScreen) {}

		tcu::Vec4 positionNdc;
		tcu::Vec2 positionScreen;
	};
};

MSInterpolationInstanceBase::VertexDataDesc MSInstanceInterpolateScreenPosition::getVertexDataDescripton (void) const
{
	VertexDataDesc vertexDataDesc;

	vertexDataDesc.verticesCount		= 4u;
	vertexDataDesc.dataStride			= sizeof(VertexData);
	vertexDataDesc.dataSize				= vertexDataDesc.verticesCount * vertexDataDesc.dataStride;
	vertexDataDesc.primitiveTopology	= VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;

	const VkVertexInputAttributeDescription vertexAttribPositionNdc =
	{
		0u,											// deUint32	location;
		0u,											// deUint32	binding;
		VK_FORMAT_R32G32B32A32_SFLOAT,				// VkFormat	format;
		DE_OFFSET_OF(VertexData, positionNdc),		// deUint32	offset;
	};

	vertexDataDesc.vertexAttribDescVec.push_back(vertexAttribPositionNdc);

	const VkVertexInputAttributeDescription vertexAttribPositionScreen =
	{
		1u,											// deUint32	location;
		0u,											// deUint32	binding;
		VK_FORMAT_R32G32_SFLOAT,					// VkFormat	format;
		DE_OFFSET_OF(VertexData, positionScreen),	// deUint32	offset;
	};

	vertexDataDesc.vertexAttribDescVec.push_back(vertexAttribPositionScreen);

	return vertexDataDesc;
}

void MSInstanceInterpolateScreenPosition::uploadVertexData (const Allocation& vertexBufferAllocation, const VertexDataDesc& vertexDataDescripton) const
{
	const tcu::UVec3 layerSize		= getLayerSize(IMAGE_TYPE_2D, m_imageMSParams.imageSize);
	const float		 screenSizeX	= static_cast<float>(layerSize.x());
	const float		 screenSizeY	= static_cast<float>(layerSize.y());

	std::vector<VertexData> vertices;

	vertices.push_back(VertexData(tcu::Vec4(-1.0f,-1.0f, 0.0f, 1.0f), tcu::Vec2(0.0f,		 0.0f)));
	vertices.push_back(VertexData(tcu::Vec4( 1.0f,-1.0f, 0.0f, 1.0f), tcu::Vec2(screenSizeX, 0.0f)));
	vertices.push_back(VertexData(tcu::Vec4(-1.0f, 1.0f, 0.0f, 1.0f), tcu::Vec2(0.0f,		 screenSizeY)));
	vertices.push_back(VertexData(tcu::Vec4( 1.0f, 1.0f, 0.0f, 1.0f), tcu::Vec2(screenSizeX, screenSizeY)));

	deMemcpy(vertexBufferAllocation.getHostPtr(), dataPointer(vertices), static_cast<std::size_t>(vertexDataDescripton.dataSize));
}

tcu::TestStatus	MSInstanceInterpolateScreenPosition::verifyResolvedImage (const tcu::ConstPixelBufferAccess& imageData) const
{
	for (deInt32 z = 0u; z < imageData.getDepth();  ++z)
	for (deInt32 y = 0u; y < imageData.getHeight(); ++y)
	for (deInt32 x = 0u; x < imageData.getWidth();  ++x)
	{
		const deInt32 firstComponent = imageData.getPixelInt(x, y, z).x();

		if (firstComponent > 0)
			return tcu::TestStatus::fail("Failed");
	}
	return tcu::TestStatus::pass("Passed");
}

class MSCaseInterpolateAtSampleSingleSample : public MSInterpolationCaseBase
{
public:
					MSCaseInterpolateAtSampleSingleSample	(tcu::TestContext&		testCtx,
															 const std::string&		name,
															 tcu::UVec3				imageSize)
	: MSInterpolationCaseBase(testCtx, name, ImageMSParams(VK_SAMPLE_COUNT_1_BIT, imageSize)) {}

	void			init									(void);
	void			initPrograms							(vk::SourceCollections& programCollection) const;
	TestInstance*	createInstance							(Context&				context) const;
};

void MSCaseInterpolateAtSampleSingleSample::init (void)
{
	m_testCtx.getLog()
		<< tcu::TestLog::Message
		<< "Verifying that using interpolateAtSample with multisample buffers not available returns sample evaluated at the center of the pixel.\n"
		<< "	Interpolate varying containing screen space location.\n"
		<< "	=> fract(screen space location) should be (about) (0.5, 0.5)\n"
		<< tcu::TestLog::EndMessage;

	MSInterpolationCaseBase::init();
}

void MSCaseInterpolateAtSampleSingleSample::initPrograms (vk::SourceCollections& programCollection) const
{
	// Create vertex shader
	std::ostringstream vs;

	vs << "#version 440\n"
		<< "layout(location = 0) in vec4 vs_in_position_ndc;\n"
		<< "layout(location = 1) in vec2 vs_in_position_screen;\n"
		<< "\n"
		<< "layout(location = 0) out vec2 vs_out_position_screen;\n"
		<< "\n"
		<< "out gl_PerVertex {\n"
		<< "	vec4  gl_Position;\n"
		<< "};\n"
		<< "void main (void)\n"
		<< "{\n"
		<< "	gl_Position				= vs_in_position_ndc;\n"
		<< "	vs_out_position_screen	= vs_in_position_screen;\n"
		<< "}\n";

	programCollection.glslSources.add("vertex_shader") << glu::VertexSource(vs.str());

	// Create fragment shader
	std::ostringstream fs;

	fs << "#version 440\n"
		<< "layout(location = 0) in vec2 fs_in_position_screen;\n"
		<< "\n"
		<< "layout(location = 0) out vec2 fs_out_color;\n"
		<< "\n"
		<< "void main (void)\n"
		<< "{\n"
		<< "	const float threshold					= 0.15625;\n"
		<< "	const vec2  position_screen_at_sample	= interpolateAtSample(fs_in_position_screen, 0);\n"
		<< "	const vec2  position_inside_pixel		= fract(position_screen_at_sample);\n"
		<< "\n"
		<< "	if (abs(position_inside_pixel.x - 0.5) <= threshold && abs(position_inside_pixel.y - 0.5) <= threshold)\n"
		<< "		fs_out_color = vec2(0.0, 1.0);\n"
		<< "	else\n"
		<< "		fs_out_color = vec2(1.0, 0.0);\n"
		<< "}\n";

	programCollection.glslSources.add("fragment_shader") << glu::FragmentSource(fs.str());
}

TestInstance* MSCaseInterpolateAtSampleSingleSample::createInstance (Context& context) const
{
	if (!context.getDeviceFeatures().sampleRateShading)
		TCU_THROW(NotSupportedError, "sampleRateShading support required");

	return new MSInstanceInterpolateScreenPosition(context, m_imageMSParams);
}

class MSCaseInterpolateAtSampleIgnoresCentroid : public MSInterpolationCaseBase
{
public:
					MSCaseInterpolateAtSampleIgnoresCentroid(tcu::TestContext&		testCtx,
															 const std::string&		name,
															 const ImageMSParams&	imageMSParams)
	: MSInterpolationCaseBase(testCtx, name, imageMSParams) {}

	void			init									(void);
	void			initPrograms							(vk::SourceCollections& programCollection) const;
	TestInstance*	createInstance							(Context&				context) const;
};

MSInterpolationCaseBase* createMSCaseInterpolateAtSampleIgnoresCentroid (tcu::TestContext& testCtx, const std::string& name, const ImageMSParams& imageMSParams)
{
	return new MSCaseInterpolateAtSampleIgnoresCentroid(testCtx, name, imageMSParams);
}

void MSCaseInterpolateAtSampleIgnoresCentroid::init (void)
{
	m_testCtx.getLog()
		<< tcu::TestLog::Message
		<< "Verifying that interpolateAtSample ignores centroid qualifier.\n"
		<< "	Interpolate varying containing screen space location with centroid and sample qualifiers.\n"
		<< "	=> interpolateAtSample(screenSample, n) ~= interpolateAtSample(screenCentroid, n)\n"
		<< tcu::TestLog::EndMessage;

	MSInterpolationCaseBase::init();
}

void MSCaseInterpolateAtSampleIgnoresCentroid::initPrograms (vk::SourceCollections& programCollection) const
{
	// Create vertex shader
	std::ostringstream vs;

	vs << "#version 440\n"
		<< "layout(location = 0) in vec4 vs_in_position_ndc;\n"
		<< "layout(location = 1) in vec2 vs_in_position_screen;\n"
		<< "\n"
		<< "layout(location = 0) out vec2 vs_out_pos_screen_centroid;\n"
		<< "layout(location = 1) out vec2 vs_out_pos_screen_fragment;\n"
		<< "\n"
		<< "out gl_PerVertex {\n"
		<< "	vec4  gl_Position;\n"
		<< "};\n"
		<< "void main (void)\n"
		<< "{\n"
		<< "	gl_Position					= vs_in_position_ndc;\n"
		<< "	vs_out_pos_screen_centroid	= vs_in_position_screen;\n"
		<< "	vs_out_pos_screen_fragment	= vs_in_position_screen;\n"
		<< "}\n";

	programCollection.glslSources.add("vertex_shader") << glu::VertexSource(vs.str());

	// Create fragment shader
	std::ostringstream fs;

	fs << "#version 440\n"
		<< "layout(location = 0) centroid in vec2 fs_in_pos_screen_centroid;\n"
		<< "layout(location = 1)		  in vec2 fs_in_pos_screen_fragment;\n"
		<< "\n"
		<< "layout(location = 0) out vec2 fs_out_color;\n"
		<< "\n"
		<< "void main (void)\n"
		<< "{\n"
		<< "	const float threshold = 0.0005;\n"
		<< "\n"
		<< "	const vec2 position_a  = interpolateAtSample(fs_in_pos_screen_centroid, gl_SampleID);\n"
		<< "	const vec2 position_b  = interpolateAtSample(fs_in_pos_screen_fragment, gl_SampleID);\n"
		<< "	const bool valuesEqual = all(lessThan(abs(position_a - position_b), vec2(threshold)));\n"
		<< "\n"
		<< "	if (valuesEqual)\n"
		<< "		fs_out_color = vec2(0.0, 1.0);\n"
		<< "	else\n"
		<< "		fs_out_color = vec2(1.0, 0.0);\n"
		<< "}\n";

	programCollection.glslSources.add("fragment_shader") << glu::FragmentSource(fs.str());
}

TestInstance* MSCaseInterpolateAtSampleIgnoresCentroid::createInstance (Context& context) const
{
	if (!context.getDeviceFeatures().sampleRateShading)
		TCU_THROW(NotSupportedError, "sampleRateShading support required");

	return new MSInstanceInterpolateScreenPosition(context, m_imageMSParams);
}

class MSCaseInterpolateAtSampleConsistency : public MSInterpolationCaseBase
{
public:
					MSCaseInterpolateAtSampleConsistency	(tcu::TestContext&		testCtx,
															 const std::string&		name,
															 const ImageMSParams&	imageMSParams)
	: MSInterpolationCaseBase(testCtx, name, imageMSParams) {}

	void			init									(void);
	void			initPrograms							(vk::SourceCollections&	programCollection) const;
	TestInstance*	createInstance							(Context&				context) const;
};

MSInterpolationCaseBase* createMSCaseInterpolateAtSampleConsistency (tcu::TestContext& testCtx, const std::string& name, const ImageMSParams& imageMSParams)
{
	return new MSCaseInterpolateAtSampleConsistency(testCtx, name, imageMSParams);
}

void MSCaseInterpolateAtSampleConsistency::init (void)
{
	m_testCtx.getLog()
		<< tcu::TestLog::Message
		<< "Verifying that interpolateAtSample with the sample set to the current sampleID returns consistent values.\n"
		<< "	Interpolate varying containing screen space location with centroid and sample qualifiers.\n"
		<< "	=> interpolateAtSample(screenCentroid, sampleID) = screenSample\n"
		<< tcu::TestLog::EndMessage;

	MSInterpolationCaseBase::init();
}

void MSCaseInterpolateAtSampleConsistency::initPrograms (vk::SourceCollections& programCollection) const
{
	// Create vertex shader
	std::ostringstream vs;

	vs << "#version 440\n"
		<< "layout(location = 0) in vec4 vs_in_position_ndc;\n"
		<< "layout(location = 1) in vec2 vs_in_position_screen;\n"
		<< "\n"
		<< "layout(location = 0) out vec2 vs_out_pos_screen_centroid;\n"
		<< "layout(location = 1) out vec2 vs_out_pos_screen_sample;\n"
		<< "\n"
		<< "out gl_PerVertex {\n"
		<< "	vec4  gl_Position;\n"
		<< "};\n"
		<< "void main (void)\n"
		<< "{\n"
		<< "	gl_Position					= vs_in_position_ndc;\n"
		<< "	vs_out_pos_screen_centroid	= vs_in_position_screen;\n"
		<< "	vs_out_pos_screen_sample	= vs_in_position_screen;\n"
		<< "}\n";

	programCollection.glslSources.add("vertex_shader") << glu::VertexSource(vs.str());

	// Create fragment shader
	std::ostringstream fs;

	fs << "#version 440\n"
		<< "layout(location = 0) centroid in vec2 fs_in_pos_screen_centroid;\n"
		<< "layout(location = 1) sample   in vec2 fs_in_pos_screen_sample;\n"
		<< "\n"
		<< "layout(location = 0) out vec2 fs_out_color;\n"
		<< "\n"
		<< "void main (void)\n"
		<< "{\n"
		<< "	const float threshold = 0.15625;\n"
		<< "\n"
		<< "	const vec2  pos_interpolated_at_sample = interpolateAtSample(fs_in_pos_screen_centroid, gl_SampleID);\n"
		<< "	const bool  valuesEqual				   = all(lessThan(abs(pos_interpolated_at_sample - fs_in_pos_screen_sample), vec2(threshold)));\n"
		<< "\n"
		<< "	if (valuesEqual)\n"
		<< "		fs_out_color = vec2(0.0, 1.0);\n"
		<< "	else\n"
		<< "		fs_out_color = vec2(1.0, 0.0);\n"
		<< "}\n";

	programCollection.glslSources.add("fragment_shader") << glu::FragmentSource(fs.str());
}

TestInstance* MSCaseInterpolateAtSampleConsistency::createInstance (Context& context) const
{
	if (!context.getDeviceFeatures().sampleRateShading)
		TCU_THROW(NotSupportedError, "sampleRateShading support required");

	return new MSInstanceInterpolateScreenPosition(context, m_imageMSParams);
}

class MSCaseInterpolateAtCentroidConsistency : public MSInterpolationCaseBase
{
public:
					MSCaseInterpolateAtCentroidConsistency	(tcu::TestContext&		testCtx,
															 const std::string&		name,
															 const ImageMSParams&	imageMSParams)
	: MSInterpolationCaseBase(testCtx, name, imageMSParams) {}

	void			init									(void);
	void			initPrograms							(vk::SourceCollections&	programCollection) const;
	TestInstance*	createInstance							(Context&				context) const;
};

MSInterpolationCaseBase* createMSCaseInterpolateAtCentroidConsistency (tcu::TestContext& testCtx, const std::string& name, const ImageMSParams& imageMSParams)
{
	return new MSCaseInterpolateAtCentroidConsistency(testCtx, name, imageMSParams);
}

void MSCaseInterpolateAtCentroidConsistency::init (void)
{
	m_testCtx.getLog()
		<< tcu::TestLog::Message
		<< "Verifying that interpolateAtCentroid does not return different values than a corresponding centroid qualified varying.\n"
		<< "	Interpolate varying containing screen space location with sample and centroid qualifiers.\n"
		<< "	=> interpolateAtCentroid(screenSample) = screenCentroid\n"
		<< tcu::TestLog::EndMessage;

	MSInterpolationCaseBase::init();
}

void MSCaseInterpolateAtCentroidConsistency::initPrograms (vk::SourceCollections& programCollection) const
{
	// Create vertex shader
	std::ostringstream vs;

	vs << "#version 440\n"
		<< "layout(location = 0) in vec4 vs_in_position_ndc;\n"
		<< "layout(location = 1) in vec2 vs_in_position_screen;\n"
		<< "\n"
		<< "layout(location = 0) out vec2 vs_out_pos_screen_sample;\n"
		<< "layout(location = 1) out vec2 vs_out_pos_screen_centroid;\n"
		<< "\n"
		<< "out gl_PerVertex {\n"
		<< "	vec4  gl_Position;\n"
		<< "};\n"
		<< "void main (void)\n"
		<< "{\n"
		<< "	gl_Position					= vs_in_position_ndc;\n"
		<< "	vs_out_pos_screen_sample	= vs_in_position_screen;\n"
		<< "	vs_out_pos_screen_centroid	= vs_in_position_screen;\n"
		<< "}\n";

	programCollection.glslSources.add("vertex_shader") << glu::VertexSource(vs.str());

	// Create fragment shader
	std::ostringstream fs;

	fs << "#version 440\n"
		<< "layout(location = 0) sample   in vec2 fs_in_pos_screen_sample;\n"
		<< "layout(location = 1) centroid in vec2 fs_in_pos_screen_centroid;\n"
		<< "\n"
		<< "layout(location = 0) out vec2 fs_out_color;\n"
		<< "\n"
		<< "void main (void)\n"
		<< "{\n"
		<< "	const float threshold = 0.0005;\n"
		<< "\n"
		<< "	const vec2 pos_interpolated_at_centroid = interpolateAtCentroid(fs_in_pos_screen_sample);\n"
		<< "	const bool valuesEqual					= all(lessThan(abs(pos_interpolated_at_centroid - fs_in_pos_screen_centroid), vec2(threshold)));\n"
		<< "\n"
		<< "	if (valuesEqual)\n"
		<< "		fs_out_color = vec2(0.0, 1.0);\n"
		<< "	else\n"
		<< "		fs_out_color = vec2(1.0, 0.0);\n"
		<< "}\n";

	programCollection.glslSources.add("fragment_shader") << glu::FragmentSource(fs.str());
}

TestInstance* MSCaseInterpolateAtCentroidConsistency::createInstance (Context& context) const
{
	if (!context.getDeviceFeatures().sampleRateShading)
		TCU_THROW(NotSupportedError, "sampleRateShading support required");

	return new MSInstanceInterpolateScreenPosition(context, m_imageMSParams);
}

class MSCaseInterpolateAtOffsetPixelCenter : public MSInterpolationCaseBase
{
public:
					MSCaseInterpolateAtOffsetPixelCenter(tcu::TestContext&		testCtx,
														 const std::string&		name,
														 const ImageMSParams&	imageMSParams)
	: MSInterpolationCaseBase(testCtx, name, imageMSParams) {}

	void			init								(void);
	void			initPrograms						(vk::SourceCollections&	programCollection) const;
	TestInstance*	createInstance						(Context&				context) const;
};

MSInterpolationCaseBase* createMSCaseInterpolateAtOffsetPixelCenter (tcu::TestContext& testCtx, const std::string& name, const ImageMSParams& imageMSParams)
{
	return new MSCaseInterpolateAtOffsetPixelCenter(testCtx, name, imageMSParams);
}

void MSCaseInterpolateAtOffsetPixelCenter::init (void)
{
	m_testCtx.getLog()
		<< tcu::TestLog::Message
		<< "Verifying that interpolateAtOffset returns value sampled at an offset from the center of the pixel.\n"
		<< "	Interpolate varying containing screen space location.\n"
		<< "	=> interpolateAtOffset(screen, offset) should be \"varying value at the pixel center\" + offset"
		<< tcu::TestLog::EndMessage;

	MSInterpolationCaseBase::init();
}

void MSCaseInterpolateAtOffsetPixelCenter::initPrograms (vk::SourceCollections& programCollection) const
{
	// Create vertex shader
	std::ostringstream vs;

	vs << "#version 440\n"
		<< "layout(location = 0) in vec4 vs_in_position_ndc;\n"
		<< "layout(location = 1) in vec2 vs_in_position_screen;\n"
		<< "\n"
		<< "layout(location = 0) out vec2 vs_out_pos_screen;\n"
		<< "layout(location = 1) out vec2 vs_out_offset;\n"
		<< "\n"
		<< "out gl_PerVertex {\n"
		<< "	vec4  gl_Position;\n"
		<< "};\n"
		<< "void main (void)\n"
		<< "{\n"
		<< "	gl_Position			= vs_in_position_ndc;\n"
		<< "	vs_out_pos_screen	= vs_in_position_screen;\n"
		<< "	vs_out_offset		= vs_in_position_ndc.xy * 0.5;\n"
		<< "}\n";

	programCollection.glslSources.add("vertex_shader") << glu::VertexSource(vs.str());

	// Create fragment shader
	std::ostringstream fs;

	fs << "#version 440\n"
		<< "layout(location = 0) in  vec2 fs_in_pos_screen;\n"
		<< "layout(location = 1) in  vec2 fs_in_offset;\n"
		<< "\n"
		<< "layout(location = 0) out vec2 fs_out_color;\n"
		<< "\n"
		<< "void main (void)\n"
		<< "{\n"
		<< "    const vec2  frag_center = interpolateAtOffset(fs_in_pos_screen, vec2(0.0));\n"
		<< "    const vec2  center_diff = abs(frag_center - fs_in_pos_screen);\n"
		<< "    const float threshold   = 0.125;\n"
		<< "    bool        valuesEqual = false;\n"
		<< "\n"
		<< "    if (all(lessThan(center_diff, vec2(0.5 + threshold)))) {\n"
		<< "        const vec2 pos_interpolated_at_offset = interpolateAtOffset(fs_in_pos_screen, fs_in_offset);\n"
		<< "        const vec2 reference_value            = frag_center + fs_in_offset;\n"
		<< "\n"
		<< "        valuesEqual = all(lessThan(abs(pos_interpolated_at_offset - reference_value), vec2(threshold)));\n"
		<< "    }\n"
		<< "\n"
		<< "    if (valuesEqual)\n"
		<< "        fs_out_color = vec2(0.0, 1.0);\n"
		<< "    else\n"
		<< "        fs_out_color = vec2(1.0, 0.0);\n"
		<< "}\n";

	programCollection.glslSources.add("fragment_shader") << glu::FragmentSource(fs.str());
}

TestInstance* MSCaseInterpolateAtOffsetPixelCenter::createInstance (Context& context) const
{
	if (!context.getDeviceFeatures().sampleRateShading)
		TCU_THROW(NotSupportedError, "sampleRateShading support required");

	return new MSInstanceInterpolateScreenPosition(context, m_imageMSParams);
}

class MSCaseInterpolateAtOffsetSamplePosition : public MSInterpolationCaseBase
{
public:
					MSCaseInterpolateAtOffsetSamplePosition	(tcu::TestContext&		testCtx,
															 const std::string&		name,
															 const ImageMSParams&	imageMSParams)
	: MSInterpolationCaseBase(testCtx, name, imageMSParams) {}

	void			init									(void);
	void			initPrograms							(vk::SourceCollections&	programCollection) const;
	TestInstance*	createInstance							(Context&				context) const;
};

MSInterpolationCaseBase* createMSCaseInterpolateAtOffsetSamplePosition (tcu::TestContext& testCtx, const std::string& name, const ImageMSParams& imageMSParams)
{
	return new MSCaseInterpolateAtOffsetSamplePosition(testCtx, name, imageMSParams);
}

void MSCaseInterpolateAtOffsetSamplePosition::init (void)
{
	m_testCtx.getLog()
		<< tcu::TestLog::Message
		<< "Verifying that interpolateAtOffset of screen position with the offset of current sample position returns value "
		<< "similar to screen position interpolated at sample.\n"
		<< "	Interpolate varying containing screen space location with and without sample qualifier.\n"
		<< "	=> interpolateAtOffset(screenFragment, samplePosition - (0.5,0.5)) = screenSample"
		<< tcu::TestLog::EndMessage;

	MSInterpolationCaseBase::init();
}

void MSCaseInterpolateAtOffsetSamplePosition::initPrograms (vk::SourceCollections& programCollection) const
{
	// Create vertex shader
	std::ostringstream vs;

	vs << "#version 440\n"
		<< "layout(location = 0) in vec4 vs_in_position_ndc;\n"
		<< "layout(location = 1) in vec2 vs_in_position_screen;\n"
		<< "\n"
		<< "layout(location = 0) out vec2 vs_out_pos_screen_fragment;\n"
		<< "layout(location = 1) out vec2 vs_out_pos_screen_sample;\n"
		<< "\n"
		<< "out gl_PerVertex {\n"
		<< "	vec4  gl_Position;\n"
		<< "};\n"
		<< "void main (void)\n"
		<< "{\n"
		<< "	gl_Position					= vs_in_position_ndc;\n"
		<< "	vs_out_pos_screen_fragment	= vs_in_position_screen;\n"
		<< "	vs_out_pos_screen_sample	= vs_in_position_screen;\n"
		<< "}\n";

	programCollection.glslSources.add("vertex_shader") << glu::VertexSource(vs.str());

	// Create fragment shader
	std::ostringstream fs;

	fs << "#version 440\n"
		<< "layout(location = 0)		in vec2 fs_in_pos_screen_fragment;\n"
		<< "layout(location = 1) sample in vec2 fs_in_pos_screen_sample;\n"
		<< "\n"
		<< "layout(location = 0) out vec2 fs_out_color;\n"
		<< "\n"
		<< "void main (void)\n"
		<< "{\n"
		<< "	const float threshold = 0.15625;\n"
		<< "\n"
		<< "	const vec2 offset					  = gl_SamplePosition - vec2(0.5, 0.5);\n"
		<< "	const vec2 pos_interpolated_at_offset = interpolateAtOffset(fs_in_pos_screen_fragment, offset);\n"
		<< "	const bool valuesEqual				  = all(lessThan(abs(pos_interpolated_at_offset - fs_in_pos_screen_sample), vec2(threshold)));\n"
		<< "\n"
		<< "	if (valuesEqual)\n"
		<< "		fs_out_color = vec2(0.0, 1.0);\n"
		<< "	else\n"
		<< "		fs_out_color = vec2(1.0, 0.0);\n"
		<< "}\n";

	programCollection.glslSources.add("fragment_shader") << glu::FragmentSource(fs.str());
}

TestInstance* MSCaseInterpolateAtOffsetSamplePosition::createInstance (Context& context) const
{
	if (!context.getDeviceFeatures().sampleRateShading)
		TCU_THROW(NotSupportedError, "sampleRateShading support required");

	return new MSInstanceInterpolateScreenPosition(context, m_imageMSParams);
}

class MSInstanceInterpolateBarycentricCoordinates : public MSInterpolationInstanceBase
{
public:
					MSInstanceInterpolateBarycentricCoordinates	(Context&				context,
																 const ImageMSParams&	imageMSParams)
	: MSInterpolationInstanceBase(context, imageMSParams) {}

	VertexDataDesc	getVertexDataDescripton						(void) const;
	void			uploadVertexData							(const Allocation& vertexBufferAllocation, const VertexDataDesc& vertexDataDescripton) const;
	tcu::TestStatus	verifyResolvedImage							(const tcu::ConstPixelBufferAccess&	imageData) const;

protected:
	struct VertexData
	{
		VertexData(const tcu::Vec4& posNdc, const tcu::Vec3& barCoord) : positionNdc(posNdc), barycentricCoord(barCoord) {}

		tcu::Vec4 positionNdc;
		tcu::Vec3 barycentricCoord;
	};
};

MSInterpolationInstanceBase::VertexDataDesc MSInstanceInterpolateBarycentricCoordinates::getVertexDataDescripton (void) const
{
	VertexDataDesc vertexDataDesc;

	vertexDataDesc.verticesCount		= 3u;
	vertexDataDesc.dataStride			= sizeof(VertexData);
	vertexDataDesc.dataSize				= vertexDataDesc.verticesCount * vertexDataDesc.dataStride;
	vertexDataDesc.primitiveTopology	= VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

	const VkVertexInputAttributeDescription vertexAttribPositionNdc =
	{
		0u,											// deUint32	location;
		0u,											// deUint32	binding;
		VK_FORMAT_R32G32B32A32_SFLOAT,				// VkFormat	format;
		DE_OFFSET_OF(VertexData, positionNdc),		// deUint32	offset;
	};

	vertexDataDesc.vertexAttribDescVec.push_back(vertexAttribPositionNdc);

	const VkVertexInputAttributeDescription vertexAttrBarCoord =
	{
		1u,											// deUint32	location;
		0u,											// deUint32	binding;
		VK_FORMAT_R32G32B32_SFLOAT,					// VkFormat	format;
		DE_OFFSET_OF(VertexData, barycentricCoord),	// deUint32	offset;
	};

	vertexDataDesc.vertexAttribDescVec.push_back(vertexAttrBarCoord);

	return vertexDataDesc;
}

void MSInstanceInterpolateBarycentricCoordinates::uploadVertexData (const Allocation& vertexBufferAllocation, const VertexDataDesc& vertexDataDescripton) const
{
	// Create buffer storing vertex data
	std::vector<VertexData> vertices;

	vertices.push_back(VertexData(tcu::Vec4(-1.0f,-1.0f, 0.0f, 1.0f), tcu::Vec3(0.0f, 0.0f, 1.0f)));
	vertices.push_back(VertexData(tcu::Vec4(-1.0f, 1.0f, 0.0f, 1.0f), tcu::Vec3(1.0f, 0.0f, 0.0f)));
	vertices.push_back(VertexData(tcu::Vec4( 1.0f,-1.0f, 0.0f, 1.0f), tcu::Vec3(0.0f, 1.0f, 0.0f)));

	deMemcpy(vertexBufferAllocation.getHostPtr(), dataPointer(vertices), static_cast<std::size_t>(vertexDataDescripton.dataSize));
}

tcu::TestStatus MSInstanceInterpolateBarycentricCoordinates::verifyResolvedImage (const tcu::ConstPixelBufferAccess& imageData) const
{
	for (deInt32 z = 0u; z < imageData.getDepth();  ++z)
	for (deInt32 y = 0u; y < imageData.getHeight(); ++y)
	for (deInt32 x = 0u; x < imageData.getWidth();  ++x)
	{
		const deInt32 firstComponent = imageData.getPixelInt(x, y, z).x();

		if (firstComponent > 0)
			return tcu::TestStatus::fail("Failed");
	}

	return tcu::TestStatus::pass("Passed");
}

class MSCaseCentroidQualifierInsidePrimitive : public MSInterpolationCaseBase
{
public:
					MSCaseCentroidQualifierInsidePrimitive	(tcu::TestContext&		testCtx,
															 const std::string&		name,
															 const ImageMSParams&	imageMSParams)
	: MSInterpolationCaseBase(testCtx, name, imageMSParams) {}

	void			init									(void);
	void			initPrograms							(vk::SourceCollections&	programCollection) const;
	TestInstance*	createInstance							(Context&				context) const;
};

MSInterpolationCaseBase* createMSCaseCentroidQualifierInsidePrimitive (tcu::TestContext& testCtx, const std::string& name, const ImageMSParams& imageMSParams)
{
	return new MSCaseCentroidQualifierInsidePrimitive(testCtx, name, imageMSParams);
}

void MSCaseCentroidQualifierInsidePrimitive::init (void)
{
	m_testCtx.getLog()
		<< tcu::TestLog::Message
		<< "Verifying that varying qualified with centroid is interpolated at location inside both the pixel and the primitive being processed.\n"
		<< "	Interpolate triangle's barycentric coordinates with centroid qualifier.\n"
		<< "	=> After interpolation we expect barycentric.xyz >= 0.0 && barycentric.xyz <= 1.0\n"
		<< tcu::TestLog::EndMessage;

	MSInterpolationCaseBase::init();
}

void MSCaseCentroidQualifierInsidePrimitive::initPrograms (vk::SourceCollections& programCollection) const
{
	// Create vertex shader
	std::ostringstream vs;

	vs << "#version 440\n"
		<< "layout(location = 0) in vec4 vs_in_position_ndc;\n"
		<< "layout(location = 1) in vec3 vs_in_barCoord;\n"
		<< "\n"
		<< "layout(location = 0) out vec3 vs_out_barCoord;\n"
		<< "\n"
		<< "out gl_PerVertex {\n"
		<< "	vec4  gl_Position;\n"
		<< "};\n"
		<< "void main (void)\n"
		<< "{\n"
		<< "	gl_Position		= vs_in_position_ndc;\n"
		<< "	vs_out_barCoord = vs_in_barCoord;\n"
		<< "}\n";

	programCollection.glslSources.add("vertex_shader") << glu::VertexSource(vs.str());

	// Create fragment shader
	std::ostringstream fs;

	fs << "#version 440\n"
		<< "layout(location = 0) centroid in vec3 fs_in_barCoord;\n"
		<< "\n"
		<< "layout(location = 0) out vec2 fs_out_color;\n"
		<< "\n"
		<< "void main (void)\n"
		<< "{\n"
		<< "	if( all(greaterThanEqual(fs_in_barCoord, vec3(0.0))) && all(lessThanEqual(fs_in_barCoord, vec3(1.0))) )\n"
		<< "			fs_out_color = vec2(0.0, 1.0);\n"
		<< "	else\n"
		<< "			fs_out_color = vec2(1.0, 0.0);\n"
		<< "}\n";

	programCollection.glslSources.add("fragment_shader") << glu::FragmentSource(fs.str());
}

TestInstance* MSCaseCentroidQualifierInsidePrimitive::createInstance (Context& context) const
{
	return new MSInstanceInterpolateBarycentricCoordinates(context, m_imageMSParams);
}

} // multisample

tcu::TestCaseGroup* makeGroup(	multisample::MSInterpolationCaseFuncPtr	createCaseFuncPtr,
								tcu::TestContext&						testCtx,
								const std::string						groupName,
								const tcu::UVec3						imageSizes[],
								const deUint32							imageSizesElemCount,
								const vk::VkSampleCountFlagBits			imageSamples[],
								const deUint32							imageSamplesElemCount)
{
	de::MovePtr<tcu::TestCaseGroup> caseGroup(new tcu::TestCaseGroup(testCtx, groupName.c_str(), ""));

	for (deUint32 imageSizeNdx = 0u; imageSizeNdx < imageSizesElemCount; ++imageSizeNdx)
	{
		const tcu::UVec3	imageSize = imageSizes[imageSizeNdx];
		std::ostringstream	imageSizeStream;

		imageSizeStream << imageSize.x() << "_" << imageSize.y() << "_" << imageSize.z();

		de::MovePtr<tcu::TestCaseGroup> sizeGroup(new tcu::TestCaseGroup(testCtx, imageSizeStream.str().c_str(), ""));

		for (deUint32 imageSamplesNdx = 0u; imageSamplesNdx < imageSamplesElemCount; ++imageSamplesNdx)
		{
			const vk::VkSampleCountFlagBits		samples			= imageSamples[imageSamplesNdx];
			const multisample::ImageMSParams	imageMSParams	= multisample::ImageMSParams(samples, imageSize);

			sizeGroup->addChild(createCaseFuncPtr(testCtx, "samples_" + de::toString(samples), imageMSParams));
		}

		caseGroup->addChild(sizeGroup.release());
	}
	return caseGroup.release();
}

tcu::TestCaseGroup* createMultisampleInterpolationTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> testGroup(new tcu::TestCaseGroup(testCtx, "multisample_interpolation", "Multisample Interpolation"));

	const tcu::UVec3 imageSizes[] =
	{
		tcu::UVec3(128u, 128u, 1u),
		tcu::UVec3(137u, 191u, 1u),
	};

	const deUint32 sizesElemCount = static_cast<deUint32>(sizeof(imageSizes) / sizeof(tcu::UVec3));

	const vk::VkSampleCountFlagBits imageSamples[] =
	{
		vk::VK_SAMPLE_COUNT_2_BIT,
		vk::VK_SAMPLE_COUNT_4_BIT,
		vk::VK_SAMPLE_COUNT_8_BIT,
		vk::VK_SAMPLE_COUNT_16_BIT,
		vk::VK_SAMPLE_COUNT_32_BIT,
		vk::VK_SAMPLE_COUNT_64_BIT,
	};

	const deUint32 samplesElemCount = static_cast<deUint32>(sizeof(imageSamples) / sizeof(vk::VkSampleCountFlagBits));

	de::MovePtr<tcu::TestCaseGroup> caseGroup(new tcu::TestCaseGroup(testCtx, "sample_interpolate_at_single_sample_", ""));

	for (deUint32 imageSizeNdx = 0u; imageSizeNdx < sizesElemCount; ++imageSizeNdx)
	{
		const tcu::UVec3	imageSize = imageSizes[imageSizeNdx];
		std::ostringstream	imageSizeStream;

		imageSizeStream << imageSize.x() << "_" << imageSize.y() << "_" << imageSize.z();

		de::MovePtr<tcu::TestCaseGroup> sizeGroup(new tcu::TestCaseGroup(testCtx, imageSizeStream.str().c_str(), ""));

		sizeGroup->addChild(new multisample::MSCaseInterpolateAtSampleSingleSample(testCtx, "samples_" + de::toString(1), imageSize));

		caseGroup->addChild(sizeGroup.release());
	}

	testGroup->addChild(caseGroup.release());

	testGroup->addChild(makeGroup(multisample::createMSCaseInterpolateAtSampleDistinctValues,	testCtx, "sample_interpolate_at_distinct_values",	imageSizes, sizesElemCount, imageSamples, samplesElemCount));
	testGroup->addChild(makeGroup(multisample::createMSCaseInterpolateAtSampleIgnoresCentroid,	testCtx, "sample_interpolate_at_ignores_centroid",	imageSizes, sizesElemCount, imageSamples, samplesElemCount));
	testGroup->addChild(makeGroup(multisample::createMSCaseInterpolateAtSampleConsistency,		testCtx, "sample_interpolate_at_consistency",		imageSizes, sizesElemCount, imageSamples, samplesElemCount));
	testGroup->addChild(makeGroup(multisample::createMSCaseSampleQualifierDistinctValues,		testCtx, "sample_qualifier_distinct_values",		imageSizes, sizesElemCount, imageSamples, samplesElemCount));
	testGroup->addChild(makeGroup(multisample::createMSCaseInterpolateAtCentroidConsistency,	testCtx, "centroid_interpolate_at_consistency",		imageSizes, sizesElemCount, imageSamples, samplesElemCount));
	testGroup->addChild(makeGroup(multisample::createMSCaseCentroidQualifierInsidePrimitive,	testCtx, "centroid_qualifier_inside_primitive",		imageSizes, sizesElemCount, imageSamples, samplesElemCount));
	testGroup->addChild(makeGroup(multisample::createMSCaseInterpolateAtOffsetPixelCenter,		testCtx, "offset_interpolate_at_pixel_center",		imageSizes, sizesElemCount, imageSamples, samplesElemCount));
	testGroup->addChild(makeGroup(multisample::createMSCaseInterpolateAtOffsetSamplePosition,	testCtx, "offset_interpolate_at_sample_position",	imageSizes, sizesElemCount, imageSamples, samplesElemCount));

	return testGroup.release();
}

} // pipeline
} // vkt
