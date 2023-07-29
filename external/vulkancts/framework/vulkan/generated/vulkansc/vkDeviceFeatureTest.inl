/* WARNING: This is auto-generated file. Do not modify, since changes will
 * be lost! Modify the generating script instead.
 */

if (const void* featuresStruct = findStructureInChain(const_cast<const void*>(deviceFeatures2.pNext), getStructureType<VkPhysicalDevice16BitStorageFeatures>()))
{
    static const Feature features[] =
    {
        FEATURE_ITEM (VkPhysicalDevice16BitStorageFeatures, storageBuffer16BitAccess),
        FEATURE_ITEM (VkPhysicalDevice16BitStorageFeatures, uniformAndStorageBuffer16BitAccess),
        FEATURE_ITEM (VkPhysicalDevice16BitStorageFeatures, storagePushConstant16),
        FEATURE_ITEM (VkPhysicalDevice16BitStorageFeatures, storageInputOutput16),
    };
    auto* supportedFeatures = reinterpret_cast<const VkPhysicalDevice16BitStorageFeatures*>(featuresStruct);
    checkFeatures(vkp, instance, instanceDriver, physicalDevice, 4, features, supportedFeatures, queueFamilyIndex, queueCount, queuePriority, numErrors, resultCollector, &extensionNames, emptyDeviceFeatures, memReservationStatMax, isSubProcess);
}

if (const void* featuresStruct = findStructureInChain(const_cast<const void*>(deviceFeatures2.pNext), getStructureType<VkPhysicalDeviceMultiviewFeatures>()))
{
    static const Feature features[] =
    {
        FEATURE_ITEM (VkPhysicalDeviceMultiviewFeatures, multiview),
        FEATURE_ITEM (VkPhysicalDeviceMultiviewFeatures, multiviewGeometryShader),
        FEATURE_ITEM (VkPhysicalDeviceMultiviewFeatures, multiviewTessellationShader),
    };
    auto* supportedFeatures = reinterpret_cast<const VkPhysicalDeviceMultiviewFeatures*>(featuresStruct);
    checkFeatures(vkp, instance, instanceDriver, physicalDevice, 3, features, supportedFeatures, queueFamilyIndex, queueCount, queuePriority, numErrors, resultCollector, &extensionNames, emptyDeviceFeatures, memReservationStatMax, isSubProcess);
}

if (const void* featuresStruct = findStructureInChain(const_cast<const void*>(deviceFeatures2.pNext), getStructureType<VkPhysicalDeviceVariablePointersFeatures>()))
{
    static const Feature features[] =
    {
        FEATURE_ITEM (VkPhysicalDeviceVariablePointersFeatures, variablePointersStorageBuffer),
        FEATURE_ITEM (VkPhysicalDeviceVariablePointersFeatures, variablePointers),
    };
    auto* supportedFeatures = reinterpret_cast<const VkPhysicalDeviceVariablePointersFeatures*>(featuresStruct);
    checkFeatures(vkp, instance, instanceDriver, physicalDevice, 2, features, supportedFeatures, queueFamilyIndex, queueCount, queuePriority, numErrors, resultCollector, &extensionNames, emptyDeviceFeatures, memReservationStatMax, isSubProcess);
}

if (const void* featuresStruct = findStructureInChain(const_cast<const void*>(deviceFeatures2.pNext), getStructureType<VkPhysicalDeviceProtectedMemoryFeatures>()))
{
    static const Feature features[] =
    {
        FEATURE_ITEM (VkPhysicalDeviceProtectedMemoryFeatures, protectedMemory),
    };
    auto* supportedFeatures = reinterpret_cast<const VkPhysicalDeviceProtectedMemoryFeatures*>(featuresStruct);
    checkFeatures(vkp, instance, instanceDriver, physicalDevice, 1, features, supportedFeatures, queueFamilyIndex, queueCount, queuePriority, numErrors, resultCollector, &extensionNames, emptyDeviceFeatures, memReservationStatMax, isSubProcess);
}

if (const void* featuresStruct = findStructureInChain(const_cast<const void*>(deviceFeatures2.pNext), getStructureType<VkPhysicalDeviceSamplerYcbcrConversionFeatures>()))
{
    static const Feature features[] =
    {
        FEATURE_ITEM (VkPhysicalDeviceSamplerYcbcrConversionFeatures, samplerYcbcrConversion),
    };
    auto* supportedFeatures = reinterpret_cast<const VkPhysicalDeviceSamplerYcbcrConversionFeatures*>(featuresStruct);
    checkFeatures(vkp, instance, instanceDriver, physicalDevice, 1, features, supportedFeatures, queueFamilyIndex, queueCount, queuePriority, numErrors, resultCollector, &extensionNames, emptyDeviceFeatures, memReservationStatMax, isSubProcess);
}

if (const void* featuresStruct = findStructureInChain(const_cast<const void*>(deviceFeatures2.pNext), getStructureType<VkPhysicalDeviceShaderDrawParametersFeatures>()))
{
    static const Feature features[] =
    {
        FEATURE_ITEM (VkPhysicalDeviceShaderDrawParametersFeatures, shaderDrawParameters),
    };
    auto* supportedFeatures = reinterpret_cast<const VkPhysicalDeviceShaderDrawParametersFeatures*>(featuresStruct);
    checkFeatures(vkp, instance, instanceDriver, physicalDevice, 1, features, supportedFeatures, queueFamilyIndex, queueCount, queuePriority, numErrors, resultCollector, &extensionNames, emptyDeviceFeatures, memReservationStatMax, isSubProcess);
}

if (const void* featuresStruct = findStructureInChain(const_cast<const void*>(deviceFeatures2.pNext), getStructureType<VkPhysicalDeviceVulkan11Features>()))
{
    static const Feature features[] =
    {
        FEATURE_ITEM (VkPhysicalDeviceVulkan11Features, storageBuffer16BitAccess),
        FEATURE_ITEM (VkPhysicalDeviceVulkan11Features, uniformAndStorageBuffer16BitAccess),
        FEATURE_ITEM (VkPhysicalDeviceVulkan11Features, storagePushConstant16),
        FEATURE_ITEM (VkPhysicalDeviceVulkan11Features, storageInputOutput16),
        FEATURE_ITEM (VkPhysicalDeviceVulkan11Features, multiview),
        FEATURE_ITEM (VkPhysicalDeviceVulkan11Features, multiviewGeometryShader),
        FEATURE_ITEM (VkPhysicalDeviceVulkan11Features, multiviewTessellationShader),
        FEATURE_ITEM (VkPhysicalDeviceVulkan11Features, variablePointersStorageBuffer),
        FEATURE_ITEM (VkPhysicalDeviceVulkan11Features, variablePointers),
        FEATURE_ITEM (VkPhysicalDeviceVulkan11Features, protectedMemory),
        FEATURE_ITEM (VkPhysicalDeviceVulkan11Features, samplerYcbcrConversion),
        FEATURE_ITEM (VkPhysicalDeviceVulkan11Features, shaderDrawParameters),
    };
    auto* supportedFeatures = reinterpret_cast<const VkPhysicalDeviceVulkan11Features*>(featuresStruct);
    checkFeatures(vkp, instance, instanceDriver, physicalDevice, 12, features, supportedFeatures, queueFamilyIndex, queueCount, queuePriority, numErrors, resultCollector, DE_NULL, emptyDeviceFeatures, memReservationStatMax, isSubProcess);
}

