/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2022 The Khronos Group Inc.
 * Copyright (c) 2022 Google Inc.
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
 * \file vktPipelineDescriptorLimits.cpp
 * \brief Descriptor limit tests
 *//*--------------------------------------------------------------------*/

#include "vktPipelineDescriptorLimitsTests.hpp"
#include "vktPipelineClearUtil.hpp"

#include "vkBuilderUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkTypeUtil.hpp"

#include "tcuImageCompare.hpp"
#include "tcuTestLog.hpp"
#include "tcuTextureUtil.hpp"

#include "deUniquePtr.hpp"

#include <array>

namespace vkt
{
namespace pipeline
{

using namespace vk;

namespace
{

enum class TestType
{
	Samplers			= 0,
	UniformBuffers		= 1,
	StorageBuffers		= 2,
	SampledImages		= 3,
	StorageImages		= 4,
	InputAttachments	= 5
};

inline VkImageCreateInfo makeImageCreateInfo (const tcu::IVec2& size, const VkFormat format, const VkImageUsageFlags usage)
{
	const VkImageCreateInfo	imageParams	=
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,	// VkStructureType			sType;
		DE_NULL,								// const void*				pNext;
		(VkImageCreateFlags)0,					// VkImageCreateFlags		flags;
		VK_IMAGE_TYPE_2D,						// VkImageType				imageType;
		format,									// VkFormat					format;
		makeExtent3D(size.x(), size.y(), 1),	// VkExtent3D				extent;
		1u,										// deUint32					mipLevels;
		1u,										// deUint32					arrayLayers;
		VK_SAMPLE_COUNT_1_BIT,					// VkSampleCountFlagBits	samples;
		VK_IMAGE_TILING_OPTIMAL,				// VkImageTiling			tiling;
		usage,									// VkImageUsageFlags		usage;
		VK_SHARING_MODE_EXCLUSIVE,				// VkSharingMode			sharingMode;
		0u,										// deUint32					queueFamilyIndexCount;
		DE_NULL,								// const deUint32*			pQueueFamilyIndices;
		VK_IMAGE_LAYOUT_UNDEFINED,				// VkImageLayout			initialLayout;
	};
	return imageParams;
}

tcu::TextureLevel generateColorImage (const VkFormat format, const tcu::IVec2 &renderSize, const tcu::Vec4 color)
{
	tcu::TextureLevel image(mapVkFormat(format), renderSize.x(), renderSize.y());
	tcu::clear(image.getAccess(), color);

	return image;
}

RenderPassWrapper makeRenderPassInputAttachment (const DeviceInterface&				vk,
												  const VkDevice					device,
												  const PipelineConstructionType	pipelineConstructionType,
												  const VkFormat					colorFormat)
{
	const VkAttachmentDescription				colorAttachmentDescription	=
	{
		(VkAttachmentDescriptionFlags)0,			// VkAttachmentDescriptionFlags	flags
		colorFormat,								// VkFormat						format
		VK_SAMPLE_COUNT_1_BIT,						// VkSampleCountFlagBits		samples
		VK_ATTACHMENT_LOAD_OP_CLEAR,				// VkAttachmentLoadOp			loadOp
		VK_ATTACHMENT_STORE_OP_STORE,				// VkAttachmentStoreOp			storeOp
		VK_ATTACHMENT_LOAD_OP_DONT_CARE,			// VkAttachmentLoadOp			stencilLoadOp
		VK_ATTACHMENT_STORE_OP_DONT_CARE,			// VkAttachmentStoreOp			stencilStoreOp
		VK_IMAGE_LAYOUT_UNDEFINED,					// VkImageLayout				initialLayout
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL	// VkImageLayout				finalLayout
	};

	const VkAttachmentDescription				inputAttachmentDescription	=
	{
		VkAttachmentDescriptionFlags(0),			// VkAttachmentDescriptionFlags	flags;
		colorFormat,								// VkFormat						format;
		VK_SAMPLE_COUNT_1_BIT,						// VkSampleCountFlagBits		samples;
		VK_ATTACHMENT_LOAD_OP_LOAD,					// VkAttachmentLoadOp			loadOp;
		VK_ATTACHMENT_STORE_OP_STORE,				// VkAttachmentStoreOp			storeOp;
		VK_ATTACHMENT_LOAD_OP_DONT_CARE,			// VkAttachmentLoadOp			stencilLoadOp;
		VK_ATTACHMENT_STORE_OP_DONT_CARE,			// VkAttachmentStoreOp			stencilStoreOp;
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,	// VkImageLayout				initialLayout;
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL	// VkImageLayout				finalLayout;
	};

	const std::vector<VkAttachmentDescription>	attachmentDescriptions		= { inputAttachmentDescription, inputAttachmentDescription, colorAttachmentDescription };

	const std::vector<VkAttachmentReference>	inputAttachmentReferences	= { { 0u, inputAttachmentDescription.finalLayout }, { 1u, inputAttachmentDescription.finalLayout } };

	const VkAttachmentReference					colorAttachmentReference	= { 2u, colorAttachmentDescription.finalLayout };

	const VkSubpassDescription					subpassDescription			=
	{
		(VkSubpassDescriptionFlags)0,								// VkSubpassDescriptionFlags	flags
		VK_PIPELINE_BIND_POINT_GRAPHICS,							// VkPipelineBindPoint			pipelineBindPoint
		static_cast<deUint32>(inputAttachmentReferences.size()),	// deUint32						inputAttachmentCount
		inputAttachmentReferences.data(),							// const VkAttachmentReference*	pInputAttachments
		1u,															// deUint32						colorAttachmentCount
		&colorAttachmentReference,									// const VkAttachmentReference*	pColorAttachments
		DE_NULL,													// const VkAttachmentReference*	pResolveAttachments
		DE_NULL,													// const VkAttachmentReference*	pDepthStencilAttachment
		0u,															// deUint32						preserveAttachmentCount
		DE_NULL														// const deUint32*				pPreserveAttachments
	};

	const VkRenderPassCreateInfo				renderPassInfo				=
	{
		VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,	// VkStructureType					sType
		DE_NULL,									// const void*						pNext
		(VkRenderPassCreateFlags)0,					// VkRenderPassCreateFlags			flags
		(deUint32)attachmentDescriptions.size(),	// deUint32							attachmentCount
		attachmentDescriptions.data(),				// const VkAttachmentDescription*	pAttachments
		1u,											// deUint32							subpassCount
		&subpassDescription,						// const VkSubpassDescription*		pSubpasses
		0u,											// deUint32							dependencyCount
		DE_NULL										// const VkSubpassDependency*		pDependencies
	};

	return RenderPassWrapper(pipelineConstructionType, vk, device, &renderPassInfo);
}

struct TestParams
{
	TestParams	(const PipelineConstructionType	pipelineConstructionType,
				const TestType					testType,
				const bool						useCompShader,
				const tcu::IVec2				framebufferSize,
				const deUint32					descCount)
				: m_pipelineConstructionType	(pipelineConstructionType)
				, m_testType					(testType)
				, m_useCompShader				(useCompShader)
				, m_framebufferSize				(framebufferSize)
				, m_descCount					(descCount)
	{}

