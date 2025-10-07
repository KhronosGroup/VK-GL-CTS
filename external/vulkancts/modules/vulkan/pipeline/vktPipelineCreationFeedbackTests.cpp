/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2019 The Khronos Group Inc.
 * Copyright (c) 2019 Valve Corporation.
 * Copyright (c) 2023 LunarG, Inc.
 * Copyright (c) 2023 Nintendo
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
 * \brief Pipeline Creation Feedback Tests
 *//*--------------------------------------------------------------------*/

#include "vktPipelineCreationFeedbackTests.hpp"
#include "vktPipelineVertexUtil.hpp"
#include "vktTestCase.hpp"
#include "vktTestCaseUtil.hpp"
#include "vkPipelineBinaryUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkRefUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkObjUtil.hpp"
#include "tcuTestLog.hpp"

#include <sstream>
#include <vector>
#include <memory>

namespace vkt
{
namespace pipeline
{

using namespace vk;

namespace
{
enum
{
    VK_MAX_SHADER_STAGES = 6,
};

enum
{
    VK_MAX_PIPELINE_PARTS = 5, // 4 parts + 1 final
};

enum
{
    PIPELINE_NDX_NO_BLOBS   = 0,
    PIPELINE_NDX_DERIVATIVE = 1,
    PIPELINE_NDX_USE_BLOBS  = 2,
    PIPELINE_NDX_COUNT,
};

enum class TestMode
{
    CACHE = 0,
    BINARY
};

// helper functions

std::string getShaderFlagStr(const VkShaderStageFlags shader, bool isDescription)
{
    std::ostringstream desc;
    if (shader & VK_SHADER_STAGE_COMPUTE_BIT)
    {
        desc << ((isDescription) ? "compute stage" : "compute_stage");
    }
    else
    {
        desc << ((isDescription) ? "vertex stage" : "vertex_stage");
        if (shader & VK_SHADER_STAGE_GEOMETRY_BIT)
            desc << ((isDescription) ? " geometry stage" : "_geometry_stage");
        if (shader & VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT)
            desc << ((isDescription) ? " tessellation control stage" : "_tessellation_control_stage");
        if (shader & VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT)
            desc << ((isDescription) ? " tessellation evaluation stage" : "_tessellation_evaluation_stage");
        desc << ((isDescription) ? " fragment stage" : "_fragment_stage");
    }

    return desc.str();
}

std::string getCaseStr(const uint32_t ndx)
{
    switch (ndx)
    {
    case PIPELINE_NDX_NO_BLOBS:
        return "No cached pipeline";
    case PIPELINE_NDX_USE_BLOBS:
        return "Cached pipeline";
    case PIPELINE_NDX_DERIVATIVE:
        return "Pipeline derivative";
    default:
        DE_FATAL("Unknown case!");
    }

    return "Unknown case";
}

// helper classes
class TestParam
{
public:
    TestParam(const PipelineConstructionType pipelineConstructionType, const TestMode testMode,
              const VkShaderStageFlags shaders, bool noCache, bool delayedDestroy,
              bool zeroOutFeedbackCount = VK_FALSE);
    virtual ~TestParam(void) = default;
    virtual const std::string generateTestName(void) const;
    PipelineConstructionType getPipelineConstructionType(void) const
    {
        return m_pipelineConstructionType;
    }
    TestMode getMode(void) const
    {
        return m_mode;
    }
    VkShaderStageFlags getShaderFlags(void) const
    {
        return m_shaders;
    }
    bool isCacheDisabled(void) const
    {
        return m_noCache;
    }
    bool isDelayedDestroy(void) const
    {
        return m_delayedDestroy;
    }
    bool isZeroOutFeedbackCount(void) const
    {
        return m_zeroOutFeedbackCount;
    }

protected:
    PipelineConstructionType m_pipelineConstructionType;
    TestMode m_mode;
    VkShaderStageFlags m_shaders;
    bool m_noCache;
    bool m_delayedDestroy;
    bool m_zeroOutFeedbackCount;
};

TestParam::TestParam(const PipelineConstructionType pipelineConstructionType, const TestMode testMode,
                     const VkShaderStageFlags shaders, bool noCache, bool delayedDestroy, bool zeroOutFeedbackCount)
    : m_pipelineConstructionType(pipelineConstructionType)
    , m_mode(testMode)
    , m_shaders(shaders)
    , m_noCache(noCache)
    , m_delayedDestroy(delayedDestroy)
    , m_zeroOutFeedbackCount(zeroOutFeedbackCount)
{
}

const std::string TestParam::generateTestName(void) const
{
    const std::string cacheString[]{"", "_no_cache"};
    const std::string delayedDestroyString[]{"", "_delayed_destroy"};
    const std::string zeroOutFeedbackCoutString[]{"", "_zero_out_feedback_cout"};
    const uint32_t cacheIndex(m_noCache && (m_mode == TestMode::CACHE));

    return getShaderFlagStr(m_shaders, false) + cacheString[cacheIndex] +
           delayedDestroyString[m_delayedDestroy ? 1 : 0] + zeroOutFeedbackCoutString[m_zeroOutFeedbackCount ? 1 : 0];
}

template <class Test>
vkt::TestCase *newTestCase(tcu::TestContext &testContext, const TestParam *testParam)
{
    return new Test(testContext, testParam->generateTestName().c_str(), testParam);
}

// Test Classes
class BaseTestCase : public vkt::TestCase
{
public:
    BaseTestCase(tcu::TestContext &testContext, const std::string &name, const TestParam *param)
        : vkt::TestCase(testContext, name)
        , m_param(*param)
    {
    }
    virtual ~BaseTestCase(void) = default;
    virtual void checkSupport(Context &context) const;

protected:
    const TestParam m_param;
};

void BaseTestCase::checkSupport(Context &context) const
{
    context.requireDeviceFunctionality("VK_EXT_pipeline_creation_feedback");
    if (m_param.getMode() == TestMode::BINARY)
        context.requireDeviceFunctionality("VK_KHR_pipeline_binary");
}

class BaseTestInstance : public vkt::TestInstance
{
public:
    BaseTestInstance(Context &context, const TestParam *param);
    virtual ~BaseTestInstance(void) = default;
    virtual tcu::TestStatus iterate(void);

protected:
    virtual tcu::TestStatus verifyTestResult(void) = 0;

protected:
    const TestParam *m_param;

    // cache is only used when m_mode is set to TestMode::CACHE
    Move<VkPipelineCache> m_cache;

    // binary related structures are used when m_mode is set to TestMode::BINARIES
    PipelineBinaryWrapper m_binaries[5];
};

BaseTestInstance::BaseTestInstance(Context &context, const TestParam *param)
    : TestInstance(context)
    , m_param(param)
    , m_binaries{
          {context.getDeviceInterface(), context.getDevice()}, {context.getDeviceInterface(), context.getDevice()},
          {context.getDeviceInterface(), context.getDevice()}, {context.getDeviceInterface(), context.getDevice()},
          {context.getDeviceInterface(), context.getDevice()},
      }
{
    const DeviceInterface &vk = m_context.getDeviceInterface();
    const VkDevice vkDevice   = m_context.getDevice();

    if ((m_param->getMode() == TestMode::CACHE) && (m_param->isCacheDisabled() == false))
    {
        const VkPipelineCacheCreateInfo pipelineCacheCreateInfo{
            VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO, // VkStructureType sType;
            nullptr,                                      // const void* pNext;
            0u,                                           // VkPipelineCacheCreateFlags flags;
            0u,                                           // uintptr_t initialDataSize;
            nullptr,                                      // const void* pInitialData;
        };

        m_cache = createPipelineCache(vk, vkDevice, &pipelineCacheCreateInfo);
    }
}

tcu::TestStatus BaseTestInstance::iterate(void)
{
    return verifyTestResult();
}

class GraphicsTestCase : public BaseTestCase
{
public:
    GraphicsTestCase(tcu::TestContext &testContext, const std::string &name, const TestParam *param)
        : BaseTestCase(testContext, name, param)
    {
    }
    virtual ~GraphicsTestCase(void) = default;
    virtual void initPrograms(SourceCollections &programCollection) const;
    virtual void checkSupport(Context &context) const;
    virtual TestInstance *createInstance(Context &context) const;
};

class GraphicsTestInstance : public BaseTestInstance
{
public:
    GraphicsTestInstance(Context &context, const TestParam *param);
    virtual ~GraphicsTestInstance(void) = default;

    using GraphicsPipelinePtr = std::unique_ptr<GraphicsPipelineWrapper>;

protected:
    void preparePipelineWrapper(GraphicsPipelineWrapper &gpw, ShaderWrapper vertShaderModule,
                                ShaderWrapper tescShaderModule, ShaderWrapper teseShaderModule,
                                ShaderWrapper geomShaderModule, ShaderWrapper fragShaderModule,
                                VkPipelineCreationFeedbackEXT *pipelineCreationFeedback, bool *pipelineCreationIsHeavy,
                                VkPipelineCreationFeedbackEXT *pipelineStageCreationFeedbacks,
                                VkPipeline basePipelineHandle, VkBool32 zeroOutFeedbackCount,
                                VkPipelineBinaryInfoKHR *monolithicBinaryInfo           = nullptr,
                                VkPipelineBinaryInfoKHR *vertexPartBinaryInfo           = nullptr,
                                VkPipelineBinaryInfoKHR *preRasterizationPartBinaryInfo = nullptr,
                                VkPipelineBinaryInfoKHR *fragmentShaderBinaryInfo       = nullptr,
                                VkPipelineBinaryInfoKHR *fragmentOutputBinaryInfo       = nullptr);
    virtual tcu::TestStatus verifyTestResult(void);
    void clearFeedbacks(void);

protected:
    const tcu::UVec2 m_renderSize;
    const VkFormat m_colorFormat;
    const VkFormat m_depthFormat;
    PipelineLayoutWrapper m_pipelineLayout;
    RenderPassWrapper m_renderPass;

