/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2025 The Khronos Group Inc.
 * Copyright (c) 2025 Valve Corporation.
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
 * \brief Vulkan Reconvergence Tests with Terminate Invocation
 *//*--------------------------------------------------------------------*/

#include "vktReconvergenceTerminateInvocationTests.hpp"
#include "vktTestCase.hpp"

#include "vkImageUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkBarrierUtil.hpp"

#include "tcuTextureUtil.hpp"
#include "tcuImageCompare.hpp"

#include "deUniquePtr.hpp"

#include <sstream>
#include <vector>
#include <limits>

namespace vkt
{
namespace Reconvergence
{

namespace
{

using namespace vk;

using GroupPtr = de::MovePtr<tcu::TestCaseGroup>;

enum class SubCase
{
    BIT_COUNT = 0,
    TERMINATE_HELPERS,
    OOB_READ,
    QUAD_ANY,
};

struct TermInvParams
{
    SubCase subCase;

    uint32_t getDivisor() const
    {
        if (subCase == SubCase::BIT_COUNT || subCase == SubCase::OOB_READ || subCase == SubCase::QUAD_ANY)
            return 2u;

        if (subCase == SubCase::TERMINATE_HELPERS)
            return 0u;

        DE_ASSERT(false);
        return std::numeric_limits<uint32_t>::max();
    }

    tcu::Vec4 getClearColor() const
    {
        return tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f); // Must not match any texture color, typically.
    }

    bool usesHelperInvBuiltIn() const
    {
        return (subCase == SubCase::QUAD_ANY || subCase == SubCase::TERMINATE_HELPERS);
    }
};

class TermInvInstance : public vkt::TestInstance
{
public:
    TermInvInstance(Context &context, const TermInvParams &params) : vkt::TestInstance(context), m_params(params)
    {
    }
    virtual ~TermInvInstance(void) = default;

    tcu::TestStatus iterate(void) override;

    virtual void checkResult(const tcu::ConstPixelBufferAccess &texture, const tcu::ConstPixelBufferAccess &framebuffer)
    {
        auto &log = m_context.getTestContext().getLog();
        log << tcu::TestLog::Image("Texture", "", texture) << tcu::TestLog::Image("Framebuffer", "", framebuffer);
        TCU_THROW(InternalError, "Proper check not implemented");
    }

protected:
    const TermInvParams m_params;
};

class TermInvBitCountInstance : public TermInvInstance
{
public:
    TermInvBitCountInstance(Context &context, const TermInvParams &params) : TermInvInstance(context, params)
    {
    }
    virtual ~TermInvBitCountInstance(void) = default;

    void checkResult(const tcu::ConstPixelBufferAccess &texture,
                     const tcu::ConstPixelBufferAccess &framebuffer) override;
};

class TermInvHelpersInstance : public TermInvInstance
{
public:
    TermInvHelpersInstance(Context &context, const TermInvParams &params) : TermInvInstance(context, params)
    {
    }
    virtual ~TermInvHelpersInstance(void) = default;

    void checkResult(const tcu::ConstPixelBufferAccess &texture,
                     const tcu::ConstPixelBufferAccess &framebuffer) override;
};

class TermInvOOBReadInstance : public TermInvBitCountInstance // Because they share the checkResult routine.
{
public:
    TermInvOOBReadInstance(Context &context, const TermInvParams &params) : TermInvBitCountInstance(context, params)
    {
    }
    virtual ~TermInvOOBReadInstance(void) = default;
};

class TermInvQuadAllInstance : public TermInvInstance
{
public:
    TermInvQuadAllInstance(Context &context, const TermInvParams &params) : TermInvInstance(context, params)
    {
    }
    virtual ~TermInvQuadAllInstance(void) = default;

    void checkResult(const tcu::ConstPixelBufferAccess &texture,
                     const tcu::ConstPixelBufferAccess &framebuffer) override;
};

class TermInvCase : public vkt::TestCase
{
public:
    TermInvCase(tcu::TestContext &testCtx, const std::string &name, const TermInvParams &params)
        : vkt::TestCase(testCtx, name)
        , m_params(params)
    {
    }
    virtual ~TermInvCase(void) = default;

