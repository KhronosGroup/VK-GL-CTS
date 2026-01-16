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
 * \brief Image processing base class
 *//*--------------------------------------------------------------------*/

#include "vktImageProcessingBase.hpp"
#include "vktImageProcessingTests.hpp"
#include "vktImageProcessingTestsUtil.hpp"
#include "vktTestCase.hpp"

#include "vkImageUtil.hpp"
#include "vkPipelineConstructionUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkBarrierUtil.hpp"

#include "deRandom.hpp"

#include "tcuImageCompare.hpp"
#include "tcuRGBA.hpp"
#include "tcuVectorType.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuVectorUtil.hpp"

#include "gluShaderProgram.hpp"

#include <cstddef>
#include <string>
#include <vector>

using namespace vk;
using namespace tcu;

namespace vkt
{
namespace ImageProcessing
{

VkVertexInputBindingDescription VertexData::getBindingDescription(void)
{
    static const VkVertexInputBindingDescription desc = {
        0u,                                        // uint32_t binding;
        static_cast<uint32_t>(sizeof(VertexData)), // uint32_t stride;
        VK_VERTEX_INPUT_RATE_VERTEX,               // VkVertexInputRate inputRate;
    };

    return desc;
}

std::vector<VkVertexInputAttributeDescription> VertexData::getAttributeDescriptions(void)
{
    static const std::vector<VkVertexInputAttributeDescription> desc = {{
        0u,                                                     // uint32_t location;
        0u,                                                     // uint32_t binding;
        VK_FORMAT_R32G32_SFLOAT,                                // VkFormat format;
        static_cast<uint32_t>(offsetof(VertexData, positions)), // uint32_t offset;
    }};

    return desc;
}

ImageProcessingTest::ImageProcessingTest(TestContext &testCtx, const std::string &name, const TestParams &testParams)
    : vkt::TestCase(testCtx, name)
    , m_params(testParams)
{
    DE_ASSERT(m_params.sampledImageParams.imageType == IMAGE_TYPE_2D);
}

ImageProcessingTest::~ImageProcessingTest(void)
{
}

void ImageProcessingTest::checkSupport(Context &context) const
{
    const auto &vki           = context.getInstanceInterface();
    const auto physicalDevice = context.getPhysicalDevice();

    if (context.getUsedApiVersion() < VK_API_VERSION_1_3)
        context.requireDeviceFunctionality("VK_KHR_format_feature_flags2");

    context.requireDeviceFunctionality("VK_QCOM_image_processing");

    const auto &features = context.getImageProcessingFeaturesQCOM();

    VkFormatProperties3 formatProperties3 = initVulkanStructure();
    VkFormatProperties2 formatProperties2 = initVulkanStructure(&formatProperties3);
    vki.getPhysicalDeviceFormatProperties2(physicalDevice, m_params.sampledImageParams.format, &formatProperties2);

    switch (m_params.imageProcOp)
    {
    case ImageProcOp::IMAGE_PROC_OP_BLOCK_MATCH_SAD:
    case ImageProcOp::IMAGE_PROC_OP_BLOCK_MATCH_SSD:
    {
        if (!features.textureBlockMatch)
            TCU_THROW(NotSupportedError, "Feature textureBlockMatch not supported");

        if ((m_params.sampledImageParams.tiling == VK_IMAGE_TILING_OPTIMAL) &&
            (formatProperties3.optimalTilingFeatures & VK_FORMAT_FEATURE_2_BLOCK_MATCHING_BIT_QCOM) == 0)
            TCU_THROW(NotSupportedError, "Format feature block matching bit not supported for optimal tiling.");

        if ((m_params.sampledImageParams.tiling == VK_IMAGE_TILING_LINEAR) &&
            (formatProperties3.linearTilingFeatures & VK_FORMAT_FEATURE_2_BLOCK_MATCHING_BIT_QCOM) == 0)
            TCU_THROW(NotSupportedError, "Format feature block matching bit not supported for linear tiling.");
    }
    break;
    case ImageProcOp::IMAGE_PROC_OP_SAMPLE_WEIGHTED:
    {
        if (!features.textureSampleWeighted)
            TCU_THROW(NotSupportedError, "Feature textureSampleWeighted not supported");

        if ((m_params.sampledImageParams.tiling == VK_IMAGE_TILING_OPTIMAL) &&
            (formatProperties3.optimalTilingFeatures & VK_FORMAT_FEATURE_2_WEIGHT_IMAGE_BIT_QCOM) == 0)
            TCU_THROW(NotSupportedError, "Format feature weight image bit not supported for optimal tiling.");
        if ((m_params.sampledImageParams.tiling == VK_IMAGE_TILING_OPTIMAL) &&
            (formatProperties3.optimalTilingFeatures & VK_FORMAT_FEATURE_2_WEIGHT_SAMPLED_IMAGE_BIT_QCOM) == 0)
            TCU_THROW(NotSupportedError, "Format feature weight sampled image bit not supported for optimal tiling.");

        if ((m_params.sampledImageParams.tiling == VK_IMAGE_TILING_LINEAR) &&
            (formatProperties3.linearTilingFeatures & VK_FORMAT_FEATURE_2_WEIGHT_IMAGE_BIT_QCOM) == 0)
            TCU_THROW(NotSupportedError, "Format feature weight image bit not supported for linear tiling.");
        if ((m_params.sampledImageParams.tiling == VK_IMAGE_TILING_LINEAR) &&
            (formatProperties3.linearTilingFeatures & VK_FORMAT_FEATURE_2_WEIGHT_SAMPLED_IMAGE_BIT_QCOM) == 0)
            TCU_THROW(NotSupportedError, "Format feature weight sampled image bit not supported for linear tiling.");
    }
    break;
    case ImageProcOp::IMAGE_PROC_OP_BOX_FILTER:
    {
        if (!features.textureBoxFilter)
            TCU_THROW(NotSupportedError, "Feature textureBoxFilter not supported");

        if ((m_params.sampledImageParams.tiling == VK_IMAGE_TILING_OPTIMAL) &&
            (formatProperties3.optimalTilingFeatures & VK_FORMAT_FEATURE_2_BOX_FILTER_SAMPLED_BIT_QCOM) == 0)
            TCU_THROW(NotSupportedError, "Format feature box filter sampled bit not supported for optimal tiling.");

        if ((m_params.sampledImageParams.tiling == VK_IMAGE_TILING_LINEAR) &&
            (formatProperties3.linearTilingFeatures & VK_FORMAT_FEATURE_2_BOX_FILTER_SAMPLED_BIT_QCOM) == 0)
            TCU_THROW(NotSupportedError, "Format feature box filter sampled bit not supported for linear tiling.");
    }
    break;
    default:
        DE_ASSERT(false);
    }

    if (m_params.updateAfterBind)
    {
        context.requireDeviceFunctionality("VK_EXT_descriptor_indexing");
        if (!context.getDescriptorIndexingFeatures().descriptorBindingSampledImageUpdateAfterBind)
            TCU_THROW(NotSupportedError, "descriptorBindingSampledImageUpdateAfterBind not supported.");
    }
}

ImageProcessingTestInstance::ImageProcessingTestInstance(Context &context, const TestParams &testParams)
    : vkt::TestInstance(context)
    , m_params(testParams)
    , m_rnd(1234)
{
}

ImageProcessingTestInstance::~ImageProcessingTestInstance(void)
{
}

VkSamplerReductionMode getVkSamplerReductionMode(const SamplerReductionMode reductionMode)
{
    const VkSamplerReductionMode redMode[] = {VK_SAMPLER_REDUCTION_MODE_WEIGHTED_AVERAGE, // NONE maps to default
                                              VK_SAMPLER_REDUCTION_MODE_WEIGHTED_AVERAGE, VK_SAMPLER_REDUCTION_MODE_MIN,
                                              VK_SAMPLER_REDUCTION_MODE_MAX};

    return redMode[reductionMode];
}

VkSamplerReductionModeCreateInfo getSamplerReductionCreateInfo(const VkSamplerReductionMode reductionMode)
{
    const VkSamplerReductionModeCreateInfo ret = {
        VK_STRUCTURE_TYPE_SAMPLER_REDUCTION_MODE_CREATE_INFO, // VkStructureType                sType
        nullptr,                                              // const void*                    pNext
        reductionMode                                         // VkSamplerReductionMode        reductionMode
    };
    return ret;
}

Move<VkSampler> ImageProcessingTestInstance::makeSampler(const bool unnorm, const VkSamplerAddressMode addrMode,
                                                         const SamplerReductionMode reductionMode)
{
    const auto &vkd   = m_context.getDeviceInterface();
    const auto device = m_context.getDevice();

    const VkSamplerCreateFlags samplerFlags = VK_SAMPLER_CREATE_IMAGE_PROCESSING_BIT_QCOM;

    const VkSamplerReductionMode redMode                     = getVkSamplerReductionMode(reductionMode);
    const VkSamplerReductionModeCreateInfo redModeCreateInfo = getSamplerReductionCreateInfo(redMode);
    const void *pNext = ((reductionMode == SAMPLER_REDUCTION_MODE_NONE) ? nullptr : &redModeCreateInfo);

    const VkSamplerCreateInfo samplerParams = {
        VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,   // VkStructureType sType;
        pNext,                                   // const void* pNext;
        samplerFlags,                            // VkSamplerCreateFlags flags;
        VK_FILTER_NEAREST,                       // VkFilter magFilter;
        VK_FILTER_NEAREST,                       // VkFilter minFilter;
        VK_SAMPLER_MIPMAP_MODE_NEAREST,          // VkSamplerMipmapMode mipmapMode;
        addrMode,                                // VkSamplerAddressMode addressModeU;
        addrMode,                                // VkSamplerAddressMode addressModeV;
        VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,   // VkSamplerAddressMode addressModeW;
        0.0f,                                    // float mipLodBias;
        VK_FALSE,                                // VkBool32 anisotropyEnable;
        1.0f,                                    // float maxAnisotropy;
        VK_FALSE,                                // VkBool32 compareEnable;
        VK_COMPARE_OP_ALWAYS,                    // VkCompareOp compareOp;
        0.0f,                                    // float minLod;
        0.0f,                                    // float maxLod;
        VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK, // VkBorderColor borderColor;
        unnorm,                                  // VkBool32 unnormalizedCoordinates;
    };

    return createSampler(vkd, device, &samplerParams);
}

TestStatus ImageProcessingTestInstance::verifyResult(const Vec4 &referenceValue, const Vec4 &resultValue,
                                                     const ConstPixelBufferAccess &referenceAccess,
                                                     const ConstPixelBufferAccess &resultAccess,
                                                     const Vec4 errorThreshold)
{
    bool resultOk = true;

    auto &log = m_context.getTestContext().getLog();

    Vec4 imgThreshold(0.0f, 0.0f, 0.0f, 0.0f); // Exact results

    // Check result of error metric comparison
    Vec4 diff = abs(referenceValue - resultValue);

    std::ostringstream infoMessage;
    infoMessage << "Result metric comparison: expected = " << referenceValue << ", got = " << resultValue
                << ", threshold = " << errorThreshold;
    log << TestLog::Message << infoMessage.str() << TestLog::EndMessage;

    // Check result of image comparison
    resultOk = floatThresholdCompare(log, "TestResults", "Test Result Images", referenceAccess, resultAccess,
                                     imgThreshold, COMPARE_LOG_ON_ERROR);

    if (!resultOk)
        return TestStatus::fail("Image comparison failed; check log for details");

    resultOk = boolAll(lessThanEqual(diff, errorThreshold));

    if (!resultOk)
    {
        infoMessage << "; check log for details";
        return TestStatus::fail(infoMessage.str());
    }

    return TestStatus::pass("Pass");
}

} // namespace ImageProcessing
} // namespace vkt
