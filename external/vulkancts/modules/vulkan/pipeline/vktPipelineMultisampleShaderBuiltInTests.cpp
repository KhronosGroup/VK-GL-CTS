/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2016 The Khronos Group Inc.
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
 *//*
* \file vktPipelineMultisampleShaderBuiltInTests.cpp
* \brief Multisample Shader BuiltIn Tests
*//*--------------------------------------------------------------------*/

#include "vktPipelineMultisampleShaderBuiltInTests.hpp"
#include "vktPipelineMultisampleBaseResolveAndPerSampleFetch.hpp"
#include "vktPipelineMakeUtil.hpp"

#include "vkBuilderUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkImageWithMemory.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkBarrierUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkTypeUtil.hpp"

#include "tcuVectorUtil.hpp"
#include "tcuTestLog.hpp"

#include <set>

using std::set;

namespace vkt
{
namespace pipeline
{
namespace multisample
{

using namespace vk;

struct VertexDataNdc
{
    VertexDataNdc(const tcu::Vec4 &posNdc) : positionNdc(posNdc)
    {
    }

    tcu::Vec4 positionNdc;
};

MultisampleInstanceBase::VertexDataDesc getVertexDataDescriptonNdc(void)
{
    MultisampleInstanceBase::VertexDataDesc vertexDataDesc;

    vertexDataDesc.verticesCount     = 4u;
    vertexDataDesc.dataStride        = sizeof(VertexDataNdc);
    vertexDataDesc.dataSize          = vertexDataDesc.verticesCount * vertexDataDesc.dataStride;
    vertexDataDesc.primitiveTopology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;

    const VkVertexInputAttributeDescription vertexAttribPositionNdc = {
        0u,                                   // uint32_t location;
        0u,                                   // uint32_t binding;
        VK_FORMAT_R32G32B32A32_SFLOAT,        // VkFormat format;
        offsetof(VertexDataNdc, positionNdc), // uint32_t offset;
    };

    vertexDataDesc.vertexAttribDescVec.push_back(vertexAttribPositionNdc);

    return vertexDataDesc;
}

void uploadVertexDataNdc(const Allocation &vertexBufferAllocation,
                         const MultisampleInstanceBase::VertexDataDesc &vertexDataDescripton)
{
    std::vector<VertexDataNdc> vertices;

    vertices.push_back(VertexDataNdc(tcu::Vec4(-1.0f, -1.0f, 0.0f, 1.0f)));
    vertices.push_back(VertexDataNdc(tcu::Vec4(1.0f, -1.0f, 0.0f, 1.0f)));
    vertices.push_back(VertexDataNdc(tcu::Vec4(-1.0f, 1.0f, 0.0f, 1.0f)));
    vertices.push_back(VertexDataNdc(tcu::Vec4(1.0f, 1.0f, 0.0f, 1.0f)));

    deMemcpy(vertexBufferAllocation.getHostPtr(), dataPointer(vertices),
             static_cast<std::size_t>(vertexDataDescripton.dataSize));
}

struct VertexDataNdcScreen
{
    VertexDataNdcScreen(const tcu::Vec4 &posNdc, const tcu::Vec2 &posScreen)
        : positionNdc(posNdc)
        , positionScreen(posScreen)
    {
    }

    tcu::Vec4 positionNdc;
    tcu::Vec2 positionScreen;
};

MultisampleInstanceBase::VertexDataDesc getVertexDataDescriptonNdcScreen(void)
{
    MultisampleInstanceBase::VertexDataDesc vertexDataDesc;

    vertexDataDesc.verticesCount     = 4u;
    vertexDataDesc.dataStride        = sizeof(VertexDataNdcScreen);
    vertexDataDesc.dataSize          = vertexDataDesc.verticesCount * vertexDataDesc.dataStride;
    vertexDataDesc.primitiveTopology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;

    const VkVertexInputAttributeDescription vertexAttribPositionNdc = {
        0u,                                         // uint32_t location;
        0u,                                         // uint32_t binding;
        VK_FORMAT_R32G32B32A32_SFLOAT,              // VkFormat format;
        offsetof(VertexDataNdcScreen, positionNdc), // uint32_t offset;
    };

    vertexDataDesc.vertexAttribDescVec.push_back(vertexAttribPositionNdc);

    const VkVertexInputAttributeDescription vertexAttribPositionScreen = {
        1u,                                            // uint32_t location;
        0u,                                            // uint32_t binding;
        VK_FORMAT_R32G32_SFLOAT,                       // VkFormat format;
        offsetof(VertexDataNdcScreen, positionScreen), // uint32_t offset;
    };

    vertexDataDesc.vertexAttribDescVec.push_back(vertexAttribPositionScreen);

    return vertexDataDesc;
}

void uploadVertexDataNdcScreen(const Allocation &vertexBufferAllocation,
                               const MultisampleInstanceBase::VertexDataDesc &vertexDataDescripton,
                               const tcu::Vec2 &screenSize)
{
    std::vector<VertexDataNdcScreen> vertices;

    vertices.push_back(VertexDataNdcScreen(tcu::Vec4(-1.0f, -1.0f, 0.0f, 1.0f), tcu::Vec2(0.0f, 0.0f)));
    vertices.push_back(VertexDataNdcScreen(tcu::Vec4(1.0f, -1.0f, 0.0f, 1.0f), tcu::Vec2(screenSize.x(), 0.0f)));
    vertices.push_back(VertexDataNdcScreen(tcu::Vec4(-1.0f, 1.0f, 0.0f, 1.0f), tcu::Vec2(0.0f, screenSize.y())));
    vertices.push_back(
        VertexDataNdcScreen(tcu::Vec4(1.0f, 1.0f, 0.0f, 1.0f), tcu::Vec2(screenSize.x(), screenSize.y())));

    deMemcpy(vertexBufferAllocation.getHostPtr(), dataPointer(vertices),
             static_cast<std::size_t>(vertexDataDescripton.dataSize));
}

bool checkForErrorMS(const vk::VkImageCreateInfo &imageMSInfo,
                     const std::vector<tcu::ConstPixelBufferAccess> &dataPerSample, const uint32_t errorCompNdx)
{
    const uint32_t numSamples = static_cast<uint32_t>(imageMSInfo.samples);

    for (uint32_t z = 0u; z < imageMSInfo.extent.depth; ++z)
        for (uint32_t y = 0u; y < imageMSInfo.extent.height; ++y)
            for (uint32_t x = 0u; x < imageMSInfo.extent.width; ++x)
            {
                for (uint32_t sampleNdx = 0u; sampleNdx < numSamples; ++sampleNdx)
                {
                    const uint32_t errorComponent = dataPerSample[sampleNdx].getPixelUint(x, y, z)[errorCompNdx];

                    if (errorComponent > 0)
                        return true;
                }
            }

    return false;
}

bool checkForErrorRS(const vk::VkImageCreateInfo &imageRSInfo, const tcu::ConstPixelBufferAccess &dataRS,
                     const uint32_t errorCompNdx)
{
    for (uint32_t z = 0u; z < imageRSInfo.extent.depth; ++z)
        for (uint32_t y = 0u; y < imageRSInfo.extent.height; ++y)
            for (uint32_t x = 0u; x < imageRSInfo.extent.width; ++x)
            {
                const uint32_t errorComponent = dataRS.getPixelUint(x, y, z)[errorCompNdx];

                if (errorComponent > 0)
                    return true;
            }

    return false;
}

template <typename CaseClassName>
class MSCase : public MSCaseBaseResolveAndPerSampleFetch
{
public:
    MSCase(tcu::TestContext &testCtx, const std::string &name, const ImageMSParams &imageMSParams)
        : MSCaseBaseResolveAndPerSampleFetch(testCtx, name, imageMSParams)
    {
    }

    virtual void checkSupport(Context &) const
    {
    }
    void init(void);
    void initPrograms(vk::SourceCollections &programCollection) const;
    TestInstance *createInstance(Context &context) const;
    static MultisampleCaseBase *createCase(tcu::TestContext &testCtx, const std::string &name,
                                           const ImageMSParams &imageMSParams);
};

template <typename CaseClassName>
MultisampleCaseBase *MSCase<CaseClassName>::createCase(tcu::TestContext &testCtx, const std::string &name,
                                                       const ImageMSParams &imageMSParams)
{
    return new MSCase<CaseClassName>(testCtx, name, imageMSParams);
}

template <typename InstanceClassName>
class MSInstance : public MSInstanceBaseResolveAndPerSampleFetch
{
public:
    MSInstance(Context &context, const ImageMSParams &imageMSParams)
        : MSInstanceBaseResolveAndPerSampleFetch(context, imageMSParams)
    {
    }

    VertexDataDesc getVertexDataDescripton(void) const;
    void uploadVertexData(const Allocation &vertexBufferAllocation, const VertexDataDesc &vertexDataDescripton) const;

    tcu::TestStatus verifyImageData(const vk::VkImageCreateInfo &imageMSInfo, const vk::VkImageCreateInfo &imageRSInfo,
                                    const std::vector<tcu::ConstPixelBufferAccess> &dataPerSample,
                                    const tcu::ConstPixelBufferAccess &dataRS) const;

