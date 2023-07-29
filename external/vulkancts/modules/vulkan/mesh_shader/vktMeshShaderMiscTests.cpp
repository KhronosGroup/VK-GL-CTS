/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2021 The Khronos Group Inc.
 * Copyright (c) 2021 Valve Corporation.
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
 * \brief Mesh Shader Misc Tests
 *//*--------------------------------------------------------------------*/

#include "vktMeshShaderMiscTests.hpp"
#include "vktTestCase.hpp"

#include "vkBuilderUtil.hpp"
#include "vkImageWithMemory.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkObjUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkBarrierUtil.hpp"

#include "tcuImageCompare.hpp"
#include "tcuTexture.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuMaybe.hpp"
#include "tcuStringTemplate.hpp"
#include "tcuTestLog.hpp"

#include <memory>
#include <utility>
#include <vector>
#include <string>
#include <sstream>
#include <map>

namespace vkt
{
namespace MeshShader
{

namespace
{

using GroupPtr = de::MovePtr<tcu::TestCaseGroup>;

using namespace vk;

// Output images will use this format.
VkFormat getOutputFormat()
{
    return VK_FORMAT_R8G8B8A8_UNORM;
}

// Threshold that's reasonable for the previous format.
float getCompareThreshold()
{
    return 0.005f; // 1/256 < 0.005 < 2/256
}

// Check mesh shader support.
void genericCheckSupport(Context &context, bool requireTaskShader, bool requireVertexStores)
{
    context.requireDeviceFunctionality("VK_NV_mesh_shader");

    const auto &meshFeatures = context.getMeshShaderFeatures();

    if (!meshFeatures.meshShader)
        TCU_THROW(NotSupportedError, "Mesh shader not supported");

    if (requireTaskShader && !meshFeatures.taskShader)
        TCU_THROW(NotSupportedError, "Task shader not supported");

    if (requireVertexStores)
    {
        const auto &features = context.getDeviceFeatures();
        if (!features.vertexPipelineStoresAndAtomics)
            TCU_THROW(NotSupportedError, "Vertex pieline stores and atomics not supported");
    }
}

struct MiscTestParams
{
    tcu::Maybe<uint32_t> taskCount;
    uint32_t meshCount;

    uint32_t width;
    uint32_t height;

    // Makes the class polymorphic and allows the right destructor to be used for subclasses.
    virtual ~MiscTestParams()
    {
    }

    bool needsTaskShader() const
    {
        return static_cast<bool>(taskCount);
    }

    uint32_t drawCount() const
    {
        if (needsTaskShader())
            return taskCount.get();
        return meshCount;
    }
};

using ParamsPtr = std::unique_ptr<MiscTestParams>;

class MeshShaderMiscCase : public vkt::TestCase
{
public:
    MeshShaderMiscCase(tcu::TestContext &testCtx, const std::string &name, const std::string &description,
                       ParamsPtr params);
    virtual ~MeshShaderMiscCase(void)
    {
    }

    void checkSupport(Context &context) const override;
    void initPrograms(vk::SourceCollections &programCollection) const override;

protected:
    std::unique_ptr<MiscTestParams> m_params;
};

MeshShaderMiscCase::MeshShaderMiscCase(tcu::TestContext &testCtx, const std::string &name,
                                       const std::string &description, ParamsPtr params)
    : vkt::TestCase(testCtx, name, description)
    , m_params(params.release())
{
}

void MeshShaderMiscCase::checkSupport(Context &context) const
{
    genericCheckSupport(context, m_params->needsTaskShader(), /*requireVertexStores*/ false);
}

// Adds the generic fragment shader. To be called by subclasses.
void MeshShaderMiscCase::initPrograms(vk::SourceCollections &programCollection) const
{
    std::string frag = "#version 450\n"
                       "#extension GL_NV_mesh_shader : enable\n"
                       "\n"
                       "layout (location=0) in perprimitiveNV vec4 primitiveColor;\n"
                       "layout (location=0) out vec4 outColor;\n"
                       "\n"
                       "void main ()\n"
                       "{\n"
                       "    outColor = primitiveColor;\n"
                       "}\n";
    programCollection.glslSources.add("frag") << glu::FragmentSource(frag);
}

class MeshShaderMiscInstance : public vkt::TestInstance
{
public:
    MeshShaderMiscInstance(Context &context, const MiscTestParams *params)
        : vkt::TestInstance(context)
        , m_params(params)
        , m_referenceLevel()
    {
    }

    void generateSolidRefLevel(const tcu::Vec4 &color, std::unique_ptr<tcu::TextureLevel> &output);
    virtual void generateReferenceLevel() = 0;

    virtual bool verifyResult(const tcu::ConstPixelBufferAccess &resultAccess,
                              const tcu::TextureLevel &referenceLevel) const;
    virtual bool verifyResult(const tcu::ConstPixelBufferAccess &resultAccess) const;
    tcu::TestStatus iterate() override;

protected:
    const MiscTestParams *m_params;
    std::unique_ptr<tcu::TextureLevel> m_referenceLevel;
};

void MeshShaderMiscInstance::generateSolidRefLevel(const tcu::Vec4 &color, std::unique_ptr<tcu::TextureLevel> &output)
{
    const auto format    = getOutputFormat();
    const auto tcuFormat = mapVkFormat(format);

    const auto iWidth  = static_cast<int>(m_params->width);
    const auto iHeight = static_cast<int>(m_params->height);

    output.reset(new tcu::TextureLevel(tcuFormat, iWidth, iHeight));

    const auto access = output->getAccess();

    // Fill with solid color.
    tcu::clear(access, color);
}

bool MeshShaderMiscInstance::verifyResult(const tcu::ConstPixelBufferAccess &resultAccess) const
{
    return verifyResult(resultAccess, *m_referenceLevel);
}

bool MeshShaderMiscInstance::verifyResult(const tcu::ConstPixelBufferAccess &resultAccess,
                                          const tcu::TextureLevel &referenceLevel) const
{
    const auto referenceAccess = referenceLevel.getAccess();

    const auto refWidth  = referenceAccess.getWidth();
    const auto refHeight = referenceAccess.getHeight();
    const auto refDepth  = referenceAccess.getDepth();

    const auto resWidth  = resultAccess.getWidth();
    const auto resHeight = resultAccess.getHeight();
    const auto resDepth  = resultAccess.getDepth();

    DE_ASSERT(resWidth == refWidth || resHeight == refHeight || resDepth == refDepth);

    // For release builds.
    DE_UNREF(refWidth);
    DE_UNREF(refHeight);
    DE_UNREF(refDepth);
    DE_UNREF(resWidth);
    DE_UNREF(resHeight);
    DE_UNREF(resDepth);

    const auto outputFormat   = getOutputFormat();
    const auto expectedFormat = mapVkFormat(outputFormat);
    const auto resFormat      = resultAccess.getFormat();
    const auto refFormat      = referenceAccess.getFormat();

    DE_ASSERT(resFormat == expectedFormat && refFormat == expectedFormat);

    // For release builds
    DE_UNREF(expectedFormat);
    DE_UNREF(resFormat);
    DE_UNREF(refFormat);

    auto &log            = m_context.getTestContext().getLog();
    const auto threshold = getCompareThreshold();
    const tcu::Vec4 thresholdVec(threshold, threshold, threshold, threshold);

    return tcu::floatThresholdCompare(log, "Result", "", referenceAccess, resultAccess, thresholdVec,
                                      tcu::COMPARE_LOG_ON_ERROR);
}

tcu::TestStatus MeshShaderMiscInstance::iterate()
{
    const auto &vkd       = m_context.getDeviceInterface();
    const auto device     = m_context.getDevice();
    auto &alloc           = m_context.getDefaultAllocator();
    const auto queueIndex = m_context.getUniversalQueueFamilyIndex();
    const auto queue      = m_context.getUniversalQueue();

    const auto imageFormat = getOutputFormat();
    const auto tcuFormat   = mapVkFormat(imageFormat);
    const auto imageExtent = makeExtent3D(m_params->width, m_params->height, 1u);
    const auto imageUsage  = (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);

    const VkImageCreateInfo colorBufferInfo = {
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, // VkStructureType sType;
        nullptr,                             // const void* pNext;
        0u,                                  // VkImageCreateFlags flags;
        VK_IMAGE_TYPE_2D,                    // VkImageType imageType;
        imageFormat,                         // VkFormat format;
        imageExtent,                         // VkExtent3D extent;
        1u,                                  // uint32_t mipLevels;
        1u,                                  // uint32_t arrayLayers;
        VK_SAMPLE_COUNT_1_BIT,               // VkSampleCountFlagBits samples;
        VK_IMAGE_TILING_OPTIMAL,             // VkImageTiling tiling;
        imageUsage,                          // VkImageUsageFlags usage;
        VK_SHARING_MODE_EXCLUSIVE,           // VkSharingMode sharingMode;
        0u,                                  // uint32_t queueFamilyIndexCount;
        nullptr,                             // const uint32_t* pQueueFamilyIndices;
        VK_IMAGE_LAYOUT_UNDEFINED,           // VkImageLayout initialLayout;
    };

    // Create color image and view.
    ImageWithMemory colorImage(vkd, device, alloc, colorBufferInfo, MemoryRequirement::Any);
    const auto colorSRR  = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
    const auto colorSRL  = makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u);
    const auto colorView = makeImageView(vkd, device, colorImage.get(), VK_IMAGE_VIEW_TYPE_2D, imageFormat, colorSRR);

    // Create a memory buffer for verification.
    const auto verificationBufferSize =
        static_cast<VkDeviceSize>(imageExtent.width * imageExtent.height * tcu::getPixelSize(tcuFormat));
    const auto verificationBufferUsage = (VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    const auto verificationBufferInfo  = makeBufferCreateInfo(verificationBufferSize, verificationBufferUsage);

    BufferWithMemory verificationBuffer(vkd, device, alloc, verificationBufferInfo, MemoryRequirement::HostVisible);
    auto &verificationBufferAlloc = verificationBuffer.getAllocation();
    void *verificationBufferData  = verificationBufferAlloc.getHostPtr();

    // Pipeline layout.
    const auto pipelineLayout = makePipelineLayout(vkd, device);

    // Shader modules.
    const auto &binaries = m_context.getBinaryCollection();
    const auto hasTask   = binaries.contains("task");

    const auto meshShader = createShaderModule(vkd, device, binaries.get("mesh"));
    const auto fragShader = createShaderModule(vkd, device, binaries.get("frag"));

    Move<VkShaderModule> taskShader;
    if (hasTask)
        taskShader = createShaderModule(vkd, device, binaries.get("task"));

    // Render pass.
    const auto renderPass = makeRenderPass(vkd, device, imageFormat);

    // Framebuffer.
    const auto framebuffer =
        makeFramebuffer(vkd, device, renderPass.get(), colorView.get(), imageExtent.width, imageExtent.height);

    // Viewport and scissor.
    const std::vector<VkViewport> viewports(1u, makeViewport(imageExtent));
    const std::vector<VkRect2D> scissors(1u, makeRect2D(imageExtent));

    const auto pipeline = makeGraphicsPipeline(vkd, device, pipelineLayout.get(), taskShader.get(), meshShader.get(),
                                               fragShader.get(), renderPass.get(), viewports, scissors);

    // Command pool and buffer.
    const auto cmdPool      = makeCommandPool(vkd, device, queueIndex);
    const auto cmdBufferPtr = allocateCommandBuffer(vkd, device, cmdPool.get(), VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    const auto cmdBuffer    = cmdBufferPtr.get();

    beginCommandBuffer(vkd, cmdBuffer);

    // Run pipeline.
    const tcu::Vec4 clearColor(0.0f, 0.0f, 0.0f, 0.0f);
    beginRenderPass(vkd, cmdBuffer, renderPass.get(), framebuffer.get(), scissors.at(0u), clearColor);
    vkd.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.get());
    vkd.cmdDrawMeshTasksNV(cmdBuffer, m_params->drawCount(), 0u);
    endRenderPass(vkd, cmdBuffer);

    // Copy color buffer to verification buffer.
    const auto colorAccess   = (VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT);
    const auto transferRead  = VK_ACCESS_TRANSFER_READ_BIT;
    const auto transferWrite = VK_ACCESS_TRANSFER_WRITE_BIT;
    const auto hostRead      = VK_ACCESS_HOST_READ_BIT;

    const auto preCopyBarrier =
        makeImageMemoryBarrier(colorAccess, transferRead, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                               VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, colorImage.get(), colorSRR);
    const auto postCopyBarrier = makeMemoryBarrier(transferWrite, hostRead);
    const auto copyRegion      = makeBufferImageCopy(imageExtent, colorSRL);

    vkd.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u,
                           0u, nullptr, 0u, nullptr, 1u, &preCopyBarrier);
    vkd.cmdCopyImageToBuffer(cmdBuffer, colorImage.get(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                             verificationBuffer.get(), 1u, &copyRegion);
    vkd.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0u, 1u,
                           &postCopyBarrier, 0u, nullptr, 0u, nullptr);

    endCommandBuffer(vkd, cmdBuffer);
    submitCommandsAndWait(vkd, device, queue, cmdBuffer);

    // Generate reference image and compare results.
    const tcu::IVec3 iExtent(static_cast<int>(imageExtent.width), static_cast<int>(imageExtent.height), 1);
    const tcu::ConstPixelBufferAccess verificationAccess(tcuFormat, iExtent, verificationBufferData);

    generateReferenceLevel();
    invalidateAlloc(vkd, device, verificationBufferAlloc);
    if (!verifyResult(verificationAccess))
        TCU_FAIL("Result does not match reference; check log for details");

    return tcu::TestStatus::pass("Pass");
}

// Verify passing more complex data between the task and mesh shaders.
class ComplexTaskDataCase : public MeshShaderMiscCase
{
public:
    ComplexTaskDataCase(tcu::TestContext &testCtx, const std::string &name, const std::string &description,
                        ParamsPtr params)
        : MeshShaderMiscCase(testCtx, name, description, std::move(params))
    {
    }

    void initPrograms(vk::SourceCollections &programCollection) const override;
    TestInstance *createInstance(Context &context) const override;
};

class ComplexTaskDataInstance : public MeshShaderMiscInstance
{
public:
    ComplexTaskDataInstance(Context &context, const MiscTestParams *params) : MeshShaderMiscInstance(context, params)
    {
    }