    GraphicsPipelinePtr m_pipeline[PIPELINE_NDX_COUNT];
    VkPipelineCreationFeedbackEXT
        m_pipelineCreationFeedback[static_cast<int>(VK_MAX_PIPELINE_PARTS) * static_cast<int>(PIPELINE_NDX_COUNT)];
    bool m_pipelineCreationIsHeavy[static_cast<int>(VK_MAX_PIPELINE_PARTS) * static_cast<int>(PIPELINE_NDX_COUNT)];
    VkPipelineCreationFeedbackEXT
        m_pipelineStageCreationFeedbacks[static_cast<int>(PIPELINE_NDX_COUNT) * static_cast<int>(VK_MAX_SHADER_STAGES)];
};

void GraphicsTestCase::initPrograms(SourceCollections &programCollection) const
{
    programCollection.glslSources.add("color_vert_1")
        << glu::VertexSource("#version 310 es\n"
                             "layout(location = 0) in vec4 position;\n"
                             "layout(location = 1) in vec4 color;\n"
                             "layout(location = 0) out highp vec4 vtxColor;\n"
                             "void main (void)\n"
                             "{\n"
                             "  gl_Position = position;\n"
                             "  vtxColor = color;\n"
                             "}\n");
    programCollection.glslSources.add("color_vert_2")
        << glu::VertexSource("#version 310 es\n"
                             "layout(location = 0) in vec4 position;\n"
                             "layout(location = 1) in vec4 color;\n"
                             "layout(location = 0) out highp vec4 vtxColor;\n"
                             "void main (void)\n"
                             "{\n"
                             "  gl_Position = position;\n"
                             "  gl_PointSize = 1.0f;\n"
                             "  vtxColor = color + vec4(0.1, 0.2, 0.3, 0.0);\n"
                             "}\n");
    programCollection.glslSources.add("color_frag")
        << glu::FragmentSource("#version 310 es\n"
                               "layout(location = 0) in highp vec4 vtxColor;\n"
                               "layout(location = 0) out highp vec4 fragColor;\n"
                               "void main (void)\n"
                               "{\n"
                               "  fragColor = vtxColor;\n"
                               "}\n");

    VkShaderStageFlags shaderFlag = m_param.getShaderFlags();
    if (shaderFlag & VK_SHADER_STAGE_GEOMETRY_BIT)
    {
        programCollection.glslSources.add("unused_geo")
            << glu::GeometrySource("#version 450 \n"
                                   "layout(triangles) in;\n"
                                   "layout(triangle_strip, max_vertices = 3) out;\n"
                                   "layout(location = 0) in highp vec4 in_vtxColor[];\n"
                                   "layout(location = 0) out highp vec4 vtxColor;\n"
                                   "out gl_PerVertex { vec4 gl_Position; float gl_PointSize; };\n"
                                   "in gl_PerVertex { vec4 gl_Position; float gl_PointSize; } gl_in[];\n"
                                   "void main (void)\n"
                                   "{\n"
                                   "  for(int ndx=0; ndx<3; ndx++)\n"
                                   "  {\n"
                                   "    gl_Position = gl_in[ndx].gl_Position;\n"
                                   "    vtxColor    = in_vtxColor[ndx];\n"
                                   "    EmitVertex();\n"
                                   "  }\n"
                                   "  EndPrimitive();\n"
                                   "}\n");
    }

    if (shaderFlag & VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT)
    {
        programCollection.glslSources.add("basic_tcs") << glu::TessellationControlSource(
            "#version 450 \n"
            "layout(vertices = 3) out;\n"
            "layout(location = 0) in highp vec4 color[];\n"
            "layout(location = 0) out highp vec4 vtxColor[];\n"
            "out gl_PerVertex { vec4 gl_Position; float gl_PointSize; } gl_out[3];\n"
            "in gl_PerVertex { vec4 gl_Position; float gl_PointSize; } gl_in[gl_MaxPatchVertices];\n"
            "void main()\n"
            "{\n"
            "  gl_TessLevelOuter[0] = 4.0;\n"
            "  gl_TessLevelOuter[1] = 4.0;\n"
            "  gl_TessLevelOuter[2] = 4.0;\n"
            "  gl_TessLevelInner[0] = 4.0;\n"
            "  gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;\n"
            "  vtxColor[gl_InvocationID] = color[gl_InvocationID];\n"
            "}\n");
    }

    if (shaderFlag & VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT)
    {
        programCollection.glslSources.add("basic_tes") << glu::TessellationEvaluationSource(
            "#version 450 \n"
            "layout(triangles, fractional_even_spacing, ccw) in;\n"
            "layout(location = 0) in highp vec4 colors[];\n"
            "layout(location = 0) out highp vec4 vtxColor;\n"
            "out gl_PerVertex { vec4 gl_Position; float gl_PointSize; };\n"
            "in gl_PerVertex { vec4 gl_Position; float gl_PointSize; } gl_in[gl_MaxPatchVertices];\n"
            "void main() \n"
            "{\n"
            "  float u = gl_TessCoord.x;\n"
            "  float v = gl_TessCoord.y;\n"
            "  float w = gl_TessCoord.z;\n"
            "  vec4 pos = vec4(0);\n"
            "  vec4 color = vec4(0);\n"
            "  pos.xyz += u * gl_in[0].gl_Position.xyz;\n"
            "  color.xyz += u * colors[0].xyz;\n"
            "  pos.xyz += v * gl_in[1].gl_Position.xyz;\n"
            "  color.xyz += v * colors[1].xyz;\n"
            "  pos.xyz += w * gl_in[2].gl_Position.xyz;\n"
            "  color.xyz += w * colors[2].xyz;\n"
            "  pos.w = 1.0;\n"
            "  color.w = 1.0;\n"
            "  gl_Position = pos;\n"
            "  vtxColor = color;\n"
            "}\n");
    }
}

void GraphicsTestCase::checkSupport(Context &context) const
{
    if (m_param.getShaderFlags() & VK_SHADER_STAGE_GEOMETRY_BIT)
        context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_GEOMETRY_SHADER);
    if ((m_param.getShaderFlags() & VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT) ||
        (m_param.getShaderFlags() & VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT))
        context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_TESSELLATION_SHADER);

    if (m_param.getMode() == TestMode::BINARY)
        context.requireDeviceFunctionality("VK_KHR_pipeline_binary");

