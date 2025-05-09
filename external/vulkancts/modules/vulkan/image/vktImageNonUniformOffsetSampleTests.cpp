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
 * \brief Tests using texture*Offset with non-uniform offset values.
 *//*--------------------------------------------------------------------*/

// Test non-uniform offsets can be used with texture*Offset functions. This requires VK_KHR_maintenance8 and the GLSL
// GL_EXT_texture_offset_non_uniform extension.
//
// These tests will (for all practical purposes) create a 3x3 texture and a 3x3 framebuffer, and will try to fill
// the framebuffer using texels from the texture sampled with texture*Offset functions.
//
// To test non-uniform offsets can be used with texture*Offset, the tests will generally use sampling coordinates for
// the top-left pixel always, and they will use the offset to actually choose which texel to obtain.
//
// To make these offsets non-uniform, the offsets will be generated pseudorandomly and obtained from an array in a
// uniform buffer at an index that will vary by invocation.
//
// We skip textureGrad* functions to simplify.
//
// We will test vertex, fragment and compute stages.
//
// * In the compute stage, the offset will vary for each invocation by using gl_LocalInvocationIndex.
// * In the vertex stage, the offset will vary by primitive coords (one primitive per pixel).
// * In the fragment stage, the offset will vary by fragment coords.
//
// Note offsets will have values between 0 and 2 in each coordinate, which fall within the mandatory limits for
// minTexelOffset and maxTexelOffset, so no checks are needed.

#include "vktImageNonUniformOffsetSampleTests.hpp"
#include "vktTestCase.hpp"
#include "vkDefs.hpp"
#include "vkTypeUtil.hpp"
#include "vkImageWithMemory.hpp"
#include "vkObjUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkBarrierUtil.hpp"

#include "tcuVector.hpp"
#include "tcuImageCompare.hpp"

#include "deUniquePtr.hpp"
#include "deRandom.hpp"

#include <vector>
#include <string>

namespace vkt
{
namespace image
{

namespace
{

using namespace vk;

enum class TestFunc
{
    TEXTURE = 0,
    TEXEL_FETCH,
    TEXTURE_LOD,
    TEXTURE_PROJ,
    TEXTURE_PROJ_LOD,
};

bool hasLodArg(TestFunc func)
{
    bool hasLod = false;

    switch (func)
    {
    case TestFunc::TEXTURE: // Fallthrough.
    case TestFunc::TEXTURE_PROJ:
        hasLod = false;
        break;
    case TestFunc::TEXEL_FETCH: // Fallthrough.
    case TestFunc::TEXTURE_LOD: // Fallthrough.
    case TestFunc::TEXTURE_PROJ_LOD:
        hasLod = true;
        break;
    default:
        DE_ASSERT(false);
        break;
    }

    return hasLod;
}

struct TestParams
{
    VkShaderStageFlagBits testStage;
    TestFunc testFunction;
    bool multiMip;

    // Get random seed based on these parameters.
    uint32_t getSeed(void) const
    {
        return ((static_cast<uint32_t>(testStage) << 16) | (static_cast<uint32_t>(testFunction) << 8) |
                static_cast<uint32_t>(multiMip));
    }

    bool isCompute() const
    {
        return (testStage == VK_SHADER_STAGE_COMPUTE_BIT);
    }

    bool isGraphics() const
    {
        return (testStage != VK_SHADER_STAGE_COMPUTE_BIT);
    }
};

// Size for the framebuffer and the texture.
tcu::IVec3 getSize(void)
{
    return tcu::IVec3(3, 3, 1);
}

class NonUniformOffsetInstance : public vkt::TestInstance
{
public:
    NonUniformOffsetInstance(Context &context, const TestParams &params) : vkt::TestInstance(context), m_params(params)
    {
    }
    virtual ~NonUniformOffsetInstance(void) = default;

    tcu::TestStatus iterate(void) override;

protected:
    const TestParams m_params;
};

class NonUniformOffsetCase : public vkt::TestCase
{
public:
    NonUniformOffsetCase(tcu::TestContext &testCtx, const std::string &name, const TestParams &params)
        : vkt::TestCase(testCtx, name)
        , m_params(params)
    {
        if (m_params.multiMip)
            DE_ASSERT(hasLodArg(m_params.testFunction));
    }
    virtual ~NonUniformOffsetCase(void) = default;