    void generateReferenceLevel() override;
};

void ComplexTaskDataInstance::generateReferenceLevel()
{
    const auto format    = getOutputFormat();
    const auto tcuFormat = mapVkFormat(format);

    const auto iWidth  = static_cast<int>(m_params->width);
    const auto iHeight = static_cast<int>(m_params->height);

    const auto halfWidth  = iWidth / 2;
    const auto halfHeight = iHeight / 2;

    m_referenceLevel.reset(new tcu::TextureLevel(tcuFormat, iWidth, iHeight));

    const auto access = m_referenceLevel->getAccess();

    // Each image quadrant gets a different color.
    for (int y = 0; y < iHeight; ++y)
        for (int x = 0; x < iWidth; ++x)
        {
            const float red     = ((y < halfHeight) ? 0.0f : 1.0f);
            const float green   = ((x < halfWidth) ? 0.0f : 1.0f);
            const auto refColor = tcu::Vec4(red, green, 1.0f, 1.0f);
            access.setPixel(refColor, x, y);
        }
}

void ComplexTaskDataCase::initPrograms(vk::SourceCollections &programCollection) const
{
    // Add the generic fragment shader.
    MeshShaderMiscCase::initPrograms(programCollection);

    const std::string taskDataDeclTemplate = "struct RowId {\n"
                                             "    uint id;\n"
                                             "};\n"
                                             "\n"
                                             "struct WorkGroupData {\n"
                                             "    float WorkGroupIdPlusOnex1000Iota[10];\n"
                                             "    RowId rowId;\n"
                                             "    uvec3 WorkGroupIdPlusOnex2000Iota;\n"
                                             "    vec2  WorkGroupIdPlusOnex3000Iota;\n"
                                             "};\n"
                                             "\n"
                                             "struct ExternalData {\n"
                                             "    float OneMillion;\n"
                                             "    uint  TwoMillion;\n"
                                             "    WorkGroupData workGroupData;\n"
                                             "};\n"
                                             "\n"
                                             "${INOUT} taskNV TaskData {\n"
                                             "    uint yes;\n"
                                             "    ExternalData externalData;\n"
                                             "} td;\n";
    const tcu::StringTemplate taskDataDecl(taskDataDeclTemplate);

    {
        std::map<std::string, std::string> taskMap;
        taskMap["INOUT"] = "out";
        std::ostringstream task;
        task << "#version 450\n"
             << "#extension GL_NV_mesh_shader : enable\n"
             << "\n"
             << "layout (local_size_x=1) in;\n"
             << "\n"
             << taskDataDecl.specialize(taskMap) << "\n"
             << "void main ()\n"
             << "{\n"
             << "    gl_TaskCountNV = 2u;\n"
             << "    td.yes = 1u;\n"
             << "    td.externalData.OneMillion = 1000000.0;\n"
             << "    td.externalData.TwoMillion = 2000000u;\n"
             << "    for (uint i = 0; i < 10; i++) {\n"
             << "        td.externalData.workGroupData.WorkGroupIdPlusOnex1000Iota[i] = float((gl_WorkGroupID.x + 1u) "
                "* 1000 + i);\n"
             << "    }\n"
             << "    {\n"
             << "        uint baseVal = (gl_WorkGroupID.x + 1u) * 2000;\n"
             << "        td.externalData.workGroupData.WorkGroupIdPlusOnex2000Iota = uvec3(baseVal, baseVal + 1, "
                "baseVal + 2);\n"
             << "    }\n"
             << "    {\n"
             << "        uint baseVal = (gl_WorkGroupID.x + 1u) * 3000;\n"
             << "        td.externalData.workGroupData.WorkGroupIdPlusOnex3000Iota = vec2(baseVal, baseVal + 1);\n"
             << "    }\n"
             << "    td.externalData.workGroupData.rowId.id = gl_WorkGroupID.x;\n"
             << "}\n";
        programCollection.glslSources.add("task") << glu::TaskSource(task.str());
    }

    {
        std::map<std::string, std::string> meshMap;
        meshMap["INOUT"] = "in";
        std::ostringstream mesh;
        mesh
            << "#version 450\n"
            << "#extension GL_NV_mesh_shader : enable\n"
            << "\n"
            << "layout(local_size_x=2) in;\n"
            << "layout(triangles) out;\n"
            << "layout(max_vertices=4, max_primitives=2) out;\n"
            << "\n"
            << "layout (location=0) out perprimitiveNV vec4 triangleColor[];\n"
            << "\n"
            << taskDataDecl.specialize(meshMap) << "\n"
            << "void main ()\n"
            << "{\n"
            << "    bool dataOK = true;\n"
            << "    dataOK = (dataOK && (td.yes == 1u));\n"
            << "    dataOK = (dataOK && (td.externalData.OneMillion == 1000000.0 && td.externalData.TwoMillion == "
               "2000000u));\n"
            << "    uint rowId = td.externalData.workGroupData.rowId.id;\n"
            << "    dataOK = (dataOK && (rowId == 0u || rowId == 1u));\n"
            << "\n"
            << "    {\n"
            << "        uint baseVal = (rowId + 1u) * 1000u;\n"
            << "        for (uint i = 0; i < 10; i++) {\n"
            << "            if (td.externalData.workGroupData.WorkGroupIdPlusOnex1000Iota[i] != float(baseVal + i)) {\n"
            << "                dataOK = false;\n"
            << "                break;\n"
            << "            }\n"
            << "        }\n"
            << "    }\n"
            << "\n"
            << "    {\n"
            << "        uint baseVal = (rowId + 1u) * 2000;\n"
            << "        uvec3 expected = uvec3(baseVal, baseVal + 1, baseVal + 2);\n"
            << "        if (td.externalData.workGroupData.WorkGroupIdPlusOnex2000Iota != expected) {\n"
            << "            dataOK = false;\n"
            << "        }\n"
            << "    }\n"
            << "\n"
            << "    {\n"
            << "        uint baseVal = (rowId + 1u) * 3000;\n"
            << "        vec2 expected = vec2(baseVal, baseVal + 1);\n"
            << "        if (td.externalData.workGroupData.WorkGroupIdPlusOnex3000Iota != expected) {\n"
            << "            dataOK = false;\n"
            << "        }\n"
            << "    }\n"
            << "\n"
            << "    uint columnId = gl_WorkGroupID.x;\n"
            << "\n"
            << "    if (dataOK) {\n"
            << "        gl_PrimitiveCountNV = 2u;\n"
            << "    }\n"
            << "    else {\n"
            << "        gl_PrimitiveCountNV = 0u;\n"
            << "        return;\n"
            << "    }\n"
            << "\n"
            << "    const vec4 outColor = vec4(rowId, columnId, 1.0f, 1.0f);\n"
            << "    triangleColor[0] = outColor;\n"
            << "    triangleColor[1] = outColor;\n"
            << "\n"
            << "    // Each local invocation will generate two points and one triangle from the quad.\n"
            << "    // The first local invocation will generate the top quad vertices.\n"
            << "    // The second invocation will generate the two bottom vertices.\n"
            << "    vec4 left  = vec4(0.0, 0.0, 0.0, 1.0);\n"
            << "    vec4 right = vec4(1.0, 0.0, 0.0, 1.0);\n"
            << "\n"
            << "    float localInvocationOffsetY = float(gl_LocalInvocationID.x);\n"
            << "    left.y  += localInvocationOffsetY;\n"
            << "    right.y += localInvocationOffsetY;\n"
            << "\n"
            << "    // The code above creates a quad from (0, 0) to (1, 1) but we need to offset it\n"
            << "    // in X and/or Y depending on the row and column, to place it in other quadrants.\n"
            << "    float quadrantOffsetX = float(int(columnId) - 1);\n"
            << "    float quadrantOffsetY = float(int(rowId) - 1);\n"
            << "\n"
            << "    left.x  += quadrantOffsetX;\n"
            << "    right.x += quadrantOffsetX;\n"
            << "\n"
            << "    left.y  += quadrantOffsetY;\n"
            << "    right.y += quadrantOffsetY;\n"
            << "\n"
            << "    uint baseVertexId = 2*gl_LocalInvocationID.x;\n"
            << "    gl_MeshVerticesNV[baseVertexId + 0].gl_Position = left;\n"
            << "    gl_MeshVerticesNV[baseVertexId + 1].gl_Position = right;\n"
            << "\n"
            << "    uint baseIndexId = 3*gl_LocalInvocationID.x;\n"
            << "    // 0,1,2 or 1,2,3 (note: triangles alternate front face this way)\n"
            << "    gl_PrimitiveIndicesNV[baseIndexId + 0] = 0 + gl_LocalInvocationID.x;\n"
            << "    gl_PrimitiveIndicesNV[baseIndexId + 1] = 1 + gl_LocalInvocationID.x;\n"
            << "    gl_PrimitiveIndicesNV[baseIndexId + 2] = 2 + gl_LocalInvocationID.x;\n"
            << "}\n";
        programCollection.glslSources.add("mesh") << glu::MeshSource(mesh.str());
    }
}

TestInstance *ComplexTaskDataCase::createInstance(Context &context) const
{
    return new ComplexTaskDataInstance(context, m_params.get());
}

// Verify drawing a single point.
class SinglePointCase : public MeshShaderMiscCase
{
public:
    SinglePointCase(tcu::TestContext &testCtx, const std::string &name, const std::string &description,
                    ParamsPtr params)
        : MeshShaderMiscCase(testCtx, name, description, std::move(params))
    {
    }

    void initPrograms(vk::SourceCollections &programCollection) const override;
    TestInstance *createInstance(Context &context) const override;
};

class SinglePointInstance : public MeshShaderMiscInstance
{
public:
    SinglePointInstance(Context &context, const MiscTestParams *params) : MeshShaderMiscInstance(context, params)
    {
    }

    void generateReferenceLevel() override;
};

TestInstance *SinglePointCase::createInstance(Context &context) const
{
    return new SinglePointInstance(context, m_params.get());
}

void SinglePointCase::initPrograms(vk::SourceCollections &programCollection) const
{
    DE_ASSERT(!m_params->needsTaskShader());

    MeshShaderMiscCase::initPrograms(programCollection);

    std::ostringstream mesh;
    mesh << "#version 450\n"
         << "#extension GL_NV_mesh_shader : enable\n"
         << "\n"
         << "layout(local_size_x=1) in;\n"
         << "layout(points) out;\n"
         << "layout(max_vertices=256, max_primitives=256) out;\n"
         << "\n"
         << "layout (location=0) out perprimitiveNV vec4 pointColor[];\n"
         << "\n"
         << "void main ()\n"
         << "{\n"
         << "    gl_PrimitiveCountNV = 1u;\n"
         << "    pointColor[0] = vec4(0.0f, 1.0f, 1.0f, 1.0f);\n"
         << "    gl_MeshVerticesNV[0].gl_Position = vec4(0.0f, 0.0f, 0.0f, 1.0f);\n"
         << "    gl_MeshVerticesNV[0].gl_PointSize = 1.0f;\n"
         << "    gl_PrimitiveIndicesNV[0] = 0;\n"
         << "}\n";
    programCollection.glslSources.add("mesh") << glu::MeshSource(mesh.str());
}

void SinglePointInstance::generateReferenceLevel()
{
    generateSolidRefLevel(tcu::Vec4(0.0f, 0.0f, 0.0f, 0.0f), m_referenceLevel);

    const auto halfWidth  = static_cast<int>(m_params->width / 2u);
    const auto halfHeight = static_cast<int>(m_params->height / 2u);
    const auto access     = m_referenceLevel->getAccess();

    access.setPixel(tcu::Vec4(0.0f, 1.0f, 1.0f, 1.0f), halfWidth, halfHeight);
}

// Verify drawing a single line.
class SingleLineCase : public MeshShaderMiscCase
{
public:
    SingleLineCase(tcu::TestContext &testCtx, const std::string &name, const std::string &description, ParamsPtr params)
        : MeshShaderMiscCase(testCtx, name, description, std::move(params))
    {
    }

    void initPrograms(vk::SourceCollections &programCollection) const override;
    TestInstance *createInstance(Context &context) const override;
};

class SingleLineInstance : public MeshShaderMiscInstance
{
public:
    SingleLineInstance(Context &context, const MiscTestParams *params) : MeshShaderMiscInstance(context, params)
    {
    }

    void generateReferenceLevel() override;
};

TestInstance *SingleLineCase::createInstance(Context &context) const
{
    return new SingleLineInstance(context, m_params.get());
}

void SingleLineCase::initPrograms(vk::SourceCollections &programCollection) const
{
    DE_ASSERT(!m_params->needsTaskShader());

    MeshShaderMiscCase::initPrograms(programCollection);

    std::ostringstream mesh;
    mesh << "#version 450\n"
         << "#extension GL_NV_mesh_shader : enable\n"
         << "\n"
         << "layout(local_size_x=1) in;\n"
         << "layout(lines) out;\n"
         << "layout(max_vertices=256, max_primitives=256) out;\n"
         << "\n"
         << "layout (location=0) out perprimitiveNV vec4 lineColor[];\n"
         << "\n"
         << "void main ()\n"
         << "{\n"
         << "    gl_PrimitiveCountNV = 1u;\n"
         << "    lineColor[0] = vec4(0.0f, 1.0f, 1.0f, 1.0f);\n"
         << "    gl_MeshVerticesNV[0].gl_Position = vec4(-1.0f, 0.0f, 0.0f, 1.0f);\n"
         << "    gl_MeshVerticesNV[1].gl_Position = vec4( 1.0f, 0.0f, 0.0f, 1.0f);\n"
         << "    gl_PrimitiveIndicesNV[0] = 0;\n"
         << "    gl_PrimitiveIndicesNV[1] = 1;\n"
         << "}\n";
    programCollection.glslSources.add("mesh") << glu::MeshSource(mesh.str());
}

void SingleLineInstance::generateReferenceLevel()
{
    generateSolidRefLevel(tcu::Vec4(0.0f, 0.0f, 0.0f, 0.0f), m_referenceLevel);

    const auto iWidth     = static_cast<int>(m_params->width);
    const auto halfHeight = static_cast<int>(m_params->height / 2u);
    const auto access     = m_referenceLevel->getAccess();

    // Center row.
    for (int x = 0; x < iWidth; ++x)
        access.setPixel(tcu::Vec4(0.0f, 1.0f, 1.0f, 1.0f), x, halfHeight);
}

// Verify drawing a single triangle.
class SingleTriangleCase : public MeshShaderMiscCase
{
public:
    SingleTriangleCase(tcu::TestContext &testCtx, const std::string &name, const std::string &description,
                       ParamsPtr params)
        : MeshShaderMiscCase(testCtx, name, description, std::move(params))
    {
    }

    void initPrograms(vk::SourceCollections &programCollection) const override;
    TestInstance *createInstance(Context &context) const override;
};

class SingleTriangleInstance : public MeshShaderMiscInstance
{
public:
    SingleTriangleInstance(Context &context, const MiscTestParams *params) : MeshShaderMiscInstance(context, params)
    {
    }

