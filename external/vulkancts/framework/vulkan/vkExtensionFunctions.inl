/* WARNING: This is auto-generated file. Do not modify, since changes will
 * be lost! Modify the generating script instead.
 */

void getInstanceExtensionFunctions (deUint32 apiVersion, ::std::string extName, ::std::vector<const char*>& functions)
{
	if (extName == "VK_KHR_surface")
	{
		functions.push_back("vkDestroySurfaceKHR");
		functions.push_back("vkGetPhysicalDeviceSurfaceSupportKHR");
		functions.push_back("vkGetPhysicalDeviceSurfaceCapabilitiesKHR");
		functions.push_back("vkGetPhysicalDeviceSurfaceFormatsKHR");
		functions.push_back("vkGetPhysicalDeviceSurfacePresentModesKHR");
		return;
	}
	if (extName == "VK_KHR_swapchain")
	{
		if(apiVersion >= VK_API_VERSION_1_1) functions.push_back("vkGetPhysicalDevicePresentRectanglesKHR");
		return;
	}
	if (extName == "VK_KHR_display")
	{
		functions.push_back("vkGetPhysicalDeviceDisplayPropertiesKHR");
		functions.push_back("vkGetPhysicalDeviceDisplayPlanePropertiesKHR");
		functions.push_back("vkGetDisplayPlaneSupportedDisplaysKHR");
		functions.push_back("vkGetDisplayModePropertiesKHR");
		functions.push_back("vkCreateDisplayModeKHR");
		functions.push_back("vkGetDisplayPlaneCapabilitiesKHR");
		functions.push_back("vkCreateDisplayPlaneSurfaceKHR");
		return;
	}
	if (extName == "VK_KHR_display_swapchain")
	{
		return;
	}
	if (extName == "VK_KHR_sampler_mirror_clamp_to_edge")
	{
		return;
	}
	if (extName == "VK_KHR_multiview")
	{
		return;
	}
	if (extName == "VK_KHR_get_physical_device_properties2")
	{
		functions.push_back("vkGetPhysicalDeviceFeatures2KHR");
		functions.push_back("vkGetPhysicalDeviceProperties2KHR");
		functions.push_back("vkGetPhysicalDeviceFormatProperties2KHR");
		functions.push_back("vkGetPhysicalDeviceImageFormatProperties2KHR");
		functions.push_back("vkGetPhysicalDeviceQueueFamilyProperties2KHR");
		functions.push_back("vkGetPhysicalDeviceMemoryProperties2KHR");
		functions.push_back("vkGetPhysicalDeviceSparseImageFormatProperties2KHR");
		return;
	}
	if (extName == "VK_KHR_device_group")
	{
		if(apiVersion < VK_API_VERSION_1_1) functions.push_back("vkGetPhysicalDevicePresentRectanglesKHR");
		return;
	}
	if (extName == "VK_KHR_shader_draw_parameters")
	{
		return;
	}
	if (extName == "VK_KHR_maintenance1")
	{
		return;
	}
	if (extName == "VK_KHR_device_group_creation")
	{
		functions.push_back("vkEnumeratePhysicalDeviceGroupsKHR");
		return;
	}
	if (extName == "VK_KHR_external_memory_capabilities")
	{
		functions.push_back("vkGetPhysicalDeviceExternalBufferPropertiesKHR");
		return;
	}
	if (extName == "VK_KHR_external_memory")
	{
		return;
	}
	if (extName == "VK_KHR_external_memory_fd")
	{
		return;
	}
	if (extName == "VK_KHR_external_semaphore_capabilities")
	{
		functions.push_back("vkGetPhysicalDeviceExternalSemaphorePropertiesKHR");
		return;
	}
	if (extName == "VK_KHR_external_semaphore")
	{
		return;
	}
	if (extName == "VK_KHR_external_semaphore_fd")
	{
		return;
	}
	if (extName == "VK_KHR_push_descriptor")
	{
		return;
	}
	if (extName == "VK_KHR_shader_float16_int8")
	{
		return;
	}
	if (extName == "VK_KHR_16bit_storage")
	{
		return;
	}
	if (extName == "VK_KHR_incremental_present")
	{
		return;
	}
	if (extName == "VK_KHR_descriptor_update_template")
	{
		return;
	}
	if (extName == "VK_KHR_imageless_framebuffer")
	{
		return;
	}
	if (extName == "VK_KHR_create_renderpass2")
	{
		return;
	}
	if (extName == "VK_KHR_shared_presentable_image")
	{
		return;
	}
	if (extName == "VK_KHR_external_fence_capabilities")
	{
		functions.push_back("vkGetPhysicalDeviceExternalFencePropertiesKHR");
		return;
	}
	if (extName == "VK_KHR_external_fence")
	{
		return;
	}
	if (extName == "VK_KHR_external_fence_fd")
	{
		return;
	}
	if (extName == "VK_KHR_performance_query")
	{
		functions.push_back("vkEnumeratePhysicalDeviceQueueFamilyPerformanceQueryCountersKHR");
		functions.push_back("vkGetPhysicalDeviceQueueFamilyPerformanceQueryPassesKHR");
		return;
	}
	if (extName == "VK_KHR_maintenance2")
	{
		return;
	}
	if (extName == "VK_KHR_get_surface_capabilities2")
	{
		functions.push_back("vkGetPhysicalDeviceSurfaceCapabilities2KHR");
		functions.push_back("vkGetPhysicalDeviceSurfaceFormats2KHR");
		return;
	}
	if (extName == "VK_KHR_variable_pointers")
	{
		return;
	}
	if (extName == "VK_KHR_get_display_properties2")
	{
		functions.push_back("vkGetPhysicalDeviceDisplayProperties2KHR");
		functions.push_back("vkGetPhysicalDeviceDisplayPlaneProperties2KHR");
		functions.push_back("vkGetDisplayModeProperties2KHR");
		functions.push_back("vkGetDisplayPlaneCapabilities2KHR");
		return;
	}
	if (extName == "VK_KHR_dedicated_allocation")
	{
		return;
	}
	if (extName == "VK_KHR_storage_buffer_storage_class")
	{
		return;
	}
	if (extName == "VK_KHR_relaxed_block_layout")
	{
		return;
	}
	if (extName == "VK_KHR_get_memory_requirements2")
	{
		return;
	}
	if (extName == "VK_KHR_image_format_list")
	{
		return;
	}
	if (extName == "VK_KHR_sampler_ycbcr_conversion")
	{
		return;
	}
	if (extName == "VK_KHR_bind_memory2")
	{
		return;
	}
	if (extName == "VK_KHR_maintenance3")
	{
		return;
	}
	if (extName == "VK_KHR_draw_indirect_count")
	{
		return;
	}
	if (extName == "VK_KHR_shader_subgroup_extended_types")
	{
		return;
	}
	if (extName == "VK_KHR_8bit_storage")
	{
		return;
	}
	if (extName == "VK_KHR_shader_atomic_int64")
	{
		return;
	}
	if (extName == "VK_KHR_shader_clock")
	{
		return;
	}
	if (extName == "VK_KHR_driver_properties")
	{
		return;
	}
	if (extName == "VK_KHR_shader_float_controls")
	{
		return;
	}
	if (extName == "VK_KHR_depth_stencil_resolve")
	{
		return;
	}
	if (extName == "VK_KHR_swapchain_mutable_format")
	{
		return;
	}
	if (extName == "VK_KHR_timeline_semaphore")
	{
		return;
	}
	if (extName == "VK_KHR_vulkan_memory_model")
	{
		return;
	}
	if (extName == "VK_KHR_shader_terminate_invocation")
	{
		return;
	}
	if (extName == "VK_KHR_fragment_shading_rate")
	{
		functions.push_back("vkGetPhysicalDeviceFragmentShadingRatesKHR");
		return;
	}
	if (extName == "VK_KHR_spirv_1_4")
	{
		return;
	}
	if (extName == "VK_KHR_surface_protected_capabilities")
	{
		return;
	}
	if (extName == "VK_KHR_separate_depth_stencil_layouts")
	{
		return;
	}
	if (extName == "VK_KHR_uniform_buffer_standard_layout")
	{
		return;
	}
	if (extName == "VK_KHR_buffer_device_address")
	{
		return;
	}
	if (extName == "VK_KHR_deferred_host_operations")
	{
		return;
	}
	if (extName == "VK_KHR_pipeline_executable_properties")
	{
		return;
	}
	if (extName == "VK_KHR_pipeline_library")
	{
		return;
	}
	if (extName == "VK_KHR_shader_non_semantic_info")
	{
		return;
	}
	if (extName == "VK_KHR_synchronization2")
	{
		return;
	}
	if (extName == "VK_KHR_zero_initialize_workgroup_memory")
	{
		return;
	}
	if (extName == "VK_KHR_workgroup_memory_explicit_layout")
	{
		return;
	}
	if (extName == "VK_KHR_copy_commands2")
	{
		return;
	}
	if (extName == "VK_EXT_debug_report")
	{
		functions.push_back("vkCreateDebugReportCallbackEXT");
		functions.push_back("vkDestroyDebugReportCallbackEXT");
		functions.push_back("vkDebugReportMessageEXT");
		return;
	}
	if (extName == "VK_NV_glsl_shader")
	{
		return;
	}
	if (extName == "VK_EXT_depth_range_unrestricted")
	{
		return;
	}
	if (extName == "VK_IMG_filter_cubic")
	{
		return;
	}
	if (extName == "VK_AMD_rasterization_order")
	{
		return;
	}
	if (extName == "VK_AMD_shader_trinary_minmax")
	{
		return;
	}
	if (extName == "VK_AMD_shader_explicit_vertex_parameter")
	{
		return;
	}
	if (extName == "VK_EXT_debug_marker")
	{
		return;
	}
	if (extName == "VK_AMD_gcn_shader")
	{
		return;
	}
	if (extName == "VK_NV_dedicated_allocation")
	{
		return;
	}
	if (extName == "VK_EXT_transform_feedback")
	{
		return;
	}
	if (extName == "VK_NVX_image_view_handle")
	{
		return;
	}
	if (extName == "VK_AMD_draw_indirect_count")
	{
		return;
	}
	if (extName == "VK_AMD_negative_viewport_height")
	{
		return;
	}
	if (extName == "VK_AMD_gpu_shader_half_float")
	{
		return;
	}
	if (extName == "VK_AMD_shader_ballot")
	{
		return;
	}
	if (extName == "VK_AMD_texture_gather_bias_lod")
	{
		return;
	}
	if (extName == "VK_AMD_shader_info")
	{
		return;
	}
	if (extName == "VK_AMD_shader_image_load_store_lod")
	{
		return;
	}
	if (extName == "VK_NV_corner_sampled_image")
	{
		return;
	}
	if (extName == "VK_IMG_format_pvrtc")
	{
		return;
	}
	if (extName == "VK_NV_external_memory_capabilities")
	{
		functions.push_back("vkGetPhysicalDeviceExternalImageFormatPropertiesNV");
		return;
	}
	if (extName == "VK_NV_external_memory")
	{
		return;
	}
	if (extName == "VK_EXT_validation_flags")
	{
		return;
	}
	if (extName == "VK_EXT_shader_subgroup_ballot")
	{
		return;
	}
	if (extName == "VK_EXT_shader_subgroup_vote")
	{
		return;
	}
	if (extName == "VK_EXT_texture_compression_astc_hdr")
	{
		return;
	}
	if (extName == "VK_EXT_astc_decode_mode")
	{
		return;
	}
	if (extName == "VK_EXT_conditional_rendering")
	{
		return;
	}
	if (extName == "VK_NV_clip_space_w_scaling")
	{
		return;
	}
	if (extName == "VK_EXT_direct_mode_display")
	{
		functions.push_back("vkReleaseDisplayEXT");
		return;
	}
	if (extName == "VK_EXT_display_surface_counter")
	{
		functions.push_back("vkGetPhysicalDeviceSurfaceCapabilities2EXT");
		return;
	}
	if (extName == "VK_EXT_display_control")
	{
		return;
	}
	if (extName == "VK_GOOGLE_display_timing")
	{
		return;
	}
	if (extName == "VK_NV_sample_mask_override_coverage")
	{
		return;
	}
	if (extName == "VK_NV_geometry_shader_passthrough")
	{
		return;
	}
	if (extName == "VK_NV_viewport_array2")
	{
		return;
	}
	if (extName == "VK_NVX_multiview_per_view_attributes")
	{
		return;
	}
	if (extName == "VK_NV_viewport_swizzle")
	{
		return;
	}
	if (extName == "VK_EXT_discard_rectangles")
	{
		return;
	}
	if (extName == "VK_EXT_conservative_rasterization")
	{
		return;
	}
	if (extName == "VK_EXT_depth_clip_enable")
	{
		return;
	}
	if (extName == "VK_EXT_swapchain_colorspace")
	{
		return;
	}
	if (extName == "VK_EXT_hdr_metadata")
	{
		return;
	}
	if (extName == "VK_EXT_external_memory_dma_buf")
	{
		return;
	}
	if (extName == "VK_EXT_queue_family_foreign")
	{
		return;
	}
	if (extName == "VK_EXT_debug_utils")
	{
		functions.push_back("vkCreateDebugUtilsMessengerEXT");
		functions.push_back("vkDestroyDebugUtilsMessengerEXT");
		functions.push_back("vkSubmitDebugUtilsMessageEXT");
		return;
	}
	if (extName == "VK_EXT_sampler_filter_minmax")
	{
		return;
	}
	if (extName == "VK_AMD_gpu_shader_int16")
	{
		return;
	}
	if (extName == "VK_AMD_mixed_attachment_samples")
	{
		return;
	}
	if (extName == "VK_AMD_shader_fragment_mask")
	{
		return;
	}
	if (extName == "VK_EXT_inline_uniform_block")
	{
		return;
	}
	if (extName == "VK_EXT_shader_stencil_export")
	{
		return;
	}
	if (extName == "VK_EXT_sample_locations")
	{
		functions.push_back("vkGetPhysicalDeviceMultisamplePropertiesEXT");
		return;
	}
	if (extName == "VK_EXT_blend_operation_advanced")
	{
		return;
	}
	if (extName == "VK_NV_fragment_coverage_to_color")
	{
		return;
	}
	if (extName == "VK_NV_framebuffer_mixed_samples")
	{
		return;
	}
	if (extName == "VK_NV_fill_rectangle")
	{
		return;
	}
	if (extName == "VK_NV_shader_sm_builtins")
	{
		return;
	}
	if (extName == "VK_EXT_post_depth_coverage")
	{
		return;
	}
	if (extName == "VK_EXT_image_drm_format_modifier")
	{
		return;
	}
	if (extName == "VK_EXT_validation_cache")
	{
		return;
	}
	if (extName == "VK_EXT_descriptor_indexing")
	{
		return;
	}
	if (extName == "VK_EXT_shader_viewport_index_layer")
	{
		return;
	}
	if (extName == "VK_NV_shading_rate_image")
	{
		return;
	}
	if (extName == "VK_NV_ray_tracing")
	{
		return;
	}
	if (extName == "VK_NV_representative_fragment_test")
	{
		return;
	}
	if (extName == "VK_EXT_filter_cubic")
	{
		return;
	}
	if (extName == "VK_QCOM_render_pass_shader_resolve")
	{
		return;
	}
	if (extName == "VK_EXT_global_priority")
	{
		return;
	}
	if (extName == "VK_EXT_external_memory_host")
	{
		return;
	}
	if (extName == "VK_AMD_buffer_marker")
	{
		return;
	}
	if (extName == "VK_AMD_pipeline_compiler_control")
	{
		return;
	}
	if (extName == "VK_EXT_calibrated_timestamps")
	{
		functions.push_back("vkGetPhysicalDeviceCalibrateableTimeDomainsEXT");
		return;
	}
	if (extName == "VK_AMD_shader_core_properties")
	{
		return;
	}
	if (extName == "VK_AMD_memory_overallocation_behavior")
	{
		return;
	}
	if (extName == "VK_EXT_vertex_attribute_divisor")
	{
		return;
	}
	if (extName == "VK_EXT_pipeline_creation_feedback")
	{
		return;
	}
	if (extName == "VK_NV_shader_subgroup_partitioned")
	{
		return;
	}
	if (extName == "VK_NV_compute_shader_derivatives")
	{
		return;
	}
	if (extName == "VK_NV_mesh_shader")
	{
		return;
	}
	if (extName == "VK_NV_fragment_shader_barycentric")
	{
		return;
	}
	if (extName == "VK_NV_shader_image_footprint")
	{
		return;
	}
	if (extName == "VK_NV_scissor_exclusive")
	{
		return;
	}
	if (extName == "VK_NV_device_diagnostic_checkpoints")
	{
		return;
	}
	if (extName == "VK_INTEL_shader_integer_functions2")
	{
		return;
	}
	if (extName == "VK_INTEL_performance_query")
	{
		return;
	}
	if (extName == "VK_EXT_pci_bus_info")
	{
		return;
	}
	if (extName == "VK_AMD_display_native_hdr")
	{
		return;
	}
	if (extName == "VK_EXT_fragment_density_map")
	{
		return;
	}
	if (extName == "VK_EXT_scalar_block_layout")
	{
		return;
	}
	if (extName == "VK_GOOGLE_hlsl_functionality1")
	{
		return;
	}
	if (extName == "VK_GOOGLE_decorate_string")
	{
		return;
	}
	if (extName == "VK_EXT_subgroup_size_control")
	{
		return;
	}
	if (extName == "VK_AMD_shader_core_properties2")
	{
		return;
	}
	if (extName == "VK_AMD_device_coherent_memory")
	{
		return;
	}
	if (extName == "VK_EXT_shader_image_atomic_int64")
	{
		return;
	}
	if (extName == "VK_EXT_memory_budget")
	{
		return;
	}
	if (extName == "VK_EXT_memory_priority")
	{
		return;
	}
	if (extName == "VK_NV_dedicated_allocation_image_aliasing")
	{
		return;
	}
	if (extName == "VK_EXT_buffer_device_address")
	{
		return;
	}
	if (extName == "VK_EXT_tooling_info")
	{
		functions.push_back("vkGetPhysicalDeviceToolPropertiesEXT");
		return;
	}
	if (extName == "VK_EXT_separate_stencil_usage")
	{
		return;
	}
	if (extName == "VK_EXT_validation_features")
	{
		return;
	}
	if (extName == "VK_NV_cooperative_matrix")
	{
		functions.push_back("vkGetPhysicalDeviceCooperativeMatrixPropertiesNV");
		return;
	}
	if (extName == "VK_NV_coverage_reduction_mode")
	{
		functions.push_back("vkGetPhysicalDeviceSupportedFramebufferMixedSamplesCombinationsNV");
		return;
	}
	if (extName == "VK_EXT_fragment_shader_interlock")
	{
		return;
	}
	if (extName == "VK_EXT_ycbcr_image_arrays")
	{
		return;
	}
	if (extName == "VK_EXT_headless_surface")
	{
		functions.push_back("vkCreateHeadlessSurfaceEXT");
		return;
	}
	if (extName == "VK_EXT_line_rasterization")
	{
		return;
	}
	if (extName == "VK_EXT_shader_atomic_float")
	{
		return;
	}
	if (extName == "VK_EXT_host_query_reset")
	{
		return;
	}
	if (extName == "VK_EXT_index_type_uint8")
	{
		return;
	}
	if (extName == "VK_EXT_extended_dynamic_state")
	{
		return;
	}
	if (extName == "VK_EXT_shader_demote_to_helper_invocation")
	{
		return;
	}
	if (extName == "VK_NV_device_generated_commands")
	{
		return;
	}
	if (extName == "VK_EXT_texel_buffer_alignment")
	{
		return;
	}
	if (extName == "VK_QCOM_render_pass_transform")
	{
		return;
	}
	if (extName == "VK_EXT_device_memory_report")
	{
		return;
	}
	if (extName == "VK_EXT_robustness2")
	{
		return;
	}
	if (extName == "VK_EXT_custom_border_color")
	{
		return;
	}
	if (extName == "VK_GOOGLE_user_type")
	{
		return;
	}
	if (extName == "VK_EXT_private_data")
	{
		return;
	}
	if (extName == "VK_EXT_pipeline_creation_cache_control")
	{
		return;
	}
	if (extName == "VK_NV_device_diagnostics_config")
	{
		return;
	}
	if (extName == "VK_NV_fragment_shading_rate_enums")
	{
		return;
	}
	if (extName == "VK_EXT_fragment_density_map2")
	{
		return;
	}
	if (extName == "VK_QCOM_rotated_copy_commands")
	{
		return;
	}
	if (extName == "VK_EXT_image_robustness")
	{
		return;
	}
	if (extName == "VK_EXT_4444_formats")
	{
		return;
	}
	if (extName == "VK_NV_acquire_winrt_display")
	{
		functions.push_back("vkAcquireWinrtDisplayNV");
		functions.push_back("vkGetWinrtDisplayNV");
		return;
	}
	if (extName == "VK_VALVE_mutable_descriptor_type")
	{
		return;
	}
	if (extName == "VK_EXT_physical_device_drm")
	{
		return;
	}
	if (extName == "VK_KHR_acceleration_structure")
	{
		return;
	}
	if (extName == "VK_KHR_ray_tracing_pipeline")
	{
		return;
	}
	if (extName == "VK_KHR_ray_query")
	{
		return;
	}
	if (extName == "VK_KHR_android_surface")
	{
		functions.push_back("vkCreateAndroidSurfaceKHR");
		return;
	}
	if (extName == "VK_ANDROID_external_memory_android_hardware_buffer")
	{
		return;
	}
	if (extName == "VK_KHR_portability_subset")
	{
		return;
	}
	if (extName == "VK_FUCHSIA_imagepipe_surface")
	{
		functions.push_back("vkCreateImagePipeSurfaceFUCHSIA");
		return;
	}
	if (extName == "VK_GGP_stream_descriptor_surface")
	{
		functions.push_back("vkCreateStreamDescriptorSurfaceGGP");
		return;
	}
	if (extName == "VK_GGP_frame_token")
	{
		return;
	}
	if (extName == "VK_MVK_ios_surface")
	{
		functions.push_back("vkCreateIOSSurfaceMVK");
		return;
	}
	if (extName == "VK_MVK_macos_surface")
	{
		functions.push_back("vkCreateMacOSSurfaceMVK");
		return;
	}
	if (extName == "VK_EXT_metal_surface")
	{
		functions.push_back("vkCreateMetalSurfaceEXT");
		return;
	}
	if (extName == "VK_NN_vi_surface")
	{
		functions.push_back("vkCreateViSurfaceNN");
		return;
	}
	if (extName == "VK_KHR_wayland_surface")
	{
		functions.push_back("vkCreateWaylandSurfaceKHR");
		functions.push_back("vkGetPhysicalDeviceWaylandPresentationSupportKHR");
		return;
	}
	if (extName == "VK_KHR_win32_surface")
	{
		functions.push_back("vkCreateWin32SurfaceKHR");
		functions.push_back("vkGetPhysicalDeviceWin32PresentationSupportKHR");
		return;
	}
	if (extName == "VK_KHR_external_memory_win32")
	{
		return;
	}
	if (extName == "VK_KHR_win32_keyed_mutex")
	{
		return;
	}
	if (extName == "VK_KHR_external_semaphore_win32")
	{
		return;
	}
	if (extName == "VK_KHR_external_fence_win32")
	{
		return;
	}
	if (extName == "VK_NV_external_memory_win32")
	{
		return;
	}
	if (extName == "VK_NV_win32_keyed_mutex")
	{
		return;
	}
	if (extName == "VK_EXT_full_screen_exclusive")
	{
		functions.push_back("vkGetPhysicalDeviceSurfacePresentModes2EXT");
		return;
	}
	if (extName == "VK_KHR_xcb_surface")
	{
		functions.push_back("vkCreateXcbSurfaceKHR");
		functions.push_back("vkGetPhysicalDeviceXcbPresentationSupportKHR");
		return;
	}
	if (extName == "VK_KHR_xlib_surface")
	{
		functions.push_back("vkCreateXlibSurfaceKHR");
		functions.push_back("vkGetPhysicalDeviceXlibPresentationSupportKHR");
		return;
	}
	if (extName == "VK_EXT_acquire_xlib_display")
	{
		functions.push_back("vkAcquireXlibDisplayEXT");
		functions.push_back("vkGetRandROutputDisplayEXT");
		return;
	}
	DE_FATAL("Extension name not found");
}