if (const void* featuresStruct = findStructureInChain(const_cast<const void*>(deviceFeatures2.pNext), getStructureType<VkPhysicalDeviceVulkan12Features>()))
{
    static const Feature features[] =
    {
        FEATURE_ITEM (VkPhysicalDeviceVulkan12Features, samplerMirrorClampToEdge),
        FEATURE_ITEM (VkPhysicalDeviceVulkan12Features, drawIndirectCount),
        FEATURE_ITEM (VkPhysicalDeviceVulkan12Features, storageBuffer8BitAccess),
        FEATURE_ITEM (VkPhysicalDeviceVulkan12Features, uniformAndStorageBuffer8BitAccess),
        FEATURE_ITEM (VkPhysicalDeviceVulkan12Features, storagePushConstant8),
        FEATURE_ITEM (VkPhysicalDeviceVulkan12Features, shaderBufferInt64Atomics),
        FEATURE_ITEM (VkPhysicalDeviceVulkan12Features, shaderSharedInt64Atomics),
        FEATURE_ITEM (VkPhysicalDeviceVulkan12Features, shaderFloat16),
        FEATURE_ITEM (VkPhysicalDeviceVulkan12Features, shaderInt8),
        FEATURE_ITEM (VkPhysicalDeviceVulkan12Features, descriptorIndexing),
        FEATURE_ITEM (VkPhysicalDeviceVulkan12Features, shaderInputAttachmentArrayDynamicIndexing),
        FEATURE_ITEM (VkPhysicalDeviceVulkan12Features, shaderUniformTexelBufferArrayDynamicIndexing),
        FEATURE_ITEM (VkPhysicalDeviceVulkan12Features, shaderStorageTexelBufferArrayDynamicIndexing),
        FEATURE_ITEM (VkPhysicalDeviceVulkan12Features, shaderUniformBufferArrayNonUniformIndexing),
        FEATURE_ITEM (VkPhysicalDeviceVulkan12Features, shaderSampledImageArrayNonUniformIndexing),
        FEATURE_ITEM (VkPhysicalDeviceVulkan12Features, shaderStorageBufferArrayNonUniformIndexing),
        FEATURE_ITEM (VkPhysicalDeviceVulkan12Features, shaderStorageImageArrayNonUniformIndexing),
        FEATURE_ITEM (VkPhysicalDeviceVulkan12Features, shaderInputAttachmentArrayNonUniformIndexing),
        FEATURE_ITEM (VkPhysicalDeviceVulkan12Features, shaderUniformTexelBufferArrayNonUniformIndexing),
        FEATURE_ITEM (VkPhysicalDeviceVulkan12Features, shaderStorageTexelBufferArrayNonUniformIndexing),
        FEATURE_ITEM (VkPhysicalDeviceVulkan12Features, descriptorBindingUniformBufferUpdateAfterBind),
        FEATURE_ITEM (VkPhysicalDeviceVulkan12Features, descriptorBindingSampledImageUpdateAfterBind),
        FEATURE_ITEM (VkPhysicalDeviceVulkan12Features, descriptorBindingStorageImageUpdateAfterBind),
        FEATURE_ITEM (VkPhysicalDeviceVulkan12Features, descriptorBindingStorageBufferUpdateAfterBind),
        FEATURE_ITEM (VkPhysicalDeviceVulkan12Features, descriptorBindingUniformTexelBufferUpdateAfterBind),
        FEATURE_ITEM (VkPhysicalDeviceVulkan12Features, descriptorBindingStorageTexelBufferUpdateAfterBind),
        FEATURE_ITEM (VkPhysicalDeviceVulkan12Features, descriptorBindingUpdateUnusedWhilePending),
        FEATURE_ITEM (VkPhysicalDeviceVulkan12Features, descriptorBindingPartiallyBound),
        FEATURE_ITEM (VkPhysicalDeviceVulkan12Features, descriptorBindingVariableDescriptorCount),
        FEATURE_ITEM (VkPhysicalDeviceVulkan12Features, runtimeDescriptorArray),
        FEATURE_ITEM (VkPhysicalDeviceVulkan12Features, samplerFilterMinmax),
        FEATURE_ITEM (VkPhysicalDeviceVulkan12Features, scalarBlockLayout),
        FEATURE_ITEM (VkPhysicalDeviceVulkan12Features, imagelessFramebuffer),
        FEATURE_ITEM (VkPhysicalDeviceVulkan12Features, uniformBufferStandardLayout),
        FEATURE_ITEM (VkPhysicalDeviceVulkan12Features, shaderSubgroupExtendedTypes),
        FEATURE_ITEM (VkPhysicalDeviceVulkan12Features, separateDepthStencilLayouts),
        FEATURE_ITEM (VkPhysicalDeviceVulkan12Features, hostQueryReset),
        FEATURE_ITEM (VkPhysicalDeviceVulkan12Features, timelineSemaphore),
        FEATURE_ITEM (VkPhysicalDeviceVulkan12Features, bufferDeviceAddress),
        FEATURE_ITEM (VkPhysicalDeviceVulkan12Features, bufferDeviceAddressCaptureReplay),
        FEATURE_ITEM (VkPhysicalDeviceVulkan12Features, bufferDeviceAddressMultiDevice),
        FEATURE_ITEM (VkPhysicalDeviceVulkan12Features, vulkanMemoryModel),
        FEATURE_ITEM (VkPhysicalDeviceVulkan12Features, vulkanMemoryModelDeviceScope),
        FEATURE_ITEM (VkPhysicalDeviceVulkan12Features, vulkanMemoryModelAvailabilityVisibilityChains),
        FEATURE_ITEM (VkPhysicalDeviceVulkan12Features, shaderOutputViewportIndex),
        FEATURE_ITEM (VkPhysicalDeviceVulkan12Features, shaderOutputLayer),
        FEATURE_ITEM (VkPhysicalDeviceVulkan12Features, subgroupBroadcastDynamicId),
    };
    auto* supportedFeatures = reinterpret_cast<const VkPhysicalDeviceVulkan12Features*>(featuresStruct);
    checkFeatures(vkp, instance, instanceDriver, physicalDevice, 47, features, supportedFeatures, queueFamilyIndex, queueCount, queuePriority, numErrors, resultCollector, DE_NULL, emptyDeviceFeatures, memReservationStatMax, isSubProcess);
}