    void generateReferenceLevel() override;
};

TestInstance *SingleTriangleCase::createInstance(Context &context) const
{
    return new SingleTriangleInstance(context, m_params.get());
}

void SingleTriangleCase::initPrograms(vk::SourceCollections &programCollection) const
{
    DE_ASSERT(!m_params->needsTaskShader());

    MeshShaderMiscCase::initPrograms(programCollection);

    const float halfPixelX = 2.0f / static_cast<float>(m_params->width);
    const float halfPixelY = 2.0f / static_cast<float>(m_params->height);

    std::ostringstream mesh;
    mesh << "#version 450\n"
         << "#extension GL_NV_mesh_shader : enable\n"
         << "\n"
         << "layout(local_size_x=1) in;\n"
         << "layout(triangles) out;\n"
         << "layout(max_vertices=256, max_primitives=256) out;\n"
         << "\n"
         << "layout (location=0) out perprimitiveNV vec4 triangleColor[];\n"
         << "\n"
         << "void main ()\n"
         << "{\n"
         << "    gl_PrimitiveCountNV = 1u;\n"
         << "    triangleColor[0] = vec4(0.0f, 1.0f, 1.0f, 1.0f);\n"
         << "    gl_MeshVerticesNV[0].gl_Position = vec4(" << halfPixelY << ", " << -halfPixelX << ", 0.0f, 1.0f);\n"
         << "    gl_MeshVerticesNV[1].gl_Position = vec4(" << halfPixelY << ", " << halfPixelX << ", 0.0f, 1.0f);\n"
         << "    gl_MeshVerticesNV[2].gl_Position = vec4(" << -halfPixelY << ", 0.0f, 0.0f, 1.0f);\n"
         << "    gl_PrimitiveIndicesNV[0] = 0;\n"
         << "    gl_PrimitiveIndicesNV[1] = 1;\n"
         << "    gl_PrimitiveIndicesNV[2] = 2;\n"
         << "}\n";
    programCollection.glslSources.add("mesh") << glu::MeshSource(mesh.str());
}

void SingleTriangleInstance::generateReferenceLevel()
{
    generateSolidRefLevel(tcu::Vec4(0.0f, 0.0f, 0.0f, 0.0f), m_referenceLevel);

    const auto halfWidth  = static_cast<int>(m_params->width / 2u);
    const auto halfHeight = static_cast<int>(m_params->height / 2u);
    const auto access     = m_referenceLevel->getAccess();

    // Single pixel in the center.
    access.setPixel(tcu::Vec4(0.0f, 1.0f, 1.0f, 1.0f), halfWidth, halfHeight);
}

// Verify drawing the maximum number of points.
class MaxPointsCase : public MeshShaderMiscCase
{
public:
    MaxPointsCase(tcu::TestContext &testCtx, const std::string &name, const std::string &description, ParamsPtr params)
        : MeshShaderMiscCase(testCtx, name, description, std::move(params))
    {
    }

    void initPrograms(vk::SourceCollections &programCollection) const override;
    TestInstance *createInstance(Context &context) const override;
};

class MaxPointsInstance : public MeshShaderMiscInstance
{
public:
    MaxPointsInstance(Context &context, const MiscTestParams *params) : MeshShaderMiscInstance(context, params)
    {
    }

    void generateReferenceLevel() override;
};

TestInstance *MaxPointsCase::createInstance(Context &context) const
{
    return new MaxPointsInstance(context, m_params.get());
}

void MaxPointsCase::initPrograms(vk::SourceCollections &programCollection) const
{
    DE_ASSERT(!m_params->needsTaskShader());

    MeshShaderMiscCase::initPrograms(programCollection);

    // Fill a 16x16 image with 256 points. Each of the 32 local invocations will handle a segment of 8 pixels. Two segments per row.
    DE_ASSERT(m_params->width == 16u && m_params->height == 16u);

    std::ostringstream mesh;
    mesh << "#version 450\n"
         << "#extension GL_NV_mesh_shader : enable\n"
         << "\n"
         << "layout(local_size_x=32) in;\n"
         << "layout(points) out;\n"
         << "layout(max_vertices=256, max_primitives=256) out;\n"
         << "\n"
         << "layout (location=0) out perprimitiveNV vec4 pointColor[];\n"
         << "\n"
         << "void main ()\n"
         << "{\n"
         << "    gl_PrimitiveCountNV = 256u;\n"
         << "    uint firstPixel = 8u * gl_LocalInvocationID.x;\n"
         << "    uint row = firstPixel / 16u;\n"
         << "    uint col = firstPixel % 16u;\n"
         << "    float pixSize = 2.0f / 16.0f;\n"
         << "    float yCoord = pixSize * (float(row) + 0.5f) - 1.0f;\n"
         << "    float baseXCoord = pixSize * (float(col) + 0.5f) - 1.0f;\n"
         << "    for (uint i = 0; i < 8u; i++) {\n"
         << "        float xCoord = baseXCoord + pixSize * float(i);\n"
         << "        uint pixId = firstPixel + i;\n"
         << "        gl_MeshVerticesNV[pixId].gl_Position = vec4(xCoord, yCoord, 0.0f, 1.0f);\n"
         << "        gl_MeshVerticesNV[pixId].gl_PointSize = 1.0f;\n"
         << "        gl_PrimitiveIndicesNV[pixId] = pixId;\n"
         << "        pointColor[pixId] = vec4(((xCoord + 1.0f) / 2.0f), ((yCoord + 1.0f) / 2.0f), 0.0f, 1.0f);\n"
         << "    }\n"
         << "}\n";
    programCollection.glslSources.add("mesh") << glu::MeshSource(mesh.str());
}

void MaxPointsInstance::generateReferenceLevel()
{
    const auto format    = getOutputFormat();
    const auto tcuFormat = mapVkFormat(format);

    const auto iWidth  = static_cast<int>(m_params->width);
    const auto iHeight = static_cast<int>(m_params->height);
    const auto fWidth  = static_cast<float>(m_params->width);
    const auto fHeight = static_cast<float>(m_params->height);

    m_referenceLevel.reset(new tcu::TextureLevel(tcuFormat, iWidth, iHeight));

    const auto access = m_referenceLevel->getAccess();

    // Fill with gradient like the shader does.
    for (int y = 0; y < iHeight; ++y)
        for (int x = 0; x < iWidth; ++x)
        {
            const tcu::Vec4 color(((static_cast<float>(x) + 0.5f) / fWidth), ((static_cast<float>(y) + 0.5f) / fHeight),
                                  0.0f, 1.0f);
            access.setPixel(color, x, y);
        }
}

// Verify drawing the maximum number of lines.
class MaxLinesCase : public MeshShaderMiscCase
{
public:
    MaxLinesCase(tcu::TestContext &testCtx, const std::string &name, const std::string &description, ParamsPtr params)
        : MeshShaderMiscCase(testCtx, name, description, std::move(params))
    {
    }

    void initPrograms(vk::SourceCollections &programCollection) const override;
    TestInstance *createInstance(Context &context) const override;
};

class MaxLinesInstance : public MeshShaderMiscInstance
{
public:
    MaxLinesInstance(Context &context, const MiscTestParams *params) : MeshShaderMiscInstance(context, params)
    {
    }

    void generateReferenceLevel() override;
};

TestInstance *MaxLinesCase::createInstance(Context &context) const
{
    return new MaxLinesInstance(context, m_params.get());
}

void MaxLinesCase::initPrograms(vk::SourceCollections &programCollection) const
{
    DE_ASSERT(!m_params->needsTaskShader());

    MeshShaderMiscCase::initPrograms(programCollection);

    // Fill a 1x1020 image with 255 lines, each line being 4 pixels tall. Each invocation will generate ~8 lines.
    DE_ASSERT(m_params->width == 1u && m_params->height == 1020u);

    std::ostringstream mesh;
    mesh << "#version 450\n"
         << "#extension GL_NV_mesh_shader : enable\n"
         << "\n"
         << "layout(local_size_x=32) in;\n"
         << "layout(lines) out;\n"
         << "layout(max_vertices=256, max_primitives=255) out;\n"
         << "\n"
         << "layout (location=0) out perprimitiveNV vec4 lineColor[];\n"
         << "\n"
         << "void main ()\n"
         << "{\n"
         << "    gl_PrimitiveCountNV = 255u;\n"
         << "    uint firstLine = 8u * gl_LocalInvocationID.x;\n"
         << "    for (uint i = 0u; i < 8u; i++) {\n"
         << "        uint lineId = firstLine + i;\n"
         << "        uint topPixel = 4u * lineId;\n"
         << "        uint bottomPixel = 3u + topPixel;\n"
         << "        if (bottomPixel < 1020u) {\n"
         << "            float bottomCoord = ((float(bottomPixel) + 1.0f) / 1020.0) * 2.0 - 1.0;\n"
         << "            gl_MeshVerticesNV[lineId + 1u].gl_Position = vec4(0.0, bottomCoord, 0.0f, 1.0f);\n"
         << "            gl_PrimitiveIndicesNV[lineId * 2u] = lineId;\n"
         << "            gl_PrimitiveIndicesNV[lineId * 2u + 1u] = lineId + 1u;\n"
         << "            lineColor[lineId] = vec4(0.0f, 1.0f, float(lineId) / 255.0f, 1.0f);\n"
         << "        } else {\n"
         << "            // The last iteration of the last invocation emits the first point\n"
         << "            gl_MeshVerticesNV[0].gl_Position = vec4(0.0, -1.0, 0.0f, 1.0f);\n"
         << "        }\n"
         << "    }\n"
         << "}\n";
    programCollection.glslSources.add("mesh") << glu::MeshSource(mesh.str());
}

void MaxLinesInstance::generateReferenceLevel()
{
    const auto format    = getOutputFormat();
    const auto tcuFormat = mapVkFormat(format);

    const auto iWidth  = static_cast<int>(m_params->width);
    const auto iHeight = static_cast<int>(m_params->height);

    m_referenceLevel.reset(new tcu::TextureLevel(tcuFormat, iWidth, iHeight));

    const auto access = m_referenceLevel->getAccess();

    // Fill lines, 4 pixels per line.
    const uint32_t kNumLines   = 255u;
    const uint32_t kLineHeight = 4u;

    for (uint32_t i = 0u; i < kNumLines; ++i)
    {
        const tcu::Vec4 color(0.0f, 1.0f, static_cast<float>(i) / static_cast<float>(kNumLines), 1.0f);
        for (uint32_t j = 0u; j < kLineHeight; ++j)
            access.setPixel(color, 0, i * kLineHeight + j);
    }
}

// Verify drawing the maximum number of triangles.
class MaxTrianglesCase : public MeshShaderMiscCase
{
public:
    MaxTrianglesCase(tcu::TestContext &testCtx, const std::string &name, const std::string &description,
                     ParamsPtr params)
        : MeshShaderMiscCase(testCtx, name, description, std::move(params))
    {
    }

    void initPrograms(vk::SourceCollections &programCollection) const override;
    TestInstance *createInstance(Context &context) const override;
};

class MaxTrianglesInstance : public MeshShaderMiscInstance
{
public:
    MaxTrianglesInstance(Context &context, const MiscTestParams *params) : MeshShaderMiscInstance(context, params)
    {
    }

    void generateReferenceLevel() override;
};

TestInstance *MaxTrianglesCase::createInstance(Context &context) const
{
    return new MaxTrianglesInstance(context, m_params.get());
}

void MaxTrianglesCase::initPrograms(vk::SourceCollections &programCollection) const
{
    DE_ASSERT(!m_params->needsTaskShader());

    MeshShaderMiscCase::initPrograms(programCollection);

    // Fill a sufficiently large image with solid color. Generate a quarter of a circle with the center in the top left corner,
    // using a triangle fan that advances from top to bottom. Each invocation will generate ~8 triangles.
    std::ostringstream mesh;
    mesh << "#version 450\n"
         << "#extension GL_NV_mesh_shader : enable\n"
         << "\n"
         << "layout(local_size_x=32) in;\n"
         << "layout(triangles) out;\n"
         << "layout(max_vertices=256, max_primitives=254) out;\n"
         << "\n"
         << "layout (location=0) out perprimitiveNV vec4 triangleColor[];\n"
         << "\n"
         << "const float PI_2 = 1.57079632679489661923;\n"
         << "const float RADIUS = 4.5;\n"
         << "\n"
         << "void main ()\n"
         << "{\n"
         << "    gl_PrimitiveCountNV = 254u;\n"
         << "    uint firstTriangle = 8u * gl_LocalInvocationID.x;\n"
         << "    for (uint i = 0u; i < 8u; i++) {\n"
         << "        uint triangleId = firstTriangle + i;\n"
         << "        if (triangleId < 254u) {\n"
         << "            uint vertexId = triangleId + 2u;\n"
         << "            float angleProportion = float(vertexId - 1u) / 254.0f;\n"
         << "            float angle = PI_2 * angleProportion;\n"
         << "            float xCoord = cos(angle) * RADIUS - 1.0;\n"
         << "            float yCoord = sin(angle) * RADIUS - 1.0;\n"
         << "            gl_MeshVerticesNV[vertexId].gl_Position = vec4(xCoord, yCoord, 0.0, 1.0);\n"
         << "            gl_PrimitiveIndicesNV[triangleId * 3u + 0u] = 0u;\n"
         << "            gl_PrimitiveIndicesNV[triangleId * 3u + 1u] = triangleId + 1u;\n"
         << "            gl_PrimitiveIndicesNV[triangleId * 3u + 2u] = triangleId + 2u;\n"
         << "            triangleColor[triangleId] = vec4(0.0f, 0.0f, 1.0f, 1.0f);\n"
         << "        } else {\n"
         << "            // The last iterations of the last invocation emit the first two vertices\n"
         << "            uint vertexId = triangleId - 254u;\n"
         << "            if (vertexId == 0u) {\n"
         << "                gl_MeshVerticesNV[0u].gl_Position = vec4(-1.0, -1.0, 0.0, 1.0);\n"
         << "            } else {\n"
         << "                gl_MeshVerticesNV[1u].gl_Position = vec4(RADIUS, -1.0, 0.0, 1.0);\n"
         << "            }\n"
         << "        }\n"
         << "    }\n"
         << "}\n";
    programCollection.glslSources.add("mesh") << glu::MeshSource(mesh.str());
}

void MaxTrianglesInstance::generateReferenceLevel()
{
    generateSolidRefLevel(tcu::Vec4(0.0f, 0.0f, 1.0f, 1.0f), m_referenceLevel);
}

// Large work groups with many threads.
class LargeWorkGroupCase : public MeshShaderMiscCase
{
public:
    LargeWorkGroupCase(tcu::TestContext &testCtx, const std::string &name, const std::string &description,
                       ParamsPtr params)
        : MeshShaderMiscCase(testCtx, name, description, std::move(params))
    {
    }

    void initPrograms(vk::SourceCollections &programCollection) const override;
    TestInstance *createInstance(Context &context) const override;

    static constexpr uint32_t kLocalInvocations = 32u;
};

class LargeWorkGroupInstance : public MeshShaderMiscInstance
{
public:
    LargeWorkGroupInstance(Context &context, const MiscTestParams *params) : MeshShaderMiscInstance(context, params)
    {
    }

