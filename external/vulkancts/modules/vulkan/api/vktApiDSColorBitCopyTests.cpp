/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2024 The Khronos Group Inc.
 * Copyright (c) 2024 Valve Corporation.
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
 * \brief Vulkan Depth/Stencil to/from Color Bit Copy Tests
 *//*--------------------------------------------------------------------*/

#include "vktApiDSColorBitCopyTests.hpp"
#include "vktTestCase.hpp"
#include "vkDefs.hpp"
#include "vkImageUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkBarrierUtil.hpp"

#include "tcuFloat.hpp"

#include "deUniquePtr.hpp"
#include "deRandom.hpp"
#include "deMemory.h"
#include "deSTLUtil.hpp"

#include <vector>

namespace vkt
{

namespace api
{

namespace
{

using namespace vk;

struct FormatGroup
{
    std::vector<VkFormat> dsFormats;
    std::vector<VkFormat> colorFormats;
    VkImageAspectFlagBits aspect;
};

struct FormatPair
{
    VkFormat srcFormat;
    VkFormat dstFormat;
    VkImageAspectFlagBits aspect;
};

std::vector<FormatGroup> getFormatGroups()
{
    // The format list matches the spec.

    std::vector<FormatGroup> groups;
    groups.reserve(4u); // 32-bit depth, 24-bit depth, 16-bit depth and 8-bit stencil.

    // 32-bit depth.
    groups.emplace_back();
    {
        auto &group        = groups.back();
        group.aspect       = VK_IMAGE_ASPECT_DEPTH_BIT;
        group.dsFormats    = std::vector<VkFormat>{VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT};
        group.colorFormats = std::vector<VkFormat>{VK_FORMAT_R32_SFLOAT, VK_FORMAT_R32_SINT, VK_FORMAT_R32_UINT};
    }

    // 24-bit depth.
    groups.emplace_back();
    {
        auto &group        = groups.back();
        group.aspect       = VK_IMAGE_ASPECT_DEPTH_BIT;
        group.dsFormats    = std::vector<VkFormat>{VK_FORMAT_X8_D24_UNORM_PACK32, VK_FORMAT_D24_UNORM_S8_UINT};
        group.colorFormats = std::vector<VkFormat>{VK_FORMAT_R32_SFLOAT, VK_FORMAT_R32_SINT, VK_FORMAT_R32_UINT};
    }

    // 16-bit depth.
    groups.emplace_back();
    {
        auto &group        = groups.back();
        group.aspect       = VK_IMAGE_ASPECT_DEPTH_BIT;
        group.dsFormats    = std::vector<VkFormat>{VK_FORMAT_D16_UNORM, VK_FORMAT_D16_UNORM_S8_UINT};
        group.colorFormats = std::vector<VkFormat>{VK_FORMAT_R16_SFLOAT, VK_FORMAT_R16_UNORM, VK_FORMAT_R16_SNORM,
                                                   VK_FORMAT_R16_UINT, VK_FORMAT_R16_SINT};
    }

    // 8-bit stencil.
    groups.emplace_back();
    {
        auto &group     = groups.back();
        group.aspect    = VK_IMAGE_ASPECT_STENCIL_BIT;
        group.dsFormats = std::vector<VkFormat>{VK_FORMAT_S8_UINT, VK_FORMAT_D32_SFLOAT_S8_UINT,
                                                VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D16_UNORM_S8_UINT};
        group.colorFormats =
            std::vector<VkFormat>{VK_FORMAT_R8_UINT, VK_FORMAT_R8_SINT, VK_FORMAT_R8_UNORM, VK_FORMAT_R8_SNORM};
    }

    return groups;
}

constexpr float kMinDepthFloatVal             = 0.125f;
constexpr float kMaxDepthFloatVal             = 1.0f;
constexpr float kMaxDepthFloatValUnrestricted = 10.0f; // This would need VK_EXT_depth_range_unrestricted for D32.
constexpr uint32_t kDepth24Mask               = 0xFFFFFFu;

float getRandomDepth32(de::Random &rnd, bool unrestricted)
{
    // Make around 1 out of every 16 values be zero, pseudorandomly.
    const auto diceRoll = rnd.getInt(0, 15);
    if (diceRoll == 0)
        return 0.0f;
    const auto actualMax = (unrestricted ? kMaxDepthFloatValUnrestricted : kMaxDepthFloatVal);
    return rnd.getFloat(kMinDepthFloatVal, actualMax);
}

uint32_t getRandomDepth24(de::Random &rnd)
{
    return (rnd.getUint32() & kDepth24Mask);
}

union Depth16
{
    tcu::Float16 floatDepth;
    int16_t intDepth;

