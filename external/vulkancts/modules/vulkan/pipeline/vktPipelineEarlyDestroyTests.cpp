/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2018 The Khronos Group Inc.
 * Copyright (c) 2018 Google Inc.
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
 * \brief Early pipeline destroying tests
 *//*--------------------------------------------------------------------*/

#include "vktPipelineEarlyDestroyTests.hpp"
#include "vktTestCase.hpp"
#include "vktTestCaseUtil.hpp"
#include "vktTestGroupUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkPrograms.hpp"
#include "vkRefUtil.hpp"
#include "vkObjUtil.hpp"
#include "deUniquePtr.hpp"
#include "tcuTexture.hpp"
#include "vkImageUtil.hpp"
#include "vkImageWithMemory.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkCmdUtil.hpp"

namespace vkt
{
namespace pipeline
{

using namespace vk;

namespace
{

void initPrograms (SourceCollections& programCollection, bool usePipelineCache)
{
	DE_UNREF(usePipelineCache);

	programCollection.glslSources.add("color_vert") << glu::VertexSource(
		"#version 450\n"
		"vec2 vertices[3];\n"
		"\n"
		"void main()\n"
		"{\n"
		"   vertices[0] = vec2(-1.0, -1.0);\n"
		"   vertices[1] = vec2( 1.0, -1.0);\n"
		"   vertices[2] = vec2( 0.0,  1.0);\n"
		"   gl_Position = vec4(vertices[gl_VertexIndex % 3], 0.0, 1.0);\n"
		"}\n");

	programCollection.glslSources.add("color_frag") << glu::FragmentSource(
		"#version 450\n"
		"\n"
		"layout(location = 0) out vec4 uFragColor;\n"
		"\n"
		"void main()\n"
		"{\n"
		"   uFragColor = vec4(0,1,0,1);\n"
		"}\n");
}

tcu::TestStatus testEarlyDestroy (Context& context, bool usePipelineCache, bool destroyLayout)
{
	const DeviceInterface&								vk							    = context.getDeviceInterface();
	const VkDevice										vkDevice						= context.getDevice();
	const Unique<VkShaderModule>						vertexShaderModule				(createShaderModule(vk, vkDevice, context.getBinaryCollection().get("color_vert"), 0));
	const Unique<VkShaderModule>						fragmentShaderModule			(createShaderModule(vk, vkDevice, context.getBinaryCollection().get("color_frag"), 0));

	const Unique<VkCommandPool>							cmdPool							(createCommandPool(vk, vkDevice, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, context.getUniversalQueueFamilyIndex()));
	const Unique<VkCommandBuffer>						cmdBuffer						(allocateCommandBuffer(vk, vkDevice, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

	const VkPipelineLayoutCreateInfo					pipelineLayoutCreateInfo		=
	{
		VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,									// VkStructureType					sType;
		DE_NULL,																		// const void*						pNext;
		0u,																				// VkPipelineLayoutCreateFlags		flags;
		0u,																				// deUint32							setLayoutCount;
		DE_NULL,																		// const VkDescriptorSetLayout*		pSetLayouts;
		0u,																				// deUint32							pushConstantRangeCount;
		DE_NULL																			// const VkPushConstantRange*		pPushConstantRanges;
	};

	// Multiple passes for destroy layout in order to increase the chance of crashing if some resource/state gets carried over from previous iterations.
	int numTests = destroyLayout ? 3 : 1;
	for(int i = 0; i < numTests; ++i)
	{
		Move<VkPipelineLayout>							pipelineLayout					(createPipelineLayout(vk, vkDevice, &pipelineLayoutCreateInfo, DE_NULL));
		const Unique<VkRenderPass>						renderPass						(makeRenderPass(vk, vkDevice, VK_FORMAT_R8G8B8A8_UNORM));
		const VkPipelineShaderStageCreateInfo			stages[]						=
		{
			{
				VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,					// VkStructureType					sType;
				DE_NULL,																// const void*						pNext;
				0u,																		// VkPipelineShaderStageCreateFlags	flags;
				VK_SHADER_STAGE_VERTEX_BIT,												// VkShaderStageFlagBits			stage;
				*vertexShaderModule,													// VkShaderModule					module;
				"main",																	// const char*						pName;
				DE_NULL																	// const VkSpecializationInfo*		pSpecializationInfo;
			},
			{
				VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,					// VkStructureType					sType;
				DE_NULL,																// const void*						pNext;
				0u,																		// VkPipelineShaderStageCreateFlags	flags;
				VK_SHADER_STAGE_FRAGMENT_BIT,											// VkShaderStageFlagBits			stage;
				*fragmentShaderModule,													// VkShaderModule					module;
				"main",																	// const char*						pName;
				DE_NULL																	// const VkSpecializationInfo*		pSpecializationInfo;
			}
		};
		const VkPipelineVertexInputStateCreateInfo		vertexInputStateCreateInfo		=
		{
			VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,					// VkStructureType							sType;
			DE_NULL,																	// const void*								pNext;
			0u,																			// VkPipelineVertexInputStateCreateFlags	flags;
			0u,																			// deUint32									vertexBindingDescriptionCount;
			DE_NULL,																	// const VkVertexInputBindingDescription*	pVertexBindingDescriptions;
			0u,																			// deUint32									vertexAttributeDescriptionCount;
			DE_NULL																		// const VkVertexInputAttributeDescription*	pVertexAttributeDescriptions;
		};
		const VkPipelineInputAssemblyStateCreateInfo	inputAssemblyStateCreateInfo	=
		{
			VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,				// VkStructureType							sType;
			DE_NULL,																	// const void*								pNext;
			0u,																			// VkPipelineInputAssemblyStateCreateFlags	flags;
			VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,										// VkPrimitiveTopology						topology;
			VK_FALSE																	// VkBool32									primitiveRestartEnable;
		};
		const VkPipelineRasterizationStateCreateInfo	rasterizationStateCreateInfo	=
		{
			VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,					// VkStructureType							sType;
			DE_NULL,																	// const void*								pNext;
			0u,																			// VkPipelineRasterizationStateCreateFlags	flags;
			VK_FALSE,																	// VkBool32									depthClampEnable;
			VK_TRUE,																	// VkBool32									rasterizerDiscardEnable;
			VK_POLYGON_MODE_FILL,														// VkPolygonMode							polygonMode;
			VK_CULL_MODE_BACK_BIT,														// VkCullModeFlags							cullMode;
			VK_FRONT_FACE_CLOCKWISE,													// VkFrontFace								frontFace;
			VK_FALSE,																	// VkBool32									depthBiasEnable;
			0.0f,																		// float									depthBiasConstantFactor;
			0.0f,																		// float									depthBiasClamp;
			0.0f,																		// float									depthBiasSlopeFactor;
			1.0f																		// float									lineWidth;
		};
		const VkPipelineColorBlendAttachmentState		colorBlendAttachmentState		=
		{
			VK_FALSE,																	// VkBool32					blendEnable;
			VK_BLEND_FACTOR_ZERO,														// VkBlendFactor			srcColorBlendFactor;
			VK_BLEND_FACTOR_ZERO,														// VkBlendFactor			dstColorBlendFactor;
			VK_BLEND_OP_ADD,															// VkBlendOp				colorBlendOp;
			VK_BLEND_FACTOR_ZERO,														// VkBlendFactor			srcAlphaBlendFactor;
			VK_BLEND_FACTOR_ZERO,														// VkBlendFactor			dstAlphaBlendFactor;
			VK_BLEND_OP_ADD,															// VkBlendOp				alphaBlendOp;
			0xf																			// VkColorComponentFlags	colorWriteMask;
		};
		const VkPipelineColorBlendStateCreateInfo		colorBlendStateCreateInfo		=
		{
			VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,					// VkStructureType								sType;
			DE_NULL,																	// const void*									pNext;
			0u,																			// VkPipelineColorBlendStateCreateFlags			flags;
			VK_FALSE,																	// VkBool32										logicOpEnable;
			VK_LOGIC_OP_CLEAR,															// VkLogicOp									logicOp;
			1u,																			// deUint32										attachmentCount;
			&colorBlendAttachmentState,													// const VkPipelineColorBlendAttachmentState*	pAttachments;
			{ 0.0f, 0.0f, 0.0f, 0.0f }													// float										blendConstants[4];
		};
		const VkPipelineCacheCreateInfo					pipelineCacheCreateInfo			=
		{
			VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO,								// VkStructureType				sType;
			DE_NULL,																	// const void*					pNext;
			0u,																			// VkPipelineCacheCreateFlags	flags;
			0u,																			// size_t						initialDataSize;
			DE_NULL																		// const void*					pInitialData;
		};
		const Unique<VkPipelineCache>					pipelineCache					(createPipelineCache(vk, vkDevice, &pipelineCacheCreateInfo));
		const VkGraphicsPipelineCreateInfo				graphicsPipelineCreateInfo		=
		{
			VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,							// VkStructureType									sType;
			DE_NULL,																	// const void*										pNext;
			VK_PIPELINE_CREATE_DISABLE_OPTIMIZATION_BIT,								// VkPipelineCreateFlags							flags;
			2u,																			// deUint32											stageCount;
			stages,																		// const VkPipelineShaderStageCreateInfo*			pStages;
			&vertexInputStateCreateInfo,												// const VkPipelineVertexInputStateCreateInfo*		pVertexInputState;
			&inputAssemblyStateCreateInfo,												// const VkPipelineInputAssemblyStateCreateInfo*	pInputAssemblyState;
			DE_NULL,																	// const VkPipelineTessellationStateCreateInfo*		pTessellationState;
			DE_NULL,																	// const VkPipelineViewportStateCreateInfo*			pViewportState;
			&rasterizationStateCreateInfo,												// const VkPipelineRasterizationStateCreateInfo*	pRasterizationState;
			DE_NULL,																	// const VkPipelineMultisampleStateCreateInfo*		pMultisampleState;
			DE_NULL,																	// const VkPipelineDepthStencilStateCreateInfo*		pDepthStencilState;
			&colorBlendStateCreateInfo,													// const VkPipelineColorBlendStateCreateInfo*		pColorBlendState;
			DE_NULL,																	// const VkPipelineDynamicStateCreateInfo*			pDynamicState;
			*pipelineLayout,															// VkPipelineLayout									layout;
			*renderPass,																// VkRenderPass										renderPass;
			0u,																			// deUint32											subpass;
			DE_NULL,																	// VkPipeline										basePipelineHandle;
			0																			// int												basePipelineIndex;
		};
		createGraphicsPipeline(vk, vkDevice, usePipelineCache ? *pipelineCache : DE_NULL, &graphicsPipelineCreateInfo);

		const deUint32 framebufferWidth													= 32;
		const deUint32 framebufferHeight												= 32;
		if (destroyLayout)
		{
			// This will destroy the pipelineLayout when going out of enclosing scope
			Move<VkPipelineLayout> layout(pipelineLayout);
		}
		const VkCommandBufferBeginInfo					cmdBufferBeginInfo				=
		{
			VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,								// VkStructureType							sType;
			DE_NULL,																	// const void*								pNext;
			0u,																			// VkCommandBufferUsageFlags				flags;
			(const VkCommandBufferInheritanceInfo*)DE_NULL								// const VkCommandBufferInheritanceInfo*	pInheritanceInfo;
		};
		if (!destroyLayout) {
			VK_CHECK(vk.beginCommandBuffer(*cmdBuffer, &cmdBufferBeginInfo));
			VK_CHECK(vk.endCommandBuffer(*cmdBuffer));
		} else {
			auto&										allocator						= context.getDefaultAllocator();
			const auto									queue							= context.getUniversalQueue();
			const VkFormat								attachmentFormat				= VK_FORMAT_R8G8B8A8_UNORM;
			const tcu::TextureFormat					textureFormat					= mapVkFormat(attachmentFormat);
			const VkDeviceSize							imageSize						= framebufferWidth * framebufferHeight * textureFormat.getPixelSize();
			const VkImageCreateInfo						imageCreateInfo					=
			{
				VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,									// VkStructureType			sType;
				DE_NULL,																// const void*				pNext;
				(VkImageCreateFlags)0,													// VkImageCreateFlags		flags;
				VK_IMAGE_TYPE_2D,														// VkImageType				imageType;
				attachmentFormat,														// VkFormat					format;
				{ framebufferWidth, framebufferHeight, 1u },							// VkExtent3D				extent;
				1u,																		// deUint32					mipLevels;
				1u,																		// deUint32					arrayLayers;
				VK_SAMPLE_COUNT_1_BIT,													// VkSampleCountFlagBits	samples;
				VK_IMAGE_TILING_OPTIMAL,												// VkImageTiling			tiling;
				VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
				VK_IMAGE_USAGE_TRANSFER_DST_BIT |
				VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,									// VkImageUsageFlags		usage;
				VK_SHARING_MODE_EXCLUSIVE,												// VkSharingMode			sharingMode;
				0u,																		// deUint32					queueFamilyIndexCount;
				DE_NULL,																// const deUint32*			pQueueFamilyIndices;
				VK_IMAGE_LAYOUT_UNDEFINED												// VkImageLayout			initialLayout;
			};
			const ImageWithMemory						attachmentImage					(vk, vkDevice, context.getDefaultAllocator(), imageCreateInfo, MemoryRequirement::Any);
			const VkImageSubresourceRange				colorSubresourceRange			= { VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u };
			const Unique<VkImageView>					attachmentImageView				(vk::makeImageView(vk, vkDevice, *attachmentImage, VK_IMAGE_VIEW_TYPE_2D, attachmentFormat, colorSubresourceRange));
			const VkBufferCreateInfo					imageBufferCreateInfo			= vk::makeBufferCreateInfo(imageSize, vk::VK_BUFFER_USAGE_TRANSFER_DST_BIT);
			const BufferWithMemory						imageBuffer						(vk, vkDevice, allocator, imageBufferCreateInfo, vk::MemoryRequirement::HostVisible);
			const Unique<VkFramebuffer>					framebuffer						(vk::makeFramebuffer(vk, vkDevice, *renderPass, *attachmentImageView, framebufferWidth, framebufferHeight, 1u));

			VK_CHECK(vk.beginCommandBuffer(*cmdBuffer, &cmdBufferBeginInfo));
			const tcu::Vec4								clearColor						= { 0.2f, 0.6f, 0.8f, 1.0f };
			VkClearValue								clearValue						=
			{
				{ { clearColor.x(), clearColor.y(),
					clearColor.z(), clearColor.w() } }									// float						float32[4];
			};
			VkClearAttachment								attachment						=
			{
				VK_IMAGE_ASPECT_COLOR_BIT,												// VkImageAspectFlags			aspectMask;
				0u,																		// deUint32						colorAttachment;
				clearValue																// VkClearValue					clearValue;
			};
			const VkRect2D								renderArea						= { { 0, 0 }, { framebufferWidth, framebufferHeight } };
			const VkClearRect								rect							=
			{
				renderArea,																// VkRect2D						rect
				0u,																		// uint32_t						baseArrayLayer
				1u																		// uint32_t						layerCount
			};
			const VkRenderPassBeginInfo				    renderPassBeginInfo				=
			{
				VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,								// VkStructureType				sType;
				DE_NULL,																// const void*					pNext;
				*renderPass,															// VkRenderPass					renderPass;
				*framebuffer,															// VkFramebuffer				framebuffer;
				renderArea,																// VkRect2D						renderArea;
				1u,																		// deUint32						clearValueCount;
				&clearValue																// const VkClearValue*			pClearValues;
			};
			vk.cmdBeginRenderPass(*cmdBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
			vk.cmdClearAttachments(*cmdBuffer, 1, &attachment, 1, &rect);
			vk.cmdEndRenderPass(*cmdBuffer);
			vk::copyImageToBuffer(vk, *cmdBuffer, *attachmentImage, *imageBuffer, tcu::IVec2(framebufferWidth, framebufferHeight));
			VK_CHECK(vk.endCommandBuffer(*cmdBuffer));

			vk::submitCommandsAndWait(vk, vkDevice, queue, *cmdBuffer);
			vk.resetCommandBuffer(*cmdBuffer, VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT);
			const auto&									imageBufferAlloc				= imageBuffer.getAllocation();
			vk::invalidateAlloc(vk, vkDevice, imageBufferAlloc);

			const auto									imageBufferPtr					= reinterpret_cast<const char*>(imageBufferAlloc.getHostPtr()) + imageBufferAlloc.getOffset();
			const tcu::ConstPixelBufferAccess			imagePixels						(textureFormat, framebufferWidth, framebufferHeight, 1u, imageBufferPtr);

			for (int z = 0; z < imagePixels.getDepth(); ++z)
			for (int y = 0; y < imagePixels.getHeight(); ++y)
			for (int x = 0; x < imagePixels.getWidth(); ++x)
			{
				const auto pixel = imagePixels.getPixel(x, y, z);
				if (pixel != clearColor) {
									std::ostringstream msg; msg << "Pixel value mismatch after framebuffer clear." << " diff: " << pixel << " vs " << clearColor;

					return tcu::TestStatus::fail(msg.str()/*"Pixel value mismatch after framebuffer clear."*/);
				}
			}
		}
	}
	// Passes as long as no crash occurred.
	return tcu::TestStatus::pass("Pass");
}

tcu::TestStatus testEarlyDestroyKeepLayout (Context& context, bool useCache)
{
	return testEarlyDestroy (context, useCache, false);
}

tcu::TestStatus testEarlyDestroyDestroyLayout (Context& context, bool useCache)
{
	return testEarlyDestroy (context, useCache, true);
}

void addEarlyDestroyTestCasesWithFunctions (tcu::TestCaseGroup* group)
{
	addFunctionCaseWithPrograms(group, "cache", "", initPrograms, testEarlyDestroyKeepLayout, true);
	addFunctionCaseWithPrograms(group, "no_cache", "", initPrograms, testEarlyDestroyKeepLayout, false);
	addFunctionCaseWithPrograms(group, "cache_destroy_layout", "", initPrograms, testEarlyDestroyDestroyLayout, true);
	addFunctionCaseWithPrograms(group, "no_cache_destroy_layout", "", initPrograms, testEarlyDestroyDestroyLayout, false);
}

} // anonymous

tcu::TestCaseGroup* createEarlyDestroyTests (tcu::TestContext& testCtx)
{
	return createTestGroup(testCtx, "early_destroy", "Tests where pipeline is destroyed early", addEarlyDestroyTestCasesWithFunctions);
}

} // pipeline
} // vkt
