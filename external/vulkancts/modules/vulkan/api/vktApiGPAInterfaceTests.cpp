/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2026 The Khronos Group Inc.
 * Copyright (c) 2026 Valve Corporation.
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
 * \brief VK_AMD_gpa_interface Tests
 *//*--------------------------------------------------------------------*/
#include "vktTestCase.hpp"
#include "vktApiGPAInterfaceTests.hpp"
#include "vktTestGroupUtil.hpp"
#include "vktTestCaseUtil.hpp"

#include "vkBarrierUtil.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkBuilderUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkRefUtil.hpp"

#include "tcuImageCompare.hpp"
#include "tcuTextureUtil.hpp"

#include <limits>
#include <memory>
#include <set>
#include <sstream>
#include <vector>

namespace vkt::api
{

namespace
{

using namespace vk;

enum GpaInterfaceFeatureFlagBits
{
    GPAI_FEATURE_PERF_COUNTERS           = (1 << 0),
    GPAI_FEATURE_STREAMING_PERF_COUNTERS = (1 << 1),
    GPAI_FEATURE_SQ_THREAD_TRACING       = (1 << 2),
    GPAI_FEATURE_CLOCK_MODES             = (1 << 3),
};
using GpaInterfaceFeatureFlags = uint32_t;

void checkFeatureSupport(Context &context, GpaInterfaceFeatureFlags flags)
{
    context.requireDeviceFunctionality("VK_AMD_gpa_interface");
    const auto &gpaFeatures = context.getGpaFeaturesAMD();

    if ((flags & GPAI_FEATURE_PERF_COUNTERS) && !gpaFeatures.perfCounters)
        TCU_THROW(NotSupportedError, "perfCounters not supported");

    if ((flags & GPAI_FEATURE_STREAMING_PERF_COUNTERS) && !gpaFeatures.streamingPerfCounters)
        TCU_THROW(NotSupportedError, "streamingPerfCounters not supported");

    if ((flags & GPAI_FEATURE_SQ_THREAD_TRACING) && !gpaFeatures.sqThreadTracing)
        TCU_THROW(NotSupportedError, "sqThreadTracing not supported");

    if ((flags & GPAI_FEATURE_CLOCK_MODES) && !gpaFeatures.clockModes)
        TCU_THROW(NotSupportedError, "clockModes not supported");
}

void checkBasicSupport(Context &context)
{
    checkFeatureSupport(context, 0u);
}

tcu::TestStatus testDeviceClockInfo(Context &context)
{
    const auto ctx                  = context.getContextCommonData();
    VkGpaDeviceGetClockInfoAMD info = initVulkanStructureConst();
    VK_CHECK(ctx.vkd.getGpaDeviceClockInfoAMD(ctx.device, &info));

    constexpr uint32_t kMinFrequency = 50;    // Less than 50 MHz is suspiciously low.
    constexpr uint32_t kMaxFrequency = 10000; // More than 10 GHz is suspiciouly high.

    constexpr float kMinRatio = 0.0f;
    constexpr float kMaxRatio = 1.0f;

    bool fail = false;
    auto &log = context.getTestContext().getLog();

    const auto checkFreq = [&](uint32_t value, const char *name)
    {
        if (value < kMinFrequency || value > kMaxFrequency)
        {
            fail = true;
            std::ostringstream msg;
            msg << name << " (" << value << ") out of expected range [" << kMinFrequency << ", " << kMaxFrequency
                << "]";
            log << tcu::TestLog::Message << msg.str() << tcu::TestLog::EndMessage;
        }
    };

    const auto checkRatio = [&](float value, const char *name)
    {
        if (value <= kMinRatio || value > kMaxRatio)
        {
            fail = true;
            std::ostringstream msg;
            msg << name << " (" << value << ") out of expected range (" << kMinRatio << ", " << kMaxRatio << "]";
            log << tcu::TestLog::Message << msg.str() << tcu::TestLog::EndMessage;
        }
    };

    checkFreq(info.engineClockFrequency, "engineClockFrequency");
    checkFreq(info.memoryClockFrequency, "memoryClockFrequency");
    checkRatio(info.engineClockRatioToPeak, "engineClockRatioToPeak");
    checkRatio(info.memoryClockRatioToPeak, "memoryClockRatioToPeak");

    if (fail)
        TCU_FAIL("Unexpected clock info values; check log for details");

    return tcu::TestStatus::pass("Pass");
}

Move<VkGpaSessionAMD> makeGpaSession(const DeviceInterface &vkd, VkDevice device,
                                     VkGpaSessionAMD copySrc = VK_NULL_HANDLE)
{
    VkGpaSessionCreateInfoAMD createInfo = initVulkanStructureConst();
    createInfo.secondaryCopySource       = copySrc;
    return createGpaSessionAMD(vkd, device, &createInfo);
}

tcu::TestStatus testCreateDestroySession(Context &context)
{
    const auto ctx     = context.getContextCommonData();
    const auto session = makeGpaSession(ctx.vkd, ctx.device);
    return tcu::TestStatus::pass("Pass");
}

// Confusingly, some vkCmd commands for this extension return a VkResult, which is highly unusual.
// The wrappers provided here make sure the result is always checked.

void cmdBeginGpaSessionAMD(const DeviceInterface &vkd, VkCommandBuffer cmdBuffer, VkGpaSessionAMD session)
{
    VK_CHECK(vkd.cmdBeginGpaSessionAMD(cmdBuffer, session));
}

void cmdEndGpaSessionAMD(const DeviceInterface &vkd, VkCommandBuffer cmdBuffer, VkGpaSessionAMD session)
{
    VK_CHECK(vkd.cmdEndGpaSessionAMD(cmdBuffer, session));
}

uint32_t cmdBeginGpaSampleAMD(const DeviceInterface &vkd, VkCommandBuffer cmdBuffer, VkGpaSessionAMD session,
                              const VkGpaSampleBeginInfoAMD *pGpaSampleBeginInfo)
{
    // This one also takes the chance to return the sample id instead of using a pointer.
    uint32_t sampleId = std::numeric_limits<uint32_t>::max();
    VK_CHECK(vkd.cmdBeginGpaSampleAMD(cmdBuffer, session, pGpaSampleBeginInfo, &sampleId));
    return sampleId;
}

void cmdEndGpaSampleAMD(const DeviceInterface &vkd, VkCommandBuffer cmdBuffer, VkGpaSessionAMD session,
                        uint32_t sampleId)
{
    // For consistency, so all CMDs use wrappers.
    vkd.cmdEndGpaSampleAMD(cmdBuffer, session, sampleId);
}

void recordEmptySession(const DeviceInterface &vkd, VkCommandBuffer cmdBuffer, VkGpaSessionAMD session)
{
    cmdBeginGpaSessionAMD(vkd, cmdBuffer, session);
    cmdEndGpaSessionAMD(vkd, cmdBuffer, session);
}

tcu::TestStatus testCreateDestroySessionWithCopy(Context &context)
{
    const auto ctx = context.getContextCommonData();

    const auto srcSession = makeGpaSession(ctx.vkd, ctx.device);

    // vkCreateGpaSessionAMD with secondaryCopySource only works after the source GPA session has a begin and end.
    CommandPoolWithBuffer cmd(ctx.vkd, ctx.device, ctx.qfIndex);
    const auto cmdBuffer = *cmd.cmdBuffer;

    beginCommandBuffer(ctx.vkd, cmdBuffer);
    recordEmptySession(ctx.vkd, cmdBuffer, *srcSession);
    endCommandBuffer(ctx.vkd, cmdBuffer);

    const auto dstSession = makeGpaSession(ctx.vkd, ctx.device, *srcSession);

    return tcu::TestStatus::pass("Pass");
}

// Transform a container of Move<VkGpaSessionAMD> to a vector<VkGpaSessionAMD>.
template <class Iter>
std::vector<VkGpaSessionAMD> getSessionHandles(Iter beginIter, Iter endIter)
{
    std::vector<VkGpaSessionAMD> handles;
    for (auto iter = beginIter; iter != endIter; ++iter)
        handles.push_back(iter->get());
    return handles;
}

template <class Iter>
bool checkSessionStatus(tcu::TestLog &log, const DeviceInterface &vkd, VkDevice device, Iter beginIter, Iter endIter)
{
    bool ok  = true;
    size_t i = 0;

    for (auto iter = beginIter; iter != endIter; ++iter, ++i)
    {
        const auto status = vkd.getGpaSessionStatusAMD(device, *iter);
        if (status != VK_SUCCESS && status != VK_NOT_READY)
        {
            ok = false;
            std::ostringstream msg;
            msg << "Session " << i << ": status " << getResultName(status);
            log << tcu::TestLog::Message << msg.str() << tcu::TestLog::EndMessage;
        }
    }

    return ok;
}

tcu::TestStatus testEmptySessions(Context &context)
{
    const auto ctx          = context.getContextCommonData();
    const auto sessionCount = 2u;

    std::vector<Move<VkGpaSessionAMD>> sessions;
    sessions.reserve(sessionCount);
    for (uint32_t i = 0u; i < sessionCount; ++i)
        sessions.push_back(makeGpaSession(ctx.vkd, ctx.device));

    CommandPoolWithBuffer cmd(ctx.vkd, ctx.device, ctx.qfIndex);
    const auto cmdBuffer = *cmd.cmdBuffer;

    beginCommandBuffer(ctx.vkd, cmdBuffer);
    for (uint32_t i = 0u; i < sessionCount; ++i)
        recordEmptySession(ctx.vkd, cmdBuffer, *sessions.at(i));
    endCommandBuffer(ctx.vkd, cmdBuffer);
    submitCommandsAndWait(ctx.vkd, ctx.device, ctx.queue, cmdBuffer);

    const auto sessionHandles = getSessionHandles(sessions.begin(), sessions.end());
    auto &log                 = context.getTestContext().getLog();

    if (!checkSessionStatus(log, ctx.vkd, ctx.device, sessionHandles.begin(), sessionHandles.end()))
        TCU_FAIL("Unexpected vkGetGpaSessionStatusAMD results for some sessions; check log for details --");

    return tcu::TestStatus::pass("Pass");
}

tcu::TestStatus testCreatedStatus(Context &context)
{
    const auto ctx     = context.getContextCommonData();
    const auto session = makeGpaSession(ctx.vkd, ctx.device);

    const auto res = ctx.vkd.getGpaSessionStatusAMD(ctx.device, *session);
    if (res != VK_NOT_READY)
    {
        std::ostringstream msg;
        msg << "Unexpected session status after creation: " << getResultName(res);
        TCU_FAIL(msg.str());
    }

    return tcu::TestStatus::pass("Pass");
}

tcu::TestStatus testUnfinishedStatus(Context &context)
{
    const auto ctx     = context.getContextCommonData();
    const auto session = makeGpaSession(ctx.vkd, ctx.device);

    CommandPoolWithBuffer cmd(ctx.vkd, ctx.device, ctx.qfIndex);
    const auto cmdBuffer = *cmd.cmdBuffer;

    const auto checkNotReady = [&](const char *errorPrefix)
    {
        const auto res = ctx.vkd.getGpaSessionStatusAMD(ctx.device, *session);
        if (res != VK_NOT_READY)
        {
            std::ostringstream msg;
            msg << errorPrefix << ": " << getResultName(res);
            TCU_FAIL(msg.str());
        }
    };

    beginCommandBuffer(ctx.vkd, cmdBuffer);
    cmdBeginGpaSessionAMD(ctx.vkd, cmdBuffer, *session);

    checkNotReady("Unexpected session status after creation");

    endCommandBuffer(ctx.vkd, cmdBuffer);
    submitCommandsAndWait(ctx.vkd, ctx.device, ctx.queue, cmdBuffer);

    checkNotReady("Unexpected session status after recording");

    return tcu::TestStatus::pass("Pass");
}

tcu::TestStatus testEmptySessionsMultiCmd(Context &context)
{
    const auto ctx                 = context.getContextCommonData();
    constexpr size_t kSessionCount = 3u;

    std::vector<Move<VkGpaSessionAMD>> sessions;
    sessions.reserve(kSessionCount);
    for (uint32_t i = 0u; i < kSessionCount; ++i)
        sessions.push_back(makeGpaSession(ctx.vkd, ctx.device));

    const auto cmdPool       = makeCommandPool(ctx.vkd, ctx.device, ctx.qfIndex);
    const auto cmdBuffer1Ptr = allocateCommandBuffer(ctx.vkd, ctx.device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    const auto cmdBuffer2Ptr = allocateCommandBuffer(ctx.vkd, ctx.device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    const auto cmdBuffer1    = *cmdBuffer1Ptr;
    const auto cmdBuffer2    = *cmdBuffer2Ptr;

    beginCommandBuffer(ctx.vkd, cmdBuffer1);
    recordEmptySession(ctx.vkd, cmdBuffer1, *sessions.front());
    cmdBeginGpaSessionAMD(ctx.vkd, cmdBuffer1, *sessions.at(1));
    endCommandBuffer(ctx.vkd, cmdBuffer1);

    beginCommandBuffer(ctx.vkd, cmdBuffer2);
    cmdEndGpaSessionAMD(ctx.vkd, cmdBuffer2, *sessions.at(1));
    recordEmptySession(ctx.vkd, cmdBuffer2, *sessions.back());
    endCommandBuffer(ctx.vkd, cmdBuffer2);

    const auto semaphore = createSemaphore(ctx.vkd, ctx.device);
    const auto fence     = createFence(ctx.vkd, ctx.device);

    const auto waitStage = static_cast<VkPipelineStageFlags>(VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
    const std::vector<VkSubmitInfo> submitInfos{
        VkSubmitInfo{
            VK_STRUCTURE_TYPE_SUBMIT_INFO,
            nullptr,
            0u,
            nullptr,
            nullptr,
            1u,
            &cmdBuffer1,
            1u,
            &semaphore.get(),
        },
        VkSubmitInfo{
            VK_STRUCTURE_TYPE_SUBMIT_INFO,
            nullptr,
            1u,
            &semaphore.get(),
            &waitStage,
            1u,
            &cmdBuffer2,
            0u,
            nullptr,
        },
    };
    ctx.vkd.queueSubmit(ctx.queue, de::sizeU32(submitInfos), de::dataOrNull(submitInfos), *fence);
    waitForFence(ctx.vkd, ctx.device, *fence);

    const auto sessionHandles = getSessionHandles(sessions.begin(), sessions.end());
    auto &log                 = context.getTestContext().getLog();

    if (!checkSessionStatus(log, ctx.vkd, ctx.device, sessionHandles.begin(), sessionHandles.end()))
        TCU_FAIL("Unexpected vkGetGpaSessionStatusAMD results for some sessions; check log for details --");

    return tcu::TestStatus::pass("Pass");
}

void checkGpaAndSync2(Context &context)
{
    checkBasicSupport(context);
    context.requireDeviceFunctionality("VK_KHR_synchronization2");
}

void fullBarrier(const DeviceInterface &vkd, VkCommandBuffer cmdBuffer)
{
    const auto stageMask = static_cast<VkPipelineStageFlags2>(VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT_KHR);
    const auto accessMask =
        static_cast<VkAccessFlags2>(VK_ACCESS_2_MEMORY_READ_BIT_KHR | VK_ACCESS_2_MEMORY_WRITE_BIT_KHR);

    const VkMemoryBarrier2KHR memoryBarrier{
        VK_STRUCTURE_TYPE_MEMORY_BARRIER_2, nullptr, stageMask, accessMask, stageMask, accessMask,
    };

    const VkDependencyInfo dependencyInfo{
        VK_STRUCTURE_TYPE_DEPENDENCY_INFO, nullptr, 0u, 1u, &memoryBarrier, 0u, nullptr, 0u, nullptr,
    };

    vkd.cmdPipelineBarrier2(cmdBuffer, &dependencyInfo);
}

tcu::TestStatus testEmptySecondaryWithCopy(Context &context)
{
    const auto ctx = context.getContextCommonData();

    const auto cmdPool       = makeCommandPool(ctx.vkd, ctx.device, ctx.qfIndex);
    const auto primaryCmdPtr = allocateCommandBuffer(ctx.vkd, ctx.device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    const auto secondaryCmdPtr =
        allocateCommandBuffer(ctx.vkd, ctx.device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_SECONDARY);

    const auto primaryCmd   = *primaryCmdPtr;
    const auto secondaryCmd = *secondaryCmdPtr;

    const auto secondarySession = makeGpaSession(ctx.vkd, ctx.device);

    const auto secondaryUsageFlags =
        static_cast<VkCommandBufferUsageFlags>(VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT);
    beginSecondaryCommandBuffer(ctx.vkd, secondaryCmd, VK_NULL_HANDLE, VK_NULL_HANDLE, secondaryUsageFlags);
    recordEmptySession(ctx.vkd, secondaryCmd, *secondarySession);
    endCommandBuffer(ctx.vkd, secondaryCmd);

    const auto primarySession0 = makeGpaSession(ctx.vkd, ctx.device, *secondarySession);
    const auto primarySession1 = makeGpaSession(ctx.vkd, ctx.device, *secondarySession);

    beginCommandBuffer(ctx.vkd, primaryCmd);
    ctx.vkd.cmdExecuteCommands(primaryCmd, 1u, &secondaryCmd);
    fullBarrier(ctx.vkd, primaryCmd);
    ctx.vkd.cmdCopyGpaSessionResultsAMD(primaryCmd, *primarySession0);
    ctx.vkd.cmdExecuteCommands(primaryCmd, 1u, &secondaryCmd);
    fullBarrier(ctx.vkd, primaryCmd);
    ctx.vkd.cmdCopyGpaSessionResultsAMD(primaryCmd, *primarySession1);
    endCommandBuffer(ctx.vkd, primaryCmd);
    submitCommandsAndWait(ctx.vkd, ctx.device, ctx.queue, primaryCmd);

    return tcu::TestStatus::pass("Pass");
}

tcu::TestStatus testResetEmptySession(Context &context)
{
    const auto ctx       = context.getContextCommonData();
    const auto cmdPool   = makeCommandPool(ctx.vkd, ctx.device, ctx.qfIndex);
    const auto firstCmd  = allocateCommandBuffer(ctx.vkd, ctx.device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    const auto secondCmd = allocateCommandBuffer(ctx.vkd, ctx.device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    const auto session   = makeGpaSession(ctx.vkd, ctx.device);

    beginCommandBuffer(ctx.vkd, *firstCmd);
    recordEmptySession(ctx.vkd, *firstCmd, *session);
    endCommandBuffer(ctx.vkd, *firstCmd);
    submitCommandsAndWait(ctx.vkd, ctx.device, ctx.queue, *firstCmd);

    const auto firstRes = ctx.vkd.getGpaSessionStatusAMD(ctx.device, *session);
    if (firstRes != VK_SUCCESS)
    {
        std::ostringstream msg;
        msg << "Unexpected session status before reset: " << getResultName(firstRes);
        TCU_FAIL(msg.str());
    }

    ctx.vkd.resetGpaSessionAMD(ctx.device, *session);

    const auto secondRes = ctx.vkd.getGpaSessionStatusAMD(ctx.device, *session);
    if (secondRes != VK_NOT_READY && secondRes != VK_SUCCESS)
    {
        std::ostringstream msg;
        msg << "Unexpected session status after reset: " << getResultName(firstRes);
        TCU_FAIL(msg.str());
    }

    beginCommandBuffer(ctx.vkd, *secondCmd);
    cmdBeginGpaSessionAMD(ctx.vkd, *secondCmd, *session);
    endCommandBuffer(ctx.vkd, *secondCmd);
    submitCommandsAndWait(ctx.vkd, ctx.device, ctx.queue, *secondCmd);

    const auto thirdRes = ctx.vkd.getGpaSessionStatusAMD(ctx.device, *session);
    if (thirdRes != VK_NOT_READY)
    {
        std::ostringstream msg;
        msg << "Unexpected session status after begin: " << getResultName(firstRes);
        TCU_FAIL(msg.str());
    }

    return tcu::TestStatus::pass("Pass");
}

struct RegularSessionParams
{
    VkShaderStageFlags stageFlags;
    bool timing; // Use VK_GPA_SAMPLE_TYPE_TIMING_AMD.

    bool isCompute() const
    {
        return (stageFlags & VK_SHADER_STAGE_COMPUTE_BIT);
    }

    uint32_t getComputeWgSize() const
    {
        return 64u;
    }

    uint32_t getComputeWgCount() const
    {
        return 64u;
    }

    tcu::IVec3 getExtent() const
    {
        return tcu::IVec3(64, 64, 1);
    }

    bool hasShaderStage(VkShaderStageFlagBits stage) const
    {
        return (stageFlags & stage);
    }

    tcu::Vec4 getClearColor() const
    {
        return tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f);
    }

    tcu::Vec4 getVertColor() const
    {
        return tcu::Vec4(0.0f, 0.0f, 1.0f, 1.0f);
    }

    tcu::Vec4 getTessColor() const
    {
        return tcu::Vec4(0.0f, 1.0f, 1.0f, 1.0f);
    }

    tcu::Vec4 getGeomColor() const
    {
        return tcu::Vec4(1.0f, 1.0f, 1.0f, 1.0f);
    }

    tcu::Vec4 getFinalColor() const
    {
        auto color = getVertColor();
        if (hasShaderStage(VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT))
            color = getTessColor();
        if (hasShaderStage(VK_SHADER_STAGE_GEOMETRY_BIT))
            color = getGeomColor();
        return color;
    }
};
using RegularSessionParamsPtr = std::shared_ptr<const RegularSessionParams>;

void checkRegularSupport(Context &context, RegularSessionParamsPtr params)
{
    GpaInterfaceFeatureFlags flags = 0u;
    flags |= GPAI_FEATURE_PERF_COUNTERS;
    checkFeatureSupport(context, flags);

    if (params->stageFlags & VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT)
        context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_TESSELLATION_SHADER);

    if (params->stageFlags & VK_SHADER_STAGE_GEOMETRY_BIT)
        context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_GEOMETRY_SHADER);
}

void initRegularPrograms(vk::SourceCollections &dst, RegularSessionParamsPtr params)
{
    if (params->isCompute())
    {
        std::ostringstream comp;
        comp << "#version 460\n"
             << "layout (local_size_x=" << params->getComputeWgSize() << ") in;\n"
             << "layout (set=0, binding=0, std430) buffer BufferBlock { uint values[]; } ssbo;\n"
             << "void main(void) { ssbo.values[gl_GlobalInvocationID.x] = 1u; }\n";
        dst.glslSources.add("comp") << glu::ComputeSource(comp.str());
    }
    else
    {
        DE_ASSERT(params->hasShaderStage(VK_SHADER_STAGE_VERTEX_BIT));
        std::ostringstream vert;
        vert << "#version 460\n"
             << "out gl_PerVertex {\n"
             << "    vec4 gl_Position;\n"
             << "};\n"
             << "layout (location=0) in vec4 inPos;\n"
             << "layout (location=0) out vec4 outColor;\n"
             << "void main(void) {\n"
             << "    gl_Position = inPos;\n"
             << "    outColor = vec4" << params->getVertColor() << ";\n"
             << "}\n";
        dst.glslSources.add("vert") << glu::VertexSource(vert.str());

        if (params->hasShaderStage(VK_SHADER_STAGE_FRAGMENT_BIT))
        {
            std::ostringstream frag;
            frag << "#version 460\n"
                 << "layout (location=0) in vec4 inColor;\n"
                 << "layout (location=0) out vec4 outColor;\n"
                 << "void main(void) {\n"
                 << "    outColor = inColor;\n"
                 << "}\n";
            dst.glslSources.add("frag") << glu::FragmentSource(frag.str());
        }

        if (params->hasShaderStage(VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT))
        {
            DE_ASSERT(params->hasShaderStage(VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT));

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
            dst.glslSources.add("tesc") << glu::TessellationControlSource(tesc.str());

            std::ostringstream tese;
            tese << "#version 460\n"
                 << "#extension GL_EXT_tessellation_shader : require\n"
                 << "layout(triangles) in;\n"
                 << "in gl_PerVertex {\n"
                 << "    vec4 gl_Position;\n"
                 << "} gl_in[gl_MaxPatchVertices];\n"
                 << "out gl_PerVertex {\n"
                 << "    vec4 gl_Position;\n"
                 << "};\n"
                 << "layout (location=0) out vec4 outColor;\n"
                 << "void main() {\n"
                 << "    outColor = vec4" << params->getTessColor() << ";\n"
                 << "    gl_Position = (gl_in[0].gl_Position * gl_TessCoord.x + \n"
                 << "                   gl_in[1].gl_Position * gl_TessCoord.y + \n"
                 << "                   gl_in[2].gl_Position * gl_TessCoord.z);\n"
                 << "}\n";
            dst.glslSources.add("tese") << glu::TessellationEvaluationSource(tese.str());
        }

        if (params->hasShaderStage(VK_SHADER_STAGE_GEOMETRY_BIT))
        {
            // Passthrough geometry shader.
            std::ostringstream geom;
            geom << "#version 460\n"
                 << "layout (triangles) in;\n"
                 << "layout (triangle_strip, max_vertices=3) out;\n"
                 << "in gl_PerVertex {\n"
                 << "    vec4 gl_Position;\n"
                 << "} gl_in[3];\n"
                 << "out gl_PerVertex {\n"
                 << "    vec4 gl_Position;\n"
                 << "};\n"
                 << "layout (location=0) out vec4 outColor;\n"
                 << "void main() {\n"
                 << "    for (uint i = 0; i < 3; ++i) {\n"
                 << "        outColor = vec4" << params->getGeomColor() << ";\n"
                 << "        gl_Position = gl_in[i].gl_Position;\n"
                 << "        EmitVertex();\n"
                 << "    }\n"
                 << "}\n";
            dst.glslSources.add("geom") << glu::GeometrySource(geom.str());
        }
    }
}

// We need this because, contrary to other properties, we need a double call to get element count and data.
std::vector<VkGpaPerfBlockPropertiesAMD> getDevicePerfBlocks(const InstanceInterface &vki, VkPhysicalDevice physDev)
{
    VkPhysicalDeviceGpaPropertiesAMD gpaProperties = initVulkanStructure();
    VkPhysicalDeviceProperties2 props2             = initVulkanStructure(&gpaProperties);

    std::vector<VkGpaPerfBlockPropertiesAMD> perfBlocks;

    vki.getPhysicalDeviceProperties2(physDev, &props2);
    if (gpaProperties.perfBlockCount > 0u)
    {
        perfBlocks.resize(gpaProperties.perfBlockCount);
        gpaProperties.pPerfBlocks = de::dataOrNull(perfBlocks);
        vki.getPhysicalDeviceProperties2(physDev, &props2);
    }

    return perfBlocks;
}

tcu::TestStatus testRegularSession(Context &context, RegularSessionParamsPtr params)
{
    const auto ctx = context.getContextCommonData();

    CommandPoolWithBuffer cmd(ctx.vkd, ctx.device, ctx.qfIndex);
    const auto cmdBuffer = *cmd.cmdBuffer;
    const auto session   = makeGpaSession(ctx.vkd, ctx.device);

    const auto perfBlocks = getDevicePerfBlocks(ctx.vki, ctx.physicalDevice);
    std::vector<VkGpaPerfCounterAMD> perfCounters;

    // According to the GPA library, these are problematic because they are exposed and do not always exist.
    const std::set<VkGpaPerfBlockAMD> skippedBlocks{
        VK_GPA_PERF_BLOCK_GL1CG_AMD, VK_GPA_PERF_BLOCK_ATC_AMD, VK_GPA_PERF_BLOCK_ATC_L2_AMD,
        VK_GPA_PERF_BLOCK_CHCG_AMD,  VK_GPA_PERF_BLOCK_GUS_AMD, VK_GPA_PERF_BLOCK_UMCCH_AMD,
        VK_GPA_PERF_BLOCK_RPB_AMD,   VK_GPA_PERF_BLOCK_PC_AMD,  VK_GPA_PERF_BLOCK_GRBM_SE_AMD,
    };

    for (uint32_t i = 0u; i < de::sizeU32(perfBlocks); ++i)
    {
        const auto &perfBlock = perfBlocks.at(i);

        if (perfBlock.instanceCount == 0u || perfBlock.maxEventID == 0u)
            continue;

        // Apparently this is the relevant field for VK_GPA_SAMPLE_TYPE_CUMULATIVE_AMD.
        if (!params->timing && perfBlock.maxGlobalOnlyCounters == 0u)
            continue;

        if (skippedBlocks.find(perfBlock.blockType) != skippedBlocks.end())
            continue;

        perfCounters.push_back(VkGpaPerfCounterAMD{
            perfBlock.blockType,
            0u,
            0u,
        });
    }

    const auto sampleType = (params->timing ? VK_GPA_SAMPLE_TYPE_TIMING_AMD : VK_GPA_SAMPLE_TYPE_CUMULATIVE_AMD);
    const auto timingPreSample =
        static_cast<VkPipelineStageFlags>(params->timing ? VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT : 0);
    const auto timingPostSample =
        static_cast<VkPipelineStageFlags>(params->timing ? VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT : 0);

    const VkGpaSampleBeginInfoAMD sampleBeginInfo{
        VK_STRUCTURE_TYPE_GPA_SAMPLE_BEGIN_INFO_AMD, //  VkStructureType             sType;
        nullptr,                                     //  const void*                 pNext;
        sampleType,                                  //  VkGpaSampleTypeAMD          sampleType;
        VK_FALSE,                                    //  VkBool32                    sampleInternalOperations;
        VK_FALSE,                                    //  VkBool32                    cacheFlushOnCounterCollection;
        VK_FALSE,                                    //  VkBool32                    sqShaderMaskEnable;
        0u,                                          //  VkGpaSqShaderStageFlagsAMD  sqShaderMask;
        de::sizeU32(perfCounters),                   //  uint32_t                    perfCounterCount;
        de::dataOrNull(perfCounters),                //  const VkGpaPerfCounterAMD*  pPerfCounters;
        0u,                                          //  uint32_t                    streamingPerfTraceSampleInterval;
        0ull,                                        //  VkDeviceSize                perfCounterDeviceMemoryLimit;
        VK_FALSE,                                    //  VkBool32                    sqThreadTraceEnable;
        VK_FALSE,         //  VkBool32                    sqThreadTraceSuppressInstructionTokens;
        0ull,             //  VkDeviceSize                sqThreadTraceDeviceMemoryLimit;
        timingPreSample,  //  VkPipelineStageFlags        timingPreSample;
        timingPostSample, //  VkPipelineStageFlags        timingPostSample;
    };

    // Prepare resources and pipeline.
    using BufferWithMemoryPtr = std::unique_ptr<BufferWithMemory>;
    using ImageWithBufferPtr  = std::unique_ptr<ImageWithBuffer>;

    const auto compWgSize    = params->getComputeWgSize();
    const auto compWgCount   = params->getComputeWgCount();
    const auto compItemCount = compWgSize * compWgCount;
    const auto colorExtent   = params->getExtent();
    const auto colorExtentVk = makeExtent3D(colorExtent);
    const auto colorFormat   = VK_FORMAT_R8G8B8A8_UNORM;
    const auto vertCount     = colorExtent.x() * colorExtent.y() * colorExtent.z() * 3; // One triangle per pixel.
    const auto &binaries     = context.getBinaryCollection();

    BufferWithMemoryPtr compBuffer;
    BufferWithMemoryPtr vertBuffer;
    ImageWithBufferPtr colorBuffer;

    Move<VkDescriptorPool> descriptorPool;
    Move<VkDescriptorSetLayout> descriptorSetLayout;
    Move<VkPipelineLayout> pipelineLayout;
    Move<VkDescriptorSet> descriptorSet;

    Move<VkRenderPass> renderPass;
    Move<VkFramebuffer> framebuffer;

    Move<VkShaderModule> compShader;
    Move<VkShaderModule> vertShader;
    Move<VkShaderModule> tescShader;
    Move<VkShaderModule> teseShader;
    Move<VkShaderModule> geomShader;
    Move<VkShaderModule> fragShader;
    Move<VkPipeline> pipeline;

    if (params->isCompute())
    {
        const auto descType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        DescriptorPoolBuilder poolBuilder;
        poolBuilder.addType(descType);
        descriptorPool = poolBuilder.build(ctx.vkd, ctx.device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);

        DescriptorSetLayoutBuilder setLayoutBuilder;
        setLayoutBuilder.addSingleBinding(descType, params->stageFlags);
        descriptorSetLayout = setLayoutBuilder.build(ctx.vkd, ctx.device);
        pipelineLayout      = makePipelineLayout(ctx.vkd, ctx.device, *descriptorSetLayout);
        descriptorSet       = makeDescriptorSet(ctx.vkd, ctx.device, *descriptorPool, *descriptorSetLayout);

        const auto compBufferSize  = static_cast<VkDeviceSize>(compItemCount * DE_SIZEOF32(uint32_t));
        const auto compBufferUsage = static_cast<VkBufferUsageFlags>(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
        const auto compBufferInfo  = makeBufferCreateInfo(compBufferSize, compBufferUsage);
        compBuffer.reset(new BufferWithMemory(ctx.vkd, ctx.device, ctx.allocator, compBufferInfo, HostIntent::RW));
        auto &alloc = compBuffer->getAllocation();
        memset(alloc.getHostPtr(), 0, static_cast<size_t>(compBufferSize));
        flushAlloc(ctx.vkd, ctx.device, alloc);

        DescriptorSetUpdateBuilder setUpdateBuilder;
        const auto descInfo = makeDescriptorBufferInfo(compBuffer->get(), 0ull, VK_WHOLE_SIZE);
        setUpdateBuilder.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u), descType,
                                     &descInfo);
        setUpdateBuilder.update(ctx.vkd, ctx.device);

        compShader = createShaderModule(ctx.vkd, ctx.device, binaries.get("comp"));
        pipeline   = makeComputePipeline(ctx.vkd, ctx.device, *pipelineLayout, *compShader);
    }
    else
    {
        const auto colorUsage =
            static_cast<VkImageUsageFlags>(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
        colorBuffer.reset(new ImageWithBuffer(ctx.vkd, ctx.device, ctx.allocator, colorExtentVk, colorFormat,
                                              colorUsage, VK_IMAGE_TYPE_2D));

        renderPass  = makeRenderPass(ctx.vkd, ctx.device, colorFormat);
        framebuffer = makeFramebuffer(ctx.vkd, ctx.device, *renderPass, colorBuffer->getImageView(),
                                      colorExtentVk.width, colorExtentVk.height);

        // One triangle per pixel.
        std::vector<tcu::Vec4> vertices;
        vertices.reserve(vertCount);

        const auto floatExtent  = colorExtent.asFloat();
        const auto pixelWidth   = 2.0f / floatExtent.x();
        const auto pixelHeight  = 2.0f / floatExtent.y();
        const auto widthMargin  = pixelWidth / 4.0f;
        const auto heightMargin = pixelHeight / 4.0f;

        for (int y = 0; y < colorExtent.y(); ++y)
            for (int x = 0; x < colorExtent.x(); ++x)
            {
                // Normalized pixel center.
                const auto xCenter = ((static_cast<float>(x) + 0.5f) / floatExtent.x()) * 2.0f - 1.0f;
                const auto yCenter = ((static_cast<float>(y) + 0.5f) / floatExtent.y()) * 2.0f - 1.0f;

                vertices.push_back(tcu::Vec4(xCenter, yCenter - heightMargin, 0.0f, 1.0f)); // Top
                vertices.push_back(
                    tcu::Vec4(xCenter - widthMargin, yCenter + heightMargin, 0.0f, 1.0f)); // Bottom left.
                vertices.push_back(
                    tcu::Vec4(xCenter + widthMargin, yCenter + heightMargin, 0.0f, 1.0f)); // Bottom right.
            }

        const auto vertBufferSize  = static_cast<VkDeviceSize>(de::dataSize(vertices));
        const auto vertBufferUsage = static_cast<VkBufferUsageFlags>(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
        const auto vertBufferInfo  = makeBufferCreateInfo(vertBufferSize, vertBufferUsage);
        vertBuffer.reset(new BufferWithMemory(ctx.vkd, ctx.device, ctx.allocator, vertBufferInfo, HostIntent::W));
        auto &alloc = vertBuffer->getAllocation();
        memcpy(alloc.getHostPtr(), de::dataOrNull(vertices), de::dataSize(vertices));
        flushAlloc(ctx.vkd, ctx.device, alloc);

        const auto hasTess = params->hasShaderStage(VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT);
        const auto hasGeom = params->hasShaderStage(VK_SHADER_STAGE_GEOMETRY_BIT);

        vertShader = createShaderModule(ctx.vkd, ctx.device, binaries.get("vert"));
        tescShader = (hasTess ? createShaderModule(ctx.vkd, ctx.device, binaries.get("tesc")) : Move<VkShaderModule>());
        teseShader = (hasTess ? createShaderModule(ctx.vkd, ctx.device, binaries.get("tese")) : Move<VkShaderModule>());
        geomShader = (hasGeom ? createShaderModule(ctx.vkd, ctx.device, binaries.get("geom")) : Move<VkShaderModule>());
        fragShader = createShaderModule(ctx.vkd, ctx.device, binaries.get("frag"));

        const std::vector<VkViewport> viewports(1u, makeViewport(colorExtent));
        const std::vector<VkRect2D> scissors(1u, makeRect2D(colorExtent));

        const auto topology = (hasTess ? VK_PRIMITIVE_TOPOLOGY_PATCH_LIST : VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
        const auto patchControlPoints = 3u;
        pipelineLayout                = makePipelineLayout(ctx.vkd, ctx.device);

        pipeline = makeGraphicsPipeline(ctx.vkd, ctx.device, *pipelineLayout, *vertShader, *tescShader, *teseShader,
                                        *geomShader, *fragShader, *renderPass, viewports, scissors, topology, 0u,
                                        patchControlPoints);
    }

    beginCommandBuffer(ctx.vkd, cmdBuffer);
    cmdBeginGpaSessionAMD(ctx.vkd, cmdBuffer, *session);
    const auto sampleId = cmdBeginGpaSampleAMD(ctx.vkd, cmdBuffer, *session, &sampleBeginInfo);
    if (params->isCompute())
    {
        const auto bindPoint = VK_PIPELINE_BIND_POINT_COMPUTE;
        ctx.vkd.cmdBindPipeline(cmdBuffer, bindPoint, *pipeline);
        ctx.vkd.cmdBindDescriptorSets(cmdBuffer, bindPoint, *pipelineLayout, 0u, 1u, &descriptorSet.get(), 0u, nullptr);
        ctx.vkd.cmdDispatch(cmdBuffer, compWgCount, 1u, 1u);
        const auto hostBarrier = makeMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
        cmdPipelineMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                                 &hostBarrier);
    }
    else
    {
        const auto renderArea       = makeRect2D(colorExtent);
        const auto clearColor       = params->getClearColor();
        const auto vertBufferOffset = static_cast<VkDeviceSize>(0);

        beginRenderPass(ctx.vkd, cmdBuffer, *renderPass, *framebuffer, renderArea, clearColor);
        ctx.vkd.cmdBindVertexBuffers(cmdBuffer, 0u, 1u, &vertBuffer->get(), &vertBufferOffset);
        ctx.vkd.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline);
        ctx.vkd.cmdDraw(cmdBuffer, static_cast<uint32_t>(vertCount), 1u, 0u, 0u);
        endRenderPass(ctx.vkd, cmdBuffer);
        copyImageToBuffer(ctx.vkd, cmdBuffer, colorBuffer->getImage(), colorBuffer->getBuffer(),
                          colorExtent.swizzle(0, 1));
    }
    cmdEndGpaSampleAMD(ctx.vkd, cmdBuffer, *session, sampleId);
    cmdEndGpaSessionAMD(ctx.vkd, cmdBuffer, *session);
    endCommandBuffer(ctx.vkd, cmdBuffer);
    submitCommandsAndWait(ctx.vkd, ctx.device, ctx.queue, cmdBuffer);

    const auto sessionStatus = ctx.vkd.getGpaSessionStatusAMD(ctx.device, *session);
    if (sessionStatus != VK_SUCCESS)
    {
        std::ostringstream msg;
        msg << "Unexpected session status: " << getResultName(sessionStatus);
        TCU_FAIL(msg.str());
    }

    size_t resultBytes = 0;
    VK_CHECK(ctx.vkd.getGpaSessionResultsAMD(ctx.device, *session, sampleId, &resultBytes, nullptr));
    if (resultBytes > 0)
    {
        std::vector<uint8_t> resultBuffer(resultBytes);
        VK_CHECK(ctx.vkd.getGpaSessionResultsAMD(ctx.device, *session, sampleId, &resultBytes,
                                                 de::dataOrNull(resultBuffer)));

        if (resultBytes != resultBuffer.size())
        {
            std::ostringstream msg;
            msg << "Amount of result bytes changed between vkGetGpaSessionResultsAMD calls: " << resultBuffer.size()
                << " to " << resultBytes;
            TCU_FAIL(msg.str());
        }
    }

    auto &log = context.getTestContext().getLog();
    bool fail = false;

    if (params->isCompute())
    {
        const std::vector<uint32_t> compRef(compItemCount, 1u);
        std::vector<uint32_t> compRes(compItemCount, 0u);
        invalidateAlloc(ctx.vkd, ctx.device, compBuffer->getAllocation());
        memcpy(de::dataOrNull(compRes), compBuffer->getAllocation().getHostPtr(), de::dataSize(compRes));

        for (uint32_t i = 0u; i < compItemCount; ++i)
        {
            const auto &res = compRes.at(i);
            const auto &ref = compRef.at(i);

            if (res != ref)
            {
                std::ostringstream msg;
                msg << "Unexpected value at position " << i << ": expected " << ref << " but got " << res;
                log << tcu::TestLog::Message << msg.str() << tcu::TestLog::EndMessage;
                fail = true;
            }
        }
    }
    else
    {
        const auto tcuFormat = mapVkFormat(colorFormat);
        tcu::TextureLevel refLevel(tcuFormat, colorExtent.x(), colorExtent.y(), colorExtent.z());
        auto reference        = refLevel.getAccess();
        const auto finalColor = params->getFinalColor();
        tcu::clear(reference, finalColor);

        auto &alloc = colorBuffer->getBufferAllocation();
        invalidateAlloc(ctx.vkd, ctx.device, alloc);

        tcu::ConstPixelBufferAccess result(tcuFormat, colorExtent, alloc.getHostPtr());

        const tcu::Vec4 threshold(0.0f);
        if (!tcu::floatThresholdCompare(log, "Color", "", reference, result, threshold, tcu::COMPARE_LOG_ON_ERROR))
            fail = true;
    }

    if (fail)
        TCU_FAIL("Unexpected results; check log for details --");

    return tcu::TestStatus::pass("Pass");
}

} // anonymous namespace

tcu::TestCaseGroup *createGpaInterfaceTests(tcu::TestContext &testCtx)
{
    return createTestGroup2(
        testCtx, "gpa_interface",
        [](tcu::TestCaseGroup *root)
        {
            addFunctionCase(root, "device_clock_info", checkBasicSupport, testDeviceClockInfo);
            addFunctionCase(root, "create_destroy_basic", checkBasicSupport, testCreateDestroySession);
            addFunctionCase(root, "create_destroy_copy", checkBasicSupport, testCreateDestroySessionWithCopy);
            addFunctionCase(root, "empty_sessions", checkBasicSupport, testEmptySessions);
            addFunctionCase(root, "created_status", checkBasicSupport, testCreatedStatus);
            addFunctionCase(root, "unfinished_status", checkBasicSupport, testUnfinishedStatus);
            addFunctionCase(root, "empty_sessions_multi_cmd", checkBasicSupport, testEmptySessionsMultiCmd);
            addFunctionCase(root, "empty_secondary_with_copy", checkGpaAndSync2, testEmptySecondaryWithCopy);
            addFunctionCase(root, "reset_empty_session", checkBasicSupport, testResetEmptySession);

            struct
            {
                VkShaderStageFlags shaderStages;
                std::string name;
            } regularSessionStages[] = {
                {static_cast<VkShaderStageFlags>(VK_SHADER_STAGE_COMPUTE_BIT), "comp"},
                {static_cast<VkShaderStageFlags>(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT),
                 "vert_frag"},
                {static_cast<VkShaderStageFlags>(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT |
                                                 VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT |
                                                 VK_SHADER_STAGE_FRAGMENT_BIT),
                 "vert_tess_frag"},
                {static_cast<VkShaderStageFlags>(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT |
                                                 VK_SHADER_STAGE_FRAGMENT_BIT),
                 "vert_geom_frag"},
                {static_cast<VkShaderStageFlags>(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT |
                                                 VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT |
                                                 VK_SHADER_STAGE_GEOMETRY_BIT | VK_SHADER_STAGE_FRAGMENT_BIT),
                 "vert_tess_geom_frag"},
            };
            for (const auto &stagesAndName : regularSessionStages)
                for (const auto timing : {false, true})
                {
                    const auto testName = "regular_session_" + stagesAndName.name + (timing ? "_timing" : "");
                    RegularSessionParamsPtr params(new RegularSessionParams{stagesAndName.shaderStages, timing});
                    addFunctionCaseWithPrograms(root, testName, checkRegularSupport, initRegularPrograms,
                                                testRegularSession, params);
                }
        });
}

} // namespace vkt::api