    Depth16() : intDepth(0)
    {
    }
};

Depth16 getRandomDepth16(de::Random &rnd, VkFormat src, VkFormat dst)
{
    Depth16 ret;

    if (src == VK_FORMAT_R16_SFLOAT || dst == VK_FORMAT_R16_SFLOAT)
        ret.floatDepth = tcu::Float16(getRandomDepth32(rnd, true)); // Unrestricted because it's not a depth format.
    else if (src == VK_FORMAT_R16_SNORM || dst == VK_FORMAT_R16_SNORM)
        ret.intDepth = static_cast<int16_t>(rnd.getInt(-32767, 32767));
    else
        ret.intDepth = static_cast<int16_t>(rnd.getUint16());

    return ret;
}

uint8_t getRandomStencil(de::Random &rnd, VkFormat src, VkFormat dst)
{
    if (src == VK_FORMAT_R8_SNORM || dst == VK_FORMAT_R8_SNORM)
    {
        uint8_t rawValue;
        auto value = static_cast<int8_t>(rnd.getInt(-127, 127));
        deMemcpy(&rawValue, &value, sizeof(rawValue));
        return rawValue;
    }
    else
        return rnd.getUint8();

    // Unreachable.
    DE_ASSERT(false);
    return 0;
}

bool isD32Format(VkFormat fmt)
{
    return (fmt == VK_FORMAT_D32_SFLOAT || fmt == VK_FORMAT_D32_SFLOAT_S8_UINT);
}

bool isD24Format(VkFormat fmt)
{
    return (fmt == VK_FORMAT_X8_D24_UNORM_PACK32 || fmt == VK_FORMAT_D24_UNORM_S8_UINT);
}

bool isD16Format(VkFormat fmt)
{
    return (fmt == VK_FORMAT_D16_UNORM || fmt == VK_FORMAT_D16_UNORM_S8_UINT);
}

uint32_t getBitCount(const FormatPair &formatPair)
{
    if (formatPair.aspect == VK_IMAGE_ASPECT_STENCIL_BIT)
        return 8u;

    DE_ASSERT(formatPair.aspect == VK_IMAGE_ASPECT_DEPTH_BIT);

    if (isD32Format(formatPair.srcFormat) || isD32Format(formatPair.dstFormat))
        return 32u;

    if (isD24Format(formatPair.srcFormat) || isD24Format(formatPair.dstFormat))
        return 24u;

    if (isD16Format(formatPair.srcFormat) || isD16Format(formatPair.dstFormat))
        return 16u;

    // Unreachable.
    DE_ASSERT(false);
    return 0u;
}

// Returns the contents of the source buffer to copy into the source image, as bytes.
std::vector<uint8_t> getRandomSrcValues(de::Random &rnd, const FormatPair &formatPair, int valueCount, bool unrestrict)
{
    std::vector<uint8_t> bytes;

    const auto formatBits = getBitCount(formatPair);

    switch (formatBits)
    {
    case 8u:
    {
        bytes.reserve(valueCount);
        for (int i = 0; i < valueCount; ++i)
            bytes.push_back(getRandomStencil(rnd, formatPair.srcFormat, formatPair.dstFormat));
        break;
    }
    case 16u:
    {
        std::vector<Depth16> values;
        values.reserve(valueCount);
        for (int i = 0; i < valueCount; ++i)
            values.push_back(getRandomDepth16(rnd, formatPair.srcFormat, formatPair.dstFormat));
        bytes.resize(de::dataSize(values), 0u);
        deMemcpy(de::dataOrNull(bytes), de::dataOrNull(values), de::dataSize(values));
        break;
    }
    case 24u:
    {
        std::vector<uint32_t> values;
        values.reserve(valueCount);
        for (int i = 0; i < valueCount; ++i)
            values.push_back(getRandomDepth24(rnd));
        bytes.resize(de::dataSize(values), 0u);
        deMemcpy(de::dataOrNull(bytes), de::dataOrNull(values), de::dataSize(values));
        break;
    }
    case 32u:
    {
        std::vector<float> values;
        values.reserve(valueCount);
        for (int i = 0; i < valueCount; ++i)
            values.push_back(getRandomDepth32(rnd, unrestrict));
        bytes.resize(de::dataSize(values), 0u);
        deMemcpy(de::dataOrNull(bytes), de::dataOrNull(values), de::dataSize(values));
        break;
    }
    default:
        DE_ASSERT(false);
        break;
    }

    return bytes;
}

enum class QueueType
{
    UNIVERSAL     = 0,
    COMPUTE_ONLY  = 1,
    TRANSFER_ONLY = 2,
};

struct TestParams
{
    FormatPair formatPair;
    uint32_t srcMipLevel;
    uint32_t dstMipLevel;
    uint32_t seed; // For the pseudorandom number generator.
    QueueType queueType;
    bool unrestricted;    // Unrestricted depth range.
    bool attachmentUsage; // Include attachment usage flags for the images instead of transfer usage only.
};

class DSColorCopyInstance : public vkt::TestInstance
{
public:
    DSColorCopyInstance(Context &context, const TestParams &params) : vkt::TestInstance(context), m_params(params)
    {
    }

    virtual ~DSColorCopyInstance() = default;

    tcu::TestStatus iterate(void) override;

protected:
    // These two depend on the parameters.
    uint32_t getQueueFamilyIndex() const;
    VkQueue getQueue() const;

    const TestParams m_params;
};

class DSColorCopyCase : public vkt::TestCase
{
public:
    DSColorCopyCase(tcu::TestContext &testCtx, const std::string &name, const TestParams &params)
        : vkt::TestCase(testCtx, name)
        , m_params(params)
    {
    }

