/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2021 The Khronos Group Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *	  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *//*!
 * \file
 * \brief  Vulkan SC pipeline identifier tests
*//*--------------------------------------------------------------------*/

#include "vktPipelineIdentifierTests.hpp"

#include <set>
#include <vector>
#include <string>

#include "vktTestCaseUtil.hpp"
#include "vkDefs.hpp"
#include "vkSafetyCriticalUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkRefUtil.hpp"
#include "tcuTestLog.hpp"

namespace vkt
{
namespace sc
{

using namespace vk;

namespace
{

enum PIPipeline
{
	PIP_UNUSED = 0,
	PIP_GRAPHICS,
	PIP_COMPUTE
};

enum PITTestType
{
	PITT_UNUSED = 0,
	PITT_MISSING_ID,
	PITT_NONEXISTING_ID,
	PITT_MATCHCONTROL
};

enum PITMatchControl
{
	PIMC_UNUSED = 0,
	PIMC_UUID_EXACT_MATCH
};

struct TestParams
{
	PITTestType				type;
	PITMatchControl			matchControl;
	bool					single;
};

void createGraphicsShaders (SourceCollections& dst, TestParams testParams)
{
	deUint32	pipelineCount = testParams.single ? 1 : 3;

	for (deUint32 i = 0; i < pipelineCount; ++i)
	{
		std::ostringstream name, code;
		name << "vertex_" << i;
		code <<
			"#version 450\n"
			"\n"
			"void main (void)\n"
			"{\n"
			"   gl_Position = vec4( "<< i <<");\n"
			"}\n";
		dst.glslSources.add(name.str()) << glu::VertexSource(code.str());
	}

	for (deUint32 i = 0; i < pipelineCount; ++i)
	{
		std::ostringstream name, code;
		name << "fragment_" << i;
		code <<
			"#version 450\n"
			"\n"
			"layout(location=0) out vec4 x;\n"
			"void main (void)\n"
			"{\n"
			"   x = vec4(" << i <<");\n"
			"}\n";
			dst.glslSources.add(name.str()) << glu::FragmentSource(code.str());
	}
}

void createComputeShaders (SourceCollections& dst, TestParams testParams)
{
	deUint32	pipelineCount = testParams.single ? 1 : 3;

	for (deUint32 i = 0; i < pipelineCount; ++i)
	{
		std::ostringstream name, code;
		name << "compute_" << i;
		code <<
			"#version 450\n"
			"layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;\n"
			"void main (void)\n"
			"{\n"
			"	uvec4 x = uvec4(" << i <<");\n"
			"}\n";

		dst.glslSources.add(name.str()) << glu::ComputeSource(code.str());
	}
}

tcu::TestStatus testGraphicsPipelineIdentifier (Context& context, TestParams testParams)
{
	const vk::PlatformInterface&							vkp								= context.getPlatformInterface();
	const InstanceInterface&								vki								= context.getInstanceInterface();
	const VkInstance										instance						= context.getInstance();
	const DeviceInterface&									vk								= context.getDeviceInterface();
	const VkDevice											device							= context.getDevice();
	const VkPhysicalDevice									physicalDevice					= context.getPhysicalDevice();

	deUint32												pipelineCount					= testParams.single ? 1 : 3;

	std::vector<Move<VkShaderModule>>						shaders;
	for (deUint32 i = 0; i < pipelineCount; ++i)
	{
		{
			std::ostringstream name;
			name << "vertex_" << i;
			shaders.emplace_back(createShaderModule(vk, device, context.getBinaryCollection().get(name.str()), 0));
		}
		{
			std::ostringstream name;
			name << "fragment_" << i;
			shaders.emplace_back(createShaderModule(vk, device, context.getBinaryCollection().get(name.str()), 0));
		}
	}

	std::vector<std::vector<VkPipelineShaderStageCreateInfo>>	shaderStageCreateInfos	(pipelineCount);
	for (deUint32 i = 0; i < pipelineCount; ++i)
	{
		shaderStageCreateInfos[i].push_back
		(
			{
				VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,						// VkStructureType                     sType;
				DE_NULL,																	// const void*                         pNext;
				(VkPipelineShaderStageCreateFlags)0,										// VkPipelineShaderStageCreateFlags    flags;
				VK_SHADER_STAGE_VERTEX_BIT,													// VkShaderStageFlagBits               stage;
				*shaders[2*i],																// VkShaderModule                      shader;
				"main",																		// const char*                         pName;
				DE_NULL,																	// const VkSpecializationInfo*         pSpecializationInfo;
			}
		);
		shaderStageCreateInfos[i].push_back
		(
			{
				VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,						// VkStructureType                     sType;
				DE_NULL,																	// const void*                         pNext;
				(VkPipelineShaderStageCreateFlags)0,										// VkPipelineShaderStageCreateFlags    flags;
				VK_SHADER_STAGE_FRAGMENT_BIT,												// VkShaderStageFlagBits               stage;
				*shaders[2*i+1],															// VkShaderModule                      shader;
				"main",																		// const char*                         pName;
				DE_NULL,																	// const VkSpecializationInfo*         pSpecializationInfo;
			}
		);
	}

	std::vector<VkPipelineVertexInputStateCreateInfo>		vertexInputStateCreateInfo		(pipelineCount);
	std::vector<VkPipelineInputAssemblyStateCreateInfo>		inputAssemblyStateCreateInfo	(pipelineCount);
	std::vector<VkPipelineViewportStateCreateInfo>			viewPortStateCreateInfo			(pipelineCount);
	std::vector<VkPipelineRasterizationStateCreateInfo>		rasterizationStateCreateInfo	(pipelineCount);
	std::vector<VkPipelineMultisampleStateCreateInfo>		multisampleStateCreateInfo		(pipelineCount);
	std::vector<VkPipelineColorBlendAttachmentState>		colorBlendAttachmentState		(pipelineCount);
	std::vector<VkPipelineColorBlendStateCreateInfo>		colorBlendStateCreateInfo		(pipelineCount);
	std::vector<VkPipelineDynamicStateCreateInfo>			dynamicStateCreateInfo			(pipelineCount);
	std::vector<std::vector<VkDynamicState>>				dynamicStates
	{
		{ VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR },
		{ VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR },
		{ VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR },
	};

	const VkPipelineLayoutCreateInfo						pipelineLayoutCreateInfo		=
	{
		VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,										// VkStructureType                     sType;
		DE_NULL,																			// const void*                         pNext;
		(VkPipelineLayoutCreateFlags)0u,													// VkPipelineLayoutCreateFlags         flags;
		0u,																					// deUint32                            setLayoutCount;
		DE_NULL,																			// const VkDescriptorSetLayout*        pSetLayouts;
		0u,																					// deUint32                            pushConstantRangeCount;
		DE_NULL																				// const VkPushConstantRange*          pPushConstantRanges;
	};
	Move<VkPipelineLayout>									pipelineLayout					= createPipelineLayout(vk, device, &pipelineLayoutCreateInfo);

	const VkFormat											format							= getRenderTargetFormat(vki, physicalDevice);

	VkAttachmentDescription									attachmentDescription;
	VkAttachmentReference									attachmentReference;
	VkSubpassDescription									subpassDescription;
	VkRenderPassCreateInfo									renderPassCreateInfo			= prepareSimpleRenderPassCI(format, attachmentDescription, attachmentReference, subpassDescription);
	Move<VkRenderPass>										renderPass						= createRenderPass(vk, device, &renderPassCreateInfo);

	std::vector<VkGraphicsPipelineCreateInfo>				graphicsPipelineCreateInfos		(pipelineCount);
	for (deUint32 i = 0; i < pipelineCount; ++i)
		graphicsPipelineCreateInfos[i] = prepareSimpleGraphicsPipelineCI(vertexInputStateCreateInfo[i], shaderStageCreateInfos[i], inputAssemblyStateCreateInfo[i], viewPortStateCreateInfo[i],
			rasterizationStateCreateInfo[i], multisampleStateCreateInfo[i], colorBlendAttachmentState[i], colorBlendStateCreateInfo[i], dynamicStateCreateInfo[i], dynamicStates[i], *pipelineLayout, *renderPass);

	std::vector<std::string>								sourcePID						{ "IDG_0000", "IDG_1111", "IDG_2222" };
	std::vector<std::string>								destPID;

	switch (testParams.type)
	{
	case PITT_MISSING_ID:
	case PITT_NONEXISTING_ID:
		destPID = { "IDG_XXXX", "IDG_1111", "IDG_2222" };
		break;
	case PITT_MATCHCONTROL:
		switch (testParams.matchControl)
		{
		case PIMC_UUID_EXACT_MATCH:
			destPID = { "IDG_0000", "IDG_1111", "IDG_2222" };
			break;
		default:
			TCU_THROW(InternalError, "Unrecognized match control");
		}
		break;
	default:
		TCU_THROW(InternalError, "Unrecognized test type");
	}

	// fill pipeline identifiers with initial values, apply pipeline names from sourcePID
	std::vector<VkPipelineOfflineCreateInfo>				pipelineIDs;
	for (deUint32 i = 0; i < pipelineCount; ++i)
	{
		pipelineIDs.emplace_back(resetPipelineOfflineCreateInfo());
		applyPipelineIdentifier(pipelineIDs[i], sourcePID[i]);
	}

	switch (testParams.matchControl)
	{
	case PIMC_UUID_EXACT_MATCH:
		for (deUint32 i = 0; i < pipelineCount; ++i)
			pipelineIDs[i].matchControl = VK_PIPELINE_MATCH_CONTROL_APPLICATION_UUID_EXACT_MATCH;
		break;
	default:
		TCU_THROW(InternalError, "Unrecognized match control");
	}

	if (!context.getTestContext().getCommandLine().isSubProcess())
	{
		// If it's a main process - we create graphics pipelines only to increase VkDeviceObjectReservationCreateInfo::computePipelineRequestCount.
		// We also fill all pipeline identifiers with distinct values ( otherwise the framework will create pipeline identifiers itself )
		for (deUint32 i = 0; i < pipelineCount; ++i)
		{
			pipelineIDs[i].pNext					= graphicsPipelineCreateInfos[i].pNext;
			graphicsPipelineCreateInfos[i].pNext	= &pipelineIDs[i];
		}

		std::vector<Move<VkPipeline>> pipelines;
		for (deUint32 i = 0; i < pipelineCount; ++i)
			pipelines.emplace_back(createGraphicsPipeline(vk, device, DE_NULL, &graphicsPipelineCreateInfos[i]));
		return tcu::TestStatus::pass("Pass");
	}

	for (deUint32 i = 0; i < pipelineCount; ++i)
		context.getResourceInterface()->fillPoolEntrySize(pipelineIDs[i]);

	// subprocess - we create the same pipeline, but we use vkCreateGraphicsPipelines directly to skip the framework
	GetDeviceProcAddrFunc									getDeviceProcAddrFunc			= (GetDeviceProcAddrFunc)vkp.getInstanceProcAddr(instance, "vkGetDeviceProcAddr");
	CreateGraphicsPipelinesFunc								createGraphicsPipelinesFunc		= (CreateGraphicsPipelinesFunc)getDeviceProcAddrFunc(device, "vkCreateGraphicsPipelines");
	DestroyPipelineFunc										destroyPipelineFunc				= (DestroyPipelineFunc)getDeviceProcAddrFunc(device, "vkDestroyPipeline");
	VkPipelineCache											pipelineCache					= context.getResourceInterface()->getPipelineCache(device);
	std::vector<VkPipeline>									pipelines						(pipelineCount);

	VkResult												expectedResult;
	std::vector<deUint8>									expectedNullHandle				(pipelineCount);
	switch (testParams.type)
	{
	case PITT_MISSING_ID:
		expectedResult = VK_ERROR_NO_PIPELINE_MATCH;
		expectedNullHandle[0] = 1;
		for (deUint32 i = 1; i < pipelineCount; ++i)
		{
			// we are skipping pipeline identifier at index 0
			applyPipelineIdentifier(pipelineIDs[i], destPID[i]);
			pipelineIDs[i].pNext					= graphicsPipelineCreateInfos[i].pNext;
			graphicsPipelineCreateInfos[i].pNext	= &pipelineIDs[i];
			expectedNullHandle[i] = 0;
		}
		break;
	case PITT_NONEXISTING_ID:
		expectedResult = VK_ERROR_NO_PIPELINE_MATCH;
		for (deUint32 i = 0; i < pipelineCount; ++i)
		{
			// Pipeline identifier at index 0 uses wrong ID for PITT_NONEXISTING_ID test
			// or a proper one for PITT_MATCHCONTROL test
			applyPipelineIdentifier(pipelineIDs[i], destPID[i]);
			pipelineIDs[i].pNext					= graphicsPipelineCreateInfos[i].pNext;
			graphicsPipelineCreateInfos[i].pNext	= &pipelineIDs[i];
			expectedNullHandle[i] = (i == 0);
		}
		break;
	case PITT_MATCHCONTROL:
		expectedResult = VK_SUCCESS;
		for (deUint32 i = 0; i < pipelineCount; ++i)
		{
			// Pipeline identifier at index 0 uses wrong ID for PITT_NONEXISTING_ID test
			// or a proper one for PITT_MATCHCONTROL test
			applyPipelineIdentifier(pipelineIDs[i], destPID[i]);
			pipelineIDs[i].pNext					= graphicsPipelineCreateInfos[i].pNext;
			graphicsPipelineCreateInfos[i].pNext	= &pipelineIDs[i];
			expectedNullHandle[i] = 0;
		}
		break;
	default:
		TCU_THROW(InternalError, "Unrecognized match control");
	}

	VkResult												result						= createGraphicsPipelinesFunc(device, pipelineCache, pipelineCount, graphicsPipelineCreateInfos.data(), DE_NULL, pipelines.data());
	bool													isOK						= true;
	for (deUint32 i = 0; i < pipelineCount; ++i)
	{
		if (expectedNullHandle[i] == 0 && pipelines[i] == DE_NULL)
		{
			context.getTestContext().getLog() << tcu::TestLog::Message << "Pipeline "<< i << " should be created" << tcu::TestLog::EndMessage;
			isOK = false;
		}
		if (expectedNullHandle[i] != 0 && pipelines[i] != DE_NULL)
		{
			context.getTestContext().getLog() << tcu::TestLog::Message << "Pipeline " << i << " should not be created" << tcu::TestLog::EndMessage;
			isOK = false;
		}
	}

	if (result != expectedResult)
	{
		context.getTestContext().getLog() << tcu::TestLog::Message << "vkCreateGraphicsPipelines returned wrong VkResult" << tcu::TestLog::EndMessage;
		isOK = false;
	}

	for (deUint32 i = 0; i < pipelineCount; ++i)
		destroyPipelineFunc(device, pipelines[i], DE_NULL);

	return isOK ? tcu::TestStatus::pass("Pass") : tcu::TestStatus::fail("Fail");
}

tcu::TestStatus testComputePipelineIdentifier (Context& context, TestParams testParams)
{
	const vk::PlatformInterface&					vkp							= context.getPlatformInterface();
	const VkInstance								instance					= context.getInstance();
	const DeviceInterface&							vk							= context.getDeviceInterface();
	const VkDevice									device						= context.getDevice();

	deUint32										pipelineCount				= testParams.single ? 1 : 3;

	std::vector<Move<VkShaderModule>>				computeShaders;
	for (deUint32 i = 0; i < pipelineCount; ++i)
	{
		std::ostringstream name;
		name << "compute_" << i;
		computeShaders.emplace_back(createShaderModule(vk, device, context.getBinaryCollection().get(name.str()), 0));
	}

	std::vector<VkPipelineShaderStageCreateInfo>	shaderStageCreateInfos		(pipelineCount);
	for (deUint32 i = 0; i < pipelineCount; ++i)
	{
		shaderStageCreateInfos[i] =
		{
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,				// VkStructureType                     sType;
			DE_NULL,															// const void*                         pNext;
			(VkPipelineShaderStageCreateFlags)0,								// VkPipelineShaderStageCreateFlags    flags;
			VK_SHADER_STAGE_COMPUTE_BIT,										// VkShaderStageFlagBits               stage;
			*computeShaders[i],													// VkShaderModule                      shader;
			"main",																// const char*                         pName;
			DE_NULL,															// const VkSpecializationInfo*         pSpecializationInfo;
		};
	}

	const VkPipelineLayoutCreateInfo				pipelineLayoutCreateInfo	=
	{
		VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,							// VkStructureType                     sType;
		DE_NULL,																// const void*                         pNext;
		(VkPipelineLayoutCreateFlags)0u,										// VkPipelineLayoutCreateFlags         flags;
		0u,																		// deUint32                            setLayoutCount;
		DE_NULL,																// const VkDescriptorSetLayout*        pSetLayouts;
		0u,																		// deUint32                            pushConstantRangeCount;
		DE_NULL																	// const VkPushConstantRange*          pPushConstantRanges;
	};
	Move<VkPipelineLayout>							pipelineLayout				= createPipelineLayout(vk, device, &pipelineLayoutCreateInfo);

	std::vector<VkComputePipelineCreateInfo>		computePipelineCreateInfos	(pipelineCount);
	for (deUint32 i = 0; i < pipelineCount; ++i)
		computePipelineCreateInfos[i] = prepareSimpleComputePipelineCI(shaderStageCreateInfos[i], *pipelineLayout);

	std::vector<std::string>						sourcePID					{ "IDC_0000", "IDC_1111", "IDC_2222" };
	std::vector<std::string>						destPID;

	switch (testParams.type)
	{
	case PITT_MISSING_ID:
	case PITT_NONEXISTING_ID:
		destPID = { "IDC_XXXX", "IDC_1111", "IDC_2222" };
		break;
	case PITT_MATCHCONTROL:
		switch (testParams.matchControl)
		{
		case PIMC_UUID_EXACT_MATCH:
			destPID = { "IDC_0000", "IDC_1111", "IDC_2222" };
			break;
		default:
			TCU_THROW(InternalError, "Unrecognized match control");
		}
		break;
	default:
		TCU_THROW(InternalError, "Unrecognized test type");
	}

	// fill pipeline identifiers with initial values, apply pipeline names from sourcePID
	std::vector<VkPipelineOfflineCreateInfo>		pipelineIDs;
	for (deUint32 i = 0; i < pipelineCount; ++i)
	{
		pipelineIDs.emplace_back(resetPipelineOfflineCreateInfo());
		applyPipelineIdentifier(pipelineIDs[i], sourcePID[i]);
	}

	switch (testParams.matchControl)
	{
	case PIMC_UUID_EXACT_MATCH:
		for (deUint32 i = 0; i < pipelineCount; ++i)
			pipelineIDs[i].matchControl = VK_PIPELINE_MATCH_CONTROL_APPLICATION_UUID_EXACT_MATCH;
		break;
	default:
		TCU_THROW(InternalError, "Unrecognized match control");
	}

	if (!context.getTestContext().getCommandLine().isSubProcess())
	{
		// If it's a main process - we create compute pipelines only to increase VkDeviceObjectReservationCreateInfo::computePipelineRequestCount.
		// We also fill all pipeline identifiers with distinct values ( otherwise the framework will create pipeline identifiers itself )
		for (deUint32 i = 0; i < pipelineCount; ++i)
		{
			pipelineIDs[i].pNext					= computePipelineCreateInfos[i].pNext;
			computePipelineCreateInfos[i].pNext		= &pipelineIDs[i];
		}

		std::vector<Move<VkPipeline>> pipelines;
		for (deUint32 i = 0; i < pipelineCount; ++i)
			pipelines.emplace_back(createComputePipeline(vk, device, DE_NULL, &computePipelineCreateInfos[i]));
		return tcu::TestStatus::pass("Pass");
	}

	for (deUint32 i = 0; i < pipelineCount; ++i)
		context.getResourceInterface()->fillPoolEntrySize(pipelineIDs[i]);

	// In subprocess we create the same pipelines, but we use vkCreateGraphicsPipelines directly to skip the framework
	GetDeviceProcAddrFunc							getDeviceProcAddrFunc		= (GetDeviceProcAddrFunc)vkp.getInstanceProcAddr(instance, "vkGetDeviceProcAddr");
	CreateComputePipelinesFunc						createComputePipelinesFunc	= (CreateComputePipelinesFunc)getDeviceProcAddrFunc(device, "vkCreateComputePipelines");
	DestroyPipelineFunc								destroyPipelineFunc			= (DestroyPipelineFunc)getDeviceProcAddrFunc(device, "vkDestroyPipeline");
	VkPipelineCache									pipelineCache				= context.getResourceInterface()->getPipelineCache(device);
	std::vector<VkPipeline>							pipelines					(pipelineCount);

	VkResult										expectedResult;
	std::vector<deUint8>							expectedNullHandle			(pipelineCount);
	switch (testParams.type)
	{
	case PITT_MISSING_ID:
		expectedResult = VK_ERROR_NO_PIPELINE_MATCH;
		expectedNullHandle[0] = 1;
		for (deUint32 i = 1; i < pipelineCount; ++i)
		{
			// we are skipping pipeline identifier at index 0
			applyPipelineIdentifier(pipelineIDs[i], destPID[i]);
			pipelineIDs[i].pNext					= computePipelineCreateInfos[i].pNext;
			computePipelineCreateInfos[i].pNext		= &pipelineIDs[i];
			expectedNullHandle[i]					= 0;
		}
		break;
	case PITT_NONEXISTING_ID:
		expectedResult = VK_ERROR_NO_PIPELINE_MATCH;
		for (deUint32 i = 0; i < pipelineCount; ++i)
		{
			// Pipeline identifier at index 0 uses wrong ID for PITT_NONEXISTING_ID test
			// or a proper one for PITT_MATCHCONTROL test
			applyPipelineIdentifier(pipelineIDs[i], destPID[i]);
			pipelineIDs[i].pNext					= computePipelineCreateInfos[i].pNext;
			computePipelineCreateInfos[i].pNext		= &pipelineIDs[i];
			expectedNullHandle[i]					= (i == 0);
		}
		break;
	case PITT_MATCHCONTROL:
		expectedResult = VK_SUCCESS;
		for (deUint32 i = 0; i < pipelineCount; ++i)
		{
			// Pipeline identifier at index 0 uses wrong ID for PITT_NONEXISTING_ID test
			// or a proper one for PITT_MATCHCONTROL test
			applyPipelineIdentifier(pipelineIDs[i], destPID[i]);
			pipelineIDs[i].pNext					= computePipelineCreateInfos[i].pNext;
			computePipelineCreateInfos[i].pNext		= &pipelineIDs[i];
			expectedNullHandle[i]					= 0;
		}
		break;
	default:
		TCU_THROW(InternalError, "Unrecognized match control");
	}

	VkResult										result						= createComputePipelinesFunc(device, pipelineCache, pipelineCount, computePipelineCreateInfos.data(), DE_NULL, pipelines.data());

	bool isOK = true;
	for (deUint32 i = 0; i < pipelineCount; ++i)
	{
		if (expectedNullHandle[i] == 0 && pipelines[i] == DE_NULL)
		{
			context.getTestContext().getLog() << tcu::TestLog::Message << "Pipeline "<< i << " should be created" << tcu::TestLog::EndMessage;
			isOK = false;
		}
		if (expectedNullHandle[i] != 0 && pipelines[i] != DE_NULL)
		{
			context.getTestContext().getLog() << tcu::TestLog::Message << "Pipeline " << i << " should not be created" << tcu::TestLog::EndMessage;
			isOK = false;
		}
	}

	if (result != expectedResult)
	{
		context.getTestContext().getLog() << tcu::TestLog::Message << "vkCreateComputePipelines returned wrong VkResult" << tcu::TestLog::EndMessage;
		isOK = false;
	}

	for (deUint32 i = 0; i < pipelineCount; ++i)
		destroyPipelineFunc(device, pipelines[i], DE_NULL);


	return isOK ? tcu::TestStatus::pass("Pass") : tcu::TestStatus::fail("Fail");
}

} // anonymous

tcu::TestCaseGroup*	createPipelineIdentifierTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(testCtx, "pipeline_identifier", "Tests verifying Vulkan SC pipeline identifier structure"));

