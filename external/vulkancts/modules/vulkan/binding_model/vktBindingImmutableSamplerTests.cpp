/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2026 The Khronos Group Inc.
 * Copyright (c) 2026 Google LLC.
 * Copyright (c) 2026 LunarG, Inc.
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
 * \brief Tests for immutable samplers
 *//*--------------------------------------------------------------------*/

#include "vktBindingImmutableSamplerTests.hpp"

#include "vktTestCase.hpp"
#include "vktTestCaseUtil.hpp"
#include "../ycbcr/vktYCbCrUtil.hpp"
#include "vkBarrierUtil.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkBuilderUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkRefUtil.hpp"
#include "vkTypeUtil.hpp"
#include "tcuTestLog.hpp"
#include "tcuVector.hpp"
#include "deStringUtil.hpp"
#include "deUniquePtr.hpp"

#include <cmath>
#include <string>
#include <vector>

namespace vkt
{
namespace BindingModel
{
namespace
{

using namespace vk;

enum class TestType
{
    NULL_HANDLE = 0,
    GARBAGE,
    DESTROYED,
    OTHER,
};

struct TestParams
{
    VkFormat format;
    TestType testType;

    bool isYcbcr() const
    {
        return isYCbCrFormat(format);
    }
};

VkSamplerCreateInfo makeSamplerCreateInfo(const void *pNext, bool other, bool isYcbcr)
{
    const bool different  = (other && !isYcbcr);
    const VkFilter filter = different ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
    const VkSamplerAddressMode addressMode =
        different ? VK_SAMPLER_ADDRESS_MODE_REPEAT : VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

    VkSamplerCreateInfo createInfo = initVulkanStructureConst(pNext);
    createInfo.magFilter           = filter;
    createInfo.minFilter           = filter;
    createInfo.addressModeU        = addressMode;
    createInfo.addressModeV        = addressMode;
    createInfo.addressModeW        = addressMode;

    return createInfo;
}

struct Resource
{
    Move<VkSamplerYcbcrConversion> conversion;
    Move<VkSampler> sampler;
    Move<VkImageView> view;
};

class ImmutableSamplerTestInstance : public vkt::TestInstance
{
public:
    ImmutableSamplerTestInstance(Context &context, const TestParams &params)
        : vkt::TestInstance(context)
        , m_params(params)
    {
    }
    virtual ~ImmutableSamplerTestInstance() = default;

    tcu::TestStatus iterate() override;

private:
    void createResource(Resource &resource, VkImage image, bool other) const;
    uint32_t getCombinedImageSamplerDescriptorCount() const;
    VkSampler getIgnoredSampler(VkSampler otherSampler) const;
    std::vector<tcu::Vec4> execute(VkImageView view, VkSampler immutableSampler, VkSampler updatedSampler) const;

    const TestParams m_params;
};

void ImmutableSamplerTestInstance::createResource(Resource &resource, VkImage image, bool other) const
{
    const auto &vkd   = m_context.getDeviceInterface();
    const auto device = m_context.getDevice();
    const auto range  = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);

