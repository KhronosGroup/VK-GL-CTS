/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2015 The Khronos Group Inc.
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
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
 * \brief Vulkan Buffer View Memory Tests
 *//*--------------------------------------------------------------------*/

#include "vktApiBufferViewAccessTests.hpp"
#include "vktApiBufferAndImageAllocationUtil.hpp"

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
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"
#include "tcuImageCompare.hpp"
#include "tcuTexture.hpp"
#include "tcuTextureUtil.hpp"
#include "deSharedPtr.hpp"
#include "deArrayUtil.hpp"
#include "tcuVectorUtil.hpp"
#include "../image/vktImageTestsUtil.hpp"

namespace vkt
{

namespace api
{

using namespace vk;

namespace
{

enum AllocationKind
{
	ALLOCATION_KIND_SUBALLOCATION										= 0,
	ALLOCATION_KIND_DEDICATED											= 1,
	ALLOCATION_KIND_LAST
};

struct BufferViewCaseParams
{
	deUint32							bufferSize;
	deUint32							bufferViewSize;
	deUint32							elementOffset;
	AllocationKind						bufferAllocationKind;
	AllocationKind						imageAllocationKind;

	VkFormat							format;
	VkBufferUsageFlags					createUsage;
	VkBufferUsageFlags					bindUsage;
	VkFormatFeatureFlags				feature;
	VkDescriptorType					descType;

	BufferViewCaseParams (deUint32 bufferSize_,
						  deUint32 bufferViewSize_,
						  deUint32 elementOffset_,
						  AllocationKind bufferAllocKind_,
						  AllocationKind imageAllocKind_,
						  VkFormat format_ = VK_FORMAT_R32_UINT,
						  VkBufferUsageFlags createUsage_ = VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT,
						  VkBufferUsageFlags bindUsage_ = VK_BUFFER_USAGE_FLAG_BITS_MAX_ENUM,
						  VkFormatFeatureFlags featureFlags_ = VK_FORMAT_FEATURE_UNIFORM_TEXEL_BUFFER_BIT,
						  VkDescriptorType descType_ = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER)
		: bufferSize(bufferSize_)
		, bufferViewSize(bufferViewSize_)
		, elementOffset(elementOffset_)
		, bufferAllocationKind(bufferAllocKind_)
		, imageAllocationKind(imageAllocKind_)
		, format(format_)
		, createUsage(createUsage_)
		, bindUsage(bindUsage_)
		, feature(featureFlags_)
		, descType(descType_)
	{
	}
};

class BufferViewTestInstance : public vkt::TestInstance
{
public:
										BufferViewTestInstance			(Context&					context,
																		 BufferViewCaseParams		testCase);
	virtual								~BufferViewTestInstance			(void);
	virtual tcu::TestStatus				iterate							(void);

private:
	void								createQuad						(void);
	tcu::TestStatus						checkResult						(deInt8						factor);

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

	Move<VkBuffer>						m_vertexBuffer;
	std::vector<tcu::Vec4>				m_vertices;
	de::MovePtr<Allocation>				m_vertexBufferAlloc;

	Move<VkPipelineLayout>				m_pipelineLayout;
	Move<VkPipeline>					m_graphicsPipelines;

	Move<VkCommandPool>					m_cmdPool;
	Move<VkCommandBuffer>				m_cmdBuffer;

