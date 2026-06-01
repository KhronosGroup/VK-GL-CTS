/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2026 The Khronos Group Inc.
 * Copyright (c) 2026 Valve Corporation.
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
 * \brief Check using image memory as Opaque FD and DMA at the same time.
 *//*--------------------------------------------------------------------*/

#include "vktMemoryOpaqueAndDmaImageTests.hpp"
#include "vktTestCase.hpp"
#include "vktTestCaseUtil.hpp"
#include "vktTestGroupUtil.hpp"
#include "vkDefs.hpp"

#include "vkBarrierUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkRefUtil.hpp"

#include "tcuVector.hpp"
#include "tcuImageCompare.hpp"

#include "deRandom.hpp"

#include <memory>
#include <string>
#include <sstream>
#include <vector>

namespace vkt::memory
{

namespace
{

using namespace vk;

struct Params
{
    VkFormat format;
    bool sample; // If true, sample the imported image. If false, copy it to a buffer.

    tcu::IVec3 getExtent() const
    {
        return tcu::IVec3(64, 64, 1);
    }

    uint32_t getRandomSeed() const
    {
        uint32_t seed = static_cast<uint32_t>(format);
        seed |= (static_cast<uint32_t>(sample) << 31);
        return seed;
    }

    VkImageTiling getTiling() const
    {
        return VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT;
    }

    VkImageUsageFlags getUsage() const
    {
        return static_cast<VkImageUsageFlags>(VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                                              (sample ? VK_IMAGE_USAGE_SAMPLED_BIT : 0));
    }

    VkFormatFeatureFlags getRequiredFeatures() const
    {
        return static_cast<VkFormatFeatureFlags>(
            sample ? VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT :
                     (VK_FORMAT_FEATURE_TRANSFER_SRC_BIT | VK_FORMAT_FEATURE_TRANSFER_DST_BIT));
    }

