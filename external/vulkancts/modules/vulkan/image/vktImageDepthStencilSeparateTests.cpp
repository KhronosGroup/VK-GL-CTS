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
 * \brief Tests for VK_FORMAT_FEATURE_2_DEPTH_STENCIL_SEPARATE_FRAMEBUFFER_ACCESS_BIT_KHR
 *//*--------------------------------------------------------------------*/

#include "vktImageDepthStencilSeparateTests.hpp"
#include "vktImageTestsUtil.hpp"
#include "tcuImageCompare.hpp"
#include "tcuTextureUtil.hpp"
#include "vkBarrierUtil.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkBuilderUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vktTestCase.hpp"
#include "vkDefs.hpp"

#include "deUniquePtr.hpp"
#include "deRandom.hpp"

#include <sstream>
#include <vector>
#include <cstddef>
#include <memory>
#include <map>

namespace vkt
{
namespace image
{

using namespace vk;

namespace
{

constexpr VkFormat kColorFormat               = VK_FORMAT_R8G8B8A8_UNORM;
constexpr float kColorThreshold               = 0.005f; // 1/255 < 0.005 < 2/255
constexpr uint32_t kFramebufferDim            = 16u;
constexpr VkPipelineBindPoint kBindPoint      = VK_PIPELINE_BIND_POINT_GRAPHICS;
constexpr VkSampleCountFlagBits kSingleSample = VK_SAMPLE_COUNT_1_BIT;
constexpr VkSampleCountFlagBits kMultiSample  = VK_SAMPLE_COUNT_4_BIT;

enum class WriteMechanism
{
    RP_CLEAR = 0, // Write to the aspect as a render pass clear.
    RP_DONT_CARE, // Write to the aspect implicitly as a don't care (may produce writes).
    TEST_STORE,   // Write to the aspect running the corresponding depth or stencil test.
    TEST_RESOLVE, // Write to the aspect running the corresponding test and then resolving the attachment.
};

bool writesWithTest(WriteMechanism m)
{
    return (m == WriteMechanism::TEST_STORE || m == WriteMechanism::TEST_RESOLVE);
}

bool writesWithResolve(WriteMechanism m)
{
    return (m == WriteMechanism::TEST_RESOLVE);
}

struct TestParams
{
    VkFormat imageFormat;              // In the [VK_FORMAT_D16_UNORM, VK_FORMAT_D32_SFLOAT_S8_UINT] range.
    VkImageAspectFlagBits writeAspect; // Either depth or stencil, and the other one will be the sample aspect.
    WriteMechanism writeMechanism;     // Mechanism used to write to the selected aspect.
    bool generalLayout;                // True if we should always use the general layout for the image.
    bool separateLayouts;              // Use separate layouts for depth and stencil.
    bool
        dynamicStencilRef; // Set StencilRefEXT from the shader; used when writeAspect == VK_IMAGE_ASPECT_STENCIL_BIT and writeMechanism == TEST_*.

    VkImageAspectFlagBits getReadAspect(void) const
    {
        DE_ASSERT(writeAspect == VK_IMAGE_ASPECT_DEPTH_BIT || writeAspect == VK_IMAGE_ASPECT_STENCIL_BIT);
        return ((writeAspect == VK_IMAGE_ASPECT_DEPTH_BIT) ? VK_IMAGE_ASPECT_STENCIL_BIT : VK_IMAGE_ASPECT_DEPTH_BIT);
    }

    inline bool writesDepth(void) const
    {
        return (writeAspect == VK_IMAGE_ASPECT_DEPTH_BIT);
    }

    inline bool writesStencil(void) const
    {
        return (writeAspect == VK_IMAGE_ASPECT_STENCIL_BIT);
    }

    inline bool readsDepth(void) const
    {
        return (getReadAspect() == VK_IMAGE_ASPECT_DEPTH_BIT);
    }

    inline bool readsStencil(void) const
    {
        return (getReadAspect() == VK_IMAGE_ASPECT_STENCIL_BIT);
    }

    // Value based on the test parameter values, and can be used with de::Random.
    uint32_t getRandomSeed(void) const
    {
        // Some bit shuffling.
        return ((static_cast<uint32_t>(imageFormat) << 16) | (static_cast<uint32_t>(writeAspect) << 8) |
                (static_cast<uint32_t>(writeMechanism) << 4) | (static_cast<uint32_t>(generalLayout ? 1u : 0u) << 3) |
                (static_cast<uint32_t>(dynamicStencilRef ? 1u : 0u)));
    }

    // Returns the image layout that should be used during shader execution.
    VkImageLayout getImageLayout(void) const
    {
        if (generalLayout)
            return VK_IMAGE_LAYOUT_GENERAL;

        DE_ASSERT(!separateLayouts);

        return (readsDepth() ? VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL :
                               VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL);
    }

    // Returns the image layout that should be used for the depth aspect.
    VkImageLayout getDepthImageLayout(void) const
    {
        if (generalLayout)
            return VK_IMAGE_LAYOUT_GENERAL;

        DE_ASSERT(separateLayouts);
        return (readsDepth() ? VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
    }

    // Returns the image layout that should be used for the stencil aspect.
    VkImageLayout getStencilImageLayout(void) const
    {
        if (generalLayout)
            return VK_IMAGE_LAYOUT_GENERAL;

        DE_ASSERT(separateLayouts);
        return (readsStencil() ? VK_IMAGE_LAYOUT_STENCIL_READ_ONLY_OPTIMAL :
                                 VK_IMAGE_LAYOUT_STENCIL_ATTACHMENT_OPTIMAL);
    }

    // Returns true if the writes will happen through the corresponding fragment test.
    bool writesInFragTest(void) const
    {
        return writesWithTest(writeMechanism);
    }

    // Returns true if the test uses multisample pipelines.
    bool isMultisample(void) const
    {
        return (writeMechanism == WriteMechanism::TEST_RESOLVE);
    }

    // Returns the appropriate sample count for the pipeline.
    VkSampleCountFlagBits getSampleCount(void) const
    {
        return (isMultisample() ? kMultiSample : kSingleSample);
    }

    // Returns the format that will allow us to store sampling results for depth or stencil (see frag shader). Note both formats
    // have been selected because they can store any depth or stencil value and they have guaranteed storage image support.
    VkFormat getStorageImageFormat(void) const
    {
        return (readsDepth() ? VK_FORMAT_R32_SFLOAT : VK_FORMAT_R32_UINT);
    }
};

struct VertexData
{
    tcu::Vec4 coords;
    tcu::Vec4 color;
    tcu::IVec4 extra; // .x() will contain the stencil ref value for the vertex. Others currently unused.

    VertexData(const tcu::Vec4 &coords_, const tcu::Vec4 &color_, const tcu::IVec4 &extra_)
        : coords(coords_)
        , color(color_)
        , extra(extra_)
    {
    }

    static std::vector<VkVertexInputBindingDescription> getBindingDescriptions(void)
    {
        std::vector<VkVertexInputBindingDescription> descriptions;
        descriptions.reserve(1u);
        descriptions.emplace_back(makeVertexInputBindingDescription(0u, static_cast<uint32_t>(sizeof(VertexData)),
                                                                    VK_VERTEX_INPUT_RATE_VERTEX));
        return descriptions;
    }