	Move<VkBuffer>						m_resultBuffer;
	de::MovePtr<Allocation>				m_resultBufferAlloc;
};

static void generateBuffer												(std::vector<deUint32>&		uniformData,
																		 deUint32					bufferSize,
																		 deInt8						factor)
{
	for (deUint32 i = 0; i < bufferSize; ++i)
		uniformData.push_back(factor * i);
}

void BufferViewTestInstance::createQuad									(void)
{
	tcu::Vec4							a(-1.0, -1.0, 0.0, 1.0);
	tcu::Vec4							b(1.0, -1.0, 0.0, 1.0);
	tcu::Vec4							c(1.0, 1.0, 0.0, 1.0);
	tcu::Vec4							d(-1.0, 1.0, 0.0, 1.0);

	// Triangle 1
	m_vertices.push_back(a);
	m_vertices.push_back(c);
	m_vertices.push_back(b);

	// Triangle 2
	m_vertices.push_back(c);
	m_vertices.push_back(a);
	m_vertices.push_back(d);
}

BufferViewTestInstance::~BufferViewTestInstance							(void)
{
}

BufferViewTestInstance::BufferViewTestInstance							(Context&					context,
																		 BufferViewCaseParams		testCase)
										: vkt::TestInstance				(context)
										, m_testCase					(testCase)
										, m_renderSize					(testCase.bufferViewSize, testCase.bufferViewSize)
										, m_colorFormat					(VK_FORMAT_R32_UINT)
										, m_pixelDataSize				(m_renderSize.x() * m_renderSize.y() * mapVkFormat(m_colorFormat).getPixelSize())
{
	const DeviceInterface&				vk								= context.getDeviceInterface();
	const VkDevice						vkDevice						= context.getDevice();
	const deUint32						queueFamilyIndex				= context.getUniversalQueueFamilyIndex();
	SimpleAllocator						memAlloc						(vk, vkDevice, getPhysicalDeviceMemoryProperties(context.getInstanceInterface(), context.getPhysicalDevice()));
	const VkComponentMapping			channelMappingRGBA				= { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };

	// Create color image
	if (m_testCase.imageAllocationKind == ALLOCATION_KIND_DEDICATED)
	{
		ImageDedicatedAllocation().createTestImage(m_renderSize, m_colorFormat, context, memAlloc, m_colorImage, MemoryRequirement::Any, m_colorImageAlloc);
	}
	else
	{
		ImageSuballocation().createTestImage(m_renderSize, m_colorFormat, context, memAlloc, m_colorImage, MemoryRequirement::Any, m_colorImageAlloc);
	}

	// Create destination buffer
	if (m_testCase.bufferAllocationKind == ALLOCATION_KIND_DEDICATED)
	{
		BufferDedicatedAllocation().createTestBuffer(vk, vkDevice, queueFamilyIndex, m_pixelDataSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT, m_context, memAlloc, m_resultBuffer, MemoryRequirement::HostVisible, m_resultBufferAlloc);
	}
	else
	{
		BufferSuballocation().createTestBuffer(vk, vkDevice, queueFamilyIndex, m_pixelDataSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT, m_context, memAlloc, m_resultBuffer, MemoryRequirement::HostVisible, m_resultBufferAlloc);
	}

	// Create color attachment view
	{
		const VkImageViewCreateInfo		colorAttachmentViewParams		=
		{
			VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,					// VkStructureType			sType;
			DE_NULL,													// const void*				pNext;
			0u,															// VkImageViewCreateFlags	flags;
			*m_colorImage,												// VkImage					image;
			VK_IMAGE_VIEW_TYPE_2D,										// VkImageViewType			viewType;
			m_colorFormat,												// VkFormat					format;
			channelMappingRGBA,											// VkChannelMapping			channels;
			{ VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u },				// VkImageSubresourceRange	subresourceRange;
		};

		m_colorAttachmentView = createImageView(vk, vkDevice, &colorAttachmentViewParams);
	}

	// Create render pass
	m_renderPass = makeRenderPass(vk, vkDevice, m_colorFormat);

	// Create framebuffer
	{
		const VkImageView				attachmentBindInfos[1]			=
		{
			*m_colorAttachmentView,
		};

		const VkFramebufferCreateInfo	framebufferParams				=
		{
			VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,					// VkStructureType			sType;
			DE_NULL,													// const void*				pNext;
			(VkFramebufferCreateFlags)0,
			*m_renderPass,												// VkRenderPass				renderPass;
			1u,															// deUint32					attachmentCount;
			attachmentBindInfos,										// const VkImageView*		pAttachments;
			(deUint32)m_renderSize.x(),									// deUint32					width;
			(deUint32)m_renderSize.y(),									// deUint32					height;
			1u															// deUint32					layers;
		};

		m_framebuffer = createFramebuffer(vk, vkDevice, &framebufferParams);
	}

	// Create descriptors
	{
		const VkDescriptorSetLayoutBinding
										layoutBindings[1]				=
		{
			{
				0u,														// deUint32					binding;
				VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,				// VkDescriptorType			descriptorType;
				1u,														// deUint32					arraySize;
				VK_SHADER_STAGE_ALL,									// VkShaderStageFlags		stageFlags;
				DE_NULL													// const VkSampler*			pImmutableSamplers;
			},
		};

		const VkDescriptorSetLayoutCreateInfo
										descriptorLayoutParams			=
		{
			VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,		// VkStructureType			sType;
			DE_NULL,													// const void*				pNext;
			(VkDescriptorSetLayoutCreateFlags)0,
			DE_LENGTH_OF_ARRAY(layoutBindings),							// deUint32					count;
			layoutBindings												// const VkDescriptorSetLayoutBinding pBinding;
		};

		m_descriptorSetLayout = createDescriptorSetLayout(vk, vkDevice, &descriptorLayoutParams);

		// Generate buffer
		std::vector<deUint32>			uniformData;
		generateBuffer(uniformData, testCase.bufferSize, 1);

		const VkDeviceSize				uniformSize						= testCase.bufferSize * sizeof(deUint32);

		BufferSuballocation().createTestBuffer(vk, vkDevice, queueFamilyIndex, uniformSize, testCase.createUsage, m_context, memAlloc, m_uniformBuffer, MemoryRequirement::HostVisible, m_uniformBufferAlloc);
		deMemcpy(m_uniformBufferAlloc->getHostPtr(), uniformData.data(), (size_t)uniformSize);
		flushAlloc(vk, vkDevice, *m_uniformBufferAlloc);

		const VkBufferViewCreateInfo	viewInfo						=
		{
			VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO,					// VkStructureType			sType;
			DE_NULL,													// void*					pNext;
			(VkBufferViewCreateFlags)0,
			*m_uniformBuffer,											// VkBuffer					buffer;
			m_colorFormat,												// VkFormat					format;
			m_testCase.elementOffset * sizeof(deUint32),				// VkDeviceSize				offset;
			m_testCase.bufferViewSize * sizeof(deUint32)				// VkDeviceSize				range;
		};

		m_uniformBufferView = createBufferView(vk, vkDevice, &viewInfo);

		const VkDescriptorPoolSize		descriptorTypes[1]				=
		{
			{
				VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,				// VkDescriptorType			type;
				1														// deUint32					count;
			}
		};

		const VkDescriptorPoolCreateInfo
										descriptorPoolParams			=
		{
			VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,				// VkStructureType			sType;
			DE_NULL,													// void*					pNext;
			VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,			// VkDescriptorPoolCreateFlags flags;
			1u,															// uint32_t					maxSets;
			DE_LENGTH_OF_ARRAY(descriptorTypes),						// deUint32					count;
			descriptorTypes												// const VkDescriptorTypeCount* pTypeCount
		};

		m_descriptorPool = createDescriptorPool(vk, vkDevice, &descriptorPoolParams);

		const VkDescriptorSetAllocateInfo
										descriptorSetParams				=
		{
			VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
			DE_NULL,
			*m_descriptorPool,
			1u,
			&m_descriptorSetLayout.get(),
		};
		m_descriptorSet = allocateDescriptorSet(vk, vkDevice, &descriptorSetParams);

		const VkWriteDescriptorSet		writeDescritporSets[]			=
		{
			{
				VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,					// VkStructureType			sType;
				DE_NULL,												// const void*				pNext;
				*m_descriptorSet,										// VkDescriptorSet			destSet;
				0,														// deUint32					destBinding;
				0,														// deUint32					destArrayElement;
				1u,														// deUint32					count;
				VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,				// VkDescriptorType			descriptorType;
				(const VkDescriptorImageInfo*)DE_NULL,
				(const VkDescriptorBufferInfo*)DE_NULL,
				&m_uniformBufferView.get(),
			}
		};

		vk.updateDescriptorSets(vkDevice, DE_LENGTH_OF_ARRAY(writeDescritporSets), writeDescritporSets, 0u, DE_NULL);
	}

	// Create pipeline layout
	{
		const VkPipelineLayoutCreateInfo
										pipelineLayoutParams			=
		{
			VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,				// VkStructureType			sType;
			DE_NULL,													// const void*				pNext;
			(VkPipelineLayoutCreateFlags)0,
			1u,															// deUint32					descriptorSetCount;
			&*m_descriptorSetLayout,									// const VkDescriptorSetLayout* pSetLayouts;
			0u,															// deUint32					pushConstantRangeCount;
			DE_NULL														// const VkPushConstantRange* pPushConstantRanges;
		};

		m_pipelineLayout = createPipelineLayout(vk, vkDevice, &pipelineLayoutParams);
	}

	// Create shaders
	{
		m_vertexShaderModule = createShaderModule(vk, vkDevice, m_context.getBinaryCollection().get("vert"), 0);
		m_fragmentShaderModule = createShaderModule(vk, vkDevice, m_context.getBinaryCollection().get("frag"), 0);
	}

	// Create pipeline
	{
		const std::vector<VkViewport>	viewports	(1, makeViewport(m_renderSize));
		const std::vector<VkRect2D>		scissors	(1, makeRect2D(m_renderSize));

		m_graphicsPipelines = makeGraphicsPipeline(vk,						// const DeviceInterface&            vk
												   vkDevice,				// const VkDevice                    device
												   *m_pipelineLayout,		// const VkPipelineLayout            pipelineLayout
												   *m_vertexShaderModule,	// const VkShaderModule              vertexShaderModule
												   DE_NULL,					// const VkShaderModule              tessellationControlModule
												   DE_NULL,					// const VkShaderModule              tessellationEvalModule
												   DE_NULL,					// const VkShaderModule              geometryShaderModule
												   *m_fragmentShaderModule,	// const VkShaderModule              fragmentShaderModule
												   *m_renderPass,			// const VkRenderPass                renderPass
												   viewports,				// const std::vector<VkViewport>&    viewports
												   scissors);				// const std::vector<VkRect2D>&      scissors
	}

	// Create vertex buffer
	{
		createQuad();
		const VkDeviceSize				vertexDataSize					= m_vertices.size() * sizeof(tcu::Vec4);

		BufferSuballocation().createTestBuffer(vk, vkDevice, queueFamilyIndex, vertexDataSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, m_context, memAlloc, m_vertexBuffer, MemoryRequirement::HostVisible, m_vertexBufferAlloc);

		// Load vertices into vertex buffer
		deMemcpy(m_vertexBufferAlloc->getHostPtr(), m_vertices.data(), (size_t)vertexDataSize);
		flushAlloc(vk, vkDevice, *m_vertexBufferAlloc);
	}

	// Create command pool
	m_cmdPool = createCommandPool(vk, vkDevice, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, queueFamilyIndex);

	// Create command buffer
	{
		m_cmdBuffer = allocateCommandBuffer(vk, vkDevice, *m_cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

		beginCommandBuffer(vk, *m_cmdBuffer, 0u);

		const VkImageMemoryBarrier		initialImageBarrier				=
		{
			VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,						// VkStructureType			sType;
			DE_NULL,													// const void*				pNext;
			0,															// VkAccessFlags			srcAccessMask;
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,						// VkAccessFlags			dstAccessMask;
			VK_IMAGE_LAYOUT_UNDEFINED,									// VkImageLayout			oldLayout;
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,					// VkImageLayout			newLayout;
			VK_QUEUE_FAMILY_IGNORED,									// deUint32					srcQueueFamilyIndex;
			VK_QUEUE_FAMILY_IGNORED,									// deUint32					destQueueFamilyIndex;
			*m_colorImage,												// VkImage					image;
			{															// VkImageSubresourceRange	subresourceRange;
				VK_IMAGE_ASPECT_COLOR_BIT,								// VkImageAspectFlags		aspectMask;
				0u,														// deUint32					baseMipLevel;
				1u,														// deUint32					mipLevels;
				0u,														// deUint32					baseArraySlice;
				1u														// deUint32					arraySize;
			}
		};

		vk.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 0, (const VkBufferMemoryBarrier*)DE_NULL, 1, &initialImageBarrier);

		beginRenderPass(vk, *m_cmdBuffer, *m_renderPass, *m_framebuffer, makeRect2D(0, 0, m_renderSize.x(), m_renderSize.y()), tcu::Vec4(0.0f));

		const VkDeviceSize				vertexBufferOffset[1]			= { 0 };

		vk.cmdBindPipeline(*m_cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_graphicsPipelines);
		vk.cmdBindDescriptorSets(*m_cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipelineLayout, 0u, 1, &*m_descriptorSet, 0u, DE_NULL);
		vk.cmdBindVertexBuffers(*m_cmdBuffer, 0, 1, &m_vertexBuffer.get(), vertexBufferOffset);
		vk.cmdDraw(*m_cmdBuffer, (deUint32)m_vertices.size(), 1, 0, 0);
		endRenderPass(vk, *m_cmdBuffer);
		copyImageToBuffer(vk, *m_cmdBuffer, *m_colorImage, *m_resultBuffer, m_renderSize);
		endCommandBuffer(vk, *m_cmdBuffer);
	}
}

tcu::TestStatus BufferViewTestInstance::checkResult						(deInt8						factor)
{
	const DeviceInterface&				vk								= m_context.getDeviceInterface();
	const VkDevice						vkDevice						= m_context.getDevice();
	const tcu::TextureFormat			tcuFormat						= mapVkFormat(m_colorFormat);
	de::MovePtr<tcu::TextureLevel>		resultLevel						(new tcu::TextureLevel(tcuFormat, m_renderSize.x(), m_renderSize.y()));

	invalidateAlloc(vk, vkDevice, *m_resultBufferAlloc);
	tcu::copy(*resultLevel, tcu::ConstPixelBufferAccess(resultLevel->getFormat(), resultLevel->getSize(), m_resultBufferAlloc->getHostPtr()));

	tcu::ConstPixelBufferAccess			pixelBuffer						= resultLevel->getAccess();
	for (deInt32 i = 0; i < (deInt32) m_renderSize.x(); ++i)
	{
		tcu::IVec4						pixel							= pixelBuffer.getPixelInt(i, i);
		deInt32							expected						= factor * (m_testCase.elementOffset + i);
		deInt32							actual							= pixel[0];
		if (expected != actual)
		{
			std::ostringstream			errorMessage;
			errorMessage << "BufferView test failed. expected: " << expected << " actual: " << actual;
			return tcu::TestStatus::fail(errorMessage.str());
		}
	}

	return tcu::TestStatus::pass("BufferView test");
}

tcu::TestStatus BufferViewTestInstance::iterate							(void)
{
	const DeviceInterface&				vk								= m_context.getDeviceInterface();
	const VkDevice						vkDevice						= m_context.getDevice();
	const VkQueue						queue							= m_context.getUniversalQueue();

	submitCommandsAndWait(vk, vkDevice, queue, m_cmdBuffer.get());

	tcu::TestStatus						testStatus						= checkResult(1);
	if (testStatus.getCode() != QP_TEST_RESULT_PASS)
		return testStatus;

	// Generate and bind another buffer
	std::vector<deUint32>				uniformData;
	const VkDeviceSize					uniformSize						= m_testCase.bufferSize * sizeof(deUint32);
	const deInt8						factor							= 2;

	generateBuffer(uniformData, m_testCase.bufferSize, factor);
	deMemcpy(m_uniformBufferAlloc->getHostPtr(), uniformData.data(), (size_t)uniformSize);
	flushAlloc(vk, vkDevice, *m_uniformBufferAlloc);

	submitCommandsAndWait(vk, vkDevice, queue, m_cmdBuffer.get());

	return checkResult(factor);
}

class BufferViewTestCase : public vkt::TestCase
{
public:
									BufferViewTestCase					(tcu::TestContext&			testCtx,
																		 const std::string&			name,
																		 const std::string&			description,
																		 BufferViewCaseParams		bufferViewTestInfo)
									: vkt::TestCase						(testCtx, name, description)
									, m_bufferViewTestInfo				(bufferViewTestInfo)
	{}

