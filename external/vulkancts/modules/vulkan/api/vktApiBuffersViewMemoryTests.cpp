/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2015 The Khronos Group Inc.
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
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
 * \brief Vulkan Buffer View Memory Tests
 *//*--------------------------------------------------------------------*/

#include "vktApiBuffersViewMemoryTests.hpp"

#include "deStringUtil.hpp"
#include "deUniquePtr.hpp"
#include "vktTestCase.hpp"
#include "vktTestCaseUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkPrograms.hpp"
#include "vkQueryUtil.hpp"
#include "vkRef.hpp"
#include "vkRefUtil.hpp"
#include "vkTypeUtil.hpp"
#include "tcuImageCompare.hpp"
#include "tcuTexture.hpp"
#include "tcuTextureUtil.hpp"

namespace vkt
{

namespace api
{

using namespace vk;

namespace
{

struct BufferViewCaseParams
{
	deUint32	bufferSize;
	deUint32	bufferViewSize;
	deUint32	elementOffset;
	deUint32	offset;
};

class BufferViewTestInstance : public vkt::TestInstance
{
public:
										BufferViewTestInstance		(Context&				context,
																	 BufferViewCaseParams	testCase);
	virtual								~BufferViewTestInstance		(void);
	virtual tcu::TestStatus				iterate						(void);

private:
	void								createQuad					(void);
	tcu::TestStatus						checkResult					(deInt8	factor = 1);

private:
	BufferViewCaseParams				m_testCase;

	const tcu::IVec2					m_renderSize;
	const VkFormat						m_colorFormat;

	const VkDeviceSize					m_pixelDataSize;

	Move<VkImage>						m_colorImage;
	de::MovePtr<Allocation>				m_colorImageAlloc;
	Move<VkImageView>					m_colorAttachmentView;
	Move<VkRenderPass>					m_renderPass;
	Move<VkFramebuffer>					m_framebuffer;

	Move<VkDescriptorSetLayout>			m_descriptorSetLayout;
	Move<VkDescriptorPool>				m_descriptorPool;
	Move<VkDescriptorSet>				m_descriptorSet;

	Move<VkBuffer>						m_uniformBuffer;
	de::MovePtr<vk::Allocation>			m_uniformBufferAlloc;
	Move<VkBufferView>					m_uniformBufferView;

	Move<VkShaderModule>				m_vertexShaderModule;
	Move<VkShaderModule>				m_fragmentShaderModule;
	Move<VkShader>						m_vertexShader;
	Move<VkShader>						m_fragmentShader;

	Move<VkBuffer>						m_vertexBuffer;
	std::vector<tcu::Vec4>				m_vertices;
	de::MovePtr<Allocation>				m_vertexBufferAlloc;

	Move<VkPipelineLayout>				m_pipelineLayout;
	Move<VkPipeline>					m_graphicsPipelines;

	Move<VkCmdPool>						m_cmdPool;
	Move<VkCmdBuffer>					m_cmdBuffer;

	Move<VkBuffer>						m_resultBuffer;
	de::MovePtr<Allocation>				m_resultBufferAlloc;

