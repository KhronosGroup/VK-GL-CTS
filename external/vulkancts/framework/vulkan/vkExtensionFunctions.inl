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
	}
	else if (extName == "VK_KHR_swapchain")
	{
		if(apiVersion >= VK_API_VERSION_1_1) functions.push_back("vkGetPhysicalDevicePresentRectanglesKHR");
	}
	else if (extName == "VK_KHR_display")
	{
		functions.push_back("vkGetPhysicalDeviceDisplayPropertiesKHR");
		functions.push_back("vkGetPhysicalDeviceDisplayPlanePropertiesKHR");
		functions.push_back("vkGetDisplayPlaneSupportedDisplaysKHR");
		functions.push_back("vkGetDisplayModePropertiesKHR");
		functions.push_back("vkCreateDisplayModeKHR");
		functions.push_back("vkGetDisplayPlaneCapabilitiesKHR");
		functions.push_back("vkCreateDisplayPlaneSurfaceKHR");
	}
	else if (extName == "VK_KHR_get_physical_device_properties2")
	{
		functions.push_back("vkGetPhysicalDeviceFeatures2KHR");
		functions.push_back("vkGetPhysicalDeviceProperties2KHR");
		functions.push_back("vkGetPhysicalDeviceFormatProperties2KHR");
		functions.push_back("vkGetPhysicalDeviceImageFormatProperties2KHR");
		functions.push_back("vkGetPhysicalDeviceQueueFamilyProperties2KHR");
		functions.push_back("vkGetPhysicalDeviceMemoryProperties2KHR");
		functions.push_back("vkGetPhysicalDeviceSparseImageFormatProperties2KHR");
	}
	else if (extName == "VK_KHR_device_group_creation")
	{
		functions.push_back("vkEnumeratePhysicalDeviceGroupsKHR");
	}
	else if (extName == "VK_KHR_external_memory_capabilities")
	{
		functions.push_back("vkGetPhysicalDeviceExternalBufferPropertiesKHR");
	}
	else if (extName == "VK_KHR_external_semaphore_capabilities")
	{
		functions.push_back("vkGetPhysicalDeviceExternalSemaphorePropertiesKHR");
	}
	else if (extName == "VK_KHR_external_fence_capabilities")
	{
		functions.push_back("vkGetPhysicalDeviceExternalFencePropertiesKHR");
	}
	else if (extName == "VK_KHR_performance_query")
	{
		functions.push_back("vkEnumeratePhysicalDeviceQueueFamilyPerformanceQueryCountersKHR");
		functions.push_back("vkGetPhysicalDeviceQueueFamilyPerformanceQueryPassesKHR");
	}
	else if (extName == "VK_KHR_get_surface_capabilities2")
	{
		functions.push_back("vkGetPhysicalDeviceSurfaceCapabilities2KHR");
		functions.push_back("vkGetPhysicalDeviceSurfaceFormats2KHR");
	}
	else if (extName == "VK_KHR_get_display_properties2")
	{
		functions.push_back("vkGetPhysicalDeviceDisplayProperties2KHR");
		functions.push_back("vkGetPhysicalDeviceDisplayPlaneProperties2KHR");
		functions.push_back("vkGetDisplayModeProperties2KHR");
		functions.push_back("vkGetDisplayPlaneCapabilities2KHR");
	}
	else if (extName == "VK_EXT_debug_report")
	{
		functions.push_back("vkCreateDebugReportCallbackEXT");
		functions.push_back("vkDestroyDebugReportCallbackEXT");
		functions.push_back("vkDebugReportMessageEXT");
	}
	else if (extName == "VK_NV_external_memory_capabilities")
	{
		functions.push_back("vkGetPhysicalDeviceExternalImageFormatPropertiesNV");
	}
	else if (extName == "VK_NVX_device_generated_commands")
	{
		functions.push_back("vkGetPhysicalDeviceGeneratedCommandsPropertiesNVX");
	}
	else if (extName == "VK_EXT_direct_mode_display")
	{
		functions.push_back("vkReleaseDisplayEXT");
	}
	else if (extName == "VK_EXT_display_surface_counter")
	{
		functions.push_back("vkGetPhysicalDeviceSurfaceCapabilities2EXT");
	}
	else if (extName == "VK_EXT_debug_utils")
	{
		functions.push_back("vkCreateDebugUtilsMessengerEXT");
		functions.push_back("vkDestroyDebugUtilsMessengerEXT");
		functions.push_back("vkSubmitDebugUtilsMessageEXT");
	}
	else if (extName == "VK_EXT_sample_locations")
	{
		functions.push_back("vkGetPhysicalDeviceMultisamplePropertiesEXT");
	}
	else if (extName == "VK_EXT_calibrated_timestamps")
	{
		functions.push_back("vkGetPhysicalDeviceCalibrateableTimeDomainsEXT");
	}
	else if (extName == "VK_EXT_tooling_info")
	{
		functions.push_back("vkGetPhysicalDeviceToolPropertiesEXT");
	}
	else if (extName == "VK_NV_cooperative_matrix")
	{
		functions.push_back("vkGetPhysicalDeviceCooperativeMatrixPropertiesNV");
	}
	else if (extName == "VK_NV_coverage_reduction_mode")
	{
		functions.push_back("vkGetPhysicalDeviceSupportedFramebufferMixedSamplesCombinationsNV");
	}
	else if (extName == "VK_EXT_headless_surface")
	{
		functions.push_back("vkCreateHeadlessSurfaceEXT");
	}
	else if (extName == "VK_KHR_android_surface")
	{
		functions.push_back("vkCreateAndroidSurfaceKHR");
	}
	else if (extName == "VK_FUCHSIA_imagepipe_surface")
	{
		functions.push_back("vkCreateImagePipeSurfaceFUCHSIA");
	}
	else if (extName == "VK_GGP_stream_descriptor_surface")
	{
		functions.push_back("vkCreateStreamDescriptorSurfaceGGP");
	}
	else if (extName == "VK_MVK_ios_surface")
	{
		functions.push_back("vkCreateIOSSurfaceMVK");
	}
	else if (extName == "VK_MVK_macos_surface")
	{
		functions.push_back("vkCreateMacOSSurfaceMVK");
	}
	else if (extName == "VK_EXT_metal_surface")
	{
		functions.push_back("vkCreateMetalSurfaceEXT");
	}
	else if (extName == "VK_NN_vi_surface")
	{
		functions.push_back("vkCreateViSurfaceNN");
	}
	else if (extName == "VK_KHR_wayland_surface")
	{
		functions.push_back("vkCreateWaylandSurfaceKHR");
		functions.push_back("vkGetPhysicalDeviceWaylandPresentationSupportKHR");
	}
	else if (extName == "VK_KHR_win32_surface")
	{
		functions.push_back("vkCreateWin32SurfaceKHR");
		functions.push_back("vkGetPhysicalDeviceWin32PresentationSupportKHR");
	}
	else if (extName == "VK_EXT_full_screen_exclusive")
	{
		functions.push_back("vkGetPhysicalDeviceSurfacePresentModes2EXT");
	}
	else if (extName == "VK_KHR_xcb_surface")
	{
		functions.push_back("vkCreateXcbSurfaceKHR");
		functions.push_back("vkGetPhysicalDeviceXcbPresentationSupportKHR");
	}
	else if (extName == "VK_KHR_xlib_surface")
	{
		functions.push_back("vkCreateXlibSurfaceKHR");
		functions.push_back("vkGetPhysicalDeviceXlibPresentationSupportKHR");
	}
	else if (extName == "VK_EXT_acquire_xlib_display")
	{
		functions.push_back("vkAcquireXlibDisplayEXT");
		functions.push_back("vkGetRandROutputDisplayEXT");
	}
	else
		DE_FATAL("Extension name not found");
}

