/* WARNING: This is auto-generated file. Do not modify, since changes will
 * be lost! Modify the generating script instead.
 * This file was generated by /scripts/gen_framework.py
 */

template<class Variant, class... Types> struct extend_variant;
template<class... X, class... Types>
    struct extend_variant<std::variant<X...>, Types...> {
        typedef std::variant<X..., Types...> type;
};
template <class X, class Variant> struct variant_index;
template <class X, class... Types> struct variant_index<X, std::variant<X, Types...>>
    : std::integral_constant<std::size_t, 0> { };
template <class X, class Y, class... Types> struct variant_index<X, std::variant<Y, Types...>>
    : std::integral_constant<std::size_t, 1 + variant_index<X, std::variant<Types...>>::value> { };
template <class X> struct variant_index<X, std::variant<>> : std::integral_constant<std::size_t, 0> { };
template <typename X, typename Variant>
    constexpr std::size_t variant_index_v = variant_index<X, Variant>::value;
typedef std::variant<
    VkPhysicalDeviceTransformFeedbackFeaturesEXT
    , VkPhysicalDeviceDynamicRenderingFeatures
    , VkPhysicalDeviceCornerSampledImageFeaturesNV
    , VkPhysicalDeviceMultiviewFeatures
    , VkPhysicalDeviceTextureCompressionASTCHDRFeatures
    , VkPhysicalDeviceASTCDecodeFeaturesEXT
    , VkPhysicalDevicePipelineRobustnessFeatures
    , VkPhysicalDeviceConditionalRenderingFeaturesEXT
    , VkPhysicalDeviceShaderFloat16Int8Features
    , VkPhysicalDevice16BitStorageFeatures
    , VkPhysicalDeviceDepthClipEnableFeaturesEXT
    , VkPhysicalDeviceImagelessFramebufferFeatures
    , VkPhysicalDeviceRelaxedLineRasterizationFeaturesIMG
    , VkPhysicalDevicePerformanceQueryFeaturesKHR
    , VkPhysicalDeviceVariablePointersFeatures
    , VkPhysicalDeviceShaderEnqueueFeaturesAMDX
    , VkPhysicalDeviceInlineUniformBlockFeatures
    , VkPhysicalDeviceShaderBfloat16FeaturesKHR
    , VkPhysicalDeviceBlendOperationAdvancedFeaturesEXT
    , VkPhysicalDeviceAccelerationStructureFeaturesKHR
    , VkPhysicalDeviceRayTracingPipelineFeaturesKHR
    , VkPhysicalDeviceRayQueryFeaturesKHR
    , VkPhysicalDeviceShaderSMBuiltinsFeaturesNV
    , VkPhysicalDeviceSamplerYcbcrConversionFeatures
    , VkPhysicalDeviceDescriptorIndexingFeatures
    , VkPhysicalDevicePortabilitySubsetFeaturesKHR
    , VkPhysicalDeviceShadingRateImageFeaturesNV
    , VkPhysicalDeviceRepresentativeFragmentTestFeaturesNV
    , VkPhysicalDeviceShaderSubgroupExtendedTypesFeatures
    , VkPhysicalDevice8BitStorageFeatures
    , VkPhysicalDeviceShaderAtomicInt64Features
    , VkPhysicalDeviceShaderClockFeaturesKHR
    , VkPhysicalDeviceGlobalPriorityQueryFeatures
    , VkPhysicalDeviceMeshShaderFeaturesNV
    , VkPhysicalDeviceShaderImageFootprintFeaturesNV
    , VkPhysicalDeviceExclusiveScissorFeaturesNV
    , VkPhysicalDeviceTimelineSemaphoreFeatures
    , VkPhysicalDeviceShaderIntegerFunctions2FeaturesINTEL
    , VkPhysicalDeviceVulkanMemoryModelFeatures
    , VkPhysicalDeviceShaderTerminateInvocationFeatures
    , VkPhysicalDeviceFragmentDensityMapFeaturesEXT
    , VkPhysicalDeviceScalarBlockLayoutFeatures
    , VkPhysicalDeviceSubgroupSizeControlFeatures
    , VkPhysicalDeviceFragmentShadingRateFeaturesKHR
    , VkPhysicalDeviceCoherentMemoryFeaturesAMD
    , VkPhysicalDeviceDynamicRenderingLocalReadFeatures
    , VkPhysicalDeviceShaderImageAtomicInt64FeaturesEXT
    , VkPhysicalDeviceShaderQuadControlFeaturesKHR
    , VkPhysicalDeviceMemoryPriorityFeaturesEXT
    , VkPhysicalDeviceDedicatedAllocationImageAliasingFeaturesNV
    , VkPhysicalDeviceSeparateDepthStencilLayoutsFeatures
    , VkPhysicalDeviceBufferDeviceAddressFeaturesEXT
    , VkPhysicalDevicePresentWaitFeaturesKHR
    , VkPhysicalDeviceCooperativeMatrixFeaturesNV
    , VkPhysicalDeviceCoverageReductionModeFeaturesNV
    , VkPhysicalDeviceFragmentShaderInterlockFeaturesEXT
    , VkPhysicalDeviceYcbcrImageArraysFeaturesEXT
    , VkPhysicalDeviceUniformBufferStandardLayoutFeatures
    , VkPhysicalDeviceProvokingVertexFeaturesEXT
    , VkPhysicalDeviceBufferDeviceAddressFeatures
    , VkPhysicalDeviceShaderAtomicFloatFeaturesEXT
    , VkPhysicalDeviceHostQueryResetFeatures
    , VkPhysicalDeviceExtendedDynamicStateFeaturesEXT
    , VkPhysicalDevicePipelineExecutablePropertiesFeaturesKHR
    , VkPhysicalDeviceHostImageCopyFeatures
    , VkPhysicalDeviceMapMemoryPlacedFeaturesEXT
    , VkPhysicalDeviceShaderAtomicFloat2FeaturesEXT
    , VkPhysicalDeviceShaderDemoteToHelperInvocationFeatures
    , VkPhysicalDeviceDeviceGeneratedCommandsFeaturesNV
    , VkPhysicalDeviceInheritedViewportScissorFeaturesNV
    , VkPhysicalDeviceShaderIntegerDotProductFeatures
    , VkPhysicalDeviceTexelBufferAlignmentFeaturesEXT
    , VkPhysicalDeviceDepthBiasControlFeaturesEXT
    , VkPhysicalDeviceDeviceMemoryReportFeaturesEXT
    , VkPhysicalDeviceCustomBorderColorFeaturesEXT
    , VkPhysicalDevicePresentBarrierFeaturesNV
    , VkPhysicalDevicePresentIdFeaturesKHR
    , VkPhysicalDevicePrivateDataFeatures
    , VkPhysicalDevicePipelineCreationCacheControlFeatures
    , VkPhysicalDeviceDiagnosticsConfigFeaturesNV
    , VkPhysicalDeviceCudaKernelLaunchFeaturesNV
    , VkPhysicalDeviceTileShadingFeaturesQCOM
    , VkPhysicalDeviceSynchronization2Features
    , VkPhysicalDeviceDescriptorBufferFeaturesEXT
    , VkPhysicalDeviceGraphicsPipelineLibraryFeaturesEXT
    , VkPhysicalDeviceShaderEarlyAndLateFragmentTestsFeaturesAMD
    , VkPhysicalDeviceFragmentShaderBarycentricFeaturesKHR
    , VkPhysicalDeviceShaderSubgroupUniformControlFlowFeaturesKHR
    , VkPhysicalDeviceZeroInitializeWorkgroupMemoryFeatures
    , VkPhysicalDeviceFragmentShadingRateEnumsFeaturesNV
    , VkPhysicalDeviceRayTracingMotionBlurFeaturesNV
    , VkPhysicalDeviceMeshShaderFeaturesEXT
    , VkPhysicalDeviceYcbcr2Plane444FormatsFeaturesEXT
    , VkPhysicalDeviceFragmentDensityMap2FeaturesEXT
    , VkPhysicalDeviceImageRobustnessFeatures
    , VkPhysicalDeviceWorkgroupMemoryExplicitLayoutFeaturesKHR
    , VkPhysicalDeviceImageCompressionControlFeaturesEXT
    , VkPhysicalDeviceAttachmentFeedbackLoopLayoutFeaturesEXT
    , VkPhysicalDevice4444FormatsFeaturesEXT
    , VkPhysicalDeviceFaultFeaturesEXT
    , VkPhysicalDeviceRGBA10X6FormatsFeaturesEXT
    , VkPhysicalDeviceVertexInputDynamicStateFeaturesEXT
    , VkPhysicalDeviceAddressBindingReportFeaturesEXT
    , VkPhysicalDeviceDepthClipControlFeaturesEXT
    , VkPhysicalDevicePrimitiveTopologyListRestartFeaturesEXT
    , VkPhysicalDeviceSubpassShadingFeaturesHUAWEI
    , VkPhysicalDeviceInvocationMaskFeaturesHUAWEI
    , VkPhysicalDeviceExternalMemoryRDMAFeaturesNV
    , VkPhysicalDevicePipelinePropertiesFeaturesEXT
    , VkPhysicalDeviceFrameBoundaryFeaturesEXT
    , VkPhysicalDeviceMultisampledRenderToSingleSampledFeaturesEXT
    , VkPhysicalDeviceExtendedDynamicState2FeaturesEXT
    , VkPhysicalDeviceColorWriteEnableFeaturesEXT
    , VkPhysicalDevicePrimitivesGeneratedQueryFeaturesEXT
    , VkPhysicalDeviceRayTracingMaintenance1FeaturesKHR
    , VkPhysicalDeviceImageViewMinLodFeaturesEXT
    , VkPhysicalDeviceMultiDrawFeaturesEXT
    , VkPhysicalDeviceImage2DViewOf3DFeaturesEXT
    , VkPhysicalDeviceShaderTileImageFeaturesEXT
    , VkPhysicalDeviceOpacityMicromapFeaturesEXT
    , VkPhysicalDeviceDisplacementMicromapFeaturesNV
    , VkPhysicalDeviceClusterCullingShaderFeaturesHUAWEI
    , VkPhysicalDeviceBorderColorSwizzleFeaturesEXT
    , VkPhysicalDevicePageableDeviceLocalMemoryFeaturesEXT
    , VkPhysicalDeviceMaintenance4Features
    , VkPhysicalDeviceShaderSubgroupRotateFeatures
    , VkPhysicalDeviceSchedulingControlsFeaturesARM
    , VkPhysicalDeviceImageSlicedViewOf3DFeaturesEXT
    , VkPhysicalDeviceDescriptorSetHostMappingFeaturesVALVE
    , VkPhysicalDeviceNonSeamlessCubeMapFeaturesEXT
    , VkPhysicalDeviceRenderPassStripedFeaturesARM
    , VkPhysicalDeviceCopyMemoryIndirectFeaturesNV
    , VkPhysicalDeviceMemoryDecompressionFeaturesNV
    , VkPhysicalDeviceDeviceGeneratedCommandsComputeFeaturesNV
    , VkPhysicalDeviceRayTracingLinearSweptSpheresFeaturesNV
    , VkPhysicalDeviceLinearColorAttachmentFeaturesNV
    , VkPhysicalDeviceShaderMaximalReconvergenceFeaturesKHR
    , VkPhysicalDeviceImageCompressionControlSwapchainFeaturesEXT
    , VkPhysicalDeviceImageProcessingFeaturesQCOM
    , VkPhysicalDeviceNestedCommandBufferFeaturesEXT
    , VkPhysicalDeviceExtendedDynamicState3FeaturesEXT
    , VkPhysicalDeviceSubpassMergeFeedbackFeaturesEXT
    , VkPhysicalDeviceTensorFeaturesARM
    , VkPhysicalDeviceShaderModuleIdentifierFeaturesEXT
    , VkPhysicalDeviceRasterizationOrderAttachmentAccessFeaturesEXT
    , VkPhysicalDeviceOpticalFlowFeaturesNV
    , VkPhysicalDeviceLegacyDitheringFeaturesEXT
    , VkPhysicalDevicePipelineProtectedAccessFeatures
    , VkPhysicalDeviceExternalFormatResolveFeaturesANDROID
    , VkPhysicalDeviceMaintenance5Features
    , VkPhysicalDeviceAntiLagFeaturesAMD
    , VkPhysicalDevicePresentId2FeaturesKHR
    , VkPhysicalDevicePresentWait2FeaturesKHR
    , VkPhysicalDeviceRayTracingPositionFetchFeaturesKHR
    , VkPhysicalDeviceShaderObjectFeaturesEXT
    , VkPhysicalDevicePipelineBinaryFeaturesKHR
    , VkPhysicalDeviceTilePropertiesFeaturesQCOM
    , VkPhysicalDeviceAmigoProfilingFeaturesSEC
    , VkPhysicalDeviceSwapchainMaintenance1FeaturesKHR
    , VkPhysicalDeviceMultiviewPerViewViewportsFeaturesQCOM
    , VkPhysicalDeviceRayTracingInvocationReorderFeaturesNV
    , VkPhysicalDeviceCooperativeVectorFeaturesNV
    , VkPhysicalDeviceExtendedSparseAddressSpaceFeaturesNV
    , VkPhysicalDeviceMutableDescriptorTypeFeaturesEXT
    , VkPhysicalDeviceLegacyVertexAttributesFeaturesEXT
    , VkPhysicalDeviceShaderCoreBuiltinsFeaturesARM
    , VkPhysicalDevicePipelineLibraryGroupHandlesFeaturesEXT
    , VkPhysicalDeviceDynamicRenderingUnusedAttachmentsFeaturesEXT
    , VkPhysicalDeviceCooperativeMatrixFeaturesKHR
    , VkPhysicalDeviceDataGraphFeaturesARM
    , VkPhysicalDeviceMultiviewPerViewRenderAreasFeaturesQCOM
    , VkPhysicalDeviceComputeShaderDerivativesFeaturesKHR
    , VkPhysicalDeviceVideoEncodeAV1FeaturesKHR
    , VkPhysicalDeviceVideoDecodeVP9FeaturesKHR
    , VkPhysicalDeviceVideoMaintenance1FeaturesKHR
    , VkPhysicalDevicePerStageDescriptorSetFeaturesNV
    , VkPhysicalDeviceImageProcessing2FeaturesQCOM
    , VkPhysicalDeviceCubicWeightsFeaturesQCOM
    , VkPhysicalDeviceYcbcrDegammaFeaturesQCOM
    , VkPhysicalDeviceCubicClampFeaturesQCOM
    , VkPhysicalDeviceAttachmentFeedbackLoopDynamicStateFeaturesEXT
    , VkPhysicalDeviceVertexAttributeDivisorFeatures
    , VkPhysicalDeviceUnifiedImageLayoutsFeaturesKHR
    , VkPhysicalDeviceShaderFloatControls2Features
    , VkPhysicalDeviceExternalMemoryScreenBufferFeaturesQNX
    , VkPhysicalDeviceIndexTypeUint8Features
    , VkPhysicalDeviceLineRasterizationFeatures
    , VkPhysicalDeviceShaderExpectAssumeFeatures
    , VkPhysicalDeviceMaintenance6Features
    , VkPhysicalDeviceDescriptorPoolOverallocationFeaturesNV
    , VkPhysicalDeviceTileMemoryHeapFeaturesQCOM
    , VkPhysicalDeviceVideoEncodeIntraRefreshFeaturesKHR
    , VkPhysicalDeviceVideoEncodeQuantizationMapFeaturesKHR
    , VkPhysicalDeviceRawAccessChainsFeaturesNV
    , VkPhysicalDeviceShaderRelaxedExtendedInstructionFeaturesKHR
    , VkPhysicalDeviceCommandBufferInheritanceFeaturesNV
    , VkPhysicalDeviceMaintenance7FeaturesKHR
    , VkPhysicalDeviceShaderAtomicFloat16VectorFeaturesNV
    , VkPhysicalDeviceShaderReplicatedCompositesFeaturesEXT
    , VkPhysicalDeviceShaderFloat8FeaturesEXT
    , VkPhysicalDeviceRayTracingValidationFeaturesNV
    , VkPhysicalDeviceClusterAccelerationStructureFeaturesNV
    , VkPhysicalDevicePartitionedAccelerationStructureFeaturesNV
    , VkPhysicalDeviceDeviceGeneratedCommandsFeaturesEXT
    , VkPhysicalDeviceMaintenance8FeaturesKHR
    , VkPhysicalDeviceImageAlignmentControlFeaturesMESA
    , VkPhysicalDeviceDepthClampControlFeaturesEXT
    , VkPhysicalDeviceMaintenance9FeaturesKHR
    , VkPhysicalDeviceVideoMaintenance2FeaturesKHR
    , VkPhysicalDeviceHdrVividFeaturesHUAWEI
    , VkPhysicalDeviceCooperativeMatrix2FeaturesNV
    , VkPhysicalDevicePipelineOpacityMicromapFeaturesARM
    , VkPhysicalDeviceDepthClampZeroOneFeaturesKHR
    , VkPhysicalDeviceFormatPackFeaturesARM
    , VkPhysicalDeviceFragmentDensityMapLayeredFeaturesVALVE
    , VkPhysicalDeviceRobustness2FeaturesKHR
    , VkPhysicalDevicePresentMeteringFeaturesNV
    , VkPhysicalDeviceFragmentDensityMapOffsetFeaturesEXT
    , VkPhysicalDeviceZeroInitializeDeviceMemoryFeaturesEXT
    , VkPhysicalDevicePresentModeFifoLatestReadyFeaturesKHR
    , VkPhysicalDevicePipelineCacheIncrementalModeFeaturesSEC
    , VkPhysicalDeviceProtectedMemoryFeatures
    , VkPhysicalDeviceShaderDrawParametersFeatures
