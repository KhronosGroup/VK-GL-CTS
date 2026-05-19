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
* \file
* \brief Descriptor Indexing misc. tests
*//*--------------------------------------------------------------------*/
#include "vktDescriptorIndexingMiscTests.hpp"
#include "deDefs.h"
#include "vktTestCase.hpp"

#include "vkBufferWithMemory.hpp"
#include "vkBuilderUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkRefUtil.hpp"
#include "vkTypeUtil.hpp"

#include "vkImageUtil.hpp"
#include "vkImageWithMemory.hpp"
#include "vkObjUtil.hpp"
#include "vkBarrierUtil.hpp"
#include "vkDefs.hpp"
#include "vkPrograms.hpp"

#include "tcuTestCase.hpp"
#include "tcuTestContext.hpp"
#include "tcuVectorType.hpp"
#include "tcuRGBA.hpp"
#include "tcuTexture.hpp"

#include "deUniquePtr.hpp"
#include "deSharedPtr.hpp"

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <string>
#include <vector>

namespace vkt
{
namespace DescriptorIndexing
{
using namespace vk;
using namespace tcu;

namespace
{

struct TestParams
{
    VkFormat format;
    uint32_t numElementsPerArray;
    Vec2 coordinate;
};

const uint32_t kNumArrays = 3u;

class CommonNonUniformDescriptorIndexTestCase : public TestCase
{
public:
    CommonNonUniformDescriptorIndexTestCase(TestContext &testCtx, const std::string &name, const TestParams &params);
    ~CommonNonUniformDescriptorIndexTestCase(void);
    void checkSupport(Context &context) const;
    void initPrograms(SourceCollections &programCollection) const;
    TestInstance *createInstance(Context &context) const;

private:
    const TestParams m_params;
};

CommonNonUniformDescriptorIndexTestCase::CommonNonUniformDescriptorIndexTestCase(TestContext &testCtx,
                                                                                 const std::string &name,
                                                                                 const TestParams &params)
    : TestCase(testCtx, name.c_str())
    , m_params(params)
{
}

CommonNonUniformDescriptorIndexTestCase::~CommonNonUniformDescriptorIndexTestCase(void)
{
}

void CommonNonUniformDescriptorIndexTestCase::checkSupport(Context &context) const
{
    const auto &vki        = context.getInstanceInterface();
    const auto &physDevice = context.getPhysicalDevice();

    const VkPhysicalDeviceDescriptorIndexingFeatures &indexingFeatures = context.getDescriptorIndexingFeatures();

    context.requireDeviceFunctionality("VK_EXT_descriptor_indexing");
    if (!indexingFeatures.runtimeDescriptorArray)
        TCU_THROW(NotSupportedError, "runtimeDescriptorArray is not supported.");
    if (!indexingFeatures.shaderSampledImageArrayNonUniformIndexing)
        TCU_THROW(NotSupportedError, "Non-uniform indexing over sampled image descriptor arrays is not supported.");

    const uint32_t maxPerStageDescriptorSampledImages =
        getPhysicalDeviceProperties(vki, physDevice).limits.maxPerStageDescriptorSampledImages;
    const uint32_t numSampledImages = kNumArrays * m_params.numElementsPerArray;

    if (numSampledImages > maxPerStageDescriptorSampledImages)
        TCU_THROW(NotSupportedError, "Too many per stage descriptors.");

    VkImageFormatProperties imageFormatProperties;
    const auto result = vki.getPhysicalDeviceImageFormatProperties(
        physDevice, m_params.format, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL,
        (VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT), 0u, &imageFormatProperties);

    if (result == VK_ERROR_FORMAT_NOT_SUPPORTED)
    {
        TCU_THROW(NotSupportedError, "Image format is not supported for required usage");
    }
}

void CommonNonUniformDescriptorIndexTestCase::initPrograms(SourceCollections &programCollection) const
{
    const auto localSizeX           = m_params.numElementsPerArray;
    const std::string coordinateStr = "vec2(" + de::floatToString(m_params.coordinate.x(), 1) + "," +
                                      de::floatToString(m_params.coordinate.x(), 1) + ")";

    DE_ASSERT(kNumArrays == 3u);

    std::ostringstream comp;
    {
        comp << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
             << "#extension GL_EXT_nonuniform_qualifier : require\n"
             << "\n"
             << "layout(local_size_x = " << localSizeX << ") in;\n"
             << "\n"
             << "layout(set = 0, binding = 0) uniform utexture2D Tex0[];\n"
             << "layout(set = 1, binding = 0) uniform utexture2D Tex1[];\n"
             << "layout(set = 2, binding = 0) uniform utexture2D Tex2[];\n"
             << "layout(set = 3, binding = 0) writeonly buffer SSBO { uvec4 data[]; };\n"
             << "layout(set = 3, binding = 1) uniform sampler Samp;\n"
             << "\n"
             << "void main()\n"
             << "{\n"
             << "    uint index = gl_GlobalInvocationID.x;\n"
             << "    uvec4 a = textureLod(nonuniformEXT(usampler2D(Tex0[index], Samp)), " << coordinateStr
             << ", 0.0);\n"
             << "    uvec4 b = textureLod(nonuniformEXT(usampler2D(Tex1[index], Samp)), " << coordinateStr
             << ", 0.0);\n"
             << "    uvec4 c = textureLod(nonuniformEXT(usampler2D(Tex2[index], Samp)), " << coordinateStr
             << ", 0.0);\n"
             << "    data[index] = a * b + c;\n"
             << "}\n";
    }
    programCollection.glslSources.add("comp") << glu::ComputeSource(comp.str());
}

class CommonNonUniformDescriptorIndexTestInstance : public TestInstance
{
public:
    CommonNonUniformDescriptorIndexTestInstance(Context &context, const TestParams &params);
    ~CommonNonUniformDescriptorIndexTestInstance(void);
    void prepareDescriptors();
    TestStatus iterate(void);

private:
    const TestParams m_params;
};

TestInstance *CommonNonUniformDescriptorIndexTestCase::createInstance(Context &context) const
{
    return new CommonNonUniformDescriptorIndexTestInstance(context, m_params);
}

CommonNonUniformDescriptorIndexTestInstance::CommonNonUniformDescriptorIndexTestInstance(Context &context,
                                                                                         const TestParams &params)
    : TestInstance(context)
    , m_params(params)
{
}

CommonNonUniformDescriptorIndexTestInstance::~CommonNonUniformDescriptorIndexTestInstance(void)
{
}

VkImageCreateInfo makeImageCreateInfo(const VkImageType &imageType, const UVec2 imageSize, const VkFormat format,
                                      const VkImageUsageFlags usage, const VkImageCreateFlags flags,
                                      const VkImageTiling tiling)
{
    const VkImageCreateInfo imageParams = {
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,            // VkStructureType sType;
        nullptr,                                        // const void* pNext;
        flags,                                          // VkImageCreateFlags flags;
        imageType,                                      // VkImageType imageType;
        format,                                         // VkFormat format;
        makeExtent3D(imageSize.x(), imageSize.y(), 1u), // VkExtent3D extent;
        1u,                                             // uint32_t mipLevels;
        1u,                                             // uint32_t arrayLayers;
        VK_SAMPLE_COUNT_1_BIT,                          // VkSampleCountFlagBits samples;
        tiling,                                         // VkImageTiling tiling;
        usage,                                          // VkImageUsageFlags usage;
        VK_SHARING_MODE_EXCLUSIVE,                      // VkSharingMode sharingMode;
        0u,                                             // uint32_t queueFamilyIndexCount;
        nullptr,                                        // const uint32_t* pQueueFamilyIndices;
        VK_IMAGE_LAYOUT_UNDEFINED,                      // VkImageLayout initialLayout;
    };
    return imageParams;
}

Move<VkSampler> makeSampler(const DeviceInterface &vkd, const VkDevice device)
{
    const VkSamplerCreateInfo samplerParams = {
        VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,   // VkStructureType sType;
        nullptr,                                 // const void* pNext;
        (VkSamplerCreateFlags)0,                 // VkSamplerCreateFlags flags;
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
        VK_COMPARE_OP_ALWAYS,                    // VkCompareOp compareOp;
        0.0f,                                    // float minLod;
        0.0f,                                    // float maxLod;
        VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK, // VkBorderColor borderColor;
        VK_FALSE,                                // VkBool32 unnormalizedCoordinates;
    };

    return createSampler(vkd, device, &samplerParams);
}

void populateColorBuffer(const DeviceInterface &vkd, const VkDevice device, const BufferWithMemory &colorBuffer,
                         const TextureFormat &format, const UVec2 &extent, const uint32_t baseColorValue)
{
    const IVec2 dim{static_cast<int32_t>(extent.x()), static_cast<int32_t>(extent.y())};
    auto &colorBufferAlloc = colorBuffer.getAllocation();
    auto colorBufferPtr    = reinterpret_cast<char *>(colorBufferAlloc.getHostPtr()) + colorBufferAlloc.getOffset();

    const PixelBufferAccess colorBufferPixels{format, dim[0], dim[1], 1, colorBufferPtr};

    const int32_t W = colorBufferPixels.getWidth();
    const int32_t H = colorBufferPixels.getHeight();
    const int32_t D = colorBufferPixels.getDepth();

    IVec4 gray = tcu::RGBA::gray().toIVec();
    const IVec4 color(gray.x() + (int32_t)baseColorValue, gray.y(), gray.z(), gray.w());

    for (int32_t x = 0; x < W; ++x)
        for (int32_t y = 0; y < H; ++y)
            for (int32_t z = 0; z < D; ++z)
            {
                colorBufferPixels.setPixel(color, x, y, z);
            }

    flushAlloc(vkd, device, colorBufferAlloc);
}

const UVec4 getColor(const DeviceInterface &vkd, const VkDevice device, const BufferWithMemory &colorBuffer,
                     const TextureFormat &format, const UVec2 &extent, const Vec2 &coordinate)
{
    const IVec2 dim{static_cast<int32_t>(extent.x()), static_cast<int32_t>(extent.y())};
    auto &colorBufferAlloc = colorBuffer.getAllocation();
    auto colorBufferPtr    = reinterpret_cast<char *>(colorBufferAlloc.getHostPtr()) + colorBufferAlloc.getOffset();

    const PixelBufferAccess colorBufferPixels{format, dim[0], dim[1], 1, colorBufferPtr};

    const int32_t W           = colorBufferPixels.getWidth();
    const int32_t H           = colorBufferPixels.getHeight();
    const IVec2 coordinateInt = IVec2(static_cast<int32_t>(coordinate.x() * static_cast<float>(W)),
                                      static_cast<int32_t>(coordinate.y() * static_cast<float>(H)));

    const UVec4 color = colorBufferPixels.getPixelUint(coordinateInt.x(), coordinateInt.y());

    flushAlloc(vkd, device, colorBufferAlloc);

    return color;
}

TestStatus CommonNonUniformDescriptorIndexTestInstance::iterate(void)
{
    const auto &vkd             = m_context.getDeviceInterface();
    const auto device           = m_context.getDevice();
    const auto queue            = m_context.getUniversalQueue();
    const auto queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();
    auto &allocator             = m_context.getDefaultAllocator();

    const VkFormat format              = m_params.format;
    const uint32_t numArrays           = kNumArrays;
    const uint32_t numElementsPerArray = m_params.numElementsPerArray;
    const uint32_t numElementsTotal    = numArrays * numElementsPerArray;

    const auto sampledImageUsage      = (VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
    const auto sampledImageType       = VK_IMAGE_TYPE_2D;
    const auto sampledImageViewType   = VK_IMAGE_VIEW_TYPE_2D;
    const auto sampledImageFormat     = mapVkFormat(format);
    const auto sampledImageSize       = UVec2(numElementsPerArray, numElementsPerArray);
    const auto sampledImageLayout     = VK_IMAGE_LAYOUT_GENERAL;
    const auto sampledImageCoordinate = m_params.coordinate;

    // Create sampled images
    std::vector<std::vector<de::SharedPtr<ImageWithMemory>>> sampledImages(numArrays);

    // Create elements per array
    for (uint32_t arrayIdx = 0u; arrayIdx < numArrays; arrayIdx++)
    {
        sampledImages[arrayIdx].resize(numElementsPerArray);
    }

    // Create sampled image for each element
    for (uint32_t arrayIdx = 0u; arrayIdx < numArrays; arrayIdx++)
    {
        std::vector<de::SharedPtr<ImageWithMemory>> &sampledImagesArray = sampledImages[arrayIdx];

        for (uint32_t elemIdx = 0u; elemIdx < numElementsPerArray; elemIdx++)
        {
            const auto sampledImageCreateInfo = makeImageCreateInfo(sampledImageType, sampledImageSize, format,
                                                                    sampledImageUsage, 0u, VK_IMAGE_TILING_OPTIMAL);
            const de::SharedPtr<ImageWithMemory> sampledImage = de::SharedPtr<ImageWithMemory>(
                new ImageWithMemory(vkd, device, allocator, sampledImageCreateInfo, MemoryRequirement::Any));

            sampledImagesArray[elemIdx] = sampledImage;
        }
    }

    // Initialize the color buffers for sampled images
    std::vector<de::SharedPtr<BufferWithMemory>> colorBuffers(numElementsTotal);
    const auto colorBufferSize = getPixelSize(sampledImageFormat) * sampledImageSize.x() * sampledImageSize.y();
    {
        const auto colorBufferCreateInfo = makeBufferCreateInfo(colorBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                                                                     VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
        for (uint32_t elemIdx = 0u; elemIdx < numElementsTotal; elemIdx++)
        {
            colorBuffers[elemIdx] = de::SharedPtr<BufferWithMemory>(
                new BufferWithMemory(vkd, device, allocator, colorBufferCreateInfo, MemoryRequirement::HostVisible));

            populateColorBuffer(vkd, device, *colorBuffers[elemIdx], sampledImageFormat, sampledImageSize, elemIdx);
        }
    }

    // Output data buffer
    const VkDeviceSize outBuffSizeBytes = sizeof(UVec4) * numElementsPerArray;
    BufferWithMemory outBuffer{
        vkd, device, allocator,
        makeBufferCreateInfo(outBuffSizeBytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT),
        MemoryRequirement::HostVisible};

    // Initialize output buffer with 0xFF
    {
        std::vector<uint8_t> randomData(static_cast<uint32_t>(outBuffSizeBytes), 0xFF);
        const auto &outBufferAlloc = outBuffer.getAllocation();
        deMemcpy(outBufferAlloc.getHostPtr(), randomData.data(), static_cast<uint32_t>(outBuffSizeBytes));
        flushAlloc(vkd, device, outBufferAlloc);
    }

    // Corresponding image views
    std::vector<Move<VkImageView>> sampledImageViews(numElementsTotal);
    const auto colorSubresourceRange = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);

    uint32_t viewIdx = 0u;
    for (uint32_t arrayIdx = 0u; arrayIdx < numArrays; arrayIdx++)
    {
        std::vector<de::SharedPtr<ImageWithMemory>> &sampledImagesArray = sampledImages[arrayIdx];

        for (uint32_t elemIdx = 0u; elemIdx < numElementsPerArray; elemIdx++)
        {
            sampledImageViews[viewIdx] = makeImageView(vkd, device, **sampledImagesArray[elemIdx], sampledImageViewType,
                                                       format, colorSubresourceRange);
            viewIdx++;
        }
    }

    // Create the single sampler
    Move<VkSampler> sampler = makeSampler(vkd, device);

    // Descriptors setup
    VkDescriptorPoolCreateFlags descPoolCreateFlags           = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    VkDescriptorSetLayoutCreateFlags descSetLayoutCreateFlags = 0u;

    const auto descType      = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    const auto descStageMask = VK_SHADER_STAGE_COMPUTE_BIT;
    const auto numDescSets   = numArrays + 1u;

    std::vector<DescriptorSetLayoutBuilder> layoutBuilderInputSampledImages(numArrays);
    std::vector<Move<VkDescriptorSetLayout>> descriptorSetLayoutInputSampledImages(numArrays);
    Move<VkDescriptorSetLayout> descriptorSetLayoutOther;
    DescriptorSetLayoutBuilder layoutBuilderOther;
    std::vector<VkDescriptorSetLayout> descriptorSetLayouts;
    Move<VkDescriptorPool> descriptorPool;
    std::vector<Move<VkDescriptorSet>> descriptorSetPtrs(numDescSets);
    std::vector<VkDescriptorSet> descriptorSets;
    std::vector<DescriptorSetUpdateBuilder> descriptorUpdateBuilderInputSampledImages(numArrays);
    DescriptorSetUpdateBuilder descriptorUpdateBuilderOther;

    // Descriptor set layout for each input sampled image array
    for (uint32_t arrayIdx = 0u; arrayIdx < numArrays; arrayIdx++)
    {
        layoutBuilderInputSampledImages[arrayIdx].addArrayBinding(descType, numElementsPerArray, descStageMask);
        descriptorSetLayoutInputSampledImages[arrayIdx] =
            layoutBuilderInputSampledImages[arrayIdx].build(vkd, device, descSetLayoutCreateFlags);
    }

    layoutBuilderOther.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, descStageMask);
    layoutBuilderOther.addSingleSamplerBinding(VK_DESCRIPTOR_TYPE_SAMPLER, descStageMask, &*sampler);
    descriptorSetLayoutOther = layoutBuilderOther.build(vkd, device, descSetLayoutCreateFlags);

    // Descriptor pool
    DescriptorPoolBuilder poolBuilder;
    poolBuilder.addType(descType, numElementsTotal);
    poolBuilder.addType(VK_DESCRIPTOR_TYPE_SAMPLER);
    poolBuilder.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
    descriptorPool = poolBuilder.build(vkd, device, descPoolCreateFlags, numDescSets);

    // Descriptor sets
    for (uint32_t arrayIdx = 0u; arrayIdx < numArrays; arrayIdx++)
    {
        descriptorSetPtrs[arrayIdx] =
            makeDescriptorSet(vkd, device, descriptorPool.get(), *descriptorSetLayoutInputSampledImages[arrayIdx]);
    }
    descriptorSetPtrs[numDescSets - 1] =
        makeDescriptorSet(vkd, device, descriptorPool.get(), *descriptorSetLayoutOther);

    // Register descriptors in the update builder
    std::vector<VkDescriptorImageInfo> sampledImageArrayDescInfos(numElementsTotal);
    for (uint32_t elemIdx = 0u; elemIdx < numElementsTotal; elemIdx++)
    {
        sampledImageArrayDescInfos[elemIdx] =
            makeDescriptorImageInfo(VK_NULL_HANDLE, *sampledImageViews[elemIdx], sampledImageLayout);
    }

    for (uint32_t arrayIdx = 0u; arrayIdx < numArrays; arrayIdx++)
    {
        const uint32_t offset                       = arrayIdx * numElementsPerArray;
        const VkDescriptorImageInfo *descImageInfos = de::dataOrNull(sampledImageArrayDescInfos) + offset;
        descriptorUpdateBuilderInputSampledImages[arrayIdx].writeArray(
            *descriptorSetPtrs[arrayIdx], DescriptorSetUpdateBuilder::Location::binding(0u), descType,
            numElementsPerArray, &descImageInfos[0]);
    }

    const VkDescriptorBufferInfo outDescriptorInfo = makeDescriptorBufferInfo(*outBuffer, 0ull, outBuffSizeBytes);
    descriptorUpdateBuilderOther.writeSingle(*descriptorSetPtrs[numDescSets - 1],
                                             DescriptorSetUpdateBuilder::Location::binding(0u),
                                             VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &outDescriptorInfo);

    // Update descriptor set with the descriptor
    for (uint32_t arrayIdx = 0u; arrayIdx < numArrays; arrayIdx++)
        descriptorUpdateBuilderInputSampledImages[arrayIdx].update(vkd, device);
    descriptorUpdateBuilderOther.update(vkd, device);

    // Command pool and command buffer
    const auto commandPool =
        createCommandPool(vkd, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex);
    const auto commandBufferPtr = allocateCommandBuffer(vkd, device, *commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    const auto commandBuffer    = *commandBufferPtr;

    // Combine all descriptor set layouts
    descriptorSetLayoutInputSampledImages.reserve(numArrays + 1u);
    std::transform(descriptorSetLayoutInputSampledImages.begin(), descriptorSetLayoutInputSampledImages.end(),
                   std::back_inserter(descriptorSetLayouts),
                   [](const Move<VkDescriptorSetLayout> &layout) { return layout.get(); });

    descriptorSetLayouts.push_back(*descriptorSetLayoutOther);

    // Get the descriptor sets from ptrs
    descriptorSets.reserve(numDescSets);
    std::transform(descriptorSetPtrs.begin(), descriptorSetPtrs.end(), std::back_inserter(descriptorSets),
                   [](const Move<VkDescriptorSet> &descSet) { return descSet.get(); });

    // Create pipeline layout
    const Unique<VkPipelineLayout> pipelineLayout(makePipelineLayout(vkd, device, descriptorSetLayouts));

    // Shader modules
    const Unique<VkShaderModule> computeModule(
        createShaderModule(vkd, device, m_context.getBinaryCollection().get("comp"), 0));

    // Create compute pipeline
    const auto pipeline = makeComputePipeline(vkd, device, *pipelineLayout, *computeModule);

    beginCommandBuffer(vkd, commandBuffer);
    {
        vkd.cmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipeline);

        // Copy all buffers to image to initialize images
        {
            const std::vector<VkBufferImageCopy> copyRegions(
                1, makeBufferImageCopy(makeExtent3D(sampledImageSize.x(), sampledImageSize.y(), 1u),
                                       makeDefaultImageSubresourceLayers()));

            uint32_t colorBufferIdx = 0;
            for (uint32_t arrayIdx = 0u; arrayIdx < numArrays; arrayIdx++)
            {
                for (uint32_t elemIdx = 0u; elemIdx < numElementsPerArray; elemIdx++)
                {
                    copyBufferToImage(vkd, commandBuffer, **colorBuffers[colorBufferIdx], colorBufferSize, copyRegions,
                                      VK_IMAGE_ASPECT_COLOR_BIT, 1u, 1u, **sampledImages[arrayIdx][elemIdx],
                                      VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                      VK_ACCESS_SHADER_READ_BIT, 0u);
                    colorBufferIdx++;
                }
            }
        }

        // Output buffer barrier (pre)
        {
            const auto outBufferBarrier = makeBufferMemoryBarrier(VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_SHADER_WRITE_BIT,
                                                                  *outBuffer, 0u, outBuffSizeBytes);
            vkd.cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                   (VkDependencyFlags)0u, 0u, nullptr, 1u, &outBufferBarrier, 0u, nullptr);
        }

        vkd.cmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayout, 0u, numDescSets,
                                  de::dataOrNull(descriptorSets), 0u, nullptr);
        vkd.cmdDispatch(commandBuffer, 1u, 1u, 1u);