void getDeviceExtensionFunctions (deUint32 apiVersion, ::std::string extName, ::std::vector<const char*>& functions)
{
	if (extName == "VK_KHR_surface")
	{
		return;
	}
	if (extName == "VK_KHR_swapchain")
	{
		functions.push_back("vkCreateSwapchainKHR");
		functions.push_back("vkDestroySwapchainKHR");
		functions.push_back("vkGetSwapchainImagesKHR");
		functions.push_back("vkAcquireNextImageKHR");
		functions.push_back("vkQueuePresentKHR");
		if(apiVersion >= VK_API_VERSION_1_1) functions.push_back("vkGetDeviceGroupPresentCapabilitiesKHR");
		if(apiVersion >= VK_API_VERSION_1_1) functions.push_back("vkGetDeviceGroupSurfacePresentModesKHR");
		if(apiVersion >= VK_API_VERSION_1_1) functions.push_back("vkAcquireNextImage2KHR");
		return;
	}
	if (extName == "VK_KHR_display")
	{
		return;
	}
	if (extName == "VK_KHR_display_swapchain")
	{
		functions.push_back("vkCreateSharedSwapchainsKHR");
		return;
	}
	if (extName == "VK_KHR_sampler_mirror_clamp_to_edge")
	{
		return;
	}
	if (extName == "VK_KHR_multiview")
	{
		return;
	}
	if (extName == "VK_KHR_get_physical_device_properties2")
	{
		return;
	}
	if (extName == "VK_KHR_device_group")
	{
		functions.push_back("vkGetDeviceGroupPeerMemoryFeaturesKHR");
		functions.push_back("vkCmdSetDeviceMaskKHR");
		functions.push_back("vkCmdDispatchBaseKHR");
		if(apiVersion < VK_API_VERSION_1_1) functions.push_back("vkGetDeviceGroupPresentCapabilitiesKHR");
		if(apiVersion < VK_API_VERSION_1_1) functions.push_back("vkGetDeviceGroupSurfacePresentModesKHR");
		if(apiVersion < VK_API_VERSION_1_1) functions.push_back("vkAcquireNextImage2KHR");
		return;
	}
	if (extName == "VK_KHR_shader_draw_parameters")
	{
		return;
	}
	if (extName == "VK_KHR_maintenance1")
	{
		functions.push_back("vkTrimCommandPoolKHR");
		return;
	}
	if (extName == "VK_KHR_device_group_creation")
	{
		return;
	}
	if (extName == "VK_KHR_external_memory_capabilities")
	{
		return;
	}
	if (extName == "VK_KHR_external_memory")
	{
		return;
	}
	if (extName == "VK_KHR_external_memory_fd")
	{
		functions.push_back("vkGetMemoryFdKHR");
		functions.push_back("vkGetMemoryFdPropertiesKHR");
		return;
	}
	if (extName == "VK_KHR_external_semaphore_capabilities")
	{
		return;
	}
	if (extName == "VK_KHR_external_semaphore")
	{
		return;
	}
	if (extName == "VK_KHR_external_semaphore_fd")
	{
		functions.push_back("vkImportSemaphoreFdKHR");
		functions.push_back("vkGetSemaphoreFdKHR");
		return;
	}
	if (extName == "VK_KHR_push_descriptor")
	{
		functions.push_back("vkCmdPushDescriptorSetKHR");
		functions.push_back("vkCmdPushDescriptorSetWithTemplateKHR");
		return;
	}
	if (extName == "VK_KHR_shader_float16_int8")
	{
		return;
	}
	if (extName == "VK_KHR_16bit_storage")
	{
		return;
	}
	if (extName == "VK_KHR_incremental_present")
	{
		return;
	}
	if (extName == "VK_KHR_descriptor_update_template")
	{
		functions.push_back("vkCreateDescriptorUpdateTemplateKHR");
		functions.push_back("vkDestroyDescriptorUpdateTemplateKHR");
		functions.push_back("vkUpdateDescriptorSetWithTemplateKHR");
		return;
	}
	if (extName == "VK_KHR_imageless_framebuffer")
	{
		return;
	}
	if (extName == "VK_KHR_create_renderpass2")
	{
		functions.push_back("vkCreateRenderPass2KHR");
		functions.push_back("vkCmdBeginRenderPass2KHR");
		functions.push_back("vkCmdNextSubpass2KHR");
		functions.push_back("vkCmdEndRenderPass2KHR");
		return;
	}
	if (extName == "VK_KHR_shared_presentable_image")
	{
		functions.push_back("vkGetSwapchainStatusKHR");
		return;
	}
	if (extName == "VK_KHR_external_fence_capabilities")
	{
		return;
	}
	if (extName == "VK_KHR_external_fence")
	{
		return;
	}
	if (extName == "VK_KHR_external_fence_fd")
	{
		functions.push_back("vkImportFenceFdKHR");
		functions.push_back("vkGetFenceFdKHR");
		return;
	}
	if (extName == "VK_KHR_performance_query")
	{
		functions.push_back("vkAcquireProfilingLockKHR");
		functions.push_back("vkReleaseProfilingLockKHR");
		return;
	}
	if (extName == "VK_KHR_maintenance2")
	{
		return;
	}
	if (extName == "VK_KHR_get_surface_capabilities2")
	{
		return;
	}
	if (extName == "VK_KHR_variable_pointers")
	{
		return;
	}
	if (extName == "VK_KHR_get_display_properties2")
	{
		return;
	}
	if (extName == "VK_KHR_dedicated_allocation")
	{
		return;
	}
	if (extName == "VK_KHR_storage_buffer_storage_class")
	{
		return;
	}
	if (extName == "VK_KHR_relaxed_block_layout")
	{
		return;
	}
	if (extName == "VK_KHR_get_memory_requirements2")
	{
		functions.push_back("vkGetImageMemoryRequirements2KHR");
		functions.push_back("vkGetBufferMemoryRequirements2KHR");
		functions.push_back("vkGetImageSparseMemoryRequirements2KHR");
		return;
	}
	if (extName == "VK_KHR_image_format_list")
	{
		return;
	}
	if (extName == "VK_KHR_sampler_ycbcr_conversion")
	{
		functions.push_back("vkCreateSamplerYcbcrConversionKHR");
		functions.push_back("vkDestroySamplerYcbcrConversionKHR");
		return;
	}
	if (extName == "VK_KHR_bind_memory2")
	{
		functions.push_back("vkBindBufferMemory2KHR");
		functions.push_back("vkBindImageMemory2KHR");
		return;
	}
	if (extName == "VK_KHR_maintenance3")
	{
		functions.push_back("vkGetDescriptorSetLayoutSupportKHR");
		return;
	}
	if (extName == "VK_KHR_draw_indirect_count")
	{
		functions.push_back("vkCmdDrawIndirectCountKHR");
		functions.push_back("vkCmdDrawIndexedIndirectCountKHR");
		return;
	}
	if (extName == "VK_KHR_shader_subgroup_extended_types")
	{
		return;
	}
	if (extName == "VK_KHR_8bit_storage")
	{
		return;
	}
	if (extName == "VK_KHR_shader_atomic_int64")
	{
		return;
	}
	if (extName == "VK_KHR_shader_clock")
	{
		return;
	}
	if (extName == "VK_KHR_driver_properties")
	{
		return;
	}
	if (extName == "VK_KHR_shader_float_controls")
	{
		return;
	}
	if (extName == "VK_KHR_depth_stencil_resolve")
	{
		return;
	}
	if (extName == "VK_KHR_swapchain_mutable_format")
	{
		return;
	}
	if (extName == "VK_KHR_timeline_semaphore")
	{
		functions.push_back("vkGetSemaphoreCounterValueKHR");
		functions.push_back("vkWaitSemaphoresKHR");
		functions.push_back("vkSignalSemaphoreKHR");
		return;
	}
	if (extName == "VK_KHR_vulkan_memory_model")
	{
		return;
	}
	if (extName == "VK_KHR_shader_terminate_invocation")
	{
		return;
	}
	if (extName == "VK_KHR_fragment_shading_rate")
	{
		functions.push_back("vkCmdSetFragmentShadingRateKHR");
		return;
	}
	if (extName == "VK_KHR_spirv_1_4")
	{
		return;
	}
	if (extName == "VK_KHR_surface_protected_capabilities")
	{
		return;
	}
	if (extName == "VK_KHR_separate_depth_stencil_layouts")
	{
		return;
	}
	if (extName == "VK_KHR_uniform_buffer_standard_layout")
	{
		return;
	}
	if (extName == "VK_KHR_buffer_device_address")
	{
		functions.push_back("vkGetBufferDeviceAddressKHR");
		functions.push_back("vkGetBufferOpaqueCaptureAddressKHR");
		functions.push_back("vkGetDeviceMemoryOpaqueCaptureAddressKHR");
		return;
	}
	if (extName == "VK_KHR_deferred_host_operations")
	{
		functions.push_back("vkCreateDeferredOperationKHR");
		functions.push_back("vkDestroyDeferredOperationKHR");
		functions.push_back("vkGetDeferredOperationMaxConcurrencyKHR");
		functions.push_back("vkGetDeferredOperationResultKHR");
		functions.push_back("vkDeferredOperationJoinKHR");
		return;
	}
	if (extName == "VK_KHR_pipeline_executable_properties")
	{
		functions.push_back("vkGetPipelineExecutablePropertiesKHR");
		functions.push_back("vkGetPipelineExecutableStatisticsKHR");
		functions.push_back("vkGetPipelineExecutableInternalRepresentationsKHR");
		return;
	}
	if (extName == "VK_KHR_pipeline_library")
	{
		return;
	}
	if (extName == "VK_KHR_shader_non_semantic_info")
	{
		return;
	}
	if (extName == "VK_KHR_synchronization2")
	{
		functions.push_back("vkCmdSetEvent2KHR");
		functions.push_back("vkCmdResetEvent2KHR");
		functions.push_back("vkCmdWaitEvents2KHR");
		functions.push_back("vkCmdPipelineBarrier2KHR");
		functions.push_back("vkCmdWriteTimestamp2KHR");
		functions.push_back("vkQueueSubmit2KHR");
		return;
	}
	if (extName == "VK_KHR_zero_initialize_workgroup_memory")
	{
		return;
	}
	if (extName == "VK_KHR_workgroup_memory_explicit_layout")
	{
		return;
	}
	if (extName == "VK_KHR_copy_commands2")
	{
		functions.push_back("vkCmdCopyBuffer2KHR");
		functions.push_back("vkCmdCopyImage2KHR");
		functions.push_back("vkCmdCopyBufferToImage2KHR");
		functions.push_back("vkCmdCopyImageToBuffer2KHR");
		functions.push_back("vkCmdBlitImage2KHR");
		functions.push_back("vkCmdResolveImage2KHR");
		return;
	}
	if (extName == "VK_EXT_debug_report")
	{
		return;
	}
	if (extName == "VK_NV_glsl_shader")
	{
		return;
	}
	if (extName == "VK_EXT_depth_range_unrestricted")
	{
		return;
	}
	if (extName == "VK_IMG_filter_cubic")
	{
		return;
	}
	if (extName == "VK_AMD_rasterization_order")
	{
		return;
	}
	if (extName == "VK_AMD_shader_trinary_minmax")
	{
		return;
	}
	if (extName == "VK_AMD_shader_explicit_vertex_parameter")
	{
		return;
	}
	if (extName == "VK_EXT_debug_marker")
	{
		functions.push_back("vkDebugMarkerSetObjectTagEXT");
		functions.push_back("vkDebugMarkerSetObjectNameEXT");
		functions.push_back("vkCmdDebugMarkerBeginEXT");
		functions.push_back("vkCmdDebugMarkerEndEXT");
		functions.push_back("vkCmdDebugMarkerInsertEXT");
		return;
	}
	if (extName == "VK_AMD_gcn_shader")
	{
		return;
	}
	if (extName == "VK_NV_dedicated_allocation")
	{
		return;
	}
	if (extName == "VK_EXT_transform_feedback")
	{
		functions.push_back("vkCmdBindTransformFeedbackBuffersEXT");
		functions.push_back("vkCmdBeginTransformFeedbackEXT");
		functions.push_back("vkCmdEndTransformFeedbackEXT");
		functions.push_back("vkCmdBeginQueryIndexedEXT");
		functions.push_back("vkCmdEndQueryIndexedEXT");
		functions.push_back("vkCmdDrawIndirectByteCountEXT");
		return;
	}
	if (extName == "VK_NVX_image_view_handle")
	{
		functions.push_back("vkGetImageViewHandleNVX");
		functions.push_back("vkGetImageViewAddressNVX");
		return;
	}
	if (extName == "VK_AMD_draw_indirect_count")
	{
		functions.push_back("vkCmdDrawIndirectCountAMD");
		functions.push_back("vkCmdDrawIndexedIndirectCountAMD");
		return;
	}
	if (extName == "VK_AMD_negative_viewport_height")
	{
		return;
	}
	if (extName == "VK_AMD_gpu_shader_half_float")
	{
		return;
	}
	if (extName == "VK_AMD_shader_ballot")
	{
		return;
	}
	if (extName == "VK_AMD_texture_gather_bias_lod")
	{
		return;
	}
	if (extName == "VK_AMD_shader_info")
	{
		functions.push_back("vkGetShaderInfoAMD");
		return;
	}
	if (extName == "VK_AMD_shader_image_load_store_lod")
	{
		return;
	}
	if (extName == "VK_NV_corner_sampled_image")
	{
		return;
	}
	if (extName == "VK_IMG_format_pvrtc")
	{
		return;
	}
	if (extName == "VK_NV_external_memory_capabilities")
	{
		return;
	}
	if (extName == "VK_NV_external_memory")
	{
		return;
	}
	if (extName == "VK_EXT_validation_flags")
	{
		return;
	}
	if (extName == "VK_EXT_shader_subgroup_ballot")
	{
		return;
	}
	if (extName == "VK_EXT_shader_subgroup_vote")
	{
		return;
	}
	if (extName == "VK_EXT_texture_compression_astc_hdr")
	{
		return;
	}
	if (extName == "VK_EXT_astc_decode_mode")
	{
		return;
	}
	if (extName == "VK_EXT_conditional_rendering")
	{
		functions.push_back("vkCmdBeginConditionalRenderingEXT");
		functions.push_back("vkCmdEndConditionalRenderingEXT");
		return;
	}
	if (extName == "VK_NV_clip_space_w_scaling")
	{
		functions.push_back("vkCmdSetViewportWScalingNV");
		return;
	}
	if (extName == "VK_EXT_direct_mode_display")
	{
		return;
	}
	if (extName == "VK_EXT_display_surface_counter")
	{
		return;
	}
	if (extName == "VK_EXT_display_control")
	{
		functions.push_back("vkDisplayPowerControlEXT");
		functions.push_back("vkRegisterDeviceEventEXT");
		functions.push_back("vkRegisterDisplayEventEXT");
		functions.push_back("vkGetSwapchainCounterEXT");
		return;
	}
	if (extName == "VK_GOOGLE_display_timing")
	{
		functions.push_back("vkGetRefreshCycleDurationGOOGLE");
		functions.push_back("vkGetPastPresentationTimingGOOGLE");
		return;
	}
	if (extName == "VK_NV_sample_mask_override_coverage")
	{
		return;
	}
	if (extName == "VK_NV_geometry_shader_passthrough")
	{
		return;
	}
	if (extName == "VK_NV_viewport_array2")
	{
		return;
	}
	if (extName == "VK_NVX_multiview_per_view_attributes")
	{
		return;
	}
	if (extName == "VK_NV_viewport_swizzle")
	{
		return;
	}
	if (extName == "VK_EXT_discard_rectangles")
	{
		functions.push_back("vkCmdSetDiscardRectangleEXT");
		return;
	}
	if (extName == "VK_EXT_conservative_rasterization")
	{
		return;
	}
	if (extName == "VK_EXT_depth_clip_enable")
	{
		return;
	}
	if (extName == "VK_EXT_swapchain_colorspace")
	{
		return;
	}
	if (extName == "VK_EXT_hdr_metadata")
	{
		functions.push_back("vkSetHdrMetadataEXT");
		return;
	}
	if (extName == "VK_EXT_external_memory_dma_buf")
	{
		return;
	}
	if (extName == "VK_EXT_queue_family_foreign")
	{
		return;
	}
	if (extName == "VK_EXT_debug_utils")
	{
		functions.push_back("vkSetDebugUtilsObjectNameEXT");
		functions.push_back("vkSetDebugUtilsObjectTagEXT");
		functions.push_back("vkQueueBeginDebugUtilsLabelEXT");
		functions.push_back("vkQueueEndDebugUtilsLabelEXT");
		functions.push_back("vkQueueInsertDebugUtilsLabelEXT");
		functions.push_back("vkCmdBeginDebugUtilsLabelEXT");
		functions.push_back("vkCmdEndDebugUtilsLabelEXT");
		functions.push_back("vkCmdInsertDebugUtilsLabelEXT");
		return;
	}
	if (extName == "VK_EXT_sampler_filter_minmax")
	{
		return;
	}
	if (extName == "VK_AMD_gpu_shader_int16")
	{
		return;
	}
	if (extName == "VK_AMD_mixed_attachment_samples")
	{
		return;
	}
	if (extName == "VK_AMD_shader_fragment_mask")
	{
		return;
	}
	if (extName == "VK_EXT_inline_uniform_block")
	{
		return;
	}
	if (extName == "VK_EXT_shader_stencil_export")
	{
		return;
	}
	if (extName == "VK_EXT_sample_locations")
	{
		functions.push_back("vkCmdSetSampleLocationsEXT");
		return;
	}
	if (extName == "VK_EXT_blend_operation_advanced")
	{
		return;
	}
	if (extName == "VK_NV_fragment_coverage_to_color")
	{
		return;
	}
	if (extName == "VK_NV_framebuffer_mixed_samples")
	{
		return;
	}
	if (extName == "VK_NV_fill_rectangle")
	{
		return;
	}
	if (extName == "VK_NV_shader_sm_builtins")
	{
		return;
	}
	if (extName == "VK_EXT_post_depth_coverage")
	{
		return;
	}
	if (extName == "VK_EXT_image_drm_format_modifier")
	{
		functions.push_back("vkGetImageDrmFormatModifierPropertiesEXT");
		return;
	}
	if (extName == "VK_EXT_validation_cache")
	{
		functions.push_back("vkCreateValidationCacheEXT");
		functions.push_back("vkDestroyValidationCacheEXT");
		functions.push_back("vkMergeValidationCachesEXT");
		functions.push_back("vkGetValidationCacheDataEXT");
		return;
	}
	if (extName == "VK_EXT_descriptor_indexing")
	{
		return;
	}
	if (extName == "VK_EXT_shader_viewport_index_layer")
	{
		return;
	}
	if (extName == "VK_NV_shading_rate_image")
	{
		functions.push_back("vkCmdBindShadingRateImageNV");
		functions.push_back("vkCmdSetViewportShadingRatePaletteNV");
		functions.push_back("vkCmdSetCoarseSampleOrderNV");
		return;
	}
	if (extName == "VK_NV_ray_tracing")
	{
		functions.push_back("vkCreateAccelerationStructureNV");
		functions.push_back("vkDestroyAccelerationStructureNV");
		functions.push_back("vkGetAccelerationStructureMemoryRequirementsNV");
		functions.push_back("vkBindAccelerationStructureMemoryNV");
		functions.push_back("vkCmdBuildAccelerationStructureNV");
		functions.push_back("vkCmdCopyAccelerationStructureNV");
		functions.push_back("vkCmdTraceRaysNV");
		functions.push_back("vkCreateRayTracingPipelinesNV");
		functions.push_back("vkGetRayTracingShaderGroupHandlesNV");
		functions.push_back("vkGetAccelerationStructureHandleNV");
		functions.push_back("vkCmdWriteAccelerationStructuresPropertiesNV");
		functions.push_back("vkCompileDeferredNV");
		return;
	}
	if (extName == "VK_NV_representative_fragment_test")
	{
		return;
	}
	if (extName == "VK_EXT_filter_cubic")
	{
		return;
	}
	if (extName == "VK_QCOM_render_pass_shader_resolve")
	{
		return;
	}
	if (extName == "VK_EXT_global_priority")
	{
		return;
	}
	if (extName == "VK_EXT_external_memory_host")
	{
		functions.push_back("vkGetMemoryHostPointerPropertiesEXT");
		return;
	}
	if (extName == "VK_AMD_buffer_marker")
	{
		functions.push_back("vkCmdWriteBufferMarkerAMD");
		return;
	}
	if (extName == "VK_AMD_pipeline_compiler_control")
	{
		return;
	}
	if (extName == "VK_EXT_calibrated_timestamps")
	{
		functions.push_back("vkGetCalibratedTimestampsEXT");
		return;
	}
	if (extName == "VK_AMD_shader_core_properties")
	{
		return;
	}
	if (extName == "VK_AMD_memory_overallocation_behavior")
	{
		return;
	}
	if (extName == "VK_EXT_vertex_attribute_divisor")
	{
		return;
	}
	if (extName == "VK_EXT_pipeline_creation_feedback")
	{
		return;
	}
	if (extName == "VK_NV_shader_subgroup_partitioned")
	{
		return;
	}
	if (extName == "VK_NV_compute_shader_derivatives")
	{
		return;
	}
	if (extName == "VK_NV_mesh_shader")
	{
		functions.push_back("vkCmdDrawMeshTasksNV");
		functions.push_back("vkCmdDrawMeshTasksIndirectNV");
		functions.push_back("vkCmdDrawMeshTasksIndirectCountNV");
		return;
	}
	if (extName == "VK_NV_fragment_shader_barycentric")
	{
		return;
	}
	if (extName == "VK_NV_shader_image_footprint")
	{
		return;
	}
	if (extName == "VK_NV_scissor_exclusive")
	{
		functions.push_back("vkCmdSetExclusiveScissorNV");
		return;
	}
	if (extName == "VK_NV_device_diagnostic_checkpoints")
	{
		functions.push_back("vkCmdSetCheckpointNV");
		functions.push_back("vkGetQueueCheckpointDataNV");
		return;
	}
	if (extName == "VK_INTEL_shader_integer_functions2")
	{
		return;
	}
	if (extName == "VK_INTEL_performance_query")
	{
		functions.push_back("vkInitializePerformanceApiINTEL");
		functions.push_back("vkUninitializePerformanceApiINTEL");
		functions.push_back("vkCmdSetPerformanceMarkerINTEL");
		functions.push_back("vkCmdSetPerformanceStreamMarkerINTEL");
		functions.push_back("vkCmdSetPerformanceOverrideINTEL");
		functions.push_back("vkAcquirePerformanceConfigurationINTEL");
		functions.push_back("vkReleasePerformanceConfigurationINTEL");
		functions.push_back("vkQueueSetPerformanceConfigurationINTEL");
		functions.push_back("vkGetPerformanceParameterINTEL");
		return;
	}
	if (extName == "VK_EXT_pci_bus_info")
	{
		return;
	}
	if (extName == "VK_AMD_display_native_hdr")
	{
		functions.push_back("vkSetLocalDimmingAMD");
		return;
	}
	if (extName == "VK_EXT_fragment_density_map")
	{
		return;
	}
	if (extName == "VK_EXT_scalar_block_layout")
	{
		return;
	}
	if (extName == "VK_GOOGLE_hlsl_functionality1")
	{
		return;
	}
	if (extName == "VK_GOOGLE_decorate_string")
	{
		return;
	}
	if (extName == "VK_EXT_subgroup_size_control")
	{
		return;
	}
	if (extName == "VK_AMD_shader_core_properties2")
	{
		return;
	}
	if (extName == "VK_AMD_device_coherent_memory")
	{
		return;
	}
	if (extName == "VK_EXT_shader_image_atomic_int64")
	{
		return;
	}
	if (extName == "VK_EXT_memory_budget")
	{
		return;
	}
	if (extName == "VK_EXT_memory_priority")
	{
		return;
	}
	if (extName == "VK_NV_dedicated_allocation_image_aliasing")
	{
		return;
	}
	if (extName == "VK_EXT_buffer_device_address")
	{
		functions.push_back("vkGetBufferDeviceAddressEXT");
		return;
	}
	if (extName == "VK_EXT_tooling_info")
	{
		return;
	}
	if (extName == "VK_EXT_separate_stencil_usage")
	{
		return;
	}
	if (extName == "VK_EXT_validation_features")
	{
		return;
	}
	if (extName == "VK_NV_cooperative_matrix")
	{
		return;
	}
	if (extName == "VK_NV_coverage_reduction_mode")
	{
		return;
	}
	if (extName == "VK_EXT_fragment_shader_interlock")
	{
		return;
	}
	if (extName == "VK_EXT_ycbcr_image_arrays")
	{
		return;
	}
	if (extName == "VK_EXT_headless_surface")
	{
		return;
	}
	if (extName == "VK_EXT_line_rasterization")
	{
		functions.push_back("vkCmdSetLineStippleEXT");
		return;
	}
	if (extName == "VK_EXT_shader_atomic_float")
	{
		return;
	}
	if (extName == "VK_EXT_host_query_reset")
	{
		functions.push_back("vkResetQueryPoolEXT");
		return;
	}
	if (extName == "VK_EXT_index_type_uint8")
	{
		return;
	}
	if (extName == "VK_EXT_extended_dynamic_state")
	{
		functions.push_back("vkCmdSetCullModeEXT");
		functions.push_back("vkCmdSetFrontFaceEXT");
		functions.push_back("vkCmdSetPrimitiveTopologyEXT");
		functions.push_back("vkCmdSetViewportWithCountEXT");
		functions.push_back("vkCmdSetScissorWithCountEXT");
		functions.push_back("vkCmdBindVertexBuffers2EXT");
		functions.push_back("vkCmdSetDepthTestEnableEXT");
		functions.push_back("vkCmdSetDepthWriteEnableEXT");
		functions.push_back("vkCmdSetDepthCompareOpEXT");
		functions.push_back("vkCmdSetDepthBoundsTestEnableEXT");
		functions.push_back("vkCmdSetStencilTestEnableEXT");
		functions.push_back("vkCmdSetStencilOpEXT");
		return;
	}
	if (extName == "VK_EXT_shader_demote_to_helper_invocation")
	{
		return;
	}
	if (extName == "VK_NV_device_generated_commands")
	{
		functions.push_back("vkGetGeneratedCommandsMemoryRequirementsNV");
		functions.push_back("vkCmdPreprocessGeneratedCommandsNV");
		functions.push_back("vkCmdExecuteGeneratedCommandsNV");
		functions.push_back("vkCmdBindPipelineShaderGroupNV");
		functions.push_back("vkCreateIndirectCommandsLayoutNV");
		functions.push_back("vkDestroyIndirectCommandsLayoutNV");
		return;
	}
	if (extName == "VK_EXT_texel_buffer_alignment")
	{
		return;
	}
	if (extName == "VK_QCOM_render_pass_transform")
	{
		return;
	}
	if (extName == "VK_EXT_device_memory_report")
	{
		return;
	}
	if (extName == "VK_EXT_robustness2")
	{
		return;
	}
	if (extName == "VK_EXT_custom_border_color")
	{
		return;
	}
	if (extName == "VK_GOOGLE_user_type")
	{
		return;
	}
	if (extName == "VK_EXT_private_data")
	{
		functions.push_back("vkCreatePrivateDataSlotEXT");
		functions.push_back("vkDestroyPrivateDataSlotEXT");
		functions.push_back("vkSetPrivateDataEXT");
		functions.push_back("vkGetPrivateDataEXT");
		return;
	}
	if (extName == "VK_EXT_pipeline_creation_cache_control")
	{
		return;
	}
	if (extName == "VK_NV_device_diagnostics_config")
	{
		return;
	}
	if (extName == "VK_NV_fragment_shading_rate_enums")
	{
		functions.push_back("vkCmdSetFragmentShadingRateEnumNV");
		return;
	}
	if (extName == "VK_EXT_fragment_density_map2")
	{
		return;
	}
	if (extName == "VK_QCOM_rotated_copy_commands")
	{
		return;
	}
	if (extName == "VK_EXT_image_robustness")
	{
		return;
	}
	if (extName == "VK_EXT_4444_formats")
	{
		return;
	}
	if (extName == "VK_NV_acquire_winrt_display")
	{
		return;
	}
	if (extName == "VK_VALVE_mutable_descriptor_type")
	{
		return;
	}
	if (extName == "VK_EXT_physical_device_drm")
	{
		return;
	}
	if (extName == "VK_KHR_acceleration_structure")
	{
		functions.push_back("vkCreateAccelerationStructureKHR");
		functions.push_back("vkDestroyAccelerationStructureKHR");
		functions.push_back("vkCmdBuildAccelerationStructuresKHR");
		functions.push_back("vkCmdBuildAccelerationStructuresIndirectKHR");
		functions.push_back("vkBuildAccelerationStructuresKHR");
		functions.push_back("vkCopyAccelerationStructureKHR");
		functions.push_back("vkCopyAccelerationStructureToMemoryKHR");
		functions.push_back("vkCopyMemoryToAccelerationStructureKHR");
		functions.push_back("vkWriteAccelerationStructuresPropertiesKHR");
		functions.push_back("vkCmdCopyAccelerationStructureKHR");
		functions.push_back("vkCmdCopyAccelerationStructureToMemoryKHR");
		functions.push_back("vkCmdCopyMemoryToAccelerationStructureKHR");
		functions.push_back("vkGetAccelerationStructureDeviceAddressKHR");
		functions.push_back("vkCmdWriteAccelerationStructuresPropertiesKHR");
		functions.push_back("vkGetDeviceAccelerationStructureCompatibilityKHR");
		functions.push_back("vkGetAccelerationStructureBuildSizesKHR");
		return;
	}
	if (extName == "VK_KHR_ray_tracing_pipeline")
	{
		functions.push_back("vkCmdTraceRaysKHR");
		functions.push_back("vkCreateRayTracingPipelinesKHR");
		functions.push_back("vkGetRayTracingCaptureReplayShaderGroupHandlesKHR");
		functions.push_back("vkCmdTraceRaysIndirectKHR");
		functions.push_back("vkGetRayTracingShaderGroupStackSizeKHR");
		functions.push_back("vkCmdSetRayTracingPipelineStackSizeKHR");
		return;
	}
	if (extName == "VK_KHR_ray_query")
	{
		return;
	}
	if (extName == "VK_KHR_android_surface")
	{
		return;
	}
	if (extName == "VK_ANDROID_external_memory_android_hardware_buffer")
	{
		functions.push_back("vkGetAndroidHardwareBufferPropertiesANDROID");
		functions.push_back("vkGetMemoryAndroidHardwareBufferANDROID");
		return;
	}
	if (extName == "VK_KHR_portability_subset")
	{
		return;
	}
	if (extName == "VK_FUCHSIA_imagepipe_surface")
	{
		return;
	}
	if (extName == "VK_GGP_stream_descriptor_surface")
	{
		return;
	}
	if (extName == "VK_GGP_frame_token")
	{
		return;
	}
	if (extName == "VK_MVK_ios_surface")
	{
		return;
	}
	if (extName == "VK_MVK_macos_surface")
	{
		return;
	}
	if (extName == "VK_EXT_metal_surface")
	{
		return;
	}
	if (extName == "VK_NN_vi_surface")
	{
		return;
	}
	if (extName == "VK_KHR_wayland_surface")
	{
		return;
	}
	if (extName == "VK_KHR_win32_surface")
	{
		return;
	}
	if (extName == "VK_KHR_external_memory_win32")
	{
		functions.push_back("vkGetMemoryWin32HandleKHR");
		functions.push_back("vkGetMemoryWin32HandlePropertiesKHR");
		return;
	}
	if (extName == "VK_KHR_win32_keyed_mutex")
	{
		return;
	}
	if (extName == "VK_KHR_external_semaphore_win32")
	{
		functions.push_back("vkImportSemaphoreWin32HandleKHR");
		functions.push_back("vkGetSemaphoreWin32HandleKHR");
		return;
	}
	if (extName == "VK_KHR_external_fence_win32")
	{
		functions.push_back("vkImportFenceWin32HandleKHR");
		functions.push_back("vkGetFenceWin32HandleKHR");
		return;
	}
	if (extName == "VK_NV_external_memory_win32")
	{
		functions.push_back("vkGetMemoryWin32HandleNV");
		return;
	}
	if (extName == "VK_NV_win32_keyed_mutex")
	{
		return;
	}
	if (extName == "VK_EXT_full_screen_exclusive")
	{
		functions.push_back("vkAcquireFullScreenExclusiveModeEXT");
		functions.push_back("vkReleaseFullScreenExclusiveModeEXT");
		functions.push_back("vkGetDeviceGroupSurfacePresentModes2EXT");
		return;
	}
	if (extName == "VK_KHR_xcb_surface")
	{
		return;
	}
	if (extName == "VK_KHR_xlib_surface")
	{
		return;
	}
	if (extName == "VK_EXT_acquire_xlib_display")
	{
		return;
	}
	DE_FATAL("Extension name not found");
}