    checkPipelineConstructionRequirements(context.getInstanceInterface(), context.getPhysicalDevice(),
                                          m_param.getPipelineConstructionType());
}

TestInstance *GraphicsTestCase::createInstance(Context &context) const
{
    return new GraphicsTestInstance(context, &m_param);
}

GraphicsTestInstance::GraphicsTestInstance(Context &context, const TestParam *param)
    : BaseTestInstance(context, param)
    , m_renderSize(32u, 32u)
    , m_colorFormat(VK_FORMAT_R8G8B8A8_UNORM)
    , m_depthFormat(VK_FORMAT_D16_UNORM)
{
    const DeviceInterface &vk = m_context.getDeviceInterface();
    const VkDevice vkDevice   = m_context.getDevice();

    PipelineConstructionType pipelineConstructionTypeForUseBlobs = param->getPipelineConstructionType();

    m_pipeline[PIPELINE_NDX_NO_BLOBS]   = GraphicsPipelinePtr(new GraphicsPipelineWrapper(
        context.getInstanceInterface(), context.getDeviceInterface(), context.getPhysicalDevice(), context.getDevice(),
        context.getDeviceExtensions(), param->getPipelineConstructionType(), VK_PIPELINE_CREATE_ALLOW_DERIVATIVES_BIT));
    m_pipeline[PIPELINE_NDX_DERIVATIVE] = GraphicsPipelinePtr(new GraphicsPipelineWrapper(
        context.getInstanceInterface(), context.getDeviceInterface(), context.getPhysicalDevice(), context.getDevice(),
        context.getDeviceExtensions(), pipelineConstructionTypeForUseBlobs, VK_PIPELINE_CREATE_DERIVATIVE_BIT));
    m_pipeline[PIPELINE_NDX_USE_BLOBS]  = GraphicsPipelinePtr(new GraphicsPipelineWrapper(
        context.getInstanceInterface(), context.getDeviceInterface(), context.getPhysicalDevice(), context.getDevice(),
        context.getDeviceExtensions(), pipelineConstructionTypeForUseBlobs, VK_PIPELINE_CREATE_ALLOW_DERIVATIVES_BIT));

    // Create pipeline layout
    {
        const VkPipelineLayoutCreateInfo pipelineLayoutParams = {
            VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, // VkStructureType sType;
            nullptr,                                       // const void* pNext;
            0u,                                            // VkPipelineLayoutCreateFlags flags;
            0u,                                            // uint32_t setLayoutCount;
            nullptr,                                       // const VkDescriptorSetLayout* pSetLayouts;
            0u,                                            // uint32_t pushConstantRangeCount;
            nullptr                                        // const VkPushConstantRange* pPushConstantRanges;
        };

        m_pipelineLayout =
            PipelineLayoutWrapper(m_param->getPipelineConstructionType(), vk, vkDevice, &pipelineLayoutParams);
    }

    // Create render pass
    m_renderPass =
        RenderPassWrapper(m_param->getPipelineConstructionType(), vk, vkDevice, m_colorFormat, m_depthFormat);

    // Create shader modules
    ShaderWrapper vertShaderModule1(vk, vkDevice, context.getBinaryCollection().get("color_vert_1"));
    ShaderWrapper vertShaderModule2(vk, vkDevice, context.getBinaryCollection().get("color_vert_2"));
    ShaderWrapper fragShaderModule(vk, vkDevice, context.getBinaryCollection().get("color_frag"));
    ShaderWrapper tescShaderModule;
    ShaderWrapper teseShaderModule;
    ShaderWrapper geomShaderModule;

    VkShaderStageFlags shaderFlags = m_param->getShaderFlags();
    if (shaderFlags & VK_SHADER_STAGE_GEOMETRY_BIT)
        geomShaderModule = ShaderWrapper(vk, vkDevice, context.getBinaryCollection().get("unused_geo"));
    if (shaderFlags & VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT)
        tescShaderModule = ShaderWrapper(vk, vkDevice, context.getBinaryCollection().get("basic_tcs"));
    if (shaderFlags & VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT)
        teseShaderModule = ShaderWrapper(vk, vkDevice, context.getBinaryCollection().get("basic_tes"));

    if (param->getMode() == TestMode::CACHE)
    {
        for (uint32_t ndx = 0; ndx < PIPELINE_NDX_COUNT; ndx++)
        {
            ShaderWrapper &vertShaderModule = (ndx == PIPELINE_NDX_DERIVATIVE) ? vertShaderModule2 : vertShaderModule1;

            if (ndx == PIPELINE_NDX_USE_BLOBS && !param->isDelayedDestroy() &&
                m_pipeline[PIPELINE_NDX_NO_BLOBS]->wasBuild())
            {
                // Destroy the NO_BLOBS pipeline to check that the cached one really hits cache,
                // except for the case where we're testing cache hit of a pipeline still active.
                m_pipeline[PIPELINE_NDX_NO_BLOBS]->destroyPipeline();
            }

            clearFeedbacks();

            VkPipeline basePipeline =
                (ndx == PIPELINE_NDX_DERIVATIVE && m_pipeline[PIPELINE_NDX_NO_BLOBS]->wasBuild()) ?
                    m_pipeline[PIPELINE_NDX_NO_BLOBS]->getPipeline() :
                    VK_NULL_HANDLE;

            preparePipelineWrapper(*m_pipeline[ndx], vertShaderModule, tescShaderModule, teseShaderModule,
                                   geomShaderModule, fragShaderModule,
                                   &m_pipelineCreationFeedback[VK_MAX_PIPELINE_PARTS * ndx],
                                   &m_pipelineCreationIsHeavy[VK_MAX_PIPELINE_PARTS * ndx],
                                   &m_pipelineStageCreationFeedbacks[VK_MAX_SHADER_STAGES * ndx], basePipeline,
                                   param->isZeroOutFeedbackCount());

            if (ndx != PIPELINE_NDX_NO_BLOBS)
            {
                // Destroy the pipeline as soon as it is created, except the NO_BLOBS because
                // it is needed as a base pipeline for the derivative case.
                if (m_pipeline[ndx]->wasBuild())
                    m_pipeline[ndx]->destroyPipeline();

                if (ndx == PIPELINE_NDX_USE_BLOBS && param->isDelayedDestroy() &&
                    m_pipeline[PIPELINE_NDX_NO_BLOBS]->wasBuild())
                {
                    // Destroy the pipeline we didn't destroy earlier for the isDelayedDestroy case.
                    m_pipeline[PIPELINE_NDX_NO_BLOBS]->destroyPipeline();
                }
            }
        }
    }
    else
    {
        // Repeat the algorithm that was used in section above for TestMode::CACHE but with unwinded loop as the code will be cleaner then

        clearFeedbacks();

        // Create pipeline that is used to create binaries
        m_pipeline[PIPELINE_NDX_NO_BLOBS]->setPipelineCreateFlags2(VK_PIPELINE_CREATE_2_ALLOW_DERIVATIVES_BIT_KHR |
                                                                   VK_PIPELINE_CREATE_2_CAPTURE_DATA_BIT_KHR);
        preparePipelineWrapper(*m_pipeline[PIPELINE_NDX_NO_BLOBS], vertShaderModule1, tescShaderModule,
                               teseShaderModule, geomShaderModule, fragShaderModule, &m_pipelineCreationFeedback[0],
                               &m_pipelineCreationIsHeavy[0], &m_pipelineStageCreationFeedbacks[0], VK_NULL_HANDLE,
                               param->isZeroOutFeedbackCount());

        if (m_param->getPipelineConstructionType() == PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC)
        {
            m_binaries[0].createPipelineBinariesFromPipeline(m_pipeline[PIPELINE_NDX_NO_BLOBS]->getPipeline());
            VkPipelineBinaryInfoKHR pipelineBinaryInfo = m_binaries[0].preparePipelineBinaryInfo();

            // Create derivative pipeline that also uses binaries
            VkPipeline basePipeline = (m_pipeline[PIPELINE_NDX_NO_BLOBS]->wasBuild()) ?
                                          m_pipeline[PIPELINE_NDX_NO_BLOBS]->getPipeline() :
                                          VK_NULL_HANDLE;
            preparePipelineWrapper(*m_pipeline[PIPELINE_NDX_DERIVATIVE], vertShaderModule2, tescShaderModule,
                                   teseShaderModule, geomShaderModule, fragShaderModule,
                                   &m_pipelineCreationFeedback[VK_MAX_PIPELINE_PARTS],
                                   &m_pipelineCreationIsHeavy[VK_MAX_PIPELINE_PARTS],
                                   &m_pipelineStageCreationFeedbacks[VK_MAX_SHADER_STAGES], basePipeline,
                                   param->isZeroOutFeedbackCount(), &pipelineBinaryInfo);

            // Destroy second pipeline as soon as it was created
            if (m_pipeline[PIPELINE_NDX_DERIVATIVE]->wasBuild())
                m_pipeline[PIPELINE_NDX_DERIVATIVE]->destroyPipeline();

            if (!param->isDelayedDestroy() && m_pipeline[PIPELINE_NDX_NO_BLOBS]->wasBuild())
            {
                // Destroy the NO_BLOBS pipeline to check that the cached one really hits cache,
                // except for the case where we're testing cache hit of a pipeline still active.
                m_pipeline[PIPELINE_NDX_NO_BLOBS]->destroyPipeline();
            }

            // Create third pipeline that just uses binaries
            preparePipelineWrapper(*m_pipeline[PIPELINE_NDX_USE_BLOBS], vertShaderModule1, tescShaderModule,
                                   teseShaderModule, geomShaderModule, fragShaderModule,
                                   &m_pipelineCreationFeedback[VK_MAX_PIPELINE_PARTS * 2],
                                   &m_pipelineCreationIsHeavy[VK_MAX_PIPELINE_PARTS * 2],
                                   &m_pipelineStageCreationFeedbacks[VK_MAX_SHADER_STAGES * 2], VK_NULL_HANDLE,
                                   param->isZeroOutFeedbackCount(), &pipelineBinaryInfo);

            // Destroy third pipeline as soon as it was created
            if (m_pipeline[PIPELINE_NDX_USE_BLOBS]->wasBuild())
                m_pipeline[PIPELINE_NDX_USE_BLOBS]->destroyPipeline();

            if (param->isDelayedDestroy() && m_pipeline[PIPELINE_NDX_NO_BLOBS]->wasBuild())
            {
                // Destroy the NO_BLOBS pipeline to check that the cached one really hits cache,
                // except for the case where we're testing cache hit of a pipeline still active.
                m_pipeline[PIPELINE_NDX_NO_BLOBS]->destroyPipeline();
            }
        }
        else
        {
            VkPipelineBinaryInfoKHR pipelinePartBinaryInfo[5];
            VkPipelineBinaryInfoKHR *binaryInfoPtr[5];
            deMemset(binaryInfoPtr, 0, 5 * sizeof(nullptr));

            // Binaries for the final linked pipeline, which could have more optimized binaries.
            m_binaries[0].createPipelineBinariesFromPipeline(m_pipeline[PIPELINE_NDX_NO_BLOBS]->getPipeline());
            if (m_binaries[0].getBinariesCount() > 0U)
            {
                pipelinePartBinaryInfo[0] = m_binaries[0].preparePipelineBinaryInfo();
                binaryInfoPtr[0]          = &pipelinePartBinaryInfo[0];
            }

            // Binaries for the pipeline libraries.
            for (uint32_t i = 0; i < 4; ++i)
            {
                VkPipeline partialPipeline = m_pipeline[PIPELINE_NDX_NO_BLOBS]->getPartialPipeline(i);
                m_binaries[1 + i].createPipelineBinariesFromPipeline(partialPipeline);
                if (m_binaries[1 + i].getBinariesCount() == 0)
                    continue;

                pipelinePartBinaryInfo[1 + i] = m_binaries[1 + i].preparePipelineBinaryInfo();
                binaryInfoPtr[1 + i]          = &pipelinePartBinaryInfo[1 + i];
            }

            // Create derivative pipeline that also uses binaries
            VkPipeline basePipeline = (m_pipeline[PIPELINE_NDX_NO_BLOBS]->wasBuild()) ?
                                          m_pipeline[PIPELINE_NDX_NO_BLOBS]->getPipeline() :
                                          VK_NULL_HANDLE;
            preparePipelineWrapper(
                *m_pipeline[PIPELINE_NDX_DERIVATIVE], vertShaderModule2, tescShaderModule, teseShaderModule,
                geomShaderModule, fragShaderModule, &m_pipelineCreationFeedback[VK_MAX_PIPELINE_PARTS],
                &m_pipelineCreationIsHeavy[VK_MAX_PIPELINE_PARTS],
                &m_pipelineStageCreationFeedbacks[VK_MAX_SHADER_STAGES], basePipeline, param->isZeroOutFeedbackCount(),
                binaryInfoPtr[0], binaryInfoPtr[1], binaryInfoPtr[2], binaryInfoPtr[3], binaryInfoPtr[4]);

            // Destroy second pipeline as soon as it was created
            if (m_pipeline[PIPELINE_NDX_DERIVATIVE]->wasBuild())
                m_pipeline[PIPELINE_NDX_DERIVATIVE]->destroyPipeline();

            if (!param->isDelayedDestroy() && m_pipeline[PIPELINE_NDX_NO_BLOBS]->wasBuild())
            {
                // Destroy the NO_BLOBS pipeline to check that the cached one really hits cache,
                // except for the case where we're testing cache hit of a pipeline still active.
                m_pipeline[PIPELINE_NDX_NO_BLOBS]->destroyPipeline();
            }

            // Create third pipeline that just uses binaries
            preparePipelineWrapper(*m_pipeline[PIPELINE_NDX_USE_BLOBS], vertShaderModule1, tescShaderModule,
                                   teseShaderModule, geomShaderModule, fragShaderModule,
                                   &m_pipelineCreationFeedback[VK_MAX_PIPELINE_PARTS * 2],
                                   &m_pipelineCreationIsHeavy[VK_MAX_PIPELINE_PARTS * 2],
                                   &m_pipelineStageCreationFeedbacks[VK_MAX_SHADER_STAGES * 2], VK_NULL_HANDLE,
                                   param->isZeroOutFeedbackCount(), binaryInfoPtr[0], binaryInfoPtr[1],
                                   binaryInfoPtr[2], binaryInfoPtr[3], binaryInfoPtr[4]);

            // Destroy third pipeline as soon as it was created
            if (m_pipeline[PIPELINE_NDX_USE_BLOBS]->wasBuild())
                m_pipeline[PIPELINE_NDX_USE_BLOBS]->destroyPipeline();

            if (param->isDelayedDestroy() && m_pipeline[PIPELINE_NDX_NO_BLOBS]->wasBuild())
            {
                // Destroy the NO_BLOBS pipeline to check that the cached one really hits cache,
                // except for the case where we're testing cache hit of a pipeline still active.
                m_pipeline[PIPELINE_NDX_NO_BLOBS]->destroyPipeline();
            }
        }
    }
}

void GraphicsTestInstance::preparePipelineWrapper(
    GraphicsPipelineWrapper &gpw, ShaderWrapper vertShaderModule, ShaderWrapper tescShaderModule,
    ShaderWrapper teseShaderModule, ShaderWrapper geomShaderModule, ShaderWrapper fragShaderModule,
    VkPipelineCreationFeedbackEXT *pipelineCreationFeedback, bool *pipelineCreationIsHeavy,
    VkPipelineCreationFeedbackEXT *pipelineStageCreationFeedbacks, VkPipeline basePipelineHandle,
    VkBool32 zeroOutFeedbackCount, VkPipelineBinaryInfoKHR *monolithicBinaryInfo,
    VkPipelineBinaryInfoKHR *vertexPartBinaryInfo, VkPipelineBinaryInfoKHR *preRasterizationPartBinaryInfo,
    VkPipelineBinaryInfoKHR *fragmentShaderBinaryInfo, VkPipelineBinaryInfoKHR *fragmentOutputBinaryInfo)
{
    const VkVertexInputBindingDescription vertexInputBindingDescription{
        0u,                          // uint32_t binding;
        sizeof(Vertex4RGBA),         // uint32_t strideInBytes;
        VK_VERTEX_INPUT_RATE_VERTEX, // VkVertexInputRate inputRate;
    };

    const VkVertexInputAttributeDescription vertexInputAttributeDescriptions[2]{
        {
            0u,                            // uint32_t location;
            0u,                            // uint32_t binding;
            VK_FORMAT_R32G32B32A32_SFLOAT, // VkFormat format;
            0u                             // uint32_t offsetInBytes;
        },
        {
            1u,                            // uint32_t location;
            0u,                            // uint32_t binding;
            VK_FORMAT_R32G32B32A32_SFLOAT, // VkFormat format;
            offsetof(Vertex4RGBA, color),  // uint32_t offsetInBytes;
        }};

    const VkPipelineVertexInputStateCreateInfo vertexInputStateParams{
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, // VkStructureType sType;
        nullptr,                                                   // const void* pNext;
        0u,                                                        // VkPipelineVertexInputStateCreateFlags flags;
        1u,                                                        // uint32_t vertexBindingDescriptionCount;
        &vertexInputBindingDescription,   // const VkVertexInputBindingDescription* pVertexBindingDescriptions;
        2u,                               // uint32_t vertexAttributeDescriptionCount;
        vertexInputAttributeDescriptions, // const VkVertexInputAttributeDescription* pVertexAttributeDescriptions;
    };

    const std::vector<VkViewport> viewport{makeViewport(m_renderSize)};
    const std::vector<VkRect2D> scissor{makeRect2D(m_renderSize)};

    const VkPipelineColorBlendAttachmentState colorBlendAttachmentState{
        VK_FALSE,             // VkBool32 blendEnable;
        VK_BLEND_FACTOR_ONE,  // VkBlendFactor srcColorBlendFactor;
        VK_BLEND_FACTOR_ZERO, // VkBlendFactor dstColorBlendFactor;
        VK_BLEND_OP_ADD,      // VkBlendOp colorBlendOp;
        VK_BLEND_FACTOR_ONE,  // VkBlendFactor srcAlphaBlendFactor;
        VK_BLEND_FACTOR_ZERO, // VkBlendFactor dstAlphaBlendFactor;
        VK_BLEND_OP_ADD,      // VkBlendOp alphaBlendOp;
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |
            VK_COLOR_COMPONENT_A_BIT // VkColorComponentFlags    colorWriteMask;
    };

    const VkPipelineColorBlendStateCreateInfo colorBlendStateParams{
        VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO, // VkStructureType sType;
        nullptr,                                                  // const void* pNext;
        0u,                                                       // VkPipelineColorBlendStateCreateFlags flags;
        VK_FALSE,                                                 // VkBool32 logicOpEnable;
        VK_LOGIC_OP_COPY,                                         // VkLogicOp logicOp;
        1u,                                                       // uint32_t attachmentCount;
        &colorBlendAttachmentState, // const VkPipelineColorBlendAttachmentState* pAttachments;
        {0.0f, 0.0f, 0.0f, 0.0f},   // float blendConst[4];
    };

    VkPipelineDepthStencilStateCreateInfo depthStencilStateParams{
        VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO, // VkStructureType sType;
        nullptr,                                                    // const void* pNext;
        0u,                                                         // VkPipelineDepthStencilStateCreateFlags flags;
        VK_TRUE,                                                    // VkBool32 depthTestEnable;
        VK_TRUE,                                                    // VkBool32 depthWriteEnable;
        VK_COMPARE_OP_LESS_OR_EQUAL,                                // VkCompareOp depthCompareOp;
        VK_FALSE,                                                   // VkBool32 depthBoundsTestEnable;
        VK_FALSE,                                                   // VkBool32 stencilTestEnable;
        // VkStencilOpState front;
        {
            VK_STENCIL_OP_KEEP,  // VkStencilOp failOp;
            VK_STENCIL_OP_KEEP,  // VkStencilOp passOp;
            VK_STENCIL_OP_KEEP,  // VkStencilOp depthFailOp;
            VK_COMPARE_OP_NEVER, // VkCompareOp compareOp;
            0u,                  // uint32_t compareMask;
            0u,                  // uint32_t writeMask;
            0u,                  // uint32_t reference;
        },
        // VkStencilOpState back;
        {
            VK_STENCIL_OP_KEEP,  // VkStencilOp failOp;
            VK_STENCIL_OP_KEEP,  // VkStencilOp passOp;
            VK_STENCIL_OP_KEEP,  // VkStencilOp depthFailOp;
            VK_COMPARE_OP_NEVER, // VkCompareOp compareOp;
            0u,                  // uint32_t compareMask;
            0u,                  // uint32_t writeMask;
            0u,                  // uint32_t reference;
        },
        0.0f, // float minDepthBounds;
        1.0f, // float maxDepthBounds;
    };

    VkPipelineCreationFeedbackCreateInfoEXT pipelineCreationFeedbackCreateInfo[VK_MAX_PIPELINE_PARTS];
    PipelineCreationFeedbackCreateInfoWrapper pipelineCreationFeedbackWrapper[VK_MAX_PIPELINE_PARTS];
    for (uint32_t i = 0u; i < VK_MAX_PIPELINE_PARTS; ++i)
    {
        pipelineCreationFeedbackCreateInfo[i]                           = initVulkanStructure();
        pipelineCreationFeedbackCreateInfo[i].pPipelineCreationFeedback = &pipelineCreationFeedback[i];
        pipelineCreationFeedbackWrapper[i].ptr                          = &pipelineCreationFeedbackCreateInfo[i];

        pipelineCreationIsHeavy[i] = false;
    }

    uint32_t geometryStages = 1u + (geomShaderModule.isSet()) + (tescShaderModule.isSet()) + (teseShaderModule.isSet());
    if (m_param->getPipelineConstructionType() == PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC)
    {
        pipelineCreationFeedbackCreateInfo[4].pipelineStageCreationFeedbackCount =
            zeroOutFeedbackCount ? 0u : (1u + geometryStages);
        pipelineCreationFeedbackCreateInfo[4].pPipelineStageCreationFeedbacks = pipelineStageCreationFeedbacks;

        pipelineCreationIsHeavy[4] = true;
    }
    else
    {
        // setup proper stages count for CreationFeedback structures
        // that will be passed to pre-rasterization and fragment shader states
        pipelineCreationFeedbackCreateInfo[1].pipelineStageCreationFeedbackCount =
            zeroOutFeedbackCount ? 0u : geometryStages;
        pipelineCreationFeedbackCreateInfo[1].pPipelineStageCreationFeedbacks    = pipelineStageCreationFeedbacks;
        pipelineCreationFeedbackCreateInfo[2].pipelineStageCreationFeedbackCount = zeroOutFeedbackCount ? 0u : 1u;
        pipelineCreationFeedbackCreateInfo[2].pPipelineStageCreationFeedbacks =
            pipelineStageCreationFeedbacks + geometryStages;

        pipelineCreationIsHeavy[1] = true;
        pipelineCreationIsHeavy[2] = true;

        if (m_param->getPipelineConstructionType() == PIPELINE_CONSTRUCTION_TYPE_LINK_TIME_OPTIMIZED_LIBRARY)
        {
            pipelineCreationIsHeavy[4] = true;
        }
    }

    // pipelineCreationIsHeavy element 0 and 3 intentionally left false,
    // because these relate to vertex input and fragment output stages, which may be
    // created in nearly zero time.

    gpw.setDefaultTopology((!tescShaderModule.isSet()) ? VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST :
                                                         VK_PRIMITIVE_TOPOLOGY_PATCH_LIST)
        .setDefaultRasterizationState()
        .setDefaultMultisampleState()
        .setMonolithicPipelineLayout(m_pipelineLayout)
        .disableShaderModules(vertexPartBinaryInfo || monolithicBinaryInfo)
        .setupVertexInputState(&vertexInputStateParams, nullptr, *m_cache, pipelineCreationFeedbackWrapper[0],
                               vertexPartBinaryInfo)
        .setupPreRasterizationShaderState3(viewport, scissor, m_pipelineLayout, *m_renderPass, 0u, vertShaderModule, {},
                                           nullptr, tescShaderModule, {}, teseShaderModule, {}, geomShaderModule, {},
                                           nullptr, nullptr, nullptr, nullptr, nullptr, {}, *m_cache,
                                           pipelineCreationFeedbackWrapper[1], preRasterizationPartBinaryInfo)
        .setupFragmentShaderState2(m_pipelineLayout, *m_renderPass, 0u, fragShaderModule, 0, &depthStencilStateParams,
                                   nullptr, nullptr, *m_cache, pipelineCreationFeedbackWrapper[2], {},
                                   fragmentShaderBinaryInfo)
        .setupFragmentOutputState(*m_renderPass, 0u, &colorBlendStateParams, nullptr, *m_cache,
                                  pipelineCreationFeedbackWrapper[3], nullptr, fragmentOutputBinaryInfo)
        .buildPipeline(*m_cache, basePipelineHandle, basePipelineHandle != VK_NULL_HANDLE ? -1 : 0,
                       pipelineCreationFeedbackWrapper[4], monolithicBinaryInfo);
}

tcu::TestStatus GraphicsTestInstance::verifyTestResult(void)
{
    tcu::TestLog &log           = m_context.getTestContext().getLog();
    bool durationZeroWarning    = false;
    bool cachedPipelineWarning  = false;
    bool isMonolithic           = m_param->getPipelineConstructionType() == PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC;
    bool isZeroOutFeedbackCout  = m_param->isZeroOutFeedbackCount();
    uint32_t finalPipelineIndex = uint32_t(VK_MAX_PIPELINE_PARTS) - 1u;
    uint32_t start              = isMonolithic ? finalPipelineIndex : 0u;
    uint32_t step               = start + 1u;

    // Iterate ofer creation feedback for all pipeline parts - if monolithic pipeline is tested then skip (step over) feedback for parts
    for (uint32_t creationFeedbackNdx = start;
         creationFeedbackNdx < static_cast<uint32_t>(VK_MAX_PIPELINE_PARTS) * static_cast<uint32_t>(PIPELINE_NDX_COUNT);
         creationFeedbackNdx += step)
    {
        uint32_t pipelineCacheNdx  = creationFeedbackNdx / uint32_t(VK_MAX_PIPELINE_PARTS);
        auto creationFeedbackFlags = m_pipelineCreationFeedback[creationFeedbackNdx].flags;
        std::string caseString     = getCaseStr(pipelineCacheNdx);
        uint32_t pipelinePartIndex = creationFeedbackNdx % uint32_t(VK_MAX_PIPELINE_PARTS);

        std::ostringstream message;
        message << caseString;
        // Check first that the no cached pipeline was missed in the pipeline cache

        // According to the spec:
        // "An implementation should write pipeline creation feedback to pPipelineCreationFeedback and
        //    may write pipeline stage creation feedback to pPipelineStageCreationFeedbacks."
        if (!(creationFeedbackFlags & VK_PIPELINE_CREATION_FEEDBACK_VALID_BIT_EXT))
        {
            // According to the spec:
            // "If the VK_PIPELINE_CREATION_FEEDBACK_VALID_BIT_EXT is not set in flags, an implementation
            //    must not set any other bits in flags, and all other VkPipelineCreationFeedbackEXT data members are undefined."
            if (m_pipelineCreationFeedback[creationFeedbackNdx].flags)
            {
                std::ostringstream errorMsg;
                errorMsg << ": Creation feedback is not valid but there are other flags written";
                return tcu::TestStatus::fail(errorMsg.str());
            }
            message << "\t\t Pipeline Creation Feedback data is not valid\n";
        }
        else
        {
            if (m_param->isCacheDisabled() &&
                creationFeedbackFlags & VK_PIPELINE_CREATION_FEEDBACK_APPLICATION_PIPELINE_CACHE_HIT_BIT_EXT)
            {
                message << ": feedback indicates pipeline hit cache when it shouldn't";
                return tcu::TestStatus::fail(message.str());
            }

            if (pipelineCacheNdx == PIPELINE_NDX_NO_BLOBS &&
                creationFeedbackFlags & VK_PIPELINE_CREATION_FEEDBACK_APPLICATION_PIPELINE_CACHE_HIT_BIT_EXT)
            {
                message << ": hit the cache when it shouldn't";
                return tcu::TestStatus::fail(message.str());
            }

            if (pipelineCacheNdx != PIPELINE_NDX_DERIVATIVE &&
                creationFeedbackFlags & VK_PIPELINE_CREATION_FEEDBACK_BASE_PIPELINE_ACCELERATION_BIT_EXT)
            {
                message << ": feedback indicates base pipeline acceleration when it shouldn't";
                return tcu::TestStatus::fail(message.str());
            }

            if (pipelineCacheNdx == PIPELINE_NDX_USE_BLOBS && !m_param->isCacheDisabled() &&
                (creationFeedbackFlags & VK_PIPELINE_CREATION_FEEDBACK_APPLICATION_PIPELINE_CACHE_HIT_BIT_EXT) == 0)
            {
                // For graphics pipeline library cache is only hit for the pre_rasterization and fragment_shader stages
                if (isMonolithic || (pipelinePartIndex == 1u) || (pipelinePartIndex == 2u))
                {
                    message << "\nWarning: Cached pipeline case did not hit the cache";
                    cachedPipelineWarning = true;
                }
            }

            if (m_pipelineCreationFeedback[creationFeedbackNdx].duration == 0)
            {
                if (m_pipelineCreationIsHeavy[creationFeedbackNdx])
                {
                    // Emit warnings only for pipelines, that are expected to have large creation times.
                    // Pipelines containing only vertex input or fragment output stages may be created in
                    // time duration less than the timer precision available on given platform.

                    message << "\nWarning: Pipeline creation feedback reports duration spent creating a pipeline was "
                               "zero nanoseconds\n";
                    durationZeroWarning = true;
                }
            }

            message << "\n";
            message << "\t\t Hit cache ? \t\t\t"
                    << (creationFeedbackFlags & VK_PIPELINE_CREATION_FEEDBACK_APPLICATION_PIPELINE_CACHE_HIT_BIT_EXT ?
                            "yes" :
                            "no")
                    << "\n";
            message << "\t\t Base Pipeline Acceleration ? \t"
                    << (creationFeedbackFlags & VK_PIPELINE_CREATION_FEEDBACK_BASE_PIPELINE_ACCELERATION_BIT_EXT ?
                            "yes" :
                            "no")
                    << "\n";
            message << "\t\t Duration (ns): \t\t" << m_pipelineCreationFeedback[creationFeedbackNdx].duration << "\n";
        }

        // dont repeat checking shader feedback for pipeline parts - just check all shaders when checkin final pipelines
        if (pipelinePartIndex == finalPipelineIndex)
        {
            VkShaderStageFlags testedShaderFlags = m_param->getShaderFlags();
            uint32_t shaderCount                 = 2u + ((testedShaderFlags & VK_SHADER_STAGE_GEOMETRY_BIT) != 0) +
                                   ((testedShaderFlags & VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT) != 0) +
                                   ((testedShaderFlags & VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT) != 0);
            for (uint32_t shader = 0; shader < shaderCount; shader++)
            {
                const uint32_t index = VK_MAX_SHADER_STAGES * pipelineCacheNdx + shader;
                message << "\t" << (shader + 1) << " shader stage\n";

                // According to the spec:
                // "An implementation should write pipeline creation feedback to pPipelineCreationFeedback and
                //      may write pipeline stage creation feedback to pPipelineStageCreationFeedbacks."
                if (m_pipelineStageCreationFeedbacks[index].flags & isZeroOutFeedbackCout)
                {
                    std::ostringstream errorMsg;
                    errorMsg << caseString << ": feedback indicates pipeline " << (shader + 1)
                             << " shader stage feedback was generated despite setting feedback count to zero";
                    return tcu::TestStatus::fail(errorMsg.str());
                }

                if (!(m_pipelineStageCreationFeedbacks[index].flags & VK_PIPELINE_CREATION_FEEDBACK_VALID_BIT_EXT))
                {
                    // According to the spec:
                    // "If the VK_PIPELINE_CREATION_FEEDBACK_VALID_BIT_EXT is not set in flags, an implementation
                    //      must not set any other bits in flags, and all other VkPipelineCreationFeedbackEXT data members are undefined."
                    if (m_pipelineStageCreationFeedbacks[index].flags)
                    {
                        std::ostringstream errorMsg;
                        errorMsg << caseString << ": Creation feedback is not valid for " << (shader + 1)
                                 << " shader stage but there are other flags written";
                        return tcu::TestStatus::fail(errorMsg.str());
                    }
                    message << "\t\t Pipeline Creation Feedback data is not valid\n";
                    continue;
                }
                if (m_param->isCacheDisabled() &&
                    m_pipelineStageCreationFeedbacks[index].flags &
                        VK_PIPELINE_CREATION_FEEDBACK_APPLICATION_PIPELINE_CACHE_HIT_BIT_EXT)
                {
                    std::ostringstream errorMsg;
                    errorMsg << caseString << ": feedback indicates pipeline " << (shader + 1)
                             << " shader stage hit cache when it shouldn't";
                    return tcu::TestStatus::fail(errorMsg.str());
                }

                if (pipelineCacheNdx == PIPELINE_NDX_USE_BLOBS && !m_param->isCacheDisabled() &&
                    (m_pipelineStageCreationFeedbacks[index].flags &
                     VK_PIPELINE_CREATION_FEEDBACK_APPLICATION_PIPELINE_CACHE_HIT_BIT_EXT) == 0)
                {
                    message << "Warning: pipeline stage did not hit the cache\n";
                    cachedPipelineWarning = true;
                }
                if (cachedPipelineWarning && m_pipelineStageCreationFeedbacks[index].flags &
                                                 VK_PIPELINE_CREATION_FEEDBACK_APPLICATION_PIPELINE_CACHE_HIT_BIT_EXT)
                {
                    // We only set the warning when the pipeline nor the pipeline stages hit the cache. If any of them did, them disable the warning.
                    cachedPipelineWarning = false;
                }

                message << "\t\t Hit cache ? \t\t\t"
                        << (m_pipelineStageCreationFeedbacks[index].flags &
                                    VK_PIPELINE_CREATION_FEEDBACK_APPLICATION_PIPELINE_CACHE_HIT_BIT_EXT ?
                                "yes" :
                                "no")
                        << "\n";
                message << "\t\t Base Pipeline Acceleration ? \t"
                        << (m_pipelineStageCreationFeedbacks[index].flags &
                                    VK_PIPELINE_CREATION_FEEDBACK_BASE_PIPELINE_ACCELERATION_BIT_EXT ?
                                "yes" :
                                "no")
                        << "\n";
                message << "\t\t Duration (ns): \t\t" << m_pipelineStageCreationFeedbacks[index].duration << "\n";
            }
        }

        log << tcu::TestLog::Message << message.str() << tcu::TestLog::EndMessage;
    }

    if (cachedPipelineWarning)
    {
        return tcu::TestStatus(QP_TEST_RESULT_QUALITY_WARNING, "Cached pipeline or stage did not hit the cache");
    }
    if (durationZeroWarning)
    {
        return tcu::TestStatus(
            QP_TEST_RESULT_QUALITY_WARNING,
            "Pipeline creation feedback reports duration spent creating a pipeline was zero nanoseconds");
    }
    return tcu::TestStatus::pass("Pass");
}

void GraphicsTestInstance::clearFeedbacks(void)
{
    deMemset(m_pipelineCreationFeedback, 0,
             sizeof(VkPipelineCreationFeedbackEXT) * VK_MAX_PIPELINE_PARTS * PIPELINE_NDX_COUNT);
    deMemset(m_pipelineStageCreationFeedbacks, 0,
             sizeof(VkPipelineCreationFeedbackEXT) * PIPELINE_NDX_COUNT * VK_MAX_SHADER_STAGES);
}

class ComputeTestCase : public BaseTestCase
{
public:
    ComputeTestCase(tcu::TestContext &testContext, const std::string &name, const TestParam *param)
        : BaseTestCase(testContext, name, param)
    {
    }
    virtual ~ComputeTestCase(void) = default;
    virtual void initPrograms(SourceCollections &programCollection) const;
    virtual TestInstance *createInstance(Context &context) const;
};

class ComputeTestInstance : public BaseTestInstance
{
public:
    ComputeTestInstance(Context &context, const TestParam *param);
    virtual ~ComputeTestInstance(void) = default;

protected:
    virtual tcu::TestStatus verifyTestResult(void);
    void buildDescriptorSets(uint32_t ndx);
    void buildShader(uint32_t ndx);
    void buildPipeline(const TestParam *param, uint32_t ndx);

protected:
    Move<VkBuffer> m_inputBuf;
    de::MovePtr<Allocation> m_inputBufferAlloc;
    Move<VkShaderModule> m_computeShaderModule[PIPELINE_NDX_COUNT];

