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
#include "vktRasterizationProvokingVertexTests.hpp"
#include "tcuRasterizationVerifier.hpp"
#include "tcuSurface.hpp"
#include "tcuRenderTarget.hpp"
#include "tcuVectorUtil.hpp"
#include "tcuStringTemplate.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuResultCollector.hpp"
#include "tcuFloatFormat.hpp"
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
	virtual void									drawPrimitives					(tcu::Surface& result, const std::vector<tcu::Vec4>& vertexData, VkPrimitiveTopology primitiveTopology);
	void											drawPrimitives					(tcu::Surface& result, const std::vector<tcu::Vec4>& vertexData, const std::vector<tcu::Vec4>& coloDrata, VkPrimitiveTopology primitiveTopology);
	void											drawPrimitives					(tcu::Surface& result, const std::vector<tcu::Vec4>& positionData, const std::vector<tcu::Vec4>& colorData, VkPrimitiveTopology primitiveTopology,
																						VkImage image, VkImage resolvedImage, VkFramebuffer frameBuffer, const deUint32 renderSize, VkBuffer resultBuffer, const Allocation& resultBufferMemory);
	virtual float									getLineWidth					(void) const;
	virtual float									getPointSize					(void) const;
	virtual bool									getLineStippleDynamic			(void) const { return false; };

	virtual
	const VkPipelineRasterizationStateCreateInfo*	getRasterizationStateCreateInfo	(void) const;

	virtual
	VkPipelineRasterizationLineStateCreateInfoEXT	initLineRasterizationStateCreateInfo	(void) const;

	virtual
	const VkPipelineRasterizationLineStateCreateInfoEXT*	getLineRasterizationStateCreateInfo	(void);

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

private:
	virtual int										getIteration					(void) const { TCU_THROW(InternalError, "Iteration undefined in the base class"); }
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
	, m_lineRasterizationStateInfo	()
{
	const DeviceInterface&						vkd						= m_context.getDeviceInterface();
	const VkDevice								vkDevice				= m_context.getDevice();
	const deUint32								queueFamilyIndex		= m_context.getUniversalQueueFamilyIndex();
	Allocator&									allocator				= m_context.getDefaultAllocator();
	DescriptorPoolBuilder						descriptorPoolBuilder;
	DescriptorSetLayoutBuilder					descriptorSetLayoutBuilder;

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

		const VkPipelineRasterizationLineStateCreateInfoEXT* lineRasterizationStateInfo = getLineRasterizationStateCreateInfo();

		if (lineRasterizationStateInfo != DE_NULL)
			appendStructurePtrToVulkanChain(&rasterizationStateInfo.pNext, lineRasterizationStateInfo);

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

VkPipelineRasterizationLineStateCreateInfoEXT BaseRenderingTestInstance::initLineRasterizationStateCreateInfo (void) const
{
	VkPipelineRasterizationLineStateCreateInfoEXT lineRasterizationStateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_LINE_STATE_CREATE_INFO_EXT,	// VkStructureType				sType;
		DE_NULL,																// const void*					pNext;
		VK_LINE_RASTERIZATION_MODE_DEFAULT_EXT,									// VkLineRasterizationModeEXT	lineRasterizationMode;
		VK_FALSE,																// VkBool32						stippledLineEnable;
		1,																		// uint32_t						lineStippleFactor;
		0xFFFF,																	// uint16_t						lineStipplePattern;
	};

	return lineRasterizationStateInfo;
}

const VkPipelineRasterizationLineStateCreateInfoEXT* BaseRenderingTestInstance::getLineRasterizationStateCreateInfo (void)
{
	if (m_lineRasterizationStateInfo.sType != VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_LINE_STATE_CREATE_INFO_EXT)
		m_lineRasterizationStateInfo = initLineRasterizationStateCreateInfo();

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
							BaseTriangleTestInstance	(Context& context, VkPrimitiveTopology primitiveTopology, VkSampleCountFlagBits sampleCount, deUint32 renderSize = RESOLUTION_POT);
	virtual tcu::TestStatus	iterate						(void);

protected:
	int						getIteration				(void) const	{ return m_iteration;		}
	int						getIterationCount			(void) const	{ return m_iterationCount;	}

private:
	virtual void			generateTriangles			(int iteration, std::vector<tcu::Vec4>& outData, std::vector<TriangleSceneSpec::SceneTriangle>& outTriangles) = DE_NULL;
	virtual bool			compareAndVerify			(std::vector<TriangleSceneSpec::SceneTriangle>&	triangles,
														 tcu::Surface&									resultImage,
														 std::vector<tcu::Vec4>&						drawBuffer);

	int						m_iteration;
	const int				m_iterationCount;
	VkPrimitiveTopology		m_primitiveTopology;
	bool					m_allIterationsPassed;
};

BaseTriangleTestInstance::BaseTriangleTestInstance (Context& context, VkPrimitiveTopology primitiveTopology, VkSampleCountFlagBits sampleCount, deUint32 renderSize)
	: BaseRenderingTestInstance		(context, sampleCount, renderSize)
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
		const bool compareOk = compareAndVerify(triangles, resultImage, drawBuffer);

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

bool BaseTriangleTestInstance::compareAndVerify (std::vector<TriangleSceneSpec::SceneTriangle>& triangles, tcu::Surface& resultImage, std::vector<tcu::Vec4>&)
{
	RasterizationArguments	args;
	TriangleSceneSpec		scene;

	tcu::IVec4				colorBits = tcu::getTextureFormatBitDepth(getTextureFormat());

	args.numSamples		= m_multisampling ? 1 : 0;
	args.subpixelBits	= m_subpixelBits;
	args.redBits		= colorBits[0];
	args.greenBits		= colorBits[1];
	args.blueBits		= colorBits[2];

	scene.triangles.swap(triangles);

	return verifyTriangleGroupRasterization(resultImage, scene, args, m_context.getTestContext().getLog());
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
														 const deUint32				additionalRenderSize = 0,
														 const deUint32				renderSize = RESOLUTION_POT,
														 const float				narrowLineWidth = 1.0f);
	virtual tcu::TestStatus		iterate					(void);
	virtual float				getLineWidth			(void) const;
	bool						getLineStippleEnable	(void) const { return m_stipple != LINESTIPPLE_DISABLED; }
	virtual bool				getLineStippleDynamic	(void) const { return m_stipple == LINESTIPPLE_DYNAMIC; };

	virtual
	VkPipelineRasterizationLineStateCreateInfoEXT	initLineRasterizationStateCreateInfo	(void) const;

	virtual
	const VkPipelineRasterizationLineStateCreateInfoEXT*	getLineRasterizationStateCreateInfo	(void);

protected:
	int							getIteration			(void) const	{ return m_iteration;		}
	int							getIterationCount		(void) const	{ return m_iterationCount;	}

private:
	virtual void				generateLines			(int iteration, std::vector<tcu::Vec4>& outData, std::vector<LineSceneSpec::SceneLine>& outLines) = DE_NULL;
	virtual bool				compareAndVerify		(std::vector<LineSceneSpec::SceneLine>&	lines,
														 tcu::Surface&							resultImage,
														 std::vector<tcu::Vec4>&				drawBuffer);

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
											const deUint32				additionalRenderSize,
											const deUint32				renderSize,
											const float					narrowLineWidth)
	: BaseRenderingTestInstance	(context, sampleCount, renderSize, VK_FORMAT_R8G8B8A8_UNORM, additionalRenderSize)
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
		m_lineWidths.resize(m_iterationCount, narrowLineWidth);

		// Bump up m_maxLineWidth for conservative rasterization
		if (narrowLineWidth > m_maxLineWidth)
			m_maxLineWidth = narrowLineWidth;
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
		{
			const bool compareOk = compareAndVerify(lines, resultImage, drawBuffer);

			if (!compareOk)
				m_allIterationsPassed = false;
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

bool BaseLineTestInstance::compareAndVerify (std::vector<LineSceneSpec::SceneLine>&	lines, tcu::Surface& resultImage, std::vector<tcu::Vec4>& drawBuffer)
{
	const float				lineWidth				= getLineWidth();
	bool					result					= true;
	tcu::Surface			additionalResultImage	(m_additionalRenderSize, m_additionalRenderSize);
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
	scene.isRectangular = m_lineRasterizationMode == VK_LINE_RASTERIZATION_MODE_RECTANGULAR_SMOOTH_EXT ||
	                      m_lineRasterizationMode == VK_LINE_RASTERIZATION_MODE_RECTANGULAR_EXT;

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
			result = false;
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
				result = false;
			}
		}

		if (!verifyRelaxedLineGroupRasterization(resultImage, scene, args, m_context.getTestContext().getLog(), (0 == m_multisampling), strict))
		{
			// Retry with weaker verification. If it passes, consider it a quality warning.
			scene.verificationMode = tcu::VERIFICATIONMODE_WEAKER;
			if (!verifyRelaxedLineGroupRasterization(resultImage, scene, args, m_context.getTestContext().getLog(), false, strict))
				result = false;
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
					result = false;
				}
				else
				{
					// Retry with weaker verification. If it passes, consider it a quality warning.
					scene.verificationMode = tcu::VERIFICATIONMODE_WEAKER;
					if (!verifyRelaxedLineGroupRasterization(resultImage, scene, args, m_context.getTestContext().getLog(), (0 == m_multisampling), strict))
						result = false;
					else
						m_qualityWarning = true;
				}
			}
		}
	}

	return result;
}

float BaseLineTestInstance::getLineWidth (void) const
{
	return m_lineWidths[m_iteration];
}

VkPipelineRasterizationLineStateCreateInfoEXT BaseLineTestInstance::initLineRasterizationStateCreateInfo (void) const
{
	VkPipelineRasterizationLineStateCreateInfoEXT lineRasterizationStateInfo	=
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
		lineRasterizationStateInfo.lineStippleFactor = lineStippleFactor;
		lineRasterizationStateInfo.lineStipplePattern = lineStipplePattern;
	}

	return lineRasterizationStateInfo;
}

const VkPipelineRasterizationLineStateCreateInfoEXT* BaseLineTestInstance::getLineRasterizationStateCreateInfo (void)
{
	if (m_lineRasterizationMode == VK_LINE_RASTERIZATION_MODE_EXT_LAST)
		return DE_NULL;

	if (m_lineRasterizationStateInfo.sType != VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_LINE_STATE_CREATE_INFO_EXT)
		m_lineRasterizationStateInfo = initLineRasterizationStateCreateInfo();

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
													 deUint32					additionalRenderSize,	// ignored
													 deUint32					renderSize				= RESOLUTION_POT,
													 float						pointSizeNarrow			= 1.0f);
	virtual tcu::TestStatus	iterate					(void);
	virtual float			getPointSize			(void) const;

protected:
	int						getIteration				(void) const	{ return m_iteration;		}
	int						getIterationCount			(void) const	{ return m_iterationCount;	}