	const PipelineConstructionType	m_pipelineConstructionType;
	const TestType					m_testType;
	const bool						m_useCompShader;
	const tcu::IVec2				m_framebufferSize;
	const deUint32					m_descCount;

	deUint32 getDescCount() const
	{
		deUint32 descCnt = m_descCount;

		if (m_testType == TestType::StorageBuffers && m_useCompShader)
			descCnt = m_descCount - 1u;

		return descCnt;
	}
};

class DescriptorLimitTestInstance : public vkt::TestInstance
{
public:
							DescriptorLimitTestInstance		(Context&						context,
															const TestParams&				params)
															: vkt::TestInstance				(context)
															, m_params						(params)
							{}

							~DescriptorLimitTestInstance	()
							{}

	virtual tcu::TestStatus	iterate							(void);

private:
	struct BufferInfo
	{
		tcu::Vec4	color;
	};
	TestParams	m_params;
};

tcu::TestStatus DescriptorLimitTestInstance::iterate (void)
{
	tcu::TestLog&							log										= m_context.getTestContext().getLog();
	const InstanceInterface&				vki										= m_context.getInstanceInterface();
	const DeviceInterface&					vk										= m_context.getDeviceInterface();
	const VkPhysicalDevice					physicalDevice							= m_context.getPhysicalDevice();
	const VkDevice							vkDevice								= m_context.getDevice();
	Allocator&								allocator								= m_context.getDefaultAllocator();
	const VkQueue							queue									= m_context.getUniversalQueue();
	const deUint32							queueFamilyIndex						= m_context.getUniversalQueueFamilyIndex();
	VkFormat								colorFormat								= VK_FORMAT_R8G8B8A8_UNORM;

	// Pick correct test parameters based on test type
	const VkShaderStageFlags				shaderStageFlags						= m_params.m_useCompShader ? VkShaderStageFlags(VK_SHADER_STAGE_COMPUTE_BIT) : VkShaderStageFlags(VK_SHADER_STAGE_FRAGMENT_BIT);
	const VkPipelineStageFlags				pipelineStageFlags						= m_params.m_useCompShader ? VkPipelineStageFlags(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT) : VkPipelineStageFlags(VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

	const VkImageUsageFlags					imageFlags								= m_params.m_testType == TestType::InputAttachments
																					? VkImageUsageFlags(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT)
																					: m_params.m_testType == TestType::StorageImages
																					? VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT
																					: VkImageUsageFlags(VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);

	const VkImageLayout						finalImageLayout						= m_params.m_testType == TestType::InputAttachments
																					? VkImageLayout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
																					: m_params.m_testType == TestType::StorageImages
																					? VkImageLayout(VK_IMAGE_LAYOUT_GENERAL)
																					: VkImageLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	// Create shaders
	ShaderWrapper							vertexShaderModule						= ShaderWrapper(vk, vkDevice, m_context.getBinaryCollection().get("vert"), 0u);
	ShaderWrapper							testedShaderModule						= ShaderWrapper(vk, vkDevice, m_context.getBinaryCollection().get("test"), 0u);

	// Create images
	const VkImageSubresourceRange			colorSubresourceRange					= makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
	const Move<VkImage>						colorImage								(makeImage(vk, vkDevice, makeImageCreateInfo(m_params.m_framebufferSize, colorFormat, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT)));
	const de::MovePtr<Allocation>			colorImageAlloc							(bindImage(vk, vkDevice, allocator, *colorImage, MemoryRequirement::Any));
	const Move<VkImageView>					colorImageView							(makeImageView(vk, vkDevice, *colorImage, VK_IMAGE_VIEW_TYPE_2D, colorFormat, colorSubresourceRange));

	const Move<VkImage>						inputImages[2]							{ (makeImage(vk, vkDevice, makeImageCreateInfo(m_params.m_framebufferSize, colorFormat, imageFlags)))
																					, (makeImage(vk, vkDevice, makeImageCreateInfo(m_params.m_framebufferSize, colorFormat, imageFlags))) };
	const de::MovePtr<Allocation>			inputImageAllocs[2]						{ (bindImage(vk, vkDevice, allocator, *inputImages[0], MemoryRequirement::Any))
																					, (bindImage(vk, vkDevice, allocator, *inputImages[1], MemoryRequirement::Any)) };
	Move<VkImageView>						inputImageViews[2]						{ (makeImageView(vk, vkDevice, *inputImages[0], VK_IMAGE_VIEW_TYPE_2D, colorFormat, colorSubresourceRange))
																					, (makeImageView(vk, vkDevice, *inputImages[1], VK_IMAGE_VIEW_TYPE_2D, colorFormat, colorSubresourceRange)) };

	std::array<tcu::Vec4, 2>				testColors								{ tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f), tcu::Vec4(0.0f, 1.0f, 0.0f, 1.0f) };

	const deUint32							descCount								= m_params.getDescCount();

	for (int i = 0; i < 2; i++)
	{
		clearColorImage(vk, vkDevice, queue, queueFamilyIndex, inputImages[i].get(),
			testColors[i],
			VK_IMAGE_LAYOUT_UNDEFINED,
			finalImageLayout,
			pipelineStageFlags);
	}

	std::vector<VkImage>					images;
	std::vector<VkImageView>				attachmentImages;

	// Create Samplers
	const tcu::Sampler						sampler = tcu::Sampler(tcu::Sampler::CLAMP_TO_EDGE, tcu::Sampler::CLAMP_TO_EDGE, tcu::Sampler::CLAMP_TO_EDGE, tcu::Sampler::NEAREST, tcu::Sampler::NEAREST, 0.0f, true, tcu::Sampler::COMPAREMODE_NONE, 0, tcu::Vec4(0.0f, 0.0f, 0.0f, 0.0f), true);
	const tcu::TextureFormat				texFormat								= mapVkFormat(colorFormat);
	const VkSamplerCreateInfo				samplerParams							= mapSampler(sampler, texFormat);

	Move<VkSampler>							samplers[2]								= { createSampler(vk, vkDevice, &samplerParams)
																					, createSampler(vk, vkDevice, &samplerParams) };

	// Create buffers
	const deUint32							bufferElementSize						= static_cast<deUint32>(sizeof(tcu::Vec4));

	const Move<VkBuffer>					uboBuffers[2]							{ (makeBuffer(vk, vkDevice, bufferElementSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT))
																					, (makeBuffer(vk, vkDevice, bufferElementSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT)) };

	const Move<VkBuffer>					ssboBuffers[2]							{ (makeBuffer(vk, vkDevice, bufferElementSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT))
																					, (makeBuffer(vk, vkDevice, bufferElementSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT)) };

	const Move<VkBuffer>					compBufferResult						(makeBuffer(vk, vkDevice, bufferElementSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT));

	const de::MovePtr<Allocation>			uboBufferAllocs[2]						{ (bindBuffer(vk, vkDevice, allocator, *uboBuffers[0], MemoryRequirement::HostVisible))
																					, (bindBuffer(vk, vkDevice, allocator, *uboBuffers[1], MemoryRequirement::HostVisible)) };

	const de::MovePtr<Allocation>			ssboBufferAllocs[2]						{ (bindBuffer(vk, vkDevice, allocator, *ssboBuffers[0], MemoryRequirement::HostVisible))
																					, (bindBuffer(vk, vkDevice, allocator, *ssboBuffers[1], MemoryRequirement::HostVisible)) };

	const de::MovePtr<Allocation>			ssboBufferAllocResult					(bindBuffer(vk, vkDevice, allocator, *compBufferResult, MemoryRequirement::HostVisible));

	// Fill buffers
	{
		char*	pPosUbos[2]				= { static_cast<char*>(uboBufferAllocs[0]->getHostPtr())
										, static_cast<char*>(uboBufferAllocs[1]->getHostPtr()) };

		char*	pPosSsbos[2]			= { static_cast<char*>(ssboBufferAllocs[0]->getHostPtr())
										, static_cast<char*>(ssboBufferAllocs[1]->getHostPtr()) };

		char*	pPosSsboResult			= static_cast<char*>(ssboBufferAllocResult->getHostPtr());

		*((tcu::Vec4*)pPosUbos[0])		= testColors[0];
		*((tcu::Vec4*)pPosUbos[1])		= testColors[1];

		flushAlloc(vk, vkDevice, *uboBufferAllocs[0]);
		flushAlloc(vk, vkDevice, *uboBufferAllocs[1]);

		*((tcu::Vec4*)pPosSsbos[0])		= testColors[0];
		*((tcu::Vec4*)pPosSsbos[1])		= testColors[1];

		flushAlloc(vk, vkDevice, *ssboBufferAllocs[0]);
		flushAlloc(vk, vkDevice, *ssboBufferAllocs[1]);

		*((tcu::Vec4*)pPosSsboResult)	= tcu::Vec4(0.0f, 0.0f, 0.0f, 0.0f);

		flushAlloc(vk, vkDevice, *ssboBufferAllocResult);
	}

	if (m_params.m_testType == TestType::InputAttachments)
	{
		for (deUint32 image = 0; image < 2; image++)
		{
			images.push_back(*inputImages[image]);
			attachmentImages.push_back(*inputImageViews[image]);
		}
	}

	images.push_back(*colorImage);
	attachmentImages.push_back(*colorImageView);

	// Result image buffer for fragment shader run
	const VkDeviceSize						resultImageBufferSizeBytes				= tcu::getPixelSize(mapVkFormat(colorFormat)) * m_params.m_framebufferSize.x() * m_params.m_framebufferSize.y();
	const Move<VkBuffer>					resultImageBuffer						(makeBuffer(vk, vkDevice, resultImageBufferSizeBytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT));
	const de::MovePtr<Allocation>			resultImageBufferAlloc					(bindBuffer(vk, vkDevice, allocator, *resultImageBuffer, MemoryRequirement::HostVisible));

	// Create vertex buffer
	const deUint32							numVertices								= 6;
	const VkDeviceSize						vertexBufferSizeBytes					= 256;
	Move<VkBuffer>							vertexBuffer							= (makeBuffer(vk, vkDevice, vertexBufferSizeBytes, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT));
	de::MovePtr<Allocation>					vertexBufferAlloc						= (bindBuffer(vk, vkDevice, allocator, *vertexBuffer, MemoryRequirement::HostVisible));

	{
		tcu::Vec4* const	pVertices	= reinterpret_cast<tcu::Vec4*>(vertexBufferAlloc->getHostPtr());

		pVertices[0]	= tcu::Vec4( 1.0f, -1.0f, 0.0f, 1.0f);
		pVertices[1]	= tcu::Vec4(-1.0f, -1.0f, 0.0f, 1.0f);
		pVertices[2]	= tcu::Vec4(-1.0f,  1.0f, 0.0f, 1.0f);
		pVertices[3]	= tcu::Vec4(-1.0f,  1.0f, 0.0f, 1.0f);
		pVertices[4]	= tcu::Vec4( 1.0f,  1.0f, 0.0f, 1.0f);
		pVertices[5]	= tcu::Vec4( 1.0f, -1.0f, 0.0f, 1.0f);

		flushAlloc(vk, vkDevice, *vertexBufferAlloc);
	}

	// Descriptor pool and descriptor set
	DescriptorPoolBuilder				poolBuilder;

	// If compute pipeline is used for testing something else than SSBOs,
	// one SSBO descriptor is still needed for writing of the test result.
	if (m_params.m_testType != TestType::StorageBuffers && m_params.m_useCompShader)
	{
		poolBuilder.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1u);
	}

