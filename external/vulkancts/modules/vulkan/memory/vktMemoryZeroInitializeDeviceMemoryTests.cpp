/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2025 The Khronos Group Inc.
 * Copyright (c) 2025-2026 Valve Corporation.
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
 * \brief Tests for VK_EXT_zero_initialize_device_memory
 *//*--------------------------------------------------------------------*/

#include "vktMemoryZeroInitializeDeviceMemoryTests.hpp"
#include "vktTestCase.hpp"
#include "vktTestCaseUtil.hpp"
#include "vktTestGroupUtil.hpp"

#include "vkDefs.hpp"
#include "vkMemUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkCmdUtil.hpp"
#include "vkBarrierUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkFormatLists.hpp"

#include "tcuTextureUtil.hpp"
#include "tcuImageCompare.hpp"

#include "deUniquePtr.hpp"

#include <algorithm>
#include <cstring>
#include <memory>
#include <vector>
#include <string>
#include <sstream>

namespace vkt
{
namespace memory
{
namespace
{

using namespace vk;

void checkZeroInitializeDeviceMemorySupport(Context &context)
{
    context.requireDeviceFunctionality("VK_EXT_zero_initialize_device_memory");
}

struct BufferAllocationParams
{
    VkDeviceSize bufferSize;
    VkBufferUsageFlags bufferUsageFlags;
    bool hostVisible;
};

void clearBufferAllocationCheckSupport(Context &context, BufferAllocationParams)
{
    checkZeroInitializeDeviceMemorySupport(context);
}

uint32_t getMemoryTypeList(const VkPhysicalDeviceMemoryProperties &memProperties, uint32_t usableMemTypes,
                           MemoryRequirement requirement, VkMemoryPropertyFlags forbidden)
{
    uint32_t testedTypes = 0u;

    for (uint32_t i = 0u; i < memProperties.memoryTypeCount; ++i)
    {
        const auto &memFlags = memProperties.memoryTypes[i].propertyFlags;

        if ((memFlags & forbidden) != 0u)
            continue;

        const uint32_t mask = (1u << i);
        if ((usableMemTypes & mask) == 0u)
            continue;

        if (!requirement.matchesHeap(memFlags))
            continue;

        testedTypes |= mask;
    }

    return testedTypes;
}

uint32_t getTestedMemoryTypes(const VkPhysicalDeviceMemoryProperties &memProperties, uint32_t usableMemTypes,
                              MemoryRequirement requirement)
{
    // We want to skip protected memory types, and device coherent AMD memory types because the extension is not enabled
    // by default.
    const VkMemoryPropertyFlags forbidden =
        (VK_MEMORY_PROPERTY_PROTECTED_BIT | VK_MEMORY_PROPERTY_DEVICE_COHERENT_BIT_AMD);
    return getMemoryTypeList(memProperties, usableMemTypes, requirement, forbidden);
}

using AllocationPtr = de::MovePtr<Allocation>;

// This function allocates memory for the buffers and images being tested, so it always adds the flag.
AllocationPtr allocateZeroInitMemory(Allocator &alloc, const VkMemoryRequirements &reqs, uint32_t memTypeIdx)
{
    const auto memTypeMask = (1u << memTypeIdx);
    DE_ASSERT((reqs.memoryTypeBits & memTypeMask) != 0u);
    DE_UNREF(memTypeMask); // For release builds.

    const VkMemoryAllocateFlagsInfo flagsInfo = {
        VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO,
        nullptr,
        VK_MEMORY_ALLOCATE_ZERO_INITIALIZE_BIT_EXT,
        0u,
    };

    const VkMemoryAllocateInfo allocateInfo = {
        VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        &flagsInfo,
        reqs.size,
        memTypeIdx,
    };
    return alloc.allocate(allocateInfo, reqs.alignment);
}

tcu::TestStatus clearBufferAllocation(Context &context, BufferAllocationParams params)
{
    const auto ctx = context.getContextCommonData();

    // Allocate buffer memory clearing it to zero.
    const auto usageFlags = (params.bufferUsageFlags | (params.hostVisible ? 0 : VK_BUFFER_USAGE_TRANSFER_SRC_BIT));
    const auto bufferCreateInfo = makeBufferCreateInfo(params.bufferSize, usageFlags);
    const auto memReqs          = (MemoryRequirement::ZeroInitialize |
                          (params.hostVisible ? MemoryRequirement::HostVisible : MemoryRequirement::Any));

    const auto templateBuffer = createBuffer(ctx.vkd, ctx.device, &bufferCreateInfo);
    const auto bufferMemReqs  = getBufferMemoryRequirements(ctx.vkd, ctx.device, *templateBuffer);

    VkPhysicalDeviceMemoryProperties memProperties;
    ctx.vki.getPhysicalDeviceMemoryProperties(ctx.physicalDevice, &memProperties);

    const auto testedMemTypes = getTestedMemoryTypes(memProperties, bufferMemReqs.memoryTypeBits, memReqs);
    if (testedMemTypes == 0u)
        TCU_THROW(NotSupportedError, "No compatible memory types found");

    std::unique_ptr<BufferWithMemory> dstBuffer;
    if (!params.hostVisible)
    {
        const auto dstBuferUsage       = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        const auto dstBufferCreateInfo = makeBufferCreateInfo(params.bufferSize, dstBuferUsage);
        dstBuffer.reset(new BufferWithMemory(ctx.vkd, ctx.device, ctx.allocator, dstBufferCreateInfo,
                                             MemoryRequirement::HostVisible));
    }

    bool fail = false;
    auto &log = context.getTestContext().getLog();

    for (uint32_t memTypeIdx = 0u; memTypeIdx < memProperties.memoryTypeCount; ++memTypeIdx)
    {
        const auto memTypeMask = (1u << memTypeIdx);
        if ((testedMemTypes & memTypeMask) == 0u)
            continue;

        const auto testedBuffer = createBuffer(ctx.vkd, ctx.device, &bufferCreateInfo);
        auto testedBufferAlloc  = allocateZeroInitMemory(ctx.allocator, bufferMemReqs, memTypeIdx);
        VK_CHECK(ctx.vkd.bindBufferMemory(ctx.device, *testedBuffer, testedBufferAlloc->getMemory(),
                                          testedBufferAlloc->getOffset()));

        if (!params.hostVisible)
        {
            CommandPoolWithBuffer cmd(ctx.vkd, ctx.device, ctx.qfIndex);
            const auto cmdBuffer = *cmd.cmdBuffer;

            beginCommandBuffer(ctx.vkd, cmdBuffer);

            const VkBufferCopy region = {0ull, 0ull, params.bufferSize};
            ctx.vkd.cmdCopyBuffer(cmdBuffer, *testedBuffer, dstBuffer->get(), 1u, &region);

            const auto barrier = makeMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
            cmdPipelineMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                                     &barrier);

            endCommandBuffer(ctx.vkd, cmdBuffer);
            submitCommandsAndWait(ctx.vkd, ctx.device, ctx.queue, cmdBuffer);

            auto &dstBufferAlloc = dstBuffer->getAllocation();
            invalidateAlloc(ctx.vkd, ctx.device, dstBufferAlloc);
        }

        const Allocation &verifiedAllocation = (dstBuffer ? dstBuffer->getAllocation() : *testedBufferAlloc);
        void *verifiedBufferData             = verifiedAllocation.getHostPtr();

        const auto bufferSizeSz = static_cast<size_t>(params.bufferSize);
        std::vector<uint8_t> refBuffer(bufferSizeSz, uint8_t{0});
        auto ret = memcmp(verifiedBufferData, refBuffer.data(), bufferSizeSz);

        if (ret != 0)
        {
            fail = true;
            std::ostringstream msg;
            msg << "Memory type " << memTypeIdx << " failed";
            log << tcu::TestLog::Message << msg.str() << tcu::TestLog::EndMessage;
        }

        context.getTestContext().touchWatchdog();
    }

    if (fail)
        TCU_FAIL("Some memory types failed; check log for details --");

    return tcu::TestStatus::pass("Pass");
}

struct ImageTransitionParams
{
    VkFormat format;
    VkImageUsageFlagBits mainUsage;
    tcu::IVec3 mipExtent;
    bool firstMip;
    VkShaderStageFlagBits readStage;

    VkImageType getImageType() const
    {
        return VK_IMAGE_TYPE_2D;
    }

    VkImageViewType getImageViewType() const
    {
        return VK_IMAGE_VIEW_TYPE_2D;
    }

    VkImageTiling getImageTiling() const
    {
        return VK_IMAGE_TILING_OPTIMAL;
    }

    VkImageCreateFlags getImageCreateFlags() const
    {
        return 0u;
    }

    uint32_t getMipLevelCount() const
    {
        return (firstMip ? 1u : 2u);
    }

    tcu::IVec3 getCreationExtent() const
    {
        return (firstMip ? mipExtent : tcu::IVec3(2u, 2u, 1u) * mipExtent);
    }

    VkPipelineStageFlagBits getReadPipelineStage() const
    {
        if (mainUsage == VK_IMAGE_USAGE_TRANSFER_SRC_BIT)
            return VK_PIPELINE_STAGE_TRANSFER_BIT;
        if (readStage == VK_SHADER_STAGE_COMPUTE_BIT)
            return VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        if (readStage == VK_SHADER_STAGE_FRAGMENT_BIT)
            return VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;

        DE_ASSERT(false);
        return VK_PIPELINE_STAGE_NONE;
    }

    tcu::TextureFormat getUncompressedFormat() const
    {
        if (isCompressedFormat(format))
            return tcu::getUncompressedFormat(mapVkCompressedFormat(format));
        return mapVkFormat(format);
    }
};

class ImageTransitionTest : public vkt::TestInstance
{
public:
    static constexpr uint32_t kWorkGroupSize = 64u;

    ImageTransitionTest(Context &context, const ImageTransitionParams &params)
        : vkt::TestInstance(context)
        , m_params(params)
    {
    }
    virtual ~ImageTransitionTest(void) = default;

    tcu::TestStatus iterate(void) override;

protected:
    const ImageTransitionParams m_params;
};

class ImageTransitionCase : public vkt::TestCase
{
public:
    ImageTransitionCase(tcu::TestContext &testCtx, const std::string &name, const ImageTransitionParams &params)
        : vkt::TestCase(testCtx, name)
        , m_params(params)
    {
        switch (m_params.mainUsage)
        {
        case VK_IMAGE_USAGE_TRANSFER_SRC_BIT:
        case VK_IMAGE_USAGE_SAMPLED_BIT:
        case VK_IMAGE_USAGE_STORAGE_BIT:
            break;
        default:
            DE_ASSERT(false);
            break;
        }

        if (isDepthStencilFormat(m_params.format))
            DE_ASSERT(m_params.mainUsage != VK_IMAGE_USAGE_STORAGE_BIT);
    }
    virtual ~ImageTransitionCase(void) = default;