    if (!m_params.isYcbcr())
    {
        const auto samplerCreateInfo = makeSamplerCreateInfo(nullptr, other, false);

        resource.sampler = createSampler(vkd, device, &samplerCreateInfo);
        resource.view    = makeImageView(vkd, device, image, VK_IMAGE_VIEW_TYPE_2D, m_params.format, range);
    }
    else
    {
        const auto formatProperties = getPhysicalDeviceFormatProperties(m_context.getInstanceInterface(),
                                                                        m_context.getPhysicalDevice(), m_params.format);

        const VkChromaLocation chromaOffset =
            ((formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_COSITED_CHROMA_SAMPLES_BIT) != 0u) ?
                VK_CHROMA_LOCATION_COSITED_EVEN :
                VK_CHROMA_LOCATION_MIDPOINT;

        const VkSamplerYcbcrModelConversion ycbcrModel =
            other ? VK_SAMPLER_YCBCR_MODEL_CONVERSION_YCBCR_709 : VK_SAMPLER_YCBCR_MODEL_CONVERSION_RGB_IDENTITY;
        const VkSamplerYcbcrRange ycbcrRange =
            other ? VK_SAMPLER_YCBCR_RANGE_ITU_NARROW : VK_SAMPLER_YCBCR_RANGE_ITU_FULL;

        VkSamplerYcbcrConversionCreateInfo conversionCreateInfo = initVulkanStructure();
        conversionCreateInfo.format                             = m_params.format;
        conversionCreateInfo.ycbcrModel                         = ycbcrModel;
        conversionCreateInfo.ycbcrRange                         = ycbcrRange;
        conversionCreateInfo.xChromaOffset                      = chromaOffset;
        conversionCreateInfo.yChromaOffset                      = chromaOffset;
        resource.conversion = createSamplerYcbcrConversion(vkd, device, &conversionCreateInfo);

        VkSamplerYcbcrConversionInfo conversionInfo = initVulkanStructure();
        conversionInfo.conversion                   = *resource.conversion;

        const auto samplerCreateInfo = makeSamplerCreateInfo(&conversionInfo, other, true);
        resource.sampler             = createSampler(vkd, device, &samplerCreateInfo);

        VkImageViewCreateInfo viewCreateInfo = initVulkanStructure(&conversionInfo);
        viewCreateInfo.image                 = image;
        viewCreateInfo.viewType              = VK_IMAGE_VIEW_TYPE_2D;
        viewCreateInfo.format                = m_params.format;
        viewCreateInfo.subresourceRange      = range;

        resource.view = createImageView(vkd, device, &viewCreateInfo);
    }
}

uint32_t ImmutableSamplerTestInstance::getCombinedImageSamplerDescriptorCount() const
{
    if (!m_params.isYcbcr())
        return 1u;

    VkPhysicalDeviceImageFormatInfo2 formatInfo = initVulkanStructure();
    formatInfo.format                           = m_params.format;
    formatInfo.type                             = VK_IMAGE_TYPE_2D;
    formatInfo.usage                            = (VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);

    VkSamplerYcbcrConversionImageFormatProperties ycbcrProperties = initVulkanStructure();
    VkImageFormatProperties2 formatProperties                     = initVulkanStructure(&ycbcrProperties);
    m_context.getInstanceInterface().getPhysicalDeviceImageFormatProperties2(m_context.getPhysicalDevice(), &formatInfo,
                                                                             &formatProperties);

    return ycbcrProperties.combinedImageSamplerDescriptorCount;
}

VkSampler ImmutableSamplerTestInstance::getIgnoredSampler(VkSampler otherSampler) const
{
    if (m_params.testType == TestType::NULL_HANDLE)
    {
        return VK_NULL_HANDLE;
    }
    else if (m_params.testType == TestType::GARBAGE)
    {
        void *invalid = reinterpret_cast<void *>(static_cast<uintptr_t>(0xdeadbeefu));
        return VkSampler(invalid);
    }
    else if (m_params.testType == TestType::DESTROYED)
    {
        const auto createInfo = makeSamplerCreateInfo(nullptr, false, false);
        const auto sampler    = createSampler(m_context.getDeviceInterface(), m_context.getDevice(), &createInfo);

        return *sampler;
    }

    return otherSampler;
}

std::vector<tcu::Vec4> ImmutableSamplerTestInstance::execute(VkImageView view, VkSampler immutableSampler,
                                                             VkSampler updatedSampler) const
{
    const auto &vkd       = m_context.getDeviceInterface();
    const auto device     = m_context.getDevice();
    auto &allocator       = m_context.getDefaultAllocator();
    const auto queue      = m_context.getUniversalQueue();
    const auto queueIndex = m_context.getUniversalQueueFamilyIndex();

    const auto resultCount        = 8u * 8u;
    const VkDeviceSize bufferSize = static_cast<VkDeviceSize>(resultCount * sizeof(tcu::Vec4));

    BufferWithMemory resultsBuffer(vkd, device, allocator,
                                   makeBufferCreateInfo(bufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT),
                                   MemoryRequirement::HostVisible);
    deMemset(resultsBuffer.getAllocation().getHostPtr(), 0, static_cast<size_t>(bufferSize));
    flushAlloc(vkd, device, resultsBuffer.getAllocation());

    DescriptorSetLayoutBuilder layoutBuilder;
    layoutBuilder.addSingleSamplerBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT,
                                          &immutableSampler);
    layoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
    const auto descriptorSetLayout = layoutBuilder.build(vkd, device);