private:
	virtual void			generatePoints			(int iteration, std::vector<tcu::Vec4>& outData, std::vector<PointSceneSpec::ScenePoint>& outPoints);
	virtual bool			compareAndVerify		(std::vector<PointSceneSpec::ScenePoint>&	points,
													 tcu::Surface&								resultImage,
													 std::vector<tcu::Vec4>&					drawBuffer);

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
									  deUint32						additionalRenderSize,
									  deUint32						renderSize,
									  float							pointSizeNarrow)
	: BaseRenderingTestInstance	(context, sampleCount, renderSize)
	, m_iteration				(0)
	, m_iterationCount			(3)
	, m_primitiveWideness		(wideness)
	, m_allIterationsPassed		(true)
	, m_maxPointSize			(pointSizeNarrow)
{
	DE_UNREF(strictness);
	DE_UNREF(stipple);
	DE_UNREF(lineRasterizationMode);
	DE_UNREF(additionalRenderSize);

	// create point sizes
	if (m_primitiveWideness == PRIMITIVEWIDENESS_NARROW)
	{
		m_pointSizes.resize(m_iterationCount, pointSizeNarrow);
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
			const bool compareOk = compareAndVerify(points, resultImage, drawBuffer);

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

bool PointTestInstance::compareAndVerify (std::vector<PointSceneSpec::ScenePoint>&	points,
										  tcu::Surface&								resultImage,
										  std::vector<tcu::Vec4>&					drawBuffer)
{
	RasterizationArguments	args;
	PointSceneSpec			scene;

	tcu::IVec4				colorBits = tcu::getTextureFormatBitDepth(getTextureFormat());

	args.numSamples		= m_multisampling ? 1 : 0;
	args.subpixelBits	= m_subpixelBits;
	args.redBits		= colorBits[0];
	args.greenBits		= colorBits[1];
	args.blueBits		= colorBits[2];

	scene.points.swap(points);

	DE_UNREF(drawBuffer);

	return verifyPointGroupRasterization(resultImage, scene, args, m_context.getTestContext().getLog());
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
				TriangleFanTestInstance			(Context& context, VkSampleCountFlagBits sampleCount);


	void		generateTriangles				(int iteration, std::vector<tcu::Vec4>& outData, std::vector<TriangleSceneSpec::SceneTriangle>& outTriangles);
};

TriangleFanTestInstance::TriangleFanTestInstance (Context& context, VkSampleCountFlagBits sampleCount)
	: BaseTriangleTestInstance(context, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN, sampleCount)
{
	if (context.isDeviceFunctionalitySupported("VK_KHR_portability_subset") &&
		!context.getPortabilitySubsetFeatures().triangleFans)
	{
		TCU_THROW(NotSupportedError, "VK_KHR_portability_subset: Triangle fans are not supported by this implementation");
	}
}

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

struct ConservativeTestConfig
{
	VkConservativeRasterizationModeEXT	conservativeRasterizationMode;
	float								extraOverestimationSize;
	VkPrimitiveTopology					primitiveTopology;
	bool								degeneratePrimitives;
	float								lineWidth;
	deUint32							resolution;
};

float getExtraOverestimationSize (const float overestimationSizeDesired, const VkPhysicalDeviceConservativeRasterizationPropertiesEXT& conservativeRasterizationProperties)
{
	const float extraOverestimationSize	= overestimationSizeDesired == TCU_INFINITY ? conservativeRasterizationProperties.maxExtraPrimitiveOverestimationSize
										: overestimationSizeDesired == -TCU_INFINITY ? conservativeRasterizationProperties.extraPrimitiveOverestimationSizeGranularity
										: overestimationSizeDesired;

	return extraOverestimationSize;
}

template <typename ConcreteTestInstance>
class ConservativeTestCase : public BaseRenderingTestCase
{
public:
									ConservativeTestCase		(tcu::TestContext&					context,
																 const std::string&					name,
																 const std::string&					description,
																 const ConservativeTestConfig&		conservativeTestConfig,
																 VkSampleCountFlagBits				sampleCount = VK_SAMPLE_COUNT_1_BIT)
										: BaseRenderingTestCase		(context, name, description, sampleCount)
										, m_conservativeTestConfig	(conservativeTestConfig)
									{}

	virtual void					checkSupport				(Context& context) const;

	virtual TestInstance*			createInstance				(Context& context) const
									{
										return new ConcreteTestInstance(context, m_conservativeTestConfig, m_sampleCount);
									}

protected:
	bool							isUseLineSubPixel			(Context& context) const;
	deUint32						getSubPixelResolution		(Context& context) const;

	const ConservativeTestConfig	m_conservativeTestConfig;
};

template <typename ConcreteTestInstance>
bool ConservativeTestCase<ConcreteTestInstance>::isUseLineSubPixel (Context& context) const
{
	return (isPrimitiveTopologyLine(m_conservativeTestConfig.primitiveTopology) && context.isDeviceFunctionalitySupported("VK_EXT_line_rasterization"));
}

template <typename ConcreteTestInstance>
deUint32 ConservativeTestCase<ConcreteTestInstance>::getSubPixelResolution (Context& context) const
{
	if (isUseLineSubPixel(context))
	{
		const VkPhysicalDeviceLineRasterizationPropertiesEXT	lineRasterizationPropertiesEXT	= context.getLineRasterizationPropertiesEXT();

		return lineRasterizationPropertiesEXT.lineSubPixelPrecisionBits;
	}
	else
	{
		return context.getDeviceProperties().limits.subPixelPrecisionBits;
	}
}

template <typename ConcreteTestInstance>
void ConservativeTestCase<ConcreteTestInstance>::checkSupport (Context& context) const
{
	context.requireDeviceFunctionality("VK_EXT_conservative_rasterization");

	const VkPhysicalDeviceConservativeRasterizationPropertiesEXT	conservativeRasterizationProperties	= context.getConservativeRasterizationPropertiesEXT();
	const deUint32													subPixelPrecisionBits				= getSubPixelResolution(context);
	const deUint32													subPixelPrecision					= 1<<subPixelPrecisionBits;
	const bool														linesPrecision						= isUseLineSubPixel(context);
	const float														primitiveOverestimationSizeMult		= float(subPixelPrecision) * conservativeRasterizationProperties.primitiveOverestimationSize;
	const bool														topologyLineOrPoint					= isPrimitiveTopologyLine(m_conservativeTestConfig.primitiveTopology) || isPrimitiveTopologyPoint(m_conservativeTestConfig.primitiveTopology);

	DE_ASSERT(subPixelPrecisionBits < sizeof(deUint32) * 8);

	context.getTestContext().getLog()
		<< tcu::TestLog::Message
		<< "maxExtraPrimitiveOverestimationSize="			<< conservativeRasterizationProperties.maxExtraPrimitiveOverestimationSize << '\n'
		<< "extraPrimitiveOverestimationSizeGranularity="	<< conservativeRasterizationProperties.extraPrimitiveOverestimationSizeGranularity << '\n'
		<< "degenerateLinesRasterized="						<< conservativeRasterizationProperties.degenerateLinesRasterized << '\n'
		<< "degenerateTrianglesRasterized="					<< conservativeRasterizationProperties.degenerateTrianglesRasterized << '\n'
		<< "primitiveOverestimationSize="					<< conservativeRasterizationProperties.primitiveOverestimationSize << " (==" << primitiveOverestimationSizeMult << '/' << subPixelPrecision << ")\n"
		<< "subPixelPrecisionBits="							<< subPixelPrecisionBits << (linesPrecision ? " (using VK_EXT_line_rasterization)" : " (using limits)") << '\n'
		<< tcu::TestLog::EndMessage;

	if (conservativeRasterizationProperties.extraPrimitiveOverestimationSizeGranularity > conservativeRasterizationProperties.maxExtraPrimitiveOverestimationSize)
		TCU_FAIL("Granularity cannot be greater than maximum extra size");

	if (topologyLineOrPoint)
	{
		if (!conservativeRasterizationProperties.conservativePointAndLineRasterization)
			TCU_THROW(NotSupportedError, "Conservative line and point rasterization is not supported");
	}

	if (m_conservativeTestConfig.conservativeRasterizationMode == VK_CONSERVATIVE_RASTERIZATION_MODE_UNDERESTIMATE_EXT)
	{
		if (conservativeRasterizationProperties.primitiveUnderestimation == DE_FALSE)
			TCU_THROW(NotSupportedError, "Underestimation is not supported");

		if (isPrimitiveTopologyLine(m_conservativeTestConfig.primitiveTopology))
		{
			const float	testLineWidth	= m_conservativeTestConfig.lineWidth;

			if (testLineWidth != 1.0f)
			{
				const VkPhysicalDeviceLimits&	limits					= context.getDeviceProperties().limits;
				const float						lineWidthRange[2]		= { limits.lineWidthRange[0], limits.lineWidthRange[1] };
				const float						lineWidthGranularity	= limits.lineWidthGranularity;

				context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_WIDE_LINES);

				if (lineWidthGranularity == 0.0f)
					TCU_THROW(NotSupportedError, "Wide lines required for test, but are not supported");

				DE_ASSERT(lineWidthGranularity > 0.0f && lineWidthRange[0] > 0.0f && lineWidthRange[1] >= lineWidthRange[0]);

				if (!de::inBounds(testLineWidth, lineWidthRange[0], lineWidthRange[1]))
					TCU_THROW(NotSupportedError, "Tested line width is not supported");

				const float	n	= (testLineWidth - lineWidthRange[0]) / lineWidthGranularity;

				if (deFloatFrac(n) != 0.0f || n * lineWidthGranularity + lineWidthRange[0] != testLineWidth)
					TCU_THROW(NotSupportedError, "Exact match of line width is required for the test");
			}
		}
		else if (isPrimitiveTopologyPoint(m_conservativeTestConfig.primitiveTopology))
		{
			const float	testPointSize	= m_conservativeTestConfig.lineWidth;

			if (testPointSize != 1.0f)
			{
				const VkPhysicalDeviceLimits&	limits					= context.getDeviceProperties().limits;
				const float						pointSizeRange[2]		= { limits.pointSizeRange[0], limits.pointSizeRange[1] };
				const float						pointSizeGranularity	= limits.pointSizeGranularity;

				context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_LARGE_POINTS);

				if (pointSizeGranularity == 0.0f)
					TCU_THROW(NotSupportedError, "Large points required for test, but are not supported");

				DE_ASSERT(pointSizeGranularity > 0.0f && pointSizeRange[0] > 0.0f && pointSizeRange[1] >= pointSizeRange[0]);

				if (!de::inBounds(testPointSize, pointSizeRange[0], pointSizeRange[1]))
					TCU_THROW(NotSupportedError, "Tested point size is not supported");

				const float	n	= (testPointSize - pointSizeRange[0]) / pointSizeGranularity;

				if (deFloatFrac(n) != 0.0f || n * pointSizeGranularity + pointSizeRange[0] != testPointSize)
					TCU_THROW(NotSupportedError, "Exact match of point size is required for the test");
			}
		}
	}
	else if (m_conservativeTestConfig.conservativeRasterizationMode == VK_CONSERVATIVE_RASTERIZATION_MODE_OVERESTIMATE_EXT)
	{
		const float extraOverestimationSize	= getExtraOverestimationSize(m_conservativeTestConfig.extraOverestimationSize, conservativeRasterizationProperties);

		if (extraOverestimationSize > conservativeRasterizationProperties.maxExtraPrimitiveOverestimationSize)
			TCU_THROW(NotSupportedError, "Specified overestimation size is not supported");

		if (topologyLineOrPoint)
		{
			if (!conservativeRasterizationProperties.conservativePointAndLineRasterization)
				TCU_THROW(NotSupportedError, "Conservative line and point rasterization is not supported");
		}

		if (isPrimitiveTopologyTriangle(m_conservativeTestConfig.primitiveTopology))
		{
			if (m_conservativeTestConfig.degeneratePrimitives)
			{
				// Enforce specification minimum required limit to avoid division by zero
				DE_ASSERT(subPixelPrecisionBits >= 4);

				// Make sure float precision of 22 bits is enough, i.e. resoultion in subpixel quarters less than float precision
				if (m_conservativeTestConfig.resolution * (1<<(subPixelPrecisionBits + 2)) > (1<<21))
					TCU_THROW(NotSupportedError, "Subpixel resolution is too high to generate degenerate primitives");
			}
		}
	}
	else
		TCU_THROW(InternalError, "Non-conservative mode tests are not supported by this class");
}

class ConservativeTraingleTestInstance : public BaseTriangleTestInstance
{
public:
																				ConservativeTraingleTestInstance				(Context&				context,
																																 ConservativeTestConfig	conservativeTestConfig,
																																 VkSampleCountFlagBits	sampleCount)
																					: BaseTriangleTestInstance						(context,
																																	 conservativeTestConfig.primitiveTopology,
																																	 sampleCount,
																																	 conservativeTestConfig.resolution)
																					, m_conservativeTestConfig						(conservativeTestConfig)
																					, m_conservativeRasterizationProperties			(context.getConservativeRasterizationPropertiesEXT())
																					, m_rasterizationConservativeStateCreateInfo	(initRasterizationConservativeStateCreateInfo())
																					, m_rasterizationStateCreateInfo				(initRasterizationStateCreateInfo())
																				{}

	void																		generateTriangles								(int											iteration,
																																 std::vector<tcu::Vec4>&						outData,
																																 std::vector<TriangleSceneSpec::SceneTriangle>&	outTriangles);
	const VkPipelineRasterizationStateCreateInfo*								getRasterizationStateCreateInfo					(void) const;

protected:
	virtual const VkPipelineRasterizationLineStateCreateInfoEXT*				getLineRasterizationStateCreateInfo				(void);

	virtual bool																compareAndVerify								(std::vector<TriangleSceneSpec::SceneTriangle>&	triangles,
																																 tcu::Surface&									resultImage,
																																 std::vector<tcu::Vec4>&						drawBuffer);
	virtual bool																compareAndVerifyOverestimatedNormal				(std::vector<TriangleSceneSpec::SceneTriangle>&	triangles,
																																 tcu::Surface&									resultImage);
	virtual bool																compareAndVerifyOverestimatedDegenerate			(std::vector<TriangleSceneSpec::SceneTriangle>&	triangles,
																																 tcu::Surface&									resultImage);
	virtual bool																compareAndVerifyUnderestimatedNormal			(std::vector<TriangleSceneSpec::SceneTriangle>&	triangles,
																																 tcu::Surface&									resultImage);
	virtual bool																compareAndVerifyUnderestimatedDegenerate		(std::vector<TriangleSceneSpec::SceneTriangle>&	triangles,
																																 tcu::Surface&									resultImage);
	void																		generateNormalTriangles							(int											iteration,
																																 std::vector<tcu::Vec4>&						outData,
																																 std::vector<TriangleSceneSpec::SceneTriangle>&	outTriangles);
	void																		generateDegenerateTriangles					(int											iteration,
																																 std::vector<tcu::Vec4>&						outData,
																																 std::vector<TriangleSceneSpec::SceneTriangle>&	outTriangles);
	void																		drawPrimitives									(tcu::Surface&									result,
																																 const std::vector<tcu::Vec4>&					vertexData,
																																 VkPrimitiveTopology							primitiveTopology);

private:
	const std::vector<VkPipelineRasterizationConservativeStateCreateInfoEXT>	initRasterizationConservativeStateCreateInfo	(void);
	const std::vector<VkPipelineRasterizationStateCreateInfo>					initRasterizationStateCreateInfo				(void);

	const ConservativeTestConfig												m_conservativeTestConfig;
	const VkPhysicalDeviceConservativeRasterizationPropertiesEXT				m_conservativeRasterizationProperties;
	const std::vector<VkPipelineRasterizationConservativeStateCreateInfoEXT>	m_rasterizationConservativeStateCreateInfo;
	const std::vector<VkPipelineRasterizationStateCreateInfo>					m_rasterizationStateCreateInfo;
};

void ConservativeTraingleTestInstance::generateTriangles (int iteration, std::vector<tcu::Vec4>& outData, std::vector<TriangleSceneSpec::SceneTriangle>& outTriangles)
{
	if (m_conservativeTestConfig.degeneratePrimitives)
		generateDegenerateTriangles(iteration, outData, outTriangles);
	else
		generateNormalTriangles(iteration, outData, outTriangles);
}

void ConservativeTraingleTestInstance::generateNormalTriangles (int iteration, std::vector<tcu::Vec4>& outData, std::vector<TriangleSceneSpec::SceneTriangle>& outTriangles)
{
	const float	halfPixel						= 1.0f / float(m_renderSize);
	const float extraOverestimationSize			= getExtraOverestimationSize(m_conservativeTestConfig.extraOverestimationSize, m_conservativeRasterizationProperties);
	const float	overestimate					= 2.0f * halfPixel * (m_conservativeRasterizationProperties.primitiveOverestimationSize + extraOverestimationSize);
	const float	overestimateMargin				= overestimate;
	const float	underestimateMargin				= 0.0f;
	const bool	isOverestimate					= m_conservativeTestConfig.conservativeRasterizationMode == VK_CONSERVATIVE_RASTERIZATION_MODE_OVERESTIMATE_EXT;
	const float	margin							= isOverestimate ? overestimateMargin : underestimateMargin;
	const char*	overestimateIterationComments[]	= { "Corner touch", "Any portion pixel coverage", "Edge touch" };

	outData.resize(6);

	switch (iteration)
	{
		case 0:
		{
			// Corner touch
			const float edge	= 2 * halfPixel + margin;
			const float left	= -1.0f + edge;
			const float right	= +1.0f - edge;
			const float up		= -1.0f + edge;
			const float down	= +1.0f - edge;

			outData[0] = tcu::Vec4( left, down, 0.0f, 1.0f);
			outData[1] = tcu::Vec4( left,   up, 0.0f, 1.0f);
			outData[2] = tcu::Vec4(right, down, 0.0f, 1.0f);

			outData[3] = tcu::Vec4( left,   up, 0.0f, 1.0f);
			outData[4] = tcu::Vec4(right, down, 0.0f, 1.0f);
			outData[5] = tcu::Vec4(right,   up, 0.0f, 1.0f);

			break;
		}

		case 1:
		{
			// Partial coverage
			const float eps		= halfPixel / 32.0f;
			const float edge	= 4.0f * halfPixel  + margin - eps;
			const float left	= -1.0f + edge;
			const float right	= +1.0f - edge;
			const float up		= -1.0f + edge;
			const float down	= +1.0f - edge;

			outData[0] = tcu::Vec4( left, down, 0.0f, 1.0f);
			outData[1] = tcu::Vec4( left,   up, 0.0f, 1.0f);
			outData[2] = tcu::Vec4(right, down, 0.0f, 1.0f);

			outData[3] = tcu::Vec4( left,   up, 0.0f, 1.0f);
			outData[4] = tcu::Vec4(right, down, 0.0f, 1.0f);
			outData[5] = tcu::Vec4(right,   up, 0.0f, 1.0f);

			break;
		}

		case 2:
		{
			// Edge touch
			const float edge	= 6.0f * halfPixel + margin;
			const float left	= -1.0f + edge;
			const float right	= +1.0f - edge;
			const float up		= -1.0f + edge;
			const float down	= +1.0f - edge;

			outData[0] = tcu::Vec4( left, down, 0.0f, 1.0f);
			outData[1] = tcu::Vec4( left,   up, 0.0f, 1.0f);
			outData[2] = tcu::Vec4(right, down, 0.0f, 1.0f);

			outData[3] = tcu::Vec4( left,   up, 0.0f, 1.0f);
			outData[4] = tcu::Vec4(right, down, 0.0f, 1.0f);
			outData[5] = tcu::Vec4(right,   up, 0.0f, 1.0f);

			break;
		}

		default:
			TCU_THROW(InternalError, "Unexpected iteration");
	}

	outTriangles.resize(outData.size() / 3);

	for (size_t ndx = 0; ndx < outTriangles.size(); ++ndx)
	{
		outTriangles[ndx].positions[0] = outData[3 * ndx + 0];	outTriangles[ndx].sharedEdge[0] = false;
		outTriangles[ndx].positions[1] = outData[3 * ndx + 1];	outTriangles[ndx].sharedEdge[1] = false;
		outTriangles[ndx].positions[2] = outData[3 * ndx + 2];	outTriangles[ndx].sharedEdge[2] = false;
	}

	// log
	if (isOverestimate)
	{
		m_context.getTestContext().getLog()
			<< tcu::TestLog::Message
			<< "Testing " << overestimateIterationComments[iteration] << " "
			<< "with rendering " << outTriangles.size() << " triangle(s):"
			<< tcu::TestLog::EndMessage;
	}
	else
	{
		m_context.getTestContext().getLog()
			<< tcu::TestLog::Message
			<< "Rendering " << outTriangles.size() << " triangle(s):"
			<< tcu::TestLog::EndMessage;
	}

	for (size_t ndx = 0; ndx < outTriangles.size(); ++ndx)
	{
		const deUint32	 multiplier	= m_renderSize / 2;

		m_context.getTestContext().getLog()
			<< tcu::TestLog::Message
			<< "Triangle " << (ndx + 1) << ":"
			<< "\n\t" << outTriangles[ndx].positions[0] << " == " << (float(multiplier) * outTriangles[ndx].positions[0]) << "/" << multiplier
			<< "\n\t" << outTriangles[ndx].positions[1] << " == " << (float(multiplier) * outTriangles[ndx].positions[1]) << "/" << multiplier
			<< "\n\t" << outTriangles[ndx].positions[2] << " == " << (float(multiplier) * outTriangles[ndx].positions[2]) << "/" << multiplier
			<< tcu::TestLog::EndMessage;
	}
}