    Move<VkBuffer> m_outputBuf[PIPELINE_NDX_COUNT];
    de::MovePtr<Allocation> m_outputBufferAlloc[PIPELINE_NDX_COUNT];

    Move<VkDescriptorPool> m_descriptorPool[PIPELINE_NDX_COUNT];
    Move<VkDescriptorSetLayout> m_descriptorSetLayout[PIPELINE_NDX_COUNT];
    Move<VkDescriptorSet> m_descriptorSet[PIPELINE_NDX_COUNT];

    Move<VkPipelineLayout> m_pipelineLayout[PIPELINE_NDX_COUNT];
    VkPipeline m_pipeline[PIPELINE_NDX_COUNT];
    VkPipelineCreationFeedbackEXT m_pipelineCreationFeedback[PIPELINE_NDX_COUNT];
    VkPipelineCreationFeedbackEXT m_pipelineStageCreationFeedback[PIPELINE_NDX_COUNT];
};

void ComputeTestCase::initPrograms(SourceCollections &programCollection) const
{
    programCollection.glslSources.add("basic_compute_1") << glu::ComputeSource(
        "#version 310 es\n"
        "layout(local_size_x = 1) in;\n"
        "layout(std430) buffer;\n"
        "layout(binding = 0) readonly buffer Input0\n"
        "{\n"
        "  vec4 elements[];\n"
        "} input_data0;\n"
        "layout(binding = 1) writeonly buffer Output\n"
        "{\n"
        "  vec4 elements[];\n"
        "} output_data;\n"
        "void main()\n"
        "{\n"
        "  uint ident = gl_GlobalInvocationID.x;\n"
        "  output_data.elements[ident] = input_data0.elements[ident] * input_data0.elements[ident];\n"
        "}");
    programCollection.glslSources.add("basic_compute_2")
        << glu::ComputeSource("#version 310 es\n"
                              "layout(local_size_x = 1) in;\n"
                              "layout(std430) buffer;\n"
                              "layout(binding = 0) readonly buffer Input0\n"
                              "{\n"
                              "  vec4 elements[];\n"
                              "} input_data0;\n"
                              "layout(binding = 1) writeonly buffer Output\n"
                              "{\n"
                              "  vec4 elements[];\n"
                              "} output_data;\n"
                              "void main()\n"
                              "{\n"
                              "  uint ident = gl_GlobalInvocationID.x;\n"
                              "  output_data.elements[ident] = input_data0.elements[ident];\n"
                              "}");
}

TestInstance *ComputeTestCase::createInstance(Context &context) const
{
    return new ComputeTestInstance(context, &m_param);
}

void ComputeTestInstance::buildDescriptorSets(uint32_t ndx)
{
    const DeviceInterface &vk = m_context.getDeviceInterface();
    const VkDevice vkDevice   = m_context.getDevice();

    // Create descriptor set layout
    DescriptorSetLayoutBuilder descLayoutBuilder;
    for (uint32_t bindingNdx = 0u; bindingNdx < 2u; bindingNdx++)
        descLayoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
    m_descriptorSetLayout[ndx] = descLayoutBuilder.build(vk, vkDevice);
}

void ComputeTestInstance::buildShader(uint32_t ndx)
{
    const DeviceInterface &vk = m_context.getDeviceInterface();
    const VkDevice vkDevice   = m_context.getDevice();

    std::string shader_name("basic_compute_");

    shader_name += (ndx == PIPELINE_NDX_DERIVATIVE) ? "2" : "1";

    // Create compute shader
    VkShaderModuleCreateInfo shaderModuleCreateInfo = {
        VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,                              // VkStructureType sType;
        nullptr,                                                                  // const void* pNext;
        0u,                                                                       // VkShaderModuleCreateFlags flags;
        m_context.getBinaryCollection().get(shader_name).getSize(),               // uintptr_t codeSize;
        (uint32_t *)m_context.getBinaryCollection().get(shader_name).getBinary(), // const uint32_t* pCode;
    };
    m_computeShaderModule[ndx] = createShaderModule(vk, vkDevice, &shaderModuleCreateInfo);
}

void ComputeTestInstance::buildPipeline(const TestParam *param, uint32_t ndx)
{
    const DeviceInterface &vk           = m_context.getDeviceInterface();
    const VkDevice vkDevice             = m_context.getDevice();
    const VkBool32 zeroOutFeedbackCount = param->isZeroOutFeedbackCount();

    deMemset(&m_pipelineCreationFeedback[ndx], 0, sizeof(VkPipelineCreationFeedbackEXT));
    deMemset(&m_pipelineStageCreationFeedback[ndx], 0, sizeof(VkPipelineCreationFeedbackEXT));

    VkPipelineCreateFlags2CreateInfoKHR pipelineFlags2CreateInfo = initVulkanStructure();
    pipelineFlags2CreateInfo.flags                               = VK_PIPELINE_CREATE_2_CAPTURE_DATA_BIT_KHR;

    VkPipelineCreationFeedbackCreateInfoEXT pipelineCreationFeedbackCreateInfo{
        VK_STRUCTURE_TYPE_PIPELINE_CREATION_FEEDBACK_CREATE_INFO_EXT, // VkStructureType sType;
        nullptr,                                                      // const void * pNext;
        &m_pipelineCreationFeedback[ndx],     // VkPipelineCreationFeedbackEXT* pPipelineCreationFeedback;
        zeroOutFeedbackCount ? 0u : 1u,       // uint32_t pipelineStageCreationFeedbackCount;
        &m_pipelineStageCreationFeedback[ndx] // VkPipelineCreationFeedbackEXT* pPipelineStageCreationFeedbacks;
    };

    // Create compute pipeline layout
    const VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{
        VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, // VkStructureType sType;
        nullptr,                                       // const void* pNext;
        0u,                                            // VkPipelineLayoutCreateFlags flags;
        1u,                                            // uint32_t setLayoutCount;
        &m_descriptorSetLayout[ndx].get(),             // const VkDescriptorSetLayout* pSetLayouts;
        0u,                                            // uint32_t pushConstantRangeCount;
        nullptr,                                       // const VkPushConstantRange* pPushConstantRanges;
    };

    m_pipelineLayout[ndx] = createPipelineLayout(vk, vkDevice, &pipelineLayoutCreateInfo);

    const VkPipelineShaderStageCreateInfo stageCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, // VkStructureType sType;
        nullptr,                                             // const void* pNext;
        0u,                                                  // VkPipelineShaderStageCreateFlags flags;
        VK_SHADER_STAGE_COMPUTE_BIT,                         // VkShaderStageFlagBits stage;
        *m_computeShaderModule[ndx],                         // VkShaderModule module;
        "main",                                              // const char* pName;
        nullptr,                                             // const VkSpecializationInfo* pSpecializationInfo;
    };

