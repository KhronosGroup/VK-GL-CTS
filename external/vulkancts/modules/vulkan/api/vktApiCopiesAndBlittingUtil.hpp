#ifndef _VKTAPICOPIESANDBLITTINGUTIL_HPP
#define _VKTAPICOPIESANDBLITTINGUTIL_HPP
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

#include "deDefs.h"
#include "deStringUtil.hpp"
#include "deUniquePtr.hpp"

#include "gluShaderProgram.hpp"
#include "gluShaderUtil.hpp"
#include "image/vktImageLoadStoreUtil.hpp"
#include "tcuImageCompare.hpp"
#include "tcuAstcUtil.hpp"
#include "tcuRGBA.hpp"
#include "tcuTestCase.hpp"
#include "tcuTexture.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuVectorType.hpp"
#include "tcuVectorUtil.hpp"
#include "tcuTestLog.hpp"
#include "tcuTexLookupVerifier.hpp"
#include "tcuCommandLine.hpp"
#include "tcuResultCollector.hpp"
#include "tcuSeedBuilder.hpp"

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
#include "vkPipelineConstructionUtil.hpp"

#include "vktCustomInstancesDevices.hpp"
#include "vkSafetyCriticalUtil.hpp"
#include "vkFormatLists.hpp"

#include "ycbcr/vktYCbCrUtil.hpp"
#include "pipeline/vktPipelineImageUtil.hpp" // required for compressed image blit
#include "image/vktImageTestsUtil.hpp"

#include <set>
#include <array>
#include <algorithm>
#include <iterator>
#include <limits>
#include <sstream>
#include <cstring>
#include <string>

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

enum FillMode
{
    FILL_MODE_GRADIENT = 0,
    FILL_MODE_PYRAMID,
    FILL_MODE_WHITE,
    FILL_MODE_BLACK,
    FILL_MODE_RED,
    FILL_MODE_RANDOM_GRAY,
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
    MAINTENANCE_8                 = (1 << 5),
    INDIRECT_COPY                 = (1 << 6),
    MAINTENANCE_10                = (1 << 7),
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

using namespace vk;
using namespace image;

const int32_t defaultSize                      = 64;
const int32_t defaultHalfSize                  = defaultSize / 2;
const int32_t defaultQuarterSize               = defaultSize / 4;
const int32_t defaultSixteenthSize             = defaultSize / 16;
const int32_t defaultQuarterSquaredSize        = defaultQuarterSize * defaultQuarterSize;
const int32_t defaultLargeSize                 = 4096;
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

VkImageCopy2KHR convertvkImageCopyTovkImageCopy2KHR(VkImageCopy imageCopy);

VkBufferCopy2KHR convertvkBufferCopyTovkBufferCopy2KHR(VkBufferCopy bufferCopy);

VkBufferImageCopy2KHR convertvkBufferImageCopyTovkBufferImageCopy2KHR(VkBufferImageCopy bufferImageCopy);

#ifndef CTS_USES_VULKANSC
VkCopyMemoryToImageIndirectCommandKHR convertvkBufferImageCopyTovkMemoryImageCopyKHR(VkDeviceAddress srcBufferAddress,
                                                                                     VkBufferImageCopy bufferImageCopy);
#endif

VkImageBlit2KHR convertvkImageBlitTovkImageBlit2KHR(VkImageBlit imageBlit);

VkImageResolve2KHR convertvkImageResolveTovkImageResolve2KHR(VkImageResolve imageResolve);

VkImageAspectFlags getAspectFlags(tcu::TextureFormat format);

VkImageAspectFlags getAspectFlags(VkFormat format);

tcu::TextureFormat getSizeCompatibleTcuTextureFormat(VkFormat format);

// This is effectively same as vk::isFloatFormat(mapTextureFormat(format))
// except that it supports some formats that are not mappable to VkFormat.
// When we are checking combined depth and stencil formats, each aspect is
// checked separately, and in some cases we construct PBA with a format that
// is not mappable to VkFormat.
bool isFloatFormat(tcu::TextureFormat format);

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
    struct Data
    {
        BufferParams buffer;
        ImageParms image;
    } src, dst;

    std::vector<CopyRegion> regions;

    struct
    {
        VkFilter filter;
        VkSampleCountFlagBits samples;
    };

    AllocationKind allocationKind;
    uint32_t extensionFlags;
    QueueSelectionOptions queueSelection;
    uint32_t mipLevels;
    uint32_t arrayLayers;
    uint32_t conditionalPredicate;
    bool singleCommand;
    uint32_t barrierCount;
    bool clearDestinationWithRed; // Used for CopyImageToImage tests to clear dst image with vec4(1.0, 0.0, 0.0, 1.0)
    bool imageOffset;
    bool useSecondaryCmdBuffer;
    bool useSparseBinding;
    bool useGeneralLayout;
    bool useConditionalRender;

