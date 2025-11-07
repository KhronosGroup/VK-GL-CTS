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
 * \brief Image processing block matching tests
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

#include "tcuImageCompare.hpp"
#include "tcuRGBA.hpp"
#include "tcuVectorType.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuTestCase.hpp"

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

namespace
{

struct BlockMatchingTestParams
{
    TestImageParams targetImageParams;
    UVec2 targetCoord;
    UVec2 referenceCoord;
    UVec2 blockSize;
};

struct TestPushConstants
{
    UVec2 targetCoord;
    UVec2 referenceCoord;
    UVec2 blockSize;
};

Vec4 calculateErrorThreshold(const UVec2 blockSize, const TextureFormat format)
{
    const uint32_t numComponents = getNumUsedChannels(format.order);
    const VkFormat vkformat      = mapTextureFormat(format);

    Vec4 result;
    const float numElements = static_cast<float>(blockSize.x()) * static_cast<float>(blockSize.y());
    const double floatEps   = 9.77e-04; // FP16 eps assumed for upper bound
    const float safetyNet   = static_cast<float>(0.001f * numElements);
    const float floatErr    = static_cast<float>(floatEps * numElements) + safetyNet;

    for (uint32_t idx = 0; idx < numComponents; idx++)
    {
        const uint32_t bits         = getFormatComponentWidth(vkformat, idx);
        const float scale           = (float)(1 << bits) - 1;
        const float quantizationErr = (static_cast<float>((1.0f / scale) / 2.0f) * numElements);
        const float fullErr         = quantizationErr + floatErr;
        result[idx]                 = (bits >= 8u) ? fullErr : floatErr;
    }
    return result;
}

class ImageProcessingBlockMatchTest : public ImageProcessingTest
{
public:
    ImageProcessingBlockMatchTest(TestContext &testCtx, const std::string &name, const TestParams &testParams,
                                  const BlockMatchingTestParams &blockMatchingTestParams, const bool testMatch,
                                  const bool testConstantDifference = false);
    virtual ~ImageProcessingBlockMatchTest(void);
    virtual void checkSupport(Context &context) const;

protected:
    const BlockMatchingTestParams m_blockMatchingParams;
    const bool m_testMatch;
    const bool m_testConstantDifference;
    const ImageType m_outImageType;
    const UVec2 m_outImageSize;
    const VkFormat m_outImageFormat;
};

ImageProcessingBlockMatchTest::ImageProcessingBlockMatchTest(TestContext &testCtx, const std::string &name,
                                                             const TestParams &testParams,
                                                             const BlockMatchingTestParams &blockMatchingTestParams,
                                                             const bool testMatch, const bool testConstantDifference)
    : ImageProcessingTest(testCtx, name, testParams)
    , m_blockMatchingParams(blockMatchingTestParams)
    , m_testMatch(testMatch)
    , m_testConstantDifference(testConstantDifference)
    , m_outImageType(IMAGE_TYPE_2D)
    , m_outImageSize(4u, 4u)
    , m_outImageFormat(VK_FORMAT_R8G8B8A8_UNORM)
{
    DE_ASSERT(m_blockMatchingParams.targetImageParams.imageType == IMAGE_TYPE_2D);
}

ImageProcessingBlockMatchTest::~ImageProcessingBlockMatchTest(void)
{
}

void ImageProcessingBlockMatchTest::checkSupport(Context &context) const
{
    const auto &vki           = context.getInstanceInterface();
    const auto physicalDevice = context.getPhysicalDevice();

    ImageProcessingTest::checkSupport(context);

    {
        VkPhysicalDeviceImageProcessingPropertiesQCOM imgProcProperties;
        deMemset(&imgProcProperties, 0, sizeof(imgProcProperties));
        imgProcProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_PROCESSING_PROPERTIES_QCOM;

        VkPhysicalDeviceProperties2 properties2;
        deMemset(&properties2, 0, sizeof(properties2));
        properties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
        properties2.pNext = &imgProcProperties;

        vki.getPhysicalDeviceProperties2(context.getPhysicalDevice(), &properties2);

        if ((m_blockMatchingParams.blockSize.x() > imgProcProperties.maxBlockMatchRegion.width) ||
            (m_blockMatchingParams.blockSize.y() > imgProcProperties.maxBlockMatchRegion.height))
            TCU_THROW(NotSupportedError, "Block size is greater than supported device limits");
    }

    {
        VkFormatProperties3 formatProperties3 = initVulkanStructure();
        VkFormatProperties2 formatProperties2 = initVulkanStructure(&formatProperties3);
        vki.getPhysicalDeviceFormatProperties2(physicalDevice, m_blockMatchingParams.targetImageParams.format,
                                               &formatProperties2);

        if ((m_blockMatchingParams.targetImageParams.tiling == VK_IMAGE_TILING_OPTIMAL) &&
            (formatProperties3.optimalTilingFeatures & VK_FORMAT_FEATURE_2_BLOCK_MATCHING_BIT_QCOM) == 0)
            TCU_THROW(NotSupportedError, "Format feature block matching bit not supported for optimal tiling.");

        if ((m_blockMatchingParams.targetImageParams.tiling == VK_IMAGE_TILING_LINEAR) &&
            (formatProperties3.linearTilingFeatures & VK_FORMAT_FEATURE_2_BLOCK_MATCHING_BIT_QCOM) == 0)
            TCU_THROW(NotSupportedError, "Format feature block matching bit not supported for linear tiling.");
    }

    {
        VkImageFormatProperties refImageFormatProperties;
        const auto result = vki.getPhysicalDeviceImageFormatProperties(
            physicalDevice, m_params.sampledImageParams.format, mapImageType(m_params.sampledImageParams.imageType),
            m_params.sampledImageParams.tiling,
            (VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLE_BLOCK_MATCH_BIT_QCOM), 0,
            &refImageFormatProperties);

        if (result != VK_SUCCESS)
        {
            if (result == VK_ERROR_FORMAT_NOT_SUPPORTED)
                TCU_THROW(NotSupportedError, "Reference image format unsupported for block matching");
            else
                TCU_FAIL("vkGetPhysicalDeviceImageFormatProperties returned unexpected error");
        }
    }

    {
        VkImageFormatProperties tgtImageFormatProperties;
        const auto result = vki.getPhysicalDeviceImageFormatProperties(
            physicalDevice, m_blockMatchingParams.targetImageParams.format,
            mapImageType(m_blockMatchingParams.targetImageParams.imageType),
            m_blockMatchingParams.targetImageParams.tiling,
            (VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLE_BLOCK_MATCH_BIT_QCOM), 0,
            &tgtImageFormatProperties);

        if (result != VK_SUCCESS)
        {
            if (result == VK_ERROR_FORMAT_NOT_SUPPORTED)
                TCU_THROW(NotSupportedError, "Target image format unsupported for block matching");
            else
                TCU_FAIL("vkGetPhysicalDeviceImageFormatProperties returned unexpected error");
        }
    }
}
class ImageProcessingBlockMatchGraphicsTest : public ImageProcessingBlockMatchTest
{
public:
    ImageProcessingBlockMatchGraphicsTest(TestContext &testCtx, const std::string &name, const TestParams &testParams,
                                          const BlockMatchingTestParams &blockMatchingTestParams, const bool testMatch,
                                          const bool testConstantDifference = false);
    virtual ~ImageProcessingBlockMatchGraphicsTest(void);