    void generateReferenceLevel() override;
};

TestInstance *LargeWorkGroupCase::createInstance(Context &context) const
{
    return new LargeWorkGroupInstance(context, m_params.get());
}

void LargeWorkGroupInstance::generateReferenceLevel()
{
    generateSolidRefLevel(tcu::Vec4(0.0f, 0.0f, 1.0f, 1.0f), m_referenceLevel);
}

void LargeWorkGroupCase::initPrograms(vk::SourceCollections &programCollection) const
{
    const auto useTaskShader  = m_params->needsTaskShader();
    const auto taskMultiplier = (useTaskShader ? m_params->taskCount.get() : 1u);

    // Add the frag shader.
    MeshShaderMiscCase::initPrograms(programCollection);

    std::ostringstream taskData;
    taskData << "taskNV TaskData {\n"
             << "    uint parentTask[" << kLocalInvocations << "];\n"
             << "} td;\n";
    const auto taskDataStr = taskData.str();

    if (useTaskShader)
    {
        std::ostringstream task;
        task << "#version 450\n"
             << "#extension GL_NV_mesh_shader : enable\n"
             << "\n"
             << "layout (local_size_x=" << kLocalInvocations << ") in;\n"
             << "\n"
             << "out " << taskDataStr << "\n"
             << "void main () {\n"
             << "    gl_TaskCountNV = " << m_params->meshCount << ";\n"
             << "    td.parentTask[gl_LocalInvocationID.x] = gl_WorkGroupID.x;\n"
             << "}\n";
        programCollection.glslSources.add("task") << glu::TaskSource(task.str());
    }

    // Needed for the code below to work.
    DE_ASSERT(m_params->width * m_params->height == taskMultiplier * m_params->meshCount * kLocalInvocations);
    DE_UNREF(taskMultiplier); // For release builds.

    // Emit one point per framebuffer pixel. The number of jobs (kLocalInvocations in each mesh shader work group, multiplied by the
    // number of mesh work groups emitted by each task work group) must be the same as the total framebuffer size. Calculate a job
    // ID corresponding to the current mesh shader invocation, and assign a pixel position to it. Draw a point at that position.
    std::ostringstream mesh;
    mesh << "#version 450\n"
         << "#extension GL_NV_mesh_shader : enable\n"
         << "\n"
         << "layout (local_size_x=" << kLocalInvocations << ") in;\n"
         << "layout (points) out;\n"
         << "layout (max_vertices=" << kLocalInvocations << ", max_primitives=" << kLocalInvocations << ") out;\n"
         << "\n"
         << (useTaskShader ? "in " + taskDataStr : "") << "\n"
         << "layout (location=0) out perprimitiveNV vec4 pointColor[];\n"
         << "\n"
         << "void main () {\n";

    if (useTaskShader)
    {
        mesh << "    uint parentTask = td.parentTask[0];\n"
             << "    if (td.parentTask[gl_LocalInvocationID.x] != parentTask) {\n"
             << "        return;\n"
             << "    }\n";
    }
    else
    {
        mesh << "    uint parentTask = 0;\n";
    }

    mesh << "    gl_PrimitiveCountNV = " << kLocalInvocations << ";\n"
         << "    uint jobId = ((parentTask * " << m_params->meshCount << ") + gl_WorkGroupID.x) * " << kLocalInvocations
         << " + gl_LocalInvocationID.x;\n"
         << "    uint row = jobId / " << m_params->width << ";\n"
         << "    uint col = jobId % " << m_params->width << ";\n"
         << "    float yCoord = (float(row + 0.5) / " << m_params->height << ".0) * 2.0 - 1.0;\n"
         << "    float xCoord = (float(col + 0.5) / " << m_params->width << ".0) * 2.0 - 1.0;\n"
         << "    gl_MeshVerticesNV[gl_LocalInvocationID.x].gl_Position = vec4(xCoord, yCoord, 0.0, 1.0);\n"
         << "    gl_MeshVerticesNV[gl_LocalInvocationID.x].gl_PointSize = 1.0;\n"
         << "    gl_PrimitiveIndicesNV[gl_LocalInvocationID.x] = gl_LocalInvocationID.x;\n"
         << "    pointColor[gl_LocalInvocationID.x] = vec4(0.0, 0.0, 1.0, 1.0);\n"
         << "}\n";
    programCollection.glslSources.add("mesh") << glu::MeshSource(mesh.str());
}

// Tests that generate no primitives of a given type.
enum class PrimitiveType
{
    POINTS = 0,
    LINES,
    TRIANGLES
};

std::string primitiveTypeName(PrimitiveType primitiveType)
{
    std::string primitiveName;

    switch (primitiveType)
    {
    case PrimitiveType::POINTS:
        primitiveName = "points";
        break;
    case PrimitiveType::LINES:
        primitiveName = "lines";
        break;
    case PrimitiveType::TRIANGLES:
        primitiveName = "triangles";
        break;
    default:
        DE_ASSERT(false);
        break;
    }

    return primitiveName;
}

struct NoPrimitivesParams : public MiscTestParams
{
    PrimitiveType primitiveType;
};

class NoPrimitivesCase : public MeshShaderMiscCase
{
public:
    NoPrimitivesCase(tcu::TestContext &testCtx, const std::string &name, const std::string &description,
                     ParamsPtr params)
        : MeshShaderMiscCase(testCtx, name, description, std::move(params))
    {
    }

    void initPrograms(vk::SourceCollections &programCollection) const override;
    TestInstance *createInstance(Context &context) const override;
};

class NoPrimitivesInstance : public MeshShaderMiscInstance
{
public:
    NoPrimitivesInstance(Context &context, const MiscTestParams *params) : MeshShaderMiscInstance(context, params)
    {
    }

    void generateReferenceLevel() override;
};

void NoPrimitivesInstance::generateReferenceLevel()
{
    // No primitives: clear color.
    generateSolidRefLevel(tcu::Vec4(0.0f, 0.0f, 0.0f, 0.0f), m_referenceLevel);
}

TestInstance *NoPrimitivesCase::createInstance(Context &context) const
{
    return new NoPrimitivesInstance(context, m_params.get());
}

void NoPrimitivesCase::initPrograms(vk::SourceCollections &programCollection) const
{
    const auto params = dynamic_cast<NoPrimitivesParams *>(m_params.get());

    DE_ASSERT(params);
    DE_ASSERT(!params->needsTaskShader());

    const auto primitiveName = primitiveTypeName(params->primitiveType);

    std::ostringstream mesh;
    mesh << "#version 450\n"
         << "#extension GL_NV_mesh_shader : enable\n"
         << "\n"
         << "layout (local_size_x=32) in;\n"
         << "layout (" << primitiveName << ") out;\n"
         << "layout (max_vertices=256, max_primitives=256) out;\n"
         << "\n"
         << "layout (location=0) out perprimitiveNV vec4 primitiveColor[];\n"
         << "\n"
         << "void main () {\n"
         << "    gl_PrimitiveCountNV = 0u;\n"
         << "}\n";

    MeshShaderMiscCase::initPrograms(programCollection);
    programCollection.glslSources.add("mesh") << glu::MeshSource(mesh.str());
}

class NoPrimitivesExtraWritesCase : public NoPrimitivesCase
{
public:
    NoPrimitivesExtraWritesCase(tcu::TestContext &testCtx, const std::string &name, const std::string &description,
                                ParamsPtr params)
        : NoPrimitivesCase(testCtx, name, description, std::move(params))
    {
    }

    void initPrograms(vk::SourceCollections &programCollection) const override;

    static constexpr uint32_t kLocalInvocations = 32u;
};

void NoPrimitivesExtraWritesCase::initPrograms(vk::SourceCollections &programCollection) const
{
    const auto params = dynamic_cast<NoPrimitivesParams *>(m_params.get());

    DE_ASSERT(params);
    DE_ASSERT(m_params->needsTaskShader());

    std::ostringstream taskData;
    taskData << "taskNV TaskData {\n"
             << "    uint localInvocations[" << kLocalInvocations << "];\n"
             << "} td;\n";
    const auto taskDataStr = taskData.str();

    std::ostringstream task;
    task << "#version 450\n"
         << "#extension GL_NV_mesh_shader : enable\n"
         << "\n"
         << "layout (local_size_x=" << kLocalInvocations << ") in;\n"
         << "\n"
         << "out " << taskDataStr << "\n"
         << "void main () {\n"
         << "    gl_TaskCountNV = " << params->meshCount << ";\n"
         << "    td.localInvocations[gl_LocalInvocationID.x] = gl_LocalInvocationID.x;\n"
         << "}\n";
    programCollection.glslSources.add("task") << glu::TaskSource(task.str());

    const auto primitiveName = primitiveTypeName(params->primitiveType);

    // Otherwise the shader would be illegal.
    DE_ASSERT(kLocalInvocations > 2u);

    uint32_t maxPrimitives = 0u;
    switch (params->primitiveType)
    {
    case PrimitiveType::POINTS:
        maxPrimitives = kLocalInvocations - 0u;
        break;
    case PrimitiveType::LINES:
        maxPrimitives = kLocalInvocations - 1u;
        break;
    case PrimitiveType::TRIANGLES:
        maxPrimitives = kLocalInvocations - 2u;
        break;
    default:
        DE_ASSERT(false);
        break;
    }

    const std::string pointSizeDecl = ((params->primitiveType == PrimitiveType::POINTS) ?
                                           "        gl_MeshVerticesNV[gl_LocalInvocationID.x].gl_PointSize = 1.0;\n" :
                                           "");

    std::ostringstream mesh;
    mesh << "#version 450\n"
         << "#extension GL_NV_mesh_shader : enable\n"
         << "\n"
         << "layout (local_size_x=" << kLocalInvocations << ") in;\n"
         << "layout (" << primitiveName << ") out;\n"
         << "layout (max_vertices=" << kLocalInvocations << ", max_primitives=" << maxPrimitives << ") out;\n"
         << "\n"
         << "in " << taskDataStr << "\n"
         << "layout (location=0) out perprimitiveNV vec4 primitiveColor[];\n"
         << "\n"
         << "shared uint sumOfIds;\n"
         << "\n"
         << "const float PI_2 = 1.57079632679489661923;\n"
         << "const float RADIUS = 1.0f;\n"
         << "\n"
         << "void main ()\n"
         << "{\n"
         << "    sumOfIds = 0u;\n"
         << "    barrier();\n"
         << "    atomicAdd(sumOfIds, td.localInvocations[gl_LocalInvocationID.x]);\n"
         << "    barrier();\n"
         << "    // This should dynamically give 0\n"
         << "    gl_PrimitiveCountNV = sumOfIds - (" << kLocalInvocations * (kLocalInvocations - 1u) / 2u << ");\n"
         << "\n"
         << "    // Emit points and primitives to the arrays in any case\n"
         << "    if (gl_LocalInvocationID.x > 0u) {\n"
         << "        float proportion = (float(gl_LocalInvocationID.x - 1u) + 0.5f) / float(" << kLocalInvocations
         << " - 1u);\n"
         << "        float angle = PI_2 * proportion;\n"
         << "        float xCoord = cos(angle) * RADIUS - 1.0;\n"
         << "        float yCoord = sin(angle) * RADIUS - 1.0;\n"
         << "        gl_MeshVerticesNV[gl_LocalInvocationID.x].gl_Position = vec4(xCoord, yCoord, 0.0, 1.0);\n"
         << pointSizeDecl << "    } else {\n"
         << "        gl_MeshVerticesNV[gl_LocalInvocationID.x].gl_Position = vec4(0.0, 0.0, 0.0, 1.0);\n"
         << pointSizeDecl << "    }\n"
         << "    uint primitiveId = max(gl_LocalInvocationID.x, " << (maxPrimitives - 1u) << ");\n"
         << "    primitiveColor[primitiveId] = vec4(0.0, 0.0, 1.0, 1.0);\n";

    if (params->primitiveType == PrimitiveType::POINTS)
    {
        mesh << "    gl_PrimitiveIndicesNV[primitiveId] = primitiveId;\n";
    }
    else if (params->primitiveType == PrimitiveType::LINES)
    {
        mesh << "    gl_PrimitiveIndicesNV[primitiveId * 2u + 0u] = primitiveId + 0u;\n"
             << "    gl_PrimitiveIndicesNV[primitiveId * 2u + 1u] = primitiveId + 1u;\n";
    }
    else if (params->primitiveType == PrimitiveType::TRIANGLES)
    {
        mesh << "    gl_PrimitiveIndicesNV[primitiveId * 3u + 0u] = 0u;\n"
             << "    gl_PrimitiveIndicesNV[primitiveId * 3u + 1u] = primitiveId + 1u;\n"
             << "    gl_PrimitiveIndicesNV[primitiveId * 3u + 2u] = primitiveId + 3u;\n";
    }
    else
        DE_ASSERT(false);

    mesh << "}\n";

    programCollection.glslSources.add("mesh") << glu::MeshSource(mesh.str());

    MeshShaderMiscCase::initPrograms(programCollection);
}

// Case testing barrier().
class SimpleBarrierCase : public MeshShaderMiscCase
{
public:
    SimpleBarrierCase(tcu::TestContext &testCtx, const std::string &name, const std::string &description,
                      ParamsPtr params)
        : MeshShaderMiscCase(testCtx, name, description, std::move(params))
    {
    }

    void initPrograms(vk::SourceCollections &programCollection) const override;
    TestInstance *createInstance(Context &context) const override;

    static constexpr uint32_t kLocalInvocations = 32u;
};

class SimpleBarrierInstance : public MeshShaderMiscInstance
{
public:
    SimpleBarrierInstance(Context &context, const MiscTestParams *params) : MeshShaderMiscInstance(context, params)
    {
    }

