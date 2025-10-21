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
 * \brief Image processing box filter sampling tests
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

struct BoxFilterSamplingTestParams
{
    bool isUnnormCoord;
    Vec2 textureCoord;
    Vec2 boxSize;
};

struct TestPushConstants
{
    Vec2 textureCoord;
    Vec2 boxSize;
};

class ImageProcessingBoxFilterTest : public ImageProcessingTest
{
public:
    ImageProcessingBoxFilterTest(TestContext &testCtx, const std::string &name, const TestParams &testParams,
                                 const BoxFilterSamplingTestParams &boxFilterTestParams);

    virtual ~ImageProcessingBoxFilterTest(void);
    virtual void checkSupport(Context &context) const;

protected:
    const BoxFilterSamplingTestParams m_boxFilterParams;
    const ImageType m_outImageType;
    const UVec2 m_outImageSize;
    const VkFormat m_outImageFormat;
};

ImageProcessingBoxFilterTest::ImageProcessingBoxFilterTest(TestContext &testCtx, const std::string &name,
                                                           const TestParams &testParams,
                                                           const BoxFilterSamplingTestParams &boxFilterTestParams)
    : ImageProcessingTest(testCtx, name, testParams)
    , m_boxFilterParams(boxFilterTestParams)
    , m_outImageType(IMAGE_TYPE_2D)
    , m_outImageSize(4u, 4u)
    , m_outImageFormat(VK_FORMAT_R8G8B8A8_UNORM)
{
}

ImageProcessingBoxFilterTest::~ImageProcessingBoxFilterTest(void)
{
}

void ImageProcessingBoxFilterTest::checkSupport(Context &context) const
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

        if ((m_boxFilterParams.boxSize.x() > static_cast<float>(imgProcProperties.maxBoxFilterBlockSize.width)) ||
            (m_boxFilterParams.boxSize.y() > static_cast<float>(imgProcProperties.maxBoxFilterBlockSize.height)))
            TCU_THROW(NotSupportedError, "Box filter size is greater than supported device limits");

        if ((m_boxFilterParams.boxSize.x() < 1.0f))
            TCU_THROW(NotSupportedError, "Box filter size is less than supported limits");
    }

    {
        VkImageFormatProperties sampledImageFormatProperties;
        const auto result = vki.getPhysicalDeviceImageFormatProperties(
            physicalDevice, m_params.sampledImageParams.format, mapImageType(m_params.sampledImageParams.imageType),
            m_params.sampledImageParams.tiling, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, 0,
            &sampledImageFormatProperties);

        if (result != VK_SUCCESS)
        {
            if (result == VK_ERROR_FORMAT_NOT_SUPPORTED)
                TCU_THROW(NotSupportedError, "Sampled image format unsupported for box filter sampling");
            else
                TCU_FAIL("vkGetPhysicalDeviceImageFormatProperties returned unexpected error");
        }
    }
}
class ImageProcessingBoxFilterGraphicsTest : public ImageProcessingBoxFilterTest
{
public:
    ImageProcessingBoxFilterGraphicsTest(TestContext &testCtx, const std::string &name, const TestParams &testParams,
                                         const BoxFilterSamplingTestParams &boxFilterTestParams);
    virtual ~ImageProcessingBoxFilterGraphicsTest(void);

    virtual void checkSupport(Context &context) const;
    virtual void initPrograms(SourceCollections &sourceCollections) const;
    virtual TestInstance *createInstance(Context &context) const;
};

ImageProcessingBoxFilterGraphicsTest::ImageProcessingBoxFilterGraphicsTest(
    TestContext &testCtx, const std::string &name, const TestParams &testParams,
    const BoxFilterSamplingTestParams &boxFilterTestParams)
    : ImageProcessingBoxFilterTest(testCtx, name, testParams, boxFilterTestParams)
{
}

ImageProcessingBoxFilterGraphicsTest::~ImageProcessingBoxFilterGraphicsTest(void)
{
}

void ImageProcessingBoxFilterGraphicsTest::checkSupport(Context &context) const
{
    const auto &vki           = context.getInstanceInterface();
    const auto physicalDevice = context.getPhysicalDevice();

    ImageProcessingBoxFilterTest::checkSupport(context);

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
                       "layout(set = 0, binding = 0) uniform highp texture2D sampledTexture;\n"
                       "layout(set = 0, binding = 1) uniform highp sampler textureSampler;\n"
                       "layout(set = 0, binding = 2) writeonly buffer outputValue {\n"
                       "  vec4 avg;\n"
                       "} sbOut;\n"
                       "layout(push_constant, std430) uniform PushConstants\n"
                       "{\n"
                       "    vec2 textureCoord;\n"
                       "    vec2 boxSize;\n"
                       "} pc;\n";
    return prog;
}

const std::string getProgMainBlock(const ImageProcOp op)
{
    std::string prog = "    // Compute\n"
                       "    vec4 boxfilterVal = " +
                       getImageProcGLSLStr(op) +
                       "(\n"
                       "        sampler2D(sampledTexture, textureSampler),\n"
                       "        pc.textureCoord,  \n"
                       "        pc.boxSize\n"
                       "    );\n"
                       "\n"
                       "    vec4 result = boxfilterVal;"
                       "\n"
                       "    if (result != vec4(0.0f, 0.0f, 0.0f, 0.0f))\n"
                       "        outColor = vec4(0.0f, 1.0f, 0.0f, 1.0f);\n" // green non-zero
                       "    else\n"
                       "        outColor = vec4(1.0f, 0.0f, 0.0f, 1.0f);\n" // red on zero
                       "    sbOut.avg = result;\n";                         // real value
    return prog;
}