::std::string instanceExtensionNames[] =
{
	"VK_KHR_surface",
	"VK_KHR_display",
	"VK_KHR_get_physical_device_properties2",
	"VK_KHR_device_group_creation",
	"VK_KHR_external_memory_capabilities",
	"VK_KHR_external_semaphore_capabilities",
	"VK_KHR_external_fence_capabilities",
	"VK_KHR_performance_query",
	"VK_KHR_get_surface_capabilities2",
	"VK_KHR_get_display_properties2",
	"VK_KHR_fragment_shading_rate",
	"VK_EXT_debug_report",
	"VK_NV_external_memory_capabilities",
	"VK_EXT_direct_mode_display",
	"VK_EXT_display_surface_counter",
	"VK_EXT_calibrated_timestamps",
	"VK_EXT_tooling_info",
	"VK_NV_cooperative_matrix",
	"VK_NV_coverage_reduction_mode",
	"VK_EXT_headless_surface",
	"VK_NV_acquire_winrt_display",
	"VK_KHR_android_surface",
	"VK_FUCHSIA_imagepipe_surface",
	"VK_GGP_stream_descriptor_surface",
	"VK_MVK_ios_surface",
	"VK_MVK_macos_surface",
	"VK_EXT_metal_surface",
	"VK_NN_vi_surface",
	"VK_KHR_wayland_surface",
	"VK_KHR_win32_surface",
	"VK_EXT_full_screen_exclusive",
	"VK_KHR_xcb_surface",
	"VK_KHR_xlib_surface",
	"VK_EXT_acquire_xlib_display"
};