    void generateReferenceLevel() override;
};

TestInstance *SimpleBarrierCase::createInstance(Context &context) const
{
    return new SimpleBarrierInstance(context, m_params.get());
}

void SimpleBarrierInstance::generateReferenceLevel()
{
    generateSolidRefLevel(tcu::Vec4(0.0f, 0.0f, 1.0f, 1.0f), m_referenceLevel);
}

void SimpleBarrierCase::initPrograms(vk::SourceCollections &programCollection) const
{
    // Generate frag shader.
    MeshShaderMiscCase::initPrograms(programCollection);

    DE_ASSERT(m_params->meshCount == 1u);
    DE_ASSERT(m_params->width == 1u && m_params->height == 1u);

    std::ostringstream meshPrimData;
    meshPrimData << "gl_PrimitiveCountNV = 1u;\n"
                 << "gl_MeshVerticesNV[0].gl_Position = vec4(0.0, 0.0, 0.0, 1.0);\n"
                 << "gl_MeshVerticesNV[0].gl_PointSize = 1.0;\n"
                 << "primitiveColor[0] = vec4(0.0, 0.0, 1.0, 1.0);\n"
                 << "gl_PrimitiveIndicesNV[0] = 0;\n";
    const std::string meshPrimStr = meshPrimData.str();

    const std::string taskOK   = "gl_TaskCountNV = 1u;\n";
    const std::string taskFAIL = "gl_TaskCountNV = 0u;\n";

    const std::string meshOK   = meshPrimStr;
    const std::string meshFAIL = "gl_PrimitiveCountNV = 0u;\n";

    const std::string okStatement   = (m_params->needsTaskShader() ? taskOK : meshOK);
    const std::string failStatement = (m_params->needsTaskShader() ? taskFAIL : meshFAIL);

    const std::string sharedDecl = "shared uint counter;\n\n";
    std::ostringstream verification;
    verification << "counter = 0;\n"
                 << "barrier();\n"
                 << "atomicAdd(counter, 1u);\n"
                 << "barrier();\n"
                 << "if (gl_LocalInvocationID.x == 0u) {\n"
                 << "    if (counter == " << kLocalInvocations << ") {\n"
                 << "\n"
                 << okStatement << "\n"
                 << "    } else {\n"
                 << "\n"
                 << failStatement << "\n"
                 << "    }\n"
                 << "}\n";

    // The mesh shader is very similar in both cases, so we use a template.
    std::ostringstream meshTemplateStr;
    meshTemplateStr << "#version 450\n"
                    << "#extension GL_NV_mesh_shader : enable\n"
                    << "\n"
                    << "layout (local_size_x=${LOCAL_SIZE}) in;\n"
                    << "layout (points) out;\n"
                    << "layout (max_vertices=1, max_primitives=1) out;\n"
                    << "\n"
                    << "layout (location=0) out perprimitiveNV vec4 primitiveColor[];\n"
                    << "\n"
                    << "${GLOBALS:opt}"
                    << "void main ()\n"
                    << "{\n"
                    << "${BODY}"
                    << "}\n";
    const tcu::StringTemplate meshTemplate = meshTemplateStr.str();

    if (m_params->needsTaskShader())
    {
        std::ostringstream task;
        task << "#version 450\n"
             << "#extension GL_NV_mesh_shader : enable\n"
             << "\n"
             << "layout (local_size_x=" << kLocalInvocations << ") in;\n"
             << "\n"
             << sharedDecl << "void main ()\n"
             << "{\n"
             << verification.str() << "}\n";

        std::map<std::string, std::string> replacements;
        replacements["LOCAL_SIZE"] = "1";
        replacements["BODY"]       = meshPrimStr;

        const auto meshStr = meshTemplate.specialize(replacements);

        programCollection.glslSources.add("task") << glu::TaskSource(task.str());
        programCollection.glslSources.add("mesh") << glu::MeshSource(meshStr);
    }
    else
    {
        std::map<std::string, std::string> replacements;
        replacements["LOCAL_SIZE"] = std::to_string(kLocalInvocations);
        replacements["BODY"]       = verification.str();
        replacements["GLOBALS"]    = sharedDecl;

        const auto meshStr = meshTemplate.specialize(replacements);

        programCollection.glslSources.add("mesh") << glu::MeshSource(meshStr);
    }
}

// Case testing memoryBarrierShared() and groupMemoryBarrier().
enum class MemoryBarrierType
{
    SHARED = 0,
    GROUP
};

struct MemoryBarrierParams : public MiscTestParams
{
    MemoryBarrierType memBarrierType;

    std::string glslFunc() const
    {
        std::string funcName;

        switch (memBarrierType)
        {
        case MemoryBarrierType::SHARED:
            funcName = "memoryBarrierShared";
            break;
        case MemoryBarrierType::GROUP:
            funcName = "groupMemoryBarrier";
            break;
        default:
            DE_ASSERT(false);
            break;
        }

        return funcName;
    }
};

class MemoryBarrierCase : public MeshShaderMiscCase
{
public:
    MemoryBarrierCase(tcu::TestContext &testCtx, const std::string &name, const std::string &description,
                      ParamsPtr params)
        : MeshShaderMiscCase(testCtx, name, description, std::move(params))
    {
    }

    void initPrograms(vk::SourceCollections &programCollection) const override;
    TestInstance *createInstance(Context &context) const override;

    static constexpr uint32_t kLocalInvocations = 2u;
};

class MemoryBarrierInstance : public MeshShaderMiscInstance
{
public:
    MemoryBarrierInstance(Context &context, const MiscTestParams *params) : MeshShaderMiscInstance(context, params)
    {
    }

    void generateReferenceLevel() override;
    bool verifyResult(const tcu::ConstPixelBufferAccess &resultAccess) const override;

protected:
    // Allow two possible outcomes.
    std::unique_ptr<tcu::TextureLevel> m_referenceLevel2;
};

TestInstance *MemoryBarrierCase::createInstance(Context &context) const
{
    return new MemoryBarrierInstance(context, m_params.get());
}

void MemoryBarrierInstance::generateReferenceLevel()
{
    generateSolidRefLevel(tcu::Vec4(0.0f, 0.0f, 1.0f, 1.0f), m_referenceLevel);
    generateSolidRefLevel(tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f), m_referenceLevel2);
}

bool MemoryBarrierInstance::verifyResult(const tcu::ConstPixelBufferAccess &resultAccess) const
{
    // Any of the two results is considered valid.
    // Clarify what we are checking in the logs; otherwise, they could be confusing.
    auto &log                                     = m_context.getTestContext().getLog();
    const std::vector<tcu::TextureLevel *> levels = {m_referenceLevel.get(), m_referenceLevel2.get()};

    bool good = false;
    for (size_t i = 0; i < levels.size(); ++i)
    {
        log << tcu::TestLog::Message << "Comparing result with reference " << i << "..." << tcu::TestLog::EndMessage;
        const auto success = MeshShaderMiscInstance::verifyResult(resultAccess, *levels[i]);
        if (success)
        {
            log << tcu::TestLog::Message << "Match! The test has passed" << tcu::TestLog::EndMessage;
            good = true;
            break;
        }
    }

    return good;
}

void MemoryBarrierCase::initPrograms(vk::SourceCollections &programCollection) const
{
    const auto params = dynamic_cast<MemoryBarrierParams *>(m_params.get());
    DE_ASSERT(params);

    // Generate frag shader.
    MeshShaderMiscCase::initPrograms(programCollection);

    DE_ASSERT(params->meshCount == 1u);
    DE_ASSERT(params->width == 1u && params->height == 1u);

    const bool taskShader = params->needsTaskShader();

    const std::string taskDataDecl = "taskNV TaskData { float blue; } td;\n\n";
    const std::string inTaskData   = "in " + taskDataDecl;
    const std::string outTaskData  = "out " + taskDataDecl;
    const auto barrierFunc         = params->glslFunc();

    std::ostringstream meshPrimData;
    meshPrimData << "gl_PrimitiveCountNV = 1u;\n"
                 << "gl_MeshVerticesNV[0].gl_Position = vec4(0.0, 0.0, 0.0, 1.0);\n"
                 << "gl_MeshVerticesNV[0].gl_PointSize = 1.0;\n"
                 << "primitiveColor[0] = vec4(0.0, 0.0, " << (taskShader ? "td.blue" : "float(iterations % 2u)")
                 << ", 1.0);\n"
                 << "gl_PrimitiveIndicesNV[0] = 0;\n";
    const std::string meshPrimStr = meshPrimData.str();

    const std::string taskAction = "gl_TaskCountNV = 1u;\ntd.blue = float(iterations % 2u);\n";
    const std::string meshAction = meshPrimStr;
    const std::string action     = (taskShader ? taskAction : meshAction);

    const std::string sharedDecl = "shared uint flags[2];\n\n";
    std::ostringstream verification;
    verification << "flags[gl_LocalInvocationID.x] = 0u;\n"
                 << "barrier();\n"
                 << "flags[gl_LocalInvocationID.x] = 1u;\n"
                 << barrierFunc << "();\n"
                 << "uint otherInvocation = 1u - gl_LocalInvocationID.x;\n"
                 << "uint iterations = 0u;\n"
                 << "while (flags[otherInvocation] != 1u) {\n"
                 << "    iterations++;\n"
                 << "}\n"
                 << "if (gl_LocalInvocationID.x == 0u) {\n"
                 << "\n"
                 << action << "\n"
                 << "}\n";

    // The mesh shader is very similar in both cases, so we use a template.
    std::ostringstream meshTemplateStr;
    meshTemplateStr << "#version 450\n"
                    << "#extension GL_NV_mesh_shader : enable\n"
                    << "\n"
                    << "layout (local_size_x=${LOCAL_SIZE}) in;\n"
                    << "layout (points) out;\n"
                    << "layout (max_vertices=1, max_primitives=1) out;\n"
                    << "\n"
                    << "layout (location=0) out perprimitiveNV vec4 primitiveColor[];\n"
                    << "\n"
                    << "${GLOBALS}"
                    << "void main ()\n"
                    << "{\n"
                    << "${BODY}"
                    << "}\n";
    const tcu::StringTemplate meshTemplate = meshTemplateStr.str();

    if (params->needsTaskShader())
    {
        std::ostringstream task;
        task << "#version 450\n"
             << "#extension GL_NV_mesh_shader : enable\n"
             << "\n"
             << "layout (local_size_x=" << kLocalInvocations << ") in;\n"
             << "\n"
             << sharedDecl << outTaskData << "void main ()\n"
             << "{\n"
             << verification.str() << "}\n";

        std::map<std::string, std::string> replacements;
        replacements["LOCAL_SIZE"] = "1";
        replacements["BODY"]       = meshPrimStr;
        replacements["GLOBALS"]    = inTaskData;

        const auto meshStr = meshTemplate.specialize(replacements);

        programCollection.glslSources.add("task") << glu::TaskSource(task.str());
        programCollection.glslSources.add("mesh") << glu::MeshSource(meshStr);
    }
    else
    {
        std::map<std::string, std::string> replacements;
        replacements["LOCAL_SIZE"] = std::to_string(kLocalInvocations);
        replacements["BODY"]       = verification.str();
        replacements["GLOBALS"]    = sharedDecl;

        const auto meshStr = meshTemplate.specialize(replacements);

        programCollection.glslSources.add("mesh") << glu::MeshSource(meshStr);
    }
}

class CustomAttributesCase : public MeshShaderMiscCase
{
public:
    CustomAttributesCase(tcu::TestContext &testCtx, const std::string &name, const std::string &description,
                         ParamsPtr params)
        : MeshShaderMiscCase(testCtx, name, description, std::move(params))
    {
    }
    virtual ~CustomAttributesCase(void)
    {
    }

    TestInstance *createInstance(Context &context) const override;
    void checkSupport(Context &context) const override;
    void initPrograms(vk::SourceCollections &programCollection) const override;
};

class CustomAttributesInstance : public MeshShaderMiscInstance
{
public:
    CustomAttributesInstance(Context &context, const MiscTestParams *params) : MeshShaderMiscInstance(context, params)
    {
    }
    virtual ~CustomAttributesInstance(void)
    {
    }

    void generateReferenceLevel() override;
    tcu::TestStatus iterate(void) override;
};

TestInstance *CustomAttributesCase::createInstance(Context &context) const
{
    return new CustomAttributesInstance(context, m_params.get());
}

void CustomAttributesCase::checkSupport(Context &context) const
{
    MeshShaderMiscCase::checkSupport(context);

    context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_MULTI_VIEWPORT);
    context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_SHADER_CLIP_DISTANCE);
}