    virtual VkPipelineMultisampleStateCreateInfo getMSStateCreateInfo(const ImageMSParams &imageMSParams) const
    {
        return MSInstanceBaseResolveAndPerSampleFetch::getMSStateCreateInfo(imageMSParams);
    }
};

class MSInstanceSampleID;

template <>
MultisampleInstanceBase::VertexDataDesc MSInstance<MSInstanceSampleID>::getVertexDataDescripton(void) const
{
    return getVertexDataDescriptonNdc();
}

template <>
void MSInstance<MSInstanceSampleID>::uploadVertexData(const Allocation &vertexBufferAllocation,
                                                      const VertexDataDesc &vertexDataDescripton) const
{
    uploadVertexDataNdc(vertexBufferAllocation, vertexDataDescripton);
}

template <>
tcu::TestStatus MSInstance<MSInstanceSampleID>::verifyImageData(
    const vk::VkImageCreateInfo &imageMSInfo, const vk::VkImageCreateInfo &imageRSInfo,
    const std::vector<tcu::ConstPixelBufferAccess> &dataPerSample, const tcu::ConstPixelBufferAccess &dataRS) const
{
    DE_UNREF(imageRSInfo);
    DE_UNREF(dataRS);

    const uint32_t numSamples = static_cast<uint32_t>(imageMSInfo.samples);

    for (uint32_t sampleNdx = 0u; sampleNdx < numSamples; ++sampleNdx)
    {
        for (uint32_t z = 0u; z < imageMSInfo.extent.depth; ++z)
            for (uint32_t y = 0u; y < imageMSInfo.extent.height; ++y)
                for (uint32_t x = 0u; x < imageMSInfo.extent.width; ++x)
                {
                    const uint32_t sampleID = dataPerSample[sampleNdx].getPixelUint(x, y, z).x();

                    if (sampleID != sampleNdx)
                        return tcu::TestStatus::fail("gl_SampleID does not have correct value");
                }
    }

    return tcu::TestStatus::pass("Passed");
}

class MSCaseSampleID;

template <>
void MSCase<MSCaseSampleID>::checkSupport(Context &context) const
{
    context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_SAMPLE_RATE_SHADING);
}

template <>
void MSCase<MSCaseSampleID>::init(void)
{
    m_testCtx.getLog() << tcu::TestLog::Message
                       << "Writing gl_SampleID to the red channel of the texture and verifying texture values.\n"
                       << "Expecting value N at sample index N of a multisample texture.\n"
                       << tcu::TestLog::EndMessage;

    MultisampleCaseBase::init();
}

template <>
void MSCase<MSCaseSampleID>::initPrograms(vk::SourceCollections &programCollection) const
{
    MSCaseBaseResolveAndPerSampleFetch::initPrograms(programCollection);

    // Create vertex shader
    std::ostringstream vs;

    vs << "#version 440\n"
       << "layout(location = 0) in vec4 vs_in_position_ndc;\n"
       << "\n"
       << "out gl_PerVertex {\n"
       << "    vec4  gl_Position;\n"
       << "};\n"
       << "void main (void)\n"
       << "{\n"
       << "    gl_Position = vs_in_position_ndc;\n"
       << "}\n";

    programCollection.glslSources.add("vertex_shader") << glu::VertexSource(vs.str());

    // Create fragment shader
    std::ostringstream fs;

    fs << "#version 440\n"
       << "\n"
       << "layout(location = 0) out vec4 fs_out_color;\n"
       << "\n"
       << "void main (void)\n"
       << "{\n"
       << "    fs_out_color = vec4(float(gl_SampleID) / float(255), 0.0, 0.0, 1.0);\n"
       << "}\n";

    programCollection.glslSources.add("fragment_shader") << glu::FragmentSource(fs.str());
}

template <>
TestInstance *MSCase<MSCaseSampleID>::createInstance(Context &context) const
{
    return new MSInstance<MSInstanceSampleID>(context, m_imageMSParams);
}

class MSInstanceSamplePosDistribution;

template <>
MultisampleInstanceBase::VertexDataDesc MSInstance<MSInstanceSamplePosDistribution>::getVertexDataDescripton(void) const
{
    return getVertexDataDescriptonNdc();
}

template <>
void MSInstance<MSInstanceSamplePosDistribution>::uploadVertexData(const Allocation &vertexBufferAllocation,
                                                                   const VertexDataDesc &vertexDataDescripton) const
{
    uploadVertexDataNdc(vertexBufferAllocation, vertexDataDescripton);
}

template <>
tcu::TestStatus MSInstance<MSInstanceSamplePosDistribution>::verifyImageData(
    const vk::VkImageCreateInfo &imageMSInfo, const vk::VkImageCreateInfo &imageRSInfo,
    const std::vector<tcu::ConstPixelBufferAccess> &dataPerSample, const tcu::ConstPixelBufferAccess &dataRS) const
{
    const uint32_t numSamples = static_cast<uint32_t>(imageMSInfo.samples);

    // approximate Bates distribution as normal
    const float variance          = (1.0f / (12.0f * (float)numSamples));
    const float standardDeviation = deFloatSqrt(variance);

    // 95% of means of sample positions are within 2 standard deviations if
    // they were randomly assigned. Sample patterns are expected to be more
    // uniform than a random pattern.
    const float distanceThreshold = 2.0f * standardDeviation;

    for (uint32_t z = 0u; z < imageRSInfo.extent.depth; ++z)
        for (uint32_t y = 0u; y < imageRSInfo.extent.height; ++y)
            for (uint32_t x = 0u; x < imageRSInfo.extent.width; ++x)
            {
                const uint32_t errorComponent = dataRS.getPixelUint(x, y, z).z();

                if (errorComponent > 0)
                    return tcu::TestStatus::fail("gl_SamplePosition is not within interval [0,1]");

                if (numSamples >= VK_SAMPLE_COUNT_4_BIT)
                {
                    const tcu::Vec2 averageSamplePos   = tcu::Vec2((float)dataRS.getPixelUint(x, y, z).x() / 255.0f,
                                                                   (float)dataRS.getPixelUint(x, y, z).y() / 255.0f);
                    const tcu::Vec2 distanceFromCenter = tcu::abs(averageSamplePos - tcu::Vec2(0.5f, 0.5f));

                    if (distanceFromCenter.x() > distanceThreshold || distanceFromCenter.y() > distanceThreshold)
                        return tcu::TestStatus::fail("Sample positions are not uniformly distributed within the pixel");
                }
            }

    for (uint32_t z = 0u; z < imageMSInfo.extent.depth; ++z)
        for (uint32_t y = 0u; y < imageMSInfo.extent.height; ++y)
            for (uint32_t x = 0u; x < imageMSInfo.extent.width; ++x)
            {
                std::vector<tcu::Vec2> samplePositions(numSamples);

                for (uint32_t sampleNdx = 0u; sampleNdx < numSamples; ++sampleNdx)
                {
                    const uint32_t errorComponent = dataPerSample[sampleNdx].getPixelUint(x, y, z).z();

                    if (errorComponent > 0)
                        return tcu::TestStatus::fail("gl_SamplePosition is not within interval [0,1]");

                    samplePositions[sampleNdx] =
                        tcu::Vec2((float)dataPerSample[sampleNdx].getPixelUint(x, y, z).x() / 255.0f,
                                  (float)dataPerSample[sampleNdx].getPixelUint(x, y, z).y() / 255.0f);
                }

                for (uint32_t sampleNdxA = 0u; sampleNdxA < numSamples; ++sampleNdxA)
                    for (uint32_t sampleNdxB = sampleNdxA + 1u; sampleNdxB < numSamples; ++sampleNdxB)
                    {
                        if (samplePositions[sampleNdxA] == samplePositions[sampleNdxB])
                            return tcu::TestStatus::fail("Two samples have the same position");
                    }

                if (numSamples >= VK_SAMPLE_COUNT_4_BIT)
                {
                    tcu::Vec2 averageSamplePos(0.0f, 0.0f);

                    for (uint32_t sampleNdx = 0u; sampleNdx < numSamples; ++sampleNdx)
                    {
                        averageSamplePos.x() += samplePositions[sampleNdx].x();
                        averageSamplePos.y() += samplePositions[sampleNdx].y();
                    }

                    averageSamplePos.x() /= (float)numSamples;
                    averageSamplePos.y() /= (float)numSamples;

                    const tcu::Vec2 distanceFromCenter = tcu::abs(averageSamplePos - tcu::Vec2(0.5f, 0.5f));

                    if (distanceFromCenter.x() > distanceThreshold || distanceFromCenter.y() > distanceThreshold)
                        return tcu::TestStatus::fail("Sample positions are not uniformly distributed within the pixel");
                }
            }

    return tcu::TestStatus::pass("Passed");
}

class MSCaseSamplePosDistribution;

template <>
void MSCase<MSCaseSamplePosDistribution>::checkSupport(Context &context) const
{
    context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_SAMPLE_RATE_SHADING);
}

template <>
void MSCase<MSCaseSamplePosDistribution>::init(void)
{
    m_testCtx.getLog() << tcu::TestLog::Message << "Verifying gl_SamplePosition value with multisample targets:\n"
                       << "    a) Expect legal sample position.\n"
                       << "    b) Sample position is unique within the set of all sample positions of a pixel.\n"
                       << "    c) Sample position distribution is uniform or almost uniform.\n"
                       << tcu::TestLog::EndMessage;

    MultisampleCaseBase::init();
}

template <>
void MSCase<MSCaseSamplePosDistribution>::initPrograms(vk::SourceCollections &programCollection) const
{
    MSCaseBaseResolveAndPerSampleFetch::initPrograms(programCollection);

    // Create vertex shader
    std::ostringstream vs;

    vs << "#version 440\n"
       << "layout(location = 0) in vec4 vs_in_position_ndc;\n"
       << "\n"
       << "out gl_PerVertex {\n"
       << "    vec4  gl_Position;\n"
       << "};\n"
       << "void main (void)\n"
       << "{\n"
       << "    gl_Position = vs_in_position_ndc;\n"
       << "}\n";

    programCollection.glslSources.add("vertex_shader") << glu::VertexSource(vs.str());

    // Create fragment shader
    std::ostringstream fs;

    fs << "#version 440\n"
       << "\n"
       << "layout(location = 0) out vec4 fs_out_color;\n"
       << "\n"
       << "void main (void)\n"
       << "{\n"
       << "    if (gl_SamplePosition.x < 0.0 || gl_SamplePosition.x > 1.0 || gl_SamplePosition.y < 0.0 || "
          "gl_SamplePosition.y > 1.0)\n"
          "        fs_out_color = vec4(0.0, 0.0, 1.0, 1.0);\n"
          "    else\n"
          "        fs_out_color = vec4(gl_SamplePosition.x, gl_SamplePosition.y, 0.0, 1.0);\n"
          "}\n";

    programCollection.glslSources.add("fragment_shader") << glu::FragmentSource(fs.str());
}

template <>
TestInstance *MSCase<MSCaseSamplePosDistribution>::createInstance(Context &context) const
{
    return new MSInstance<MSInstanceSamplePosDistribution>(context, m_imageMSParams);
}

class MSInstanceSamplePosCorrectness;

template <>
MultisampleInstanceBase::VertexDataDesc MSInstance<MSInstanceSamplePosCorrectness>::getVertexDataDescripton(void) const
{
    return getVertexDataDescriptonNdcScreen();
}

template <>
void MSInstance<MSInstanceSamplePosCorrectness>::uploadVertexData(const Allocation &vertexBufferAllocation,
                                                                  const VertexDataDesc &vertexDataDescripton) const
{
    const tcu::UVec3 layerSize = getLayerSize(IMAGE_TYPE_2D, m_imageMSParams.imageSize);

    uploadVertexDataNdcScreen(vertexBufferAllocation, vertexDataDescripton,
                              tcu::Vec2(static_cast<float>(layerSize.x()), static_cast<float>(layerSize.y())));
}

template <>
tcu::TestStatus MSInstance<MSInstanceSamplePosCorrectness>::verifyImageData(
    const vk::VkImageCreateInfo &imageMSInfo, const vk::VkImageCreateInfo &imageRSInfo,
    const std::vector<tcu::ConstPixelBufferAccess> &dataPerSample, const tcu::ConstPixelBufferAccess &dataRS) const
{
    if (checkForErrorMS(imageMSInfo, dataPerSample, 0))
        return tcu::TestStatus::fail("Varying values are not sampled at gl_SamplePosition");

    if (checkForErrorRS(imageRSInfo, dataRS, 0))
        return tcu::TestStatus::fail("Varying values are not sampled at gl_SamplePosition");

    return tcu::TestStatus::pass("Passed");
}

class MSCaseSamplePosCorrectness;

template <>
void MSCase<MSCaseSamplePosCorrectness>::checkSupport(Context &context) const
{
    context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_SAMPLE_RATE_SHADING);
}

