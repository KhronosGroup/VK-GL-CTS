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
 * \brief Image processing weight image sampling tests
 *//*--------------------------------------------------------------------*/

#include "vktImageProcessingTests.hpp"
#include "vktImageProcessingTestsUtil.hpp"
#include "vktImageProcessingBase.hpp"
#include "vktTestCase.hpp"

#include "vkBufferWithMemory.hpp"
#include "vkImageWithMemory.hpp"
#include "vkImageUtil.hpp"
#include "vkPipelineConstructionUtil.hpp"
#include "vkComputePipelineConstructionUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkBarrierUtil.hpp"
#include "vkTypeUtil.hpp"

#include "deDefs.h"
#include "deStringUtil.hpp"
#include "deUniquePtr.hpp"
#include "deMath.h"

#include "tcuImageCompare.hpp"
#include "tcuRGBA.hpp"
#include "tcuVectorType.hpp"
#include "tcuVectorUtil.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuTestCase.hpp"
#include "tcuTexture.hpp"

#include "gluShaderProgram.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

using namespace vk;
using namespace tcu;

namespace vkt
{
namespace ImageProcessing
{

namespace
{

enum WeightType
{
    WEIGHT_TYPE_ZERO,
    WEIGHT_TYPE_ONE,
    WEIGHT_TYPE_OTHER,
};

struct WeightSamplingTestParams
{
    TestImageParams weightImageParams;
    bool isUnnormCoord;
    Vec2 textureCoord;
    VkOffset2D filterCenter;
    VkExtent2D filterSize;
    uint32_t filterPhases;
    uint32_t weightImageLayers;
    WeightType weightValue;
};

struct TestPushConstants
{
    Vec2 textureCoord;
};

class ImageProcessingWeightSamplingTest : public ImageProcessingTest
{
public:
    ImageProcessingWeightSamplingTest(TestContext &testCtx, const std::string &name, const TestParams &testParams,
                                      const WeightSamplingTestParams &weightSamplingTestParams);

    virtual ~ImageProcessingWeightSamplingTest(void);
    virtual void checkSupport(Context &context) const;

protected:
    const WeightSamplingTestParams m_weightSamplingParams;
    const ImageType m_outImageType;
    const UVec2 m_outImageSize;
    const VkFormat m_outImageFormat;
};

ImageProcessingWeightSamplingTest::ImageProcessingWeightSamplingTest(
    TestContext &testCtx, const std::string &name, const TestParams &testParams,
    const WeightSamplingTestParams &weightSamplingTestParams)
    : ImageProcessingTest(testCtx, name, testParams)
    , m_weightSamplingParams(weightSamplingTestParams)
    , m_outImageType(IMAGE_TYPE_2D)
    , m_outImageSize(4u, 4u)
    , m_outImageFormat(VK_FORMAT_R8G8B8A8_UNORM)
{
    // VUID-VkImageViewSampleWeightCreateInfoQCOM-filterCenter-06960
    DE_ASSERT(m_weightSamplingParams.filterCenter.x <=
              static_cast<int32_t>(m_weightSamplingParams.filterSize.width - 1));
    // VUID-VkImageViewSampleWeightCreateInfoQCOM-filterCenter-06961
    DE_ASSERT(m_weightSamplingParams.filterCenter.y <=
              static_cast<int32_t>(m_weightSamplingParams.filterSize.height - 1));
    DE_ASSERT(
        deIsPowerOfTwo32(static_cast<int32_t>(deFloatSqrt(static_cast<float>(m_weightSamplingParams.filterPhases)))));

    // VUID-VkImageViewCreateInfo-pNext-06946
    DE_ASSERT((m_weightSamplingParams.weightImageParams.components.r == VK_COMPONENT_SWIZZLE_IDENTITY) &&
              (m_weightSamplingParams.weightImageParams.components.g == VK_COMPONENT_SWIZZLE_IDENTITY) &&
              (m_weightSamplingParams.weightImageParams.components.b == VK_COMPONENT_SWIZZLE_IDENTITY) &&
              (m_weightSamplingParams.weightImageParams.components.a == VK_COMPONENT_SWIZZLE_IDENTITY));

    // VUID-VkImageViewCreateInfo-pNext-06949
    // VUID-VkImageViewCreateInfo-pNext-06951
    // VUID-VkImageViewCreateInfo-pNext-06954
    // VUID-VkImageViewCreateInfo-pNext-06955
    // VUID-VkImageViewCreateInfo-pNext-06956
    DE_ASSERT(((m_weightSamplingParams.weightImageParams.imageType == IMAGE_TYPE_1D_ARRAY) &&
               (m_weightSamplingParams.weightImageLayers == 2u) &&
               (m_weightSamplingParams.weightImageParams.imageSize.x() >=
                (m_weightSamplingParams.filterPhases *
                 deMaxu32(static_cast<int32_t>(deAlign32(m_weightSamplingParams.filterSize.width, 4)),
                          m_weightSamplingParams.filterSize.height)))) ||
              ((m_weightSamplingParams.weightImageParams.imageType == IMAGE_TYPE_2D_ARRAY) &&
               (m_weightSamplingParams.weightImageLayers >= m_weightSamplingParams.filterPhases) &&
               (m_weightSamplingParams.weightImageParams.imageSize.x() >= m_weightSamplingParams.filterSize.width) &&
               (m_weightSamplingParams.weightImageParams.imageSize.y() >= m_weightSamplingParams.filterSize.height)));

    DE_ASSERT(m_weightSamplingParams.filterCenter.x >= 0);
    DE_ASSERT(m_weightSamplingParams.filterCenter.y >= 0);
}

ImageProcessingWeightSamplingTest::~ImageProcessingWeightSamplingTest(void)
{
}

void ImageProcessingWeightSamplingTest::checkSupport(Context &context) const
{
    const auto &vki           = context.getInstanceInterface();
    const auto physicalDevice = context.getPhysicalDevice();

    ImageProcessingTest::checkSupport(context);

    // Weight image format features
    {
        VkFormatProperties3 formatProperties3 = initVulkanStructure();
        VkFormatProperties2 formatProperties2 = initVulkanStructure(&formatProperties3);
        vki.getPhysicalDeviceFormatProperties2(physicalDevice, m_weightSamplingParams.weightImageParams.format,
                                               &formatProperties2);

        if ((m_weightSamplingParams.weightImageParams.tiling == VK_IMAGE_TILING_OPTIMAL) &&
            (formatProperties3.optimalTilingFeatures & VK_FORMAT_FEATURE_2_WEIGHT_IMAGE_BIT_QCOM) == 0)
            TCU_THROW(NotSupportedError, "Format feature weight image bit not supported for optimal tiling.");

        if ((m_weightSamplingParams.weightImageParams.tiling == VK_IMAGE_TILING_LINEAR) &&
            (formatProperties3.linearTilingFeatures & VK_FORMAT_FEATURE_2_WEIGHT_IMAGE_BIT_QCOM) == 0)
            TCU_THROW(NotSupportedError, "Format feature weight image bit not supported for linear tiling.");
    }

    // Limits
    {
        VkPhysicalDeviceImageProcessingPropertiesQCOM imgProcProperties;
        deMemset(&imgProcProperties, 0, sizeof(imgProcProperties));
        imgProcProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_PROCESSING_PROPERTIES_QCOM;

        VkPhysicalDeviceProperties2 properties2;
        deMemset(&properties2, 0, sizeof(properties2));
        properties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
        properties2.pNext = &imgProcProperties;

        vki.getPhysicalDeviceProperties2(context.getPhysicalDevice(), &properties2);

        if ((m_weightSamplingParams.filterSize.width > imgProcProperties.maxWeightFilterDimension.width) ||
            (m_weightSamplingParams.filterSize.height > imgProcProperties.maxWeightFilterDimension.height))
            TCU_THROW(NotSupportedError, "Weight filter size is greater than supported device limits");

        if ((m_weightSamplingParams.filterPhases > imgProcProperties.maxWeightFilterPhases))
            TCU_THROW(NotSupportedError, "Weight filter phases are more than supported device limits");
    }

    // Sampled image
    {
        VkImageFormatProperties sampledImageFormatProperties;
        const auto result = vki.getPhysicalDeviceImageFormatProperties(
            physicalDevice, m_params.sampledImageParams.format, mapImageType(m_params.sampledImageParams.imageType),
            m_params.sampledImageParams.tiling, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, 0,
            &sampledImageFormatProperties);

        if (result != VK_SUCCESS)
        {
            if (result == VK_ERROR_FORMAT_NOT_SUPPORTED)
                TCU_THROW(NotSupportedError, "Sampled image format unsupported for weight image sampling");
            else
                TCU_FAIL("vkGetPhysicalDeviceImageFormatProperties returned unexpected error");
        }
    }

    // Weight image
    {
        VkImageFormatProperties weightImageFormatProperties;
        const auto result = vki.getPhysicalDeviceImageFormatProperties(
            physicalDevice, m_weightSamplingParams.weightImageParams.format,
            mapImageType(m_weightSamplingParams.weightImageParams.imageType),
            m_weightSamplingParams.weightImageParams.tiling,
            VK_IMAGE_USAGE_SAMPLE_WEIGHT_BIT_QCOM | VK_IMAGE_USAGE_TRANSFER_DST_BIT, 0, &weightImageFormatProperties);

        if (result != VK_SUCCESS)
        {
            if (result == VK_ERROR_FORMAT_NOT_SUPPORTED)
                TCU_THROW(NotSupportedError, "Weight image format unsupported for weight image sampling");
            else
                TCU_FAIL("vkGetPhysicalDeviceImageFormatProperties returned unexpected error");
        }
    }
}
class ImageProcessingWeightSamplingGraphicsTest : public ImageProcessingWeightSamplingTest
{
public:
    ImageProcessingWeightSamplingGraphicsTest(TestContext &testCtx, const std::string &name,
                                              const TestParams &testParams,
                                              const WeightSamplingTestParams &weightSamplingTestParams);
    virtual ~ImageProcessingWeightSamplingGraphicsTest(void);

    virtual void checkSupport(Context &context) const;
    virtual void initPrograms(SourceCollections &sourceCollections) const;
    virtual TestInstance *createInstance(Context &context) const;
};

ImageProcessingWeightSamplingGraphicsTest::ImageProcessingWeightSamplingGraphicsTest(
    TestContext &testCtx, const std::string &name, const TestParams &testParams,
    const WeightSamplingTestParams &weightSamplingTestParams)
    : ImageProcessingWeightSamplingTest(testCtx, name, testParams, weightSamplingTestParams)
{
}

ImageProcessingWeightSamplingGraphicsTest::~ImageProcessingWeightSamplingGraphicsTest(void)
{
}

void ImageProcessingWeightSamplingGraphicsTest::checkSupport(Context &context) const
{
    const auto &vki           = context.getInstanceInterface();
    const auto physicalDevice = context.getPhysicalDevice();

    ImageProcessingWeightSamplingTest::checkSupport(context);

    {
        VkImageFormatProperties outImageFormatProperties;
        const auto result = vki.getPhysicalDeviceImageFormatProperties(
            physicalDevice, m_outImageFormat, mapImageType(m_outImageType), VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, 0, &outImageFormatProperties);

        if (result != VK_SUCCESS)
        {
            if (result == VK_ERROR_FORMAT_NOT_SUPPORTED)
                TCU_THROW(NotSupportedError, "Format unsupported for color attachment");
            else
                TCU_FAIL("vkGetPhysicalDeviceImageFormatProperties returned unexpected error");
        }
    }

    checkPipelineConstructionRequirements(vki, physicalDevice, m_params.pipelineConstructionType);
}

const std::string getProgPreMain(const bool is2DWeightImage)
{
    const std::string weightTexStr = is2DWeightImage ? "texture2DArray" : "texture1DArray";

    std::string prog = "#extension GL_QCOM_image_processing : require\n"
                       "\n"
                       "layout(set = 0, binding = 0) uniform highp texture2D sampledTexture;\n"
                       "layout(set = 0, binding = 1) uniform highp " +
                       weightTexStr +
                       " kernelTexture;\n"
                       "layout(set = 0, binding = 2) uniform highp sampler textureSampler;\n"
                       "layout(set = 0, binding = 3) uniform highp sampler kernelSampler;\n"
                       "layout(set = 0, binding = 4) writeonly buffer outputValue {\n"
                       "  vec4 avg;\n"
                       "} sbOut;\n"
                       "layout(push_constant, std430) uniform PushConstants\n"
                       "{\n"
                       "    vec2 textureCoord;\n"
                       "} pc;\n";
    return prog;
}

const std::string getProgMainBlock(const ImageProcOp op, const bool is2DWeightImage)
{
    const std::string weightSamplerStr = is2DWeightImage ? "sampler2DArray" : "sampler1DArray";

    std::string prog = "    // Compute\n"
                       "    vec4 weightSampleVal = " +
                       getImageProcGLSLStr(op) +
                       "(\n"
                       "        sampler2D(sampledTexture, textureSampler),\n"
                       "        pc.textureCoord,  \n"
                       "        " +
                       weightSamplerStr +
                       "(kernelTexture, kernelSampler)\n"
                       "    );\n"
                       "\n"
                       "    vec4 result = weightSampleVal;"
                       "\n"
                       "    if (result != vec4(0.0f, 0.0f, 0.0f, 0.0f))\n"
                       "        outColor = vec4(0.0f, 1.0f, 0.0f, 1.0f);\n" // green non-zero
                       "    else\n"
                       "        outColor = vec4(1.0f, 0.0f, 0.0f, 1.0f);\n" // red on zero
                       "    sbOut.avg = result;\n";                         // real value
    return prog;
}

void ImageProcessingWeightSamplingGraphicsTest::initPrograms(SourceCollections &sourceCollections) const
{
    const vk::ShaderBuildOptions shaderBuildOpt(sourceCollections.usedVulkanVersion, vk::SPIRV_VERSION_1_4, 0u, true);

    const bool is2DWeightImage = (m_weightSamplingParams.weightImageParams.imageSize.y() > 1u) ? true : false;

    std::ostringstream vert;
    {
        vert << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n";
        if (m_params.stageMask & VK_SHADER_STAGE_VERTEX_BIT)
        {
            vert << "layout(location = 0) in vec2 inPosition;\n"
                 << getProgPreMain(is2DWeightImage) << "\n"
                 << "layout(location = 0) out vec4 outColor;\n"
                 << "\n"
                 << "void main() {\n"
                 << getProgMainBlock(m_params.imageProcOp, is2DWeightImage)
                 << "    gl_Position = vec4(inPosition, 0.0, 1.0);\n"
                 << "}\n";
        }
        else // regular vertex shader
        {
            vert << "layout(location = 0) in vec2 inPosition;\n"
                 << "\n"
                 << "void main() {\n"
                 << "    gl_Position = vec4(inPosition, 0.0, 1.0);\n"
                 << "}\n";
        }
    }
    sourceCollections.glslSources.add("vert") << glu::VertexSource(vert.str()) << shaderBuildOpt;

    std::ostringstream frag;
    {
        frag << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n";
        if (m_params.stageMask & VK_SHADER_STAGE_FRAGMENT_BIT)
        {
            frag << "\n"
                 << getProgPreMain(is2DWeightImage) << "\n"
                 << "layout(location = 0) out vec4 outColor;\n"
                 << "\n"
                 << "void main() {\n"
                 << getProgMainBlock(m_params.imageProcOp, is2DWeightImage) << "}\n";
        }
        else
        {
            frag << "layout(location = 0) in vec4 inColor;\n"
                 << "layout(location = 0) out vec4 fragColor;\n"
                 << "\n"
                 << "void main() {\n"
                 << "    fragColor = inColor;\n"
                 << "}\n";
        }
    }
    sourceCollections.glslSources.add("frag") << glu::FragmentSource(frag.str()) << shaderBuildOpt;
}

class ImageProcessingWeightSamplingTestInstance : public ImageProcessingTestInstance
{
public:
    ImageProcessingWeightSamplingTestInstance(Context &context, const TestParams &testParams,
                                              const WeightSamplingTestParams &weightSamplingTestParams,
                                              const ImageType outImageType, const UVec2 outImageSize,
                                              const VkFormat outImageFormat);
    ~ImageProcessingWeightSamplingTestInstance(void);

