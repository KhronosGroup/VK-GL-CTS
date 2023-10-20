/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
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
 * \file
 * \brief Shader Object API Tests
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"
#include "tcuTestCase.hpp"
#include "deUniquePtr.hpp"
#include "tcuTestCase.hpp"
#include "vktTestCase.hpp"
#include "vkQueryUtil.hpp"
#include "vktCustomInstancesDevices.hpp"
#include "tcuCommandLine.hpp"

namespace vkt
{
namespace ShaderObject
{

namespace
{

enum ShaderObjectApiTest {
	EXT_DISCARD_RECTANGLES = 0,
	NV_SCISSOR_EXCLUSIVE,
	KHR_DYNAMIC_RENDERING,
	SHADER_BINARY_UUID,
};

class ShaderObjectApiInstance : public vkt::TestInstance
{
public:
						ShaderObjectApiInstance		(Context& context)
							: vkt::TestInstance (context)
							{}
	virtual				~ShaderObjectApiInstance	(void) {}

	tcu::TestStatus		iterate						(void) override;
};

tcu::TestStatus ShaderObjectApiInstance::iterate (void)
{
	const vk::InstanceInterface&			vki				= m_context.getInstanceInterface();
	const vk::PlatformInterface&			vkp				= m_context.getPlatformInterface();
	const auto								instance		= m_context.getInstance();
	const auto								physicalDevice	= m_context.getPhysicalDevice();
	vk::VkPhysicalDeviceFeatures2			deviceFeatures	= vk::initVulkanStructure();

	vki.getPhysicalDeviceFeatures2(physicalDevice, &deviceFeatures);

	const auto								queuePriority	= 1.0f;
	const vk::VkDeviceQueueCreateInfo		queueInfo
	{
		vk::VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,		//	VkStructureType					sType;
		nullptr,											//	const void*						pNext;
		0u,													//	VkDeviceQueueCreateFlags		flags;
		m_context.getUniversalQueueFamilyIndex(),			//	deUint32						queueFamilyIndex;
		1u,													//	deUint32						queueCount;
		&queuePriority,										//	const float*					pQueuePriorities;
	};

	std::vector<const char*>				extensions		= { "VK_EXT_shader_object" };

	vk::VkPhysicalDeviceShaderObjectFeaturesEXT shaderObjectFeaturesEXT	= vk::initVulkanStructure();
	vk::VkPhysicalDeviceFeatures2				features2				= vk::initVulkanStructure(&shaderObjectFeaturesEXT);
	vki.getPhysicalDeviceFeatures2(physicalDevice, &features2);

	const vk::VkDeviceCreateInfo			deviceInfo
	{
		vk::VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,			//	VkStructureType					sType;
		&features2,											//	const void*						pNext;
		0u,													//	VkDeviceCreateFlags				flags;
		1u,													//	deUint32						queueCreateInfoCount;
		&queueInfo,											//	const VkDeviceQueueCreateInfo*	pQueueCreateInfos;
		0u,													//	deUint32						enabledLayerCount;
		nullptr,											//	const char* const*				ppEnabledLayerNames;
		(deUint32)extensions.size(),						//	deUint32						enabledExtensionCount;
		extensions.data(),									//	const char* const*				ppEnabledExtensionNames;
		DE_NULL,											//	const VkPhysicalDeviceFeatures*	pEnabledFeatures;
	};

	const auto								device			= createCustomDevice(m_context.getTestContext().getCommandLine().isValidationEnabled(), vkp, instance, vki, physicalDevice, &deviceInfo);

	de::MovePtr<vk::DeviceDriver>			vkd				(new vk::DeviceDriver(vkp, instance, device.get(), m_context.getUsedApiVersion()));
	const auto&								vk				= *vkd.get();

	const std::vector<std::string> functions = {
		// VK_EXT_extended_dynamic_state
		"vkCmdBindVertexBuffers2EXT",
		"vkCmdSetCullModeEXT",
		"vkCmdSetDepthBoundsTestEnableEXT",
		"vkCmdSetDepthCompareOpEXT",
		"vkCmdSetDepthTestEnableEXT",
		"vkCmdSetDepthWriteEnableEXT",
		"vkCmdSetFrontFaceEXT",
		"vkCmdSetPrimitiveTopologyEXT",
		"vkCmdSetScissorWithCountEXT",
		"vkCmdSetStencilOpEXT",
		"vkCmdSetStencilTestEnableEXT",
		"vkCmdSetViewportWithCountEXT",
		// VK_EXT_extended_dynamic_state2
		"vkCmdSetDepthBiasEnableEXT",
		"vkCmdSetLogicOpEXT",
		"vkCmdSetPatchControlPointsEXT",
		"vkCmdSetPrimitiveRestartEnableEXT",
		"vkCmdSetRasterizerDiscardEnableEXT",
		// VK_EXT_extended_dynamic_state3
		"vkCmdSetAlphaToCoverageEnableEXT",
		"vkCmdSetAlphaToOneEnableEXT",
		"vkCmdSetColorBlendAdvancedEXT",
		"vkCmdSetColorBlendEnableEXT",
		"vkCmdSetColorBlendEquationEXT",
		"vkCmdSetColorWriteMaskEXT",
		"vkCmdSetConservativeRasterizationModeEXT",
		"vkCmdSetCoverageModulationModeNV",
		"vkCmdSetCoverageModulationTableEnableNV",
		"vkCmdSetCoverageModulationTableNV",
		"vkCmdSetCoverageReductionModeNV",
		"vkCmdSetCoverageToColorEnableNV",
		"vkCmdSetCoverageToColorLocationNV",
		"vkCmdSetDepthClampEnableEXT",
		"vkCmdSetDepthClipEnableEXT",
		"vkCmdSetDepthClipNegativeOneToOneEXT",
		"vkCmdSetExtraPrimitiveOverestimationSizeEXT",
		"vkCmdSetLineRasterizationModeEXT",
		"vkCmdSetLineStippleEnableEXT",
		"vkCmdSetLogicOpEnableEXT",
		"vkCmdSetPolygonModeEXT",
		"vkCmdSetProvokingVertexModeEXT",
		"vkCmdSetRasterizationSamplesEXT",
		"vkCmdSetRasterizationStreamEXT",
		"vkCmdSetRepresentativeFragmentTestEnableNV",
		"vkCmdSetSampleLocationsEnableEXT",
		"vkCmdSetSampleMaskEXT",
		"vkCmdSetShadingRateImageEnableNV",
		"vkCmdSetTessellationDomainOriginEXT",
		"vkCmdSetViewportSwizzleNV",
		"vkCmdSetViewportWScalingEnableNV",
		// VK_EXT_vertex_input_dynamic_state
		"vkCmdSetVertexInputEXT",
	};

	for (const auto& func : functions)
	{
		const auto& f = vk.getDeviceProcAddr(*device, func.c_str());
		if (f == DE_NULL)
			return tcu::TestStatus::fail("Failed: " + func);
	}

	return tcu::TestStatus::pass("Pass");
}

class ShaderObjectApiCase : public vkt::TestCase
{
public:
					ShaderObjectApiCase		(tcu::TestContext& testCtx, const std::string& name, const std::string& description)
						: vkt::TestCase		(testCtx, name, description)
						{}
	virtual			~ShaderObjectApiCase	(void) {}