    virtual void checkSupport(Context &context) const;
    virtual void initPrograms(SourceCollections &sourceCollections) const;
    virtual TestInstance *createInstance(Context &context) const;
};

ImageProcessingBlockMatchGraphicsTest::ImageProcessingBlockMatchGraphicsTest(
    TestContext &testCtx, const std::string &name, const TestParams &testParams,
    const BlockMatchingTestParams &blockMatchingTestParams, const bool testMatch, const bool testConstantDifference)
    : ImageProcessingBlockMatchTest(testCtx, name, testParams, blockMatchingTestParams, testMatch,
                                    testConstantDifference)
{
}

ImageProcessingBlockMatchGraphicsTest::~ImageProcessingBlockMatchGraphicsTest(void)
{
}

void ImageProcessingBlockMatchGraphicsTest::checkSupport(Context &context) const
{
    const auto &vki           = context.getInstanceInterface();
    const auto physicalDevice = context.getPhysicalDevice();

    ImageProcessingBlockMatchTest::checkSupport(context);

    {
        VkFormatProperties3 formatProperties3 = initVulkanStructure();
        VkFormatProperties2 formatProperties2 = initVulkanStructure(&formatProperties3);
        vki.getPhysicalDeviceFormatProperties2(physicalDevice, m_outImageFormat, &formatProperties2);
        const auto &tilingFeatures = formatProperties3.optimalTilingFeatures;

        if ((m_outImageType == IMAGE_TYPE_2D) && !(tilingFeatures & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT))
            TCU_THROW(NotSupportedError, "Format not supported for color attachment");
    }

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

const std::string getProgPreMain()
{
    std::string prog = "#extension GL_QCOM_image_processing : require\n"
                       "\n"
                       "layout(set = 0, binding = 0) uniform highp texture2D targetTexture;\n"
                       "layout(set = 0, binding = 1) uniform highp texture2D referenceTexture;\n"
                       "layout(set = 0, binding = 2) uniform highp sampler targetSampler;\n"
                       "layout(set = 0, binding = 3) uniform highp sampler referenceSampler;\n"
                       "layout(set = 0, binding = 4) writeonly buffer outputError {\n"
                       "  vec4 outError;\n"
                       "} sbOut;\n"
                       "layout(push_constant, std430) uniform PushConstants\n"
                       "{\n"
                       "    uvec2 targetCoord;\n"
                       "    uvec2 referenceCoord;\n"
                       "    uvec2 blockSize;\n"
                       "} pc;\n";
    return prog;
}

const std::string getProgMainBlock(const ImageProcOp op)
{
    std::string prog = "    // Compute\n"
                       "    vec4 blkMatchVal = " +
                       getImageProcGLSLStr(op) +
                       "(\n"
                       "        sampler2D(targetTexture, targetSampler),\n"
                       "        pc.targetCoord,  \n"
                       "        sampler2D(referenceTexture, referenceSampler),\n"
                       "        pc.referenceCoord,\n"
                       "        pc.blockSize\n"
                       "    );\n"
                       "\n"
                       "    vec4 err = blkMatchVal;"
                       "\n"
                       "    if (err == vec4(0.0f, 0.0f, 0.0f, 0.0f))\n"
                       "        outColor = vec4(0.0f, 1.0f, 0.0f, 1.0f);\n" // green on match
                       "    else\n"
                       "        outColor = vec4(1.0f, 0.0f, 0.0f, 1.0f);\n" // red on mismatch
                       "    sbOut.outError = err;\n";
    return prog;
}

void ImageProcessingBlockMatchGraphicsTest::initPrograms(SourceCollections &sourceCollections) const
{
    const vk::ShaderBuildOptions shaderBuildOpt(sourceCollections.usedVulkanVersion, vk::SPIRV_VERSION_1_4, 0u, true);

    std::ostringstream vert;
    {
        vert << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n";
        if (m_params.stageMask & VK_SHADER_STAGE_VERTEX_BIT)
        {
            vert << "layout(location = 0) in vec2 inPosition;\n"
                 << getProgPreMain() << "\n"
                 << "layout(location = 0) out vec4 outColor;\n"
                 << "\n"
                 << "void main() {\n"
                 << getProgMainBlock(m_params.imageProcOp) << "    gl_Position = vec4(inPosition, 0.0, 1.0);\n"
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
                 << getProgPreMain() << "\n"
                 << "layout(location = 0) out vec4 outColor;\n"
                 << "\n"
                 << "void main() {\n"
                 << getProgMainBlock(m_params.imageProcOp) << "}\n";
        }
        else
        {
            frag << "layout(location = 0) in vec4 inColor;\n"
                 << "layout(location = 0) out vec4 fragColor;\n"
                 << "\n"
                 << "void main() {\n"
                 << "    fragColor = inColor;"
                 << "}\n";
        }
    }
    sourceCollections.glslSources.add("frag") << glu::FragmentSource(frag.str()) << shaderBuildOpt;
}

class ImageProcessingBlockMatchTestInstance : public ImageProcessingTestInstance
{
public:
    ImageProcessingBlockMatchTestInstance(Context &context, const TestParams &testParams,
                                          const BlockMatchingTestParams &blockMatchingTestParams, const bool testMatch,
                                          const bool testConstantDifference, const ImageType outImageType,
                                          const UVec2 outImageSize, const VkFormat outImageFormat);
    ~ImageProcessingBlockMatchTestInstance(void);

    virtual void addSupplementaryDescBindings(DescriptorSetLayoutExtBuilder &layoutBuilder);
    virtual void addSupplementaryDescTypes(DescriptorPoolBuilder &poolBuilder);
    virtual void writeSupplementaryDescriptors();
    void prepareDescriptors(const bool useTargetAsReference = false);
    void populateColorBuffer(const BufferWithMemory &colorBuffer, const UVec2 &imageSize, const VkFormat format,
                             const UVec2 &coordinates, const bool fillEmpty = true, const bool useSrcColor = false,
                             const BufferWithMemory *srcColorBuffer = nullptr, const UVec2 srcImageSize = UVec2(0u, 0u),
                             const UVec2 srcRegion = UVec2(0u, 0u));

    void prepareCommandBuffer();
    virtual void executeBarriers();
    virtual void executeBegin();
    virtual void executeBindPipeline();
    virtual void executeBindOtherBindings();
    virtual void executeProgram();
    virtual void executeEnd();
    void executeCommands(const VkPipelineLayout pipelineLayout, const BufferWithMemory &tgtColorBuffer,
                         const ImageWithMemory &tgtImage, const BufferWithMemory &refColorBuffer,
                         const ImageWithMemory &refImage, const BufferWithMemory &resultBuffer,
                         const ImageWithMemory &resultImage, const bool isSelfTest = false);

    const Vec4 buildStandardResult(ImageProcessingResult &expectedResult, const BufferWithMemory &tgtColorBuffer,
                                   const BufferWithMemory &refColorBuffer);

protected:
    const BlockMatchingTestParams m_blockMatchingParams;
    const bool m_testMatch;
    const bool m_testConstantDifference;
    const float m_constantDifference;

    const ImageType m_outImageType;
    const UVec2 m_outImageSize;
    const VkFormat m_outImageFormat;

    Move<VkDescriptorSetLayout> m_descriptorSetLayout;
    Move<VkDescriptorPool> m_descriptorPool;
    Move<VkDescriptorSet> m_descriptorSet;
    DescriptorSetUpdateBuilder m_descriptorUpdateBuilder;
    Move<VkCommandPool> m_cmdPool;
    Move<VkCommandBuffer> m_cmdBuffer;
    Move<VkSampler> m_targetSampler;
    Move<VkSampler> m_referenceSampler;
    Move<VkImageView> m_targetView;
    Move<VkImageView> m_referenceView;
    de::MovePtr<BufferWithMemory> m_errBuffer;
};

ImageProcessingBlockMatchTestInstance::ImageProcessingBlockMatchTestInstance(
    Context &context, const TestParams &testParams, const BlockMatchingTestParams &blockMatchingTestParams,
    const bool testMatch, const bool testConstantDifference, const ImageType outImageType, const UVec2 outImageSize,
    const VkFormat outImageFormat)
    : ImageProcessingTestInstance(context, testParams)
    , m_blockMatchingParams(blockMatchingTestParams)
    , m_testMatch(testMatch)
    , m_testConstantDifference(testConstantDifference)
    , m_constantDifference(m_testConstantDifference ? 0.5f : 0.0f)
    , m_outImageType(outImageType)
    , m_outImageSize(outImageSize)
    , m_outImageFormat(outImageFormat)
{
}

ImageProcessingBlockMatchTestInstance::~ImageProcessingBlockMatchTestInstance(void)
{
}

void ImageProcessingBlockMatchTestInstance::addSupplementaryDescBindings(DescriptorSetLayoutExtBuilder &layoutBuilder)
{
    DE_UNREF(layoutBuilder);
}

void ImageProcessingBlockMatchTestInstance::addSupplementaryDescTypes(DescriptorPoolBuilder &poolBuilder)
{
    DE_UNREF(poolBuilder);
}

void ImageProcessingBlockMatchTestInstance::writeSupplementaryDescriptors()
{
}

void ImageProcessingBlockMatchTestInstance::prepareDescriptors(const bool useTargetAsReference)
{
    const auto &vkd   = m_context.getDeviceInterface();
    const auto device = m_context.getDevice();

    VkDescriptorPoolCreateFlags descPoolCreateFlags           = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    VkDescriptorSetLayoutCreateFlags descSetLayoutCreateFlags = 0u;
    VkDescriptorBindingFlags descBindingFlag                  = 0u;

    const auto descType = VK_DESCRIPTOR_TYPE_BLOCK_MATCH_IMAGE_QCOM;

    if (m_params.updateAfterBind)
    {
        descPoolCreateFlags |= VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
        descSetLayoutCreateFlags |= VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
        descBindingFlag |= VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;
    }

    // Descriptor set layout
    DescriptorSetLayoutExtBuilder layoutBuilder;
    layoutBuilder.addSingleBinding(descType, m_params.stageMask);
    layoutBuilder.addSingleBinding(descType, m_params.stageMask);
    layoutBuilder.addSingleSamplerBinding(VK_DESCRIPTOR_TYPE_SAMPLER, m_params.stageMask, &m_targetSampler.get());
    layoutBuilder.addSingleSamplerBinding(VK_DESCRIPTOR_TYPE_SAMPLER, m_params.stageMask, &m_referenceSampler.get());
    layoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, m_params.stageMask);
    addSupplementaryDescBindings(layoutBuilder);
    m_descriptorSetLayout = layoutBuilder.buildExt(vkd, device, descSetLayoutCreateFlags, descBindingFlag);

    // Descriptor pool
    DescriptorPoolBuilder poolBuilder;
    poolBuilder.addType(descType);
    poolBuilder.addType(descType);
    poolBuilder.addType(VK_DESCRIPTOR_TYPE_SAMPLER);
    poolBuilder.addType(VK_DESCRIPTOR_TYPE_SAMPLER);
    poolBuilder.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
    addSupplementaryDescTypes(poolBuilder);
    m_descriptorPool = poolBuilder.build(vkd, device, descPoolCreateFlags, 1u);

    // Descriptor set
    m_descriptorSet = makeDescriptorSet(vkd, device, m_descriptorPool.get(), m_descriptorSetLayout.get());

    // Register descriptors in the update builder
    const auto tgtDescImageInfo =
        makeDescriptorImageInfo(VK_NULL_HANDLE, m_targetView.get(), m_blockMatchingParams.targetImageParams.layout);
    m_descriptorUpdateBuilder.writeSingle(m_descriptorSet.get(), DescriptorSetUpdateBuilder::Location::binding(0u),
                                          descType, &tgtDescImageInfo);
    const auto refDescImageInfo = makeDescriptorImageInfo(
        VK_NULL_HANDLE, useTargetAsReference ? m_targetView.get() : m_referenceView.get(),
        useTargetAsReference ? m_blockMatchingParams.targetImageParams.layout : m_params.sampledImageParams.layout);
    m_descriptorUpdateBuilder.writeSingle(m_descriptorSet.get(), DescriptorSetUpdateBuilder::Location::binding(1u),
                                          descType, &refDescImageInfo);

    const VkDeviceSize errBuffSizeBytes            = sizeof(Vec4);
    const VkDescriptorBufferInfo errDescriptorInfo = makeDescriptorBufferInfo(**m_errBuffer, 0ull, errBuffSizeBytes);
    m_descriptorUpdateBuilder.writeSingle(m_descriptorSet.get(), DescriptorSetUpdateBuilder::Location::binding(4u),
                                          VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &errDescriptorInfo);
    writeSupplementaryDescriptors();

    // Update descriptor set with the descriptor
    if (!m_params.updateAfterBind)
        m_descriptorUpdateBuilder.update(vkd, device);
}

void ImageProcessingBlockMatchTestInstance::populateColorBuffer(const BufferWithMemory &colorBuffer,
                                                                const UVec2 &imageSize, const VkFormat format,
                                                                const UVec2 &coordinates, const bool fillEmpty,
                                                                const bool useSrcColor,
                                                                const BufferWithMemory *srcColorBuffer,
                                                                const UVec2 srcImageSize, const UVec2 srcRegion)
{
    const auto &vkd   = m_context.getDeviceInterface();
    const auto device = m_context.getDevice();

    const IVec2 fillSize{static_cast<int>(imageSize.x()), static_cast<int>(imageSize.y())};
    auto &colorBufferAlloc = colorBuffer.getAllocation();
    auto colorBufferPtr    = reinterpret_cast<char *>(colorBufferAlloc.getHostPtr()) + colorBufferAlloc.getOffset();

    const TextureFormat tcuFormat = mapVkFormat(format);
    const PixelBufferAccess colorBufferPixels{tcuFormat, fillSize[0], fillSize[1], 1, colorBufferPtr};

    const int W = colorBufferPixels.getWidth();
    const int H = colorBufferPixels.getHeight();
    const int D = colorBufferPixels.getDepth();

    const IVec2 srcSize{static_cast<int>(srcImageSize.x()), static_cast<int>(srcImageSize.y())};
    auto &srcColorBufferAlloc = useSrcColor ? srcColorBuffer->getAllocation() : colorBuffer.getAllocation();
    auto srcColorBufferPtr =
        reinterpret_cast<char *>(srcColorBufferAlloc.getHostPtr()) + srcColorBufferAlloc.getOffset();
    const PixelBufferAccess srcColorBufferPixels{mapVkFormat(m_params.sampledImageParams.format), srcSize[0],
                                                 srcSize[1], 1, srcColorBufferPtr};

    const float minChannelValue = m_constantDifference;
    const float channelValue    = m_rnd.getFloat(minChannelValue, 1.0f) - minChannelValue;
    const Vec4 uniformColor     = Vec4(channelValue, channelValue, channelValue, channelValue);

    const int coordX = static_cast<int>(coordinates.x());
    const int coordY = static_cast<int>(coordinates.y());

    const int blockW = static_cast<int>(m_blockMatchingParams.blockSize.x());
    const int blockH = static_cast<int>(m_blockMatchingParams.blockSize.y());

    const int numComponents = getNumUsedChannels(tcuFormat.order);

    for (int x = 0; x < W; ++x)
        for (int y = 0; y < H; ++y)
            for (int z = 0; z < D; ++z)
            {
                if (de::inBounds(x, coordX, coordX + blockW) && de::inBounds(y, coordY, coordY + blockH))
                {
                    float colorR = m_rnd.getFloat(minChannelValue, 1.0f) - minChannelValue;
                    float colorG = m_rnd.getFloat(minChannelValue, 1.0f) - minChannelValue;
                    float colorB = m_rnd.getFloat(minChannelValue, 1.0f) - minChannelValue;
                    float colorA = m_rnd.getFloat(minChannelValue, 1.0f) - minChannelValue;
                    const Vec4 randomColor(colorR, colorG, colorB, colorA);

                    Vec4 color = (m_params.randomReference ? randomColor : uniformColor);

                    for (int32_t compIdx = 0; compIdx < numComponents; compIdx++)
                    {
                        uint32_t compWidth = getFormatComponentWidth(format, compIdx);
                        if (compWidth < 8u)
                            color[compIdx] = 1.0f;
                    }

                    if (useSrcColor)
                    {
                        const int offsetX = x - coordX;
                        const int offsetY = y - coordY;
                        const Vec4 srcColor =
                            srcColorBufferPixels.getPixel(srcRegion.x() + offsetX, srcRegion.y() + offsetY, z);
                        color = srcColor + Vec4(m_constantDifference);
                    }

                    colorBufferPixels.setPixel(color, x, y, z);
                }
                else
                {
                    if (fillEmpty)
                        colorBufferPixels.setPixel(tcu::RGBA::gray().toVec(), x, y, z);
                }
            }

    flushAlloc(vkd, device, colorBufferAlloc);
    if (useSrcColor)
        flushAlloc(vkd, device, srcColorBufferAlloc);
}

void ImageProcessingBlockMatchTestInstance::executeBarriers()
{
}

void ImageProcessingBlockMatchTestInstance::executeBegin()
{
}

void ImageProcessingBlockMatchTestInstance::executeBindPipeline()
{
}

void ImageProcessingBlockMatchTestInstance::executeBindOtherBindings()
{
}

void ImageProcessingBlockMatchTestInstance::executeProgram()
{
}

void ImageProcessingBlockMatchTestInstance::executeEnd()
{
}

void ImageProcessingBlockMatchTestInstance::prepareCommandBuffer()
{
    const auto &vkd       = m_context.getDeviceInterface();
    const auto device     = m_context.getDevice();
    const auto queueIndex = m_context.getUniversalQueueFamilyIndex();

    // Command pool and command buffer
    m_cmdPool   = createCommandPool(vkd, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueIndex);
    m_cmdBuffer = allocateCommandBuffer(vkd, device, m_cmdPool.get(), VK_COMMAND_BUFFER_LEVEL_PRIMARY);
}

void ImageProcessingBlockMatchTestInstance::executeCommands(
    const VkPipelineLayout pipelineLayout, const BufferWithMemory &tgtColorBuffer, const ImageWithMemory &tgtImage,
    const BufferWithMemory &refColorBuffer, const ImageWithMemory &refImage, const BufferWithMemory &resultBuffer,
    const ImageWithMemory &resultImage, const bool isSelfTest)
{
    const auto &vkd          = m_context.getDeviceInterface();
    const auto device        = m_context.getDevice();
    const auto queue         = m_context.getUniversalQueue();
    const bool isComputeTest = ((m_params.stageMask & VK_SHADER_STAGE_COMPUTE_BIT) == VK_SHADER_STAGE_COMPUTE_BIT);
    const auto cmdBuffer     = m_cmdBuffer.get();

    const VkImageSubresourceLayers layerSubresource = makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u);

