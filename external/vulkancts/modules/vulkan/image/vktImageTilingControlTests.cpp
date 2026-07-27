/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2026 The Khronos Group Inc.
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
 * \file  vktImageTilingControlTests.cpp
 * \brief Tests for VK_EXT_image_tiling_control
 *
 * The extension lets an application hint the driver towards a smaller memory
 * footprint (MIN_SIZE) or higher performance (MAX_PERFORMANCE) when creating
 * an image. These tests verify that:
 *   1. An image created with MIN_SIZE has a size that is less than or equal to
 *      the DEFAULT, MAX_PERFORMANCE and unspecified variants.
 *   2. The memory requirements and subresource layout queries return identical
 *      results whether an image object is actually created first or the
 *      creationless (VkImageCreateInfo based) query variants are used.
 *//*--------------------------------------------------------------------*/

#include "vktImageTilingControlTests.hpp"

#include "vktTestCase.hpp"

#include "vkDefs.hpp"
#include "vkPlatform.hpp"
#include "vkRefUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkStrUtil.hpp"

#include "tcuTestLog.hpp"
#include "tcuCommandLine.hpp"
#include "tcuTextureUtil.hpp"

#include "deStringUtil.hpp"

#include <sstream>
#include <string>
#include <vector>

namespace vkt
{
namespace image
{

#ifndef CTS_USES_VULKANSC

namespace
{
using namespace vk;

// The tiling control variant a test case exercises. UNSPECIFIED means no
// VkImageTilingControlCreateInfoEXT is chained into VkImageCreateInfo at all.
enum class TilingMode
{
    UNSPECIFIED = 0,
    DEFAULT,
    MIN_SIZE,
    MAX_PERFORMANCE,
};

// The tiling arrangement an image is created with.
enum class TilingScheme
{
    OPTIMAL = 0,
    DRM_MODIFIER,
};

VkImageTiling getVkTiling(TilingScheme scheme)
{
    return (scheme == TilingScheme::DRM_MODIFIER) ? VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT : VK_IMAGE_TILING_OPTIMAL;
}

const char *getTilingModeName(TilingMode mode)
{
    switch (mode)
    {
    case TilingMode::UNSPECIFIED:
        return "unspecified";
    case TilingMode::DEFAULT:
        return "default";
    case TilingMode::MIN_SIZE:
        return "min_space";
    case TilingMode::MAX_PERFORMANCE:
        return "max_performance";
    default:
        DE_ASSERT(false);
        return "";
    }
}

VkImageTilingControlEXT getTilingControlValue(TilingMode mode)
{
    switch (mode)
    {
    case TilingMode::DEFAULT:
        return VK_IMAGE_TILING_CONTROL_DEFAULT_EXT;
    case TilingMode::MIN_SIZE:
        return VK_IMAGE_TILING_CONTROL_MIN_SIZE_EXT;
    case TilingMode::MAX_PERFORMANCE:
        return VK_IMAGE_TILING_CONTROL_MAX_PERFORMANCE_EXT;
    default:
        DE_ASSERT(false);
        return VK_IMAGE_TILING_CONTROL_DEFAULT_EXT;
    }
}

// The single aspect a subresource layout query targets for the given format.
// For depth/stencil formats vkGetImageSubresourceLayout accepts exactly one of
// the depth or stencil aspects; YCbCr planar formats use the first plane.
VkImageAspectFlagBits getSubresourceAspect(VkFormat format, TilingScheme tilingScheme)
{
    // For DRM-modifier images the query addresses memory planes, not color or
    // depth/stencil aspects (VUID-vkGetImageSubresourceLayout-tiling-09433).
    if (tilingScheme == TilingScheme::DRM_MODIFIER)
    {
        return VK_IMAGE_ASPECT_MEMORY_PLANE_0_BIT_EXT;
    }

    if (isYCbCrFormat(format))
    {
        // Multi-planar formats are addressed per-plane; single-plane YCbCr
        // formats (e.g. packed 422) use the color aspect.
        return (getPlaneCount(format) > 1) ? VK_IMAGE_ASPECT_PLANE_0_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
    }

    const tcu::TextureFormat textureFormat = mapVkFormat(format);
    if (tcu::hasDepthComponent(textureFormat.order))
    {
        return VK_IMAGE_ASPECT_DEPTH_BIT;
    }
    if (tcu::hasStencilComponent(textureFormat.order))
    {
        return VK_IMAGE_ASPECT_STENCIL_BIT;
    }

    return VK_IMAGE_ASPECT_COLOR_BIT;
}

// The kind of query parity that a test case verifies.
enum class TestType
{
    MEMORY_REQUIREMENTS = 0, // vkGetImageMemoryRequirements2 vs vkGetDeviceImageMemoryRequirements
    SUBRESOURCE_LAYOUT,      // vkGetImageSubresourceLayout    vs vkGetDeviceImageSubresourceLayout
};

struct TestParams
{
    TestType testType;
    VkImageType imageType;
    VkFormat format;
    VkExtent3D extent; // 3D: real depth. 2D: depth must be 1, layers held separately.
    uint32_t arrayLayers;
    uint32_t mipLevels; // 0 means "full mip chain".
    TilingScheme tilingScheme;
    VkImageUsageFlags usage;
};

uint32_t getFullMipLevels(VkFormat format, const VkExtent3D &extent)
{
    uint32_t mipmapEdge = de::max(de::max(extent.width, extent.height), extent.depth);

    // For chroma-subsampled (YCbCr) formats the smallest plane limits the mip
    // chain: a fully populated chain cannot shrink a plane below one texel.
    if (isYCbCrFormat(format))
    {
        const tcu::UVec3 alignment = getImageSizeAlignment(format);
        if (alignment.x() > 1u)
        {
            mipmapEdge = de::min(mipmapEdge, extent.width / alignment.x());
        }
        if (alignment.y() > 1u)
        {
            mipmapEdge = de::min(mipmapEdge, extent.height / alignment.y());
        }
        if (alignment.z() > 1u)
        {
            mipmapEdge = de::min(mipmapEdge, extent.depth / alignment.z());
        }
    }

    uint32_t levels = 1u;
    for (uint32_t dim = mipmapEdge; dim > 1u; dim >>= 1)
    {
        ++levels;
    }
    return levels;
}

uint32_t getEffectiveMipLevels(const TestParams &params)
{
    return (params.mipLevels == 0u) ? getFullMipLevels(params.format, params.extent) : params.mipLevels;
}

// All DRM format modifiers the driver advertises for the given format.
std::vector<uint64_t> getDrmFormatModifiers(const InstanceInterface &vki, VkPhysicalDevice physicalDevice,
                                            VkFormat format)
{
    VkDrmFormatModifierPropertiesListEXT modifierList = initVulkanStructure();
    VkFormatProperties2 formatProperties              = initVulkanStructure(&modifierList);
    vki.getPhysicalDeviceFormatProperties2(physicalDevice, format, &formatProperties);

    std::vector<VkDrmFormatModifierPropertiesEXT> modifierProperties(modifierList.drmFormatModifierCount);
    modifierList.pDrmFormatModifierProperties = modifierProperties.data();
    vki.getPhysicalDeviceFormatProperties2(physicalDevice, format, &formatProperties);

    std::vector<uint64_t> modifiers;
    for (const auto &props : modifierProperties)
    {
        modifiers.push_back(props.drmFormatModifier);
    }
    return modifiers;
}

// The subset of the format's DRM modifiers that support the requested image
// parameters, as reported by vkGetPhysicalDeviceImageFormatProperties2.
std::vector<uint64_t> getCompatibleModifiers(const InstanceInterface &vki, VkPhysicalDevice physicalDevice,
                                             VkFormat format, VkImageType imageType, VkImageUsageFlags usage)
{
    std::vector<uint64_t> compatibleModifiers;

    for (const uint64_t modifier : getDrmFormatModifiers(vki, physicalDevice, format))
    {
        VkPhysicalDeviceImageDrmFormatModifierInfoEXT modifierInfo = initVulkanStructure();
        modifierInfo.drmFormatModifier                             = modifier;
        modifierInfo.sharingMode                                   = VK_SHARING_MODE_EXCLUSIVE;

        VkImageFormatListCreateInfo formatListInfo = initVulkanStructure(&modifierInfo);
        formatListInfo.viewFormatCount             = 1u;
        formatListInfo.pViewFormats                = &format;

        VkPhysicalDeviceImageFormatInfo2 imageFormatInfo = initVulkanStructure(&formatListInfo);
        imageFormatInfo.format                           = format;
        imageFormatInfo.type                             = imageType;
        imageFormatInfo.tiling                           = VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT;
        imageFormatInfo.usage                            = usage;

        VkImageFormatProperties2 imageFormatProperties = initVulkanStructure();
        const VkResult result =
            vki.getPhysicalDeviceImageFormatProperties2(physicalDevice, &imageFormatInfo, &imageFormatProperties);

        if (result == VK_ERROR_FORMAT_NOT_SUPPORTED)
        {
            continue;
        }
        VK_CHECK(result);

        compatibleModifiers.push_back(modifier);
    }

    return compatibleModifiers;
}

// Holds the structures a DRM-format-modifier image chains into its pNext, so
// their storage outlives the VkImageCreateInfo returned by makeImageCreateInfo.
struct DrmModifierChain
{
    VkImageDrmFormatModifierListCreateInfoEXT modifierListInfo;
    VkImageFormatListCreateInfo formatListInfo;
    VkFormat format;
};

// Build a VkImageCreateInfo for the given params.
VkImageCreateInfo makeImageCreateInfo(const TestParams &params, TilingMode mode,
                                      VkImageTilingControlCreateInfoEXT &tilingControlInfo, DrmModifierChain &drmChain,
                                      const std::vector<uint64_t> &modifiers)
{
    VkImageCreateInfo createInfo = initVulkanStructure();
    createInfo.flags             = 0u;
    createInfo.imageType         = params.imageType;
    createInfo.format            = params.format;
    createInfo.extent            = params.extent;
    createInfo.mipLevels         = getEffectiveMipLevels(params);
    createInfo.arrayLayers       = params.arrayLayers;
    createInfo.samples           = VK_SAMPLE_COUNT_1_BIT;
    createInfo.tiling            = getVkTiling(params.tilingScheme);
    createInfo.usage             = params.usage;
    createInfo.sharingMode       = VK_SHARING_MODE_EXCLUSIVE;
    createInfo.initialLayout     = VK_IMAGE_LAYOUT_UNDEFINED;

    void *pNextHead = nullptr;

    if (params.tilingScheme == TilingScheme::DRM_MODIFIER)
    {
        drmChain.format = params.format;

        drmChain.modifierListInfo                        = initVulkanStructure();
        drmChain.modifierListInfo.drmFormatModifierCount = static_cast<uint32_t>(modifiers.size());
        drmChain.modifierListInfo.pDrmFormatModifiers    = modifiers.data();

        drmChain.formatListInfo                 = initVulkanStructure(&drmChain.modifierListInfo);
        drmChain.formatListInfo.viewFormatCount = 1u;
        drmChain.formatListInfo.pViewFormats    = &drmChain.format;

        pNextHead = &drmChain.formatListInfo;
    }

    if (mode != TilingMode::UNSPECIFIED)
    {
        tilingControlInfo.tilingControl = getTilingControlValue(mode);
        tilingControlInfo.pNext         = pNextHead;
        pNextHead                       = &tilingControlInfo;
    }

    createInfo.pNext = pNextHead;

    return createInfo;
}

// vkCreateImage + vkGetImageMemoryRequirements2
VkMemoryRequirements getCreatedImageMemoryRequirements(const DeviceInterface &vk, VkDevice device,
                                                       const VkImageCreateInfo &createInfo)
{
    const Unique<VkImage> image(createImage(vk, device, &createInfo));

    VkImageMemoryRequirementsInfo2 info = initVulkanStructure();
    info.image                          = *image;

    VkMemoryRequirements2 req = initVulkanStructure();
    vk.getImageMemoryRequirements2(device, &info, &req);

    return req.memoryRequirements;
}

// vkGetDeviceImageMemoryRequirements (creationless)
VkMemoryRequirements getCreationlessImageMemoryRequirements(const DeviceInterface &vk, VkDevice device,
                                                            const VkImageCreateInfo &createInfo)
{
    VkDeviceImageMemoryRequirementsKHR info = initVulkanStructure();
    info.pCreateInfo                        = &createInfo;
    info.planeAspect                        = VkImageAspectFlagBits(0);

    VkMemoryRequirements2 req = initVulkanStructure();
    vk.getDeviceImageMemoryRequirements(device, &info, &req);

    return req.memoryRequirements;
}

// vkCreateImage + vkGetImageSubresourceLayout
VkSubresourceLayout getCreatedImageSubresourceLayout(const DeviceInterface &vk, VkDevice device,
                                                     const VkImageCreateInfo &createInfo,
                                                     const VkImageSubresource &subresource)
{
    const Unique<VkImage> image(createImage(vk, device, &createInfo));

    VkSubresourceLayout layout = {};
    vk.getImageSubresourceLayout(device, *image, &subresource, &layout);

    return layout;
}

// vkGetDeviceImageSubresourceLayout (creationless)
VkSubresourceLayout getCreationlessImageSubresourceLayout(const DeviceInterface &vk, VkDevice device,
                                                          const VkImageCreateInfo &createInfo,
                                                          const VkImageSubresource &subresource)
{
    VkImageSubresource2KHR subresource2 = initVulkanStructure();
    subresource2.imageSubresource       = subresource;

    VkDeviceImageSubresourceInfoKHR info = initVulkanStructure();
    info.pCreateInfo                     = &createInfo;
    info.pSubresource                    = &subresource2;

    VkSubresourceLayout2KHR layout2 = initVulkanStructure();
    vk.getDeviceImageSubresourceLayout(device, &info, &layout2);

    return layout2.subresourceLayout;
}

// Human-friendly description of a set of memory requirements.
std::string toString(const VkMemoryRequirements &req)
{
    std::ostringstream str;
    str << "size=" << req.size << ", alignment=" << req.alignment << ", memoryTypeBits=0x" << std::hex
        << req.memoryTypeBits;
    return str.str();
}

class TilingControlInstance : public vkt::TestInstance
{
public:
    TilingControlInstance(Context &context, const TestParams &params) : vkt::TestInstance(context), m_params(params)
    {
    }
    virtual ~TilingControlInstance(void) = default;