    void checkSupport(Context &context) const override;
    void initPrograms(vk::SourceCollections &programCollection) const override;
    TestInstance *createInstance(Context &context) const override
    {
        return new ImageTransitionTest(context, m_params);
    }

protected:
    const ImageTransitionParams m_params;
};

void ImageTransitionCase::checkSupport(Context &context) const
{
    checkZeroInitializeDeviceMemorySupport(context);

    const auto ctx = context.getContextCommonData();
    VkImageFormatProperties formatProperties;

    const auto result = ctx.vki.getPhysicalDeviceImageFormatProperties(
        ctx.physicalDevice, m_params.format, m_params.getImageType(), m_params.getImageTiling(),
        static_cast<VkImageUsageFlags>(m_params.mainUsage), m_params.getImageCreateFlags(), &formatProperties);

    if (result != VK_SUCCESS)
    {
        if (result == VK_ERROR_FORMAT_NOT_SUPPORTED)
            TCU_THROW(NotSupportedError, "Format not supported for the target usage");
        else
            TCU_FAIL(std::string("vkGetPhysicalDeviceImageFormatProperties returned ") + getResultName(result));
    }

    const auto creationExtent   = m_params.getCreationExtent();
    const auto creationExtentVk = makeExtent3D(creationExtent);

    if (creationExtentVk.width > formatProperties.maxExtent.width ||
        creationExtentVk.height > formatProperties.maxExtent.height ||
        creationExtentVk.depth > formatProperties.maxExtent.depth)
    {
        TCU_THROW(NotSupportedError, "Requested extent not supported");
    }

    const auto mipLevelCount = m_params.getMipLevelCount();
    if (mipLevelCount > formatProperties.maxMipLevels)
        TCU_THROW(NotSupportedError, "Requested mip level count not supported");
}

std::string getShaderImageFormatQualifier(const tcu::TextureFormat &format)
{
    const char *orderPart;
    const char *typePart;

    switch (format.order)
    {
    case tcu::TextureFormat::R:
        orderPart = "r";
        break;
    case tcu::TextureFormat::RG:
        orderPart = "rg";
        break;
    case tcu::TextureFormat::RGB:
        orderPart = "rgb";
        break;
    case tcu::TextureFormat::RGBA:
        orderPart = "rgba";
        break;

    default:
        DE_FATAL("Unexpected channel order");
        orderPart = nullptr;
    }

    switch (format.type)
    {
    case tcu::TextureFormat::FLOAT:
        typePart = "32f";
        break;
    case tcu::TextureFormat::HALF_FLOAT:
        typePart = "16f";
        break;

    case tcu::TextureFormat::UNSIGNED_INT32:
        typePart = "32ui";
        break;
    case tcu::TextureFormat::UNSIGNED_INT16:
        typePart = "16ui";
        break;
    case tcu::TextureFormat::UNSIGNED_INT8:
        typePart = "8ui";
        break;

    case tcu::TextureFormat::SIGNED_INT32:
        typePart = "32i";
        break;
    case tcu::TextureFormat::SIGNED_INT16:
        typePart = "16i";
        break;
    case tcu::TextureFormat::SIGNED_INT8:
        typePart = "8i";
        break;

    case tcu::TextureFormat::UNORM_INT16:
        typePart = "16";
        break;
    case tcu::TextureFormat::UNORM_INT8:
        typePart = "8";
        break;

    case tcu::TextureFormat::SNORM_INT16:
        typePart = "16_snorm";
        break;
    case tcu::TextureFormat::SNORM_INT8:
        typePart = "8_snorm";
        break;

    default:
        DE_FATAL("Unexpected channel type");
        typePart = nullptr;
    }

    return std::string() + orderPart + typePart;
}

void ImageTransitionCase::initPrograms(vk::SourceCollections &programCollection) const
{
    constexpr auto kWorkGroupSize = ImageTransitionTest::kWorkGroupSize;

    if (m_params.mainUsage == VK_IMAGE_USAGE_TRANSFER_SRC_BIT)
    {
        DE_ASSERT(m_params.readStage == VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM);
        return;
    }

    const auto tcuFormat    = m_params.getUncompressedFormat();
    const auto channelClass = tcu::getTextureChannelClass(tcuFormat.type);
    const bool isInt        = (channelClass == tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER);
    const bool isUint       = (channelClass == tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER);
    const bool isStorageImg = (m_params.mainUsage == VK_IMAGE_USAGE_STORAGE_BIT);

    // The resource will be at set=0, binding=0. Set resource params here.
    const auto formatSuffix =
        (isStorageImg ? (", " + getShaderImageFormatQualifier(m_params.getUncompressedFormat())) : "");
    const auto resTypePrefix = (isInt ? "i" : (isUint ? "u" : ""));
    const auto resourceType  = (isStorageImg ? "image2D" : "sampler2D");
    const auto pixelType     = (isInt ? "ivec4" : (isUint ? "uvec4" : "vec4"));
    const auto readOp        = (isStorageImg ? "imageLoad" : "texelFetch");
    const auto lodArg        = (isStorageImg ? "" : ", 0");

    std::ostringstream descriptors;
    descriptors << "layout (set=0, binding=0" << formatSuffix << ") uniform " << resTypePrefix << resourceType
                << " res;\n"
                << "layout (set=0, binding=1) buffer OutBlock { " << pixelType << " pixels[]; } ssbo;\n";
    const auto descriptorDecl = descriptors.str();

    if (m_params.readStage == VK_SHADER_STAGE_COMPUTE_BIT)
    {
        std::ostringstream comp;
        comp << "#version 460\n"
             << "layout (local_size_x=" << kWorkGroupSize << ", local_size_y=1, local_size_z=1) in;\n"
             << descriptorDecl << "void main(void) {\n"
             << "    // One row per WG.\n"
             << "    const uint width = " << m_params.mipExtent.x() << ";\n"
             << "    const uint height = " << m_params.mipExtent.y() << ";\n"
             << "    const uint wgSize = gl_WorkGroupSize.x;\n"
             << "    const uint pixelsPerInv = (width + (wgSize - 1u)) / wgSize;\n"
             << "    for (uint i = 0; i < pixelsPerInv; ++i) {\n"
             << "        const uint col = i * wgSize + gl_LocalInvocationIndex;\n"
             << "        const uint row = gl_WorkGroupID.x;\n"
             << "        if (col < width && row < height) {\n"
             << "            " << pixelType << " color = " << readOp << "(res, ivec2(col, row)" << lodArg << ");\n"
             << "            const uint outIndex = row * width + col;\n"
             << "            ssbo.pixels[outIndex] = color;\n"
             << "        }\n"
             << "    }\n"
             << "}\n";
        programCollection.glslSources.add("comp") << glu::ComputeSource(comp.str());
    }
    else if (m_params.readStage == VK_SHADER_STAGE_FRAGMENT_BIT)
    {
        std::ostringstream vert;
        vert << "#version 460\n"
             << "vec2 positions[3] = vec2[](\n"
             << "    vec2(-1.0, -1.0),\n"
             << "    vec2( 3.0, -1.0),\n"
             << "    vec2(-1.0,  3.0)\n"
             << ");\n"
             << "void main (void) {\n"
             << "    gl_Position = vec4(positions[gl_VertexIndex % 3], 0.0, 1.0);\n"
             << "    gl_PointSize = 1.0;\n"
             << "}\n";
        programCollection.glslSources.add("vert") << glu::VertexSource(vert.str());

        std::ostringstream frag;
        frag << "#version 460\n"
             << descriptorDecl << "void main(void) {\n"
             << "    const uint width = " << m_params.mipExtent.x() << ";\n"
             << "    const uint col = uint(gl_FragCoord.x);\n"
             << "    const uint row = uint(gl_FragCoord.y);\n"
             << "    " << pixelType << " color = " << readOp << "(res, ivec2(col, row)" << lodArg << ");\n"
             << "    const uint outIndex = row * width + col;\n"
             << "    ssbo.pixels[outIndex] = color;\n"
             << "}\n";
        programCollection.glslSources.add("frag") << glu::FragmentSource(frag.str());
    }
    else
        DE_ASSERT(false);
}

tcu::TestStatus ImageTransitionTest::iterate(void)
{
    const auto ctx              = m_context.getContextCommonData();
    const auto constructionType = PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC;

    const auto mipLevelCount = m_params.getMipLevelCount();
    const auto imageUsage    = static_cast<VkImageUsageFlags>(VK_IMAGE_USAGE_TRANSFER_SRC_BIT | m_params.mainUsage);
    const auto pixelCount    = m_params.mipExtent.x() * m_params.mipExtent.y() * m_params.mipExtent.z();
    const bool isCompressed  = isCompressedFormat(m_params.format);
    const auto tcuFormat     = m_params.getUncompressedFormat();
    const auto channelClass  = tcu::getTextureChannelClass(tcuFormat.type);
    const bool isInt         = (channelClass == tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER);
    const bool isUint        = (channelClass == tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER);
    const auto mipExtentVk   = makeExtent3D(m_params.mipExtent);

    const VkImageCreateInfo imageCreateInfo = {
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        nullptr,
        m_params.getImageCreateFlags(),
        m_params.getImageType(),
        m_params.format,
        makeExtent3D(m_params.getCreationExtent()),
        mipLevelCount,
        1u,
        VK_SAMPLE_COUNT_1_BIT,
        m_params.getImageTiling(),
        imageUsage,
        VK_SHARING_MODE_EXCLUSIVE,
        0u,
        nullptr,
        VK_IMAGE_LAYOUT_ZERO_INITIALIZED_EXT,
    };
    const auto templateImage = createImage(ctx.vkd, ctx.device, &imageCreateInfo);
    const auto imageMemReqs  = getImageMemoryRequirements(ctx.vkd, ctx.device, *templateImage);

    VkPhysicalDeviceMemoryProperties memProperties;
    ctx.vki.getPhysicalDeviceMemoryProperties(ctx.physicalDevice, &memProperties);

    const auto testedMemTypes =
        getTestedMemoryTypes(memProperties, imageMemReqs.memoryTypeBits, MemoryRequirement::ZeroInitialize);

    const bool isTransfer = (m_params.mainUsage == VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    const auto fullSRR    = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, imageCreateInfo.mipLevels, 0u,
                                                      imageCreateInfo.arrayLayers);
    const auto viewLevel  = mipLevelCount - 1u;
    const auto viewSRR    = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, viewLevel, 1u, 0u, 1u);

    VkDeviceSize bufferSize        = 0ull;
    VkBufferUsageFlags bufferUsage = 0u;

    if (isTransfer)
    {
        DE_ASSERT(!isCompressed); // We would need some special calculations taking into account the block size.
        const auto pixelSize = tcu::getPixelSize(tcuFormat);
        bufferSize           = pixelSize * pixelCount;
        bufferUsage          = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    }
    else
    {
        // Fixed buffer size: array of vec4, ivec4 or uvec4, which are equal in size.
        bufferSize  = pixelCount * sizeof(tcu::Vec4);
        bufferUsage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    }

    const auto bufferCreateInfo = makeBufferCreateInfo(static_cast<VkDeviceSize>(bufferSize), bufferUsage);
    BufferWithMemory buffer(ctx.vkd, ctx.device, ctx.allocator, bufferCreateInfo, MemoryRequirement::HostVisible);
    auto &bufferAlloc = buffer.getAllocation();

    auto &log = m_context.getTestContext().getLog();
    bool fail = false;