	Move<VkFence>						m_fence;
};

static void generateBuffer (std::vector<tcu::IVec4>& uniformData, deUint32 bufferSize, deInt8 factor = 1)
{
	for (deUint32 i = 0; i < bufferSize; ++i)
		uniformData.push_back(tcu::IVec4(factor * i, factor * (i + 1), factor * (i + 2), factor * (i + 3)));
}

void BufferViewTestInstance::createQuad (void)
{
	tcu::Vec4 a(-1.0, -1.0, 0.0, 1.0);
	tcu::Vec4 b(1.0, -1.0, 0.0, 1.0);
	tcu::Vec4 c(1.0, 1.0, 0.0, 1.0);
	tcu::Vec4 d(-1.0, 1.0, 0.0, 1.0);

	// Triangle 1
	m_vertices.push_back(a);
	m_vertices.push_back(c);
	m_vertices.push_back(b);

	// Triangle 2
	m_vertices.push_back(c);
	m_vertices.push_back(a);
	m_vertices.push_back(d);
}

BufferViewTestInstance::~BufferViewTestInstance	(void)
{
}

BufferViewTestInstance::BufferViewTestInstance (Context& context, BufferViewCaseParams testCase)
	: vkt::TestInstance		(context)
	, m_testCase			(testCase)
	, m_renderSize			(testCase.bufferViewSize, testCase.bufferViewSize)
	, m_colorFormat			(VK_FORMAT_R32_UINT)
	, m_pixelDataSize		(m_renderSize.x() * m_renderSize.y() * mapVkFormat(m_colorFormat).getPixelSize())
{
	const DeviceInterface&		vk					= context.getDeviceInterface();
	const VkDevice				vkDevice			= context.getDevice();
	const deUint32				queueFamilyIndex	= context.getUniversalQueueFamilyIndex();
	SimpleAllocator				memAlloc			(vk, vkDevice, getPhysicalDeviceMemoryProperties(context.getInstanceInterface(), context.getPhysicalDevice()));
	const VkChannelMapping		channelMappingRGBA	= { VK_CHANNEL_SWIZZLE_R, VK_CHANNEL_SWIZZLE_G, VK_CHANNEL_SWIZZLE_B, VK_CHANNEL_SWIZZLE_A };

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
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,									// VkImageLayout		initialLayout;
		};

		m_colorImage			= createImage(vk, vkDevice, &colorImageParams);

		// Allocate and bind color image memory
		m_colorImageAlloc		= memAlloc.allocate(getImageMemoryRequirements(vk, vkDevice, *m_colorImage), MemoryRequirement::Any);
		VK_CHECK(vk.bindImageMemory(vkDevice, *m_colorImage, m_colorImageAlloc->getMemory(), m_colorImageAlloc->getOffset()));
	}

	// Create destination buffer
	{
		const VkBufferCreateInfo bufferParams =
		{
			VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,		// VkStructureType		sType;
			DE_NULL,									// const void*			pNext;
			m_pixelDataSize,							// VkDeviceSize			size;
			VK_BUFFER_USAGE_TRANSFER_DESTINATION_BIT,   // VkBufferUsageFlags	usage;
			0u,											// VkBufferCreateFlags	flags;
			VK_SHARING_MODE_EXCLUSIVE,					// VkSharingMode		sharingMode;
			0u,											// deUint32				queueFamilyCount;
			DE_NULL,									// const deUint32*		pQueueFamilyIndices;
		};

		m_resultBuffer		= createBuffer(vk, vkDevice, &bufferParams);
		m_resultBufferAlloc = memAlloc.allocate(getBufferMemoryRequirements(vk, vkDevice, *m_resultBuffer), MemoryRequirement::HostVisible);

		VK_CHECK(vk.bindBufferMemory(vkDevice, *m_resultBuffer, m_resultBufferAlloc->getMemory(), m_resultBufferAlloc->getOffset()));
	}