    virtual tcu::TestStatus iterate(void);

private:
    tcu::TestStatus iterateMemoryRequirements(const DeviceInterface &vk, VkDevice device);
    tcu::TestStatus iterateSubresourceLayout(const DeviceInterface &vk, VkDevice device);
    std::vector<uint64_t> getImageModifiers(void);

    const TestParams m_params;
};

// The DRM modifiers to offer at image creation time, or an empty list for
// optimal tiling. checkSupport has already guaranteed the list is non-empty
// for DRM-modifier cases.
std::vector<uint64_t> TilingControlInstance::getImageModifiers(void)
{
    if (m_params.tilingScheme != TilingScheme::DRM_MODIFIER)
    {
        return {};
    }

    return getCompatibleModifiers(m_context.getInstanceInterface(), m_context.getPhysicalDevice(), m_params.format,
                                  m_params.imageType, m_params.usage);
}

tcu::TestStatus TilingControlInstance::iterate(void)
{
    const DeviceInterface &vk = m_context.getDeviceInterface();
    const VkDevice device     = m_context.getDevice();

    if (m_params.testType == TestType::MEMORY_REQUIREMENTS)
    {
        return iterateMemoryRequirements(vk, device);
    }
    else
    {
        return iterateSubresourceLayout(vk, device);
    }
}

tcu::TestStatus TilingControlInstance::iterateMemoryRequirements(const DeviceInterface &vk, VkDevice device)
{
    tcu::TestLog &log = m_context.getTestContext().getLog();

    const std::vector<uint64_t> modifiers = getImageModifiers();

    const TilingMode modes[] = {TilingMode::UNSPECIFIED, TilingMode::DEFAULT, TilingMode::MIN_SIZE,
                                TilingMode::MAX_PERFORMANCE};

    VkMemoryRequirements perMode[DE_LENGTH_OF_ARRAY(modes)];

    for (int i = 0; i < DE_LENGTH_OF_ARRAY(modes); ++i)
    {
        const TilingMode mode = modes[i];

        VkImageTilingControlCreateInfoEXT tilingControlInfo = initVulkanStructure();
        DrmModifierChain drmChain                           = {};
        const VkImageCreateInfo createInfo =
            makeImageCreateInfo(m_params, mode, tilingControlInfo, drmChain, modifiers);

        const VkMemoryRequirements created      = getCreatedImageMemoryRequirements(vk, device, createInfo);
        const VkMemoryRequirements creationless = getCreationlessImageMemoryRequirements(vk, device, createInfo);

        log << tcu::TestLog::Message << getTilingModeName(mode) << ": created {" << toString(created)
            << "}, creationless {" << toString(creationless) << "}" << tcu::TestLog::EndMessage;

        if (created.size != creationless.size || created.memoryTypeBits != creationless.memoryTypeBits)
        {
            return tcu::TestStatus::fail(std::string("Created vs creationless memory requirements differ for ") +
                                         getTilingModeName(mode));
        }

        perMode[static_cast<int>(mode)] = created;
    }

    const VkMemoryRequirements &minSpaceReq = perMode[static_cast<int>(TilingMode::MIN_SIZE)];

    for (int i = 0; i < DE_LENGTH_OF_ARRAY(modes); ++i)
    {
        if (modes[i] == TilingMode::MIN_SIZE)
        {
            continue;
        }

        const VkMemoryRequirements &modeReq = perMode[static_cast<int>(modes[i])];
        if (minSpaceReq.size > modeReq.size)
        {
            return tcu::TestStatus::fail(std::string("MIN_SIZE size (") + de::toString(minSpaceReq.size) +
                                         ") is larger than " + getTilingModeName(modes[i]) + " size (" +
                                         de::toString(modeReq.size) + ")");
        }
    }

    // An absent VkImageTilingControlCreateInfoEXT must behave exactly like DEFAULT.
    const VkMemoryRequirements &unspecifiedReq = perMode[static_cast<int>(TilingMode::UNSPECIFIED)];
    const VkMemoryRequirements &defaultReq     = perMode[static_cast<int>(TilingMode::DEFAULT)];
    if (unspecifiedReq.size != defaultReq.size || unspecifiedReq.alignment != defaultReq.alignment)
    {
        return tcu::TestStatus::fail(std::string("Unspecified tiling control (size=") +
                                     de::toString(unspecifiedReq.size) +
                                     ", alignment=" + de::toString(unspecifiedReq.alignment) +
                                     ") differs from default (size=" + de::toString(defaultReq.size) +
                                     ", alignment=" + de::toString(defaultReq.alignment) + ")");
    }

    return tcu::TestStatus::pass("Pass");
}

tcu::TestStatus TilingControlInstance::iterateSubresourceLayout(const DeviceInterface &vk, VkDevice device)
{
    tcu::TestLog &log = m_context.getTestContext().getLog();

    const std::vector<uint64_t> modifiers = getImageModifiers();

    const TilingMode modes[] = {TilingMode::UNSPECIFIED, TilingMode::DEFAULT, TilingMode::MIN_SIZE,
                                TilingMode::MAX_PERFORMANCE};

    const VkImageSubresource subresource =
        makeImageSubresource(getSubresourceAspect(m_params.format, m_params.tilingScheme), 0u, 0u);

    VkDeviceSize perModeSize[DE_LENGTH_OF_ARRAY(modes)];

    for (int i = 0; i < DE_LENGTH_OF_ARRAY(modes); ++i)
    {
        const TilingMode mode = modes[i];

        VkImageTilingControlCreateInfoEXT tilingControlInfo = initVulkanStructure();
        DrmModifierChain drmChain                           = {};
        const VkImageCreateInfo createInfo =
            makeImageCreateInfo(m_params, mode, tilingControlInfo, drmChain, modifiers);

        const VkSubresourceLayout created = getCreatedImageSubresourceLayout(vk, device, createInfo, subresource);
        const VkSubresourceLayout creationless =
            getCreationlessImageSubresourceLayout(vk, device, createInfo, subresource);

        log << tcu::TestLog::Message << getTilingModeName(mode) << ": created {offset=" << created.offset
            << ", size=" << created.size << ", rowPitch=" << created.rowPitch << ", arrayPitch=" << created.arrayPitch
            << ", depthPitch=" << created.depthPitch << "}, creationless {offset=" << creationless.offset
            << ", size=" << creationless.size << ", rowPitch=" << creationless.rowPitch
            << ", arrayPitch=" << creationless.arrayPitch << ", depthPitch=" << creationless.depthPitch << "}"
            << tcu::TestLog::EndMessage;

        if (created.offset != creationless.offset || created.size != creationless.size ||
            created.rowPitch != creationless.rowPitch || created.arrayPitch != creationless.arrayPitch ||
            created.depthPitch != creationless.depthPitch)
        {
            return tcu::TestStatus::fail(std::string("Created vs creationless subresource layout differ for ") +
                                         getTilingModeName(mode));
        }

        perModeSize[static_cast<int>(mode)] = created.size;
    }

    const VkDeviceSize minSpaceSize = perModeSize[static_cast<int>(TilingMode::MIN_SIZE)];

    for (int i = 0; i < DE_LENGTH_OF_ARRAY(modes); ++i)
    {
        if (modes[i] == TilingMode::MIN_SIZE)
        {
            continue;
        }

        const VkDeviceSize modeSize = perModeSize[static_cast<int>(modes[i])];
        if (minSpaceSize > modeSize)
        {
            return tcu::TestStatus::fail(std::string("MIN_SIZE subresource size (") + de::toString(minSpaceSize) +
                                         ") is larger than " + getTilingModeName(modes[i]) + " size (" +
                                         de::toString(modeSize) + ")");
        }
    }

    // An absent VkImageTilingControlCreateInfoEXT must behave exactly like DEFAULT.
    const VkDeviceSize unspecifiedSize = perModeSize[static_cast<int>(TilingMode::UNSPECIFIED)];
    const VkDeviceSize defaultSize     = perModeSize[static_cast<int>(TilingMode::DEFAULT)];
    if (unspecifiedSize != defaultSize)
    {
        return tcu::TestStatus::fail(std::string("Unspecified tiling control subresource size (") +
                                     de::toString(unspecifiedSize) + ") differs from default (" +
                                     de::toString(defaultSize) + ")");
    }

    return tcu::TestStatus::pass("Pass");
}

class TilingControlCase : public vkt::TestCase
{
public:
    TilingControlCase(tcu::TestContext &testCtx, const std::string &name, const TestParams &params)
        : vkt::TestCase(testCtx, name)
        , m_params(params)
    {
    }
    virtual ~TilingControlCase(void) = default;