    void checkSupport(Context &context) const override
    {
        context.requireDeviceFunctionality("VK_KHR_maintenance8");
    }

    TestInstance *createInstance(Context &context) const override
    {
        return new NonUniformOffsetInstance(context, m_params);
    }

    void initPrograms(vk::SourceCollections &programCollection) const override;

protected:
    const TestParams m_params;
};

// This declartion matches the GLSL declaration below.
struct PushConstantBlock
{
    tcu::Vec4 coords;
    tcu::Vec2 size;
    float lod;
};

void NonUniformOffsetCase::initPrograms(vk::SourceCollections &programCollection) const
{
    const auto spvValFlags = ShaderBuildOptions::FLAG_ALLOW_NON_CONST_OFFSETS;
    const ShaderBuildOptions buildOptions(programCollection.usedVulkanVersion, SPIRV_VERSION_1_0, spvValFlags);

    const auto fbSize     = getSize();
    const auto pixelCount = fbSize.x() * fbSize.y() * fbSize.z();

    const std::string pushConstantDecl =
        "layout (push_constant, std430) uniform PCBlock { vec4 coords; vec2 size; float lod; } pc;\n";
    const std::string offsetDataDecl = "layout (set=0, binding=0) uniform OffsetDataBlock { ivec4 offsets[" +
                                       std::to_string(pixelCount) + "]; } offsetData;\n";
    const std::string textureDecl = "layout (set=0, binding=1) uniform sampler2D inTex;\n";

    std::string coordsDecl;
    std::string readTexture;

    // We suppose there's a variable called offset that contains the offset for each invocation.
    // offset will be calculated differently depending on the type of shader.
    switch (m_params.testFunction)
    {
    case TestFunc::TEXTURE:
        coordsDecl  = "const vec2 texCoords = pc.coords.xy;\n";
        readTexture = "const vec4 pixel = textureOffset(inTex, texCoords, offset);\n";
        break;
    case TestFunc::TEXEL_FETCH:
        coordsDecl  = "const ivec2 texCoords = ivec2(pc.coords.xy);\n";
        readTexture = "const vec4 pixel = texelFetchOffset(inTex, texCoords, int(pc.lod), offset);\n";
        break;
    case TestFunc::TEXTURE_LOD:
        coordsDecl  = "const vec2 texCoords = pc.coords.xy;\n";
        readTexture = "const vec4 pixel = textureLodOffset(inTex, texCoords, pc.lod, offset);\n";
        break;
    case TestFunc::TEXTURE_PROJ:
        coordsDecl  = "const vec3 texCoords = pc.coords.xyz;\n";
        readTexture = "const vec4 pixel = textureProjOffset(inTex, texCoords, offset);\n";
        break;
    case TestFunc::TEXTURE_PROJ_LOD:
        coordsDecl  = "const vec3 texCoords = pc.coords.xyz;\n";
        readTexture = "const vec4 pixel = textureProjLodOffset(inTex, texCoords, pc.lod, offset);\n";
        break;
    default:
        DE_ASSERT(false);
        break;
    }

    if (m_params.testStage == VK_SHADER_STAGE_COMPUTE_BIT)
    {
        std::ostringstream comp;
        comp << "#version 460\n"
             << "#extension GL_EXT_texture_offset_non_const : enable\n"
             << pushConstantDecl << offsetDataDecl << textureDecl
             << "layout (rgba8, set=0, binding=2) uniform image2D outColor;\n"
             << "layout (local_size_x=" << fbSize.x() << ", local_size_y=" << fbSize.y() << ", local_size_z=1) in;\n"
             << "void main(void) {\n"
             << "    const uint offsetIndex = (gl_LocalInvocationID.y * gl_WorkGroupSize.x) + gl_LocalInvocationID.x;\n"
             << "    const ivec2 offset = offsetData.offsets[offsetIndex].xy;\n"
             << "    " << coordsDecl << "    " << readTexture
             << "    imageStore(outColor, ivec2(gl_LocalInvocationID.xy), pixel);\n"
             << "}\n";
        programCollection.glslSources.add("comp") << glu::ComputeSource(comp.str()) << buildOptions;
    }
    else if (m_params.testStage == VK_SHADER_STAGE_FRAGMENT_BIT)
    {
        std::ostringstream vert;
        vert << "#version 460\n"
             << "layout (location=0) in vec4 inPos;\n"
             << "void main(void) {\n"
             << "    gl_Position = inPos;\n"
             << "    gl_PointSize = 1.0;\n"
             << "}\n";
        programCollection.glslSources.add("vert") << glu::VertexSource(vert.str());

        std::ostringstream frag;
        frag << "#version 460\n"
             << "#extension GL_EXT_texture_offset_non_const : enable\n"
             << pushConstantDecl << offsetDataDecl << textureDecl << "layout (location=0) out vec4 outColor;\n"
             << "void main(void) {\n"
             << "    const uint offsetIndex = uint(gl_FragCoord.y) * uint(pc.size.x) + uint(gl_FragCoord.x);\n"
             << "    const ivec2 offset = offsetData.offsets[offsetIndex].xy;\n"
             << "    " << coordsDecl << "    " << readTexture << "    outColor = pixel;\n"
             << "}\n";
        programCollection.glslSources.add("frag") << glu::FragmentSource(frag.str()) << buildOptions;
    }
    else if (m_params.testStage == VK_SHADER_STAGE_VERTEX_BIT)
    {
        std::ostringstream vert;
        vert << "#version 460\n"
             << "#extension GL_EXT_texture_offset_non_const : enable\n"
             << pushConstantDecl << offsetDataDecl << textureDecl << "layout (location=0) out vec4 outColor;\n"
             << "layout (location=0) in vec4 inPos;\n"
             << "void main(void) {\n"
             << "    const uvec2 pixelId = uvec2((inPos.xy + vec2(1.0, 1.0)) / vec2(2.0, 2.0) * pc.size);"
             << "    const uint offsetIndex = pixelId.y * uint(pc.size.x) + pixelId.x;\n"
             << "    const ivec2 offset = offsetData.offsets[offsetIndex].xy;\n"
             << "    " << coordsDecl << "    " << readTexture << "    outColor = pixel;\n"
             << "    gl_Position = inPos;\n"
             << "    gl_PointSize = 1.0;\n"
             << "}\n";
        programCollection.glslSources.add("vert") << glu::VertexSource(vert.str()) << buildOptions;

        std::ostringstream frag;
        frag << "#version 460\n"
             << "layout (location=0) out vec4 outColor;\n"
             << "layout (location=0) in vec4 inColor;\n"
             << "void main(void) {\n"
             << "    outColor = inColor;\n"
             << "}\n";
        programCollection.glslSources.add("frag") << glu::FragmentSource(frag.str());
    }
    else
        DE_ASSERT(false);
}

constexpr int kTriangleVertices = 3;

tcu::TestStatus NonUniformOffsetInstance::iterate(void)
{
    const auto ctx          = m_context.getContextCommonData();
    const auto fbExtent     = getSize();
    const auto pixelCount   = fbExtent.x() * fbExtent.y() * fbExtent.z();
    const auto apiExtent    = makeExtent3D(fbExtent);
    const auto imgFormat    = VK_FORMAT_R8G8B8A8_UNORM;
    const auto mipLevels    = (m_params.multiMip ? 4u : 1u);
    const auto lastMipLevel = mipLevels - 1u;
    const auto texExtent =
        makeExtent3D((apiExtent.width << lastMipLevel), (apiExtent.height << lastMipLevel), apiExtent.depth);
    const auto texUsage = (VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
    const auto fbUsage  = (VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                          (m_params.isCompute() ? VK_IMAGE_USAGE_STORAGE_BIT : VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT));

    // We need to prepare the texture, the sampler, the framebuffer (or storage image), the output verification buffer and the uniform buffer.

    const VkImageCreateInfo textureCreateInfo = {
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        nullptr,
        0u,
        VK_IMAGE_TYPE_2D,
        imgFormat,
        texExtent,
        mipLevels,
        1u,
        VK_SAMPLE_COUNT_1_BIT,
        VK_IMAGE_TILING_OPTIMAL,
        texUsage,
        VK_SHARING_MODE_EXCLUSIVE,
        0u,
        nullptr,
        VK_IMAGE_LAYOUT_UNDEFINED,
    };
    ImageWithMemory texImg(ctx.vkd, ctx.device, ctx.allocator, textureCreateInfo, MemoryRequirement::Any);
    const auto texSRR  = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, mipLevels, 0u, 1u);
    const auto texView = makeImageView(ctx.vkd, ctx.device, *texImg, VK_IMAGE_VIEW_TYPE_2D, imgFormat, texSRR);
    const auto texSRL  = makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, lastMipLevel, 0u, 1u);