void ImageProcessingBoxFilterGraphicsTest::initPrograms(SourceCollections &sourceCollections) const
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
                 << "    fragColor = inColor;\n"
                 << "}\n";
        }
    }
    sourceCollections.glslSources.add("frag") << glu::FragmentSource(frag.str()) << shaderBuildOpt;
}

class ImageProcessingBoxFilterTestInstance : public ImageProcessingTestInstance
{
public:
    ImageProcessingBoxFilterTestInstance(Context &context, const TestParams &testParams,
                                         const BoxFilterSamplingTestParams &boxFilterTestParams,
                                         const ImageType outImageType, const UVec2 outImageSize,
                                         const VkFormat outImageFormat);
    ~ImageProcessingBoxFilterTestInstance(void);

    virtual void addSupplementaryDescBindings(DescriptorSetLayoutExtBuilder &layoutBuilder);
    virtual void addSupplementaryDescTypes(DescriptorPoolBuilder &poolBuilder);
    virtual void writeSupplementaryDescriptors();
    void prepareDescriptors();
    void populateColorBuffer(const BufferWithMemory &colorBuffer, const UVec2 &imageSize, const VkFormat format);

    void prepareCommandBuffer();
    virtual void executeBarriers();
    virtual void executeBegin();
    virtual void executeBindPipeline();
    virtual void executeBindOtherBindings();
    virtual void executeProgram();
    virtual void executeEnd();
    void executeCommands(const VkPipelineLayout pipelineLayout, const BufferWithMemory &texColorBuffer,
                         const ImageWithMemory &texImage, const BufferWithMemory &resultBuffer,
                         const ImageWithMemory &resultImage);

    const Vec4 buildStandardResult(ImageProcessingResult &expectedResult, const BufferWithMemory &texColorBuffer);

protected:
    const BoxFilterSamplingTestParams m_boxFilterParams;

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
    de::MovePtr<BufferWithMemory> m_resultBuffer;
};

ImageProcessingBoxFilterTestInstance::ImageProcessingBoxFilterTestInstance(
    Context &context, const TestParams &testParams, const BoxFilterSamplingTestParams &boxFilterTestParams,
    const ImageType outImageType, const UVec2 outImageSize, const VkFormat outImageFormat)
    : ImageProcessingTestInstance(context, testParams)
    , m_boxFilterParams(boxFilterTestParams)
    , m_outImageType(outImageType)
    , m_outImageSize(outImageSize)
    , m_outImageFormat(outImageFormat)
{
}

ImageProcessingBoxFilterTestInstance::~ImageProcessingBoxFilterTestInstance(void)
{
}

void ImageProcessingBoxFilterTestInstance::addSupplementaryDescBindings(DescriptorSetLayoutExtBuilder &layoutBuilder)
{
    DE_UNREF(layoutBuilder);
}

void ImageProcessingBoxFilterTestInstance::addSupplementaryDescTypes(DescriptorPoolBuilder &poolBuilder)
{
    DE_UNREF(poolBuilder);
}

void ImageProcessingBoxFilterTestInstance::writeSupplementaryDescriptors()
{
}

void ImageProcessingBoxFilterTestInstance::prepareDescriptors()
{
    const auto &vkd   = m_context.getDeviceInterface();
    const auto device = m_context.getDevice();

    VkDescriptorPoolCreateFlags descPoolCreateFlags           = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    VkDescriptorSetLayoutCreateFlags descSetLayoutCreateFlags = 0u;
    VkDescriptorBindingFlags descBindingFlag                  = 0u;

    const auto descType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;

    if (m_params.updateAfterBind)
    {
        descPoolCreateFlags |= VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
        descSetLayoutCreateFlags |= VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
        descBindingFlag |= VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;
    }

    // Descriptor set layout
    DescriptorSetLayoutExtBuilder layoutBuilder;
    layoutBuilder.addSingleBinding(descType, m_params.stageMask);
    layoutBuilder.addSingleSamplerBinding(VK_DESCRIPTOR_TYPE_SAMPLER, m_params.stageMask, &m_sampledImageSampler.get());
    layoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, m_params.stageMask);
    addSupplementaryDescBindings(layoutBuilder);
    m_descriptorSetLayout = layoutBuilder.buildExt(vkd, device, descSetLayoutCreateFlags, descBindingFlag);

    // Descriptor pool
    DescriptorPoolBuilder poolBuilder;
    poolBuilder.addType(descType);
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
                                          descType, &texDescImageInfo);

    const VkDeviceSize resBuffSizeBytes            = sizeof(Vec4);
    const VkDescriptorBufferInfo resDescriptorInfo = makeDescriptorBufferInfo(**m_resultBuffer, 0ull, resBuffSizeBytes);
    m_descriptorUpdateBuilder.writeSingle(m_descriptorSet.get(), DescriptorSetUpdateBuilder::Location::binding(2u),
                                          VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &resDescriptorInfo);
    writeSupplementaryDescriptors();

    // Update descriptor set with the descriptor
    if (!m_params.updateAfterBind)
        m_descriptorUpdateBuilder.update(vkd, device);
}

void ImageProcessingBoxFilterTestInstance::populateColorBuffer(const BufferWithMemory &colorBuffer,
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
                float colorR = m_rnd.getFloat(minChannelValue, maxChannelValue);
                float colorG = m_rnd.getFloat(minChannelValue, maxChannelValue);
                float colorB = m_rnd.getFloat(minChannelValue, maxChannelValue);
                float colorA = m_rnd.getFloat(minChannelValue, maxChannelValue);
                const Vec4 randomColor(colorR, colorG, colorB, colorA);

                Vec4 color = (m_params.randomReference ? randomColor : uniformColor);

                colorBufferPixels.setPixel(color, x, y, z);
            }

    flushAlloc(vkd, device, colorBufferAlloc);
}

