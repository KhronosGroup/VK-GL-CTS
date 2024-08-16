/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2023 The Khronos Group Inc.
 * Copyright (c) 2023 Valve Corporation.
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
 * \brief Vulkan Fragment Shader Invocation and Sample Cound Tests
 *//*--------------------------------------------------------------------*/
#include "vktQueryPoolFragInvocationTests.hpp"
#include "tcuImageCompare.hpp"
#include "vkBarrierUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vktTestCaseUtil.hpp"
#include "vkBuilderUtil.hpp"

#include <sstream>

namespace vkt
{
namespace QueryPool
{

namespace
{

using namespace vk;

enum class QueryType
{
    INVOCATIONS = 0,
    OCCLUSION
};

std::string getQueryTypeName(const QueryType qType)
{
    switch (qType)
    {
    case QueryType::INVOCATIONS:
        return "frag_invs";
    case QueryType::OCCLUSION:
        return "occlusion";
    default:
        break;
    }

    DE_ASSERT(false);
    return "";
}

enum class FragShaderVariant
{
    FLAT           = 0,
    VERTEX_COLOR   = 1,
    ATOMIC_COUNTER = 2,
};

struct TestParams
{
    const QueryType queryType;
    const bool secondary;
    const FragShaderVariant fragShaderVariant;
};

tcu::Vec4 getGeometryColor(void)
{
    return tcu::Vec4(0.0f, 0.0f, 1.0f, 1.0f);
}

tcu::Vec4 getClearColor(void)
{
    return tcu::Vec4(0.0f, 0.0f, 0.0f, 0.0f);
}

void checkSupport(Context &context, TestParams params)
{
    if (params.secondary)
        context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_INHERITED_QUERIES);

    if (params.queryType == QueryType::OCCLUSION)
        context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_OCCLUSION_QUERY_PRECISE);
    else if (params.queryType == QueryType::INVOCATIONS)
        context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_PIPELINE_STATISTICS_QUERY);

    if (params.fragShaderVariant == FragShaderVariant::ATOMIC_COUNTER)
        context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_FRAGMENT_STORES_AND_ATOMICS);
}

void initPrograms(vk::SourceCollections &programCollection, TestParams params)
{
    const bool isAtomic      = (params.fragShaderVariant == FragShaderVariant::ATOMIC_COUNTER);
    const bool isVertexColor = (params.fragShaderVariant == FragShaderVariant::VERTEX_COLOR);

    std::ostringstream vert;
    vert << "#version 460\n"
         << "layout (location=0) in vec4 inPos;\n"
         << (isVertexColor ? "layout (location=1) in vec4 inColor;\n" : "")
         << (isVertexColor ? "layout (location=0) out vec4 outColor;\n" : "") << "void main() {\n"
         << "    gl_Position = inPos;\n"
         << (isVertexColor ? "    outColor = inColor;\n" : "") << "}";
    programCollection.glslSources.add("vert") << glu::VertexSource(vert.str());

    std::ostringstream frag;
    frag << "#version 460\n"
         << "layout (location=0) out vec4 outColor;\n"
         << (isVertexColor ? "layout (location=0) in vec4 vtxColor;\n" : "")
         << (isAtomic ? "layout (set=0, binding=0) buffer CounterBlock { uint counter; } cb;\n" : "")
         << "void main() {\n"
         << (isAtomic ? "    atomicAdd(cb.counter, 1u);\n" : "")
         << "    outColor = " << (isVertexColor ? "vtxColor" : ("vec4" + de::toString(getGeometryColor()))) << ";\n"
         << "}";
    ;
    programCollection.glslSources.add("frag") << glu::FragmentSource(frag.str());
}