    virtual ~DSColorCopyCase() = default;

    TestInstance *createInstance(Context &context) const override;
    void checkSupport(Context &context) const override;

protected:
    const TestParams m_params;
};

TestInstance *DSColorCopyCase::createInstance(Context &context) const
{
    return new DSColorCopyInstance(context, m_params);
}

VkImageUsageFlags getImageUsage(VkFormat format, bool attachmentUsage)
{
    const bool isDepthStencil    = isDepthStencilFormat(format);
    VkImageUsageFlags usageFlags = (VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);

    if (attachmentUsage)
        usageFlags |=
            (isDepthStencil ? VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT : VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);

    return usageFlags;
}

bool isFormatSupported(const InstanceInterface &vki, VkPhysicalDevice physicalDevice, VkFormat format,
                       uint32_t mipLevel, bool attachmentUsage)
{
    const auto imageType   = VK_IMAGE_TYPE_2D;
    const auto tiling      = VK_IMAGE_TILING_OPTIMAL;
    const auto usage       = getImageUsage(format, attachmentUsage);
    const auto createFlags = 0u;

    VkImageFormatProperties formatProperties;
    VkResult result = vki.getPhysicalDeviceImageFormatProperties(physicalDevice, format, imageType, tiling, usage,
                                                                 createFlags, &formatProperties);

    if (result == VK_ERROR_FORMAT_NOT_SUPPORTED)
        return false;

    VK_CHECK(result);

    if (formatProperties.maxMipLevels <= mipLevel)
        return false;

    return true;
}

void DSColorCopyCase::checkSupport(Context &context) const
{
    const auto ctx = context.getContextCommonData();

    const std::vector<std::pair<VkFormat, uint32_t>> formatParams{
        std::make_pair(m_params.formatPair.srcFormat, m_params.srcMipLevel),
        std::make_pair(m_params.formatPair.dstFormat, m_params.dstMipLevel),
    };

    for (const auto &param : formatParams)
    {
        if (!isFormatSupported(ctx.vki, ctx.physicalDevice, param.first, param.second, m_params.attachmentUsage))
        {
            std::ostringstream msg;
            msg << "Format " << getFormatName(param.first) << " does not support required features";
            TCU_THROW(NotSupportedError, msg.str());
        }
    }

#ifndef CTS_USES_VULKANSC
    if (m_params.queueType != QueueType::UNIVERSAL)
    {
        context.requireDeviceFunctionality("VK_KHR_maintenance10");
        context.requireDeviceFunctionality("VK_KHR_format_feature_flags2");

        vk::VkFormatProperties3 srcFormatProperties3{VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_3};
        vk::VkFormatProperties2 srcFormatProperties{VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2, &srcFormatProperties3};
        ctx.vki.getPhysicalDeviceFormatProperties2(ctx.physicalDevice, m_params.formatPair.srcFormat,
                                                   &srcFormatProperties);

        vk::VkFormatProperties3 dstFormatProperties3{VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_3};
        vk::VkFormatProperties2 dstFormatProperties{VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2, &dstFormatProperties3};
        ctx.vki.getPhysicalDeviceFormatProperties2(ctx.physicalDevice, m_params.formatPair.dstFormat,
                                                   &dstFormatProperties);

        // The get*Queue() methods will throw NotSupportedError if the queue is not available.
        if (m_params.queueType == QueueType::COMPUTE_ONLY)
        {
            context.getComputeQueue();

            if (isDepthStencilFormat(m_params.formatPair.srcFormat))
            {
                if ((m_params.formatPair.aspect & VK_IMAGE_ASPECT_DEPTH_BIT) != 0U &&
                    (srcFormatProperties3.optimalTilingFeatures &
                     VK_FORMAT_FEATURE_2_DEPTH_COPY_ON_COMPUTE_QUEUE_BIT_KHR) == 0ULL)
                {
                    std::ostringstream msg;
                    msg << "Source format " << getFormatName(m_params.formatPair.srcFormat)
                        << " does not support VK_FORMAT_FEATURE_2_DEPTH_COPY_ON_COMPUTE_QUEUE_BIT_KHR";
                    TCU_THROW(NotSupportedError, msg.str());
                }

                if ((m_params.formatPair.aspect & VK_IMAGE_ASPECT_STENCIL_BIT) != 0U &&
                    (srcFormatProperties3.optimalTilingFeatures &
                     VK_FORMAT_FEATURE_2_STENCIL_COPY_ON_COMPUTE_QUEUE_BIT_KHR) == 0ULL)
                {
                    std::ostringstream msg;
                    msg << "Source format " << getFormatName(m_params.formatPair.srcFormat)
                        << " does not support VK_FORMAT_FEATURE_2_STENCIL_COPY_ON_COMPUTE_QUEUE_BIT_KHR";
                    TCU_THROW(NotSupportedError, msg.str());
                }
            }

            if (isDepthStencilFormat(m_params.formatPair.dstFormat))
            {
                if ((m_params.formatPair.aspect & VK_IMAGE_ASPECT_DEPTH_BIT) != 0U &&
                    (dstFormatProperties3.optimalTilingFeatures &
                     VK_FORMAT_FEATURE_2_DEPTH_COPY_ON_COMPUTE_QUEUE_BIT_KHR) == 0ULL)
                {
                    std::ostringstream msg;
                    msg << "Destination format " << getFormatName(m_params.formatPair.dstFormat)
                        << " does not support VK_FORMAT_FEATURE_2_DEPTH_COPY_ON_COMPUTE_QUEUE_BIT_KHR";
                    TCU_THROW(NotSupportedError, msg.str());
                }

                if ((m_params.formatPair.aspect & VK_IMAGE_ASPECT_STENCIL_BIT) != 0U &&
                    (dstFormatProperties3.optimalTilingFeatures &
                     VK_FORMAT_FEATURE_2_STENCIL_COPY_ON_COMPUTE_QUEUE_BIT_KHR) == 0ULL)
                {
                    std::ostringstream msg;
                    msg << "Destination format " << getFormatName(m_params.formatPair.dstFormat)
                        << " does not support VK_FORMAT_FEATURE_2_STENCIL_COPY_ON_COMPUTE_QUEUE_BIT_KHR";
                    TCU_THROW(NotSupportedError, msg.str());
                }
            }
        }
        else if (m_params.queueType == QueueType::TRANSFER_ONLY)
        {
            context.getTransferQueue();

            if (isDepthStencilFormat(m_params.formatPair.srcFormat))
            {
                if ((m_params.formatPair.aspect & VK_IMAGE_ASPECT_DEPTH_BIT) != 0U &&
                    (srcFormatProperties3.optimalTilingFeatures &
                     VK_FORMAT_FEATURE_2_DEPTH_COPY_ON_TRANSFER_QUEUE_BIT_KHR) == 0ULL)
                {
                    std::ostringstream msg;
                    msg << "Source format " << getFormatName(m_params.formatPair.srcFormat)
                        << " does not support VK_FORMAT_FEATURE_2_DEPTH_COPY_ON_TRANSFER_QUEUE_BIT_KHR";
                    TCU_THROW(NotSupportedError, msg.str());
                }

                if ((m_params.formatPair.aspect & VK_IMAGE_ASPECT_STENCIL_BIT) != 0U &&
                    (srcFormatProperties3.optimalTilingFeatures &
                     VK_FORMAT_FEATURE_2_STENCIL_COPY_ON_TRANSFER_QUEUE_BIT_KHR) == 0ULL)
                {
                    std::ostringstream msg;
                    msg << "Source format " << getFormatName(m_params.formatPair.srcFormat)
                        << " does not support VK_FORMAT_FEATURE_2_STENCIL_COPY_ON_TRANSFER_QUEUE_BIT_KHR";
                    TCU_THROW(NotSupportedError, msg.str());
                }
            }

            if (isDepthStencilFormat(m_params.formatPair.dstFormat))
            {
                if ((m_params.formatPair.aspect & VK_IMAGE_ASPECT_DEPTH_BIT) != 0U &&
                    (dstFormatProperties3.optimalTilingFeatures &
                     VK_FORMAT_FEATURE_2_DEPTH_COPY_ON_TRANSFER_QUEUE_BIT_KHR) == 0ULL)
                {
                    std::ostringstream msg;
                    msg << "Destination format " << getFormatName(m_params.formatPair.dstFormat)
                        << " does not support VK_FORMAT_FEATURE_2_DEPTH_COPY_ON_TRANSFER_QUEUE_BIT_KHR";
                    TCU_THROW(NotSupportedError, msg.str());
                }

                if ((m_params.formatPair.aspect & VK_IMAGE_ASPECT_STENCIL_BIT) != 0U &&
                    (dstFormatProperties3.optimalTilingFeatures &
                     VK_FORMAT_FEATURE_2_STENCIL_COPY_ON_TRANSFER_QUEUE_BIT_KHR) == 0ULL)
                {
                    std::ostringstream msg;
                    msg << "Destination format " << getFormatName(m_params.formatPair.dstFormat)
                        << " does not support VK_FORMAT_FEATURE_2_STENCIL_COPY_ON_TRANSFER_QUEUE_BIT_KHR";
                    TCU_THROW(NotSupportedError, msg.str());
                }
            }
        }
    }
#endif // CTS_USES_VULKANSC

    context.requireDeviceFunctionality("VK_KHR_maintenance8");

    if (m_params.unrestricted)
        context.requireDeviceFunctionality("VK_EXT_depth_range_unrestricted");
}

struct PixelValue
{
    uint32_t bitCount;