if (const void* featuresStruct = findStructureInChain(const_cast<const void*>(deviceFeatures2.pNext), getStructureType<VkPhysicalDevice8BitStorageFeatures>()))
{
    static const Feature features[] =
    {
        FEATURE_ITEM (VkPhysicalDevice8BitStorageFeatures, storageBuffer8BitAccess),
        FEATURE_ITEM (VkPhysicalDevice8BitStorageFeatures, uniformAndStorageBuffer8BitAccess),
        FEATURE_ITEM (VkPhysicalDevice8BitStorageFeatures, storagePushConstant8),
    };
    auto* supportedFeatures = reinterpret_cast<const VkPhysicalDevice8BitStorageFeatures*>(featuresStruct);
    checkFeatures(vkp, instance, instanceDriver, physicalDevice, 3, features, supportedFeatures, queueFamilyIndex, queueCount, queuePriority, numErrors, resultCollector, &extensionNames, emptyDeviceFeatures, memReservationStatMax, isSubProcess);
}

if (const void* featuresStruct = findStructureInChain(const_cast<const void*>(deviceFeatures2.pNext), getStructureType<VkPhysicalDeviceShaderAtomicInt64Features>()))
{
    static const Feature features[] =
    {
        FEATURE_ITEM (VkPhysicalDeviceShaderAtomicInt64Features, shaderBufferInt64Atomics),
        FEATURE_ITEM (VkPhysicalDeviceShaderAtomicInt64Features, shaderSharedInt64Atomics),
    };
    auto* supportedFeatures = reinterpret_cast<const VkPhysicalDeviceShaderAtomicInt64Features*>(featuresStruct);
    checkFeatures(vkp, instance, instanceDriver, physicalDevice, 2, features, supportedFeatures, queueFamilyIndex, queueCount, queuePriority, numErrors, resultCollector, &extensionNames, emptyDeviceFeatures, memReservationStatMax, isSubProcess);
}

if (const void* featuresStruct = findStructureInChain(const_cast<const void*>(deviceFeatures2.pNext), getStructureType<VkPhysicalDeviceShaderFloat16Int8Features>()))
{
    static const Feature features[] =
    {
        FEATURE_ITEM (VkPhysicalDeviceShaderFloat16Int8Features, shaderFloat16),
        FEATURE_ITEM (VkPhysicalDeviceShaderFloat16Int8Features, shaderInt8),
    };
    auto* supportedFeatures = reinterpret_cast<const VkPhysicalDeviceShaderFloat16Int8Features*>(featuresStruct);
    checkFeatures(vkp, instance, instanceDriver, physicalDevice, 2, features, supportedFeatures, queueFamilyIndex, queueCount, queuePriority, numErrors, resultCollector, &extensionNames, emptyDeviceFeatures, memReservationStatMax, isSubProcess);
}

if (const void* featuresStruct = findStructureInChain(const_cast<const void*>(deviceFeatures2.pNext), getStructureType<VkPhysicalDeviceDescriptorIndexingFeatures>()))
{
    static const Feature features[] =
    {
        FEATURE_ITEM (VkPhysicalDeviceDescriptorIndexingFeatures, shaderInputAttachmentArrayDynamicIndexing),
        FEATURE_ITEM (VkPhysicalDeviceDescriptorIndexingFeatures, shaderUniformTexelBufferArrayDynamicIndexing),
        FEATURE_ITEM (VkPhysicalDeviceDescriptorIndexingFeatures, shaderStorageTexelBufferArrayDynamicIndexing),
        FEATURE_ITEM (VkPhysicalDeviceDescriptorIndexingFeatures, shaderUniformBufferArrayNonUniformIndexing),
        FEATURE_ITEM (VkPhysicalDeviceDescriptorIndexingFeatures, shaderSampledImageArrayNonUniformIndexing),
        FEATURE_ITEM (VkPhysicalDeviceDescriptorIndexingFeatures, shaderStorageBufferArrayNonUniformIndexing),
        FEATURE_ITEM (VkPhysicalDeviceDescriptorIndexingFeatures, shaderStorageImageArrayNonUniformIndexing),
        FEATURE_ITEM (VkPhysicalDeviceDescriptorIndexingFeatures, shaderInputAttachmentArrayNonUniformIndexing),
        FEATURE_ITEM (VkPhysicalDeviceDescriptorIndexingFeatures, shaderUniformTexelBufferArrayNonUniformIndexing),
        FEATURE_ITEM (VkPhysicalDeviceDescriptorIndexingFeatures, shaderStorageTexelBufferArrayNonUniformIndexing),
        FEATURE_ITEM (VkPhysicalDeviceDescriptorIndexingFeatures, descriptorBindingUniformBufferUpdateAfterBind),
        FEATURE_ITEM (VkPhysicalDeviceDescriptorIndexingFeatures, descriptorBindingSampledImageUpdateAfterBind),
        FEATURE_ITEM (VkPhysicalDeviceDescriptorIndexingFeatures, descriptorBindingStorageImageUpdateAfterBind),
        FEATURE_ITEM (VkPhysicalDeviceDescriptorIndexingFeatures, descriptorBindingStorageBufferUpdateAfterBind),
        FEATURE_ITEM (VkPhysicalDeviceDescriptorIndexingFeatures, descriptorBindingUniformTexelBufferUpdateAfterBind),
        FEATURE_ITEM (VkPhysicalDeviceDescriptorIndexingFeatures, descriptorBindingStorageTexelBufferUpdateAfterBind),
        FEATURE_ITEM (VkPhysicalDeviceDescriptorIndexingFeatures, descriptorBindingUpdateUnusedWhilePending),
        FEATURE_ITEM (VkPhysicalDeviceDescriptorIndexingFeatures, descriptorBindingPartiallyBound),
        FEATURE_ITEM (VkPhysicalDeviceDescriptorIndexingFeatures, descriptorBindingVariableDescriptorCount),
        FEATURE_ITEM (VkPhysicalDeviceDescriptorIndexingFeatures, runtimeDescriptorArray),
    };
    auto* supportedFeatures = reinterpret_cast<const VkPhysicalDeviceDescriptorIndexingFeatures*>(featuresStruct);
    checkFeatures(vkp, instance, instanceDriver, physicalDevice, 20, features, supportedFeatures, queueFamilyIndex, queueCount, queuePriority, numErrors, resultCollector, &extensionNames, emptyDeviceFeatures, memReservationStatMax, isSubProcess);
}

if (const void* featuresStruct = findStructureInChain(const_cast<const void*>(deviceFeatures2.pNext), getStructureType<VkPhysicalDeviceScalarBlockLayoutFeatures>()))
{
    static const Feature features[] =
    {
        FEATURE_ITEM (VkPhysicalDeviceScalarBlockLayoutFeatures, scalarBlockLayout),
    };
    auto* supportedFeatures = reinterpret_cast<const VkPhysicalDeviceScalarBlockLayoutFeatures*>(featuresStruct);
    checkFeatures(vkp, instance, instanceDriver, physicalDevice, 1, features, supportedFeatures, queueFamilyIndex, queueCount, queuePriority, numErrors, resultCollector, &extensionNames, emptyDeviceFeatures, memReservationStatMax, isSubProcess);
}