    static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions(void)
    {
        std::vector<VkVertexInputAttributeDescription> descriptions;
        descriptions.reserve(3u);
        descriptions.emplace_back(makeVertexInputAttributeDescription(
            0u, 0u, VK_FORMAT_R32G32B32A32_SFLOAT, static_cast<uint32_t>(offsetof(VertexData, coords))));
        descriptions.emplace_back(makeVertexInputAttributeDescription(
            1u, 0u, VK_FORMAT_R32G32B32A32_SFLOAT, static_cast<uint32_t>(offsetof(VertexData, color))));
        descriptions.emplace_back(makeVertexInputAttributeDescription(
            2u, 0u, VK_FORMAT_R32G32B32A32_SINT, static_cast<uint32_t>(offsetof(VertexData, extra))));
        return descriptions;
    }
};

// Returns the framebuffer extent.
tcu::IVec3 getFramebufferExtent(void)
{
    return tcu::IVec3(static_cast<int>(kFramebufferDim), static_cast<int>(kFramebufferDim), 1);
}

// Usages for the depth/stencil image.
VkImageUsageFlags getDepthStencilUsage(bool multiSample)
{
    VkImageUsageFlags usageFlags = (VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);

    if (!multiSample)
        usageFlags |= (VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);

    return usageFlags;
}

// Usage for the storage image.
VkImageUsageFlags getStorageImageUsage(void)
{
    return (VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
}

// Pre-filling some common data.
VkImageCreateInfo makeImageCreateInfo(VkFormat format, VkExtent3D extent, VkSampleCountFlagBits sampleCount,
                                      VkImageUsageFlags usage)
{
    const VkImageCreateInfo createInfo = {
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, // VkStructureType sType;
        nullptr,                             // const void* pNext;
        0u,                                  // VkImageCreateFlags flags;
        VK_IMAGE_TYPE_2D,                    // VkImageType imageType;
        format,                              // VkFormat format;
        extent,                              // VkExtent3D extent;
        1u,                                  // uint32_t mipLevels;
        1u,                                  // uint32_t arrayLayers;
        sampleCount,                         // VkSampleCountFlagBits samples;
        VK_IMAGE_TILING_OPTIMAL,             // VkImageTiling tiling;
        usage,                               // VkImageUsageFlags usage;
        VK_SHARING_MODE_EXCLUSIVE,           // VkSharingMode sharingMode;
        0u,                                  // uint32_t queueFamilyIndexCount;
        nullptr,                             // const uint32_t* pQueueFamilyIndices;
        VK_IMAGE_LAYOUT_UNDEFINED,           // VkImageLayout initialLayout;
    };

    return createInfo;
}

class DepthStencilSeparateInstance : public vkt::TestInstance
{
public:
    DepthStencilSeparateInstance(Context &context, const TestParams &params)
        : vkt::TestInstance(context)
        , m_params(params)
    {
    }

    virtual ~DepthStencilSeparateInstance(void)
    {
    }

    tcu::TestStatus iterate(void);

protected:
    const TestParams m_params;
};

class DepthStencilSeparateCase : public vkt::TestCase
{
public:
    DepthStencilSeparateCase(tcu::TestContext &testCtx, const std::string &name, const TestParams &params)
        : vkt::TestCase(testCtx, name)
        , m_params(params)
    {
    }

    virtual ~DepthStencilSeparateCase(void)
    {
    }

    void initPrograms(SourceCollections &programCollection) const override;
    TestInstance *createInstance(Context &context) const override;
    void checkSupport(Context &context) const override;

protected:
    const TestParams m_params;
};

TestInstance *DepthStencilSeparateCase::createInstance(Context &context) const
{
    return new DepthStencilSeparateInstance(context, m_params);
}

void DepthStencilSeparateCase::checkSupport(Context &context) const
{
    context.requireInstanceFunctionality("VK_KHR_get_physical_device_properties2");
    context.requireDeviceFunctionality("VK_KHR_maintenance7");
    context.requireDeviceFunctionality("VK_KHR_format_feature_flags2");

#ifndef CTS_USES_VULKANSC
    const auto &m7properties = context.getMaintenance7Properties();
    if (!m7properties.separateDepthStencilAttachmentAccess)
        TCU_THROW(NotSupportedError, "separateDepthStencilAttachmentAccess not supported");
#endif // CTS_USES_VULKANSC

    VkFormatProperties3KHR fp3 = initVulkanStructure();
    VkFormatProperties2KHR fp2 = initVulkanStructure(&fp3);

    // We need to check support for single-sample and multi-sample usages, which differ slightly.
    std::map<VkSampleCountFlagBits, VkImageUsageFlags> usageCases;
    usageCases[VK_SAMPLE_COUNT_1_BIT] = getDepthStencilUsage(false);
    if (m_params.isMultisample())
        usageCases[m_params.getSampleCount()] = getDepthStencilUsage(true);

    const auto ctx = context.getContextCommonData();
    for (const auto &keyValue : usageCases)
    {
        const auto sampleCount  = keyValue.first;
        const auto dsUsage      = keyValue.second;
        const auto extent       = makeExtent3D(getFramebufferExtent());
        const auto dsCreateInfo = makeImageCreateInfo(m_params.imageFormat, extent, sampleCount, dsUsage);

        const VkPhysicalDeviceImageFormatInfo2 imgFormatInfo = {
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2, // VkStructureType sType;
            nullptr,                                               // const void* pNext;
            dsCreateInfo.format,                                   // VkFormat format;
            dsCreateInfo.imageType,                                // VkImageType type;
            dsCreateInfo.tiling,                                   // VkImageTiling tiling;
            dsCreateInfo.usage,                                    // VkImageUsageFlags usage;
            dsCreateInfo.flags,                                    // VkImageCreateFlags flags;
        };

        VkImageFormatProperties2 imgFormatProperties = initVulkanStructure();

        const auto result =
            ctx.vki.getPhysicalDeviceImageFormatProperties2(ctx.physicalDevice, &imgFormatInfo, &imgFormatProperties);
        if (result == VK_ERROR_FORMAT_NOT_SUPPORTED
#ifndef CTS_USES_VULKANSC
            || result == VK_ERROR_IMAGE_USAGE_NOT_SUPPORTED_KHR
#endif // CTS_USES_VULKANSC
        )
        {
            TCU_THROW(NotSupportedError,
                      "Format not supported or usage not supported for this format with sample count " +
                          std::to_string(sampleCount));
        }
        VK_CHECK(result);

        if ((imgFormatProperties.imageFormatProperties.sampleCounts & dsCreateInfo.samples) != dsCreateInfo.samples)
            TCU_THROW(NotSupportedError,
                      "Sample count " + std::to_string(sampleCount) + " not supported for this format");
    }

    ctx.vki.getPhysicalDeviceFormatProperties2(ctx.physicalDevice, m_params.imageFormat, &fp2);

    const auto requiredFeatures =
        (VK_FORMAT_FEATURE_2_DEPTH_STENCIL_ATTACHMENT_BIT | VK_FORMAT_FEATURE_2_SAMPLED_IMAGE_BIT |
         VK_FORMAT_FEATURE_2_TRANSFER_SRC_BIT | VK_FORMAT_FEATURE_2_TRANSFER_DST_BIT);

    if ((fp3.optimalTilingFeatures & requiredFeatures) != requiredFeatures)
        TCU_THROW(NotSupportedError, "Required features not supported for this format");

    if (m_params.dynamicStencilRef)
        context.requireDeviceFunctionality("VK_EXT_shader_stencil_export");

    if (m_params.isMultisample())
        context.requireDeviceFunctionality("VK_KHR_depth_stencil_resolve");

    if (m_params.separateLayouts)
        context.requireDeviceFunctionality("VK_KHR_separate_depth_stencil_layouts");
}

void DepthStencilSeparateCase::initPrograms(SourceCollections &programCollection) const
{
    // Vertex shader will pass everything to the fragment shader.
    std::ostringstream vert;
    vert << "#version 460\n"
         << "\n"
         << "layout (location=0) in vec4 inPos;\n"
         << "layout (location=1) in vec4 inColor;\n"
         << "layout (location=2) in ivec4 inExtra;\n"
         << "\n"
         << "layout (location=0) out vec4 outColor;\n"
         << "layout (location=1) out flat ivec4 outExtra;\n"
         << "\n"
         << "void main (void)\n"
         << "{\n"
         << "    gl_Position  = inPos;\n"
         << "    gl_PointSize = 1.0;\n"
         << "\n"
         << "    outColor = inColor;\n"
         << "    outExtra = inExtra;\n"
         << "}\n";
    programCollection.glslSources.add("vert") << glu::VertexSource(vert.str());

    // Fragment shader.
    std::string readDesc;
    std::string writeDesc;
    std::string pixelCopy;
    std::string samplerTypeSuffix;

    if (m_params.readsDepth())
    {
        readDesc  = "layout (set=0, binding=0) uniform sampler2D depthSampler;\n";
        writeDesc = "layout (r32f, set=0, binding=1) uniform image2D depthCopy;\n";
        pixelCopy = "imageStore(depthCopy, texCoords, texelFetch(depthSampler, texCoords, 0));\n";
    }
    else if (m_params.readsStencil())
    {
        readDesc  = "layout (set=0, binding=0) uniform usampler2D stencilSampler;\n";
        writeDesc = "layout (r32ui, set=0, binding=1) uniform uimage2D stencilCopy;\n";
        pixelCopy = "imageStore(stencilCopy, texCoords, texelFetch(stencilSampler, texCoords, 0));\n";
    }
    else
        DE_ASSERT(false);

    std::ostringstream frag;
    frag << "#version 460\n"
         << (m_params.dynamicStencilRef ? "#extension GL_ARB_shader_stencil_export : enable\n" : "") << "\n"
         << "layout (location=0) in vec4 inColor;\n"
         << "layout (location=1) in flat ivec4 inExtra;\n"
         << "\n"
         << "layout (location=0) out vec4 outColor;\n"
         << "\n"
         << readDesc << writeDesc << "\n"
         << "void main (void)\n"
         << "{\n"
         << "    outColor = inColor;\n"
         << "    " << (m_params.dynamicStencilRef ? "gl_FragStencilRefARB = inExtra.x;\n" : "")
         << "    const ivec2 texCoords = ivec2(gl_FragCoord.xy);\n"
         << "    " << pixelCopy << "}\n";
    programCollection.glslSources.add("frag") << glu::FragmentSource(frag.str());
}

// Used to handle render pass vs render pass 2.
class AttachmentDescription
{
public:
    AttachmentDescription(VkAttachmentDescriptionFlags flags, VkFormat format, VkSampleCountFlagBits samples,
                          VkAttachmentLoadOp loadOp, VkAttachmentStoreOp storeOp, VkAttachmentLoadOp stencilLoadOp,
                          VkAttachmentStoreOp stencilStoreOp, VkImageLayout initialLayout, VkImageLayout finalLayout,
                          VkImageLayout initialStencilLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                          VkImageLayout finalStencilLayout   = VK_IMAGE_LAYOUT_UNDEFINED)
    {
        m_data                = initVulkanStructure();
        m_data.flags          = flags;
        m_data.format         = format;
        m_data.samples        = samples;
        m_data.loadOp         = loadOp;
        m_data.storeOp        = storeOp;
        m_data.stencilLoadOp  = stencilLoadOp;
        m_data.stencilStoreOp = stencilStoreOp;
        m_data.initialLayout  = initialLayout;
        m_data.finalLayout    = finalLayout;

        m_stencilLayout                      = initVulkanStructure();
        m_stencilLayout.stencilInitialLayout = initialStencilLayout;
        m_stencilLayout.stencilFinalLayout   = finalStencilLayout;
    }

    operator VkAttachmentDescription2(void) const
    {
        return m_data;
    }

    operator VkAttachmentDescriptionStencilLayout(void) const
    {
        return m_stencilLayout;
    }

    operator VkAttachmentDescription(void) const
    {
        VkAttachmentDescription result;
        result.flags          = m_data.flags;
        result.format         = m_data.format;
        result.samples        = m_data.samples;
        result.loadOp         = m_data.loadOp;
        result.storeOp        = m_data.storeOp;
        result.stencilLoadOp  = m_data.stencilLoadOp;
        result.stencilStoreOp = m_data.stencilStoreOp;
        result.initialLayout  = m_data.initialLayout;
        result.finalLayout    = m_data.finalLayout;
        return result;
    }

protected:
    VkAttachmentDescription2 m_data;
    VkAttachmentDescriptionStencilLayout m_stencilLayout;
};

// Used to handle render pass vs render pass 2.
class AttachmentReference
{
public:
    AttachmentReference(uint32_t attachment, VkImageLayout layout,
                        VkImageLayout stencilLayout = VK_IMAGE_LAYOUT_UNDEFINED)
    {
        m_data            = initVulkanStructure();
        m_data.attachment = attachment;
        m_data.layout     = layout;
        m_data.aspectMask = 0u; // We will not use input attachments here.

        m_stencilLayout               = initVulkanStructure();
        m_stencilLayout.stencilLayout = stencilLayout;
    }

    operator VkAttachmentReferenceStencilLayout(void) const
    {
        return m_stencilLayout;
    }

    operator VkAttachmentReference2(void) const
    {
        return m_data;
    }

    operator VkAttachmentReference(void) const
    {
        VkAttachmentReference result;
        result.attachment = m_data.attachment;
        result.layout     = m_data.layout;
        return result;
    }

protected:
    VkAttachmentReference2 m_data;
    VkAttachmentReferenceStencilLayout m_stencilLayout;
};

Move<VkRenderPass> makeSeparateRenderPass(const DeviceInterface &vkd, VkDevice device, const TestParams &params)
{
    const auto mainSampleCount = params.getSampleCount();
    const bool isMultisample   = params.isMultisample();
    const bool separateLayouts = params.separateLayouts;

    std::vector<AttachmentDescription> attDescs;
    attDescs.reserve(4u);

    // Color attachment.
    attDescs.emplace_back(AttachmentDescription{
        0u,                          // VkAttachmentDescriptionFlags flags;
        kColorFormat,                // VkFormat format;
        mainSampleCount,             // VkSampleCountFlagBits samples;
        VK_ATTACHMENT_LOAD_OP_CLEAR, // VkAttachmentLoadOp loadOp;
        (isMultisample ? VK_ATTACHMENT_STORE_OP_DONT_CARE :
                         VK_ATTACHMENT_STORE_OP_STORE), // VkAttachmentStoreOp storeOp;
        VK_ATTACHMENT_LOAD_OP_DONT_CARE,                // VkAttachmentLoadOp stencilLoadOp;
        VK_ATTACHMENT_STORE_OP_DONT_CARE,               // VkAttachmentStoreOp stencilStoreOp;
        VK_IMAGE_LAYOUT_UNDEFINED,                      // VkImageLayout initialLayout;
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,       // VkImageLayout finalLayout;
    });

    const auto initialDSLayout      = (separateLayouts ? params.getDepthImageLayout() : params.getImageLayout());
    const auto finalDSLayout        = (separateLayouts ? params.getDepthImageLayout() : params.getImageLayout());
    const auto initialStencilLayout = (separateLayouts ? params.getStencilImageLayout() : VK_IMAGE_LAYOUT_UNDEFINED);
    const auto finalStencilLayout   = (separateLayouts ? params.getStencilImageLayout() : VK_IMAGE_LAYOUT_UNDEFINED);

    VkAttachmentLoadOp depthLoadOp   = VK_ATTACHMENT_LOAD_OP_MAX_ENUM;
    VkAttachmentStoreOp depthStoreOp = VK_ATTACHMENT_STORE_OP_MAX_ENUM;

    VkAttachmentLoadOp stencilLoadOp   = VK_ATTACHMENT_LOAD_OP_MAX_ENUM;
    VkAttachmentStoreOp stencilStoreOp = VK_ATTACHMENT_STORE_OP_MAX_ENUM;

    // One of the aspects will be the one read from the frag shader and have its test disabled, and the other one will have its
    // test enabled and will be written to with the selected write mechanism. Here we decide which is which.
    VkAttachmentLoadOp &readOnlyLoadOp   = (params.readsStencil() ? stencilLoadOp : depthLoadOp);
    VkAttachmentStoreOp &readOnlyStoreOp = (params.readsStencil() ? stencilStoreOp : depthStoreOp);

    VkAttachmentLoadOp &readWriteLoadOp   = (params.readsStencil() ? depthLoadOp : stencilLoadOp);
    VkAttachmentStoreOp &readWriteStoreOp = (params.readsStencil() ? depthStoreOp : stencilStoreOp);

    // The aspect which will be read-only will have its values pre-loaded and will preserve them through the store operation.
    // Note the store op will likely result in SYNC-HAZARD-WRITE-AFTER-READ, but the fragment shader reads from the same pixel
    // it's working with, so writes to that pixel are guaranteed to happen after the pixel has been processed and there is no
    // real data race.
    readOnlyLoadOp  = VK_ATTACHMENT_LOAD_OP_LOAD;
    readOnlyStoreOp = VK_ATTACHMENT_STORE_OP_STORE;

    // For the aspect being written to, the load and store operations used depend on the write mechanism.
    if (params.writeMechanism == WriteMechanism::RP_CLEAR)
    {
        readWriteLoadOp  = VK_ATTACHMENT_LOAD_OP_CLEAR;
        readWriteStoreOp = VK_ATTACHMENT_STORE_OP_STORE;
    }
    else if (params.writeMechanism == WriteMechanism::RP_DONT_CARE)
    {
        readWriteLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        readWriteStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    }
    else
    {
        readWriteLoadOp  = VK_ATTACHMENT_LOAD_OP_LOAD;
        readWriteStoreOp = VK_ATTACHMENT_STORE_OP_STORE;
    }

    // Depth/stencil attachment.
    attDescs.emplace_back(AttachmentDescription{
        0u,                                                                  // VkAttachmentDescriptionFlags flags;
        params.imageFormat,                                                  // VkFormat format;
        mainSampleCount,                                                     // VkSampleCountFlagBits samples;
        depthLoadOp,                                                         // VkAttachmentLoadOp loadOp;
        (isMultisample ? VK_ATTACHMENT_STORE_OP_DONT_CARE : depthStoreOp),   // VkAttachmentStoreOp storeOp;
        stencilLoadOp,                                                       // VkAttachmentLoadOp stencilLoadOp;
        (isMultisample ? VK_ATTACHMENT_STORE_OP_DONT_CARE : stencilStoreOp), // VkAttachmentStoreOp stencilStoreOp;
        initialDSLayout,                                                     // VkImageLayout initialLayout;
        finalDSLayout,                                                       // VkImageLayout finalLayout;
        initialStencilLayout,
        finalStencilLayout,
    });

    if (isMultisample)
    {
        // Color resolve attachment.
        attDescs.emplace_back(AttachmentDescription{
            0u,                                       // VkAttachmentDescriptionFlags flags;
            kColorFormat,                             // VkFormat format;
            kSingleSample,                            // VkSampleCountFlagBits samples;
            VK_ATTACHMENT_LOAD_OP_DONT_CARE,          // VkAttachmentLoadOp loadOp;
            VK_ATTACHMENT_STORE_OP_STORE,             // VkAttachmentStoreOp storeOp;
            VK_ATTACHMENT_LOAD_OP_DONT_CARE,          // VkAttachmentLoadOp stencilLoadOp;
            VK_ATTACHMENT_STORE_OP_DONT_CARE,         // VkAttachmentStoreOp stencilStoreOp;
            VK_IMAGE_LAYOUT_UNDEFINED,                // VkImageLayout initialLayout;
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, // VkImageLayout finalLayout;
        });

        // Depth/stencil resolve attachment. Note we may load data into this because we'll sample the resolve attachment during the
        // render pass, so it needs to have the right data from the start.
        attDescs.emplace_back(AttachmentDescription{
            0u,                 // VkAttachmentDescriptionFlags flags;
            params.imageFormat, // VkFormat format;
            kSingleSample,      // VkSampleCountFlagBits samples;
            depthLoadOp,        // VkAttachmentLoadOp loadOp;
            depthStoreOp,       // VkAttachmentStoreOp storeOp;
            stencilLoadOp,      // VkAttachmentLoadOp stencilLoadOp;
            stencilStoreOp,     // VkAttachmentStoreOp stencilStoreOp;
            initialDSLayout,    // VkImageLayout initialLayout;
            finalDSLayout,      // VkImageLayout finalLayout;
            initialStencilLayout,
            finalStencilLayout,
        });
    }

    std::vector<AttachmentReference> attRefs;
    attRefs.reserve(4u);

    attRefs.emplace_back(AttachmentReference(0u, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL));
    attRefs.emplace_back(AttachmentReference(1u, finalDSLayout, finalStencilLayout));

    if (isMultisample)
    {
        // Resolve attachment references.
        attRefs.emplace_back(AttachmentReference(2u, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL));
        attRefs.emplace_back(AttachmentReference(3u, finalDSLayout, finalStencilLayout));
    }

    if (isMultisample || separateLayouts)
    {
        std::vector<VkAttachmentDescription2> descriptions;
        descriptions.reserve(attDescs.size());
        for (const auto &desc : attDescs)
            descriptions.emplace_back(static_cast<VkAttachmentDescription2>(desc));
        const auto stencilDescription = static_cast<VkAttachmentDescriptionStencilLayout>(attDescs.at(1u));

        std::vector<VkAttachmentReference2> references;
        references.reserve(attRefs.size());
        for (const auto &ref : attRefs)
            references.emplace_back(static_cast<VkAttachmentReference2>(ref));
        const auto stencilReference = static_cast<VkAttachmentReferenceStencilLayout>(attRefs.at(1u));

        if (separateLayouts)
        {
            DE_ASSERT(!isMultisample);
            descriptions.at(1u).pNext = &stencilDescription;
            references.at(1u).pNext   = &stencilReference;
        }

#ifndef CTS_USES_VULKANSC
        const auto depthResolveMode = (params.readsDepth() ? VK_RESOLVE_MODE_NONE : VK_RESOLVE_MODE_SAMPLE_ZERO_BIT);
        const auto stencilResolveMode =
            (params.readsStencil() ? VK_RESOLVE_MODE_NONE : VK_RESOLVE_MODE_SAMPLE_ZERO_BIT);

        const VkSubpassDescriptionDepthStencilResolve dsResolve = {
            VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_DEPTH_STENCIL_RESOLVE, // VkStructureType sType;
            nullptr,                                                     // const void* pNext;
            depthResolveMode,                                            // VkResolveModeFlagBits depthResolveMode;
            stencilResolveMode,                                          // VkResolveModeFlagBits stencilResolveMode;
            (isMultisample ? &references.at(3u) :
                             nullptr), // const VkAttachmentReference2* pDepthStencilResolveAttachment;
        };
#endif // CTS_USES_VULKANSC

        const VkSubpassDescription2 subpass = {
            VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_2, // VkStructureType sType;
#ifndef CTS_USES_VULKANSC
            (isMultisample ? &dsResolve : nullptr), // const void* pNext;
#else
            nullptr,
#endif                                                      // CTS_USES_VULKANSC
            0u,                                             // VkSubpassDescriptionFlags flags;
            kBindPoint,                                     // VkPipelineBindPoint pipelineBindPoint;
            0u,                                             // uint32_t viewMask;
            0u,                                             // uint32_t inputAttachmentCount;
            nullptr,                                        // const VkAttachmentReference2* pInputAttachments;
            1u,                                             // uint32_t colorAttachmentCount;
            &references.at(0u),                             // const VkAttachmentReference2* pColorAttachments;
            (isMultisample ? &references.at(2u) : nullptr), // const VkAttachmentReference2* pResolveAttachments;
            &references.at(1u),                             // const VkAttachmentReference2* pDepthStencilAttachment;
            0u,                                             // uint32_t preserveAttachmentCount;
            nullptr,                                        // const uint32_t* pPreserveAttachments;
        };

        const VkRenderPassCreateInfo2 createInfo = {
            VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO_2, // VkStructureType sType;
            nullptr,                                     // const void* pNext;
            0u,                                          // VkRenderPassCreateFlags flags;
            de::sizeU32(descriptions),                   // uint32_t attachmentCount;
            de::dataOrNull(descriptions),                // const VkAttachmentDescription2* pAttachments;
            1u,                                          // uint32_t subpassCount;
            &subpass,                                    // const VkSubpassDescription2* pSubpasses;
            0u,                                          // uint32_t dependencyCount;
            nullptr,                                     // const VkSubpassDependency2* pDependencies;
            0u,                                          // uint32_t correlatedViewMaskCount;
            nullptr,                                     // const uint32_t* pCorrelatedViewMasks;
        };

        return createRenderPass2(vkd, device, &createInfo);
    }
    else
    {
        DE_ASSERT(attDescs.size() == 2u);
        DE_ASSERT(attRefs.size() == 2u);

        std::vector<VkAttachmentDescription> descriptions;
        descriptions.reserve(attDescs.size());
        for (const auto &desc : attDescs)
            descriptions.emplace_back(static_cast<VkAttachmentDescription>(desc));

        std::vector<VkAttachmentReference> references;
        references.reserve(attRefs.size());
        for (const auto &ref : attRefs)
            references.emplace_back(static_cast<VkAttachmentReference>(ref));

        const VkSubpassDescription subpass = {
            0u,                 // VkSubpassDescriptionFlags flags;
            kBindPoint,         // VkPipelineBindPoint pipelineBindPoint;
            0u,                 // uint32_t inputAttachmentCount;
            nullptr,            // const VkAttachmentReference* pInputAttachments;
            1u,                 // uint32_t colorAttachmentCount;
            &references.at(0u), // const VkAttachmentReference* pColorAttachments;
            nullptr,            // const VkAttachmentReference* pResolveAttachments;
            &references.at(1u), // const VkAttachmentReference* pDepthStencilAttachment;
            0u,                 // uint32_t preserveAttachmentCount;
            nullptr,            // const uint32_t* pPreserveAttachments;
        };

        const VkRenderPassCreateInfo createInfo = {
            VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO, // VkStructureType sType;
            nullptr,                                   // const void* pNext;
            0u,                                        // VkRenderPassCreateFlags flags;
            de::sizeU32(descriptions),                 // uint32_t attachmentCount;
            de::dataOrNull(descriptions),              // const VkAttachmentDescription* pAttachments;
            1u,                                        // uint32_t subpassCount;
            &subpass,                                  // const VkSubpassDescription* pSubpasses;
            0u,                                        // uint32_t dependencyCount;
            nullptr,                                   // const VkSubpassDependency* pDependencies;
        };

        return createRenderPass(vkd, device, &createInfo);
    }

    DE_ASSERT(false); // Unreachable.
    return Move<VkRenderPass>();
}

float getDepthThreshold(const tcu::TextureFormat &format)
{
    DE_ASSERT(format.order == tcu::TextureFormat::D);

    if (format.type == tcu::TextureFormat::UNORM_INT16)
        return 1.5f / 65535.0f; // D16
    else if (format.type == tcu::TextureFormat::UNSIGNED_INT_24_8_REV)
        return static_cast<float>(1.5 / 16777215.0); // D24
    else if (format.type == tcu::TextureFormat::FLOAT)
        return static_cast<float>(
            1.0 / 33554431.0); // D32: This could be exact, but lets simply make it a bit stricter than 24 bits.
    else
        DE_ASSERT(false);

    return 0.0f;
}

VkPipelineMultisampleStateCreateInfo makePipelineMultisampleStateCreateInfo(VkSampleCountFlagBits sampleCount)
{
    const VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, // VkStructureType sType;
        nullptr,                                                  // const void* pNext;
        0u,                                                       // VkPipelineMultisampleStateCreateFlags flags;
        sampleCount,                                              // VkSampleCountFlagBits rasterizationSamples;
        VK_FALSE,                                                 // VkBool32 sampleShadingEnable;
        1.0f,                                                     // float minSampleShading;
        nullptr,                                                  // const VkSampleMask* pSampleMask;
        VK_FALSE,                                                 // VkBool32 alphaToCoverageEnable;
        VK_FALSE,                                                 // VkBool32 alphaToOneEnable;
    };

    return multisampleStateCreateInfo;
}

tcu::TestStatus DepthStencilSeparateInstance::iterate(void)
{
    const auto ctx = m_context.getContextCommonData();
    de::Random rnd(m_params.getRandomSeed());
    const auto readLayout       = (m_params.separateLayouts ? (m_params.readsDepth() ? m_params.getDepthImageLayout() :
                                                                                       m_params.getStencilImageLayout()) :
                                                              m_params.getImageLayout());
    const auto mainSampleCount  = m_params.getSampleCount();
    const auto storageImgLayout = VK_IMAGE_LAYOUT_GENERAL;
    const auto topology         = VK_PRIMITIVE_TOPOLOGY_POINT_LIST; // One point per pixel, drawn in the middle.
    const auto viewType         = VK_IMAGE_VIEW_TYPE_2D;
    const auto shaderAccesses   = (VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
    const auto tcuColorFormat   = mapVkFormat(kColorFormat);

    // Vertex buffer.
    const tcu::IVec3 fbExtent = getFramebufferExtent();
    const auto vkExtent       = makeExtent3D(fbExtent);
    const uint32_t pixelCount = kFramebufferDim * kFramebufferDim;

    // One point per pixel.
    std::vector<VertexData> vertices;
    vertices.reserve(pixelCount);

    const auto getFramebufferCoord = [](uint32_t c, uint32_t total)
    { return (static_cast<float>(c) + 0.5f) / static_cast<float>(total) * 2.0f - 1.0f; };

    for (uint32_t y = 0u; y < kFramebufferDim; ++y)
        for (uint32_t x = 0u; x < kFramebufferDim; ++x)
        {
            // The vertices array will always contain pseudorandom data for depth, stencil and color.
            const float xCoord = getFramebufferCoord(x, fbExtent.x());
            const float yCoord = getFramebufferCoord(y, fbExtent.y());
            const float depth  = rnd.getFloat() * 0.5f + 0.5f; // Restrict depth values to [0.5, 1) to avoid denormals.
            const int stencil  = rnd.getInt(1, 255);           // Avoid value zero as that may be used for clears.

            const float r = rnd.getFloat();
            const float g = rnd.getFloat();
            const float b = rnd.getFloat();
            const float a = 1.0f;

            vertices.emplace_back(tcu::Vec4(xCoord, yCoord, depth, 1.0f), tcu::Vec4(r, g, b, a),
                                  tcu::IVec4(stencil, 0, 0, 0));
        }

    const auto vertexBufferSize       = static_cast<VkDeviceSize>(de::dataSize(vertices));
    const auto vertexBufferCreateInfo = makeBufferCreateInfo(vertexBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    BufferWithMemory vertexBuffer(ctx.vkd, ctx.device, ctx.allocator, vertexBufferCreateInfo,
                                  MemoryRequirement::HostVisible);
    auto &vertexBufferAlloc = vertexBuffer.getAllocation();
    void *vertexBufferData  = vertexBufferAlloc.getHostPtr();

    deMemcpy(vertexBufferData, de::dataOrNull(vertices), de::dataSize(vertices));
    flushAlloc(ctx.vkd, ctx.device, vertexBufferAlloc);

    const std::vector<VkBuffer> vertexBuffers(1u, *vertexBuffer);
    const std::vector<VkDeviceSize> vertexBufferOffsets(1u, 0ull);

    // Color attachment (will be verified).
    const auto colorSRR   = makeDefaultImageSubresourceRange();
    const auto colorUsage = (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    ImageWithBuffer colorBuffer(ctx.vkd, ctx.device, ctx.allocator, vkExtent, kColorFormat, colorUsage,
                                VK_IMAGE_TYPE_2D, colorSRR, 1u, kSingleSample);

    // Depth/stencil buffer.
    const auto dsSRR =
        makeImageSubresourceRange((VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT), 0u, 1u, 0u, 1u);
    const auto depthSRR     = makeImageSubresourceRange(VK_IMAGE_ASPECT_DEPTH_BIT, 0u, 1u, 0u, 1u);
    const auto stencilSRR   = makeImageSubresourceRange(VK_IMAGE_ASPECT_STENCIL_BIT, 0u, 1u, 0u, 1u);
    const auto dsUsage      = getDepthStencilUsage(false);
    const auto dsCreateInfo = makeImageCreateInfo(m_params.imageFormat, vkExtent, kSingleSample, dsUsage);
    ImageWithMemory dsBuffer(ctx.vkd, ctx.device, ctx.allocator, dsCreateInfo, MemoryRequirement::Any);

    // Image view for both aspects, to be used as the depth/stencil attachment or resolve attachment.
    const auto dsImageView = makeImageView(ctx.vkd, ctx.device, *dsBuffer, viewType, m_params.imageFormat, dsSRR);

    // Multisample images, used in some variants.
    using ImageWithMemoryPtr = std::unique_ptr<ImageWithMemory>;

    ImageWithMemoryPtr colorMSBuffer;
    ImageWithMemoryPtr dsMSBuffer;
    Move<VkImageView> colorMSView;
    Move<VkImageView> dsMSView;

    if (m_params.isMultisample())
    {
        const auto colorMSUsage      = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        const auto colorMSCreateInfo = makeImageCreateInfo(kColorFormat, vkExtent, mainSampleCount, colorMSUsage);
        colorMSBuffer.reset(
            new ImageWithMemory(ctx.vkd, ctx.device, ctx.allocator, colorMSCreateInfo, MemoryRequirement::Any));
        colorMSView = makeImageView(ctx.vkd, ctx.device, colorMSBuffer->get(), viewType, kColorFormat, colorSRR);

        const auto dsMSUsage      = getDepthStencilUsage(true);
        const auto dsMSCreateInfo = makeImageCreateInfo(m_params.imageFormat, vkExtent, mainSampleCount, dsMSUsage);
        dsMSBuffer.reset(
            new ImageWithMemory(ctx.vkd, ctx.device, ctx.allocator, dsMSCreateInfo, MemoryRequirement::Any));
        dsMSView = makeImageView(ctx.vkd, ctx.device, dsMSBuffer->get(), viewType, m_params.imageFormat, dsSRR);
    }

    // Image view of the read aspect to be used for sampling. Note we always sample the single-sample image.
    const auto readSRR   = makeImageSubresourceRange(m_params.getReadAspect(), 0u, 1u, 0u, 1u);
    const auto readImage = *dsBuffer;
    const auto readView  = makeImageView(ctx.vkd, ctx.device, readImage, viewType, m_params.imageFormat, readSRR);

    // To make it easier, we'll sample the depth/stencil buffer using unnormalized coordinates (see shader code).
    const auto borderColor =
        (m_params.readsDepth() ? VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE : VK_BORDER_COLOR_INT_OPAQUE_WHITE);
    const VkSamplerCreateInfo samplerCreateInfo = {
        VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO, // VkStructureType sType;
        nullptr,                               // const void* pNext;
        0u,                                    // VkSamplerCreateFlags flags;
        VK_FILTER_NEAREST,                     // VkFilter magFilter;
        VK_FILTER_NEAREST,                     // VkFilter minFilter;
        VK_SAMPLER_MIPMAP_MODE_NEAREST,        // VkSamplerMipmapMode mipmapMode;
        VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, // VkSamplerAddressMode addressModeU;
        VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, // VkSamplerAddressMode addressModeV;
        VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, // VkSamplerAddressMode addressModeW;
        0.0f,                                  // float mipLodBias;
        VK_FALSE,                              // VkBool32 anisotropyEnable;
        0.0f,                                  // float maxAnisotropy;
        VK_FALSE,                              // VkBool32 compareEnable;
        VK_COMPARE_OP_ALWAYS,                  // VkCompareOp compareOp;
        0.0f,                                  // float minLod;
        0.0f,                                  // float maxLod;
        borderColor,                           // VkBorderColor borderColor;
        VK_TRUE,                               // VkBool32 unnormalizedCoordinates;
    };
    const auto sampler = createSampler(ctx.vkd, ctx.device, &samplerCreateInfo);

    // Storage image to store sampling results and verify them.
    const auto storageImgFormat = m_params.getStorageImageFormat();
    const auto storageTcuFormat = mapVkFormat(storageImgFormat);
    const auto storageImgUsage  = getStorageImageUsage();
    ImageWithBuffer storageImg(ctx.vkd, ctx.device, ctx.allocator, vkExtent, storageImgFormat, storageImgUsage,
                               VK_IMAGE_TYPE_2D, colorSRR, 1u, kSingleSample);

    // Descriptor set layout, pool and descriptor set.
    DescriptorSetLayoutBuilder setLayoutBuilder;
    setLayoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
    setLayoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_FRAGMENT_BIT);
    const auto setLayout = setLayoutBuilder.build(ctx.vkd, ctx.device);

    DescriptorPoolBuilder poolBuilder;
    poolBuilder.addType(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    poolBuilder.addType(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
    const auto descriptorPool =
        poolBuilder.build(ctx.vkd, ctx.device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);
    const auto descriptorSet = makeDescriptorSet(ctx.vkd, ctx.device, *descriptorPool, *setLayout);

    DescriptorSetUpdateBuilder setUpdateBuilder;
    const auto sampledImgInfo = makeDescriptorImageInfo(*sampler, *readView, readLayout);
    const auto storageImgInfo = makeDescriptorImageInfo(VK_NULL_HANDLE, storageImg.getImageView(), storageImgLayout);
    setUpdateBuilder.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u),
                                 VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &sampledImgInfo);
    setUpdateBuilder.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(1u),
                                 VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &storageImgInfo);
    setUpdateBuilder.update(ctx.vkd, ctx.device);

    // Render pass and framebuffer.
    std::vector<VkImageView> attachments;
    attachments.reserve(4u);
    if (m_params.isMultisample())
    {
        attachments.emplace_back(*colorMSView);
        attachments.emplace_back(*dsMSView);
    }
    attachments.emplace_back(colorBuffer.getImageView());
    attachments.emplace_back(*dsImageView);

    const auto renderPass  = makeSeparateRenderPass(ctx.vkd, ctx.device, m_params);
    const auto framebuffer = makeFramebuffer(ctx.vkd, ctx.device, *renderPass, de::sizeU32(attachments),
                                             de::dataOrNull(attachments), vkExtent.width, vkExtent.height);

    // Pipeline.
    const auto &binaries  = m_context.getBinaryCollection();
    const auto vertModule = createShaderModule(ctx.vkd, ctx.device, binaries.get("vert"));
    const auto fragModule = createShaderModule(ctx.vkd, ctx.device, binaries.get("frag"));

    const auto pipelineLayout = makePipelineLayout(ctx.vkd, ctx.device, *setLayout);

    const std::vector<VkViewport> viewports(1u, makeViewport(vkExtent));
    const std::vector<VkRect2D> scissors(1u, makeRect2D(vkExtent));

    const auto bindingDescs = VertexData::getBindingDescriptions();
    const auto attribDescs  = VertexData::getAttributeDescriptions();

    const VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, // VkStructureType sType;
        nullptr,                                                   // const void* pNext;
        0u,                                                        // VkPipelineVertexInputStateCreateFlags flags;
        de::sizeU32(bindingDescs),                                 // uint32_t vertexBindingDescriptionCount;
        de::dataOrNull(bindingDescs), // const VkVertexInputBindingDescription* pVertexBindingDescriptions;
        de::sizeU32(attribDescs),     // uint32_t vertexAttributeDescriptionCount;
        de::dataOrNull(attribDescs),  // const VkVertexInputAttributeDescription* pVertexAttributeDescriptions;
    };

    const auto depthTestEnabled   = (m_params.writesDepth() && m_params.writesInFragTest());
    const auto stencilTestEnabled = (m_params.writesStencil() && m_params.writesInFragTest());
    const auto stencilPassOp =
        (stencilTestEnabled ? VK_STENCIL_OP_REPLACE : VK_STENCIL_OP_KEEP); // VUID-vkCmdDraw-None-06887.
    const auto stencilState = makeStencilOpState(VK_STENCIL_OP_KEEP, stencilPassOp, VK_STENCIL_OP_KEEP,
                                                 VK_COMPARE_OP_ALWAYS, 0xFFu, 0xFFu, 0u);

    const VkPipelineDepthStencilStateCreateInfo depthStencilStateCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO, // VkStructureType sType;
        nullptr,                                                    // const void* pNext;
        0u,                                                         // VkPipelineDepthStencilStateCreateFlags flags;
        depthTestEnabled,                                           // VkBool32 depthTestEnable;
        depthTestEnabled,                                           // VkBool32 depthWriteEnable;
        VK_COMPARE_OP_ALWAYS,                                       // VkCompareOp depthCompareOp;
        VK_FALSE,                                                   // VkBool32 depthBoundsTestEnable;
        stencilTestEnabled,                                         // VkBool32 stencilTestEnable;
        stencilState,                                               // VkStencilOpState front;
        stencilState,                                               // VkStencilOpState back;
        0.0f,                                                       // float minDepthBounds;
        0.0f,                                                       // float maxDepthBounds;
    };