    TestParams(void)
    {
        regions.resize(0);
        allocationKind          = ALLOCATION_KIND_DEDICATED;
        extensionFlags          = NONE;
        queueSelection          = QueueSelectionOptions::Universal;
        mipLevels               = 1u;
        arrayLayers             = 1u;
        conditionalPredicate    = 0u;
        singleCommand           = true;
        barrierCount            = 1u;
        src.image.createFlags   = VK_IMAGE_CREATE_FLAG_BITS_MAX_ENUM;
        dst.image.createFlags   = VK_IMAGE_CREATE_FLAG_BITS_MAX_ENUM;
        src.buffer.fillMode     = FILL_MODE_GRADIENT;
        src.image.fillMode      = FILL_MODE_GRADIENT;
        src.image.extent        = defaultExtent;
        src.buffer.size         = defaultSize;
        dst.buffer.fillMode     = FILL_MODE_GRADIENT;
        dst.image.fillMode      = FILL_MODE_WHITE;
        dst.image.extent        = defaultExtent;
        dst.buffer.size         = defaultSize;
        clearDestinationWithRed = false;
        samples                 = VK_SAMPLE_COUNT_1_BIT;
        imageOffset             = false;
        useSecondaryCmdBuffer   = false;
        useSparseBinding        = false;
        useGeneralLayout        = false;
        useConditionalRender    = false;
    }

    bool usesNonUniversalQueue() const
    {
        return queueSelection != QueueSelectionOptions::Universal;
    }
};

using TestParamsPtr = de::SharedPtr<TestParams>;

struct TestGroupParams
{
    AllocationKind allocationKind;
    uint32_t extensionFlags;
    QueueSelectionOptions queueSelection;
    bool useSecondaryCmdBuffer;
    bool useSparseBinding;
    bool useGeneralLayout;
};

using TestGroupParamsPtr = de::SharedPtr<TestGroupParams>;

de::MovePtr<Allocation> allocateBuffer(const InstanceInterface &vki, const DeviceInterface &vkd,
                                       const VkPhysicalDevice &physDevice, const VkDevice device,
                                       const VkBuffer &buffer, const MemoryRequirement requirement,
                                       Allocator &allocator, AllocationKind allocationKind);

de::MovePtr<Allocation> allocateImage(const InstanceInterface &vki, const DeviceInterface &vkd,
                                      const VkPhysicalDevice &physDevice, const VkDevice device, const VkImage &image,
                                      const MemoryRequirement requirement, Allocator &allocator,
                                      AllocationKind allocationKind, const uint32_t offset);

void checkExtensionSupport(Context &context, uint32_t flags);

uint32_t getArraySize(const ImageParms &parms);

VkImageCreateFlags getCreateFlags(const ImageParms &parms);

VkExtent3D getExtent3D(const ImageParms &parms, uint32_t mipLevel = 0u);

const tcu::TextureFormat mapCombinedToDepthTransferFormat(const tcu::TextureFormat &combinedFormat);

// Submits commands maybe waiting for a semaphore in a set of stages.
// If the semaphore to wait on is not VK_NULL_HANDLE, it will be destroyed after the wait to avoid accidental reuse.
// This is a wrapper to handle the need to use a sparse semaphore in some of these tests.
void submitCommandsAndWaitWithSync(const DeviceInterface &vkd, VkDevice device, VkQueue queue,
                                   VkCommandBuffer cmdBuffer, Move<VkSemaphore> *waitSemaphore = nullptr,
                                   VkPipelineStageFlags waitStages = 0u);

void submitCommandsAndWaitWithTransferSync(const DeviceInterface &vkd, VkDevice device, VkQueue queue,
                                           VkCommandBuffer cmdBuffer, Move<VkSemaphore> *waitSemaphore = nullptr,
                                           bool indirectCopy = false);

void checkTransferQueueGranularity(Context &context, const VkExtent3D &extent, VkImageType imageType);

std::string getSampleCountCaseName(VkSampleCountFlagBits sampleFlag);

std::string getFormatCaseName(VkFormat format);

std::string getImageLayoutCaseName(VkImageLayout layout);

bool isSupportedDepthStencilFormat(const InstanceInterface &vki, const VkPhysicalDevice physDevice,
                                   const VkFormat format);

tcu::Vec4 linearToSRGBIfNeeded(const tcu::TextureFormat &format, const tcu::Vec4 &color);

void scaleFromWholeSrcBuffer(const tcu::PixelBufferAccess &dst, const tcu::ConstPixelBufferAccess &src,
                             const VkOffset3D regionOffset, const VkOffset3D regionExtent,
                             tcu::Sampler::FilterMode filter, const MirrorMode mirrorMode = 0u);

void blit(const tcu::PixelBufferAccess &dst, const tcu::ConstPixelBufferAccess &src,
          const tcu::Sampler::FilterMode filter, const MirrorMode mirrorMode);

void flipCoordinates(CopyRegion &region, const MirrorMode mirrorMode);

// Mirror X, Y and Z as required by the offset values in the 3 axes.
MirrorMode getMirrorMode(const VkOffset3D from, const VkOffset3D to);