	// Create color attachment view
	{
		const VkImageViewCreateInfo colorAttachmentViewParams =
		{
			VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,			// VkStructureType			sType;
			DE_NULL,											// const void*				pNext;
			*m_colorImage,										// VkImage					image;
			VK_IMAGE_VIEW_TYPE_2D,								// VkImageViewType			viewType;
			m_colorFormat,										// VkFormat					format;
			channelMappingRGBA,									// VkChannelMapping			channels;
			{ VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u },		// VkImageSubresourceRange	subresourceRange;
			0u													// VkImageViewCreateFlags	flags;
		};

		m_colorAttachmentView = createImageView(vk, vkDevice, &colorAttachmentViewParams);
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

		const VkAttachmentReference colorAttachmentReference =
		{
			0u,													// deUint32			attachment;
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL			// VkImageLayout	layout;
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
			{ ~0u, VK_IMAGE_LAYOUT_GENERAL },					// VkAttachmentReference			depthStencilAttachment;
			0u,													// deUint32							preserveCount;
			DE_NULL												// const VkAttachmentReference*		pPreserveAttachments;
		};

		const VkRenderPassCreateInfo renderPassParams =
		{
			VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,			// VkStructureType					sType;
			DE_NULL,											// const void*						pNext;
			1u,													// deUint32							attachmentCount;
			&colorAttachmentDescription,						// const VkAttachmentDescription*	pAttachments;
			1u,													// deUint32							subpassCount;
			&subpassDescription,								// const VkSubpassDescription*		pSubpasses;
			0u,													// deUint32							dependencyCount;
			DE_NULL												// const VkSubpassDependency*		pDependencies;
		};

		m_renderPass = createRenderPass(vk, vkDevice, &renderPassParams);
	}

	// Create framebuffer
	{
		const VkImageView attachmentBindInfos[1] =
		{
			*m_colorAttachmentView,
		};

		const VkFramebufferCreateInfo framebufferParams =
		{
			VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,			// VkStructureType				sType;
			DE_NULL,											// const void*					pNext;
			*m_renderPass,										// VkRenderPass					renderPass;
			1u,													// deUint32						attachmentCount;
			attachmentBindInfos,								// const VkImageView*			pAttachments;
			(deUint32)m_renderSize.x(),							// deUint32						width;
			(deUint32)m_renderSize.y(),							// deUint32						height;
			1u													// deUint32						layers;
		};

		m_framebuffer = createFramebuffer(vk, vkDevice, &framebufferParams);
	}