    // Host version of the texture.
    const auto tcuFormat = mapVkFormat(imgFormat);
    tcu::TextureLevel hostTexture(tcuFormat, fbExtent.x(), fbExtent.y(), fbExtent.z());
    auto hostTexAccess = hostTexture.getAccess();

    DE_ASSERT(fbExtent.x() > 1);
    DE_ASSERT(fbExtent.y() > 1);
    DE_ASSERT(fbExtent.z() == 1);

    const auto floatExtent = fbExtent.asFloat();

    for (int y = 0; y < fbExtent.y(); ++y)
        for (int x = 0; x < fbExtent.x(); ++x)
        {
            const float r     = static_cast<float>(x) / (floatExtent.x() - 1.0f);
            const float g     = static_cast<float>(y) / (floatExtent.y() - 1.0f);
            constexpr float b = 0.5f;
            constexpr float a = 1.0f;

            hostTexAccess.setPixel(tcu::Vec4(r, g, b, a), x, y);
        }

    // Copy texture data to a host-visible buffer.
    // This will have to be copied to the proper texture mip level later.
    const auto texBufferPixelSize  = tcu::getPixelSize(tcuFormat);
    const auto texBufferSize       = static_cast<VkDeviceSize>(texBufferPixelSize * pixelCount);
    const auto texBufferCreateInfo = makeBufferCreateInfo(texBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    BufferWithMemory texBuffer(ctx.vkd, ctx.device, ctx.allocator, texBufferCreateInfo, MemoryRequirement::HostVisible);
    {
        auto &alloc = texBuffer.getAllocation();
        deMemcpy(alloc.getHostPtr(), hostTexAccess.getDataPtr(), static_cast<size_t>(texBufferSize));
        flushAlloc(ctx.vkd, ctx.device, alloc);
    }

    const VkSamplerCreateInfo samplerCreateInfo = {
        VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        nullptr,
        0u,
        VK_FILTER_NEAREST,
        VK_FILTER_NEAREST,
        VK_SAMPLER_MIPMAP_MODE_NEAREST,
        VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        0.0f,
        VK_FALSE,
        0.0f,
        VK_FALSE,
        VK_COMPARE_OP_ALWAYS,
        0.0f,
        static_cast<float>(mipLevels - 1u),
        VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
        VK_FALSE,
    };
    const auto sampler = createSampler(ctx.vkd, ctx.device, &samplerCreateInfo);

    // Framebuffer (or storage buffer) image, view and buffer.
    ImageWithBuffer fbImg(ctx.vkd, ctx.device, ctx.allocator, apiExtent, imgFormat, fbUsage, VK_IMAGE_TYPE_2D);
    const auto fbSRR = makeDefaultImageSubresourceRange();
    const auto fbUsageLayout =
        (m_params.isCompute() ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    de::MovePtr<BufferWithMemory> vertexBuffer;
    std::vector<tcu::Vec4> vertices;

    if (m_params.isGraphics())
    {
        if (m_params.testStage == VK_SHADER_STAGE_FRAGMENT_BIT)
        {
            // Full-screen quad as a triangle strip.
            vertices.reserve(4u);
            vertices.emplace_back(-1.0f, -1.0f, 0.0f, 1.0f);
            vertices.emplace_back(-1.0f, 1.0f, 0.0f, 1.0f);
            vertices.emplace_back(1.0f, -1.0f, 0.0f, 1.0f);
            vertices.emplace_back(1.0f, 1.0f, 0.0f, 1.0f);
        }
        else if (m_params.testStage == VK_SHADER_STAGE_VERTEX_BIT)
        {
            // One triangle per pixel.
            vertices.reserve(pixelCount * kTriangleVertices);

            const auto pixelWidth  = 2.0f / floatExtent.x();
            const auto pixelHeight = 2.0f / floatExtent.y();
            const auto horMargin   = pixelWidth / 4.0f;
            const auto vertMargin  = pixelHeight / 4.0f;

            for (int y = 0; y < fbExtent.y(); ++y)
                for (int x = 0; x < fbExtent.x(); ++x)
                {
                    const auto xCenter = (static_cast<float>(x) + 0.5f) / floatExtent.x() * 2.0f - 1.0f;
                    const auto yCenter = (static_cast<float>(y) + 0.5f) / floatExtent.y() * 2.0f - 1.0f;

                    vertices.emplace_back(xCenter - horMargin, yCenter + vertMargin, 0.0f, 1.0f);
                    vertices.emplace_back(xCenter + horMargin, yCenter + vertMargin, 0.0f, 1.0f);
                    vertices.emplace_back(xCenter, yCenter - vertMargin, 0.0f, 1.0f);
                }
        }
        else
            DE_ASSERT(false);

        const auto vtxBufferSize       = static_cast<VkDeviceSize>(de::dataSize(vertices));
        const auto vtxBufferCreateInfo = makeBufferCreateInfo(vtxBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
        vertexBuffer = de::MovePtr(new BufferWithMemory(ctx.vkd, ctx.device, ctx.allocator, vtxBufferCreateInfo,
                                                        MemoryRequirement::HostVisible));
        {
            auto &alloc = vertexBuffer->getAllocation();
            deMemcpy(alloc.getHostPtr(), de::dataOrNull(vertices), de::dataSize(vertices));
            flushAlloc(ctx.vkd, ctx.device, alloc);
        }
    }

    // Prepare pseudorandom offsets. As the default sample coordinates will be 0,0 these offsets will be the ones
    // actually choosing which texel to sample. Note the offsets are stored as ivec4 to avoid std140 confusions, but
    // in reality only the first two components are used.
    std::vector<tcu::IVec4> offsets;
    offsets.reserve(pixelCount);

    for (int y = 0; y < fbExtent.y(); ++y)
        for (int x = 0; x < fbExtent.x(); ++x)
            offsets.emplace_back(x, y, 0, 0);

    const auto rndSeed = m_params.getSeed();
    de::Random rnd(rndSeed);
    rnd.shuffle(begin(offsets), end(offsets)); // Shuffle offsets pseudorandomly.

    const auto offsetsBufferSize       = static_cast<VkDeviceSize>(de::dataSize(offsets));
    const auto offsetsBufferCreateInfo = makeBufferCreateInfo(offsetsBufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
    BufferWithMemory offsetsBuffer(ctx.vkd, ctx.device, ctx.allocator, offsetsBufferCreateInfo,
                                   MemoryRequirement::HostVisible);
    {
        auto &alloc = offsetsBuffer.getAllocation();
        deMemcpy(alloc.getHostPtr(), de::dataOrNull(offsets), de::dataSize(offsets));
        flushAlloc(ctx.vkd, ctx.device, alloc);
    }

    // Push constants.
    const float topLeftXCenter = 0.5f / floatExtent.x();
    const float topLeftYCenter = 0.5f / floatExtent.y();

    const PushConstantBlock pcData{
        tcu::Vec4(topLeftXCenter, topLeftYCenter, 1.0f, 1.0f),
        floatExtent.swizzle(0, 1),
        static_cast<float>(lastMipLevel),
    };

    // Descriptor set.
    DescriptorPoolBuilder descPoolBuilder;
    descPoolBuilder.addType(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    descPoolBuilder.addType(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    if (m_params.isCompute())
        descPoolBuilder.addType(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
    const auto descPool =
        descPoolBuilder.build(ctx.vkd, ctx.device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);

    DescriptorSetLayoutBuilder setLayoutBuilder;
    setLayoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, m_params.testStage);
    setLayoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, m_params.testStage);
    if (m_params.isCompute())
        setLayoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, m_params.testStage);
    const auto setLayout     = setLayoutBuilder.build(ctx.vkd, ctx.device);
    const auto descriptorSet = makeDescriptorSet(ctx.vkd, ctx.device, *descPool, *setLayout);

    DescriptorSetUpdateBuilder setUpdateBuilder;
    using Location = DescriptorSetUpdateBuilder::Location;
    {
        const auto bufferInfo = makeDescriptorBufferInfo(*offsetsBuffer, 0u, VK_WHOLE_SIZE);
        setUpdateBuilder.writeSingle(*descriptorSet, Location::binding(0u), VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                     &bufferInfo);
    }
    {
        const auto samplerInfo = makeDescriptorImageInfo(*sampler, *texView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        setUpdateBuilder.writeSingle(*descriptorSet, Location::binding(1u), VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                     &samplerInfo);
    }
    if (m_params.isCompute())
    {
        const auto outputImgInfo = makeDescriptorImageInfo(VK_NULL_HANDLE, fbImg.getImageView(), fbUsageLayout);
        setUpdateBuilder.writeSingle(*descriptorSet, Location::binding(2u), VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                     &outputImgInfo);
    }
    setUpdateBuilder.update(ctx.vkd, ctx.device);

    // Pipeline.
    const auto pcSize         = static_cast<uint32_t>(sizeof(pcData));
    const auto pcRange        = makePushConstantRange(m_params.testStage, 0u, pcSize);
    const auto pipelineLayout = makePipelineLayout(ctx.vkd, ctx.device, *setLayout, &pcRange);

    Move<VkPipeline> pipeline;
    Move<VkShaderModule> compModule;
    Move<VkShaderModule> vertModule;
    Move<VkShaderModule> fragModule;
    Move<VkRenderPass> renderPass;
    Move<VkFramebuffer> framebuffer;

    const auto &binaries = m_context.getBinaryCollection();

    if (m_params.isCompute())
    {
        compModule = createShaderModule(ctx.vkd, ctx.device, binaries.get("comp"));
        pipeline   = makeComputePipeline(ctx.vkd, ctx.device, *pipelineLayout, *compModule);
    }
    else if (m_params.isGraphics())
    {
        vertModule = createShaderModule(ctx.vkd, ctx.device, binaries.get("vert"));
        fragModule = createShaderModule(ctx.vkd, ctx.device, binaries.get("frag"));

        renderPass = makeRenderPass(ctx.vkd, ctx.device, imgFormat);
        framebuffer =
            makeFramebuffer(ctx.vkd, ctx.device, *renderPass, fbImg.getImageView(), apiExtent.width, apiExtent.height);

        const std::vector<VkViewport> viewports(1u, makeViewport(fbExtent));
        const std::vector<VkRect2D> scissors(1u, makeRect2D(fbExtent));

        VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_LAST;

        if (m_params.testStage == VK_SHADER_STAGE_VERTEX_BIT)
            topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        else if (m_params.testStage == VK_SHADER_STAGE_FRAGMENT_BIT)
            topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
        else
            DE_ASSERT(false);

        pipeline =
            makeGraphicsPipeline(ctx.vkd, ctx.device, *pipelineLayout, *vertModule, VK_NULL_HANDLE, VK_NULL_HANDLE,
                                 VK_NULL_HANDLE, *fragModule, *renderPass, viewports, scissors, topology);
    }
    else
        DE_ASSERT(false);

    CommandPoolWithBuffer cmd(ctx.vkd, ctx.device, ctx.qfIndex);
    const auto cmdBuffer = *cmd.cmdBuffer;

    beginCommandBuffer(ctx.vkd, cmdBuffer);

    VkPipelineStageFlags texUsagePipelineStage = 0u;
    switch (m_params.testStage)
    {
    case VK_SHADER_STAGE_COMPUTE_BIT:
        texUsagePipelineStage |= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        break;
    case VK_SHADER_STAGE_VERTEX_BIT:
        texUsagePipelineStage |= VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
        break;
    case VK_SHADER_STAGE_FRAGMENT_BIT:
        texUsagePipelineStage |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        break;
    default:
        DE_ASSERT(false);
        break;
    }

    const tcu::Vec4 clearColor(0.0f, 0.0f, 0.0f, 1.0f); // Different from all texture colors.
    const auto clearColorValue = makeClearValueColor(clearColor);

    if (m_params.isCompute())
    {
        // For graphics, the framebuffer image will be cleared and transitioned using the render pass.
        // However, for compute we need to clear the storage image and move it to the right layout ourselves.
        const auto preClearBarrier =
            makeImageMemoryBarrier(0u, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, fbImg.getImage(), fbSRR);
        cmdPipelineImageMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                      VK_PIPELINE_STAGE_TRANSFER_BIT, &preClearBarrier);

        ctx.vkd.cmdClearColorImage(cmdBuffer, fbImg.getImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                   &clearColorValue.color, 1u, &fbSRR);

        const auto postClearBarrier = makeImageMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_WRITE_BIT,
                                                             VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                                             VK_IMAGE_LAYOUT_GENERAL, fbImg.getImage(), fbSRR);
        cmdPipelineImageMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, texUsagePipelineStage,
                                      &postClearBarrier);
    }

    // Prepare texture: clear the whole image and copy the texture buffer to the appropriate mip level.
    {
        const auto preClearBarrier = makeImageMemoryBarrier(0u, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                                                            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, *texImg, texSRR);
        cmdPipelineImageMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                      VK_PIPELINE_STAGE_TRANSFER_BIT, &preClearBarrier);

        ctx.vkd.cmdClearColorImage(cmdBuffer, *texImg, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearColorValue.color, 1u,
                                   &texSRR);

        const auto writeWaitBarrier = makeImageMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
                                                             VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                                             VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, *texImg, texSRR);
        cmdPipelineImageMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                      VK_PIPELINE_STAGE_TRANSFER_BIT, &writeWaitBarrier);