    union
    {
        uint8_t bits8;
        uint16_t bits16;
        uint32_t bits32;
        float bits32f; // For debugging purposes.
    };

    PixelValue(const void *pixelPtr, uint32_t bitCount_) : bitCount(bitCount_)
    {
        if (bitCount == 8u)
            deMemcpy(&bits8, pixelPtr, 1u);
        else if (bitCount == 16u)
            deMemcpy(&bits16, pixelPtr, 2u);
        else if (bitCount == 24u)
        {
            deMemcpy(&bits32, pixelPtr, 4u);
            bits32 &= kDepth24Mask;
        }
        else if (bitCount == 32u)
            deMemcpy(&bits32, pixelPtr, 4u);
        else
            DE_ASSERT(false);
    }

    bool operator!=(const PixelValue &other) const
    {
        DE_ASSERT(bitCount == other.bitCount);

        if (bitCount == 8u)
            return (bits8 != other.bits8);
        if (bitCount == 16u)
            return (bits16 != other.bits16);
        if (bitCount == 24u || bitCount == 32u)
            return (bits32 != other.bits32);

        // Unreachable.
        DE_ASSERT(false);
        return true;
    }
};

std::ostream &operator<<(std::ostream &out, const PixelValue &pixel)
{
    std::ostringstream msg;
    msg << std::hex << std::setfill('0') << "0x";

    if (pixel.bitCount == 8u)
        msg << std::setw(2) << static_cast<int>(pixel.bits8);
    else if (pixel.bitCount == 16u)
        msg << std::setw(4) << pixel.bits16;
    else if (pixel.bitCount == 24u)
        msg << std::setw(6) << pixel.bits32;
    else if (pixel.bitCount == 32u)
        msg << std::setw(8) << pixel.bits32;
    else
        DE_ASSERT(false);

    return (out << msg.str());
}

uint32_t DSColorCopyInstance::getQueueFamilyIndex() const
{
    if (m_params.queueType == QueueType::UNIVERSAL)
        return m_context.getUniversalQueueFamilyIndex();

    if (m_params.queueType == QueueType::COMPUTE_ONLY)
        return m_context.getComputeQueueFamilyIndex();

    if (m_params.queueType == QueueType::TRANSFER_ONLY)
        return m_context.getTransferQueueFamilyIndex();

    // Unreachable.
    DE_ASSERT(false);
    return std::numeric_limits<uint32_t>::max();
}

VkQueue DSColorCopyInstance::getQueue() const
{
    if (m_params.queueType == QueueType::UNIVERSAL)
        return m_context.getUniversalQueue();

    if (m_params.queueType == QueueType::COMPUTE_ONLY)
        return m_context.getComputeQueue();

    if (m_params.queueType == QueueType::TRANSFER_ONLY)
        return m_context.getTransferQueue();

    // Unreachable.
    DE_ASSERT(false);
    return VK_NULL_HANDLE;
}

tcu::TestStatus DSColorCopyInstance::iterate(void)
{
    const auto ctx = m_context.getContextCommonData();
    const tcu::IVec3 baseExtent(16, 16, 1);
    const auto baseVkExtent = makeExtent3D(baseExtent);
    const tcu::IVec3 srcExtent((baseExtent.x() << m_params.srcMipLevel), (baseExtent.y() << m_params.srcMipLevel),
                               baseExtent.z());
    const tcu::IVec3 dstExtent((baseExtent.x() << m_params.dstMipLevel), (baseExtent.y() << m_params.dstMipLevel),
                               baseExtent.z());
    const auto srcVkExtent  = makeExtent3D(srcExtent);
    const auto dstVkExtent  = makeExtent3D(dstExtent);
    const auto pixelCount   = static_cast<uint32_t>(baseExtent.x() * baseExtent.y() * baseExtent.z());
    const auto srcMipLevels = m_params.srcMipLevel + 1u;
    const auto dstMipLevels = m_params.dstMipLevel + 1u;
    const auto srcImgUsage  = getImageUsage(m_params.formatPair.srcFormat, m_params.attachmentUsage);
    const auto dstImgUsage  = getImageUsage(m_params.formatPair.dstFormat, m_params.attachmentUsage);
    const bool isSrcDS      = isDepthStencilFormat(m_params.formatPair.srcFormat);
    const bool isDstDS      = isDepthStencilFormat(m_params.formatPair.dstFormat);
    const auto srcAspect    = (isSrcDS ? m_params.formatPair.aspect : VK_IMAGE_ASPECT_COLOR_BIT);
    const auto dstAspect    = (isDstDS ? m_params.formatPair.aspect : VK_IMAGE_ASPECT_COLOR_BIT);
    const auto srcSRR       = makeImageSubresourceRange(srcAspect, 0u, srcMipLevels, 0u, 1u);
    const auto dstSRR       = makeImageSubresourceRange(dstAspect, 0u, dstMipLevels, 0u, 1u);
    const auto srcSRL       = makeImageSubresourceLayers(srcAspect, m_params.srcMipLevel, 0u, 1u);
    const auto dstSRL       = makeImageSubresourceLayers(dstAspect, m_params.dstMipLevel, 0u, 1u);
    const auto zeroOffset   = makeOffset3D(0, 0, 0);
    const bool isXferQueue  = (m_params.queueType == QueueType::TRANSFER_ONLY);
    const bool useStaging   = (isSrcDS && isXferQueue);
    de::Random rnd(m_params.seed);

    const auto srcTcuFormat = mapVkFormat(m_params.formatPair.srcFormat);
    const auto dstTcuFormat = mapVkFormat(m_params.formatPair.dstFormat);

    tcu::TextureFormat srcCopyFormat;
    tcu::TextureFormat dstCopyFormat;

    if (m_params.formatPair.aspect == VK_IMAGE_ASPECT_DEPTH_BIT)
    {
        srcCopyFormat = (isSrcDS ? getDepthCopyFormat(m_params.formatPair.srcFormat) : srcTcuFormat);
        dstCopyFormat = (isDstDS ? getDepthCopyFormat(m_params.formatPair.dstFormat) : dstTcuFormat);
    }
    else if (m_params.formatPair.aspect == VK_IMAGE_ASPECT_STENCIL_BIT)
    {
        srcCopyFormat = (isSrcDS ? getStencilCopyFormat(m_params.formatPair.srcFormat) : srcTcuFormat);
        dstCopyFormat = (isDstDS ? getStencilCopyFormat(m_params.formatPair.dstFormat) : dstTcuFormat);
    }
    else
        DE_ASSERT(false);

    // Prepare source and destination buffers.
    const auto srcPixelSize = static_cast<uint32_t>(tcu::getPixelSize(srcCopyFormat));
    const auto dstPixelSize = static_cast<uint32_t>(tcu::getPixelSize(dstCopyFormat));

    const auto srcBufferSize = static_cast<VkDeviceSize>(srcPixelSize * pixelCount);
    const auto dstBufferSize = static_cast<VkDeviceSize>(dstPixelSize * pixelCount);

    const auto srcBufferCreateInfo = makeBufferCreateInfo(srcBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    BufferWithMemory srcBuffer(ctx.vkd, ctx.device, ctx.allocator, srcBufferCreateInfo, MemoryRequirement::HostVisible);

    const auto dstBufferCreateInfo = makeBufferCreateInfo(dstBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    BufferWithMemory dstBuffer(ctx.vkd, ctx.device, ctx.allocator, dstBufferCreateInfo, MemoryRequirement::HostVisible);

    // Copy source values to the source buffer.
    const auto srcValues = getRandomSrcValues(rnd, m_params.formatPair, pixelCount, m_params.unrestricted);
    deMemcpy(srcBuffer.getAllocation().getHostPtr(), de::dataOrNull(srcValues), de::dataSize(srcValues));

    flushAlloc(ctx.vkd, ctx.device, srcBuffer.getAllocation());

    // Create source and destination images.
    const VkImageCreateInfo srcImgCreateInfo = {
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        nullptr,
        0u,
        VK_IMAGE_TYPE_2D,
        m_params.formatPair.srcFormat,
        srcVkExtent,
        srcMipLevels,
        1u,
        VK_SAMPLE_COUNT_1_BIT,
        VK_IMAGE_TILING_OPTIMAL,
        srcImgUsage,
        VK_SHARING_MODE_EXCLUSIVE,
        0u,
        nullptr,
        VK_IMAGE_LAYOUT_UNDEFINED,
    };
    ImageWithMemory srcImage(ctx.vkd, ctx.device, ctx.allocator, srcImgCreateInfo, MemoryRequirement::Any);

    std::unique_ptr<ImageWithMemory> stagingImage;
    if (useStaging)
        stagingImage.reset(
            new ImageWithMemory(ctx.vkd, ctx.device, ctx.allocator, srcImgCreateInfo, MemoryRequirement::Any));

    const VkImageCreateInfo dstImgCreateInfo = {
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        nullptr,
        0u,
        VK_IMAGE_TYPE_2D,
        m_params.formatPair.dstFormat,
        dstVkExtent,
        dstMipLevels,
        1u,
        VK_SAMPLE_COUNT_1_BIT,
        VK_IMAGE_TILING_OPTIMAL,
        dstImgUsage,
        VK_SHARING_MODE_EXCLUSIVE,
        0u,
        nullptr,
        VK_IMAGE_LAYOUT_UNDEFINED,
    };
    ImageWithMemory dstImage(ctx.vkd, ctx.device, ctx.allocator, dstImgCreateInfo, MemoryRequirement::Any);

    const auto qfIndex = getQueueFamilyIndex();
    const auto queue   = getQueue();

    // When using the transfer queue and a source image that's depth/stencil, we cannot call vkCmdCopyBufferToImage with
    // it due to VUID-vkCmdCopyBufferToImage-commandBuffer-07739. We will apply a workaround that uploads the contents
    // of the image to a staging image using the universal queue, and in the transfer queue we copy from the staging
    // image to the final source image instead of the buffer.

    std::unique_ptr<CommandPoolWithBuffer> stagingCmd;
    if (useStaging)
    {
        // Note: universal queue.
        stagingCmd.reset(new CommandPoolWithBuffer(ctx.vkd, ctx.device, ctx.qfIndex));
        const auto stagingCmdBuffer = *stagingCmd->cmdBuffer;
        beginCommandBuffer(ctx.vkd, stagingCmdBuffer);

        // Staging image needs to be prepared with the source buffer contents.
        const auto preB2IBarrier = makeImageMemoryBarrier(0u, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                                                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, **stagingImage, srcSRR);
        cmdPipelineImageMemoryBarrier(ctx.vkd, stagingCmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                      VK_PIPELINE_STAGE_TRANSFER_BIT, &preB2IBarrier);
        const auto srcImgRegion = makeBufferImageCopy(baseVkExtent, srcSRL);
        ctx.vkd.cmdCopyBufferToImage(stagingCmdBuffer, *srcBuffer, **stagingImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                     1u, &srcImgRegion);

        // Transfer ownership of the staging image to the transfer queue.
        const auto releaseBarrier =
            makeImageMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, 0u, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                   VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, **stagingImage, srcSRR, ctx.qfIndex, qfIndex);
        cmdPipelineImageMemoryBarrier(ctx.vkd, stagingCmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, &releaseBarrier);

        endCommandBuffer(ctx.vkd, stagingCmdBuffer);
    }

    CommandPoolWithBuffer cmd(ctx.vkd, ctx.device, qfIndex);
    const auto cmdBuffer = *cmd.cmdBuffer;

    beginCommandBuffer(ctx.vkd, cmdBuffer);
    {
        // Source image needs to be prepared with the source buffer or staging image contents.
        const auto preB2IBarrier = makeImageMemoryBarrier(0u, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                                                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, *srcImage, srcSRR);
        cmdPipelineImageMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                      VK_PIPELINE_STAGE_TRANSFER_BIT, &preB2IBarrier);

        if (useStaging)
        {
            // Acquire ownership of the staging image.
            const auto acquireBarrier = makeImageMemoryBarrier(
                0u, VK_ACCESS_TRANSFER_READ_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, **stagingImage, srcSRR, ctx.qfIndex, qfIndex);
            cmdPipelineImageMemoryBarrier(ctx.vkd, cmdBuffer, 0, VK_PIPELINE_STAGE_TRANSFER_BIT, &acquireBarrier);

            // Copy staging image to source image.
            const VkImageCopy copyRegion{
                srcSRL, zeroOffset, srcSRL, zeroOffset, baseVkExtent,
            };
            ctx.vkd.cmdCopyImage(cmdBuffer, **stagingImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, *srcImage,
                                 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1u, &copyRegion);
        }
        else
        {
            // Copy buffer contents to source image.
            const auto srcImgRegion = makeBufferImageCopy(baseVkExtent, srcSRL);
            ctx.vkd.cmdCopyBufferToImage(cmdBuffer, *srcBuffer, *srcImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1u,
                                         &srcImgRegion);
        }
    }
    {
        // Now both images need to be prepared for the image copy operation.
        const std::vector<VkImageMemoryBarrier> preI2IBarriers{
            // Source image needs a layout switch.
            makeImageMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
                                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                   *srcImage, srcSRR),
            // Same for the destination image.
            makeImageMemoryBarrier(0u, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, *dstImage, dstSRR),
        };
        const auto srcStages = (VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT);
        const auto dstStages = VK_PIPELINE_STAGE_TRANSFER_BIT;
        cmdPipelineImageMemoryBarrier(ctx.vkd, cmdBuffer, srcStages, dstStages, de::dataOrNull(preI2IBarriers),
                                      preI2IBarriers.size());
    }
    {
        const VkImageCopy copyRegion{
            srcSRL, zeroOffset, dstSRL, zeroOffset, baseVkExtent,
        };
        ctx.vkd.cmdCopyImage(cmdBuffer, *srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, *dstImage,
                             VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1u, &copyRegion);
    }
    {
        // Copy destination image to destination buffer.
        const auto postI2IBarrier = makeImageMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
                                                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                                           VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, *dstImage, dstSRR);
        cmdPipelineImageMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                      VK_PIPELINE_STAGE_TRANSFER_BIT, &postI2IBarrier);
        const auto copyRegion = makeBufferImageCopy(baseVkExtent, dstSRL);
        ctx.vkd.cmdCopyImageToBuffer(cmdBuffer, *dstImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, *dstBuffer, 1u,
                                     &copyRegion);