    for (uint32_t memTypeIdx = 0u; memTypeIdx < memProperties.memoryTypeCount; ++memTypeIdx)
    {
        const auto memTypeMask = (1u << memTypeIdx);
        if ((testedMemTypes & memTypeMask) == 0u)
            continue;

        const auto image = createImage(ctx.vkd, ctx.device, &imageCreateInfo);
        auto imageAlloc  = allocateZeroInitMemory(ctx.allocator, imageMemReqs, memTypeIdx);
        VK_CHECK(ctx.vkd.bindImageMemory(ctx.device, *image, imageAlloc->getMemory(), imageAlloc->getOffset()));

        const auto imageView = (isTransfer ? Move<VkImageView>() :
                                             makeImageView(ctx.vkd, ctx.device, *image, m_params.getImageViewType(),
                                                           m_params.format, viewSRR));

        CommandPoolWithBuffer cmd(ctx.vkd, ctx.device, ctx.qfIndex);
        const auto cmdBuffer = *cmd.cmdBuffer;

        if (isTransfer)
        {
            beginCommandBuffer(ctx.vkd, cmdBuffer);
            {
                const VkImageMemoryBarrier barrier = {
                    VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                    nullptr,
                    0u,
                    VK_ACCESS_TRANSFER_READ_BIT,
                    VK_IMAGE_LAYOUT_ZERO_INITIALIZED_EXT,
                    VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                    ctx.qfIndex,
                    ctx.qfIndex,
                    *image,
                    fullSRR,
                };
                cmdPipelineImageMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                              VK_PIPELINE_STAGE_TRANSFER_BIT, &barrier);
            }
            {
                const auto copySRL = makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, viewLevel, 0u, 1u);
                const auto region  = makeBufferImageCopy(mipExtentVk, copySRL);
                ctx.vkd.cmdCopyImageToBuffer(cmdBuffer, *image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, *buffer, 1u,
                                             &region);
            }
            {
                const auto barrier = makeMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
                cmdPipelineMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                                         &barrier);
            }
            endCommandBuffer(ctx.vkd, cmdBuffer);
            submitCommandsAndWait(ctx.vkd, ctx.device, ctx.queue, cmdBuffer);
        }
        else
        {
            VkBorderColor borderColor;

            if (isInt || isUint)
                borderColor = VK_BORDER_COLOR_INT_TRANSPARENT_BLACK;
            else
                borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;

            const VkSamplerCreateInfo samplerCreateInfo = {
                VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
                nullptr,
                0u,
                VK_FILTER_NEAREST,
                VK_FILTER_NEAREST,
                VK_SAMPLER_MIPMAP_MODE_NEAREST,
                VK_SAMPLER_ADDRESS_MODE_REPEAT,
                VK_SAMPLER_ADDRESS_MODE_REPEAT,
                VK_SAMPLER_ADDRESS_MODE_REPEAT,
                0.0f,
                VK_FALSE,
                0.0f,
                VK_FALSE,
                VK_COMPARE_OP_NEVER,
                0.0f,
                0.0f,
                borderColor,
                VK_FALSE,
            };
            const auto sampler = createSampler(ctx.vkd, ctx.device, &samplerCreateInfo);

            DescriptorPoolBuilder poolBuilder;

            VkDescriptorType imgDescType = VK_DESCRIPTOR_TYPE_MAX_ENUM;
            VkImageLayout finalLayout    = VK_IMAGE_LAYOUT_UNDEFINED;
            VkSampler descriptorSampler  = VK_NULL_HANDLE;

            const VkDescriptorType bufferDescType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;

            if (m_params.mainUsage == VK_IMAGE_USAGE_STORAGE_BIT)
            {
                imgDescType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
                finalLayout = VK_IMAGE_LAYOUT_GENERAL;
            }
            else if (m_params.mainUsage == VK_IMAGE_USAGE_SAMPLED_BIT)
            {
                imgDescType       = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                finalLayout       = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                descriptorSampler = *sampler;
            }
            else
                DE_ASSERT(false);

            poolBuilder.addType(imgDescType);
            poolBuilder.addType(bufferDescType);

            const auto descriptorPool =
                poolBuilder.build(ctx.vkd, ctx.device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);

            DescriptorSetLayoutBuilder setLayoutBuilder;
            setLayoutBuilder.addSingleBinding(imgDescType, m_params.readStage);
            setLayoutBuilder.addSingleBinding(bufferDescType, m_params.readStage);
            const auto setLayout = setLayoutBuilder.build(ctx.vkd, ctx.device);
            PipelineLayoutWrapper pipelineLayout(constructionType, ctx.vkd, ctx.device, *setLayout);

            const auto descriptorSet = makeDescriptorSet(ctx.vkd, ctx.device, *descriptorPool, *setLayout);

            DescriptorSetUpdateBuilder updateBuilder;
            const auto binding        = DescriptorSetUpdateBuilder::Location::binding;
            const auto imgDescInfo    = makeDescriptorImageInfo(descriptorSampler, *imageView, finalLayout);
            const auto bufferDescInfo = makeDescriptorBufferInfo(*buffer, 0ull, VK_WHOLE_SIZE);
            updateBuilder.writeSingle(*descriptorSet, binding(0u), imgDescType, &imgDescInfo);
            updateBuilder.writeSingle(*descriptorSet, binding(1u), bufferDescType, &bufferDescInfo);
            updateBuilder.update(ctx.vkd, ctx.device);

            using GraphicsPipelineWrapperPtr = std::unique_ptr<GraphicsPipelineWrapper>;
            using RenderPassWrapperPtr       = std::unique_ptr<RenderPassWrapper>;

            Move<VkPipeline> compPipeline;
            GraphicsPipelineWrapperPtr graphicsPipeline;
            RenderPassWrapperPtr renderPass;

            const std::vector<VkViewport> viewports(1u, makeViewport(mipExtentVk));
            const std::vector<VkRect2D> scissors(1u, makeRect2D(mipExtentVk));

            const auto &binaries = m_context.getBinaryCollection();

            if (m_params.readStage == VK_SHADER_STAGE_COMPUTE_BIT)
            {
                const auto compShader = createShaderModule(ctx.vkd, ctx.device, binaries.get("comp"));
                compPipeline          = makeComputePipeline(ctx.vkd, ctx.device, *pipelineLayout, *compShader);
            }
            else if (m_params.readStage == VK_SHADER_STAGE_FRAGMENT_BIT)
            {
                graphicsPipeline.reset(new GraphicsPipelineWrapper(ctx.vki, ctx.vkd, ctx.physicalDevice, ctx.device,
                                                                   m_context.getDeviceExtensions(), constructionType));
                auto &pipeline = *graphicsPipeline;

                const VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo = initVulkanStructure();
                const VkPipelineColorBlendStateCreateInfo colorBlendState             = initVulkanStructure();

                renderPass.reset(new RenderPassWrapper(constructionType, ctx.vkd, ctx.device));
                renderPass->createFramebuffer(ctx.vkd, ctx.device, VK_NULL_HANDLE, VK_NULL_HANDLE, mipExtentVk.width,
                                              mipExtentVk.height);

                ShaderWrapper vertexShader(ctx.vkd, ctx.device, binaries.get("vert"));
                ShaderWrapper fragShader(ctx.vkd, ctx.device, binaries.get("frag"));

                pipeline.setDefaultTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
                    .setDefaultRasterizationState()
                    .setDefaultDepthStencilState()
                    .setDefaultMultisampleState()
                    .setupVertexInputState(&vertexInputStateCreateInfo)
                    .setupPreRasterizationShaderState(viewports, scissors, pipelineLayout, renderPass->get(), 0u,
                                                      vertexShader)
                    .setupFragmentShaderState(pipelineLayout, renderPass->get(), 0u, fragShader)
                    .setupFragmentOutputState(renderPass->get(), 0u, &colorBlendState)
                    .buildPipeline();
            }

            beginCommandBuffer(ctx.vkd, cmdBuffer);
            {
                const VkImageMemoryBarrier barrier = {
                    VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                    nullptr,
                    0u,
                    VK_ACCESS_SHADER_READ_BIT,
                    VK_IMAGE_LAYOUT_ZERO_INITIALIZED_EXT,
                    finalLayout,
                    ctx.qfIndex,
                    ctx.qfIndex,
                    *image,
                    fullSRR,
                };
                cmdPipelineImageMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                              VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, &barrier);
            }
            if (m_params.readStage == VK_SHADER_STAGE_COMPUTE_BIT)
            {
                ctx.vkd.cmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayout, 0u, 1u,
                                              &descriptorSet.get(), 0u, nullptr);
                ctx.vkd.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *compPipeline);
                ctx.vkd.cmdDispatch(cmdBuffer, static_cast<uint32_t>(m_params.mipExtent.y()), 1u, 1u);
            }
            else if (m_params.readStage == VK_SHADER_STAGE_FRAGMENT_BIT)
            {
                renderPass->begin(ctx.vkd, cmdBuffer, scissors.at(0u));
                ctx.vkd.cmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 0u, 1u,
                                              &descriptorSet.get(), 0u, nullptr);
                graphicsPipeline->bind(cmdBuffer);
                ctx.vkd.cmdDraw(cmdBuffer, 3u, 1u, 0u, 0u);
                renderPass->end(ctx.vkd, cmdBuffer);
            }
            else
                DE_ASSERT(false);
            {
                const auto srcAccess = (isTransfer ? VK_ACCESS_TRANSFER_WRITE_BIT : VK_ACCESS_SHADER_WRITE_BIT);
                const auto srcStage  = m_params.getReadPipelineStage();
                const auto barrier   = makeMemoryBarrier(srcAccess, VK_ACCESS_HOST_READ_BIT);
                cmdPipelineMemoryBarrier(ctx.vkd, cmdBuffer, srcStage, VK_PIPELINE_STAGE_HOST_BIT, &barrier);
            }
            endCommandBuffer(ctx.vkd, cmdBuffer);
            submitCommandsAndWait(ctx.vkd, ctx.device, ctx.queue, cmdBuffer);
        }

        invalidateAlloc(ctx.vkd, ctx.device, bufferAlloc);

        VkFormat bufferFormat = VK_FORMAT_UNDEFINED;

        if (isTransfer)
            bufferFormat = m_params.format;
        else
        {
            if (isInt)
                bufferFormat = VK_FORMAT_R32G32B32A32_SINT;
            else if (isUint)
                bufferFormat = VK_FORMAT_R32G32B32A32_UINT;
            else
                bufferFormat = VK_FORMAT_R32G32B32A32_SFLOAT;
        }

        const auto tcuBufferFormat = mapVkFormat(bufferFormat);
        tcu::ConstPixelBufferAccess bufferAccess(tcuBufferFormat, m_params.mipExtent, bufferAlloc.getHostPtr());
        tcu::TextureLevel refLevel(tcuBufferFormat, m_params.mipExtent.x(), m_params.mipExtent.y(),
                                   m_params.mipExtent.z());
        tcu::PixelBufferAccess reference = refLevel.getAccess();

        // This is not entirely correct but it's true for BC1. If we add more compressed format we'd need to tune this.
        const bool hasAlpha = (tcu::getNumUsedChannels(tcuFormat.order) > 3 && !isCompressed);

        if (isInt)
        {
            const tcu::IVec4 refColor(0, 0, 0, (hasAlpha ? 0 : 1));
            tcu::clear(reference, refColor);
        }
        else if (isUint)
        {
            const tcu::UVec4 refColor(0u, 0u, 0u, (hasAlpha ? 0u : 1u));
            tcu::clear(reference, refColor);
        }
        else
        {
            const tcu::Vec4 refColor(0.0f, 0.0f, 0.0f, (hasAlpha ? 0.0f : 1.0f));
            tcu::clear(reference, refColor);
        }

        bool bufferOK = false;

        const tcu::UVec4 intThreshold(0u, 0u, 0u, 0u);
        const tcu::Vec4 floatThreshold(0.0f, 0.0f, 0.0f, 0.0f);

        if (isInt || isUint)
            bufferOK = tcu::intThresholdCompare(log, "Result", "", reference, bufferAccess, intThreshold,
                                                tcu::COMPARE_LOG_ON_ERROR);
        else
            bufferOK = tcu::floatThresholdCompare(log, "Result", "", reference, bufferAccess, floatThreshold,
                                                  tcu::COMPARE_LOG_ON_ERROR);

        if (!bufferOK)
        {
            fail = true;
            std::ostringstream msg;
            msg << "Unexpected results in output buffer for memory type " << memTypeIdx;
            log << tcu::TestLog::Message << msg.str() << tcu::TestLog::EndMessage;
        }

        m_context.getTestContext().touchWatchdog();
    }

    if (fail)
        TCU_FAIL("Some memory types failed; check log for details --");

    return tcu::TestStatus::pass("Pass");
}

struct DepthFormatParams
{
    VkFormat format;
    tcu::IVec3 mipExtent;
    bool firstMip;

    VkImageType getImageType() const
    {
        return VK_IMAGE_TYPE_2D;
    }

    VkImageViewType getImageViewType() const
    {
        return VK_IMAGE_VIEW_TYPE_2D;
    }

    VkImageTiling getImageTiling() const
    {
        return VK_IMAGE_TILING_OPTIMAL;
    }

    VkImageCreateFlags getImageCreateFlags() const
    {
        return 0u;
    }

    VkImageUsageFlags getImageUsage() const
    {
        return static_cast<VkImageUsageFlags>(VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
    }

    uint32_t getMipLevelCount() const
    {
        return (firstMip ? 1u : 2u);
    }

    tcu::IVec3 getCreationExtent() const
    {
        return (firstMip ? mipExtent : tcu::IVec3(2u, 2u, 1u) * mipExtent);
    }

    tcu::Vec4 getClearColor() const
    {
        return tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f);
    }

    tcu::Vec4 getGeomColor() const
    {
        return tcu::Vec4(0.0f, 0.0f, 1.0f, 1.0f);
    }
};

class DepthFormatTest : public vkt::TestInstance
{
public:
    DepthFormatTest(Context &context, const DepthFormatParams &params) : vkt::TestInstance(context), m_params(params)
    {
    }
    virtual ~DepthFormatTest(void) = default;

    tcu::TestStatus iterate(void) override;

protected:
    const DepthFormatParams m_params;
};

