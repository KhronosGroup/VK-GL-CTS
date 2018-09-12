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

tcu::TestStatus testEarlyDestroy (Context& context, bool usePipelineCache)
{
	const DeviceInterface&							vk								= context.getDeviceInterface();
	const VkDevice									vkDevice						= context.getDevice();
	const Unique<VkShaderModule>					vertexShaderModule				(createShaderModule(vk, vkDevice, context.getBinaryCollection().get("color_vert"), 0));
	const Unique<VkShaderModule>					fragmentShaderModule			(createShaderModule(vk, vkDevice, context.getBinaryCollection().get("color_frag"), 0));

	const Unique<VkCommandPool>						cmdPool							(createCommandPool(vk, vkDevice, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, context.getUniversalQueueFamilyIndex()));
	const Unique<VkCommandBuffer>					cmdBuffer						(allocateCommandBuffer(vk, vkDevice, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

	const VkPipelineLayoutCreateInfo				pipelineLayoutCreateInfo		=
	{
		VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,	// VkStructureType                 sType;
		DE_NULL,										// const void*                     pNext;
		0u,												// VkPipelineLayoutCreateFlags     flags;
		0u,												// deUint32                        setLayoutCount;
		DE_NULL,										// const VkDescriptorSetLayout*    pSetLayouts;
		0u,												// deUint32                        pushConstantRangeCount;
		DE_NULL											// const VkPushConstantRange*      pPushConstantRanges;
	};

	const Unique<VkPipelineLayout>					pipelineLayout					(createPipelineLayout(vk, vkDevice, &pipelineLayoutCreateInfo, DE_NULL));

	const Unique<VkRenderPass>						renderPass						(makeRenderPass(vk, vkDevice, VK_FORMAT_R8G8B8A8_UNORM));

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
		VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,							// VkPrimitiveTopology                        topology;
		VK_FALSE														// VkBool32                                   primitiveRestartEnable;
	};

	const VkPipelineRasterizationStateCreateInfo	rasterizationStateCreateInfo	=
	{
		VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,	// VkStructureType                            sType;
		DE_NULL,													// const void*                                pNext;
		0u,															// VkPipelineRasterizationStateCreateFlags    flags;
		VK_FALSE,													// VkBool32                                   depthClampEnable;
		VK_TRUE,													// VkBool32                                   rasterizerDiscardEnable;
		VK_POLYGON_MODE_FILL,										// VkPolygonMode                              polygonMode;
		VK_CULL_MODE_BACK_BIT,										// VkCullModeFlags                            cullMode;
		VK_FRONT_FACE_CLOCKWISE,									// VkFrontFace                                frontFace;
		VK_FALSE,													// VkBool32                                   depthBiasEnable;
		0.0f,														// float                                      depthBiasConstantFactor;
		0.0f,														// float                                      depthBiasClamp;
		0.0f,														// float                                      depthBiasSlopeFactor;
		1.0f														// float                                      lineWidth;
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
		VK_LOGIC_OP_CLEAR,											// VkLogicOp                                     logicOp;
		1u,															// deUint32                                      attachmentCount;
		&colorBlendAttachmentState,									// const VkPipelineColorBlendAttachmentState*    pAttachments;
		{ 0.0f, 0.0f, 0.0f, 0.0f }									// float                                         blendConstants[4];
	};

	const VkPipelineCacheCreateInfo					pipelineCacheCreateInfo			=
	{
		VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO,	// VkStructureType               sType;
		DE_NULL,										// const void*                   pNext;
		0u,												// VkPipelineCacheCreateFlags    flags;
		0u,												// size_t                        initialDataSize;
		DE_NULL											// const void*                   pInitialData;
	};

	const Unique<VkPipelineCache>					pipelineCache					(createPipelineCache(vk, vkDevice, &pipelineCacheCreateInfo));

	const VkGraphicsPipelineCreateInfo				graphicsPipelineCreateInfo		=
	{
		VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,	// VkStructureType                                  sType;
		DE_NULL,											// const void*                                      pNext;
		VK_PIPELINE_CREATE_DISABLE_OPTIMIZATION_BIT,		// VkPipelineCreateFlags                            flags;
		2u,													// deUint32                                         stageCount;
		stages,												// const VkPipelineShaderStageCreateInfo*           pStages;
		&vertexInputStateCreateInfo,						// const VkPipelineVertexInputStateCreateInfo*      pVertexInputState;
		&inputAssemblyStateCreateInfo,						// const VkPipelineInputAssemblyStateCreateInfo*    pInputAssemblyState;
		DE_NULL,											// const VkPipelineTessellationStateCreateInfo*     pTessellationState;
		DE_NULL,											// const VkPipelineViewportStateCreateInfo*         pViewportState;
		&rasterizationStateCreateInfo,						// const VkPipelineRasterizationStateCreateInfo*    pRasterizationState;
		DE_NULL,											// const VkPipelineMultisampleStateCreateInfo*      pMultisampleState;
		DE_NULL,											// const VkPipelineDepthStencilStateCreateInfo*     pDepthStencilState;
		&colorBlendStateCreateInfo,							// const VkPipelineColorBlendStateCreateInfo*       pColorBlendState;
		DE_NULL,											// const VkPipelineDynamicStateCreateInfo*          pDynamicState;
		*pipelineLayout,									// VkPipelineLayout                                 layout;
		*renderPass,										// VkRenderPass                                     renderPass;
		0u,													// deUint32                                         subpass;
		DE_NULL,											// VkPipeline                                       basePipelineHandle;
		0													// int                                              basePipelineIndex;
	};

	createGraphicsPipeline(vk, vkDevice, usePipelineCache ? *pipelineCache : DE_NULL, &graphicsPipelineCreateInfo);

	const VkCommandBufferBeginInfo					cmdBufferBeginInfo				=
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,	// VkStructureType                          sType;
		DE_NULL,										// const void*                              pNext;
		0u,												// VkCommandBufferUsageFlags                flags;
		(const VkCommandBufferInheritanceInfo*)DE_NULL	// const VkCommandBufferInheritanceInfo*    pInheritanceInfo;
	};

	VK_CHECK(vk.beginCommandBuffer(*cmdBuffer, &cmdBufferBeginInfo));
	VK_CHECK(vk.endCommandBuffer(*cmdBuffer));

	// Passes as long as no crash occurred.
	return tcu::TestStatus::pass("Pass");
}

void addEarlyDestroyTestCasesWithFunctions (tcu::TestCaseGroup* group)
{
	addFunctionCaseWithPrograms(group, "cache", "", initPrograms, testEarlyDestroy, true);
	addFunctionCaseWithPrograms(group, "no_cache", "", initPrograms, testEarlyDestroy, false);
}

} // anonymous

tcu::TestCaseGroup* createEarlyDestroyTests (tcu::TestContext& testCtx)
{
	return createTestGroup(testCtx, "early_destroy", "Tests where pipeline is destroyed early", addEarlyDestroyTestCasesWithFunctions);
}

} // pipeline
} // vkt