    virtual void checkSupport(Context &context) const;
    virtual TestInstance *createInstance(Context &context) const
    {
        return new TilingControlInstance(context, m_params);
    }

private:
    const TestParams m_params;
};

void TilingControlCase::checkSupport(Context &context) const
{
    context.requireDeviceFunctionality("VK_EXT_image_tiling_control");

    if (m_params.testType == TestType::MEMORY_REQUIREMENTS)
    {
        context.requireDeviceFunctionality("VK_KHR_maintenance4");
    }
    else
    {
        context.requireDeviceFunctionality("VK_KHR_maintenance5");
    }

    if (isYCbCrFormat(m_params.format))
    {
        context.requireDeviceFunctionality("VK_KHR_sampler_ycbcr_conversion");

        if (!context.getSamplerYcbcrConversionFeatures().samplerYcbcrConversion)
        {
            TCU_THROW(NotSupportedError, "samplerYcbcrConversion feature not supported");
        }
    }

    if (!context.getImageTilingControlFeaturesEXT().imageTilingControl)
    {
        TCU_THROW(NotSupportedError, "imageTilingControl feature not supported");
    }

    const InstanceInterface &vki          = context.getInstanceInterface();
    const VkPhysicalDevice physicalDevice = context.getPhysicalDevice();

    if (m_params.tilingScheme == TilingScheme::DRM_MODIFIER)
    {
        context.requireDeviceFunctionality("VK_EXT_image_drm_format_modifier");

        if (getCompatibleModifiers(vki, physicalDevice, m_params.format, m_params.imageType, m_params.usage).empty())
        {
            TCU_THROW(NotSupportedError, "No matching DRM modifier");
        }

        return;
    }

    // Verify the format/type/tiling/usage combination and its extents are supported.
    VkImageFormatProperties formatProperties;
    const VkResult result = vki.getPhysicalDeviceImageFormatProperties(
        physicalDevice, m_params.format, m_params.imageType, getVkTiling(m_params.tilingScheme), m_params.usage, 0u,
        &formatProperties);

    if (result == VK_ERROR_FORMAT_NOT_SUPPORTED)
    {
        TCU_THROW(NotSupportedError, "Image format/type/tiling/usage combination not supported");
    }
    VK_CHECK(result);

    if (m_params.extent.width > formatProperties.maxExtent.width ||
        m_params.extent.height > formatProperties.maxExtent.height ||
        m_params.extent.depth > formatProperties.maxExtent.depth)
    {
        TCU_THROW(NotSupportedError, "Image extent not supported");
    }

    if (getEffectiveMipLevels(m_params) > formatProperties.maxMipLevels)
    {
        TCU_THROW(NotSupportedError, "Required number of mip levels not supported");
    }

    if (m_params.arrayLayers > formatProperties.maxArrayLayers)
    {
        TCU_THROW(NotSupportedError, "Required number of array layers not supported");
    }
}

std::string getCaseName(const TestParams &params)
{
    std::ostringstream str;
    str << params.extent.width << "x" << params.extent.height;
    if (params.imageType == VK_IMAGE_TYPE_3D)
    {
        str << "x" << params.extent.depth;
    }
    if (params.arrayLayers > 1u)
    {
        str << "_" << params.arrayLayers << "layers";
    }
    str << (params.mipLevels == 0u ? "_full_mips" : "_1_mip");
    str << (params.tilingScheme == TilingScheme::DRM_MODIFIER ? "_drm_modifier" : "_optimal");
    return str.str();
}

// Optimal-tiling geometries used for the memory_requirements comparison, where
// the driver's swizzle choice actually influences the footprint.
void addOptimalMemoryRequirementCases(std::vector<TestParams> &cases, VkFormat format)
{
    const VkImageUsageFlags usage =
        (VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);

    const struct
    {
        VkImageType type;
        VkExtent3D extent;
        uint32_t layers;
    } geometries[] = {
        {VK_IMAGE_TYPE_2D, {256u, 256u, 1u}, 1u},
        {VK_IMAGE_TYPE_2D, {1024u, 1024u, 1u}, 1u},
        {VK_IMAGE_TYPE_2D, {256u, 256u, 1u}, 6u},
        {VK_IMAGE_TYPE_3D, {64u, 64u, 64u}, 1u},
    };

    for (const auto &geom : geometries)
    {
        // Depth/stencil and YCbCr formats are only valid as 2D images.
        if (geom.type != VK_IMAGE_TYPE_2D && (isDepthStencilFormat(format) || isYCbCrFormat(format)))
        {
            continue;
        }

        for (const uint32_t mips : {0u, 1u})
        {
            cases.push_back(TestParams{TestType::MEMORY_REQUIREMENTS, geom.type, format, geom.extent, geom.layers, mips,
                                       TilingScheme::OPTIMAL, usage});
        }
    }
}

// DRM-format-modifier geometries. Modifier images are restricted to a single
// mip level, single array layer and 2D, and the driver picks the tiling from
// the offered list (optionally influenced by the tiling control hint).
void addDrmModifierCases(std::vector<TestParams> &cases, TestType testType, VkFormat format)
{
    const VkImageUsageFlags usage = (VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                                     VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);

    const VkExtent3D extents[] = {
        {128u, 128u, 1u},
        {1024u, 1024u, 1u},
    };

    for (const VkExtent3D &extent : extents)
    {
        cases.push_back(
            TestParams{testType, VK_IMAGE_TYPE_2D, format, extent, 1u, 1u, TilingScheme::DRM_MODIFIER, usage});
    }
}

void addTilingControlCases(tcu::TestCaseGroup *group, TestType testType)
{
    tcu::TestContext &testCtx = group->getTestContext();

    const VkFormat formats[] = {
        VK_FORMAT_R8_UNORM,
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_FORMAT_R16G16B16A16_SFLOAT,
        VK_FORMAT_R32G32B32A32_SFLOAT,
        VK_FORMAT_D32_SFLOAT,
        VK_FORMAT_D24_UNORM_S8_UINT,
        VK_FORMAT_S8_UINT,
        VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM,
        VK_FORMAT_G8B8G8R8_422_UNORM,
    };

    for (const VkFormat format : formats)
    {
        if (isYCbCrFormat(format) && getPlaneCount(format) > 1)
        {
            continue;
        }

        de::MovePtr<tcu::TestCaseGroup> formatGroup(
            new tcu::TestCaseGroup(testCtx, getFormatSimpleName(format).c_str()));

        std::vector<TestParams> cases;

        // Optimal tiling is not queryable by vkGetImageSubresourceLayout, so
        // memory_requirements covers optimal + DRM modifier, while
        // subresource_layout covers DRM modifier only.
        if (testType == TestType::MEMORY_REQUIREMENTS)
        {
            addOptimalMemoryRequirementCases(cases, format);
        }
        addDrmModifierCases(cases, testType, format);

        for (const TestParams &params : cases)
        {
            formatGroup->addChild(new TilingControlCase(testCtx, getCaseName(params), params));
        }

        group->addChild(formatGroup.release());
    }
}

// -------------------------------------------------------------------------
// alignment_control tests - interaction of VK_EXT_image_tiling_control with
// VK_MESA_image_alignment_control.
// -------------------------------------------------------------------------

class AlignmentControlInstance : public vkt::TestInstance
{
public:
    AlignmentControlInstance(Context &context) : vkt::TestInstance(context)
    {
    }
    virtual ~AlignmentControlInstance(void) = default;