	virtual							~BufferViewTestCase					(void)
	{}
	virtual	void					initPrograms						(SourceCollections&			programCollection) const;

	virtual TestInstance*			createInstance						(Context&					context) const
	{
		return new BufferViewTestInstance(context, m_bufferViewTestInfo);
	}
private:
	BufferViewCaseParams			m_bufferViewTestInfo;
};

void BufferViewTestCase::initPrograms									(SourceCollections&			programCollection) const
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
		"#extension GL_EXT_texture_buffer : enable\n"
		"layout (set=0, binding=0) uniform highp utextureBuffer u_buffer;\n"
		"layout (location = 0) out highp uint o_color;\n"
		"void main()\n"
		"{\n"
		"	o_color = texelFetch(u_buffer, int(gl_FragCoord.x)).x;\n"
		"}\n");
}

class BufferViewAllFormatsTestInstance : public vkt::TestInstance
{
public:
										BufferViewAllFormatsTestInstance		(Context&					context,
																				 BufferViewCaseParams		testCase);
	virtual								~BufferViewAllFormatsTestInstance		(void);
	virtual tcu::TestStatus				iterate									(void);

private:
	void								checkTexelBufferSupport					(Context&					context,
																				 VkFormat					format,
																				 BufferViewCaseParams		testCase);
	int									getFetchPos								(int fetchPosNdx);
	tcu::TestStatus						checkResult								();
	tcu::TestStatus						checkResultFloat						();
	void								populateSourceBuffer					(const tcu::PixelBufferAccess& access, deUint32 bufferNdx);

private:
	enum
	{
		// some arbitrary points
		SAMPLE_POINT_0 = 6,
		SAMPLE_POINT_1 = 51,
		SAMPLE_POINT_2 = 42,
		SAMPLE_POINT_3 = 25,
	};

	BufferViewCaseParams				m_testCase;
	const VkFormat						m_bufferFormat;

	Move<VkDescriptorSetLayout>			m_descriptorSetLayout;
	Move<VkDescriptorPool>				m_descriptorPool;
	Move<VkDescriptorSet>				m_descriptorSet;

	Move<VkBuffer>						m_uniformBuffer;
	de::MovePtr<vk::Allocation>			m_uniformBufferAlloc;
	Move<VkBufferView>					m_uniformBufferView;
	Move<VkShaderModule>				m_computeShaderModule;
	Move<VkPipelineLayout>				m_pipelineLayout;
	Move<VkPipeline>					m_computePipeline;

	Move<VkCommandPool>					m_cmdPool;
	Move<VkCommandBuffer>				m_cmdBuffer;

	Move<VkBuffer>						m_resultBuffer;
	de::MovePtr<Allocation>				m_resultBufferAlloc;

