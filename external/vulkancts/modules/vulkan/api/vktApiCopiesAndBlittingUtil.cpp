/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2025 The Khronos Group Inc.
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
 * \brief Vulkan Copies And Blitting Util
 *//*--------------------------------------------------------------------*/

#include "vktApiCopiesAndBlittingUtil.hpp"

namespace vkt
{

namespace api
{

VkImageCopy2KHR convertvkImageCopyTovkImageCopy2KHR(VkImageCopy imageCopy)
{
    const VkImageCopy2KHR imageCopy2 = {
        VK_STRUCTURE_TYPE_IMAGE_COPY_2_KHR, // VkStructureType sType;
        nullptr,                            // const void* pNext;
        imageCopy.srcSubresource,           // VkImageSubresourceLayers srcSubresource;
        imageCopy.srcOffset,                // VkOffset3D srcOffset;
        imageCopy.dstSubresource,           // VkImageSubresourceLayers dstSubresource;
        imageCopy.dstOffset,                // VkOffset3D dstOffset;
        imageCopy.extent                    // VkExtent3D extent;
    };
    return imageCopy2;
}
VkBufferCopy2KHR convertvkBufferCopyTovkBufferCopy2KHR(VkBufferCopy bufferCopy)
{
    const VkBufferCopy2KHR bufferCopy2 = {
        VK_STRUCTURE_TYPE_BUFFER_COPY_2_KHR, // VkStructureType sType;
        nullptr,                             // const void* pNext;
        bufferCopy.srcOffset,                // VkDeviceSize srcOffset;
        bufferCopy.dstOffset,                // VkDeviceSize dstOffset;
        bufferCopy.size,                     // VkDeviceSize size;
    };
    return bufferCopy2;
}

VkBufferImageCopy2KHR convertvkBufferImageCopyTovkBufferImageCopy2KHR(VkBufferImageCopy bufferImageCopy)
{
    const VkBufferImageCopy2KHR bufferImageCopy2 = {
        VK_STRUCTURE_TYPE_BUFFER_IMAGE_COPY_2_KHR, // VkStructureType sType;
        nullptr,                                   // const void* pNext;
        bufferImageCopy.bufferOffset,              // VkDeviceSize bufferOffset;
        bufferImageCopy.bufferRowLength,           // uint32_t bufferRowLength;
        bufferImageCopy.bufferImageHeight,         // uint32_t bufferImageHeight;
        bufferImageCopy.imageSubresource,          // VkImageSubresourceLayers imageSubresource;
        bufferImageCopy.imageOffset,               // VkOffset3D imageOffset;
        bufferImageCopy.imageExtent                // VkExtent3D imageExtent;
    };
    return bufferImageCopy2;
}

#ifndef CTS_USES_VULKANSC
VkCopyMemoryToImageIndirectCommandKHR convertvkBufferImageCopyTovkMemoryImageCopyKHR(VkDeviceAddress srcBufferAddress,
                                                                                     VkBufferImageCopy bufferImageCopy)
{
    const VkCopyMemoryToImageIndirectCommandKHR memoryImageCopy = {
        srcBufferAddress + bufferImageCopy.bufferOffset, // VkDeviceAddress srcAddress;
        bufferImageCopy.bufferRowLength,                 // uint32_t bufferRowLength;
        bufferImageCopy.bufferImageHeight,               // uint32_t bufferImageHeight;
        bufferImageCopy.imageSubresource,                // VkImageSubresourceLayers imageSubresource;
        bufferImageCopy.imageOffset,                     // VkOffset3D imageOffset;
        bufferImageCopy.imageExtent                      // VkExtent3D imageExtent;
    };
    return memoryImageCopy;
}
#endif

VkImageBlit2KHR convertvkImageBlitTovkImageBlit2KHR(VkImageBlit imageBlit)
{
    const VkImageBlit2KHR imageBlit2 = {VK_STRUCTURE_TYPE_IMAGE_BLIT_2_KHR, // VkStructureType sType;
                                        nullptr,                            // const void* pNext;
                                        imageBlit.srcSubresource,           // VkImageSubresourceLayers srcSubresource;
                                        {                                   // VkOffset3D srcOffsets[2];
                                         {
                                             imageBlit.srcOffsets[0].x, // VkOffset3D srcOffsets[0].x;
                                             imageBlit.srcOffsets[0].y, // VkOffset3D srcOffsets[0].y;
                                             imageBlit.srcOffsets[0].z  // VkOffset3D srcOffsets[0].z;
                                         },
                                         {
                                             imageBlit.srcOffsets[1].x, // VkOffset3D srcOffsets[1].x;
                                             imageBlit.srcOffsets[1].y, // VkOffset3D srcOffsets[1].y;
                                             imageBlit.srcOffsets[1].z  // VkOffset3D srcOffsets[1].z;
                                         }},
                                        imageBlit.dstSubresource, // VkImageSubresourceLayers dstSubresource;
                                        {                         // VkOffset3D srcOffsets[2];
                                         {
                                             imageBlit.dstOffsets[0].x, // VkOffset3D dstOffsets[0].x;
                                             imageBlit.dstOffsets[0].y, // VkOffset3D dstOffsets[0].y;
                                             imageBlit.dstOffsets[0].z  // VkOffset3D dstOffsets[0].z;
                                         },
                                         {
                                             imageBlit.dstOffsets[1].x, // VkOffset3D dstOffsets[1].x;
                                             imageBlit.dstOffsets[1].y, // VkOffset3D dstOffsets[1].y;
                                             imageBlit.dstOffsets[1].z  // VkOffset3D dstOffsets[1].z;
                                         }}};
    return imageBlit2;
}

VkImageResolve2KHR convertvkImageResolveTovkImageResolve2KHR(VkImageResolve imageResolve)
{
    const VkImageResolve2KHR imageResolve2 = {
        VK_STRUCTURE_TYPE_IMAGE_RESOLVE_2_KHR, // VkStructureType sType;
        nullptr,                               // const void* pNext;
        imageResolve.srcSubresource,           // VkImageSubresourceLayers srcSubresource;
        imageResolve.srcOffset,                // VkOffset3D srcOffset;
        imageResolve.dstSubresource,           // VkImageSubresourceLayers dstSubresource;
        imageResolve.dstOffset,                // VkOffset3D dstOffset;
        imageResolve.extent                    // VkExtent3D extent;
    };
    return imageResolve2;
}

VkImageAspectFlags getAspectFlags(tcu::TextureFormat format)
{
    VkImageAspectFlags aspectFlag = 0;
    aspectFlag |= (tcu::hasDepthComponent(format.order) ? VK_IMAGE_ASPECT_DEPTH_BIT : 0);
    aspectFlag |= (tcu::hasStencilComponent(format.order) ? VK_IMAGE_ASPECT_STENCIL_BIT : 0);

    if (!aspectFlag)
        aspectFlag = VK_IMAGE_ASPECT_COLOR_BIT;

    return aspectFlag;
}

VkImageAspectFlags getAspectFlags(VkFormat format)
{
    if (isCompressedFormat(format))
        return VK_IMAGE_ASPECT_COLOR_BIT;
    else
        return getAspectFlags(mapVkFormat(format));
}

tcu::TextureFormat getSizeCompatibleTcuTextureFormat(VkFormat format)
{
    if (isCompressedFormat(format))
        return (getBlockSizeInBytes(format) == 8) ? mapVkFormat(VK_FORMAT_R16G16B16A16_UINT) :
                                                    mapVkFormat(VK_FORMAT_R32G32B32A32_UINT);
    else
        return mapVkFormat(format);
}

// This is effectively same as vk::isFloatFormat(mapTextureFormat(format))
// except that it supports some formats that are not mappable to VkFormat.
// When we are checking combined depth and stencil formats, each aspect is
// checked separately, and in some cases we construct PBA with a format that
// is not mappable to VkFormat.
bool isFloatFormat(tcu::TextureFormat format)
{
    return tcu::getTextureChannelClass(format.type) == tcu::TEXTURECHANNELCLASS_FLOATING_POINT;
}

de::MovePtr<Allocation> allocateBuffer(const InstanceInterface &vki, const DeviceInterface &vkd,
                                       const VkPhysicalDevice &physDevice, const VkDevice device,
                                       const VkBuffer &buffer, const MemoryRequirement requirement,
                                       Allocator &allocator, AllocationKind allocationKind)
{
    switch (allocationKind)
    {
    case ALLOCATION_KIND_SUBALLOCATED:
    {
        const VkMemoryRequirements memoryRequirements = getBufferMemoryRequirements(vkd, device, buffer);

        return allocator.allocate(memoryRequirements, requirement);
    }

    case ALLOCATION_KIND_DEDICATED:
    {
        return allocateDedicated(vki, vkd, physDevice, device, buffer, requirement);
    }

    default:
    {
        TCU_THROW(InternalError, "Invalid allocation kind");
    }
    }
}

de::MovePtr<Allocation> allocateImage(const InstanceInterface &vki, const DeviceInterface &vkd,
                                      const VkPhysicalDevice &physDevice, const VkDevice device, const VkImage &image,
                                      const MemoryRequirement requirement, Allocator &allocator,
                                      AllocationKind allocationKind, const uint32_t offset)
{
    switch (allocationKind)
    {
    case ALLOCATION_KIND_SUBALLOCATED:
    {
        VkMemoryRequirements memoryRequirements = getImageMemoryRequirements(vkd, device, image);
        memoryRequirements.size += offset;

        return allocator.allocate(memoryRequirements, requirement);
    }

    case ALLOCATION_KIND_DEDICATED:
    {
        VkMemoryRequirements memoryRequirements = getImageMemoryRequirements(vkd, device, image);
        memoryRequirements.size += offset;

        const VkMemoryDedicatedAllocateInfo dedicatedAllocationInfo = {
            VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO, // VkStructureType        sType
            nullptr,                                          // const void*            pNext
            image,                                            // VkImage                image
            VK_NULL_HANDLE,                                   // VkBuffer                buffer
        };

        return allocateExtended(vki, vkd, physDevice, device, memoryRequirements, requirement,
                                &dedicatedAllocationInfo);
    }

    default:
    {
        TCU_THROW(InternalError, "Invalid allocation kind");
    }
    }
}

void checkExtensionSupport(Context &context, uint32_t flags)
{
    if (flags & COPY_COMMANDS_2)
    {
        if (!context.isDeviceFunctionalitySupported("VK_KHR_copy_commands2"))
            TCU_THROW(NotSupportedError, "VK_KHR_copy_commands2 is not supported");
    }

    if (flags & SEPARATE_DEPTH_STENCIL_LAYOUT)
    {
        if (!context.isDeviceFunctionalitySupported("VK_KHR_separate_depth_stencil_layouts"))
            TCU_THROW(NotSupportedError, "VK_KHR_separate_depth_stencil_layouts is not supported");
    }

    if (flags & MAINTENANCE_1)
    {
        if (!context.isDeviceFunctionalitySupported("VK_KHR_maintenance1"))
            TCU_THROW(NotSupportedError, "VK_KHR_maintenance1 is not supported");
    }

    if (flags & MAINTENANCE_5)
    {
        if (!context.isDeviceFunctionalitySupported("VK_KHR_maintenance5"))
            TCU_THROW(NotSupportedError, "VK_KHR_maintenance5 is not supported");
    }

    if (flags & INDIRECT_COPY)
    {
        if (!context.isDeviceFunctionalitySupported("VK_KHR_copy_memory_indirect"))
            TCU_THROW(NotSupportedError, "VK_KHR_copy_memory_indirect is not supported");
    }

    if (flags & SPARSE_BINDING)
        context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_SPARSE_BINDING);