    virtual void addSupplementaryDescBindings(DescriptorSetLayoutExtBuilder &layoutBuilder);
    virtual void addSupplementaryDescTypes(DescriptorPoolBuilder &poolBuilder);
    virtual void writeSupplementaryDescriptors();
    void prepareDescriptors();
    void populateColorBuffer(const BufferWithMemory &colorBuffer, const UVec2 &imageSize, const VkFormat format);
    void populateNonSeparableWeightBuffer(const BufferWithMemory &weightBuffers, const UVec2 &imageSize,
                                          const uint32_t imageLayers, const VkFormat format);
    void populateSeparableWeightBuffer(const BufferWithMemory &weightBuffers, const UVec2 &imageSize,
                                       const uint32_t imageLayers, const VkFormat format);

    void prepareCommandBuffer();
    virtual void executeBarriers();
    virtual void executeBegin();
    virtual void executeBindPipeline();
    virtual void executeBindOtherBindings();
    virtual void executeProgram();
    virtual void executeEnd();
    void executeCommands(const VkPipelineLayout pipelineLayout, const BufferWithMemory &texColorBuffer,
                         const ImageWithMemory &texImage, const BufferWithMemory &weightBuffer,
                         const ImageWithMemory &weightImage, const BufferWithMemory &resultBuffer,
                         const ImageWithMemory &resultImage);

    std::vector<Vec4> buildStandardResult(ImageProcessingResult &expectedResult, const BufferWithMemory &texColorBuffer,
                                          const BufferWithMemory &weightBuffer);

    Move<VkImageView> makeWeightImageView(const DeviceInterface &vk, const VkDevice vkDevice, const VkImage image,
                                          const VkImageSubresourceRange subresourceRange);

protected:
    const WeightSamplingTestParams m_weightSamplingParams;

    const ImageType m_outImageType;
    const UVec2 m_outImageSize;
    const VkFormat m_outImageFormat;

    Move<VkDescriptorSetLayout> m_descriptorSetLayout;
    Move<VkDescriptorPool> m_descriptorPool;
    Move<VkDescriptorSet> m_descriptorSet;
    DescriptorSetUpdateBuilder m_descriptorUpdateBuilder;
    Move<VkCommandPool> m_cmdPool;
    Move<VkCommandBuffer> m_cmdBuffer;
    Move<VkSampler> m_sampledImageSampler;
    Move<VkImageView> m_sampledImageView;
    Move<VkSampler> m_weightImageSampler;
    Move<VkImageView> m_weightImageView;
    de::MovePtr<BufferWithMemory> m_resultBuffer;
};

ImageProcessingWeightSamplingTestInstance::ImageProcessingWeightSamplingTestInstance(
    Context &context, const TestParams &testParams, const WeightSamplingTestParams &weightSamplingTestParams,
    const ImageType outImageType, const UVec2 outImageSize, const VkFormat outImageFormat)
    : ImageProcessingTestInstance(context, testParams)
    , m_weightSamplingParams(weightSamplingTestParams)
    , m_outImageType(outImageType)
    , m_outImageSize(outImageSize)
    , m_outImageFormat(outImageFormat)
{
}

ImageProcessingWeightSamplingTestInstance::~ImageProcessingWeightSamplingTestInstance(void)
{
}

void ImageProcessingWeightSamplingTestInstance::addSupplementaryDescBindings(
    DescriptorSetLayoutExtBuilder &layoutBuilder)
{
    DE_UNREF(layoutBuilder);
}

void ImageProcessingWeightSamplingTestInstance::addSupplementaryDescTypes(DescriptorPoolBuilder &poolBuilder)
{
    DE_UNREF(poolBuilder);
}

void ImageProcessingWeightSamplingTestInstance::writeSupplementaryDescriptors()
{
}

void ImageProcessingWeightSamplingTestInstance::prepareDescriptors()
{
    const auto &vkd   = m_context.getDeviceInterface();
    const auto device = m_context.getDevice();

    VkDescriptorPoolCreateFlags descPoolCreateFlags           = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    VkDescriptorSetLayoutCreateFlags descSetLayoutCreateFlags = 0u;
    VkDescriptorBindingFlags descBindingFlag                  = 0u;

    const auto sampledImageDescType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    const auto weightImageDescType  = VK_DESCRIPTOR_TYPE_SAMPLE_WEIGHT_IMAGE_QCOM;

    if (m_params.updateAfterBind)
    {
        descPoolCreateFlags |= VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
        descSetLayoutCreateFlags |= VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
        descBindingFlag |= VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;
    }

    // Descriptor set layout
    DescriptorSetLayoutExtBuilder layoutBuilder;
    layoutBuilder.addSingleBinding(sampledImageDescType, m_params.stageMask);
    layoutBuilder.addSingleBinding(weightImageDescType, m_params.stageMask);
    layoutBuilder.addSingleSamplerBinding(VK_DESCRIPTOR_TYPE_SAMPLER, m_params.stageMask, &m_sampledImageSampler.get());
    layoutBuilder.addSingleSamplerBinding(VK_DESCRIPTOR_TYPE_SAMPLER, m_params.stageMask, &m_weightImageSampler.get());
    layoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, m_params.stageMask);
    addSupplementaryDescBindings(layoutBuilder);
    m_descriptorSetLayout = layoutBuilder.buildExt(vkd, device, descSetLayoutCreateFlags, descBindingFlag);

    // Descriptor pool
    DescriptorPoolBuilder poolBuilder;
    poolBuilder.addType(sampledImageDescType);
    poolBuilder.addType(weightImageDescType);
    poolBuilder.addType(VK_DESCRIPTOR_TYPE_SAMPLER);
    poolBuilder.addType(VK_DESCRIPTOR_TYPE_SAMPLER);
    poolBuilder.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
    addSupplementaryDescTypes(poolBuilder);
    m_descriptorPool = poolBuilder.build(vkd, device, descPoolCreateFlags, 1u);

    // Descriptor set
    m_descriptorSet = makeDescriptorSet(vkd, device, m_descriptorPool.get(), m_descriptorSetLayout.get());

    // Register descriptors in the update builder
    const auto texDescImageInfo =
        makeDescriptorImageInfo(VK_NULL_HANDLE, m_sampledImageView.get(), m_params.sampledImageParams.layout);
    m_descriptorUpdateBuilder.writeSingle(m_descriptorSet.get(), DescriptorSetUpdateBuilder::Location::binding(0u),
                                          sampledImageDescType, &texDescImageInfo);
    const auto weightDescImageInfo = makeDescriptorImageInfo(VK_NULL_HANDLE, m_weightImageView.get(),
                                                             m_weightSamplingParams.weightImageParams.layout);
    m_descriptorUpdateBuilder.writeSingle(m_descriptorSet.get(), DescriptorSetUpdateBuilder::Location::binding(1),
                                          weightImageDescType, &weightDescImageInfo);

    const VkDeviceSize resBuffSizeBytes            = sizeof(Vec4);
    const VkDescriptorBufferInfo resDescriptorInfo = makeDescriptorBufferInfo(**m_resultBuffer, 0ull, resBuffSizeBytes);
    m_descriptorUpdateBuilder.writeSingle(m_descriptorSet.get(), DescriptorSetUpdateBuilder::Location::binding(4u),
                                          VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &resDescriptorInfo);
    writeSupplementaryDescriptors();