	de::ArrayBuffer<deUint8>			m_sourceBuffer;
	tcu::ConstPixelBufferAccess			m_sourceView;
};

void BufferViewAllFormatsTestInstance::checkTexelBufferSupport			(Context& context, VkFormat format, BufferViewCaseParams testCase)
{
	const InstanceInterface&	vki				= context.getInstanceInterface();
	const VkPhysicalDevice		physicalDevice	= context.getPhysicalDevice();

	VkFormatProperties					properties;
	properties = getPhysicalDeviceFormatProperties(vki, physicalDevice, format);

	if (!(properties.bufferFeatures & testCase.feature))
		TCU_THROW(NotSupportedError, "Format not supported");

#ifndef CTS_USES_VULKANSC
	if(testCase.bindUsage != VK_BUFFER_USAGE_FLAG_BITS_MAX_ENUM)
	{
		if (!context.isDeviceFunctionalitySupported("VK_KHR_maintenance5"))
			TCU_THROW(NotSupportedError, "Extension VK_KHR_maintenance5 not supported");
	}
#endif
}

BufferViewAllFormatsTestInstance::~BufferViewAllFormatsTestInstance		(void)
{
}

/* Taken from BindingShaderAccessTests.cpp */
void BufferViewAllFormatsTestInstance::populateSourceBuffer				(const tcu::PixelBufferAccess& access, deUint32 bufferNdx)
{
	DE_ASSERT(access.getHeight() == 1);
	DE_ASSERT(access.getDepth() == 1);

	const deInt32 width = access.getWidth();

	for (int x = 0; x < width; ++x)
	{
		int	red		= 255 * x / width;												//!< gradient from 0 -> max (detects large offset errors)
		int	green	= ((x % 2 == 0) ? (127) : (0)) + ((x % 4 < 3) ? (128) : (0));	//!< 3-level M pattern (detects small offset errors)
		int	blue	= 16 * (x % 16);												//!< 16-long triangle wave

		DE_ASSERT(de::inRange(red, 0, 255));
		DE_ASSERT(de::inRange(green, 0, 255));
		DE_ASSERT(de::inRange(blue, 0, 255));

		if (bufferNdx % 2 == 0) red		= 255 - red;
		if (bufferNdx % 3 == 0) green	= 255 - green;
		if (bufferNdx % 4 == 0) blue	= 255 - blue;

		access.setPixel(tcu::IVec4(red, green, blue, 255), x, 0, 0);
	}
}

BufferViewAllFormatsTestInstance::BufferViewAllFormatsTestInstance		(Context&					context,
																		 BufferViewCaseParams		testCase)
										: vkt::TestInstance				(context)
										, m_testCase					(testCase)
										, m_bufferFormat				(testCase.format)
{
	const DeviceInterface&				vk								= context.getDeviceInterface();
	const VkDevice						vkDevice						= context.getDevice();
	const deUint32						queueFamilyIndex				= context.getUniversalQueueFamilyIndex();
	SimpleAllocator						memAlloc						(vk, vkDevice, getPhysicalDeviceMemoryProperties(context.getInstanceInterface(), context.getPhysicalDevice()));

	checkTexelBufferSupport(context, m_bufferFormat, testCase);

	// Create a result buffer
	BufferSuballocation().createTestBuffer(vk, vkDevice, queueFamilyIndex, sizeof(tcu::Vec4[4]), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, m_context, memAlloc, m_resultBuffer, MemoryRequirement::HostVisible, m_resultBufferAlloc);

	// Create descriptors
	{
		const VkDescriptorSetLayoutBinding layoutBindings[2]			=
		{
			{
				0u,														// deUint32					binding;
				VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,						// VkDescriptorType			descriptorType;
				1u,														// deUint32					arraySize;
				VK_SHADER_STAGE_COMPUTE_BIT,							// VkShaderStageFlags		stageFlags;
				DE_NULL													// const VkSampler*			pImmutableSamplers;
			},
			{
				1u,														// deUint32					binding;
				testCase.descType,										// VkDescriptorType			descriptorType;
				1u,														// deUint32					arraySize;
				VK_SHADER_STAGE_COMPUTE_BIT,							// VkShaderStageFlags		stageFlags;
				DE_NULL													// const VkSampler*			pImmutableSamplers;
			},
		};

		const VkDescriptorSetLayoutCreateInfo descriptorLayoutParams	=
		{
			VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,		// VkStructureType			sType;
			DE_NULL,													// const void*				pNext;
			(VkDescriptorSetLayoutCreateFlags)0,
			DE_LENGTH_OF_ARRAY(layoutBindings),							// deUint32					count;
			layoutBindings												// const VkDescriptorSetLayoutBinding pBinding;
		};

		m_descriptorSetLayout = createDescriptorSetLayout(vk, vkDevice, &descriptorLayoutParams);


		// Generate buffer
		const tcu::TextureFormat tcuFormat	= mapVkFormat(m_bufferFormat);

		de::ArrayBuffer<deUint8> sourceBuffer(testCase.bufferSize);
		populateSourceBuffer(tcu::PixelBufferAccess(tcuFormat, tcu::IVec3(testCase.bufferSize / tcuFormat.getPixelSize(), 1, 1), sourceBuffer.getPtr()), 0);

		m_sourceBuffer = sourceBuffer;
		m_sourceView = tcu::ConstPixelBufferAccess(tcuFormat, tcu::IVec3(64, 1, 1), m_sourceBuffer.getPtr());

		BufferSuballocation().createTestBuffer(vk, vkDevice, queueFamilyIndex, sourceBuffer.size(), testCase.createUsage, m_context, memAlloc, m_uniformBuffer, MemoryRequirement::HostVisible, m_uniformBufferAlloc);
		deMemcpy(m_uniformBufferAlloc->getHostPtr(), sourceBuffer.getPtr(), sourceBuffer.size());
		flushAlloc(vk, vkDevice, *m_uniformBufferAlloc);

		VkBufferViewCreateInfo	viewInfo								=
		{
			VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO,					// VkStructureType			sType;
			DE_NULL,													// void*					pNext;
			(VkBufferViewCreateFlags)0,
			*m_uniformBuffer,											// VkBuffer					buffer;
			m_bufferFormat,												// VkFormat					format;
			m_testCase.elementOffset,									// VkDeviceSize				offset;
			VK_WHOLE_SIZE												// VkDeviceSize				range;
		};

#ifndef CTS_USES_VULKANSC
		VkBufferUsageFlags2CreateInfoKHR bindUsageInfo;
		if (testCase.bindUsage != VK_BUFFER_USAGE_FLAG_BITS_MAX_ENUM)
		{
			bindUsageInfo.sType	= VK_STRUCTURE_TYPE_BUFFER_USAGE_FLAGS_2_CREATE_INFO_KHR;	// VkStructureType			sType;
			bindUsageInfo.pNext	= DE_NULL;													// const void*				pNext;
			bindUsageInfo.usage	= testCase.bindUsage;										// VkBufferUsageFlags2KHR	usage;

			viewInfo.pNext		= &bindUsageInfo;
		}
#endif

		m_uniformBufferView = createBufferView(vk, vkDevice, &viewInfo);

		const VkDescriptorPoolSize		descriptorTypes[2]				=
		{
			{
				VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,						// VkDescriptorType			type;
				1														// deUint32					count;
			},
			{
				testCase.descType,										// VkDescriptorType			type;
				1														// deUint32					count;
			}
		};

		const VkDescriptorPoolCreateInfo
										descriptorPoolParams			=
		{
			VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,				// VkStructureType			sType;
			DE_NULL,													// void*					pNext;
			VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,			// VkDescriptorPoolCreateFlags flags;
			1u,															// uint32_t					maxSets;
			DE_LENGTH_OF_ARRAY(descriptorTypes),						// deUint32					count;
			descriptorTypes												// const VkDescriptorTypeCount* pTypeCount
		};

		m_descriptorPool = createDescriptorPool(vk, vkDevice, &descriptorPoolParams);

		const VkDescriptorSetAllocateInfo
										descriptorSetParams				=
		{
			VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
			DE_NULL,
			*m_descriptorPool,
			1u,
			&m_descriptorSetLayout.get(),
		};
		m_descriptorSet = allocateDescriptorSet(vk, vkDevice, &descriptorSetParams);

		const VkDescriptorBufferInfo	outBufferInfo					=
		{
			m_resultBuffer.get(),
			0,
			sizeof(tcu::Vec4[4])
		};

		const VkWriteDescriptorSet		writeDescritporSets[]			=
		{
			{
				VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,					// VkStructureType			sType;
				DE_NULL,												// const void*				pNext;
				*m_descriptorSet,										// VkDescriptorSet			destSet;
				0,														// deUint32					destBinding;
				0,														// deUint32					destArrayElement;
				1u,														// deUint32					count;
				VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,						// VkDescriptorType			descriptorType;
				(const VkDescriptorImageInfo*)DE_NULL,
				&outBufferInfo,
				(const VkBufferView*)DE_NULL,
			},
			{
				VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,					// VkStructureType			sType;
				DE_NULL,												// const void*				pNext;
				*m_descriptorSet,										// VkDescriptorSet			destSet;
				1,														// deUint32					destBinding;
				0,														// deUint32					destArrayElement;
				1u,														// deUint32					count;
				testCase.descType,										// VkDescriptorType			descriptorType;
				(const VkDescriptorImageInfo*)DE_NULL,
				(const VkDescriptorBufferInfo*)DE_NULL,
				&m_uniformBufferView.get(),
			}
		};

		vk.updateDescriptorSets(vkDevice, DE_LENGTH_OF_ARRAY(writeDescritporSets), writeDescritporSets, 0u, DE_NULL);
	}

	// Create pipeline layout
	{
		const VkPipelineLayoutCreateInfo pipelineLayoutParams			=
		{
			VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,				// VkStructureType			sType;
			DE_NULL,													// const void*				pNext;
			(VkPipelineLayoutCreateFlags)0,
			1u,															// deUint32					descriptorSetCount;
			&*m_descriptorSetLayout,									// const VkDescriptorSetLayout* pSetLayouts;
			0u,															// deUint32					pushConstantRangeCount;
			DE_NULL														// const VkPushConstantRange* pPushConstantRanges;
		};

		m_pipelineLayout = createPipelineLayout(vk, vkDevice, &pipelineLayoutParams);
	}

	// Create shaders
	{
		m_computeShaderModule = createShaderModule(vk, vkDevice, m_context.getBinaryCollection().get("comp"), 0);
	}

	// Create pipeline
	{
		m_computePipeline         = makeComputePipeline(vk, vkDevice, m_pipelineLayout.get(), m_computeShaderModule.get());
	}

	// Create command pool
	m_cmdPool = createCommandPool(vk, vkDevice, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, queueFamilyIndex);

	// Create and record a command buffer
	{
		m_cmdBuffer = allocateCommandBuffer(vk, vkDevice, *m_cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

		beginCommandBuffer(vk, *m_cmdBuffer, 0u);

		vk.cmdBindPipeline(*m_cmdBuffer, vk::VK_PIPELINE_BIND_POINT_COMPUTE, *m_computePipeline);
		vk.cmdBindDescriptorSets(*m_cmdBuffer, vk::VK_PIPELINE_BIND_POINT_COMPUTE, *m_pipelineLayout, 0u, 1u, &*m_descriptorSet, 0u, nullptr);

		const vk::VkBufferMemoryBarrier	barrier		=
		{
			vk::VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
			DE_NULL,
			vk::VK_ACCESS_HOST_WRITE_BIT,			// srcAccessMask
			vk::VK_ACCESS_UNIFORM_READ_BIT,			// dstAccessMask
			VK_QUEUE_FAMILY_IGNORED,				// srcQueueFamilyIndex
			VK_QUEUE_FAMILY_IGNORED,				// destQueueFamilyIndex
			*m_resultBuffer,						// buffer
			0u,										// offset
			sizeof(tcu::Vec4[4]),					// size
		};
		const vk::VkBufferMemoryBarrier bufferBarrier =
		{
			vk::VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
			DE_NULL,
			vk::VK_ACCESS_SHADER_WRITE_BIT,			// srcAccessMask
			vk::VK_ACCESS_HOST_READ_BIT,			// dstAccessMask
			VK_QUEUE_FAMILY_IGNORED,				// srcQueueFamilyIndex
			VK_QUEUE_FAMILY_IGNORED,				// destQueueFamilyIndex
			*m_resultBuffer,						// buffer
			(vk::VkDeviceSize)0u,					// offset
			sizeof(tcu::Vec4[4]),					// size
		};

		vk.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0u, 0u, nullptr, 0u, &barrier, 0u, nullptr);
		//vk.cmdDispatch(*m_cmdBuffer, 1u, 1u, 1u);
		vk.cmdDispatch(*m_cmdBuffer, 4u, 1u, 1u);
		vk.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0u, 0u, nullptr, 0u, &bufferBarrier, 0u, nullptr);
		endCommandBuffer(vk, *m_cmdBuffer);
	}
}

