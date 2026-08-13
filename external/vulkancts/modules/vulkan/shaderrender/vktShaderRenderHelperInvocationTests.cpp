/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2026 The Khronos Group Inc.
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
 * \brief Helper invocation load tests.
 *
 * Verifies that helper invocations remain active for load operations
 * (e.g. texture sampling) by storing a sampled value into an array,
 * loading it back and cross-checking quad neighbours with subgroup quad
 * broadcasts.
 *//*--------------------------------------------------------------------*/

#include "vktShaderRenderHelperInvocationTests.hpp"

#include "vktTestCase.hpp"
#include "subgroups/vktSubgroupsTestsUtils.hpp"

#include "vkBarrierUtil.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkBuilderUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkImageWithMemory.hpp"
#include "vkMemUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkPrograms.hpp"
#include "vkQueryUtil.hpp"
#include "vkRef.hpp"
#include "vkRefUtil.hpp"
#include "vkTypeUtil.hpp"

#include "gluShaderProgram.hpp"

#include "tcuStringTemplate.hpp"
#include "tcuTestLog.hpp"
#include "tcuVector.hpp"

#include "deStringUtil.hpp"
#include "deUniquePtr.hpp"

#include <map>
#include <vector>

namespace vkt
{
namespace sr
{
namespace
{

using namespace vk;

enum
{
    RENDER_SIZE = 16, // Width and height of the render target (and the source texture).
    ARRAY_SIZE  = 32, // Local array length; large enough to defeat small-array optimizations.
};

enum HelperInvocationVariant
{
    VARIANT_BASE = 0,   // Straight-line sample, store and load.
    VARIANT_DIVERGENT,  // Sample inside a quad-uniform but subgroup-divergent branch.
    VARIANT_LOOP,       // Sample inside a quad-uniform data-dependent loop.
    VARIANT_DERIVATIVE, // Explicit-gradient sample driven by dFdx/dFdy.
    VARIANT_FUNCTION,   // Sample and round-trip behind a function call boundary.
};

const VkFormat kTextureFormat = VK_FORMAT_R32G32B32A32_SFLOAT;
const VkFormat kColorFormat   = VK_FORMAT_R8G8B8A8_UNORM;

tcu::Vec4 referenceTexel(int x, int y)
{
    return tcu::Vec4(float(x), float(y), float(x) * 0.5f - float(y) * 0.25f, 1.0f);
}

std::string genLoadedBlock(const std::string &targetVar, const std::string &sampleExpr, int arraySize)
{
    static const tcu::StringTemplate tmpl(R"glsl(    {
        vec4 sampled = ${SAMPLE_EXPR};
        vec4 scratch[${ARRAY_SIZE}];
        for (int i = 0; i < ${ARRAY_SIZE}; ++i)
            scratch[i] = vec4(float(i) - 13.0);
        uint sx = uint(gl_FragCoord.x);
        uint sy = uint(gl_FragCoord.y);
        int storeIdx = int((sx * 7u + sy * 13u + 1u) % uint(${ARRAY_SIZE}));
        scratch[storeIdx] = sampled;
        int loadIdx = 0;
        for (int i = 0; i < ${ARRAY_SIZE}; ++i)
            if (i == storeIdx)
                loadIdx = i;
        ${TARGET_VAR} = scratch[loadIdx];
    }
)glsl");

    std::map<std::string, std::string> params;
    params["SAMPLE_EXPR"] = sampleExpr;
    params["TARGET_VAR"]  = targetVar;
    params["ARRAY_SIZE"]  = de::toString(arraySize);
    return tmpl.specialize(params);
}

std::string genVerificationTail(int renderSize)
{
    static const tcu::StringTemplate laneCheck(R"glsl(    {
        vec4 laneLoaded = subgroupQuadBroadcast(loaded, ${LANE}u);
        vec4 laneCoord = subgroupQuadBroadcast(gl_FragCoord, ${LANE}u);
        helperBits |= subgroupQuadBroadcast(uint(gl_HelperInvocation), ${LANE}u);
        ivec2 lanePix = clamp(ivec2(laneCoord.xy), ivec2(0), ivec2(${MAX_COORD}, ${MAX_COORD}));
        vec4 expected = texelFetch(u_tex, lanePix, 0);
        if (any(greaterThan(abs(laneLoaded - expected), vec4(0.01))))
            ok = false;
    }
)glsl");