void ConservativeTraingleTestInstance::generateDegenerateTriangles (int iteration, std::vector<tcu::Vec4>& outData, std::vector<TriangleSceneSpec::SceneTriangle>& outTriangles)
{
	tcu::TestLog&	log								= m_context.getTestContext().getLog();
	const float		pixelSize						= 2.0f / float(m_renderSize);
	const deUint32	subPixels						= 1u << m_context.getDeviceProperties().limits.subPixelPrecisionBits;
	const float		subPixelSize					= pixelSize / float(subPixels);
	const float		extraOverestimationSize			= getExtraOverestimationSize(m_conservativeTestConfig.extraOverestimationSize, m_conservativeRasterizationProperties);
	const float		totalOverestimate				= m_conservativeRasterizationProperties.primitiveOverestimationSize + extraOverestimationSize;
	const float		totalOverestimateInSubPixels	= deFloatCeil(totalOverestimate * float(subPixels));
	const float		overestimate					= subPixelSize * totalOverestimateInSubPixels;
	const float		overestimateSafetyMargin		= subPixelSize * 0.125f;
	const float		overestimateMargin				= overestimate + overestimateSafetyMargin;
	const float		underestimateMargin				= 0.0f;
	const bool		isOverestimate					= m_conservativeTestConfig.conservativeRasterizationMode == VK_CONSERVATIVE_RASTERIZATION_MODE_OVERESTIMATE_EXT;
	const float		margin							= isOverestimate ? overestimateMargin : underestimateMargin;
	const char*		overestimateIterationComments[]	= { "Backfacing", "Generate pixels", "Use provoking vertex" };

	if (pixelSize < 2 * overestimateMargin)
		TCU_THROW(NotSupportedError, "Could not generate degenerate triangle for such overestimate parameters");

	outData.clear();

	switch (iteration)
	{
		case 0:
		case 1:
		case 2:
		{
			for (int rowNdx = 0; rowNdx < 3; ++rowNdx)
			for (int colNdx = 0; colNdx < 4; ++colNdx)
			{
				const float	offsetX		= -1.0f + float(4 * (colNdx + 1)) * pixelSize;
				const float	offsetY		= -1.0f + float(4 * (rowNdx + 1)) * pixelSize;
				const float	left		= offsetX + margin;
				const float	right		= offsetX + margin + 0.25f * subPixelSize;
				const float	up			= offsetY + margin;
				const float	down		= offsetY + margin + 0.25f * subPixelSize;
				const bool	luPresent	= (rowNdx & 1) == 0;
				const bool	rdPresent	= (rowNdx & 2) == 0;
				const bool	luCW		= (colNdx & 1) == 0;
				const bool	rdCW		= (colNdx & 2) == 0;

				DE_ASSERT(left < right);
				DE_ASSERT(up < down);

				if (luPresent)
				{
					if (luCW)
					{
						// CW triangle left up
						outData.push_back(tcu::Vec4( left, down, 0.0f, 1.0f));
						outData.push_back(tcu::Vec4( left,   up, 0.0f, 1.0f));
						outData.push_back(tcu::Vec4(right,   up, 0.0f, 1.0f));
					}
					else
					{
						// CCW triangle left up
						outData.push_back(tcu::Vec4(right,   up, 0.0f, 1.0f));
						outData.push_back(tcu::Vec4( left,   up, 0.0f, 1.0f));
						outData.push_back(tcu::Vec4( left, down, 0.0f, 1.0f));
					}
				}

				if (rdPresent)
				{
					if (rdCW)
					{
						// CW triangle right down
						outData.push_back(tcu::Vec4(right,   up, 0.0f, 1.0f));
						outData.push_back(tcu::Vec4(right, down, 0.0f, 1.0f));
						outData.push_back(tcu::Vec4( left, down, 0.0f, 1.0f));
					}
					else
					{
						// CCW triangle right down
						outData.push_back(tcu::Vec4( left, down, 0.0f, 1.0f));
						outData.push_back(tcu::Vec4(right, down, 0.0f, 1.0f));
						outData.push_back(tcu::Vec4(right,   up, 0.0f, 1.0f));
					}
				}
			}

			break;
		}

		default:
			TCU_THROW(InternalError, "Unexpected iteration");
	}

	outTriangles.resize(outData.size() / 3);

	for (size_t ndx = 0; ndx < outTriangles.size(); ++ndx)
	{
		outTriangles[ndx].positions[0] = outData[3 * ndx + 0];	outTriangles[ndx].sharedEdge[0] = false;
		outTriangles[ndx].positions[1] = outData[3 * ndx + 1];	outTriangles[ndx].sharedEdge[1] = false;
		outTriangles[ndx].positions[2] = outData[3 * ndx + 2];	outTriangles[ndx].sharedEdge[2] = false;
	}

	// log
	if (isOverestimate)
	{
		m_context.getTestContext().getLog()
			<< tcu::TestLog::Message
			<< "Testing " << overestimateIterationComments[iteration] << " "
			<< "with rendering " << outTriangles.size() << " triangle(s):"
			<< tcu::TestLog::EndMessage;
	}
	else
	{
		m_context.getTestContext().getLog()
			<< tcu::TestLog::Message
			<< "Rendering " << outTriangles.size() << " triangle(s):"
			<< tcu::TestLog::EndMessage;
	}

	for (int ndx = 0; ndx < (int)outTriangles.size(); ++ndx)
	{
		const deUint32	multiplierInt	= m_renderSize / 2;
		const deUint32	multiplierFrac	= subPixels;
		std::string		coordsString;

		for (size_t vertexNdx = 0; vertexNdx < 3; ++vertexNdx)
		{
			const tcu::Vec4&	pos				= outTriangles[ndx].positions[vertexNdx];
			std::ostringstream	coordsFloat;
			std::ostringstream	coordsNatural;

			for (int coordNdx = 0; coordNdx < 2; ++coordNdx)
			{
				const char*	sep		= (coordNdx < 1) ? "," : "";
				const float	coord	= pos[coordNdx];
				const char	sign	= deSign(coord) < 0 ? '-' : '+';
				const float	m		= deFloatFloor(float(multiplierInt) * deFloatAbs(coord));
				const float	r		= deFloatFrac(float(multiplierInt) * deFloatAbs(coord)) * float(multiplierFrac);

				coordsFloat << std::fixed << std::setw(13) << std::setprecision(10) << coord << sep;
				coordsNatural << sign << '(' << m << '+' << r << '/' << multiplierFrac << ')' << sep;
			}

			coordsString += "\n\t[" + coordsFloat.str() + "] == [" + coordsNatural.str() + "] / " + de::toString(multiplierInt);
		}

		log << tcu::TestLog::Message
			<< "Triangle " << (ndx + 1) << ':'
			<< coordsString
			<< tcu::TestLog::EndMessage;
	}
}

void ConservativeTraingleTestInstance::drawPrimitives (tcu::Surface& result, const std::vector<tcu::Vec4>& vertexData, VkPrimitiveTopology primitiveTopology)
{
	if (m_conservativeTestConfig.degeneratePrimitives && getIteration() == 2)
	{
		// Set provoking vertex color to white
		tcu::Vec4				colorProvoking	(1.0f, 1.0f, 1.0f, 1.0f);
		tcu::Vec4				colorOther		(0.0f, 1.0f, 1.0f, 1.0f);
		std::vector<tcu::Vec4>	colorData;

		colorData.reserve(vertexData.size());

		for (size_t vertexNdx = 0; vertexNdx < vertexData.size(); ++vertexNdx)
			if (vertexNdx % 3 == 0)
				colorData.push_back(colorProvoking);
			else
				colorData.push_back(colorOther);

		BaseRenderingTestInstance::drawPrimitives(result, vertexData, colorData, primitiveTopology);
	}
	else
		BaseRenderingTestInstance::drawPrimitives(result, vertexData, primitiveTopology);
}

bool ConservativeTraingleTestInstance::compareAndVerify (std::vector<TriangleSceneSpec::SceneTriangle>& triangles, tcu::Surface& resultImage, std::vector<tcu::Vec4>& drawBuffer)
{
	DE_UNREF(drawBuffer);

	switch (m_conservativeTestConfig.conservativeRasterizationMode)
	{
		case VK_CONSERVATIVE_RASTERIZATION_MODE_OVERESTIMATE_EXT:
		{
			if (m_conservativeTestConfig.degeneratePrimitives)
				return compareAndVerifyOverestimatedDegenerate(triangles, resultImage);
			else
				return compareAndVerifyOverestimatedNormal(triangles, resultImage);

			break;
		}

		case VK_CONSERVATIVE_RASTERIZATION_MODE_UNDERESTIMATE_EXT:
		{
			if (m_conservativeTestConfig.degeneratePrimitives)
				return compareAndVerifyUnderestimatedDegenerate(triangles, resultImage);
			else
				return compareAndVerifyUnderestimatedNormal(triangles, resultImage);

			break;
		}

		default:
			TCU_THROW(InternalError, "Unknown conservative rasterization mode");
	}
}

bool ConservativeTraingleTestInstance::compareAndVerifyOverestimatedNormal (std::vector<TriangleSceneSpec::SceneTriangle>& triangles, tcu::Surface& resultImage)
{
	DE_UNREF(triangles);

	const int			start					= getIteration() + 1;
	const int			end						= resultImage.getHeight() - start;
	const tcu::RGBA		backgroundColor			= tcu::RGBA(0, 0, 0, 255);
	const tcu::RGBA		foregroundColor			= tcu::RGBA(255, 255, 255, 255);
	const tcu::RGBA		unexpectedPixelColor	= tcu::RGBA(255, 0, 0, 255);
	tcu::TestLog&		log						= m_context.getTestContext().getLog();
	int					errX					= 0;
	int					errY					= 0;
	deUint32			errValue				= 0;
	bool				result					= true;

	DE_ASSERT(resultImage.getHeight() == resultImage.getWidth());

	for (int y = start; result && y < end; ++y)
	for (int x = start; result && x < end; ++x)
	{
		if (resultImage.getPixel(x,y).getPacked() != foregroundColor.getPacked())
		{
			result		= false;
			errX		= x;
			errY		= y;
			errValue	= resultImage.getPixel(x,y).getPacked();

			break;
		}
	}

	if (!result)
	{
		tcu::Surface	errorMask		(resultImage.getWidth(), resultImage.getHeight());
		tcu::Surface	expectedImage	(resultImage.getWidth(), resultImage.getHeight());

		for (int y = 0; y < errorMask.getHeight(); ++y)
		for (int x = 0; x < errorMask.getWidth(); ++x)
		{
			errorMask.setPixel(x, y, backgroundColor);
			expectedImage.setPixel(x, y, backgroundColor);
		}

		for (int y = start; y < end; ++y)
		for (int x = start; x < end; ++x)
		{
			expectedImage.setPixel(x, y, foregroundColor);

			if (resultImage.getPixel(x, y).getPacked() != foregroundColor.getPacked())
				errorMask.setPixel(x, y, unexpectedPixelColor);
		}

		log << tcu::TestLog::Message << "Invalid pixels found starting at " << errX << "," << errY << " value=0x" << std::hex << errValue
			<< tcu::TestLog::EndMessage;
		log << tcu::TestLog::ImageSet("Verification result", "Result of rendering")
			<< tcu::TestLog::Image("Result",	"Result",		resultImage)
			<< tcu::TestLog::Image("Expected",	"Expected",		expectedImage)
			<< tcu::TestLog::Image("ErrorMask", "ErrorMask",	errorMask)
			<< tcu::TestLog::EndImageSet;
	}
	else
	{
		log << tcu::TestLog::Message << "No invalid pixels found." << tcu::TestLog::EndMessage;
		log << tcu::TestLog::ImageSet("Verification result", "Result of rendering")
			<< tcu::TestLog::Image("Result", "Result", resultImage)
			<< tcu::TestLog::EndImageSet;
	}

	return result;
}

bool ConservativeTraingleTestInstance::compareAndVerifyOverestimatedDegenerate (std::vector<TriangleSceneSpec::SceneTriangle>& triangles, tcu::Surface& resultImage)
{
	DE_UNREF(triangles);

	const char*			iterationComments[]		= { "Cull back face triangles", "Cull front face triangles", "Cull none" };
	const tcu::RGBA		backgroundColor			= tcu::RGBA(0, 0, 0, 255);
	const tcu::RGBA		foregroundColor			= tcu::RGBA(255, 255, 255, 255);
	const tcu::RGBA		unexpectedPixelColor	= tcu::RGBA(255, 0, 0, 255);
	tcu::TestLog&		log						= m_context.getTestContext().getLog();
	bool				result					= true;
	tcu::Surface		referenceImage			(resultImage.getWidth(), resultImage.getHeight());

	for (int y = 0; y < resultImage.getHeight(); ++y)
	for (int x = 0; x < resultImage.getWidth(); ++x)
		referenceImage.setPixel(x, y, backgroundColor);

	if (m_conservativeRasterizationProperties.degenerateTrianglesRasterized)
	{
		if (getIteration() != 0)
		{
			log << tcu::TestLog::Message << "Triangles expected to be rasterized with at least one pixel of white color each" << tcu::TestLog::EndMessage;

			for (int rowNdx = 0; rowNdx < 3; ++rowNdx)
			for (int colNdx = 0; colNdx < 4; ++colNdx)
			{
				referenceImage.setPixel(4 * (colNdx + 1), 4 * (rowNdx + 1), foregroundColor);

				// Allow implementations that need to be extra conservative with degenerate triangles,
				// which may cause extra coverage.
				if (resultImage.getPixel(4 * (colNdx + 1) - 1, 4 * (rowNdx + 1) - 1) == foregroundColor)
					referenceImage.setPixel(4 * (colNdx + 1) - 1, 4 * (rowNdx + 1) - 1, foregroundColor);
				if (resultImage.getPixel(4 * (colNdx + 1) - 1, 4 * (rowNdx + 1)) == foregroundColor)
					referenceImage.setPixel(4 * (colNdx + 1) - 1, 4 * (rowNdx + 1), foregroundColor);
				if (resultImage.getPixel(4 * (colNdx + 1), 4 * (rowNdx + 1) - 1) == foregroundColor)
					referenceImage.setPixel(4 * (colNdx + 1), 4 * (rowNdx + 1) - 1, foregroundColor);
			}
		}
		else
			log << tcu::TestLog::Message << "Triangles expected to be culled due to backfacing culling and all degenerate triangles assumed to be backfacing" << tcu::TestLog::EndMessage;
	}
	else
		log << tcu::TestLog::Message << "Triangles expected to be culled due to degenerateTrianglesRasterized=false" << tcu::TestLog::EndMessage;

	for (int y = 0; result && y < resultImage.getHeight(); ++y)
	for (int x = 0; result && x < resultImage.getWidth(); ++x)
	{
		if (resultImage.getPixel(x,y).getPacked() != referenceImage.getPixel(x,y).getPacked())
		{
			result = false;

			break;
		}
	}

	if (!result)
	{
		tcu::Surface	errorMask	(resultImage.getWidth(), resultImage.getHeight());

		for (int y = 0; y < errorMask.getHeight(); ++y)
		for (int x = 0; x < errorMask.getWidth(); ++x)
		{
			if (resultImage.getPixel(x, y).getPacked() != referenceImage.getPixel(x, y).getPacked())
				errorMask.setPixel(x, y, unexpectedPixelColor);
			else
				errorMask.setPixel(x, y, backgroundColor);
		}

		log << tcu::TestLog::Message << "Invalid pixels found for mode '" << iterationComments[getIteration()] << "'"
			<< tcu::TestLog::EndMessage;
		log << tcu::TestLog::ImageSet("Verification result", "Result of rendering")
			<< tcu::TestLog::Image("Result",	"Result",		resultImage)
			<< tcu::TestLog::Image("Reference",	"Reference",	referenceImage)
			<< tcu::TestLog::Image("ErrorMask", "ErrorMask",	errorMask)
			<< tcu::TestLog::EndImageSet;
	}
	else
	{
		log << tcu::TestLog::Message << "No invalid pixels found." << tcu::TestLog::EndMessage;
		log << tcu::TestLog::ImageSet("Verification result", "Result of rendering")
			<< tcu::TestLog::Image("Result", "Result", resultImage)
			<< tcu::TestLog::EndImageSet;
	}

	return result;
}

