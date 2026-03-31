/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2016 The Khronos Group Inc.
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
 *//*
* \file vktPipelineMultisampleBase.cpp
* \brief Multisample Tests Base Classes
*//*--------------------------------------------------------------------*/

#include "vktPipelineMultisampleBase.hpp"
#include "vkQueryUtil.hpp"

namespace vkt
{
namespace pipeline
{
namespace multisample
{

using namespace vk;

void MultisampleCaseBase::validateImageSize(const InstanceInterface &instance, const VkPhysicalDevice physicalDevice,
                                            const ImageType imageType, const tcu::UVec3 &imageSize) const
{
    const VkPhysicalDeviceProperties deviceProperties = getPhysicalDeviceProperties(instance, physicalDevice);

    bool isImageSizeValid = true;

    switch (imageType)
    {
    case IMAGE_TYPE_1D:
        isImageSizeValid = imageSize.x() <= deviceProperties.limits.maxImageDimension1D;
        break;
    case IMAGE_TYPE_1D_ARRAY:
        isImageSizeValid = imageSize.x() <= deviceProperties.limits.maxImageDimension1D &&
                           imageSize.z() <= deviceProperties.limits.maxImageArrayLayers;
        break;
    case IMAGE_TYPE_2D:
        isImageSizeValid = imageSize.x() <= deviceProperties.limits.maxImageDimension2D &&
                           imageSize.y() <= deviceProperties.limits.maxImageDimension2D;
        break;
    case IMAGE_TYPE_2D_ARRAY:
        isImageSizeValid = imageSize.x() <= deviceProperties.limits.maxImageDimension2D &&
                           imageSize.y() <= deviceProperties.limits.maxImageDimension2D &&
                           imageSize.z() <= deviceProperties.limits.maxImageArrayLayers;
        break;
    case IMAGE_TYPE_CUBE:
        isImageSizeValid = imageSize.x() <= deviceProperties.limits.maxImageDimensionCube &&
                           imageSize.y() <= deviceProperties.limits.maxImageDimensionCube;
        break;
    case IMAGE_TYPE_CUBE_ARRAY:
        isImageSizeValid = imageSize.x() <= deviceProperties.limits.maxImageDimensionCube &&
                           imageSize.y() <= deviceProperties.limits.maxImageDimensionCube &&
                           imageSize.z() <= deviceProperties.limits.maxImageArrayLayers;
        break;
    case IMAGE_TYPE_3D:
        isImageSizeValid = imageSize.x() <= deviceProperties.limits.maxImageDimension3D &&
                           imageSize.y() <= deviceProperties.limits.maxImageDimension3D &&
                           imageSize.z() <= deviceProperties.limits.maxImageDimension3D;
        break;
    default:
        DE_FATAL("Unknown image type");
    }

    if (!isImageSizeValid)
    {
        std::ostringstream notSupportedStream;

        notSupportedStream << "Image type (" << getImageTypeName(imageType) << ") with size (" << imageSize.x() << ", "
                           << imageSize.y() << ", " << imageSize.z() << ") not supported by device" << std::endl;

        const std::string notSupportedString = notSupportedStream.str();

        TCU_THROW(NotSupportedError, notSupportedString.c_str());
    }
}

void MultisampleCaseBase::validateImageFeatureFlags(const InstanceInterface &instance,
                                                    const VkPhysicalDevice physicalDevice, const VkFormat format,
                                                    const VkFormatFeatureFlags featureFlags) const
{
    const VkFormatProperties formatProperties = getPhysicalDeviceFormatProperties(instance, physicalDevice, format);

    if ((formatProperties.optimalTilingFeatures & featureFlags) != featureFlags)
    {
        std::ostringstream notSupportedStream;

        notSupportedStream << "Device does not support image format " << format << " for feature flags " << featureFlags
                           << std::endl;

        const std::string notSupportedString = notSupportedStream.str();

        TCU_THROW(NotSupportedError, notSupportedString.c_str());
    }
}

void MultisampleCaseBase::validateImageInfo(const InstanceInterface &instance, const VkPhysicalDevice physicalDevice,
                                            VkFormat format, VkExtent3D extent, VkImageType imageType,
                                            VkSampleCountFlagBits samples, VkImageTiling tiling,
                                            VkImageUsageFlags usage, VkImageCreateFlags flags,
                                            uint32_t arrayLayers) const
{
    VkImageFormatProperties imageFormatProps;
    instance.getPhysicalDeviceImageFormatProperties(physicalDevice, format, imageType, tiling, usage, flags,
                                                    &imageFormatProps);

    if (imageFormatProps.maxExtent.width < extent.width || imageFormatProps.maxExtent.height < extent.height ||
        imageFormatProps.maxExtent.depth < extent.depth)
    {
        std::ostringstream notSupportedStream;

        notSupportedStream << "Image extent (" << extent.width << ", " << extent.height << ", " << extent.depth
                           << ") exceeds allowed maximum (" << imageFormatProps.maxExtent.width << ", "
                           << imageFormatProps.maxExtent.height << ", " << imageFormatProps.maxExtent.depth << ")";

        const std::string notSupportedString = notSupportedStream.str();

        TCU_THROW(NotSupportedError, notSupportedString.c_str());
    }

    if (imageFormatProps.maxArrayLayers < arrayLayers)
    {
        std::ostringstream notSupportedStream;

        notSupportedStream << "Image layers count of " << arrayLayers << " exceeds allowed maximum which is "
                           << imageFormatProps.maxArrayLayers;

        const std::string notSupportedString = notSupportedStream.str();

        TCU_THROW(NotSupportedError, notSupportedString.c_str());
    }

    if (!(imageFormatProps.sampleCounts & samples))
    {
        std::ostringstream notSupportedStream;

        notSupportedStream << "Samples count of " << samples << " not supported for image";

        const std::string notSupportedString = notSupportedStream.str();

        TCU_THROW(NotSupportedError, notSupportedString.c_str());
    }
}

} // namespace multisample
} // namespace pipeline
} // namespace vkt