class DepthFormatCase : public vkt::TestCase
{
public:
    DepthFormatCase(tcu::TestContext &testCtx, const std::string &name, const DepthFormatParams &params)
        : vkt::TestCase(testCtx, name)
        , m_params(params)
    {
    }
    virtual ~DepthFormatCase(void) = default;

    void checkSupport(Context &context) const override;
    void initPrograms(vk::SourceCollections &programCollection) const override;
    TestInstance *createInstance(Context &context) const override
    {
        return new DepthFormatTest(context, m_params);
    }

protected:
    const DepthFormatParams m_params;
};

void DepthFormatCase::checkSupport(Context &context) const
{
    checkZeroInitializeDeviceMemorySupport(context);

    const auto ctx = context.getContextCommonData();
    VkImageFormatProperties formatProperties;

    const auto result = ctx.vki.getPhysicalDeviceImageFormatProperties(
        ctx.physicalDevice, m_params.format, m_params.getImageType(), m_params.getImageTiling(),
        m_params.getImageUsage(), m_params.getImageCreateFlags(), &formatProperties);

    if (result != VK_SUCCESS)
    {
        if (result == VK_ERROR_FORMAT_NOT_SUPPORTED)
            TCU_THROW(NotSupportedError, "format not supported for the target usage");
        else
            TCU_FAIL(std::string("vkGetPhysicalDeviceImageFormatProperties returned ") + getResultName(result));
    }

    const auto creationExtent   = m_params.getCreationExtent();
    const auto creationExtentVk = makeExtent3D(creationExtent);

    if (creationExtentVk.width > formatProperties.maxExtent.width ||
        creationExtentVk.height > formatProperties.maxExtent.height ||
        creationExtentVk.depth > formatProperties.maxExtent.depth)
    {
        TCU_THROW(NotSupportedError, "Requested extent not supported");
    }

    const auto mipLevelCount = m_params.getMipLevelCount();
    if (mipLevelCount > formatProperties.maxMipLevels)
        TCU_THROW(NotSupportedError, "Requested mip level count not supported");
}

void DepthFormatCase::initPrograms(vk::SourceCollections &programCollection) const
{
    std::ostringstream vert;
    vert << "#version 460\n"
         << "vec2 positions[3] = vec2[](\n"
         << "    vec2(-1.0, -1.0),\n"
         << "    vec2( 3.0, -1.0),\n"
         << "    vec2(-1.0,  3.0)\n"
         << ");\n"
         << "void main (void) {\n"
         << "    gl_Position = vec4(positions[gl_VertexIndex % 3], 0.0, 1.0);\n"
         << "    gl_PointSize = 1.0;\n"
         << "}\n";
    programCollection.glslSources.add("vert") << glu::VertexSource(vert.str());

    std::ostringstream frag;
    frag << "#version 460\n"
         << "layout (location=0) out vec4 outColor;\n"
         << "void main(void) {\n"
         << "    outColor = vec4" << m_params.getGeomColor() << ";\n"
         << "}\n";
    programCollection.glslSources.add("frag") << glu::FragmentSource(frag.str());
}

tcu::TestStatus DepthFormatTest::iterate(void)
{
    const auto ctx              = m_context.getContextCommonData();
    const auto constructionType = PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC;

    const auto colorFormat = VK_FORMAT_R8G8B8A8_UNORM;
    const auto fbExtent    = makeExtent3D(m_params.mipExtent);
    const auto colorUsage =
        (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
    const auto colorSRR      = makeDefaultImageSubresourceRange();
    const auto mipLevelCount = m_params.getMipLevelCount();

    // Color attachment.
    ImageWithBuffer colorImg(ctx.vkd, ctx.device, ctx.allocator, fbExtent, colorFormat, colorUsage,
                             m_params.getImageType(), colorSRR);

    // Depth/stencil attachment.
    const VkImageCreateInfo dsCreateInfo = {
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        nullptr,
        m_params.getImageCreateFlags(),
        m_params.getImageType(),
        m_params.format,
        makeExtent3D(m_params.getCreationExtent()),
        mipLevelCount,
        1u,
        VK_SAMPLE_COUNT_1_BIT,
        VK_IMAGE_TILING_OPTIMAL,
        m_params.getImageUsage(),
        VK_SHARING_MODE_EXCLUSIVE,
        0u,
        nullptr,
        VK_IMAGE_LAYOUT_ZERO_INITIALIZED_EXT,
    };
    const auto templateImage = createImage(ctx.vkd, ctx.device, &dsCreateInfo);

    VkPhysicalDeviceMemoryProperties memProperties;
    ctx.vki.getPhysicalDeviceMemoryProperties(ctx.physicalDevice, &memProperties);

    const auto imageMemReqs = getImageMemoryRequirements(ctx.vkd, ctx.device, *templateImage);
    const auto testedMemTypes =
        getTestedMemoryTypes(memProperties, imageMemReqs.memoryTypeBits, MemoryRequirement::ZeroInitialize);

    const auto tcuDSFormat = mapVkFormat(m_params.format);
    const auto dsAspects   = getImageAspectFlags(tcuDSFormat);
    const auto dsFullSRR =
        makeImageSubresourceRange(dsAspects, 0u, dsCreateInfo.mipLevels, 0u, dsCreateInfo.arrayLayers);
    const auto viewLevel = mipLevelCount - 1u;
    const auto viewSRR   = makeImageSubresourceRange(dsAspects, viewLevel, 1u, 0u, 1u);

    auto &log = m_context.getTestContext().getLog();
    bool fail = false;

    for (uint32_t memTypeIdx = 0u; memTypeIdx < memProperties.memoryTypeCount; ++memTypeIdx)
    {
        const auto memTypeMask = (1u << memTypeIdx);
        if ((testedMemTypes & memTypeMask) == 0u)
            continue;

        const auto dsImg   = createImage(ctx.vkd, ctx.device, &dsCreateInfo);
        const auto dsAlloc = allocateZeroInitMemory(ctx.allocator, imageMemReqs, memTypeIdx);
        VK_CHECK(ctx.vkd.bindImageMemory(ctx.device, *dsImg, dsAlloc->getMemory(), dsAlloc->getOffset()));

        const auto dsView =
            makeImageView(ctx.vkd, ctx.device, dsImg.get(), m_params.getImageViewType(), m_params.format, viewSRR);

        CommandPoolWithBuffer cmd(ctx.vkd, ctx.device, ctx.qfIndex);
        const auto cmdBuffer = *cmd.cmdBuffer;

        PipelineLayoutWrapper pipelineLayout(constructionType, ctx.vkd, ctx.device);
        RenderPassWrapper renderPass(constructionType, ctx.vkd, ctx.device, colorFormat, m_params.format,
                                     VK_ATTACHMENT_LOAD_OP_LOAD);

        const std::vector<VkImage> fbImages{colorImg.getImage(), dsImg.get()};
        const std::vector<VkImageView> fbViews{colorImg.getImageView(), *dsView};

        renderPass.createFramebuffer(ctx.vkd, ctx.device, de::sizeU32(fbImages), de::dataOrNull(fbImages),
                                     de::dataOrNull(fbViews), fbExtent.width, fbExtent.height);

        const auto &binaries = m_context.getBinaryCollection();
        ShaderWrapper vertShader(ctx.vkd, ctx.device, binaries.get("vert"));
        ShaderWrapper fragShader(ctx.vkd, ctx.device, binaries.get("frag"));

        const VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo = initVulkanStructure();

        const auto depthEnable   = ((dsAspects & VK_IMAGE_ASPECT_DEPTH_BIT) ? VK_TRUE : VK_FALSE);
        const auto stencilEnable = ((dsAspects & VK_IMAGE_ASPECT_STENCIL_BIT) ? VK_TRUE : VK_FALSE);
        const auto stencilOp     = makeStencilOpState(VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP,
                                                      VK_COMPARE_OP_EQUAL, 0xFFu, 0xFFu, 0u);

        const VkPipelineDepthStencilStateCreateInfo depthStencilStateCreateInfo = {
            VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
            nullptr,
            0u,
            depthEnable,
            VK_FALSE,
            VK_COMPARE_OP_EQUAL,
            VK_FALSE,
            stencilEnable,
            stencilOp,
            stencilOp,
            0.0f,
            1.0f,
        };

        const std::vector<VkViewport> viewports(1u, makeViewport(fbExtent));
        const std::vector<VkRect2D> scissors(1u, makeRect2D(fbExtent));

        GraphicsPipelineWrapper pipeline(ctx.vki, ctx.vkd, ctx.physicalDevice, ctx.device,
                                         m_context.getDeviceExtensions(), constructionType);
        pipeline.setDefaultTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
            .setDefaultRasterizationState()
            .setDefaultDepthStencilState()
            .setDefaultMultisampleState()
            .setDefaultColorBlendState()
            .setupVertexInputState(&vertexInputStateCreateInfo)
            .setupPreRasterizationShaderState(viewports, scissors, pipelineLayout, renderPass.get(), 0u, vertShader)
            .setupFragmentShaderState(pipelineLayout, renderPass.get(), 0u, fragShader, &depthStencilStateCreateInfo)
            .setupFragmentOutputState(renderPass.get(), 0u)
            .buildPipeline();

        beginCommandBuffer(ctx.vkd, cmdBuffer);
        {
            const auto dstAccess =
                (VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT);
            const auto dstStages =
                (VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT);

            const VkImageMemoryBarrier barrier = {
                VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                nullptr,
                0u,
                dstAccess,
                VK_IMAGE_LAYOUT_ZERO_INITIALIZED_EXT,
                VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                ctx.qfIndex,
                ctx.qfIndex,
                *dsImg,
                dsFullSRR,
            };
            cmdPipelineImageMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, dstStages, &barrier);

            // Clear color image.
            const VkImageMemoryBarrier preClearBarrier = {
                VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                nullptr,
                0u,
                VK_ACCESS_TRANSFER_WRITE_BIT,
                VK_IMAGE_LAYOUT_UNDEFINED,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                ctx.qfIndex,
                ctx.qfIndex,
                colorImg.getImage(),
                colorSRR,
            };
            cmdPipelineImageMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                          VK_PIPELINE_STAGE_TRANSFER_BIT, &preClearBarrier);

            const auto clearColor = makeClearValueColorVec4(m_params.getClearColor());
            ctx.vkd.cmdClearColorImage(cmdBuffer, colorImg.getImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                       &clearColor.color, 1u, &colorSRR);

            const VkImageMemoryBarrier postClearBarrier = {
                VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                nullptr,
                VK_ACCESS_TRANSFER_WRITE_BIT,
                (VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT),
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                ctx.qfIndex,
                ctx.qfIndex,
                colorImg.getImage(),
                colorSRR,
            };
            cmdPipelineImageMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                          VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, &postClearBarrier);
        }
        renderPass.begin(ctx.vkd, cmdBuffer, scissors.at(0u));
        pipeline.bind(cmdBuffer);
        ctx.vkd.cmdDraw(cmdBuffer, 3u, 1u, 0u, 0u);
        renderPass.end(ctx.vkd, cmdBuffer);

        copyImageToBuffer(ctx.vkd, cmdBuffer, colorImg.getImage(), colorImg.getBuffer(),
                          m_params.mipExtent.swizzle(0, 1));

        endCommandBuffer(ctx.vkd, cmdBuffer);
        submitCommandsAndWait(ctx.vkd, ctx.device, ctx.queue, cmdBuffer);

        invalidateAlloc(ctx.vkd, ctx.device, colorImg.getBufferAllocation());

        const auto tcuColorFormat = mapVkFormat(colorFormat);
        tcu::TextureLevel refLevel(tcuColorFormat, m_params.mipExtent.x(), m_params.mipExtent.y());
        tcu::PixelBufferAccess reference = refLevel.getAccess();
        tcu::clear(reference, m_params.getGeomColor());

        tcu::ConstPixelBufferAccess result(tcuColorFormat, m_params.mipExtent,
                                           colorImg.getBufferAllocation().getHostPtr());

        const tcu::Vec4 threshold(0.0f, 0.0f, 0.0f, 0.0f);

        if (!tcu::floatThresholdCompare(log, "Result", "", reference, result, threshold, tcu::COMPARE_LOG_ON_ERROR))
        {
            fail = true;
            std::ostringstream msg;
            msg << "Unexpected results in color buffer for memory type " << memTypeIdx;
            log << tcu::TestLog::Message << msg.str() << tcu::TestLog::EndMessage;
        }

        m_context.getTestContext().touchWatchdog();
    }

    if (fail)
        TCU_FAIL("Some memory types failed; check log for details --");

    return tcu::TestStatus::pass("Pass");
}