    // Update descriptor set with the descriptor
    if (!m_params.updateAfterBind)
        m_descriptorUpdateBuilder.update(vkd, device);
}

void ImageProcessingWeightSamplingTestInstance::populateColorBuffer(const BufferWithMemory &colorBuffer,
                                                                    const UVec2 &imageSize, const VkFormat format)
{
    const auto &vkd   = m_context.getDeviceInterface();
    const auto device = m_context.getDevice();

    const IVec2 fillSize{static_cast<int>(imageSize.x()), static_cast<int>(imageSize.y())};
    auto &colorBufferAlloc = colorBuffer.getAllocation();
    auto colorBufferPtr    = reinterpret_cast<char *>(colorBufferAlloc.getHostPtr()) + colorBufferAlloc.getOffset();
    const PixelBufferAccess colorBufferPixels{mapVkFormat(format), fillSize[0], fillSize[1], 1, colorBufferPtr};

    const int W = colorBufferPixels.getWidth();
    const int H = colorBufferPixels.getHeight();
    const int D = colorBufferPixels.getDepth();

    const float minChannelValue = 0.01f;
    const float maxChannelValue = isSignedFormat(format) ? 0.3f : 1.0f;

    const Vec4 uniformColor = Vec4(maxChannelValue, maxChannelValue, maxChannelValue, maxChannelValue);

    for (int x = 0; x < W; ++x)
        for (int y = 0; y < H; ++y)
            for (int z = 0; z < D; ++z)
            {
                const float colorR = m_rnd.getFloat(minChannelValue, maxChannelValue);
                const float colorG = m_rnd.getFloat(minChannelValue, maxChannelValue);
                const float colorB = m_rnd.getFloat(minChannelValue, maxChannelValue);
                const float colorA = m_rnd.getFloat(minChannelValue, maxChannelValue);
                const Vec4 randomColor(colorR, colorG, colorB, colorA);

                const Vec4 color = (m_params.randomReference ? randomColor : uniformColor);

                colorBufferPixels.setPixel(color, x, y, z);
            }

    flushAlloc(vkd, device, colorBufferAlloc);
}

bool getWeightValue(Vec4 &weightValue, const WeightType type, SamplerReductionMode redMode)
{
    bool useWeightValue = false;

    if ((redMode == SAMPLER_REDUCTION_MODE_MIN) || (redMode == SAMPLER_REDUCTION_MODE_MAX))
    {
        useWeightValue = true;

        switch (type)
        {
        case WEIGHT_TYPE_ONE:
            weightValue = Vec4(1.0f, 1.0f, 1.0f, 1.0f);
            break;
        case WEIGHT_TYPE_ZERO:
            weightValue = Vec4(0.0f, 0.0f, 0.0f, 0.0f);
            break;
        case WEIGHT_TYPE_OTHER:
            // weightValue used as initialized
            break;
        default:
            DE_ASSERT(false);
        }
    }

    return useWeightValue;
}

void ImageProcessingWeightSamplingTestInstance::populateNonSeparableWeightBuffer(const BufferWithMemory &weightBuffer,
                                                                                 const UVec2 &imageSize,
                                                                                 const uint32_t imageLayers,
                                                                                 const VkFormat format)
{
    const auto &vkd   = m_context.getDeviceInterface();
    const auto device = m_context.getDevice();

    const IVec3 fillSize{static_cast<int>(imageSize.x()), static_cast<int>(imageSize.y()),
                         static_cast<int>(imageLayers)};

    auto &weightBufferAlloc = weightBuffer.getAllocation();
    auto weightBufferPtr    = reinterpret_cast<char *>(weightBufferAlloc.getHostPtr()) + weightBufferAlloc.getOffset();
    const PixelBufferAccess weightBufferPixels{mapVkFormat(format), fillSize[0], fillSize[1], fillSize[2],
                                               weightBufferPtr};

    const int W = weightBufferPixels.getWidth();
    const int H = weightBufferPixels.getHeight();
    const int D = weightBufferPixels.getDepth();

    const float minChannelValue = 0.01f;
    const float maxChannelValue = isSignedFormat(format) ? 0.3f : 1.0f;

    const float non01ChannelValue = m_rnd.getFloat(0.1f, 0.9f); // between 0 and 1
    Vec4 weightValue              = Vec4(non01ChannelValue, non01ChannelValue, non01ChannelValue, non01ChannelValue);
    const bool useWeightValue =
        getWeightValue(weightValue, m_weightSamplingParams.weightValue, m_params.sampledImageParams.reductionMode);

    // Main filter region
    const int filterW = static_cast<int>(m_weightSamplingParams.filterSize.width);
    const int filterH = static_cast<int>(m_weightSamplingParams.filterSize.height);

    for (int x = 0; x < W; ++x)
        for (int y = 0; y < H; ++y)
            for (int z = 0; z < D; ++z)
            {
                if (de::inBounds(x, 0, filterW) && de::inBounds(y, 0, filterH))
                {
                    const float weightR = m_rnd.getFloat(minChannelValue, maxChannelValue);
                    const float weightG = m_rnd.getFloat(minChannelValue, maxChannelValue);
                    const float weightB = m_rnd.getFloat(minChannelValue, maxChannelValue);
                    const float weightA = m_rnd.getFloat(minChannelValue, maxChannelValue);
                    const Vec4 randomWeight(weightR, weightG, weightB, weightA);

                    const float layerIdxWeight = (static_cast<float>(z) + 1.0f) / 10.0f;
                    const Vec4 uniformWeight   = Vec4(layerIdxWeight, layerIdxWeight, layerIdxWeight, layerIdxWeight);

                    const Vec4 weight =
                        useWeightValue ? weightValue : (m_params.randomReference ? randomWeight : uniformWeight);

                    // std::cout << "Wt coords: " << x << ", " << y << ", " << z << " Wt: " << weight << "\n";

                    weightBufferPixels.setPixel(weight, x, y, z);
                }
                else // Outside filter region
                {
                    weightBufferPixels.setPixel(Vec4(1.0f, 1.0f, 1.0f, 1.0f), x, y, z);
                }
            }

    flushAlloc(vkd, device, weightBufferAlloc);
}

void ImageProcessingWeightSamplingTestInstance::populateSeparableWeightBuffer(const BufferWithMemory &weightBuffer,
                                                                              const UVec2 &imageSize,
                                                                              const uint32_t imageLayers,
                                                                              const VkFormat format)
{
    DE_ASSERT(imageLayers == 2u);

    const auto &vkd   = m_context.getDeviceInterface();
    const auto device = m_context.getDevice();

    const IVec3 fillSize{static_cast<int>(imageSize.x()), static_cast<int>(imageSize.y()),
                         static_cast<int>(imageLayers)};

    auto &weightBufferAlloc = weightBuffer.getAllocation();
    auto weightBufferPtr    = reinterpret_cast<char *>(weightBufferAlloc.getHostPtr()) + weightBufferAlloc.getOffset();
    const PixelBufferAccess weightBufferPixels{mapVkFormat(format), fillSize[0], fillSize[1], fillSize[2],
                                               weightBufferPtr};

    const float minChannelValue = 0.01f;
    const float maxChannelValue = isSignedFormat(format) ? 0.3f : 1.0f;

    const float non01ChannelValue = m_rnd.getFloat(0.1f, 0.9f); // between 0 and 1
    Vec4 weightValue              = Vec4(non01ChannelValue, non01ChannelValue, non01ChannelValue, non01ChannelValue);
    const bool useWeightValue =
        getWeightValue(weightValue, m_weightSamplingParams.weightValue, m_params.sampledImageParams.reductionMode);

    // Parameters
    const uint32_t filterW     = m_weightSamplingParams.filterSize.width;
    const uint32_t filterH     = m_weightSamplingParams.filterSize.height;
    const uint32_t numPhases   = m_weightSamplingParams.filterPhases;
    const uint32_t phaseCountH = static_cast<uint32_t>(deSqrt(numPhases));
    const uint32_t phaseCountV = phaseCountH;

    // Make all weights 1.0f, outside filter region will be 1.0f
    // If used by mistake, test should fail because of increase in sum
    tcu::clear(weightBufferPixels, Vec4(1.0f));

    const uint32_t numLayers = imageLayers;

    for (uint32_t layerIdx = 0; layerIdx < numLayers; layerIdx++)
    {
        const uint32_t filterSize = (layerIdx == 0u) ? filterW : filterH;
        const uint32_t phaseCount = (layerIdx == 0u) ? phaseCountH : phaseCountV;

        uint32_t elementIdx = 0u;

        for (int32_t remaining = filterSize; remaining > 0; remaining -= filterSize)
        {
            for (uint32_t pIdx = 0; pIdx < phaseCount; pIdx++)
            {
                const uint32_t elementCount = de::min(remaining, 4); // elements per row

                for (uint32_t eIdx = 0; eIdx < elementCount; eIdx++)
                {
                    const float weightR = m_rnd.getFloat(minChannelValue, maxChannelValue);
                    const float weightG = m_rnd.getFloat(minChannelValue, maxChannelValue);
                    const float weightB = m_rnd.getFloat(minChannelValue, maxChannelValue);
                    const float weightA = m_rnd.getFloat(minChannelValue, maxChannelValue);
                    const Vec4 randomWeight(weightR, weightG, weightB, weightA);

                    const float phaseIdxWeight = (static_cast<float>(pIdx) + 1.0f) / 10.0f;
                    const Vec4 uniformWeight   = Vec4(
                        phaseIdxWeight, phaseIdxWeight, phaseIdxWeight,
                        phaseIdxWeight); // (layerIdx == 0u) ? Vec4(phaseIdxWeight, phaseIdxWeight, phaseIdxWeight, phaseIdxWeight) : Vec4(1.0f);

                    const Vec4 weight =
                        useWeightValue ? weightValue : (m_params.randomReference ? randomWeight : uniformWeight);

                    // std::cout << "Wt coords: " << elementIdx << ", 0, " << layerIdx << " Wt: " << weight << "\n";

                    weightBufferPixels.setPixel(weight, elementIdx, 0, layerIdx);

                    elementIdx++;
                }

                elementIdx += (4u - elementCount);
            }
        }
    }

    flushAlloc(vkd, device, weightBufferAlloc);
}

void ImageProcessingWeightSamplingTestInstance::executeBarriers()
{
}

void ImageProcessingWeightSamplingTestInstance::executeBegin()
{
}

void ImageProcessingWeightSamplingTestInstance::executeBindPipeline()
{
}

void ImageProcessingWeightSamplingTestInstance::executeBindOtherBindings()
{
}

void ImageProcessingWeightSamplingTestInstance::executeProgram()
{
}

void ImageProcessingWeightSamplingTestInstance::executeEnd()
{
}

void ImageProcessingWeightSamplingTestInstance::prepareCommandBuffer()
{
    const auto &vkd       = m_context.getDeviceInterface();
    const auto device     = m_context.getDevice();
    const auto queueIndex = m_context.getUniversalQueueFamilyIndex();

    // Command pool and command buffer
    m_cmdPool   = createCommandPool(vkd, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueIndex);
    m_cmdBuffer = allocateCommandBuffer(vkd, device, m_cmdPool.get(), VK_COMMAND_BUFFER_LEVEL_PRIMARY);
}

void ImageProcessingWeightSamplingTestInstance::executeCommands(
    const VkPipelineLayout pipelineLayout, const BufferWithMemory &texColorBuffer, const ImageWithMemory &texImage,
    const BufferWithMemory &weightBuffer, const ImageWithMemory &weightImage, const BufferWithMemory &resultBuffer,
    const ImageWithMemory &resultImage)
{
    const auto &vkd          = m_context.getDeviceInterface();
    const auto device        = m_context.getDevice();
    const auto queue         = m_context.getUniversalQueue();
    const bool isComputeTest = ((m_params.stageMask & VK_SHADER_STAGE_COMPUTE_BIT) == VK_SHADER_STAGE_COMPUTE_BIT);
    const auto cmdBuffer     = m_cmdBuffer.get();

    beginCommandBuffer(vkd, cmdBuffer);

    executeBarriers();

    // Copy color buffer to sampled image and change layout
    {
        const VkImageSubresourceLayers layerSubresource =
            makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u);
        const auto texTcuFormat               = mapVkFormat(m_params.sampledImageParams.format);
        const VkDeviceSize texColorBufferSize = static_cast<VkDeviceSize>(
            static_cast<uint32_t>(getPixelSize(texTcuFormat)) * m_params.sampledImageParams.imageSize.x() *
            m_params.sampledImageParams.imageSize.y() * 1u);
        const VkExtent3D texExtent =
            makeExtent3D(m_params.sampledImageParams.imageSize.x(), m_params.sampledImageParams.imageSize.y(), 1u);
        const std::vector<VkBufferImageCopy> bufferImageCopy(1, makeBufferImageCopy(texExtent, layerSubresource));
        copyBufferToImage(vkd, cmdBuffer, texColorBuffer.get(), texColorBufferSize, bufferImageCopy,
                          VK_IMAGE_ASPECT_COLOR_BIT, 1u, 1u, texImage.get(), m_params.sampledImageParams.layout);
    }

    // Copy weights to weight image
    {
        const auto weightImgTcuFormat       = mapVkFormat(m_weightSamplingParams.weightImageParams.format);
        const VkDeviceSize weightBufferSize = static_cast<VkDeviceSize>(
            static_cast<uint32_t>(getPixelSize(weightImgTcuFormat)) *
            m_weightSamplingParams.weightImageParams.imageSize.x() *
            m_weightSamplingParams.weightImageParams.imageSize.y() * 1u * m_weightSamplingParams.weightImageLayers);
        const VkExtent3D weightImgExtent = makeExtent3D(m_weightSamplingParams.weightImageParams.imageSize.x(),
                                                        m_weightSamplingParams.weightImageParams.imageSize.y(), 1u);
        const VkDeviceSize weightBufferLayerSize =
            static_cast<VkDeviceSize>(static_cast<uint32_t>(getPixelSize(weightImgTcuFormat)) *
                                      m_weightSamplingParams.weightImageParams.imageSize.x() *
                                      m_weightSamplingParams.weightImageParams.imageSize.y() * 1u);
        std::vector<VkBufferImageCopy> copyRegions(m_weightSamplingParams.weightImageLayers);
        {
            for (uint32_t layerIdx = 0; layerIdx < m_weightSamplingParams.weightImageLayers; layerIdx++)
            {
                const VkImageSubresourceLayers layerSubresource =
                    makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, layerIdx, 1u);
                copyRegions[layerIdx]              = makeBufferImageCopy(weightImgExtent, layerSubresource);
                copyRegions[layerIdx].bufferOffset = weightBufferLayerSize * layerIdx;
            }
        }

        copyBufferToImage(vkd, cmdBuffer, weightBuffer.get(), weightBufferSize, copyRegions, VK_IMAGE_ASPECT_COLOR_BIT,
                          1u, m_weightSamplingParams.weightImageLayers, weightImage.get(),
                          m_weightSamplingParams.weightImageParams.layout);
    }

    executeBegin();
    {
        executeBindPipeline();

        vkd.cmdBindDescriptorSets(cmdBuffer,
                                  isComputeTest ? VK_PIPELINE_BIND_POINT_COMPUTE : VK_PIPELINE_BIND_POINT_GRAPHICS,
                                  pipelineLayout, 0u, 1u, &m_descriptorSet.get(), 0u, nullptr);

        if (m_params.updateAfterBind)
            m_descriptorUpdateBuilder.update(vkd, device);

        executeBindOtherBindings();

        {
            // Push constant data
            const TestPushConstants pushConstants = {m_weightSamplingParams.textureCoord};
            vkd.cmdPushConstants(cmdBuffer, pipelineLayout, m_params.stageMask, 0u,
                                 static_cast<uint32_t>(sizeof(pushConstants)), &pushConstants);
        }

        executeProgram();
    }
    executeEnd();