	// Create descriptors
	{
		const VkDescriptorSetLayoutBinding layoutBindings[1] =
		{
			{
				VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,			// VkDescriptorType		descriptorType;
				1u,											// deUint32				arraySize;
				VK_SHADER_STAGE_ALL,						// VkShaderStageFlags	stageFlags;
				DE_NULL										// const VkSampler*		pImmutableSamplers;
			},
		};

		const VkDescriptorSetLayoutCreateInfo descriptorLayoutParams =
		{
			VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,	// VkStructureType						sType;
			DE_NULL,												// cost void*							pNexŧ;
			DE_LENGTH_OF_ARRAY(layoutBindings),						// deUint32								count;
			layoutBindings											// const VkDescriptorSetLayoutBinding	pBinding;
		};

		m_descriptorSetLayout = createDescriptorSetLayout(vk, vkDevice, &descriptorLayoutParams);

		// Generate buffer
		std::vector<tcu::IVec4> uniformData;
		generateBuffer(uniformData, testCase.bufferSize);

		const VkDeviceSize uniformSize = testCase.bufferSize * sizeof(tcu::Vec4);
		const VkBufferCreateInfo uniformBufferParams =
		{
			VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,		// VkStructureType		sType;
			DE_NULL,									// const void*			pNext;
			uniformSize,								// VkDeviceSize			size;
			VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT,	// VkBufferUsageFlags	usage;
			0u,											// VkBufferCreateFlags	flags; <-- TODO: 0u?
			VK_SHARING_MODE_EXCLUSIVE,					// VkSharingMode		sharingMode;
			1u,											// deUint32				queueFamilyIndexCount;
			&queueFamilyIndex							// const deUint32*		pQueueFamilyIndices;
		};

		m_uniformBuffer			= createBuffer(vk, vkDevice, &uniformBufferParams);
		m_uniformBufferAlloc	= memAlloc.allocate(getBufferMemoryRequirements(vk, vkDevice, *m_uniformBuffer), MemoryRequirement::HostVisible);

		VK_CHECK(vk.bindBufferMemory(vkDevice, *m_uniformBuffer, m_uniformBufferAlloc->getMemory(), 0));
		deMemcpy(m_uniformBufferAlloc->getHostPtr(), uniformData.data(), uniformSize);

		const VkBufferViewCreateInfo viewInfo =
		{
			VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO,							// VkStructureType	sType;
			DE_NULL,															// void*			pNext;
			*m_uniformBuffer,													// VkBuffer			buffer;
			m_colorFormat,														// VkFormat			format;
			m_testCase.elementOffset * sizeof(tcu::IVec4) + m_testCase.offset,	// VkDeviceSize		offset;
			m_testCase.bufferViewSize * sizeof(tcu::IVec4)						// VkDeviceSize		range;
		};

		m_uniformBufferView = createBufferView(vk, vkDevice, &viewInfo);

		const VkDescriptorTypeCount descriptorTypes[1] =
		{
			{
				VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,				// VkDescriptorType		type;
				1												// deUint32				count;
			}
		};

		const VkDescriptorPoolCreateInfo descriptorPoolParams =
		{
			VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,		// VkStructureType					sType;
			DE_NULL,											// void*							pNext;
			VK_DESCRIPTOR_POOL_USAGE_ONE_SHOT,					// VkDescriptorPoolUsage			poolUsage;
			1u,													// uint32_t							maxSets;
			DE_LENGTH_OF_ARRAY(descriptorTypes),				// deUint32							count;
			descriptorTypes										// const VkDescriptorTypeCount*		pTypeCount
		};

		m_descriptorPool = createDescriptorPool(vk, vkDevice, &descriptorPoolParams);
		m_descriptorSet = allocDescriptorSet(vk, vkDevice, *m_descriptorPool, VK_DESCRIPTOR_SET_USAGE_STATIC, *m_descriptorSetLayout);

		const VkDescriptorInfo descriptorInfos[] =
		{
			{
				*m_uniformBufferView,						// VkBufferView				bufferView;
				0,											// VkSampler				sampler;
				0,											// VkImageView				imageView;
				(VkImageLayout)0,							// VkImageLayout			imageLayout;
				{ DE_NULL, 0u, 0u }							// VkDescriptorBufferInfo	bufferInfo;
			}
		};

		const VkWriteDescriptorSet writeDescritporSets[] =
		{
			{
				VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,		// VkStructureType			sType;
				DE_NULL,									// const void*				pNext;
				*m_descriptorSet,							// VkDescriptorSet			destSet;
				0,											// deUint32					destBinding;
				0,											// deUint32					destArrayElement;
				1u,											// deUint32					count;
				VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,	// VkDescriptorType			descriptorType;
				descriptorInfos,							// const VkDescriptorInfo*	pDescriptors;
			}
		};

		vk.updateDescriptorSets(vkDevice, 1, writeDescritporSets, 0u, DE_NULL);
	}

	// Create pipeline layout
	{
		const VkPipelineLayoutCreateInfo pipelineLayoutParams =
		{
			VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,		// VkStructureType				sType;
			DE_NULL,											// const void*					pNext;
			1u,													// deUint32						descriptorSetCount;
			&*m_descriptorSetLayout,							// const VkDescriptorSetLayout*	pSetLayouts;
			0u,													// deUint32						pushConstantRangeCount;
			DE_NULL												// const VkPushConstantRange*	pPushConstantRanges;
		};

		m_pipelineLayout = createPipelineLayout(vk, vkDevice, &pipelineLayoutParams);
	}

	// Create shaders
	{
		m_vertexShaderModule	= createShaderModule(vk, vkDevice, m_context.getBinaryCollection().get("vert"), 0);
		m_fragmentShaderModule	= createShaderModule(vk, vkDevice, m_context.getBinaryCollection().get("frag"), 0);

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
			0u,										// deUint32					binding;
			sizeof(tcu::Vec4),						// deUint32					strideInBytes;
			VK_VERTEX_INPUT_STEP_RATE_VERTEX		// VkVertexInputStepRate	stepRate;
		};

