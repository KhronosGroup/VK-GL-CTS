#ifndef _VKIMAGEUTIL_HPP
#define _VKIMAGEUTIL_HPP
/*-------------------------------------------------------------------------
 * Vulkan CTS Framework
 * --------------------
 *
 * Copyright (c) 2015 The Khronos Group Inc.
 * Copyright (c) 2015 Imagination Technologies Ltd.
 * Copyright (c) 2015 Google Inc.
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
 * \brief Utilities for images.
 *//*--------------------------------------------------------------------*/

#include "vkDefs.hpp"
#include "vkMemUtil.hpp"
#include "tcuTexture.hpp"
#include "tcuCompressedTexture.hpp"
#include "deSharedPtr.hpp"

namespace vk
{

bool isFloatFormat(VkFormat format);
bool isUfloatFormat(VkFormat format);
bool isSfloatFormat(VkFormat format);
bool isUnormFormat(VkFormat format);
bool isSnormFormat(VkFormat format);
bool isIntFormat(VkFormat format);
bool isUintFormat(VkFormat format);
bool isDepthStencilFormat(VkFormat format);
bool isCompressedFormat(VkFormat format);
bool isSrgbFormat(VkFormat format);

bool isSupportedByFramework(VkFormat format);
void checkImageSupport(const InstanceInterface &vki, VkPhysicalDevice physicalDevice,
                       const VkImageCreateInfo &imageCreateInfo);

tcu::TextureFormat mapVkFormat(VkFormat format);
tcu::CompressedTexFormat mapVkCompressedFormat(VkFormat format);
tcu::TextureFormat getDepthCopyFormat(VkFormat combinedFormat);
tcu::TextureFormat getStencilCopyFormat(VkFormat combinedFormat);

tcu::Sampler mapVkSampler(const VkSamplerCreateInfo &samplerCreateInfo);
tcu::Sampler::CompareMode mapVkSamplerCompareOp(VkCompareOp compareOp);
tcu::Sampler::WrapMode mapVkSamplerAddressMode(VkSamplerAddressMode addressMode);
tcu::Sampler::ReductionMode mapVkSamplerReductionMode(VkSamplerReductionMode reductionMode);
tcu::Sampler::FilterMode mapVkMinTexFilter(VkFilter filter, VkSamplerMipmapMode mipMode);
tcu::Sampler::FilterMode mapVkMagTexFilter(VkFilter filter);

VkFilter mapFilterMode(tcu::Sampler::FilterMode filterMode);
VkSamplerMipmapMode mapMipmapMode(tcu::Sampler::FilterMode filterMode);
VkSamplerAddressMode mapWrapMode(tcu::Sampler::WrapMode wrapMode);
VkCompareOp mapCompareMode(tcu::Sampler::CompareMode mode);
VkFormat mapTextureFormat(const tcu::TextureFormat &format);
VkFormat mapCompressedTextureFormat(const tcu::CompressedTexFormat format);
VkSamplerCreateInfo mapSampler(const tcu::Sampler &sampler, const tcu::TextureFormat &format, float minLod = 0.0f,
                               float maxLod = 1000.0f, bool unnormal = false);
rr::GenericVec4 mapVkColor(const VkClearColorValue &color);
VkClearColorValue mapVkColor(const rr::GenericVec4 &color);

void imageUtilSelfTest(void);

float getRepresentableDiffUnorm(const VkFormat format, const uint32_t componentNdx);
float getRepresentableDiffSnorm(const VkFormat format, const uint32_t componentNdx);
uint32_t getFormatComponentWidth(const VkFormat format, const uint32_t componentNdx);
uint32_t getBlockSizeInBytes(const VkFormat compressedFormat);
uint32_t getBlockWidth(const VkFormat compressedFormat);
uint32_t getBlockHeight(const VkFormat compressedFormat);

const uint32_t BUFFER_IMAGE_COPY_OFFSET_GRANULARITY = 4u;

// \todo [2017-05-18 pyry] Consider moving this to tcu
struct PlanarFormatDescription
{
    enum
    {
        MAX_CHANNELS = 4,
        MAX_PLANES   = 3
    };