void CustomAttributesCase::initPrograms(vk::SourceCollections &programCollection) const
{
    std::ostringstream frag;
    frag << "#version 450\n"
         << "#extension GL_NV_mesh_shader : enable\n"
         << "\n"
         << "layout (location=0) in vec4 customAttribute1;\n"
         << "layout (location=1) in flat float customAttribute2;\n"
         << "layout (location=2) in flat int customAttribute3;\n"
         << "\n"
         << "layout (location=3) in perprimitiveNV flat uvec4 customAttribute4;\n"
         << "layout (location=4) in perprimitiveNV float customAttribute5;\n"
         << "\n"
         << "layout (location=0) out vec4 outColor;\n"
         << "\n"
         << "void main ()\n"
         << "{\n"
         << "    bool goodPrimitiveID = (gl_PrimitiveID == 1000 || gl_PrimitiveID == 1001);\n"
         << "    bool goodViewportIndex = (gl_ViewportIndex == 1);\n"
         << "    bool goodCustom1 = (customAttribute1.x >= 0.25 && customAttribute1.x <= 0.5 &&\n"
         << "                        customAttribute1.y >= 0.5  && customAttribute1.y <= 1.0 &&\n"
         << "                        customAttribute1.z >= 10.0 && customAttribute1.z <= 20.0 &&\n"
         << "                        customAttribute1.w == 3.0);\n"
         << "    bool goodCustom2 = (customAttribute2 == 1.0 || customAttribute2 == 2.0);\n"
         << "    bool goodCustom3 = (customAttribute3 == 3 || customAttribute3 == 4);\n"
         << "    bool goodCustom4 = ((gl_PrimitiveID == 1000 && customAttribute4 == uvec4(100, 101, 102, 103)) ||\n"
         << "                        (gl_PrimitiveID == 1001 && customAttribute4 == uvec4(200, 201, 202, 203)));\n"
         << "    bool goodCustom5 = ((gl_PrimitiveID == 1000 && customAttribute5 == 6.0) ||\n"
         << "                        (gl_PrimitiveID == 1001 && customAttribute5 == 7.0));\n"
         << "    \n"
         << "    if (goodPrimitiveID && goodViewportIndex && goodCustom1 && goodCustom2 && goodCustom3 && goodCustom4 "
            "&& goodCustom5) {\n"
         << "        outColor = vec4(0.0, 0.0, 1.0, 1.0);\n"
         << "    } else {\n"
         << "        outColor = vec4(0.0, 0.0, 0.0, 1.0);\n"
         << "    }\n"
         << "}\n";
    programCollection.glslSources.add("frag") << glu::FragmentSource(frag.str());

    std::ostringstream pvdDataDeclStream;
    pvdDataDeclStream << "    vec4 positions[4];\n"
                      << "    float pointSizes[4];\n"
                      << "    float clipDistances[4];\n"
                      << "    vec4 custom1[4];\n"
                      << "    float custom2[4];\n"
                      << "    int custom3[4];\n";
    const auto pvdDataDecl = pvdDataDeclStream.str();

    std::ostringstream ppdDataDeclStream;
    ppdDataDeclStream << "    int primitiveIds[2];\n"
                      << "    int viewportIndices[2];\n"
                      << "    uvec4 custom4[2];\n"
                      << "    float custom5[2];\n";
    const auto ppdDataDecl = ppdDataDeclStream.str();

    std::ostringstream bindingsDeclStream;
    bindingsDeclStream << "layout (set=0, binding=0, std430) buffer PerVertexData {\n"
                       << pvdDataDecl << "} pvd;\n"
                       << "layout (set=0, binding=1) uniform PerPrimitiveData {\n"
                       << ppdDataDecl << "} ppd;\n"
                       << "\n";
    const auto bindingsDecl = bindingsDeclStream.str();

    std::ostringstream taskDataStream;
    taskDataStream << "taskNV TaskData {\n"
                   << pvdDataDecl << ppdDataDecl << "} td;\n"
                   << "\n";
    const auto taskDataDecl = taskDataStream.str();

    const auto taskShader = m_params->needsTaskShader();

    const auto meshPvdPrefix = (taskShader ? "td" : "pvd");
    const auto meshPpdPrefix = (taskShader ? "td" : "ppd");

    std::ostringstream mesh;
    mesh << "#version 450\n"
         << "#extension GL_NV_mesh_shader : enable\n"
         << "\n"
         << "layout (local_size_x=1) in;\n"
         << "layout (max_primitives=2, max_vertices=4) out;\n"
         << "layout (triangles) out;\n"
         << "\n"
         << "out gl_MeshPerVertexNV {\n"
         << "    vec4  gl_Position;\n"
         << "    float gl_PointSize;\n"
         << "    float gl_ClipDistance[1];\n"
         << "} gl_MeshVerticesNV[];\n"
         << "\n"
         << "layout (location=0) out vec4 customAttribute1[];\n"
         << "layout (location=1) out flat float customAttribute2[];\n"
         << "layout (location=2) out int customAttribute3[];\n"
         << "\n"
         << "layout (location=3) out perprimitiveNV uvec4 customAttribute4[];\n"
         << "layout (location=4) out perprimitiveNV float customAttribute5[];\n"
         << "\n"
         << "out perprimitiveNV gl_MeshPerPrimitiveNV {\n"
         << "  int gl_PrimitiveID;\n"
         << "  int gl_ViewportIndex;\n"
         << "} gl_MeshPrimitivesNV[];\n"
         << "\n"
         << (taskShader ? "in " + taskDataDecl : bindingsDecl) << "void main ()\n"
         << "{\n"
         << "    gl_PrimitiveCountNV = 2u;\n"
         << "\n"
         << "    gl_MeshVerticesNV[0].gl_Position = " << meshPvdPrefix
         << ".positions[0]; //vec4(-1.0, -1.0, 0.0, 1.0)\n"
         << "    gl_MeshVerticesNV[1].gl_Position = " << meshPvdPrefix
         << ".positions[1]; //vec4( 1.0, -1.0, 0.0, 1.0)\n"
         << "    gl_MeshVerticesNV[2].gl_Position = " << meshPvdPrefix
         << ".positions[2]; //vec4(-1.0,  1.0, 0.0, 1.0)\n"
         << "    gl_MeshVerticesNV[3].gl_Position = " << meshPvdPrefix
         << ".positions[3]; //vec4( 1.0,  1.0, 0.0, 1.0)\n"
         << "\n"
         << "    gl_MeshVerticesNV[0].gl_PointSize = " << meshPvdPrefix << ".pointSizes[0]; //1.0\n"
         << "    gl_MeshVerticesNV[1].gl_PointSize = " << meshPvdPrefix << ".pointSizes[1]; //1.0\n"
         << "    gl_MeshVerticesNV[2].gl_PointSize = " << meshPvdPrefix << ".pointSizes[2]; //1.0\n"
         << "    gl_MeshVerticesNV[3].gl_PointSize = " << meshPvdPrefix << ".pointSizes[3]; //1.0\n"
         << "\n"
         << "    // Remove geometry on the right side.\n"
         << "    gl_MeshVerticesNV[0].gl_ClipDistance[0] = " << meshPvdPrefix << ".clipDistances[0]; // 1.0\n"
         << "    gl_MeshVerticesNV[1].gl_ClipDistance[0] = " << meshPvdPrefix << ".clipDistances[1]; //-1.0\n"
         << "    gl_MeshVerticesNV[2].gl_ClipDistance[0] = " << meshPvdPrefix << ".clipDistances[2]; // 1.0\n"
         << "    gl_MeshVerticesNV[3].gl_ClipDistance[0] = " << meshPvdPrefix << ".clipDistances[3]; //-1.0\n"
         << "    \n"
         << "    gl_PrimitiveIndicesNV[0] = 0;\n"
         << "    gl_PrimitiveIndicesNV[1] = 2;\n"
         << "    gl_PrimitiveIndicesNV[2] = 1;\n"
         << "\n"
         << "    gl_PrimitiveIndicesNV[3] = 2;\n"
         << "    gl_PrimitiveIndicesNV[4] = 3;\n"
         << "    gl_PrimitiveIndicesNV[5] = 1;\n"
         << "\n"
         << "    gl_MeshPrimitivesNV[0].gl_PrimitiveID = " << meshPpdPrefix << ".primitiveIds[0]; //1000\n"
         << "    gl_MeshPrimitivesNV[1].gl_PrimitiveID = " << meshPpdPrefix << ".primitiveIds[1]; //1001\n"
         << "\n"
         << "    gl_MeshPrimitivesNV[0].gl_ViewportIndex = " << meshPpdPrefix << ".viewportIndices[0]; //1\n"
         << "    gl_MeshPrimitivesNV[1].gl_ViewportIndex = " << meshPpdPrefix << ".viewportIndices[1]; //1\n"
         << "\n"
         << "    // Custom per-vertex attributes\n"
         << "    customAttribute1[0] = " << meshPvdPrefix << ".custom1[0]; //vec4(0.25, 0.5, 10.0, 3.0)\n"
         << "    customAttribute1[1] = " << meshPvdPrefix << ".custom1[1]; //vec4(0.25, 1.0, 20.0, 3.0)\n"
         << "    customAttribute1[2] = " << meshPvdPrefix << ".custom1[2]; //vec4( 0.5, 0.5, 20.0, 3.0)\n"
         << "    customAttribute1[3] = " << meshPvdPrefix << ".custom1[3]; //vec4( 0.5, 1.0, 10.0, 3.0)\n"
         << "\n"
         << "    customAttribute2[0] = " << meshPvdPrefix << ".custom2[0]; //1.0f\n"
         << "    customAttribute2[1] = " << meshPvdPrefix << ".custom2[1]; //1.0f\n"
         << "    customAttribute2[2] = " << meshPvdPrefix << ".custom2[2]; //2.0f\n"
         << "    customAttribute2[3] = " << meshPvdPrefix << ".custom2[3]; //2.0f\n"
         << "\n"
         << "    customAttribute3[0] = " << meshPvdPrefix << ".custom3[0]; //3\n"
         << "    customAttribute3[1] = " << meshPvdPrefix << ".custom3[1]; //3\n"
         << "    customAttribute3[2] = " << meshPvdPrefix << ".custom3[2]; //4\n"
         << "    customAttribute3[3] = " << meshPvdPrefix << ".custom3[3]; //4\n"
         << "\n"
         << "    // Custom per-primitive attributes.\n"
         << "    customAttribute4[0] = " << meshPpdPrefix << ".custom4[0]; //uvec4(100, 101, 102, 103)\n"
         << "    customAttribute4[1] = " << meshPpdPrefix << ".custom4[1]; //uvec4(200, 201, 202, 203)\n"
         << "\n"
         << "    customAttribute5[0] = " << meshPpdPrefix << ".custom5[0]; //6.0\n"
         << "    customAttribute5[1] = " << meshPpdPrefix << ".custom5[1]; //7.0\n"
         << "}\n";
    programCollection.glslSources.add("mesh") << glu::MeshSource(mesh.str());

    if (taskShader)
    {
        std::ostringstream task;
        task << "#version 450\n"
             << "#extension GL_NV_mesh_shader : enable\n"
             << "\n"
             << "out " << taskDataDecl << bindingsDecl << "void main ()\n"
             << "{\n"
             << "    gl_TaskCountNV = " << m_params->meshCount << ";\n"
             << "\n"
             << "    td.positions[0] = pvd.positions[0];\n"
             << "    td.positions[1] = pvd.positions[1];\n"
             << "    td.positions[2] = pvd.positions[2];\n"
             << "    td.positions[3] = pvd.positions[3];\n"
             << "\n"
             << "    td.pointSizes[0] = pvd.pointSizes[0];\n"
             << "    td.pointSizes[1] = pvd.pointSizes[1];\n"
             << "    td.pointSizes[2] = pvd.pointSizes[2];\n"
             << "    td.pointSizes[3] = pvd.pointSizes[3];\n"
             << "\n"
             << "    td.clipDistances[0] = pvd.clipDistances[0];\n"
             << "    td.clipDistances[1] = pvd.clipDistances[1];\n"
             << "    td.clipDistances[2] = pvd.clipDistances[2];\n"
             << "    td.clipDistances[3] = pvd.clipDistances[3];\n"
             << "\n"
             << "    td.custom1[0] = pvd.custom1[0];\n"
             << "    td.custom1[1] = pvd.custom1[1];\n"
             << "    td.custom1[2] = pvd.custom1[2];\n"
             << "    td.custom1[3] = pvd.custom1[3];\n"
             << "\n"
             << "    td.custom2[0] = pvd.custom2[0];\n"
             << "    td.custom2[1] = pvd.custom2[1];\n"
             << "    td.custom2[2] = pvd.custom2[2];\n"
             << "    td.custom2[3] = pvd.custom2[3];\n"
             << "\n"
             << "    td.custom3[0] = pvd.custom3[0];\n"
             << "    td.custom3[1] = pvd.custom3[1];\n"
             << "    td.custom3[2] = pvd.custom3[2];\n"
             << "    td.custom3[3] = pvd.custom3[3];\n"
             << "\n"
             << "    td.primitiveIds[0] = ppd.primitiveIds[0];\n"
             << "    td.primitiveIds[1] = ppd.primitiveIds[1];\n"
             << "\n"
             << "    td.viewportIndices[0] = ppd.viewportIndices[0];\n"
             << "    td.viewportIndices[1] = ppd.viewportIndices[1];\n"
             << "\n"
             << "    td.custom4[0] = ppd.custom4[0];\n"
             << "    td.custom4[1] = ppd.custom4[1];\n"
             << "\n"
             << "    td.custom5[0] = ppd.custom5[0];\n"
             << "    td.custom5[1] = ppd.custom5[1];\n"
             << "}\n";
        programCollection.glslSources.add("task") << glu::TaskSource(task.str());
    }
}

void CustomAttributesInstance::generateReferenceLevel()
{
    const auto format    = getOutputFormat();
    const auto tcuFormat = mapVkFormat(format);

    const auto iWidth  = static_cast<int>(m_params->width);
    const auto iHeight = static_cast<int>(m_params->height);

    const auto halfWidth  = iWidth / 2;
    const auto halfHeight = iHeight / 2;

    m_referenceLevel.reset(new tcu::TextureLevel(tcuFormat, iWidth, iHeight));

    const auto access     = m_referenceLevel->getAccess();
    const auto clearColor = tcu::Vec4(0.0f, 0.0f, 0.0f, 0.0f);
    const auto blueColor  = tcu::Vec4(0.0f, 0.0f, 1.0f, 1.0f);

    tcu::clear(access, clearColor);

    // Fill the top left quarter.
    for (int y = 0; y < halfWidth; ++y)
        for (int x = 0; x < halfHeight; ++x)
        {
            access.setPixel(blueColor, x, y);
        }
}