    std::string laneChecks;
    for (int lane = 0; lane < 4; ++lane)
    {
        std::map<std::string, std::string> laneParams;
        laneParams["LANE"]      = de::toString(lane);
        laneParams["MAX_COORD"] = de::toString(renderSize - 1);
        laneChecks += laneCheck.specialize(laneParams);
    }

    static const tcu::StringTemplate tmpl(R"glsl(    bool ok = true;
    uint helperBits = 0u;
${LANE_CHECKS}    uint px = uint(gl_FragCoord.x);
    uint py = uint(gl_FragCoord.y);
    uint idx = py * uint(${RENDER_SIZE}) + px;
    if (idx < uint(${PIXEL_COUNT}))
        results[idx] = (ok ? 1u : 0u) | (helperBits != 0u ? 2u : 0u);
    o_color = ok ? vec4(0.0, 1.0, 0.0, 1.0) : vec4(1.0, 0.0, 0.0, 1.0);
)glsl");

    std::map<std::string, std::string> params;
    params["LANE_CHECKS"] = laneChecks;
    params["RENDER_SIZE"] = de::toString(renderSize);
    params["PIXEL_COUNT"] = de::toString(renderSize * renderSize);
    return tmpl.specialize(params);
}

class HelperInvocationLoadInstance : public TestInstance
{
public:
    HelperInvocationLoadInstance(Context &context);
    virtual ~HelperInvocationLoadInstance(void)
    {
    }