// TEST MECHANISM for the color_attachment_layouts tests:
//
// Images created with VK_IMAGE_LAYOUT_ZERO_INITIALIZED_EXT and bound to zero-initialized memory start with all-zero
// contents. Implementations using color compression may skip initializing their compression metadata when the image
// leaves that layout, because zeroed metadata may already mean "cleared to zero". Any other state the implementation
// tracks for the image must still be initialized as if the image had been cleared. If it is not, a later transition
// to a layout where the image is kept uncompressed may skip the decompression it needs, writes in that layout go to
// uncompressed data while the metadata still says "cleared to zero", and reads after going back to a compressed
// layout return zeros instead of the written data.
//
// Steps:
// - Create a large color image (kExtent) with COLOR_ATTACHMENT usage, initial layout ZERO_INITIALIZED_EXT, and bind it
//   to zero-initialized memory. Repeat the test for every memory type that can be zero-initialized.
// - Transition ZERO_INITIALIZED_EXT -> COLOR_ATTACHMENT_OPTIMAL, with no draws or clears.
// - Transition to the write layout and write a pattern to the whole image (except in partial draws, see below):
//     exclusive_transfer_dst:  exclusive image, vkCmdCopyBufferToImage in TRANSFER_DST_OPTIMAL on the universal queue.
//     exclusive_general:       same, but in GENERAL.
//     concurrent_transfer_dst: concurrent image (universal + compute queue families), vkCmdCopyBufferToImage in
//                              TRANSFER_DST_OPTIMAL on the universal queue.
//     concurrent_general:      same, but in GENERAL.
//     concurrent_transfer_dst_compute_write:
//                              same as concurrent_transfer_dst, but the transition to TRANSFER_DST_OPTIMAL, the copy
//                              and the transition to the read layout happen on the compute queue.
//     qfot_compute:            exclusive image, release to the compute queue family while transitioning to
//                              TRANSFER_DST_OPTIMAL, copy there, release back to the universal queue family.
//     feedback_loop:           draw the pattern in ATTACHMENT_FEEDBACK_LOOP_OPTIMAL_EXT.
//     partial_draw:            draw the pattern in COLOR_ATTACHMENT_OPTIMAL with a scissor covering only the left half
//                              of the image (rounded up). The right half must still be zero when read back.
//     concurrent_draw:         concurrent image (universal + compute queue families), draw the pattern in
//                              COLOR_ATTACHMENT_OPTIMAL and read it back in GENERAL (see below).
// - Transition to the read layout and read the image back on the universal queue:
//     read_copy:   vkCmdCopyImageToBuffer from TRANSFER_SRC_OPTIMAL.
//     read_sample: texelFetch from SHADER_READ_ONLY_OPTIMAL in a compute shader, packing each texel into a storage
//                  buffer with the same memory layout as the image format.
//     read_general_copy, read_general_sample: same, but reading in GENERAL.
//
// concurrent_draw is general coverage rather than a check for the issue above: when the image is transitioned to
// GENERAL, a layout implementations may keep uncompressed for concurrent images, it contains compressed data written by
// the draw instead of untouched zero-initialized contents. Drawing is expected to make implementations track the image
// as compressed, so the decompression should not be skipped even if the zero-initialized state is mishandled.
//
// mip_levels_partial_draw_read_copy covers a related issue with mipmapped images, where some levels may need special
// metadata initialization when leaving ZERO_INITIALIZED_EXT. The image has a full mip chain, and each level is drawn
// to as in partial_draw. Every level is copied back and the right half must still be zero.
//
// Pattern, the same in copies and draws (x, y: texel coordinates; level: mip level):
//     R8G8B8A8_UNORM:     (x & 0xFF, y & 0xFF, ((x >> 8) & 0xF) | (((y >> 8) & 0xF) << 4), 0xFF - level) / 255
//     R16G16B16A16_UINT:  (x, y, x ^ y, 0xFFFF - level)
//   The alpha channel is never zero, so no texel in the pattern is zero. For example, texel (300, 2) in level 0 is
//   (44, 2, 1, 255) in R8G8B8A8_UNORM, and (300, 2, 302, 65535) in R16G16B16A16_UINT.
//
// Verification: the readback buffer is compared byte by byte with the expected data. Only on mismatch the images are
// compared with tcu::intThresholdCompare, which decides the result and logs the images along with an error mask. The
// threshold is 0 except for draws to R8G8B8A8_UNORM images, where it is 1: the fragment shader output k / 255 may not
// be exactly k after multiplying by 255 (e.g. if the division is implemented with a reciprocal), and the conversion to
// UNORM may then return either of the two closest integers. The tolerance does not hide the zeros caused by a bug,
// because the alpha channel of the pattern is never close to zero.
//
// Variants: format (R8G8B8A8_UNORM, R16G16B16A16_UINT, since compression may depend on the texel size), write method
// and read method as described above. Cases using the compute queue family (concurrent_*, qfot_compute) are not
// supported without a compute-only queue family different from the universal one. The exclusive_* cases cover the same
// copies on implementations with a single queue family, even if they may keep the image compressed in those layouts.
namespace ColorLayouts
{

constexpr VkExtent3D kExtent{1024u, 512u, 1u}; // Large enough for implementations to enable color compression.
constexpr uint32_t kLocalSize = 64u;           // Compute readback workgroup size.

static_assert(kExtent.width % kLocalSize == 0u, "The compute readback dispatch requires full workgroups");

enum class WriteMethod
{
    EXCLUSIVE_TRANSFER_DST = 0, // Copy into TRANSFER_DST_OPTIMAL on the universal queue, exclusive image.
    EXCLUSIVE_GENERAL,          // Copy into GENERAL on the universal queue, exclusive image.
    CONCURRENT_TRANSFER_DST,    // Copy into TRANSFER_DST_OPTIMAL on the universal queue, concurrent image.
    CONCURRENT_GENERAL,         // Copy into GENERAL on the universal queue, concurrent image.
    CONCURRENT_COMPUTE_WRITE,   // Copy into TRANSFER_DST_OPTIMAL on the compute queue, concurrent image.
    QFOT_COMPUTE,               // Copy into TRANSFER_DST_OPTIMAL on the compute queue, exclusive image with QFOTs.
    FEEDBACK_LOOP,              // Draw in ATTACHMENT_FEEDBACK_LOOP_OPTIMAL_EXT on the universal queue.
    PARTIAL_DRAW,               // Partial draw in COLOR_ATTACHMENT_OPTIMAL.
    CONCURRENT_DRAW,            // Draw in COLOR_ATTACHMENT_OPTIMAL and read in GENERAL, concurrent image.
    MIP_LEVELS_DRAW,            // Partial draws to every mip level in COLOR_ATTACHMENT_OPTIMAL.
};

enum class ReadMethod
{
    COPY = 0, // vkCmdCopyImageToBuffer from TRANSFER_SRC_OPTIMAL (GENERAL with concurrent_draw).
    SAMPLE,   // texelFetch in a compute shader from SHADER_READ_ONLY_OPTIMAL (GENERAL with concurrent_draw).
};

struct Params
{
    VkFormat format;
    WriteMethod writeMethod;
    ReadMethod readMethod;

    bool isConcurrent() const
    {
        return (writeMethod == WriteMethod::CONCURRENT_TRANSFER_DST || writeMethod == WriteMethod::CONCURRENT_GENERAL ||
                writeMethod == WriteMethod::CONCURRENT_COMPUTE_WRITE || writeMethod == WriteMethod::CONCURRENT_DRAW);
    }

    // Cases where the image is written on the compute queue instead of the universal queue.
    bool isComputeQueueWrite() const
    {
        return (writeMethod == WriteMethod::CONCURRENT_COMPUTE_WRITE || writeMethod == WriteMethod::QFOT_COMPUTE);
    }

    bool needsComputeQueue() const
    {
        return (isConcurrent() || writeMethod == WriteMethod::QFOT_COMPUTE);
    }

    bool isDrawWrite() const
    {
        return (writeMethod == WriteMethod::FEEDBACK_LOOP || writeMethod == WriteMethod::PARTIAL_DRAW ||
                writeMethod == WriteMethod::MIP_LEVELS_DRAW || writeMethod == WriteMethod::CONCURRENT_DRAW);
    }

    // The rest of the write methods use copies.
    bool isCopyWrite() const
    {
        return !isDrawWrite();
    }

    // Draws covering only the left half of each mip level.
    bool isPartialDraw() const
    {
        return (writeMethod == WriteMethod::PARTIAL_DRAW || writeMethod == WriteMethod::MIP_LEVELS_DRAW);
    }

    bool isUint() const
    {
        DE_ASSERT(format == VK_FORMAT_R8G8B8A8_UNORM || format == VK_FORMAT_R16G16B16A16_UINT);
        return (format == VK_FORMAT_R16G16B16A16_UINT);
    }

    uint32_t getMipLevelCount() const
    {
        if (writeMethod != WriteMethod::MIP_LEVELS_DRAW)
            return 1u;

        const auto maxSize = static_cast<int32_t>(std::max(kExtent.width, kExtent.height));
        return static_cast<uint32_t>(deLog2Floor32(maxSize)) + 1u;
    }