    beginCommandBuffer(vkd, cmdBuffer);

    executeBarriers();

    // Copy target color buffer to target image
    {
        const auto tgtTcuFormat               = mapVkFormat(m_blockMatchingParams.targetImageParams.format);
        const VkDeviceSize tgtColorBufferSize = static_cast<VkDeviceSize>(
            static_cast<uint32_t>(getPixelSize(tgtTcuFormat)) * m_blockMatchingParams.targetImageParams.imageSize.x() *
            m_blockMatchingParams.targetImageParams.imageSize.y() * 1u);
        const VkExtent3D tgtExtent = makeExtent3D(m_blockMatchingParams.targetImageParams.imageSize.x(),
                                                  m_blockMatchingParams.targetImageParams.imageSize.y(), 1u);
        const std::vector<VkBufferImageCopy> bufferImageCopy(1, makeBufferImageCopy(tgtExtent, layerSubresource));
        copyBufferToImage(vkd, cmdBuffer, tgtColorBuffer.get(), tgtColorBufferSize, bufferImageCopy,
                          VK_IMAGE_ASPECT_COLOR_BIT, 1u, 1u, tgtImage.get(),
                          m_blockMatchingParams.targetImageParams.layout);
    }

    // Copy reference color buffer to reference image
    if (!isSelfTest)
    {
        const auto refTcuFormat               = mapVkFormat(m_params.sampledImageParams.format);
        const VkDeviceSize refColorBufferSize = static_cast<VkDeviceSize>(
            static_cast<uint32_t>(getPixelSize(refTcuFormat)) * m_params.sampledImageParams.imageSize.x() *
            m_params.sampledImageParams.imageSize.y() * 1u);
        const VkExtent3D refExtent =
            makeExtent3D(m_params.sampledImageParams.imageSize.x(), m_params.sampledImageParams.imageSize.y(), 1u);
        const std::vector<VkBufferImageCopy> bufferImageCopy(1, makeBufferImageCopy(refExtent, layerSubresource));
        copyBufferToImage(vkd, cmdBuffer, refColorBuffer.get(), refColorBufferSize, bufferImageCopy,
                          VK_IMAGE_ASPECT_COLOR_BIT, 1u, 1u, refImage.get(), m_params.sampledImageParams.layout);
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
            const TestPushConstants pushConstants = {m_blockMatchingParams.targetCoord,
                                                     m_blockMatchingParams.referenceCoord,
                                                     m_blockMatchingParams.blockSize};
            vkd.cmdPushConstants(cmdBuffer, pipelineLayout, m_params.stageMask, 0u,
                                 static_cast<uint32_t>(sizeof(pushConstants)), &pushConstants);
        }