    // To use the pseudorandom stencil values for each point when the reference value will not be set from the shader, we make the
    // reference value dynamic, change it before each draw and draw a single point per draw call.
    const bool singlePointDraws = (stencilTestEnabled && !m_params.dynamicStencilRef);

    std::vector<VkDynamicState> dynamicStates;
    if (singlePointDraws)
        dynamicStates.push_back(VK_DYNAMIC_STATE_STENCIL_REFERENCE);

    const VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO, // VkStructureType sType;
        nullptr,                                              // const void* pNext;
        0u,                                                   // VkPipelineDynamicStateCreateFlags flags;
        de::sizeU32(dynamicStates),                           // uint32_t dynamicStateCount;
        de::dataOrNull(dynamicStates),                        // const VkDynamicState* pDynamicStates;
    };

    const auto multisampleStateCreateInfo = makePipelineMultisampleStateCreateInfo(mainSampleCount);

    const auto pipeline = makeGraphicsPipeline(
        ctx.vkd, ctx.device, *pipelineLayout, *vertModule, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, *fragModule,
        *renderPass, viewports, scissors, topology, 0u, 0u, &vertexInputStateCreateInfo, nullptr,
        &multisampleStateCreateInfo, &depthStencilStateCreateInfo, nullptr, &dynamicStateCreateInfo);