    if (flags & MAINTENANCE_8)
        context.requireDeviceFunctionality("VK_KHR_maintenance8");

    if (flags & MAINTENANCE_10)
        context.requireDeviceFunctionality("VK_KHR_maintenance10");
}

uint32_t getArraySize(const ImageParms &parms)
{
    return (parms.imageType != VK_IMAGE_TYPE_3D) ? parms.extent.depth : 1u;
}

VkImageCreateFlags getCreateFlags(const ImageParms &parms)
{
    if (parms.createFlags == VK_IMAGE_CREATE_FLAG_BITS_MAX_ENUM)
        return parms.imageType == VK_IMAGE_TYPE_2D && parms.extent.depth % 6 == 0 ?
                   VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT :
                   0;
    else
        return parms.createFlags;
}

VkExtent3D getExtent3D(const ImageParms &parms, uint32_t mipLevel)
{
    const bool isCompressed    = isCompressedFormat(parms.format);
    const uint32_t blockWidth  = (isCompressed) ? getBlockWidth(parms.format) : 1u;
    const uint32_t blockHeight = (isCompressed) ? getBlockHeight(parms.format) : 1u;

    if (isCompressed && mipLevel != 0u)
        DE_FATAL("Not implemented");

    const VkExtent3D extent = {
        (parms.extent.width >> mipLevel) * blockWidth,
        (parms.imageType != VK_IMAGE_TYPE_1D) ? ((parms.extent.height >> mipLevel) * blockHeight) : 1u,
        (parms.imageType == VK_IMAGE_TYPE_3D) ? parms.extent.depth : 1u,
    };
    return extent;
}

const tcu::TextureFormat mapCombinedToDepthTransferFormat(const tcu::TextureFormat &combinedFormat)
{
    tcu::TextureFormat format;
    switch (combinedFormat.type)
    {
    case tcu::TextureFormat::UNORM_INT16:
    case tcu::TextureFormat::UNSIGNED_INT_16_8_8:
        format = tcu::TextureFormat(tcu::TextureFormat::D, tcu::TextureFormat::UNORM_INT16);
        break;
    case tcu::TextureFormat::UNSIGNED_INT_24_8_REV:
        format = tcu::TextureFormat(tcu::TextureFormat::D, tcu::TextureFormat::UNSIGNED_INT_24_8_REV);
        break;
    case tcu::TextureFormat::FLOAT_UNSIGNED_INT_24_8_REV:
    case tcu::TextureFormat::FLOAT:
        format = tcu::TextureFormat(tcu::TextureFormat::D, tcu::TextureFormat::FLOAT);
        break;
    default:
        DE_ASSERT(false);
        break;
    }
    return format;
}

void checkTransferQueueGranularity(Context &context, const VkExtent3D &extent, VkImageType imageType)
{
    const auto queueIndex = context.getTransferQueueFamilyIndex();
    if (queueIndex == -1)
        TCU_THROW(NotSupportedError, "No queue family found that only supports transfer queue.");

    const std::vector<VkQueueFamilyProperties> queueProps =
        getPhysicalDeviceQueueFamilyProperties(context.getInstanceInterface(), context.getPhysicalDevice());
    DE_ASSERT((int)queueProps.size() > queueIndex);
    const VkQueueFamilyProperties *xferProps = &queueProps[queueIndex];

    std::ostringstream ss;
    switch (imageType)
    {
    case VK_IMAGE_TYPE_1D:
        if (extent.width < xferProps->minImageTransferGranularity.width)
        {
            ss << "1d copy extent " << extent.width << " too small for queue granularity";
            TCU_THROW(NotSupportedError, ss.str());
        }
        break;
    case VK_IMAGE_TYPE_2D:
        if (extent.width < xferProps->minImageTransferGranularity.width ||
            extent.height < xferProps->minImageTransferGranularity.height)
        {
            ss << "2d copy extent (" << extent.width << ", " << extent.height << ") too small for queue granularity";
            TCU_THROW(NotSupportedError, ss.str());
        }
        break;
    case VK_IMAGE_TYPE_3D:
        if (extent.width < xferProps->minImageTransferGranularity.width ||
            extent.height < xferProps->minImageTransferGranularity.height ||
            extent.depth < xferProps->minImageTransferGranularity.depth)
        {
            ss << "3d copy extent (" << extent.width << ", " << extent.height << ", " << extent.depth
               << ") too small for queue granularity";
            TCU_THROW(NotSupportedError, ss.str());
        }
        break;
    default:
        DE_ASSERT(false && "Unexpected image type");
    }
}

std::string getSampleCountCaseName(VkSampleCountFlagBits sampleFlag)
{
    return de::toLower(de::toString(getSampleCountFlagsStr(sampleFlag)).substr(16));
}

std::string getFormatCaseName(VkFormat format)
{
    return de::toLower(de::toString(getFormatStr(format)).substr(10));
}

std::string getImageLayoutCaseName(VkImageLayout layout)
{
    switch (layout)
    {
    case VK_IMAGE_LAYOUT_GENERAL:
        return "general";
    case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
    case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
        return "optimal";
    default:
        DE_ASSERT(false);
        return "";
    }
}

bool isSupportedDepthStencilFormat(const InstanceInterface &vki, const VkPhysicalDevice physDevice,
                                   const VkFormat format)
{
    VkFormatProperties formatProps;
    vki.getPhysicalDeviceFormatProperties(physDevice, format, &formatProps);
    return (formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0;
}

tcu::Vec4 linearToSRGBIfNeeded(const tcu::TextureFormat &format, const tcu::Vec4 &color)
{
    return isSRGB(format) ? linearToSRGB(color) : color;
}

void scaleFromWholeSrcBuffer(const tcu::PixelBufferAccess &dst, const tcu::ConstPixelBufferAccess &src,
                             const VkOffset3D regionOffset, const VkOffset3D regionExtent,
                             tcu::Sampler::FilterMode filter, const MirrorMode mirrorMode)
{
    DE_ASSERT(filter == tcu::Sampler::LINEAR || filter == tcu::Sampler::CUBIC);

    tcu::Sampler sampler(tcu::Sampler::CLAMP_TO_EDGE, tcu::Sampler::CLAMP_TO_EDGE, tcu::Sampler::CLAMP_TO_EDGE, filter,
                         filter, 0.0f, false, tcu::Sampler::COMPAREMODE_NONE, 0, tcu::Vec4(0.0f), true);

    float sX = (float)regionExtent.x / (float)dst.getWidth();
    float sY = (float)regionExtent.y / (float)dst.getHeight();
    float sZ = (float)regionExtent.z / (float)dst.getDepth();

    for (int z = 0; z < dst.getDepth(); z++)
        for (int y = 0; y < dst.getHeight(); y++)
            for (int x = 0; x < dst.getWidth(); x++)
            {
                float srcX = ((mirrorMode & MIRROR_MODE_X) != 0) ?
                                 (float)regionExtent.x + (float)regionOffset.x - ((float)x + 0.5f) * sX :
                                 (float)regionOffset.x + ((float)x + 0.5f) * sX;
                float srcY = ((mirrorMode & MIRROR_MODE_Y) != 0) ?
                                 (float)regionExtent.y + (float)regionOffset.y - ((float)y + 0.5f) * sY :
                                 (float)regionOffset.y + ((float)y + 0.5f) * sY;
                float srcZ = ((mirrorMode & MIRROR_MODE_Z) != 0) ?
                                 (float)regionExtent.z + (float)regionOffset.z - ((float)z + 0.5f) * sZ :
                                 (float)regionOffset.z + ((float)z + 0.5f) * sZ;
                if (dst.getDepth() > 1)
                    dst.setPixel(linearToSRGBIfNeeded(dst.getFormat(), src.sample3D(sampler, filter, srcX, srcY, srcZ)),
                                 x, y, z);
                else
                    dst.setPixel(linearToSRGBIfNeeded(dst.getFormat(), src.sample2D(sampler, filter, srcX, srcY, 0)), x,
                                 y);
            }
}

void blit(const tcu::PixelBufferAccess &dst, const tcu::ConstPixelBufferAccess &src,
          const tcu::Sampler::FilterMode filter, const MirrorMode mirrorMode)
{
    DE_ASSERT(filter == tcu::Sampler::NEAREST || filter == tcu::Sampler::LINEAR || filter == tcu::Sampler::CUBIC);

    tcu::Sampler sampler(tcu::Sampler::CLAMP_TO_EDGE, tcu::Sampler::CLAMP_TO_EDGE, tcu::Sampler::CLAMP_TO_EDGE, filter,
                         filter, 0.0f, false, tcu::Sampler::COMPAREMODE_NONE, 0, tcu::Vec4(0.0f), true);

    const float sX = (float)src.getWidth() / (float)dst.getWidth();
    const float sY = (float)src.getHeight() / (float)dst.getHeight();
    const float sZ = (float)src.getDepth() / (float)dst.getDepth();

    const int xOffset = (mirrorMode & MIRROR_MODE_X) ? dst.getWidth() - 1 : 0;
    const int yOffset = (mirrorMode & MIRROR_MODE_Y) ? dst.getHeight() - 1 : 0;
    const int zOffset = (mirrorMode & MIRROR_MODE_Z) ? dst.getDepth() - 1 : 0;

    const int xScale = (mirrorMode & MIRROR_MODE_X) ? -1 : 1;
    const int yScale = (mirrorMode & MIRROR_MODE_Y) ? -1 : 1;
    const int zScale = (mirrorMode & MIRROR_MODE_Z) ? -1 : 1;

    for (int z = 0; z < dst.getDepth(); ++z)
        for (int y = 0; y < dst.getHeight(); ++y)
            for (int x = 0; x < dst.getWidth(); ++x)
            {
                dst.setPixel(
                    linearToSRGBIfNeeded(dst.getFormat(), src.sample3D(sampler, filter, ((float)x + 0.5f) * sX,
                                                                       ((float)y + 0.5f) * sY, ((float)z + 0.5f) * sZ)),
                    x * xScale + xOffset, y * yScale + yOffset, z * zScale + zOffset);
            }
}

void flipCoordinates(CopyRegion &region, const MirrorMode mirrorMode)
{
    const VkOffset3D dstOffset0 = region.imageBlit.dstOffsets[0];
    const VkOffset3D dstOffset1 = region.imageBlit.dstOffsets[1];
    const VkOffset3D srcOffset0 = region.imageBlit.srcOffsets[0];
    const VkOffset3D srcOffset1 = region.imageBlit.srcOffsets[1];

    if (mirrorMode != 0u)
    {
        //sourceRegion
        region.imageBlit.srcOffsets[0].x = std::min(srcOffset0.x, srcOffset1.x);
        region.imageBlit.srcOffsets[0].y = std::min(srcOffset0.y, srcOffset1.y);
        region.imageBlit.srcOffsets[0].z = std::min(srcOffset0.z, srcOffset1.z);

        region.imageBlit.srcOffsets[1].x = std::max(srcOffset0.x, srcOffset1.x);
        region.imageBlit.srcOffsets[1].y = std::max(srcOffset0.y, srcOffset1.y);
        region.imageBlit.srcOffsets[1].z = std::max(srcOffset0.z, srcOffset1.z);

        //destinationRegion
        region.imageBlit.dstOffsets[0].x = std::min(dstOffset0.x, dstOffset1.x);
        region.imageBlit.dstOffsets[0].y = std::min(dstOffset0.y, dstOffset1.y);
        region.imageBlit.dstOffsets[0].z = std::min(dstOffset0.z, dstOffset1.z);

        region.imageBlit.dstOffsets[1].x = std::max(dstOffset0.x, dstOffset1.x);
        region.imageBlit.dstOffsets[1].y = std::max(dstOffset0.y, dstOffset1.y);
        region.imageBlit.dstOffsets[1].z = std::max(dstOffset0.z, dstOffset1.z);
    }
}

MirrorMode getMirrorMode(const VkOffset3D from, const VkOffset3D to)
{
    MirrorMode mode = 0u;

    if (from.x > to.x)
        mode |= MIRROR_MODE_X;

    if (from.y > to.y)
        mode |= MIRROR_MODE_Y;

    if (from.z > to.z)
        mode |= MIRROR_MODE_Z;

    return mode;
}

MirrorMode getMirrorMode(const VkOffset3D s1, const VkOffset3D s2, const VkOffset3D d1, const VkOffset3D d2)
{
    static const MirrorModeBits kBits[] = {MIRROR_MODE_X, MIRROR_MODE_Y, MIRROR_MODE_Z};

    const MirrorMode source      = getMirrorMode(s1, s2);
    const MirrorMode destination = getMirrorMode(d1, d2);

    MirrorMode mode = 0u;

    for (int i = 0; i < DE_LENGTH_OF_ARRAY(kBits); ++i)
    {
        const MirrorModeBits bit = kBits[i];
        if ((source & bit) != (destination & bit))
            mode |= bit;
    }

    return mode;
}

float calculateFloatConversionError(int srcBits)
{
    if (srcBits > 0)
    {
        const int clampedBits   = de::clamp<int>(srcBits, 0, 32);
        const float srcMaxValue = de::max((float)(1ULL << clampedBits) - 1.0f, 1.0f);
        const float error       = 1.0f / srcMaxValue;

        return de::clamp<float>(error, 0.0f, 1.0f);
    }
    else
        return 1.0f;
}

tcu::Vec4 getFormatThreshold(const tcu::TextureFormat &format)
{
    tcu::Vec4 threshold(0.01f);

    switch (format.type)
    {
    case tcu::TextureFormat::HALF_FLOAT:
        threshold = tcu::Vec4(0.005f);
        break;

    case tcu::TextureFormat::FLOAT:
    case tcu::TextureFormat::FLOAT64:
        threshold = tcu::Vec4(0.001f);
        break;

    case tcu::TextureFormat::UNSIGNED_INT_11F_11F_10F_REV:
        threshold = tcu::Vec4(0.02f, 0.02f, 0.0625f, 1.0f);
        break;

    case tcu::TextureFormat::UNSIGNED_INT_999_E5_REV:
        threshold = tcu::Vec4(0.05f, 0.05f, 0.05f, 1.0f);
        break;

    case tcu::TextureFormat::UNORM_INT_1010102_REV:
        threshold = tcu::Vec4(0.002f, 0.002f, 0.002f, 0.3f);
        break;

    case tcu::TextureFormat::UNORM_INT8:
        threshold = tcu::Vec4(0.008f, 0.008f, 0.008f, 0.008f);
        break;

    default:
        const tcu::IVec4 bits = tcu::getTextureFormatMantissaBitDepth(format);
        threshold = tcu::Vec4(calculateFloatConversionError(bits.x()), calculateFloatConversionError(bits.y()),
                              calculateFloatConversionError(bits.z()), calculateFloatConversionError(bits.w()));
    }

    // Return value matching the channel order specified by the format
    if (format.order == tcu::TextureFormat::BGR || format.order == tcu::TextureFormat::BGRA)
        return threshold.swizzle(2, 1, 0, 3);
    else
        return threshold;
}

tcu::Vec4 getCompressedFormatThreshold(const tcu::CompressedTexFormat &format)
{
    bool isSigned(false);
    tcu::IVec4 bitDepth(0);

    switch (format)
    {
    case tcu::COMPRESSEDTEXFORMAT_EAC_SIGNED_R11:
        bitDepth = {7, 0, 0, 0};
        isSigned = true;
        break;

    case tcu::COMPRESSEDTEXFORMAT_EAC_R11:
        bitDepth = {8, 0, 0, 0};
        break;

    case tcu::COMPRESSEDTEXFORMAT_EAC_SIGNED_RG11:
        bitDepth = {7, 7, 0, 0};
        isSigned = true;
        break;

    case tcu::COMPRESSEDTEXFORMAT_EAC_RG11:
        bitDepth = {8, 8, 0, 0};
        break;

    case tcu::COMPRESSEDTEXFORMAT_ETC1_RGB8:
    case tcu::COMPRESSEDTEXFORMAT_ETC2_RGB8:
    case tcu::COMPRESSEDTEXFORMAT_ETC2_SRGB8:
        bitDepth = {8, 8, 8, 0};
        break;

    case tcu::COMPRESSEDTEXFORMAT_ETC2_RGB8_PUNCHTHROUGH_ALPHA1:
    case tcu::COMPRESSEDTEXFORMAT_ETC2_SRGB8_PUNCHTHROUGH_ALPHA1:
        bitDepth = {8, 8, 8, 1};
        break;

    case tcu::COMPRESSEDTEXFORMAT_ETC2_EAC_RGBA8:
    case tcu::COMPRESSEDTEXFORMAT_ETC2_EAC_SRGB8_ALPHA8:
    case tcu::COMPRESSEDTEXFORMAT_ASTC_4x4_RGBA:
    case tcu::COMPRESSEDTEXFORMAT_ASTC_5x4_RGBA:
    case tcu::COMPRESSEDTEXFORMAT_ASTC_5x5_RGBA:
    case tcu::COMPRESSEDTEXFORMAT_ASTC_6x5_RGBA:
    case tcu::COMPRESSEDTEXFORMAT_ASTC_6x6_RGBA:
    case tcu::COMPRESSEDTEXFORMAT_ASTC_8x5_RGBA:
    case tcu::COMPRESSEDTEXFORMAT_ASTC_8x6_RGBA:
    case tcu::COMPRESSEDTEXFORMAT_ASTC_8x8_RGBA:
    case tcu::COMPRESSEDTEXFORMAT_ASTC_10x5_RGBA:
    case tcu::COMPRESSEDTEXFORMAT_ASTC_10x6_RGBA:
    case tcu::COMPRESSEDTEXFORMAT_ASTC_10x8_RGBA:
    case tcu::COMPRESSEDTEXFORMAT_ASTC_10x10_RGBA:
    case tcu::COMPRESSEDTEXFORMAT_ASTC_12x10_RGBA:
    case tcu::COMPRESSEDTEXFORMAT_ASTC_12x12_RGBA:
    case tcu::COMPRESSEDTEXFORMAT_ASTC_4x4_SRGB8_ALPHA8:
    case tcu::COMPRESSEDTEXFORMAT_ASTC_5x4_SRGB8_ALPHA8:
    case tcu::COMPRESSEDTEXFORMAT_ASTC_5x5_SRGB8_ALPHA8:
    case tcu::COMPRESSEDTEXFORMAT_ASTC_6x5_SRGB8_ALPHA8:
    case tcu::COMPRESSEDTEXFORMAT_ASTC_6x6_SRGB8_ALPHA8:
    case tcu::COMPRESSEDTEXFORMAT_ASTC_8x5_SRGB8_ALPHA8:
    case tcu::COMPRESSEDTEXFORMAT_ASTC_8x6_SRGB8_ALPHA8:
    case tcu::COMPRESSEDTEXFORMAT_ASTC_8x8_SRGB8_ALPHA8:
    case tcu::COMPRESSEDTEXFORMAT_ASTC_10x5_SRGB8_ALPHA8:
    case tcu::COMPRESSEDTEXFORMAT_ASTC_10x6_SRGB8_ALPHA8:
    case tcu::COMPRESSEDTEXFORMAT_ASTC_10x8_SRGB8_ALPHA8:
    case tcu::COMPRESSEDTEXFORMAT_ASTC_10x10_SRGB8_ALPHA8:
    case tcu::COMPRESSEDTEXFORMAT_ASTC_12x10_SRGB8_ALPHA8:
    case tcu::COMPRESSEDTEXFORMAT_ASTC_12x12_SRGB8_ALPHA8:
        bitDepth = {8, 8, 8, 8};
        break;

    case tcu::COMPRESSEDTEXFORMAT_BC1_RGB_UNORM_BLOCK:
    case tcu::COMPRESSEDTEXFORMAT_BC1_RGB_SRGB_BLOCK:
    case tcu::COMPRESSEDTEXFORMAT_BC2_UNORM_BLOCK:
    case tcu::COMPRESSEDTEXFORMAT_BC2_SRGB_BLOCK:
    case tcu::COMPRESSEDTEXFORMAT_BC3_UNORM_BLOCK:
    case tcu::COMPRESSEDTEXFORMAT_BC3_SRGB_BLOCK:
        bitDepth = {5, 6, 5, 0};
        break;

    case tcu::COMPRESSEDTEXFORMAT_BC1_RGBA_UNORM_BLOCK:
    case tcu::COMPRESSEDTEXFORMAT_BC1_RGBA_SRGB_BLOCK:
    case tcu::COMPRESSEDTEXFORMAT_BC7_UNORM_BLOCK:
    case tcu::COMPRESSEDTEXFORMAT_BC7_SRGB_BLOCK:
        bitDepth = {5, 5, 5, 1};
        break;

    case tcu::COMPRESSEDTEXFORMAT_BC4_SNORM_BLOCK:
        bitDepth = {7, 0, 0, 0};
        isSigned = true;
        break;

    case tcu::COMPRESSEDTEXFORMAT_BC4_UNORM_BLOCK:
        bitDepth = {8, 0, 0, 0};
        break;

    case tcu::COMPRESSEDTEXFORMAT_BC5_SNORM_BLOCK:
        bitDepth = {7, 7, 0, 0};
        isSigned = true;
        break;

    case tcu::COMPRESSEDTEXFORMAT_BC5_UNORM_BLOCK:
        bitDepth = {8, 8, 0, 0};
        break;

    case tcu::COMPRESSEDTEXFORMAT_BC6H_SFLOAT_BLOCK:
        return tcu::Vec4(0.01f);
    case tcu::COMPRESSEDTEXFORMAT_BC6H_UFLOAT_BLOCK:
        return tcu::Vec4(0.005f);

    default:
        DE_ASSERT(false);
    }

    const float range = isSigned ? 1.0f - (-1.0f) : 1.0f - 0.0f;
    tcu::Vec4 v;
    for (int i = 0; i < 4; ++i)
    {
        if (bitDepth[i] == 0)
            v[i] = 1.0f;
        else
            v[i] = range / static_cast<float>((1 << bitDepth[i]) - 1);
    }
    return v;
}

VkSamplerCreateInfo makeSamplerCreateInfo()
{
    const VkSamplerCreateInfo defaultSamplerParams = {
        VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,   // VkStructureType sType;
        nullptr,                                 // const void* pNext;
        0u,                                      // VkSamplerCreateFlags flags;
        VK_FILTER_NEAREST,                       // VkFilter magFilter;
        VK_FILTER_NEAREST,                       // VkFilter minFilter;
        VK_SAMPLER_MIPMAP_MODE_NEAREST,          // VkSamplerMipmapMode mipmapMode;
        VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,   // VkSamplerAddressMode addressModeU;
        VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,   // VkSamplerAddressMode addressModeV;
        VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,   // VkSamplerAddressMode addressModeW;
        0.0f,                                    // float mipLodBias;
        VK_FALSE,                                // VkBool32 anisotropyEnable;
        1.0f,                                    // float maxAnisotropy;
        VK_FALSE,                                // VkBool32 compareEnable;
        VK_COMPARE_OP_NEVER,                     // VkCompareOp compareOp;
        0.0f,                                    // float minLod;
        0.25f,                                   // float maxLod;
        VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK, // VkBorderColor borderColor;
        VK_FALSE                                 // VkBool32 unnormalizedCoordinates;
    };

    return defaultSamplerParams;
}

VkImageViewType mapImageViewType(const VkImageType imageType)
{
    switch (imageType)
    {
    case VK_IMAGE_TYPE_1D:
        return VK_IMAGE_VIEW_TYPE_1D;
    case VK_IMAGE_TYPE_2D:
        return VK_IMAGE_VIEW_TYPE_2D;
    default:
        DE_ASSERT(false);
    }
    return VK_IMAGE_VIEW_TYPE_LAST;
}

tcu::IVec3 getSizeInBlocks(const VkFormat imageFormat, const VkImageType imageType, const VkExtent3D imageExtent)
{
    const tcu::CompressedTexFormat compressedFormat(mapVkCompressedFormat(imageFormat));
    const tcu::IVec3 blockSize = tcu::getBlockPixelSize(compressedFormat);
    const tcu::IVec3 size      = {static_cast<int>(imageExtent.width), static_cast<int>(imageExtent.height), 1};

    tcu::IVec3 actualBlockSize(1, 1, 1);

    switch (imageType)
    {
    case vk::VK_IMAGE_TYPE_1D:
        actualBlockSize = tcu::IVec3(blockSize.x(), 1, 1);
        break;
    case vk::VK_IMAGE_TYPE_2D:
        actualBlockSize = tcu::IVec3(blockSize.x(), blockSize.y(), 1);
        break;
    default:
        DE_ASSERT(false); // unsupported
    }

    return (size / actualBlockSize);
}

tcu::Vec4 getFloatOrFixedPointFormatThreshold(const tcu::TextureFormat &format)
{
    const tcu::TextureChannelClass channelClass = tcu::getTextureChannelClass(format.type);
    const tcu::IVec4 bitDepth                   = tcu::getTextureFormatBitDepth(format);

    if (channelClass == tcu::TEXTURECHANNELCLASS_FLOATING_POINT)
    {
        return getFormatThreshold(format);
    }
    else if (channelClass == tcu::TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT ||
             channelClass == tcu::TEXTURECHANNELCLASS_SIGNED_FIXED_POINT)
    {
        const bool isSigned = (channelClass == tcu::TEXTURECHANNELCLASS_SIGNED_FIXED_POINT);
        const float range   = isSigned ? 1.0f - (-1.0f) : 1.0f - 0.0f;

        tcu::Vec4 v;
        for (int i = 0; i < 4; ++i)
        {
            if (bitDepth[i] == 0)
                v[i] = 1.0f;
            else
                v[i] = range / static_cast<float>((1 << bitDepth[i]) - 1);
        }
        return v;
    }
    else
    {
        DE_ASSERT(0);
        return tcu::Vec4();
    }
}

bool floatNearestBlitCompare(const tcu::ConstPixelBufferAccess &source, const tcu::ConstPixelBufferAccess &result,
                             const tcu::Vec4 &sourceThreshold, const tcu::Vec4 &resultThreshold,
                             const tcu::PixelBufferAccess &errorMask, const TestParams &params)
{
    const tcu::Sampler sampler(tcu::Sampler::CLAMP_TO_EDGE, tcu::Sampler::CLAMP_TO_EDGE, tcu::Sampler::CLAMP_TO_EDGE,
                               tcu::Sampler::NEAREST, tcu::Sampler::NEAREST, 0.0f, true, tcu::Sampler::COMPAREMODE_NONE,
                               0, tcu::Vec4(0.0f), true);
    const tcu::IVec4 dstBitDepth(tcu::getTextureFormatBitDepth(result.getFormat()));
    tcu::LookupPrecision precision;

    precision.colorMask      = tcu::notEqual(dstBitDepth, tcu::IVec4(0));
    precision.colorThreshold = tcu::max(sourceThreshold, resultThreshold);

    const struct Capture
    {
        const tcu::ConstPixelBufferAccess &source;
        const tcu::ConstPixelBufferAccess &result;
        const tcu::Sampler &sampler;
        const tcu::LookupPrecision &precision;
        const TestParams &params;
        const bool isSRGB;
    } capture = {source, result, sampler, precision, params, tcu::isSRGB(result.getFormat())};

    const struct Loop : CompareEachPixelInEachRegion
    {
        Loop(void)
        {
        }

        bool compare(const void *pUserData, const int x, const int y, const int z, const tcu::Vec3 &srcNormCoord) const
        {
            const Capture &c                                  = *static_cast<const Capture *>(pUserData);
            const tcu::TexLookupScaleMode lookupScaleDontCare = tcu::TEX_LOOKUP_SCALE_MINIFY;
            tcu::Vec4 dstColor                                = c.result.getPixel(x, y, z);

            // TexLookupVerifier performs a conversion to linear space, so we have to as well
            if (c.isSRGB)
                dstColor = tcu::sRGBToLinear(dstColor);

            return tcu::isLevel3DLookupResultValid(c.source, c.sampler, lookupScaleDontCare, c.precision, srcNormCoord,
                                                   dstColor);
        }
    } loop;

    return loop.forEach(&capture, params, source.getWidth(), source.getHeight(), source.getDepth(), errorMask);
}

bool intNearestBlitCompare(const tcu::ConstPixelBufferAccess &source, const tcu::ConstPixelBufferAccess &result,
                           const tcu::PixelBufferAccess &errorMask, const TestParams &params)
{
    const tcu::Sampler sampler(tcu::Sampler::CLAMP_TO_EDGE, tcu::Sampler::CLAMP_TO_EDGE, tcu::Sampler::CLAMP_TO_EDGE,
                               tcu::Sampler::NEAREST, tcu::Sampler::NEAREST, 0.0f, true, tcu::Sampler::COMPAREMODE_NONE,
                               0, tcu::Vec4(0.0f), true);
    tcu::IntLookupPrecision precision;

    {
        const tcu::IVec4 srcBitDepth = tcu::getTextureFormatBitDepth(source.getFormat());
        const tcu::IVec4 dstBitDepth = tcu::getTextureFormatBitDepth(result.getFormat());

        for (uint32_t i = 0; i < 4; ++i)
        {
            precision.colorThreshold[i] = de::max(de::max(srcBitDepth[i] / 8, dstBitDepth[i] / 8), 1);
            precision.colorMask[i]      = dstBitDepth[i] != 0;
        }
    }

    // Prepare a source image with a matching (converted) pixel format. Ideally, we would've used a wrapper that
    // does the conversion on the fly without wasting memory, but this approach is more straightforward.
    tcu::TextureLevel convertedSourceTexture(result.getFormat(), source.getWidth(), source.getHeight(),
                                             source.getDepth());
    const tcu::PixelBufferAccess convertedSource = convertedSourceTexture.getAccess();

    for (int z = 0; z < source.getDepth(); ++z)
        for (int y = 0; y < source.getHeight(); ++y)
            for (int x = 0; x < source.getWidth(); ++x)
                convertedSource.setPixel(source.getPixelInt(x, y, z), x, y,
                                         z); // will be clamped to max. representable value

    const struct Capture
    {
        const tcu::ConstPixelBufferAccess &source;
        const tcu::ConstPixelBufferAccess &result;
        const tcu::Sampler &sampler;
        const tcu::IntLookupPrecision &precision;
    } capture = {convertedSource, result, sampler, precision};

    const struct Loop : CompareEachPixelInEachRegion
    {
        Loop(void)
        {
        }

        bool compare(const void *pUserData, const int x, const int y, const int z, const tcu::Vec3 &srcNormCoord) const
        {
            const Capture &c                                  = *static_cast<const Capture *>(pUserData);
            const tcu::TexLookupScaleMode lookupScaleDontCare = tcu::TEX_LOOKUP_SCALE_MINIFY;
            const tcu::IVec4 dstColor                         = c.result.getPixelInt(x, y, z);

            return tcu::isLevel3DLookupResultValid(c.source, c.sampler, lookupScaleDontCare, c.precision, srcNormCoord,
                                                   dstColor);
        }
    } loop;

    return loop.forEach(&capture, params, source.getWidth(), source.getHeight(), source.getDepth(), errorMask);
}

// Classes used for performing copies and blitting operations
CopiesAndBlittingTestInstance::CopiesAndBlittingTestInstance(Context &context, TestParams testParams)
    : vkt::TestInstance(context)
    , m_params(testParams)
{
    // Store default device, queue and allocator. Some tests override these with custom device and queue.
    m_device    = context.getDevice();
    m_allocator = &context.getDefaultAllocator();

    const DeviceInterface &vk = context.getDeviceInterface();

    int queueFamilyIdx = context.getUniversalQueueFamilyIndex();
    m_queueFamilyIndices.push_back(queueFamilyIdx);

    switch (m_params.queueSelection)
    {
    case QueueSelectionOptions::ComputeOnly:
    {
        m_otherQueue   = context.getComputeQueue();
        queueFamilyIdx = context.getComputeQueueFamilyIndex();
        TCU_CHECK_INTERNAL(queueFamilyIdx != -1);
        m_queueFamilyIndices.push_back(queueFamilyIdx);
        m_otherCmdPool =
            createCommandPool(vk, m_device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIdx);
        m_otherCmdBuffer = allocateCommandBuffer(vk, m_device, *m_otherCmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
        break;
    }
    case QueueSelectionOptions::TransferOnly:
    {
        m_otherQueue   = context.getTransferQueue();
        queueFamilyIdx = context.getTransferQueueFamilyIndex();
        TCU_CHECK_INTERNAL(queueFamilyIdx != -1);
        m_queueFamilyIndices.push_back(queueFamilyIdx);
        m_otherCmdPool =
            createCommandPool(vk, m_device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIdx);
        m_otherCmdBuffer = allocateCommandBuffer(vk, m_device, *m_otherCmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
        break;
    }
    case QueueSelectionOptions::Universal:
    {
        break; // Unconditionally created below.
    }
    }

    m_universalQueue     = context.getUniversalQueue();
    m_universalCmdPool   = createCommandPool(vk, m_device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
                                             context.getUniversalQueueFamilyIndex());
    m_universalCmdBuffer = allocateCommandBuffer(vk, m_device, *m_universalCmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

    if (m_params.useSecondaryCmdBuffer)
    {
        m_secondaryCmdPool =
            createCommandPool(vk, m_device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIdx);
        m_secondaryCmdBuffer =
            allocateCommandBuffer(vk, m_device, *m_secondaryCmdPool, VK_COMMAND_BUFFER_LEVEL_SECONDARY);
    }
}

void CopiesAndBlittingTestInstance::generateBuffer(tcu::PixelBufferAccess buffer, int width, int height, int depth,
                                                   FillMode mode)
{
    const tcu::TextureChannelClass channelClass = tcu::getTextureChannelClass(buffer.getFormat().type);
    tcu::Vec4 maxValue(1.0f);

    if (buffer.getFormat().order == tcu::TextureFormat::S)
    {
        // Stencil-only is stored in the first component. Stencil is always 8 bits.
        maxValue.x() = 1 << 8;
    }
    else if (buffer.getFormat().order == tcu::TextureFormat::DS)
    {
        // In a combined format, fillWithComponentGradients expects stencil in the fourth component.
        maxValue.w() = 1 << 8;
    }
    else if (channelClass == tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER ||
             channelClass == tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER)
    {
        // The tcu::Vectors we use as pixels are 32-bit, so clamp to that.
        const tcu::IVec4 bits = tcu::min(tcu::getTextureFormatBitDepth(buffer.getFormat()), tcu::IVec4(32));
        const int signBit     = (channelClass == tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER ? 1 : 0);

        for (int i = 0; i < 4; ++i)
        {
            if (bits[i] != 0)
                maxValue[i] = static_cast<float>((uint64_t(1) << (bits[i] - signBit)) - 1);
        }
    }

    if (mode == FILL_MODE_GRADIENT)
    {
        tcu::fillWithComponentGradients2(buffer, tcu::Vec4(0.0f, 0.0f, 0.0f, 0.0f), maxValue);
        return;
    }

    if (mode == FILL_MODE_PYRAMID)
    {
        tcu::fillWithComponentGradients3(buffer, tcu::Vec4(0.0f, 0.0f, 0.0f, 0.0f), maxValue);
        return;
    }

    de::Random randomGen(deInt32Hash((uint32_t)buffer.getFormat().type));
    const tcu::Vec4 redColor(maxValue.x(), 0.0, 0.0, maxValue.w());
    const tcu::Vec4 greenColor(0.0, maxValue.y(), 0.0, maxValue.w());
    const tcu::Vec4 blueColor(0.0, 0.0, maxValue.z(), maxValue.w());
    const tcu::Vec4 whiteColor(maxValue.x(), maxValue.y(), maxValue.z(), maxValue.w());
    const tcu::Vec4 blackColor(0.0f, 0.0f, 0.0f, 0.0f);

    for (int z = 0; z < depth; ++z)
        for (int y = 0; y < height; ++y)
            for (int x = 0; x < width; ++x)
            {
                switch (mode)
                {
                case FILL_MODE_WHITE:
                    if (tcu::isCombinedDepthStencilType(buffer.getFormat().type))
                    {
                        buffer.setPixDepth(1.0f, x, y, z);
                        if (tcu::hasStencilComponent(buffer.getFormat().order))
                            buffer.setPixStencil(255, x, y, z);
                    }
                    else
                        buffer.setPixel(whiteColor, x, y, z);
                    break;

                case FILL_MODE_BLACK:
                    if (tcu::isCombinedDepthStencilType(buffer.getFormat().type))
                    {
                        buffer.setPixDepth(0.0f, x, y, z);
                        if (tcu::hasStencilComponent(buffer.getFormat().order))
                            buffer.setPixStencil(0, x, y, z);
                    }
                    else
                        buffer.setPixel(blackColor, x, y, z);
                    break;

                case FILL_MODE_RED:
                    if (tcu::isCombinedDepthStencilType(buffer.getFormat().type))
                    {
                        buffer.setPixDepth(redColor[0], x, y, z);
                        if (tcu::hasStencilComponent(buffer.getFormat().order))
                            buffer.setPixStencil((int)redColor[3], x, y, z);
                    }
                    else
                        buffer.setPixel(redColor, x, y, z);
                    break;

                case FILL_MODE_RANDOM_GRAY:
                {
                    // generate random gray color but multiply it by 0.95 to not generate
                    // value that can be interpreted as NaNs when copied to float formats
                    tcu::Vec4 randomGrayColor(randomGen.getFloat() * 0.95f);
                    randomGrayColor.w() = maxValue.w();
                    buffer.setPixel(randomGrayColor, x, y, z);
                    break;
                }

                case FILL_MODE_BLUE_RED_X:
                case FILL_MODE_BLUE_RED_Y:
                case FILL_MODE_BLUE_RED_Z:
                    bool useBlue;
                    switch (mode)
                    {
                    case FILL_MODE_BLUE_RED_X:
                        useBlue = (x & 1);
                        break;
                    case FILL_MODE_BLUE_RED_Y:
                        useBlue = (y & 1);
                        break;
                    case FILL_MODE_BLUE_RED_Z:
                        useBlue = (z & 1);
                        break;
                    default:
                        DE_ASSERT(false);
                        break;
                    }
                    if (tcu::isCombinedDepthStencilType(buffer.getFormat().type))
                    {
                        buffer.setPixDepth((useBlue ? blueColor[0] : redColor[0]), x, y, z);
                        if (tcu::hasStencilComponent(buffer.getFormat().order))
                            buffer.setPixStencil((useBlue ? (int)blueColor[3] : (int)redColor[3]), x, y, z);
                    }
                    else
                        buffer.setPixel((useBlue ? blueColor : redColor), x, y, z);
                    break;

                case FILL_MODE_MULTISAMPLE:
                {
                    float xScaled = static_cast<float>(x) / static_cast<float>(width);
                    float yScaled = static_cast<float>(y) / static_cast<float>(height);
                    buffer.setPixel((xScaled == yScaled) ? tcu::Vec4(0.0, 0.5, 0.5, 1.0) :
                                                           ((xScaled > yScaled) ? greenColor : blueColor),
                                    x, y, z);
                    break;
                }

                default:
                    break;
                }
            }
}

void CopiesAndBlittingTestInstance::uploadBuffer(const tcu::ConstPixelBufferAccess &bufferAccess,
                                                 const Allocation &bufferAlloc)
{
    const DeviceInterface &vk = m_context.getDeviceInterface();
    const uint32_t bufferSize = calculateSize(bufferAccess);

    // Write buffer data
    deMemcpy(bufferAlloc.getHostPtr(), bufferAccess.getDataPtr(), bufferSize);
    flushAlloc(vk, m_device, bufferAlloc);
}

void submitCommandsAndWaitWithSync(const DeviceInterface &vkd, VkDevice device, VkQueue queue,
                                   VkCommandBuffer cmdBuffer, Move<VkSemaphore> *waitSemaphore,
                                   VkPipelineStageFlags waitStages)
{
    std::vector<VkSemaphore> waitSemaphores;
    std::vector<VkPipelineStageFlags> waitStagesVec;

    if (waitSemaphore != nullptr && waitSemaphore->get() != VK_NULL_HANDLE)
    {
        waitSemaphores.push_back(**waitSemaphore);
        waitStagesVec.push_back(waitStages);
    }

    DE_ASSERT(waitSemaphores.size() == waitStagesVec.size());

    submitCommandsAndWait(vkd, device, queue, cmdBuffer, false, 1u, de::sizeU32(waitSemaphores),
                          de::dataOrNull(waitSemaphores), de::dataOrNull(waitStagesVec));

    // Destroy semaphore after work completes.
    if (waitSemaphore != nullptr)
        *waitSemaphore = Move<VkSemaphore>();
}

void submitCommandsAndWaitWithTransferSync(const DeviceInterface &vkd, VkDevice device, VkQueue queue,
                                           VkCommandBuffer cmdBuffer, Move<VkSemaphore> *waitSemaphore,
                                           bool indirectCopy)
{
    (void)indirectCopy;
    auto waitStages = static_cast<VkPipelineStageFlags>(VK_PIPELINE_STAGE_TRANSFER_BIT);
#ifndef CTS_USES_VULKANSC
    if (indirectCopy)
        waitStages = static_cast<VkPipelineStageFlags>(VK_PIPELINE_STAGE_2_COPY_INDIRECT_BIT_KHR);
#endif
    submitCommandsAndWaitWithSync(vkd, device, queue, cmdBuffer, waitSemaphore, waitStages);
}

void CopiesAndBlittingTestInstance::uploadImageAspect(const tcu::ConstPixelBufferAccess &imageAccess,
                                                      const VkImage &image, const ImageParms &parms,
                                                      const uint32_t mipLevels, const bool useGeneralLayout,
                                                      Move<VkSemaphore> *semaphore)
{
    const InstanceInterface &vki        = m_context.getInstanceInterface();
    const DeviceInterface &vk           = m_context.getDeviceInterface();
    const VkPhysicalDevice vkPhysDevice = m_context.getPhysicalDevice();
    const VkDevice vkDevice             = m_device;
    Allocator &memAlloc                 = *m_allocator;
    Move<VkBuffer> buffer;
    const uint32_t bufferSize = calculateSize(imageAccess);
    de::MovePtr<Allocation> bufferAlloc;
    const uint32_t arraySize     = getArraySize(parms);
    const VkExtent3D imageExtent = getExtent3D(parms);
    std::vector<VkBufferImageCopy> copyRegions;

    // Create source buffer
    {
        const VkBufferCreateInfo bufferParams = {
            VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, // VkStructureType sType;
            nullptr,                              // const void* pNext;
            0u,                                   // VkBufferCreateFlags flags;
            bufferSize,                           // VkDeviceSize size;
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,     // VkBufferUsageFlags usage;
            m_queueFamilyIndices.size() > 1 ? VK_SHARING_MODE_CONCURRENT :
                                              VK_SHARING_MODE_EXCLUSIVE, // VkSharingMode sharingMode;
            (uint32_t)m_queueFamilyIndices.size(),                       // uint32_t queueFamilyIndexCount;
            m_queueFamilyIndices.data(),                                 // const uint32_t* pQueueFamilyIndices;
        };

        buffer      = createBuffer(vk, vkDevice, &bufferParams);
        bufferAlloc = allocateBuffer(vki, vk, vkPhysDevice, vkDevice, *buffer, MemoryRequirement::HostVisible, memAlloc,
                                     m_params.allocationKind);
        VK_CHECK(vk.bindBufferMemory(vkDevice, *buffer, bufferAlloc->getMemory(), bufferAlloc->getOffset()));
    }

    // Barriers for copying buffer to image
    const VkBufferMemoryBarrier preBufferBarrier =
        makeBufferMemoryBarrier(VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, *buffer, 0u, bufferSize);

    const VkImageAspectFlags formatAspect = (m_params.extensionFlags & SEPARATE_DEPTH_STENCIL_LAYOUT) ?
                                                getAspectFlags(imageAccess.getFormat()) :
                                                getAspectFlags(parms.format);
    const bool skipPreImageBarrier        = (m_params.extensionFlags & SEPARATE_DEPTH_STENCIL_LAYOUT) ?
                                                false :
                                                ((formatAspect == (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT) &&
                                           getAspectFlags(imageAccess.getFormat()) == VK_IMAGE_ASPECT_STENCIL_BIT));

    const VkMemoryBarrier postMemoryBarrier = makeMemoryBarrier(0u, VK_ACCESS_TRANSFER_WRITE_BIT);

    const VkImageLayout layout = useGeneralLayout ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    const VkImageMemoryBarrier preImageBarrier =
        makeImageMemoryBarrier(0u, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, layout, image,
                               makeImageSubresourceRange(formatAspect, 0u, mipLevels, 0u, arraySize));

    const VkImageMemoryBarrier postImageBarrier =
        makeImageMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, image,
                               makeImageSubresourceRange(formatAspect, 0u, mipLevels, 0u, arraySize));

    uint32_t blockWidth, blockHeight;
    std::tie(blockWidth, blockHeight) = parms.texelBlockDimensions();

    for (uint32_t mipLevelNdx = 0; mipLevelNdx < mipLevels; mipLevelNdx++)
    {
        const VkExtent3D copyExtent =
            makeExtent3D(imageExtent.width >> mipLevelNdx, imageExtent.height >> mipLevelNdx, imageExtent.depth);
        uint32_t rowLength                 = ((copyExtent.width + blockWidth - 1) / blockWidth) * blockWidth;
        uint32_t imageHeight               = ((copyExtent.height + blockHeight - 1) / blockHeight) * blockHeight;
        const VkBufferImageCopy copyRegion = {
            0u,          // VkDeviceSize bufferOffset;
            rowLength,   // uint32_t bufferRowLength;
            imageHeight, // uint32_t bufferImageHeight;
            {
                getAspectFlags(imageAccess.getFormat()), // VkImageAspectFlags aspect;
                mipLevelNdx,                             // uint32_t mipLevel;
                0u,                                      // uint32_t baseArrayLayer;
                arraySize,                               // uint32_t layerCount;
            },                                           // VkImageSubresourceLayers imageSubresource;
            {0, 0, 0},                                   // VkOffset3D imageOffset;
            copyExtent                                   // VkExtent3D imageExtent;
        };

        copyRegions.push_back(copyRegion);
    }

    // Write buffer data
    deMemcpy(bufferAlloc->getHostPtr(), imageAccess.getDataPtr(), bufferSize);
    flushAlloc(vk, vkDevice, *bufferAlloc);

    // Copy buffer to image on the universal queue, since not all image aspects may be transferred on dedicated queues.
    beginCommandBuffer(vk, *m_universalCmdBuffer);
    vk.cmdPipelineBarrier(*m_universalCmdBuffer, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                          (VkDependencyFlags)0, 0, nullptr, 1, &preBufferBarrier, ((skipPreImageBarrier) ? 0 : 1),
                          &preImageBarrier);
    vk.cmdCopyBufferToImage(*m_universalCmdBuffer, *buffer, image, layout, (uint32_t)copyRegions.size(),
                            &copyRegions[0]);
    vk.cmdPipelineBarrier(*m_universalCmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                          (VkDependencyFlags)0, (useGeneralLayout ? 1 : 0), &postMemoryBarrier, 0, nullptr,
                          (useGeneralLayout ? 0 : 1), &postImageBarrier);
    endCommandBuffer(vk, *m_universalCmdBuffer);

    submitCommandsAndWaitWithTransferSync(vk, vkDevice, m_universalQueue, *m_universalCmdBuffer, semaphore);

    m_context.resetCommandPoolForVKSC(vkDevice, *m_universalCmdPool);
}

void CopiesAndBlittingTestInstance::uploadImage(const tcu::ConstPixelBufferAccess &src, VkImage dst,
                                                const ImageParms &parms, const uint32_t mipLevels,
                                                const bool useGeneralLayout, Move<VkSemaphore> *semaphore)
{
    if (tcu::isCombinedDepthStencilType(src.getFormat().type))
    {
        if (tcu::hasDepthComponent(src.getFormat().order))
        {
            tcu::TextureLevel depthTexture(mapCombinedToDepthTransferFormat(src.getFormat()), src.getWidth(),
                                           src.getHeight(), src.getDepth());
            tcu::copy(depthTexture.getAccess(), tcu::getEffectiveDepthStencilAccess(src, tcu::Sampler::MODE_DEPTH));
            uploadImageAspect(depthTexture.getAccess(), dst, parms, mipLevels, useGeneralLayout, semaphore);
        }

        if (tcu::hasStencilComponent(src.getFormat().order))
        {
            tcu::TextureLevel stencilTexture(
                tcu::getEffectiveDepthStencilTextureFormat(src.getFormat(), tcu::Sampler::MODE_STENCIL), src.getWidth(),
                src.getHeight(), src.getDepth());
            tcu::copy(stencilTexture.getAccess(), tcu::getEffectiveDepthStencilAccess(src, tcu::Sampler::MODE_STENCIL));
            uploadImageAspect(stencilTexture.getAccess(), dst, parms, mipLevels, useGeneralLayout, semaphore);
        }
    }
    else
        uploadImageAspect(src, dst, parms, mipLevels, useGeneralLayout, semaphore);
}

tcu::TestStatus CopiesAndBlittingTestInstance::checkTestResult(tcu::ConstPixelBufferAccess result)
{
    const tcu::ConstPixelBufferAccess expected = m_expectedTextureLevel[0]->getAccess();

    if (isFloatFormat(result.getFormat()))
    {
        const tcu::Vec4 threshold(0.0f);
        if (!tcu::floatThresholdCompare(m_context.getTestContext().getLog(), "Compare", "Result comparison", expected,
                                        result, threshold, tcu::COMPARE_LOG_ON_ERROR))
            return tcu::TestStatus::fail("CopiesAndBlitting test");
    }
    else
    {
        const tcu::UVec4 threshold(0u);
        if (tcu::hasDepthComponent(result.getFormat().order) || tcu::hasStencilComponent(result.getFormat().order))
        {
            if (!tcu::dsThresholdCompare(m_context.getTestContext().getLog(), "Compare", "Result comparison", expected,
                                         result, 0.1f, tcu::COMPARE_LOG_ON_ERROR))
                return tcu::TestStatus::fail("CopiesAndBlitting test");
        }
        else
        {
            if (!tcu::intThresholdCompare(m_context.getTestContext().getLog(), "Compare", "Result comparison", expected,
                                          result, threshold, tcu::COMPARE_LOG_ON_ERROR))
                return tcu::TestStatus::fail("CopiesAndBlitting test");
        }
    }

    return tcu::TestStatus::pass("CopiesAndBlitting test");
}

void CopiesAndBlittingTestInstance::generateExpectedResult()
{
    const tcu::ConstPixelBufferAccess src = m_sourceTextureLevel->getAccess();
    const tcu::ConstPixelBufferAccess dst = m_destinationTextureLevel->getAccess();

    m_expectedTextureLevel[0] = de::MovePtr<tcu::TextureLevel>(
        new tcu::TextureLevel(dst.getFormat(), dst.getWidth(), dst.getHeight(), dst.getDepth()));
    tcu::copy(m_expectedTextureLevel[0]->getAccess(), dst);

    for (uint32_t i = 0; i < m_params.regions.size(); i++)
        copyRegionToTextureLevel(src, m_expectedTextureLevel[0]->getAccess(), m_params.regions[i]);
}

void CopiesAndBlittingTestInstance::generateExpectedResult(CopyRegion *region)
{
    const tcu::ConstPixelBufferAccess src = m_sourceTextureLevel->getAccess();
    const tcu::ConstPixelBufferAccess dst = m_destinationTextureLevel->getAccess();

    m_expectedTextureLevel[0] = de::MovePtr<tcu::TextureLevel>(
        new tcu::TextureLevel(dst.getFormat(), dst.getWidth(), dst.getHeight(), dst.getDepth()));
    tcu::copy(m_expectedTextureLevel[0]->getAccess(), dst);

    copyRegionToTextureLevel(src, m_expectedTextureLevel[0]->getAccess(), *region);
}

void CopiesAndBlittingTestInstance::readImageAspect(vk::VkImage image, const tcu::PixelBufferAccess &dst,
                                                    const ImageParms &imageParms, const uint32_t mipLevel,
                                                    const bool useGeneralLayout, Move<VkSemaphore> *semaphore)
{
    const InstanceInterface &vki      = m_context.getInstanceInterface();
    const DeviceInterface &vk         = m_context.getDeviceInterface();
    const VkPhysicalDevice physDevice = m_context.getPhysicalDevice();
    const VkDevice device             = m_device;
    Allocator &allocator              = *m_allocator;

    Move<VkBuffer> buffer;
    de::MovePtr<Allocation> bufferAlloc;
    const VkDeviceSize pixelDataSize = calculateSize(dst);
    const VkExtent3D imageExtent     = getExtent3D(imageParms, mipLevel);

    // Create destination buffer
    {
        const VkBufferCreateInfo bufferParams = {
            VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, // VkStructureType sType;
            nullptr,                              // const void* pNext;
            0u,                                   // VkBufferCreateFlags flags;
            pixelDataSize,                        // VkDeviceSize size;
            VK_BUFFER_USAGE_TRANSFER_DST_BIT,     // VkBufferUsageFlags usage;
            m_queueFamilyIndices.size() > 1 ? VK_SHARING_MODE_CONCURRENT :
                                              VK_SHARING_MODE_EXCLUSIVE, // VkSharingMode sharingMode;
            (uint32_t)m_queueFamilyIndices.size(),                       // uint32_t queueFamilyIndexCount;
            m_queueFamilyIndices.data(),                                 // const uint32_t* pQueueFamilyIndices;
        };

        buffer      = createBuffer(vk, device, &bufferParams);
        bufferAlloc = allocateBuffer(vki, vk, physDevice, device, *buffer, MemoryRequirement::HostVisible, allocator,
                                     m_params.allocationKind);
        VK_CHECK(vk.bindBufferMemory(device, *buffer, bufferAlloc->getMemory(), bufferAlloc->getOffset()));

        deMemset(bufferAlloc->getHostPtr(), 0, static_cast<size_t>(pixelDataSize));
        flushAlloc(vk, device, *bufferAlloc);
    }

    // Barriers for copying image to buffer
    const VkImageAspectFlags formatAspect = getAspectFlags(imageParms.format);
    const VkMemoryBarrier memoryBarrier = makeMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
    const VkImageMemoryBarrier imageBarrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, // VkStructureType sType;
                                               nullptr,                                // const void* pNext;
                                               VK_ACCESS_TRANSFER_WRITE_BIT,           // VkAccessFlags srcAccessMask;
                                               VK_ACCESS_TRANSFER_READ_BIT,            // VkAccessFlags dstAccessMask;
                                               imageParms.operationLayout,             // VkImageLayout oldLayout;
                                               VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,   // VkImageLayout newLayout;
                                               VK_QUEUE_FAMILY_IGNORED,                // uint32_t srcQueueFamilyIndex;
                                               VK_QUEUE_FAMILY_IGNORED,                // uint32_t dstQueueFamilyIndex;
                                               image,                                  // VkImage image;
                                               {
                                                   // VkImageSubresourceRange subresourceRange;
                                                   formatAspect,            // VkImageAspectFlags aspectMask;
                                                   mipLevel,                // uint32_t baseMipLevel;
                                                   1u,                      // uint32_t mipLevels;
                                                   0u,                      // uint32_t baseArraySlice;
                                                   getArraySize(imageParms) // uint32_t arraySize;
                                               }};

    const VkBufferMemoryBarrier bufferBarrier = {
        VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER, // VkStructureType sType;
        nullptr,                                 // const void* pNext;
        VK_ACCESS_TRANSFER_WRITE_BIT,            // VkAccessFlags srcAccessMask;
        VK_ACCESS_HOST_READ_BIT,                 // VkAccessFlags dstAccessMask;
        VK_QUEUE_FAMILY_IGNORED,                 // uint32_t srcQueueFamilyIndex;
        VK_QUEUE_FAMILY_IGNORED,                 // uint32_t dstQueueFamilyIndex;
        *buffer,                                 // VkBuffer buffer;
        0u,                                      // VkDeviceSize offset;
        pixelDataSize                            // VkDeviceSize size;
    };

    const VkMemoryBarrier postMemoryBarrier =
        makeMemoryBarrier(VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_TRANSFER_WRITE_BIT);
    const VkImageMemoryBarrier postImageBarrier = {
        VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, // VkStructureType sType;
        nullptr,                                // const void* pNext;
        VK_ACCESS_TRANSFER_READ_BIT,            // VkAccessFlags srcAccessMask;
        VK_ACCESS_TRANSFER_WRITE_BIT,           // VkAccessFlags dstAccessMask;
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,   // VkImageLayout oldLayout;
        imageParms.operationLayout,             // VkImageLayout newLayout;
        VK_QUEUE_FAMILY_IGNORED,                // uint32_t srcQueueFamilyIndex;
        VK_QUEUE_FAMILY_IGNORED,                // uint32_t dstQueueFamilyIndex;
        image,                                  // VkImage image;
        {
            formatAspect,            // VkImageAspectFlags aspectMask;
            mipLevel,                // uint32_t baseMipLevel;
            1u,                      // uint32_t mipLevels;
            0u,                      // uint32_t baseArraySlice;
            getArraySize(imageParms) // uint32_t arraySize;
        }                            // VkImageSubresourceRange subresourceRange;
    };

    // Copy image to buffer
    const bool isCompressed    = isCompressedFormat(imageParms.format);
    const uint32_t blockWidth  = (isCompressed) ? getBlockWidth(imageParms.format) : 1u;
    const uint32_t blockHeight = (isCompressed) ? getBlockHeight(imageParms.format) : 1u;
    uint32_t rowLength         = ((imageExtent.width + blockWidth - 1) / blockWidth) * blockWidth;
    uint32_t imageHeight       = ((imageExtent.height + blockHeight - 1) / blockHeight) * blockHeight;

    // Copy image to buffer - note that there are cases where m_params.dst.image.format is not the same as dst.getFormat()
    const VkImageAspectFlags aspect    = isCompressedFormat(m_params.dst.image.format) ?
                                             static_cast<VkImageAspectFlags>(VK_IMAGE_ASPECT_COLOR_BIT) :
                                             getAspectFlags(dst.getFormat());
    const VkBufferImageCopy copyRegion = {
        0u,          // VkDeviceSize bufferOffset;
        rowLength,   // uint32_t bufferRowLength;
        imageHeight, // uint32_t bufferImageHeight;
        {
            aspect,                   // VkImageAspectFlags aspect;
            mipLevel,                 // uint32_t mipLevel;
            0u,                       // uint32_t baseArrayLayer;
            getArraySize(imageParms), // uint32_t layerCount;
        },                            // VkImageSubresourceLayers imageSubresource;
        {0, 0, 0},                    // VkOffset3D imageOffset;
        imageExtent                   // VkExtent3D imageExtent;
    };

    beginCommandBuffer(vk, *m_universalCmdBuffer);
    vk.cmdPipelineBarrier(*m_universalCmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                          (VkDependencyFlags)0, (useGeneralLayout ? 1 : 0), &memoryBarrier, 0, nullptr,
                          (useGeneralLayout ? 0 : 1), &imageBarrier);
    VkImageLayout layout = useGeneralLayout ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    vk.cmdCopyImageToBuffer(*m_universalCmdBuffer, image, layout, *buffer, 1u, &copyRegion);
    vk.cmdPipelineBarrier(*m_universalCmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                          VK_PIPELINE_STAGE_HOST_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT, (VkDependencyFlags)0,
                          (useGeneralLayout ? 1 : 0), &postMemoryBarrier, 1, &bufferBarrier, (useGeneralLayout ? 0 : 1),
                          &postImageBarrier);
    endCommandBuffer(vk, *m_universalCmdBuffer);

    submitCommandsAndWaitWithTransferSync(vk, device, m_universalQueue, *m_universalCmdBuffer, semaphore);

    m_context.resetCommandPoolForVKSC(device, *m_universalCmdPool);

    // Read buffer data
    invalidateAlloc(vk, device, *bufferAlloc);
    tcu::copy(dst, tcu::ConstPixelBufferAccess(dst.getFormat(), dst.getSize(), bufferAlloc->getHostPtr()));
}

uint32_t CopiesAndBlittingTestInstance::calculateSize(tcu::ConstPixelBufferAccess src) const
{
    return src.getWidth() * src.getHeight() * src.getDepth() * tcu::getPixelSize(src.getFormat());
}

de::MovePtr<tcu::TextureLevel> CopiesAndBlittingTestInstance::readImage(vk::VkImage image, const ImageParms &parms,
                                                                        const uint32_t mipLevel,
                                                                        const bool useGeneralLayout,
                                                                        Move<VkSemaphore> *semaphore)
{
    const tcu::TextureFormat imageFormat = getSizeCompatibleTcuTextureFormat(parms.format);
    de::MovePtr<tcu::TextureLevel> resultLevel(new tcu::TextureLevel(
        imageFormat, parms.extent.width >> mipLevel, parms.extent.height >> mipLevel, parms.extent.depth));

    if (tcu::isCombinedDepthStencilType(imageFormat.type))
    {
        if (tcu::hasDepthComponent(imageFormat.order))
        {
            tcu::TextureLevel depthTexture(mapCombinedToDepthTransferFormat(imageFormat),
                                           parms.extent.width >> mipLevel, parms.extent.height >> mipLevel,
                                           parms.extent.depth);
            readImageAspect(image, depthTexture.getAccess(), parms, mipLevel, useGeneralLayout, semaphore);
            tcu::copy(tcu::getEffectiveDepthStencilAccess(resultLevel->getAccess(), tcu::Sampler::MODE_DEPTH),
                      depthTexture.getAccess());
        }

        if (tcu::hasStencilComponent(imageFormat.order))
        {
            tcu::TextureLevel stencilTexture(
                tcu::getEffectiveDepthStencilTextureFormat(imageFormat, tcu::Sampler::MODE_STENCIL),
                parms.extent.width >> mipLevel, parms.extent.height >> mipLevel, parms.extent.depth);
            readImageAspect(image, stencilTexture.getAccess(), parms, mipLevel, useGeneralLayout, semaphore);
            tcu::copy(tcu::getEffectiveDepthStencilAccess(resultLevel->getAccess(), tcu::Sampler::MODE_STENCIL),
                      stencilTexture.getAccess());
        }
    }
    else
        readImageAspect(image, resultLevel->getAccess(), parms, mipLevel, useGeneralLayout, semaphore);

    return resultLevel;
}

CopiesAndBlittingTestInstance::ExecutionCtx CopiesAndBlittingTestInstance::activeExecutionCtx()
{
    if (m_params.queueSelection != QueueSelectionOptions::Universal)
    {
        return std::make_tuple(m_otherQueue, *m_otherCmdBuffer, *m_otherCmdPool);
    }
    else
    {
        return std::make_tuple(m_universalQueue, *m_universalCmdBuffer, *m_universalCmdPool);
    }
}

uint32_t CopiesAndBlittingTestInstance::activeQueueFamilyIndex() const
{
    int familyIndex = -1;
    switch (m_params.queueSelection)
    {
    case QueueSelectionOptions::ComputeOnly:
        familyIndex = m_context.getComputeQueueFamilyIndex();
        break;
    case QueueSelectionOptions::TransferOnly:
        familyIndex = m_context.getTransferQueueFamilyIndex();
        break;
    case QueueSelectionOptions::Universal:
        familyIndex = m_context.getUniversalQueueFamilyIndex();
        break;
    }
    TCU_CHECK_INTERNAL(familyIndex >= 0);
    return (uint32_t)familyIndex;
}

CopiesAndBlittingTestInstanceWithSparseSemaphore::CopiesAndBlittingTestInstanceWithSparseSemaphore(Context &context,
                                                                                                   TestParams params)
    : CopiesAndBlittingTestInstance(context, params)
    , m_sparseSemaphore()
{
}

void CopiesAndBlittingTestInstanceWithSparseSemaphore::uploadImage(const tcu::ConstPixelBufferAccess &src, VkImage dst,
                                                                   const ImageParms &parms, const bool useGeneralLayout,
                                                                   const uint32_t mipLevels)
{
    CopiesAndBlittingTestInstance::uploadImage(src, dst, parms, mipLevels, useGeneralLayout, &m_sparseSemaphore);
}

de::MovePtr<tcu::TextureLevel> CopiesAndBlittingTestInstanceWithSparseSemaphore::readImage(vk::VkImage image,
                                                                                           const ImageParms &imageParms,
                                                                                           const uint32_t mipLevel)
{
    return CopiesAndBlittingTestInstance::readImage(image, imageParms, mipLevel, m_params.useGeneralLayout,
                                                    &m_sparseSemaphore);
}

} // namespace api
} // namespace vkt