// Records the render pass commands (bind pipeline and draw) and optionally binds descriptor sets.
void recordRenderPassCommands(const DeviceInterface &vkd, const VkCommandBuffer cmdBuffer,
                              const VkPipelineBindPoint bindPoint, const VkPipeline pipeline,
                              const VkPipelineLayout pipelineLayout,
                              const VkDescriptorSet descriptorSet, // Can be made VK_NULL_HANDLE to avoid binding.
                              const VkBuffer vertexBuffer, const VkDeviceSize vertexBufferOffset)
{
    vkd.cmdBindPipeline(cmdBuffer, bindPoint, pipeline);

    if (descriptorSet != VK_NULL_HANDLE)
        vkd.cmdBindDescriptorSets(cmdBuffer, bindPoint, pipelineLayout, 0u, 1u, &descriptorSet, 0u, nullptr);

    vkd.cmdBindVertexBuffers(cmdBuffer, 0u, 1u, &vertexBuffer, &vertexBufferOffset);
    vkd.cmdDraw(cmdBuffer, 3u, 1u, 0u, 0u);
}

tcu::TestStatus testInvocations(Context &context, const TestParams params)
{
    const auto ctx = context.getContextCommonData();
    const tcu::IVec3 fbExtent(64, 64, 1);
    const auto vkExtent        = makeExtent3D(fbExtent);
    const auto colorFormat     = VK_FORMAT_R8G8B8A8_UNORM;
    const auto colorSRR        = makeDefaultImageSubresourceRange();
    const auto colorSRL        = makeDefaultImageSubresourceLayers();
    const auto colorUsage      = (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    const auto imageType       = VK_IMAGE_TYPE_2D;
    const auto bindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
    const auto isAtomic        = (params.fragShaderVariant == FragShaderVariant::ATOMIC_COUNTER);
    const auto resShaderStages = VK_SHADER_STAGE_FRAGMENT_BIT; // Shader stages for the resources.
    const auto geometryColor   = getGeometryColor();

    ImageWithBuffer colorBuffer(ctx.vkd, ctx.device, ctx.allocator, vkExtent, colorFormat, colorUsage, imageType,
                                colorSRR);

    const auto &binaries  = context.getBinaryCollection();
    const auto vertModule = createShaderModule(ctx.vkd, ctx.device, binaries.get("vert"));
    const auto fragModule = createShaderModule(ctx.vkd, ctx.device, binaries.get("frag"));

    // Vertex buffer, which is going to contain interleaved pairs of positions and colors.
    const std::vector<tcu::Vec4> vertices = {
        // position                                // color
        tcu::Vec4(-1.0f, -1.0f, 0.0f, 1.0f), geometryColor, tcu::Vec4(3.0f, -1.0f, 0.0f, 1.0f), geometryColor,
        tcu::Vec4(-1.0f, 3.0f, 0.0f, 1.0f),  geometryColor,
    };

    const uint32_t kVec4Sz        = static_cast<uint32_t>(sizeof(tcu::Vec4));
    const uint32_t vertexStride   = (kVec4Sz * 2u);
    const uint32_t positionOffset = 0u;
    const uint32_t colorOffset    = kVec4Sz;

    // Vertex buffer
    const auto vbSize = static_cast<VkDeviceSize>(de::dataSize(vertices));
    const auto vbInfo = makeBufferCreateInfo(vbSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    BufferWithMemory vertexBuffer(ctx.vkd, ctx.device, ctx.allocator, vbInfo, MemoryRequirement::HostVisible);
    const auto vbAlloc  = vertexBuffer.getAllocation();
    void *vbData        = vbAlloc.getHostPtr();
    const auto vbOffset = static_cast<VkDeviceSize>(0);

    deMemcpy(vbData, de::dataOrNull(vertices), de::dataSize(vertices));
    flushAlloc(ctx.vkd, ctx.device, vbAlloc); // strictly speaking, not needed.

    using BufferWithMemoryPtr = std::unique_ptr<BufferWithMemory>;

    // Used in the atomic buffer case.
    Move<VkDescriptorSetLayout> setLayout;
    Move<VkDescriptorPool> descriptorPool;
    Move<VkDescriptorSet> descriptorSet;
    BufferWithMemoryPtr atomicBuffer;

    const auto atomicBufferType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    const auto atomicBufferSize = static_cast<VkDeviceSize>(sizeof(uint32_t));

    if (isAtomic)
    {
        // Zero-out atomic counter.
        const auto atomicBufferInfo = makeBufferCreateInfo(atomicBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
        atomicBuffer.reset(
            new BufferWithMemory(ctx.vkd, ctx.device, ctx.allocator, atomicBufferInfo, MemoryRequirement::HostVisible));
        deMemset(atomicBuffer->getAllocation().getHostPtr(), 0, static_cast<size_t>(atomicBufferSize));

        DescriptorSetLayoutBuilder layoutBuilder;
        layoutBuilder.addSingleBinding(atomicBufferType, resShaderStages);
        setLayout = layoutBuilder.build(ctx.vkd, ctx.device);

        DescriptorPoolBuilder descPoolBuilder;
        descPoolBuilder.addType(atomicBufferType);
        descriptorPool =
            descPoolBuilder.build(ctx.vkd, ctx.device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);

        descriptorSet = makeDescriptorSet(ctx.vkd, ctx.device, *descriptorPool, *setLayout);

        DescriptorSetUpdateBuilder updateBuilder;
        const auto atomicBufferDescriptorInfo = makeDescriptorBufferInfo(atomicBuffer->get(), 0ull, atomicBufferSize);
        updateBuilder.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u), atomicBufferType,
                                  &atomicBufferDescriptorInfo);
        updateBuilder.update(ctx.vkd, ctx.device);
    }

    // Pipeline layout, render pass and framebuffer.
    const auto pipelineLayout = makePipelineLayout(ctx.vkd, ctx.device, *setLayout);
    const auto renderPass     = makeRenderPass(ctx.vkd, ctx.device, colorFormat);
    const auto framebuffer    = makeFramebuffer(ctx.vkd, ctx.device, renderPass.get(), colorBuffer.getImageView(),
                                                vkExtent.width, vkExtent.height);

    const bool isInvQuery = (params.queryType == QueryType::INVOCATIONS);
    const auto queryType  = (isInvQuery ? VK_QUERY_TYPE_PIPELINE_STATISTICS : VK_QUERY_TYPE_OCCLUSION);
    const auto statFlags =
        (isInvQuery ?
             static_cast<VkQueryPipelineStatisticFlags>(VK_QUERY_PIPELINE_STATISTIC_FRAGMENT_SHADER_INVOCATIONS_BIT) :
             0u);
    const auto controlFlags = (isInvQuery ? 0u : static_cast<VkQueryControlFlags>(VK_QUERY_CONTROL_PRECISE_BIT));

    const VkQueryPoolCreateInfo queryPoolCreateInfo = {
        VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO, // VkStructureType sType;
        nullptr,                                  // const void* pNext;
        0u,                                       // VkQueryPoolCreateFlags flags;
        queryType,                                // VkQueryType queryType;
        1u,                                       // uint32_t queryCount;
        statFlags,                                // VkQueryPipelineStatisticFlags pipelineStatistics;
    };
    const auto queryPool = createQueryPool(ctx.vkd, ctx.device, &queryPoolCreateInfo);

    const std::vector<VkViewport> viewports(1u, makeViewport(vkExtent));
    const std::vector<VkRect2D> scissors(1u, makeRect2D(fbExtent));

    // Vertex buffer description.
    const auto vtxBinding = makeVertexInputBindingDescription(0u, vertexStride, VK_VERTEX_INPUT_RATE_VERTEX);
    const std::vector<VkVertexInputAttributeDescription> vtxAttribs{
        makeVertexInputAttributeDescription(0u, 0u, VK_FORMAT_R32G32B32A32_SFLOAT, positionOffset),
        makeVertexInputAttributeDescription(1u, 0u, VK_FORMAT_R32G32B32A32_SFLOAT, colorOffset),
    };
    const VkPipelineVertexInputStateCreateInfo inputStateCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, // VkStructureType sType;
        nullptr,                                                   // const void* pNext;
        0u,                                                        // VkPipelineVertexInputStateCreateFlags flags;
        1u,                                                        // uint32_t vertexBindingDescriptionCount;
        &vtxBinding,                // const VkVertexInputBindingDescription* pVertexBindingDescriptions;
        de::sizeU32(vtxAttribs),    // uint32_t vertexAttributeDescriptionCount;
        de::dataOrNull(vtxAttribs), // const VkVertexInputAttributeDescription* pVertexAttributeDescriptions;
    };

    const auto pipeline =
        makeGraphicsPipeline(ctx.vkd, ctx.device, pipelineLayout.get(), vertModule.get(), VK_NULL_HANDLE,
                             VK_NULL_HANDLE, VK_NULL_HANDLE, fragModule.get(), renderPass.get(), viewports, scissors,
                             VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0u, 0u, &inputStateCreateInfo);

    CommandPoolWithBuffer cmd(ctx.vkd, ctx.device, ctx.qfIndex);
    VkCommandBuffer primaryCmdBuffer = cmd.cmdBuffer.get();
    Move<VkCommandBuffer> secCmdBufferPtr;

    if (params.secondary)
    {
        secCmdBufferPtr =
            allocateCommandBuffer(ctx.vkd, ctx.device, cmd.cmdPool.get(), VK_COMMAND_BUFFER_LEVEL_SECONDARY);
        const auto secCmdBuffer = secCmdBufferPtr.get();

        const VkCommandBufferInheritanceInfo inheritanceInfo = {
            VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO,             // VkStructureType sType;
            nullptr,                                                       // const void* pNext;
            renderPass.get(),                                              // VkRenderPass renderPass;
            0u,                                                            // uint32_t subpass;
            framebuffer.get(),                                             // VkFramebuffer framebuffer;
            ((queryType == VK_QUERY_TYPE_OCCLUSION) ? VK_TRUE : VK_FALSE), // VkBool32 occlusionQueryEnable;
            controlFlags,                                                  // VkQueryControlFlags queryFlags;
            statFlags, // VkQueryPipelineStatisticFlags pipelineStatistics;
        };

        const auto usageFlags =
            (VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT | VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT);
        const VkCommandBufferBeginInfo beginInfo = {
            VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, // VkStructureType sType;
            nullptr,                                     // const void* pNext;
            usageFlags,                                  // VkCommandBufferUsageFlags flags;
            &inheritanceInfo,                            // const VkCommandBufferInheritanceInfo* pInheritanceInfo;
        };

        VK_CHECK(ctx.vkd.beginCommandBuffer(secCmdBuffer, &beginInfo));
        recordRenderPassCommands(ctx.vkd, secCmdBuffer, bindPoint, pipeline.get(), *pipelineLayout, *descriptorSet,
                                 *vertexBuffer, vbOffset);
        endCommandBuffer(ctx.vkd, secCmdBuffer);
    }

    const auto subpassContents =
        (params.secondary ? VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS : VK_SUBPASS_CONTENTS_INLINE);
    const auto clearColor = makeClearValueColor(getClearColor());

    beginCommandBuffer(ctx.vkd, primaryCmdBuffer);
    ctx.vkd.cmdResetQueryPool(primaryCmdBuffer, queryPool.get(), 0u, 1u);
    ctx.vkd.cmdBeginQuery(primaryCmdBuffer, queryPool.get(), 0u, controlFlags);
    beginRenderPass(ctx.vkd, primaryCmdBuffer, renderPass.get(), framebuffer.get(), scissors.at(0), clearColor,
                    subpassContents);
    if (!params.secondary)
        recordRenderPassCommands(ctx.vkd, primaryCmdBuffer, bindPoint, pipeline.get(), *pipelineLayout, *descriptorSet,
                                 *vertexBuffer, vbOffset);
    else
        ctx.vkd.cmdExecuteCommands(primaryCmdBuffer, 1u, &secCmdBufferPtr.get());
    endRenderPass(ctx.vkd, primaryCmdBuffer);
    ctx.vkd.cmdEndQuery(primaryCmdBuffer, queryPool.get(), 0u);
    {
        const auto preTransferBarrier = makeImageMemoryBarrier(
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, colorBuffer.getImage(), colorSRR);
        cmdPipelineImageMemoryBarrier(ctx.vkd, primaryCmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                      VK_PIPELINE_STAGE_TRANSFER_BIT, &preTransferBarrier);

        const auto copyRegion = makeBufferImageCopy(vkExtent, colorSRL);
        ctx.vkd.cmdCopyImageToBuffer(primaryCmdBuffer, colorBuffer.getImage(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                     colorBuffer.getBuffer(), 1u, &copyRegion);

        // Synchronize color buffer copy, and possible atomic writes from the frag shader, to the host.
        {
            VkAccessFlags srcAccess        = VK_ACCESS_TRANSFER_WRITE_BIT;
            VkPipelineStageFlags srcStages = VK_PIPELINE_STAGE_TRANSFER_BIT;

            if (isAtomic)
            {
                srcAccess |= VK_ACCESS_SHADER_WRITE_BIT;
                srcStages |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            }

            const auto preHostBarrier = makeMemoryBarrier(srcAccess, VK_ACCESS_HOST_READ_BIT);
            cmdPipelineMemoryBarrier(ctx.vkd, primaryCmdBuffer, srcStages, VK_PIPELINE_STAGE_HOST_BIT, &preHostBarrier);
        }
    }
    endCommandBuffer(ctx.vkd, primaryCmdBuffer);
    submitCommandsAndWait(ctx.vkd, ctx.device, ctx.queue, primaryCmdBuffer);

    const auto resultAllocation = colorBuffer.getBufferAllocation();
    invalidateAlloc(ctx.vkd, ctx.device, resultAllocation);

    uint32_t queryResult = 0u;
    VK_CHECK(ctx.vkd.getQueryPoolResults(ctx.device, queryPool.get(), 0u, 1u, sizeof(queryResult), &queryResult,
                                         static_cast<VkDeviceSize>(sizeof(queryResult)), VK_QUERY_RESULT_WAIT_BIT));

    const auto pixelCount = vkExtent.width * vkExtent.height * vkExtent.depth;

    // Check query results.
    if (isInvQuery)
    {
        uint32_t minCount = std::numeric_limits<uint32_t>::max();

        if (params.fragShaderVariant == FragShaderVariant::FLAT)
        {
            // Implementations are allowed to reuse fragment shader invocations to shade different fragments under some circumstances:
            // - The frag shader statically computes the same value for different framebuffer locations, and
            // - It does not write to any storage resources.
            // The spec does not mention a minimum number of invocations, but in practice we're tying this to enabling fragment shading
            // rate support automatically. We'll suppose implementations not supporting fragment shading rate will not do this, and
            // those supporting it will not run less invocations than the whole framebuffer divided into areas of maxFramentSize pixels.
            // If this proves problematic, we can relax the check later.
            const auto maxFragmentSize = (context.isDeviceFunctionalitySupported("VK_KHR_fragment_shading_rate") ?
                                              context.getFragmentShadingRateProperties().maxFragmentSize :
                                              makeExtent2D(1u, 1u));
            const auto maxFragmentWidth =
                std::max(maxFragmentSize.width, 1u); // In case an implementation reports zero.
            const auto maxFragmentHeight = std::max(maxFragmentSize.height, 1u); // Ditto.
            const auto minCols           = vkExtent.width / maxFragmentWidth;
            const auto minRows           = vkExtent.height / maxFragmentHeight;

            minCount = minCols * minRows * vkExtent.depth;
        }
        else
            minCount = pixelCount;

        if (queryResult < minCount)
        {
            std::ostringstream msg;
            msg << "Framebuffer size: " << vkExtent.width << "x" << vkExtent.height
                << "; expected query result to be at least " << minCount << " but found " << queryResult;
            return tcu::TestStatus::fail(msg.str());
        }
    }
    else
    {
        if (pixelCount != queryResult)
        {
            std::ostringstream msg;
            msg << "Framebuffer size: " << vkExtent.width << "x" << vkExtent.height << "; expected query result to be "
                << pixelCount << " but found " << queryResult;
            return tcu::TestStatus::fail(msg.str());
        }
    }

    if (isAtomic)
    {
        // Verify atomic counter.
        auto &atomicBufferAlloc = atomicBuffer->getAllocation();
        invalidateAlloc(ctx.vkd, ctx.device, atomicBufferAlloc);

        uint32_t atomicCounter = 0u;
        deMemcpy(&atomicCounter, atomicBufferAlloc.getHostPtr(), sizeof(atomicCounter));

        if (atomicCounter != pixelCount)
        {
            std::ostringstream msg;
            msg << "Framebuffer size: " << vkExtent.width << "x" << vkExtent.height
                << "; expected atomic counter to be " << pixelCount << " but found " << atomicCounter;
            return tcu::TestStatus::fail(msg.str());
        }
    }

    // Check color buffer.
    const auto tcuFormat = mapVkFormat(colorFormat);
    auto &log            = context.getTestContext().getLog();
    const tcu::Vec4 colorThreshold(0.0f, 0.0f, 0.0f, 0.0f); // Expect exact color result.
    tcu::ConstPixelBufferAccess resultAccess(tcuFormat, fbExtent, resultAllocation.getHostPtr());

    if (!tcu::floatThresholdCompare(log, "Result", "", geometryColor, resultAccess, colorThreshold,
                                    tcu::COMPARE_LOG_ON_ERROR))
        return tcu::TestStatus::fail("Unexpected results in color buffer -- check log for details");

    return tcu::TestStatus::pass("Pass");
}

} // anonymous namespace

