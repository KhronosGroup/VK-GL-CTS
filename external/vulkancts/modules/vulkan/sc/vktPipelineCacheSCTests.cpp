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
 * \brief  Vulkan SC pipeline cache tests
*//*--------------------------------------------------------------------*/

#include "vktPipelineCacheSCTests.hpp"

#include <set>
#include <vector>
#include <string>

#include "vktTestCaseUtil.hpp"
#include "vktCustomInstancesDevices.hpp"
#include "vkDefs.hpp"
#include "vkDeviceUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkRefUtil.hpp"
#include "vkSafetyCriticalUtil.hpp"
#include "tcuTestLog.hpp"

namespace vkt
{
namespace sc
{

using namespace vk;

namespace
{

enum PipelineCacheTestType
{
	PCTT_UNUSED = 0,
	PCTT_WRONG_VENDOR_ID,
	PCTT_WRONG_DEVICE_ID
};

struct TestParams
{
	PipelineCacheTestType	type;
};

void createShaders (SourceCollections& dst, TestParams params)
{
	DE_UNREF(params);

	{
		std::ostringstream name, code;
		name << "vertex";
		code <<
			"#version 450\n"
			"\n"
			"void main (void)\n"
			"{\n"
			"   gl_Position = vec4( 1.0 );\n"
			"}\n";
		dst.glslSources.add(name.str()) << glu::VertexSource(code.str());
	}

	{
		std::ostringstream name, code;
		name << "fragment";
		code <<
			"#version 450\n"
			"\n"
			"layout(location=0) out vec4 x;\n"
			"void main (void)\n"
			"{\n"
			"   x = vec4( 1.0 );\n"
			"}\n";
		dst.glslSources.add(name.str()) << glu::FragmentSource(code.str());
	}

	{
		std::ostringstream name, code;
		name << "compute";
		code <<
			"#version 450\n"
			"layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;\n"
			"void main (void)\n"
			"{\n"
			"	uvec4 x = uvec4( 1 );\n"
			"}\n";
		dst.glslSources.add(name.str()) << glu::ComputeSource(code.str());
	}
}

tcu::TestStatus createPipelineCacheTest (Context& context, TestParams testParams)
{
	const vk::PlatformInterface&							vkp							= context.getPlatformInterface();
	const CustomInstance									instance					(createCustomInstanceFromContext(context));
	const InstanceDriver&									instanceDriver				(instance.getDriver());
	const VkPhysicalDevice									physicalDevice				= chooseDevice(instanceDriver, instance, context.getTestContext().getCommandLine());

	std::string												graphicsPID					= "PCST_GRAPHICS";
	std::string												computePID					= "PCST_COMPUTE";

	// In main process : prepare one graphics pipeline and one compute pipeline
	// Actually these pipelines are here only to ensure that pipeline cache is not empty. We don't use them in subprocess
	if (!context.getTestContext().getCommandLine().isSubProcess())
	{
		const DeviceInterface&								vk							= context.getDeviceInterface();
		const VkDevice										device						= context.getDevice();

		// graphics pipeline
		{
			Move<VkShaderModule>							vertexShader				= createShaderModule(vk, device, context.getBinaryCollection().get("vertex"), 0);
			Move<VkShaderModule>							fragmentShader				= createShaderModule(vk, device, context.getBinaryCollection().get("fragment"), 0);

			std::vector<VkPipelineShaderStageCreateInfo>	shaderStageCreateInfos;
			shaderStageCreateInfos.push_back
			(
				{
					VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,				// VkStructureType                     sType;
					DE_NULL,															// const void*                         pNext;
					(VkPipelineShaderStageCreateFlags)0,								// VkPipelineShaderStageCreateFlags    flags;
					VK_SHADER_STAGE_VERTEX_BIT,											// VkShaderStageFlagBits               stage;
					*vertexShader,														// VkShaderModule                      shader;
					"main",																// const char*                         pName;
					DE_NULL,															// const VkSpecializationInfo*         pSpecializationInfo;
				}
			);
			shaderStageCreateInfos.push_back
			(
				{
					VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,				// VkStructureType                     sType;
					DE_NULL,															// const void*                         pNext;
					(VkPipelineShaderStageCreateFlags)0,								// VkPipelineShaderStageCreateFlags    flags;
					VK_SHADER_STAGE_FRAGMENT_BIT,										// VkShaderStageFlagBits               stage;
					*fragmentShader,													// VkShaderModule                      shader;
					"main",																// const char*                         pName;
					DE_NULL,															// const VkSpecializationInfo*         pSpecializationInfo;
				}
			);

			VkPipelineVertexInputStateCreateInfo			vertexInputStateCreateInfo;
			VkPipelineInputAssemblyStateCreateInfo			inputAssemblyStateCreateInfo;
			VkPipelineViewportStateCreateInfo				viewPortStateCreateInfo;
			VkPipelineRasterizationStateCreateInfo			rasterizationStateCreateInfo;
			VkPipelineMultisampleStateCreateInfo			multisampleStateCreateInfo;
			VkPipelineColorBlendAttachmentState				colorBlendAttachmentState;
			VkPipelineColorBlendStateCreateInfo				colorBlendStateCreateInfo;
			VkPipelineDynamicStateCreateInfo				dynamicStateCreateInfo;
			std::vector<VkDynamicState>						dynamicStates{ VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };

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

			const VkFormat									format						= getRenderTargetFormat(instanceDriver, physicalDevice);

			VkAttachmentDescription							attachmentDescription;
			VkAttachmentReference							attachmentReference;
			VkSubpassDescription							subpassDescription;
			VkRenderPassCreateInfo							renderPassCreateInfo		= prepareSimpleRenderPassCI(format, attachmentDescription, attachmentReference, subpassDescription);
			Move<VkRenderPass>								renderPass					= createRenderPass(vk, device, &renderPassCreateInfo);

			VkGraphicsPipelineCreateInfo					graphicsPipelineCreateInfo	= prepareSimpleGraphicsPipelineCI(vertexInputStateCreateInfo, shaderStageCreateInfos, inputAssemblyStateCreateInfo, viewPortStateCreateInfo,
					rasterizationStateCreateInfo, multisampleStateCreateInfo, colorBlendAttachmentState, colorBlendStateCreateInfo, dynamicStateCreateInfo, dynamicStates, *pipelineLayout, *renderPass);

			// connect pipeline identifier
			VkPipelineOfflineCreateInfo						pipelineID					= resetPipelineOfflineCreateInfo();
			applyPipelineIdentifier(pipelineID, graphicsPID);
			pipelineID.pNext															= graphicsPipelineCreateInfo.pNext;
			graphicsPipelineCreateInfo.pNext											= &pipelineID;

			// creation of a pipeline in main process registers it in pipeline cache
			Move<VkPipeline>								graphicsPipeline			= createGraphicsPipeline(vk, device, DE_NULL, &graphicsPipelineCreateInfo);
		}

		// compute pipeline
		{
			Move<VkShaderModule>							computeShader				= createShaderModule(vk, device, context.getBinaryCollection().get("compute"), 0);
			VkPipelineShaderStageCreateInfo					shaderStageCreateInfo		=
			{
				VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,					// VkStructureType                     sType;
				DE_NULL,																// const void*                         pNext;
				(VkPipelineShaderStageCreateFlags)0,									// VkPipelineShaderStageCreateFlags    flags;
				VK_SHADER_STAGE_COMPUTE_BIT,											// VkShaderStageFlagBits               stage;
				*computeShader,															// VkShaderModule                      shader;
				"main",																	// const char*                         pName;
				DE_NULL,																// const VkSpecializationInfo*         pSpecializationInfo;
			};

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

			VkComputePipelineCreateInfo						computePipelineCreateInfo	= prepareSimpleComputePipelineCI(shaderStageCreateInfo, *pipelineLayout);

			// connect pipeline identifier
			VkPipelineOfflineCreateInfo						pipelineID					= resetPipelineOfflineCreateInfo();
			applyPipelineIdentifier(pipelineID, computePID);
			pipelineID.pNext															= computePipelineCreateInfo.pNext;
			computePipelineCreateInfo.pNext												= &pipelineID;

			// creation of a pipeline in main process registers it in pipeline cache
			Move<VkPipeline>								computePipeline				= createComputePipeline(vk, device, DE_NULL, &computePipelineCreateInfo);
		}
		return tcu::TestStatus::pass("Pass");
	}

	// Subprocess : prepare pipeline cache data according to test type
	// Copy data from ResourceInterface
	std::vector<deUint8>									customCacheData				(context.getResourceInterface()->getCacheDataSize());
	deMemcpy(customCacheData.data(), context.getResourceInterface()->getCacheData(), context.getResourceInterface()->getCacheDataSize());
	deUintptr												initialDataSize				= customCacheData.size();
	VkPipelineCacheHeaderVersionSafetyCriticalOne*			pcHeader					= (VkPipelineCacheHeaderVersionSafetyCriticalOne*)customCacheData.data();

	switch (testParams.type)
	{
		case PCTT_WRONG_VENDOR_ID:
		{
			pcHeader->headerVersionOne.vendorID = deUint32(VK_VENDOR_ID_MAX_ENUM);
			break;
		}
		case PCTT_WRONG_DEVICE_ID:
		{
			pcHeader->headerVersionOne.deviceID = 0xFFFFFFFF;
			break;
		}
		default:
			TCU_THROW(InternalError, "Unrecognized test type");
	}

	// Now, create custom device
	const float												queuePriority				= 1.0f;

	const VkDeviceQueueCreateInfo							deviceQueueCI				=
	{
		VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,										// sType
		DE_NULL,																		// pNext
		(VkDeviceQueueCreateFlags)0u,													// flags
		0,																				//queueFamilyIndex;
		1,																				//queueCount;
		&queuePriority,																	//pQueuePriorities;
	};

	VkDeviceCreateInfo										deviceCreateInfo			=
	{
		VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,											// sType;
		DE_NULL,																		// pNext;
		(VkDeviceCreateFlags)0u,														// flags
		1,																				// queueRecordCount;
		&deviceQueueCI,																	// pRequestedQueues;
		0,																				// layerCount;
		DE_NULL,																		// ppEnabledLayerNames;
		0,																				// extensionCount;
		DE_NULL,																		// ppEnabledExtensionNames;
		DE_NULL,																		// pEnabledFeatures;
	};

	VkDeviceObjectReservationCreateInfo						objectInfo					= resetDeviceObjectReservationCreateInfo();
	objectInfo.pNext																	= DE_NULL;
	objectInfo.pipelineLayoutRequestCount												= 2u;
	objectInfo.renderPassRequestCount													= 1u;
	objectInfo.subpassDescriptionRequestCount											= 1u;
	objectInfo.attachmentDescriptionRequestCount										= 1u;
	objectInfo.graphicsPipelineRequestCount												= 1u;
	objectInfo.computePipelineRequestCount												= 1u;
	objectInfo.pipelineCacheRequestCount												= 2u;

	VkPipelineCacheCreateInfo								pipelineCacheCreateInfo		=
	{
		VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO,									// VkStructureType				sType;
		DE_NULL,																		// const void*					pNext;
		VK_PIPELINE_CACHE_CREATE_READ_ONLY_BIT |
			VK_PIPELINE_CACHE_CREATE_USE_APPLICATION_STORAGE_BIT,						// VkPipelineCacheCreateFlags	flags;
		initialDataSize,																// deUintptr					initialDataSize;
		customCacheData.data()															// const void*					pInitialData;
	};
	objectInfo.pipelineCacheCreateInfoCount												= 1u;
	objectInfo.pPipelineCacheCreateInfos												= &pipelineCacheCreateInfo;

	std::vector<VkPipelinePoolSize>							poolSizes					= context.getResourceInterface()->getPipelinePoolSizes();
	if (!poolSizes.empty())
	{
		objectInfo.pipelinePoolSizeCount												= deUint32(poolSizes.size());
		objectInfo.pPipelinePoolSizes													= poolSizes.data();
	}
	void* pNext																			= &objectInfo;

	VkPhysicalDeviceVulkanSC10Features						sc10Features				= createDefaultSC10Features();
	sc10Features.pNext																	= pNext;
	pNext																				= &sc10Features;

	deviceCreateInfo.pNext																= pNext;

	tcu::TestStatus											testStatus					= tcu::TestStatus::pass("Pass");
	Move<VkDevice>											device;
	{
		std::vector<const char*>								enabledLayers;

		if (deviceCreateInfo.enabledLayerCount == 0u && context.getTestContext().getCommandLine().isValidationEnabled())
		{
			enabledLayers							= getValidationLayers(instanceDriver, physicalDevice);
			deviceCreateInfo.enabledLayerCount		= static_cast<deUint32>(enabledLayers.size());
			deviceCreateInfo.ppEnabledLayerNames	= (enabledLayers.empty() ? DE_NULL : enabledLayers.data());
		}
		VkDevice		object	= 0;
		VkResult		result	= instanceDriver.createDevice(physicalDevice, &deviceCreateInfo, DE_NULL, &object);
		switch (testParams.type)
		{
			case PCTT_WRONG_VENDOR_ID:
			case PCTT_WRONG_DEVICE_ID:
				if (result != VK_ERROR_INVALID_PIPELINE_CACHE_DATA)
					testStatus = tcu::TestStatus::fail("Fail");
				break;
			default:
				TCU_THROW(InternalError, "Unrecognized test type");
		}
		if (result != VK_SUCCESS)
			return testStatus;
		device					=  Move<VkDevice>(check<VkDevice>(object), Deleter<VkDevice>(vkp, instance, object, DE_NULL));
	}

	// create our own pipeline cache in subprocess. Use VK functions directly
	GetDeviceProcAddrFunc									getDeviceProcAddrFunc		= (GetDeviceProcAddrFunc)vkp.getInstanceProcAddr(instance, "vkGetDeviceProcAddr");
	CreatePipelineCacheFunc									createPipelineCacheFunc		= (CreatePipelineCacheFunc)getDeviceProcAddrFunc(*device, "vkCreatePipelineCache");
	DestroyPipelineCacheFunc								destroyPipelineCacheFunc	= (DestroyPipelineCacheFunc)getDeviceProcAddrFunc(*device, "vkDestroyPipelineCache");

	VkPipelineCache											pipelineCache;
	VkResult												result						= createPipelineCacheFunc(*device, &pipelineCacheCreateInfo, DE_NULL, &pipelineCache);

	switch (testParams.type)
	{
		case PCTT_WRONG_VENDOR_ID:
		case PCTT_WRONG_DEVICE_ID:
			if (result != VK_ERROR_INVALID_PIPELINE_CACHE_DATA)
				testStatus = tcu::TestStatus::fail("Fail");
			break;
		default:
			TCU_THROW(InternalError, "Unrecognized test type");
	}

	if (result == VK_SUCCESS)
		destroyPipelineCacheFunc(*device, pipelineCache, DE_NULL);

	return testStatus;
}

} // anonymous

tcu::TestCaseGroup*	createPipelineCacheTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(testCtx, "pipeline_cache", "Tests verifying Vulkan SC pipeline cache"));

	const struct
	{
		PipelineCacheTestType						testType;
		const char*									name;
	} tests[] =
	{
		{ PCTT_WRONG_VENDOR_ID,	"incorrect_vendor_id"		},
		{ PCTT_WRONG_DEVICE_ID,	"incorrect_device_id"		},
	};

	for (int testIdx = 0; testIdx < DE_LENGTH_OF_ARRAY(tests); ++testIdx)
	{
		TestParams params{ tests[testIdx].testType };
		addFunctionCaseWithPrograms(group.get(), tests[testIdx].name, "", createShaders, createPipelineCacheTest, params);
	}

	return group.release();
}

}	// sc

}	// vkt