bool ConservativeTraingleTestInstance::compareAndVerifyUnderestimatedNormal (std::vector<TriangleSceneSpec::SceneTriangle>& triangles, tcu::Surface& resultImage)
{
	DE_UNREF(triangles);

	const tcu::RGBA		backgroundColor			= tcu::RGBA(0, 0, 0, 255);
	const tcu::RGBA		foregroundColor			= tcu::RGBA(255, 255, 255, 255);
	const tcu::RGBA		unexpectedPixelColor	= tcu::RGBA(255, 0, 0, 255);
	const tcu::IVec2	viewportSize			= tcu::IVec2(resultImage.getWidth(), resultImage.getHeight());
	tcu::TestLog&		log						= m_context.getTestContext().getLog();
	int					errX					= -1;
	int					errY					= -1;
	deUint32			errValue				= 0;
	tcu::Surface		referenceImage			(resultImage.getWidth(), resultImage.getHeight());
	bool				result					= true;

	DE_ASSERT(resultImage.getHeight() == resultImage.getWidth());

	for (int y = 0; y < resultImage.getHeight(); ++y)
	for (int x = 0; x < resultImage.getWidth(); ++x)
		referenceImage.setPixel(x, y, backgroundColor);

	for (size_t triangleNdx = 0; triangleNdx < triangles.size(); ++triangleNdx)
	{
		const tcu::Vec4&	p0	= triangles[triangleNdx].positions[0];
		const tcu::Vec4&	p1	= triangles[triangleNdx].positions[1];
		const tcu::Vec4&	p2	= triangles[triangleNdx].positions[2];

		for (int y = 0; y < resultImage.getHeight(); ++y)
		for (int x = 0; x < resultImage.getWidth(); ++x)
		{
			if (calculateUnderestimateTriangleCoverage(p0, p1, p2, tcu::IVec2(x,y), m_subpixelBits, viewportSize) == tcu::COVERAGE_FULL)
				referenceImage.setPixel(x, y, foregroundColor);
		}
	}

	for (int y = 0; result && y < resultImage.getHeight(); ++y)
	for (int x = 0; result && x < resultImage.getWidth(); ++x)
		if (resultImage.getPixel(x, y).getPacked() != referenceImage.getPixel(x, y).getPacked())
		{
			result		= false;
			errX		= x;
			errY		= y;
			errValue	= resultImage.getPixel(x,y).getPacked();
		}

	if (!result)
	{
		tcu::Surface	errorMask	(resultImage.getWidth(), resultImage.getHeight());

		for (int y = 0; y < errorMask.getHeight(); ++y)
		for (int x = 0; x < errorMask.getWidth(); ++x)
		{
			if (resultImage.getPixel(x,y).getPacked() != referenceImage.getPixel(x,y).getPacked())
				errorMask.setPixel(x, y, unexpectedPixelColor);
			else
				errorMask.setPixel(x, y, backgroundColor);
		}

		log << tcu::TestLog::Message << "Invalid pixels found starting at " << errX << "," << errY << " value=0x" << std::hex << errValue
			<< tcu::TestLog::EndMessage;
		log << tcu::TestLog::ImageSet("Verification result", "Result of rendering")
			<< tcu::TestLog::Image("Result",	"Result",		resultImage)
			<< tcu::TestLog::Image("Refernce",	"Refernce",		referenceImage)
			<< tcu::TestLog::Image("ErrorMask", "ErrorMask",	errorMask)
			<< tcu::TestLog::EndImageSet;
	}
	else
	{
		log << tcu::TestLog::Message << "No invalid pixels found." << tcu::TestLog::EndMessage;
		log << tcu::TestLog::ImageSet("Verification result", "Result of rendering")
			<< tcu::TestLog::Image("Result", "Result", resultImage)
			<< tcu::TestLog::EndImageSet;
	}

	return result;
}

bool ConservativeTraingleTestInstance::compareAndVerifyUnderestimatedDegenerate (std::vector<TriangleSceneSpec::SceneTriangle>& triangles, tcu::Surface& resultImage)
{
	DE_UNREF(triangles);

	const char*			iterationComments[]		= { "Cull back face triangles", "Cull front face triangles", "Cull none" };
	const tcu::RGBA		backgroundColor			= tcu::RGBA(0, 0, 0, 255);
	const tcu::RGBA		unexpectedPixelColor	= tcu::RGBA(255, 0, 0, 255);
	tcu::TestLog&		log						= m_context.getTestContext().getLog();
	int					errX					= 0;
	int					errY					= 0;
	deUint32			errValue				= 0;
	bool				result					= true;

	if (m_conservativeRasterizationProperties.degenerateTrianglesRasterized)
	{
		if (getIteration() != 0)
			log << tcu::TestLog::Message << "Triangles expected to be not rendered due to no one triangle can fully cover fragment" << tcu::TestLog::EndMessage;
		else
			log << tcu::TestLog::Message << "Triangles expected to be culled due to backfacing culling and all degenerate triangles assumed to be backfacing" << tcu::TestLog::EndMessage;
	}
	else
		log << tcu::TestLog::Message << "Triangles expected to be culled due to degenerateTrianglesRasterized=false" << tcu::TestLog::EndMessage;

	for (int y = 0; result && y < resultImage.getHeight(); ++y)
	for (int x = 0; result && x < resultImage.getWidth(); ++x)
	{
		if (resultImage.getPixel(x, y).getPacked() != backgroundColor.getPacked())
		{
			result		= false;
			errX		= x;
			errY		= y;
			errValue	= resultImage.getPixel(x,y).getPacked();

			break;
		}
	}

	if (!result)
	{
		tcu::Surface	referenceImage	(resultImage.getWidth(), resultImage.getHeight());
		tcu::Surface	errorMask		(resultImage.getWidth(), resultImage.getHeight());

		for (int y = 0; y < resultImage.getHeight(); ++y)
		for (int x = 0; x < resultImage.getWidth(); ++x)
			referenceImage.setPixel(x, y, backgroundColor);

		for (int y = 0; y < errorMask.getHeight(); ++y)
		for (int x = 0; x < errorMask.getWidth(); ++x)
		{
			if (resultImage.getPixel(x, y).getPacked() != referenceImage.getPixel(x, y).getPacked())
				errorMask.setPixel(x, y, unexpectedPixelColor);
			else
				errorMask.setPixel(x, y, backgroundColor);
		}

		log << tcu::TestLog::Message << "Invalid pixels found for mode '" << iterationComments[getIteration()] << "' starting at " << errX << "," << errY << " value=0x" << std::hex << errValue
			<< tcu::TestLog::EndMessage;

		log << tcu::TestLog::ImageSet("Verification result", "Result of rendering")
			<< tcu::TestLog::Image("Result",	"Result",		resultImage)
			<< tcu::TestLog::Image("Reference",	"Reference",	referenceImage)
			<< tcu::TestLog::Image("ErrorMask", "ErrorMask",	errorMask)
			<< tcu::TestLog::EndImageSet;
	}
	else
	{
		log << tcu::TestLog::Message << "No invalid pixels found." << tcu::TestLog::EndMessage;
		log << tcu::TestLog::ImageSet("Verification result", "Result of rendering")
			<< tcu::TestLog::Image("Result", "Result", resultImage)
			<< tcu::TestLog::EndImageSet;
	}

	return result;
}

const std::vector<VkPipelineRasterizationConservativeStateCreateInfoEXT> ConservativeTraingleTestInstance::initRasterizationConservativeStateCreateInfo (void)
{
	const float															extraOverestimationSize	= getExtraOverestimationSize(m_conservativeTestConfig.extraOverestimationSize, m_conservativeRasterizationProperties);
	std::vector<VkPipelineRasterizationConservativeStateCreateInfoEXT>	result;

	result.reserve(getIterationCount());

	for (int iteration = 0; iteration < getIterationCount(); ++iteration)
	{
		const VkPipelineRasterizationConservativeStateCreateInfoEXT	rasterizationConservativeStateCreateInfo	=
		{
			VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_CONSERVATIVE_STATE_CREATE_INFO_EXT,	//  VkStructureType											sType;
			DE_NULL,																		//  const void*												pNext;
			(VkPipelineRasterizationConservativeStateCreateFlagsEXT)0,						//  VkPipelineRasterizationConservativeStateCreateFlagsEXT	flags;
			m_conservativeTestConfig.conservativeRasterizationMode,							//  VkConservativeRasterizationModeEXT						conservativeRasterizationMode;
			extraOverestimationSize															//  float													extraPrimitiveOverestimationSize;
		};

		result.push_back(rasterizationConservativeStateCreateInfo);
	}

	return result;
}

const std::vector<VkPipelineRasterizationStateCreateInfo> ConservativeTraingleTestInstance::initRasterizationStateCreateInfo (void)
{
	std::vector<VkPipelineRasterizationStateCreateInfo>	result;

	result.reserve(getIterationCount());

	for (int iteration = 0; iteration < getIterationCount(); ++iteration)
	{
		const VkCullModeFlags							cullModeFlags					= (!m_conservativeTestConfig.degeneratePrimitives) ? VK_CULL_MODE_NONE
																						: (iteration == 0) ? VK_CULL_MODE_BACK_BIT
																						: (iteration == 1) ? VK_CULL_MODE_FRONT_BIT
																						: VK_CULL_MODE_NONE;

		const VkPipelineRasterizationStateCreateInfo	rasterizationStateCreateInfo	=
		{
			VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,		//  VkStructureType							sType;
			&m_rasterizationConservativeStateCreateInfo[iteration],			//  const void*								pNext;
			0,																//  VkPipelineRasterizationStateCreateFlags	flags;
			false,															//  VkBool32								depthClampEnable;
			false,															//  VkBool32								rasterizerDiscardEnable;
			VK_POLYGON_MODE_FILL,											//  VkPolygonMode							polygonMode;
			cullModeFlags,													//  VkCullModeFlags							cullMode;
			VK_FRONT_FACE_COUNTER_CLOCKWISE,								//  VkFrontFace								frontFace;
			VK_FALSE,														//  VkBool32								depthBiasEnable;
			0.0f,															//  float									depthBiasConstantFactor;
			0.0f,															//  float									depthBiasClamp;
			0.0f,															//  float									depthBiasSlopeFactor;
			getLineWidth(),													//  float									lineWidth;
		};

		result.push_back(rasterizationStateCreateInfo);
	}

	return result;
}

const VkPipelineRasterizationStateCreateInfo* ConservativeTraingleTestInstance::getRasterizationStateCreateInfo	(void) const
{
	return &m_rasterizationStateCreateInfo[getIteration()];
}

const VkPipelineRasterizationLineStateCreateInfoEXT* ConservativeTraingleTestInstance::getLineRasterizationStateCreateInfo	(void)
{
	return DE_NULL;
}


class ConservativeLineTestInstance : public BaseLineTestInstance
{
public:
																				ConservativeLineTestInstance					(Context&								context,
																																 ConservativeTestConfig					conservativeTestConfig,
																																 VkSampleCountFlagBits					sampleCount);

	void																		generateLines									(int									iteration,
																																 std::vector<tcu::Vec4>&				outData,
																																 std::vector<LineSceneSpec::SceneLine>&	outLines);
	const VkPipelineRasterizationStateCreateInfo*								getRasterizationStateCreateInfo					(void) const;

protected:
	virtual const VkPipelineRasterizationLineStateCreateInfoEXT*				getLineRasterizationStateCreateInfo				(void);

	virtual bool																compareAndVerify								(std::vector<LineSceneSpec::SceneLine>&	lines,
																																 tcu::Surface&							resultImage,
																																 std::vector<tcu::Vec4>&				drawBuffer);
	virtual bool																compareAndVerifyOverestimatedNormal				(std::vector<LineSceneSpec::SceneLine>&	lines,
																																 tcu::Surface&							resultImage);
	virtual bool																compareAndVerifyOverestimatedDegenerate			(std::vector<LineSceneSpec::SceneLine>&	lines,
																																 tcu::Surface&							resultImage);
	virtual bool																compareAndVerifyUnderestimatedNormal			(std::vector<LineSceneSpec::SceneLine>&	lines,
																																 tcu::Surface&							resultImage);
	virtual bool																compareAndVerifyUnderestimatedDegenerate		(std::vector<LineSceneSpec::SceneLine>&	lines,
																																 tcu::Surface&							resultImage);
	void																		generateNormalLines								(int									iteration,
																																 std::vector<tcu::Vec4>&				outData,
																																 std::vector<LineSceneSpec::SceneLine>&	outLines);
	void																		generateDegenerateLines							(int									iteration,
																																 std::vector<tcu::Vec4>&				outData,
																																 std::vector<LineSceneSpec::SceneLine>&	outLines);
	void																		drawPrimitives									(tcu::Surface&							result,
																																 const std::vector<tcu::Vec4>&			vertexData,
																																 VkPrimitiveTopology					primitiveTopology);

private:
	const std::vector<VkPipelineRasterizationConservativeStateCreateInfoEXT>	initRasterizationConservativeStateCreateInfo	(void);
	const std::vector<VkPipelineRasterizationStateCreateInfo>					initRasterizationStateCreateInfo				(void);

	const ConservativeTestConfig												m_conservativeTestConfig;
	const VkPhysicalDeviceConservativeRasterizationPropertiesEXT				m_conservativeRasterizationProperties;
	const std::vector<VkPipelineRasterizationConservativeStateCreateInfoEXT>	m_rasterizationConservativeStateCreateInfo;
	const std::vector<VkPipelineRasterizationStateCreateInfo>					m_rasterizationStateCreateInfo;
};

ConservativeLineTestInstance::ConservativeLineTestInstance (Context&				context,
															ConservativeTestConfig	conservativeTestConfig,
															VkSampleCountFlagBits	sampleCount)
	: BaseLineTestInstance							(
														context,
														conservativeTestConfig.primitiveTopology,
														PRIMITIVEWIDENESS_NARROW,
														PRIMITIVESTRICTNESS_IGNORE,
														sampleCount,
														LINESTIPPLE_DISABLED,
														VK_LINE_RASTERIZATION_MODE_DEFAULT_EXT,
														0,
														conservativeTestConfig.resolution,
														conservativeTestConfig.lineWidth
													)
	, m_conservativeTestConfig						(conservativeTestConfig)
	, m_conservativeRasterizationProperties			(context.getConservativeRasterizationPropertiesEXT())
	, m_rasterizationConservativeStateCreateInfo	(initRasterizationConservativeStateCreateInfo())
	, m_rasterizationStateCreateInfo				(initRasterizationStateCreateInfo())
{
}

void ConservativeLineTestInstance::generateLines (int iteration, std::vector<tcu::Vec4>& outData, std::vector<LineSceneSpec::SceneLine>& outLines)
{
	if (m_conservativeTestConfig.degeneratePrimitives)
		generateDegenerateLines(iteration, outData, outLines);
	else
		generateNormalLines(iteration, outData, outLines);
}