	if (m_params.m_testType == TestType::Samplers)
	{
		poolBuilder.addType(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, descCount);
	}

	if (m_params.m_testType == TestType::UniformBuffers)
	{
		poolBuilder.addType(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, descCount);
	}

	if (m_params.m_testType == TestType::StorageBuffers)
	{
		// We must be an extra careful here.
		// Since the result buffer as well the input buffers are allocated from the same descriptor pool
		// full descriptor count should be used while allocating the pool when compute shader is used.
		poolBuilder.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, m_params.m_descCount);
	}

	if (m_params.m_testType == TestType::SampledImages)
	{
		poolBuilder.addType(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, descCount);
	}

	if (m_params.m_testType == TestType::StorageImages)
	{
		poolBuilder.addType(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, descCount);
	}

	if (m_params.m_testType == TestType::InputAttachments)
	{
		poolBuilder.addType(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, descCount);
	}

	const Move<VkDescriptorPool>			descriptorPool							= poolBuilder.build(vk, vkDevice, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u + (m_params.m_useCompShader ? 1u : 0u));

	DescriptorSetLayoutBuilder				layoutBuilderAttachments;

	if (m_params.m_testType == TestType::Samplers)
	{
		for (uint32_t i = 0; i < descCount; i++)
		{
			layoutBuilderAttachments.addSingleBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, shaderStageFlags);
		}
	}

	if (m_params.m_testType == TestType::UniformBuffers)
	{
		for (uint32_t i = 0; i < descCount; i++)
		{
			layoutBuilderAttachments.addSingleBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, shaderStageFlags);
		}
	}

	if (m_params.m_testType == TestType::StorageBuffers)
	{
		for (uint32_t i = 0; i < descCount; i++)
		{
			layoutBuilderAttachments.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, shaderStageFlags);
		}
	}

	if (m_params.m_testType == TestType::SampledImages)
	{
		for (uint32_t i = 0; i < descCount; i++)
		{
			layoutBuilderAttachments.addSingleBinding(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, shaderStageFlags);
		}
	}

	if (m_params.m_testType == TestType::StorageImages)
	{
		for (uint32_t i = 0; i < descCount; i++)
		{
			layoutBuilderAttachments.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, shaderStageFlags);
		}
	}

	if (m_params.m_testType == TestType::InputAttachments)
	{
		for (uint32_t i = 0; i < descCount; i++)
		{
			layoutBuilderAttachments.addSingleBinding(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, VK_SHADER_STAGE_FRAGMENT_BIT);
		}
	}

	const Move<VkDescriptorSetLayout>		descriptorSetLayout						= layoutBuilderAttachments.build(vk, vkDevice);
	const Move<VkDescriptorSet>				descriptorSet							= makeDescriptorSet(vk, vkDevice, descriptorPool.get(), descriptorSetLayout.get());


	DescriptorSetLayoutBuilder				layoutBuilderAttachmentsResult;

	Move<VkDescriptorSetLayout>				descriptorSetLayoutResult;
	Move<VkDescriptorSet>					descriptorSetResult;

	if (m_params.m_useCompShader)
	{
		layoutBuilderAttachmentsResult.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);

		descriptorSetLayoutResult	= layoutBuilderAttachmentsResult.build(vk, vkDevice);
		descriptorSetResult			= makeDescriptorSet(vk, vkDevice, descriptorPool.get(), descriptorSetLayoutResult.get());
	}

	// Setup renderpass and framebuffer.
	RenderPassWrapper						renderPass;
	if (m_params.m_testType == TestType::InputAttachments)
		renderPass = (makeRenderPassInputAttachment(vk, vkDevice, m_params.m_pipelineConstructionType, colorFormat));
	else
		renderPass = (RenderPassWrapper(m_params.m_pipelineConstructionType, vk, vkDevice, colorFormat));

	renderPass.createFramebuffer(vk, vkDevice, static_cast<deUint32>(attachmentImages.size()), images.data(), attachmentImages.data(), m_params.m_framebufferSize.x(), m_params.m_framebufferSize.y());

	// Command buffer
	const Move<VkCommandPool>				cmdPool									(createCommandPool(vk, vkDevice, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex));
	const Move<VkCommandBuffer>				cmdBuffer								(allocateCommandBuffer(vk, vkDevice, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

	std::vector<VkClearValue>				clearColorValues;

	if (m_params.m_testType == TestType::InputAttachments)
	{
		clearColorValues.push_back(defaultClearValue(colorFormat));
		clearColorValues.push_back(defaultClearValue(colorFormat));
	}

	clearColorValues.push_back(defaultClearValue(colorFormat));

	const VkDeviceSize						vertexBufferOffset						= 0ull;

	// Bind buffers
	const vk::VkDescriptorImageInfo			imageInfos[2]							= { makeDescriptorImageInfo(*samplers[0], *inputImageViews[0], m_params.m_testType == TestType::StorageImages
																					? VK_IMAGE_LAYOUT_GENERAL
																					: VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
																					, makeDescriptorImageInfo(*samplers[1], *inputImageViews[1], m_params.m_testType == TestType::StorageImages
																					? VK_IMAGE_LAYOUT_GENERAL
																					: VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) };

	const vk::VkDescriptorBufferInfo		uboInfos[2]								= { makeDescriptorBufferInfo(*uboBuffers[0], 0u, bufferElementSize)
																					, makeDescriptorBufferInfo(*uboBuffers[1], 0u, bufferElementSize) };

	const vk::VkDescriptorBufferInfo		ssboInfos[2]							= { makeDescriptorBufferInfo(*ssboBuffers[0], 0u, bufferElementSize)
																					, makeDescriptorBufferInfo(*ssboBuffers[1], 0u, bufferElementSize) };

	const vk::VkDescriptorBufferInfo		ssboInfoResult							= makeDescriptorBufferInfo(*compBufferResult, 0u, bufferElementSize);

	DescriptorSetUpdateBuilder				updateBuilder;

	if (m_params.m_useCompShader)
	{
		updateBuilder.writeSingle(*descriptorSetResult, DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &ssboInfoResult);
	}

	if (m_params.m_testType == TestType::Samplers)
	{
		for (deUint32 bufferID = 0; bufferID < descCount - 1u; bufferID++)
		{
			updateBuilder.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(bufferID), VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &imageInfos[0]);
		}

		updateBuilder.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(descCount - 1u), VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &imageInfos[1]);
	}

	if (m_params.m_testType == TestType::UniformBuffers)
	{
		for (deUint32 bufferID = 0; bufferID < descCount - 1u; bufferID++)
		{
			updateBuilder.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(bufferID), VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &uboInfos[0]);
		}

		updateBuilder.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(descCount - 1u), VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &uboInfos[1]);
	}

	if (m_params.m_testType == TestType::StorageBuffers)
	{
		for (deUint32 bufferID = 0; bufferID < (descCount - 1u); bufferID++)
		{
			updateBuilder.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(bufferID), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &ssboInfos[0]);
		}

		updateBuilder.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(descCount - 1u), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &ssboInfos[1]);
	}

	if (m_params.m_testType == TestType::SampledImages)
	{
		for (deUint32 bufferID = 0; bufferID < descCount - 1u; bufferID++)
		{
			updateBuilder.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(bufferID), VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, &imageInfos[0]);
		}

		updateBuilder.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(descCount - 1u), VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, &imageInfos[1]);
	}

	if (m_params.m_testType == TestType::StorageImages)
	{
		for (deUint32 bufferID = 0; bufferID < descCount - 1u; bufferID++)
		{
			updateBuilder.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(bufferID), VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &imageInfos[0]);
		}

		updateBuilder.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(descCount - 1u), VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &imageInfos[1]);
	}

	if (m_params.m_testType == TestType::InputAttachments)
	{
		for (deUint32 bufferID = 0; bufferID < descCount - 1u; bufferID++)
		{
			updateBuilder.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(bufferID), VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, &imageInfos[0]);
		}

		updateBuilder.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(descCount - 1u), VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, &imageInfos[1]);
	}

	updateBuilder.update(vk, vkDevice);

	// Create pipeline layout
	std::vector<VkDescriptorSetLayout>		descSetLayouts							= { descriptorSetLayout.get() };

	if (m_params.m_useCompShader)
	{
		descSetLayouts.push_back(descriptorSetLayoutResult.get());
	}

	const VkPipelineLayoutCreateInfo		pipelineLayoutInfo						=
	{
		VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,	// VkStructureType				sType;
		DE_NULL,										// const void*					pNext;
		0u,												// VkPipelineLayoutCreateFlags	flags;
		static_cast<deUint32>(descSetLayouts.size()),	// deUint32						descriptorSetCount;
		descSetLayouts.data(),							// const VkDescriptorSetLayout*	pSetLayouts;
		0u,												// deUint32						pushConstantRangeCount;
		DE_NULL											// const VkPushDescriptorRange*	pPushDescriptorRanges;
	};

	const PipelineLayoutWrapper				pipelineLayout							(m_params.m_pipelineConstructionType, vk, vkDevice, &pipelineLayoutInfo);
	Move<VkPipeline>						computePipeline							{};
	GraphicsPipelineWrapper					graphicsPipelineWrapper					{ vki, vk, physicalDevice, vkDevice, m_context.getDeviceExtensions(), m_params.m_pipelineConstructionType };

	if (m_params.m_useCompShader)
	{
		computePipeline = (makeComputePipeline(vk, vkDevice, pipelineLayout.get(), testedShaderModule.getModule()));
	}
	else
	{
		const std::vector<VkViewport>	viewports	{ makeViewport(m_params.m_framebufferSize) };
		const std::vector<VkRect2D>		scissors	{ makeRect2D(m_params.m_framebufferSize) };
		VkSampleMask					sampleMask	= 0x1;

		const VkPipelineMultisampleStateCreateInfo	multisampleStateCreateInfo
		{
			VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,	// VkStructureType							sType
			DE_NULL,													// const void*								pNext
			0u,															// VkPipelineMultisampleStateCreateFlags	flags
			VK_SAMPLE_COUNT_1_BIT,										// VkSampleCountFlagBits					rasterizationSamples
			DE_FALSE,													// VkBool32									sampleShadingEnable
			0.0f,														// float									minSampleShading
			&sampleMask,												// const VkSampleMask*						pSampleMask
			DE_FALSE,													// VkBool32									alphaToCoverageEnable
			DE_FALSE,													// VkBool32									alphaToOneEnable
		};

		graphicsPipelineWrapper.setDefaultDepthStencilState()
			.setDefaultColorBlendState()
			.setDefaultRasterizationState()
			.setupVertexInputState()
			.setupPreRasterizationShaderState(viewports,
				scissors,
				pipelineLayout,
				renderPass.get(),
				0u,
				vertexShaderModule)
			.setupFragmentShaderState(pipelineLayout,
				renderPass.get(),
				0u,
				testedShaderModule,
				DE_NULL,
				&multisampleStateCreateInfo)
			.setupFragmentOutputState(renderPass.get(), 0u, DE_NULL, &multisampleStateCreateInfo)
			.setMonolithicPipelineLayout(pipelineLayout)
			.buildPipeline();
	}

	beginCommandBuffer(vk, *cmdBuffer);

	if (m_params.m_useCompShader)
	{
		const std::vector<VkDescriptorSet> descSets = { descriptorSet.get(), descriptorSetResult.get() };

		vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline.get());
		vk.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayout, 0u, static_cast<deUint32>(descSets.size()), descSets.data(), 0u, DE_NULL);
		vk.cmdDispatch(*cmdBuffer, 1u, 1u, 1u);
	}
	else
	{
		renderPass.begin(vk, *cmdBuffer, makeRect2D(0, 0, m_params.m_framebufferSize.x(), m_params.m_framebufferSize.y()), static_cast<deUint32>(clearColorValues.size()), clearColorValues.data());
		graphicsPipelineWrapper.bind(*cmdBuffer);
		vk.cmdBindVertexBuffers(*cmdBuffer, 0u, 1u, &vertexBuffer.get(), &vertexBufferOffset);
		vk.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 0u, 1u, &descriptorSet.get(), 0u, DE_NULL);
		vk.cmdDraw(*cmdBuffer, numVertices, 1u, 0u, 0u);
		renderPass.end(vk, *cmdBuffer);
		copyImageToBuffer(vk, *cmdBuffer, *colorImage, *resultImageBuffer, m_params.m_framebufferSize, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
	}

	endCommandBuffer(vk, *cmdBuffer);

	submitCommandsAndWait(vk, vkDevice, queue, *cmdBuffer);

	// Check results
	if (!m_params.m_useCompShader)
	{
		invalidateAlloc(vk, vkDevice, *resultImageBufferAlloc);

		const tcu::ConstPixelBufferAccess	imagePixelAccess	(mapVkFormat(colorFormat), m_params.m_framebufferSize.x(), m_params.m_framebufferSize.y(), 1, resultImageBufferAlloc->getHostPtr());
		const tcu::TextureLevel				referenceTexture	= generateColorImage(colorFormat, m_params.m_framebufferSize, testColors[1]);

		if (!tcu::floatThresholdCompare(log, "Compare color output", "Image result comparison", referenceTexture.getAccess(), imagePixelAccess, tcu::Vec4(0.0f), tcu::COMPARE_LOG_RESULT))
			return tcu::TestStatus::fail("Rendered color image is not correct");
	}
	else
	{
		invalidateAlloc(vk, vkDevice, *ssboBufferAllocResult);
		const tcu::Vec4	resultValue	= *static_cast<tcu::Vec4*>(ssboBufferAllocResult->getHostPtr());

		if (!(resultValue == tcu::Vec4(0.0, 1.0, 0.0, 1.0)))
			return tcu::TestStatus::fail("Result buffer value is not correct");
	}

	return tcu::TestStatus::pass("Success");
}