int BufferViewAllFormatsTestInstance::getFetchPos (int fetchPosNdx)
{
	static const int fetchPositions[4] =
	{
		SAMPLE_POINT_0,
		SAMPLE_POINT_1,
		SAMPLE_POINT_2,
		SAMPLE_POINT_3,
	};

	return fetchPositions[fetchPosNdx];
}

tcu::TestStatus BufferViewAllFormatsTestInstance::checkResult						()
{
	const DeviceInterface&				vk					= m_context.getDeviceInterface();
	const VkDevice						vkDevice			= m_context.getDevice();
	bool								allResultsOk		= true;

	tcu::UVec4							results[4];
	invalidateAlloc(vk, vkDevice, *m_resultBufferAlloc);
	deMemcpy(results, m_resultBufferAlloc->getHostPtr(), sizeof(tcu::UVec4[4]));

	// verify
	for (int resultNdx = 0; resultNdx < 4; ++resultNdx)
	{
		const tcu::UVec4	result				= results[resultNdx];
		const tcu::UVec4	conversionThreshold	= tcu::UVec4(0);
		tcu::UVec4			reference			= tcu::UVec4(0);

		reference	+= m_sourceView.getPixelUint(getFetchPos(resultNdx), 0, 0);

		if (tcu::boolAny(tcu::greaterThan(tcu::abs(result - reference), conversionThreshold)))
		{
			allResultsOk = false;

			m_context.getTestContext().getLog()
				<< tcu::TestLog::Message
				<< "Test sample " << resultNdx << ": Expected " << reference << ", got " << result
				<< tcu::TestLog::EndMessage;
		}
	}

	if (allResultsOk)
		return tcu::TestStatus::pass("Pass");
	else
		return tcu::TestStatus::fail("Invalid result values");
}

tcu::TestStatus BufferViewAllFormatsTestInstance::checkResultFloat					()
{
	const DeviceInterface&				vk					= m_context.getDeviceInterface();
	const VkDevice						vkDevice			= m_context.getDevice();
	bool								allResultsOk		= true;

	tcu::Vec4							results[4];
	invalidateAlloc(vk, vkDevice, *m_resultBufferAlloc);
	deMemcpy(results, m_resultBufferAlloc->getHostPtr(), sizeof(tcu::Vec4[4]));

	// verify
	for (int resultNdx = 0; resultNdx < 4; ++resultNdx)
	{
		const tcu::Vec4	result				= results[resultNdx];
		const tcu::Vec4	conversionThreshold	= tcu::Vec4(1.0f / 255.0f);
		tcu::Vec4		reference			= tcu::Vec4(0.0f);

		reference	+= m_sourceView.getPixel(getFetchPos(resultNdx), 0, 0);

		if (tcu::boolAny(tcu::greaterThan(tcu::abs(result - reference), conversionThreshold)))
		{
			allResultsOk = false;

			m_context.getTestContext().getLog()
				<< tcu::TestLog::Message
				<< "Test sample " << resultNdx << ": Expected " << reference << ", got " << result
				<< tcu::TestLog::EndMessage;
		}
	}

	if (allResultsOk)
		return tcu::TestStatus::pass("Pass");
	else
		return tcu::TestStatus::fail("Invalid result values");
}