tcu::TestStatus CustomAttributesInstance::iterate()
{
    struct PerVertexData
    {
        tcu::Vec4 positions[4];
        float pointSizes[4];
        float clipDistances[4];
        tcu::Vec4 custom1[4];
        float custom2[4];
        int32_t custom3[4];
    };

    struct PerPrimitiveData
    {
        // Note some of these are declared as vectors to match the std140 layout.
        tcu::IVec4 primitiveIds[2];
        tcu::IVec4 viewportIndices[2];
        tcu::UVec4 custom4[2];
        tcu::Vec4 custom5[2];
    };

    const auto &vkd       = m_context.getDeviceInterface();
    const auto device     = m_context.getDevice();
    auto &alloc           = m_context.getDefaultAllocator();
    const auto queueIndex = m_context.getUniversalQueueFamilyIndex();
    const auto queue      = m_context.getUniversalQueue();

    const auto imageFormat = getOutputFormat();
    const auto tcuFormat   = mapVkFormat(imageFormat);
    const auto imageExtent = makeExtent3D(m_params->width, m_params->height, 1u);
    const auto imageUsage  = (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);

    const auto &binaries = m_context.getBinaryCollection();
    const auto hasTask   = binaries.contains("task");
    const auto bufStages = (hasTask ? VK_SHADER_STAGE_TASK_BIT_NV : VK_SHADER_STAGE_MESH_BIT_NV);

    const VkImageCreateInfo colorBufferInfo = {
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, // VkStructureType sType;
        nullptr,                             // const void* pNext;
        0u,                                  // VkImageCreateFlags flags;
        VK_IMAGE_TYPE_2D,                    // VkImageType imageType;
        imageFormat,                         // VkFormat format;
        imageExtent,                         // VkExtent3D extent;
        1u,                                  // uint32_t mipLevels;
        1u,                                  // uint32_t arrayLayers;
        VK_SAMPLE_COUNT_1_BIT,               // VkSampleCountFlagBits samples;
        VK_IMAGE_TILING_OPTIMAL,             // VkImageTiling tiling;
        imageUsage,                          // VkImageUsageFlags usage;
        VK_SHARING_MODE_EXCLUSIVE,           // VkSharingMode sharingMode;
        0u,                                  // uint32_t queueFamilyIndexCount;
        nullptr,                             // const uint32_t* pQueueFamilyIndices;
        VK_IMAGE_LAYOUT_UNDEFINED,           // VkImageLayout initialLayout;
    };

    // Create color image and view.
    ImageWithMemory colorImage(vkd, device, alloc, colorBufferInfo, MemoryRequirement::Any);
    const auto colorSRR  = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
    const auto colorSRL  = makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u);
    const auto colorView = makeImageView(vkd, device, colorImage.get(), VK_IMAGE_VIEW_TYPE_2D, imageFormat, colorSRR);

    // Create a memory buffer for verification.
    const auto verificationBufferSize =
        static_cast<VkDeviceSize>(imageExtent.width * imageExtent.height * tcu::getPixelSize(tcuFormat));
    const auto verificationBufferUsage = (VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    const auto verificationBufferInfo  = makeBufferCreateInfo(verificationBufferSize, verificationBufferUsage);

    BufferWithMemory verificationBuffer(vkd, device, alloc, verificationBufferInfo, MemoryRequirement::HostVisible);
    auto &verificationBufferAlloc = verificationBuffer.getAllocation();
    void *verificationBufferData  = verificationBufferAlloc.getHostPtr();

    // This needs to match what the fragment shader will expect.
    const PerVertexData perVertexData = {
        // tcu::Vec4 positions[4];
        {
            tcu::Vec4(-1.0f, -1.0f, 0.0f, 1.0f),
            tcu::Vec4(1.0f, -1.0f, 0.0f, 1.0f),
            tcu::Vec4(-1.0f, 1.0f, 0.0f, 1.0f),
            tcu::Vec4(1.0f, 1.0f, 0.0f, 1.0f),
        },
        // float pointSizes[4];
        {
            1.0f,
            1.0f,
            1.0f,
            1.0f,
        },
        // float clipDistances[4];
        {
            1.0f,
            -1.0f,
            1.0f,
            -1.0f,
        },
        // tcu::Vec4 custom1[4];
        {
            tcu::Vec4(0.25, 0.5, 10.0, 3.0),
            tcu::Vec4(0.25, 1.0, 20.0, 3.0),
            tcu::Vec4(0.5, 0.5, 20.0, 3.0),
            tcu::Vec4(0.5, 1.0, 10.0, 3.0),
        },
        // float custom2[4];
        {
            1.0f,
            1.0f,
            2.0f,
            2.0f,
        },
        // int32_t custom3[4];
        {3, 3, 4, 4},
    };

    // This needs to match what the fragment shader will expect. Reminder: some of these are declared as gvec4 to match the std140
    // layout, but only the first component is actually used.
    const PerPrimitiveData perPrimitiveData = {
        // int primitiveIds[2];
        {
            tcu::IVec4(1000, 0, 0, 0),
            tcu::IVec4(1001, 0, 0, 0),
        },
        // int viewportIndices[2];
        {
            tcu::IVec4(1, 0, 0, 0),
            tcu::IVec4(1, 0, 0, 0),
        },
        // uvec4 custom4[2];
        {
            tcu::UVec4(100u, 101u, 102u, 103u),
            tcu::UVec4(200u, 201u, 202u, 203u),
        },
        // float custom5[2];
        {
            tcu::Vec4(6.0f, 0.0f, 0.0f, 0.0f),
            tcu::Vec4(7.0f, 0.0f, 0.0f, 0.0f),
        },
    };

    // Create and fill buffers with this data.
    const auto pvdSize = static_cast<VkDeviceSize>(sizeof(perVertexData));
    const auto pvdInfo = makeBufferCreateInfo(pvdSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    BufferWithMemory pvdData(vkd, device, alloc, pvdInfo, MemoryRequirement::HostVisible);
    auto &pvdAlloc = pvdData.getAllocation();
    void *pvdPtr   = pvdAlloc.getHostPtr();

    const auto ppdSize = static_cast<VkDeviceSize>(sizeof(perPrimitiveData));
    const auto ppdInfo = makeBufferCreateInfo(ppdSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
    BufferWithMemory ppdData(vkd, device, alloc, ppdInfo, MemoryRequirement::HostVisible);
    auto &ppdAlloc = ppdData.getAllocation();
    void *ppdPtr   = ppdAlloc.getHostPtr();

    deMemcpy(pvdPtr, &perVertexData, sizeof(perVertexData));
    deMemcpy(ppdPtr, &perPrimitiveData, sizeof(perPrimitiveData));

    flushAlloc(vkd, device, pvdAlloc);
    flushAlloc(vkd, device, ppdAlloc);

    // Descriptor set layout.
    DescriptorSetLayoutBuilder setLayoutBuilder;
    setLayoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, bufStages);
    setLayoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, bufStages);
    const auto setLayout = setLayoutBuilder.build(vkd, device);

    // Create and update descriptor set.
    DescriptorPoolBuilder descriptorPoolBuilder;
    descriptorPoolBuilder.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
    descriptorPoolBuilder.addType(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    const auto descriptorPool =
        descriptorPoolBuilder.build(vkd, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);
    const auto descriptorSet = makeDescriptorSet(vkd, device, descriptorPool.get(), setLayout.get());

    DescriptorSetUpdateBuilder updateBuilder;
    const auto storageBufferInfo = makeDescriptorBufferInfo(pvdData.get(), 0ull, pvdSize);
    const auto uniformBufferInfo = makeDescriptorBufferInfo(ppdData.get(), 0ull, ppdSize);
    updateBuilder.writeSingle(descriptorSet.get(), DescriptorSetUpdateBuilder::Location::binding(0u),
                              VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &storageBufferInfo);
    updateBuilder.writeSingle(descriptorSet.get(), DescriptorSetUpdateBuilder::Location::binding(1u),
                              VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &uniformBufferInfo);
    updateBuilder.update(vkd, device);

    // Pipeline layout.
    const auto pipelineLayout = makePipelineLayout(vkd, device, setLayout.get());

    // Shader modules.
    const auto meshShader = createShaderModule(vkd, device, binaries.get("mesh"));
    const auto fragShader = createShaderModule(vkd, device, binaries.get("frag"));

    Move<VkShaderModule> taskShader;
    if (hasTask)
        taskShader = createShaderModule(vkd, device, binaries.get("task"));

    // Render pass.
    const auto renderPass = makeRenderPass(vkd, device, imageFormat);

    // Framebuffer.
    const auto framebuffer =
        makeFramebuffer(vkd, device, renderPass.get(), colorView.get(), imageExtent.width, imageExtent.height);

    // Viewport and scissor.
    const auto topHalf = makeViewport(imageExtent.width, imageExtent.height / 2u);
    const std::vector<VkViewport> viewports{makeViewport(imageExtent), topHalf};
    const std::vector<VkRect2D> scissors(2u, makeRect2D(imageExtent));

    const auto pipeline = makeGraphicsPipeline(vkd, device, pipelineLayout.get(), taskShader.get(), meshShader.get(),
                                               fragShader.get(), renderPass.get(), viewports, scissors);

    // Command pool and buffer.
    const auto cmdPool      = makeCommandPool(vkd, device, queueIndex);
    const auto cmdBufferPtr = allocateCommandBuffer(vkd, device, cmdPool.get(), VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    const auto cmdBuffer    = cmdBufferPtr.get();

    beginCommandBuffer(vkd, cmdBuffer);

    // Run pipeline.
    const tcu::Vec4 clearColor(0.0f, 0.0f, 0.0f, 0.0f);
    beginRenderPass(vkd, cmdBuffer, renderPass.get(), framebuffer.get(), scissors.at(0u), clearColor);
    vkd.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.get());
    vkd.cmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout.get(), 0u, 1u,
                              &descriptorSet.get(), 0u, nullptr);
    vkd.cmdDrawMeshTasksNV(cmdBuffer, m_params->drawCount(), 0u);
    endRenderPass(vkd, cmdBuffer);

    // Copy color buffer to verification buffer.
    const auto colorAccess   = (VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT);
    const auto transferRead  = VK_ACCESS_TRANSFER_READ_BIT;
    const auto transferWrite = VK_ACCESS_TRANSFER_WRITE_BIT;
    const auto hostRead      = VK_ACCESS_HOST_READ_BIT;

    const auto preCopyBarrier =
        makeImageMemoryBarrier(colorAccess, transferRead, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                               VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, colorImage.get(), colorSRR);
    const auto postCopyBarrier = makeMemoryBarrier(transferWrite, hostRead);
    const auto copyRegion      = makeBufferImageCopy(imageExtent, colorSRL);

    vkd.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u,
                           0u, nullptr, 0u, nullptr, 1u, &preCopyBarrier);
    vkd.cmdCopyImageToBuffer(cmdBuffer, colorImage.get(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                             verificationBuffer.get(), 1u, &copyRegion);
    vkd.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0u, 1u,
                           &postCopyBarrier, 0u, nullptr, 0u, nullptr);

    endCommandBuffer(vkd, cmdBuffer);
    submitCommandsAndWait(vkd, device, queue, cmdBuffer);

    // Generate reference image and compare results.
    const tcu::IVec3 iExtent(static_cast<int>(imageExtent.width), static_cast<int>(imageExtent.height), 1);
    const tcu::ConstPixelBufferAccess verificationAccess(tcuFormat, iExtent, verificationBufferData);

    generateReferenceLevel();
    invalidateAlloc(vkd, device, verificationBufferAlloc);
    if (!verifyResult(verificationAccess))
        TCU_FAIL("Result does not match reference; check log for details");

    return tcu::TestStatus::pass("Pass");
}

// Tests that use push constants in the new stages.
class PushConstantCase : public MeshShaderMiscCase
{
public:
    PushConstantCase(tcu::TestContext &testCtx, const std::string &name, const std::string &description,
                     ParamsPtr params)
        : MeshShaderMiscCase(testCtx, name, description, std::move(params))
    {
    }

    void initPrograms(vk::SourceCollections &programCollection) const override;
    TestInstance *createInstance(Context &context) const override;
};

class PushConstantInstance : public MeshShaderMiscInstance
{
public:
    PushConstantInstance(Context &context, const MiscTestParams *params) : MeshShaderMiscInstance(context, params)
    {
    }

    void generateReferenceLevel() override;
    tcu::TestStatus iterate() override;
};

TestInstance *PushConstantCase::createInstance(Context &context) const
{
    return new PushConstantInstance(context, m_params.get());
}

void PushConstantInstance::generateReferenceLevel()
{
    generateSolidRefLevel(tcu::Vec4(0.0f, 0.0f, 1.0f, 1.0f), m_referenceLevel);
}

void PushConstantCase::initPrograms(vk::SourceCollections &programCollection) const
{
    const auto useTaskShader = m_params->needsTaskShader();
    const auto pcNumFloats   = (useTaskShader ? 2u : 4u);

    std::ostringstream pushConstantStream;
    pushConstantStream << "layout (push_constant, std430) uniform PushConstantBlock {\n"
                       << "    layout (offset=${PCOFFSET}) float values[" << pcNumFloats << "];\n"
                       << "} pc;\n"
                       << "\n";
    const tcu::StringTemplate pushConstantsTemplate(pushConstantStream.str());
    using TemplateMap = std::map<std::string, std::string>;

    std::ostringstream taskDataStream;
    taskDataStream << "taskNV TaskData {\n"
                   << "    float values[2];\n"
                   << "} td;\n"
                   << "\n";
    const auto taskDataDecl = taskDataStream.str();

    if (useTaskShader)
    {
        TemplateMap taskMap;
        taskMap["PCOFFSET"] = std::to_string(2u * sizeof(float));

        std::ostringstream task;
        task << "#version 450\n"
             << "#extension GL_NV_mesh_shader : enable\n"
             << "\n"
             << "layout(local_size_x=1) in;\n"
             << "\n"
             << "out " << taskDataDecl << pushConstantsTemplate.specialize(taskMap) << "void main ()\n"
             << "{\n"
             << "    gl_TaskCountNV = " << m_params->meshCount << ";\n"
             << "\n"
             << "    td.values[0] = pc.values[0];\n"
             << "    td.values[1] = pc.values[1];\n"
             << "}\n";
        programCollection.glslSources.add("task") << glu::TaskSource(task.str());
    }

    {
        const std::string blue  = (useTaskShader ? "td.values[0] + pc.values[0]" : "pc.values[0] + pc.values[2]");
        const std::string alpha = (useTaskShader ? "td.values[1] + pc.values[1]" : "pc.values[1] + pc.values[3]");

        TemplateMap meshMap;
        meshMap["PCOFFSET"] = "0";

        std::ostringstream mesh;
        mesh << "#version 450\n"
             << "#extension GL_NV_mesh_shader : enable\n"
             << "\n"
             << "layout(local_size_x=1) in;\n"
             << "layout(triangles) out;\n"
             << "layout(max_vertices=3, max_primitives=1) out;\n"
             << "\n"
             << "layout (location=0) out perprimitiveNV vec4 triangleColor[];\n"
             << "\n"
             << pushConstantsTemplate.specialize(meshMap) << (useTaskShader ? "in " + taskDataDecl : "")
             << "void main ()\n"
             << "{\n"
             << "    gl_PrimitiveCountNV = 1;\n"
             << "\n"
             << "    gl_MeshVerticesNV[0].gl_Position = vec4(-1.0, -1.0, 0.0, 1.0);\n"
             << "    gl_MeshVerticesNV[1].gl_Position = vec4( 3.0, -1.0, 0.0, 1.0);\n"
             << "    gl_MeshVerticesNV[2].gl_Position = vec4(-1.0,  3.0, 0.0, 1.0);\n"
             << "\n"
             << "    gl_PrimitiveIndicesNV[0] = 0;\n"
             << "    gl_PrimitiveIndicesNV[1] = 1;\n"
             << "    gl_PrimitiveIndicesNV[2] = 2;\n"
             << "\n"
             << "    triangleColor[0] = vec4(0.0, 0.0, " << blue << ", " << alpha << ");\n"
             << "}\n";
        programCollection.glslSources.add("mesh") << glu::MeshSource(mesh.str());
    }

    // Add default fragment shader.
    MeshShaderMiscCase::initPrograms(programCollection);
}

