/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2025 LunarG, Inc.
 * Copyright (c) 2025 Nintendo
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
 * \brief 3D Image With Maintenance9 2D Array Compatible Bit Tests
 *//*--------------------------------------------------------------------*/

#include "vktTestCase.hpp"
#include "vkObjUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkBarrierUtil.hpp"
#include "deRandom.hpp"
#include "ycbcr/vktYCbCrUtil.hpp"
#include "vkBuilderUtil.hpp"

namespace vkt
{
namespace image
{

using namespace vk;

namespace
{

struct TestParameters
{
    uint32_t firstLayer;
    uint32_t secondLayer;
    uint32_t totalLayers;
    VkImageTiling tiling;
    VkImageViewType imageViewType;
};

class ArrayCompatibleTestInstance : public vkt::TestInstance
{
public:
    ArrayCompatibleTestInstance(vkt::Context &context, const TestParameters &parameters)
        : vkt::TestInstance(context)
        , m_parameters(parameters)
    {
    }

private:
    VkImageSubresourceRange makeSubresourceRange(uint32_t base);
    VkImageSubresourceLayers makeSubresourceLayers(uint32_t base);
    void transitionUnusedLayers(const DeviceInterface &vk, VkCommandBuffer commandBuffer, VkImage image);
    void transitionAllLayers(const DeviceInterface &vk, VkCommandBuffer commandBuffer, VkImage image,
                             VkImageLayout layout);
    tcu::TestStatus iterate(void);

    const TestParameters m_parameters;
};

VkImageSubresourceRange ArrayCompatibleTestInstance::makeSubresourceRange(uint32_t base)
{
    return makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, base, 1u);
}

VkImageSubresourceLayers ArrayCompatibleTestInstance::makeSubresourceLayers(uint32_t base)
{
    return makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, base, 1u);
}

void ArrayCompatibleTestInstance::transitionUnusedLayers(const DeviceInterface &vk, VkCommandBuffer commandBuffer,
                                                         VkImage image)
{
    for (uint32_t i = 0; i < m_parameters.totalLayers; ++i)
    {
        if (i != m_parameters.firstLayer && i != m_parameters.secondLayer)
        {
            VkImageMemoryBarrier imageMemoryBarrier =
                makeImageMemoryBarrier(VK_ACCESS_NONE, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                                       VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, image, makeSubresourceRange(i));
            vk.cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u,
                                  0u, nullptr, 0u, nullptr, 1u, &imageMemoryBarrier);
        }
    }
}

void ArrayCompatibleTestInstance::transitionAllLayers(const DeviceInterface &vk, VkCommandBuffer commandBuffer,
                                                      VkImage image, VkImageLayout layout)
{
    for (uint32_t i = 0; i < m_parameters.totalLayers; ++i)
    {
        VkImageMemoryBarrier imageMemoryBarrier =
            makeImageMemoryBarrier(VK_ACCESS_NONE, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, layout,
                                   image, makeSubresourceRange(i));
        vk.cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u,
                              nullptr, 0u, nullptr, 1u, &imageMemoryBarrier);
    }
}