    {
        const VkDeviceSize resBuffSizeBytes         = sizeof(Vec4);
        const VkBufferMemoryBarrier resWriteBarrier = makeBufferMemoryBarrier(
            VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT, **m_resultBuffer, 0ull, resBuffSizeBytes);

        const VkPipelineStageFlags srcPipelineStageFlags =
            isComputeTest ? VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT :
                            ((m_params.stageMask & VK_SHADER_STAGE_VERTEX_BIT) ? VK_PIPELINE_STAGE_VERTEX_SHADER_BIT :
                                                                                 VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

        vkd.cmdPipelineBarrier(cmdBuffer, srcPipelineStageFlags, VK_PIPELINE_STAGE_HOST_BIT, (VkDependencyFlags)0, 0,
                               nullptr, 1, &resWriteBarrier, 0, nullptr);
    }

    {
        const IVec2 resultSize{static_cast<int>(m_outImageSize.x()), static_cast<int>(m_outImageSize.y())};
        const VkAccessFlags srcAccessMask =
            isComputeTest ? VK_ACCESS_SHADER_WRITE_BIT : VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        const VkImageLayout oldLayout =
            isComputeTest ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        copyImageToBuffer(vkd, cmdBuffer, resultImage.get(), resultBuffer.get(), resultSize, srcAccessMask, oldLayout);
    }

    endCommandBuffer(vkd, cmdBuffer);
    submitCommandsAndWait(vkd, device, queue, cmdBuffer);
}

std::vector<Vec4> ImageProcessingWeightSamplingTestInstance::buildStandardResult(ImageProcessingResult &expectedResult,
                                                                                 const BufferWithMemory &texColorBuffer,
                                                                                 const BufferWithMemory &weightBuffer)
{
    const auto &vkd   = m_context.getDeviceInterface();
    const auto device = m_context.getDevice();

    const IVec2 texRegionSize{static_cast<int>(m_params.sampledImageParams.imageSize.x()),
                              static_cast<int>(m_params.sampledImageParams.imageSize.y())};
    auto &texColorBufferAlloc = texColorBuffer.getAllocation();
    auto texColorBufferPtr =
        reinterpret_cast<char *>(texColorBufferAlloc.getHostPtr()) + texColorBufferAlloc.getOffset();
    const PixelBufferAccess texColorBufferPix{mapVkFormat(m_params.sampledImageParams.format), texRegionSize[0],
                                              texRegionSize[1], 1, texColorBufferPtr};

    const IVec3 weightRegionSize{static_cast<int>(m_weightSamplingParams.weightImageParams.imageSize.x()),
                                 static_cast<int>(m_weightSamplingParams.weightImageParams.imageSize.y()),
                                 static_cast<int>(m_weightSamplingParams.weightImageLayers)};
    auto &weightBufferAlloc = weightBuffer.getAllocation();
    auto weightBufferPtr    = reinterpret_cast<char *>(weightBufferAlloc.getHostPtr()) + weightBufferAlloc.getOffset();
    const PixelBufferAccess weightBufferPix{mapVkFormat(m_weightSamplingParams.weightImageParams.format),
                                            weightRegionSize[0], weightRegionSize[1], weightRegionSize[2],
                                            weightBufferPtr};

    const auto candidates = expectedResult.getWeightSamplingResultCandidates(
        m_weightSamplingParams.isUnnormCoord,
        (m_weightSamplingParams.weightImageParams.imageType != IMAGE_TYPE_1D_ARRAY), texColorBufferPix,
        m_weightSamplingParams.textureCoord, weightBufferPix, m_weightSamplingParams.filterCenter,
        m_weightSamplingParams.filterSize, m_weightSamplingParams.filterPhases,
        m_weightSamplingParams.weightImageParams.addrMode,
        getVkSamplerReductionMode(m_weightSamplingParams.weightImageParams.reductionMode),
        m_params.sampledImageParams.components);

    flushAlloc(vkd, device, texColorBufferAlloc);
    flushAlloc(vkd, device, weightBufferAlloc);

    return candidates;
}

Move<VkImageView> ImageProcessingWeightSamplingTestInstance::makeWeightImageView(
    const DeviceInterface &vk, const VkDevice vkDevice, const VkImage image,
    const VkImageSubresourceRange subresourceRange)
{
    const VkImageViewSampleWeightCreateInfoQCOM weightViewParams = {
        VK_STRUCTURE_TYPE_IMAGE_VIEW_SAMPLE_WEIGHT_CREATE_INFO_QCOM, // VkStructureType	sType;
        nullptr,                                                     // const void*		pNext;
        m_weightSamplingParams.filterCenter,                         // VkOffset2D		filterCenter;
        m_weightSamplingParams.filterSize,                           // VkExtent2D		filterSize;
        m_weightSamplingParams.filterPhases,                         // uint32_t		numPhases;
    };

    const VkImageViewCreateInfo imageViewParams = {
        VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,                             // VkStructureType sType;
        &weightViewParams,                                                    // const void* pNext;
        0u,                                                                   // VkImageViewCreateFlags flags;
        image,                                                                // VkImage image;
        mapImageViewType(m_weightSamplingParams.weightImageParams.imageType), // VkImageViewType viewType;
        m_weightSamplingParams.weightImageParams.format,                      // VkFormat format;
        m_weightSamplingParams.weightImageParams.components,                  // VkComponentMapping components;
        subresourceRange, // VkImageSubresourceRange subresourceRange;
    };
    return createImageView(vk, vkDevice, &imageViewParams);
}

class ImageProcessingWeightSamplingGraphicsTestInstance : public ImageProcessingWeightSamplingTestInstance
{
public:
    ImageProcessingWeightSamplingGraphicsTestInstance(Context &context, const TestParams &params,
                                                      const WeightSamplingTestParams &weightSamplingTestParams,
                                                      const ImageType outImageType, const UVec2 outImageSize,
                                                      const VkFormat outImageFormat);
    ~ImageProcessingWeightSamplingGraphicsTestInstance(void);

    void makeRenderPass();
    void makeGraphicsPipeline(const PipelineLayoutWrapper &pipelineLayout, const VkExtent3D extent,
                              const ShaderWrapper &vertexModule, const ShaderWrapper &fragModule);

    virtual void executeBarriers();
    virtual void executeBegin();
    virtual void executeBindPipeline();
    virtual void executeBindOtherBindings();
    virtual void executeProgram();
    virtual void executeEnd();

    virtual TestStatus iterate(void);

protected:
    RenderPassWrapper m_renderPass;
    GraphicsPipelineWrapper m_graphicsPipeline;
    std::vector<VertexData> m_vertexData;
    VkDeviceSize m_vertexBufferSize;
    de::MovePtr<BufferWithMemory> m_vertexBuffer;
};

ImageProcessingWeightSamplingGraphicsTestInstance::ImageProcessingWeightSamplingGraphicsTestInstance(
    Context &context, const TestParams &testParams, const WeightSamplingTestParams &weightSamplingTestParams,
    const ImageType outImageType, const UVec2 outImageSize, const VkFormat outImageFormat)
    : ImageProcessingWeightSamplingTestInstance(context, testParams, weightSamplingTestParams, outImageType,
                                                outImageSize, outImageFormat)
    , m_graphicsPipeline(context.getInstanceInterface(), context.getDeviceInterface(), context.getPhysicalDevice(),
                         context.getDevice(), context.getDeviceExtensions(), m_params.pipelineConstructionType)
{
    // Positions and texture coordinates
    m_vertexData.push_back(VertexData(Vec2(1.0f, -1.0f)));
    m_vertexData.push_back(VertexData(Vec2(-1.0f, -1.0f)));
    m_vertexData.push_back(VertexData(Vec2(-1.0f, 1.0f)));
    m_vertexData.push_back(VertexData(Vec2(-1.0f, 1.0f)));
    m_vertexData.push_back(VertexData(Vec2(1.0f, -1.0f)));
    m_vertexData.push_back(VertexData(Vec2(1.0f, 1.0f)));
}

ImageProcessingWeightSamplingGraphicsTestInstance::~ImageProcessingWeightSamplingGraphicsTestInstance(void)
{
}

void ImageProcessingWeightSamplingGraphicsTestInstance::makeRenderPass()
{
    const auto &vkd   = m_context.getDeviceInterface();
    const auto device = m_context.getDevice();

    const VkAttachmentDescription colorAttachment = {
        0u,                                       // VkAttachmentDescriptionFlags flags;
        m_outImageFormat,                         // VkFormat format;
        VK_SAMPLE_COUNT_1_BIT,                    // VkSampleCountFlagBits samples;
        VK_ATTACHMENT_LOAD_OP_CLEAR,              // VkAttachmentLoadOp loadOp;
        VK_ATTACHMENT_STORE_OP_STORE,             // VkAttachmentStoreOp storeOp;
        VK_ATTACHMENT_LOAD_OP_DONT_CARE,          // VkAttachmentLoadOp stencilLoadOp;
        VK_ATTACHMENT_STORE_OP_DONT_CARE,         // VkAttachmentStoreOp stencilStoreOp;
        VK_IMAGE_LAYOUT_UNDEFINED,                // VkImageLayout initialLayout;
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, // VkImageLayout finalLayout;
    };

    const VkAttachmentReference colorRef = {
        0u,                                       // uint32_t attachment;
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, // VkImageLayout layout;
    };

    const VkSubpassDescription subpass = {
        0u,                              // VkSubpassDescriptionFlags flags;
        VK_PIPELINE_BIND_POINT_GRAPHICS, // VkPipelineBindPoint pipelineBindPoint;
        0u,                              // uint32_t inputAttachmentCount;
        nullptr,                         // const VkAttachmentReference* pInputAttachments;
        1u,                              // uint32_t colorAttachmentCount;
        &colorRef,                       // const VkAttachmentReference* pColorAttachments;
        0u,                              // const VkAttachmentReference* pResolveAttachments;
        nullptr,                         // const VkAttachmentReference* pDepthStencilAttachment;
        0u,                              // uint32_t preserveAttachmentCount;
        nullptr,                         // const uint32_t* pPreserveAttachments;
    };

    const VkRenderPassCreateInfo renderPassInfo = {
        VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO, // VkStructureType sType;
        nullptr,                                   // const void* pNext;
        0u,                                        // VkRenderPassCreateFlags flags;
        1u,                                        // uint32_t attachmentCount;
        &colorAttachment,                          // const VkAttachmentDescription* pAttachments;
        1u,                                        // uint32_t subpassCount;
        &subpass,                                  // const VkSubpassDescription* pSubpasses;
        0u,                                        // uint32_t dependencyCount;
        nullptr,                                   // const VkSubpassDependency* pDependencies;
    };

    m_renderPass = RenderPassWrapper(m_params.pipelineConstructionType, vkd, device, &renderPassInfo);
}

void ImageProcessingWeightSamplingGraphicsTestInstance::makeGraphicsPipeline(
    const PipelineLayoutWrapper &pipelineLayout, const VkExtent3D extent, const ShaderWrapper &vertexModule,
    const ShaderWrapper &fragModule)
{
    const std::vector<VkViewport> viewports{makeViewport(extent)};
    const VkRect2D renderArea = makeRect2D(extent);
    const std::vector<VkRect2D> scissors{renderArea};

    const auto vertBindingDesc   = VertexData::getBindingDescription();
    const auto vertAttributeDesc = VertexData::getAttributeDescriptions();

    const VkPipelineVertexInputStateCreateInfo vertexInputInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, // VkStructureType                             sType
        nullptr,                                                   // const void*                                 pNext
        0u,                                                        // VkPipelineVertexInputStateCreateFlags       flags
        1u,                                // uint32_t                                    vertexBindingDescriptionCount
        &vertBindingDesc,                  // const VkVertexInputBindingDescription*      pVertexBindingDescriptions
        de::sizeU32(vertAttributeDesc),    // uint32_t                     vertexAttributeDescriptionCount
        de::dataOrNull(vertAttributeDesc), // const VkVertexInputAttributeDescription*    pVertexAttributeDescriptions
    };

    m_graphicsPipeline.setDefaultDepthStencilState()
        .setDefaultRasterizationState()
        .setDefaultMultisampleState()
        .setDefaultColorBlendState()
        .setupVertexInputState(&vertexInputInfo)
        .setupPreRasterizationShaderState(viewports, scissors, pipelineLayout, *m_renderPass, 0u, vertexModule)
        .setupFragmentShaderState(pipelineLayout, *m_renderPass, 0u, fragModule)
        .setupFragmentOutputState(*m_renderPass)
        .setMonolithicPipelineLayout(pipelineLayout)
        .buildPipeline();
}

void ImageProcessingWeightSamplingGraphicsTestInstance::executeBarriers()
{
    const auto &vkd                = m_context.getDeviceInterface();
    const auto vertexBufferBarrier = makeBufferMemoryBarrier(
        VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT, m_vertexBuffer->get(), 0ull, m_vertexBufferSize);

    vkd.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, 0u, 0u,
                           nullptr, 1u, &vertexBufferBarrier, 0u, nullptr);
}

void ImageProcessingWeightSamplingGraphicsTestInstance::executeBegin()
{
    const auto &vkd = m_context.getDeviceInterface();
    const Vec4 clearColor(tcu::RGBA::black().toVec());
    const VkExtent3D extent   = makeExtent3D(m_outImageSize.x(), m_outImageSize.y(), 1u);
    const VkRect2D renderArea = makeRect2D(extent.width, extent.height);

    m_renderPass.begin(vkd, *m_cmdBuffer, renderArea, clearColor);
}

void ImageProcessingWeightSamplingGraphicsTestInstance::executeBindPipeline()
{
    m_graphicsPipeline.bind(*m_cmdBuffer);
}

void ImageProcessingWeightSamplingGraphicsTestInstance::executeBindOtherBindings()
{
    const auto &vkd                       = m_context.getDeviceInterface();
    const VkDeviceSize vertexBufferOffset = 0ull;

    vkd.cmdBindVertexBuffers(*m_cmdBuffer, 0u, 1u, &m_vertexBuffer->get(), &vertexBufferOffset);
}

void ImageProcessingWeightSamplingGraphicsTestInstance::executeProgram()
{
    const auto &vkd = m_context.getDeviceInterface();

    vkd.cmdDraw(*m_cmdBuffer, de::sizeU32(m_vertexData), 1u, 0u, 0u);
}

void ImageProcessingWeightSamplingGraphicsTestInstance::executeEnd()
{
    const auto &vkd = m_context.getDeviceInterface();

    m_renderPass.end(vkd, *m_cmdBuffer);
}