tcu::TestStatus BufferViewAllFormatsTestInstance::iterate							(void)
{
	const DeviceInterface&				vk					= m_context.getDeviceInterface();
	const VkDevice						vkDevice			= m_context.getDevice();
	const VkQueue						queue				= m_context.getUniversalQueue();

	submitCommandsAndWait(vk, vkDevice, queue, m_cmdBuffer.get());

	if (isIntFormat(m_bufferFormat) || isUintFormat(m_bufferFormat))
		return checkResult();
	else
		return checkResultFloat();
}


class BufferViewAllFormatsTestCase : public vkt::TestCase
{
public:
									BufferViewAllFormatsTestCase		(tcu::TestContext&			testCtx,
																		 const std::string&			name,
																		 const std::string&			description,
																		 BufferViewCaseParams		bufferViewTestInfo)
									: vkt::TestCase						(testCtx, name, description)
									, m_bufferViewTestInfo				(bufferViewTestInfo)
	{}

	virtual							~BufferViewAllFormatsTestCase		(void)
	{}
	virtual	void					initPrograms						(SourceCollections&			programCollection) const;
	virtual void					checkSupport						(Context&					context) const
	{
#ifndef CTS_USES_VULKANSC
		if ((m_bufferViewTestInfo.format == VK_FORMAT_A8_UNORM_KHR) ||
			(m_bufferViewTestInfo.format == VK_FORMAT_A1B5G5R5_UNORM_PACK16_KHR))
			context.requireDeviceFunctionality("VK_KHR_maintenance5");
#else
		DE_UNREF(context);
#endif // CTS_USES_VULKANSC
	}

	virtual TestInstance*			createInstance						(Context&					context) const
	{
		return new BufferViewAllFormatsTestInstance(context, m_bufferViewTestInfo);
	}

private:
	BufferViewCaseParams			m_bufferViewTestInfo;
};

const std::string strLayoutFormat										(VkFormat format)
{
	std::ostringstream	buf;

	buf << ", " << image::getShaderImageFormatQualifier(mapVkFormat(format)).c_str();

	return buf.str();
}

void BufferViewAllFormatsTestCase::initPrograms							(SourceCollections&			programCollection) const
{
	std::ostringstream	buf;

	const bool			isIntFmt		= isIntFormat(m_bufferViewTestInfo.format);
	const bool			isUintFmt		= isUintFormat(m_bufferViewTestInfo.format);

	bool				isUniform;
	if (m_bufferViewTestInfo.bindUsage != VK_BUFFER_USAGE_FLAG_BITS_MAX_ENUM)
	{
		isUniform = m_bufferViewTestInfo.bindUsage == VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT ? true : false;
	}
	else
	{
		isUniform = m_bufferViewTestInfo.createUsage == VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT ? true : false;
	}
	const char* const	storageType		= isUniform ? "textureBuffer " : "imageBuffer ";
	const char* const	extraOption		= isUniform ? "" : "readonly ";
	const std::string	stringFmtLayout = isUniform ? "" : strLayoutFormat(m_bufferViewTestInfo.format);
	const char* const	fmtLayout		= isUniform ? "" : stringFmtLayout.c_str();
	const char* const	opName			= isUniform ? "texelFetch" : "imageLoad";
	const char* const	outFormat		= isIntFmt  ? "i"			   : isUintFmt ? "u" : "";
	const char* const	inFormat		= vk::isScaledFormat(m_bufferViewTestInfo.format)? "" : outFormat;

	buf << "#version 440\n"
		<< "#extension GL_EXT_texture_buffer : require\n"
		<< "layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;\n"
		<< "layout(set = 0, binding = 1" << fmtLayout << ") uniform highp " << extraOption << inFormat << storageType << " texelBuffer;\n"
		<< "layout(set = 0, binding = 0, std140) writeonly buffer OutBuf\n"
		<< "{\n"
		<< "	highp " << outFormat << "vec4 read_colors[4];\n"
		<< "} b_out;\n"
		<< "void main (void)\n"
		<< "{\n"
		<< "	highp int quadrant_id = int(gl_WorkGroupID.x);\n"
		<< "	highp " << outFormat << "vec4 result_color;\n"
		<< "	result_color = " << outFormat << "vec4(0);\n"
		<< "	if (quadrant_id == 0)\n"
		<< "		result_color += " << outFormat << "vec4(" << opName << "(texelBuffer, 6));\n"
		<< "	else if (quadrant_id == 1)\n"
		<< "		result_color += " << outFormat << "vec4(" << opName << "(texelBuffer, 51));\n"
		<< "	else if (quadrant_id == 2)\n"
		<< "		result_color += " << outFormat << "vec4(" << opName << "(texelBuffer, 42));\n"
		<< "	else\n"
		<< "		result_color += " << outFormat << "vec4(" << opName << "(texelBuffer, 25));\n"
		<< "	b_out.read_colors[gl_WorkGroupID.x] = result_color;\n"
		<< "}\n";

	programCollection.glslSources.add("comp") << glu::ComputeSource(buf.str());
}

} // anonymous

bool isSupportedImageLoadStore (const tcu::TextureFormat& format)
{
	if (!image::isPackedType(mapTextureFormat(format)))
	{
		switch (format.order)
		{
			case tcu::TextureFormat::RGBA:
				break;
			default:
				return false;
		}

		switch (format.type)
		{
			case tcu::TextureFormat::FLOAT:
			case tcu::TextureFormat::HALF_FLOAT:

			case tcu::TextureFormat::UNSIGNED_INT32:
			case tcu::TextureFormat::UNSIGNED_INT16:
			case tcu::TextureFormat::UNSIGNED_INT8:

			case tcu::TextureFormat::SIGNED_INT32:
			case tcu::TextureFormat::SIGNED_INT16:
			case tcu::TextureFormat::SIGNED_INT8:

			case tcu::TextureFormat::UNORM_INT16:
			case tcu::TextureFormat::UNORM_INT8:

			case tcu::TextureFormat::SNORM_INT16:
			case tcu::TextureFormat::SNORM_INT8:
				break;

			default:
				return false;
		}
	}
	else
	{
		switch (mapTextureFormat(format))
		{
			case VK_FORMAT_A2B10G10R10_UNORM_PACK32:
			case VK_FORMAT_A2B10G10R10_UINT_PACK32:
				break;

			default:
				return false;
		}
	}

	return true;
}