tcu::TestStatus PushConstantInstance::iterate()
{
    const auto &vkd       = m_context.getDeviceInterface();
    const auto device     = m_context.getDevice();
    auto &alloc           = m_context.getDefaultAllocator();
    const auto queueIndex = m_context.getUniversalQueueFamilyIndex();
    const auto queue      = m_context.getUniversalQueue();

    const auto imageFormat = getOutputFormat();
    const auto tcuFormat   = mapVkFormat(imageFormat);
    const auto imageExtent = makeExtent3D(m_params->width, m_params->height, 1u);
    const auto imageUsage  = (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);

    const auto &binaries = m_context.getBinaryCollection();
    const auto hasTask   = binaries.contains("task");

    const VkImageCreateInfo colorBufferInfo = {
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, // VkStructureType sType;
        nullptr,                             // const void* pNext;
        0u,                                  // VkImageCreateFlags flags;
        VK_IMAGE_TYPE_2D,                    // VkImageType imageType;
        imageFormat,                         // VkFormat format;
        imageExtent,                         // VkExtent3D extent;
        1u,                                  // uint32_t mipLevels;
        1u,                                  // uint32_t arrayLayers;
        VK_SAMPLE_COUNT_1_BIT,               // VkSampleCountFlagBits samples;
        VK_IMAGE_TILING_OPTIMAL,             // VkImageTiling tiling;
        imageUsage,                          // VkImageUsageFlags usage;
        VK_SHARING_MODE_EXCLUSIVE,           // VkSharingMode sharingMode;
        0u,                                  // uint32_t queueFamilyIndexCount;
        nullptr,                             // const uint32_t* pQueueFamilyIndices;
        VK_IMAGE_LAYOUT_UNDEFINED,           // VkImageLayout initialLayout;
    };

    // Create color image and view.
    ImageWithMemory colorImage(vkd, device, alloc, colorBufferInfo, MemoryRequirement::Any);
    const auto colorSRR  = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
    const auto colorSRL  = makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u);
    const auto colorView = makeImageView(vkd, device, colorImage.get(), VK_IMAGE_VIEW_TYPE_2D, imageFormat, colorSRR);

    // Create a memory buffer for verification.
    const auto verificationBufferSize =
        static_cast<VkDeviceSize>(imageExtent.width * imageExtent.height * tcu::getPixelSize(tcuFormat));
    const auto verificationBufferUsage = (VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    const auto verificationBufferInfo  = makeBufferCreateInfo(verificationBufferSize, verificationBufferUsage);

    BufferWithMemory verificationBuffer(vkd, device, alloc, verificationBufferInfo, MemoryRequirement::HostVisible);
    auto &verificationBufferAlloc = verificationBuffer.getAllocation();
    void *verificationBufferData  = verificationBufferAlloc.getHostPtr();

    // Push constant ranges.
    std::vector<float> pcData{0.25f, 0.25f, 0.75f, 0.75f};
    const auto pcSize     = static_cast<uint32_t>(de::dataSize(pcData));
    const auto pcHalfSize = pcSize / 2u;

    std::vector<VkPushConstantRange> pcRanges;
    if (hasTask)
    {
        pcRanges.push_back(makePushConstantRange(VK_SHADER_STAGE_MESH_BIT_NV, 0u, pcHalfSize));
        pcRanges.push_back(makePushConstantRange(VK_SHADER_STAGE_TASK_BIT_NV, pcHalfSize, pcHalfSize));
    }
    else
    {
        pcRanges.push_back(makePushConstantRange(VK_SHADER_STAGE_MESH_BIT_NV, 0u, pcSize));
    }

    // Pipeline layout.
    const auto pipelineLayout =
        makePipelineLayout(vkd, device, 0u, nullptr, static_cast<uint32_t>(pcRanges.size()), de::dataOrNull(pcRanges));

    // Shader modules.
    const auto meshShader = createShaderModule(vkd, device, binaries.get("mesh"));
    const auto fragShader = createShaderModule(vkd, device, binaries.get("frag"));

    Move<VkShaderModule> taskShader;
    if (hasTask)
        taskShader = createShaderModule(vkd, device, binaries.get("task"));

    // Render pass.
    const auto renderPass = makeRenderPass(vkd, device, imageFormat);

    // Framebuffer.
    const auto framebuffer =
        makeFramebuffer(vkd, device, renderPass.get(), colorView.get(), imageExtent.width, imageExtent.height);

    // Viewport and scissor.
    const std::vector<VkViewport> viewports(1u, makeViewport(imageExtent));
    const std::vector<VkRect2D> scissors(1u, makeRect2D(imageExtent));

    const auto pipeline = makeGraphicsPipeline(vkd, device, pipelineLayout.get(), taskShader.get(), meshShader.get(),
                                               fragShader.get(), renderPass.get(), viewports, scissors);

    // Command pool and buffer.
    const auto cmdPool      = makeCommandPool(vkd, device, queueIndex);
    const auto cmdBufferPtr = allocateCommandBuffer(vkd, device, cmdPool.get(), VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    const auto cmdBuffer    = cmdBufferPtr.get();

    beginCommandBuffer(vkd, cmdBuffer);

    // Run pipeline.
    const tcu::Vec4 clearColor(0.0f, 0.0f, 0.0f, 0.0f);
    beginRenderPass(vkd, cmdBuffer, renderPass.get(), framebuffer.get(), scissors.at(0u), clearColor);
    vkd.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.get());
    for (const auto &range : pcRanges)
        vkd.cmdPushConstants(cmdBuffer, pipelineLayout.get(), range.stageFlags, range.offset, range.size,
                             reinterpret_cast<const char *>(pcData.data()) + range.offset);
    vkd.cmdDrawMeshTasksNV(cmdBuffer, m_params->drawCount(), 0u);
    endRenderPass(vkd, cmdBuffer);

    // Copy color buffer to verification buffer.
    const auto colorAccess   = (VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT);
    const auto transferRead  = VK_ACCESS_TRANSFER_READ_BIT;
    const auto transferWrite = VK_ACCESS_TRANSFER_WRITE_BIT;
    const auto hostRead      = VK_ACCESS_HOST_READ_BIT;

    const auto preCopyBarrier =
        makeImageMemoryBarrier(colorAccess, transferRead, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                               VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, colorImage.get(), colorSRR);
    const auto postCopyBarrier = makeMemoryBarrier(transferWrite, hostRead);
    const auto copyRegion      = makeBufferImageCopy(imageExtent, colorSRL);

    vkd.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u,
                           0u, nullptr, 0u, nullptr, 1u, &preCopyBarrier);
    vkd.cmdCopyImageToBuffer(cmdBuffer, colorImage.get(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                             verificationBuffer.get(), 1u, &copyRegion);
    vkd.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0u, 1u,
                           &postCopyBarrier, 0u, nullptr, 0u, nullptr);

    endCommandBuffer(vkd, cmdBuffer);
    submitCommandsAndWait(vkd, device, queue, cmdBuffer);

    // Generate reference image and compare results.
    const tcu::IVec3 iExtent(static_cast<int>(imageExtent.width), static_cast<int>(imageExtent.height), 1);
    const tcu::ConstPixelBufferAccess verificationAccess(tcuFormat, iExtent, verificationBufferData);

    generateReferenceLevel();
    invalidateAlloc(vkd, device, verificationBufferAlloc);
    if (!verifyResult(verificationAccess))
        TCU_FAIL("Result does not match reference; check log for details");

    return tcu::TestStatus::pass("Pass");
}

} // namespace

tcu::TestCaseGroup *createMeshShaderMiscTests(tcu::TestContext &testCtx)
{
    GroupPtr miscTests(new tcu::TestCaseGroup(testCtx, "misc", "Mesh Shader Misc Tests"));

    {
        ParamsPtr paramsPtr(new MiscTestParams);

        paramsPtr->taskCount = tcu::just(2u);
        paramsPtr->meshCount = 2u;
        paramsPtr->width     = 8u;
        paramsPtr->height    = 8u;

        miscTests->addChild(new ComplexTaskDataCase(testCtx, "complex_task_data",
                                                    "Pass a complex structure from the task to the mesh shader",
                                                    std::move(paramsPtr)));
    }

    {
        ParamsPtr paramsPtr(new MiscTestParams);

        paramsPtr->taskCount = tcu::nothing<uint32_t>();
        paramsPtr->meshCount = 1u;
        paramsPtr->width     = 5u; // Use an odd value so there's a pixel in the exact center.
        paramsPtr->height    = 7u; // Idem.

        miscTests->addChild(new SinglePointCase(testCtx, "single_point", "Draw a single point", std::move(paramsPtr)));
    }

    {
        ParamsPtr paramsPtr(new MiscTestParams);

        paramsPtr->taskCount = tcu::nothing<uint32_t>();
        paramsPtr->meshCount = 1u;
        paramsPtr->width     = 8u;
        paramsPtr->height    = 5u; // Use an odd value so there's a center line.

        miscTests->addChild(new SingleLineCase(testCtx, "single_line", "Draw a single line", std::move(paramsPtr)));
    }

    {
        ParamsPtr paramsPtr(new MiscTestParams);

        paramsPtr->taskCount = tcu::nothing<uint32_t>();
        paramsPtr->meshCount = 1u;
        paramsPtr->width     = 5u; // Use an odd value so there's a pixel in the exact center.
        paramsPtr->height    = 7u; // Idem.

        miscTests->addChild(
            new SingleTriangleCase(testCtx, "single_triangle", "Draw a single triangle", std::move(paramsPtr)));
    }

    {
        ParamsPtr paramsPtr(new MiscTestParams);

        paramsPtr->taskCount = tcu::nothing<uint32_t>();
        paramsPtr->meshCount = 1u;
        paramsPtr->width     = 16u;
        paramsPtr->height    = 16u;

        miscTests->addChild(
            new MaxPointsCase(testCtx, "max_points", "Draw the maximum number of points", std::move(paramsPtr)));
    }

    {
        ParamsPtr paramsPtr(new MiscTestParams);

        paramsPtr->taskCount = tcu::nothing<uint32_t>();
        paramsPtr->meshCount = 1u;
        paramsPtr->width     = 1u;
        paramsPtr->height    = 1020u;

        miscTests->addChild(
            new MaxLinesCase(testCtx, "max_lines", "Draw the maximum number of lines", std::move(paramsPtr)));
    }

    {
        ParamsPtr paramsPtr(new MiscTestParams);

        paramsPtr->taskCount = tcu::nothing<uint32_t>();
        paramsPtr->meshCount = 1u;
        paramsPtr->width     = 512u;
        paramsPtr->height    = 512u;

        miscTests->addChild(new MaxTrianglesCase(testCtx, "max_triangles", "Draw the maximum number of triangles",
                                                 std::move(paramsPtr)));
    }

    {
        ParamsPtr paramsPtr(new MiscTestParams);

        paramsPtr->taskCount = tcu::just(65535u);
        paramsPtr->meshCount = 1u;
        paramsPtr->width     = 1360u;
        paramsPtr->height    = 1542u;

        miscTests->addChild(new LargeWorkGroupCase(
            testCtx, "many_task_work_groups", "Generate a large number of task work groups", std::move(paramsPtr)));
    }

    {
        ParamsPtr paramsPtr(new MiscTestParams);

        paramsPtr->taskCount = tcu::nothing<uint32_t>();
        paramsPtr->meshCount = 65535u;
        paramsPtr->width     = 1360u;
        paramsPtr->height    = 1542u;

        miscTests->addChild(new LargeWorkGroupCase(
            testCtx, "many_mesh_work_groups", "Generate a large number of mesh work groups", std::move(paramsPtr)));
    }

    {
        ParamsPtr paramsPtr(new MiscTestParams);

        paramsPtr->taskCount = tcu::just(512u);
        paramsPtr->meshCount = 512u;
        paramsPtr->width     = 4096u;
        paramsPtr->height    = 2048u;

        miscTests->addChild(new LargeWorkGroupCase(testCtx, "many_task_mesh_work_groups",
                                                   "Generate a large number of task and mesh work groups",
                                                   std::move(paramsPtr)));
    }

    {
        const PrimitiveType types[] = {
            PrimitiveType::POINTS,
            PrimitiveType::LINES,
            PrimitiveType::TRIANGLES,
        };

        for (int i = 0; i < 2; ++i)
        {
            const bool extraWrites = (i > 0);

            for (const auto primType : types)
            {
                std::unique_ptr<NoPrimitivesParams> params(new NoPrimitivesParams);
                params->taskCount     = (extraWrites ? tcu::just(1u) : tcu::nothing<uint32_t>());
                params->meshCount     = 1u;
                params->width         = 16u;
                params->height        = 16u;
                params->primitiveType = primType;

                ParamsPtr paramsPtr(params.release());
                const auto primName    = primitiveTypeName(primType);
                const std::string name = "no_" + primName + (extraWrites ? "_extra_writes" : "");
                const std::string desc = "Run a pipeline that generates no " + primName +
                                         (extraWrites ? " but generates primitive data" : "");

                miscTests->addChild(extraWrites ?
                                        (new NoPrimitivesExtraWritesCase(testCtx, name, desc, std::move(paramsPtr))) :
                                        (new NoPrimitivesCase(testCtx, name, desc, std::move(paramsPtr))));
            }
        }
    }

    {
        for (int i = 0; i < 2; ++i)
        {
            const bool useTaskShader = (i == 0);

            ParamsPtr paramsPtr(new MiscTestParams);

            paramsPtr->taskCount = (useTaskShader ? tcu::just(1u) : tcu::nothing<uint32_t>());
            paramsPtr->meshCount = 1u;
            paramsPtr->width     = 1u;
            paramsPtr->height    = 1u;

            const std::string shader = (useTaskShader ? "task" : "mesh");
            const std::string name   = "barrier_in_" + shader;
            const std::string desc   = "Use a control barrier in the " + shader + " shader";

            miscTests->addChild(new SimpleBarrierCase(testCtx, name, desc, std::move(paramsPtr)));
        }
    }

    {
        const struct
        {
            MemoryBarrierType memBarrierType;
            std::string caseName;
        } barrierTypes[] = {
            {MemoryBarrierType::SHARED, "memory_barrier_shared"},
            {MemoryBarrierType::GROUP, "group_memory_barrier"},
        };

        for (const auto &barrierCase : barrierTypes)
        {
            for (int i = 0; i < 2; ++i)
            {
                const bool useTaskShader = (i == 0);

                std::unique_ptr<MemoryBarrierParams> paramsPtr(new MemoryBarrierParams);

                paramsPtr->taskCount      = (useTaskShader ? tcu::just(1u) : tcu::nothing<uint32_t>());
                paramsPtr->meshCount      = 1u;
                paramsPtr->width          = 1u;
                paramsPtr->height         = 1u;
                paramsPtr->memBarrierType = barrierCase.memBarrierType;

                const std::string shader = (useTaskShader ? "task" : "mesh");
                const std::string name   = barrierCase.caseName + "_in_" + shader;
                const std::string desc   = "Use " + paramsPtr->glslFunc() + "() in the " + shader + " shader";

                miscTests->addChild(new MemoryBarrierCase(testCtx, name, desc, std::move(paramsPtr)));
            }
        }
    }

    {
        for (int i = 0; i < 2; ++i)
        {
            const bool useTaskShader = (i > 0);
            const auto name          = std::string("custom_attributes") + (useTaskShader ? "_and_task_shader" : "");
            const auto desc          = std::string("Use several custom vertex and primitive attributes") +
                              (useTaskShader ? " and also a task shader" : "");

            ParamsPtr paramsPtr(new MiscTestParams);

            paramsPtr->taskCount = (useTaskShader ? tcu::just(1u) : tcu::nothing<uint32_t>());
            paramsPtr->meshCount = 1u;
            paramsPtr->width     = 32u;
            paramsPtr->height    = 32u;

            miscTests->addChild(new CustomAttributesCase(testCtx, name, desc, std::move(paramsPtr)));
        }
    }

    {
        for (int i = 0; i < 2; ++i)
        {
            const bool useTaskShader = (i > 0);
            const auto name          = std::string("push_constant") + (useTaskShader ? "_and_task_shader" : "");
            const auto desc          = std::string("Use push constants in the mesh shader stage") +
                              (useTaskShader ? " and also in the task shader stage" : "");

            ParamsPtr paramsPtr(new MiscTestParams);

            paramsPtr->taskCount = (useTaskShader ? tcu::just(1u) : tcu::nothing<uint32_t>());
            paramsPtr->meshCount = 1u;
            paramsPtr->width     = 16u;
            paramsPtr->height    = 16u;

            miscTests->addChild(new PushConstantCase(testCtx, name, desc, std::move(paramsPtr)));
        }
    }

    return miscTests.release();
}

} // namespace MeshShader
} // namespace vkt