    virtual tcu::TestStatus iterate(void);
};

tcu::TestStatus AlignmentControlInstance::iterate(void)
{
    tcu::TestLog &log = m_context.getTestContext().getLog();

    const InstanceInterface &vki          = m_context.getInstanceInterface();
    const VkPhysicalDevice physicalDevice = m_context.getPhysicalDevice();

    VkPhysicalDeviceImageAlignmentControlPropertiesMESA alignmentProperties = initVulkanStructure();
    VkPhysicalDeviceProperties2 properties2                                 = initVulkanStructure(&alignmentProperties);
    vki.getPhysicalDeviceProperties2(physicalDevice, &properties2);

    const DeviceInterface &vk = m_context.getDeviceInterface();
    const VkDevice device     = m_context.getDevice();

    // A 1024x1024 R8G8B8A8_UNORM image with 1 mip and 1 layer.
    const TestParams imageParams = {
        TestType::MEMORY_REQUIREMENTS,
        VK_IMAGE_TYPE_2D,
        VK_FORMAT_R8G8B8A8_UNORM,
        {1024u, 1024u, 1u},
        1u,
        1u,
        TilingScheme::OPTIMAL,
        VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
    };

    const uint32_t mask = alignmentProperties.supportedImageAlignmentMask;
    log << tcu::TestLog::Message << "supportedImageAlignmentMask = 0x" << std::hex << mask << tcu::TestLog::EndMessage;

    // Query memory requirements for the fixed image with a MESA alignment hint of
    // maximumRequestedAlignment, optionally adding a tiling control hint. Also
    // verifies created vs creationless parity. Returns the resulting alignment.
    const auto queryAlignment = [&](TilingMode mode, uint32_t maximumRequestedAlignment) -> VkDeviceSize
    {
        VkImageAlignmentControlCreateInfoMESA alignmentInfo = initVulkanStructure();
        alignmentInfo.maximumRequestedAlignment             = maximumRequestedAlignment;

        VkImageTilingControlCreateInfoEXT tilingControlInfo = initVulkanStructure();
        DrmModifierChain drmChain;
        VkImageCreateInfo createInfo =
            makeImageCreateInfo(imageParams, mode, tilingControlInfo, drmChain, std::vector<uint64_t>{});
        alignmentInfo.pNext = createInfo.pNext;
        createInfo.pNext    = &alignmentInfo;

        const VkMemoryRequirements created      = getCreatedImageMemoryRequirements(vk, device, createInfo);
        const VkMemoryRequirements creationless = getCreationlessImageMemoryRequirements(vk, device, createInfo);

        log << tcu::TestLog::Message << "  " << getTilingModeName(mode) << ": created {" << toString(created)
            << "}, creationless {" << toString(creationless) << "}" << tcu::TestLog::EndMessage;

        if (created.size != creationless.size || created.alignment != creationless.alignment ||
            created.memoryTypeBits != creationless.memoryTypeBits)
        {
            TCU_FAIL(std::string("Created vs creationless memory requirements differ for ") + getTilingModeName(mode) +
                     " maximumRequestedAlignment=" + de::toString(maximumRequestedAlignment));
        }

        return created.alignment;
    };

    for (uint32_t bit = 1u; bit != 0u; bit <<= 1)
    {
        if ((mask & bit) == 0u)
        {
            continue;
        }

        const uint32_t maximumRequestedAlignment = bit;
        log << tcu::TestLog::Message << "Testing maximumRequestedAlignment = " << maximumRequestedAlignment
            << tcu::TestLog::EndMessage;

        // Baseline: MESA hint only, no tiling control. The spec permits the
        // implementation to return a larger alignment than requested when the
        // request can't be honored, so if this already exceeds the request the
        // hint isn't being honored and there is nothing to compare against.
        const VkDeviceSize baselineAlignment = queryAlignment(TilingMode::UNSPECIFIED, maximumRequestedAlignment);
        if (baselineAlignment > maximumRequestedAlignment)
        {
            log << tcu::TestLog::Message << "  baseline alignment not honored, skipping MAX_PERFORMANCE check"
                << tcu::TestLog::EndMessage;
            continue;
        }

        // The MESA hint was honored without a tiling control hint, so adding the
        // MAX_PERFORMANCE hint must not push the alignment above the request.
        const VkDeviceSize perfAlignment = queryAlignment(TilingMode::MAX_PERFORMANCE, maximumRequestedAlignment);
        if (perfAlignment > maximumRequestedAlignment)
        {
            return tcu::TestStatus::fail(std::string("MAX_PERFORMANCE alignment (") + de::toString(perfAlignment) +
                                         ") exceeds maximumRequestedAlignment (" +
                                         de::toString(maximumRequestedAlignment) +
                                         ") even though the baseline honored it");
        }
    }

    return tcu::TestStatus::pass("Pass");
}

class AlignmentControlCase : public vkt::TestCase
{
public:
    AlignmentControlCase(tcu::TestContext &testCtx, const std::string &name) : vkt::TestCase(testCtx, name)
    {
    }
    virtual ~AlignmentControlCase(void) = default;