    VkComputePipelineCreateInfo pipelineCreateInfo = {
        VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO, // VkStructureType sType;
        &pipelineCreationFeedbackCreateInfo,            // const void* pNext;
        0u,                                             // VkPipelineCreateFlags flags;
        stageCreateInfo,                                // VkPipelineShaderStageCreateInfo stage;
        *m_pipelineLayout[ndx],                         // VkPipelineLayout layout;
        VK_NULL_HANDLE,                                 // VkPipeline basePipelineHandle;
        0u,                                             // int32_t basePipelineIndex;
    };

    if (ndx != PIPELINE_NDX_DERIVATIVE)
    {
        pipelineCreateInfo.flags = VK_PIPELINE_CREATE_ALLOW_DERIVATIVES_BIT;
    }

    if (ndx == PIPELINE_NDX_DERIVATIVE)
    {
        pipelineCreateInfo.flags              = VK_PIPELINE_CREATE_DERIVATIVE_BIT;
        pipelineCreateInfo.basePipelineHandle = m_pipeline[PIPELINE_NDX_NO_BLOBS];
        pipelineCreateInfo.basePipelineIndex  = -1;
    }

    if (ndx == PIPELINE_NDX_USE_BLOBS && !param->isDelayedDestroy())
    {
        // Destroy the NO_BLOBS pipeline to check that the cached/binary one really hits cache,
        // except for the case where we're testing cache hit of a pipeline still active.
        vk.destroyPipeline(vkDevice, m_pipeline[PIPELINE_NDX_NO_BLOBS], nullptr);
    }