tcu::TestStatus ArrayCompatibleTestInstance::iterate(void)
{
    const DeviceInterface &vk       = m_context.getDeviceInterface();
    const VkDevice device           = m_context.getDevice();
    const uint32_t queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();
    const VkQueue queue             = m_context.getUniversalQueue();
    auto &alloc                     = m_context.getDefaultAllocator();
    tcu::TestLog &log               = m_context.getTestContext().getLog();

    const VkFormat format                     = VK_FORMAT_R8G8B8A8_UNORM;
    const VkExtent3D extent                   = {32u, 32u, m_parameters.totalLayers};
    const VkExtent3D copyExtent               = {extent.width, extent.height, 1u};
    const VkComponentMapping componentMapping = makeComponentMappingRGBA();

    VkImageCreateFlags createFlags = VK_IMAGE_CREATE_2D_ARRAY_COMPATIBLE_BIT;
#ifndef CTS_USES_VULKANSC
    if (m_parameters.imageViewType == VK_IMAGE_VIEW_TYPE_2D)
        createFlags |= VK_IMAGE_CREATE_2D_VIEW_COMPATIBLE_BIT_EXT;
#endif

    VkImageCreateInfo imageCreateInfo = {
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, // VkStructureType          sType
        nullptr,                             // const void*              pNext
        createFlags,                         // VkImageCreateFlags       flags
        VK_IMAGE_TYPE_3D,                    // VkImageType              imageType
        format,                              // VkFormat                 format
        extent,                              // VkExtent3D               extent
        1u,                                  // uint32_t                 mipLevels
        1u,                                  // uint32_t                 arrayLayers
        VK_SAMPLE_COUNT_1_BIT,               // VkSampleCountFlagBits    samples
        m_parameters.tiling,                 // VkImageTiling            tiling
        VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
            VK_IMAGE_USAGE_SAMPLED_BIT, // VkImageUsageFlags        usage
        VK_SHARING_MODE_EXCLUSIVE,      // VkSharingMode            sharingMode
        0,                              // uint32_t                 queueFamilyIndexCount
        nullptr,                        // const uint32_t*          pQueueFamilyIndices
        VK_IMAGE_LAYOUT_UNDEFINED       // VkImageLayout            initialLayout
    };

    de::MovePtr<ImageWithMemory> image =
        de::MovePtr<ImageWithMemory>(new ImageWithMemory(vk, device, alloc, imageCreateInfo, MemoryRequirement::Any));

    const VkImageSubresourceRange imageViewSubresourceRange = m_parameters.imageViewType == VK_IMAGE_VIEW_TYPE_3D ?
                                                                  makeSubresourceRange(0u) :
                                                                  makeSubresourceRange(m_parameters.secondLayer);

    VkImageViewCreateInfo imageViewCreateInfo = {
        VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, // VkStructureType sType;
        nullptr,                                  // const void* pNext;
        (VkImageViewCreateFlags)0u,               // VkImageViewCreateFlags flags;
        **image,                                  // VkImage image;
        m_parameters.imageViewType,               // VkImageViewType viewType;
        format,                                   // VkFormat format;
        componentMapping,                         // VkComponentMapping components;
        imageViewSubresourceRange                 // VkImageSubresourceRange subresourceRange;
    };
    auto sampledImageView = createImageView(vk, device, &imageViewCreateInfo, nullptr);

    const Move<VkCommandPool> cmdPool(
        createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex));
    const Move<VkCommandBuffer> cmdBuffer(allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

    const uint32_t pixelSize                = tcu::getPixelSize(mapVkFormat(format));
    const uint32_t layerSize                = extent.width * extent.height * pixelSize;
    const uint32_t ssboSize                 = layerSize * 4u;
    de::MovePtr<BufferWithMemory> srcBuffer = de::MovePtr<BufferWithMemory>(
        new BufferWithMemory(vk, device, alloc, makeBufferCreateInfo(layerSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT),
                             MemoryRequirement::HostVisible));
    de::MovePtr<BufferWithMemory> dstBuffer = de::MovePtr<BufferWithMemory>(new BufferWithMemory(
        vk, device, alloc,
        makeBufferCreateInfo(layerSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT),
        MemoryRequirement::HostVisible));
    de::MovePtr<BufferWithMemory> ssbo      = de::MovePtr<BufferWithMemory>(
        new BufferWithMemory(vk, device, alloc, makeBufferCreateInfo(ssboSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT),
                                  MemoryRequirement::HostVisible));

    std::vector<uint8_t> testData(layerSize);
    de::Random randomGen(deInt32Hash((uint32_t)m_parameters.tiling));
    ycbcr::fillRandomNoNaN(&randomGen, testData.data(), layerSize, format);

    auto &srcBufferAlloc = srcBuffer->getAllocation();
    memcpy(srcBufferAlloc.getHostPtr(), testData.data(), layerSize);
    flushAlloc(vk, device, srcBufferAlloc);

    DescriptorSetLayoutBuilder descriptorBuilder;
    descriptorBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT);
    descriptorBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);

    const auto descriptorSetLayout(descriptorBuilder.build(vk, device));
    const PipelineLayoutWrapper pipelineLayout(PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC, vk, device, *descriptorSetLayout);

    DescriptorPoolBuilder poolBuilder;
    poolBuilder.addType(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    poolBuilder.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);

    VkSamplerCreateInfo samplerParams = {
        VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,   // VkStructureType sType;
        nullptr,                                 // const void* pNext;
        0u,                                      // VkSamplerCreateFlags flags;
        VK_FILTER_NEAREST,                       // VkFilter magFilter;
        VK_FILTER_NEAREST,                       // VkFilter minFilter;
        VK_SAMPLER_MIPMAP_MODE_NEAREST,          // VkSamplerMipmapMode mipmapMode;
        VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER, // VkSamplerAddressMode addressModeU;
        VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER, // VkSamplerAddressMode addressModeV;
        VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER, // VkSamplerAddressMode addressModeW;
        0.0f,                                    // float mipLodBias;
        VK_FALSE,                                // VkBool32 anisotropyEnable;
        0.0f,                                    // float maxAnisotropy;
        VK_FALSE,                                // VkBool32 compareEnable;
        VK_COMPARE_OP_NEVER,                     // VkCompareOp compareOp;
        0.0f,                                    // float minLod;
        VK_LOD_CLAMP_NONE,                       // float maxLod;
        VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE,      // VkBorderColor borderColor;
        VK_FALSE                                 // VkBool32 unnormalizedCoordinates;
    };
    const Move<VkSampler> sampler = createSampler(vk, device, &samplerParams);
    VkDescriptorImageInfo descriptorImageInfo(
        makeDescriptorImageInfo(*sampler, *sampledImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL));
    const VkDescriptorBufferInfo descriptorBufferInfo(makeDescriptorBufferInfo(**ssbo, 0u, VK_WHOLE_SIZE));

    const Move<VkDescriptorPool> descriptorPool =
        poolBuilder.build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1);
    const Move<VkDescriptorSet> descriptorSet(makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout));

    DescriptorSetUpdateBuilder updateBuilder;
    updateBuilder.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u),
                              VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &descriptorImageInfo);
    updateBuilder.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(1u),
                              VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &descriptorBufferInfo);
    updateBuilder.update(vk, device);

    const Unique<VkShaderModule> cs(createShaderModule(vk, device, m_context.getBinaryCollection().get("comp"), 0u));
    const VkPipelineShaderStageCreateInfo pipelineShaderStageParams = {
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        nullptr,
        (VkPipelineShaderStageCreateFlags)0u,
        VK_SHADER_STAGE_COMPUTE_BIT,
        *cs,
        "main",
        nullptr,
    };
    const VkComputePipelineCreateInfo pipelineCreateInfo = {
        VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        nullptr,
        (VkPipelineCreateFlags)0u,
        pipelineShaderStageParams,
        *pipelineLayout,
        VK_NULL_HANDLE,
        0,
    };
    const Move<VkPipeline> computePipeline = createComputePipeline(vk, device, VK_NULL_HANDLE, &pipelineCreateInfo);

    beginCommandBuffer(vk, *cmdBuffer);
    {
        VkImageSubresourceRange subresourceRange = makeSubresourceRange(m_parameters.firstLayer);
        VkImageMemoryBarrier imageMemoryBarrier =
            makeImageMemoryBarrier(VK_ACCESS_NONE, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, **image, subresourceRange);
        vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u,
                              nullptr, 0u, nullptr, 1u, &imageMemoryBarrier);

        VkBufferImageCopy region;
        region.bufferOffset      = 0u;
        region.bufferRowLength   = 0u;
        region.bufferImageHeight = 0u;
        region.imageSubresource  = makeSubresourceLayers(0u);
        region.imageOffset       = {0u, 0u, (int)m_parameters.firstLayer};
        region.imageExtent       = {extent.width, extent.height, 1u};
        vk.cmdCopyBufferToImage(*cmdBuffer, **srcBuffer, **image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1u, &region);
    }
    transitionUnusedLayers(vk, *cmdBuffer, **image);
    {
        VkImageMemoryBarrier imageMemoryBarriers[2];
        imageMemoryBarriers[0] = makeImageMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
                                                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
                                                        **image, makeSubresourceRange(m_parameters.firstLayer));
        imageMemoryBarriers[1] =
            makeImageMemoryBarrier(VK_ACCESS_NONE, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                                   VK_IMAGE_LAYOUT_GENERAL, **image, makeSubresourceRange(m_parameters.secondLayer));
        vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u,
                              nullptr, 0u, nullptr, 2u, imageMemoryBarriers);

        VkImageCopy region;
        region.srcSubresource = makeSubresourceLayers(0u);
        region.srcOffset      = {0u, 0u, (int)m_parameters.firstLayer};
        region.dstSubresource = makeSubresourceLayers(0u);
        region.dstOffset      = {0u, 0u, (int)m_parameters.secondLayer};
        region.extent         = copyExtent;
        vk.cmdCopyImage(*cmdBuffer, **image, VK_IMAGE_LAYOUT_GENERAL, **image, VK_IMAGE_LAYOUT_GENERAL, 1u, &region);
    }
    transitionUnusedLayers(vk, *cmdBuffer, **image);
    if (m_parameters.imageViewType == VK_IMAGE_VIEW_TYPE_3D)
        transitionAllLayers(vk, *cmdBuffer, **image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    {
        VkImageLayout previousLayout = m_parameters.imageViewType == VK_IMAGE_VIEW_TYPE_3D ?
                                           VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL :
                                           VK_IMAGE_LAYOUT_GENERAL;

        VkImageMemoryBarrier imageMemoryBarrier = makeImageMemoryBarrier(
            VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, previousLayout,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, **image, makeSubresourceRange(m_parameters.secondLayer));
        vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0u, 0u,
                              nullptr, 0u, nullptr, 1u, &imageMemoryBarrier);

        vk.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayout, 0u, 1u, &*descriptorSet,
                                 0u, nullptr);
        vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *computePipeline);
        vk.cmdDispatch(*cmdBuffer, extent.width, extent.height, 1u);
    }
    transitionUnusedLayers(vk, *cmdBuffer, **image);
    {
        VkImageSubresourceRange subresourceRange = makeSubresourceRange(m_parameters.secondLayer);
        VkImageMemoryBarrier imageMemoryBarrier  = makeImageMemoryBarrier(
            VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, **image, subresourceRange);
        vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u,
                              nullptr, 0u, nullptr, 1u, &imageMemoryBarrier);

        VkBufferImageCopy region;
        region.bufferOffset      = 0u;
        region.bufferRowLength   = 0u;
        region.bufferImageHeight = 0u;
        region.imageSubresource  = makeSubresourceLayers(0u);
        region.imageOffset       = {0u, 0u, (int)m_parameters.secondLayer};
        region.imageExtent       = {extent.width, extent.height, 1u};
        vk.cmdCopyImageToBuffer(*cmdBuffer, **image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, **dstBuffer, 1u, &region);
    }
    transitionUnusedLayers(vk, *cmdBuffer, **image);

    VkBufferMemoryBarrier bufferMemoryBarrier = makeBufferMemoryBarrier(
        VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT, **dstBuffer, 0u, layerSize);
    vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                          VK_PIPELINE_STAGE_HOST_BIT, 0u, 0u, nullptr, 1u, &bufferMemoryBarrier, 0u, nullptr);

    endCommandBuffer(vk, *cmdBuffer);
    submitCommandsAndWait(vk, device, queue, *cmdBuffer);

    auto &dstBufferAlloc  = dstBuffer->getAllocation();
    auto &ssboBufferAlloc = ssbo->getAllocation();
    uint8_t *srcPtr       = reinterpret_cast<uint8_t *>(srcBufferAlloc.getHostPtr());
    uint8_t *dstPtr       = reinterpret_cast<uint8_t *>(dstBufferAlloc.getHostPtr());
    float *ssboPtr        = reinterpret_cast<float *>(ssboBufferAlloc.getHostPtr());
    if (memcmp(srcPtr, dstPtr, layerSize) != 0)
    {
        for (uint32_t i = 0; i < layerSize; ++i)
        {
            if (srcPtr[i] != dstPtr[i])
            {
                log << tcu::TestLog::Message << "Mismatch at byte " << i << ". Src value: " << srcPtr[i]
                    << ", dst value: " << dstPtr[i] << "." << tcu::TestLog::EndMessage;
            }
        }
        const tcu::IVec3 imageDim(static_cast<int>(extent.width), static_cast<int>(extent.height),
                                  static_cast<int>(extent.depth));
        tcu::ConstPixelBufferAccess reference(vk::mapVkFormat(format), imageDim, srcBufferAlloc.getHostPtr());
        tcu::ConstPixelBufferAccess result(vk::mapVkFormat(format), imageDim, dstBufferAlloc.getHostPtr());
        log << tcu::TestLog::Image("Reference", "", reference);
        log << tcu::TestLog::Image("Result", "", result);
        return tcu::TestStatus::fail("Fail");
    }
    for (uint32_t i = 0; i < layerSize; ++i)
    {
        float srcValue      = float(srcPtr[i]);
        float ssboValue     = ssboPtr[i] * 256;
        const float epsilon = 1.0f;
        if (abs(srcValue - ssboValue) > epsilon)
        {
            return tcu::TestStatus::fail("Fail");
        }
    }

    return tcu::TestStatus::pass("Pass");
}