    std::string getName() const
    {
        return getFormatSimpleName(format) + "_" + (sample ? "sample" : "transfer");
    }
};
using ParamsPtr = std::shared_ptr<const Params>;

VkExternalMemoryProperties getExternalMemoryProperties(const InstanceInterface &vki, VkPhysicalDevice physicalDevice,
                                                       VkExternalMemoryHandleTypeFlagBits handleType, VkFormat format,
                                                       VkImageTiling tiling, VkImageUsageFlags usage, uint64_t modifier)
{
    VkPhysicalDeviceImageDrmFormatModifierInfoEXT modifierInfo = initVulkanStructure();
    modifierInfo.drmFormatModifier                             = modifier;

    VkPhysicalDeviceExternalImageFormatInfo externalImageFormatInfo = initVulkanStructureConst(&modifierInfo);
    externalImageFormatInfo.handleType                              = handleType;

    VkPhysicalDeviceImageFormatInfo2 imageFormatInfo = initVulkanStructureConst(&externalImageFormatInfo);
    imageFormatInfo.format                           = format;
    imageFormatInfo.tiling                           = tiling;
    imageFormatInfo.usage                            = usage;

    VkExternalImageFormatProperties externalFormatProperties = initVulkanStructureConst();
    VkImageFormatProperties2 formatProperties                = initVulkanStructure(&externalFormatProperties);

    const auto result =
        vki.getPhysicalDeviceImageFormatProperties2(physicalDevice, &imageFormatInfo, &formatProperties);
    if (result == VK_ERROR_FORMAT_NOT_SUPPORTED
#ifndef CTS_USES_VULKANSC
        || result == VK_ERROR_IMAGE_USAGE_NOT_SUPPORTED_KHR
#endif // CTS_USES_VULKANSC
    )
        TCU_THROW(NotSupportedError, "Format not supported for intended usage, tiling or handle type");

    if (result == VK_SUCCESS)
        return externalFormatProperties.externalMemoryProperties;

    TCU_FAIL("Unexpected error in vkGetPhysicalDeviceImageFormatProperties2");
    return VkExternalMemoryProperties{}; // Unreachable.
}

struct ModifierPlaneCount
{
    uint64_t modifier;
    uint32_t planeCount;
};

// Find the first acceptable modifier that meets our feature requirements.
tcu::Maybe<ModifierPlaneCount> getCompatibleDrmModifier(const InstanceInterface &vki, VkPhysicalDevice physicalDevice,
                                                        VkFormat format, VkFormatFeatureFlags req)
{
    tcu::Maybe<ModifierPlaneCount> modifier = tcu::Nothing;

    VkDrmFormatModifierPropertiesListEXT drmModifierProperties = initVulkanStructureConst();
    VkFormatProperties2 formatProperties                       = initVulkanStructure(&drmModifierProperties);
    vki.getPhysicalDeviceFormatProperties2(physicalDevice, format, &formatProperties);

    if (drmModifierProperties.drmFormatModifierCount > 0u)
    {
        std::vector<VkDrmFormatModifierPropertiesEXT> modifierVec(drmModifierProperties.drmFormatModifierCount);
        drmModifierProperties.pDrmFormatModifierProperties = modifierVec.data();

        vki.getPhysicalDeviceFormatProperties2(physicalDevice, format, &formatProperties);
        for (uint32_t i = 0u; i < drmModifierProperties.drmFormatModifierCount; ++i)
        {
            const auto &modifierInfo = modifierVec.at(i);
            if ((modifierInfo.drmFormatModifierTilingFeatures & req) == req)
            {
                modifier = tcu::just(
                    ModifierPlaneCount{modifierInfo.drmFormatModifier, modifierInfo.drmFormatModifierPlaneCount});
                break;
            }
        }
    }

    return modifier;
}

std::string getHandleTypeStr(VkExternalMemoryHandleTypeFlagBits handleType)
{
    std::string ret;

    if (handleType == VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT)
        ret = "opaque_fd";
    else if (handleType == VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT)
        ret = "dma_buf";

    DE_ASSERT(!ret.empty());
    return ret;
}

std::vector<VkExternalMemoryHandleTypeFlagBits> getBothHandleTypes()
{
    const std::vector<VkExternalMemoryHandleTypeFlagBits> handleTypeVec{
        VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT,
        VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT,
    };
    DE_ASSERT(handleTypeVec.size() == 2);

    return handleTypeVec;
}

void checkSupport(Context &context, ParamsPtr params)
{
    context.requireInstanceFunctionality("VK_KHR_get_physical_device_properties2");
    context.requireInstanceFunctionality("VK_KHR_external_memory_capabilities");
    context.requireDeviceFunctionality("VK_KHR_external_memory");
    context.requireDeviceFunctionality("VK_KHR_external_memory_fd");
    context.requireDeviceFunctionality("VK_EXT_external_memory_dma_buf");
    context.requireDeviceFunctionality("VK_EXT_image_drm_format_modifier");
    context.requireDeviceFunctionality("VK_KHR_maintenance1");

    const auto format = params->format;
    const auto tiling = params->getTiling();
    const auto usage  = params->getUsage();

    const auto ctx = context.getContextCommonData();

    // Note the formats chosen for these tests all have mandatory support for sampling (hence also transferring) and
    // color attachment support, so we skip those basic checks.

    // We need a DRM format modifier that supports the features we need.
    const auto requiredFeatures       = params->getRequiredFeatures();
    const auto modifierPlaneCountInfo = getCompatibleDrmModifier(ctx.vki, ctx.physicalDevice, format, requiredFeatures);
    if (!modifierPlaneCountInfo)
    {
        std::ostringstream msg;
        msg << getFormatSimpleName(format) << " does not have a DRM modifier which supports the required features";
        TCU_THROW(NotSupportedError, msg.str());
    }
    const auto modifierPlaneCount = *modifierPlaneCountInfo;

    const auto bothHandleTypes = getBothHandleTypes();
    bool compatible            = false;

    for (size_t i = 0u; i < bothHandleTypes.size(); ++i)
    {
        const auto handleType      = bothHandleTypes.at(i);
        const auto otherHandleType = bothHandleTypes.at(1 - i);

        const auto memProps = getExternalMemoryProperties(ctx.vki, ctx.physicalDevice, handleType, format, tiling,
                                                          usage, modifierPlaneCount.modifier);

        if ((memProps.externalMemoryFeatures & VK_EXTERNAL_MEMORY_FEATURE_IMPORTABLE_BIT) == 0)
        {
            std::ostringstream msg;
            msg << "Handles of type " << getHandleTypeStr(handleType) << " cannot be imported";
            TCU_THROW(NotSupportedError, msg.str());
        }

        // Finding the other handle type in one of the flag fields is good enough.
        if ((memProps.compatibleHandleTypes & otherHandleType))
            compatible = true;
    }

    if (!compatible)
    {
        std::ostringstream msg;
        msg << "OPAQUE_FD not compatible with DMA_BUF";
        TCU_THROW(NotSupportedError, msg.str());
    }
}

void initPrograms(vk::SourceCollections &dst, ParamsPtr params)
{
    if (params->sample)
    {
        std::ostringstream vert;
        vert << "#version 460\n"
             << "vec2 positions[3] = vec2[](\n"
             << "        vec2(-1.0, -1.0),"
             << "        vec2(3.0, -1.0),"
             << "        vec2(-1.0, 3.0)"
             << ");\n"
             << "void main() {\n"
             << "        gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);\n"
             << "}";
        dst.glslSources.add("vert") << glu::VertexSource(vert.str());

        const auto extent = params->getExtent();
        std::ostringstream frag;
        frag << "#version 460\n"
             << "layout (location=0) out vec4 outColor;\n"
             << "layout (set=0, binding=0) uniform sampler2D img;\n"
             << "void main(void) {\n"
             << "    const vec2 extent = vec2" << extent.swizzle(0, 1) << ";\n"
             << "    const vec2 coords = gl_FragCoord.xy / extent;\n"
             << "    outColor = texture(img, coords);\n"
             << "}\n";
        dst.glslSources.add("frag") << glu::FragmentSource(frag.str());
    }
}

tcu::TestStatus runTest(Context &context, ParamsPtr params)
{
    const auto ctx             = context.getContextCommonData();
    const auto format          = params->format;
    const auto extent          = params->getExtent();
    const auto extentVk        = makeExtent3D(extent);
    const auto tiling          = params->getTiling();
    const auto usage           = params->getUsage();
    const auto bothHandleTypes = getBothHandleTypes();
    const auto colorSRR        = makeDefaultImageSubresourceRange();
    const auto colorSRL        = makeDefaultImageSubresourceLayers();
    const auto copyRegion      = makeBufferImageCopy(extentVk, colorSRL);
    const auto imgType         = VK_IMAGE_TYPE_2D;
    const auto rgbThreshold    = 0.005f; // 1/255 < 0.005 < 2/255 to allow for some imprecission.
    const tcu::Vec4 threshold(rgbThreshold, rgbThreshold, rgbThreshold, 0.0f);

    VkExternalMemoryHandleTypeFlags bothHandleTypesMask = 0u;
    for (const auto flagBit : bothHandleTypes)
        bothHandleTypesMask |= flagBit;

    const auto requiredFormatFeatures = params->getRequiredFeatures();
    const auto modifierPlaneCountInfo =
        getCompatibleDrmModifier(ctx.vki, ctx.physicalDevice, format, requiredFormatFeatures);
    DE_ASSERT(!!modifierPlaneCountInfo);
    const auto modifierPlaneCount = *modifierPlaneCountInfo;

    VkImageDrmFormatModifierListCreateInfoEXT drmModCreateInfo = initVulkanStructure();
    drmModCreateInfo.drmFormatModifierCount                    = 1u;
    drmModCreateInfo.pDrmFormatModifiers                       = &modifierPlaneCount.modifier;

    VkExternalMemoryImageCreateInfo externalMemoryCreateInfo = initVulkanStructure(&drmModCreateInfo);
    externalMemoryCreateInfo.handleTypes                     = bothHandleTypesMask;

    VkImageCreateInfo imgCreateInfo{
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        &externalMemoryCreateInfo,
        0u,
        imgType,
        format,
        extentVk,
        1u,
        1u,
        VK_SAMPLE_COUNT_1_BIT,
        tiling,
        usage,
        VK_SHARING_MODE_EXCLUSIVE,
        0u,
        nullptr,
        VK_IMAGE_LAYOUT_UNDEFINED,
    };
    const auto srcImage = createImage(ctx.vkd, ctx.device, &imgCreateInfo);
    const auto memReqs  = getImageMemoryRequirements(ctx.vkd, ctx.device, *srcImage);

    // Select the first available memory type.
    tcu::Maybe<uint32_t> selectedMemoryTypeInfo = tcu::Nothing;
    const auto maxMemoryType                    = DE_SIZEOF32(uint32_t) * 8u;
    for (uint32_t i = 0u; i < maxMemoryType; ++i)
    {
        if (memReqs.memoryTypeBits & (1 << i))
        {
            selectedMemoryTypeInfo = tcu::just(i);
            break;
        }
    }
    DE_ASSERT(!!selectedMemoryTypeInfo);
    const auto selectedMemoryType = *selectedMemoryTypeInfo;

    const auto opaqueFdMemoryProperties =
        getExternalMemoryProperties(ctx.vki, ctx.physicalDevice, VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT, format,
                                    tiling, usage, modifierPlaneCount.modifier);
    const auto dmaBufMemoryProperties =
        getExternalMemoryProperties(ctx.vki, ctx.physicalDevice, VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT, format,
                                    tiling, usage, modifierPlaneCount.modifier);

    const bool requiresDedicated = (static_cast<bool>(opaqueFdMemoryProperties.externalMemoryFeatures &
                                                      VK_EXTERNAL_MEMORY_FEATURE_DEDICATED_ONLY_BIT) ||
                                    static_cast<bool>(dmaBufMemoryProperties.externalMemoryFeatures &
                                                      VK_EXTERNAL_MEMORY_FEATURE_DEDICATED_ONLY_BIT));

    VkMemoryDedicatedAllocateInfo memoryDedicatedAllocateInfo{
        VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO,
        nullptr,
        *srcImage,
        VK_NULL_HANDLE,
    };

    const auto dedicatedPtr = (requiresDedicated ? &memoryDedicatedAllocateInfo : nullptr);

    // This is the key for these tests: the memory is allocated with both opaque fd and DMA buffer export handle types.
    VkExportMemoryAllocateInfo exportMemoryAllocateInfo{
        VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO,
        dedicatedPtr,
        bothHandleTypesMask,
    };

    VkMemoryAllocateInfo memoryAllocateInfo{
        VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        &exportMemoryAllocateInfo,
        memReqs.size,
        selectedMemoryType,
    };

    const auto deviceMemory = allocateMemory(ctx.vkd, ctx.device, &memoryAllocateInfo);
    VK_CHECK(ctx.vkd.bindImageMemory(ctx.device, *srcImage, *deviceMemory, 0ull));

    // Create a source buffer with pixel data and copy data to the image with it.
    const auto seed = params->getRandomSeed();
    de::Random rng(seed);

    const auto tcuFormat = mapVkFormat(format);
    tcu::TextureLevel refLevel(tcuFormat, extent.x(), extent.y(), extent.z());
    tcu::PixelBufferAccess reference = refLevel.getAccess();

    for (int y = 0; y < extent.y(); ++y)
        for (int x = 0; x < extent.x(); ++x)
        {
            const auto red   = rng.getFloat();
            const auto green = rng.getFloat();
            const auto blue  = rng.getFloat();

            const tcu::Vec4 color(red, green, blue, 1.0f);
            reference.setPixel(color, x, y);
        }

    const auto pixelSize  = tcu::getPixelSize(tcuFormat);
    const auto bufferSize = static_cast<VkDeviceSize>(pixelSize * extent.x() * extent.y() * extent.z());
    const auto bufferUsage =
        static_cast<VkBufferUsageFlags>(VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    const auto bufferInfo = makeBufferCreateInfo(bufferSize, bufferUsage);
    BufferWithMemory srcBuffer(ctx.vkd, ctx.device, ctx.allocator, bufferInfo, HostIntent::W);
    {
        auto &alloc = srcBuffer.getAllocation();
        memcpy(alloc.getHostPtr(), reference.getDataPtr(), static_cast<size_t>(bufferSize));
        flushAlloc(ctx.vkd, ctx.device, alloc);
    }

    // Copy data to the source image.
    const auto readLayout =
        (params->sample ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

    {
        CommandPoolWithBuffer cmd(ctx.vkd, ctx.device, ctx.qfIndex);
        const auto cmdBuffer = *cmd.cmdBuffer;

        beginCommandBuffer(ctx.vkd, cmdBuffer);

        const auto preCopy = makeImageMemoryBarrier(0u, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                                                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, *srcImage, colorSRR);
        cmdPipelineImageMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                      VK_PIPELINE_STAGE_TRANSFER_BIT, &preCopy);

        ctx.vkd.cmdCopyBufferToImage(cmdBuffer, *srcBuffer, *srcImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1u,
                                     &copyRegion);

        const auto postCopy = makeImageMemoryBarrier(
            VK_ACCESS_TRANSFER_WRITE_BIT, 0u, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, readLayout, *srcImage, colorSRR);
        cmdPipelineImageMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                      VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, &postCopy);

        endCommandBuffer(ctx.vkd, cmdBuffer);
        submitCommandsAndWait(ctx.vkd, ctx.device, ctx.queue, cmdBuffer);
    }

    // Query subresource layouts, which will be needed later.
    const std::vector<VkImageAspectFlagBits> planeAspects{
        VK_IMAGE_ASPECT_MEMORY_PLANE_0_BIT_EXT,
        VK_IMAGE_ASPECT_MEMORY_PLANE_1_BIT_EXT,
        VK_IMAGE_ASPECT_MEMORY_PLANE_2_BIT_EXT,
        VK_IMAGE_ASPECT_MEMORY_PLANE_3_BIT_EXT,
    };
    if (modifierPlaneCount.planeCount > de::sizeU32(planeAspects))
        TCU_FAIL("Plane count too large");

    std::vector<VkSubresourceLayout> planeLayouts(modifierPlaneCount.planeCount);
    for (uint32_t i = 0u; i < modifierPlaneCount.planeCount; ++i)
    {
        const VkImageSubresource subresource{
            planeAspects.at(i),
            0u,
            0u,
        };
        ctx.vkd.getImageSubresourceLayout(ctx.device, *srcImage, &subresource, &planeLayouts.at(i));
    }

    // Fix info that will be used later.
    for (auto &planeLayout : planeLayouts)
    {
        planeLayout.size       = 0;  // VUID-VkImageDrmFormatModifierExplicitCreateInfoEXT-size-02267
        planeLayout.arrayPitch = 0u; // VUID-VkImageDrmFormatModifierExplicitCreateInfoEXT-arrayPitch-02268
        planeLayout.depthPitch = 0u; // VUID-VkImageDrmFormatModifierExplicitCreateInfoEXT-depthPitch-02269
    }

    // Now we need to export the image memory as both handle types, import it and try to see if contents match.
    bool fail = false;
    auto &log = context.getTestContext().getLog();

    for (const auto handleType : bothHandleTypes)
    {
        // Create image first with explicit modifier info and plane layout information.
        const VkImageDrmFormatModifierExplicitCreateInfoEXT explicitModifierInfo{
            VK_STRUCTURE_TYPE_IMAGE_DRM_FORMAT_MODIFIER_EXPLICIT_CREATE_INFO_EXT,
            nullptr,
            modifierPlaneCount.modifier,
            de::sizeU32(planeLayouts),
            de::dataOrNull(planeLayouts),
        };

        // Redo the creation chain.
        externalMemoryCreateInfo.pNext = &explicitModifierInfo;
        imgCreateInfo.pNext            = &externalMemoryCreateInfo;

        const auto dstImage = createImage(ctx.vkd, ctx.device, &imgCreateInfo);

        // Obtain memory fd.
        const VkMemoryGetFdInfoKHR getFdInfo{
            VK_STRUCTURE_TYPE_MEMORY_GET_FD_INFO_KHR,
            nullptr,
            *deviceMemory,
            handleType,
        };
        int fd = -1;
        VK_CHECK(ctx.vkd.getMemoryFdKHR(ctx.device, &getFdInfo, &fd));
        if (fd < 0)
            TCU_FAIL("Got invalid file descriptor");

        // Import the previous memory object as memory for the image, including the dedicated allocation info if needed.
        memoryDedicatedAllocateInfo.image = *dstImage;
        VkImportMemoryFdInfoKHR memoryFdInfo{
            VK_STRUCTURE_TYPE_IMPORT_MEMORY_FD_INFO_KHR,
            dedicatedPtr,
            handleType,
            fd,
        };
        memoryAllocateInfo.pNext = &memoryFdInfo;

        // Create and bind to dst image.
        const auto importedMemory = allocateMemory(ctx.vkd, ctx.device, &memoryAllocateInfo);
        VK_CHECK(ctx.vkd.bindImageMemory(ctx.device, *dstImage, *importedMemory, 0ull));

        if (params->sample)
        {
            // Create a render pass and pipeline for this.
            const auto colorUsage =
                static_cast<VkImageUsageFlags>(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
            ImageWithBuffer colorBuffer(ctx.vkd, ctx.device, ctx.allocator, extentVk, format, colorUsage, imgType);

            const auto renderPass  = makeRenderPass(ctx.vkd, ctx.device, format);
            const auto framebuffer = makeFramebuffer(ctx.vkd, ctx.device, *renderPass, colorBuffer.getImageView(),
                                                     extentVk.width, extentVk.height);

            const VkSamplerCreateInfo samplerInfo{
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
                1.0f,
                VK_FALSE,
                VK_COMPARE_OP_NEVER,
                0.0f,
                0.0f,
                VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
                VK_FALSE,
            };
            const auto sampler = createSampler(ctx.vkd, ctx.device, &samplerInfo);

            const auto descType   = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            const auto descStages = static_cast<VkShaderStageFlags>(VK_SHADER_STAGE_FRAGMENT_BIT);

            DescriptorPoolBuilder poolBuilder;
            poolBuilder.addType(descType);
            const auto descriptorPool =
                poolBuilder.build(ctx.vkd, ctx.device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);

            DescriptorSetLayoutBuilder setLayoutBuilder;
            setLayoutBuilder.addSingleBinding(descType, descStages);
            const auto setLayout      = setLayoutBuilder.build(ctx.vkd, ctx.device);
            const auto pipelineLayout = makePipelineLayout(ctx.vkd, ctx.device, *setLayout);
            const auto descriptorSet  = makeDescriptorSet(ctx.vkd, ctx.device, *descriptorPool, *setLayout);

            const auto dstImageView =
                makeImageView(ctx.vkd, ctx.device, *dstImage, VK_IMAGE_VIEW_TYPE_2D, format, colorSRR);

            // We will sample the dst image.
            DescriptorSetUpdateBuilder updateBuilder;
            const auto descInfo = makeDescriptorImageInfo(*sampler, *dstImageView, readLayout);
            updateBuilder.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u), descType,
                                      &descInfo);
            updateBuilder.update(ctx.vkd, ctx.device);

            const auto &binaries  = context.getBinaryCollection();
            const auto vertShader = createShaderModule(ctx.vkd, ctx.device, binaries.get("vert"));
            const auto fragShader = createShaderModule(ctx.vkd, ctx.device, binaries.get("frag"));

            const std::vector<VkViewport> viewports(1u, makeViewport(extent));
            const std::vector<VkRect2D> scissors(1u, makeRect2D(extent));

            const VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo = initVulkanStructureConst();

            const auto pipeline =
                makeGraphicsPipeline(ctx.vkd, ctx.device, *pipelineLayout, *vertShader, VK_NULL_HANDLE, VK_NULL_HANDLE,
                                     VK_NULL_HANDLE, *fragShader, *renderPass, viewports, scissors,
                                     VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0u, 0u, &vertexInputStateCreateInfo);

            CommandPoolWithBuffer cmd(ctx.vkd, ctx.device, ctx.qfIndex);
            const auto cmdBuffer = *cmd.cmdBuffer;

            beginCommandBuffer(ctx.vkd, cmdBuffer);

            const auto prepareBarrier =
                makeImageMemoryBarrier(0u, VK_ACCESS_TRANSFER_READ_BIT, readLayout, readLayout, *dstImage, colorSRR);
            cmdPipelineImageMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                          VK_PIPELINE_STAGE_TRANSFER_BIT, &prepareBarrier);

            const tcu::Vec4 clearColor(0.0f);
            beginRenderPass(ctx.vkd, cmdBuffer, *renderPass, *framebuffer, scissors.front(), clearColor);
            const auto bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
            ctx.vkd.cmdBindPipeline(cmdBuffer, bindPoint, *pipeline);
            ctx.vkd.cmdBindDescriptorSets(cmdBuffer, bindPoint, *pipelineLayout, 0u, 1u, &descriptorSet.get(), 0u,
                                          nullptr);
            ctx.vkd.cmdDraw(cmdBuffer, 3u, 1u, 0u, 0u); // Fixed triangle. See vertex shader.
            endRenderPass(ctx.vkd, cmdBuffer);

            copyImageToBuffer(ctx.vkd, cmdBuffer, colorBuffer.getImage(), colorBuffer.getBuffer(),
                              extent.swizzle(0, 1));

            endCommandBuffer(ctx.vkd, cmdBuffer);
            submitCommandsAndWait(ctx.vkd, ctx.device, ctx.queue, cmdBuffer);

            auto &alloc = colorBuffer.getBufferAllocation();
            invalidateAlloc(ctx.vkd, ctx.device, alloc);
            tcu::ConstPixelBufferAccess result(tcuFormat, extent, alloc.getHostPtr());

            // We need to check it has preserved contents.
            if (!tcu::floatThresholdCompare(log, getHandleTypeStr(handleType).c_str(), "", reference, result, threshold,
                                            tcu::COMPARE_LOG_ON_ERROR))
                fail = true;
        }
        else
        {
            BufferWithMemory dstBuffer(ctx.vkd, ctx.device, ctx.allocator, bufferInfo, HostIntent::R);

            CommandPoolWithBuffer cmd(ctx.vkd, ctx.device, ctx.qfIndex);
            const auto cmdBuffer = *cmd.cmdBuffer;

            beginCommandBuffer(ctx.vkd, cmdBuffer);

            const auto prepareBarrier =
                makeImageMemoryBarrier(0u, VK_ACCESS_TRANSFER_READ_BIT, readLayout, readLayout, *dstImage, colorSRR);
            cmdPipelineImageMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                          VK_PIPELINE_STAGE_TRANSFER_BIT, &prepareBarrier);