    bool needsMaximalReconvergence() const
    {
        return ((m_params.subCase == SubCase::BIT_COUNT) || (m_params.subCase == SubCase::TERMINATE_HELPERS));
    }

    void checkSupport(Context &context) const override;
    void initPrograms(vk::SourceCollections &programCollection) const override;
    TestInstance *createInstance(Context &context) const override
    {
        if (m_params.subCase == SubCase::BIT_COUNT)
            return new TermInvBitCountInstance(context, m_params);

        if (m_params.subCase == SubCase::TERMINATE_HELPERS)
            return new TermInvHelpersInstance(context, m_params);

        if (m_params.subCase == SubCase::OOB_READ)
            return new TermInvOOBReadInstance(context, m_params);

        if (m_params.subCase == SubCase::QUAD_ANY)
            return new TermInvQuadAllInstance(context, m_params);

        DE_ASSERT(false);
        return nullptr;
        ;
    }

protected:
    const TermInvParams m_params;
};

void TermInvCase::checkSupport(Context &context) const
{
    if (needsMaximalReconvergence())
        context.requireDeviceFunctionality("VK_KHR_shader_maximal_reconvergence");

    const auto minVersion = (m_params.usesHelperInvBuiltIn() ? VK_API_VERSION_1_3 : VK_API_VERSION_1_1);
    if (context.getUsedApiVersion() < minVersion)
        TCU_THROW(NotSupportedError, "Minimum Vulkan version requirement not met");

    const auto &subgroupProperties = context.getSubgroupProperties();

    if (!(subgroupProperties.supportedStages & VK_SHADER_STAGE_FRAGMENT_BIT))
        TCU_THROW(NotSupportedError, "Subgroup support in the fragment shader required");

    if (!(subgroupProperties.supportedOperations & VK_SUBGROUP_FEATURE_BASIC_BIT))
        TCU_THROW(NotSupportedError, "VK_SUBGROUP_FEATURE_BASIC_BIT required");

    if (m_params.subCase == SubCase::BIT_COUNT)
    {
        if (!(subgroupProperties.supportedOperations & VK_SUBGROUP_FEATURE_BALLOT_BIT))
            TCU_THROW(NotSupportedError, "VK_SUBGROUP_FEATURE_BALLOT_BIT required");
    }

    if (m_params.subCase == SubCase::TERMINATE_HELPERS || m_params.subCase == SubCase::QUAD_ANY)
    {
        if (!(subgroupProperties.supportedOperations & VK_SUBGROUP_FEATURE_VOTE_BIT))
            TCU_THROW(NotSupportedError, "VK_SUBGROUP_FEATURE_VOTE_BIT required");
    }

    context.requireDeviceFunctionality("VK_KHR_shader_quad_control");
}

void TermInvCase::initPrograms(vk::SourceCollections &programCollection) const
{
    // Full-screen triangle that saves us from having to create a vertex buffer.
    std::ostringstream vert;
    vert << "#version 460\n"
         << "const vec4 vertices[] = vec4[](\n"
         << "    vec4(-1.0, -1.0, 0.0, 1.0),\n"
         << "    vec4(-1.0,  3.0, 0.0, 1.0),\n"
         << "    vec4( 3.0, -1.0, 0.0, 1.0)\n"
         << ");\n"
         << "void main (void) {\n"
         << "    gl_Position = vertices[gl_VertexIndex % 3];\n"
         << "}\n";
    programCollection.glslSources.add("vert") << glu::VertexSource(vert.str());

    const auto spirvVersion = (m_params.usesHelperInvBuiltIn() ? SPIRV_VERSION_1_6 : SPIRV_VERSION_1_3);
    ShaderBuildOptions buildOptions(programCollection.usedVulkanVersion, spirvVersion, 0u);

    // The initial part is common for all shaders.
    std::ostringstream frag;
    frag << "#version 460\n"
         << "#extension GL_KHR_shader_subgroup_ballot : enable\n"
         << "#extension GL_KHR_shader_subgroup_vote : enable\n"
         << "#extension GL_EXT_terminate_invocation : enable\n"
         << (needsMaximalReconvergence() ? "#extension GL_EXT_maximal_reconvergence : enable\n" : "")
         << "#extension GL_EXT_shader_quad_control : enable\n"
         << "\n"
         << "layout (full_quads) in;\n"
         << "\n"
         << "layout (location=0) out vec4 outColor;\n"
         << "layout (set=0, binding=0) uniform sampler2D inTexture;\n"
         << "layout (set=0, binding=1, std430) readonly buffer InValuesBlock {\n"
         << "    vec4 values[];\n"
         << "} inValues;\n"
         << "\n"
         << "layout (push_constant, std430) uniform PCBlock {\n"
         << "    uint divisor;\n"
         << "    uint divisorCopy;\n"
         << "    uint indexZero;\n"
         << "    uint indexLarge;\n"
         << "    float width;\n"
         << "    float height;\n"
         << "} pc;\n"
         << "\n"
         << "void main()\n"
         << (needsMaximalReconvergence() ? "[[maximally_reconverges]]\n" : "") << "{\n";

    // Main body.
    if (m_params.subCase == SubCase::BIT_COUNT)
    {
        frag << "    // The texture should only have non-zero variable red values and alpha 1.0.\n"
             << "    vec2 dim = vec2(pc.width, pc.height);\n"
             << "    vec2 sampleCoords = gl_FragCoord.xy / dim;\n"
             << "    vec4 inColor = texture(inTexture, sampleCoords);\n"
             << "    \n"
             << "    bool should_terminate = (gl_SubgroupInvocationID % pc.divisor == 0u);\n"
             << "    bool should_terminate_2 = (gl_SubgroupInvocationID % pc.divisorCopy == 0u);\n"
             << "    \n"
             << "    uvec4 all_ballot = subgroupBallot(true);\n"
             << "    uint all_count = subgroupBallotBitCount(all_ballot);\n"
             << "    \n"
             << "    uvec4 terminated_ballot = subgroupBallot(should_terminate);\n"
             << "    uint terminated_count = subgroupBallotBitCount(terminated_ballot);\n"
             << "    \n"
             << "    // Separate condition to prevent the compiler from being too smart.\n"
             << "    if (should_terminate_2)\n"
             << "        terminateInvocation;\n"
             << "    \n"
             << "    uvec4 alive_ballot = subgroupBallot(true);\n"
             << "    uint alive_count = subgroupBallotBitCount(alive_ballot);\n"
             << "    \n"
             << "    bool success = (terminated_count + alive_count == all_count); \n"
             << "    if (success)\n"
             << "        inColor.b = 1.0;\n"
             << "    \n"
             << "    // Output framebuffer:\n"
             << "    // * Half the pixels should be (textureRed, 0.0, 1.0, 1.0).\n"
             << "    // * The other half should have the clear color.\n"
             << "    outColor = inColor;\n";
    }
    else if (m_params.subCase == SubCase::TERMINATE_HELPERS)
    {
        frag << "    // The texture should only have non-zero variable red values and alpha 1.0.\n"
             << "    vec2 dim = vec2(pc.width, pc.height);\n"
             << "    vec2 sampleCoords = gl_FragCoord.xy / dim;\n"
             << "    vec4 inColor = texture(inTexture, sampleCoords);\n"
             << "    \n"
             << "    // Divisor and divisor copy are all both 0.\n"
             << "    bool should_terminate = (gl_HelperInvocation && pc.divisor == 0u); \n"
             << "    bool should_terminate_2 = (gl_HelperInvocation && pc.divisorCopy == 0u);\n"
             << "    \n"
             << "    // Separate condition to prevent the compiler from being too smart.\n"
             << "    if (should_terminate_2)\n"
             << "        terminateInvocation;\n"
             << "    \n"
             << "    bool success = (subgroupAny(should_terminate) == false); \n"
             << "    if (success)\n"
             << "        inColor.b = 1.0;\n"
             << "    \n"
             << "    // Output framebuffer:\n"
             << "    // * All terminated invocations would be helpers.\n"
             << "    // * All pixels should be (textureRed, 0.0, 1.0, 1.0).\n"
             << "    outColor = inColor;\n";
    }
    else if (m_params.subCase == SubCase::OOB_READ)
    {
        frag << "    // The texture should only have non-zero variable red values and alpha 1.0.\n"
             << "    vec2 dim = vec2(pc.width, pc.height);\n"
             << "    vec2 sampleCoords = gl_FragCoord.xy / dim;\n"
             << "    vec4 inColor = texture(inTexture, sampleCoords);\n"
             << "    \n"
             << "    bool should_terminate = (gl_SubgroupInvocationID % pc.divisor == 0u);\n"
             << "    bool should_terminate_2 = (gl_SubgroupInvocationID % pc.divisorCopy == 0u);\n"
             << "\n"
             << "    // Separate condition to prevent the compiler from being too smart.\n"
             << "    if (should_terminate_2)\n"
             << "        terminateInvocation;\n"
             << "\n"
             << "    // Attempt to trigger an invalid read. The buffer will contain (0, 0, 1, 0) at index zero.\n"
             << "    uint index = (should_terminate ? pc.indexLarge : pc.indexZero);\n"
             << "    inColor = inColor + inValues.values[index];\n"
             << "\n"
             << "    // Output framebuffer:\n"
             << "    // * Half the pixels should be (textureRed, 0.0, 1.0, 1.0).\n"
             << "    // * The other half should have the clear color.\n"
             << "    // * No crashes, obviously.\n"
             << "    outColor = inColor;\n";
    }
    else if (m_params.subCase == SubCase::QUAD_ANY)
    {
        frag << "    vec4 inColor = vec4(0.0, 0.0, 1.0, 1.0);\n"
             << "    \n"
             << "    bool should_terminate = (gl_SubgroupInvocationID % pc.divisor == 0u);\n"
             << "\n"
             << "    if (should_terminate)\n"
             << "        terminateInvocation;\n"
             << "\n"
             << "    if (subgroupQuadAny(gl_HelperInvocation)) {\n"
             << "      // This should always be false if terminateInvocation is implemented properly.\n"
             << "      vec2 dim = vec2(pc.width, pc.height);\n"
             << "      vec2 sampleCoords = gl_FragCoord.xy / dim;\n"
             << "      inColor = texture(inTexture, sampleCoords);\n"
             << "    }\n"
             << "    // Output framebuffer:\n"
             << "    // * Half the pixels should be (0.0, 0.0, 1.0, 1.0).\n"
             << "    // * The other half should have the clear color.\n"
             << "    outColor = inColor;\n";
    }
    else
        DE_ASSERT(false);

    frag << "}\n";

    programCollection.glslSources.add("frag") << glu::FragmentSource(frag.str()) << buildOptions;
}

tcu::TestStatus TermInvInstance::iterate(void)
{
    const auto ctx = m_context.getContextCommonData();
    const tcu::IVec3 fbExtent(32, 32, 1);
    const auto extent     = makeExtent3D(fbExtent);
    const auto format     = VK_FORMAT_R8G8B8A8_UNORM;
    const auto tcuFormat  = mapVkFormat(format);
    const auto texUsage   = (VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
    const auto fbUsage    = (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    const auto descStages = VK_SHADER_STAGE_FRAGMENT_BIT;
    const auto colorSRR   = makeDefaultImageSubresourceRange();
    const auto colorSRL   = makeDefaultImageSubresourceLayers();
    const auto bindPoint  = VK_PIPELINE_BIND_POINT_GRAPHICS;
    const auto clearColor = m_params.getClearColor();

    // Texture.
    const VkImageCreateInfo texImgCreateInfo{
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        nullptr,
        0u,
        VK_IMAGE_TYPE_2D,
        format,
        extent,
        1u,
        1u,
        VK_SAMPLE_COUNT_1_BIT,
        VK_IMAGE_TILING_OPTIMAL,
        texUsage,
        VK_SHARING_MODE_EXCLUSIVE,
        0u,
        nullptr,
        VK_IMAGE_LAYOUT_UNDEFINED,
    };
    ImageWithMemory texImg(ctx.vkd, ctx.device, ctx.allocator, texImgCreateInfo, MemoryRequirement::Any);
    const auto texImgView = makeImageView(ctx.vkd, ctx.device, *texImg, VK_IMAGE_VIEW_TYPE_2D, format, colorSRR);

    const auto texBufferSize =
        static_cast<VkDeviceSize>(tcu::getPixelSize(tcuFormat) * fbExtent.x() * fbExtent.y() * fbExtent.z());
    const auto texBufferCreateInfo = makeBufferCreateInfo(texBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    BufferWithMemory texBuffer(ctx.vkd, ctx.device, ctx.allocator, texBufferCreateInfo, MemoryRequirement::HostVisible);
    auto &texBufferAlloc = texBuffer.getAllocation();
    tcu::PixelBufferAccess texAccess(tcuFormat, fbExtent, texBufferAlloc.getHostPtr());
    {
        const tcu::Vec4 minColor(0.004f, 0.0f, 0.0f, 1.0f);
        const tcu::Vec4 maxColor(1.000f, 0.0f, 0.0f, 1.0f);
        tcu::fillWithComponentGradients(texAccess, minColor, maxColor);
        flushAlloc(ctx.vkd, ctx.device, texBufferAlloc);
    }

    // Framebuffer.
    ImageWithBuffer fbImg(ctx.vkd, ctx.device, ctx.allocator, extent, format, fbUsage, VK_IMAGE_TYPE_2D);

    // Sampler.
    const VkSamplerCreateInfo samplerCreateInfo{
        VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        nullptr,
        0u,
        VK_FILTER_LINEAR,
        VK_FILTER_LINEAR,
        VK_SAMPLER_MIPMAP_MODE_LINEAR,
        VK_SAMPLER_ADDRESS_MODE_REPEAT,
        VK_SAMPLER_ADDRESS_MODE_REPEAT,
        VK_SAMPLER_ADDRESS_MODE_REPEAT,
        0.0f,
        VK_FALSE,
        1.0f,
        VK_FALSE,
        VK_COMPARE_OP_NEVER,
        0.0f,
        1.0f,
        VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
        VK_FALSE,
    };
    const auto sampler = createSampler(ctx.vkd, ctx.device, &samplerCreateInfo);

    // Storage buffer, with just a single value.
    const auto valuesBufferSize  = static_cast<VkDeviceSize>(sizeof(tcu::Vec4));
    const auto valuesBufferUsage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    const auto valuesBufferInfo  = makeBufferCreateInfo(valuesBufferSize, valuesBufferUsage);
    BufferWithMemory valuesBuffer(ctx.vkd, ctx.device, ctx.allocator, valuesBufferInfo, MemoryRequirement::HostVisible);
    {
        auto &alloc = valuesBuffer.getAllocation();
        const tcu::Vec4 blueOne(0.0f, 0.0f, 1.0f, 0.0f);
        memcpy(alloc.getHostPtr(), &blueOne, sizeof(blueOne));
        flushAlloc(ctx.vkd, ctx.device, alloc);
    }

    struct PushConstants
    {
        uint32_t divisor;
        uint32_t divisorCopy;
        uint32_t indexZero;
        uint32_t indexLarge;
        float width;
        float height;
    };

    DescriptorSetLayoutBuilder setLayoutBuilder;
    setLayoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, descStages);
    setLayoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, descStages);
    const auto setLayout = setLayoutBuilder.build(ctx.vkd, ctx.device);

    const auto pcSize   = DE_SIZEOF32(PushConstants);
    const auto pcStages = descStages;
    const auto pcRange  = makePushConstantRange(pcStages, 0u, pcSize);

    const auto pipelineLayout = makePipelineLayout(ctx.vkd, ctx.device, *setLayout, &pcRange);

    // Create and prepare descriptors.
    DescriptorPoolBuilder descPoolBuilder;
    descPoolBuilder.addType(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1u);
    descPoolBuilder.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1u);
    const auto descriptorPool =
        descPoolBuilder.build(ctx.vkd, ctx.device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);
    const auto descriptorSet = makeDescriptorSet(ctx.vkd, ctx.device, *descriptorPool, *setLayout);

    const auto binding = DescriptorSetUpdateBuilder::Location::binding;
    DescriptorSetUpdateBuilder setUpdateBuilder;
    const auto sampledImageDescInfo =
        makeDescriptorImageInfo(*sampler, *texImgView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    const auto storageBufferDescInfo = makeDescriptorBufferInfo(*valuesBuffer, 0u, VK_WHOLE_SIZE);
    setUpdateBuilder.writeSingle(*descriptorSet, binding(0u), VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                 &sampledImageDescInfo);
    setUpdateBuilder.writeSingle(*descriptorSet, binding(1u), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                 &storageBufferDescInfo);
    setUpdateBuilder.update(ctx.vkd, ctx.device);

    // Pipeline.
    const auto renderPass = makeRenderPass(ctx.vkd, ctx.device, format);
    const auto framebuffer =
        makeFramebuffer(ctx.vkd, ctx.device, *renderPass, fbImg.getImageView(), extent.width, extent.height);

    const std::vector<VkViewport> viewports(1u, makeViewport(fbExtent));
    const std::vector<VkRect2D> scissors(1u, makeRect2D(fbExtent));

    const VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo = initVulkanStructure();

    const auto &binaries  = m_context.getBinaryCollection();
    const auto vertShader = createShaderModule(ctx.vkd, ctx.device, binaries.get("vert"));
    const auto fragShader = createShaderModule(ctx.vkd, ctx.device, binaries.get("frag"));
    const auto pipeline   = makeGraphicsPipeline(
        ctx.vkd, ctx.device, *pipelineLayout, *vertShader, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, *fragShader,
        *renderPass, viewports, scissors, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0u, 0u, &vertexInputStateCreateInfo);

    CommandPoolWithBuffer cmd(ctx.vkd, ctx.device, ctx.qfIndex);
    const auto cmdBuffer = *cmd.cmdBuffer;

    beginCommandBuffer(ctx.vkd, cmdBuffer);
    {
        // Prepare texture.
        const auto preCopyBarrier = makeImageMemoryBarrier(0u, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                                                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, *texImg, colorSRR);
        cmdPipelineImageMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                      VK_PIPELINE_STAGE_TRANSFER_BIT, &preCopyBarrier);
        const auto copyRegion = makeBufferImageCopy(extent, colorSRL);
        ctx.vkd.cmdCopyBufferToImage(cmdBuffer, *texBuffer, *texImg, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1u,
                                     &copyRegion);
        const auto postCopyBarrier = makeImageMemoryBarrier(
            VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, *texImg, colorSRR);
        cmdPipelineImageMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                      VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, &postCopyBarrier);
    }
    beginRenderPass(ctx.vkd, cmdBuffer, *renderPass, *framebuffer, scissors.at(0u), clearColor);
    ctx.vkd.cmdBindPipeline(cmdBuffer, bindPoint, *pipeline);
    ctx.vkd.cmdBindDescriptorSets(cmdBuffer, bindPoint, *pipelineLayout, 0u, 1u, &descriptorSet.get(), 0u, nullptr);
    {
        const PushConstants pcValue{
            m_params.getDivisor(),  m_params.getDivisor(),   0u, std::numeric_limits<uint32_t>::max(),
            viewports.at(0u).width, viewports.at(0u).height,
        };
        ctx.vkd.cmdPushConstants(cmdBuffer, *pipelineLayout, pcStages, 0u, pcSize, &pcValue);
    }
    ctx.vkd.cmdDraw(cmdBuffer, 3u, 1u, 0u, 0u);
    endRenderPass(ctx.vkd, cmdBuffer);
    copyImageToBuffer(ctx.vkd, cmdBuffer, fbImg.getImage(), fbImg.getBuffer(), fbExtent.swizzle(0, 1));
    endCommandBuffer(ctx.vkd, cmdBuffer);
    submitCommandsAndWait(ctx.vkd, ctx.device, ctx.queue, cmdBuffer);