if (const void* featuresStruct = findStructureInChain(const_cast<const void*>(deviceFeatures2.pNext), getStructureType<VkPhysicalDeviceVulkanMemoryModelFeatures>()))
{
    static const Feature features[] =
    {
        FEATURE_ITEM (VkPhysicalDeviceVulkanMemoryModelFeatures, vulkanMemoryModel),
        FEATURE_ITEM (VkPhysicalDeviceVulkanMemoryModelFeatures, vulkanMemoryModelDeviceScope),
        FEATURE_ITEM (VkPhysicalDeviceVulkanMemoryModelFeatures, vulkanMemoryModelAvailabilityVisibilityChains),
    };
    auto* supportedFeatures = reinterpret_cast<const VkPhysicalDeviceVulkanMemoryModelFeatures*>(featuresStruct);
    checkFeatures(vkp, instance, instanceDriver, physicalDevice, 3, features, supportedFeatures, queueFamilyIndex, queueCount, queuePriority, numErrors, resultCollector, &extensionNames, emptyDeviceFeatures, memReservationStatMax, isSubProcess);
}

if (const void* featuresStruct = findStructureInChain(const_cast<const void*>(deviceFeatures2.pNext), getStructureType<VkPhysicalDeviceImagelessFramebufferFeatures>()))
{
    static const Feature features[] =
    {
        FEATURE_ITEM (VkPhysicalDeviceImagelessFramebufferFeatures, imagelessFramebuffer),
    };
    auto* supportedFeatures = reinterpret_cast<const VkPhysicalDeviceImagelessFramebufferFeatures*>(featuresStruct);
    checkFeatures(vkp, instance, instanceDriver, physicalDevice, 1, features, supportedFeatures, queueFamilyIndex, queueCount, queuePriority, numErrors, resultCollector, &extensionNames, emptyDeviceFeatures, memReservationStatMax, isSubProcess);
}

if (const void* featuresStruct = findStructureInChain(const_cast<const void*>(deviceFeatures2.pNext), getStructureType<VkPhysicalDeviceUniformBufferStandardLayoutFeatures>()))
{
    static const Feature features[] =
    {
        FEATURE_ITEM (VkPhysicalDeviceUniformBufferStandardLayoutFeatures, uniformBufferStandardLayout),
    };
    auto* supportedFeatures = reinterpret_cast<const VkPhysicalDeviceUniformBufferStandardLayoutFeatures*>(featuresStruct);
    checkFeatures(vkp, instance, instanceDriver, physicalDevice, 1, features, supportedFeatures, queueFamilyIndex, queueCount, queuePriority, numErrors, resultCollector, &extensionNames, emptyDeviceFeatures, memReservationStatMax, isSubProcess);
}

if (const void* featuresStruct = findStructureInChain(const_cast<const void*>(deviceFeatures2.pNext), getStructureType<VkPhysicalDeviceShaderSubgroupExtendedTypesFeatures>()))
{
    static const Feature features[] =
    {
        FEATURE_ITEM (VkPhysicalDeviceShaderSubgroupExtendedTypesFeatures, shaderSubgroupExtendedTypes),
    };
    auto* supportedFeatures = reinterpret_cast<const VkPhysicalDeviceShaderSubgroupExtendedTypesFeatures*>(featuresStruct);
    checkFeatures(vkp, instance, instanceDriver, physicalDevice, 1, features, supportedFeatures, queueFamilyIndex, queueCount, queuePriority, numErrors, resultCollector, &extensionNames, emptyDeviceFeatures, memReservationStatMax, isSubProcess);
}

if (const void* featuresStruct = findStructureInChain(const_cast<const void*>(deviceFeatures2.pNext), getStructureType<VkPhysicalDeviceSeparateDepthStencilLayoutsFeatures>()))
{
    static const Feature features[] =
    {
        FEATURE_ITEM (VkPhysicalDeviceSeparateDepthStencilLayoutsFeatures, separateDepthStencilLayouts),
    };
    auto* supportedFeatures = reinterpret_cast<const VkPhysicalDeviceSeparateDepthStencilLayoutsFeatures*>(featuresStruct);
    checkFeatures(vkp, instance, instanceDriver, physicalDevice, 1, features, supportedFeatures, queueFamilyIndex, queueCount, queuePriority, numErrors, resultCollector, &extensionNames, emptyDeviceFeatures, memReservationStatMax, isSubProcess);
}

if (const void* featuresStruct = findStructureInChain(const_cast<const void*>(deviceFeatures2.pNext), getStructureType<VkPhysicalDeviceHostQueryResetFeatures>()))
{
    static const Feature features[] =
    {
        FEATURE_ITEM (VkPhysicalDeviceHostQueryResetFeatures, hostQueryReset),
    };
    auto* supportedFeatures = reinterpret_cast<const VkPhysicalDeviceHostQueryResetFeatures*>(featuresStruct);
    checkFeatures(vkp, instance, instanceDriver, physicalDevice, 1, features, supportedFeatures, queueFamilyIndex, queueCount, queuePriority, numErrors, resultCollector, &extensionNames, emptyDeviceFeatures, memReservationStatMax, isSubProcess);
}

if (const void* featuresStruct = findStructureInChain(const_cast<const void*>(deviceFeatures2.pNext), getStructureType<VkPhysicalDeviceTimelineSemaphoreFeatures>()))
{
    static const Feature features[] =
    {
        FEATURE_ITEM (VkPhysicalDeviceTimelineSemaphoreFeatures, timelineSemaphore),
    };
    auto* supportedFeatures = reinterpret_cast<const VkPhysicalDeviceTimelineSemaphoreFeatures*>(featuresStruct);
    checkFeatures(vkp, instance, instanceDriver, physicalDevice, 1, features, supportedFeatures, queueFamilyIndex, queueCount, queuePriority, numErrors, resultCollector, &extensionNames, emptyDeviceFeatures, memReservationStatMax, isSubProcess);
}

if (const void* featuresStruct = findStructureInChain(const_cast<const void*>(deviceFeatures2.pNext), getStructureType<VkPhysicalDeviceBufferDeviceAddressFeatures>()))
{
    static const Feature features[] =
    {
        FEATURE_ITEM (VkPhysicalDeviceBufferDeviceAddressFeatures, bufferDeviceAddress),
        FEATURE_ITEM (VkPhysicalDeviceBufferDeviceAddressFeatures, bufferDeviceAddressCaptureReplay),
        FEATURE_ITEM (VkPhysicalDeviceBufferDeviceAddressFeatures, bufferDeviceAddressMultiDevice),
    };
    auto* supportedFeatures = reinterpret_cast<const VkPhysicalDeviceBufferDeviceAddressFeatures*>(featuresStruct);
    checkFeatures(vkp, instance, instanceDriver, physicalDevice, 3, features, supportedFeatures, queueFamilyIndex, queueCount, queuePriority, numErrors, resultCollector, &extensionNames, emptyDeviceFeatures, memReservationStatMax, isSubProcess);
}