tcu::TestCaseGroup* createBufferViewAccessTests							(tcu::TestContext&			testCtx)
{
	const char* const				bufferTexts[ALLOCATION_KIND_LAST]	=
	{
		"buffer_suballocated",
		"buffer_dedicated_alloc"
	};

	const char* const				imageTexts[ALLOCATION_KIND_LAST]	=
	{
		"image_suballocated",
		"image_dedicated_alloc"
	};

	de::MovePtr<tcu::TestCaseGroup>	bufferViewTests						(new tcu::TestCaseGroup(testCtx, "access", "BufferView Access Tests"));
	de::MovePtr<tcu::TestCaseGroup>	bufferViewAllocationGroupTests[]	=
	{
		de::MovePtr<tcu::TestCaseGroup>(new tcu::TestCaseGroup(testCtx, "suballocation", "BufferView Access Tests for Suballocated Objects")),
		de::MovePtr<tcu::TestCaseGroup>(new tcu::TestCaseGroup(testCtx, "dedicated_alloc", "BufferView Access Tests for Dedicatedly Allocated Objects"))
	};

	for (deUint32 buffersAllocationNdx = 0u; buffersAllocationNdx < ALLOCATION_KIND_LAST; ++buffersAllocationNdx)
	for (deUint32 imageAllocationNdx = 0u; imageAllocationNdx < ALLOCATION_KIND_LAST; ++imageAllocationNdx)
	{
		const deUint32				testCaseGroupNdx					= (buffersAllocationNdx == 0u && imageAllocationNdx == 0u) ? 0u : 1u;
		de::MovePtr<tcu::TestCaseGroup>&
									currentTestsGroup					= bufferViewAllocationGroupTests[testCaseGroupNdx];
		{
			const BufferViewCaseParams	info							=
			{
				512,													// deUint32					bufferSize
				512,													// deUint32					bufferViewSize
				0,														// deUint32					elementOffset
				static_cast<AllocationKind>(buffersAllocationNdx),
				static_cast<AllocationKind>(imageAllocationNdx)
			};
			std::ostringstream		name;
			name << "buffer_view_memory_test_complete";
			if (testCaseGroupNdx != 0)
				name << "_with_" << bufferTexts[buffersAllocationNdx] << "_" << imageTexts[imageAllocationNdx];
			std::ostringstream		description;
			description << "bufferSize: " << info.bufferSize << " bufferViewSize: " << info.bufferViewSize << " bufferView element offset: " << info.elementOffset;
			currentTestsGroup->addChild(new BufferViewTestCase(testCtx, name.str(), description.str(), info));
		}

		{
			const BufferViewCaseParams	info							=
			{
				4096,													// deUint32					bufferSize
				512,													// deUint32					bufferViewSize
				0,														// deUint32					elementOffset
				static_cast<AllocationKind>(buffersAllocationNdx),
				static_cast<AllocationKind>(imageAllocationNdx)
			};
			std::ostringstream		name;
			name << "buffer_view_memory_test_partial_offset0";
			if (testCaseGroupNdx != 0)
				name << "_with_" << bufferTexts[buffersAllocationNdx] << "_" << imageTexts[imageAllocationNdx];
			std::ostringstream		description;
			description << "bufferSize: " << info.bufferSize << " bufferViewSize: " << info.bufferViewSize << " bufferView element offset: " << info.elementOffset;
			currentTestsGroup->addChild(new BufferViewTestCase(testCtx, name.str(), description.str(), info));
		}

		{
			const BufferViewCaseParams	info							=
			{
				4096,													// deUint32					bufferSize
				512,													// deUint32					bufferViewSize
				128,													// deUint32					elementOffset
				static_cast<AllocationKind>(buffersAllocationNdx),
				static_cast<AllocationKind>(imageAllocationNdx)
			};
			std::ostringstream		name;
			name << "buffer_view_memory_test_partial_offset1";
			if (testCaseGroupNdx != 0)
				name << "_with_" << bufferTexts[buffersAllocationNdx] << "_" << imageTexts[imageAllocationNdx];
			std::ostringstream		description;
			description << "bufferSize: " << info.bufferSize << " bufferViewSize: " << info.bufferViewSize << " bufferView element offset: " << info.elementOffset;
			currentTestsGroup->addChild(new BufferViewTestCase(testCtx, name.str(), description.str(), info));
		}
	}

	for (deUint32 subgroupNdx = 0u; subgroupNdx < DE_LENGTH_OF_ARRAY(bufferViewAllocationGroupTests); ++subgroupNdx)
	{
		bufferViewTests->addChild(bufferViewAllocationGroupTests[subgroupNdx].release());
	}

	VkFormat testFormats[] =
	{
		VK_FORMAT_R4G4_UNORM_PACK8,
		VK_FORMAT_R4G4B4A4_UNORM_PACK16,
		VK_FORMAT_B4G4R4A4_UNORM_PACK16,
		VK_FORMAT_R5G6B5_UNORM_PACK16,
		VK_FORMAT_B5G6R5_UNORM_PACK16,
		VK_FORMAT_R5G5B5A1_UNORM_PACK16,
		VK_FORMAT_B5G5R5A1_UNORM_PACK16,
		VK_FORMAT_A1R5G5B5_UNORM_PACK16,
#ifndef CTS_USES_VULKANSC
		VK_FORMAT_A1B5G5R5_UNORM_PACK16_KHR,
#endif // CTS_USES_VULKANSC
		VK_FORMAT_R8_UNORM,
		VK_FORMAT_R8_SNORM,
		VK_FORMAT_R8_USCALED,
		VK_FORMAT_R8_SSCALED,
		VK_FORMAT_R8_UINT,
		VK_FORMAT_R8_SINT,
#ifndef CTS_USES_VULKANSC
		VK_FORMAT_A8_UNORM_KHR,
#endif // CTS_USES_VULKANSC
		VK_FORMAT_R8G8_UNORM,
		VK_FORMAT_R8G8_SNORM,
		VK_FORMAT_R8G8_USCALED,
		VK_FORMAT_R8G8_SSCALED,
		VK_FORMAT_R8G8_UINT,
		VK_FORMAT_R8G8_SINT,
		VK_FORMAT_R8G8B8_UNORM,
		VK_FORMAT_R8G8B8_SNORM,
		VK_FORMAT_R8G8B8_USCALED,
		VK_FORMAT_R8G8B8_SSCALED,
		VK_FORMAT_R8G8B8_UINT,
		VK_FORMAT_R8G8B8_SINT,
		VK_FORMAT_B8G8R8_UNORM,
		VK_FORMAT_B8G8R8_SNORM,
		VK_FORMAT_B8G8R8_USCALED,
		VK_FORMAT_B8G8R8_SSCALED,
		VK_FORMAT_B8G8R8_UINT,
		VK_FORMAT_B8G8R8_SINT,
		VK_FORMAT_R8G8B8A8_UNORM,
		VK_FORMAT_R8G8B8A8_SNORM,
		VK_FORMAT_R8G8B8A8_USCALED,
		VK_FORMAT_R8G8B8A8_SSCALED,
		VK_FORMAT_R8G8B8A8_UINT,
		VK_FORMAT_R8G8B8A8_SINT,
		VK_FORMAT_B8G8R8A8_UNORM,
		VK_FORMAT_B8G8R8A8_SNORM,
		VK_FORMAT_B8G8R8A8_USCALED,
		VK_FORMAT_B8G8R8A8_SSCALED,
		VK_FORMAT_B8G8R8A8_UINT,
		VK_FORMAT_B8G8R8A8_SINT,
		VK_FORMAT_A8B8G8R8_UNORM_PACK32,
		VK_FORMAT_A8B8G8R8_SNORM_PACK32,
		VK_FORMAT_A8B8G8R8_USCALED_PACK32,
		VK_FORMAT_A8B8G8R8_SSCALED_PACK32,
		VK_FORMAT_A8B8G8R8_UINT_PACK32,
		VK_FORMAT_A8B8G8R8_SINT_PACK32,
		VK_FORMAT_A2R10G10B10_UNORM_PACK32,
		VK_FORMAT_A2R10G10B10_SNORM_PACK32,
		VK_FORMAT_A2R10G10B10_USCALED_PACK32,
		VK_FORMAT_A2R10G10B10_SSCALED_PACK32,
		VK_FORMAT_A2R10G10B10_UINT_PACK32,
		VK_FORMAT_A2R10G10B10_SINT_PACK32,
		VK_FORMAT_A2B10G10R10_UNORM_PACK32,
		VK_FORMAT_A2B10G10R10_SNORM_PACK32,
		VK_FORMAT_A2B10G10R10_USCALED_PACK32,
		VK_FORMAT_A2B10G10R10_SSCALED_PACK32,
		VK_FORMAT_A2B10G10R10_UINT_PACK32,
		VK_FORMAT_A2B10G10R10_SINT_PACK32,
		VK_FORMAT_R16_UNORM,
		VK_FORMAT_R16_SNORM,
		VK_FORMAT_R16_USCALED,
		VK_FORMAT_R16_SSCALED,
		VK_FORMAT_R16_UINT,
		VK_FORMAT_R16_SINT,
		VK_FORMAT_R16_SFLOAT,
		VK_FORMAT_R16G16_UNORM,
		VK_FORMAT_R16G16_SNORM,
		VK_FORMAT_R16G16_USCALED,
		VK_FORMAT_R16G16_SSCALED,
		VK_FORMAT_R16G16_UINT,
		VK_FORMAT_R16G16_SINT,
		VK_FORMAT_R16G16_SFLOAT,
		VK_FORMAT_R16G16B16_UNORM,
		VK_FORMAT_R16G16B16_SNORM,
		VK_FORMAT_R16G16B16_USCALED,
		VK_FORMAT_R16G16B16_SSCALED,
		VK_FORMAT_R16G16B16_UINT,
		VK_FORMAT_R16G16B16_SINT,
		VK_FORMAT_R16G16B16_SFLOAT,
		VK_FORMAT_R16G16B16A16_UNORM,
		VK_FORMAT_R16G16B16A16_SNORM,
		VK_FORMAT_R16G16B16A16_USCALED,
		VK_FORMAT_R16G16B16A16_SSCALED,
		VK_FORMAT_R16G16B16A16_UINT,
		VK_FORMAT_R16G16B16A16_SINT,
		VK_FORMAT_R16G16B16A16_SFLOAT,
		VK_FORMAT_R32_UINT,
		VK_FORMAT_R32_SINT,
		VK_FORMAT_R32_SFLOAT,
		VK_FORMAT_R32G32_UINT,
		VK_FORMAT_R32G32_SINT,
		VK_FORMAT_R32G32_SFLOAT,
	};

	{
		const char* const					usageName[]						= { "uniform_texel_buffer", "storage_texel_buffer"};
		const vk::VkBufferUsageFlags		createUsage[]					= { vk::VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT, vk::VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT };
		const vk::VkBufferUsageFlags		bindUsage[]						= { vk::VK_BUFFER_USAGE_FLAG_BITS_MAX_ENUM, vk::VK_BUFFER_USAGE_FLAG_BITS_MAX_ENUM };
		const vk::VkFormatFeatureFlags		feature[]						= { vk::VK_FORMAT_FEATURE_UNIFORM_TEXEL_BUFFER_BIT, vk::VK_FORMAT_FEATURE_STORAGE_TEXEL_BUFFER_BIT };
		const vk::VkDescriptorType			descType[]						= { vk::VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, vk::VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER };

		for (deUint32 usageNdx = 0; usageNdx < DE_LENGTH_OF_ARRAY(createUsage); ++usageNdx)
		{
			de::MovePtr<tcu::TestCaseGroup>	usageGroup		(new tcu::TestCaseGroup(testCtx, usageName[usageNdx], ""));

			for (deUint32 formatIdx = 0; formatIdx < DE_LENGTH_OF_ARRAY(testFormats); formatIdx++)
			{
				const auto			skip	= strlen("VK_FORMAT_");
				const std::string	fmtName	= de::toLower(std::string(getFormatName(testFormats[formatIdx])).substr(skip));

				de::MovePtr<tcu::TestCaseGroup>	formatGroup		(new tcu::TestCaseGroup(testCtx, fmtName.c_str(), ""));

				if (createUsage[usageNdx] == VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT && !isSupportedImageLoadStore(mapVkFormat(testFormats[formatIdx])))
					continue;

				const BufferViewCaseParams	info							=
				{
					512,													// deUint32					bufferSize
					128,													// deUint32					bufferViewSize
					0,														// deUint32					elementOffset
					ALLOCATION_KIND_SUBALLOCATION,							// AllocationKind			bufferAllocationKind
					ALLOCATION_KIND_SUBALLOCATION,							// AllocationKind			imageAllocationKind

					testFormats[formatIdx],									// VkFormat					format
					createUsage[usageNdx],									// VkBufferUsageFlags		createUsage
					bindUsage[usageNdx],									// VkBufferUsageFlags		bindUsage
					feature[usageNdx],										// VkFormatFeatureFlags2KHR	feature
					descType[usageNdx],										// VkDescriptorType			descType
				};

				std::ostringstream		description;
				description << "bufferFormat: " << getFormatName(testFormats[formatIdx]) << " bufferSize: " << info.bufferSize << " bufferViewSize: " << info.bufferViewSize << " bufferView element offset: " << info.elementOffset;

				usageGroup->addChild(new BufferViewAllFormatsTestCase(testCtx, fmtName.c_str(), description.str(), info));
			}

			bufferViewTests->addChild(usageGroup.release());
		}
	}

#ifndef CTS_USES_VULKANSC
	de::MovePtr<tcu::TestCaseGroup> uniformStorageGroup		(new tcu::TestCaseGroup(testCtx, "uniform_storage_texel_buffer", ""));
	{

		const char* const					usageName[]	= { "bind_as_uniform", "bind_as_storage" };
		const vk::VkBufferUsageFlags		createUsage	= vk::VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT | vk::VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT;
		const vk::VkBufferUsageFlags		bindUsage[]	= { vk::VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT, vk::VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT };
		const vk::VkFormatFeatureFlags		feature[]	= { vk::VK_FORMAT_FEATURE_UNIFORM_TEXEL_BUFFER_BIT, vk::VK_FORMAT_FEATURE_STORAGE_TEXEL_BUFFER_BIT };
		const vk::VkDescriptorType			descType[]	= { vk::VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, vk::VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER };

		for (deUint32 usageNdx = 0; usageNdx < DE_LENGTH_OF_ARRAY(usageName); ++usageNdx)
		{
			de::MovePtr<tcu::TestCaseGroup>	usageGroup		(new tcu::TestCaseGroup(testCtx, usageName[usageNdx], ""));

			for (deUint32 formatIdx = 0; formatIdx < DE_LENGTH_OF_ARRAY(testFormats); formatIdx++)
			{
				const auto			skip	= strlen("VK_FORMAT_");
				const std::string	fmtName	= de::toLower(std::string(getFormatName(testFormats[formatIdx])).substr(skip));

				de::MovePtr<tcu::TestCaseGroup>	formatGroup(new tcu::TestCaseGroup(testCtx, fmtName.c_str(), ""));

				if (bindUsage[usageNdx] == VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT && !isSupportedImageLoadStore(mapVkFormat(testFormats[formatIdx])))
					continue;

				const BufferViewCaseParams	info =
				{
					512,													// deUint32					bufferSize
					128,													// deUint32					bufferViewSize
					0,														// deUint32					elementOffset
					ALLOCATION_KIND_SUBALLOCATION,							// AllocationKind			bufferAllocationKind
					ALLOCATION_KIND_SUBALLOCATION,							// AllocationKind			imageAllocationKind

					testFormats[formatIdx],									// VkFormat					format
					createUsage,											// VkBufferUsageFlags		createUsage
					bindUsage[usageNdx],									// VkBufferUsageFlags		bindUsage
					feature[usageNdx],										// VkFormatFeatureFlags2KHR	feature
					descType[usageNdx],										// VkDescriptorType			descType
				};

				std::ostringstream		description;
				description << "bufferFormat: " << getFormatName(testFormats[formatIdx]) << " bufferSize: " << info.bufferSize << " bufferViewSize: " << info.bufferViewSize << " bufferView element offset: " << info.elementOffset;

				usageGroup->addChild(new BufferViewAllFormatsTestCase(testCtx, fmtName.c_str(), description.str(), info));
			}

			uniformStorageGroup->addChild(usageGroup.release());
		}
	}

	bufferViewTests->addChild(uniformStorageGroup.release());
#endif

	return bufferViewTests.release();
}

} // api
} // vkt