void ConservativeLineTestInstance::generateNormalLines (int iteration, std::vector<tcu::Vec4>& outData, std::vector<LineSceneSpec::SceneLine>& outLines)
{
	const char*		iterationComment		= "";
	const float		halfPixel				= 1.0f / float(m_renderSize);
	const float		extraOverestimationSize	= getExtraOverestimationSize(m_conservativeTestConfig.extraOverestimationSize, m_conservativeRasterizationProperties);
	const float		overestimate			= 2.0f * halfPixel * (m_conservativeRasterizationProperties.primitiveOverestimationSize + extraOverestimationSize);
	const float		overestimateMargin		= overestimate;
	const float		underestimateMargin		= 0.0f;
	const bool		isOverestimate			= m_conservativeTestConfig.conservativeRasterizationMode == VK_CONSERVATIVE_RASTERIZATION_MODE_OVERESTIMATE_EXT;
	const float		margin					= isOverestimate ? overestimateMargin : underestimateMargin;
	const float		edge					= 4 * halfPixel + margin;
	const float		left					= -1.0f + edge;
	const float		right					= +1.0f - edge;
	const float		up						= -1.0f + edge;
	const float		down					= +1.0f - edge;

	outData.reserve(2);

	if (isOverestimate)
	{
		const char*		iterationComments[]		= { "Horizontal up line", "Vertical line", "Horizontal down line" };

		iterationComment = iterationComments[iteration];

		switch (iteration)
		{
			case 0:
			{
				outData.push_back(tcu::Vec4(              left,   up + halfPixel, 0.0f, 1.0f));
				outData.push_back(tcu::Vec4(             right,   up + halfPixel, 0.0f, 1.0f));

				break;
			}

			case 1:
			{
				outData.push_back(tcu::Vec4(  left + halfPixel,               up, 0.0f, 1.0f));
				outData.push_back(tcu::Vec4(  left + halfPixel,             down, 0.0f, 1.0f));

				break;
			}

			case 2:
			{
				outData.push_back(tcu::Vec4(              left, down - halfPixel, 0.0f, 1.0f));
				outData.push_back(tcu::Vec4(             right, down - halfPixel, 0.0f, 1.0f));

				break;
			}

			default:
				TCU_THROW(InternalError, "Unexpected iteration");
		}
	}
	else
	{
		const char*		iterationComments[]	= { "Horizontal lines", "Vertical lines", "Diagonal lines" };
		const deUint32	subPixels			= 1u << m_subpixelBits;
		const float		subPixelSize		= 2.0f * halfPixel / float(subPixels);
		const float		blockStep			= 16.0f * 2.0f * halfPixel;
		const float		lineWidth			= 2.0f * halfPixel * getLineWidth();
		const float		offsets[]			=
		{
			float(1) * blockStep,
			float(2) * blockStep + halfPixel,
			float(3) * blockStep + 0.5f * lineWidth + 2.0f * subPixelSize,
			float(4) * blockStep + 0.5f * lineWidth - 2.0f * subPixelSize,
		};

		iterationComment = iterationComments[iteration];

		outData.reserve(DE_LENGTH_OF_ARRAY(offsets));

		switch (iteration)
		{
			case 0:
			{
				for (size_t lineNdx = 0; lineNdx < DE_LENGTH_OF_ARRAY(offsets); ++lineNdx)
				{
					outData.push_back(tcu::Vec4( left + halfPixel, up + offsets[lineNdx], 0.0f, 1.0f));
					outData.push_back(tcu::Vec4(right - halfPixel, up + offsets[lineNdx], 0.0f, 1.0f));
				}

				break;
			}

			case 1:
			{
				for (size_t lineNdx = 0; lineNdx < DE_LENGTH_OF_ARRAY(offsets); ++lineNdx)
				{
					outData.push_back(tcu::Vec4(left + offsets[lineNdx],   up + halfPixel, 0.0f, 1.0f));
					outData.push_back(tcu::Vec4(left + offsets[lineNdx], down - halfPixel, 0.0f, 1.0f));
				}

				break;
			}

			case 2:
			{
				for (size_t lineNdx = 0; lineNdx < DE_LENGTH_OF_ARRAY(offsets); ++lineNdx)
				{
					outData.push_back(tcu::Vec4(left + offsets[lineNdx],          up + halfPixel, 0.0f, 1.0f));
					outData.push_back(tcu::Vec4(      right - halfPixel, down - offsets[lineNdx], 0.0f, 1.0f));
				}

				break;
			}

			default:
				TCU_THROW(InternalError, "Unexpected iteration");
		}
	}

	DE_ASSERT(outData.size() % 2 == 0);
	outLines.resize(outData.size() / 2);
	for(size_t lineNdx = 0; lineNdx < outLines.size(); ++lineNdx)
	{
		outLines[lineNdx].positions[0] = outData[2 * lineNdx + 0];
		outLines[lineNdx].positions[1] = outData[2 * lineNdx + 1];
	}

	// log
	m_context.getTestContext().getLog()
		<< tcu::TestLog::Message
		<< "Testing " << iterationComment << " "
		<< "with rendering " << outLines.size() << " line(s):"
		<< tcu::TestLog::EndMessage;

	for (int ndx = 0; ndx < (int)outLines.size(); ++ndx)
	{
		const deUint32	 multiplier	= m_renderSize / 2;

		m_context.getTestContext().getLog()
			<< tcu::TestLog::Message
			<< "Line " << (ndx+1) << ":"
			<< "\n\t" << outLines[ndx].positions[0] << " == " << (float(multiplier) * outLines[ndx].positions[0]) << "/" << multiplier
			<< "\n\t" << outLines[ndx].positions[1] << " == " << (float(multiplier) * outLines[ndx].positions[1]) << "/" << multiplier
			<< tcu::TestLog::EndMessage;
	}
}

void ConservativeLineTestInstance::generateDegenerateLines (int iteration, std::vector<tcu::Vec4>& outData, std::vector<LineSceneSpec::SceneLine>& outLines)
{
	const bool		isOverestimate		= m_conservativeTestConfig.conservativeRasterizationMode == VK_CONSERVATIVE_RASTERIZATION_MODE_OVERESTIMATE_EXT;
	const float		pixelSize			= 2.0f / float(m_renderSize);
	const deUint32	subPixels			= 1u << m_context.getDeviceProperties().limits.subPixelPrecisionBits;
	const float		subPixelSize		= pixelSize / float(subPixels);
	const char*		iterationComments[]	= { "Horizontal line", "Vertical line", "Diagonal line" };

	outData.clear();

	if (isOverestimate)
	{
		const float		extraOverestimationSize			= getExtraOverestimationSize(m_conservativeTestConfig.extraOverestimationSize, m_conservativeRasterizationProperties);
		const float		totalOverestimate				= m_conservativeRasterizationProperties.primitiveOverestimationSize + extraOverestimationSize;
		const float		totalOverestimateInSubPixels	= deFloatCeil(totalOverestimate * float(subPixels));
		const float		overestimate					= subPixelSize * totalOverestimateInSubPixels;
		const float		overestimateSafetyMargin		= subPixelSize * 0.125f;
		const float		margin							= overestimate + overestimateSafetyMargin;
		const float		originOffset					= -1.0f + 1 * pixelSize;
		const float		originLeft						= originOffset + margin;
		const float		originRight						= originOffset + margin + 0.25f * subPixelSize;
		const float		originUp						= originOffset + margin;
		const float		originDown						= originOffset + margin + 0.25f * subPixelSize;

		switch (iteration)
		{
			case 0:
			{
				outData.push_back(tcu::Vec4( originLeft,   originUp, 0.0f, 1.0f));
				outData.push_back(tcu::Vec4(originRight,   originUp, 0.0f, 1.0f));

				break;
			}

			case 1:
			{
				outData.push_back(tcu::Vec4( originLeft,   originUp, 0.0f, 1.0f));
				outData.push_back(tcu::Vec4( originLeft, originDown, 0.0f, 1.0f));

				break;
			}

			case 2:
			{
				outData.push_back(tcu::Vec4( originLeft,   originUp, 0.0f, 1.0f));
				outData.push_back(tcu::Vec4(originRight, originDown, 0.0f, 1.0f));

				break;
			}

			default:
				TCU_THROW(InternalError, "Unexpected iteration");
		}
	}
	else
	{
		size_t rowStart	= 3 * getIteration();
		size_t rowEnd	= 3 * (getIteration() + 1);

		for (size_t rowNdx = rowStart; rowNdx < rowEnd; ++rowNdx)
		for (size_t colNdx = 0; colNdx < 3 * 3; ++colNdx)
		{
			const float		originOffsetY	= -1.0f + float(4 * (1 + rowNdx)) * pixelSize;
			const float		originOffsetX	= -1.0f + float(4 * (1 + colNdx)) * pixelSize;
			const float		x0				= float(rowNdx % 3);
			const float		y0				= float(rowNdx / 3);
			const float		x1				= float(colNdx % 3);
			const float		y1				= float(colNdx / 3);
			const tcu::Vec4	p0				= tcu::Vec4(originOffsetX + x0 * pixelSize / 2.0f, originOffsetY + y0 * pixelSize / 2.0f, 0.0f, 1.0f);
			const tcu::Vec4	p1				= tcu::Vec4(originOffsetX + x1 * pixelSize / 2.0f, originOffsetY + y1 * pixelSize / 2.0f, 0.0f, 1.0f);

			if (x0 == x1 && y0 == y1)
				continue;

			outData.push_back(p0);
			outData.push_back(p1);
		}
	}

	outLines.resize(outData.size() / 2);

	for (size_t ndx = 0; ndx < outLines.size(); ++ndx)
	{
		outLines[ndx].positions[0] = outData[2 * ndx + 0];
		outLines[ndx].positions[1] = outData[2 * ndx + 1];
	}

	// log
	m_context.getTestContext().getLog()
		<< tcu::TestLog::Message
		<< "Testing " << iterationComments[iteration] << " "
		<< "with rendering " << outLines.size() << " line(s):"
		<< tcu::TestLog::EndMessage;

	for (int ndx = 0; ndx < (int)outLines.size(); ++ndx)
	{
		const deUint32	multiplierInt	= m_renderSize / 2;
		const deUint32	multiplierFrac	= subPixels;
		std::string		coordsString;

		for (size_t vertexNdx = 0; vertexNdx < 2; ++vertexNdx)
		{
			const tcu::Vec4&	pos				= outLines[ndx].positions[vertexNdx];
			std::ostringstream	coordsFloat;
			std::ostringstream	coordsNatural;

			for (int coordNdx = 0; coordNdx < 2; ++coordNdx)
			{
				const char*	sep		= (coordNdx < 1) ? "," : "";
				const float	coord	= pos[coordNdx];
				const char	sign	= deSign(coord) < 0 ? '-' : '+';
				const float	m		= deFloatFloor(float(multiplierInt) * deFloatAbs(coord));
				const float	r		= deFloatFrac(float(multiplierInt) * deFloatAbs(coord)) * float(multiplierFrac);

				coordsFloat << std::fixed << std::setw(13) << std::setprecision(10) << coord << sep;
				coordsNatural << sign << '(' << m << '+' << r << '/' << multiplierFrac << ')' << sep;
			}

			coordsString += "\n\t[" + coordsFloat.str() + "] == [" + coordsNatural.str() + "] / " + de::toString(multiplierInt);
		}

		m_context.getTestContext().getLog()
			<< tcu::TestLog::Message
			<< "Line " << (ndx + 1) << ':'
			<< coordsString
			<< tcu::TestLog::EndMessage;
	}
}

void ConservativeLineTestInstance::drawPrimitives (tcu::Surface& result, const std::vector<tcu::Vec4>& vertexData, VkPrimitiveTopology primitiveTopology)
{
	if (m_conservativeTestConfig.degeneratePrimitives)
	{
		// Set provoking vertex color to white
		tcu::Vec4				colorProvoking	(1.0f, 1.0f, 1.0f, 1.0f);
		tcu::Vec4				colorOther		(0.0f, 1.0f, 1.0f, 1.0f);
		std::vector<tcu::Vec4>	colorData;

		colorData.reserve(vertexData.size());

		for (size_t vertexNdx = 0; vertexNdx < vertexData.size(); ++vertexNdx)
			if (vertexNdx % 2 == 0)
				colorData.push_back(colorProvoking);
			else
				colorData.push_back(colorOther);

		BaseRenderingTestInstance::drawPrimitives(result, vertexData, colorData, primitiveTopology);
	}
	else
		BaseRenderingTestInstance::drawPrimitives(result, vertexData, primitiveTopology);
}

bool ConservativeLineTestInstance::compareAndVerify (std::vector<LineSceneSpec::SceneLine>& lines, tcu::Surface& resultImage, std::vector<tcu::Vec4>& drawBuffer)
{
	DE_UNREF(drawBuffer);

	switch (m_conservativeTestConfig.conservativeRasterizationMode)
	{
		case VK_CONSERVATIVE_RASTERIZATION_MODE_OVERESTIMATE_EXT:
		{
			if (m_conservativeTestConfig.degeneratePrimitives)
				return compareAndVerifyOverestimatedDegenerate(lines, resultImage);
			else
				return compareAndVerifyOverestimatedNormal(lines, resultImage);

			break;
		}
		case VK_CONSERVATIVE_RASTERIZATION_MODE_UNDERESTIMATE_EXT:
		{
			if (m_conservativeTestConfig.degeneratePrimitives)
				return compareAndVerifyUnderestimatedDegenerate(lines, resultImage);
			else
				return compareAndVerifyUnderestimatedNormal(lines, resultImage);

			break;
		}

		default:
			TCU_THROW(InternalError, "Unknown conservative rasterization mode");
	}
}

bool ConservativeLineTestInstance::compareAndVerifyOverestimatedNormal (std::vector<LineSceneSpec::SceneLine>& lines, tcu::Surface& resultImage)
{
	DE_UNREF(lines);

	const int			b						= 3; // bar width
	const int			w						= resultImage.getWidth() - 1;
	const int			h						= resultImage.getHeight() - 1;
	const int			xStarts[]				= {     1,     1,     1 };
	const int			xEnds[]					= { w - 1,     b, w - 1 };
	const int			yStarts[]				= {     1,     1, h - b };
	const int			yEnds[]					= {     b, h - 1, h - 1 };
	const int			xStart					= xStarts[getIteration()];
	const int			xEnd					= xEnds[getIteration()];
	const int			yStart					= yStarts[getIteration()];
	const int			yEnd					= yEnds[getIteration()];
	const tcu::RGBA		backgroundColor			= tcu::RGBA(0, 0, 0, 255);
	const tcu::RGBA		foregroundColor			= tcu::RGBA(255, 255, 255, 255);
	const tcu::RGBA		unexpectedPixelColor	= tcu::RGBA(255, 0, 0, 255);
	tcu::TestLog&		log						= m_context.getTestContext().getLog();
	int					errX					= 0;
	int					errY					= 0;
	deUint32			errValue				= 0;
	bool				result					= true;

	DE_ASSERT(resultImage.getHeight() == resultImage.getWidth());

	for (int y = yStart; result && y < yEnd; ++y)
	for (int x = xStart; result && x < xEnd; ++x)
	{
		if (resultImage.getPixel(x,y).getPacked() != foregroundColor.getPacked())
		{
			result		= false;
			errX		= x;
			errY		= y;
			errValue	= resultImage.getPixel(x,y).getPacked();

			break;
		}
	}

	if (!result)
	{
		tcu::Surface	errorMask	(resultImage.getWidth(), resultImage.getHeight());

		for (int y = 0; y < errorMask.getHeight(); ++y)
		for (int x = 0; x < errorMask.getWidth(); ++x)
			errorMask.setPixel(x, y, backgroundColor);

		for (int y = yStart; y < yEnd; ++y)
		for (int x = xStart; x < xEnd; ++x)
		{
			if (resultImage.getPixel(x,y).getPacked() != foregroundColor.getPacked())
				errorMask.setPixel(x,y, unexpectedPixelColor);
		}

		log << tcu::TestLog::Message << "Invalid pixels found starting at " << errX << "," << errY << " value=0x" << std::hex << errValue
			<< tcu::TestLog::EndMessage;
		log << tcu::TestLog::ImageSet("Verification result", "Result of rendering")
			<< tcu::TestLog::Image("Result",	"Result",		resultImage)
			<< tcu::TestLog::Image("ErrorMask", "ErrorMask",	errorMask)
			<< tcu::TestLog::EndImageSet;
	}
	else
	{
		log << tcu::TestLog::Message << "No invalid pixels found." << tcu::TestLog::EndMessage;
		log << tcu::TestLog::ImageSet("Verification result", "Result of rendering")
			<< tcu::TestLog::Image("Result", "Result", resultImage)
			<< tcu::TestLog::EndImageSet;
	}

	return result;
}