TestStatus ImageProcessingWeightSamplingGraphicsTestInstance::iterate(void)
{
    const auto &vkd   = m_context.getDeviceInterface();
    const auto device = m_context.getDevice();
    auto &allocator   = m_context.getDefaultAllocator();

    const auto texUsage       = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    const auto weightImgUsage = VK_IMAGE_USAGE_SAMPLE_WEIGHT_BIT_QCOM | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    const auto texImageViewType   = mapImageViewType(m_params.sampledImageParams.imageType);
    const auto outImageUsage      = (VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
    const auto texTcuFormat       = mapVkFormat(m_params.sampledImageParams.format);
    const auto weightImgTcuFormat = mapVkFormat(m_weightSamplingParams.weightImageParams.format);

    const bool isNonSeparableFilter = (m_weightSamplingParams.weightImageParams.imageType != IMAGE_TYPE_1D_ARRAY);

    // Vertex buffer
    m_vertexBufferSize = static_cast<VkDeviceSize>(m_vertexData.size() * sizeof(decltype(m_vertexData)::value_type));

    m_vertexBuffer = de::MovePtr<BufferWithMemory>(new BufferWithMemory(
        vkd, device, allocator, makeBufferCreateInfo(m_vertexBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT),
        MemoryRequirement::HostVisible));
    // Copy data to vertex buffer
    {
        const auto &vertexAlloc  = m_vertexBuffer->getAllocation();
        const auto vertexDataPtr = reinterpret_cast<char *>(vertexAlloc.getHostPtr()) + vertexAlloc.getOffset();
        deMemcpy(vertexDataPtr, m_vertexData.data(), static_cast<size_t>(m_vertexBufferSize));
        flushAlloc(vkd, device, vertexAlloc);
    }

    // Create sampled image and weight image
    const VkImageCreateInfo sampledImageCreateInfo =
        makeImageCreateInfo(m_params.sampledImageParams.imageType, m_params.sampledImageParams.imageSize,
                            m_params.sampledImageParams.format, texUsage, 0u, m_params.sampledImageParams.tiling);
    const VkImageCreateInfo weightImageCreateInfo = makeImageCreateInfo(
        m_weightSamplingParams.weightImageParams.imageType, m_weightSamplingParams.weightImageParams.imageSize,
        m_weightSamplingParams.weightImageParams.format, weightImgUsage, 0u,
        m_weightSamplingParams.weightImageParams.tiling, m_weightSamplingParams.weightImageLayers);

    const ImageWithMemory sampledImage{vkd, device, allocator, sampledImageCreateInfo, MemoryRequirement::Any};
    const ImageWithMemory weightImage{vkd, device, allocator, weightImageCreateInfo, MemoryRequirement::Any};

    // Corresponding image views
    const auto colorSubresourceRange = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);

    m_sampledImageView =
        makeImageViewUtil(vkd, device, sampledImage.get(), texImageViewType, m_params.sampledImageParams.format,
                          colorSubresourceRange, m_params.sampledImageParams.components);

    const auto weightImgColorSubresourceRange =
        makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, m_weightSamplingParams.weightImageLayers);
    m_weightImageView = makeWeightImageView(vkd, device, weightImage.get(), weightImgColorSubresourceRange);

    // Create textures
    const VkDeviceSize texColorBufferSize = static_cast<VkDeviceSize>(
        static_cast<uint32_t>(getPixelSize(texTcuFormat)) * m_params.sampledImageParams.imageSize.x() *
        m_params.sampledImageParams.imageSize.y() * 1u);
    const auto texBufferInfo = makeBufferCreateInfo(texColorBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);

    const BufferWithMemory texColorBuffer{vkd, device, allocator, texBufferInfo, MemoryRequirement::HostVisible};

    // Create weight buffer
    const VkDeviceSize weightBufferSize = static_cast<VkDeviceSize>(
        static_cast<uint32_t>(getPixelSize(weightImgTcuFormat)) *
        m_weightSamplingParams.weightImageParams.imageSize.x() *
        m_weightSamplingParams.weightImageParams.imageSize.y() * m_weightSamplingParams.weightImageLayers);
    const auto weightBufferInfo = makeBufferCreateInfo(weightBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);

    const BufferWithMemory weightBuffer{vkd, device, allocator, weightBufferInfo, MemoryRequirement::HostVisible};

    // Fill sampled image's color buffer
    populateColorBuffer(texColorBuffer, m_params.sampledImageParams.imageSize, m_params.sampledImageParams.format);

    // Fill weight buffer
    if (isNonSeparableFilter)
        populateNonSeparableWeightBuffer(weightBuffer, m_weightSamplingParams.weightImageParams.imageSize,
                                         m_weightSamplingParams.weightImageLayers,
                                         m_weightSamplingParams.weightImageParams.format);
    else
        populateSeparableWeightBuffer(weightBuffer, m_weightSamplingParams.weightImageParams.imageSize,
                                      m_weightSamplingParams.weightImageLayers,
                                      m_weightSamplingParams.weightImageParams.format);

    // Prepare inputs and outputs
    const VkDeviceSize resBuffSizeBytes = sizeof(Vec4);
    m_resultBuffer                      = de::MovePtr<BufferWithMemory>(new BufferWithMemory(
        vkd, device, allocator, makeBufferCreateInfo(resBuffSizeBytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT),
        MemoryRequirement::HostVisible));

    m_sampledImageSampler = makeSampler(m_weightSamplingParams.isUnnormCoord, m_params.sampledImageParams.addrMode,
                                        m_params.sampledImageParams.reductionMode);

    m_weightImageSampler = makeSampler(true /* unnormalized coordinates */,
                                       m_weightSamplingParams.weightImageParams.addrMode, SAMPLER_REDUCTION_MODE_NONE);

    prepareDescriptors();

    // Push constant range
    const VkPushConstantRange pcRange = {
        m_params.stageMask,                               // VkShaderStageFlags stageFlags;
        0u,                                               // uint32_t offset;
        static_cast<uint32_t>(sizeof(TestPushConstants)), // uint32_t size;
    };

    // Shader modules
    const auto vertexModule = ShaderWrapper(vkd, device, m_context.getBinaryCollection().get("vert"), 0u);
    const auto fragModule   = ShaderWrapper(vkd, device, m_context.getBinaryCollection().get("frag"), 0u);

    // Command pool and command buffer
    prepareCommandBuffer();

    // Render pass
    makeRenderPass();

    // Framebuffer
    const ImageWithMemory colorImage{vkd, device, allocator,
                                     makeImageCreateInfo(m_outImageType, m_outImageSize, m_outImageFormat,
                                                         outImageUsage, 0u, vk::VK_IMAGE_TILING_OPTIMAL),
                                     MemoryRequirement::Any};
    const auto colorView = makeImageViewUtil(vkd, device, colorImage.get(), VK_IMAGE_VIEW_TYPE_2D, m_outImageFormat,
                                             colorSubresourceRange);

    const VkExtent3D extent = makeExtent3D(m_outImageSize.x(), m_outImageSize.y(), 1u);
    m_renderPass.createFramebuffer(vkd, device, 1u, &colorImage.get(), &colorView.get(), extent.width, extent.height,
                                   extent.depth);

    // Pipeline layout
    const VkPipelineLayoutCreateInfo pipelineLayoutInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, // VkStructureType sType;
        nullptr,                                       // const void* pNext;
        0u,                                            // VkPipelineLayoutCreateFlags flags;
        1u,                                            // uint32_t setLayoutCount;
        &m_descriptorSetLayout.get(),                  // const VkDescriptorSetLayout* pSetLayouts;
        1u,                                            // uint32_t pushConstantRangeCount;
        &pcRange,                                      // const VkPushConstantRange* pPushConstantRanges;
    };
    const PipelineLayoutWrapper pipelineLayout(m_params.pipelineConstructionType, vkd, device, &pipelineLayoutInfo);

    // Graphics pipeline
    makeGraphicsPipeline(pipelineLayout, extent, vertexModule, fragModule);

    // Result = red on zero, green on non-zero
    const VkDeviceSize resultBufferSize =
        static_cast<VkDeviceSize>(static_cast<uint32_t>(getPixelSize(mapVkFormat(m_outImageFormat))) * extent.width *
                                  extent.height * extent.depth);
    const IVec2 resultSize{static_cast<int>(extent.width), static_cast<int>(extent.height)};
    const auto resultBufferInfo = makeBufferCreateInfo(resultBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    const BufferWithMemory resultBuffer{vkd, device, allocator, resultBufferInfo, MemoryRequirement::HostVisible};

    // Command execution
    executeCommands(pipelineLayout.get(), texColorBuffer, sampledImage, weightBuffer, weightImage, resultBuffer,
                    colorImage);

    // Get results
    const auto &resultBufferAlloc = resultBuffer.getAllocation();
    invalidateAlloc(vkd, device, resultBufferAlloc);
    const auto &resBufferAllocation = (*m_resultBuffer).getAllocation();
    invalidateAlloc(vkd, device, resBufferAllocation);

    const auto resultsBufferPtr =
        reinterpret_cast<const char *>(resultBufferAlloc.getHostPtr()) + resultBufferAlloc.getOffset();
    const ConstPixelBufferAccess resultPixels{mapVkFormat(m_outImageFormat), resultSize[0], resultSize[1], 1,
                                              resultsBufferPtr};

    const Vec4 *resBufferPtr = static_cast<Vec4 *>(resBufferAllocation.getHostPtr());

    // Get reference result
    const VkSamplerReductionMode redMode = getVkSamplerReductionMode(m_params.sampledImageParams.reductionMode);
    ImageProcessingResult expectedResult(mapVkFormat(m_outImageFormat), m_outImageSize.x(), m_outImageSize.y(),
                                         m_params.sampledImageParams.addrMode, redMode);
    const auto expectedCandidates = buildStandardResult(expectedResult, texColorBuffer, weightBuffer);
    const Vec4 resultValue        = *resBufferPtr;

    const Vec4 errorThreshold = Vec4(0.5f, 0.5f, 0.5f, 0.5f);
    const bool skipComparison = ((m_params.sampledImageParams.reductionMode == SAMPLER_REDUCTION_MODE_MIN) ||
                                 (m_params.sampledImageParams.reductionMode == SAMPLER_REDUCTION_MODE_MAX)) &&
                                (m_weightSamplingParams.weightValue == WEIGHT_TYPE_OTHER);

    if (skipComparison)
        return TestStatus::pass("Pass");

    // Accept the result if it falls within the threshold of any of the expected candidates
    for (const auto &expectedValue : expectedCandidates)
    {
        const Vec4 diff = abs(expectedValue - resultValue);
        if (boolAll(lessThanEqual(diff, errorThreshold)) &&
            ((expectedValue != Vec4(0.0f)) == (resultValue != Vec4(0.0f))))
        {
            // Repaint the reference image to match the accepted candidate
            tcu::clear(expectedResult.getAccess(),
                       (expectedValue != Vec4(0.0f)) ? RGBA::green().toVec() : RGBA::red().toVec());
            return verifyResult(expectedValue, resultValue, expectedResult.getAccess(), resultPixels, errorThreshold);
        }
    }

    // No candidate matched, report failure against the primary (floor/floor) candidate
    return verifyResult(expectedCandidates[0], resultValue, expectedResult.getAccess(), resultPixels, errorThreshold);
}

class ImageProcessingWeightSamplingComputeTest : public ImageProcessingWeightSamplingTest
{
public:
    ImageProcessingWeightSamplingComputeTest(TestContext &testCtx, const std::string &name,
                                             const TestParams &testParams,
                                             const WeightSamplingTestParams &weightSamplingTestParams);
    virtual ~ImageProcessingWeightSamplingComputeTest(void);

    virtual void checkSupport(Context &context) const;
    virtual void initPrograms(SourceCollections &sourceCollections) const;
    virtual TestInstance *createInstance(Context &context) const;
};

ImageProcessingWeightSamplingComputeTest::ImageProcessingWeightSamplingComputeTest(
    TestContext &testCtx, const std::string &name, const TestParams &testParams,
    const WeightSamplingTestParams &weightSamplingTestParams)
    : ImageProcessingWeightSamplingTest(testCtx, name, testParams, weightSamplingTestParams)
{
}

ImageProcessingWeightSamplingComputeTest::~ImageProcessingWeightSamplingComputeTest(void)
{
}

void ImageProcessingWeightSamplingComputeTest::checkSupport(Context &context) const
{
    const auto &vki           = context.getInstanceInterface();
    const auto physicalDevice = context.getPhysicalDevice();

    ImageProcessingWeightSamplingTest::checkSupport(context);

    {
        VkFormatProperties3 formatProperties3 = initVulkanStructure();
        VkFormatProperties2 formatProperties2 = initVulkanStructure(&formatProperties3);
        vki.getPhysicalDeviceFormatProperties2(physicalDevice, m_outImageFormat, &formatProperties2);

        const auto &tilingFeatures = formatProperties3.optimalTilingFeatures;

        if ((m_outImageType == IMAGE_TYPE_2D) && !(tilingFeatures & VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT))
            TCU_THROW(NotSupportedError, "Format not supported for storage images");
    }

    {
        VkImageFormatProperties storeImageFormatProperties;
        const auto result = vki.getPhysicalDeviceImageFormatProperties(
            physicalDevice, m_outImageFormat, mapImageType(m_outImageType), VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT, 0, &storeImageFormatProperties);

        if (result != VK_SUCCESS)
        {
            if (result == VK_ERROR_FORMAT_NOT_SUPPORTED)
                TCU_THROW(NotSupportedError, "Format unsupported for storage image");
            else
                TCU_FAIL("vkGetPhysicalDeviceImageFormatProperties returned unexpected error");
        }
    }

    const auto maxComputeWorkGroupCount = context.getDeviceProperties().limits.maxComputeWorkGroupCount;
    if ((m_outImageSize.x() > maxComputeWorkGroupCount[0]) || (m_outImageSize.y() > maxComputeWorkGroupCount[1]))
        TCU_THROW(NotSupportedError, "Image size exceeds compute limits");
}

void ImageProcessingWeightSamplingComputeTest::initPrograms(SourceCollections &sourceCollections) const
{
    const vk::ShaderBuildOptions shaderBuildOpt(sourceCollections.usedVulkanVersion, vk::SPIRV_VERSION_1_4, 0u, true);

    const std::string imageTypeStr = getFormatPrefix(mapVkFormat(m_outImageFormat)) + "image" +
                                     "2D"; // only 2D image support by weight image sampling

    const bool is2DWeightImage = (m_weightSamplingParams.weightImageParams.imageSize.y() > 1u) ? true : false;

    std::ostringstream comp;
    {
        comp << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
             << "layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;\n"
             << getProgPreMain(is2DWeightImage) << "\n"
             << "layout(set = 0, binding = 5) uniform writeonly " << imageTypeStr << " outputImage;\n"
             << "\n"
             << "void main() {\n"
             << "\n"
             << "    int gx = int(gl_GlobalInvocationID.x);\n"
             << "    int gy = int(gl_GlobalInvocationID.y);\n"
             << "    vec4 outColor = vec4(1.0f, 0.0f, 0.0f, 1.0f);" // red on zero
             << "\n"
             << getProgMainBlock(m_params.imageProcOp, is2DWeightImage)
             << "    imageStore(outputImage, ivec2(gx, gy), outColor);\n"
             << "}\n";
    }
    sourceCollections.glslSources.add("comp") << glu::ComputeSource(comp.str()) << shaderBuildOpt;
}

