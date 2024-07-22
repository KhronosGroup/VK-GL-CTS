/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2015-2020 The Khronos Group Inc.
 * Copyright (c) 2020 Google Inc.
 * Copyright (c) 2015-2016 Samsung Electronics Co., Ltd.
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
 * \brief Vulkan Copies And Blitting Tests
 *//*--------------------------------------------------------------------*/

#include "vktApiCopiesAndBlittingTests.hpp"

#include "deStringUtil.hpp"
#include "deUniquePtr.hpp"

#include "tcuImageCompare.hpp"
#include "tcuAstcUtil.hpp"
#include "tcuTexture.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuVectorType.hpp"
#include "tcuVectorUtil.hpp"
#include "tcuTestLog.hpp"
#include "tcuTexLookupVerifier.hpp"
#include "tcuCommandLine.hpp"
#include "tcuResultCollector.hpp"
#include "tcuSeedBuilder.hpp"

#include "ycbcr/vktYCbCrUtil.hpp"

#include "vkImageUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkPrograms.hpp"
#include "vkQueryUtil.hpp"
#include "vkDeviceUtil.hpp"
#include "vkRefUtil.hpp"
#include "vktTestCase.hpp"
#include "vktTestCaseUtil.hpp"
#include "vktTestGroupUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkImageWithMemory.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkBarrierUtil.hpp"

#include "pipeline/vktPipelineImageUtil.hpp" // required for compressed image blit
#include "vktCustomInstancesDevices.hpp"
#include "vkSafetyCriticalUtil.hpp"

#include <set>
#include <array>
#include <algorithm>
#include <iterator>
#include <limits>
#include <sstream>

#ifdef CTS_USES_VULKANSC
// VulkanSC has VK_KHR_copy_commands2 entry points, but not core entry points.
#define cmdCopyImage2 cmdCopyImage2KHR
#define cmdCopyBuffer2 cmdCopyBuffer2KHR
#define cmdCopyImageToBuffer2 cmdCopyImageToBuffer2KHR
#define cmdCopyBufferToImage2 cmdCopyBufferToImage2KHR
#define cmdBlitImage2 cmdBlitImage2KHR
#define cmdResolveImage2 cmdResolveImage2KHR
#endif // CTS_USES_VULKANSC

namespace vkt
{

namespace api
{

namespace
{

enum FillMode
{
    FILL_MODE_GRADIENT = 0,
    FILL_MODE_PYRAMID,
    FILL_MODE_WHITE,
    FILL_MODE_BLACK,
    FILL_MODE_RED,
    FILL_MODE_MULTISAMPLE,
    FILL_MODE_BLUE_RED_X,
    FILL_MODE_BLUE_RED_Y,
    FILL_MODE_BLUE_RED_Z,

    FILL_MODE_LAST
};

enum MirrorModeBits
{
    MIRROR_MODE_X    = (1 << 0),
    MIRROR_MODE_Y    = (1 << 1),
    MIRROR_MODE_Z    = (1 << 2),
    MIRROR_MODE_LAST = (1 << 3),
};

using MirrorMode = uint32_t;

enum AllocationKind
{
    ALLOCATION_KIND_SUBALLOCATED,
    ALLOCATION_KIND_DEDICATED,
};

// In the case of testing new extension, add a flag to this enum and
// handle it in the checkExtensionSupport() function
enum ExtensionUseBits
{
    NONE                          = 0,
    COPY_COMMANDS_2               = (1 << 0),
    SEPARATE_DEPTH_STENCIL_LAYOUT = (1 << 1),
    MAINTENANCE_1                 = (1 << 2),
    MAINTENANCE_5                 = (1 << 3),
    SPARSE_BINDING                = (1 << 4),
};

template <typename Type>
class BinaryCompare
{
public:
    bool operator()(const Type &a, const Type &b) const
    {
        return deMemCmp(&a, &b, sizeof(Type)) < 0;
    }
};

typedef std::set<vk::VkFormat, BinaryCompare<vk::VkFormat>> FormatSet;

FormatSet dedicatedAllocationImageToImageFormatsToTestSet;
FormatSet dedicatedAllocationBlittingFormatsToTestSet;

using namespace vk;

const int32_t defaultSize                      = 64;
const int32_t defaultHalfSize                  = defaultSize / 2;
const int32_t defaultQuarterSize               = defaultSize / 4;
const int32_t defaultSixteenthSize             = defaultSize / 16;
const int32_t defaultQuarterSquaredSize        = defaultQuarterSize * defaultQuarterSize;
const uint32_t defaultRootSize                 = static_cast<uint32_t>(deSqrt(defaultSize));
const VkExtent3D defaultExtent                 = {defaultSize, defaultSize, 1};
const VkExtent3D defaultHalfExtent             = {defaultHalfSize, defaultHalfSize, 1};
const VkExtent3D defaultQuarterExtent          = {defaultQuarterSize, defaultQuarterSize, 1};
const VkExtent3D defaultRootExtent             = {defaultRootSize, defaultRootSize, 1};
const VkExtent3D default1dExtent               = {defaultSize, 1, 1};
const VkExtent3D default1dQuarterSquaredExtent = {defaultQuarterSquaredSize, 1, 1};
const VkExtent3D default3dExtent               = {defaultQuarterSize, defaultQuarterSize, defaultQuarterSize};
const VkExtent3D default3dSmallExtent          = {defaultSixteenthSize, defaultSixteenthSize, defaultSixteenthSize};

const VkImageSubresourceLayers defaultSourceLayer = {
    VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
    0u,                        // uint32_t mipLevel;
    0u,                        // uint32_t baseArrayLayer;
    1u,                        // uint32_t layerCount;
};

VkImageCopy2KHR convertvkImageCopyTovkImageCopy2KHR(VkImageCopy imageCopy)
{
    const VkImageCopy2KHR imageCopy2 = {
        VK_STRUCTURE_TYPE_IMAGE_COPY_2_KHR, // VkStructureType sType;
        DE_NULL,                            // const void* pNext;
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
        DE_NULL,                             // const void* pNext;
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
        DE_NULL,                                   // const void* pNext;
        bufferImageCopy.bufferOffset,              // VkDeviceSize bufferOffset;
        bufferImageCopy.bufferRowLength,           // uint32_t bufferRowLength;
        bufferImageCopy.bufferImageHeight,         // uint32_t bufferImageHeight;
        bufferImageCopy.imageSubresource,          // VkImageSubresourceLayers imageSubresource;
        bufferImageCopy.imageOffset,               // VkOffset3D imageOffset;
        bufferImageCopy.imageExtent                // VkExtent3D imageExtent;
    };
    return bufferImageCopy2;
}

VkImageBlit2KHR convertvkImageBlitTovkImageBlit2KHR(VkImageBlit imageBlit)
{
    const VkImageBlit2KHR imageBlit2 = {VK_STRUCTURE_TYPE_IMAGE_BLIT_2_KHR, // VkStructureType sType;
                                        DE_NULL,                            // const void* pNext;
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
        DE_NULL,                               // const void* pNext;
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

union CopyRegion
{
    VkBufferCopy bufferCopy;
    VkImageCopy imageCopy;
    VkBufferImageCopy bufferImageCopy;
    VkImageBlit imageBlit;
    VkImageResolve imageResolve;
};

struct ImageParms
{
    VkImageType imageType;
    VkFormat format;
    VkExtent3D extent;
    VkImageTiling tiling;
    VkImageLayout operationLayout;
    VkImageCreateFlags createFlags;
    FillMode fillMode;

    std::pair<uint32_t, uint32_t> texelBlockDimensions() const
    {
        const bool isCompressed    = isCompressedFormat(format);
        const uint32_t blockWidth  = (isCompressed) ? getBlockWidth(format) : 1u;
        const uint32_t blockHeight = (isCompressed) ? getBlockHeight(format) : 1u;
        return std::make_pair(blockWidth, blockHeight);
    }
};

enum class QueueSelectionOptions
{
    Universal = 0,
    ComputeOnly,
    TransferOnly
};

struct CustomDeviceData
{
    Move<VkDevice> device;
    de::MovePtr<Allocator> allocator;
    uint32_t queueFamilyIndex{0};
};

struct BufferParams
{
    VkDeviceSize size;
    FillMode fillMode;
};

struct TestParams
{
    union Data
    {
        BufferParams buffer;
        ImageParms image;
    } src, dst;

    std::vector<CopyRegion> regions;

    union
    {
        VkFilter filter;
        VkSampleCountFlagBits samples;
    };

    AllocationKind allocationKind;
    uint32_t extensionFlags;
    QueueSelectionOptions queueSelection;
    uint32_t mipLevels;
    uint32_t arrayLayers;
    bool singleCommand;
    uint32_t barrierCount;
    bool
        clearDestinationWithRed; // Used for CopyImageToImage tests to clear dst image with vec4(1.0f, 0.0f, 0.0f, 1.0f)
    bool imageOffset;
    bool useSecondaryCmdBuffer;
    bool useSparseBinding;

    TestParams(void)
    {
        allocationKind          = ALLOCATION_KIND_DEDICATED;
        extensionFlags          = NONE;
        queueSelection          = QueueSelectionOptions::Universal;
        mipLevels               = 1u;
        arrayLayers             = 1u;
        singleCommand           = true;
        barrierCount            = 1u;
        src.image.createFlags   = VK_IMAGE_CREATE_FLAG_BITS_MAX_ENUM;
        dst.image.createFlags   = VK_IMAGE_CREATE_FLAG_BITS_MAX_ENUM;
        src.buffer.fillMode     = FILL_MODE_GRADIENT;
        src.image.fillMode      = FILL_MODE_GRADIENT;
        dst.buffer.fillMode     = FILL_MODE_GRADIENT;
        dst.image.fillMode      = FILL_MODE_WHITE;
        clearDestinationWithRed = false;
        samples                 = VK_SAMPLE_COUNT_1_BIT;
        imageOffset             = false;
        useSecondaryCmdBuffer   = false;
        useSparseBinding        = false;
    }

    bool usesNonUniversalQueue() const
    {
        return queueSelection != QueueSelectionOptions::Universal;
    }
};

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
            DE_NULL,                                          // const void*            pNext
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

    if (flags & SPARSE_BINDING)
        context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_SPARSE_BINDING);
}

inline uint32_t getArraySize(const ImageParms &parms)
{
    return (parms.imageType != VK_IMAGE_TYPE_3D) ? parms.extent.depth : 1u;
}

inline VkImageCreateFlags getCreateFlags(const ImageParms &parms)
{
    if (parms.createFlags == VK_IMAGE_CREATE_FLAG_BITS_MAX_ENUM)
        return parms.imageType == VK_IMAGE_TYPE_2D && parms.extent.depth % 6 == 0 ?
                   VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT :
                   0;
    else
        return parms.createFlags;
}

inline VkExtent3D getExtent3D(const ImageParms &parms, uint32_t mipLevel = 0u)
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

class CopiesAndBlittingTestInstance : public vkt::TestInstance
{
public:
    CopiesAndBlittingTestInstance(Context &context, TestParams testParams);
    virtual tcu::TestStatus iterate(void) = 0;

protected:
    const TestParams m_params;
    VkDevice m_device;
    Allocator *m_allocator;
    VkQueue m_universalQueue{VK_NULL_HANDLE};
    Move<VkCommandPool> m_universalCmdPool;
    Move<VkCommandBuffer> m_universalCmdBuffer;
    VkQueue m_otherQueue{VK_NULL_HANDLE}; // Dedicated compute/transfer queue
    Move<VkCommandPool> m_otherCmdPool;   // Dedicated compute/transfer command bool & buffer
    Move<VkCommandBuffer> m_otherCmdBuffer;
    Move<VkCommandPool> m_secondaryCmdPool;
    Move<VkCommandBuffer> m_secondaryCmdBuffer;
    Move<VkCommandPool> m_sparseCmdPool;
    Move<VkCommandBuffer> m_sparseCmdBuffer;

    de::MovePtr<tcu::TextureLevel> m_sourceTextureLevel;
    de::MovePtr<tcu::TextureLevel> m_destinationTextureLevel;
    de::MovePtr<tcu::TextureLevel> m_expectedTextureLevel[16];

    // For tests that use multiple queues, this will be a >1 sized array containing the queue familiy indices,
    // used for setting up concurrently accessed resources.
    std::vector<uint32_t> m_queueFamilyIndices;

    void generateBuffer(tcu::PixelBufferAccess buffer, int width, int height, int depth = 1,
                        FillMode = FILL_MODE_GRADIENT);
    virtual void generateExpectedResult(void);
    void uploadBuffer(const tcu::ConstPixelBufferAccess &bufferAccess, const Allocation &bufferAlloc);
    void uploadImage(const tcu::ConstPixelBufferAccess &src, VkImage dst, const ImageParms &parms,
                     const uint32_t mipLevels = 1u);
    virtual tcu::TestStatus checkTestResult(tcu::ConstPixelBufferAccess result);
    virtual void copyRegionToTextureLevel(tcu::ConstPixelBufferAccess src, tcu::PixelBufferAccess dst,
                                          CopyRegion region, uint32_t mipLevel = 0u) = 0;
    uint32_t calculateSize(tcu::ConstPixelBufferAccess src) const
    {
        return src.getWidth() * src.getHeight() * src.getDepth() * tcu::getPixelSize(src.getFormat());
    }

    de::MovePtr<tcu::TextureLevel> readImage(vk::VkImage image, const ImageParms &imageParms,
                                             const uint32_t mipLevel = 0u);

    using ExecutionCtx = std::tuple<VkQueue, VkCommandBuffer, VkCommandPool>;
    ExecutionCtx activeExecutionCtx()
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

    uint32_t activeQueueFamilyIndex() const
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

private:
    void uploadImageAspect(const tcu::ConstPixelBufferAccess &src, const VkImage &dst, const ImageParms &parms,
                           const uint32_t mipLevels = 1u);
    void readImageAspect(vk::VkImage src, const tcu::PixelBufferAccess &dst, const ImageParms &parms,
                         const uint32_t mipLevel = 0u);
};

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

void CopiesAndBlittingTestInstance::uploadImageAspect(const tcu::ConstPixelBufferAccess &imageAccess,
                                                      const VkImage &image, const ImageParms &parms,
                                                      const uint32_t mipLevels)
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
            DE_NULL,                              // const void* pNext;
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

    const VkImageMemoryBarrier preImageBarrier = makeImageMemoryBarrier(
        0u, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, image,
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
                          (VkDependencyFlags)0, 0, (const VkMemoryBarrier *)DE_NULL, 1, &preBufferBarrier,
                          (skipPreImageBarrier ? 0 : 1), (skipPreImageBarrier ? DE_NULL : &preImageBarrier));
    vk.cmdCopyBufferToImage(*m_universalCmdBuffer, *buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                            (uint32_t)copyRegions.size(), &copyRegions[0]);
    vk.cmdPipelineBarrier(*m_universalCmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                          (VkDependencyFlags)0, 0, (const VkMemoryBarrier *)DE_NULL, 0,
                          (const VkBufferMemoryBarrier *)DE_NULL, 1, &postImageBarrier);
    endCommandBuffer(vk, *m_universalCmdBuffer);

    submitCommandsAndWait(vk, vkDevice, m_universalQueue, *m_universalCmdBuffer);
    m_context.resetCommandPoolForVKSC(vkDevice, *m_universalCmdPool);
}

void CopiesAndBlittingTestInstance::uploadImage(const tcu::ConstPixelBufferAccess &src, VkImage dst,
                                                const ImageParms &parms, const uint32_t mipLevels)
{
    if (tcu::isCombinedDepthStencilType(src.getFormat().type))
    {
        if (tcu::hasDepthComponent(src.getFormat().order))
        {
            tcu::TextureLevel depthTexture(mapCombinedToDepthTransferFormat(src.getFormat()), src.getWidth(),
                                           src.getHeight(), src.getDepth());
            tcu::copy(depthTexture.getAccess(), tcu::getEffectiveDepthStencilAccess(src, tcu::Sampler::MODE_DEPTH));
            uploadImageAspect(depthTexture.getAccess(), dst, parms, mipLevels);
        }

        if (tcu::hasStencilComponent(src.getFormat().order))
        {
            tcu::TextureLevel stencilTexture(
                tcu::getEffectiveDepthStencilTextureFormat(src.getFormat(), tcu::Sampler::MODE_STENCIL), src.getWidth(),
                src.getHeight(), src.getDepth());
            tcu::copy(stencilTexture.getAccess(), tcu::getEffectiveDepthStencilAccess(src, tcu::Sampler::MODE_STENCIL));
            uploadImageAspect(stencilTexture.getAccess(), dst, parms, mipLevels);
        }
    }
    else
        uploadImageAspect(src, dst, parms, mipLevels);
}

tcu::TestStatus CopiesAndBlittingTestInstance::checkTestResult(tcu::ConstPixelBufferAccess result)
{
    const tcu::ConstPixelBufferAccess expected = m_expectedTextureLevel[0]->getAccess();

    if (isFloatFormat(result.getFormat()))
    {
        const tcu::Vec4 threshold(0.0f);
        if (!tcu::floatThresholdCompare(m_context.getTestContext().getLog(), "Compare", "Result comparison", expected,
                                        result, threshold, tcu::COMPARE_LOG_RESULT))
            return tcu::TestStatus::fail("CopiesAndBlitting test");
    }
    else
    {
        const tcu::UVec4 threshold(0u);
        if (tcu::hasDepthComponent(result.getFormat().order) || tcu::hasStencilComponent(result.getFormat().order))
        {
            if (!tcu::dsThresholdCompare(m_context.getTestContext().getLog(), "Compare", "Result comparison", expected,
                                         result, 0.1f, tcu::COMPARE_LOG_RESULT))
                return tcu::TestStatus::fail("CopiesAndBlitting test");
        }
        else
        {
            if (!tcu::intThresholdCompare(m_context.getTestContext().getLog(), "Compare", "Result comparison", expected,
                                          result, threshold, tcu::COMPARE_LOG_RESULT))
                return tcu::TestStatus::fail("CopiesAndBlitting test");
        }
    }

    return tcu::TestStatus::pass("CopiesAndBlitting test");
}

void CopiesAndBlittingTestInstance::generateExpectedResult(void)
{
    const tcu::ConstPixelBufferAccess src = m_sourceTextureLevel->getAccess();
    const tcu::ConstPixelBufferAccess dst = m_destinationTextureLevel->getAccess();

    m_expectedTextureLevel[0] = de::MovePtr<tcu::TextureLevel>(
        new tcu::TextureLevel(dst.getFormat(), dst.getWidth(), dst.getHeight(), dst.getDepth()));
    tcu::copy(m_expectedTextureLevel[0]->getAccess(), dst);

    for (uint32_t i = 0; i < m_params.regions.size(); i++)
        copyRegionToTextureLevel(src, m_expectedTextureLevel[0]->getAccess(), m_params.regions[i]);
}

void CopiesAndBlittingTestInstance::readImageAspect(vk::VkImage image, const tcu::PixelBufferAccess &dst,
                                                    const ImageParms &imageParms, const uint32_t mipLevel)
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
            DE_NULL,                              // const void* pNext;
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
    const VkImageAspectFlags formatAspect   = getAspectFlags(imageParms.format);
    const VkImageMemoryBarrier imageBarrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, // VkStructureType sType;
                                               DE_NULL,                                // const void* pNext;
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
        DE_NULL,                                 // const void* pNext;
        VK_ACCESS_TRANSFER_WRITE_BIT,            // VkAccessFlags srcAccessMask;
        VK_ACCESS_HOST_READ_BIT,                 // VkAccessFlags dstAccessMask;
        VK_QUEUE_FAMILY_IGNORED,                 // uint32_t srcQueueFamilyIndex;
        VK_QUEUE_FAMILY_IGNORED,                 // uint32_t dstQueueFamilyIndex;
        *buffer,                                 // VkBuffer buffer;
        0u,                                      // VkDeviceSize offset;
        pixelDataSize                            // VkDeviceSize size;
    };

    const VkImageMemoryBarrier postImageBarrier = {
        VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, // VkStructureType sType;
        DE_NULL,                                // const void* pNext;
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
                          (VkDependencyFlags)0, 0, (const VkMemoryBarrier *)DE_NULL, 0,
                          (const VkBufferMemoryBarrier *)DE_NULL, 1, &imageBarrier);
    vk.cmdCopyImageToBuffer(*m_universalCmdBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, *buffer, 1u,
                            &copyRegion);
    vk.cmdPipelineBarrier(*m_universalCmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                          VK_PIPELINE_STAGE_HOST_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT, (VkDependencyFlags)0, 0,
                          (const VkMemoryBarrier *)DE_NULL, 1, &bufferBarrier, 1, &postImageBarrier);
    endCommandBuffer(vk, *m_universalCmdBuffer);

    submitCommandsAndWait(vk, device, m_universalQueue, *m_universalCmdBuffer);
    m_context.resetCommandPoolForVKSC(device, *m_universalCmdPool);

    // Read buffer data
    invalidateAlloc(vk, device, *bufferAlloc);
    tcu::copy(dst, tcu::ConstPixelBufferAccess(dst.getFormat(), dst.getSize(), bufferAlloc->getHostPtr()));
}

de::MovePtr<tcu::TextureLevel> CopiesAndBlittingTestInstance::readImage(vk::VkImage image, const ImageParms &parms,
                                                                        const uint32_t mipLevel)
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
            readImageAspect(image, depthTexture.getAccess(), parms, mipLevel);
            tcu::copy(tcu::getEffectiveDepthStencilAccess(resultLevel->getAccess(), tcu::Sampler::MODE_DEPTH),
                      depthTexture.getAccess());
        }

        if (tcu::hasStencilComponent(imageFormat.order))
        {
            tcu::TextureLevel stencilTexture(
                tcu::getEffectiveDepthStencilTextureFormat(imageFormat, tcu::Sampler::MODE_STENCIL),
                parms.extent.width >> mipLevel, parms.extent.height >> mipLevel, parms.extent.depth);
            readImageAspect(image, stencilTexture.getAccess(), parms, mipLevel);
            tcu::copy(tcu::getEffectiveDepthStencilAccess(resultLevel->getAccess(), tcu::Sampler::MODE_STENCIL),
                      stencilTexture.getAccess());
        }
    }
    else
        readImageAspect(image, resultLevel->getAccess(), parms, mipLevel);

    return resultLevel;
}

// Copy from image to image.

class CopyImageToImage final : public CopiesAndBlittingTestInstance
{
public:
    CopyImageToImage(Context &context, TestParams params);
    tcu::TestStatus iterate(void) override;

private:
    tcu::TestStatus checkTestResult(tcu::ConstPixelBufferAccess result = tcu::ConstPixelBufferAccess()) override;
    void copyRegionToTextureLevel(tcu::ConstPixelBufferAccess src, tcu::PixelBufferAccess dst, CopyRegion region,
                                  uint32_t mipLevel = 0u) override;

    Move<VkImage> m_source;
    de::MovePtr<Allocation> m_sourceImageAlloc;
    Move<VkImage> m_destination;
    de::MovePtr<Allocation> m_destinationImageAlloc;
    std::vector<de::SharedPtr<Allocation>> m_sparseAllocations;
    Move<VkSemaphore> m_sparseSemaphore;
};

CopyImageToImage::CopyImageToImage(Context &context, TestParams params) : CopiesAndBlittingTestInstance(context, params)
{
    const InstanceInterface &vki        = context.getInstanceInterface();
    const DeviceInterface &vk           = context.getDeviceInterface();
    const VkPhysicalDevice vkPhysDevice = context.getPhysicalDevice();

    // Create source image
    {
        VkImageCreateInfo sourceImageParams = {
            VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,                               // VkStructureType sType;
            DE_NULL,                                                           // const void* pNext;
            getCreateFlags(m_params.src.image),                                // VkImageCreateFlags flags;
            m_params.src.image.imageType,                                      // VkImageType imageType;
            m_params.src.image.format,                                         // VkFormat format;
            getExtent3D(m_params.src.image),                                   // VkExtent3D extent;
            1u,                                                                // uint32_t mipLevels;
            getArraySize(m_params.src.image),                                  // uint32_t arraySize;
            VK_SAMPLE_COUNT_1_BIT,                                             // uint32_t samples;
            VK_IMAGE_TILING_OPTIMAL,                                           // VkImageTiling tiling;
            VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, // VkImageUsageFlags usage;
            m_queueFamilyIndices.size() > 1 ? VK_SHARING_MODE_CONCURRENT :
                                              VK_SHARING_MODE_EXCLUSIVE, // VkSharingMode sharingMode;
            (uint32_t)m_queueFamilyIndices.size(),                       // uint32_t queueFamilyIndexCount;
            m_queueFamilyIndices.data(),                                 // const uint32_t* pQueueFamilyIndices;
            VK_IMAGE_LAYOUT_UNDEFINED,                                   // VkImageLayout initialLayout;
        };

#ifndef CTS_USES_VULKANSC
        if (!params.useSparseBinding)
        {
#endif
            m_source           = createImage(vk, m_device, &sourceImageParams);
            m_sourceImageAlloc = allocateImage(vki, vk, vkPhysDevice, m_device, *m_source, MemoryRequirement::Any,
                                               *m_allocator, m_params.allocationKind, 0u);
            VK_CHECK(vk.bindImageMemory(m_device, *m_source, m_sourceImageAlloc->getMemory(),
                                        m_sourceImageAlloc->getOffset()));
#ifndef CTS_USES_VULKANSC
        }
        else
        {
            sourceImageParams.flags |=
                (vk::VK_IMAGE_CREATE_SPARSE_BINDING_BIT | vk::VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT);
            vk::VkImageFormatProperties imageFormatProperties;
            if (vki.getPhysicalDeviceImageFormatProperties(vkPhysDevice, sourceImageParams.format,
                                                           sourceImageParams.imageType, sourceImageParams.tiling,
                                                           sourceImageParams.usage, sourceImageParams.flags,
                                                           &imageFormatProperties) == vk::VK_ERROR_FORMAT_NOT_SUPPORTED)
            {
                TCU_THROW(NotSupportedError, "Image format not supported");
            }
            m_source = createImage(
                vk, m_device,
                &sourceImageParams); //de::MovePtr<SparseImage>(new SparseImage(vk, vk, vkPhysDevice, vki, sourceImageParams, m_queue, *m_allocator, mapVkFormat(sourceImageParams.format)));
            m_sparseSemaphore = createSemaphore(vk, m_device);
            allocateAndBindSparseImage(vk, m_device, vkPhysDevice, vki, sourceImageParams, m_sparseSemaphore.get(),
                                       context.getSparseQueue(), *m_allocator, m_sparseAllocations,
                                       mapVkFormat(sourceImageParams.format), m_source.get());
        }
#endif
    }

    // Create destination image
    {
        const VkImageCreateInfo destinationImageParams = {
            VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,                               // VkStructureType sType;
            DE_NULL,                                                           // const void* pNext;
            getCreateFlags(m_params.dst.image),                                // VkImageCreateFlags flags;
            m_params.dst.image.imageType,                                      // VkImageType imageType;
            m_params.dst.image.format,                                         // VkFormat format;
            getExtent3D(m_params.dst.image),                                   // VkExtent3D extent;
            1u,                                                                // uint32_t mipLevels;
            getArraySize(m_params.dst.image),                                  // uint32_t arraySize;
            VK_SAMPLE_COUNT_1_BIT,                                             // uint32_t samples;
            VK_IMAGE_TILING_OPTIMAL,                                           // VkImageTiling tiling;
            VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, // VkImageUsageFlags usage;
            m_queueFamilyIndices.size() > 1 ? VK_SHARING_MODE_CONCURRENT :
                                              VK_SHARING_MODE_EXCLUSIVE, // VkSharingMode sharingMode;
            (uint32_t)m_queueFamilyIndices.size(),                       // uint32_t queueFamilyIndexCount;
            m_queueFamilyIndices.data(),                                 // const uint32_t* pQueueFamilyIndices;
            VK_IMAGE_LAYOUT_UNDEFINED,                                   // VkImageLayout initialLayout;
        };

        m_destination           = createImage(vk, m_device, &destinationImageParams);
        m_destinationImageAlloc = allocateImage(vki, vk, vkPhysDevice, m_device, *m_destination, MemoryRequirement::Any,
                                                *m_allocator, m_params.allocationKind, 0u);
        VK_CHECK(vk.bindImageMemory(m_device, *m_destination, m_destinationImageAlloc->getMemory(),
                                    m_destinationImageAlloc->getOffset()));
    }
}

tcu::TestStatus CopyImageToImage::iterate(void)
{
    const bool srcCompressed = isCompressedFormat(m_params.src.image.format);
    const bool dstCompressed = isCompressedFormat(m_params.dst.image.format);

    const tcu::TextureFormat srcTcuFormat = getSizeCompatibleTcuTextureFormat(m_params.src.image.format);
    const tcu::TextureFormat dstTcuFormat = getSizeCompatibleTcuTextureFormat(m_params.dst.image.format);

    m_sourceTextureLevel = de::MovePtr<tcu::TextureLevel>(
        new tcu::TextureLevel(srcTcuFormat, (int)m_params.src.image.extent.width, (int)m_params.src.image.extent.height,
                              (int)m_params.src.image.extent.depth));
    generateBuffer(m_sourceTextureLevel->getAccess(), m_params.src.image.extent.width, m_params.src.image.extent.height,
                   m_params.src.image.extent.depth, m_params.src.image.fillMode);
    m_destinationTextureLevel = de::MovePtr<tcu::TextureLevel>(
        new tcu::TextureLevel(dstTcuFormat, (int)m_params.dst.image.extent.width, (int)m_params.dst.image.extent.height,
                              (int)m_params.dst.image.extent.depth));
    generateBuffer(m_destinationTextureLevel->getAccess(), m_params.dst.image.extent.width,
                   m_params.dst.image.extent.height, m_params.dst.image.extent.depth,
                   m_params.clearDestinationWithRed ? FILL_MODE_RED : m_params.dst.image.fillMode);
    generateExpectedResult();

    uploadImage(m_sourceTextureLevel->getAccess(), m_source.get(), m_params.src.image);
    uploadImage(m_destinationTextureLevel->getAccess(), m_destination.get(), m_params.dst.image);

    const DeviceInterface &vk = m_context.getDeviceInterface();
    const VkDevice vkDevice   = m_device;

    VkQueue queue                    = VK_NULL_HANDLE;
    VkCommandBuffer cmdbuf           = VK_NULL_HANDLE;
    VkCommandPool cmdpool            = VK_NULL_HANDLE;
    std::tie(queue, cmdbuf, cmdpool) = activeExecutionCtx();

    std::vector<VkImageCopy> imageCopies;
    std::vector<VkImageCopy2KHR> imageCopies2KHR;
    for (uint32_t i = 0; i < m_params.regions.size(); i++)
    {
        VkImageCopy imageCopy = m_params.regions[i].imageCopy;

        // When copying between compressed and uncompressed formats the extent
        // members represent the texel dimensions of the source image.
        if (srcCompressed)
        {
            const uint32_t blockWidth  = getBlockWidth(m_params.src.image.format);
            const uint32_t blockHeight = getBlockHeight(m_params.src.image.format);

            imageCopy.srcOffset.x *= blockWidth;
            imageCopy.extent.width *= blockWidth;

            // VUID-vkCmdCopyImage-srcImage-00146
            if (m_params.src.image.imageType != vk::VK_IMAGE_TYPE_1D)
            {
                imageCopy.srcOffset.y *= blockHeight;
                imageCopy.extent.height *= blockHeight;
            }
        }

        if (dstCompressed)
        {
            const uint32_t blockWidth  = getBlockWidth(m_params.dst.image.format);
            const uint32_t blockHeight = getBlockHeight(m_params.dst.image.format);

            imageCopy.dstOffset.x *= blockWidth;

            // VUID-vkCmdCopyImage-dstImage-00152
            if (m_params.dst.image.imageType != vk::VK_IMAGE_TYPE_1D)
            {
                imageCopy.dstOffset.y *= blockHeight;
            }
        }

        if (!(m_params.extensionFlags & COPY_COMMANDS_2))
        {
            imageCopies.push_back(imageCopy);
        }
        else
        {
            DE_ASSERT(m_params.extensionFlags & COPY_COMMANDS_2);
            imageCopies2KHR.push_back(convertvkImageCopyTovkImageCopy2KHR(imageCopy));
        }
    }

    VkImageMemoryBarrier imageBarriers[] = {
        // source image
        {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, // VkStructureType sType;
         DE_NULL,                                // const void* pNext;
         VK_ACCESS_TRANSFER_WRITE_BIT,           // VkAccessFlags srcAccessMask;
         VK_ACCESS_TRANSFER_READ_BIT,            // VkAccessFlags dstAccessMask;
         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,   // VkImageLayout oldLayout;
         m_params.src.image.operationLayout,     // VkImageLayout newLayout;
         VK_QUEUE_FAMILY_IGNORED,                // uint32_t srcQueueFamilyIndex;
         VK_QUEUE_FAMILY_IGNORED,                // uint32_t dstQueueFamilyIndex;
         m_source.get(),                         // VkImage image;
         {
             // VkImageSubresourceRange subresourceRange;
             getAspectFlags(srcTcuFormat),    // VkImageAspectFlags aspectMask;
             0u,                              // uint32_t baseMipLevel;
             1u,                              // uint32_t mipLevels;
             0u,                              // uint32_t baseArraySlice;
             getArraySize(m_params.src.image) // uint32_t arraySize;
         }},
        // destination image
        {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, // VkStructureType sType;
         DE_NULL,                                // const void* pNext;
         VK_ACCESS_TRANSFER_WRITE_BIT,           // VkAccessFlags srcAccessMask;
         VK_ACCESS_TRANSFER_WRITE_BIT,           // VkAccessFlags dstAccessMask;
         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,   // VkImageLayout oldLayout;
         m_params.dst.image.operationLayout,     // VkImageLayout newLayout;
         VK_QUEUE_FAMILY_IGNORED,                // uint32_t srcQueueFamilyIndex;
         VK_QUEUE_FAMILY_IGNORED,                // uint32_t dstQueueFamilyIndex;
         m_destination.get(),                    // VkImage image;
         {
             // VkImageSubresourceRange subresourceRange;
             getAspectFlags(dstTcuFormat),    // VkImageAspectFlags aspectMask;
             0u,                              // uint32_t baseMipLevel;
             1u,                              // uint32_t mipLevels;
             0u,                              // uint32_t baseArraySlice;
             getArraySize(m_params.dst.image) // uint32_t arraySize;
         }},
    };

    VkCommandBuffer recordingBuf = cmdbuf;
    if (m_params.useSecondaryCmdBuffer)
    {
        beginSecondaryCommandBuffer(vk, *m_secondaryCmdBuffer);
        recordingBuf = *m_secondaryCmdBuffer;
    }
    else
    {
        beginCommandBuffer(vk, cmdbuf);
    }

    vk.cmdPipelineBarrier(recordingBuf, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                          (VkDependencyFlags)0, 0, (const VkMemoryBarrier *)DE_NULL, 0,
                          (const VkBufferMemoryBarrier *)DE_NULL, DE_LENGTH_OF_ARRAY(imageBarriers), imageBarriers);

    if (m_params.clearDestinationWithRed)
    {
        VkImageSubresourceRange range = {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u};
        VkClearColorValue clearColor;

        clearColor.float32[0] = 1.0f;
        clearColor.float32[1] = 0.0f;
        clearColor.float32[2] = 0.0f;
        clearColor.float32[3] = 1.0f;
        vk.cmdClearColorImage(recordingBuf, m_destination.get(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearColor, 1u,
                              &range);
        imageBarriers[0].oldLayout = imageBarriers[0].newLayout;
        imageBarriers[1].oldLayout = imageBarriers[1].newLayout;
        vk.cmdPipelineBarrier(recordingBuf, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                              (VkDependencyFlags)0, 0, (const VkMemoryBarrier *)DE_NULL, 0,
                              (const VkBufferMemoryBarrier *)DE_NULL, DE_LENGTH_OF_ARRAY(imageBarriers), imageBarriers);
    }

    if (!(m_params.extensionFlags & COPY_COMMANDS_2))
    {
        vk.cmdCopyImage(recordingBuf, m_source.get(), m_params.src.image.operationLayout, m_destination.get(),
                        m_params.dst.image.operationLayout, (uint32_t)imageCopies.size(), imageCopies.data());
    }
    else
    {
        DE_ASSERT(m_params.extensionFlags & COPY_COMMANDS_2);
        const VkCopyImageInfo2KHR copyImageInfo2KHR = {
            VK_STRUCTURE_TYPE_COPY_IMAGE_INFO_2_KHR, // VkStructureType sType;
            DE_NULL,                                 // const void* pNext;
            m_source.get(),                          // VkImage srcImage;
            m_params.src.image.operationLayout,      // VkImageLayout srcImageLayout;
            m_destination.get(),                     // VkImage dstImage;
            m_params.dst.image.operationLayout,      // VkImageLayout dstImageLayout;
            (uint32_t)imageCopies2KHR.size(),        // uint32_t regionCount;
            imageCopies2KHR.data()                   // const VkImageCopy2KHR* pRegions;
        };

        vk.cmdCopyImage2(recordingBuf, &copyImageInfo2KHR);
    }

    endCommandBuffer(vk, recordingBuf);

    if (m_params.useSecondaryCmdBuffer)
    {
        beginCommandBuffer(vk, cmdbuf);
        vk.cmdExecuteCommands(cmdbuf, 1, &recordingBuf);
        endCommandBuffer(vk, cmdbuf);
    }

    if (m_params.useSparseBinding)
    {
        const VkPipelineStageFlags stageBits[] = {VK_PIPELINE_STAGE_TRANSFER_BIT};
        submitCommandsAndWait(vk, vkDevice, queue, cmdbuf, false, 1u, 1u, &*m_sparseSemaphore, stageBits);
    }
    else
    {
        submitCommandsAndWait(vk, vkDevice, queue, cmdbuf);
    }
    m_context.resetCommandPoolForVKSC(vkDevice, cmdpool);

    if (m_params.useSecondaryCmdBuffer)
        m_context.resetCommandPoolForVKSC(vkDevice, *m_secondaryCmdPool);

    de::MovePtr<tcu::TextureLevel> resultTextureLevel = readImage(*m_destination, m_params.dst.image);

    return checkTestResult(resultTextureLevel->getAccess());
}

tcu::TestStatus CopyImageToImage::checkTestResult(tcu::ConstPixelBufferAccess result)
{
    const tcu::Vec4 fThreshold(0.0f);
    const tcu::UVec4 uThreshold(0u);

    if (tcu::isCombinedDepthStencilType(result.getFormat().type))
    {
        if (tcu::hasDepthComponent(result.getFormat().order))
        {
            const tcu::Sampler::DepthStencilMode mode     = tcu::Sampler::MODE_DEPTH;
            const tcu::ConstPixelBufferAccess depthResult = tcu::getEffectiveDepthStencilAccess(result, mode);
            const tcu::ConstPixelBufferAccess expectedResult =
                tcu::getEffectiveDepthStencilAccess(m_expectedTextureLevel[0]->getAccess(), mode);

            if (isFloatFormat(result.getFormat()))
            {
                if (!tcu::floatThresholdCompare(m_context.getTestContext().getLog(), "Compare", "Result comparison",
                                                expectedResult, depthResult, fThreshold, tcu::COMPARE_LOG_RESULT))
                    return tcu::TestStatus::fail("CopiesAndBlitting test");
            }
            else
            {
                if (!tcu::intThresholdCompare(m_context.getTestContext().getLog(), "Compare", "Result comparison",
                                              expectedResult, depthResult, uThreshold, tcu::COMPARE_LOG_RESULT))
                    return tcu::TestStatus::fail("CopiesAndBlitting test");
            }
        }

        if (tcu::hasStencilComponent(result.getFormat().order))
        {
            const tcu::Sampler::DepthStencilMode mode       = tcu::Sampler::MODE_STENCIL;
            const tcu::ConstPixelBufferAccess stencilResult = tcu::getEffectiveDepthStencilAccess(result, mode);
            const tcu::ConstPixelBufferAccess expectedResult =
                tcu::getEffectiveDepthStencilAccess(m_expectedTextureLevel[0]->getAccess(), mode);

            if (isFloatFormat(result.getFormat()))
            {
                if (!tcu::floatThresholdCompare(m_context.getTestContext().getLog(), "Compare", "Result comparison",
                                                expectedResult, stencilResult, fThreshold, tcu::COMPARE_LOG_RESULT))
                    return tcu::TestStatus::fail("CopiesAndBlitting test");
            }
            else
            {
                if (!tcu::intThresholdCompare(m_context.getTestContext().getLog(), "Compare", "Result comparison",
                                              expectedResult, stencilResult, uThreshold, tcu::COMPARE_LOG_RESULT))
                    return tcu::TestStatus::fail("CopiesAndBlitting test");
            }
        }
    }
    else
    {
        if (!tcu::bitwiseCompare(m_context.getTestContext().getLog(), "Compare", "Result comparison",
                                 m_expectedTextureLevel[0]->getAccess(), result, tcu::COMPARE_LOG_RESULT))
            return tcu::TestStatus::fail("CopiesAndBlitting test");
    }

    return tcu::TestStatus::pass("CopiesAndBlitting test");
}

void CopyImageToImage::copyRegionToTextureLevel(tcu::ConstPixelBufferAccess src, tcu::PixelBufferAccess dst,
                                                CopyRegion region, uint32_t mipLevel)
{
    DE_UNREF(mipLevel);

    VkOffset3D srcOffset = region.imageCopy.srcOffset;
    VkOffset3D dstOffset = region.imageCopy.dstOffset;
    VkExtent3D extent    = region.imageCopy.extent;

    if (region.imageCopy.dstSubresource.baseArrayLayer > region.imageCopy.srcSubresource.baseArrayLayer)
    {
        dstOffset.z  = srcOffset.z;
        extent.depth = std::max(region.imageCopy.extent.depth, region.imageCopy.srcSubresource.layerCount);
    }

    if (region.imageCopy.dstSubresource.baseArrayLayer < region.imageCopy.srcSubresource.baseArrayLayer)
    {
        srcOffset.z  = dstOffset.z;
        extent.depth = std::max(region.imageCopy.extent.depth, region.imageCopy.srcSubresource.layerCount);
    }

    if (tcu::isCombinedDepthStencilType(src.getFormat().type))
    {
        DE_ASSERT(src.getFormat() == dst.getFormat());

        // Copy depth.
        if (tcu::hasDepthComponent(src.getFormat().order))
        {
            const tcu::ConstPixelBufferAccess srcSubRegion =
                getEffectiveDepthStencilAccess(tcu::getSubregion(src, srcOffset.x, srcOffset.y, srcOffset.z,
                                                                 extent.width, extent.height, extent.depth),
                                               tcu::Sampler::MODE_DEPTH);
            const tcu::PixelBufferAccess dstSubRegion =
                getEffectiveDepthStencilAccess(tcu::getSubregion(dst, dstOffset.x, dstOffset.y, dstOffset.z,
                                                                 extent.width, extent.height, extent.depth),
                                               tcu::Sampler::MODE_DEPTH);
            tcu::copy(dstSubRegion, srcSubRegion);
        }

        // Copy stencil.
        if (tcu::hasStencilComponent(src.getFormat().order))
        {
            const tcu::ConstPixelBufferAccess srcSubRegion =
                getEffectiveDepthStencilAccess(tcu::getSubregion(src, srcOffset.x, srcOffset.y, srcOffset.z,
                                                                 extent.width, extent.height, extent.depth),
                                               tcu::Sampler::MODE_STENCIL);
            const tcu::PixelBufferAccess dstSubRegion =
                getEffectiveDepthStencilAccess(tcu::getSubregion(dst, dstOffset.x, dstOffset.y, dstOffset.z,
                                                                 extent.width, extent.height, extent.depth),
                                               tcu::Sampler::MODE_STENCIL);
            tcu::copy(dstSubRegion, srcSubRegion);
        }
    }
    else
    {
        const tcu::ConstPixelBufferAccess srcSubRegion =
            tcu::getSubregion(src, srcOffset.x, srcOffset.y, srcOffset.z, extent.width, extent.height, extent.depth);
        // CopyImage acts like a memcpy. Replace the destination format with the srcformat to use a memcpy.
        const tcu::PixelBufferAccess dstWithSrcFormat(srcSubRegion.getFormat(), dst.getSize(), dst.getDataPtr());
        const tcu::PixelBufferAccess dstSubRegion = tcu::getSubregion(
            dstWithSrcFormat, dstOffset.x, dstOffset.y, dstOffset.z, extent.width, extent.height, extent.depth);

        tcu::copy(dstSubRegion, srcSubRegion);
    }
}

static void checkTransferQueueGranularity(Context &context, const VkExtent3D &extent, VkImageType imageType)
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

class CopyImageToImageTestCase : public vkt::TestCase
{
public:
    CopyImageToImageTestCase(tcu::TestContext &testCtx, const std::string &name, const TestParams params)
        : vkt::TestCase(testCtx, name)
        , m_params(params)
    {
    }

    virtual TestInstance *createInstance(Context &context) const
    {
        return new CopyImageToImage(context, m_params);
    }

    virtual void checkSupport(Context &context) const
    {
        if (m_params.allocationKind == ALLOCATION_KIND_DEDICATED)
        {
            if (!context.isDeviceFunctionalitySupported("VK_KHR_dedicated_allocation"))
                TCU_THROW(NotSupportedError, "VK_KHR_dedicated_allocation is not supported");
        }

#ifndef CTS_USES_VULKANSC
        if (m_params.src.image.format == VK_FORMAT_A8_UNORM_KHR ||
            m_params.dst.image.format == VK_FORMAT_A8_UNORM_KHR ||
            m_params.src.image.format == VK_FORMAT_A1B5G5R5_UNORM_PACK16_KHR ||
            m_params.dst.image.format == VK_FORMAT_A1B5G5R5_UNORM_PACK16_KHR)
            context.requireDeviceFunctionality("VK_KHR_maintenance5");
#endif // CTS_USES_VULKANSC

        checkExtensionSupport(context, m_params.extensionFlags);

        const VkPhysicalDeviceLimits limits = context.getDeviceProperties().limits;
        VkImageFormatProperties properties;

        if ((context.getInstanceInterface().getPhysicalDeviceImageFormatProperties(
                 context.getPhysicalDevice(), m_params.src.image.format, m_params.src.image.imageType,
                 VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_SRC_BIT, 0,
                 &properties) == VK_ERROR_FORMAT_NOT_SUPPORTED) ||
            (context.getInstanceInterface().getPhysicalDeviceImageFormatProperties(
                 context.getPhysicalDevice(), m_params.dst.image.format, m_params.dst.image.imageType,
                 VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT, 0,
                 &properties) == VK_ERROR_FORMAT_NOT_SUPPORTED))
        {
            TCU_THROW(NotSupportedError, "Format not supported");
        }

        // Check maxImageDimension1D
        {
            if (m_params.src.image.imageType == VK_IMAGE_TYPE_1D &&
                m_params.src.image.extent.width > limits.maxImageDimension1D)
                TCU_THROW(NotSupportedError, "Requested 1D src image dimensions not supported");

            if (m_params.dst.image.imageType == VK_IMAGE_TYPE_1D &&
                m_params.dst.image.extent.width > limits.maxImageDimension1D)
                TCU_THROW(NotSupportedError, "Requested 1D dst image dimensions not supported");
        }

        // Check maxImageDimension2D
        {
            if (m_params.src.image.imageType == VK_IMAGE_TYPE_2D &&
                (m_params.src.image.extent.width > limits.maxImageDimension2D ||
                 m_params.src.image.extent.height > limits.maxImageDimension2D))
            {
                TCU_THROW(NotSupportedError, "Requested 2D src image dimensions not supported");
            }

            if (m_params.dst.image.imageType == VK_IMAGE_TYPE_2D &&
                (m_params.dst.image.extent.width > limits.maxImageDimension2D ||
                 m_params.dst.image.extent.height > limits.maxImageDimension2D))
            {
                TCU_THROW(NotSupportedError, "Requested 2D dst image dimensions not supported");
            }
        }

        // Check maxImageDimension3D
        {
            if (m_params.src.image.imageType == VK_IMAGE_TYPE_3D &&
                (m_params.src.image.extent.width > limits.maxImageDimension3D ||
                 m_params.src.image.extent.height > limits.maxImageDimension3D ||
                 m_params.src.image.extent.depth > limits.maxImageDimension3D))
            {
                TCU_THROW(NotSupportedError, "Requested 3D src image dimensions not supported");
            }

            if (m_params.dst.image.imageType == VK_IMAGE_TYPE_3D &&
                (m_params.dst.image.extent.width > limits.maxImageDimension3D ||
                 m_params.dst.image.extent.height > limits.maxImageDimension3D ||
                 m_params.src.image.extent.depth > limits.maxImageDimension3D))
            {
                TCU_THROW(NotSupportedError, "Requested 3D dst image dimensions not supported");
            }
        }

        // Check queue transfer granularity requirements
        if (m_params.queueSelection == QueueSelectionOptions::TransferOnly)
        {
            for (const auto &res : {m_params.src, m_params.dst})
                checkTransferQueueGranularity(context, res.image.extent, res.image.imageType);
            for (const auto &region : m_params.regions)
            {
                checkTransferQueueGranularity(context, region.imageCopy.extent, m_params.src.image.imageType);
                checkTransferQueueGranularity(context, region.imageCopy.extent, m_params.dst.image.imageType);
            }
        }
    }

private:
    TestParams m_params;
};

class CopyImageToImageMipmap : public CopiesAndBlittingTestInstance
{
public:
    CopyImageToImageMipmap(Context &context, TestParams params);
    virtual tcu::TestStatus iterate(void);

protected:
    tcu::TestStatus checkResult(tcu::ConstPixelBufferAccess result, tcu::ConstPixelBufferAccess expected);

private:
    Move<VkImage> m_source;
    de::MovePtr<Allocation> m_sourceImageAlloc;
    Move<VkImage> m_destination;
    de::MovePtr<Allocation> m_destinationImageAlloc;
    std::vector<de::SharedPtr<Allocation>> m_sparseAllocations;
    Move<VkSemaphore> m_sparseSemaphore;

    virtual void copyRegionToTextureLevel(tcu::ConstPixelBufferAccess src, tcu::PixelBufferAccess dst,
                                          CopyRegion region, uint32_t mipLevel = 0u);
};

CopyImageToImageMipmap::CopyImageToImageMipmap(Context &context, TestParams params)
    : CopiesAndBlittingTestInstance(context, params)
{
    const InstanceInterface &vki        = context.getInstanceInterface();
    const DeviceInterface &vk           = context.getDeviceInterface();
    const VkPhysicalDevice vkPhysDevice = context.getPhysicalDevice();

    // Create source image
    {
        VkImageCreateInfo sourceImageParams = {
            VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,                               // VkStructureType sType;
            DE_NULL,                                                           // const void* pNext;
            getCreateFlags(m_params.src.image),                                // VkImageCreateFlags flags;
            m_params.src.image.imageType,                                      // VkImageType imageType;
            m_params.src.image.format,                                         // VkFormat format;
            getExtent3D(m_params.src.image),                                   // VkExtent3D extent;
            params.mipLevels,                                                  // uint32_t mipLevels;
            getArraySize(m_params.src.image),                                  // uint32_t arraySize;
            VK_SAMPLE_COUNT_1_BIT,                                             // uint32_t samples;
            VK_IMAGE_TILING_OPTIMAL,                                           // VkImageTiling tiling;
            VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, // VkImageUsageFlags usage;
            m_queueFamilyIndices.size() > 1 ? VK_SHARING_MODE_CONCURRENT :
                                              VK_SHARING_MODE_EXCLUSIVE, // VkSharingMode sharingMode;
            (uint32_t)m_queueFamilyIndices.size(),                       // uint32_t queueFamilyIndexCount;
            m_queueFamilyIndices.data(),                                 // const uint32_t* pQueueFamilyIndices;
            VK_IMAGE_LAYOUT_UNDEFINED,                                   // VkImageLayout initialLayout;
        };

#ifndef CTS_USES_VULKANSC
        if (!params.useSparseBinding)
        {
#endif
            m_source           = createImage(vk, m_device, &sourceImageParams);
            m_sourceImageAlloc = allocateImage(vki, vk, vkPhysDevice, m_device, *m_source, MemoryRequirement::Any,
                                               *m_allocator, m_params.allocationKind, 0u);
            VK_CHECK(vk.bindImageMemory(m_device, *m_source, m_sourceImageAlloc->getMemory(),
                                        m_sourceImageAlloc->getOffset()));
#ifndef CTS_USES_VULKANSC
        }
        else
        {
            sourceImageParams.flags |=
                (vk::VK_IMAGE_CREATE_SPARSE_BINDING_BIT | vk::VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT);
            vk::VkImageFormatProperties imageFormatProperties;
            if (vki.getPhysicalDeviceImageFormatProperties(vkPhysDevice, sourceImageParams.format,
                                                           sourceImageParams.imageType, sourceImageParams.tiling,
                                                           sourceImageParams.usage, sourceImageParams.flags,
                                                           &imageFormatProperties) == vk::VK_ERROR_FORMAT_NOT_SUPPORTED)
            {
                TCU_THROW(NotSupportedError, "Image format not supported");
            }
            m_source = createImage(
                vk, m_device,
                &sourceImageParams); //de::MovePtr<SparseImage>(new SparseImage(vk, vk, vkPhysDevice, vki, sourceImageParams, m_queue, *m_allocator, mapVkFormat(sourceImageParams.format)));
            m_sparseSemaphore = createSemaphore(vk, m_device);
            allocateAndBindSparseImage(vk, m_device, vkPhysDevice, vki, sourceImageParams, m_sparseSemaphore.get(),
                                       context.getSparseQueue(), *m_allocator, m_sparseAllocations,
                                       mapVkFormat(sourceImageParams.format), m_source.get());
        }
#endif
    }

    // Create destination image
    {
        const VkImageCreateInfo destinationImageParams = {
            VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,                               // VkStructureType sType;
            DE_NULL,                                                           // const void* pNext;
            getCreateFlags(m_params.dst.image),                                // VkImageCreateFlags flags;
            m_params.dst.image.imageType,                                      // VkImageType imageType;
            m_params.dst.image.format,                                         // VkFormat format;
            getExtent3D(m_params.dst.image),                                   // VkExtent3D extent;
            params.mipLevels,                                                  // uint32_t mipLevels;
            getArraySize(m_params.dst.image),                                  // uint32_t arraySize;
            VK_SAMPLE_COUNT_1_BIT,                                             // uint32_t samples;
            VK_IMAGE_TILING_OPTIMAL,                                           // VkImageTiling tiling;
            VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, // VkImageUsageFlags usage;
            m_queueFamilyIndices.size() > 1 ? VK_SHARING_MODE_CONCURRENT :
                                              VK_SHARING_MODE_EXCLUSIVE, // VkSharingMode sharingMode;
            (uint32_t)m_queueFamilyIndices.size(),                       // uint32_t queueFamilyIndexCount;
            m_queueFamilyIndices.data(),                                 // const uint32_t* pQueueFamilyIndices;
            VK_IMAGE_LAYOUT_UNDEFINED,                                   // VkImageLayout initialLayout;
        };

        m_destination           = createImage(vk, m_device, &destinationImageParams);
        m_destinationImageAlloc = allocateImage(vki, vk, vkPhysDevice, m_device, *m_destination, MemoryRequirement::Any,
                                                *m_allocator, m_params.allocationKind, 0u);
        VK_CHECK(vk.bindImageMemory(m_device, *m_destination, m_destinationImageAlloc->getMemory(),
                                    m_destinationImageAlloc->getOffset()));
    }
}

tcu::TestStatus CopyImageToImageMipmap::iterate(void)
{
    const tcu::TextureFormat srcTcuFormat = getSizeCompatibleTcuTextureFormat(m_params.src.image.format);
    const tcu::TextureFormat dstTcuFormat = getSizeCompatibleTcuTextureFormat(m_params.dst.image.format);

    m_sourceTextureLevel = de::MovePtr<tcu::TextureLevel>(
        new tcu::TextureLevel(srcTcuFormat, (int)m_params.src.image.extent.width, (int)m_params.src.image.extent.height,
                              (int)m_params.src.image.extent.depth));
    generateBuffer(m_sourceTextureLevel->getAccess(), m_params.src.image.extent.width, m_params.src.image.extent.height,
                   m_params.src.image.extent.depth, m_params.src.image.fillMode);
    uploadImage(m_sourceTextureLevel->getAccess(), m_source.get(), m_params.src.image, m_params.mipLevels);

    m_destinationTextureLevel = de::MovePtr<tcu::TextureLevel>(
        new tcu::TextureLevel(dstTcuFormat, (int)m_params.dst.image.extent.width, (int)m_params.dst.image.extent.height,
                              (int)m_params.dst.image.extent.depth));
    generateBuffer(m_destinationTextureLevel->getAccess(), m_params.dst.image.extent.width,
                   m_params.dst.image.extent.height, m_params.dst.image.extent.depth, FILL_MODE_RED);
    uploadImage(m_destinationTextureLevel->getAccess(), m_destination.get(), m_params.dst.image, m_params.mipLevels);

    const DeviceInterface &vk                   = m_context.getDeviceInterface();
    const VkDevice vkDevice                     = m_device;
    VkQueue queue                               = VK_NULL_HANDLE;
    VkCommandBuffer commandBuffer               = VK_NULL_HANDLE;
    VkCommandPool commandPool                   = VK_NULL_HANDLE;
    std::tie(queue, commandBuffer, commandPool) = activeExecutionCtx();

    std::vector<VkImageCopy> imageCopies;
    std::vector<VkImageCopy2KHR> imageCopies2KHR;
    for (uint32_t i = 0; i < m_params.regions.size(); i++)
    {
        VkImageCopy imageCopy = m_params.regions[i].imageCopy;
        uint32_t blockWidth, blockHeight;
        std::tie(blockWidth, blockHeight) = m_params.src.image.texelBlockDimensions();
        if (blockWidth != 1 || blockHeight != 1)
        {
            imageCopy.srcOffset.x *= blockWidth;
            imageCopy.srcOffset.y *= blockHeight;
            // When copying between compressed and uncompressed formats the extent
            // members represent the texel dimensions of the source image.
            imageCopy.extent.width *= blockWidth;
            imageCopy.extent.height *= blockHeight;
        }

        std::tie(blockWidth, blockHeight) = m_params.dst.image.texelBlockDimensions();
        if (blockWidth != 1 || blockHeight != 1)
        {
            imageCopy.dstOffset.x *= blockWidth;
            imageCopy.dstOffset.y *= blockHeight;
        }

        if (!(m_params.extensionFlags & COPY_COMMANDS_2))
        {
            imageCopies.push_back(imageCopy);
        }
        else
        {
            DE_ASSERT(m_params.extensionFlags & COPY_COMMANDS_2);
            imageCopies2KHR.push_back(convertvkImageCopyTovkImageCopy2KHR(imageCopy));
        }
    }

    VkImageMemoryBarrier imageBarriers[] = {
        // source image
        {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, // VkStructureType sType;
         DE_NULL,                                // const void* pNext;
         VK_ACCESS_TRANSFER_WRITE_BIT,           // VkAccessFlags srcAccessMask;
         VK_ACCESS_TRANSFER_READ_BIT,            // VkAccessFlags dstAccessMask;
         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,   // VkImageLayout oldLayout;
         m_params.src.image.operationLayout,     // VkImageLayout newLayout;
         VK_QUEUE_FAMILY_IGNORED,                // uint32_t srcQueueFamilyIndex;
         VK_QUEUE_FAMILY_IGNORED,                // uint32_t dstQueueFamilyIndex;
         m_source.get(),                         // VkImage image;
         {
             // VkImageSubresourceRange subresourceRange;
             getAspectFlags(srcTcuFormat),    // VkImageAspectFlags aspectMask;
             0u,                              // uint32_t baseMipLevel;
             m_params.mipLevels,              // uint32_t mipLevels;
             0u,                              // uint32_t baseArraySlice;
             getArraySize(m_params.src.image) // uint32_t arraySize;
         }},
        // destination image
        {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, // VkStructureType sType;
         DE_NULL,                                // const void* pNext;
         VK_ACCESS_TRANSFER_WRITE_BIT,           // VkAccessFlags srcAccessMask;
         VK_ACCESS_TRANSFER_WRITE_BIT,           // VkAccessFlags dstAccessMask;
         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,   // VkImageLayout oldLayout;
         m_params.dst.image.operationLayout,     // VkImageLayout newLayout;
         VK_QUEUE_FAMILY_IGNORED,                // uint32_t srcQueueFamilyIndex;
         VK_QUEUE_FAMILY_IGNORED,                // uint32_t dstQueueFamilyIndex;
         m_destination.get(),                    // VkImage image;
         {
             // VkImageSubresourceRange subresourceRange;
             getAspectFlags(dstTcuFormat),    // VkImageAspectFlags aspectMask;
             0u,                              // uint32_t baseMipLevel;
             m_params.mipLevels,              // uint32_t mipLevels;
             0u,                              // uint32_t baseArraySlice;
             getArraySize(m_params.dst.image) // uint32_t arraySize;
         }},
    };

    beginCommandBuffer(vk, commandBuffer);
    vk.cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                          (VkDependencyFlags)0, 0, (const VkMemoryBarrier *)DE_NULL, 0,
                          (const VkBufferMemoryBarrier *)DE_NULL, DE_LENGTH_OF_ARRAY(imageBarriers), imageBarriers);

    if (!(m_params.extensionFlags & COPY_COMMANDS_2))
    {
        vk.cmdCopyImage(commandBuffer, m_source.get(), m_params.src.image.operationLayout, m_destination.get(),
                        m_params.dst.image.operationLayout, (uint32_t)imageCopies.size(), imageCopies.data());
    }
    else
    {
        DE_ASSERT(m_params.extensionFlags & COPY_COMMANDS_2);
        const VkCopyImageInfo2KHR copyImageInfo2KHR = {
            VK_STRUCTURE_TYPE_COPY_IMAGE_INFO_2_KHR, // VkStructureType sType;
            DE_NULL,                                 // const void* pNext;
            m_source.get(),                          // VkImage srcImage;
            m_params.src.image.operationLayout,      // VkImageLayout srcImageLayout;
            m_destination.get(),                     // VkImage dstImage;
            m_params.dst.image.operationLayout,      // VkImageLayout dstImageLayout;
            (uint32_t)imageCopies2KHR.size(),        // uint32_t regionCount;
            imageCopies2KHR.data()                   // const VkImageCopy2KHR* pRegions;
        };

        vk.cmdCopyImage2(commandBuffer, &copyImageInfo2KHR);
    }

    endCommandBuffer(vk, commandBuffer);

    if (m_params.useSparseBinding)
    {
        const VkPipelineStageFlags stageBits[] = {VK_PIPELINE_STAGE_TRANSFER_BIT};
        submitCommandsAndWait(vk, vkDevice, queue, commandBuffer, false, 1u, 1u, &*m_sparseSemaphore, stageBits);
    }
    else
    {
        submitCommandsAndWait(vk, vkDevice, queue, commandBuffer);
    }

    m_context.resetCommandPoolForVKSC(vkDevice, commandPool);

    for (uint32_t miplevel = 0; miplevel < m_params.mipLevels; miplevel++)
    {
        de::MovePtr<tcu::TextureLevel> resultTextureLevel   = readImage(*m_destination, m_params.dst.image, miplevel);
        de::MovePtr<tcu::TextureLevel> expectedTextureLevel = readImage(*m_source, m_params.src.image, miplevel);

        tcu::TestStatus result = checkResult(resultTextureLevel->getAccess(), expectedTextureLevel->getAccess());
        if (result.getCode() != QP_TEST_RESULT_PASS)
            return result;
    }
    return tcu::TestStatus::pass("Pass");
}

tcu::TestStatus CopyImageToImageMipmap::checkResult(tcu::ConstPixelBufferAccess result,
                                                    tcu::ConstPixelBufferAccess expected)
{
    const tcu::Vec4 fThreshold(0.0f);
    const tcu::UVec4 uThreshold(0u);

    if (tcu::isCombinedDepthStencilType(result.getFormat().type))
    {
        if (tcu::hasDepthComponent(result.getFormat().order))
        {
            const tcu::Sampler::DepthStencilMode mode        = tcu::Sampler::MODE_DEPTH;
            const tcu::ConstPixelBufferAccess depthResult    = tcu::getEffectiveDepthStencilAccess(result, mode);
            const tcu::ConstPixelBufferAccess expectedResult = tcu::getEffectiveDepthStencilAccess(expected, mode);

            if (isFloatFormat(result.getFormat()))
            {
                if (!tcu::floatThresholdCompare(m_context.getTestContext().getLog(), "Compare", "Result comparison",
                                                expectedResult, depthResult, fThreshold, tcu::COMPARE_LOG_RESULT))
                    return tcu::TestStatus::fail("CopiesAndBlitting test");
            }
            else
            {
                if (!tcu::intThresholdCompare(m_context.getTestContext().getLog(), "Compare", "Result comparison",
                                              expectedResult, depthResult, uThreshold, tcu::COMPARE_LOG_RESULT))
                    return tcu::TestStatus::fail("CopiesAndBlitting test");
            }
        }

        if (tcu::hasStencilComponent(result.getFormat().order))
        {
            const tcu::Sampler::DepthStencilMode mode        = tcu::Sampler::MODE_STENCIL;
            const tcu::ConstPixelBufferAccess stencilResult  = tcu::getEffectiveDepthStencilAccess(result, mode);
            const tcu::ConstPixelBufferAccess expectedResult = tcu::getEffectiveDepthStencilAccess(expected, mode);

            if (isFloatFormat(result.getFormat()))
            {
                if (!tcu::floatThresholdCompare(m_context.getTestContext().getLog(), "Compare", "Result comparison",
                                                expectedResult, stencilResult, fThreshold, tcu::COMPARE_LOG_RESULT))
                    return tcu::TestStatus::fail("CopiesAndBlitting test");
            }
            else
            {
                if (!tcu::intThresholdCompare(m_context.getTestContext().getLog(), "Compare", "Result comparison",
                                              expectedResult, stencilResult, uThreshold, tcu::COMPARE_LOG_RESULT))
                    return tcu::TestStatus::fail("CopiesAndBlitting test");
            }
        }
    }
    else
    {
        if (isFloatFormat(result.getFormat()))
        {
            if (!tcu::floatThresholdCompare(m_context.getTestContext().getLog(), "Compare", "Result comparison",
                                            expected, result, fThreshold, tcu::COMPARE_LOG_RESULT))
                return tcu::TestStatus::fail("CopiesAndBlitting test");
        }
        else if (isSnormFormat(mapTextureFormat(result.getFormat())))
        {
            // There may be an ambiguity between two possible binary representations of 1.0.
            // Get rid of that by expanding the data to floats and re-normalizing again.

            tcu::TextureLevel resultSnorm(result.getFormat(), result.getWidth(), result.getHeight(), result.getDepth());
            {
                tcu::TextureLevel resultFloat(
                    tcu::TextureFormat(resultSnorm.getFormat().order, tcu::TextureFormat::FLOAT),
                    resultSnorm.getWidth(), resultSnorm.getHeight(), resultSnorm.getDepth());

                tcu::copy(resultFloat.getAccess(), result);
                tcu::copy(resultSnorm, resultFloat.getAccess());
            }

            tcu::TextureLevel expectedSnorm(expected.getFormat(), expected.getWidth(), expected.getHeight(),
                                            expected.getDepth());

            {
                tcu::TextureLevel expectedFloat(
                    tcu::TextureFormat(expectedSnorm.getFormat().order, tcu::TextureFormat::FLOAT),
                    expectedSnorm.getWidth(), expectedSnorm.getHeight(), expectedSnorm.getDepth());

                tcu::copy(expectedFloat.getAccess(), m_expectedTextureLevel[0]->getAccess());
                tcu::copy(expectedSnorm, expectedFloat.getAccess());
            }

            if (!tcu::intThresholdCompare(m_context.getTestContext().getLog(), "Compare", "Result comparison",
                                          expectedSnorm.getAccess(), resultSnorm.getAccess(), uThreshold,
                                          tcu::COMPARE_LOG_RESULT))
                return tcu::TestStatus::fail("CopiesAndBlitting test");
        }
        else
        {
            if (!tcu::intThresholdCompare(m_context.getTestContext().getLog(), "Compare", "Result comparison", expected,
                                          result, uThreshold, tcu::COMPARE_LOG_RESULT))
                return tcu::TestStatus::fail("CopiesAndBlitting test");
        }
    }

    return tcu::TestStatus::pass("CopiesAndBlitting test");
}

void CopyImageToImageMipmap::copyRegionToTextureLevel(tcu::ConstPixelBufferAccess src, tcu::PixelBufferAccess dst,
                                                      CopyRegion region, uint32_t mipLevel)
{
    DE_UNREF(mipLevel);

    VkOffset3D srcOffset = region.imageCopy.srcOffset;
    VkOffset3D dstOffset = region.imageCopy.dstOffset;
    VkExtent3D extent    = region.imageCopy.extent;

    if (m_params.src.image.imageType == VK_IMAGE_TYPE_3D && m_params.dst.image.imageType == VK_IMAGE_TYPE_2D)
    {
        dstOffset.z  = srcOffset.z;
        extent.depth = std::max(region.imageCopy.extent.depth, region.imageCopy.dstSubresource.layerCount);
    }
    if (m_params.src.image.imageType == VK_IMAGE_TYPE_2D && m_params.dst.image.imageType == VK_IMAGE_TYPE_3D)
    {
        srcOffset.z  = dstOffset.z;
        extent.depth = std::max(region.imageCopy.extent.depth, region.imageCopy.srcSubresource.layerCount);
    }

    if (tcu::isCombinedDepthStencilType(src.getFormat().type))
    {
        DE_ASSERT(src.getFormat() == dst.getFormat());

        // Copy depth.
        if (tcu::hasDepthComponent(src.getFormat().order))
        {
            const tcu::ConstPixelBufferAccess srcSubRegion =
                getEffectiveDepthStencilAccess(tcu::getSubregion(src, srcOffset.x, srcOffset.y, srcOffset.z,
                                                                 extent.width, extent.height, extent.depth),
                                               tcu::Sampler::MODE_DEPTH);
            const tcu::PixelBufferAccess dstSubRegion =
                getEffectiveDepthStencilAccess(tcu::getSubregion(dst, dstOffset.x, dstOffset.y, dstOffset.z,
                                                                 extent.width, extent.height, extent.depth),
                                               tcu::Sampler::MODE_DEPTH);
            tcu::copy(dstSubRegion, srcSubRegion);
        }

        // Copy stencil.
        if (tcu::hasStencilComponent(src.getFormat().order))
        {
            const tcu::ConstPixelBufferAccess srcSubRegion =
                getEffectiveDepthStencilAccess(tcu::getSubregion(src, srcOffset.x, srcOffset.y, srcOffset.z,
                                                                 extent.width, extent.height, extent.depth),
                                               tcu::Sampler::MODE_STENCIL);
            const tcu::PixelBufferAccess dstSubRegion =
                getEffectiveDepthStencilAccess(tcu::getSubregion(dst, dstOffset.x, dstOffset.y, dstOffset.z,
                                                                 extent.width, extent.height, extent.depth),
                                               tcu::Sampler::MODE_STENCIL);
            tcu::copy(dstSubRegion, srcSubRegion);
        }
    }
    else
    {
        const tcu::ConstPixelBufferAccess srcSubRegion =
            tcu::getSubregion(src, srcOffset.x, srcOffset.y, srcOffset.z, extent.width, extent.height, extent.depth);
        // CopyImage acts like a memcpy. Replace the destination format with the srcformat to use a memcpy.
        const tcu::PixelBufferAccess dstWithSrcFormat(srcSubRegion.getFormat(), dst.getSize(), dst.getDataPtr());
        const tcu::PixelBufferAccess dstSubRegion = tcu::getSubregion(
            dstWithSrcFormat, dstOffset.x, dstOffset.y, dstOffset.z, extent.width, extent.height, extent.depth);

        tcu::copy(dstSubRegion, srcSubRegion);
    }
}

class CopyImageToImageMipmapTestCase : public vkt::TestCase
{
public:
    CopyImageToImageMipmapTestCase(tcu::TestContext &testCtx, const std::string &name, const TestParams params)
        : vkt::TestCase(testCtx, name)
        , m_params(params)
    {
    }

    virtual TestInstance *createInstance(Context &context) const
    {
        return new CopyImageToImageMipmap(context, m_params);
    }

    virtual void checkSupport(Context &context) const
    {
        if (m_params.allocationKind == ALLOCATION_KIND_DEDICATED)
        {
            if (!context.isDeviceFunctionalitySupported("VK_KHR_dedicated_allocation"))
                TCU_THROW(NotSupportedError, "VK_KHR_dedicated_allocation is not supported");
        }

        checkExtensionSupport(context, m_params.extensionFlags);

        const VkPhysicalDeviceLimits limits = context.getDeviceProperties().limits;
        VkImageFormatProperties properties;

        if ((context.getInstanceInterface().getPhysicalDeviceImageFormatProperties(
                 context.getPhysicalDevice(), m_params.src.image.format, m_params.src.image.imageType,
                 VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_SRC_BIT, 0,
                 &properties) == VK_ERROR_FORMAT_NOT_SUPPORTED) ||
            (context.getInstanceInterface().getPhysicalDeviceImageFormatProperties(
                 context.getPhysicalDevice(), m_params.dst.image.format, m_params.dst.image.imageType,
                 VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT, 0,
                 &properties) == VK_ERROR_FORMAT_NOT_SUPPORTED))
        {
            TCU_THROW(NotSupportedError, "Format not supported");
        }

        // Check maxImageDimension1D
        {
            if (m_params.src.image.imageType == VK_IMAGE_TYPE_1D &&
                m_params.src.image.extent.width > limits.maxImageDimension1D)
                TCU_THROW(NotSupportedError, "Requested 1D src image dimensions not supported");

            if (m_params.dst.image.imageType == VK_IMAGE_TYPE_1D &&
                m_params.dst.image.extent.width > limits.maxImageDimension1D)
                TCU_THROW(NotSupportedError, "Requested 1D dst image dimensions not supported");
        }

        // Check maxImageDimension2D
        {
            if (m_params.src.image.imageType == VK_IMAGE_TYPE_2D &&
                (m_params.src.image.extent.width > limits.maxImageDimension2D ||
                 m_params.src.image.extent.height > limits.maxImageDimension2D))
            {
                TCU_THROW(NotSupportedError, "Requested 2D src image dimensions not supported");
            }

            if (m_params.dst.image.imageType == VK_IMAGE_TYPE_2D &&
                (m_params.dst.image.extent.width > limits.maxImageDimension2D ||
                 m_params.dst.image.extent.height > limits.maxImageDimension2D))
            {
                TCU_THROW(NotSupportedError, "Requested 2D dst image dimensions not supported");
            }
        }

        // Check maxImageDimension3D
        {
            if (m_params.src.image.imageType == VK_IMAGE_TYPE_3D &&
                (m_params.src.image.extent.width > limits.maxImageDimension3D ||
                 m_params.src.image.extent.height > limits.maxImageDimension3D ||
                 m_params.src.image.extent.depth > limits.maxImageDimension3D))
            {
                TCU_THROW(NotSupportedError, "Requested 3D src image dimensions not supported");
            }

            if (m_params.dst.image.imageType == VK_IMAGE_TYPE_3D &&
                (m_params.dst.image.extent.width > limits.maxImageDimension3D ||
                 m_params.dst.image.extent.height > limits.maxImageDimension3D ||
                 m_params.src.image.extent.depth > limits.maxImageDimension3D))
            {
                TCU_THROW(NotSupportedError, "Requested 3D dst image dimensions not supported");
            }
        }

        // Check queue transfer granularity requirements
        if (m_params.queueSelection == QueueSelectionOptions::TransferOnly)
            for (const auto &res : {m_params.src, m_params.dst})
                checkTransferQueueGranularity(context, res.image.extent, res.image.imageType);
    }

private:
    TestParams m_params;
};

// Copy from buffer to buffer.

class CopyBufferToBuffer : public CopiesAndBlittingTestInstance
{
public:
    CopyBufferToBuffer(Context &context, TestParams params);
    virtual tcu::TestStatus iterate(void);

private:
    virtual void copyRegionToTextureLevel(tcu::ConstPixelBufferAccess, tcu::PixelBufferAccess, CopyRegion,
                                          uint32_t mipLevel = 0u);
    Move<VkBuffer> m_source;
    de::MovePtr<Allocation> m_sourceBufferAlloc;
    Move<VkBuffer> m_destination;
    de::MovePtr<Allocation> m_destinationBufferAlloc;
};

CopyBufferToBuffer::CopyBufferToBuffer(Context &context, TestParams params)
    : CopiesAndBlittingTestInstance(context, params)
{
    const InstanceInterface &vki        = context.getInstanceInterface();
    const DeviceInterface &vk           = context.getDeviceInterface();
    const VkPhysicalDevice vkPhysDevice = context.getPhysicalDevice();

    // Create source buffer
    {
        const VkBufferCreateInfo sourceBufferParams = {
            VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, // VkStructureType sType;
            DE_NULL,                              // const void* pNext;
            0u,                                   // VkBufferCreateFlags flags;
            m_params.src.buffer.size,             // VkDeviceSize size;
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,     // VkBufferUsageFlags usage;
            VK_SHARING_MODE_EXCLUSIVE,            // VkSharingMode sharingMode;
            0u,                                   // uint32_t queueFamilyIndexCount;
            (const uint32_t *)DE_NULL,            // const uint32_t* pQueueFamilyIndices;
        };

        m_source            = createBuffer(vk, m_device, &sourceBufferParams);
        m_sourceBufferAlloc = allocateBuffer(vki, vk, vkPhysDevice, m_device, *m_source, MemoryRequirement::HostVisible,
                                             *m_allocator, m_params.allocationKind);
        VK_CHECK(vk.bindBufferMemory(m_device, *m_source, m_sourceBufferAlloc->getMemory(),
                                     m_sourceBufferAlloc->getOffset()));
    }

    // Create destination buffer
    {
        const VkBufferCreateInfo destinationBufferParams = {
            VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, // VkStructureType sType;
            DE_NULL,                              // const void* pNext;
            0u,                                   // VkBufferCreateFlags flags;
            m_params.dst.buffer.size,             // VkDeviceSize size;
            VK_BUFFER_USAGE_TRANSFER_DST_BIT,     // VkBufferUsageFlags usage;
            VK_SHARING_MODE_EXCLUSIVE,            // VkSharingMode sharingMode;
            0u,                                   // uint32_t queueFamilyIndexCount;
            (const uint32_t *)DE_NULL,            // const uint32_t* pQueueFamilyIndices;
        };

        m_destination = createBuffer(vk, m_device, &destinationBufferParams);
        m_destinationBufferAlloc =
            allocateBuffer(vki, vk, vkPhysDevice, m_device, *m_destination, MemoryRequirement::HostVisible,
                           *m_allocator, m_params.allocationKind);
        VK_CHECK(vk.bindBufferMemory(m_device, *m_destination, m_destinationBufferAlloc->getMemory(),
                                     m_destinationBufferAlloc->getOffset()));
    }
}

tcu::TestStatus CopyBufferToBuffer::iterate(void)
{
    const int srcLevelWidth = (int)(m_params.src.buffer.size /
                                    4); // Here the format is VK_FORMAT_R32_UINT, we need to divide the buffer size by 4
    m_sourceTextureLevel =
        de::MovePtr<tcu::TextureLevel>(new tcu::TextureLevel(mapVkFormat(VK_FORMAT_R32_UINT), srcLevelWidth, 1));
    generateBuffer(m_sourceTextureLevel->getAccess(), srcLevelWidth, 1, 1, FILL_MODE_RED);

    const int dstLevelWidth = (int)(m_params.dst.buffer.size / 4);
    m_destinationTextureLevel =
        de::MovePtr<tcu::TextureLevel>(new tcu::TextureLevel(mapVkFormat(VK_FORMAT_R32_UINT), dstLevelWidth, 1));
    generateBuffer(m_destinationTextureLevel->getAccess(), dstLevelWidth, 1, 1, FILL_MODE_BLACK);

    generateExpectedResult();

    uploadBuffer(m_sourceTextureLevel->getAccess(), *m_sourceBufferAlloc);
    uploadBuffer(m_destinationTextureLevel->getAccess(), *m_destinationBufferAlloc);

    const DeviceInterface &vk                   = m_context.getDeviceInterface();
    VkQueue queue                               = VK_NULL_HANDLE;
    VkCommandBuffer commandBuffer               = VK_NULL_HANDLE;
    VkCommandPool commandPool                   = VK_NULL_HANDLE;
    std::tie(queue, commandBuffer, commandPool) = activeExecutionCtx();

    const VkBufferMemoryBarrier srcBufferBarrier = {
        VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER, // VkStructureType sType;
        DE_NULL,                                 // const void* pNext;
        VK_ACCESS_HOST_WRITE_BIT,                // VkAccessFlags srcAccessMask;
        VK_ACCESS_TRANSFER_READ_BIT,             // VkAccessFlags dstAccessMask;
        VK_QUEUE_FAMILY_IGNORED,                 // uint32_t srcQueueFamilyIndex;
        VK_QUEUE_FAMILY_IGNORED,                 // uint32_t dstQueueFamilyIndex;
        *m_source,                               // VkBuffer buffer;
        0u,                                      // VkDeviceSize offset;
        m_params.src.buffer.size                 // VkDeviceSize size;
    };

    const VkBufferMemoryBarrier dstBufferBarrier = {
        VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER, // VkStructureType sType;
        DE_NULL,                                 // const void* pNext;
        VK_ACCESS_TRANSFER_WRITE_BIT,            // VkAccessFlags srcAccessMask;
        VK_ACCESS_HOST_READ_BIT,                 // VkAccessFlags dstAccessMask;
        VK_QUEUE_FAMILY_IGNORED,                 // uint32_t srcQueueFamilyIndex;
        VK_QUEUE_FAMILY_IGNORED,                 // uint32_t dstQueueFamilyIndex;
        *m_destination,                          // VkBuffer buffer;
        0u,                                      // VkDeviceSize offset;
        m_params.dst.buffer.size                 // VkDeviceSize size;
    };

    std::vector<VkBufferCopy> bufferCopies;
    std::vector<VkBufferCopy2KHR> bufferCopies2KHR;
    for (uint32_t i = 0; i < m_params.regions.size(); i++)
    {
        if (!(m_params.extensionFlags & COPY_COMMANDS_2))
        {
            bufferCopies.push_back(m_params.regions[i].bufferCopy);
        }
        else
        {
            DE_ASSERT(m_params.extensionFlags & COPY_COMMANDS_2);
            bufferCopies2KHR.push_back(convertvkBufferCopyTovkBufferCopy2KHR(m_params.regions[i].bufferCopy));
        }
    }

    beginCommandBuffer(vk, commandBuffer);
    vk.cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                          (VkDependencyFlags)0, 0, (const VkMemoryBarrier *)DE_NULL, 1, &srcBufferBarrier, 0,
                          (const VkImageMemoryBarrier *)DE_NULL);

    if (!(m_params.extensionFlags & COPY_COMMANDS_2))
    {
        vk.cmdCopyBuffer(commandBuffer, m_source.get(), m_destination.get(), (uint32_t)m_params.regions.size(),
                         &bufferCopies[0]);
    }
    else
    {
        DE_ASSERT(m_params.extensionFlags & COPY_COMMANDS_2);
        const VkCopyBufferInfo2KHR copyBufferInfo2KHR = {
            VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2_KHR, // VkStructureType sType;
            DE_NULL,                                  // const void* pNext;
            m_source.get(),                           // VkBuffer srcBuffer;
            m_destination.get(),                      // VkBuffer dstBuffer;
            (uint32_t)m_params.regions.size(),        // uint32_t regionCount;
            &bufferCopies2KHR[0]                      // const VkBufferCopy2KHR* pRegions;
        };

        vk.cmdCopyBuffer2(commandBuffer, &copyBufferInfo2KHR);
    }

    vk.cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                          (VkDependencyFlags)0, 0, (const VkMemoryBarrier *)DE_NULL, 1, &dstBufferBarrier, 0,
                          (const VkImageMemoryBarrier *)DE_NULL);
    endCommandBuffer(vk, commandBuffer);
    submitCommandsAndWait(vk, m_device, queue, commandBuffer);
    m_context.resetCommandPoolForVKSC(m_device, commandPool);

    // Read buffer data
    de::MovePtr<tcu::TextureLevel> resultLevel(
        new tcu::TextureLevel(mapVkFormat(VK_FORMAT_R32_UINT), dstLevelWidth, 1));
    invalidateAlloc(vk, m_device, *m_destinationBufferAlloc);
    tcu::copy(*resultLevel, tcu::ConstPixelBufferAccess(resultLevel->getFormat(), resultLevel->getSize(),
                                                        m_destinationBufferAlloc->getHostPtr()));

    return checkTestResult(resultLevel->getAccess());
}

void CopyBufferToBuffer::copyRegionToTextureLevel(tcu::ConstPixelBufferAccess src, tcu::PixelBufferAccess dst,
                                                  CopyRegion region, uint32_t mipLevel)
{
    DE_UNREF(mipLevel);

    deMemcpy((uint8_t *)dst.getDataPtr() + region.bufferCopy.dstOffset,
             (uint8_t *)src.getDataPtr() + region.bufferCopy.srcOffset, (size_t)region.bufferCopy.size);
}

class BufferToBufferTestCase : public vkt::TestCase
{
public:
    BufferToBufferTestCase(tcu::TestContext &testCtx, const std::string &name, const TestParams params)
        : vkt::TestCase(testCtx, name)
        , m_params(params)
    {
    }

    virtual TestInstance *createInstance(Context &context) const
    {
        return new CopyBufferToBuffer(context, m_params);
    }

    virtual void checkSupport(Context &context) const
    {
        checkExtensionSupport(context, m_params.extensionFlags);
    }

private:
    TestParams m_params;
};

// Copy from image to buffer.

class CopyImageToBuffer : public CopiesAndBlittingTestInstance
{
public:
    CopyImageToBuffer(Context &context, TestParams testParams);
    virtual tcu::TestStatus iterate(void);

private:
    virtual void copyRegionToTextureLevel(tcu::ConstPixelBufferAccess src, tcu::PixelBufferAccess dst,
                                          CopyRegion region, uint32_t mipLevel = 0u);

    tcu::TextureFormat m_textureFormat;
    VkDeviceSize m_bufferSize;

    Move<VkImage> m_source;
    de::MovePtr<Allocation> m_sourceImageAlloc;
    Move<VkBuffer> m_destination;
    de::MovePtr<Allocation> m_destinationBufferAlloc;

    std::vector<de::SharedPtr<Allocation>> m_sparseAllocations;
    Move<VkSemaphore> m_sparseSemaphore;
};

CopyImageToBuffer::CopyImageToBuffer(Context &context, TestParams testParams)
    : CopiesAndBlittingTestInstance(context, testParams)
    , m_textureFormat(mapVkFormat(testParams.src.image.format))
    , m_bufferSize(m_params.dst.buffer.size * tcu::getPixelSize(m_textureFormat))
{
    const InstanceInterface &vki        = context.getInstanceInterface();
    const DeviceInterface &vk           = context.getDeviceInterface();
    const VkPhysicalDevice vkPhysDevice = context.getPhysicalDevice();

    // Create source image
    {
        VkImageCreateInfo sourceImageParams = {
            VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,                               // VkStructureType sType;
            DE_NULL,                                                           // const void* pNext;
            getCreateFlags(m_params.src.image),                                // VkImageCreateFlags flags;
            m_params.src.image.imageType,                                      // VkImageType imageType;
            m_params.src.image.format,                                         // VkFormat format;
            getExtent3D(m_params.src.image),                                   // VkExtent3D extent;
            1u,                                                                // uint32_t mipLevels;
            getArraySize(m_params.src.image),                                  // uint32_t arraySize;
            VK_SAMPLE_COUNT_1_BIT,                                             // uint32_t samples;
            VK_IMAGE_TILING_OPTIMAL,                                           // VkImageTiling tiling;
            VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, // VkImageUsageFlags usage;
            VK_SHARING_MODE_EXCLUSIVE,                                         // VkSharingMode sharingMode;
            0u,                                                                // uint32_t queueFamilyIndexCount;
            (const uint32_t *)DE_NULL,                                         // const uint32_t* pQueueFamilyIndices;
            VK_IMAGE_LAYOUT_UNDEFINED,                                         // VkImageLayout initialLayout;
        };

#ifndef CTS_USES_VULKANSC
        if (!testParams.useSparseBinding)
        {
#endif
            m_source           = createImage(vk, m_device, &sourceImageParams);
            m_sourceImageAlloc = allocateImage(vki, vk, vkPhysDevice, m_device, *m_source, MemoryRequirement::Any,
                                               *m_allocator, m_params.allocationKind, 0u);
            VK_CHECK(vk.bindImageMemory(m_device, *m_source, m_sourceImageAlloc->getMemory(),
                                        m_sourceImageAlloc->getOffset()));
#ifndef CTS_USES_VULKANSC
        }
        else
        {
            sourceImageParams.flags |=
                (vk::VK_IMAGE_CREATE_SPARSE_BINDING_BIT | vk::VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT);
            vk::VkImageFormatProperties imageFormatProperties;
            if (vki.getPhysicalDeviceImageFormatProperties(vkPhysDevice, sourceImageParams.format,
                                                           sourceImageParams.imageType, sourceImageParams.tiling,
                                                           sourceImageParams.usage, sourceImageParams.flags,
                                                           &imageFormatProperties) == vk::VK_ERROR_FORMAT_NOT_SUPPORTED)
            {
                TCU_THROW(NotSupportedError, "Image format not supported");
            }
            m_source = createImage(
                vk, m_device,
                &sourceImageParams); //de::MovePtr<SparseImage>(new SparseImage(vk, vk, vkPhysDevice, vki, sourceImageParams, m_queue, *m_allocator, mapVkFormat(sourceImageParams.format)));
            m_sparseSemaphore = createSemaphore(vk, m_device);
            allocateAndBindSparseImage(vk, m_device, vkPhysDevice, vki, sourceImageParams, m_sparseSemaphore.get(),
                                       context.getSparseQueue(), *m_allocator, m_sparseAllocations,
                                       mapVkFormat(sourceImageParams.format), m_source.get());
        }
#endif
    }

    // Create destination buffer
    {
        const VkBufferCreateInfo destinationBufferParams = {
            VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, // VkStructureType sType;
            DE_NULL,                              // const void* pNext;
            0u,                                   // VkBufferCreateFlags flags;
            m_bufferSize,                         // VkDeviceSize size;
            VK_BUFFER_USAGE_TRANSFER_DST_BIT,     // VkBufferUsageFlags usage;
            VK_SHARING_MODE_EXCLUSIVE,            // VkSharingMode sharingMode;
            0u,                                   // uint32_t queueFamilyIndexCount;
            (const uint32_t *)DE_NULL,            // const uint32_t* pQueueFamilyIndices;
        };

        m_destination = createBuffer(vk, m_device, &destinationBufferParams);
        m_destinationBufferAlloc =
            allocateBuffer(vki, vk, vkPhysDevice, m_device, *m_destination, MemoryRequirement::HostVisible,
                           *m_allocator, m_params.allocationKind);
        VK_CHECK(vk.bindBufferMemory(m_device, *m_destination, m_destinationBufferAlloc->getMemory(),
                                     m_destinationBufferAlloc->getOffset()));
    }
}

tcu::TestStatus CopyImageToBuffer::iterate(void)
{
    m_sourceTextureLevel = de::MovePtr<tcu::TextureLevel>(
        new tcu::TextureLevel(m_textureFormat, m_params.src.image.extent.width, m_params.src.image.extent.height,
                              m_params.src.image.extent.depth));
    generateBuffer(m_sourceTextureLevel->getAccess(), m_params.src.image.extent.width, m_params.src.image.extent.height,
                   m_params.src.image.extent.depth, m_params.src.image.fillMode);
    m_destinationTextureLevel =
        de::MovePtr<tcu::TextureLevel>(new tcu::TextureLevel(m_textureFormat, (int)m_params.dst.buffer.size, 1));
    generateBuffer(m_destinationTextureLevel->getAccess(), (int)m_params.dst.buffer.size, 1, 1,
                   m_params.dst.buffer.fillMode);

    generateExpectedResult();

    uploadImage(m_sourceTextureLevel->getAccess(), *m_source, m_params.src.image);
    uploadBuffer(m_destinationTextureLevel->getAccess(), *m_destinationBufferAlloc);

    const DeviceInterface &vk                   = m_context.getDeviceInterface();
    const VkDevice vkDevice                     = m_device;
    VkQueue queue                               = VK_NULL_HANDLE;
    VkCommandBuffer commandBuffer               = VK_NULL_HANDLE;
    VkCommandPool commandPool                   = VK_NULL_HANDLE;
    std::tie(queue, commandBuffer, commandPool) = activeExecutionCtx();

    // Barriers for copying image to buffer
    const VkImageMemoryBarrier imageBarrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, // VkStructureType sType;
                                               DE_NULL,                                // const void* pNext;
                                               VK_ACCESS_TRANSFER_WRITE_BIT,           // VkAccessFlags srcAccessMask;
                                               VK_ACCESS_TRANSFER_READ_BIT,            // VkAccessFlags dstAccessMask;
                                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,   // VkImageLayout oldLayout;
                                               VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,   // VkImageLayout newLayout;
                                               VK_QUEUE_FAMILY_IGNORED,                // uint32_t srcQueueFamilyIndex;
                                               VK_QUEUE_FAMILY_IGNORED,                // uint32_t dstQueueFamilyIndex;
                                               *m_source,                              // VkImage image;
                                               {
                                                   // VkImageSubresourceRange subresourceRange;
                                                   getAspectFlags(m_textureFormat), // VkImageAspectFlags aspectMask;
                                                   0u,                              // uint32_t baseMipLevel;
                                                   1u,                              // uint32_t mipLevels;
                                                   0u,                              // uint32_t baseArraySlice;
                                                   getArraySize(m_params.src.image) // uint32_t arraySize;
                                               }};

    const VkBufferMemoryBarrier bufferBarrier = {
        VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER, // VkStructureType sType;
        DE_NULL,                                 // const void* pNext;
        VK_ACCESS_TRANSFER_WRITE_BIT,            // VkAccessFlags srcAccessMask;
        VK_ACCESS_HOST_READ_BIT,                 // VkAccessFlags dstAccessMask;
        VK_QUEUE_FAMILY_IGNORED,                 // uint32_t srcQueueFamilyIndex;
        VK_QUEUE_FAMILY_IGNORED,                 // uint32_t dstQueueFamilyIndex;
        *m_destination,                          // VkBuffer buffer;
        0u,                                      // VkDeviceSize offset;
        m_bufferSize                             // VkDeviceSize size;
    };

    // Copy from image to buffer
    std::vector<VkBufferImageCopy> bufferImageCopies;
    std::vector<VkBufferImageCopy2KHR> bufferImageCopies2KHR;
    for (uint32_t i = 0; i < m_params.regions.size(); i++)
    {
        if (!(m_params.extensionFlags & COPY_COMMANDS_2))
        {
            bufferImageCopies.push_back(m_params.regions[i].bufferImageCopy);
        }
        else
        {
            DE_ASSERT(m_params.extensionFlags & COPY_COMMANDS_2);
            bufferImageCopies2KHR.push_back(
                convertvkBufferImageCopyTovkBufferImageCopy2KHR(m_params.regions[i].bufferImageCopy));
        }
    }

    beginCommandBuffer(vk, commandBuffer);
    vk.cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                          (VkDependencyFlags)0, 0, (const VkMemoryBarrier *)DE_NULL, 0,
                          (const VkBufferMemoryBarrier *)DE_NULL, 1, &imageBarrier);

    if (!(m_params.extensionFlags & COPY_COMMANDS_2))
    {
        vk.cmdCopyImageToBuffer(commandBuffer, m_source.get(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                m_destination.get(), (uint32_t)m_params.regions.size(), &bufferImageCopies[0]);
    }
    else
    {
        DE_ASSERT(m_params.extensionFlags & COPY_COMMANDS_2);
        const VkCopyImageToBufferInfo2KHR copyImageToBufferInfo2KHR = {
            VK_STRUCTURE_TYPE_COPY_IMAGE_TO_BUFFER_INFO_2_KHR, // VkStructureType sType;
            DE_NULL,                                           // const void* pNext;
            m_source.get(),                                    // VkImage srcImage;
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,              // VkImageLayout srcImageLayout;
            m_destination.get(),                               // VkBuffer dstBuffer;
            (uint32_t)m_params.regions.size(),                 // uint32_t regionCount;
            &bufferImageCopies2KHR[0]                          // const VkBufferImageCopy2KHR* pRegions;
        };

        vk.cmdCopyImageToBuffer2(commandBuffer, &copyImageToBufferInfo2KHR);
    }

    vk.cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                          (VkDependencyFlags)0, 0, (const VkMemoryBarrier *)DE_NULL, 1, &bufferBarrier, 0,
                          (const VkImageMemoryBarrier *)DE_NULL);
    endCommandBuffer(vk, commandBuffer);

    if (m_params.useSparseBinding)
    {
        const VkPipelineStageFlags stageBits[] = {VK_PIPELINE_STAGE_TRANSFER_BIT};
        submitCommandsAndWait(vk, vkDevice, queue, commandBuffer, false, 1u, 1u, &*m_sparseSemaphore, stageBits);
    }
    else
    {
        submitCommandsAndWait(vk, vkDevice, queue, commandBuffer);
    }

    m_context.resetCommandPoolForVKSC(vkDevice, commandPool);

    // Read buffer data
    de::MovePtr<tcu::TextureLevel> resultLevel(
        new tcu::TextureLevel(m_textureFormat, (int)m_params.dst.buffer.size, 1));
    invalidateAlloc(vk, vkDevice, *m_destinationBufferAlloc);
    tcu::copy(*resultLevel, tcu::ConstPixelBufferAccess(resultLevel->getFormat(), resultLevel->getSize(),
                                                        m_destinationBufferAlloc->getHostPtr()));

    return checkTestResult(resultLevel->getAccess());
}

class CopyImageToBufferTestCase : public vkt::TestCase
{
public:
    CopyImageToBufferTestCase(tcu::TestContext &testCtx, const std::string &name, const TestParams params)
        : vkt::TestCase(testCtx, name)
        , m_params(params)
    {
    }

    virtual TestInstance *createInstance(Context &context) const
    {
        return new CopyImageToBuffer(context, m_params);
    }

    virtual void checkSupport(Context &context) const
    {
        checkExtensionSupport(context, m_params.extensionFlags);
        // Check queue transfer granularity requirements
        if (m_params.queueSelection == QueueSelectionOptions::TransferOnly)
        {
            checkTransferQueueGranularity(context, m_params.src.image.extent, m_params.src.image.imageType);
            for (const auto &region : m_params.regions)
            {
                checkTransferQueueGranularity(context, region.bufferImageCopy.imageExtent,
                                              m_params.src.image.imageType);
            }
        }
    }

private:
    TestParams m_params;
};

void CopyImageToBuffer::copyRegionToTextureLevel(tcu::ConstPixelBufferAccess src, tcu::PixelBufferAccess dst,
                                                 CopyRegion region, uint32_t mipLevel)
{
    DE_UNREF(mipLevel);

    uint32_t rowLength = region.bufferImageCopy.bufferRowLength;
    if (!rowLength)
        rowLength = region.bufferImageCopy.imageExtent.width;

    uint32_t imageHeight = region.bufferImageCopy.bufferImageHeight;
    if (!imageHeight)
        imageHeight = region.bufferImageCopy.imageExtent.height;

    const int texelSize           = src.getFormat().getPixelSize();
    const VkExtent3D extent       = region.bufferImageCopy.imageExtent;
    const VkOffset3D srcOffset    = region.bufferImageCopy.imageOffset;
    const int texelOffset         = (int)region.bufferImageCopy.bufferOffset / texelSize;
    const uint32_t baseArrayLayer = region.bufferImageCopy.imageSubresource.baseArrayLayer;

    for (uint32_t z = 0; z < extent.depth; z++)
    {
        for (uint32_t y = 0; y < extent.height; y++)
        {
            int texelIndex = texelOffset + (z * imageHeight + y) * rowLength;
            const tcu::ConstPixelBufferAccess srcSubRegion =
                tcu::getSubregion(src, srcOffset.x, srcOffset.y + y, srcOffset.z + z + baseArrayLayer,
                                  region.bufferImageCopy.imageExtent.width, 1, 1);
            const tcu::PixelBufferAccess dstSubRegion =
                tcu::getSubregion(dst, texelIndex, 0, region.bufferImageCopy.imageExtent.width, 1);
            tcu::copy(dstSubRegion, srcSubRegion);
        }
    }
}

// Copy levels from compressed mipmap images into a buffer.
class CopyCompressedImageToBuffer final : public CopiesAndBlittingTestInstance
{
public:
    CopyCompressedImageToBuffer(Context &context, TestParams testParams);

    virtual tcu::TestStatus iterate(void) override;

private:
    virtual void copyRegionToTextureLevel(tcu::ConstPixelBufferAccess, tcu::PixelBufferAccess, CopyRegion,
                                          uint32_t) override
    {
        TCU_THROW(InternalError, "copyRegionToTextureLevel not implemented for CopyCompressedImageToBuffer");
    }

    // Contains a randomly generated compressed texture pyramid.
    using TestTexture2DSp = de::SharedPtr<pipeline::TestTexture2DArray>;
    TestTexture2DSp m_texture;
    de::MovePtr<ImageWithMemory> m_source;
    de::MovePtr<BufferWithMemory> m_sourceBuffer;
    de::MovePtr<BufferWithMemory> m_destination;
};

CopyCompressedImageToBuffer::CopyCompressedImageToBuffer(Context &context, TestParams testParams)
    : CopiesAndBlittingTestInstance(context, testParams)
    , m_texture(TestTexture2DSp(new pipeline::TestTexture2DArray(
          mapVkCompressedFormat(testParams.src.image.format), testParams.src.image.extent.width,
          testParams.src.image.extent.height, testParams.arrayLayers)))
{
}

tcu::TestStatus CopyCompressedImageToBuffer::iterate(void)
{
    const DeviceInterface &vk        = m_context.getDeviceInterface();
    const VkDevice vkDevice          = m_device;
    Allocator &memAlloc              = *m_allocator;
    const ImageParms &srcImageParams = m_params.src.image;

    VkQueue queue                               = VK_NULL_HANDLE;
    VkCommandBuffer commandBuffer               = VK_NULL_HANDLE;
    VkCommandPool commandPool                   = VK_NULL_HANDLE;
    std::tie(queue, commandBuffer, commandPool) = activeExecutionCtx();

    // Create source image, containing all the mip levels.
    {
        const VkImageCreateInfo sourceImageParams = {
            VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,                               // VkStructureType sType;
            DE_NULL,                                                           // const void* pNext;
            getCreateFlags(m_params.src.image),                                // VkImageCreateFlags flags;
            m_params.src.image.imageType,                                      // VkImageType imageType;
            m_params.src.image.format,                                         // VkFormat format;
            m_params.src.image.extent,                                         // VkExtent3D extent;
            (uint32_t)m_texture->getNumLevels(),                               // uint32_t mipLevels;
            m_params.arrayLayers,                                              // uint32_t arraySize;
            VK_SAMPLE_COUNT_1_BIT,                                             // uint32_t samples;
            VK_IMAGE_TILING_OPTIMAL,                                           // VkImageTiling tiling;
            VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, // VkImageUsageFlags usage;
            m_queueFamilyIndices.size() > 1 ? VK_SHARING_MODE_CONCURRENT :
                                              VK_SHARING_MODE_EXCLUSIVE, // VkSharingMode sharingMode;
            (uint32_t)m_queueFamilyIndices.size(),                       // uint32_t queueFamilyIndexCount;
            m_queueFamilyIndices.data(),                                 // const uint32_t* pQueueFamilyIndices;
            VK_IMAGE_LAYOUT_UNDEFINED,                                   // VkImageLayout initialLayout;
        };

        m_source = de::MovePtr<ImageWithMemory>(
            new ImageWithMemory(vk, vkDevice, memAlloc, sourceImageParams, vk::MemoryRequirement::Any));
    }

    // Upload the compressed image.
    // FIXME: This could be a utility.
    //    pipeline::uploadTestTexture(vk, vkDevice, queue, queueFamilyIndex, memAlloc, *m_texture, m_source->get(), vk::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    // Does not allow using an external command pool, the utilities there could fruitfully be generalised.
    m_sourceBuffer = de::MovePtr<BufferWithMemory>(new BufferWithMemory(
        vk, vkDevice, memAlloc, makeBufferCreateInfo(m_texture->getCompressedSize(), VK_BUFFER_USAGE_TRANSFER_SRC_BIT),
        vk::MemoryRequirement::HostVisible));
    m_texture->write(reinterpret_cast<uint8_t *>(m_sourceBuffer->getAllocation().getHostPtr()));
    flushAlloc(vk, vkDevice, m_sourceBuffer->getAllocation());
    std::vector<VkBufferImageCopy> copyRegions = m_texture->getBufferCopyRegions();
    copyBufferToImage(vk, vkDevice, queue, activeQueueFamilyIndex(), m_sourceBuffer->get(),
                      m_texture->getCompressedSize(), copyRegions, nullptr, VK_IMAGE_ASPECT_COLOR_BIT,
                      m_texture->getNumLevels(), m_texture->getArraySize(), m_source->get(),
                      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_READ_BIT,
                      &commandPool, 0);

    // VKSC requires static allocation, so allocate a large enough buffer for each individual mip level of
    // the compressed source image, rather than creating a corresponding buffer for each level in the loop
    // below.
    auto level0BuferSize = m_texture->getCompressedLevel(0, 0).getDataSize();
    m_destination        = de::MovePtr<BufferWithMemory>(new BufferWithMemory(
        vk, vkDevice, memAlloc, makeBufferCreateInfo(level0BuferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT),
        MemoryRequirement::HostVisible));

    // Copy each miplevel of the uploaded image into a buffer, and
    // check the buffer matches the appropriate test texture level.
    for (uint32_t mipLevelToCheckIdx = 0; mipLevelToCheckIdx < (uint32_t)m_texture->getNumLevels();
         mipLevelToCheckIdx++)
        for (uint32_t arrayLayerToCheckIdx = 0; arrayLayerToCheckIdx < (uint32_t)m_texture->getArraySize();
             arrayLayerToCheckIdx++)
        {
            const tcu::CompressedTexture compressedMipLevelToCheck =
                m_texture->getCompressedLevel(mipLevelToCheckIdx, arrayLayerToCheckIdx);
            uint32_t bufferSize = compressedMipLevelToCheck.getDataSize();

            // Clear the buffer to zero before copying into it as a precaution.
            deMemset(m_destination->getAllocation().getHostPtr(), 0, bufferSize);
            flushAlloc(vk, vkDevice, m_destination->getAllocation());

            // Barrier to get the source image's selected mip-level / layer in the right format for transfer.
            const auto imageBarrier = makeImageMemoryBarrier(
                VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, m_source->get(),
                {
                    // VkImageSubresourceRange subresourceRange;
                    VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
                    mipLevelToCheckIdx,        // uint32_t baseMipLevel;
                    1u,                        // uint32_t mipLevels;
                    arrayLayerToCheckIdx,      // uint32_t baseArraySlice;
                    1u,                        // uint32_t arraySize;
                });

            // Barrier to wait for the transfer from image to buffer to complete.
            const auto bufferBarrier = makeBufferMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT,
                                                               m_destination->get(), 0, bufferSize);

            // Copy from image to buffer
            VkBufferImageCopy copyRegion;
            copyRegion = makeBufferImageCopy(
                mipLevelExtents(srcImageParams.extent, mipLevelToCheckIdx),
                makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, mipLevelToCheckIdx, arrayLayerToCheckIdx, 1));

            VkBufferImageCopy bufferImageCopy;
            VkBufferImageCopy2KHR bufferImageCopy2KHR;
            if (!(m_params.extensionFlags & COPY_COMMANDS_2))
            {
                bufferImageCopy = copyRegion;
            }
            else
            {
                DE_ASSERT(m_params.extensionFlags & COPY_COMMANDS_2);
                bufferImageCopy2KHR = convertvkBufferImageCopyTovkBufferImageCopy2KHR(copyRegion);
            }

            beginCommandBuffer(vk, commandBuffer);
            // Transition the selected miplevel to the right format for the transfer.
            vk.cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                  (VkDependencyFlags)0, 0, (const VkMemoryBarrier *)DE_NULL, 0,
                                  (const VkBufferMemoryBarrier *)DE_NULL, 1, &imageBarrier);

            // Copy the mip level to the buffer.
            if (!(m_params.extensionFlags & COPY_COMMANDS_2))
            {
                vk.cmdCopyImageToBuffer(commandBuffer, m_source->get(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                        m_destination->get(), 1u, &bufferImageCopy);
            }
            else
            {
                DE_ASSERT(m_params.extensionFlags & COPY_COMMANDS_2);
                const VkCopyImageToBufferInfo2KHR copyImageToBufferInfo2KHR = {
                    VK_STRUCTURE_TYPE_COPY_IMAGE_TO_BUFFER_INFO_2_KHR, // VkStructureType sType;
                    DE_NULL,                                           // const void* pNext;
                    m_source->get(),                                   // VkImage srcImage;
                    VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,              // VkImageLayout srcImageLayout;
                    m_destination->get(),                              // VkBuffer dstBuffer;
                    1u,                                                // uint32_t regionCount;
                    &bufferImageCopy2KHR                               // const VkBufferImageCopy2KHR* pRegions;
                };

                vk.cmdCopyImageToBuffer2(commandBuffer, &copyImageToBufferInfo2KHR);
            }

            // Prepare to read from the host visible barrier.
            vk.cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                                  (VkDependencyFlags)0, 0, (const VkMemoryBarrier *)DE_NULL, 1, &bufferBarrier, 0,
                                  (const VkImageMemoryBarrier *)DE_NULL);
            endCommandBuffer(vk, commandBuffer);

            submitCommandsAndWait(vk, vkDevice, queue, commandBuffer);
            m_context.resetCommandPoolForVKSC(vkDevice, commandPool);

            invalidateAlloc(vk, vkDevice, m_destination->getAllocation());
            // Read and compare buffer data.
            const uint8_t *referenceData = (uint8_t *)compressedMipLevelToCheck.getData();
            const uint8_t *resultData    = (uint8_t *)m_destination->getAllocation().getHostPtr();
            int result                   = deMemCmp(referenceData, resultData, bufferSize);
            if (result != 0)
            {
                std::ostringstream msg;
                msg << "Incorrect data retrieved for mip level " << mipLevelToCheckIdx << ", layer "
                    << arrayLayerToCheckIdx << " - extents (" << compressedMipLevelToCheck.getWidth() << ", "
                    << compressedMipLevelToCheck.getHeight() << ")";
                return tcu::TestStatus::fail(msg.str());
            }
        }

    return tcu::TestStatus::pass("OK");
}

class CopyCompressedImageToBufferTestCase : public vkt::TestCase
{
public:
    CopyCompressedImageToBufferTestCase(tcu::TestContext &testCtx, const std::string &name, const TestParams params)
        : vkt::TestCase(testCtx, name)
        , m_params(params)
    {
    }

    virtual TestInstance *createInstance(Context &context) const
    {
        return new CopyCompressedImageToBuffer(context, m_params);
    }
    virtual void checkSupport(Context &context) const;

private:
    TestParams m_params;
};

void CopyCompressedImageToBufferTestCase::checkSupport(Context &context) const
{
    DE_ASSERT(m_params.src.image.tiling == VK_IMAGE_TILING_OPTIMAL);
    DE_ASSERT(m_params.src.image.imageType == vk::VK_IMAGE_TYPE_2D);

    checkExtensionSupport(context, m_params.extensionFlags);

    VkFormatProperties formatProps;
    context.getInstanceInterface().getPhysicalDeviceFormatProperties(context.getPhysicalDevice(),
                                                                     m_params.src.image.format, &formatProps);

    VkImageFormatProperties imageFormatProperties;

    const auto &instance = context.getInstanceInterface();
    if (instance.getPhysicalDeviceImageFormatProperties(context.getPhysicalDevice(), m_params.src.image.format,
                                                        m_params.src.image.imageType, VK_IMAGE_TILING_OPTIMAL,
                                                        VK_IMAGE_USAGE_TRANSFER_SRC_BIT, 0,
                                                        &imageFormatProperties) == VK_ERROR_FORMAT_NOT_SUPPORTED)
    {
        TCU_THROW(NotSupportedError, "Format not supported");
    }

    if (!(formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_TRANSFER_SRC_BIT))
        TCU_THROW(NotSupportedError, "TRANSFER_SRC is not supported on this image type");
}

// Copy from buffer to image.

class CopyBufferToImage : public CopiesAndBlittingTestInstance
{
public:
    CopyBufferToImage(Context &context, TestParams testParams);
    virtual tcu::TestStatus iterate(void);

private:
    virtual void copyRegionToTextureLevel(tcu::ConstPixelBufferAccess src, tcu::PixelBufferAccess dst,
                                          CopyRegion region, uint32_t mipLevel = 0u);

    tcu::TextureFormat m_textureFormat;
    VkDeviceSize m_bufferSize;

    Move<VkBuffer> m_source;
    de::MovePtr<Allocation> m_sourceBufferAlloc;
    Move<VkImage> m_destination;
    de::MovePtr<Allocation> m_destinationImageAlloc;
    std::vector<de::SharedPtr<Allocation>> m_sparseAllocations;
    Move<VkSemaphore> m_sparseSemaphore;
};

CopyBufferToImage::CopyBufferToImage(Context &context, TestParams testParams)
    : CopiesAndBlittingTestInstance(context, testParams)
    , m_textureFormat(mapVkFormat(testParams.dst.image.format))
    , m_bufferSize(m_params.src.buffer.size * tcu::getPixelSize(m_textureFormat))
{
    const InstanceInterface &vki        = context.getInstanceInterface();
    const DeviceInterface &vk           = context.getDeviceInterface();
    const VkPhysicalDevice vkPhysDevice = context.getPhysicalDevice();

    // Create source buffer
    {
        const VkBufferCreateInfo sourceBufferParams = {
            VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, // VkStructureType sType;
            DE_NULL,                              // const void* pNext;
            0u,                                   // VkBufferCreateFlags flags;
            m_bufferSize,                         // VkDeviceSize size;
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,     // VkBufferUsageFlags usage;
            VK_SHARING_MODE_EXCLUSIVE,            // VkSharingMode sharingMode;
            0u,                                   // uint32_t queueFamilyIndexCount;
            (const uint32_t *)DE_NULL,            // const uint32_t* pQueueFamilyIndices;
        };

        m_source            = createBuffer(vk, m_device, &sourceBufferParams);
        m_sourceBufferAlloc = allocateBuffer(vki, vk, vkPhysDevice, m_device, *m_source, MemoryRequirement::HostVisible,
                                             *m_allocator, m_params.allocationKind);
        VK_CHECK(vk.bindBufferMemory(m_device, *m_source, m_sourceBufferAlloc->getMemory(),
                                     m_sourceBufferAlloc->getOffset()));
    }

    // Create destination image
    {
        VkImageCreateInfo destinationImageParams = {
            VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,                               // VkStructureType sType;
            DE_NULL,                                                           // const void* pNext;
            getCreateFlags(m_params.dst.image),                                // VkImageCreateFlags flags;
            m_params.dst.image.imageType,                                      // VkImageType imageType;
            m_params.dst.image.format,                                         // VkFormat format;
            getExtent3D(m_params.dst.image),                                   // VkExtent3D extent;
            1u,                                                                // uint32_t mipLevels;
            getArraySize(m_params.dst.image),                                  // uint32_t arraySize;
            VK_SAMPLE_COUNT_1_BIT,                                             // uint32_t samples;
            VK_IMAGE_TILING_OPTIMAL,                                           // VkImageTiling tiling;
            VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, // VkImageUsageFlags usage;
            VK_SHARING_MODE_EXCLUSIVE,                                         // VkSharingMode sharingMode;
            0u,                                                                // uint32_t queueFamilyIndexCount;
            (const uint32_t *)DE_NULL,                                         // const uint32_t* pQueueFamilyIndices;
            VK_IMAGE_LAYOUT_UNDEFINED,                                         // VkImageLayout initialLayout;
        };

#ifndef CTS_USES_VULKANSC
        if (!testParams.useSparseBinding)
        {
#endif
            m_destination           = createImage(vk, m_device, &destinationImageParams);
            m_destinationImageAlloc = allocateImage(vki, vk, vkPhysDevice, m_device, *m_destination,
                                                    MemoryRequirement::Any, *m_allocator, m_params.allocationKind, 0u);
            VK_CHECK(vk.bindImageMemory(m_device, *m_destination, m_destinationImageAlloc->getMemory(),
                                        m_destinationImageAlloc->getOffset()));
#ifndef CTS_USES_VULKANSC
        }
        else
        {
            destinationImageParams.flags |=
                (vk::VK_IMAGE_CREATE_SPARSE_BINDING_BIT | vk::VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT);
            vk::VkImageFormatProperties imageFormatProperties;
            if (vki.getPhysicalDeviceImageFormatProperties(
                    vkPhysDevice, destinationImageParams.format, destinationImageParams.imageType,
                    destinationImageParams.tiling, destinationImageParams.usage, destinationImageParams.flags,
                    &imageFormatProperties) == vk::VK_ERROR_FORMAT_NOT_SUPPORTED)
            {
                TCU_THROW(NotSupportedError, "Image format not supported");
            }
            m_destination     = createImage(vk, m_device, &destinationImageParams);
            m_sparseSemaphore = createSemaphore(vk, m_device);
            allocateAndBindSparseImage(vk, m_device, vkPhysDevice, vki, destinationImageParams, m_sparseSemaphore.get(),
                                       context.getSparseQueue(), *m_allocator, m_sparseAllocations,
                                       mapVkFormat(destinationImageParams.format), m_destination.get());
        }
#endif
    }
}

tcu::TestStatus CopyBufferToImage::iterate(void)
{
    m_sourceTextureLevel =
        de::MovePtr<tcu::TextureLevel>(new tcu::TextureLevel(m_textureFormat, (int)m_params.src.buffer.size, 1));
    generateBuffer(m_sourceTextureLevel->getAccess(), (int)m_params.src.buffer.size, 1, 1,
                   m_params.src.buffer.fillMode);
    m_destinationTextureLevel = de::MovePtr<tcu::TextureLevel>(
        new tcu::TextureLevel(m_textureFormat, m_params.dst.image.extent.width, m_params.dst.image.extent.height,
                              m_params.dst.image.extent.depth));

    generateBuffer(m_destinationTextureLevel->getAccess(), m_params.dst.image.extent.width,
                   m_params.dst.image.extent.height, m_params.dst.image.extent.depth, m_params.dst.image.fillMode);

    generateExpectedResult();

    uploadBuffer(m_sourceTextureLevel->getAccess(), *m_sourceBufferAlloc);
    uploadImage(m_destinationTextureLevel->getAccess(), *m_destination, m_params.dst.image);

    const DeviceInterface &vk                   = m_context.getDeviceInterface();
    const VkDevice vkDevice                     = m_device;
    VkQueue queue                               = VK_NULL_HANDLE;
    VkCommandBuffer commandBuffer               = VK_NULL_HANDLE;
    VkCommandPool commandPool                   = VK_NULL_HANDLE;
    std::tie(queue, commandBuffer, commandPool) = activeExecutionCtx();

    const VkImageMemoryBarrier imageBarrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, // VkStructureType sType;
                                               DE_NULL,                                // const void* pNext;
                                               VK_ACCESS_TRANSFER_WRITE_BIT,           // VkAccessFlags srcAccessMask;
                                               VK_ACCESS_TRANSFER_WRITE_BIT,           // VkAccessFlags dstAccessMask;
                                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,   // VkImageLayout oldLayout;
                                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,   // VkImageLayout newLayout;
                                               VK_QUEUE_FAMILY_IGNORED,                // uint32_t srcQueueFamilyIndex;
                                               VK_QUEUE_FAMILY_IGNORED,                // uint32_t dstQueueFamilyIndex;
                                               *m_destination,                         // VkImage image;
                                               {
                                                   // VkImageSubresourceRange subresourceRange;
                                                   getAspectFlags(m_textureFormat), // VkImageAspectFlags aspectMask;
                                                   0u,                              // uint32_t baseMipLevel;
                                                   1u,                              // uint32_t mipLevels;
                                                   0u,                              // uint32_t baseArraySlice;
                                                   getArraySize(m_params.dst.image) // uint32_t arraySize;
                                               }};

    // Copy from buffer to image
    std::vector<VkBufferImageCopy> bufferImageCopies;
    std::vector<VkBufferImageCopy2KHR> bufferImageCopies2KHR;
    for (uint32_t i = 0; i < m_params.regions.size(); i++)
    {
        if (!(m_params.extensionFlags & COPY_COMMANDS_2))
        {
            bufferImageCopies.push_back(m_params.regions[i].bufferImageCopy);
        }
        else
        {
            DE_ASSERT(m_params.extensionFlags & COPY_COMMANDS_2);
            bufferImageCopies2KHR.push_back(
                convertvkBufferImageCopyTovkBufferImageCopy2KHR(m_params.regions[i].bufferImageCopy));
        }
    }

    beginCommandBuffer(vk, commandBuffer);
    vk.cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                          (VkDependencyFlags)0, 0, (const VkMemoryBarrier *)DE_NULL, 0,
                          (const VkBufferMemoryBarrier *)DE_NULL, 1, &imageBarrier);

    if (!(m_params.extensionFlags & COPY_COMMANDS_2))
    {
        vk.cmdCopyBufferToImage(commandBuffer, m_source.get(), m_destination.get(),
                                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, (uint32_t)m_params.regions.size(),
                                bufferImageCopies.data());
    }
    else
    {
        DE_ASSERT(m_params.extensionFlags & COPY_COMMANDS_2);
        const VkCopyBufferToImageInfo2KHR copyBufferToImageInfo2KHR = {
            VK_STRUCTURE_TYPE_COPY_BUFFER_TO_IMAGE_INFO_2_KHR, // VkStructureType sType;
            DE_NULL,                                           // const void* pNext;
            m_source.get(),                                    // VkBuffer srcBuffer;
            m_destination.get(),                               // VkImage dstImage;
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,              // VkImageLayout dstImageLayout;
            (uint32_t)m_params.regions.size(),                 // uint32_t regionCount;
            bufferImageCopies2KHR.data()                       // const VkBufferImageCopy2KHR* pRegions;
        };

        vk.cmdCopyBufferToImage2(commandBuffer, &copyBufferToImageInfo2KHR);
    }

    endCommandBuffer(vk, commandBuffer);

    if (m_params.useSparseBinding)
    {
        const VkPipelineStageFlags stageBits[] = {VK_PIPELINE_STAGE_TRANSFER_BIT};
        submitCommandsAndWait(vk, vkDevice, queue, commandBuffer, false, 1u, 1u, &*m_sparseSemaphore, stageBits);
    }
    else
    {
        submitCommandsAndWait(vk, vkDevice, queue, commandBuffer);
    }

    m_context.resetCommandPoolForVKSC(vkDevice, commandPool);

    de::MovePtr<tcu::TextureLevel> resultLevel = readImage(*m_destination, m_params.dst.image);
    return checkTestResult(resultLevel->getAccess());
}

class CopyBufferToImageTestCase : public vkt::TestCase
{
public:
    CopyBufferToImageTestCase(tcu::TestContext &testCtx, const std::string &name, const TestParams params)
        : vkt::TestCase(testCtx, name)
        , m_params(params)
    {
    }

    virtual ~CopyBufferToImageTestCase(void)
    {
    }

    virtual TestInstance *createInstance(Context &context) const
    {
        return new CopyBufferToImage(context, m_params);
    }

    virtual void checkSupport(Context &context) const
    {
        checkExtensionSupport(context, m_params.extensionFlags);
        // Check queue transfer granularity requirements
        if (m_params.queueSelection == QueueSelectionOptions::TransferOnly)
        {
            checkTransferQueueGranularity(context, m_params.dst.image.extent, m_params.dst.image.imageType);
            for (const auto &region : m_params.regions)
            {
                checkTransferQueueGranularity(context, region.bufferImageCopy.imageExtent,
                                              m_params.dst.image.imageType);
            }
        }
    }

private:
    TestParams m_params;
};

void CopyBufferToImage::copyRegionToTextureLevel(tcu::ConstPixelBufferAccess src, tcu::PixelBufferAccess dst,
                                                 CopyRegion region, uint32_t mipLevel)
{
    DE_UNREF(mipLevel);

    uint32_t rowLength = region.bufferImageCopy.bufferRowLength;
    if (!rowLength)
        rowLength = region.bufferImageCopy.imageExtent.width;

    uint32_t imageHeight = region.bufferImageCopy.bufferImageHeight;
    if (!imageHeight)
        imageHeight = region.bufferImageCopy.imageExtent.height;

    const int texelSize           = dst.getFormat().getPixelSize();
    const VkExtent3D extent       = region.bufferImageCopy.imageExtent;
    const VkOffset3D dstOffset    = region.bufferImageCopy.imageOffset;
    const int texelOffset         = (int)region.bufferImageCopy.bufferOffset / texelSize;
    const uint32_t baseArrayLayer = region.bufferImageCopy.imageSubresource.baseArrayLayer;

    for (uint32_t z = 0; z < extent.depth; z++)
    {
        for (uint32_t y = 0; y < extent.height; y++)
        {
            int texelIndex = texelOffset + (z * imageHeight + y) * rowLength;
            const tcu::ConstPixelBufferAccess srcSubRegion =
                tcu::getSubregion(src, texelIndex, 0, region.bufferImageCopy.imageExtent.width, 1);
            const tcu::PixelBufferAccess dstSubRegion =
                tcu::getSubregion(dst, dstOffset.x, dstOffset.y + y, dstOffset.z + z + baseArrayLayer,
                                  region.bufferImageCopy.imageExtent.width, 1, 1);
            tcu::copy(dstSubRegion, srcSubRegion);
        }
    }
}

class CopyBufferToDepthStencil : public CopiesAndBlittingTestInstance
{
public:
    CopyBufferToDepthStencil(Context &context, TestParams testParams);
    virtual tcu::TestStatus iterate(void);

private:
    virtual void copyRegionToTextureLevel(tcu::ConstPixelBufferAccess src, tcu::PixelBufferAccess dst,
                                          CopyRegion region, uint32_t mipLevel = 0u);

    tcu::TextureFormat m_textureFormat;
    VkDeviceSize m_bufferSize;

    Move<VkBuffer> m_source;
    de::MovePtr<Allocation> m_sourceBufferAlloc;
    Move<VkImage> m_destination;
    de::MovePtr<Allocation> m_destinationImageAlloc;
    std::vector<de::SharedPtr<Allocation>> m_sparseAllocations;
    Move<VkSemaphore> m_sparseSemaphore;
};

void CopyBufferToDepthStencil::copyRegionToTextureLevel(tcu::ConstPixelBufferAccess src, tcu::PixelBufferAccess dst,
                                                        CopyRegion region, uint32_t mipLevel)
{
    DE_UNREF(mipLevel);

    uint32_t rowLength = region.bufferImageCopy.bufferRowLength;
    if (!rowLength)
        rowLength = region.bufferImageCopy.imageExtent.width;

    uint32_t imageHeight = region.bufferImageCopy.bufferImageHeight;
    if (!imageHeight)
        imageHeight = region.bufferImageCopy.imageExtent.height;

    const int texelSize        = dst.getFormat().getPixelSize();
    const VkExtent3D extent    = region.bufferImageCopy.imageExtent;
    const VkOffset3D dstOffset = region.bufferImageCopy.imageOffset;
    const int texelOffset      = (int)region.bufferImageCopy.bufferOffset / texelSize;

    for (uint32_t z = 0; z < extent.depth; z++)
    {
        for (uint32_t y = 0; y < extent.height; y++)
        {
            int texelIndex = texelOffset + (z * imageHeight + y) * rowLength;
            const tcu::ConstPixelBufferAccess srcSubRegion =
                tcu::getSubregion(src, texelIndex, 0, region.bufferImageCopy.imageExtent.width, 1);
            const tcu::PixelBufferAccess dstSubRegion = tcu::getSubregion(
                dst, dstOffset.x, dstOffset.y + y, dstOffset.z + z, region.bufferImageCopy.imageExtent.width, 1, 1);

            if (region.bufferImageCopy.imageSubresource.aspectMask == VK_IMAGE_ASPECT_DEPTH_BIT)
            {
                tcu::copy(dstSubRegion, tcu::getEffectiveDepthStencilAccess(srcSubRegion, tcu::Sampler::MODE_DEPTH),
                          false);
            }
            else
            {
                tcu::copy(dstSubRegion, tcu::getEffectiveDepthStencilAccess(srcSubRegion, tcu::Sampler::MODE_STENCIL),
                          false);
            }
        }
    }
}

bool isSupportedDepthStencilFormat(const InstanceInterface &vki, const VkPhysicalDevice physDevice,
                                   const VkFormat format)
{
    VkFormatProperties formatProps;
    vki.getPhysicalDeviceFormatProperties(physDevice, format, &formatProps);
    return (formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0;
}

CopyBufferToDepthStencil::CopyBufferToDepthStencil(Context &context, TestParams testParams)
    : CopiesAndBlittingTestInstance(context, testParams)
    , m_textureFormat(mapVkFormat(testParams.dst.image.format))
    , m_bufferSize(0)
{
    const InstanceInterface &vki        = context.getInstanceInterface();
    const DeviceInterface &vk           = context.getDeviceInterface();
    const VkPhysicalDevice vkPhysDevice = context.getPhysicalDevice();
    const VkDevice vkDevice             = m_device;
    Allocator &memAlloc                 = context.getDefaultAllocator();
    const bool hasDepth                 = tcu::hasDepthComponent(mapVkFormat(m_params.dst.image.format).order);
    const bool hasStencil               = tcu::hasStencilComponent(mapVkFormat(m_params.dst.image.format).order);

    if (!isSupportedDepthStencilFormat(vki, vkPhysDevice, testParams.dst.image.format))
    {
        TCU_THROW(NotSupportedError, "Image format not supported.");
    }

    if (hasDepth)
    {
        glw::GLuint texelSize = m_textureFormat.getPixelSize();
        if (texelSize > sizeof(float))
        {
            // We must have D32F_S8 format, depth must be packed so we only need
            // to allocate space for the D32F part. Stencil will be separate
            texelSize = sizeof(float);
        }
        m_bufferSize += static_cast<VkDeviceSize>(m_params.dst.image.extent.width) *
                        static_cast<VkDeviceSize>(m_params.dst.image.extent.height) *
                        static_cast<VkDeviceSize>(texelSize);
    }
    if (hasStencil)
    {
        // Stencil is always 8bits and packed.
        m_bufferSize += static_cast<VkDeviceSize>(m_params.dst.image.extent.width) *
                        static_cast<VkDeviceSize>(m_params.dst.image.extent.height);
    }

    // Create source buffer, this is where the depth & stencil data will go that's used by test's regions.
    {
        const VkBufferCreateInfo sourceBufferParams = {
            VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, // VkStructureType sType;
            DE_NULL,                              // const void* pNext;
            0u,                                   // VkBufferCreateFlags flags;
            m_bufferSize,                         // VkDeviceSize size;
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,     // VkBufferUsageFlags usage;
            VK_SHARING_MODE_EXCLUSIVE,            // VkSharingMode sharingMode;
            0u,                                   // uint32_t queueFamilyIndexCount;
            (const uint32_t *)DE_NULL,            // const uint32_t* pQueueFamilyIndices;
        };

        m_source            = createBuffer(vk, vkDevice, &sourceBufferParams);
        m_sourceBufferAlloc = allocateBuffer(vki, vk, vkPhysDevice, vkDevice, *m_source, MemoryRequirement::HostVisible,
                                             memAlloc, m_params.allocationKind);
        VK_CHECK(vk.bindBufferMemory(vkDevice, *m_source, m_sourceBufferAlloc->getMemory(),
                                     m_sourceBufferAlloc->getOffset()));
    }

    // Create destination image
    {
        VkImageCreateInfo destinationImageParams = {
            VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,                               // VkStructureType sType;
            DE_NULL,                                                           // const void* pNext;
            getCreateFlags(m_params.dst.image),                                // VkImageCreateFlags flags;
            m_params.dst.image.imageType,                                      // VkImageType imageType;
            m_params.dst.image.format,                                         // VkFormat format;
            getExtent3D(m_params.dst.image),                                   // VkExtent3D extent;
            1u,                                                                // uint32_t mipLevels;
            getArraySize(m_params.dst.image),                                  // uint32_t arraySize;
            VK_SAMPLE_COUNT_1_BIT,                                             // uint32_t samples;
            VK_IMAGE_TILING_OPTIMAL,                                           // VkImageTiling tiling;
            VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, // VkImageUsageFlags usage;
            VK_SHARING_MODE_EXCLUSIVE,                                         // VkSharingMode sharingMode;
            0u,                                                                // uint32_t queueFamilyIndexCount;
            (const uint32_t *)DE_NULL,                                         // const uint32_t* pQueueFamilyIndices;
            VK_IMAGE_LAYOUT_UNDEFINED,                                         // VkImageLayout initialLayout;
        };

#ifndef CTS_USES_VULKANSC
        if (!testParams.useSparseBinding)
        {
#endif
            m_destination           = createImage(vk, m_device, &destinationImageParams);
            m_destinationImageAlloc = allocateImage(vki, vk, vkPhysDevice, m_device, *m_destination,
                                                    MemoryRequirement::Any, *m_allocator, m_params.allocationKind, 0u);
            VK_CHECK(vk.bindImageMemory(m_device, *m_destination, m_destinationImageAlloc->getMemory(),
                                        m_destinationImageAlloc->getOffset()));
#ifndef CTS_USES_VULKANSC
        }
        else
        {
            destinationImageParams.flags |=
                (vk::VK_IMAGE_CREATE_SPARSE_BINDING_BIT | vk::VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT);
            vk::VkImageFormatProperties imageFormatProperties;
            if (vki.getPhysicalDeviceImageFormatProperties(
                    vkPhysDevice, destinationImageParams.format, destinationImageParams.imageType,
                    destinationImageParams.tiling, destinationImageParams.usage, destinationImageParams.flags,
                    &imageFormatProperties) == vk::VK_ERROR_FORMAT_NOT_SUPPORTED)
            {
                TCU_THROW(NotSupportedError, "Image format not supported");
            }
            m_destination     = createImage(vk, m_device, &destinationImageParams);
            m_sparseSemaphore = createSemaphore(vk, m_device);
            allocateAndBindSparseImage(vk, m_device, vkPhysDevice, vki, destinationImageParams, m_sparseSemaphore.get(),
                                       context.getSparseQueue(), *m_allocator, m_sparseAllocations,
                                       mapVkFormat(destinationImageParams.format), m_destination.get());
        }
#endif
    }
}

tcu::TestStatus CopyBufferToDepthStencil::iterate(void)
{
    // Create source depth/stencil content. Treat as 1D texture to get different pattern
    m_sourceTextureLevel =
        de::MovePtr<tcu::TextureLevel>(new tcu::TextureLevel(m_textureFormat, (int)m_params.src.buffer.size, 1));
    // Fill buffer with linear gradiant
    generateBuffer(m_sourceTextureLevel->getAccess(), (int)m_params.src.buffer.size, 1, 1);

    // Create image layer for depth/stencil
    m_destinationTextureLevel = de::MovePtr<tcu::TextureLevel>(
        new tcu::TextureLevel(m_textureFormat, m_params.dst.image.extent.width, m_params.dst.image.extent.height,
                              m_params.dst.image.extent.depth));

    // Fill image layer with 2D gradiant
    generateBuffer(m_destinationTextureLevel->getAccess(), m_params.dst.image.extent.width,
                   m_params.dst.image.extent.height, m_params.dst.image.extent.depth);

    // Fill m_extendedTextureLevel with copy of m_destinationTextureLevel
    // Then iterate over each of the regions given in m_params.regions and copy m_sourceTextureLevel content to m_extendedTextureLevel
    // This emulates what the HW will be doing.
    generateExpectedResult();

    // Upload our source depth/stencil content to the source buffer
    // This is the buffer that will be used by region commands
    std::vector<VkBufferImageCopy> bufferImageCopies;
    std::vector<VkBufferImageCopy2KHR> bufferImageCopies2KHR;
    VkDeviceSize bufferOffset  = 0;
    const VkDevice vkDevice    = m_device;
    const DeviceInterface &vk  = m_context.getDeviceInterface();
    char *dstPtr               = reinterpret_cast<char *>(m_sourceBufferAlloc->getHostPtr());
    bool depthLoaded           = false;
    bool stencilLoaded         = false;
    VkDeviceSize depthOffset   = 0;
    VkDeviceSize stencilOffset = 0;

    // To be able to test ordering depth & stencil differently
    // we take the given copy regions and use that as the desired order
    // and copy the appropriate data into place and compute the appropriate
    // data offsets to be used in the copy command.
    for (uint32_t i = 0; i < m_params.regions.size(); i++)
    {
        tcu::ConstPixelBufferAccess bufferAccess = m_sourceTextureLevel->getAccess();
        uint32_t bufferSize        = bufferAccess.getWidth() * bufferAccess.getHeight() * bufferAccess.getDepth();
        VkBufferImageCopy copyData = m_params.regions[i].bufferImageCopy;
        char *srcPtr;

        if (copyData.imageSubresource.aspectMask == VK_IMAGE_ASPECT_DEPTH_BIT && !depthLoaded)
        {
            // Create level that is same component as depth buffer (e.g. D16, D24, D32F)
            tcu::TextureLevel depthTexture(mapCombinedToDepthTransferFormat(bufferAccess.getFormat()),
                                           bufferAccess.getWidth(), bufferAccess.getHeight(), bufferAccess.getDepth());
            bufferSize *= tcu::getPixelSize(depthTexture.getFormat());
            // Copy depth component only from source data. This gives us packed depth-only data.
            tcu::copy(depthTexture.getAccess(),
                      tcu::getEffectiveDepthStencilAccess(bufferAccess, tcu::Sampler::MODE_DEPTH));
            srcPtr = (char *)depthTexture.getAccess().getDataPtr();
            // Copy packed depth-only data to output buffer
            deMemcpy(dstPtr, srcPtr, bufferSize);
            depthLoaded = true;
            depthOffset = bufferOffset;
            dstPtr += bufferSize;
            bufferOffset += bufferSize;
            copyData.bufferOffset += depthOffset;
        }
        else if (!stencilLoaded)
        {
            // Create level that is same component as stencil buffer (always 8-bits)
            tcu::TextureLevel stencilTexture(
                tcu::getEffectiveDepthStencilTextureFormat(bufferAccess.getFormat(), tcu::Sampler::MODE_STENCIL),
                bufferAccess.getWidth(), bufferAccess.getHeight(), bufferAccess.getDepth());
            // Copy stencil component only from source data. This gives us packed stencil-only data.
            tcu::copy(stencilTexture.getAccess(),
                      tcu::getEffectiveDepthStencilAccess(bufferAccess, tcu::Sampler::MODE_STENCIL));
            srcPtr = (char *)stencilTexture.getAccess().getDataPtr();
            // Copy packed stencil-only data to output buffer
            deMemcpy(dstPtr, srcPtr, bufferSize);
            stencilLoaded = true;
            stencilOffset = bufferOffset;
            dstPtr += bufferSize;
            bufferOffset += bufferSize;

            // Reference image generation uses pixel offsets based on buffer offset.
            // We need to adjust the offset now that the stencil data is not interleaved.
            copyData.bufferOffset /= tcu::getPixelSize(m_textureFormat);

            copyData.bufferOffset += stencilOffset;
        }

        if (!(m_params.extensionFlags & COPY_COMMANDS_2))
        {
            bufferImageCopies.push_back(copyData);
        }
        else
        {
            DE_ASSERT(m_params.extensionFlags & COPY_COMMANDS_2);
            bufferImageCopies2KHR.push_back(convertvkBufferImageCopyTovkBufferImageCopy2KHR(copyData));
        }
    }

    flushAlloc(vk, vkDevice, *m_sourceBufferAlloc);

    // Upload the depth/stencil data from m_destinationTextureLevel to initialize
    // depth and stencil to known values.
    // Uses uploadImageAspect so makes its own buffers for depth and stencil
    // aspects (as needed) and copies them with independent vkCmdCopyBufferToImage commands.
    uploadImage(m_destinationTextureLevel->getAccess(), *m_destination, m_params.dst.image);

    const VkImageMemoryBarrier imageBarrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, // VkStructureType sType;
                                               DE_NULL,                                // const void* pNext;
                                               VK_ACCESS_TRANSFER_WRITE_BIT,           // VkAccessFlags srcAccessMask;
                                               VK_ACCESS_TRANSFER_WRITE_BIT,           // VkAccessFlags dstAccessMask;
                                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,   // VkImageLayout oldLayout;
                                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,   // VkImageLayout newLayout;
                                               VK_QUEUE_FAMILY_IGNORED,                // uint32_t srcQueueFamilyIndex;
                                               VK_QUEUE_FAMILY_IGNORED,                // uint32_t dstQueueFamilyIndex;
                                               *m_destination,                         // VkImage image;
                                               {
                                                   // VkImageSubresourceRange subresourceRange;
                                                   getAspectFlags(m_textureFormat), // VkImageAspectFlags aspectMask;
                                                   0u,                              // uint32_t baseMipLevel;
                                                   1u,                              // uint32_t mipLevels;
                                                   0u,                              // uint32_t baseArraySlice;
                                                   1u                               // uint32_t arraySize;
                                               }};

    // Copy from buffer to depth/stencil image

    beginCommandBuffer(vk, *m_universalCmdBuffer);
    vk.cmdPipelineBarrier(*m_universalCmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                          (VkDependencyFlags)0, 0, (const VkMemoryBarrier *)DE_NULL, 0,
                          (const VkBufferMemoryBarrier *)DE_NULL, 1, &imageBarrier);

    if (!(m_params.extensionFlags & COPY_COMMANDS_2))
    {
        if (m_params.singleCommand)
        {
            // Issue a single copy command with regions defined by the test.
            vk.cmdCopyBufferToImage(*m_universalCmdBuffer, m_source.get(), m_destination.get(),
                                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, (uint32_t)m_params.regions.size(),
                                    bufferImageCopies.data());
        }
        else
        {
            // Issue a a copy command per region defined by the test.
            for (uint32_t i = 0; i < bufferImageCopies.size(); i++)
            {
                if (i > 0)
                    vk.cmdPipelineBarrier(*m_universalCmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                          VK_PIPELINE_STAGE_TRANSFER_BIT, (VkDependencyFlags)0, 0,
                                          (const VkMemoryBarrier *)DE_NULL, 0, (const VkBufferMemoryBarrier *)DE_NULL,
                                          1, &imageBarrier);

                vk.cmdCopyBufferToImage(*m_universalCmdBuffer, m_source.get(), m_destination.get(),
                                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &bufferImageCopies[i]);
            }
        }
    }
    else
    {
        DE_ASSERT(m_params.extensionFlags & COPY_COMMANDS_2);

        if (m_params.singleCommand)
        {
            // Issue a single copy command with regions defined by the test.
            const VkCopyBufferToImageInfo2KHR copyBufferToImageInfo2KHR = {
                VK_STRUCTURE_TYPE_COPY_BUFFER_TO_IMAGE_INFO_2_KHR, // VkStructureType sType;
                DE_NULL,                                           // const void* pNext;
                m_source.get(),                                    // VkBuffer srcBuffer;
                m_destination.get(),                               // VkImage dstImage;
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,              // VkImageLayout dstImageLayout;
                (uint32_t)m_params.regions.size(),                 // uint32_t regionCount;
                bufferImageCopies2KHR.data()                       // const VkBufferImageCopy2KHR* pRegions;
            };
            vk.cmdCopyBufferToImage2(*m_universalCmdBuffer, &copyBufferToImageInfo2KHR);
        }
        else
        {
            // Issue a a copy command per region defined by the test.
            for (uint32_t i = 0; i < bufferImageCopies2KHR.size(); i++)
            {
                if (i > 0)
                    vk.cmdPipelineBarrier(*m_universalCmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                          VK_PIPELINE_STAGE_TRANSFER_BIT, (VkDependencyFlags)0, 0,
                                          (const VkMemoryBarrier *)DE_NULL, 0, (const VkBufferMemoryBarrier *)DE_NULL,
                                          1, &imageBarrier);

                const VkCopyBufferToImageInfo2KHR copyBufferToImageInfo2KHR = {
                    VK_STRUCTURE_TYPE_COPY_BUFFER_TO_IMAGE_INFO_2_KHR, // VkStructureType sType;
                    DE_NULL,                                           // const void* pNext;
                    m_source.get(),                                    // VkBuffer srcBuffer;
                    m_destination.get(),                               // VkImage dstImage;
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,              // VkImageLayout dstImageLayout;
                    1,                                                 // uint32_t regionCount;
                    &bufferImageCopies2KHR[i]                          // const VkBufferImageCopy2KHR* pRegions;
                };
                // Issue a single copy command with regions defined by the test.
                vk.cmdCopyBufferToImage2(*m_universalCmdBuffer, &copyBufferToImageInfo2KHR);
            }
        }
    }

    endCommandBuffer(vk, *m_universalCmdBuffer);

    if (m_params.useSparseBinding)
    {
        const VkPipelineStageFlags stageBits[] = {VK_PIPELINE_STAGE_TRANSFER_BIT};
        submitCommandsAndWait(vk, vkDevice, m_universalQueue, *m_universalCmdBuffer, false, 1u, 1u, &*m_sparseSemaphore,
                              stageBits);
    }
    else
    {
        submitCommandsAndWait(vk, vkDevice, m_universalQueue, *m_universalCmdBuffer);
    }

    m_context.resetCommandPoolForVKSC(vkDevice, *m_universalCmdPool);

    de::MovePtr<tcu::TextureLevel> resultLevel = readImage(*m_destination, m_params.dst.image);

    // For combined depth/stencil formats both aspects are checked even when the test only
    // copies one. Clear such aspects here for both the result and the reference.
    if (tcu::hasDepthComponent(m_textureFormat.order) && !depthLoaded)
    {
        tcu::clearDepth(m_expectedTextureLevel[0]->getAccess(), 0.0f);
        tcu::clearDepth(resultLevel->getAccess(), 0.0f);
    }
    if (tcu::hasStencilComponent(m_textureFormat.order) && !stencilLoaded)
    {
        tcu::clearStencil(m_expectedTextureLevel[0]->getAccess(), 0);
        tcu::clearStencil(resultLevel->getAccess(), 0);
    }

    return checkTestResult(resultLevel->getAccess());
}

class CopyBufferToDepthStencilTestCase : public vkt::TestCase
{
public:
    CopyBufferToDepthStencilTestCase(tcu::TestContext &testCtx, const std::string &name, const TestParams params)
        : vkt::TestCase(testCtx, name)
        , m_params(params)
    {
    }

    virtual ~CopyBufferToDepthStencilTestCase(void)
    {
    }

    virtual TestInstance *createInstance(Context &context) const
    {
        return new CopyBufferToDepthStencil(context, m_params);
    }

    virtual void checkSupport(Context &context) const
    {
        checkExtensionSupport(context, m_params.extensionFlags);
    }

private:
    TestParams m_params;
};

// CompressedTextureForBlit is a helper class that stores compressed texture data.
// Implementation is based on pipeline::TestTexture2D but it allocates only one level
// and has special cases needed for blits to some formats.

class CompressedTextureForBlit
{
public:
    CompressedTextureForBlit(const tcu::CompressedTexFormat &srcFormat, int width, int height, int depth);

    tcu::PixelBufferAccess getDecompressedAccess() const;
    const tcu::CompressedTexture &getCompressedTexture() const;

protected:
    tcu::CompressedTexture m_compressedTexture;
    de::ArrayBuffer<uint8_t> m_decompressedData;
    tcu::PixelBufferAccess m_decompressedAccess;
};

CompressedTextureForBlit::CompressedTextureForBlit(const tcu::CompressedTexFormat &srcFormat, int width, int height,
                                                   int depth)
    : m_compressedTexture(srcFormat, width, height, depth)
{
    de::Random random(123);

    const int compressedDataSize(m_compressedTexture.getDataSize());
    uint8_t *compressedData((uint8_t *)m_compressedTexture.getData());

    tcu::TextureFormat decompressedSrcFormat(tcu::getUncompressedFormat(srcFormat));
    const int decompressedDataSize(tcu::getPixelSize(decompressedSrcFormat) * width * height * depth);

    // generate random data for compresed textre
    if (tcu::isAstcFormat(srcFormat))
    {
        // comparison doesn't currently handle invalid blocks correctly so we use only valid blocks
        tcu::astc::generateRandomValidBlocks(compressedData, compressedDataSize / tcu::astc::BLOCK_SIZE_BYTES,
                                             srcFormat, tcu::TexDecompressionParams::ASTCMODE_LDR, random.getUint32());
    }
    else if ((srcFormat == tcu::COMPRESSEDTEXFORMAT_BC6H_UFLOAT_BLOCK) ||
             (srcFormat == tcu::COMPRESSEDTEXFORMAT_BC6H_SFLOAT_BLOCK))
    {
        // special case - when we are blitting compressed floating-point image we can't have both big and small values
        // in compressed image; to resolve this we are constructing source texture out of set of predefined compressed
        // blocks that after decompression will have components in proper range

        typedef std::array<uint32_t, 4> BC6HBlock;
        DE_STATIC_ASSERT(sizeof(BC6HBlock) == (4 * sizeof(uint32_t)));
        std::vector<BC6HBlock> validBlocks;

        if (srcFormat == tcu::COMPRESSEDTEXFORMAT_BC6H_UFLOAT_BLOCK)
        {
            // define set of few valid blocks that contain values from <0; 1> range
            validBlocks = {
                {{1686671500, 3957317723, 3010132342, 2420137890}}, {{3538027716, 298848033, 1925786021, 2022072301}},
                {{2614043466, 1636155440, 1023731774, 1894349986}}, {{3433039318, 1294346072, 1587319645, 1738449906}},
                {{1386298160, 1639492154, 1273285776, 361562050}},  {{1310110688, 526460754, 3630858047, 537617591}},
                {{3270356556, 2432993217, 2415924417, 1792488857}}, {{1204947583, 353249154, 3739153467, 2068076443}},
            };
        }
        else
        {
            // define set of few valid blocks that contain values from <-1; 1> range
            validBlocks = {
                {{2120678840, 3264271120, 4065378848, 3479743703}}, {{1479697556, 3480872527, 3369382558, 568252340}},
                {{1301480032, 1607738094, 3055221704, 3663953681}}, {{3531657186, 2285472028, 1429601507, 1969308187}},
                {{73229044, 650504649, 1120954865, 2626631975}},    {{3872486086, 15326178, 2565171269, 2857722432}},
                {{1301480032, 1607738094, 3055221704, 3663953681}}, {{73229044, 650504649, 1120954865, 2626631975}},
            };
        }

        uint32_t *compressedDataUint32 = reinterpret_cast<uint32_t *>(compressedData);
        const int blocksCount          = compressedDataSize / static_cast<int>(sizeof(BC6HBlock));

        // fill data using randomly selected valid blocks
        for (int blockNdx = 0; blockNdx < blocksCount; blockNdx++)
        {
            uint32_t selectedBlock = random.getUint32() % static_cast<uint32_t>(validBlocks.size());
            deMemcpy(compressedDataUint32, validBlocks[selectedBlock].data(), sizeof(BC6HBlock));
            compressedDataUint32 += 4;
        }
    }
    else if (srcFormat != tcu::COMPRESSEDTEXFORMAT_ETC1_RGB8)
    {
        // random initial values cause assertion during the decompression in case of COMPRESSEDTEXFORMAT_ETC1_RGB8 format
        for (int byteNdx = 0; byteNdx < compressedDataSize; byteNdx++)
            compressedData[byteNdx] = 0xFF & random.getUint32();
    }

    // alocate space for decompressed texture
    m_decompressedData.setStorage(decompressedDataSize);
    m_decompressedAccess =
        tcu::PixelBufferAccess(decompressedSrcFormat, width, height, depth, m_decompressedData.getPtr());

    // store decompressed data
    m_compressedTexture.decompress(m_decompressedAccess,
                                   tcu::TexDecompressionParams(tcu::TexDecompressionParams::ASTCMODE_LDR));
}

tcu::PixelBufferAccess CompressedTextureForBlit::getDecompressedAccess() const
{
    return m_decompressedAccess;
}

const tcu::CompressedTexture &CompressedTextureForBlit::getCompressedTexture() const
{
    return m_compressedTexture;
}

// Copy from image to image with scaling.

class BlittingImages : public CopiesAndBlittingTestInstance
{
public:
    BlittingImages(Context &context, TestParams params);
    virtual tcu::TestStatus iterate(void);

protected:
    virtual tcu::TestStatus checkTestResult(tcu::ConstPixelBufferAccess result);
    virtual void copyRegionToTextureLevel(tcu::ConstPixelBufferAccess src, tcu::PixelBufferAccess dst,
                                          CopyRegion region, uint32_t mipLevel = 0u);
    virtual void generateExpectedResult(void);
    void uploadCompressedImage(const VkImage &image, const ImageParms &parms);

private:
    bool checkNonNearestFilteredResult(const tcu::ConstPixelBufferAccess &result,
                                       const tcu::ConstPixelBufferAccess &clampedReference,
                                       const tcu::ConstPixelBufferAccess &unclampedReference,
                                       const tcu::TextureFormat &sourceFormat);
    bool checkNearestFilteredResult(const tcu::ConstPixelBufferAccess &result,
                                    const tcu::ConstPixelBufferAccess &source);

    bool checkCompressedNonNearestFilteredResult(const tcu::ConstPixelBufferAccess &result,
                                                 const tcu::ConstPixelBufferAccess &clampedReference,
                                                 const tcu::ConstPixelBufferAccess &unclampedReference,
                                                 const tcu::CompressedTexFormat format);
    bool checkCompressedNearestFilteredResult(const tcu::ConstPixelBufferAccess &result,
                                              const tcu::ConstPixelBufferAccess &source,
                                              const tcu::CompressedTexFormat format);

    Move<VkImage> m_source;
    de::MovePtr<Allocation> m_sourceImageAlloc;
    Move<VkImage> m_destination;
    de::MovePtr<Allocation> m_destinationImageAlloc;
    std::vector<de::SharedPtr<Allocation>> m_sparseAllocations;
    Move<VkSemaphore> m_sparseSemaphore;

    de::MovePtr<tcu::TextureLevel> m_unclampedExpectedTextureLevel;

    // helper used only when bliting from compressed formats
    typedef de::SharedPtr<CompressedTextureForBlit> CompressedTextureForBlitSp;
    CompressedTextureForBlitSp m_sourceCompressedTexture;
    CompressedTextureForBlitSp m_destinationCompressedTexture;
};

BlittingImages::BlittingImages(Context &context, TestParams params) : CopiesAndBlittingTestInstance(context, params)
{
    const InstanceInterface &vki        = context.getInstanceInterface();
    const DeviceInterface &vk           = context.getDeviceInterface();
    const VkPhysicalDevice vkPhysDevice = context.getPhysicalDevice();
    const VkDevice vkDevice             = m_device;
    Allocator &memAlloc                 = context.getDefaultAllocator();

    // Create source image
    {
        VkImageCreateInfo sourceImageParams = {
            VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,                               // VkStructureType sType;
            DE_NULL,                                                           // const void* pNext;
            getCreateFlags(m_params.src.image),                                // VkImageCreateFlags flags;
            m_params.src.image.imageType,                                      // VkImageType imageType;
            m_params.src.image.format,                                         // VkFormat format;
            getExtent3D(m_params.src.image),                                   // VkExtent3D extent;
            1u,                                                                // uint32_t mipLevels;
            getArraySize(m_params.src.image),                                  // uint32_t arraySize;
            VK_SAMPLE_COUNT_1_BIT,                                             // uint32_t samples;
            m_params.src.image.tiling,                                         // VkImageTiling tiling;
            VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, // VkImageUsageFlags usage;
            VK_SHARING_MODE_EXCLUSIVE,                                         // VkSharingMode sharingMode;
            0u,                                                                // uint32_t queueFamilyIndexCount;
            (const uint32_t *)DE_NULL,                                         // const uint32_t* pQueueFamilyIndices;
            VK_IMAGE_LAYOUT_UNDEFINED,                                         // VkImageLayout initialLayout;
        };

#ifndef CTS_USES_VULKANSC
        if (!params.useSparseBinding)
        {
#endif
            m_source           = createImage(vk, m_device, &sourceImageParams);
            m_sourceImageAlloc = allocateImage(vki, vk, vkPhysDevice, m_device, *m_source, MemoryRequirement::Any,
                                               *m_allocator, m_params.allocationKind, 0u);
            VK_CHECK(vk.bindImageMemory(m_device, *m_source, m_sourceImageAlloc->getMemory(),
                                        m_sourceImageAlloc->getOffset()));
#ifndef CTS_USES_VULKANSC
        }
        else
        {
            sourceImageParams.flags |=
                (vk::VK_IMAGE_CREATE_SPARSE_BINDING_BIT | vk::VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT);
            vk::VkImageFormatProperties imageFormatProperties;
            if (vki.getPhysicalDeviceImageFormatProperties(vkPhysDevice, sourceImageParams.format,
                                                           sourceImageParams.imageType, sourceImageParams.tiling,
                                                           sourceImageParams.usage, sourceImageParams.flags,
                                                           &imageFormatProperties) == vk::VK_ERROR_FORMAT_NOT_SUPPORTED)
            {
                TCU_THROW(NotSupportedError, "Image format not supported");
            }
            m_source = createImage(
                vk, m_device,
                &sourceImageParams); //de::MovePtr<SparseImage>(new SparseImage(vk, vk, vkPhysDevice, vki, sourceImageParams, m_queue, *m_allocator, mapVkFormat(sourceImageParams.format)));
            m_sparseSemaphore = createSemaphore(vk, m_device);
            allocateAndBindSparseImage(vk, m_device, vkPhysDevice, vki, sourceImageParams, m_sparseSemaphore.get(),
                                       context.getSparseQueue(), *m_allocator, m_sparseAllocations,
                                       mapVkFormat(sourceImageParams.format), m_source.get());
        }
#endif
    }

    // Create destination image
    {
        const VkImageCreateInfo destinationImageParams = {
            VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,                               // VkStructureType sType;
            DE_NULL,                                                           // const void* pNext;
            getCreateFlags(m_params.dst.image),                                // VkImageCreateFlags flags;
            m_params.dst.image.imageType,                                      // VkImageType imageType;
            m_params.dst.image.format,                                         // VkFormat format;
            getExtent3D(m_params.dst.image),                                   // VkExtent3D extent;
            1u,                                                                // uint32_t mipLevels;
            getArraySize(m_params.dst.image),                                  // uint32_t arraySize;
            VK_SAMPLE_COUNT_1_BIT,                                             // uint32_t samples;
            m_params.dst.image.tiling,                                         // VkImageTiling tiling;
            VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, // VkImageUsageFlags usage;
            VK_SHARING_MODE_EXCLUSIVE,                                         // VkSharingMode sharingMode;
            0u,                                                                // uint32_t queueFamilyIndexCount;
            (const uint32_t *)DE_NULL,                                         // const uint32_t* pQueueFamilyIndices;
            VK_IMAGE_LAYOUT_UNDEFINED,                                         // VkImageLayout initialLayout;
        };

        m_destination           = createImage(vk, vkDevice, &destinationImageParams);
        m_destinationImageAlloc = allocateImage(vki, vk, vkPhysDevice, vkDevice, *m_destination, MemoryRequirement::Any,
                                                memAlloc, m_params.allocationKind, 0u);
        VK_CHECK(vk.bindImageMemory(vkDevice, *m_destination, m_destinationImageAlloc->getMemory(),
                                    m_destinationImageAlloc->getOffset()));
    }
}

tcu::TestStatus BlittingImages::iterate(void)
{
    const DeviceInterface &vk        = m_context.getDeviceInterface();
    const VkDevice vkDevice          = m_device;
    const ImageParms &srcImageParams = m_params.src.image;
    const int srcWidth               = static_cast<int>(srcImageParams.extent.width);
    const int srcHeight              = static_cast<int>(srcImageParams.extent.height);
    const int srcDepth               = static_cast<int>(srcImageParams.extent.depth);
    const ImageParms &dstImageParams = m_params.dst.image;
    const int dstWidth               = static_cast<int>(dstImageParams.extent.width);
    const int dstHeight              = static_cast<int>(dstImageParams.extent.height);
    const int dstDepth               = static_cast<int>(dstImageParams.extent.depth);

    std::vector<VkImageBlit> regions;
    std::vector<VkImageBlit2KHR> regions2KHR;

    // setup blit regions - they are also needed for reference generation
    if (!(m_params.extensionFlags & COPY_COMMANDS_2))
    {
        regions.reserve(m_params.regions.size());
        for (const auto &r : m_params.regions)
            regions.push_back(r.imageBlit);
    }
    else
    {
        DE_ASSERT(m_params.extensionFlags & COPY_COMMANDS_2);
        regions2KHR.reserve(m_params.regions.size());
        for (const auto &r : m_params.regions)
            regions2KHR.push_back(convertvkImageBlitTovkImageBlit2KHR(r.imageBlit));
    }

    // generate source image
    if (isCompressedFormat(srcImageParams.format))
    {
        // for compressed images srcImageParams.fillMode is not used - we are using random data
        tcu::CompressedTexFormat compressedFormat = mapVkCompressedFormat(srcImageParams.format);
        m_sourceCompressedTexture =
            CompressedTextureForBlitSp(new CompressedTextureForBlit(compressedFormat, srcWidth, srcHeight, srcDepth));
        uploadCompressedImage(m_source.get(), srcImageParams);
    }
    else
    {
        // non-compressed image is filled with selected fillMode
        const tcu::TextureFormat srcTcuFormat = mapVkFormat(srcImageParams.format);
        m_sourceTextureLevel =
            de::MovePtr<tcu::TextureLevel>(new tcu::TextureLevel(srcTcuFormat, srcWidth, srcHeight, srcDepth));
        generateBuffer(m_sourceTextureLevel->getAccess(), srcWidth, srcHeight, srcDepth, srcImageParams.fillMode);
        uploadImage(m_sourceTextureLevel->getAccess(), m_source.get(), srcImageParams);
    }

    // generate destination image
    if (isCompressedFormat(dstImageParams.format))
    {
        // compressed images are filled with random data
        tcu::CompressedTexFormat compressedFormat = mapVkCompressedFormat(dstImageParams.format);
        m_destinationCompressedTexture =
            CompressedTextureForBlitSp(new CompressedTextureForBlit(compressedFormat, srcWidth, srcHeight, srcDepth));
        uploadCompressedImage(m_destination.get(), dstImageParams);
    }
    else
    {
        // non-compressed image is filled with white background
        const tcu::TextureFormat dstTcuFormat = mapVkFormat(dstImageParams.format);
        m_destinationTextureLevel =
            de::MovePtr<tcu::TextureLevel>(new tcu::TextureLevel(dstTcuFormat, dstWidth, dstHeight, dstDepth));
        generateBuffer(m_destinationTextureLevel->getAccess(), dstWidth, dstHeight, dstDepth, dstImageParams.fillMode);
        uploadImage(m_destinationTextureLevel->getAccess(), m_destination.get(), dstImageParams);
    }

    generateExpectedResult();

    // Barriers for copying images to buffer
    const VkImageMemoryBarrier imageBarriers[]{
        {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, // VkStructureType sType;
         DE_NULL,                                // const void* pNext;
         VK_ACCESS_TRANSFER_WRITE_BIT,           // VkAccessFlags srcAccessMask;
         VK_ACCESS_TRANSFER_READ_BIT,            // VkAccessFlags dstAccessMask;
         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,   // VkImageLayout oldLayout;
         srcImageParams.operationLayout,         // VkImageLayout newLayout;
         VK_QUEUE_FAMILY_IGNORED,                // uint32_t srcQueueFamilyIndex;
         VK_QUEUE_FAMILY_IGNORED,                // uint32_t dstQueueFamilyIndex;
         m_source.get(),                         // VkImage image;
         {
             // VkImageSubresourceRange subresourceRange;
             getAspectFlags(srcImageParams.format), //   VkImageAspectFlags aspectMask;
             0u,                                    //   uint32_t baseMipLevel;
             1u,                                    //   uint32_t mipLevels;
             0u,                                    //   uint32_t baseArraySlice;
             getArraySize(m_params.src.image)       //   uint32_t arraySize;
         }},
        {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, // VkStructureType sType;
         DE_NULL,                                // const void* pNext;
         VK_ACCESS_TRANSFER_WRITE_BIT,           // VkAccessFlags srcAccessMask;
         VK_ACCESS_TRANSFER_WRITE_BIT,           // VkAccessFlags dstAccessMask;
         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,   // VkImageLayout oldLayout;
         dstImageParams.operationLayout,         // VkImageLayout newLayout;
         VK_QUEUE_FAMILY_IGNORED,                // uint32_t srcQueueFamilyIndex;
         VK_QUEUE_FAMILY_IGNORED,                // uint32_t dstQueueFamilyIndex;
         m_destination.get(),                    // VkImage image;
         {
             // VkImageSubresourceRange subresourceRange;
             getAspectFlags(dstImageParams.format), //   VkImageAspectFlags aspectMask;
             0u,                                    //   uint32_t baseMipLevel;
             1u,                                    //   uint32_t mipLevels;
             0u,                                    //   uint32_t baseArraySlice;
             getArraySize(m_params.dst.image)       //   uint32_t arraySize;
         }}};

    beginCommandBuffer(vk, *m_universalCmdBuffer);
    vk.cmdPipelineBarrier(*m_universalCmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                          (VkDependencyFlags)0, 0, (const VkMemoryBarrier *)DE_NULL, 0,
                          (const VkBufferMemoryBarrier *)DE_NULL, 2, imageBarriers);

    if (!(m_params.extensionFlags & COPY_COMMANDS_2))
    {
        vk.cmdBlitImage(*m_universalCmdBuffer, m_source.get(), srcImageParams.operationLayout, m_destination.get(),
                        dstImageParams.operationLayout, (uint32_t)m_params.regions.size(), &regions[0],
                        m_params.filter);
    }
    else
    {
        DE_ASSERT(m_params.extensionFlags & COPY_COMMANDS_2);
        const VkBlitImageInfo2KHR blitImageInfo2KHR{
            VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2_KHR, // VkStructureType sType;
            DE_NULL,                                 // const void* pNext;
            m_source.get(),                          // VkImage srcImage;
            srcImageParams.operationLayout,          // VkImageLayout srcImageLayout;
            m_destination.get(),                     // VkImage dstImage;
            dstImageParams.operationLayout,          // VkImageLayout dstImageLayout;
            (uint32_t)m_params.regions.size(),       // uint32_t regionCount;
            &regions2KHR[0],                         // const VkImageBlit2KHR* pRegions;
            m_params.filter,                         // VkFilter filter;
        };
        vk.cmdBlitImage2(*m_universalCmdBuffer, &blitImageInfo2KHR);
    }

    endCommandBuffer(vk, *m_universalCmdBuffer);
    if (m_params.useSparseBinding)
    {
        const VkPipelineStageFlags stageBits[] = {VK_PIPELINE_STAGE_TRANSFER_BIT};
        submitCommandsAndWait(vk, vkDevice, m_universalQueue, *m_universalCmdBuffer, false, 1u, 1u, &*m_sparseSemaphore,
                              stageBits);
    }
    else
    {
        submitCommandsAndWait(vk, vkDevice, m_universalQueue, *m_universalCmdBuffer);
    }
    m_context.resetCommandPoolForVKSC(vkDevice, *m_universalCmdPool);

    de::MovePtr<tcu::TextureLevel> resultLevel = readImage(*m_destination, dstImageParams);
    tcu::PixelBufferAccess resultAccess        = resultLevel->getAccess();

    // if blit was done to a compressed format we need to decompress it to be able to verify it
    if (m_destinationCompressedTexture)
    {
        uint8_t *const compressedDataSrc(static_cast<uint8_t *>(resultAccess.getDataPtr()));
        const tcu::CompressedTexFormat dstCompressedFormat(mapVkCompressedFormat(dstImageParams.format));
        tcu::TextureLevel decompressedLevel(getUncompressedFormat(dstCompressedFormat), dstWidth, dstHeight, dstDepth);
        tcu::PixelBufferAccess decompressedAccess(decompressedLevel.getAccess());

        tcu::decompress(decompressedAccess, dstCompressedFormat, compressedDataSrc);

        return checkTestResult(decompressedAccess);
    }

    return checkTestResult(resultAccess);
}

static float calculateFloatConversionError(int srcBits)
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

bool BlittingImages::checkNonNearestFilteredResult(const tcu::ConstPixelBufferAccess &result,
                                                   const tcu::ConstPixelBufferAccess &clampedExpected,
                                                   const tcu::ConstPixelBufferAccess &unclampedExpected,
                                                   const tcu::TextureFormat &srcFormat)
{
    tcu::TestLog &log(m_context.getTestContext().getLog());
    const tcu::TextureFormat dstFormat             = result.getFormat();
    const tcu::TextureChannelClass dstChannelClass = tcu::getTextureChannelClass(dstFormat.type);
    const tcu::TextureChannelClass srcChannelClass = tcu::getTextureChannelClass(srcFormat.type);
    bool isOk                                      = false;

    log << tcu::TestLog::Section("ClampedSourceImage", "Region with clamped edges on source image.");

    // if either of srcImage or dstImage stores values as a signed/unsigned integer,
    // the other must also store values a signed/unsigned integer
    // e.g. blit unorm to uscaled is not allowed as uscaled formats store data as integers
    // despite the fact that both formats are sampled as floats
    bool dstImageIsIntClass = dstChannelClass == tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER ||
                              dstChannelClass == tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER;
    bool srcImageIsIntClass = srcChannelClass == tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER ||
                              srcChannelClass == tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER;
    if (dstImageIsIntClass != srcImageIsIntClass)
    {
        log << tcu::TestLog::EndSection;
        return false;
    }

    if (isFloatFormat(dstFormat))
    {
        const bool srcIsSRGB       = tcu::isSRGB(srcFormat);
        const tcu::Vec4 srcMaxDiff = getFormatThreshold(srcFormat) * tcu::Vec4(srcIsSRGB ? 2.0f : 1.0f);
        const tcu::Vec4 dstMaxDiff = getFormatThreshold(dstFormat);
        const tcu::Vec4 threshold =
            (srcMaxDiff + dstMaxDiff) * ((m_params.filter == VK_FILTER_CUBIC_EXT) ? 1.5f : 1.0f);

        isOk = tcu::floatThresholdCompare(log, "Compare", "Result comparsion", clampedExpected, result, threshold,
                                          tcu::COMPARE_LOG_RESULT);
        log << tcu::TestLog::EndSection;

        if (!isOk)
        {
            log << tcu::TestLog::Section("NonClampedSourceImage", "Region with non-clamped edges on source image.");
            isOk = tcu::floatThresholdCompare(log, "Compare", "Result comparsion", unclampedExpected, result, threshold,
                                              tcu::COMPARE_LOG_RESULT);
            log << tcu::TestLog::EndSection;
        }
    }
    else
    {
        tcu::UVec4 threshold;
        // Calculate threshold depending on channel width of destination format.
        const tcu::IVec4 dstBitDepth = tcu::getTextureFormatBitDepth(dstFormat);
        const tcu::IVec4 srcBitDepth = tcu::getTextureFormatBitDepth(srcFormat);
        for (uint32_t i = 0; i < 4; ++i)
        {
            DE_ASSERT(dstBitDepth[i] < std::numeric_limits<uint64_t>::digits);
            DE_ASSERT(srcBitDepth[i] < std::numeric_limits<uint64_t>::digits);
            uint64_t threshold64 =
                1 + de::max(((UINT64_C(1) << dstBitDepth[i]) - 1) /
                                de::clamp((UINT64_C(1) << srcBitDepth[i]) - 1, UINT64_C(1), UINT64_C(256)),
                            UINT64_C(1));
            DE_ASSERT(threshold64 <= std::numeric_limits<uint32_t>::max());
            threshold[i] = static_cast<uint32_t>(threshold64);
        }

        isOk = tcu::intThresholdCompare(log, "Compare", "Result comparsion", clampedExpected, result, threshold,
                                        tcu::COMPARE_LOG_RESULT);
        log << tcu::TestLog::EndSection;

        if (!isOk)
        {
            log << tcu::TestLog::Section("NonClampedSourceImage", "Region with non-clamped edges on source image.");
            isOk = tcu::intThresholdCompare(log, "Compare", "Result comparsion", unclampedExpected, result, threshold,
                                            tcu::COMPARE_LOG_RESULT);
            log << tcu::TestLog::EndSection;
        }
    }

    return isOk;
}

bool BlittingImages::checkCompressedNonNearestFilteredResult(const tcu::ConstPixelBufferAccess &result,
                                                             const tcu::ConstPixelBufferAccess &clampedReference,
                                                             const tcu::ConstPixelBufferAccess &unclampedReference,
                                                             const tcu::CompressedTexFormat format)
{
    tcu::TestLog &log                  = m_context.getTestContext().getLog();
    const tcu::TextureFormat dstFormat = result.getFormat();

    // there are rare cases wher one or few pixels have slightly bigger error
    // in one of channels this accepted error allows those casses to pass
    const tcu::Vec4 acceptedError(0.06f);

    const tcu::Vec4 srcMaxDiff = getCompressedFormatThreshold(format);
    const tcu::Vec4 dstMaxDiff =
        m_destinationCompressedTexture ?
            getCompressedFormatThreshold(m_destinationCompressedTexture->getCompressedTexture().getFormat()) :
            getFormatThreshold(dstFormat);
    const tcu::Vec4 threshold =
        (srcMaxDiff + dstMaxDiff) * ((m_params.filter == VK_FILTER_CUBIC_EXT) ? 1.5f : 1.0f) + acceptedError;

    bool filteredResultVerification(false);
    tcu::Vec4 filteredResultMinValue(-6e6);
    tcu::Vec4 filteredResultMaxValue(6e6);
    tcu::TextureLevel filteredResult;
    tcu::TextureLevel filteredClampedReference;
    tcu::TextureLevel filteredUnclampedReference;

    if (((format == tcu::COMPRESSEDTEXFORMAT_BC6H_SFLOAT_BLOCK) ||
         (format == tcu::COMPRESSEDTEXFORMAT_BC6H_UFLOAT_BLOCK)))
    {
        if ((dstFormat.type == tcu::TextureFormat::FLOAT) || (dstFormat.type == tcu::TextureFormat::HALF_FLOAT))
        {
            // for compressed formats we are using random data and for bc6h formats
            // this will give us also large color values; when we are bliting to
            // a format that accepts large values we can end up with large diferences
            // betwean filtered result and reference; to avoid that we need to remove
            // values that are to big from verification
            filteredResultVerification = true;
            filteredResultMinValue     = tcu::Vec4(-10.0f);
            filteredResultMaxValue     = tcu::Vec4(10.0f);
        }
        else if (dstFormat.type == tcu::TextureFormat::UNSIGNED_INT_11F_11F_10F_REV)
        {
            // we need to clamp some formats to <0;1> range as it has
            // small precision for big numbers compared to reference
            filteredResultVerification = true;
            filteredResultMinValue     = tcu::Vec4(0.0f);
            filteredResultMaxValue     = tcu::Vec4(1.0f);
        }
        // else don't use filtered verification
    }

    if (filteredResultVerification)
    {
        filteredResult.setStorage(dstFormat, result.getWidth(), result.getHeight(), result.getDepth());
        tcu::PixelBufferAccess filteredResultAcccess(filteredResult.getAccess());

        filteredClampedReference.setStorage(dstFormat, result.getWidth(), result.getHeight(), result.getDepth());
        tcu::PixelBufferAccess filteredClampedAcccess(filteredClampedReference.getAccess());

        filteredUnclampedReference.setStorage(dstFormat, result.getWidth(), result.getHeight(), result.getDepth());
        tcu::PixelBufferAccess filteredUnclampedResultAcccess(filteredUnclampedReference.getAccess());

        for (int32_t z = 0; z < result.getDepth(); z++)
            for (int32_t y = 0; y < result.getHeight(); y++)
                for (int32_t x = 0; x < result.getWidth(); x++)
                {
                    tcu::Vec4 resultTexel             = result.getPixel(x, y, z);
                    tcu::Vec4 clampedReferenceTexel   = clampedReference.getPixel(x, y, z);
                    tcu::Vec4 unclampedReferenceTexel = unclampedReference.getPixel(x, y, z);

                    resultTexel = tcu::clamp(resultTexel, filteredResultMinValue, filteredResultMaxValue);
                    clampedReferenceTexel =
                        tcu::clamp(clampedReferenceTexel, filteredResultMinValue, filteredResultMaxValue);
                    unclampedReferenceTexel =
                        tcu::clamp(unclampedReferenceTexel, filteredResultMinValue, filteredResultMaxValue);

                    filteredResultAcccess.setPixel(resultTexel, x, y, z);
                    filteredClampedAcccess.setPixel(clampedReferenceTexel, x, y, z);
                    filteredUnclampedResultAcccess.setPixel(unclampedReferenceTexel, x, y, z);
                }
    }

    const tcu::ConstPixelBufferAccess clampedRef =
        filteredResultVerification ? filteredClampedReference.getAccess() : clampedReference;
    const tcu::ConstPixelBufferAccess res = filteredResultVerification ? filteredResult.getAccess() : result;

    log << tcu::TestLog::Section("ClampedSourceImage", "Region with clamped edges on source image.");
    bool isOk = tcu::floatThresholdCompare(log, "Compare", "Result comparsion", clampedRef, res, threshold,
                                           tcu::COMPARE_LOG_RESULT);
    log << tcu::TestLog::EndSection;

    if (!isOk)
    {
        const tcu::ConstPixelBufferAccess unclampedRef =
            filteredResultVerification ? filteredUnclampedReference.getAccess() : unclampedReference;

        log << tcu::TestLog::Section("NonClampedSourceImage", "Region with non-clamped edges on source image.");
        isOk = tcu::floatThresholdCompare(log, "Compare", "Result comparsion", unclampedRef, res, threshold,
                                          tcu::COMPARE_LOG_RESULT);
        log << tcu::TestLog::EndSection;
    }

    return isOk;
}

//! Utility to encapsulate coordinate computation and loops.
struct CompareEachPixelInEachRegion
{
protected:
    void getUsedZRange(int32_t range[2], const ImageParms &imgParams, const VkImageSubresourceLayers &layers,
                       const VkOffset3D offsets[2]) const
    {
        if (imgParams.imageType == VK_IMAGE_TYPE_3D)
        {
            range[0] = offsets[0].z;
            range[1] = offsets[1].z;
        }
        else if (imgParams.imageType == VK_IMAGE_TYPE_2D)
        {
            range[0] = static_cast<int32_t>(layers.baseArrayLayer);
            range[1] =
                static_cast<int32_t>(layers.baseArrayLayer + ((layers.layerCount == VK_REMAINING_ARRAY_LAYERS) ?
                                                                  (getArraySize(imgParams) - layers.baseArrayLayer) :
                                                                  layers.layerCount));
        }
        else
        {
            range[0] = 0;
            range[1] = 1;
        }
    }

public:
    virtual ~CompareEachPixelInEachRegion(void)
    {
    }
    virtual bool compare(const void *pUserData, const int x, const int y, const int z,
                         const tcu::Vec3 &srcNormCoord) const = 0;

    bool forEach(const void *pUserData, const TestParams &params, const int sourceWidth, const int sourceHeight,
                 const int sourceDepth, const tcu::PixelBufferAccess &errorMask) const
    {
        bool compareOk = true;

        for (std::vector<CopyRegion>::const_iterator regionIter = params.regions.begin();
             regionIter != params.regions.end(); ++regionIter)
        {
            const VkImageBlit &blit = regionIter->imageBlit;

            int32_t srcZ[2];
            int32_t dstZ[2];

            getUsedZRange(srcZ, params.src.image, blit.srcSubresource, blit.srcOffsets);
            getUsedZRange(dstZ, params.dst.image, blit.dstSubresource, blit.dstOffsets);

            const int xStart   = deMin32(blit.dstOffsets[0].x, blit.dstOffsets[1].x);
            const int yStart   = deMin32(blit.dstOffsets[0].y, blit.dstOffsets[1].y);
            const int zStart   = deMin32(dstZ[0], dstZ[1]);
            const int xEnd     = deMax32(blit.dstOffsets[0].x, blit.dstOffsets[1].x);
            const int yEnd     = deMax32(blit.dstOffsets[0].y, blit.dstOffsets[1].y);
            const int zEnd     = deMax32(dstZ[0], dstZ[1]);
            const float xScale = static_cast<float>(blit.srcOffsets[1].x - blit.srcOffsets[0].x) /
                                 static_cast<float>(blit.dstOffsets[1].x - blit.dstOffsets[0].x);
            const float yScale = static_cast<float>(blit.srcOffsets[1].y - blit.srcOffsets[0].y) /
                                 static_cast<float>(blit.dstOffsets[1].y - blit.dstOffsets[0].y);
            const float zScale  = static_cast<float>(srcZ[1] - srcZ[0]) / static_cast<float>(dstZ[1] - dstZ[0]);
            const float srcInvW = 1.0f / static_cast<float>(sourceWidth);
            const float srcInvH = 1.0f / static_cast<float>(sourceHeight);
            const float srcInvD = 1.0f / static_cast<float>(sourceDepth);

            for (int z = zStart; z < zEnd; z++)
                for (int y = yStart; y < yEnd; y++)
                    for (int x = xStart; x < xEnd; x++)
                    {
                        const tcu::Vec3 srcNormCoord(
                            (xScale * (static_cast<float>(x - blit.dstOffsets[0].x) + 0.5f) +
                             static_cast<float>(blit.srcOffsets[0].x)) *
                                srcInvW,
                            (yScale * (static_cast<float>(y - blit.dstOffsets[0].y) + 0.5f) +
                             static_cast<float>(blit.srcOffsets[0].y)) *
                                srcInvH,
                            (zScale * (static_cast<float>(z - dstZ[0]) + 0.5f) + static_cast<float>(srcZ[0])) *
                                srcInvD);

                        if (!compare(pUserData, x, y, z, srcNormCoord))
                        {
                            errorMask.setPixel(tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f), x, y, z);
                            compareOk = false;
                        }
                    }
        }
        return compareOk;
    }
};

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

bool BlittingImages::checkNearestFilteredResult(const tcu::ConstPixelBufferAccess &result,
                                                const tcu::ConstPixelBufferAccess &source)
{
    tcu::TestLog &log(m_context.getTestContext().getLog());
    const tcu::TextureFormat dstFormat             = result.getFormat();
    const tcu::TextureFormat srcFormat             = source.getFormat();
    const tcu::TextureChannelClass dstChannelClass = tcu::getTextureChannelClass(dstFormat.type);
    const tcu::TextureChannelClass srcChannelClass = tcu::getTextureChannelClass(srcFormat.type);

    tcu::TextureLevel errorMaskStorage(tcu::TextureFormat(tcu::TextureFormat::RGB, tcu::TextureFormat::UNORM_INT8),
                                       result.getWidth(), result.getHeight(), result.getDepth());
    tcu::PixelBufferAccess errorMask = errorMaskStorage.getAccess();
    tcu::Vec4 pixelBias(0.0f, 0.0f, 0.0f, 0.0f);
    tcu::Vec4 pixelScale(1.0f, 1.0f, 1.0f, 1.0f);
    bool ok = false;

    tcu::clear(errorMask, tcu::Vec4(0.0f, 1.0f, 0.0f, 1.0));

    // if either of srcImage or dstImage stores values as a signed/unsigned integer,
    // the other must also store values a signed/unsigned integer
    // e.g. blit unorm to uscaled is not allowed as uscaled formats store data as integers
    // despite the fact that both formats are sampled as floats
    bool dstImageIsIntClass = dstChannelClass == tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER ||
                              dstChannelClass == tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER;
    bool srcImageIsIntClass = srcChannelClass == tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER ||
                              srcChannelClass == tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER;
    if (dstImageIsIntClass != srcImageIsIntClass)
        return false;

    if (dstImageIsIntClass)
    {
        ok = intNearestBlitCompare(source, result, errorMask, m_params);
    }
    else
    {
        const tcu::Vec4 srcMaxDiff = getFloatOrFixedPointFormatThreshold(source.getFormat());
        const tcu::Vec4 dstMaxDiff = getFloatOrFixedPointFormatThreshold(result.getFormat());
        ok = floatNearestBlitCompare(source, result, srcMaxDiff, dstMaxDiff, errorMask, m_params);
    }

    if (result.getFormat() != tcu::TextureFormat(tcu::TextureFormat::RGBA, tcu::TextureFormat::UNORM_INT8))
        tcu::computePixelScaleBias(result, pixelScale, pixelBias);

    if (!ok)
    {
        log << tcu::TestLog::ImageSet("Compare", "Result comparsion")
            << tcu::TestLog::Image("Result", "Result", result, pixelScale, pixelBias)
            << tcu::TestLog::Image("ErrorMask", "Error mask", errorMask) << tcu::TestLog::EndImageSet;
    }
    else
    {
        log << tcu::TestLog::ImageSet("Compare", "Result comparsion")
            << tcu::TestLog::Image("Result", "Result", result, pixelScale, pixelBias) << tcu::TestLog::EndImageSet;
    }

    return ok;
}

bool BlittingImages::checkCompressedNearestFilteredResult(const tcu::ConstPixelBufferAccess &result,
                                                          const tcu::ConstPixelBufferAccess &source,
                                                          const tcu::CompressedTexFormat format)
{
    tcu::TestLog &log(m_context.getTestContext().getLog());
    tcu::TextureFormat errorMaskFormat(tcu::TextureFormat::RGB, tcu::TextureFormat::UNORM_INT8);
    tcu::TextureLevel errorMaskStorage(errorMaskFormat, result.getWidth(), result.getHeight(), result.getDepth());
    tcu::PixelBufferAccess errorMask(errorMaskStorage.getAccess());
    tcu::Vec4 pixelBias(0.0f, 0.0f, 0.0f, 0.0f);
    tcu::Vec4 pixelScale(1.0f, 1.0f, 1.0f, 1.0f);
    const tcu::TextureFormat &resultFormat(result.getFormat());
    VkFormat nativeResultFormat(mapTextureFormat(resultFormat));

    // there are rare cases wher one or few pixels have slightly bigger error
    // in one of channels this accepted error allows those casses to pass
    const tcu::Vec4 acceptedError(0.04f);
    const tcu::Vec4 srcMaxDiff(acceptedError + getCompressedFormatThreshold(format));
    const tcu::Vec4 dstMaxDiff(
        acceptedError +
        (m_destinationCompressedTexture ?
             getCompressedFormatThreshold(m_destinationCompressedTexture->getCompressedTexture().getFormat()) :
             getFloatOrFixedPointFormatThreshold(resultFormat)));

    tcu::TextureLevel clampedSourceLevel;
    bool clampSource(false);
    tcu::Vec4 clampSourceMinValue(-1.0f);
    tcu::Vec4 clampSourceMaxValue(1.0f);
    tcu::TextureLevel clampedResultLevel;
    bool clampResult(false);

    tcu::clear(errorMask, tcu::Vec4(0.0f, 1.0f, 0.0f, 1.0));

    if (resultFormat != tcu::TextureFormat(tcu::TextureFormat::RGBA, tcu::TextureFormat::UNORM_INT8))
        tcu::computePixelScaleBias(result, pixelScale, pixelBias);

    log << tcu::TestLog::ImageSet("Compare", "Result comparsion")
        << tcu::TestLog::Image("Result", "Result", result, pixelScale, pixelBias);

    // for compressed formats source buffer access is not actual compressed format
    // but equivalent uncompressed format that is some cases needs additional
    // modifications so that sampling it will produce valid reference
    if ((format == tcu::COMPRESSEDTEXFORMAT_BC6H_SFLOAT_BLOCK) ||
        (format == tcu::COMPRESSEDTEXFORMAT_BC6H_UFLOAT_BLOCK))
    {
        if (resultFormat.type == tcu::TextureFormat::UNSIGNED_INT_11F_11F_10F_REV)
        {
            // for compressed formats we are using random data and for some formats it
            // can be outside of <-1;1> range - for cases where result is not a float
            // format we need to clamp source to <-1;1> range as this will be done on
            // the device but not in software sampler in framework
            clampSource = true;
            // for this format we also need to clamp the result as precision of
            // this format is smaller then precision of calculations in framework;
            // the biger color valus are the bigger errors can be
            clampResult = true;

            if (format == tcu::COMPRESSEDTEXFORMAT_BC6H_SFLOAT_BLOCK)
                clampSourceMinValue = tcu::Vec4(0.0f);
        }
        else if ((resultFormat.type != tcu::TextureFormat::FLOAT) &&
                 (resultFormat.type != tcu::TextureFormat::HALF_FLOAT))
        {
            // clamp source for all non float formats
            clampSource = true;
        }
    }

    if (isUnormFormat(nativeResultFormat) || isUfloatFormat(nativeResultFormat))
    {
        // when tested compressed format is signed but the result format
        // is unsigned we need to clamp source to <0; x> so that proper
        // reference is calculated
        if ((format == tcu::COMPRESSEDTEXFORMAT_EAC_SIGNED_R11) ||
            (format == tcu::COMPRESSEDTEXFORMAT_EAC_SIGNED_RG11) ||
            (format == tcu::COMPRESSEDTEXFORMAT_BC4_SNORM_BLOCK) ||
            (format == tcu::COMPRESSEDTEXFORMAT_BC5_SNORM_BLOCK) ||
            (format == tcu::COMPRESSEDTEXFORMAT_BC6H_SFLOAT_BLOCK))
        {
            clampSource         = true;
            clampSourceMinValue = tcu::Vec4(0.0f);
        }
    }

    if (clampSource || clampResult)
    {
        if (clampSource)
        {
            clampedSourceLevel.setStorage(source.getFormat(), source.getWidth(), source.getHeight(), source.getDepth());
            tcu::PixelBufferAccess clampedSourceAcccess(clampedSourceLevel.getAccess());

            for (int32_t z = 0; z < source.getDepth(); z++)
                for (int32_t y = 0; y < source.getHeight(); y++)
                    for (int32_t x = 0; x < source.getWidth(); x++)
                    {
                        tcu::Vec4 texel = source.getPixel(x, y, z);
                        texel = tcu::clamp(texel, tcu::Vec4(clampSourceMinValue), tcu::Vec4(clampSourceMaxValue));
                        clampedSourceAcccess.setPixel(texel, x, y, z);
                    }
        }

        if (clampResult)
        {
            clampedResultLevel.setStorage(result.getFormat(), result.getWidth(), result.getHeight(), result.getDepth());
            tcu::PixelBufferAccess clampedResultAcccess(clampedResultLevel.getAccess());

            for (int32_t z = 0; z < result.getDepth(); z++)
                for (int32_t y = 0; y < result.getHeight(); y++)
                    for (int32_t x = 0; x < result.getWidth(); x++)
                    {
                        tcu::Vec4 texel = result.getPixel(x, y, z);
                        texel           = tcu::clamp(texel, tcu::Vec4(-1.0f), tcu::Vec4(1.0f));
                        clampedResultAcccess.setPixel(texel, x, y, z);
                    }
        }
    }

    const tcu::ConstPixelBufferAccess src = clampSource ? clampedSourceLevel.getAccess() : source;
    const tcu::ConstPixelBufferAccess res = clampResult ? clampedResultLevel.getAccess() : result;

    if (floatNearestBlitCompare(src, res, srcMaxDiff, dstMaxDiff, errorMask, m_params))
    {
        log << tcu::TestLog::EndImageSet;
        return true;
    }

    log << tcu::TestLog::Image("ErrorMask", "Error mask", errorMask) << tcu::TestLog::EndImageSet;
    return false;
}

tcu::TestStatus BlittingImages::checkTestResult(tcu::ConstPixelBufferAccess result)
{
    DE_ASSERT(m_params.filter == VK_FILTER_NEAREST || m_params.filter == VK_FILTER_LINEAR ||
              m_params.filter == VK_FILTER_CUBIC_EXT);
    const std::string failMessage("Result image is incorrect");

    if (m_params.filter != VK_FILTER_NEAREST)
    {
        if (tcu::isCombinedDepthStencilType(result.getFormat().type))
        {
            if (tcu::hasDepthComponent(result.getFormat().order))
            {
                const tcu::Sampler::DepthStencilMode mode     = tcu::Sampler::MODE_DEPTH;
                const tcu::ConstPixelBufferAccess depthResult = tcu::getEffectiveDepthStencilAccess(result, mode);
                const tcu::ConstPixelBufferAccess clampedExpected =
                    tcu::getEffectiveDepthStencilAccess(m_expectedTextureLevel[0]->getAccess(), mode);
                const tcu::ConstPixelBufferAccess unclampedExpected =
                    tcu::getEffectiveDepthStencilAccess(m_unclampedExpectedTextureLevel->getAccess(), mode);
                const tcu::TextureFormat sourceFormat =
                    tcu::getEffectiveDepthStencilTextureFormat(mapVkFormat(m_params.src.image.format), mode);

                if (!checkNonNearestFilteredResult(depthResult, clampedExpected, unclampedExpected, sourceFormat))
                    return tcu::TestStatus::fail(failMessage);
            }

            if (tcu::hasStencilComponent(result.getFormat().order))
            {
                const tcu::Sampler::DepthStencilMode mode       = tcu::Sampler::MODE_STENCIL;
                const tcu::ConstPixelBufferAccess stencilResult = tcu::getEffectiveDepthStencilAccess(result, mode);
                const tcu::ConstPixelBufferAccess clampedExpected =
                    tcu::getEffectiveDepthStencilAccess(m_expectedTextureLevel[0]->getAccess(), mode);
                const tcu::ConstPixelBufferAccess unclampedExpected =
                    tcu::getEffectiveDepthStencilAccess(m_unclampedExpectedTextureLevel->getAccess(), mode);
                const tcu::TextureFormat sourceFormat =
                    tcu::getEffectiveDepthStencilTextureFormat(mapVkFormat(m_params.src.image.format), mode);

                if (!checkNonNearestFilteredResult(stencilResult, clampedExpected, unclampedExpected, sourceFormat))
                    return tcu::TestStatus::fail(failMessage);
            }
        }
        else if (m_sourceCompressedTexture)
        {
            const tcu::CompressedTexture &compressedLevel = m_sourceCompressedTexture->getCompressedTexture();
            if (!checkCompressedNonNearestFilteredResult(result, m_expectedTextureLevel[0]->getAccess(),
                                                         m_unclampedExpectedTextureLevel->getAccess(),
                                                         compressedLevel.getFormat()))
                return tcu::TestStatus::fail(failMessage);
        }
        else
        {
            const tcu::TextureFormat sourceFormat = mapVkFormat(m_params.src.image.format);
            if (!checkNonNearestFilteredResult(result, m_expectedTextureLevel[0]->getAccess(),
                                               m_unclampedExpectedTextureLevel->getAccess(), sourceFormat))
                return tcu::TestStatus::fail(failMessage);
        }
    }
    else // NEAREST filtering
    {
        if (tcu::isCombinedDepthStencilType(result.getFormat().type))
        {
            if (tcu::hasDepthComponent(result.getFormat().order))
            {
                const tcu::Sampler::DepthStencilMode mode     = tcu::Sampler::MODE_DEPTH;
                const tcu::ConstPixelBufferAccess depthResult = tcu::getEffectiveDepthStencilAccess(result, mode);
                const tcu::ConstPixelBufferAccess depthSource =
                    tcu::getEffectiveDepthStencilAccess(m_sourceTextureLevel->getAccess(), mode);

                if (!checkNearestFilteredResult(depthResult, depthSource))
                    return tcu::TestStatus::fail(failMessage);
            }

            if (tcu::hasStencilComponent(result.getFormat().order))
            {
                const tcu::Sampler::DepthStencilMode mode       = tcu::Sampler::MODE_STENCIL;
                const tcu::ConstPixelBufferAccess stencilResult = tcu::getEffectiveDepthStencilAccess(result, mode);
                const tcu::ConstPixelBufferAccess stencilSource =
                    tcu::getEffectiveDepthStencilAccess(m_sourceTextureLevel->getAccess(), mode);

                if (!checkNearestFilteredResult(stencilResult, stencilSource))
                    return tcu::TestStatus::fail(failMessage);
            }
        }
        else if (m_sourceCompressedTexture)
        {
            const tcu::CompressedTexture &compressedLevel   = m_sourceCompressedTexture->getCompressedTexture();
            const tcu::PixelBufferAccess &decompressedLevel = m_sourceCompressedTexture->getDecompressedAccess();

            if (!checkCompressedNearestFilteredResult(result, decompressedLevel, compressedLevel.getFormat()))
                return tcu::TestStatus::fail(failMessage);
        }
        else if (!checkNearestFilteredResult(result, m_sourceTextureLevel->getAccess()))
            return tcu::TestStatus::fail(failMessage);
    }

    return tcu::TestStatus::pass("Pass");
}

tcu::Vec4 linearToSRGBIfNeeded(const tcu::TextureFormat &format, const tcu::Vec4 &color)
{
    return isSRGB(format) ? linearToSRGB(color) : color;
}

void scaleFromWholeSrcBuffer(const tcu::PixelBufferAccess &dst, const tcu::ConstPixelBufferAccess &src,
                             const VkOffset3D regionOffset, const VkOffset3D regionExtent,
                             tcu::Sampler::FilterMode filter, const MirrorMode mirrorMode = 0u)
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

// Mirror X, Y and Z as required by the offset values in the 3 axes.
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

// Mirror the axes that are mirrored either in the source or destination, but not both.
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

void BlittingImages::copyRegionToTextureLevel(tcu::ConstPixelBufferAccess src, tcu::PixelBufferAccess dst,
                                              CopyRegion region, uint32_t mipLevel)
{
    DE_UNREF(mipLevel);

    const MirrorMode mirrorMode = getMirrorMode(region.imageBlit.srcOffsets[0], region.imageBlit.srcOffsets[1],
                                                region.imageBlit.dstOffsets[0], region.imageBlit.dstOffsets[1]);

    flipCoordinates(region, mirrorMode);

    const VkOffset3D srcOffset = region.imageBlit.srcOffsets[0];
    const VkOffset3D srcExtent = {
        region.imageBlit.srcOffsets[1].x - srcOffset.x,
        region.imageBlit.srcOffsets[1].y - srcOffset.y,
        region.imageBlit.srcOffsets[1].z - srcOffset.z,
    };
    const VkOffset3D dstOffset = region.imageBlit.dstOffsets[0];
    const VkOffset3D dstExtent = {
        region.imageBlit.dstOffsets[1].x - dstOffset.x,
        region.imageBlit.dstOffsets[1].y - dstOffset.y,
        region.imageBlit.dstOffsets[1].z - dstOffset.z,
    };

    tcu::Sampler::FilterMode filter;
    switch (m_params.filter)
    {
    case VK_FILTER_LINEAR:
        filter = tcu::Sampler::LINEAR;
        break;
    case VK_FILTER_CUBIC_EXT:
        filter = tcu::Sampler::CUBIC;
        break;
    case VK_FILTER_NEAREST:
    default:
        filter = tcu::Sampler::NEAREST;
        break;
    }

    if (tcu::isCombinedDepthStencilType(src.getFormat().type))
    {
        DE_ASSERT(src.getFormat() == dst.getFormat());

        // Scale depth.
        if (tcu::hasDepthComponent(src.getFormat().order))
        {
            const tcu::ConstPixelBufferAccess srcSubRegion = getEffectiveDepthStencilAccess(
                tcu::getSubregion(src, srcOffset.x, srcOffset.y, srcOffset.z, srcExtent.x, srcExtent.y, srcExtent.z),
                tcu::Sampler::MODE_DEPTH);
            const tcu::PixelBufferAccess dstSubRegion = getEffectiveDepthStencilAccess(
                tcu::getSubregion(dst, dstOffset.x, dstOffset.y, dstOffset.z, dstExtent.x, dstExtent.y, dstExtent.z),
                tcu::Sampler::MODE_DEPTH);
            tcu::scale(dstSubRegion, srcSubRegion, filter);

            if (filter != tcu::Sampler::NEAREST)
            {
                const tcu::ConstPixelBufferAccess depthSrc =
                    getEffectiveDepthStencilAccess(src, tcu::Sampler::MODE_DEPTH);
                const tcu::PixelBufferAccess unclampedSubRegion = getEffectiveDepthStencilAccess(
                    tcu::getSubregion(m_unclampedExpectedTextureLevel->getAccess(), dstOffset.x, dstOffset.y,
                                      dstOffset.z, dstExtent.x, dstExtent.y, dstExtent.z),
                    tcu::Sampler::MODE_DEPTH);
                scaleFromWholeSrcBuffer(unclampedSubRegion, depthSrc, srcOffset, srcExtent, filter, mirrorMode);
            }
        }

        // Scale stencil.
        if (tcu::hasStencilComponent(src.getFormat().order))
        {
            const tcu::ConstPixelBufferAccess srcSubRegion = getEffectiveDepthStencilAccess(
                tcu::getSubregion(src, srcOffset.x, srcOffset.y, srcOffset.z, srcExtent.x, srcExtent.y, srcExtent.z),
                tcu::Sampler::MODE_STENCIL);
            const tcu::PixelBufferAccess dstSubRegion = getEffectiveDepthStencilAccess(
                tcu::getSubregion(dst, dstOffset.x, dstOffset.y, dstOffset.z, dstExtent.x, dstExtent.y, dstExtent.z),
                tcu::Sampler::MODE_STENCIL);
            blit(dstSubRegion, srcSubRegion, filter, mirrorMode);

            if (filter != tcu::Sampler::NEAREST)
            {
                const tcu::ConstPixelBufferAccess stencilSrc =
                    getEffectiveDepthStencilAccess(src, tcu::Sampler::MODE_STENCIL);
                const tcu::PixelBufferAccess unclampedSubRegion = getEffectiveDepthStencilAccess(
                    tcu::getSubregion(m_unclampedExpectedTextureLevel->getAccess(), dstOffset.x, dstOffset.y,
                                      dstOffset.z, dstExtent.x, dstExtent.y, dstExtent.z),
                    tcu::Sampler::MODE_STENCIL);
                scaleFromWholeSrcBuffer(unclampedSubRegion, stencilSrc, srcOffset, srcExtent, filter, mirrorMode);
            }
        }
    }
    else
    {
        const tcu::ConstPixelBufferAccess srcSubRegion =
            tcu::getSubregion(src, srcOffset.x, srcOffset.y, srcOffset.z, srcExtent.x, srcExtent.y, srcExtent.z);
        const tcu::PixelBufferAccess dstSubRegion =
            tcu::getSubregion(dst, dstOffset.x, dstOffset.y, dstOffset.z, dstExtent.x, dstExtent.y, dstExtent.z);
        blit(dstSubRegion, srcSubRegion, filter, mirrorMode);

        if (filter != tcu::Sampler::NEAREST)
        {
            const tcu::PixelBufferAccess unclampedSubRegion =
                tcu::getSubregion(m_unclampedExpectedTextureLevel->getAccess(), dstOffset.x, dstOffset.y, dstOffset.z,
                                  dstExtent.x, dstExtent.y, dstExtent.z);
            scaleFromWholeSrcBuffer(unclampedSubRegion, src, srcOffset, srcExtent, filter, mirrorMode);
        }
    }
}

void BlittingImages::generateExpectedResult(void)
{
    const tcu::ConstPixelBufferAccess src = m_sourceCompressedTexture ?
                                                m_sourceCompressedTexture->getDecompressedAccess() :
                                                m_sourceTextureLevel->getAccess();
    const tcu::ConstPixelBufferAccess dst = m_destinationCompressedTexture ?
                                                m_destinationCompressedTexture->getDecompressedAccess() :
                                                m_destinationTextureLevel->getAccess();

    m_expectedTextureLevel[0] = de::MovePtr<tcu::TextureLevel>(
        new tcu::TextureLevel(dst.getFormat(), dst.getWidth(), dst.getHeight(), dst.getDepth()));
    tcu::copy(m_expectedTextureLevel[0]->getAccess(), dst);

    if (m_params.filter != VK_FILTER_NEAREST)
    {
        m_unclampedExpectedTextureLevel = de::MovePtr<tcu::TextureLevel>(
            new tcu::TextureLevel(dst.getFormat(), dst.getWidth(), dst.getHeight(), dst.getDepth()));
        tcu::copy(m_unclampedExpectedTextureLevel->getAccess(), dst);
    }

    for (uint32_t i = 0; i < m_params.regions.size(); i++)
    {
        CopyRegion region = m_params.regions[i];
        copyRegionToTextureLevel(src, m_expectedTextureLevel[0]->getAccess(), region);
    }
}

void BlittingImages::uploadCompressedImage(const VkImage &image, const ImageParms &parms)
{
    DE_ASSERT(m_sourceCompressedTexture);

    const InstanceInterface &vki        = m_context.getInstanceInterface();
    const DeviceInterface &vk           = m_context.getDeviceInterface();
    const VkPhysicalDevice vkPhysDevice = m_context.getPhysicalDevice();
    const VkDevice vkDevice             = m_device;
    Allocator &memAlloc                 = *m_allocator;
    Move<VkBuffer> buffer;
    const uint32_t bufferSize = m_sourceCompressedTexture->getCompressedTexture().getDataSize();
    de::MovePtr<Allocation> bufferAlloc;
    const uint32_t arraySize = getArraySize(parms);
    const VkExtent3D imageExtent{
        parms.extent.width,
        (parms.imageType != VK_IMAGE_TYPE_1D) ? parms.extent.height : 1u,
        (parms.imageType == VK_IMAGE_TYPE_3D) ? parms.extent.depth : 1u,
    };

    // Create source buffer
    {
        const VkBufferCreateInfo bufferParams{
            VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, // VkStructureType sType;
            DE_NULL,                              // const void* pNext;
            0u,                                   // VkBufferCreateFlags flags;
            bufferSize,                           // VkDeviceSize size;
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,     // VkBufferUsageFlags usage;
            VK_SHARING_MODE_EXCLUSIVE,            // VkSharingMode sharingMode;
            0u,                                   // uint32_t queueFamilyIndexCount;
            (const uint32_t *)DE_NULL,            // const uint32_t* pQueueFamilyIndices;
        };

        buffer      = createBuffer(vk, vkDevice, &bufferParams);
        bufferAlloc = allocateBuffer(vki, vk, vkPhysDevice, vkDevice, *buffer, MemoryRequirement::HostVisible, memAlloc,
                                     m_params.allocationKind);
        VK_CHECK(vk.bindBufferMemory(vkDevice, *buffer, bufferAlloc->getMemory(), bufferAlloc->getOffset()));
    }

    // Barriers for copying buffer to image
    const VkBufferMemoryBarrier preBufferBarrier{
        VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER, // VkStructureType sType;
        DE_NULL,                                 // const void* pNext;
        VK_ACCESS_HOST_WRITE_BIT,                // VkAccessFlags srcAccessMask;
        VK_ACCESS_TRANSFER_READ_BIT,             // VkAccessFlags dstAccessMask;
        VK_QUEUE_FAMILY_IGNORED,                 // uint32_t srcQueueFamilyIndex;
        VK_QUEUE_FAMILY_IGNORED,                 // uint32_t dstQueueFamilyIndex;
        *buffer,                                 // VkBuffer buffer;
        0u,                                      // VkDeviceSize offset;
        bufferSize                               // VkDeviceSize size;
    };

    const VkImageMemoryBarrier preImageBarrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, // VkStructureType sType;
                                               DE_NULL,                                // const void* pNext;
                                               0u,                                     // VkAccessFlags srcAccessMask;
                                               VK_ACCESS_TRANSFER_WRITE_BIT,           // VkAccessFlags dstAccessMask;
                                               VK_IMAGE_LAYOUT_UNDEFINED,              // VkImageLayout oldLayout;
                                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,   // VkImageLayout newLayout;
                                               VK_QUEUE_FAMILY_IGNORED,                // uint32_t srcQueueFamilyIndex;
                                               VK_QUEUE_FAMILY_IGNORED,                // uint32_t dstQueueFamilyIndex;
                                               image,                                  // VkImage image;
                                               {
                                                   // VkImageSubresourceRange subresourceRange;
                                                   VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspect;
                                                   0u,                        // uint32_t baseMipLevel;
                                                   1u,                        // uint32_t mipLevels;
                                                   0u,                        // uint32_t baseArraySlice;
                                                   arraySize,                 // uint32_t arraySize;
                                               }};

    const VkImageMemoryBarrier postImageBarrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, // VkStructureType sType;
                                                DE_NULL,                                // const void* pNext;
                                                VK_ACCESS_TRANSFER_WRITE_BIT,           // VkAccessFlags srcAccessMask;
                                                VK_ACCESS_TRANSFER_WRITE_BIT,           // VkAccessFlags dstAccessMask;
                                                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,   // VkImageLayout oldLayout;
                                                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,   // VkImageLayout newLayout;
                                                VK_QUEUE_FAMILY_IGNORED,                // uint32_t srcQueueFamilyIndex;
                                                VK_QUEUE_FAMILY_IGNORED,                // uint32_t dstQueueFamilyIndex;
                                                image,                                  // VkImage image;
                                                {
                                                    // VkImageSubresourceRange subresourceRange;
                                                    VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspect;
                                                    0u,                        // uint32_t baseMipLevel;
                                                    1u,                        // uint32_t mipLevels;
                                                    0u,                        // uint32_t baseArraySlice;
                                                    arraySize,                 // uint32_t arraySize;
                                                }};

    const VkExtent3D copyExtent{imageExtent.width, imageExtent.height, imageExtent.depth};

    VkBufferImageCopy copyRegion{
        0u,                // VkDeviceSize bufferOffset;
        copyExtent.width,  // uint32_t bufferRowLength;
        copyExtent.height, // uint32_t bufferImageHeight;
        {
            VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspect;
            0u,                        // uint32_t mipLevel;
            0u,                        // uint32_t baseArrayLayer;
            arraySize,                 // uint32_t layerCount;
        },                             // VkImageSubresourceLayers imageSubresource;
        {0, 0, 0},                     // VkOffset3D imageOffset;
        copyExtent                     // VkExtent3D imageExtent;
    };

    // Write buffer data
    deMemcpy(bufferAlloc->getHostPtr(), m_sourceCompressedTexture->getCompressedTexture().getData(), bufferSize);
    flushAlloc(vk, vkDevice, *bufferAlloc);

    // Copy buffer to image
    beginCommandBuffer(vk, *m_universalCmdBuffer);
    vk.cmdPipelineBarrier(*m_universalCmdBuffer, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                          (VkDependencyFlags)0, 0, (const VkMemoryBarrier *)DE_NULL, 1, &preBufferBarrier, 1,
                          &preImageBarrier);
    vk.cmdCopyBufferToImage(*m_universalCmdBuffer, *buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1u,
                            &copyRegion);
    vk.cmdPipelineBarrier(*m_universalCmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                          (VkDependencyFlags)0, 0, (const VkMemoryBarrier *)DE_NULL, 0,
                          (const VkBufferMemoryBarrier *)DE_NULL, 1, &postImageBarrier);
    endCommandBuffer(vk, *m_universalCmdBuffer);

    submitCommandsAndWait(vk, vkDevice, m_universalQueue, *m_universalCmdBuffer);
    m_context.resetCommandPoolForVKSC(vkDevice, *m_universalCmdPool);
}

class BlitImageTestCase : public vkt::TestCase
{
public:
    BlitImageTestCase(tcu::TestContext &testCtx, const std::string &name, const TestParams params)
        : vkt::TestCase(testCtx, name)
        , m_params(params)
    {
    }

    virtual TestInstance *createInstance(Context &context) const
    {
        return new BlittingImages(context, m_params);
    }

    virtual void checkSupport(Context &context) const
    {

#ifndef CTS_USES_VULKANSC
        if (m_params.src.image.format == VK_FORMAT_A8_UNORM_KHR ||
            m_params.dst.image.format == VK_FORMAT_A8_UNORM_KHR ||
            m_params.src.image.format == VK_FORMAT_A1B5G5R5_UNORM_PACK16_KHR ||
            m_params.dst.image.format == VK_FORMAT_A1B5G5R5_UNORM_PACK16_KHR)
            context.requireDeviceFunctionality("VK_KHR_maintenance5");
#endif // CTS_USES_VULKANSC

        VkImageFormatProperties properties;
        if (context.getInstanceInterface().getPhysicalDeviceImageFormatProperties(
                context.getPhysicalDevice(), m_params.src.image.format, m_params.src.image.imageType,
                m_params.src.image.tiling, VK_IMAGE_USAGE_TRANSFER_SRC_BIT, 0,
                &properties) == VK_ERROR_FORMAT_NOT_SUPPORTED)
        {
            TCU_THROW(NotSupportedError, "Source format not supported");
        }
        if (context.getInstanceInterface().getPhysicalDeviceImageFormatProperties(
                context.getPhysicalDevice(), m_params.dst.image.format, m_params.dst.image.imageType,
                m_params.dst.image.tiling, VK_IMAGE_USAGE_TRANSFER_DST_BIT, 0,
                &properties) == VK_ERROR_FORMAT_NOT_SUPPORTED)
        {
            TCU_THROW(NotSupportedError, "Destination format not supported");
        }

        checkExtensionSupport(context, m_params.extensionFlags);

        VkFormatProperties srcFormatProperties;
        context.getInstanceInterface().getPhysicalDeviceFormatProperties(
            context.getPhysicalDevice(), m_params.src.image.format, &srcFormatProperties);
        VkFormatFeatureFlags srcFormatFeatures = m_params.src.image.tiling == VK_IMAGE_TILING_LINEAR ?
                                                     srcFormatProperties.linearTilingFeatures :
                                                     srcFormatProperties.optimalTilingFeatures;
        if (!(srcFormatFeatures & VK_FORMAT_FEATURE_BLIT_SRC_BIT))
        {
            TCU_THROW(NotSupportedError, "Format feature blit source not supported");
        }

        VkFormatProperties dstFormatProperties;
        context.getInstanceInterface().getPhysicalDeviceFormatProperties(
            context.getPhysicalDevice(), m_params.dst.image.format, &dstFormatProperties);
        VkFormatFeatureFlags dstFormatFeatures = m_params.dst.image.tiling == VK_IMAGE_TILING_LINEAR ?
                                                     dstFormatProperties.linearTilingFeatures :
                                                     dstFormatProperties.optimalTilingFeatures;
        if (!(dstFormatFeatures & VK_FORMAT_FEATURE_BLIT_DST_BIT))
        {
            TCU_THROW(NotSupportedError, "Format feature blit destination not supported");
        }

        if (m_params.filter == VK_FILTER_LINEAR &&
            !(srcFormatFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT))
        {
            TCU_THROW(NotSupportedError, "Source format feature sampled image filter linear not supported");
        }

        if (m_params.filter == VK_FILTER_CUBIC_EXT)
        {
            context.requireDeviceFunctionality("VK_EXT_filter_cubic");

            if (!(srcFormatFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_CUBIC_BIT_EXT))
            {
                TCU_THROW(NotSupportedError, "Source format feature sampled image filter cubic not supported");
            }
        }

        checkExtensionSupport(context, m_params.extensionFlags);
    }

private:
    TestParams m_params;
};

class BlittingMipmaps : public CopiesAndBlittingTestInstance
{
public:
    BlittingMipmaps(Context &context, TestParams params);
    virtual tcu::TestStatus iterate(void);

protected:
    virtual tcu::TestStatus checkTestResult(tcu::ConstPixelBufferAccess result = tcu::ConstPixelBufferAccess());
    virtual void copyRegionToTextureLevel(tcu::ConstPixelBufferAccess src, tcu::PixelBufferAccess dst,
                                          CopyRegion region, uint32_t mipLevel = 0u);
    virtual void generateExpectedResult(void);

private:
    bool checkNonNearestFilteredResult(void);
    bool checkNearestFilteredResult(void);

    Move<VkImage> m_source;
    de::MovePtr<Allocation> m_sourceImageAlloc;
    Move<VkImage> m_destination;
    de::MovePtr<Allocation> m_destinationImageAlloc;
    std::vector<de::SharedPtr<Allocation>> m_sparseAllocations;
    Move<VkSemaphore> m_sparseSemaphore;

    de::MovePtr<tcu::TextureLevel> m_unclampedExpectedTextureLevel[16];
};

BlittingMipmaps::BlittingMipmaps(Context &context, TestParams params) : CopiesAndBlittingTestInstance(context, params)
{
    const InstanceInterface &vki        = context.getInstanceInterface();
    const DeviceInterface &vk           = context.getDeviceInterface();
    const VkPhysicalDevice vkPhysDevice = context.getPhysicalDevice();
    const VkDevice vkDevice             = m_device;
    Allocator &memAlloc                 = context.getDefaultAllocator();

    // Create source image
    {
        VkImageCreateInfo sourceImageParams = {
            VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,                               // VkStructureType sType;
            DE_NULL,                                                           // const void* pNext;
            getCreateFlags(m_params.src.image),                                // VkImageCreateFlags flags;
            m_params.src.image.imageType,                                      // VkImageType imageType;
            m_params.src.image.format,                                         // VkFormat format;
            getExtent3D(m_params.src.image),                                   // VkExtent3D extent;
            1u,                                                                // uint32_t mipLevels;
            getArraySize(m_params.src.image),                                  // uint32_t arraySize;
            VK_SAMPLE_COUNT_1_BIT,                                             // uint32_t samples;
            VK_IMAGE_TILING_OPTIMAL,                                           // VkImageTiling tiling;
            VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, // VkImageUsageFlags usage;
            VK_SHARING_MODE_EXCLUSIVE,                                         // VkSharingMode sharingMode;
            0u,                                                                // uint32_t queueFamilyIndexCount;
            (const uint32_t *)DE_NULL,                                         // const uint32_t* pQueueFamilyIndices;
            VK_IMAGE_LAYOUT_UNDEFINED,                                         // VkImageLayout initialLayout;
        };

#ifndef CTS_USES_VULKANSC
        if (!params.useSparseBinding)
        {
#endif
            m_source           = createImage(vk, m_device, &sourceImageParams);
            m_sourceImageAlloc = allocateImage(vki, vk, vkPhysDevice, m_device, *m_source, MemoryRequirement::Any,
                                               *m_allocator, m_params.allocationKind, 0u);
            VK_CHECK(vk.bindImageMemory(m_device, *m_source, m_sourceImageAlloc->getMemory(),
                                        m_sourceImageAlloc->getOffset()));
#ifndef CTS_USES_VULKANSC
        }
        else
        {
            sourceImageParams.flags |=
                (vk::VK_IMAGE_CREATE_SPARSE_BINDING_BIT | vk::VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT);
            vk::VkImageFormatProperties imageFormatProperties;
            if (vki.getPhysicalDeviceImageFormatProperties(vkPhysDevice, sourceImageParams.format,
                                                           sourceImageParams.imageType, sourceImageParams.tiling,
                                                           sourceImageParams.usage, sourceImageParams.flags,
                                                           &imageFormatProperties) == vk::VK_ERROR_FORMAT_NOT_SUPPORTED)
            {
                TCU_THROW(NotSupportedError, "Image format not supported");
            }
            m_source = createImage(
                vk, m_device,
                &sourceImageParams); //de::MovePtr<SparseImage>(new SparseImage(vk, vk, vkPhysDevice, vki, sourceImageParams, m_queue, *m_allocator, mapVkFormat(sourceImageParams.format)));
            m_sparseSemaphore = createSemaphore(vk, m_device);
            allocateAndBindSparseImage(vk, m_device, vkPhysDevice, vki, sourceImageParams, m_sparseSemaphore.get(),
                                       context.getSparseQueue(), *m_allocator, m_sparseAllocations,
                                       mapVkFormat(sourceImageParams.format), m_source.get());
        }
#endif
    }

    // Create destination image
    {
        const VkImageCreateInfo destinationImageParams = {
            VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,                               // VkStructureType sType;
            DE_NULL,                                                           // const void* pNext;
            getCreateFlags(m_params.dst.image),                                // VkImageCreateFlags flags;
            m_params.dst.image.imageType,                                      // VkImageType imageType;
            m_params.dst.image.format,                                         // VkFormat format;
            getExtent3D(m_params.dst.image),                                   // VkExtent3D extent;
            m_params.mipLevels,                                                // uint32_t mipLevels;
            getArraySize(m_params.dst.image),                                  // uint32_t arraySize;
            VK_SAMPLE_COUNT_1_BIT,                                             // uint32_t samples;
            VK_IMAGE_TILING_OPTIMAL,                                           // VkImageTiling tiling;
            VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, // VkImageUsageFlags usage;
            VK_SHARING_MODE_EXCLUSIVE,                                         // VkSharingMode sharingMode;
            0u,                                                                // uint32_t queueFamilyIndexCount;
            (const uint32_t *)DE_NULL,                                         // const uint32_t* pQueueFamilyIndices;
            VK_IMAGE_LAYOUT_UNDEFINED,                                         // VkImageLayout initialLayout;
        };

        m_destination           = createImage(vk, vkDevice, &destinationImageParams);
        m_destinationImageAlloc = allocateImage(vki, vk, vkPhysDevice, vkDevice, *m_destination, MemoryRequirement::Any,
                                                memAlloc, m_params.allocationKind, 0u);
        VK_CHECK(vk.bindImageMemory(vkDevice, *m_destination, m_destinationImageAlloc->getMemory(),
                                    m_destinationImageAlloc->getOffset()));
    }
}

tcu::TestStatus BlittingMipmaps::iterate(void)
{
    const tcu::TextureFormat srcTcuFormat = mapVkFormat(m_params.src.image.format);
    const tcu::TextureFormat dstTcuFormat = mapVkFormat(m_params.dst.image.format);
    m_sourceTextureLevel                  = de::MovePtr<tcu::TextureLevel>(
        new tcu::TextureLevel(srcTcuFormat, m_params.src.image.extent.width, m_params.src.image.extent.height,
                                               m_params.src.image.extent.depth));
    generateBuffer(m_sourceTextureLevel->getAccess(), m_params.src.image.extent.width, m_params.src.image.extent.height,
                   m_params.src.image.extent.depth, m_params.src.image.fillMode);
    m_destinationTextureLevel = de::MovePtr<tcu::TextureLevel>(
        new tcu::TextureLevel(dstTcuFormat, (int)m_params.dst.image.extent.width, (int)m_params.dst.image.extent.height,
                              (int)m_params.dst.image.extent.depth));
    generateBuffer(m_destinationTextureLevel->getAccess(), m_params.dst.image.extent.width,
                   m_params.dst.image.extent.height, m_params.dst.image.extent.depth, m_params.dst.image.fillMode);
    generateExpectedResult();

    uploadImage(m_sourceTextureLevel->getAccess(), m_source.get(), m_params.src.image);

    uploadImage(m_destinationTextureLevel->getAccess(), m_destination.get(), m_params.dst.image, m_params.mipLevels);

    const DeviceInterface &vk = m_context.getDeviceInterface();
    const VkDevice vkDevice   = m_device;

    std::vector<VkImageBlit> regions;
    std::vector<VkImageBlit2KHR> regions2KHR;
    for (uint32_t i = 0; i < m_params.regions.size(); i++)
    {
        if (!(m_params.extensionFlags & COPY_COMMANDS_2))
        {
            regions.push_back(m_params.regions[i].imageBlit);
        }
        else
        {
            DE_ASSERT(m_params.extensionFlags & COPY_COMMANDS_2);
            regions2KHR.push_back(convertvkImageBlitTovkImageBlit2KHR(m_params.regions[i].imageBlit));
        }
    }

    // Copy source image to mip level 0 when generating mipmaps with multiple blit commands
    if (!m_params.singleCommand)
        uploadImage(m_sourceTextureLevel->getAccess(), m_destination.get(), m_params.dst.image, 1u);

    beginCommandBuffer(vk, *m_universalCmdBuffer);

    // Blit all mip levels with a single blit command
    if (m_params.singleCommand)
    {
        {
            // Source image layout
            const VkImageMemoryBarrier srcImageBarrier = {
                VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, // VkStructureType sType;
                DE_NULL,                                // const void* pNext;
                VK_ACCESS_TRANSFER_WRITE_BIT,           // VkAccessFlags srcAccessMask;
                VK_ACCESS_TRANSFER_READ_BIT,            // VkAccessFlags dstAccessMask;
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,   // VkImageLayout oldLayout;
                m_params.src.image.operationLayout,     // VkImageLayout newLayout;
                VK_QUEUE_FAMILY_IGNORED,                // uint32_t srcQueueFamilyIndex;
                VK_QUEUE_FAMILY_IGNORED,                // uint32_t dstQueueFamilyIndex;
                m_source.get(),                         // VkImage image;
                {
                    // VkImageSubresourceRange subresourceRange;
                    getAspectFlags(srcTcuFormat),    // VkImageAspectFlags   aspectMask;
                    0u,                              // uint32_t baseMipLevel;
                    1u,                              // uint32_t mipLevels;
                    0u,                              // uint32_t baseArraySlice;
                    getArraySize(m_params.src.image) // uint32_t arraySize;
                }};

            // Destination image layout
            const VkImageMemoryBarrier dstImageBarrier = {
                VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, // VkStructureType sType;
                DE_NULL,                                // const void* pNext;
                VK_ACCESS_TRANSFER_WRITE_BIT,           // VkAccessFlags srcAccessMask;
                VK_ACCESS_TRANSFER_WRITE_BIT,           // VkAccessFlags dstAccessMask;
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,   // VkImageLayout oldLayout;
                m_params.dst.image.operationLayout,     // VkImageLayout newLayout;
                VK_QUEUE_FAMILY_IGNORED,                // uint32_t srcQueueFamilyIndex;
                VK_QUEUE_FAMILY_IGNORED,                // uint32_t dstQueueFamilyIndex;
                m_destination.get(),                    // VkImage image;
                {
                    // VkImageSubresourceRange subresourceRange;
                    getAspectFlags(dstTcuFormat),    // VkImageAspectFlags   aspectMask;
                    0u,                              // uint32_t baseMipLevel;
                    m_params.mipLevels,              // uint32_t mipLevels;
                    0u,                              // uint32_t baseArraySlice;
                    getArraySize(m_params.dst.image) // uint32_t arraySize;
                }};

            vk.cmdPipelineBarrier(*m_universalCmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                  (VkDependencyFlags)0, 0, (const VkMemoryBarrier *)DE_NULL, 0,
                                  (const VkBufferMemoryBarrier *)DE_NULL, 1, &srcImageBarrier);
            vk.cmdPipelineBarrier(*m_universalCmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                  (VkDependencyFlags)0, 0, (const VkMemoryBarrier *)DE_NULL, 0,
                                  (const VkBufferMemoryBarrier *)DE_NULL, 1, &dstImageBarrier);

            if (!(m_params.extensionFlags & COPY_COMMANDS_2))
            {
                vk.cmdBlitImage(*m_universalCmdBuffer, m_source.get(), m_params.src.image.operationLayout,
                                m_destination.get(), m_params.dst.image.operationLayout,
                                (uint32_t)m_params.regions.size(), &regions[0], m_params.filter);
            }
            else
            {
                DE_ASSERT(m_params.extensionFlags & COPY_COMMANDS_2);
                const VkBlitImageInfo2KHR BlitImageInfo2KHR = {
                    VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2_KHR, // VkStructureType sType;
                    DE_NULL,                                 // const void* pNext;
                    m_source.get(),                          // VkImage srcImage;
                    m_params.src.image.operationLayout,      // VkImageLayout srcImageLayout;
                    m_destination.get(),                     // VkImage dstImage;
                    m_params.dst.image.operationLayout,      // VkImageLayout dstImageLayout;
                    (uint32_t)m_params.regions.size(),       // uint32_t regionCount;
                    &regions2KHR[0],                         // const VkImageBlit2KHR* pRegions;
                    m_params.filter                          // VkFilter filter;
                };
                vk.cmdBlitImage2(*m_universalCmdBuffer, &BlitImageInfo2KHR);
            }
        }
    }
    // Blit mip levels with multiple blit commands
    else
    {
        // Prepare all mip levels for reading
        {
            for (uint32_t barrierno = 0; barrierno < m_params.barrierCount; barrierno++)
            {
                VkImageMemoryBarrier preImageBarrier = {
                    VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, // VkStructureType sType;
                    DE_NULL,                                // const void* pNext;
                    VK_ACCESS_TRANSFER_WRITE_BIT,           // VkAccessFlags srcAccessMask;
                    VK_ACCESS_TRANSFER_READ_BIT,            // VkAccessFlags dstAccessMask;
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,   // VkImageLayout oldLayout;
                    m_params.src.image.operationLayout,     // VkImageLayout newLayout;
                    VK_QUEUE_FAMILY_IGNORED,                // uint32_t srcQueueFamilyIndex;
                    VK_QUEUE_FAMILY_IGNORED,                // uint32_t dstQueueFamilyIndex;
                    m_destination.get(),                    // VkImage image;
                    {
                        // VkImageSubresourceRange subresourceRange;
                        getAspectFlags(dstTcuFormat),    // VkImageAspectFlags aspectMask;
                        0u,                              // uint32_t baseMipLevel;
                        VK_REMAINING_MIP_LEVELS,         // uint32_t mipLevels;
                        0u,                              // uint32_t baseArraySlice;
                        getArraySize(m_params.src.image) // uint32_t arraySize;
                    }};

                if (getArraySize(m_params.src.image) == 1)
                {
                    DE_ASSERT(barrierno < m_params.mipLevels);
                    preImageBarrier.subresourceRange.baseMipLevel = barrierno;
                    preImageBarrier.subresourceRange.levelCount =
                        (barrierno + 1 < m_params.barrierCount) ? 1 : VK_REMAINING_MIP_LEVELS;
                }
                else
                {
                    preImageBarrier.subresourceRange.baseArrayLayer = barrierno;
                    preImageBarrier.subresourceRange.layerCount =
                        (barrierno + 1 < m_params.barrierCount) ? 1 : VK_REMAINING_ARRAY_LAYERS;
                }
                vk.cmdPipelineBarrier(*m_universalCmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                      VK_PIPELINE_STAGE_TRANSFER_BIT, (VkDependencyFlags)0, 0,
                                      (const VkMemoryBarrier *)DE_NULL, 0, (const VkBufferMemoryBarrier *)DE_NULL, 1,
                                      &preImageBarrier);
            }
        }

        for (uint32_t regionNdx = 0u; regionNdx < (uint32_t)m_params.regions.size(); regionNdx++)
        {
            const uint32_t mipLevel = m_params.regions[regionNdx].imageBlit.dstSubresource.mipLevel;

            // Prepare single mip level for writing
            const VkImageMemoryBarrier preImageBarrier = {
                VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, // VkStructureType sType;
                DE_NULL,                                // const void* pNext;
                VK_ACCESS_TRANSFER_READ_BIT,            // VkAccessFlags srcAccessMask;
                VK_ACCESS_TRANSFER_WRITE_BIT,           // VkAccessFlags dstAccessMask;
                m_params.src.image.operationLayout,     // VkImageLayout oldLayout;
                m_params.dst.image.operationLayout,     // VkImageLayout newLayout;
                VK_QUEUE_FAMILY_IGNORED,                // uint32_t srcQueueFamilyIndex;
                VK_QUEUE_FAMILY_IGNORED,                // uint32_t dstQueueFamilyIndex;
                m_destination.get(),                    // VkImage image;
                {
                    // VkImageSubresourceRange subresourceRange;
                    getAspectFlags(dstTcuFormat),    // VkImageAspectFlags aspectMask;
                    mipLevel,                        // uint32_t baseMipLevel;
                    1u,                              // uint32_t mipLevels;
                    0u,                              // uint32_t baseArraySlice;
                    getArraySize(m_params.dst.image) // uint32_t arraySize;
                }};

            // Prepare single mip level for reading
            const VkImageMemoryBarrier postImageBarrier = {
                VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, // VkStructureType sType;
                DE_NULL,                                // const void* pNext;
                VK_ACCESS_TRANSFER_WRITE_BIT,           // VkAccessFlags srcAccessMask;
                VK_ACCESS_TRANSFER_READ_BIT,            // VkAccessFlags dstAccessMask;
                m_params.dst.image.operationLayout,     // VkImageLayout oldLayout;
                m_params.src.image.operationLayout,     // VkImageLayout newLayout;
                VK_QUEUE_FAMILY_IGNORED,                // uint32_t srcQueueFamilyIndex;
                VK_QUEUE_FAMILY_IGNORED,                // uint32_t dstQueueFamilyIndex;
                m_destination.get(),                    // VkImage image;
                {
                    // VkImageSubresourceRange subresourceRange;
                    getAspectFlags(dstTcuFormat),    // VkImageAspectFlags aspectMask;
                    mipLevel,                        // uint32_t baseMipLevel;
                    1u,                              // uint32_t mipLevels;
                    0u,                              // uint32_t baseArraySlice;
                    getArraySize(m_params.src.image) // uint32_t arraySize;
                }};

            vk.cmdPipelineBarrier(*m_universalCmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                  (VkDependencyFlags)0, 0, (const VkMemoryBarrier *)DE_NULL, 0,
                                  (const VkBufferMemoryBarrier *)DE_NULL, 1, &preImageBarrier);

            if (!(m_params.extensionFlags & COPY_COMMANDS_2))
            {
                vk.cmdBlitImage(*m_universalCmdBuffer, m_destination.get(), m_params.src.image.operationLayout,
                                m_destination.get(), m_params.dst.image.operationLayout, 1u, &regions[regionNdx],
                                m_params.filter);
            }
            else
            {
                DE_ASSERT(m_params.extensionFlags & COPY_COMMANDS_2);
                const VkBlitImageInfo2KHR BlitImageInfo2KHR = {
                    VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2_KHR, // VkStructureType sType;
                    DE_NULL,                                 // const void* pNext;
                    m_destination.get(),                     // VkImage srcImage;
                    m_params.src.image.operationLayout,      // VkImageLayout srcImageLayout;
                    m_destination.get(),                     // VkImage dstImage;
                    m_params.dst.image.operationLayout,      // VkImageLayout dstImageLayout;
                    1u,                                      // uint32_t regionCount;
                    &regions2KHR[regionNdx],                 // const VkImageBlit2KHR* pRegions;
                    m_params.filter                          // VkFilter filter;
                };
                vk.cmdBlitImage2(*m_universalCmdBuffer, &BlitImageInfo2KHR);
            }

            vk.cmdPipelineBarrier(*m_universalCmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                  (VkDependencyFlags)0, 0, (const VkMemoryBarrier *)DE_NULL, 0,
                                  (const VkBufferMemoryBarrier *)DE_NULL, 1, &postImageBarrier);
        }

        // Prepare all mip levels for writing
        {
            const VkImageMemoryBarrier postImageBarrier = {
                VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, // VkStructureType sType;
                DE_NULL,                                // const void* pNext;
                VK_ACCESS_TRANSFER_READ_BIT,            // VkAccessFlags srcAccessMask;
                VK_ACCESS_TRANSFER_WRITE_BIT,           // VkAccessFlags dstAccessMask;
                m_params.src.image.operationLayout,     // VkImageLayout oldLayout;
                m_params.dst.image.operationLayout,     // VkImageLayout newLayout;
                VK_QUEUE_FAMILY_IGNORED,                // uint32_t srcQueueFamilyIndex;
                VK_QUEUE_FAMILY_IGNORED,                // uint32_t dstQueueFamilyIndex;
                m_destination.get(),                    // VkImage image;
                {
                    // VkImageSubresourceRange subresourceRange;
                    getAspectFlags(dstTcuFormat),    // VkImageAspectFlags aspectMask;
                    0u,                              // uint32_t baseMipLevel;
                    VK_REMAINING_MIP_LEVELS,         // uint32_t mipLevels;
                    0u,                              // uint32_t baseArraySlice;
                    getArraySize(m_params.dst.image) // uint32_t arraySize;
                }};

            vk.cmdPipelineBarrier(*m_universalCmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                  (VkDependencyFlags)0, 0, (const VkMemoryBarrier *)DE_NULL, 0,
                                  (const VkBufferMemoryBarrier *)DE_NULL, 1, &postImageBarrier);
        }
    }

    endCommandBuffer(vk, *m_universalCmdBuffer);

    if (m_params.useSparseBinding)
    {
        const VkPipelineStageFlags stageBits[] = {VK_PIPELINE_STAGE_TRANSFER_BIT};
        submitCommandsAndWait(vk, vkDevice, m_universalQueue, *m_universalCmdBuffer, false, 1u, 1u, &*m_sparseSemaphore,
                              stageBits);
    }
    else
    {
        submitCommandsAndWait(vk, vkDevice, m_universalQueue, *m_universalCmdBuffer);
    }

    m_context.resetCommandPoolForVKSC(vkDevice, *m_universalCmdPool);

    return checkTestResult();
}

bool BlittingMipmaps::checkNonNearestFilteredResult(void)
{
    tcu::TestLog &log(m_context.getTestContext().getLog());
    bool allLevelsOk = true;

    for (uint32_t mipLevelNdx = 0u; mipLevelNdx < m_params.mipLevels; mipLevelNdx++)
    {
        // Update reference results with previous results that have been verified.
        // This needs to be done such that accumulated errors don't exceed the fixed threshold.
        for (uint32_t i = 0; i < m_params.regions.size(); i++)
        {
            const CopyRegion region    = m_params.regions[i];
            const uint32_t srcMipLevel = m_params.regions[i].imageBlit.srcSubresource.mipLevel;
            const uint32_t dstMipLevel = m_params.regions[i].imageBlit.dstSubresource.mipLevel;
            de::MovePtr<tcu::TextureLevel> prevResultLevel;
            tcu::ConstPixelBufferAccess src;
            if (srcMipLevel < mipLevelNdx)
            {
                // Generate expected result from rendered result that was previously verified
                prevResultLevel = readImage(*m_destination, m_params.dst.image, srcMipLevel);
                src             = prevResultLevel->getAccess();
            }
            else
            {
                // Previous reference mipmaps might have changed, so recompute expected result
                src = m_expectedTextureLevel[srcMipLevel]->getAccess();
            }
            copyRegionToTextureLevel(src, m_expectedTextureLevel[dstMipLevel]->getAccess(), region, dstMipLevel);
        }

        de::MovePtr<tcu::TextureLevel> resultLevel      = readImage(*m_destination, m_params.dst.image, mipLevelNdx);
        const tcu::ConstPixelBufferAccess &resultAccess = resultLevel->getAccess();

        const tcu::Sampler::DepthStencilMode mode =
            tcu::hasDepthComponent(resultAccess.getFormat().order)   ? tcu::Sampler::MODE_DEPTH :
            tcu::hasStencilComponent(resultAccess.getFormat().order) ? tcu::Sampler::MODE_STENCIL :
                                                                       tcu::Sampler::MODE_LAST;
        const tcu::ConstPixelBufferAccess result = tcu::hasDepthComponent(resultAccess.getFormat().order) ?
                                                       getEffectiveDepthStencilAccess(resultAccess, mode) :
                                                   tcu::hasStencilComponent(resultAccess.getFormat().order) ?
                                                       getEffectiveDepthStencilAccess(resultAccess, mode) :
                                                       resultAccess;
        const tcu::ConstPixelBufferAccess clampedLevel =
            tcu::hasDepthComponent(resultAccess.getFormat().order) ?
                getEffectiveDepthStencilAccess(m_expectedTextureLevel[mipLevelNdx]->getAccess(), mode) :
            tcu::hasStencilComponent(resultAccess.getFormat().order) ?
                getEffectiveDepthStencilAccess(m_expectedTextureLevel[mipLevelNdx]->getAccess(), mode) :
                m_expectedTextureLevel[mipLevelNdx]->getAccess();
        const tcu::ConstPixelBufferAccess unclampedLevel =
            tcu::hasDepthComponent(resultAccess.getFormat().order) ?
                getEffectiveDepthStencilAccess(m_unclampedExpectedTextureLevel[mipLevelNdx]->getAccess(), mode) :
            tcu::hasStencilComponent(resultAccess.getFormat().order) ?
                getEffectiveDepthStencilAccess(m_unclampedExpectedTextureLevel[mipLevelNdx]->getAccess(), mode) :
                m_unclampedExpectedTextureLevel[mipLevelNdx]->getAccess();
        const tcu::TextureFormat srcFormat =
            tcu::hasDepthComponent(resultAccess.getFormat().order) ?
                tcu::getEffectiveDepthStencilTextureFormat(mapVkFormat(m_params.src.image.format), mode) :
            tcu::hasStencilComponent(resultAccess.getFormat().order) ?
                tcu::getEffectiveDepthStencilTextureFormat(mapVkFormat(m_params.src.image.format), mode) :
                mapVkFormat(m_params.src.image.format);

        const tcu::TextureFormat dstFormat = result.getFormat();
        bool singleLevelOk                 = false;
        std::vector<CopyRegion> mipLevelRegions;

        for (size_t regionNdx = 0u; regionNdx < m_params.regions.size(); regionNdx++)
            if (m_params.regions.at(regionNdx).imageBlit.dstSubresource.mipLevel == mipLevelNdx)
                mipLevelRegions.push_back(m_params.regions.at(regionNdx));

        log << tcu::TestLog::Section("ClampedSourceImage", "Region with clamped edges on source image.");

        if (isFloatFormat(dstFormat))
        {
            const bool srcIsSRGB       = tcu::isSRGB(srcFormat);
            const tcu::Vec4 srcMaxDiff = getFormatThreshold(srcFormat) * tcu::Vec4(srcIsSRGB ? 2.0f : 1.0f);
            const tcu::Vec4 dstMaxDiff = getFormatThreshold(dstFormat);
            const tcu::Vec4 threshold =
                (srcMaxDiff + dstMaxDiff) * ((m_params.filter == VK_FILTER_CUBIC_EXT) ? 1.5f : 1.0f);

            singleLevelOk = tcu::floatThresholdCompare(log, "Compare", "Result comparsion", clampedLevel, result,
                                                       threshold, tcu::COMPARE_LOG_RESULT);
            log << tcu::TestLog::EndSection;

            if (!singleLevelOk)
            {
                log << tcu::TestLog::Section("NonClampedSourceImage", "Region with non-clamped edges on source image.");
                singleLevelOk = tcu::floatThresholdCompare(log, "Compare", "Result comparsion", unclampedLevel, result,
                                                           threshold, tcu::COMPARE_LOG_RESULT);
                log << tcu::TestLog::EndSection;
            }
        }
        else
        {
            tcu::UVec4 threshold;
            // Calculate threshold depending on channel width of destination format.
            const tcu::IVec4 dstBitDepth = tcu::getTextureFormatBitDepth(dstFormat);
            const tcu::IVec4 srcBitDepth = tcu::getTextureFormatBitDepth(srcFormat);
            for (uint32_t i = 0; i < 4; ++i)
            {
                DE_ASSERT(dstBitDepth[i] < std::numeric_limits<uint64_t>::digits);
                DE_ASSERT(srcBitDepth[i] < std::numeric_limits<uint64_t>::digits);
                uint64_t threshold64 =
                    1 + de::max(((UINT64_C(1) << dstBitDepth[i]) - 1) /
                                    de::clamp((UINT64_C(1) << srcBitDepth[i]) - 1, UINT64_C(1), UINT64_C(256)),
                                UINT64_C(1));
                DE_ASSERT(threshold64 <= std::numeric_limits<uint32_t>::max());
                threshold[i] = static_cast<uint32_t>(threshold64);
            }

            singleLevelOk = tcu::intThresholdCompare(log, "Compare", "Result comparsion", clampedLevel, result,
                                                     threshold, tcu::COMPARE_LOG_RESULT);
            log << tcu::TestLog::EndSection;

            if (!singleLevelOk)
            {
                log << tcu::TestLog::Section("NonClampedSourceImage", "Region with non-clamped edges on source image.");
                singleLevelOk = tcu::intThresholdCompare(log, "Compare", "Result comparsion", unclampedLevel, result,
                                                         threshold, tcu::COMPARE_LOG_RESULT);
                log << tcu::TestLog::EndSection;
            }
        }
        allLevelsOk &= singleLevelOk;
    }

    return allLevelsOk;
}

bool BlittingMipmaps::checkNearestFilteredResult(void)
{
    bool allLevelsOk = true;
    tcu::TestLog &log(m_context.getTestContext().getLog());

    for (uint32_t mipLevelNdx = 0u; mipLevelNdx < m_params.mipLevels; mipLevelNdx++)
    {
        de::MovePtr<tcu::TextureLevel> resultLevel      = readImage(*m_destination, m_params.dst.image, mipLevelNdx);
        const tcu::ConstPixelBufferAccess &resultAccess = resultLevel->getAccess();

        const tcu::Sampler::DepthStencilMode mode =
            tcu::hasDepthComponent(resultAccess.getFormat().order)   ? tcu::Sampler::MODE_DEPTH :
            tcu::hasStencilComponent(resultAccess.getFormat().order) ? tcu::Sampler::MODE_STENCIL :
                                                                       tcu::Sampler::MODE_LAST;
        const tcu::ConstPixelBufferAccess result = tcu::hasDepthComponent(resultAccess.getFormat().order) ?
                                                       getEffectiveDepthStencilAccess(resultAccess, mode) :
                                                   tcu::hasStencilComponent(resultAccess.getFormat().order) ?
                                                       getEffectiveDepthStencilAccess(resultAccess, mode) :
                                                       resultAccess;
        const tcu::ConstPixelBufferAccess source =
            (m_params.singleCommand || mipLevelNdx == 0) ? //  Read from source image
                tcu::hasDepthComponent(resultAccess.getFormat().order) ?
                tcu::getEffectiveDepthStencilAccess(m_sourceTextureLevel->getAccess(), mode) :
                tcu::hasStencilComponent(resultAccess.getFormat().order) ?
                tcu::getEffectiveDepthStencilAccess(m_sourceTextureLevel->getAccess(), mode) :
                m_sourceTextureLevel->getAccess()
                //  Read from destination image
                :
                tcu::hasDepthComponent(resultAccess.getFormat().order) ?
                tcu::getEffectiveDepthStencilAccess(m_expectedTextureLevel[mipLevelNdx - 1u]->getAccess(), mode) :
                tcu::hasStencilComponent(resultAccess.getFormat().order) ?
                tcu::getEffectiveDepthStencilAccess(m_expectedTextureLevel[mipLevelNdx - 1u]->getAccess(), mode) :
                m_expectedTextureLevel[mipLevelNdx - 1u]->getAccess();
        const tcu::TextureFormat dstFormat             = result.getFormat();
        const tcu::TextureChannelClass dstChannelClass = tcu::getTextureChannelClass(dstFormat.type);
        bool singleLevelOk                             = false;
        std::vector<CopyRegion> mipLevelRegions;

        for (size_t regionNdx = 0u; regionNdx < m_params.regions.size(); regionNdx++)
            if (m_params.regions.at(regionNdx).imageBlit.dstSubresource.mipLevel == mipLevelNdx)
                mipLevelRegions.push_back(m_params.regions.at(regionNdx));

        // Use the calculated regions instead of the original ones.
        TestParams newParams = m_params;
        newParams.regions    = mipLevelRegions;

        tcu::TextureLevel errorMaskStorage(tcu::TextureFormat(tcu::TextureFormat::RGB, tcu::TextureFormat::UNORM_INT8),
                                           result.getWidth(), result.getHeight(), result.getDepth());
        tcu::PixelBufferAccess errorMask = errorMaskStorage.getAccess();
        tcu::Vec4 pixelBias(0.0f, 0.0f, 0.0f, 0.0f);
        tcu::Vec4 pixelScale(1.0f, 1.0f, 1.0f, 1.0f);

        tcu::clear(errorMask, tcu::Vec4(0.0f, 1.0f, 0.0f, 1.0));

        if (dstChannelClass == tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER ||
            dstChannelClass == tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER)
        {
            singleLevelOk = intNearestBlitCompare(source, result, errorMask, newParams);
        }
        else
        {
            const tcu::Vec4 srcMaxDiff = getFloatOrFixedPointFormatThreshold(source.getFormat());
            const tcu::Vec4 dstMaxDiff = getFloatOrFixedPointFormatThreshold(result.getFormat());

            singleLevelOk = floatNearestBlitCompare(source, result, srcMaxDiff, dstMaxDiff, errorMask, newParams);
        }

        if (dstFormat != tcu::TextureFormat(tcu::TextureFormat::RGBA, tcu::TextureFormat::UNORM_INT8))
            tcu::computePixelScaleBias(result, pixelScale, pixelBias);

        if (!singleLevelOk)
        {
            log << tcu::TestLog::ImageSet("Compare", "Result comparsion, level " + de::toString(mipLevelNdx))
                << tcu::TestLog::Image("Result", "Result", result, pixelScale, pixelBias)
                << tcu::TestLog::Image("Reference", "Reference", source, pixelScale, pixelBias)
                << tcu::TestLog::Image("ErrorMask", "Error mask", errorMask) << tcu::TestLog::EndImageSet;
        }
        else
        {
            log << tcu::TestLog::ImageSet("Compare", "Result comparsion, level " + de::toString(mipLevelNdx))
                << tcu::TestLog::Image("Result", "Result", result, pixelScale, pixelBias) << tcu::TestLog::EndImageSet;
        }

        allLevelsOk &= singleLevelOk;
    }

    return allLevelsOk;
}

tcu::TestStatus BlittingMipmaps::checkTestResult(tcu::ConstPixelBufferAccess result)
{
    DE_UNREF(result);
    DE_ASSERT(m_params.filter == VK_FILTER_NEAREST || m_params.filter == VK_FILTER_LINEAR ||
              m_params.filter == VK_FILTER_CUBIC_EXT);
    const std::string failMessage("Result image is incorrect");

    if (m_params.filter != VK_FILTER_NEAREST)
    {
        if (!checkNonNearestFilteredResult())
            return tcu::TestStatus::fail(failMessage);
    }
    else // NEAREST filtering
    {
        if (!checkNearestFilteredResult())
            return tcu::TestStatus::fail(failMessage);
    }

    return tcu::TestStatus::pass("Pass");
}

void BlittingMipmaps::copyRegionToTextureLevel(tcu::ConstPixelBufferAccess src, tcu::PixelBufferAccess dst,
                                               CopyRegion region, uint32_t mipLevel)
{
    DE_ASSERT(src.getDepth() == dst.getDepth());

    const MirrorMode mirrorMode = getMirrorMode(region.imageBlit.srcOffsets[0], region.imageBlit.srcOffsets[1],
                                                region.imageBlit.dstOffsets[0], region.imageBlit.dstOffsets[1]);

    flipCoordinates(region, mirrorMode);

    const VkOffset3D srcOffset = region.imageBlit.srcOffsets[0];
    const VkOffset3D srcExtent = {region.imageBlit.srcOffsets[1].x - srcOffset.x,
                                  region.imageBlit.srcOffsets[1].y - srcOffset.y,
                                  region.imageBlit.srcOffsets[1].z - srcOffset.z};
    const VkOffset3D dstOffset = region.imageBlit.dstOffsets[0];
    const VkOffset3D dstExtent = {region.imageBlit.dstOffsets[1].x - dstOffset.x,
                                  region.imageBlit.dstOffsets[1].y - dstOffset.y,
                                  region.imageBlit.dstOffsets[1].z - dstOffset.z};

    tcu::Sampler::FilterMode filter;
    switch (m_params.filter)
    {
    case VK_FILTER_LINEAR:
        filter = tcu::Sampler::LINEAR;
        break;
    case VK_FILTER_CUBIC_EXT:
        filter = tcu::Sampler::CUBIC;
        break;
    case VK_FILTER_NEAREST:
    default:
        filter = tcu::Sampler::NEAREST;
        break;
    }

    if (tcu::isCombinedDepthStencilType(src.getFormat().type))
    {
        DE_ASSERT(src.getFormat() == dst.getFormat());
        // Scale depth.
        if (tcu::hasDepthComponent(src.getFormat().order))
        {
            const tcu::ConstPixelBufferAccess srcSubRegion = getEffectiveDepthStencilAccess(
                tcu::getSubregion(src, srcOffset.x, srcOffset.y, srcExtent.x, srcExtent.y), tcu::Sampler::MODE_DEPTH);
            const tcu::PixelBufferAccess dstSubRegion = getEffectiveDepthStencilAccess(
                tcu::getSubregion(dst, dstOffset.x, dstOffset.y, dstExtent.x, dstExtent.y), tcu::Sampler::MODE_DEPTH);
            tcu::scale(dstSubRegion, srcSubRegion, filter);

            if (filter != tcu::Sampler::NEAREST)
            {
                const tcu::ConstPixelBufferAccess depthSrc =
                    getEffectiveDepthStencilAccess(src, tcu::Sampler::MODE_DEPTH);
                const tcu::PixelBufferAccess unclampedSubRegion = getEffectiveDepthStencilAccess(
                    tcu::getSubregion(m_unclampedExpectedTextureLevel[0]->getAccess(), dstOffset.x, dstOffset.y,
                                      dstExtent.x, dstExtent.y),
                    tcu::Sampler::MODE_DEPTH);
                scaleFromWholeSrcBuffer(unclampedSubRegion, depthSrc, srcOffset, srcExtent, filter);
            }
        }

        // Scale stencil.
        if (tcu::hasStencilComponent(src.getFormat().order))
        {
            const tcu::ConstPixelBufferAccess srcSubRegion = getEffectiveDepthStencilAccess(
                tcu::getSubregion(src, srcOffset.x, srcOffset.y, srcExtent.x, srcExtent.y), tcu::Sampler::MODE_STENCIL);
            const tcu::PixelBufferAccess dstSubRegion = getEffectiveDepthStencilAccess(
                tcu::getSubregion(dst, dstOffset.x, dstOffset.y, dstExtent.x, dstExtent.y), tcu::Sampler::MODE_STENCIL);
            blit(dstSubRegion, srcSubRegion, filter, mirrorMode);

            if (filter != tcu::Sampler::NEAREST)
            {
                const tcu::ConstPixelBufferAccess stencilSrc =
                    getEffectiveDepthStencilAccess(src, tcu::Sampler::MODE_STENCIL);
                const tcu::PixelBufferAccess unclampedSubRegion = getEffectiveDepthStencilAccess(
                    tcu::getSubregion(m_unclampedExpectedTextureLevel[0]->getAccess(), dstOffset.x, dstOffset.y,
                                      dstExtent.x, dstExtent.y),
                    tcu::Sampler::MODE_STENCIL);
                scaleFromWholeSrcBuffer(unclampedSubRegion, stencilSrc, srcOffset, srcExtent, filter);
            }
        }
    }
    else
    {
        for (int layerNdx = 0u; layerNdx < src.getDepth(); layerNdx++)
        {
            const tcu::ConstPixelBufferAccess srcSubRegion =
                tcu::getSubregion(src, srcOffset.x, srcOffset.y, layerNdx, srcExtent.x, srcExtent.y, 1);
            const tcu::PixelBufferAccess dstSubRegion =
                tcu::getSubregion(dst, dstOffset.x, dstOffset.y, layerNdx, dstExtent.x, dstExtent.y, 1);
            blit(dstSubRegion, srcSubRegion, filter, mirrorMode);

            if (filter != tcu::Sampler::NEAREST)
            {
                const tcu::PixelBufferAccess unclampedSubRegion =
                    tcu::getSubregion(m_unclampedExpectedTextureLevel[mipLevel]->getAccess(), dstOffset.x, dstOffset.y,
                                      layerNdx, dstExtent.x, dstExtent.y, 1);
                scaleFromWholeSrcBuffer(unclampedSubRegion, srcSubRegion, srcOffset, srcExtent, filter);
            }
        }
    }
}

void BlittingMipmaps::generateExpectedResult(void)
{
    const tcu::ConstPixelBufferAccess src = m_sourceTextureLevel->getAccess();
    const tcu::ConstPixelBufferAccess dst = m_destinationTextureLevel->getAccess();

    for (uint32_t mipLevelNdx = 0u; mipLevelNdx < m_params.mipLevels; mipLevelNdx++)
        m_expectedTextureLevel[mipLevelNdx] = de::MovePtr<tcu::TextureLevel>(new tcu::TextureLevel(
            dst.getFormat(), dst.getWidth() >> mipLevelNdx, dst.getHeight() >> mipLevelNdx, dst.getDepth()));

    tcu::copy(m_expectedTextureLevel[0]->getAccess(), src);

    if (m_params.filter != VK_FILTER_NEAREST)
    {
        for (uint32_t mipLevelNdx = 0u; mipLevelNdx < m_params.mipLevels; mipLevelNdx++)
            m_unclampedExpectedTextureLevel[mipLevelNdx] = de::MovePtr<tcu::TextureLevel>(new tcu::TextureLevel(
                dst.getFormat(), dst.getWidth() >> mipLevelNdx, dst.getHeight() >> mipLevelNdx, dst.getDepth()));

        tcu::copy(m_unclampedExpectedTextureLevel[0]->getAccess(), src);
    }

    for (uint32_t i = 0; i < m_params.regions.size(); i++)
    {
        CopyRegion region = m_params.regions[i];
        copyRegionToTextureLevel(
            m_expectedTextureLevel[m_params.regions[i].imageBlit.srcSubresource.mipLevel]->getAccess(),
            m_expectedTextureLevel[m_params.regions[i].imageBlit.dstSubresource.mipLevel]->getAccess(), region,
            m_params.regions[i].imageBlit.dstSubresource.mipLevel);
    }
}

class BlitMipmapTestCase : public vkt::TestCase
{
public:
    BlitMipmapTestCase(tcu::TestContext &testCtx, const std::string &name, const TestParams params)
        : vkt::TestCase(testCtx, name)
        , m_params(params)
    {
    }

    virtual TestInstance *createInstance(Context &context) const
    {
        return new BlittingMipmaps(context, m_params);
    }

    virtual void checkSupport(Context &context) const
    {
        const InstanceInterface &vki        = context.getInstanceInterface();
        const VkPhysicalDevice vkPhysDevice = context.getPhysicalDevice();
        {
            VkImageFormatProperties properties;
            if (context.getInstanceInterface().getPhysicalDeviceImageFormatProperties(
                    context.getPhysicalDevice(), m_params.src.image.format, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL,
                    VK_IMAGE_USAGE_TRANSFER_SRC_BIT, 0, &properties) == VK_ERROR_FORMAT_NOT_SUPPORTED)
            {
                TCU_THROW(NotSupportedError, "Format not supported");
            }
            else if ((m_params.src.image.extent.width > properties.maxExtent.width) ||
                     (m_params.src.image.extent.height > properties.maxExtent.height) ||
                     (m_params.src.image.extent.depth > properties.maxArrayLayers))
            {
                TCU_THROW(NotSupportedError, "Image size not supported");
            }
        }

        {
            VkImageFormatProperties properties;
            if (context.getInstanceInterface().getPhysicalDeviceImageFormatProperties(
                    context.getPhysicalDevice(), m_params.dst.image.format, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL,
                    VK_IMAGE_USAGE_TRANSFER_DST_BIT, 0, &properties) == VK_ERROR_FORMAT_NOT_SUPPORTED)
            {
                TCU_THROW(NotSupportedError, "Format not supported");
            }
            else if ((m_params.dst.image.extent.width > properties.maxExtent.width) ||
                     (m_params.dst.image.extent.height > properties.maxExtent.height) ||
                     (m_params.dst.image.extent.depth > properties.maxArrayLayers))
            {
                TCU_THROW(NotSupportedError, "Image size not supported");
            }
            else if (m_params.mipLevels > properties.maxMipLevels)
            {
                TCU_THROW(NotSupportedError, "Number of mip levels not supported");
            }

            checkExtensionSupport(context, m_params.extensionFlags);
        }

        const VkFormatProperties srcFormatProperties =
            getPhysicalDeviceFormatProperties(vki, vkPhysDevice, m_params.src.image.format);
        if (!(srcFormatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_SRC_BIT))
        {
            TCU_THROW(NotSupportedError, "Format feature blit source not supported");
        }

        const VkFormatProperties dstFormatProperties =
            getPhysicalDeviceFormatProperties(vki, vkPhysDevice, m_params.dst.image.format);
        if (!(dstFormatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_DST_BIT))
        {
            TCU_THROW(NotSupportedError, "Format feature blit destination not supported");
        }

        if (m_params.filter == VK_FILTER_LINEAR &&
            !(srcFormatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT))
            TCU_THROW(NotSupportedError, "Source format feature sampled image filter linear not supported");

        if (m_params.filter == VK_FILTER_CUBIC_EXT)
        {
            context.requireDeviceFunctionality("VK_EXT_filter_cubic");

            if (!(srcFormatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_CUBIC_BIT_EXT))
            {
                TCU_THROW(NotSupportedError, "Source format feature sampled image filter cubic not supported");
            }
        }
    }

private:
    TestParams m_params;
};

// Resolve image to image.

enum ResolveImageToImageOptions
{
    NO_OPTIONAL_OPERATION = 0,
    COPY_MS_IMAGE_TO_MS_IMAGE,
    COPY_MS_IMAGE_TO_ARRAY_MS_IMAGE,
    COPY_MS_IMAGE_LAYER_TO_MS_IMAGE,
    COPY_MS_IMAGE_TO_MS_IMAGE_MULTIREGION,
    COPY_MS_IMAGE_TO_MS_IMAGE_NO_CAB,
    COPY_MS_IMAGE_TO_MS_IMAGE_COMPUTE,
    COPY_MS_IMAGE_TO_MS_IMAGE_TRANSFER
};

class ResolveImageToImage : public CopiesAndBlittingTestInstance
{
public:
    ResolveImageToImage(Context &context, TestParams params, ResolveImageToImageOptions options);
    virtual tcu::TestStatus iterate(void);

    static inline bool shouldVerifyIntermediateResults(ResolveImageToImageOptions option)
    {
        return option == COPY_MS_IMAGE_TO_MS_IMAGE || option == COPY_MS_IMAGE_TO_ARRAY_MS_IMAGE ||
               option == COPY_MS_IMAGE_LAYER_TO_MS_IMAGE || option == COPY_MS_IMAGE_TO_MS_IMAGE_COMPUTE ||
               option == COPY_MS_IMAGE_TO_MS_IMAGE_TRANSFER;
    }

protected:
    virtual tcu::TestStatus checkTestResult(tcu::ConstPixelBufferAccess result);
    void copyMSImageToMSImage(uint32_t copyArraySize);
    tcu::TestStatus checkIntermediateCopy(void);

private:
    de::MovePtr<vk::Allocator> m_alternativeAllocator;
    Move<VkImage> m_multisampledImage;
    de::MovePtr<Allocation> m_multisampledImageAlloc;

    Move<VkImage> m_destination;
    de::MovePtr<Allocation> m_destinationImageAlloc;
    std::vector<de::SharedPtr<Allocation>> m_sparseAllocations;
    Move<VkSemaphore> m_sparseSemaphore;

    Move<VkImage> m_multisampledCopyImage;
    de::MovePtr<Allocation> m_multisampledCopyImageAlloc;
    Move<VkImage> m_multisampledCopyNoCabImage;
    de::MovePtr<Allocation> m_multisampledCopyImageNoCabAlloc;

    const TestParams m_params;
    const ResolveImageToImageOptions m_options;

    virtual void copyRegionToTextureLevel(tcu::ConstPixelBufferAccess src, tcu::PixelBufferAccess dst,
                                          CopyRegion region, uint32_t mipLevel = 0u);
};

ResolveImageToImage::ResolveImageToImage(Context &context, TestParams params, const ResolveImageToImageOptions options)
    : CopiesAndBlittingTestInstance(context, params)
    , m_params(params)
    , m_options(options)
{
    const InstanceInterface &vki = m_context.getInstanceInterface();
    const DeviceInterface &vk    = m_context.getDeviceInterface();

    VkQueue queue                               = VK_NULL_HANDLE;
    VkCommandBuffer commandBuffer               = VK_NULL_HANDLE;
    VkCommandPool commandPool                   = VK_NULL_HANDLE;
    std::tie(queue, commandBuffer, commandPool) = activeExecutionCtx();

    Allocator &memAlloc                           = *m_allocator;
    const VkPhysicalDevice vkPhysDevice           = m_context.getPhysicalDevice();
    const VkDevice vkDevice                       = m_device;
    const VkComponentMapping componentMappingRGBA = {VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G,
                                                     VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A};
    Move<VkRenderPass> renderPass;

    Move<VkShaderModule> vertexShaderModule =
        createShaderModule(vk, vkDevice, m_context.getBinaryCollection().get("vert"), 0);
    Move<VkShaderModule> fragmentShaderModule =
        createShaderModule(vk, vkDevice, m_context.getBinaryCollection().get("frag"), 0);
    std::vector<tcu::Vec4> vertices;

    Move<VkBuffer> vertexBuffer;
    de::MovePtr<Allocation> vertexBufferAlloc;

    Move<VkPipelineLayout> pipelineLayout;
    Move<VkPipeline> graphicsPipeline;

    const VkSampleCountFlagBits rasterizationSamples = m_params.samples;

    // Create color image.
    {
        VkImageCreateInfo colorImageParams = {
            VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, // VkStructureType sType;
            DE_NULL,                             // const void* pNext;
            getCreateFlags(m_params.src.image),  // VkImageCreateFlags flags;
            m_params.src.image.imageType,        // VkImageType imageType;
            m_params.src.image.format,           // VkFormat format;
            getExtent3D(m_params.src.image),     // VkExtent3D extent;
            1u,                                  // uint32_t mipLevels;
            getArraySize(m_params.src.image),    // uint32_t arrayLayers;
            rasterizationSamples,                // VkSampleCountFlagBits samples;
            VK_IMAGE_TILING_OPTIMAL,             // VkImageTiling tiling;
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT  // VkImageUsageFlags usage;
                | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT,
            VK_SHARING_MODE_EXCLUSIVE, // VkSharingMode sharingMode;
            0u,                        // uint32_t queueFamilyIndexCount;
            (const uint32_t *)DE_NULL, // const uint32_t* pQueueFamilyIndices;
            VK_IMAGE_LAYOUT_UNDEFINED, // VkImageLayout initialLayout;
        };

        m_multisampledImage      = createImage(vk, vkDevice, &colorImageParams);
        VkMemoryRequirements req = getImageMemoryRequirements(vk, vkDevice, *m_multisampledImage);

        // Allocate and bind color image memory.
        uint32_t offset          = m_params.imageOffset ? static_cast<uint32_t>(req.alignment) : 0u;
        m_multisampledImageAlloc = allocateImage(vki, vk, vkPhysDevice, vkDevice, *m_multisampledImage,
                                                 MemoryRequirement::Any, memAlloc, m_params.allocationKind, offset);

        VK_CHECK(vk.bindImageMemory(vkDevice, *m_multisampledImage, m_multisampledImageAlloc->getMemory(), offset));

        switch (m_options)
        {
        case COPY_MS_IMAGE_TO_MS_IMAGE_MULTIREGION:
        case COPY_MS_IMAGE_TO_MS_IMAGE_COMPUTE:
        case COPY_MS_IMAGE_TO_MS_IMAGE_TRANSFER:
        case COPY_MS_IMAGE_TO_MS_IMAGE:
        {
            colorImageParams.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                                     VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
            m_multisampledCopyImage = createImage(vk, vkDevice, &colorImageParams);
            // Allocate and bind color image memory.
            m_multisampledCopyImageAlloc = allocateImage(vki, vk, vkPhysDevice, vkDevice, *m_multisampledCopyImage,
                                                         MemoryRequirement::Any, memAlloc, m_params.allocationKind, 0u);
            VK_CHECK(vk.bindImageMemory(vkDevice, *m_multisampledCopyImage, m_multisampledCopyImageAlloc->getMemory(),
                                        m_multisampledCopyImageAlloc->getOffset()));
            break;
        }
        case COPY_MS_IMAGE_LAYER_TO_MS_IMAGE:
        case COPY_MS_IMAGE_TO_ARRAY_MS_IMAGE:
        {
            colorImageParams.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                                     VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
            colorImageParams.arrayLayers = getArraySize(m_params.dst.image);
            m_multisampledCopyImage      = createImage(vk, vkDevice, &colorImageParams);
            // Allocate and bind color image memory.
            m_multisampledCopyImageAlloc = allocateImage(vki, vk, vkPhysDevice, vkDevice, *m_multisampledCopyImage,
                                                         MemoryRequirement::Any, memAlloc, m_params.allocationKind, 0u);
            VK_CHECK(vk.bindImageMemory(vkDevice, *m_multisampledCopyImage, m_multisampledCopyImageAlloc->getMemory(),
                                        m_multisampledCopyImageAlloc->getOffset()));
            break;
        }
        case COPY_MS_IMAGE_TO_MS_IMAGE_NO_CAB:
        {
            colorImageParams.usage =
                VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
            colorImageParams.arrayLayers = getArraySize(m_params.dst.image);
            m_multisampledCopyImage      = createImage(vk, vkDevice, &colorImageParams);
            m_multisampledCopyNoCabImage = createImage(vk, vkDevice, &colorImageParams);
            // Allocate and bind color image memory.
            m_multisampledCopyImageAlloc = allocateImage(vki, vk, vkPhysDevice, vkDevice, *m_multisampledCopyImage,
                                                         MemoryRequirement::Any, memAlloc, m_params.allocationKind, 0u);
            VK_CHECK(vk.bindImageMemory(vkDevice, *m_multisampledCopyImage, m_multisampledCopyImageAlloc->getMemory(),
                                        m_multisampledCopyImageAlloc->getOffset()));
            m_multisampledCopyImageNoCabAlloc =
                allocateImage(vki, vk, vkPhysDevice, vkDevice, *m_multisampledCopyNoCabImage, MemoryRequirement::Any,
                              memAlloc, m_params.allocationKind, 0u);
            VK_CHECK(vk.bindImageMemory(vkDevice, *m_multisampledCopyNoCabImage,
                                        m_multisampledCopyImageNoCabAlloc->getMemory(),
                                        m_multisampledCopyImageNoCabAlloc->getOffset()));
            break;
        }

        default:
            break;
        }
    }

    // Create destination image.
    {
        VkImageCreateInfo destinationImageParams = {
            VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,                               // VkStructureType sType;
            DE_NULL,                                                           // const void* pNext;
            getCreateFlags(m_params.dst.image),                                // VkImageCreateFlags flags;
            m_params.dst.image.imageType,                                      // VkImageType imageType;
            m_params.dst.image.format,                                         // VkFormat format;
            getExtent3D(m_params.dst.image),                                   // VkExtent3D extent;
            1u,                                                                // uint32_t mipLevels;
            getArraySize(m_params.dst.image),                                  // uint32_t arraySize;
            VK_SAMPLE_COUNT_1_BIT,                                             // uint32_t samples;
            VK_IMAGE_TILING_OPTIMAL,                                           // VkImageTiling tiling;
            VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, // VkImageUsageFlags usage;
            VK_SHARING_MODE_EXCLUSIVE,                                         // VkSharingMode sharingMode;
            0u,                                                                // uint32_t queueFamilyIndexCount;
            (const uint32_t *)DE_NULL,                                         // const uint32_t* pQueueFamilyIndices;
            VK_IMAGE_LAYOUT_UNDEFINED,                                         // VkImageLayout initialLayout;
        };

#ifndef CTS_USES_VULKANSC
        if (!params.useSparseBinding)
        {
#endif
            m_destination           = createImage(vk, m_device, &destinationImageParams);
            m_destinationImageAlloc = allocateImage(vki, vk, vkPhysDevice, m_device, *m_destination,
                                                    MemoryRequirement::Any, *m_allocator, m_params.allocationKind, 0u);
            VK_CHECK(vk.bindImageMemory(m_device, *m_destination, m_destinationImageAlloc->getMemory(),
                                        m_destinationImageAlloc->getOffset()));
#ifndef CTS_USES_VULKANSC
        }
        else
        {
            destinationImageParams.flags |=
                (vk::VK_IMAGE_CREATE_SPARSE_BINDING_BIT | vk::VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT);
            vk::VkImageFormatProperties imageFormatProperties;
            if (vki.getPhysicalDeviceImageFormatProperties(
                    vkPhysDevice, destinationImageParams.format, destinationImageParams.imageType,
                    destinationImageParams.tiling, destinationImageParams.usage, destinationImageParams.flags,
                    &imageFormatProperties) == vk::VK_ERROR_FORMAT_NOT_SUPPORTED)
            {
                TCU_THROW(NotSupportedError, "Image format not supported");
            }
            m_destination     = createImage(vk, m_device, &destinationImageParams);
            m_sparseSemaphore = createSemaphore(vk, m_device);
            allocateAndBindSparseImage(vk, m_device, vkPhysDevice, vki, destinationImageParams, m_sparseSemaphore.get(),
                                       context.getSparseQueue(), *m_allocator, m_sparseAllocations,
                                       mapVkFormat(destinationImageParams.format), m_destination.get());
        }
#endif
    }

    // Barriers for image clearing.
    std::vector<VkImageMemoryBarrier> srcImageBarriers;

    const VkImageMemoryBarrier m_multisampledImageBarrier = {
        VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, // VkStructureType sType;
        DE_NULL,                                // const void* pNext;
        0u,                                     // VkAccessFlags srcAccessMask;
        VK_ACCESS_MEMORY_WRITE_BIT,             // VkAccessFlags dstAccessMask;
        VK_IMAGE_LAYOUT_UNDEFINED,              // VkImageLayout oldLayout;
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,   // VkImageLayout newLayout;
        VK_QUEUE_FAMILY_IGNORED,                // uint32_t srcQueueFamilyIndex;
        VK_QUEUE_FAMILY_IGNORED,                // uint32_t dstQueueFamilyIndex;
        m_multisampledImage.get(),              // VkImage image;
        {
            // VkImageSubresourceRange subresourceRange;
            VK_IMAGE_ASPECT_COLOR_BIT,       // VkImageAspectFlags aspectMask;
            0u,                              // uint32_t baseMipLevel;
            1u,                              // uint32_t mipLevels;
            0u,                              // uint32_t baseArraySlice;
            getArraySize(m_params.src.image) // uint32_t arraySize;
        }};
    const VkImageMemoryBarrier m_multisampledCopyImageBarrier = {
        VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, // VkStructureType sType;
        DE_NULL,                                // const void* pNext;
        0u,                                     // VkAccessFlags srcAccessMask;
        VK_ACCESS_MEMORY_WRITE_BIT,             // VkAccessFlags dstAccessMask;
        VK_IMAGE_LAYOUT_UNDEFINED,              // VkImageLayout oldLayout;
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,   // VkImageLayout newLayout;
        VK_QUEUE_FAMILY_IGNORED,                // uint32_t srcQueueFamilyIndex;
        VK_QUEUE_FAMILY_IGNORED,                // uint32_t dstQueueFamilyIndex;
        m_multisampledCopyImage.get(),          // VkImage image;
        {
            // VkImageSubresourceRange subresourceRange;
            VK_IMAGE_ASPECT_COLOR_BIT,       // VkImageAspectFlags aspectMask;
            0u,                              // uint32_t baseMipLevel;
            1u,                              // uint32_t mipLevels;
            0u,                              // uint32_t baseArraySlice;
            getArraySize(m_params.dst.image) // uint32_t arraySize;
        }};
    const VkImageMemoryBarrier m_multisampledCopyImageNoCabBarrier = {
        VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, // VkStructureType sType;
        DE_NULL,                                // const void* pNext;
        0u,                                     // VkAccessFlags srcAccessMask;
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,   // VkAccessFlags dstAccessMask;
        VK_IMAGE_LAYOUT_UNDEFINED,              // VkImageLayout oldLayout;
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,   // VkImageLayout newLayout;
        VK_QUEUE_FAMILY_IGNORED,                // uint32_t srcQueueFamilyIndex;
        VK_QUEUE_FAMILY_IGNORED,                // uint32_t dstQueueFamilyIndex;
        m_multisampledCopyNoCabImage.get(),     // VkImage image;
        {
            // VkImageSubresourceRange subresourceRange;
            VK_IMAGE_ASPECT_COLOR_BIT,       // VkImageAspectFlags aspectMask;
            0u,                              // uint32_t baseMipLevel;
            1u,                              // uint32_t mipLevels;
            0u,                              // uint32_t baseArraySlice;
            getArraySize(m_params.dst.image) // uint32_t arraySize;
        }};

    // Only use one barrier if no options have been given.
    if (m_options != DE_NULL)
    {
        srcImageBarriers.push_back(m_multisampledImageBarrier);
        srcImageBarriers.push_back(m_multisampledCopyImageBarrier);
        // Add the third barrier if option is as below.
        if (m_options == COPY_MS_IMAGE_TO_MS_IMAGE_NO_CAB)
            srcImageBarriers.push_back(m_multisampledCopyImageNoCabBarrier);
    }
    else
    {
        srcImageBarriers.push_back(m_multisampledImageBarrier);
    }

    // Create render pass.
    {
        const VkAttachmentDescription attachmentDescription = {
            0u,                                   // VkAttachmentDescriptionFlags flags;
            m_params.src.image.format,            // VkFormat format;
            rasterizationSamples,                 // VkSampleCountFlagBits samples;
            VK_ATTACHMENT_LOAD_OP_CLEAR,          // VkAttachmentLoadOp loadOp;
            VK_ATTACHMENT_STORE_OP_STORE,         // VkAttachmentStoreOp storeOp;
            VK_ATTACHMENT_LOAD_OP_DONT_CARE,      // VkAttachmentLoadOp stencilLoadOp;
            VK_ATTACHMENT_STORE_OP_DONT_CARE,     // VkAttachmentStoreOp stencilStoreOp;
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, // VkImageLayout initialLayout;
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL  // VkImageLayout finalLayout;
        };

        const VkAttachmentReference colorAttachmentReference = {
            0u,                                      // uint32_t attachment;
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL // VkImageLayout layout;
        };

        const VkSubpassDescription subpassDescription = {
            0u,                              // VkSubpassDescriptionFlags flags;
            VK_PIPELINE_BIND_POINT_GRAPHICS, // VkPipelineBindPoint pipelineBindPoint;
            0u,                              // uint32_t inputAttachmentCount;
            DE_NULL,                         // const VkAttachmentReference* pInputAttachments;
            1u,                              // uint32_t colorAttachmentCount;
            &colorAttachmentReference,       // const VkAttachmentReference* pColorAttachments;
            DE_NULL,                         // const VkAttachmentReference* pResolveAttachments;
            DE_NULL,                         // const VkAttachmentReference* pDepthStencilAttachment;
            0u,                              // uint32_t preserveAttachmentCount;
            DE_NULL                          // const VkAttachmentReference* pPreserveAttachments;
        };

        // Subpass dependency is used to synchronize the memory access of the image clear and color attachment write in some test cases.
        const VkSubpassDependency subpassDependency = {
            VK_SUBPASS_EXTERNAL,                           //uint32_t srcSubpass;
            0u,                                            //uint32_t dstSubpass;
            VK_PIPELINE_STAGE_TRANSFER_BIT,                //VkPipelineStageFlags srcStageMask;
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, //VkPipelineStageFlags dstStageMask;
            VK_ACCESS_TRANSFER_WRITE_BIT,                  //VkAccessFlags srcAccessMask;
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,          //VkAccessFlags dstAccessMask;
            0u                                             //VkDependencyFlags dependencyFlags;
        };

        const bool useSubpassDependency =
            m_options == COPY_MS_IMAGE_LAYER_TO_MS_IMAGE || m_options == COPY_MS_IMAGE_TO_MS_IMAGE_MULTIREGION;
        const VkRenderPassCreateInfo renderPassParams = {
            VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO, // VkStructureType sType;
            DE_NULL,                                   // const void* pNext;
            0u,                                        // VkRenderPassCreateFlags flags;
            1u,                                        // uint32_t attachmentCount;
            &attachmentDescription,                    // const VkAttachmentDescription* pAttachments;
            1u,                                        // uint32_t subpassCount;
            &subpassDescription,                       // const VkSubpassDescription* pSubpasses;
            useSubpassDependency ? 1u : 0u,            // uint32_t dependencyCount;
            &subpassDependency                         // const VkSubpassDependency* pDependencies;
        };

        renderPass = createRenderPass(vk, vkDevice, &renderPassParams);
    }

    // Create pipeline layout
    {
        const VkPipelineLayoutCreateInfo pipelineLayoutParams = {
            VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, // VkStructureType sType;
            DE_NULL,                                       // const void* pNext;
            0u,                                            // VkPipelineLayoutCreateFlags flags;
            0u,                                            // uint32_t setLayoutCount;
            DE_NULL,                                       // const VkDescriptorSetLayout* pSetLayouts;
            0u,                                            // uint32_t pushConstantRangeCount;
            DE_NULL                                        // const VkPushConstantRange* pPushConstantRanges;
        };

        pipelineLayout = createPipelineLayout(vk, vkDevice, &pipelineLayoutParams);
    }

    // Create upper half triangle.
    {
        const tcu::Vec4 a(-1.0, -1.0, 0.0, 1.0);
        const tcu::Vec4 b(1.0, -1.0, 0.0, 1.0);
        const tcu::Vec4 c(1.0, 1.0, 0.0, 1.0);
        // Add triangle.
        vertices.push_back(a);
        vertices.push_back(c);
        vertices.push_back(b);
    }

    // Create vertex buffer.
    {
        const VkDeviceSize vertexDataSize           = vertices.size() * sizeof(tcu::Vec4);
        const VkBufferCreateInfo vertexBufferParams = {
            VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, // VkStructureType sType;
            DE_NULL,                              // const void* pNext;
            0u,                                   // VkBufferCreateFlags flags;
            vertexDataSize,                       // VkDeviceSize size;
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,    // VkBufferUsageFlags usage;
            VK_SHARING_MODE_EXCLUSIVE,            // VkSharingMode sharingMode;
            0u,                                   // uint32_t queueFamilyIndexCount;
            (const uint32_t *)DE_NULL,            // const uint32_t* pQueueFamilyIndices;
        };

        vertexBuffer      = createBuffer(vk, vkDevice, &vertexBufferParams);
        vertexBufferAlloc = allocateBuffer(vki, vk, vkPhysDevice, vkDevice, *vertexBuffer,
                                           MemoryRequirement::HostVisible, memAlloc, m_params.allocationKind);
        VK_CHECK(vk.bindBufferMemory(vkDevice, *vertexBuffer, vertexBufferAlloc->getMemory(),
                                     vertexBufferAlloc->getOffset()));

        // Load vertices into vertex buffer.
        deMemcpy(vertexBufferAlloc->getHostPtr(), vertices.data(), (size_t)vertexDataSize);
        flushAlloc(vk, vkDevice, *vertexBufferAlloc);
    }

    {
        Move<VkFramebuffer> framebuffer;
        Move<VkImageView> sourceAttachmentView;

        uint32_t baseArrayLayer = m_options == COPY_MS_IMAGE_LAYER_TO_MS_IMAGE ? 2u : 0u;

        // Create color attachment view.
        {
            const VkImageViewCreateInfo colorAttachmentViewParams = {
                VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,               // VkStructureType sType;
                DE_NULL,                                                // const void* pNext;
                0u,                                                     // VkImageViewCreateFlags flags;
                *m_multisampledImage,                                   // VkImage image;
                VK_IMAGE_VIEW_TYPE_2D,                                  // VkImageViewType viewType;
                m_params.src.image.format,                              // VkFormat format;
                componentMappingRGBA,                                   // VkComponentMapping components;
                {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, baseArrayLayer, 1u} // VkImageSubresourceRange subresourceRange;
            };
            sourceAttachmentView = createImageView(vk, vkDevice, &colorAttachmentViewParams);
        }

        // Create framebuffer
        {
            const VkFramebufferCreateInfo framebufferParams = {
                VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO, // VkStructureType sType;
                DE_NULL,                                   // const void* pNext;
                0u,                                        // VkFramebufferCreateFlags flags;
                *renderPass,                               // VkRenderPass renderPass;
                1u,                                        // uint32_t attachmentCount;
                &sourceAttachmentView.get(),               // const VkImageView* pAttachments;
                m_params.src.image.extent.width,           // uint32_t width;
                m_params.src.image.extent.height,          // uint32_t height;
                1u                                         // uint32_t layers;
            };

            framebuffer = createFramebuffer(vk, vkDevice, &framebufferParams);
        }

        // Create pipeline
        {
            const std::vector<VkViewport> viewports(1, makeViewport(m_params.src.image.extent));
            const std::vector<VkRect2D> scissors(1, makeRect2D(m_params.src.image.extent));

            const VkPipelineMultisampleStateCreateInfo multisampleStateParams = {
                VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, // VkStructureType sType;
                DE_NULL,                                                  // const void* pNext;
                0u,                   // VkPipelineMultisampleStateCreateFlags flags;
                rasterizationSamples, // VkSampleCountFlagBits rasterizationSamples;
                VK_FALSE,             // VkBool32 sampleShadingEnable;
                0.0f,                 // float minSampleShading;
                DE_NULL,              // const VkSampleMask* pSampleMask;
                VK_FALSE,             // VkBool32 alphaToCoverageEnable;
                VK_FALSE              // VkBool32 alphaToOneEnable;
            };

            graphicsPipeline = makeGraphicsPipeline(
                vk,                    // const DeviceInterface&                        vk
                vkDevice,              // const VkDevice                                device
                *pipelineLayout,       // const VkPipelineLayout                        pipelineLayout
                *vertexShaderModule,   // const VkShaderModule                          vertexShaderModule
                VK_NULL_HANDLE,        // const VkShaderModule                          tessellationControlModule
                VK_NULL_HANDLE,        // const VkShaderModule                          tessellationEvalModule
                VK_NULL_HANDLE,        // const VkShaderModule                          geometryShaderModule
                *fragmentShaderModule, // const VkShaderModule                          fragmentShaderModule
                *renderPass,           // const VkRenderPass                            renderPass
                viewports,             // const std::vector<VkViewport>&                viewports
                scissors,              // const std::vector<VkRect2D>&                  scissors
                VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, // const VkPrimitiveTopology                     topology
                0u,                                  // const uint32_t                                subpass
                0u,                                  // const uint32_t                                patchControlPoints
                DE_NULL,                  // const VkPipelineVertexInputStateCreateInfo*   vertexInputStateCreateInfo
                DE_NULL,                  // const VkPipelineRasterizationStateCreateInfo* rasterizationStateCreateInfo
                &multisampleStateParams); // const VkPipelineMultisampleStateCreateInfo*   multisampleStateCreateInfo
        }

        // Create command buffer
        {
            beginCommandBuffer(vk, *m_universalCmdBuffer, 0u);

            if (m_options == COPY_MS_IMAGE_LAYER_TO_MS_IMAGE || m_options == COPY_MS_IMAGE_TO_MS_IMAGE_MULTIREGION)
            {
                // Change the image layouts.
                vk.cmdPipelineBarrier(*m_universalCmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                      VK_PIPELINE_STAGE_TRANSFER_BIT, (VkDependencyFlags)0, 0,
                                      (const VkMemoryBarrier *)DE_NULL, 0, (const VkBufferMemoryBarrier *)DE_NULL,
                                      (uint32_t)srcImageBarriers.size(), srcImageBarriers.data());

                // Clear the 'm_multisampledImage'.
                {
                    const VkClearColorValue clearValue = {{0.0f, 0.0f, 0.0f, 1.0f}};
                    const auto clearRange = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u,
                                                                      m_params.src.image.extent.depth);
                    vk.cmdClearColorImage(*m_universalCmdBuffer, m_multisampledImage.get(),
                                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearValue, 1u, &clearRange);
                }

                // Clear the 'm_multisampledCopyImage' with different color.
                {
                    const VkClearColorValue clearValue = {{1.0f, 1.0f, 1.0f, 1.0f}};
                    const auto clearRange = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u,
                                                                      m_params.src.image.extent.depth);
                    vk.cmdClearColorImage(*m_universalCmdBuffer, m_multisampledCopyImage.get(),
                                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearValue, 1u, &clearRange);
                }
            }
            else
            {
                // Change the image layouts.
                vk.cmdPipelineBarrier(*m_universalCmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, (VkDependencyFlags)0, 0,
                                      (const VkMemoryBarrier *)DE_NULL, 0, (const VkBufferMemoryBarrier *)DE_NULL,
                                      (uint32_t)srcImageBarriers.size(), srcImageBarriers.data());
            }

            beginRenderPass(vk, *m_universalCmdBuffer, *renderPass, *framebuffer,
                            makeRect2D(0, 0, m_params.src.image.extent.width, m_params.src.image.extent.height),
                            tcu::Vec4(0.0f, 0.0f, 1.0f, 1.0f));

            const VkDeviceSize vertexBufferOffset = 0u;

            vk.cmdBindPipeline(*m_universalCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *graphicsPipeline);
            vk.cmdBindVertexBuffers(*m_universalCmdBuffer, 0, 1, &vertexBuffer.get(), &vertexBufferOffset);
            vk.cmdDraw(*m_universalCmdBuffer, (uint32_t)vertices.size(), 1, 0, 0);

            endRenderPass(vk, *m_universalCmdBuffer);
            endCommandBuffer(vk, *m_universalCmdBuffer);
        }

        if (m_params.useSparseBinding)
        {
            const VkPipelineStageFlags stageBits[] = {VK_PIPELINE_STAGE_TRANSFER_BIT};
            submitCommandsAndWait(vk, vkDevice, m_universalQueue, *m_universalCmdBuffer, false, 1u, 1u,
                                  &*m_sparseSemaphore, stageBits);
        }
        else
        {
            submitCommandsAndWait(vk, vkDevice, m_universalQueue, *m_universalCmdBuffer);
        }

        m_context.resetCommandPoolForVKSC(vkDevice, *m_universalCmdPool);
    }
}

tcu::TestStatus ResolveImageToImage::iterate(void)
{
    const tcu::TextureFormat srcTcuFormat = mapVkFormat(m_params.src.image.format);
    const tcu::TextureFormat dstTcuFormat = mapVkFormat(m_params.dst.image.format);

    // upload the destination image
    m_destinationTextureLevel = de::MovePtr<tcu::TextureLevel>(
        new tcu::TextureLevel(dstTcuFormat, (int)m_params.dst.image.extent.width, (int)m_params.dst.image.extent.height,
                              (int)m_params.dst.image.extent.depth));
    generateBuffer(m_destinationTextureLevel->getAccess(), m_params.dst.image.extent.width,
                   m_params.dst.image.extent.height, m_params.dst.image.extent.depth);
    uploadImage(m_destinationTextureLevel->getAccess(), m_destination.get(), m_params.dst.image);

    m_sourceTextureLevel = de::MovePtr<tcu::TextureLevel>(
        new tcu::TextureLevel(srcTcuFormat, (int)m_params.src.image.extent.width, (int)m_params.src.image.extent.height,
                              (int)m_params.dst.image.extent.depth));

    generateBuffer(m_sourceTextureLevel->getAccess(), m_params.src.image.extent.width, m_params.src.image.extent.height,
                   m_params.dst.image.extent.depth, FILL_MODE_MULTISAMPLE);
    generateExpectedResult();

    VkImage sourceImage      = m_multisampledImage.get();
    uint32_t sourceArraySize = getArraySize(m_params.src.image);

    switch (m_options)
    {
    case COPY_MS_IMAGE_LAYER_TO_MS_IMAGE:
    case COPY_MS_IMAGE_TO_ARRAY_MS_IMAGE:
        // Duplicate the multisampled image to a multisampled image array
        sourceArraySize = getArraySize(m_params.dst.image); // fall through
    case COPY_MS_IMAGE_TO_MS_IMAGE_MULTIREGION:
    case COPY_MS_IMAGE_TO_MS_IMAGE_NO_CAB:
    case COPY_MS_IMAGE_TO_MS_IMAGE_COMPUTE:
    case COPY_MS_IMAGE_TO_MS_IMAGE_TRANSFER:
    case COPY_MS_IMAGE_TO_MS_IMAGE:
        copyMSImageToMSImage(sourceArraySize);
        sourceImage = m_multisampledCopyImage.get();
        break;
    default:
        break;
    }

    const DeviceInterface &vk = m_context.getDeviceInterface();
    const VkDevice vkDevice   = m_device;

    std::vector<VkImageResolve> imageResolves;
    std::vector<VkImageResolve2KHR> imageResolves2KHR;
    for (CopyRegion region : m_params.regions)
    {
        // If copying multiple regions, make sure that the same regions are
        // used for resolving as the ones used for copying.
        if (m_options == COPY_MS_IMAGE_TO_MS_IMAGE_MULTIREGION)
        {
            VkExtent3D partialExtent = {getExtent3D(m_params.src.image).width / 2,
                                        getExtent3D(m_params.src.image).height / 2,
                                        getExtent3D(m_params.src.image).depth};

            const VkImageResolve imageResolve = {
                region.imageResolve.srcSubresource, // VkImageSubresourceLayers srcSubresource;
                region.imageResolve.dstOffset,      // VkOffset3D srcOffset;
                region.imageResolve.dstSubresource, // VkImageSubresourceLayers dstSubresource;
                region.imageResolve.dstOffset,      // VkOffset3D dstOffset;
                partialExtent,                      // VkExtent3D extent;
            };

            if (!(m_params.extensionFlags & COPY_COMMANDS_2))
            {
                imageResolves.push_back(imageResolve);
            }
            else
            {
                DE_ASSERT(m_params.extensionFlags & COPY_COMMANDS_2);
                imageResolves2KHR.push_back(convertvkImageResolveTovkImageResolve2KHR(imageResolve));
            }
        }
        else
        {
            if (!(m_params.extensionFlags & COPY_COMMANDS_2))
            {
                imageResolves.push_back(region.imageResolve);
            }
            else
            {
                DE_ASSERT(m_params.extensionFlags & COPY_COMMANDS_2);
                imageResolves2KHR.push_back(convertvkImageResolveTovkImageResolve2KHR(region.imageResolve));
            }
        }
    }

    const VkImageMemoryBarrier imageBarriers[] = {
        // source image
        {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, // VkStructureType sType;
         DE_NULL,                                // const void* pNext;
         VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,   // VkAccessFlags srcAccessMask;
         VK_ACCESS_TRANSFER_READ_BIT,            // VkAccessFlags dstAccessMask;
         m_options == NO_OPTIONAL_OPERATION ? m_params.dst.image.operationLayout :
                                              m_params.src.image.operationLayout, // VkImageLayout oldLayout;
         m_params.src.image.operationLayout,                                      // VkImageLayout newLayout;
         VK_QUEUE_FAMILY_IGNORED,                                                 // uint32_t srcQueueFamilyIndex;
         VK_QUEUE_FAMILY_IGNORED,                                                 // uint32_t dstQueueFamilyIndex;
         sourceImage,                                                             // VkImage image;
         {
             // VkImageSubresourceRange subresourceRange;
             getAspectFlags(srcTcuFormat), // VkImageAspectFlags aspectMask;
             0u,                           // uint32_t baseMipLevel;
             1u,                           // uint32_t mipLevels;
             0u,                           // uint32_t baseArraySlice;
             sourceArraySize               // uint32_t arraySize;
         }},
        // destination image
        {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, // VkStructureType sType;
         DE_NULL,                                // const void* pNext;
         0u,                                     // VkAccessFlags srcAccessMask;
         VK_ACCESS_TRANSFER_WRITE_BIT,           // VkAccessFlags dstAccessMask;
         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,   // VkImageLayout oldLayout;
         m_params.dst.image.operationLayout,     // VkImageLayout newLayout;
         VK_QUEUE_FAMILY_IGNORED,                // uint32_t srcQueueFamilyIndex;
         VK_QUEUE_FAMILY_IGNORED,                // uint32_t dstQueueFamilyIndex;
         m_destination.get(),                    // VkImage image;
         {
             // VkImageSubresourceRange subresourceRange;
             getAspectFlags(dstTcuFormat),    // VkImageAspectFlags aspectMask;
             0u,                              // uint32_t baseMipLevel;
             1u,                              // uint32_t mipLevels;
             0u,                              // uint32_t baseArraySlice;
             getArraySize(m_params.dst.image) // uint32_t arraySize;
         }},
    };

    const VkImageMemoryBarrier postImageBarrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, // VkStructureType sType;
                                                   DE_NULL,                                // const void* pNext;
                                                   VK_ACCESS_TRANSFER_WRITE_BIT,       // VkAccessFlags srcAccessMask;
                                                   VK_ACCESS_HOST_READ_BIT,            // VkAccessFlags dstAccessMask;
                                                   m_params.dst.image.operationLayout, // VkImageLayout oldLayout;
                                                   m_params.dst.image.operationLayout, // VkImageLayout newLayout;
                                                   VK_QUEUE_FAMILY_IGNORED,            // uint32_t srcQueueFamilyIndex;
                                                   VK_QUEUE_FAMILY_IGNORED,            // uint32_t dstQueueFamilyIndex;
                                                   m_destination.get(),                // VkImage image;
                                                   {
                                                       // VkImageSubresourceRange subresourceRange;
                                                       getAspectFlags(dstTcuFormat), // VkImageAspectFlags aspectMask;
                                                       0u,                           // uint32_t baseMipLevel;
                                                       1u,                           // uint32_t mipLevels;
                                                       0u,                           // uint32_t baseArraySlice;
                                                       getArraySize(m_params.dst.image) // uint32_t arraySize;
                                                   }};

    beginCommandBuffer(vk, *m_universalCmdBuffer);
    vk.cmdPipelineBarrier(*m_universalCmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                          VK_PIPELINE_STAGE_TRANSFER_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier *)DE_NULL, 0,
                          (const VkBufferMemoryBarrier *)DE_NULL, DE_LENGTH_OF_ARRAY(imageBarriers), imageBarriers);

    if (!(m_params.extensionFlags & COPY_COMMANDS_2))
    {
        vk.cmdResolveImage(*m_universalCmdBuffer, sourceImage, m_params.src.image.operationLayout, m_destination.get(),
                           m_params.dst.image.operationLayout, (uint32_t)m_params.regions.size(), imageResolves.data());
    }
    else
    {
        DE_ASSERT(m_params.extensionFlags & COPY_COMMANDS_2);
        const VkResolveImageInfo2KHR ResolveImageInfo2KHR = {
            VK_STRUCTURE_TYPE_RESOLVE_IMAGE_INFO_2_KHR, // VkStructureType sType;
            DE_NULL,                                    // const void* pNext;
            sourceImage,                                // VkImage srcImage;
            m_params.src.image.operationLayout,         // VkImageLayout srcImageLayout;
            m_destination.get(),                        // VkImage dstImage;
            m_params.dst.image.operationLayout,         // VkImageLayout dstImageLayout;
            (uint32_t)m_params.regions.size(),          // uint32_t regionCount;
            imageResolves2KHR.data()                    // const  VkImageResolve2KHR* pRegions;
        };
        vk.cmdResolveImage2(*m_universalCmdBuffer, &ResolveImageInfo2KHR);
    }

    vk.cmdPipelineBarrier(*m_universalCmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                          (VkDependencyFlags)0, 0, (const VkMemoryBarrier *)DE_NULL, 0,
                          (const VkBufferMemoryBarrier *)DE_NULL, 1, &postImageBarrier);
    endCommandBuffer(vk, *m_universalCmdBuffer);
    submitCommandsAndWait(vk, vkDevice, m_universalQueue, *m_universalCmdBuffer);
    m_context.resetCommandPoolForVKSC(vkDevice, *m_universalCmdPool);

    de::MovePtr<tcu::TextureLevel> resultTextureLevel = readImage(*m_destination, m_params.dst.image);

    if (shouldVerifyIntermediateResults(m_options))
    {
        // Verify the intermediate multisample copy operation happens properly instead of, for example, shuffling samples around or
        // resolving the image and giving every sample the same value.
        const auto intermediateResult = checkIntermediateCopy();
        if (intermediateResult.getCode() != QP_TEST_RESULT_PASS)
            return intermediateResult;
    }

    return checkTestResult(resultTextureLevel->getAccess());
}

tcu::TestStatus ResolveImageToImage::checkTestResult(tcu::ConstPixelBufferAccess result)
{
    const tcu::ConstPixelBufferAccess expected = m_expectedTextureLevel[0]->getAccess();
    const float fuzzyThreshold                 = 0.01f;

    if (m_options == COPY_MS_IMAGE_LAYER_TO_MS_IMAGE)
    {
        // Check that all the layers that have not been written to are solid white.
        tcu::Vec4 expectedColor(1.0f, 1.0f, 1.0f, 1.0f);
        for (int arrayLayerNdx = 0; arrayLayerNdx < (int)getArraySize(m_params.dst.image) - 1; ++arrayLayerNdx)
        {
            const tcu::ConstPixelBufferAccess resultSub =
                getSubregion(result, 0u, 0u, arrayLayerNdx, result.getWidth(), result.getHeight(), 1u);
            if (resultSub.getPixel(0, 0) != expectedColor)
                return tcu::TestStatus::fail("CopiesAndBlitting test. Layers image differs from initialized value.");
        }

        // Check that the layer that has been copied to is the same as the layer that has been copied from.
        const tcu::ConstPixelBufferAccess expectedSub =
            getSubregion(expected, 0u, 0u, 2u, expected.getWidth(), expected.getHeight(), 1u);
        const tcu::ConstPixelBufferAccess resultSub =
            getSubregion(result, 0u, 0u, 4u, result.getWidth(), result.getHeight(), 1u);
        if (!tcu::fuzzyCompare(m_context.getTestContext().getLog(), "Compare", "Result comparsion", expectedSub,
                               resultSub, fuzzyThreshold, tcu::COMPARE_LOG_RESULT))
            return tcu::TestStatus::fail("CopiesAndBlitting test");
    }
    else
    {
        for (int arrayLayerNdx = 0; arrayLayerNdx < (int)getArraySize(m_params.dst.image); ++arrayLayerNdx)
        {
            const tcu::ConstPixelBufferAccess expectedSub =
                getSubregion(expected, 0u, 0u, arrayLayerNdx, expected.getWidth(), expected.getHeight(), 1u);
            const tcu::ConstPixelBufferAccess resultSub =
                getSubregion(result, 0u, 0u, arrayLayerNdx, result.getWidth(), result.getHeight(), 1u);
            if (!tcu::fuzzyCompare(m_context.getTestContext().getLog(), "Compare", "Result comparsion", expectedSub,
                                   resultSub, fuzzyThreshold, tcu::COMPARE_LOG_RESULT))
                return tcu::TestStatus::fail("CopiesAndBlitting test");
        }
    }

    return tcu::TestStatus::pass("CopiesAndBlitting test");
}

void ResolveImageToImage::copyRegionToTextureLevel(tcu::ConstPixelBufferAccess src, tcu::PixelBufferAccess dst,
                                                   CopyRegion region, uint32_t mipLevel)
{
    DE_UNREF(mipLevel);

    VkOffset3D srcOffset = region.imageResolve.srcOffset;
    srcOffset.z          = region.imageResolve.srcSubresource.baseArrayLayer;
    VkOffset3D dstOffset = region.imageResolve.dstOffset;
    dstOffset.z          = region.imageResolve.dstSubresource.baseArrayLayer;
    VkExtent3D extent    = region.imageResolve.extent;
    extent.depth         = (region.imageResolve.srcSubresource.layerCount == VK_REMAINING_ARRAY_LAYERS) ?
                               (src.getDepth() - region.imageResolve.srcSubresource.baseArrayLayer) :
                               region.imageResolve.srcSubresource.layerCount;

    const tcu::ConstPixelBufferAccess srcSubRegion =
        getSubregion(src, srcOffset.x, srcOffset.y, srcOffset.z, extent.width, extent.height, extent.depth);
    // CopyImage acts like a memcpy. Replace the destination format with the srcformat to use a memcpy.
    const tcu::PixelBufferAccess dstWithSrcFormat(srcSubRegion.getFormat(), dst.getSize(), dst.getDataPtr());
    const tcu::PixelBufferAccess dstSubRegion = getSubregion(dstWithSrcFormat, dstOffset.x, dstOffset.y, dstOffset.z,
                                                             extent.width, extent.height, extent.depth);

    tcu::copy(dstSubRegion, srcSubRegion);
}

tcu::TestStatus ResolveImageToImage::checkIntermediateCopy(void)
{
    const auto &vkd          = m_context.getDeviceInterface();
    const auto device        = m_device;
    const auto queueIndex    = m_context.getUniversalQueueFamilyIndex();
    auto &alloc              = *m_allocator;
    const auto currentLayout = m_params.src.image.operationLayout;
    const auto numDstLayers  = getArraySize(m_params.dst.image);
    const auto numInputAttachments =
        m_options == COPY_MS_IMAGE_LAYER_TO_MS_IMAGE ? 2u : numDstLayers + 1u; // For the source image.
    constexpr auto numSets = 2u; // 1 for the output buffer, 1 for the input attachments.
    const auto fbWidth     = m_params.src.image.extent.width;
    const auto fbHeight    = m_params.src.image.extent.height;

    // Push constants.
    const std::array<int, 3> pushConstantData = {{
        static_cast<int>(fbWidth),
        static_cast<int>(fbHeight),
        static_cast<int>(m_params.samples),
    }};
    const auto pushConstantSize =
        static_cast<uint32_t>(pushConstantData.size() * sizeof(decltype(pushConstantData)::value_type));

    // Shader modules.
    const auto vertexModule       = createShaderModule(vkd, device, m_context.getBinaryCollection().get("vert"), 0u);
    const auto verificationModule = createShaderModule(vkd, device, m_context.getBinaryCollection().get("verify"), 0u);

    // Descriptor sets.
    DescriptorPoolBuilder poolBuilder;
    poolBuilder.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
    poolBuilder.addType(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, numInputAttachments);
    const auto descriptorPool =
        poolBuilder.build(vkd, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, numSets);

    DescriptorSetLayoutBuilder layoutBuilderBuffer;
    layoutBuilderBuffer.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT);
    const auto outputBufferSetLayout = layoutBuilderBuffer.build(vkd, device);

    DescriptorSetLayoutBuilder layoutBuilderAttachments;
    for (uint32_t i = 0u; i < numInputAttachments; ++i)
        layoutBuilderAttachments.addSingleBinding(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, VK_SHADER_STAGE_FRAGMENT_BIT);
    const auto inputAttachmentsSetLayout = layoutBuilderAttachments.build(vkd, device);

    const auto descriptorSetBuffer = makeDescriptorSet(vkd, device, descriptorPool.get(), outputBufferSetLayout.get());
    const auto descriptorSetAttachments =
        makeDescriptorSet(vkd, device, descriptorPool.get(), inputAttachmentsSetLayout.get());

    // Array with raw descriptor sets.
    const std::array<VkDescriptorSet, numSets> descriptorSets = {{
        descriptorSetBuffer.get(),
        descriptorSetAttachments.get(),
    }};

    // Pipeline layout.
    const std::array<VkDescriptorSetLayout, numSets> setLayouts = {{
        outputBufferSetLayout.get(),
        inputAttachmentsSetLayout.get(),
    }};

    const VkPushConstantRange pushConstantRange = {
        VK_SHADER_STAGE_FRAGMENT_BIT, // VkShaderStageFlags stageFlags;
        0u,                           // uint32_t offset;
        pushConstantSize,             // uint32_t size;
    };

    const VkPipelineLayoutCreateInfo pipelineLayoutInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, // VkStructureType sType;
        nullptr,                                       // const void* pNext;
        0u,                                            // VkPipelineLayoutCreateFlags flags;
        static_cast<uint32_t>(setLayouts.size()),      // uint32_t setLayoutCount;
        setLayouts.data(),                             // const VkDescriptorSetLayout* pSetLayouts;
        1u,                                            // uint32_t pushConstantRangeCount;
        &pushConstantRange,                            // const VkPushConstantRange* pPushConstantRanges;
    };

    const auto pipelineLayout = createPipelineLayout(vkd, device, &pipelineLayoutInfo);

    // Render pass.
    const VkAttachmentDescription commonAttachmentDescription = {
        0u,                               // VkAttachmentDescriptionFlags flags;
        m_params.src.image.format,        // VkFormat format;
        m_params.samples,                 // VkSampleCountFlagBits samples;
        VK_ATTACHMENT_LOAD_OP_LOAD,       // VkAttachmentLoadOp loadOp;
        VK_ATTACHMENT_STORE_OP_STORE,     // VkAttachmentStoreOp storeOp;
        VK_ATTACHMENT_LOAD_OP_DONT_CARE,  // VkAttachmentLoadOp stencilLoadOp;
        VK_ATTACHMENT_STORE_OP_DONT_CARE, // VkAttachmentStoreOp stencilStoreOp;
        currentLayout,                    // VkImageLayout initialLayout;
        currentLayout,                    // VkImageLayout finalLayout;
    };
    const std::vector<VkAttachmentDescription> attachmentDescriptions(numInputAttachments, commonAttachmentDescription);

    std::vector<VkAttachmentReference> inputAttachmentReferences;
    inputAttachmentReferences.reserve(numInputAttachments);
    for (uint32_t i = 0u; i < numInputAttachments; ++i)
    {
        const VkAttachmentReference reference = {i, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        inputAttachmentReferences.push_back(reference);
    }

    const VkSubpassDescription subpassDescription = {
        0u,                                                      // VkSubpassDescriptionFlags flags;
        VK_PIPELINE_BIND_POINT_GRAPHICS,                         // VkPipelineBindPoint pipelineBindPoint;
        static_cast<uint32_t>(inputAttachmentReferences.size()), // uint32_t inputAttachmentCount;
        inputAttachmentReferences.data(),                        // const VkAttachmentReference* pInputAttachments;
        0u,                                                      // uint32_t colorAttachmentCount;
        nullptr,                                                 // const VkAttachmentReference* pColorAttachments;
        nullptr,                                                 // const VkAttachmentReference* pResolveAttachments;
        nullptr, // const VkAttachmentReference* pDepthStencilAttachment;
        0u,      // uint32_t preserveAttachmentCount;
        nullptr, // const uint32_t* pPreserveAttachments;
    };

    // self-dependency - load op is considered to write the attachment
    const VkSubpassDependency subpassDependency{
        0,                                             // uint32_t srcSubpass;
        0,                                             // uint32_t dstSubpass;
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, // VkPipelineStageFlags srcStageMask;
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, // VkPipelineStageFlags dstStageMask;
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,          // VkAccessFlags srcAccessMask;
        VK_ACCESS_COLOR_ATTACHMENT_READ_BIT,           // VkAccessFlags dstAccessMask;
        VK_DEPENDENCY_BY_REGION_BIT                    // VkDependencyFlags dependencyFlags;
    };

    const VkRenderPassCreateInfo renderPassInfo{
        VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,            // VkStructureType sType;
        nullptr,                                              // const void* pNext;
        0u,                                                   // VkRenderPassCreateFlags flags;
        static_cast<uint32_t>(attachmentDescriptions.size()), // uint32_t attachmentCount;
        attachmentDescriptions.data(),                        // const VkAttachmentDescription* pAttachments;
        1u,                                                   // uint32_t subpassCount;
        &subpassDescription,                                  // const VkSubpassDescription* pSubpasses;
        1u,                                                   // uint32_t dependencyCount;
        &subpassDependency,                                   // const VkSubpassDependency* pDependencies;
    };

    const auto renderPass = createRenderPass(vkd, device, &renderPassInfo);

    // Framebuffer.
    std::vector<Move<VkImageView>> imageViews;
    std::vector<VkImageView> imageViewsRaw;

    if (m_options == COPY_MS_IMAGE_LAYER_TO_MS_IMAGE)
    {
        imageViews.push_back(makeImageView(vkd, device, m_multisampledImage.get(), VK_IMAGE_VIEW_TYPE_2D,
                                           m_params.src.image.format,
                                           makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 2u, 1u)));
        imageViews.push_back(makeImageView(vkd, device, m_multisampledCopyImage.get(), VK_IMAGE_VIEW_TYPE_2D,
                                           m_params.src.image.format,
                                           makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 4u, 1u)));
    }
    else
    {
        imageViews.push_back(makeImageView(vkd, device, m_multisampledImage.get(), VK_IMAGE_VIEW_TYPE_2D,
                                           m_params.src.image.format,
                                           makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u)));
        for (uint32_t i = 0u; i < numDstLayers; ++i)
        {
            const auto subresourceRange = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, i, 1u);
            imageViews.push_back(makeImageView(vkd, device, m_multisampledCopyImage.get(), VK_IMAGE_VIEW_TYPE_2D,
                                               m_params.dst.image.format, subresourceRange));
        }
    }

    imageViewsRaw.reserve(imageViews.size());
    std::transform(begin(imageViews), end(imageViews), std::back_inserter(imageViewsRaw),
                   [](const Move<VkImageView> &ptr) { return ptr.get(); });

    const auto framebuffer = makeFramebuffer(vkd, device, renderPass.get(), static_cast<uint32_t>(imageViewsRaw.size()),
                                             imageViewsRaw.data(), fbWidth, fbHeight);

    // Storage buffer.
    const auto bufferCount = static_cast<size_t>(fbWidth * fbHeight * m_params.samples);
    const auto bufferSize  = static_cast<VkDeviceSize>(bufferCount * sizeof(int32_t));
    BufferWithMemory buffer(vkd, device, alloc, makeBufferCreateInfo(bufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT),
                            MemoryRequirement::HostVisible);
    auto &bufferAlloc = buffer.getAllocation();
    void *bufferData  = bufferAlloc.getHostPtr();

    // Update descriptor sets.
    DescriptorSetUpdateBuilder updater;

    const auto bufferInfo = makeDescriptorBufferInfo(buffer.get(), 0ull, bufferSize);
    updater.writeSingle(descriptorSetBuffer.get(), DescriptorSetUpdateBuilder::Location::binding(0u),
                        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &bufferInfo);

    std::vector<VkDescriptorImageInfo> imageInfos;
    imageInfos.reserve(imageViewsRaw.size());
    for (size_t i = 0; i < imageViewsRaw.size(); ++i)
        imageInfos.push_back(
            makeDescriptorImageInfo(VK_NULL_HANDLE, imageViewsRaw[i], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL));

    for (size_t i = 0; i < imageInfos.size(); ++i)
        updater.writeSingle(descriptorSetAttachments.get(),
                            DescriptorSetUpdateBuilder::Location::binding(static_cast<uint32_t>(i)),
                            VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, &imageInfos[i]);

    updater.update(vkd, device);

    // Vertex buffer.
    std::vector<tcu::Vec4> fullScreenQuad;
    {
        // Full screen quad so every framebuffer pixel and sample location is verified by the shader.
        const tcu::Vec4 topLeft(-1.0f, -1.0f, 0.0f, 1.0f);
        const tcu::Vec4 topRight(1.0f, -1.0f, 0.0f, 1.0f);
        const tcu::Vec4 bottomLeft(-1.0f, 1.0f, 0.0f, 1.0f);
        const tcu::Vec4 bottomRight(1.0f, 1.0f, 0.0f, 1.0f);

        fullScreenQuad.reserve(6u);
        fullScreenQuad.push_back(topLeft);
        fullScreenQuad.push_back(topRight);
        fullScreenQuad.push_back(bottomRight);
        fullScreenQuad.push_back(topLeft);
        fullScreenQuad.push_back(bottomRight);
        fullScreenQuad.push_back(bottomLeft);
    }

    const auto vertexBufferSize =
        static_cast<VkDeviceSize>(fullScreenQuad.size() * sizeof(decltype(fullScreenQuad)::value_type));
    const auto vertexBufferInfo = makeBufferCreateInfo(vertexBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    const BufferWithMemory vertexBuffer(vkd, device, alloc, vertexBufferInfo, MemoryRequirement::HostVisible);
    const auto vertexBufferHandler        = vertexBuffer.get();
    auto &vertexBufferAlloc               = vertexBuffer.getAllocation();
    void *vertexBufferData                = vertexBufferAlloc.getHostPtr();
    const VkDeviceSize vertexBufferOffset = 0ull;

    deMemcpy(vertexBufferData, fullScreenQuad.data(), static_cast<size_t>(vertexBufferSize));
    flushAlloc(vkd, device, vertexBufferAlloc);

    // Graphics pipeline.
    const std::vector<VkViewport> viewports(1, makeViewport(m_params.src.image.extent));
    const std::vector<VkRect2D> scissors(1, makeRect2D(m_params.src.image.extent));

    const VkPipelineMultisampleStateCreateInfo multisampleStateParams = {
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, // VkStructureType sType;
        nullptr,                                                  // const void* pNext;
        0u,                                                       // VkPipelineMultisampleStateCreateFlags flags;
        VK_SAMPLE_COUNT_1_BIT,                                    // VkSampleCountFlagBits rasterizationSamples;
        VK_FALSE,                                                 // VkBool32 sampleShadingEnable;
        0.0f,                                                     // float minSampleShading;
        nullptr,                                                  // const VkSampleMask* pSampleMask;
        VK_FALSE,                                                 // VkBool32 alphaToCoverageEnable;
        VK_FALSE                                                  // VkBool32 alphaToOneEnable;
    };

    const auto graphicsPipeline = makeGraphicsPipeline(
        vkd,                                 // const DeviceInterface&                        vk
        device,                              // const VkDevice                                device
        pipelineLayout.get(),                // const VkPipelineLayout                        pipelineLayout
        vertexModule.get(),                  // const VkShaderModule                          vertexShaderModule
        VK_NULL_HANDLE,                      // const VkShaderModule                          tessellationControlModule
        VK_NULL_HANDLE,                      // const VkShaderModule                          tessellationEvalModule
        VK_NULL_HANDLE,                      // const VkShaderModule                          geometryShaderModule
        verificationModule.get(),            // const VkShaderModule                          fragmentShaderModule
        renderPass.get(),                    // const VkRenderPass                            renderPass
        viewports,                           // const std::vector<VkViewport>&                viewports
        scissors,                            // const std::vector<VkRect2D>&                  scissors
        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, // const VkPrimitiveTopology                     topology
        0u,                                  // const uint32_t                                subpass
        0u,                                  // const uint32_t                                patchControlPoints
        nullptr,                             // const VkPipelineVertexInputStateCreateInfo*   vertexInputStateCreateInfo
        nullptr,                  // const VkPipelineRasterizationStateCreateInfo* rasterizationStateCreateInfo
        &multisampleStateParams); // const VkPipelineMultisampleStateCreateInfo*   multisampleStateCreateInfo

    // Command buffer.
    const auto cmdPool      = makeCommandPool(vkd, device, queueIndex);
    const auto cmdBufferPtr = allocateCommandBuffer(vkd, device, cmdPool.get(), VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    const auto cmdBuffer    = cmdBufferPtr.get();

    // Make sure multisample copy data is available to the fragment shader.
    const auto imagesBarrier = makeMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_INPUT_ATTACHMENT_READ_BIT);

    // Make sure input attachment can be read by the shader after the loadop is executed at the start of the renderpass
    const auto loadBarrier =
        makeMemoryBarrier(VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_COLOR_ATTACHMENT_READ_BIT);

    // Make sure verification buffer data is available on the host.
    const auto bufferBarrier = makeMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);

    // Record and submit command buffer.
    beginCommandBuffer(vkd, cmdBuffer);
    vkd.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0u, 1u,
                           &imagesBarrier, 0u, nullptr, 0u, nullptr);
    beginRenderPass(vkd, cmdBuffer, renderPass.get(), framebuffer.get(), makeRect2D(m_params.src.image.extent));
    vkd.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                           VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_DEPENDENCY_BY_REGION_BIT, 1u, &loadBarrier,
                           0u, nullptr, 0u, nullptr);
    vkd.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline.get());
    vkd.cmdBindVertexBuffers(cmdBuffer, 0u, 1u, &vertexBufferHandler, &vertexBufferOffset);
    vkd.cmdPushConstants(cmdBuffer, pipelineLayout.get(), VK_SHADER_STAGE_FRAGMENT_BIT, 0u, pushConstantSize,
                         pushConstantData.data());
    vkd.cmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout.get(), 0u,
                              static_cast<uint32_t>(descriptorSets.size()), descriptorSets.data(), 0u, nullptr);
    vkd.cmdDraw(cmdBuffer, static_cast<uint32_t>(fullScreenQuad.size()), 1u, 0u, 0u);
    endRenderPass(vkd, cmdBuffer);
    vkd.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0u, 1u,
                           &bufferBarrier, 0u, nullptr, 0u, nullptr);
    endCommandBuffer(vkd, cmdBuffer);
    submitCommandsAndWait(vkd, device, m_universalQueue, cmdBuffer);
    m_context.resetCommandPoolForVKSC(device, *cmdPool);

    // Verify intermediate results.
    invalidateAlloc(vkd, device, bufferAlloc);
    std::vector<int32_t> outputFlags(bufferCount, 0);
    deMemcpy(outputFlags.data(), bufferData, static_cast<size_t>(bufferSize));

    auto &log = m_context.getTestContext().getLog();
    log << tcu::TestLog::Message << "Verifying intermediate multisample copy results" << tcu::TestLog::EndMessage;

    const auto sampleCount = static_cast<uint32_t>(m_params.samples);

    for (uint32_t x = 0u; x < fbWidth; ++x)
        for (uint32_t y = 0u; y < fbHeight; ++y)
            for (uint32_t s = 0u; s < sampleCount; ++s)
            {
                const auto index = (y * fbWidth + x) * sampleCount + s;
                if (!outputFlags[index])
                {
                    std::ostringstream msg;
                    msg << "Intermediate verification failed for coordinates (" << x << ", " << y << ") sample " << s;
                    return tcu::TestStatus::fail(msg.str());
                }
            }

    log << tcu::TestLog::Message << "Intermediate multisample copy verification passed" << tcu::TestLog::EndMessage;
    return tcu::TestStatus::pass("Pass");
}

void ResolveImageToImage::copyMSImageToMSImage(uint32_t copyArraySize)
{
    const DeviceInterface &vk             = m_context.getDeviceInterface();
    const VkDevice vkDevice               = m_device;
    const tcu::TextureFormat srcTcuFormat = mapVkFormat(m_params.src.image.format);
    std::vector<VkImageCopy> imageCopies;
    std::vector<VkImageCopy2KHR> imageCopies2KHR;

    if (m_options == COPY_MS_IMAGE_LAYER_TO_MS_IMAGE)
    {
        const VkImageSubresourceLayers sourceSubresourceLayers = {
            getAspectFlags(srcTcuFormat), // VkImageAspectFlags aspectMask;
            0u,                           // uint32_t mipLevel;
            2u,                           // uint32_t baseArrayLayer;
            1u                            // uint32_t layerCount;
        };

        const VkImageSubresourceLayers destinationSubresourceLayers = {
            getAspectFlags(srcTcuFormat), // VkImageAspectFlags    aspectMask;//getAspectFlags(dstTcuFormat)
            0u,                           // uint32_t mipLevel;
            4u,                           // uint32_t baseArrayLayer;
            1u                            // uint32_t layerCount;
        };

        const VkImageCopy imageCopy = {
            sourceSubresourceLayers,         // VkImageSubresourceLayers srcSubresource;
            {0, 0, 0},                       // VkOffset3D srcOffset;
            destinationSubresourceLayers,    // VkImageSubresourceLayers dstSubresource;
            {0, 0, 0},                       // VkOffset3D dstOffset;
            getExtent3D(m_params.src.image), // VkExtent3D extent;
        };

        if (!(m_params.extensionFlags & COPY_COMMANDS_2))
        {
            imageCopies.push_back(imageCopy);
        }
        else
        {
            DE_ASSERT(m_params.extensionFlags & COPY_COMMANDS_2);
            imageCopies2KHR.push_back(convertvkImageCopyTovkImageCopy2KHR(imageCopy));
        }
    }
    else if (m_options == COPY_MS_IMAGE_TO_MS_IMAGE_MULTIREGION)
    {
        VkExtent3D partialExtent = {getExtent3D(m_params.src.image).width / 2,
                                    getExtent3D(m_params.src.image).height / 2, getExtent3D(m_params.src.image).depth};

        for (CopyRegion region : m_params.regions)
        {
            const VkImageCopy imageCopy = {
                region.imageResolve.srcSubresource, // VkImageSubresourceLayers srcSubresource;
                region.imageResolve.srcOffset,      // VkOffset3D srcOffset;
                region.imageResolve.dstSubresource, // VkImageSubresourceLayers dstSubresource;
                region.imageResolve.dstOffset,      // VkOffset3D dstOffset;
                partialExtent,                      // VkExtent3D extent;
            };

            if (!(m_params.extensionFlags & COPY_COMMANDS_2))
            {
                imageCopies.push_back(imageCopy);
            }
            else
            {
                DE_ASSERT(m_params.extensionFlags & COPY_COMMANDS_2);
                imageCopies2KHR.push_back(convertvkImageCopyTovkImageCopy2KHR(imageCopy));
            }
        }
    }
    else
    {
        for (uint32_t layerNdx = 0; layerNdx < copyArraySize; ++layerNdx)
        {
            const VkImageSubresourceLayers sourceSubresourceLayers = {
                getAspectFlags(srcTcuFormat), // VkImageAspectFlags aspectMask;
                0u,                           // uint32_t mipLevel;
                0u,                           // uint32_t baseArrayLayer;
                1u                            // uint32_t layerCount;
            };

            const VkImageSubresourceLayers destinationSubresourceLayers = {
                getAspectFlags(srcTcuFormat), // VkImageAspectFlags    aspectMask;//getAspectFlags(dstTcuFormat)
                0u,                           // uint32_t mipLevel;
                layerNdx,                     // uint32_t baseArrayLayer;
                1u                            // uint32_t layerCount;
            };

            const VkImageCopy imageCopy = {
                sourceSubresourceLayers,         // VkImageSubresourceLayers srcSubresource;
                {0, 0, 0},                       // VkOffset3D srcOffset;
                destinationSubresourceLayers,    // VkImageSubresourceLayers dstSubresource;
                {0, 0, 0},                       // VkOffset3D dstOffset;
                getExtent3D(m_params.src.image), // VkExtent3D extent;
            };

            if (!(m_params.extensionFlags & COPY_COMMANDS_2))
            {
                imageCopies.push_back(imageCopy);
            }
            else
            {
                DE_ASSERT(m_params.extensionFlags & COPY_COMMANDS_2);
                imageCopies2KHR.push_back(convertvkImageCopyTovkImageCopy2KHR(imageCopy));
            }
        }
    }

    VkImageSubresourceRange subresourceRange = {
        getAspectFlags(srcTcuFormat), // VkImageAspectFlags    aspectMask
        0u,                           // uint32_t                baseMipLevel
        1u,                           // uint32_t                mipLevels
        0u,                           // uint32_t                baseArraySlice
        copyArraySize                 // uint32_t                arraySize
    };

    // m_multisampledImage
    const VkImageMemoryBarrier m_multisampledImageBarrier = {
        VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, // VkStructureType sType;
        DE_NULL,                                // const void* pNext;
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,   // VkAccessFlags srcAccessMask;
        VK_ACCESS_TRANSFER_READ_BIT,            // VkAccessFlags dstAccessMask;
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,   // VkImageLayout oldLayout;
        m_params.src.image.operationLayout,     // VkImageLayout newLayout;
        VK_QUEUE_FAMILY_IGNORED,                // uint32_t srcQueueFamilyIndex;
        VK_QUEUE_FAMILY_IGNORED,                // uint32_t dstQueueFamilyIndex;
        m_multisampledImage.get(),              // VkImage image;
        {
            // VkImageSubresourceRange subresourceRange;
            getAspectFlags(srcTcuFormat),    // VkImageAspectFlags aspectMask;
            0u,                              // uint32_t baseMipLevel;
            1u,                              // uint32_t mipLevels;
            0u,                              // uint32_t baseArraySlice;
            getArraySize(m_params.src.image) // uint32_t arraySize;
        }};
    // m_multisampledCopyImage
    VkImageMemoryBarrier m_multisampledCopyImageBarrier = {
        VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, // VkStructureType sType;
        DE_NULL,                                // const void* pNext;
        0,                                      // VkAccessFlags srcAccessMask;
        VK_ACCESS_TRANSFER_WRITE_BIT,           // VkAccessFlags dstAccessMask;
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,   // VkImageLayout oldLayout;
        m_params.dst.image.operationLayout,     // VkImageLayout newLayout;
        VK_QUEUE_FAMILY_IGNORED,                // uint32_t srcQueueFamilyIndex;
        VK_QUEUE_FAMILY_IGNORED,                // uint32_t dstQueueFamilyIndex;
        m_multisampledCopyImage.get(),          // VkImage image;
        subresourceRange                        // VkImageSubresourceRange subresourceRange;
    };

    // m_multisampledCopyNoCabImage (no USAGE_COLOR_ATTACHMENT_BIT)
    const VkImageMemoryBarrier m_multisampledCopyNoCabImageBarrier = {
        VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, // VkStructureType sType;
        DE_NULL,                                // const void* pNext;
        0,                                      // VkAccessFlags srcAccessMask;
        VK_ACCESS_TRANSFER_WRITE_BIT,           // VkAccessFlags dstAccessMask;
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,   // VkImageLayout oldLayout;
        m_params.dst.image.operationLayout,     // VkImageLayout newLayout;
        VK_QUEUE_FAMILY_IGNORED,                // uint32_t srcQueueFamilyIndex;
        VK_QUEUE_FAMILY_IGNORED,                // uint32_t dstQueueFamilyIndex;
        m_multisampledCopyNoCabImage.get(),     // VkImage image;
        subresourceRange                        // VkImageSubresourceRange subresourceRange;
    };

    // destination image
    const VkImageMemoryBarrier multisampledCopyImagePostBarrier = {
        VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, // VkStructureType sType;
        DE_NULL,                                // const void* pNext;
        VK_ACCESS_TRANSFER_WRITE_BIT,           // VkAccessFlags srcAccessMask;
        VK_ACCESS_MEMORY_READ_BIT,              // VkAccessFlags dstAccessMask;
        m_params.dst.image.operationLayout,     // VkImageLayout oldLayout;
        m_params.src.image.operationLayout,     // VkImageLayout newLayout;
        VK_QUEUE_FAMILY_IGNORED,                // uint32_t srcQueueFamilyIndex;
        VK_QUEUE_FAMILY_IGNORED,                // uint32_t dstQueueFamilyIndex;
        m_multisampledCopyImage.get(),          // VkImage image;
        subresourceRange                        // VkImageSubresourceRange subresourceRange;
    };

    // destination image (no USAGE_COLOR_ATTACHMENT_BIT)
    const VkImageMemoryBarrier betweenCopyImageBarrier = {
        VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, // VkStructureType sType;
        DE_NULL,                                // const void* pNext;
        VK_ACCESS_TRANSFER_WRITE_BIT,           // VkAccessFlags srcAccessMask;
        VK_ACCESS_TRANSFER_READ_BIT,            // VkAccessFlags dstAccessMask;
        m_params.dst.image.operationLayout,     // VkImageLayout oldLayout;
        m_params.src.image.operationLayout,     // VkImageLayout newLayout;
        VK_QUEUE_FAMILY_IGNORED,                // uint32_t srcQueueFamilyIndex;
        VK_QUEUE_FAMILY_IGNORED,                // uint32_t dstQueueFamilyIndex;
        m_multisampledCopyNoCabImage.get(),     // VkImage image;
        subresourceRange                        // VkImageSubresourceRange subresourceRange;
    };

    uint32_t familyIndex                        = activeQueueFamilyIndex();
    VkQueue queue                               = VK_NULL_HANDLE;
    VkCommandBuffer commandBuffer               = VK_NULL_HANDLE;
    VkCommandPool commandPool                   = VK_NULL_HANDLE;
    std::tie(queue, commandBuffer, commandPool) = activeExecutionCtx();

    // Queue family ownership transfer. Move ownership of the m_multisampledImage and m_multisampledImageCopy to the compute/transfer queue.
    if (m_params.queueSelection != QueueSelectionOptions::Universal)
    {
        // Release ownership from graphics queue.
        {
            std::vector<VkImageMemoryBarrier> barriers;
            barriers.reserve(2);

            // Barrier for m_multisampledImage
            VkImageMemoryBarrier releaseBarrier = m_multisampledImageBarrier;
            releaseBarrier.dstAccessMask        = 0u; // dstAccessMask is ignored in ownership release operation.
            releaseBarrier.srcQueueFamilyIndex  = m_context.getUniversalQueueFamilyIndex();
            releaseBarrier.dstQueueFamilyIndex  = familyIndex;
            barriers.push_back(releaseBarrier);

            // Barrier for m_multisampledCopyImage
            releaseBarrier                     = m_multisampledCopyImageBarrier;
            releaseBarrier.dstAccessMask       = 0u; // dstAccessMask is ignored in ownership release operation.
            releaseBarrier.srcQueueFamilyIndex = m_context.getUniversalQueueFamilyIndex();
            releaseBarrier.dstQueueFamilyIndex = familyIndex;
            barriers.push_back(releaseBarrier);

            beginCommandBuffer(vk, *m_universalCmdBuffer);
            vk.cmdPipelineBarrier(*m_universalCmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                  VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, (VkDependencyFlags)0, 0,
                                  (const VkMemoryBarrier *)DE_NULL, 0, (const VkBufferMemoryBarrier *)DE_NULL,
                                  (uint32_t)barriers.size(), barriers.data());
            endCommandBuffer(vk, *m_universalCmdBuffer);
            submitCommandsAndWait(vk, vkDevice, m_universalQueue, *m_universalCmdBuffer);
            m_context.resetCommandPoolForVKSC(vkDevice, *m_universalCmdPool);
        }

        // Acquire ownership to compute / transfer queue.
        {
            std::vector<VkImageMemoryBarrier> barriers;
            barriers.reserve(2);

            // Barrier for m_multisampledImage
            VkImageMemoryBarrier acquireBarrier = m_multisampledImageBarrier;
            acquireBarrier.srcAccessMask        = 0u; // srcAccessMask is ignored in ownership acquire operation.
            acquireBarrier.srcQueueFamilyIndex  = m_context.getUniversalQueueFamilyIndex();
            acquireBarrier.dstQueueFamilyIndex  = familyIndex;
            barriers.push_back(acquireBarrier);

            // Barrier for m_multisampledImage
            acquireBarrier                     = m_multisampledCopyImageBarrier;
            acquireBarrier.srcAccessMask       = 0u; // srcAccessMask is ignored in ownership acquire operation.
            acquireBarrier.srcQueueFamilyIndex = m_context.getUniversalQueueFamilyIndex();
            acquireBarrier.dstQueueFamilyIndex = familyIndex;
            barriers.push_back(acquireBarrier);

            beginCommandBuffer(vk, commandBuffer);
            vk.cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                  (VkDependencyFlags)0, 0, (const VkMemoryBarrier *)DE_NULL, 0,
                                  (const VkBufferMemoryBarrier *)DE_NULL, (uint32_t)barriers.size(), barriers.data());
            endCommandBuffer(vk, commandBuffer);
            submitCommandsAndWait(vk, vkDevice, queue, commandBuffer);
            m_context.resetCommandPoolForVKSC(vkDevice, commandPool);
        }

        beginCommandBuffer(vk, commandBuffer);
    }
    else
    {
        // Universal queue

        std::vector<VkImageMemoryBarrier> imageBarriers;

        imageBarriers.push_back(m_multisampledImageBarrier);
        // Only use one barrier if no options have been given.
        if (m_options != NO_OPTIONAL_OPERATION)
        {
            imageBarriers.push_back(m_multisampledCopyImageBarrier);
            // Add the third barrier if option is as below.
            if (m_options == COPY_MS_IMAGE_TO_MS_IMAGE_NO_CAB)
                imageBarriers.push_back(m_multisampledCopyNoCabImageBarrier);
        }

        beginCommandBuffer(vk, commandBuffer);
        vk.cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                              VK_PIPELINE_STAGE_TRANSFER_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier *)DE_NULL,
                              0, (const VkBufferMemoryBarrier *)DE_NULL, (uint32_t)imageBarriers.size(),
                              imageBarriers.data());
    }

    if (!(m_params.extensionFlags & COPY_COMMANDS_2))
    {
        if (m_options == COPY_MS_IMAGE_TO_MS_IMAGE_NO_CAB)
        {
            vk.cmdCopyImage(commandBuffer, m_multisampledImage.get(), m_params.src.image.operationLayout,
                            m_multisampledCopyNoCabImage.get(), m_params.dst.image.operationLayout,
                            (uint32_t)imageCopies.size(), imageCopies.data());
            vk.cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                  (VkDependencyFlags)0, 0, (const VkMemoryBarrier *)DE_NULL, 0,
                                  (const VkBufferMemoryBarrier *)DE_NULL, 1u, &betweenCopyImageBarrier);
            vk.cmdCopyImage(commandBuffer, m_multisampledCopyNoCabImage.get(), m_params.src.image.operationLayout,
                            m_multisampledCopyImage.get(), m_params.dst.image.operationLayout,
                            (uint32_t)imageCopies.size(), imageCopies.data());
        }
        else
        {
            vk.cmdCopyImage(commandBuffer, m_multisampledImage.get(), m_params.src.image.operationLayout,
                            m_multisampledCopyImage.get(), m_params.dst.image.operationLayout,
                            (uint32_t)imageCopies.size(), imageCopies.data());
        }
    }
    else
    {
        if (m_options == COPY_MS_IMAGE_TO_MS_IMAGE_NO_CAB)
        {
            DE_ASSERT(m_params.extensionFlags & COPY_COMMANDS_2);
            const VkCopyImageInfo2KHR copyImageInfo2KHR = {
                VK_STRUCTURE_TYPE_COPY_IMAGE_INFO_2_KHR, // VkStructureType sType;
                DE_NULL,                                 // const void* pNext;
                m_multisampledImage.get(),               // VkImage srcImage;
                m_params.src.image.operationLayout,      // VkImageLayout srcImageLayout;
                m_multisampledCopyNoCabImage.get(),      // VkImage dstImage;
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,    // VkImageLayout dstImageLayout;
                (uint32_t)imageCopies2KHR.size(),        // uint32_t regionCount;
                imageCopies2KHR.data()                   // const VkImageCopy2KHR* pRegions;
            };
            const VkCopyImageInfo2KHR copyImageInfo2KHRCopy = {
                VK_STRUCTURE_TYPE_COPY_IMAGE_INFO_2_KHR,  // VkStructureType sType;
                DE_NULL,                                  // const void* pNext;
                m_multisampledCopyNoCabImage.get(),       // VkImage srcImage;
                vk::VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, // VkImageLayout srcImageLayout;
                m_multisampledCopyImage.get(),            // VkImage dstImage;
                m_params.dst.image.operationLayout,       // VkImageLayout dstImageLayout;
                (uint32_t)imageCopies2KHR.size(),         // uint32_t regionCount;
                imageCopies2KHR.data()                    // const VkImageCopy2KHR* pRegions;
            };

            vk.cmdCopyImage2(commandBuffer, &copyImageInfo2KHR);
            vk.cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                  (VkDependencyFlags)0, 0, (const VkMemoryBarrier *)DE_NULL, 0,
                                  (const VkBufferMemoryBarrier *)DE_NULL, 1u, &betweenCopyImageBarrier);
            vk.cmdCopyImage2(commandBuffer, &copyImageInfo2KHRCopy);
        }
        else
        {
            DE_ASSERT(m_params.extensionFlags & COPY_COMMANDS_2);
            const VkCopyImageInfo2KHR copyImageInfo2KHR = {
                VK_STRUCTURE_TYPE_COPY_IMAGE_INFO_2_KHR, // VkStructureType sType;
                DE_NULL,                                 // const void* pNext;
                m_multisampledImage.get(),               // VkImage srcImage;
                m_params.src.image.operationLayout,      // VkImageLayout srcImageLayout;
                m_multisampledCopyImage.get(),           // VkImage dstImage;
                m_params.dst.image.operationLayout,      // VkImageLayout dstImageLayout;
                (uint32_t)imageCopies2KHR.size(),        // uint32_t regionCount;
                imageCopies2KHR.data()                   // const VkImageCopy2KHR* pRegions;
            };
            vk.cmdCopyImage2(commandBuffer, &copyImageInfo2KHR);
        }
    }

    if (m_params.queueSelection != QueueSelectionOptions::Universal)
    {
        endCommandBuffer(vk, commandBuffer);
        submitCommandsAndWait(vk, vkDevice, queue, commandBuffer);
        m_context.resetCommandPoolForVKSC(vkDevice, commandPool);

        VkImageMemoryBarrier srcImageBarrier = makeImageMemoryBarrier(
            0u, 0u, m_params.src.image.operationLayout, m_params.src.image.operationLayout, m_multisampledImage.get(),
            m_multisampledImageBarrier.subresourceRange, familyIndex, m_context.getUniversalQueueFamilyIndex());
        // Release ownership from compute / transfer queue.
        {
            std::vector<VkImageMemoryBarrier> barriers;
            barriers.reserve(2);

            VkImageMemoryBarrier releaseBarrier = multisampledCopyImagePostBarrier;
            releaseBarrier.dstAccessMask        = 0u; // dstAccessMask is ignored in ownership release operation.
            releaseBarrier.srcQueueFamilyIndex  = familyIndex;
            releaseBarrier.dstQueueFamilyIndex  = m_context.getUniversalQueueFamilyIndex();
            barriers.push_back(releaseBarrier);

            releaseBarrier               = srcImageBarrier;
            releaseBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            releaseBarrier.dstAccessMask = 0u; // dstAccessMask is ignored in ownership release operation.
            barriers.push_back(releaseBarrier);

            beginCommandBuffer(vk, commandBuffer);
            vk.cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                                  (VkDependencyFlags)0, 0, (const VkMemoryBarrier *)DE_NULL, 0,
                                  (const VkBufferMemoryBarrier *)DE_NULL, (uint32_t)barriers.size(), barriers.data());
            endCommandBuffer(vk, commandBuffer);
            submitCommandsAndWait(vk, vkDevice, queue, commandBuffer);
            m_context.resetCommandPoolForVKSC(vkDevice, commandPool);
        }

        // Move ownership back to graphics queue.
        {
            std::vector<VkImageMemoryBarrier> barriers;
            barriers.reserve(2);

            VkImageMemoryBarrier acquireBarrier = multisampledCopyImagePostBarrier;
            acquireBarrier.srcAccessMask        = 0u; // srcAccessMask is ignored in ownership acquire operation.
            acquireBarrier.srcQueueFamilyIndex  = familyIndex;
            acquireBarrier.dstQueueFamilyIndex  = m_context.getUniversalQueueFamilyIndex();
            barriers.push_back(acquireBarrier);

            acquireBarrier               = srcImageBarrier;
            acquireBarrier.srcAccessMask = 0u; // srcAccessMask is ignored in ownership acquire operation.
            acquireBarrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
            barriers.push_back(acquireBarrier);

            beginCommandBuffer(vk, *m_universalCmdBuffer);
            vk.cmdPipelineBarrier(*m_universalCmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                  VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, (VkDependencyFlags)0, 0,
                                  (const VkMemoryBarrier *)DE_NULL, 0, (const VkBufferMemoryBarrier *)DE_NULL,
                                  (uint32_t)barriers.size(), barriers.data());
            endCommandBuffer(vk, *m_universalCmdBuffer);
            submitCommandsAndWait(vk, vkDevice, m_universalQueue, *m_universalCmdBuffer);
            m_context.resetCommandPoolForVKSC(vkDevice, *m_universalCmdPool);
        }
    }
    else
    {
        vk.cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                              (VkDependencyFlags)0, 0, (const VkMemoryBarrier *)DE_NULL, 0,
                              (const VkBufferMemoryBarrier *)DE_NULL, 1u, &multisampledCopyImagePostBarrier);
        endCommandBuffer(vk, commandBuffer);
        submitCommandsAndWait(vk, vkDevice, queue, commandBuffer);
        m_context.resetCommandPoolForVKSC(vkDevice, commandPool);
    }
}

class ResolveImageToImageTestCase : public vkt::TestCase
{
public:
    ResolveImageToImageTestCase(tcu::TestContext &testCtx, const std::string &name, const TestParams params,
                                const ResolveImageToImageOptions options = NO_OPTIONAL_OPERATION)
        : vkt::TestCase(testCtx, name)
        , m_params(params)
        , m_options(options)
    {
    }

    virtual void initPrograms(SourceCollections &programCollection) const;

    virtual TestInstance *createInstance(Context &context) const
    {
        return new ResolveImageToImage(context, m_params, m_options);
    }

    virtual void checkSupport(Context &context) const
    {
        const VkSampleCountFlagBits rasterizationSamples = m_params.samples;

        // Intermediate result check uses fragmentStoresAndAtomics.
        if (ResolveImageToImage::shouldVerifyIntermediateResults(m_options) &&
            !context.getDeviceFeatures().fragmentStoresAndAtomics)
        {
            TCU_THROW(NotSupportedError, "fragmentStoresAndAtomics not supported");
        }

        if (!(context.getDeviceProperties().limits.framebufferColorSampleCounts & rasterizationSamples))
            throw tcu::NotSupportedError("Unsupported number of rasterization samples");

        VkImageFormatProperties properties;
        if ((context.getInstanceInterface().getPhysicalDeviceImageFormatProperties(
                 context.getPhysicalDevice(), m_params.src.image.format, m_params.src.image.imageType,
                 VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_SRC_BIT, 0,
                 &properties) == VK_ERROR_FORMAT_NOT_SUPPORTED) ||
            (context.getInstanceInterface().getPhysicalDeviceImageFormatProperties(
                 context.getPhysicalDevice(), m_params.dst.image.format, m_params.dst.image.imageType,
                 VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT, 0,
                 &properties) == VK_ERROR_FORMAT_NOT_SUPPORTED))
        {
            TCU_THROW(NotSupportedError, "Format not supported");
        }

        checkExtensionSupport(context, m_params.extensionFlags);

        // Find at least one queue family that supports compute queue but does NOT support graphics queue.
        if (m_options == COPY_MS_IMAGE_TO_MS_IMAGE_COMPUTE)
        {
            if (context.getComputeQueueFamilyIndex() == -1)
                TCU_THROW(NotSupportedError, "No queue family found that only supports compute queue.");
        }

        // Find at least one queue family that supports transfer queue but does NOT support graphics and compute queue.
        if (m_options == COPY_MS_IMAGE_TO_MS_IMAGE_TRANSFER)
        {
            if (context.getTransferQueueFamilyIndex() == -1)
                TCU_THROW(NotSupportedError, "No queue family found that only supports transfer queue.");
        }
    }

private:
    TestParams m_params;
    const ResolveImageToImageOptions m_options;
};

void ResolveImageToImageTestCase::initPrograms(SourceCollections &programCollection) const
{
    programCollection.glslSources.add("vert") << glu::VertexSource("#version 310 es\n"
                                                                   "layout (location = 0) in highp vec4 a_position;\n"
                                                                   "void main()\n"
                                                                   "{\n"
                                                                   "    gl_Position = a_position;\n"
                                                                   "}\n");

    programCollection.glslSources.add("frag") << glu::FragmentSource("#version 310 es\n"
                                                                     "layout (location = 0) out highp vec4 o_color;\n"
                                                                     "void main()\n"
                                                                     "{\n"
                                                                     "    o_color = vec4(0.0, 1.0, 0.0, 1.0);\n"
                                                                     "}\n");

    if (m_options == COPY_MS_IMAGE_TO_MS_IMAGE || m_options == COPY_MS_IMAGE_TO_ARRAY_MS_IMAGE ||
        m_options == COPY_MS_IMAGE_LAYER_TO_MS_IMAGE || m_options == COPY_MS_IMAGE_TO_MS_IMAGE_MULTIREGION ||
        m_options == COPY_MS_IMAGE_TO_MS_IMAGE_COMPUTE || m_options == COPY_MS_IMAGE_TO_MS_IMAGE_TRANSFER)
    {
        // The shader verifies all layers in the copied image are the same as the source image.
        // This needs an image view per layer in the copied image.
        // Set 0 contains the output buffer.
        // Set 1 contains the input attachments.

        std::ostringstream verificationShader;

        verificationShader
            << "#version 450\n"
            << "\n"
            << "layout (push_constant, std430) uniform PushConstants {\n"
            << "    int width;\n"
            << "    int height;\n"
            << "    int samples;\n"
            << "};\n"
            << "layout (set=0, binding=0) buffer VerificationResults {\n"
            << "    int verificationFlags[];\n"
            << "};\n"
            << "layout (input_attachment_index=0, set=1, binding=0) uniform subpassInputMS attachment0;\n";

        const auto dstLayers = getArraySize(m_params.dst.image);

        if (m_options == COPY_MS_IMAGE_LAYER_TO_MS_IMAGE)
        {
            verificationShader
                << "layout (input_attachment_index=1, set=1, binding=1) uniform subpassInputMS attachment1;\n";
        }
        else
        {
            for (uint32_t layerNdx = 0u; layerNdx < dstLayers; ++layerNdx)
            {
                const auto i = layerNdx + 1u;
                verificationShader << "layout (input_attachment_index=" << i << ", set=1, binding=" << i
                                   << ") uniform subpassInputMS attachment" << i << ";\n";
            }
        }

        // Using a loop to iterate over each sample avoids the need for the sampleRateShading feature. The pipeline needs to be
        // created with a single sample.
        verificationShader << "\n"
                           << "void main() {\n"
                           << "    for (int sampleID = 0; sampleID < samples; ++sampleID) {\n"
                           << "        vec4 orig = subpassLoad(attachment0, sampleID);\n";

        std::ostringstream testCondition;
        if (m_options == COPY_MS_IMAGE_LAYER_TO_MS_IMAGE)
        {
            verificationShader << "        vec4 copy = subpassLoad(attachment1, sampleID);\n";
            testCondition << "orig == copy";
        }
        else
        {
            for (uint32_t layerNdx = 0u; layerNdx < dstLayers; ++layerNdx)
            {
                const auto i = layerNdx + 1u;
                verificationShader << "        vec4 copy" << i << " = subpassLoad(attachment" << i << ", sampleID);\n";
            }

            for (uint32_t layerNdx = 0u; layerNdx < dstLayers; ++layerNdx)
            {
                const auto i = layerNdx + 1u;
                testCondition << (layerNdx == 0u ? "" : " && ") << "orig == copy" << i;
            }
        }

        verificationShader << "\n"
                           << "        ivec3 coords  = ivec3(int(gl_FragCoord.x), int(gl_FragCoord.y), sampleID);\n"
                           << "        int bufferPos = (coords.y * width + coords.x) * samples + coords.z;\n"
                           << "\n"
                           << "        verificationFlags[bufferPos] = ((" << testCondition.str() << ") ? 1 : 0); \n"
                           << "    }\n"
                           << "}\n";

        programCollection.glslSources.add("verify") << glu::FragmentSource(verificationShader.str());
    }
}

class DepthStencilMSAA : public vkt::TestInstance
{
public:
    enum CopyOptions
    {
        COPY_WHOLE_IMAGE,
        COPY_ARRAY_TO_ARRAY,
        COPY_PARTIAL
    };

    struct TestParameters
    {
        AllocationKind allocationKind;
        uint32_t extensionFlags;
        CopyOptions copyOptions;
        VkSampleCountFlagBits samples;
        VkImageLayout srcImageLayout;
        VkImageLayout dstImageLayout;
        VkFormat imageFormat;
        VkImageAspectFlags copyAspect;
        bool imageOffset;
    };

    DepthStencilMSAA(Context &context, TestParameters testParameters);
    tcu::TestStatus iterate(void) override;

protected:
    tcu::TestStatus checkCopyResults(VkCommandBuffer cmdBuffer, const VkImageAspectFlagBits &aspectToVerify,
                                     VkImage srcImage, VkImage dstImage);

private:
    // Returns image aspects used in the copy regions.
    VkImageAspectFlags getUsedImageAspects()
    {
        auto aspectFlags = (VkImageAspectFlags)0;
        for (const auto &region : m_regions)
        {
            aspectFlags |= region.imageCopy.srcSubresource.aspectMask;
        }
        return aspectFlags;
    }

    ImageParms m_srcImage;
    ImageParms m_dstImage;
    std::vector<CopyRegion> m_regions;
    const TestParameters m_params;
    const float m_clearValue = 0.0f;
};

DepthStencilMSAA::DepthStencilMSAA(Context &context, TestParameters testParameters)
    : vkt::TestInstance(context)
    , m_params(testParameters)
{
    // params.src.image is the parameters used to create the copy source image
    m_srcImage.imageType       = VK_IMAGE_TYPE_2D;
    m_srcImage.format          = testParameters.imageFormat;
    m_srcImage.extent          = defaultExtent;
    m_srcImage.tiling          = VK_IMAGE_TILING_OPTIMAL;
    m_srcImage.operationLayout = testParameters.srcImageLayout;
    m_srcImage.createFlags     = 0u;

    // params.src.image is the parameters used to create the copy destination image
    m_dstImage.imageType       = VK_IMAGE_TYPE_2D;
    m_dstImage.format          = testParameters.imageFormat;
    m_dstImage.extent          = defaultExtent;
    m_dstImage.tiling          = VK_IMAGE_TILING_OPTIMAL;
    m_dstImage.operationLayout = testParameters.dstImageLayout;
    m_dstImage.createFlags     = 0u;

    const VkImageSubresourceLayers depthSubresourceLayers = {
        VK_IMAGE_ASPECT_DEPTH_BIT, // VkImageAspectFlags aspectMask;
        0u,                        // uint32_t mipLevel;
        0u,                        // uint32_t baseArrayLayer;
        1u                         // uint32_t layerCount;
    };

    const VkImageSubresourceLayers stencilSubresourceLayers = {
        VK_IMAGE_ASPECT_STENCIL_BIT, // VkImageAspectFlags aspectMask;
        0u,                          // uint32_t mipLevel;
        0u,                          // uint32_t baseArrayLayer;
        1u                           // uint32_t layerCount;
    };

    VkImageCopy depthCopy = {
        depthSubresourceLayers, // VkImageSubresourceLayers srcSubresource;
        {0, 0, 0},              // VkOffset3D srcOffset;
        depthSubresourceLayers, // VkImageSubresourceLayers dstSubresource;
        {0, 0, 0},              // VkOffset3D dstOffset;
        defaultExtent,          // VkExtent3D extent;
    };

    VkImageCopy stencilCopy = {
        stencilSubresourceLayers, // VkImageSubresourceLayers srcSubresource;
        {0, 0, 0},                // VkOffset3D srcOffset;
        stencilSubresourceLayers, // VkImageSubresourceLayers dstSubresource;
        {0, 0, 0},                // VkOffset3D dstOffset;
        defaultExtent,            // VkExtent3D extent;
    };

    if (testParameters.copyOptions == DepthStencilMSAA::COPY_ARRAY_TO_ARRAY)
    {
        m_srcImage.extent.depth                   = 5u;
        depthCopy.srcSubresource.baseArrayLayer   = 2u;
        depthCopy.dstSubresource.baseArrayLayer   = 3u;
        stencilCopy.srcSubresource.baseArrayLayer = 2u;
        stencilCopy.dstSubresource.baseArrayLayer = 3u;
    }

    CopyRegion depthCopyRegion;
    CopyRegion stencilCopyRegion;
    depthCopyRegion.imageCopy   = depthCopy;
    stencilCopyRegion.imageCopy = stencilCopy;

    std::vector<CopyRegion> depthRegions;
    std::vector<CopyRegion> stencilRegions;

    if (testParameters.copyOptions == DepthStencilMSAA::COPY_PARTIAL)
    {
        if (testParameters.copyAspect & VK_IMAGE_ASPECT_DEPTH_BIT)
        {
            depthCopyRegion.imageCopy.extent = {defaultHalfSize, defaultHalfSize, 1};
            // Copy region from bottom right to bottom left
            depthCopyRegion.imageCopy.srcOffset = {defaultHalfSize, defaultHalfSize, 0};
            depthCopyRegion.imageCopy.dstOffset = {0, defaultHalfSize, 0};
            depthRegions.push_back(depthCopyRegion);
            // Copy region from top right to bottom right
            depthCopyRegion.imageCopy.srcOffset = {defaultHalfSize, 0, 0};
            depthCopyRegion.imageCopy.dstOffset = {defaultHalfSize, defaultHalfSize, 0};
            depthRegions.push_back(depthCopyRegion);
        }
        if (testParameters.copyAspect & VK_IMAGE_ASPECT_STENCIL_BIT)
        {
            stencilCopyRegion.imageCopy.extent = {defaultHalfSize, defaultHalfSize, 1};
            // Copy region from bottom right to bottom left
            stencilCopyRegion.imageCopy.srcOffset = {defaultHalfSize, defaultHalfSize, 0};
            stencilCopyRegion.imageCopy.dstOffset = {0, defaultHalfSize, 0};
            stencilRegions.push_back(stencilCopyRegion);
            // Copy region from top right to bottom right
            stencilCopyRegion.imageCopy.srcOffset = {defaultHalfSize, 0, 0};
            stencilCopyRegion.imageCopy.dstOffset = {defaultHalfSize, defaultHalfSize, 0};
            stencilRegions.push_back(stencilCopyRegion);
        }
    }
    else
    {
        // Copy the default region (full image)
        if (testParameters.copyAspect & VK_IMAGE_ASPECT_DEPTH_BIT)
        {
            depthRegions.push_back(depthCopyRegion);
        }
        if (testParameters.copyAspect & VK_IMAGE_ASPECT_STENCIL_BIT)
        {
            stencilRegions.push_back(stencilCopyRegion);
        }
    }

    if (testParameters.copyAspect & VK_IMAGE_ASPECT_DEPTH_BIT)
    {
        m_regions.insert(m_regions.end(), depthRegions.begin(), depthRegions.end());
    }

    if (testParameters.copyAspect & VK_IMAGE_ASPECT_STENCIL_BIT)
    {
        m_regions.insert(m_regions.end(), stencilRegions.begin(), stencilRegions.end());
    }
}

tcu::TestStatus DepthStencilMSAA::iterate(void)
{
    const DeviceInterface &vk           = m_context.getDeviceInterface();
    const InstanceInterface &vki        = m_context.getInstanceInterface();
    const VkDevice vkDevice             = m_context.getDevice();
    const VkPhysicalDevice vkPhysDevice = m_context.getPhysicalDevice();
    const VkQueue queue                 = m_context.getUniversalQueue();
    const uint32_t queueFamilyIndex     = m_context.getUniversalQueueFamilyIndex();
    Allocator &memAlloc                 = m_context.getDefaultAllocator();
    Move<VkCommandPool> cmdPool =
        createCommandPool(vk, vkDevice, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex);
    Move<VkCommandBuffer> cmdBuffer = allocateCommandBuffer(vk, vkDevice, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

    const tcu::TextureFormat srcTcuFormat = mapVkFormat(m_srcImage.format);
    const tcu::TextureFormat dstTcuFormat = mapVkFormat(m_dstImage.format);
    VkImageAspectFlags aspectFlags        = getUsedImageAspects();
    uint32_t sourceArraySize              = getArraySize(m_srcImage);

    Move<VkImage> srcImage;
    de::MovePtr<Allocation> srcImageAlloc;
    Move<VkImage> dstImage;
    de::MovePtr<Allocation> dstImageAlloc;

    // 1. Create the images and draw a triangle to the source image.
    {
        const VkComponentMapping componentMappingRGBA = {VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G,
                                                         VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A};
        Move<VkShaderModule> vertexShaderModule =
            createShaderModule(vk, vkDevice, m_context.getBinaryCollection().get("vert"), 0);
        Move<VkShaderModule> fragmentShaderModule =
            createShaderModule(vk, vkDevice, m_context.getBinaryCollection().get("frag"), 0);
        std::vector<tcu::Vec4> vertices;
        Move<VkBuffer> vertexBuffer;
        de::MovePtr<Allocation> vertexBufferAlloc;
        Move<VkPipelineLayout> pipelineLayout;
        Move<VkPipeline> graphicsPipeline;
        Move<VkRenderPass> renderPass;

        // Create multisampled depth/stencil image (srcImage) and the copy destination image (dstImage).
        {
            VkImageCreateInfo multiSampledImageParams = {
                VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,        // VkStructureType sType;
                DE_NULL,                                    // const void* pNext;
                getCreateFlags(m_srcImage),                 // VkImageCreateFlags flags;
                m_srcImage.imageType,                       // VkImageType imageType;
                m_srcImage.format,                          // VkFormat format;
                getExtent3D(m_srcImage),                    // VkExtent3D extent;
                1u,                                         // uint32_t mipLevels;
                getArraySize(m_srcImage),                   // uint32_t arrayLayers;
                m_params.samples,                           // VkSampleCountFlagBits samples;
                VK_IMAGE_TILING_OPTIMAL,                    // VkImageTiling tiling;
                VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT // VkImageUsageFlags usage;
                    | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL |
                    VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT,
                VK_SHARING_MODE_EXCLUSIVE, // VkSharingMode sharingMode;
                1u,                        // uint32_t queueFamilyIndexCount;
                &queueFamilyIndex,         // const uint32_t* pQueueFamilyIndices;
                VK_IMAGE_LAYOUT_UNDEFINED, // VkImageLayout initialLayout;
            };

            srcImage = createImage(vk, vkDevice, &multiSampledImageParams);

            VkMemoryRequirements req = getImageMemoryRequirements(vk, vkDevice, *srcImage);
            uint32_t offset          = m_params.imageOffset ? static_cast<uint32_t>(req.alignment) : 0;

            srcImageAlloc = allocateImage(vki, vk, vkPhysDevice, vkDevice, srcImage.get(), MemoryRequirement::Any,
                                          memAlloc, m_params.allocationKind, offset);
            VK_CHECK(vk.bindImageMemory(vkDevice, srcImage.get(), srcImageAlloc->getMemory(),
                                        srcImageAlloc->getOffset() + offset));

            dstImage      = createImage(vk, vkDevice, &multiSampledImageParams);
            dstImageAlloc = allocateImage(vki, vk, vkPhysDevice, vkDevice, dstImage.get(), MemoryRequirement::Any,
                                          memAlloc, m_params.allocationKind, 0u);
            VK_CHECK(
                vk.bindImageMemory(vkDevice, dstImage.get(), dstImageAlloc->getMemory(), dstImageAlloc->getOffset()));
        }

        // Create render pass.
        {
            const VkImageLayout initialLayout                   = m_params.copyOptions == COPY_ARRAY_TO_ARRAY ?
                                                                      VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL :
                                                                      VK_IMAGE_LAYOUT_UNDEFINED;
            const VkAttachmentDescription attachmentDescription = {
                0u,                                              // VkAttachmentDescriptionFlags        flags
                m_srcImage.format,                               // VkFormat                            format
                m_params.samples,                                // VkSampleCountFlagBits            samples
                VK_ATTACHMENT_LOAD_OP_CLEAR,                     // VkAttachmentLoadOp                loadOp
                VK_ATTACHMENT_STORE_OP_STORE,                    // VkAttachmentStoreOp                storeOp
                VK_ATTACHMENT_LOAD_OP_CLEAR,                     // VkAttachmentLoadOp                stencilLoadOp
                VK_ATTACHMENT_STORE_OP_STORE,                    // VkAttachmentStoreOp                stencilStoreOp
                initialLayout,                                   // VkImageLayout                    initialLayout
                VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL // VkImageLayout                    finalLayout
            };

            const VkAttachmentReference attachmentReference = {
                0u,                                              // uint32_t            attachment
                VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL // VkImageLayout    layout
            };

            const VkSubpassDescription subpassDescription = {
                0u,                              // VkSubpassDescriptionFlags    flags
                VK_PIPELINE_BIND_POINT_GRAPHICS, // VkPipelineBindPoint            pipelineBindPoint
                0u,                              // uint32_t                        inputAttachmentCount
                DE_NULL,                         // const VkAttachmentReference*    pInputAttachments
                0u,                              // uint32_t                        colorAttachmentCount
                DE_NULL,                         // const VkAttachmentReference*    pColorAttachments
                DE_NULL,                         // const VkAttachmentReference*    pResolveAttachments
                &attachmentReference,            // const VkAttachmentReference*    pDepthStencilAttachment
                0u,                              // uint32_t                        preserveAttachmentCount
                DE_NULL                          // const VkAttachmentReference*    pPreserveAttachments
            };

            const VkRenderPassCreateInfo renderPassParams = {
                VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO, // VkStructureType sType;
                DE_NULL,                                   // const void* pNext;
                0u,                                        // VkRenderPassCreateFlags flags;
                1u,                                        // uint32_t attachmentCount;
                &attachmentDescription,                    // const VkAttachmentDescription* pAttachments;
                1u,                                        // uint32_t subpassCount;
                &subpassDescription,                       // const VkSubpassDescription* pSubpasses;
                0u,                                        // uint32_t dependencyCount;
                DE_NULL                                    // const VkSubpassDependency* pDependencies;
            };

            renderPass = createRenderPass(vk, vkDevice, &renderPassParams);
        }

        // Create pipeline layout
        {
            const VkPipelineLayoutCreateInfo pipelineLayoutParams = {
                VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, // VkStructureType sType;
                DE_NULL,                                       // const void* pNext;
                0u,                                            // VkPipelineLayoutCreateFlags flags;
                0u,                                            // uint32_t setLayoutCount;
                DE_NULL,                                       // const VkDescriptorSetLayout* pSetLayouts;
                0u,                                            // uint32_t pushConstantRangeCount;
                DE_NULL                                        // const VkPushConstantRange* pPushConstantRanges;
            };

            pipelineLayout = createPipelineLayout(vk, vkDevice, &pipelineLayoutParams);
        }

        // Create upper half triangle.
        {
            // Add triangle.
            vertices.emplace_back(-1.0f, -1.0f, 0.0f, 1.0f);
            vertices.emplace_back(1.0f, -1.0f, 0.0f, 1.0f);
            vertices.emplace_back(1.0f, 1.0f, 0.0f, 1.0f);
        }

        // Create vertex buffer.
        {
            const VkDeviceSize vertexDataSize           = vertices.size() * sizeof(tcu::Vec4);
            const VkBufferCreateInfo vertexBufferParams = {
                VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, // VkStructureType sType;
                DE_NULL,                              // const void* pNext;
                0u,                                   // VkBufferCreateFlags flags;
                vertexDataSize,                       // VkDeviceSize size;
                VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,    // VkBufferUsageFlags usage;
                VK_SHARING_MODE_EXCLUSIVE,            // VkSharingMode sharingMode;
                1u,                                   // uint32_t queueFamilyIndexCount;
                &queueFamilyIndex                     // const uint32_t* pQueueFamilyIndices;
            };

            vertexBuffer      = createBuffer(vk, vkDevice, &vertexBufferParams);
            vertexBufferAlloc = allocateBuffer(vki, vk, vkPhysDevice, vkDevice, *vertexBuffer,
                                               MemoryRequirement::HostVisible, memAlloc, m_params.allocationKind);
            VK_CHECK(vk.bindBufferMemory(vkDevice, *vertexBuffer, vertexBufferAlloc->getMemory(),
                                         vertexBufferAlloc->getOffset()));

            // Load vertices into vertex buffer.
            deMemcpy(vertexBufferAlloc->getHostPtr(), vertices.data(), (size_t)vertexDataSize);
            flushAlloc(vk, vkDevice, *vertexBufferAlloc);
        }

        {
            Move<VkFramebuffer> framebuffer;
            Move<VkImageView> sourceAttachmentView;

            // Create depth/stencil attachment view.
            {
                const uint32_t arrayLayer = m_params.copyOptions == COPY_ARRAY_TO_ARRAY ? 2u : 0u;
                const VkImageViewCreateInfo depthStencilAttachmentViewParams = {
                    VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, // VkStructureType sType;
                    DE_NULL,                                  // const void* pNext;
                    0u,                                       // VkImageViewCreateFlags flags;
                    *srcImage,                                // VkImage image;
                    VK_IMAGE_VIEW_TYPE_2D,                    // VkImageViewType viewType;
                    m_srcImage.format,                        // VkFormat format;
                    componentMappingRGBA,                     // VkComponentMapping components;
                    {aspectFlags, 0u, 1u, arrayLayer, 1u}     // VkImageSubresourceRange subresourceRange;
                };
                sourceAttachmentView = createImageView(vk, vkDevice, &depthStencilAttachmentViewParams);
            }

            // Create framebuffer
            {
                const VkFramebufferCreateInfo framebufferParams = {
                    VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO, // VkStructureType sType;
                    DE_NULL,                                   // const void* pNext;
                    0u,                                        // VkFramebufferCreateFlags flags;
                    *renderPass,                               // VkRenderPass renderPass;
                    1u,                                        // uint32_t attachmentCount;
                    &sourceAttachmentView.get(),               // const VkImageView* pAttachments;
                    m_srcImage.extent.width,                   // uint32_t width;
                    m_srcImage.extent.height,                  // uint32_t height;
                    1u                                         // uint32_t layers;
                };

                framebuffer = createFramebuffer(vk, vkDevice, &framebufferParams);
            }

            // Create pipeline
            {
                const std::vector<VkViewport> viewports(1, makeViewport(m_srcImage.extent));
                const std::vector<VkRect2D> scissors(1, makeRect2D(m_srcImage.extent));

                const VkPipelineMultisampleStateCreateInfo multisampleStateParams = {
                    VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, // VkStructureType sType;
                    DE_NULL,                                                  // const void* pNext;
                    0u,               // VkPipelineMultisampleStateCreateFlags flags;
                    m_params.samples, // VkSampleCountFlagBits rasterizationSamples;
                    VK_FALSE,         // VkBool32 sampleShadingEnable;
                    0.0f,             // float minSampleShading;
                    DE_NULL,          // const VkSampleMask* pSampleMask;
                    VK_FALSE,         // VkBool32 alphaToCoverageEnable;
                    VK_FALSE          // VkBool32 alphaToOneEnable;
                };

                const VkStencilOpState stencilOpState = {
                    VK_STENCIL_OP_KEEP,    // VkStencilOp    failOp
                    VK_STENCIL_OP_REPLACE, // VkStencilOp    passOp
                    VK_STENCIL_OP_KEEP,    // VkStencilOp    depthFailOp
                    VK_COMPARE_OP_ALWAYS,  // VkCompareOp    compareOp
                    0,                     // uint32_t        compareMask
                    0xFF,                  // uint32_t        writeMask
                    0xFF                   // uint32_t        reference
                };

                const VkPipelineDepthStencilStateCreateInfo depthStencilStateCreateInfoDefault = {
                    VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO, // VkStructureType                            sType
                    DE_NULL, // const void*                                pNext
                    0u,      // VkPipelineDepthStencilStateCreateFlags    flags
                    aspectFlags & VK_IMAGE_ASPECT_DEPTH_BIT ?
                        VK_TRUE :
                        VK_FALSE, // VkBool32                                    depthTestEnable
                    aspectFlags & VK_IMAGE_ASPECT_DEPTH_BIT ?
                        VK_TRUE :
                        VK_FALSE,         // VkBool32                                    depthWriteEnable
                    VK_COMPARE_OP_ALWAYS, // VkCompareOp                                depthCompareOp
                    VK_FALSE,             // VkBool32                                    depthBoundsTestEnable
                    aspectFlags & VK_IMAGE_ASPECT_STENCIL_BIT ?
                        VK_TRUE :
                        VK_FALSE,   // VkBool32                                    stencilTestEnable
                    stencilOpState, // VkStencilOpState                            front
                    stencilOpState, // VkStencilOpState                            back
                    0.0f,           // float                                    minDepthBounds
                    1.0f,           // float                                    maxDepthBounds
                };

                graphicsPipeline = makeGraphicsPipeline(
                    vk,                  // const DeviceInterface&                            vk
                    vkDevice,            // const VkDevice                                    device
                    *pipelineLayout,     // const VkPipelineLayout                            pipelineLayout
                    *vertexShaderModule, // const VkShaderModule                                vertexShaderModule
                    VK_NULL_HANDLE, // const VkShaderModule                                tessellationControlModule
                    VK_NULL_HANDLE, // const VkShaderModule                                tessellationEvalModule
                    VK_NULL_HANDLE, // const VkShaderModule                                geometryShaderModule
                    *fragmentShaderModule, // const VkShaderModule                                fragmentShaderModule
                    *renderPass,           // const VkRenderPass                                renderPass
                    viewports,             // const std::vector<VkViewport>&                    viewports
                    scissors,              // const std::vector<VkRect2D>&                        scissors
                    VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, // const VkPrimitiveTopology                        topology
                    0u,                                  // const uint32_t                                    subpass
                    0u,      // const uint32_t                                    patchControlPoints
                    DE_NULL, // const VkPipelineVertexInputStateCreateInfo*        vertexInputStateCreateInfo
                    DE_NULL, // const VkPipelineRasterizationStateCreateInfo*    rasterizationStateCreateInfo
                    &multisampleStateParams, // const VkPipelineMultisampleStateCreateInfo*        multisampleStateCreateInfo
                    &depthStencilStateCreateInfoDefault); // const VkPipelineDepthStencilStateCreateInfo*        depthStencilStateCreateInfo
            }

            // Create command buffer
            {
                beginCommandBuffer(vk, *cmdBuffer, 0u);

                const VkClearValue srcImageClearValue = makeClearValueDepthStencil(0.1f, 0x10);

                // Change the layout of each layer of the depth / stencil image to VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL and clear the images.
                const VkClearValue copiedImageClearValue =
                    makeClearValueDepthStencil(m_clearValue, (uint32_t)m_clearValue);
                const auto subResourceRange = makeImageSubresourceRange( // VkImageSubresourceRange    subresourceRange
                    (getAspectFlags(m_srcImage.format)),                 // VkImageAspectFlags        aspectMask
                    0u,                                                  // uint32_t                    baseMipLevel
                    1u,                                                  // uint32_t                    levelCount
                    0u,                                                  // uint32_t                    baseArrayLayer
                    getArraySize(m_srcImage));

                const VkImageMemoryBarrier preClearBarrier =
                    makeImageMemoryBarrier(0u,                           // VkAccessFlags            srcAccessMask
                                           VK_ACCESS_TRANSFER_WRITE_BIT, // VkAccessFlags            dstAccessMask
                                           VK_IMAGE_LAYOUT_UNDEFINED,    // VkImageLayout            oldLayout
                                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, // VkImageLayout            newLayout
                                           srcImage.get(),                       // VkImage                    image
                                           subResourceRange); // VkImageSubresourceRange    subresourceRange
                std::vector<VkImageMemoryBarrier> preClearBarriers(2u, preClearBarrier);
                preClearBarriers[1].image = dstImage.get();
                vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                      (VkDependencyFlags)0, 0, (const VkMemoryBarrier *)DE_NULL, 0,
                                      (const VkBufferMemoryBarrier *)DE_NULL, 2, preClearBarriers.data());

                vk.cmdClearDepthStencilImage(
                    *cmdBuffer,                           // VkCommandBuffer                    commandBuffer
                    srcImage.get(),                       // VkImage                            image
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, // VkImageLayout                    imageLayout
                    &srcImageClearValue.depthStencil,     // const VkClearDepthStencilValue*    pDepthStencil
                    1u,                                   // uint32_t                            rangeCount
                    &subResourceRange);                   // const VkImageSubresourceRange*    pRanges

                vk.cmdClearDepthStencilImage(
                    *cmdBuffer,                           // VkCommandBuffer                    commandBuffer
                    dstImage.get(),                       // VkImage                            image
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, // VkImageLayout                    imageLayout
                    &copiedImageClearValue.depthStencil,  // const VkClearDepthStencilValue*    pDepthStencil
                    1u,                                   // uint32_t                            rangeCount
                    &subResourceRange);                   // const VkImageSubresourceRange*    pRanges

                // Post clear barrier
                const VkImageMemoryBarrier postClearBarrier = makeImageMemoryBarrier(
                    VK_ACCESS_TRANSFER_WRITE_BIT,                     // VkAccessFlags            srcAccessMask
                    VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,     // VkAccessFlags            dstAccessMask
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,             // VkImageLayout            oldLayout
                    VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, // VkImageLayout            newLayout
                    srcImage.get(),                                   // VkImage                    image
                    subResourceRange);                                // VkImageSubresourceRange    subresourceRange

                vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                      VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT, (VkDependencyFlags)0, 0,
                                      (const VkMemoryBarrier *)DE_NULL, 0, (const VkBufferMemoryBarrier *)DE_NULL, 1,
                                      &postClearBarrier);

                beginRenderPass(vk, *cmdBuffer, *renderPass, *framebuffer,
                                makeRect2D(0, 0, m_srcImage.extent.width, m_srcImage.extent.height), 1u,
                                &srcImageClearValue);

                const VkDeviceSize vertexBufferOffset = 0u;

                vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *graphicsPipeline);
                vk.cmdBindVertexBuffers(*cmdBuffer, 0, 1, &vertexBuffer.get(), &vertexBufferOffset);
                vk.cmdDraw(*cmdBuffer, (uint32_t)vertices.size(), 1, 0, 0);

                endRenderPass(vk, *cmdBuffer);
                endCommandBuffer(vk, *cmdBuffer);
            }

            submitCommandsAndWait(vk, vkDevice, queue, *cmdBuffer);
            m_context.resetCommandPoolForVKSC(vkDevice, *cmdPool);
        }
    }

    // 2. Record a command buffer that contains the copy operation(s).
    beginCommandBuffer(vk, *cmdBuffer);
    {
        // Change the image layouts and synchronize the memory access before copying
        {
            const VkImageMemoryBarrier imageBarriers[] = {
                // srcImage
                makeImageMemoryBarrier(
                    VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,     // VkAccessFlags            srcAccessMask
                    VK_ACCESS_TRANSFER_READ_BIT,                      // VkAccessFlags            dstAccessMask
                    VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, // VkImageLayout            oldLayout
                    m_srcImage.operationLayout,                       // VkImageLayout            newLayout
                    srcImage.get(),                                   // VkImage                    image
                    makeImageSubresourceRange(                        // VkImageSubresourceRange    subresourceRange
                        getAspectFlags(srcTcuFormat),                 // VkImageAspectFlags    aspectMask
                        0u,                                           // uint32_t                baseMipLevel
                        1u,                                           // uint32_t                levelCount
                        0u,                                           // uint32_t                baseArrayLayer
                        sourceArraySize                               // uint32_t                layerCount
                        )),
                // dstImage
                makeImageMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT,         // VkAccessFlags            srcAccessMask
                                       VK_ACCESS_TRANSFER_WRITE_BIT,         // VkAccessFlags            dstAccessMask
                                       VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, // VkImageLayout            oldLayout
                                       m_dstImage.operationLayout,           // VkImageLayout            newLayout
                                       dstImage.get(),                       // VkImage                    image
                                       makeImageSubresourceRange(        // VkImageSubresourceRange    subresourceRange
                                           getAspectFlags(dstTcuFormat), // VkImageAspectFlags    aspectMask
                                           0u,                           // uint32_t                baseMipLevel
                                           1u,                           // uint32_t                levelCount
                                           0u,                           // uint32_t                baseArrayLayer
                                           sourceArraySize               // uint32_t                layerCount
                                           )),
            };
            vk.cmdPipelineBarrier(
                *cmdBuffer, VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_PIPELINE_STAGE_TRANSFER_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier *)DE_NULL, 0,
                (const VkBufferMemoryBarrier *)DE_NULL, 2u, imageBarriers);
        }

        std::vector<VkImageCopy> imageCopies;
        std::vector<VkImageCopy2KHR> imageCopies2KHR;
        for (const auto &region : m_regions)
        {
            if (!(m_params.extensionFlags & COPY_COMMANDS_2))
            {
                imageCopies.push_back(region.imageCopy);
            }
            else
            {
                DE_ASSERT(m_params.extensionFlags & COPY_COMMANDS_2);
                imageCopies2KHR.push_back(convertvkImageCopyTovkImageCopy2KHR(region.imageCopy));
            }
        }

        if (!(m_params.extensionFlags & COPY_COMMANDS_2))
        {
            vk.cmdCopyImage(*cmdBuffer, srcImage.get(), m_srcImage.operationLayout, dstImage.get(),
                            m_dstImage.operationLayout, (uint32_t)imageCopies.size(), imageCopies.data());
        }
        else
        {
            DE_ASSERT(m_params.extensionFlags & COPY_COMMANDS_2);
            const VkCopyImageInfo2KHR copyImageInfo2KHR = {
                VK_STRUCTURE_TYPE_COPY_IMAGE_INFO_2_KHR, // VkStructureType sType;
                DE_NULL,                                 // const void* pNext;
                srcImage.get(),                          // VkImage srcImage;
                m_srcImage.operationLayout,              // VkImageLayout srcImageLayout;
                dstImage.get(),                          // VkImage dstImage;
                m_dstImage.operationLayout,              // VkImageLayout dstImageLayout;
                (uint32_t)imageCopies2KHR.size(),        // uint32_t regionCount;
                imageCopies2KHR.data()                   // const VkImageCopy2KHR* pRegions;
            };

            vk.cmdCopyImage2(*cmdBuffer, &copyImageInfo2KHR);
        }
    }
    endCommandBuffer(vk, *cmdBuffer);
    submitCommandsAndWait(vk, vkDevice, queue, *cmdBuffer);
    m_context.resetCommandPoolForVKSC(vkDevice, *cmdPool);

    // Verify that all samples have been copied properly from all aspects.
    const auto usedImageAspects = getUsedImageAspects();
    if (usedImageAspects & VK_IMAGE_ASPECT_DEPTH_BIT)
    {
        auto copyResult = checkCopyResults(cmdBuffer.get(), VK_IMAGE_ASPECT_DEPTH_BIT, srcImage.get(), dstImage.get());
        if (copyResult.getCode() != QP_TEST_RESULT_PASS)
            return copyResult;
    }
    if (usedImageAspects & VK_IMAGE_ASPECT_STENCIL_BIT)
    {
        auto copyResult =
            checkCopyResults(cmdBuffer.get(), VK_IMAGE_ASPECT_STENCIL_BIT, srcImage.get(), dstImage.get());
        if (copyResult.getCode() != QP_TEST_RESULT_PASS)
            return copyResult;
    }
    return tcu::TestStatus::pass("pass");
}

tcu::TestStatus DepthStencilMSAA::checkCopyResults(VkCommandBuffer cmdBuffer,
                                                   const VkImageAspectFlagBits &aspectToVerify, VkImage srcImage,
                                                   VkImage dstImage)
{
    DE_ASSERT((aspectToVerify & VK_IMAGE_ASPECT_DEPTH_BIT) || (aspectToVerify & VK_IMAGE_ASPECT_STENCIL_BIT));

    const auto &vkd                = m_context.getDeviceInterface();
    const auto device              = m_context.getDevice();
    const auto queue               = m_context.getUniversalQueue();
    auto &alloc                    = m_context.getDefaultAllocator();
    const auto layerCount          = getArraySize(m_srcImage);
    const auto numInputAttachments = layerCount + 1u; // +1 for the source image.
    const auto numOutputBuffers    = 2u;              // 1 for the reference and 1 for the copied values.
    const auto numSets             = 2u;              // 1 for the output buffers, 1 for the input attachments.
    const auto fbWidth             = m_srcImage.extent.width;
    const auto fbHeight            = m_srcImage.extent.height;
    const auto aspectFlags         = getUsedImageAspects();

    // Shader modules.
    const auto vertexModule = createShaderModule(vkd, device, m_context.getBinaryCollection().get("vert"), 0u);
    const auto verificationModule =
        createShaderModule(vkd, device,
                           m_context.getBinaryCollection().get(
                               aspectToVerify & VK_IMAGE_ASPECT_DEPTH_BIT ? "verify_depth" : "verify_stencil"),
                           0u);

    // Descriptor sets.
    DescriptorPoolBuilder poolBuilder;
    poolBuilder.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, numOutputBuffers);
    poolBuilder.addType(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, numInputAttachments);
    const auto descriptorPool =
        poolBuilder.build(vkd, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, numSets);

    DescriptorSetLayoutBuilder layoutBuilderBuffer;
    layoutBuilderBuffer.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT);
    layoutBuilderBuffer.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT);
    const auto outputBufferSetLayout = layoutBuilderBuffer.build(vkd, device);

    DescriptorSetLayoutBuilder layoutBuilderAttachments;
    for (uint32_t i = 0u; i < numInputAttachments; ++i)
        layoutBuilderAttachments.addSingleBinding(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, VK_SHADER_STAGE_FRAGMENT_BIT);
    const auto inputAttachmentsSetLayout = layoutBuilderAttachments.build(vkd, device);

    const auto descriptorSetBuffer = makeDescriptorSet(vkd, device, descriptorPool.get(), outputBufferSetLayout.get());
    const auto descriptorSetAttachments =
        makeDescriptorSet(vkd, device, descriptorPool.get(), inputAttachmentsSetLayout.get());

    // Array with raw descriptor sets.
    const std::array<VkDescriptorSet, numSets> descriptorSets = {{
        descriptorSetBuffer.get(),
        descriptorSetAttachments.get(),
    }};

    // Pipeline layout.
    const std::array<VkDescriptorSetLayout, numSets> setLayouts = {{
        outputBufferSetLayout.get(),
        inputAttachmentsSetLayout.get(),
    }};

    // Push constants.
    std::array<int, 3> pushConstantData = {{
        static_cast<int>(fbWidth),
        static_cast<int>(fbHeight),
        static_cast<int>(m_params.samples),
    }};

    const auto pushConstantSize =
        static_cast<uint32_t>(pushConstantData.size() * sizeof(decltype(pushConstantData)::value_type));

    const VkPushConstantRange pushConstantRange = {
        VK_SHADER_STAGE_FRAGMENT_BIT, // VkShaderStageFlags stageFlags;
        0u,                           // uint32_t offset;
        pushConstantSize,             // uint32_t size;
    };

    const VkPipelineLayoutCreateInfo pipelineLayoutInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, // VkStructureType sType;
        nullptr,                                       // const void* pNext;
        0u,                                            // VkPipelineLayoutCreateFlags flags;
        static_cast<uint32_t>(setLayouts.size()),      // uint32_t setLayoutCount;
        setLayouts.data(),                             // const VkDescriptorSetLayout* pSetLayouts;
        1u,                                            // uint32_t pushConstantRangeCount;
        &pushConstantRange,                            // const VkPushConstantRange* pPushConstantRanges;
    };

    const auto pipelineLayout = createPipelineLayout(vkd, device, &pipelineLayoutInfo);

    // Render pass.
    const VkAttachmentDescription commonAttachmentDescription = {
        0u,                                   // VkAttachmentDescriptionFlags flags;
        m_srcImage.format,                    // VkFormat format;
        m_params.samples,                     // VkSampleCountFlagBits samples;
        VK_ATTACHMENT_LOAD_OP_LOAD,           // VkAttachmentLoadOp loadOp;
        VK_ATTACHMENT_STORE_OP_STORE,         // VkAttachmentStoreOp storeOp;
        VK_ATTACHMENT_LOAD_OP_DONT_CARE,      // VkAttachmentLoadOp stencilLoadOp;
        VK_ATTACHMENT_STORE_OP_DONT_CARE,     // VkAttachmentStoreOp stencilStoreOp;
        m_dstImage.operationLayout,           // VkImageLayout initialLayout;
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, // VkImageLayout finalLayout;
    };

    std::vector<VkAttachmentDescription> attachmentDescriptions(numInputAttachments, commonAttachmentDescription);
    // Set the first attachment's (m_srcImage) initial layout to match the layout it was left after copying.
    attachmentDescriptions[0].initialLayout = m_srcImage.operationLayout;

    std::vector<VkAttachmentReference> inputAttachmentReferences;
    inputAttachmentReferences.reserve(numInputAttachments);
    for (uint32_t i = 0u; i < numInputAttachments; ++i)
    {
        const VkAttachmentReference reference = {i, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        inputAttachmentReferences.push_back(reference);
    }

    const VkSubpassDescription subpassDescription = {
        0u,                                                      // VkSubpassDescriptionFlags flags;
        VK_PIPELINE_BIND_POINT_GRAPHICS,                         // VkPipelineBindPoint pipelineBindPoint;
        static_cast<uint32_t>(inputAttachmentReferences.size()), // uint32_t inputAttachmentCount;
        inputAttachmentReferences.data(),                        // const VkAttachmentReference* pInputAttachments;
        0u,                                                      // uint32_t colorAttachmentCount;
        nullptr,                                                 // const VkAttachmentReference* pColorAttachments;
        nullptr,                                                 // const VkAttachmentReference* pResolveAttachments;
        nullptr, // const VkAttachmentReference* pDepthStencilAttachment;
        0u,      // uint32_t preserveAttachmentCount;
        nullptr, // const uint32_t* pPreserveAttachments;
    };

    const VkRenderPassCreateInfo renderPassInfo = {
        VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,            // VkStructureType sType;
        nullptr,                                              // const void* pNext;
        0u,                                                   // VkRenderPassCreateFlags flags;
        static_cast<uint32_t>(attachmentDescriptions.size()), // uint32_t attachmentCount;
        attachmentDescriptions.data(),                        // const VkAttachmentDescription* pAttachments;
        1u,                                                   // uint32_t subpassCount;
        &subpassDescription,                                  // const VkSubpassDescription* pSubpasses;
        0u,                                                   // uint32_t dependencyCount;
        nullptr,                                              // const VkSubpassDependency* pDependencies;
    };

    const auto renderPass = createRenderPass(vkd, device, &renderPassInfo);

    // Framebuffer.
    std::vector<Move<VkImageView>> imageViews;
    std::vector<VkImageView> imageViewsRaw;

    const uint32_t srcArrayLayer = m_params.copyOptions == COPY_ARRAY_TO_ARRAY ? 2u : 0u;
    imageViews.push_back(makeImageView(vkd, device, srcImage, VK_IMAGE_VIEW_TYPE_2D, m_srcImage.format,
                                       makeImageSubresourceRange(aspectFlags, 0u, 1u, srcArrayLayer, 1u)));
    for (uint32_t i = 0u; i < layerCount; ++i)
    {
        const auto subresourceRange = makeImageSubresourceRange(aspectFlags, 0u, 1u, i, 1u);
        imageViews.push_back(
            makeImageView(vkd, device, dstImage, VK_IMAGE_VIEW_TYPE_2D, m_srcImage.format, subresourceRange));
    }

    imageViewsRaw.reserve(imageViews.size());
    std::transform(begin(imageViews), end(imageViews), std::back_inserter(imageViewsRaw),
                   [](const Move<VkImageView> &ptr) { return ptr.get(); });

    const auto framebuffer = makeFramebuffer(vkd, device, renderPass.get(), static_cast<uint32_t>(imageViewsRaw.size()),
                                             imageViewsRaw.data(), fbWidth, fbHeight);

    // Create storage buffers for both original and copied multisampled depth/stencil images.
    const auto bufferCount = static_cast<size_t>(fbWidth * fbHeight * m_params.samples);
    const auto bufferSize  = static_cast<VkDeviceSize>(bufferCount * sizeof(float));
    BufferWithMemory bufferOriginal(vkd, device, alloc,
                                    makeBufferCreateInfo(bufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT),
                                    MemoryRequirement::HostVisible);
    BufferWithMemory bufferCopied(vkd, device, alloc,
                                  makeBufferCreateInfo(bufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT),
                                  MemoryRequirement::HostVisible);
    auto &bufferOriginalAlloc = bufferOriginal.getAllocation();
    auto &bufferCopiedAlloc   = bufferCopied.getAllocation();

    // Update descriptor sets.
    DescriptorSetUpdateBuilder updater;

    const auto bufferOriginalInfo = makeDescriptorBufferInfo(bufferOriginal.get(), 0ull, bufferSize);
    const auto bufferCopiedInfo   = makeDescriptorBufferInfo(bufferCopied.get(), 0ull, bufferSize);
    updater.writeSingle(descriptorSetBuffer.get(), DescriptorSetUpdateBuilder::Location::binding(0u),
                        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &bufferOriginalInfo);
    updater.writeSingle(descriptorSetBuffer.get(), DescriptorSetUpdateBuilder::Location::binding(1u),
                        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &bufferCopiedInfo);

    std::vector<VkDescriptorImageInfo> imageInfos;
    imageInfos.reserve(imageViewsRaw.size());
    for (size_t i = 0; i < imageViewsRaw.size(); ++i)
    {
        imageInfos.push_back(
            makeDescriptorImageInfo(VK_NULL_HANDLE, imageViewsRaw[i], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL));
        updater.writeSingle(descriptorSetAttachments.get(),
                            DescriptorSetUpdateBuilder::Location::binding(static_cast<uint32_t>(i)),
                            VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, &imageInfos[i]);
    }

    updater.update(vkd, device);

    // Vertex buffer.
    std::vector<tcu::Vec4> fullScreenQuad;
    {
        // Full screen quad so every framebuffer pixel and sample location is verified by the shader.
        const tcu::Vec4 topLeft(-1.0f, -1.0f, 0.0f, 1.0f);
        const tcu::Vec4 topRight(1.0f, -1.0f, 0.0f, 1.0f);
        const tcu::Vec4 bottomLeft(-1.0f, 1.0f, 0.0f, 1.0f);
        const tcu::Vec4 bottomRight(1.0f, 1.0f, 0.0f, 1.0f);

        fullScreenQuad.reserve(6u);
        fullScreenQuad.push_back(topLeft);
        fullScreenQuad.push_back(topRight);
        fullScreenQuad.push_back(bottomRight);
        fullScreenQuad.push_back(topLeft);
        fullScreenQuad.push_back(bottomRight);
        fullScreenQuad.push_back(bottomLeft);
    }

    const auto vertexBufferSize =
        static_cast<VkDeviceSize>(fullScreenQuad.size() * sizeof(decltype(fullScreenQuad)::value_type));
    const auto vertexBufferInfo = makeBufferCreateInfo(vertexBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    const BufferWithMemory vertexBuffer(vkd, device, alloc, vertexBufferInfo, MemoryRequirement::HostVisible);
    const VkDeviceSize vertexBufferOffset = 0ull;

    deMemcpy(vertexBuffer.getAllocation().getHostPtr(), fullScreenQuad.data(), static_cast<size_t>(vertexBufferSize));
    flushAlloc(vkd, device, vertexBuffer.getAllocation());

    // Graphics pipeline.
    const std::vector<VkViewport> viewports(1, makeViewport(m_srcImage.extent));
    const std::vector<VkRect2D> scissors(1, makeRect2D(m_srcImage.extent));

    const VkPipelineMultisampleStateCreateInfo multisampleStateParams = {
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, // VkStructureType sType;
        nullptr,                                                  // const void* pNext;
        0u,                                                       // VkPipelineMultisampleStateCreateFlags flags;
        VK_SAMPLE_COUNT_1_BIT,                                    // VkSampleCountFlagBits rasterizationSamples;
        VK_FALSE,                                                 // VkBool32 sampleShadingEnable;
        0.0f,                                                     // float minSampleShading;
        nullptr,                                                  // const VkSampleMask* pSampleMask;
        VK_FALSE,                                                 // VkBool32 alphaToCoverageEnable;
        VK_FALSE                                                  // VkBool32 alphaToOneEnable;
    };

    const auto graphicsPipeline = makeGraphicsPipeline(
        vkd,                      // const DeviceInterface&                            vk
        device,                   // const VkDevice                                    device
        pipelineLayout.get(),     // const VkPipelineLayout                            pipelineLayout
        vertexModule.get(),       // const VkShaderModule                                vertexShaderModule
        VK_NULL_HANDLE,           // const VkShaderModule                                tessellationControlModule
        VK_NULL_HANDLE,           // const VkShaderModule                                tessellationEvalModule
        VK_NULL_HANDLE,           // const VkShaderModule                                geometryShaderModule
        verificationModule.get(), // const VkShaderModule                                fragmentShaderModule
        renderPass.get(),         // const VkRenderPass                                renderPass
        viewports,                // const std::vector<VkViewport>&                    viewports
        scissors,                 // const std::vector<VkRect2D>&                        scissors
        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, // const VkPrimitiveTopology                        topology
        0u,                                  // const uint32_t                                    subpass
        0u,                                  // const uint32_t                                    patchControlPoints
        nullptr,                  // const VkPipelineVertexInputStateCreateInfo*        vertexInputStateCreateInfo
        nullptr,                  // const VkPipelineRasterizationStateCreateInfo*    rasterizationStateCreateInfo
        &multisampleStateParams); // const VkPipelineMultisampleStateCreateInfo*        multisampleStateCreateInfo

    // Make sure multisample copy data is available to the fragment shader.
    const auto imagesBarrier = makeMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_INPUT_ATTACHMENT_READ_BIT);

    // Record and submit command buffer.
    beginCommandBuffer(vkd, cmdBuffer);
    vkd.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0u, 1u,
                           &imagesBarrier, 0u, nullptr, 0u, nullptr);
    beginRenderPass(vkd, cmdBuffer, renderPass.get(), framebuffer.get(), makeRect2D(m_srcImage.extent));
    vkd.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline.get());
    vkd.cmdBindVertexBuffers(cmdBuffer, 0u, 1u, &vertexBuffer.get(), &vertexBufferOffset);

    vkd.cmdPushConstants(cmdBuffer, pipelineLayout.get(), VK_SHADER_STAGE_FRAGMENT_BIT, 0u, pushConstantSize,
                         pushConstantData.data());
    vkd.cmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout.get(), 0u,
                              static_cast<uint32_t>(descriptorSets.size()), descriptorSets.data(), 0u, nullptr);
    vkd.cmdDraw(cmdBuffer, static_cast<uint32_t>(fullScreenQuad.size()), 1u, 0u, 0u);

    endRenderPass(vkd, cmdBuffer);

    // Make sure verification buffer data is available on the host.
    const auto bufferBarrier = makeMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
    vkd.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0u, 1u,
                           &bufferBarrier, 0u, nullptr, 0u, nullptr);
    endCommandBuffer(vkd, cmdBuffer);
    submitCommandsAndWait(vkd, device, queue, cmdBuffer);

    // Verify intermediate results.
    invalidateAlloc(vkd, device, bufferOriginalAlloc);
    invalidateAlloc(vkd, device, bufferCopiedAlloc);
    std::vector<float> outputOriginal(bufferCount, 0);
    std::vector<float> outputCopied(bufferCount, 0);
    deMemcpy(outputOriginal.data(), bufferOriginalAlloc.getHostPtr(), static_cast<size_t>(bufferSize));
    deMemcpy(outputCopied.data(), bufferCopiedAlloc.getHostPtr(), static_cast<size_t>(bufferSize));

    auto &log = m_context.getTestContext().getLog();
    log << tcu::TestLog::Message << "Verifying intermediate multisample copy results" << tcu::TestLog::EndMessage;

    const auto sampleCount = static_cast<uint32_t>(m_params.samples);

    // Verify copied region(s)
    for (const auto &region : m_regions)
    {
        for (uint32_t x = 0u; x < region.imageCopy.extent.width; ++x)
            for (uint32_t y = 0u; y < region.imageCopy.extent.height; ++y)
                for (uint32_t s = 0u; s < sampleCount; ++s)
                {
                    tcu::UVec2 srcCoord(x + region.imageCopy.srcOffset.x, y + region.imageCopy.srcOffset.y);
                    tcu::UVec2 dstCoord(x + region.imageCopy.dstOffset.x, y + region.imageCopy.dstOffset.y);
                    const auto srcIndex = (srcCoord.y() * fbWidth + srcCoord.x()) * sampleCount + s;
                    const auto dstIndex = (dstCoord.y() * fbWidth + dstCoord.x()) * sampleCount + s;
                    if (outputOriginal[srcIndex] != outputCopied[dstIndex])
                    {
                        std::ostringstream msg;
                        msg << "Intermediate verification failed for coordinates (" << x << ", " << y << ") sample "
                            << s << ". "
                            << "result: " << outputCopied[dstIndex] << " expected: " << outputOriginal[srcIndex];
                        return tcu::TestStatus::fail(msg.str());
                    }
                }
    }

    if (m_params.copyOptions == COPY_PARTIAL)
    {
        // In the partial copy tests the destination image contains copied data only in the bottom half of the image.
        // Verify that the upper half of the image is left at it's clear value (0).
        for (uint32_t x = 0u; x < m_srcImage.extent.width; x++)
            for (uint32_t y = 0u; y < m_srcImage.extent.height / 2; y++)
                for (uint32_t s = 0u; s < sampleCount; ++s)
                {
                    const auto bufferIndex = (y * fbWidth + x) * sampleCount + s;
                    if (outputCopied[bufferIndex] != m_clearValue)
                    {
                        std::ostringstream msg;
                        msg << "Intermediate verification failed for coordinates (" << x << ", " << y << ") sample "
                            << s << ". "
                            << "result: " << outputCopied[bufferIndex] << " expected: 0.0";
                        return tcu::TestStatus::fail(msg.str());
                    }
                }
    }

    log << tcu::TestLog::Message << "Intermediate multisample copy verification passed" << tcu::TestLog::EndMessage;
    return tcu::TestStatus::pass("Pass");
}

class DepthStencilMSAATestCase : public vkt::TestCase
{
public:
    DepthStencilMSAATestCase(tcu::TestContext &testCtx, const std::string &name,
                             const DepthStencilMSAA::TestParameters testParams)
        : vkt::TestCase(testCtx, name)
        , m_params(testParams)
    {
    }

    virtual void initPrograms(SourceCollections &programCollection) const;

    virtual TestInstance *createInstance(Context &context) const
    {
        return new DepthStencilMSAA(context, m_params);
    }

    virtual void checkSupport(Context &context) const
    {
        checkExtensionSupport(context, m_params.extensionFlags);

        const VkSampleCountFlagBits rasterizationSamples = m_params.samples;

        if (!context.getDeviceFeatures().fragmentStoresAndAtomics)
            TCU_THROW(NotSupportedError, "fragmentStoresAndAtomics not supported");

        if ((m_params.copyAspect & VK_IMAGE_ASPECT_DEPTH_BIT) &&
            !(context.getDeviceProperties().limits.framebufferDepthSampleCounts & rasterizationSamples))
            TCU_THROW(NotSupportedError, "Unsupported number of depth samples");

        if ((m_params.copyAspect & VK_IMAGE_ASPECT_STENCIL_BIT) &&
            !(context.getDeviceProperties().limits.framebufferDepthSampleCounts & rasterizationSamples))
            TCU_THROW(NotSupportedError, "Unsupported number of stencil samples");

        VkImageUsageFlags usageFlags = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                                       VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;

        VkImageFormatProperties properties;
        if ((context.getInstanceInterface().getPhysicalDeviceImageFormatProperties(
                 context.getPhysicalDevice(), m_params.imageFormat, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL,
                 usageFlags, 0, &properties) == VK_ERROR_FORMAT_NOT_SUPPORTED))
        {
            TCU_THROW(NotSupportedError, "Format not supported");
        }
    }

private:
    uint32_t getArrayLayerCount() const
    {
        return (m_params.copyOptions == DepthStencilMSAA::COPY_ARRAY_TO_ARRAY) ? 5u : 1u;
    }

    void createVerificationShader(std::ostringstream &shaderCode, const VkImageAspectFlagBits attachmentAspect) const
    {
        DE_ASSERT(attachmentAspect & (VK_IMAGE_ASPECT_STENCIL_BIT | VK_IMAGE_ASPECT_DEPTH_BIT));
        // The shader copies the sample values from the source and destination image to output buffers OriginalValue and
        // CopiedValues, respectively. If the dst image contains multiple array layers, only one layer has the copied data
        // and the rest should be filled with the clear value (0). This is also verified in this shader.
        // Array layer cases need an image view per layer in the copied image.
        // Set 0 contains the output buffers.
        // Set 1 contains the input attachments.

        std::string inputAttachmentPrefix = attachmentAspect == VK_IMAGE_ASPECT_STENCIL_BIT ? "u" : "";

        shaderCode << "#version 450\n"
                   << "\n"
                   << "layout (push_constant, std430) uniform PushConstants {\n"
                   << "    int width;\n"
                   << "    int height;\n"
                   << "    int samples;\n"
                   << "};\n"
                   << "layout (set=0, binding=0) buffer OriginalValues {\n"
                   << "    float outputOriginal[];\n"
                   << "};\n"
                   << "layout (set=0, binding=1) buffer CopiedValues {\n"
                   << "    float outputCopied[];\n"
                   << "};\n"
                   << "layout (input_attachment_index=0, set=1, binding=0) uniform " << inputAttachmentPrefix
                   << "subpassInputMS attachment0;\n";

        const auto layerCount = getArrayLayerCount();
        for (uint32_t layerNdx = 0u; layerNdx < layerCount; ++layerNdx)
        {
            const auto i = layerNdx + 1u;
            shaderCode << "layout (input_attachment_index=" << i << ", set=1, binding=" << i << ") uniform "
                       << inputAttachmentPrefix << "subpassInputMS attachment" << i << ";\n";
        }

        // Using a loop to iterate over each sample avoids the need for the sampleRateShading feature. The pipeline needs to be
        // created with a single sample.
        shaderCode << "\n"
                   << "void main() {\n"
                   << "    for (int sampleID = 0; sampleID < samples; ++sampleID) {\n"
                   << "        ivec3 coords  = ivec3(int(gl_FragCoord.x), int(gl_FragCoord.y), sampleID);\n"
                   << "        int bufferPos = (coords.y * width + coords.x) * samples + coords.z;\n"
                   << "        " << inputAttachmentPrefix << "vec4 orig = subpassLoad(attachment0, sampleID);\n"
                   << "        outputOriginal[bufferPos] = orig.r;\n";

        for (uint32_t layerNdx = 0u; layerNdx < layerCount; ++layerNdx)
        {
            const auto i = layerNdx + 1u;
            shaderCode << "        " << inputAttachmentPrefix << "vec4 copy" << i << " = subpassLoad(attachment" << i
                       << ", sampleID);\n";
        }

        std::ostringstream testCondition;
        std::string layerToVerify = m_params.copyOptions == DepthStencilMSAA::COPY_ARRAY_TO_ARRAY ? "copy4" : "copy1";
        shaderCode << "\n"
                   << "        outputCopied[bufferPos] = " << layerToVerify << ".r; \n";

        if (m_params.copyOptions == DepthStencilMSAA::COPY_ARRAY_TO_ARRAY)
        {
            // In array layer copy tests the copied image should be in the layer 3 and other layers should be value of 0 or 0.0 depending on the format.
            // This verifies that all the samples in the other layers have proper values.
            shaderCode << "        bool equalEmptyLayers = ";
            for (uint32_t layerNdx = 0u; layerNdx < layerCount; ++layerNdx)
            {
                if (layerNdx == 3)
                    continue;
                const auto i = layerNdx + 1u;
                shaderCode << "copy" << i << ".r == " << (attachmentAspect == VK_IMAGE_ASPECT_STENCIL_BIT ? "0" : "0.0")
                           << (layerNdx < 4u ? " && " : ";\n");
            }
            shaderCode << "        if (!equalEmptyLayers)\n"
                       << "            outputCopied[bufferPos]--; \n";
        }

        shaderCode << "    }\n"
                   << "}\n";
    }

    DepthStencilMSAA::TestParameters m_params;
};

void DepthStencilMSAATestCase::initPrograms(SourceCollections &programCollection) const
{
    programCollection.glslSources.add("vert") << glu::VertexSource("#version 310 es\n"
                                                                   "layout (location = 0) in highp vec4 a_position;\n"
                                                                   "void main()\n"
                                                                   "{\n"
                                                                   "    gl_Position = vec4(a_position.xy, 1.0, 1.0);\n"
                                                                   "}\n");

    programCollection.glslSources.add("frag") << glu::FragmentSource("#version 310 es\n"
                                                                     "void main()\n"
                                                                     "{}\n");

    // Create the verifying shader for the depth aspect if the depth is used.
    if (m_params.copyAspect & VK_IMAGE_ASPECT_DEPTH_BIT)
    {
        std::ostringstream verificationShader;
        // All the depth formats are float types, so the input attachment prefix is not used.
        createVerificationShader(verificationShader, VK_IMAGE_ASPECT_DEPTH_BIT);
        programCollection.glslSources.add("verify_depth") << glu::FragmentSource(verificationShader.str());
    }

    // Create the verifying shader for the stencil aspect if the stencil is used.
    if (m_params.copyAspect & VK_IMAGE_ASPECT_STENCIL_BIT)
    {
        std::ostringstream verificationShader;
        // All the stencil formats are uint types, so the input attachment prefix is "u".
        createVerificationShader(verificationShader, VK_IMAGE_ASPECT_STENCIL_BIT);
        programCollection.glslSources.add("verify_stencil") << glu::FragmentSource(verificationShader.str());
    }
}

struct BufferOffsetParams
{
    static constexpr uint32_t kMaxOffset = 8u;

    uint32_t srcOffset;
    uint32_t dstOffset;
};

void checkZerosAt(const std::vector<uint8_t> &bufferData, uint32_t from, uint32_t count)
{
    constexpr uint8_t zero{0};
    for (uint32_t i = 0; i < count; ++i)
    {
        const auto &val = bufferData[from + i];
        if (val != zero)
        {
            std::ostringstream msg;
            msg << "Unexpected non-zero byte found at position " << (from + i) << ": " << static_cast<int>(val);
            TCU_FAIL(msg.str());
        }
    }
}

tcu::TestStatus bufferOffsetTest(Context &ctx, BufferOffsetParams params)
{
    // Try to copy blocks of sizes 1 to kMaxOffset. Each copy region will use a block of kMaxOffset*2 bytes to take into account srcOffset and dstOffset.
    constexpr auto kMaxOffset  = BufferOffsetParams::kMaxOffset;
    constexpr auto kBlockSize  = kMaxOffset * 2u;
    constexpr auto kBufferSize = kMaxOffset * kBlockSize;

    DE_ASSERT(params.srcOffset < kMaxOffset);
    DE_ASSERT(params.dstOffset < kMaxOffset);

    const auto &vkd   = ctx.getDeviceInterface();
    const auto device = ctx.getDevice();
    auto &alloc       = ctx.getDefaultAllocator();
    const auto qIndex = ctx.getUniversalQueueFamilyIndex();
    const auto queue  = ctx.getUniversalQueue();

    const auto srcBufferInfo = makeBufferCreateInfo(kBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    const auto dstBufferInfo = makeBufferCreateInfo(kBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT);

    BufferWithMemory srcBuffer(vkd, device, alloc, srcBufferInfo, MemoryRequirement::HostVisible);
    BufferWithMemory dstBuffer(vkd, device, alloc, dstBufferInfo, MemoryRequirement::HostVisible);
    auto &srcAlloc = srcBuffer.getAllocation();
    auto &dstAlloc = dstBuffer.getAllocation();

    // Zero-out destination buffer.
    deMemset(dstAlloc.getHostPtr(), 0, kBufferSize);
    flushAlloc(vkd, device, dstAlloc);

    // Fill source buffer with nonzero bytes.
    std::vector<uint8_t> srcData;
    srcData.reserve(kBufferSize);
    for (uint32_t i = 0; i < kBufferSize; ++i)
        srcData.push_back(static_cast<uint8_t>(100u + i));
    deMemcpy(srcAlloc.getHostPtr(), srcData.data(), de::dataSize(srcData));
    flushAlloc(vkd, device, srcAlloc);

    // Copy regions.
    std::vector<VkBufferCopy> copies;
    copies.reserve(kMaxOffset);
    for (uint32_t i = 0; i < kMaxOffset; ++i)
    {
        const auto blockStart = kBlockSize * i;
        const auto copySize   = i + 1u;
        const auto bufferCopy = makeBufferCopy(params.srcOffset + blockStart, params.dstOffset + blockStart, copySize);
        copies.push_back(bufferCopy);
    }

    const auto cmdPool      = makeCommandPool(vkd, device, qIndex);
    const auto cmdBufferPtr = allocateCommandBuffer(vkd, device, cmdPool.get(), VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    const auto cmdBuffer    = cmdBufferPtr.get();

    beginCommandBuffer(vkd, cmdBuffer);
    vkd.cmdCopyBuffer(cmdBuffer, srcBuffer.get(), dstBuffer.get(), static_cast<uint32_t>(copies.size()), copies.data());
    const auto barrier = makeMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
    vkd.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0u, 1u, &barrier, 0u,
                           nullptr, 0u, nullptr);
    endCommandBuffer(vkd, cmdBuffer);
    submitCommandsAndWait(vkd, device, queue, cmdBuffer);
    invalidateAlloc(vkd, device, dstAlloc);

    // Verify destination buffer data.
    std::vector<uint8_t> dstData(kBufferSize);
    deMemcpy(dstData.data(), dstAlloc.getHostPtr(), de::dataSize(dstData));

    for (uint32_t blockIdx = 0; blockIdx < kMaxOffset; ++blockIdx)
    {
        const auto blockStart = kBlockSize * blockIdx;
        const auto copySize   = blockIdx + 1u;

        // Verify no data has been written before dstOffset.
        checkZerosAt(dstData, blockStart, params.dstOffset);

        // Verify copied block.
        for (uint32_t i = 0; i < copySize; ++i)
        {
            const auto &dstVal = dstData[blockStart + params.dstOffset + i];
            const auto &srcVal = srcData[blockStart + params.srcOffset + i];
            if (dstVal != srcVal)
            {
                std::ostringstream msg;
                msg << "Unexpected value found at position " << (blockStart + params.dstOffset + i) << ": expected "
                    << static_cast<int>(srcVal) << " but found " << static_cast<int>(dstVal);
                TCU_FAIL(msg.str());
            }
        }

        // Verify no data has been written after copy block.
        checkZerosAt(dstData, blockStart + params.dstOffset + copySize, kBlockSize - (params.dstOffset + copySize));
    }

    return tcu::TestStatus::pass("Pass");
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

struct TestGroupParams
{
    AllocationKind allocationKind;
    uint32_t extensionFlags;
    QueueSelectionOptions queueSelection;
    bool useSecondaryCmdBuffer;
    bool useSparseBinding;
};

using TestGroupParamsPtr = de::SharedPtr<TestGroupParams>;

void addImageToImageSimpleTests(tcu::TestCaseGroup *group, TestGroupParamsPtr testGroupParams)
{
    tcu::TestContext &testCtx = group->getTestContext();

    {
        TestParams params;
        params.src.image.imageType       = VK_IMAGE_TYPE_2D;
        params.src.image.format          = VK_FORMAT_R8G8B8A8_UINT;
        params.src.image.extent          = defaultExtent;
        params.src.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
        params.src.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        params.dst.image.imageType       = VK_IMAGE_TYPE_2D;
        params.dst.image.format          = VK_FORMAT_R8G8B8A8_UINT;
        params.dst.image.extent          = defaultExtent;
        params.dst.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
        params.dst.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        params.allocationKind            = testGroupParams->allocationKind;
        params.extensionFlags            = testGroupParams->extensionFlags;
        params.queueSelection            = testGroupParams->queueSelection;
        params.useSecondaryCmdBuffer     = testGroupParams->useSecondaryCmdBuffer;
        params.useSparseBinding          = testGroupParams->useSparseBinding;

        {
            const VkImageCopy testCopy = {
                defaultSourceLayer, // VkImageSubresourceLayers srcSubresource;
                {0, 0, 0},          // VkOffset3D srcOffset;
                defaultSourceLayer, // VkImageSubresourceLayers dstSubresource;
                {0, 0, 0},          // VkOffset3D dstOffset;
                defaultExtent,      // VkExtent3D extent;
            };

            CopyRegion imageCopy;
            imageCopy.imageCopy = testCopy;
            params.regions.push_back(imageCopy);
        }

        group->addChild(new CopyImageToImageTestCase(testCtx, "whole_image", params));
    }

    {
        TestParams params;
        params.src.image.imageType       = VK_IMAGE_TYPE_2D;
        params.src.image.format          = VK_FORMAT_R8G8B8A8_UINT;
        params.src.image.extent          = defaultExtent;
        params.src.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
        params.src.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        params.dst.image.imageType       = VK_IMAGE_TYPE_2D;
        params.dst.image.format          = VK_FORMAT_R32_UINT;
        params.dst.image.extent          = defaultExtent;
        params.dst.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
        params.dst.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        params.allocationKind            = testGroupParams->allocationKind;
        params.extensionFlags            = testGroupParams->extensionFlags;
        params.queueSelection            = testGroupParams->queueSelection;
        params.useSecondaryCmdBuffer     = testGroupParams->useSecondaryCmdBuffer;
        params.useSparseBinding          = testGroupParams->useSparseBinding;

        {
            const VkImageCopy testCopy = {
                defaultSourceLayer, // VkImageSubresourceLayers srcSubresource;
                {0, 0, 0},          // VkOffset3D srcOffset;
                defaultSourceLayer, // VkImageSubresourceLayers dstSubresource;
                {0, 0, 0},          // VkOffset3D dstOffset;
                defaultExtent,      // VkExtent3D extent;
            };

            CopyRegion imageCopy;
            imageCopy.imageCopy = testCopy;
            params.regions.push_back(imageCopy);
        }

        group->addChild(new CopyImageToImageTestCase(testCtx, "whole_image_diff_format", params));
    }

    {
        TestParams params;
        params.src.image.imageType       = VK_IMAGE_TYPE_2D;
        params.src.image.format          = VK_FORMAT_R8G8B8A8_UINT;
        params.src.image.extent          = defaultExtent;
        params.src.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
        params.src.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        params.dst.image.imageType       = VK_IMAGE_TYPE_2D;
        params.dst.image.format          = VK_FORMAT_R8G8B8A8_UINT;
        params.dst.image.extent          = defaultExtent;
        params.dst.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
        params.dst.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        params.allocationKind            = testGroupParams->allocationKind;
        params.extensionFlags            = testGroupParams->extensionFlags;
        params.queueSelection            = testGroupParams->queueSelection;
        params.useSecondaryCmdBuffer     = testGroupParams->useSecondaryCmdBuffer;
        params.useSparseBinding          = testGroupParams->useSparseBinding;

        {
            const VkImageCopy testCopy = {
                defaultSourceLayer,                                  // VkImageSubresourceLayers srcSubresource;
                {0, 0, 0},                                           // VkOffset3D srcOffset;
                defaultSourceLayer,                                  // VkImageSubresourceLayers dstSubresource;
                {defaultQuarterSize, defaultQuarterSize / 2, 0},     // VkOffset3D dstOffset;
                {defaultQuarterSize / 2, defaultQuarterSize / 2, 1}, // VkExtent3D extent;
            };

            CopyRegion imageCopy;
            imageCopy.imageCopy = testCopy;
            params.regions.push_back(imageCopy);
        }

        group->addChild(new CopyImageToImageTestCase(testCtx, "partial_image", params));
    }

    static const struct
    {
        std::string name;
        vk::VkFormat format1;
        vk::VkFormat format2;
    } formats[] = {{"diff_format", vk::VK_FORMAT_R32_UINT, vk::VK_FORMAT_R8G8B8A8_UNORM},
                   {"same_format", vk::VK_FORMAT_R8G8B8A8_UNORM, vk::VK_FORMAT_R8G8B8A8_UNORM}};
    static const struct
    {
        std::string name;
        vk::VkBool32 clear;
    } clears[] = {{"clear", VK_TRUE}, {"noclear", VK_FALSE}};
    static const struct
    {
        std::string name;
        VkExtent3D extent;
    } extents[] = {{"npot", {65u, 63u, 1u}}, {"pot", {64u, 64u, 1u}}};

    for (const auto &format : formats)
    {
        for (const auto &clear : clears)
        {
            if (testGroupParams->queueSelection == QueueSelectionOptions::TransferOnly)
                continue;

            for (const auto &extent : extents)
            {
                TestParams params;
                params.src.image.imageType       = VK_IMAGE_TYPE_2D;
                params.src.image.format          = format.format1;
                params.src.image.extent          = extent.extent;
                params.src.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
                params.src.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
                params.dst.image.imageType       = VK_IMAGE_TYPE_2D;
                params.dst.image.format          = format.format2;
                params.dst.image.extent          = extent.extent;
                params.dst.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
                params.dst.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                params.allocationKind            = testGroupParams->allocationKind;
                params.extensionFlags            = testGroupParams->extensionFlags;
                params.queueSelection            = testGroupParams->queueSelection;
                params.useSecondaryCmdBuffer     = testGroupParams->useSecondaryCmdBuffer;
                params.useSparseBinding          = testGroupParams->useSparseBinding;
                params.clearDestinationWithRed   = clear.clear;

                {
                    VkImageCopy testCopy = {
                        defaultSourceLayer, // VkImageSubresourceLayers srcSubresource;
                        {34, 34, 0},        // VkOffset3D srcOffset;
                        defaultSourceLayer, // VkImageSubresourceLayers dstSubresource;
                        {0, 0, 0},          // VkOffset3D dstOffset;
                        {31, 29, 1}         // VkExtent3D extent;
                    };

                    if (extent.name == "pot")
                    {
                        testCopy.srcOffset = {16, 16, 0};
                        testCopy.extent    = {32, 32, 1};
                    }

                    CopyRegion imageCopy;
                    imageCopy.imageCopy = testCopy;
                    params.regions.push_back(imageCopy);
                }

                // Example test case name: "partial_image_npot_diff_format_clear"
                const std::string testCaseName = "partial_image_" + extent.name + "_" + format.name + "_" + clear.name;

                group->addChild(new CopyImageToImageTestCase(testCtx, testCaseName, params));
            }
        }
    }

    {
        TestParams params;
        params.src.image.imageType       = VK_IMAGE_TYPE_2D;
        params.src.image.format          = VK_FORMAT_D32_SFLOAT;
        params.src.image.extent          = defaultExtent;
        params.src.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
        params.src.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        params.dst.image.imageType       = VK_IMAGE_TYPE_2D;
        params.dst.image.format          = VK_FORMAT_D32_SFLOAT;
        params.dst.image.extent          = defaultExtent;
        params.dst.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
        params.dst.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        params.allocationKind            = testGroupParams->allocationKind;
        params.extensionFlags            = testGroupParams->extensionFlags;
        params.queueSelection            = testGroupParams->queueSelection;
        params.useSecondaryCmdBuffer     = testGroupParams->useSecondaryCmdBuffer;
        params.useSparseBinding          = testGroupParams->useSparseBinding;

        {
            const VkImageSubresourceLayers sourceLayer = {
                VK_IMAGE_ASPECT_DEPTH_BIT, // VkImageAspectFlags aspectMask;
                0u,                        // uint32_t mipLevel;
                0u,                        // uint32_t baseArrayLayer;
                1u                         // uint32_t layerCount;
            };
            const VkImageCopy testCopy = {
                sourceLayer,                                         // VkImageSubresourceLayers srcSubresource;
                {0, 0, 0},                                           // VkOffset3D srcOffset;
                sourceLayer,                                         // VkImageSubresourceLayers dstSubresource;
                {defaultQuarterSize, defaultQuarterSize / 2, 0},     // VkOffset3D dstOffset;
                {defaultQuarterSize / 2, defaultQuarterSize / 2, 1}, // VkExtent3D extent;
            };

            CopyRegion imageCopy;
            imageCopy.imageCopy = testCopy;
            params.regions.push_back(imageCopy);
        }

        group->addChild(new CopyImageToImageTestCase(testCtx, "depth", params));
    }

    {
        TestParams params;
        params.src.image.imageType       = VK_IMAGE_TYPE_2D;
        params.src.image.format          = VK_FORMAT_S8_UINT;
        params.src.image.extent          = defaultExtent;
        params.src.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
        params.src.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        params.dst.image.imageType       = VK_IMAGE_TYPE_2D;
        params.dst.image.format          = VK_FORMAT_S8_UINT;
        params.dst.image.extent          = defaultExtent;
        params.dst.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
        params.dst.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        params.allocationKind            = testGroupParams->allocationKind;
        params.extensionFlags            = testGroupParams->extensionFlags;
        params.queueSelection            = testGroupParams->queueSelection;
        params.useSecondaryCmdBuffer     = testGroupParams->useSecondaryCmdBuffer;
        params.useSparseBinding          = testGroupParams->useSparseBinding;

        {
            const VkImageSubresourceLayers sourceLayer = {
                VK_IMAGE_ASPECT_STENCIL_BIT, // VkImageAspectFlags aspectMask;
                0u,                          // uint32_t mipLevel;
                0u,                          // uint32_t baseArrayLayer;
                1u                           // uint32_t layerCount;
            };
            const VkImageCopy testCopy = {
                sourceLayer,                                         // VkImageSubresourceLayers srcSubresource;
                {0, 0, 0},                                           // VkOffset3D srcOffset;
                sourceLayer,                                         // VkImageSubresourceLayers dstSubresource;
                {defaultQuarterSize, defaultQuarterSize / 2, 0},     // VkOffset3D dstOffset;
                {defaultQuarterSize / 2, defaultQuarterSize / 2, 1}, // VkExtent3D extent;
            };

            CopyRegion imageCopy;
            imageCopy.imageCopy = testCopy;
            params.regions.push_back(imageCopy);
        }

        group->addChild(new CopyImageToImageTestCase(testCtx, "stencil", params));
    }
}

struct CopyColorTestParams
{
    TestParams params;
    const VkFormat *compatibleFormats;
};

void addImageToImageAllFormatsColorSrcFormatDstFormatTests(tcu::TestCaseGroup *group, TestParams params)
{
    const VkImageLayout copySrcLayouts[] = {VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL};
    const VkImageLayout copyDstLayouts[] = {VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL};

    for (int srcLayoutNdx = 0u; srcLayoutNdx < DE_LENGTH_OF_ARRAY(copySrcLayouts); ++srcLayoutNdx)
    {
        params.src.image.operationLayout = copySrcLayouts[srcLayoutNdx];

        for (int dstLayoutNdx = 0u; dstLayoutNdx < DE_LENGTH_OF_ARRAY(copyDstLayouts); ++dstLayoutNdx)
        {
            params.dst.image.operationLayout = copyDstLayouts[dstLayoutNdx];

            const std::string testName = getImageLayoutCaseName(params.src.image.operationLayout) + "_" +
                                         getImageLayoutCaseName(params.dst.image.operationLayout);
            const std::string description = "From layout " + getImageLayoutCaseName(params.src.image.operationLayout) +
                                            " to " + getImageLayoutCaseName(params.dst.image.operationLayout);
            group->addChild(new CopyImageToImageTestCase(group->getTestContext(), testName, params));
        }
    }
}

bool isAllowedImageToImageAllFormatsColorSrcFormatTests(const CopyColorTestParams &testParams)
{
    bool result = true;

    if (testParams.params.allocationKind == ALLOCATION_KIND_DEDICATED)
    {
        DE_ASSERT(!dedicatedAllocationImageToImageFormatsToTestSet.empty());

        result = de::contains(dedicatedAllocationImageToImageFormatsToTestSet, testParams.params.dst.image.format) ||
                 de::contains(dedicatedAllocationImageToImageFormatsToTestSet, testParams.params.src.image.format);
    }

    return result;
}

void addImageToImageAllFormatsColorSrcFormatTests(tcu::TestCaseGroup *group, CopyColorTestParams testParams)
{
    // If testParams.compatibleFormats is nullptr, the destination format will be copied from the source format.
    const VkFormat srcFormatOnly[2] = {testParams.params.src.image.format, VK_FORMAT_UNDEFINED};
    const VkFormat *formatList      = (testParams.compatibleFormats ? testParams.compatibleFormats : srcFormatOnly);

    for (int dstFormatIndex = 0; formatList[dstFormatIndex] != VK_FORMAT_UNDEFINED; ++dstFormatIndex)
    {
        testParams.params.dst.image.format = formatList[dstFormatIndex];

        const VkFormat srcFormat = testParams.params.src.image.format;
        const VkFormat dstFormat = testParams.params.dst.image.format;

        if (!isSupportedByFramework(dstFormat) && !isCompressedFormat(dstFormat))
            continue;

        if (!isAllowedImageToImageAllFormatsColorSrcFormatTests(testParams))
            continue;

        if (isCompressedFormat(srcFormat) && isCompressedFormat(dstFormat))
            if ((getBlockWidth(srcFormat) != getBlockWidth(dstFormat)) ||
                (getBlockHeight(srcFormat) != getBlockHeight(dstFormat)))
                continue;

        const std::string description = "Copy to destination format " + getFormatCaseName(dstFormat);
        addTestGroup(group, getFormatCaseName(dstFormat), addImageToImageAllFormatsColorSrcFormatDstFormatTests,
                     testParams.params);
    }
}

const VkFormat compatibleFormats8Bit[] = {VK_FORMAT_R4G4_UNORM_PACK8, VK_FORMAT_R8_UNORM,   VK_FORMAT_R8_SNORM,
                                          VK_FORMAT_R8_USCALED,       VK_FORMAT_R8_SSCALED, VK_FORMAT_R8_UINT,
                                          VK_FORMAT_R8_SINT,          VK_FORMAT_R8_SRGB,

                                          VK_FORMAT_UNDEFINED};

const VkFormat compatibleFormats8BitA[] = {
#ifndef CTS_USES_VULKANSC
    VK_FORMAT_A8_UNORM_KHR,
#endif // CTS_USES_VULKANSC
    VK_FORMAT_UNDEFINED};

const VkFormat compatibleFormats16Bit[] = {VK_FORMAT_R4G4B4A4_UNORM_PACK16,
                                           VK_FORMAT_B4G4R4A4_UNORM_PACK16,
                                           VK_FORMAT_R5G6B5_UNORM_PACK16,
                                           VK_FORMAT_B5G6R5_UNORM_PACK16,
                                           VK_FORMAT_R5G5B5A1_UNORM_PACK16,
                                           VK_FORMAT_B5G5R5A1_UNORM_PACK16,
                                           VK_FORMAT_A1R5G5B5_UNORM_PACK16,
#ifndef CTS_USES_VULKANSC
                                           VK_FORMAT_A1B5G5R5_UNORM_PACK16_KHR,
#endif // CTS_USES_VULKANSC
                                           VK_FORMAT_R8G8_UNORM,
                                           VK_FORMAT_R8G8_SNORM,
                                           VK_FORMAT_R8G8_USCALED,
                                           VK_FORMAT_R8G8_SSCALED,
                                           VK_FORMAT_R8G8_UINT,
                                           VK_FORMAT_R8G8_SINT,
                                           VK_FORMAT_R8G8_SRGB,
                                           VK_FORMAT_R16_UNORM,
                                           VK_FORMAT_R16_SNORM,
                                           VK_FORMAT_R16_USCALED,
                                           VK_FORMAT_R16_SSCALED,
                                           VK_FORMAT_R16_UINT,
                                           VK_FORMAT_R16_SINT,
                                           VK_FORMAT_R16_SFLOAT,
                                           VK_FORMAT_A4R4G4B4_UNORM_PACK16_EXT,
                                           VK_FORMAT_A4B4G4R4_UNORM_PACK16_EXT,

                                           VK_FORMAT_UNDEFINED};
const VkFormat compatibleFormats24Bit[] = {VK_FORMAT_R8G8B8_UNORM,   VK_FORMAT_R8G8B8_SNORM,   VK_FORMAT_R8G8B8_USCALED,
                                           VK_FORMAT_R8G8B8_SSCALED, VK_FORMAT_R8G8B8_UINT,    VK_FORMAT_R8G8B8_SINT,
                                           VK_FORMAT_R8G8B8_SRGB,    VK_FORMAT_B8G8R8_UNORM,   VK_FORMAT_B8G8R8_SNORM,
                                           VK_FORMAT_B8G8R8_USCALED, VK_FORMAT_B8G8R8_SSCALED, VK_FORMAT_B8G8R8_UINT,
                                           VK_FORMAT_B8G8R8_SINT,    VK_FORMAT_B8G8R8_SRGB,

                                           VK_FORMAT_UNDEFINED};
const VkFormat compatibleFormats32Bit[] = {VK_FORMAT_R8G8B8A8_UNORM,
                                           VK_FORMAT_R8G8B8A8_SNORM,
                                           VK_FORMAT_R8G8B8A8_USCALED,
                                           VK_FORMAT_R8G8B8A8_SSCALED,
                                           VK_FORMAT_R8G8B8A8_UINT,
                                           VK_FORMAT_R8G8B8A8_SINT,
                                           VK_FORMAT_R8G8B8A8_SRGB,
                                           VK_FORMAT_B8G8R8A8_UNORM,
                                           VK_FORMAT_B8G8R8A8_SNORM,
                                           VK_FORMAT_B8G8R8A8_USCALED,
                                           VK_FORMAT_B8G8R8A8_SSCALED,
                                           VK_FORMAT_B8G8R8A8_UINT,
                                           VK_FORMAT_B8G8R8A8_SINT,
                                           VK_FORMAT_B8G8R8A8_SRGB,
                                           VK_FORMAT_A8B8G8R8_UNORM_PACK32,
                                           VK_FORMAT_A8B8G8R8_SNORM_PACK32,
                                           VK_FORMAT_A8B8G8R8_USCALED_PACK32,
                                           VK_FORMAT_A8B8G8R8_SSCALED_PACK32,
                                           VK_FORMAT_A8B8G8R8_UINT_PACK32,
                                           VK_FORMAT_A8B8G8R8_SINT_PACK32,
                                           VK_FORMAT_A8B8G8R8_SRGB_PACK32,
                                           VK_FORMAT_A2R10G10B10_UNORM_PACK32,
                                           VK_FORMAT_A2R10G10B10_SNORM_PACK32,
                                           VK_FORMAT_A2R10G10B10_USCALED_PACK32,
                                           VK_FORMAT_A2R10G10B10_SSCALED_PACK32,
                                           VK_FORMAT_A2R10G10B10_UINT_PACK32,
                                           VK_FORMAT_A2R10G10B10_SINT_PACK32,
                                           VK_FORMAT_A2B10G10R10_UNORM_PACK32,
                                           VK_FORMAT_A2B10G10R10_SNORM_PACK32,
                                           VK_FORMAT_A2B10G10R10_USCALED_PACK32,
                                           VK_FORMAT_A2B10G10R10_SSCALED_PACK32,
                                           VK_FORMAT_A2B10G10R10_UINT_PACK32,
                                           VK_FORMAT_A2B10G10R10_SINT_PACK32,
                                           VK_FORMAT_R16G16_UNORM,
                                           VK_FORMAT_R16G16_SNORM,
                                           VK_FORMAT_R16G16_USCALED,
                                           VK_FORMAT_R16G16_SSCALED,
                                           VK_FORMAT_R16G16_UINT,
                                           VK_FORMAT_R16G16_SINT,
                                           VK_FORMAT_R16G16_SFLOAT,
                                           VK_FORMAT_R32_UINT,
                                           VK_FORMAT_R32_SINT,
                                           VK_FORMAT_R32_SFLOAT,

                                           VK_FORMAT_UNDEFINED};
const VkFormat compatibleFormats48Bit[] = {
    VK_FORMAT_R16G16B16_UNORM, VK_FORMAT_R16G16B16_SNORM, VK_FORMAT_R16G16B16_USCALED, VK_FORMAT_R16G16B16_SSCALED,
    VK_FORMAT_R16G16B16_UINT,  VK_FORMAT_R16G16B16_SINT,  VK_FORMAT_R16G16B16_SFLOAT,

    VK_FORMAT_UNDEFINED};
const VkFormat compatibleFormats64Bit[]  = {VK_FORMAT_R16G16B16A16_UNORM,
                                            VK_FORMAT_R16G16B16A16_SNORM,
                                            VK_FORMAT_R16G16B16A16_USCALED,
                                            VK_FORMAT_R16G16B16A16_SSCALED,
                                            VK_FORMAT_R16G16B16A16_UINT,
                                            VK_FORMAT_R16G16B16A16_SINT,
                                            VK_FORMAT_R16G16B16A16_SFLOAT,
                                            VK_FORMAT_R32G32_UINT,
                                            VK_FORMAT_R32G32_SINT,
                                            VK_FORMAT_R32G32_SFLOAT,
                                            VK_FORMAT_R64_UINT,
                                            VK_FORMAT_R64_SINT,
                                            VK_FORMAT_R64_SFLOAT,

                                            VK_FORMAT_BC1_RGB_UNORM_BLOCK,
                                            VK_FORMAT_BC1_RGB_SRGB_BLOCK,
                                            VK_FORMAT_BC1_RGBA_UNORM_BLOCK,
                                            VK_FORMAT_BC1_RGBA_SRGB_BLOCK,
                                            VK_FORMAT_BC4_UNORM_BLOCK,
                                            VK_FORMAT_BC4_SNORM_BLOCK,

                                            VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK,
                                            VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK,
                                            VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK,
                                            VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK,

                                            VK_FORMAT_EAC_R11_UNORM_BLOCK,
                                            VK_FORMAT_EAC_R11_SNORM_BLOCK,

                                            VK_FORMAT_UNDEFINED};
const VkFormat compatibleFormats96Bit[]  = {VK_FORMAT_R32G32B32_UINT, VK_FORMAT_R32G32B32_SINT,
                                            VK_FORMAT_R32G32B32_SFLOAT,

                                            VK_FORMAT_UNDEFINED};
const VkFormat compatibleFormats128Bit[] = {VK_FORMAT_R32G32B32A32_UINT,
                                            VK_FORMAT_R32G32B32A32_SINT,
                                            VK_FORMAT_R32G32B32A32_SFLOAT,
                                            VK_FORMAT_R64G64_UINT,
                                            VK_FORMAT_R64G64_SINT,
                                            VK_FORMAT_R64G64_SFLOAT,

                                            VK_FORMAT_BC2_UNORM_BLOCK,
                                            VK_FORMAT_BC2_SRGB_BLOCK,
                                            VK_FORMAT_BC3_UNORM_BLOCK,
                                            VK_FORMAT_BC3_SRGB_BLOCK,
                                            VK_FORMAT_BC5_UNORM_BLOCK,
                                            VK_FORMAT_BC5_SNORM_BLOCK,
                                            VK_FORMAT_BC6H_UFLOAT_BLOCK,
                                            VK_FORMAT_BC6H_SFLOAT_BLOCK,
                                            VK_FORMAT_BC7_UNORM_BLOCK,
                                            VK_FORMAT_BC7_SRGB_BLOCK,

                                            VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK,
                                            VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK,

                                            VK_FORMAT_EAC_R11G11_UNORM_BLOCK,
                                            VK_FORMAT_EAC_R11G11_SNORM_BLOCK,

                                            VK_FORMAT_ASTC_4x4_UNORM_BLOCK,
                                            VK_FORMAT_ASTC_4x4_SRGB_BLOCK,
                                            VK_FORMAT_ASTC_5x4_UNORM_BLOCK,
                                            VK_FORMAT_ASTC_5x4_SRGB_BLOCK,
                                            VK_FORMAT_ASTC_5x5_UNORM_BLOCK,
                                            VK_FORMAT_ASTC_5x5_SRGB_BLOCK,
                                            VK_FORMAT_ASTC_6x5_UNORM_BLOCK,
                                            VK_FORMAT_ASTC_6x5_SRGB_BLOCK,
                                            VK_FORMAT_ASTC_6x6_UNORM_BLOCK,
                                            VK_FORMAT_ASTC_6x6_SRGB_BLOCK,
                                            VK_FORMAT_ASTC_8x5_UNORM_BLOCK,
                                            VK_FORMAT_ASTC_8x5_SRGB_BLOCK,
                                            VK_FORMAT_ASTC_8x6_UNORM_BLOCK,
                                            VK_FORMAT_ASTC_8x6_SRGB_BLOCK,
                                            VK_FORMAT_ASTC_8x8_UNORM_BLOCK,
                                            VK_FORMAT_ASTC_8x8_SRGB_BLOCK,
                                            VK_FORMAT_ASTC_10x5_UNORM_BLOCK,
                                            VK_FORMAT_ASTC_10x5_SRGB_BLOCK,
                                            VK_FORMAT_ASTC_10x6_UNORM_BLOCK,
                                            VK_FORMAT_ASTC_10x6_SRGB_BLOCK,
                                            VK_FORMAT_ASTC_10x8_UNORM_BLOCK,
                                            VK_FORMAT_ASTC_10x8_SRGB_BLOCK,
                                            VK_FORMAT_ASTC_10x10_UNORM_BLOCK,
                                            VK_FORMAT_ASTC_10x10_SRGB_BLOCK,
                                            VK_FORMAT_ASTC_12x10_UNORM_BLOCK,
                                            VK_FORMAT_ASTC_12x10_SRGB_BLOCK,
                                            VK_FORMAT_ASTC_12x12_UNORM_BLOCK,
                                            VK_FORMAT_ASTC_12x12_SRGB_BLOCK,

                                            VK_FORMAT_UNDEFINED};
const VkFormat compatibleFormats192Bit[] = {VK_FORMAT_R64G64B64_UINT, VK_FORMAT_R64G64B64_SINT,
                                            VK_FORMAT_R64G64B64_SFLOAT,

                                            VK_FORMAT_UNDEFINED};
const VkFormat compatibleFormats256Bit[] = {VK_FORMAT_R64G64B64A64_UINT, VK_FORMAT_R64G64B64A64_SINT,
                                            VK_FORMAT_R64G64B64A64_SFLOAT,

                                            VK_FORMAT_UNDEFINED};

const VkFormat *colorImageFormatsToTest[] = {compatibleFormats8Bit,   compatibleFormats8BitA, compatibleFormats16Bit,
                                             compatibleFormats24Bit,  compatibleFormats32Bit, compatibleFormats48Bit,
                                             compatibleFormats64Bit,  compatibleFormats96Bit, compatibleFormats128Bit,
                                             compatibleFormats192Bit, compatibleFormats256Bit};

const VkFormat compatibleFormatsUInts[]  = {VK_FORMAT_R8_UINT,
                                            VK_FORMAT_R8G8_UINT,
                                            VK_FORMAT_R8G8B8_UINT,
                                            VK_FORMAT_B8G8R8_UINT,
                                            VK_FORMAT_R8G8B8A8_UINT,
                                            VK_FORMAT_B8G8R8A8_UINT,
                                            VK_FORMAT_A8B8G8R8_UINT_PACK32,
                                            VK_FORMAT_A2R10G10B10_UINT_PACK32,
                                            VK_FORMAT_A2B10G10R10_UINT_PACK32,
                                            VK_FORMAT_R16_UINT,
                                            VK_FORMAT_R16G16_UINT,
                                            VK_FORMAT_R16G16B16_UINT,
                                            VK_FORMAT_R16G16B16A16_UINT,
                                            VK_FORMAT_R32_UINT,
                                            VK_FORMAT_R32G32_UINT,
                                            VK_FORMAT_R32G32B32_UINT,
                                            VK_FORMAT_R32G32B32A32_UINT,
                                            VK_FORMAT_R64_UINT,
                                            VK_FORMAT_R64G64_UINT,
                                            VK_FORMAT_R64G64B64_UINT,
                                            VK_FORMAT_R64G64B64A64_UINT,

                                            VK_FORMAT_UNDEFINED};
const VkFormat compatibleFormatsSInts[]  = {VK_FORMAT_R8_SINT,
                                            VK_FORMAT_R8G8_SINT,
                                            VK_FORMAT_R8G8B8_SINT,
                                            VK_FORMAT_B8G8R8_SINT,
                                            VK_FORMAT_R8G8B8A8_SINT,
                                            VK_FORMAT_B8G8R8A8_SINT,
                                            VK_FORMAT_A8B8G8R8_SINT_PACK32,
                                            VK_FORMAT_A2R10G10B10_SINT_PACK32,
                                            VK_FORMAT_A2B10G10R10_SINT_PACK32,
                                            VK_FORMAT_R16_SINT,
                                            VK_FORMAT_R16G16_SINT,
                                            VK_FORMAT_R16G16B16_SINT,
                                            VK_FORMAT_R16G16B16A16_SINT,
                                            VK_FORMAT_R32_SINT,
                                            VK_FORMAT_R32G32_SINT,
                                            VK_FORMAT_R32G32B32_SINT,
                                            VK_FORMAT_R32G32B32A32_SINT,
                                            VK_FORMAT_R64_SINT,
                                            VK_FORMAT_R64G64_SINT,
                                            VK_FORMAT_R64G64B64_SINT,
                                            VK_FORMAT_R64G64B64A64_SINT,

                                            VK_FORMAT_UNDEFINED};
const VkFormat compatibleFormatsFloats[] = {VK_FORMAT_R4G4_UNORM_PACK8,
                                            VK_FORMAT_R4G4B4A4_UNORM_PACK16,
                                            VK_FORMAT_B4G4R4A4_UNORM_PACK16,
                                            VK_FORMAT_R5G6B5_UNORM_PACK16,
                                            VK_FORMAT_B5G6R5_UNORM_PACK16,
                                            VK_FORMAT_R5G5B5A1_UNORM_PACK16,
                                            VK_FORMAT_B5G5R5A1_UNORM_PACK16,
                                            VK_FORMAT_A1R5G5B5_UNORM_PACK16,
#ifndef CTS_USES_VULKANSC
                                            VK_FORMAT_A1B5G5R5_UNORM_PACK16_KHR,
#endif // CTS_USES_VULKANSC
                                            VK_FORMAT_R8_UNORM,
                                            VK_FORMAT_R8_SNORM,
                                            VK_FORMAT_R8_USCALED,
                                            VK_FORMAT_R8_SSCALED,
#ifndef CTS_USES_VULKANSC
                                            VK_FORMAT_A8_UNORM_KHR,
#endif // CTS_USES_VULKANSC
                                            VK_FORMAT_R8G8_UNORM,
                                            VK_FORMAT_R8G8_SNORM,
                                            VK_FORMAT_R8G8_USCALED,
                                            VK_FORMAT_R8G8_SSCALED,
                                            VK_FORMAT_R8G8B8_UNORM,
                                            VK_FORMAT_R8G8B8_SNORM,
                                            VK_FORMAT_R8G8B8_USCALED,
                                            VK_FORMAT_R8G8B8_SSCALED,
                                            VK_FORMAT_B8G8R8_UNORM,
                                            VK_FORMAT_B8G8R8_SNORM,
                                            VK_FORMAT_B8G8R8_USCALED,
                                            VK_FORMAT_B8G8R8_SSCALED,
                                            VK_FORMAT_R8G8B8A8_UNORM,
                                            VK_FORMAT_R8G8B8A8_SNORM,
                                            VK_FORMAT_R8G8B8A8_USCALED,
                                            VK_FORMAT_R8G8B8A8_SSCALED,
                                            VK_FORMAT_B8G8R8A8_UNORM,
                                            VK_FORMAT_B8G8R8A8_SNORM,
                                            VK_FORMAT_B8G8R8A8_USCALED,
                                            VK_FORMAT_B8G8R8A8_SSCALED,
                                            VK_FORMAT_A8B8G8R8_UNORM_PACK32,
                                            VK_FORMAT_A8B8G8R8_SNORM_PACK32,
                                            VK_FORMAT_A8B8G8R8_USCALED_PACK32,
                                            VK_FORMAT_A8B8G8R8_SSCALED_PACK32,
                                            VK_FORMAT_A2R10G10B10_UNORM_PACK32,
                                            VK_FORMAT_A2R10G10B10_SNORM_PACK32,
                                            VK_FORMAT_A2R10G10B10_USCALED_PACK32,
                                            VK_FORMAT_A2R10G10B10_SSCALED_PACK32,
                                            VK_FORMAT_A2B10G10R10_UNORM_PACK32,
                                            VK_FORMAT_A2B10G10R10_SNORM_PACK32,
                                            VK_FORMAT_A2B10G10R10_USCALED_PACK32,
                                            VK_FORMAT_A2B10G10R10_SSCALED_PACK32,
                                            VK_FORMAT_R16_UNORM,
                                            VK_FORMAT_R16_SNORM,
                                            VK_FORMAT_R16_USCALED,
                                            VK_FORMAT_R16_SSCALED,
                                            VK_FORMAT_R16_SFLOAT,
                                            VK_FORMAT_R16G16_UNORM,
                                            VK_FORMAT_R16G16_SNORM,
                                            VK_FORMAT_R16G16_USCALED,
                                            VK_FORMAT_R16G16_SSCALED,
                                            VK_FORMAT_R16G16_SFLOAT,
                                            VK_FORMAT_R16G16B16_UNORM,
                                            VK_FORMAT_R16G16B16_SNORM,
                                            VK_FORMAT_R16G16B16_USCALED,
                                            VK_FORMAT_R16G16B16_SSCALED,
                                            VK_FORMAT_R16G16B16_SFLOAT,
                                            VK_FORMAT_R16G16B16A16_UNORM,
                                            VK_FORMAT_R16G16B16A16_SNORM,
                                            VK_FORMAT_R16G16B16A16_USCALED,
                                            VK_FORMAT_R16G16B16A16_SSCALED,
                                            VK_FORMAT_R16G16B16A16_SFLOAT,
                                            VK_FORMAT_R32_SFLOAT,
                                            VK_FORMAT_R32G32_SFLOAT,
                                            VK_FORMAT_R32G32B32_SFLOAT,
                                            VK_FORMAT_R32G32B32A32_SFLOAT,
                                            VK_FORMAT_R64_SFLOAT,
                                            VK_FORMAT_R64G64_SFLOAT,
                                            VK_FORMAT_R64G64B64_SFLOAT,
                                            VK_FORMAT_R64G64B64A64_SFLOAT,
                                            VK_FORMAT_B10G11R11_UFLOAT_PACK32,
                                            VK_FORMAT_E5B9G9R9_UFLOAT_PACK32,

                                            VK_FORMAT_A4R4G4B4_UNORM_PACK16_EXT,
                                            VK_FORMAT_A4B4G4R4_UNORM_PACK16_EXT,

                                            VK_FORMAT_R10X6G10X6B10X6A10X6_UNORM_4PACK16,

                                            VK_FORMAT_UNDEFINED};

const VkFormat compressedFormatsFloats[] = {VK_FORMAT_BC1_RGB_UNORM_BLOCK,
                                            VK_FORMAT_BC1_RGBA_UNORM_BLOCK,
                                            VK_FORMAT_BC2_UNORM_BLOCK,
                                            VK_FORMAT_BC3_UNORM_BLOCK,
                                            VK_FORMAT_BC4_UNORM_BLOCK,
                                            VK_FORMAT_BC4_SNORM_BLOCK,
                                            VK_FORMAT_BC5_UNORM_BLOCK,
                                            VK_FORMAT_BC5_SNORM_BLOCK,
                                            VK_FORMAT_BC6H_UFLOAT_BLOCK,
                                            VK_FORMAT_BC6H_SFLOAT_BLOCK,
                                            VK_FORMAT_BC7_UNORM_BLOCK,
                                            VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK,
                                            VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK,
                                            VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK,
                                            VK_FORMAT_EAC_R11_UNORM_BLOCK,
                                            VK_FORMAT_EAC_R11_SNORM_BLOCK,
                                            VK_FORMAT_EAC_R11G11_UNORM_BLOCK,
                                            VK_FORMAT_EAC_R11G11_SNORM_BLOCK,
                                            VK_FORMAT_ASTC_4x4_UNORM_BLOCK,
                                            VK_FORMAT_ASTC_5x4_UNORM_BLOCK,
                                            VK_FORMAT_ASTC_5x5_UNORM_BLOCK,
                                            VK_FORMAT_ASTC_6x5_UNORM_BLOCK,
                                            VK_FORMAT_ASTC_6x6_UNORM_BLOCK,
                                            VK_FORMAT_ASTC_8x5_UNORM_BLOCK,
                                            VK_FORMAT_ASTC_8x6_UNORM_BLOCK,
                                            VK_FORMAT_ASTC_8x8_UNORM_BLOCK,
                                            VK_FORMAT_ASTC_10x5_UNORM_BLOCK,
                                            VK_FORMAT_ASTC_10x6_UNORM_BLOCK,
                                            VK_FORMAT_ASTC_10x8_UNORM_BLOCK,
                                            VK_FORMAT_ASTC_10x10_UNORM_BLOCK,
                                            VK_FORMAT_ASTC_12x10_UNORM_BLOCK,
                                            VK_FORMAT_ASTC_12x12_UNORM_BLOCK,

                                            VK_FORMAT_UNDEFINED};

const VkFormat compatibleFormatsSrgb[] = {
    VK_FORMAT_R8_SRGB,       VK_FORMAT_R8G8_SRGB,     VK_FORMAT_R8G8B8_SRGB,          VK_FORMAT_B8G8R8_SRGB,
    VK_FORMAT_R8G8B8A8_SRGB, VK_FORMAT_B8G8R8A8_SRGB, VK_FORMAT_A8B8G8R8_SRGB_PACK32,

    VK_FORMAT_UNDEFINED};

const VkFormat compressedFormatsSrgb[] = {VK_FORMAT_BC1_RGB_SRGB_BLOCK,
                                          VK_FORMAT_BC1_RGBA_SRGB_BLOCK,
                                          VK_FORMAT_BC2_SRGB_BLOCK,
                                          VK_FORMAT_BC3_SRGB_BLOCK,
                                          VK_FORMAT_BC7_SRGB_BLOCK,
                                          VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK,
                                          VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK,
                                          VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK,
                                          VK_FORMAT_ASTC_4x4_SRGB_BLOCK,
                                          VK_FORMAT_ASTC_5x4_SRGB_BLOCK,
                                          VK_FORMAT_ASTC_5x5_SRGB_BLOCK,
                                          VK_FORMAT_ASTC_6x5_SRGB_BLOCK,
                                          VK_FORMAT_ASTC_6x6_SRGB_BLOCK,
                                          VK_FORMAT_ASTC_8x5_SRGB_BLOCK,
                                          VK_FORMAT_ASTC_8x6_SRGB_BLOCK,
                                          VK_FORMAT_ASTC_8x8_SRGB_BLOCK,
                                          VK_FORMAT_ASTC_10x5_SRGB_BLOCK,
                                          VK_FORMAT_ASTC_10x6_SRGB_BLOCK,
                                          VK_FORMAT_ASTC_10x8_SRGB_BLOCK,
                                          VK_FORMAT_ASTC_10x10_SRGB_BLOCK,
                                          VK_FORMAT_ASTC_12x10_SRGB_BLOCK,
                                          VK_FORMAT_ASTC_12x12_SRGB_BLOCK,

                                          VK_FORMAT_UNDEFINED};

const VkFormat dedicatedAllocationImageToImageFormatsToTest[] = {
    // From compatibleFormats8Bit
    VK_FORMAT_R4G4_UNORM_PACK8,
    VK_FORMAT_R8_SRGB,

    // From compatibleFormats16Bit
    VK_FORMAT_R4G4B4A4_UNORM_PACK16,
    VK_FORMAT_R16_SFLOAT,

    // From compatibleFormats24Bit
    VK_FORMAT_R8G8B8_UNORM,
    VK_FORMAT_B8G8R8_SRGB,

    // From compatibleFormats32Bit
    VK_FORMAT_R8G8B8A8_UNORM,
    VK_FORMAT_R32_SFLOAT,

    // From compatibleFormats48Bit
    VK_FORMAT_R16G16B16_UNORM,
    VK_FORMAT_R16G16B16_SFLOAT,

    // From compatibleFormats64Bit
    VK_FORMAT_R16G16B16A16_UNORM,
    VK_FORMAT_R64_SFLOAT,

    // From compatibleFormats96Bit
    VK_FORMAT_R32G32B32_UINT,
    VK_FORMAT_R32G32B32_SFLOAT,

    // From compatibleFormats128Bit
    VK_FORMAT_R32G32B32A32_UINT,
    VK_FORMAT_R64G64_SFLOAT,

    // From compatibleFormats192Bit
    VK_FORMAT_R64G64B64_UINT,
    VK_FORMAT_R64G64B64_SFLOAT,

    // From compatibleFormats256Bit
    VK_FORMAT_R64G64B64A64_UINT,
    VK_FORMAT_R64G64B64A64_SFLOAT,
};

void addImageToImageAllFormatsColorTests(tcu::TestCaseGroup *group, TestGroupParamsPtr testGroupParams)
{
    if (testGroupParams->allocationKind == ALLOCATION_KIND_DEDICATED)
    {
        const int numOfDedicatedAllocationImageToImageFormatsToTest =
            DE_LENGTH_OF_ARRAY(dedicatedAllocationImageToImageFormatsToTest);
        for (int compatibleFormatsIndex = 0; compatibleFormatsIndex < numOfDedicatedAllocationImageToImageFormatsToTest;
             ++compatibleFormatsIndex)
            dedicatedAllocationImageToImageFormatsToTestSet.insert(
                dedicatedAllocationImageToImageFormatsToTest[compatibleFormatsIndex]);
    }

    // 1D to 1D tests.
    {
        // 1D to 1D copies
        de::MovePtr<tcu::TestCaseGroup> subGroup(new tcu::TestCaseGroup(group->getTestContext(), "1d_to_1d"));

        TestParams params;
        params.src.image.imageType = VK_IMAGE_TYPE_1D;
        params.dst.image.imageType = VK_IMAGE_TYPE_1D;
        params.src.image.extent    = default1dExtent;
        params.dst.image.extent    = default1dExtent;
        params.src.image.tiling    = VK_IMAGE_TILING_OPTIMAL;
        params.dst.image.tiling    = VK_IMAGE_TILING_OPTIMAL;
        params.src.image.fillMode  = FILL_MODE_WHITE;
        params.dst.image.fillMode  = FILL_MODE_GRADIENT;
        params.allocationKind      = testGroupParams->allocationKind;
        params.extensionFlags      = testGroupParams->extensionFlags;
        params.queueSelection      = testGroupParams->queueSelection;

        for (int32_t i = defaultQuarterSize; i < defaultSize; i += defaultSize / 2)
        {
            const VkImageCopy testCopy = {
                defaultSourceLayer,         // VkImageSubresourceLayers srcSubresource;
                {0, 0, 0},                  // VkOffset3D srcOffset;
                defaultSourceLayer,         // VkImageSubresourceLayers dstSubresource;
                {i, 0, 0},                  // VkOffset3D dstOffset;
                {defaultQuarterSize, 1, 1}, // VkExtent3D extent;
            };

            CopyRegion imageCopy;
            imageCopy.imageCopy = testCopy;

            params.regions.push_back(imageCopy);
        }

        const int numOfColorImageFormatsToTest = DE_LENGTH_OF_ARRAY(colorImageFormatsToTest);
        for (int compatibleFormatsIndex = 0; compatibleFormatsIndex < numOfColorImageFormatsToTest;
             ++compatibleFormatsIndex)
        {
            const VkFormat *compatibleFormats = colorImageFormatsToTest[compatibleFormatsIndex];
            for (int srcFormatIndex = 0; compatibleFormats[srcFormatIndex] != VK_FORMAT_UNDEFINED; ++srcFormatIndex)
            {
                params.src.image.format = compatibleFormats[srcFormatIndex];
                if (!isSupportedByFramework(params.src.image.format) && !isCompressedFormat(params.src.image.format))
                    continue;

                CopyColorTestParams testParams;
                testParams.params            = params;
                testParams.compatibleFormats = nullptr;

                const std::string testName = getFormatCaseName(params.src.image.format);
                addTestGroup(subGroup.get(), testName, addImageToImageAllFormatsColorSrcFormatTests, testParams);
            }
        }

        group->addChild(subGroup.release());
    }

    // 1D to 2D tests.
    {
        // 1D to 2D copies
        de::MovePtr<tcu::TestCaseGroup> subGroup(new tcu::TestCaseGroup(group->getTestContext(), "1d_to_2d"));

        TestParams params;
        params.src.image.imageType = VK_IMAGE_TYPE_1D;
        params.dst.image.imageType = VK_IMAGE_TYPE_2D;
        params.src.image.extent    = default1dExtent;
        params.dst.image.extent    = defaultRootExtent;
        params.src.image.tiling    = VK_IMAGE_TILING_OPTIMAL;
        params.dst.image.tiling    = VK_IMAGE_TILING_OPTIMAL;
        params.src.image.fillMode  = FILL_MODE_WHITE;
        params.dst.image.fillMode  = FILL_MODE_GRADIENT;
        params.allocationKind      = testGroupParams->allocationKind;
        params.extensionFlags      = testGroupParams->extensionFlags;
        params.queueSelection      = testGroupParams->queueSelection;
        params.extensionFlags |= MAINTENANCE_5;

        for (uint32_t i = 0; i < defaultRootSize; ++i)
        {
            const VkImageCopy testCopy = {
                defaultSourceLayer,                                // VkImageSubresourceLayers srcSubresource;
                {static_cast<int32_t>(defaultRootSize * i), 0, 0}, // VkOffset3D srcOffset;
                defaultSourceLayer,                                // VkImageSubresourceLayers dstSubresource;
                {0, static_cast<int32_t>(i), 0},                   // VkOffset3D dstOffset;
                {defaultRootSize, 1, 1},                           // VkExtent3D extent;
            };

            CopyRegion imageCopy;
            imageCopy.imageCopy = testCopy;

            params.regions.push_back(imageCopy);
        }

        const int numOfColorImageFormatsToTest = DE_LENGTH_OF_ARRAY(colorImageFormatsToTest);
        for (int compatibleFormatsIndex = 0; compatibleFormatsIndex < numOfColorImageFormatsToTest;
             ++compatibleFormatsIndex)
        {
            const VkFormat *compatibleFormats = colorImageFormatsToTest[compatibleFormatsIndex];
            for (int srcFormatIndex = 0; compatibleFormats[srcFormatIndex] != VK_FORMAT_UNDEFINED; ++srcFormatIndex)
            {
                params.src.image.format = compatibleFormats[srcFormatIndex];
                if (!isSupportedByFramework(params.src.image.format) && !isCompressedFormat(params.src.image.format))
                    continue;

                CopyColorTestParams testParams;
                testParams.params            = params;
                testParams.compatibleFormats = nullptr;

                const std::string testName = getFormatCaseName(params.src.image.format);
                addTestGroup(subGroup.get(), testName, addImageToImageAllFormatsColorSrcFormatTests, testParams);
            }
        }

        group->addChild(subGroup.release());
    }

    // 1D to 3D tests.
    {
        de::MovePtr<tcu::TestCaseGroup> subGroup(new tcu::TestCaseGroup(group->getTestContext(), "1d_to_3d"));

        TestParams params;
        params.src.image.imageType = VK_IMAGE_TYPE_1D;
        params.dst.image.imageType = VK_IMAGE_TYPE_3D;
        params.src.image.extent    = default1dExtent;
        params.dst.image.extent    = default3dSmallExtent;
        params.src.image.tiling    = VK_IMAGE_TILING_OPTIMAL;
        params.dst.image.tiling    = VK_IMAGE_TILING_OPTIMAL;
        params.src.image.fillMode  = FILL_MODE_WHITE;
        params.dst.image.fillMode  = FILL_MODE_GRADIENT;
        params.allocationKind      = testGroupParams->allocationKind;
        params.extensionFlags      = testGroupParams->extensionFlags;
        params.queueSelection      = testGroupParams->queueSelection;
        params.extensionFlags |= MAINTENANCE_5;

        for (int32_t i = 0; i < defaultSixteenthSize; ++i)
        {
            for (int32_t j = 0; j < defaultSixteenthSize; ++j)
            {
                const VkImageCopy testCopy = {
                    defaultSourceLayer, // VkImageSubresourceLayers srcSubresource;
                    {i * defaultQuarterSize + j * defaultSixteenthSize, 0, 0}, // VkOffset3D srcOffset;
                    defaultSourceLayer,               // VkImageSubresourceLayers dstSubresource;
                    {0, j, i % defaultSixteenthSize}, // VkOffset3D dstOffset;
                    {defaultSixteenthSize, 1, 1},     // VkExtent3D extent;
                };

                CopyRegion imageCopy;
                imageCopy.imageCopy = testCopy;

                params.regions.push_back(imageCopy);
            }
        }

        const int numOfColorImageFormatsToTest = DE_LENGTH_OF_ARRAY(colorImageFormatsToTest);
        for (int compatibleFormatsIndex = 0; compatibleFormatsIndex < numOfColorImageFormatsToTest;
             ++compatibleFormatsIndex)
        {
            const VkFormat *compatibleFormats = colorImageFormatsToTest[compatibleFormatsIndex];
            for (int srcFormatIndex = 0; compatibleFormats[srcFormatIndex] != VK_FORMAT_UNDEFINED; ++srcFormatIndex)
            {
                params.src.image.format = compatibleFormats[srcFormatIndex];
                if (!isSupportedByFramework(params.src.image.format) && !isCompressedFormat(params.src.image.format))
                    continue;

                CopyColorTestParams testParams;
                testParams.params            = params;
                testParams.compatibleFormats = nullptr;

                const std::string testName = getFormatCaseName(params.src.image.format);
                addTestGroup(subGroup.get(), testName, addImageToImageAllFormatsColorSrcFormatTests, testParams);
            }
        }

        group->addChild(subGroup.release());
    }

    // 2D to 1D tests.
    {
        de::MovePtr<tcu::TestCaseGroup> subGroup(new tcu::TestCaseGroup(group->getTestContext(), "2d_to_1d"));

        TestParams params;
        params.src.image.imageType = VK_IMAGE_TYPE_2D;
        params.dst.image.imageType = VK_IMAGE_TYPE_1D;
        params.src.image.extent    = defaultQuarterExtent;
        params.dst.image.extent    = default1dQuarterSquaredExtent;
        params.src.image.tiling    = VK_IMAGE_TILING_OPTIMAL;
        params.dst.image.tiling    = VK_IMAGE_TILING_OPTIMAL;
        params.src.image.fillMode  = FILL_MODE_WHITE;
        params.dst.image.fillMode  = FILL_MODE_GRADIENT;
        params.allocationKind      = testGroupParams->allocationKind;
        params.extensionFlags      = testGroupParams->extensionFlags;
        params.queueSelection      = testGroupParams->queueSelection;
        params.extensionFlags |= MAINTENANCE_5;

        for (int32_t i = 0; i < defaultQuarterSize; ++i)
        {
            const VkImageCopy testCopy = {
                defaultSourceLayer,             // VkImageSubresourceLayers srcSubresource;
                {0, i, 0},                      // VkOffset3D srcOffset;
                defaultSourceLayer,             // VkImageSubresourceLayers dstSubresource;
                {i * defaultQuarterSize, 0, 0}, // VkOffset3D dstOffset;
                {defaultQuarterSize, 1, 1},     // VkExtent3D extent;
            };

            CopyRegion imageCopy;
            imageCopy.imageCopy = testCopy;

            params.regions.push_back(imageCopy);
        }

        const int numOfColorImageFormatsToTest = DE_LENGTH_OF_ARRAY(colorImageFormatsToTest);
        for (int compatibleFormatsIndex = 0; compatibleFormatsIndex < numOfColorImageFormatsToTest;
             ++compatibleFormatsIndex)
        {
            const VkFormat *compatibleFormats = colorImageFormatsToTest[compatibleFormatsIndex];
            for (int srcFormatIndex = 0; compatibleFormats[srcFormatIndex] != VK_FORMAT_UNDEFINED; ++srcFormatIndex)
            {
                params.src.image.format = compatibleFormats[srcFormatIndex];
                if (!isSupportedByFramework(params.src.image.format) && !isCompressedFormat(params.src.image.format))
                    continue;

                CopyColorTestParams testParams;
                testParams.params            = params;
                testParams.compatibleFormats = compatibleFormats;

                const std::string testName = getFormatCaseName(params.src.image.format);
                addTestGroup(subGroup.get(), testName, addImageToImageAllFormatsColorSrcFormatTests, testParams);
            }
        }

        group->addChild(subGroup.release());
    }

    // 2D to 2D tests.
    {
        de::MovePtr<tcu::TestCaseGroup> subGroup(new tcu::TestCaseGroup(group->getTestContext(), "2d_to_2d"));

        TestParams params;
        params.src.image.imageType = VK_IMAGE_TYPE_2D;
        params.dst.image.imageType = VK_IMAGE_TYPE_2D;
        params.src.image.extent    = defaultExtent;
        params.dst.image.extent    = defaultExtent;
        params.src.image.tiling    = VK_IMAGE_TILING_OPTIMAL;
        params.dst.image.tiling    = VK_IMAGE_TILING_OPTIMAL;
        params.src.image.fillMode  = FILL_MODE_WHITE;
        params.dst.image.fillMode  = FILL_MODE_GRADIENT;
        params.allocationKind      = testGroupParams->allocationKind;
        params.extensionFlags      = testGroupParams->extensionFlags;
        params.queueSelection      = testGroupParams->queueSelection;

        for (int32_t i = 0; i < defaultSize; i += defaultQuarterSize)
        {
            const VkImageCopy testCopy = {
                defaultSourceLayer,                           // VkImageSubresourceLayers srcSubresource;
                {0, 0, 0},                                    // VkOffset3D srcOffset;
                defaultSourceLayer,                           // VkImageSubresourceLayers dstSubresource;
                {i, defaultSize - i - defaultQuarterSize, 0}, // VkOffset3D dstOffset;
                {defaultQuarterSize, defaultQuarterSize, 1},  // VkExtent3D extent;
            };

            CopyRegion imageCopy;
            imageCopy.imageCopy = testCopy;

            params.regions.push_back(imageCopy);
        }

        const int numOfColorImageFormatsToTest = DE_LENGTH_OF_ARRAY(colorImageFormatsToTest);
        for (int compatibleFormatsIndex = 0; compatibleFormatsIndex < numOfColorImageFormatsToTest;
             ++compatibleFormatsIndex)
        {
            const VkFormat *compatibleFormats = colorImageFormatsToTest[compatibleFormatsIndex];
            for (int srcFormatIndex = 0; compatibleFormats[srcFormatIndex] != VK_FORMAT_UNDEFINED; ++srcFormatIndex)
            {
                params.src.image.format = compatibleFormats[srcFormatIndex];
                if (!isSupportedByFramework(params.src.image.format) && !isCompressedFormat(params.src.image.format))
                    continue;

                CopyColorTestParams testParams;
                testParams.params            = params;
                testParams.compatibleFormats = compatibleFormats;

                const std::string testName = getFormatCaseName(params.src.image.format);
                addTestGroup(subGroup.get(), testName, addImageToImageAllFormatsColorSrcFormatTests, testParams);
            }
        }

        group->addChild(subGroup.release());
    }

    // 2D to 3D tests.
    {
        de::MovePtr<tcu::TestCaseGroup> subGroup(new tcu::TestCaseGroup(group->getTestContext(), "2d_to_3d"));

        TestParams params;
        params.src.image.imageType = VK_IMAGE_TYPE_2D;
        params.dst.image.imageType = VK_IMAGE_TYPE_3D;
        params.src.image.extent    = defaultExtent;
        params.dst.image.extent    = default3dSmallExtent;
        params.src.image.tiling    = VK_IMAGE_TILING_OPTIMAL;
        params.dst.image.tiling    = VK_IMAGE_TILING_OPTIMAL;
        params.src.image.fillMode  = FILL_MODE_WHITE;
        params.dst.image.fillMode  = FILL_MODE_GRADIENT;
        params.allocationKind      = testGroupParams->allocationKind;
        params.extensionFlags      = testGroupParams->extensionFlags;
        params.queueSelection      = testGroupParams->queueSelection;
        params.extensionFlags |= MAINTENANCE_1;

        for (int32_t i = 0; i < defaultSixteenthSize; ++i)
        {
            const VkImageCopy testCopy = {
                defaultSourceLayer,                                      // VkImageSubresourceLayers srcSubresource;
                {i * defaultSixteenthSize, i % defaultSixteenthSize, 0}, // VkOffset3D srcOffset;
                defaultSourceLayer,                                      // VkImageSubresourceLayers dstSubresource;
                {0, 0, i},                                               // VkOffset3D dstOffset;
                {defaultSixteenthSize, defaultSixteenthSize, 1},         // VkExtent3D extent;
            };

            CopyRegion imageCopy;
            imageCopy.imageCopy = testCopy;

            params.regions.push_back(imageCopy);
        }

        const int numOfColorImageFormatsToTest = DE_LENGTH_OF_ARRAY(colorImageFormatsToTest);
        for (int compatibleFormatsIndex = 0; compatibleFormatsIndex < numOfColorImageFormatsToTest;
             ++compatibleFormatsIndex)
        {
            const VkFormat *compatibleFormats = colorImageFormatsToTest[compatibleFormatsIndex];
            for (int srcFormatIndex = 0; compatibleFormats[srcFormatIndex] != VK_FORMAT_UNDEFINED; ++srcFormatIndex)
            {
                params.src.image.format = compatibleFormats[srcFormatIndex];
                if (!isSupportedByFramework(params.src.image.format) && !isCompressedFormat(params.src.image.format))
                    continue;

                CopyColorTestParams testParams;
                testParams.params            = params;
                testParams.compatibleFormats = compatibleFormats;

                const std::string testName = getFormatCaseName(params.src.image.format);
                addTestGroup(subGroup.get(), testName, addImageToImageAllFormatsColorSrcFormatTests, testParams);
            }
        }

        group->addChild(subGroup.release());
    }

    // 3D to 1D tests.
    {
        de::MovePtr<tcu::TestCaseGroup> subGroup(new tcu::TestCaseGroup(group->getTestContext(), "3d_to_1d"));

        TestParams params;
        params.src.image.imageType = VK_IMAGE_TYPE_3D;
        params.dst.image.imageType = VK_IMAGE_TYPE_1D;
        params.src.image.extent    = default3dSmallExtent;
        params.dst.image.extent    = default1dExtent;
        params.src.image.tiling    = VK_IMAGE_TILING_OPTIMAL;
        params.dst.image.tiling    = VK_IMAGE_TILING_OPTIMAL;
        params.src.image.fillMode  = FILL_MODE_WHITE;
        params.dst.image.fillMode  = FILL_MODE_GRADIENT;
        params.allocationKind      = testGroupParams->allocationKind;
        params.extensionFlags      = testGroupParams->extensionFlags;
        params.queueSelection      = testGroupParams->queueSelection;
        params.extensionFlags |= MAINTENANCE_5;

        for (int32_t i = 0; i < defaultSixteenthSize; ++i)
        {
            for (int32_t j = 0; j < defaultSixteenthSize; ++j)
            {
                const VkImageCopy testCopy = {
                    defaultSourceLayer,                                      // VkImageSubresourceLayers srcSubresource;
                    {0, j % defaultSixteenthSize, i % defaultSixteenthSize}, // VkOffset3D srcOffset;
                    defaultSourceLayer,                                      // VkImageSubresourceLayers dstSubresource;
                    {j * defaultSixteenthSize + i * defaultQuarterSize, 0, 0}, // VkOffset3D dstOffset;
                    {defaultSixteenthSize, 1, 1},                              // VkExtent3D extent;
                };

                CopyRegion imageCopy;
                imageCopy.imageCopy = testCopy;

                params.regions.push_back(imageCopy);
            }
        }

        const int numOfColorImageFormatsToTest = DE_LENGTH_OF_ARRAY(colorImageFormatsToTest);
        for (int compatibleFormatsIndex = 0; compatibleFormatsIndex < numOfColorImageFormatsToTest;
             ++compatibleFormatsIndex)
        {
            const VkFormat *compatibleFormats = colorImageFormatsToTest[compatibleFormatsIndex];
            for (int srcFormatIndex = 0; compatibleFormats[srcFormatIndex] != VK_FORMAT_UNDEFINED; ++srcFormatIndex)
            {
                params.src.image.format = compatibleFormats[srcFormatIndex];
                if (!isSupportedByFramework(params.src.image.format) && !isCompressedFormat(params.src.image.format))
                    continue;

                CopyColorTestParams testParams;
                testParams.params            = params;
                testParams.compatibleFormats = nullptr;

                const std::string testName = getFormatCaseName(params.src.image.format);
                addTestGroup(subGroup.get(), testName, addImageToImageAllFormatsColorSrcFormatTests, testParams);
            }
        }

        group->addChild(subGroup.release());
    }

    // 3D to 2D tests.
    {
        de::MovePtr<tcu::TestCaseGroup> subGroup(new tcu::TestCaseGroup(group->getTestContext(), "3d_to_2d"));

        TestParams params;
        params.src.image.imageType = VK_IMAGE_TYPE_3D;
        params.dst.image.imageType = VK_IMAGE_TYPE_2D;
        params.src.image.extent    = default3dExtent;
        params.dst.image.extent    = defaultExtent;
        params.src.image.tiling    = VK_IMAGE_TILING_OPTIMAL;
        params.dst.image.tiling    = VK_IMAGE_TILING_OPTIMAL;
        params.src.image.fillMode  = FILL_MODE_WHITE;
        params.dst.image.fillMode  = FILL_MODE_GRADIENT;
        params.allocationKind      = testGroupParams->allocationKind;
        params.extensionFlags      = testGroupParams->extensionFlags;
        params.queueSelection      = testGroupParams->queueSelection;
        params.extensionFlags |= MAINTENANCE_1;

        for (int32_t i = 0; i < defaultSixteenthSize; ++i)
        {
            for (int32_t j = 0; j < defaultSixteenthSize; ++j)
            {
                const VkImageCopy testCopy = {
                    defaultSourceLayer,                                  // VkImageSubresourceLayers srcSubresource;
                    {0, 0, i * defaultSixteenthSize + j},                // VkOffset3D srcOffset;
                    defaultSourceLayer,                                  // VkImageSubresourceLayers dstSubresource;
                    {j * defaultQuarterSize, i * defaultQuarterSize, 0}, // VkOffset3D dstOffset;
                    {defaultQuarterSize, defaultQuarterSize, 1},         // VkExtent3D extent;
                };

                CopyRegion imageCopy;
                imageCopy.imageCopy = testCopy;

                params.regions.push_back(imageCopy);
            }
        }

        const int numOfColorImageFormatsToTest = DE_LENGTH_OF_ARRAY(colorImageFormatsToTest);
        for (int compatibleFormatsIndex = 0; compatibleFormatsIndex < numOfColorImageFormatsToTest;
             ++compatibleFormatsIndex)
        {
            const VkFormat *compatibleFormats = colorImageFormatsToTest[compatibleFormatsIndex];
            for (int srcFormatIndex = 0; compatibleFormats[srcFormatIndex] != VK_FORMAT_UNDEFINED; ++srcFormatIndex)
            {
                params.src.image.format = compatibleFormats[srcFormatIndex];
                if (!isSupportedByFramework(params.src.image.format) && !isCompressedFormat(params.src.image.format))
                    continue;

                CopyColorTestParams testParams;
                testParams.params            = params;
                testParams.compatibleFormats = nullptr;

                const std::string testName = getFormatCaseName(params.src.image.format);
                addTestGroup(subGroup.get(), testName, addImageToImageAllFormatsColorSrcFormatTests, testParams);
            }
        }

        group->addChild(subGroup.release());
    }

    // 3D to 3D tests. Note we use smaller dimensions here for performance reasons.
    {
        de::MovePtr<tcu::TestCaseGroup> subGroup(new tcu::TestCaseGroup(group->getTestContext(), "3d_to_3d"));

        TestParams params;
        params.src.image.imageType = VK_IMAGE_TYPE_3D;
        params.dst.image.imageType = VK_IMAGE_TYPE_3D;
        params.src.image.extent    = default3dExtent;
        params.dst.image.extent    = default3dExtent;
        params.src.image.tiling    = VK_IMAGE_TILING_OPTIMAL;
        params.dst.image.tiling    = VK_IMAGE_TILING_OPTIMAL;
        params.src.image.fillMode  = FILL_MODE_WHITE;
        params.dst.image.fillMode  = FILL_MODE_GRADIENT;
        params.allocationKind      = testGroupParams->allocationKind;
        params.extensionFlags      = testGroupParams->extensionFlags;
        params.queueSelection      = testGroupParams->queueSelection;

        for (int32_t i = 0; i < defaultQuarterSize; i += defaultSixteenthSize)
        {
            const VkImageCopy testCopy = {
                defaultSourceLayer,                                    // VkImageSubresourceLayers srcSubresource;
                {0, 0, 0},                                             // VkOffset3D srcOffset;
                defaultSourceLayer,                                    // VkImageSubresourceLayers dstSubresource;
                {i, defaultQuarterSize - i - defaultSixteenthSize, i}, // VkOffset3D dstOffset;
                {defaultSixteenthSize, defaultSixteenthSize, defaultSixteenthSize}, // VkExtent3D extent;
            };

            CopyRegion imageCopy;
            imageCopy.imageCopy = testCopy;

            params.regions.push_back(imageCopy);
        }

        const int numOfColorImageFormatsToTest = DE_LENGTH_OF_ARRAY(colorImageFormatsToTest);
        for (int compatibleFormatsIndex = 0; compatibleFormatsIndex < numOfColorImageFormatsToTest;
             ++compatibleFormatsIndex)
        {
            const VkFormat *compatibleFormats = colorImageFormatsToTest[compatibleFormatsIndex];
            for (int srcFormatIndex = 0; compatibleFormats[srcFormatIndex] != VK_FORMAT_UNDEFINED; ++srcFormatIndex)
            {
                params.src.image.format = compatibleFormats[srcFormatIndex];
                if (!isSupportedByFramework(params.src.image.format) && !isCompressedFormat(params.src.image.format))
                    continue;

                CopyColorTestParams testParams;
                testParams.params            = params;
                testParams.compatibleFormats = nullptr;

                const std::string testName = getFormatCaseName(params.src.image.format);
                addTestGroup(subGroup.get(), testName, addImageToImageAllFormatsColorSrcFormatTests, testParams);
            }
        }

        group->addChild(subGroup.release());
    }
}

void addImageToImageDimensionsTests(tcu::TestCaseGroup *group, TestGroupParamsPtr testGroupParams)
{
    tcu::TestContext &testCtx = group->getTestContext();

    const VkFormat testFormats[][2] = {// From compatibleFormats8Bit
                                       {VK_FORMAT_R4G4_UNORM_PACK8, VK_FORMAT_R8_SRGB},
                                       // From compatibleFormats16Bit
                                       {
                                           VK_FORMAT_R4G4B4A4_UNORM_PACK16,
                                           VK_FORMAT_R16_SFLOAT,
                                       },
                                       // From compatibleFormats24Bit
                                       {VK_FORMAT_R8G8B8_UNORM, VK_FORMAT_B8G8R8_SRGB},
                                       // From compatibleFormats32Bit
                                       {VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_R32_SFLOAT},
                                       // From compatibleFormats48Bit
                                       {VK_FORMAT_R16G16B16_UNORM, VK_FORMAT_R16G16B16_SFLOAT},
                                       // From compatibleFormats64Bit
                                       {VK_FORMAT_R16G16B16A16_UNORM, VK_FORMAT_R64_SFLOAT},
                                       // From compatibleFormats96Bit
                                       {VK_FORMAT_R32G32B32_UINT, VK_FORMAT_R32G32B32_SFLOAT},
                                       // From compatibleFormats128Bit
                                       {VK_FORMAT_R32G32B32A32_UINT, VK_FORMAT_R64G64_SFLOAT},
                                       // From compatibleFormats192Bit
                                       {
                                           VK_FORMAT_R64G64B64_UINT,
                                           VK_FORMAT_R64G64B64_SFLOAT,
                                       },
                                       // From compatibleFormats256Bit
                                       {VK_FORMAT_R64G64B64A64_UINT, VK_FORMAT_R64G64B64A64_SFLOAT}};

    const tcu::UVec2 imageDimensions[] = {
        // large pot x small pot
        tcu::UVec2(4096, 4u), tcu::UVec2(8192, 4u), tcu::UVec2(16384, 4u), tcu::UVec2(32768, 4u),

        // large pot x small npot
        tcu::UVec2(4096, 6u), tcu::UVec2(8192, 6u), tcu::UVec2(16384, 6u), tcu::UVec2(32768, 6u),

        // small pot x large pot
        tcu::UVec2(4u, 4096), tcu::UVec2(4u, 8192), tcu::UVec2(4u, 16384), tcu::UVec2(4u, 32768),

        // small npot x large pot
        tcu::UVec2(6u, 4096), tcu::UVec2(6u, 8192), tcu::UVec2(6u, 16384), tcu::UVec2(6u, 32768)};

    const VkImageLayout copySrcLayouts[] = {VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL};

    const VkImageLayout copyDstLayouts[] = {VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL};

    if (testGroupParams->allocationKind == ALLOCATION_KIND_DEDICATED)
    {
        for (int compatibleFormatsIndex = 0;
             compatibleFormatsIndex < DE_LENGTH_OF_ARRAY(dedicatedAllocationImageToImageFormatsToTest);
             compatibleFormatsIndex++)
            dedicatedAllocationImageToImageFormatsToTestSet.insert(
                dedicatedAllocationImageToImageFormatsToTest[compatibleFormatsIndex]);
    }

    // Image dimensions
    for (size_t dimensionNdx = 0; dimensionNdx < DE_LENGTH_OF_ARRAY(imageDimensions); dimensionNdx++)
    {
        CopyRegion copyRegion;
        CopyColorTestParams testParams;

        const VkExtent3D extent = {imageDimensions[dimensionNdx].x(), imageDimensions[dimensionNdx].y(), 1};

        const VkImageCopy testCopy = {
            defaultSourceLayer, // VkImageSubresourceLayers srcSubresource;
            {0, 0, 0},          // VkOffset3D srcOffset;
            defaultSourceLayer, // VkImageSubresourceLayers dstSubresource;
            {0, 0, 0},          // VkOffset3D dstOffset;
            extent,             // VkExtent3D extent;
        };

        testParams.params.src.image.tiling    = VK_IMAGE_TILING_OPTIMAL;
        testParams.params.src.image.imageType = VK_IMAGE_TYPE_2D;
        testParams.params.src.image.extent    = extent;

        testParams.params.dst.image.tiling    = VK_IMAGE_TILING_OPTIMAL;
        testParams.params.dst.image.imageType = VK_IMAGE_TYPE_2D;
        testParams.params.dst.image.extent    = extent;

        copyRegion.imageCopy               = testCopy;
        testParams.params.allocationKind   = testGroupParams->allocationKind;
        testParams.params.extensionFlags   = testGroupParams->extensionFlags;
        testParams.params.queueSelection   = testGroupParams->queueSelection;
        testParams.params.useSparseBinding = testGroupParams->useSparseBinding;

        testParams.params.regions.push_back(copyRegion);

        const std::string dimensionStr = "src" + de::toString(testParams.params.src.image.extent.width) + "x" +
                                         de::toString(testParams.params.src.image.extent.height) + "_dst" +
                                         de::toString(testParams.params.dst.image.extent.width) + "x" +
                                         de::toString(testParams.params.dst.image.extent.height);
        tcu::TestCaseGroup *imageSizeGroup = new tcu::TestCaseGroup(testCtx, dimensionStr.c_str());

        // Compatible formats for copying
        for (int compatibleFormatsIndex = 0; compatibleFormatsIndex < DE_LENGTH_OF_ARRAY(testFormats);
             compatibleFormatsIndex++)
        {
            const VkFormat *compatibleFormats = testFormats[compatibleFormatsIndex];

            testParams.compatibleFormats = compatibleFormats;

            // Source image format
            for (int srcFormatIndex = 0; srcFormatIndex < DE_LENGTH_OF_ARRAY(testFormats[compatibleFormatsIndex]);
                 srcFormatIndex++)
            {
                testParams.params.src.image.format = testParams.compatibleFormats[srcFormatIndex];

                if (!isSupportedByFramework(testParams.params.src.image.format) &&
                    !isCompressedFormat(testParams.params.src.image.format))
                    continue;

                tcu::TestCaseGroup *srcFormatGroup =
                    new tcu::TestCaseGroup(testCtx, getFormatCaseName(testParams.params.src.image.format).c_str());

                // Destination image format
                for (int dstFormatIndex = 0; dstFormatIndex < DE_LENGTH_OF_ARRAY(testFormats[compatibleFormatsIndex]);
                     dstFormatIndex++)
                {
                    testParams.params.dst.image.format = testParams.compatibleFormats[dstFormatIndex];

                    if (!isSupportedByFramework(testParams.params.dst.image.format) &&
                        !isCompressedFormat(testParams.params.dst.image.format))
                        continue;

                    if (!isAllowedImageToImageAllFormatsColorSrcFormatTests(testParams))
                        continue;

                    if (isCompressedFormat(testParams.params.src.image.format) &&
                        isCompressedFormat(testParams.params.dst.image.format))
                    {
                        if ((getBlockWidth(testParams.params.src.image.format) !=
                             getBlockWidth(testParams.params.dst.image.format)) ||
                            (getBlockHeight(testParams.params.src.image.format) !=
                             getBlockHeight(testParams.params.dst.image.format)))
                            continue;
                    }

                    tcu::TestCaseGroup *dstFormatGroup =
                        new tcu::TestCaseGroup(testCtx, getFormatCaseName(testParams.params.dst.image.format).c_str());

                    // Source/destionation image layouts
                    for (int srcLayoutNdx = 0u; srcLayoutNdx < DE_LENGTH_OF_ARRAY(copySrcLayouts); srcLayoutNdx++)
                    {
                        testParams.params.src.image.operationLayout = copySrcLayouts[srcLayoutNdx];

                        for (int dstLayoutNdx = 0u; dstLayoutNdx < DE_LENGTH_OF_ARRAY(copyDstLayouts); dstLayoutNdx++)
                        {
                            testParams.params.dst.image.operationLayout = copyDstLayouts[dstLayoutNdx];

                            const std::string testName =
                                getImageLayoutCaseName(testParams.params.src.image.operationLayout) + "_" +
                                getImageLayoutCaseName(testParams.params.dst.image.operationLayout);
                            const TestParams params = testParams.params;

                            dstFormatGroup->addChild(new CopyImageToImageTestCase(testCtx, testName, params));
                        }
                    }

                    srcFormatGroup->addChild(dstFormatGroup);
                }

                imageSizeGroup->addChild(srcFormatGroup);
            }
        }

        group->addChild(imageSizeGroup);
    }
}

void addImageToImageAllFormatsDepthStencilFormatsTests(tcu::TestCaseGroup *group, TestParams params)
{
    const VkImageLayout copySrcLayouts[] = {VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL};
    const VkImageLayout copyDstLayouts[] = {VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL};

    for (int srcLayoutNdx = 0u; srcLayoutNdx < DE_LENGTH_OF_ARRAY(copySrcLayouts); ++srcLayoutNdx)
    {
        params.src.image.operationLayout = copySrcLayouts[srcLayoutNdx];
        for (int dstLayoutNdx = 0u; dstLayoutNdx < DE_LENGTH_OF_ARRAY(copyDstLayouts); ++dstLayoutNdx)
        {
            params.dst.image.operationLayout = copyDstLayouts[dstLayoutNdx];

            const std::string testName = getImageLayoutCaseName(params.src.image.operationLayout) + "_" +
                                         getImageLayoutCaseName(params.dst.image.operationLayout);
            group->addChild(new CopyImageToImageTestCase(group->getTestContext(), testName, params));
        }
    }
}

void addImageToImageAllFormatsDepthStencilTests(tcu::TestCaseGroup *group, TestGroupParamsPtr testGroupParams)
{
    const VkFormat depthAndStencilFormats[] = {
        VK_FORMAT_D16_UNORM,         VK_FORMAT_X8_D24_UNORM_PACK32, VK_FORMAT_D32_SFLOAT,         VK_FORMAT_S8_UINT,
        VK_FORMAT_D16_UNORM_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT,   VK_FORMAT_D32_SFLOAT_S8_UINT,
    };

    // 1D to 1D tests.
    {
        de::MovePtr<tcu::TestCaseGroup> subGroup(new tcu::TestCaseGroup(group->getTestContext(), "1d_to_1d"));

        for (int compatibleFormatsIndex = 0; compatibleFormatsIndex < DE_LENGTH_OF_ARRAY(depthAndStencilFormats);
             ++compatibleFormatsIndex)
        {
            TestParams params;
            params.src.image.imageType = VK_IMAGE_TYPE_1D;
            params.dst.image.imageType = VK_IMAGE_TYPE_1D;
            params.src.image.extent    = default1dExtent;
            params.dst.image.extent    = default1dExtent;
            params.src.image.format    = depthAndStencilFormats[compatibleFormatsIndex];
            params.dst.image.format    = params.src.image.format;
            params.src.image.tiling    = VK_IMAGE_TILING_OPTIMAL;
            params.dst.image.tiling    = VK_IMAGE_TILING_OPTIMAL;
            params.allocationKind      = testGroupParams->allocationKind;
            params.extensionFlags      = testGroupParams->extensionFlags;
            params.queueSelection      = testGroupParams->queueSelection;
            params.useSparseBinding    = testGroupParams->useSparseBinding;

            bool hasDepth   = tcu::hasDepthComponent(mapVkFormat(params.src.image.format).order);
            bool hasStencil = tcu::hasStencilComponent(mapVkFormat(params.src.image.format).order);

            const VkImageSubresourceLayers defaultDepthSourceLayer   = {VK_IMAGE_ASPECT_DEPTH_BIT, 0u, 0u, 1u};
            const VkImageSubresourceLayers defaultStencilSourceLayer = {VK_IMAGE_ASPECT_STENCIL_BIT, 0u, 0u, 1u};

            for (int32_t i = defaultQuarterSize; i < defaultSize; i += defaultSize / 2)
            {
                CopyRegion copyRegion;
                const VkOffset3D srcOffset = {0, 0, 0};
                const VkOffset3D dstOffset = {i, 0, 0};
                const VkExtent3D extent    = {defaultQuarterSize, 1, 1};

                if (hasDepth)
                {
                    const VkImageCopy testCopy = {
                        defaultDepthSourceLayer, // VkImageSubresourceLayers srcSubresource;
                        srcOffset,               // VkOffset3D srcOffset;
                        defaultDepthSourceLayer, // VkImageSubresourceLayers dstSubresource;
                        dstOffset,               // VkOffset3D dstOffset;
                        extent,                  // VkExtent3D extent;
                    };

                    copyRegion.imageCopy = testCopy;
                    params.regions.push_back(copyRegion);
                }
                if (hasStencil)
                {
                    const VkImageCopy testCopy = {
                        defaultStencilSourceLayer, // VkImageSubresourceLayers srcSubresource;
                        srcOffset,                 // VkOffset3D srcOffset;
                        defaultStencilSourceLayer, // VkImageSubresourceLayers dstSubresource;
                        dstOffset,                 // VkOffset3D dstOffset;
                        extent,                    // VkExtent3D extent;
                    };

                    copyRegion.imageCopy = testCopy;
                    params.regions.push_back(copyRegion);
                }
            }

            const std::string testName =
                getFormatCaseName(params.src.image.format) + "_" + getFormatCaseName(params.dst.image.format);
            addTestGroup(subGroup.get(), testName, addImageToImageAllFormatsDepthStencilFormatsTests, params);

            if (hasDepth && hasStencil)
            {
                params.extensionFlags |= SEPARATE_DEPTH_STENCIL_LAYOUT;
                const std::string testName2 = getFormatCaseName(params.src.image.format) + "_" +
                                              getFormatCaseName(params.dst.image.format) + "_separate_layouts";
                addTestGroup(subGroup.get(), testName2, addImageToImageAllFormatsDepthStencilFormatsTests, params);
            }
        }

        group->addChild(subGroup.release());
    }

    // 1D to 2D tests.
    {
        de::MovePtr<tcu::TestCaseGroup> subGroup(new tcu::TestCaseGroup(group->getTestContext(), "1d_to_2d"));

        for (int compatibleFormatsIndex = 0; compatibleFormatsIndex < DE_LENGTH_OF_ARRAY(depthAndStencilFormats);
             ++compatibleFormatsIndex)
        {
            TestParams params;
            params.src.image.imageType = VK_IMAGE_TYPE_1D;
            params.dst.image.imageType = VK_IMAGE_TYPE_2D;
            params.src.image.extent    = default1dExtent;
            params.dst.image.extent    = defaultRootExtent;
            params.src.image.format    = depthAndStencilFormats[compatibleFormatsIndex];
            params.dst.image.format    = params.src.image.format;
            params.src.image.tiling    = VK_IMAGE_TILING_OPTIMAL;
            params.dst.image.tiling    = VK_IMAGE_TILING_OPTIMAL;
            params.allocationKind      = testGroupParams->allocationKind;
            params.extensionFlags      = testGroupParams->extensionFlags;
            params.queueSelection      = testGroupParams->queueSelection;
            params.useSparseBinding    = testGroupParams->useSparseBinding;
            params.extensionFlags |= MAINTENANCE_5;

            bool hasDepth   = tcu::hasDepthComponent(mapVkFormat(params.src.image.format).order);
            bool hasStencil = tcu::hasStencilComponent(mapVkFormat(params.src.image.format).order);

            const VkImageSubresourceLayers defaultDepthSourceLayer   = {VK_IMAGE_ASPECT_DEPTH_BIT, 0u, 0u, 1u};
            const VkImageSubresourceLayers defaultStencilSourceLayer = {VK_IMAGE_ASPECT_STENCIL_BIT, 0u, 0u, 1u};

            for (uint32_t i = 0; i < defaultRootSize; ++i)
            {
                CopyRegion copyRegion;
                const VkOffset3D srcOffset = {static_cast<int32_t>(i * defaultRootSize), 0, 0};
                const VkOffset3D dstOffset = {0, static_cast<int32_t>(i), 0};
                const VkExtent3D extent    = {defaultRootSize, 1, 1};

                if (hasDepth)
                {
                    const VkImageCopy testCopy = {
                        defaultDepthSourceLayer, // VkImageSubresourceLayers srcSubresource;
                        srcOffset,               // VkOffset3D srcOffset;
                        defaultDepthSourceLayer, // VkImageSubresourceLayers dstSubresource;
                        dstOffset,               // VkOffset3D dstOffset;
                        extent,                  // VkExtent3D extent;
                    };

                    copyRegion.imageCopy = testCopy;
                    params.regions.push_back(copyRegion);
                }
                if (hasStencil)
                {
                    const VkImageCopy testCopy = {
                        defaultStencilSourceLayer, // VkImageSubresourceLayers srcSubresource;
                        srcOffset,                 // VkOffset3D srcOffset;
                        defaultStencilSourceLayer, // VkImageSubresourceLayers dstSubresource;
                        dstOffset,                 // VkOffset3D dstOffset;
                        extent,                    // VkExtent3D extent;
                    };

                    copyRegion.imageCopy = testCopy;
                    params.regions.push_back(copyRegion);
                }
            }

            const std::string testName =
                getFormatCaseName(params.src.image.format) + "_" + getFormatCaseName(params.dst.image.format);
            addTestGroup(subGroup.get(), testName, addImageToImageAllFormatsDepthStencilFormatsTests, params);

            if (hasDepth && hasStencil)
            {
                params.extensionFlags |= SEPARATE_DEPTH_STENCIL_LAYOUT;
                const std::string testName2 = getFormatCaseName(params.src.image.format) + "_" +
                                              getFormatCaseName(params.dst.image.format) + "_separate_layouts";
                addTestGroup(subGroup.get(), testName2, addImageToImageAllFormatsDepthStencilFormatsTests, params);
            }
        }

        group->addChild(subGroup.release());
    }

    // 1D to 3D tests.
    {
        de::MovePtr<tcu::TestCaseGroup> subGroup(new tcu::TestCaseGroup(group->getTestContext(), "1d_to_3d"));

        for (int compatibleFormatsIndex = 0; compatibleFormatsIndex < DE_LENGTH_OF_ARRAY(depthAndStencilFormats);
             ++compatibleFormatsIndex)
        {
            TestParams params;
            params.src.image.imageType = VK_IMAGE_TYPE_1D;
            params.dst.image.imageType = VK_IMAGE_TYPE_3D;
            params.src.image.extent    = default1dExtent;
            params.dst.image.extent    = default3dSmallExtent;
            params.src.image.format    = depthAndStencilFormats[compatibleFormatsIndex];
            params.dst.image.format    = params.src.image.format;
            params.src.image.tiling    = VK_IMAGE_TILING_OPTIMAL;
            params.dst.image.tiling    = VK_IMAGE_TILING_OPTIMAL;
            params.allocationKind      = testGroupParams->allocationKind;
            params.extensionFlags      = testGroupParams->extensionFlags;
            params.queueSelection      = testGroupParams->queueSelection;
            params.useSparseBinding    = testGroupParams->useSparseBinding;
            params.extensionFlags |= MAINTENANCE_5;

            bool hasDepth   = tcu::hasDepthComponent(mapVkFormat(params.src.image.format).order);
            bool hasStencil = tcu::hasStencilComponent(mapVkFormat(params.src.image.format).order);

            const VkImageSubresourceLayers defaultDepthSourceLayer   = {VK_IMAGE_ASPECT_DEPTH_BIT, 0u, 0u, 1u};
            const VkImageSubresourceLayers defaultStencilSourceLayer = {VK_IMAGE_ASPECT_STENCIL_BIT, 0u, 0u, 1u};

            for (int32_t i = 0; i < defaultSixteenthSize; ++i)
            {
                for (int32_t j = 0; j < defaultSixteenthSize; ++j)
                {
                    CopyRegion copyRegion;
                    const VkOffset3D srcOffset = {i * defaultQuarterSize + j * defaultSixteenthSize, 0, 0};
                    const VkOffset3D dstOffset = {0, j, i};
                    const VkExtent3D extent    = {defaultSixteenthSize, 1, 1};

                    if (hasDepth)
                    {
                        const VkImageCopy testCopy = {
                            defaultDepthSourceLayer, // VkImageSubresourceLayers srcSubresource;
                            srcOffset,               // VkOffset3D srcOffset;
                            defaultDepthSourceLayer, // VkImageSubresourceLayers dstSubresource;
                            dstOffset,               // VkOffset3D dstOffset;
                            extent,                  // VkExtent3D extent;
                        };

                        copyRegion.imageCopy = testCopy;
                        params.regions.push_back(copyRegion);
                    }
                    if (hasStencil)
                    {
                        const VkImageCopy testCopy = {
                            defaultStencilSourceLayer, // VkImageSubresourceLayers srcSubresource;
                            srcOffset,                 // VkOffset3D srcOffset;
                            defaultStencilSourceLayer, // VkImageSubresourceLayers dstSubresource;
                            dstOffset,                 // VkOffset3D dstOffset;
                            extent,                    // VkExtent3D extent;
                        };

                        copyRegion.imageCopy = testCopy;
                        params.regions.push_back(copyRegion);
                    }
                }
            }

            const std::string testName =
                getFormatCaseName(params.src.image.format) + "_" + getFormatCaseName(params.dst.image.format);
            addTestGroup(subGroup.get(), testName, addImageToImageAllFormatsDepthStencilFormatsTests, params);

            if (hasDepth && hasStencil)
            {
                params.extensionFlags |= SEPARATE_DEPTH_STENCIL_LAYOUT;
                const std::string testName2 = getFormatCaseName(params.src.image.format) + "_" +
                                              getFormatCaseName(params.dst.image.format) + "_separate_layouts";
                addTestGroup(subGroup.get(), testName2, addImageToImageAllFormatsDepthStencilFormatsTests, params);
            }
        }

        group->addChild(subGroup.release());
    }

    // 2D to 1D tests.
    {
        de::MovePtr<tcu::TestCaseGroup> subGroup(new tcu::TestCaseGroup(group->getTestContext(), "2d_to_1d"));

        for (int compatibleFormatsIndex = 0; compatibleFormatsIndex < DE_LENGTH_OF_ARRAY(depthAndStencilFormats);
             ++compatibleFormatsIndex)
        {
            TestParams params;
            params.src.image.imageType = VK_IMAGE_TYPE_2D;
            params.dst.image.imageType = VK_IMAGE_TYPE_1D;
            params.src.image.extent    = defaultQuarterExtent;
            params.dst.image.extent    = default1dQuarterSquaredExtent;
            params.src.image.format    = depthAndStencilFormats[compatibleFormatsIndex];
            params.dst.image.format    = params.src.image.format;
            params.src.image.tiling    = VK_IMAGE_TILING_OPTIMAL;
            params.dst.image.tiling    = VK_IMAGE_TILING_OPTIMAL;
            params.allocationKind      = testGroupParams->allocationKind;
            params.extensionFlags      = testGroupParams->extensionFlags;
            params.queueSelection      = testGroupParams->queueSelection;
            params.useSparseBinding    = testGroupParams->useSparseBinding;
            params.extensionFlags |= MAINTENANCE_5;

            bool hasDepth   = tcu::hasDepthComponent(mapVkFormat(params.src.image.format).order);
            bool hasStencil = tcu::hasStencilComponent(mapVkFormat(params.src.image.format).order);

            const VkImageSubresourceLayers defaultDepthSourceLayer   = {VK_IMAGE_ASPECT_DEPTH_BIT, 0u, 0u, 1u};
            const VkImageSubresourceLayers defaultStencilSourceLayer = {VK_IMAGE_ASPECT_STENCIL_BIT, 0u, 0u, 1u};
            const VkImageSubresourceLayers defaultDSSourceLayer      = {
                VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT, 0u, 0u, 1u};

            for (int32_t i = 0; i < defaultQuarterSize; ++i)
            {
                CopyRegion copyRegion;
                const VkOffset3D srcOffset = {0, i, 0};
                const VkOffset3D dstOffset = {i * defaultQuarterSize, 0, 0};
                const VkExtent3D extent    = {defaultQuarterSize, 1, 1};

                if (hasDepth)
                {
                    const VkImageCopy testCopy = {
                        defaultDepthSourceLayer, // VkImageSubresourceLayers srcSubresource;
                        srcOffset,               // VkOffset3D srcOffset;
                        defaultDepthSourceLayer, // VkImageSubresourceLayers dstSubresource;
                        dstOffset,               // VkOffset3D dstOffset;
                        extent,                  // VkExtent3D extent;
                    };

                    copyRegion.imageCopy = testCopy;
                    params.regions.push_back(copyRegion);
                }
                if (hasStencil)
                {
                    const VkImageCopy testCopy = {
                        defaultStencilSourceLayer, // VkImageSubresourceLayers srcSubresource;
                        srcOffset,                 // VkOffset3D srcOffset;
                        defaultStencilSourceLayer, // VkImageSubresourceLayers dstSubresource;
                        dstOffset,                 // VkOffset3D dstOffset;
                        extent,                    // VkExtent3D extent;
                    };

                    copyRegion.imageCopy = testCopy;
                    params.regions.push_back(copyRegion);
                }
            }

            const std::string testName =
                getFormatCaseName(params.src.image.format) + "_" + getFormatCaseName(params.dst.image.format);
            addTestGroup(subGroup.get(), testName, addImageToImageAllFormatsDepthStencilFormatsTests, params);

            if (hasDepth && hasStencil)
            {
                params.extensionFlags |= SEPARATE_DEPTH_STENCIL_LAYOUT;
                const std::string testName2 = getFormatCaseName(params.src.image.format) + "_" +
                                              getFormatCaseName(params.dst.image.format) + "_separate_layouts";
                addTestGroup(subGroup.get(), testName2, addImageToImageAllFormatsDepthStencilFormatsTests, params);

                // DS Image copy
                {
                    params.extensionFlags &= ~SEPARATE_DEPTH_STENCIL_LAYOUT;
                    // Clear previous vkImageCopy elements
                    params.regions.clear();

                    for (int32_t i = 0; i < defaultQuarterSize; ++i)
                    {
                        CopyRegion copyRegion;
                        const VkOffset3D srcOffset = {0, i, 0};
                        const VkOffset3D dstOffset = {i * defaultQuarterSize, 0, 0};
                        const VkExtent3D extent    = {defaultQuarterSize, 1, 1};

                        const VkImageCopy testCopy = {
                            defaultDSSourceLayer, // VkImageSubresourceLayers srcSubresource;
                            srcOffset,            // VkOffset3D srcOffset;
                            defaultDSSourceLayer, // VkImageSubresourceLayers dstSubresource;
                            dstOffset,            // VkOffset3D dstOffset;
                            extent,               // VkExtent3D extent;
                        };

                        copyRegion.imageCopy = testCopy;
                        params.regions.push_back(copyRegion);
                    }

                    const std::string testName3 = getFormatCaseName(params.src.image.format) + "_" +
                                                  getFormatCaseName(params.dst.image.format) + "_depth_stencil_aspects";
                    addTestGroup(subGroup.get(), testName3, addImageToImageAllFormatsDepthStencilFormatsTests, params);
                }
            }
        }

        group->addChild(subGroup.release());
    }

    // 2D to 2D tests.
    {
        de::MovePtr<tcu::TestCaseGroup> subGroup(new tcu::TestCaseGroup(group->getTestContext(), "2d_to_2d"));

        for (int compatibleFormatsIndex = 0; compatibleFormatsIndex < DE_LENGTH_OF_ARRAY(depthAndStencilFormats);
             ++compatibleFormatsIndex)
        {
            TestParams params;
            params.src.image.imageType = VK_IMAGE_TYPE_2D;
            params.dst.image.imageType = VK_IMAGE_TYPE_2D;
            params.src.image.extent    = defaultExtent;
            params.dst.image.extent    = defaultExtent;
            params.src.image.format    = depthAndStencilFormats[compatibleFormatsIndex];
            params.dst.image.format    = params.src.image.format;
            params.src.image.tiling    = VK_IMAGE_TILING_OPTIMAL;
            params.dst.image.tiling    = VK_IMAGE_TILING_OPTIMAL;
            params.allocationKind      = testGroupParams->allocationKind;
            params.extensionFlags      = testGroupParams->extensionFlags;
            params.queueSelection      = testGroupParams->queueSelection;
            params.useSparseBinding    = testGroupParams->useSparseBinding;

            bool hasDepth   = tcu::hasDepthComponent(mapVkFormat(params.src.image.format).order);
            bool hasStencil = tcu::hasStencilComponent(mapVkFormat(params.src.image.format).order);

            const VkImageSubresourceLayers defaultDepthSourceLayer   = {VK_IMAGE_ASPECT_DEPTH_BIT, 0u, 0u, 1u};
            const VkImageSubresourceLayers defaultStencilSourceLayer = {VK_IMAGE_ASPECT_STENCIL_BIT, 0u, 0u, 1u};
            const VkImageSubresourceLayers defaultDSSourceLayer      = {
                VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT, 0u, 0u, 1u};

            for (int32_t i = 0; i < defaultSize; i += defaultQuarterSize)
            {
                CopyRegion copyRegion;
                const VkOffset3D srcOffset = {0, 0, 0};
                const VkOffset3D dstOffset = {i, defaultSize - i - defaultQuarterSize, 0};
                const VkExtent3D extent    = {defaultQuarterSize, defaultQuarterSize, 1};

                if (hasDepth)
                {
                    const VkImageCopy testCopy = {
                        defaultDepthSourceLayer, // VkImageSubresourceLayers srcSubresource;
                        srcOffset,               // VkOffset3D srcOffset;
                        defaultDepthSourceLayer, // VkImageSubresourceLayers dstSubresource;
                        dstOffset,               // VkOffset3D dstOffset;
                        extent,                  // VkExtent3D extent;
                    };

                    copyRegion.imageCopy = testCopy;
                    params.regions.push_back(copyRegion);
                }
                if (hasStencil)
                {
                    const VkImageCopy testCopy = {
                        defaultStencilSourceLayer, // VkImageSubresourceLayers srcSubresource;
                        srcOffset,                 // VkOffset3D srcOffset;
                        defaultStencilSourceLayer, // VkImageSubresourceLayers dstSubresource;
                        dstOffset,                 // VkOffset3D dstOffset;
                        extent,                    // VkExtent3D extent;
                    };

                    copyRegion.imageCopy = testCopy;
                    params.regions.push_back(copyRegion);
                }
            }

            const std::string testName =
                getFormatCaseName(params.src.image.format) + "_" + getFormatCaseName(params.dst.image.format);
            addTestGroup(subGroup.get(), testName, addImageToImageAllFormatsDepthStencilFormatsTests, params);

            if (hasDepth && hasStencil)
            {
                params.extensionFlags |= SEPARATE_DEPTH_STENCIL_LAYOUT;
                const std::string testName2 = getFormatCaseName(params.src.image.format) + "_" +
                                              getFormatCaseName(params.dst.image.format) + "_separate_layouts";
                addTestGroup(subGroup.get(), testName2, addImageToImageAllFormatsDepthStencilFormatsTests, params);

                // DS Image copy
                {
                    params.extensionFlags &= ~SEPARATE_DEPTH_STENCIL_LAYOUT;
                    // Clear previous vkImageCopy elements
                    params.regions.clear();

                    for (int32_t i = 0; i < defaultSize; i += defaultQuarterSize)
                    {
                        CopyRegion copyRegion;
                        const VkOffset3D srcOffset = {0, 0, 0};
                        const VkOffset3D dstOffset = {i, defaultSize - i - defaultQuarterSize, 0};
                        const VkExtent3D extent    = {defaultQuarterSize, defaultQuarterSize, 1};

                        const VkImageCopy testCopy = {
                            defaultDSSourceLayer, // VkImageSubresourceLayers srcSubresource;
                            srcOffset,            // VkOffset3D srcOffset;
                            defaultDSSourceLayer, // VkImageSubresourceLayers dstSubresource;
                            dstOffset,            // VkOffset3D dstOffset;
                            extent,               // VkExtent3D extent;
                        };

                        copyRegion.imageCopy = testCopy;
                        params.regions.push_back(copyRegion);
                    }

                    const std::string testName3 = getFormatCaseName(params.src.image.format) + "_" +
                                                  getFormatCaseName(params.dst.image.format) + "_depth_stencil_aspects";
                    addTestGroup(subGroup.get(), testName3, addImageToImageAllFormatsDepthStencilFormatsTests, params);
                }
            }
        }

        group->addChild(subGroup.release());
    }

    // 2D to 3D tests.
    {
        de::MovePtr<tcu::TestCaseGroup> subGroup(new tcu::TestCaseGroup(group->getTestContext(), "2d_to_3d"));

        for (int compatibleFormatsIndex = 0; compatibleFormatsIndex < DE_LENGTH_OF_ARRAY(depthAndStencilFormats);
             ++compatibleFormatsIndex)
        {
            TestParams params;
            params.src.image.imageType = VK_IMAGE_TYPE_2D;
            params.dst.image.imageType = VK_IMAGE_TYPE_3D;
            params.src.image.extent    = defaultExtent;
            params.dst.image.extent    = default3dSmallExtent;
            params.src.image.format    = depthAndStencilFormats[compatibleFormatsIndex];
            params.dst.image.format    = params.src.image.format;
            params.src.image.tiling    = VK_IMAGE_TILING_OPTIMAL;
            params.dst.image.tiling    = VK_IMAGE_TILING_OPTIMAL;
            params.allocationKind      = testGroupParams->allocationKind;
            params.extensionFlags      = testGroupParams->extensionFlags;
            params.queueSelection      = testGroupParams->queueSelection;
            params.useSparseBinding    = testGroupParams->useSparseBinding;
            params.extensionFlags |= MAINTENANCE_1;

            bool hasDepth   = tcu::hasDepthComponent(mapVkFormat(params.src.image.format).order);
            bool hasStencil = tcu::hasStencilComponent(mapVkFormat(params.src.image.format).order);

            const VkImageSubresourceLayers defaultDepthSourceLayer   = {VK_IMAGE_ASPECT_DEPTH_BIT, 0u, 0u, 1u};
            const VkImageSubresourceLayers defaultStencilSourceLayer = {VK_IMAGE_ASPECT_STENCIL_BIT, 0u, 0u, 1u};
            const VkImageSubresourceLayers defaultDSSourceLayer      = {
                VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT, 0u, 0u, 1u};

            for (int32_t i = 0; i < defaultSixteenthSize; ++i)
            {
                CopyRegion copyRegion;
                const VkOffset3D srcOffset = {i * defaultSixteenthSize, i % defaultSixteenthSize, 0};
                const VkOffset3D dstOffset = {0, 0, i};
                const VkExtent3D extent    = {defaultSixteenthSize, defaultSixteenthSize, 1};

                if (hasDepth)
                {
                    const VkImageCopy testCopy = {
                        defaultDepthSourceLayer, // VkImageSubresourceLayers srcSubresource;
                        srcOffset,               // VkOffset3D srcOffset;
                        defaultDepthSourceLayer, // VkImageSubresourceLayers dstSubresource;
                        dstOffset,               // VkOffset3D dstOffset;
                        extent,                  // VkExtent3D extent;
                    };

                    copyRegion.imageCopy = testCopy;
                    params.regions.push_back(copyRegion);
                }
                if (hasStencil)
                {
                    const VkImageCopy testCopy = {
                        defaultStencilSourceLayer, // VkImageSubresourceLayers srcSubresource;
                        srcOffset,                 // VkOffset3D srcOffset;
                        defaultStencilSourceLayer, // VkImageSubresourceLayers dstSubresource;
                        dstOffset,                 // VkOffset3D dstOffset;
                        extent,                    // VkExtent3D extent;
                    };

                    copyRegion.imageCopy = testCopy;
                    params.regions.push_back(copyRegion);
                }
            }

            const std::string testName =
                getFormatCaseName(params.src.image.format) + "_" + getFormatCaseName(params.dst.image.format);
            addTestGroup(subGroup.get(), testName, addImageToImageAllFormatsDepthStencilFormatsTests, params);

            if (hasDepth && hasStencil)
            {
                params.extensionFlags |= SEPARATE_DEPTH_STENCIL_LAYOUT;
                const std::string testName2 = getFormatCaseName(params.src.image.format) + "_" +
                                              getFormatCaseName(params.dst.image.format) + "_separate_layouts";
                addTestGroup(subGroup.get(), testName2, addImageToImageAllFormatsDepthStencilFormatsTests, params);

                // DS Image copy
                {
                    params.extensionFlags &= ~SEPARATE_DEPTH_STENCIL_LAYOUT;
                    // Clear previous vkImageCopy elements
                    params.regions.clear();

                    for (int32_t i = 0; i < defaultSixteenthSize; ++i)
                    {
                        CopyRegion copyRegion;
                        const VkOffset3D srcOffset = {i * defaultSixteenthSize, i % defaultSixteenthSize, 0};
                        const VkOffset3D dstOffset = {0, 0, i};
                        const VkExtent3D extent    = {defaultSixteenthSize, defaultSixteenthSize, 1};

                        const VkImageCopy testCopy = {
                            defaultDSSourceLayer, // VkImageSubresourceLayers srcSubresource;
                            srcOffset,            // VkOffset3D srcOffset;
                            defaultDSSourceLayer, // VkImageSubresourceLayers dstSubresource;
                            dstOffset,            // VkOffset3D dstOffset;
                            extent,               // VkExtent3D extent;
                        };

                        copyRegion.imageCopy = testCopy;
                        params.regions.push_back(copyRegion);
                    }

                    const std::string testName3 = getFormatCaseName(params.src.image.format) + "_" +
                                                  getFormatCaseName(params.dst.image.format) + "_depth_stencil_aspects";
                    addTestGroup(subGroup.get(), testName3, addImageToImageAllFormatsDepthStencilFormatsTests, params);
                }
            }
        }

        group->addChild(subGroup.release());
    }

    // 3D to 1D tests.
    {
        de::MovePtr<tcu::TestCaseGroup> subGroup(new tcu::TestCaseGroup(group->getTestContext(), "3d_to_1d"));

        for (int compatibleFormatsIndex = 0; compatibleFormatsIndex < DE_LENGTH_OF_ARRAY(depthAndStencilFormats);
             ++compatibleFormatsIndex)
        {
            TestParams params;
            params.src.image.imageType = VK_IMAGE_TYPE_3D;
            params.dst.image.imageType = VK_IMAGE_TYPE_1D;
            params.src.image.extent    = default3dSmallExtent;
            params.dst.image.extent    = default1dExtent;
            params.src.image.format    = depthAndStencilFormats[compatibleFormatsIndex];
            params.dst.image.format    = params.src.image.format;
            params.src.image.tiling    = VK_IMAGE_TILING_OPTIMAL;
            params.dst.image.tiling    = VK_IMAGE_TILING_OPTIMAL;
            params.allocationKind      = testGroupParams->allocationKind;
            params.extensionFlags      = testGroupParams->extensionFlags;
            params.queueSelection      = testGroupParams->queueSelection;
            params.useSparseBinding    = testGroupParams->useSparseBinding;
            params.extensionFlags |= MAINTENANCE_5;

            bool hasDepth   = tcu::hasDepthComponent(mapVkFormat(params.src.image.format).order);
            bool hasStencil = tcu::hasStencilComponent(mapVkFormat(params.src.image.format).order);

            const VkImageSubresourceLayers defaultDepthSourceLayer   = {VK_IMAGE_ASPECT_DEPTH_BIT, 0u, 0u, 1u};
            const VkImageSubresourceLayers defaultStencilSourceLayer = {VK_IMAGE_ASPECT_STENCIL_BIT, 0u, 0u, 1u};

            for (int32_t i = 0; i < defaultSixteenthSize; ++i)
            {
                for (int32_t j = 0; j < defaultSixteenthSize; ++j)
                {
                    CopyRegion copyRegion;
                    const VkOffset3D srcOffset = {0, j % defaultSixteenthSize, i % defaultSixteenthSize};
                    const VkOffset3D dstOffset = {j * defaultSixteenthSize + i * defaultQuarterSize, 0, 0};
                    const VkExtent3D extent    = {defaultSixteenthSize, 1, 1};

                    if (hasDepth)
                    {
                        const VkImageCopy testCopy = {
                            defaultDepthSourceLayer, // VkImageSubresourceLayers srcSubresource;
                            srcOffset,               // VkOffset3D srcOffset;
                            defaultDepthSourceLayer, // VkImageSubresourceLayers dstSubresource;
                            dstOffset,               // VkOffset3D dstOffset;
                            extent,                  // VkExtent3D extent;
                        };

                        copyRegion.imageCopy = testCopy;
                        params.regions.push_back(copyRegion);
                    }
                    if (hasStencil)
                    {
                        const VkImageCopy testCopy = {
                            defaultStencilSourceLayer, // VkImageSubresourceLayers srcSubresource;
                            srcOffset,                 // VkOffset3D srcOffset;
                            defaultStencilSourceLayer, // VkImageSubresourceLayers dstSubresource;
                            dstOffset,                 // VkOffset3D dstOffset;
                            extent,                    // VkExtent3D extent;
                        };

                        copyRegion.imageCopy = testCopy;
                        params.regions.push_back(copyRegion);
                    }
                }
            }

            const std::string testName =
                getFormatCaseName(params.src.image.format) + "_" + getFormatCaseName(params.dst.image.format);
            addTestGroup(subGroup.get(), testName, addImageToImageAllFormatsDepthStencilFormatsTests, params);

            if (hasDepth && hasStencil)
            {
                params.extensionFlags |= SEPARATE_DEPTH_STENCIL_LAYOUT;
                const std::string testName2 = getFormatCaseName(params.src.image.format) + "_" +
                                              getFormatCaseName(params.dst.image.format) + "_separate_layouts";
                addTestGroup(subGroup.get(), testName2, addImageToImageAllFormatsDepthStencilFormatsTests, params);
            }
        }

        group->addChild(subGroup.release());
    }

    // 3D to 2D tests.
    {
        de::MovePtr<tcu::TestCaseGroup> subGroup(new tcu::TestCaseGroup(group->getTestContext(), "3d_to_2d"));

        for (int compatibleFormatsIndex = 0; compatibleFormatsIndex < DE_LENGTH_OF_ARRAY(depthAndStencilFormats);
             ++compatibleFormatsIndex)
        {
            TestParams params;
            params.src.image.imageType = VK_IMAGE_TYPE_3D;
            params.dst.image.imageType = VK_IMAGE_TYPE_2D;
            params.src.image.extent    = default3dExtent;
            params.dst.image.extent    = defaultExtent;
            params.src.image.format    = depthAndStencilFormats[compatibleFormatsIndex];
            params.dst.image.format    = params.src.image.format;
            params.src.image.tiling    = VK_IMAGE_TILING_OPTIMAL;
            params.dst.image.tiling    = VK_IMAGE_TILING_OPTIMAL;
            params.allocationKind      = testGroupParams->allocationKind;
            params.extensionFlags      = testGroupParams->extensionFlags;
            params.queueSelection      = testGroupParams->queueSelection;
            params.useSparseBinding    = testGroupParams->useSparseBinding;
            params.extensionFlags |= MAINTENANCE_1;

            bool hasDepth   = tcu::hasDepthComponent(mapVkFormat(params.src.image.format).order);
            bool hasStencil = tcu::hasStencilComponent(mapVkFormat(params.src.image.format).order);

            const VkImageSubresourceLayers defaultDepthSourceLayer   = {VK_IMAGE_ASPECT_DEPTH_BIT, 0u, 0u, 1u};
            const VkImageSubresourceLayers defaultStencilSourceLayer = {VK_IMAGE_ASPECT_STENCIL_BIT, 0u, 0u, 1u};

            for (int32_t i = 0; i < defaultSixteenthSize; ++i)
            {
                for (int32_t j = 0; j < defaultSixteenthSize; ++j)
                {
                    CopyRegion copyRegion;
                    const VkOffset3D srcOffset = {0, 0, i % defaultSixteenthSize + j};
                    const VkOffset3D dstOffset = {j * defaultQuarterSize, i * defaultQuarterSize, 0};
                    const VkExtent3D extent    = {defaultQuarterSize, defaultQuarterSize, 1};

                    if (hasDepth)
                    {
                        const VkImageCopy testCopy = {
                            defaultDepthSourceLayer, // VkImageSubresourceLayers srcSubresource;
                            srcOffset,               // VkOffset3D srcOffset;
                            defaultDepthSourceLayer, // VkImageSubresourceLayers dstSubresource;
                            dstOffset,               // VkOffset3D dstOffset;
                            extent,                  // VkExtent3D extent;
                        };

                        copyRegion.imageCopy = testCopy;
                        params.regions.push_back(copyRegion);
                    }
                    if (hasStencil)
                    {
                        const VkImageCopy testCopy = {
                            defaultStencilSourceLayer, // VkImageSubresourceLayers srcSubresource;
                            srcOffset,                 // VkOffset3D srcOffset;
                            defaultStencilSourceLayer, // VkImageSubresourceLayers dstSubresource;
                            dstOffset,                 // VkOffset3D dstOffset;
                            extent,                    // VkExtent3D extent;
                        };

                        copyRegion.imageCopy = testCopy;
                        params.regions.push_back(copyRegion);
                    }
                }
            }

            const std::string testName =
                getFormatCaseName(params.src.image.format) + "_" + getFormatCaseName(params.dst.image.format);
            addTestGroup(subGroup.get(), testName, addImageToImageAllFormatsDepthStencilFormatsTests, params);

            if (hasDepth && hasStencil)
            {
                params.extensionFlags |= SEPARATE_DEPTH_STENCIL_LAYOUT;
                const std::string testName2 = getFormatCaseName(params.src.image.format) + "_" +
                                              getFormatCaseName(params.dst.image.format) + "_separate_layouts";
                addTestGroup(subGroup.get(), testName2, addImageToImageAllFormatsDepthStencilFormatsTests, params);
            }
        }

        group->addChild(subGroup.release());
    }

    // 3D tests. Note we use smaller dimensions here for performance reasons.
    {
        de::MovePtr<tcu::TestCaseGroup> subGroup(new tcu::TestCaseGroup(group->getTestContext(), "3d_to_3d"));

        for (int compatibleFormatsIndex = 0; compatibleFormatsIndex < DE_LENGTH_OF_ARRAY(depthAndStencilFormats);
             ++compatibleFormatsIndex)
        {
            TestParams params;
            params.src.image.imageType = VK_IMAGE_TYPE_3D;
            params.dst.image.imageType = VK_IMAGE_TYPE_3D;
            params.src.image.extent    = default3dExtent;
            params.dst.image.extent    = default3dExtent;
            params.src.image.format    = depthAndStencilFormats[compatibleFormatsIndex];
            params.dst.image.format    = params.src.image.format;
            params.src.image.tiling    = VK_IMAGE_TILING_OPTIMAL;
            params.dst.image.tiling    = VK_IMAGE_TILING_OPTIMAL;
            params.allocationKind      = testGroupParams->allocationKind;
            params.extensionFlags      = testGroupParams->extensionFlags;
            params.queueSelection      = testGroupParams->queueSelection;
            params.useSparseBinding    = testGroupParams->useSparseBinding;

            bool hasDepth   = tcu::hasDepthComponent(mapVkFormat(params.src.image.format).order);
            bool hasStencil = tcu::hasStencilComponent(mapVkFormat(params.src.image.format).order);

            const VkImageSubresourceLayers defaultDepthSourceLayer   = {VK_IMAGE_ASPECT_DEPTH_BIT, 0u, 0u, 1u};
            const VkImageSubresourceLayers defaultStencilSourceLayer = {VK_IMAGE_ASPECT_STENCIL_BIT, 0u, 0u, 1u};

            for (int32_t i = 0; i < defaultQuarterSize; i += defaultSixteenthSize)
            {
                CopyRegion copyRegion;
                const VkOffset3D srcOffset = {0, 0, 0};
                const VkOffset3D dstOffset = {i, defaultQuarterSize - i - defaultSixteenthSize, i};
                const VkExtent3D extent    = {defaultSixteenthSize, defaultSixteenthSize, defaultSixteenthSize};

                if (hasDepth)
                {
                    const VkImageCopy testCopy = {
                        defaultDepthSourceLayer, // VkImageSubresourceLayers srcSubresource;
                        srcOffset,               // VkOffset3D srcOffset;
                        defaultDepthSourceLayer, // VkImageSubresourceLayers dstSubresource;
                        dstOffset,               // VkOffset3D dstOffset;
                        extent,                  // VkExtent3D extent;
                    };

                    copyRegion.imageCopy = testCopy;
                    params.regions.push_back(copyRegion);
                }
                if (hasStencil)
                {
                    const VkImageCopy testCopy = {
                        defaultStencilSourceLayer, // VkImageSubresourceLayers srcSubresource;
                        srcOffset,                 // VkOffset3D srcOffset;
                        defaultStencilSourceLayer, // VkImageSubresourceLayers dstSubresource;
                        dstOffset,                 // VkOffset3D dstOffset;
                        extent,                    // VkExtent3D extent;
                    };

                    copyRegion.imageCopy = testCopy;
                    params.regions.push_back(copyRegion);
                }
            }

            const std::string testName =
                getFormatCaseName(params.src.image.format) + "_" + getFormatCaseName(params.dst.image.format);
            addTestGroup(subGroup.get(), testName, addImageToImageAllFormatsDepthStencilFormatsTests, params);

            if (hasDepth && hasStencil)
            {
                params.extensionFlags |= SEPARATE_DEPTH_STENCIL_LAYOUT;
                const std::string testName2 = getFormatCaseName(params.src.image.format) + "_" +
                                              getFormatCaseName(params.dst.image.format) + "_separate_layouts";
                addTestGroup(subGroup.get(), testName2, addImageToImageAllFormatsDepthStencilFormatsTests, params);
            }
        }

        group->addChild(subGroup.release());
    }
}

void addImageToImageAllFormatsTests(tcu::TestCaseGroup *group, TestGroupParamsPtr testGroupParams)
{
    addTestGroup(group, "color", addImageToImageAllFormatsColorTests, testGroupParams);
    if (testGroupParams->queueSelection == QueueSelectionOptions::Universal)
        addTestGroup(group, "depth_stencil", addImageToImageAllFormatsDepthStencilTests, testGroupParams);
}

void addImageToImage3dImagesTests(tcu::TestCaseGroup *group, TestGroupParamsPtr testGroupParams)
{
    tcu::TestContext &testCtx = group->getTestContext();

    {
        TestParams params3DTo2D;
        const uint32_t slicesLayers            = 16u;
        params3DTo2D.src.image.imageType       = VK_IMAGE_TYPE_3D;
        params3DTo2D.src.image.format          = VK_FORMAT_R8G8B8A8_UINT;
        params3DTo2D.src.image.extent          = defaultHalfExtent;
        params3DTo2D.src.image.extent.depth    = slicesLayers;
        params3DTo2D.src.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
        params3DTo2D.src.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        params3DTo2D.dst.image.imageType       = VK_IMAGE_TYPE_2D;
        params3DTo2D.dst.image.format          = VK_FORMAT_R8G8B8A8_UINT;
        params3DTo2D.dst.image.extent          = defaultHalfExtent;
        params3DTo2D.dst.image.extent.depth    = slicesLayers;
        params3DTo2D.dst.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
        params3DTo2D.dst.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        params3DTo2D.allocationKind            = testGroupParams->allocationKind;
        params3DTo2D.extensionFlags            = testGroupParams->extensionFlags;
        params3DTo2D.queueSelection            = testGroupParams->queueSelection;

        for (uint32_t slicesLayersNdx = 0; slicesLayersNdx < slicesLayers; ++slicesLayersNdx)
        {
            const VkImageSubresourceLayers sourceLayer = {
                VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
                0u,                        // uint32_t mipLevel;
                0u,                        // uint32_t baseArrayLayer;
                1u                         // uint32_t layerCount;
            };

            const VkImageSubresourceLayers destinationLayer = {
                VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
                0u,                        // uint32_t mipLevel;
                slicesLayersNdx,           // uint32_t baseArrayLayer;
                1u                         // uint32_t layerCount;
            };

            const VkImageCopy testCopy = {
                sourceLayer,                      // VkImageSubresourceLayers srcSubresource;
                {0, 0, (int32_t)slicesLayersNdx}, // VkOffset3D srcOffset;
                destinationLayer,                 // VkImageSubresourceLayers dstSubresource;
                {0, 0, 0},                        // VkOffset3D dstOffset;
                defaultHalfExtent,                // VkExtent3D extent;
            };

            CopyRegion imageCopy;
            imageCopy.imageCopy = testCopy;

            params3DTo2D.regions.push_back(imageCopy);
        }
        group->addChild(new CopyImageToImageTestCase(testCtx, "3d_to_2d_by_slices", params3DTo2D));
    }

    {
        TestParams params2DTo3D;
        const uint32_t slicesLayers            = 16u;
        params2DTo3D.src.image.imageType       = VK_IMAGE_TYPE_2D;
        params2DTo3D.src.image.format          = VK_FORMAT_R8G8B8A8_UINT;
        params2DTo3D.src.image.extent          = defaultHalfExtent;
        params2DTo3D.src.image.extent.depth    = slicesLayers;
        params2DTo3D.src.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
        params2DTo3D.src.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        params2DTo3D.dst.image.imageType       = VK_IMAGE_TYPE_3D;
        params2DTo3D.dst.image.format          = VK_FORMAT_R8G8B8A8_UINT;
        params2DTo3D.dst.image.extent          = defaultHalfExtent;
        params2DTo3D.dst.image.extent.depth    = slicesLayers;
        params2DTo3D.dst.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
        params2DTo3D.dst.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        params2DTo3D.allocationKind            = testGroupParams->allocationKind;
        params2DTo3D.extensionFlags            = testGroupParams->extensionFlags;
        params2DTo3D.queueSelection            = testGroupParams->queueSelection;

        for (uint32_t slicesLayersNdx = 0; slicesLayersNdx < slicesLayers; ++slicesLayersNdx)
        {
            const VkImageSubresourceLayers sourceLayer = {
                VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
                0u,                        // uint32_t mipLevel;
                slicesLayersNdx,           // uint32_t baseArrayLayer;
                1u                         // uint32_t layerCount;
            };

            const VkImageSubresourceLayers destinationLayer = {
                VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
                0u,                        // uint32_t mipLevel;
                0u,                        // uint32_t baseArrayLayer;
                1u                         // uint32_t layerCount;
            };

            const VkImageCopy testCopy = {
                sourceLayer,                      // VkImageSubresourceLayers srcSubresource;
                {0, 0, 0},                        // VkOffset3D srcOffset;
                destinationLayer,                 // VkImageSubresourceLayers dstSubresource;
                {0, 0, (int32_t)slicesLayersNdx}, // VkOffset3D dstOffset;
                defaultHalfExtent,                // VkExtent3D extent;
            };

            CopyRegion imageCopy;
            imageCopy.imageCopy = testCopy;

            params2DTo3D.regions.push_back(imageCopy);
        }

        group->addChild(new CopyImageToImageTestCase(testCtx, "2d_to_3d_by_layers", params2DTo3D));
    }

    {
        TestParams params3DTo2D;
        const uint32_t slicesLayers            = 16u;
        params3DTo2D.src.image.imageType       = VK_IMAGE_TYPE_3D;
        params3DTo2D.src.image.format          = VK_FORMAT_R8G8B8A8_UINT;
        params3DTo2D.src.image.extent          = defaultHalfExtent;
        params3DTo2D.src.image.extent.depth    = slicesLayers;
        params3DTo2D.src.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
        params3DTo2D.src.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        params3DTo2D.dst.image.imageType       = VK_IMAGE_TYPE_2D;
        params3DTo2D.dst.image.format          = VK_FORMAT_R8G8B8A8_UINT;
        params3DTo2D.dst.image.extent          = defaultHalfExtent;
        params3DTo2D.dst.image.extent.depth    = slicesLayers;
        params3DTo2D.dst.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
        params3DTo2D.dst.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        params3DTo2D.allocationKind            = testGroupParams->allocationKind;
        params3DTo2D.extensionFlags            = testGroupParams->extensionFlags;
        params3DTo2D.queueSelection            = testGroupParams->queueSelection;

        {
            const VkImageSubresourceLayers sourceLayer = {
                VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
                0u,                        // uint32_t mipLevel;
                0u,                        // uint32_t baseArrayLayer;
                1u                         // uint32_t layerCount;
            };

            const VkImageSubresourceLayers destinationLayer = {
                VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
                0u,                        // uint32_t mipLevel;
                0,                         // uint32_t baseArrayLayer;
                slicesLayers               // uint32_t layerCount;
            };

            const VkImageCopy testCopy = {
                sourceLayer,                  // VkImageSubresourceLayers srcSubresource;
                {0, 0, 0},                    // VkOffset3D srcOffset;
                destinationLayer,             // VkImageSubresourceLayers dstSubresource;
                {0, 0, 0},                    // VkOffset3D dstOffset;
                params3DTo2D.src.image.extent // VkExtent3D extent;
            };

            CopyRegion imageCopy;
            imageCopy.imageCopy = testCopy;

            params3DTo2D.regions.push_back(imageCopy);
        }
        group->addChild(new CopyImageToImageTestCase(testCtx, "3d_to_2d_whole", params3DTo2D));
    }

    {
        TestParams params2DTo3D;
        const uint32_t slicesLayers            = 16u;
        params2DTo3D.src.image.imageType       = VK_IMAGE_TYPE_2D;
        params2DTo3D.src.image.format          = VK_FORMAT_R8G8B8A8_UINT;
        params2DTo3D.src.image.extent          = defaultHalfExtent;
        params2DTo3D.src.image.extent.depth    = slicesLayers;
        params2DTo3D.src.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
        params2DTo3D.src.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        params2DTo3D.dst.image.imageType       = VK_IMAGE_TYPE_3D;
        params2DTo3D.dst.image.format          = VK_FORMAT_R8G8B8A8_UINT;
        params2DTo3D.dst.image.extent          = defaultHalfExtent;
        params2DTo3D.dst.image.extent.depth    = slicesLayers;
        params2DTo3D.dst.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
        params2DTo3D.dst.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        params2DTo3D.allocationKind            = testGroupParams->allocationKind;
        params2DTo3D.extensionFlags            = testGroupParams->extensionFlags;
        params2DTo3D.queueSelection            = testGroupParams->queueSelection;

        {
            const VkImageSubresourceLayers sourceLayer = {
                VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
                0u,                        // uint32_t mipLevel;
                0u,                        // uint32_t baseArrayLayer;
                slicesLayers               // uint32_t layerCount;
            };

            const VkImageSubresourceLayers destinationLayer = {
                VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
                0u,                        // uint32_t mipLevel;
                0u,                        // uint32_t baseArrayLayer;
                1u                         // uint32_t layerCount;
            };

            const VkImageCopy testCopy = {
                sourceLayer,                   // VkImageSubresourceLayers srcSubresource;
                {0, 0, 0},                     // VkOffset3D srcOffset;
                destinationLayer,              // VkImageSubresourceLayers dstSubresource;
                {0, 0, 0},                     // VkOffset3D dstOffset;
                params2DTo3D.src.image.extent, // VkExtent3D extent;
            };

            CopyRegion imageCopy;
            imageCopy.imageCopy = testCopy;

            params2DTo3D.regions.push_back(imageCopy);
        }

        group->addChild(new CopyImageToImageTestCase(testCtx, "2d_to_3d_whole", params2DTo3D));
    }

    {
        TestParams params3DTo2D;
        const uint32_t slicesLayers            = 16u;
        params3DTo2D.src.image.imageType       = VK_IMAGE_TYPE_3D;
        params3DTo2D.src.image.format          = VK_FORMAT_R8G8B8A8_UINT;
        params3DTo2D.src.image.extent          = defaultHalfExtent;
        params3DTo2D.src.image.extent.depth    = slicesLayers;
        params3DTo2D.src.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
        params3DTo2D.src.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        params3DTo2D.dst.image.imageType       = VK_IMAGE_TYPE_2D;
        params3DTo2D.dst.image.format          = VK_FORMAT_R8G8B8A8_UINT;
        params3DTo2D.dst.image.extent          = defaultHalfExtent;
        params3DTo2D.dst.image.extent.depth    = slicesLayers;
        params3DTo2D.dst.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
        params3DTo2D.dst.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        params3DTo2D.allocationKind            = testGroupParams->allocationKind;
        params3DTo2D.extensionFlags            = testGroupParams->extensionFlags;
        params3DTo2D.queueSelection            = testGroupParams->queueSelection;

        const uint32_t regionWidth  = defaultHalfExtent.width / slicesLayers - 1;
        const uint32_t regionHeight = defaultHalfExtent.height / slicesLayers - 1;

        for (uint32_t slicesLayersNdx = 0; slicesLayersNdx < slicesLayers; ++slicesLayersNdx)
        {
            const VkImageSubresourceLayers sourceLayer = {
                VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
                0u,                        // uint32_t mipLevel;
                0u,                        // uint32_t baseArrayLayer;
                1u                         // uint32_t layerCount;
            };

            const VkImageSubresourceLayers destinationLayer = {
                VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
                0u,                        // uint32_t mipLevel;
                slicesLayersNdx,           // uint32_t baseArrayLayer;
                1u                         // uint32_t layerCount;
            };

            const VkImageCopy testCopy = {
                sourceLayer, // VkImageSubresourceLayers srcSubresource;
                {0, (int32_t)(regionHeight * slicesLayersNdx), (int32_t)slicesLayersNdx}, // VkOffset3D srcOffset;
                destinationLayer,                                 // VkImageSubresourceLayers dstSubresource;
                {(int32_t)(regionWidth * slicesLayersNdx), 0, 0}, // VkOffset3D dstOffset;
                {(defaultHalfExtent.width - regionWidth * slicesLayersNdx),
                 (defaultHalfExtent.height - regionHeight * slicesLayersNdx), 1} // VkExtent3D extent;
            };

            CopyRegion imageCopy;
            imageCopy.imageCopy = testCopy;
            params3DTo2D.regions.push_back(imageCopy);
        }
        group->addChild(new CopyImageToImageTestCase(testCtx, "3d_to_2d_regions", params3DTo2D));
    }

    {
        TestParams params2DTo3D;
        const uint32_t slicesLayers            = 16u;
        params2DTo3D.src.image.imageType       = VK_IMAGE_TYPE_2D;
        params2DTo3D.src.image.format          = VK_FORMAT_R8G8B8A8_UINT;
        params2DTo3D.src.image.extent          = defaultHalfExtent;
        params2DTo3D.src.image.extent.depth    = slicesLayers;
        params2DTo3D.src.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
        params2DTo3D.src.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        params2DTo3D.dst.image.imageType       = VK_IMAGE_TYPE_3D;
        params2DTo3D.dst.image.format          = VK_FORMAT_R8G8B8A8_UINT;
        params2DTo3D.dst.image.extent          = defaultHalfExtent;
        params2DTo3D.dst.image.extent.depth    = slicesLayers;
        params2DTo3D.dst.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
        params2DTo3D.dst.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        params2DTo3D.allocationKind            = testGroupParams->allocationKind;
        params2DTo3D.extensionFlags            = testGroupParams->extensionFlags;
        params2DTo3D.queueSelection            = testGroupParams->queueSelection;

        const uint32_t regionWidth  = defaultHalfExtent.width / slicesLayers - 1;
        const uint32_t regionHeight = defaultHalfExtent.height / slicesLayers - 1;

        for (uint32_t slicesLayersNdx = 0; slicesLayersNdx < slicesLayers; ++slicesLayersNdx)
        {
            const VkImageSubresourceLayers sourceLayer = {
                VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
                0u,                        // uint32_t mipLevel;
                slicesLayersNdx,           // uint32_t baseArrayLayer;
                1u                         // uint32_t layerCount;
            };

            const VkImageSubresourceLayers destinationLayer = {
                VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
                0u,                        // uint32_t mipLevel;
                0u,                        // uint32_t baseArrayLayer;
                1u                         // uint32_t layerCount;
            };

            const VkImageCopy testCopy = {
                sourceLayer,                                      // VkImageSubresourceLayers srcSubresource;
                {(int32_t)(regionWidth * slicesLayersNdx), 0, 0}, // VkOffset3D srcOffset;
                destinationLayer,                                 // VkImageSubresourceLayers dstSubresource;
                {0, (int32_t)(regionHeight * slicesLayersNdx), (int32_t)(slicesLayersNdx)}, // VkOffset3D dstOffset;
                {defaultHalfExtent.width - regionWidth * slicesLayersNdx,
                 defaultHalfExtent.height - regionHeight * slicesLayersNdx, 1} // VkExtent3D extent;
            };

            CopyRegion imageCopy;
            imageCopy.imageCopy = testCopy;

            params2DTo3D.regions.push_back(imageCopy);
        }

        group->addChild(new CopyImageToImageTestCase(testCtx, "2d_to_3d_regions", params2DTo3D));
    }
}

void addImageToImageCubeTests(tcu::TestCaseGroup *group, TestGroupParamsPtr testGroupParams)
{
    tcu::TestContext &testCtx = group->getTestContext();

    {
        TestParams paramsCubeToArray;
        const uint32_t arrayLayers                  = 6u;
        paramsCubeToArray.src.image.createFlags     = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
        paramsCubeToArray.src.image.imageType       = VK_IMAGE_TYPE_2D;
        paramsCubeToArray.src.image.format          = VK_FORMAT_R8G8B8A8_UINT;
        paramsCubeToArray.src.image.extent          = defaultHalfExtent;
        paramsCubeToArray.src.image.extent.depth    = arrayLayers;
        paramsCubeToArray.src.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
        paramsCubeToArray.src.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        paramsCubeToArray.dst.image.createFlags     = 0;
        paramsCubeToArray.dst.image.imageType       = VK_IMAGE_TYPE_2D;
        paramsCubeToArray.dst.image.format          = VK_FORMAT_R8G8B8A8_UINT;
        paramsCubeToArray.dst.image.extent          = defaultHalfExtent;
        paramsCubeToArray.dst.image.extent.depth    = arrayLayers;
        paramsCubeToArray.dst.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
        paramsCubeToArray.dst.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        paramsCubeToArray.dst.image.fillMode        = FILL_MODE_GRADIENT;
        paramsCubeToArray.allocationKind            = testGroupParams->allocationKind;
        paramsCubeToArray.extensionFlags            = testGroupParams->extensionFlags;
        paramsCubeToArray.queueSelection            = testGroupParams->queueSelection;

        for (uint32_t arrayLayersNdx = 0; arrayLayersNdx < arrayLayers; ++arrayLayersNdx)
        {
            const VkImageSubresourceLayers sourceLayer = {
                VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
                0u,                        // uint32_t mipLevel;
                arrayLayersNdx,            // uint32_t baseArrayLayer;
                1u                         // uint32_t layerCount;
            };

            const VkImageSubresourceLayers destinationLayer = {
                VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
                0u,                        // uint32_t mipLevel;
                arrayLayersNdx,            // uint32_t baseArrayLayer;
                1u                         // uint32_t layerCount;
            };

            const VkImageCopy testCopy = {
                sourceLayer,      // VkImageSubresourceLayers srcSubresource;
                {0, 0, 0},        // VkOffset3D srcOffset;
                destinationLayer, // VkImageSubresourceLayers dstSubresource;
                {0, 0, 0},        // VkOffset3D dstOffset;
                defaultHalfExtent // VkExtent3D extent;
            };

            CopyRegion imageCopy;
            imageCopy.imageCopy = testCopy;

            paramsCubeToArray.regions.push_back(imageCopy);
        }

        group->addChild(new CopyImageToImageTestCase(testCtx, "cube_to_array_layers", paramsCubeToArray));
    }

    {
        TestParams paramsCubeToArray;
        const uint32_t arrayLayers                  = 6u;
        paramsCubeToArray.src.image.createFlags     = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
        paramsCubeToArray.src.image.imageType       = VK_IMAGE_TYPE_2D;
        paramsCubeToArray.src.image.format          = VK_FORMAT_R8G8B8A8_UINT;
        paramsCubeToArray.src.image.extent          = defaultHalfExtent;
        paramsCubeToArray.src.image.extent.depth    = arrayLayers;
        paramsCubeToArray.src.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
        paramsCubeToArray.src.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        paramsCubeToArray.dst.image.createFlags     = 0;
        paramsCubeToArray.dst.image.imageType       = VK_IMAGE_TYPE_2D;
        paramsCubeToArray.dst.image.format          = VK_FORMAT_R8G8B8A8_UINT;
        paramsCubeToArray.dst.image.extent          = defaultHalfExtent;
        paramsCubeToArray.dst.image.extent.depth    = arrayLayers;
        paramsCubeToArray.dst.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
        paramsCubeToArray.dst.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        paramsCubeToArray.dst.image.fillMode        = FILL_MODE_GRADIENT;
        paramsCubeToArray.allocationKind            = testGroupParams->allocationKind;
        paramsCubeToArray.extensionFlags            = testGroupParams->extensionFlags;
        paramsCubeToArray.queueSelection            = testGroupParams->queueSelection;

        {
            const VkImageSubresourceLayers sourceLayer = {
                VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
                0u,                        // uint32_t mipLevel;
                0u,                        // uint32_t baseArrayLayer;
                arrayLayers                // uint32_t layerCount;
            };

            const VkImageSubresourceLayers destinationLayer = {
                VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
                0u,                        // uint32_t mipLevel;
                0u,                        // uint32_t baseArrayLayer;
                arrayLayers                // uint32_t layerCount;
            };

            const VkImageCopy testCopy = {
                sourceLayer,      // VkImageSubresourceLayers srcSubresource;
                {0, 0, 0},        // VkOffset3D srcOffset;
                destinationLayer, // VkImageSubresourceLayers dstSubresource;
                {0, 0, 0},        // VkOffset3D dstOffset;
                defaultHalfExtent // VkExtent3D extent;
            };

            CopyRegion imageCopy;
            imageCopy.imageCopy = testCopy;

            paramsCubeToArray.regions.push_back(imageCopy);
        }

        group->addChild(new CopyImageToImageTestCase(testCtx, "cube_to_array_whole", paramsCubeToArray));
    }

    {
        TestParams paramsArrayToCube;
        const uint32_t arrayLayers                  = 6u;
        paramsArrayToCube.src.image.createFlags     = 0;
        paramsArrayToCube.src.image.imageType       = VK_IMAGE_TYPE_2D;
        paramsArrayToCube.src.image.format          = VK_FORMAT_R8G8B8A8_UINT;
        paramsArrayToCube.src.image.extent          = defaultHalfExtent;
        paramsArrayToCube.src.image.extent.depth    = arrayLayers;
        paramsArrayToCube.src.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
        paramsArrayToCube.src.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        paramsArrayToCube.dst.image.createFlags     = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
        paramsArrayToCube.dst.image.imageType       = VK_IMAGE_TYPE_2D;
        paramsArrayToCube.dst.image.format          = VK_FORMAT_R8G8B8A8_UINT;
        paramsArrayToCube.dst.image.extent          = defaultHalfExtent;
        paramsArrayToCube.dst.image.extent.depth    = arrayLayers;
        paramsArrayToCube.dst.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
        paramsArrayToCube.dst.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        paramsArrayToCube.dst.image.fillMode        = FILL_MODE_GRADIENT;
        paramsArrayToCube.allocationKind            = testGroupParams->allocationKind;
        paramsArrayToCube.extensionFlags            = testGroupParams->extensionFlags;
        paramsArrayToCube.queueSelection            = testGroupParams->queueSelection;

        for (uint32_t arrayLayersNdx = 0; arrayLayersNdx < arrayLayers; ++arrayLayersNdx)
        {
            const VkImageSubresourceLayers sourceLayer = {
                VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
                0u,                        // uint32_t mipLevel;
                arrayLayersNdx,            // uint32_t baseArrayLayer;
                1u                         // uint32_t layerCount;
            };

            const VkImageSubresourceLayers destinationLayer = {
                VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
                0u,                        // uint32_t mipLevel;
                arrayLayersNdx,            // uint32_t baseArrayLayer;
                1u                         // uint32_t layerCount;
            };

            const VkImageCopy testCopy = {
                sourceLayer,      // VkImageSubresourceLayers srcSubresource;
                {0, 0, 0},        // VkOffset3D srcOffset;
                destinationLayer, // VkImageSubresourceLayers dstSubresource;
                {0, 0, 0},        // VkOffset3D dstOffset;
                defaultHalfExtent // VkExtent3D extent;
            };

            CopyRegion imageCopy;
            imageCopy.imageCopy = testCopy;

            paramsArrayToCube.regions.push_back(imageCopy);
        }

        group->addChild(new CopyImageToImageTestCase(testCtx, "array_to_cube_layers", paramsArrayToCube));
    }

    {
        TestParams paramsArrayToCube;
        const uint32_t arrayLayers                  = 6u;
        paramsArrayToCube.src.image.createFlags     = 0;
        paramsArrayToCube.src.image.imageType       = VK_IMAGE_TYPE_2D;
        paramsArrayToCube.src.image.format          = VK_FORMAT_R8G8B8A8_UINT;
        paramsArrayToCube.src.image.extent          = defaultHalfExtent;
        paramsArrayToCube.src.image.extent.depth    = arrayLayers;
        paramsArrayToCube.src.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
        paramsArrayToCube.src.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        paramsArrayToCube.dst.image.createFlags     = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
        paramsArrayToCube.dst.image.imageType       = VK_IMAGE_TYPE_2D;
        paramsArrayToCube.dst.image.format          = VK_FORMAT_R8G8B8A8_UINT;
        paramsArrayToCube.dst.image.extent          = defaultHalfExtent;
        paramsArrayToCube.dst.image.extent.depth    = arrayLayers;
        paramsArrayToCube.dst.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
        paramsArrayToCube.dst.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        paramsArrayToCube.dst.image.fillMode        = FILL_MODE_GRADIENT;
        paramsArrayToCube.allocationKind            = testGroupParams->allocationKind;
        paramsArrayToCube.extensionFlags            = testGroupParams->extensionFlags;
        paramsArrayToCube.queueSelection            = testGroupParams->queueSelection;

        {
            const VkImageSubresourceLayers sourceLayer = {
                VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
                0u,                        // uint32_t mipLevel;
                0u,                        // uint32_t baseArrayLayer;
                arrayLayers                // uint32_t layerCount;
            };

            const VkImageSubresourceLayers destinationLayer = {
                VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
                0u,                        // uint32_t mipLevel;
                0u,                        // uint32_t baseArrayLayer;
                arrayLayers                // uint32_t layerCount;
            };

            const VkImageCopy testCopy = {
                sourceLayer,      // VkImageSubresourceLayers srcSubresource;
                {0, 0, 0},        // VkOffset3D srcOffset;
                destinationLayer, // VkImageSubresourceLayers dstSubresource;
                {0, 0, 0},        // VkOffset3D dstOffset;
                defaultHalfExtent // VkExtent3D extent;
            };

            CopyRegion imageCopy;
            imageCopy.imageCopy = testCopy;

            paramsArrayToCube.regions.push_back(imageCopy);
        }

        group->addChild(new CopyImageToImageTestCase(testCtx, "array_to_cube_whole", paramsArrayToCube));
    }

    {
        TestParams paramsCubeToArray;
        const uint32_t arrayLayers                  = 6u;
        paramsCubeToArray.src.image.createFlags     = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
        paramsCubeToArray.src.image.imageType       = VK_IMAGE_TYPE_2D;
        paramsCubeToArray.src.image.format          = VK_FORMAT_R8G8B8A8_UINT;
        paramsCubeToArray.src.image.extent          = defaultHalfExtent;
        paramsCubeToArray.src.image.extent.depth    = arrayLayers;
        paramsCubeToArray.src.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
        paramsCubeToArray.src.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        paramsCubeToArray.dst.image.createFlags     = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
        paramsCubeToArray.dst.image.imageType       = VK_IMAGE_TYPE_2D;
        paramsCubeToArray.dst.image.format          = VK_FORMAT_R8G8B8A8_UINT;
        paramsCubeToArray.dst.image.extent          = defaultHalfExtent;
        paramsCubeToArray.dst.image.extent.depth    = arrayLayers;
        paramsCubeToArray.dst.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
        paramsCubeToArray.dst.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        paramsCubeToArray.dst.image.fillMode        = FILL_MODE_GRADIENT;
        paramsCubeToArray.allocationKind            = testGroupParams->allocationKind;
        paramsCubeToArray.extensionFlags            = testGroupParams->extensionFlags;
        paramsCubeToArray.queueSelection            = testGroupParams->queueSelection;

        for (uint32_t arrayLayersNdx = 0; arrayLayersNdx < arrayLayers; ++arrayLayersNdx)
        {
            const VkImageSubresourceLayers sourceLayer = {
                VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
                0u,                        // uint32_t mipLevel;
                arrayLayersNdx,            // uint32_t baseArrayLayer;
                1u                         // uint32_t layerCount;
            };

            const VkImageSubresourceLayers destinationLayer = {
                VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
                0u,                        // uint32_t mipLevel;
                arrayLayersNdx,            // uint32_t baseArrayLayer;
                1u                         // uint32_t layerCount;
            };

            const VkImageCopy testCopy = {
                sourceLayer,      // VkImageSubresourceLayers srcSubresource;
                {0, 0, 0},        // VkOffset3D srcOffset;
                destinationLayer, // VkImageSubresourceLayers dstSubresource;
                {0, 0, 0},        // VkOffset3D dstOffset;
                defaultHalfExtent // VkExtent3D extent;
            };

            CopyRegion imageCopy;
            imageCopy.imageCopy = testCopy;

            paramsCubeToArray.regions.push_back(imageCopy);
        }

        group->addChild(new CopyImageToImageTestCase(testCtx, "cube_to_cube_layers", paramsCubeToArray));
    }

    {
        TestParams paramsCubeToCube;
        const uint32_t arrayLayers                 = 6u;
        paramsCubeToCube.src.image.createFlags     = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
        paramsCubeToCube.src.image.imageType       = VK_IMAGE_TYPE_2D;
        paramsCubeToCube.src.image.format          = VK_FORMAT_R8G8B8A8_UINT;
        paramsCubeToCube.src.image.extent          = defaultHalfExtent;
        paramsCubeToCube.src.image.extent.depth    = arrayLayers;
        paramsCubeToCube.src.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
        paramsCubeToCube.src.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        paramsCubeToCube.dst.image.createFlags     = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
        paramsCubeToCube.dst.image.imageType       = VK_IMAGE_TYPE_2D;
        paramsCubeToCube.dst.image.format          = VK_FORMAT_R8G8B8A8_UINT;
        paramsCubeToCube.dst.image.extent          = defaultHalfExtent;
        paramsCubeToCube.dst.image.extent.depth    = arrayLayers;
        paramsCubeToCube.dst.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
        paramsCubeToCube.dst.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        paramsCubeToCube.dst.image.fillMode        = FILL_MODE_GRADIENT;
        paramsCubeToCube.allocationKind            = testGroupParams->allocationKind;
        paramsCubeToCube.extensionFlags            = testGroupParams->extensionFlags;
        paramsCubeToCube.queueSelection            = testGroupParams->queueSelection;

        {
            const VkImageSubresourceLayers sourceLayer = {
                VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
                0u,                        // uint32_t mipLevel;
                0u,                        // uint32_t baseArrayLayer;
                arrayLayers                // uint32_t layerCount;
            };

            const VkImageSubresourceLayers destinationLayer = {
                VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
                0u,                        // uint32_t mipLevel;
                0u,                        // uint32_t baseArrayLayer;
                arrayLayers                // uint32_t layerCount;
            };

            const VkImageCopy testCopy = {
                sourceLayer,      // VkImageSubresourceLayers srcSubresource;
                {0, 0, 0},        // VkOffset3D srcOffset;
                destinationLayer, // VkImageSubresourceLayers dstSubresource;
                {0, 0, 0},        // VkOffset3D dstOffset;
                defaultHalfExtent // VkExtent3D extent;
            };

            CopyRegion imageCopy;
            imageCopy.imageCopy = testCopy;

            paramsCubeToCube.regions.push_back(imageCopy);
        }

        group->addChild(new CopyImageToImageTestCase(testCtx, "cube_to_cube_whole", paramsCubeToCube));
    }
}

void addImageToImageArrayTests(tcu::TestCaseGroup *group, TestGroupParamsPtr testGroupParams)
{
    tcu::TestContext &testCtx = group->getTestContext();

    {
        TestParams paramsArrayToArray;
        const uint32_t arrayLayers                   = 16u;
        paramsArrayToArray.src.image.imageType       = VK_IMAGE_TYPE_2D;
        paramsArrayToArray.src.image.format          = VK_FORMAT_R8G8B8A8_UINT;
        paramsArrayToArray.src.image.extent          = defaultHalfExtent;
        paramsArrayToArray.src.image.extent.depth    = arrayLayers;
        paramsArrayToArray.src.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
        paramsArrayToArray.src.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        paramsArrayToArray.dst.image.imageType       = VK_IMAGE_TYPE_2D;
        paramsArrayToArray.dst.image.format          = VK_FORMAT_R8G8B8A8_UINT;
        paramsArrayToArray.dst.image.extent          = defaultHalfExtent;
        paramsArrayToArray.dst.image.extent.depth    = arrayLayers;
        paramsArrayToArray.dst.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
        paramsArrayToArray.dst.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        paramsArrayToArray.dst.image.fillMode        = FILL_MODE_GRADIENT;
        paramsArrayToArray.allocationKind            = testGroupParams->allocationKind;
        paramsArrayToArray.extensionFlags            = testGroupParams->extensionFlags;
        paramsArrayToArray.queueSelection            = testGroupParams->queueSelection;
        paramsArrayToArray.useSparseBinding          = testGroupParams->useSparseBinding;

        for (uint32_t arrayLayersNdx = 0; arrayLayersNdx < arrayLayers; ++arrayLayersNdx)
        {
            const VkImageSubresourceLayers sourceLayer = {
                VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
                0u,                        // uint32_t mipLevel;
                arrayLayersNdx,            // uint32_t baseArrayLayer;
                1u                         // uint32_t layerCount;
            };

            const VkImageSubresourceLayers destinationLayer = {
                VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
                0u,                        // uint32_t mipLevel;
                arrayLayersNdx,            // uint32_t baseArrayLayer;
                1u                         // uint32_t layerCount;
            };

            const VkImageCopy testCopy = {
                sourceLayer,      // VkImageSubresourceLayers srcSubresource;
                {0, 0, 0},        // VkOffset3D srcOffset;
                destinationLayer, // VkImageSubresourceLayers dstSubresource;
                {0, 0, 0},        // VkOffset3D dstOffset;
                defaultHalfExtent // VkExtent3D extent;
            };

            CopyRegion imageCopy;
            imageCopy.imageCopy = testCopy;

            paramsArrayToArray.regions.push_back(imageCopy);
        }

        group->addChild(new CopyImageToImageTestCase(testCtx, "array_to_array_layers", paramsArrayToArray));
    }

    {
        TestParams paramsArrayToArray;
        const uint32_t arrayLayers                   = 16u;
        paramsArrayToArray.src.image.imageType       = VK_IMAGE_TYPE_2D;
        paramsArrayToArray.src.image.format          = VK_FORMAT_R8G8B8A8_UINT;
        paramsArrayToArray.src.image.extent          = defaultHalfExtent;
        paramsArrayToArray.src.image.extent.depth    = arrayLayers;
        paramsArrayToArray.src.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
        paramsArrayToArray.src.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        paramsArrayToArray.dst.image.imageType       = VK_IMAGE_TYPE_2D;
        paramsArrayToArray.dst.image.format          = VK_FORMAT_R8G8B8A8_UINT;
        paramsArrayToArray.dst.image.extent          = defaultHalfExtent;
        paramsArrayToArray.dst.image.extent.depth    = arrayLayers;
        paramsArrayToArray.dst.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
        paramsArrayToArray.dst.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        paramsArrayToArray.dst.image.fillMode        = FILL_MODE_GRADIENT;
        paramsArrayToArray.allocationKind            = testGroupParams->allocationKind;
        paramsArrayToArray.extensionFlags            = testGroupParams->extensionFlags;
        paramsArrayToArray.queueSelection            = testGroupParams->queueSelection;

        {
            const VkImageSubresourceLayers sourceLayer = {
                VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
                0u,                        // uint32_t mipLevel;
                0u,                        // uint32_t baseArrayLayer;
                arrayLayers                // uint32_t layerCount;
            };

            const VkImageSubresourceLayers destinationLayer = {
                VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
                0u,                        // uint32_t mipLevel;
                0u,                        // uint32_t baseArrayLayer;
                arrayLayers                // uint32_t layerCount;
            };

            const VkImageCopy testCopy = {
                sourceLayer,      // VkImageSubresourceLayers srcSubresource;
                {0, 0, 0},        // VkOffset3D srcOffset;
                destinationLayer, // VkImageSubresourceLayers dstSubresource;
                {0, 0, 0},        // VkOffset3D dstOffset;
                defaultHalfExtent // VkExtent3D extent;
            };

            CopyRegion imageCopy;
            imageCopy.imageCopy = testCopy;

            paramsArrayToArray.regions.push_back(imageCopy);
        }

        group->addChild(new CopyImageToImageTestCase(testCtx, "array_to_array_whole", paramsArrayToArray));
    }

    if (testGroupParams->queueSelection == QueueSelectionOptions::Universal)
    {
        TestParams paramsArrayToArray;
        const uint32_t arrayLayers                   = 16u;
        paramsArrayToArray.src.image.imageType       = VK_IMAGE_TYPE_2D;
        paramsArrayToArray.src.image.format          = VK_FORMAT_R8G8B8A8_UINT;
        paramsArrayToArray.src.image.extent          = defaultHalfExtent;
        paramsArrayToArray.src.image.extent.depth    = arrayLayers;
        paramsArrayToArray.src.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
        paramsArrayToArray.src.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        paramsArrayToArray.dst.image.imageType       = VK_IMAGE_TYPE_2D;
        paramsArrayToArray.dst.image.format          = VK_FORMAT_R8G8B8A8_UINT;
        paramsArrayToArray.dst.image.extent          = defaultHalfExtent;
        paramsArrayToArray.dst.image.extent.depth    = arrayLayers;
        paramsArrayToArray.dst.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
        paramsArrayToArray.dst.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        paramsArrayToArray.dst.image.fillMode        = FILL_MODE_GRADIENT;
        paramsArrayToArray.allocationKind            = testGroupParams->allocationKind;
        paramsArrayToArray.extensionFlags            = testGroupParams->extensionFlags;
        paramsArrayToArray.queueSelection            = testGroupParams->queueSelection;
        paramsArrayToArray.extensionFlags |= MAINTENANCE_5;

        {
            const VkImageSubresourceLayers sourceLayer = {
                VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
                0u,                        // uint32_t mipLevel;
                0u,                        // uint32_t baseArrayLayer;
                VK_REMAINING_ARRAY_LAYERS  // uint32_t layerCount;
            };

            const VkImageSubresourceLayers destinationLayer = {
                VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
                0u,                        // uint32_t mipLevel;
                0u,                        // uint32_t baseArrayLayer;
                VK_REMAINING_ARRAY_LAYERS  // uint32_t layerCount;
            };

            const VkImageCopy testCopy = {
                sourceLayer,      // VkImageSubresourceLayers srcSubresource;
                {0, 0, 0},        // VkOffset3D srcOffset;
                destinationLayer, // VkImageSubresourceLayers dstSubresource;
                {0, 0, 0},        // VkOffset3D dstOffset;
                defaultHalfExtent // VkExtent3D extent;
            };

            CopyRegion imageCopy;
            imageCopy.imageCopy = testCopy;

            paramsArrayToArray.regions.push_back(imageCopy);
        }

        group->addChild(
            new CopyImageToImageTestCase(testCtx, "array_to_array_whole_remaining_layers", paramsArrayToArray));
    }

    {
        TestParams paramsArrayToArray;
        const uint32_t arrayLayers                   = 16u;
        paramsArrayToArray.src.image.imageType       = VK_IMAGE_TYPE_2D;
        paramsArrayToArray.src.image.format          = VK_FORMAT_R8G8B8A8_UINT;
        paramsArrayToArray.src.image.extent          = defaultHalfExtent;
        paramsArrayToArray.src.image.extent.depth    = arrayLayers;
        paramsArrayToArray.src.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
        paramsArrayToArray.src.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        paramsArrayToArray.dst.image.imageType       = VK_IMAGE_TYPE_2D;
        paramsArrayToArray.dst.image.format          = VK_FORMAT_R8G8B8A8_UINT;
        paramsArrayToArray.dst.image.extent          = defaultHalfExtent;
        paramsArrayToArray.dst.image.extent.depth    = arrayLayers;
        paramsArrayToArray.dst.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
        paramsArrayToArray.dst.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        paramsArrayToArray.dst.image.fillMode        = FILL_MODE_GRADIENT;
        paramsArrayToArray.allocationKind            = testGroupParams->allocationKind;
        paramsArrayToArray.extensionFlags            = testGroupParams->extensionFlags;
        paramsArrayToArray.queueSelection            = testGroupParams->queueSelection;
        paramsArrayToArray.extensionFlags |= MAINTENANCE_5;

        {
            const VkImageSubresourceLayers sourceLayer = {
                VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
                0u,                        // uint32_t mipLevel;
                3u,                        // uint32_t baseArrayLayer;
                VK_REMAINING_ARRAY_LAYERS  // uint32_t layerCount;
            };

            const VkImageSubresourceLayers destinationLayer = {
                VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
                0u,                        // uint32_t mipLevel;
                3u,                        // uint32_t baseArrayLayer;
                VK_REMAINING_ARRAY_LAYERS  // uint32_t layerCount;
            };

            const VkImageCopy testCopy = {
                sourceLayer,      // VkImageSubresourceLayers srcSubresource;
                {0, 0, 0},        // VkOffset3D srcOffset;
                destinationLayer, // VkImageSubresourceLayers dstSubresource;
                {0, 0, 0},        // VkOffset3D dstOffset;
                defaultHalfExtent // VkExtent3D extent;
            };

            CopyRegion imageCopy;
            imageCopy.imageCopy = testCopy;

            paramsArrayToArray.regions.push_back(imageCopy);
        }

        group->addChild(
            new CopyImageToImageTestCase(testCtx, "array_to_array_partial_remaining_layers", paramsArrayToArray));
    }

    {
        TestParams paramsArrayToArray;
        const uint32_t arrayLayers                   = 16u;
        paramsArrayToArray.src.image.imageType       = VK_IMAGE_TYPE_2D;
        paramsArrayToArray.src.image.extent          = defaultHalfExtent;
        paramsArrayToArray.src.image.extent.depth    = arrayLayers;
        paramsArrayToArray.src.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
        paramsArrayToArray.src.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        paramsArrayToArray.dst.image.imageType       = VK_IMAGE_TYPE_2D;
        paramsArrayToArray.dst.image.extent          = defaultHalfExtent;
        paramsArrayToArray.dst.image.extent.depth    = arrayLayers;
        paramsArrayToArray.dst.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
        paramsArrayToArray.dst.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        paramsArrayToArray.dst.image.fillMode        = FILL_MODE_GRADIENT;
        paramsArrayToArray.allocationKind            = testGroupParams->allocationKind;
        paramsArrayToArray.extensionFlags            = testGroupParams->extensionFlags;
        paramsArrayToArray.queueSelection            = testGroupParams->queueSelection;
        paramsArrayToArray.useSparseBinding          = testGroupParams->useSparseBinding;
        paramsArrayToArray.mipLevels = deLog2Floor32(deMaxu32(defaultHalfExtent.width, defaultHalfExtent.height)) + 1u;

        for (uint32_t mipLevelNdx = 0u; mipLevelNdx < paramsArrayToArray.mipLevels; mipLevelNdx++)
        {
            const VkImageSubresourceLayers sourceLayer = {
                VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
                mipLevelNdx,               // uint32_t mipLevel;
                0u,                        // uint32_t baseArrayLayer;
                arrayLayers                // uint32_t layerCount;
            };

            const VkImageSubresourceLayers destinationLayer = {
                VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
                mipLevelNdx,               // uint32_t mipLevel;
                0u,                        // uint32_t baseArrayLayer;
                arrayLayers                // uint32_t layerCount;
            };

            const VkExtent3D extent = {
                (uint32_t)deMax(defaultHalfExtent.width >> mipLevelNdx, 1),  // uint32_t    width;
                (uint32_t)deMax(defaultHalfExtent.height >> mipLevelNdx, 1), // uint32_t    height;
                1u,                                                          // uint32_t    depth;
            };

            const VkImageCopy testCopy = {
                sourceLayer,      // VkImageSubresourceLayers srcSubresource;
                {0, 0, 0},        // VkOffset3D srcOffset;
                destinationLayer, // VkImageSubresourceLayers dstSubresource;
                {0, 0, 0},        // VkOffset3D dstOffset;
                extent            // VkExtent3D extent;
            };

            CopyRegion imageCopy;
            imageCopy.imageCopy = testCopy;

            paramsArrayToArray.regions.push_back(imageCopy);
        }

        VkFormat imageFormats[] = {VK_FORMAT_R8G8B8A8_UINT, VK_FORMAT_D32_SFLOAT, VK_FORMAT_D16_UNORM,
                                   VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_S8_UINT};

        for (uint32_t imageFormatsNdx = 0; imageFormatsNdx < DE_LENGTH_OF_ARRAY(imageFormats); imageFormatsNdx++)
        {
            paramsArrayToArray.src.image.format = imageFormats[imageFormatsNdx];
            paramsArrayToArray.dst.image.format = imageFormats[imageFormatsNdx];
            for (uint32_t regionNdx = 0u; regionNdx < paramsArrayToArray.regions.size(); regionNdx++)
            {
                paramsArrayToArray.regions[regionNdx].imageCopy.srcSubresource.aspectMask =
                    getImageAspectFlags(mapVkFormat(imageFormats[imageFormatsNdx]));
                paramsArrayToArray.regions[regionNdx].imageCopy.dstSubresource.aspectMask =
                    getImageAspectFlags(mapVkFormat(imageFormats[imageFormatsNdx]));
            }
            std::ostringstream testName;
            const std::string formatName = getFormatName(imageFormats[imageFormatsNdx]);
            testName << "array_to_array_whole_mipmap_" << de::toLower(formatName.substr(10));
            group->addChild(new CopyImageToImageMipmapTestCase(testCtx, testName.str(), paramsArrayToArray));
        }
    }
}

void addImageToImageTestsSimpleOnly(tcu::TestCaseGroup *group, TestGroupParamsPtr testGroupParams)
{
    addTestGroup(group, "simple_tests", addImageToImageSimpleTests, testGroupParams);
}

void addImageToImageTests(tcu::TestCaseGroup *group, TestGroupParamsPtr testGroupParams)
{
    addTestGroup(group, "simple_tests", addImageToImageSimpleTests, testGroupParams);
    if (!testGroupParams->useSparseBinding)
        addTestGroup(group, "all_formats", addImageToImageAllFormatsTests, testGroupParams);
    addTestGroup(group, "3d_images", addImageToImage3dImagesTests, testGroupParams);
    if (!testGroupParams->useSparseBinding)
        addTestGroup(group, "dimensions", addImageToImageDimensionsTests, testGroupParams);
    addTestGroup(group, "cube", addImageToImageCubeTests, testGroupParams);
    addTestGroup(group, "array", addImageToImageArrayTests, testGroupParams);
}

void add1dImageToBufferTests(tcu::TestCaseGroup *group, TestGroupParamsPtr testGroupParams)
{
    tcu::TestContext &testCtx = group->getTestContext();

    {
        TestParams params;
        params.src.image.imageType       = VK_IMAGE_TYPE_1D;
        params.src.image.format          = VK_FORMAT_R8G8B8A8_UNORM;
        params.src.image.extent          = default1dExtent;
        params.src.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
        params.src.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        params.dst.buffer.size           = defaultSize;
        params.allocationKind            = testGroupParams->allocationKind;
        params.extensionFlags            = testGroupParams->extensionFlags;
        params.queueSelection            = testGroupParams->queueSelection;
        params.useSparseBinding          = testGroupParams->useSparseBinding;

        const VkBufferImageCopy bufferImageCopy = {
            0u,                 // VkDeviceSize bufferOffset;
            0u,                 // uint32_t bufferRowLength;
            0u,                 // uint32_t bufferImageHeight;
            defaultSourceLayer, // VkImageSubresourceLayers imageSubresource;
            {0, 0, 0},          // VkOffset3D imageOffset;
            default1dExtent     // VkExtent3D imageExtent;
        };
        CopyRegion copyRegion;
        copyRegion.bufferImageCopy = bufferImageCopy;

        params.regions.push_back(copyRegion);

        group->addChild(new CopyImageToBufferTestCase(testCtx, "tightly_sized_buffer", params));
    }

    {
        TestParams params;
        uint32_t bufferImageHeight       = defaultSize + 1u;
        params.src.image.imageType       = VK_IMAGE_TYPE_1D;
        params.src.image.format          = VK_FORMAT_R8G8B8A8_UNORM;
        params.src.image.extent          = default1dExtent;
        params.src.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
        params.src.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        params.dst.buffer.size           = bufferImageHeight;
        params.allocationKind            = testGroupParams->allocationKind;
        params.extensionFlags            = testGroupParams->extensionFlags;
        params.queueSelection            = testGroupParams->queueSelection;
        params.useSparseBinding          = testGroupParams->useSparseBinding;

        const VkBufferImageCopy bufferImageCopy = {
            0u,                 // VkDeviceSize bufferOffset;
            0u,                 // uint32_t bufferRowLength;
            bufferImageHeight,  // uint32_t bufferImageHeight;
            defaultSourceLayer, // VkImageSubresourceLayers imageSubresource;
            {0, 0, 0},          // VkOffset3D imageOffset;
            default1dExtent     // VkExtent3D imageExtent;
        };
        CopyRegion copyRegion;
        copyRegion.bufferImageCopy = bufferImageCopy;

        params.regions.push_back(copyRegion);

        group->addChild(new CopyImageToBufferTestCase(testCtx, "larger_buffer", params));
    }

    {
        TestParams params;
        uint32_t arrayLayers             = 16u;
        params.src.image.imageType       = VK_IMAGE_TYPE_1D;
        params.src.image.format          = VK_FORMAT_R8G8B8A8_UNORM;
        params.src.image.extent          = default1dExtent;
        params.src.image.extent.depth    = arrayLayers;
        params.src.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
        params.src.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        params.dst.buffer.size           = defaultSize * arrayLayers;
        params.allocationKind            = testGroupParams->allocationKind;
        params.extensionFlags            = testGroupParams->extensionFlags;
        params.queueSelection            = testGroupParams->queueSelection;
        params.useSparseBinding          = testGroupParams->useSparseBinding;

        const int pixelSize = tcu::getPixelSize(mapVkFormat(params.src.image.format));
        for (uint32_t arrayLayerNdx = 0; arrayLayerNdx < arrayLayers; arrayLayerNdx++)
        {
            const VkDeviceSize offset               = defaultSize * pixelSize * arrayLayerNdx;
            const VkBufferImageCopy bufferImageCopy = {
                offset,      // VkDeviceSize bufferOffset;
                0u,          // uint32_t bufferRowLength;
                defaultSize, // uint32_t bufferImageHeight;
                {
                    VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
                    0u,                        // uint32_t mipLevel;
                    arrayLayerNdx,             // uint32_t baseArrayLayer;
                    1u,                        // uint32_t layerCount;
                },                             // VkImageSubresourceLayers imageSubresource;
                {0, 0, 0},                     // VkOffset3D imageOffset;
                default1dExtent                // VkExtent3D imageExtent;
            };
            CopyRegion copyRegion;
            copyRegion.bufferImageCopy = bufferImageCopy;

            params.regions.push_back(copyRegion);
        }

        group->addChild(new CopyImageToBufferTestCase(testCtx, "array_tightly_sized_buffer", params));
    }

    {
        TestParams params;
        uint32_t arrayLayers             = 16u;
        uint32_t bufferImageHeight       = defaultSize + 1u;
        params.src.image.imageType       = VK_IMAGE_TYPE_1D;
        params.src.image.format          = VK_FORMAT_R8G8B8A8_UNORM;
        params.src.image.extent          = default1dExtent;
        params.src.image.extent.depth    = arrayLayers;
        params.src.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
        params.src.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        params.dst.buffer.size           = bufferImageHeight * arrayLayers;
        params.allocationKind            = testGroupParams->allocationKind;
        params.extensionFlags            = testGroupParams->extensionFlags;
        params.queueSelection            = testGroupParams->queueSelection;
        params.useSparseBinding          = testGroupParams->useSparseBinding;

        const int pixelSize = tcu::getPixelSize(mapVkFormat(params.src.image.format));
        for (uint32_t arrayLayerNdx = 0; arrayLayerNdx < arrayLayers; arrayLayerNdx++)
        {
            const VkDeviceSize offset               = bufferImageHeight * pixelSize * arrayLayerNdx;
            const VkBufferImageCopy bufferImageCopy = {
                offset,            // VkDeviceSize bufferOffset;
                0u,                // uint32_t bufferRowLength;
                bufferImageHeight, // uint32_t bufferImageHeight;
                {
                    VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
                    0u,                        // uint32_t mipLevel;
                    arrayLayerNdx,             // uint32_t baseArrayLayer;
                    1u,                        // uint32_t layerCount;
                },                             // VkImageSubresourceLayers imageSubresource;
                {0, 0, 0},                     // VkOffset3D imageOffset;
                default1dExtent                // VkExtent3D imageExtent;
            };
            CopyRegion copyRegion;
            copyRegion.bufferImageCopy = bufferImageCopy;

            params.regions.push_back(copyRegion);
        }

        group->addChild(new CopyImageToBufferTestCase(testCtx, "array_larger_buffer", params));
    }

    {
        TestParams params;
        const uint32_t baseLayer         = 0u;
        const uint32_t layerCount        = 16u;
        params.src.image.imageType       = VK_IMAGE_TYPE_1D;
        params.src.image.format          = VK_FORMAT_R8G8B8A8_UNORM;
        params.src.image.extent          = default1dExtent;
        params.src.image.extent.depth    = layerCount;
        params.src.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
        params.src.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        params.src.image.fillMode        = FILL_MODE_RED;
        params.dst.buffer.size           = defaultSize * layerCount;
        params.dst.buffer.fillMode       = FILL_MODE_RED;
        params.allocationKind            = testGroupParams->allocationKind;
        params.extensionFlags            = testGroupParams->extensionFlags;
        params.queueSelection            = testGroupParams->queueSelection;
        params.useSparseBinding          = testGroupParams->useSparseBinding;
        params.extensionFlags |= MAINTENANCE_5;

        const VkImageSubresourceLayers defaultLayer = {
            VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
            0u,                        // uint32_t mipLevel;
            baseLayer,                 // uint32_t baseArrayLayer;
            VK_REMAINING_ARRAY_LAYERS  // uint32_t layerCount;
        };

        const VkBufferImageCopy bufferImageCopy = {
            0u,             // VkDeviceSize bufferOffset;
            0u,             // uint32_t bufferRowLength;
            0u,             // uint32_t bufferImageHeight;
            defaultLayer,   // VkImageSubresourceLayers imageSubresource;
            {0, 0, 0},      // VkOffset3D imageOffset;
            default1dExtent // VkExtent3D imageExtent;
        };

        CopyRegion copyRegion;
        copyRegion.bufferImageCopy = bufferImageCopy;

        params.regions.push_back(copyRegion);

        group->addChild(new CopyImageToBufferTestCase(testCtx, "array_all_remaining_layers", params));
    }

    {
        TestParams params;
        const uint32_t baseLayer         = 2u;
        const uint32_t layerCount        = 16u;
        params.src.image.imageType       = VK_IMAGE_TYPE_1D;
        params.src.image.format          = VK_FORMAT_R8G8B8A8_UNORM;
        params.src.image.extent          = default1dExtent;
        params.src.image.extent.depth    = layerCount;
        params.src.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
        params.src.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        params.src.image.fillMode        = FILL_MODE_RED;
        params.dst.buffer.size           = defaultSize * layerCount;
        params.dst.buffer.fillMode       = FILL_MODE_RED;
        params.allocationKind            = testGroupParams->allocationKind;
        params.extensionFlags            = testGroupParams->extensionFlags;
        params.queueSelection            = testGroupParams->queueSelection;
        params.useSparseBinding          = testGroupParams->useSparseBinding;
        params.extensionFlags |= MAINTENANCE_5;

        const VkImageSubresourceLayers defaultLayer = {
            VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
            0u,                        // uint32_t mipLevel;
            baseLayer,                 // uint32_t baseArrayLayer;
            VK_REMAINING_ARRAY_LAYERS  // uint32_t layerCount;
        };

        const VkBufferImageCopy bufferImageCopy = {
            0u,             // VkDeviceSize bufferOffset;
            0u,             // uint32_t bufferRowLength;
            0u,             // uint32_t bufferImageHeight;
            defaultLayer,   // VkImageSubresourceLayers imageSubresource;
            {0, 0, 0},      // VkOffset3D imageOffset;
            default1dExtent // VkExtent3D imageExtent;
        };

        CopyRegion copyRegion;
        copyRegion.bufferImageCopy = bufferImageCopy;

        params.regions.push_back(copyRegion);

        group->addChild(new CopyImageToBufferTestCase(testCtx, "array_not_all_remaining_layers", params));
    }
}

void add2dImageToBufferTests(tcu::TestCaseGroup *group, TestGroupParamsPtr testGroupParams)
{
    tcu::TestContext &testCtx = group->getTestContext();

    {
        TestParams params;
        params.src.image.imageType       = VK_IMAGE_TYPE_2D;
        params.src.image.format          = VK_FORMAT_R8G8B8A8_UNORM;
        params.src.image.extent          = defaultExtent;
        params.src.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
        params.src.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        params.dst.buffer.size           = defaultSize * defaultSize;
        params.allocationKind            = testGroupParams->allocationKind;
        params.extensionFlags            = testGroupParams->extensionFlags;
        params.queueSelection            = testGroupParams->queueSelection;
        params.useSparseBinding          = testGroupParams->useSparseBinding;

        const VkBufferImageCopy bufferImageCopy = {
            0u,                 // VkDeviceSize bufferOffset;
            0u,                 // uint32_t bufferRowLength;
            0u,                 // uint32_t bufferImageHeight;
            defaultSourceLayer, // VkImageSubresourceLayers imageSubresource;
            {0, 0, 0},          // VkOffset3D imageOffset;
            defaultExtent       // VkExtent3D imageExtent;
        };
        CopyRegion copyRegion;
        copyRegion.bufferImageCopy = bufferImageCopy;

        params.regions.push_back(copyRegion);

        group->addChild(new CopyImageToBufferTestCase(testCtx, "whole", params));
    }

    {
        TestParams params;
        params.src.image.imageType       = VK_IMAGE_TYPE_2D;
        params.src.image.format          = VK_FORMAT_R8G8B8A8_UNORM;
        params.src.image.extent          = defaultExtent;
        params.src.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
        params.src.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        params.dst.buffer.size           = defaultSize * defaultSize;
        params.allocationKind            = testGroupParams->allocationKind;
        params.extensionFlags            = testGroupParams->extensionFlags;
        params.queueSelection            = testGroupParams->queueSelection;
        params.useSparseBinding          = testGroupParams->useSparseBinding;

        const VkBufferImageCopy bufferImageCopy = {
            defaultSize * defaultHalfSize,               // VkDeviceSize bufferOffset;
            0u,                                          // uint32_t bufferRowLength;
            0u,                                          // uint32_t bufferImageHeight;
            defaultSourceLayer,                          // VkImageSubresourceLayers imageSubresource;
            {defaultQuarterSize, defaultQuarterSize, 0}, // VkOffset3D imageOffset;
            defaultHalfExtent                            // VkExtent3D imageExtent;
        };
        CopyRegion copyRegion;
        copyRegion.bufferImageCopy = bufferImageCopy;

        params.regions.push_back(copyRegion);

        group->addChild(new CopyImageToBufferTestCase(testCtx, "buffer_offset", params));
    }

    if (testGroupParams->queueSelection == QueueSelectionOptions::Universal)
    {
        TestParams params;
        params.src.image.imageType       = VK_IMAGE_TYPE_2D;
        params.src.image.format          = VK_FORMAT_R8_UNORM;
        params.src.image.extent          = defaultExtent;
        params.src.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
        params.src.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        params.dst.buffer.size           = defaultSize * defaultSize;
        params.allocationKind            = testGroupParams->allocationKind;
        params.extensionFlags            = testGroupParams->extensionFlags;
        params.queueSelection            = testGroupParams->queueSelection;
        params.useSparseBinding          = testGroupParams->useSparseBinding;

        const VkBufferImageCopy bufferImageCopy = {
            defaultSize * defaultHalfSize + 1u,          // VkDeviceSize bufferOffset;
            0u,                                          // uint32_t bufferRowLength;
            0u,                                          // uint32_t bufferImageHeight;
            defaultSourceLayer,                          // VkImageSubresourceLayers imageSubresource;
            {defaultQuarterSize, defaultQuarterSize, 0}, // VkOffset3D imageOffset;
            defaultHalfExtent                            // VkExtent3D imageExtent;
        };
        CopyRegion copyRegion;
        copyRegion.bufferImageCopy = bufferImageCopy;

        params.regions.push_back(copyRegion);

        group->addChild(new CopyImageToBufferTestCase(testCtx, "buffer_offset_relaxed", params));
    }

    {
        TestParams params;
        params.src.image.imageType       = VK_IMAGE_TYPE_2D;
        params.src.image.format          = VK_FORMAT_R8G8B8A8_UNORM;
        params.src.image.extent          = defaultExtent;
        params.src.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
        params.src.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        params.dst.buffer.size           = defaultSize * defaultSize;
        params.allocationKind            = testGroupParams->allocationKind;
        params.extensionFlags            = testGroupParams->extensionFlags;
        params.queueSelection            = testGroupParams->queueSelection;
        params.useSparseBinding          = testGroupParams->useSparseBinding;

        const int pixelSize           = tcu::getPixelSize(mapVkFormat(params.src.image.format));
        const VkDeviceSize bufferSize = pixelSize * params.dst.buffer.size;
        const VkDeviceSize offsetSize = pixelSize * defaultQuarterSize * defaultQuarterSize;
        uint32_t divisor              = 1;
        for (VkDeviceSize offset = 0; offset < bufferSize - offsetSize; offset += offsetSize, ++divisor)
        {
            const uint32_t bufferRowLength   = defaultQuarterSize;
            const uint32_t bufferImageHeight = defaultQuarterSize;
            const VkExtent3D imageExtent     = {defaultQuarterSize / divisor, defaultQuarterSize, 1};
            DE_ASSERT(!bufferRowLength || bufferRowLength >= imageExtent.width);
            DE_ASSERT(!bufferImageHeight || bufferImageHeight >= imageExtent.height);
            DE_ASSERT(imageExtent.width * imageExtent.height * imageExtent.depth <= offsetSize);

            CopyRegion region;
            const VkBufferImageCopy bufferImageCopy = {
                offset,             // VkDeviceSize bufferOffset;
                bufferRowLength,    // uint32_t bufferRowLength;
                bufferImageHeight,  // uint32_t bufferImageHeight;
                defaultSourceLayer, // VkImageSubresourceLayers imageSubresource;
                {0, 0, 0},          // VkOffset3D imageOffset;
                imageExtent         // VkExtent3D imageExtent;
            };
            region.bufferImageCopy = bufferImageCopy;
            params.regions.push_back(region);
        }

        group->addChild(new CopyImageToBufferTestCase(testCtx, "regions", params));
    }

    {
        TestParams params;
        params.src.image.imageType       = VK_IMAGE_TYPE_2D;
        params.src.image.format          = VK_FORMAT_R8G8B8A8_UNORM;
        params.src.image.extent          = defaultExtent;
        params.src.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
        params.src.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        params.dst.buffer.size           = (defaultHalfSize - 1u) * defaultSize + defaultHalfSize;
        params.allocationKind            = testGroupParams->allocationKind;
        params.extensionFlags            = testGroupParams->extensionFlags;
        params.queueSelection            = testGroupParams->queueSelection;
        params.useSparseBinding          = testGroupParams->useSparseBinding;

        const VkBufferImageCopy bufferImageCopy = {
            0u,                                          // VkDeviceSize bufferOffset;
            defaultSize,                                 // uint32_t bufferRowLength;
            defaultSize,                                 // uint32_t bufferImageHeight;
            defaultSourceLayer,                          // VkImageSubresourceLayers imageSubresource;
            {defaultQuarterSize, defaultQuarterSize, 0}, // VkOffset3D imageOffset;
            defaultHalfExtent                            // VkExtent3D imageExtent;
        };
        CopyRegion copyRegion;
        copyRegion.bufferImageCopy = bufferImageCopy;

        params.regions.push_back(copyRegion);

        group->addChild(new CopyImageToBufferTestCase(testCtx, "tightly_sized_buffer", params));
    }

    {
        TestParams params;
        uint32_t bufferImageHeight       = defaultSize + 1u;
        params.src.image.imageType       = VK_IMAGE_TYPE_2D;
        params.src.image.format          = VK_FORMAT_R8G8B8A8_UNORM;
        params.src.image.extent          = defaultExtent;
        params.src.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
        params.src.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        params.dst.buffer.size           = bufferImageHeight * defaultSize;
        params.allocationKind            = testGroupParams->allocationKind;
        params.extensionFlags            = testGroupParams->extensionFlags;
        params.queueSelection            = testGroupParams->queueSelection;
        params.useSparseBinding          = testGroupParams->useSparseBinding;

        const VkBufferImageCopy bufferImageCopy = {
            0u,                 // VkDeviceSize bufferOffset;
            defaultSize,        // uint32_t bufferRowLength;
            bufferImageHeight,  // uint32_t bufferImageHeight;
            defaultSourceLayer, // VkImageSubresourceLayers imageSubresource;
            {0, 0, 0},          // VkOffset3D imageOffset;
            defaultExtent       // VkExtent3D imageExtent;
        };
        CopyRegion copyRegion;
        copyRegion.bufferImageCopy = bufferImageCopy;

        params.regions.push_back(copyRegion);

        group->addChild(new CopyImageToBufferTestCase(testCtx, "larger_buffer", params));
    }

    {
        TestParams params;
        params.src.image.imageType       = VK_IMAGE_TYPE_2D;
        params.src.image.format          = VK_FORMAT_R8G8B8A8_UNORM;
        params.src.image.extent          = defaultExtent;
        params.src.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
        params.src.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        params.dst.buffer.size           = (defaultHalfSize - 1u) * defaultSize + defaultHalfSize + defaultQuarterSize;
        params.allocationKind            = testGroupParams->allocationKind;
        params.extensionFlags            = testGroupParams->extensionFlags;
        params.queueSelection            = testGroupParams->queueSelection;
        params.useSparseBinding          = testGroupParams->useSparseBinding;

        const VkBufferImageCopy bufferImageCopy = {
            defaultQuarterSize,                          // VkDeviceSize bufferOffset;
            defaultSize,                                 // uint32_t bufferRowLength;
            defaultSize,                                 // uint32_t bufferImageHeight;
            defaultSourceLayer,                          // VkImageSubresourceLayers imageSubresource;
            {defaultQuarterSize, defaultQuarterSize, 0}, // VkOffset3D imageOffset;
            defaultHalfExtent                            // VkExtent3D imageExtent;
        };
        CopyRegion copyRegion;
        copyRegion.bufferImageCopy = bufferImageCopy;

        params.regions.push_back(copyRegion);

        group->addChild(new CopyImageToBufferTestCase(testCtx, "tightly_sized_buffer_offset", params));
    }

    {
        TestParams params;
        uint32_t arrayLayers             = 16u;
        params.src.image.imageType       = VK_IMAGE_TYPE_2D;
        params.src.image.format          = VK_FORMAT_R8G8B8A8_UNORM;
        params.src.image.extent          = defaultHalfExtent;
        params.src.image.extent.depth    = arrayLayers;
        params.src.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
        params.src.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        params.dst.buffer.size           = defaultHalfSize * defaultHalfSize * arrayLayers;
        params.allocationKind            = testGroupParams->allocationKind;
        params.extensionFlags            = testGroupParams->extensionFlags;
        params.queueSelection            = testGroupParams->queueSelection;
        params.useSparseBinding          = testGroupParams->useSparseBinding;

        const int pixelSize = tcu::getPixelSize(mapVkFormat(params.src.image.format));
        for (uint32_t arrayLayerNdx = 0; arrayLayerNdx < arrayLayers; arrayLayerNdx++)
        {
            const VkDeviceSize offset               = defaultHalfSize * defaultHalfSize * pixelSize * arrayLayerNdx;
            const VkBufferImageCopy bufferImageCopy = {
                offset, // VkDeviceSize bufferOffset;
                0u,     // uint32_t bufferRowLength;
                0u,     // uint32_t bufferImageHeight;
                {
                    VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
                    0u,                        // uint32_t mipLevel;
                    arrayLayerNdx,             // uint32_t baseArrayLayer;
                    1u,                        // uint32_t layerCount;
                },                             // VkImageSubresourceLayers imageSubresource;
                {0, 0, 0},                     // VkOffset3D imageOffset;
                defaultHalfExtent              // VkExtent3D imageExtent;
            };
            CopyRegion copyRegion;
            copyRegion.bufferImageCopy = bufferImageCopy;

            params.regions.push_back(copyRegion);
        }
        group->addChild(new CopyImageToBufferTestCase(testCtx, "array", params));
    }

    {
        TestParams params;
        uint32_t arrayLayers             = 16u;
        uint32_t imageBufferHeight       = defaultHalfSize + 1u;
        params.src.image.imageType       = VK_IMAGE_TYPE_2D;
        params.src.image.format          = VK_FORMAT_R8G8B8A8_UNORM;
        params.src.image.extent          = defaultHalfExtent;
        params.src.image.extent.depth    = arrayLayers;
        params.src.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
        params.src.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        params.dst.buffer.size           = defaultHalfSize * imageBufferHeight * arrayLayers;
        params.allocationKind            = testGroupParams->allocationKind;
        params.extensionFlags            = testGroupParams->extensionFlags;
        params.queueSelection            = testGroupParams->queueSelection;
        params.useSparseBinding          = testGroupParams->useSparseBinding;

        const int pixelSize = tcu::getPixelSize(mapVkFormat(params.src.image.format));
        for (uint32_t arrayLayerNdx = 0; arrayLayerNdx < arrayLayers; arrayLayerNdx++)
        {
            const VkDeviceSize offset               = defaultHalfSize * imageBufferHeight * pixelSize * arrayLayerNdx;
            const VkBufferImageCopy bufferImageCopy = {
                offset,            // VkDeviceSize bufferOffset;
                0u,                // uint32_t bufferRowLength;
                imageBufferHeight, // uint32_t bufferImageHeight;
                {
                    VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
                    0u,                        // uint32_t mipLevel;
                    arrayLayerNdx,             // uint32_t baseArrayLayer;
                    1u,                        // uint32_t layerCount;
                },                             // VkImageSubresourceLayers imageSubresource;
                {0, 0, 0},                     // VkOffset3D imageOffset;
                defaultHalfExtent              // VkExtent3D imageExtent;
            };
            CopyRegion copyRegion;
            copyRegion.bufferImageCopy = bufferImageCopy;

            params.regions.push_back(copyRegion);
        }
        group->addChild(new CopyImageToBufferTestCase(testCtx, "array_larger_buffer", params));
    }

    {
        TestParams params;
        uint32_t arrayLayers             = 16u;
        params.src.image.imageType       = VK_IMAGE_TYPE_2D;
        params.src.image.format          = VK_FORMAT_R8G8B8A8_UNORM;
        params.src.image.extent          = defaultHalfExtent;
        params.src.image.extent.depth    = arrayLayers;
        params.src.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
        params.src.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        params.dst.buffer.size           = defaultHalfSize * defaultHalfSize * arrayLayers;
        params.allocationKind            = testGroupParams->allocationKind;
        params.extensionFlags            = testGroupParams->extensionFlags;
        params.queueSelection            = testGroupParams->queueSelection;
        params.useSparseBinding          = testGroupParams->useSparseBinding;

        const int pixelSize = tcu::getPixelSize(mapVkFormat(params.src.image.format));
        for (uint32_t arrayLayerNdx = 0; arrayLayerNdx < arrayLayers; arrayLayerNdx++)
        {
            const VkDeviceSize offset               = defaultHalfSize * defaultHalfSize * pixelSize * arrayLayerNdx;
            const VkBufferImageCopy bufferImageCopy = {
                offset,          // VkDeviceSize bufferOffset;
                defaultHalfSize, // uint32_t bufferRowLength;
                defaultHalfSize, // uint32_t bufferImageHeight;
                {
                    VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
                    0u,                        // uint32_t mipLevel;
                    arrayLayerNdx,             // uint32_t baseArrayLayer;
                    1u,                        // uint32_t layerCount;
                },                             // VkImageSubresourceLayers imageSubresource;
                {0, 0, 0},                     // VkOffset3D imageOffset;
                defaultHalfExtent              // VkExtent3D imageExtent;
            };
            CopyRegion copyRegion;
            copyRegion.bufferImageCopy = bufferImageCopy;

            params.regions.push_back(copyRegion);
        }
        group->addChild(new CopyImageToBufferTestCase(testCtx, "array_tightly_sized_buffer", params));
    }

    {
        TestParams params;
        const uint32_t baseLayer         = 0u;
        const uint32_t layerCount        = 16u;
        params.src.image.imageType       = VK_IMAGE_TYPE_2D;
        params.src.image.format          = VK_FORMAT_R8G8B8A8_UNORM;
        params.src.image.extent          = defaultHalfExtent;
        params.src.image.extent.depth    = layerCount;
        params.src.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
        params.src.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        params.src.image.fillMode        = FILL_MODE_RED;
        params.dst.buffer.size           = defaultHalfSize * defaultHalfSize * layerCount;
        params.dst.buffer.fillMode       = FILL_MODE_RED;
        params.allocationKind            = testGroupParams->allocationKind;
        params.extensionFlags            = testGroupParams->extensionFlags;
        params.queueSelection            = testGroupParams->queueSelection;
        params.useSparseBinding          = testGroupParams->useSparseBinding;
        params.extensionFlags |= MAINTENANCE_5;

        const VkImageSubresourceLayers defaultLayer = {
            VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
            0u,                        // uint32_t mipLevel;
            baseLayer,                 // uint32_t baseArrayLayer;
            VK_REMAINING_ARRAY_LAYERS  // uint32_t layerCount;
        };

        const VkBufferImageCopy bufferImageCopy = {
            0,                // VkDeviceSize bufferOffset;
            0,                // uint32_t bufferRowLength;
            0,                // uint32_t bufferImageHeight;
            defaultLayer,     // VkImageSubresourceLayers imageSubresource;
            {0, 0, 0},        // VkOffset3D imageOffset;
            defaultHalfExtent // VkExtent3D imageExtent;
        };

        CopyRegion copyRegion;
        copyRegion.bufferImageCopy = bufferImageCopy;

        params.regions.push_back(copyRegion);

        group->addChild(new CopyImageToBufferTestCase(testCtx, "array_all_remaining_layers", params));
    }

    {
        TestParams params;
        const uint32_t baseLayer         = 2u;
        const uint32_t layerCount        = 16u;
        params.src.image.imageType       = VK_IMAGE_TYPE_2D;
        params.src.image.format          = VK_FORMAT_R8G8B8A8_UNORM;
        params.src.image.extent          = defaultHalfExtent;
        params.src.image.extent.depth    = layerCount;
        params.src.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
        params.src.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        params.src.image.fillMode        = FILL_MODE_RED;
        params.dst.buffer.size           = defaultHalfSize * defaultHalfSize * layerCount;
        params.dst.buffer.fillMode       = FILL_MODE_RED;
        params.allocationKind            = testGroupParams->allocationKind;
        params.extensionFlags            = testGroupParams->extensionFlags;
        params.queueSelection            = testGroupParams->queueSelection;
        params.useSparseBinding          = testGroupParams->useSparseBinding;
        params.extensionFlags |= MAINTENANCE_5;

        const VkImageSubresourceLayers defaultLayer = {
            VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
            0u,                        // uint32_t mipLevel;
            baseLayer,                 // uint32_t baseArrayLayer;
            VK_REMAINING_ARRAY_LAYERS  // uint32_t layerCount;
        };

        const VkBufferImageCopy bufferImageCopy = {
            0,                // VkDeviceSize bufferOffset;
            0,                // uint32_t bufferRowLength;
            0,                // uint32_t bufferImageHeight;
            defaultLayer,     // VkImageSubresourceLayers imageSubresource;
            {0, 0, 0},        // VkOffset3D imageOffset;
            defaultHalfExtent // VkExtent3D imageExtent;
        };

        CopyRegion copyRegion;
        copyRegion.bufferImageCopy = bufferImageCopy;

        params.regions.push_back(copyRegion);

        group->addChild(new CopyImageToBufferTestCase(testCtx, "array_not_all_remaining_layers", params));
    }

    // those tests are performed for all queues, no need to repeat them
    // when testGroupParams->queueSelection is set to TransferOnly
    if (testGroupParams->queueSelection == QueueSelectionOptions::Universal)
    {
        VkExtent3D extents[] = {
            // Most miplevels will be multiples of four. All power-of-2 edge sizes. Never a weird mip level with extents smaller than the blockwidth.
            {64, 64, 1},
            // Odd mip edge multiples, two lowest miplevels on the y-axis will have widths of 3 and 1 respectively, less than the compression blocksize, and potentially tricky.
            {64, 192, 1},
        };

        uint32_t arrayLayers[] = {1, 2, 5};

        auto getCaseName = [](VkFormat format, VkExtent3D extent, uint32_t numLayers, std::string queueName)
        {
            std::string caseName = "mip_copies_" + getFormatCaseName(format) + "_" + std::to_string(extent.width) +
                                   "x" + std::to_string(extent.height);
            if (numLayers > 1)
                caseName.append("_" + std::to_string(numLayers) + "_layers");
            caseName.append("_" + queueName);
            return caseName;
        };

        for (const auto &extent : extents)
            for (const auto numLayers : arrayLayers)
            {
                TestParams params;
                params.src.image.imageType       = VK_IMAGE_TYPE_2D;
                params.src.image.extent          = extent;
                params.src.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
                params.src.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
                params.allocationKind            = testGroupParams->allocationKind;
                params.extensionFlags            = testGroupParams->extensionFlags;
                params.queueSelection            = testGroupParams->queueSelection;
                params.useSparseBinding          = testGroupParams->useSparseBinding;
                params.arrayLayers               = numLayers;

                for (const VkFormat *format = compressedFormatsFloats; *format != VK_FORMAT_UNDEFINED; format++)
                {
                    params.src.image.format = *format;
                    {
                        params.queueSelection = QueueSelectionOptions::Universal;
                        group->addChild(new CopyCompressedImageToBufferTestCase(
                            testCtx, getCaseName(*format, params.src.image.extent, numLayers, "universal"), params));
                        params.queueSelection = QueueSelectionOptions::ComputeOnly;
                        group->addChild(new CopyCompressedImageToBufferTestCase(
                            testCtx, getCaseName(*format, params.src.image.extent, numLayers, "compute"), params));
                        params.queueSelection = QueueSelectionOptions::TransferOnly;
                        group->addChild(new CopyCompressedImageToBufferTestCase(
                            testCtx, getCaseName(*format, params.src.image.extent, numLayers, "transfer"), params));
                    }
                }
            }
    }
}

void addBufferToDepthStencilTests(tcu::TestCaseGroup *group, AllocationKind allocationKind, uint32_t extensionFlags)
{
    tcu::TestContext &testCtx = group->getTestContext();

    const struct
    {
        const char *name;
        const VkFormat format;
    } depthAndStencilFormats[] = {{"d16_unorm", VK_FORMAT_D16_UNORM},
                                  {"x8_d24_unorm_pack32", VK_FORMAT_X8_D24_UNORM_PACK32},
                                  {"d32_sfloat", VK_FORMAT_D32_SFLOAT},
                                  {"d16_unorm_s8_uint", VK_FORMAT_D16_UNORM_S8_UINT},
                                  {"d24_unorm_s8_uint", VK_FORMAT_D24_UNORM_S8_UINT},
                                  {"d32_sfloat_s8_uint", VK_FORMAT_D32_SFLOAT_S8_UINT}};

    const VkImageSubresourceLayers depthSourceLayer = {
        VK_IMAGE_ASPECT_DEPTH_BIT, // VkImageAspectFlags aspectMask;
        0u,                        // uint32_t mipLevel;
        0u,                        // uint32_t baseArrayLayer;
        1u,                        // uint32_t layerCount;
    };

    const VkBufferImageCopy bufferDepthCopy = {
        0u,               // VkDeviceSize bufferOffset;
        0u,               // uint32_t bufferRowLength;
        0u,               // uint32_t bufferImageHeight;
        depthSourceLayer, // VkImageSubresourceLayers imageSubresource;
        {0, 0, 0},        // VkOffset3D imageOffset;
        defaultExtent     // VkExtent3D imageExtent;
    };

    const VkBufferImageCopy bufferDepthCopyOffset = {
        32,                                          // VkDeviceSize bufferOffset;
        defaultHalfSize + defaultQuarterSize,        // uint32_t bufferRowLength;
        defaultHalfSize + defaultQuarterSize,        // uint32_t bufferImageHeight;
        depthSourceLayer,                            // VkImageSubresourceLayers imageSubresource;
        {defaultQuarterSize, defaultQuarterSize, 0}, // VkOffset3D imageOffset;
        defaultHalfExtent                            // VkExtent3D imageExtent;
    };

    const VkImageSubresourceLayers stencilSourceLayer = {
        VK_IMAGE_ASPECT_STENCIL_BIT, // VkImageAspectFlags aspectMask;
        0u,                          // uint32_t mipLevel;
        0u,                          // uint32_t baseArrayLayer;
        1u,                          // uint32_t layerCount;
    };

    const VkBufferImageCopy bufferStencilCopy = {
        0u,                 // VkDeviceSize bufferOffset;
        0u,                 // uint32_t bufferRowLength;
        0u,                 // uint32_t bufferImageHeight;
        stencilSourceLayer, // VkImageSubresourceLayers imageSubresource;
        {0, 0, 0},          // VkOffset3D imageOffset;
        defaultExtent       // VkExtent3D imageExtent;
    };

    const VkBufferImageCopy bufferStencilCopyOffset = {
        32,                                          // VkDeviceSize bufferOffset;
        defaultHalfSize + defaultQuarterSize,        // uint32_t bufferRowLength;
        defaultHalfSize + defaultQuarterSize,        // uint32_t bufferImageHeight;
        stencilSourceLayer,                          // VkImageSubresourceLayers imageSubresource;
        {defaultQuarterSize, defaultQuarterSize, 0}, // VkOffset3D imageOffset;
        defaultHalfExtent                            // VkExtent3D imageExtent;
    };

    const bool useOffset[] = {false, true};

    // Note: Depth stencil tests I want to do
    // Formats: D16, D24S8, D32FS8
    // Test writing each component with separate CopyBufferToImage commands
    // Test writing both components in one CopyBufferToImage command
    // Swap order of writes of Depth & Stencil
    // whole surface, subimages?
    // Similar tests as BufferToImage?
    for (const auto config : depthAndStencilFormats)
        for (const auto offset : useOffset)
        {
            // TODO: Check that this format is supported before creating tests?
            //if (isSupportedDepthStencilFormat(vki, physDevice, VK_FORMAT_D24_UNORM_S8_UINT))

            CopyRegion copyDepthRegion;
            CopyRegion copyStencilRegion;
            TestParams params;
            const tcu::TextureFormat format = mapVkFormat(config.format);
            const bool hasDepth             = tcu::hasDepthComponent(format.order);
            const bool hasStencil           = tcu::hasStencilComponent(format.order);
            std::string testName            = config.name;

            if (offset)
            {
                copyDepthRegion.bufferImageCopy   = bufferDepthCopyOffset;
                copyStencilRegion.bufferImageCopy = bufferStencilCopyOffset;
                testName                          = "buffer_offset_" + testName;
                params.src.buffer.size = (defaultHalfSize - 1u) * defaultSize + defaultHalfSize + defaultQuarterSize;
            }
            else
            {
                copyDepthRegion.bufferImageCopy   = bufferDepthCopy;
                copyStencilRegion.bufferImageCopy = bufferStencilCopy;
                params.src.buffer.size            = defaultSize * defaultSize;
            }

            params.dst.image.imageType       = VK_IMAGE_TYPE_2D;
            params.dst.image.format          = config.format;
            params.dst.image.extent          = defaultExtent;
            params.dst.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
            params.dst.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            params.allocationKind            = allocationKind;
            params.extensionFlags            = extensionFlags;

            if (hasDepth && hasStencil)
            {
                params.singleCommand = true;

                params.regions.push_back(copyDepthRegion);
                params.regions.push_back(copyStencilRegion);

                group->addChild(new CopyBufferToDepthStencilTestCase(testCtx, testName + "_DS", params));

                params.singleCommand = false;

                group->addChild(new CopyBufferToDepthStencilTestCase(testCtx, testName + "_D_S", params));

                params.regions.clear();
                params.regions.push_back(copyStencilRegion);
                params.regions.push_back(copyDepthRegion);

                group->addChild(new CopyBufferToDepthStencilTestCase(testCtx, testName + "_S_D", params));

                params.singleCommand = true;
                group->addChild(new CopyBufferToDepthStencilTestCase(testCtx, testName + "_SD", params));
            }

            if (hasStencil)
            {
                params.regions.clear();
                params.regions.push_back(copyStencilRegion);

                group->addChild(new CopyBufferToDepthStencilTestCase(testCtx, testName + "_S", params));
            }

            if (hasDepth)
            {
                params.regions.clear();
                params.regions.push_back(copyDepthRegion);

                group->addChild(new CopyBufferToDepthStencilTestCase(testCtx, testName + "_D", params));
            }
        }
}

void add1dBufferToImageTests(tcu::TestCaseGroup *group, TestGroupParamsPtr testGroupParams)
{
    tcu::TestContext &testCtx = group->getTestContext();

    {
        TestParams params;
        params.src.buffer.size           = defaultSize;
        params.dst.image.imageType       = VK_IMAGE_TYPE_1D;
        params.dst.image.format          = VK_FORMAT_R8G8B8A8_UINT;
        params.dst.image.extent          = default1dExtent;
        params.dst.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
        params.dst.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        params.allocationKind            = testGroupParams->allocationKind;
        params.extensionFlags            = testGroupParams->extensionFlags;
        params.queueSelection            = testGroupParams->queueSelection;
        params.useSparseBinding          = testGroupParams->useSparseBinding;

        const VkBufferImageCopy bufferImageCopy = {
            0u,                 // VkDeviceSize bufferOffset;
            0u,                 // uint32_t bufferRowLength;
            0u,                 // uint32_t bufferImageHeight;
            defaultSourceLayer, // VkImageSubresourceLayers imageSubresource;
            {0, 0, 0},          // VkOffset3D imageOffset;
            default1dExtent     // VkExtent3D imageExtent;
        };
        CopyRegion copyRegion;
        copyRegion.bufferImageCopy = bufferImageCopy;

        params.regions.push_back(copyRegion);

        group->addChild(new CopyBufferToImageTestCase(testCtx, "tightly_sized_buffer", params));
    }

    {
        TestParams params;
        uint32_t bufferImageHeight       = defaultSize + 1u;
        params.src.buffer.size           = bufferImageHeight;
        params.dst.image.imageType       = VK_IMAGE_TYPE_1D;
        params.dst.image.format          = VK_FORMAT_R8G8B8A8_UINT;
        params.dst.image.extent          = default1dExtent;
        params.dst.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
        params.dst.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        params.allocationKind            = testGroupParams->allocationKind;
        params.extensionFlags            = testGroupParams->extensionFlags;
        params.queueSelection            = testGroupParams->queueSelection;
        params.useSparseBinding          = testGroupParams->useSparseBinding;

        const VkBufferImageCopy bufferImageCopy = {
            0u,                 // VkDeviceSize bufferOffset;
            0u,                 // uint32_t bufferRowLength;
            bufferImageHeight,  // uint32_t bufferImageHeight;
            defaultSourceLayer, // VkImageSubresourceLayers imageSubresource;
            {0, 0, 0},          // VkOffset3D imageOffset;
            default1dExtent     // VkExtent3D imageExtent;
        };
        CopyRegion copyRegion;
        copyRegion.bufferImageCopy = bufferImageCopy;

        params.regions.push_back(copyRegion);

        group->addChild(new CopyBufferToImageTestCase(testCtx, "larger_buffer", params));
    }

    {
        TestParams params;
        uint32_t arrayLayers             = 16u;
        params.src.buffer.size           = defaultSize * arrayLayers;
        params.dst.image.imageType       = VK_IMAGE_TYPE_1D;
        params.dst.image.format          = VK_FORMAT_R8G8B8A8_UNORM;
        params.dst.image.extent          = default1dExtent;
        params.dst.image.extent.depth    = arrayLayers;
        params.dst.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
        params.dst.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        params.allocationKind            = testGroupParams->allocationKind;
        params.extensionFlags            = testGroupParams->extensionFlags;
        params.queueSelection            = testGroupParams->queueSelection;
        params.useSparseBinding          = testGroupParams->useSparseBinding;

        const int pixelSize = tcu::getPixelSize(mapVkFormat(params.dst.image.format));
        for (uint32_t arrayLayerNdx = 0; arrayLayerNdx < arrayLayers; arrayLayerNdx++)
        {
            const VkDeviceSize offset               = defaultSize * pixelSize * arrayLayerNdx;
            const VkBufferImageCopy bufferImageCopy = {
                offset, // VkDeviceSize bufferOffset;
                0u,     // uint32_t bufferRowLength;
                0u,     // uint32_t bufferImageHeight;
                {
                    VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
                    0u,                        // uint32_t mipLevel;
                    arrayLayerNdx,             // uint32_t baseArrayLayer;
                    1u,                        // uint32_t layerCount;
                },                             // VkImageSubresourceLayers imageSubresource;
                {0, 0, 0},                     // VkOffset3D imageOffset;
                default1dExtent                // VkExtent3D imageExtent;
            };
            CopyRegion copyRegion;
            copyRegion.bufferImageCopy = bufferImageCopy;

            params.regions.push_back(copyRegion);
        }

        group->addChild(new CopyBufferToImageTestCase(testCtx, "array_tightly_sized_buffer", params));
    }

    {
        TestParams params;
        const uint32_t baseLayer         = 0u;
        const uint32_t layerCount        = 16u;
        params.src.buffer.size           = defaultSize * layerCount;
        params.src.buffer.fillMode       = FILL_MODE_RED;
        params.dst.image.imageType       = VK_IMAGE_TYPE_1D;
        params.dst.image.format          = VK_FORMAT_R8G8B8A8_UNORM;
        params.dst.image.extent          = default1dExtent;
        params.dst.image.extent.depth    = layerCount;
        params.dst.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
        params.dst.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        params.dst.image.fillMode        = FILL_MODE_RED;
        params.allocationKind            = testGroupParams->allocationKind;
        params.extensionFlags            = testGroupParams->extensionFlags;
        params.queueSelection            = testGroupParams->queueSelection;
        params.useSparseBinding          = testGroupParams->useSparseBinding;
        params.extensionFlags |= MAINTENANCE_5;

        const VkImageSubresourceLayers defaultLayer = {
            VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
            0u,                        // uint32_t mipLevel;
            baseLayer,                 // uint32_t baseArrayLayer;
            VK_REMAINING_ARRAY_LAYERS  // uint32_t layerCount;
        };

        const VkBufferImageCopy bufferImageCopy = {
            0u,             // VkDeviceSize bufferOffset;
            0u,             // uint32_t bufferRowLength;
            0u,             // uint32_t bufferImageHeight;
            defaultLayer,   // VkImageSubresourceLayers imageSubresource;
            {0, 0, 0},      // VkOffset3D imageOffset;
            default1dExtent // VkExtent3D imageExtent;
        };

        CopyRegion copyRegion;
        copyRegion.bufferImageCopy = bufferImageCopy;

        params.regions.push_back(copyRegion);

        group->addChild(new CopyBufferToImageTestCase(testCtx, "array_all_remaining_layers", params));
    }

    {
        TestParams params;
        const uint32_t baseLayer         = 2u;
        const uint32_t layerCount        = 16u;
        params.src.buffer.size           = defaultSize * layerCount;
        params.src.buffer.fillMode       = FILL_MODE_RED;
        params.dst.image.imageType       = VK_IMAGE_TYPE_1D;
        params.dst.image.format          = VK_FORMAT_R8G8B8A8_UNORM;
        params.dst.image.extent          = default1dExtent;
        params.dst.image.extent.depth    = layerCount;
        params.dst.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
        params.dst.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        params.dst.image.fillMode        = FILL_MODE_RED;
        params.allocationKind            = testGroupParams->allocationKind;
        params.extensionFlags            = testGroupParams->extensionFlags;
        params.queueSelection            = testGroupParams->queueSelection;
        params.useSparseBinding          = testGroupParams->useSparseBinding;
        params.extensionFlags |= MAINTENANCE_5;

        const VkImageSubresourceLayers defaultLayer = {
            VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
            0u,                        // uint32_t mipLevel;
            baseLayer,                 // uint32_t baseArrayLayer;
            VK_REMAINING_ARRAY_LAYERS  // uint32_t layerCount;
        };

        const VkBufferImageCopy bufferImageCopy = {
            0u,             // VkDeviceSize bufferOffset;
            0u,             // uint32_t bufferRowLength;
            0u,             // uint32_t bufferImageHeight;
            defaultLayer,   // VkImageSubresourceLayers imageSubresource;
            {0, 0, 0},      // VkOffset3D imageOffset;
            default1dExtent // VkExtent3D imageExtent;
        };

        CopyRegion copyRegion;
        copyRegion.bufferImageCopy = bufferImageCopy;

        params.regions.push_back(copyRegion);

        group->addChild(new CopyBufferToImageTestCase(testCtx, "array_not_all_remaining_layers", params));
    }

    {
        TestParams params;
        uint32_t arrayLayers             = 16u;
        uint32_t bufferImageHeight       = defaultSize + 1u;
        params.src.buffer.size           = defaultSize * arrayLayers;
        params.dst.image.imageType       = VK_IMAGE_TYPE_1D;
        params.dst.image.format          = VK_FORMAT_R8G8B8A8_UNORM;
        params.dst.image.extent          = default1dExtent;
        params.dst.image.extent.depth    = arrayLayers;
        params.dst.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
        params.dst.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        params.allocationKind            = testGroupParams->allocationKind;
        params.extensionFlags            = testGroupParams->extensionFlags;
        params.queueSelection            = testGroupParams->queueSelection;
        params.useSparseBinding          = testGroupParams->useSparseBinding;

        const int pixelSize = tcu::getPixelSize(mapVkFormat(params.dst.image.format));
        for (uint32_t arrayLayerNdx = 0; arrayLayerNdx < arrayLayers; arrayLayerNdx++)
        {
            const VkDeviceSize offset               = defaultSize * pixelSize * arrayLayerNdx;
            const VkBufferImageCopy bufferImageCopy = {
                offset,            // VkDeviceSize bufferOffset;
                0u,                // uint32_t bufferRowLength;
                bufferImageHeight, // uint32_t bufferImageHeight;
                {
                    VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
                    0u,                        // uint32_t mipLevel;
                    arrayLayerNdx,             // uint32_t baseArrayLayer;
                    1u,                        // uint32_t layerCount;
                },                             // VkImageSubresourceLayers imageSubresource;
                {0, 0, 0},                     // VkOffset3D imageOffset;
                default1dExtent                // VkExtent3D imageExtent;
            };
            CopyRegion copyRegion;
            copyRegion.bufferImageCopy = bufferImageCopy;

            params.regions.push_back(copyRegion);
        }

        group->addChild(new CopyBufferToImageTestCase(testCtx, "array_larger_buffer", params));
    }
}

void add2dBufferToImageTests(tcu::TestCaseGroup *group, TestGroupParamsPtr testGroupParams)
{
    tcu::TestContext &testCtx = group->getTestContext();

    {
        TestParams params;
        params.src.buffer.size           = defaultSize * defaultSize;
        params.dst.image.imageType       = VK_IMAGE_TYPE_2D;
        params.dst.image.format          = VK_FORMAT_R8G8B8A8_UINT;
        params.dst.image.extent          = defaultExtent;
        params.dst.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
        params.dst.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        params.allocationKind            = testGroupParams->allocationKind;
        params.extensionFlags            = testGroupParams->extensionFlags;
        params.queueSelection            = testGroupParams->queueSelection;
        params.useSparseBinding          = testGroupParams->useSparseBinding;

        const VkBufferImageCopy bufferImageCopy = {
            0u,                 // VkDeviceSize bufferOffset;
            0u,                 // uint32_t bufferRowLength;
            0u,                 // uint32_t bufferImageHeight;
            defaultSourceLayer, // VkImageSubresourceLayers imageSubresource;
            {0, 0, 0},          // VkOffset3D imageOffset;
            defaultExtent       // VkExtent3D imageExtent;
        };
        CopyRegion copyRegion;
        copyRegion.bufferImageCopy = bufferImageCopy;

        params.regions.push_back(copyRegion);

        group->addChild(new CopyBufferToImageTestCase(testCtx, "whole", params));
    }

    {
        TestParams params;
        params.src.buffer.size           = defaultSize * defaultSize;
        params.dst.image.imageType       = VK_IMAGE_TYPE_2D;
        params.dst.image.format          = VK_FORMAT_R8G8B8A8_UNORM;
        params.dst.image.extent          = defaultExtent;
        params.dst.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
        params.dst.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        params.allocationKind            = testGroupParams->allocationKind;
        params.extensionFlags            = testGroupParams->extensionFlags;
        params.queueSelection            = testGroupParams->queueSelection;
        params.useSparseBinding          = testGroupParams->useSparseBinding;

        CopyRegion region;
        uint32_t divisor = 1;
        for (int offset = 0; (offset + defaultQuarterSize / divisor < defaultSize) && (defaultQuarterSize > divisor);
             offset += defaultQuarterSize / divisor++)
        {
            const VkBufferImageCopy bufferImageCopy = {
                0u,                           // VkDeviceSize bufferOffset;
                0u,                           // uint32_t bufferRowLength;
                0u,                           // uint32_t bufferImageHeight;
                defaultSourceLayer,           // VkImageSubresourceLayers imageSubresource;
                {offset, defaultHalfSize, 0}, // VkOffset3D imageOffset;
                {defaultQuarterSize / divisor, defaultQuarterSize / divisor, 1} // VkExtent3D imageExtent;
            };
            region.bufferImageCopy = bufferImageCopy;
            params.regions.push_back(region);
        }

        group->addChild(new CopyBufferToImageTestCase(testCtx, "regions", params));
    }

    {
        TestParams params;
        params.src.buffer.size           = defaultSize * defaultSize;
        params.dst.image.imageType       = VK_IMAGE_TYPE_2D;
        params.dst.image.format          = VK_FORMAT_R8G8B8A8_UNORM;
        params.dst.image.extent          = defaultExtent;
        params.dst.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
        params.dst.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        params.allocationKind            = testGroupParams->allocationKind;
        params.extensionFlags            = testGroupParams->extensionFlags;
        params.queueSelection            = testGroupParams->queueSelection;
        params.useSparseBinding          = testGroupParams->useSparseBinding;

        const VkBufferImageCopy bufferImageCopy = {
            defaultQuarterSize,                          // VkDeviceSize bufferOffset;
            defaultHalfSize + defaultQuarterSize,        // uint32_t bufferRowLength;
            defaultHalfSize + defaultQuarterSize,        // uint32_t bufferImageHeight;
            defaultSourceLayer,                          // VkImageSubresourceLayers imageSubresource;
            {defaultQuarterSize, defaultQuarterSize, 0}, // VkOffset3D imageOffset;
            defaultHalfExtent                            // VkExtent3D imageExtent;
        };
        CopyRegion copyRegion;
        copyRegion.bufferImageCopy = bufferImageCopy;

        params.regions.push_back(copyRegion);

        group->addChild(new CopyBufferToImageTestCase(testCtx, "buffer_offset", params));
    }

    if (testGroupParams->queueSelection == QueueSelectionOptions::Universal)
    {
        TestParams params;
        params.src.buffer.size           = defaultSize * defaultSize;
        params.dst.image.imageType       = VK_IMAGE_TYPE_2D;
        params.dst.image.format          = VK_FORMAT_R8_UNORM;
        params.dst.image.extent          = defaultExtent;
        params.dst.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
        params.dst.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        params.allocationKind            = testGroupParams->allocationKind;
        params.extensionFlags            = testGroupParams->extensionFlags;
        params.queueSelection            = testGroupParams->queueSelection;
        params.useSparseBinding          = testGroupParams->useSparseBinding;

        const VkBufferImageCopy bufferImageCopy = {
            defaultQuarterSize + 1u,                     // VkDeviceSize bufferOffset;
            defaultHalfSize + defaultQuarterSize,        // uint32_t bufferRowLength;
            defaultHalfSize + defaultQuarterSize,        // uint32_t bufferImageHeight;
            defaultSourceLayer,                          // VkImageSubresourceLayers imageSubresource;
            {defaultQuarterSize, defaultQuarterSize, 0}, // VkOffset3D imageOffset;
            defaultHalfExtent                            // VkExtent3D imageExtent;
        };
        CopyRegion copyRegion;
        copyRegion.bufferImageCopy = bufferImageCopy;

        params.regions.push_back(copyRegion);

        group->addChild(new CopyBufferToImageTestCase(testCtx, "buffer_offset_relaxed", params));
    }

    {
        TestParams params;
        params.src.buffer.size           = (defaultHalfSize - 1u) * defaultSize + defaultHalfSize;
        params.dst.image.imageType       = VK_IMAGE_TYPE_2D;
        params.dst.image.format          = VK_FORMAT_R8G8B8A8_UNORM;
        params.dst.image.extent          = defaultExtent;
        params.dst.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
        params.dst.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        params.allocationKind            = testGroupParams->allocationKind;
        params.extensionFlags            = testGroupParams->extensionFlags;
        params.queueSelection            = testGroupParams->queueSelection;
        params.useSparseBinding          = testGroupParams->useSparseBinding;

        const VkBufferImageCopy bufferImageCopy = {
            0u,                                          // VkDeviceSize bufferOffset;
            defaultSize,                                 // uint32_t bufferRowLength;
            defaultSize,                                 // uint32_t bufferImageHeight;
            defaultSourceLayer,                          // VkImageSubresourceLayers imageSubresource;
            {defaultQuarterSize, defaultQuarterSize, 0}, // VkOffset3D imageOffset;
            defaultHalfExtent                            // VkExtent3D imageExtent;
        };
        CopyRegion copyRegion;
        copyRegion.bufferImageCopy = bufferImageCopy;

        params.regions.push_back(copyRegion);

        group->addChild(new CopyBufferToImageTestCase(testCtx, "tightly_sized_buffer", params));
    }

    {
        TestParams params;
        uint32_t bufferImageHeight       = defaultSize + 1u;
        params.src.buffer.size           = defaultSize * bufferImageHeight;
        params.dst.image.imageType       = VK_IMAGE_TYPE_2D;
        params.dst.image.format          = VK_FORMAT_R8G8B8A8_UNORM;
        params.dst.image.extent          = defaultExtent;
        params.dst.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
        params.dst.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        params.allocationKind            = testGroupParams->allocationKind;
        params.extensionFlags            = testGroupParams->extensionFlags;
        params.queueSelection            = testGroupParams->queueSelection;
        params.useSparseBinding          = testGroupParams->useSparseBinding;

        const VkBufferImageCopy bufferImageCopy = {
            0u,                 // VkDeviceSize bufferOffset;
            defaultSize,        // uint32_t bufferRowLength;
            bufferImageHeight,  // uint32_t bufferImageHeight;
            defaultSourceLayer, // VkImageSubresourceLayers imageSubresource;
            {0, 0, 0},          // VkOffset3D imageOffset;
            defaultHalfExtent   // VkExtent3D imageExtent;
        };
        CopyRegion copyRegion;
        copyRegion.bufferImageCopy = bufferImageCopy;

        params.regions.push_back(copyRegion);

        group->addChild(new CopyBufferToImageTestCase(testCtx, "larger_buffer", params));
    }

    {
        TestParams params;
        params.src.buffer.size           = (defaultHalfSize - 1u) * defaultSize + defaultHalfSize + defaultQuarterSize;
        params.dst.image.imageType       = VK_IMAGE_TYPE_2D;
        params.dst.image.format          = VK_FORMAT_R8G8B8A8_UNORM;
        params.dst.image.extent          = defaultExtent;
        params.dst.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
        params.dst.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        params.allocationKind            = testGroupParams->allocationKind;
        params.extensionFlags            = testGroupParams->extensionFlags;
        params.queueSelection            = testGroupParams->queueSelection;
        params.useSparseBinding          = testGroupParams->useSparseBinding;

        const VkBufferImageCopy bufferImageCopy = {
            defaultQuarterSize,                          // VkDeviceSize bufferOffset;
            defaultSize,                                 // uint32_t bufferRowLength;
            defaultSize,                                 // uint32_t bufferImageHeight;
            defaultSourceLayer,                          // VkImageSubresourceLayers imageSubresource;
            {defaultQuarterSize, defaultQuarterSize, 0}, // VkOffset3D imageOffset;
            defaultHalfExtent                            // VkExtent3D imageExtent;
        };
        CopyRegion copyRegion;
        copyRegion.bufferImageCopy = bufferImageCopy;

        params.regions.push_back(copyRegion);

        group->addChild(new CopyBufferToImageTestCase(testCtx, "tightly_sized_buffer_offset", params));
    }

    {
        TestParams params;
        uint32_t arrayLayers             = 16u;
        params.src.buffer.size           = defaultHalfSize * defaultHalfSize * arrayLayers;
        params.dst.image.imageType       = VK_IMAGE_TYPE_2D;
        params.dst.image.format          = VK_FORMAT_R8G8B8A8_UNORM;
        params.dst.image.extent          = defaultHalfExtent;
        params.dst.image.extent.depth    = arrayLayers;
        params.dst.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
        params.dst.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        params.allocationKind            = testGroupParams->allocationKind;
        params.extensionFlags            = testGroupParams->extensionFlags;
        params.queueSelection            = testGroupParams->queueSelection;
        params.useSparseBinding          = testGroupParams->useSparseBinding;

        const int pixelSize = tcu::getPixelSize(mapVkFormat(params.dst.image.format));
        for (uint32_t arrayLayerNdx = 0; arrayLayerNdx < arrayLayers; arrayLayerNdx++)
        {
            const VkDeviceSize offset               = defaultHalfSize * defaultHalfSize * pixelSize * arrayLayerNdx;
            const VkBufferImageCopy bufferImageCopy = {
                offset, // VkDeviceSize bufferOffset;
                0u,     // uint32_t bufferRowLength;
                0u,     // uint32_t bufferImageHeight;
                {
                    VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
                    0u,                        // uint32_t mipLevel;
                    arrayLayerNdx,             // uint32_t baseArrayLayer;
                    1u,                        // uint32_t layerCount;
                },                             // VkImageSubresourceLayers imageSubresource;
                {0, 0, 0},                     // VkOffset3D imageOffset;
                defaultHalfExtent              // VkExtent3D imageExtent;
            };
            CopyRegion copyRegion;
            copyRegion.bufferImageCopy = bufferImageCopy;

            params.regions.push_back(copyRegion);
        }
        group->addChild(new CopyBufferToImageTestCase(testCtx, "array", params));
    }

    {
        TestParams params;
        uint32_t arrayLayers             = 16u;
        uint32_t bufferImageHeight       = defaultHalfSize + 1u;
        params.src.buffer.size           = defaultHalfSize * bufferImageHeight * arrayLayers;
        params.dst.image.imageType       = VK_IMAGE_TYPE_2D;
        params.dst.image.format          = VK_FORMAT_R8G8B8A8_UNORM;
        params.dst.image.extent          = defaultHalfExtent;
        params.dst.image.extent.depth    = arrayLayers;
        params.dst.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
        params.dst.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        params.allocationKind            = testGroupParams->allocationKind;
        params.extensionFlags            = testGroupParams->extensionFlags;
        params.queueSelection            = testGroupParams->queueSelection;
        params.useSparseBinding          = testGroupParams->useSparseBinding;

        const int pixelSize = tcu::getPixelSize(mapVkFormat(params.dst.image.format));
        for (uint32_t arrayLayerNdx = 0; arrayLayerNdx < arrayLayers; arrayLayerNdx++)
        {
            const VkDeviceSize offset               = defaultHalfSize * bufferImageHeight * pixelSize * arrayLayerNdx;
            const VkBufferImageCopy bufferImageCopy = {
                offset,            // VkDeviceSize bufferOffset;
                defaultHalfSize,   // uint32_t bufferRowLength;
                bufferImageHeight, // uint32_t bufferImageHeight;
                {
                    VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
                    0u,                        // uint32_t mipLevel;
                    arrayLayerNdx,             // uint32_t baseArrayLayer;
                    1u,                        // uint32_t layerCount;
                },                             // VkImageSubresourceLayers imageSubresource;
                {0, 0, 0},                     // VkOffset3D imageOffset;
                defaultHalfExtent              // VkExtent3D imageExtent;
            };
            CopyRegion copyRegion;
            copyRegion.bufferImageCopy = bufferImageCopy;

            params.regions.push_back(copyRegion);
        }
        group->addChild(new CopyBufferToImageTestCase(testCtx, "array_larger_buffer", params));
    }

    {
        TestParams params;
        uint32_t arrayLayers             = 16u;
        params.src.buffer.size           = defaultHalfSize * defaultHalfSize * arrayLayers;
        params.dst.image.imageType       = VK_IMAGE_TYPE_2D;
        params.dst.image.format          = VK_FORMAT_R8G8B8A8_UNORM;
        params.dst.image.extent          = defaultHalfExtent;
        params.dst.image.extent.depth    = arrayLayers;
        params.dst.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
        params.dst.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        params.allocationKind            = testGroupParams->allocationKind;
        params.extensionFlags            = testGroupParams->extensionFlags;
        params.queueSelection            = testGroupParams->queueSelection;
        params.useSparseBinding          = testGroupParams->useSparseBinding;

        const int pixelSize = tcu::getPixelSize(mapVkFormat(params.dst.image.format));
        for (uint32_t arrayLayerNdx = 0; arrayLayerNdx < arrayLayers; arrayLayerNdx++)
        {
            const VkDeviceSize offset               = defaultHalfSize * defaultHalfSize * pixelSize * arrayLayerNdx;
            const VkBufferImageCopy bufferImageCopy = {
                offset,          // VkDeviceSize bufferOffset;
                defaultHalfSize, // uint32_t bufferRowLength;
                defaultHalfSize, // uint32_t bufferImageHeight;
                {
                    VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
                    0u,                        // uint32_t mipLevel;
                    arrayLayerNdx,             // uint32_t baseArrayLayer;
                    1u,                        // uint32_t layerCount;
                },                             // VkImageSubresourceLayers imageSubresource;
                {0, 0, 0},                     // VkOffset3D imageOffset;
                defaultHalfExtent              // VkExtent3D imageExtent;
            };
            CopyRegion copyRegion;
            copyRegion.bufferImageCopy = bufferImageCopy;

            params.regions.push_back(copyRegion);
        }
        group->addChild(new CopyBufferToImageTestCase(testCtx, "array_tightly_sized_buffer", params));
    }

    {
        TestParams params;
        const uint32_t baseLayer         = 0u;
        const uint32_t layerCount        = 16u;
        params.src.buffer.size           = defaultHalfSize * defaultHalfSize * layerCount;
        params.src.buffer.fillMode       = FILL_MODE_RED;
        params.dst.image.imageType       = VK_IMAGE_TYPE_2D;
        params.dst.image.format          = VK_FORMAT_R8G8B8A8_UNORM;
        params.dst.image.extent          = defaultHalfExtent;
        params.dst.image.extent.depth    = layerCount;
        params.dst.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
        params.dst.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        params.dst.image.fillMode        = FILL_MODE_RED;
        params.allocationKind            = testGroupParams->allocationKind;
        params.extensionFlags            = testGroupParams->extensionFlags;
        params.queueSelection            = testGroupParams->queueSelection;
        params.useSparseBinding          = testGroupParams->useSparseBinding;
        params.extensionFlags |= MAINTENANCE_5;

        const VkImageSubresourceLayers defaultLayer = {
            VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
            0u,                        // uint32_t mipLevel;
            baseLayer,                 // uint32_t baseArrayLayer;
            VK_REMAINING_ARRAY_LAYERS  // uint32_t layerCount;
        };

        const VkBufferImageCopy bufferImageCopy = {
            0,                // VkDeviceSize bufferOffset;
            0,                // uint32_t bufferRowLength;
            0,                // uint32_t bufferImageHeight;
            defaultLayer,     // VkImageSubresourceLayers imageSubresource;
            {0, 0, 0},        // VkOffset3D imageOffset;
            defaultHalfExtent // VkExtent3D imageExtent;
        };

        CopyRegion copyRegion;
        copyRegion.bufferImageCopy = bufferImageCopy;

        params.regions.push_back(copyRegion);

        group->addChild(new CopyBufferToImageTestCase(testCtx, "array_all_remaining_layers", params));
    }

    {
        TestParams params;
        const uint32_t baseLayer         = 2u;
        const uint32_t layerCount        = 16u;
        params.src.buffer.size           = defaultHalfSize * defaultHalfSize * layerCount;
        params.src.buffer.fillMode       = FILL_MODE_RED;
        params.dst.image.imageType       = VK_IMAGE_TYPE_2D;
        params.dst.image.format          = VK_FORMAT_R8G8B8A8_UNORM;
        params.dst.image.extent          = defaultHalfExtent;
        params.dst.image.extent.depth    = layerCount;
        params.dst.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
        params.dst.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        params.dst.image.fillMode        = FILL_MODE_RED;
        params.allocationKind            = testGroupParams->allocationKind;
        params.extensionFlags            = testGroupParams->extensionFlags;
        params.queueSelection            = testGroupParams->queueSelection;
        params.useSparseBinding          = testGroupParams->useSparseBinding;
        params.extensionFlags |= MAINTENANCE_5;

        const VkImageSubresourceLayers defaultLayer = {
            VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
            0u,                        // uint32_t mipLevel;
            baseLayer,                 // uint32_t baseArrayLayer;
            VK_REMAINING_ARRAY_LAYERS  // uint32_t layerCount;
        };

        const VkBufferImageCopy bufferImageCopy = {
            0,                // VkDeviceSize bufferOffset;
            0,                // uint32_t bufferRowLength;
            0,                // uint32_t bufferImageHeight;
            defaultLayer,     // VkImageSubresourceLayers imageSubresource;
            {0, 0, 0},        // VkOffset3D imageOffset;
            defaultHalfExtent // VkExtent3D imageExtent;
        };

        CopyRegion copyRegion;
        copyRegion.bufferImageCopy = bufferImageCopy;

        params.regions.push_back(copyRegion);

        group->addChild(new CopyBufferToImageTestCase(testCtx, "array_not_all_remaining_layers", params));
    }
}

void addBufferToBufferTests(tcu::TestCaseGroup *group, TestGroupParamsPtr testGroupParams)
{
    tcu::TestContext &testCtx = group->getTestContext();

    {
        TestParams params;
        params.src.buffer.size  = defaultSize;
        params.dst.buffer.size  = defaultSize;
        params.allocationKind   = testGroupParams->allocationKind;
        params.extensionFlags   = testGroupParams->extensionFlags;
        params.queueSelection   = testGroupParams->queueSelection;
        params.useSparseBinding = testGroupParams->useSparseBinding;

        const VkBufferCopy bufferCopy = {
            0u,          // VkDeviceSize srcOffset;
            0u,          // VkDeviceSize dstOffset;
            defaultSize, // VkDeviceSize size;
        };

        CopyRegion copyRegion;
        copyRegion.bufferCopy = bufferCopy;
        params.regions.push_back(copyRegion);

        group->addChild(new BufferToBufferTestCase(testCtx, "whole", params));
    }

    // Filter is VK_FILTER_NEAREST.
    {
        TestParams params;
        params.src.buffer.size  = defaultQuarterSize;
        params.dst.buffer.size  = defaultQuarterSize;
        params.allocationKind   = testGroupParams->allocationKind;
        params.extensionFlags   = testGroupParams->extensionFlags;
        params.queueSelection   = testGroupParams->queueSelection;
        params.useSparseBinding = testGroupParams->useSparseBinding;

        const VkBufferCopy bufferCopy = {
            12u, // VkDeviceSize srcOffset;
            4u,  // VkDeviceSize dstOffset;
            1u,  // VkDeviceSize size;
        };

        CopyRegion copyRegion;
        copyRegion.bufferCopy = bufferCopy;
        params.regions.push_back(copyRegion);

        group->addChild(new BufferToBufferTestCase(testCtx, "partial", params));
    }

    {
        const uint32_t size = 16;
        TestParams params;
        params.src.buffer.size  = size;
        params.dst.buffer.size  = size * (size + 1);
        params.allocationKind   = testGroupParams->allocationKind;
        params.extensionFlags   = testGroupParams->extensionFlags;
        params.queueSelection   = testGroupParams->queueSelection;
        params.useSparseBinding = testGroupParams->useSparseBinding;

        // Copy region with size 1..size
        for (unsigned int i = 1; i <= size; i++)
        {
            const VkBufferCopy bufferCopy = {
                0,        // VkDeviceSize srcOffset;
                i * size, // VkDeviceSize dstOffset;
                i,        // VkDeviceSize size;
            };

            CopyRegion copyRegion;
            copyRegion.bufferCopy = bufferCopy;
            params.regions.push_back(copyRegion);
        }

        group->addChild(new BufferToBufferTestCase(testCtx, "regions", params));
    }

    {
        TestParams params;
        params.src.buffer.size  = 32;
        params.dst.buffer.size  = 32;
        params.allocationKind   = testGroupParams->allocationKind;
        params.extensionFlags   = testGroupParams->extensionFlags;
        params.queueSelection   = testGroupParams->queueSelection;
        params.useSparseBinding = testGroupParams->useSparseBinding;

        // Copy four unaligned regions
        for (unsigned int i = 0; i < 4; i++)
        {
            const VkBufferCopy bufferCopy{
                3 + i * 3, // VkDeviceSize    srcOffset;    3  6   9  12
                1 + i * 5, // VkDeviceSize    dstOffset;    1  6  11  16
                2 + i,     // VkDeviceSize    size;        2  3   4   5
            };

            CopyRegion copyRegion;
            copyRegion.bufferCopy = bufferCopy;
            params.regions.push_back(copyRegion);
        }

        group->addChild(new BufferToBufferTestCase(testCtx, "unaligned_regions", params));
    }
}

void addBlittingImageSimpleTests(tcu::TestCaseGroup *group, TestParams &params)
{
    tcu::TestContext &testCtx = group->getTestContext();

    // Filter is VK_FILTER_NEAREST.
    {
        params.filter = VK_FILTER_NEAREST;

        params.dst.image.format = VK_FORMAT_R8G8B8A8_UNORM;
        group->addChild(new BlitImageTestCase(testCtx, "nearest", params));

        params.dst.image.format = VK_FORMAT_R32_SFLOAT;
        group->addChild(
            new BlitImageTestCase(testCtx, getFormatCaseName(params.dst.image.format) + "_nearest", params));

        params.dst.image.format = VK_FORMAT_B8G8R8A8_UNORM;
        group->addChild(
            new BlitImageTestCase(testCtx, getFormatCaseName(params.dst.image.format) + "_nearest", params));
    }

    // Filter is VK_FILTER_LINEAR.
    {
        params.filter = VK_FILTER_LINEAR;

        params.dst.image.format = VK_FORMAT_R8G8B8A8_UNORM;
        group->addChild(new BlitImageTestCase(testCtx, "linear", params));

        params.dst.image.format = VK_FORMAT_R32_SFLOAT;
        group->addChild(new BlitImageTestCase(testCtx, getFormatCaseName(params.dst.image.format) + "_linear", params));

        params.dst.image.format = VK_FORMAT_B8G8R8A8_UNORM;
        group->addChild(new BlitImageTestCase(testCtx, getFormatCaseName(params.dst.image.format) + "_linear", params));
    }

    // Filter is VK_FILTER_CUBIC_EXT.
    // Cubic filtering can only be used with 2D images.
    if (params.dst.image.imageType == VK_IMAGE_TYPE_2D)
    {
        params.filter = VK_FILTER_CUBIC_EXT;

        params.dst.image.format = VK_FORMAT_R8G8B8A8_UNORM;
        group->addChild(new BlitImageTestCase(testCtx, "cubic", params));

        params.dst.image.format = VK_FORMAT_R32_SFLOAT;
        group->addChild(new BlitImageTestCase(testCtx, getFormatCaseName(params.dst.image.format) + "_cubic", params));

        params.dst.image.format = VK_FORMAT_B8G8R8A8_UNORM;
        group->addChild(new BlitImageTestCase(testCtx, getFormatCaseName(params.dst.image.format) + "_cubic", params));
    }
}

void addBlittingImageSimpleWholeTests(tcu::TestCaseGroup *group, TestParams params)
{
    DE_ASSERT(params.src.image.imageType == params.dst.image.imageType);
    const int32_t imageDepth      = params.src.image.imageType == VK_IMAGE_TYPE_3D ? defaultSize : 1;
    params.src.image.extent       = defaultExtent;
    params.dst.image.extent       = defaultExtent;
    params.src.image.extent.depth = imageDepth;
    params.dst.image.extent.depth = imageDepth;

    {
        const VkImageBlit imageBlit = {
            defaultSourceLayer,                                  // VkImageSubresourceLayers srcSubresource;
            {{0, 0, 0}, {defaultSize, defaultSize, imageDepth}}, // VkOffset3D srcOffsets[2];

            defaultSourceLayer,                                 // VkImageSubresourceLayers dstSubresource;
            {{0, 0, 0}, {defaultSize, defaultSize, imageDepth}} // VkOffset3D dstOffset[2];
        };

        CopyRegion region;
        region.imageBlit = imageBlit;
        params.regions.push_back(region);
    }

    addBlittingImageSimpleTests(group, params);
}

void addBlittingImageArrayTests(tcu::TestCaseGroup *group, TestParams params)
{
    DE_ASSERT(params.src.image.imageType == params.dst.image.imageType);

    tcu::TestContext &testCtx = group->getTestContext();

    {
        const uint32_t baseLayer      = 0u;
        const uint32_t layerCount     = 16u;
        params.dst.image.format       = VK_FORMAT_R8G8B8A8_UNORM;
        params.src.image.extent       = defaultExtent;
        params.dst.image.extent       = defaultExtent;
        params.src.image.extent.depth = layerCount;
        params.dst.image.extent.depth = layerCount;
        params.filter                 = VK_FILTER_NEAREST;
        params.extensionFlags |= MAINTENANCE_5;

        const VkImageSubresourceLayers defaultLayer = {
            VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
            0u,                        // uint32_t mipLevel;
            baseLayer,                 // uint32_t baseArrayLayer;
            VK_REMAINING_ARRAY_LAYERS  // uint32_t layerCount;
        };

        const VkImageBlit imageBlit = {
            defaultLayer,                               // VkImageSubresourceLayers srcSubresource;
            {{0, 0, 0}, {defaultSize, defaultSize, 1}}, // VkOffset3D srcOffsets[2];

            defaultLayer,                              // VkImageSubresourceLayers dstSubresource;
            {{0, 0, 0}, {defaultSize, defaultSize, 1}} // VkOffset3D dstOffset[2];
        };

        CopyRegion region;
        region.imageBlit = imageBlit;

        params.regions.push_back(region);

        group->addChild(new BlitImageTestCase(testCtx, "all_remaining_layers", params));
    }

    params.regions.clear();

    {
        const uint32_t baseLayer      = 2u;
        const uint32_t layerCount     = 16u;
        params.dst.image.format       = VK_FORMAT_R8G8B8A8_UNORM;
        params.src.image.extent       = defaultExtent;
        params.dst.image.extent       = defaultExtent;
        params.src.image.extent.depth = layerCount;
        params.dst.image.extent.depth = layerCount;
        params.filter                 = VK_FILTER_NEAREST;
        params.extensionFlags |= MAINTENANCE_5;

        const VkImageSubresourceLayers defaultLayer = {
            VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
            0u,                        // uint32_t mipLevel;
            baseLayer,                 // uint32_t baseArrayLayer;
            VK_REMAINING_ARRAY_LAYERS  // uint32_t layerCount;
        };

        const VkImageBlit imageBlit = {
            defaultLayer,                               // VkImageSubresourceLayers srcSubresource;
            {{0, 0, 0}, {defaultSize, defaultSize, 1}}, // VkOffset3D srcOffsets[2];

            defaultLayer,                              // VkImageSubresourceLayers dstSubresource;
            {{0, 0, 0}, {defaultSize, defaultSize, 1}} // VkOffset3D dstOffset[2];
        };

        CopyRegion region;
        region.imageBlit = imageBlit;

        params.regions.push_back(region);

        group->addChild(new BlitImageTestCase(testCtx, "not_all_remaining_layers", params));
    }
}

void addBlittingImageSimpleMirrorXYTests(tcu::TestCaseGroup *group, TestParams params)
{
    DE_ASSERT(params.src.image.imageType == params.dst.image.imageType);
    const int32_t imageDepth      = params.src.image.imageType == VK_IMAGE_TYPE_3D ? defaultSize : 1;
    params.src.image.extent       = defaultExtent;
    params.dst.image.extent       = defaultExtent;
    params.src.image.extent.depth = imageDepth;
    params.dst.image.extent.depth = imageDepth;

    {
        const VkImageBlit imageBlit = {
            defaultSourceLayer,                                  // VkImageSubresourceLayers srcSubresource;
            {{0, 0, 0}, {defaultSize, defaultSize, imageDepth}}, // VkOffset3D srcOffsets[2];

            defaultSourceLayer,                                 // VkImageSubresourceLayers dstSubresource;
            {{defaultSize, defaultSize, 0}, {0, 0, imageDepth}} // VkOffset3D dstOffset[2];
        };

        CopyRegion region;
        region.imageBlit = imageBlit;
        params.regions.push_back(region);
    }

    addBlittingImageSimpleTests(group, params);
}

void addBlittingImageSimpleMirrorXTests(tcu::TestCaseGroup *group, TestParams params)
{
    DE_ASSERT(params.src.image.imageType == params.dst.image.imageType);
    const int32_t imageDepth      = params.src.image.imageType == VK_IMAGE_TYPE_3D ? defaultSize : 1;
    params.src.image.extent       = defaultExtent;
    params.dst.image.extent       = defaultExtent;
    params.src.image.extent.depth = imageDepth;
    params.dst.image.extent.depth = imageDepth;

    {
        const VkImageBlit imageBlit = {
            defaultSourceLayer,                                  // VkImageSubresourceLayers srcSubresource;
            {{0, 0, 0}, {defaultSize, defaultSize, imageDepth}}, // VkOffset3D srcOffsets[2];

            defaultSourceLayer,                                 // VkImageSubresourceLayers dstSubresource;
            {{defaultSize, 0, 0}, {0, defaultSize, imageDepth}} // VkOffset3D dstOffset[2];
        };

        CopyRegion region;
        region.imageBlit = imageBlit;
        params.regions.push_back(region);
    }

    addBlittingImageSimpleTests(group, params);
}

void addBlittingImageSimpleMirrorYTests(tcu::TestCaseGroup *group, TestParams params)
{
    DE_ASSERT(params.src.image.imageType == params.dst.image.imageType);
    const int32_t imageDepth      = params.src.image.imageType == VK_IMAGE_TYPE_3D ? defaultSize : 1;
    params.src.image.extent       = defaultExtent;
    params.dst.image.extent       = defaultExtent;
    params.src.image.extent.depth = imageDepth;
    params.dst.image.extent.depth = imageDepth;

    {
        const VkImageBlit imageBlit = {
            defaultSourceLayer,                                  // VkImageSubresourceLayers srcSubresource;
            {{0, 0, 0}, {defaultSize, defaultSize, imageDepth}}, // VkOffset3D srcOffsets[2];

            defaultSourceLayer,                                 // VkImageSubresourceLayers dstSubresource;
            {{0, defaultSize, 0}, {defaultSize, 0, imageDepth}} // VkOffset3D dstOffset[2];
        };

        CopyRegion region;
        region.imageBlit = imageBlit;
        params.regions.push_back(region);
    }

    addBlittingImageSimpleTests(group, params);
}

void addBlittingImageSimpleMirrorZTests(tcu::TestCaseGroup *group, TestParams params)
{
    DE_ASSERT(params.src.image.imageType == params.dst.image.imageType);
    DE_ASSERT(params.src.image.imageType == VK_IMAGE_TYPE_3D);
    params.src.image.extent       = defaultExtent;
    params.dst.image.extent       = defaultExtent;
    params.src.image.extent.depth = defaultSize;
    params.dst.image.extent.depth = defaultSize;

    {
        const VkImageBlit imageBlit = {
            defaultSourceLayer,                                   // VkImageSubresourceLayers srcSubresource;
            {{0, 0, 0}, {defaultSize, defaultSize, defaultSize}}, // VkOffset3D srcOffsets[2];

            defaultSourceLayer,                                  // VkImageSubresourceLayers dstSubresource;
            {{0, 0, defaultSize}, {defaultSize, defaultSize, 0}} // VkOffset3D dstOffset[2];
        };

        CopyRegion region;
        region.imageBlit = imageBlit;
        params.regions.push_back(region);
    }

    addBlittingImageSimpleTests(group, params);
}

void addBlittingImageSimpleMirrorSubregionsTests(tcu::TestCaseGroup *group, TestParams params)
{
    DE_ASSERT(params.src.image.imageType == params.dst.image.imageType);
    const int32_t imageDepth      = params.src.image.imageType == VK_IMAGE_TYPE_3D ? defaultSize : 1;
    params.src.image.extent       = defaultExtent;
    params.dst.image.extent       = defaultExtent;
    params.src.image.extent.depth = imageDepth;
    params.dst.image.extent.depth = imageDepth;

    // No mirroring.
    {
        const VkImageBlit imageBlit = {
            defaultSourceLayer,                                          // VkImageSubresourceLayers srcSubresource;
            {{0, 0, 0}, {defaultHalfSize, defaultHalfSize, imageDepth}}, // VkOffset3D srcOffsets[2];

            defaultSourceLayer,                                         // VkImageSubresourceLayers dstSubresource;
            {{0, 0, 0}, {defaultHalfSize, defaultHalfSize, imageDepth}} // VkOffset3D dstOffset[2];
        };

        CopyRegion region;
        region.imageBlit = imageBlit;
        params.regions.push_back(region);
    }

    // Flipping y coordinates.
    {
        const VkImageBlit imageBlit = {
            defaultSourceLayer, // VkImageSubresourceLayers srcSubresource;
            {{defaultHalfSize, 0, 0}, {defaultSize, defaultHalfSize, imageDepth}}, // VkOffset3D srcOffsets[2];

            defaultSourceLayer, // VkImageSubresourceLayers dstSubresource;
            {{defaultHalfSize, defaultHalfSize, 0}, {defaultSize, 0, imageDepth}} // VkOffset3D dstOffset[2];
        };
        CopyRegion region;
        region.imageBlit = imageBlit;
        params.regions.push_back(region);
    }

    // Flipping x coordinates.
    {
        const VkImageBlit imageBlit = {
            defaultSourceLayer, // VkImageSubresourceLayers srcSubresource;
            {{0, defaultHalfSize, 0}, {defaultHalfSize, defaultSize, imageDepth}}, // VkOffset3D srcOffsets[2];

            defaultSourceLayer, // VkImageSubresourceLayers dstSubresource;
            {{defaultHalfSize, defaultHalfSize, 0}, {0, defaultSize, imageDepth}} // VkOffset3D dstOffset[2];
        };

        CopyRegion region;
        region.imageBlit = imageBlit;
        params.regions.push_back(region);
    }

    // Flipping x and y coordinates.
    {
        const VkImageBlit imageBlit = {
            defaultSourceLayer, // VkImageSubresourceLayers srcSubresource;
            {{defaultHalfSize, defaultHalfSize, 0},
             {defaultSize, defaultSize, imageDepth}}, // VkOffset3D srcOffsets[2];

            defaultSourceLayer, // VkImageSubresourceLayers dstSubresource;
            {{defaultSize, defaultSize, 0}, {defaultHalfSize, defaultHalfSize, imageDepth}} // VkOffset3D dstOffset[2];
        };

        CopyRegion region;
        region.imageBlit = imageBlit;
        params.regions.push_back(region);
    }

    addBlittingImageSimpleTests(group, params);
}

void addBlittingImageSimpleScalingWhole1Tests(tcu::TestCaseGroup *group, TestParams params)
{
    DE_ASSERT(params.src.image.imageType == params.dst.image.imageType);
    const int32_t imageDepth      = params.src.image.imageType == VK_IMAGE_TYPE_3D ? defaultSize : 1;
    const int32_t halfImageDepth  = params.src.image.imageType == VK_IMAGE_TYPE_3D ? defaultHalfSize : 1;
    params.src.image.extent       = defaultExtent;
    params.dst.image.extent       = defaultHalfExtent;
    params.src.image.extent.depth = imageDepth;
    params.dst.image.extent.depth = halfImageDepth;

    {
        const VkImageBlit imageBlit = {
            defaultSourceLayer,                                  // VkImageSubresourceLayers srcSubresource;
            {{0, 0, 0}, {defaultSize, defaultSize, imageDepth}}, // VkOffset3D srcOffsets[2];

            defaultSourceLayer,                                             // VkImageSubresourceLayers dstSubresource;
            {{0, 0, 0}, {defaultHalfSize, defaultHalfSize, halfImageDepth}} // VkOffset3D dstOffset[2];
        };

        CopyRegion region;
        region.imageBlit = imageBlit;
        params.regions.push_back(region);
    }

    addBlittingImageSimpleTests(group, params);
}

void addBlittingImageSimpleScalingWhole2Tests(tcu::TestCaseGroup *group, TestParams params)
{
    DE_ASSERT(params.src.image.imageType == params.dst.image.imageType);
    const int32_t imageDepth      = params.src.image.imageType == VK_IMAGE_TYPE_3D ? defaultSize : 1;
    const int32_t halfImageDepth  = params.src.image.imageType == VK_IMAGE_TYPE_3D ? defaultHalfSize : 1;
    params.src.image.extent       = defaultHalfExtent;
    params.dst.image.extent       = defaultExtent;
    params.src.image.extent.depth = halfImageDepth;
    params.dst.image.extent.depth = imageDepth;

    {
        const VkImageBlit imageBlit = {
            defaultSourceLayer,                                              // VkImageSubresourceLayers srcSubresource;
            {{0, 0, 0}, {defaultHalfSize, defaultHalfSize, halfImageDepth}}, // VkOffset3D srcOffsets[2];

            defaultSourceLayer,                                 // VkImageSubresourceLayers dstSubresource;
            {{0, 0, 0}, {defaultSize, defaultSize, imageDepth}} // VkOffset3D dstOffset[2];
        };

        CopyRegion region;
        region.imageBlit = imageBlit;
        params.regions.push_back(region);
    }

    addBlittingImageSimpleTests(group, params);
}

void addBlittingImageSimpleScalingAndOffsetTests(tcu::TestCaseGroup *group, TestParams params)
{
    DE_ASSERT(params.src.image.imageType == params.dst.image.imageType);
    const int32_t imageDepth      = params.src.image.imageType == VK_IMAGE_TYPE_3D ? defaultSize : 1;
    const int32_t srcDepthOffset  = params.src.image.imageType == VK_IMAGE_TYPE_3D ? defaultQuarterSize : 0;
    const int32_t srcDepthSize    = params.src.image.imageType == VK_IMAGE_TYPE_3D ? defaultQuarterSize * 3 : 1;
    params.src.image.extent       = defaultExtent;
    params.dst.image.extent       = defaultExtent;
    params.src.image.extent.depth = imageDepth;
    params.dst.image.extent.depth = imageDepth;

    {
        const VkImageBlit imageBlit = {
            defaultSourceLayer, // VkImageSubresourceLayers srcSubresource;
            {{defaultQuarterSize, defaultQuarterSize, srcDepthOffset},
             {defaultQuarterSize * 3, defaultQuarterSize * 3, srcDepthSize}}, // VkOffset3D srcOffsets[2];

            defaultSourceLayer,                                 // VkImageSubresourceLayers dstSubresource;
            {{0, 0, 0}, {defaultSize, defaultSize, imageDepth}} // VkOffset3D dstOffset[2];
        };

        CopyRegion region;
        region.imageBlit = imageBlit;
        params.regions.push_back(region);
    }

    addBlittingImageSimpleTests(group, params);
}

void addBlittingImageSimpleWithoutScalingPartialTests(tcu::TestCaseGroup *group, TestParams params)
{
    DE_ASSERT(params.src.image.imageType == params.dst.image.imageType);
    const bool is3dBlit     = params.src.image.imageType == VK_IMAGE_TYPE_3D;
    params.src.image.extent = defaultExtent;
    params.dst.image.extent = defaultExtent;

    if (is3dBlit)
    {
        params.src.image.extent.depth = defaultSize;
        params.dst.image.extent.depth = defaultSize;
    }

    {
        CopyRegion region;
        for (int i = 0; i < defaultSize; i += defaultQuarterSize)
        {
            const VkImageBlit imageBlit = {
                defaultSourceLayer, // VkImageSubresourceLayers srcSubresource;
                {{defaultSize - defaultQuarterSize - i, defaultSize - defaultQuarterSize - i,
                  is3dBlit ? defaultSize - defaultQuarterSize - i : 0},
                 {defaultSize - i, defaultSize - i, is3dBlit ? defaultSize - i : 1}}, // VkOffset3D srcOffsets[2];

                defaultSourceLayer, // VkImageSubresourceLayers dstSubresource;
                {{i, i, is3dBlit ? i : 0},
                 {i + defaultQuarterSize, i + defaultQuarterSize,
                  is3dBlit ? i + defaultQuarterSize : 1}} // VkOffset3D dstOffset[2];
            };
            region.imageBlit = imageBlit;
            params.regions.push_back(region);
        }
    }

    addBlittingImageSimpleTests(group, params);
}

void addBlittingImageSimpleTests(tcu::TestCaseGroup *group, AllocationKind allocationKind, uint32_t extensionFlags)
{
    TestParams params;
    params.src.image.format          = VK_FORMAT_R8G8B8A8_UNORM;
    params.src.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
    params.src.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    params.dst.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
    params.dst.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    params.allocationKind            = allocationKind;
    params.extensionFlags            = extensionFlags;
    params.src.image.imageType       = VK_IMAGE_TYPE_2D;
    params.dst.image.imageType       = VK_IMAGE_TYPE_2D;
    addTestGroup(group, "whole", addBlittingImageSimpleWholeTests, params);
    addTestGroup(group, "array", addBlittingImageArrayTests, params);
    addTestGroup(group, "mirror_xy", addBlittingImageSimpleMirrorXYTests, params);
    addTestGroup(group, "mirror_x", addBlittingImageSimpleMirrorXTests, params);
    addTestGroup(group, "mirror_y", addBlittingImageSimpleMirrorYTests, params);
    addTestGroup(group, "mirror_subregions", addBlittingImageSimpleMirrorSubregionsTests, params);
    addTestGroup(group, "scaling_whole1", addBlittingImageSimpleScalingWhole1Tests, params);
    addTestGroup(group, "scaling_whole2", addBlittingImageSimpleScalingWhole2Tests, params);
    addTestGroup(group, "scaling_and_offset", addBlittingImageSimpleScalingAndOffsetTests, params);
    addTestGroup(group, "without_scaling_partial", addBlittingImageSimpleWithoutScalingPartialTests, params);

    params.src.image.imageType = VK_IMAGE_TYPE_3D;
    params.dst.image.imageType = VK_IMAGE_TYPE_3D;
    addTestGroup(group, "whole_3d", addBlittingImageSimpleWholeTests, params);
    addTestGroup(group, "mirror_xy_3d", addBlittingImageSimpleMirrorXYTests, params);
    addTestGroup(group, "mirror_x_3d", addBlittingImageSimpleMirrorXTests, params);
    addTestGroup(group, "mirror_y_3d", addBlittingImageSimpleMirrorYTests, params);
    addTestGroup(group, "mirror_z_3d", addBlittingImageSimpleMirrorZTests, params);
    addTestGroup(group, "mirror_subregions_3d", addBlittingImageSimpleMirrorSubregionsTests, params);
    addTestGroup(group, "scaling_whole1_3d", addBlittingImageSimpleScalingWhole1Tests, params);
    addTestGroup(group, "scaling_whole2_3d", addBlittingImageSimpleScalingWhole2Tests, params);
    addTestGroup(group, "scaling_and_offset_3d", addBlittingImageSimpleScalingAndOffsetTests, params);
    addTestGroup(group, "without_scaling_partial_3d", addBlittingImageSimpleWithoutScalingPartialTests, params);
}

enum FilterMaskBits
{
    FILTER_MASK_NEAREST = 0, // Always tested.
    FILTER_MASK_LINEAR  = (1u << 0),
    FILTER_MASK_CUBIC   = (1u << 1),
};

using FilterMask = uint32_t;

FilterMask makeFilterMask(bool onlyNearest, bool discardCubicFilter)
{
    FilterMask mask = FILTER_MASK_NEAREST;

    if (!onlyNearest)
    {
        mask |= FILTER_MASK_LINEAR;
        if (!discardCubicFilter)
            mask |= FILTER_MASK_CUBIC;
    }

    return mask;
}

struct BlitColorTestParams
{
    TestParams params;
    const VkFormat *compatibleFormats;
    FilterMask testFilters;
};

bool isAllowedBlittingAllFormatsColorSrcFormatTests(const BlitColorTestParams &testParams)
{
    bool result = true;

    if (testParams.params.allocationKind == ALLOCATION_KIND_DEDICATED)
    {
        DE_ASSERT(!dedicatedAllocationBlittingFormatsToTestSet.empty());

        result = de::contains(dedicatedAllocationBlittingFormatsToTestSet, testParams.params.dst.image.format) ||
                 de::contains(dedicatedAllocationBlittingFormatsToTestSet, testParams.params.src.image.format);
    }

    return result;
}

const VkFormat linearOtherImageFormatsToTest[] = {
    // From compatibleFormats8Bit
    VK_FORMAT_R4G4_UNORM_PACK8,
    VK_FORMAT_R8_SRGB,

    // From compatibleFormats16Bit
    VK_FORMAT_R4G4B4A4_UNORM_PACK16,
    VK_FORMAT_R16_SFLOAT,

    // From compatibleFormats24Bit
    VK_FORMAT_R8G8B8_UNORM,
    VK_FORMAT_B8G8R8_SRGB,

    // From compatibleFormats32Bit
    VK_FORMAT_R8G8B8A8_UNORM,
    VK_FORMAT_R32_SFLOAT,

    // From compatibleFormats48Bit
    VK_FORMAT_R16G16B16_UNORM,
    VK_FORMAT_R16G16B16_SFLOAT,

    // From compatibleFormats64Bit
    VK_FORMAT_R16G16B16A16_UNORM,
    VK_FORMAT_R64_SFLOAT,

    // From compatibleFormats96Bit
    VK_FORMAT_R32G32B32_UINT,
    VK_FORMAT_R32G32B32_SFLOAT,

    // From compatibleFormats128Bit
    VK_FORMAT_R32G32B32A32_UINT,
    VK_FORMAT_R64G64_SFLOAT,

    // From compatibleFormats192Bit
    VK_FORMAT_R64G64B64_UINT,
    VK_FORMAT_R64G64B64_SFLOAT,

    // From compatibleFormats256Bit
    VK_FORMAT_R64G64B64A64_UINT,
    VK_FORMAT_R64G64B64A64_SFLOAT,
};

std::string getBlitImageTilingLayoutCaseName(VkImageTiling tiling, VkImageLayout layout)
{
    switch (tiling)
    {
    case VK_IMAGE_TILING_OPTIMAL:
        return getImageLayoutCaseName(layout);
    case VK_IMAGE_TILING_LINEAR:
        return "linear";
    default:
        DE_ASSERT(false);
        return "";
    }
}

void addBlittingImageAllFormatsColorSrcFormatDstFormatTests(tcu::TestCaseGroup *group, BlitColorTestParams testParams)
{
    tcu::TestContext &testCtx = group->getTestContext();

    FormatSet linearOtherImageFormatsToTestSet;
    const int numOfOtherImageFormatsToTestFilter = DE_LENGTH_OF_ARRAY(linearOtherImageFormatsToTest);
    for (int otherImageFormatsIndex = 0; otherImageFormatsIndex < numOfOtherImageFormatsToTestFilter;
         ++otherImageFormatsIndex)
        linearOtherImageFormatsToTestSet.insert(linearOtherImageFormatsToTest[otherImageFormatsIndex]);

    const VkImageTiling blitSrcTilings[] = {
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_TILING_LINEAR,
    };
    const VkImageLayout blitSrcLayouts[] = {VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL};
    const VkImageTiling blitDstTilings[] = {
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_TILING_LINEAR,
    };
    const VkImageLayout blitDstLayouts[] = {VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL};

    for (int srcTilingNdx = 0u; srcTilingNdx < DE_LENGTH_OF_ARRAY(blitSrcTilings); ++srcTilingNdx)
    {
        testParams.params.src.image.tiling = blitSrcTilings[srcTilingNdx];

        for (int srcLayoutNdx = 0u; srcLayoutNdx < DE_LENGTH_OF_ARRAY(blitSrcLayouts); ++srcLayoutNdx)
        {
            testParams.params.src.image.operationLayout = blitSrcLayouts[srcLayoutNdx];

            // Don't bother testing VK_IMAGE_TILING_LINEAR + VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL as it's likely to be the same as VK_IMAGE_LAYOUT_GENERAL
            if (testParams.params.src.image.tiling == VK_IMAGE_TILING_LINEAR &&
                testParams.params.src.image.operationLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
                continue;

            for (int dstTilingNdx = 0u; dstTilingNdx < DE_LENGTH_OF_ARRAY(blitDstTilings); ++dstTilingNdx)
            {
                testParams.params.dst.image.tiling = blitDstTilings[dstTilingNdx];

                for (int dstLayoutNdx = 0u; dstLayoutNdx < DE_LENGTH_OF_ARRAY(blitDstLayouts); ++dstLayoutNdx)
                {
                    testParams.params.dst.image.operationLayout = blitDstLayouts[dstLayoutNdx];

                    // Don't bother testing VK_IMAGE_TILING_LINEAR + VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL as it's likely to be the same as VK_IMAGE_LAYOUT_GENERAL
                    if (testParams.params.dst.image.tiling == VK_IMAGE_TILING_LINEAR &&
                        testParams.params.dst.image.operationLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
                        continue;

                    if ((testParams.params.dst.image.tiling == VK_IMAGE_TILING_LINEAR &&
                         !de::contains(linearOtherImageFormatsToTestSet, testParams.params.src.image.format)) ||
                        (testParams.params.src.image.tiling == VK_IMAGE_TILING_LINEAR &&
                         !de::contains(linearOtherImageFormatsToTestSet, testParams.params.dst.image.format)))
                        continue;

                    testParams.params.filter = VK_FILTER_NEAREST;
                    const std::string testName =
                        getBlitImageTilingLayoutCaseName(testParams.params.src.image.tiling,
                                                         testParams.params.src.image.operationLayout) +
                        "_" +
                        getBlitImageTilingLayoutCaseName(testParams.params.dst.image.tiling,
                                                         testParams.params.dst.image.operationLayout);
                    group->addChild(new BlitImageTestCase(testCtx, testName + "_nearest", testParams.params));

                    if (testParams.testFilters & FILTER_MASK_LINEAR)
                    {
                        testParams.params.filter = VK_FILTER_LINEAR;
                        group->addChild(new BlitImageTestCase(testCtx, testName + "_linear", testParams.params));
                    }

                    if (testParams.testFilters & FILTER_MASK_CUBIC)
                    {
                        testParams.params.filter = VK_FILTER_CUBIC_EXT;
                        group->addChild(new BlitImageTestCase(testCtx, testName + "_cubic", testParams.params));
                    }

                    if ((testParams.params.src.image.imageType == VK_IMAGE_TYPE_3D) &&
                        !isCompressedFormat(testParams.params.src.image.format))
                    {
                        const struct
                        {
                            FillMode mode;
                            const char *name;
                        } modeList[] = {
                            {FILL_MODE_BLUE_RED_X, "x"},
                            {FILL_MODE_BLUE_RED_Y, "y"},
                            {FILL_MODE_BLUE_RED_Z, "z"},
                        };

                        auto otherParams                      = testParams;
                        otherParams.params.dst.image.fillMode = FILL_MODE_WHITE;

                        for (int i = 0; i < DE_LENGTH_OF_ARRAY(modeList); ++i)
                        {
                            otherParams.params.src.image.fillMode = modeList[i].mode;

                            otherParams.params.filter = VK_FILTER_LINEAR;
                            group->addChild(new BlitImageTestCase(
                                testCtx, testName + "_linear_stripes_" + modeList[i].name, otherParams.params));

                            otherParams.params.filter = VK_FILTER_NEAREST;
                            group->addChild(new BlitImageTestCase(
                                testCtx, testName + "_nearest_stripes_" + modeList[i].name, otherParams.params));
                        }
                    }
                }
            }
        }
    }
}

void addBlittingImageAllFormatsColorSrcFormatTests(tcu::TestCaseGroup *group, BlitColorTestParams testParams)
{
    VkFormat srcFormat = testParams.params.src.image.format;

    if (testParams.compatibleFormats)
    {
        for (int dstFormatIndex = 0; testParams.compatibleFormats[dstFormatIndex] != VK_FORMAT_UNDEFINED;
             ++dstFormatIndex)
        {
            testParams.params.dst.image.format = testParams.compatibleFormats[dstFormatIndex];
            if (!isSupportedByFramework(testParams.params.dst.image.format))
                continue;

            if (!isAllowedBlittingAllFormatsColorSrcFormatTests(testParams))
                continue;

            addTestGroup(group, getFormatCaseName(testParams.params.dst.image.format),
                         addBlittingImageAllFormatsColorSrcFormatDstFormatTests, testParams);
        }
    }

    // If testParams.compatibleFormats is nullptr, the destination format will be copied from the source format
    // When testParams.compatibleFormats is not nullptr but format is compressed we also need to add that format
    // as it is not on compatibleFormats list
    if (!testParams.compatibleFormats || isCompressedFormat(srcFormat))
    {
        testParams.params.dst.image.format = srcFormat;

        addTestGroup(group, getFormatCaseName(srcFormat), addBlittingImageAllFormatsColorSrcFormatDstFormatTests,
                     testParams);
    }
}

const VkFormat dedicatedAllocationBlittingFormatsToTest[] = {
    // compatibleFormatsUInts
    VK_FORMAT_R8_UINT,
    VK_FORMAT_R64G64B64A64_UINT,

    // compatibleFormatsSInts
    VK_FORMAT_R8_SINT,
    VK_FORMAT_R64G64B64A64_SINT,

    // compatibleFormatsFloats
    VK_FORMAT_R4G4_UNORM_PACK8,
    VK_FORMAT_E5B9G9R9_UFLOAT_PACK32,

    // compatibleFormatsSrgb
    VK_FORMAT_R8_SRGB,
    VK_FORMAT_A8B8G8R8_SRGB_PACK32,
};

// skip cubic filtering test for the following data formats
const FormatSet onlyNearestAndLinearFormatsToTest = {VK_FORMAT_A8B8G8R8_USCALED_PACK32,
                                                     VK_FORMAT_A8B8G8R8_SSCALED_PACK32, VK_FORMAT_A8B8G8R8_UINT_PACK32,
                                                     VK_FORMAT_A8B8G8R8_SINT_PACK32};

// astc formats have diferent block sizes and thus require diferent resolutions for images
enum class AstcImageSizeType
{
    SIZE_64_64 = 0,
    SIZE_60_64,
    SIZE_64_60,
    SIZE_60_60,
};

const std::map<VkFormat, AstcImageSizeType> astcSizes{
    {VK_FORMAT_ASTC_4x4_SRGB_BLOCK, AstcImageSizeType::SIZE_64_64},
    {VK_FORMAT_ASTC_4x4_UNORM_BLOCK, AstcImageSizeType::SIZE_64_64},
    {VK_FORMAT_ASTC_5x4_SRGB_BLOCK, AstcImageSizeType::SIZE_60_64},
    {VK_FORMAT_ASTC_5x4_UNORM_BLOCK, AstcImageSizeType::SIZE_60_64},
    {VK_FORMAT_ASTC_5x5_SRGB_BLOCK, AstcImageSizeType::SIZE_60_60},
    {VK_FORMAT_ASTC_5x5_UNORM_BLOCK, AstcImageSizeType::SIZE_60_60},
    {VK_FORMAT_ASTC_6x5_SRGB_BLOCK, AstcImageSizeType::SIZE_60_60},
    {VK_FORMAT_ASTC_6x5_UNORM_BLOCK, AstcImageSizeType::SIZE_60_60},
    {VK_FORMAT_ASTC_6x6_SRGB_BLOCK, AstcImageSizeType::SIZE_60_60},
    {VK_FORMAT_ASTC_6x6_UNORM_BLOCK, AstcImageSizeType::SIZE_60_60},
    {VK_FORMAT_ASTC_8x5_SRGB_BLOCK, AstcImageSizeType::SIZE_64_60},
    {VK_FORMAT_ASTC_8x5_UNORM_BLOCK, AstcImageSizeType::SIZE_64_60},
    {VK_FORMAT_ASTC_8x6_SRGB_BLOCK, AstcImageSizeType::SIZE_64_60},
    {VK_FORMAT_ASTC_8x6_UNORM_BLOCK, AstcImageSizeType::SIZE_64_60},
    {VK_FORMAT_ASTC_8x8_SRGB_BLOCK, AstcImageSizeType::SIZE_64_64},
    {VK_FORMAT_ASTC_8x8_UNORM_BLOCK, AstcImageSizeType::SIZE_64_64},
    {VK_FORMAT_ASTC_10x5_SRGB_BLOCK, AstcImageSizeType::SIZE_60_60},
    {VK_FORMAT_ASTC_10x5_UNORM_BLOCK, AstcImageSizeType::SIZE_60_60},
    {VK_FORMAT_ASTC_10x6_SRGB_BLOCK, AstcImageSizeType::SIZE_60_60},
    {VK_FORMAT_ASTC_10x6_UNORM_BLOCK, AstcImageSizeType::SIZE_60_60},
    {VK_FORMAT_ASTC_10x8_SRGB_BLOCK, AstcImageSizeType::SIZE_60_64},
    {VK_FORMAT_ASTC_10x8_UNORM_BLOCK, AstcImageSizeType::SIZE_60_64},
    {VK_FORMAT_ASTC_10x10_SRGB_BLOCK, AstcImageSizeType::SIZE_60_60},
    {VK_FORMAT_ASTC_10x10_UNORM_BLOCK, AstcImageSizeType::SIZE_60_60},
    {VK_FORMAT_ASTC_12x10_SRGB_BLOCK, AstcImageSizeType::SIZE_60_60},
    {VK_FORMAT_ASTC_12x10_UNORM_BLOCK, AstcImageSizeType::SIZE_60_60},
    {VK_FORMAT_ASTC_12x12_SRGB_BLOCK, AstcImageSizeType::SIZE_60_60},
    {VK_FORMAT_ASTC_12x12_UNORM_BLOCK, AstcImageSizeType::SIZE_60_60}};

std::vector<CopyRegion> create2DCopyRegions(int32_t srcWidth, int32_t srcHeight, int32_t dstWidth, int32_t dstHeight)
{
    CopyRegion region;
    std::vector<CopyRegion> regionsVector;

    int32_t fourthOfSrcWidth  = srcWidth / 4;
    int32_t fourthOfSrcHeight = srcHeight / 4;
    int32_t fourthOfDstWidth  = dstWidth / 4;
    int32_t fourthOfDstHeight = dstHeight / 4;

    // to the top of resulting image copy whole source image but with increasingly smaller sizes
    for (int i = 0, j = 1; (i + fourthOfDstWidth / j < dstWidth) && (fourthOfDstWidth > j); i += fourthOfDstWidth / j++)
    {
        region.imageBlit = {
            defaultSourceLayer,                    // VkImageSubresourceLayers srcSubresource;
            {{0, 0, 0}, {srcWidth, srcHeight, 1}}, // VkOffset3D srcOffsets[2];

            defaultSourceLayer, // VkImageSubresourceLayers dstSubresource;
            {{i, 0, 0}, {i + fourthOfDstWidth / j, fourthOfDstHeight / j, 1}} // VkOffset3D dstOffset[2];
        };
        regionsVector.push_back(region);
    }

    // to the bottom of resulting image copy parts of source image;
    for (int i = 0; i < 4; ++i)
    {
        int srcX = i * fourthOfSrcWidth;
        int srcY = i * fourthOfSrcHeight;
        int dstX = i * fourthOfDstWidth;

        region.imageBlit = {
            defaultSourceLayer, // VkImageSubresourceLayers srcSubresource;
            {{srcX, srcY, 0}, {srcX + fourthOfSrcWidth, srcY + fourthOfSrcHeight, 1}}, // VkOffset3D srcOffsets[2];

            defaultSourceLayer, // VkImageSubresourceLayers dstSubresource;
            {{dstX, 2 * fourthOfDstHeight, 0},
             {dstX + fourthOfDstWidth, 3 * fourthOfDstHeight, 1}} // VkOffset3D dstOffset[2];
        };

        regionsVector.push_back(region);
    }

    return regionsVector;
}

void addBufferToImageTests(tcu::TestCaseGroup *group, TestGroupParamsPtr testGroupParams)
{
    addTestGroup(group, "1d_images", add1dBufferToImageTests, testGroupParams);
    addTestGroup(group, "2d_images", add2dBufferToImageTests, testGroupParams);
}

void addImageToBufferTests(tcu::TestCaseGroup *group, TestGroupParamsPtr testGroupParams)
{
    addTestGroup(group, "1d_images", add1dImageToBufferTests, testGroupParams);
    addTestGroup(group, "2d_images", add2dImageToBufferTests, testGroupParams);
}

void addBlittingImageAllFormatsColorTests(tcu::TestCaseGroup *group, AllocationKind allocationKind,
                                          uint32_t extensionFlags)
{
    const struct
    {
        const VkFormat *sourceFormats;
        const VkFormat *destinationFormats;
        const bool onlyNearest;
    } colorImageFormatsToTestBlit[] = {
        {compatibleFormatsUInts, compatibleFormatsUInts, true},
        {compatibleFormatsSInts, compatibleFormatsSInts, true},
        {compatibleFormatsFloats, compatibleFormatsFloats, false},
        {compressedFormatsFloats, compatibleFormatsFloats, false},
        {compatibleFormatsSrgb, compatibleFormatsSrgb, false},
        {compressedFormatsSrgb, compatibleFormatsSrgb, false},
    };

    const int numOfColorImageFormatsToTest = DE_LENGTH_OF_ARRAY(colorImageFormatsToTestBlit);

    if (allocationKind == ALLOCATION_KIND_DEDICATED)
    {
        const int numOfColorImageFormatsToTestFilter = DE_LENGTH_OF_ARRAY(dedicatedAllocationBlittingFormatsToTest);
        for (int compatibleFormatsIndex = 0; compatibleFormatsIndex < numOfColorImageFormatsToTestFilter;
             ++compatibleFormatsIndex)
            dedicatedAllocationBlittingFormatsToTestSet.insert(
                dedicatedAllocationBlittingFormatsToTest[compatibleFormatsIndex]);
    }

    // 2D tests.
    {
        de::MovePtr<tcu::TestCaseGroup> subGroup(new tcu::TestCaseGroup(group->getTestContext(), "2d"));

        TestParams params;
        params.src.image.imageType = VK_IMAGE_TYPE_2D;
        params.dst.image.imageType = VK_IMAGE_TYPE_2D;
        params.src.image.extent    = defaultExtent;
        params.dst.image.extent    = defaultExtent;
        params.src.image.tiling    = VK_IMAGE_TILING_OPTIMAL;
        params.dst.image.tiling    = VK_IMAGE_TILING_OPTIMAL;
        params.allocationKind      = allocationKind;
        params.extensionFlags      = extensionFlags;

        // create all required copy regions
        const std::map<AstcImageSizeType, std::vector<CopyRegion>> imageRegions{
            {AstcImageSizeType::SIZE_64_64, create2DCopyRegions(64, 64, 64, 64)},
            {AstcImageSizeType::SIZE_60_64, create2DCopyRegions(60, 64, 60, 64)},
            {AstcImageSizeType::SIZE_64_60, create2DCopyRegions(64, 60, 64, 60)},
            {AstcImageSizeType::SIZE_60_60, create2DCopyRegions(60, 60, 60, 60)},
        };

        for (int compatibleFormatsIndex = 0; compatibleFormatsIndex < numOfColorImageFormatsToTest;
             ++compatibleFormatsIndex)
        {
            const VkFormat *sourceFormats      = colorImageFormatsToTestBlit[compatibleFormatsIndex].sourceFormats;
            const VkFormat *destinationFormats = colorImageFormatsToTestBlit[compatibleFormatsIndex].destinationFormats;
            const bool onlyNearest             = colorImageFormatsToTestBlit[compatibleFormatsIndex].onlyNearest;
            for (int srcFormatIndex = 0; sourceFormats[srcFormatIndex] != VK_FORMAT_UNDEFINED; ++srcFormatIndex)
            {
                VkFormat srcFormat      = sourceFormats[srcFormatIndex];
                params.src.image.format = srcFormat;

                const bool onlyNearestAndLinear =
                    de::contains(onlyNearestAndLinearFormatsToTest, params.src.image.format);

                // most of tests are using regions caluculated for 64x64 size but astc formats require custom regions
                params.regions = imageRegions.at(AstcImageSizeType::SIZE_64_64);
                if (isCompressedFormat(srcFormat) && isAstcFormat(mapVkCompressedFormat(srcFormat)))
                    params.regions = imageRegions.at(astcSizes.at(srcFormat));

                // use the fact that first region contains the size of full source image
                // and make source and destination the same size - this is needed for astc formats
                const VkOffset3D &srcImageSize = params.regions[0].imageBlit.srcOffsets[1];
                VkExtent3D &srcImageExtent     = params.src.image.extent;
                VkExtent3D &dstImageExtent     = params.dst.image.extent;
                srcImageExtent.width           = srcImageSize.x;
                srcImageExtent.height          = srcImageSize.y;
                dstImageExtent.width           = srcImageSize.x;
                dstImageExtent.height          = srcImageSize.y;

                BlitColorTestParams testParams{params, destinationFormats,
                                               makeFilterMask(onlyNearest, onlyNearestAndLinear)};

                addTestGroup(subGroup.get(), getFormatCaseName(params.src.image.format),
                             addBlittingImageAllFormatsColorSrcFormatTests, testParams);
            }
        }

        group->addChild(subGroup.release());
    }

    // 1D tests.
    {
        de::MovePtr<tcu::TestCaseGroup> subGroup(new tcu::TestCaseGroup(group->getTestContext(), "1d"));

        TestParams params;
        params.src.image.imageType = VK_IMAGE_TYPE_1D;
        params.dst.image.imageType = VK_IMAGE_TYPE_1D;
        params.src.image.extent    = default1dExtent;
        params.dst.image.extent    = default1dExtent;
        params.src.image.tiling    = VK_IMAGE_TILING_OPTIMAL;
        params.dst.image.tiling    = VK_IMAGE_TILING_OPTIMAL;
        params.allocationKind      = allocationKind;
        params.extensionFlags      = extensionFlags;

        CopyRegion region;
        for (int i = 0; i < defaultSize; i += defaultSize / 2)
        {
            const VkImageBlit imageBlit = {
                defaultSourceLayer,               // VkImageSubresourceLayers srcSubresource;
                {{0, 0, 0}, {defaultSize, 1, 1}}, // VkOffset3D srcOffsets[2];

                defaultSourceLayer,                         // VkImageSubresourceLayers dstSubresource;
                {{i, 0, 0}, {i + defaultQuarterSize, 1, 1}} // VkOffset3D dstOffset[2];
            };
            region.imageBlit = imageBlit;
            params.regions.push_back(region);
        }

        {
            const VkImageBlit imageBlit = {
                defaultSourceLayer,                      // VkImageSubresourceLayers srcSubresource;
                {{0, 0, 0}, {defaultQuarterSize, 1, 1}}, // VkOffset3D srcOffsets[2];

                defaultSourceLayer,                                          // VkImageSubresourceLayers dstSubresource;
                {{defaultQuarterSize, 0, 0}, {2 * defaultQuarterSize, 1, 1}} // VkOffset3D dstOffset[2];
            };
            region.imageBlit = imageBlit;
            params.regions.push_back(region);
        }

        for (int compatibleFormatsIndex = 0; compatibleFormatsIndex < numOfColorImageFormatsToTest;
             ++compatibleFormatsIndex)
        {
            const VkFormat *sourceFormats = colorImageFormatsToTestBlit[compatibleFormatsIndex].sourceFormats;
            const bool onlyNearest        = colorImageFormatsToTestBlit[compatibleFormatsIndex].onlyNearest;
            for (int srcFormatIndex = 0; sourceFormats[srcFormatIndex] != VK_FORMAT_UNDEFINED; ++srcFormatIndex)
            {
                params.src.image.format = sourceFormats[srcFormatIndex];
                if (!isSupportedByFramework(params.src.image.format))
                    continue;

                // Cubic filtering can only be used with 2D images.
                const bool onlyNearestAndLinear = true;

                BlitColorTestParams testParams{params, nullptr, makeFilterMask(onlyNearest, onlyNearestAndLinear)};

                addTestGroup(subGroup.get(), getFormatCaseName(params.src.image.format),
                             addBlittingImageAllFormatsColorSrcFormatTests, testParams);
            }
        }

        group->addChild(subGroup.release());
    }

    // 3D tests. Note we use smaller dimensions here for performance reasons.
    {
        de::MovePtr<tcu::TestCaseGroup> subGroup(new tcu::TestCaseGroup(group->getTestContext(), "3d"));

        TestParams params;
        params.src.image.imageType = VK_IMAGE_TYPE_3D;
        params.dst.image.imageType = VK_IMAGE_TYPE_3D;
        params.src.image.extent    = default3dExtent;
        params.dst.image.extent    = default3dExtent;
        params.src.image.tiling    = VK_IMAGE_TILING_OPTIMAL;
        params.dst.image.tiling    = VK_IMAGE_TILING_OPTIMAL;
        params.allocationKind      = allocationKind;
        params.extensionFlags      = extensionFlags;

        CopyRegion region;
        for (int i = 0, j = 1; (i + defaultSixteenthSize / j < defaultQuarterSize) && (defaultSixteenthSize > j);
             i += defaultSixteenthSize / j++)
        {
            const VkImageBlit imageBlit = {
                defaultSourceLayer, // VkImageSubresourceLayers srcSubresource;
                {{0, 0, 0}, {defaultQuarterSize, defaultQuarterSize, defaultQuarterSize}}, // VkOffset3D srcOffsets[2];

                defaultSourceLayer, // VkImageSubresourceLayers dstSubresource;
                {{i, 0, i},
                 {i + defaultSixteenthSize / j, defaultSixteenthSize / j,
                  i + defaultSixteenthSize / j}} // VkOffset3D dstOffset[2];
            };
            region.imageBlit = imageBlit;
            params.regions.push_back(region);
        }
        for (int i = 0; i < defaultQuarterSize; i += defaultSixteenthSize)
        {
            const VkImageBlit imageBlit = {
                defaultSourceLayer, // VkImageSubresourceLayers srcSubresource;
                {{i, i, i},
                 {i + defaultSixteenthSize, i + defaultSixteenthSize,
                  i + defaultSixteenthSize}}, // VkOffset3D srcOffsets[2];

                defaultSourceLayer, // VkImageSubresourceLayers dstSubresource;
                {{i, defaultQuarterSize / 2, i},
                 {i + defaultSixteenthSize, defaultQuarterSize / 2 + defaultSixteenthSize,
                  i + defaultSixteenthSize}} // VkOffset3D dstOffset[2];
            };
            region.imageBlit = imageBlit;
            params.regions.push_back(region);
        }

        for (int compatibleFormatsIndex = 0; compatibleFormatsIndex < numOfColorImageFormatsToTest;
             ++compatibleFormatsIndex)
        {
            const VkFormat *sourceFormats = colorImageFormatsToTestBlit[compatibleFormatsIndex].sourceFormats;
            const bool onlyNearest        = colorImageFormatsToTestBlit[compatibleFormatsIndex].onlyNearest;
            for (int srcFormatIndex = 0; sourceFormats[srcFormatIndex] != VK_FORMAT_UNDEFINED; ++srcFormatIndex)
            {
                params.src.image.format = sourceFormats[srcFormatIndex];
                if (!isSupportedByFramework(params.src.image.format))
                    continue;

                // Cubic filtering can only be used with 2D images.
                const bool onlyNearestAndLinear = true;

                BlitColorTestParams testParams{params, nullptr, makeFilterMask(onlyNearest, onlyNearestAndLinear)};

                addTestGroup(subGroup.get(), getFormatCaseName(params.src.image.format),
                             addBlittingImageAllFormatsColorSrcFormatTests, testParams);
            }
        }

        group->addChild(subGroup.release());
    }
}

void addBlittingImageAllFormatsDepthStencilFormatsTests(tcu::TestCaseGroup *group, TestParams params)
{
    const VkImageLayout blitSrcLayouts[] = {VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL};
    const VkImageLayout blitDstLayouts[] = {VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL};

    for (int srcLayoutNdx = 0u; srcLayoutNdx < DE_LENGTH_OF_ARRAY(blitSrcLayouts); ++srcLayoutNdx)
    {
        params.src.image.operationLayout = blitSrcLayouts[srcLayoutNdx];

        for (int dstLayoutNdx = 0u; dstLayoutNdx < DE_LENGTH_OF_ARRAY(blitDstLayouts); ++dstLayoutNdx)
        {
            params.dst.image.operationLayout = blitDstLayouts[dstLayoutNdx];
            params.filter                    = VK_FILTER_NEAREST;

            const std::string testName = getImageLayoutCaseName(params.src.image.operationLayout) + "_" +
                                         getImageLayoutCaseName(params.dst.image.operationLayout);

            group->addChild(new BlitImageTestCase(group->getTestContext(), testName + "_nearest", params));
        }
    }
}

void addBlittingImageAllFormatsDepthStencilTests(tcu::TestCaseGroup *group, AllocationKind allocationKind,
                                                 uint32_t extensionFlags)
{
    const VkFormat depthAndStencilFormats[] = {
        VK_FORMAT_D16_UNORM,         VK_FORMAT_X8_D24_UNORM_PACK32, VK_FORMAT_D32_SFLOAT,         VK_FORMAT_S8_UINT,
        VK_FORMAT_D16_UNORM_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT,   VK_FORMAT_D32_SFLOAT_S8_UINT,
    };

    const VkImageSubresourceLayers defaultDepthSourceLayer   = {VK_IMAGE_ASPECT_DEPTH_BIT, 0u, 0u, 1u};
    const VkImageSubresourceLayers defaultStencilSourceLayer = {VK_IMAGE_ASPECT_STENCIL_BIT, 0u, 0u, 1u};
    const VkImageSubresourceLayers defaultDSSourceLayer = {VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT, 0u,
                                                           0u, 1u};

    // 2D tests
    {
        de::MovePtr<tcu::TestCaseGroup> subGroup(new tcu::TestCaseGroup(group->getTestContext(), "2d"));

        for (int compatibleFormatsIndex = 0; compatibleFormatsIndex < DE_LENGTH_OF_ARRAY(depthAndStencilFormats);
             ++compatibleFormatsIndex)
        {
            TestParams params;
            params.src.image.imageType = VK_IMAGE_TYPE_2D;
            params.src.image.extent    = defaultExtent;
            params.src.image.tiling    = VK_IMAGE_TILING_OPTIMAL;
            params.src.image.format    = depthAndStencilFormats[compatibleFormatsIndex];
            params.dst.image.extent    = defaultExtent;
            params.dst.image.imageType = VK_IMAGE_TYPE_2D;
            params.dst.image.format    = params.src.image.format;
            params.dst.image.tiling    = VK_IMAGE_TILING_OPTIMAL;
            params.allocationKind      = allocationKind;
            params.extensionFlags      = extensionFlags;

            bool hasDepth   = tcu::hasDepthComponent(mapVkFormat(params.src.image.format).order);
            bool hasStencil = tcu::hasStencilComponent(mapVkFormat(params.src.image.format).order);

            CopyRegion region;
            for (int i = 0, j = 1; (i + defaultQuarterSize / j < defaultSize) && (defaultQuarterSize > j);
                 i += defaultQuarterSize / j++)
            {
                const VkOffset3D srcOffset0 = {0, 0, 0};
                const VkOffset3D srcOffset1 = {defaultSize, defaultSize, 1};
                const VkOffset3D dstOffset0 = {i, 0, 0};
                const VkOffset3D dstOffset1 = {i + defaultQuarterSize / j, defaultQuarterSize / j, 1};

                if (hasDepth)
                {
                    const VkImageBlit imageBlit = {
                        defaultDepthSourceLayer,  // VkImageSubresourceLayers srcSubresource;
                        {srcOffset0, srcOffset1}, // VkOffset3D srcOffsets[2];
                        defaultDepthSourceLayer,  // VkImageSubresourceLayers dstSubresource;
                        {dstOffset0, dstOffset1}, // VkOffset3D dstOffset[2];
                    };
                    region.imageBlit = imageBlit;
                    params.regions.push_back(region);
                }
                if (hasStencil)
                {
                    const VkImageBlit imageBlit = {
                        defaultStencilSourceLayer, // VkImageSubresourceLayers srcSubresource;
                        {srcOffset0, srcOffset1},  // VkOffset3D srcOffsets[2];
                        defaultStencilSourceLayer, // VkImageSubresourceLayers dstSubresource;
                        {dstOffset0, dstOffset1},  // VkOffset3D dstOffset[2];
                    };
                    region.imageBlit = imageBlit;
                    params.regions.push_back(region);
                }
            }
            for (int i = 0; i < defaultSize; i += defaultQuarterSize)
            {
                const VkOffset3D srcOffset0 = {i, i, 0};
                const VkOffset3D srcOffset1 = {i + defaultQuarterSize, i + defaultQuarterSize, 1};
                const VkOffset3D dstOffset0 = {i, defaultSize / 2, 0};
                const VkOffset3D dstOffset1 = {i + defaultQuarterSize, defaultSize / 2 + defaultQuarterSize, 1};

                if (hasDepth)
                {
                    const VkImageBlit imageBlit = {
                        defaultDepthSourceLayer,  // VkImageSubresourceLayers srcSubresource;
                        {srcOffset0, srcOffset1}, // VkOffset3D srcOffsets[2];
                        defaultDepthSourceLayer,  // VkImageSubresourceLayers dstSubresource;
                        {dstOffset0, dstOffset1}  // VkOffset3D dstOffset[2];
                    };
                    region.imageBlit = imageBlit;
                    params.regions.push_back(region);
                }
                if (hasStencil)
                {
                    const VkImageBlit imageBlit = {
                        defaultStencilSourceLayer, // VkImageSubresourceLayers srcSubresource;
                        {srcOffset0, srcOffset1},  // VkOffset3D srcOffsets[2];
                        defaultStencilSourceLayer, // VkImageSubresourceLayers dstSubresource;
                        {dstOffset0, dstOffset1}   // VkOffset3D dstOffset[2];
                    };
                    region.imageBlit = imageBlit;
                    params.regions.push_back(region);
                }
                if (hasDepth && hasStencil)
                {
                    const VkOffset3D dstDSOffset0 = {i, 3 * defaultQuarterSize, 0};
                    const VkOffset3D dstDSOffset1 = {i + defaultQuarterSize, defaultSize, 1};
                    const VkImageBlit imageBlit   = {
                        defaultDSSourceLayer,        // VkImageSubresourceLayers srcSubresource;
                        {srcOffset0, srcOffset1},    // VkOffset3D srcOffsets[2];
                        defaultDSSourceLayer,        // VkImageSubresourceLayers dstSubresource;
                        {dstDSOffset0, dstDSOffset1} // VkOffset3D dstOffset[2];
                    };
                    region.imageBlit = imageBlit;
                    params.regions.push_back(region);
                }
            }

            const std::string testName =
                getFormatCaseName(params.src.image.format) + "_" + getFormatCaseName(params.dst.image.format);
            addTestGroup(subGroup.get(), testName, addBlittingImageAllFormatsDepthStencilFormatsTests, params);

            if (hasDepth && hasStencil)
            {
                params.extensionFlags |= SEPARATE_DEPTH_STENCIL_LAYOUT;
                const std::string testName2 = getFormatCaseName(params.src.image.format) + "_" +
                                              getFormatCaseName(params.dst.image.format) + "_separate_layouts";
                addTestGroup(subGroup.get(), testName2, addBlittingImageAllFormatsDepthStencilFormatsTests, params);
            }
        }

        group->addChild(subGroup.release());
    }

    // 1D tests
    {
        de::MovePtr<tcu::TestCaseGroup> subGroup(new tcu::TestCaseGroup(group->getTestContext(), "1d"));

        for (int compatibleFormatsIndex = 0; compatibleFormatsIndex < DE_LENGTH_OF_ARRAY(depthAndStencilFormats);
             ++compatibleFormatsIndex)
        {
            TestParams params;
            params.src.image.imageType = VK_IMAGE_TYPE_1D;
            params.dst.image.imageType = VK_IMAGE_TYPE_1D;
            params.src.image.extent    = default1dExtent;
            params.dst.image.extent    = default1dExtent;
            params.src.image.format    = depthAndStencilFormats[compatibleFormatsIndex];
            params.dst.image.format    = params.src.image.format;
            params.src.image.tiling    = VK_IMAGE_TILING_OPTIMAL;
            params.dst.image.tiling    = VK_IMAGE_TILING_OPTIMAL;
            params.allocationKind      = allocationKind;
            params.extensionFlags      = extensionFlags;

            bool hasDepth   = tcu::hasDepthComponent(mapVkFormat(params.src.image.format).order);
            bool hasStencil = tcu::hasStencilComponent(mapVkFormat(params.src.image.format).order);

            CopyRegion region;
            for (int i = 0; i < defaultSize; i += defaultSize / 2)
            {
                const VkOffset3D srcOffset0 = {0, 0, 0};
                const VkOffset3D srcOffset1 = {defaultSize, 1, 1};
                const VkOffset3D dstOffset0 = {i, 0, 0};
                const VkOffset3D dstOffset1 = {i + defaultQuarterSize, 1, 1};

                if (hasDepth)
                {
                    const VkImageBlit imageBlit = {
                        defaultDepthSourceLayer,  // VkImageSubresourceLayers srcSubresource;
                        {srcOffset0, srcOffset1}, // VkOffset3D srcOffsets[2];
                        defaultDepthSourceLayer,  // VkImageSubresourceLayers dstSubresource;
                        {dstOffset0, dstOffset1}, // VkOffset3D dstOffset[2];
                    };
                    region.imageBlit = imageBlit;
                    params.regions.push_back(region);
                }
                if (hasStencil)
                {
                    const VkImageBlit imageBlit = {
                        defaultStencilSourceLayer, // VkImageSubresourceLayers srcSubresource;
                        {srcOffset0, srcOffset1},  // VkOffset3D srcOffsets[2];
                        defaultStencilSourceLayer, // VkImageSubresourceLayers dstSubresource;
                        {dstOffset0, dstOffset1},  // VkOffset3D dstOffset[2];
                    };
                    region.imageBlit = imageBlit;
                    params.regions.push_back(region);
                }
            }

            {
                const VkOffset3D srcOffset0 = {0, 0, 0};
                const VkOffset3D srcOffset1 = {defaultQuarterSize, 1, 1};
                const VkOffset3D dstOffset0 = {defaultQuarterSize, 0, 0};
                const VkOffset3D dstOffset1 = {2 * defaultQuarterSize, 1, 1};

                if (hasDepth)
                {
                    const VkImageBlit imageBlit = {
                        defaultDepthSourceLayer,  // VkImageSubresourceLayers srcSubresource;
                        {srcOffset0, srcOffset1}, // VkOffset3D srcOffsets[2];
                        defaultDepthSourceLayer,  // VkImageSubresourceLayers dstSubresource;
                        {dstOffset0, dstOffset1}  // VkOffset3D dstOffset[2];
                    };
                    region.imageBlit = imageBlit;
                    params.regions.push_back(region);
                }
                if (hasStencil)
                {
                    const VkImageBlit imageBlit = {
                        defaultStencilSourceLayer, // VkImageSubresourceLayers srcSubresource;
                        {srcOffset0, srcOffset1},  // VkOffset3D srcOffsets[2];
                        defaultStencilSourceLayer, // VkImageSubresourceLayers dstSubresource;
                        {dstOffset0, dstOffset1}   // VkOffset3D dstOffset[2];
                    };
                    region.imageBlit = imageBlit;
                    params.regions.push_back(region);
                }
                if (hasDepth && hasStencil)
                {
                    const VkOffset3D dstDSOffset0 = {3 * defaultQuarterSize, 0, 0};
                    const VkOffset3D dstDSOffset1 = {3 * defaultQuarterSize + defaultQuarterSize / 2, 1, 1};
                    const VkImageBlit imageBlit   = {
                        defaultDSSourceLayer,        // VkImageSubresourceLayers srcSubresource;
                        {srcOffset0, srcOffset1},    // VkOffset3D srcOffsets[2];
                        defaultDSSourceLayer,        // VkImageSubresourceLayers dstSubresource;
                        {dstDSOffset0, dstDSOffset1} // VkOffset3D dstOffset[2];
                    };
                    region.imageBlit = imageBlit;
                    params.regions.push_back(region);
                }
            }

            const std::string testName =
                getFormatCaseName(params.src.image.format) + "_" + getFormatCaseName(params.dst.image.format);
            addTestGroup(subGroup.get(), testName, addBlittingImageAllFormatsDepthStencilFormatsTests, params);

            if (hasDepth && hasStencil)
            {
                params.extensionFlags |= SEPARATE_DEPTH_STENCIL_LAYOUT;
                const std::string testName2 = getFormatCaseName(params.src.image.format) + "_" +
                                              getFormatCaseName(params.dst.image.format) + "_separate_layouts";
                addTestGroup(subGroup.get(), testName2, addBlittingImageAllFormatsDepthStencilFormatsTests, params);
            }
        }

        group->addChild(subGroup.release());
    }

    // 3D tests. Note we use smaller dimensions here for performance reasons.
    {
        de::MovePtr<tcu::TestCaseGroup> subGroup(new tcu::TestCaseGroup(group->getTestContext(), "3d"));

        for (int compatibleFormatsIndex = 0; compatibleFormatsIndex < DE_LENGTH_OF_ARRAY(depthAndStencilFormats);
             ++compatibleFormatsIndex)
        {
            TestParams params;
            params.src.image.imageType = VK_IMAGE_TYPE_3D;
            params.dst.image.imageType = VK_IMAGE_TYPE_3D;
            params.src.image.extent    = default3dExtent;
            params.dst.image.extent    = default3dExtent;
            params.src.image.format    = depthAndStencilFormats[compatibleFormatsIndex];
            params.dst.image.format    = params.src.image.format;
            params.src.image.tiling    = VK_IMAGE_TILING_OPTIMAL;
            params.dst.image.tiling    = VK_IMAGE_TILING_OPTIMAL;
            params.allocationKind      = allocationKind;
            params.extensionFlags      = extensionFlags;

            bool hasDepth   = tcu::hasDepthComponent(mapVkFormat(params.src.image.format).order);
            bool hasStencil = tcu::hasStencilComponent(mapVkFormat(params.src.image.format).order);

            CopyRegion region;
            for (int i = 0, j = 1; (i + defaultSixteenthSize / j < defaultQuarterSize) && (defaultSixteenthSize > j);
                 i += defaultSixteenthSize / j++)
            {
                const VkOffset3D srcOffset0 = {0, 0, 0};
                const VkOffset3D srcOffset1 = {defaultQuarterSize, defaultQuarterSize, defaultQuarterSize};
                const VkOffset3D dstOffset0 = {i, 0, i};
                const VkOffset3D dstOffset1 = {i + defaultSixteenthSize / j, defaultSixteenthSize / j,
                                               i + defaultSixteenthSize / j};

                if (hasDepth)
                {
                    const VkImageBlit imageBlit = {
                        defaultDepthSourceLayer,  // VkImageSubresourceLayers srcSubresource;
                        {srcOffset0, srcOffset1}, // VkOffset3D srcOffsets[2];
                        defaultDepthSourceLayer,  // VkImageSubresourceLayers dstSubresource;
                        {dstOffset0, dstOffset1}, // VkOffset3D dstOffset[2];
                    };
                    region.imageBlit = imageBlit;
                    params.regions.push_back(region);
                }
                if (hasStencil)
                {
                    const VkImageBlit imageBlit = {
                        defaultStencilSourceLayer, // VkImageSubresourceLayers srcSubresource;
                        {srcOffset0, srcOffset1},  // VkOffset3D srcOffsets[2];
                        defaultStencilSourceLayer, // VkImageSubresourceLayers dstSubresource;
                        {dstOffset0, dstOffset1},  // VkOffset3D dstOffset[2];
                    };
                    region.imageBlit = imageBlit;
                    params.regions.push_back(region);
                }
            }
            for (int i = 0; i < defaultQuarterSize; i += defaultSixteenthSize)
            {
                const VkOffset3D srcOffset0 = {i, i, i};
                const VkOffset3D srcOffset1 = {i + defaultSixteenthSize, i + defaultSixteenthSize,
                                               i + defaultSixteenthSize};
                const VkOffset3D dstOffset0 = {i, defaultQuarterSize / 2, i};
                const VkOffset3D dstOffset1 = {i + defaultSixteenthSize, defaultQuarterSize / 2 + defaultSixteenthSize,
                                               i + defaultSixteenthSize};

                if (hasDepth)
                {
                    const VkImageBlit imageBlit = {
                        defaultDepthSourceLayer,  // VkImageSubresourceLayers srcSubresource;
                        {srcOffset0, srcOffset1}, // VkOffset3D srcOffsets[2];
                        defaultDepthSourceLayer,  // VkImageSubresourceLayers dstSubresource;
                        {dstOffset0, dstOffset1}  // VkOffset3D dstOffset[2];
                    };
                    region.imageBlit = imageBlit;
                    params.regions.push_back(region);
                }
                if (hasStencil)
                {
                    const VkImageBlit imageBlit = {
                        defaultStencilSourceLayer, // VkImageSubresourceLayers srcSubresource;
                        {srcOffset0, srcOffset1},  // VkOffset3D srcOffsets[2];
                        defaultStencilSourceLayer, // VkImageSubresourceLayers dstSubresource;
                        {dstOffset0, dstOffset1}   // VkOffset3D dstOffset[2];
                    };
                    region.imageBlit = imageBlit;
                    params.regions.push_back(region);
                }
                if (hasDepth && hasStencil)
                {
                    const VkOffset3D dstDSOffset0 = {i, 3 * defaultSixteenthSize, i};
                    const VkOffset3D dstDSOffset1 = {i + defaultSixteenthSize, defaultQuarterSize,
                                                     i + defaultSixteenthSize};
                    const VkImageBlit imageBlit   = {
                        defaultDSSourceLayer,        // VkImageSubresourceLayers srcSubresource;
                        {srcOffset0, srcOffset1},    // VkOffset3D srcOffsets[2];
                        defaultDSSourceLayer,        // VkImageSubresourceLayers dstSubresource;
                        {dstDSOffset0, dstDSOffset1} // VkOffset3D dstOffset[2];
                    };
                    region.imageBlit = imageBlit;
                    params.regions.push_back(region);
                }
            }

            const std::string testName =
                getFormatCaseName(params.src.image.format) + "_" + getFormatCaseName(params.dst.image.format);
            addTestGroup(subGroup.get(), testName, addBlittingImageAllFormatsDepthStencilFormatsTests, params);

            if (hasDepth && hasStencil)
            {
                params.extensionFlags |= SEPARATE_DEPTH_STENCIL_LAYOUT;
                const std::string testName2 = getFormatCaseName(params.src.image.format) + "_" +
                                              getFormatCaseName(params.dst.image.format) + "_separate_layouts";
                addTestGroup(subGroup.get(), testName2, addBlittingImageAllFormatsDepthStencilFormatsTests, params);
            }
        }

        group->addChild(subGroup.release());
    }
}

void addBlittingImageAllFormatsMipmapFormatTests(tcu::TestCaseGroup *group, BlitColorTestParams testParams)
{
    tcu::TestContext &testCtx = group->getTestContext();

    const VkImageLayout blitSrcLayouts[] = {VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL};
    const VkImageLayout blitDstLayouts[] = {VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL};

    for (int srcLayoutNdx = 0u; srcLayoutNdx < DE_LENGTH_OF_ARRAY(blitSrcLayouts); ++srcLayoutNdx)
    {
        testParams.params.src.image.operationLayout = blitSrcLayouts[srcLayoutNdx];
        for (int dstLayoutNdx = 0u; dstLayoutNdx < DE_LENGTH_OF_ARRAY(blitDstLayouts); ++dstLayoutNdx)
        {
            testParams.params.dst.image.operationLayout = blitDstLayouts[dstLayoutNdx];

            testParams.params.filter   = VK_FILTER_NEAREST;
            const std::string testName = getImageLayoutCaseName(testParams.params.src.image.operationLayout) + "_" +
                                         getImageLayoutCaseName(testParams.params.dst.image.operationLayout);
            group->addChild(new BlitMipmapTestCase(testCtx, testName + "_nearest", testParams.params));

            if (testParams.testFilters & FILTER_MASK_LINEAR)
            {
                testParams.params.filter = VK_FILTER_LINEAR;
                group->addChild(new BlitMipmapTestCase(testCtx, testName + "_linear", testParams.params));
            }

            if (testParams.testFilters & FILTER_MASK_CUBIC)
            {
                testParams.params.filter = VK_FILTER_CUBIC_EXT;
                group->addChild(new BlitMipmapTestCase(testCtx, testName + "_cubic", testParams.params));
            }
        }
    }
}

void addBlittingImageAllFormatsBaseLevelMipmapTests(tcu::TestCaseGroup *group, AllocationKind allocationKind,
                                                    uint32_t extensionFlags)
{
    const struct
    {
        const VkFormat *const compatibleFormats;
        const bool onlyNearest;
    } colorImageFormatsToTestBlit[] = {
        {compatibleFormatsUInts, true},
        {compatibleFormatsSInts, true},
        {compatibleFormatsFloats, false},
        {compatibleFormatsSrgb, false},
    };

    const int numOfColorImageFormatsToTest = DE_LENGTH_OF_ARRAY(colorImageFormatsToTestBlit);

    const int layerCountsToTest[] = {1, 6};

    TestParams params;
    params.src.image.imageType = VK_IMAGE_TYPE_2D;
    params.src.image.extent    = defaultExtent;
    params.src.image.tiling    = VK_IMAGE_TILING_OPTIMAL;
    params.dst.image.imageType = VK_IMAGE_TYPE_2D;
    params.dst.image.extent    = defaultExtent;
    params.dst.image.tiling    = VK_IMAGE_TILING_OPTIMAL;
    params.allocationKind      = allocationKind;
    params.extensionFlags      = extensionFlags;
    params.mipLevels           = deLog2Floor32(deMaxu32(defaultExtent.width, defaultExtent.height)) + 1u;
    params.singleCommand       = true;

    CopyRegion region;
    for (uint32_t mipLevelNdx = 0u; mipLevelNdx < params.mipLevels; mipLevelNdx++)
    {
        VkImageSubresourceLayers destLayer = defaultSourceLayer;
        destLayer.mipLevel                 = mipLevelNdx;

        const VkImageBlit imageBlit = {
            defaultSourceLayer,                         // VkImageSubresourceLayers srcSubresource;
            {{0, 0, 0}, {defaultSize, defaultSize, 1}}, // VkOffset3D srcOffsets[2];

            destLayer, // VkImageSubresourceLayers dstSubresource;
            {{0, 0, 0}, {defaultSize >> mipLevelNdx, defaultSize >> mipLevelNdx, 1}} // VkOffset3D dstOffset[2];
        };
        region.imageBlit = imageBlit;
        params.regions.push_back(region);
    }

    if (allocationKind == ALLOCATION_KIND_DEDICATED)
    {
        const int numOfColorImageFormatsToTestFilter = DE_LENGTH_OF_ARRAY(dedicatedAllocationBlittingFormatsToTest);
        for (int compatibleFormatsIndex = 0; compatibleFormatsIndex < numOfColorImageFormatsToTestFilter;
             ++compatibleFormatsIndex)
            dedicatedAllocationBlittingFormatsToTestSet.insert(
                dedicatedAllocationBlittingFormatsToTest[compatibleFormatsIndex]);
    }

    for (int layerCountIndex = 0; layerCountIndex < DE_LENGTH_OF_ARRAY(layerCountsToTest); layerCountIndex++)
    {
        const int layerCount             = layerCountsToTest[layerCountIndex];
        const std::string layerGroupName = "layercount_" + de::toString(layerCount);

        de::MovePtr<tcu::TestCaseGroup> layerCountGroup(
            new tcu::TestCaseGroup(group->getTestContext(), layerGroupName.c_str()));

        for (int compatibleFormatsIndex = 0; compatibleFormatsIndex < numOfColorImageFormatsToTest;
             ++compatibleFormatsIndex)
        {
            const VkFormat *compatibleFormats = colorImageFormatsToTestBlit[compatibleFormatsIndex].compatibleFormats;
            const bool onlyNearest            = colorImageFormatsToTestBlit[compatibleFormatsIndex].onlyNearest;

            for (int srcFormatIndex = 0; compatibleFormats[srcFormatIndex] != VK_FORMAT_UNDEFINED; ++srcFormatIndex)
            {
                params.src.image.format = compatibleFormats[srcFormatIndex];
                params.dst.image.format = compatibleFormats[srcFormatIndex];

                if (!isSupportedByFramework(params.src.image.format))
                    continue;

                const bool onlyNearestAndLinear =
                    de::contains(onlyNearestAndLinearFormatsToTest, params.src.image.format);

                BlitColorTestParams testParams;
                testParams.params            = params;
                testParams.compatibleFormats = compatibleFormats;
                testParams.testFilters       = makeFilterMask(onlyNearest, onlyNearestAndLinear);

                testParams.params.src.image.extent.depth = layerCount;
                testParams.params.dst.image.extent.depth = layerCount;

                for (size_t regionNdx = 0; regionNdx < testParams.params.regions.size(); regionNdx++)
                {
                    testParams.params.regions[regionNdx].imageBlit.srcSubresource.layerCount = layerCount;
                    testParams.params.regions[regionNdx].imageBlit.dstSubresource.layerCount = layerCount;
                }

                addTestGroup(layerCountGroup.get(), getFormatCaseName(params.src.image.format),
                             addBlittingImageAllFormatsMipmapFormatTests, testParams);
            }
        }
        group->addChild(layerCountGroup.release());
    }
}

void addBlittingImageAllFormatsPreviousLevelMipmapTests(tcu::TestCaseGroup *group, AllocationKind allocationKind,
                                                        uint32_t extensionFlags)
{
    const struct
    {
        const VkFormat *const compatibleFormats;
        const bool onlyNearest;
    } colorImageFormatsToTestBlit[] = {
        {compatibleFormatsUInts, true},
        {compatibleFormatsSInts, true},
        {compatibleFormatsFloats, false},
        {compatibleFormatsSrgb, false},
    };

    const int numOfColorImageFormatsToTest = DE_LENGTH_OF_ARRAY(colorImageFormatsToTestBlit);

    const int layerCountsToTest[] = {1, 6};

    TestParams params;
    params.src.image.imageType = VK_IMAGE_TYPE_2D;
    params.src.image.extent    = defaultExtent;
    params.src.image.tiling    = VK_IMAGE_TILING_OPTIMAL;
    params.dst.image.imageType = VK_IMAGE_TYPE_2D;
    params.dst.image.extent    = defaultExtent;
    params.dst.image.tiling    = VK_IMAGE_TILING_OPTIMAL;
    params.allocationKind      = allocationKind;
    params.extensionFlags      = extensionFlags;
    params.mipLevels           = deLog2Floor32(deMaxu32(defaultExtent.width, defaultExtent.height)) + 1u;
    params.singleCommand       = false;

    CopyRegion region;
    for (uint32_t mipLevelNdx = 1u; mipLevelNdx < params.mipLevels; mipLevelNdx++)
    {
        VkImageSubresourceLayers srcLayer  = defaultSourceLayer;
        VkImageSubresourceLayers destLayer = defaultSourceLayer;

        srcLayer.mipLevel  = mipLevelNdx - 1u;
        destLayer.mipLevel = mipLevelNdx;

        const VkImageBlit imageBlit = {
            srcLayer, // VkImageSubresourceLayers srcSubresource;
            {{0, 0, 0},
             {defaultSize >> (mipLevelNdx - 1u), defaultSize >> (mipLevelNdx - 1u), 1}}, // VkOffset3D srcOffsets[2];

            destLayer, // VkImageSubresourceLayers dstSubresource;
            {{0, 0, 0}, {defaultSize >> mipLevelNdx, defaultSize >> mipLevelNdx, 1}} // VkOffset3D dstOffset[2];
        };
        region.imageBlit = imageBlit;
        params.regions.push_back(region);
    }

    if (allocationKind == ALLOCATION_KIND_DEDICATED)
    {
        const int numOfColorImageFormatsToTestFilter = DE_LENGTH_OF_ARRAY(dedicatedAllocationBlittingFormatsToTest);
        for (int compatibleFormatsIndex = 0; compatibleFormatsIndex < numOfColorImageFormatsToTestFilter;
             ++compatibleFormatsIndex)
            dedicatedAllocationBlittingFormatsToTestSet.insert(
                dedicatedAllocationBlittingFormatsToTest[compatibleFormatsIndex]);
    }

    for (int layerCountIndex = 0; layerCountIndex < DE_LENGTH_OF_ARRAY(layerCountsToTest); layerCountIndex++)
    {
        const int layerCount             = layerCountsToTest[layerCountIndex];
        const std::string layerGroupName = "layercount_" + de::toString(layerCount);

        de::MovePtr<tcu::TestCaseGroup> layerCountGroup(
            new tcu::TestCaseGroup(group->getTestContext(), layerGroupName.c_str()));

        for (int compatibleFormatsIndex = 0; compatibleFormatsIndex < numOfColorImageFormatsToTest;
             ++compatibleFormatsIndex)
        {
            const VkFormat *compatibleFormats = colorImageFormatsToTestBlit[compatibleFormatsIndex].compatibleFormats;
            const bool onlyNearest            = colorImageFormatsToTestBlit[compatibleFormatsIndex].onlyNearest;

            for (int srcFormatIndex = 0; compatibleFormats[srcFormatIndex] != VK_FORMAT_UNDEFINED; ++srcFormatIndex)
            {
                params.src.image.format = compatibleFormats[srcFormatIndex];
                params.dst.image.format = compatibleFormats[srcFormatIndex];

                if (!isSupportedByFramework(params.src.image.format))
                    continue;

                const bool onlyNearestAndLinear =
                    de::contains(onlyNearestAndLinearFormatsToTest, params.src.image.format);

                BlitColorTestParams testParams;
                testParams.params            = params;
                testParams.compatibleFormats = compatibleFormats;
                testParams.testFilters       = makeFilterMask(onlyNearest, onlyNearestAndLinear);

                testParams.params.src.image.extent.depth = layerCount;
                testParams.params.dst.image.extent.depth = layerCount;

                for (size_t regionNdx = 0; regionNdx < testParams.params.regions.size(); regionNdx++)
                {
                    testParams.params.regions[regionNdx].imageBlit.srcSubresource.layerCount = layerCount;
                    testParams.params.regions[regionNdx].imageBlit.dstSubresource.layerCount = layerCount;
                }

                addTestGroup(layerCountGroup.get(), getFormatCaseName(params.src.image.format),
                             addBlittingImageAllFormatsMipmapFormatTests, testParams);
            }
        }
        group->addChild(layerCountGroup.release());
    }

    for (int multiLayer = 0; multiLayer < 2; multiLayer++)
    {
        const int layerCount = multiLayer ? 6 : 1;

        for (int barrierCount = 1; barrierCount < 4; barrierCount++)
        {
            if (layerCount != 1 || barrierCount != 1)
            {
                const std::string barrierGroupName =
                    (multiLayer ? "layerbarriercount_" : "mipbarriercount_") + de::toString(barrierCount);

                de::MovePtr<tcu::TestCaseGroup> barrierCountGroup(
                    new tcu::TestCaseGroup(group->getTestContext(), barrierGroupName.c_str()));

                params.barrierCount = barrierCount;

                // Only go through a few common formats
                for (int srcFormatIndex = 2; srcFormatIndex < 6; ++srcFormatIndex)
                {
                    params.src.image.format = compatibleFormatsUInts[srcFormatIndex];
                    params.dst.image.format = compatibleFormatsUInts[srcFormatIndex];

                    if (!isSupportedByFramework(params.src.image.format))
                        continue;

                    BlitColorTestParams testParams;
                    testParams.params            = params;
                    testParams.compatibleFormats = compatibleFormatsUInts;
                    testParams.testFilters       = FILTER_MASK_NEAREST;

                    testParams.params.src.image.extent.depth = layerCount;
                    testParams.params.dst.image.extent.depth = layerCount;

                    for (size_t regionNdx = 0; regionNdx < testParams.params.regions.size(); regionNdx++)
                    {
                        testParams.params.regions[regionNdx].imageBlit.srcSubresource.layerCount = layerCount;
                        testParams.params.regions[regionNdx].imageBlit.dstSubresource.layerCount = layerCount;
                    }

                    addTestGroup(barrierCountGroup.get(), getFormatCaseName(params.src.image.format),
                                 addBlittingImageAllFormatsMipmapFormatTests, testParams);
                }
                group->addChild(barrierCountGroup.release());
            }
        }
    }
}

void addBlittingImageAllFormatsMipmapTests(tcu::TestCaseGroup *group, AllocationKind allocationKind,
                                           uint32_t extensionFlags)
{
    addTestGroup(group, "from_base_level", addBlittingImageAllFormatsBaseLevelMipmapTests, allocationKind,
                 extensionFlags);
    addTestGroup(group, "from_previous_level", addBlittingImageAllFormatsPreviousLevelMipmapTests, allocationKind,
                 extensionFlags);
}

void addBlittingImageAllFormatsTests(tcu::TestCaseGroup *group, AllocationKind allocationKind, uint32_t extensionFlags)
{
    addTestGroup(group, "color", addBlittingImageAllFormatsColorTests, allocationKind, extensionFlags);
    addTestGroup(group, "depth_stencil", addBlittingImageAllFormatsDepthStencilTests, allocationKind, extensionFlags);
    addTestGroup(group, "generate_mipmaps", addBlittingImageAllFormatsMipmapTests, allocationKind, extensionFlags);
}

void addBlittingImageTests(tcu::TestCaseGroup *group, AllocationKind allocationKind, uint32_t extensionFlags)
{
    addTestGroup(group, "simple_tests", addBlittingImageSimpleTests, allocationKind, extensionFlags);
    addTestGroup(group, "all_formats", addBlittingImageAllFormatsTests, allocationKind, extensionFlags);
}

const VkSampleCountFlagBits samples[] = {VK_SAMPLE_COUNT_2_BIT,  VK_SAMPLE_COUNT_4_BIT,  VK_SAMPLE_COUNT_8_BIT,
                                         VK_SAMPLE_COUNT_16_BIT, VK_SAMPLE_COUNT_32_BIT, VK_SAMPLE_COUNT_64_BIT};
const VkExtent3D resolveExtent        = {256u, 256u, 1};

void addResolveImageWholeTests(tcu::TestCaseGroup *group, AllocationKind allocationKind, uint32_t extensionFlags)
{
    TestParams params;
    params.src.image.imageType       = VK_IMAGE_TYPE_2D;
    params.src.image.format          = VK_FORMAT_R8G8B8A8_UNORM;
    params.src.image.extent          = resolveExtent;
    params.src.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
    params.src.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    params.dst.image.imageType       = VK_IMAGE_TYPE_2D;
    params.dst.image.format          = VK_FORMAT_R8G8B8A8_UNORM;
    params.dst.image.extent          = resolveExtent;
    params.dst.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
    params.dst.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    params.allocationKind            = allocationKind;
    params.extensionFlags            = extensionFlags;

    {
        const VkImageSubresourceLayers sourceLayer = {
            VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
            0u,                        // uint32_t mipLevel;
            0u,                        // uint32_t baseArrayLayer;
            1u                         // uint32_t layerCount;
        };
        const VkImageResolve testResolve = {
            sourceLayer,   // VkImageSubresourceLayers srcSubresource;
            {0, 0, 0},     // VkOffset3D srcOffset;
            sourceLayer,   // VkImageSubresourceLayers dstSubresource;
            {0, 0, 0},     // VkOffset3D dstOffset;
            resolveExtent, // VkExtent3D extent;
        };

        CopyRegion imageResolve;
        imageResolve.imageResolve = testResolve;
        params.regions.push_back(imageResolve);
    }

    for (int samplesIndex = 0; samplesIndex < DE_LENGTH_OF_ARRAY(samples); ++samplesIndex)
    {
        params.imageOffset = false;
        params.samples     = samples[samplesIndex];
        group->addChild(new ResolveImageToImageTestCase(group->getTestContext(),
                                                        getSampleCountCaseName(samples[samplesIndex]), params));
        params.imageOffset = true;
        if (allocationKind != ALLOCATION_KIND_DEDICATED)
        {
            group->addChild(new ResolveImageToImageTestCase(
                group->getTestContext(), getSampleCountCaseName(samples[samplesIndex]) + "_bind_offset", params));
        }
    }
}

void addResolveImagePartialTests(tcu::TestCaseGroup *group, AllocationKind allocationKind, uint32_t extensionFlags)
{
    TestParams params;
    params.src.image.imageType       = VK_IMAGE_TYPE_2D;
    params.src.image.format          = VK_FORMAT_R8G8B8A8_UNORM;
    params.src.image.extent          = resolveExtent;
    params.src.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
    params.src.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    params.dst.image.imageType       = VK_IMAGE_TYPE_2D;
    params.dst.image.format          = VK_FORMAT_R8G8B8A8_UNORM;
    params.dst.image.extent          = resolveExtent;
    params.dst.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
    params.dst.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    params.allocationKind            = allocationKind;
    params.extensionFlags            = extensionFlags;

    {
        const VkImageSubresourceLayers sourceLayer = {
            VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
            0u,                        // uint32_t mipLevel;
            0u,                        // uint32_t baseArrayLayer;
            1u                         // uint32_t layerCount;
        };
        const VkImageResolve testResolve = {
            sourceLayer,      // VkImageSubresourceLayers srcSubresource;
            {0, 0, 0},        // VkOffset3D srcOffset;
            sourceLayer,      // VkImageSubresourceLayers dstSubresource;
            {64u, 64u, 0},    // VkOffset3D dstOffset;
            {128u, 128u, 1u}, // VkExtent3D extent;
        };

        CopyRegion imageResolve;
        imageResolve.imageResolve = testResolve;
        params.regions.push_back(imageResolve);
    }

    for (int samplesIndex = 0; samplesIndex < DE_LENGTH_OF_ARRAY(samples); ++samplesIndex)
    {
        params.samples     = samples[samplesIndex];
        params.imageOffset = false;
        group->addChild(new ResolveImageToImageTestCase(group->getTestContext(),
                                                        getSampleCountCaseName(samples[samplesIndex]), params));
        params.imageOffset = true;
        if (allocationKind != ALLOCATION_KIND_DEDICATED)
        {
            group->addChild(new ResolveImageToImageTestCase(
                group->getTestContext(), getSampleCountCaseName(samples[samplesIndex]) + "_bind_offset", params));
        }
    }
}

void addResolveImageWithRegionsTests(tcu::TestCaseGroup *group, AllocationKind allocationKind, uint32_t extensionFlags)
{
    TestParams params;
    params.src.image.imageType       = VK_IMAGE_TYPE_2D;
    params.src.image.format          = VK_FORMAT_R8G8B8A8_UNORM;
    params.src.image.extent          = resolveExtent;
    params.src.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
    params.src.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    params.dst.image.imageType       = VK_IMAGE_TYPE_2D;
    params.dst.image.format          = VK_FORMAT_R8G8B8A8_UNORM;
    params.dst.image.extent          = resolveExtent;
    params.dst.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
    params.dst.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    params.allocationKind            = allocationKind;
    params.extensionFlags            = extensionFlags;
    params.imageOffset               = allocationKind != ALLOCATION_KIND_DEDICATED;

    {
        const VkImageSubresourceLayers sourceLayer = {
            VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
            0u,                        // uint32_t mipLevel;
            0u,                        // uint32_t baseArrayLayer;
            1u                         // uint32_t layerCount;
        };

        for (int i = 0; i < 256; i += 64)
        {
            const VkImageResolve testResolve = {
                sourceLayer,    // VkImageSubresourceLayers srcSubresource;
                {i, i, 0},      // VkOffset3D srcOffset;
                sourceLayer,    // VkImageSubresourceLayers dstSubresource;
                {i, 0, 0},      // VkOffset3D dstOffset;
                {64u, 64u, 1u}, // VkExtent3D extent;
            };

            CopyRegion imageResolve;
            imageResolve.imageResolve = testResolve;
            params.regions.push_back(imageResolve);
        }
    }

    for (int samplesIndex = 0; samplesIndex < DE_LENGTH_OF_ARRAY(samples); ++samplesIndex)
    {
        params.samples = samples[samplesIndex];
        group->addChild(new ResolveImageToImageTestCase(group->getTestContext(),
                                                        getSampleCountCaseName(samples[samplesIndex]), params));
    }
}

void addResolveImageWholeCopyBeforeResolvingTests(tcu::TestCaseGroup *group, AllocationKind allocationKind,
                                                  uint32_t extensionFlags)
{
    TestParams params;
    params.src.image.imageType       = VK_IMAGE_TYPE_2D;
    params.src.image.format          = VK_FORMAT_R8G8B8A8_UNORM;
    params.src.image.extent          = defaultExtent;
    params.src.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
    params.src.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    params.dst.image.imageType       = VK_IMAGE_TYPE_2D;
    params.dst.image.format          = VK_FORMAT_R8G8B8A8_UNORM;
    params.dst.image.extent          = defaultExtent;
    params.dst.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
    params.dst.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    params.allocationKind            = allocationKind;
    params.extensionFlags            = extensionFlags;

    {
        const VkImageSubresourceLayers sourceLayer = {
            VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
            0u,                        // uint32_t mipLevel;
            0u,                        // uint32_t baseArrayLayer;
            1u                         // uint32_t layerCount;
        };

        const VkImageResolve testResolve = {
            sourceLayer,   // VkImageSubresourceLayers srcSubresource;
            {0, 0, 0},     // VkOffset3D srcOffset;
            sourceLayer,   // VkImageSubresourceLayers dstSubresource;
            {0, 0, 0},     // VkOffset3D dstOffset;
            defaultExtent, // VkExtent3D extent;
        };

        CopyRegion imageResolve;
        imageResolve.imageResolve = testResolve;
        params.regions.push_back(imageResolve);
    }

    for (int samplesIndex = 0; samplesIndex < DE_LENGTH_OF_ARRAY(samples); ++samplesIndex)
    {
        params.samples     = samples[samplesIndex];
        params.imageOffset = false;
        group->addChild(new ResolveImageToImageTestCase(
            group->getTestContext(), getSampleCountCaseName(samples[samplesIndex]), params, COPY_MS_IMAGE_TO_MS_IMAGE));
        params.imageOffset = true;
        if (allocationKind != ALLOCATION_KIND_DEDICATED)
        {
            group->addChild(new ResolveImageToImageTestCase(
                group->getTestContext(), getSampleCountCaseName(samples[samplesIndex]) + "_bind_offset", params,
                COPY_MS_IMAGE_TO_MS_IMAGE));
        }
    }
}

void addComputeAndTransferQueueTests(tcu::TestCaseGroup *group, AllocationKind allocationKind, uint32_t extensionFlags)
{
    de::MovePtr<tcu::TestCaseGroup> computeGroup(
        new tcu::TestCaseGroup(group->getTestContext(), "whole_copy_before_resolving_compute"));
    de::MovePtr<tcu::TestCaseGroup> transferGroup(
        new tcu::TestCaseGroup(group->getTestContext(), "whole_copy_before_resolving_transfer"));

    TestParams params;
    params.src.image.imageType       = VK_IMAGE_TYPE_2D;
    params.src.image.format          = VK_FORMAT_R8G8B8A8_UNORM;
    params.src.image.extent          = defaultExtent;
    params.src.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
    params.src.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    params.dst.image.imageType       = VK_IMAGE_TYPE_2D;
    params.dst.image.format          = VK_FORMAT_R8G8B8A8_UNORM;
    params.dst.image.extent          = defaultExtent;
    params.dst.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
    params.dst.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    params.allocationKind            = allocationKind;
    params.extensionFlags            = extensionFlags;

    {
        const VkImageSubresourceLayers sourceLayer = {
            VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
            0u,                        // uint32_t mipLevel;
            0u,                        // uint32_t baseArrayLayer;
            1u                         // uint32_t layerCount;
        };

        const VkImageResolve testResolve = {
            sourceLayer,   // VkImageSubresourceLayers srcSubresource;
            {0, 0, 0},     // VkOffset3D srcOffset;
            sourceLayer,   // VkImageSubresourceLayers dstSubresource;
            {0, 0, 0},     // VkOffset3D dstOffset;
            defaultExtent, // VkExtent3D extent;
        };

        CopyRegion imageResolve;
        imageResolve.imageResolve = testResolve;
        params.regions.push_back(imageResolve);
    }

    for (const auto &sample : samples)
    {
        params.samples = sample;

        params.queueSelection = QueueSelectionOptions::ComputeOnly;
        computeGroup->addChild(new ResolveImageToImageTestCase(group->getTestContext(), getSampleCountCaseName(sample),
                                                               params, COPY_MS_IMAGE_TO_MS_IMAGE_COMPUTE));

        params.queueSelection = QueueSelectionOptions::TransferOnly;
        transferGroup->addChild(new ResolveImageToImageTestCase(group->getTestContext(), getSampleCountCaseName(sample),
                                                                params, COPY_MS_IMAGE_TO_MS_IMAGE_TRANSFER));
    }

    group->addChild(computeGroup.release());
    group->addChild(transferGroup.release());
}

void addResolveImageWholeCopyWithoutCabBeforeResolvingTests(tcu::TestCaseGroup *group, AllocationKind allocationKind,
                                                            uint32_t extensionFlags)
{
    TestParams params;
    params.src.image.imageType       = VK_IMAGE_TYPE_2D;
    params.src.image.format          = VK_FORMAT_R8G8B8A8_UNORM;
    params.src.image.extent          = defaultExtent;
    params.src.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
    params.src.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    params.dst.image.imageType       = VK_IMAGE_TYPE_2D;
    params.dst.image.format          = VK_FORMAT_R8G8B8A8_UNORM;
    params.dst.image.extent          = defaultExtent;
    params.dst.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
    params.dst.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    params.allocationKind            = allocationKind;
    params.extensionFlags            = extensionFlags;

    {
        const VkImageSubresourceLayers sourceLayer = {
            VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
            0u,                        // uint32_t mipLevel;
            0u,                        // uint32_t baseArrayLayer;
            1u                         // uint32_t layerCount;
        };

        const VkImageResolve testResolve = {
            sourceLayer,   // VkImageSubresourceLayers srcSubresource;
            {0, 0, 0},     // VkOffset3D srcOffset;
            sourceLayer,   // VkImageSubresourceLayers dstSubresource;
            {0, 0, 0},     // VkOffset3D dstOffset;
            defaultExtent, // VkExtent3D extent;
        };

        CopyRegion imageResolve;
        imageResolve.imageResolve = testResolve;
        params.regions.push_back(imageResolve);
    }

    for (int samplesIndex = 0; samplesIndex < DE_LENGTH_OF_ARRAY(samples); ++samplesIndex)
    {
        params.samples     = samples[samplesIndex];
        params.imageOffset = false;
        group->addChild(new ResolveImageToImageTestCase(group->getTestContext(),
                                                        getSampleCountCaseName(samples[samplesIndex]), params,
                                                        COPY_MS_IMAGE_TO_MS_IMAGE_NO_CAB));
        params.imageOffset = true;
        if (allocationKind != ALLOCATION_KIND_DEDICATED)
        {
            group->addChild(new ResolveImageToImageTestCase(
                group->getTestContext(), getSampleCountCaseName(samples[samplesIndex]) + "_bind_offset", params,
                COPY_MS_IMAGE_TO_MS_IMAGE_NO_CAB));
        }
    }
}

void addResolveImageWholeCopyDiffLayoutsBeforeResolvingTests(tcu::TestCaseGroup *group, AllocationKind allocationKind,
                                                             uint32_t extensionFlags)
{
    TestParams params;
    params.src.image.imageType       = VK_IMAGE_TYPE_2D;
    params.src.image.format          = VK_FORMAT_R8G8B8A8_UNORM;
    params.src.image.extent          = defaultExtent;
    params.src.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
    params.src.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    params.dst.image.imageType       = VK_IMAGE_TYPE_2D;
    params.dst.image.format          = VK_FORMAT_R8G8B8A8_UNORM;
    params.dst.image.extent          = defaultExtent;
    params.dst.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
    params.dst.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    params.allocationKind            = allocationKind;
    params.extensionFlags            = extensionFlags;

    {
        const VkImageSubresourceLayers sourceLayer = {
            VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
            0u,                        // uint32_t mipLevel;
            0u,                        // uint32_t baseArrayLayer;
            1u                         // uint32_t layerCount;
        };

        const VkImageResolve testResolve = {
            sourceLayer,   // VkImageSubresourceLayers srcSubresource;
            {0, 0, 0},     // VkOffset3D srcOffset;
            sourceLayer,   // VkImageSubresourceLayers dstSubresource;
            {0, 0, 0},     // VkOffset3D dstOffset;
            defaultExtent, // VkExtent3D extent;
        };

        CopyRegion imageResolve;
        imageResolve.imageResolve = testResolve;
        params.regions.push_back(imageResolve);
    }

    const struct
    {
        VkImageLayout layout;
        std::string name;
    } imageLayouts[] = {{VK_IMAGE_LAYOUT_GENERAL, "general"},
                        {VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, "transfer_src_optimal"},
                        {VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, "transfer_dst_optimal"}};

    for (int samplesIndex = 0; samplesIndex < DE_LENGTH_OF_ARRAY(samples); ++samplesIndex)
        for (int srcLayoutIndex = 0; srcLayoutIndex < DE_LENGTH_OF_ARRAY(imageLayouts); ++srcLayoutIndex)
            for (int dstLayoutIndex = 0; dstLayoutIndex < DE_LENGTH_OF_ARRAY(imageLayouts); ++dstLayoutIndex)
            {
                params.src.image.operationLayout = imageLayouts[srcLayoutIndex].layout;
                params.dst.image.operationLayout = imageLayouts[dstLayoutIndex].layout;
                if (params.src.image.operationLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL ||
                    params.dst.image.operationLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
                    continue;
                params.samples       = samples[samplesIndex];
                std::string testName = getSampleCountCaseName(samples[samplesIndex]) + "_" +
                                       imageLayouts[srcLayoutIndex].name + "_" + imageLayouts[dstLayoutIndex].name;
                params.imageOffset = false;
                group->addChild(new ResolveImageToImageTestCase(group->getTestContext(), testName, params,
                                                                COPY_MS_IMAGE_TO_MS_IMAGE));
                params.imageOffset = true;
                if (allocationKind != ALLOCATION_KIND_DEDICATED)
                {
                    group->addChild(new ResolveImageToImageTestCase(group->getTestContext(), testName + "_bind_offset",
                                                                    params, COPY_MS_IMAGE_TO_MS_IMAGE));
                }
            }
}

void addResolveImageLayerCopyBeforeResolvingTests(tcu::TestCaseGroup *group, AllocationKind allocationKind,
                                                  uint32_t extensionFlags)
{
    TestParams params;
    params.src.image.imageType       = VK_IMAGE_TYPE_2D;
    params.src.image.format          = VK_FORMAT_R8G8B8A8_UNORM;
    params.src.image.extent          = defaultExtent;
    params.src.image.extent.depth    = 5u;
    params.src.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
    params.src.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    params.dst.image.imageType       = VK_IMAGE_TYPE_2D;
    params.dst.image.format          = VK_FORMAT_R8G8B8A8_UNORM;
    params.dst.image.extent          = defaultExtent;
    params.dst.image.extent.depth    = 5u;
    params.dst.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
    params.dst.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    params.allocationKind            = allocationKind;
    params.extensionFlags            = extensionFlags;

    for (uint32_t layerNdx = 0; layerNdx < params.src.image.extent.depth; ++layerNdx)
    {
        const VkImageSubresourceLayers sourceLayer = {
            VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
            0u,                        // uint32_t mipLevel;
            layerNdx,                  // uint32_t baseArrayLayer;
            1u                         // uint32_t layerCount;
        };

        const VkImageResolve testResolve = {
            sourceLayer,   // VkImageSubresourceLayers srcSubresource;
            {0, 0, 0},     // VkOffset3D srcOffset;
            sourceLayer,   // VkImageSubresourceLayers dstSubresource;
            {0, 0, 0},     // VkOffset3D dstOffset;
            defaultExtent, // VkExtent3D extent;
        };

        CopyRegion imageResolve;
        imageResolve.imageResolve = testResolve;
        params.regions.push_back(imageResolve);
    }

    for (int samplesIndex = 0; samplesIndex < DE_LENGTH_OF_ARRAY(samples); ++samplesIndex)
    {
        params.samples     = samples[samplesIndex];
        params.imageOffset = false;
        group->addChild(new ResolveImageToImageTestCase(group->getTestContext(),
                                                        getSampleCountCaseName(samples[samplesIndex]), params,
                                                        COPY_MS_IMAGE_LAYER_TO_MS_IMAGE));
        params.imageOffset = true;
        if (allocationKind != ALLOCATION_KIND_DEDICATED)
        {
            group->addChild(new ResolveImageToImageTestCase(
                group->getTestContext(), getSampleCountCaseName(samples[samplesIndex]) + "_bind_offset", params,
                COPY_MS_IMAGE_LAYER_TO_MS_IMAGE));
        }
    }
}

void addResolveCopyImageWithRegionsTests(tcu::TestCaseGroup *group, AllocationKind allocationKind,
                                         uint32_t extensionFlags)
{
    TestParams params;
    params.src.image.imageType       = VK_IMAGE_TYPE_2D;
    params.src.image.format          = VK_FORMAT_R8G8B8A8_UNORM;
    params.src.image.extent          = resolveExtent;
    params.src.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
    params.src.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    params.dst.image.imageType       = VK_IMAGE_TYPE_2D;
    params.dst.image.format          = VK_FORMAT_R8G8B8A8_UNORM;
    params.dst.image.extent          = resolveExtent;
    params.dst.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
    params.dst.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    params.allocationKind            = allocationKind;
    params.extensionFlags            = extensionFlags;

    int32_t imageHalfWidth     = getExtent3D(params.src.image).width / 2;
    int32_t imageHalfHeight    = getExtent3D(params.src.image).height / 2;
    VkExtent3D halfImageExtent = {resolveExtent.width / 2, resolveExtent.height / 2, 1u};

    // Lower right corner to lower left corner.
    {
        const VkImageSubresourceLayers sourceLayer = {
            VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
            0u,                        // uint32_t mipLevel;
            0u,                        // uint32_t baseArrayLayer;
            1u                         // uint32_t layerCount;
        };

        const VkImageResolve testResolve = {
            sourceLayer,                          // VkImageSubresourceLayers srcSubresource;
            {imageHalfWidth, imageHalfHeight, 0}, // VkOffset3D srcOffset;
            sourceLayer,                          // VkImageSubresourceLayers dstSubresource;
            {0, imageHalfHeight, 0},              // VkOffset3D dstOffset;
            halfImageExtent,                      // VkExtent3D extent;
        };

        CopyRegion imageResolve;
        imageResolve.imageResolve = testResolve;
        params.regions.push_back(imageResolve);
    }

    // Upper right corner to lower right corner.
    {
        const VkImageSubresourceLayers sourceLayer = {
            VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
            0u,                        // uint32_t mipLevel;
            0u,                        // uint32_t baseArrayLayer;
            1u                         // uint32_t layerCount;
        };

        const VkImageResolve testResolve = {
            sourceLayer,                          // VkImageSubresourceLayers srcSubresource;
            {imageHalfWidth, 0, 0},               // VkOffset3D srcOffset;
            sourceLayer,                          // VkImageSubresourceLayers dstSubresource;
            {imageHalfWidth, imageHalfHeight, 0}, // VkOffset3D dstOffset;
            halfImageExtent,                      // VkExtent3D extent;
        };

        CopyRegion imageResolve;
        imageResolve.imageResolve = testResolve;
        params.regions.push_back(imageResolve);
    }

    for (int samplesIndex = 0; samplesIndex < DE_LENGTH_OF_ARRAY(samples); ++samplesIndex)
    {
        params.samples     = samples[samplesIndex];
        params.imageOffset = false;
        group->addChild(new ResolveImageToImageTestCase(group->getTestContext(),
                                                        getSampleCountCaseName(samples[samplesIndex]), params,
                                                        COPY_MS_IMAGE_TO_MS_IMAGE_MULTIREGION));
        params.imageOffset = true;
        if (allocationKind != ALLOCATION_KIND_DEDICATED)
        {
            group->addChild(new ResolveImageToImageTestCase(
                group->getTestContext(), getSampleCountCaseName(samples[samplesIndex]) + "_bind_offset", params,
                COPY_MS_IMAGE_TO_MS_IMAGE_MULTIREGION));
        }
    }
}

void addResolveImageWholeArrayImageTests(tcu::TestCaseGroup *group, AllocationKind allocationKind,
                                         uint32_t extensionFlags)
{
    TestParams params;
    params.src.image.imageType       = VK_IMAGE_TYPE_2D;
    params.src.image.format          = VK_FORMAT_R8G8B8A8_UNORM;
    params.src.image.extent          = defaultExtent;
    params.src.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
    params.src.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    params.dst.image.imageType       = VK_IMAGE_TYPE_2D;
    params.dst.image.format          = VK_FORMAT_R8G8B8A8_UNORM;
    params.dst.image.extent          = defaultExtent;
    params.dst.image.extent.depth    = 5u;
    params.dst.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
    params.dst.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    params.allocationKind            = allocationKind;
    params.extensionFlags            = extensionFlags;

    for (uint32_t layerNdx = 0; layerNdx < params.dst.image.extent.depth; ++layerNdx)
    {
        const VkImageSubresourceLayers sourceLayer = {
            VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
            0u,                        // uint32_t mipLevel;
            layerNdx,                  // uint32_t baseArrayLayer;
            1u                         // uint32_t layerCount;
        };

        const VkImageResolve testResolve = {
            sourceLayer,   // VkImageSubresourceLayers srcSubresource;
            {0, 0, 0},     // VkOffset3D srcOffset;
            sourceLayer,   // VkImageSubresourceLayers dstSubresource;
            {0, 0, 0},     // VkOffset3D dstOffset;
            defaultExtent, // VkExtent3D extent;
        };

        CopyRegion imageResolve;
        imageResolve.imageResolve = testResolve;
        params.regions.push_back(imageResolve);
    }

    for (int samplesIndex = 0; samplesIndex < DE_LENGTH_OF_ARRAY(samples); ++samplesIndex)
    {
        params.samples     = samples[samplesIndex];
        params.imageOffset = false;
        group->addChild(new ResolveImageToImageTestCase(group->getTestContext(),
                                                        getSampleCountCaseName(samples[samplesIndex]), params,
                                                        COPY_MS_IMAGE_TO_ARRAY_MS_IMAGE));
        params.imageOffset = true;
        if (allocationKind != ALLOCATION_KIND_DEDICATED)
        {
            group->addChild(new ResolveImageToImageTestCase(
                group->getTestContext(), getSampleCountCaseName(samples[samplesIndex]) + "_bind_offset", params,
                COPY_MS_IMAGE_TO_ARRAY_MS_IMAGE));
        }
    }
}

void addResolveImageWholeArrayImageSingleRegionTests(tcu::TestCaseGroup *group, AllocationKind allocationKind,
                                                     uint32_t extensionFlags)
{
    {
        TestParams params;
        const uint32_t layerCount        = 5u;
        params.src.image.imageType       = VK_IMAGE_TYPE_2D;
        params.src.image.format          = VK_FORMAT_R8G8B8A8_UNORM;
        params.src.image.extent          = defaultExtent;
        params.src.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        params.dst.image.imageType       = VK_IMAGE_TYPE_2D;
        params.dst.image.format          = VK_FORMAT_R8G8B8A8_UNORM;
        params.dst.image.extent          = defaultExtent;
        params.dst.image.extent.depth    = layerCount;
        params.dst.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        params.allocationKind            = allocationKind;
        params.extensionFlags            = extensionFlags;

        const VkImageSubresourceLayers sourceLayer = {
            VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
            0u,                        // uint32_t mipLevel;
            0,                         // uint32_t baseArrayLayer;
            layerCount                 // uint32_t layerCount;
        };

        const VkImageResolve testResolve = {
            sourceLayer,   // VkImageSubresourceLayers srcSubresource;
            {0, 0, 0},     // VkOffset3D srcOffset;
            sourceLayer,   // VkImageSubresourceLayers dstSubresource;
            {0, 0, 0},     // VkOffset3D dstOffset;
            defaultExtent, // VkExtent3D extent;
        };

        CopyRegion imageResolve;
        imageResolve.imageResolve = testResolve;
        params.regions.push_back(imageResolve);

        for (int samplesIndex = 0; samplesIndex < DE_LENGTH_OF_ARRAY(samples); ++samplesIndex)
        {
            params.samples     = samples[samplesIndex];
            params.imageOffset = false;
            group->addChild(new ResolveImageToImageTestCase(group->getTestContext(),
                                                            getSampleCountCaseName(samples[samplesIndex]), params,
                                                            COPY_MS_IMAGE_TO_ARRAY_MS_IMAGE));
            params.imageOffset = true;
            if (allocationKind != ALLOCATION_KIND_DEDICATED)
            {
                group->addChild(new ResolveImageToImageTestCase(
                    group->getTestContext(), getSampleCountCaseName(samples[samplesIndex]) + "_bind_offset", params,
                    COPY_MS_IMAGE_TO_ARRAY_MS_IMAGE));
            }
        }
    }

    {
        TestParams params;
        const uint32_t baseLayer         = 0u;
        const uint32_t layerCount        = 5u;
        params.src.image.imageType       = VK_IMAGE_TYPE_2D;
        params.src.image.format          = VK_FORMAT_R8G8B8A8_UNORM;
        params.src.image.extent          = defaultExtent;
        params.src.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        params.dst.image.imageType       = VK_IMAGE_TYPE_2D;
        params.dst.image.format          = VK_FORMAT_R8G8B8A8_UNORM;
        params.dst.image.extent          = defaultExtent;
        params.dst.image.extent.depth    = layerCount;
        params.dst.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        params.allocationKind            = allocationKind;
        params.extensionFlags            = extensionFlags;
        params.extensionFlags |= MAINTENANCE_5;

        const VkImageSubresourceLayers sourceLayer = {
            VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
            0u,                        // uint32_t mipLevel;
            baseLayer,                 // uint32_t baseArrayLayer;
            VK_REMAINING_ARRAY_LAYERS  // uint32_t layerCount;
        };

        const VkImageResolve testResolve = {
            sourceLayer,   // VkImageSubresourceLayers srcSubresource;
            {0, 0, 0},     // VkOffset3D srcOffset;
            sourceLayer,   // VkImageSubresourceLayers dstSubresource;
            {0, 0, 0},     // VkOffset3D dstOffset;
            defaultExtent, // VkExtent3D extent;
        };

        CopyRegion imageResolve;
        imageResolve.imageResolve = testResolve;
        params.regions.push_back(imageResolve);

        for (int samplesIndex = 0; samplesIndex < DE_LENGTH_OF_ARRAY(samples); ++samplesIndex)
        {
            params.samples     = samples[samplesIndex];
            params.imageOffset = false;
            group->addChild(new ResolveImageToImageTestCase(
                group->getTestContext(), getSampleCountCaseName(samples[samplesIndex]) + "_all_remaining_layers",
                params, COPY_MS_IMAGE_TO_ARRAY_MS_IMAGE));
            params.imageOffset = true;
            if (allocationKind != ALLOCATION_KIND_DEDICATED)
            {
                group->addChild(new ResolveImageToImageTestCase(group->getTestContext(),
                                                                getSampleCountCaseName(samples[samplesIndex]) +
                                                                    "_all_remaining_layers_bind_offset",
                                                                params, COPY_MS_IMAGE_TO_ARRAY_MS_IMAGE));
            }
        }
    }

    {
        TestParams params;
        const uint32_t baseLayer         = 2u;
        const uint32_t layerCount        = 5u;
        params.src.image.imageType       = VK_IMAGE_TYPE_2D;
        params.src.image.format          = VK_FORMAT_R8G8B8A8_UNORM;
        params.src.image.extent          = defaultExtent;
        params.src.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        params.dst.image.imageType       = VK_IMAGE_TYPE_2D;
        params.dst.image.format          = VK_FORMAT_R8G8B8A8_UNORM;
        params.dst.image.extent          = defaultExtent;
        params.dst.image.extent.depth    = layerCount;
        params.dst.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        params.allocationKind            = allocationKind;
        params.extensionFlags            = extensionFlags;
        params.extensionFlags |= MAINTENANCE_5;

        const VkImageSubresourceLayers sourceLayer = {
            VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
            0u,                        // uint32_t mipLevel;
            baseLayer,                 // uint32_t baseArrayLayer;
            VK_REMAINING_ARRAY_LAYERS  // uint32_t layerCount;
        };

        const VkImageResolve testResolve = {
            sourceLayer,   // VkImageSubresourceLayers srcSubresource;
            {0, 0, 0},     // VkOffset3D srcOffset;
            sourceLayer,   // VkImageSubresourceLayers dstSubresource;
            {0, 0, 0},     // VkOffset3D dstOffset;
            defaultExtent, // VkExtent3D extent;
        };

        CopyRegion imageResolve;
        imageResolve.imageResolve = testResolve;
        params.regions.push_back(imageResolve);

        for (int samplesIndex = 0; samplesIndex < DE_LENGTH_OF_ARRAY(samples); ++samplesIndex)
        {
            params.samples     = samples[samplesIndex];
            params.imageOffset = false;
            group->addChild(new ResolveImageToImageTestCase(
                group->getTestContext(), getSampleCountCaseName(samples[samplesIndex]) + "_not_all_remaining_layers",
                params, COPY_MS_IMAGE_TO_ARRAY_MS_IMAGE));
            params.imageOffset = true;
            if (allocationKind != ALLOCATION_KIND_DEDICATED)
            {
                group->addChild(new ResolveImageToImageTestCase(group->getTestContext(),
                                                                getSampleCountCaseName(samples[samplesIndex]) +
                                                                    "_not_all_remaining_layers_bind_offset",
                                                                params, COPY_MS_IMAGE_TO_ARRAY_MS_IMAGE));
            }
        }
    }
}

void addResolveImageDiffImageSizeTests(tcu::TestCaseGroup *group, AllocationKind allocationKind,
                                       uint32_t extensionFlags)
{
    tcu::TestContext &testCtx = group->getTestContext();
    TestParams params;
    params.src.image.imageType       = VK_IMAGE_TYPE_2D;
    params.src.image.format          = VK_FORMAT_R8G8B8A8_UNORM;
    params.src.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
    params.src.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    params.dst.image.imageType       = VK_IMAGE_TYPE_2D;
    params.dst.image.format          = VK_FORMAT_R8G8B8A8_UNORM;
    params.dst.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
    params.dst.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    params.allocationKind            = allocationKind;
    params.extensionFlags            = extensionFlags;

    {
        const VkImageSubresourceLayers sourceLayer = {
            VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
            0u,                        // uint32_t mipLevel;
            0u,                        // uint32_t baseArrayLayer;
            1u                         // uint32_t layerCount;
        };
        const VkImageResolve testResolve = {
            sourceLayer,   // VkImageSubresourceLayers srcSubresource;
            {0, 0, 0},     // VkOffset3D srcOffset;
            sourceLayer,   // VkImageSubresourceLayers dstSubresource;
            {0, 0, 0},     // VkOffset3D dstOffset;
            resolveExtent, // VkExtent3D extent;
        };
        CopyRegion imageResolve;
        imageResolve.imageResolve = testResolve;
        params.regions.push_back(imageResolve);
    }

    const VkExtent3D imageExtents[] = {{resolveExtent.width + 10, resolveExtent.height, resolveExtent.depth},
                                       {resolveExtent.width, resolveExtent.height * 2, resolveExtent.depth},
                                       {resolveExtent.width, resolveExtent.height, resolveExtent.depth + 10}};

    for (int srcImageExtentIndex = 0; srcImageExtentIndex < DE_LENGTH_OF_ARRAY(imageExtents); ++srcImageExtentIndex)
    {
        const VkExtent3D &srcImageSize = imageExtents[srcImageExtentIndex];
        params.src.image.extent        = srcImageSize;
        params.dst.image.extent        = resolveExtent;
        for (int samplesIndex = 0; samplesIndex < DE_LENGTH_OF_ARRAY(samples); ++samplesIndex)
        {
            params.samples = samples[samplesIndex];
            std::ostringstream testName;
            testName << "src_" << srcImageSize.width << "_" << srcImageSize.height << "_" << srcImageSize.depth << "_"
                     << getSampleCountCaseName(samples[samplesIndex]);
            std::ostringstream description;
            group->addChild(new ResolveImageToImageTestCase(testCtx, testName.str(), params));
        }
    }
    for (int dstImageExtentIndex = 0; dstImageExtentIndex < DE_LENGTH_OF_ARRAY(imageExtents); ++dstImageExtentIndex)
    {
        const VkExtent3D &dstImageSize = imageExtents[dstImageExtentIndex];
        params.src.image.extent        = resolveExtent;
        params.dst.image.extent        = dstImageSize;
        for (int samplesIndex = 0; samplesIndex < DE_LENGTH_OF_ARRAY(samples); ++samplesIndex)
        {
            params.samples = samples[samplesIndex];
            std::ostringstream testName;
            testName << "dst_" << dstImageSize.width << "_" << dstImageSize.height << "_" << dstImageSize.depth << "_"
                     << getSampleCountCaseName(samples[samplesIndex]);
            std::ostringstream description;
            params.imageOffset = false;
            group->addChild(new ResolveImageToImageTestCase(testCtx, testName.str(), params));
            params.imageOffset = true;
            if (allocationKind != ALLOCATION_KIND_DEDICATED)
            {
                group->addChild(new ResolveImageToImageTestCase(testCtx, testName.str() + "_bind_offset", params));
            }
        }
    }
}

void addDepthStencilCopyMSAATest(tcu::TestCaseGroup *group, DepthStencilMSAA::TestParameters testCreateParams)
{
    // Run all the tests with one of the bare depth format and one bare stencil format + mandatory combined formats.
    const struct
    {
        const std::string name;
        const VkFormat vkFormat;
    } depthAndStencilFormats[] = {
        {"d32_sfloat", VK_FORMAT_D32_SFLOAT},
        {"s8_uint", VK_FORMAT_S8_UINT},
        {"d16_unorm_s8_uint", VK_FORMAT_D16_UNORM_S8_UINT},
        {"d24_unorm_s8_uint", VK_FORMAT_D24_UNORM_S8_UINT},
    };

    // Both image layouts will be tested only with full image copy tests to limit the number of tests.
    const VkImageLayout srcImageLayouts[] = {VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL};
    const VkImageLayout dstImageLayouts[] = {VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL};

    for (const auto &srcLayout : srcImageLayouts)
    {
        for (const auto &dstLayout : dstImageLayouts)
        {
            testCreateParams.srcImageLayout = srcLayout;
            testCreateParams.dstImageLayout = dstLayout;
            for (const auto &format : depthAndStencilFormats)
            {
                testCreateParams.imageFormat = format.vkFormat;
                const auto textureFormat     = mapVkFormat(format.vkFormat);
                bool hasDepth                = tcu::hasDepthComponent(textureFormat.order);
                bool hasStencil              = tcu::hasStencilComponent(textureFormat.order);
                std::string testNameBase =
                    format.name + "_" +
                    (testCreateParams.copyOptions == DepthStencilMSAA::COPY_WHOLE_IMAGE ?
                         getImageLayoutCaseName(srcLayout) + "_" + getImageLayoutCaseName(dstLayout) + "_" :
                         "");

                if (hasDepth)
                {
                    testCreateParams.copyAspect = VK_IMAGE_ASPECT_DEPTH_BIT;
                    for (auto sample : samples)
                    {
                        testCreateParams.samples     = sample;
                        testCreateParams.imageOffset = false;
                        group->addChild(new DepthStencilMSAATestCase(
                            group->getTestContext(), testNameBase + "D_" + getSampleCountCaseName(sample),
                            testCreateParams));
                        testCreateParams.imageOffset = true;
                        if (testCreateParams.allocationKind != ALLOCATION_KIND_DEDICATED)
                        {
                            group->addChild(new DepthStencilMSAATestCase(
                                group->getTestContext(),
                                testNameBase + "D_" + getSampleCountCaseName(sample) + "_bind_offset",
                                testCreateParams));
                        }
                    }
                }

                if (hasStencil)
                {
                    testCreateParams.copyAspect = VK_IMAGE_ASPECT_STENCIL_BIT;
                    for (auto sample : samples)
                    {
                        testCreateParams.samples     = sample;
                        testCreateParams.imageOffset = false;
                        group->addChild(new DepthStencilMSAATestCase(
                            group->getTestContext(), testNameBase + "S_" + getSampleCountCaseName(sample),
                            testCreateParams));
                        testCreateParams.imageOffset = true;
                        if (testCreateParams.allocationKind != ALLOCATION_KIND_DEDICATED)
                        {
                            group->addChild(new DepthStencilMSAATestCase(
                                group->getTestContext(),
                                testNameBase + "S_" + getSampleCountCaseName(sample) + "_bind_offset",
                                testCreateParams));
                        }
                    }
                }
            }
            if (testCreateParams.copyOptions != DepthStencilMSAA::COPY_WHOLE_IMAGE)
                break;
        }
        if (testCreateParams.copyOptions != DepthStencilMSAA::COPY_WHOLE_IMAGE)
            break;
    }
}

void addDepthStencilCopyMSAATestGroup(tcu::TestCaseGroup *group, AllocationKind allocationKind, uint32_t extensionFlags)
{
    // Allocation kind, extension use copy option parameters are defined here. Rest of the parameters are defined in `addDepthStencilCopyMSAATest` function.
    DepthStencilMSAA::TestParameters testParams = {};
    testParams.allocationKind                   = allocationKind;
    testParams.extensionFlags                   = extensionFlags;

    testParams.copyOptions = DepthStencilMSAA::COPY_WHOLE_IMAGE;
    addTestGroup(group, "whole", addDepthStencilCopyMSAATest, testParams);

    testParams.copyOptions = DepthStencilMSAA::COPY_PARTIAL;
    addTestGroup(group, "partial", addDepthStencilCopyMSAATest, testParams);

    testParams.copyOptions = DepthStencilMSAA::COPY_ARRAY_TO_ARRAY;
    addTestGroup(group, "array_to_array", addDepthStencilCopyMSAATest, testParams);
}

void addBufferCopyOffsetTests(tcu::TestCaseGroup *group)
{
    de::MovePtr<tcu::TestCaseGroup> subGroup(
        new tcu::TestCaseGroup(group->getTestContext(), "buffer_to_buffer_with_offset"));

    for (uint32_t srcOffset = 0u; srcOffset < BufferOffsetParams::kMaxOffset; ++srcOffset)
        for (uint32_t dstOffset = 0u; dstOffset < BufferOffsetParams::kMaxOffset; ++dstOffset)
        {
            BufferOffsetParams params{srcOffset, dstOffset};
            addFunctionCase(subGroup.get(), de::toString(srcOffset) + "_" + de::toString(dstOffset), bufferOffsetTest,
                            params);
        }

    group->addChild(subGroup.release());
}

void addResolveImageTests(tcu::TestCaseGroup *group, AllocationKind allocationKind, uint32_t extensionFlags)
{
    addTestGroup(group, "whole", addResolveImageWholeTests, allocationKind, extensionFlags);
    addTestGroup(group, "partial", addResolveImagePartialTests, allocationKind, extensionFlags);
    addTestGroup(group, "with_regions", addResolveImageWithRegionsTests, allocationKind, extensionFlags);
    addTestGroup(group, "whole_copy_before_resolving", addResolveImageWholeCopyBeforeResolvingTests, allocationKind,
                 extensionFlags);
    addTestGroup(group, "whole_copy_before_resolving_no_cab", addResolveImageWholeCopyWithoutCabBeforeResolvingTests,
                 allocationKind, extensionFlags);
    addComputeAndTransferQueueTests(group, allocationKind, extensionFlags);
    addTestGroup(group, "diff_layout_copy_before_resolving", addResolveImageWholeCopyDiffLayoutsBeforeResolvingTests,
                 allocationKind, extensionFlags);
    addTestGroup(group, "layer_copy_before_resolving", addResolveImageLayerCopyBeforeResolvingTests, allocationKind,
                 extensionFlags);
    addTestGroup(group, "copy_with_regions_before_resolving", addResolveCopyImageWithRegionsTests, allocationKind,
                 extensionFlags);
    addTestGroup(group, "whole_array_image", addResolveImageWholeArrayImageTests, allocationKind, extensionFlags);
    addTestGroup(group, "whole_array_image_one_region", addResolveImageWholeArrayImageSingleRegionTests, allocationKind,
                 extensionFlags);
    addTestGroup(group, "diff_image_size", addResolveImageDiffImageSizeTests, allocationKind, extensionFlags);
}

void addSparseCopyTests(tcu::TestCaseGroup *group, AllocationKind allocationKind, uint32_t extensionFlags)
{
    DE_ASSERT((extensionFlags & COPY_COMMANDS_2) && (extensionFlags & SPARSE_BINDING));

    {
        TestGroupParamsPtr universalGroupParams(new TestGroupParams{
            allocationKind,
            extensionFlags,
            QueueSelectionOptions::Universal,
            false,
            true,
        });
        addTestGroup(group, "image_to_image", addImageToImageTests, universalGroupParams);
    }
}

void addCopiesAndBlittingTests(tcu::TestCaseGroup *group, AllocationKind allocationKind, uint32_t extensionFlags)
{
    TestGroupParamsPtr universalGroupParams(new TestGroupParams{
        allocationKind,
        extensionFlags,
        QueueSelectionOptions::Universal,
        false,
        false,
    });

    addTestGroup(group, "image_to_image", addImageToImageTests, universalGroupParams);
    addTestGroup(group, "image_to_buffer", addImageToBufferTests, universalGroupParams);
    addTestGroup(group, "buffer_to_image", addBufferToImageTests, universalGroupParams);
    addTestGroup(group, "buffer_to_depthstencil", addBufferToDepthStencilTests, allocationKind, extensionFlags);
    addTestGroup(group, "buffer_to_buffer", addBufferToBufferTests, universalGroupParams);
    addTestGroup(group, "blit_image", addBlittingImageTests, allocationKind, extensionFlags);
    addTestGroup(group, "resolve_image", addResolveImageTests, allocationKind, extensionFlags);
    addTestGroup(group, "depth_stencil_msaa_copy", addDepthStencilCopyMSAATestGroup, allocationKind, extensionFlags);

    if (extensionFlags == COPY_COMMANDS_2)
    {
        TestGroupParamsPtr transferOnlyGroup(new TestGroupParams{
            allocationKind,
            extensionFlags,
            QueueSelectionOptions::TransferOnly,
            false,
            false,
        });
        addTestGroup(group, "image_to_image_transfer_queue", addImageToImageTests, transferOnlyGroup);
        addTestGroup(group, "image_to_buffer_transfer_queue", addImageToBufferTests, transferOnlyGroup);
        addTestGroup(group, "buffer_to_image_transfer_queue", addBufferToImageTests, transferOnlyGroup);
        addTestGroup(group, "buffer_to_buffer_transfer_queue", addBufferToBufferTests, transferOnlyGroup);

        TestGroupParamsPtr transferWithSecondaryBuffer(new TestGroupParams{
            allocationKind,
            extensionFlags,
            QueueSelectionOptions::TransferOnly,
            true,
            false,
        });
        addTestGroup(group, "image_to_image_transfer_queue_secondary", addImageToImageTestsSimpleOnly,
                     transferWithSecondaryBuffer);

        TestGroupParamsPtr transferWithSparse(new TestGroupParams{
            allocationKind,
            extensionFlags | SPARSE_BINDING,
            QueueSelectionOptions::TransferOnly,
            false,
            true,
        });
        addTestGroup(group, "image_to_image_transfer_sparse", addImageToImageTestsSimpleOnly, transferWithSparse);
    }
}

void addCoreCopiesAndBlittingTests(tcu::TestCaseGroup *group)
{
    uint32_t extensionFlags = 0;
    addCopiesAndBlittingTests(group, ALLOCATION_KIND_SUBALLOCATED, extensionFlags);
    addBufferCopyOffsetTests(group);
}

void addDedicatedAllocationCopiesAndBlittingTests(tcu::TestCaseGroup *group)
{
    uint32_t extensionFlags = 0;
    addCopiesAndBlittingTests(group, ALLOCATION_KIND_DEDICATED, extensionFlags);
}

// Multiplane image copies on dedicated transfer queues
namespace multiplane
{

using namespace ycbcr;
using tcu::TestLog;
using tcu::UVec2;

struct ImageConfig
{
    ImageConfig(vk::VkFormat format_, vk::VkImageTiling tiling_, bool disjoint_, const UVec2 &size_)
        : format(format_)
        , tiling(tiling_)
        , disjoint(disjoint_)
        , size(size_)
    {
    }

    vk::VkFormat format;
    vk::VkImageTiling tiling;
    bool disjoint;
    tcu::UVec2 size;
};

struct TestConfig
{
    TestConfig(const ImageConfig &src_, const ImageConfig &dst_, const bool intermediateBuffer_)
        : src(src_)
        , dst(dst_)
        , intermediateBuffer(intermediateBuffer_)
    {
    }

    ImageConfig src;
    ImageConfig dst;
    bool intermediateBuffer;
};

void checkFormatSupport(Context &context, const ImageConfig &config)
{
    const auto &instInt(context.getInstanceInterface());

    {
        const vk::VkPhysicalDeviceImageFormatInfo2 imageFormatInfo = {
            vk::VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2,            // sType;
            DE_NULL,                                                              // pNext;
            config.format,                                                        // format;
            vk::VK_IMAGE_TYPE_2D,                                                 // type;
            vk::VK_IMAGE_TILING_OPTIMAL,                                          // tiling;
            vk::VK_IMAGE_USAGE_TRANSFER_DST_BIT | vk::VK_IMAGE_USAGE_SAMPLED_BIT, // usage;
            (vk::VkImageCreateFlags)0u                                            // flags
        };

        vk::VkImageFormatProperties2 imageFormatProperties = {};
        imageFormatProperties.sType                        = vk::VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2;

        vk::VkResult result = instInt.getPhysicalDeviceImageFormatProperties2(context.getPhysicalDevice(),
                                                                              &imageFormatInfo, &imageFormatProperties);
        if (result == vk::VK_ERROR_FORMAT_NOT_SUPPORTED)
            TCU_THROW(NotSupportedError, "Format not supported.");
        VK_CHECK(result);

        // Check for plane compatible format support when the disjoint flag is being used
        if (config.disjoint)
        {
            const vk::PlanarFormatDescription formatDescription = vk::getPlanarFormatDescription(config.format);

            for (uint32_t channelNdx = 0; channelNdx < 4; ++channelNdx)
            {
                if (!formatDescription.hasChannelNdx(channelNdx))
                    continue;
                uint32_t planeNdx                  = formatDescription.channels[channelNdx].planeNdx;
                vk::VkFormat planeCompatibleFormat = getPlaneCompatibleFormat(formatDescription, planeNdx);

                const vk::VkPhysicalDeviceImageFormatInfo2 planeImageFormatInfo = {
                    vk::VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2,            // sType;
                    DE_NULL,                                                              // pNext;
                    planeCompatibleFormat,                                                // format;
                    vk::VK_IMAGE_TYPE_2D,                                                 // type;
                    vk::VK_IMAGE_TILING_OPTIMAL,                                          // tiling;
                    vk::VK_IMAGE_USAGE_TRANSFER_DST_BIT | vk::VK_IMAGE_USAGE_SAMPLED_BIT, // usage;
                    (vk::VkImageCreateFlags)0u                                            // flags
                };

                vk::VkResult planesResult = instInt.getPhysicalDeviceImageFormatProperties2(
                    context.getPhysicalDevice(), &planeImageFormatInfo, &imageFormatProperties);
                if (planesResult == vk::VK_ERROR_FORMAT_NOT_SUPPORTED)
                    TCU_THROW(NotSupportedError, "Plane compatibile format not supported.");
                VK_CHECK(planesResult);
            }
        }
    }

    {
        const vk::VkFormatProperties properties(vk::getPhysicalDeviceFormatProperties(
            context.getInstanceInterface(), context.getPhysicalDevice(), config.format));
        const vk::VkFormatFeatureFlags features(config.tiling == vk::VK_IMAGE_TILING_OPTIMAL ?
                                                    properties.optimalTilingFeatures :
                                                    properties.linearTilingFeatures);

        if ((features & vk::VK_FORMAT_FEATURE_TRANSFER_SRC_BIT) == 0 &&
            (features & vk::VK_FORMAT_FEATURE_TRANSFER_DST_BIT) == 0)
        {
            TCU_THROW(NotSupportedError, "Format doesn't support copies");
        }

        if (config.disjoint && ((features & vk::VK_FORMAT_FEATURE_DISJOINT_BIT) == 0))
            TCU_THROW(NotSupportedError, "Format doesn't support disjoint planes");
    }
}

void checkSupport(Context &context, const TestConfig config)
{
    const vk::VkPhysicalDeviceLimits limits = context.getDeviceProperties().limits;

    if (context.getTransferQueueFamilyIndex() == -1)
        TCU_THROW(NotSupportedError, "Device does not have dedicated transfer queues");

    if (config.src.size.x() > limits.maxImageDimension2D || config.src.size.y() > limits.maxImageDimension2D ||
        config.dst.size.x() > limits.maxImageDimension2D || config.dst.size.y() > limits.maxImageDimension2D)
    {
        TCU_THROW(NotSupportedError, "Requested image dimensions not supported");
    }

    checkFormatSupport(context, config.src);
    checkFormatSupport(context, config.dst);
}

bool isCompatible(vk::VkFormat srcFormat, vk::VkFormat dstFormat)
{
    if (srcFormat == dstFormat)
        return true;
    else
    {
        DE_ASSERT(srcFormat != VK_FORMAT_UNDEFINED && dstFormat != VK_FORMAT_UNDEFINED);

        if (de::contains(DE_ARRAY_BEGIN(compatibleFormats8Bit), DE_ARRAY_END(compatibleFormats8Bit), srcFormat) &&
            de::contains(DE_ARRAY_BEGIN(compatibleFormats8Bit), DE_ARRAY_END(compatibleFormats8Bit), dstFormat))
            return true;

        if (de::contains(DE_ARRAY_BEGIN(compatibleFormats16Bit), DE_ARRAY_END(compatibleFormats16Bit), srcFormat) &&
            de::contains(DE_ARRAY_BEGIN(compatibleFormats16Bit), DE_ARRAY_END(compatibleFormats16Bit), dstFormat))
            return true;

        if (de::contains(DE_ARRAY_BEGIN(compatibleFormats24Bit), DE_ARRAY_END(compatibleFormats24Bit), srcFormat) &&
            de::contains(DE_ARRAY_BEGIN(compatibleFormats24Bit), DE_ARRAY_END(compatibleFormats24Bit), dstFormat))
            return true;

        if (de::contains(DE_ARRAY_BEGIN(compatibleFormats32Bit), DE_ARRAY_END(compatibleFormats32Bit), srcFormat) &&
            de::contains(DE_ARRAY_BEGIN(compatibleFormats32Bit), DE_ARRAY_END(compatibleFormats32Bit), dstFormat))
            return true;

        if (de::contains(DE_ARRAY_BEGIN(compatibleFormats48Bit), DE_ARRAY_END(compatibleFormats48Bit), srcFormat) &&
            de::contains(DE_ARRAY_BEGIN(compatibleFormats48Bit), DE_ARRAY_END(compatibleFormats48Bit), dstFormat))
            return true;

        if (de::contains(DE_ARRAY_BEGIN(compatibleFormats64Bit), DE_ARRAY_END(compatibleFormats64Bit), srcFormat) &&
            de::contains(DE_ARRAY_BEGIN(compatibleFormats64Bit), DE_ARRAY_END(compatibleFormats64Bit), dstFormat))
            return true;

        if (de::contains(DE_ARRAY_BEGIN(compatibleFormats96Bit), DE_ARRAY_END(compatibleFormats96Bit), srcFormat) &&
            de::contains(DE_ARRAY_BEGIN(compatibleFormats96Bit), DE_ARRAY_END(compatibleFormats96Bit), dstFormat))
            return true;

        if (de::contains(DE_ARRAY_BEGIN(compatibleFormats128Bit), DE_ARRAY_END(compatibleFormats128Bit), srcFormat) &&
            de::contains(DE_ARRAY_BEGIN(compatibleFormats128Bit), DE_ARRAY_END(compatibleFormats128Bit), dstFormat))
            return true;

        if (de::contains(DE_ARRAY_BEGIN(compatibleFormats192Bit), DE_ARRAY_END(compatibleFormats192Bit), srcFormat) &&
            de::contains(DE_ARRAY_BEGIN(compatibleFormats192Bit), DE_ARRAY_END(compatibleFormats192Bit), dstFormat))
            return true;

        if (de::contains(DE_ARRAY_BEGIN(compatibleFormats256Bit), DE_ARRAY_END(compatibleFormats256Bit), srcFormat) &&
            de::contains(DE_ARRAY_BEGIN(compatibleFormats256Bit), DE_ARRAY_END(compatibleFormats256Bit), dstFormat))
            return true;

        return false;
    }
}

UVec2 randomUVec2(de::Random &rng, const UVec2 &min, const UVec2 &max)
{
    UVec2 result;

    result[0] = min[0] + (rng.getUint32() % (1 + max[0] - min[0]));
    result[1] = min[1] + (rng.getUint32() % (1 + max[1] - min[1]));

    return result;
}

void genCopies(de::Random &rng, size_t copyCount, vk::VkFormat srcFormat, const UVec2 &srcSize, vk::VkFormat dstFormat,
               const UVec2 &dstSize, std::vector<vk::VkImageCopy> *copies)
{
    std::vector<std::pair<uint32_t, uint32_t>> pairs;
    const vk::PlanarFormatDescription srcPlaneInfo(vk::getPlanarFormatDescription(srcFormat));
    const vk::PlanarFormatDescription dstPlaneInfo(vk::getPlanarFormatDescription(dstFormat));

    for (uint32_t srcPlaneNdx = 0; srcPlaneNdx < srcPlaneInfo.numPlanes; srcPlaneNdx++)
    {
        for (uint32_t dstPlaneNdx = 0; dstPlaneNdx < dstPlaneInfo.numPlanes; dstPlaneNdx++)
        {
            const vk::VkFormat srcPlaneFormat(getPlaneCompatibleFormat(srcPlaneInfo, srcPlaneNdx));
            const vk::VkFormat dstPlaneFormat(getPlaneCompatibleFormat(dstPlaneInfo, dstPlaneNdx));

            if (isCompatible(srcPlaneFormat, dstPlaneFormat))
                pairs.push_back(std::make_pair(srcPlaneNdx, dstPlaneNdx));
        }
    }

    DE_ASSERT(!pairs.empty());

    copies->reserve(copyCount);

    for (size_t copyNdx = 0; copyNdx < copyCount; copyNdx++)
    {
        const std::pair<uint32_t, uint32_t> planes(
            rng.choose<std::pair<uint32_t, uint32_t>>(pairs.begin(), pairs.end()));

        const uint32_t srcPlaneNdx(planes.first);
        const vk::VkFormat srcPlaneFormat(getPlaneCompatibleFormat(srcPlaneInfo, srcPlaneNdx));
        const UVec2 srcBlockExtent(getBlockExtent(srcPlaneFormat));
        const UVec2 srcPlaneExtent(getPlaneExtent(srcPlaneInfo, srcSize, srcPlaneNdx, 0));
        const UVec2 srcPlaneBlockExtent(srcPlaneExtent / srcBlockExtent);

        const uint32_t dstPlaneNdx(planes.second);
        const vk::VkFormat dstPlaneFormat(getPlaneCompatibleFormat(dstPlaneInfo, dstPlaneNdx));
        const UVec2 dstBlockExtent(getBlockExtent(dstPlaneFormat));
        const UVec2 dstPlaneExtent(getPlaneExtent(dstPlaneInfo, dstSize, dstPlaneNdx, 0));
        const UVec2 dstPlaneBlockExtent(dstPlaneExtent / dstBlockExtent);

        const UVec2 copyBlockExtent(
            randomUVec2(rng, UVec2(1u, 1u), tcu::min(srcPlaneBlockExtent, dstPlaneBlockExtent)));
        const UVec2 srcOffset(srcBlockExtent * randomUVec2(rng, UVec2(0u, 0u), srcPlaneBlockExtent - copyBlockExtent));
        const UVec2 dstOffset(dstBlockExtent * randomUVec2(rng, UVec2(0u, 0u), dstPlaneBlockExtent - copyBlockExtent));
        const UVec2 copyExtent(copyBlockExtent * srcBlockExtent);
        const vk::VkImageCopy copy = {
            // src
            {static_cast<vk::VkImageAspectFlags>(srcPlaneInfo.numPlanes > 1 ? vk::getPlaneAspect(srcPlaneNdx) :
                                                                              vk::VK_IMAGE_ASPECT_COLOR_BIT),
             0u, 0u, 1u},
            {
                (int32_t)srcOffset.x(),
                (int32_t)srcOffset.y(),
                0,
            },
            // dst
            {static_cast<vk::VkImageAspectFlags>(dstPlaneInfo.numPlanes > 1 ? vk::getPlaneAspect(dstPlaneNdx) :
                                                                              vk::VK_IMAGE_ASPECT_COLOR_BIT),
             0u, 0u, 1u},
            {
                (int32_t)dstOffset.x(),
                (int32_t)dstOffset.y(),
                0,
            },
            // size
            {copyExtent.x(), copyExtent.y(), 1u}};

        copies->push_back(copy);
    }
}

tcu::SeedBuilder &operator<<(tcu::SeedBuilder &builder, const ImageConfig &config)
{

    builder << (uint32_t)config.format << (uint32_t)config.tiling << config.disjoint << config.size[0]
            << config.size[1];
    return builder;
}

void logImageInfo(TestLog &log, const ImageConfig &config)
{
    log << TestLog::Message << "Format: " << config.format << TestLog::EndMessage;
    log << TestLog::Message << "Tiling: " << config.tiling << TestLog::EndMessage;
    log << TestLog::Message << "Size: " << config.size << TestLog::EndMessage;
    log << TestLog::Message << "Disjoint: " << (config.disjoint ? "true" : "false") << TestLog::EndMessage;
}

void logTestCaseInfo(TestLog &log, const TestConfig &config, const std::vector<vk::VkImageCopy> &copies)
{
    {
        const tcu::ScopedLogSection section(log, "SourceImage", "SourceImage");
        logImageInfo(log, config.src);
    }

    {
        const tcu::ScopedLogSection section(log, "DestinationImage", "DestinationImage");
        logImageInfo(log, config.dst);
    }
    {
        const tcu::ScopedLogSection section(log, "Copies", "Copies");

        for (size_t copyNdx = 0; copyNdx < copies.size(); copyNdx++)
            log << TestLog::Message << copies[copyNdx] << TestLog::EndMessage;
    }
}

vk::VkFormat chooseFloatFormat(vk::VkFormat srcFormat, vk::VkFormat dstFormat)
{
    const std::vector<vk::VkFormat> floatFormats = {
        vk::VK_FORMAT_B10G11R11_UFLOAT_PACK32, vk::VK_FORMAT_R16_SFLOAT,
        vk::VK_FORMAT_R16G16_SFLOAT,           vk::VK_FORMAT_R16G16B16_SFLOAT,
        vk::VK_FORMAT_R16G16B16A16_SFLOAT,     vk::VK_FORMAT_R32_SFLOAT,
        vk::VK_FORMAT_R32G32_SFLOAT,           vk::VK_FORMAT_R32G32B32_SFLOAT,
        vk::VK_FORMAT_R32G32B32A32_SFLOAT,     vk::VK_FORMAT_R64_SFLOAT,
        vk::VK_FORMAT_R64G64_SFLOAT,           vk::VK_FORMAT_R64G64B64_SFLOAT,
        vk::VK_FORMAT_R64G64B64A64_SFLOAT,
    };

    if (std::find(floatFormats.begin(), floatFormats.end(), srcFormat) != floatFormats.end())
        return srcFormat;

    return dstFormat;
}

vk::Move<vk::VkImage> createImage(const vk::DeviceInterface &vkd, vk::VkDevice device, vk::VkFormat format,
                                  const UVec2 &size, bool disjoint, vk::VkImageTiling tiling)
{
    const vk::VkImageCreateInfo createInfo = {
        vk::VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        DE_NULL,
        disjoint ? (vk::VkImageCreateFlags)vk::VK_IMAGE_CREATE_DISJOINT_BIT : (vk::VkImageCreateFlags)0u,

        vk::VK_IMAGE_TYPE_2D,
        format,
        vk::makeExtent3D(size.x(), size.y(), 1u),
        1u,
        1u,
        vk::VK_SAMPLE_COUNT_1_BIT,
        tiling,
        vk::VK_IMAGE_USAGE_TRANSFER_SRC_BIT | vk::VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        vk::VK_SHARING_MODE_EXCLUSIVE,
        0u,
        (const uint32_t *)DE_NULL,
        tiling == vk::VK_IMAGE_TILING_LINEAR ? vk::VK_IMAGE_LAYOUT_PREINITIALIZED : vk::VK_IMAGE_LAYOUT_UNDEFINED,
    };

    return vk::createImage(vkd, device, &createInfo);
}

uint32_t getBlockByteSize(vk::VkFormat format)
{
    switch (format)
    {
    case vk::VK_FORMAT_B8G8R8G8_422_UNORM:
    case vk::VK_FORMAT_G8B8G8R8_422_UNORM:
        return 4u;

    case vk::VK_FORMAT_B10X6G10X6R10X6G10X6_422_UNORM_4PACK16:
    case vk::VK_FORMAT_B12X4G12X4R12X4G12X4_422_UNORM_4PACK16:
    case vk::VK_FORMAT_B16G16R16G16_422_UNORM:
    case vk::VK_FORMAT_G10X6B10X6G10X6R10X6_422_UNORM_4PACK16:
    case vk::VK_FORMAT_G12X4B12X4G12X4R12X4_422_UNORM_4PACK16:
    case vk::VK_FORMAT_G16B16G16R16_422_UNORM:
    case vk::VK_FORMAT_R10X6G10X6B10X6A10X6_UNORM_4PACK16:
    case vk::VK_FORMAT_R12X4G12X4B12X4A12X4_UNORM_4PACK16:
    case vk::VK_FORMAT_R16G16B16A16_UNORM:
        return 4u * 2u;

    case vk::VK_FORMAT_R10X6_UNORM_PACK16:
    case vk::VK_FORMAT_R12X4_UNORM_PACK16:
        return 2u;

    case vk::VK_FORMAT_R10X6G10X6_UNORM_2PACK16:
    case vk::VK_FORMAT_R12X4G12X4_UNORM_2PACK16:
        return 2u * 2u;

    case vk::VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16:
    case vk::VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_444_UNORM_3PACK16:
        return 3u * 2u;

    case vk::VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16:
    case vk::VK_FORMAT_G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16:
    case vk::VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16:
    case vk::VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16:
    case vk::VK_FORMAT_G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16:
    case vk::VK_FORMAT_G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16:
    case vk::VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16:
    case vk::VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16:
    case vk::VK_FORMAT_G16_B16R16_2PLANE_420_UNORM:
    case vk::VK_FORMAT_G16_B16R16_2PLANE_422_UNORM:
    case vk::VK_FORMAT_G16_B16_R16_3PLANE_420_UNORM:
    case vk::VK_FORMAT_G16_B16_R16_3PLANE_422_UNORM:
    case vk::VK_FORMAT_G16_B16_R16_3PLANE_444_UNORM:
    case vk::VK_FORMAT_G8_B8R8_2PLANE_420_UNORM:
    case vk::VK_FORMAT_G8_B8R8_2PLANE_422_UNORM:
    case vk::VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM:
    case vk::VK_FORMAT_G8_B8_R8_3PLANE_422_UNORM:
    case vk::VK_FORMAT_G8_B8_R8_3PLANE_444_UNORM:
    case vk::VK_FORMAT_G8_B8R8_2PLANE_444_UNORM_EXT:
    case vk::VK_FORMAT_G10X6_B10X6R10X6_2PLANE_444_UNORM_3PACK16_EXT:
    case vk::VK_FORMAT_G12X4_B12X4R12X4_2PLANE_444_UNORM_3PACK16_EXT:
    case vk::VK_FORMAT_G16_B16R16_2PLANE_444_UNORM_EXT:
        DE_FATAL("Plane formats not supported");
        return ~0u;

    default:
        return (uint32_t)vk::mapVkFormat(format).getPixelSize();
    }
}

bool isCopyCompatible(vk::VkFormat srcFormat, vk::VkFormat dstFormat)
{
    if (isYCbCrFormat(srcFormat) && isYCbCrFormat(dstFormat))
    {
        const vk::PlanarFormatDescription srcPlaneInfo(vk::getPlanarFormatDescription(srcFormat));
        const vk::PlanarFormatDescription dstPlaneInfo(vk::getPlanarFormatDescription(dstFormat));

        for (uint32_t srcPlaneNdx = 0; srcPlaneNdx < srcPlaneInfo.numPlanes; srcPlaneNdx++)
        {
            for (uint32_t dstPlaneNdx = 0; dstPlaneNdx < dstPlaneInfo.numPlanes; dstPlaneNdx++)
            {
                const vk::VkFormat srcPlaneFormat(getPlaneCompatibleFormat(srcFormat, srcPlaneNdx));
                const vk::VkFormat dstPlaneFormat(getPlaneCompatibleFormat(dstFormat, dstPlaneNdx));

                if (isCompatible(srcPlaneFormat, dstPlaneFormat))
                    return true;
            }
        }
    }
    else if (isYCbCrFormat(srcFormat))
    {
        const vk::PlanarFormatDescription srcPlaneInfo(vk::getPlanarFormatDescription(srcFormat));

        for (uint32_t srcPlaneNdx = 0; srcPlaneNdx < srcPlaneInfo.numPlanes; srcPlaneNdx++)
        {
            const vk::VkFormat srcPlaneFormat(getPlaneCompatibleFormat(srcFormat, srcPlaneNdx));

            if (isCompatible(srcPlaneFormat, dstFormat))
                return true;
        }
    }
    else if (isYCbCrFormat(dstFormat))
    {
        const vk::PlanarFormatDescription dstPlaneInfo(vk::getPlanarFormatDescription(dstFormat));

        for (uint32_t dstPlaneNdx = 0; dstPlaneNdx < dstPlaneInfo.numPlanes; dstPlaneNdx++)
        {
            const vk::VkFormat dstPlaneFormat(getPlaneCompatibleFormat(dstFormat, dstPlaneNdx));

            if (isCompatible(dstPlaneFormat, srcFormat))
                return true;
        }
    }
    else
        return isCompatible(srcFormat, dstFormat);

    return false;
}

tcu::TestStatus testCopies(Context &context, TestConfig config)
{
    const size_t copyCount = 10;
    auto &log(context.getTestContext().getLog());

    MultiPlaneImageData srcData(config.src.format, config.src.size);
    MultiPlaneImageData dstData(config.dst.format, config.dst.size);
    MultiPlaneImageData result(config.dst.format, config.dst.size);

    std::vector<vk::VkImageCopy> copies;

    tcu::SeedBuilder builder;
    builder << 6792903u << config.src << config.dst;

    de::Random rng(builder.get());
    const bool noNan = true;
    genCopies(rng, copyCount, config.src.format, config.src.size, config.dst.format, config.dst.size, &copies);
    logTestCaseInfo(log, config, copies);

    // To avoid putting NaNs in dst in the image copy
    fillRandom(&rng, &srcData, chooseFloatFormat(config.src.format, config.dst.format), noNan);
    fillRandom(&rng, &dstData, config.dst.format, noNan);

    {
        const vk::DeviceInterface &vkd(context.getDeviceInterface());
        const vk::VkDevice device(context.getDevice());

        const vk::Unique<vk::VkImage> srcImage(
            createImage(vkd, device, config.src.format, config.src.size, config.src.disjoint, config.src.tiling));
        const vk::MemoryRequirement srcMemoryRequirement(config.src.tiling == vk::VK_IMAGE_TILING_OPTIMAL ?
                                                             vk::MemoryRequirement::Any :
                                                             vk::MemoryRequirement::HostVisible);
        const vk::VkImageCreateFlags srcCreateFlags(config.src.disjoint ? vk::VK_IMAGE_CREATE_DISJOINT_BIT :
                                                                          (vk::VkImageCreateFlagBits)0u);
        const std::vector<AllocationSp> srcImageMemory(
            allocateAndBindImageMemory(vkd, device, context.getDefaultAllocator(), *srcImage, config.src.format,
                                       srcCreateFlags, srcMemoryRequirement));

        const vk::Unique<vk::VkImage> dstImage(
            createImage(vkd, device, config.dst.format, config.dst.size, config.dst.disjoint, config.dst.tiling));
        const vk::MemoryRequirement dstMemoryRequirement(config.dst.tiling == vk::VK_IMAGE_TILING_OPTIMAL ?
                                                             vk::MemoryRequirement::Any :
                                                             vk::MemoryRequirement::HostVisible);
        const vk::VkImageCreateFlags dstCreateFlags(config.dst.disjoint ? vk::VK_IMAGE_CREATE_DISJOINT_BIT :
                                                                          (vk::VkImageCreateFlagBits)0u);
        const std::vector<AllocationSp> dstImageMemory(
            allocateAndBindImageMemory(vkd, device, context.getDefaultAllocator(), *dstImage, config.dst.format,
                                       dstCreateFlags, dstMemoryRequirement));

        if (config.src.tiling == vk::VK_IMAGE_TILING_OPTIMAL)
            uploadImage(vkd, device, context.getUniversalQueueFamilyIndex(), context.getDefaultAllocator(), *srcImage,
                        srcData, vk::VK_ACCESS_TRANSFER_READ_BIT, vk::VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
        else
            fillImageMemory(vkd, device, context.getUniversalQueueFamilyIndex(), *srcImage, srcImageMemory, srcData,
                            vk::VK_ACCESS_TRANSFER_READ_BIT, vk::VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

        if (config.dst.tiling == vk::VK_IMAGE_TILING_OPTIMAL)
            uploadImage(vkd, device, context.getUniversalQueueFamilyIndex(), context.getDefaultAllocator(), *dstImage,
                        dstData, vk::VK_ACCESS_TRANSFER_WRITE_BIT, vk::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        else
            fillImageMemory(vkd, device, context.getUniversalQueueFamilyIndex(), *dstImage, dstImageMemory, dstData,
                            vk::VK_ACCESS_TRANSFER_WRITE_BIT, vk::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

        {
            const uint32_t transferQueueFamilyNdx(context.getTransferQueueFamilyIndex());
            const vk::VkQueue transferQueue(context.getTransferQueue());
            const vk::Unique<vk::VkCommandPool> transferCmdPool(
                createCommandPool(vkd, device, (vk::VkCommandPoolCreateFlags)0, transferQueueFamilyNdx));
            const vk::Unique<vk::VkCommandBuffer> transferCmdBuffer(
                allocateCommandBuffer(vkd, device, *transferCmdPool, vk::VK_COMMAND_BUFFER_LEVEL_PRIMARY));

            beginCommandBuffer(vkd, *transferCmdBuffer);

            std::vector<de::MovePtr<vk::BufferWithMemory>> buffers(copies.size());

            for (size_t i = 0; i < copies.size(); i++)
            {
                const uint32_t srcPlaneNdx(
                    copies[i].srcSubresource.aspectMask != vk::VK_IMAGE_ASPECT_COLOR_BIT ?
                        vk::getAspectPlaneNdx((vk::VkImageAspectFlagBits)copies[i].srcSubresource.aspectMask) :
                        0u);

                const vk::VkFormat srcPlaneFormat(getPlaneCompatibleFormat(config.src.format, srcPlaneNdx));

                const uint32_t blockSizeBytes(getBlockByteSize(srcPlaneFormat));
                const vk::VkDeviceSize bufferSize = config.src.size.x() * config.src.size.y() * blockSizeBytes;
                const vk::VkBufferCreateInfo bufferCreateInfo = {
                    vk::VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, // VkStructureType sType;
                    DE_NULL,                                  // const void* pNext;
                    0u,                                       // VkBufferCreateFlags flags;
                    bufferSize,                               // VkDeviceSize size;
                    vk::VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                        vk::VK_BUFFER_USAGE_TRANSFER_DST_BIT, // VkBufferUsageFlags usage;
                    vk::VK_SHARING_MODE_EXCLUSIVE,            // VkSharingMode sharingMode;
                    0u,                                       // uint32_t queueFamilyIndexCount;
                    (const uint32_t *)DE_NULL,                // const uint32_t* pQueueFamilyIndices;
                };
                buffers[i] = de::MovePtr<vk::BufferWithMemory>(new vk::BufferWithMemory(
                    vkd, device, context.getDefaultAllocator(), bufferCreateInfo, vk::MemoryRequirement::Any));

                if (config.intermediateBuffer)
                {
                    const vk::VkBufferImageCopy imageToBufferCopy = {
                        0u,                       // VkDeviceSize bufferOffset;
                        0u,                       // uint32_t bufferRowLength;
                        0u,                       // uint32_t bufferImageHeight;
                        copies[i].srcSubresource, // VkImageSubresourceLayers imageSubresource;
                        copies[i].srcOffset,      // VkOffset3D imageOffset;
                        copies[i].extent,         // VkExtent3D imageExtent;
                    };
                    vkd.cmdCopyImageToBuffer(*transferCmdBuffer, *srcImage, vk::VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                             **buffers[i], 1, &imageToBufferCopy);

                    const vk::VkBufferMemoryBarrier bufferBarrier = {
                        vk::VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER, // VkStructureType sType;
                        DE_NULL,                                     // const void* pNext;
                        vk::VK_ACCESS_TRANSFER_WRITE_BIT,            // VkAccessFlags srcAccessMask;
                        vk::VK_ACCESS_TRANSFER_READ_BIT,             // VkAccessFlags dstAccessMask;
                        VK_QUEUE_FAMILY_IGNORED,                     // uint32_t srcQueueFamilyIndex;
                        VK_QUEUE_FAMILY_IGNORED,                     // uint32_t dstQueueFamilyIndex;
                        **buffers[i],                                // VkBuffer buffer;
                        0u,                                          // VkDeviceSize offset;
                        VK_WHOLE_SIZE,                               // VkDeviceSize size;
                    };

                    vkd.cmdPipelineBarrier(*transferCmdBuffer,
                                           (vk::VkPipelineStageFlags)vk::VK_PIPELINE_STAGE_TRANSFER_BIT,
                                           (vk::VkPipelineStageFlags)vk::VK_PIPELINE_STAGE_TRANSFER_BIT,
                                           (vk::VkDependencyFlags)0u, 0u, (const vk::VkMemoryBarrier *)DE_NULL, 1u,
                                           &bufferBarrier, 0u, (const vk::VkImageMemoryBarrier *)DE_NULL);

                    const vk::VkBufferImageCopy bufferToImageCopy = {
                        0u,                       // VkDeviceSize bufferOffset;
                        0u,                       // uint32_t bufferRowLength;
                        0u,                       // uint32_t bufferImageHeight;
                        copies[i].dstSubresource, // VkImageSubresourceLayers imageSubresource;
                        copies[i].dstOffset,      // VkOffset3D imageOffset;
                        copies[i].extent,         // VkExtent3D imageExtent;
                    };
                    vkd.cmdCopyBufferToImage(*transferCmdBuffer, **buffers[i], *dstImage,
                                             vk::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &bufferToImageCopy);
                }
                else
                {
                    vkd.cmdCopyImage(*transferCmdBuffer, *srcImage, vk::VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, *dstImage,
                                     vk::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copies[i]);
                }

                const vk::VkImageMemoryBarrier preCopyBarrier = {vk::VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                                                                 DE_NULL,
                                                                 vk::VK_ACCESS_TRANSFER_WRITE_BIT,
                                                                 vk::VK_ACCESS_TRANSFER_READ_BIT |
                                                                     vk::VK_ACCESS_TRANSFER_WRITE_BIT,
                                                                 vk::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                                                 vk::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                                                 VK_QUEUE_FAMILY_IGNORED,
                                                                 VK_QUEUE_FAMILY_IGNORED,
                                                                 *dstImage,
                                                                 {vk::VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u}};

                vkd.cmdPipelineBarrier(*transferCmdBuffer, (vk::VkPipelineStageFlags)vk::VK_PIPELINE_STAGE_TRANSFER_BIT,
                                       (vk::VkPipelineStageFlags)vk::VK_PIPELINE_STAGE_TRANSFER_BIT,
                                       (vk::VkDependencyFlags)0u, 0u, (const vk::VkMemoryBarrier *)DE_NULL, 0u,
                                       (const vk::VkBufferMemoryBarrier *)DE_NULL, 1u, &preCopyBarrier);
            }

            endCommandBuffer(vkd, *transferCmdBuffer);

            submitCommandsAndWait(vkd, device, transferQueue, *transferCmdBuffer);
        }

        if (config.dst.tiling == vk::VK_IMAGE_TILING_OPTIMAL)
            downloadImage(vkd, device, context.getTransferQueueFamilyIndex(), context.getDefaultAllocator(), *dstImage,
                          &result, vk::VK_ACCESS_TRANSFER_WRITE_BIT, vk::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        else
            readImageMemory(vkd, device, context.getTransferQueueFamilyIndex(), *dstImage, dstImageMemory, &result,
                            vk::VK_ACCESS_TRANSFER_WRITE_BIT, vk::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    }

    {
        MultiPlaneImageData reference(dstData);
        const size_t maxErrorCount = 30;
        size_t errorCount          = 0;

        for (size_t copyNdx = 0; copyNdx < copies.size(); copyNdx++)
        {
            const vk::VkImageCopy &copy(copies[copyNdx]);

            const uint32_t srcPlaneNdx(
                copy.srcSubresource.aspectMask != vk::VK_IMAGE_ASPECT_COLOR_BIT ?
                    vk::getAspectPlaneNdx((vk::VkImageAspectFlagBits)copy.srcSubresource.aspectMask) :
                    0u);
            const UVec2 srcPlaneExtent(getPlaneExtent(srcData.getDescription(), config.src.size, srcPlaneNdx, 0));

            const vk::VkFormat srcPlaneFormat(getPlaneCompatibleFormat(config.src.format, srcPlaneNdx));
            const UVec2 srcBlockExtent(getBlockExtent(srcPlaneFormat));

            const uint32_t blockSizeBytes(getBlockByteSize(srcPlaneFormat));

            const UVec2 srcPlaneBlockExtent(srcPlaneExtent / srcBlockExtent);
            const UVec2 srcBlockOffset(copy.srcOffset.x / srcBlockExtent.x(), copy.srcOffset.y / srcBlockExtent.y());
            const UVec2 srcBlockPitch(blockSizeBytes, blockSizeBytes * srcPlaneBlockExtent.x());

            const uint32_t dstPlaneNdx(
                copy.dstSubresource.aspectMask != vk::VK_IMAGE_ASPECT_COLOR_BIT ?
                    vk::getAspectPlaneNdx((vk::VkImageAspectFlagBits)copy.dstSubresource.aspectMask) :
                    0u);
            const UVec2 dstPlaneExtent(getPlaneExtent(dstData.getDescription(), config.dst.size, dstPlaneNdx, 0));

            const vk::VkFormat dstPlaneFormat(getPlaneCompatibleFormat(config.dst.format, dstPlaneNdx));
            const UVec2 dstBlockExtent(getBlockExtent(dstPlaneFormat));

            const UVec2 dstPlaneBlockExtent(dstPlaneExtent / dstBlockExtent);
            const UVec2 dstBlockOffset(copy.dstOffset.x / dstBlockExtent.x(), copy.dstOffset.y / dstBlockExtent.y());
            const UVec2 dstBlockPitch(blockSizeBytes, blockSizeBytes * dstPlaneBlockExtent.x());

            const UVec2 blockExtent(copy.extent.width / srcBlockExtent.x(), copy.extent.height / srcBlockExtent.y());

            DE_ASSERT(blockSizeBytes == getBlockByteSize(dstPlaneFormat));

            for (uint32_t y = 0; y < blockExtent.y(); y++)
            {
                const uint32_t size   = blockExtent.x() * blockSizeBytes;
                const uint32_t srcPos = tcu::dot(srcBlockPitch, UVec2(srcBlockOffset.x(), srcBlockOffset.y() + y));
                const uint32_t dstPos = tcu::dot(dstBlockPitch, UVec2(dstBlockOffset.x(), dstBlockOffset.y() + y));

                deMemcpy(((uint8_t *)reference.getPlanePtr(dstPlaneNdx)) + dstPos,
                         ((const uint8_t *)srcData.getPlanePtr(srcPlaneNdx)) + srcPos, size);
            }
        }

        bool ignoreLsb6Bits = areLsb6BitsDontCare(srcData.getFormat(), dstData.getFormat());
        bool ignoreLsb4Bits = areLsb4BitsDontCare(srcData.getFormat(), dstData.getFormat());

        for (uint32_t planeNdx = 0; planeNdx < result.getDescription().numPlanes; ++planeNdx)
        {
            uint32_t planeSize = vk::getPlaneSizeInBytes(result.getDescription(), result.getSize(), planeNdx, 0u, 1u);
            for (size_t byteNdx = 0; byteNdx < planeSize; byteNdx++)
            {
                const uint8_t res = ((const uint8_t *)result.getPlanePtr(planeNdx))[byteNdx];
                const uint8_t ref = ((const uint8_t *)reference.getPlanePtr(planeNdx))[byteNdx];

                uint8_t mask = 0xFF;
                if (!(byteNdx & 0x01) && (ignoreLsb6Bits))
                    mask = 0xC0;
                else if (!(byteNdx & 0x01) && (ignoreLsb4Bits))
                    mask = 0xF0;

                if ((res & mask) != (ref & mask))
                {
                    log << TestLog::Message << "Plane: " << planeNdx << ", Offset: " << byteNdx
                        << ", Expected: " << (uint32_t)(ref & mask) << ", Got: " << (uint32_t)(res & mask)
                        << TestLog::EndMessage;
                    errorCount++;

                    if (errorCount > maxErrorCount)
                        break;
                }
            }

            if (errorCount > maxErrorCount)
                break;
        }

        if (errorCount > 0)
            return tcu::TestStatus::fail(
                "Failed, found " +
                (errorCount > maxErrorCount ? de::toString(maxErrorCount) + "+" : de::toString(errorCount)) +
                " incorrect bytes");
        else
            return tcu::TestStatus::pass("Pass");
    }
}

tcu::TestCaseGroup *createMultiplaneCopyTests(tcu::TestContext &testCtx)
{
    const struct
    {
        VkImageTiling value;
        const char *name;
    } tilings[] = {{VK_IMAGE_TILING_OPTIMAL, "optimal"}, {VK_IMAGE_TILING_LINEAR, "linear"}};

    const VkFormat multiplaneFormats[] = {VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM,
                                          VK_FORMAT_G8_B8R8_2PLANE_420_UNORM,
                                          VK_FORMAT_G8_B8_R8_3PLANE_422_UNORM,
                                          VK_FORMAT_G8_B8R8_2PLANE_422_UNORM,
                                          VK_FORMAT_G8_B8_R8_3PLANE_444_UNORM,
                                          VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16,
                                          VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16,
                                          VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16,
                                          VK_FORMAT_G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16,
                                          VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16,
                                          VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16,
                                          VK_FORMAT_G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16,
                                          VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16,
                                          VK_FORMAT_G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16,
                                          VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_444_UNORM_3PACK16,
                                          VK_FORMAT_G16_B16_R16_3PLANE_420_UNORM,
                                          VK_FORMAT_G16_B16R16_2PLANE_420_UNORM,
                                          VK_FORMAT_G16_B16_R16_3PLANE_422_UNORM,
                                          VK_FORMAT_G16_B16R16_2PLANE_422_UNORM};

    std::vector<VkImageCreateFlags> createFlags{
        VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT,
        VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT,
        VK_IMAGE_CREATE_ALIAS_BIT
        /* VK_IMAGE_CREATE_SPLIT_INSTANCE_BIND_REGIONS_BIT - present tests use only one physical device */
        ,
        VK_IMAGE_CREATE_2D_ARRAY_COMPATIBLE_BIT
        /* VK_IMAGE_CREATE_BLOCK_TEXEL_VIEW_COMPATIBLE_BIT not apply with planar formats */
        ,
        VK_IMAGE_CREATE_EXTENDED_USAGE_BIT,
        VK_IMAGE_CREATE_SAMPLE_LOCATIONS_COMPATIBLE_DEPTH_BIT_EXT};

    de::MovePtr<tcu::TestCaseGroup> multiplaneGroup(new tcu::TestCaseGroup(testCtx, "multiplanar_xfer"));

    for (size_t srcFormatNdx = 0; srcFormatNdx < de::arrayLength(multiplaneFormats); srcFormatNdx++)
    {
        const vk::VkFormat srcFormat(multiplaneFormats[srcFormatNdx]);
        const UVec2 srcSize(isYCbCrFormat(srcFormat) ? UVec2(24u, 16u) : UVec2(23u, 17u));
        const std::string srcFormatName(de::toLower(std::string(getFormatName(srcFormat)).substr(10)));
        de::MovePtr<tcu::TestCaseGroup> srcFormatGroup(new tcu::TestCaseGroup(testCtx, srcFormatName.c_str()));
        for (size_t dstFormatNdx = 0; dstFormatNdx < DE_LENGTH_OF_ARRAY(multiplaneFormats); dstFormatNdx++)
        {
            const vk::VkFormat dstFormat(multiplaneFormats[dstFormatNdx]);
            const UVec2 dstSize(isYCbCrFormat(dstFormat) ? UVec2(24u, 16u) : UVec2(23u, 17u));
            const std::string dstFormatName(de::toLower(std::string(getFormatName(dstFormat)).substr(10)));

            if ((!vk::isYCbCrFormat(srcFormat) && !vk::isYCbCrFormat(dstFormat)) ||
                !isCopyCompatible(srcFormat, dstFormat))
                continue;

            de::MovePtr<tcu::TestCaseGroup> dstFormatGroup(new tcu::TestCaseGroup(testCtx, dstFormatName.c_str()));
            for (size_t srcTilingNdx = 0; srcTilingNdx < DE_LENGTH_OF_ARRAY(tilings); srcTilingNdx++)
            {
                const vk::VkImageTiling srcTiling = tilings[srcTilingNdx].value;
                const char *const srcTilingName   = tilings[srcTilingNdx].name;

                for (size_t dstTilingNdx = 0; dstTilingNdx < DE_LENGTH_OF_ARRAY(tilings); dstTilingNdx++)
                {
                    const vk::VkImageTiling dstTiling = tilings[dstTilingNdx].value;
                    const char *const dstTilingName   = tilings[dstTilingNdx].name;

                    if (srcTiling == VK_IMAGE_TILING_LINEAR || dstTiling == VK_IMAGE_TILING_LINEAR)
                        continue;

                    for (size_t srcDisjointNdx = 0; srcDisjointNdx < 2; srcDisjointNdx++)
                        for (size_t dstDisjointNdx = 0; dstDisjointNdx < 2; dstDisjointNdx++)
                            for (size_t useBufferNdx = 0; useBufferNdx < 2; useBufferNdx++)
                            {
                                const bool srcDisjoint = srcDisjointNdx == 1;
                                const bool dstDisjoint = dstDisjointNdx == 1;
                                const bool useBuffer   = useBufferNdx == 1;
                                const TestConfig config(ImageConfig(srcFormat, srcTiling, srcDisjoint, srcSize),
                                                        ImageConfig(dstFormat, dstTiling, dstDisjoint, dstSize),
                                                        useBuffer);

                                addFunctionCase(dstFormatGroup.get(),
                                                std::string(srcTilingName) + (srcDisjoint ? "_disjoint_" : "_") +
                                                    (useBuffer ? "buffer_" : "") + std::string(dstTilingName) +
                                                    (dstDisjoint ? "_disjoint" : ""),
                                                checkSupport, testCopies, config);
                            }
                }
            }
            srcFormatGroup->addChild(dstFormatGroup.release());
        }

        multiplaneGroup->addChild(srcFormatGroup.release());
    }

    return multiplaneGroup.release();
}

} // namespace multiplane

static void cleanupGroup(tcu::TestCaseGroup *)
{
}

} // namespace

tcu::TestCaseGroup *createCopiesAndBlittingTests(tcu::TestContext &testCtx)
{
    de::MovePtr<tcu::TestCaseGroup> copiesAndBlittingTests(new tcu::TestCaseGroup(testCtx, "copy_and_blit"));

    copiesAndBlittingTests->addChild(createTestGroup(testCtx, "core", addCoreCopiesAndBlittingTests, cleanupGroup));
    copiesAndBlittingTests->addChild(
        createTestGroup(testCtx, "dedicated_allocation", addDedicatedAllocationCopiesAndBlittingTests, cleanupGroup));
    copiesAndBlittingTests->addChild(createTestGroup(
        testCtx, "copy_commands2",
        [](tcu::TestCaseGroup *group) { addCopiesAndBlittingTests(group, ALLOCATION_KIND_DEDICATED, COPY_COMMANDS_2); },
        cleanupGroup));
    copiesAndBlittingTests->addChild(createTestGroup(
        testCtx, "sparse",
        [](tcu::TestCaseGroup *group)
        { addSparseCopyTests(group, ALLOCATION_KIND_DEDICATED, COPY_COMMANDS_2 | SPARSE_BINDING); },
        cleanupGroup));
    copiesAndBlittingTests->addChild(multiplane::createMultiplaneCopyTests(copiesAndBlittingTests->getTestContext()));

    return copiesAndBlittingTests.release();
}

} // namespace api
} // namespace vkt
