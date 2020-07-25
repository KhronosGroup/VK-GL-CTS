/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2016 The Khronos Group Inc.
 * Copyright (c) 2016 Samsung Electronics Co., Ltd.
 * Copyright (c) 2014 The Android Open Source Project
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
 * \brief Functional rasterization tests.
 *//*--------------------------------------------------------------------*/

#include "vktTestGroupUtil.hpp"
#include "vktAmberTestCase.hpp"
#include "vktRasterizationTests.hpp"
#include "vktRasterizationFragShaderSideEffectsTests.hpp"
#include "tcuRasterizationVerifier.hpp"
#include "tcuSurface.hpp"
#include "tcuRenderTarget.hpp"
#include "tcuVectorUtil.hpp"
#include "tcuStringTemplate.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuResultCollector.hpp"
#include "vkImageUtil.hpp"
#include "deStringUtil.hpp"
#include "deRandom.hpp"
#include "vktTestCase.hpp"
#include "vktTestCaseUtil.hpp"
#include "vkPrograms.hpp"
#include "vkMemUtil.hpp"
#include "vkRefUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkImageWithMemory.hpp"
#include "vkBarrierUtil.hpp"

#include <vector>
#include <sstream>

using namespace vk;

namespace vkt
{
namespace rasterization
{
namespace
{

using tcu::RasterizationArguments;
using tcu::TriangleSceneSpec;
using tcu::PointSceneSpec;
using tcu::LineSceneSpec;
using tcu::LineInterpolationMethod;

static const char* const s_shaderVertexTemplate =	"#version 310 es\n"
													"layout(location = 0) in highp vec4 a_position;\n"
													"layout(location = 1) in highp vec4 a_color;\n"
													"layout(location = 0) ${INTERPOLATION}out highp vec4 v_color;\n"
													"layout (set=0, binding=0) uniform PointSize {\n"
													"	highp float u_pointSize;\n"
													"};\n"
													"void main ()\n"
													"{\n"
													"	gl_Position = a_position;\n"
													"	gl_PointSize = u_pointSize;\n"
													"	v_color = a_color;\n"
													"}\n";

static const char* const s_shaderFragmentTemplate =	"#version 310 es\n"
													"layout(location = 0) out highp vec4 fragColor;\n"
													"layout(location = 0) ${INTERPOLATION}in highp vec4 v_color;\n"
													"void main ()\n"
													"{\n"
													"	fragColor = v_color;\n"
													"}\n";
enum InterpolationCaseFlags
{
	INTERPOLATIONFLAGS_NONE = 0,
	INTERPOLATIONFLAGS_PROJECTED = (1 << 1),
	INTERPOLATIONFLAGS_FLATSHADE = (1 << 2),
};

enum ResolutionValues
{
	RESOLUTION_POT = 256,
	RESOLUTION_NPOT = 258
};

enum PrimitiveWideness
{
	PRIMITIVEWIDENESS_NARROW = 0,
	PRIMITIVEWIDENESS_WIDE,

	PRIMITIVEWIDENESS_LAST
};

enum LineStipple
{
	LINESTIPPLE_DISABLED = 0,
	LINESTIPPLE_STATIC,
	LINESTIPPLE_DYNAMIC,

	LINESTIPPLE_LAST
};

static const deUint32 lineStippleFactor = 2;
static const deUint32 lineStipplePattern = 0x0F0F;

enum PrimitiveStrictness
{
	PRIMITIVESTRICTNESS_STRICT = 0,
	PRIMITIVESTRICTNESS_NONSTRICT,
	PRIMITIVESTRICTNESS_IGNORE,

	PRIMITIVESTRICTNESS_LAST
};

class BaseRenderingTestCase : public TestCase
{
public:
								BaseRenderingTestCase	(tcu::TestContext& context, const std::string& name, const std::string& description, VkSampleCountFlagBits sampleCount = VK_SAMPLE_COUNT_1_BIT, deBool flatshade = DE_FALSE);
	virtual						~BaseRenderingTestCase	(void);

	virtual void				initPrograms			(vk::SourceCollections& programCollection) const;

protected:
	const VkSampleCountFlagBits	m_sampleCount;
	const deBool				m_flatshade;
};

BaseRenderingTestCase::BaseRenderingTestCase (tcu::TestContext& context, const std::string& name, const std::string& description, VkSampleCountFlagBits sampleCount, deBool flatshade)
	: TestCase(context, name, description)
	, m_sampleCount	(sampleCount)
	, m_flatshade	(flatshade)
{
}

void BaseRenderingTestCase::initPrograms (vk::SourceCollections& programCollection) const
{
	tcu::StringTemplate					vertexSource	(s_shaderVertexTemplate);
	tcu::StringTemplate					fragmentSource	(s_shaderFragmentTemplate);
	std::map<std::string, std::string>	params;

	params["INTERPOLATION"] = (m_flatshade) ? ("flat ") : ("");

	programCollection.glslSources.add("vertext_shader") << glu::VertexSource(vertexSource.specialize(params));
	programCollection.glslSources.add("fragment_shader") << glu::FragmentSource(fragmentSource.specialize(params));
}

BaseRenderingTestCase::~BaseRenderingTestCase (void)
{
}

class BaseRenderingTestInstance : public TestInstance
{
public:
													BaseRenderingTestInstance		(Context& context, VkSampleCountFlagBits sampleCount = VK_SAMPLE_COUNT_1_BIT, deUint32 renderSize = RESOLUTION_POT, VkFormat imageFormat = VK_FORMAT_R8G8B8A8_UNORM, deUint32 additionalRenderSize = 0);
													~BaseRenderingTestInstance		(void);

protected:
	void											addImageTransitionBarrier		(VkCommandBuffer commandBuffer, VkImage image, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask, VkImageLayout oldLayout, VkImageLayout newLayout) const;
	void											drawPrimitives					(tcu::Surface& result, const std::vector<tcu::Vec4>& vertexData, VkPrimitiveTopology primitiveTopology);
	void											drawPrimitives					(tcu::Surface& result, const std::vector<tcu::Vec4>& vertexData, const std::vector<tcu::Vec4>& coloDrata, VkPrimitiveTopology primitiveTopology);
	void											drawPrimitives					(tcu::Surface& result, const std::vector<tcu::Vec4>& positionData, const std::vector<tcu::Vec4>& colorData, VkPrimitiveTopology primitiveTopology,
																						VkImage image, VkImage resolvedImage, VkFramebuffer frameBuffer, const deUint32 renderSize, VkBuffer resultBuffer, const Allocation& resultBufferMemory);
	virtual float									getLineWidth					(void) const;
	virtual float									getPointSize					(void) const;
	virtual bool									getLineStippleDynamic			(void) const { return false; };

	virtual
	const VkPipelineRasterizationStateCreateInfo*	getRasterizationStateCreateInfo	(void) const;

	virtual
	VkPipelineRasterizationLineStateCreateInfoEXT*	getLineRasterizationStateCreateInfo	(void);

	virtual
	const VkPipelineColorBlendStateCreateInfo*		getColorBlendStateCreateInfo	(void) const;

	const tcu::TextureFormat&						getTextureFormat				(void) const;

	const deUint32									m_renderSize;
	const VkSampleCountFlagBits						m_sampleCount;
	deUint32										m_subpixelBits;
	const deBool									m_multisampling;

	const VkFormat									m_imageFormat;
	const tcu::TextureFormat						m_textureFormat;
	Move<VkCommandPool>								m_commandPool;

	Move<VkImage>									m_image;
	de::MovePtr<Allocation>							m_imageMemory;
	Move<VkImageView>								m_imageView;

	Move<VkImage>									m_resolvedImage;
	de::MovePtr<Allocation>							m_resolvedImageMemory;
	Move<VkImageView>								m_resolvedImageView;

	Move<VkRenderPass>								m_renderPass;
	Move<VkFramebuffer>								m_frameBuffer;

	Move<VkDescriptorPool>							m_descriptorPool;
	Move<VkDescriptorSet>							m_descriptorSet;
	Move<VkDescriptorSetLayout>						m_descriptorSetLayout;

	Move<VkBuffer>									m_uniformBuffer;
	de::MovePtr<Allocation>							m_uniformBufferMemory;
	const VkDeviceSize								m_uniformBufferSize;

	Move<VkPipelineLayout>							m_pipelineLayout;

	Move<VkShaderModule>							m_vertexShaderModule;
	Move<VkShaderModule>							m_fragmentShaderModule;

	Move<VkBuffer>									m_resultBuffer;
	de::MovePtr<Allocation>							m_resultBufferMemory;
	const VkDeviceSize								m_resultBufferSize;

	const deUint32									m_additionalRenderSize;
	const VkDeviceSize								m_additionalResultBufferSize;

	VkPipelineRasterizationLineStateCreateInfoEXT	m_lineRasterizationStateInfo;
};

BaseRenderingTestInstance::BaseRenderingTestInstance (Context& context, VkSampleCountFlagBits sampleCount, deUint32 renderSize, VkFormat imageFormat, deUint32 additionalRenderSize)
	: TestInstance			(context)
	, m_renderSize			(renderSize)
	, m_sampleCount			(sampleCount)
	, m_subpixelBits		(context.getDeviceProperties().limits.subPixelPrecisionBits)
	, m_multisampling		(m_sampleCount != VK_SAMPLE_COUNT_1_BIT)
	, m_imageFormat			(imageFormat)
	, m_textureFormat		(vk::mapVkFormat(m_imageFormat))
	, m_uniformBufferSize	(sizeof(float))
	, m_resultBufferSize	(renderSize * renderSize * m_textureFormat.getPixelSize())
	, m_additionalRenderSize(additionalRenderSize)
	, m_additionalResultBufferSize(additionalRenderSize * additionalRenderSize * m_textureFormat.getPixelSize())
{
	const DeviceInterface&						vkd						= m_context.getDeviceInterface();
	const VkDevice								vkDevice				= m_context.getDevice();
	const deUint32								queueFamilyIndex		= m_context.getUniversalQueueFamilyIndex();
	Allocator&									allocator				= m_context.getDefaultAllocator();
	DescriptorPoolBuilder						descriptorPoolBuilder;
	DescriptorSetLayoutBuilder					descriptorSetLayoutBuilder;

	deMemset(&m_lineRasterizationStateInfo, 0, sizeof(m_lineRasterizationStateInfo));

	// Command Pool
	m_commandPool = createCommandPool(vkd, vkDevice, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex);

	// Image
	{
		const VkImageUsageFlags	imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
		VkImageFormatProperties	properties;

		if ((m_context.getInstanceInterface().getPhysicalDeviceImageFormatProperties(m_context.getPhysicalDevice(),
																					 m_imageFormat,
																					 VK_IMAGE_TYPE_2D,
																					 VK_IMAGE_TILING_OPTIMAL,
																					 imageUsage,
																					 0,
																					 &properties) == VK_ERROR_FORMAT_NOT_SUPPORTED))
		{
			TCU_THROW(NotSupportedError, "Format not supported");
		}

		if ((properties.sampleCounts & m_sampleCount) != m_sampleCount)
		{
			TCU_THROW(NotSupportedError, "Format not supported");
		}

		const VkImageCreateInfo					imageCreateInfo			=
		{
			VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,		// VkStructureType			sType;
			DE_NULL,									// const void*				pNext;
			0u,											// VkImageCreateFlags		flags;
			VK_IMAGE_TYPE_2D,							// VkImageType				imageType;
			m_imageFormat,								// VkFormat					format;
			{ m_renderSize,	m_renderSize, 1u },			// VkExtent3D				extent;
			1u,											// deUint32					mipLevels;
			1u,											// deUint32					arrayLayers;
			m_sampleCount,								// VkSampleCountFlagBits	samples;
			VK_IMAGE_TILING_OPTIMAL,					// VkImageTiling			tiling;
			imageUsage,									// VkImageUsageFlags		usage;
			VK_SHARING_MODE_EXCLUSIVE,					// VkSharingMode			sharingMode;
			1u,											// deUint32					queueFamilyIndexCount;
			&queueFamilyIndex,							// const deUint32*			pQueueFamilyIndices;
			VK_IMAGE_LAYOUT_UNDEFINED					// VkImageLayout			initialLayout;
		};

		m_image = vk::createImage(vkd, vkDevice, &imageCreateInfo, DE_NULL);

		m_imageMemory	= allocator.allocate(getImageMemoryRequirements(vkd, vkDevice, *m_image), MemoryRequirement::Any);
		VK_CHECK(vkd.bindImageMemory(vkDevice, *m_image, m_imageMemory->getMemory(), m_imageMemory->getOffset()));
	}

	// Image View
	{
		const VkImageViewCreateInfo				imageViewCreateInfo		=
		{
			VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,	// VkStructureType				sType;
			DE_NULL,									// const void*					pNext;
			0u,											// VkImageViewCreateFlags		flags;
			*m_image,									// VkImage						image;
			VK_IMAGE_VIEW_TYPE_2D,						// VkImageViewType				viewType;
			m_imageFormat,								// VkFormat						format;
			makeComponentMappingRGBA(),					// VkComponentMapping			components;
			{
				VK_IMAGE_ASPECT_COLOR_BIT,					// VkImageAspectFlags			aspectMask;
				0u,											// deUint32						baseMipLevel;
				1u,											// deUint32						mipLevels;
				0u,											// deUint32						baseArrayLayer;
				1u,											// deUint32						arraySize;
			},											// VkImageSubresourceRange		subresourceRange;
		};

		m_imageView = vk::createImageView(vkd, vkDevice, &imageViewCreateInfo, DE_NULL);
	}

	if (m_multisampling)
	{
		{
			// Resolved Image
			const VkImageUsageFlags	imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
			VkImageFormatProperties	properties;

			if ((m_context.getInstanceInterface().getPhysicalDeviceImageFormatProperties(m_context.getPhysicalDevice(),
																						 m_imageFormat,
																						 VK_IMAGE_TYPE_2D,
																						 VK_IMAGE_TILING_OPTIMAL,
																						 imageUsage,
																						 0,
																						 &properties) == VK_ERROR_FORMAT_NOT_SUPPORTED))
			{
				TCU_THROW(NotSupportedError, "Format not supported");
			}

			const VkImageCreateInfo					imageCreateInfo			=
			{
				VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,		// VkStructureType			sType;
				DE_NULL,									// const void*				pNext;
				0u,											// VkImageCreateFlags		flags;
				VK_IMAGE_TYPE_2D,							// VkImageType				imageType;
				m_imageFormat,								// VkFormat					format;
				{ m_renderSize,	m_renderSize, 1u },			// VkExtent3D				extent;
				1u,											// deUint32					mipLevels;
				1u,											// deUint32					arrayLayers;
				VK_SAMPLE_COUNT_1_BIT,						// VkSampleCountFlagBits	samples;
				VK_IMAGE_TILING_OPTIMAL,					// VkImageTiling			tiling;
				imageUsage,									// VkImageUsageFlags		usage;
				VK_SHARING_MODE_EXCLUSIVE,					// VkSharingMode			sharingMode;
				1u,											// deUint32					queueFamilyIndexCount;
				&queueFamilyIndex,							// const deUint32*			pQueueFamilyIndices;
				VK_IMAGE_LAYOUT_UNDEFINED					// VkImageLayout			initialLayout;
			};

			m_resolvedImage			= vk::createImage(vkd, vkDevice, &imageCreateInfo, DE_NULL);
			m_resolvedImageMemory	= allocator.allocate(getImageMemoryRequirements(vkd, vkDevice, *m_resolvedImage), MemoryRequirement::Any);
			VK_CHECK(vkd.bindImageMemory(vkDevice, *m_resolvedImage, m_resolvedImageMemory->getMemory(), m_resolvedImageMemory->getOffset()));
		}

		// Resolved Image View
		{
			const VkImageViewCreateInfo				imageViewCreateInfo		=
			{
				VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,	// VkStructureType				sType;
				DE_NULL,									// const void*					pNext;
				0u,											// VkImageViewCreateFlags		flags;
				*m_resolvedImage,							// VkImage						image;
				VK_IMAGE_VIEW_TYPE_2D,						// VkImageViewType				viewType;
				m_imageFormat,								// VkFormat						format;
				makeComponentMappingRGBA(),					// VkComponentMapping			components;
				{
					VK_IMAGE_ASPECT_COLOR_BIT,					// VkImageAspectFlags			aspectMask;
					0u,											// deUint32						baseMipLevel;
					1u,											// deUint32						mipLevels;
					0u,											// deUint32						baseArrayLayer;
					1u,											// deUint32						arraySize;
				},											// VkImageSubresourceRange		subresourceRange;
			};

			m_resolvedImageView = vk::createImageView(vkd, vkDevice, &imageViewCreateInfo, DE_NULL);
		}

	}

	// Render Pass
	{
		const VkImageLayout						imageLayout				= VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		const VkAttachmentDescription			attachmentDesc[]		=
		{
			{
				0u,													// VkAttachmentDescriptionFlags		flags;
				m_imageFormat,										// VkFormat							format;
				m_sampleCount,										// VkSampleCountFlagBits			samples;
				VK_ATTACHMENT_LOAD_OP_CLEAR,						// VkAttachmentLoadOp				loadOp;
				VK_ATTACHMENT_STORE_OP_STORE,						// VkAttachmentStoreOp				storeOp;
				VK_ATTACHMENT_LOAD_OP_DONT_CARE,					// VkAttachmentLoadOp				stencilLoadOp;
				VK_ATTACHMENT_STORE_OP_DONT_CARE,					// VkAttachmentStoreOp				stencilStoreOp;
				imageLayout,										// VkImageLayout					initialLayout;
				imageLayout,										// VkImageLayout					finalLayout;
			},
			{
				0u,													// VkAttachmentDescriptionFlags		flags;
				m_imageFormat,										// VkFormat							format;
				VK_SAMPLE_COUNT_1_BIT,								// VkSampleCountFlagBits			samples;
				VK_ATTACHMENT_LOAD_OP_DONT_CARE,					// VkAttachmentLoadOp				loadOp;
				VK_ATTACHMENT_STORE_OP_STORE,						// VkAttachmentStoreOp				storeOp;
				VK_ATTACHMENT_LOAD_OP_DONT_CARE,					// VkAttachmentLoadOp				stencilLoadOp;
				VK_ATTACHMENT_STORE_OP_DONT_CARE,					// VkAttachmentStoreOp				stencilStoreOp;
				imageLayout,										// VkImageLayout					initialLayout;
				imageLayout,										// VkImageLayout					finalLayout;
			}
		};

		const VkAttachmentReference				attachmentRef			=
		{
			0u,													// deUint32							attachment;
			imageLayout,										// VkImageLayout					layout;
		};

		const VkAttachmentReference				resolveAttachmentRef	=
		{
			1u,													// deUint32							attachment;
			imageLayout,										// VkImageLayout					layout;
		};

		const VkSubpassDescription				subpassDesc				=
		{
			0u,													// VkSubpassDescriptionFlags		flags;
			VK_PIPELINE_BIND_POINT_GRAPHICS,					// VkPipelineBindPoint				pipelineBindPoint;
			0u,													// deUint32							inputAttachmentCount;
			DE_NULL,											// const VkAttachmentReference*		pInputAttachments;
			1u,													// deUint32							colorAttachmentCount;
			&attachmentRef,										// const VkAttachmentReference*		pColorAttachments;
			m_multisampling ? &resolveAttachmentRef : DE_NULL,	// const VkAttachmentReference*		pResolveAttachments;
			DE_NULL,											// const VkAttachmentReference*		pDepthStencilAttachment;
			0u,													// deUint32							preserveAttachmentCount;
			DE_NULL,											// const VkAttachmentReference*		pPreserveAttachments;
		};

		const VkRenderPassCreateInfo			renderPassCreateInfo	=
		{
			VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,			// VkStructureType					sType;
			DE_NULL,											// const void*						pNext;
			0u,													// VkRenderPassCreateFlags			flags;
			m_multisampling ? 2u : 1u,							// deUint32							attachmentCount;
			attachmentDesc,										// const VkAttachmentDescription*	pAttachments;
			1u,													// deUint32							subpassCount;
			&subpassDesc,										// const VkSubpassDescription*		pSubpasses;
			0u,													// deUint32							dependencyCount;
			DE_NULL,											// const VkSubpassDependency*		pDependencies;
		};

		m_renderPass =  createRenderPass(vkd, vkDevice, &renderPassCreateInfo, DE_NULL);
	}

	// FrameBuffer
	{
		const VkImageView						attachments[]			=
		{
			*m_imageView,
			*m_resolvedImageView
		};

		const VkFramebufferCreateInfo			framebufferCreateInfo	=
		{
			VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,	// VkStructureType			sType;
			DE_NULL,									// const void*				pNext;
			0u,											// VkFramebufferCreateFlags	flags;
			*m_renderPass,								// VkRenderPass				renderPass;
			m_multisampling ? 2u : 1u,					// deUint32					attachmentCount;
			attachments,								// const VkImageView*		pAttachments;
			m_renderSize,								// deUint32					width;
			m_renderSize,								// deUint32					height;
			1u,											// deUint32					layers;
		};

		m_frameBuffer = createFramebuffer(vkd, vkDevice, &framebufferCreateInfo, DE_NULL);
	}

	// Uniform Buffer
	{
		const VkBufferCreateInfo				bufferCreateInfo		=
		{
			VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,		// VkStructureType		sType;
			DE_NULL,									// const void*			pNext;
			0u,											// VkBufferCreateFlags	flags;
			m_uniformBufferSize,						// VkDeviceSize			size;
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,			// VkBufferUsageFlags	usage;
			VK_SHARING_MODE_EXCLUSIVE,					// VkSharingMode		sharingMode;
			1u,											// deUint32				queueFamilyIndexCount;
			&queueFamilyIndex							// const deUint32*		pQueueFamilyIndices;
		};

		m_uniformBuffer			= createBuffer(vkd, vkDevice, &bufferCreateInfo);
		m_uniformBufferMemory	= allocator.allocate(getBufferMemoryRequirements(vkd, vkDevice, *m_uniformBuffer), MemoryRequirement::HostVisible);

		VK_CHECK(vkd.bindBufferMemory(vkDevice, *m_uniformBuffer, m_uniformBufferMemory->getMemory(), m_uniformBufferMemory->getOffset()));
	}

	// Descriptors
	{
		descriptorPoolBuilder.addType(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
		m_descriptorPool = descriptorPoolBuilder.build(vkd, vkDevice, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);

		descriptorSetLayoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_ALL);
		m_descriptorSetLayout = descriptorSetLayoutBuilder.build(vkd, vkDevice);

		const VkDescriptorSetAllocateInfo		descriptorSetParams		=
		{
			VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
			DE_NULL,
			*m_descriptorPool,
			1u,
			&m_descriptorSetLayout.get(),
		};

		m_descriptorSet = allocateDescriptorSet(vkd, vkDevice, &descriptorSetParams);

		const VkDescriptorBufferInfo			descriptorBufferInfo	=
		{
			*m_uniformBuffer,							// VkBuffer		buffer;
			0u,											// VkDeviceSize	offset;
			VK_WHOLE_SIZE								// VkDeviceSize	range;
		};

		const VkWriteDescriptorSet				writeDescritporSet		=
		{
			VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,		// VkStructureType					sType;
			DE_NULL,									// const void*						pNext;
			*m_descriptorSet,							// VkDescriptorSet					destSet;
			0,											// deUint32							destBinding;
			0,											// deUint32							destArrayElement;
			1u,											// deUint32							count;
			VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,			// VkDescriptorType					descriptorType;
			DE_NULL,									// const VkDescriptorImageInfo*		pImageInfo;
			&descriptorBufferInfo,						// const VkDescriptorBufferInfo*	pBufferInfo;
			DE_NULL										// const VkBufferView*				pTexelBufferView;
		};

		vkd.updateDescriptorSets(vkDevice, 1u, &writeDescritporSet, 0u, DE_NULL);
	}

	// Pipeline Layout
	{
		const VkPipelineLayoutCreateInfo		pipelineLayoutCreateInfo	=
		{
			VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,		// VkStructureType				sType;
			DE_NULL,											// const void*					pNext;
			0u,													// VkPipelineLayoutCreateFlags	flags;
			1u,													// deUint32						descriptorSetCount;
			&m_descriptorSetLayout.get(),						// const VkDescriptorSetLayout*	pSetLayouts;
			0u,													// deUint32						pushConstantRangeCount;
			DE_NULL												// const VkPushConstantRange*	pPushConstantRanges;
		};

		m_pipelineLayout = createPipelineLayout(vkd, vkDevice, &pipelineLayoutCreateInfo);
	}

	// Shaders
	{
		m_vertexShaderModule	= createShaderModule(vkd, vkDevice, m_context.getBinaryCollection().get("vertext_shader"), 0);
		m_fragmentShaderModule	= createShaderModule(vkd, vkDevice, m_context.getBinaryCollection().get("fragment_shader"), 0);
	}

	// Result Buffer
	{
		const VkBufferCreateInfo				bufferCreateInfo		=
		{
			VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,		// VkStructureType		sType;
			DE_NULL,									// const void*			pNext;
			0u,											// VkBufferCreateFlags	flags;
			m_resultBufferSize,							// VkDeviceSize			size;
			VK_BUFFER_USAGE_TRANSFER_DST_BIT,			// VkBufferUsageFlags	usage;
			VK_SHARING_MODE_EXCLUSIVE,					// VkSharingMode		sharingMode;
			1u,											// deUint32				queueFamilyIndexCount;
			&queueFamilyIndex							// const deUint32*		pQueueFamilyIndices;
		};

		m_resultBuffer			= createBuffer(vkd, vkDevice, &bufferCreateInfo);
		m_resultBufferMemory	= allocator.allocate(getBufferMemoryRequirements(vkd, vkDevice, *m_resultBuffer), MemoryRequirement::HostVisible);

		VK_CHECK(vkd.bindBufferMemory(vkDevice, *m_resultBuffer, m_resultBufferMemory->getMemory(), m_resultBufferMemory->getOffset()));
	}

	m_context.getTestContext().getLog() << tcu::TestLog::Message << "Sample count = " << getSampleCountFlagsStr(m_sampleCount) << tcu::TestLog::EndMessage;
	m_context.getTestContext().getLog() << tcu::TestLog::Message << "SUBPIXEL_BITS = " << m_subpixelBits << tcu::TestLog::EndMessage;
}

BaseRenderingTestInstance::~BaseRenderingTestInstance (void)
{
}


void BaseRenderingTestInstance::addImageTransitionBarrier(VkCommandBuffer commandBuffer, VkImage image, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask, VkImageLayout oldLayout, VkImageLayout newLayout) const
{

	const DeviceInterface&			vkd					= m_context.getDeviceInterface();

	const VkImageSubresourceRange	subResourcerange	=
	{
		VK_IMAGE_ASPECT_COLOR_BIT,		// VkImageAspectFlags	aspectMask;
		0,								// deUint32				baseMipLevel;
		1,								// deUint32				levelCount;
		0,								// deUint32				baseArrayLayer;
		1								// deUint32				layerCount;
	};

	const VkImageMemoryBarrier		imageBarrier		=
	{
		VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,		// VkStructureType			sType;
		DE_NULL,									// const void*				pNext;
		srcAccessMask,								// VkAccessFlags			srcAccessMask;
		dstAccessMask,								// VkAccessFlags			dstAccessMask;
		oldLayout,									// VkImageLayout			oldLayout;
		newLayout,									// VkImageLayout			newLayout;
		VK_QUEUE_FAMILY_IGNORED,					// deUint32					srcQueueFamilyIndex;
		VK_QUEUE_FAMILY_IGNORED,					// deUint32					destQueueFamilyIndex;
		image,										// VkImage					image;
		subResourcerange							// VkImageSubresourceRange	subresourceRange;
	};

	vkd.cmdPipelineBarrier(commandBuffer, srcStageMask, dstStageMask, 0, 0, DE_NULL, 0, DE_NULL, 1, &imageBarrier);
}

void BaseRenderingTestInstance::drawPrimitives (tcu::Surface& result, const std::vector<tcu::Vec4>& vertexData, VkPrimitiveTopology primitiveTopology)
{
	// default to color white
	const std::vector<tcu::Vec4> colorData(vertexData.size(), tcu::Vec4(1.0f, 1.0f, 1.0f, 1.0f));

	drawPrimitives(result, vertexData, colorData, primitiveTopology);
}

void BaseRenderingTestInstance::drawPrimitives (tcu::Surface& result, const std::vector<tcu::Vec4>& positionData, const std::vector<tcu::Vec4>& colorData, VkPrimitiveTopology primitiveTopology)
{
	drawPrimitives(result, positionData, colorData, primitiveTopology, *m_image, *m_resolvedImage, *m_frameBuffer, m_renderSize, *m_resultBuffer, *m_resultBufferMemory);
}
void BaseRenderingTestInstance::drawPrimitives (tcu::Surface& result, const std::vector<tcu::Vec4>& positionData, const std::vector<tcu::Vec4>& colorData, VkPrimitiveTopology primitiveTopology,
					VkImage image, VkImage resolvedImage, VkFramebuffer frameBuffer, const deUint32 renderSize, VkBuffer resultBuffer, const Allocation& resultBufferMemory)
{
	const DeviceInterface&						vkd						= m_context.getDeviceInterface();
	const VkDevice								vkDevice				= m_context.getDevice();
	const VkQueue								queue					= m_context.getUniversalQueue();
	const deUint32								queueFamilyIndex		= m_context.getUniversalQueueFamilyIndex();
	Allocator&									allocator				= m_context.getDefaultAllocator();
	const size_t								attributeBatchSize		= positionData.size() * sizeof(tcu::Vec4);

	Move<VkCommandBuffer>						commandBuffer;
	Move<VkPipeline>							graphicsPipeline;
	Move<VkBuffer>								vertexBuffer;
	de::MovePtr<Allocation>						vertexBufferMemory;
	const VkPhysicalDeviceProperties			properties				= m_context.getDeviceProperties();

	if (attributeBatchSize > properties.limits.maxVertexInputAttributeOffset)
	{
		std::stringstream message;
		message << "Larger vertex input attribute offset is needed (" << attributeBatchSize << ") than the available maximum (" << properties.limits.maxVertexInputAttributeOffset << ").";
		TCU_THROW(NotSupportedError, message.str().c_str());
	}

	// Create Graphics Pipeline
	{
		const VkVertexInputBindingDescription	vertexInputBindingDescription =
		{
			0u,								// deUint32					binding;
			sizeof(tcu::Vec4),				// deUint32					strideInBytes;
			VK_VERTEX_INPUT_RATE_VERTEX		// VkVertexInputStepRate	stepRate;
		};

		const VkVertexInputAttributeDescription	vertexInputAttributeDescriptions[2] =
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
				(deUint32)attributeBatchSize		// deUint32	offsetInBytes;
			}
		};

		const VkPipelineVertexInputStateCreateInfo	vertexInputStateParams =
		{
			VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,		// VkStructureType							sType;
			DE_NULL,														// const void*								pNext;
			0,																// VkPipelineVertexInputStateCreateFlags	flags;
			1u,																// deUint32									bindingCount;
			&vertexInputBindingDescription,									// const VkVertexInputBindingDescription*	pVertexBindingDescriptions;
			2u,																// deUint32									attributeCount;
			vertexInputAttributeDescriptions								// const VkVertexInputAttributeDescription*	pVertexAttributeDescriptions;
		};

		const std::vector<VkViewport>	viewports	(1, makeViewport(tcu::UVec2(renderSize)));
		const std::vector<VkRect2D>		scissors	(1, makeRect2D(tcu::UVec2(renderSize)));