template <>
void MSCase<MSCaseSamplePosCorrectness>::init(void)
{
    m_testCtx.getLog() << tcu::TestLog::Message << "Verifying gl_SamplePosition correctness:\n"
                       << "    1) Varying values should be sampled at the sample position.\n"
                       << " => fract(position_screen) == gl_SamplePosition\n"
                       << tcu::TestLog::EndMessage;

    MultisampleCaseBase::init();
}

template <>
void MSCase<MSCaseSamplePosCorrectness>::initPrograms(vk::SourceCollections &programCollection) const
{
    MSCaseBaseResolveAndPerSampleFetch::initPrograms(programCollection);

    // Create vertex shaders
    std::ostringstream vs;

    vs << "#version 440\n"
       << "layout(location = 0) in vec4 vs_in_position_ndc;\n"
       << "layout(location = 1) in vec2 vs_in_position_screen;\n"
       << "\n"
       << "layout(location = 0) sample out vec2 vs_out_position_screen;\n"
       << "\n"
       << "out gl_PerVertex {\n"
       << "    vec4  gl_Position;\n"
       << "};\n"
       << "void main (void)\n"
       << "{\n"
       << "    gl_Position = vs_in_position_ndc;\n"
       << "    vs_out_position_screen = vs_in_position_screen;\n"
       << "}\n";

    programCollection.glslSources.add("vertex_shader") << glu::VertexSource(vs.str());

    // Create fragment shader
    std::ostringstream fs;

    fs << "#version 440\n"
       << "layout(location = 0) sample in vec2 fs_in_position_screen;\n"
       << "\n"
       << "layout(location = 0) out vec4 fs_out_color;\n"
       << "\n"
       << "void main (void)\n"
       << "{\n"
       << "    const float threshold = 0.15625; // 4 subpixel bits. Assume 3 accurate bits + 0.03125 for other errors\n"
       << "    const ivec2 nearby_pixel = ivec2(floor(fs_in_position_screen));\n"
       << "    bool ok = false;\n"
       << "\n"
       << "    // sample at edge + inaccuaries may cause us to round to any neighboring pixel\n"
       << "    // check all neighbors for any match\n"
       << "    for (int dy = -1; dy <= 1; ++dy)\n"
       << "    for (int dx = -1; dx <= 1; ++dx)\n"
       << "    {\n"
       << "        ivec2 current_pixel = nearby_pixel + ivec2(dx, dy);\n"
       << "        vec2 position_inside_pixel = vec2(current_pixel) + gl_SamplePosition;\n"
       << "        vec2 position_diff = abs(position_inside_pixel - fs_in_position_screen);\n"
       << "\n"
       << "        if (all(lessThan(position_diff, vec2(threshold))))\n"
       << "            ok = true;\n"
       << "    }\n"
       << "\n"
       << "    if (ok)\n"
       << "        fs_out_color = vec4(0.0, 1.0, 0.0, 1.0);\n"
       << "    else\n"
       << "        fs_out_color = vec4(1.0, 0.0, 0.0, 1.0);\n"
       << "}\n";

    programCollection.glslSources.add("fragment_shader") << glu::FragmentSource(fs.str());
}

template <>
TestInstance *MSCase<MSCaseSamplePosCorrectness>::createInstance(Context &context) const
{
    return new MSInstance<MSInstanceSamplePosCorrectness>(context, m_imageMSParams);
}

class MSInstanceSampleMaskPattern : public MSInstanceBaseResolveAndPerSampleFetch
{
public:
    MSInstanceSampleMaskPattern(Context &context, const ImageMSParams &imageMSParams);

    VkPipelineMultisampleStateCreateInfo getMSStateCreateInfo(const ImageMSParams &imageMSParams) const;

    const VkDescriptorSetLayout *createMSPassDescSetLayout(const ImageMSParams &imageMSParams);

    const VkDescriptorSet *createMSPassDescSet(const ImageMSParams &imageMSParams,
                                               const VkDescriptorSetLayout *descSetLayout);

    VertexDataDesc getVertexDataDescripton(void) const;

    void uploadVertexData(const Allocation &vertexBufferAllocation, const VertexDataDesc &vertexDataDescripton) const;

    tcu::TestStatus verifyImageData(const vk::VkImageCreateInfo &imageMSInfo, const vk::VkImageCreateInfo &imageRSInfo,
                                    const std::vector<tcu::ConstPixelBufferAccess> &dataPerSample,
                                    const tcu::ConstPixelBufferAccess &dataRS) const;

protected:
    VkSampleMask m_sampleMask;
    Move<VkDescriptorSetLayout> m_descriptorSetLayout;
    Move<VkDescriptorPool> m_descriptorPool;
    Move<VkDescriptorSet> m_descriptorSet;
    de::MovePtr<Buffer> m_buffer;
};

MSInstanceSampleMaskPattern::MSInstanceSampleMaskPattern(Context &context, const ImageMSParams &imageMSParams)
    : MSInstanceBaseResolveAndPerSampleFetch(context, imageMSParams)
{
    m_sampleMask = 0xAAAAAAAAu & ((1u << imageMSParams.numSamples) - 1u);
}

VkPipelineMultisampleStateCreateInfo MSInstanceSampleMaskPattern::getMSStateCreateInfo(
    const ImageMSParams &imageMSParams) const
{
    const VkPipelineMultisampleStateCreateInfo multisampleStateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, // VkStructureType sType;
        DE_NULL,                                                  // const void* pNext;
        (VkPipelineMultisampleStateCreateFlags)0u,                // VkPipelineMultisampleStateCreateFlags flags;
        imageMSParams.numSamples,                                 // VkSampleCountFlagBits rasterizationSamples;
        VK_FALSE,                                                 // VkBool32 sampleShadingEnable;
        1.0f,                                                     // float minSampleShading;
        &m_sampleMask,                                            // const VkSampleMask* pSampleMask;
        VK_FALSE,                                                 // VkBool32 alphaToCoverageEnable;
        VK_FALSE,                                                 // VkBool32 alphaToOneEnable;
    };

    return multisampleStateInfo;
}