if (const void* featuresStruct = findStructureInChain(const_cast<const void*>(deviceFeatures2.pNext), getStructureType<VkPhysicalDeviceVulkanSC10Features>()))
{
    static const Feature features[] =
    {
        FEATURE_ITEM (VkPhysicalDeviceVulkanSC10Features, shaderAtomicInstructions),
    };
    auto* supportedFeatures = reinterpret_cast<const VkPhysicalDeviceVulkanSC10Features*>(featuresStruct);
    checkFeatures(vkp, instance, instanceDriver, physicalDevice, 1, features, supportedFeatures, queueFamilyIndex, queueCount, queuePriority, numErrors, resultCollector, &extensionNames, emptyDeviceFeatures, memReservationStatMax, isSubProcess);
}

if (const void* featuresStruct = findStructureInChain(const_cast<const void*>(deviceFeatures2.pNext), getStructureType<VkPhysicalDevicePerformanceQueryFeaturesKHR>()))
{
    static const Feature features[] =
    {
        FEATURE_ITEM (VkPhysicalDevicePerformanceQueryFeaturesKHR, performanceCounterQueryPools),
        FEATURE_ITEM (VkPhysicalDevicePerformanceQueryFeaturesKHR, performanceCounterMultipleQueryPools),
    };
    auto* supportedFeatures = reinterpret_cast<const VkPhysicalDevicePerformanceQueryFeaturesKHR*>(featuresStruct);
    checkFeatures(vkp, instance, instanceDriver, physicalDevice, 2, features, supportedFeatures, queueFamilyIndex, queueCount, queuePriority, numErrors, resultCollector, &extensionNames, emptyDeviceFeatures, memReservationStatMax, isSubProcess);
}

if (const void* featuresStruct = findStructureInChain(const_cast<const void*>(deviceFeatures2.pNext), getStructureType<VkPhysicalDeviceShaderClockFeaturesKHR>()))
{
    static const Feature features[] =
    {
        FEATURE_ITEM (VkPhysicalDeviceShaderClockFeaturesKHR, shaderSubgroupClock),
        FEATURE_ITEM (VkPhysicalDeviceShaderClockFeaturesKHR, shaderDeviceClock),
    };
    auto* supportedFeatures = reinterpret_cast<const VkPhysicalDeviceShaderClockFeaturesKHR*>(featuresStruct);
    checkFeatures(vkp, instance, instanceDriver, physicalDevice, 2, features, supportedFeatures, queueFamilyIndex, queueCount, queuePriority, numErrors, resultCollector, &extensionNames, emptyDeviceFeatures, memReservationStatMax, isSubProcess);
}

if (const void* featuresStruct = findStructureInChain(const_cast<const void*>(deviceFeatures2.pNext), getStructureType<VkPhysicalDeviceShaderTerminateInvocationFeaturesKHR>()))
{
    static const Feature features[] =
    {
        FEATURE_ITEM (VkPhysicalDeviceShaderTerminateInvocationFeaturesKHR, shaderTerminateInvocation),
    };
    auto* supportedFeatures = reinterpret_cast<const VkPhysicalDeviceShaderTerminateInvocationFeaturesKHR*>(featuresStruct);
    checkFeatures(vkp, instance, instanceDriver, physicalDevice, 1, features, supportedFeatures, queueFamilyIndex, queueCount, queuePriority, numErrors, resultCollector, &extensionNames, emptyDeviceFeatures, memReservationStatMax, isSubProcess);
}

if (const void* featuresStruct = findStructureInChain(const_cast<const void*>(deviceFeatures2.pNext), getStructureType<VkPhysicalDeviceFragmentShadingRateFeaturesKHR>()))
{
    static const Feature features[] =
    {
        FEATURE_ITEM (VkPhysicalDeviceFragmentShadingRateFeaturesKHR, pipelineFragmentShadingRate),
        FEATURE_ITEM (VkPhysicalDeviceFragmentShadingRateFeaturesKHR, primitiveFragmentShadingRate),
        FEATURE_ITEM (VkPhysicalDeviceFragmentShadingRateFeaturesKHR, attachmentFragmentShadingRate),
    };
    auto* supportedFeatures = reinterpret_cast<const VkPhysicalDeviceFragmentShadingRateFeaturesKHR*>(featuresStruct);
    checkFeatures(vkp, instance, instanceDriver, physicalDevice, 3, features, supportedFeatures, queueFamilyIndex, queueCount, queuePriority, numErrors, resultCollector, &extensionNames, emptyDeviceFeatures, memReservationStatMax, isSubProcess);
}

if (const void* featuresStruct = findStructureInChain(const_cast<const void*>(deviceFeatures2.pNext), getStructureType<VkPhysicalDeviceSynchronization2FeaturesKHR>()))
{
    static const Feature features[] =
    {
        FEATURE_ITEM (VkPhysicalDeviceSynchronization2FeaturesKHR, synchronization2),
    };
    auto* supportedFeatures = reinterpret_cast<const VkPhysicalDeviceSynchronization2FeaturesKHR*>(featuresStruct);
    checkFeatures(vkp, instance, instanceDriver, physicalDevice, 1, features, supportedFeatures, queueFamilyIndex, queueCount, queuePriority, numErrors, resultCollector, &extensionNames, emptyDeviceFeatures, memReservationStatMax, isSubProcess);
}

if (const void* featuresStruct = findStructureInChain(const_cast<const void*>(deviceFeatures2.pNext), getStructureType<VkPhysicalDeviceTextureCompressionASTCHDRFeaturesEXT>()))
{
    static const Feature features[] =
    {
        FEATURE_ITEM (VkPhysicalDeviceTextureCompressionASTCHDRFeaturesEXT, textureCompressionASTC_HDR),
    };
    auto* supportedFeatures = reinterpret_cast<const VkPhysicalDeviceTextureCompressionASTCHDRFeaturesEXT*>(featuresStruct);
    checkFeatures(vkp, instance, instanceDriver, physicalDevice, 1, features, supportedFeatures, queueFamilyIndex, queueCount, queuePriority, numErrors, resultCollector, &extensionNames, emptyDeviceFeatures, memReservationStatMax, isSubProcess);
}

if (const void* featuresStruct = findStructureInChain(const_cast<const void*>(deviceFeatures2.pNext), getStructureType<VkPhysicalDeviceASTCDecodeFeaturesEXT>()))
{
    static const Feature features[] =
    {
        FEATURE_ITEM (VkPhysicalDeviceASTCDecodeFeaturesEXT, decodeModeSharedExponent),
    };
    auto* supportedFeatures = reinterpret_cast<const VkPhysicalDeviceASTCDecodeFeaturesEXT*>(featuresStruct);
    checkFeatures(vkp, instance, instanceDriver, physicalDevice, 1, features, supportedFeatures, queueFamilyIndex, queueCount, queuePriority, numErrors, resultCollector, &extensionNames, emptyDeviceFeatures, memReservationStatMax, isSubProcess);
}