    if (m_param->getMode() == TestMode::CACHE)
        vk.createComputePipelines(vkDevice, *m_cache, 1u, &pipelineCreateInfo, nullptr, &m_pipeline[ndx]);
    else
    {
        // we need to switch to using VkPipelineCreateFlags2KHR and also include flags that were specified in pipelineCreateInfo
        pipelineFlags2CreateInfo.flags |= static_cast<VkPipelineCreateFlags2KHR>(pipelineCreateInfo.flags);
        pipelineCreationFeedbackCreateInfo.pNext = &pipelineFlags2CreateInfo;

        if (ndx == PIPELINE_NDX_NO_BLOBS)
        {
            // create pipeline
            vk.createComputePipelines(vkDevice, *m_cache, 1u, &pipelineCreateInfo, nullptr, &m_pipeline[ndx]);

            // prepare pipeline binaries
            m_binaries[0].createPipelineBinariesFromPipeline(m_pipeline[ndx]);
        }
        else
        {
            // create pipeline using binary data and use pipelineCreateInfo with no shader stage
            VkPipelineBinaryInfoKHR pipelineBinaryInfo = m_binaries[0].preparePipelineBinaryInfo();
            pipelineCreateInfo.pNext                   = &pipelineBinaryInfo;
            pipelineCreateInfo.stage.module            = VK_NULL_HANDLE;
            vk.createComputePipelines(vkDevice, *m_cache, 1u, &pipelineCreateInfo, nullptr, &m_pipeline[ndx]);
        }
    }