		const VkVertexInputAttributeDescription vertexInputAttributeDescriptions[1] =
		{
			{
				0u,									// deUint32	location;
				0u,									// deUint32	binding;
				VK_FORMAT_R32G32B32A32_SFLOAT,		// VkFormat	format;
				0u									// deUint32	offsetInBytes;
			}
		};

		const VkPipelineVertexInputStateCreateInfo vertexInputStateParams =
		{
			VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,		// VkStructureType							sType;
			DE_NULL,														// const void*								pNext;
			1u,																// deUint32									bindingCount;
			&vertexInputBindingDescription,									// const VkVertexInputBindingDescription*	pVertexBindingDescriptions;
			1u,																// deUint32									attributeCount;
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

		const VkPipelineDynamicStateCreateInfo	dynamicStateParams		=
		{
			VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,		// VkStructureType			sType;
			DE_NULL,													// const void*				pNext;
			0u,															// deUint32					dynamicStateCount;
			DE_NULL														// const VkDynamicState*	pDynamicStates;
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
			DE_NULL,											// const VkPipelineMultisampleStateCreateInfo*		pMultisampleState;
			DE_NULL,											// const VkPipelineDepthStencilStateCreateInfo*		pDepthStencilState;
			&colorBlendStateParams,								// const VkPipelineColorBlendStateCreateInfo*		pColorBlendState;
			&dynamicStateParams,								// const VkPipelineDynamicStateCreateInfo*			pDynamicState;
			0u,													// VkPipelineCreateFlags							flags;
			*m_pipelineLayout,									// VkPipelineLayout									layout;
			*m_renderPass,										// VkRenderPass										renderPass;
			0u,													// deUint32											subpass;
			0u,													// VkPipeline										basePipelineHandle;
			0u													// deInt32											basePipelineIndex;
		};

		m_graphicsPipelines		= createGraphicsPipeline(vk, vkDevice, DE_NULL, &graphicsPipelineParams);
	}

