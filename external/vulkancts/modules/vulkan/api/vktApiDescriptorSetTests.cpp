/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
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
 * \brief Descriptor set tests
 *//*--------------------------------------------------------------------*/

#include "vktApiDescriptorSetTests.hpp"
#include "vktTestCaseUtil.hpp"

#include "vkQueryUtil.hpp"
#include "vkRefUtil.hpp"
#include "vkPrograms.hpp"

namespace vkt
{
namespace api
{

namespace
{

using namespace std;
using namespace vk;

// Descriptor set layout used to create a pipeline layout is destroyed prior to creating a pipeline
Move<VkPipelineLayout> createPipelineLayoutDestroyDescriptorSetLayout (const DeviceInterface& vk, const VkDevice& device)
{
	const VkDescriptorSetLayoutCreateInfo	descriptorSetLayoutInfo		=
	{
		VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,	// VkStructureType						sType;
		DE_NULL,												// const void*							pNext;
		(VkDescriptorSetLayoutCreateFlags)0,					// VkDescriptorSetLayoutCreateFlags		flags;
		0u,														// deUint32								bindingCount;
		DE_NULL,												// const VkDescriptorSetLayoutBinding*	pBindings;
	};

	Unique<VkDescriptorSetLayout>			descriptorSetLayout			(createDescriptorSetLayout(vk, device, &descriptorSetLayoutInfo));

	const VkPipelineLayoutCreateInfo		pipelineLayoutCreateInfo	=
	{
		VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,			// VkStructureType					sType;
		DE_NULL,												// const void*						pNext;
		(VkPipelineLayoutCreateFlags)0,							// VkPipelineLayoutCreateFlags		flags;
		1u,														// deUint32							setLayoutCount;
		&descriptorSetLayout.get(),								// const VkDescriptorSetLayout*		pSetLayouts;
		0u,														// deUint32							pushConstantRangeCount;
		DE_NULL													// const VkPushConstantRange*		pPushConstantRanges;
	};

	return createPipelineLayout(vk, device, &pipelineLayoutCreateInfo);
}

tcu::TestStatus descriptorSetLayoutLifetimeGraphicsTest (Context& context)
{
	const DeviceInterface&							vk								= context.getDeviceInterface();
	const VkDevice									device							= context.getDevice();

	Unique<VkPipelineLayout>						pipelineLayout					(createPipelineLayoutDestroyDescriptorSetLayout(vk, device));

	const Unique<VkShaderModule>					vertexShaderModule				(createShaderModule(vk, device, context.getBinaryCollection().get("vertex"), 0));

	const VkPipelineShaderStageCreateInfo			shaderStageCreateInfo			=
	{
		VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,	// VkStructureType					sType;
		DE_NULL,												// const void*						pNext;
		(VkPipelineShaderStageCreateFlags)0,					// VkPipelineShaderStageCreateFlags	flags;
		VK_SHADER_STAGE_VERTEX_BIT,								// VkShaderStageFlagBits			stage;
		vertexShaderModule.get(),								// VkShaderModule					shader;
		"main",													// const char*						pName;
		DE_NULL,												// const VkSpecializationInfo*		pSpecializationInfo;
	};

	const VkPipelineVertexInputStateCreateInfo		vertexInputStateCreateInfo		=
	{
		VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,	// VkStructureType							sType;
		DE_NULL,													// const void*								pNext;
		(VkPipelineVertexInputStateCreateFlags)0,					// VkPipelineVertexInputStateCreateFlags	flags;
		0u,															// deUint32									vertexBindingDescriptionCount;
		DE_NULL,													// const VkVertexInputBindingDescription*	pVertexBindingDescriptions;
		0u,															// deUint32									vertexAttributeDescriptionCount;
		DE_NULL														// const VkVertexInputAttributeDescription*	pVertexAttributeDescriptions;
	};

	const VkPipelineInputAssemblyStateCreateInfo	inputAssemblyStateCreateInfo	=
	{
		VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,	// VkStructureType							sType;
		DE_NULL,														// const void*								pNext;
		(VkPipelineInputAssemblyStateCreateFlags)0,						// VkPipelineInputAssemblyStateCreateFlags	flags;
		VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,							// VkPrimitiveTopology						topology;
		VK_FALSE														// VkBool32									primitiveRestartEnable;
	};

	const VkPipelineRasterizationStateCreateInfo	rasterizationStateCreateInfo	=
	{
		VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,		// VkStructureType							sType;
		DE_NULL,														// const void*								pNext;
		(VkPipelineRasterizationStateCreateFlags)0,						// VkPipelineRasterizationStateCreateFlags	flags;
		VK_FALSE,														// VkBool32									depthClampEnable;
		VK_TRUE,														// VkBool32									rasterizerDiscardEnable;
		VK_POLYGON_MODE_FILL,											// VkPolygonMode							polygonMode;
		VK_CULL_MODE_NONE,												// VkCullModeFlags							cullMode;
		VK_FRONT_FACE_CLOCKWISE,										// VkFrontFace								frontFace;
		VK_FALSE,														// VkBool32									depthBiasEnable;
		0.0f,															// float									depthBiasConstantFactor;
		0.0f,															// float									depthBiasClamp;
		0.0f,															// float									depthBiasSlopeFactor;
		1.0f															// float									lineWidth;
	};

	const VkSubpassDescription						subpassDescription				=
	{
		(VkSubpassDescriptionFlags)0,		// VkSubpassDescriptionFlags		flags;
		VK_PIPELINE_BIND_POINT_GRAPHICS,	// VkPipelineBindPoint				pipelineBindPoint
		0u,									// deUint32							inputAttachmentCount
		DE_NULL,							// const VkAttachmentReference*		pInputAttachments
		0u,									// deUint32							colorAttachmentCount
		DE_NULL,							// const VkAttachmentReference*		pColorAttachments
		DE_NULL,							// const VkAttachmentReference*		pResolveAttachments
		DE_NULL,							// const VkAttachmentReference*		pDepthStencilAttachment
		0u,									// deUint32							preserveAttachmentCount
		DE_NULL								// const deUint32*					pPreserveAttachments
	};

	const VkRenderPassCreateInfo					renderPassCreateInfo			=
	{
		VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,		// VkStructureType					sType;
		DE_NULL,										// const void*						pNext;
		(VkRenderPassCreateFlags)0,						// VkRenderPassCreateFlags			flags;
		0u,												// deUint32							attachmentCount
		DE_NULL,										// const VkAttachmentDescription*	pAttachments
		1u,												// deUint32							subpassCount
		&subpassDescription,							// const VkSubpassDescription*		pSubpasses
		0u,												// deUint32							dependencyCount
		DE_NULL											// const VkSubpassDependency*		pDependencies
	};

	Unique<VkRenderPass>							renderPass						(createRenderPass(vk, device, &renderPassCreateInfo));

	const VkGraphicsPipelineCreateInfo				graphicsPipelineCreateInfo		=
	{
		VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,	// VkStructureType									sType;
		DE_NULL,											// const void*										pNext;
		(VkPipelineCreateFlags)0,							// VkPipelineCreateFlags							flags;
		1u,													// deUint32											stageCount;
		&shaderStageCreateInfo,								// const VkPipelineShaderStageCreateInfo*			pStages;
		&vertexInputStateCreateInfo,						// const VkPipelineVertexInputStateCreateInfo*		pVertexInputState;
		&inputAssemblyStateCreateInfo,						// const VkPipelineInputAssemblyStateCreateInfo*	pInputAssemblyState;
		DE_NULL,											// const VkPipelineTessellationStateCreateInfo*		pTessellationState;
		DE_NULL,											// const VkPipelineViewportStateCreateInfo*			pViewportState;
		&rasterizationStateCreateInfo,						// const VkPipelineRasterizationStateCreateInfo*	pRasterizationState;
		DE_NULL,											// const VkPipelineMultisampleStateCreateInfo*		pMultisampleState;
		DE_NULL,											// const VkPipelineDepthStencilStateCreateInfo*		pDepthStencilState;
		DE_NULL,											// const VkPipelineColorBlendStateCreateInfo*		pColorBlendState;
		DE_NULL,											// const VkPipelineDynamicStateCreateInfo*			pDynamicState;
		pipelineLayout.get(),								// VkPipelineLayout									layout;
		renderPass.get(),									// VkRenderPass										renderPass;
		0u,													// deUint32											subpass;
		DE_NULL,											// VkPipeline										basePipelineHandle;
		0													// int												basePipelineIndex;
	};

	Unique<VkPipeline>								graphicsPipeline				(createGraphicsPipeline(vk, device, DE_NULL, &graphicsPipelineCreateInfo));

	// Test should always pass
	return tcu::TestStatus::pass("Pass");
}

tcu::TestStatus descriptorSetLayoutLifetimeComputeTest (Context& context)
{
	const DeviceInterface&					vk							= context.getDeviceInterface();
	const VkDevice							device						= context.getDevice();

	Unique<VkPipelineLayout>				pipelineLayout				(createPipelineLayoutDestroyDescriptorSetLayout(vk, device));

	const Unique<VkShaderModule>			computeShaderModule			(createShaderModule(vk, device, context.getBinaryCollection().get("compute"), 0));

	const VkPipelineShaderStageCreateInfo	shaderStageCreateInfo		=
	{
		VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,	// VkStructureType					sType;
		DE_NULL,												// const void*						pNext;
		(VkPipelineShaderStageCreateFlags)0,					// VkPipelineShaderStageCreateFlags	flags;
		VK_SHADER_STAGE_COMPUTE_BIT,							// VkShaderStageFlagBits			stage;
		computeShaderModule.get(),								// VkShaderModule					shader;
		"main",													// const char*						pName;
		DE_NULL													// const VkSpecializationInfo*		pSpecializationInfo;
	};

	const VkComputePipelineCreateInfo		computePipelineCreateInfo	=
	{
		VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,		// VkStructureType					sType
		DE_NULL,											// const void*						pNext
		(VkPipelineCreateFlags)0,							// VkPipelineCreateFlags			flags
		shaderStageCreateInfo,								// VkPipelineShaderStageCreateInfo	stage
		pipelineLayout.get(),								// VkPipelineLayout					layout
		DE_NULL,											// VkPipeline						basePipelineHandle
		0													// int								basePipelineIndex
	};

	Unique<VkPipeline>						computePipeline				(createComputePipeline(vk, device, DE_NULL, &computePipelineCreateInfo));

	// Test should always pass
	return tcu::TestStatus::pass("Pass");
}

tcu::TestStatus emptyDescriptorSetLayoutTest (Context& context, VkDescriptorSetLayoutCreateFlags descriptorSetLayoutCreateFlags)
{
	const DeviceInterface&					vk								= context.getDeviceInterface();
	const VkDevice							device							= context.getDevice();

	if (descriptorSetLayoutCreateFlags == VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR)
		if (!context.isDeviceFunctionalitySupported("VK_KHR_push_descriptor"))
			TCU_THROW(NotSupportedError, "VK_KHR_push_descriptor extension not supported");

	const VkDescriptorSetLayoutCreateInfo	descriptorSetLayoutCreateInfo	=
	{
		VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,	// VkStructureType                        sType;
		DE_NULL,												// const void*                            pNext;
		descriptorSetLayoutCreateFlags,							// VkDescriptorSetLayoutCreateFlags       flags;
		0u,														// deUint32                               bindingCount;
		DE_NULL													// const VkDescriptorSetLayoutBinding*    pBindings;
	};

	Unique<VkDescriptorSetLayout>			descriptorSetLayout				(createDescriptorSetLayout(vk, device, &descriptorSetLayoutCreateInfo));

	// Test should always pass
	return tcu::TestStatus::pass("Pass");
}

} // anonymous

void createDescriptorSetLayoutLifetimeGraphicsSource (SourceCollections& dst)
{
	dst.glslSources.add("vertex") << glu::VertexSource(
		"#version 310 es\n"
		"void main (void)\n"
		"{\n"
		"    gl_Position = vec4(0.0, 0.0, 0.0, 1.0);\n"
		"}\n");
}

void createDescriptorSetLayoutLifetimeComputeSource (SourceCollections& dst)
{
	dst.glslSources.add("compute") << glu::ComputeSource(
		"#version 310 es\n"
		"layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;\n"
		"void main (void)\n"
		"{\n"
		"}\n");
}

tcu::TestCaseGroup* createDescriptorSetLayoutLifetimeTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> descriptorSetLayoutLifetimeTests(new tcu::TestCaseGroup(testCtx, "descriptor_set_layout_lifetime", "Descriptor set layout lifetime tests"));