class DescriptorLimitTest : public vkt::TestCase
{
public:
							DescriptorLimitTest		(tcu::TestContext&				testContext,
													 const std::string&				name,
													 const std::string&				description,
													 const TestParams&				params)
							: TestCase(testContext, name, description)
							, m_params						(params)
							{}

	virtual					~DescriptorLimitTest	(void)
							{}

	virtual void			initPrograms			(SourceCollections& programCollection) const;
	virtual void			checkSupport			(Context& context) const;
	virtual TestInstance*	createInstance			(Context& context) const;

private:
	TestParams	m_params;
};

void DescriptorLimitTest::initPrograms (SourceCollections& sourceCollections) const
{
	std::ostringstream	testTypeStr;
	std::ostringstream	fragResultStr;
	std::ostringstream	compResultStr;
	const deUint32		descCount		= m_params.getDescCount();

	if (m_params.m_testType == TestType::Samplers)
	{
		testTypeStr		<< "layout(set = 0, binding = " << descCount - 1u << ") uniform sampler2D texSamplerInput;\n";

		fragResultStr	<<	"    const vec2 coords = vec2(0, 0);\n"
						<<	"    fragColor = texture(texSamplerInput, coords);\n";

		compResultStr	<<	"    const vec2 coords = vec2(0, 0);\n"
						<<	"    outputData.color = texture(texSamplerInput, coords);\n";
	}

	if (m_params.m_testType == TestType::UniformBuffers)
	{
		testTypeStr		<< "layout(set = 0, binding = " << descCount - 1u << ") uniform uboInput\n"
						<< "{\n"
						<< "    vec4 color;\n"
						<< "} inputData;\n"
						<< "\n";

		fragResultStr	<< "    fragColor = inputData.color;\n";
		compResultStr	<< "    outputData.color = inputData.color;\n";
	}

	if (m_params.m_testType == TestType::StorageBuffers)
	{
		testTypeStr		<< "layout(set = 0, binding = " << (descCount - 1u) << ") readonly buffer ssboInput\n"
						<< "{\n"
						<< "    vec4 color;\n"
						<< "} inputData;\n"
						<< "\n";

		fragResultStr	<< "    fragColor = inputData.color;\n";
		compResultStr	<< "    outputData.color = inputData.color;\n";
	}

	if (m_params.m_testType == TestType::SampledImages)
	{
		testTypeStr		<< "#extension GL_EXT_samplerless_texture_functions : enable\n"
						<< "layout(set = 0, binding = " << descCount - 1u << ") uniform texture2D imageInput;\n";

		fragResultStr	<< "    fragColor = texelFetch(imageInput, ivec2(gl_FragCoord.xy), 0);\n";
		compResultStr	<< "    const ivec2 coords = ivec2(0, 0);\n"
						<< "    outputData.color = texelFetch(imageInput, coords, 0);\n";
	}

	if (m_params.m_testType == TestType::StorageImages)
	{
		testTypeStr		<< "#extension GL_EXT_samplerless_texture_functions : enable\n"
						<< "layout(set = 0, binding = " << descCount - 1u << ", rgba8) uniform image2D imageInput;\n";

		fragResultStr	<< "    fragColor = imageLoad(imageInput, ivec2(gl_FragCoord.xy));\n";
		compResultStr	<< "    const ivec2 coords = ivec2(0, 0);\n"
						<< "    outputData.color = imageLoad(imageInput, coords);\n";
	}

	if (m_params.m_testType == TestType::InputAttachments)
	{
		testTypeStr << "layout (input_attachment_index = 1, set = 0, binding = " << descCount - 1u << ") uniform subpassInput imageInput;\n";

		fragResultStr	<< "    fragColor = subpassLoad(imageInput);\n";
		compResultStr	<< "    outputData.color = vec4(0.0, 0.0, 0.0, 1.0);\n";
	}

	std::ostringstream vertexSrc;
	vertexSrc	<< glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
				<< "\n"
				<< "layout(location = 0) in vec4 position;\n"
				<< "\n"
				<< "void main (void)\n"
				<< "{\n"
				<< "    gl_Position = position;\n"
				<< "}\n";

	sourceCollections.glslSources.add("vert") << glu::VertexSource(vertexSrc.str());

	std::ostringstream testSrc;

	if (!m_params.m_useCompShader)
	{
		testSrc << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
			<< "\n"
			<< "layout(location = 0) out vec4 fragColor;\n"
			<< "\n"
			<< testTypeStr.str()
			<< "void main (void)\n"
			<< "{\n"
			<< fragResultStr.str()
			<< "}\n";

		sourceCollections.glslSources.add("test") << glu::FragmentSource(testSrc.str());
	}
	else
	{
		testSrc	<< glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
				<< "\n"
				// Input attachments are not supported by compute shaders.
				<< (m_params.m_testType != TestType::InputAttachments ? testTypeStr.str() : "")
				<< "layout(set = 1, binding = 0) buffer ssboOutput\n"
				<< "{\n"
				<< "    vec4 color;\n"
				<< "} outputData;\n"
				<< "\n"
				<< "void main (void)\n"
				<< "{\n"
				<< compResultStr.str()
				<< "}\n";

		sourceCollections.glslSources.add("test") << glu::ComputeSource(testSrc.str());
	}
}