class ArrayCompatibleTestCase : public vkt::TestCase
{
public:
    ArrayCompatibleTestCase(tcu::TestContext &context, const char *name, const TestParameters &parameters)
        : TestCase(context, name)
        , m_parameters(parameters)
    {
    }

private:
    vkt::TestInstance *createInstance(vkt::Context &context) const
    {
        return new ArrayCompatibleTestInstance(context, m_parameters);
    }
    void checkSupport(vkt::Context &context) const;
    void initPrograms(SourceCollections &programCollection) const;

    const TestParameters m_parameters;
};

void ArrayCompatibleTestCase::checkSupport(vkt::Context &context) const
{
    context.requireDeviceFunctionality("VK_KHR_maintenance9");

    const vk::InstanceInterface &vki    = context.getInstanceInterface();
    vk::VkPhysicalDevice physicalDevice = context.getPhysicalDevice();

    vk::VkImageFormatProperties imageFormatProperties;
    if (vki.getPhysicalDeviceImageFormatProperties(
            physicalDevice, VK_FORMAT_R8G8B8A8_UNORM, vk::VK_IMAGE_TYPE_3D, m_parameters.tiling,
            VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            VK_IMAGE_CREATE_2D_ARRAY_COMPATIBLE_BIT, &imageFormatProperties) == vk::VK_ERROR_FORMAT_NOT_SUPPORTED)
        TCU_THROW(NotSupportedError, "Image format not supported.");

#ifndef CTS_USES_VULKANSC
    if (m_parameters.imageViewType == VK_IMAGE_VIEW_TYPE_2D)
    {
        context.requireDeviceFunctionality("VK_EXT_image_2d_view_of_3d");
        if (!context.getImage2DViewOf3DFeaturesEXT().sampler2DViewOf3D)
            TCU_THROW(NotSupportedError, "sampler2DViewOf3D not supported.");
    }
#endif
}