void getDeviceExtensionFunctions (deUint32 apiVersion, ::std::string extName, ::std::vector<const char*>& functions)
{
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
	}
	else if (extName == "VK_KHR_display_swapchain")
	{
		functions.push_back("vkCreateSharedSwapchainsKHR");
	}
	else if (extName == "VK_KHR_device_group")
	{
		functions.push_back("vkGetDeviceGroupPeerMemoryFeaturesKHR");
		functions.push_back("vkCmdSetDeviceMaskKHR");
		functions.push_back("vkCmdDispatchBaseKHR");
		if(apiVersion < VK_API_VERSION_1_1) functions.push_back("vkGetDeviceGroupPresentCapabilitiesKHR");
		if(apiVersion < VK_API_VERSION_1_1) functions.push_back("vkGetDeviceGroupSurfacePresentModesKHR");
		if(apiVersion < VK_API_VERSION_1_1) functions.push_back("vkAcquireNextImage2KHR");
	}
	else if (extName == "VK_KHR_maintenance1")
	{
		functions.push_back("vkTrimCommandPoolKHR");
	}
	else if (extName == "VK_KHR_external_memory_fd")
	{
		functions.push_back("vkGetMemoryFdKHR");
		functions.push_back("vkGetMemoryFdPropertiesKHR");
	}
	else if (extName == "VK_KHR_external_semaphore_fd")
	{
		functions.push_back("vkImportSemaphoreFdKHR");
		functions.push_back("vkGetSemaphoreFdKHR");
	}
	else if (extName == "VK_KHR_push_descriptor")
	{
		functions.push_back("vkCmdPushDescriptorSetKHR");
		functions.push_back("vkCmdPushDescriptorSetWithTemplateKHR");
	}
	else if (extName == "VK_KHR_descriptor_update_template")
	{
		functions.push_back("vkCreateDescriptorUpdateTemplateKHR");
		functions.push_back("vkDestroyDescriptorUpdateTemplateKHR");
		functions.push_back("vkUpdateDescriptorSetWithTemplateKHR");
	}
	else if (extName == "VK_KHR_create_renderpass2")
	{
		functions.push_back("vkCreateRenderPass2KHR");
		functions.push_back("vkCmdBeginRenderPass2KHR");
		functions.push_back("vkCmdNextSubpass2KHR");
		functions.push_back("vkCmdEndRenderPass2KHR");
	}
	else if (extName == "VK_KHR_shared_presentable_image")
	{
		functions.push_back("vkGetSwapchainStatusKHR");
	}
	else if (extName == "VK_KHR_external_fence_fd")
	{
		functions.push_back("vkImportFenceFdKHR");
		functions.push_back("vkGetFenceFdKHR");
	}
	else if (extName == "VK_KHR_performance_query")
	{
		functions.push_back("vkAcquireProfilingLockKHR");
		functions.push_back("vkReleaseProfilingLockKHR");
	}
	else if (extName == "VK_KHR_get_memory_requirements2")
	{
		functions.push_back("vkGetImageMemoryRequirements2KHR");
		functions.push_back("vkGetBufferMemoryRequirements2KHR");
		functions.push_back("vkGetImageSparseMemoryRequirements2KHR");
	}
	else if (extName == "VK_KHR_sampler_ycbcr_conversion")
	{
		functions.push_back("vkCreateSamplerYcbcrConversionKHR");
		functions.push_back("vkDestroySamplerYcbcrConversionKHR");
	}
	else if (extName == "VK_KHR_bind_memory2")
	{
		functions.push_back("vkBindBufferMemory2KHR");
		functions.push_back("vkBindImageMemory2KHR");
	}
	else if (extName == "VK_KHR_maintenance3")
	{
		functions.push_back("vkGetDescriptorSetLayoutSupportKHR");
	}
	else if (extName == "VK_KHR_draw_indirect_count")
	{
		functions.push_back("vkCmdDrawIndirectCountKHR");
		functions.push_back("vkCmdDrawIndexedIndirectCountKHR");
	}
	else if (extName == "VK_KHR_timeline_semaphore")
	{
		functions.push_back("vkGetSemaphoreCounterValueKHR");
		functions.push_back("vkWaitSemaphoresKHR");
		functions.push_back("vkSignalSemaphoreKHR");
	}
	else if (extName == "VK_KHR_buffer_device_address")
	{
		functions.push_back("vkGetBufferDeviceAddressKHR");
		functions.push_back("vkGetBufferOpaqueCaptureAddressKHR");
		functions.push_back("vkGetDeviceMemoryOpaqueCaptureAddressKHR");
	}
	else if (extName == "VK_KHR_pipeline_executable_properties")
	{
		functions.push_back("vkGetPipelineExecutablePropertiesKHR");
		functions.push_back("vkGetPipelineExecutableStatisticsKHR");
		functions.push_back("vkGetPipelineExecutableInternalRepresentationsKHR");
	}
	else if (extName == "VK_EXT_debug_marker")
	{
		functions.push_back("vkDebugMarkerSetObjectTagEXT");
		functions.push_back("vkDebugMarkerSetObjectNameEXT");
		functions.push_back("vkCmdDebugMarkerBeginEXT");
		functions.push_back("vkCmdDebugMarkerEndEXT");
		functions.push_back("vkCmdDebugMarkerInsertEXT");
	}
	else if (extName == "VK_EXT_transform_feedback")
	{
		functions.push_back("vkCmdBindTransformFeedbackBuffersEXT");
		functions.push_back("vkCmdBeginTransformFeedbackEXT");
		functions.push_back("vkCmdEndTransformFeedbackEXT");
		functions.push_back("vkCmdBeginQueryIndexedEXT");
		functions.push_back("vkCmdEndQueryIndexedEXT");
		functions.push_back("vkCmdDrawIndirectByteCountEXT");
	}
	else if (extName == "VK_NVX_image_view_handle")
	{
		functions.push_back("vkGetImageViewHandleNVX");
	}
	else if (extName == "VK_AMD_draw_indirect_count")
	{
		functions.push_back("vkCmdDrawIndirectCountAMD");
		functions.push_back("vkCmdDrawIndexedIndirectCountAMD");
	}
	else if (extName == "VK_AMD_shader_info")
	{
		functions.push_back("vkGetShaderInfoAMD");
	}
	else if (extName == "VK_EXT_conditional_rendering")
	{
		functions.push_back("vkCmdBeginConditionalRenderingEXT");
		functions.push_back("vkCmdEndConditionalRenderingEXT");
	}
	else if (extName == "VK_NVX_device_generated_commands")
	{
		functions.push_back("vkCmdProcessCommandsNVX");
		functions.push_back("vkCmdReserveSpaceForCommandsNVX");
		functions.push_back("vkCreateIndirectCommandsLayoutNVX");
		functions.push_back("vkDestroyIndirectCommandsLayoutNVX");
		functions.push_back("vkCreateObjectTableNVX");
		functions.push_back("vkDestroyObjectTableNVX");
		functions.push_back("vkRegisterObjectsNVX");
		functions.push_back("vkUnregisterObjectsNVX");
	}
	else if (extName == "VK_NV_clip_space_w_scaling")
	{
		functions.push_back("vkCmdSetViewportWScalingNV");
	}
	else if (extName == "VK_EXT_display_control")
	{
		functions.push_back("vkDisplayPowerControlEXT");
		functions.push_back("vkRegisterDeviceEventEXT");
		functions.push_back("vkRegisterDisplayEventEXT");
		functions.push_back("vkGetSwapchainCounterEXT");
	}
	else if (extName == "VK_GOOGLE_display_timing")
	{
		functions.push_back("vkGetRefreshCycleDurationGOOGLE");
		functions.push_back("vkGetPastPresentationTimingGOOGLE");
	}
	else if (extName == "VK_EXT_discard_rectangles")
	{
		functions.push_back("vkCmdSetDiscardRectangleEXT");
	}
	else if (extName == "VK_EXT_hdr_metadata")
	{
		functions.push_back("vkSetHdrMetadataEXT");
	}
	else if (extName == "VK_EXT_debug_utils")
	{
		functions.push_back("vkSetDebugUtilsObjectNameEXT");
		functions.push_back("vkSetDebugUtilsObjectTagEXT");
		functions.push_back("vkQueueBeginDebugUtilsLabelEXT");
		functions.push_back("vkQueueEndDebugUtilsLabelEXT");
		functions.push_back("vkQueueInsertDebugUtilsLabelEXT");
		functions.push_back("vkCmdBeginDebugUtilsLabelEXT");
		functions.push_back("vkCmdEndDebugUtilsLabelEXT");
		functions.push_back("vkCmdInsertDebugUtilsLabelEXT");
	}
	else if (extName == "VK_EXT_sample_locations")
	{
		functions.push_back("vkCmdSetSampleLocationsEXT");
	}
	else if (extName == "VK_EXT_image_drm_format_modifier")
	{
		functions.push_back("vkGetImageDrmFormatModifierPropertiesEXT");
	}
	else if (extName == "VK_EXT_validation_cache")
	{
		functions.push_back("vkCreateValidationCacheEXT");
		functions.push_back("vkDestroyValidationCacheEXT");
		functions.push_back("vkMergeValidationCachesEXT");
		functions.push_back("vkGetValidationCacheDataEXT");
	}
	else if (extName == "VK_NV_shading_rate_image")
	{
		functions.push_back("vkCmdBindShadingRateImageNV");
		functions.push_back("vkCmdSetViewportShadingRatePaletteNV");
		functions.push_back("vkCmdSetCoarseSampleOrderNV");
	}
	else if (extName == "VK_NV_ray_tracing")
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
	}
	else if (extName == "VK_EXT_external_memory_host")
	{
		functions.push_back("vkGetMemoryHostPointerPropertiesEXT");
	}
	else if (extName == "VK_AMD_buffer_marker")
	{
		functions.push_back("vkCmdWriteBufferMarkerAMD");
	}
	else if (extName == "VK_EXT_calibrated_timestamps")
	{
		functions.push_back("vkGetCalibratedTimestampsEXT");
	}
	else if (extName == "VK_NV_mesh_shader")
	{
		functions.push_back("vkCmdDrawMeshTasksNV");
		functions.push_back("vkCmdDrawMeshTasksIndirectNV");
		functions.push_back("vkCmdDrawMeshTasksIndirectCountNV");
	}
	else if (extName == "VK_NV_scissor_exclusive")
	{
		functions.push_back("vkCmdSetExclusiveScissorNV");
	}
	else if (extName == "VK_NV_device_diagnostic_checkpoints")
	{
		functions.push_back("vkCmdSetCheckpointNV");
		functions.push_back("vkGetQueueCheckpointDataNV");
	}
	else if (extName == "VK_INTEL_performance_query")
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
	}
	else if (extName == "VK_AMD_display_native_hdr")
	{
		functions.push_back("vkSetLocalDimmingAMD");
	}
	else if (extName == "VK_EXT_buffer_device_address")
	{
		functions.push_back("vkGetBufferDeviceAddressEXT");
	}
	else if (extName == "VK_EXT_line_rasterization")
	{
		functions.push_back("vkCmdSetLineStippleEXT");
	}
	else if (extName == "VK_EXT_host_query_reset")
	{
		functions.push_back("vkResetQueryPoolEXT");
	}
	else if (extName == "VK_ANDROID_external_memory_android_hardware_buffer")
	{
		functions.push_back("vkGetAndroidHardwareBufferPropertiesANDROID");
		functions.push_back("vkGetMemoryAndroidHardwareBufferANDROID");
	}
	else if (extName == "VK_KHR_external_memory_win32")
	{
		functions.push_back("vkGetMemoryWin32HandleKHR");
		functions.push_back("vkGetMemoryWin32HandlePropertiesKHR");
	}
	else if (extName == "VK_KHR_external_semaphore_win32")
	{
		functions.push_back("vkImportSemaphoreWin32HandleKHR");
		functions.push_back("vkGetSemaphoreWin32HandleKHR");
	}
	else if (extName == "VK_KHR_external_fence_win32")
	{
		functions.push_back("vkImportFenceWin32HandleKHR");
		functions.push_back("vkGetFenceWin32HandleKHR");
	}
	else if (extName == "VK_NV_external_memory_win32")
	{
		functions.push_back("vkGetMemoryWin32HandleNV");
	}
	else if (extName == "VK_EXT_full_screen_exclusive")
	{
		functions.push_back("vkAcquireFullScreenExclusiveModeEXT");
		functions.push_back("vkReleaseFullScreenExclusiveModeEXT");
		functions.push_back("vkGetDeviceGroupSurfacePresentModes2EXT");
	}
	else
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
	"VK_EXT_debug_report",
	"VK_NV_external_memory_capabilities",
	"VK_EXT_direct_mode_display",
	"VK_EXT_display_surface_counter",
	"VK_EXT_calibrated_timestamps",
	"VK_EXT_tooling_info",
	"VK_NV_cooperative_matrix",
	"VK_NV_coverage_reduction_mode",
	"VK_EXT_headless_surface",
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
	"VK_KHR_pipeline_executable_properties",
	"VK_EXT_debug_marker",
	"VK_EXT_transform_feedback",
	"VK_NVX_image_view_handle",
	"VK_AMD_draw_indirect_count",
	"VK_AMD_shader_info",
	"VK_EXT_conditional_rendering",
	"VK_NVX_device_generated_commands",
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
	"VK_ANDROID_external_memory_android_hardware_buffer",
	"VK_KHR_external_memory_win32",
	"VK_KHR_external_semaphore_win32",
	"VK_KHR_external_fence_win32",
	"VK_NV_external_memory_win32"
};