tcu::TestCaseGroup *createFragInvocationTests(tcu::TestContext &testContext)
{
    using GroupPtr = de::MovePtr<tcu::TestCaseGroup>;

    // Test implementations do not optimize out fragment shader invocations
    GroupPtr mainGroup(new tcu::TestCaseGroup(testContext, "frag_invocations"));

    const struct
    {
        FragShaderVariant fragShaderVariant;
        const std::string suffix;
    } fragShaderVariantCases[] = {
        {FragShaderVariant::FLAT, ""},
        {FragShaderVariant::VERTEX_COLOR, "_with_vertex_color"},
        {FragShaderVariant::ATOMIC_COUNTER, "_with_atomic_counter"},
    };

    for (const auto queryType : {QueryType::OCCLUSION, QueryType::INVOCATIONS})
    {
        const auto groupName = getQueryTypeName(queryType);
        GroupPtr queryTypeGroup(new tcu::TestCaseGroup(testContext, groupName.c_str()));

        for (const auto secondaryCase : {false, true})
        {
            for (const auto &fragShaderVariantCase : fragShaderVariantCases)
            {
                const auto testName = (secondaryCase ? "secondary" : "primary") + fragShaderVariantCase.suffix;
                const TestParams params{queryType, secondaryCase, fragShaderVariantCase.fragShaderVariant};
                addFunctionCaseWithPrograms(queryTypeGroup.get(), testName, checkSupport, initPrograms, testInvocations,
                                            params);
            }
        }

        mainGroup->addChild(queryTypeGroup.release());
    }

    return mainGroup.release();
}

} // namespace QueryPool
} // namespace vkt