            // Copy the imported image to a buffer.
            ctx.vkd.cmdCopyImageToBuffer(cmdBuffer, *dstImage, readLayout, *dstBuffer, 1u, &copyRegion);

            const auto hostBarrier = makeMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
            cmdPipelineMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                                     &hostBarrier);

            endCommandBuffer(ctx.vkd, cmdBuffer);
            submitCommandsAndWait(ctx.vkd, ctx.device, ctx.queue, cmdBuffer);

            auto &alloc = dstBuffer.getAllocation();
            invalidateAlloc(ctx.vkd, ctx.device, alloc);
            tcu::ConstPixelBufferAccess result(tcuFormat, extent, alloc.getHostPtr());

            // Verify contents were preserved.
            if (!tcu::floatThresholdCompare(log, getHandleTypeStr(handleType).c_str(), "", reference, result, threshold,
                                            tcu::COMPARE_LOG_ON_ERROR))
                fail = true;
        }
    }

    if (fail)
        TCU_FAIL("Unexpected results in imported memory image; check log for details --");

    return tcu::TestStatus::pass("Pass");
}

void populateMainGroup(tcu::TestCaseGroup *mainGroup)
{
    for (const auto format : {VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_B8G8R8A8_UNORM})
        for (const bool sample : {false, true})
        {
            ParamsPtr params(new Params{format, sample});
            addFunctionCaseWithPrograms(mainGroup, params->getName(), checkSupport, initPrograms, runTest, params);
        }
}

} // anonymous namespace

tcu::TestCaseGroup *createOpaqueAndDmaImageTests(tcu::TestContext &testCtx)
{
    return createTestGroup(testCtx, "opaque_and_dma", populateMainGroup);
}

} // namespace vkt::memory
