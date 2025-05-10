/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2024 The Khronos Group Inc.
 * Copyright (c) 2024 Valve Corporation.
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
 * \brief Device Generated Commands EXT Tessellation State Tests
 *//*--------------------------------------------------------------------*/

#include "vktDGCGraphicsTessStateTestsExt.hpp"
#include "tcuImageCompare.hpp"
#include "vktTestCase.hpp"
#include "vktDGCUtilExt.hpp"
#include "vktDGCUtilCommon.hpp"

#include "vkCmdUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkTypeUtil.hpp"

#include "tcuTestCase.hpp"
#include "tcuStringTemplate.hpp"
#include "tcuTextureUtil.hpp"

#include "deUniquePtr.hpp"

#include <utility>

namespace vkt
{
namespace DGC
{

namespace
{

#define USE_DGC_PATH 1
//#undef USE_DGC_PATH

using namespace vk;

enum class Spacing
{
    EQUAL = 0,
    ODD   = 1,
    EVEN  = 2,
};

std::string toString(Spacing spacing)
{
    std::string repr;
    switch (spacing)
    {
    case Spacing::EQUAL:
        repr = "equal_spacing";
        break;
    case Spacing::ODD:
        repr = "fractional_odd_spacing";
        break;
    case Spacing::EVEN:
        repr = "fractional_even_spacing";
        break;
    default:
        break;
    }

    return repr;
}

enum class PrimitiveType
{
    TRIANGLES = 0,
    ISOLINES  = 1,
    QUADS     = 2,
};

std::string toString(PrimitiveType outputPrimitive)
{
    std::string repr;
    switch (outputPrimitive)
    {
    case PrimitiveType::TRIANGLES:
        repr = "triangles";
        break;
    case PrimitiveType::ISOLINES:
        repr = "isolines";
        break;
    case PrimitiveType::QUADS:
        repr = "quads";
        break;
    default:
        break;
    }

    return repr;
}

using Spacings       = std::pair<Spacing, Spacing>;
using PrimitiveTypes = std::pair<PrimitiveType, PrimitiveType>;
using PatchSizes     = std::pair<uint32_t, uint32_t>;

struct LayerParams
{
    PrimitiveType primitiveType;
    Spacing spacing;
    uint32_t patchSize;

    // These will be used as keys in a map later, so we have to be able to sort them somehow.
protected:
    uint32_t getKey() const
    {
        return (static_cast<uint32_t>(primitiveType) | (static_cast<uint32_t>(spacing) << 8) | (patchSize << 16));
    }

public:
    bool operator<(const LayerParams &other) const
    {
        return (getKey() < other.getKey());
    }
};

using LayerParamPair = std::pair<LayerParams, LayerParams>;

struct TessStateParams
{
    PipelineConstructionType constructionType;
    bool preprocess;
    LayerParamPair layerParams;

    TessStateParams(PipelineConstructionType constructionType_, bool preprocess_, PrimitiveType outputPrimitiveFirst,
                    PrimitiveType outputPrimitiveSecond, Spacing spacingFirst, Spacing spacingSecond,
                    uint32_t patchSizeFirst, uint32_t patchSizeSecond)
        : constructionType(constructionType_)
        , preprocess(preprocess_)
        , layerParams(std::make_pair(LayerParams{outputPrimitiveFirst, spacingFirst, patchSizeFirst},
                                     LayerParams{outputPrimitiveSecond, spacingSecond, patchSizeSecond}))
    {
        for (const auto patchSize : {layerParams.first.patchSize, layerParams.second.patchSize})
        {
            DE_ASSERT(patchSize == 3u || patchSize == 4u);
            DE_UNREF(patchSize); // For release builds.
        }
    }

    tcu::IVec3 getExtent() const
    {
        return tcu::IVec3(32, 32, 1);
    }

    VkShaderStageFlags getShaderStages() const
    {
        return (VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT |
                VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
    }
};

// Reference images that will be used to check each layer.
#include "vktDGCGraphicsTessStateRefImages.hpp"

// Test Case and Instance.
class TessStateInstance : public vkt::TestInstance
{
public:
    TessStateInstance(Context &context, const TessStateParams &params) : vkt::TestInstance(context), m_params(params)
    {
    }

    virtual ~TessStateInstance(void) = default;

    tcu::TestStatus iterate(void) override;

protected:
    const TessStateParams m_params;
};

class TessStateCase : public vkt::TestCase
{
public:
    TessStateCase(tcu::TestContext &testCtx, const std::string &name, const TessStateParams &params)
        : vkt::TestCase(testCtx, name)
        , m_params(params)
    {
    }
    virtual ~TessStateCase(void) = default;

    void checkSupport(Context &context) const override;
    void initPrograms(vk::SourceCollections &programCollection) const override;
    TestInstance *createInstance(Context &context) const override;

protected:
    const TessStateParams m_params;
};

TestInstance *TessStateCase::createInstance(Context &context) const
{
    return new TessStateInstance(context, m_params);
}

void TessStateCase::checkSupport(Context &context) const
{
    context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_TESSELLATION_SHADER);

    // This is required for the layer built-in.
    if (!context.contextSupports(vk::ApiVersion(0u, 1u, 2u, 0u)))
        context.requireDeviceFunctionality("VK_EXT_shader_viewport_index_layer");
    else
    {
        const auto &features = context.getDeviceVulkan12Features();
        if (!features.shaderOutputLayer)
            TCU_THROW(NotSupportedError, "shaderOutputLayer feature not supported");
    }