if (const void* featuresStruct = findStructureInChain(const_cast<const void*>(deviceFeatures2.pNext), getStructureType<VkPhysicalDeviceDepthClipEnableFeaturesEXT>()))
{
    static const Feature features[] =
    {
        FEATURE_ITEM (VkPhysicalDeviceDepthClipEnableFeaturesEXT, depthClipEnable),
    };
    auto* supportedFeatures = reinterpret_cast<const VkPhysicalDeviceDepthClipEnableFeaturesEXT*>(featuresStruct);
    checkFeatures(vkp, instance, instanceDriver, physicalDevice, 1, features, supportedFeatures, queueFamilyIndex, queueCount, queuePriority, numErrors, resultCollector, &extensionNames, emptyDeviceFeatures, memReservationStatMax, isSubProcess);
}

if (const void* featuresStruct = findStructureInChain(const_cast<const void*>(deviceFeatures2.pNext), getStructureType<VkPhysicalDeviceBlendOperationAdvancedFeaturesEXT>()))
{
    static const Feature features[] =
    {
        FEATURE_ITEM (VkPhysicalDeviceBlendOperationAdvancedFeaturesEXT, advancedBlendCoherentOperations),
    };
    auto* supportedFeatures = reinterpret_cast<const VkPhysicalDeviceBlendOperationAdvancedFeaturesEXT*>(featuresStruct);
    checkFeatures(vkp, instance, instanceDriver, physicalDevice, 1, features, supportedFeatures, queueFamilyIndex, queueCount, queuePriority, numErrors, resultCollector, &extensionNames, emptyDeviceFeatures, memReservationStatMax, isSubProcess);
}

if (const void* featuresStruct = findStructureInChain(const_cast<const void*>(deviceFeatures2.pNext), getStructureType<VkPhysicalDeviceVertexAttributeDivisorFeaturesEXT>()))
{
    static const Feature features[] =
    {
        FEATURE_ITEM (VkPhysicalDeviceVertexAttributeDivisorFeaturesEXT, vertexAttributeInstanceRateDivisor),
        FEATURE_ITEM (VkPhysicalDeviceVertexAttributeDivisorFeaturesEXT, vertexAttributeInstanceRateZeroDivisor),
    };
    auto* supportedFeatures = reinterpret_cast<const VkPhysicalDeviceVertexAttributeDivisorFeaturesEXT*>(featuresStruct);
    checkFeatures(vkp, instance, instanceDriver, physicalDevice, 2, features, supportedFeatures, queueFamilyIndex, queueCount, queuePriority, numErrors, resultCollector, &extensionNames, emptyDeviceFeatures, memReservationStatMax, isSubProcess);
}

if (const void* featuresStruct = findStructureInChain(const_cast<const void*>(deviceFeatures2.pNext), getStructureType<VkPhysicalDeviceSubgroupSizeControlFeaturesEXT>()))
{
    static const Feature features[] =
    {
        FEATURE_ITEM (VkPhysicalDeviceSubgroupSizeControlFeaturesEXT, subgroupSizeControl),
        FEATURE_ITEM (VkPhysicalDeviceSubgroupSizeControlFeaturesEXT, computeFullSubgroups),
    };
    auto* supportedFeatures = reinterpret_cast<const VkPhysicalDeviceSubgroupSizeControlFeaturesEXT*>(featuresStruct);
    checkFeatures(vkp, instance, instanceDriver, physicalDevice, 2, features, supportedFeatures, queueFamilyIndex, queueCount, queuePriority, numErrors, resultCollector, &extensionNames, emptyDeviceFeatures, memReservationStatMax, isSubProcess);
}

if (const void* featuresStruct = findStructureInChain(const_cast<const void*>(deviceFeatures2.pNext), getStructureType<VkPhysicalDeviceShaderImageAtomicInt64FeaturesEXT>()))
{
    static const Feature features[] =
    {
        FEATURE_ITEM (VkPhysicalDeviceShaderImageAtomicInt64FeaturesEXT, shaderImageInt64Atomics),
        FEATURE_ITEM (VkPhysicalDeviceShaderImageAtomicInt64FeaturesEXT, sparseImageInt64Atomics),
    };
    auto* supportedFeatures = reinterpret_cast<const VkPhysicalDeviceShaderImageAtomicInt64FeaturesEXT*>(featuresStruct);
    checkFeatures(vkp, instance, instanceDriver, physicalDevice, 2, features, supportedFeatures, queueFamilyIndex, queueCount, queuePriority, numErrors, resultCollector, &extensionNames, emptyDeviceFeatures, memReservationStatMax, isSubProcess);
}

if (const void* featuresStruct = findStructureInChain(const_cast<const void*>(deviceFeatures2.pNext), getStructureType<VkPhysicalDeviceFragmentShaderInterlockFeaturesEXT>()))
{
    static const Feature features[] =
    {
        FEATURE_ITEM (VkPhysicalDeviceFragmentShaderInterlockFeaturesEXT, fragmentShaderSampleInterlock),
        FEATURE_ITEM (VkPhysicalDeviceFragmentShaderInterlockFeaturesEXT, fragmentShaderPixelInterlock),
        FEATURE_ITEM (VkPhysicalDeviceFragmentShaderInterlockFeaturesEXT, fragmentShaderShadingRateInterlock),
    };
    auto* supportedFeatures = reinterpret_cast<const VkPhysicalDeviceFragmentShaderInterlockFeaturesEXT*>(featuresStruct);
    checkFeatures(vkp, instance, instanceDriver, physicalDevice, 3, features, supportedFeatures, queueFamilyIndex, queueCount, queuePriority, numErrors, resultCollector, &extensionNames, emptyDeviceFeatures, memReservationStatMax, isSubProcess);
}

if (const void* featuresStruct = findStructureInChain(const_cast<const void*>(deviceFeatures2.pNext), getStructureType<VkPhysicalDeviceYcbcrImageArraysFeaturesEXT>()))
{
    static const Feature features[] =
    {
        FEATURE_ITEM (VkPhysicalDeviceYcbcrImageArraysFeaturesEXT, ycbcrImageArrays),
    };
    auto* supportedFeatures = reinterpret_cast<const VkPhysicalDeviceYcbcrImageArraysFeaturesEXT*>(featuresStruct);
    checkFeatures(vkp, instance, instanceDriver, physicalDevice, 1, features, supportedFeatures, queueFamilyIndex, queueCount, queuePriority, numErrors, resultCollector, &extensionNames, emptyDeviceFeatures, memReservationStatMax, isSubProcess);
}