		const VkPipelineMultisampleStateCreateInfo multisampleStateParams =
		{
			VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,		// VkStructureType							sType;
			DE_NULL,														// const void*								pNext;
			0u,																// VkPipelineMultisampleStateCreateFlags	flags;
			m_sampleCount,													// VkSampleCountFlagBits					rasterizationSamples;
			VK_FALSE,														// VkBool32									sampleShadingEnable;
			0.0f,															// float									minSampleShading;
			DE_NULL,														// const VkSampleMask*						pSampleMask;
			VK_FALSE,														// VkBool32									alphaToCoverageEnable;
			VK_FALSE														// VkBool32									alphaToOneEnable;
		};


		VkPipelineRasterizationStateCreateInfo rasterizationStateInfo = *getRasterizationStateCreateInfo();

		rasterizationStateInfo.pNext = getLineRasterizationStateCreateInfo();

		VkPipelineDynamicStateCreateInfo			dynamicStateCreateInfo =
		{
			VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,	// VkStructureType                      sType
			DE_NULL,												// const void*                          pNext
			0u,														// VkPipelineDynamicStateCreateFlags    flags
			0u,														// deUint32                             dynamicStateCount
			DE_NULL													// const VkDynamicState*                pDynamicStates
		};

		VkDynamicState dynamicState = VK_DYNAMIC_STATE_LINE_STIPPLE_EXT;
		if (getLineStippleDynamic())
		{
			dynamicStateCreateInfo.dynamicStateCount = 1;
			dynamicStateCreateInfo.pDynamicStates = &dynamicState;
		}

		graphicsPipeline = makeGraphicsPipeline(vkd,								// const DeviceInterface&                        vk
												vkDevice,							// const VkDevice                                device
												*m_pipelineLayout,					// const VkPipelineLayout                        pipelineLayout
												*m_vertexShaderModule,				// const VkShaderModule                          vertexShaderModule
												DE_NULL,							// const VkShaderModule                          tessellationControlShaderModule
												DE_NULL,							// const VkShaderModule                          tessellationEvalShaderModule
												DE_NULL,							// const VkShaderModule                          geometryShaderModule
												*m_fragmentShaderModule,			// const VkShaderModule                          fragmentShaderModule
												*m_renderPass,						// const VkRenderPass                            renderPass
												viewports,							// const std::vector<VkViewport>&                viewports
												scissors,							// const std::vector<VkRect2D>&                  scissors
												primitiveTopology,					// const VkPrimitiveTopology                     topology
												0u,									// const deUint32                                subpass
												0u,									// const deUint32                                patchControlPoints
												&vertexInputStateParams,			// const VkPipelineVertexInputStateCreateInfo*   vertexInputStateCreateInfo
												&rasterizationStateInfo,			// const VkPipelineRasterizationStateCreateInfo* rasterizationStateCreateInfo
												&multisampleStateParams,			// const VkPipelineMultisampleStateCreateInfo*   multisampleStateCreateInfo
												DE_NULL,							// const VkPipelineDepthStencilStateCreateInfo*  depthStencilStateCreateInfo,
												getColorBlendStateCreateInfo(),		// const VkPipelineColorBlendStateCreateInfo*    colorBlendStateCreateInfo,
												&dynamicStateCreateInfo);			// const VkPipelineDynamicStateCreateInfo*       dynamicStateCreateInfo
	}

	// Create Vertex Buffer
	{
		const VkBufferCreateInfo			vertexBufferParams		=
		{
			VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,		// VkStructureType		sType;
			DE_NULL,									// const void*			pNext;
			0u,											// VkBufferCreateFlags	flags;
			attributeBatchSize * 2,						// VkDeviceSize			size;
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,			// VkBufferUsageFlags	usage;
			VK_SHARING_MODE_EXCLUSIVE,					// VkSharingMode		sharingMode;
			1u,											// deUint32				queueFamilyCount;
			&queueFamilyIndex							// const deUint32*		pQueueFamilyIndices;
		};

		vertexBuffer		= createBuffer(vkd, vkDevice, &vertexBufferParams);
		vertexBufferMemory	= allocator.allocate(getBufferMemoryRequirements(vkd, vkDevice, *vertexBuffer), MemoryRequirement::HostVisible);

		VK_CHECK(vkd.bindBufferMemory(vkDevice, *vertexBuffer, vertexBufferMemory->getMemory(), vertexBufferMemory->getOffset()));

		// Load vertices into vertex buffer
		deMemcpy(vertexBufferMemory->getHostPtr(), positionData.data(), attributeBatchSize);
		deMemcpy(reinterpret_cast<deUint8*>(vertexBufferMemory->getHostPtr()) +  attributeBatchSize, colorData.data(), attributeBatchSize);
		flushAlloc(vkd, vkDevice, *vertexBufferMemory);
	}

	// Create Command Buffer
	commandBuffer = allocateCommandBuffer(vkd, vkDevice, *m_commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

	// Begin Command Buffer
	beginCommandBuffer(vkd, *commandBuffer);

	addImageTransitionBarrier(*commandBuffer, image,
							  VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,				// VkPipelineStageFlags		srcStageMask
							  VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,				// VkPipelineStageFlags		dstStageMask
							  0,												// VkAccessFlags			srcAccessMask
							  VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,				// VkAccessFlags			dstAccessMask
							  VK_IMAGE_LAYOUT_UNDEFINED,						// VkImageLayout			oldLayout;
							  VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);		// VkImageLayout			newLayout;

	if (m_multisampling) {
		addImageTransitionBarrier(*commandBuffer, resolvedImage,
								  VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,				// VkPipelineStageFlags		srcStageMask
								  VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,				// VkPipelineStageFlags		dstStageMask
								  0,												// VkAccessFlags			srcAccessMask
								  VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,				// VkAccessFlags			dstAccessMask
								  VK_IMAGE_LAYOUT_UNDEFINED,						// VkImageLayout			oldLayout;
								  VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);		// VkImageLayout			newLayout;
	}

	// Begin Render Pass
	beginRenderPass(vkd, *commandBuffer, *m_renderPass, frameBuffer, vk::makeRect2D(0, 0, renderSize, renderSize), tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f));

	const VkDeviceSize						vertexBufferOffset		= 0;

	vkd.cmdBindPipeline(*commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *graphicsPipeline);
	vkd.cmdBindDescriptorSets(*commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipelineLayout, 0u, 1, &m_descriptorSet.get(), 0u, DE_NULL);
	vkd.cmdBindVertexBuffers(*commandBuffer, 0, 1, &vertexBuffer.get(), &vertexBufferOffset);
	if (getLineStippleDynamic())
		vkd.cmdSetLineStippleEXT(*commandBuffer, lineStippleFactor, lineStipplePattern);
	vkd.cmdDraw(*commandBuffer, (deUint32)positionData.size(), 1, 0, 0);
	endRenderPass(vkd, *commandBuffer);

	// Copy Image
	copyImageToBuffer(vkd, *commandBuffer, m_multisampling ? resolvedImage : image, resultBuffer, tcu::IVec2(renderSize, renderSize));

	endCommandBuffer(vkd, *commandBuffer);

	// Set Point Size
	{
		float	pointSize	= getPointSize();
		deMemcpy(m_uniformBufferMemory->getHostPtr(), &pointSize, (size_t)m_uniformBufferSize);
		flushAlloc(vkd, vkDevice, *m_uniformBufferMemory);
	}

	// Submit
	submitCommandsAndWait(vkd, vkDevice, queue, commandBuffer.get());

	invalidateAlloc(vkd, vkDevice, resultBufferMemory);
	tcu::copy(result.getAccess(), tcu::ConstPixelBufferAccess(m_textureFormat, tcu::IVec3(renderSize, renderSize, 1), resultBufferMemory.getHostPtr()));
}

float BaseRenderingTestInstance::getLineWidth (void) const
{
	return 1.0f;
}

float BaseRenderingTestInstance::getPointSize (void) const
{
	return 1.0f;
}

const VkPipelineRasterizationStateCreateInfo* BaseRenderingTestInstance::getRasterizationStateCreateInfo (void) const
{
	static VkPipelineRasterizationStateCreateInfo	rasterizationStateCreateInfo	=
	{
		VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,		// VkStructureType							sType;
		DE_NULL,														// const void*								pNext;
		0,																// VkPipelineRasterizationStateCreateFlags	flags;
		false,															// VkBool32									depthClipEnable;
		false,															// VkBool32									rasterizerDiscardEnable;
		VK_POLYGON_MODE_FILL,											// VkFillMode								fillMode;
		VK_CULL_MODE_NONE,												// VkCullMode								cullMode;
		VK_FRONT_FACE_COUNTER_CLOCKWISE,								// VkFrontFace								frontFace;
		VK_FALSE,														// VkBool32									depthBiasEnable;
		0.0f,															// float									depthBias;
		0.0f,															// float									depthBiasClamp;
		0.0f,															// float									slopeScaledDepthBias;
		getLineWidth(),													// float									lineWidth;
	};

	rasterizationStateCreateInfo.lineWidth = getLineWidth();
	return &rasterizationStateCreateInfo;
}

VkPipelineRasterizationLineStateCreateInfoEXT* BaseRenderingTestInstance::getLineRasterizationStateCreateInfo (void)
{
	m_lineRasterizationStateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_LINE_STATE_CREATE_INFO_EXT,	// VkStructureType				sType;
		DE_NULL,																// const void*					pNext;
		VK_LINE_RASTERIZATION_MODE_DEFAULT_EXT,									// VkLineRasterizationModeEXT	lineRasterizationMode;
		VK_FALSE,																// VkBool32						stippledLineEnable;
		1,																		// uint32_t						lineStippleFactor;
		0xFFFF,																	// uint16_t						lineStipplePattern;
	};

	return &m_lineRasterizationStateInfo;
}

const VkPipelineColorBlendStateCreateInfo* BaseRenderingTestInstance::getColorBlendStateCreateInfo (void) const
{
	static const VkPipelineColorBlendAttachmentState	colorBlendAttachmentState	=
	{
		false,														// VkBool32			blendEnable;
		VK_BLEND_FACTOR_ONE,										// VkBlend			srcBlendColor;
		VK_BLEND_FACTOR_ZERO,										// VkBlend			destBlendColor;
		VK_BLEND_OP_ADD,											// VkBlendOp		blendOpColor;
		VK_BLEND_FACTOR_ONE,										// VkBlend			srcBlendAlpha;
		VK_BLEND_FACTOR_ZERO,										// VkBlend			destBlendAlpha;
		VK_BLEND_OP_ADD,											// VkBlendOp		blendOpAlpha;
		(VK_COLOR_COMPONENT_R_BIT |
		 VK_COLOR_COMPONENT_G_BIT |
		 VK_COLOR_COMPONENT_B_BIT |
		 VK_COLOR_COMPONENT_A_BIT)									// VkChannelFlags	channelWriteMask;
	};

	static const VkPipelineColorBlendStateCreateInfo	colorBlendStateParams		=
	{
		VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,	// VkStructureType								sType;
		DE_NULL,													// const void*									pNext;
		0,															// VkPipelineColorBlendStateCreateFlags			flags;
		false,														// VkBool32										logicOpEnable;
		VK_LOGIC_OP_COPY,											// VkLogicOp									logicOp;
		1u,															// deUint32										attachmentCount;
		&colorBlendAttachmentState,									// const VkPipelineColorBlendAttachmentState*	pAttachments;
		{ 0.0f, 0.0f, 0.0f, 0.0f },									// float										blendConst[4];
	};

	return &colorBlendStateParams;
}

const tcu::TextureFormat& BaseRenderingTestInstance::getTextureFormat (void) const
{
	return m_textureFormat;
}

class BaseTriangleTestInstance : public BaseRenderingTestInstance
{
public:
							BaseTriangleTestInstance	(Context& context, VkPrimitiveTopology primitiveTopology, VkSampleCountFlagBits sampleCount);
	virtual tcu::TestStatus	iterate						(void);

private:
	virtual void			generateTriangles			(int iteration, std::vector<tcu::Vec4>& outData, std::vector<TriangleSceneSpec::SceneTriangle>& outTriangles) = DE_NULL;

	int						m_iteration;
	const int				m_iterationCount;
	VkPrimitiveTopology		m_primitiveTopology;
	bool					m_allIterationsPassed;
};

BaseTriangleTestInstance::BaseTriangleTestInstance (Context& context, VkPrimitiveTopology primitiveTopology, VkSampleCountFlagBits sampleCount)
	: BaseRenderingTestInstance		(context, sampleCount)
	, m_iteration					(0)
	, m_iterationCount				(3)
	, m_primitiveTopology			(primitiveTopology)
	, m_allIterationsPassed			(true)
{
}

tcu::TestStatus BaseTriangleTestInstance::iterate (void)
{
	const std::string								iterationDescription	= "Test iteration " + de::toString(m_iteration+1) + " / " + de::toString(m_iterationCount);
	const tcu::ScopedLogSection						section					(m_context.getTestContext().getLog(), iterationDescription, iterationDescription);
	tcu::Surface									resultImage				(m_renderSize, m_renderSize);
	std::vector<tcu::Vec4>							drawBuffer;
	std::vector<TriangleSceneSpec::SceneTriangle>	triangles;

	generateTriangles(m_iteration, drawBuffer, triangles);

	// draw image
	drawPrimitives(resultImage, drawBuffer, m_primitiveTopology);

	// compare
	{
		bool					compareOk;
		RasterizationArguments	args;
		TriangleSceneSpec		scene;

		tcu::IVec4				colorBits = tcu::getTextureFormatBitDepth(getTextureFormat());

		args.numSamples		= m_multisampling ? 1 : 0;
		args.subpixelBits	= m_subpixelBits;
		args.redBits		= colorBits[0];
		args.greenBits		= colorBits[1];
		args.blueBits		= colorBits[2];

		scene.triangles.swap(triangles);

		compareOk = verifyTriangleGroupRasterization(resultImage, scene, args, m_context.getTestContext().getLog());

		if (!compareOk)
			m_allIterationsPassed = false;
	}

	// result
	if (++m_iteration == m_iterationCount)
	{
		if (m_allIterationsPassed)
			return tcu::TestStatus::pass("Pass");
		else
			return tcu::TestStatus::fail("Incorrect rasterization");
	}
	else
		return tcu::TestStatus::incomplete();
}

class BaseLineTestInstance : public BaseRenderingTestInstance
{
public:
								BaseLineTestInstance	(Context&					context,
														 VkPrimitiveTopology		primitiveTopology,
														 PrimitiveWideness			wideness,
														 PrimitiveStrictness		strictness,
														 VkSampleCountFlagBits		sampleCount,
														 LineStipple				stipple,
														 VkLineRasterizationModeEXT	lineRasterizationMode,
														 const deUint32				additionalRenderSize = 0);
	virtual tcu::TestStatus		iterate					(void);
	virtual float				getLineWidth			(void) const;
	bool						getLineStippleEnable	(void) const { return m_stipple != LINESTIPPLE_DISABLED; }
	virtual bool				getLineStippleDynamic	(void) const { return m_stipple == LINESTIPPLE_DYNAMIC; };

	virtual
	VkPipelineRasterizationLineStateCreateInfoEXT*		getLineRasterizationStateCreateInfo	(void);

private:
	virtual void				generateLines			(int iteration, std::vector<tcu::Vec4>& outData, std::vector<LineSceneSpec::SceneLine>& outLines) = DE_NULL;

	bool						resultHasAlpha			(tcu::Surface& result);

	int							m_iteration;
	const int					m_iterationCount;
	VkPrimitiveTopology			m_primitiveTopology;
	const PrimitiveWideness		m_primitiveWideness;
	const PrimitiveStrictness	m_primitiveStrictness;
	bool						m_allIterationsPassed;
	bool						m_qualityWarning;
	float						m_maxLineWidth;
	std::vector<float>			m_lineWidths;
	LineStipple					m_stipple;
	VkLineRasterizationModeEXT	m_lineRasterizationMode;
	Move<VkImage>				m_additionalImage;
	de::MovePtr<Allocation>		m_additionalImageMemory;
	Move<VkImageView>			m_additionalImageView;
	Move<VkImage>				m_additionalResolvedImage;
	de::MovePtr<Allocation>		m_additionalResolvedImageMemory;
	Move<VkImageView>			m_additionalResolvedImageView;
	Move<VkFramebuffer>			m_additionalFrameBuffer;
	Move<VkBuffer>				m_additionalResultBuffer;
	de::MovePtr<Allocation>		m_additionalResultBufferMemory;
};

BaseLineTestInstance::BaseLineTestInstance (Context&					context,
											VkPrimitiveTopology			primitiveTopology,
											PrimitiveWideness			wideness,
											PrimitiveStrictness			strictness,
											VkSampleCountFlagBits		sampleCount,
											LineStipple					stipple,
											VkLineRasterizationModeEXT	lineRasterizationMode,
											const deUint32 additionalRenderSize)
	: BaseRenderingTestInstance	(context, sampleCount, RESOLUTION_POT, VK_FORMAT_R8G8B8A8_UNORM, additionalRenderSize)
	, m_iteration				(0)
	, m_iterationCount			(3)
	, m_primitiveTopology		(primitiveTopology)
	, m_primitiveWideness		(wideness)
	, m_primitiveStrictness		(strictness)
	, m_allIterationsPassed		(true)
	, m_qualityWarning			(false)
	, m_maxLineWidth			(1.0f)
	, m_stipple					(stipple)
	, m_lineRasterizationMode	(lineRasterizationMode)
{
	DE_ASSERT(m_primitiveWideness < PRIMITIVEWIDENESS_LAST);

	if (m_lineRasterizationMode != VK_LINE_RASTERIZATION_MODE_EXT_LAST)
	{
		if (context.isDeviceFunctionalitySupported("VK_EXT_line_rasterization"))
		{
			VkPhysicalDeviceLineRasterizationPropertiesEXT lineRasterizationProperties =
			{
				VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_LINE_RASTERIZATION_PROPERTIES_EXT,	// VkStructureType	sType;
				DE_NULL,																// void*			pNext;
				0u,																		// deUint32			lineSubPixelPrecisionBits;
			};

			VkPhysicalDeviceProperties2 deviceProperties2;
			deviceProperties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
			deviceProperties2.pNext = &lineRasterizationProperties;

			context.getInstanceInterface().getPhysicalDeviceProperties2(m_context.getPhysicalDevice(), &deviceProperties2);

			m_subpixelBits = lineRasterizationProperties.lineSubPixelPrecisionBits;
		}
	}

	// create line widths
	if (m_primitiveWideness == PRIMITIVEWIDENESS_NARROW)
	{
		m_lineWidths.resize(m_iterationCount, 1.0f);
	}
	else if (m_primitiveWideness == PRIMITIVEWIDENESS_WIDE)
	{
		const float*	range = context.getDeviceProperties().limits.lineWidthRange;

		m_context.getTestContext().getLog() << tcu::TestLog::Message << "ALIASED_LINE_WIDTH_RANGE = [" << range[0] << ", " << range[1] << "]" << tcu::TestLog::EndMessage;

		DE_ASSERT(range[1] > 1.0f);

		// set hand picked sizes
		m_lineWidths.push_back(5.0f);
		m_lineWidths.push_back(10.0f);

		// Do not pick line width with 0.5 fractional value as rounding direction is not defined.
		if (deFloatFrac(range[1]) == 0.5f)
		{
			m_lineWidths.push_back(range[1] - context.getDeviceProperties().limits.lineWidthGranularity);
		}
		else
		{
			m_lineWidths.push_back(range[1]);
		}

		DE_ASSERT((int)m_lineWidths.size() == m_iterationCount);

		m_maxLineWidth = range[1];
	}
	else
		DE_ASSERT(false);

	// Create image, image view and frame buffer for testing at an additional resolution if required.
	if (m_additionalRenderSize != 0)
	{
		const DeviceInterface&						vkd						= m_context.getDeviceInterface();
		const VkDevice								vkDevice				= m_context.getDevice();
		const deUint32								queueFamilyIndex		= m_context.getUniversalQueueFamilyIndex();
		Allocator&									allocator				= m_context.getDefaultAllocator();
		DescriptorPoolBuilder						descriptorPoolBuilder;
		DescriptorSetLayoutBuilder					descriptorSetLayoutBuilder;
		{
			const VkImageUsageFlags	imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
			const VkImageCreateInfo					imageCreateInfo			=
			{
				VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,		// VkStructureType			sType;
				DE_NULL,									// const void*				pNext;
				0u,											// VkImageCreateFlags		flags;
				VK_IMAGE_TYPE_2D,							// VkImageType				imageType;
				m_imageFormat,								// VkFormat					format;
				{ m_additionalRenderSize, m_additionalRenderSize, 1u },			// VkExtent3D				extent;
				1u,											// deUint32					mipLevels;
				1u,											// deUint32					arrayLayers;
				m_sampleCount,								// VkSampleCountFlagBits	samples;
				VK_IMAGE_TILING_OPTIMAL,					// VkImageTiling			tiling;
				imageUsage,									// VkImageUsageFlags		usage;
				VK_SHARING_MODE_EXCLUSIVE,					// VkSharingMode			sharingMode;
				1u,											// deUint32					queueFamilyIndexCount;
				&queueFamilyIndex,							// const deUint32*			pQueueFamilyIndices;
				VK_IMAGE_LAYOUT_UNDEFINED					// VkImageLayout			initialLayout;
			};

			m_additionalImage = vk::createImage(vkd, vkDevice, &imageCreateInfo, DE_NULL);

			m_additionalImageMemory	= allocator.allocate(getImageMemoryRequirements(vkd, vkDevice, *m_additionalImage), MemoryRequirement::Any);
			VK_CHECK(vkd.bindImageMemory(vkDevice, *m_additionalImage, m_additionalImageMemory->getMemory(), m_additionalImageMemory->getOffset()));
		}

		// Image View
		{
			const VkImageViewCreateInfo				imageViewCreateInfo		=
			{
				VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,	// VkStructureType				sType;
				DE_NULL,									// const void*					pNext;
				0u,											// VkImageViewCreateFlags		flags;
				*m_additionalImage,							// VkImage						image;
				VK_IMAGE_VIEW_TYPE_2D,						// VkImageViewType				viewType;
				m_imageFormat,								// VkFormat						format;
				makeComponentMappingRGBA(),					// VkComponentMapping			components;
				{
					VK_IMAGE_ASPECT_COLOR_BIT,					// VkImageAspectFlags			aspectMask;
					0u,											// deUint32						baseMipLevel;
					1u,											// deUint32						mipLevels;
					0u,											// deUint32						baseArrayLayer;
					1u,											// deUint32						arraySize;
				},											// VkImageSubresourceRange		subresourceRange;
			};

			m_additionalImageView = vk::createImageView(vkd, vkDevice, &imageViewCreateInfo, DE_NULL);
		}

		if (m_multisampling)
		{
			{
				const VkImageUsageFlags	imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
				const VkImageCreateInfo					imageCreateInfo			=
				{
					VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,		// VkStructureType			sType;
					DE_NULL,									// const void*				pNext;
					0u,											// VkImageCreateFlags		flags;
					VK_IMAGE_TYPE_2D,							// VkImageType				imageType;
					m_imageFormat,								// VkFormat					format;
					{ m_additionalRenderSize,	m_additionalRenderSize, 1u },			// VkExtent3D				extent;
					1u,											// deUint32					mipLevels;
					1u,											// deUint32					arrayLayers;
					VK_SAMPLE_COUNT_1_BIT,						// VkSampleCountFlagBits	samples;
					VK_IMAGE_TILING_OPTIMAL,					// VkImageTiling			tiling;
					imageUsage,									// VkImageUsageFlags		usage;
					VK_SHARING_MODE_EXCLUSIVE,					// VkSharingMode			sharingMode;
					1u,											// deUint32					queueFamilyIndexCount;
					&queueFamilyIndex,							// const deUint32*			pQueueFamilyIndices;
					VK_IMAGE_LAYOUT_UNDEFINED					// VkImageLayout			initialLayout;
				};

				m_additionalResolvedImage			= vk::createImage(vkd, vkDevice, &imageCreateInfo, DE_NULL);
				m_additionalResolvedImageMemory	= allocator.allocate(getImageMemoryRequirements(vkd, vkDevice, *m_additionalResolvedImage), MemoryRequirement::Any);
				VK_CHECK(vkd.bindImageMemory(vkDevice, *m_additionalResolvedImage, m_additionalResolvedImageMemory->getMemory(), m_additionalResolvedImageMemory->getOffset()));
			}

			// Image view
			{
				const VkImageViewCreateInfo				imageViewCreateInfo		=
				{
					VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,	// VkStructureType				sType;
					DE_NULL,									// const void*					pNext;
					0u,											// VkImageViewCreateFlags		flags;
					*m_additionalResolvedImage,					// VkImage						image;
					VK_IMAGE_VIEW_TYPE_2D,						// VkImageViewType				viewType;
					m_imageFormat,								// VkFormat						format;
					makeComponentMappingRGBA(),					// VkComponentMapping			components;
					{
						VK_IMAGE_ASPECT_COLOR_BIT,					// VkImageAspectFlags			aspectMask;
						0u,											// deUint32						baseMipLevel;
						1u,											// deUint32						mipLevels;
						0u,											// deUint32						baseArrayLayer;
						1u,											// deUint32						arraySize;
					},											// VkImageSubresourceRange		subresourceRange;
				};
				m_additionalResolvedImageView = vk::createImageView(vkd, vkDevice, &imageViewCreateInfo, DE_NULL);
			}
		}

		{
			const VkImageView						attachments[]			=
			{
				*m_additionalImageView,
				*m_additionalResolvedImageView
			};

			const VkFramebufferCreateInfo			framebufferCreateInfo	=
			{
				VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,	// VkStructureType			sType;
				DE_NULL,									// const void*				pNext;
				0u,											// VkFramebufferCreateFlags	flags;
				*m_renderPass,								// VkRenderPass				renderPass;
				m_multisampling ? 2u : 1u,					// deUint32					attachmentCount;
				attachments,								// const VkImageView*		pAttachments;
				m_additionalRenderSize,						// deUint32					width;
				m_additionalRenderSize,						// deUint32					height;
				1u,											// deUint32					layers;
			};
			m_additionalFrameBuffer = createFramebuffer(vkd, vkDevice, &framebufferCreateInfo, DE_NULL);
		}

		// Framebuffer
		{
			const VkBufferCreateInfo				bufferCreateInfo		=
			{
				VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,		// VkStructureType		sType;
				DE_NULL,									// const void*			pNext;
				0u,											// VkBufferCreateFlags	flags;
				m_additionalResultBufferSize,							// VkDeviceSize			size;
				VK_BUFFER_USAGE_TRANSFER_DST_BIT,			// VkBufferUsageFlags	usage;
				VK_SHARING_MODE_EXCLUSIVE,					// VkSharingMode		sharingMode;
				1u,											// deUint32				queueFamilyIndexCount;
				&queueFamilyIndex							// const deUint32*		pQueueFamilyIndices;
			};

			m_additionalResultBuffer			= createBuffer(vkd, vkDevice, &bufferCreateInfo);
			m_additionalResultBufferMemory	= allocator.allocate(getBufferMemoryRequirements(vkd, vkDevice, *m_additionalResultBuffer), MemoryRequirement::HostVisible);

			VK_CHECK(vkd.bindBufferMemory(vkDevice, *m_additionalResultBuffer, m_additionalResultBufferMemory->getMemory(), m_additionalResultBufferMemory->getOffset()));
		}
	}
}

bool BaseLineTestInstance::resultHasAlpha(tcu::Surface& resultImage)
{
	bool hasAlpha = false;
	for (int y = 0; y < resultImage.getHeight() && !hasAlpha; ++y)
	for (int x = 0; x < resultImage.getWidth(); ++x)
	{
		const tcu::RGBA		color				= resultImage.getPixel(x, y);
		if (color.getAlpha() > 0 && color.getAlpha() < 0xFF)
		{
			hasAlpha = true;
			break;
		}
	}
	return hasAlpha;
}

tcu::TestStatus BaseLineTestInstance::iterate (void)
{
	const std::string						iterationDescription	= "Test iteration " + de::toString(m_iteration+1) + " / " + de::toString(m_iterationCount);
	const tcu::ScopedLogSection				section					(m_context.getTestContext().getLog(), iterationDescription, iterationDescription);
	const float								lineWidth				= getLineWidth();
	tcu::Surface							resultImage				(m_renderSize, m_renderSize);
	tcu::Surface							additionalResultImage	(m_additionalRenderSize, m_additionalRenderSize);
	std::vector<tcu::Vec4>					drawBuffer;
	std::vector<LineSceneSpec::SceneLine>	lines;

	// supported?
	if (lineWidth <= m_maxLineWidth)
	{
		// gen data
		generateLines(m_iteration, drawBuffer, lines);

		// draw image
		drawPrimitives(resultImage, drawBuffer, m_primitiveTopology);

		// compare
		RasterizationArguments	args;
		LineSceneSpec			scene;
		tcu::IVec4				colorBits	= tcu::getTextureFormatBitDepth(getTextureFormat());
		bool					strict		= m_primitiveStrictness == PRIMITIVESTRICTNESS_STRICT;

		args.numSamples		= m_multisampling ? 1 : 0;
		args.subpixelBits	= m_subpixelBits;
		args.redBits		= colorBits[0];
		args.greenBits		= colorBits[1];
		args.blueBits		= colorBits[2];

		scene.lines.swap(lines);
		scene.lineWidth = lineWidth;
		scene.stippleEnable = getLineStippleEnable();
		scene.stippleFactor = getLineStippleEnable() ? lineStippleFactor : 1;
		scene.stipplePattern = getLineStippleEnable() ? lineStipplePattern : 0xFFFF;
		scene.isStrip = m_primitiveTopology == VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
		scene.isSmooth = m_lineRasterizationMode == VK_LINE_RASTERIZATION_MODE_RECTANGULAR_SMOOTH_EXT;

		// Choose verification mode. Smooth lines assume mostly over-rasterization (bloated lines with a falloff).
		// Stippled lines lose some precision across segments in a strip, so need a weaker threshold than normal
		// lines. For simple cases, check for an exact match (STRICT).
		if (scene.isSmooth)
			scene.verificationMode = tcu::VERIFICATIONMODE_SMOOTH;
		else if (scene.stippleEnable)
			scene.verificationMode = tcu::VERIFICATIONMODE_WEAKER;
		else
			scene.verificationMode = tcu::VERIFICATIONMODE_STRICT;

		if (m_lineRasterizationMode == VK_LINE_RASTERIZATION_MODE_BRESENHAM_EXT)
		{
			// bresenham is "no AA" in GL, so set numSamples to zero.
			args.numSamples = 0;
			if (!verifyLineGroupRasterization(resultImage, scene, args, m_context.getTestContext().getLog()))
				m_allIterationsPassed = false;
		}
		else
		{
			if (scene.isSmooth)
			{
				// Smooth lines get the fractional coverage multiplied into the alpha component,
				// so do a sanity check to validate that there is at least one pixel in the image
				// with a fractional opacity.
				bool hasAlpha = resultHasAlpha(resultImage);
				if (!hasAlpha)
				{
					m_context.getTestContext().getLog() << tcu::TestLog::Message << "Missing alpha transparency (failed)." << tcu::TestLog::EndMessage;
					m_allIterationsPassed = false;
				}
			}

			if (!verifyRelaxedLineGroupRasterization(resultImage, scene, args, m_context.getTestContext().getLog(), (0 == m_multisampling), strict))
			{
				// Retry with weaker verification. If it passes, consider it a quality warning.
				scene.verificationMode = tcu::VERIFICATIONMODE_WEAKER;
				if (!verifyRelaxedLineGroupRasterization(resultImage, scene, args, m_context.getTestContext().getLog(), false, strict))
					m_allIterationsPassed = false;
				else
					m_qualityWarning = true;
			}

			if (m_additionalRenderSize != 0)
			{
				const std::vector<tcu::Vec4> colorData(drawBuffer.size(), tcu::Vec4(1.0f, 1.0f, 1.0f, 1.0f));

				if (scene.isSmooth)
					scene.verificationMode = tcu::VERIFICATIONMODE_SMOOTH;
				else if (scene.stippleEnable)
					scene.verificationMode = tcu::VERIFICATIONMODE_WEAKER;
				else
					scene.verificationMode = tcu::VERIFICATIONMODE_STRICT;

				drawPrimitives(additionalResultImage, drawBuffer, colorData, m_primitiveTopology, *m_additionalImage, *m_additionalResolvedImage, *m_additionalFrameBuffer, m_additionalRenderSize, *m_additionalResultBuffer, *m_additionalResultBufferMemory);

				// Compare
				if (!verifyRelaxedLineGroupRasterization(additionalResultImage, scene, args, m_context.getTestContext().getLog(), (0 == m_multisampling), strict))
				{
					if (strict)
					{
						m_allIterationsPassed = false;
					}
					else
					{
						// Retry with weaker verification. If it passes, consider it a quality warning.
						scene.verificationMode = tcu::VERIFICATIONMODE_WEAKER;
						if (!verifyRelaxedLineGroupRasterization(resultImage, scene, args, m_context.getTestContext().getLog(), (0 == m_multisampling), strict))
							m_allIterationsPassed = false;
						else
							m_qualityWarning = true;
					}
				}
			}
		}
	}
	else
		m_context.getTestContext().getLog() << tcu::TestLog::Message << "Line width " << lineWidth << " not supported, skipping iteration." << tcu::TestLog::EndMessage;

	// result
	if (++m_iteration == m_iterationCount)
	{
		if (!m_allIterationsPassed)
			return tcu::TestStatus::fail("Incorrect rasterization");
		else if (m_qualityWarning)
			return tcu::TestStatus(QP_TEST_RESULT_QUALITY_WARNING, "Low-quality line rasterization");
		else
			return tcu::TestStatus::pass("Pass");
	}
	else
		return tcu::TestStatus::incomplete();
}