    // Buffers used to pre-fill the depth/stencil image before the render pass (for VK_LOAD_OP_LOAD and other interesting cases).
    const auto depthCopyFormat   = getDepthCopyFormat(m_params.imageFormat);
    const auto stencilCopyFormat = getStencilCopyFormat(m_params.imageFormat);

    tcu::TextureLevel preFillDepthLevel(depthCopyFormat, fbExtent.x(), fbExtent.y(), fbExtent.z());
    tcu::TextureLevel preFillStencilLevel(stencilCopyFormat, fbExtent.x(), fbExtent.y(), fbExtent.z());

    const auto preFillDepthAccess   = preFillDepthLevel.getAccess();
    const auto preFillStencilAccess = preFillStencilLevel.getAccess();

    if (depthTestEnabled)
        tcu::clearDepth(preFillDepthAccess, 0.0f);
    else
    {
        size_t pointIdx = 0u;
        for (int y = 0; y < fbExtent.y(); ++y)
            for (int x = 0; x < fbExtent.x(); ++x)
                preFillDepthAccess.setPixDepth(vertices.at(pointIdx++).coords.z(), x, y);
    }

    if (stencilTestEnabled)
        tcu::clearStencil(preFillStencilAccess, 0);
    else
    {
        size_t pointIdx = 0u;
        for (int y = 0; y < fbExtent.y(); ++y)
            for (int x = 0; x < fbExtent.x(); ++x)
                preFillStencilAccess.setPixStencil(vertices.at(pointIdx++).extra.x(), x, y);
    }