        const auto preHostBarrier = makeMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
        cmdPipelineMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                                 &preHostBarrier);
    }
    endCommandBuffer(ctx.vkd, cmdBuffer);

    if (useStaging)
    {
        // We need to submit two command buffers to different queues in this case.
        const auto sem = createSemaphore(ctx.vkd, ctx.device);
        const VkSubmitInfo stagingInfo{
            VK_STRUCTURE_TYPE_SUBMIT_INFO, nullptr, 0u,         nullptr, nullptr, 1u,
            &stagingCmd->cmdBuffer.get(),  1u,      &sem.get(),
        };
        VK_CHECK(ctx.vkd.queueSubmit(ctx.queue, 1u, &stagingInfo, VK_NULL_HANDLE));

        const VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        const auto fence = submitCommands(ctx.vkd, ctx.device, queue, cmdBuffer, false, 1u, 1u, &sem.get(), &waitStage);
        waitForFence(ctx.vkd, ctx.device, *fence);
    }
    else
        submitCommandsAndWait(ctx.vkd, ctx.device, queue, cmdBuffer);

    invalidateAlloc(ctx.vkd, ctx.device, dstBuffer.getAllocation());

    tcu::ConstPixelBufferAccess srcAccess(srcCopyFormat, baseExtent, de::dataOrNull(srcValues));
    tcu::ConstPixelBufferAccess dstAccess(dstCopyFormat, baseExtent, dstBuffer.getAllocation().getHostPtr());

    const auto bitCount = getBitCount(m_params.formatPair);

    bool ok   = true;
    auto &log = m_context.getTestContext().getLog();

    for (int y = 0; y < baseExtent.y(); ++y)
        for (int x = 0; x < baseExtent.x(); ++x)
        {
            const PixelValue srcPixel(srcAccess.getPixelPtr(x, y), bitCount);
            const PixelValue dstPixel(dstAccess.getPixelPtr(x, y), bitCount);

            if (srcPixel != dstPixel)
            {
                ok = false;
                std::ostringstream msg;
                msg << "Unexpected value at (" << x << ", " << y << "): expected " << srcPixel << " but found "
                    << dstPixel;
                log << tcu::TestLog::Message << msg.str() << tcu::TestLog::EndMessage;
            }
        }

    if (!ok)
        return tcu::TestStatus::fail("Unexpected results found; check log for details");
    return tcu::TestStatus::pass("Pass");
}