float BaseLineTestInstance::getLineWidth (void) const
{
	return m_lineWidths[m_iteration];
}

VkPipelineRasterizationLineStateCreateInfoEXT* BaseLineTestInstance::getLineRasterizationStateCreateInfo (void)
{
	if (m_lineRasterizationMode == VK_LINE_RASTERIZATION_MODE_EXT_LAST)
		return DE_NULL;

	m_lineRasterizationStateInfo	=
	{
		VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_LINE_STATE_CREATE_INFO_EXT,	// VkStructureType				sType;
		DE_NULL,																// const void*					pNext;
		m_lineRasterizationMode,												// VkLineRasterizationModeEXT	lineRasterizationMode;
		getLineStippleEnable() ? VK_TRUE : VK_FALSE,							// VkBool32						stippledLineEnable;
		1,																		// uint32_t						lineStippleFactor;
		0xFFFF,																	// uint16_t						lineStipplePattern;
	};

	if (m_stipple == LINESTIPPLE_STATIC)
	{
		m_lineRasterizationStateInfo.lineStippleFactor = lineStippleFactor;
		m_lineRasterizationStateInfo.lineStipplePattern = lineStipplePattern;
	}

	return &m_lineRasterizationStateInfo;
}

class PointTestInstance : public BaseRenderingTestInstance
{
public:
							PointTestInstance		(Context&					context,
													 PrimitiveWideness			wideness,
													 PrimitiveStrictness		strictness,				// ignored
													 VkSampleCountFlagBits		sampleCount,
													 LineStipple				stipple,				// ignored
													 VkLineRasterizationModeEXT	lineRasterizationMode,	// ignored
													 deUint32					renderSize);			// ignored
	virtual tcu::TestStatus	iterate					(void);
	virtual float			getPointSize			(void) const;

private:
	virtual void			generatePoints			(int iteration, std::vector<tcu::Vec4>& outData, std::vector<PointSceneSpec::ScenePoint>& outPoints);

	int						m_iteration;
	const int				m_iterationCount;
	const PrimitiveWideness	m_primitiveWideness;
	bool					m_allIterationsPassed;
	float					m_maxPointSize;
	std::vector<float>		m_pointSizes;
};

PointTestInstance::PointTestInstance (Context&						context,
									  PrimitiveWideness				wideness,
									  PrimitiveStrictness			strictness,
									  VkSampleCountFlagBits			sampleCount,
									  LineStipple					stipple,
									  VkLineRasterizationModeEXT	lineRasterizationMode,
									  deUint32						renderSize)
	: BaseRenderingTestInstance	(context, sampleCount)
	, m_iteration				(0)
	, m_iterationCount			(3)
	, m_primitiveWideness		(wideness)
	, m_allIterationsPassed		(true)
	, m_maxPointSize			(1.0f)
{
	DE_UNREF(strictness);
	DE_UNREF(stipple);
	DE_UNREF(lineRasterizationMode);
	DE_UNREF(renderSize);

	// create point sizes
	if (m_primitiveWideness == PRIMITIVEWIDENESS_NARROW)
	{
		m_pointSizes.resize(m_iterationCount, 1.0f);
	}
	else if (m_primitiveWideness == PRIMITIVEWIDENESS_WIDE)
	{
		const float*	range = context.getDeviceProperties().limits.pointSizeRange;

		m_context.getTestContext().getLog() << tcu::TestLog::Message << "GL_ALIASED_POINT_SIZE_RANGE = [" << range[0] << ", " << range[1] << "]" << tcu::TestLog::EndMessage;

		DE_ASSERT(range[1] > 1.0f);

		// set hand picked sizes
		m_pointSizes.push_back(10.0f);
		m_pointSizes.push_back(25.0f);
		m_pointSizes.push_back(range[1]);
		DE_ASSERT((int)m_pointSizes.size() == m_iterationCount);

		m_maxPointSize = range[1];
	}
	else
		DE_ASSERT(false);
}

tcu::TestStatus PointTestInstance::iterate (void)
{
	const std::string						iterationDescription	= "Test iteration " + de::toString(m_iteration+1) + " / " + de::toString(m_iterationCount);
	const tcu::ScopedLogSection				section					(m_context.getTestContext().getLog(), iterationDescription, iterationDescription);
	const float								pointSize				= getPointSize();
	tcu::Surface							resultImage				(m_renderSize, m_renderSize);
	std::vector<tcu::Vec4>					drawBuffer;
	std::vector<PointSceneSpec::ScenePoint>	points;

	// supported?
	if (pointSize <= m_maxPointSize)
	{
		// gen data
		generatePoints(m_iteration, drawBuffer, points);

		// draw image
		drawPrimitives(resultImage, drawBuffer, VK_PRIMITIVE_TOPOLOGY_POINT_LIST);

		// compare
		{
			bool					compareOk;
			RasterizationArguments	args;
			PointSceneSpec			scene;

			tcu::IVec4				colorBits = tcu::getTextureFormatBitDepth(getTextureFormat());

			args.numSamples		= m_multisampling ? 1 : 0;
			args.subpixelBits	= m_subpixelBits;
			args.redBits		= colorBits[0];
			args.greenBits		= colorBits[1];
			args.blueBits		= colorBits[2];

			scene.points.swap(points);

			compareOk = verifyPointGroupRasterization(resultImage, scene, args, m_context.getTestContext().getLog());

			if (!compareOk)
				m_allIterationsPassed = false;
		}
	}
	else
		m_context.getTestContext().getLog() << tcu::TestLog::Message << "Point size " << pointSize << " not supported, skipping iteration." << tcu::TestLog::EndMessage;

	// result
	if (++m_iteration == m_iterationCount)
	{
		if (m_allIterationsPassed)
			return tcu::TestStatus::pass("Pass");
		else
			return tcu::TestStatus::fail("Incorrect rasterization");
	}
	else
		return tcu::TestStatus::incomplete();
}

float PointTestInstance::getPointSize (void) const
{
	return m_pointSizes[m_iteration];
}

void PointTestInstance::generatePoints (int iteration, std::vector<tcu::Vec4>& outData, std::vector<PointSceneSpec::ScenePoint>& outPoints)
{
	outData.resize(6);

	switch (iteration)
	{
		case 0:
			// \note: these values are chosen arbitrarily
			outData[0] = tcu::Vec4( 0.2f,  0.8f, 0.0f, 1.0f);
			outData[1] = tcu::Vec4( 0.5f,  0.2f, 0.0f, 1.0f);
			outData[2] = tcu::Vec4( 0.5f,  0.3f, 0.0f, 1.0f);
			outData[3] = tcu::Vec4(-0.5f,  0.2f, 0.0f, 1.0f);
			outData[4] = tcu::Vec4(-0.2f, -0.4f, 0.0f, 1.0f);
			outData[5] = tcu::Vec4(-0.4f,  0.2f, 0.0f, 1.0f);
			break;

		case 1:
			outData[0] = tcu::Vec4(-0.499f, 0.128f, 0.0f, 1.0f);
			outData[1] = tcu::Vec4(-0.501f,  -0.3f, 0.0f, 1.0f);
			outData[2] = tcu::Vec4(  0.11f,  -0.2f, 0.0f, 1.0f);
			outData[3] = tcu::Vec4(  0.11f,   0.2f, 0.0f, 1.0f);
			outData[4] = tcu::Vec4(  0.88f,   0.9f, 0.0f, 1.0f);
			outData[5] = tcu::Vec4(   0.4f,   1.2f, 0.0f, 1.0f);
			break;

		case 2:
			outData[0] = tcu::Vec4( -0.9f, -0.3f, 0.0f, 1.0f);
			outData[1] = tcu::Vec4(  0.3f, -0.9f, 0.0f, 1.0f);
			outData[2] = tcu::Vec4( -0.4f, -0.1f, 0.0f, 1.0f);
			outData[3] = tcu::Vec4(-0.11f,  0.2f, 0.0f, 1.0f);
			outData[4] = tcu::Vec4( 0.88f,  0.7f, 0.0f, 1.0f);
			outData[5] = tcu::Vec4( -0.4f,  0.4f, 0.0f, 1.0f);
			break;
	}

	outPoints.resize(outData.size());
	for (int pointNdx = 0; pointNdx < (int)outPoints.size(); ++pointNdx)
	{
		outPoints[pointNdx].position = outData[pointNdx];
		outPoints[pointNdx].pointSize = getPointSize();
	}

	// log
	m_context.getTestContext().getLog() << tcu::TestLog::Message << "Rendering " << outPoints.size() << " point(s): (point size = " << getPointSize() << ")" << tcu::TestLog::EndMessage;
	for (int pointNdx = 0; pointNdx < (int)outPoints.size(); ++pointNdx)
		m_context.getTestContext().getLog() << tcu::TestLog::Message << "Point " << (pointNdx+1) << ":\t" << outPoints[pointNdx].position << tcu::TestLog::EndMessage;
}

template <typename ConcreteTestInstance>
class PointSizeTestCase : public BaseRenderingTestCase
{
public:
							PointSizeTestCase	(tcu::TestContext&		context,
												 std::string&			name,
												 std::string&			description,
												 deUint32				renderSize,
												 float					pointSize,
												 VkSampleCountFlagBits	sampleCount = VK_SAMPLE_COUNT_1_BIT)
								: BaseRenderingTestCase (context, name, description, sampleCount)
								, m_pointSize	(pointSize)
								, m_renderSize	(renderSize)
							{}

	virtual TestInstance*	createInstance		(Context& context) const
							{
								VkPhysicalDeviceProperties	properties	(context.getDeviceProperties());

								if (m_renderSize > properties.limits.maxViewportDimensions[0] || m_renderSize > properties.limits.maxViewportDimensions[1])
									TCU_THROW(NotSupportedError , "Viewport dimensions not supported");

								if (m_renderSize > properties.limits.maxFramebufferWidth || m_renderSize > properties.limits.maxFramebufferHeight)
									TCU_THROW(NotSupportedError , "Framebuffer width/height not supported");

								return new ConcreteTestInstance(context, m_renderSize, m_pointSize);
							}

	virtual	void			checkSupport		(Context& context) const
							{
								context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_LARGE_POINTS);
							}
protected:
	const float				m_pointSize;
	const deUint32			m_renderSize;
};

class PointSizeTestInstance : public BaseRenderingTestInstance
{
public:
							PointSizeTestInstance	(Context& context, deUint32 renderSize, float pointSize);
	virtual tcu::TestStatus	iterate					(void);
	virtual float			getPointSize			(void) const;

private:
	void					generatePointData		(PointSceneSpec::ScenePoint& outPoint);
	void					drawPoint				(tcu::PixelBufferAccess& result, tcu::PointSceneSpec::ScenePoint& point);
	bool					verifyPoint				(tcu::TestLog& log, tcu::PixelBufferAccess& access, float pointSize);
	bool					isPointSizeClamped		(float pointSize, float maxPointSizeLimit);

	const float				m_pointSize;
	const float				m_maxPointSize;
	const deUint32			m_renderSize;
	const VkFormat			m_format;
};

PointSizeTestInstance::PointSizeTestInstance (Context& context, deUint32 renderSize, float pointSize)
	: BaseRenderingTestInstance	(context, vk::VK_SAMPLE_COUNT_1_BIT, renderSize, VK_FORMAT_R8_UNORM)
	, m_pointSize				(pointSize)
	, m_maxPointSize			(context.getDeviceProperties().limits.pointSizeRange[1])
	, m_renderSize				(renderSize)
	, m_format					(VK_FORMAT_R8_UNORM) // Use single-channel format to minimize memory allocation when using large render targets
{
}

tcu::TestStatus PointSizeTestInstance::iterate (void)
{
	tcu::TextureLevel			resultBuffer	(mapVkFormat(m_format), m_renderSize, m_renderSize);
	tcu::PixelBufferAccess		access			(resultBuffer.getAccess());
	PointSceneSpec::ScenePoint	point;

	// Generate data
	generatePointData(point);

	// Draw
	drawPoint(access, point);

	// Compare
	{
		// pointSize must either be specified pointSize or clamped to device limit pointSizeRange[1]
		const float	pointSize	(deFloatMin(m_pointSize, m_maxPointSize));
		const bool	compareOk	(verifyPoint(m_context.getTestContext().getLog(), access, pointSize));

		// Result
		if (compareOk)
			return isPointSizeClamped(pointSize, m_maxPointSize) ? tcu::TestStatus::pass("Pass, pointSize clamped to pointSizeRange[1]") : tcu::TestStatus::pass("Pass");
		else
			return tcu::TestStatus::fail("Incorrect rasterization");
	}
}

float PointSizeTestInstance::getPointSize (void) const
{
	return m_pointSize;
}

void PointSizeTestInstance::generatePointData (PointSceneSpec::ScenePoint& outPoint)
{
	const tcu::PointSceneSpec::ScenePoint point =
	{
		tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f),	// position
		tcu::Vec4(1.0f, 0.0f, 0.0f, 0.0f),	// color
		m_pointSize							// pointSize
	};

	outPoint = point;

	// log
	{
		tcu::TestLog& log = m_context.getTestContext().getLog();

		log << tcu::TestLog::Message << "Point position: "	<< de::toString(point.position)		<< tcu::TestLog::EndMessage;
		log << tcu::TestLog::Message << "Point color: "		<< de::toString(point.color)		<< tcu::TestLog::EndMessage;
		log << tcu::TestLog::Message << "Point size: "		<< de::toString(point.pointSize)	<< tcu::TestLog::EndMessage;
		log << tcu::TestLog::Message << "Render size: "		<< de::toString(m_renderSize)		<< tcu::TestLog::EndMessage;
		log << tcu::TestLog::Message << "Format: "			<< de::toString(m_format)			<< tcu::TestLog::EndMessage;
	}
}

void PointSizeTestInstance::drawPoint (tcu::PixelBufferAccess& result, PointSceneSpec::ScenePoint& point)
{
	const tcu::Vec4			positionData		(point.position);
	const tcu::Vec4			colorData			(point.color);

	const DeviceInterface&	vkd					(m_context.getDeviceInterface());
	const VkDevice			vkDevice			(m_context.getDevice());
	const VkQueue			queue				(m_context.getUniversalQueue());
	const deUint32			queueFamilyIndex	(m_context.getUniversalQueueFamilyIndex());
	const size_t			attributeBatchSize	(sizeof(tcu::Vec4));
	Allocator&				allocator			(m_context.getDefaultAllocator());

	Move<VkCommandBuffer>	commandBuffer;
	Move<VkPipeline>		graphicsPipeline;
	Move<VkBuffer>			vertexBuffer;
	de::MovePtr<Allocation>	vertexBufferMemory;

	// Create Graphics Pipeline
	{
		const std::vector<VkViewport>				viewports							(1, makeViewport(tcu::UVec2(m_renderSize)));
		const std::vector<VkRect2D>					scissors							(1, makeRect2D(tcu::UVec2(m_renderSize)));

		const VkVertexInputBindingDescription		vertexInputBindingDescription		=
		{
			0u,									// deUint32					binding;
			(deUint32)(2 * sizeof(tcu::Vec4)),	// deUint32					strideInBytes;
			VK_VERTEX_INPUT_RATE_VERTEX			// VkVertexInputStepRate	stepRate;
		};

		const VkVertexInputAttributeDescription		vertexInputAttributeDescriptions[2]	=
		{
			{
				0u,								// deUint32	location;
				0u,								// deUint32	binding;
				VK_FORMAT_R32G32B32A32_SFLOAT,	// VkFormat	format;
				0u								// deUint32	offsetInBytes;
			},
			{
				1u,								// deUint32	location;
				0u,								// deUint32	binding;
				VK_FORMAT_R32G32B32A32_SFLOAT,	// VkFormat	format;
				(deUint32)sizeof(tcu::Vec4)		// deUint32	offsetInBytes;
			}
		};

		const VkPipelineVertexInputStateCreateInfo	vertexInputStateParams				=
		{
			VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,	// VkStructureType							sType;
			DE_NULL,													// const void*								pNext;
			0,															// VkPipelineVertexInputStateCreateFlags	flags;
			1u,															// deUint32									bindingCount;
			&vertexInputBindingDescription,								// const VkVertexInputBindingDescription*	pVertexBindingDescriptions;
			2u,															// deUint32									attributeCount;
			vertexInputAttributeDescriptions							// const VkVertexInputAttributeDescription*	pVertexAttributeDescriptions;
		};

		graphicsPipeline = makeGraphicsPipeline(vkd,								// const DeviceInterface&							 vk
												vkDevice,							// const VkDevice									 device
												*m_pipelineLayout,					// const VkPipelineLayout							 pipelineLayout
												*m_vertexShaderModule,				// const VkShaderModule								 vertexShaderModule
												DE_NULL,							// const VkShaderModule								 tessellationControlShaderModule
												DE_NULL,							// const VkShaderModule								 tessellationEvalShaderModule
												DE_NULL,							// const VkShaderModule								 geometryShaderModule
												*m_fragmentShaderModule,			// const VkShaderModule								 fragmentShaderModule
												*m_renderPass,						// const VkRenderPass								 renderPass
												viewports,							// const std::vector<VkViewport>&					 viewports
												scissors,							// const std::vector<VkRect2D>&						 scissors
												VK_PRIMITIVE_TOPOLOGY_POINT_LIST,	// const VkPrimitiveTopology						 topology
												0u,									// const deUint32									 subpass
												0u,									// const deUint32									 patchControlPoints
												&vertexInputStateParams,			// const VkPipelineVertexInputStateCreateInfo*		 vertexInputStateCreateInfo
												getRasterizationStateCreateInfo(),	// const VkPipelineRasterizationStateCreateInfo*	 rasterizationStateCreateInfo
												DE_NULL,							// const VkPipelineMultisampleStateCreateInfo*		 multisampleStateCreateInfo
												DE_NULL,							// const VkPipelineDepthStencilStateCreateInfo*		 depthStencilStateCreateInfo,
												getColorBlendStateCreateInfo());	// const VkPipelineColorBlendStateCreateInfo*		 colorBlendStateCreateInfo
	}

	// Create Vertex Buffer
	{
		const VkBufferCreateInfo	vertexBufferParams =
		{
			VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,	// VkStructureType		sType;
			DE_NULL,								// const void*			pNext;
			0u,										// VkBufferCreateFlags	flags;
			attributeBatchSize * 2,					// VkDeviceSize			size;
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,		// VkBufferUsageFlags	usage;
			VK_SHARING_MODE_EXCLUSIVE,				// VkSharingMode		sharingMode;
			1u,										// deUint32				queueFamilyCount;
			&queueFamilyIndex						// const deUint32*		pQueueFamilyIndices;
		};

		vertexBuffer		= createBuffer(vkd, vkDevice, &vertexBufferParams);
		vertexBufferMemory	= allocator.allocate(getBufferMemoryRequirements(vkd, vkDevice, *vertexBuffer), MemoryRequirement::HostVisible);

		VK_CHECK(vkd.bindBufferMemory(vkDevice, *vertexBuffer, vertexBufferMemory->getMemory(), vertexBufferMemory->getOffset()));

		// Load vertices into vertex buffer
		deMemcpy(vertexBufferMemory->getHostPtr(), &positionData, attributeBatchSize);
		deMemcpy(reinterpret_cast<deUint8*>(vertexBufferMemory->getHostPtr()) +  attributeBatchSize, &colorData, attributeBatchSize);
		flushAlloc(vkd, vkDevice, *vertexBufferMemory);
	}

	// Create Command Buffer
	commandBuffer = allocateCommandBuffer(vkd, vkDevice, *m_commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

	// Begin Command Buffer
	beginCommandBuffer(vkd, *commandBuffer);

	addImageTransitionBarrier(*commandBuffer, *m_image,
							  VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,			// VkPipelineStageFlags		srcStageMask
							  VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,			// VkPipelineStageFlags		dstStageMask
							  0,											// VkAccessFlags			srcAccessMask
							  VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,			// VkAccessFlags			dstAccessMask
							  VK_IMAGE_LAYOUT_UNDEFINED,					// VkImageLayout			oldLayout;
							  VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);	// VkImageLayout			newLayout;

	// Begin Render Pass
	beginRenderPass(vkd, *commandBuffer, *m_renderPass, *m_frameBuffer, vk::makeRect2D(0, 0, m_renderSize, m_renderSize), tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f));

	const VkDeviceSize vertexBufferOffset = 0;

	vkd.cmdBindPipeline(*commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *graphicsPipeline);
	vkd.cmdBindDescriptorSets(*commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipelineLayout, 0u, 1, &m_descriptorSet.get(), 0u, DE_NULL);
	vkd.cmdBindVertexBuffers(*commandBuffer, 0, 1, &vertexBuffer.get(), &vertexBufferOffset);
	vkd.cmdDraw(*commandBuffer, 1, 1, 0, 0);
	endRenderPass(vkd, *commandBuffer);

	// Copy Image
	copyImageToBuffer(vkd, *commandBuffer, *m_image, *m_resultBuffer, tcu::IVec2(m_renderSize, m_renderSize));

	endCommandBuffer(vkd, *commandBuffer);

	// Set Point Size
	{
		float pointSize = getPointSize();

		deMemcpy(m_uniformBufferMemory->getHostPtr(), &pointSize, (size_t)m_uniformBufferSize);
		flushAlloc(vkd, vkDevice, *m_uniformBufferMemory);
	}

	// Submit
	submitCommandsAndWait(vkd, vkDevice, queue, commandBuffer.get());

	invalidateAlloc(vkd, vkDevice, *m_resultBufferMemory);
	tcu::copy(result, tcu::ConstPixelBufferAccess(m_textureFormat, tcu::IVec3(m_renderSize, m_renderSize, 1), m_resultBufferMemory->getHostPtr()));
}

bool PointSizeTestInstance::verifyPoint (tcu::TestLog& log, tcu::PixelBufferAccess& image, float pointSize)
{
	const float	expectedPointColor				(1.0f);
	const float	expectedBackgroundColor			(0.0f);
	deUint32	pointWidth						(0u);
	deUint32	pointHeight						(0u);
	bool		incorrectlyColoredPixelsFound	(false);
	bool		isOk							(true);

	// Verify rasterized point width and color
	for (size_t x = 0; x < (deUint32)image.getWidth(); x++)
	{
		float pixelColor = image.getPixel((deUint32)x, image.getHeight() / 2).x();

		if (pixelColor == expectedPointColor)
			pointWidth++;

		if ((pixelColor != expectedPointColor) && (pixelColor != expectedBackgroundColor))
			incorrectlyColoredPixelsFound = true;
	}

	// Verify rasterized point height and color
	for (size_t y = 0; y < (deUint32)image.getHeight(); y++)
	{
		float pixelColor = image.getPixel((deUint32)y, image.getWidth() / 2).x();

		if (pixelColor == expectedPointColor)
			pointHeight++;

		if ((pixelColor != expectedPointColor) && (pixelColor != expectedBackgroundColor))
			incorrectlyColoredPixelsFound = true;
	}

	// Compare amount of rasterized point pixels to expected pointSize.
	if ((pointWidth != (deUint32)deRoundFloatToInt32(pointSize)) || (pointHeight != (deUint32)deRoundFloatToInt32(pointSize)))
	{
		log << tcu::TestLog::Message << "Incorrect point size. Expected pointSize: " << de::toString(pointSize)
			<< ". Rasterized point width: " << pointWidth << " pixels, height: "
			<< pointHeight << " pixels." << tcu::TestLog::EndMessage;

		isOk = false;
	}

	// Check incorrectly colored pixels
	if (incorrectlyColoredPixelsFound)
	{
		log << tcu::TestLog::Message << "Incorrectly colored pixels found." << tcu::TestLog::EndMessage;
		isOk = false;
	}

	return isOk;
}

bool PointSizeTestInstance::isPointSizeClamped (float pointSize, float maxPointSizeLimit)
{
	return (pointSize == maxPointSizeLimit);
}

template <typename ConcreteTestInstance>
class BaseTestCase : public BaseRenderingTestCase
{
public:
							BaseTestCase	(tcu::TestContext& context, const std::string& name, const std::string& description, VkSampleCountFlagBits sampleCount = VK_SAMPLE_COUNT_1_BIT)
								: BaseRenderingTestCase(context, name, description, sampleCount)
							{}

	virtual TestInstance*	createInstance	(Context& context) const
							{
								return new ConcreteTestInstance(context, m_sampleCount);
							}
};

class TrianglesTestInstance : public BaseTriangleTestInstance
{
public:
							TrianglesTestInstance	(Context& context, VkSampleCountFlagBits sampleCount)
								: BaseTriangleTestInstance(context, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, sampleCount)
							{}

	void					generateTriangles		(int iteration, std::vector<tcu::Vec4>& outData, std::vector<TriangleSceneSpec::SceneTriangle>& outTriangles);
};

void TrianglesTestInstance::generateTriangles (int iteration, std::vector<tcu::Vec4>& outData, std::vector<TriangleSceneSpec::SceneTriangle>& outTriangles)
{
	outData.resize(6);

	switch (iteration)
	{
		case 0:
			// \note: these values are chosen arbitrarily
			outData[0] = tcu::Vec4( 0.2f,  0.8f, 0.0f, 1.0f);
			outData[1] = tcu::Vec4( 0.5f,  0.2f, 0.0f, 1.0f);
			outData[2] = tcu::Vec4( 0.5f,  0.3f, 0.0f, 1.0f);
			outData[3] = tcu::Vec4(-0.5f,  0.2f, 0.0f, 1.0f);
			outData[4] = tcu::Vec4(-1.5f, -0.4f, 0.0f, 1.0f);
			outData[5] = tcu::Vec4(-0.4f,  0.2f, 0.0f, 1.0f);
			break;

		case 1:
			outData[0] = tcu::Vec4(-0.499f, 0.128f, 0.0f, 1.0f);
			outData[1] = tcu::Vec4(-0.501f,  -0.3f, 0.0f, 1.0f);
			outData[2] = tcu::Vec4(  0.11f,  -0.2f, 0.0f, 1.0f);
			outData[3] = tcu::Vec4(  0.11f,   0.2f, 0.0f, 1.0f);
			outData[4] = tcu::Vec4(  0.88f,   0.9f, 0.0f, 1.0f);
			outData[5] = tcu::Vec4(   0.4f,   1.2f, 0.0f, 1.0f);
			break;

		case 2:
			outData[0] = tcu::Vec4( -0.9f, -0.3f, 0.0f, 1.0f);
			outData[1] = tcu::Vec4(  1.1f, -0.9f, 0.0f, 1.0f);
			outData[2] = tcu::Vec4( -1.1f, -0.1f, 0.0f, 1.0f);
			outData[3] = tcu::Vec4(-0.11f,  0.2f, 0.0f, 1.0f);
			outData[4] = tcu::Vec4( 0.88f,  0.7f, 0.0f, 1.0f);
			outData[5] = tcu::Vec4( -0.4f,  0.4f, 0.0f, 1.0f);
			break;
	}

	outTriangles.resize(2);
	outTriangles[0].positions[0] = outData[0];	outTriangles[0].sharedEdge[0] = false;
	outTriangles[0].positions[1] = outData[1];	outTriangles[0].sharedEdge[1] = false;
	outTriangles[0].positions[2] = outData[2];	outTriangles[0].sharedEdge[2] = false;

	outTriangles[1].positions[0] = outData[3];	outTriangles[1].sharedEdge[0] = false;
	outTriangles[1].positions[1] = outData[4];	outTriangles[1].sharedEdge[1] = false;
	outTriangles[1].positions[2] = outData[5];	outTriangles[1].sharedEdge[2] = false;

	// log
	m_context.getTestContext().getLog() << tcu::TestLog::Message << "Rendering " << outTriangles.size() << " triangle(s):" << tcu::TestLog::EndMessage;
	for (int triangleNdx = 0; triangleNdx < (int)outTriangles.size(); ++triangleNdx)
	{
		m_context.getTestContext().getLog()
			<< tcu::TestLog::Message
			<< "Triangle " << (triangleNdx+1) << ":"
			<< "\n\t" << outTriangles[triangleNdx].positions[0]
			<< "\n\t" << outTriangles[triangleNdx].positions[1]
			<< "\n\t" << outTriangles[triangleNdx].positions[2]
			<< tcu::TestLog::EndMessage;
	}
}

class TriangleStripTestInstance : public BaseTriangleTestInstance
{
public:
				TriangleStripTestInstance		(Context& context, VkSampleCountFlagBits sampleCount)
					: BaseTriangleTestInstance(context, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, sampleCount)
				{}

	void		generateTriangles				(int iteration, std::vector<tcu::Vec4>& outData, std::vector<TriangleSceneSpec::SceneTriangle>& outTriangles);
};