void DescriptorLimitTest::checkSupport (Context& context) const
{
	const InstanceInterface&		vki				= context.getInstanceInterface();
	const VkPhysicalDevice			physDevice		= context.getPhysicalDevice();
	const VkPhysicalDeviceLimits	limits			= getPhysicalDeviceProperties(vki, physDevice).limits;

	// We have to make sure, that we don't bind anything outside of valid descriptor binding locations determined by maxPerStageResources.
	if (m_params.m_descCount > limits.maxPerStageResources - 1u)
		TCU_THROW(NotSupportedError, "maxPerStageResources (" + std::to_string(limits.maxPerStageResources) + ")");

	if (m_params.m_testType == TestType::Samplers)
	{
		if(m_params.m_descCount > limits.maxPerStageDescriptorSamplers)
			TCU_THROW(NotSupportedError, "maxPerStageDescriptorSamplers (" + std::to_string(limits.maxPerStageDescriptorSamplers) + ")");
	}

	if (m_params.m_testType == TestType::UniformBuffers)
	{
		if (m_params.m_descCount > limits.maxPerStageDescriptorUniformBuffers)
			TCU_THROW(NotSupportedError, "maxPerStageDescriptorUniformBuffers (" + std::to_string(limits.maxPerStageDescriptorUniformBuffers) + ")");
	}

	if (m_params.m_testType == TestType::StorageBuffers)
	{
		if (m_params.m_descCount > limits.maxPerStageDescriptorStorageBuffers)
			TCU_THROW(NotSupportedError, "maxPerStageDescriptorStorageBuffers (" + std::to_string(limits.maxPerStageDescriptorStorageBuffers) + ")");
	}

	if (m_params.m_testType == TestType::SampledImages)
	{
		if (m_params.m_descCount > limits.maxPerStageDescriptorSampledImages)
			TCU_THROW(NotSupportedError, "maxPerStageDescriptorSampledImages (" + std::to_string(limits.maxPerStageDescriptorSampledImages) + ")");
	}

	if (m_params.m_testType == TestType::StorageImages)
	{
		if (m_params.m_descCount > limits.maxPerStageDescriptorStorageImages)
			TCU_THROW(NotSupportedError, "maxPerStageDescriptorStorageImages (" + std::to_string(limits.maxPerStageDescriptorStorageImages) + ")");
	}

	if (m_params.m_testType == TestType::InputAttachments)
	{
		if (m_params.m_descCount > limits.maxPerStageDescriptorInputAttachments)
			TCU_THROW(NotSupportedError, "maxPerStageDescriptorInputAttachments (" + std::to_string(limits.maxPerStageDescriptorInputAttachments) + ")");
	}

	checkPipelineConstructionRequirements(vki, physDevice, m_params.m_pipelineConstructionType);
}