    DescriptorPoolBuilder poolBuilder;
    poolBuilder.addType(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, getCombinedImageSamplerDescriptorCount());
    poolBuilder.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);

    const auto descriptorPool = poolBuilder.build(vkd, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);
    const auto descriptorSet  = makeDescriptorSet(vkd, device, *descriptorPool, *descriptorSetLayout);

    const VkDescriptorImageInfo imageInfo = {updatedSampler, view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    const auto bufferInfo                 = makeDescriptorBufferInfo(*resultsBuffer, 0u, bufferSize);

    DescriptorSetUpdateBuilder()
        .writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u),
                     VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &imageInfo)
        .writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(1u),
                     VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &bufferInfo)
        .update(vkd, device);

    const auto shaderModule   = createShaderModule(vkd, device, m_context.getBinaryCollection().get("comp"));
    const auto pipelineLayout = makePipelineLayout(vkd, device, *descriptorSetLayout);
    const auto pipeline       = makeComputePipeline(vkd, device, *pipelineLayout, *shaderModule);

    const auto cmdPool   = makeCommandPool(vkd, device, queueIndex);
    const auto cmdBuffer = allocateCommandBuffer(vkd, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

    beginCommandBuffer(vkd, *cmdBuffer);
    vkd.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipeline);
    vkd.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayout, 0u, 1u, &*descriptorSet, 0u,
                              nullptr);
    vkd.cmdDispatch(*cmdBuffer, 1u, 1u, 1u);
    const auto barrier = makeMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
    cmdPipelineMemoryBarrier(vkd, *cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                             &barrier);
    endCommandBuffer(vkd, *cmdBuffer);
    submitCommandsAndWait(vkd, device, queue, *cmdBuffer);

    invalidateAlloc(vkd, device, resultsBuffer.getAllocation());

    std::vector<tcu::Vec4> results(resultCount);
    deMemcpy(results.data(), resultsBuffer.getAllocation().getHostPtr(), static_cast<size_t>(bufferSize));

    return results;
}