void TriangleStripTestInstance::generateTriangles (int iteration, std::vector<tcu::Vec4>& outData, std::vector<TriangleSceneSpec::SceneTriangle>& outTriangles)
{
	outData.resize(5);

	switch (iteration)
	{
		case 0:
			// \note: these values are chosen arbitrarily
			outData[0] = tcu::Vec4(-0.504f,  0.8f,   0.0f, 1.0f);
			outData[1] = tcu::Vec4(-0.2f,   -0.2f,   0.0f, 1.0f);
			outData[2] = tcu::Vec4(-0.2f,    0.199f, 0.0f, 1.0f);
			outData[3] = tcu::Vec4( 0.5f,    0.201f, 0.0f, 1.0f);
			outData[4] = tcu::Vec4( 1.5f,    0.4f,   0.0f, 1.0f);
			break;

		case 1:
			outData[0] = tcu::Vec4(-0.499f, 0.129f,  0.0f, 1.0f);
			outData[1] = tcu::Vec4(-0.501f,  -0.3f,  0.0f, 1.0f);
			outData[2] = tcu::Vec4(  0.11f,  -0.2f,  0.0f, 1.0f);
			outData[3] = tcu::Vec4(  0.11f,  -0.31f, 0.0f, 1.0f);
			outData[4] = tcu::Vec4(  0.88f,   0.9f,  0.0f, 1.0f);
			break;

		case 2:
			outData[0] = tcu::Vec4( -0.9f, -0.3f,  0.0f, 1.0f);
			outData[1] = tcu::Vec4(  1.1f, -0.9f,  0.0f, 1.0f);
			outData[2] = tcu::Vec4(-0.87f, -0.1f,  0.0f, 1.0f);
			outData[3] = tcu::Vec4(-0.11f,  0.19f, 0.0f, 1.0f);
			outData[4] = tcu::Vec4( 0.88f,  0.7f,  0.0f, 1.0f);
			break;
	}

	outTriangles.resize(3);
	outTriangles[0].positions[0] = outData[0];	outTriangles[0].sharedEdge[0] = false;
	outTriangles[0].positions[1] = outData[1];	outTriangles[0].sharedEdge[1] = true;
	outTriangles[0].positions[2] = outData[2];	outTriangles[0].sharedEdge[2] = false;

	outTriangles[1].positions[0] = outData[2];	outTriangles[1].sharedEdge[0] = true;
	outTriangles[1].positions[1] = outData[1];	outTriangles[1].sharedEdge[1] = false;
	outTriangles[1].positions[2] = outData[3];	outTriangles[1].sharedEdge[2] = true;

	outTriangles[2].positions[0] = outData[2];	outTriangles[2].sharedEdge[0] = true;
	outTriangles[2].positions[1] = outData[3];	outTriangles[2].sharedEdge[1] = false;
	outTriangles[2].positions[2] = outData[4];	outTriangles[2].sharedEdge[2] = false;

	// log
	m_context.getTestContext().getLog() << tcu::TestLog::Message << "Rendering triangle strip, " << outData.size() << " vertices." << tcu::TestLog::EndMessage;
	for (int vtxNdx = 0; vtxNdx < (int)outData.size(); ++vtxNdx)
	{
		m_context.getTestContext().getLog()
			<< tcu::TestLog::Message
			<< "\t" << outData[vtxNdx]
			<< tcu::TestLog::EndMessage;
	}
}

class TriangleFanTestInstance : public BaseTriangleTestInstance
{
public:
				TriangleFanTestInstance			(Context& context, VkSampleCountFlagBits sampleCount)
					: BaseTriangleTestInstance(context, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN, sampleCount)
				{}

	void		generateTriangles				(int iteration, std::vector<tcu::Vec4>& outData, std::vector<TriangleSceneSpec::SceneTriangle>& outTriangles);
};

void TriangleFanTestInstance::generateTriangles (int iteration, std::vector<tcu::Vec4>& outData, std::vector<TriangleSceneSpec::SceneTriangle>& outTriangles)
{
	outData.resize(5);

	switch (iteration)
	{
		case 0:
			// \note: these values are chosen arbitrarily
			outData[0] = tcu::Vec4( 0.01f,  0.0f, 0.0f, 1.0f);
			outData[1] = tcu::Vec4( 0.5f,   0.2f, 0.0f, 1.0f);
			outData[2] = tcu::Vec4( 0.46f,  0.3f, 0.0f, 1.0f);
			outData[3] = tcu::Vec4(-0.5f,   0.2f, 0.0f, 1.0f);
			outData[4] = tcu::Vec4(-1.5f,  -0.4f, 0.0f, 1.0f);
			break;

		case 1:
			outData[0] = tcu::Vec4(-0.499f, 0.128f, 0.0f, 1.0f);
			outData[1] = tcu::Vec4(-0.501f,  -0.3f, 0.0f, 1.0f);
			outData[2] = tcu::Vec4(  0.11f,  -0.2f, 0.0f, 1.0f);
			outData[3] = tcu::Vec4(  0.11f,   0.2f, 0.0f, 1.0f);
			outData[4] = tcu::Vec4(  0.88f,   0.9f, 0.0f, 1.0f);
			break;

		case 2:
			outData[0] = tcu::Vec4( -0.9f, -0.3f, 0.0f, 1.0f);
			outData[1] = tcu::Vec4(  1.1f, -0.9f, 0.0f, 1.0f);
			outData[2] = tcu::Vec4(  0.7f, -0.1f, 0.0f, 1.0f);
			outData[3] = tcu::Vec4( 0.11f,  0.2f, 0.0f, 1.0f);
			outData[4] = tcu::Vec4( 0.88f,  0.7f, 0.0f, 1.0f);
			break;
	}

	outTriangles.resize(3);
	outTriangles[0].positions[0] = outData[0];	outTriangles[0].sharedEdge[0] = false;
	outTriangles[0].positions[1] = outData[1];	outTriangles[0].sharedEdge[1] = false;
	outTriangles[0].positions[2] = outData[2];	outTriangles[0].sharedEdge[2] = true;

	outTriangles[1].positions[0] = outData[0];	outTriangles[1].sharedEdge[0] = true;
	outTriangles[1].positions[1] = outData[2];	outTriangles[1].sharedEdge[1] = false;
	outTriangles[1].positions[2] = outData[3];	outTriangles[1].sharedEdge[2] = true;

	outTriangles[2].positions[0] = outData[0];	outTriangles[2].sharedEdge[0] = true;
	outTriangles[2].positions[1] = outData[3];	outTriangles[2].sharedEdge[1] = false;
	outTriangles[2].positions[2] = outData[4];	outTriangles[2].sharedEdge[2] = false;

	// log
	m_context.getTestContext().getLog() << tcu::TestLog::Message << "Rendering triangle fan, " << outData.size() << " vertices." << tcu::TestLog::EndMessage;
	for (int vtxNdx = 0; vtxNdx < (int)outData.size(); ++vtxNdx)
	{
		m_context.getTestContext().getLog()
			<< tcu::TestLog::Message
			<< "\t" << outData[vtxNdx]
			<< tcu::TestLog::EndMessage;
	}
}

template <typename ConcreteTestInstance>
class WidenessTestCase : public BaseRenderingTestCase
{
public:
								WidenessTestCase	(tcu::TestContext&			context,
													 const std::string&			name,
													 const std::string&			description,
													 PrimitiveWideness			wideness,
													 PrimitiveStrictness		strictness,
													 bool						isLineTest,
													 VkSampleCountFlagBits		sampleCount,
													 LineStipple				stipple,
													 VkLineRasterizationModeEXT	lineRasterizationMode,
													 deUint32					additionalRenderSize	= 0)
									: BaseRenderingTestCase		(context, name, description, sampleCount)
									, m_wideness(wideness)
									, m_strictness				(strictness)
									, m_isLineTest				(isLineTest)
									, m_stipple					(stipple)
									, m_lineRasterizationMode	(lineRasterizationMode)
									, m_additionalRenderSize	(additionalRenderSize)
								{}

	virtual TestInstance*		createInstance		(Context& context) const
								{
									return new ConcreteTestInstance(context, m_wideness, m_strictness, m_sampleCount, m_stipple, m_lineRasterizationMode, m_additionalRenderSize);
								}

	virtual	void				checkSupport		(Context& context) const
								{
									if (m_isLineTest)
									{
										if (m_wideness == PRIMITIVEWIDENESS_WIDE)
											context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_WIDE_LINES);

										switch (m_lineRasterizationMode)
										{
											default:
												TCU_THROW(InternalError, "Unknown line rasterization mode");

											case VK_LINE_RASTERIZATION_MODE_EXT_LAST:
											{
												if (m_strictness == PRIMITIVESTRICTNESS_STRICT)
													if (!context.getDeviceProperties().limits.strictLines)
														TCU_THROW(NotSupportedError, "Strict rasterization is not supported");

												if (m_strictness == PRIMITIVESTRICTNESS_NONSTRICT)
													if (context.getDeviceProperties().limits.strictLines)
														TCU_THROW(NotSupportedError, "Nonstrict rasterization is not supported");

												break;
											}

											case VK_LINE_RASTERIZATION_MODE_DEFAULT_EXT:
											{
												if (!context.getDeviceProperties().limits.strictLines)
													TCU_THROW(NotSupportedError, "Strict rasterization is not supported");

												if (getLineStippleEnable() &&
													!context.getLineRasterizationFeaturesEXT().stippledRectangularLines)
													TCU_THROW(NotSupportedError, "Stippled rectangular lines not supported");
												break;
											}

											case VK_LINE_RASTERIZATION_MODE_RECTANGULAR_EXT:
											{
												if (!context.getLineRasterizationFeaturesEXT().rectangularLines)
													TCU_THROW(NotSupportedError, "Rectangular lines not supported");

												if (getLineStippleEnable() &&
													!context.getLineRasterizationFeaturesEXT().stippledRectangularLines)
													TCU_THROW(NotSupportedError, "Stippled rectangular lines not supported");
												break;
											}

											case VK_LINE_RASTERIZATION_MODE_BRESENHAM_EXT:
											{
												if (!context.getLineRasterizationFeaturesEXT().bresenhamLines)
													TCU_THROW(NotSupportedError, "Bresenham lines not supported");

												if (getLineStippleEnable() &&
													!context.getLineRasterizationFeaturesEXT().stippledBresenhamLines)
													TCU_THROW(NotSupportedError, "Stippled Bresenham lines not supported");
												break;
											}

											case VK_LINE_RASTERIZATION_MODE_RECTANGULAR_SMOOTH_EXT:
											{
												if (!context.getLineRasterizationFeaturesEXT().smoothLines)
													TCU_THROW(NotSupportedError, "Smooth lines not supported");

												if (getLineStippleEnable() &&
													!context.getLineRasterizationFeaturesEXT().stippledSmoothLines)
													TCU_THROW(NotSupportedError, "Stippled smooth lines not supported");
												break;
											}
										}
									}
									else
									{
										if (m_wideness == PRIMITIVEWIDENESS_WIDE)
											context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_LARGE_POINTS);
									}
								}

	bool					getLineStippleEnable	(void) const { return m_stipple != LINESTIPPLE_DISABLED; }
	virtual bool			getLineStippleDynamic	(void) const { return m_stipple == LINESTIPPLE_DYNAMIC; };

protected:
	const PrimitiveWideness				m_wideness;
	const PrimitiveStrictness			m_strictness;
	const bool							m_isLineTest;
	const LineStipple					m_stipple;
	const VkLineRasterizationModeEXT	m_lineRasterizationMode;
	const deUint32						m_additionalRenderSize;
};

class LinesTestInstance : public BaseLineTestInstance
{
public:
								LinesTestInstance	(Context& context, PrimitiveWideness wideness, PrimitiveStrictness strictness, VkSampleCountFlagBits sampleCount, LineStipple stipple, VkLineRasterizationModeEXT lineRasterizationMode, deUint32 additionalRenderSize = 0)
									: BaseLineTestInstance(context, VK_PRIMITIVE_TOPOLOGY_LINE_LIST, wideness, strictness, sampleCount, stipple, lineRasterizationMode, additionalRenderSize)
								{}

	virtual void				generateLines		(int iteration, std::vector<tcu::Vec4>& outData, std::vector<LineSceneSpec::SceneLine>& outLines);
};

void LinesTestInstance::generateLines (int iteration, std::vector<tcu::Vec4>& outData, std::vector<LineSceneSpec::SceneLine>& outLines)
{
	outData.resize(8);

	switch (iteration)
	{
		case 0:
			// \note: these values are chosen arbitrarily
			outData[0] = tcu::Vec4( 0.01f,  0.0f, 0.0f, 1.0f);
			outData[1] = tcu::Vec4(  0.5f,  0.2f, 0.0f, 1.0f);
			outData[2] = tcu::Vec4( 0.46f,  0.3f, 0.0f, 1.0f);
			outData[3] = tcu::Vec4( -0.3f,  0.2f, 0.0f, 1.0f);
			outData[4] = tcu::Vec4( -1.5f, -0.4f, 0.0f, 1.0f);
			outData[5] = tcu::Vec4(  0.1f,  0.5f, 0.0f, 1.0f);
			outData[6] = tcu::Vec4( 0.75f, -0.4f, 0.0f, 1.0f);
			outData[7] = tcu::Vec4(  0.3f,  0.8f, 0.0f, 1.0f);
			break;

		case 1:
			outData[0] = tcu::Vec4(-0.499f, 0.128f, 0.0f, 1.0f);
			outData[1] = tcu::Vec4(-0.501f,  -0.3f, 0.0f, 1.0f);
			outData[2] = tcu::Vec4(  0.11f,  -0.2f, 0.0f, 1.0f);
			outData[3] = tcu::Vec4(  0.11f,   0.2f, 0.0f, 1.0f);
			outData[4] = tcu::Vec4(  0.88f,   0.9f, 0.0f, 1.0f);
			outData[5] = tcu::Vec4(  0.18f,  -0.2f, 0.0f, 1.0f);
			outData[6] = tcu::Vec4(   0.0f,   1.0f, 0.0f, 1.0f);
			outData[7] = tcu::Vec4(   0.0f,  -1.0f, 0.0f, 1.0f);
			break;

		case 2:
			outData[0] = tcu::Vec4( -0.9f, -0.3f, 0.0f, 1.0f);
			outData[1] = tcu::Vec4(  1.1f, -0.9f, 0.0f, 1.0f);
			outData[2] = tcu::Vec4(  0.7f, -0.1f, 0.0f, 1.0f);
			outData[3] = tcu::Vec4( 0.11f,  0.2f, 0.0f, 1.0f);
			outData[4] = tcu::Vec4( 0.88f,  0.7f, 0.0f, 1.0f);
			outData[5] = tcu::Vec4(  0.8f, -0.7f, 0.0f, 1.0f);
			outData[6] = tcu::Vec4(  0.9f,  0.7f, 0.0f, 1.0f);
			outData[7] = tcu::Vec4( -0.9f,  0.7f, 0.0f, 1.0f);
			break;
	}

	outLines.resize(4);
	outLines[0].positions[0] = outData[0];
	outLines[0].positions[1] = outData[1];
	outLines[1].positions[0] = outData[2];
	outLines[1].positions[1] = outData[3];
	outLines[2].positions[0] = outData[4];
	outLines[2].positions[1] = outData[5];
	outLines[3].positions[0] = outData[6];
	outLines[3].positions[1] = outData[7];

	// log
	m_context.getTestContext().getLog() << tcu::TestLog::Message << "Rendering " << outLines.size() << " lines(s): (width = " << getLineWidth() << ")" << tcu::TestLog::EndMessage;
	for (int lineNdx = 0; lineNdx < (int)outLines.size(); ++lineNdx)
	{
		m_context.getTestContext().getLog()
			<< tcu::TestLog::Message
			<< "Line " << (lineNdx+1) << ":"
			<< "\n\t" << outLines[lineNdx].positions[0]
			<< "\n\t" << outLines[lineNdx].positions[1]
			<< tcu::TestLog::EndMessage;
	}
}

class LineStripTestInstance : public BaseLineTestInstance
{
public:
					LineStripTestInstance	(Context& context, PrimitiveWideness wideness, PrimitiveStrictness strictness, VkSampleCountFlagBits sampleCount, LineStipple stipple, VkLineRasterizationModeEXT lineRasterizationMode, deUint32)
						: BaseLineTestInstance(context, VK_PRIMITIVE_TOPOLOGY_LINE_STRIP, wideness, strictness, sampleCount, stipple, lineRasterizationMode)
					{}

	virtual void	generateLines			(int iteration, std::vector<tcu::Vec4>& outData, std::vector<LineSceneSpec::SceneLine>& outLines);
};

void LineStripTestInstance::generateLines (int iteration, std::vector<tcu::Vec4>& outData, std::vector<LineSceneSpec::SceneLine>& outLines)
{
	outData.resize(4);

	switch (iteration)
	{
		case 0:
			// \note: these values are chosen arbitrarily
			outData[0] = tcu::Vec4( 0.01f,  0.0f, 0.0f, 1.0f);
			outData[1] = tcu::Vec4( 0.5f,   0.2f, 0.0f, 1.0f);
			outData[2] = tcu::Vec4( 0.46f,  0.3f, 0.0f, 1.0f);
			outData[3] = tcu::Vec4(-0.5f,   0.2f, 0.0f, 1.0f);
			break;

		case 1:
			outData[0] = tcu::Vec4(-0.499f, 0.128f, 0.0f, 1.0f);
			outData[1] = tcu::Vec4(-0.501f,  -0.3f, 0.0f, 1.0f);
			outData[2] = tcu::Vec4(  0.11f,  -0.2f, 0.0f, 1.0f);
			outData[3] = tcu::Vec4(  0.11f,   0.2f, 0.0f, 1.0f);
			break;

		case 2:
			outData[0] = tcu::Vec4( -0.9f, -0.3f, 0.0f, 1.0f);
			outData[1] = tcu::Vec4(  0.9f, -0.9f, 0.0f, 1.0f);
			outData[2] = tcu::Vec4(  0.7f, -0.1f, 0.0f, 1.0f);
			outData[3] = tcu::Vec4( 0.11f,  0.2f, 0.0f, 1.0f);
			break;
	}

	outLines.resize(3);
	outLines[0].positions[0] = outData[0];
	outLines[0].positions[1] = outData[1];
	outLines[1].positions[0] = outData[1];
	outLines[1].positions[1] = outData[2];
	outLines[2].positions[0] = outData[2];
	outLines[2].positions[1] = outData[3];

	// log
	m_context.getTestContext().getLog() << tcu::TestLog::Message << "Rendering line strip, width = " << getLineWidth() << ", " << outData.size() << " vertices." << tcu::TestLog::EndMessage;
	for (int vtxNdx = 0; vtxNdx < (int)outData.size(); ++vtxNdx)
	{
		m_context.getTestContext().getLog()
			<< tcu::TestLog::Message
			<< "\t" << outData[vtxNdx]
			<< tcu::TestLog::EndMessage;
	}
}

class FillRuleTestInstance : public BaseRenderingTestInstance
{
public:
	enum FillRuleCaseType
	{
		FILLRULECASE_BASIC = 0,
		FILLRULECASE_REVERSED,
		FILLRULECASE_CLIPPED_FULL,
		FILLRULECASE_CLIPPED_PARTIAL,
		FILLRULECASE_PROJECTED,

		FILLRULECASE_LAST
	};
														FillRuleTestInstance			(Context& context, FillRuleCaseType type, VkSampleCountFlagBits sampleCount);
	virtual tcu::TestStatus								iterate							(void);

private:

	virtual const VkPipelineColorBlendStateCreateInfo*	getColorBlendStateCreateInfo	(void) const;
	int													getRenderSize					(FillRuleCaseType type) const;
	int													getNumIterations				(FillRuleCaseType type) const;
	void												generateTriangles				(int iteration, std::vector<tcu::Vec4>& outData) const;

	const FillRuleCaseType								m_caseType;
	int													m_iteration;
	const int											m_iterationCount;
	bool												m_allIterationsPassed;

};

FillRuleTestInstance::FillRuleTestInstance (Context& context, FillRuleCaseType type, VkSampleCountFlagBits sampleCount)
	: BaseRenderingTestInstance		(context, sampleCount, getRenderSize(type))
	, m_caseType					(type)
	, m_iteration					(0)
	, m_iterationCount				(getNumIterations(type))
	, m_allIterationsPassed			(true)
{
	DE_ASSERT(type < FILLRULECASE_LAST);
}