bool ConservativeLineTestInstance::compareAndVerifyOverestimatedDegenerate (std::vector<LineSceneSpec::SceneLine>& lines, tcu::Surface& resultImage)
{
	DE_UNREF(lines);

	const char*			iterationComments[]		= { "Horizontal line", "Vertical line", "Diagonal line" };
	const tcu::RGBA		backgroundColor			= tcu::RGBA(0, 0, 0, 255);
	const tcu::RGBA		foregroundColor			= tcu::RGBA(255, 255, 255, 255);
	const tcu::RGBA		unexpectedPixelColor	= tcu::RGBA(255, 0, 0, 255);
	tcu::TestLog&		log						= m_context.getTestContext().getLog();
	bool				result					= true;
	tcu::Surface		referenceImage			(resultImage.getWidth(), resultImage.getHeight());

	for (int y = 0; y < resultImage.getHeight(); ++y)
	for (int x = 0; x < resultImage.getWidth(); ++x)
		referenceImage.setPixel(x, y, backgroundColor);

	if (m_conservativeRasterizationProperties.degenerateLinesRasterized)
	{
		log << tcu::TestLog::Message << "Lines expected to be rasterized with white color" << tcu::TestLog::EndMessage;

		// This pixel will alway be covered due to the placement of the line.
		referenceImage.setPixel(1, 1, foregroundColor);

		// Additional pixels will be covered based on the extra bloat added to the primitive.
		const float extraOverestimation = getExtraOverestimationSize(m_conservativeTestConfig.extraOverestimationSize, m_conservativeRasterizationProperties);
		const int xExtent = 1 + int((extraOverestimation * 2.0f) + 0.5f);
		const int yExtent = xExtent;

		for (int y = 0; y <= yExtent; ++y)
		for (int x = 0; x <= xExtent; ++x)
			referenceImage.setPixel(x, y, foregroundColor);
	}
	else
		log << tcu::TestLog::Message << "Lines expected to be culled" << tcu::TestLog::EndMessage;

	for (int y = 0; result && y < resultImage.getHeight(); ++y)
	for (int x = 0; result && x < resultImage.getWidth(); ++x)
	{
		if (resultImage.getPixel(x, y).getPacked() != referenceImage.getPixel(x, y).getPacked())
		{
			result = false;

			break;
		}
	}

	if (!result)
	{
		tcu::Surface	errorMask	(resultImage.getWidth(), resultImage.getHeight());

		for (int y = 0; y < errorMask.getHeight(); ++y)
		for (int x = 0; x < errorMask.getWidth(); ++x)
		{
			if (resultImage.getPixel(x, y).getPacked() != referenceImage.getPixel(x, y).getPacked())
				errorMask.setPixel(x, y, unexpectedPixelColor);
			else
				errorMask.setPixel(x, y, backgroundColor);
		}

		log << tcu::TestLog::Message << "Invalid pixels found for mode " << iterationComments[getIteration()]
			<< tcu::TestLog::EndMessage;
		log << tcu::TestLog::ImageSet("Verification result", "Result of rendering")
			<< tcu::TestLog::Image("Result",	"Result",		resultImage)
			<< tcu::TestLog::Image("Reference",	"Reference",	referenceImage)
			<< tcu::TestLog::Image("ErrorMask", "ErrorMask",	errorMask)
			<< tcu::TestLog::EndImageSet;
	}
	else
	{
		log << tcu::TestLog::Message << "No invalid pixels found." << tcu::TestLog::EndMessage;
		log << tcu::TestLog::ImageSet("Verification result", "Result of rendering")
			<< tcu::TestLog::Image("Result", "Result", resultImage)
			<< tcu::TestLog::EndImageSet;
	}

	return result;
}

bool ConservativeLineTestInstance::compareAndVerifyUnderestimatedNormal (std::vector<LineSceneSpec::SceneLine>& lines, tcu::Surface& resultImage)
{
	DE_UNREF(lines);

	const tcu::RGBA		backgroundColor			= tcu::RGBA(0, 0, 0, 255);
	const tcu::RGBA		foregroundColor			= tcu::RGBA(255, 255, 255, 255);
	const tcu::RGBA		unexpectedPixelColor	= tcu::RGBA(255, 0, 0, 255);
	tcu::TestLog&		log						= m_context.getTestContext().getLog();
	int					errX					= -1;
	int					errY					= -1;
	tcu::RGBA			errValue;
	bool				result					= true;
	tcu::Surface		referenceImage			(resultImage.getWidth(), resultImage.getHeight());

	DE_ASSERT(resultImage.getHeight() == resultImage.getWidth());

	for (int y = 0; y < referenceImage.getHeight(); ++y)
	for (int x = 0; x < referenceImage.getWidth(); ++x)
		referenceImage.setPixel(x, y, backgroundColor);

	if (getLineWidth() > 1.0f)
	{
		const tcu::IVec2	viewportSize(resultImage.getWidth(), resultImage.getHeight());

		for (size_t lineNdx = 0; lineNdx < lines.size(); ++lineNdx)
		for (int y = 0; y < resultImage.getHeight(); ++y)
		for (int x = 0; x < resultImage.getWidth(); ++x)
		{
			if (calculateUnderestimateLineCoverage(lines[lineNdx].positions[0], lines[lineNdx].positions[1], getLineWidth(), tcu::IVec2(x,y), viewportSize) == tcu::COVERAGE_FULL)
				referenceImage.setPixel(x, y, foregroundColor);
		}
	}

	for (int y = 0; result && y < resultImage.getHeight(); ++y)
	for (int x = 0; result && x < resultImage.getWidth(); ++x)
	{
		if (resultImage.getPixel(x,y).getPacked() != referenceImage.getPixel(x,y).getPacked())
		{
			result		= false;
			errX		= x;
			errY		= y;
			errValue	= resultImage.getPixel(x,y);

			break;
		}
	}

	if (!result)
	{
		tcu::Surface	errorMask	(resultImage.getWidth(), resultImage.getHeight());

		for (int y = 0; y < errorMask.getHeight(); ++y)
		for (int x = 0; x < errorMask.getWidth(); ++x)
			errorMask.setPixel(x, y, backgroundColor);

		for (int y = 0; y < errorMask.getHeight(); ++y)
		for (int x = 0; x < errorMask.getWidth();  ++x)
		{
			if (resultImage.getPixel(x,y).getPacked() != referenceImage.getPixel(x,y).getPacked())
				errorMask.setPixel(x, y, unexpectedPixelColor);
		}

		log << tcu::TestLog::Message << "Invalid pixels found starting at " << errX << "," << errY << " errValue=" << errValue
			<< tcu::TestLog::EndMessage;
		log << tcu::TestLog::ImageSet("Verification result", "Result of rendering")
			<< tcu::TestLog::Image("Result",	"Result",		resultImage)
			<< tcu::TestLog::Image("Reference",	"Reference",	referenceImage)
			<< tcu::TestLog::Image("ErrorMask", "ErrorMask",	errorMask)
			<< tcu::TestLog::EndImageSet;
	}
	else
	{
		log << tcu::TestLog::Message << "No invalid pixels found." << tcu::TestLog::EndMessage;
		log << tcu::TestLog::ImageSet("Verification result", "Result of rendering")
			<< tcu::TestLog::Image("Result", "Result", resultImage)
			<< tcu::TestLog::EndImageSet;
	}

	return result;
}

bool ConservativeLineTestInstance::compareAndVerifyUnderestimatedDegenerate (std::vector<LineSceneSpec::SceneLine>& lines, tcu::Surface& resultImage)
{
	DE_UNREF(lines);

	const tcu::RGBA		backgroundColor			= tcu::RGBA(0, 0, 0, 255);
	const tcu::RGBA		unexpectedPixelColor	= tcu::RGBA(255, 0, 0, 255);
	tcu::TestLog&		log						= m_context.getTestContext().getLog();
	bool				result					= true;
	tcu::Surface		referenceImage			(resultImage.getWidth(), resultImage.getHeight());

	for (int y = 0; y < resultImage.getHeight(); ++y)
	for (int x = 0; x < resultImage.getWidth(); ++x)
		referenceImage.setPixel(x, y, backgroundColor);

	log << tcu::TestLog::Message << "No lines expected to be rasterized" << tcu::TestLog::EndMessage;

	for (int y = 0; result && y < resultImage.getHeight(); ++y)
	for (int x = 0; result && x < resultImage.getWidth(); ++x)
	{
		if (resultImage.getPixel(x,y).getPacked() != referenceImage.getPixel(x,y).getPacked())
		{
			result = false;

			break;
		}
	}

	if (!result)
	{
		tcu::Surface	errorMask	(resultImage.getWidth(), resultImage.getHeight());

		for (int y = 0; y < errorMask.getHeight(); ++y)
		for (int x = 0; x < errorMask.getWidth(); ++x)
		{
			if (resultImage.getPixel(x, y).getPacked() != referenceImage.getPixel(x, y).getPacked())
				errorMask.setPixel(x, y, unexpectedPixelColor);
			else
				errorMask.setPixel(x, y, backgroundColor);
		}

		log << tcu::TestLog::Message << "Invalid pixels found" << tcu::TestLog::EndMessage;
		log << tcu::TestLog::ImageSet("Verification result", "Result of rendering")
			<< tcu::TestLog::Image("Result",	"Result",		resultImage)
			<< tcu::TestLog::Image("Reference",	"Reference",	referenceImage)
			<< tcu::TestLog::Image("ErrorMask", "ErrorMask",	errorMask)
			<< tcu::TestLog::EndImageSet;
	}
	else
	{
		log << tcu::TestLog::Message << "No invalid pixels found." << tcu::TestLog::EndMessage;
		log << tcu::TestLog::ImageSet("Verification result", "Result of rendering")
			<< tcu::TestLog::Image("Result", "Result", resultImage)
			<< tcu::TestLog::EndImageSet;
	}

	return result;
}

const std::vector<VkPipelineRasterizationConservativeStateCreateInfoEXT> ConservativeLineTestInstance::initRasterizationConservativeStateCreateInfo (void)
{
	const float															extraOverestimationSize	= getExtraOverestimationSize(m_conservativeTestConfig.extraOverestimationSize, m_conservativeRasterizationProperties);
	std::vector<VkPipelineRasterizationConservativeStateCreateInfoEXT>	result;

	result.reserve(getIterationCount());

	for (int iteration = 0; iteration < getIterationCount(); ++iteration)
	{
		const VkPipelineRasterizationConservativeStateCreateInfoEXT	rasterizationConservativeStateCreateInfo	=
		{
			VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_CONSERVATIVE_STATE_CREATE_INFO_EXT,	//  VkStructureType											sType;
			DE_NULL,																		//  const void*												pNext;
			(VkPipelineRasterizationConservativeStateCreateFlagsEXT)0,						//  VkPipelineRasterizationConservativeStateCreateFlagsEXT	flags;
			m_conservativeTestConfig.conservativeRasterizationMode,							//  VkConservativeRasterizationModeEXT						conservativeRasterizationMode;
			extraOverestimationSize															//  float													extraPrimitiveOverestimationSize;
		};

		result.push_back(rasterizationConservativeStateCreateInfo);
	}

	return result;
}

const std::vector<VkPipelineRasterizationStateCreateInfo> ConservativeLineTestInstance::initRasterizationStateCreateInfo (void)
{
	std::vector<VkPipelineRasterizationStateCreateInfo>	result;

	result.reserve(getIterationCount());

	for (int iteration = 0; iteration < getIterationCount(); ++iteration)
	{
		const VkPipelineRasterizationStateCreateInfo	rasterizationStateCreateInfo	=
		{
			VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,		//  VkStructureType							sType;
			&m_rasterizationConservativeStateCreateInfo[iteration],			//  const void*								pNext;
			0,																//  VkPipelineRasterizationStateCreateFlags	flags;
			false,															//  VkBool32								depthClampEnable;
			false,															//  VkBool32								rasterizerDiscardEnable;
			VK_POLYGON_MODE_FILL,											//  VkPolygonMode							polygonMode;
			VK_CULL_MODE_NONE,												//  VkCullModeFlags							cullMode;
			VK_FRONT_FACE_COUNTER_CLOCKWISE,								//  VkFrontFace								frontFace;
			VK_FALSE,														//  VkBool32								depthBiasEnable;
			0.0f,															//  float									depthBiasConstantFactor;
			0.0f,															//  float									depthBiasClamp;
			0.0f,															//  float									depthBiasSlopeFactor;
			getLineWidth(),													//  float									lineWidth;
		};

		result.push_back(rasterizationStateCreateInfo);
	}

	return result;
}

const VkPipelineRasterizationStateCreateInfo* ConservativeLineTestInstance::getRasterizationStateCreateInfo	(void) const
{
	return &m_rasterizationStateCreateInfo[getIteration()];
}

const VkPipelineRasterizationLineStateCreateInfoEXT* ConservativeLineTestInstance::getLineRasterizationStateCreateInfo	(void)
{
	return DE_NULL;
}


class ConservativePointTestInstance : public PointTestInstance
{
public:
																				ConservativePointTestInstance					(Context&				context,
																																 ConservativeTestConfig	conservativeTestConfig,
																																 VkSampleCountFlagBits	sampleCount)
																					: PointTestInstance								(
																																		context,
																																		PRIMITIVEWIDENESS_NARROW,
																																		PRIMITIVESTRICTNESS_IGNORE,
																																		sampleCount,
																																		LINESTIPPLE_DISABLED,
																																		VK_LINE_RASTERIZATION_MODE_DEFAULT_EXT,
																																		0,
																																		conservativeTestConfig.resolution,
																																		conservativeTestConfig.lineWidth
																																	)
																					, m_conservativeTestConfig						(conservativeTestConfig)
																					, m_conservativeRasterizationProperties			(context.getConservativeRasterizationPropertiesEXT())
																					, m_rasterizationConservativeStateCreateInfo	(initRasterizationConservativeStateCreateInfo())
																					, m_rasterizationStateCreateInfo				(initRasterizationStateCreateInfo())
																					, m_renderStart									()
																					, m_renderEnd									()
																				{}

	void																		generatePoints									(int										iteration,
																																 std::vector<tcu::Vec4>&					outData,
																																 std::vector<PointSceneSpec::ScenePoint>&	outPoints);
	const VkPipelineRasterizationStateCreateInfo*								getRasterizationStateCreateInfo					(void) const;

protected:
	virtual const VkPipelineRasterizationLineStateCreateInfoEXT*				getLineRasterizationStateCreateInfo				(void);

	virtual bool																compareAndVerify								(std::vector<PointSceneSpec::ScenePoint>&	points,
																																 tcu::Surface&								resultImage,
																																 std::vector<tcu::Vec4>&					drawBuffer);
	virtual bool																compareAndVerifyOverestimated					(std::vector<PointSceneSpec::ScenePoint>&	points,
																																 tcu::Surface&								resultImage);
	virtual bool																compareAndVerifyUnderestimated					(std::vector<PointSceneSpec::ScenePoint>&	points,
																																 tcu::Surface&								resultImage);

private:
	const std::vector<VkPipelineRasterizationConservativeStateCreateInfoEXT>	initRasterizationConservativeStateCreateInfo	(void);
	const std::vector<VkPipelineRasterizationStateCreateInfo>					initRasterizationStateCreateInfo				(void);

	const ConservativeTestConfig												m_conservativeTestConfig;
	const VkPhysicalDeviceConservativeRasterizationPropertiesEXT				m_conservativeRasterizationProperties;
	const std::vector<VkPipelineRasterizationConservativeStateCreateInfoEXT>	m_rasterizationConservativeStateCreateInfo;
	const std::vector<VkPipelineRasterizationStateCreateInfo>					m_rasterizationStateCreateInfo;
	std::vector<int>															m_renderStart;
	std::vector<int>															m_renderEnd;
};