    invalidateAlloc(ctx.vkd, ctx.device, fbImg.getBufferAllocation());
    tcu::ConstPixelBufferAccess result(tcuFormat, fbExtent, fbImg.getBufferAllocation().getHostPtr());

    // Result checking will vary by type of test. See below.
    checkResult(texAccess, result);

    return tcu::TestStatus::pass("Pass");
}

void TermInvBitCountInstance::checkResult(const tcu::ConstPixelBufferAccess &texture,
                                          const tcu::ConstPixelBufferAccess &framebuffer)
{
    const auto extent = framebuffer.getSize();
    tcu::TextureLevel refLevel(framebuffer.getFormat(), extent.x(), extent.y(), extent.z());
    tcu::PixelBufferAccess reference = refLevel.getAccess();
    const auto clearColor            = m_params.getClearColor();
    const int divisor                = static_cast<int>(m_params.getDivisor());

    for (int y = 0; y < extent.y(); ++y)
        for (int x = 0; x < extent.x(); ++x)
        {
            // Set blue to 1 like in the frag shader.
            auto modifiedPixel = texture.getPixel(x, y);
            modifiedPixel.z()  = 1.0f;

            const bool terminated = (x % divisor == 0);
            reference.setPixel((terminated ? clearColor : modifiedPixel), x, y);
        }

    // Allow some imprecission in the red component due to sampling.
    const tcu::Vec4 threshold(0.005f, 0.0f, 0.0f, 0.0f); // 1/255 < 0.005 < 2/255

    auto &log = m_context.getTestContext().getLog();
    if (!tcu::floatThresholdCompare(log, "Result", "", reference, framebuffer, threshold, tcu::COMPARE_LOG_ON_ERROR))
        TCU_FAIL("Unexpected results found in color buffer; check log for details --");
}

void TermInvHelpersInstance::checkResult(const tcu::ConstPixelBufferAccess &texture,
                                         const tcu::ConstPixelBufferAccess &framebuffer)
{
    const auto extent = framebuffer.getSize();
    tcu::TextureLevel refLevel(framebuffer.getFormat(), extent.x(), extent.y(), extent.z());
    tcu::PixelBufferAccess reference = refLevel.getAccess();

    for (int y = 0; y < extent.y(); ++y)
        for (int x = 0; x < extent.x(); ++x)
        {
            // Set blue to 1 like in the frag shader.
            auto modifiedPixel = texture.getPixel(x, y);
            modifiedPixel.z()  = 1.0f;
            reference.setPixel(modifiedPixel, x, y);
        }

    // Allow some imprecission in the red component due to sampling.
    const tcu::Vec4 threshold(0.005f, 0.0f, 0.0f, 0.0f); // 1/255 < 0.005 < 2/255

    auto &log = m_context.getTestContext().getLog();
    if (!tcu::floatThresholdCompare(log, "Result", "", reference, framebuffer, threshold, tcu::COMPARE_LOG_ON_ERROR))
        TCU_FAIL("Unexpected results found in color buffer; check log for details --");
}

void TermInvQuadAllInstance::checkResult(const tcu::ConstPixelBufferAccess &,
                                         const tcu::ConstPixelBufferAccess &framebuffer)
{
    const auto extent = framebuffer.getSize();
    tcu::TextureLevel refLevel(framebuffer.getFormat(), extent.x(), extent.y(), extent.z());
    tcu::PixelBufferAccess reference = refLevel.getAccess();
    const auto clearColor            = m_params.getClearColor();
    const auto geomColor             = tcu::Vec4(0.0f, 0.0f, 1.0f, 1.0f); // Must match the frag shader.
    const int divisor                = static_cast<int>(m_params.getDivisor());

    for (int y = 0; y < extent.y(); ++y)
        for (int x = 0; x < extent.x(); ++x)
        {
            const bool terminated = (x % divisor == 0);
            reference.setPixel((terminated ? clearColor : geomColor), x, y);
        }

    // Fixed colors with 0 and 1 should be exact.
    const tcu::Vec4 threshold(0.0f);

    auto &log = m_context.getTestContext().getLog();
    if (!tcu::floatThresholdCompare(log, "Result", "", reference, framebuffer, threshold, tcu::COMPARE_LOG_ON_ERROR))
        TCU_FAIL("Unexpected results found in color buffer; check log for details --");
}

} // anonymous namespace

tcu::TestCaseGroup *createTerminateInvocationTests(tcu::TestContext &testCtx)
{
    GroupPtr mainGroup(new tcu::TestCaseGroup(testCtx, "terminate_invocation"));

    {
        const TermInvParams params{SubCase::BIT_COUNT};
        mainGroup->addChild(new TermInvCase(testCtx, "bit_count", params));
    }
    {
        const TermInvParams params{SubCase::TERMINATE_HELPERS};
        mainGroup->addChild(new TermInvCase(testCtx, "terminate_helpers", params));
    }
    {
        const TermInvParams params{SubCase::OOB_READ};
        mainGroup->addChild(new TermInvCase(testCtx, "oob_read", params));
    }
    {
        const TermInvParams params{SubCase::QUAD_ANY};
        mainGroup->addChild(new TermInvCase(testCtx, "quad_any", params));
    }

    return mainGroup.release();
}

} // namespace Reconvergence
} // namespace vkt