tcu::TestStatus ImmutableSamplerTestInstance::iterate()
{
    const auto &vkd   = m_context.getDeviceInterface();
    const auto device = m_context.getDevice();
    auto &allocator   = m_context.getDefaultAllocator();
    tcu::TestLog &log = m_context.getTestContext().getLog();

    VkImageCreateInfo imageCreateInfo = initVulkanStructure();
    imageCreateInfo.imageType         = VK_IMAGE_TYPE_2D;
    imageCreateInfo.format            = m_params.format;
    imageCreateInfo.extent            = makeExtent3D(8u, 8u, 1u);
    imageCreateInfo.mipLevels         = 1u;
    imageCreateInfo.arrayLayers       = 1u;
    imageCreateInfo.samples           = VK_SAMPLE_COUNT_1_BIT;
    imageCreateInfo.usage             = (VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
    const auto image                  = createImage(vkd, device, &imageCreateInfo);

    const std::vector<ycbcr::AllocationSp> allocations(
        ycbcr::allocateAndBindImageMemory(vkd, device, allocator, *image, m_params.format, 0u));
    ycbcr::MultiPlaneImageData imageData(m_params.format, tcu::UVec2(8u, 8u));
    ycbcr::fillGradient(&imageData, tcu::Vec4(0.2f, 0.1f, 0.3f, 1.0f), tcu::Vec4(0.8f, 0.9f, 0.7f, 1.0f));
    ycbcr::uploadImage(vkd, device, m_context.getUniversalQueueFamilyIndex(), allocator, *image, imageData,
                       VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    Resource resource;
    createResource(resource, *image, false);
    Resource otherResource;
    createResource(otherResource, *image, true);

    const auto reference      = execute(*resource.view, *resource.sampler, *resource.sampler);
    const auto ignoredSampler = getIgnoredSampler(*otherResource.sampler);
    const auto result         = execute(*resource.view, *resource.sampler, ignoredSampler);

    float diff = 0.0f;
    for (size_t i = 0; i < reference.size(); ++i)
        for (int c = 0; c < 4; ++c)
            diff = de::max(diff, std::fabs(reference[i][c] - result[i][c]));

    if (diff > 0.02f)
    {
        log << tcu::TestLog::Message << "Immutable sampler was updated and produced different results" << diff
            << tcu::TestLog::EndMessage;

        return tcu::TestStatus::fail("Fail");
    }

    return tcu::TestStatus::pass("Pass");
}

class ImmutableSamplerTestCase : public vkt::TestCase
{
public:
    ImmutableSamplerTestCase(tcu::TestContext &testCtx, const std::string &name, const TestParams &params)
        : vkt::TestCase(testCtx, name)
        , m_params(params)
    {
    }
    virtual ~ImmutableSamplerTestCase() = default;

    void initPrograms(SourceCollections &programCollection) const override;
    void checkSupport(Context &context) const override;
    TestInstance *createInstance(Context &context) const override
    {
        return new ImmutableSamplerTestInstance(context, m_params);
    }

private:
    const TestParams m_params;
};

void ImmutableSamplerTestCase::initPrograms(SourceCollections &programCollection) const
{
    const std::string comp = "#version 450\n"
                             "layout(local_size_x=8, local_size_y=8) in;\n"
                             "layout(set=0, binding=0) uniform sampler2D image;\n"
                             "layout(set=0, binding=1, std430) writeonly buffer Buffer {\n"
                             "    vec4 values[];\n"
                             "} buf;\n"
                             "void main () {\n"
                             "    uvec2 coord = gl_GlobalInvocationID.xy;\n"
                             "    vec2 uv = (vec2(coord) + 0.5f) / 8.0f * 1.5f - 0.25f;\n"
                             "    buf.values[coord.y * 8u + coord.x] = texture(image, uv);\n"
                             "}\n";

    programCollection.glslSources.add("comp") << glu::ComputeSource(comp);
}

void ImmutableSamplerTestCase::checkSupport(Context &context) const
{
    if (m_params.isYcbcr())
    {
        context.requireDeviceFunctionality("VK_KHR_sampler_ycbcr_conversion");
        ycbcr::checkImageSupport(context, m_params.format, 0u);
    }
    else
    {
        VkFormatFeatureFlags required = (VK_FORMAT_FEATURE_TRANSFER_DST_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT);
        const auto formatProperties   = getPhysicalDeviceFormatProperties(context.getInstanceInterface(),
                                                                          context.getPhysicalDevice(), m_params.format);

        if ((formatProperties.optimalTilingFeatures & required) != required)
            TCU_THROW(NotSupportedError, "Format does not support the required features");
    }
}

std::string getFormatCaseName(VkFormat format)
{
    return de::toLower(de::toString(getFormatStr(format)).substr(10));
}

} // namespace

tcu::TestCaseGroup *createImmutableSamplerTests(tcu::TestContext &testCtx)
{
    de::MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(testCtx, "immutable_sampler"));

    const struct
    {
        TestType testType;
        const char *name;
    } testTypes[] = {
        {TestType::NULL_HANDLE, "null"},
        {TestType::GARBAGE, "garbage"},
        {TestType::DESTROYED, "destroyed"},
        {TestType::OTHER, "other"},
    };

    const VkFormat formats[] = {
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM,
    };

    for (const auto &testType : testTypes)
    {
        de::MovePtr<tcu::TestCaseGroup> typeGroup(new tcu::TestCaseGroup(testCtx, testType.name));
        for (const auto format : formats)
        {
            TestParams params;
            params.format   = format;
            params.testType = testType.testType;

            typeGroup->addChild(new ImmutableSamplerTestCase(testCtx, getFormatCaseName(format), params));
        }
        group->addChild(typeGroup.release());
    }

    return group.release();
}

} // namespace BindingModel
} // namespace vkt