class ImageProcessingWeightSamplingComputeTestInstance : public ImageProcessingWeightSamplingTestInstance
{
public:
    ImageProcessingWeightSamplingComputeTestInstance(Context &context, const TestParams &params,
                                                     const WeightSamplingTestParams &weightSamplingTestParams,
                                                     const ImageType outImageType, const UVec2 outImageSize,
                                                     const VkFormat outImageFormat);
    ~ImageProcessingWeightSamplingComputeTestInstance(void);

    virtual void addSupplementaryDescBindings(DescriptorSetLayoutExtBuilder &layoutBuilder);
    virtual void addSupplementaryDescTypes(DescriptorPoolBuilder &poolBuilder);
    virtual void writeSupplementaryDescriptors();

    virtual void executeBarriers();
    virtual void executeBindPipeline();
    virtual void executeProgram();

    virtual TestStatus iterate(void);

protected:
    de::MovePtr<ImageWithMemory> m_outImage;
    Move<VkImageView> m_outImageView;
    Move<VkPipeline> m_computePipeline;
};

ImageProcessingWeightSamplingComputeTestInstance::ImageProcessingWeightSamplingComputeTestInstance(
    Context &context, const TestParams &testParams, const WeightSamplingTestParams &weightSamplingTestParams,
    const ImageType outImageType, const UVec2 outImageSize, const VkFormat outImageFormat)
    : ImageProcessingWeightSamplingTestInstance(context, testParams, weightSamplingTestParams, outImageType,
                                                outImageSize, outImageFormat)
{
}

ImageProcessingWeightSamplingComputeTestInstance::~ImageProcessingWeightSamplingComputeTestInstance(void)
{
}

void ImageProcessingWeightSamplingComputeTestInstance::addSupplementaryDescBindings(
    DescriptorSetLayoutExtBuilder &layoutBuilder)
{
    layoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, m_params.stageMask);
}

void ImageProcessingWeightSamplingComputeTestInstance::addSupplementaryDescTypes(DescriptorPoolBuilder &poolBuilder)
{
    poolBuilder.addType(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
}

void ImageProcessingWeightSamplingComputeTestInstance::writeSupplementaryDescriptors()
{
    const auto storeDescImageInfo =
        makeDescriptorImageInfo(VK_NULL_HANDLE, m_outImageView.get(), VK_IMAGE_LAYOUT_GENERAL);
    m_descriptorUpdateBuilder.writeSingle(m_descriptorSet.get(), DescriptorSetUpdateBuilder::Location::binding(5u),
                                          VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &storeDescImageInfo);
}

void ImageProcessingWeightSamplingComputeTestInstance::executeBarriers()
{
    const auto &vkd                  = m_context.getDeviceInterface();
    const auto colorSubresourceRange = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
    const VkImageMemoryBarrier outImageBarrier =
        makeImageMemoryBarrier(0u, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
                               m_outImage->get(), colorSubresourceRange);

    vkd.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                           (VkDependencyFlags)0, 0, nullptr, 0, nullptr, 1, &outImageBarrier);
}

void ImageProcessingWeightSamplingComputeTestInstance::executeBindPipeline()
{
    const auto &vkd = m_context.getDeviceInterface();

    vkd.cmdBindPipeline(*m_cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_computePipeline.get());
}

void ImageProcessingWeightSamplingComputeTestInstance::executeProgram()
{
    const auto &vkd         = m_context.getDeviceInterface();
    const VkExtent3D extent = makeExtent3D(m_outImageSize.x(), m_outImageSize.y(), 1u);

    vkd.cmdDispatch(*m_cmdBuffer, extent.width, extent.height, extent.depth);
}

TestStatus ImageProcessingWeightSamplingComputeTestInstance::iterate(void)
{
    const auto &vkd   = m_context.getDeviceInterface();
    const auto device = m_context.getDevice();
    auto &allocator   = m_context.getDefaultAllocator();

    const auto texUsage       = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    const auto weightImgUsage = VK_IMAGE_USAGE_SAMPLE_WEIGHT_BIT_QCOM | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    const auto texImageViewType   = mapImageViewType(m_params.sampledImageParams.imageType);
    const auto outImageUsage      = (VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT);
    const auto texTcuFormat       = mapVkFormat(m_params.sampledImageParams.format);
    const auto weightImgTcuFormat = mapVkFormat(m_weightSamplingParams.weightImageParams.format);

    const bool isNonSeparableFilter = (m_weightSamplingParams.weightImageParams.imageType != IMAGE_TYPE_1D_ARRAY);

    // Create sampled image and weight image
    const VkImageCreateInfo sampledImageCreateInfo =
        makeImageCreateInfo(m_params.sampledImageParams.imageType, m_params.sampledImageParams.imageSize,
                            m_params.sampledImageParams.format, texUsage, 0u, m_params.sampledImageParams.tiling);
    const VkImageCreateInfo weightImageCreateInfo = makeImageCreateInfo(
        m_weightSamplingParams.weightImageParams.imageType, m_weightSamplingParams.weightImageParams.imageSize,
        m_weightSamplingParams.weightImageParams.format, weightImgUsage, 0u,
        m_weightSamplingParams.weightImageParams.tiling, m_weightSamplingParams.weightImageLayers);

    const ImageWithMemory sampledImage{vkd, device, allocator, sampledImageCreateInfo, MemoryRequirement::Any};
    const ImageWithMemory weightImage{vkd, device, allocator, weightImageCreateInfo, MemoryRequirement::Any};

    // Corresponding image views
    const auto colorSubresourceRange = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);

    m_sampledImageView =
        makeImageViewUtil(vkd, device, sampledImage.get(), texImageViewType, m_params.sampledImageParams.format,
                          colorSubresourceRange, m_params.sampledImageParams.components);

    const auto weightImgColorSubresourceRange =
        makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, m_weightSamplingParams.weightImageLayers);

    m_weightImageView = makeWeightImageView(vkd, device, weightImage.get(), weightImgColorSubresourceRange);

    // Create textures
    const VkDeviceSize texColorBufferSize = static_cast<VkDeviceSize>(
        static_cast<uint32_t>(getPixelSize(texTcuFormat)) * m_params.sampledImageParams.imageSize.x() *
        m_params.sampledImageParams.imageSize.y() * 1u);
    const auto texBufferInfo = makeBufferCreateInfo(texColorBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);

    const BufferWithMemory texColorBuffer{vkd, device, allocator, texBufferInfo, MemoryRequirement::HostVisible};

    // Create weight buffer
    const VkDeviceSize weightBufferSize = static_cast<VkDeviceSize>(
        static_cast<uint32_t>(getPixelSize(weightImgTcuFormat)) *
        m_weightSamplingParams.weightImageParams.imageSize.x() *
        m_weightSamplingParams.weightImageParams.imageSize.y() * m_weightSamplingParams.weightImageLayers);
    const auto weightBufferInfo = makeBufferCreateInfo(weightBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);

    const BufferWithMemory weightBuffer{vkd, device, allocator, weightBufferInfo, MemoryRequirement::HostVisible};

    // Fill sampled Image's color buffer
    populateColorBuffer(texColorBuffer, m_params.sampledImageParams.imageSize, m_params.sampledImageParams.format);

    // Fill weight buffer
    if (isNonSeparableFilter)
        populateNonSeparableWeightBuffer(weightBuffer, m_weightSamplingParams.weightImageParams.imageSize,
                                         m_weightSamplingParams.weightImageLayers,
                                         m_weightSamplingParams.weightImageParams.format);
    else
        populateSeparableWeightBuffer(weightBuffer, m_weightSamplingParams.weightImageParams.imageSize,
                                      m_weightSamplingParams.weightImageLayers,
                                      m_weightSamplingParams.weightImageParams.format);

    // Prepare inputs and outputs
    const VkDeviceSize resBuffSizeBytes = sizeof(Vec4);
    m_resultBuffer                      = de::MovePtr<BufferWithMemory>(new BufferWithMemory(
        vkd, device, allocator, makeBufferCreateInfo(resBuffSizeBytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT),
        MemoryRequirement::HostVisible));

    m_sampledImageSampler = makeSampler(m_weightSamplingParams.isUnnormCoord, m_params.sampledImageParams.addrMode,
                                        m_params.sampledImageParams.reductionMode);

    m_weightImageSampler = makeSampler(true /* unnormalized coordinates */,
                                       m_weightSamplingParams.weightImageParams.addrMode, SAMPLER_REDUCTION_MODE_NONE);

    const VkImageCreateInfo outImageCreateInfo = makeImageCreateInfo(m_outImageType, m_outImageSize, m_outImageFormat,
                                                                     outImageUsage, 0u, VK_IMAGE_TILING_OPTIMAL);
    m_outImage                                 = de::MovePtr<ImageWithMemory>(
        new ImageWithMemory(vkd, device, allocator, outImageCreateInfo, MemoryRequirement::Any));

    m_outImageView = makeImageViewUtil(vkd, device, m_outImage->get(), VK_IMAGE_VIEW_TYPE_2D, m_outImageFormat,
                                       colorSubresourceRange);

    prepareDescriptors();

    // Push constant range
    const VkPushConstantRange pcRange = {
        VK_SHADER_STAGE_COMPUTE_BIT,                      // VkShaderStageFlags stageFlags;
        0u,                                               // uint32_t offset;
        static_cast<uint32_t>(sizeof(TestPushConstants)), // uint32_t size;
    };

    // Shader modules
    const Unique<VkShaderModule> computeModule(
        createShaderModule(vkd, device, m_context.getBinaryCollection().get("comp"), 0));

    // Command pool and command buffer
    prepareCommandBuffer();

    // Pipeline layout
    const Unique<VkPipelineLayout> pipelineLayout(
        makePipelineLayout(vkd, device, m_descriptorSetLayout.get(), &pcRange));

    // Create compute pipeline
    m_computePipeline = makeComputePipeline(vkd, device, *pipelineLayout, *computeModule);

    // Result = red on zero, green on non-zero
    const VkExtent3D extent = makeExtent3D(m_outImageSize.x(), m_outImageSize.y(), 1u);
    const VkDeviceSize resultBufferSize =
        static_cast<VkDeviceSize>(static_cast<uint32_t>(getPixelSize(mapVkFormat(m_outImageFormat))) * extent.width *
                                  extent.height * extent.depth);
    const IVec2 resultSize{static_cast<int>(extent.width), static_cast<int>(extent.height)};
    const auto resultBufferInfo = makeBufferCreateInfo(resultBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    const BufferWithMemory resultBuffer{vkd, device, allocator, resultBufferInfo, MemoryRequirement::HostVisible};

    // Command execution
    executeCommands(pipelineLayout.get(), texColorBuffer, sampledImage, weightBuffer, weightImage, resultBuffer,
                    *m_outImage);

    // Get results
    const auto &resultBufferAlloc = resultBuffer.getAllocation();
    invalidateAlloc(vkd, device, resultBufferAlloc);
    const auto &resBufferAllocation = (*m_resultBuffer).getAllocation();
    invalidateAlloc(vkd, device, resBufferAllocation);

    const auto resultsBufferPtr =
        reinterpret_cast<const char *>(resultBufferAlloc.getHostPtr()) + resultBufferAlloc.getOffset();
    const ConstPixelBufferAccess resultPixels{mapVkFormat(m_outImageFormat), resultSize[0], resultSize[1], 1,
                                              resultsBufferPtr};

    const Vec4 *resBufferPtr = static_cast<Vec4 *>(resBufferAllocation.getHostPtr());

    // Get reference result
    const VkSamplerReductionMode redMode = getVkSamplerReductionMode(m_params.sampledImageParams.reductionMode);
    ImageProcessingResult expectedResult(mapVkFormat(m_outImageFormat), m_outImageSize.x(), m_outImageSize.y(),
                                         m_params.sampledImageParams.addrMode, redMode);
    const auto expectedCandidates = buildStandardResult(expectedResult, texColorBuffer, weightBuffer);
    const Vec4 resultValue        = *resBufferPtr;

    const Vec4 errorThreshold = Vec4(0.5f, 0.5f, 0.5f, 0.5f);

    // Accept the result if it falls within the threshold of any of the expected candidates
    for (const auto &expectedValue : expectedCandidates)
    {
        const Vec4 diff = abs(expectedValue - resultValue);
        if (boolAll(lessThanEqual(diff, errorThreshold)) &&
            ((expectedValue != Vec4(0.0f)) == (resultValue != Vec4(0.0f))))
        {
            // Repaint the reference image to match the accepted candidate
            tcu::clear(expectedResult.getAccess(),
                       (expectedValue != Vec4(0.0f)) ? RGBA::green().toVec() : RGBA::red().toVec());
            return verifyResult(expectedValue, resultValue, expectedResult.getAccess(), resultPixels, errorThreshold);
        }
    }

    // No candidate matched, report failure against the primary (floor/floor) candidate
    return verifyResult(expectedCandidates[0], resultValue, expectedResult.getAccess(), resultPixels, errorThreshold);
}