void ConservativePointTestInstance::generatePoints (int iteration, std::vector<tcu::Vec4>& outData, std::vector<PointSceneSpec::ScenePoint>& outPoints)
{
	const float	pixelSize		= 2.0f / float(m_renderSize);
	const bool	isOverestimate	= m_conservativeTestConfig.conservativeRasterizationMode == VK_CONSERVATIVE_RASTERIZATION_MODE_OVERESTIMATE_EXT;

	m_renderStart.clear();
	m_renderEnd.clear();

	if (isOverestimate)
	{
		const float	extraOverestimationSize	= getExtraOverestimationSize(m_conservativeTestConfig.extraOverestimationSize, m_conservativeRasterizationProperties);
		const float	overestimate			= m_conservativeRasterizationProperties.primitiveOverestimationSize + extraOverestimationSize;
		const float	halfRenderAreaSize		= overestimate + 0.5f;
		const float	pointCenterOffset		= 2.0f + 0.5f * float(iteration) + halfRenderAreaSize;
		const float	pointEdgeStart			= pointCenterOffset - halfRenderAreaSize;
		const float	pointEdgeEnd			= pointEdgeStart + 2 * halfRenderAreaSize;
		const int	renderStart				= int(deFloatFloor(pointEdgeStart)) + int((deFloatFrac(pointEdgeStart) > 0.0f) ? 0 : -1);
		const int	renderEnd				= int(deFloatCeil(pointEdgeEnd)) + int((deFloatFrac(pointEdgeEnd) > 0.0f) ? 0 : 1);

		outData.push_back(tcu::Vec4(-1.0f + pixelSize * pointCenterOffset, -1.0f + pixelSize * pointCenterOffset, 0.0f, 1.0f));

		m_renderStart.push_back(renderStart);
		m_renderEnd.push_back(renderEnd);
	}
	else
	{
		const float	pointSize			= m_conservativeTestConfig.lineWidth;
		const float	halfRenderAreaSize	= pointSize / 2.0f;

		switch (iteration)
		{
			case 0:
			{
				const float	pointCenterOffset	= (pointSize + 1.0f + deFloatFrac(pointSize)) / 2.0f;
				const float	pointEdgeStart		= pointCenterOffset - halfRenderAreaSize;
				const float	pointEdgeEnd		= pointEdgeStart + 2.0f * halfRenderAreaSize;
				const int	renderStart			= (m_renderSize / 2) + int(deFloatCeil(pointEdgeStart));
				const int	renderEnd			= (m_renderSize / 2) + int(deFloatFloor(pointEdgeEnd));

				outData.push_back(tcu::Vec4(pixelSize * pointCenterOffset, pixelSize * pointCenterOffset, 0.0f, 1.0f));

				m_renderStart.push_back(renderStart);
				m_renderEnd.push_back(renderEnd);

				break;
			}

			case 1:
			{
				const float subPixelSize		= 1.0f / float(1u<<(m_subpixelBits - 1));
				const float	pointBottomLeft		= 1.0f - subPixelSize;
				const float	pointCenterOffset	= pointBottomLeft + pointSize / 2.0f;
				const float	pointEdgeStart		= pointCenterOffset - halfRenderAreaSize;
				const float	pointEdgeEnd		= pointEdgeStart + 2.0f * halfRenderAreaSize;
				const int	renderStart			= (m_renderSize / 2) + int(deFloatCeil(pointEdgeStart));
				const int	renderEnd			= (m_renderSize / 2) + int(deFloatFloor(pointEdgeEnd));

				outData.push_back(tcu::Vec4(pixelSize * pointCenterOffset, pixelSize * pointCenterOffset, 0.0f, 1.0f));

				m_renderStart.push_back(renderStart);
				m_renderEnd.push_back(renderEnd);

				break;
			}

			case 2:
			{
				// Edges of a point are considered not covered. Top-left coverage rule is not applicable for underestimate rasterization.
				const float	pointCenterOffset	= (pointSize + deFloatFrac(pointSize)) / 2.0f;
				const float	pointEdgeStart		= pointCenterOffset - halfRenderAreaSize;
				const float	pointEdgeEnd		= pointEdgeStart + 2.0f * halfRenderAreaSize;
				const int	renderStart			= (m_renderSize / 2) + int(deFloatCeil(pointEdgeStart)) + 1;
				const int	renderEnd			= (m_renderSize / 2) + int(deFloatFloor(pointEdgeEnd)) - 1;

				outData.push_back(tcu::Vec4(pixelSize * pointCenterOffset, pixelSize * pointCenterOffset, 0.0f, 1.0f));

				m_renderStart.push_back(renderStart);
				m_renderEnd.push_back(renderEnd);

				break;
			}

			default:
				TCU_THROW(InternalError, "Unexpected iteration");
		}
	}

	outPoints.resize(outData.size());
	for (size_t ndx = 0; ndx < outPoints.size(); ++ndx)
	{
		outPoints[ndx].position = outData[ndx];
		outPoints[ndx].pointSize = getPointSize();
	}

	// log
	m_context.getTestContext().getLog()
		<< tcu::TestLog::Message
		<< "Testing conservative point rendering "
		<< "with rendering " << outPoints.size() << " points(s):"
		<< tcu::TestLog::EndMessage;
	for (int ndx = 0; ndx < (int)outPoints.size(); ++ndx)
	{
		const deUint32	 multiplier	= m_renderSize / 2;

		m_context.getTestContext().getLog()
			<< tcu::TestLog::Message
			<< "Point " << (ndx+1) << ":"
			<< "\n\t" << outPoints[ndx].position << " == " << (float(multiplier) * outPoints[ndx].position) << "/" << multiplier
			<< tcu::TestLog::EndMessage;
	}
}

bool ConservativePointTestInstance::compareAndVerify (std::vector<PointSceneSpec::ScenePoint>& points, tcu::Surface& resultImage, std::vector<tcu::Vec4>& drawBuffer)
{
	DE_UNREF(drawBuffer);

	switch (m_conservativeTestConfig.conservativeRasterizationMode)
	{
		case VK_CONSERVATIVE_RASTERIZATION_MODE_OVERESTIMATE_EXT:
		{
			return compareAndVerifyOverestimated(points, resultImage);
		}
		case VK_CONSERVATIVE_RASTERIZATION_MODE_UNDERESTIMATE_EXT:
		{
			return compareAndVerifyUnderestimated(points, resultImage);
		}

		default:
			TCU_THROW(InternalError, "Unknown conservative rasterization mode");
	}
}

bool ConservativePointTestInstance::compareAndVerifyOverestimated (std::vector<PointSceneSpec::ScenePoint>& points, tcu::Surface& resultImage)
{
	DE_UNREF(points);

	const char*			iterationComments[]		= { "Edges and corners", "Partial coverage", "Edges and corners" };
	const tcu::RGBA		backgroundColor			= tcu::RGBA(0, 0, 0, 255);
	const tcu::RGBA		foregroundColor			= tcu::RGBA(255, 255, 255, 255);
	const tcu::RGBA		unexpectedPixelColor	= tcu::RGBA(255, 0, 0, 255);
	tcu::TestLog&		log						= m_context.getTestContext().getLog();
	int					errX					= 0;
	int					errY					= 0;
	deUint32			errValue				= 0;
	bool				result					= true;

	log << tcu::TestLog::Message << "Points expected to be rasterized with white color" << tcu::TestLog::EndMessage;
	log << tcu::TestLog::Message << "Testing " << iterationComments[getIteration()] << tcu::TestLog::EndMessage;

	for (size_t renderAreaNdx = 0; result && renderAreaNdx < m_renderStart.size(); ++renderAreaNdx)
	{
		const int renderStart	= m_renderStart[renderAreaNdx];
		const int renderEnd		= m_renderEnd[renderAreaNdx];

		for (int y = renderStart; result && y < renderEnd; ++y)
		for (int x = renderStart; result && x < renderEnd; ++x)
		{
			if (resultImage.getPixel(x,y).getPacked() != foregroundColor.getPacked())
			{
				result		= false;
				errX		= x;
				errY		= y;
				errValue	= resultImage.getPixel(x, y).getPacked();

				break;
			}
		}
	}

	if (!result)
	{
		tcu::Surface		referenceImage	(resultImage.getWidth(), resultImage.getHeight());
		tcu::Surface		errorMask		(resultImage.getWidth(), resultImage.getHeight());
		std::ostringstream	css;

		for (int y = 0; y < resultImage.getHeight(); ++y)
		for (int x = 0; x < resultImage.getWidth(); ++x)
			referenceImage.setPixel(x, y, backgroundColor);

		for (size_t renderAreaNdx = 0; renderAreaNdx < m_renderStart.size(); ++renderAreaNdx)
		{
			const int renderStart	= m_renderStart[renderAreaNdx];
			const int renderEnd		= m_renderEnd[renderAreaNdx];

			for (int y = renderStart; y < renderEnd; ++y)
			for (int x = renderStart; x < renderEnd; ++x)
				referenceImage.setPixel(x, y, foregroundColor);
		}

		for (int y = 0; y < errorMask.getHeight(); ++y)
		for (int x = 0; x < errorMask.getWidth(); ++x)
		{
			if (resultImage.getPixel(x, y).getPacked() != referenceImage.getPixel(x, y).getPacked())
				errorMask.setPixel(x, y, unexpectedPixelColor);
			else
				errorMask.setPixel(x, y, backgroundColor);
		}

		css << std::endl;
		for (size_t renderAreaNdx = 0; renderAreaNdx < m_renderStart.size(); ++renderAreaNdx)
		{
			const int renderStart	= m_renderStart[renderAreaNdx];
			const int renderEnd		= m_renderEnd[renderAreaNdx];

			css << "[" << renderStart << "," << renderEnd << ") x [" << renderStart << "," << renderEnd << ")" << std::endl;
		}

		log << tcu::TestLog::Message << "Invalid pixels found starting at " << errX << "," << errY << " value=0x" << std::hex << errValue
			<< tcu::TestLog::EndMessage;
		log << tcu::TestLog::Message << "Expected area(s) to be filled:" << css.str()
			<< tcu::TestLog::EndMessage;
		log << tcu::TestLog::ImageSet("Verification result", "Result of rendering")
			<< tcu::TestLog::Image("Result",	"Result",		resultImage)
			<< tcu::TestLog::Image("Reference",	"Reference",	referenceImage)
			<< tcu::TestLog::Image("ErrorMask", "ErrorMask",	errorMask)
			<< tcu::TestLog::EndImageSet;
	}
	else
	{
		log << tcu::TestLog::Message << "No invalid pixels found." << tcu::TestLog::EndMessage;
		log << tcu::TestLog::ImageSet("Verification result", "Result of rendering")
			<< tcu::TestLog::Image("Result", "Result", resultImage)
			<< tcu::TestLog::EndImageSet;
	}

	return result;
}

bool ConservativePointTestInstance::compareAndVerifyUnderestimated (std::vector<PointSceneSpec::ScenePoint>& points, tcu::Surface& resultImage)
{
	DE_UNREF(points);

	const char*			iterationComments[]		= { "Full coverage", "Full coverage with subpixel", "Exact coverage" };
	const tcu::RGBA		backgroundColor			= tcu::RGBA(0, 0, 0, 255);
	const tcu::RGBA		foregroundColor			= tcu::RGBA(255, 255, 255, 255);
	const tcu::RGBA		unexpectedPixelColor	= tcu::RGBA(255, 0, 0, 255);
	tcu::TestLog&		log						= m_context.getTestContext().getLog();
	int					errX					= 0;
	int					errY					= 0;
	deUint32			errValue				= 0;
	bool				result					= true;
	tcu::Surface		referenceImage			(resultImage.getWidth(), resultImage.getHeight());

	log << tcu::TestLog::Message << "Points expected to be rasterized with white color" << tcu::TestLog::EndMessage;
	log << tcu::TestLog::Message << "Testing " << iterationComments[getIteration()] << tcu::TestLog::EndMessage;

	for (int y = 0; y < resultImage.getHeight(); ++y)
	for (int x = 0; x < resultImage.getWidth(); ++x)
		referenceImage.setPixel(x, y, backgroundColor);

	for (size_t renderAreaNdx = 0; result && renderAreaNdx < m_renderStart.size(); ++renderAreaNdx)
	{
		const int renderStart	= m_renderStart[renderAreaNdx];
		const int renderEnd		= m_renderEnd[renderAreaNdx];

		for (int y = renderStart; y < renderEnd; ++y)
		for (int x = renderStart; x < renderEnd; ++x)
			referenceImage.setPixel(x, y, foregroundColor);
	}

	for (int y = 0; result && y < resultImage.getHeight(); ++y)
	for (int x = 0; result && x < resultImage.getWidth(); ++x)
	{
		if (resultImage.getPixel(x, y).getPacked() != referenceImage.getPixel(x, y).getPacked())
		{
			result		= false;
			errX		= x;
			errY		= y;
			errValue	= resultImage.getPixel(x, y).getPacked();

			break;
		}
	}

	if (!result)
	{
		tcu::Surface		errorMask	(resultImage.getWidth(), resultImage.getHeight());
		std::ostringstream	css;

		for (int y = 0; y < errorMask.getHeight(); ++y)
		for (int x = 0; x < errorMask.getWidth(); ++x)
		{
			if (resultImage.getPixel(x, y).getPacked() != referenceImage.getPixel(x, y).getPacked())
				errorMask.setPixel(x, y, unexpectedPixelColor);
			else
				errorMask.setPixel(x, y, backgroundColor);
		}

		css << std::endl;
		for (size_t renderAreaNdx = 0; renderAreaNdx < m_renderStart.size(); ++renderAreaNdx)
		{
			const int renderStart	= m_renderStart[renderAreaNdx];
			const int renderEnd		= m_renderEnd[renderAreaNdx];

			css << "[" << renderStart << "," << renderEnd << ") x [" << renderStart << "," << renderEnd << ")" << std::endl;
		}

		log << tcu::TestLog::Message << "Invalid pixels found starting at " << errX << "," << errY << " value=0x" << std::hex << errValue
			<< tcu::TestLog::EndMessage;
		log << tcu::TestLog::Message << "Expected area(s) to be filled:" << css.str()
			<< tcu::TestLog::EndMessage;
		log << tcu::TestLog::ImageSet("Verification result", "Result of rendering")
			<< tcu::TestLog::Image("Result",	"Result",		resultImage)
			<< tcu::TestLog::Image("Reference",	"Reference",	referenceImage)
			<< tcu::TestLog::Image("ErrorMask", "ErrorMask",	errorMask)
			<< tcu::TestLog::EndImageSet;
	}
	else
	{
		log << tcu::TestLog::Message << "No invalid pixels found." << tcu::TestLog::EndMessage;
		log << tcu::TestLog::ImageSet("Verification result", "Result of rendering")
			<< tcu::TestLog::Image("Result", "Result", resultImage)
			<< tcu::TestLog::EndImageSet;
	}

	return result;
}

const std::vector<VkPipelineRasterizationConservativeStateCreateInfoEXT> ConservativePointTestInstance::initRasterizationConservativeStateCreateInfo (void)
{
	const float															extraOverestimationSize	= getExtraOverestimationSize(m_conservativeTestConfig.extraOverestimationSize, m_conservativeRasterizationProperties);
	std::vector<VkPipelineRasterizationConservativeStateCreateInfoEXT>	result;

	result.reserve(getIterationCount());

	for (int iteration = 0; iteration < getIterationCount(); ++iteration)
	{
		const VkPipelineRasterizationConservativeStateCreateInfoEXT	rasterizationConservativeStateCreateInfo	=
		{
			VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_CONSERVATIVE_STATE_CREATE_INFO_EXT,	//  VkStructureType											sType;
			DE_NULL,																		//  const void*												pNext;
			(VkPipelineRasterizationConservativeStateCreateFlagsEXT)0,						//  VkPipelineRasterizationConservativeStateCreateFlagsEXT	flags;
			m_conservativeTestConfig.conservativeRasterizationMode,							//  VkConservativeRasterizationModeEXT						conservativeRasterizationMode;
			extraOverestimationSize															//  float													extraPrimitiveOverestimationSize;
		};

		result.push_back(rasterizationConservativeStateCreateInfo);
	}

	return result;
}

const std::vector<VkPipelineRasterizationStateCreateInfo> ConservativePointTestInstance::initRasterizationStateCreateInfo (void)
{
	std::vector<VkPipelineRasterizationStateCreateInfo>	result;

	result.reserve(getIterationCount());

	for (int iteration = 0; iteration < getIterationCount(); ++iteration)
	{
		const VkPipelineRasterizationStateCreateInfo	rasterizationStateCreateInfo	=
		{
			VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,		//  VkStructureType							sType;
			&m_rasterizationConservativeStateCreateInfo[iteration],			//  const void*								pNext;
			0,																//  VkPipelineRasterizationStateCreateFlags	flags;
			false,															//  VkBool32								depthClampEnable;
			false,															//  VkBool32								rasterizerDiscardEnable;
			VK_POLYGON_MODE_FILL,											//  VkPolygonMode							polygonMode;
			VK_CULL_MODE_NONE,												//  VkCullModeFlags							cullMode;
			VK_FRONT_FACE_COUNTER_CLOCKWISE,								//  VkFrontFace								frontFace;
			VK_FALSE,														//  VkBool32								depthBiasEnable;
			0.0f,															//  float									depthBiasConstantFactor;
			0.0f,															//  float									depthBiasClamp;
			0.0f,															//  float									depthBiasSlopeFactor;
			0.0f,															//  float									lineWidth;
		};

		result.push_back(rasterizationStateCreateInfo);
	}

	return result;
}

const VkPipelineRasterizationStateCreateInfo* ConservativePointTestInstance::getRasterizationStateCreateInfo	(void) const
{
	return &m_rasterizationStateCreateInfo[getIteration()];
}

const VkPipelineRasterizationLineStateCreateInfoEXT* ConservativePointTestInstance::getLineRasterizationStateCreateInfo	(void)
{
	return DE_NULL;
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
	void						checkSupport		(Context& context) const;
protected:
	const VkCullModeFlags		m_cullMode;
	const VkPrimitiveTopology	m_primitiveTopology;
	const VkFrontFace			m_frontFace;
	const VkPolygonMode			m_polygonMode;
};