TestInstance* DescriptorLimitTest::createInstance (Context& context) const
{
	return new DescriptorLimitTestInstance(context, m_params);
}

}

tcu::TestCaseGroup* createDescriptorLimitsTests (tcu::TestContext& testCtx, PipelineConstructionType pipelineConstructionType)
{
	de::MovePtr<tcu::TestCaseGroup>	descriptorLimitTestGroup	(new tcu::TestCaseGroup(testCtx, "descriptor_limits", "Descriptor limits tests"));
	const tcu::IVec2				frameBufferSize				= tcu::IVec2(32, 32);

	const std::vector<deUint32>		numDescriptors				=
	{
		3u,		4u,		5u,		6u,		7u,		8u,		9u,		10u,	11u,
		12u,	13u,	14u,	15u,	16u,	17u,	18u,	19u,	20u,
		31u,	32u,	63u,	64u,	100u,	127u,	128u,	199u,	200u,
		256u,	512u,	1024u,	2048u,	4096u,	8192u,	16384u,	32768u,	65535u
	};

	if (pipelineConstructionType == PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC)
	{
		de::MovePtr<tcu::TestCaseGroup>	computeShaderGroup(new tcu::TestCaseGroup(testCtx, "compute_shader", "Compute shader test group"));

		for (const auto& descId : numDescriptors)
		{
			const deUint32	testValue		= descId;

			{
				TestParams params(pipelineConstructionType, TestType::Samplers, true, frameBufferSize, testValue);
				computeShaderGroup->addChild(new DescriptorLimitTest(testCtx, "samplers_" + std::to_string(testValue), "", params));
			}

			{
				TestParams params(pipelineConstructionType, TestType::UniformBuffers, true, frameBufferSize, testValue);
				computeShaderGroup->addChild(new DescriptorLimitTest(testCtx, "uniform_buffers_" + std::to_string(testValue), "", params));
			}

			{
				TestParams params(pipelineConstructionType, TestType::StorageBuffers, true, frameBufferSize, testValue);
				computeShaderGroup->addChild(new DescriptorLimitTest(testCtx, "storage_buffers_" + std::to_string(testValue), "", params));
			}

			{
				TestParams params(pipelineConstructionType, TestType::SampledImages, true, frameBufferSize, testValue);
				computeShaderGroup->addChild(new DescriptorLimitTest(testCtx, "sampled_images_" + std::to_string(testValue), "", params));
			}

			{
				TestParams params(pipelineConstructionType, TestType::StorageImages, true, frameBufferSize, testValue);
				computeShaderGroup->addChild(new DescriptorLimitTest(testCtx, "storage_images_" + std::to_string(testValue), "", params));
			}
		}

		descriptorLimitTestGroup->addChild(computeShaderGroup.release());
	}

	de::MovePtr<tcu::TestCaseGroup>	fragmentShaderGroup(new tcu::TestCaseGroup(testCtx, "fragment_shader", "Fragment shader test group"));

	for (const auto& descId : numDescriptors)
	{
		const deUint32	testValue	= descId;

		{
			TestParams params(pipelineConstructionType, TestType::Samplers, false, frameBufferSize, testValue);
			fragmentShaderGroup->addChild(new DescriptorLimitTest(testCtx, "samplers_" + std::to_string(testValue), "", params));
		}

		{
			TestParams params(pipelineConstructionType, TestType::UniformBuffers, false, frameBufferSize, testValue);
			fragmentShaderGroup->addChild(new DescriptorLimitTest(testCtx, "uniform_buffers_" + std::to_string(testValue), "", params));
		}

		{
			TestParams params(pipelineConstructionType, TestType::StorageBuffers, false, frameBufferSize, testValue);
			fragmentShaderGroup->addChild(new DescriptorLimitTest(testCtx, "storage_buffers_" + std::to_string(testValue), "", params));
		}

		{
			TestParams params(pipelineConstructionType, TestType::SampledImages, false, frameBufferSize, testValue);
			fragmentShaderGroup->addChild(new DescriptorLimitTest(testCtx, "sampled_images_" + std::to_string(testValue), "", params));
		}

		{
			TestParams params(pipelineConstructionType, TestType::StorageImages, false, frameBufferSize, testValue);
			fragmentShaderGroup->addChild(new DescriptorLimitTest(testCtx, "storage_images_" + std::to_string(testValue), "", params));
		}

		if (!vk::isConstructionTypeShaderObject(pipelineConstructionType))
		{
			TestParams params(pipelineConstructionType, TestType::InputAttachments, false, frameBufferSize, testValue);
			fragmentShaderGroup->addChild(new DescriptorLimitTest(testCtx, "input_attachments_" + std::to_string(testValue), "", params));
		}
	}

	descriptorLimitTestGroup->addChild(fragmentShaderGroup.release());

	return descriptorLimitTestGroup.release();
}

} // pipeline
} // vkt