    enum ChannelFlags
    {
        CHANNEL_R = (1u << 0), // Has "R" (0) channel
        CHANNEL_G = (1u << 1), // Has "G" (1) channel
        CHANNEL_B = (1u << 2), // Has "B" (2) channel
        CHANNEL_A = (1u << 3), // Has "A" (3) channel
    };

    struct Plane
    {
        uint8_t elementSizeBytes;
        uint8_t widthDivisor;
        uint8_t heightDivisor;
        VkFormat planeCompatibleFormat;
    };

    struct Channel
    {
        uint8_t planeNdx;
        uint8_t type;        // tcu::TextureChannelClass value
        uint8_t offsetBits;  // Offset in element in bits
        uint8_t sizeBits;    // Value size in bits
        uint8_t strideBytes; // Pixel stride (in bytes), usually plane elementSize
    };

    uint8_t numPlanes;
    uint8_t presentChannels;
    uint8_t blockWidth;
    uint8_t blockHeight;
    Plane planes[MAX_PLANES];
    Channel channels[MAX_CHANNELS];

    inline bool hasChannelNdx(uint32_t ndx) const
    {
        DE_ASSERT(de::inBounds(ndx, 0u, 4u));
        return (presentChannels & (1u << ndx)) != 0;
    }
};

bool isYCbCrFormat(VkFormat format);
bool isYCbCrExtensionFormat(VkFormat format);
PlanarFormatDescription getPlanarFormatDescription(VkFormat format);
int getPlaneCount(VkFormat format);
uint32_t getMipmapCount(VkFormat format, const vk::PlanarFormatDescription &formatDescription,
                        const vk::VkImageFormatProperties &imageFormatProperties, const vk::VkExtent3D &extent);

uint32_t getPlaneSizeInBytes(const PlanarFormatDescription &formatInfo, const VkExtent3D &baseExtents,
                             const uint32_t planeNdx, const uint32_t mipmapLevel, const uint32_t mipmapMemoryAlignment);
uint32_t getPlaneSizeInBytes(const PlanarFormatDescription &formatInfo, const tcu::UVec2 &baseExtents,
                             const uint32_t planeNdx, const uint32_t mipmapLevel, const uint32_t mipmapMemoryAlignment);
VkExtent3D getPlaneExtent(const PlanarFormatDescription &formatInfo, const VkExtent3D &baseExtents,
                          const uint32_t planeNdx, const uint32_t mipmapLevel);
tcu::UVec2 getPlaneExtent(const PlanarFormatDescription &formatInfo, const tcu::UVec2 &baseExtents,
                          const uint32_t planeNdx, const uint32_t mipmapLevel);
tcu::UVec3 getImageSizeAlignment(VkFormat format);
tcu::UVec3 getImageSizeAlignment(const PlanarFormatDescription &formatInfo);
tcu::UVec2 getBlockExtent(VkFormat format);
tcu::UVec2 getBlockExtent(const PlanarFormatDescription &formatInfo);
VkFormat getPlaneCompatibleFormat(VkFormat format, uint32_t planeNdx);
VkFormat getPlaneCompatibleFormat(const PlanarFormatDescription &formatInfo, uint32_t planeNdx);

VkImageAspectFlagBits getPlaneAspect(uint32_t planeNdx);
uint32_t getAspectPlaneNdx(VkImageAspectFlagBits planeAspect);
bool isChromaSubsampled(VkFormat format);
bool isYCbCr422Format(VkFormat format);
bool isYCbCr420Format(VkFormat format);

tcu::PixelBufferAccess getChannelAccess(const PlanarFormatDescription &formatInfo, const tcu::UVec2 &size,
                                        const uint32_t *planeRowPitches, void *const *planePtrs, uint32_t channelNdx);
tcu::ConstPixelBufferAccess getChannelAccess(const PlanarFormatDescription &formatInfo, const tcu::UVec2 &size,
                                             const uint32_t *planeRowPitches, const void *const *planePtrs,
                                             uint32_t channelNdx);
tcu::PixelBufferAccess getChannelAccess(const PlanarFormatDescription &formatInfo, const tcu::UVec3 &size,
                                        const uint32_t *planeRowPitches, void *const *planePtrs, uint32_t channelNdx);
tcu::ConstPixelBufferAccess getChannelAccess(const PlanarFormatDescription &formatInfo, const tcu::UVec3 &size,
                                             const uint32_t *planeRowPitches, const void *const *planePtrs,
                                             uint32_t channelNdx);
VkImageAspectFlags getImageAspectFlags(const tcu::TextureFormat textureFormat);
VkExtent3D mipLevelExtents(const VkExtent3D &baseExtents, const uint32_t mipLevel);
tcu::UVec3 alignedDivide(const VkExtent3D &extent, const VkExtent3D &divisor);

/*--------------------------------------------------------------------*//*!
 * Copies buffer data into an image. The buffer is expected to be
 * in a state after host write.
*//*--------------------------------------------------------------------*/
void copyBufferToImage(const DeviceInterface &vk, vk::VkDevice device, vk::VkQueue queue, uint32_t queueFamilyIndex,
                       const vk::VkBuffer &buffer, vk::VkDeviceSize bufferSize,
                       const std::vector<vk::VkBufferImageCopy> &copyRegions, const vk::VkSemaphore *waitSemaphore,
                       vk::VkImageAspectFlags imageAspectFlags, uint32_t mipLevels, uint32_t arrayLayers,
                       vk::VkImage destImage, VkImageLayout destImageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                       VkPipelineStageFlags destImageDstStageFlags = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

void copyBufferToImage(const DeviceInterface &vk, const VkCommandBuffer &cmdBuffer, const VkBuffer &buffer,
                       vk::VkDeviceSize bufferSize, const std::vector<VkBufferImageCopy> &copyRegions,
                       VkImageAspectFlags imageAspectFlags, uint32_t mipLevels, uint32_t arrayLayers, VkImage destImage,
                       VkImageLayout destImageLayout               = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                       VkPipelineStageFlags destImageDstStageFlags = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

/*--------------------------------------------------------------------*//*!
 * Copies image data into a buffer. The buffer is expected to be
 * read by the host.
*//*--------------------------------------------------------------------*/
void copyImageToBuffer(const DeviceInterface &vk, vk::VkCommandBuffer cmdBuffer, vk::VkImage image, vk::VkBuffer buffer,
                       tcu::IVec2 size, vk::VkAccessFlags srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                       vk::VkImageLayout oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, uint32_t numLayers = 1u,
                       VkImageAspectFlags barrierAspect = VK_IMAGE_ASPECT_COLOR_BIT,
                       VkImageAspectFlags copyAspect    = VK_IMAGE_ASPECT_COLOR_BIT);

void copyImageToBuffer(const DeviceInterface &vk, vk::VkCommandBuffer cmdBuffer, vk::VkImage image, vk::VkBuffer buffer,
                       vk::VkFormat format, tcu::IVec2 size, uint32_t mipLevel = 0u,
                       vk::VkAccessFlags srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                       vk::VkImageLayout oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, uint32_t numLayers = 1u,
                       VkImageAspectFlags barrierAspect = VK_IMAGE_ASPECT_COLOR_BIT,
                       VkImageAspectFlags copyAspect    = VK_IMAGE_ASPECT_COLOR_BIT);

/*--------------------------------------------------------------------*//*!
 * Clear a color image
*//*--------------------------------------------------------------------*/
void clearColorImage(const DeviceInterface &vk, const vk::VkDevice device, const vk::VkQueue queue,
                     uint32_t queueFamilyIndex, vk::VkImage image, tcu::Vec4 clearColor, vk::VkImageLayout oldLayout,
                     vk::VkImageLayout newLayout, vk::VkPipelineStageFlags dstStageFlags);

/*--------------------------------------------------------------------*//*!
 * Initialize color image with a chessboard pattern
*//*--------------------------------------------------------------------*/
void initColorImageChessboardPattern(const DeviceInterface &vk, const vk::VkDevice device, const vk::VkQueue queue,
                                     uint32_t queueFamilyIndex, Allocator &allocator, vk::VkImage image,
                                     vk::VkFormat format, tcu::Vec4 colorValue0, tcu::Vec4 colorValue1,
                                     uint32_t imageWidth, uint32_t imageHeight, uint32_t tileSize,
                                     vk::VkImageLayout oldLayout, vk::VkImageLayout newLayout,
                                     vk::VkPipelineStageFlags dstStageFlags);

/*--------------------------------------------------------------------*//*!
 * Copies depth/stencil image data into two separate buffers.
 * The buffers are expected to be read by the host.
*//*--------------------------------------------------------------------*/
void copyDepthStencilImageToBuffers(const DeviceInterface &vk, vk::VkCommandBuffer cmdBuffer, vk::VkImage image,
                                    vk::VkBuffer depthBuffer, vk::VkBuffer stencilBuffer, tcu::IVec2 size,
                                    vk::VkAccessFlags srcAccessMask, vk::VkImageLayout oldLayout,
                                    uint32_t numLayers = 1u);

/*--------------------------------------------------------------------*//*!
 * Clear a depth/stencil image
*//*--------------------------------------------------------------------*/
void clearDepthStencilImage(const DeviceInterface &vk, const vk::VkDevice device, const vk::VkQueue queue,
                            uint32_t queueFamilyIndex, vk::VkImage image, float depthValue, uint32_t stencilValue,
                            vk::VkImageLayout oldLayout, vk::VkImageLayout newLayout,
                            vk::VkPipelineStageFlags dstStageFlags);

/*--------------------------------------------------------------------*//*!
 * Initialize depth and stencil channels with a chessboard pattern
*//*--------------------------------------------------------------------*/
void initDepthStencilImageChessboardPattern(const DeviceInterface &vk, const vk::VkDevice device,
                                            const vk::VkQueue queue, uint32_t queueFamilyIndex, Allocator &allocator,
                                            vk::VkImage image, vk::VkFormat format, float depthValue0,
                                            float depthValue1, uint32_t stencilValue0, uint32_t stencilValue1,
                                            uint32_t imageWidth, uint32_t imageHeight, uint32_t tileSize,
                                            vk::VkImageLayout oldLayout, vk::VkImageLayout newLayout,
                                            vk::VkPipelineStageFlags dstStageFlags);

/*--------------------------------------------------------------------*//*!
 * Checks if the physical device supports creation of the specified
 * image format.
 *//*--------------------------------------------------------------------*/
bool checkSparseImageFormatSupport(const VkPhysicalDevice physicalDevice, const InstanceInterface &instance,
                                   const VkFormat format, const VkImageType imageType,
                                   const VkSampleCountFlagBits sampleCount, const VkImageUsageFlags usageFlags,
                                   const VkImageTiling imageTiling);

bool checkSparseImageFormatSupport(const vk::VkPhysicalDevice physicalDevice, const vk::InstanceInterface &instance,
                                   const vk::VkImageCreateInfo &imageCreateInfo);

/*--------------------------------------------------------------------*//*!
 * Allocates memory for a sparse image and handles the memory binding.
 *//*--------------------------------------------------------------------*/
void allocateAndBindSparseImage(const vk::DeviceInterface &vk, vk::VkDevice device,
                                const vk::VkPhysicalDevice physicalDevice, const vk::InstanceInterface &instance,
                                const vk::VkImageCreateInfo &imageCreateInfo, const vk::VkSemaphore &signalSemaphore,
                                vk::VkQueue queue, vk::Allocator &allocator,
                                std::vector<de::SharedPtr<vk::Allocation>> &allocations, tcu::TextureFormat format,
                                vk::VkImage destImage);

} // namespace vk

#endif // _VKIMAGEUTIL_HPP