	const struct
	{
		PIPipeline									pipeline;
		const char*									name;
		FunctionPrograms1<TestParams>::Function		initPrograms;
		FunctionInstance1<TestParams>::Function		testFunction;
	} pipelineTypes[] =
	{
		{ PIP_GRAPHICS,				"graphics",	createGraphicsShaders,	testGraphicsPipelineIdentifier	},
		{ PIP_COMPUTE,				"compute",	createComputeShaders,	testComputePipelineIdentifier	},
	};

	const struct
	{
		PITTestType									type;
		const char*									name;
	} testTypes[] =
	{
		{ PITT_MISSING_ID,			"missing_pid"		},
		{ PITT_NONEXISTING_ID,		"nonexisting_pid"	},
		{ PITT_MATCHCONTROL,		"match_control"		},
	};

	const struct
	{
		PITMatchControl								control;
		const char*									name;
	} matchControls[] =
	{
		{ PIMC_UUID_EXACT_MATCH,	"exact_match"		},
	};

	const struct
	{
		bool										single;
		const char*									name;
	} cardinalities[] =
	{
		{ true,						"single"	},
		{ false,					"multiple"	},
	};

	for (int pipelineIdx = 0; pipelineIdx < DE_LENGTH_OF_ARRAY(pipelineTypes); ++pipelineIdx)
	{
		de::MovePtr<tcu::TestCaseGroup> pipelineGroup(new tcu::TestCaseGroup(testCtx, pipelineTypes[pipelineIdx].name, ""));

		for (int typeIdx = 0; typeIdx < DE_LENGTH_OF_ARRAY(testTypes); ++typeIdx)
		{
			de::MovePtr<tcu::TestCaseGroup> typeGroup(new tcu::TestCaseGroup(testCtx, testTypes[typeIdx].name, ""));

			for (int matchIdx = 0; matchIdx < DE_LENGTH_OF_ARRAY(matchControls); ++matchIdx)
			{
				de::MovePtr<tcu::TestCaseGroup> matchGroup(new tcu::TestCaseGroup(testCtx, matchControls[matchIdx].name, ""));

				for (int cardIdx = 0; cardIdx < DE_LENGTH_OF_ARRAY(cardinalities); ++cardIdx)
				{
					TestParams testParams{ testTypes[typeIdx].type, matchControls[matchIdx].control, cardinalities[cardIdx].single };

					addFunctionCaseWithPrograms(matchGroup.get(), cardinalities[cardIdx].name, "", pipelineTypes[pipelineIdx].initPrograms, pipelineTypes[pipelineIdx].testFunction, testParams);
				}
				typeGroup->addChild(matchGroup.release());
			}
			pipelineGroup->addChild(typeGroup.release());
		}
		group->addChild(pipelineGroup.release());
	}
	return group.release();
}

}	// sc

}	// vkt
