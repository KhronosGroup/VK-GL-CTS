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
 * \brief Device Generated Commands EXT Graphics XFB Tests
 *//*--------------------------------------------------------------------*/

#include "vktDGCGraphicsXfbTestsExt.hpp"
#include "util/vktShaderObjectUtil.hpp"
#include "vkBarrierUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkObjUtil.hpp"
#include "vktTestCase.hpp"
#include "vktDGCUtilExt.hpp"

#include "tcuTextureUtil.hpp"
#include "tcuImageCompare.hpp"

#include "deUniquePtr.hpp"

#include <sstream>
#include <vector>
#include <map>
#include <set>

namespace vkt
{
namespace DGC
{

namespace
{

using namespace vk;

struct Params
{
    bool discardXFB;
    bool useGeom;
    bool useTess;
    bool useShaderObjects;

    VkShaderStageFlags getShaderStages() const
    {
        VkShaderStageFlags stages = (VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
        if (useGeom)
            stages |= VK_SHADER_STAGE_GEOMETRY_BIT;
        if (useTess)
            stages |= (VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT | VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT);
        return stages;
    }
};

class XfbTestInstance : public vkt::TestInstance
{
public:
    XfbTestInstance(Context &context, const Params &params) : vkt::TestInstance(context), m_params(params)
    {
    }
    virtual ~XfbTestInstance(void) = default;

    tcu::TestStatus iterate(void) override;

protected:
    const Params m_params;
};

class XfbTestCase : public vkt::TestCase
{
public:
    XfbTestCase(tcu::TestContext &testCtx, const std::string &name, const Params &params)
        : vkt::TestCase(testCtx, name)
        , m_params(params)
    {
    }
    virtual ~XfbTestCase(void) = default;

    void initPrograms(vk::SourceCollections &programCollection) const override;
    TestInstance *createInstance(Context &context) const override;
    void checkSupport(Context &context) const override;

protected:
    const Params m_params;
};

void XfbTestCase::checkSupport(Context &context) const
{
    const auto stages = m_params.getShaderStages();
    checkDGCExtSupport(context, stages, 0u, 0u, 0u, true /*xfb*/);

    context.requireDeviceFunctionality("VK_EXT_transform_feedback");

    if (m_params.useShaderObjects)
        context.requireDeviceFunctionality("VK_EXT_shader_object");

    if (m_params.useGeom)
        context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_GEOMETRY_SHADER);