const VkDescriptorSetLayout *MSInstanceSampleMaskPattern::createMSPassDescSetLayout(const ImageMSParams &imageMSParams)
{
    DE_UNREF(imageMSParams);

    const DeviceInterface &deviceInterface = m_context.getDeviceInterface();
    const VkDevice device                  = m_context.getDevice();

    // Create descriptor set layout
    m_descriptorSetLayout = DescriptorSetLayoutBuilder()
                                .addSingleBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT)
                                .build(deviceInterface, device);

    return &m_descriptorSetLayout.get();
}

const VkDescriptorSet *MSInstanceSampleMaskPattern::createMSPassDescSet(const ImageMSParams &imageMSParams,
                                                                        const VkDescriptorSetLayout *descSetLayout)
{
    DE_UNREF(imageMSParams);

    const DeviceInterface &deviceInterface = m_context.getDeviceInterface();
    const VkDevice device                  = m_context.getDevice();
    Allocator &allocator                   = m_context.getDefaultAllocator();

    // Create descriptor pool
    m_descriptorPool = DescriptorPoolBuilder()
                           .addType(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1u)
                           .build(deviceInterface, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);

    // Create descriptor set
    m_descriptorSet = makeDescriptorSet(deviceInterface, device, *m_descriptorPool, *descSetLayout);

    const VkBufferCreateInfo bufferSampleMaskInfo =
        makeBufferCreateInfo(sizeof(VkSampleMask), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);

    m_buffer = de::MovePtr<Buffer>(
        new Buffer(deviceInterface, device, allocator, bufferSampleMaskInfo, MemoryRequirement::HostVisible));

    deMemcpy(m_buffer->getAllocation().getHostPtr(), &m_sampleMask, sizeof(VkSampleMask));

    flushAlloc(deviceInterface, device, m_buffer->getAllocation());

    const VkDescriptorBufferInfo descBufferInfo = makeDescriptorBufferInfo(**m_buffer, 0u, sizeof(VkSampleMask));

    DescriptorSetUpdateBuilder()
        .writeSingle(*m_descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u),
                     VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &descBufferInfo)
        .update(deviceInterface, device);

    return &m_descriptorSet.get();
}

MultisampleInstanceBase::VertexDataDesc MSInstanceSampleMaskPattern::getVertexDataDescripton(void) const
{
    return getVertexDataDescriptonNdc();
}

void MSInstanceSampleMaskPattern::uploadVertexData(const Allocation &vertexBufferAllocation,
                                                   const VertexDataDesc &vertexDataDescripton) const
{
    uploadVertexDataNdc(vertexBufferAllocation, vertexDataDescripton);
}

tcu::TestStatus MSInstanceSampleMaskPattern::verifyImageData(
    const vk::VkImageCreateInfo &imageMSInfo, const vk::VkImageCreateInfo &imageRSInfo,
    const std::vector<tcu::ConstPixelBufferAccess> &dataPerSample, const tcu::ConstPixelBufferAccess &dataRS) const
{
    DE_UNREF(imageRSInfo);
    DE_UNREF(dataRS);

    if (checkForErrorMS(imageMSInfo, dataPerSample, 0))
        return tcu::TestStatus::fail("gl_SampleMaskIn bits have not been killed by pSampleMask state");

    return tcu::TestStatus::pass("Passed");
}

class MSCaseSampleMaskPattern;

template <>
void MSCase<MSCaseSampleMaskPattern>::init(void)
{
    m_testCtx.getLog() << tcu::TestLog::Message
                       << "Verifying gl_SampleMaskIn value with pSampleMask state. gl_SampleMaskIn does not contain "
                          "any bits set that are have been killed by pSampleMask state. Expecting:\n"
                       << "Expected result: gl_SampleMaskIn AND ~(pSampleMask) should be zero.\n"
                       << tcu::TestLog::EndMessage;

    MultisampleCaseBase::init();
}

template <>
void MSCase<MSCaseSampleMaskPattern>::initPrograms(vk::SourceCollections &programCollection) const
{
    MSCaseBaseResolveAndPerSampleFetch::initPrograms(programCollection);

    // Create vertex shader
    std::ostringstream vs;

    vs << "#version 440\n"
       << "layout(location = 0) in vec4 vs_in_position_ndc;\n"
       << "\n"
       << "out gl_PerVertex {\n"
       << "    vec4  gl_Position;\n"
       << "};\n"
       << "void main (void)\n"
       << "{\n"
       << "    gl_Position = vs_in_position_ndc;\n"
       << "}\n";

    programCollection.glslSources.add("vertex_shader") << glu::VertexSource(vs.str());

    // Create fragment shader
    std::ostringstream fs;

    fs << "#version 440\n"
       << "\n"
       << "layout(location = 0) out vec4 fs_out_color;\n"
       << "\n"
       << "layout(set = 0, binding = 0, std140) uniform SampleMaskBlock\n"
       << "{\n"
       << "    int sampleMaskPattern;\n"
       << "};"
       << "\n"
       << "void main (void)\n"
       << "{\n"
       << "    if ((gl_SampleMaskIn[0] & ~sampleMaskPattern) != 0)\n"
       << "        fs_out_color = vec4(1.0, 0.0, 0.0, 1.0);\n"
       << "    else\n"
       << "        fs_out_color = vec4(0.0, 1.0, 0.0, 1.0);\n"
       << "}\n";

    programCollection.glslSources.add("fragment_shader") << glu::FragmentSource(fs.str());
}

template <>
TestInstance *MSCase<MSCaseSampleMaskPattern>::createInstance(Context &context) const
{
    return new MSInstanceSampleMaskPattern(context, m_imageMSParams);
}

class MSInstanceSampleMaskBitCount;

template <>
MultisampleInstanceBase::VertexDataDesc MSInstance<MSInstanceSampleMaskBitCount>::getVertexDataDescripton(void) const
{
    return getVertexDataDescriptonNdc();
}

template <>
void MSInstance<MSInstanceSampleMaskBitCount>::uploadVertexData(const Allocation &vertexBufferAllocation,
                                                                const VertexDataDesc &vertexDataDescripton) const
{
    uploadVertexDataNdc(vertexBufferAllocation, vertexDataDescripton);
}

template <>
tcu::TestStatus MSInstance<MSInstanceSampleMaskBitCount>::verifyImageData(
    const vk::VkImageCreateInfo &imageMSInfo, const vk::VkImageCreateInfo &imageRSInfo,
    const std::vector<tcu::ConstPixelBufferAccess> &dataPerSample, const tcu::ConstPixelBufferAccess &dataRS) const
{
    DE_UNREF(imageRSInfo);
    DE_UNREF(dataRS);

    if (checkForErrorMS(imageMSInfo, dataPerSample, 0))
        return tcu::TestStatus::fail("gl_SampleMaskIn has more than one bit set for some shader invocations");

    return tcu::TestStatus::pass("Passed");
}

class MSCaseSampleMaskBitCount;

template <>
void MSCase<MSCaseSampleMaskBitCount>::checkSupport(Context &context) const
{
    context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_SAMPLE_RATE_SHADING);
}

template <>
void MSCase<MSCaseSampleMaskBitCount>::init(void)
{
    m_testCtx.getLog() << tcu::TestLog::Message << "Verifying gl_SampleMaskIn.\n"
                       << "    Fragment shader will be invoked numSamples times.\n"
                       << " => gl_SampleMaskIn should have only one bit set for each shader invocation.\n"
                       << tcu::TestLog::EndMessage;

    MultisampleCaseBase::init();
}

template <>
void MSCase<MSCaseSampleMaskBitCount>::initPrograms(vk::SourceCollections &programCollection) const
{
    MSCaseBaseResolveAndPerSampleFetch::initPrograms(programCollection);

    // Create vertex shader
    std::ostringstream vs;

    vs << "#version 440\n"
       << "layout(location = 0) in vec4 vs_in_position_ndc;\n"
       << "\n"
       << "out gl_PerVertex {\n"
       << "    vec4  gl_Position;\n"
       << "};\n"
       << "void main (void)\n"
       << "{\n"
       << "    gl_Position = vs_in_position_ndc;\n"
       << "}\n";

    programCollection.glslSources.add("vertex_shader") << glu::VertexSource(vs.str());

    // Create fragment shader
    std::ostringstream fs;

    fs << "#version 440\n"
       << "\n"
       << "layout(location = 0) out vec4 fs_out_color;\n"
       << "\n"
       << "void main (void)\n"
       << "{\n"
       << "    uint maskBitCount = 0u;\n"
       << "\n"
       << "    for (int i = 0; i < 32; ++i)\n"
       << "        if (((gl_SampleMaskIn[0] >> i) & 0x01) == 0x01)\n"
       << "            ++maskBitCount;\n"
       << "\n"
       << "    if (maskBitCount != 1u)\n"
       << "        fs_out_color = vec4(1.0, 0.0, 0.0, 1.0);\n"
       << "    else\n"
       << "        fs_out_color = vec4(0.0, 1.0, 0.0, 1.0);\n"
       << "}\n";

    programCollection.glslSources.add("fragment_shader") << glu::FragmentSource(fs.str());
}

template <>
TestInstance *MSCase<MSCaseSampleMaskBitCount>::createInstance(Context &context) const
{
    return new MSInstance<MSInstanceSampleMaskBitCount>(context, m_imageMSParams);
}

class MSInstanceSampleMaskCorrectBit;

template <>
MultisampleInstanceBase::VertexDataDesc MSInstance<MSInstanceSampleMaskCorrectBit>::getVertexDataDescripton(void) const
{
    return getVertexDataDescriptonNdc();
}