        // Output buffer barrier (post)
        {
            const auto outBufferBarrier = makeBufferMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT,
                                                                  *outBuffer, 0u, outBuffSizeBytes);
            vkd.cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                                   (VkDependencyFlags)0u, 0u, nullptr, 1u, &outBufferBarrier, 0u, nullptr);
        }
    }
    endCommandBuffer(vkd, commandBuffer);
    submitCommandsAndWait(vkd, device, queue, commandBuffer);

    const auto &outBufferAlloc = outBuffer.getAllocation();
    invalidateAlloc(vkd, device, outBufferAlloc);

    // Validate results
    const UVec4 *outBufferPtr = static_cast<UVec4 *>(outBufferAlloc.getHostPtr());

    // Initialize reference data
    std::vector<UVec4> total(numElementsPerArray);
    {
        for (uint32_t elemIdx = 0u; elemIdx < numElementsPerArray; elemIdx++)
        {
            for (uint32_t arrayIdx = 0u; arrayIdx < (numArrays - 2u); arrayIdx++)
            {
                uint32_t offset   = (arrayIdx * numElementsPerArray) + elemIdx;
                const UVec4 color = getColor(vkd, device, *colorBuffers[offset], sampledImageFormat, sampledImageSize,
                                             sampledImageCoordinate);
                total[elemIdx] += color;
            }
            {
                // numArrays - 2u
                uint32_t offset   = ((numArrays - 2u) * numElementsPerArray) + elemIdx;
                const UVec4 color = getColor(vkd, device, *colorBuffers[offset], sampledImageFormat, sampledImageSize,
                                             sampledImageCoordinate);
                total[elemIdx]    = total[elemIdx] * color;
            }
            {
                // numArrays - 1u
                uint32_t offset   = ((numArrays - 1u) * numElementsPerArray) + elemIdx;
                const UVec4 color = getColor(vkd, device, *colorBuffers[offset], sampledImageFormat, sampledImageSize,
                                             sampledImageCoordinate);
                total[elemIdx] += color;
            }
        }
    }

    for (uint32_t dataIdx = 0u; dataIdx < numElementsPerArray; dataIdx++)
    {
        const UVec4 ref = total[dataIdx];
        const UVec4 res = outBufferPtr[dataIdx];

        if (deMemCmp(&ref, &res, sizeof(UVec4)) != 0)
            return TestStatus::fail("Fail");
    }

    return TestStatus::pass("Pass");
}

} // namespace

void createDescriptorIndexingMiscTests(TestCaseGroup *mainGroup)
{
    TestContext &testContext = mainGroup->getTestContext();

    struct
    {
        Vec2 coordinate;
        std::string name;

    } testCoords[] = {{Vec2(0.0f, 0.0f), "0"}, {Vec2(0.5f, 0.5f), "mid"}};

    for (const auto arraySize : {8u, 64u})
    {
        for (const auto &coord : testCoords)
        {
            TestParams params = {
                VK_FORMAT_R32G32B32A32_UINT, // VkFormat format;
                arraySize,                   // uint32_t numElementsPerArray;
                coord.coordinate             // Vec2 coordinate;
            };

            const std::string testName =
                "misc_common_nonuniform_index_arraysize_" + de::toString(arraySize) + "_at_" + coord.name;

            mainGroup->addChild(new CommonNonUniformDescriptorIndexTestCase(testContext, testName, params));
        }
    }
}

} // namespace DescriptorIndexing
} // namespace vkt