if (const void* featuresStruct = findStructureInChain(const_cast<const void*>(deviceFeatures2.pNext), getStructureType<VkPhysicalDeviceLineRasterizationFeaturesEXT>()))
{
    static const Feature features[] =
    {
        FEATURE_ITEM (VkPhysicalDeviceLineRasterizationFeaturesEXT, rectangularLines),
        FEATURE_ITEM (VkPhysicalDeviceLineRasterizationFeaturesEXT, bresenhamLines),
        FEATURE_ITEM (VkPhysicalDeviceLineRasterizationFeaturesEXT, smoothLines),
        FEATURE_ITEM (VkPhysicalDeviceLineRasterizationFeaturesEXT, stippledRectangularLines),
        FEATURE_ITEM (VkPhysicalDeviceLineRasterizationFeaturesEXT, stippledBresenhamLines),
        FEATURE_ITEM (VkPhysicalDeviceLineRasterizationFeaturesEXT, stippledSmoothLines),
    };
    auto* supportedFeatures = reinterpret_cast<const VkPhysicalDeviceLineRasterizationFeaturesEXT*>(featuresStruct);
    checkFeatures(vkp, instance, instanceDriver, physicalDevice, 6, features, supportedFeatures, queueFamilyIndex, queueCount, queuePriority, numErrors, resultCollector, &extensionNames, emptyDeviceFeatures, memReservationStatMax, isSubProcess);
}

if (const void* featuresStruct = findStructureInChain(const_cast<const void*>(deviceFeatures2.pNext), getStructureType<VkPhysicalDeviceShaderAtomicFloatFeaturesEXT>()))
{
    static const Feature features[] =
    {
        FEATURE_ITEM (VkPhysicalDeviceShaderAtomicFloatFeaturesEXT, shaderBufferFloat32Atomics),
        FEATURE_ITEM (VkPhysicalDeviceShaderAtomicFloatFeaturesEXT, shaderBufferFloat32AtomicAdd),
        FEATURE_ITEM (VkPhysicalDeviceShaderAtomicFloatFeaturesEXT, shaderBufferFloat64Atomics),
        FEATURE_ITEM (VkPhysicalDeviceShaderAtomicFloatFeaturesEXT, shaderBufferFloat64AtomicAdd),
        FEATURE_ITEM (VkPhysicalDeviceShaderAtomicFloatFeaturesEXT, shaderSharedFloat32Atomics),
        FEATURE_ITEM (VkPhysicalDeviceShaderAtomicFloatFeaturesEXT, shaderSharedFloat32AtomicAdd),
        FEATURE_ITEM (VkPhysicalDeviceShaderAtomicFloatFeaturesEXT, shaderSharedFloat64Atomics),
        FEATURE_ITEM (VkPhysicalDeviceShaderAtomicFloatFeaturesEXT, shaderSharedFloat64AtomicAdd),
        FEATURE_ITEM (VkPhysicalDeviceShaderAtomicFloatFeaturesEXT, shaderImageFloat32Atomics),
        FEATURE_ITEM (VkPhysicalDeviceShaderAtomicFloatFeaturesEXT, shaderImageFloat32AtomicAdd),
        FEATURE_ITEM (VkPhysicalDeviceShaderAtomicFloatFeaturesEXT, sparseImageFloat32Atomics),
        FEATURE_ITEM (VkPhysicalDeviceShaderAtomicFloatFeaturesEXT, sparseImageFloat32AtomicAdd),
    };
    auto* supportedFeatures = reinterpret_cast<const VkPhysicalDeviceShaderAtomicFloatFeaturesEXT*>(featuresStruct);
    checkFeatures(vkp, instance, instanceDriver, physicalDevice, 12, features, supportedFeatures, queueFamilyIndex, queueCount, queuePriority, numErrors, resultCollector, &extensionNames, emptyDeviceFeatures, memReservationStatMax, isSubProcess);
}

if (const void* featuresStruct = findStructureInChain(const_cast<const void*>(deviceFeatures2.pNext), getStructureType<VkPhysicalDeviceIndexTypeUint8FeaturesEXT>()))
{
    static const Feature features[] =
    {
        FEATURE_ITEM (VkPhysicalDeviceIndexTypeUint8FeaturesEXT, indexTypeUint8),
    };
    auto* supportedFeatures = reinterpret_cast<const VkPhysicalDeviceIndexTypeUint8FeaturesEXT*>(featuresStruct);
    checkFeatures(vkp, instance, instanceDriver, physicalDevice, 1, features, supportedFeatures, queueFamilyIndex, queueCount, queuePriority, numErrors, resultCollector, &extensionNames, emptyDeviceFeatures, memReservationStatMax, isSubProcess);
}

if (const void* featuresStruct = findStructureInChain(const_cast<const void*>(deviceFeatures2.pNext), getStructureType<VkPhysicalDeviceExtendedDynamicStateFeaturesEXT>()))
{
    static const Feature features[] =
    {
        FEATURE_ITEM (VkPhysicalDeviceExtendedDynamicStateFeaturesEXT, extendedDynamicState),
    };
    auto* supportedFeatures = reinterpret_cast<const VkPhysicalDeviceExtendedDynamicStateFeaturesEXT*>(featuresStruct);
    checkFeatures(vkp, instance, instanceDriver, physicalDevice, 1, features, supportedFeatures, queueFamilyIndex, queueCount, queuePriority, numErrors, resultCollector, &extensionNames, emptyDeviceFeatures, memReservationStatMax, isSubProcess);
}

if (const void* featuresStruct = findStructureInChain(const_cast<const void*>(deviceFeatures2.pNext), getStructureType<VkPhysicalDeviceShaderDemoteToHelperInvocationFeaturesEXT>()))
{
    static const Feature features[] =
    {
        FEATURE_ITEM (VkPhysicalDeviceShaderDemoteToHelperInvocationFeaturesEXT, shaderDemoteToHelperInvocation),
    };
    auto* supportedFeatures = reinterpret_cast<const VkPhysicalDeviceShaderDemoteToHelperInvocationFeaturesEXT*>(featuresStruct);
    checkFeatures(vkp, instance, instanceDriver, physicalDevice, 1, features, supportedFeatures, queueFamilyIndex, queueCount, queuePriority, numErrors, resultCollector, &extensionNames, emptyDeviceFeatures, memReservationStatMax, isSubProcess);
}

if (const void* featuresStruct = findStructureInChain(const_cast<const void*>(deviceFeatures2.pNext), getStructureType<VkPhysicalDeviceTexelBufferAlignmentFeaturesEXT>()))
{
    static const Feature features[] =
    {
        FEATURE_ITEM (VkPhysicalDeviceTexelBufferAlignmentFeaturesEXT, texelBufferAlignment),
    };
    auto* supportedFeatures = reinterpret_cast<const VkPhysicalDeviceTexelBufferAlignmentFeaturesEXT*>(featuresStruct);
    checkFeatures(vkp, instance, instanceDriver, physicalDevice, 1, features, supportedFeatures, queueFamilyIndex, queueCount, queuePriority, numErrors, resultCollector, &extensionNames, emptyDeviceFeatures, memReservationStatMax, isSubProcess);
}