    VkImageUsageFlags getImageUsage() const
    {
        // Note the write usages: images that are only read may be kept compressed in any layout.
        VkImageUsageFlags usage = (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                                   VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
        if (writeMethod == WriteMethod::FEEDBACK_LOOP)
            usage |= VK_IMAGE_USAGE_ATTACHMENT_FEEDBACK_LOOP_BIT_EXT;
        return usage;
    }

    VkImageLayout getWriteLayout() const
    {
        switch (writeMethod)
        {
        case WriteMethod::EXCLUSIVE_TRANSFER_DST:
        case WriteMethod::CONCURRENT_TRANSFER_DST:
        case WriteMethod::CONCURRENT_COMPUTE_WRITE:
        case WriteMethod::QFOT_COMPUTE:
            return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        case WriteMethod::EXCLUSIVE_GENERAL:
        case WriteMethod::CONCURRENT_GENERAL:
            return VK_IMAGE_LAYOUT_GENERAL;
        case WriteMethod::FEEDBACK_LOOP:
            return VK_IMAGE_LAYOUT_ATTACHMENT_FEEDBACK_LOOP_OPTIMAL_EXT;
        case WriteMethod::PARTIAL_DRAW:
        case WriteMethod::MIP_LEVELS_DRAW:
        case WriteMethod::CONCURRENT_DRAW:
            return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        default:
            break;
        }

        DE_ASSERT(false);
        return VK_IMAGE_LAYOUT_UNDEFINED;
    }

    VkImageLayout getReadLayout() const
    {
        if (writeMethod == WriteMethod::CONCURRENT_DRAW)
            return VK_IMAGE_LAYOUT_GENERAL;

        return ((readMethod == ReadMethod::COPY) ? VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL :
                                                   VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }

    // The queue family list is only used with concurrent images, and it must outlive the returned structure.
    std::unique_ptr<VkImageCreateInfo> getImageCreateInfo(const std::vector<uint32_t> &concurrentQueueFamilies) const
    {
        const bool concurrent = isConcurrent();
        DE_ASSERT(!concurrent || concurrentQueueFamilies.size() > 1u);

        return std::make_unique<VkImageCreateInfo>(VkImageCreateInfo{
            VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            nullptr,
            0u,
            VK_IMAGE_TYPE_2D,
            format,
            kExtent,
            getMipLevelCount(),
            1u,
            VK_SAMPLE_COUNT_1_BIT,
            VK_IMAGE_TILING_OPTIMAL,
            getImageUsage(),
            (concurrent ? VK_SHARING_MODE_CONCURRENT : VK_SHARING_MODE_EXCLUSIVE),
            (concurrent ? de::sizeU32(concurrentQueueFamilies) : 0u),
            (concurrent ? de::dataOrNull(concurrentQueueFamilies) : nullptr),
            VK_IMAGE_LAYOUT_ZERO_INITIALIZED_EXT,
        });
    }
};

using ParamsPtr = std::shared_ptr<const Params>;

// Queue families sharing concurrent images, empty for exclusive images. The compute queue family must be supported.
std::vector<uint32_t> getConcurrentQueueFamilies(Context &context, const Params &params)
{
    if (!params.isConcurrent())
        return std::vector<uint32_t>();

    const auto computeQF = context.getComputeQueueFamilyIndex();
    DE_ASSERT(computeQF >= 0);
    return std::vector<uint32_t>{context.getUniversalQueueFamilyIndex(), static_cast<uint32_t>(computeQF)};
}

// Number of columns drawn to (or written) in each level. The rest of the level must remain zero.
uint32_t getCoveredWidth(const Params &params, uint32_t level)
{
    const auto width = mipLevelExtents(kExtent, level).width;
    return (params.isPartialDraw() ? (width + 1u) / 2u : width);
}

// Writes the expected pattern for the given pixel. Must match the fragment shader in initPrograms. Every pixel has a
// non-zero alpha value, so no pixel in the pattern is zero.
void writePatternPixel(uint8_t *dst, bool isUint, uint32_t x, uint32_t y, uint32_t level)
{
    if (isUint)
    {
        const uint16_t pixel[4] = {
            static_cast<uint16_t>(x),
            static_cast<uint16_t>(y),
            static_cast<uint16_t>(x ^ y),
            static_cast<uint16_t>(0xFFFFu - level),
        };
        std::memcpy(dst, pixel, sizeof(pixel));
    }
    else
    {
        const uint8_t pixel[4] = {
            static_cast<uint8_t>(x & 0xFFu),
            static_cast<uint8_t>(y & 0xFFu),
            static_cast<uint8_t>(((x >> 8) & 0xFu) | (((y >> 8) & 0xFu) << 4)),
            static_cast<uint8_t>(0xFFu - level),
        };
        std::memcpy(dst, pixel, sizeof(pixel));
    }
}

void checkSupport(Context &context, ParamsPtr params)
{
    checkZeroInitializeDeviceMemorySupport(context);

    if (params->needsComputeQueue())
    {
        const auto computeQF = context.getComputeQueueFamilyIndex();
        if (computeQF < 0)
            TCU_THROW(NotSupportedError, "No compute-only queue family available");
        if (static_cast<uint32_t>(computeQF) == context.getUniversalQueueFamilyIndex())
            TCU_THROW(NotSupportedError, "Compute queue family must differ from the universal queue family");
    }

    const auto ctx                     = context.getContextCommonData();
    const auto concurrentQueueFamilies = getConcurrentQueueFamilies(context, *params);
    const auto imageCreateInfo         = params->getImageCreateInfo(concurrentQueueFamilies);
    VkImageFormatProperties formatProperties;

    const auto result = ctx.vki.getPhysicalDeviceImageFormatProperties(
        ctx.physicalDevice, imageCreateInfo->format, imageCreateInfo->imageType, imageCreateInfo->tiling,
        imageCreateInfo->usage, imageCreateInfo->flags, &formatProperties);

    if (result == VK_ERROR_FORMAT_NOT_SUPPORTED)
        TCU_THROW(NotSupportedError, "Format not supported for the required usage");
    VK_CHECK(result);

    const auto &extent = imageCreateInfo->extent;
    if (extent.width > formatProperties.maxExtent.width || extent.height > formatProperties.maxExtent.height ||
        extent.depth > formatProperties.maxExtent.depth)
        TCU_THROW(NotSupportedError, "Image extent not supported");

    if (imageCreateInfo->mipLevels > formatProperties.maxMipLevels)
        TCU_THROW(NotSupportedError, "Mip level count not supported");

    if (imageCreateInfo->arrayLayers > formatProperties.maxArrayLayers)
        TCU_THROW(NotSupportedError, "Array layer count not supported");

    if ((formatProperties.sampleCounts & imageCreateInfo->samples) == 0u)
        TCU_THROW(NotSupportedError, "Sample count not supported");

    if (params->writeMethod == WriteMethod::FEEDBACK_LOOP)
    {
        context.requireDeviceFunctionality("VK_EXT_attachment_feedback_loop_layout");
        if (!context.getAttachmentFeedbackLoopLayoutFeaturesEXT().attachmentFeedbackLoopLayout)
            TCU_THROW(NotSupportedError, "attachmentFeedbackLoopLayout not supported");
    }
}

void initPrograms(SourceCollections &programCollection, ParamsPtr params)
{
    const bool isUint = params->isUint();

    if (!params->isCopyWrite())
    {
        std::ostringstream vert;
        vert << "#version 460\n"
             << "vec2 positions[3] = vec2[](\n"
             << "    vec2(-1.0, -1.0),\n"
             << "    vec2( 3.0, -1.0),\n"
             << "    vec2(-1.0,  3.0)\n"
             << ");\n"
             << "void main (void) {\n"
             << "    gl_Position = vec4(positions[gl_VertexIndex % 3], 0.0, 1.0);\n"
             << "}\n";
        programCollection.glslSources.add("vert") << glu::VertexSource(vert.str());

        // Must match writePatternPixel.
        std::ostringstream frag;
        frag << "#version 460\n"
             << "layout (push_constant, std430) uniform PCBlock { uint level; } pc;\n"
             << "layout (location=0) out " << (isUint ? "uvec4" : "vec4") << " outColor;\n"
             << "void main (void) {\n"
             << "    const uvec2 c = uvec2(gl_FragCoord.xy);\n";
        if (isUint)
            frag << "    outColor = uvec4(c.x, c.y, c.x ^ c.y, 0xFFFFu - pc.level);\n";
        else
            frag << "    const uvec4 v = uvec4(c.x & 0xFFu, c.y & 0xFFu, ((c.x >> 8) & 0xFu) | (((c.y >> 8) & 0xFu) << "
                    "4), 0xFFu - pc.level);\n"
                 << "    outColor = vec4(v) / 255.0;\n";
        frag << "}\n";
        programCollection.glslSources.add("frag") << glu::FragmentSource(frag.str());
    }

    if (params->readMethod == ReadMethod::SAMPLE)
    {
        // Stores texels in the buffer with the same memory layout as the image format, so results can be compared
        // byte by byte with the expected data. The conversions are exact for both formats.
        std::ostringstream comp;
        comp << "#version 460\n"
             << "layout (local_size_x=" << kLocalSize << ", local_size_y=1, local_size_z=1) in;\n"
             << "layout (set=0, binding=0) uniform " << (isUint ? "usampler2D" : "sampler2D") << " tex;\n"
             << "layout (set=0, binding=1, std430) buffer OutBlock { uint data[]; } ssbo;\n"
             << "void main (void) {\n"
             << "    const uint width = " << kExtent.width << ";\n"
             << "    const uvec2 c = gl_GlobalInvocationID.xy;\n"
             << "    const uint idx = c.y * width + c.x;\n";
        if (isUint)
            comp << "    const uvec4 t = texelFetch(tex, ivec2(c), 0);\n"
                 << "    ssbo.data[idx * 2u + 0u] = (t.x & 0xFFFFu) | (t.y << 16);\n"
                 << "    ssbo.data[idx * 2u + 1u] = (t.z & 0xFFFFu) | (t.w << 16);\n";
        else
            comp << "    ssbo.data[idx] = packUnorm4x8(texelFetch(tex, ivec2(c), 0));\n";
        comp << "}\n";
        programCollection.glslSources.add("comp") << glu::ComputeSource(comp.str());
    }
}

// Render pass that loads and stores the attachment, keeping it in the same layout all the time.
Move<VkRenderPass> makeLoadStoreRenderPass(const DeviceInterface &vkd, VkDevice device, VkFormat format,
                                           VkImageLayout layout)
{
    const VkAttachmentDescription attachment = {
        0u,
        format,
        VK_SAMPLE_COUNT_1_BIT,
        VK_ATTACHMENT_LOAD_OP_LOAD,
        VK_ATTACHMENT_STORE_OP_STORE,
        VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        VK_ATTACHMENT_STORE_OP_DONT_CARE,
        layout,
        layout,
    };

    const VkAttachmentReference colorRef = {0u, layout};

    const VkSubpassDescription subpass = {
        0u, VK_PIPELINE_BIND_POINT_GRAPHICS, 0u, nullptr, 1u, &colorRef, nullptr, nullptr, 0u, nullptr,
    };

    const VkRenderPassCreateInfo createInfo = {
        VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO, nullptr, 0u, 1u, &attachment, 1u, &subpass, 0u, nullptr,
    };

    return createRenderPass(vkd, device, &createInfo);
}

tcu::TestStatus iterate(Context &context, ParamsPtr params)
{
    const auto ctx           = context.getContextCommonData();
    const auto tcuFormat     = mapVkFormat(params->format);
    const auto pixelSize     = static_cast<uint32_t>(tcu::getPixelSize(tcuFormat));
    const bool isUint        = params->isUint();
    const auto mipLevelCount = params->getMipLevelCount();
    const auto writeLayout   = params->getWriteLayout();
    const auto readLayout    = params->getReadLayout();
    const bool isCopyWrite   = params->isCopyWrite();
    const bool isQfot        = (params->writeMethod == WriteMethod::QFOT_COMPUTE);
    const bool computeWrite  = params->isComputeQueueWrite();
    const bool isFeedback    = (params->writeMethod == WriteMethod::FEEDBACK_LOOP);
    const bool sampleRead    = (params->readMethod == ReadMethod::SAMPLE);
    const auto colorSRR      = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, mipLevelCount, 0u, 1u);
    const auto computeQF =
        (params->needsComputeQueue() ? static_cast<uint32_t>(context.getComputeQueueFamilyIndex()) : ctx.qfIndex);
    const auto concurrentQueueFamilies = getConcurrentQueueFamilies(context, *params);
    const auto imageCreateInfo         = params->getImageCreateInfo(concurrentQueueFamilies);
    auto &log                          = context.getTestContext().getLog();

    DE_ASSERT(!(sampleRead && mipLevelCount > 1u)); // Only level 0 is read in the sampled case.

    // Stages and accesses for the write and read operations.
    const VkPipelineStageFlags writeStage =
        (isCopyWrite ? VK_PIPELINE_STAGE_TRANSFER_BIT : VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
    const VkAccessFlags writeAccess =
        (isCopyWrite ?
             static_cast<VkAccessFlags>(VK_ACCESS_TRANSFER_WRITE_BIT) :
             static_cast<VkAccessFlags>(VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT));
    const VkPipelineStageFlags readStage =
        (sampleRead ? VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT : VK_PIPELINE_STAGE_TRANSFER_BIT);
    const VkAccessFlags readAccess = (sampleRead ? VK_ACCESS_SHADER_READ_BIT : VK_ACCESS_TRANSFER_READ_BIT);

    // Expected contents of each level, tightly packed one after the other. Level 0 is also the copy source.
    std::vector<VkDeviceSize> levelOffsets;
    VkDeviceSize totalSize = 0ull;
    for (uint32_t level = 0u; level < mipLevelCount; ++level)
    {
        const auto levelExtent = mipLevelExtents(kExtent, level);
        levelOffsets.push_back(totalSize);
        totalSize += static_cast<VkDeviceSize>(levelExtent.width * levelExtent.height) * pixelSize;
    }

    std::vector<uint8_t> expected(static_cast<size_t>(totalSize), uint8_t{0});
    for (uint32_t level = 0u; level < mipLevelCount; ++level)
    {
        const auto levelExtent  = mipLevelExtents(kExtent, level);
        const auto coveredWidth = getCoveredWidth(*params, level);
        auto levelData          = expected.data() + levelOffsets.at(level);

        for (uint32_t y = 0u; y < levelExtent.height; ++y)
            for (uint32_t x = 0u; x < coveredWidth; ++x)
            {
                const auto pixelOffset = static_cast<size_t>(y * levelExtent.width + x) * pixelSize;
                writePatternPixel(levelData + pixelOffset, isUint, x, y, level);
            }
    }

    const auto level0Size   = static_cast<VkDeviceSize>(kExtent.width * kExtent.height) * pixelSize;
    const auto level0Layers = makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u);

    // Upload buffer for copy-based writes.
    using BufferPtr = std::unique_ptr<BufferWithMemory>;
    BufferPtr uploadBuffer;
    if (isCopyWrite)
    {
        const auto createInfo = makeBufferCreateInfo(level0Size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
        uploadBuffer =
            std::make_unique<BufferWithMemory>(ctx.vkd, ctx.device, ctx.allocator, createInfo, HostIntent::W);
        auto &alloc = uploadBuffer->getAllocation();
        std::memcpy(alloc.getHostPtr(), expected.data(), static_cast<size_t>(level0Size));
        flushAlloc(ctx.vkd, ctx.device, alloc);
    }

    // Readback buffer.
    const auto readbackSize  = (sampleRead ? level0Size : totalSize);
    const auto readbackUsage = (sampleRead ? VK_BUFFER_USAGE_STORAGE_BUFFER_BIT : VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    const auto readbackCreateInfo = makeBufferCreateInfo(readbackSize, readbackUsage);
    BufferWithMemory readbackBuffer(ctx.vkd, ctx.device, ctx.allocator, readbackCreateInfo, HostIntent::R);
    auto &readbackAlloc = readbackBuffer.getAllocation();

    // Objects for draw-based writes.
    Move<VkRenderPass> renderPass;
    Move<VkPipelineLayout> graphicsPipelineLayout;
    Move<VkPipeline> graphicsPipeline;

    if (!isCopyWrite)
    {
        const auto pcStages = VK_SHADER_STAGE_FRAGMENT_BIT;
        const auto pcSize   = DE_SIZEOF32(uint32_t);
        const auto pcRange  = makePushConstantRange(pcStages, 0u, pcSize);

        renderPass             = makeLoadStoreRenderPass(ctx.vkd, ctx.device, params->format, writeLayout);
        graphicsPipelineLayout = makePipelineLayout(ctx.vkd, ctx.device, VK_NULL_HANDLE, &pcRange);

        const auto &binaries  = context.getBinaryCollection();
        const auto vertShader = createShaderModule(ctx.vkd, ctx.device, binaries.get("vert"));
        const auto fragShader = createShaderModule(ctx.vkd, ctx.device, binaries.get("frag"));
        const VkPipelineVertexInputStateCreateInfo vertexInput = initVulkanStructure();

        // Empty viewport and scissor vectors: both will be dynamic (see the makeGraphicsPipeline implementation in
        // vkObjUtil.cpp for details).
        const std::vector<VkViewport> viewports;
        const std::vector<VkRect2D> scissors;

        const VkPipelineCreateFlags pipelineFlags =
            (isFeedback ?
                 static_cast<VkPipelineCreateFlags>(VK_PIPELINE_CREATE_COLOR_ATTACHMENT_FEEDBACK_LOOP_BIT_EXT) :
                 0u);

        graphicsPipeline = makeGraphicsPipeline(
            ctx.vkd, ctx.device, *graphicsPipelineLayout, *vertShader, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE,
            *fragShader, *renderPass, viewports, scissors, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0u, 0u, &vertexInput,
            nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, pipelineFlags);
    }

    // Objects for sampled reads.
    Move<VkSampler> sampler;
    Move<VkDescriptorSetLayout> setLayout;
    Move<VkPipelineLayout> computePipelineLayout;
    Move<VkPipeline> computePipeline;

    if (sampleRead)
    {
        const VkSamplerCreateInfo samplerCreateInfo = {
            VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
            nullptr,
            0u,
            VK_FILTER_NEAREST,
            VK_FILTER_NEAREST,
            VK_SAMPLER_MIPMAP_MODE_NEAREST,
            VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            0.0f,
            VK_FALSE,
            0.0f,
            VK_FALSE,
            VK_COMPARE_OP_NEVER,
            0.0f,
            0.0f,
            VK_BORDER_COLOR_INT_TRANSPARENT_BLACK,
            VK_FALSE,
        };
        sampler = createSampler(ctx.vkd, ctx.device, &samplerCreateInfo);

        const auto stage = VK_SHADER_STAGE_COMPUTE_BIT;
        DescriptorSetLayoutBuilder setLayoutBuilder;
        setLayoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, stage);
        setLayoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, stage);
        setLayout = setLayoutBuilder.build(ctx.vkd, ctx.device);

        computePipelineLayout = makePipelineLayout(ctx.vkd, ctx.device, *setLayout);
        const auto compShader = createShaderModule(ctx.vkd, ctx.device, context.getBinaryCollection().get("comp"));
        computePipeline       = makeComputePipeline(ctx.vkd, ctx.device, *computePipelineLayout, *compShader);
    }

    // Test every memory type that can be zero-initialized.
    const auto templateImage = createImage(ctx.vkd, ctx.device, imageCreateInfo.get());
    const auto imageMemReqs  = getImageMemoryRequirements(ctx.vkd, ctx.device, *templateImage);

    VkPhysicalDeviceMemoryProperties memProperties;
    ctx.vki.getPhysicalDeviceMemoryProperties(ctx.physicalDevice, &memProperties);

    const auto testedMemTypes =
        getTestedMemoryTypes(memProperties, imageMemReqs.memoryTypeBits, MemoryRequirement::ZeroInitialize);

    if (testedMemTypes == 0u)
        TCU_THROW(NotSupportedError, "No memory types can be used to zero-initialize the image");

    bool fail = false;

    for (uint32_t memTypeIdx = 0u; memTypeIdx < memProperties.memoryTypeCount; ++memTypeIdx)
    {
        if ((testedMemTypes & (1u << memTypeIdx)) == 0u)
            continue;

        const auto image = createImage(ctx.vkd, ctx.device, imageCreateInfo.get());
        const auto alloc = allocateZeroInitMemory(ctx.allocator, imageMemReqs, memTypeIdx);
        VK_CHECK(ctx.vkd.bindImageMemory(ctx.device, *image, alloc->getMemory(), alloc->getOffset()));

        // One view and framebuffer per mip level for draw-based writes. The level 0 view is also used for sampling.
        std::vector<Move<VkImageView>> levelViews;
        std::vector<Move<VkFramebuffer>> framebuffers;
        const bool needsViews = (!isCopyWrite || sampleRead);

        for (uint32_t level = 0u; needsViews && level < mipLevelCount; ++level)
        {
            const auto levelSRR = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, level, 1u, 0u, 1u);
            levelViews.push_back(
                makeImageView(ctx.vkd, ctx.device, *image, VK_IMAGE_VIEW_TYPE_2D, params->format, levelSRR));

            if (!isCopyWrite)
            {
                const auto levelExtent = mipLevelExtents(kExtent, level);
                framebuffers.push_back(makeFramebuffer(ctx.vkd, ctx.device, *renderPass, *levelViews.back(),
                                                       levelExtent.width, levelExtent.height));
            }
        }

        Move<VkDescriptorPool> descriptorPool;
        Move<VkDescriptorSet> descriptorSet;

        if (sampleRead)
        {
            DescriptorPoolBuilder poolBuilder;
            poolBuilder.addType(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
            poolBuilder.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
            descriptorPool =
                poolBuilder.build(ctx.vkd, ctx.device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);
            descriptorSet = makeDescriptorSet(ctx.vkd, ctx.device, *descriptorPool, *setLayout);

            using Location     = DescriptorSetUpdateBuilder::Location;
            const auto imgInfo = makeDescriptorImageInfo(*sampler, *levelViews.at(0u), readLayout);
            const auto bufInfo = makeDescriptorBufferInfo(readbackBuffer.get(), 0ull, readbackSize);

            DescriptorSetUpdateBuilder updateBuilder;
            updateBuilder.writeSingle(*descriptorSet, Location::binding(0u), VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                      &imgInfo);
            updateBuilder.writeSingle(*descriptorSet, Location::binding(1u), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                      &bufInfo);
            updateBuilder.update(ctx.vkd, ctx.device);
        }

        CommandPoolWithBuffer cmd(ctx.vkd, ctx.device, ctx.qfIndex);
        const auto cmdBuffer = *cmd.cmdBuffer;

        beginCommandBuffer(ctx.vkd, cmdBuffer);
        {
            // Transition to a compressed layout without writing anything to the image.
            const auto barrier = makeImageMemoryBarrier(
                0u, (VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT),
                VK_IMAGE_LAYOUT_ZERO_INITIALIZED_EXT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, *image, colorSRR);
            cmdPipelineImageMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                          VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, &barrier);
        }

        const auto recordCopyWrite = [&](VkCommandBuffer cmdBuf)
        {
            const auto region = makeBufferImageCopy(kExtent, level0Layers);
            ctx.vkd.cmdCopyBufferToImage(cmdBuf, uploadBuffer->get(), *image, writeLayout, 1u, &region);
        };

        if (computeWrite)
        {
            // Write on the compute queue and read the image back on the universal queue. Exclusive images need queue
            // family ownership transfers, while concurrent images only need the layout transitions.
            const auto universalQF = (isQfot ? ctx.qfIndex : VK_QUEUE_FAMILY_IGNORED);
            const auto writeQF     = (isQfot ? computeQF : VK_QUEUE_FAMILY_IGNORED);

            if (isQfot)
            {
                const auto release = makeImageMemoryBarrier(0u, 0u, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                                            writeLayout, *image, colorSRR, universalQF, writeQF);
                cmdPipelineImageMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                              VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, &release);
            }
            endCommandBuffer(ctx.vkd, cmdBuffer);
            submitCommandsAndWait(ctx.vkd, ctx.device, ctx.queue, cmdBuffer);

            CommandPoolWithBuffer computeCmd(ctx.vkd, ctx.device, computeQF);
            const auto computeCmdBuffer = *computeCmd.cmdBuffer;

            beginCommandBuffer(ctx.vkd, computeCmdBuffer);
            {
                const auto barrier = makeImageMemoryBarrier(0u, writeAccess, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                                            writeLayout, *image, colorSRR, universalQF, writeQF);
                cmdPipelineImageMemoryBarrier(ctx.vkd, computeCmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, writeStage,
                                              &barrier);
            }
            recordCopyWrite(computeCmdBuffer);
            {
                const auto barrier = makeImageMemoryBarrier(writeAccess, 0u, writeLayout, readLayout, *image, colorSRR,
                                                            writeQF, universalQF);
                cmdPipelineImageMemoryBarrier(ctx.vkd, computeCmdBuffer, writeStage,
                                              VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, &barrier);
            }
            endCommandBuffer(ctx.vkd, computeCmdBuffer);
            submitCommandsAndWait(ctx.vkd, ctx.device, context.getComputeQueue(), computeCmdBuffer);

            // Reuse the universal queue command buffer for the read operations. With exclusive images, this barrier
            // is the acquire operation. With concurrent images, it only makes the writes visible.
            beginCommandBuffer(ctx.vkd, cmdBuffer);
            {
                const auto oldLayout = (isQfot ? writeLayout : readLayout);
                const auto barrier   = makeImageMemoryBarrier(0u, readAccess, oldLayout, readLayout, *image, colorSRR,
                                                              writeQF, universalQF);
                cmdPipelineImageMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, readStage,
                                              &barrier);
            }
        }
        else
        {
            if (writeLayout != VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
            {
                const auto barrier = makeImageMemoryBarrier(0u, writeAccess, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                                            writeLayout, *image, colorSRR);
                cmdPipelineImageMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                              writeStage, &barrier);
            }

            if (isCopyWrite)
                recordCopyWrite(cmdBuffer);
            else
            {
                const auto pcStages = VK_SHADER_STAGE_FRAGMENT_BIT;

                for (uint32_t level = 0u; level < mipLevelCount; ++level)
                {
                    const auto levelExtent = mipLevelExtents(kExtent, level);
                    const auto viewport    = makeViewport(levelExtent);
                    const auto scissor     = makeRect2D(getCoveredWidth(*params, level), levelExtent.height);

                    beginRenderPass(ctx.vkd, cmdBuffer, *renderPass, *framebuffers.at(level), makeRect2D(levelExtent));
                    ctx.vkd.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *graphicsPipeline);
                    ctx.vkd.cmdSetViewport(cmdBuffer, 0u, 1u, &viewport);
                    ctx.vkd.cmdSetScissor(cmdBuffer, 0u, 1u, &scissor);
                    ctx.vkd.cmdPushConstants(cmdBuffer, *graphicsPipelineLayout, pcStages, 0u, DE_SIZEOF32(level),
                                             &level);
                    ctx.vkd.cmdDraw(cmdBuffer, 3u, 1u, 0u, 0u);
                    endRenderPass(ctx.vkd, cmdBuffer);
                }
            }

