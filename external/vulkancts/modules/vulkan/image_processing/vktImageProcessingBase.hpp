#ifndef _VKTIMAGEPROCESSINGBASE_HPP
#define _VKTIMAGEPROCESSINGBASE_HPP
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

#include "vktImageProcessingTests.hpp"
#include "vktImageProcessingTestsUtil.hpp"
#include "vktTestCase.hpp"

#include "deRandom.hpp"
#include "deUniquePtr.hpp"

#include "tcuDefs.hpp"

using namespace tcu;

namespace vkt
{
namespace ImageProcessing
{

enum SamplerReductionMode
{
    SAMPLER_REDUCTION_MODE_NONE         = 0,
    SAMPLER_REDUCTION_MODE_WEIGHTED_AVG = 1,
    SAMPLER_REDUCTION_MODE_MIN          = 2,
    SAMPLER_REDUCTION_MODE_MAX          = 3,
};

VkSamplerReductionMode getVkSamplerReductionMode(const SamplerReductionMode reductionMode);

// In case of block matching, these are parameters of reference image
struct TestImageParams
{
    ImageType imageType;
    UVec2 imageSize;
    VkFormat format;
    VkImageTiling tiling;
    VkImageLayout layout;
    VkComponentMapping components;
    VkSamplerAddressMode addrMode;
    SamplerReductionMode reductionMode;
};

struct TestParams
{
    ImageProcOp imageProcOp;
    TestImageParams sampledImageParams;
    bool randomReference;
    bool updateAfterBind;
    PipelineConstructionType pipelineConstructionType; // ignored when testCompute = true
    VkShaderStageFlags stageMask;                      // ignored when testCompute = true
};

struct VertexData
{
    Vec2 positions;

    VertexData(const Vec2 pos) : positions(pos)
    {
    }
    static VkVertexInputBindingDescription getBindingDescription(void);
    static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions(void);
};

class ImageProcessingTest : public vkt::TestCase
{
public:
    ImageProcessingTest(TestContext &testCtx, const std::string &name, const TestParams &testParams);
    virtual ~ImageProcessingTest(void);
    virtual void checkSupport(Context &context) const;

protected:
    TestParams m_params;
};

class ImageProcessingTestInstance : public vkt::TestInstance
{
public:
    ImageProcessingTestInstance(Context &context, const TestParams &params);
    ~ImageProcessingTestInstance(void);

protected:
    Move<VkSampler> makeSampler(const bool unnorm, const VkSamplerAddressMode addrMode,
                                const SamplerReductionMode reductionMode);

    TestStatus verifyResult(const Vec4 &referenceError, const Vec4 &resultError,
                            const ConstPixelBufferAccess &referenceAccess, const ConstPixelBufferAccess &resultAccess,
                            const Vec4 errorThreshold);

    const TestParams m_params;
    de::Random m_rnd;
};

} // namespace ImageProcessing
} // namespace vkt

#endif // _VKTIMAGEPROCESSINGBASE_HPP