    virtual tcu::TestStatus iterate(void);

private:
    tcu::TestStatus verifyResult(const uint32_t *results, uint32_t count) const;
};

HelperInvocationLoadInstance::HelperInvocationLoadInstance(Context &context) : TestInstance(context)
{
}

tcu::TestStatus HelperInvocationLoadInstance::iterate(void)
{
    const DeviceInterface &vk       = m_context.getDeviceInterface();
    const VkDevice device           = m_context.getDevice();
    const VkQueue queue             = m_context.getUniversalQueue();
    const uint32_t queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();
    Allocator &allocator            = m_context.getDefaultAllocator();

    const uint32_t width         = static_cast<uint32_t>(RENDER_SIZE);
    const uint32_t height        = static_cast<uint32_t>(RENDER_SIZE);
    const uint32_t pixelCount    = width * height;
    const VkExtent3D imageExtent = {width, height, 1u};

    const VkImageCreateInfo textureImageInfo = {
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        nullptr,
        (VkImageCreateFlags)0u,
        VK_IMAGE_TYPE_2D,
        kTextureFormat,
        imageExtent,
        1u,
        1u,
        VK_SAMPLE_COUNT_1_BIT,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        VK_SHARING_MODE_EXCLUSIVE,
        0u,
        nullptr,
        VK_IMAGE_LAYOUT_UNDEFINED,
    };
    ImageWithMemory textureImage(vk, device, allocator, textureImageInfo, MemoryRequirement::Any);

    const VkDeviceSize textureSize       = static_cast<VkDeviceSize>(pixelCount) * sizeof(float) * 4u;
    const VkBufferCreateInfo stagingInfo = makeBufferCreateInfo(textureSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    BufferWithMemory stagingBuffer(vk, device, allocator, stagingInfo, MemoryRequirement::HostVisible);
    Allocation &stagingAlloc = stagingBuffer.getAllocation();
    {
        float *const texels = static_cast<float *>(stagingAlloc.getHostPtr());
        for (uint32_t y = 0u; y < height; ++y)
            for (uint32_t x = 0u; x < width; ++x)
            {
                const tcu::Vec4 value = referenceTexel(static_cast<int>(x), static_cast<int>(y));
                float *const dst      = texels + (y * width + x) * 4u;
                dst[0]                = value.x();
                dst[1]                = value.y();
                dst[2]                = value.z();
                dst[3]                = value.w();
            }
        flushAlloc(vk, device, stagingAlloc);
    }

    {
        const VkBufferImageCopy copyRegion = {
            0u,
            0u,
            0u,
            makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u),
            makeOffset3D(0, 0, 0),
            imageExtent,
        };
        const std::vector<VkBufferImageCopy> copyRegions(1u, copyRegion);
        copyBufferToImage(vk, device, queue, queueFamilyIndex, *stagingBuffer, textureSize, copyRegions, nullptr,
                          VK_IMAGE_ASPECT_COLOR_BIT, 1u, 1u, *textureImage);
    }

    const VkImageSubresourceRange textureSubresource =
        makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
    Move<VkImageView> textureView =
        makeImageView(vk, device, *textureImage, VK_IMAGE_VIEW_TYPE_2D, kTextureFormat, textureSubresource);

    Move<VkSampler> sampler;
    {
        const VkSamplerCreateInfo samplerInfo = {
            VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
            nullptr,
            (VkSamplerCreateFlags)0u,
            VK_FILTER_NEAREST,
            VK_FILTER_NEAREST,
            VK_SAMPLER_MIPMAP_MODE_NEAREST,
            VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            0.0f,
            VK_FALSE,
            1.0f,
            VK_FALSE,
            VK_COMPARE_OP_NEVER,
            0.0f,
            0.0f,
            VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK,
            VK_FALSE,
        };
        sampler = createSampler(vk, device, &samplerInfo);
    }

    // Result storage buffer, one entry per pixel, pre-initialized to a sentinel.
    const uint32_t notWritten           = ~0u;
    const VkDeviceSize resultSize       = static_cast<VkDeviceSize>(pixelCount) * sizeof(uint32_t);
    const VkBufferCreateInfo resultInfo = makeBufferCreateInfo(resultSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    BufferWithMemory resultBuffer(vk, device, allocator, resultInfo, MemoryRequirement::HostVisible);
    Allocation &resultAlloc = resultBuffer.getAllocation();
    {
        uint32_t *const ptr = static_cast<uint32_t *>(resultAlloc.getHostPtr());
        for (uint32_t i = 0u; i < pixelCount; ++i)
            ptr[i] = notWritten;
        flushAlloc(vk, device, resultAlloc);
    }

    // Color attachment (its contents are not used for verification).
    const VkImageCreateInfo colorImageInfo = {
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        nullptr,
        (VkImageCreateFlags)0u,
        VK_IMAGE_TYPE_2D,
        kColorFormat,
        imageExtent,
        1u,
        1u,
        VK_SAMPLE_COUNT_1_BIT,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        VK_SHARING_MODE_EXCLUSIVE,
        0u,
        nullptr,
        VK_IMAGE_LAYOUT_UNDEFINED,
    };
    ImageWithMemory colorImage(vk, device, allocator, colorImageInfo, MemoryRequirement::Any);

    const VkImageSubresourceRange colorSubresource =
        makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
    Move<VkImageView> colorView =
        makeImageView(vk, device, *colorImage, VK_IMAGE_VIEW_TYPE_2D, kColorFormat, colorSubresource);

    Move<VkRenderPass> renderPass   = makeRenderPass(vk, device, kColorFormat);
    Move<VkFramebuffer> framebuffer = makeFramebuffer(vk, device, *renderPass, *colorView, width, height);

    // Descriptors: combined image sampler (binding 0) and the result buffer (binding 1).
    Move<VkDescriptorSetLayout> descriptorSetLayout =
        DescriptorSetLayoutBuilder()
            .addSingleBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
            .addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT)
            .build(vk, device);

    Move<VkDescriptorPool> descriptorPool =
        DescriptorPoolBuilder()
            .addType(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
            .addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
            .build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);

    Move<VkDescriptorSet> descriptorSet = makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout);
    {
        const VkDescriptorImageInfo imageInfo =
            makeDescriptorImageInfo(*sampler, *textureView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        const VkDescriptorBufferInfo bufferInfo = makeDescriptorBufferInfo(*resultBuffer, 0u, resultSize);

        DescriptorSetUpdateBuilder()
            .writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u),
                         VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &imageInfo)
            .writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(1u),
                         VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &bufferInfo)
            .update(vk, device);
    }

    Move<VkPipelineLayout> pipelineLayout = makePipelineLayout(vk, device, *descriptorSetLayout);
    Move<VkShaderModule> vertModule = createShaderModule(vk, device, m_context.getBinaryCollection().get("vert"), 0u);
    Move<VkShaderModule> fragModule = createShaderModule(vk, device, m_context.getBinaryCollection().get("frag"), 0u);

    const std::vector<VkViewport> viewports(1u, makeViewport(width, height));
    const std::vector<VkRect2D> scissors(1u, makeRect2D(width, height));

    const VkPipelineVertexInputStateCreateInfo vertexInputState = {
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        nullptr,
        (VkPipelineVertexInputStateCreateFlags)0u,
        0u,
        nullptr,
        0u,
        nullptr,
    };

    Move<VkPipeline> pipeline = makeGraphicsPipeline(
        vk, device, *pipelineLayout, *vertModule, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, *fragModule,
        *renderPass, viewports, scissors, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0u, 0u, &vertexInputState);

    Move<VkCommandPool> commandPool =
        makeCommandPool(vk, device, queueFamilyIndex, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
    Move<VkCommandBuffer> commandBuffer =
        allocateCommandBuffer(vk, device, *commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

    beginCommandBuffer(vk, *commandBuffer);
    beginRenderPass(vk, *commandBuffer, *renderPass, *framebuffer, makeRect2D(width, height),
                    tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f));
    vk.cmdBindPipeline(*commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline);
    vk.cmdBindDescriptorSets(*commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 0u, 1u,
                             &descriptorSet.get(), 0u, nullptr);
    vk.cmdDraw(*commandBuffer, 3u, 1u, 0u, 0u);
    endRenderPass(vk, *commandBuffer);

    {
        const VkBufferMemoryBarrier barrier =
            makeBufferMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT, *resultBuffer, 0u, resultSize);
        vk.cmdPipelineBarrier(*commandBuffer, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                              (VkDependencyFlags)0u, 0u, nullptr, 1u, &barrier, 0u, nullptr);
    }
    endCommandBuffer(vk, *commandBuffer);

    submitCommandsAndWait(vk, device, queue, *commandBuffer);

    invalidateAlloc(vk, device, resultAlloc);
    return verifyResult(static_cast<const uint32_t *>(resultAlloc.getHostPtr()), pixelCount);
}