            const auto barrier =
                makeImageMemoryBarrier(writeAccess, readAccess, writeLayout, readLayout, *image, colorSRR);
            cmdPipelineImageMemoryBarrier(ctx.vkd, cmdBuffer, writeStage, readStage, &barrier);
        }

        // Read the image back into the readback buffer.
        if (sampleRead)
        {
            ctx.vkd.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *computePipeline);
            ctx.vkd.cmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *computePipelineLayout, 0u, 1u,
                                          &descriptorSet.get(), 0u, nullptr);
            ctx.vkd.cmdDispatch(cmdBuffer, kExtent.width / kLocalSize, kExtent.height, 1u);

            const auto barrier = makeMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
            cmdPipelineMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                     VK_PIPELINE_STAGE_HOST_BIT, &barrier);
        }
        else
        {
            std::vector<VkBufferImageCopy> regions;
            regions.reserve(mipLevelCount);

            for (uint32_t level = 0u; level < mipLevelCount; ++level)
            {
                const auto levelExtent = mipLevelExtents(kExtent, level);
                const auto levelLayers = makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, level, 0u, 1u);
                auto region            = makeBufferImageCopy(levelExtent, levelLayers);
                region.bufferOffset    = levelOffsets.at(level);
                regions.push_back(region);
            }

            ctx.vkd.cmdCopyImageToBuffer(cmdBuffer, *image, readLayout, readbackBuffer.get(), de::sizeU32(regions),
                                         de::dataOrNull(regions));

            const auto barrier = makeMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
            cmdPipelineMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                                     &barrier);
        }

        endCommandBuffer(ctx.vkd, cmdBuffer);
        submitCommandsAndWait(ctx.vkd, ctx.device, ctx.queue, cmdBuffer);
        invalidateAlloc(ctx.vkd, ctx.device, readbackAlloc);

        // Quick byte comparison first. On mismatch, the threshold comparison decides the result and logs the images.
        const auto resultData    = reinterpret_cast<const uint8_t *>(readbackAlloc.getHostPtr());
        const auto checkedLevels = (sampleRead ? 1u : mipLevelCount);
        const auto threshold     = ((!isCopyWrite && !isUint) ? 1u : 0u); // See the TEST MECHANISM comment.

        for (uint32_t level = 0u; level < checkedLevels; ++level)
        {
            const auto levelExtent = mipLevelExtents(kExtent, level);
            const auto width       = static_cast<int>(levelExtent.width);
            const auto height      = static_cast<int>(levelExtent.height);
            const auto offset      = static_cast<size_t>(levelOffsets.at(level));
            const auto levelSize   = static_cast<size_t>(width * height) * pixelSize;

            if (std::memcmp(resultData + offset, expected.data() + offset, levelSize) == 0)
                continue;

            const auto setName = "MemType" + std::to_string(memTypeIdx) + "Level" + std::to_string(level);
            const auto setDesc = "Memory type " + std::to_string(memTypeIdx) + ", mip level " + std::to_string(level);

            log << tcu::TestLog::Message << setDesc
                << ": results differ from the expected bytes; comparing with threshold " << threshold
                << tcu::TestLog::EndMessage;

            const tcu::ConstPixelBufferAccess reference(tcuFormat, width, height, 1, expected.data() + offset);
            const tcu::ConstPixelBufferAccess result(tcuFormat, width, height, 1, resultData + offset);

            if (!tcu::intThresholdCompare(log, setName.c_str(), setDesc.c_str(), reference, result,
                                          tcu::UVec4(threshold), tcu::COMPARE_LOG_ON_ERROR))
                fail = true;
        }
    }

    if (fail)
        TCU_FAIL("Unexpected results in some memory types or mip levels; check log for details --");

    return tcu::TestStatus::pass("Pass");
}