using GroupPtr = de::MovePtr<tcu::TestCaseGroup>;

} // namespace

tcu::TestCaseGroup *createDSColorBitCopyTests(tcu::TestContext &testCtx)
{
    GroupPtr mainGroup(new tcu::TestCaseGroup(testCtx, "ds_color_copy"));

    const auto formatGroups = getFormatGroups();
    for (const auto &formatGroup : formatGroups)
        for (const auto &dsFormat : formatGroup.dsFormats)
            for (const auto &colorFormat : formatGroup.colorFormats)
                for (const bool dsToColor : {true, false})
                    for (const uint32_t srcMipLevel : {0u, 3u})
                        for (const uint32_t dstMipLevel : {0u, 3u})
                            for (const bool attUsage : {false, true})
                            {
                                if (attUsage && (srcMipLevel != 0u || dstMipLevel != 0u))
                                    continue;

                                const auto srcFormat = (dsToColor ? dsFormat : colorFormat);
                                const auto dstFormat = (dsToColor ? colorFormat : dsFormat);

                                const auto seed =
                                    ((static_cast<uint32_t>(srcFormat) << 24) |
                                     (static_cast<uint32_t>(dstFormat) << 16) |
                                     (static_cast<uint32_t>(formatGroup.aspect) << 8) |
                                     (static_cast<uint32_t>(srcMipLevel) << 2) | (static_cast<uint32_t>(dstMipLevel)));

                                for (const auto queueType :
                                     {QueueType::UNIVERSAL, QueueType::COMPUTE_ONLY, QueueType::TRANSFER_ONLY})
                                {
#ifdef CTS_USES_VULKANSC
                                    // These tests need to be skipped for now due to VUs *-10217 and *-10218.
                                    if (queueType != QueueType::UNIVERSAL)
                                        continue;
#endif // CTS_USES_VULKANSC

                                    const FormatPair formatPair{
                                        srcFormat,
                                        dstFormat,
                                        formatGroup.aspect,
                                    };

                                    // Values for the "unrestricted" test parameter.
                                    static const std::vector<bool> alwaysRestricted{false};
                                    static const std::vector<bool> sometimesRestricted{false, true};

                                    const auto bitCount = getBitCount(formatPair);
                                    const auto &unrestrictedValues =
                                        ((bitCount == 32u) ? sometimesRestricted : alwaysRestricted);

                                    for (const bool unrestricted : unrestrictedValues)
                                    {
                                        const TestParams params{
                                            formatPair, srcMipLevel,  dstMipLevel, seed,
                                            queueType,  unrestricted, attUsage,
                                        };

                                        static const std::map<QueueType, std::string> queueTypeSuffix{
                                            std::make_pair(QueueType::UNIVERSAL, ""),
                                            std::make_pair(QueueType::COMPUTE_ONLY, "_cq"),
                                            std::make_pair(QueueType::TRANSFER_ONLY, "_tq"),
                                        };

                                        const std::string unrestrictedSuffix = (unrestricted ? "_unrestricted" : "");
                                        const std::string usageSuffix        = (attUsage ? "_att_usage" : "");

                                        const auto testName =
                                            getFormatSimpleName(srcFormat) + "_" + getFormatSimpleName(dstFormat) +
                                            (formatGroup.aspect == VK_IMAGE_ASPECT_DEPTH_BIT ? "_depth" : "_stencil") +
                                            "_level" + std::to_string(srcMipLevel) + "_to_level" +
                                            std::to_string(dstMipLevel) + unrestrictedSuffix + usageSuffix +
                                            queueTypeSuffix.at(queueType);

                                        mainGroup->addChild(new DSColorCopyCase(testCtx, testName, params));
                                    }
                                }
                            }

    return mainGroup.release();
}

} // namespace api
} // namespace vkt