    const auto fbPixelCount          = fbExtent.x() * fbExtent.y() * fbExtent.z();
    const auto depthCopyBufferSize   = static_cast<VkDeviceSize>(tcu::getPixelSize(depthCopyFormat) * fbPixelCount);
    const auto stencilCopyBufferSize = static_cast<VkDeviceSize>(tcu::getPixelSize(stencilCopyFormat) * fbPixelCount);

    const auto preFillDepthBufferInfo   = makeBufferCreateInfo(depthCopyBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    const auto preFillStencilBufferInfo = makeBufferCreateInfo(stencilCopyBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);

    BufferWithMemory preFillDepthBuffer(ctx.vkd, ctx.device, ctx.allocator, preFillDepthBufferInfo,
                                        MemoryRequirement::HostVisible);
    BufferWithMemory prefillStencilBuffer(ctx.vkd, ctx.device, ctx.allocator, preFillStencilBufferInfo,
                                          MemoryRequirement::HostVisible);

    auto &preFillDepthBufferAlloc   = preFillDepthBuffer.getAllocation();
    auto &preFillStencilBufferAlloc = prefillStencilBuffer.getAllocation();

    void *preFillDepthBufferData   = preFillDepthBufferAlloc.getHostPtr();
    void *preFillStencilBufferData = preFillStencilBufferAlloc.getHostPtr();

    deMemcpy(preFillDepthBufferData, preFillDepthAccess.getDataPtr(), static_cast<size_t>(depthCopyBufferSize));
    deMemcpy(preFillStencilBufferData, preFillStencilAccess.getDataPtr(), static_cast<size_t>(stencilCopyBufferSize));

    flushAlloc(ctx.vkd, ctx.device, preFillDepthBufferAlloc);
    flushAlloc(ctx.vkd, ctx.device, preFillStencilBufferAlloc);

    // Buffers used to verify depth/stencil values.
    const auto depthVerifBufferCreateInfo = makeBufferCreateInfo(depthCopyBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    const auto stencilVerifBufferCreateInfo =
        makeBufferCreateInfo(stencilCopyBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT);

    BufferWithMemory depthVerifBuffer(ctx.vkd, ctx.device, ctx.allocator, depthVerifBufferCreateInfo,
                                      MemoryRequirement::HostVisible);
    BufferWithMemory stencilVerifBuffer(ctx.vkd, ctx.device, ctx.allocator, stencilVerifBufferCreateInfo,
                                        MemoryRequirement::HostVisible);

    // Command buffer.
    CommandPoolWithBuffer cmd(ctx.vkd, ctx.device, ctx.qfIndex);
    const auto cmdBuffer = *cmd.cmdBuffer;

    beginCommandBuffer(ctx.vkd, cmdBuffer);

    VkImageLayout dsLayout       = VK_IMAGE_LAYOUT_UNDEFINED;
    const auto depthSRL          = makeImageSubresourceLayers(VK_IMAGE_ASPECT_DEPTH_BIT, 0u, 0u, 1u);
    const auto stencilSRL        = makeImageSubresourceLayers(VK_IMAGE_ASPECT_STENCIL_BIT, 0u, 0u, 1u);
    const auto colorSRL          = makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u);
    const auto depthCopyRegion   = makeBufferImageCopy(vkExtent, depthSRL);
    const auto stencilCopyRegion = makeBufferImageCopy(vkExtent, stencilSRL);
    const auto colorCopyRegion   = makeBufferImageCopy(vkExtent, colorSRL);

    // Transfer pre-fill contents to the depth/stencil image.
    const auto preCopyLayout =
        (m_params.generalLayout ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    const auto preFillPrepareBarrier =
        makeImageMemoryBarrier(0u, VK_ACCESS_TRANSFER_WRITE_BIT, dsLayout, preCopyLayout, *dsBuffer, dsSRR);
    cmdPipelineImageMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                  &preFillPrepareBarrier);
    dsLayout = preCopyLayout;

    ctx.vkd.cmdCopyBufferToImage(cmdBuffer, *preFillDepthBuffer, *dsBuffer, dsLayout, 1u, &depthCopyRegion);
    ctx.vkd.cmdCopyBufferToImage(cmdBuffer, *prefillStencilBuffer, *dsBuffer, dsLayout, 1u, &stencilCopyRegion);

    // Transition image to the layout used in the render pass. Note the depth/stencil resolve operations happen as part of the
    // VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT using access VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, despite the names.
    if (m_params.separateLayouts)
    {
        const auto depthAccess =
            (m_params.readsDepth() ?
                 (shaderAccesses | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT /* load_op_load */) :
                 (VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT));
        const auto depthStages =
            (m_params.readsDepth() ?
                 (VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                  VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT) :
                 (VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT));

        const auto stencilAccess =
            (m_params.readsStencil() ?
                 (shaderAccesses | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT /*load_op_load*/) :
                 (VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT));
        const auto stencilStages =
            (m_params.readsStencil() ?
                 (VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                  VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT) :
                 (VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT));

        const auto depthBarrier   = makeImageMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, depthAccess, dsLayout,
                                                           m_params.getDepthImageLayout(), *dsBuffer, depthSRR);
        const auto stencilBarrier = makeImageMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, stencilAccess, dsLayout,
                                                           m_params.getStencilImageLayout(), *dsBuffer, stencilSRR);