template <>
void MSInstance<MSInstanceSampleMaskCorrectBit>::uploadVertexData(const Allocation &vertexBufferAllocation,
                                                                  const VertexDataDesc &vertexDataDescripton) const
{
    uploadVertexDataNdc(vertexBufferAllocation, vertexDataDescripton);
}

template <>
tcu::TestStatus MSInstance<MSInstanceSampleMaskCorrectBit>::verifyImageData(
    const vk::VkImageCreateInfo &imageMSInfo, const vk::VkImageCreateInfo &imageRSInfo,
    const std::vector<tcu::ConstPixelBufferAccess> &dataPerSample, const tcu::ConstPixelBufferAccess &dataRS) const
{
    DE_UNREF(imageRSInfo);
    DE_UNREF(dataRS);

    if (checkForErrorMS(imageMSInfo, dataPerSample, 0))
        return tcu::TestStatus::fail("The bit corresponsing to current gl_SampleID is not set in gl_SampleMaskIn");

    return tcu::TestStatus::pass("Passed");
}

class MSCaseSampleMaskCorrectBit;

template <>
void MSCase<MSCaseSampleMaskCorrectBit>::checkSupport(Context &context) const
{
    context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_SAMPLE_RATE_SHADING);
}

template <>
void MSCase<MSCaseSampleMaskCorrectBit>::init(void)
{
    m_testCtx.getLog()
        << tcu::TestLog::Message << "Verifying gl_SampleMaskIn.\n"
        << "    Fragment shader will be invoked numSamples times.\n"
        << " => In each invocation gl_SampleMaskIn should have the bit set that corresponds to gl_SampleID.\n"
        << tcu::TestLog::EndMessage;

    MultisampleCaseBase::init();
}

template <>
void MSCase<MSCaseSampleMaskCorrectBit>::initPrograms(vk::SourceCollections &programCollection) const
{
    MSCaseBaseResolveAndPerSampleFetch::initPrograms(programCollection);

    // Create vertex shader
    std::ostringstream vs;

    vs << "#version 440\n"
       << "layout(location = 0) in vec4 vs_in_position_ndc;\n"
       << "\n"
       << "out gl_PerVertex {\n"
       << "    vec4  gl_Position;\n"
       << "};\n"
       << "void main (void)\n"
       << "{\n"
       << "    gl_Position = vs_in_position_ndc;\n"
       << "}\n";

    programCollection.glslSources.add("vertex_shader") << glu::VertexSource(vs.str());

    // Create fragment shader
    std::ostringstream fs;

    fs << "#version 440\n"
       << "\n"
       << "layout(location = 0) out vec4 fs_out_color;\n"
       << "\n"
       << "void main (void)\n"
       << "{\n"
       << "    if (((gl_SampleMaskIn[0] >> gl_SampleID) & 0x01) == 0x01)\n"
       << "        fs_out_color = vec4(0.0, 1.0, 0.0, 1.0);\n"
       << "    else\n"
       << "        fs_out_color = vec4(1.0, 0.0, 0.0, 1.0);\n"
       << "}\n";

    programCollection.glslSources.add("fragment_shader") << glu::FragmentSource(fs.str());
}

template <>
TestInstance *MSCase<MSCaseSampleMaskCorrectBit>::createInstance(Context &context) const
{
    return new MSInstance<MSInstanceSampleMaskCorrectBit>(context, m_imageMSParams);
}

class MSInstanceSampleMaskWrite;

template <>
MultisampleInstanceBase::VertexDataDesc MSInstance<MSInstanceSampleMaskWrite>::getVertexDataDescripton(void) const
{
    return getVertexDataDescriptonNdc();
}

template <>
void MSInstance<MSInstanceSampleMaskWrite>::uploadVertexData(const Allocation &vertexBufferAllocation,
                                                             const VertexDataDesc &vertexDataDescripton) const
{
    uploadVertexDataNdc(vertexBufferAllocation, vertexDataDescripton);
}

//! Creates VkPipelineMultisampleStateCreateInfo with sample shading disabled.
template <>
VkPipelineMultisampleStateCreateInfo MSInstance<MSInstanceSampleMaskWrite>::getMSStateCreateInfo(
    const ImageMSParams &imageMSParams) const
{
    const VkPipelineMultisampleStateCreateInfo multisampleStateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, // VkStructureType sType;
        DE_NULL,                                                  // const void* pNext;
        (VkPipelineMultisampleStateCreateFlags)0u,                // VkPipelineMultisampleStateCreateFlags flags;
        imageMSParams.numSamples,                                 // VkSampleCountFlagBits rasterizationSamples;
        VK_FALSE,                                                 // VkBool32 sampleShadingEnable;
        0.0f,                                                     // float minSampleShading;
        DE_NULL,                                                  // const VkSampleMask* pSampleMask;
        VK_FALSE,                                                 // VkBool32 alphaToCoverageEnable;
        VK_FALSE,                                                 // VkBool32 alphaToOneEnable;
    };

    return multisampleStateInfo;
}

template <>
tcu::TestStatus MSInstance<MSInstanceSampleMaskWrite>::verifyImageData(
    const vk::VkImageCreateInfo &imageMSInfo, const vk::VkImageCreateInfo &imageRSInfo,
    const std::vector<tcu::ConstPixelBufferAccess> &dataPerSample, const tcu::ConstPixelBufferAccess &dataRS) const
{
    const uint32_t numSamples = static_cast<uint32_t>(imageMSInfo.samples);

    for (uint32_t z = 0u; z < imageMSInfo.extent.depth; ++z)
        for (uint32_t y = 0u; y < imageMSInfo.extent.height; ++y)
            for (uint32_t x = 0u; x < imageMSInfo.extent.width; ++x)
            {
                for (uint32_t sampleNdx = 0u; sampleNdx < numSamples; ++sampleNdx)
                {
                    const uint32_t firstComponent = dataPerSample[sampleNdx].getPixelUint(x, y, z)[0];

                    if (firstComponent != 0u && firstComponent != 255u)
                        return tcu::TestStatus::fail("Expected color to be zero or saturated on the first channel");
                }
            }

    for (uint32_t z = 0u; z < imageRSInfo.extent.depth; ++z)
        for (uint32_t y = 0u; y < imageRSInfo.extent.height; ++y)
            for (uint32_t x = 0u; x < imageRSInfo.extent.width; ++x)
            {
                const float firstComponent = dataRS.getPixel(x, y, z)[0];

                if (deFloatAbs(firstComponent - 0.5f) > 0.02f)
                    return tcu::TestStatus::fail("Expected resolve color to be half intensity on the first channel");
            }

    return tcu::TestStatus::pass("Passed");
}

class MSCaseSampleMaskWrite;

template <>
void MSCase<MSCaseSampleMaskWrite>::init(void)
{
    m_testCtx.getLog() << tcu::TestLog::Message << "Discarding half of the samples using gl_SampleMask."
                       << "Expecting half intensity on multisample targets (numSamples > 1)\n"
                       << tcu::TestLog::EndMessage;

    MultisampleCaseBase::init();
}

template <>
void MSCase<MSCaseSampleMaskWrite>::initPrograms(vk::SourceCollections &programCollection) const
{
    MSCaseBaseResolveAndPerSampleFetch::initPrograms(programCollection);

    // Create vertex shader
    std::ostringstream vs;

    vs << "#version 440\n"
       << "layout(location = 0) in vec4 vs_in_position_ndc;\n"
       << "\n"
       << "out gl_PerVertex {\n"
       << "    vec4  gl_Position;\n"
       << "};\n"
       << "void main (void)\n"
       << "{\n"
       << "    gl_Position = vs_in_position_ndc;\n"
       << "}\n";

    programCollection.glslSources.add("vertex_shader") << glu::VertexSource(vs.str());

    // Create fragment shader
    std::ostringstream fs;

    fs << "#version 440\n"
       << "\n"
       << "layout(location = 0) out vec4 fs_out_color;\n"
       << "\n"
       << "void main (void)\n"
       << "{\n"
       << "    gl_SampleMask[0] = 0xAAAAAAAA;\n"
       << "\n"
       << "    fs_out_color = vec4(1.0, 0.0, 0.0, 1.0);\n"
       << "}\n";

    programCollection.glslSources.add("fragment_shader") << glu::FragmentSource(fs.str());
}

template <>
TestInstance *MSCase<MSCaseSampleMaskWrite>::createInstance(Context &context) const
{
    return new MSInstance<MSInstanceSampleMaskWrite>(context, m_imageMSParams);
}

struct WriteSampleParams
{
    vk::VkSampleCountFlagBits sampleCount;
};

class WriteSampleTest : public vkt::TestCase
{
public:
    WriteSampleTest(tcu::TestContext &testCtx, const std::string &name, const std::string &desc,
                    const WriteSampleParams &params)
        : vkt::TestCase(testCtx, name, desc)
        , m_params(params)
    {
    }
    virtual ~WriteSampleTest(void)
    {
    }

    virtual void initPrograms(vk::SourceCollections &programCollection) const;
    virtual vkt::TestInstance *createInstance(Context &context) const;
    virtual void checkSupport(Context &context) const;

    static void assertSampleCount(uint32_t sampleCount);
    static uint32_t bitsPerCoord(uint32_t sampleCount);
    static uint32_t imageSize(uint32_t sampleCount);
    static vk::VkExtent3D getExtent3D(uint32_t sampleCount);
    static std::string getShaderDecl(const tcu::Vec4 &color);