	void			checkSupport			(vkt::Context& context) const override;
	TestInstance*	createInstance			(Context& context) const override { return new ShaderObjectApiInstance(context); }
};

void ShaderObjectApiCase::checkSupport (Context& context) const
{
	context.requireDeviceFunctionality("VK_EXT_shader_object");
}

class ShaderObjectExtensionVersionInstance : public vkt::TestInstance
{
public:
						ShaderObjectExtensionVersionInstance	(Context& context, const ShaderObjectApiTest test)
							: vkt::TestInstance	(context)
							, m_test			(test)
							{}
	virtual				~ShaderObjectExtensionVersionInstance	(void) {}

	tcu::TestStatus		iterate									(void) override;
private:
	ShaderObjectApiTest m_test;
};

tcu::TestStatus ShaderObjectExtensionVersionInstance::iterate (void)
{
	vk::VkInstance									instance		= m_context.getInstance();
	vk::InstanceDriver								instanceDriver	(m_context.getPlatformInterface(), instance);
	vk::VkPhysicalDevice							physicalDevice  = m_context.getPhysicalDevice();
	const vk::InstanceInterface&					vki				= m_context.getInstanceInterface();
	const vk::ApiVersion							deviceVersion	= vk::unpackVersion(m_context.getDeviceVersion());
	tcu::TestLog&									log				= m_context.getTestContext().getLog();

	vk::VkPhysicalDeviceShaderObjectPropertiesEXT	shaderObjectProperties = vk::initVulkanStructure();

	vk::VkPhysicalDeviceProperties					properties;
	deMemset(&properties, 0, sizeof(vk::VkPhysicalDeviceProperties));
	vk::VkPhysicalDeviceProperties2					properties2 =
	{
		vk::VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,			// VkStructureType					sType
		&shaderObjectProperties,									// const void*						pNext
		properties													// VkPhysicalDeviceProperties		properties
	};

	instanceDriver.getPhysicalDeviceProperties2(physicalDevice, &properties2);

	const std::vector<vk::VkExtensionProperties>&				deviceExtensionProperties = enumerateCachedDeviceExtensionProperties(vki, physicalDevice);

	if (m_test == SHADER_BINARY_UUID)
	{
		bool nonZero = false;
		for (deUint32 i = 0; i < VK_UUID_SIZE; ++i)
		{
			if (shaderObjectProperties.shaderBinaryUUID[i] != 0)
			{
				nonZero = true;
				break;
			}
		}
		if (!nonZero)
		{
			log << tcu::TestLog::Message << "All shaderBinaryUUID bytes are 0" << tcu::TestLog::EndMessage;
			return tcu::TestStatus::fail("Fail");
		}
	}
	else if (m_test == KHR_DYNAMIC_RENDERING)
	{
		if (deviceVersion.majorNum == 1 && deviceVersion.minorNum < 3)
		{
			bool found = false;
			for (const auto& ext : deviceExtensionProperties)
			{
				if (strcmp(ext.extensionName, "VK_KHR_dynamic_rendering") == 0)
				{
					found = true;
					break;
				}
			}
			if (!found)
			{
				log << tcu::TestLog::Message << "VK_EXT_shader_object is supported, but vulkan version is < 1.3 and VK_KHR_dynamic_rendering is not supported" << tcu::TestLog::EndMessage;
				return tcu::TestStatus::fail("Fail");
			}
		}
	}
	else
	{
		for (const auto& ext : deviceExtensionProperties)
		{
			if (m_test == EXT_DISCARD_RECTANGLES)
			{
				if (strcmp(ext.extensionName, "VK_EXT_discard_rectangles") == 0)
				{
					if (ext.specVersion < 2)
					{
						log << tcu::TestLog::Message << "VK_EXT_shader_object and VK_EXT_discard_rectangles are supported, but VK_EXT_discard_rectangles reports version " << ext.specVersion << tcu::TestLog::EndMessage;
						return tcu::TestStatus::fail("Fail");
					}
					break;
				}
			}
			else if (m_test == NV_SCISSOR_EXCLUSIVE)
			{
				if (strcmp(ext.extensionName, "VK_NV_scissor_exclusive") == 0)
				{
					if (ext.specVersion < 2)
					{
						log << tcu::TestLog::Message << "VK_EXT_shader_object and VK_NV_scissor_exclusive are supported, but VK_NV_scissor_exclusive reports version " << ext.specVersion << tcu::TestLog::EndMessage;
						return tcu::TestStatus::fail("Fail");
					}
					break;
				}
			}
		}
	}
	return tcu::TestStatus::pass("Pass");
}

class ShaderObjectExtensionVersionCase : public vkt::TestCase
{
public:
					ShaderObjectExtensionVersionCase	(tcu::TestContext& testCtx, const std::string& name, const std::string& description, const ShaderObjectApiTest test)
						: vkt::TestCase					(testCtx, name, description)
						, m_test						(test)
						{}
	virtual			~ShaderObjectExtensionVersionCase	(void) {}