void createTests(tcu::TestCaseGroup *group)
{
    const VkFormat formatList[] = {
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_FORMAT_R16G16B16A16_UINT,
    };

    const struct
    {
        WriteMethod writeMethod;
        const char *name;
    } writeCases[] = {
        {WriteMethod::EXCLUSIVE_TRANSFER_DST, "exclusive_transfer_dst"},
        {WriteMethod::EXCLUSIVE_GENERAL, "exclusive_general"},
        {WriteMethod::CONCURRENT_TRANSFER_DST, "concurrent_transfer_dst"},
        {WriteMethod::CONCURRENT_GENERAL, "concurrent_general"},
        {WriteMethod::CONCURRENT_COMPUTE_WRITE, "concurrent_transfer_dst_compute_write"},
        {WriteMethod::QFOT_COMPUTE, "qfot_compute"},
        {WriteMethod::FEEDBACK_LOOP, "feedback_loop"},
        {WriteMethod::PARTIAL_DRAW, "partial_draw"},
        {WriteMethod::CONCURRENT_DRAW, "concurrent_draw"},
    };

    const struct
    {
        ReadMethod readMethod;
        const char *name;
    } readCases[] = {
        {ReadMethod::COPY, "copy"},
        {ReadMethod::SAMPLE, "sample"},
    };

    for (const auto format : formatList)
    {
        const auto prefix = getFormatSimpleName(format) + "_";

        for (const auto &writeCase : writeCases)
            for (const auto &readCase : readCases)
            {
                const auto params      = std::make_shared<const Params>(Params{
                    format,
                    writeCase.writeMethod,
                    readCase.readMethod,
                });
                const bool readGeneral = (params->getReadLayout() == VK_IMAGE_LAYOUT_GENERAL);
                const auto testName =
                    prefix + writeCase.name + (readGeneral ? "_read_general_" : "_read_") + readCase.name;
                addFunctionCaseWithPrograms(group, testName, checkSupport, initPrograms, iterate, params);
            }

        const auto params = std::make_shared<const Params>(Params{
            format,
            WriteMethod::MIP_LEVELS_DRAW,
            ReadMethod::COPY,
        });
        addFunctionCaseWithPrograms(group, prefix + "mip_levels_partial_draw_read_copy", checkSupport, initPrograms,
                                    iterate, params);
    }
}

} // namespace ColorLayouts

using GroupPtr = de::MovePtr<tcu::TestCaseGroup>;

} // namespace

tcu::TestCaseGroup *createClearedAllocationControlTests(tcu::TestContext &testCtx)
{
    GroupPtr mainGroup(new tcu::TestCaseGroup(testCtx, "zero_initialize_device_memory"));
    GroupPtr bufferGroup(new tcu::TestCaseGroup(testCtx, "clear_buffer"));
    GroupPtr imageTransition(new tcu::TestCaseGroup(testCtx, "image_transition"));

    const std::vector<VkDeviceSize> bufferSizeCases{1u, 4u, 4096u, 4194304u};
    struct BufferUsageFlagName
    {
        VkBufferUsageFlagBits usageBit;
        const char *name;
    };
    const std::vector<BufferUsageFlagName> bufferUsageCases{
        {VK_BUFFER_USAGE_TRANSFER_DST_BIT, "transfer_dst"},
        {VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT, "uniform_texel_buffer"},
        {VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT, "storage_texel_buffer"},
        {VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, "uniform_buffer"},
        {VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, "storage_buffer"},
        {VK_BUFFER_USAGE_INDEX_BUFFER_BIT, "index_buffer"},
        {VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, "vertex_buffer"},
        {VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT, "indirect_buffer"},
    };

    for (const auto &usage : bufferUsageCases)
        for (const auto &size : bufferSizeCases)
            for (const bool hostVisible : {false, true})
            {
                const BufferAllocationParams params{
                    size,
                    static_cast<VkBufferUsageFlags>(usage.usageBit),
                    hostVisible,
                };

                const auto testName =
                    std::string(usage.name) + "_" + std::to_string(size) + (hostVisible ? "_host_visible" : "");

                addFunctionCase(bufferGroup.get(), testName, clearBufferAllocationCheckSupport, clearBufferAllocation,
                                params);
            }

    const std::vector<tcu::IVec2> mipSizes{
        tcu::IVec2(1, 1),
        tcu::IVec2(4, 4),
        tcu::IVec2(53, 92),
        tcu::IVec2(512, 512),
    };

    {
        const std::vector<VkFormat> formatList{
            VK_FORMAT_R8_UNORM,
            VK_FORMAT_R8G8_UNORM,
            VK_FORMAT_R16_UNORM,
            VK_FORMAT_R8G8B8_UNORM,
            VK_FORMAT_R8G8B8A8_UNORM,
            VK_FORMAT_R32_UINT,
            VK_FORMAT_R32_SINT,
            VK_FORMAT_R32_SFLOAT,
            VK_FORMAT_R32G32B32A32_SFLOAT,
            VK_FORMAT_BC1_RGBA_UNORM_BLOCK,
        };

        const struct
        {
            VkImageUsageFlagBits usage;
            const char *name;
        } usageCases[] = {
            {VK_IMAGE_USAGE_TRANSFER_SRC_BIT, "transfer_src"},
            {VK_IMAGE_USAGE_SAMPLED_BIT, "sampled"},
            {VK_IMAGE_USAGE_STORAGE_BIT, "storage"},
        };

        const struct
        {
            VkShaderStageFlagBits readStage;
            const char *name;
        } readStageCases[] = {
            {VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM, "xfer"},
            {VK_SHADER_STAGE_COMPUTE_BIT, "comp"},
            {VK_SHADER_STAGE_FRAGMENT_BIT, "frag"},
        };

        for (const auto &format : formatList)
            for (const auto &usageCase : usageCases)
            {
                if (!isCompressedFormat(format))
                {
                    // RGB8 storage images do not exist.
                    const auto tcuFormat = mapVkFormat(format);
                    if (tcu::getNumUsedChannels(tcuFormat.order) == 3 &&
                        (usageCase.usage & VK_IMAGE_USAGE_STORAGE_BIT) != 0u)
                        continue;
                }

                for (const auto &readStageCase : readStageCases)
                {
                    const bool isTransfer = (usageCase.usage == VK_IMAGE_USAGE_TRANSFER_SRC_BIT);

                    if (isTransfer && readStageCase.readStage != VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM)
                        continue;

                    if (!isTransfer && readStageCase.readStage == VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM)
                        continue;

                    if (isTransfer && isCompressedFormat(format))
                        continue;

                    for (const auto &mipSize : mipSizes)
                        for (const bool firstMip : {true, false})
                        {
                            const ImageTransitionParams params{
                                format,   usageCase.usage,         tcu::IVec3(mipSize.x(), mipSize.y(), 1),
                                firstMip, readStageCase.readStage,
                            };
                            const auto testName = getFormatSimpleName(format) + "_" + usageCase.name + "_shader_" +
                                                  readStageCase.name + "_" + std::to_string(mipSize.x()) + "x" +
                                                  std::to_string(mipSize.y()) +
                                                  (firstMip ? "_first_mip" : "_second_mip");
                            imageTransition->addChild(new ImageTransitionCase(testCtx, testName, params));
                        }
                }
            }
    }

    {

        for (const VkFormat format : formats::depthAndStencilFormats)
            for (const auto &mipSize : mipSizes)
                for (const bool firstMip : {true, false})
                {
                    const DepthFormatParams params{
                        format,
                        tcu::IVec3(mipSize.x(), mipSize.y(), 1),
                        firstMip,
                    };
                    const auto testName = getFormatSimpleName(format) + "_" + std::to_string(mipSize.x()) + "x" +
                                          std::to_string(mipSize.y()) + (firstMip ? "_first_mip" : "_second_mip");
                    imageTransition->addChild(new DepthFormatCase(testCtx, testName, params));
                }
    }

    mainGroup->addChild(bufferGroup.release());
    mainGroup->addChild(imageTransition.release());
    mainGroup->addChild(createTestGroup2(testCtx, "color_attachment_layouts", ColorLayouts::createTests));

    return mainGroup.release();
}

} // namespace memory
} // namespace vkt