    if (m_params.useTess)
        context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_TESSELLATION_SHADER);
}

TestInstance *XfbTestCase::createInstance(Context &context) const
{
    return new XfbTestInstance(context, m_params);
}

void XfbTestCase::initPrograms(vk::SourceCollections &programCollection) const
{
    bool xfbGeom = false;
    bool xfbTess = false;
    bool xfbVert = false;

    // Last stage will be using xfb.
    if (m_params.useGeom)
        xfbGeom = true;
    else if (m_params.useTess)
        xfbTess = true;
    else
        xfbVert = true;

    const std::string xfbPrefix = "layout(xfb_buffer = 0, xfb_offset = 0) ";

    std::ostringstream vert;
    vert << "#version 460\n"
         << (xfbVert ? xfbPrefix : "") << "out gl_PerVertex {\n"
         << "    vec4 gl_Position;\n"
         << "};\n"
         << "layout (location=0) in vec4 inPos;\n"
         << "void main(void) {\n"
         << "    gl_Position = inPos;\n"
         << "}\n";
    programCollection.glslSources.add("vert") << glu::VertexSource(vert.str());

    std::ostringstream frag;
    frag << "#version 460\n"
         << "layout (location=0) out vec4 outColor;\n"
         << "void main(void) {\n"
         << "    outColor = vec4(0.0, 0.0, 1.0, 1.0);\n"
         << "}\n";
    programCollection.glslSources.add("frag") << glu::FragmentSource(frag.str());

    if (m_params.useTess)
    {
        // Passthrough tessellation shaders.
        std::ostringstream tesc;
        tesc << "#version 460\n"
             << "#extension GL_EXT_tessellation_shader : require\n"
             << "layout(vertices=3) out;\n"
             << "in gl_PerVertex\n"
             << "{\n"
             << "    vec4 gl_Position;\n"
             << "} gl_in[gl_MaxPatchVertices];\n"
             << "out gl_PerVertex\n"
             << "{\n"
             << "    vec4 gl_Position;\n"
             << "} gl_out[];\n"
             << "void main() {\n"
             << "    gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;\n"
             << "    gl_TessLevelOuter[0] = 1.0;\n"
             << "    gl_TessLevelOuter[1] = 1.0;\n"
             << "    gl_TessLevelOuter[2] = 1.0;\n"
             << "    gl_TessLevelOuter[3] = 1.0;\n"
             << "    gl_TessLevelInner[0] = 1.0;\n"
             << "    gl_TessLevelInner[1] = 1.0;\n"
             << "}\n";
        programCollection.glslSources.add("tesc") << glu::TessellationControlSource(tesc.str());

        std::ostringstream tese;
        tese << "#version 460\n"
             << "#extension GL_EXT_tessellation_shader : require\n"
             << "layout(triangles) in;\n"
             << "in gl_PerVertex {\n"
             << "    vec4 gl_Position;\n"
             << "} gl_in[gl_MaxPatchVertices];\n"
             << (xfbTess ? xfbPrefix : "") << "out gl_PerVertex {\n"
             << "    vec4 gl_Position;\n"
             << "};\n"
             << "void main() {\n"
             << "    gl_Position = (gl_in[0].gl_Position * gl_TessCoord.x + \n"
             << "                   gl_in[1].gl_Position * gl_TessCoord.y + \n"
             << "                   gl_in[2].gl_Position * gl_TessCoord.z);\n"
             << "}\n";
        programCollection.glslSources.add("tese") << glu::TessellationEvaluationSource(tese.str());
    }

    if (m_params.useGeom)
    {
        // Passthrough geometry shader.
        std::ostringstream geom;
        geom << "#version 460\n"
             << "layout (triangles) in;\n"
             << "layout (triangle_strip, max_vertices=3) out;\n"
             << "in gl_PerVertex {\n"
             << "    vec4 gl_Position;\n"
             << "} gl_in[3];\n"
             << (xfbGeom ? xfbPrefix : "") << "out gl_PerVertex {\n"
             << "    vec4 gl_Position;\n"
             << "};\n"
             << "void main() {\n"
             << "    for (uint i = 0; i < 3; ++i) {\n"
             << "        gl_Position = gl_in[i].gl_Position;\n"
             << "        EmitVertex();\n"
             << "    }\n"
             << "}\n";
        programCollection.glslSources.add("geom") << glu::GeometrySource(geom.str());
    }
}

void bindShaders(const Context &context, const Params &params, VkCommandBuffer cmdBuffer,
                 const Move<VkShaderEXT> &vertShader, const Move<VkShaderEXT> &fragShader,
                 const Move<VkShaderEXT> &tescShader, const Move<VkShaderEXT> &teseShader,
                 const Move<VkShaderEXT> &geomShader)
{
    const auto ctx           = context.getContextCommonData();
    const auto &features     = context.getDeviceFeatures();
    const auto &meshFeatures = context.getMeshShaderFeaturesEXT();

    std::map<VkShaderStageFlagBits, VkShaderEXT> boundShaders;

    boundShaders[VK_SHADER_STAGE_VERTEX_BIT]   = vertShader.get();
    boundShaders[VK_SHADER_STAGE_FRAGMENT_BIT] = fragShader.get();

    if (params.useTess)
    {
        boundShaders[VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT]    = tescShader.get();
        boundShaders[VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT] = teseShader.get();
    }
    else if (features.tessellationShader)
    {
        boundShaders[VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT]    = VK_NULL_HANDLE;
        boundShaders[VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT] = VK_NULL_HANDLE;
    }

    if (params.useGeom)
        boundShaders[VK_SHADER_STAGE_GEOMETRY_BIT] = geomShader.get();
    else if (features.geometryShader)
        boundShaders[VK_SHADER_STAGE_GEOMETRY_BIT] = VK_NULL_HANDLE;

    if (meshFeatures.taskShader)
        boundShaders[VK_SHADER_STAGE_TASK_BIT_EXT] = VK_NULL_HANDLE;
    if (meshFeatures.meshShader)
        boundShaders[VK_SHADER_STAGE_MESH_BIT_EXT] = VK_NULL_HANDLE;

    for (const auto &stageShader : boundShaders)
        ctx.vkd.cmdBindShadersEXT(cmdBuffer, 1u, &stageShader.first, &stageShader.second);
}

// This is needed for verifyTriangle to be able to sort vertices.
// The order itself is not that important as long as it's consistent.
struct LessVec4
{
    bool operator()(const tcu::Vec4 &a, const tcu::Vec4 &b) const
    {
        // Lexicographical order by component order.
        for (uint32_t i = 0; i < tcu::Vec4::SIZE; ++i)
        {
            if (a[i] < b[i])
                return true;
            else if (a[i] > b[i])
                return false;
        }

        return false;
    }
};

// Help check both triangles match despite vertex order.
bool verifyTriangle(const tcu::Vec4 &a1, const tcu::Vec4 &a2, const tcu::Vec4 &a3, const tcu::Vec4 &b1,
                    const tcu::Vec4 &b2, const tcu::Vec4 &b3)
{
    LessVec4 compare;
    const std::set<tcu::Vec4, LessVec4> a({a1, a2, a3}, compare);
    const std::set<tcu::Vec4, LessVec4> b({b1, b2, b3}, compare);

    return (a == b);
}

Move<VkShaderEXT> makeShader(const vk::DeviceInterface &vkd, vk::VkDevice device, vk::VkShaderStageFlagBits stage,
                             vk::VkShaderCreateFlagsEXT shaderFlags, const vk::ProgramBinary &shaderBinary,
                             const std::vector<vk::VkDescriptorSetLayout> &setLayouts,
                             const std::vector<vk::VkPushConstantRange> &pushConstantRanges, bool tessellationFeature,
                             bool geometryFeature)
{
    if (shaderBinary.getFormat() != PROGRAM_FORMAT_SPIRV)
        TCU_THROW(InternalError, "Program format not supported");

    VkShaderStageFlags nextStage = 0u;
    switch (stage)
    {
    case VK_SHADER_STAGE_VERTEX_BIT:
        if (tessellationFeature)
            nextStage |= VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
        if (geometryFeature)
            nextStage |= VK_SHADER_STAGE_GEOMETRY_BIT;
        nextStage |= VK_SHADER_STAGE_FRAGMENT_BIT;
        break;
    case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:
        DE_ASSERT(tessellationFeature);
        nextStage |= VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
        break;
    case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT:
        DE_ASSERT(tessellationFeature);
        if (geometryFeature)
            nextStage |= VK_SHADER_STAGE_GEOMETRY_BIT;
        nextStage |= VK_SHADER_STAGE_FRAGMENT_BIT;
        break;
    case VK_SHADER_STAGE_GEOMETRY_BIT:
        DE_ASSERT(geometryFeature);
        nextStage |= VK_SHADER_STAGE_FRAGMENT_BIT;
        break;
    case VK_SHADER_STAGE_TASK_BIT_EXT:
        nextStage |= VK_SHADER_STAGE_MESH_BIT_EXT;
        break;
    case VK_SHADER_STAGE_MESH_BIT_EXT:
        nextStage |= VK_SHADER_STAGE_FRAGMENT_BIT;
        break;
    default:
        break;
    }

    const VkShaderCreateInfoEXT shaderCreateInfo = {
        VK_STRUCTURE_TYPE_SHADER_CREATE_INFO_EXT, // VkStructureType sType;
        nullptr,                                  // const void* pNext;
        shaderFlags,                              // VkShaderCreateFlagsEXT flags;
        stage,                                    // VkShaderStageFlagBits stage;
        nextStage,                                // VkShaderStageFlags nextStage;
        VK_SHADER_CODE_TYPE_SPIRV_EXT,            // VkShaderCodeTypeEXT codeType;
        shaderBinary.getSize(),                   // size_t codeSize;
        shaderBinary.getBinary(),                 // const void* pCode;
        "main",                                   // const char* pName;
        de::sizeU32(setLayouts),                  // uint32_t setLayoutCount;
        de::dataOrNull(setLayouts),               // const VkDescriptorSetLayout* pSetLayouts;
        de::sizeU32(pushConstantRanges),          // uint32_t pushConstantRangeCount;
        de::dataOrNull(pushConstantRanges),       // const VkPushConstantRange* pPushConstantRanges;
        nullptr,                                  // const VkSpecializationInfo* pSpecializationInfo;
    };

    shaderBinary.setUsed();
    return createShader(vkd, device, shaderCreateInfo);
}

tcu::TestStatus XfbTestInstance::iterate(void)
{
    const auto ctx = m_context.getContextCommonData();
    const tcu::IVec3 fbExtent(8, 8, 1);
    const auto apiExtent   = makeExtent3D(fbExtent);
    const auto colorFormat = VK_FORMAT_R8G8B8A8_UNORM;
    const auto tcuFormat   = mapVkFormat(colorFormat);
    const auto colorUsage =
        (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
    const auto colorSRR            = makeDefaultImageSubresourceRange();
    const auto perDrawVertices     = (m_params.useTess ? 6u : 4u); // patch list vs triangle strip.
    const auto perDrawTriangles    = 2u;
    const auto perTriangleVertices = 3u;
    const auto drawCount           = 2u;
    const auto inputVertexCount    = perDrawVertices * drawCount;
    const auto outputTriangleCount = perDrawTriangles * drawCount;
    const auto outputVertexCount   = perTriangleVertices * outputTriangleCount;
    const auto xfbTopology =
        (m_params.useTess ? VK_PRIMITIVE_TOPOLOGY_PATCH_LIST : VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP);
    const auto indirectDrawTopology =
        (m_params.useTess ? VK_PRIMITIVE_TOPOLOGY_PATCH_LIST : VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    const auto shaderStages = m_params.getShaderStages();

    // Vertices using 2 half-screen quads.
    std::vector<tcu::Vec4> vertices;
    vertices.reserve(inputVertexCount);

    if (m_params.useTess)
    {
        // Patch list in this case, which works as a triangle list with the passthrough shaders.

        // First patches.
        vertices.push_back(tcu::Vec4(-1.0f, -1.0f, 0.0f, 1.0f));
        vertices.push_back(tcu::Vec4(-1.0f, 0.0f, 0.0f, 1.0f));
        vertices.push_back(tcu::Vec4(1.0f, -1.0f, 0.0f, 1.0f));
        vertices.push_back(tcu::Vec4(1.0f, -1.0f, 0.0f, 1.0f));
        vertices.push_back(tcu::Vec4(-1.0f, 0.0f, 0.0f, 1.0f));
        vertices.push_back(tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f));

        // Second patches.
        vertices.push_back(tcu::Vec4(-1.0f, 0.0f, 0.0f, 1.0f));
        vertices.push_back(tcu::Vec4(-1.0f, 1.0f, 0.0f, 1.0f));
        vertices.push_back(tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f));
        vertices.push_back(tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f));
        vertices.push_back(tcu::Vec4(-1.0f, 1.0f, 0.0f, 1.0f));
        vertices.push_back(tcu::Vec4(1.0f, 1.0f, 0.0f, 1.0f));
    }
    else
    {
        // First strip.
        vertices.push_back(tcu::Vec4(-1.0f, -1.0f, 0.0f, 1.0f));
        vertices.push_back(tcu::Vec4(-1.0f, 0.0f, 0.0f, 1.0f));
        vertices.push_back(tcu::Vec4(1.0f, -1.0f, 0.0f, 1.0f));
        vertices.push_back(tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f));

