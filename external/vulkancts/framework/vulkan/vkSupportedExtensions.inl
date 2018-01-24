/* WARNING: This is auto-generated file. Do not modify, since changes will
 * be lost! Modify the generating script instead.
 */

void getCoreDeviceExtensionsImpl (deUint32 coreVersion, ::std::vector<const char*>& dst)
{
	if (coreVersion >= VK_API_VERSION_1_1)
	{
		dst.push_back("VK_KHR_multiview");
		dst.push_back("VK_KHR_device_group");
		dst.push_back("VK_KHR_shader_draw_parameters");
		dst.push_back("VK_KHR_maintenance1");
		dst.push_back("VK_KHR_external_memory");
		dst.push_back("VK_KHR_external_semaphore");
		dst.push_back("VK_KHR_16bit_storage");
		dst.push_back("VK_KHR_descriptor_update_template");
		dst.push_back("VK_KHR_external_fence");
		dst.push_back("VK_KHR_maintenance2");
		dst.push_back("VK_KHR_variable_pointers");
		dst.push_back("VK_KHR_dedicated_allocation");
		dst.push_back("VK_KHR_storage_buffer_storage_class");
		dst.push_back("VK_KHR_relaxed_block_layout");
		dst.push_back("VK_KHR_get_memory_requirements2");
		dst.push_back("VK_KHR_sampler_ycbcr_conversion");
		dst.push_back("VK_KHR_bind_memory2");
		dst.push_back("VK_KHR_maintenance3");
	}
}

void getCoreInstanceExtensionsImpl (deUint32 coreVersion, ::std::vector<const char*>& dst)
{
	if (coreVersion >= VK_API_VERSION_1_1)
	{
		dst.push_back("VK_KHR_get_physical_device_properties2");
		dst.push_back("VK_KHR_device_group_creation");
		dst.push_back("VK_KHR_external_memory_capabilities");
		dst.push_back("VK_KHR_external_semaphore_capabilities");
		dst.push_back("VK_KHR_external_fence_capabilities");
	}
}