    if (ndx != PIPELINE_NDX_NO_BLOBS)
    {
        // Destroy the pipeline as soon as it is created, except the NO_BLOBS because
        // it is needed as a base pipeline for the derivative case.
        vk.destroyPipeline(vkDevice, m_pipeline[ndx], nullptr);

        if (ndx == PIPELINE_NDX_USE_BLOBS && param->isDelayedDestroy())
        {
            // Destroy the pipeline we didn't destroy earlier for the isDelayedDestroy case.
            vk.destroyPipeline(vkDevice, m_pipeline[PIPELINE_NDX_NO_BLOBS], nullptr);
        }
    }
}

ComputeTestInstance::ComputeTestInstance(Context &context, const TestParam *param) : BaseTestInstance(context, param)
{
    for (uint32_t ndx = 0; ndx < PIPELINE_NDX_COUNT; ndx++)
    {
        buildDescriptorSets(ndx);
        buildShader(ndx);
        buildPipeline(param, ndx);
    }
}

tcu::TestStatus ComputeTestInstance::verifyTestResult(void)
{
    tcu::TestLog &log          = m_context.getTestContext().getLog();
    bool durationZeroWarning   = false;
    bool cachedPipelineWarning = false;

    for (uint32_t ndx = 0; ndx < PIPELINE_NDX_COUNT; ndx++)
    {
        std::ostringstream message;
        message << getCaseStr(ndx);

        // No need to check per stage status as it is compute pipeline (only one stage) and Vulkan spec mentions that:
        // "One common scenario for an implementation to skip per-stage feedback is when
        // VK_PIPELINE_CREATION_FEEDBACK_APPLICATION_PIPELINE_CACHE_HIT_BIT_EXT is set in pPipelineCreationFeedback."
        //
        // Check first that the no cached pipeline was missed in the pipeline cache

        // According to the spec:
        // "An implementation should write pipeline creation feedback to pPipelineCreationFeedback and
        //    may write pipeline stage creation feedback to pPipelineStageCreationFeedbacks."
        if (!(m_pipelineCreationFeedback[ndx].flags & VK_PIPELINE_CREATION_FEEDBACK_VALID_BIT_EXT))
        {
            // According to the spec:
            // "If the VK_PIPELINE_CREATION_FEEDBACK_VALID_BIT_EXT is not set in flags, an implementation
            //    must not set any other bits in flags, and all other VkPipelineCreationFeedbackEXT data members are undefined."
            if (m_pipelineCreationFeedback[ndx].flags)
            {
                std::ostringstream errorMsg;
                errorMsg << ": Creation feedback is not valid but there are other flags written";
                return tcu::TestStatus::fail(errorMsg.str());
            }
            message << "\t\t Pipeline Creation Feedback data is not valid\n";
        }
        else
        {
            if (m_param->isCacheDisabled() && m_pipelineCreationFeedback[ndx].flags &
                                                  VK_PIPELINE_CREATION_FEEDBACK_APPLICATION_PIPELINE_CACHE_HIT_BIT_EXT)
            {
                message << ": feedback indicates pipeline hit cache when it shouldn't";
                return tcu::TestStatus::fail(message.str());
            }

            if (ndx == PIPELINE_NDX_NO_BLOBS &&
                m_pipelineCreationFeedback[ndx].flags &
                    VK_PIPELINE_CREATION_FEEDBACK_APPLICATION_PIPELINE_CACHE_HIT_BIT_EXT)
            {
                message << ": hit the cache when it shouldn't";
                return tcu::TestStatus::fail(message.str());
            }

            if (!(ndx == PIPELINE_NDX_DERIVATIVE && !m_param->isCacheDisabled()) &&
                m_pipelineCreationFeedback[ndx].flags &
                    VK_PIPELINE_CREATION_FEEDBACK_BASE_PIPELINE_ACCELERATION_BIT_EXT)
            {
                message << ": feedback indicates base pipeline acceleration when it shouldn't";
                return tcu::TestStatus::fail(message.str());
            }

            if (ndx == PIPELINE_NDX_USE_BLOBS && !m_param->isCacheDisabled() &&
                (m_pipelineCreationFeedback[ndx].flags &
                 VK_PIPELINE_CREATION_FEEDBACK_APPLICATION_PIPELINE_CACHE_HIT_BIT_EXT) == 0)
            {
                message << "\nWarning: Cached pipeline case did not hit the cache";
                cachedPipelineWarning = true;
            }

            if (m_pipelineCreationFeedback[ndx].duration == 0)
            {
                message << "\nWarning: Pipeline creation feedback reports duration spent creating a pipeline was zero "
                           "nanoseconds\n";
                durationZeroWarning = true;
            }

            message << "\n";

            message << "\t\t Hit cache ? \t\t\t"
                    << (m_pipelineCreationFeedback[ndx].flags &
                                VK_PIPELINE_CREATION_FEEDBACK_APPLICATION_PIPELINE_CACHE_HIT_BIT_EXT ?
                            "yes" :
                            "no")
                    << "\n";
            message << "\t\t Base Pipeline Acceleration ? \t"
                    << (m_pipelineCreationFeedback[ndx].flags &
                                VK_PIPELINE_CREATION_FEEDBACK_BASE_PIPELINE_ACCELERATION_BIT_EXT ?
                            "yes" :
                            "no")
                    << "\n";
            message << "\t\t Duration (ns): \t\t" << m_pipelineCreationFeedback[ndx].duration << "\n";

            message << "\t Compute Stage\n";
        }

        // According to the spec:
        // "An implementation should write pipeline creation feedback to pPipelineCreationFeedback and
        //    may write pipeline stage creation feedback to pPipelineStageCreationFeedbacks."
        if (!(m_pipelineStageCreationFeedback[ndx].flags & VK_PIPELINE_CREATION_FEEDBACK_VALID_BIT_EXT))
        {
            // According to the spec:
            // "If the VK_PIPELINE_CREATION_FEEDBACK_VALID_BIT_EXT is not set in flags, an implementation
            //    must not set any other bits in flags, and all other VkPipelineCreationFeedbackEXT data members are undefined."
            if (m_pipelineStageCreationFeedback[ndx].flags)
            {
                std::ostringstream errorMsg;
                errorMsg << getCaseStr(ndx)
                         << ": Creation feedback is not valid for compute stage but there are other flags written";
                return tcu::TestStatus::fail(errorMsg.str());
            }
            message << "\t\t Pipeline Creation Feedback data is not valid\n";
        }
        else
        {
            if (m_param->isCacheDisabled() && m_pipelineStageCreationFeedback[ndx].flags &
                                                  VK_PIPELINE_CREATION_FEEDBACK_APPLICATION_PIPELINE_CACHE_HIT_BIT_EXT)
            {
                std::ostringstream errorMsg;
                errorMsg << getCaseStr(ndx)
                         << ": feedback indicates pipeline compute stage hit cache when it shouldn't";
                return tcu::TestStatus::fail(errorMsg.str());
            }

            if (ndx == PIPELINE_NDX_USE_BLOBS && !m_param->isCacheDisabled() &&
                (m_pipelineStageCreationFeedback[ndx].flags &
                 VK_PIPELINE_CREATION_FEEDBACK_APPLICATION_PIPELINE_CACHE_HIT_BIT_EXT) == 0)
            {
                message << "Warning: pipeline stage did not hit the cache\n";
                cachedPipelineWarning = true;
            }
            if (cachedPipelineWarning && m_pipelineStageCreationFeedback[ndx].flags &
                                             VK_PIPELINE_CREATION_FEEDBACK_APPLICATION_PIPELINE_CACHE_HIT_BIT_EXT)
            {
                // We only set the warning when the pipeline nor the pipeline stages hit the cache. If any of them did, them disable the warning.
                cachedPipelineWarning = false;
            }

            message << "\t\t Hit cache ? \t\t\t"
                    << (m_pipelineStageCreationFeedback[ndx].flags &
                                VK_PIPELINE_CREATION_FEEDBACK_APPLICATION_PIPELINE_CACHE_HIT_BIT_EXT ?
                            "yes" :
                            "no")
                    << "\n";
            message << "\t\t Base Pipeline Acceleration ? \t"
                    << (m_pipelineStageCreationFeedback[ndx].flags &
                                VK_PIPELINE_CREATION_FEEDBACK_BASE_PIPELINE_ACCELERATION_BIT_EXT ?
                            "yes" :
                            "no")
                    << "\n";
            message << "\t\t Duration (ns): \t\t" << m_pipelineStageCreationFeedback[ndx].duration << "\n";
        }

        log << tcu::TestLog::Message << message.str() << tcu::TestLog::EndMessage;
    }

    if (cachedPipelineWarning)
    {
        return tcu::TestStatus(QP_TEST_RESULT_QUALITY_WARNING, "Cached pipeline or stage did not hit the cache");
    }
    if (durationZeroWarning)
    {
        return tcu::TestStatus(
            QP_TEST_RESULT_QUALITY_WARNING,
            "Pipeline creation feedback reports duration spent creating a pipeline was zero nanoseconds");
    }
    return tcu::TestStatus::pass("Pass");
}
} // namespace