        executeProgram();
    }
    executeEnd();

    {
        const VkDeviceSize errBuffSizeBytes         = sizeof(Vec4);
        const VkBufferMemoryBarrier errWriteBarrier = makeBufferMemoryBarrier(
            VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT, **m_errBuffer, 0ull, errBuffSizeBytes);

        const VkPipelineStageFlags srcPipelineStageFlags =
            isComputeTest ? VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT :
                            ((m_params.stageMask & VK_SHADER_STAGE_VERTEX_BIT) ? VK_PIPELINE_STAGE_VERTEX_SHADER_BIT :
                                                                                 VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

        vkd.cmdPipelineBarrier(cmdBuffer, srcPipelineStageFlags, VK_PIPELINE_STAGE_HOST_BIT, (VkDependencyFlags)0, 0,
                               nullptr, 1, &errWriteBarrier, 0, nullptr);
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

const Vec4 ImageProcessingBlockMatchTestInstance::buildStandardResult(ImageProcessingResult &expectedResult,
                                                                      const BufferWithMemory &tgtColorBuffer,
                                                                      const BufferWithMemory &refColorBuffer)
{
    const auto &vkd   = m_context.getDeviceInterface();
    const auto device = m_context.getDevice();
    const bool isSSD  = (m_params.imageProcOp == ImageProcOp::IMAGE_PROC_OP_BLOCK_MATCH_SSD);

    const IVec2 tgtRegionSize{static_cast<int>(m_blockMatchingParams.targetImageParams.imageSize.x()),
                              static_cast<int>(m_blockMatchingParams.targetImageParams.imageSize.y())};
    auto &tgtColorBufferAlloc = tgtColorBuffer.getAllocation();
    auto tgtColorBufferPtr =
        reinterpret_cast<char *>(tgtColorBufferAlloc.getHostPtr()) + tgtColorBufferAlloc.getOffset();
    const PixelBufferAccess tgtColorBufferPix{mapVkFormat(m_blockMatchingParams.targetImageParams.format),
                                              tgtRegionSize[0], tgtRegionSize[1], 1, tgtColorBufferPtr};

    const IVec2 refRegionSize{static_cast<int>(m_params.sampledImageParams.imageSize.x()),
                              static_cast<int>(m_params.sampledImageParams.imageSize.y())};
    auto &refColorBufferAlloc = refColorBuffer.getAllocation();
    auto refColorBufferPtr =
        reinterpret_cast<char *>(refColorBufferAlloc.getHostPtr()) + refColorBufferAlloc.getOffset();
    const PixelBufferAccess refColorBufferPix{mapVkFormat(m_params.sampledImageParams.format), refRegionSize[0],
                                              refRegionSize[1], 1, refColorBufferPtr};

    const auto blockMatchingError = expectedResult.getBlockMatchingResult(
        isSSD, tgtColorBufferPix, m_blockMatchingParams.targetCoord, refColorBufferPix,
        m_blockMatchingParams.referenceCoord, m_blockMatchingParams.blockSize,
        m_blockMatchingParams.targetImageParams.components);

    flushAlloc(vkd, device, tgtColorBufferAlloc);
    flushAlloc(vkd, device, refColorBufferAlloc);

    return blockMatchingError;
}

class ImageProcessingBlockMatchGraphicsTestInstance : public ImageProcessingBlockMatchTestInstance
{
public:
    ImageProcessingBlockMatchGraphicsTestInstance(Context &context, const TestParams &params,
                                                  const BlockMatchingTestParams &blockMatchingTestParams,
                                                  const bool testMatch, const bool testConstantDifference,
                                                  const ImageType outImageType, const UVec2 outImageSize,
                                                  const VkFormat outImageFormat);
    ~ImageProcessingBlockMatchGraphicsTestInstance(void);

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

ImageProcessingBlockMatchGraphicsTestInstance::ImageProcessingBlockMatchGraphicsTestInstance(
    Context &context, const TestParams &testParams, const BlockMatchingTestParams &blockMatchingTestParams,
    const bool testMatch, const bool testConstantDifference, const ImageType outImageType, const UVec2 outImageSize,
    const VkFormat outImageFormat)
    : ImageProcessingBlockMatchTestInstance(context, testParams, blockMatchingTestParams, testMatch,
                                            testConstantDifference, outImageType, outImageSize, outImageFormat)
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

ImageProcessingBlockMatchGraphicsTestInstance::~ImageProcessingBlockMatchGraphicsTestInstance(void)
{
}

void ImageProcessingBlockMatchGraphicsTestInstance::makeRenderPass()
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

void ImageProcessingBlockMatchGraphicsTestInstance::makeGraphicsPipeline(const PipelineLayoutWrapper &pipelineLayout,
                                                                         const VkExtent3D extent,
                                                                         const ShaderWrapper &vertexModule,
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

void ImageProcessingBlockMatchGraphicsTestInstance::executeBarriers()
{
    const auto &vkd                = m_context.getDeviceInterface();
    const auto vertexBufferBarrier = makeBufferMemoryBarrier(
        VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT, m_vertexBuffer->get(), 0ull, m_vertexBufferSize);

    vkd.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, 0u, 0u,
                           nullptr, 1u, &vertexBufferBarrier, 0u, nullptr);
}

void ImageProcessingBlockMatchGraphicsTestInstance::executeBegin()
{
    const auto &vkd = m_context.getDeviceInterface();
    const Vec4 clearColor(tcu::RGBA::black().toVec());
    const VkExtent3D extent   = makeExtent3D(m_outImageSize.x(), m_outImageSize.y(), 1u);
    const VkRect2D renderArea = makeRect2D(extent.width, extent.height);

    m_renderPass.begin(vkd, *m_cmdBuffer, renderArea, clearColor);
}

void ImageProcessingBlockMatchGraphicsTestInstance::executeBindPipeline()
{
    m_graphicsPipeline.bind(*m_cmdBuffer);
}

void ImageProcessingBlockMatchGraphicsTestInstance::executeBindOtherBindings()
{
    const auto &vkd                       = m_context.getDeviceInterface();
    const VkDeviceSize vertexBufferOffset = 0ull;

    vkd.cmdBindVertexBuffers(*m_cmdBuffer, 0u, 1u, &m_vertexBuffer->get(), &vertexBufferOffset);
}

void ImageProcessingBlockMatchGraphicsTestInstance::executeProgram()
{
    const auto &vkd = m_context.getDeviceInterface();

    vkd.cmdDraw(*m_cmdBuffer, de::sizeU32(m_vertexData), 1u, 0u, 0u);
}

void ImageProcessingBlockMatchGraphicsTestInstance::executeEnd()
{
    const auto &vkd = m_context.getDeviceInterface();

    m_renderPass.end(vkd, *m_cmdBuffer);
}

TestStatus ImageProcessingBlockMatchGraphicsTestInstance::iterate(void)
{
    const auto &vkd   = m_context.getDeviceInterface();
    const auto device = m_context.getDevice();
    auto &allocator   = m_context.getDefaultAllocator();

    const bool unnorm   = ((m_params.imageProcOp == ImageProcOp::IMAGE_PROC_OP_BLOCK_MATCH_SAD) ||
                         (m_params.imageProcOp == ImageProcOp::IMAGE_PROC_OP_BLOCK_MATCH_SSD));
    const auto texUsage = (VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLE_BLOCK_MATCH_BIT_QCOM);

    // Image types are actually same for both target and reference images - 2D
    const auto tgtImageViewType = mapImageViewType(m_blockMatchingParams.targetImageParams.imageType);
    const auto refImageViewType = mapImageViewType(m_params.sampledImageParams.imageType);
    const auto outImageUsage    = (VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
    const auto tgtTcuFormat     = mapVkFormat(m_blockMatchingParams.targetImageParams.format);
    const auto refTcuFormat     = mapVkFormat(m_params.sampledImageParams.format);

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

    // Create reference and target images
    const VkImageCreateInfo tgtImageCreateInfo = makeImageCreateInfo(
        m_blockMatchingParams.targetImageParams.imageType, m_blockMatchingParams.targetImageParams.imageSize,
        m_blockMatchingParams.targetImageParams.format, texUsage, 0u, m_blockMatchingParams.targetImageParams.tiling);
    const VkImageCreateInfo refImageCreateInfo =
        makeImageCreateInfo(m_params.sampledImageParams.imageType, m_params.sampledImageParams.imageSize,
                            m_params.sampledImageParams.format, texUsage, 0u, m_params.sampledImageParams.tiling);

    const ImageWithMemory tgtImage{vkd, device, allocator, tgtImageCreateInfo, MemoryRequirement::Any};
    const ImageWithMemory refImage{vkd, device, allocator, refImageCreateInfo, MemoryRequirement::Any};

    // Corresponding image views
    const auto colorSubresourceRange = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);

    m_targetView = makeImageViewUtil(vkd, device, tgtImage.get(), tgtImageViewType,
                                     m_blockMatchingParams.targetImageParams.format, colorSubresourceRange);
    m_referenceView =
        makeImageViewUtil(vkd, device, refImage.get(), refImageViewType, m_params.sampledImageParams.format,
                          colorSubresourceRange, m_params.sampledImageParams.components);

    // Create textures
    const VkDeviceSize tgtColorBufferSize = static_cast<VkDeviceSize>(
        static_cast<uint32_t>(getPixelSize(tgtTcuFormat)) * m_blockMatchingParams.targetImageParams.imageSize.x() *
        m_blockMatchingParams.targetImageParams.imageSize.y() * 1u);
    const auto tgtBufferInfo              = makeBufferCreateInfo(tgtColorBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    const VkDeviceSize refColorBufferSize = static_cast<VkDeviceSize>(
        static_cast<uint32_t>(getPixelSize(refTcuFormat)) * m_params.sampledImageParams.imageSize.x() *
        m_params.sampledImageParams.imageSize.y() * 1u);
    const auto refBufferInfo = makeBufferCreateInfo(refColorBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);

    const BufferWithMemory tgtColorBuffer{vkd, device, allocator, tgtBufferInfo, MemoryRequirement::HostVisible};
    const BufferWithMemory refColorBuffer{vkd, device, allocator, refBufferInfo, MemoryRequirement::HostVisible};

    // Fill reference color buffer
    populateColorBuffer(refColorBuffer, m_params.sampledImageParams.imageSize, m_params.sampledImageParams.format,
                        m_blockMatchingParams.referenceCoord);

    // Fill target color buffer
    if (m_testMatch)
        populateColorBuffer(tgtColorBuffer, m_blockMatchingParams.targetImageParams.imageSize,
                            m_blockMatchingParams.targetImageParams.format, m_blockMatchingParams.targetCoord,
                            true /* fillEmpty */, true /* useSrcColor */, &refColorBuffer,
                            m_params.sampledImageParams.imageSize, m_blockMatchingParams.referenceCoord);
    else
        populateColorBuffer(tgtColorBuffer, m_blockMatchingParams.targetImageParams.imageSize,
                            m_blockMatchingParams.targetImageParams.format, m_blockMatchingParams.targetCoord);

    // Prepare inputs and outputs
    const VkDeviceSize errBuffSizeBytes = sizeof(Vec4);
    m_errBuffer                         = de::MovePtr<BufferWithMemory>(new BufferWithMemory(
        vkd, device, allocator, makeBufferCreateInfo(errBuffSizeBytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT),
        MemoryRequirement::HostVisible));
    m_targetSampler                     = makeSampler(unnorm, m_blockMatchingParams.targetImageParams.addrMode,
                                                      m_blockMatchingParams.targetImageParams.reductionMode);
    m_referenceSampler =
        makeSampler(unnorm, m_params.sampledImageParams.addrMode, m_params.sampledImageParams.reductionMode);

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

    // Result = red on mismatch, green on match
    const VkDeviceSize resultBufferSize =
        static_cast<VkDeviceSize>(static_cast<uint32_t>(getPixelSize(mapVkFormat(m_outImageFormat))) * extent.width *
                                  extent.height * extent.depth);
    const IVec2 resultSize{static_cast<int>(extent.width), static_cast<int>(extent.height)};
    const auto resultBufferInfo = makeBufferCreateInfo(resultBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    const BufferWithMemory resultBuffer{vkd, device, allocator, resultBufferInfo, MemoryRequirement::HostVisible};

    // Command execution
    executeCommands(pipelineLayout.get(), tgtColorBuffer, tgtImage, refColorBuffer, refImage, resultBuffer, colorImage);

    // Get results
    const auto &resultBufferAlloc = resultBuffer.getAllocation();
    invalidateAlloc(vkd, device, resultBufferAlloc);
    const auto &errBufferAllocation = (*m_errBuffer).getAllocation();
    invalidateAlloc(vkd, device, errBufferAllocation);

    const auto resultsBufferPtr =
        reinterpret_cast<const char *>(resultBufferAlloc.getHostPtr()) + resultBufferAlloc.getOffset();
    const ConstPixelBufferAccess resultPixels{mapVkFormat(m_outImageFormat), resultSize[0], resultSize[1], 1,
                                              resultsBufferPtr};

    const Vec4 *errBufferPtr = static_cast<Vec4 *>(errBufferAllocation.getHostPtr());

    // Get reference result
    // Use address mode of target and reduction mode of reference
    const VkSamplerReductionMode redMode = getVkSamplerReductionMode(m_params.sampledImageParams.reductionMode);
    ImageProcessingResult expectedResult(mapVkFormat(m_outImageFormat), m_outImageSize.x(), m_outImageSize.y(),
                                         m_blockMatchingParams.targetImageParams.addrMode, redMode);
    const auto expectedBlockMatchingError = buildStandardResult(expectedResult, tgtColorBuffer, refColorBuffer);
    const Vec4 resultError                = *errBufferPtr;

    // Assumption: reference and target formats are same
    const Vec4 errorThreshold = calculateErrorThreshold(m_blockMatchingParams.blockSize, tgtTcuFormat);
    return verifyResult(expectedBlockMatchingError, resultError, expectedResult.getAccess(), resultPixels,
                        errorThreshold);
}

class ImageProcessingBlockMatchComputeTest : public ImageProcessingBlockMatchTest
{
public:
    ImageProcessingBlockMatchComputeTest(TestContext &testCtx, const std::string &name, const TestParams &testParams,
                                         const BlockMatchingTestParams &blockMatchingTestParams, const bool testMatch,
                                         const bool testConstantDifference = false);
    virtual ~ImageProcessingBlockMatchComputeTest(void);

    virtual void checkSupport(Context &context) const;
    virtual void initPrograms(SourceCollections &sourceCollections) const;
    virtual TestInstance *createInstance(Context &context) const;
};

ImageProcessingBlockMatchComputeTest::ImageProcessingBlockMatchComputeTest(
    TestContext &testCtx, const std::string &name, const TestParams &testParams,
    const BlockMatchingTestParams &blockMatchingTestParams, const bool testMatch, const bool testConstantDifference)
    : ImageProcessingBlockMatchTest(testCtx, name, testParams, blockMatchingTestParams, testMatch,
                                    testConstantDifference)
{
}

ImageProcessingBlockMatchComputeTest::~ImageProcessingBlockMatchComputeTest(void)
{
}

void ImageProcessingBlockMatchComputeTest::checkSupport(Context &context) const
{
    const auto &vki           = context.getInstanceInterface();
    const auto physicalDevice = context.getPhysicalDevice();

    ImageProcessingBlockMatchTest::checkSupport(context);

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
        TCU_THROW(NotSupportedError, "Compute workgroup count not supported");
}

void ImageProcessingBlockMatchComputeTest::initPrograms(SourceCollections &sourceCollections) const
{
    const vk::ShaderBuildOptions shaderBuildOpt(sourceCollections.usedVulkanVersion, vk::SPIRV_VERSION_1_4, 0u, true);

    const std::string imageTypeStr =
        getFormatPrefix(mapVkFormat(m_outImageFormat)) + "image" + "2D"; // only 2D image support by block matching

    std::ostringstream comp;
    {
        comp << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
             << "layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;\n"
             << getProgPreMain() << "\n"
             << "layout(set = 0, binding = 5) uniform writeonly " << imageTypeStr << " outputImage;\n"
             << "\n"
             << "void main() {\n"
             << "\n"
             << "    int gx = int(gl_GlobalInvocationID.x);\n"
             << "    int gy = int(gl_GlobalInvocationID.y);\n"
             << "    vec4 outColor = vec4(1.0f, 0.0f, 0.0f, 1.0f);" // red on mismatch
             << "\n"
             << getProgMainBlock(m_params.imageProcOp) << "    imageStore(outputImage, ivec2(gx, gy), outColor);\n"
             << "}\n";
    }
    sourceCollections.glslSources.add("comp") << glu::ComputeSource(comp.str()) << shaderBuildOpt;
}

class ImageProcessingBlockMatchComputeTestInstance : public ImageProcessingBlockMatchTestInstance
{
public:
    ImageProcessingBlockMatchComputeTestInstance(Context &context, const TestParams &params,
                                                 const BlockMatchingTestParams &blockMatchingTestParams,
                                                 const bool testMatch, const bool testConstantDifference,
                                                 const ImageType outImageType, const UVec2 outImageSize,
                                                 const VkFormat outImageFormat);
    ~ImageProcessingBlockMatchComputeTestInstance(void);

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

ImageProcessingBlockMatchComputeTestInstance::ImageProcessingBlockMatchComputeTestInstance(
    Context &context, const TestParams &testParams, const BlockMatchingTestParams &blockMatchingTestParams,
    const bool testMatch, const bool testConstantDifference, const ImageType outImageType, const UVec2 outImageSize,
    const VkFormat outImageFormat)
    : ImageProcessingBlockMatchTestInstance(context, testParams, blockMatchingTestParams, testMatch,
                                            testConstantDifference, outImageType, outImageSize, outImageFormat)
{
}

ImageProcessingBlockMatchComputeTestInstance::~ImageProcessingBlockMatchComputeTestInstance(void)
{
}

void ImageProcessingBlockMatchComputeTestInstance::addSupplementaryDescBindings(
    DescriptorSetLayoutExtBuilder &layoutBuilder)
{
    layoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, m_params.stageMask);
}

void ImageProcessingBlockMatchComputeTestInstance::addSupplementaryDescTypes(DescriptorPoolBuilder &poolBuilder)
{
    poolBuilder.addType(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
}

void ImageProcessingBlockMatchComputeTestInstance::writeSupplementaryDescriptors()
{
    const auto storeDescImageInfo =
        makeDescriptorImageInfo(VK_NULL_HANDLE, m_outImageView.get(), VK_IMAGE_LAYOUT_GENERAL);
    m_descriptorUpdateBuilder.writeSingle(m_descriptorSet.get(), DescriptorSetUpdateBuilder::Location::binding(5u),
                                          VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &storeDescImageInfo);
}

void ImageProcessingBlockMatchComputeTestInstance::executeBarriers()
{
    const auto &vkd                  = m_context.getDeviceInterface();
    const auto colorSubresourceRange = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
    const VkImageMemoryBarrier outImageBarrier =
        makeImageMemoryBarrier(0u, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
                               m_outImage->get(), colorSubresourceRange);

    vkd.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                           (VkDependencyFlags)0, 0, nullptr, 0, nullptr, 1, &outImageBarrier);
}

void ImageProcessingBlockMatchComputeTestInstance::executeBindPipeline()
{
    const auto &vkd = m_context.getDeviceInterface();

    vkd.cmdBindPipeline(*m_cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_computePipeline.get());
}

void ImageProcessingBlockMatchComputeTestInstance::executeProgram()
{
    const auto &vkd         = m_context.getDeviceInterface();
    const VkExtent3D extent = makeExtent3D(m_outImageSize.x(), m_outImageSize.y(), 1u);

    vkd.cmdDispatch(*m_cmdBuffer, extent.width, extent.height, extent.depth);
}

TestStatus ImageProcessingBlockMatchComputeTestInstance::iterate(void)
{
    const auto &vkd   = m_context.getDeviceInterface();
    const auto device = m_context.getDevice();
    auto &allocator   = m_context.getDefaultAllocator();

    const bool unnorm = ((m_params.imageProcOp == ImageProcOp::IMAGE_PROC_OP_BLOCK_MATCH_SAD) ||
                         (m_params.imageProcOp == ImageProcOp::IMAGE_PROC_OP_BLOCK_MATCH_SSD));

    const auto texUsage = (VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLE_BLOCK_MATCH_BIT_QCOM);

    // Image types are actually same for both target and reference images - 2D
    const auto tgtImageViewType = mapImageViewType(m_blockMatchingParams.targetImageParams.imageType);
    const auto refImageViewType = mapImageViewType(m_params.sampledImageParams.imageType);
    const auto outImageUsage    = (VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT);
    const auto tgtTcuFormat     = mapVkFormat(m_blockMatchingParams.targetImageParams.format);
    const auto refTcuFormat     = mapVkFormat(m_params.sampledImageParams.format);

    // Create reference and target images
    const VkImageCreateInfo tgtImageCreateInfo = makeImageCreateInfo(
        m_blockMatchingParams.targetImageParams.imageType, m_blockMatchingParams.targetImageParams.imageSize,
        m_blockMatchingParams.targetImageParams.format, texUsage, 0u, m_blockMatchingParams.targetImageParams.tiling);
    const VkImageCreateInfo refImageCreateInfo =
        makeImageCreateInfo(m_params.sampledImageParams.imageType, m_params.sampledImageParams.imageSize,
                            m_params.sampledImageParams.format, texUsage, 0u, m_params.sampledImageParams.tiling);

    const ImageWithMemory tgtImage{vkd, device, allocator, tgtImageCreateInfo, MemoryRequirement::Any};
    const ImageWithMemory refImage{vkd, device, allocator, refImageCreateInfo, MemoryRequirement::Any};

    // Corresponding image views
    const auto colorSubresourceRange = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);

    m_targetView = makeImageViewUtil(vkd, device, tgtImage.get(), tgtImageViewType,
                                     m_blockMatchingParams.targetImageParams.format, colorSubresourceRange);
    m_referenceView =
        makeImageViewUtil(vkd, device, refImage.get(), refImageViewType, m_params.sampledImageParams.format,
                          colorSubresourceRange, m_params.sampledImageParams.components);

    // Create textures
    const VkDeviceSize tgtColorBufferSize = static_cast<VkDeviceSize>(
        static_cast<uint32_t>(getPixelSize(tgtTcuFormat)) * m_blockMatchingParams.targetImageParams.imageSize.x() *
        m_blockMatchingParams.targetImageParams.imageSize.y() * 1u);
    const auto tgtBufferInfo              = makeBufferCreateInfo(tgtColorBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    const VkDeviceSize refColorBufferSize = static_cast<VkDeviceSize>(
        static_cast<uint32_t>(getPixelSize(refTcuFormat)) * m_params.sampledImageParams.imageSize.x() *
        m_params.sampledImageParams.imageSize.y() * 1u);
    const auto refBufferInfo = makeBufferCreateInfo(refColorBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);

    const BufferWithMemory tgtColorBuffer{vkd, device, allocator, tgtBufferInfo, MemoryRequirement::HostVisible};
    const BufferWithMemory refColorBuffer{vkd, device, allocator, refBufferInfo, MemoryRequirement::HostVisible};

    // Fill reference color buffer
    populateColorBuffer(refColorBuffer, m_params.sampledImageParams.imageSize, m_params.sampledImageParams.format,
                        m_blockMatchingParams.referenceCoord);

    // Fill target color buffer
    if (m_testMatch)
        populateColorBuffer(tgtColorBuffer, m_blockMatchingParams.targetImageParams.imageSize,
                            m_blockMatchingParams.targetImageParams.format, m_blockMatchingParams.targetCoord,
                            true /* fillEmpty */, true /* useSrcColor */, &refColorBuffer,
                            m_params.sampledImageParams.imageSize, m_blockMatchingParams.referenceCoord);
    else
        populateColorBuffer(tgtColorBuffer, m_blockMatchingParams.targetImageParams.imageSize,
                            m_blockMatchingParams.targetImageParams.format, m_blockMatchingParams.targetCoord);

    // Prepare inputs and outputs
    const VkDeviceSize errBuffSizeBytes = sizeof(Vec4);
    m_errBuffer                         = de::MovePtr<BufferWithMemory>(new BufferWithMemory(
        vkd, device, allocator, makeBufferCreateInfo(errBuffSizeBytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT),
        MemoryRequirement::HostVisible));
    m_targetSampler                     = makeSampler(unnorm, m_blockMatchingParams.targetImageParams.addrMode,
                                                      m_blockMatchingParams.targetImageParams.reductionMode);
    m_referenceSampler =
        makeSampler(unnorm, m_params.sampledImageParams.addrMode, m_params.sampledImageParams.reductionMode);

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

    // Result = red on mismatch, green on match
    const VkExtent3D extent = makeExtent3D(m_outImageSize.x(), m_outImageSize.y(), 1u);
    const VkDeviceSize resultBufferSize =
        static_cast<VkDeviceSize>(static_cast<uint32_t>(getPixelSize(mapVkFormat(m_outImageFormat))) * extent.width *
                                  extent.height * extent.depth);
    const IVec2 resultSize{static_cast<int>(extent.width), static_cast<int>(extent.height)};
    const auto resultBufferInfo = makeBufferCreateInfo(resultBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    const BufferWithMemory resultBuffer{vkd, device, allocator, resultBufferInfo, MemoryRequirement::HostVisible};

    // Command execution
    executeCommands(pipelineLayout.get(), tgtColorBuffer, tgtImage, refColorBuffer, refImage, resultBuffer,
                    *m_outImage);

    // Get results
    const auto &resultBufferAlloc = resultBuffer.getAllocation();
    invalidateAlloc(vkd, device, resultBufferAlloc);
    const auto &errBufferAllocation = (*m_errBuffer).getAllocation();
    invalidateAlloc(vkd, device, errBufferAllocation);

    const auto resultsBufferPtr =
        reinterpret_cast<const char *>(resultBufferAlloc.getHostPtr()) + resultBufferAlloc.getOffset();
    const ConstPixelBufferAccess resultPixels{mapVkFormat(m_outImageFormat), resultSize[0], resultSize[1], 1,
                                              resultsBufferPtr};

    const Vec4 *errBufferPtr = static_cast<Vec4 *>(errBufferAllocation.getHostPtr());

    // Get reference result
    // Use address mode of target and reduction mode of reference
    const VkSamplerReductionMode redMode = getVkSamplerReductionMode(m_params.sampledImageParams.reductionMode);
    ImageProcessingResult expectedResult(mapVkFormat(m_outImageFormat), m_outImageSize.x(), m_outImageSize.y(),
                                         m_blockMatchingParams.targetImageParams.addrMode, redMode);
    const auto expectedBlockMatchingError = buildStandardResult(expectedResult, tgtColorBuffer, refColorBuffer);
    const Vec4 resultError                = *errBufferPtr;

    // Assumption: reference and target formats are same
    const Vec4 errorThreshold = calculateErrorThreshold(m_blockMatchingParams.blockSize, tgtTcuFormat);
    return verifyResult(expectedBlockMatchingError, resultError, expectedResult.getAccess(), resultPixels,
                        errorThreshold);
}

TestInstance *ImageProcessingBlockMatchGraphicsTest::createInstance(Context &context) const
{
    return new ImageProcessingBlockMatchGraphicsTestInstance(context, m_params, m_blockMatchingParams, m_testMatch,
                                                             m_testConstantDifference, m_outImageType, m_outImageSize,
                                                             m_outImageFormat);
}

TestInstance *ImageProcessingBlockMatchComputeTest::createInstance(Context &context) const
{
    return new ImageProcessingBlockMatchComputeTestInstance(context, m_params, m_blockMatchingParams, m_testMatch,
                                                            m_testConstantDifference, m_outImageType, m_outImageSize,
                                                            m_outImageFormat);
}

// Test to compare two blocks of the same image
class ImageProcessingBlockMatchSelfTest : public ImageProcessingBlockMatchComputeTest
{
public:
    ImageProcessingBlockMatchSelfTest(TestContext &testCtx, const std::string &name, const TestParams &testParams,
                                      const BlockMatchingTestParams &blockMatchingTestParams, const bool testMatch);
    virtual ~ImageProcessingBlockMatchSelfTest(void);
    virtual TestInstance *createInstance(Context &context) const;
};

ImageProcessingBlockMatchSelfTest::ImageProcessingBlockMatchSelfTest(
    TestContext &testCtx, const std::string &name, const TestParams &testParams,
    const BlockMatchingTestParams &blockMatchingTestParams, const bool testMatch)
    : ImageProcessingBlockMatchComputeTest(testCtx, name, testParams, blockMatchingTestParams, testMatch)
{
}

ImageProcessingBlockMatchSelfTest::~ImageProcessingBlockMatchSelfTest(void)
{
}

class ImageProcessingBlockMatchSelfTestInstance : public ImageProcessingBlockMatchComputeTestInstance
{
public:
    ImageProcessingBlockMatchSelfTestInstance(Context &context, const TestParams &params,
                                              const BlockMatchingTestParams &blockMatchingTestParams,
                                              const bool testMatch, const ImageType outImageType,
                                              const UVec2 outImageSize, const VkFormat outImageFormat);
    ~ImageProcessingBlockMatchSelfTestInstance(void);

    virtual TestStatus iterate(void);
};

ImageProcessingBlockMatchSelfTestInstance::ImageProcessingBlockMatchSelfTestInstance(
    Context &context, const TestParams &testParams, const BlockMatchingTestParams &blockMatchingTestParams,
    const bool testMatch, const ImageType outImageType, const UVec2 outImageSize, const VkFormat outImageFormat)
    : ImageProcessingBlockMatchComputeTestInstance(context, testParams, blockMatchingTestParams, testMatch,
                                                   false /*testConstantDifference*/, outImageType, outImageSize,
                                                   outImageFormat)
{
}

ImageProcessingBlockMatchSelfTestInstance::~ImageProcessingBlockMatchSelfTestInstance(void)
{
}

TestStatus ImageProcessingBlockMatchSelfTestInstance::iterate(void)
{
    const auto &vkd   = m_context.getDeviceInterface();
    const auto device = m_context.getDevice();
    auto &allocator   = m_context.getDefaultAllocator();

    const bool unnorm = ((m_params.imageProcOp == ImageProcOp::IMAGE_PROC_OP_BLOCK_MATCH_SAD) ||
                         (m_params.imageProcOp == ImageProcOp::IMAGE_PROC_OP_BLOCK_MATCH_SSD));

    const auto texUsage = (VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLE_BLOCK_MATCH_BIT_QCOM);

    // Image types are actually same for both target and reference images - 2D
    // In case of self test, most properties are same
    const auto tgtImageViewType = mapImageViewType(m_blockMatchingParams.targetImageParams.imageType);
    const auto outImageUsage    = (VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT);

    const auto tgtTcuFormat = mapVkFormat(m_blockMatchingParams.targetImageParams.format);

    // Create reference and target images
    const VkImageCreateInfo tgtImageCreateInfo = makeImageCreateInfo(
        m_blockMatchingParams.targetImageParams.imageType, m_blockMatchingParams.targetImageParams.imageSize,
        m_blockMatchingParams.targetImageParams.format, texUsage, 0u, m_blockMatchingParams.targetImageParams.tiling);

    const ImageWithMemory tgtImage{vkd, device, allocator, tgtImageCreateInfo, MemoryRequirement::Any};

    // Corresponding image views
    const auto colorSubresourceRange = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);

    m_targetView = makeImageViewUtil(vkd, device, tgtImage.get(), tgtImageViewType,
                                     m_blockMatchingParams.targetImageParams.format, colorSubresourceRange);

    // Create textures
    const VkDeviceSize singleColorBufferSize = static_cast<VkDeviceSize>(
        static_cast<uint32_t>(getPixelSize(tgtTcuFormat)) * m_blockMatchingParams.targetImageParams.imageSize.x() *
        m_blockMatchingParams.targetImageParams.imageSize.y() * 1u);
    const auto singleBufferInfo = makeBufferCreateInfo(singleColorBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);

    const BufferWithMemory singleColorBuffer{vkd, device, allocator, singleBufferInfo, MemoryRequirement::HostVisible};

    // Fill reference region of the color buffer using target image size and format
    populateColorBuffer(singleColorBuffer, m_blockMatchingParams.targetImageParams.imageSize,
                        m_blockMatchingParams.targetImageParams.format, m_blockMatchingParams.referenceCoord);

    // Fill target region of the color buffer
    populateColorBuffer(singleColorBuffer, m_blockMatchingParams.targetImageParams.imageSize,
                        m_blockMatchingParams.targetImageParams.format, m_blockMatchingParams.targetCoord,
                        false /* fillEmpty */, true /* useSrcColor */, &singleColorBuffer,
                        m_params.sampledImageParams.imageSize, m_blockMatchingParams.referenceCoord);

    // Prepare inputs and outputs
    const VkDeviceSize errBuffSizeBytes = sizeof(Vec4);
    m_errBuffer                         = de::MovePtr<BufferWithMemory>(new BufferWithMemory(
        vkd, device, allocator, makeBufferCreateInfo(errBuffSizeBytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT),
        MemoryRequirement::HostVisible));

    // Even though target and reference image are the same in self test, each has its own sampler
    m_targetSampler = makeSampler(unnorm, m_blockMatchingParams.targetImageParams.addrMode,
                                  m_blockMatchingParams.targetImageParams.reductionMode);
    m_referenceSampler =
        makeSampler(unnorm, m_params.sampledImageParams.addrMode, m_params.sampledImageParams.reductionMode);

    const VkImageCreateInfo outImageCreateInfo = makeImageCreateInfo(m_outImageType, m_outImageSize, m_outImageFormat,
                                                                     outImageUsage, 0u, VK_IMAGE_TILING_OPTIMAL);
    m_outImage                                 = de::MovePtr<ImageWithMemory>(
        new ImageWithMemory(vkd, device, allocator, outImageCreateInfo, MemoryRequirement::Any));

    m_outImageView = makeImageViewUtil(vkd, device, m_outImage->get(), VK_IMAGE_VIEW_TYPE_2D, m_outImageFormat,
                                       colorSubresourceRange);

    prepareDescriptors(true /* useTargetAsReference */);

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

    // Result = red on mismatch, green on match
    const VkExtent3D extent = makeExtent3D(m_outImageSize.x(), m_outImageSize.y(), 1u);
    const VkDeviceSize resultBufferSize =
        static_cast<VkDeviceSize>(static_cast<uint32_t>(getPixelSize(mapVkFormat(m_outImageFormat))) * extent.width *
                                  extent.height * extent.depth);
    const IVec2 resultSize{static_cast<int>(extent.width), static_cast<int>(extent.height)};
    const auto resultBufferInfo = makeBufferCreateInfo(resultBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    const BufferWithMemory resultBuffer{vkd, device, allocator, resultBufferInfo, MemoryRequirement::HostVisible};

    // Command execution
    executeCommands(pipelineLayout.get(), singleColorBuffer, tgtImage, singleColorBuffer, tgtImage, resultBuffer,
                    *m_outImage, true /*isSelfTest*/);

    // Get results
    const auto &resultBufferAlloc = resultBuffer.getAllocation();
    invalidateAlloc(vkd, device, resultBufferAlloc);
    const auto &errBufferAllocation = (*m_errBuffer).getAllocation();
    invalidateAlloc(vkd, device, errBufferAllocation);

    const auto resultsBufferPtr =
        reinterpret_cast<const char *>(resultBufferAlloc.getHostPtr()) + resultBufferAlloc.getOffset();
    const ConstPixelBufferAccess resultPixels{mapVkFormat(m_outImageFormat), resultSize[0], resultSize[1], 1,
                                              resultsBufferPtr};

    const Vec4 *errBufferPtr = static_cast<Vec4 *>(errBufferAllocation.getHostPtr());

    // Get reference result
    // Use address mode of target image and reduction mode of reference image
    // In any case, both are same in self test
    const VkSamplerReductionMode redMode = getVkSamplerReductionMode(m_params.sampledImageParams.reductionMode);
    ImageProcessingResult expectedResult(mapVkFormat(m_outImageFormat), m_outImageSize.x(), m_outImageSize.y(),
                                         m_blockMatchingParams.targetImageParams.addrMode, redMode);
    const auto expectedBlockMatchingError = buildStandardResult(expectedResult, singleColorBuffer, singleColorBuffer);
    const Vec4 resultError                = *errBufferPtr;

    // Reference and target formats are same in case of self
    const Vec4 errorThreshold = calculateErrorThreshold(m_blockMatchingParams.blockSize, tgtTcuFormat);
    return verifyResult(expectedBlockMatchingError, resultError, expectedResult.getAccess(), resultPixels,
                        errorThreshold);
}

TestInstance *ImageProcessingBlockMatchSelfTest::createInstance(Context &context) const
{
    return new ImageProcessingBlockMatchSelfTestInstance(context, m_params, m_blockMatchingParams, m_testMatch,
                                                         m_outImageType, m_outImageSize, m_outImageFormat);
}

struct CombinedTestParams
{
    TestParams testParams;
    BlockMatchingTestParams blockMatchingParams;
};

CombinedTestParams getCommonTestParams(const ImageProcOp op, const VkFormat format = VK_FORMAT_R8G8B8A8_UNORM,
                                       VkShaderStageFlags stageMask = VK_SHADER_STAGE_FRAGMENT_BIT)
{
    CombinedTestParams combinedParams;

    // Target image parameters
    const TestImageParams defaultTgtImageParams = {
        IMAGE_TYPE_2D,                            // imageType
        UVec2(64u, 64u),                          // imageSize
        format,                                   // format
        VK_IMAGE_TILING_OPTIMAL,                  // tiling
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, // layout
        makeComponentMappingIdentity(),           // components
        VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,    // addrMode
        SAMPLER_REDUCTION_MODE_NONE               // reductionMode
    };

    // Reference image parameters
    const TestImageParams defaultRefImageParams = defaultTgtImageParams;

    const BlockMatchingTestParams defaultBlockMatchingParams = {
        defaultTgtImageParams, // targetImageParams: Target image parameters are same as reference image for basic tests
        UVec2(0u, 0u),         // targetCoord
        UVec2(0u, 0u),         // referenceCoord
        UVec2(32u, 32u)        // blockSize
    };

    const TestParams defaultTestParams = {
        op,                                    // imageProcOp
        defaultRefImageParams,                 // sampledImageParams
        true,                                  // randomReference
        false,                                 // updateAfterBind
        PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC, // pipelineConstructionType
        stageMask                              // stageMask
    };

    combinedParams.blockMatchingParams = defaultBlockMatchingParams;
    combinedParams.testParams          = defaultTestParams;

    return combinedParams;
}

std::vector<CombinedTestParams> getSamplerAddressModeTestParams(const ImageProcOp op,
                                                                const VkSamplerAddressMode addrMode,
                                                                const VkFormat format)
{
    std::vector<CombinedTestParams> combinedParams;

    CombinedTestParams commonCombinedParams = getCommonTestParams(op, format);

    // Target and reference images have the same address mode
    commonCombinedParams.testParams.sampledImageParams.addrMode         = addrMode;
    commonCombinedParams.blockMatchingParams.targetImageParams.addrMode = addrMode;

    {
        CombinedTestParams params0              = commonCombinedParams;
        params0.blockMatchingParams.targetCoord = UVec2(32u, 32u); // center
        params0.blockMatchingParams.blockSize   = UVec2(40u, 40u); // out of bounds for target image

        combinedParams.push_back(params0);
    }

    // Target image is smaller than reference image
    {
        CombinedTestParams params1 = commonCombinedParams;

        params1.blockMatchingParams.targetImageParams.imageSize = UVec2(16u, 16u);
        params1.testParams.sampledImageParams.imageSize         = UVec2(32u, 32u);
        params1.blockMatchingParams.blockSize =
            UVec2(params1.testParams.sampledImageParams.imageSize.x(),
                  params1.testParams.sampledImageParams.imageSize.y()); // out of bounds for target image

        combinedParams.push_back(params1);
    }

    {
        CombinedTestParams params2 = commonCombinedParams;
        // Block is outside the corner of target image
        params2.blockMatchingParams.targetCoord = UVec2(64u, 64u);

        combinedParams.push_back(params2);
    }

    return combinedParams;
}

std::vector<CombinedTestParams> getSamplerReductionModeTestParams(const ImageProcOp op,
                                                                  const SamplerReductionMode refRedMode,
                                                                  const VkFormat format)
{
    std::vector<CombinedTestParams> combinedParams;

    CombinedTestParams commonCombinedParams = getCommonTestParams(op, format);

    commonCombinedParams.testParams.sampledImageParams.reductionMode = refRedMode;

    // Different reduction mode for target and reference images
    for (uint32_t tgtRedMode = SAMPLER_REDUCTION_MODE_NONE; tgtRedMode <= SAMPLER_REDUCTION_MODE_MAX; tgtRedMode++)
    {
        CombinedTestParams params = commonCombinedParams;

        params.blockMatchingParams.targetImageParams.reductionMode = static_cast<SamplerReductionMode>(tgtRedMode);

        combinedParams.push_back(params);
    }

    return combinedParams;
}

std::vector<CombinedTestParams> getTilingTestParams(const ImageProcOp op, const VkImageTiling refTiling,
                                                    const VkFormat format)
{
    std::vector<CombinedTestParams> combinedParams;

    CombinedTestParams commonCombinedParams = getCommonTestParams(op, format);

    commonCombinedParams.testParams.sampledImageParams.tiling = refTiling;

    for (const auto tilingType : {VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_TILING_LINEAR})
    {
        CombinedTestParams params = commonCombinedParams;

        // Both target and reference images using optimal tiling case is covered in basic tests
        if ((refTiling == VK_IMAGE_TILING_OPTIMAL) && (tilingType == VK_IMAGE_TILING_OPTIMAL))
            continue;

        params.blockMatchingParams.targetImageParams.tiling = tilingType;

        combinedParams.push_back(params);
    }

    return combinedParams;
}

std::vector<CombinedTestParams> getLayoutTestParams(const ImageProcOp op, const VkImageLayout refLayout,
                                                    const VkFormat format)
{
    std::vector<CombinedTestParams> combinedParams;

    CombinedTestParams commonCombinedParams = getCommonTestParams(op, format);

    commonCombinedParams.testParams.sampledImageParams.layout = refLayout;

    for (const auto layout : {VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL})
    {
        CombinedTestParams params = commonCombinedParams;

        // Both target and reference images using optimal layout is covered in basic tests
        if ((refLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) &&
            (layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL))
            continue;

        params.blockMatchingParams.targetImageParams.layout = layout;

        combinedParams.push_back(params);
    }

    return combinedParams;
}

std::vector<CombinedTestParams> getBlockSizeTestParams(const ImageProcOp op,
                                                       const VkFormat format = VK_FORMAT_R8G8B8A8_UNORM)
{
    std::vector<CombinedTestParams> combinedParams;

    CombinedTestParams commonCombinedParams = getCommonTestParams(op, format);

    // Common image size 64x64
    const struct
    {
        UVec2 targetCoord;
        UVec2 referenceCoord;
        UVec2 blockSize;
    } blockSizes[] = {// non-zero target coordinates
                      {
                          UVec2(32u, 32u), // targetCoord
                          UVec2(0u, 0u),   // referenceCoord
                          UVec2(32u, 32u)  // blockSize
                      },

                      // non-zero reference coordinates
                      {
                          UVec2(0u, 0u),   // targetCoord
                          UVec2(16u, 16u), // referenceCoord
                          UVec2(32u, 32u)  // blockSize
                      },

                      // one block size
                      {
                          UVec2(0u, 0u), // targetCoord
                          UVec2(0u, 0u), // referenceCoord
                          UVec2(1u, 1u)  // blockSize
                      },

                      // block size = image size
                      {
                          UVec2(0u, 0u),  // targetCoord
                          UVec2(0u, 0u),  // referenceCoord
                          UVec2(64u, 64u) // blockSize
                      },

                      // block size = rectangular
                      {
                          UVec2(0u, 0u),  // targetCoord
                          UVec2(63u, 0u), // referenceCoord
                          UVec2(1u, 64u)  // blockSize
                      }};

    for (int idx = 0; idx < DE_LENGTH_OF_ARRAY(blockSizes); idx++)
    {
        CombinedTestParams params = commonCombinedParams;

        params.blockMatchingParams.targetCoord    = blockSizes[idx].targetCoord;
        params.blockMatchingParams.referenceCoord = blockSizes[idx].referenceCoord;
        params.blockMatchingParams.blockSize      = blockSizes[idx].blockSize;

        combinedParams.push_back(params);
    }

    return combinedParams;
}

} // namespace

TestCaseGroup *createImageProcessingBlockMatchingCommonTests(
    TestContext &testCtx, const bool testCompute,
    const PipelineConstructionType pipelineConstructionType = PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC)
{
    de::MovePtr<TestCaseGroup> testGroup(new TestCaseGroup(testCtx, "block_matching"));

    const struct
    {
        const ImageProcOp op;
        const std::string opName;
        const std::vector<VkFormat> opFormats;

    } imageProcessingOps[] = {

        {ImageProcOp::IMAGE_PROC_OP_BLOCK_MATCH_SAD, "sad",
         getOpSupportedFormats(ImageProcOp::IMAGE_PROC_OP_BLOCK_MATCH_SAD)},
        {ImageProcOp::IMAGE_PROC_OP_BLOCK_MATCH_SSD, "ssd",
         getOpSupportedFormats(ImageProcOp::IMAGE_PROC_OP_BLOCK_MATCH_SSD)}};

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
        const VkImageTiling tiling;
        const std::string tilingName;
    } tilingTypes[] = {{VK_IMAGE_TILING_OPTIMAL, "optimal"}, {VK_IMAGE_TILING_LINEAR, "linear"}};

    const struct
    {
        const VkImageLayout layout;
        const std::string layoutName;
    } layouts[] = {{VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, "rdonly_optimal"}, {VK_IMAGE_LAYOUT_GENERAL, "general"}};

    // Non-identity component mapping for reference image
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

    // Shader stages in which block matching operation is used
    // Fragment stage is already tested in basic tests
    const VkShaderStageFlags shaderStages[]{
        VK_SHADER_STAGE_VERTEX_BIT,
    };

    for (int imageProcOpNdx = 0; imageProcOpNdx < DE_LENGTH_OF_ARRAY(imageProcessingOps); ++imageProcOpNdx)
    {
        de::MovePtr<TestCaseGroup> imageProcOpGroup(
            new TestCaseGroup(testCtx, imageProcessingOps[imageProcOpNdx].opName.c_str()));

        const std::vector<VkFormat> &supportedFormats = imageProcessingOps[imageProcOpNdx].opFormats;

        // Basic tests
        {
            de::MovePtr<TestCaseGroup> basicGroup(new TestCaseGroup(testCtx, "basic"));

            for (size_t imageFormatNdx = 0; imageFormatNdx < supportedFormats.size(); ++imageFormatNdx)
            {
                CombinedTestParams params =
                    getCommonTestParams(imageProcessingOps[imageProcOpNdx].op, supportedFormats[imageFormatNdx]);

                for (const auto &match : {true, false})
                {
                    for (const auto &randomReference : {true, false})
                    {
                        for (const auto &constantDifference : {true, false})
                        {
                            // Constant difference does not apply to matching blocks
                            if (match && constantDifference)
                                continue;

                            params.testParams.randomReference          = randomReference;
                            params.testParams.pipelineConstructionType = pipelineConstructionType;

                            const auto testName = getFormatShortString(supportedFormats[imageFormatNdx]) +
                                                  (match ? "_same" : "_diff") + (randomReference ? "_random" : "") +
                                                  (constantDifference ? "_constdiff" : "");

                            if (!testCompute)
                                basicGroup->addChild(new ImageProcessingBlockMatchGraphicsTest(
                                    testCtx, testName, params.testParams, params.blockMatchingParams, match,
                                    constantDifference));
                            else
                            {
                                params.testParams.stageMask = VK_SHADER_STAGE_COMPUTE_BIT;

                                basicGroup->addChild(new ImageProcessingBlockMatchComputeTest(
                                    testCtx, testName, params.testParams, params.blockMatchingParams, match,
                                    constantDifference));
                            }
                        }
                    }
                }
            }

            imageProcOpGroup->addChild(basicGroup.release());
        }

        // Compute only has basic tests
        if (!testCompute && (pipelineConstructionType == PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC))
        {
            // Different block sizes to match
            {
                de::MovePtr<TestCaseGroup> blockSizeGroup(new TestCaseGroup(testCtx, "block_sizes"));

                const std::vector<CombinedTestParams> &blockSizeTestParamsList =
                    getBlockSizeTestParams(imageProcessingOps[imageProcOpNdx].op);

                for (size_t paramNdx = 0; paramNdx < blockSizeTestParamsList.size(); ++paramNdx)
                {
                    CombinedTestParams params = blockSizeTestParamsList[paramNdx];

                    const auto testName = "params" + de::toString(paramNdx);
                    ;

                    blockSizeGroup->addChild(new ImageProcessingBlockMatchGraphicsTest(
                        testCtx, testName, params.testParams, params.blockMatchingParams, true /* match */,
                        false /* constantDifference*/));
                }

                imageProcOpGroup->addChild(blockSizeGroup.release());
            }

            // Sampler address modes
            {
                // Same address mode used for target and reference images
                de::MovePtr<TestCaseGroup> addrModesGroup(new TestCaseGroup(testCtx, "address_modes"));

                for (int addrModeNdx = 0; addrModeNdx < DE_LENGTH_OF_ARRAY(addressModes); ++addrModeNdx)
                {
                    for (size_t imageFormatNdx = 0; imageFormatNdx < supportedFormats.size(); ++imageFormatNdx)
                    {
                        const std::vector<CombinedTestParams> &addrModeTestParamsList = getSamplerAddressModeTestParams(
                            imageProcessingOps[imageProcOpNdx].op, addressModes[addrModeNdx].addrMode,
                            supportedFormats[imageFormatNdx]);

                        for (size_t paramNdx = 0; paramNdx < addrModeTestParamsList.size(); ++paramNdx)
                        {
                            const CombinedTestParams &params = addrModeTestParamsList[paramNdx];

                            const std::string paramsName = "_params" + de::toString(paramNdx);
                            const auto testName          = addressModes[addrModeNdx].addrModeName + paramsName + "_" +
                                                  getFormatShortString(supportedFormats[imageFormatNdx]);

                            addrModesGroup->addChild(new ImageProcessingBlockMatchGraphicsTest(
                                testCtx, testName, params.testParams, params.blockMatchingParams, true /* match */,
                                false /* constantDifference*/));
                        }
                    }
                }

                imageProcOpGroup->addChild(addrModesGroup.release());
            }

            // Sampler reduction modes
            {
                // Combinations of reference and target reduction modes
                de::MovePtr<TestCaseGroup> reductionModesGroup(new TestCaseGroup(testCtx, "reduction_modes"));

                // Reference image reduction modes
                for (int redModeNdx = 0; redModeNdx < DE_LENGTH_OF_ARRAY(reductionModes); ++redModeNdx)
                {
                    for (size_t imageFormatNdx = 0; imageFormatNdx < supportedFormats.size(); ++imageFormatNdx)
                    {
                        const std::vector<CombinedTestParams> &redModeTestParamsList =
                            getSamplerReductionModeTestParams(imageProcessingOps[imageProcOpNdx].op,
                                                              reductionModes[redModeNdx].reductionMode,
                                                              supportedFormats[imageFormatNdx]);

                        for (size_t paramNdx = 0; paramNdx < redModeTestParamsList.size(); ++paramNdx)
                        {
                            const CombinedTestParams &params = redModeTestParamsList[paramNdx];

                            const std::string paramsName = "_params" + de::toString(paramNdx);
                            const auto testName = reductionModes[redModeNdx].reductionModeName + paramsName + "_" +
                                                  getFormatShortString(supportedFormats[imageFormatNdx]);

                            reductionModesGroup->addChild(new ImageProcessingBlockMatchGraphicsTest(
                                testCtx, testName, params.testParams, params.blockMatchingParams, true /* match */,
                                false /* constantDifference*/));
                        }
                    }
                }

                imageProcOpGroup->addChild(reductionModesGroup.release());
            }

            // Tiling
            {
                de::MovePtr<TestCaseGroup> tilingGroup(new TestCaseGroup(testCtx, "tiling"));

                for (int tilingNdx = 0; tilingNdx < DE_LENGTH_OF_ARRAY(tilingTypes); ++tilingNdx)
                {
                    for (size_t imageFormatNdx = 0; imageFormatNdx < supportedFormats.size(); ++imageFormatNdx)
                    {
                        const std::vector<CombinedTestParams> &tilingTestParamsList =
                            getTilingTestParams(imageProcessingOps[imageProcOpNdx].op, tilingTypes[tilingNdx].tiling,
                                                supportedFormats[imageFormatNdx]);

                        for (size_t paramNdx = 0; paramNdx < tilingTestParamsList.size(); ++paramNdx)
                        {
                            const CombinedTestParams &params = tilingTestParamsList[paramNdx];

                            const std::string paramsName = "_params" + de::toString(paramNdx);
                            const auto testName          = tilingTypes[tilingNdx].tilingName + paramsName + "_" +
                                                  getFormatShortString(supportedFormats[imageFormatNdx]);

                            tilingGroup->addChild(new ImageProcessingBlockMatchGraphicsTest(
                                testCtx, testName, params.testParams, params.blockMatchingParams, true /* match */,
                                false /* constantDifference*/));
                        }
                    }
                }

                imageProcOpGroup->addChild(tilingGroup.release());
            }

            // Swizzles for reference image
            {
                de::MovePtr<TestCaseGroup> swizzleGroup(new TestCaseGroup(testCtx, "swizzles"));

                for (int swizzleNdx = 0; swizzleNdx < DE_LENGTH_OF_ARRAY(swizzles); ++swizzleNdx)
                {
                    CombinedTestParams params = getCommonTestParams(imageProcessingOps[imageProcOpNdx].op);
                    params.testParams.sampledImageParams.components = swizzles[swizzleNdx].components;

                    const auto testName = swizzles[swizzleNdx].compMappingName;

                    swizzleGroup->addChild(new ImageProcessingBlockMatchGraphicsTest(
                        testCtx, testName, params.testParams, params.blockMatchingParams, true /* match */,
                        false /* constantDifference*/));
                }

                imageProcOpGroup->addChild(swizzleGroup.release());
            }

            // Image layouts
            {
                de::MovePtr<TestCaseGroup> layoutGroup(new TestCaseGroup(testCtx, "layouts"));

                for (int layoutNdx = 0; layoutNdx < DE_LENGTH_OF_ARRAY(layouts); ++layoutNdx)
                {
                    for (size_t imageFormatNdx = 0; imageFormatNdx < supportedFormats.size(); ++imageFormatNdx)
                    {
                        const std::vector<CombinedTestParams> &layoutTestParamsList =
                            getLayoutTestParams(imageProcessingOps[imageProcOpNdx].op, layouts[layoutNdx].layout,
                                                supportedFormats[imageFormatNdx]);

                        for (size_t paramNdx = 0; paramNdx < layoutTestParamsList.size(); ++paramNdx)
                        {
                            const CombinedTestParams &params = layoutTestParamsList[paramNdx];

                            const std::string paramsName = "_params" + de::toString(paramNdx);
                            const auto testName          = layouts[layoutNdx].layoutName + paramsName + "_" +
                                                  getFormatShortString(supportedFormats[imageFormatNdx]);

                            layoutGroup->addChild(new ImageProcessingBlockMatchGraphicsTest(
                                testCtx, testName, params.testParams, params.blockMatchingParams, true /* match */,
                                false /* constantDifference */));
                        }
                    }
                }

                imageProcOpGroup->addChild(layoutGroup.release());
            }

            // Block matching used in other shader stages
            {
                // Testing with fixed format having all components, only block match, fixed address mode: VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE and no sampler reduction mode, optimal tiling and swizzle
                de::MovePtr<TestCaseGroup> stageGroup(new TestCaseGroup(testCtx, "shader_stages"));

                for (int stageNdx = 0; stageNdx < DE_LENGTH_OF_ARRAY(shaderStages); ++stageNdx)
                {
                    CombinedTestParams params   = getCommonTestParams(imageProcessingOps[imageProcOpNdx].op);
                    params.testParams.stageMask = shaderStages[stageNdx];

                    const auto testName = getStageNames(shaderStages[stageNdx]);

                    stageGroup->addChild(new ImageProcessingBlockMatchGraphicsTest(
                        testCtx, testName, params.testParams, params.blockMatchingParams, true /* match */,
                        false /* constantDifference */));
                }

                imageProcOpGroup->addChild(stageGroup.release());
            }

            // Descriptor tests
            {
                de::MovePtr<TestCaseGroup> descGroup(new TestCaseGroup(testCtx, "descriptors"));

                // update-after-bind
                {
                    CombinedTestParams params         = getCommonTestParams(imageProcessingOps[imageProcOpNdx].op);
                    params.testParams.updateAfterBind = true;

                    for (const auto &match : {true, false})
                    {
                        for (const auto &randomReference : {true, false})
                        {
                            params.testParams.randomReference = randomReference;

                            const auto testName = std::string("updateAfterBind") + "_" + (match ? "same" : "diff") +
                                                  (randomReference ? "_random" : "");

                            descGroup->addChild(new ImageProcessingBlockMatchGraphicsTest(
                                testCtx, testName, params.testParams, params.blockMatchingParams, match));
                        }
                    }
                }

                imageProcOpGroup->addChild(descGroup.release());
            }
        }
        else
        {
            if (testCompute)
            {
                // Self tests: Compare different blocks of the same image
                {
                    de::MovePtr<TestCaseGroup> selfGroup(new TestCaseGroup(testCtx, "self"));

                    CombinedTestParams params = getCommonTestParams(
                        imageProcessingOps[imageProcOpNdx].op, VK_FORMAT_R8G8B8A8_UNORM, VK_SHADER_STAGE_COMPUTE_BIT);

                    // Overlap case not supported by test case implementation
                    params.blockMatchingParams.referenceCoord = UVec2(32u, 32u);
                    params.blockMatchingParams.targetCoord    = UVec2(0u, 0u);

                    for (const auto &match : {true, false})
                    {
                        for (const auto &randomReference : {true, false})
                        {
                            params.testParams.randomReference = randomReference;

                            const auto testName =
                                std::string(match ? "same" : "diff") + (randomReference ? "_random" : "");

                            selfGroup->addChild(new ImageProcessingBlockMatchSelfTest(
                                testCtx, testName, params.testParams, params.blockMatchingParams, match));
                        }
                    }

                    imageProcOpGroup->addChild(selfGroup.release());
                }
            }
        }

        testGroup->addChild(imageProcOpGroup.release());
    }

    return testGroup.release();
}

TestCaseGroup *createImageProcessingBlockMatchingGraphicsTests(TestContext &testCtx,
                                                               const PipelineConstructionType pipelineConstructionType)
{
    return createImageProcessingBlockMatchingCommonTests(testCtx, false, pipelineConstructionType);
}

TestCaseGroup *createImageProcessingBlockMatchingComputeTests(TestContext &testCtx)
{
    return createImageProcessingBlockMatchingCommonTests(testCtx, true);
}

} // namespace ImageProcessing
} // namespace vkt