        const auto copyRegion = makeBufferImageCopy(apiExtent, texSRL);
        ctx.vkd.cmdCopyBufferToImage(cmdBuffer, *texBuffer, *texImg, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1u,
                                     &copyRegion);

        const auto postClearBarrier = makeImageMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                                                             VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                                             VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, *texImg, texSRR);
        cmdPipelineImageMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, texUsagePipelineStage,
                                      &postClearBarrier);
    }

    // Dispatch work.
    if (m_params.isCompute())
    {
        ctx.vkd.cmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayout, 0u, 1u,
                                      &descriptorSet.get(), 0u, nullptr);
        ctx.vkd.cmdPushConstants(cmdBuffer, *pipelineLayout, m_params.testStage, 0u, pcSize, &pcData);
        ctx.vkd.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipeline);
        ctx.vkd.cmdDispatch(cmdBuffer, 1u, 1u, 1u);
    }
    else if (m_params.isGraphics())
    {
        const VkDeviceSize vertexBufferOffset = 0ull;
        beginRenderPass(ctx.vkd, cmdBuffer, *renderPass, *framebuffer, makeRect2D(fbExtent), clearColor);
        ctx.vkd.cmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 0u, 1u,
                                      &descriptorSet.get(), 0u, nullptr);
        ctx.vkd.cmdPushConstants(cmdBuffer, *pipelineLayout, m_params.testStage, 0u, pcSize, &pcData);
        ctx.vkd.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline);
        ctx.vkd.cmdBindVertexBuffers(cmdBuffer, 0u, 1u, &vertexBuffer->get(), &vertexBufferOffset);
        ctx.vkd.cmdDraw(cmdBuffer, de::sizeU32(vertices), 1u, 0u, 0u);
        endRenderPass(ctx.vkd, cmdBuffer);
    }
    else
        DE_ASSERT(false);

    // Copy framebuffer to verification buffer.
    {
        const auto srcAccessMask =
            (m_params.isCompute() ? VK_ACCESS_SHADER_WRITE_BIT : VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
        const auto oldLayout =
            (m_params.isCompute() ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        copyImageToBuffer(ctx.vkd, cmdBuffer, fbImg.getImage(), fbImg.getBuffer(), fbExtent.swizzle(0, 1),
                          srcAccessMask, oldLayout);
    }

    endCommandBuffer(ctx.vkd, cmdBuffer);
    submitCommandsAndWait(ctx.vkd, ctx.device, ctx.queue, cmdBuffer);

    // Create a reference image.
    tcu::TextureLevel referenceLevel(tcuFormat, fbExtent.x(), fbExtent.y(), fbExtent.z());
    auto referenceAccess = referenceLevel.getAccess();

    for (int y = 0; y < fbExtent.y(); ++y)
        for (int x = 0; x < fbExtent.x(); ++x)
        {
            const auto pixelIdx = y * fbExtent.x() + x;
            const auto &offset  = offsets.at(pixelIdx);
            const auto color    = hostTexAccess.getPixel(offset.x(), offset.y());
            referenceAccess.setPixel(color, x, y);
        }

    // Result access.
    auto &fbAlloc = fbImg.getBufferAllocation();
    invalidateAlloc(ctx.vkd, ctx.device, fbAlloc);
    tcu::ConstPixelBufferAccess resultAccess(tcuFormat, fbExtent, fbAlloc.getHostPtr());

    const auto rgbThreshold = 0.005f; // 1/255 < 0.005 < 2/255
    const tcu::Vec4 threshold(rgbThreshold, rgbThreshold, rgbThreshold, 0.0f);

    auto &log = m_context.getTestContext().getLog();
    if (!tcu::floatThresholdCompare(log, "Result", "", referenceAccess, resultAccess, threshold,
                                    tcu::COMPARE_LOG_ON_ERROR))
        TCU_FAIL("Unexpected results in color buffer; check log for details --");

    return tcu::TestStatus::pass("Pass");
}

} // namespace