de::MovePtr<tcu::TestCaseGroup> createTestsInternal(tcu::TestContext &testCtx,
                                                    PipelineConstructionType pipelineConstructionType,
                                                    TestMode testMode, de::MovePtr<tcu::TestCaseGroup> blobTests)
{
    // Test pipeline creation feedback with graphics pipeline.
    {
        de::MovePtr<tcu::TestCaseGroup> graphicsTests(new tcu::TestCaseGroup(testCtx, "graphics_tests"));

        const VkShaderStageFlags vertFragStages     = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        const VkShaderStageFlags vertGeomFragStages = vertFragStages | VK_SHADER_STAGE_GEOMETRY_BIT;
        const VkShaderStageFlags vertTessFragStages =
            vertFragStages | VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT | VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
        const bool disableCacheInDelayedDestroy = (testMode != TestMode::CACHE);

        const std::vector<TestParam> testParams{
            //                                   shaders, noCache, delayedDestroy, zeroOutFeedbackCount
            {pipelineConstructionType, testMode, vertFragStages, false, false},
            {pipelineConstructionType, testMode, vertGeomFragStages, false, false},
            {pipelineConstructionType, testMode, vertTessFragStages, false, false},
            {pipelineConstructionType, testMode, vertFragStages, true, false},
            {pipelineConstructionType, testMode, vertFragStages, true, false, true},
            {pipelineConstructionType, testMode, vertGeomFragStages, true, false},
            {pipelineConstructionType, testMode, vertTessFragStages, true, false},
            {pipelineConstructionType, testMode, vertFragStages, disableCacheInDelayedDestroy, true},
            {pipelineConstructionType, testMode, vertGeomFragStages, disableCacheInDelayedDestroy, true},
            {pipelineConstructionType, testMode, vertTessFragStages, disableCacheInDelayedDestroy, true},
        };

        for (auto &param : testParams)
        {
            if (!param.isCacheDisabled() && (testMode == TestMode::BINARY))
                continue;
            graphicsTests->addChild(newTestCase<GraphicsTestCase>(testCtx, &param));
        }

        blobTests->addChild(graphicsTests.release());
    }

    // Compute Pipeline Tests - don't repeat those tests for graphics pipeline library
    if (pipelineConstructionType == PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC)
    {
        de::MovePtr<tcu::TestCaseGroup> computeTests(new tcu::TestCaseGroup(testCtx, "compute_tests"));

        const std::vector<TestParam> testParams{
            {pipelineConstructionType, testMode, VK_SHADER_STAGE_COMPUTE_BIT, false, false},
            {pipelineConstructionType, testMode, VK_SHADER_STAGE_COMPUTE_BIT, true, false},
            {pipelineConstructionType, testMode, VK_SHADER_STAGE_COMPUTE_BIT, false, true},
        };

        for (auto &param : testParams)
        {
            if (param.isCacheDisabled() && (testMode == TestMode::BINARY))
                continue;
            computeTests->addChild(newTestCase<ComputeTestCase>(testCtx, &param));
        }

        blobTests->addChild(computeTests.release());
    }

    return blobTests;
}

tcu::TestCaseGroup *createCreationFeedbackTests(tcu::TestContext &testCtx,
                                                PipelineConstructionType pipelineConstructionType)
{
    de::MovePtr<tcu::TestCaseGroup> mainGroup(new tcu::TestCaseGroup(testCtx, "creation_feedback"));
    return createTestsInternal(testCtx, pipelineConstructionType, TestMode::CACHE, mainGroup).release();
}

de::MovePtr<tcu::TestCaseGroup> addPipelineBinaryCreationFeedbackTests(
    tcu::TestContext &testCtx, PipelineConstructionType pipelineConstructionType,
    de::MovePtr<tcu::TestCaseGroup> binaryGroup)
{
    de::MovePtr<tcu::TestCaseGroup> feedbackGroup(new tcu::TestCaseGroup(testCtx, "creation_feedback"));
    binaryGroup->addChild(
        createTestsInternal(testCtx, pipelineConstructionType, TestMode::BINARY, feedbackGroup).release());
    return binaryGroup;
}

} // namespace pipeline

} // namespace vkt