tcu::TestStatus FillRuleTestInstance::iterate (void)
{
	const std::string						iterationDescription	= "Test iteration " + de::toString(m_iteration+1) + " / " + de::toString(m_iterationCount);
	const tcu::ScopedLogSection				section					(m_context.getTestContext().getLog(), iterationDescription, iterationDescription);
	tcu::IVec4								colorBits				= tcu::getTextureFormatBitDepth(getTextureFormat());
	const int								thresholdRed			= 1 << (8 - colorBits[0]);
	const int								thresholdGreen			= 1 << (8 - colorBits[1]);
	const int								thresholdBlue			= 1 << (8 - colorBits[2]);
	tcu::Surface							resultImage				(m_renderSize, m_renderSize);
	std::vector<tcu::Vec4>					drawBuffer;

	generateTriangles(m_iteration, drawBuffer);

	// draw image
	{
		const std::vector<tcu::Vec4>	colorBuffer		(drawBuffer.size(), tcu::Vec4(0.5f, 0.5f, 0.5f, 1.0f));

		m_context.getTestContext().getLog() << tcu::TestLog::Message << "Drawing gray triangles with shared edges.\nEnabling additive blending to detect overlapping fragments." << tcu::TestLog::EndMessage;

		drawPrimitives(resultImage, drawBuffer, colorBuffer, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
	}

	// verify no overdraw
	{
		const tcu::RGBA	triangleColor	= tcu::RGBA(127, 127, 127, 255);
		bool			overdraw		= false;

		m_context.getTestContext().getLog() << tcu::TestLog::Message << "Verifying result." << tcu::TestLog::EndMessage;

		for (int y = 0; y < resultImage.getHeight(); ++y)
		for (int x = 0; x < resultImage.getWidth();  ++x)
		{
			const tcu::RGBA color = resultImage.getPixel(x, y);

			// color values are greater than triangle color? Allow lower values for multisampled edges and background.
			if ((color.getRed()   - triangleColor.getRed())   > thresholdRed   ||
				(color.getGreen() - triangleColor.getGreen()) > thresholdGreen ||
				(color.getBlue()  - triangleColor.getBlue())  > thresholdBlue)
				overdraw = true;
		}

		// results
		if (!overdraw)
			m_context.getTestContext().getLog() << tcu::TestLog::Message << "No overlapping fragments detected." << tcu::TestLog::EndMessage;
		else
		{
			m_context.getTestContext().getLog()	<< tcu::TestLog::Message << "Overlapping fragments detected, image is not valid." << tcu::TestLog::EndMessage;
			m_allIterationsPassed = false;
		}
	}

	// verify no missing fragments in the full viewport case
	if (m_caseType == FILLRULECASE_CLIPPED_FULL)
	{
		bool missingFragments = false;

		m_context.getTestContext().getLog() << tcu::TestLog::Message << "Searching missing fragments." << tcu::TestLog::EndMessage;

		for (int y = 0; y < resultImage.getHeight(); ++y)
		for (int x = 0; x < resultImage.getWidth();  ++x)
		{
			const tcu::RGBA color = resultImage.getPixel(x, y);

			// black? (background)
			if (color.getRed()   <= thresholdRed   ||
				color.getGreen() <= thresholdGreen ||
				color.getBlue()  <= thresholdBlue)
				missingFragments = true;
		}

		// results
		if (!missingFragments)
			m_context.getTestContext().getLog() << tcu::TestLog::Message << "No missing fragments detected." << tcu::TestLog::EndMessage;
		else
		{
			m_context.getTestContext().getLog()	<< tcu::TestLog::Message << "Missing fragments detected, image is not valid." << tcu::TestLog::EndMessage;

			m_allIterationsPassed = false;
		}
	}

	m_context.getTestContext().getLog()	<< tcu::TestLog::ImageSet("Result of rendering", "Result of rendering")
										<< tcu::TestLog::Image("Result", "Result", resultImage)
										<< tcu::TestLog::EndImageSet;

	// result
	if (++m_iteration == m_iterationCount)
	{
		if (m_allIterationsPassed)
			return tcu::TestStatus::pass("Pass");
		else
			return tcu::TestStatus::fail("Found invalid pixels");
	}
	else
		return tcu::TestStatus::incomplete();
}

int FillRuleTestInstance::getRenderSize (FillRuleCaseType type) const
{
	if (type == FILLRULECASE_CLIPPED_FULL || type == FILLRULECASE_CLIPPED_PARTIAL)
		return RESOLUTION_POT / 4;
	else
		return RESOLUTION_POT;
}

int FillRuleTestInstance::getNumIterations (FillRuleCaseType type) const
{
	if (type == FILLRULECASE_CLIPPED_FULL || type == FILLRULECASE_CLIPPED_PARTIAL)
		return 15;
	else
		return 2;
}

void FillRuleTestInstance::generateTriangles (int iteration, std::vector<tcu::Vec4>& outData) const
{
	switch (m_caseType)
	{
		case FILLRULECASE_BASIC:
		case FILLRULECASE_REVERSED:
		case FILLRULECASE_PROJECTED:
		{
			const int	numRows		= 4;
			const int	numColumns	= 4;
			const float	quadSide	= 0.15f;
			de::Random	rnd			(0xabcd);

			outData.resize(6 * numRows * numColumns);

			for (int col = 0; col < numColumns; ++col)
			for (int row = 0; row < numRows;    ++row)
			{
				const tcu::Vec2 center		= tcu::Vec2(((float)row + 0.5f) / (float)numRows * 2.0f - 1.0f, ((float)col + 0.5f) / (float)numColumns * 2.0f - 1.0f);
				const float		rotation	= (float)(iteration * numColumns * numRows + col * numRows + row) / (float)(m_iterationCount * numColumns * numRows) * DE_PI / 2.0f;
				const tcu::Vec2 sideH		= quadSide * tcu::Vec2(deFloatCos(rotation), deFloatSin(rotation));
				const tcu::Vec2 sideV		= tcu::Vec2(sideH.y(), -sideH.x());
				const tcu::Vec2 quad[4]		=
				{
					center + sideH + sideV,
					center + sideH - sideV,
					center - sideH - sideV,
					center - sideH + sideV,
				};

				if (m_caseType == FILLRULECASE_BASIC)
				{
					outData[6 * (col * numRows + row) + 0] = tcu::Vec4(quad[0].x(), quad[0].y(), 0.0f, 1.0f);
					outData[6 * (col * numRows + row) + 1] = tcu::Vec4(quad[1].x(), quad[1].y(), 0.0f, 1.0f);
					outData[6 * (col * numRows + row) + 2] = tcu::Vec4(quad[2].x(), quad[2].y(), 0.0f, 1.0f);
					outData[6 * (col * numRows + row) + 3] = tcu::Vec4(quad[2].x(), quad[2].y(), 0.0f, 1.0f);
					outData[6 * (col * numRows + row) + 4] = tcu::Vec4(quad[0].x(), quad[0].y(), 0.0f, 1.0f);
					outData[6 * (col * numRows + row) + 5] = tcu::Vec4(quad[3].x(), quad[3].y(), 0.0f, 1.0f);
				}
				else if (m_caseType == FILLRULECASE_REVERSED)
				{
					outData[6 * (col * numRows + row) + 0] = tcu::Vec4(quad[0].x(), quad[0].y(), 0.0f, 1.0f);
					outData[6 * (col * numRows + row) + 1] = tcu::Vec4(quad[1].x(), quad[1].y(), 0.0f, 1.0f);
					outData[6 * (col * numRows + row) + 2] = tcu::Vec4(quad[2].x(), quad[2].y(), 0.0f, 1.0f);
					outData[6 * (col * numRows + row) + 3] = tcu::Vec4(quad[0].x(), quad[0].y(), 0.0f, 1.0f);
					outData[6 * (col * numRows + row) + 4] = tcu::Vec4(quad[2].x(), quad[2].y(), 0.0f, 1.0f);
					outData[6 * (col * numRows + row) + 5] = tcu::Vec4(quad[3].x(), quad[3].y(), 0.0f, 1.0f);
				}
				else if (m_caseType == FILLRULECASE_PROJECTED)
				{
					const float w0 = rnd.getFloat(0.1f, 4.0f);
					const float w1 = rnd.getFloat(0.1f, 4.0f);
					const float w2 = rnd.getFloat(0.1f, 4.0f);
					const float w3 = rnd.getFloat(0.1f, 4.0f);

					outData[6 * (col * numRows + row) + 0] = tcu::Vec4(quad[0].x() * w0, quad[0].y() * w0, 0.0f, w0);
					outData[6 * (col * numRows + row) + 1] = tcu::Vec4(quad[1].x() * w1, quad[1].y() * w1, 0.0f, w1);
					outData[6 * (col * numRows + row) + 2] = tcu::Vec4(quad[2].x() * w2, quad[2].y() * w2, 0.0f, w2);
					outData[6 * (col * numRows + row) + 3] = tcu::Vec4(quad[2].x() * w2, quad[2].y() * w2, 0.0f, w2);
					outData[6 * (col * numRows + row) + 4] = tcu::Vec4(quad[0].x() * w0, quad[0].y() * w0, 0.0f, w0);
					outData[6 * (col * numRows + row) + 5] = tcu::Vec4(quad[3].x() * w3, quad[3].y() * w3, 0.0f, w3);
				}
				else
					DE_ASSERT(DE_FALSE);
			}

			break;
		}

		case FILLRULECASE_CLIPPED_PARTIAL:
		case FILLRULECASE_CLIPPED_FULL:
		{
			const float		quadSide	= (m_caseType == FILLRULECASE_CLIPPED_PARTIAL) ? (1.0f) : (2.0f);
			const tcu::Vec2 center		= (m_caseType == FILLRULECASE_CLIPPED_PARTIAL) ? (tcu::Vec2(0.5f, 0.5f)) : (tcu::Vec2(0.0f, 0.0f));
			const float		rotation	= (float)(iteration) / (float)(m_iterationCount - 1) * DE_PI / 2.0f;
			const tcu::Vec2 sideH		= quadSide * tcu::Vec2(deFloatCos(rotation), deFloatSin(rotation));
			const tcu::Vec2 sideV		= tcu::Vec2(sideH.y(), -sideH.x());
			const tcu::Vec2 quad[4]		=
			{
				center + sideH + sideV,
				center + sideH - sideV,
				center - sideH - sideV,
				center - sideH + sideV,
			};

			outData.resize(6);
			outData[0] = tcu::Vec4(quad[0].x(), quad[0].y(), 0.0f, 1.0f);
			outData[1] = tcu::Vec4(quad[1].x(), quad[1].y(), 0.0f, 1.0f);
			outData[2] = tcu::Vec4(quad[2].x(), quad[2].y(), 0.0f, 1.0f);
			outData[3] = tcu::Vec4(quad[2].x(), quad[2].y(), 0.0f, 1.0f);
			outData[4] = tcu::Vec4(quad[0].x(), quad[0].y(), 0.0f, 1.0f);
			outData[5] = tcu::Vec4(quad[3].x(), quad[3].y(), 0.0f, 1.0f);
			break;
		}

		default:
			DE_ASSERT(DE_FALSE);
	}
}

const VkPipelineColorBlendStateCreateInfo* FillRuleTestInstance::getColorBlendStateCreateInfo (void) const
{
	static const VkPipelineColorBlendAttachmentState colorBlendAttachmentState =
	{
		true,														// VkBool32			blendEnable;
		VK_BLEND_FACTOR_ONE,										// VkBlend			srcBlendColor;
		VK_BLEND_FACTOR_ONE,										// VkBlend			destBlendColor;
		VK_BLEND_OP_ADD,											// VkBlendOp		blendOpColor;
		VK_BLEND_FACTOR_ONE,										// VkBlend			srcBlendAlpha;
		VK_BLEND_FACTOR_ONE,										// VkBlend			destBlendAlpha;
		VK_BLEND_OP_ADD,											// VkBlendOp		blendOpAlpha;
		(VK_COLOR_COMPONENT_R_BIT |
		 VK_COLOR_COMPONENT_G_BIT |
		 VK_COLOR_COMPONENT_B_BIT |
		 VK_COLOR_COMPONENT_A_BIT)									// VkChannelFlags	channelWriteMask;
	};

	static const VkPipelineColorBlendStateCreateInfo colorBlendStateParams =
	{
		VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,	// VkStructureType								sType;
		DE_NULL,													// const void*									pNext;
		0,															// VkPipelineColorBlendStateCreateFlags			flags;
		false,														// VkBool32										logicOpEnable;
		VK_LOGIC_OP_COPY,											// VkLogicOp									logicOp;
		1u,															// deUint32										attachmentCount;
		&colorBlendAttachmentState,									// const VkPipelineColorBlendAttachmentState*	pAttachments;
		{ 0.0f, 0.0f, 0.0f, 0.0f },									// float										blendConst[4];
	};

	return &colorBlendStateParams;
}


class FillRuleTestCase : public BaseRenderingTestCase
{
public:
								FillRuleTestCase	(tcu::TestContext& context, const std::string& name, const std::string& description, FillRuleTestInstance::FillRuleCaseType type, VkSampleCountFlagBits sampleCount = VK_SAMPLE_COUNT_1_BIT)
									: BaseRenderingTestCase	(context, name, description, sampleCount)
									, m_type				(type)
								{}

	virtual TestInstance*		createInstance		(Context& context) const
								{
									return new FillRuleTestInstance(context, m_type, m_sampleCount);
								}
protected:
	const FillRuleTestInstance::FillRuleCaseType m_type;
};

class CullingTestInstance : public BaseRenderingTestInstance
{
public:
													CullingTestInstance				(Context& context, VkCullModeFlags cullMode, VkPrimitiveTopology primitiveTopology, VkFrontFace frontFace, VkPolygonMode polygonMode)
														: BaseRenderingTestInstance		(context, VK_SAMPLE_COUNT_1_BIT, RESOLUTION_POT)
														, m_cullMode					(cullMode)
														, m_primitiveTopology			(primitiveTopology)
														, m_frontFace					(frontFace)
														, m_polygonMode					(polygonMode)
														, m_multisampling				(true)
													{}
	virtual
	const VkPipelineRasterizationStateCreateInfo*	getRasterizationStateCreateInfo (void) const;

	tcu::TestStatus									iterate							(void);

private:
	void											generateVertices				(std::vector<tcu::Vec4>& outData) const;
	void											extractTriangles				(std::vector<TriangleSceneSpec::SceneTriangle>& outTriangles, const std::vector<tcu::Vec4>& vertices) const;
	void											extractLines					(std::vector<TriangleSceneSpec::SceneTriangle>& outTriangles, std::vector<LineSceneSpec::SceneLine>& outLines) const;
	void											extractPoints					(std::vector<TriangleSceneSpec::SceneTriangle>& outTriangles, std::vector<PointSceneSpec::ScenePoint>& outPoints) const;
	bool											triangleOrder					(const tcu::Vec4& v0, const tcu::Vec4& v1, const tcu::Vec4& v2) const;

	const VkCullModeFlags							m_cullMode;
	const VkPrimitiveTopology						m_primitiveTopology;
	const VkFrontFace								m_frontFace;
	const VkPolygonMode								m_polygonMode;
	const bool										m_multisampling;
};


tcu::TestStatus CullingTestInstance::iterate (void)
{
	DE_ASSERT(m_polygonMode <= VK_POLYGON_MODE_POINT);

	tcu::Surface									resultImage						(m_renderSize, m_renderSize);
	std::vector<tcu::Vec4>							drawBuffer;
	std::vector<TriangleSceneSpec::SceneTriangle>	triangles;
	std::vector<PointSceneSpec::ScenePoint>			points;
	std::vector<LineSceneSpec::SceneLine>			lines;

	const InstanceInterface&						vk				= m_context.getInstanceInterface();
	const VkPhysicalDevice							physicalDevice	= m_context.getPhysicalDevice();
	const VkPhysicalDeviceFeatures					deviceFeatures	= getPhysicalDeviceFeatures(vk, physicalDevice);

	if (!(deviceFeatures.fillModeNonSolid) && (m_polygonMode == VK_POLYGON_MODE_LINE || m_polygonMode == VK_POLYGON_MODE_POINT))
		TCU_THROW(NotSupportedError, "Wireframe fill modes are not supported");

	// generate scene
	generateVertices(drawBuffer);
	extractTriangles(triangles, drawBuffer);

	if (m_polygonMode == VK_POLYGON_MODE_LINE)
		extractLines(triangles ,lines);
	else if (m_polygonMode == VK_POLYGON_MODE_POINT)
		extractPoints(triangles, points);

	// draw image
	{
		m_context.getTestContext().getLog() << tcu::TestLog::Message << "Setting front face to " << m_frontFace << tcu::TestLog::EndMessage;
		m_context.getTestContext().getLog() << tcu::TestLog::Message << "Setting cull face to " << m_cullMode << tcu::TestLog::EndMessage;
		m_context.getTestContext().getLog() << tcu::TestLog::Message << "Drawing test pattern (" << m_primitiveTopology << ")" << tcu::TestLog::EndMessage;

		drawPrimitives(resultImage, drawBuffer, m_primitiveTopology);
	}

	// compare
	{
		RasterizationArguments	args;
		tcu::IVec4				colorBits	= tcu::getTextureFormatBitDepth(getTextureFormat());
		bool					isCompareOk	= false;

		args.numSamples		= m_multisampling ? 1 : 0;
		args.subpixelBits	= m_subpixelBits;
		args.redBits		= colorBits[0];
		args.greenBits		= colorBits[1];
		args.blueBits		= colorBits[2];

		switch (m_polygonMode)
		{
			case VK_POLYGON_MODE_LINE:
			{
				LineSceneSpec scene;
				scene.lineWidth = 0;
				scene.lines.swap(lines);
				isCompareOk = verifyLineGroupRasterization(resultImage, scene, args, m_context.getTestContext().getLog());
				break;
			}
			case VK_POLYGON_MODE_POINT:
			{
				PointSceneSpec scene;
				scene.points.swap(points);
				isCompareOk = verifyPointGroupRasterization(resultImage, scene, args, m_context.getTestContext().getLog());
				break;
			}
			default:
			{
				TriangleSceneSpec scene;
				scene.triangles.swap(triangles);
				isCompareOk = verifyTriangleGroupRasterization(resultImage, scene, args, m_context.getTestContext().getLog(), tcu::VERIFICATIONMODE_WEAK);
				break;
			}
		}

		if (isCompareOk)
			return tcu::TestStatus::pass("Pass");
		else
			return tcu::TestStatus::fail("Incorrect rendering");
	}
}

void CullingTestInstance::generateVertices (std::vector<tcu::Vec4>& outData) const
{
	de::Random rnd(543210);

	outData.resize(6);
	for (int vtxNdx = 0; vtxNdx < (int)outData.size(); ++vtxNdx)
	{
		outData[vtxNdx].x() = rnd.getFloat(-0.9f, 0.9f);
		outData[vtxNdx].y() = rnd.getFloat(-0.9f, 0.9f);
		outData[vtxNdx].z() = 0.0f;
		outData[vtxNdx].w() = 1.0f;
	}
}

void CullingTestInstance::extractTriangles (std::vector<TriangleSceneSpec::SceneTriangle>& outTriangles, const std::vector<tcu::Vec4>& vertices) const
{
	const bool cullDirection = (m_cullMode == VK_CULL_MODE_FRONT_BIT) ^ (m_frontFace == VK_FRONT_FACE_COUNTER_CLOCKWISE);

	// No triangles
	if (m_cullMode == VK_CULL_MODE_FRONT_AND_BACK)
		return;

	switch (m_primitiveTopology)
	{
		case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST:
		{
			for (int vtxNdx = 0; vtxNdx < (int)vertices.size() - 2; vtxNdx += 3)
			{
				const tcu::Vec4& v0 = vertices[vtxNdx + 0];
				const tcu::Vec4& v1 = vertices[vtxNdx + 1];
				const tcu::Vec4& v2 = vertices[vtxNdx + 2];

				if (triangleOrder(v0, v1, v2) != cullDirection)
				{
					TriangleSceneSpec::SceneTriangle tri;
					tri.positions[0] = v0;	tri.sharedEdge[0] = false;
					tri.positions[1] = v1;	tri.sharedEdge[1] = false;
					tri.positions[2] = v2;	tri.sharedEdge[2] = false;

					outTriangles.push_back(tri);
				}
			}
			break;
		}

		case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP:
		{
			for (int vtxNdx = 0; vtxNdx < (int)vertices.size() - 2; ++vtxNdx)
			{
				const tcu::Vec4& v0 = vertices[vtxNdx + 0];
				const tcu::Vec4& v1 = vertices[vtxNdx + 1];
				const tcu::Vec4& v2 = vertices[vtxNdx + 2];

				if (triangleOrder(v0, v1, v2) != (cullDirection ^ (vtxNdx % 2 != 0)))
				{
					TriangleSceneSpec::SceneTriangle tri;
					tri.positions[0] = v0;	tri.sharedEdge[0] = false;
					tri.positions[1] = v1;	tri.sharedEdge[1] = false;
					tri.positions[2] = v2;	tri.sharedEdge[2] = false;

					outTriangles.push_back(tri);
				}
			}
			break;
		}

		case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN:
		{
			for (int vtxNdx = 1; vtxNdx < (int)vertices.size() - 1; ++vtxNdx)
			{
				const tcu::Vec4& v0 = vertices[0];
				const tcu::Vec4& v1 = vertices[vtxNdx + 0];
				const tcu::Vec4& v2 = vertices[vtxNdx + 1];

				if (triangleOrder(v0, v1, v2) != cullDirection)
				{
					TriangleSceneSpec::SceneTriangle tri;
					tri.positions[0] = v0;	tri.sharedEdge[0] = false;
					tri.positions[1] = v1;	tri.sharedEdge[1] = false;
					tri.positions[2] = v2;	tri.sharedEdge[2] = false;

					outTriangles.push_back(tri);
				}
			}
			break;
		}

		default:
			DE_ASSERT(false);
	}
}

void CullingTestInstance::extractLines (std::vector<TriangleSceneSpec::SceneTriangle>&	outTriangles,
										std::vector<LineSceneSpec::SceneLine>&			outLines) const
{
	for (int triNdx = 0; triNdx < (int)outTriangles.size(); ++triNdx)
	{
		for (int vrtxNdx = 0; vrtxNdx < 2; ++vrtxNdx)
		{
			LineSceneSpec::SceneLine line;
			line.positions[0] = outTriangles.at(triNdx).positions[vrtxNdx];
			line.positions[1] = outTriangles.at(triNdx).positions[vrtxNdx + 1];

			outLines.push_back(line);
		}
		LineSceneSpec::SceneLine line;
		line.positions[0] = outTriangles.at(triNdx).positions[2];
		line.positions[1] = outTriangles.at(triNdx).positions[0];
		outLines.push_back(line);
	}
}

void CullingTestInstance::extractPoints (std::vector<TriangleSceneSpec::SceneTriangle>	&outTriangles,
										std::vector<PointSceneSpec::ScenePoint>			&outPoints) const
{
	for (int triNdx = 0; triNdx < (int)outTriangles.size(); ++triNdx)
	{
		for (int vrtxNdx = 0; vrtxNdx < 3; ++vrtxNdx)
		{
			PointSceneSpec::ScenePoint point;
			point.position = outTriangles.at(triNdx).positions[vrtxNdx];
			point.pointSize = 1.0f;

			outPoints.push_back(point);
		}
	}
}

bool CullingTestInstance::triangleOrder (const tcu::Vec4& v0, const tcu::Vec4& v1, const tcu::Vec4& v2) const
{
	const tcu::Vec2 s0 = v0.swizzle(0, 1) / v0.w();
	const tcu::Vec2 s1 = v1.swizzle(0, 1) / v1.w();
	const tcu::Vec2 s2 = v2.swizzle(0, 1) / v2.w();

	// cross
	return ((s1.x() - s0.x()) * (s2.y() - s0.y()) - (s2.x() - s0.x()) * (s1.y() - s0.y())) > 0;
}


const VkPipelineRasterizationStateCreateInfo* CullingTestInstance::getRasterizationStateCreateInfo (void) const
{
	static VkPipelineRasterizationStateCreateInfo	rasterizationStateCreateInfo	=
	{
		VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,		// VkStructureType							sType;
		DE_NULL,														// const void*								pNext;
		0,																// VkPipelineRasterizationStateCreateFlags	flags;
		false,															// VkBool32									depthClipEnable;
		false,															// VkBool32									rasterizerDiscardEnable;
		VK_POLYGON_MODE_FILL,											// VkFillMode								fillMode;
		VK_CULL_MODE_NONE,												// VkCullMode								cullMode;
		VK_FRONT_FACE_COUNTER_CLOCKWISE,								// VkFrontFace								frontFace;
		VK_FALSE,														// VkBool32									depthBiasEnable;
		0.0f,															// float									depthBias;
		0.0f,															// float									depthBiasClamp;
		0.0f,															// float									slopeScaledDepthBias;
		getLineWidth(),													// float									lineWidth;
	};

	rasterizationStateCreateInfo.lineWidth		= getLineWidth();
	rasterizationStateCreateInfo.cullMode		= m_cullMode;
	rasterizationStateCreateInfo.frontFace		= m_frontFace;
	rasterizationStateCreateInfo.polygonMode	= m_polygonMode;

	return &rasterizationStateCreateInfo;
}

class CullingTestCase : public BaseRenderingTestCase
{
public:
								CullingTestCase		(tcu::TestContext& context, const std::string& name, const std::string& description, VkCullModeFlags cullMode, VkPrimitiveTopology primitiveTopology, VkFrontFace frontFace, VkPolygonMode polygonMode, VkSampleCountFlagBits sampleCount = VK_SAMPLE_COUNT_1_BIT)
									: BaseRenderingTestCase	(context, name, description, sampleCount)
									, m_cullMode			(cullMode)
									, m_primitiveTopology	(primitiveTopology)
									, m_frontFace			(frontFace)
									, m_polygonMode			(polygonMode)
								{}

	virtual TestInstance*		createInstance		(Context& context) const
								{
									return new CullingTestInstance(context, m_cullMode, m_primitiveTopology, m_frontFace, m_polygonMode);
								}
protected:
	const VkCullModeFlags		m_cullMode;
	const VkPrimitiveTopology	m_primitiveTopology;
	const VkFrontFace			m_frontFace;
	const VkPolygonMode			m_polygonMode;
};

class DiscardTestInstance : public BaseRenderingTestInstance
{
public:
															DiscardTestInstance					(Context& context, VkPrimitiveTopology primitiveTopology, deBool queryFragmentShaderInvocations)
																: BaseRenderingTestInstance			(context, VK_SAMPLE_COUNT_1_BIT, RESOLUTION_POT)
																, m_primitiveTopology				(primitiveTopology)
																, m_queryFragmentShaderInvocations	(queryFragmentShaderInvocations)
															{}

	virtual const VkPipelineRasterizationStateCreateInfo*	getRasterizationStateCreateInfo		(void) const;
	tcu::TestStatus											iterate								(void);

private:
	void													generateVertices					(std::vector<tcu::Vec4>& outData) const;
	void													extractTriangles					(std::vector<TriangleSceneSpec::SceneTriangle>& outTriangles, const std::vector<tcu::Vec4>& vertices) const;
	void													extractLines						(std::vector<LineSceneSpec::SceneLine>& outLines, const std::vector<tcu::Vec4>& vertices) const;
	void													extractPoints						(std::vector<PointSceneSpec::ScenePoint>& outPoints, const std::vector<tcu::Vec4>& vertices) const;
	void													drawPrimitives						(tcu::Surface& result, const std::vector<tcu::Vec4>& positionData, VkPrimitiveTopology primitiveTopology, Move<VkQueryPool>& queryPool);

	const VkPrimitiveTopology								m_primitiveTopology;
	const deBool											m_queryFragmentShaderInvocations;
};

tcu::TestStatus DiscardTestInstance::iterate (void)
{
	const DeviceInterface&							vkd			= m_context.getDeviceInterface();
	const VkDevice									vkDevice	= m_context.getDevice();
	deUint64										queryResult	= 0u;
	tcu::Surface									resultImage	(m_renderSize, m_renderSize);
	std::vector<tcu::Vec4>							drawBuffer;
	std::vector<PointSceneSpec::ScenePoint>			points;
	std::vector<LineSceneSpec::SceneLine>			lines;
	std::vector<TriangleSceneSpec::SceneTriangle>	triangles;

	generateVertices(drawBuffer);

	switch (m_primitiveTopology)
	{
		case VK_PRIMITIVE_TOPOLOGY_POINT_LIST:
			extractPoints(points, drawBuffer);
			break;

		case VK_PRIMITIVE_TOPOLOGY_LINE_LIST:
		case VK_PRIMITIVE_TOPOLOGY_LINE_STRIP:
			extractLines(lines, drawBuffer);
			break;

		case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST:
		case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP:
		case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN:
			extractTriangles(triangles, drawBuffer);
			break;

		default:
			DE_ASSERT(false);
	}

	const VkQueryPoolCreateInfo queryPoolCreateInfo =
	{
		VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,						// VkStructureType					sType
		DE_NULL,														// const void*						pNext
		(VkQueryPoolCreateFlags)0,										// VkQueryPoolCreateFlags			flags
		VK_QUERY_TYPE_PIPELINE_STATISTICS ,								// VkQueryType						queryType
		1u,																// deUint32							entryCount
		VK_QUERY_PIPELINE_STATISTIC_FRAGMENT_SHADER_INVOCATIONS_BIT,	// VkQueryPipelineStatisticFlags	pipelineStatistics
	};

	if (m_queryFragmentShaderInvocations)
	{
		Move<VkQueryPool> queryPool	= createQueryPool(vkd, vkDevice, &queryPoolCreateInfo);

		drawPrimitives(resultImage, drawBuffer, m_primitiveTopology, queryPool);
		vkd.getQueryPoolResults(vkDevice, *queryPool, 0u, 1u, sizeof(deUint64), &queryResult, 0u, VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);
	}
	else
		BaseRenderingTestInstance::drawPrimitives(resultImage, drawBuffer, m_primitiveTopology);

	// compare
	{
		tcu::IVec4						colorBits	= tcu::getTextureFormatBitDepth(getTextureFormat());

		const RasterizationArguments	args		=
		{
			0,							// int	numSamples;
			(int)m_subpixelBits,		// int	subpixelBits;
			colorBits[0],				// int	redBits;
			colorBits[1],				// int	greenBits;
			colorBits[2]				// int	blueBits;
		};

		// Empty scene to compare to, primitives should be discarded before rasterization
		TriangleSceneSpec				scene;

		const bool						isCompareOk	= verifyTriangleGroupRasterization(resultImage,
																					   scene,
																					   args,
																					   m_context.getTestContext().getLog(),
																					   tcu::VERIFICATIONMODE_STRICT);

		if (isCompareOk)
		{
			if (m_queryFragmentShaderInvocations && queryResult > 0u)
				return tcu::TestStatus::fail("Fragment shader invocations occured");
			else
				return tcu::TestStatus::pass("Pass");
		}
		else
			return tcu::TestStatus::fail("Incorrect rendering");
	}
}

void DiscardTestInstance::generateVertices (std::vector<tcu::Vec4>& outData) const
{
	de::Random rnd(12345);

	outData.resize(6);

	for (int vtxNdx = 0; vtxNdx < (int)outData.size(); ++vtxNdx)
	{
		outData[vtxNdx].x() = rnd.getFloat(-0.9f, 0.9f);
		outData[vtxNdx].y() = rnd.getFloat(-0.9f, 0.9f);
		outData[vtxNdx].z() = 0.0f;
		outData[vtxNdx].w() = 1.0f;
	}
}

void DiscardTestInstance::extractTriangles (std::vector<TriangleSceneSpec::SceneTriangle>& outTriangles, const std::vector<tcu::Vec4>& vertices) const
{
	switch (m_primitiveTopology)
	{
		case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST:
		{
			for (int vtxNdx = 0; vtxNdx < (int)vertices.size() - 2; vtxNdx += 3)
			{
				TriangleSceneSpec::SceneTriangle	tri;
				const tcu::Vec4&					v0	= vertices[vtxNdx + 0];
				const tcu::Vec4&					v1	= vertices[vtxNdx + 1];
				const tcu::Vec4&					v2	= vertices[vtxNdx + 2];

				tri.positions[0] = v0;
				tri.positions[1] = v1;
				tri.positions[2] = v2;

				outTriangles.push_back(tri);
			}

			break;
		}

		case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP:
		{
			for (int vtxNdx = 0; vtxNdx < (int)vertices.size() - 2; ++vtxNdx)
			{
				TriangleSceneSpec::SceneTriangle	tri;
				const tcu::Vec4&					v0	= vertices[vtxNdx + 0];
				const tcu::Vec4&					v1	= vertices[vtxNdx + 1];
				const tcu::Vec4&					v2	= vertices[vtxNdx + 2];

				tri.positions[0] = v0;
				tri.positions[1] = v1;
				tri.positions[2] = v2;

				outTriangles.push_back(tri);
			}

			break;
		}

		case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN:
		{
			for (int vtxNdx = 1; vtxNdx < (int)vertices.size() - 1; ++vtxNdx)
			{
				TriangleSceneSpec::SceneTriangle	tri;
				const tcu::Vec4&					v0	= vertices[0];
				const tcu::Vec4&					v1	= vertices[vtxNdx + 0];
				const tcu::Vec4&					v2	= vertices[vtxNdx + 1];

				tri.positions[0] = v0;
				tri.positions[1] = v1;
				tri.positions[2] = v2;

				outTriangles.push_back(tri);
			}

			break;
		}

		default:
			DE_ASSERT(false);
	}
}

void DiscardTestInstance::extractLines (std::vector<LineSceneSpec::SceneLine>& outLines, const std::vector<tcu::Vec4>& vertices) const
{
	switch (m_primitiveTopology)
	{
		case VK_PRIMITIVE_TOPOLOGY_LINE_LIST:
		{
			for (int vtxNdx = 0; vtxNdx < (int)vertices.size() - 1; vtxNdx += 2)
			{
				LineSceneSpec::SceneLine line;

				line.positions[0] = vertices[vtxNdx + 0];
				line.positions[1] = vertices[vtxNdx + 1];

				outLines.push_back(line);
			}

			break;
		}

		case VK_PRIMITIVE_TOPOLOGY_LINE_STRIP:
		{
			for (int vtxNdx = 0; vtxNdx < (int)vertices.size() - 1; ++vtxNdx)
			{
				LineSceneSpec::SceneLine line;

				line.positions[0] = vertices[vtxNdx + 0];
				line.positions[1] = vertices[vtxNdx + 1];

				outLines.push_back(line);
			}

			break;
		}

		default:
			DE_ASSERT(false);
		}
}

void DiscardTestInstance::extractPoints (std::vector<PointSceneSpec::ScenePoint>& outPoints, const std::vector<tcu::Vec4>& vertices) const
{
	for (int pointNdx = 0; pointNdx < (int)outPoints.size(); ++pointNdx)
	{
		for (int vrtxNdx = 0; vrtxNdx < 3; ++vrtxNdx)
		{
			PointSceneSpec::ScenePoint point;

			point.position	= vertices[vrtxNdx];
			point.pointSize	= 1.0f;

			outPoints.push_back(point);
		}
	}
}

const VkPipelineRasterizationStateCreateInfo* DiscardTestInstance::getRasterizationStateCreateInfo (void) const
{
	static const VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,	// VkStructureType							sType;
		NULL,														// const void*								pNext;
		0,															// VkPipelineRasterizationStateCreateFlags	flags;
		VK_FALSE,													// VkBool32									depthClipEnable;
		VK_TRUE,													// VkBool32									rasterizerDiscardEnable;
		VK_POLYGON_MODE_FILL,										// VkFillMode								fillMode;
		VK_CULL_MODE_NONE,											// VkCullMode								cullMode;
		VK_FRONT_FACE_COUNTER_CLOCKWISE,							// VkFrontFace								frontFace;
		VK_FALSE,													// VkBool32									depthBiasEnable;
		0.0f,														// float									depthBias;
		0.0f,														// float									depthBiasClamp;
		0.0f,														// float									slopeScaledDepthBias;
		getLineWidth(),												// float									lineWidth;
	};

	return &rasterizationStateCreateInfo;
}

void DiscardTestInstance::drawPrimitives (tcu::Surface& result, const std::vector<tcu::Vec4>& positionData, VkPrimitiveTopology primitiveTopology, Move<VkQueryPool>& queryPool)
{
	const DeviceInterface&				vkd					= m_context.getDeviceInterface();
	const VkDevice						vkDevice			= m_context.getDevice();
	const VkPhysicalDeviceProperties	properties			= m_context.getDeviceProperties();
	const VkQueue						queue				= m_context.getUniversalQueue();
	const deUint32						queueFamilyIndex	= m_context.getUniversalQueueFamilyIndex();
	Allocator&							allocator			= m_context.getDefaultAllocator();

	const size_t						attributeBatchSize	= positionData.size() * sizeof(tcu::Vec4);
	const VkDeviceSize					vertexBufferOffset	= 0;
	de::MovePtr<Allocation>				vertexBufferMemory;
	Move<VkBuffer>						vertexBuffer;
	Move<VkCommandBuffer>				commandBuffer;
	Move<VkPipeline>					graphicsPipeline;

	if (attributeBatchSize > properties.limits.maxVertexInputAttributeOffset)
	{
		std::stringstream message;
		message << "Larger vertex input attribute offset is needed (" << attributeBatchSize << ") than the available maximum (" << properties.limits.maxVertexInputAttributeOffset << ").";
		TCU_THROW(NotSupportedError, message.str().c_str());
	}

	// Create Graphics Pipeline
	{
		const VkVertexInputBindingDescription		vertexInputBindingDescription		=
		{
			0u,										// deUint32					binding;
			sizeof(tcu::Vec4),						// deUint32					strideInBytes;
			VK_VERTEX_INPUT_RATE_VERTEX				// VkVertexInputStepRate	stepRate;
		};

		const VkVertexInputAttributeDescription		vertexInputAttributeDescriptions[2]	=
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
				(deUint32)attributeBatchSize		// deUint32	offsetInBytes;
			}
		};

		const VkPipelineVertexInputStateCreateInfo	vertexInputStateParams				=
		{
			VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,	// VkStructureType							sType;
			DE_NULL,													// const void*								pNext;
			0,															// VkPipelineVertexInputStateCreateFlags	flags;
			1u,															// deUint32									bindingCount;
			&vertexInputBindingDescription,								// const VkVertexInputBindingDescription*	pVertexBindingDescriptions;
			2u,															// deUint32									attributeCount;
			vertexInputAttributeDescriptions							// const VkVertexInputAttributeDescription*	pVertexAttributeDescriptions;
		};

		const std::vector<VkViewport>				viewports							(1, makeViewport(tcu::UVec2(m_renderSize)));
		const std::vector<VkRect2D>					scissors							(1, makeRect2D(tcu::UVec2(m_renderSize)));

		const VkPipelineMultisampleStateCreateInfo	multisampleStateParams				=
		{
			VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,	// VkStructureType							sType;
			DE_NULL,													// const void*								pNext;
			0u,															// VkPipelineMultisampleStateCreateFlags	flags;
			m_sampleCount,												// VkSampleCountFlagBits					rasterizationSamples;
			VK_FALSE,													// VkBool32									sampleShadingEnable;
			0.0f,														// float									minSampleShading;
			DE_NULL,													// const VkSampleMask*						pSampleMask;
			VK_FALSE,													// VkBool32									alphaToCoverageEnable;
			VK_FALSE													// VkBool32									alphaToOneEnable;
		};

		graphicsPipeline = makeGraphicsPipeline(vkd,								// const DeviceInterface&							vk
												vkDevice,							// const VkDevice									device
												*m_pipelineLayout,					// const VkPipelineLayout							pipelineLayout
												*m_vertexShaderModule,				// const VkShaderModule								vertexShaderModule
												DE_NULL,							// const VkShaderModule								tessellationControlShaderModule
												DE_NULL,							// const VkShaderModule								tessellationEvalShaderModule
												DE_NULL,							// const VkShaderModule								geometryShaderModule
												*m_fragmentShaderModule,			// const VkShaderModule								fragmentShaderModule
												*m_renderPass,						// const VkRenderPass								renderPass
												viewports,							// const std::vector<VkViewport>&					viewports
												scissors,							// const std::vector<VkRect2D>&						scissors
												primitiveTopology,					// const VkPrimitiveTopology						topology
												0u,									// const deUint32									subpass
												0u,									// const deUint32									patchControlPoints
												&vertexInputStateParams,			// const VkPipelineVertexInputStateCreateInfo*		vertexInputStateCreateInfo
												getRasterizationStateCreateInfo(),	// const VkPipelineRasterizationStateCreateInfo*	rasterizationStateCreateInfo
												&multisampleStateParams,			// const VkPipelineMultisampleStateCreateInfo*		multisampleStateCreateInfo
												DE_NULL,							// const VkPipelineDepthStencilStateCreateInfo*		depthStencilStateCreateInfo,
												getColorBlendStateCreateInfo());	// const VkPipelineColorBlendStateCreateInfo*		colorBlendStateCreateInfo
	}

	// Create Vertex Buffer
	{
		const VkBufferCreateInfo					vertexBufferParams		=
		{
			VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,	// VkStructureType		sType;
			DE_NULL,								// const void*			pNext;
			0u,										// VkBufferCreateFlags	flags;
			attributeBatchSize * 2,					// VkDeviceSize			size;
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,		// VkBufferUsageFlags	usage;
			VK_SHARING_MODE_EXCLUSIVE,				// VkSharingMode		sharingMode;
			1u,										// deUint32				queueFamilyCount;
			&queueFamilyIndex						// const deUint32*		pQueueFamilyIndices;
		};

		const std::vector<tcu::Vec4>				colorData				(positionData.size(), tcu::Vec4(1.0f, 1.0f, 1.0f, 1.0f));

		vertexBuffer		= createBuffer(vkd, vkDevice, &vertexBufferParams);
		vertexBufferMemory	= allocator.allocate(getBufferMemoryRequirements(vkd, vkDevice, *vertexBuffer), MemoryRequirement::HostVisible);

		VK_CHECK(vkd.bindBufferMemory(vkDevice, *vertexBuffer, vertexBufferMemory->getMemory(), vertexBufferMemory->getOffset()));

		// Load vertices into vertex buffer
		deMemcpy(vertexBufferMemory->getHostPtr(), positionData.data(), attributeBatchSize);
		deMemcpy(reinterpret_cast<deUint8*>(vertexBufferMemory->getHostPtr()) + attributeBatchSize, colorData.data(), attributeBatchSize);
		flushAlloc(vkd, vkDevice, *vertexBufferMemory);
	}

	// Create Command Buffer
	commandBuffer = allocateCommandBuffer(vkd, vkDevice, *m_commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

	// Begin Command Buffer
	beginCommandBuffer(vkd, *commandBuffer);

	addImageTransitionBarrier(*commandBuffer,									// VkCommandBuffer			commandBuffer
							  *m_image,											// VkImage					image
							  VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,				// VkPipelineStageFlags		srcStageMask
							  VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,				// VkPipelineStageFlags		dstStageMask
							  0,												// VkAccessFlags			srcAccessMask
							  VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,				// VkAccessFlags			dstAccessMask
							  VK_IMAGE_LAYOUT_UNDEFINED,						// VkImageLayout			oldLayout;
							  VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);		// VkImageLayout			newLayout;

	if (m_multisampling)
	{
		addImageTransitionBarrier(*commandBuffer,								// VkCommandBuffer			commandBuffer
								  *m_resolvedImage,								// VkImage					image
								  VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,			// VkPipelineStageFlags		srcStageMask
								  VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,			// VkPipelineStageFlags		dstStageMask
								  0,											// VkAccessFlags			srcAccessMask
								  VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,			// VkAccessFlags			dstAccessMask
								  VK_IMAGE_LAYOUT_UNDEFINED,					// VkImageLayout			oldLayout;
								  VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);	// VkImageLayout			newLayout;
	}

	// Reset query pool
	vkd.cmdResetQueryPool(*commandBuffer, *queryPool, 0u, 1u);

	// Begin render pass and start query
	beginRenderPass(vkd, *commandBuffer, *m_renderPass, *m_frameBuffer, vk::makeRect2D(0, 0, m_renderSize, m_renderSize), tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f));
	vkd.cmdBeginQuery(*commandBuffer, *queryPool, 0u, (VkQueryControlFlags)0u);
	vkd.cmdBindPipeline(*commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *graphicsPipeline);
	vkd.cmdBindDescriptorSets(*commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipelineLayout, 0u, 1, &m_descriptorSet.get(), 0u, DE_NULL);
	vkd.cmdBindVertexBuffers(*commandBuffer, 0, 1, &vertexBuffer.get(), &vertexBufferOffset);
	vkd.cmdDraw(*commandBuffer, (deUint32)positionData.size(), 1, 0, 0);
	endRenderPass(vkd, *commandBuffer);
	vkd.cmdEndQuery(*commandBuffer, *queryPool, 0u);

	// Copy Image
	copyImageToBuffer(vkd, *commandBuffer, m_multisampling ? *m_resolvedImage : *m_image, *m_resultBuffer, tcu::IVec2(m_renderSize, m_renderSize));

	endCommandBuffer(vkd, *commandBuffer);

	// Set Point Size
	{
		float pointSize = getPointSize();

		deMemcpy(m_uniformBufferMemory->getHostPtr(), &pointSize, (size_t)m_uniformBufferSize);
		flushAlloc(vkd, vkDevice, *m_uniformBufferMemory);
	}

	// Submit
	submitCommandsAndWait(vkd, vkDevice, queue, commandBuffer.get());

	invalidateAlloc(vkd, vkDevice, *m_resultBufferMemory);
	tcu::copy(result.getAccess(), tcu::ConstPixelBufferAccess(m_textureFormat, tcu::IVec3(m_renderSize, m_renderSize, 1), m_resultBufferMemory->getHostPtr()));
}