	// Create vertex buffer
	{
		createQuad();
		const VkDeviceSize vertexDataSize = m_vertices.size() * sizeof(tcu::Vec4);
		const VkBufferCreateInfo vertexBufferParams =
		{
			VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,		// VkStructureType		sType;
			DE_NULL,									// const void*			pNext;
			vertexDataSize,								// VkDeviceSize			size;
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,			// VkBufferUsageFlags	usage;
			0u,											// VkBufferCreateFlags	flags;
			VK_SHARING_MODE_EXCLUSIVE,					// VkSharingMode		sharingMode;
			1u,											// deUint32				queueFamilyCount;
			&queueFamilyIndex							// const deUint32*		pQueueFamilyIndices;
		};

		m_vertexBuffer		= createBuffer(vk, vkDevice, &vertexBufferParams);
		m_vertexBufferAlloc	= memAlloc.allocate(getBufferMemoryRequirements(vk, vkDevice, *m_vertexBuffer), MemoryRequirement::HostVisible);

		VK_CHECK(vk.bindBufferMemory(vkDevice, *m_vertexBuffer, m_vertexBufferAlloc->getMemory(), m_vertexBufferAlloc->getOffset()));

		// Load vertices into vertex buffer
		deMemcpy(m_vertexBufferAlloc->getHostPtr(), m_vertices.data(), vertexDataSize);
		flushMappedMemoryRange(vk, vkDevice, m_vertexBufferAlloc->getMemory(), m_vertexBufferAlloc->getOffset(), vertexDataSize);
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

		const VkClearValue clearValue = makeClearValueColorF32(0.0, 0.0, 0.0, 0.0);

		const VkClearValue attachmentClearValues[1] =
		{
			clearValue,
		};

		const VkRenderPassBeginInfo renderPassBeginInfo =
		{
			VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,				// VkStructureType		sType;
			DE_NULL,												// const void*			pNext;
			*m_renderPass,											// VkRenderPass			renderPass;
			*m_framebuffer,											// VkFramebuffer		framebuffer;
			{ { 0, 0 }, { m_renderSize.x(), m_renderSize.y() } },	// VkRect2D				renderArea;
			1u,														// deUint32				clearValueCount;
			attachmentClearValues									// const VkClearValue*	pClearValues;
		};

		m_cmdBuffer = createCommandBuffer(vk, vkDevice, &cmdBufferParams);

		VK_CHECK(vk.beginCommandBuffer(*m_cmdBuffer, &cmdBufferBeginInfo));
		vk.cmdBeginRenderPass(*m_cmdBuffer, &renderPassBeginInfo, VK_RENDER_PASS_CONTENTS_INLINE);

		const VkDeviceSize	vertexBufferOffset[1] = { 0 };

		vk.cmdBindPipeline(*m_cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_graphicsPipelines);
		vk.cmdBindDescriptorSets(*m_cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipelineLayout, 0u, 1, &*m_descriptorSet, 0u, DE_NULL);
		vk.cmdBindVertexBuffers(*m_cmdBuffer, 0, 1, &m_vertexBuffer.get(), vertexBufferOffset);
		vk.cmdDraw(*m_cmdBuffer, (deUint32)m_vertices.size(), 1, 0, 0);

		const VkImageMemoryBarrier imageBarrier =
		{
			VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,		// VkStructureType			sType;
			DE_NULL,									// const void*				pNext;
			VK_MEMORY_OUTPUT_COLOR_ATTACHMENT_BIT,		// VkMemoryOutputFlags		outputMask;
			VK_MEMORY_INPUT_TRANSFER_BIT,				// VkMemoryInputFlags		inputMask;
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,	// VkImageLayout			oldLayout;
			VK_IMAGE_LAYOUT_TRANSFER_SOURCE_OPTIMAL,	// VkImageLayout			newLayout;
			VK_QUEUE_FAMILY_IGNORED,					// deUint32					srcQueueFamilyIndex;
			VK_QUEUE_FAMILY_IGNORED,					// deUint32					destQueueFamilyIndex;
			*m_colorImage,								// VkImage					image;
			{											// VkImageSubresourceRange	subresourceRange;
				VK_IMAGE_ASPECT_COLOR_BIT,				// VkImageAspectFlags	aspectMask;
				0u,										// deUint32				baseMipLevel;
				1u,										// deUint32				mipLevels;
				0u,										// deUint32				baseArraySlice;
				1u										// deUint32				arraySize;
			}
		};

		const VkBufferMemoryBarrier bufferBarrier =
		{
			VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,	// VkStructureType		sType;
			DE_NULL,									// const void*			pNext;
			VK_MEMORY_OUTPUT_TRANSFER_BIT,				// VkMemoryOutputFlags	outputMask;
			VK_MEMORY_INPUT_HOST_READ_BIT,				// VkMemoryInputFlags	inputMask;
			VK_QUEUE_FAMILY_IGNORED,					// deUint32				srcQueueFamilyIndex;
			VK_QUEUE_FAMILY_IGNORED,					// deUint32				destQueueFamilyIndex;
			*m_resultBuffer,							// VkBuffer				buffer;
			0u,											// VkDeviceSize			offset;
			m_pixelDataSize								// VkDeviceSize			size;
		};

		const VkBufferImageCopy copyRegion =
		{
			0u,											// VkDeviceSize				bufferOffset;
			(deUint32)m_renderSize.x(),					// deUint32					bufferRowLength;
			(deUint32)m_renderSize.y(),					// deUint32					bufferImageHeight;
			{ VK_IMAGE_ASPECT_COLOR, 0u, 0u, 1u },		// VkImageSubresourceCopy	imageSubresource;
			{ 0, 0, 0 },								// VkOffset3D				imageOffset;
			{ m_renderSize.x(), m_renderSize.y(), 1 }	// VkExtent3D				imageExtent;
		};

		const void* const	imageBarrierPtr		= &imageBarrier;
		const void* const	bufferBarrierPtr	= &bufferBarrier;

		vk.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_FALSE, 1, &imageBarrierPtr);
		vk.cmdCopyImageToBuffer(*m_cmdBuffer, *m_colorImage, VK_IMAGE_LAYOUT_TRANSFER_SOURCE_OPTIMAL, *m_resultBuffer, 1, &copyRegion);
		vk.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_FALSE, 1, &bufferBarrierPtr);

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

tcu::TestStatus BufferViewTestInstance::checkResult (deInt8 factor)
{
	const DeviceInterface&			vk					= m_context.getDeviceInterface();
	const VkDevice					vkDevice			= m_context.getDevice();
	const tcu::TextureFormat		tcuFormat			= mapVkFormat(m_colorFormat);
	de::MovePtr<tcu::TextureLevel>	resultLevel			(new tcu::TextureLevel(tcuFormat, m_renderSize.x(), m_renderSize.y()));

	invalidateMappedMemoryRange(vk, vkDevice, m_resultBufferAlloc->getMemory(), m_resultBufferAlloc->getOffset(), m_pixelDataSize);
	tcu::copy(*resultLevel, tcu::ConstPixelBufferAccess(resultLevel->getFormat(), resultLevel->getSize(), m_resultBufferAlloc->getHostPtr()));

	tcu::ConstPixelBufferAccess pixelBuffer = resultLevel->getAccess();
	for (deInt32 i = 0; i < (deInt32) m_renderSize.x(); ++i)
	{
		tcu::IVec4 pixel	= pixelBuffer.getPixelInt(i, i);
		deInt32 expected	= factor * (m_testCase.elementOffset + i + (m_testCase.offset / 4) % 4);
		deInt32 actual		= pixel[0];
		if (expected != actual)
		{
			std::ostringstream errorMessage;
			errorMessage << "BufferView test failed. expected: " << expected << " actual: " << actual;
			return tcu::TestStatus::fail(errorMessage.str());
		}
	}

	return tcu::TestStatus::pass("BufferView test");
}

tcu::TestStatus BufferViewTestInstance::iterate (void)
{
	const DeviceInterface&		vk			= m_context.getDeviceInterface();
	const VkDevice				vkDevice	= m_context.getDevice();
	const VkQueue				queue		= m_context.getUniversalQueue();

	VK_CHECK(vk.resetFences(vkDevice, 1, &m_fence.get()));
	VK_CHECK(vk.queueSubmit(queue, 1, &m_cmdBuffer.get(), *m_fence));
	VK_CHECK(vk.waitForFences(vkDevice, 1, &m_fence.get(), true, ~(0ull) /* infinity */));

	tcu::TestStatus				testStatus	= checkResult();
	if (testStatus.getCode() != QP_TEST_RESULT_PASS)
	{
		return testStatus;
	}

	// Generate and bind another buffer
	std::vector<tcu::IVec4>		uniformData;
	const VkDeviceSize			uniformSize	= m_testCase.bufferSize * sizeof(tcu::Vec4);
	const deInt8				factor		= 2;

	generateBuffer(uniformData, m_testCase.bufferSize, factor);
	deMemcpy(m_uniformBufferAlloc->getHostPtr(), uniformData.data(), uniformSize);

	VK_CHECK(vk.resetFences(vkDevice, 1, &m_fence.get()));
	VK_CHECK(vk.queueSubmit(queue, 1, &m_cmdBuffer.get(), *m_fence));
	VK_CHECK(vk.waitForFences(vkDevice, 1, &m_fence.get(), true, ~(0ull) /* infinity */));

	return checkResult(factor);
}

class BufferViewTestCase : public vkt::TestCase
{
public:
							BufferViewTestCase			(tcu::TestContext&			testCtx,
														 const std::string&			name,
														 const std::string&			description,
														 BufferViewCaseParams	bufferViewTestInfo)
								: vkt::TestCase			(testCtx, name, description)
								, m_bufferViewTestInfo	(bufferViewTestInfo)
							{}