if (const void* featuresStruct = findStructureInChain(const_cast<const void*>(deviceFeatures2.pNext), getStructureType<VkPhysicalDeviceRobustness2FeaturesEXT>()))
{
    static const Feature features[] =
    {
        FEATURE_ITEM (VkPhysicalDeviceRobustness2FeaturesEXT, robustBufferAccess2),
        FEATURE_ITEM (VkPhysicalDeviceRobustness2FeaturesEXT, robustImageAccess2),
        FEATURE_ITEM (VkPhysicalDeviceRobustness2FeaturesEXT, nullDescriptor),
    };
    auto* supportedFeatures = reinterpret_cast<const VkPhysicalDeviceRobustness2FeaturesEXT*>(featuresStruct);
    checkFeatures(vkp, instance, instanceDriver, physicalDevice, 3, features, supportedFeatures, queueFamilyIndex, queueCount, queuePriority, numErrors, resultCollector, &extensionNames, emptyDeviceFeatures, memReservationStatMax, isSubProcess);
}

if (const void* featuresStruct = findStructureInChain(const_cast<const void*>(deviceFeatures2.pNext), getStructureType<VkPhysicalDeviceCustomBorderColorFeaturesEXT>()))
{
    static const Feature features[] =
    {
        FEATURE_ITEM (VkPhysicalDeviceCustomBorderColorFeaturesEXT, customBorderColors),
        FEATURE_ITEM (VkPhysicalDeviceCustomBorderColorFeaturesEXT, customBorderColorWithoutFormat),
    };
    auto* supportedFeatures = reinterpret_cast<const VkPhysicalDeviceCustomBorderColorFeaturesEXT*>(featuresStruct);
    checkFeatures(vkp, instance, instanceDriver, physicalDevice, 2, features, supportedFeatures, queueFamilyIndex, queueCount, queuePriority, numErrors, resultCollector, &extensionNames, emptyDeviceFeatures, memReservationStatMax, isSubProcess);
}

if (const void* featuresStruct = findStructureInChain(const_cast<const void*>(deviceFeatures2.pNext), getStructureType<VkPhysicalDeviceYcbcr2Plane444FormatsFeaturesEXT>()))
{
    static const Feature features[] =
    {
        FEATURE_ITEM (VkPhysicalDeviceYcbcr2Plane444FormatsFeaturesEXT, ycbcr2plane444Formats),
    };
    auto* supportedFeatures = reinterpret_cast<const VkPhysicalDeviceYcbcr2Plane444FormatsFeaturesEXT*>(featuresStruct);
    checkFeatures(vkp, instance, instanceDriver, physicalDevice, 1, features, supportedFeatures, queueFamilyIndex, queueCount, queuePriority, numErrors, resultCollector, &extensionNames, emptyDeviceFeatures, memReservationStatMax, isSubProcess);
}

if (const void* featuresStruct = findStructureInChain(const_cast<const void*>(deviceFeatures2.pNext), getStructureType<VkPhysicalDeviceImageRobustnessFeaturesEXT>()))
{
    static const Feature features[] =
    {
        FEATURE_ITEM (VkPhysicalDeviceImageRobustnessFeaturesEXT, robustImageAccess),
    };
    auto* supportedFeatures = reinterpret_cast<const VkPhysicalDeviceImageRobustnessFeaturesEXT*>(featuresStruct);
    checkFeatures(vkp, instance, instanceDriver, physicalDevice, 1, features, supportedFeatures, queueFamilyIndex, queueCount, queuePriority, numErrors, resultCollector, &extensionNames, emptyDeviceFeatures, memReservationStatMax, isSubProcess);
}

if (const void* featuresStruct = findStructureInChain(const_cast<const void*>(deviceFeatures2.pNext), getStructureType<VkPhysicalDevice4444FormatsFeaturesEXT>()))
{
    static const Feature features[] =
    {
        FEATURE_ITEM (VkPhysicalDevice4444FormatsFeaturesEXT, formatA4R4G4B4),
        FEATURE_ITEM (VkPhysicalDevice4444FormatsFeaturesEXT, formatA4B4G4R4),
    };
    auto* supportedFeatures = reinterpret_cast<const VkPhysicalDevice4444FormatsFeaturesEXT*>(featuresStruct);
    checkFeatures(vkp, instance, instanceDriver, physicalDevice, 2, features, supportedFeatures, queueFamilyIndex, queueCount, queuePriority, numErrors, resultCollector, &extensionNames, emptyDeviceFeatures, memReservationStatMax, isSubProcess);
}

if (const void* featuresStruct = findStructureInChain(const_cast<const void*>(deviceFeatures2.pNext), getStructureType<VkPhysicalDeviceVertexInputDynamicStateFeaturesEXT>()))
{
    static const Feature features[] =
    {
        FEATURE_ITEM (VkPhysicalDeviceVertexInputDynamicStateFeaturesEXT, vertexInputDynamicState),
    };
    auto* supportedFeatures = reinterpret_cast<const VkPhysicalDeviceVertexInputDynamicStateFeaturesEXT*>(featuresStruct);
    checkFeatures(vkp, instance, instanceDriver, physicalDevice, 1, features, supportedFeatures, queueFamilyIndex, queueCount, queuePriority, numErrors, resultCollector, &extensionNames, emptyDeviceFeatures, memReservationStatMax, isSubProcess);
}

if (const void* featuresStruct = findStructureInChain(const_cast<const void*>(deviceFeatures2.pNext), getStructureType<VkPhysicalDeviceExtendedDynamicState2FeaturesEXT>()))
{
    static const Feature features[] =
    {
        FEATURE_ITEM (VkPhysicalDeviceExtendedDynamicState2FeaturesEXT, extendedDynamicState2),
        FEATURE_ITEM (VkPhysicalDeviceExtendedDynamicState2FeaturesEXT, extendedDynamicState2LogicOp),
        FEATURE_ITEM (VkPhysicalDeviceExtendedDynamicState2FeaturesEXT, extendedDynamicState2PatchControlPoints),
    };
    auto* supportedFeatures = reinterpret_cast<const VkPhysicalDeviceExtendedDynamicState2FeaturesEXT*>(featuresStruct);
    checkFeatures(vkp, instance, instanceDriver, physicalDevice, 3, features, supportedFeatures, queueFamilyIndex, queueCount, queuePriority, numErrors, resultCollector, &extensionNames, emptyDeviceFeatures, memReservationStatMax, isSubProcess);
}

if (const void* featuresStruct = findStructureInChain(const_cast<const void*>(deviceFeatures2.pNext), getStructureType<VkPhysicalDeviceColorWriteEnableFeaturesEXT>()))
{
    static const Feature features[] =
    {
        FEATURE_ITEM (VkPhysicalDeviceColorWriteEnableFeaturesEXT, colorWriteEnable),
    };
    auto* supportedFeatures = reinterpret_cast<const VkPhysicalDeviceColorWriteEnableFeaturesEXT*>(featuresStruct);
    checkFeatures(vkp, instance, instanceDriver, physicalDevice, 1, features, supportedFeatures, queueFamilyIndex, queueCount, queuePriority, numErrors, resultCollector, &extensionNames, emptyDeviceFeatures, memReservationStatMax, isSubProcess);
}