    const auto ctx = context.getContextCommonData();
    checkPipelineConstructionRequirements(ctx.vki, ctx.physicalDevice, m_params.constructionType);

#ifdef USE_DGC_PATH
    const bool useESO                 = isConstructionTypeShaderObject(m_params.constructionType);
    const auto shaderStages           = m_params.getShaderStages();
    const auto bindStagesPipeline     = (useESO ? 0u : shaderStages);
    const auto bindStagesShaderObject = (useESO ? shaderStages : 0u);
    checkDGCExtSupport(context, shaderStages, bindStagesPipeline, bindStagesShaderObject);
#endif
}

void TessStateCase::initPrograms(vk::SourceCollections &programCollection) const
{
    using TemplateMap = std::map<std::string, std::string>;

    const auto extent = m_params.getExtent();
    DE_ASSERT(extent.z() == 1);
    const auto floatExtent = extent.swizzle(0, 1).asFloat();

    const auto deltaX = std::to_string((2.0f / floatExtent.x()) * 0.25f); // A quarter pixel.
    const auto deltaY = std::to_string((2.0f / floatExtent.y()) * 0.25f); // Ditto.

    const std::string positions =
        std::string() +
        // The delta will make sure if we draw geometry as points, we will reach the sampling point.
        // clang-format off
        "const vec2 positions[4] = vec2[](\n"
            + "    vec2(-1.0 + " + deltaX + ", -1.0 + " + deltaY + "),\n"
            + "    vec2(-1.0 + " + deltaX + ",  1.0 - " + deltaY + "),\n"
            + "    vec2( 1.0 - " + deltaX + ", -1.0 + " + deltaY + "),\n"
            + "    vec2( 1.0 - " + deltaX + ",  1.0 - " + deltaY + ")\n"
            + ");\n";
    // clang-format on

    std::ostringstream vert;
    vert << "#version 460\n"
         << positions << "void main() {\n"
         << "    gl_Position  = vec4(positions[gl_VertexIndex % 4], 0.0, 1.0);\n"
         << "    gl_PointSize = 1.0;\n"
         << "}\n";
    programCollection.glslSources.add("vert") << glu::VertexSource(vert.str());

    std::ostringstream frag;
    frag << "#version 460\n"
         << "layout (location=0) out vec4 outColor;\n"
         << "void main() {\n"
         << "    outColor = vec4(0.0, 0.0, 1.0, 1.0);\n"
         << "}\n";
    programCollection.glslSources.add("frag") << glu::FragmentSource(frag.str());

    std::ostringstream tesc;
    tesc << "#version 460\n"
         << positions << "layout (vertices=${PATCH_SIZE}) out;\n"
         << "void main() {\n"
         << "    if (gl_InvocationID >= gl_PatchVerticesIn) {\n"
         << "        gl_out[gl_InvocationID].gl_Position  = vec4(positions[3], 0.0, 1.0);\n"
         << "        gl_out[gl_InvocationID].gl_PointSize = 1.0;\n"
         << "    }\n"
         << "    else {\n"
         << "        gl_out[gl_InvocationID].gl_Position  = gl_in[gl_InvocationID].gl_Position;\n"
         << "        gl_out[gl_InvocationID].gl_PointSize = gl_in[gl_InvocationID].gl_PointSize;\n"
         << "    }\n"
         << "    if (gl_InvocationID == 0) {\n"
         << "        gl_TessLevelOuter[0] = 3.25;\n"
         << "        gl_TessLevelOuter[1] = 5.25;\n"
         << "        gl_TessLevelOuter[2] = 7.25;\n"
         << "        gl_TessLevelOuter[3] = 9.25;\n"
         << "        gl_TessLevelInner[0] = 3.25;\n"
         << "        gl_TessLevelInner[1] = 5.25;\n"
         << "    }\n"
         << "}\n";
    tcu::StringTemplate tescTemplate(tesc.str());

    TemplateMap tescMap1;
    TemplateMap tescMap2;
    tescMap1["PATCH_SIZE"] = std::to_string(m_params.layerParams.first.patchSize);
    tescMap2["PATCH_SIZE"] = std::to_string(m_params.layerParams.second.patchSize);

    const auto tesc1 = tescTemplate.specialize(tescMap1);
    const auto tesc2 = tescTemplate.specialize(tescMap2);

    programCollection.glslSources.add("tesc1") << glu::TessellationControlSource(tesc1);
    programCollection.glslSources.add("tesc2") << glu::TessellationControlSource(tesc2);

    // The way to calculate coordinates depends directly on the patch primitive type and the number of input vertices.
    using PrimTypeVertCount = std::pair<PrimitiveType, uint32_t>;
    using PositionCalcMap   = std::map<PrimTypeVertCount, std::string>;

    PositionCalcMap positionCalcMap;

    positionCalcMap[std::make_pair(PrimitiveType::ISOLINES, 3u)] =
        std::string() +
        // Create points inside the triangle by making them proportional to gl_TessCoord.xy.
        // This supposes we're using a right-angled triangle as is the case with the positions we use.
        "    const float xCoord = gl_TessCoord.x;\n" + "    const float yCoord = (1.0 - xCoord) * gl_TessCoord.y;\n" +
        "    const float width  = gl_in[2].gl_Position.x - gl_in[0].gl_Position.x;\n" +
        "    const float height = gl_in[1].gl_Position.y - gl_in[0].gl_Position.y;\n" +
        "    const float xPos   = gl_in[0].gl_Position.x + width * xCoord;\n" +
        "    const float yPos   = gl_in[0].gl_Position.y + height * yCoord;\n" +
        "    gl_Position = vec4(xPos, yPos, 0.0, 1.0);\n";

    positionCalcMap[std::make_pair(PrimitiveType::QUADS, 3u)] =
        positionCalcMap[std::make_pair(PrimitiveType::ISOLINES, 3u)];

    positionCalcMap[std::make_pair(PrimitiveType::TRIANGLES, 3u)] =
        std::string() +
        // Undo barycentrics.
        "    const float u = gl_TessCoord.x;\n" + "    const float v = gl_TessCoord.y;\n" +
        "    const float w = gl_TessCoord.z;\n" +
        "    gl_Position = (u * gl_in[0].gl_Position) + (v * gl_in[1].gl_Position) + (w * gl_in[2].gl_Position);\n";

    positionCalcMap[std::make_pair(PrimitiveType::ISOLINES, 4u)] =
        std::string() +
        // Create points inside the rectangle formed by the 4 corners.
        "    const float xCoord = gl_TessCoord.x;\n" + "    const float yCoord = gl_TessCoord.y;\n" +
        "    const float width  = gl_in[2].gl_Position.x - gl_in[0].gl_Position.x;\n" +
        "    const float height = gl_in[3].gl_Position.y - gl_in[2].gl_Position.y;\n" + // Make sure we use gl_in[3]
        "    const float xPos   = gl_in[0].gl_Position.x + width * xCoord;\n" +
        "    const float yPos   = gl_in[0].gl_Position.y + height * yCoord;\n" +
        "    gl_Position = vec4(xPos, yPos, 0.0, 1.0);\n";

    positionCalcMap[std::make_pair(PrimitiveType::QUADS, 4u)] =
        positionCalcMap[std::make_pair(PrimitiveType::ISOLINES, 4u)];

    positionCalcMap[std::make_pair(PrimitiveType::TRIANGLES, 4u)] =
        std::string() +
        // Undo barycentrics using a triangle where the third vertex is in the mid point of the last 2 points.
        "    const float u = gl_TessCoord.x;\n" + "    const float v = gl_TessCoord.y;\n" +
        "    const float w = gl_TessCoord.z;\n" +
        "    const vec4 p1 = gl_in[0].gl_Position;\n"
        "    const vec4 p2 = gl_in[1].gl_Position;\n"
        "    const vec4 p3 = gl_in[2].gl_Position * 0.5 + gl_in[3].gl_Position * 0.5;\n"
        "    gl_Position = (u * p1) + (v * p2) + (w * p3);\n";

    std::ostringstream tese;
    tese << "#version 460\n"
         << "#extension GL_ARB_shader_viewport_layer_array : enable\n"
         << "layout(${OUTPUT_PRIMITIVE}, ${SPACING}, point_mode) in;\n"
         << "void main()\n"
         << "{\n"
         << "${POSITION_CALC}"
         << "    gl_PointSize = 1.0;\n"
         << "    gl_Layer = ${LAYER};\n"
         << "}\n";

    TemplateMap teseMap1;
    TemplateMap teseMap2;

    teseMap1["OUTPUT_PRIMITIVE"] = toString(m_params.layerParams.first.primitiveType);
    teseMap1["SPACING"]          = toString(m_params.layerParams.first.spacing);
    teseMap1["POSITION_CALC"] =
        positionCalcMap[std::make_pair(m_params.layerParams.first.primitiveType, m_params.layerParams.first.patchSize)];
    teseMap1["LAYER"] = "0";

    teseMap2["OUTPUT_PRIMITIVE"] = toString(m_params.layerParams.second.primitiveType);
    teseMap2["SPACING"]          = toString(m_params.layerParams.second.spacing);
    teseMap2["POSITION_CALC"]    = positionCalcMap[std::make_pair(m_params.layerParams.second.primitiveType,
                                                                  m_params.layerParams.second.patchSize)];
    teseMap2["LAYER"]            = "1";

    // We need to build the tessellation evaluation shader for SPV-1.5 and for SPV-1.0 due to the gl_Layer usage.
    const vk::ShaderBuildOptions spv15Opts(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_5, 0u, false);

    const tcu::StringTemplate teseTemplate = tese.str();
    const auto tese1                       = teseTemplate.specialize(teseMap1);
    const auto tese2                       = teseTemplate.specialize(teseMap2);

    programCollection.glslSources.add("tese1-spv10") << glu::TessellationEvaluationSource(tese1);
    programCollection.glslSources.add("tese1-spv15") << glu::TessellationEvaluationSource(tese1) << spv15Opts;

    programCollection.glslSources.add("tese2-spv10") << glu::TessellationEvaluationSource(tese2);
    programCollection.glslSources.add("tese2-spv15") << glu::TessellationEvaluationSource(tese2) << spv15Opts;
}

tcu::TestStatus TessStateInstance::iterate(void)
{
    const auto ctx            = m_context.getContextCommonData();
    const tcu::IVec3 fbExtent = m_params.getExtent();
    const auto vkExtent       = makeExtent3D(fbExtent);
    const auto layerCount     = 2u;
    const auto fbFormat       = VK_FORMAT_R8G8B8A8_UNORM;
    const auto tcuFormat      = mapVkFormat(fbFormat);
    const auto refFormat      = tcu::TextureFormat(tcu::TextureFormat::RGB, tcu::TextureFormat::UNORM_INT8);
    const auto fbUsage        = (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    const tcu::Vec4 clearColor(0.0f, 0.0f, 0.0f, 1.0f);
    const tcu::Vec4 threshold(0.0f, 0.0f, 0.0f, 0.0f); // When using 0 and 1 only, we expect exact results.
    const auto colorSRR = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, layerCount);

    // Color buffer with verification buffer.
    ImageWithBuffer colorBuffer(ctx.vkd, ctx.device, ctx.allocator, vkExtent, fbFormat, fbUsage, VK_IMAGE_TYPE_2D,
                                colorSRR, layerCount);

    // Modules.
    const auto &binaries = m_context.getBinaryCollection();
    ShaderWrapper vertModule(ctx.vkd, ctx.device, binaries.get("vert"));
    ShaderWrapper fragModule(ctx.vkd, ctx.device, binaries.get("frag"));
    ShaderWrapper tesc1Module(ctx.vkd, ctx.device, binaries.get("tesc1"));
    ShaderWrapper tesc2Module(ctx.vkd, ctx.device, binaries.get("tesc2"));
    ShaderWrapper tese1Module_spv10(ctx.vkd, ctx.device, binaries.get("tese1-spv10"));
    ShaderWrapper tese1Module_spv15(ctx.vkd, ctx.device, binaries.get("tese1-spv15"));
    ShaderWrapper tese2Module_spv10(ctx.vkd, ctx.device, binaries.get("tese2-spv10"));
    ShaderWrapper tese2Module_spv15(ctx.vkd, ctx.device, binaries.get("tese2-spv15"));

    const auto vk12Support     = m_context.contextSupports(vk::ApiVersion(0u, 1u, 2u, 0u));
    ShaderWrapper &tese1Module = (vk12Support ? tese1Module_spv15 : tese1Module_spv10);
    ShaderWrapper &tese2Module = (vk12Support ? tese2Module_spv15 : tese2Module_spv10);

    const std::vector<VkViewport> viewports(1u, makeViewport(vkExtent));
    const std::vector<VkRect2D> scissors(1u, makeRect2D(vkExtent));

    PipelineLayoutWrapper pipelineLayout(m_params.constructionType, ctx.vkd, ctx.device);
    RenderPassWrapper renderPass(m_params.constructionType, ctx.vkd, ctx.device, fbFormat);
    renderPass.createFramebuffer(ctx.vkd, ctx.device, colorBuffer.getImage(), colorBuffer.getImageView(),
                                 vkExtent.width, vkExtent.height, layerCount);

    const VkDrawIndirectCommand kDrawCmd{
        3u,
        1u,
        0u,
        0u,
    };

    const VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo = initVulkanStructure();
    GraphicsPipelineWrapper pipeline1(ctx.vki, ctx.vkd, ctx.physicalDevice, ctx.device, m_context.getDeviceExtensions(),
                                      m_params.constructionType);
    pipeline1.setDefaultMultisampleState()
        .setDefaultColorBlendState()
        .setDefaultDepthStencilState()
        .setDefaultRasterizationState()
        .setDefaultTopology(VK_PRIMITIVE_TOPOLOGY_PATCH_LIST)
        .setDefaultPatchControlPoints(3u)
#ifdef USE_DGC_PATH
        .setPipelineCreateFlags2(VK_PIPELINE_CREATE_2_INDIRECT_BINDABLE_BIT_EXT)
        .setShaderCreateFlags(VK_SHADER_CREATE_INDIRECT_BINDABLE_BIT_EXT)
#endif
        .setupVertexInputState(&vertexInputStateCreateInfo)
        .setupPreRasterizationShaderState(viewports, scissors, pipelineLayout, renderPass.get(), 0u, vertModule,
                                          nullptr, tesc1Module, tese1Module)
        .setupFragmentShaderState(pipelineLayout, renderPass.get(), 0u, fragModule)
        .setupFragmentOutputState(renderPass.get())
        .buildPipeline();

    GraphicsPipelineWrapper pipeline2(ctx.vki, ctx.vkd, ctx.physicalDevice, ctx.device, m_context.getDeviceExtensions(),
                                      m_params.constructionType);
    pipeline2.setDefaultMultisampleState()
        .setDefaultColorBlendState()
        .setDefaultDepthStencilState()
        .setDefaultRasterizationState()
        .setDefaultTopology(VK_PRIMITIVE_TOPOLOGY_PATCH_LIST)
        .setDefaultPatchControlPoints(3u)
#ifdef USE_DGC_PATH
        .setPipelineCreateFlags2(VK_PIPELINE_CREATE_2_INDIRECT_BINDABLE_BIT_EXT)
        .setShaderCreateFlags(VK_SHADER_CREATE_INDIRECT_BINDABLE_BIT_EXT)
#endif
        .setupVertexInputState(&vertexInputStateCreateInfo)
        .setupPreRasterizationShaderState(viewports, scissors, pipelineLayout, renderPass.get(), 0u, vertModule,
                                          nullptr, tesc2Module, tese2Module)
        .setupFragmentShaderState(pipelineLayout, renderPass.get(), 0u, fragModule)
        .setupFragmentOutputState(renderPass.get())
        .buildPipeline();

#ifdef USE_DGC_PATH
    const auto pipelineCount         = layerCount; // One pipeline per layer.
    const auto perPipelineStageCount = 4u;         // vert, tesc, tesc, frag
    const bool useESO                = isConstructionTypeShaderObject(m_params.constructionType);
    const auto shaderStages          = m_params.getShaderStages();
    const auto iesType               = (useESO ? VK_INDIRECT_EXECUTION_SET_INFO_TYPE_SHADER_OBJECTS_EXT :
                                                 VK_INDIRECT_EXECUTION_SET_INFO_TYPE_PIPELINES_EXT);

    VkIndirectCommandsLayoutUsageFlagsEXT cmdsLayoutFlags = 0u;
    if (m_params.preprocess)
        cmdsLayoutFlags |= VK_INDIRECT_COMMANDS_LAYOUT_USAGE_EXPLICIT_PREPROCESS_BIT_EXT;

    IndirectCommandsLayoutBuilderExt cmdsLayoutBuilder(cmdsLayoutFlags, shaderStages, *pipelineLayout);
    cmdsLayoutBuilder.addExecutionSetToken(cmdsLayoutBuilder.getStreamRange(), iesType, shaderStages);
    cmdsLayoutBuilder.addDrawToken(cmdsLayoutBuilder.getStreamRange());
    const auto cmdsLayout = cmdsLayoutBuilder.build(ctx.vkd, ctx.device);

    ExecutionSetManagerPtr iesManager;
    if (useESO)
    {
        const std::vector<vk::VkDescriptorSetLayout> noDescriptorSetLayouts;
        const std::vector<vk::VkPushConstantRange> noPCRanges;

        const std::vector<IESStageInfo> stageInfos{
            IESStageInfo(pipeline1.getShader(VK_SHADER_STAGE_VERTEX_BIT), noDescriptorSetLayouts),
            IESStageInfo(pipeline1.getShader(VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT), noDescriptorSetLayouts),
            IESStageInfo(pipeline1.getShader(VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT), noDescriptorSetLayouts),
            IESStageInfo(pipeline1.getShader(VK_SHADER_STAGE_FRAGMENT_BIT), noDescriptorSetLayouts),
        };
        DE_ASSERT(perPipelineStageCount == de::sizeU32(stageInfos));

        iesManager = makeExecutionSetManagerShader(ctx.vkd, ctx.device, stageInfos, noPCRanges,
                                                   perPipelineStageCount * pipelineCount);
        iesManager->addShader(perPipelineStageCount + 0u, pipeline2.getShader(VK_SHADER_STAGE_VERTEX_BIT));
        iesManager->addShader(perPipelineStageCount + 1u,
                              pipeline2.getShader(VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT));
        iesManager->addShader(perPipelineStageCount + 2u,
                              pipeline2.getShader(VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT));
        iesManager->addShader(perPipelineStageCount + 3u, pipeline2.getShader(VK_SHADER_STAGE_FRAGMENT_BIT));
    }
    else
    {
        iesManager = makeExecutionSetManagerPipeline(ctx.vkd, ctx.device, pipeline1.getPipeline(), pipelineCount);
        iesManager->addPipeline(1u, pipeline2.getPipeline());
    }
    iesManager->update();

    // DGC buffer contents.
    std::vector<uint32_t> dgcData;
    dgcData.reserve((cmdsLayoutBuilder.getStreamStride() * pipelineCount) / DE_SIZEOF32(uint32_t));
    for (uint32_t i = 0u; i < pipelineCount; ++i)
    {
        // IES token.
        if (useESO)
        {
            dgcData.push_back(i * perPipelineStageCount + 0u);
            dgcData.push_back(i * perPipelineStageCount + 1u);
            dgcData.push_back(i * perPipelineStageCount + 2u);
            dgcData.push_back(i * perPipelineStageCount + 3u);
        }
        else
            dgcData.push_back(i);

        // Draw token.
        pushBackElement(dgcData, kDrawCmd);
    }

    const auto dgcBufferSize = static_cast<VkDeviceSize>(de::dataSize(dgcData));
    DGCBuffer dgcBuffer(ctx.vkd, ctx.device, ctx.allocator, dgcBufferSize);
    {
        auto &alloc = dgcBuffer.getAllocation();
        memcpy(alloc.getHostPtr(), de::dataOrNull(dgcData), de::dataSize(dgcData));
    }

    PreprocessBufferExt preprocessBuffer(ctx.vkd, ctx.device, ctx.allocator, iesManager->get(), *cmdsLayout,
                                         pipelineCount, 0u);
#endif

    CommandPoolWithBuffer cmd(ctx.vkd, ctx.device, ctx.qfIndex);
    const auto cmdBuffer = *cmd.cmdBuffer;

    beginCommandBuffer(ctx.vkd, cmdBuffer);
    renderPass.begin(ctx.vkd, cmdBuffer, scissors.at(0u), clearColor);
#ifdef USE_DGC_PATH
    pipeline1.bind(cmdBuffer); // Bind initial state, including initial shader state.
    const DGCGenCmdsInfo cmdsInfo(shaderStages, iesManager->get(), *cmdsLayout, dgcBuffer.getDeviceAddress(),
                                  dgcBuffer.getSize(), preprocessBuffer.getDeviceAddress(), preprocessBuffer.getSize(),
                                  pipelineCount, 0ull, 0u);

    Move<VkCommandBuffer> preprocessCmdBuffer;
    if (m_params.preprocess)
    {
        preprocessCmdBuffer = allocateCommandBuffer(ctx.vkd, ctx.device, *cmd.cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
        beginCommandBuffer(ctx.vkd, *preprocessCmdBuffer);
        ctx.vkd.cmdPreprocessGeneratedCommandsEXT(*preprocessCmdBuffer, &cmdsInfo.get(), cmdBuffer);
        preprocessToExecuteBarrierExt(ctx.vkd, *preprocessCmdBuffer);
        endCommandBuffer(ctx.vkd, *preprocessCmdBuffer);
    }
    ctx.vkd.cmdExecuteGeneratedCommandsEXT(cmdBuffer, makeVkBool(m_params.preprocess), &cmdsInfo.get());
#else
    pipeline1.bind(cmdBuffer);
    ctx.vkd.cmdDraw(cmdBuffer, kDrawCmd.vertexCount, kDrawCmd.instanceCount, kDrawCmd.firstVertex,
                    kDrawCmd.firstInstance);
    pipeline2.bind(cmdBuffer);
    ctx.vkd.cmdDraw(cmdBuffer, kDrawCmd.vertexCount, kDrawCmd.instanceCount, kDrawCmd.firstVertex,
                    kDrawCmd.firstInstance);
#endif
    renderPass.end(ctx.vkd, cmdBuffer);
    copyImageToBuffer(ctx.vkd, cmdBuffer, colorBuffer.getImage(), colorBuffer.getBuffer(), fbExtent.swizzle(0, 1),
                      VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, layerCount,
                      VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_ASPECT_COLOR_BIT,
                      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
    endCommandBuffer(ctx.vkd, cmdBuffer);
#ifdef USE_DGC_PATH
    submitAndWaitWithPreprocess(ctx.vkd, ctx.device, ctx.queue, cmdBuffer, *preprocessCmdBuffer);
#else
    submitCommandsAndWait(ctx.vkd, ctx.device, ctx.queue, cmdBuffer);
#endif

    // Verify color output.
    const tcu::IVec3 verifExtent(fbExtent.x(), fbExtent.y(), static_cast<int>(layerCount));
    invalidateAlloc(ctx.vkd, ctx.device, colorBuffer.getBufferAllocation());
    tcu::PixelBufferAccess resultAccess(tcuFormat, verifExtent, colorBuffer.getBufferAllocation().getHostPtr());

    // Create reference map for the different parameter combinations and use the right ones.
    std::map<LayerParams, const uint8_t *> referenceMap;
    referenceMap[LayerParams{PrimitiveType::TRIANGLES, Spacing::EQUAL, 3u}] = triangles__equal_spacing__3;
    referenceMap[LayerParams{PrimitiveType::TRIANGLES, Spacing::EQUAL, 4u}] = triangles__equal_spacing__4;
    referenceMap[LayerParams{PrimitiveType::TRIANGLES, Spacing::ODD, 3u}]   = triangles__fractional_odd_spacing__3;
    referenceMap[LayerParams{PrimitiveType::TRIANGLES, Spacing::ODD, 4u}]   = triangles__fractional_odd_spacing__4;
    referenceMap[LayerParams{PrimitiveType::TRIANGLES, Spacing::EVEN, 3u}]  = triangles__fractional_even_spacing__3;
    referenceMap[LayerParams{PrimitiveType::TRIANGLES, Spacing::EVEN, 4u}]  = triangles__fractional_even_spacing__4;
    referenceMap[LayerParams{PrimitiveType::ISOLINES, Spacing::EQUAL, 3u}]  = isolines__equal_spacing__3;
    referenceMap[LayerParams{PrimitiveType::ISOLINES, Spacing::EQUAL, 4u}]  = isolines__equal_spacing__4;
    referenceMap[LayerParams{PrimitiveType::ISOLINES, Spacing::ODD, 3u}]    = isolines__fractional_odd_spacing__3;
    referenceMap[LayerParams{PrimitiveType::ISOLINES, Spacing::ODD, 4u}]    = isolines__fractional_odd_spacing__4;
    referenceMap[LayerParams{PrimitiveType::ISOLINES, Spacing::EVEN, 3u}]   = isolines__fractional_even_spacing__3;
    referenceMap[LayerParams{PrimitiveType::ISOLINES, Spacing::EVEN, 4u}]   = isolines__fractional_even_spacing__4;
    referenceMap[LayerParams{PrimitiveType::QUADS, Spacing::EQUAL, 3u}]     = quads__equal_spacing__3;
    referenceMap[LayerParams{PrimitiveType::QUADS, Spacing::EQUAL, 4u}]     = quads__equal_spacing__4;
    referenceMap[LayerParams{PrimitiveType::QUADS, Spacing::ODD, 3u}]       = quads__fractional_odd_spacing__3;
    referenceMap[LayerParams{PrimitiveType::QUADS, Spacing::ODD, 4u}]       = quads__fractional_odd_spacing__4;
    referenceMap[LayerParams{PrimitiveType::QUADS, Spacing::EVEN, 3u}]      = quads__fractional_even_spacing__3;
    referenceMap[LayerParams{PrimitiveType::QUADS, Spacing::EVEN, 4u}]      = quads__fractional_even_spacing__4;

    auto &log                = m_context.getTestContext().getLog();
    bool fail                = false;
    const size_t kHeaderSize = strlen(kCommonHeader);

    DE_ASSERT(verifExtent.z() == 2);
    for (int z = 0; z < verifExtent.z(); ++z)
    {
        const auto resultLayer = tcu::getSubregion(resultAccess, 0, 0, z, fbExtent.x(), fbExtent.y(), 1);

        const auto &key             = (z == 0 ? m_params.layerParams.first : m_params.layerParams.second);
        const uint8_t *refLayerData = referenceMap.at(key);
        DE_ASSERT(memcmp(refLayerData, kCommonHeader, kHeaderSize) == 0);
        const tcu::ConstPixelBufferAccess referenceLayer(refFormat, fbExtent, refLayerData + kHeaderSize);

        const auto imageName = "Layer" + std::to_string(z);

        if (!tcu::floatThresholdCompare(log, imageName.c_str(), "", referenceLayer, resultLayer, threshold,
                                        tcu::COMPARE_LOG_ON_ERROR))
        {
            fail = true;
        }
    }

    if (fail)
        TCU_FAIL("Unexpected color in result buffer; check log for details");

    return tcu::TestStatus::pass("Pass");
}

class DynamicPCPInstance : public vkt::TestInstance
{
public:
    struct Params
    {
        PipelineConstructionType constructionType;
        bool useIES;
        bool usePreprocess;

        VkShaderStageFlags getShaderStages() const
        {
            return (VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT |
                    VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
        }

        tcu::IVec3 getExtent() const
        {
            return tcu::IVec3(64, 64, 1);
        }

    protected:
        uint32_t getDrawOffsetCount() const
        {
            return 4u;
        }

        uint32_t getTessVariationCount() const
        {
            return (useIES ? getDrawOffsetCount() : 1u);
        }

    public:
        std::vector<tcu::Vec4> getTessColors() const
        {
            static const std::vector<tcu::Vec4> colorCatalogue{
                tcu::Vec4(0.0f, 1.0f, 1.0f, 1.0f),
                tcu::Vec4(1.0f, 0.0f, 1.0f, 1.0f),
                tcu::Vec4(1.0f, 1.0f, 0.0f, 1.0f),
                tcu::Vec4(1.0f, 1.0f, 1.0f, 1.0f),
            };

            std::vector<tcu::Vec4> colors;
            const auto tessCount = getTessVariationCount();
            const auto drawCount = getDrawOffsetCount();

            DE_ASSERT(tessCount == 1u || tessCount == drawCount);
            DE_ASSERT(de::sizeU32(colorCatalogue) == drawCount);
            DE_UNREF(drawCount); // For release builds.

            colors.reserve(tessCount);
            if (useIES)
                colors = colorCatalogue;
            else
                colors.push_back(colorCatalogue.front());
            return colors;
        }

        std::vector<tcu::Vec4> getDrawOffsets() const
        {
            const std::vector<tcu::Vec4> offsets{
                tcu::Vec4(-1.0f, -1.0f, 0.0f, 0.0f),
                tcu::Vec4(0.0f, -1.0f, 0.0f, 0.0f),
                tcu::Vec4(-1.0f, 0.0f, 0.0f, 0.0f),
                tcu::Vec4(0.0f, 0.0f, 0.0f, 0.0f),
            };
            DE_ASSERT(de::sizeU32(offsets) == getDrawOffsetCount());
            return offsets;
        }

        uint32_t getActualPCP() const
        {
            return 4u; // We will use quads for the patch.
        }

        uint32_t getStaticPCP() const
        {
            return 3u; // But the static value will hint triangles instead.
        }
    };

    DynamicPCPInstance(Context &context, const Params &params) : vkt::TestInstance(context), m_params(params)
    {
    }
    virtual ~DynamicPCPInstance(void) = default;

    tcu::TestStatus iterate(void) override;

protected:
    const Params m_params;
};

class DynamicPCPCase : public vkt::TestCase
{
public:
    DynamicPCPCase(tcu::TestContext &testCtx, const std::string &name, const DynamicPCPInstance::Params &params)
        : vkt::TestCase(testCtx, name)
        , m_params(params)
    {
    }
    virtual ~DynamicPCPCase(void) = default;

    void checkSupport(Context &context) const override;
    void initPrograms(vk::SourceCollections &programCollection) const override;
    TestInstance *createInstance(Context &context) const override
    {
        return new DynamicPCPInstance(context, m_params);
    }

protected:
    const DynamicPCPInstance::Params m_params;
};

void DynamicPCPCase::checkSupport(Context &context) const
{
#ifdef USE_DGC_PATH
    const auto stages     = m_params.getShaderStages();
    const auto bindStages = (m_params.useIES ? stages : 0u);
    DE_ASSERT(!isConstructionTypeShaderObject(m_params.constructionType));

    checkDGCExtSupport(context, stages, bindStages);
#endif // USE_DGC_PATH

    const auto &eds2Features = context.getExtendedDynamicState2FeaturesEXT();
    if (!eds2Features.extendedDynamicState2PatchControlPoints)
        TCU_THROW(NotSupportedError, "extendedDynamicState2PatchControlPoints not supported");
}

void DynamicPCPCase::initPrograms(vk::SourceCollections &programCollection) const
{
    std::ostringstream vert;
    vert << "#version 460\n"
         << "layout (location=0) in vec4 inPos;\n"
         << "layout (push_constant) uniform PCBlock { vec4 offset; } pc;\n"
         << "void main (void) {\n"
         << "    gl_Position = inPos + pc.offset;\n"
         << "    gl_PointSize = 1.0;\n"
         << "}\n";
    programCollection.glslSources.add("vert") << glu::VertexSource(vert.str());

    std::ostringstream frag;
    frag << "#version 460\n"
         << "layout (location=0) in vec4 inColor;\n"
         << "layout (location=0) out vec4 outColor;\n"
         << "void main (void) {\n"
         << "    outColor = inColor;\n"
         << "}\n";
    programCollection.glslSources.add("frag") << glu::FragmentSource(frag.str());

    const auto tessColors = m_params.getTessColors();
    const auto colorCount = de::sizeU32(tessColors);
    const auto pcp        = m_params.getActualPCP();

    for (uint32_t i = 0u; i < colorCount; ++i)
    {
        std::ostringstream tesc;
        tesc << "#version 460\n"
             << "layout (vertices=" << pcp << ") out;\n" // Vertices pass through without changes.
             << "void main (void) {\n"
             << "    const bool goodPVI = (gl_PatchVerticesIn == " << pcp << ");\n"
             << "    const float posOffset = (goodPVI ? 0.0 : 10.0);\n"
             << "\n"
             << "    gl_out[gl_InvocationID].gl_Position  = gl_in[gl_InvocationID].gl_Position + posOffset;\n"
             << "    gl_out[gl_InvocationID].gl_PointSize = gl_in[gl_InvocationID].gl_PointSize;\n"
             << "\n"
             << "    const float extraLevels = " << i << ".0;\n"
             << "    if (gl_InvocationID == 0) {\n"
             << "        gl_TessLevelOuter[0] = 3.0 + extraLevels;\n"
             << "        gl_TessLevelOuter[1] = 5.0 + extraLevels;\n"
             << "        gl_TessLevelOuter[2] = 7.0 + extraLevels;\n"
             << "        gl_TessLevelOuter[3] = 9.0 + extraLevels;\n"
             << "        gl_TessLevelInner[0] = 3.0 + extraLevels;\n"
             << "        gl_TessLevelInner[1] = 5.0 + extraLevels;\n"
             << "    }\n"
             << "}\n";
        {
            const auto shaderName = "tesc" + std::to_string(i);
            programCollection.glslSources.add(shaderName) << glu::TessellationControlSource(tesc.str());
        }

        std::ostringstream tese;
        tese << "#version 460\n"
             << "layout (quads, point_mode) in;\n"
             << "layout (location=0) out vec4 vertColor;\n"
             << "void main (void) {\n"
             << "    const bool goodPVI = (gl_PatchVerticesIn == " << pcp << ");\n"
             << "    const float posOffset = (goodPVI ? 0.0 : 10.0);\n"
             << "\n"
             << "    const float u = gl_TessCoord.x;\n"
             << "    const float v = gl_TessCoord.y;\n"
             << "    const vec4 p0 = gl_in[0].gl_Position;\n"
             << "    const vec4 p1 = gl_in[1].gl_Position;\n"
             << "    const vec4 p2 = gl_in[2].gl_Position;\n"
             << "    const vec4 p3 = gl_in[3].gl_Position;\n"
             << "    gl_Position = ((1 - u) * (1 - v) * p0 + (1 - u) * v * p1 + u * (1 - v) * p2 + u * v * p3) + "
                "posOffset;\n"
             << "    gl_PointSize = 1.0;\n"
             << "    vertColor = vec4" << tessColors.at(i) << ";\n"
             << "}\n";
        {
            const auto shaderName = "tese" + std::to_string(i);
            programCollection.glslSources.add(shaderName) << glu::TessellationEvaluationSource(tese.str());
        }
    }
}

tcu::TestStatus DynamicPCPInstance::iterate(void)
{
    const auto &ctx      = m_context.getContextCommonData();
    const auto fbExtent  = m_params.getExtent();
    const auto vkExtent  = makeExtent3D(fbExtent);
    const auto fbFormat  = VK_FORMAT_R8G8B8A8_UNORM;
    const auto tcuFormat = mapVkFormat(fbFormat);
    const auto fbUsage   = (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    const tcu::Vec4 clearColor(0.0f, 0.0f, 0.0f, 1.0f);
    const tcu::Vec4 threshold(0.0f, 0.0f, 0.0f, 0.0f); // When using 0 and 1 only, we expect exact results.

    // Color buffers for the result and reference images, both with verification buffer.
    ImageWithBuffer colorBufferRes(ctx.vkd, ctx.device, ctx.allocator, vkExtent, fbFormat, fbUsage, VK_IMAGE_TYPE_2D);

    ImageWithBuffer colorBufferRef(ctx.vkd, ctx.device, ctx.allocator, vkExtent, fbFormat, fbUsage, VK_IMAGE_TYPE_2D);

    // Vertices. These will be offset with the push constants for each section.
    const std::vector<tcu::Vec4> vertices{
        tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f),
        tcu::Vec4(0.0f, 1.0f, 0.0f, 1.0f),
        tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f),
        tcu::Vec4(1.0f, 1.0f, 0.0f, 1.0f),
    };
    const auto vertexCount = de::sizeU32(vertices);

    // Vertex buffer
    const auto vbSize = static_cast<VkDeviceSize>(de::dataSize(vertices));
    const auto vbInfo = makeBufferCreateInfo(vbSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    BufferWithMemory vertexBuffer(ctx.vkd, ctx.device, ctx.allocator, vbInfo, MemoryRequirement::HostVisible);
    const auto vbAlloc  = vertexBuffer.getAllocation();
    void *vbData        = vbAlloc.getHostPtr();
    const auto vbOffset = static_cast<VkDeviceSize>(0);

    deMemcpy(vbData, de::dataOrNull(vertices), de::dataSize(vertices));
    flushAlloc(ctx.vkd, ctx.device, vbAlloc); // strictly speaking, not needed.

    // Push constants.
    const auto pcSize   = DE_SIZEOF32(tcu::Vec4);
    const auto pcStages = static_cast<VkShaderStageFlags>(VK_SHADER_STAGE_VERTEX_BIT);
    const auto pcRange  = makePushConstantRange(pcStages, 0u, pcSize);

    PipelineLayoutWrapper pipelineLayout(m_params.constructionType, ctx.vkd, ctx.device, VK_NULL_HANDLE, &pcRange);
    RenderPassWrapper renderPassRes(m_params.constructionType, ctx.vkd, ctx.device, fbFormat);
    RenderPassWrapper renderPassRef = renderPassRes.clone();
    renderPassRes.createFramebuffer(ctx.vkd, ctx.device, colorBufferRes.getImage(), colorBufferRes.getImageView(),
                                    vkExtent.width, vkExtent.height);
    renderPassRef.createFramebuffer(ctx.vkd, ctx.device, colorBufferRef.getImage(), colorBufferRef.getImageView(),
                                    vkExtent.width, vkExtent.height);

    // Modules.
    const auto &binaries = m_context.getBinaryCollection();
    ShaderWrapper vertShader(ctx.vkd, ctx.device, binaries.get("vert"));
    ShaderWrapper fragShader(ctx.vkd, ctx.device, binaries.get("frag"));

    using ShaderPtr = std::unique_ptr<ShaderWrapper>;
    std::vector<ShaderPtr> tescShaders;
    std::vector<ShaderPtr> teseShaders;

    const auto tessColors = m_params.getTessColors();
    tescShaders.reserve(tessColors.size());
    teseShaders.reserve(tessColors.size());

    for (uint32_t i = 0u; i < de::sizeU32(tessColors); ++i)
    {
        const auto suffix   = std::to_string(i);
        const auto tescName = "tesc" + suffix;
        const auto teseName = "tese" + suffix;

        tescShaders.emplace_back(new ShaderWrapper(ctx.vkd, ctx.device, binaries.get(tescName)));
        teseShaders.emplace_back(new ShaderWrapper(ctx.vkd, ctx.device, binaries.get(teseName)));
    }

    const std::vector<VkViewport> viewports(1u, makeViewport(vkExtent));
    const std::vector<VkRect2D> scissors(1u, makeRect2D(vkExtent));

    using PipelineWrapperPtr = std::unique_ptr<GraphicsPipelineWrapper>;

    const auto goodPCP = m_params.getActualPCP();
    const auto badPCP  = m_params.getStaticPCP();

    const auto drawOffsets = m_params.getDrawOffsets();

    const auto cmdPool      = makeCommandPool(ctx.vkd, ctx.device, ctx.qfIndex);
    const auto resCmdBuffer = allocateCommandBuffer(ctx.vkd, ctx.device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    const auto refCmdBuffer = allocateCommandBuffer(ctx.vkd, ctx.device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

    // Result pipelines, using dynamic state.
    const std::vector<VkDynamicState> dynamicStates{VK_DYNAMIC_STATE_PATCH_CONTROL_POINTS_EXT};

    const VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        nullptr,
        0u,
        de::sizeU32(dynamicStates),
        de::dataOrNull(dynamicStates),
    };

#ifdef USE_DGC_PATH
    const VkPipelineCreateFlags2KHR pipelineFlags2 =
        (m_params.useIES ? VK_PIPELINE_CREATE_2_INDIRECT_BINDABLE_BIT_EXT : 0);
    const VkShaderCreateFlagsEXT shaderFlags = (m_params.useIES ? VK_SHADER_CREATE_INDIRECT_BINDABLE_BIT_EXT : 0);
#else
    const VkPipelineCreateFlags2KHR pipelineFlags2 = 0u;
    const VkShaderCreateFlagsEXT shaderFlags       = 0u;
#endif

    std::vector<PipelineWrapperPtr> resPipelines;
    resPipelines.reserve(tessColors.size());
    for (uint32_t i = 0u; i < de::sizeU32(tessColors); ++i)
    {
        resPipelines.emplace_back(new GraphicsPipelineWrapper(ctx.vki, ctx.vkd, ctx.physicalDevice, ctx.device,
                                                              m_context.getDeviceExtensions(),
                                                              m_params.constructionType));
        auto &pipeline = *resPipelines.back();
        pipeline.setDefaultColorBlendState()
            .setDefaultDepthStencilState()
            .setDefaultMultisampleState()
            .setDefaultRasterizationState()
            .setDefaultPatchControlPoints(badPCP)
            .setPipelineCreateFlags2(pipelineFlags2)
            .setShaderCreateFlags(shaderFlags)
            .setDefaultTopology(VK_PRIMITIVE_TOPOLOGY_PATCH_LIST)
            .setDynamicState(&dynamicStateCreateInfo)
            .setupVertexInputState()
            .setupPreRasterizationShaderState(viewports, scissors, pipelineLayout, renderPassRef.get(), 0u, vertShader,
                                              nullptr, *tescShaders.at(i), *teseShaders.at(i))
            .setupFragmentShaderState(pipelineLayout, renderPassRef.get(), 0u, fragShader)
            .setupFragmentOutputState(renderPassRef.get())
            .buildPipeline();
    }

#ifdef USE_DGC_PATH
    // Commands layout.
    const auto useESO = isConstructionTypeShaderObject(m_params.constructionType);
    DE_ASSERT(!useESO); // Not handled below.
    DE_UNREF(useESO);   // For release builds.

    const auto shaderStages = m_params.getShaderStages();

    const VkIndirectCommandsLayoutUsageFlagsEXT cmdsLayoutFlags =
        (m_params.usePreprocess ? VK_INDIRECT_COMMANDS_LAYOUT_USAGE_EXPLICIT_PREPROCESS_BIT_EXT : 0);
    IndirectCommandsLayoutBuilderExt cmdsLayoutBuilder(cmdsLayoutFlags, shaderStages, pipelineLayout.get());
    if (m_params.useIES)
        cmdsLayoutBuilder.addExecutionSetToken(0u, VK_INDIRECT_EXECUTION_SET_INFO_TYPE_PIPELINES_EXT, shaderStages);
    cmdsLayoutBuilder.addPushConstantToken(cmdsLayoutBuilder.getStreamRange(), pcRange);
    cmdsLayoutBuilder.addDrawToken(cmdsLayoutBuilder.getStreamRange());
    const auto cmdsLayout = cmdsLayoutBuilder.build(ctx.vkd, ctx.device);

    ExecutionSetManagerPtr iesManager;
    VkIndirectExecutionSetEXT iesHandle = VK_NULL_HANDLE;

    if (m_params.useIES)
    {
        iesManager = makeExecutionSetManagerPipeline(ctx.vkd, ctx.device, resPipelines.front()->getPipeline(),
                                                     de::sizeU32(resPipelines));
        for (uint32_t i = 0u; i < de::sizeU32(resPipelines); ++i)
            iesManager->addPipeline(i, resPipelines.at(i)->getPipeline());
        iesManager->update();
        iesHandle = iesManager->get();
    }

    // DGC buffer contents.
    const auto sequenceCount = de::sizeU32(drawOffsets);
    std::vector<uint32_t> dgcData;
    dgcData.reserve((sequenceCount * cmdsLayoutBuilder.getStreamStride()) / DE_SIZEOF32(uint32_t));
    for (uint32_t i = 0u; i < sequenceCount; ++i)
    {
        if (m_params.useIES)
            dgcData.push_back(i);
        pushBackElement(dgcData, drawOffsets.at(i));
        dgcData.push_back(vertexCount);
        dgcData.push_back(1u); // instanceCount
        dgcData.push_back(0u); // firstVertex
        dgcData.push_back(0u); // firstInstance
    }

    // DGC buffer and preprocess buffer.
    DGCBuffer dgcBuffer(ctx.vkd, ctx.device, ctx.allocator, de::dataSize(dgcData));
    {
        auto &alloc = dgcBuffer.getAllocation();
        memcpy(alloc.getHostPtr(), de::dataOrNull(dgcData), de::dataSize(dgcData));
    }

    const auto preprocessPipeline =
        ((iesHandle != VK_NULL_HANDLE) ? VK_NULL_HANDLE : resPipelines.front()->getPipeline());
    PreprocessBufferExt preprocessBuffer(ctx.vkd, ctx.device, ctx.allocator, iesHandle, *cmdsLayout, sequenceCount, 0u,
                                         preprocessPipeline);
#endif
    Move<VkCommandBuffer> preprocessCmdBuffer;
    VkCommandBuffer cmdBuffer = *resCmdBuffer;

    beginCommandBuffer(ctx.vkd, cmdBuffer);
    renderPassRes.begin(ctx.vkd, cmdBuffer, scissors.at(0u), clearColor);
    ctx.vkd.cmdBindVertexBuffers(cmdBuffer, 0u, 1u, &vertexBuffer.get(), &vbOffset);
    ctx.vkd.cmdSetPatchControlPointsEXT(cmdBuffer, goodPCP);
#ifdef USE_DGC_PATH
    resPipelines.front()->bind(cmdBuffer); // Bind initial state.
    {
        DGCGenCmdsInfo cmdsInfo(shaderStages, iesHandle, *cmdsLayout, dgcBuffer.getDeviceAddress(), dgcBuffer.getSize(),
                                preprocessBuffer.getDeviceAddress(), preprocessBuffer.getSize(), sequenceCount, 0ull,
                                0u, preprocessPipeline);

        if (m_params.usePreprocess)
        {
            preprocessCmdBuffer = allocateCommandBuffer(ctx.vkd, ctx.device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
            beginCommandBuffer(ctx.vkd, *preprocessCmdBuffer);
            ctx.vkd.cmdPreprocessGeneratedCommandsEXT(*preprocessCmdBuffer, &cmdsInfo.get(), cmdBuffer);
            preprocessToExecuteBarrierExt(ctx.vkd, *preprocessCmdBuffer);
            endCommandBuffer(ctx.vkd, *preprocessCmdBuffer);
        }
        ctx.vkd.cmdExecuteGeneratedCommandsEXT(cmdBuffer, makeVkBool(m_params.usePreprocess), &cmdsInfo.get());
    }
#else
    for (uint32_t i = 0u; i < de::sizeU32(drawOffsets); ++i)
    {
        const auto pipelineIdx = (i >= de::sizeU32(resPipelines) ? 0u : i);
        resPipelines.at(pipelineIdx)->bind(cmdBuffer);
        ctx.vkd.cmdPushConstants(cmdBuffer, *pipelineLayout, pcStages, 0u, pcSize, &drawOffsets.at(i));
        ctx.vkd.cmdDraw(cmdBuffer, vertexCount, 1u, 0u, 0u);
    }
#endif
    renderPassRes.end(ctx.vkd, cmdBuffer);
    copyImageToBuffer(ctx.vkd, cmdBuffer, colorBufferRes.getImage(), colorBufferRes.getBuffer(), fbExtent.swizzle(0, 1),
                      VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 1u,
                      VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_ASPECT_COLOR_BIT,
                      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
    endCommandBuffer(ctx.vkd, cmdBuffer);
    submitAndWaitWithPreprocess(ctx.vkd, ctx.device, ctx.queue, cmdBuffer, *preprocessCmdBuffer);

    // Reference pipelines.
    std::vector<PipelineWrapperPtr> refPipelines;
    refPipelines.reserve(tessColors.size());
    for (uint32_t i = 0u; i < de::sizeU32(tessColors); ++i)
    {
        refPipelines.emplace_back(new GraphicsPipelineWrapper(ctx.vki, ctx.vkd, ctx.physicalDevice, ctx.device,
                                                              m_context.getDeviceExtensions(),
                                                              m_params.constructionType));
        auto &pipeline = *refPipelines.back();
        pipeline.setDefaultColorBlendState()
            .setDefaultDepthStencilState()
            .setDefaultMultisampleState()
            .setDefaultRasterizationState()
            .setDefaultPatchControlPoints(goodPCP)
            .setDefaultTopology(VK_PRIMITIVE_TOPOLOGY_PATCH_LIST)
            .setupVertexInputState()
            .setupPreRasterizationShaderState(viewports, scissors, pipelineLayout, renderPassRef.get(), 0u, vertShader,
                                              nullptr, *tescShaders.at(i), *teseShaders.at(i))
            .setupFragmentShaderState(pipelineLayout, renderPassRef.get(), 0u, fragShader)
            .setupFragmentOutputState(renderPassRef.get())
            .buildPipeline();
    }

    // Generate reference image.
    cmdBuffer = *refCmdBuffer;

    beginCommandBuffer(ctx.vkd, cmdBuffer);
    renderPassRef.begin(ctx.vkd, cmdBuffer, scissors.at(0u), clearColor);
    ctx.vkd.cmdBindVertexBuffers(cmdBuffer, 0u, 1u, &vertexBuffer.get(), &vbOffset);
    for (uint32_t i = 0u; i < de::sizeU32(drawOffsets); ++i)
    {
        const auto pipelineIdx = (i >= de::sizeU32(refPipelines) ? 0u : i);
        refPipelines.at(pipelineIdx)->bind(cmdBuffer);
        ctx.vkd.cmdPushConstants(cmdBuffer, *pipelineLayout, pcStages, 0u, pcSize, &drawOffsets.at(i));
        ctx.vkd.cmdDraw(cmdBuffer, vertexCount, 1u, 0u, 0u);
    }
    renderPassRef.end(ctx.vkd, cmdBuffer);
    copyImageToBuffer(ctx.vkd, cmdBuffer, colorBufferRef.getImage(), colorBufferRef.getBuffer(), fbExtent.swizzle(0, 1),
                      VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 1u,
                      VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_ASPECT_COLOR_BIT,
                      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
    endCommandBuffer(ctx.vkd, cmdBuffer);
    submitCommandsAndWait(ctx.vkd, ctx.device, ctx.queue, cmdBuffer);

    // Verify color output.
    invalidateAlloc(ctx.vkd, ctx.device, colorBufferRef.getBufferAllocation());
    tcu::PixelBufferAccess referenceAccess(tcuFormat, fbExtent, colorBufferRef.getBufferAllocation().getHostPtr());

    invalidateAlloc(ctx.vkd, ctx.device, colorBufferRes.getBufferAllocation());
    tcu::PixelBufferAccess resultAccess(tcuFormat, fbExtent, colorBufferRes.getBufferAllocation().getHostPtr());

    auto &log = m_context.getTestContext().getLog();
    if (!tcu::floatThresholdCompare(log, "Result", "", referenceAccess, resultAccess, threshold,
                                    tcu::COMPARE_LOG_EVERYTHING))
        return tcu::TestStatus::fail("Unexpected color in result buffer; check log for details");

    return tcu::TestStatus::pass("Pass");
}

} // namespace

tcu::TestCaseGroup *createDGCGraphicsTessStateTestsExt(tcu::TestContext &testCtx)
{
    using GroupPtr = de::MovePtr<tcu::TestCaseGroup>;
    GroupPtr mainGroup(new tcu::TestCaseGroup(testCtx, "tess_state", ""));

    struct
    {
        PipelineConstructionType constructionType;
        const char *name;
    } constructionTypes[] = {
        {PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC, "monolithic"},
        {PIPELINE_CONSTRUCTION_TYPE_FAST_LINKED_LIBRARY, "fast_lib"},
        {PIPELINE_CONSTRUCTION_TYPE_SHADER_OBJECT_UNLINKED_SPIRV, "shader_objects"},
    };

    for (const auto &constructionTypeCase : constructionTypes)
    {
        GroupPtr cTypeGroup(new tcu::TestCaseGroup(testCtx, constructionTypeCase.name));

        for (const auto firstPrim : {PrimitiveType::TRIANGLES, PrimitiveType::ISOLINES, PrimitiveType::QUADS})
            for (const auto secondPrim : {PrimitiveType::TRIANGLES, PrimitiveType::ISOLINES, PrimitiveType::QUADS})
            {
                const auto primGroupName = toString(firstPrim) + "_" + toString(secondPrim);
                GroupPtr primGroup(new tcu::TestCaseGroup(testCtx, primGroupName.c_str()));

                for (const auto firstSpacing : {Spacing::EQUAL, Spacing::ODD, Spacing::EVEN})
                    for (const auto secondSpacing : {Spacing::EQUAL, Spacing::ODD, Spacing::EVEN})
                    {
                        const auto spacingGroupName = toString(firstSpacing) + "_" + toString(secondSpacing);
                        GroupPtr spacingGroup(new tcu::TestCaseGroup(testCtx, spacingGroupName.c_str()));

                        for (const auto firstSize : {3u, 4u})
                            for (const auto secondSize : {3u, 4u})
                                for (const auto preprocess : {false, true})
                                {
                                    const bool identical = (firstPrim == secondPrim && firstSpacing == secondSpacing &&
                                                            firstSize == secondSize);
#if 0
                                    // Simplified combinations for debugging.
                                    if (!identical)
#else
                                    if (identical)
#endif
                                        continue;

#ifndef USE_DGC_PATH
                                    if (preprocess)
                                        continue;
#endif
                                    const auto caseName = std::to_string(firstSize) + "_" + std::to_string(secondSize) +
                                                          (preprocess ? "_preprocess" : "");
                                    const TessStateParams params(constructionTypeCase.constructionType, preprocess,
                                                                 firstPrim, secondPrim, firstSpacing, secondSpacing,
                                                                 firstSize, secondSize);

                                    spacingGroup->addChild(new TessStateCase(testCtx, caseName, params));
                                }

                        primGroup->addChild(spacingGroup.release());
                    }

                cTypeGroup->addChild(primGroup.release());
            }
        mainGroup->addChild(cTypeGroup.release());
    }

    {
        GroupPtr dynamicStatesGroup(new tcu::TestCaseGroup(testCtx, "dynamic_states"));

        for (const auto &constructionTypeCase : constructionTypes)
        {
            if (isConstructionTypeShaderObject(constructionTypeCase.constructionType))
                continue; // With shader objects, everything is already dynamic.

            GroupPtr cTypeGroup(new tcu::TestCaseGroup(testCtx, constructionTypeCase.name));

            for (const bool useIES : {false, true})
                for (const bool preprocess : {false, true})
                {
                    const DynamicPCPInstance::Params params{
                        constructionTypeCase.constructionType,
                        useIES,
                        preprocess,
                    };
                    const auto testName =
                        std::string("pcp") + (useIES ? "_ies" : "") + (preprocess ? "_preprocess" : "");

                    cTypeGroup->addChild(new DynamicPCPCase(testCtx, testName, params));
                }

            dynamicStatesGroup->addChild(cTypeGroup.release());
        }
        mainGroup->addChild(dynamicStatesGroup.release());
    }

    return mainGroup.release();
}

} // namespace DGC
} // namespace vkt