	virtual					~BufferViewTestCase			(void) {}
	virtual	void			initPrograms				(SourceCollections&			programCollection) const;

	virtual TestInstance*	createInstance				(Context&					context) const
							{
								return new BufferViewTestInstance(context, m_bufferViewTestInfo);
							}
private:
	BufferViewCaseParams	m_bufferViewTestInfo;
};

void BufferViewTestCase::initPrograms (SourceCollections& programCollection) const
{
	programCollection.glslSources.add("vert") << glu::VertexSource(
		"#version 310 es\n"
		"layout (location = 0) in highp vec4 a_position;\n"
		"void main()\n"
		"{\n"
		"	gl_Position = a_position;\n"
		"}\n");


	programCollection.glslSources.add("frag") << glu::FragmentSource(
		"#version 310 es\n"
		"layout (set=0, binding=0) uniform u_buffer {\n"
		"	highp uint item[" + de::toString(m_bufferViewTestInfo.bufferViewSize) + "];\n"
		"};\n"
		"layout (location = 0) out highp uint o_color;\n"
		"void main()\n"
		"{\n"
		"	o_color = item[int(gl_FragCoord.x)];\n"
		"}\n");
}

} // anonymous

tcu::TestCaseGroup* createBufferViewMemoryTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup>	bufferViewTests	(new tcu::TestCaseGroup(testCtx, "buffersViewMemory", "BufferView Memory Tests"));

	{
		BufferViewCaseParams info =
		{
			512,	// deUint32	bufferSize
			512,	// deUint32	bufferViewSize
			0,		// deUint32	elementOffset
			0,		// deUint32 offset
		};
		std::ostringstream description;
		description << "bufferSize: " << info.bufferSize << " bufferViewSize: " << " bufferView element offset: " << info.elementOffset	<< " offset: " << info.offset;
		bufferViewTests->addChild(new BufferViewTestCase(testCtx, "buffer_view_memory_test_complete", description.str(), info));
	}

	{
		BufferViewCaseParams info =
		{
			4096,	// deUint32	bufferSize
			512,	// deUint32	bufferViewSize
			0,		// deUint32	elementOffset
			0,		// deUint32 offset
		};
		std::ostringstream description;
		description << "bufferSize: " << info.bufferSize << " bufferViewSize: " << " bufferView element offset: " << info.elementOffset	<< " offset: " << info.offset;
		bufferViewTests->addChild(new BufferViewTestCase(testCtx, "buffer_view_memory_test_partial_offset0", description.str(), info));
	}

	{
		BufferViewCaseParams info =
		{
			4096,	// deUint32	bufferSize
			512,	// deUint32	bufferViewSize
			128,	// deUint32	elementOffset
			0,		// deUint32 offset
		};
		std::ostringstream description;
		description << "bufferSize: " << info.bufferSize << " bufferViewSize: " << " bufferView element offset: " << info.elementOffset	<< " offset: " << info.offset;
		bufferViewTests->addChild(new BufferViewTestCase(testCtx, "buffer_view_memory_test_partial_offset1", description.str(), info));
	}

	{
		BufferViewCaseParams info =
		{
			4096,	// deUint32	bufferSize
			512,	// deUint32	bufferViewSize
			128,	// deUint32	elementOffset
			4,		// deUint32 offset
		};
		std::ostringstream description;
		description << "bufferSize: " << info.bufferSize << " bufferViewSize: " << " bufferView element offset: " << info.elementOffset	<< " offset: " << info.offset;
		bufferViewTests->addChild(new BufferViewTestCase(testCtx, "buffer_view_memory_test_partial_offset2", description.str(), info));
	}

	return bufferViewTests.release();
}

} // api
} // vkt