        const std::vector<VkImageMemoryBarrier> barriers{depthBarrier, stencilBarrier};
        cmdPipelineImageMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, (depthStages | stencilStages),
                                      de::dataOrNull(barriers), barriers.size());
        dsLayout = VK_IMAGE_LAYOUT_UNDEFINED; // Lets make it clear there's not a single dsLayout for now.
    }
    else
    {
        const auto rpAccesses =
            (m_params.isMultisample() ? (VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | shaderAccesses) :
                                        (VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                                         VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | shaderAccesses));
        const auto rpStages =
            (m_params.isMultisample() ?
                 (VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT) :
                 (VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT |
                  VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT));
        const auto rpPrepareBarrier =
            makeImageMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, rpAccesses, dsLayout, readLayout, *dsBuffer, dsSRR);
        cmdPipelineImageMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, rpStages, &rpPrepareBarrier);
        dsLayout = readLayout;
    }

    if (m_params.isMultisample())
    {
        // Transfer the multisample image to the layout used in the render pass.
        const auto msAccesses = (VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                                 VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | shaderAccesses);
        const auto msStages = (VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT |
                               VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
        const auto msPrepareBarrier =
            makeImageMemoryBarrier(0u, msAccesses, VK_IMAGE_LAYOUT_UNDEFINED, readLayout, dsMSBuffer->get(), dsSRR);
        cmdPipelineImageMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, msStages,
                                      &msPrepareBarrier);
    }

    // Transition storage image to the general layout to be used in the frag shader.
    const auto storagePrepareBarrier = makeImageMemoryBarrier(0u, shaderAccesses, VK_IMAGE_LAYOUT_UNDEFINED,
                                                              VK_IMAGE_LAYOUT_GENERAL, storageImg.getImage(), colorSRR);
    cmdPipelineImageMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                  VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, &storagePrepareBarrier);

    // Clear values for those images that need it.
    std::vector<VkClearValue> clearValues;
    clearValues.push_back(makeClearValueColor(tcu::Vec4(0.0f, 0.0f, 0.0f, 0.0f)));
    if (m_params.writeMechanism == WriteMechanism::RP_CLEAR)
        clearValues.push_back(makeClearValueDepthStencil(0.0f, 0u));

    beginRenderPass(ctx.vkd, cmdBuffer, *renderPass, *framebuffer, scissors.at(0u), de::sizeU32(clearValues),
                    de::dataOrNull(clearValues));
    ctx.vkd.cmdBindDescriptorSets(cmdBuffer, kBindPoint, *pipelineLayout, 0u, 1u, &descriptorSet.get(), 0u, nullptr);
    ctx.vkd.cmdBindPipeline(cmdBuffer, kBindPoint, *pipeline);
    DE_ASSERT(bindingDescs.size() == vertexBuffers.size());
    DE_ASSERT(bindingDescs.size() == vertexBufferOffsets.size());
    ctx.vkd.cmdBindVertexBuffers(cmdBuffer, 0u, de::sizeU32(bindingDescs), de::dataOrNull(vertexBuffers),
                                 de::dataOrNull(vertexBufferOffsets));
    if (singlePointDraws)
    {
        for (size_t i = 0u; i < vertices.size(); ++i)
        {
            ctx.vkd.cmdSetStencilReference(cmdBuffer, VK_STENCIL_FACE_FRONT_AND_BACK,
                                           static_cast<uint32_t>(vertices.at(i).extra.x()));
            ctx.vkd.cmdDraw(cmdBuffer, 1u, 1u, static_cast<uint32_t>(i), 0u);
        }
    }
    else
        ctx.vkd.cmdDraw(cmdBuffer, de::sizeU32(vertices), 1u, 0u, 0u);
    endRenderPass(ctx.vkd, cmdBuffer);

    // Prepare images for verification copy.
    std::vector<VkImageMemoryBarrier> preCopyBarriers;
    preCopyBarriers.reserve(4u);

    const auto preVerifLayout =
        (m_params.generalLayout ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

    // Color and storage preparation.
    preCopyBarriers.emplace_back(makeImageMemoryBarrier(
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, colorBuffer.getImage(), colorSRR));
    preCopyBarriers.emplace_back(makeImageMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
                                                        VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL,
                                                        storageImg.getImage(), colorSRR));

    // Depth/stencil preparation.
    if (m_params.separateLayouts)
    {
        const auto depthAccess =
            (m_params.readsDepth() ?
                 (shaderAccesses | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT /* store_op_store */) :
                 (VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT));

        const auto stencilAccess =
            (m_params.readsStencil() ?
                 (shaderAccesses | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT /* store_op_store */) :
                 (VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT));

        preCopyBarriers.emplace_back(makeImageMemoryBarrier(depthAccess, VK_ACCESS_TRANSFER_READ_BIT,
                                                            m_params.getDepthImageLayout(), preVerifLayout, *dsBuffer,
                                                            depthSRR));
        preCopyBarriers.emplace_back(makeImageMemoryBarrier(stencilAccess, VK_ACCESS_TRANSFER_READ_BIT,
                                                            m_params.getStencilImageLayout(), preVerifLayout, *dsBuffer,
                                                            stencilSRR));
    }
    else
    {
        const auto prevAccess =
            (m_params.isMultisample() ?
                 VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT // Depth/stencil resolve happens with this access.
                 :
                 VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT);

        preCopyBarriers.emplace_back(makeImageMemoryBarrier(prevAccess, VK_ACCESS_TRANSFER_READ_BIT, dsLayout,
                                                            preVerifLayout, *dsBuffer, dsSRR));
    }

    const auto writeStages =
        (VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
         VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
    cmdPipelineImageMemoryBarrier(ctx.vkd, cmdBuffer, writeStages, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                  de::dataOrNull(preCopyBarriers), preCopyBarriers.size());
    dsLayout = preVerifLayout; // Always single layout again.

    // Copy images for verification.
    ctx.vkd.cmdCopyImageToBuffer(cmdBuffer, colorBuffer.getImage(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                 colorBuffer.getBuffer(), 1u, &colorCopyRegion);
    ctx.vkd.cmdCopyImageToBuffer(cmdBuffer, storageImg.getImage(), VK_IMAGE_LAYOUT_GENERAL, storageImg.getBuffer(), 1u,
                                 &colorCopyRegion);
    ctx.vkd.cmdCopyImageToBuffer(cmdBuffer, *dsBuffer, dsLayout, *depthVerifBuffer, 1u, &depthCopyRegion);
    ctx.vkd.cmdCopyImageToBuffer(cmdBuffer, *dsBuffer, dsLayout, *stencilVerifBuffer, 1u, &stencilCopyRegion);

    // Sync to host reads.
    const auto preHostBarrier = makeMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
    cmdPipelineMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                             &preHostBarrier);

    endCommandBuffer(ctx.vkd, cmdBuffer);
    submitCommandsAndWait(ctx.vkd, ctx.device, ctx.queue, cmdBuffer);

    // Verify color and depth/stencil buffers.
    invalidateAlloc(ctx.vkd, ctx.device, colorBuffer.getBufferAllocation());
    invalidateAlloc(ctx.vkd, ctx.device, storageImg.getBufferAllocation());
    invalidateAlloc(ctx.vkd, ctx.device, depthVerifBuffer.getAllocation());
    invalidateAlloc(ctx.vkd, ctx.device, stencilVerifBuffer.getAllocation());

    tcu::ConstPixelBufferAccess colorAccess(mapVkFormat(kColorFormat), fbExtent,
                                            colorBuffer.getBufferAllocation().getHostPtr());
    tcu::ConstPixelBufferAccess storageAccess(storageTcuFormat, fbExtent,
                                              storageImg.getBufferAllocation().getHostPtr());
    tcu::ConstPixelBufferAccess depthAccess(depthCopyFormat, fbExtent, depthVerifBuffer.getAllocation().getHostPtr());
    tcu::ConstPixelBufferAccess stencilAccess(stencilCopyFormat, fbExtent,
                                              stencilVerifBuffer.getAllocation().getHostPtr());

    // Generate reference images for these.
    tcu::TextureLevel refColorLevel(tcuColorFormat, fbExtent.x(), fbExtent.y(), fbExtent.z());
    tcu::TextureLevel refStorageLevel(storageTcuFormat, fbExtent.x(), fbExtent.y(), fbExtent.z());
    tcu::TextureLevel refDepthLevel(depthCopyFormat, fbExtent.x(), fbExtent.y(), fbExtent.z());
    tcu::TextureLevel refStencilLevel(stencilCopyFormat, fbExtent.x(), fbExtent.y(), fbExtent.z());

    const auto refColorAccess   = refColorLevel.getAccess();
    const auto refStorageAccess = refStorageLevel.getAccess();
    const auto refDepthAccess   = refDepthLevel.getAccess();
    const auto refStencilAccess = refStencilLevel.getAccess();

    uint32_t pixelIdx = 0u;
    for (int y = 0; y < fbExtent.y(); ++y)
        for (int x = 0; x < fbExtent.x(); ++x)
        {
            const auto &vertex = vertices.at(pixelIdx++);
            refColorAccess.setPixel(vertex.color, x, y);

            const float depth =
                (m_params.writesDepth() && m_params.writeMechanism == WriteMechanism::RP_CLEAR ? 0.0f :
                                                                                                 vertex.coords.z());
            refDepthAccess.setPixDepth(depth, x, y);

            const int stencil =
                (m_params.writesStencil() && m_params.writeMechanism == WriteMechanism::RP_CLEAR ? 0 :
                                                                                                   vertex.extra.x());
            refStencilAccess.setPixStencil(stencil, x, y);

            if (m_params.readsDepth())
                refStorageAccess.setPixel(tcu::Vec4(vertex.coords.z(), 0.0f, 0.0f, 0.0f), x, y);
            else if (m_params.readsStencil())
                refStorageAccess.setPixel(tcu::UVec4(static_cast<uint32_t>(vertex.extra.x()), 0u, 0u, 0u), x, y);
            else
                DE_ASSERT(false);
        }

    auto &log = m_context.getTestContext().getLog();

    const auto depthThreshold   = getDepthThreshold(depthCopyFormat);
    const auto stencilThreshold = 0.0f;

    bool colorOK = tcu::floatThresholdCompare(log, "ColorBuffer", "", refColorAccess, colorAccess,
                                              tcu::Vec4(kColorThreshold, kColorThreshold, kColorThreshold, 0.0f),
                                              tcu::COMPARE_LOG_EVERYTHING);

    bool storageOK =
        (m_params.readsDepth() ?
             tcu::floatThresholdCompare(log, "StorageBuffer", "", refStorageAccess, storageAccess,
                                        tcu::Vec4(depthThreshold, 0.0f, 0.0f, 0.0f), tcu::COMPARE_LOG_EVERYTHING) :
             tcu::intThresholdCompare(log, "StorageBuffer", "", refStorageAccess, storageAccess,
                                      tcu::UVec4(0u, 0u, 0u, 0u), tcu::COMPARE_LOG_EVERYTHING));

    bool depthOK =
        (m_params.readsDepth() || // In this case the depth values will be verified through the storage image.
         (m_params.writesDepth() && m_params.writeMechanism == WriteMechanism::RP_DONT_CARE) ||
         tcu::dsThresholdCompare(log, "DepthBuffer", "", refDepthAccess, depthAccess, depthThreshold,
                                 tcu::COMPARE_LOG_EVERYTHING));

    bool stencilOK =
        (m_params.readsStencil() || // In this case the stencil values will be verified through the storage image.
         (m_params.writesStencil() && m_params.writeMechanism == WriteMechanism::RP_DONT_CARE) ||
         tcu::dsThresholdCompare(log, "DepthBuffer", "", refStencilAccess, stencilAccess, stencilThreshold,
                                 tcu::COMPARE_LOG_EVERYTHING));

    if (colorOK && storageOK && depthOK && stencilOK)
        return tcu::TestStatus::pass("Pass");
    return tcu::TestStatus::fail("Unexpected contents in one or more output buffers; check log for details");
}

} // namespace