    static const tcu::Vec4 kClearColor;
    static const tcu::Vec4 kBadColor;
    static const tcu::Vec4 kGoodColor;
    static const tcu::Vec4 kWriteColor;

    static const set<uint32_t> kValidSampleCounts;
    static constexpr vk::VkFormat kImageFormat = vk::VK_FORMAT_R8G8B8A8_UNORM;

    // Keep these two in sync.
    static constexpr vk::VkImageUsageFlags kUsageFlags =
        (vk::VK_IMAGE_USAGE_STORAGE_BIT | vk::VK_IMAGE_USAGE_TRANSFER_SRC_BIT | vk::VK_IMAGE_USAGE_TRANSFER_DST_BIT);
    static constexpr vk::VkFormatFeatureFlags kFeatureFlags =
        (vk::VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT | vk::VK_FORMAT_FEATURE_TRANSFER_SRC_BIT |
         vk::VK_FORMAT_FEATURE_TRANSFER_DST_BIT);

private:
    WriteSampleParams m_params;
};

const tcu::Vec4 WriteSampleTest::kClearColor{0.0f, 0.0f, 0.0f, 1.0f};
const tcu::Vec4 WriteSampleTest::kBadColor{1.0f, 0.0f, 0.0f, 1.0f};
const tcu::Vec4 WriteSampleTest::kGoodColor{0.0f, 1.0f, 0.0f, 1.0f};
const tcu::Vec4 WriteSampleTest::kWriteColor{0.0f, 0.0f, 1.0f, 1.0f};

class WriteSampleTestInstance : public vkt::TestInstance
{
public:
    WriteSampleTestInstance(vkt::Context &context, const WriteSampleParams &params)
        : vkt::TestInstance(context)
        , m_params(params)
    {
    }

    virtual ~WriteSampleTestInstance(void)
    {
    }

    virtual tcu::TestStatus iterate(void);

private:
    WriteSampleParams m_params;
};

const set<uint32_t> WriteSampleTest::kValidSampleCounts = {
    vk::VK_SAMPLE_COUNT_2_BIT,
    vk::VK_SAMPLE_COUNT_4_BIT,
    vk::VK_SAMPLE_COUNT_8_BIT,
    vk::VK_SAMPLE_COUNT_16_BIT,
};

void WriteSampleTest::assertSampleCount(uint32_t sampleCount)
{
    DE_ASSERT(kValidSampleCounts.find(sampleCount) != kValidSampleCounts.end());
    DE_UNREF(sampleCount); // for release builds.
}

// The test will try to verify all write combinations for the given sample count, and will verify one combination per image pixel.
// This means the following image sizes need to be used:
//        - 2 samples: 2x2
//        - 4 samples: 4x4
//        - 8 samples: 16x16
//        - 16 samples: 256x256
// In other words, images will be square with 2^(samples-1) pixels on each side.
uint32_t WriteSampleTest::imageSize(uint32_t sampleCount)
{
    assertSampleCount(sampleCount);
    return (1u << (sampleCount >> 1u));
}

// When dealing with N samples, each coordinate (x, y) will be used to decide which samples will be written to, using N/2 bits for each of the X and Y values.
uint32_t WriteSampleTest::bitsPerCoord(uint32_t numSamples)
{
    assertSampleCount(numSamples);
    return (numSamples / 2u);
}

vk::VkExtent3D WriteSampleTest::getExtent3D(uint32_t sampleCount)
{
    const uint32_t size = imageSize(sampleCount);
    return vk::VkExtent3D{size, size, 1u};
}

std::string WriteSampleTest::getShaderDecl(const tcu::Vec4 &color)
{
    std::ostringstream declaration;
    declaration << "vec4(" << color.x() << ", " << color.y() << ", " << color.z() << ", " << color.w() << ")";
    return declaration.str();
}

void WriteSampleTest::checkSupport(Context &context) const
{
    const auto &vki           = context.getInstanceInterface();
    const auto physicalDevice = context.getPhysicalDevice();

    // Check multisample storage images support.
    const auto features = vk::getPhysicalDeviceFeatures(vki, physicalDevice);
    if (!features.shaderStorageImageMultisample)
        TCU_THROW(NotSupportedError, "Using multisample images as storage is not supported");

    // Check the specific image format.
    const auto properties = vk::getPhysicalDeviceFormatProperties(vki, physicalDevice, kImageFormat);
    if (!(properties.optimalTilingFeatures & kFeatureFlags))
        TCU_THROW(NotSupportedError, "Format does not support the required features");

    // Check the supported sample count.
    const auto imgProps = vk::getPhysicalDeviceImageFormatProperties(
        vki, physicalDevice, kImageFormat, vk::VK_IMAGE_TYPE_2D, vk::VK_IMAGE_TILING_OPTIMAL, kUsageFlags, 0u);
    if (!(imgProps.sampleCounts & m_params.sampleCount))
        TCU_THROW(NotSupportedError, "Format does not support the required sample count");
}

void WriteSampleTest::initPrograms(vk::SourceCollections &programCollection) const
{
    std::ostringstream writeColorDecl, goodColorDecl, badColorDecl, clearColorDecl, allColorDecl;

    writeColorDecl << "        vec4  wcolor   = " << getShaderDecl(kWriteColor) << ";\n";
    goodColorDecl << "        vec4  bcolor   = " << getShaderDecl(kBadColor) << ";\n";
    badColorDecl << "        vec4  gcolor   = " << getShaderDecl(kGoodColor) << ";\n";
    clearColorDecl << "        vec4  ccolor   = " << getShaderDecl(kClearColor) << ";\n";
    allColorDecl << writeColorDecl.str() << goodColorDecl.str() << badColorDecl.str() << clearColorDecl.str();

    std::ostringstream shaderWrite;

    const auto bpc   = de::toString(bitsPerCoord(m_params.sampleCount));
    const auto count = de::toString(m_params.sampleCount);

    shaderWrite << "#version 450\n"
                << "\n"
                << "layout (rgba8, set=0, binding=0) uniform image2DMS writeImg;\n"
                << "layout (rgba8, set=0, binding=1) uniform image2D   verificationImg;\n"
                << "\n"
                << "void main()\n"
                << "{\n"
                << writeColorDecl.str() << "        uvec2 ucoords  = uvec2(gl_GlobalInvocationID.xy);\n"
                << "        ivec2 icoords  = ivec2(ucoords);\n"
                << "        uint writeMask = ((ucoords.x << " << bpc << ") | ucoords.y);\n"
                << "        for (uint i = 0; i < " << count << "; ++i)\n"
                << "        {\n"
                << "                if ((writeMask & (1 << i)) != 0)\n"
                << "                        imageStore(writeImg, icoords, int(i), wcolor);\n"
                << "        }\n"
                << "}\n";

    std::ostringstream shaderVerify;

    shaderVerify << "#version 450\n"
                 << "\n"
                 << "layout (rgba8, set=0, binding=0) uniform image2DMS writeImg;\n"
                 << "layout (rgba8, set=0, binding=1) uniform image2D   verificationImg;\n"
                 << "\n"
                 << "void main()\n"
                 << "{\n"
                 << allColorDecl.str() << "        uvec2 ucoords  = uvec2(gl_GlobalInvocationID.xy);\n"
                 << "        ivec2 icoords  = ivec2(ucoords);\n"
                 << "        uint writeMask = ((ucoords.x << " << bpc << ") | ucoords.y);\n"
                 << "        bool ok = true;\n"
                 << "        for (uint i = 0; i < " << count << "; ++i)\n"
                 << "        {\n"
                 << "                bool expectWrite = ((writeMask & (1 << i)) != 0);\n"
                 << "                vec4 sampleColor = imageLoad(writeImg, icoords, int(i));\n"
                 << "                vec4 wantedColor = (expectWrite ? wcolor : ccolor);\n"
                 << "                ok = ok && (sampleColor == wantedColor);\n"
                 << "        }\n"
                 << "        vec4 resultColor = (ok ? gcolor : bcolor);\n"
                 << "        imageStore(verificationImg, icoords, resultColor);\n"
                 << "}\n";

    programCollection.glslSources.add("write") << glu::ComputeSource(shaderWrite.str());
    programCollection.glslSources.add("verify") << glu::ComputeSource(shaderVerify.str());
}

vkt::TestInstance *WriteSampleTest::createInstance(Context &context) const
{
    return new WriteSampleTestInstance{context, m_params};
}

