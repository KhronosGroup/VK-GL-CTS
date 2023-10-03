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
 * \brief  Vulkan SC API Tests
*//*--------------------------------------------------------------------*/

#include "vktSafetyCriticalApiTests.hpp"

#include <set>
#include <vector>
#include <string>

#include "vkDefs.hpp"
#include "vktTestCaseUtil.hpp"
#include "vkQueryUtil.hpp"

namespace vkt
{
namespace sc
{
namespace
{

tcu::TestStatus forbiddenCoreCommands (Context& context)
{
	std::vector<std::string> commands
	{
		"vkTrimCommandPool",
		"vkTrimCommandPoolKHR",
		"vkDestroyCommandPool",
		"vkCreateShaderModule",
		"vkDestroyShaderModule",
		"vkMergePipelineCaches",
		"vkGetPipelineCacheData",
		"vkFreeMemory",
		"vkDestroyDescriptorPool",
		"vkCreateDescriptorUpdateTemplateKHR",
		"vkDestroyDescriptorUpdateTemplateKHR",
		"vkUpdateDescriptorSetWithTemplateKHR",
		"vkCmdPushDescriptorSetWithTemplateKHR",
		"vkDestroyQueryPool",
		"vkGetPhysicalDeviceSparseImageFormatProperties",
		"vkGetPhysicalDeviceSparseImageFormatProperties2",
		"vkGetPhysicalDeviceSparseImageFormatProperties2KHR",
		"vkGetImageSparseMemoryRequirements",
		"vkGetImageSparseMemoryRequirements2",
		"vkGetImageSparseMemoryRequirements2KHR",
		"vkQueueBindSparse",
		"vkDestroySwapchainKHR"
	};

	vk::VkDevice device				= context.getDevice();
	const vk::DeviceInterface& vkd	= context.getDeviceInterface();

	for (const auto& commandName : commands)
	{
		void* commandPointer = (void *)vkd.getDeviceProcAddr(device, commandName.c_str());
		if( commandPointer != DE_NULL )
			TCU_THROW(TestError, commandName + std::string(" should not be accessible"));
	}

	return tcu::TestStatus::pass("All forbidden commands are not accessible");
}

tcu::TestStatus forbiddenCoreExtensions (Context& context)
{
	// vector of Vulkan 1.1 and Vulkan 1.2 extensions that should not be advertised in Vulkan SC 1.0
	std::set<std::string> coreExtensions
	{
		"VK_KHR_16bit_storage",
		"VK_KHR_bind_memory2",
		"VK_KHR_dedicated_allocation",
		"VK_KHR_descriptor_update_template",
		"VK_KHR_device_group",
		"VK_KHR_device_group_creation",
		"VK_KHR_external_fence",
		"VK_KHR_external_fence_capabilities",
		"VK_KHR_external_memory",
		"VK_KHR_external_memory_capabilities",
		"VK_KHR_external_semaphore",
		"VK_KHR_external_semaphore_capabilities",
		"VK_KHR_get_memory_requirements2",
		"VK_KHR_get_physical_device_properties2",
		"VK_KHR_maintenance1",
		"VK_KHR_maintenance2",
		"VK_KHR_maintenance3",
		"VK_KHR_multiview",
		"VK_KHR_relaxed_block_layout",
		"VK_KHR_sampler_ycbcr_conversion",
		"VK_KHR_shader_draw_parameters",
		"VK_KHR_storage_buffer_storage_class",
		"VK_KHR_variable_pointers",
		"VK_KHR_8bit_storage",
		"VK_KHR_buffer_device_address",
		"VK_KHR_create_renderpass2",
		"VK_KHR_depth_stencil_resolve",
		"VK_KHR_draw_indirect_count",
		"VK_KHR_driver_properties",
		"VK_KHR_image_format_list",
		"VK_KHR_imageless_framebuffer",
		"VK_KHR_sampler_mirror_clamp_to_edge",
		"VK_KHR_separate_depth_stencil_layouts",
		"VK_KHR_shader_atomic_int64",
		"VK_KHR_shader_float16_int8",
		"VK_KHR_shader_float_controls",
		"VK_KHR_shader_subgroup_extended_types",
		"VK_KHR_spirv_1_4",
		"VK_KHR_timeline_semaphore",
		"VK_KHR_uniform_buffer_standard_layout",
		"VK_KHR_vulkan_memory_model",
		"VK_EXT_descriptor_indexing",
		"VK_EXT_host_query_reset",
		"VK_EXT_sampler_filter_minmax",
		"VK_EXT_scalar_block_layout",
		"VK_EXT_separate_stencil_usage",
		"VK_EXT_shader_viewport_index_layer",
	};

	vk::VkPhysicalDevice							physicalDevice		= context.getPhysicalDevice();
	const vk::InstanceInterface&					vki					= context.getInstanceInterface();
	const std::vector<vk::VkExtensionProperties>&	deviceExtensions	= vk::enumerateCachedDeviceExtensionProperties(vki, physicalDevice);

	for (const auto& extension : deviceExtensions)
	{
		const std::string extensionName(extension.extensionName);
		if(coreExtensions.find(extensionName)!= coreExtensions.end())
			TCU_THROW(TestError, extensionName + std::string(" extension is explicitly forbidden"));
	}

	return tcu::TestStatus::pass("No extensions from forbidden set");
}

tcu::TestStatus forbiddenPromotedCommands (Context& context)
{
	std::vector<std::string> commands
	{
		"vkBindBufferMemory2KHR",
		"vkBindImageMemory2KHR",
		"vkCreateDescriptorUpdateTemplateKHR",
		"vkDestroyDescriptorUpdateTemplateKHR",
		"vkUpdateDescriptorSetWithTemplateKHR",
		"vkCmdPushDescriptorSetWithTemplateKHR",
		"vkCmdDispatchBaseKHR",
		"vkCmdSetDeviceMaskKHR",
		"vkGetDeviceGroupPeerMemoryFeaturesKHR",
		"vkEnumeratePhysicalDeviceGroupsKHR",
		"vkGetPhysicalDeviceExternalFencePropertiesKHR",
		"vkGetPhysicalDeviceExternalBufferPropertiesKHR",
		"vkGetPhysicalDeviceExternalSemaphorePropertiesKHR",
		"vkGetBufferMemoryRequirements2KHR",
		"vkGetImageMemoryRequirements2KHR",
		"vkGetImageSparseMemoryRequirements2KHR",
		"vkGetPhysicalDeviceFeatures2KHR",
		"vkGetPhysicalDeviceFormatProperties2KHR",
		"vkGetPhysicalDeviceImageFormatProperties2KHR",
		"vkGetPhysicalDeviceMemoryProperties2KHR",
		"vkGetPhysicalDeviceProperties2KHR",
		"vkGetPhysicalDeviceQueueFamilyProperties2KHR",
		"vkGetPhysicalDeviceSparseImageFormatProperties2KHR",
		"vkTrimCommandPoolKHR",
		"vkGetDescriptorSetLayoutSupportKHR",
		"vkCreateSamplerYcbcrConversionKHR",
		"vkDestroySamplerYcbcrConversionKHR",
		"vkGetBufferDeviceAddressKHR",
		"vkGetBufferOpaqueCaptureAddressKHR",
		"vkGetDeviceMemoryOpaqueCaptureAddressKHR",
		"vkCmdBeginRenderPass2KHR",
		"vkCmdEndRenderPass2KHR",
		"vkCmdNextSubpass2KHR",
		"vkCreateRenderPass2KHR",
		"vkCmdDrawIndexedIndirectCountKHR",
		"vkCmdDrawIndirectCountKHR",
		"vkGetSemaphoreCounterValueKHR",
		"vkSignalSemaphoreKHR",
		"vkWaitSemaphoresKHR",
		"vkResetQueryPoolEXT",
	};

	vk::VkDevice device				= context.getDevice();
	const vk::DeviceInterface& vkd	= context.getDeviceInterface();

	for (const auto& commandName : commands)
	{
		void* commandPointer = (void *)vkd.getDeviceProcAddr(device, commandName.c_str());
		if( commandPointer != DE_NULL )
			TCU_THROW(TestError, commandName + std::string(" should not be accessible"));
	}

	return tcu::TestStatus::pass("All forbidden commands are not accessible");
}

tcu::TestStatus forbiddenDeviceFeatures (Context& context)
{
	if (context.getDeviceFeatures().shaderResourceResidency != VK_FALSE)
		TCU_THROW(TestError, "VkPhysicalDeviceFeatures::shaderResourceResidency must be VK_FALSE");
	if (context.getDeviceFeatures().sparseBinding != VK_FALSE)
		TCU_THROW(TestError, "VkPhysicalDeviceFeatures::sparseBinding must be VK_FALSE");

	if (context.getDeviceFeatures().sparseResidencyBuffer != VK_FALSE)
		TCU_THROW(TestError, "VkPhysicalDeviceFeatures::sparseResidencyBuffer must be VK_FALSE");
	if (context.getDeviceFeatures().sparseResidencyImage2D != VK_FALSE)
		TCU_THROW(TestError, "VkPhysicalDeviceFeatures::sparseResidencyImage2D must be VK_FALSE");
	if (context.getDeviceFeatures().sparseResidencyImage3D != VK_FALSE)
		TCU_THROW(TestError, "VkPhysicalDeviceFeatures::sparseResidencyImage3D must be VK_FALSE");
	if (context.getDeviceFeatures().sparseResidency2Samples != VK_FALSE)
		TCU_THROW(TestError, "VkPhysicalDeviceFeatures::sparseResidency2Samples must be VK_FALSE");
	if (context.getDeviceFeatures().sparseResidency4Samples != VK_FALSE)
		TCU_THROW(TestError, "VkPhysicalDeviceFeatures::sparseResidency4Samples must be VK_FALSE");
	if (context.getDeviceFeatures().sparseResidency8Samples != VK_FALSE)
		TCU_THROW(TestError, "VkPhysicalDeviceFeatures::sparseResidency8Samples must be VK_FALSE");
	if (context.getDeviceFeatures().sparseResidency16Samples != VK_FALSE)
		TCU_THROW(TestError, "VkPhysicalDeviceFeatures::sparseResidency16Samples must be VK_FALSE");
	if (context.getDeviceFeatures().sparseResidencyAliased != VK_FALSE)
		TCU_THROW(TestError, "VkPhysicalDeviceFeatures::sparseResidencyAliased must be VK_FALSE");

	return tcu::TestStatus::pass("Pass");
}

tcu::TestStatus forbiddenDeviceProperties (Context& context)
{
	if (context.getDeviceProperties().sparseProperties.residencyStandard2DBlockShape != VK_FALSE)
		TCU_THROW(TestError, "VkPhysicalDeviceSparseProperties::residencyStandard2DBlockShape must be VK_FALSE");
	if (context.getDeviceProperties().sparseProperties.residencyStandard2DMultisampleBlockShape != VK_FALSE)
		TCU_THROW(TestError, "VkPhysicalDeviceSparseProperties::residencyStandard2DMultisampleBlockShape must be VK_FALSE");
	if (context.getDeviceProperties().sparseProperties.residencyStandard3DBlockShape != VK_FALSE)
		TCU_THROW(TestError, "VkPhysicalDeviceSparseProperties::residencyStandard3DBlockShape must be VK_FALSE");
	if (context.getDeviceProperties().sparseProperties.residencyAlignedMipSize != VK_FALSE)
		TCU_THROW(TestError, "VkPhysicalDeviceSparseProperties::residencyAlignedMipSize must be VK_FALSE");
	if (context.getDeviceProperties().sparseProperties.residencyNonResidentStrict != VK_FALSE)
		TCU_THROW(TestError, "VkPhysicalDeviceSparseProperties::residencyNonResidentStrict must be VK_FALSE");

	return tcu::TestStatus::pass("Pass");
}

tcu::TestStatus allowedExtensions (Context& context)
{
	// vector of extensions that are explicitly allowed in Vulkan SC 1.0
	std::set<std::string> extensions
	{
		"VK_KHR_copy_commands2",
		"VK_KHR_display",
		"VK_KHR_display_swapchain",
		"VK_KHR_external_fence_fd",
		"VK_KHR_external_memory_fd",
		"VK_KHR_external_semaphore_fd",
		"VK_KHR_fragment_shading_rate",
		"VK_KHR_get_display_properties2",
		"VK_KHR_get_surface_capabilities2",
		"VK_KHR_incremental_present",
		"VK_KHR_object_refresh",
		"VK_KHR_shader_clock",
		"VK_KHR_shader_terminate_invocation",
		"VK_KHR_shared_presentable_image",
		"VK_KHR_surface",
		"VK_KHR_swapchain",
		"VK_KHR_swapchain_mutable_format",
		"VK_KHR_synchronization2",
		"VK_EXT_4444_formats",
		"VK_EXT_astc_decode_mode",
		"VK_EXT_blend_operation_advanced",
		"VK_EXT_calibrated_timestamps",
		"VK_EXT_color_write_enable",
		"VK_EXT_conservative_rasterization",
		"VK_EXT_custom_border_color",
		"VK_EXT_debug_utils",
		"VK_EXT_depth_clip_enable",
		"VK_EXT_depth_range_unrestricted",
		"VK_EXT_direct_mode_display",
		"VK_EXT_discard_rectangles",
		"VK_EXT_display_control",
		"VK_EXT_display_surface_counter",
		"VK_EXT_extended_dynamic_state",
		"VK_EXT_extended_dynamic_state2",
		"VK_EXT_external_memory_dma_buf",
		"VK_EXT_external_memory_host",
		"VK_EXT_filter_cubic",
		"VK_EXT_fragment_shader_interlock",
		"VK_EXT_global_priority",
		"VK_EXT_hdr_metadata",
		"VK_EXT_headless_surface",
		"VK_EXT_image_drm_format_modifier",
		"VK_EXT_image_robustness",
		"VK_EXT_index_type_uint8",
		"VK_EXT_line_rasterization",
		"VK_EXT_memory_budget",
		"VK_EXT_pci_bus_info",
		"VK_EXT_post_depth_coverage",
		"VK_EXT_queue_family_foreign",
		"VK_EXT_robustness2",
		"VK_EXT_sample_locations",
		"VK_EXT_shader_atomic_float",
		"VK_EXT_shader_demote_to_helper_invocation",
		"VK_EXT_shader_image_atomic_int64",
		"VK_EXT_shader_stencil_export",
		"VK_EXT_subgroup_size_control",
		"VK_EXT_swapchain_colorspace",
		"VK_EXT_texel_buffer_alignment",
		"VK_EXT_texture_compression_astc_hdr",
		"VK_EXT_validation_features",
		"VK_EXT_vertex_attribute_divisor",
		"VK_EXT_vertex_input_dynamic_state",
		"VK_EXT_ycbcr_2plane_444_formats",
		"VK_EXT_ycbcr_image_arrays",
	};

	vk::VkPhysicalDevice							physicalDevice		= context.getPhysicalDevice();
	const vk::InstanceInterface&					vki					= context.getInstanceInterface();
	const std::vector<vk::VkExtensionProperties>&	deviceExtensions	= vk::enumerateCachedDeviceExtensionProperties(vki, physicalDevice);

	for (const auto& extension : deviceExtensions)
	{
		const std::string extensionName(extension.extensionName);

		// this test applies only to VK_KHR_* and VK_EXT_* extensions
		if (extensionName.find("VK_KHR") != 0 && extensionName.find("VK_EXT") != 0)
			continue;

		if(extensions.find(extensionName)==extensions.end())
			TCU_THROW(TestError, extensionName + std::string(" extension is not allowed"));
	}

	return tcu::TestStatus::pass("All implemented extensions are defined in specification");
}

} // anonymous

tcu::TestCaseGroup*	createSafetyCriticalAPITests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(testCtx, "api", "Tests verifying Vulkan SC specific API"));

	addFunctionCase(group.get(), "forbidden_core_commands",		"Verify existence of functions removed from Vulkan",				forbiddenCoreCommands);
	addFunctionCase(group.get(), "forbidden_core_extensions",	"Verify existence of extensions removed from Vulkan",				forbiddenCoreExtensions);
	addFunctionCase(group.get(), "forbidden_promoted_commands",	"Verify existence of promoted functions removed from Vulkan",		forbiddenPromotedCommands);
	addFunctionCase(group.get(), "forbidden_features",			"Verify if specific device features are forbidden for Vulkan SC",	forbiddenDeviceFeatures);
	addFunctionCase(group.get(), "forbidden_properties",		"Verify if specific device properties are forbidden for Vulkan SC", forbiddenDeviceProperties);
	addFunctionCase(group.get(), "allowed_extensions",			"Verify if extensions are allowed for Vulkan SC",					allowedExtensions);

	return group.release();
}

}	// sc

}	// vkt