void ArrayCompatibleTestCase::initPrograms(SourceCollections &programCollection) const
{
    std::ostringstream comp;
    comp << "#version 450\n"
         << "\n"
         << "layout (local_size_x = 1, local_size_y = 1, local_size_z = 1) in;\n";
    if (m_parameters.imageViewType == VK_IMAGE_VIEW_TYPE_3D)
        comp << "layout (set = 0, binding = 0) uniform sampler3D inputImage;\n";
    else
        comp << "layout (set = 0, binding = 0) uniform sampler2D inputImage;\n";
    comp << "layout (set = 0, binding = 1) buffer outputBuffer {\n"
         << "    vec4 color[];\n"
         << "} data;\n"
         << "\n"
         << "void main() {\n";
    if (m_parameters.imageViewType == VK_IMAGE_VIEW_TYPE_3D)
    {
        comp << "    vec3 pixelCoords = vec3(gl_GlobalInvocationID.xy / vec2(32.0f, 32.0f), "
             << m_parameters.secondLayer << ".0f / " << m_parameters.totalLayers << ".0f);\n";
    }
    else
    {
        comp << "    vec2 pixelCoords = vec2(gl_GlobalInvocationID.xy / vec2(32.0f, 32.0f));\n";
    }
    comp << "    uint index = gl_GlobalInvocationID.y * 32 + gl_GlobalInvocationID.x;\n"
         << "    data.color[index] = texture(inputImage, pixelCoords);\n"
         << "}\n";

    programCollection.glslSources.add("comp") << glu::ComputeSource(comp.str());
}

} // namespace

