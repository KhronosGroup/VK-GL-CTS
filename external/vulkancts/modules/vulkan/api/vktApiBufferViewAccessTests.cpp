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
		BufferDedicatedAllocation().createTestBuffer(m_pixelDataSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT, m_context, memAlloc, m_resultBuffer, MemoryRequirement::HostVisible, m_resultBufferAlloc);
	}
	else
	{
		BufferSuballocation().createTestBuffer(m_pixelDataSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT, m_context, memAlloc, m_resultBuffer, MemoryRequirement::HostVisible, m_resultBufferAlloc);
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

		BufferSuballocation().createTestBuffer(uniformSize, VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT, m_context, memAlloc, m_uniformBuffer, MemoryRequirement::HostVisible, m_uniformBufferAlloc);
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

		BufferSuballocation().createTestBuffer(vertexDataSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, m_context, memAlloc, m_vertexBuffer, MemoryRequirement::HostVisible, m_vertexBufferAlloc);

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

} // anonymous

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
	return bufferViewTests.release();
}

} // api
} // vkt