::std::string deviceExtensionNames[] =
{
	"VK_KHR_swapchain",
	"VK_KHR_display_swapchain",
	"VK_KHR_device_group",
	"VK_KHR_maintenance1",
	"VK_KHR_external_memory_fd",
	"VK_KHR_external_semaphore_fd",
	"VK_KHR_push_descriptor",
	"VK_KHR_descriptor_update_template",
	"VK_KHR_create_renderpass2",
	"VK_KHR_shared_presentable_image",
	"VK_KHR_external_fence_fd",
	"VK_KHR_get_memory_requirements2",
	"VK_KHR_sampler_ycbcr_conversion",
	"VK_KHR_bind_memory2",
	"VK_KHR_maintenance3",
	"VK_KHR_draw_indirect_count",
	"VK_KHR_timeline_semaphore",
	"VK_KHR_buffer_device_address",
	"VK_KHR_deferred_host_operations",
	"VK_KHR_pipeline_executable_properties",
	"VK_KHR_synchronization2",
	"VK_KHR_copy_commands2",
	"VK_EXT_debug_marker",
	"VK_EXT_transform_feedback",
	"VK_NVX_image_view_handle",
	"VK_AMD_draw_indirect_count",
	"VK_AMD_shader_info",
	"VK_EXT_conditional_rendering",
	"VK_NV_clip_space_w_scaling",
	"VK_EXT_display_control",
	"VK_GOOGLE_display_timing",
	"VK_EXT_discard_rectangles",
	"VK_EXT_hdr_metadata",
	"VK_EXT_debug_utils",
	"VK_EXT_sample_locations",
	"VK_EXT_image_drm_format_modifier",
	"VK_EXT_validation_cache",
	"VK_NV_shading_rate_image",
	"VK_NV_ray_tracing",
	"VK_EXT_external_memory_host",
	"VK_AMD_buffer_marker",
	"VK_NV_mesh_shader",
	"VK_NV_scissor_exclusive",
	"VK_NV_device_diagnostic_checkpoints",
	"VK_INTEL_performance_query",
	"VK_AMD_display_native_hdr",
	"VK_EXT_buffer_device_address",
	"VK_EXT_line_rasterization",
	"VK_EXT_host_query_reset",
	"VK_EXT_extended_dynamic_state",
	"VK_NV_device_generated_commands",
	"VK_EXT_private_data",
	"VK_NV_fragment_shading_rate_enums",
	"VK_KHR_acceleration_structure",
	"VK_KHR_ray_tracing_pipeline",
	"VK_ANDROID_external_memory_android_hardware_buffer",
	"VK_KHR_external_memory_win32",
	"VK_KHR_external_semaphore_win32",
	"VK_KHR_external_fence_win32",
	"VK_NV_external_memory_win32"
};