tcu::TestStatus HelperInvocationLoadInstance::verifyResult(const uint32_t *results, uint32_t count) const
{
    tcu::TestLog &log          = m_context.getTestContext().getLog();
    const uint32_t kNotWritten = ~0u;
    const uint32_t kPassBit    = 0x1u;
    const uint32_t kHelperBit  = 0x2u;

    uint32_t shadedCount = 0u;
    uint32_t failCount   = 0u;
    uint32_t helperCount = 0u;

    for (uint32_t i = 0u; i < count; ++i)
    {
        if (results[i] == kNotWritten)
            continue;

        ++shadedCount;
        if ((results[i] & kPassBit) == 0u)
            ++failCount;
        if ((results[i] & kHelperBit) != 0u)
            ++helperCount;
    }

    log << tcu::TestLog::Message << "Shaded fragments: " << shadedCount << ", failed quad cross-check: " << failCount
        << ", fragments that observed a helper invocation in their quad: " << helperCount << tcu::TestLog::EndMessage;

    if (failCount != 0u)
        return tcu::TestStatus::fail("Helper invocation produced an incorrect loaded value");

    if (shadedCount == 0u)
        return tcu::TestStatus::fail("No fragments were shaded; nothing was verified");

    if (helperCount == 0u)
        return tcu::TestStatus::fail("No helper invocations were generated; the load behaviour was not exercised");

    return tcu::TestStatus::pass("Pass");
}

class HelperInvocationLoadCase : public TestCase
{
public:
    HelperInvocationLoadCase(tcu::TestContext &testCtx, const std::string &name, HelperInvocationVariant variant)
        : TestCase(testCtx, name)
        , m_variant(variant)
    {
    }
    virtual ~HelperInvocationLoadCase(void)
    {
    }

    virtual void initPrograms(SourceCollections &programCollection) const;
    virtual void checkSupport(Context &context) const;
    virtual TestInstance *createInstance(Context &context) const
    {
        return new HelperInvocationLoadInstance(context);
    }

private:
    const HelperInvocationVariant m_variant;
};

void HelperInvocationLoadCase::checkSupport(Context &context) const
{
    if (!subgroups::areQuadOperationsSupportedForStages(context, VK_SHADER_STAGE_FRAGMENT_BIT))
        TCU_THROW(NotSupportedError, "Quad subgroup operations are not supported in the fragment stage");

    if (subgroups::getSubgroupSize(context) < 4u)
        TCU_THROW(NotSupportedError, "Subgroup size of at least 4 is required");
}