tcu::TestStatus WriteSampleTestInstance::iterate(void)
{
    const auto &vkd       = m_context.getDeviceInterface();
    const auto device     = m_context.getDevice();
    auto &allocator       = m_context.getDefaultAllocator();
    const auto queue      = m_context.getUniversalQueue();
    const auto queueIndex = m_context.getUniversalQueueFamilyIndex();
    const auto extent3D   = WriteSampleTest::getExtent3D(m_params.sampleCount);

    // Create storage image and verification image.
    const vk::VkImageCreateInfo storageImageInfo = {
        vk::VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, // VkStructureType sType;
        nullptr,                                 // const void* pNext;
        0u,                                      // VkImageCreateFlags flags;
        vk::VK_IMAGE_TYPE_2D,                    // VkImageType imageType;
        WriteSampleTest::kImageFormat,           // VkFormat format;
        extent3D,                                // VkExtent3D extent;
        1u,                                      // uint32_t mipLevels;
        1u,                                      // uint32_t arrayLayers;
        m_params.sampleCount,                    // VkSampleCountFlagBits samples;
        vk::VK_IMAGE_TILING_OPTIMAL,             // VkImageTiling tiling;
        WriteSampleTest::kUsageFlags,            // VkImageUsageFlags usage;
        vk::VK_SHARING_MODE_EXCLUSIVE,           // VkSharingMode sharingMode;
        1u,                                      // uint32_t queueFamilyIndexCount;
        &queueIndex,                             // const uint32_t* pQueueFamilyIndices;
        vk::VK_IMAGE_LAYOUT_UNDEFINED,           // VkImageLayout initialLayout;
    };

    const vk::VkImageCreateInfo verificationImageInfo = {
        vk::VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, // VkStructureType sType;
        nullptr,                                 // const void* pNext;
        0u,                                      // VkImageCreateFlags flags;
        vk::VK_IMAGE_TYPE_2D,                    // VkImageType imageType;
        WriteSampleTest::kImageFormat,           // VkFormat format;
        extent3D,                                // VkExtent3D extent;
        1u,                                      // uint32_t mipLevels;
        1u,                                      // uint32_t arrayLayers;
        vk::VK_SAMPLE_COUNT_1_BIT,               // VkSampleCountFlagBits samples;
        vk::VK_IMAGE_TILING_OPTIMAL,             // VkImageTiling tiling;
        WriteSampleTest::kUsageFlags,            // VkImageUsageFlags usage;
        vk::VK_SHARING_MODE_EXCLUSIVE,           // VkSharingMode sharingMode;
        1u,                                      // uint32_t queueFamilyIndexCount;
        &queueIndex,                             // const uint32_t* pQueueFamilyIndices;
        vk::VK_IMAGE_LAYOUT_UNDEFINED,           // VkImageLayout initialLayout;
    };

    vk::ImageWithMemory storageImgPrt{vkd, device, allocator, storageImageInfo, vk::MemoryRequirement::Any};
    vk::ImageWithMemory verificationImgPtr{vkd, device, allocator, verificationImageInfo, vk::MemoryRequirement::Any};

    const vk::VkImageSubresourceRange kSubresourceRange = {
        vk::VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
        0u,                            // uint32_t baseMipLevel;
        1u,                            // uint32_t levelCount;
        0u,                            // uint32_t baseArrayLayer;
        1u,                            // uint32_t layerCount;
    };

    auto storageImgViewPtr      = vk::makeImageView(vkd, device, storageImgPrt.get(), vk::VK_IMAGE_VIEW_TYPE_2D,
                                                    WriteSampleTest::kImageFormat, kSubresourceRange);
    auto verificationImgViewPtr = vk::makeImageView(vkd, device, verificationImgPtr.get(), vk::VK_IMAGE_VIEW_TYPE_2D,
                                                    WriteSampleTest::kImageFormat, kSubresourceRange);

    // Prepare a staging buffer to check verification image.
    const auto tcuFormat          = vk::mapVkFormat(WriteSampleTest::kImageFormat);
    const VkDeviceSize bufferSize = extent3D.width * extent3D.height * extent3D.depth * tcu::getPixelSize(tcuFormat);
    const auto stagingBufferInfo  = vk::makeBufferCreateInfo(bufferSize, vk::VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    vk::BufferWithMemory stagingBuffer{vkd, device, allocator, stagingBufferInfo, MemoryRequirement::HostVisible};

    // Descriptor set layout.
    vk::DescriptorSetLayoutBuilder layoutBuilder;
    layoutBuilder.addSingleBinding(vk::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, vk::VK_SHADER_STAGE_COMPUTE_BIT);
    layoutBuilder.addSingleBinding(vk::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, vk::VK_SHADER_STAGE_COMPUTE_BIT);
    auto descriptorSetLayout = layoutBuilder.build(vkd, device);

    // Descriptor pool.
    vk::DescriptorPoolBuilder poolBuilder;
    poolBuilder.addType(vk::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 2u);
    auto descriptorPool = poolBuilder.build(vkd, device, vk::VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);

    // Descriptor set.
    const auto descriptorSet = vk::makeDescriptorSet(vkd, device, descriptorPool.get(), descriptorSetLayout.get());

    // Update descriptor set using the images.
    const auto storageImgDescriptorInfo =
        vk::makeDescriptorImageInfo(DE_NULL, storageImgViewPtr.get(), vk::VK_IMAGE_LAYOUT_GENERAL);
    const auto verificationImgDescriptorInfo =
        vk::makeDescriptorImageInfo(DE_NULL, verificationImgViewPtr.get(), vk::VK_IMAGE_LAYOUT_GENERAL);

    vk::DescriptorSetUpdateBuilder updateBuilder;
    updateBuilder.writeSingle(descriptorSet.get(), vk::DescriptorSetUpdateBuilder::Location::binding(0u),
                              vk::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &storageImgDescriptorInfo);
    updateBuilder.writeSingle(descriptorSet.get(), vk::DescriptorSetUpdateBuilder::Location::binding(1u),
                              vk::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &verificationImgDescriptorInfo);
    updateBuilder.update(vkd, device);

    // Create write and verification compute pipelines.
    auto shaderWriteModule  = vk::createShaderModule(vkd, device, m_context.getBinaryCollection().get("write"), 0u);
    auto shaderVerifyModule = vk::createShaderModule(vkd, device, m_context.getBinaryCollection().get("verify"), 0u);
    auto pipelineLayout     = vk::makePipelineLayout(vkd, device, descriptorSetLayout.get());

    const vk::VkComputePipelineCreateInfo writePipelineCreateInfo = {
        vk::VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        nullptr,
        0u, // flags
        {
            // compute shader
            vk::VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, // VkStructureType sType;
            nullptr,                                                 // const void* pNext;
            0u,                                                      // VkPipelineShaderStageCreateFlags flags;
            vk::VK_SHADER_STAGE_COMPUTE_BIT,                         // VkShaderStageFlagBits stage;
            shaderWriteModule.get(),                                 // VkShaderModule module;
            "main",                                                  // const char* pName;
            nullptr,                                                 // const VkSpecializationInfo* pSpecializationInfo;
        },
        pipelineLayout.get(), // layout
        DE_NULL,              // basePipelineHandle
        0,                    // basePipelineIndex
    };

    auto verificationPipelineCreateInfo         = writePipelineCreateInfo;
    verificationPipelineCreateInfo.stage.module = shaderVerifyModule.get();

    auto writePipeline        = vk::createComputePipeline(vkd, device, DE_NULL, &writePipelineCreateInfo);
    auto verificationPipeline = vk::createComputePipeline(vkd, device, DE_NULL, &verificationPipelineCreateInfo);

    // Transition images to the correct layout and buffers at different stages.
    auto storageImgPreClearBarrier =
        vk::makeImageMemoryBarrier(0, vk::VK_ACCESS_TRANSFER_WRITE_BIT, vk::VK_IMAGE_LAYOUT_UNDEFINED,
                                   vk::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, storageImgPrt.get(), kSubresourceRange);
    auto storageImgPreShaderBarrier = vk::makeImageMemoryBarrier(
        vk::VK_ACCESS_TRANSFER_WRITE_BIT, vk::VK_ACCESS_SHADER_WRITE_BIT, vk::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        vk::VK_IMAGE_LAYOUT_GENERAL, storageImgPrt.get(), kSubresourceRange);
    auto verificationImgPreShaderBarrier =
        vk::makeImageMemoryBarrier(0, vk::VK_ACCESS_SHADER_WRITE_BIT, vk::VK_IMAGE_LAYOUT_UNDEFINED,
                                   vk::VK_IMAGE_LAYOUT_GENERAL, verificationImgPtr.get(), kSubresourceRange);
    auto storageImgPreVerificationBarrier = vk::makeImageMemoryBarrier(
        vk::VK_ACCESS_SHADER_WRITE_BIT, vk::VK_ACCESS_SHADER_READ_BIT, vk::VK_IMAGE_LAYOUT_GENERAL,
        vk::VK_IMAGE_LAYOUT_GENERAL, storageImgPrt.get(), kSubresourceRange);
    auto verificationImgPostBarrier = vk::makeImageMemoryBarrier(
        vk::VK_ACCESS_SHADER_WRITE_BIT, vk::VK_ACCESS_TRANSFER_READ_BIT, vk::VK_IMAGE_LAYOUT_GENERAL,
        vk::VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, verificationImgPtr.get(), kSubresourceRange);
    auto bufferBarrier = vk::makeBufferMemoryBarrier(vk::VK_ACCESS_TRANSFER_WRITE_BIT, vk::VK_ACCESS_HOST_READ_BIT,
                                                     stagingBuffer.get(), 0ull, bufferSize);

    // Command buffer.
    auto cmdPool      = vk::makeCommandPool(vkd, device, queueIndex);
    auto cmdBufferPtr = vk::allocateCommandBuffer(vkd, device, cmdPool.get(), vk::VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    auto cmdBuffer    = cmdBufferPtr.get();

    // Clear color for the storage image.
    const auto clearColor = vk::makeClearValueColor(WriteSampleTest::kClearColor);

    const vk::VkBufferImageCopy copyRegion = {
        0ull,            // VkDeviceSize bufferOffset;
        extent3D.width,  // uint32_t bufferRowLength;
        extent3D.height, // uint32_t bufferImageHeight;
        {
            // VkImageSubresourceLayers imageSubresource;
            vk::VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
            0u,                            // uint32_t mipLevel;
            0u,                            // uint32_t baseArrayLayer;
            1u,                            // uint32_t layerCount;
        },
        {0, 0, 0}, // VkOffset3D imageOffset;
        extent3D,  // VkExtent3D imageExtent;
    };

    // Record and submit commands.
    vk::beginCommandBuffer(vkd, cmdBuffer);
    // Clear storage image.
    vkd.cmdPipelineBarrier(cmdBuffer, vk::VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, vk::VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u,
                           nullptr, 0u, nullptr, 1u, &storageImgPreClearBarrier);
    vkd.cmdClearColorImage(cmdBuffer, storageImgPrt.get(), vk::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearColor.color,
                           1u, &kSubresourceRange);

    // Bind write pipeline and descriptor set.
    vkd.cmdBindPipeline(cmdBuffer, vk::VK_PIPELINE_BIND_POINT_COMPUTE, writePipeline.get());
    vkd.cmdBindDescriptorSets(cmdBuffer, vk::VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout.get(), 0, 1u,
                              &descriptorSet.get(), 0u, nullptr);

    // Transition images to the appropriate layout before running the shader.
    vkd.cmdPipelineBarrier(cmdBuffer, vk::VK_PIPELINE_STAGE_TRANSFER_BIT, vk::VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0u,
                           0u, nullptr, 0u, nullptr, 1u, &storageImgPreShaderBarrier);
    vkd.cmdPipelineBarrier(cmdBuffer, vk::VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, vk::VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                           0u, 0u, nullptr, 0u, nullptr, 1u, &verificationImgPreShaderBarrier);

    // Run shader.
    vkd.cmdDispatch(cmdBuffer, extent3D.width, extent3D.height, extent3D.depth);

    // Bind verification pipeline.
    vkd.cmdBindPipeline(cmdBuffer, vk::VK_PIPELINE_BIND_POINT_COMPUTE, verificationPipeline.get());

    // Make sure writes happen before reads in the second dispatch for the storage image.
    vkd.cmdPipelineBarrier(cmdBuffer, vk::VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                           vk::VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0u, 0u, nullptr, 0u, nullptr, 1u,
                           &storageImgPreVerificationBarrier);

    // Run verification shader.
    vkd.cmdDispatch(cmdBuffer, extent3D.width, extent3D.height, extent3D.depth);

    // Change verification image layout to prepare the transfer.
    vkd.cmdPipelineBarrier(cmdBuffer, vk::VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, vk::VK_PIPELINE_STAGE_TRANSFER_BIT, 0u,
                           0u, nullptr, 0u, nullptr, 1u, &verificationImgPostBarrier);

    // Copy verification image to staging buffer.
    vkd.cmdCopyImageToBuffer(cmdBuffer, verificationImgPtr.get(), vk::VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                             stagingBuffer.get(), 1u, &copyRegion);
    vkd.cmdPipelineBarrier(cmdBuffer, vk::VK_PIPELINE_STAGE_TRANSFER_BIT, vk::VK_PIPELINE_STAGE_HOST_BIT, 0, 0u,
                           nullptr, 1u, &bufferBarrier, 0u, nullptr);

    vk::endCommandBuffer(vkd, cmdBuffer);

    // Run shaders.
    vk::submitCommandsAndWait(vkd, device, queue, cmdBuffer);

    // Read buffer pixels.
    const auto &bufferAlloc = stagingBuffer.getAllocation();
    vk::invalidateAlloc(vkd, device, bufferAlloc);

    // Copy buffer data to texture level and verify all pixels have the proper color.
    tcu::TextureLevel texture{tcuFormat, static_cast<int>(extent3D.width), static_cast<int>(extent3D.height),
                              static_cast<int>(extent3D.depth)};
    const auto access = texture.getAccess();
    deMemcpy(access.getDataPtr(), reinterpret_cast<char *>(bufferAlloc.getHostPtr()) + bufferAlloc.getOffset(),
             static_cast<size_t>(bufferSize));

    for (int i = 0; i < access.getWidth(); ++i)
        for (int j = 0; j < access.getHeight(); ++j)
            for (int k = 0; k < access.getDepth(); ++k)
            {
                if (access.getPixel(i, j, k) != WriteSampleTest::kGoodColor)
                {
                    std::ostringstream msg;
                    msg << "Invalid result at pixel (" << i << ", " << j << ", " << k
                        << "); check error mask for more details";
                    m_context.getTestContext().getLog()
                        << tcu::TestLog::Image("ErrorMask", "Indicates which pixels have unexpected values", access);
                    return tcu::TestStatus::fail(msg.str());
                }
            }

    return tcu::TestStatus::pass("Pass");
}

} // namespace multisample

tcu::TestCaseGroup *createMultisampleShaderBuiltInTests(tcu::TestContext &testCtx)
{
    de::MovePtr<tcu::TestCaseGroup> testGroup(
        new tcu::TestCaseGroup(testCtx, "multisample_shader_builtin", "Multisample Shader BuiltIn Tests"));

    const tcu::UVec3 imageSizes[] = {
        tcu::UVec3(128u, 128u, 1u),
        tcu::UVec3(137u, 191u, 1u),
    };

    const uint32_t sizesElemCount = static_cast<uint32_t>(sizeof(imageSizes) / sizeof(tcu::UVec3));

    const vk::VkSampleCountFlagBits samplesSetFull[] = {
        vk::VK_SAMPLE_COUNT_2_BIT,  vk::VK_SAMPLE_COUNT_4_BIT,  vk::VK_SAMPLE_COUNT_8_BIT,
        vk::VK_SAMPLE_COUNT_16_BIT, vk::VK_SAMPLE_COUNT_32_BIT, vk::VK_SAMPLE_COUNT_64_BIT,
    };

    const uint32_t samplesSetFullCount =
        static_cast<uint32_t>(sizeof(samplesSetFull) / sizeof(vk::VkSampleCountFlagBits));

    testGroup->addChild(makeMSGroup<multisample::MSCase<multisample::MSCaseSampleID>>(
        testCtx, "sample_id", imageSizes, sizesElemCount, samplesSetFull, samplesSetFullCount));

    de::MovePtr<tcu::TestCaseGroup> samplePositionGroup(
        new tcu::TestCaseGroup(testCtx, "sample_position", "Sample Position Tests"));

    samplePositionGroup->addChild(makeMSGroup<multisample::MSCase<multisample::MSCaseSamplePosDistribution>>(
        testCtx, "distribution", imageSizes, sizesElemCount, samplesSetFull, samplesSetFullCount));
    samplePositionGroup->addChild(makeMSGroup<multisample::MSCase<multisample::MSCaseSamplePosCorrectness>>(
        testCtx, "correctness", imageSizes, sizesElemCount, samplesSetFull, samplesSetFullCount));

    testGroup->addChild(samplePositionGroup.release());

    const vk::VkSampleCountFlagBits samplesSetReduced[] = {
        vk::VK_SAMPLE_COUNT_2_BIT,  vk::VK_SAMPLE_COUNT_4_BIT,  vk::VK_SAMPLE_COUNT_8_BIT,
        vk::VK_SAMPLE_COUNT_16_BIT, vk::VK_SAMPLE_COUNT_32_BIT,
    };

    const uint32_t samplesSetReducedCount = static_cast<uint32_t>(DE_LENGTH_OF_ARRAY(samplesSetReduced));

    de::MovePtr<tcu::TestCaseGroup> sampleMaskGroup(
        new tcu::TestCaseGroup(testCtx, "sample_mask", "Sample Mask Tests"));

    sampleMaskGroup->addChild(makeMSGroup<multisample::MSCase<multisample::MSCaseSampleMaskPattern>>(
        testCtx, "pattern", imageSizes, sizesElemCount, samplesSetReduced, samplesSetReducedCount));
    sampleMaskGroup->addChild(makeMSGroup<multisample::MSCase<multisample::MSCaseSampleMaskBitCount>>(
        testCtx, "bit_count", imageSizes, sizesElemCount, samplesSetReduced, samplesSetReducedCount));
    sampleMaskGroup->addChild(makeMSGroup<multisample::MSCase<multisample::MSCaseSampleMaskCorrectBit>>(
        testCtx, "correct_bit", imageSizes, sizesElemCount, samplesSetReduced, samplesSetReducedCount));
    sampleMaskGroup->addChild(makeMSGroup<multisample::MSCase<multisample::MSCaseSampleMaskWrite>>(
        testCtx, "write", imageSizes, sizesElemCount, samplesSetReduced, samplesSetReducedCount));

    testGroup->addChild(sampleMaskGroup.release());

    {
        de::MovePtr<tcu::TestCaseGroup> imageWriteSampleGroup(
            new tcu::TestCaseGroup(testCtx, "image_write_sample", "Test OpImageWrite with a sample ID"));

        for (auto count : multisample::WriteSampleTest::kValidSampleCounts)
        {
            multisample::WriteSampleParams params{static_cast<vk::VkSampleCountFlagBits>(count)};
            const auto countStr = de::toString(count);
            imageWriteSampleGroup->addChild(new multisample::WriteSampleTest(
                testCtx, countStr + "_samples", "Test image with " + countStr + " samples", params));
        }

        testGroup->addChild(imageWriteSampleGroup.release());
    }

    return testGroup.release();
}

} // namespace pipeline
} // namespace vkt