TestInstance *ImageProcessingWeightSamplingComputeTest::createInstance(Context &context) const
{
    return new ImageProcessingWeightSamplingComputeTestInstance(context, m_params, m_weightSamplingParams,
                                                                m_outImageType, m_outImageSize, m_outImageFormat);
}

TestInstance *ImageProcessingWeightSamplingGraphicsTest::createInstance(Context &context) const
{
    return new ImageProcessingWeightSamplingGraphicsTestInstance(context, m_params, m_weightSamplingParams,
                                                                 m_outImageType, m_outImageSize, m_outImageFormat);
}
struct CombinedTestParams
{
    TestParams testParams;
    WeightSamplingTestParams weightSamplingParams;
};

CombinedTestParams getCommonTestParams(const ImageProcOp op, const VkFormat format = VK_FORMAT_R8G8B8A8_UNORM,
                                       VkShaderStageFlags stageMask       = VK_SHADER_STAGE_FRAGMENT_BIT,
                                       const bool isNonSeparableFilter    = true,
                                       const bool enableSubtexelFiltering = false, const bool isUnnormCoord = true)
{
    CombinedTestParams combinedParams;

    // Sampled image parameters
    const TestImageParams defaultSampledImageParams = {
        IMAGE_TYPE_2D,                            // imageType
        UVec2(5u, 5u),                            // imageSize
        format,                                   // format
        VK_IMAGE_TILING_OPTIMAL,                  // tiling
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, // layout
        makeComponentMappingIdentity(),           // components
        VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,    // addrMode
        SAMPLER_REDUCTION_MODE_NONE               // reductionMode
    };

    const ImageType weightImageType = isNonSeparableFilter ? IMAGE_TYPE_2D_ARRAY : IMAGE_TYPE_1D_ARRAY;
    const UVec2 weightImageSize     = isNonSeparableFilter ? UVec2(3u, 3u) /* 2D */ : UVec2(16u, 1u) /* 1D */;

    // Weight image parameters
    const TestImageParams defaultWeightImageParams = {
        weightImageType,                          // imageType
        weightImageSize,                          // imageSize
        VK_FORMAT_R8_UNORM,                       // format
        VK_IMAGE_TILING_OPTIMAL,                  // tiling
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, // layout
        makeComponentMappingIdentity(),           // components
        VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,    // addrMode
        SAMPLER_REDUCTION_MODE_NONE               // reductionMode
    };

    const uint32_t numPhases = enableSubtexelFiltering ? 4u : 1u;
    const uint32_t numLayers = isNonSeparableFilter ? numPhases : 2u;
    const Vec2 texCoord      = isUnnormCoord ? (enableSubtexelFiltering ? Vec2(2.5f, 2.5f) : Vec2(2.0f, 2.0f)) :
                                               (enableSubtexelFiltering ? Vec2(0.5f, 0.5f) : Vec2(0.4f, 0.4f));

    const WeightSamplingTestParams defaultWeightSamplingParams = {
        defaultWeightImageParams,
        isUnnormCoord,        // bool isUnnormCoord
        texCoord,             // Vec2 textureCoord
        makeOffset2D(1, 1),   // VkOffset2D filterCenter
        makeExtent2D(3u, 3u), // VkExtent2D filterSize
        numPhases,            // uint32_t filterPhases
        numLayers,            // uint32_t weightImageLayers
        WEIGHT_TYPE_OTHER     // WeightType weightValue
    };

    const TestParams defaultTestParams = {
        op,                                    // imageProcOp
        defaultSampledImageParams,             // sampledImageParams
        true,                                  // randomReference
        false,                                 // updateAfterBind
        PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC, // pipelineConstructionType
        stageMask                              // stageMask
    };

    combinedParams.weightSamplingParams = defaultWeightSamplingParams;
    combinedParams.testParams           = defaultTestParams;

    return combinedParams;
}

std::vector<CombinedTestParams> getWeightFilterTestParams(const ImageProcOp op,
                                                          const VkFormat format = VK_FORMAT_R8G8B8A8_UNORM)
{
    std::vector<CombinedTestParams> combinedParams;

    // Different parameters with nonseparable filter
    {
        CombinedTestParams commonCombinedParams = getCommonTestParams(op, format);

        // Texture coordinate 0, 0
        {
            CombinedTestParams params = commonCombinedParams;

            params.weightSamplingParams.textureCoord = Vec2(0.0f, 0.0f);

            combinedParams.push_back(params);
        }

        // Filter size same as sampled image size
        {
            CombinedTestParams params = commonCombinedParams;

            params.testParams.sampledImageParams.imageSize          = UVec2(6u, 6u);
            params.weightSamplingParams.weightImageParams.imageSize = UVec2(6u, 6u);
            params.weightSamplingParams.filterSize                  = makeExtent2D(6u, 6u);
            params.weightSamplingParams.textureCoord                = Vec2(0.0f, 0.0f);
            params.weightSamplingParams.filterCenter                = makeOffset2D(0, 0);

            combinedParams.push_back(params);
        }

        // Filter size smaller than weight image size
        {
            CombinedTestParams params = commonCombinedParams;

            params.testParams.sampledImageParams.imageSize          = UVec2(6u, 6u);
            params.weightSamplingParams.weightImageParams.imageSize = UVec2(6u, 6u);
            params.weightSamplingParams.filterSize                  = makeExtent2D(3u, 3u);
            params.weightSamplingParams.textureCoord                = Vec2(0.0f, 0.0f);
            params.weightSamplingParams.filterCenter                = makeOffset2D(0, 0);

            combinedParams.push_back(params);
        }
    }

    // Different parameters with separable filter
    {
        CombinedTestParams commonCombinedParams =
            getCommonTestParams(op, format, VK_SHADER_STAGE_FRAGMENT_BIT, false /* isNonSeparableFilter */);

        // Filter size smaller than weight image size with separable filter
        {
            CombinedTestParams params = commonCombinedParams;

            params.testParams.sampledImageParams.imageSize          = UVec2(6u, 6u);
            params.weightSamplingParams.weightImageParams.imageSize = UVec2(32u, 1u);
            params.weightSamplingParams.filterSize                  = makeExtent2D(3u, 3u);
            params.weightSamplingParams.textureCoord                = Vec2(0.0f, 0.0f);
            params.weightSamplingParams.filterCenter                = makeOffset2D(0, 0);

            combinedParams.push_back(params);
        }

        // Filter size greater than 4 with separable filter
        {
            CombinedTestParams params = commonCombinedParams;

            params.testParams.sampledImageParams.imageSize          = UVec2(6u, 6u);
            params.weightSamplingParams.weightImageParams.imageSize = UVec2(32u, 1u);
            params.weightSamplingParams.filterSize                  = makeExtent2D(5u, 5u);
            params.weightSamplingParams.textureCoord                = Vec2(0.0f, 0.0f);
            params.weightSamplingParams.filterCenter                = makeOffset2D(0, 0);

            combinedParams.push_back(params);
        }
    }

    // Different subtexel coordinates
    {
        const Vec2 subTexelCoords[] = {
            Vec2(2.1f, 2.6f), Vec2(2.35f, 2.75f), Vec2(2.0f, 2.0f), // no fractional part
        };

        // With nonseparable filter
        {
            CombinedTestParams params =
                getCommonTestParams(op, format, VK_SHADER_STAGE_FRAGMENT_BIT, true /* isNonSeparableFilter */,
                                    true /* enableSubtexelFiltering */);

            params.testParams.randomReference = false;

            for (uint32_t idx = 0; idx < DE_LENGTH_OF_ARRAY(subTexelCoords); idx++)
            {
                params.weightSamplingParams.textureCoord = subTexelCoords[idx];

                combinedParams.push_back(params);
            }
        }

        // With separable filter
        {
            CombinedTestParams params =
                getCommonTestParams(op, format, VK_SHADER_STAGE_FRAGMENT_BIT, false /* isNonSeparableFilter */,
                                    true /* enableSubtexelFiltering */);

            params.testParams.randomReference = false;

            for (uint32_t idx = 0; idx < DE_LENGTH_OF_ARRAY(subTexelCoords); idx++)
            {
                params.weightSamplingParams.textureCoord = subTexelCoords[idx];

                combinedParams.push_back(params);
            }
        }
    }

    return combinedParams;
}

std::vector<CombinedTestParams> getSamplerAddressModeTestParams(const ImageProcOp op,
                                                                const VkSamplerAddressMode addrMode,
                                                                const VkFormat format)
{
    std::vector<CombinedTestParams> combinedParams;

    // Nonseparable filter
    {
        CombinedTestParams commonCombinedParams = getCommonTestParams(op, format);

        commonCombinedParams.testParams.sampledImageParams.addrMode = addrMode;

        // Only corner pixel included
        {
            CombinedTestParams params0 = commonCombinedParams;

            params0.weightSamplingParams.textureCoord = Vec2(5.0f, 5.0f);

            combinedParams.push_back(params0);
        }

        // Filter area is outside the sampled image
        {
            CombinedTestParams params1 = commonCombinedParams;

            params1.weightSamplingParams.textureCoord = Vec2(6.0f, 6.0f);

            combinedParams.push_back(params1);
        }
    }

    // Separable filter
    {
        CombinedTestParams commonCombinedParams =
            getCommonTestParams(op, format, VK_SHADER_STAGE_FRAGMENT_BIT, false /* isNonSeparableFilter */);

        commonCombinedParams.testParams.sampledImageParams.addrMode = addrMode;

        {
            CombinedTestParams params2 = commonCombinedParams;

            params2.weightSamplingParams.textureCoord = Vec2(5.0f, 5.0f);

            combinedParams.push_back(params2);
        }
    }

    return combinedParams;
}

std::vector<CombinedTestParams> getTilingTestParams(const ImageProcOp op, const VkImageTiling sampledImageTiling,
                                                    const VkFormat format)
{
    std::vector<CombinedTestParams> combinedParams;

    CombinedTestParams commonCombinedParams = getCommonTestParams(op, format);

    commonCombinedParams.testParams.sampledImageParams.tiling = sampledImageTiling;

    for (const auto tilingType : {VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_TILING_LINEAR})
    {
        CombinedTestParams params = commonCombinedParams;

        // Both sampled and weight images using optimal tiling case is covered in basic tests
        if ((sampledImageTiling == VK_IMAGE_TILING_OPTIMAL) && (tilingType == VK_IMAGE_TILING_OPTIMAL))
            continue;

        params.weightSamplingParams.weightImageParams.tiling = tilingType;

        combinedParams.push_back(params);
    }

    return combinedParams;
}

std::vector<CombinedTestParams> getLayoutTestParams(const ImageProcOp op, const VkImageLayout sampledImageLayout,
                                                    const VkFormat format)
{
    std::vector<CombinedTestParams> combinedParams;

    CombinedTestParams commonCombinedParams = getCommonTestParams(op, format);

    commonCombinedParams.testParams.sampledImageParams.layout = sampledImageLayout;

    for (const auto layout :
         {VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL})
    {
        CombinedTestParams params = commonCombinedParams;

        // Both sampled and weight images using optimal layout is covered in basic tests
        if ((sampledImageLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) &&
            (layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL))
            continue;

        params.weightSamplingParams.weightImageParams.layout = layout;

        combinedParams.push_back(params);
    }

    return combinedParams;
}

} // namespace