        // Second strip.
        vertices.push_back(tcu::Vec4(-1.0f, 0.0f, 0.0f, 1.0f));
        vertices.push_back(tcu::Vec4(-1.0f, 1.0f, 0.0f, 1.0f));
        vertices.push_back(tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f));
        vertices.push_back(tcu::Vec4(1.0f, 1.0f, 0.0f, 1.0f));
    }

    const VkDeviceSize vertexBufferOffset = 0ull;
    const auto vertexBufferInfo = makeBufferCreateInfo(de::dataSize(vertices), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    BufferWithMemory vertexBuffer(ctx.vkd, ctx.device, ctx.allocator, vertexBufferInfo, MemoryRequirement::HostVisible);
    {
        auto &alloc   = vertexBuffer.getAllocation();
        void *dataPtr = alloc.getHostPtr();
        deMemcpy(dataPtr, de::dataOrNull(vertices), de::dataSize(vertices));
    }

    // Color buffer for the indirect draw without XFB.
    ImageWithBuffer colorDrawBuffer(ctx.vkd, ctx.device, ctx.allocator, apiExtent, colorFormat, colorUsage,
                                    VK_IMAGE_TYPE_2D, colorSRR);

    // Color buffer for the intermediate draw with XFB on.
    std::unique_ptr<ImageWithBuffer> colorXfbBuffer;
    if (!m_params.discardXFB)
        colorXfbBuffer.reset(new ImageWithBuffer(ctx.vkd, ctx.device, ctx.allocator, apiExtent, colorFormat, colorUsage,
                                                 VK_IMAGE_TYPE_2D, colorSRR));

    const auto pipelineLayout = makePipelineLayout(ctx.vkd, ctx.device);

    std::vector<Move<VkRenderPass>> renderPasses;
    std::vector<Move<VkFramebuffer>> frameBuffers;

    Move<VkShaderModule> vertModule;
    Move<VkShaderModule> fragModule;
    Move<VkShaderModule> tescModule;
    Move<VkShaderModule> teseModule;
    Move<VkShaderModule> geomModule;
    std::vector<Move<VkPipeline>> pipelines;

    Move<VkShaderEXT> vertShader;
    Move<VkShaderEXT> fragShader;
    Move<VkShaderEXT> tescShader;
    Move<VkShaderEXT> teseShader;
    Move<VkShaderEXT> geomShader;

    const auto &binaries = m_context.getBinaryCollection();

    // Render passes and frambuffers in usage order.
    const auto xfbFormat    = (m_params.discardXFB ? VK_FORMAT_UNDEFINED : colorFormat);
    const auto xfbImageView = (m_params.discardXFB ? VK_NULL_HANDLE : colorXfbBuffer->getImageView());

    if (!m_params.useShaderObjects)
    {
        renderPasses.emplace_back(
            makeRenderPass(ctx.vkd, ctx.device, xfbFormat, VK_FORMAT_UNDEFINED, VK_ATTACHMENT_LOAD_OP_LOAD));
        renderPasses.emplace_back(
            makeRenderPass(ctx.vkd, ctx.device, colorFormat, VK_FORMAT_UNDEFINED, VK_ATTACHMENT_LOAD_OP_LOAD));

        const bool nullAttachment       = (xfbImageView == VK_NULL_HANDLE);
        const uint32_t attachmentCount  = (nullAttachment ? 0u : 1u);
        const VkImageView *pAttachments = (nullAttachment ? nullptr : &xfbImageView);

        frameBuffers.push_back(makeFramebuffer(ctx.vkd, ctx.device, *renderPasses.front(), attachmentCount,
                                               pAttachments, apiExtent.width, apiExtent.height));
        frameBuffers.push_back(makeFramebuffer(ctx.vkd, ctx.device, *renderPasses.back(),
                                               colorDrawBuffer.getImageView(), apiExtent.width, apiExtent.height));
    }

    const VkPipelineRasterizationStateCreateInfo rasterizationXfbState = {
        VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        nullptr,
        0u,
        VK_FALSE,
        makeVkBool(m_params.discardXFB),
        VK_POLYGON_MODE_FILL,
        VK_CULL_MODE_NONE,
        VK_FRONT_FACE_COUNTER_CLOCKWISE,
        VK_FALSE,
        0.0f,
        0.0f,
        0.0f,
        1.0f,
    };
    VkPipelineRasterizationStateCreateInfo rasterizationDrawState = rasterizationXfbState;
    rasterizationDrawState.rasterizerDiscardEnable                = VK_FALSE;

    const std::vector<VkViewport> viewports(1u, makeViewport(fbExtent));
    const std::vector<VkRect2D> scissors(1u, makeRect2D(fbExtent));

    const auto vertexBinding =
        makeVertexInputBindingDescription(0u, DE_SIZEOF32(tcu::Vec4), VK_VERTEX_INPUT_RATE_VERTEX);
    const auto vertexAttrib = makeVertexInputAttributeDescription(0u, 0u, VK_FORMAT_R32G32B32A32_SFLOAT, 0u);

    const VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, nullptr, 0u, 1u, &vertexBinding, 1u, &vertexAttrib,
    };

    const std::vector<VkDescriptorSetLayout> noLayouts;
    const std::vector<VkPushConstantRange> noPCRanges;
    const auto patchControlPoints = (m_params.useTess ? perTriangleVertices : 0u);

    std::vector<VkShaderEXT> shaderHandles;
    shaderHandles.reserve(5u);

    const auto &features   = m_context.getDeviceFeatures();
    const auto tessFeature = (features.tessellationShader == VK_TRUE);
    const auto geomFeature = (features.geometryShader == VK_TRUE);

    // Shaders, modules and pipelines.
    if (m_params.useShaderObjects)
    {
        vertShader = makeShader(ctx.vkd, ctx.device, VK_SHADER_STAGE_VERTEX_BIT, 0u, binaries.get("vert"), noLayouts,
                                noPCRanges, tessFeature, geomFeature);
        fragShader = makeShader(ctx.vkd, ctx.device, VK_SHADER_STAGE_FRAGMENT_BIT, 0u, binaries.get("frag"), noLayouts,
                                noPCRanges, tessFeature, geomFeature);

        shaderHandles.push_back(*vertShader);
        shaderHandles.push_back(*fragShader);

        if (m_params.useTess)
        {
            tescShader = makeShader(ctx.vkd, ctx.device, VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT, 0u,
                                    binaries.get("tesc"), noLayouts, noPCRanges, tessFeature, geomFeature);
            teseShader = makeShader(ctx.vkd, ctx.device, VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, 0u,
                                    binaries.get("tese"), noLayouts, noPCRanges, tessFeature, geomFeature);

            shaderHandles.push_back(*tescShader);
            shaderHandles.push_back(*teseShader);
        }
        if (m_params.useGeom)
        {
            geomShader = makeShader(ctx.vkd, ctx.device, VK_SHADER_STAGE_GEOMETRY_BIT, 0u, binaries.get("geom"),
                                    noLayouts, noPCRanges, tessFeature, geomFeature);

            shaderHandles.push_back(*geomShader);
        }
    }
    else
    {
        vertModule = createShaderModule(ctx.vkd, ctx.device, binaries.get("vert"));
        fragModule = createShaderModule(ctx.vkd, ctx.device, binaries.get("frag"));
        if (m_params.useTess)
        {
            tescModule = createShaderModule(ctx.vkd, ctx.device, binaries.get("tesc"));
            teseModule = createShaderModule(ctx.vkd, ctx.device, binaries.get("tese"));
        }
        if (m_params.useGeom)
            geomModule = createShaderModule(ctx.vkd, ctx.device, binaries.get("geom"));

        // Pipeline to be used during XFB.
        const auto xfbFragModule = (m_params.discardXFB ? VK_NULL_HANDLE : *fragModule);

        pipelines.emplace_back(makeGraphicsPipeline(ctx.vkd, ctx.device, *pipelineLayout, *vertModule, *tescModule,
                                                    *teseModule, *geomModule, xfbFragModule, *renderPasses.front(),
                                                    viewports, scissors, xfbTopology, 0u, patchControlPoints,
                                                    &vertexInputStateCreateInfo, &rasterizationXfbState));

        // Pipeline to be used in the indirect draw.
        // Note: frag module always present, different rasterization info.
        pipelines.emplace_back(makeGraphicsPipeline(ctx.vkd, ctx.device, *pipelineLayout, *vertModule, *tescModule,
                                                    *teseModule, *geomModule, *fragModule, *renderPasses.back(),
                                                    viewports, scissors, indirectDrawTopology, 0u, patchControlPoints,
                                                    &vertexInputStateCreateInfo, &rasterizationDrawState));
    }

    // Indirect commands layout.
    IndirectCommandsLayoutBuilderExt cmdsLayoutBuilder(0u, shaderStages, VK_NULL_HANDLE);
    cmdsLayoutBuilder.addDrawToken(0u);
    const auto cmdsLayout = cmdsLayoutBuilder.build(ctx.vkd, ctx.device);

    // DGC sequences.
    std::vector<uint32_t> dgcData;
    dgcData.reserve(cmdsLayoutBuilder.getStreamStride() * drawCount / DE_SIZEOF32(uint32_t));
    for (uint32_t i = 0u; i < drawCount; ++i)
    {
        dgcData.push_back(perDrawVertices);     // vertexCount
        dgcData.push_back(1u);                  // instanceCount
        dgcData.push_back(i * perDrawVertices); // firstVertex
        dgcData.push_back(0u);                  // firstInstance
    }

    DGCBuffer dgcBuffer(ctx.vkd, ctx.device, ctx.allocator, de::dataSize(dgcData));
    {
        auto &alloc   = dgcBuffer.getAllocation();
        void *dataPtr = alloc.getHostPtr();
        deMemcpy(dataPtr, de::dataOrNull(dgcData), de::dataSize(dgcData));
    }

    // Preprocess buffer.
    const auto preprocessPipeline = (m_params.useShaderObjects ? VK_NULL_HANDLE : *pipelines.front());
    const auto preprocessShaders  = (m_params.useShaderObjects ? &shaderHandles : nullptr);
    PreprocessBufferExt preprocessBuffer(ctx.vkd, ctx.device, ctx.allocator, VK_NULL_HANDLE, *cmdsLayout, drawCount, 0u,
                                         preprocessPipeline, preprocessShaders);

    const DGCGenCmdsInfo cmdsInfo(shaderStages, VK_NULL_HANDLE, *cmdsLayout, dgcBuffer.getDeviceAddress(),
                                  dgcBuffer.getSize(), preprocessBuffer.getDeviceAddress(), preprocessBuffer.getSize(),
                                  drawCount, 0ull, 0u, preprocessPipeline, preprocessShaders);

    const CommandPoolWithBuffer cmd(ctx.vkd, ctx.device, ctx.qfIndex);
    const auto cmdBuffer = *cmd.cmdBuffer;

    beginCommandBuffer(ctx.vkd, cmdBuffer);

    // Clear and prepare color buffers.
    const tcu::Vec4 clearColor(0.0f, 0.0f, 0.0f, 1.0f);
    const auto apiClearColor = makeClearValueColor(clearColor);

    {
        {
            std::vector<VkImageMemoryBarrier> preClearBarriers;
            preClearBarriers.reserve(2u);

            preClearBarriers.push_back(
                makeImageMemoryBarrier(0u, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                                       VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, colorDrawBuffer.getImage(), colorSRR));
            if (colorXfbBuffer)
                preClearBarriers.push_back(
                    makeImageMemoryBarrier(0u, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, colorXfbBuffer->getImage(), colorSRR));

            cmdPipelineImageMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                          VK_PIPELINE_STAGE_TRANSFER_BIT, de::dataOrNull(preClearBarriers),
                                          preClearBarriers.size());
        }

        ctx.vkd.cmdClearColorImage(cmdBuffer, colorDrawBuffer.getImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                   &apiClearColor.color, 1u, &colorSRR);
        if (colorXfbBuffer)
            ctx.vkd.cmdClearColorImage(cmdBuffer, colorXfbBuffer->getImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                       &apiClearColor.color, 1u, &colorSRR);

        {
            std::vector<VkImageMemoryBarrier> postClearBarriers;
            postClearBarriers.reserve(2u);
            const auto colorAttAccess = (VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);

            postClearBarriers.push_back(makeImageMemoryBarrier(
                VK_ACCESS_TRANSFER_WRITE_BIT, colorAttAccess, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, colorDrawBuffer.getImage(), colorSRR));
            if (colorXfbBuffer)
                postClearBarriers.push_back(makeImageMemoryBarrier(
                    VK_ACCESS_TRANSFER_WRITE_BIT, colorAttAccess, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, colorXfbBuffer->getImage(), colorSRR));

            cmdPipelineImageMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                          VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                          de::dataOrNull(postClearBarriers), postClearBarriers.size());
        }
    }

    ctx.vkd.cmdBindVertexBuffers(cmdBuffer, 0u, 1u, &vertexBuffer.get(), &vertexBufferOffset);

    if (m_params.useShaderObjects)
        beginRendering(ctx.vkd, cmdBuffer, xfbImageView, scissors.at(0u), apiClearColor,
                       VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    else
        beginRenderPass(ctx.vkd, cmdBuffer, *renderPasses.front(), *frameBuffers.front(), scissors.at(0u));

    if (m_params.useShaderObjects)
    {
        bindShaders(m_context, m_params, cmdBuffer, vertShader, fragShader, tescShader, teseShader, geomShader);
        vkt::shaderobjutil::bindShaderObjectState(
            ctx.vkd, m_context.getDeviceExtensions(), cmdBuffer, viewports, scissors, xfbTopology, patchControlPoints,
            &vertexInputStateCreateInfo, &rasterizationXfbState, nullptr, nullptr, nullptr);
    }
    else
    {
        const auto bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        ctx.vkd.cmdBindPipeline(cmdBuffer, bindPoint, *pipelines.front());
    }

    // XFB counter buffer.
    const auto counterBufferUsage =
        (VK_BUFFER_USAGE_TRANSFORM_FEEDBACK_COUNTER_BUFFER_BIT_EXT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT);
    const auto counterBufferInfo = makeBufferCreateInfo(DE_SIZEOF32(uint32_t), counterBufferUsage);
    BufferWithMemory counterBuffer(ctx.vkd, ctx.device, ctx.allocator, counterBufferInfo,
                                   MemoryRequirement::HostVisible);
    const VkDeviceSize counterBufferOffset = 0ull;
    {
        auto &alloc   = counterBuffer.getAllocation();
        void *dataPtr = alloc.getHostPtr();
        deMemset(dataPtr, 0, sizeof(uint32_t));
    }

    // XFB buffer.
    const VkDeviceSize xfbBufferSize   = static_cast<VkDeviceSize>(DE_SIZEOF32(tcu::Vec4) * outputVertexCount);
    const VkDeviceSize xfbBufferOffset = 0ull;
    const auto xfbBufferUsage = (VK_BUFFER_USAGE_TRANSFORM_FEEDBACK_BUFFER_BIT_EXT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    const auto xfbBufferInfo  = makeBufferCreateInfo(xfbBufferSize, xfbBufferUsage);
    BufferWithMemory xfbBuffer(ctx.vkd, ctx.device, ctx.allocator, xfbBufferInfo, MemoryRequirement::HostVisible);
    auto &xfbBufferAlloc = xfbBuffer.getAllocation();
    void *xfbBufferData  = xfbBufferAlloc.getHostPtr();
    deMemset(xfbBufferData, 0, static_cast<size_t>(xfbBufferSize));

    ctx.vkd.cmdBindTransformFeedbackBuffersEXT(cmdBuffer, 0u, 1u, &xfbBuffer.get(), &xfbBufferOffset, &xfbBufferSize);

    ctx.vkd.cmdBeginTransformFeedbackEXT(cmdBuffer, 0u, 1u, &counterBuffer.get(), &counterBufferOffset);
    {
        // Draw once for each quad.
        ctx.vkd.cmdExecuteGeneratedCommandsEXT(cmdBuffer, VK_FALSE, &cmdsInfo.get());
    }
    ctx.vkd.cmdEndTransformFeedbackEXT(cmdBuffer, 0u, 1u, &counterBuffer.get(), &counterBufferOffset);

    if (m_params.useShaderObjects)
        ctx.vkd.cmdEndRendering(cmdBuffer);
    else
        ctx.vkd.cmdEndRenderPass(cmdBuffer);

    {
        // Synchronize transform feedback writes to indirect draws.
        std::vector<VkMemoryBarrier> barriers;
        barriers.reserve(2u);

        barriers.push_back(makeMemoryBarrier(VK_ACCESS_TRANSFORM_FEEDBACK_COUNTER_WRITE_BIT_EXT,
                                             VK_ACCESS_TRANSFORM_FEEDBACK_COUNTER_READ_BIT_EXT));
        barriers.push_back(
            makeMemoryBarrier(VK_ACCESS_TRANSFORM_FEEDBACK_WRITE_BIT_EXT, VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT));
        const auto dstStages = (VK_PIPELINE_STAGE_VERTEX_INPUT_BIT | VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT);
        cmdPipelineMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_TRANSFORM_FEEDBACK_BIT_EXT, dstStages,
                                 de::dataOrNull(barriers), barriers.size());
    }

    // Second render pass.
    if (m_params.useShaderObjects)
        beginRendering(ctx.vkd, cmdBuffer, colorDrawBuffer.getImageView(), scissors.at(0u), apiClearColor,
                       VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    else
        beginRenderPass(ctx.vkd, cmdBuffer, *renderPasses.back(), *frameBuffers.back(), scissors.at(0u));

    if (m_params.useShaderObjects)
    {
        bindShaders(m_context, m_params, cmdBuffer, vertShader, fragShader, tescShader, teseShader, geomShader);
        vkt::shaderobjutil::bindShaderObjectState(
            ctx.vkd, m_context.getDeviceExtensions(), cmdBuffer, viewports, scissors, indirectDrawTopology,
            patchControlPoints, &vertexInputStateCreateInfo, &rasterizationDrawState, nullptr, nullptr, nullptr);
    }
    else
    {
        const auto bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        ctx.vkd.cmdBindPipeline(cmdBuffer, bindPoint, *pipelines.back());
    }

    // Indirect draw.
    ctx.vkd.cmdBindVertexBuffers(cmdBuffer, 0u, 1u, &xfbBuffer.get(), &vertexBufferOffset);
    ctx.vkd.cmdDrawIndirectByteCountEXT(cmdBuffer, 1u, 0u, counterBuffer.get(), 0ull, 0u, DE_SIZEOF32(tcu::Vec4));

    if (m_params.useShaderObjects)
        ctx.vkd.cmdEndRendering(cmdBuffer);
    else
        ctx.vkd.cmdEndRenderPass(cmdBuffer);

    // Copy color buffers.
    const auto copyExtent = fbExtent.swizzle(0, 1);
    if (colorXfbBuffer)
        copyImageToBuffer(ctx.vkd, cmdBuffer, colorXfbBuffer->getImage(), colorXfbBuffer->getBuffer(), copyExtent);
    copyImageToBuffer(ctx.vkd, cmdBuffer, colorDrawBuffer.getImage(), colorDrawBuffer.getBuffer(), copyExtent);

    endCommandBuffer(ctx.vkd, cmdBuffer);
    submitCommandsAndWait(ctx.vkd, ctx.device, ctx.queue, cmdBuffer);

    // Verify color buffers.
    tcu::TextureLevel referenceLevel(tcuFormat, fbExtent.x(), fbExtent.y(), fbExtent.z());
    auto referenceAccess = referenceLevel.getAccess();
    const tcu::Vec4 geometryColor(0.0f, 0.0f, 1.0f, 1.0f); // Must match fragment shader.
    tcu::clear(referenceAccess, geometryColor);

    if (colorXfbBuffer)
        invalidateAlloc(ctx.vkd, ctx.device, colorXfbBuffer->getBufferAllocation());
    invalidateAlloc(ctx.vkd, ctx.device, colorDrawBuffer.getBufferAllocation());

    auto &log = m_context.getTestContext().getLog();
    const tcu::Vec4 threshold(0.0f, 0.0f, 0.0f, 0.0f);

    bool fail = false;
    if (colorXfbBuffer)
    {
        tcu::ConstPixelBufferAccess resultAccess(tcuFormat, fbExtent,
                                                 colorXfbBuffer->getBufferAllocation().getHostPtr());
        if (!tcu::floatThresholdCompare(log, "XFBDrawResult", "", referenceAccess, resultAccess, threshold,
                                        tcu::COMPARE_LOG_ON_ERROR))
            fail = true;
    }
    {
        tcu::ConstPixelBufferAccess resultAccess(tcuFormat, fbExtent,
                                                 colorDrawBuffer.getBufferAllocation().getHostPtr());
        if (!tcu::floatThresholdCompare(log, "IndirectDrawResult", "", referenceAccess, resultAccess, threshold,
                                        tcu::COMPARE_LOG_ON_ERROR))
            fail = true;
    }

    // Verify vertex output.
    std::vector<tcu::Vec4> xfbOutput(outputVertexCount);
    invalidateAlloc(ctx.vkd, ctx.device, xfbBufferAlloc);
    deMemcpy(de::dataOrNull(xfbOutput), xfbBufferData, static_cast<size_t>(xfbBufferSize));

    std::vector<tcu::Vec4> expectedXfbOutput;
    expectedXfbOutput.reserve(outputVertexCount);
    {
        if (m_params.useTess)
        {
            // In this case the output triangles come from the patch list (triangle list), so we copy vertices directly.
            DE_ASSERT(de::sizeU32(vertices) == outputVertexCount);
            std::copy(begin(vertices), end(vertices), std::back_inserter(expectedXfbOutput));
        }
        else
        {
            // In this case we build the triangle list from the strip.

            // First quad.
            expectedXfbOutput.push_back(vertices.at(0u));
            expectedXfbOutput.push_back(vertices.at(1u));
            expectedXfbOutput.push_back(vertices.at(2u));

            expectedXfbOutput.push_back(vertices.at(2u));
            expectedXfbOutput.push_back(vertices.at(1u));
            expectedXfbOutput.push_back(vertices.at(3u));

            // Second quad.
            expectedXfbOutput.push_back(vertices.at(4u));
            expectedXfbOutput.push_back(vertices.at(5u));
            expectedXfbOutput.push_back(vertices.at(6u));

            expectedXfbOutput.push_back(vertices.at(6u));
            expectedXfbOutput.push_back(vertices.at(5u));
            expectedXfbOutput.push_back(vertices.at(7u));
        }
    }

    for (uint32_t i = 0; i < outputTriangleCount; ++i)
    {
        const auto firstVertex = i * perTriangleVertices;

        const auto j1 = firstVertex + 0u;
        const auto j2 = firstVertex + 1u;
        const auto j3 = firstVertex + 2u;

        const auto &a1 = expectedXfbOutput.at(j1);
        const auto &a2 = expectedXfbOutput.at(j2);
        const auto &a3 = expectedXfbOutput.at(j3);

        const auto &b1 = xfbOutput.at(j1);
        const auto &b2 = xfbOutput.at(j2);
        const auto &b3 = xfbOutput.at(j3);

        bool triangleOK = verifyTriangle(a1, a2, a3, b1, b2, b3);
        if (!triangleOK)
        {
            log << tcu::TestLog::Message << "Error in output triangle " << i << ": expected in any order " << a1 << " "
                << a2 << " " << a3 << " but found " << b1 << " " << b2 << " " << b3 << tcu::TestLog::EndMessage;
            fail = true;
        }
    }

    if (fail)
        return tcu::TestStatus::fail("Unexpected result in color buffers or vertex buffers; check log for details");
    return tcu::TestStatus::pass("Pass");
}

} // anonymous namespace

tcu::TestCaseGroup *createDGCGraphicsXfbTestsExt(tcu::TestContext &testCtx)
{
    using GroupPtr = de::MovePtr<tcu::TestCaseGroup>;

    GroupPtr mainGroup(new tcu::TestCaseGroup(testCtx, "xfb"));

    for (const bool discardXFB : {false, true})
        for (const bool useGeom : {false, true})
            for (const bool useTess : {false, true})
                for (const bool useShaderObjects : {false, true})
                {
                    const Params params{
                        discardXFB,
                        useGeom,
                        useTess,
                        useShaderObjects,
                    };

                    const auto testName = std::string(discardXFB ? "discard" : "nodiscard") + (useTess ? "_tess" : "") +
                                          (useGeom ? "_geom" : "") + (useShaderObjects ? "_shader_objects" : "");

                    mainGroup->addChild(new XfbTestCase(testCtx, testName, params));
                }

    return mainGroup.release();
}

} // namespace DGC
} // namespace vkt