void HelperInvocationLoadCase::initPrograms(SourceCollections &programCollection) const
{
    const int renderSize = static_cast<int>(RENDER_SIZE);
    const int arraySize  = static_cast<int>(ARRAY_SIZE);

    {
        static const char *const vertSrc = R"glsl(#version 450
void main (void)
{
    vec2 positions[3] = vec2[](vec2(-1.0, -1.0), vec2(1.0, -1.0), vec2(-1.0, 1.0));
    gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);
}
)glsl";
        programCollection.glslSources.add("vert") << glu::VertexSource(vertSrc);
    }

    {
        std::string roundTripFn;
        if (m_variant == VARIANT_FUNCTION)
        {
            static const tcu::StringTemplate fnTmpl(R"glsl(vec4 roundTrip (vec2 uv)
{
    vec4 loaded;
${BODY}    return loaded;
}
)glsl");
            std::map<std::string, std::string> fnParams;
            fnParams["BODY"] = genLoadedBlock("loaded", "texture(u_tex, uv)", arraySize);
            roundTripFn      = fnTmpl.specialize(fnParams);
        }

        std::string body;
        switch (m_variant)
        {
        case VARIANT_BASE:
            body = "    vec4 loaded;\n" + genLoadedBlock("loaded", "texture(u_tex, uv)", arraySize);
            break;

        case VARIANT_DERIVATIVE:
            // textureGrad with derivative-derived gradients also requires live helper
            // lanes, and verifies the load through an explicit-gradient instruction.
            body = "    vec4 loaded;\n" +
                   genLoadedBlock("loaded", "textureGrad(u_tex, uv, dFdx(uv), dFdy(uv))", arraySize);
            break;

        case VARIANT_DIVERGENT:
            // Branch on the quad id so the four lanes of a quad stay uniform (implicit
            // LOD remains defined) while neighbouring quads diverge.
            body = "    ivec2 quadId = ivec2(gl_FragCoord.xy) / ivec2(2);\n"
                   "    vec4 loaded = vec4(0.0);\n"
                   "    if (((quadId.x + quadId.y) & 1) == 0)\n" +
                   genLoadedBlock("loaded", "texture(u_tex, uv)", arraySize) + "    else\n" +
                   genLoadedBlock("loaded", "texture(u_tex, uv)", arraySize);
            break;

        case VARIANT_LOOP:
            // Quad-uniform, data-dependent trip count keeps the sample inside the loop
            // body without making the loop itself diverge within a quad.
            body = "    ivec2 quadId = ivec2(gl_FragCoord.xy) / ivec2(2);\n"
                   "    int iters = 1 + (quadId.x & 3);\n"
                   "    vec4 loaded = vec4(0.0);\n"
                   "    for (int k = 0; k < iters; ++k)\n" +
                   genLoadedBlock("loaded", "texture(u_tex, uv)", arraySize);
            break;

        case VARIANT_FUNCTION:
            body = "    vec4 loaded = roundTrip(uv);\n";
            break;
        }

        static const tcu::StringTemplate fragTmpl(R"glsl(#version 450
#extension GL_KHR_shader_subgroup_basic : require
#extension GL_KHR_shader_subgroup_quad : require
layout(location = 0) out vec4 o_color;
layout(set = 0, binding = 0) uniform sampler2D u_tex;
layout(set = 0, binding = 1) buffer ResultBlock { uint results[]; };
${ROUND_TRIP_FN}void main (void)
{
    const vec2 fbSize = vec2(${RENDER_SIZE}.0, ${RENDER_SIZE}.0);
    vec2 uv = gl_FragCoord.xy / fbSize;
${BODY}${VERIFY_TAIL}}
)glsl");

        std::map<std::string, std::string> fragParams;
        fragParams["ROUND_TRIP_FN"] = roundTripFn;
        fragParams["RENDER_SIZE"]   = de::toString(renderSize);
        fragParams["BODY"]          = body;
        fragParams["VERIFY_TAIL"]   = genVerificationTail(renderSize);

        const ShaderBuildOptions buildOptions(programCollection.usedVulkanVersion, SPIRV_VERSION_1_3, 0u);
        programCollection.glslSources.add("frag")
            << glu::FragmentSource(fragTmpl.specialize(fragParams)) << buildOptions;
    }
}

} // namespace

tcu::TestCaseGroup *createHelperInvocationTests(tcu::TestContext &testCtx)
{
    de::MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(testCtx, "helper_invocation"));
    group->addChild(new HelperInvocationLoadCase(testCtx, "load_after_store", VARIANT_BASE));
    group->addChild(new HelperInvocationLoadCase(testCtx, "load_after_store_divergent", VARIANT_DIVERGENT));
    group->addChild(new HelperInvocationLoadCase(testCtx, "load_after_store_loop", VARIANT_LOOP));
    group->addChild(new HelperInvocationLoadCase(testCtx, "load_after_store_derivative", VARIANT_DERIVATIVE));
    group->addChild(new HelperInvocationLoadCase(testCtx, "load_after_store_function", VARIANT_FUNCTION));
    return group.release();
}

} // namespace sr
} // namespace vkt