TestCaseGroup *createImageProcessingWeightImageSamplingCommonTests(
    TestContext &testCtx, const bool testCompute,
    const PipelineConstructionType pipelineConstructionType = PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC)
{
    de::MovePtr<TestCaseGroup> testGroup(new TestCaseGroup(testCtx, "weight_image_sampling"));

    const struct
    {
        const VkSamplerAddressMode addrMode; // U and V should be same
        const std::string addrModeName;

    } addressModes[] = {{VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, "clamp_to_edge"},
                        {VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER, "clamp_to_border"}};

    const struct
    {
        const SamplerReductionMode reductionMode;
        const std::string reductionModeName;
    } reductionModes[] = {{SAMPLER_REDUCTION_MODE_WEIGHTED_AVG, "weighted_average"},
                          {SAMPLER_REDUCTION_MODE_MIN, "min"},
                          {SAMPLER_REDUCTION_MODE_MAX, "max"}};

    const struct
    {
        const WeightType redModeWeighValue;
        const std::string redModeWeightTypeName;
    } reductionModeWeights[] = {
        {WEIGHT_TYPE_ONE, "with_weight_one"},
        {WEIGHT_TYPE_ONE, "with_weight_zero"},
        {WEIGHT_TYPE_ONE, "with_weight_other"},
    };

    const struct
    {
        const VkImageTiling tiling;
        const std::string tilingName;
    } tilingTypes[] = {{VK_IMAGE_TILING_OPTIMAL, "optimal"}, {VK_IMAGE_TILING_LINEAR, "linear"}};

    const struct
    {
        const VkImageLayout layout;
        const std::string layoutName;
    } layouts[] = {{VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, "shader_rdonly_optimal"},
                   {VK_IMAGE_LAYOUT_GENERAL, "general"},
                   {VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL, "rdonly_optimal"}};

    // Non-identity component mapping for sampled image
    const struct
    {
        const VkComponentMapping components;
        const std::string compMappingName;
    } swizzles[] = {
        {{makeComponentMapping(VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_R,
                               VK_COMPONENT_SWIZZLE_A)},
         "bgra"},
        {{makeComponentMapping(VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_ZERO, VK_COMPONENT_SWIZZLE_ONE,
                               VK_COMPONENT_SWIZZLE_A)},
         "g01a"},
        {{makeComponentMapping(VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_IDENTITY,
                               VK_COMPONENT_SWIZZLE_ONE)},
         "rbg1"},
    };

    // Shader stages in which box filter operation is used
    // Fragment stage is already tested in basic tests
    const VkShaderStageFlags shaderStages[]{
        VK_SHADER_STAGE_VERTEX_BIT,
    };

    // Supported formats for weight image
    const std::vector<VkFormat> &supportedWeightImageFormats = {VK_FORMAT_R8_UNORM, VK_FORMAT_R16_SFLOAT};

    const std::vector<VkFormat> &supportedFormats = getOpSupportedFormats(ImageProcOp::IMAGE_PROC_OP_SAMPLE_WEIGHTED);

    // Basic tests
    {
        de::MovePtr<TestCaseGroup> basicGroup(new TestCaseGroup(testCtx, "basic"));

        for (size_t imageFormatNdx = 0; imageFormatNdx < supportedFormats.size(); ++imageFormatNdx)
        {
            for (size_t weightImageFormatNdx = 0; weightImageFormatNdx < supportedWeightImageFormats.size();
                 ++weightImageFormatNdx)
            {
                // isNonSeparableFilter = true, 2D non-separable weight filters
                // isNonSeparableFilter = false, 1D separable weight filters
                for (const auto isNonSeparableFilter : {true, false})
                {
                    // enableSubtexelFiltering = true, phases > 1
                    // enableSubtexelFiltering = true, phases == 1
                    for (const auto enableSubtexelFiltering : {true, false})
                    {
                        // randomReference = true, both sampled and weight images will have random values
                        // randomReference = true, both images will have uniform values
                        for (const auto &randomReference : {true, false})
                        {
                            CombinedTestParams params = getCommonTestParams(
                                ImageProcOp::IMAGE_PROC_OP_SAMPLE_WEIGHTED, supportedFormats[imageFormatNdx],
                                VK_SHADER_STAGE_FRAGMENT_BIT, isNonSeparableFilter, enableSubtexelFiltering);
                            params.testParams.randomReference          = randomReference;
                            params.testParams.pipelineConstructionType = pipelineConstructionType;
                            params.weightSamplingParams.weightImageParams.format =
                                supportedWeightImageFormats[weightImageFormatNdx];

                            const auto testName =
                                getFormatShortString(supportedFormats[imageFormatNdx]) + "_weight_" +
                                getFormatShortString(supportedWeightImageFormats[weightImageFormatNdx]) +
                                (randomReference ? "_random" : "") + (isNonSeparableFilter ? "" : "_separable") +
                                (enableSubtexelFiltering ? "_subtexel" : "");

                            if (!testCompute)
                                basicGroup->addChild(new ImageProcessingWeightSamplingGraphicsTest(
                                    testCtx, testName, params.testParams, params.weightSamplingParams));
                            else
                            {
                                params.testParams.stageMask = VK_SHADER_STAGE_COMPUTE_BIT;

                                basicGroup->addChild(new ImageProcessingWeightSamplingComputeTest(
                                    testCtx, testName, params.testParams, params.weightSamplingParams));
                            }
                        }
                    }
                }
            }
        }

        testGroup->addChild(basicGroup.release());
    }

    // Compute only has basic tests
    if (!testCompute && (pipelineConstructionType == PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC))
    {
        // Different weight sampling parameters
        {
            de::MovePtr<TestCaseGroup> weightFilterParamsGroup(new TestCaseGroup(testCtx, "weight_filter_params"));

            const std::vector<CombinedTestParams> &weightFilterTestParamsList =
                getWeightFilterTestParams(ImageProcOp::IMAGE_PROC_OP_SAMPLE_WEIGHTED);

            for (size_t paramNdx = 0; paramNdx < weightFilterTestParamsList.size(); ++paramNdx)
            {
                CombinedTestParams params = weightFilterTestParamsList[paramNdx];

                const auto testName = "params" + de::toString(paramNdx);

                weightFilterParamsGroup->addChild(new ImageProcessingWeightSamplingGraphicsTest(
                    testCtx, testName, params.testParams, params.weightSamplingParams));
            }

            testGroup->addChild(weightFilterParamsGroup.release());
        }

        // Sampler address modes
        {
            de::MovePtr<TestCaseGroup> addrModesGroup(new TestCaseGroup(testCtx, "address_modes"));

            for (int addrModeNdx = 0; addrModeNdx < DE_LENGTH_OF_ARRAY(addressModes); ++addrModeNdx)
            {
                for (size_t imageFormatNdx = 0; imageFormatNdx < supportedFormats.size(); ++imageFormatNdx)
                {
                    const std::vector<CombinedTestParams> &addrModeTestParamsList = getSamplerAddressModeTestParams(
                        ImageProcOp::IMAGE_PROC_OP_SAMPLE_WEIGHTED, addressModes[addrModeNdx].addrMode,
                        supportedFormats[imageFormatNdx]);

                    for (size_t paramNdx = 0; paramNdx < addrModeTestParamsList.size(); ++paramNdx)
                    {
                        const CombinedTestParams &params = addrModeTestParamsList[paramNdx];

                        const std::string paramsName = "_params" + de::toString(paramNdx);
                        const auto testName          = addressModes[addrModeNdx].addrModeName + paramsName + "_" +
                                              getFormatShortString(supportedFormats[imageFormatNdx]);

                        addrModesGroup->addChild(new ImageProcessingWeightSamplingGraphicsTest(
                            testCtx, testName, params.testParams, params.weightSamplingParams));
                    }
                }
            }

            testGroup->addChild(addrModesGroup.release());
        }

        // Sampler reduction modes
        {
            de::MovePtr<TestCaseGroup> reductionModesGroup(new TestCaseGroup(testCtx, "reduction_modes"));

            // Sampled image reduction modes
            for (int redModeNdx = 0; redModeNdx < DE_LENGTH_OF_ARRAY(reductionModes); ++redModeNdx)
            {
                for (size_t imageFormatNdx = 0; imageFormatNdx < supportedFormats.size(); ++imageFormatNdx)
                {
                    CombinedTestParams params = getCommonTestParams(ImageProcOp::IMAGE_PROC_OP_SAMPLE_WEIGHTED,
                                                                    supportedFormats[imageFormatNdx]);

                    params.testParams.sampledImageParams.reductionMode = reductionModes[redModeNdx].reductionMode;

                    for (int redModeWtIdx = 0; redModeWtIdx < DE_LENGTH_OF_ARRAY(reductionModeWeights); ++redModeWtIdx)
                    {
                        params.weightSamplingParams.weightValue = reductionModeWeights[redModeWtIdx].redModeWeighValue;

                        const auto testName = reductionModes[redModeNdx].reductionModeName + "_" +
                                              reductionModeWeights[redModeWtIdx].redModeWeightTypeName + "_" +
                                              getFormatShortString(supportedFormats[imageFormatNdx]);

                        reductionModesGroup->addChild(new ImageProcessingWeightSamplingGraphicsTest(
                            testCtx, testName, params.testParams, params.weightSamplingParams));
                    }
                }
            }

            testGroup->addChild(reductionModesGroup.release());
        }

        // Tiling
        {
            de::MovePtr<TestCaseGroup> tilingGroup(new TestCaseGroup(testCtx, "tiling"));

            for (int tilingNdx = 0; tilingNdx < DE_LENGTH_OF_ARRAY(tilingTypes); ++tilingNdx)
            {
                for (size_t imageFormatNdx = 0; imageFormatNdx < supportedFormats.size(); ++imageFormatNdx)
                {
                    const std::vector<CombinedTestParams> &tilingTestParamsList =
                        getTilingTestParams(ImageProcOp::IMAGE_PROC_OP_SAMPLE_WEIGHTED, tilingTypes[tilingNdx].tiling,
                                            supportedFormats[imageFormatNdx]);

                    for (size_t paramNdx = 0; paramNdx < tilingTestParamsList.size(); ++paramNdx)
                    {
                        const CombinedTestParams &params = tilingTestParamsList[paramNdx];

                        const std::string paramsName = "_params" + de::toString(paramNdx);
                        const auto testName          = tilingTypes[tilingNdx].tilingName + paramsName + "_" +
                                              getFormatShortString(supportedFormats[imageFormatNdx]);

                        tilingGroup->addChild(new ImageProcessingWeightSamplingGraphicsTest(
                            testCtx, testName, params.testParams, params.weightSamplingParams));
                    }
                }
            }

            testGroup->addChild(tilingGroup.release());
        }

        // Swizzles for sampled image
        {
            de::MovePtr<TestCaseGroup> swizzleGroup(new TestCaseGroup(testCtx, "swizzles"));

            for (int swizzleNdx = 0; swizzleNdx < DE_LENGTH_OF_ARRAY(swizzles); ++swizzleNdx)
            {
                CombinedTestParams params = getCommonTestParams(ImageProcOp::IMAGE_PROC_OP_SAMPLE_WEIGHTED);
                params.testParams.sampledImageParams.components = swizzles[swizzleNdx].components;

                const auto testName = swizzles[swizzleNdx].compMappingName;

                swizzleGroup->addChild(new ImageProcessingWeightSamplingGraphicsTest(
                    testCtx, testName, params.testParams, params.weightSamplingParams));
            }

            testGroup->addChild(swizzleGroup.release());
        }

        // Image layouts
        {
            de::MovePtr<TestCaseGroup> layoutGroup(new TestCaseGroup(testCtx, "layouts"));

            for (int layoutNdx = 0; layoutNdx < DE_LENGTH_OF_ARRAY(layouts); ++layoutNdx)
            {
                for (size_t imageFormatNdx = 0; imageFormatNdx < supportedFormats.size(); ++imageFormatNdx)
                {
                    const std::vector<CombinedTestParams> &layoutTestParamsList =
                        getLayoutTestParams(ImageProcOp::IMAGE_PROC_OP_SAMPLE_WEIGHTED, layouts[layoutNdx].layout,
                                            supportedFormats[imageFormatNdx]);

                    for (size_t paramNdx = 0; paramNdx < layoutTestParamsList.size(); ++paramNdx)
                    {
                        const CombinedTestParams &params = layoutTestParamsList[paramNdx];

                        const std::string paramsName = "_params" + de::toString(paramNdx);
                        const auto testName          = layouts[layoutNdx].layoutName + paramsName + "_" +
                                              getFormatShortString(supportedFormats[imageFormatNdx]);

                        layoutGroup->addChild(new ImageProcessingWeightSamplingGraphicsTest(
                            testCtx, testName, params.testParams, params.weightSamplingParams));
                    }
                }
            }

            testGroup->addChild(layoutGroup.release());
        }

        // Weight sampling used in other shader stages
        {
            // Testing with fixed format having all components, fixed address mode: VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE and no sampler reduction mode, optimal tiling and swizzle
            de::MovePtr<TestCaseGroup> stageGroup(new TestCaseGroup(testCtx, "shader_stages"));

            for (int stageNdx = 0; stageNdx < DE_LENGTH_OF_ARRAY(shaderStages); ++stageNdx)
            {
                CombinedTestParams params   = getCommonTestParams(ImageProcOp::IMAGE_PROC_OP_SAMPLE_WEIGHTED);
                params.testParams.stageMask = shaderStages[stageNdx];

                const auto testName = getStageNames(shaderStages[stageNdx]);

                stageGroup->addChild(new ImageProcessingWeightSamplingGraphicsTest(testCtx, testName, params.testParams,
                                                                                   params.weightSamplingParams));
            }

            testGroup->addChild(stageGroup.release());
        }

        // Descriptor tests
        {
            de::MovePtr<TestCaseGroup> descGroup(new TestCaseGroup(testCtx, "descriptors"));

            // update-after-bind
            {
                CombinedTestParams params         = getCommonTestParams(ImageProcOp::IMAGE_PROC_OP_SAMPLE_WEIGHTED);
                params.testParams.updateAfterBind = true;

                for (const auto &randomReference : {true, false})
                {
                    params.testParams.randomReference = randomReference;

                    const auto testName = std::string("updateAfterBind") + (randomReference ? "_random" : "");

                    descGroup->addChild(new ImageProcessingWeightSamplingGraphicsTest(
                        testCtx, testName, params.testParams, params.weightSamplingParams));
                }
            }

            testGroup->addChild(descGroup.release());
        }

        // Normalized coordinates
        {
            de::MovePtr<TestCaseGroup> coordsGroup(new TestCaseGroup(testCtx, "normalized_coords"));

            for (size_t imageFormatNdx = 0; imageFormatNdx < supportedFormats.size(); ++imageFormatNdx)
            {
                for (const auto isNonSeparableFilter : {true, false})
                {
                    for (const auto enableSubtexelFiltering : {true, false})
                    {
                        CombinedTestParams params = getCommonTestParams(
                            ImageProcOp::IMAGE_PROC_OP_SAMPLE_WEIGHTED, supportedFormats[imageFormatNdx],
                            VK_SHADER_STAGE_FRAGMENT_BIT, isNonSeparableFilter, enableSubtexelFiltering,
                            false /* normalized coordinates */);

                        const auto testName = getFormatShortString(supportedFormats[imageFormatNdx]) +
                                              (isNonSeparableFilter ? "" : "_separable") +
                                              (enableSubtexelFiltering ? "_subtexel" : "");
                        ;

                        coordsGroup->addChild(new ImageProcessingWeightSamplingGraphicsTest(
                            testCtx, testName, params.testParams, params.weightSamplingParams));
                    }
                }
            }

            testGroup->addChild(coordsGroup.release());
        }
    }

    return testGroup.release();
}

TestCaseGroup *createImageProcessingWeightImageSamplingGraphicsTests(
    TestContext &testCtx, const PipelineConstructionType pipelineConstructionType)
{
    return createImageProcessingWeightImageSamplingCommonTests(testCtx, false, pipelineConstructionType);
}

TestCaseGroup *createImageProcessingWeightImageSamplingComputeTests(TestContext &testCtx)
{
    return createImageProcessingWeightImageSamplingCommonTests(testCtx, true);
}

} // namespace ImageProcessing
} // namespace vkt