void ImageProcessingBoxFilterTestInstance::executeBarriers()
{
}

void ImageProcessingBoxFilterTestInstance::executeBegin()
{
}

void ImageProcessingBoxFilterTestInstance::executeBindPipeline()
{
}

void ImageProcessingBoxFilterTestInstance::executeBindOtherBindings()
{
}

void ImageProcessingBoxFilterTestInstance::executeProgram()
{
}

void ImageProcessingBoxFilterTestInstance::executeEnd()
{
}

void ImageProcessingBoxFilterTestInstance::prepareCommandBuffer()
{
    const auto &vkd       = m_context.getDeviceInterface();
    const auto device     = m_context.getDevice();
    const auto queueIndex = m_context.getUniversalQueueFamilyIndex();

    // Command pool and command buffer
    m_cmdPool   = createCommandPool(vkd, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueIndex);
    m_cmdBuffer = allocateCommandBuffer(vkd, device, m_cmdPool.get(), VK_COMMAND_BUFFER_LEVEL_PRIMARY);
}

void ImageProcessingBoxFilterTestInstance::executeCommands(const VkPipelineLayout pipelineLayout,
                                                           const BufferWithMemory &texColorBuffer,
                                                           const ImageWithMemory &texImage,
                                                           const BufferWithMemory &resultBuffer,
                                                           const ImageWithMemory &resultImage)
{
    const auto &vkd          = m_context.getDeviceInterface();
    const auto device        = m_context.getDevice();
    const auto queue         = m_context.getUniversalQueue();
    const bool isComputeTest = ((m_params.stageMask & VK_SHADER_STAGE_COMPUTE_BIT) == VK_SHADER_STAGE_COMPUTE_BIT);
    const auto cmdBuffer     = m_cmdBuffer.get();

    const VkImageSubresourceLayers layerSubresource = makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u);

    beginCommandBuffer(vkd, cmdBuffer);

    executeBarriers();

    // Copy color buffer to sampled image and change layout
    {
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
            const TestPushConstants pushConstants = {m_boxFilterParams.textureCoord, m_boxFilterParams.boxSize};
            vkd.cmdPushConstants(cmdBuffer, pipelineLayout, m_params.stageMask, 0u,
                                 static_cast<uint32_t>(sizeof(pushConstants)), &pushConstants);
        }

        executeProgram();
    }
    executeEnd();

    {
        const VkDeviceSize resBuffSizeBytes         = sizeof(Vec4);
        const VkBufferMemoryBarrier errWriteBarrier = makeBufferMemoryBarrier(
            VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT, **m_resultBuffer, 0ull, resBuffSizeBytes);

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

const Vec4 ImageProcessingBoxFilterTestInstance::buildStandardResult(ImageProcessingResult &expectedResult,
                                                                     const BufferWithMemory &texColorBuffer)
{
    const auto &vkd              = m_context.getDeviceInterface();
    const auto device            = m_context.getDevice();
    const uint32_t precisionBits = m_context.getDeviceProperties().limits.subTexelPrecisionBits;

    const IVec2 texRegionSize{static_cast<int>(m_params.sampledImageParams.imageSize.x()),
                              static_cast<int>(m_params.sampledImageParams.imageSize.y())};
    auto &texColorBufferAlloc = texColorBuffer.getAllocation();
    auto texColorBufferPtr =
        reinterpret_cast<char *>(texColorBufferAlloc.getHostPtr()) + texColorBufferAlloc.getOffset();
    const PixelBufferAccess texColorBufferPix{mapVkFormat(m_params.sampledImageParams.format), texRegionSize[0],
                                              texRegionSize[1], 1, texColorBufferPtr};

    const auto boxFilterValue = expectedResult.getBoxFilterSamplingResult(
        m_boxFilterParams.isUnnormCoord, texColorBufferPix, m_boxFilterParams.textureCoord, m_boxFilterParams.boxSize,
        precisionBits, m_params.sampledImageParams.components);

    flushAlloc(vkd, device, texColorBufferAlloc);

    return boxFilterValue;
}

class ImageProcessingBoxFilterGraphicsTestInstance : public ImageProcessingBoxFilterTestInstance
{
public:
    ImageProcessingBoxFilterGraphicsTestInstance(Context &context, const TestParams &params,
                                                 const BoxFilterSamplingTestParams &boxFilterTestParams,
                                                 const ImageType outImageType, const UVec2 outImageSize,
                                                 const VkFormat outImageFormat);
    ~ImageProcessingBoxFilterGraphicsTestInstance(void);

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

ImageProcessingBoxFilterGraphicsTestInstance::ImageProcessingBoxFilterGraphicsTestInstance(
    Context &context, const TestParams &testParams, const BoxFilterSamplingTestParams &boxFilterTestParams,
    const ImageType outImageType, const UVec2 outImageSize, const VkFormat outImageFormat)
    : ImageProcessingBoxFilterTestInstance(context, testParams, boxFilterTestParams, outImageType, outImageSize,
                                           outImageFormat)
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

ImageProcessingBoxFilterGraphicsTestInstance::~ImageProcessingBoxFilterGraphicsTestInstance(void)
{
}

void ImageProcessingBoxFilterGraphicsTestInstance::makeRenderPass()
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

void ImageProcessingBoxFilterGraphicsTestInstance::makeGraphicsPipeline(const PipelineLayoutWrapper &pipelineLayout,
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

void ImageProcessingBoxFilterGraphicsTestInstance::executeBarriers()
{
    const auto &vkd                = m_context.getDeviceInterface();
    const auto vertexBufferBarrier = makeBufferMemoryBarrier(
        VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT, m_vertexBuffer->get(), 0ull, m_vertexBufferSize);

    vkd.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, 0u, 0u,
                           nullptr, 1u, &vertexBufferBarrier, 0u, nullptr);
}

void ImageProcessingBoxFilterGraphicsTestInstance::executeBegin()
{
    const auto &vkd = m_context.getDeviceInterface();
    const Vec4 clearColor(tcu::RGBA::black().toVec());
    const VkExtent3D extent   = makeExtent3D(m_outImageSize.x(), m_outImageSize.y(), 1u);
    const VkRect2D renderArea = makeRect2D(extent.width, extent.height);

    m_renderPass.begin(vkd, *m_cmdBuffer, renderArea, clearColor);
}

void ImageProcessingBoxFilterGraphicsTestInstance::executeBindPipeline()
{
    m_graphicsPipeline.bind(*m_cmdBuffer);
}

void ImageProcessingBoxFilterGraphicsTestInstance::executeBindOtherBindings()
{
    const auto &vkd                       = m_context.getDeviceInterface();
    const VkDeviceSize vertexBufferOffset = 0ull;

    vkd.cmdBindVertexBuffers(*m_cmdBuffer, 0u, 1u, &m_vertexBuffer->get(), &vertexBufferOffset);
}

void ImageProcessingBoxFilterGraphicsTestInstance::executeProgram()
{
    const auto &vkd = m_context.getDeviceInterface();

    vkd.cmdDraw(*m_cmdBuffer, de::sizeU32(m_vertexData), 1u, 0u, 0u);
}

void ImageProcessingBoxFilterGraphicsTestInstance::executeEnd()
{
    const auto &vkd = m_context.getDeviceInterface();

    m_renderPass.end(vkd, *m_cmdBuffer);
}

TestStatus ImageProcessingBoxFilterGraphicsTestInstance::iterate(void)
{
    const auto &vkd   = m_context.getDeviceInterface();
    const auto device = m_context.getDevice();
    auto &allocator   = m_context.getDefaultAllocator();

    const auto texUsage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    const auto texImageViewType = mapImageViewType(m_params.sampledImageParams.imageType);
    const auto outImageUsage    = (VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
    const auto texTcuFormat     = mapVkFormat(m_params.sampledImageParams.format);

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

    // Create sampled image
    const VkImageCreateInfo sampledImageCreateInfo =
        makeImageCreateInfo(m_params.sampledImageParams.imageType, m_params.sampledImageParams.imageSize,
                            m_params.sampledImageParams.format, texUsage, 0u, m_params.sampledImageParams.tiling);

    const ImageWithMemory sampledImage{vkd, device, allocator, sampledImageCreateInfo, MemoryRequirement::Any};

    // Corresponding image views
    const auto colorSubresourceRange = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);

    m_sampledImageView =
        makeImageViewUtil(vkd, device, sampledImage.get(), texImageViewType, m_params.sampledImageParams.format,
                          colorSubresourceRange, m_params.sampledImageParams.components);

    // Create textures
    const VkDeviceSize texColorBufferSize = static_cast<VkDeviceSize>(
        static_cast<uint32_t>(getPixelSize(texTcuFormat)) * m_params.sampledImageParams.imageSize.x() *
        m_params.sampledImageParams.imageSize.y() * 1u);
    const auto texBufferInfo = makeBufferCreateInfo(texColorBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);

    const BufferWithMemory texColorBuffer{vkd, device, allocator, texBufferInfo, MemoryRequirement::HostVisible};

    // Fill sampled Image's color buffer
    populateColorBuffer(texColorBuffer, m_params.sampledImageParams.imageSize, m_params.sampledImageParams.format);

    // Prepare inputs and outputs
    const VkDeviceSize resBuffSizeBytes = sizeof(Vec4);
    m_resultBuffer                      = de::MovePtr<BufferWithMemory>(new BufferWithMemory(
        vkd, device, allocator, makeBufferCreateInfo(resBuffSizeBytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT),
        MemoryRequirement::HostVisible));

    m_sampledImageSampler = makeSampler(m_boxFilterParams.isUnnormCoord, m_params.sampledImageParams.addrMode,
                                        m_params.sampledImageParams.reductionMode);

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
    executeCommands(pipelineLayout.get(), texColorBuffer, sampledImage, resultBuffer, colorImage);

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
    const auto expectedBoxFilterSamplingValue = buildStandardResult(expectedResult, texColorBuffer);
    const Vec4 resultValue                    = *resBufferPtr;

    const Vec4 errorThreshold = Vec4(0.5f, 0.5f, 0.5f, 0.5f);
    return verifyResult(expectedBoxFilterSamplingValue, resultValue, expectedResult.getAccess(), resultPixels,
                        errorThreshold);
}

class ImageProcessingBoxFilterComputeTest : public ImageProcessingBoxFilterTest
{
public:
    ImageProcessingBoxFilterComputeTest(TestContext &testCtx, const std::string &name, const TestParams &testParams,
                                        const BoxFilterSamplingTestParams &boxFilterTestParams);
    virtual ~ImageProcessingBoxFilterComputeTest(void);

    virtual void checkSupport(Context &context) const;
    virtual void initPrograms(SourceCollections &sourceCollections) const;
    virtual TestInstance *createInstance(Context &context) const;
};

ImageProcessingBoxFilterComputeTest::ImageProcessingBoxFilterComputeTest(
    TestContext &testCtx, const std::string &name, const TestParams &testParams,
    const BoxFilterSamplingTestParams &boxFilterTestParams)
    : ImageProcessingBoxFilterTest(testCtx, name, testParams, boxFilterTestParams)
{
}

ImageProcessingBoxFilterComputeTest::~ImageProcessingBoxFilterComputeTest(void)
{
}

void ImageProcessingBoxFilterComputeTest::checkSupport(Context &context) const
{
    const auto &vki           = context.getInstanceInterface();
    const auto physicalDevice = context.getPhysicalDevice();

    ImageProcessingBoxFilterTest::checkSupport(context);

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

void ImageProcessingBoxFilterComputeTest::initPrograms(SourceCollections &sourceCollections) const
{
    const vk::ShaderBuildOptions shaderBuildOpt(sourceCollections.usedVulkanVersion, vk::SPIRV_VERSION_1_4, 0u, true);

    const std::string imageTypeStr =
        getFormatPrefix(mapVkFormat(m_outImageFormat)) + "image" + "2D"; // only 2D image support by box filtering

    std::ostringstream comp;
    {
        comp << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
             << "layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;\n"
             << getProgPreMain() << "\n"
             << "layout(set = 0, binding = 3) uniform writeonly " << imageTypeStr << " outputImage;\n"
             << "\n"
             << "void main() {\n"
             << "\n"
             << "    int gx = int(gl_GlobalInvocationID.x);\n"
             << "    int gy = int(gl_GlobalInvocationID.y);\n"
             << "    vec4 outColor = vec4(1.0f, 0.0f, 0.0f, 1.0f);" // red on zero
             << "\n"
             << getProgMainBlock(m_params.imageProcOp) << "    imageStore(outputImage, ivec2(gx, gy), outColor);\n"
             << "}\n";
    }
    sourceCollections.glslSources.add("comp") << glu::ComputeSource(comp.str()) << shaderBuildOpt;
}

class ImageProcessingBoxFilterComputeTestInstance : public ImageProcessingBoxFilterTestInstance
{
public:
    ImageProcessingBoxFilterComputeTestInstance(Context &context, const TestParams &params,
                                                const BoxFilterSamplingTestParams &boxFilterTestParams,
                                                const ImageType outImageType, const UVec2 outImageSize,
                                                const VkFormat outImageFormat);
    ~ImageProcessingBoxFilterComputeTestInstance(void);

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

ImageProcessingBoxFilterComputeTestInstance::ImageProcessingBoxFilterComputeTestInstance(
    Context &context, const TestParams &testParams, const BoxFilterSamplingTestParams &boxFilterTestParams,
    const ImageType outImageType, const UVec2 outImageSize, const VkFormat outImageFormat)
    : ImageProcessingBoxFilterTestInstance(context, testParams, boxFilterTestParams, outImageType, outImageSize,
                                           outImageFormat)
{
}

ImageProcessingBoxFilterComputeTestInstance::~ImageProcessingBoxFilterComputeTestInstance(void)
{
}

void ImageProcessingBoxFilterComputeTestInstance::addSupplementaryDescBindings(
    DescriptorSetLayoutExtBuilder &layoutBuilder)
{
    layoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, m_params.stageMask);
}

void ImageProcessingBoxFilterComputeTestInstance::addSupplementaryDescTypes(DescriptorPoolBuilder &poolBuilder)
{
    poolBuilder.addType(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
}

void ImageProcessingBoxFilterComputeTestInstance::writeSupplementaryDescriptors()
{
    const auto storeDescImageInfo =
        makeDescriptorImageInfo(VK_NULL_HANDLE, m_outImageView.get(), VK_IMAGE_LAYOUT_GENERAL);
    m_descriptorUpdateBuilder.writeSingle(m_descriptorSet.get(), DescriptorSetUpdateBuilder::Location::binding(3u),
                                          VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &storeDescImageInfo);
}

void ImageProcessingBoxFilterComputeTestInstance::executeBarriers()
{
    const auto &vkd                  = m_context.getDeviceInterface();
    const auto colorSubresourceRange = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
    const VkImageMemoryBarrier outImageBarrier =
        makeImageMemoryBarrier(0u, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
                               m_outImage->get(), colorSubresourceRange);

    vkd.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                           (VkDependencyFlags)0, 0, nullptr, 0, nullptr, 1, &outImageBarrier);
}

void ImageProcessingBoxFilterComputeTestInstance::executeBindPipeline()
{
    const auto &vkd = m_context.getDeviceInterface();

    vkd.cmdBindPipeline(*m_cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_computePipeline.get());
}

void ImageProcessingBoxFilterComputeTestInstance::executeProgram()
{
    const auto &vkd         = m_context.getDeviceInterface();
    const VkExtent3D extent = makeExtent3D(m_outImageSize.x(), m_outImageSize.y(), 1u);

    vkd.cmdDispatch(*m_cmdBuffer, extent.width, extent.height, extent.depth);
}

TestStatus ImageProcessingBoxFilterComputeTestInstance::iterate(void)
{
    const auto &vkd   = m_context.getDeviceInterface();
    const auto device = m_context.getDevice();
    auto &allocator   = m_context.getDefaultAllocator();

    const auto texUsage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    const auto texImageViewType = mapImageViewType(m_params.sampledImageParams.imageType);
    const auto outImageUsage    = (VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT);
    const auto texTcuFormat     = mapVkFormat(m_params.sampledImageParams.format);

    // Create sampled image
    const VkImageCreateInfo sampledImageCreateInfo =
        makeImageCreateInfo(m_params.sampledImageParams.imageType, m_params.sampledImageParams.imageSize,
                            m_params.sampledImageParams.format, texUsage, 0u, m_params.sampledImageParams.tiling);

    const ImageWithMemory sampledImage{vkd, device, allocator, sampledImageCreateInfo, MemoryRequirement::Any};

    // Corresponding image views
    const auto colorSubresourceRange = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);

    m_sampledImageView =
        makeImageViewUtil(vkd, device, sampledImage.get(), texImageViewType, m_params.sampledImageParams.format,
                          colorSubresourceRange, m_params.sampledImageParams.components);

    // Create textures
    const VkDeviceSize texColorBufferSize = static_cast<VkDeviceSize>(
        static_cast<uint32_t>(getPixelSize(texTcuFormat)) * m_params.sampledImageParams.imageSize.x() *
        m_params.sampledImageParams.imageSize.y() * 1u);
    const auto texBufferInfo = makeBufferCreateInfo(texColorBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);

    const BufferWithMemory texColorBuffer{vkd, device, allocator, texBufferInfo, MemoryRequirement::HostVisible};

    // Fill sampled Image's color buffer
    populateColorBuffer(texColorBuffer, m_params.sampledImageParams.imageSize, m_params.sampledImageParams.format);

    // Prepare inputs and outputs
    const VkDeviceSize resBuffSizeBytes = sizeof(Vec4);
    m_resultBuffer                      = de::MovePtr<BufferWithMemory>(new BufferWithMemory(
        vkd, device, allocator, makeBufferCreateInfo(resBuffSizeBytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT),
        MemoryRequirement::HostVisible));

    m_sampledImageSampler = makeSampler(m_boxFilterParams.isUnnormCoord, m_params.sampledImageParams.addrMode,
                                        m_params.sampledImageParams.reductionMode);

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
    executeCommands(pipelineLayout.get(), texColorBuffer, sampledImage, resultBuffer, *m_outImage);

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
    const auto expectedBoxFilterSamplingValue = buildStandardResult(expectedResult, texColorBuffer);
    const Vec4 resultError                    = *resBufferPtr;

    const Vec4 errorThreshold = Vec4(0.5f, 0.5f, 0.5f, 0.5f);
    return verifyResult(expectedBoxFilterSamplingValue, resultError, expectedResult.getAccess(), resultPixels,
                        errorThreshold);
}

TestInstance *ImageProcessingBoxFilterComputeTest::createInstance(Context &context) const
{
    return new ImageProcessingBoxFilterComputeTestInstance(context, m_params, m_boxFilterParams, m_outImageType,
                                                           m_outImageSize, m_outImageFormat);
}

TestInstance *ImageProcessingBoxFilterGraphicsTest::createInstance(Context &context) const
{
    return new ImageProcessingBoxFilterGraphicsTestInstance(context, m_params, m_boxFilterParams, m_outImageType,
                                                            m_outImageSize, m_outImageFormat);
}
struct CombinedTestParams
{
    TestParams testParams;
    BoxFilterSamplingTestParams boxFilterParams;
};

CombinedTestParams getCommonTestParams(const ImageProcOp op, const VkFormat format = VK_FORMAT_R8G8B8A8_UNORM,
                                       VkShaderStageFlags stageMask = VK_SHADER_STAGE_FRAGMENT_BIT,
                                       const bool isUnnormCoord     = true)
{
    CombinedTestParams combinedParams;

    // Sampled image parameters
    const TestImageParams defaultSampledImageParams = {
        IMAGE_TYPE_2D,                            // imageType
        UVec2(8u, 4u),                            // imageSize
        format,                                   // format
        VK_IMAGE_TILING_OPTIMAL,                  // tiling
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, // layout
        makeComponentMappingIdentity(),           // components
        VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,    // addrMode
        SAMPLER_REDUCTION_MODE_NONE               // reductionMode
    };

    const Vec2 texCoord = isUnnormCoord ? Vec2(4.125f, 2.625f) : Vec2(0.515f, 0.656f);

    const BoxFilterSamplingTestParams defaultBoxFilterParams = {
        isUnnormCoord,     // isUnnormCoord
        texCoord,          // textureCoord
        Vec2(2.75f, 2.25f) // boxSize
    };

    const TestParams defaultTestParams = {
        op,                                    // imageProcOp
        defaultSampledImageParams,             // sampledImageParams
        true,                                  // randomReference
        false,                                 // updateAfterBind
        PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC, // pipelineConstructionType
        stageMask                              // stageMask
    };

    combinedParams.boxFilterParams = defaultBoxFilterParams;
    combinedParams.testParams      = defaultTestParams;

    return combinedParams;
}

std::vector<CombinedTestParams> getBoxFilterTestParams(const ImageProcOp op,
                                                       const VkFormat format = VK_FORMAT_R8G8B8A8_UNORM)
{
    std::vector<CombinedTestParams> combinedParams;

    CombinedTestParams commonCombinedParams = getCommonTestParams(op, format);

    // image size 32x32
    const struct
    {
        Vec2 textureCoord;
        Vec2 boxSize;
    } boxFilterParams[] = {// tex coord is not center and box size is image size
                           {
                               Vec2(0.0f, 0.0f),  // textureCoord
                               Vec2(32.0f, 32.0f) // boxSize
                           },

                           // minimum box size
                           {
                               Vec2(16.0f, 16.0f), // textureCoord
                               Vec2(1.0f, 1.0f)    // boxSize
                           },

                           {
                               Vec2(16.0f, 16.0f), // textureCoord
                               Vec2(8.0f, 1.0f)    // boxSize
                           },

                           {
                               Vec2(16.0f, 16.0f),   // textureCoord
                               Vec2(31.111f, 1.999f) // boxSize
                           }};

    commonCombinedParams.testParams.sampledImageParams.imageSize = UVec2(32u, 32u);

    for (int idx = 0; idx < DE_LENGTH_OF_ARRAY(boxFilterParams); idx++)
    {
        CombinedTestParams params = commonCombinedParams;

        params.boxFilterParams.textureCoord = boxFilterParams[idx].textureCoord;
        params.boxFilterParams.boxSize      = boxFilterParams[idx].boxSize;

        combinedParams.push_back(params);
    }

    return combinedParams;
}

std::vector<CombinedTestParams> getSamplerAddressModeTestParams(const ImageProcOp op,
                                                                const VkSamplerAddressMode addrMode,
                                                                const VkFormat format)
{
    std::vector<CombinedTestParams> combinedParams;

    CombinedTestParams commonCombinedParams = getCommonTestParams(op, format);

    commonCombinedParams.testParams.sampledImageParams.addrMode = addrMode;

    // Only corner pixel included
    {
        CombinedTestParams params0 = commonCombinedParams;

        params0.testParams.sampledImageParams.imageSize = UVec2(32u, 32u);
        params0.boxFilterParams.textureCoord            = Vec2(31.0f, 31.0f);
        params0.boxFilterParams.boxSize                 = Vec2(8.0f, 8.0f);

        combinedParams.push_back(params0);
    }

    // Box is outside the sampled image
    {
        CombinedTestParams params1 = commonCombinedParams;

        params1.testParams.sampledImageParams.imageSize = UVec2(32u, 32u);
        params1.boxFilterParams.textureCoord            = Vec2(34.0f, 34.0f);
        params1.boxFilterParams.boxSize                 = Vec2(2.0f, 2.0f);

        combinedParams.push_back(params1);
    }

    return combinedParams;
}

} // namespace

TestCaseGroup *createImageProcessingBoxFilterSamplingCommonTests(
    TestContext &testCtx, const bool testCompute,
    const PipelineConstructionType pipelineConstructionType = PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC)
{
    de::MovePtr<TestCaseGroup> testGroup(new TestCaseGroup(testCtx, "box_filter_sampling"));

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

    const std::vector<VkFormat> &supportedFormats = getOpSupportedFormats(ImageProcOp::IMAGE_PROC_OP_BOX_FILTER);

    // Basic tests
    {
        de::MovePtr<TestCaseGroup> basicGroup(new TestCaseGroup(testCtx, "basic"));

        for (size_t imageFormatNdx = 0; imageFormatNdx < supportedFormats.size(); ++imageFormatNdx)
        {
            CombinedTestParams params =
                getCommonTestParams(ImageProcOp::IMAGE_PROC_OP_BOX_FILTER, supportedFormats[imageFormatNdx]);

            for (const auto &randomReference : {true, false})
            {
                params.testParams.randomReference          = randomReference;
                params.testParams.pipelineConstructionType = pipelineConstructionType;

                const auto testName =
                    getFormatShortString(supportedFormats[imageFormatNdx]) + (randomReference ? "_random" : "");

                if (!testCompute)
                    basicGroup->addChild(new ImageProcessingBoxFilterGraphicsTest(testCtx, testName, params.testParams,
                                                                                  params.boxFilterParams));
                else
                {
                    params.testParams.stageMask = VK_SHADER_STAGE_COMPUTE_BIT;

                    basicGroup->addChild(new ImageProcessingBoxFilterComputeTest(testCtx, testName, params.testParams,
                                                                                 params.boxFilterParams));
                }
            }
        }

        testGroup->addChild(basicGroup.release());
    }

    // Compute only has basic tests
    if (!testCompute && (pipelineConstructionType == PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC))
    {
        // Different texture coordinates and box sizes
        {
            de::MovePtr<TestCaseGroup> boxFilterParamsGroup(new TestCaseGroup(testCtx, "box_filter_params"));

            const std::vector<CombinedTestParams> &boxFilterTestParamsList =
                getBoxFilterTestParams(ImageProcOp::IMAGE_PROC_OP_BOX_FILTER);

            for (size_t paramNdx = 0; paramNdx < boxFilterTestParamsList.size(); ++paramNdx)
            {
                CombinedTestParams params = boxFilterTestParamsList[paramNdx];

                const auto testName = "params" + de::toString(paramNdx);

                boxFilterParamsGroup->addChild(new ImageProcessingBoxFilterGraphicsTest(
                    testCtx, testName, params.testParams, params.boxFilterParams));
            }

            testGroup->addChild(boxFilterParamsGroup.release());
        }

        // Sampler address modes
        {
            de::MovePtr<TestCaseGroup> addrModesGroup(new TestCaseGroup(testCtx, "address_modes"));

            for (int addrModeNdx = 0; addrModeNdx < DE_LENGTH_OF_ARRAY(addressModes); ++addrModeNdx)
            {
                for (size_t imageFormatNdx = 0; imageFormatNdx < supportedFormats.size(); ++imageFormatNdx)
                {
                    const std::vector<CombinedTestParams> &addrModeTestParamsList = getSamplerAddressModeTestParams(
                        ImageProcOp::IMAGE_PROC_OP_BOX_FILTER, addressModes[addrModeNdx].addrMode,
                        supportedFormats[imageFormatNdx]);

                    for (size_t paramNdx = 0; paramNdx < addrModeTestParamsList.size(); ++paramNdx)
                    {
                        const CombinedTestParams &params = addrModeTestParamsList[paramNdx];

                        const std::string paramsName = "_params" + de::toString(paramNdx);
                        const auto testName          = addressModes[addrModeNdx].addrModeName + paramsName + "_" +
                                              getFormatShortString(supportedFormats[imageFormatNdx]);

                        addrModesGroup->addChild(new ImageProcessingBoxFilterGraphicsTest(
                            testCtx, testName, params.testParams, params.boxFilterParams));
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
                    CombinedTestParams params =
                        getCommonTestParams(ImageProcOp::IMAGE_PROC_OP_BOX_FILTER, supportedFormats[imageFormatNdx]);

                    params.testParams.sampledImageParams.reductionMode = reductionModes[redModeNdx].reductionMode;

                    const auto testName = reductionModes[redModeNdx].reductionModeName + "_" +
                                          getFormatShortString(supportedFormats[imageFormatNdx]);

                    reductionModesGroup->addChild(new ImageProcessingBoxFilterGraphicsTest(
                        testCtx, testName, params.testParams, params.boxFilterParams));
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
                    CombinedTestParams params =
                        getCommonTestParams(ImageProcOp::IMAGE_PROC_OP_BOX_FILTER, supportedFormats[imageFormatNdx]);

                    params.testParams.sampledImageParams.tiling = tilingTypes[tilingNdx].tiling;

                    const auto testName = tilingTypes[tilingNdx].tilingName + "_" +
                                          getFormatShortString(supportedFormats[imageFormatNdx]);

                    tilingGroup->addChild(new ImageProcessingBoxFilterGraphicsTest(testCtx, testName, params.testParams,
                                                                                   params.boxFilterParams));
                }
            }

            testGroup->addChild(tilingGroup.release());
        }

        // Swizzles for sampled image
        {
            de::MovePtr<TestCaseGroup> swizzleGroup(new TestCaseGroup(testCtx, "swizzles"));

            for (int swizzleNdx = 0; swizzleNdx < DE_LENGTH_OF_ARRAY(swizzles); ++swizzleNdx)
            {
                CombinedTestParams params = getCommonTestParams(ImageProcOp::IMAGE_PROC_OP_BOX_FILTER);
                params.testParams.sampledImageParams.components = swizzles[swizzleNdx].components;

                const auto testName = swizzles[swizzleNdx].compMappingName;

                swizzleGroup->addChild(new ImageProcessingBoxFilterGraphicsTest(testCtx, testName, params.testParams,
                                                                                params.boxFilterParams));
            }

            testGroup->addChild(swizzleGroup.release());
        }

        // Image layouts
        {
            de::MovePtr<TestCaseGroup> layoutGroup(new TestCaseGroup(testCtx, "layouts"));

            for (int layoutNdx = 0; layoutNdx < DE_LENGTH_OF_ARRAY(layouts); ++layoutNdx)
            {
                if (layouts[layoutNdx].layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
                    continue;

                for (size_t imageFormatNdx = 0; imageFormatNdx < supportedFormats.size(); ++imageFormatNdx)
                {
                    CombinedTestParams params =
                        getCommonTestParams(ImageProcOp::IMAGE_PROC_OP_BOX_FILTER, supportedFormats[imageFormatNdx]);

                    params.testParams.sampledImageParams.layout = layouts[layoutNdx].layout;

                    const auto testName =
                        layouts[layoutNdx].layoutName + "_" + getFormatShortString(supportedFormats[imageFormatNdx]);

                    layoutGroup->addChild(new ImageProcessingBoxFilterGraphicsTest(testCtx, testName, params.testParams,
                                                                                   params.boxFilterParams));
                }
            }

            testGroup->addChild(layoutGroup.release());
        }

        // Box filtering used in other shader stages
        {
            // Testing with fixed format having all components, fixed address mode: VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE and no sampler reduction mode, optimal tiling and swizzle
            de::MovePtr<TestCaseGroup> stageGroup(new TestCaseGroup(testCtx, "shader_stages"));

            for (int stageNdx = 0; stageNdx < DE_LENGTH_OF_ARRAY(shaderStages); ++stageNdx)
            {
                CombinedTestParams params   = getCommonTestParams(ImageProcOp::IMAGE_PROC_OP_BOX_FILTER);
                params.testParams.stageMask = shaderStages[stageNdx];

                const auto testName = getStageNames(shaderStages[stageNdx]);

                stageGroup->addChild(new ImageProcessingBoxFilterGraphicsTest(testCtx, testName, params.testParams,
                                                                              params.boxFilterParams));
            }

            testGroup->addChild(stageGroup.release());
        }

        // Descriptor tests
        {
            de::MovePtr<TestCaseGroup> descGroup(new TestCaseGroup(testCtx, "descriptors"));

            // update-after-bind
            {
                CombinedTestParams params         = getCommonTestParams(ImageProcOp::IMAGE_PROC_OP_BOX_FILTER);
                params.testParams.updateAfterBind = true;

                for (const auto &randomReference : {true, false})
                {
                    params.testParams.randomReference = randomReference;

                    const auto testName = std::string("updateAfterBind") + (randomReference ? "_random" : "");

                    descGroup->addChild(new ImageProcessingBoxFilterGraphicsTest(testCtx, testName, params.testParams,
                                                                                 params.boxFilterParams));
                }
            }

            testGroup->addChild(descGroup.release());
        }

        // Normalized coordinates
        {
            de::MovePtr<TestCaseGroup> coordsGroup(new TestCaseGroup(testCtx, "normalized_coords"));

            for (size_t imageFormatNdx = 0; imageFormatNdx < supportedFormats.size(); ++imageFormatNdx)
            {
                CombinedTestParams params =
                    getCommonTestParams(ImageProcOp::IMAGE_PROC_OP_BOX_FILTER, supportedFormats[imageFormatNdx],
                                        VK_SHADER_STAGE_FRAGMENT_BIT, false /* normalized coordinates */);

                const auto testName = getFormatShortString(supportedFormats[imageFormatNdx]);

                coordsGroup->addChild(new ImageProcessingBoxFilterGraphicsTest(testCtx, testName, params.testParams,
                                                                               params.boxFilterParams));
            }

            testGroup->addChild(coordsGroup.release());
        }
    }

    return testGroup.release();
}

TestCaseGroup *createImageProcessingBoxFilterSamplingGraphicsTests(
    TestContext &testCtx, const PipelineConstructionType pipelineConstructionType)
{
    return createImageProcessingBoxFilterSamplingCommonTests(testCtx, false, pipelineConstructionType);
}

TestCaseGroup *createImageProcessingBoxFilterSamplingComputeTests(TestContext &testCtx)
{
    return createImageProcessingBoxFilterSamplingCommonTests(testCtx, true);
}

} // namespace ImageProcessing
} // namespace vkt