tcu::TestCaseGroup *createImageDepthStencilSeparateTests(tcu::TestContext &testCtx)
{
    using TestGroupPtr = de::MovePtr<tcu::TestCaseGroup>;

    TestGroupPtr mainGroup(new tcu::TestCaseGroup(testCtx, "depth_stencil_separate_access"));

    const VkFormat dsFormats[] = {
        VK_FORMAT_D16_UNORM_S8_UINT,
        VK_FORMAT_D24_UNORM_S8_UINT,
        VK_FORMAT_D32_SFLOAT_S8_UINT,
    };

    const struct
    {
        VkImageAspectFlagBits aspect;
        const char *name;
    } writeAspects[] = {
        {VK_IMAGE_ASPECT_DEPTH_BIT, "write_depth"},
        {VK_IMAGE_ASPECT_STENCIL_BIT, "write_stencil"},
    };

    const struct
    {
        WriteMechanism mechanism;
        const char *name;
    } writeMechanisms[] = {
        {WriteMechanism::RP_CLEAR, "render_pass_clears"},
        {WriteMechanism::RP_DONT_CARE, "render_pass_dont_care"},
        {WriteMechanism::TEST_STORE, "test_and_store"},
        {WriteMechanism::TEST_RESOLVE, "test_and_resolve"},
    };

    for (const auto format : dsFormats)
    {
        const auto groupName = getFormatShortString(format);
        TestGroupPtr formatGroup(new tcu::TestCaseGroup(testCtx, groupName.c_str()));

        for (const auto &writeAspect : writeAspects)
            for (const auto &writeMechanism : writeMechanisms)
                for (const auto &generalLayout : {false, true})
                    for (const auto &separateLayouts : {false, true})
                        for (const auto dynamicStencilRef : {false, true})
                        {
                            if (dynamicStencilRef && (writeAspect.aspect != VK_IMAGE_ASPECT_STENCIL_BIT ||
                                                      !writesWithTest(writeMechanism.mechanism)))
                                continue;

                            // Would not make sense.
                            if (generalLayout && separateLayouts)
                                continue;

                            // Avoid combinatory explosion.
                            if (writesWithResolve(writeMechanism.mechanism) && separateLayouts)
                                continue;

                            const auto layoutSuffix  = (generalLayout ? "_general_layout" : "");
                            const auto slSuffix      = (separateLayouts ? "_separate_layouts" : "");
                            const auto stencilSuffix = (dynamicStencilRef ? "_dynamic_stencil_ref" : "");
                            const auto testName      = writeAspect.name + std::string("_") + writeMechanism.name +
                                                  layoutSuffix + slSuffix + stencilSuffix;

                            const TestParams params{format,        writeAspect.aspect, writeMechanism.mechanism,
                                                    generalLayout, separateLayouts,    dynamicStencilRef};

                            formatGroup->addChild(new DepthStencilSeparateCase(testCtx, testName, params));
                        }

        mainGroup->addChild(formatGroup.release());
    }

    return mainGroup.release();
}

} // namespace image
} // namespace vkt