	void			checkSupport						(vkt::Context& context) const override;
	TestInstance*	createInstance						(Context& context) const override { return new ShaderObjectExtensionVersionInstance(context, m_test); }
private:
	ShaderObjectApiTest m_test;
};

void ShaderObjectExtensionVersionCase::checkSupport(Context& context) const
{
	context.requireDeviceFunctionality("VK_EXT_shader_object");
	if (m_test == EXT_DISCARD_RECTANGLES)
	{
		context.requireDeviceFunctionality("VK_EXT_discard_rectangles");
	}
	else if (m_test == NV_SCISSOR_EXCLUSIVE)
	{
		context.requireDeviceFunctionality("VK_NV_scissor_exclusive");
	}
}
}

tcu::TestCaseGroup* createShaderObjectApiTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> apiGroup(new tcu::TestCaseGroup(testCtx, "api", ""));
	apiGroup->addChild(new ShaderObjectApiCase(testCtx, "get_device_proc_addr", "Test vkGetDeviceProcAddr"));

	const struct
	{
		ShaderObjectApiTest		test;
		const char*				name;
	} apiTests[] =
	{
		{ EXT_DISCARD_RECTANGLES,	"discard_rectangles"	},
		{ NV_SCISSOR_EXCLUSIVE,		"scissor_exclusive"		},
		{ KHR_DYNAMIC_RENDERING,	"dynamic_rendering"		},
		{ SHADER_BINARY_UUID,		"shader_binary_uuid"	},
	};

	for (const auto& test : apiTests)
	{
		apiGroup->addChild(new ShaderObjectExtensionVersionCase(testCtx, test.name, "", test.test));
	}
	return apiGroup.release();
}

} // ShaderObject
} // vkt