// Mirror the axes that are mirrored either in the source or destination, but not both.
MirrorMode getMirrorMode(const VkOffset3D s1, const VkOffset3D s2, const VkOffset3D d1, const VkOffset3D d2);

float calculateFloatConversionError(int srcBits);

tcu::Vec4 getFormatThreshold(const tcu::TextureFormat &format);

tcu::Vec4 getCompressedFormatThreshold(const tcu::CompressedTexFormat &format);

VkSamplerCreateInfo makeSamplerCreateInfo();

VkImageViewType mapImageViewType(const VkImageType imageType);

tcu::IVec3 getSizeInBlocks(const VkFormat imageFormat, const VkImageType imageType, const VkExtent3D imageExtent);

// Classes used for performing copies and blitting operations
class CopiesAndBlittingTestInstance : public vkt::TestInstance
{
public:
    CopiesAndBlittingTestInstance(Context &context, TestParams testParams);
    virtual tcu::TestStatus iterate(void) = 0;

protected:
    TestParams m_params;
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

    de::MovePtr<tcu::TextureLevel> m_sourceTextureLevel;
    de::MovePtr<tcu::TextureLevel> m_destinationTextureLevel;
    de::MovePtr<tcu::TextureLevel> m_expectedTextureLevel[16];

    // For tests that use multiple queues, this will be a >1 sized array containing the queue familiy indices,
    // used for setting up concurrently accessed resources.
    std::vector<uint32_t> m_queueFamilyIndices;

    void generateBuffer(tcu::PixelBufferAccess buffer, int width, int height, int depth = 1,
                        FillMode = FILL_MODE_GRADIENT);
    virtual void generateExpectedResult(void);
    virtual void generateExpectedResult(CopyRegion *region);
    void uploadBuffer(const tcu::ConstPixelBufferAccess &bufferAccess, const Allocation &bufferAlloc);
    void uploadImage(const tcu::ConstPixelBufferAccess &src, VkImage dst, const ImageParms &parms,
                     const uint32_t mipLevels, const bool useGeneralLayout, Move<VkSemaphore> *semaphore);
    virtual tcu::TestStatus checkTestResult(tcu::ConstPixelBufferAccess result);
    virtual void copyRegionToTextureLevel(tcu::ConstPixelBufferAccess src, tcu::PixelBufferAccess dst,
                                          CopyRegion region, uint32_t mipLevel = 0u) = 0;
    uint32_t calculateSize(tcu::ConstPixelBufferAccess src) const;

    de::MovePtr<tcu::TextureLevel> readImage(vk::VkImage image, const ImageParms &imageParms, const uint32_t mipLevel,
                                             const bool useGeneralLayout, Move<VkSemaphore> *semaphore);

    using ExecutionCtx = std::tuple<VkQueue, VkCommandBuffer, VkCommandPool>;
    ExecutionCtx activeExecutionCtx();

    uint32_t activeQueueFamilyIndex() const;

private:
    void uploadImageAspect(const tcu::ConstPixelBufferAccess &src, const VkImage &dst, const ImageParms &parms,
                           const uint32_t mipLevels, const bool useGeneralLayout, Move<VkSemaphore> *semaphore);
    void readImageAspect(vk::VkImage src, const tcu::PixelBufferAccess &dst, const ImageParms &parms,
                         const uint32_t mipLevel, const bool useGeneralLayout, Move<VkSemaphore> *semaphore);
};

class CopiesAndBlittingTestInstanceWithSparseSemaphore : public CopiesAndBlittingTestInstance
{
public:
    CopiesAndBlittingTestInstanceWithSparseSemaphore(Context &context, TestParams params);

    void uploadImage(const tcu::ConstPixelBufferAccess &src, VkImage dst, const ImageParms &parms,
                     const bool useGeneralLayout, const uint32_t mipLevels = 1u);

    de::MovePtr<tcu::TextureLevel> readImage(vk::VkImage image, const ImageParms &imageParms,
                                             const uint32_t mipLevel = 0u);

protected:
    Move<VkSemaphore> m_sparseSemaphore;
};

// Utility to encapsulate coordinate computation and loops.
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

tcu::Vec4 getFloatOrFixedPointFormatThreshold(const tcu::TextureFormat &format);

bool floatNearestBlitCompare(const tcu::ConstPixelBufferAccess &source, const tcu::ConstPixelBufferAccess &result,
                             const tcu::Vec4 &sourceThreshold, const tcu::Vec4 &resultThreshold,
                             const tcu::PixelBufferAccess &errorMask, const TestParams &params);

bool intNearestBlitCompare(const tcu::ConstPixelBufferAccess &source, const tcu::ConstPixelBufferAccess &result,
                           const tcu::PixelBufferAccess &errorMask, const TestParams &params);
} // namespace api
} // namespace vkt

#endif // _VKTAPICOPIESANDBLITTINGUTIL_HPP
