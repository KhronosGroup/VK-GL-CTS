/*-------------------------------------------------------------------------
 * Vulkan CTS
 * ----------
 *
 * Copyright (c) 2019 The Khronos Group Inc.
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
 */

#include "deSTLUtil.hpp"
#include "deString.h"
#include "vkQueryUtil.hpp"
#include "vkDeviceFeatures.inl"
#include "vkDeviceFeatures.hpp"

namespace vk
{

DeviceFeatures::DeviceFeatures(const InstanceInterface &vki, const uint32_t apiVersion,
                               const VkPhysicalDevice physicalDevice,
                               const std::vector<std::string> &instanceExtensions,
                               const std::vector<std::string> &deviceExtensions, const bool enableAllFeatures)
{
    VkPhysicalDeviceRobustness2FeaturesEXT *robustness2Features         = nullptr;
    VkPhysicalDeviceImageRobustnessFeaturesEXT *imageRobustnessFeatures = nullptr;
#ifndef CTS_USES_VULKANSC
    VkPhysicalDeviceFragmentShadingRateFeaturesKHR *fragmentShadingRateFeatures             = nullptr;
    VkPhysicalDeviceShadingRateImageFeaturesNV *shadingRateImageFeaturesNV                  = nullptr;
    VkPhysicalDeviceFragmentDensityMapFeaturesEXT *fragmentDensityMapFeatures               = nullptr;
    VkPhysicalDevicePageableDeviceLocalMemoryFeaturesEXT *pageableDeviceLocalMemoryFeatures = nullptr;
    VkPhysicalDeviceMutableDescriptorTypeFeaturesEXT *mutableDescriptorTypeFeatures         = nullptr;
    VkPhysicalDeviceLegacyDitheringFeaturesEXT *legacyDitheringFeatures                     = nullptr;
    VkPhysicalDeviceFaultFeaturesEXT *deviceFaultFeatures                                   = nullptr;
#endif // CTS_USES_VULKANSC

    m_coreFeatures2    = initVulkanStructure();
    m_vulkan11Features = initVulkanStructure();
    m_vulkan12Features = initVulkanStructure();
#ifndef CTS_USES_VULKANSC
    m_vulkan13Features = initVulkanStructure();
    m_vulkan14Features = initVulkanStructure();
#endif // CTS_USES_VULKANSC
#ifdef CTS_USES_VULKANSC
    m_vulkanSC10Features = initVulkanStructure();
#endif // CTS_USES_VULKANSC

    auto assignRawStructPtr = [](auto **featureStruct, void *rawStructPtr)
    { *featureStruct = reinterpret_cast<decltype(*featureStruct)>(rawStructPtr); };

    if (isInstanceExtensionSupported(apiVersion, instanceExtensions, "VK_KHR_get_physical_device_properties2"))
    {
        const auto deviceExtensionProperties = enumerateDeviceExtensionProperties(vki, physicalDevice, nullptr);
        void **nextPtr                       = &m_coreFeatures2.pNext;
        std::vector<FeatureStructWrapperBase *> featuresToFillFromBlob;
#ifndef CTS_USES_VULKANSC
        bool vk14Supported = (apiVersion >= VK_MAKE_API_VERSION(0, 1, 4, 0));
        bool vk13Supported = (apiVersion >= VK_MAKE_API_VERSION(0, 1, 3, 0));
#endif // CTS_USES_VULKANSC
        bool vk12Supported = (apiVersion >= VK_MAKE_API_VERSION(0, 1, 2, 0));
#ifdef CTS_USES_VULKANSC
        bool vksc10Supported = (apiVersion >= VK_MAKE_API_VERSION(1, 1, 0, 0));
#endif // CTS_USES_VULKANSC

        m_features.reserve(std::size(featureStructCreationArray));

        // since vk12 we have blob structures combining features of couple previously
        // available feature structures, that now in vk12+ must be removed from chain
        if (vk12Supported)
        {
            addToChainVulkanStructure(&nextPtr, m_vulkan11Features);
            addToChainVulkanStructure(&nextPtr, m_vulkan12Features);

#ifndef CTS_USES_VULKANSC
            if (vk13Supported)
                addToChainVulkanStructure(&nextPtr, m_vulkan13Features);
            if (vk14Supported)
                addToChainVulkanStructure(&nextPtr, m_vulkan14Features);
#endif // CTS_USES_VULKANSC
        }
#ifdef CTS_USES_VULKANSC
        if (vksc10Supported)
        {
            addToChainVulkanStructure(&nextPtr, m_vulkanSC10Features);
        }
#endif // CTS_USES_VULKANSC

        std::vector<std::string> allDeviceExtensions = deviceExtensions;
        std::vector<const char *> coreExtensions;
        getCoreDeviceExtensions(apiVersion, coreExtensions);
        for (const auto &coreExt : coreExtensions)
            if (!isExtensionStructSupported(allDeviceExtensions, coreExt))
                allDeviceExtensions.push_back(coreExt);

        // iterate over data for all feature that are defined in specification
        for (const auto &featureStructCreationData : featureStructCreationArray)
        {
            if (verifyFeatureAddCriteria(featureStructCreationData, allDeviceExtensions, deviceExtensionProperties))
            {
                FeatureStructWrapperBase *p = (*featureStructCreationData.creatorFunction)();
                if (p == nullptr)
                    continue;

                // if feature struct is part of VkPhysicalDeviceVulkan1{1,2,3,4}Features
                // we dont add it to the chain but store and fill later from blob data
                bool featureFilledFromBlob = false;
                if (vk12Supported)
                {
                    uint32_t blobApiVersion = getBlobFeaturesVersion(p->getFeatureDesc().sType);
                    if (blobApiVersion)
                        featureFilledFromBlob = (apiVersion >= blobApiVersion);
                }

                if (featureFilledFromBlob)
                    featuresToFillFromBlob.push_back(p);
                else
                {
                    VkStructureType structType = p->getFeatureDesc().sType;
                    void *rawStructPtr         = p->getFeatureTypeRaw();

                    if (structType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ROBUSTNESS_2_FEATURES_EXT)
                        assignRawStructPtr(&robustness2Features, rawStructPtr);
                    else if (structType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_ROBUSTNESS_FEATURES_EXT)
                        assignRawStructPtr(&imageRobustnessFeatures, rawStructPtr);
#ifndef CTS_USES_VULKANSC
                    else if (structType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADING_RATE_FEATURES_KHR)
                        assignRawStructPtr(&fragmentShadingRateFeatures, rawStructPtr);
                    else if (structType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADING_RATE_IMAGE_FEATURES_NV)
                        assignRawStructPtr(&shadingRateImageFeaturesNV, rawStructPtr);
                    else if (structType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_DENSITY_MAP_FEATURES_EXT)
                        assignRawStructPtr(&fragmentDensityMapFeatures, rawStructPtr);
                    else if (structType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PAGEABLE_DEVICE_LOCAL_MEMORY_FEATURES_EXT)
                        assignRawStructPtr(&pageableDeviceLocalMemoryFeatures, rawStructPtr);
                    else if (structType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MUTABLE_DESCRIPTOR_TYPE_FEATURES_EXT)
                        assignRawStructPtr(&mutableDescriptorTypeFeatures, rawStructPtr);
                    else if (structType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_LEGACY_DITHERING_FEATURES_EXT)
                        assignRawStructPtr(&legacyDitheringFeatures, rawStructPtr);
                    else if (structType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FAULT_FEATURES_EXT)
                        assignRawStructPtr(&deviceFaultFeatures, rawStructPtr);
#endif // CTS_USES_VULKANSC
                    // add to chain
                    *nextPtr = rawStructPtr;
                    nextPtr  = p->getFeatureTypeNext();
                }
                m_features.push_back(p);
            }
            else
            {
#ifndef CTS_USES_VULKANSC
                const auto &featureName = featureStructCreationData.name;
                // Some non-standard promotions may need feature structs filled in anyway.
                if ((featureName == "VK_EXT_extended_dynamic_state") && vk13Supported)
                {
                    FeatureStructWrapperBase *p = (*featureStructCreationData.creatorFunction)();
                    if (p == nullptr)
                        continue;

                    auto f =
                        reinterpret_cast<VkPhysicalDeviceExtendedDynamicStateFeaturesEXT *>(p->getFeatureTypeRaw());
                    f->extendedDynamicState = true;
                    m_features.push_back(p);
                }
                if ((featureName == "VK_EXT_extended_dynamic_state2") && vk13Supported)
                {
                    FeatureStructWrapperBase *p = (*featureStructCreationData.creatorFunction)();
                    if (p == nullptr)
                        continue;

                    auto f =
                        reinterpret_cast<VkPhysicalDeviceExtendedDynamicState2FeaturesEXT *>(p->getFeatureTypeRaw());
                    f->extendedDynamicState2 = true;
                    m_features.push_back(p);
                }
#endif // CTS_USES_VULKANSC
            }
        }

        vki.getPhysicalDeviceFeatures2(physicalDevice, &m_coreFeatures2);

        // fill data from VkPhysicalDeviceVulkan1{1,2,3,4}Features
        if (vk12Supported)
        {
            AllFeaturesBlobs allBlobs = {
                m_vulkan11Features, m_vulkan12Features,
#ifndef CTS_USES_VULKANSC
                m_vulkan13Features, m_vulkan14Features,
#endif // CTS_USES_VULKANSC
                // add blobs from future vulkan versions here
            };

            for (auto feature : featuresToFillFromBlob)
                feature->initializeFeatureFromBlob(allBlobs);
        }
    }
    else
        m_coreFeatures2.features = getPhysicalDeviceFeatures(vki, physicalDevice);

    // 'enableAllFeatures' is used to create a complete list of supported features.
    if (!enableAllFeatures)
    {
        // Disable robustness by default, as it has an impact on performance on some HW.
        if (robustness2Features)
        {
            robustness2Features->robustBufferAccess2 = false;
            robustness2Features->robustImageAccess2  = false;
            robustness2Features->nullDescriptor      = false;
        }
        if (imageRobustnessFeatures)
        {
            imageRobustnessFeatures->robustImageAccess = false;
        }
        m_coreFeatures2.features.robustBufferAccess = false;

#ifndef CTS_USES_VULKANSC
        m_vulkan13Features.robustImageAccess = false;

        // Disable VK_EXT_fragment_density_map and VK_NV_shading_rate_image features
        // that must: not be enabled if KHR fragment shading rate features are enabled.
        if (fragmentShadingRateFeatures && (fragmentShadingRateFeatures->pipelineFragmentShadingRate ||
                                            fragmentShadingRateFeatures->primitiveFragmentShadingRate ||
                                            fragmentShadingRateFeatures->attachmentFragmentShadingRate))
        {
            if (shadingRateImageFeaturesNV)
                shadingRateImageFeaturesNV->shadingRateImage = false;
            if (fragmentDensityMapFeatures)
                fragmentDensityMapFeatures->fragmentDensityMap = false;
        }

        // Disable pageableDeviceLocalMemory by default since it may modify the behavior
        // of device-local, and even host-local, memory allocations for all tests.
        // pageableDeviceLocalMemory will use targetted testing on a custom device.
        if (pageableDeviceLocalMemoryFeatures)
            pageableDeviceLocalMemoryFeatures->pageableDeviceLocalMemory = false;

        // Disable mutableDescriptorTypeFeatures by default because it can
        // impact performance on some hardware.
        if (mutableDescriptorTypeFeatures)
            mutableDescriptorTypeFeatures->mutableDescriptorType = false;

        // Disable legacyDitheringFeatures by default because it interacts with
        // dynamic_rendering. On some hardware DR tests may fail on precision.
        // Float thresholds would need to be more lenient for low bitrate formats
        // when DR is used togehrt with legacy dithering.
        if (legacyDitheringFeatures)
            legacyDitheringFeatures->legacyDithering = false;

        // Disable deviceFaultVendorBinary by default because it can impact
        // performance.
        if (deviceFaultFeatures)
        {
            deviceFaultFeatures->deviceFaultVendorBinary = false;
        }
#endif // CTS_USES_VULKANSC
    }
}

bool DeviceFeatures::verifyFeatureAddCriteria(const FeatureStructCreationData &item,
                                              const std::vector<std::string> &allDeviceExtensions,
                                              const std::vector<VkExtensionProperties> &properties)
{
    const auto &featureName = item.name;

    // check if this is core feature
    bool isFeatureAvailable = (featureName == "core_feature");

    // check if this feature is available on current device
    if (!isFeatureAvailable)
        isFeatureAvailable = isExtensionStructSupported(allDeviceExtensions, featureName);

    // if this is promoted feature and it is not available then check also older version
    // e.g. if VK_KHR_line_rasterization is not supported try VK_EXT_line_rasterization
    if (!isFeatureAvailable)
    {
        const auto previousFeatureExtName = getPreviousFeatureExtName(featureName);
        isFeatureAvailable                = isExtensionStructSupported(allDeviceExtensions, previousFeatureExtName);
    }

    if (!isFeatureAvailable)
        return false;

#ifndef CTS_USES_VULKANSC
    if (item.name == VK_KHR_VULKAN_MEMORY_MODEL_EXTENSION_NAME)
    {
        for (const auto &property : properties)
        {
            if (property.extensionName == item.name)
                return (property.specVersion == item.specVersion);
        }
    }
#else  // CTS_USES_VULKANSC
    DE_UNREF(properties);
#endif // CTS_USES_VULKANSC

    return true;
}

bool DeviceFeatures::contains(const std::string &feature, bool throwIfNotExists) const
{
    for (const auto f : m_features)
    {
        if (f->getFeatureDesc().name == feature)
            return true;
    }

    if (throwIfNotExists)
        TCU_THROW(NotSupportedError, "Feature " + feature + " is not supported");

    return false;
}

bool DeviceFeatures::isDeviceFeatureInitialized(VkStructureType sType) const
{
    for (const auto f : m_features)
    {
        if (f->getFeatureDesc().sType == sType)
            return true;
    }
    return false;
}

uint32_t DeviceFeatures::getBlobFeatureVersion(VkStructureType sType)
{
    return getBlobFeaturesVersion(sType);
}

std::set<VkStructureType> DeviceFeatures::getVersionBlobFeatures(uint32_t version)
{
    return getVersionBlobFeatureList(version);
}

DeviceFeatures::~DeviceFeatures(void)
{
    for (auto p : m_features)
        delete p;

    m_features.clear();
}

} // namespace vk