class DiscardTestCase : public BaseRenderingTestCase
{
public:
								DiscardTestCase					(tcu::TestContext& context, const std::string& name, const std::string& description, VkPrimitiveTopology primitiveTopology, deBool queryFragmentShaderInvocations, VkSampleCountFlagBits sampleCount = VK_SAMPLE_COUNT_1_BIT)
									: BaseRenderingTestCase				(context, name, description, sampleCount)
									, m_primitiveTopology				(primitiveTopology)
									, m_queryFragmentShaderInvocations	(queryFragmentShaderInvocations)
								{}

	virtual TestInstance*		createInstance		(Context& context) const
								{
									return new DiscardTestInstance (context, m_primitiveTopology, m_queryFragmentShaderInvocations);
								}

	virtual	void				checkSupport		(Context& context) const
								{
									if (m_queryFragmentShaderInvocations)
										context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_PIPELINE_STATISTICS_QUERY);
								}

protected:
	const VkPrimitiveTopology	m_primitiveTopology;
	const deBool				m_queryFragmentShaderInvocations;
};

class TriangleInterpolationTestInstance : public BaseRenderingTestInstance
{
public:

								TriangleInterpolationTestInstance	(Context& context, VkPrimitiveTopology primitiveTopology, int flags, VkSampleCountFlagBits sampleCount)
									: BaseRenderingTestInstance	(context, sampleCount, RESOLUTION_POT)
									, m_primitiveTopology		(primitiveTopology)
									, m_projective				((flags & INTERPOLATIONFLAGS_PROJECTED) != 0)
									, m_iterationCount			(3)
									, m_iteration				(0)
									, m_allIterationsPassed		(true)
									, m_flatshade				((flags & INTERPOLATIONFLAGS_FLATSHADE) != 0)
								{}

	tcu::TestStatus				iterate								(void);


private:
	void						generateVertices					(int iteration, std::vector<tcu::Vec4>& outVertices, std::vector<tcu::Vec4>& outColors) const;
	void						extractTriangles					(std::vector<TriangleSceneSpec::SceneTriangle>& outTriangles, const std::vector<tcu::Vec4>& vertices, const std::vector<tcu::Vec4>& colors) const;


	VkPrimitiveTopology			m_primitiveTopology;
	const bool					m_projective;
	const int					m_iterationCount;
	int							m_iteration;
	bool						m_allIterationsPassed;
	const deBool				m_flatshade;
};

tcu::TestStatus TriangleInterpolationTestInstance::iterate (void)
{
	const std::string								iterationDescription	= "Test iteration " + de::toString(m_iteration+1) + " / " + de::toString(m_iterationCount);
	const tcu::ScopedLogSection						section					(m_context.getTestContext().getLog(), "Iteration" + de::toString(m_iteration+1), iterationDescription);
	tcu::Surface									resultImage				(m_renderSize, m_renderSize);
	std::vector<tcu::Vec4>							drawBuffer;
	std::vector<tcu::Vec4>							colorBuffer;
	std::vector<TriangleSceneSpec::SceneTriangle>	triangles;

	// generate scene
	generateVertices(m_iteration, drawBuffer, colorBuffer);
	extractTriangles(triangles, drawBuffer, colorBuffer);

	// log
	{
		m_context.getTestContext().getLog() << tcu::TestLog::Message << "Generated vertices:" << tcu::TestLog::EndMessage;
		for (int vtxNdx = 0; vtxNdx < (int)drawBuffer.size(); ++vtxNdx)
			m_context.getTestContext().getLog() << tcu::TestLog::Message << "\t" << drawBuffer[vtxNdx] << ",\tcolor= " << colorBuffer[vtxNdx] << tcu::TestLog::EndMessage;
	}

	// draw image
	drawPrimitives(resultImage, drawBuffer, colorBuffer, m_primitiveTopology);

	// compare
	{
		RasterizationArguments	args;
		TriangleSceneSpec		scene;
		tcu::IVec4				colorBits	= tcu::getTextureFormatBitDepth(getTextureFormat());

		args.numSamples		= m_multisampling ? 1 : 0;
		args.subpixelBits	= m_subpixelBits;
		args.redBits		= colorBits[0];
		args.greenBits		= colorBits[1];
		args.blueBits		= colorBits[2];

		scene.triangles.swap(triangles);

		if (!verifyTriangleGroupInterpolation(resultImage, scene, args, m_context.getTestContext().getLog()))
			m_allIterationsPassed = false;
	}

	// result
	if (++m_iteration == m_iterationCount)
	{
		if (m_allIterationsPassed)
			return tcu::TestStatus::pass("Pass");
		else
			return tcu::TestStatus::fail("Found invalid pixel values");
	}
	else
		return tcu::TestStatus::incomplete();
}

void TriangleInterpolationTestInstance::generateVertices (int iteration, std::vector<tcu::Vec4>& outVertices, std::vector<tcu::Vec4>& outColors) const
{
	// use only red, green and blue
	const tcu::Vec4 colors[] =
	{
		tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f),
		tcu::Vec4(0.0f, 1.0f, 0.0f, 1.0f),
		tcu::Vec4(0.0f, 0.0f, 1.0f, 1.0f),
	};

	de::Random rnd(123 + iteration * 1000 + (int)m_primitiveTopology);

	outVertices.resize(6);
	outColors.resize(6);

	for (int vtxNdx = 0; vtxNdx < (int)outVertices.size(); ++vtxNdx)
	{
		outVertices[vtxNdx].x() = rnd.getFloat(-0.9f, 0.9f);
		outVertices[vtxNdx].y() = rnd.getFloat(-0.9f, 0.9f);
		outVertices[vtxNdx].z() = 0.0f;

		if (!m_projective)
			outVertices[vtxNdx].w() = 1.0f;
		else
		{
			const float w = rnd.getFloat(0.2f, 4.0f);

			outVertices[vtxNdx].x() *= w;
			outVertices[vtxNdx].y() *= w;
			outVertices[vtxNdx].z() *= w;
			outVertices[vtxNdx].w() = w;
		}

		outColors[vtxNdx] = colors[vtxNdx % DE_LENGTH_OF_ARRAY(colors)];
	}
}

void TriangleInterpolationTestInstance::extractTriangles (std::vector<TriangleSceneSpec::SceneTriangle>& outTriangles, const std::vector<tcu::Vec4>& vertices, const std::vector<tcu::Vec4>& colors) const
{
	switch (m_primitiveTopology)
	{
		case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST:
		{
			for (int vtxNdx = 0; vtxNdx < (int)vertices.size() - 2; vtxNdx += 3)
			{
				TriangleSceneSpec::SceneTriangle tri;
				tri.positions[0]	= vertices[vtxNdx + 0];
				tri.positions[1]	= vertices[vtxNdx + 1];
				tri.positions[2]	= vertices[vtxNdx + 2];
				tri.sharedEdge[0]	= false;
				tri.sharedEdge[1]	= false;
				tri.sharedEdge[2]	= false;

				if (m_flatshade)
				{
					tri.colors[0] = colors[vtxNdx];
					tri.colors[1] = colors[vtxNdx];
					tri.colors[2] = colors[vtxNdx];
				}
				else
				{
					tri.colors[0] = colors[vtxNdx + 0];
					tri.colors[1] = colors[vtxNdx + 1];
					tri.colors[2] = colors[vtxNdx + 2];
				}

				outTriangles.push_back(tri);
			}
			break;
		}

		case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP:
		{
			for (int vtxNdx = 0; vtxNdx < (int)vertices.size() - 2; ++vtxNdx)
			{
				TriangleSceneSpec::SceneTriangle tri;
				tri.positions[0]	= vertices[vtxNdx + 0];
				tri.positions[1]	= vertices[vtxNdx + 1];
				tri.positions[2]	= vertices[vtxNdx + 2];
				tri.sharedEdge[0]	= false;
				tri.sharedEdge[1]	= false;
				tri.sharedEdge[2]	= false;

				if (m_flatshade)
				{
					tri.colors[0] = colors[vtxNdx];
					tri.colors[1] = colors[vtxNdx];
					tri.colors[2] = colors[vtxNdx];
				}
				else
				{
					tri.colors[0] = colors[vtxNdx + 0];
					tri.colors[1] = colors[vtxNdx + 1];
					tri.colors[2] = colors[vtxNdx + 2];
				}

				outTriangles.push_back(tri);
			}
			break;
		}

		case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN:
		{
			for (int vtxNdx = 1; vtxNdx < (int)vertices.size() - 1; ++vtxNdx)
			{
				TriangleSceneSpec::SceneTriangle tri;
				tri.positions[0]	= vertices[0];
				tri.positions[1]	= vertices[vtxNdx + 0];
				tri.positions[2]	= vertices[vtxNdx + 1];
				tri.sharedEdge[0]	= false;
				tri.sharedEdge[1]	= false;
				tri.sharedEdge[2]	= false;

				if (m_flatshade)
				{
					tri.colors[0] = colors[vtxNdx];
					tri.colors[1] = colors[vtxNdx];
					tri.colors[2] = colors[vtxNdx];
				}
				else
				{
					tri.colors[0] = colors[0];
					tri.colors[1] = colors[vtxNdx + 0];
					tri.colors[2] = colors[vtxNdx + 1];
				}

				outTriangles.push_back(tri);
			}
			break;
		}

		default:
			DE_ASSERT(false);
	}
}

class TriangleInterpolationTestCase : public BaseRenderingTestCase
{
public:
								TriangleInterpolationTestCase	(tcu::TestContext& context, const std::string& name, const std::string& description, VkPrimitiveTopology primitiveTopology, int flags, VkSampleCountFlagBits sampleCount = VK_SAMPLE_COUNT_1_BIT)
									: BaseRenderingTestCase		(context, name, description, sampleCount, (flags & INTERPOLATIONFLAGS_FLATSHADE) != 0)
									, m_primitiveTopology		(primitiveTopology)
									, m_flags					(flags)
								{}

	virtual TestInstance*		createInstance					(Context& context) const
								{
									return new TriangleInterpolationTestInstance(context, m_primitiveTopology, m_flags, m_sampleCount);
								}
protected:
	const VkPrimitiveTopology	m_primitiveTopology;
	const int					m_flags;
};

class LineInterpolationTestInstance : public BaseRenderingTestInstance
{
public:
							LineInterpolationTestInstance	(Context& context, VkPrimitiveTopology primitiveTopology, int flags, PrimitiveWideness wideness, PrimitiveStrictness strictness, VkSampleCountFlagBits sampleCount);

	virtual tcu::TestStatus	iterate							(void);

private:
	void					generateVertices				(int iteration, std::vector<tcu::Vec4>& outVertices, std::vector<tcu::Vec4>& outColors) const;
	void					extractLines					(std::vector<LineSceneSpec::SceneLine>& outLines, const std::vector<tcu::Vec4>& vertices, const std::vector<tcu::Vec4>& colors) const;
	virtual float			getLineWidth					(void) const;

	VkPrimitiveTopology		m_primitiveTopology;
	const bool				m_projective;
	const int				m_iterationCount;
	const PrimitiveWideness	m_primitiveWideness;

	int						m_iteration;
	bool					m_allIterationsPassed;
	float					m_maxLineWidth;
	std::vector<float>		m_lineWidths;
	bool					m_flatshade;
	PrimitiveStrictness		m_strictness;
};

LineInterpolationTestInstance::LineInterpolationTestInstance (Context& context, VkPrimitiveTopology primitiveTopology, int flags, PrimitiveWideness wideness, PrimitiveStrictness strictness, VkSampleCountFlagBits sampleCount)
	: BaseRenderingTestInstance			(context, sampleCount)
	, m_primitiveTopology				(primitiveTopology)
	, m_projective						((flags & INTERPOLATIONFLAGS_PROJECTED) != 0)
	, m_iterationCount					(3)
	, m_primitiveWideness				(wideness)
	, m_iteration						(0)
	, m_allIterationsPassed				(true)
	, m_maxLineWidth					(1.0f)
	, m_flatshade						((flags & INTERPOLATIONFLAGS_FLATSHADE) != 0)
	, m_strictness						(strictness)
{
	DE_ASSERT(m_primitiveWideness < PRIMITIVEWIDENESS_LAST);

	// create line widths
	if (m_primitiveWideness == PRIMITIVEWIDENESS_NARROW)
	{
		m_lineWidths.resize(m_iterationCount, 1.0f);
	}
	else if (m_primitiveWideness == PRIMITIVEWIDENESS_WIDE)
	{
		const float*	range = context.getDeviceProperties().limits.lineWidthRange;

		m_context.getTestContext().getLog() << tcu::TestLog::Message << "ALIASED_LINE_WIDTH_RANGE = [" << range[0] << ", " << range[1] << "]" << tcu::TestLog::EndMessage;

		DE_ASSERT(range[1] > 1.0f);

		// set hand picked sizes
		m_lineWidths.push_back(5.0f);
		m_lineWidths.push_back(10.0f);
		m_lineWidths.push_back(range[1]);
		DE_ASSERT((int)m_lineWidths.size() == m_iterationCount);

		m_maxLineWidth = range[1];
	}
	else
		DE_ASSERT(false);
}

tcu::TestStatus LineInterpolationTestInstance::iterate (void)
{
	const std::string						iterationDescription	= "Test iteration " + de::toString(m_iteration+1) + " / " + de::toString(m_iterationCount);
	const tcu::ScopedLogSection				section					(m_context.getTestContext().getLog(), "Iteration" + de::toString(m_iteration+1), iterationDescription);
	const float								lineWidth				= getLineWidth();
	tcu::Surface							resultImage				(m_renderSize, m_renderSize);
	std::vector<tcu::Vec4>					drawBuffer;
	std::vector<tcu::Vec4>					colorBuffer;
	std::vector<LineSceneSpec::SceneLine>	lines;

	// supported?
	if (lineWidth <= m_maxLineWidth)
	{
		// generate scene
		generateVertices(m_iteration, drawBuffer, colorBuffer);
		extractLines(lines, drawBuffer, colorBuffer);

		// log
		{
			m_context.getTestContext().getLog() << tcu::TestLog::Message << "Generated vertices:" << tcu::TestLog::EndMessage;
			for (int vtxNdx = 0; vtxNdx < (int)drawBuffer.size(); ++vtxNdx)
				m_context.getTestContext().getLog() << tcu::TestLog::Message << "\t" << drawBuffer[vtxNdx] << ",\tcolor= " << colorBuffer[vtxNdx] << tcu::TestLog::EndMessage;
		}

		// draw image
		drawPrimitives(resultImage, drawBuffer, colorBuffer, m_primitiveTopology);

		// compare
		{
			RasterizationArguments	args;
			LineSceneSpec			scene;

			tcu::IVec4				colorBits = tcu::getTextureFormatBitDepth(getTextureFormat());

			args.numSamples		= m_multisampling ? 1 : 0;
			args.subpixelBits	= m_subpixelBits;
			args.redBits		= colorBits[0];
			args.greenBits		= colorBits[1];
			args.blueBits		= colorBits[2];

			scene.lines.swap(lines);
			scene.lineWidth = getLineWidth();

			switch (m_strictness)
			{
				case PRIMITIVESTRICTNESS_STRICT:
				{
					if (!verifyTriangulatedLineGroupInterpolation(resultImage, scene, args, m_context.getTestContext().getLog(), true))
						m_allIterationsPassed = false;

					break;
				}

				case PRIMITIVESTRICTNESS_NONSTRICT:
				case PRIMITIVESTRICTNESS_IGNORE:
				{
					if (!verifyTriangulatedLineGroupInterpolation(resultImage, scene, args, m_context.getTestContext().getLog(), false, true))
						m_allIterationsPassed = false;

					break;
				}

				default:
					TCU_THROW(InternalError, "Not implemented");
			}
		}
	}
	else
		m_context.getTestContext().getLog() << tcu::TestLog::Message << "Line width " << lineWidth << " not supported, skipping iteration." << tcu::TestLog::EndMessage;

	// result
	if (++m_iteration == m_iterationCount)
	{
		if (m_allIterationsPassed)
			return tcu::TestStatus::pass("Pass");
		else
			return tcu::TestStatus::fail("Incorrect rasterization");
	}
	else
		return tcu::TestStatus::incomplete();
}

void LineInterpolationTestInstance::generateVertices (int iteration, std::vector<tcu::Vec4>& outVertices, std::vector<tcu::Vec4>& outColors) const
{
	// use only red, green and blue
	const tcu::Vec4 colors[] =
	{
		tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f),
		tcu::Vec4(0.0f, 1.0f, 0.0f, 1.0f),
		tcu::Vec4(0.0f, 0.0f, 1.0f, 1.0f),
	};

	de::Random rnd(123 + iteration * 1000 + (int)m_primitiveTopology);

	outVertices.resize(6);
	outColors.resize(6);

	for (int vtxNdx = 0; vtxNdx < (int)outVertices.size(); ++vtxNdx)
	{
		outVertices[vtxNdx].x() = rnd.getFloat(-0.9f, 0.9f);
		outVertices[vtxNdx].y() = rnd.getFloat(-0.9f, 0.9f);
		outVertices[vtxNdx].z() = 0.0f;

		if (!m_projective)
			outVertices[vtxNdx].w() = 1.0f;
		else
		{
			const float w = rnd.getFloat(0.2f, 4.0f);

			outVertices[vtxNdx].x() *= w;
			outVertices[vtxNdx].y() *= w;
			outVertices[vtxNdx].z() *= w;
			outVertices[vtxNdx].w() = w;
		}

		outColors[vtxNdx] = colors[vtxNdx % DE_LENGTH_OF_ARRAY(colors)];
	}
}

void LineInterpolationTestInstance::extractLines (std::vector<LineSceneSpec::SceneLine>& outLines, const std::vector<tcu::Vec4>& vertices, const std::vector<tcu::Vec4>& colors) const
{
	switch (m_primitiveTopology)
	{
		case VK_PRIMITIVE_TOPOLOGY_LINE_LIST:
		{
			for (int vtxNdx = 0; vtxNdx < (int)vertices.size() - 1; vtxNdx += 2)
			{
				LineSceneSpec::SceneLine line;
				line.positions[0] = vertices[vtxNdx + 0];
				line.positions[1] = vertices[vtxNdx + 1];

				if (m_flatshade)
				{
					line.colors[0] = colors[vtxNdx];
					line.colors[1] = colors[vtxNdx];
				}
				else
				{
					line.colors[0] = colors[vtxNdx + 0];
					line.colors[1] = colors[vtxNdx + 1];
				}

				outLines.push_back(line);
			}
			break;
		}

		case VK_PRIMITIVE_TOPOLOGY_LINE_STRIP:
		{
			for (int vtxNdx = 0; vtxNdx < (int)vertices.size() - 1; ++vtxNdx)
			{
				LineSceneSpec::SceneLine line;
				line.positions[0] = vertices[vtxNdx + 0];
				line.positions[1] = vertices[vtxNdx + 1];

				if (m_flatshade)
				{
					line.colors[0] = colors[vtxNdx];
					line.colors[1] = colors[vtxNdx];
				}
				else
				{
					line.colors[0] = colors[vtxNdx + 0];
					line.colors[1] = colors[vtxNdx + 1];
				}

				outLines.push_back(line);
			}
			break;
		}

		default:
			DE_ASSERT(false);
	}
}

float LineInterpolationTestInstance::getLineWidth (void) const
{
	return m_lineWidths[m_iteration];
}

class LineInterpolationTestCase : public BaseRenderingTestCase
{
public:
								LineInterpolationTestCase		(tcu::TestContext&		context,
																 const std::string&		name,
																 const std::string&		description,
																 VkPrimitiveTopology	primitiveTopology,
																 int					flags,
																 PrimitiveWideness		wideness,
																 PrimitiveStrictness	strictness,
																 VkSampleCountFlagBits	sampleCount = VK_SAMPLE_COUNT_1_BIT)
									: BaseRenderingTestCase		(context, name, description, sampleCount, (flags & INTERPOLATIONFLAGS_FLATSHADE) != 0)
									, m_primitiveTopology		(primitiveTopology)
									, m_flags					(flags)
									, m_wideness				(wideness)
									, m_strictness				(strictness)
								{}

	virtual TestInstance*		createInstance					(Context& context) const
								{
									return new LineInterpolationTestInstance(context, m_primitiveTopology, m_flags, m_wideness, m_strictness, m_sampleCount);
								}

	virtual	void				checkSupport		(Context& context) const
								{
									if (m_strictness == PRIMITIVESTRICTNESS_STRICT &&
										!context.getDeviceProperties().limits.strictLines)
										TCU_THROW(NotSupportedError, "Strict rasterization is not supported");

									if (m_wideness == PRIMITIVEWIDENESS_WIDE)
										context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_WIDE_LINES);
								}
protected:
	const VkPrimitiveTopology	m_primitiveTopology;
	const int					m_flags;
	const PrimitiveWideness		m_wideness;
	const PrimitiveStrictness	m_strictness;
};

class StrideZeroCase : public vkt::TestCase
{
public:
	struct Params
	{
		std::vector<tcu::Vec2>	bufferData;
		deUint32				drawVertexCount;
	};

							StrideZeroCase		(tcu::TestContext& testCtx, const std::string& name, const std::string& description, const Params& params)
								: vkt::TestCase	(testCtx, name, description)
								, m_params		(params)
								{}

	virtual					~StrideZeroCase		(void) {}

	virtual void			initPrograms		(vk::SourceCollections& programCollection) const;
	virtual TestInstance*	createInstance		(Context& context) const;
	virtual void			checkSupport		(Context& context) const;

	static constexpr vk::VkFormat				kColorFormat	= vk::VK_FORMAT_R8G8B8A8_UNORM;
	static constexpr vk::VkFormatFeatureFlags	kColorFeatures	= (vk::VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT | vk::VK_FORMAT_FEATURE_TRANSFER_SRC_BIT);
	static constexpr vk::VkImageUsageFlags		kColorUsage		= (vk::VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | vk::VK_IMAGE_USAGE_TRANSFER_SRC_BIT);

	//	(-1,-1)
	//		+-----+-----+
	//		|     |     |
	//		|  a  |  b  |	a = (-0.5, -0.5)
	//		|     |     |	b = ( 0.5, -0.5)
	//		+-----------+	c = (-0.5,  0.5)
	//		|     |     |	d = ( 0.5,  0.5)
	//		|  c  |  d  |
	//		|     |     |
	//		+-----+-----+
	//					(1,1)
	static constexpr deUint32					kImageDim		= 2u;
	static const float							kCornerDelta;	// 0.5f;

	static const tcu::Vec4						kClearColor;
	static const tcu::Vec4						kDrawColor;

private:
	Params m_params;
};

const float		StrideZeroCase::kCornerDelta	= 0.5f;
const tcu::Vec4	StrideZeroCase::kClearColor		(0.0f, 0.0f, 0.0f, 1.0f);
const tcu::Vec4	StrideZeroCase::kDrawColor		(1.0f, 1.0f, 1.0f, 1.0f);

class StrideZeroInstance : public vkt::TestInstance
{
public:
								StrideZeroInstance	(Context& context, const StrideZeroCase::Params& params)
									: vkt::TestInstance	(context)
									, m_params			(params)
									{}

	virtual						~StrideZeroInstance	(void) {}

	virtual tcu::TestStatus		iterate				(void);

private:
	StrideZeroCase::Params m_params;
};

void StrideZeroCase::initPrograms (vk::SourceCollections& programCollection) const
{
	std::ostringstream vert;
	std::ostringstream frag;

	std::ostringstream drawColor;
	drawColor
		<< std::setprecision(2) << std::fixed
		<< "vec4(" << kDrawColor.x() << ", " << kDrawColor.y() << ", " << kDrawColor.z() << ", " << kDrawColor.w() << ")";

	vert
		<< "#version 450\n"
		<< "layout (location=0) in vec2 inPos;\n"
		<< "void main() {\n"
		<< "    gl_Position = vec4(inPos, 0.0, 1.0);\n"
		<< "    gl_PointSize = 1.0;\n"
		<< "}\n"
		;

	frag
		<< "#version 450\n"
		<< "layout (location=0) out vec4 outColor;\n"
		<< "void main() {\n"
		<< "    outColor = " << drawColor.str() << ";\n"
		<< "}\n"
		;

	programCollection.glslSources.add("vert") << glu::VertexSource(vert.str());
	programCollection.glslSources.add("frag") << glu::FragmentSource(frag.str());
}

TestInstance* StrideZeroCase::createInstance (Context& context) const
{
	return new StrideZeroInstance(context, m_params);
}

void StrideZeroCase::checkSupport (Context& context) const
{
	const auto properties = vk::getPhysicalDeviceFormatProperties(context.getInstanceInterface(), context.getPhysicalDevice(), kColorFormat);
	if ((properties.optimalTilingFeatures & kColorFeatures) != kColorFeatures)
		TCU_THROW(NotSupportedError, "Required image format not supported");
}