tcu::TestCaseGroup *createImage2dArrayCompatibleTests(tcu::TestContext &testCtx)
{
    de::MovePtr<tcu::TestCaseGroup> testGroup(new tcu::TestCaseGroup(testCtx, "2d_array_compatible"));

    struct LayersTest
    {
        uint32_t first;
        uint32_t second;
        uint32_t total;
        const char *name;
    } tests[] = {
        {0u, 1u, 8u, "0_1_8"},
        {3u, 7u, 16u, "3_7_16"},
        {3u, 4u, 5u, "3_4_5"},
    };

    struct TilingTest
    {
        VkImageTiling tiling;
        const char *name;
    } tilingTests[] = {
        {VK_IMAGE_TILING_LINEAR, "linear"},
        {VK_IMAGE_TILING_OPTIMAL, "optimal"},
    };

    struct ImageViewTypeTest
    {
        VkImageViewType imageViewType;
        const char *name;
    } imageViewTypeTests[] = {
#ifndef CTS_USES_VULKANSC
        {VK_IMAGE_VIEW_TYPE_2D, "2d"},
#endif
        {VK_IMAGE_VIEW_TYPE_3D, "3d"},
    };

    for (const auto &test : tests)
    {
        de::MovePtr<tcu::TestCaseGroup> testLayerGroup(new tcu::TestCaseGroup(testCtx, test.name));
        for (const auto &tiling : tilingTests)
        {
            de::MovePtr<tcu::TestCaseGroup> tilingGroup(new tcu::TestCaseGroup(testCtx, tiling.name));
            for (const auto &imageViewType : imageViewTypeTests)
            {
                TestParameters parameters;
                parameters.firstLayer    = test.first;
                parameters.secondLayer   = test.second;
                parameters.totalLayers   = test.total;
                parameters.tiling        = tiling.tiling;
                parameters.imageViewType = imageViewType.imageViewType;

                tilingGroup->addChild(new ArrayCompatibleTestCase(testCtx, imageViewType.name, parameters));
            }
            testLayerGroup->addChild(tilingGroup.release());
        }

        testGroup->addChild(testLayerGroup.release());
    }

    return testGroup.release();
}

} // namespace image
} // namespace vkt
