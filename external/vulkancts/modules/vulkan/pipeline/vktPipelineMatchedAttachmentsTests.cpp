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
 * \brief Matched attachments tests
 *//*--------------------------------------------------------------------*/

#include "vktPipelineMatchedAttachmentsTests.hpp"
#include "vktTestCase.hpp"
#include "vktTestCaseUtil.hpp"
#include "vktTestGroupUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkPrograms.hpp"
#include "vkRefUtil.hpp"
#include "deUniquePtr.hpp"

namespace vkt
{
namespace pipeline
{

using namespace vk;

namespace
{

struct MatchedAttachmentsTestParams
{
	bool	usePipelineCache;
};

void initPrograms (SourceCollections& programCollection, const MatchedAttachmentsTestParams params)
{
	DE_UNREF(params);

	programCollection.glslSources.add("color_vert") << glu::VertexSource(
		"#version 450\n"
		"\n"
		"void main(){\n"
		"    gl_Position = vec4(1);\n"
		"}\n");

	programCollection.glslSources.add("color_frag") << glu::FragmentSource(
		"#version 450\n"
		"\n"
		"layout(input_attachment_index=0, set=0, binding=0) uniform subpassInput x;\n"
		"layout(location=0) out vec4 color;\n"
		"void main() {\n"
		"   color = subpassLoad(x);\n"
		"}\n");
}

tcu::TestStatus testMatchedAttachments (Context& context, const MatchedAttachmentsTestParams params)
{
	const DeviceInterface&							vk								= context.getDeviceInterface();
	const VkDevice									vkDevice						= context.getDevice();
	const Unique<VkShaderModule>					vertexShaderModule				(createShaderModule(vk, vkDevice, context.getBinaryCollection().get("color_vert"), 0));
	const Unique<VkShaderModule>					fragmentShaderModule			(createShaderModule(vk, vkDevice, context.getBinaryCollection().get("color_frag"), 0));

	const VkDescriptorSetLayoutBinding				descriptorSetLayoutBinding		=
	{
		0u,										// deUint32              binding;
		VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,	// VkDescriptorType      descriptorType;
		1u,										// deUint32              descriptorCount;
		VK_SHADER_STAGE_FRAGMENT_BIT,			// VkShaderStageFlags    stageFlags;
		DE_NULL									// const VkSampler*      pImmutableSamplers;
	};

	const VkDescriptorSetLayoutCreateInfo			descriptorSetLayoutCreateInfo	 =
	{
		VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,	// VkStructureType                        sType;
		DE_NULL,												// const void*                            pNext;
		0u,														// VkDescriptorSetLayoutCreateFlags       flags;
		1u,														// deUint32                               bindingCount;
		&descriptorSetLayoutBinding								// const VkDescriptorSetLayoutBinding*    pBindings;
	} ;

	const Unique<VkDescriptorSetLayout>				descriptorSetLayout				(createDescriptorSetLayout(vk, vkDevice, &descriptorSetLayoutCreateInfo, DE_NULL));

	const VkPipelineLayoutCreateInfo				pipelineLayoutCreateInfo		=
	{
		VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,	// VkStructureType                 sType;
		DE_NULL,										// const void*                     pNext;
		0u,												// VkPipelineLayoutCreateFlags     flags;
		1u,												// deUint32                        setLayoutCount;
		&(*descriptorSetLayout),						// const VkDescriptorSetLayout*    pSetLayouts;
		0u,												// deUint32                        pushConstantRangeCount;
		DE_NULL											// const VkPushConstantRange*      pPushConstantRanges;
	};

	const Unique<VkPipelineLayout>					pipelineLayout					(createPipelineLayout(vk, vkDevice, &pipelineLayoutCreateInfo, DE_NULL));

	const VkAttachmentDescription					descs[2]						=
	{
		{
			0u,											// VkAttachmentDescriptionFlags    flags;
			VK_FORMAT_R8G8B8A8_UNORM,					// VkFormat                        format;
			VK_SAMPLE_COUNT_1_BIT,						// VkSampleCountFlagBits           samples;
			VK_ATTACHMENT_LOAD_OP_LOAD,					// VkAttachmentLoadOp              loadOp;
			VK_ATTACHMENT_STORE_OP_STORE,				// VkAttachmentStoreOp             storeOp;
			VK_ATTACHMENT_LOAD_OP_LOAD,					// VkAttachmentLoadOp              stencilLoadOp;
			VK_ATTACHMENT_STORE_OP_STORE,				// VkAttachmentStoreOp             stencilStoreOp;
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,	// VkImageLayout                   initialLayout;
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL	// VkImageLayout                   finalLayout;
		},
		{
			0u,								// VkAttachmentDescriptionFlags    flags;
			VK_FORMAT_R8G8B8A8_UNORM,		// VkFormat                        format;
			VK_SAMPLE_COUNT_1_BIT,			// VkSampleCountFlagBits           samples;
			VK_ATTACHMENT_LOAD_OP_LOAD,		// VkAttachmentLoadOp              loadOp;
			VK_ATTACHMENT_STORE_OP_STORE,	// VkAttachmentStoreOp             storeOp;
			VK_ATTACHMENT_LOAD_OP_LOAD,		// VkAttachmentLoadOp              stencilLoadOp;
			VK_ATTACHMENT_STORE_OP_STORE,	// VkAttachmentStoreOp             stencilStoreOp;
			VK_IMAGE_LAYOUT_GENERAL,		// VkImageLayout                   initialLayout;
			VK_IMAGE_LAYOUT_GENERAL			// VkImageLayout                   finalLayout;
		}
	};

	const VkAttachmentReference						color							=
	{
		0u,											// deUint32         attachment;
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL	// VkImageLayout    layout;
	};

	const VkAttachmentReference						input							=
	{
		1u,						// deUint32         attachment;
		VK_IMAGE_LAYOUT_GENERAL	// VkImageLayout    layout;
	};

	const VkSubpassDescription						subpassDescription				=
	{
		0u,									// VkSubpassDescriptionFlags       flags;
		VK_PIPELINE_BIND_POINT_GRAPHICS,	// VkPipelineBindPoint             pipelineBindPoint;
		1u,									// deUint32                        inputAttachmentCount;
		&input,								// const VkAttachmentReference*    pInputAttachments;
		1u,									// deUint32                        colorAttachmentCount;
		&color,								// const VkAttachmentReference*    pColorAttachments;
		DE_NULL,							// const VkAttachmentReference*    pResolveAttachments;
		DE_NULL,							// const VkAttachmentReference*    pDepthStencilAttachment;
		0u,									// deUint32                        preserveAttachmentCount;
		DE_NULL								// const deUint32*                 pPreserveAttachments;
	};

	const VkRenderPassCreateInfo					renderPassCreateInfo			=
	{
		VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,	// VkStructureType                   sType;
		DE_NULL,									// const void*                       pNext;
		0u,											// VkRenderPassCreateFlags           flags;
		2u,											// deUint32                          attachmentCount;
		descs,										// const VkAttachmentDescription*    pAttachments;
		1u,											// deUint32                          subpassCount;
		&subpassDescription,						// const VkSubpassDescription*       pSubpasses;
		0u,											// deUint32                          dependencyCount;
		DE_NULL										// const VkSubpassDependency*        pDependencies;
	};

	const Unique<VkRenderPass>						renderPass						(createRenderPass(vk, vkDevice, &renderPassCreateInfo, DE_NULL));

	const VkPipelineCacheCreateInfo					pipelineCacheCreateInfo			=
	{
		VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO,	// VkStructureType               sType;
		DE_NULL,										// const void*                   pNext;
		0u,												// VkPipelineCacheCreateFlags    flags;
		0u,												// size_t                        initialDataSize;
		DE_NULL											// const void*                   pInitialData;
	};

	const Unique<VkPipelineCache>					pipelineCache					(createPipelineCache(vk, vkDevice, &pipelineCacheCreateInfo));

	const VkPipelineShaderStageCreateInfo			stages[]						=
	{
		{
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,	// VkStructureType                     sType;
			DE_NULL,												// const void*                         pNext;
			0u,														// VkPipelineShaderStageCreateFlags    flags;
			VK_SHADER_STAGE_VERTEX_BIT,								// VkShaderStageFlagBits               stage;
			*vertexShaderModule,									// VkShaderModule                      module;
			"main",													// const char*                         pName;
			DE_NULL													// const VkSpecializationInfo*         pSpecializationInfo;
		},
		{
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,	// VkStructureType                     sType;
			DE_NULL,												// const void*                         pNext;
			0u,														// VkPipelineShaderStageCreateFlags    flags;
			VK_SHADER_STAGE_FRAGMENT_BIT,							// VkShaderStageFlagBits               stage;
			*fragmentShaderModule,									// VkShaderModule                      module;
			"main",													// const char*                         pName;
			DE_NULL													// const VkSpecializationInfo*         pSpecializationInfo;
		}
	};

	const VkPipelineVertexInputStateCreateInfo		vertexInputStateCreateInfo		=
	{
		VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,	// VkStructureType                             sType;
		DE_NULL,													// const void*                                 pNext;
		0u,															// VkPipelineVertexInputStateCreateFlags       flags;
		0u,															// deUint32                                    vertexBindingDescriptionCount;
		DE_NULL,													// const VkVertexInputBindingDescription*      pVertexBindingDescriptions;
		0u,															// deUint32                                    vertexAttributeDescriptionCount;
		DE_NULL														// const VkVertexInputAttributeDescription*    pVertexAttributeDescriptions;
	};

	const VkPipelineInputAssemblyStateCreateInfo	inputAssemblyStateCreateInfo	=
	{
		VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,	// VkStructureType                            sType;
		DE_NULL,														// const void*                                pNext;
		0u,																// VkPipelineInputAssemblyStateCreateFlags    flags;
		VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,							// VkPrimitiveTopology                        topology;
		VK_FALSE														// VkBool32                                   primitiveRestartEnable;
	};

	const VkPipelineViewportStateCreateInfo			viewportStateCreateInfo			=
	{
		VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,	// VkStructureType                       sType;
		DE_NULL,												// const void*                           pNext;
		0u,														// VkPipelineViewportStateCreateFlags    flags;
		1u,														// deUint32                              viewportCount;
		DE_NULL,												// const VkViewport*                     pViewports;
		1u,														// deUint32                              scissorCount;
		DE_NULL													// const VkRect2D*                       pScissors;
	};

	const VkPipelineRasterizationStateCreateInfo	rasterizationStateCreateInfo	=
	{
		VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,	// VkStructureType                            sType;
		DE_NULL,													// const void*                                pNext;
		0u,															// VkPipelineRasterizationStateCreateFlags    flags;
		VK_FALSE,													// VkBool32                                   depthClampEnable;
		VK_FALSE,													// VkBool32                                   rasterizerDiscardEnable;
		VK_POLYGON_MODE_FILL,										// VkPolygonMode                              polygonMode;
		VK_CULL_MODE_BACK_BIT,										// VkCullModeFlags                            cullMode;
		VK_FRONT_FACE_CLOCKWISE,									// VkFrontFace                                frontFace;
		VK_FALSE,													// VkBool32                                   depthBiasEnable;
		0.0f,														// float                                      depthBiasConstantFactor;
		0.0f,														// float                                      depthBiasClamp;
		0.0f,														// float                                      depthBiasSlopeFactor;
		1.0f														// float                                      lineWidth;
	};

	const VkPipelineMultisampleStateCreateInfo		multisampleStateCreateInfo		=
	{
		VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,	// VkStructureType                          sType;
		DE_NULL,													// const void*                              pNext;
		0u,															// VkPipelineMultisampleStateCreateFlags    flags;
		VK_SAMPLE_COUNT_1_BIT,										// VkSampleCountFlagBits                    rasterizationSamples;
		VK_FALSE,													// VkBool32                                 sampleShadingEnable;
		0.0f,														// float                                    minSampleShading;
		DE_NULL,													// const VkSampleMask*                      pSampleMask;
		VK_FALSE,													// VkBool32                                 alphaToCoverageEnable;
		VK_FALSE													// VkBool32                                 alphaToOneEnable;
	};

	const VkPipelineColorBlendAttachmentState		colorBlendAttachmentState		=
	{
		VK_FALSE,				// VkBool32                 blendEnable;
		VK_BLEND_FACTOR_ZERO,	// VkBlendFactor            srcColorBlendFactor;
		VK_BLEND_FACTOR_ZERO,	// VkBlendFactor            dstColorBlendFactor;
		VK_BLEND_OP_ADD,		// VkBlendOp                colorBlendOp;
		VK_BLEND_FACTOR_ZERO,	// VkBlendFactor            srcAlphaBlendFactor;
		VK_BLEND_FACTOR_ZERO,	// VkBlendFactor            dstAlphaBlendFactor;
		VK_BLEND_OP_ADD,		// VkBlendOp                alphaBlendOp;
		0xf						// VkColorComponentFlags    colorWriteMask;
	};

	const VkPipelineColorBlendStateCreateInfo		colorBlendStateCreateInfo		=
	{
		VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,	// VkStructureType                               sType;
		DE_NULL,													// const void*                                   pNext;
		0u,															// VkPipelineColorBlendStateCreateFlags          flags;
		VK_FALSE,													// VkBool32                                      logicOpEnable;
		VK_LOGIC_OP_COPY,											// VkLogicOp                                     logicOp;
		1u,															// deUint32                                      attachmentCount;
		&colorBlendAttachmentState,									// const VkPipelineColorBlendAttachmentState*    pAttachments;
		{ 1.0f, 1.0f, 1.0f, 1.0f }									// float                                         blendConstants[4];
	};

	const VkDynamicState							dynamicState[]					=
	{
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR
	};

	const VkPipelineDynamicStateCreateInfo			dynamicStateCreateInfo			=
	{
		VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,	// VkStructureType                      sType;
		DE_NULL,												// const void*                          pNext;
		0u,														// VkPipelineDynamicStateCreateFlags    flags;
		2u,														// deUint32                             dynamicStateCount;
		dynamicState											// const VkDynamicState*                pDynamicStates;
	};

	const VkGraphicsPipelineCreateInfo				graphicsPipelineCreateInfo		=
	{
		VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,	// VkStructureType                                  sType;
		DE_NULL,											// const void*                                      pNext;
		0u,													// VkPipelineCreateFlags                            flags;
		2u,													// deUint32                                         stageCount;
		stages,												// const VkPipelineShaderStageCreateInfo*           pStages;
		&vertexInputStateCreateInfo,						// const VkPipelineVertexInputStateCreateInfo*      pVertexInputState;
		&inputAssemblyStateCreateInfo,						// const VkPipelineInputAssemblyStateCreateInfo*    pInputAssemblyState;
		DE_NULL,											// const VkPipelineTessellationStateCreateInfo*     pTessellationState;
		&viewportStateCreateInfo,							// const VkPipelineViewportStateCreateInfo*         pViewportState;
		&rasterizationStateCreateInfo,						// const VkPipelineRasterizationStateCreateInfo*    pRasterizationState;
		&multisampleStateCreateInfo,						// const VkPipelineMultisampleStateCreateInfo*      pMultisampleState;
		DE_NULL,											// const VkPipelineDepthStencilStateCreateInfo*     pDepthStencilState;
		&colorBlendStateCreateInfo,							// const VkPipelineColorBlendStateCreateInfo*       pColorBlendState;
		&dynamicStateCreateInfo,							// const VkPipelineDynamicStateCreateInfo*          pDynamicState;
		*pipelineLayout,									// VkPipelineLayout                                 layout;
		*renderPass,										// VkRenderPass                                     renderPass;
		0u,													// deUint32                                         subpass;
		DE_NULL,											// VkPipeline                                       basePipelineHandle;
		0													// int                                              basePipelineIndex;
	};

	createGraphicsPipeline(vk, vkDevice, params.usePipelineCache ? *pipelineCache : DE_NULL, &graphicsPipelineCreateInfo);

	// Passes as long as createGraphicsPipeline didn't crash.
	return tcu::TestStatus::pass("Pass");
}

void addMatchedAttachmentsTestCasesWithFunctions (tcu::TestCaseGroup* group)
{
	const MatchedAttachmentsTestParams useCache = { true };
	addFunctionCaseWithPrograms(group, "cache", "", initPrograms, testMatchedAttachments, useCache);

	const MatchedAttachmentsTestParams noCache = { false };
	addFunctionCaseWithPrograms(group, "no_cache", "", initPrograms, testMatchedAttachments, noCache);
}

} // anonymous

tcu::TestCaseGroup* createMatchedAttachmentsTests (tcu::TestContext& testCtx)
{
	return createTestGroup(testCtx, "matched_attachments", "Matched attachments tests", addMatchedAttachmentsTestCasesWithFunctions);
}

} // pipeline
} // vkt