// Creates a vertex buffer with the given data but uses zero as the binding stride.
// Then, tries to draw the requested number of points. Only the first point should ever be used.
tcu::TestStatus StrideZeroInstance::iterate (void)
{
	const auto&		vkd			= m_context.getDeviceInterface();
	const auto		device		= m_context.getDevice();
	auto&			alloc		= m_context.getDefaultAllocator();
	const auto		queue		= m_context.getUniversalQueue();
	const auto		queueIndex	= m_context.getUniversalQueueFamilyIndex();
	constexpr auto	kImageDim	= StrideZeroCase::kImageDim;
	const auto		colorExtent	= vk::makeExtent3D(kImageDim, kImageDim, 1u);

	// Prepare vertex buffer.
	const auto					vertexBufferSize	= static_cast<vk::VkDeviceSize>(m_params.bufferData.size() * sizeof(decltype(m_params.bufferData)::value_type));
	const auto					vertexBufferInfo	= vk::makeBufferCreateInfo(vertexBufferSize, vk::VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
	const vk::BufferWithMemory	vertexBuffer(		vkd, device, alloc, vertexBufferInfo, vk::MemoryRequirement::HostVisible);
	auto&						vertexBufferAlloc	= vertexBuffer.getAllocation();
	const vk::VkDeviceSize		vertexBufferOffset	= 0ull;
	deMemcpy(vertexBufferAlloc.getHostPtr(), m_params.bufferData.data(), static_cast<size_t>(vertexBufferSize));
	flushAlloc(vkd, device, vertexBufferAlloc);

	// Prepare render image.
	const vk::VkImageCreateInfo colorAttachmentInfo =
	{
		vk::VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,	//	VkStructureType			sType;
		nullptr,									//	const void*				pNext;
		0u,											//	VkImageCreateFlags		flags;
		vk::VK_IMAGE_TYPE_2D,						//	VkImageType				imageType;
		StrideZeroCase::kColorFormat,				//	VkFormat				format;
		colorExtent,								//	VkExtent3D				extent;
		1u,											//	deUint32				mipLevels;
		1u,											//	deUint32				arrayLayers;
		vk::VK_SAMPLE_COUNT_1_BIT,					//	VkSampleCountFlagBits	samples;
		vk::VK_IMAGE_TILING_OPTIMAL,				//	VkImageTiling			tiling;
		StrideZeroCase::kColorUsage,				//	VkImageUsageFlags		usage;
		vk::VK_SHARING_MODE_EXCLUSIVE,				//	VkSharingMode			sharingMode;
		1u,											//	deUint32				queueFamilyIndexCount;
		&queueIndex,								//	const deUint32*			pQueueFamilyIndices;
		vk::VK_IMAGE_LAYOUT_UNDEFINED,				//	VkImageLayout			initialLayout;
	};
	const vk::ImageWithMemory colorAttachment(vkd, device, alloc, colorAttachmentInfo, vk::MemoryRequirement::Any);

	const auto colorSubresourceRange	= vk::makeImageSubresourceRange(vk::VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
	const auto colorAttachmentView		= vk::makeImageView(vkd, device, colorAttachment.get(), vk::VK_IMAGE_VIEW_TYPE_2D, StrideZeroCase::kColorFormat, colorSubresourceRange);

	const vk::VkVertexInputBindingDescription vertexBinding =
	{
		0u,									//	deUint32			binding;
		0u,									//	deUint32			stride;		[IMPORTANT]
		vk::VK_VERTEX_INPUT_RATE_VERTEX,	//	VkVertexInputRate	inputRate;
	};

	const vk::VkVertexInputAttributeDescription vertexAttribute =
	{
		0u,								//	deUint32	location;
		0u,								//	deUint32	binding;
		vk::VK_FORMAT_R32G32_SFLOAT,	//	VkFormat	format;
		0u,								//	deUint32	offset;
	};

	const vk::VkPipelineVertexInputStateCreateInfo vertexInput =
	{
		vk::VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,	//	VkStructureType								sType;
		nullptr,														//	const void*									pNext;
		0u,																//	VkPipelineVertexInputStateCreateFlags		flags;
		1u,																//	deUint32									vertexBindingDescriptionCount;
		&vertexBinding,													//	const VkVertexInputBindingDescription*		pVertexBindingDescriptions;
		1u,																//	deUint32									vertexAttributeDescriptionCount;
		&vertexAttribute,												//	const VkVertexInputAttributeDescription*	pVertexAttributeDescriptions;
	};

	const auto renderArea		= vk::makeRect2D(kImageDim, kImageDim);
	const auto viewports		= std::vector<vk::VkViewport>(1, vk::makeViewport(kImageDim, kImageDim));
	const auto scissors			= std::vector<vk::VkRect2D>(1, renderArea);
	const auto pipelineLayout	= vk::makePipelineLayout(vkd, device);
	const auto vertexShader		= vk::createShaderModule(vkd, device, m_context.getBinaryCollection().get("vert"), 0u);
	const auto fragmentShader	= vk::createShaderModule(vkd, device, m_context.getBinaryCollection().get("frag"), 0u);
	const auto renderPass		= vk::makeRenderPass(vkd, device, StrideZeroCase::kColorFormat);
	const auto graphicsPipeline	= vk::makeGraphicsPipeline(vkd, device, pipelineLayout.get(),
		vertexShader.get(), DE_NULL, DE_NULL, DE_NULL, fragmentShader.get(),					// Shaders.
		renderPass.get(), viewports, scissors, vk::VK_PRIMITIVE_TOPOLOGY_POINT_LIST, 0u, 0u,	// Render pass, viewports, scissors, topology.
		&vertexInput);																			// Vertex input state.
	const auto framebuffer		= vk::makeFramebuffer(vkd, device, renderPass.get(), colorAttachmentView.get(), kImageDim, kImageDim);

	const auto cmdPool		= vk::makeCommandPool(vkd, device, queueIndex);
	const auto cmdBufferPtr	= vk::allocateCommandBuffer(vkd, device, cmdPool.get(), vk::VK_COMMAND_BUFFER_LEVEL_PRIMARY);
	const auto cmdBuffer	= cmdBufferPtr.get();

	// Buffer used to verify results.
	const auto					tcuFormat			= vk::mapVkFormat(StrideZeroCase::kColorFormat);
	const auto					colorBufferSize		= static_cast<vk::VkDeviceSize>(tcu::getPixelSize(tcuFormat)) * kImageDim * kImageDim;
	const auto					colorBufferInfo		= vk::makeBufferCreateInfo(colorBufferSize, vk::VK_BUFFER_USAGE_TRANSFER_DST_BIT);
	const vk::BufferWithMemory	colorBuffer			(vkd, device, alloc, colorBufferInfo, vk::MemoryRequirement::HostVisible);
	auto&						colorBufferAlloc	= colorBuffer.getAllocation();
	void*						colorBufferPtr		= colorBufferAlloc.getHostPtr();
	const auto					colorLayers			= vk::makeImageSubresourceLayers(vk::VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u);
	const auto					copyRegion			= vk::makeBufferImageCopy(colorExtent, colorLayers);

	// Barriers from attachment to buffer and buffer to host.
	const auto colorAttachmentBarrier	= vk::makeImageMemoryBarrier	(vk::VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, vk::VK_ACCESS_TRANSFER_READ_BIT, vk::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, vk::VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, colorAttachment.get(), colorSubresourceRange);
	const auto colorBufferBarrier		= vk::makeBufferMemoryBarrier	(vk::VK_ACCESS_TRANSFER_WRITE_BIT, vk::VK_ACCESS_HOST_READ_BIT, colorBuffer.get(), 0ull, colorBufferSize);

	vk::beginCommandBuffer(vkd, cmdBuffer);
	vk::beginRenderPass(vkd, cmdBuffer, renderPass.get(), framebuffer.get(), renderArea, StrideZeroCase::kClearColor);
	vkd.cmdBindVertexBuffers(cmdBuffer, 0u, 1u, &vertexBuffer.get(), &vertexBufferOffset);
	vkd.cmdBindPipeline(cmdBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline.get());
	vkd.cmdDraw(cmdBuffer, m_params.drawVertexCount, 1u, 0u, 0u);
	vk::endRenderPass(vkd, cmdBuffer);
	vkd.cmdPipelineBarrier(cmdBuffer, vk::VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, vk::VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u, nullptr, 0u, nullptr, 1u, &colorAttachmentBarrier);
	vkd.cmdCopyImageToBuffer(cmdBuffer, colorAttachment.get(), vk::VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, colorBuffer.get(), 1u, &copyRegion);
	vkd.cmdPipelineBarrier(cmdBuffer, vk::VK_PIPELINE_STAGE_TRANSFER_BIT, vk::VK_PIPELINE_STAGE_HOST_BIT, 0u, 0u, nullptr, 1u, &colorBufferBarrier, 0u, nullptr);
	vk::endCommandBuffer(vkd, cmdBuffer);

	vk::submitCommandsAndWait(vkd, device, queue, cmdBuffer);

	// Invalidate color buffer alloc.
	vk::invalidateAlloc(vkd, device, colorBufferAlloc);

	// Check buffer.
	const int							imageDimI	= static_cast<int>(kImageDim);
	const tcu::ConstPixelBufferAccess	colorPixels	(tcuFormat, imageDimI, imageDimI, 1, colorBufferPtr);
	tcu::TestStatus						testStatus	= tcu::TestStatus::pass("Pass");
	auto&								log			= m_context.getTestContext().getLog();

	for (int x = 0; x < imageDimI; ++x)
	for (int y = 0; y < imageDimI; ++y)
	{
		// Only the top-left corner should have draw data.
		const auto expectedColor	= ((x == 0 && y == 0) ? StrideZeroCase::kDrawColor : StrideZeroCase::kClearColor);
		const auto imageColor		= colorPixels.getPixel(x, y);

		if (expectedColor != imageColor)
		{
			log
				<< tcu::TestLog::Message
				<< "Unexpected color found in pixel (" << x << ", " << y << "): "
				<< "expected (" << expectedColor.x() << ", " << expectedColor.y() << ", " << expectedColor.z() << ", " << expectedColor.w() << ") "
				<< "and found (" << imageColor.x() << ", " << imageColor.y() << ", " << imageColor.z() << ", " << imageColor.w() << ")"
				<< tcu::TestLog::EndMessage;

			testStatus = tcu::TestStatus::fail("Failed; Check log for details");
		}
	}

	return testStatus;
}

void createRasterizationTests (tcu::TestCaseGroup* rasterizationTests)
{
	tcu::TestContext&	testCtx		=	rasterizationTests->getTestContext();

	// .primitives
	{
		tcu::TestCaseGroup* const primitives = new tcu::TestCaseGroup(testCtx, "primitives", "Primitive rasterization");

		rasterizationTests->addChild(primitives);

		tcu::TestCaseGroup* const nostippleTests = new tcu::TestCaseGroup(testCtx, "no_stipple", "No stipple");
		tcu::TestCaseGroup* const stippleStaticTests = new tcu::TestCaseGroup(testCtx, "static_stipple", "Line stipple static");
		tcu::TestCaseGroup* const stippleDynamicTests = new tcu::TestCaseGroup(testCtx, "dynamic_stipple", "Line stipple dynamic");
		tcu::TestCaseGroup* const strideZeroTests = new tcu::TestCaseGroup(testCtx, "stride_zero", "Test input assembly with stride zero");

		primitives->addChild(nostippleTests);
		primitives->addChild(stippleStaticTests);
		primitives->addChild(stippleDynamicTests);
		primitives->addChild(strideZeroTests);

		// .stride_zero
		{
			{
				StrideZeroCase::Params params;
				params.bufferData.emplace_back(-StrideZeroCase::kCornerDelta, -StrideZeroCase::kCornerDelta);
				params.drawVertexCount = 1u;
				strideZeroTests->addChild(new StrideZeroCase(testCtx, "single_point", "Attempt to draw 1 point with stride 0", params));
			}
			{
				StrideZeroCase::Params params;
				params.bufferData.emplace_back(-StrideZeroCase::kCornerDelta, -StrideZeroCase::kCornerDelta);
				params.bufferData.emplace_back( StrideZeroCase::kCornerDelta, -StrideZeroCase::kCornerDelta);
				params.bufferData.emplace_back(-StrideZeroCase::kCornerDelta,  StrideZeroCase::kCornerDelta);
				params.bufferData.emplace_back( StrideZeroCase::kCornerDelta,  StrideZeroCase::kCornerDelta);
				params.drawVertexCount = static_cast<deUint32>(params.bufferData.size());
				strideZeroTests->addChild(new StrideZeroCase(testCtx, "four_points", "Attempt to draw 4 points with stride 0 and 4 points in the buffer", params));
			}
			{
				StrideZeroCase::Params params;
				params.bufferData.emplace_back(-StrideZeroCase::kCornerDelta, -StrideZeroCase::kCornerDelta);
				params.drawVertexCount = 100000u;
				strideZeroTests->addChild(new StrideZeroCase(testCtx, "many_points", "Attempt to draw many points with stride 0 with one point in the buffer", params));
			}
		}

		nostippleTests->addChild(new BaseTestCase<TrianglesTestInstance>		(testCtx, "triangles",			"Render primitives as VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, verify rasterization result"));
		nostippleTests->addChild(new BaseTestCase<TriangleStripTestInstance>	(testCtx, "triangle_strip",		"Render primitives as VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, verify rasterization result"));
		nostippleTests->addChild(new BaseTestCase<TriangleFanTestInstance>		(testCtx, "triangle_fan",		"Render primitives as VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN, verify rasterization result"));
		nostippleTests->addChild(new WidenessTestCase<PointTestInstance>		(testCtx, "points",				"Render primitives as VK_PRIMITIVE_TOPOLOGY_POINT_LIST, verify rasterization result",					PRIMITIVEWIDENESS_WIDE,		PRIMITIVESTRICTNESS_IGNORE, false, VK_SAMPLE_COUNT_1_BIT, LINESTIPPLE_DISABLED, VK_LINE_RASTERIZATION_MODE_DEFAULT_EXT));

		nostippleTests->addChild(new WidenessTestCase<LinesTestInstance>		(testCtx, "strict_lines",			"Render primitives as VK_PRIMITIVE_TOPOLOGY_LINE_LIST in strict mode, verify rasterization result",						PRIMITIVEWIDENESS_NARROW,	PRIMITIVESTRICTNESS_STRICT, true, VK_SAMPLE_COUNT_1_BIT, LINESTIPPLE_DISABLED, VK_LINE_RASTERIZATION_MODE_EXT_LAST));
		nostippleTests->addChild(new WidenessTestCase<LineStripTestInstance>	(testCtx, "strict_line_strip",		"Render primitives as VK_PRIMITIVE_TOPOLOGY_LINE_STRIP in strict mode, verify rasterization result",					PRIMITIVEWIDENESS_NARROW,	PRIMITIVESTRICTNESS_STRICT, true, VK_SAMPLE_COUNT_1_BIT, LINESTIPPLE_DISABLED, VK_LINE_RASTERIZATION_MODE_EXT_LAST));
		nostippleTests->addChild(new WidenessTestCase<LinesTestInstance>		(testCtx, "strict_lines_wide",		"Render primitives as VK_PRIMITIVE_TOPOLOGY_LINE_LIST in strict mode with wide lines, verify rasterization result",		PRIMITIVEWIDENESS_WIDE,		PRIMITIVESTRICTNESS_STRICT, true, VK_SAMPLE_COUNT_1_BIT, LINESTIPPLE_DISABLED, VK_LINE_RASTERIZATION_MODE_EXT_LAST));
		nostippleTests->addChild(new WidenessTestCase<LineStripTestInstance>	(testCtx, "strict_line_strip_wide",	"Render primitives as VK_PRIMITIVE_TOPOLOGY_LINE_STRIP in strict mode with wide lines, verify rasterization result",	PRIMITIVEWIDENESS_WIDE,		PRIMITIVESTRICTNESS_STRICT, true, VK_SAMPLE_COUNT_1_BIT, LINESTIPPLE_DISABLED, VK_LINE_RASTERIZATION_MODE_EXT_LAST));

		nostippleTests->addChild(new WidenessTestCase<LinesTestInstance>		(testCtx, "non_strict_lines",			"Render primitives as VK_PRIMITIVE_TOPOLOGY_LINE_LIST in nonstrict mode, verify rasterization result",					PRIMITIVEWIDENESS_NARROW,	PRIMITIVESTRICTNESS_NONSTRICT, true, VK_SAMPLE_COUNT_1_BIT, LINESTIPPLE_DISABLED, VK_LINE_RASTERIZATION_MODE_EXT_LAST));
		nostippleTests->addChild(new WidenessTestCase<LineStripTestInstance>	(testCtx, "non_strict_line_strip",		"Render primitives as VK_PRIMITIVE_TOPOLOGY_LINE_STRIP in nonstrict mode, verify rasterization result",					PRIMITIVEWIDENESS_NARROW,	PRIMITIVESTRICTNESS_NONSTRICT, true, VK_SAMPLE_COUNT_1_BIT, LINESTIPPLE_DISABLED, VK_LINE_RASTERIZATION_MODE_EXT_LAST));
		nostippleTests->addChild(new WidenessTestCase<LinesTestInstance>		(testCtx, "non_strict_lines_wide",		"Render primitives as VK_PRIMITIVE_TOPOLOGY_LINE_LIST in nonstrict mode with wide lines, verify rasterization result",	PRIMITIVEWIDENESS_WIDE,		PRIMITIVESTRICTNESS_NONSTRICT, true, VK_SAMPLE_COUNT_1_BIT, LINESTIPPLE_DISABLED, VK_LINE_RASTERIZATION_MODE_EXT_LAST));
		nostippleTests->addChild(new WidenessTestCase<LineStripTestInstance>	(testCtx, "non_strict_line_strip_wide",	"Render primitives as VK_PRIMITIVE_TOPOLOGY_LINE_STRIP in nonstrict mode with wide lines, verify rasterization result",	PRIMITIVEWIDENESS_WIDE,		PRIMITIVESTRICTNESS_NONSTRICT, true, VK_SAMPLE_COUNT_1_BIT, LINESTIPPLE_DISABLED, VK_LINE_RASTERIZATION_MODE_EXT_LAST));

		for (int i = 0; i < 3; ++i) {

			tcu::TestCaseGroup *g = i == 2 ? stippleDynamicTests : i == 1 ? stippleStaticTests : nostippleTests;

			LineStipple stipple = (LineStipple)i;

			g->addChild(new WidenessTestCase<LinesTestInstance>		(testCtx, "lines",						"Render primitives as VK_PRIMITIVE_TOPOLOGY_LINE_LIST, verify rasterization result",						PRIMITIVEWIDENESS_NARROW,	PRIMITIVESTRICTNESS_IGNORE, true, VK_SAMPLE_COUNT_1_BIT, stipple, VK_LINE_RASTERIZATION_MODE_DEFAULT_EXT, i == 0 ? RESOLUTION_NPOT : 0));
			g->addChild(new WidenessTestCase<LineStripTestInstance>	(testCtx, "line_strip",					"Render primitives as VK_PRIMITIVE_TOPOLOGY_LINE_STRIP, verify rasterization result",						PRIMITIVEWIDENESS_NARROW,	PRIMITIVESTRICTNESS_IGNORE, true, VK_SAMPLE_COUNT_1_BIT, stipple, VK_LINE_RASTERIZATION_MODE_DEFAULT_EXT));
			g->addChild(new WidenessTestCase<LinesTestInstance>		(testCtx, "lines_wide",					"Render primitives as VK_PRIMITIVE_TOPOLOGY_LINE_LIST with wide lines, verify rasterization result",		PRIMITIVEWIDENESS_WIDE,		PRIMITIVESTRICTNESS_IGNORE, true, VK_SAMPLE_COUNT_1_BIT, stipple, VK_LINE_RASTERIZATION_MODE_DEFAULT_EXT));
			g->addChild(new WidenessTestCase<LineStripTestInstance>	(testCtx, "line_strip_wide",			"Render primitives as VK_PRIMITIVE_TOPOLOGY_LINE_STRIP with wide lines, verify rasterization result",		PRIMITIVEWIDENESS_WIDE,		PRIMITIVESTRICTNESS_IGNORE, true, VK_SAMPLE_COUNT_1_BIT, stipple, VK_LINE_RASTERIZATION_MODE_DEFAULT_EXT));

			g->addChild(new WidenessTestCase<LinesTestInstance>		(testCtx, "rectangular_lines",			"Render primitives as VK_PRIMITIVE_TOPOLOGY_LINE_LIST, verify rasterization result",						PRIMITIVEWIDENESS_NARROW,	PRIMITIVESTRICTNESS_IGNORE, true, VK_SAMPLE_COUNT_1_BIT, stipple, VK_LINE_RASTERIZATION_MODE_RECTANGULAR_EXT));
			g->addChild(new WidenessTestCase<LineStripTestInstance>	(testCtx, "rectangular_line_strip",		"Render primitives as VK_PRIMITIVE_TOPOLOGY_LINE_STRIP, verify rasterization result",						PRIMITIVEWIDENESS_NARROW,	PRIMITIVESTRICTNESS_IGNORE, true, VK_SAMPLE_COUNT_1_BIT, stipple, VK_LINE_RASTERIZATION_MODE_RECTANGULAR_EXT));
			g->addChild(new WidenessTestCase<LinesTestInstance>		(testCtx, "rectangular_lines_wide",		"Render primitives as VK_PRIMITIVE_TOPOLOGY_LINE_LIST with wide lines, verify rasterization result",		PRIMITIVEWIDENESS_WIDE,		PRIMITIVESTRICTNESS_IGNORE, true, VK_SAMPLE_COUNT_1_BIT, stipple, VK_LINE_RASTERIZATION_MODE_RECTANGULAR_EXT));
			g->addChild(new WidenessTestCase<LineStripTestInstance>	(testCtx, "rectangular_line_strip_wide","Render primitives as VK_PRIMITIVE_TOPOLOGY_LINE_STRIP with wide lines, verify rasterization result",		PRIMITIVEWIDENESS_WIDE,		PRIMITIVESTRICTNESS_IGNORE, true, VK_SAMPLE_COUNT_1_BIT, stipple, VK_LINE_RASTERIZATION_MODE_RECTANGULAR_EXT));

			g->addChild(new WidenessTestCase<LinesTestInstance>		(testCtx, "bresenham_lines",			"Render primitives as VK_PRIMITIVE_TOPOLOGY_LINE_LIST, verify rasterization result",						PRIMITIVEWIDENESS_NARROW,	PRIMITIVESTRICTNESS_IGNORE, true, VK_SAMPLE_COUNT_1_BIT, stipple, VK_LINE_RASTERIZATION_MODE_BRESENHAM_EXT));
			g->addChild(new WidenessTestCase<LineStripTestInstance>	(testCtx, "bresenham_line_strip",		"Render primitives as VK_PRIMITIVE_TOPOLOGY_LINE_STRIP, verify rasterization result",						PRIMITIVEWIDENESS_NARROW,	PRIMITIVESTRICTNESS_IGNORE, true, VK_SAMPLE_COUNT_1_BIT, stipple, VK_LINE_RASTERIZATION_MODE_BRESENHAM_EXT));
			g->addChild(new WidenessTestCase<LinesTestInstance>		(testCtx, "bresenham_lines_wide",		"Render primitives as VK_PRIMITIVE_TOPOLOGY_LINE_LIST with wide lines, verify rasterization result",		PRIMITIVEWIDENESS_WIDE,		PRIMITIVESTRICTNESS_IGNORE, true, VK_SAMPLE_COUNT_1_BIT, stipple, VK_LINE_RASTERIZATION_MODE_BRESENHAM_EXT));
			g->addChild(new WidenessTestCase<LineStripTestInstance>	(testCtx, "bresenham_line_strip_wide",	"Render primitives as VK_PRIMITIVE_TOPOLOGY_LINE_STRIP with wide lines, verify rasterization result",		PRIMITIVEWIDENESS_WIDE,		PRIMITIVESTRICTNESS_IGNORE, true, VK_SAMPLE_COUNT_1_BIT, stipple, VK_LINE_RASTERIZATION_MODE_BRESENHAM_EXT));

			g->addChild(new WidenessTestCase<LinesTestInstance>		(testCtx, "smooth_lines",				"Render primitives as VK_PRIMITIVE_TOPOLOGY_LINE_LIST, verify rasterization result",						PRIMITIVEWIDENESS_NARROW,	PRIMITIVESTRICTNESS_IGNORE, true, VK_SAMPLE_COUNT_1_BIT, stipple, VK_LINE_RASTERIZATION_MODE_RECTANGULAR_SMOOTH_EXT));
			g->addChild(new WidenessTestCase<LineStripTestInstance>	(testCtx, "smooth_line_strip",			"Render primitives as VK_PRIMITIVE_TOPOLOGY_LINE_STRIP, verify rasterization result",						PRIMITIVEWIDENESS_NARROW,	PRIMITIVESTRICTNESS_IGNORE, true, VK_SAMPLE_COUNT_1_BIT, stipple, VK_LINE_RASTERIZATION_MODE_RECTANGULAR_SMOOTH_EXT));
			g->addChild(new WidenessTestCase<LinesTestInstance>		(testCtx, "smooth_lines_wide",			"Render primitives as VK_PRIMITIVE_TOPOLOGY_LINE_LIST with wide lines, verify rasterization result",		PRIMITIVEWIDENESS_WIDE,		PRIMITIVESTRICTNESS_IGNORE, true, VK_SAMPLE_COUNT_1_BIT, stipple, VK_LINE_RASTERIZATION_MODE_RECTANGULAR_SMOOTH_EXT));
			g->addChild(new WidenessTestCase<LineStripTestInstance>	(testCtx, "smooth_line_strip_wide",		"Render primitives as VK_PRIMITIVE_TOPOLOGY_LINE_STRIP with wide lines, verify rasterization result",		PRIMITIVEWIDENESS_WIDE,		PRIMITIVESTRICTNESS_IGNORE, true, VK_SAMPLE_COUNT_1_BIT, stipple, VK_LINE_RASTERIZATION_MODE_RECTANGULAR_SMOOTH_EXT));
		}
	}

	// .primitive_size
	{
		tcu::TestCaseGroup* const primitiveSize = new tcu::TestCaseGroup(testCtx, "primitive_size", "Primitive size");
		rasterizationTests->addChild(primitiveSize);

		// .points
		{
			tcu::TestCaseGroup* const points = new tcu::TestCaseGroup(testCtx, "points", "Point size");

			static const struct TestCombinations
			{
				const deUint32	renderSize;
				const float		pointSize;
			} testCombinations[] =
			{
				{ 1024,		128.0f		},
				{ 1024,		256.0f		},
				{ 1024,		512.0f		},
				{ 2048,		1024.0f		},
				{ 4096,		2048.0f		},
				{ 8192,		4096.0f		},
				{ 9216,		8192.0f		},
				{ 10240,	10000.0f	}
			};

			for (size_t testCombNdx = 0; testCombNdx < DE_LENGTH_OF_ARRAY(testCombinations); testCombNdx++)
			{
				std::string	testCaseName	= "point_size_" + de::toString(testCombinations[testCombNdx].pointSize);
				deUint32	renderSize		= testCombinations[testCombNdx].renderSize;
				float		pointSize		= testCombinations[testCombNdx].pointSize;

				points->addChild(new PointSizeTestCase<PointSizeTestInstance>	(testCtx, testCaseName,	testCaseName, renderSize, pointSize));
			}

			primitiveSize->addChild(points);
		}
	}

	// .fill_rules
	{
		tcu::TestCaseGroup* const fillRules = new tcu::TestCaseGroup(testCtx, "fill_rules", "Primitive fill rules");

		rasterizationTests->addChild(fillRules);

		fillRules->addChild(new FillRuleTestCase(testCtx,	"basic_quad",			"Verify fill rules",	FillRuleTestInstance::FILLRULECASE_BASIC));
		fillRules->addChild(new FillRuleTestCase(testCtx,	"basic_quad_reverse",	"Verify fill rules",	FillRuleTestInstance::FILLRULECASE_REVERSED));
		fillRules->addChild(new FillRuleTestCase(testCtx,	"clipped_full",			"Verify fill rules",	FillRuleTestInstance::FILLRULECASE_CLIPPED_FULL));
		fillRules->addChild(new FillRuleTestCase(testCtx,	"clipped_partly",		"Verify fill rules",	FillRuleTestInstance::FILLRULECASE_CLIPPED_PARTIAL));
		fillRules->addChild(new FillRuleTestCase(testCtx,	"projected",			"Verify fill rules",	FillRuleTestInstance::FILLRULECASE_PROJECTED));
	}

	// .culling
	{
		static const struct CullMode
		{
			VkCullModeFlags	mode;
			const char*		prefix;
		} cullModes[] =
		{
			{ VK_CULL_MODE_FRONT_BIT,				"front_"	},
			{ VK_CULL_MODE_BACK_BIT,				"back_"		},
			{ VK_CULL_MODE_FRONT_AND_BACK,			"both_"		},
		};
		static const struct PrimitiveType
		{
			VkPrimitiveTopology	type;
			const char*			name;
		} primitiveTypes[] =
		{
			{ VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,			"triangles"			},
			{ VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,			"triangle_strip"	},
			{ VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN,			"triangle_fan"		},
		};
		static const struct FrontFaceOrder
		{
			VkFrontFace	mode;
			const char*	postfix;
		} frontOrders[] =
		{
			{ VK_FRONT_FACE_COUNTER_CLOCKWISE,	""			},
			{ VK_FRONT_FACE_CLOCKWISE,			"_reverse"	},
		};

		static const struct PolygonMode
		{
			VkPolygonMode	mode;
			const char*		name;
		} polygonModes[] =
		{
			{ VK_POLYGON_MODE_FILL,		""		},
			{ VK_POLYGON_MODE_LINE,		"_line"		},
			{ VK_POLYGON_MODE_POINT,	"_point"	}
		};

		tcu::TestCaseGroup* const culling = new tcu::TestCaseGroup(testCtx, "culling", "Culling");

		rasterizationTests->addChild(culling);

		for (int cullModeNdx	= 0; cullModeNdx	< DE_LENGTH_OF_ARRAY(cullModes);		++cullModeNdx)
		for (int primitiveNdx	= 0; primitiveNdx	< DE_LENGTH_OF_ARRAY(primitiveTypes);	++primitiveNdx)
		for (int frontOrderNdx	= 0; frontOrderNdx	< DE_LENGTH_OF_ARRAY(frontOrders);		++frontOrderNdx)
		for (int polygonModeNdx = 0; polygonModeNdx	< DE_LENGTH_OF_ARRAY(polygonModes);		++polygonModeNdx)
		{
			if (!(cullModes[cullModeNdx].mode == VK_CULL_MODE_FRONT_AND_BACK && polygonModes[polygonModeNdx].mode != VK_POLYGON_MODE_FILL))
			{
				const std::string name = std::string(cullModes[cullModeNdx].prefix) + primitiveTypes[primitiveNdx].name + frontOrders[frontOrderNdx].postfix + polygonModes[polygonModeNdx].name;
				culling->addChild(new CullingTestCase(testCtx, name, "Test primitive culling.", cullModes[cullModeNdx].mode, primitiveTypes[primitiveNdx].type, frontOrders[frontOrderNdx].mode, polygonModes[polygonModeNdx].mode));
			}
		}
	}

	// .discard
	{
		static const struct PrimitiveType
		{
			VkPrimitiveTopology	type;
			const char*			name;
		} primitiveTypes[] =
		{
			{ VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,	"triangle_list"		},
			{ VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,	"triangle_strip"	},
			{ VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN,	"triangle_fan"		},
			{ VK_PRIMITIVE_TOPOLOGY_LINE_LIST,		"line_list"			},
			{ VK_PRIMITIVE_TOPOLOGY_LINE_STRIP,		"line_strip"		},
			{ VK_PRIMITIVE_TOPOLOGY_POINT_LIST,		"point_list"		}
		};

		static const struct queryPipeline
		{
			deBool			useQuery;
			const char*		name;
		} queryPipeline[] =
		{
			{ DE_FALSE,	"query_pipeline_false"	},
			{ DE_TRUE,	"query_pipeline_true"	},
		};

		tcu::TestCaseGroup* const discard = new tcu::TestCaseGroup(testCtx, "discard", "Rasterizer discard");

		for (int primitiveNdx = 0; primitiveNdx < DE_LENGTH_OF_ARRAY(primitiveTypes); ++primitiveNdx)
		{
			tcu::TestCaseGroup* const primitive = new tcu::TestCaseGroup(testCtx, primitiveTypes[primitiveNdx].name, "Rasterizer discard");

			for (int useQueryNdx = 0; useQueryNdx < DE_LENGTH_OF_ARRAY(queryPipeline); useQueryNdx++)
			{
				const std::string name = std::string(queryPipeline[useQueryNdx].name);

				primitive->addChild(new DiscardTestCase(testCtx, name, "Test primitive discarding.", primitiveTypes[primitiveNdx].type, queryPipeline[useQueryNdx].useQuery));
			}

			discard->addChild(primitive);
		}

		rasterizationTests->addChild(discard);
	}

	// .interpolation
	{
		tcu::TestCaseGroup* const interpolation = new tcu::TestCaseGroup(testCtx, "interpolation", "Test interpolation");

		rasterizationTests->addChild(interpolation);

		// .basic
		{
			tcu::TestCaseGroup* const basic = new tcu::TestCaseGroup(testCtx, "basic", "Non-projective interpolation");

			interpolation->addChild(basic);

			basic->addChild(new TriangleInterpolationTestCase		(testCtx, "triangles",		"Verify triangle interpolation",		VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,	INTERPOLATIONFLAGS_NONE));
			basic->addChild(new TriangleInterpolationTestCase		(testCtx, "triangle_strip",	"Verify triangle strip interpolation",	VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,	INTERPOLATIONFLAGS_NONE));
			basic->addChild(new TriangleInterpolationTestCase		(testCtx, "triangle_fan",	"Verify triangle fan interpolation",	VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN,		INTERPOLATIONFLAGS_NONE));
			basic->addChild(new LineInterpolationTestCase			(testCtx, "lines",			"Verify line interpolation",			VK_PRIMITIVE_TOPOLOGY_LINE_LIST,		INTERPOLATIONFLAGS_NONE,	PRIMITIVEWIDENESS_NARROW,	PRIMITIVESTRICTNESS_IGNORE));
			basic->addChild(new LineInterpolationTestCase			(testCtx, "line_strip",		"Verify line strip interpolation",		VK_PRIMITIVE_TOPOLOGY_LINE_STRIP,		INTERPOLATIONFLAGS_NONE,	PRIMITIVEWIDENESS_NARROW,	PRIMITIVESTRICTNESS_IGNORE));
			basic->addChild(new LineInterpolationTestCase			(testCtx, "lines_wide",		"Verify wide line interpolation",		VK_PRIMITIVE_TOPOLOGY_LINE_LIST,		INTERPOLATIONFLAGS_NONE,	PRIMITIVEWIDENESS_WIDE,		PRIMITIVESTRICTNESS_IGNORE));
			basic->addChild(new LineInterpolationTestCase			(testCtx, "line_strip_wide","Verify wide line strip interpolation",	VK_PRIMITIVE_TOPOLOGY_LINE_STRIP,		INTERPOLATIONFLAGS_NONE,	PRIMITIVEWIDENESS_WIDE,		PRIMITIVESTRICTNESS_IGNORE));

			basic->addChild(new LineInterpolationTestCase			(testCtx, "strict_lines",			"Verify strict line interpolation",				VK_PRIMITIVE_TOPOLOGY_LINE_LIST,	INTERPOLATIONFLAGS_NONE,	PRIMITIVEWIDENESS_NARROW,	PRIMITIVESTRICTNESS_STRICT));
			basic->addChild(new LineInterpolationTestCase			(testCtx, "strict_line_strip",		"Verify strict line strip interpolation",		VK_PRIMITIVE_TOPOLOGY_LINE_STRIP,	INTERPOLATIONFLAGS_NONE,	PRIMITIVEWIDENESS_NARROW,	PRIMITIVESTRICTNESS_STRICT));
			basic->addChild(new LineInterpolationTestCase			(testCtx, "strict_lines_wide",		"Verify strict wide line interpolation",		VK_PRIMITIVE_TOPOLOGY_LINE_LIST,	INTERPOLATIONFLAGS_NONE,	PRIMITIVEWIDENESS_WIDE,		PRIMITIVESTRICTNESS_STRICT));
			basic->addChild(new LineInterpolationTestCase			(testCtx, "strict_line_strip_wide",	"Verify strict wide line strip interpolation",	VK_PRIMITIVE_TOPOLOGY_LINE_STRIP,	INTERPOLATIONFLAGS_NONE,	PRIMITIVEWIDENESS_WIDE,		PRIMITIVESTRICTNESS_STRICT));

			basic->addChild(new LineInterpolationTestCase			(testCtx, "non_strict_lines",			"Verify non-strict line interpolation",				VK_PRIMITIVE_TOPOLOGY_LINE_LIST,	INTERPOLATIONFLAGS_NONE,	PRIMITIVEWIDENESS_NARROW,	PRIMITIVESTRICTNESS_NONSTRICT));
			basic->addChild(new LineInterpolationTestCase			(testCtx, "non_strict_line_strip",		"Verify non-strict line strip interpolation",		VK_PRIMITIVE_TOPOLOGY_LINE_STRIP,	INTERPOLATIONFLAGS_NONE,	PRIMITIVEWIDENESS_NARROW,	PRIMITIVESTRICTNESS_NONSTRICT));
			basic->addChild(new LineInterpolationTestCase			(testCtx, "non_strict_lines_wide",		"Verify non-strict wide line interpolation",		VK_PRIMITIVE_TOPOLOGY_LINE_LIST,	INTERPOLATIONFLAGS_NONE,	PRIMITIVEWIDENESS_WIDE,		PRIMITIVESTRICTNESS_NONSTRICT));
			basic->addChild(new LineInterpolationTestCase			(testCtx, "non_strict_line_strip_wide",	"Verify non-strict wide line strip interpolation",	VK_PRIMITIVE_TOPOLOGY_LINE_STRIP,	INTERPOLATIONFLAGS_NONE,	PRIMITIVEWIDENESS_WIDE,		PRIMITIVESTRICTNESS_NONSTRICT));
		}

		// .projected
		{
			tcu::TestCaseGroup* const projected = new tcu::TestCaseGroup(testCtx, "projected", "Projective interpolation");

			interpolation->addChild(projected);

			projected->addChild(new TriangleInterpolationTestCase	(testCtx, "triangles",		"Verify triangle interpolation",		VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,	INTERPOLATIONFLAGS_PROJECTED));
			projected->addChild(new TriangleInterpolationTestCase	(testCtx, "triangle_strip",	"Verify triangle strip interpolation",	VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,	INTERPOLATIONFLAGS_PROJECTED));
			projected->addChild(new TriangleInterpolationTestCase	(testCtx, "triangle_fan",	"Verify triangle fan interpolation",	VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN,		INTERPOLATIONFLAGS_PROJECTED));
			projected->addChild(new LineInterpolationTestCase		(testCtx, "lines",			"Verify line interpolation",			VK_PRIMITIVE_TOPOLOGY_LINE_LIST,		INTERPOLATIONFLAGS_PROJECTED,	PRIMITIVEWIDENESS_NARROW,	PRIMITIVESTRICTNESS_IGNORE));
			projected->addChild(new LineInterpolationTestCase		(testCtx, "line_strip",		"Verify line strip interpolation",		VK_PRIMITIVE_TOPOLOGY_LINE_STRIP,		INTERPOLATIONFLAGS_PROJECTED,	PRIMITIVEWIDENESS_NARROW,	PRIMITIVESTRICTNESS_IGNORE));
			projected->addChild(new LineInterpolationTestCase		(testCtx, "lines_wide",		"Verify wide line interpolation",		VK_PRIMITIVE_TOPOLOGY_LINE_LIST,		INTERPOLATIONFLAGS_PROJECTED,	PRIMITIVEWIDENESS_WIDE,		PRIMITIVESTRICTNESS_IGNORE));
			projected->addChild(new LineInterpolationTestCase		(testCtx, "line_strip_wide","Verify wide line strip interpolation",	VK_PRIMITIVE_TOPOLOGY_LINE_STRIP,		INTERPOLATIONFLAGS_PROJECTED,	PRIMITIVEWIDENESS_WIDE,		PRIMITIVESTRICTNESS_IGNORE));

			projected->addChild(new LineInterpolationTestCase		(testCtx, "strict_lines",			"Verify strict line interpolation",				VK_PRIMITIVE_TOPOLOGY_LINE_LIST,		INTERPOLATIONFLAGS_PROJECTED,	PRIMITIVEWIDENESS_NARROW,	PRIMITIVESTRICTNESS_STRICT));
			projected->addChild(new LineInterpolationTestCase		(testCtx, "strict_line_strip",		"Verify strict line strip interpolation",		VK_PRIMITIVE_TOPOLOGY_LINE_STRIP,		INTERPOLATIONFLAGS_PROJECTED,	PRIMITIVEWIDENESS_NARROW,	PRIMITIVESTRICTNESS_STRICT));
			projected->addChild(new LineInterpolationTestCase		(testCtx, "strict_lines_wide",		"Verify strict wide line interpolation",		VK_PRIMITIVE_TOPOLOGY_LINE_LIST,		INTERPOLATIONFLAGS_PROJECTED,	PRIMITIVEWIDENESS_WIDE,		PRIMITIVESTRICTNESS_STRICT));
			projected->addChild(new LineInterpolationTestCase		(testCtx, "strict_line_strip_wide",	"Verify strict wide line strip interpolation",	VK_PRIMITIVE_TOPOLOGY_LINE_STRIP,		INTERPOLATIONFLAGS_PROJECTED,	PRIMITIVEWIDENESS_WIDE,		PRIMITIVESTRICTNESS_STRICT));

			projected->addChild(new LineInterpolationTestCase		(testCtx, "non_strict_lines",			"Verify non-strict line interpolation",				VK_PRIMITIVE_TOPOLOGY_LINE_LIST,		INTERPOLATIONFLAGS_PROJECTED,	PRIMITIVEWIDENESS_NARROW,	PRIMITIVESTRICTNESS_NONSTRICT));
			projected->addChild(new LineInterpolationTestCase		(testCtx, "non_strict_line_strip",		"Verify non-strict line strip interpolation",		VK_PRIMITIVE_TOPOLOGY_LINE_STRIP,		INTERPOLATIONFLAGS_PROJECTED,	PRIMITIVEWIDENESS_NARROW,	PRIMITIVESTRICTNESS_NONSTRICT));
			projected->addChild(new LineInterpolationTestCase		(testCtx, "non_strict_lines_wide",		"Verify non-strict wide line interpolation",		VK_PRIMITIVE_TOPOLOGY_LINE_LIST,		INTERPOLATIONFLAGS_PROJECTED,	PRIMITIVEWIDENESS_WIDE,		PRIMITIVESTRICTNESS_NONSTRICT));
			projected->addChild(new LineInterpolationTestCase		(testCtx, "non_strict_line_strip_wide",	"Verify non-strict wide line strip interpolation",	VK_PRIMITIVE_TOPOLOGY_LINE_STRIP,		INTERPOLATIONFLAGS_PROJECTED,	PRIMITIVEWIDENESS_WIDE,		PRIMITIVESTRICTNESS_NONSTRICT));
		}
	}

	// .flatshading
	{
		tcu::TestCaseGroup* const flatshading = new tcu::TestCaseGroup(testCtx, "flatshading", "Test flatshading");

		rasterizationTests->addChild(flatshading);

		flatshading->addChild(new TriangleInterpolationTestCase		(testCtx, "triangles",		"Verify triangle flatshading",			VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,	INTERPOLATIONFLAGS_FLATSHADE));
		flatshading->addChild(new TriangleInterpolationTestCase		(testCtx, "triangle_strip",	"Verify triangle strip flatshading",	VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,	INTERPOLATIONFLAGS_FLATSHADE));
		flatshading->addChild(new TriangleInterpolationTestCase		(testCtx, "triangle_fan",	"Verify triangle fan flatshading",		VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN,		INTERPOLATIONFLAGS_FLATSHADE));
		flatshading->addChild(new LineInterpolationTestCase			(testCtx, "lines",			"Verify line flatshading",				VK_PRIMITIVE_TOPOLOGY_LINE_LIST,		INTERPOLATIONFLAGS_FLATSHADE,	PRIMITIVEWIDENESS_NARROW,	PRIMITIVESTRICTNESS_IGNORE));
		flatshading->addChild(new LineInterpolationTestCase			(testCtx, "line_strip",		"Verify line strip flatshading",		VK_PRIMITIVE_TOPOLOGY_LINE_STRIP,		INTERPOLATIONFLAGS_FLATSHADE,	PRIMITIVEWIDENESS_NARROW,	PRIMITIVESTRICTNESS_IGNORE));
		flatshading->addChild(new LineInterpolationTestCase			(testCtx, "lines_wide",		"Verify wide line flatshading",			VK_PRIMITIVE_TOPOLOGY_LINE_LIST,		INTERPOLATIONFLAGS_FLATSHADE,	PRIMITIVEWIDENESS_WIDE,		PRIMITIVESTRICTNESS_IGNORE));
		flatshading->addChild(new LineInterpolationTestCase			(testCtx, "line_strip_wide","Verify wide line strip flatshading",	VK_PRIMITIVE_TOPOLOGY_LINE_STRIP,		INTERPOLATIONFLAGS_FLATSHADE,	PRIMITIVEWIDENESS_WIDE,		PRIMITIVESTRICTNESS_IGNORE));

		flatshading->addChild(new LineInterpolationTestCase			(testCtx, "strict_lines",			"Verify strict line flatshading",				VK_PRIMITIVE_TOPOLOGY_LINE_LIST,		INTERPOLATIONFLAGS_FLATSHADE,	PRIMITIVEWIDENESS_NARROW,	PRIMITIVESTRICTNESS_STRICT));
		flatshading->addChild(new LineInterpolationTestCase			(testCtx, "strict_line_strip",		"Verify strict line strip flatshading",			VK_PRIMITIVE_TOPOLOGY_LINE_STRIP,		INTERPOLATIONFLAGS_FLATSHADE,	PRIMITIVEWIDENESS_NARROW,	PRIMITIVESTRICTNESS_STRICT));
		flatshading->addChild(new LineInterpolationTestCase			(testCtx, "strict_lines_wide",		"Verify strict wide line flatshading",			VK_PRIMITIVE_TOPOLOGY_LINE_LIST,		INTERPOLATIONFLAGS_FLATSHADE,	PRIMITIVEWIDENESS_WIDE,		PRIMITIVESTRICTNESS_STRICT));
		flatshading->addChild(new LineInterpolationTestCase			(testCtx, "strict_line_strip_wide",	"Verify strict wide line strip flatshading",	VK_PRIMITIVE_TOPOLOGY_LINE_STRIP,		INTERPOLATIONFLAGS_FLATSHADE,	PRIMITIVEWIDENESS_WIDE,		PRIMITIVESTRICTNESS_STRICT));

		flatshading->addChild(new LineInterpolationTestCase			(testCtx, "non_strict_lines",			"Verify non-strict line flatshading",				VK_PRIMITIVE_TOPOLOGY_LINE_LIST,		INTERPOLATIONFLAGS_FLATSHADE,	PRIMITIVEWIDENESS_NARROW,	PRIMITIVESTRICTNESS_NONSTRICT));
		flatshading->addChild(new LineInterpolationTestCase			(testCtx, "non_strict_line_strip",		"Verify non-strict line strip flatshading",			VK_PRIMITIVE_TOPOLOGY_LINE_STRIP,		INTERPOLATIONFLAGS_FLATSHADE,	PRIMITIVEWIDENESS_NARROW,	PRIMITIVESTRICTNESS_NONSTRICT));
		flatshading->addChild(new LineInterpolationTestCase			(testCtx, "non_strict_lines_wide",		"Verify non-strict wide line flatshading",			VK_PRIMITIVE_TOPOLOGY_LINE_LIST,		INTERPOLATIONFLAGS_FLATSHADE,	PRIMITIVEWIDENESS_WIDE,		PRIMITIVESTRICTNESS_NONSTRICT));
		flatshading->addChild(new LineInterpolationTestCase			(testCtx, "non_strict_line_strip_wide",	"Verify non-strict wide line strip flatshading",	VK_PRIMITIVE_TOPOLOGY_LINE_STRIP,		INTERPOLATIONFLAGS_FLATSHADE,	PRIMITIVEWIDENESS_WIDE,		PRIMITIVESTRICTNESS_NONSTRICT));
	}

	const VkSampleCountFlagBits samples[] =
	{
		VK_SAMPLE_COUNT_2_BIT,
		VK_SAMPLE_COUNT_4_BIT,
		VK_SAMPLE_COUNT_8_BIT,
		VK_SAMPLE_COUNT_16_BIT,
		VK_SAMPLE_COUNT_32_BIT,
		VK_SAMPLE_COUNT_64_BIT
	};

	for (int samplesNdx = 0; samplesNdx < DE_LENGTH_OF_ARRAY(samples); samplesNdx++)
	{
		std::ostringstream caseName;

		caseName << "_multisample_" << (2 << samplesNdx) << "_bit";

		// .primitives
		{
			tcu::TestCaseGroup* const primitives = new tcu::TestCaseGroup(testCtx, ("primitives" + caseName.str()).c_str(), "Primitive rasterization");

			rasterizationTests->addChild(primitives);

			tcu::TestCaseGroup* const nostippleTests = new tcu::TestCaseGroup(testCtx, "no_stipple", "No stipple");
			tcu::TestCaseGroup* const stippleStaticTests = new tcu::TestCaseGroup(testCtx, "static_stipple", "Line stipple static");
			tcu::TestCaseGroup* const stippleDynamicTests = new tcu::TestCaseGroup(testCtx, "dynamic_stipple", "Line stipple dynamic");

			primitives->addChild(nostippleTests);
			primitives->addChild(stippleStaticTests);
			primitives->addChild(stippleDynamicTests);

			nostippleTests->addChild(new BaseTestCase<TrianglesTestInstance>		(testCtx, "triangles",			"Render primitives as VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, verify rasterization result",					samples[samplesNdx]));
			nostippleTests->addChild(new WidenessTestCase<PointTestInstance>		(testCtx, "points",				"Render primitives as VK_PRIMITIVE_TOPOLOGY_POINT_LIST, verify rasterization result",						PRIMITIVEWIDENESS_WIDE,	PRIMITIVESTRICTNESS_IGNORE,	false, samples[samplesNdx], LINESTIPPLE_DISABLED, VK_LINE_RASTERIZATION_MODE_DEFAULT_EXT));

			nostippleTests->addChild(new WidenessTestCase<LinesTestInstance>		(testCtx, "strict_lines",		"Render primitives as VK_PRIMITIVE_TOPOLOGY_LINE_LIST in strict mode, verify rasterization result",						PRIMITIVEWIDENESS_NARROW,	PRIMITIVESTRICTNESS_STRICT,	true, samples[samplesNdx], LINESTIPPLE_DISABLED, VK_LINE_RASTERIZATION_MODE_EXT_LAST));
			nostippleTests->addChild(new WidenessTestCase<LinesTestInstance>		(testCtx, "strict_lines_wide",	"Render primitives as VK_PRIMITIVE_TOPOLOGY_LINE_LIST in strict mode with wide lines, verify rasterization result",		PRIMITIVEWIDENESS_WIDE,		PRIMITIVESTRICTNESS_STRICT,	true, samples[samplesNdx], LINESTIPPLE_DISABLED, VK_LINE_RASTERIZATION_MODE_EXT_LAST));

			nostippleTests->addChild(new WidenessTestCase<LinesTestInstance>		(testCtx, "non_strict_lines",		"Render primitives as VK_PRIMITIVE_TOPOLOGY_LINE_LIST in nonstrict mode, verify rasterization result",						PRIMITIVEWIDENESS_NARROW,	PRIMITIVESTRICTNESS_NONSTRICT,	true, samples[samplesNdx], LINESTIPPLE_DISABLED, VK_LINE_RASTERIZATION_MODE_EXT_LAST));
			nostippleTests->addChild(new WidenessTestCase<LinesTestInstance>		(testCtx, "non_strict_lines_wide",	"Render primitives as VK_PRIMITIVE_TOPOLOGY_LINE_LIST in nonstrict mode with wide lines, verify rasterization result",		PRIMITIVEWIDENESS_WIDE,		PRIMITIVESTRICTNESS_NONSTRICT,	true, samples[samplesNdx], LINESTIPPLE_DISABLED, VK_LINE_RASTERIZATION_MODE_EXT_LAST));

			for (int i = 0; i < 3; ++i) {

				tcu::TestCaseGroup *g = i == 2 ? stippleDynamicTests : i == 1 ? stippleStaticTests : nostippleTests;

				LineStipple stipple = (LineStipple)i;

				g->addChild(new WidenessTestCase<LinesTestInstance>		(testCtx, "lines",						"Render primitives as VK_PRIMITIVE_TOPOLOGY_LINE_LIST, verify rasterization result",						PRIMITIVEWIDENESS_NARROW,	PRIMITIVESTRICTNESS_IGNORE, true, samples[samplesNdx], stipple, VK_LINE_RASTERIZATION_MODE_DEFAULT_EXT, i == 0 ? RESOLUTION_NPOT : 0));
				g->addChild(new WidenessTestCase<LineStripTestInstance>	(testCtx, "line_strip",					"Render primitives as VK_PRIMITIVE_TOPOLOGY_LINE_STRIP, verify rasterization result",						PRIMITIVEWIDENESS_NARROW,	PRIMITIVESTRICTNESS_IGNORE, true, samples[samplesNdx], stipple, VK_LINE_RASTERIZATION_MODE_DEFAULT_EXT));
				g->addChild(new WidenessTestCase<LinesTestInstance>		(testCtx, "lines_wide",					"Render primitives as VK_PRIMITIVE_TOPOLOGY_LINE_LIST with wide lines, verify rasterization result",		PRIMITIVEWIDENESS_WIDE,		PRIMITIVESTRICTNESS_IGNORE, true, samples[samplesNdx], stipple, VK_LINE_RASTERIZATION_MODE_DEFAULT_EXT));
				g->addChild(new WidenessTestCase<LineStripTestInstance>	(testCtx, "line_strip_wide",			"Render primitives as VK_PRIMITIVE_TOPOLOGY_LINE_STRIP with wide lines, verify rasterization result",		PRIMITIVEWIDENESS_WIDE,		PRIMITIVESTRICTNESS_IGNORE, true, samples[samplesNdx], stipple, VK_LINE_RASTERIZATION_MODE_DEFAULT_EXT));

				g->addChild(new WidenessTestCase<LinesTestInstance>		(testCtx, "rectangular_lines",			"Render primitives as VK_PRIMITIVE_TOPOLOGY_LINE_LIST, verify rasterization result",						PRIMITIVEWIDENESS_NARROW,	PRIMITIVESTRICTNESS_IGNORE, true, samples[samplesNdx], stipple, VK_LINE_RASTERIZATION_MODE_RECTANGULAR_EXT));
				g->addChild(new WidenessTestCase<LineStripTestInstance>	(testCtx, "rectangular_line_strip",		"Render primitives as VK_PRIMITIVE_TOPOLOGY_LINE_STRIP, verify rasterization result",						PRIMITIVEWIDENESS_NARROW,	PRIMITIVESTRICTNESS_IGNORE, true, samples[samplesNdx], stipple, VK_LINE_RASTERIZATION_MODE_RECTANGULAR_EXT));
				g->addChild(new WidenessTestCase<LinesTestInstance>		(testCtx, "rectangular_lines_wide",		"Render primitives as VK_PRIMITIVE_TOPOLOGY_LINE_LIST with wide lines, verify rasterization result",		PRIMITIVEWIDENESS_WIDE,		PRIMITIVESTRICTNESS_IGNORE, true, samples[samplesNdx], stipple, VK_LINE_RASTERIZATION_MODE_RECTANGULAR_EXT));
				g->addChild(new WidenessTestCase<LineStripTestInstance>	(testCtx, "rectangular_line_strip_wide","Render primitives as VK_PRIMITIVE_TOPOLOGY_LINE_STRIP with wide lines, verify rasterization result",		PRIMITIVEWIDENESS_WIDE,		PRIMITIVESTRICTNESS_IGNORE, true, samples[samplesNdx], stipple, VK_LINE_RASTERIZATION_MODE_RECTANGULAR_EXT));

				g->addChild(new WidenessTestCase<LinesTestInstance>		(testCtx, "bresenham_lines",			"Render primitives as VK_PRIMITIVE_TOPOLOGY_LINE_LIST, verify rasterization result",						PRIMITIVEWIDENESS_NARROW,	PRIMITIVESTRICTNESS_IGNORE, true, samples[samplesNdx], stipple, VK_LINE_RASTERIZATION_MODE_BRESENHAM_EXT));
				g->addChild(new WidenessTestCase<LineStripTestInstance>	(testCtx, "bresenham_line_strip",		"Render primitives as VK_PRIMITIVE_TOPOLOGY_LINE_STRIP, verify rasterization result",						PRIMITIVEWIDENESS_NARROW,	PRIMITIVESTRICTNESS_IGNORE, true, samples[samplesNdx], stipple, VK_LINE_RASTERIZATION_MODE_BRESENHAM_EXT));
				g->addChild(new WidenessTestCase<LinesTestInstance>		(testCtx, "bresenham_lines_wide",		"Render primitives as VK_PRIMITIVE_TOPOLOGY_LINE_LIST with wide lines, verify rasterization result",		PRIMITIVEWIDENESS_WIDE,		PRIMITIVESTRICTNESS_IGNORE, true, samples[samplesNdx], stipple, VK_LINE_RASTERIZATION_MODE_BRESENHAM_EXT));
				g->addChild(new WidenessTestCase<LineStripTestInstance>	(testCtx, "bresenham_line_strip_wide",	"Render primitives as VK_PRIMITIVE_TOPOLOGY_LINE_STRIP with wide lines, verify rasterization result",		PRIMITIVEWIDENESS_WIDE,		PRIMITIVESTRICTNESS_IGNORE, true, samples[samplesNdx], stipple, VK_LINE_RASTERIZATION_MODE_BRESENHAM_EXT));

				g->addChild(new WidenessTestCase<LinesTestInstance>		(testCtx, "smooth_lines",				"Render primitives as VK_PRIMITIVE_TOPOLOGY_LINE_LIST, verify rasterization result",						PRIMITIVEWIDENESS_NARROW,	PRIMITIVESTRICTNESS_IGNORE, true, samples[samplesNdx], stipple, VK_LINE_RASTERIZATION_MODE_RECTANGULAR_SMOOTH_EXT));
				g->addChild(new WidenessTestCase<LineStripTestInstance>	(testCtx, "smooth_line_strip",			"Render primitives as VK_PRIMITIVE_TOPOLOGY_LINE_STRIP, verify rasterization result",						PRIMITIVEWIDENESS_NARROW,	PRIMITIVESTRICTNESS_IGNORE, true, samples[samplesNdx], stipple, VK_LINE_RASTERIZATION_MODE_RECTANGULAR_SMOOTH_EXT));
				g->addChild(new WidenessTestCase<LinesTestInstance>		(testCtx, "smooth_lines_wide",			"Render primitives as VK_PRIMITIVE_TOPOLOGY_LINE_LIST with wide lines, verify rasterization result",		PRIMITIVEWIDENESS_WIDE,		PRIMITIVESTRICTNESS_IGNORE, true, samples[samplesNdx], stipple, VK_LINE_RASTERIZATION_MODE_RECTANGULAR_SMOOTH_EXT));
				g->addChild(new WidenessTestCase<LineStripTestInstance>	(testCtx, "smooth_line_strip_wide",		"Render primitives as VK_PRIMITIVE_TOPOLOGY_LINE_STRIP with wide lines, verify rasterization result",		PRIMITIVEWIDENESS_WIDE,		PRIMITIVESTRICTNESS_IGNORE, true, samples[samplesNdx], stipple, VK_LINE_RASTERIZATION_MODE_RECTANGULAR_SMOOTH_EXT));
			}
		}

		// .fill_rules
		{
			tcu::TestCaseGroup* const fillRules = new tcu::TestCaseGroup(testCtx, ("fill_rules" + caseName.str()).c_str(), "Primitive fill rules");

			rasterizationTests->addChild(fillRules);

			fillRules->addChild(new FillRuleTestCase(testCtx,	"basic_quad",			"Verify fill rules",	FillRuleTestInstance::FILLRULECASE_BASIC,			samples[samplesNdx]));
			fillRules->addChild(new FillRuleTestCase(testCtx,	"basic_quad_reverse",	"Verify fill rules",	FillRuleTestInstance::FILLRULECASE_REVERSED,		samples[samplesNdx]));
			fillRules->addChild(new FillRuleTestCase(testCtx,	"clipped_full",			"Verify fill rules",	FillRuleTestInstance::FILLRULECASE_CLIPPED_FULL,	samples[samplesNdx]));
			fillRules->addChild(new FillRuleTestCase(testCtx,	"clipped_partly",		"Verify fill rules",	FillRuleTestInstance::FILLRULECASE_CLIPPED_PARTIAL,	samples[samplesNdx]));
			fillRules->addChild(new FillRuleTestCase(testCtx,	"projected",			"Verify fill rules",	FillRuleTestInstance::FILLRULECASE_PROJECTED,		samples[samplesNdx]));
		}

		// .interpolation
		{
			tcu::TestCaseGroup* const interpolation = new tcu::TestCaseGroup(testCtx, ("interpolation" + caseName.str()).c_str(), "Test interpolation");

			rasterizationTests->addChild(interpolation);

			interpolation->addChild(new TriangleInterpolationTestCase		(testCtx, "triangles",		"Verify triangle interpolation",		VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,	INTERPOLATIONFLAGS_NONE,															samples[samplesNdx]));
			interpolation->addChild(new LineInterpolationTestCase			(testCtx, "lines",			"Verify line interpolation",			VK_PRIMITIVE_TOPOLOGY_LINE_LIST,		INTERPOLATIONFLAGS_NONE,	PRIMITIVEWIDENESS_NARROW,	PRIMITIVESTRICTNESS_IGNORE,	samples[samplesNdx]));
			interpolation->addChild(new LineInterpolationTestCase			(testCtx, "lines_wide",		"Verify wide line interpolation",		VK_PRIMITIVE_TOPOLOGY_LINE_LIST,		INTERPOLATIONFLAGS_NONE,	PRIMITIVEWIDENESS_WIDE,		PRIMITIVESTRICTNESS_IGNORE,	samples[samplesNdx]));

			interpolation->addChild(new LineInterpolationTestCase			(testCtx, "strict_lines",			"Verify strict line interpolation",			VK_PRIMITIVE_TOPOLOGY_LINE_LIST,		INTERPOLATIONFLAGS_NONE,	PRIMITIVEWIDENESS_NARROW,	PRIMITIVESTRICTNESS_STRICT,	samples[samplesNdx]));
			interpolation->addChild(new LineInterpolationTestCase			(testCtx, "strict_lines_wide",		"Verify strict wide line interpolation",	VK_PRIMITIVE_TOPOLOGY_LINE_LIST,		INTERPOLATIONFLAGS_NONE,	PRIMITIVEWIDENESS_WIDE,		PRIMITIVESTRICTNESS_STRICT,	samples[samplesNdx]));

			interpolation->addChild(new LineInterpolationTestCase			(testCtx, "non_strict_lines",			"Verify non-strict line interpolation",			VK_PRIMITIVE_TOPOLOGY_LINE_LIST,		INTERPOLATIONFLAGS_NONE,	PRIMITIVEWIDENESS_NARROW,	PRIMITIVESTRICTNESS_NONSTRICT,	samples[samplesNdx]));
			interpolation->addChild(new LineInterpolationTestCase			(testCtx, "non_strict_lines_wide",		"Verify non-strict wide line interpolation",	VK_PRIMITIVE_TOPOLOGY_LINE_LIST,		INTERPOLATIONFLAGS_NONE,	PRIMITIVEWIDENESS_WIDE,		PRIMITIVESTRICTNESS_NONSTRICT,	samples[samplesNdx]));
		}
	}

	// .provoking_vertex
	{
		tcu::TestCaseGroup* const	provokingVertex		= new tcu::TestCaseGroup(testCtx, "provoking_vertex", "Test provoking vertex");

		struct Params { const char *type; bool requireGeometryShader; };
		Params	primitiveTypes[]	=
		{
			{ "triangle_list", false },
			{ "triangle_list_with_adjacency", true },
			{ "triangle_strip", false },
			{ "triangle_strip_with_adjacency", true },
			{ "triangle_fan", false },
			{ "line_list", false },
			{ "line_list_with_adjacency", true },
			{ "line_strip", false },
			{ "line_strip_with_adjacency", true },
		};

		rasterizationTests->addChild(provokingVertex);

		for (deUint32 primitiveTypeIdx = 0; primitiveTypeIdx < DE_LENGTH_OF_ARRAY(primitiveTypes); primitiveTypeIdx++)
		{
			Params &					params		= primitiveTypes[primitiveTypeIdx];
			const std::string			file		= std::string(params.type) + ".amber";

			cts_amber::AmberTestCase*	testCase	= cts_amber::createAmberTestCase(testCtx, params.type, "", "provoking_vertex", file);
			if (params.requireGeometryShader)
			{
				testCase->addRequirement("Features.geometryShader");
			}
			provokingVertex->addChild(testCase);
		}
	}

	// .line_continuity
	{
		tcu::TestCaseGroup* const	lineContinuity	= new tcu::TestCaseGroup(testCtx, "line_continuity", "Test line continuity");
		static const char			dataDir[]		= "rasterization/line_continuity";

		struct Case
		{
			std::string	name;
			std::string	desc;
			bool		requireFillModeNonSolid;
		};

		static const Case cases[] =
		{
			{	"line-strip",			"Test line strip drawing produces continuous lines",	false	},
			{	"polygon-mode-lines",	"Test triangles drawn with lines are continuous",		true	}
		};

		rasterizationTests->addChild(lineContinuity);

		for (int i = 0; i < DE_LENGTH_OF_ARRAY(cases); ++i)
		{
			const std::string			fileName	= cases[i].name + ".amber";
			cts_amber::AmberTestCase*	testCase	= cts_amber::createAmberTestCase(testCtx, cases[i].name.c_str(), cases[i].desc.c_str(), dataDir, fileName);

			if (cases[i].requireFillModeNonSolid)
			{
				testCase->addRequirement("Features.fillModeNonSolid");
			}

			lineContinuity->addChild(testCase);
		}
	}

	// Fragment shader side effects.
	{
		rasterizationTests->addChild(createFragSideEffectsTests(testCtx));
	}
}

} // anonymous

tcu::TestCaseGroup* createTests (tcu::TestContext& testCtx)
{
	return createTestGroup(testCtx, "rasterization", "Rasterization Tests", createRasterizationTests);
}

} // rasterization
} // vkt