tcu::TestCaseGroup *createImageNonUniformOffsetSampleTests(tcu::TestContext &testCtx)
{
    using GroupPtr = de::MovePtr<tcu::TestCaseGroup>;

    GroupPtr mainGroup(new tcu::TestCaseGroup(testCtx, "non_uniform_offset_sample"));

    const struct
    {
        VkShaderStageFlagBits stage;
        const char *name;
    } testStages[] = {
        {VK_SHADER_STAGE_VERTEX_BIT, "vert"},
        {VK_SHADER_STAGE_FRAGMENT_BIT, "frag"},
        {VK_SHADER_STAGE_COMPUTE_BIT, "comp"},
    };

    const struct
    {
        TestFunc testFunc;
        const char *prefix;
    } testFuncCases[] = {
        {TestFunc::TEXTURE, "texture"},
        {TestFunc::TEXEL_FETCH, "texel_fetch"},
        {TestFunc::TEXTURE_LOD, "texture_lod"},
        {TestFunc::TEXTURE_PROJ, "texture_proj"},
        {TestFunc::TEXTURE_PROJ_LOD, "texture_proj_lod"},
    };

    for (const auto &testFunc : testFuncCases)
    {
        const auto funcName = testFunc.prefix + std::string("_offset");
        GroupPtr funcGroup(new tcu::TestCaseGroup(testCtx, funcName.c_str()));

        for (const auto multiMip : {false, true})
            for (const auto &testStage : testStages)
            {
                if (multiMip && !hasLodArg(testFunc.testFunc))
                    continue;

                const TestParams params{
                    testStage.stage,
                    testFunc.testFunc,
                    multiMip,
                };

                const auto testName = (multiMip ? "multi_mip_" : "single_mip_") + std::string(testStage.name);
                funcGroup->addChild(new NonUniformOffsetCase(testCtx, testName, params));
            }

        mainGroup->addChild(funcGroup.release());
    }

    return mainGroup.release();
}

} // namespace image
} // namespace vkt