    virtual void checkSupport(Context &context) const;
    virtual TestInstance *createInstance(Context &context) const
    {
        return new AlignmentControlInstance(context);
    }
};

void AlignmentControlCase::checkSupport(Context &context) const
{
    context.requireDeviceFunctionality("VK_EXT_image_tiling_control");
    context.requireDeviceFunctionality("VK_KHR_maintenance4");
    context.requireDeviceFunctionality("VK_MESA_image_alignment_control");

    if (!context.getImageTilingControlFeaturesEXT().imageTilingControl)
    {
        TCU_THROW(NotSupportedError, "imageTilingControl feature not supported");
    }
    if (!context.getImageAlignmentControlFeaturesMESA().imageAlignmentControl)
    {
        TCU_THROW(NotSupportedError, "imageAlignmentControl feature not supported");
    }
}

void addAlignmentControlCases(tcu::TestCaseGroup *group)
{
    tcu::TestContext &testCtx = group->getTestContext();
    group->addChild(new AlignmentControlCase(testCtx, "supported_alignments"));
}

} // namespace

tcu::TestCaseGroup *createImageTilingControlTests(tcu::TestContext &testCtx)
{
    // Root group for VK_EXT_image_tiling_control.
    de::MovePtr<tcu::TestCaseGroup> rootGroup(new tcu::TestCaseGroup(testCtx, "tiling_control"));

    de::MovePtr<tcu::TestCaseGroup> memReqGroup(new tcu::TestCaseGroup(testCtx, "memory_requirements"));
    addTilingControlCases(memReqGroup.get(), TestType::MEMORY_REQUIREMENTS);
    rootGroup->addChild(memReqGroup.release());

    de::MovePtr<tcu::TestCaseGroup> subresourceGroup(new tcu::TestCaseGroup(testCtx, "subresource_layout"));
    addTilingControlCases(subresourceGroup.get(), TestType::SUBRESOURCE_LAYOUT);
    rootGroup->addChild(subresourceGroup.release());

    de::MovePtr<tcu::TestCaseGroup> alignGroup(new tcu::TestCaseGroup(testCtx, "alignment_control"));
    addAlignmentControlCases(alignGroup.get());
    rootGroup->addChild(alignGroup.release());

    return rootGroup.release();
}

#else // CTS_USES_VULKANSC

tcu::TestCaseGroup *createImageTilingControlTests(tcu::TestContext &testCtx)
{
    // VK_EXT_image_tiling_control relies on creationless queries that are not
    // available in Vulkan SC, so expose an empty group.
    return new tcu::TestCaseGroup(testCtx, "tiling_control");
}

#endif // CTS_USES_VULKANSC

} // namespace image
} // namespace vkt