void CullingTestCase::checkSupport (Context& context) const
{
	if (context.isDeviceFunctionalitySupported("VK_KHR_portability_subset"))
	{
		const VkPhysicalDevicePortabilitySubsetFeaturesKHR& subsetFeatures = context.getPortabilitySubsetFeatures();
		if (m_polygonMode == VK_POLYGON_MODE_POINT && !subsetFeatures.pointPolygons)
			TCU_THROW(NotSupportedError, "VK_KHR_portability_subset: Point polygons are not supported by this implementation");
		if (m_primitiveTopology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN && !subsetFeatures.triangleFans)
			TCU_THROW(NotSupportedError, "VK_KHR_portability_subset: Triangle fans are not supported by this implementation");
	}
}

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
	void													drawPrimitivesDiscard				(tcu::Surface& result, const std::vector<tcu::Vec4>& positionData, VkPrimitiveTopology primitiveTopology, Move<VkQueryPool>& queryPool);

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

		drawPrimitivesDiscard(resultImage, drawBuffer, m_primitiveTopology, queryPool);
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

void DiscardTestInstance::drawPrimitivesDiscard (tcu::Surface& result, const std::vector<tcu::Vec4>& positionData, VkPrimitiveTopology primitiveTopology, Move<VkQueryPool>& queryPool)
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

									if (m_primitiveTopology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN &&
											context.isDeviceFunctionalitySupported("VK_KHR_portability_subset") &&
											!context.getPortabilitySubsetFeatures().triangleFans)
										TCU_THROW(NotSupportedError, "VK_KHR_portability_subset: Triangle fans are not supported by this implementation");
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

	virtual	void				checkSupport		(Context& context) const
								{
									if (m_primitiveTopology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN &&
										context.isDeviceFunctionalitySupported("VK_KHR_portability_subset") &&
										!context.getPortabilitySubsetFeatures().triangleFans)
									{
										TCU_THROW(NotSupportedError, "VK_KHR_portability_subset: Triangle fans are not supported by this implementation");
									}
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

	// .conservative
	{
		typedef struct
		{
			float			size;
			const char*		name;
		} overestimateSizes;

		const overestimateSizes overestimateNormalSizes[]	=
		{
			{ 0.00f,			"0_00" },
			{ 0.25f,			"0_25" },
			{ 0.50f,			"0_50" },
			{ 0.75f,			"0_75" },
			{ 1.00f,			"1_00" },
			{ 2.00f,			"2_00" },
			{ 4.00f,			"4_00" },
			{ -TCU_INFINITY,	"min" },
			{ TCU_INFINITY,		"max" },
		};
		const overestimateSizes overestimateDegenerate[]	=
		{
			{ 0.00f,			"0_00" },
			{ 0.25f,			"0_25" },
			{ -TCU_INFINITY,	"min" },
			{ TCU_INFINITY,		"max" },
		};
		const overestimateSizes underestimateLineWidths[]	=
		{
			{ 0.50f,			"0_50" },
			{ 1.00f,			"1_00" },
			{ 1.50f,			"1_50" },
		};
		const overestimateSizes underestimatePointSizes[]	=
		{
			{ 1.00f,			"1_00" },
			{ 1.50f,			"1_50" },
			{ 2.00f,			"2_00" },
			{ 3.00f,			"3_00" },
			{ 4.00f,			"4_00" },
			{ 8.00f,			"8_00" },
		};
		const struct PrimitiveType
		{
			VkPrimitiveTopology	type;
			const char*			name;
		}
		primitiveTypes[] =
		{
			{ VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,	"triangles"		},
			{ VK_PRIMITIVE_TOPOLOGY_LINE_LIST,		"lines"			},
			{ VK_PRIMITIVE_TOPOLOGY_POINT_LIST,		"points"		}
		};
		const VkSampleCountFlagBits samples[] =
		{
			VK_SAMPLE_COUNT_1_BIT,
			VK_SAMPLE_COUNT_2_BIT,
			VK_SAMPLE_COUNT_4_BIT,
			VK_SAMPLE_COUNT_8_BIT,
			VK_SAMPLE_COUNT_16_BIT,
			VK_SAMPLE_COUNT_32_BIT,
			VK_SAMPLE_COUNT_64_BIT
		};

		tcu::TestCaseGroup* const conservative = new tcu::TestCaseGroup(testCtx, "conservative", "Conservative rasterization tests");

		rasterizationTests->addChild(conservative);

		{
			tcu::TestCaseGroup* const overestimate = new tcu::TestCaseGroup(testCtx, "overestimate", "Overestimate tests");

			conservative->addChild(overestimate);

			for (int samplesNdx = 0; samplesNdx < DE_LENGTH_OF_ARRAY(samples); ++samplesNdx)
			{
				const std::string samplesGroupName = "samples_" + de::toString(samples[samplesNdx]);

				tcu::TestCaseGroup* const samplesGroup = new tcu::TestCaseGroup(testCtx, samplesGroupName.c_str(), "Samples tests");

				overestimate->addChild(samplesGroup);

				for (int primitiveTypeNdx = 0; primitiveTypeNdx < DE_LENGTH_OF_ARRAY(primitiveTypes); ++primitiveTypeNdx)
				{
					tcu::TestCaseGroup* const primitiveGroup = new tcu::TestCaseGroup(testCtx, primitiveTypes[primitiveTypeNdx].name, "Primitive tests");

					samplesGroup->addChild(primitiveGroup);

					{
						tcu::TestCaseGroup* const normal = new tcu::TestCaseGroup(testCtx, "normal", "Normal conservative rasterization tests");

						primitiveGroup->addChild(normal);

						for (int overestimateSizesNdx = 0; overestimateSizesNdx < DE_LENGTH_OF_ARRAY(overestimateNormalSizes); ++overestimateSizesNdx)
						{
							const ConservativeTestConfig	config	=
							{
								VK_CONSERVATIVE_RASTERIZATION_MODE_OVERESTIMATE_EXT,	//  VkConservativeRasterizationModeEXT	conservativeRasterizationMode;
								overestimateNormalSizes[overestimateSizesNdx].size,		//  float								extraOverestimationSize;
								primitiveTypes[primitiveTypeNdx].type,					//  VkPrimitiveTopology					primitiveTopology;
								false,													//  bool								degeneratePrimitives;
								1.0f,													//  float								lineWidth;
								RESOLUTION_POT,											//  deUint32							resolution;
							};

							if (primitiveTypes[primitiveTypeNdx].type == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
								normal->addChild(new ConservativeTestCase<ConservativeTraingleTestInstance>	(testCtx,
																											 overestimateNormalSizes[overestimateSizesNdx].name,
																											 "Overestimate test, verify rasterization result",
																											 config,
																											 samples[samplesNdx]));

							if (primitiveTypes[primitiveTypeNdx].type == VK_PRIMITIVE_TOPOLOGY_LINE_LIST)
								normal->addChild(new ConservativeTestCase<ConservativeLineTestInstance>		(testCtx,
																											 overestimateNormalSizes[overestimateSizesNdx].name,
																											 "Overestimate test, verify rasterization result",
																											 config,
																											 samples[samplesNdx]));

							if (primitiveTypes[primitiveTypeNdx].type == VK_PRIMITIVE_TOPOLOGY_POINT_LIST)
								normal->addChild(new ConservativeTestCase<ConservativePointTestInstance>	(testCtx,
																											 overestimateNormalSizes[overestimateSizesNdx].name,
																											 "Overestimate test, verify rasterization result",
																											 config,
																											 samples[samplesNdx]));
						}
					}

					{
						tcu::TestCaseGroup* const degenerate = new tcu::TestCaseGroup(testCtx, "degenerate", "Degenerate primitives conservative rasterization tests");

						primitiveGroup->addChild(degenerate);

						for (int overestimateSizesNdx = 0; overestimateSizesNdx < DE_LENGTH_OF_ARRAY(overestimateDegenerate); ++overestimateSizesNdx)
						{
							const ConservativeTestConfig	config	=
							{
								VK_CONSERVATIVE_RASTERIZATION_MODE_OVERESTIMATE_EXT,	//  VkConservativeRasterizationModeEXT	conservativeRasterizationMode;
								overestimateDegenerate[overestimateSizesNdx].size,		//  float								extraOverestimationSize;
								primitiveTypes[primitiveTypeNdx].type,					//  VkPrimitiveTopology					primitiveTopology;
								true,													//  bool								degeneratePrimitives;
								1.0f,													//  float								lineWidth;
								64u,													//  deUint32							resolution;
							};

							if (primitiveTypes[primitiveTypeNdx].type == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
								degenerate->addChild(new ConservativeTestCase<ConservativeTraingleTestInstance>	(testCtx,
																												 overestimateDegenerate[overestimateSizesNdx].name,
																												 "Overestimate triangle test, verify rasterization result",
																												 config,
																												 samples[samplesNdx]));

							if (primitiveTypes[primitiveTypeNdx].type == VK_PRIMITIVE_TOPOLOGY_LINE_LIST)
								degenerate->addChild(new ConservativeTestCase<ConservativeLineTestInstance>		(testCtx,
																												 overestimateDegenerate[overestimateSizesNdx].name,
																												 "Overestimate line test, verify rasterization result",
																												 config,
																												 samples[samplesNdx]));
						}
					}
				}
			}
		}

		{
			tcu::TestCaseGroup* const underestimate = new tcu::TestCaseGroup(testCtx, "underestimate", "Underestimate tests");

			conservative->addChild(underestimate);

			for (int samplesNdx = 0; samplesNdx < DE_LENGTH_OF_ARRAY(samples); ++samplesNdx)
			{
				const std::string samplesGroupName = "samples_" + de::toString(samples[samplesNdx]);

				tcu::TestCaseGroup* const samplesGroup = new tcu::TestCaseGroup(testCtx, samplesGroupName.c_str(), "Samples tests");

				underestimate->addChild(samplesGroup);

				for (int primitiveTypeNdx = 0; primitiveTypeNdx < DE_LENGTH_OF_ARRAY(primitiveTypes); ++primitiveTypeNdx)
				{
					tcu::TestCaseGroup* const primitiveGroup = new tcu::TestCaseGroup(testCtx, primitiveTypes[primitiveTypeNdx].name, "Primitive tests");

					samplesGroup->addChild(primitiveGroup);

					{
						tcu::TestCaseGroup* const normal = new tcu::TestCaseGroup(testCtx, "normal", "Normal conservative rasterization tests");

						primitiveGroup->addChild(normal);

						ConservativeTestConfig	config	=
						{
							VK_CONSERVATIVE_RASTERIZATION_MODE_UNDERESTIMATE_EXT,	//  VkConservativeRasterizationModeEXT	conservativeRasterizationMode;
							0.0f,													//  float								extraOverestimationSize;
							primitiveTypes[primitiveTypeNdx].type,					//  VkPrimitiveTopology					primitiveTopology;
							false,													//  bool								degeneratePrimitives;
							1.0f,													//  float								lineWidth;
							64u,													//  deUint32							resolution;
						};

						if (primitiveTypes[primitiveTypeNdx].type == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
							normal->addChild(new ConservativeTestCase<ConservativeTraingleTestInstance>	(testCtx,
																										 "test",
																										 "Underestimate test, verify rasterization result",
																										 config,
																										 samples[samplesNdx]));

						if (primitiveTypes[primitiveTypeNdx].type == VK_PRIMITIVE_TOPOLOGY_LINE_LIST)
						{
							for (int underestimateWidthNdx = 0; underestimateWidthNdx < DE_LENGTH_OF_ARRAY(underestimateLineWidths); ++underestimateWidthNdx)
							{
								config.lineWidth = underestimateLineWidths[underestimateWidthNdx].size;
								normal->addChild(new ConservativeTestCase<ConservativeLineTestInstance>		(testCtx,
																											 underestimateLineWidths[underestimateWidthNdx].name,
																											 "Underestimate test, verify rasterization result",
																											 config,
																											 samples[samplesNdx]));
							}
						}

						if (primitiveTypes[primitiveTypeNdx].type == VK_PRIMITIVE_TOPOLOGY_POINT_LIST)
						{
							for (int underestimatePointSizeNdx = 0; underestimatePointSizeNdx < DE_LENGTH_OF_ARRAY(underestimatePointSizes); ++underestimatePointSizeNdx)
							{
								config.lineWidth = underestimatePointSizes[underestimatePointSizeNdx].size;
								normal->addChild(new ConservativeTestCase<ConservativePointTestInstance>	(testCtx,
																											 underestimatePointSizes[underestimatePointSizeNdx].name,
																											 "Underestimate test, verify rasterization result",
																											 config,
																											 samples[samplesNdx]));
							}
						}
					}

					{
						tcu::TestCaseGroup* const degenerate = new tcu::TestCaseGroup(testCtx, "degenerate", "Degenerate primitives conservative rasterization tests");

						primitiveGroup->addChild(degenerate);

						ConservativeTestConfig	config	=
						{
							VK_CONSERVATIVE_RASTERIZATION_MODE_UNDERESTIMATE_EXT,	//  VkConservativeRasterizationModeEXT	conservativeRasterizationMode;
							0.0f,													//  float								extraOverestimationSize;
							primitiveTypes[primitiveTypeNdx].type,					//  VkPrimitiveTopology					primitiveTopology;
							true,													//  bool								degeneratePrimitives;
							1.0f,													//  float								lineWidth;
							64u,													//  deUint32							resolution;
						};

						if (primitiveTypes[primitiveTypeNdx].type == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
							degenerate->addChild(new ConservativeTestCase<ConservativeTraingleTestInstance>	(testCtx,
																											 "test",
																											 "Underestimate triangle test, verify rasterization result",
																											 config,
																											 samples[samplesNdx]));

						if (primitiveTypes[primitiveTypeNdx].type == VK_PRIMITIVE_TOPOLOGY_LINE_LIST)
						{
							for (int underestimateWidthNdx = 0; underestimateWidthNdx < DE_LENGTH_OF_ARRAY(underestimateLineWidths); ++underestimateWidthNdx)
							{
								config.lineWidth = underestimateLineWidths[underestimateWidthNdx].size;
								degenerate->addChild(new ConservativeTestCase<ConservativeLineTestInstance>		(testCtx,
																												 underestimateLineWidths[underestimateWidthNdx].name,
																												 "Underestimate line test, verify rasterization result",
																												 config,
																												 samples[samplesNdx]));
							}
						}
					}
				}
			}
		}
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
		rasterizationTests->addChild(createProvokingVertexTests(testCtx));
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

	// .depth bias
	{
		tcu::TestCaseGroup* const	depthBias	= new tcu::TestCaseGroup(testCtx, "depth_bias", "Test depth bias");
		static const char			dataDir[]	= "rasterization/depth_bias";

		static const struct
		{
			std::string name;
			vk::VkFormat format;
			std::string description;
		} cases [] =
		{
			{"d16_unorm",	vk::VK_FORMAT_D16_UNORM,			"Test depth bias with format D16_UNORM"},
			{"d32_sfloat",	vk::VK_FORMAT_D32_SFLOAT,			"Test depth bias with format D32_SFLOAT"},
			{"d24_unorm",	vk::VK_FORMAT_D24_UNORM_S8_UINT,	"Test depth bias with format D24_UNORM_S8_UINT"}
		};

		for (int i = 0; i < DE_LENGTH_OF_ARRAY(cases); ++i)
		{
			const VkImageCreateInfo vkImageCreateInfo = {
				VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,			// sType
				nullptr,										// pNext
				0,												// flags
				VK_IMAGE_TYPE_2D,								// imageType
				cases[i].format,								// format
				{250, 250, 1},									// extent
				1,												// mipLevels
				1,												// arrayLayers
				VK_SAMPLE_COUNT_1_BIT,							// samples
				VK_IMAGE_TILING_OPTIMAL,						// tiling
				VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,	// usage
				VK_SHARING_MODE_EXCLUSIVE,						// sharingMode
				0,												// queueFamilyIndexCount
				nullptr,										// pQueueFamilyIndices
				VK_IMAGE_LAYOUT_UNDEFINED,						// initialLayout
			};

			std::vector<std::string>		requirements = std::vector<std::string>(0);
			std::vector<VkImageCreateInfo>	imageRequirements;
			imageRequirements.push_back(vkImageCreateInfo);
			const std::string			fileName	= cases[i].name + ".amber";
			cts_amber::AmberTestCase*	testCase	= cts_amber::createAmberTestCase(testCtx, cases[i].name.c_str(), cases[i].description.c_str(), dataDir, fileName, requirements, imageRequirements);

			depthBias->addChild(testCase);
		}

		rasterizationTests->addChild(depthBias);
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