	addFunctionCaseWithPrograms(descriptorSetLayoutLifetimeTests.get(), "graphics", "Test descriptor set layout lifetime in graphics pipeline", createDescriptorSetLayoutLifetimeGraphicsSource, descriptorSetLayoutLifetimeGraphicsTest);
	addFunctionCaseWithPrograms(descriptorSetLayoutLifetimeTests.get(), "compute", "Test descriptor set layout lifetime in compute pipeline", createDescriptorSetLayoutLifetimeComputeSource,  descriptorSetLayoutLifetimeComputeTest);

	return descriptorSetLayoutLifetimeTests.release();
}

tcu::TestCaseGroup* createEmptyDescriptorSetLayoutTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> emptyDescriptorSetLayoutTests(new tcu::TestCaseGroup(testCtx, "empty_set", "Create empty descriptor set layout tests"));

	addFunctionCase(emptyDescriptorSetLayoutTests.get(), "normal", "Create empty desciptor set layout", emptyDescriptorSetLayoutTest, (VkDescriptorSetLayoutCreateFlags)0u);
	addFunctionCase(emptyDescriptorSetLayoutTests.get(), "push_descriptor", "Create empty push descriptor set layout", emptyDescriptorSetLayoutTest, (VkDescriptorSetLayoutCreateFlags)VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR);

	return emptyDescriptorSetLayoutTests.release();
}

tcu::TestCaseGroup* createDescriptorSetLayoutTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> descriptorSetLayoutTests(new tcu::TestCaseGroup(testCtx, "descriptor_set_layout", "Descriptor set layout tests"));

	descriptorSetLayoutTests->addChild(createEmptyDescriptorSetLayoutTests(testCtx));

	return descriptorSetLayoutTests.release();
}

tcu::TestCaseGroup* createDescriptorSetTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> descriptorSetTests(new tcu::TestCaseGroup(testCtx, "descriptor_set", "Descriptor set tests"));

	descriptorSetTests->addChild(createDescriptorSetLayoutLifetimeTests(testCtx));
	descriptorSetTests->addChild(createDescriptorSetLayoutTests(testCtx));

	return descriptorSetTests.release();
}

} // api
} // vkt