> ImplementedFeaturesVariant;
typedef typename extend_variant<ImplementedFeaturesVariant,
    VkPhysicalDeviceFeatures
#ifdef VK_API_VERSION_1_1
    , VkPhysicalDeviceFeatures2
    , VkPhysicalDeviceVulkan11Features
#endif
#ifdef VK_API_VERSION_1_2
    , VkPhysicalDeviceVulkan12Features
#endif
#if defined(VK_API_VERSION_1_3) && !defined(CTS_USES_VULKANSC)
    , VkPhysicalDeviceVulkan13Features
#endif
#if defined(VK_API_VERSION_1_4) && !defined(CTS_USES_VULKANSC)
    , VkPhysicalDeviceVulkan14Features
#endif
#ifdef CTS_USES_VULKANSC
    , VkFaultCallbackInfo
    , VkPhysicalDeviceVulkanSC10Features
    , VkDeviceObjectReservationCreateInfo
#endif
>::type FullFeaturesVariant;
template <class> VkStructureType feature2sType;
template <> inline constexpr VkStructureType feature2sType<VkPhysicalDeviceFeatures> = VK_STRUCTURE_TYPE_MAX_ENUM;
template <> inline constexpr VkStructureType feature2sType<VkPhysicalDeviceFeatures2> = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
template <> inline constexpr VkStructureType feature2sType<VkPhysicalDeviceVulkan11Features> = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
template <> inline constexpr VkStructureType feature2sType<VkPhysicalDeviceVulkan12Features> = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
#if defined(VK_API_VERSION_1_3) && !defined(CTS_USES_VULKANSC)
template <> inline constexpr VkStructureType feature2sType<VkPhysicalDeviceVulkan13Features> = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
#endif
#if defined(VK_API_VERSION_1_4) && !defined(CTS_USES_VULKANSC)
template <> inline constexpr VkStructureType feature2sType<VkPhysicalDeviceVulkan14Features> = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES;
#endif
#ifdef CTS_USES_VULKANSC
template <> inline constexpr VkStructureType feature2sType<VkFaultCallbackInfo> = VK_STRUCTURE_TYPE_FAULT_CALLBACK_INFO;
template <> inline constexpr VkStructureType feature2sType<VkPhysicalDeviceVulkanSC10Features> = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_SC_1_0_FEATURES;
template <> inline constexpr VkStructureType feature2sType<VkDeviceObjectReservationCreateInfo> = VK_STRUCTURE_TYPE_DEVICE_OBJECT_RESERVATION_CREATE_INFO;
#endif
template <class X> VkStructureType getFeatureSType() {
if constexpr (variant_index_v<X, ImplementedFeaturesVariant> < std::variant_size_v<ImplementedFeaturesVariant>)
    return vk::makeFeatureDesc<X>().sType;
else
    return feature2sType<X>;
}
template<class, class = void>
    struct hasPnextOfVoidPtr : std::false_type {};
template<class X>
    struct hasPnextOfVoidPtr<X, std::void_t<decltype(std::declval<X>().pNext)>>
        : std::integral_constant<bool,
              std::is_same<decltype(std::declval<X>().pNext), void*>::value ||
              std::is_same<decltype(std::declval<X>().pNext), const void*>::value> {};
