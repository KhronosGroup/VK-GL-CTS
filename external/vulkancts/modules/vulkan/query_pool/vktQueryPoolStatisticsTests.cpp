/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2016 The Khronos Group Inc.
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
 * \brief Vulkan Statistics Query Tests
 *//*--------------------------------------------------------------------*/

#include "vktQueryPoolStatisticsTests.hpp"
#include "vktTestCase.hpp"

#include "vktDrawImageObjectUtil.hpp"
#include "vktDrawBufferObjectUtil.hpp"
#include "vktDrawCreateInfoUtil.hpp"
#include "vktCustomInstancesDevices.hpp"
#include "vkBuilderUtil.hpp"
#include "vkRefUtil.hpp"
#include "vkPrograms.hpp"
#include "vkTypeUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"
#ifdef CTS_USES_VULKANSC
#include "vkSafetyCriticalUtil.hpp"
#endif // CTS_USES_VULKANSC
#include "vkBarrierUtil.hpp"
#include "vkDeviceUtil.hpp"

#include "deMath.h"

#include "tcuTestLog.hpp"
#include "tcuImageCompare.hpp"
#include "vkImageUtil.hpp"
#include "tcuCommandLine.hpp"
#include "tcuRGBA.hpp"
#include "tcuStringTemplate.hpp"

#include <vector>
#include <utility>
#include <algorithm>
#include <numeric>

using std::pair;
using std::vector;

namespace vkt
{
namespace QueryPool
{
namespace
{

using namespace vk;
using namespace Draw;

constexpr uint32_t kFloatSize = static_cast<uint32_t>(sizeof(float));

//Test parameters
enum
{
    WIDTH  = 64,
    HEIGHT = 64
};

enum ResetType
{
    RESET_TYPE_NORMAL = 0,
    RESET_TYPE_HOST,
    RESET_TYPE_BEFORE_COPY,
    RESET_TYPE_AFTER_COPY,
    RESET_TYPE_LAST
};

enum CopyType
{
    COPY_TYPE_GET = 0,
    COPY_TYPE_CMD,
};

enum StrideType
{
    STRIDE_TYPE_VALID = 0,
    STRIDE_TYPE_ZERO,
};

enum CommandBufferType
{
    PRIMARY,
    SECONDARY,
    SECONDARY_INHERITED
};

enum ClearOperation
{
    CLEAR_NOOP,
    CLEAR_COLOR,
    CLEAR_DEPTH,
    CLEAR_SKIP,
};

enum TessPrimitiveMode
{
    TESS_PRIM_TRIANGLES,
    TESS_PRIM_ISOLINES,
    TESS_PRIM_QUADS
};

constexpr uint32_t kTriangleVertices         = 3u;
constexpr uint32_t kMaxTessellationPatchSize = 32u;

std::string inputTypeToGLString(const VkPrimitiveTopology &inputType)
{
    switch (inputType)
    {
    case VK_PRIMITIVE_TOPOLOGY_POINT_LIST:
        return "points";
    case VK_PRIMITIVE_TOPOLOGY_LINE_LIST:
    case VK_PRIMITIVE_TOPOLOGY_LINE_STRIP:
        return "lines";
    case VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY:
    case VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY:
        return "lines_adjacency";
    case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST:
    case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP:
    case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN:
    case VK_PRIMITIVE_TOPOLOGY_PATCH_LIST:
        return "triangles";
    case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY:
    case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY:
        return "triangles_adjacency";
    default:
        DE_ASSERT(false);
        return "error";
    }
}

std::string outputTypeToGLString(const VkPrimitiveTopology &outputType)
{
    switch (outputType)
    {
    case VK_PRIMITIVE_TOPOLOGY_POINT_LIST:
        return "points";
    case VK_PRIMITIVE_TOPOLOGY_LINE_STRIP:
    case VK_PRIMITIVE_TOPOLOGY_LINE_LIST:
    case VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY:
    case VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY:
        return "line_strip";
    case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP:
    case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST:
    case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN:
    case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY:
    case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY:
    case VK_PRIMITIVE_TOPOLOGY_PATCH_LIST:
        return "triangle_strip";
    default:
        DE_ASSERT(false);
        return "error";
    }
}

using Pair32                        = pair<uint32_t, uint32_t>;
using Pair64                        = pair<uint64_t, uint64_t>;
using ResultsVector                 = vector<uint64_t>;
using ResultsVectorWithAvailability = vector<Pair64>;

// Get query pool results as a vector. Note results are always converted to
// uint64_t, but the actual vkGetQueryPoolResults call will use the 64-bits flag
// or not depending on your preferences.
vk::VkResult GetQueryPoolResultsVector(ResultsVector &output, const DeviceInterface &vk, vk::VkDevice device,
                                       vk::VkQueryPool queryPool, uint32_t firstQuery, uint32_t queryCount,
                                       VkQueryResultFlags flags)
{
    if (flags & vk::VK_QUERY_RESULT_WITH_AVAILABILITY_BIT)
        TCU_THROW(InternalError, "Availability flag passed when expecting results as ResultsVector");

    vk::VkResult result;
    output.resize(queryCount);

    if (flags & vk::VK_QUERY_RESULT_64_BIT)
    {
        constexpr size_t stride = sizeof(ResultsVector::value_type);
        const size_t totalSize  = stride * output.size();
        result =
            vk.getQueryPoolResults(device, queryPool, firstQuery, queryCount, totalSize, output.data(), stride, flags);
    }
    else
    {
        using IntermediateVector = vector<uint32_t>;

        IntermediateVector intermediate(queryCount);

        // Try to preserve existing data if possible.
        std::transform(begin(output), end(output), begin(intermediate),
                       [](uint64_t v) { return static_cast<uint32_t>(v); });

        constexpr size_t stride = sizeof(decltype(intermediate)::value_type);
        const size_t totalSize  = stride * intermediate.size();

        // Get and copy results.
        result = vk.getQueryPoolResults(device, queryPool, firstQuery, queryCount, totalSize, intermediate.data(),
                                        stride, flags);
        std::copy(begin(intermediate), end(intermediate), begin(output));
    }

    return result;
}

// Same as the normal GetQueryPoolResultsVector but returning the availability
// bit associated to each query in addition to the query value.
vk::VkResult GetQueryPoolResultsVector(ResultsVectorWithAvailability &output, const DeviceInterface &vk,
                                       vk::VkDevice device, vk::VkQueryPool queryPool, uint32_t firstQuery,
                                       uint32_t queryCount, VkQueryResultFlags flags)
{
    flags |= vk::VK_QUERY_RESULT_WITH_AVAILABILITY_BIT;

    vk::VkResult result;
    output.resize(queryCount);

    if (flags & vk::VK_QUERY_RESULT_64_BIT)
    {
        constexpr size_t stride = sizeof(ResultsVectorWithAvailability::value_type);
        const size_t totalSize  = stride * output.size();
        result =
            vk.getQueryPoolResults(device, queryPool, firstQuery, queryCount, totalSize, output.data(), stride, flags);
    }
    else
    {
        using IntermediateVector = vector<Pair32>;

        IntermediateVector intermediate(queryCount);

        // Try to preserve existing output data if possible.
        std::transform(begin(output), end(output), begin(intermediate),
                       [](const Pair64 &p) {
                           return Pair32{static_cast<uint32_t>(p.first), static_cast<uint32_t>(p.second)};
                       });

        constexpr size_t stride = sizeof(decltype(intermediate)::value_type);
        const size_t totalSize  = stride * intermediate.size();

        // Get and copy.
        result = vk.getQueryPoolResults(device, queryPool, firstQuery, queryCount, totalSize, intermediate.data(),
                                        stride, flags);
        std::transform(begin(intermediate), end(intermediate), begin(output),
                       [](const Pair32 &p) {
                           return Pair64{p.first, p.second};
                       });
    }

    return result;
}

// Get query pool results as a vector. Note results are always converted to
// uint64_t, but the actual vkCmdCopyQueryPoolResults call will use the 64-bits flag
// or not depending on your preferences.
void cmdCopyQueryPoolResultsVector(ResultsVector &output, const DeviceInterface &vk, vk::VkDevice device,
                                   const vk::Allocation &allocation, uint32_t queryCount, VkQueryResultFlags flags,
                                   bool dstOffset)
{
    if (flags & vk::VK_QUERY_RESULT_WITH_AVAILABILITY_BIT)
        TCU_THROW(InternalError, "Availability flag passed when expecting results as ResultsVector");

    output.resize(queryCount);

    void *allocationData = allocation.getHostPtr();
    vk::invalidateAlloc(vk, device, allocation);

    if (flags & vk::VK_QUERY_RESULT_64_BIT)
    {
        constexpr size_t stride = sizeof(ResultsVector::value_type);
        const size_t totalSize  = stride * output.size();
        const uint32_t offset   = dstOffset ? 1u : 0u;
        deMemcpy(output.data(), (reinterpret_cast<ResultsVector::value_type *>(allocationData) + offset), totalSize);
    }
    else
    {
        using IntermediateVector = vector<uint32_t>;

        IntermediateVector intermediate(queryCount);

        // Try to preserve existing data if possible.
        std::transform(begin(output), end(output), begin(intermediate),
                       [](uint64_t v) { return static_cast<uint32_t>(v); });

        constexpr size_t stride = sizeof(decltype(intermediate)::value_type);
        const size_t totalSize  = stride * intermediate.size();
        const uint32_t offset   = dstOffset ? 1u : 0u;
        // Get and copy results.
        deMemcpy(intermediate.data(), (reinterpret_cast<decltype(intermediate)::value_type *>(allocationData) + offset),
                 totalSize);
        std::copy(begin(intermediate), end(intermediate), begin(output));
    }
}

// Same as the normal cmdCopyQueryPoolResultsVector but returning the availability
// bit associated to each query in addition to the query value.
void cmdCopyQueryPoolResultsVector(ResultsVectorWithAvailability &output, const DeviceInterface &vk,
                                   vk::VkDevice device, const vk::Allocation &allocation, uint32_t queryCount,
                                   VkQueryResultFlags flags, bool dstOffset)
{
    flags |= vk::VK_QUERY_RESULT_WITH_AVAILABILITY_BIT;

    output.resize(queryCount);

    void *allocationData = allocation.getHostPtr();
    vk::invalidateAlloc(vk, device, allocation);

    if (flags & vk::VK_QUERY_RESULT_64_BIT)
    {
        constexpr size_t stride = sizeof(ResultsVectorWithAvailability::value_type);
        const size_t totalSize  = stride * output.size();
        const uint32_t offset   = dstOffset ? 1u : 0u;
        deMemcpy(output.data(),
                 (reinterpret_cast<ResultsVectorWithAvailability::value_type *>(allocationData) + offset), totalSize);
    }
    else
    {
        using IntermediateVector = vector<Pair32>;

        IntermediateVector intermediate(queryCount);

        // Try to preserve existing output data if possible.
        std::transform(begin(output), end(output), begin(intermediate),
                       [](const Pair64 &p) {
                           return Pair32{static_cast<uint32_t>(p.first), static_cast<uint32_t>(p.second)};
                       });

        constexpr size_t stride = sizeof(decltype(intermediate)::value_type);
        const size_t totalSize  = stride * intermediate.size();
        const uint32_t offset   = dstOffset ? 1u : 0u;

        // Get and copy.
        deMemcpy(intermediate.data(), (reinterpret_cast<decltype(intermediate)::value_type *>(allocationData) + offset),
                 totalSize);
        std::transform(begin(intermediate), end(intermediate), begin(output),
                       [](const Pair32 &p) {
                           return Pair64{p.first, p.second};
                       });
    }
}

// Generic parameters structure.
struct GenericParameters
{
    ResetType resetType;
    CopyType copyType;
    bool query64Bits;
    bool dstOffset;
    StrideType strideType;

    GenericParameters(ResetType resetType_, CopyType copyType_, bool query64Bits_, bool dstOffset_,
                      StrideType strideType_)
        : resetType{resetType_}
        , copyType{copyType_}
        , query64Bits{query64Bits_}
        , dstOffset{dstOffset_}
        , strideType{strideType_}
    {
    }

    VkQueryResultFlags querySizeFlags() const
    {
        return (query64Bits ? static_cast<VkQueryResultFlags>(vk::VK_QUERY_RESULT_64_BIT) : 0u);
    }
};

void beginSecondaryCommandBuffer(
    const DeviceInterface &vk, const VkCommandBuffer commandBuffer, const VkQueryPipelineStatisticFlags queryFlags,
    const VkRenderPass renderPass = VK_NULL_HANDLE, const VkFramebuffer framebuffer = VK_NULL_HANDLE,
    const VkCommandBufferUsageFlags bufferUsageFlags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT)
{
    const VkCommandBufferInheritanceInfo secCmdBufInheritInfo = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO,
        nullptr,
        renderPass,              // renderPass
        0u,                      // subpass
        framebuffer,             // framebuffer
        VK_FALSE,                // occlusionQueryEnable
        (VkQueryControlFlags)0u, // queryFlags
        queryFlags,              // pipelineStatistics
    };

    const VkCommandBufferBeginInfo info = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, // VkStructureType sType;
        nullptr,                                     // const void* pNext;
        bufferUsageFlags,                            // VkCommandBufferUsageFlags flags;
        &secCmdBufInheritInfo,                       // const VkCommandBufferInheritanceInfo* pInheritanceInfo;
    };
    VK_CHECK(vk.beginCommandBuffer(commandBuffer, &info));
}

Move<VkQueryPool> makeQueryPool(const DeviceInterface &vk, const VkDevice device, uint32_t queryCount,
                                VkQueryPipelineStatisticFlags statisticFlags)
{
    const VkQueryPoolCreateInfo queryPoolCreateInfo = {
        VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO, // VkStructureType                    sType
        nullptr,                                  // const void*                        pNext
        (VkQueryPoolCreateFlags)0,                // VkQueryPoolCreateFlags            flags
        VK_QUERY_TYPE_PIPELINE_STATISTICS,        // VkQueryType                        queryType
        queryCount,                               // uint32_t                            entryCount
        statisticFlags,                           // VkQueryPipelineStatisticFlags    pipelineStatistics
    };
    return createQueryPool(vk, device, &queryPoolCreateInfo);
}

double calculatePearsonCorrelation(const std::vector<uint64_t> &x, const ResultsVector &y)
{
    // This function calculates Pearson correlation coefficient ( https://en.wikipedia.org/wiki/Pearson_correlation_coefficient )
    // Two statistical variables are linear ( == fully corellated ) when fabs( Pearson corelation coefficient ) == 1
    // Two statistical variables are independent when pearson corelation coefficient == 0
    // If fabs( Pearson coefficient ) is > 0.8 then these two variables are almost linear

    DE_ASSERT(x.size() == y.size());
    DE_ASSERT(x.size() > 1);

    // calculate mean values
    double xMean = 0.0, yMean = 0.0;
    for (uint32_t i = 0; i < x.size(); ++i)
    {
        xMean += static_cast<double>(x[i]);
        yMean += static_cast<double>(y[i]);
    }
    xMean /= static_cast<double>(x.size());
    yMean /= static_cast<double>(x.size());

    // calculate standard deviations
    double xS = 0.0, yS = 0.0;
    for (uint32_t i = 0; i < x.size(); ++i)
    {
        double xv = static_cast<double>(x[i]) - xMean;
        double yv = static_cast<double>(y[i]) - yMean;

        xS += xv * xv;
        yS += yv * yv;
    }
    xS = sqrt(xS / static_cast<double>(x.size() - 1));
    yS = sqrt(yS / static_cast<double>(x.size() - 1));

    // calculate Pearson coefficient
    double pearson = 0.0;
    for (uint32_t i = 0; i < x.size(); ++i)
    {
        double xv = (static_cast<double>(x[i]) - xMean) / xS;
        double yv = (static_cast<double>(y[i]) - yMean) / yS;
        pearson += xv * yv;
    }

    return pearson / static_cast<double>(x.size() - 1);
}

double calculatePearsonCorrelation(const std::vector<uint64_t> &x, const ResultsVectorWithAvailability &ya)
{
    ResultsVector y;
    for (const auto &elt : ya)
        y.push_back(elt.first);
    return calculatePearsonCorrelation(x, y);
}

using BufferPtr = de::SharedPtr<Buffer>;

void clearBuffer(const DeviceInterface &vk, const VkDevice device, const BufferPtr buffer,
                 const VkDeviceSize bufferSizeBytes)
{
    const std::vector<uint8_t> data((size_t)bufferSizeBytes, 0u);
    const Allocation &allocation = buffer->getBoundMemory();
    void *allocationData         = allocation.getHostPtr();
    invalidateAlloc(vk, device, allocation);
    deMemcpy(allocationData, &data[0], (size_t)bufferSizeBytes);
    flushAlloc(vk, device, allocation);
}

class StatisticQueryTestInstance : public TestInstance
{
public:
    StatisticQueryTestInstance(Context &context, uint32_t queryCount, bool dstOffset, bool useComputeQueue);

protected:
    struct ValueAndAvailability
    {
        uint64_t value;
        uint64_t availability;
    };

    VkDeviceSize m_resetBufferSize;
    BufferPtr m_resetBuffer;
    bool dstOffset;
    const bool m_useComputeQueue;

    virtual void checkExtensions(bool hostResetQueryEnabled);
    BufferPtr createResetBuffer(void) const;
    void fillResetBuffer(const BufferPtr &buffer) const;
    tcu::TestStatus verifyUnavailable();
};

BufferPtr StatisticQueryTestInstance::createResetBuffer(void) const
{
    return Buffer::createAndAlloc(m_context.getDeviceInterface(), m_context.getDevice(),
                                  BufferCreateInfo(m_resetBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT),
                                  m_context.getDefaultAllocator(), vk::MemoryRequirement::HostVisible);
}

void StatisticQueryTestInstance::fillResetBuffer(const BufferPtr &buffer) const
{
    const vk::Allocation &allocation = buffer->getBoundMemory();
    void *allocationData             = allocation.getHostPtr();
    deMemset(allocationData, 0xff, static_cast<size_t>(m_resetBufferSize));
    flushAlloc(m_context.getDeviceInterface(), m_context.getDevice(), allocation);
}

StatisticQueryTestInstance::StatisticQueryTestInstance(Context &context, uint32_t queryCount, bool dstOffset_,
                                                       bool useComputeQueue)
    : TestInstance(context)
    , m_resetBufferSize((queryCount + (dstOffset_ ? 1u : 0u)) * sizeof(ValueAndAvailability))
    , m_resetBuffer()
    , dstOffset(dstOffset_)
    , m_useComputeQueue(useComputeQueue)
{
    m_resetBuffer = createResetBuffer();
    fillResetBuffer(m_resetBuffer);
}

void StatisticQueryTestInstance::checkExtensions(bool hostResetQueryEnabled)
{
    if (!m_context.getDeviceFeatures().pipelineStatisticsQuery)
        throw tcu::NotSupportedError("Pipeline statistics queries are not supported");

    if (hostResetQueryEnabled == true)
    {
        // Check VK_EXT_host_query_reset is supported
        m_context.requireDeviceFunctionality("VK_EXT_host_query_reset");
        if (m_context.getHostQueryResetFeatures().hostQueryReset == VK_FALSE)
            throw tcu::NotSupportedError(
                std::string("Implementation doesn't support resetting queries from the host").c_str());
    }
}

tcu::TestStatus StatisticQueryTestInstance::verifyUnavailable()
{
    const vk::Allocation &allocation = m_resetBuffer->getBoundMemory();
    const void *allocationData       = allocation.getHostPtr();
    uint32_t size                    = dstOffset ? 2 : 1;
    std::vector<ValueAndAvailability> va;
    va.resize(size);

    vk::invalidateAlloc(m_context.getDeviceInterface(), m_context.getDevice(), allocation);
    deMemcpy(va.data(), allocationData, size * sizeof(ValueAndAvailability));

    bool failed = false;
    for (uint32_t idx = 0u; idx < size; idx++)
    {
        if (dstOffset && idx == 0)
        {
            // Check that the contents between 0 and dstOffset were not overwritten.
            failed |= va[idx].availability != 0xfffffffffffffffful || va[idx].value != 0xfffffffffffffffful;
            continue;
        }

        failed |= va[idx].availability != 0;
    }

    return failed ? tcu::TestStatus::fail("Availability bit nonzero after resetting query or dstOffset wrong values") :
                    tcu::TestStatus::pass("Pass");
}

class ComputeInvocationsTestInstance : public StatisticQueryTestInstance
{
public:
    struct ParametersCompute : public GenericParameters
    {
        ParametersCompute(const tcu::UVec3 &localSize_, const tcu::UVec3 &groupSize_, const std::string &shaderName_,
                          ResetType resetType_, CopyType copyType_, bool query64Bits_, bool dstOffset_,
                          StrideType strideType_, bool useComputeQueue_)
            : GenericParameters{resetType_, copyType_, query64Bits_, dstOffset_, strideType_}
            , localSize(localSize_)
            , groupSize(groupSize_)
            , shaderName(shaderName_)
            , useComputeQueue(useComputeQueue_)
        {
        }

        tcu::UVec3 localSize;
        tcu::UVec3 groupSize;
        std::string shaderName;
        const bool useComputeQueue;
    };
    ComputeInvocationsTestInstance(Context &context, const std::vector<ParametersCompute> &parameters);
    tcu::TestStatus iterate(void);

protected:
    virtual tcu::TestStatus executeTest(const VkCommandPool &cmdPool, const VkPipelineLayout pipelineLayout,
                                        const VkDescriptorSet &descriptorSet, const BufferPtr buffer,
                                        const VkDeviceSize bufferSizeBytes);
    uint32_t getComputeExecution(const ParametersCompute &parm) const
    {
        return parm.localSize.x() * parm.localSize.y() * parm.localSize.z() * parm.groupSize.x() * parm.groupSize.y() *
               parm.groupSize.z();
    }
    const std::vector<ParametersCompute> &m_parameters;
};

ComputeInvocationsTestInstance::ComputeInvocationsTestInstance(Context &context,
                                                               const std::vector<ParametersCompute> &parameters)
    : StatisticQueryTestInstance(context, 1u, parameters[0].dstOffset, parameters[0].useComputeQueue)
    , m_parameters(parameters)
{
}

tcu::TestStatus ComputeInvocationsTestInstance::iterate(void)
{
    // These should have the same value throughout the whole vector.
    const bool hostQueryReset = ((m_parameters[0].resetType == RESET_TYPE_HOST) ? true : false);

    checkExtensions(hostQueryReset);

    const uint32_t queueFamilyIndex = m_context.getDeviceQueueInfo(0u).familyIndex;
    const DeviceInterface &vk       = m_context.getDeviceInterface();
    const VkDevice device           = m_context.getDevice();
    uint32_t maxSize                = 0u;

    for (size_t parametersNdx = 0; parametersNdx < m_parameters.size(); ++parametersNdx)
        maxSize = deMaxu32(maxSize, getComputeExecution(m_parameters[parametersNdx]));

    const VkDeviceSize bufferSizeBytes = static_cast<VkDeviceSize>(
        deAlignSize(static_cast<size_t>(sizeof(uint32_t) * maxSize),
                    static_cast<size_t>(m_context.getDeviceProperties().limits.nonCoherentAtomSize)));
    BufferPtr buffer =
        Buffer::createAndAlloc(vk, device, BufferCreateInfo(bufferSizeBytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT),
                               m_context.getDefaultAllocator(), MemoryRequirement::HostVisible);

    const Unique<VkDescriptorSetLayout> descriptorSetLayout(
        DescriptorSetLayoutBuilder()
            .addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
            .build(vk, device));

    const Unique<VkPipelineLayout> pipelineLayout(makePipelineLayout(vk, device, *descriptorSetLayout));

    const Unique<VkDescriptorPool> descriptorPool(
        DescriptorPoolBuilder()
            .addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
            .build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u));

    const VkDescriptorSetAllocateInfo allocateParams = {
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, // VkStructureType sType;
        nullptr,                                        // const void* pNext;
        *descriptorPool,                                // VkDescriptorPool descriptorPool;
        1u,                                             // uint32_t setLayoutCount;
        &(*descriptorSetLayout),                        // const VkDescriptorSetLayout* pSetLayouts;
    };

    const Unique<VkDescriptorSet> descriptorSet(allocateDescriptorSet(vk, device, &allocateParams));
    const VkDescriptorBufferInfo descriptorInfo = {
        buffer->object(), //VkBuffer buffer;
        0ull,             //VkDeviceSize offset;
        bufferSizeBytes,  //VkDeviceSize range;
    };

    DescriptorSetUpdateBuilder()
        .writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u),
                     VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &descriptorInfo)
        .update(vk, device);

    const CmdPoolCreateInfo cmdPoolCreateInfo(queueFamilyIndex);
    const Unique<VkCommandPool> cmdPool(createCommandPool(vk, device, &cmdPoolCreateInfo));

    return executeTest(*cmdPool, *pipelineLayout, *descriptorSet, buffer, bufferSizeBytes);
}

tcu::TestStatus ComputeInvocationsTestInstance::executeTest(const VkCommandPool &cmdPool,
                                                            const VkPipelineLayout pipelineLayout,
                                                            const VkDescriptorSet &descriptorSet,
                                                            const BufferPtr buffer, const VkDeviceSize bufferSizeBytes)
{
    const DeviceInterface &vk                        = m_context.getDeviceInterface();
    const VkDevice device                            = m_context.getDevice();
    const VkQueue queue                              = m_context.getDeviceQueueInfo(0u).queue;
    const VkBufferMemoryBarrier computeFinishBarrier = {
        VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,                // VkStructureType sType;
        nullptr,                                                // const void* pNext;
        VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, // VkAccessFlags srcAccessMask;
        VK_ACCESS_HOST_READ_BIT,                                // VkAccessFlags dstAccessMask;
        VK_QUEUE_FAMILY_IGNORED,                                // uint32_t srcQueueFamilyIndex;
        VK_QUEUE_FAMILY_IGNORED,                                // uint32_t destQueueFamilyIndex;
        buffer->object(),                                       // VkBuffer buffer;
        0ull,                                                   // VkDeviceSize offset;
        bufferSizeBytes,                                        // VkDeviceSize size;
    };

    std::vector<tcu::TestStatus> statuses(m_parameters.size(), tcu::TestStatus(QP_TEST_RESULT_PASS, std::string()));
    auto updateStatus = [&](size_t index, const tcu::TestStatus &status) -> void
    {
#ifdef CTS_USES_VULKANSC
        if (statuses[index].getCode() == QP_TEST_RESULT_PASS)
            statuses[index] = status;
#else
        DE_UNREF(index);
        throw status;
#endif
    };

    for (size_t parametersNdx = 0u; parametersNdx < m_parameters.size(); ++parametersNdx)
        try
        {
            clearBuffer(vk, device, buffer, bufferSizeBytes);
            const Unique<VkShaderModule> shaderModule(createShaderModule(
                vk, device, m_context.getBinaryCollection().get(m_parameters[parametersNdx].shaderName),
                (VkShaderModuleCreateFlags)0u));

            const VkPipelineShaderStageCreateInfo pipelineShaderStageParams = {
                VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, // VkStructureType sType;
                nullptr,                                             // const void* pNext;
                (VkPipelineShaderStageCreateFlags)0u,                // VkPipelineShaderStageCreateFlags flags;
                VK_SHADER_STAGE_COMPUTE_BIT,                         // VkShaderStageFlagBits stage;
                *shaderModule,                                       // VkShaderModule module;
                "main",                                              // const char* pName;
                nullptr,                                             // const VkSpecializationInfo* pSpecializationInfo;
            };

            const VkComputePipelineCreateInfo pipelineCreateInfo = {
                VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO, // VkStructureType sType;
                nullptr,                                        // const void* pNext;
                (VkPipelineCreateFlags)0u,                      // VkPipelineCreateFlags flags;
                pipelineShaderStageParams,                      // VkPipelineShaderStageCreateInfo stage;
                pipelineLayout,                                 // VkPipelineLayout layout;
                VK_NULL_HANDLE,                                 // VkPipeline basePipelineHandle;
                0,                                              // int32_t basePipelineIndex;
            };
            const Unique<VkPipeline> pipeline(createComputePipeline(vk, device, VK_NULL_HANDLE, &pipelineCreateInfo));

            const Unique<VkCommandBuffer> cmdBuffer(
                allocateCommandBuffer(vk, device, cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));
            const Unique<VkQueryPool> queryPool(
                makeQueryPool(vk, device, 1u, VK_QUERY_PIPELINE_STATISTIC_COMPUTE_SHADER_INVOCATIONS_BIT));

            beginCommandBuffer(vk, *cmdBuffer);
            if (m_parameters[0].resetType != RESET_TYPE_HOST)
                vk.cmdResetQueryPool(*cmdBuffer, *queryPool, 0u, 1u);

            vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipeline);
            vk.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0u, 1u, &descriptorSet,
                                     0u, nullptr);

            vk.cmdBeginQuery(*cmdBuffer, *queryPool, 0u, (VkQueryControlFlags)0u);
            vk.cmdDispatch(*cmdBuffer, m_parameters[parametersNdx].groupSize.x(),
                           m_parameters[parametersNdx].groupSize.y(), m_parameters[parametersNdx].groupSize.z());
            vk.cmdEndQuery(*cmdBuffer, *queryPool, 0u);

            if (m_parameters[0].resetType == RESET_TYPE_BEFORE_COPY ||
                m_parameters[0].resetType == RESET_TYPE_AFTER_COPY || m_parameters[0].copyType == COPY_TYPE_CMD)
            {
                VkDeviceSize stride          = m_parameters[0].querySizeFlags() ? sizeof(uint64_t) : sizeof(uint32_t);
                vk::VkQueryResultFlags flags = m_parameters[0].querySizeFlags() | VK_QUERY_RESULT_WAIT_BIT;

                if (m_parameters[0].resetType == RESET_TYPE_HOST)
                {
                    flags |= VK_QUERY_RESULT_WITH_AVAILABILITY_BIT;
                    stride *= 2u;
                }

                if (m_parameters[0].resetType == RESET_TYPE_BEFORE_COPY)
                {
                    vk.cmdResetQueryPool(*cmdBuffer, *queryPool, 0u, 1u);
                    flags  = VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT;
                    stride = sizeof(ValueAndAvailability);
                }

                VkDeviceSize dstOffsetQuery = (m_parameters[0].dstOffset) ? stride : 0;
                VkDeviceSize copyStride     = stride;

                if (m_parameters[0].strideType == STRIDE_TYPE_ZERO)
                    copyStride = 0u;

                vk.cmdCopyQueryPoolResults(*cmdBuffer, *queryPool, 0, 1u, m_resetBuffer->object(), dstOffsetQuery,
                                           copyStride, flags);

                if (m_parameters[0].resetType == RESET_TYPE_AFTER_COPY)
                    vk.cmdResetQueryPool(*cmdBuffer, *queryPool, 0u, 1u);

                const VkBufferMemoryBarrier barrier = {
                    VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER, //  VkStructureType sType;
                    nullptr,                                 //  const void* pNext;
                    VK_ACCESS_TRANSFER_WRITE_BIT,            //  VkAccessFlags srcAccessMask;
                    VK_ACCESS_HOST_READ_BIT,                 //  VkAccessFlags dstAccessMask;
                    VK_QUEUE_FAMILY_IGNORED,                 //  uint32_t srcQueueFamilyIndex;
                    VK_QUEUE_FAMILY_IGNORED,                 //  uint32_t destQueueFamilyIndex;
                    m_resetBuffer->object(),                 //  VkBuffer buffer;
                    0u,                                      //  VkDeviceSize offset;
                    1u * stride + dstOffsetQuery,            //  VkDeviceSize size;
                };
                vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                                      (VkDependencyFlags)0, 0, nullptr, 1u, &barrier, 0, nullptr);
            }

            vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                                  (VkDependencyFlags)0u, 0u, nullptr, 1u, &computeFinishBarrier, 0u, nullptr);

            endCommandBuffer(vk, *cmdBuffer);

            m_context.getTestContext().getLog()
                << tcu::TestLog::Message
                << "Compute shader invocations: " << getComputeExecution(m_parameters[parametersNdx])
                << tcu::TestLog::EndMessage;

            if (m_parameters[0].resetType == RESET_TYPE_HOST)
                vk.resetQueryPool(device, *queryPool, 0u, 1u);

            // Wait for completion
            submitCommandsAndWait(vk, device, queue, *cmdBuffer);

            // Validate the results
            const Allocation &bufferAllocation = buffer->getBoundMemory();
            invalidateAlloc(vk, device, bufferAllocation);

            if (m_parameters[0].resetType == RESET_TYPE_NORMAL || m_parameters[0].resetType == RESET_TYPE_AFTER_COPY)
            {
                ResultsVector data;

                if (m_parameters[0].copyType == COPY_TYPE_CMD)
                {
                    const vk::Allocation &allocation = m_resetBuffer->getBoundMemory();
                    cmdCopyQueryPoolResultsVector(data, vk, device, allocation, 1u,
                                                  (VK_QUERY_RESULT_WAIT_BIT | m_parameters[0].querySizeFlags()),
                                                  m_parameters[0].dstOffset);
                }
                else
                {
                    VK_CHECK(GetQueryPoolResultsVector(data, vk, device, *queryPool, 0u, 1u,
                                                       (VK_QUERY_RESULT_WAIT_BIT | m_parameters[0].querySizeFlags())));
                }

                if (getComputeExecution(m_parameters[parametersNdx]) != data[0])
                    updateStatus(parametersNdx, tcu::TestStatus::fail("QueryPoolResults incorrect"));
            }
            else if (m_parameters[0].resetType == RESET_TYPE_HOST)
            {
                ResultsVectorWithAvailability data;

                if (m_parameters[0].copyType == COPY_TYPE_CMD)
                {
                    const vk::Allocation &allocation = m_resetBuffer->getBoundMemory();
                    cmdCopyQueryPoolResultsVector(data, vk, device, allocation, 1u,
                                                  (VK_QUERY_RESULT_WAIT_BIT | m_parameters[0].querySizeFlags() |
                                                   VK_QUERY_RESULT_WITH_AVAILABILITY_BIT),
                                                  m_parameters[0].dstOffset);
                }
                else
                {
                    VK_CHECK(GetQueryPoolResultsVector(data, vk, device, *queryPool, 0u, 1u,
                                                       (VK_QUERY_RESULT_WAIT_BIT | m_parameters[0].querySizeFlags() |
                                                        VK_QUERY_RESULT_WITH_AVAILABILITY_BIT)));
                }

                if (getComputeExecution(m_parameters[parametersNdx]) != data[0].first || data[0].second == 0)
                    updateStatus(parametersNdx, tcu::TestStatus::fail("QueryPoolResults incorrect"));

                uint64_t temp = data[0].first;

                vk.resetQueryPool(device, *queryPool, 0, 1u);
                vk::VkResult res = GetQueryPoolResultsVector(
                    data, vk, device, *queryPool, 0u, 1u,
                    (m_parameters[0].querySizeFlags() | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT));
                /* From Vulkan spec:
                 *
                 * If VK_QUERY_RESULT_WAIT_BIT and VK_QUERY_RESULT_PARTIAL_BIT are both not set then no result values are written to pData
                 * for queries that are in the unavailable state at the time of the call, and vkGetQueryPoolResults returns VK_NOT_READY.
                 * However, availability state is still written to pData for those queries if VK_QUERY_RESULT_WITH_AVAILABILITY_BIT is set.
                 */
                if (res != vk::VK_NOT_READY || data[0].first != temp || data[0].second != 0u)
                    updateStatus(parametersNdx, tcu::TestStatus::fail("QueryPoolResults incorrect reset"));
            }
            else
            {
                // With RESET_TYPE_BEFORE_COPY, we only need to verify the result after the copy include an availability bit set as zero.
                updateStatus(parametersNdx, verifyUnavailable());
            }

            const uint32_t *bufferPtr = static_cast<uint32_t *>(bufferAllocation.getHostPtr());
            for (uint32_t ndx = 0u; ndx < getComputeExecution(m_parameters[parametersNdx]); ++ndx)
            {
                if (bufferPtr[ndx] != ndx)
                    updateStatus(parametersNdx,
                                 tcu::TestStatus::fail("Compute shader didn't write data to the buffer"));
            }
        }
        catch (const tcu::TestStatus &catchStatus)
        {
            return catchStatus;
        }
        catch (...)
        {
            throw;
        }

    for (const tcu::TestStatus &catchStatus : statuses)
    {
        if (catchStatus.getCode() != QP_TEST_RESULT_PASS)
            return catchStatus;
    }

    return tcu::TestStatus::pass("Pass");
}

class ComputeInvocationsSecondaryTestInstance : public ComputeInvocationsTestInstance
{
public:
    ComputeInvocationsSecondaryTestInstance(Context &context, const std::vector<ParametersCompute> &parameters);

protected:
    tcu::TestStatus executeTest(const VkCommandPool &cmdPool, const VkPipelineLayout pipelineLayout,
                                const VkDescriptorSet &descriptorSet, const BufferPtr buffer,
                                const VkDeviceSize bufferSizeBytes);
    virtual tcu::TestStatus checkResult(const BufferPtr buffer, const VkQueryPool queryPool);
};

ComputeInvocationsSecondaryTestInstance::ComputeInvocationsSecondaryTestInstance(
    Context &context, const std::vector<ParametersCompute> &parameters)
    : ComputeInvocationsTestInstance(context, parameters)
{
}

tcu::TestStatus ComputeInvocationsSecondaryTestInstance::executeTest(const VkCommandPool &cmdPool,
                                                                     const VkPipelineLayout pipelineLayout,
                                                                     const VkDescriptorSet &descriptorSet,
                                                                     const BufferPtr buffer,
                                                                     const VkDeviceSize bufferSizeBytes)
{
    typedef de::SharedPtr<Unique<VkShaderModule>> VkShaderModuleSp;
    typedef de::SharedPtr<Unique<VkPipeline>> VkPipelineSp;

    const DeviceInterface &vk = m_context.getDeviceInterface();
    const VkDevice device     = m_context.getDevice();
    const VkQueue queue       = m_context.getDeviceQueueInfo(0u).queue;

    const VkBufferMemoryBarrier computeShaderWriteBarrier = {
        VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,                // VkStructureType sType;
        nullptr,                                                // const void* pNext;
        VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, // VkAccessFlags srcAccessMask;
        VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, // VkAccessFlags dstAccessMask;
        VK_QUEUE_FAMILY_IGNORED,                                // uint32_t srcQueueFamilyIndex;
        VK_QUEUE_FAMILY_IGNORED,                                // uint32_t destQueueFamilyIndex;
        buffer->object(),                                       // VkBuffer buffer;
        0ull,                                                   // VkDeviceSize offset;
        bufferSizeBytes,                                        // VkDeviceSize size;
    };

    const VkBufferMemoryBarrier computeFinishBarrier = {
        VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,                // VkStructureType sType;
        nullptr,                                                // const void* pNext;
        VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, // VkAccessFlags srcAccessMask;
        VK_ACCESS_HOST_READ_BIT,                                // VkAccessFlags dstAccessMask;
        VK_QUEUE_FAMILY_IGNORED,                                // uint32_t srcQueueFamilyIndex;
        VK_QUEUE_FAMILY_IGNORED,                                // uint32_t destQueueFamilyIndex;
        buffer->object(),                                       // VkBuffer buffer;
        0ull,                                                   // VkDeviceSize offset;
        bufferSizeBytes,                                        // VkDeviceSize size;
    };

    std::vector<VkShaderModuleSp> shaderModule;
    std::vector<VkPipelineSp> pipeline;
    for (size_t parametersNdx = 0; parametersNdx < m_parameters.size(); ++parametersNdx)
    {
        shaderModule.push_back(VkShaderModuleSp(new Unique<VkShaderModule>(
            createShaderModule(vk, device, m_context.getBinaryCollection().get(m_parameters[parametersNdx].shaderName),
                               (VkShaderModuleCreateFlags)0u))));
        const VkPipelineShaderStageCreateInfo pipelineShaderStageParams = {
            VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, // VkStructureType sType;
            nullptr,                                             // const void* pNext;
            0u,                                                  // VkPipelineShaderStageCreateFlags flags;
            VK_SHADER_STAGE_COMPUTE_BIT,                         // VkShaderStageFlagBits stage;
            shaderModule.back().get()->get(),                    // VkShaderModule module;
            "main",                                              // const char* pName;
            nullptr,                                             // const VkSpecializationInfo* pSpecializationInfo;
        };

        const VkComputePipelineCreateInfo pipelineCreateInfo = {
            VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO, // VkStructureType sType;
            nullptr,                                        // const void* pNext;
            0u,                                             // VkPipelineCreateFlags flags;
            pipelineShaderStageParams,                      // VkPipelineShaderStageCreateInfo stage;
            pipelineLayout,                                 // VkPipelineLayout layout;
            VK_NULL_HANDLE,                                 // VkPipeline basePipelineHandle;
            0,                                              // int32_t basePipelineIndex;
        };
        pipeline.push_back(VkPipelineSp(
            new Unique<VkPipeline>(createComputePipeline(vk, device, VK_NULL_HANDLE, &pipelineCreateInfo))));
    }

    const Unique<VkCommandBuffer> primaryCmdBuffer(
        allocateCommandBuffer(vk, device, cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));
    const Unique<VkCommandBuffer> secondaryCmdBuffer(
        allocateCommandBuffer(vk, device, cmdPool, VK_COMMAND_BUFFER_LEVEL_SECONDARY));

    const Unique<VkQueryPool> queryPool(
        makeQueryPool(vk, device, 1u, VK_QUERY_PIPELINE_STATISTIC_COMPUTE_SHADER_INVOCATIONS_BIT));

    clearBuffer(vk, device, buffer, bufferSizeBytes);
    beginSecondaryCommandBuffer(vk, *secondaryCmdBuffer, VK_QUERY_PIPELINE_STATISTIC_COMPUTE_SHADER_INVOCATIONS_BIT);
    vk.cmdBindDescriptorSets(*secondaryCmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0u, 1u,
                             &descriptorSet, 0u, nullptr);
    if (m_parameters[0].resetType != RESET_TYPE_HOST)
        vk.cmdResetQueryPool(*secondaryCmdBuffer, *queryPool, 0u, 1u);
    vk.cmdBeginQuery(*secondaryCmdBuffer, *queryPool, 0u, (VkQueryControlFlags)0u);
    for (size_t parametersNdx = 0; parametersNdx < m_parameters.size(); ++parametersNdx)
    {
        vk.cmdBindPipeline(*secondaryCmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline[parametersNdx].get()->get());
        vk.cmdDispatch(*secondaryCmdBuffer, m_parameters[parametersNdx].groupSize.x(),
                       m_parameters[parametersNdx].groupSize.y(), m_parameters[parametersNdx].groupSize.z());

        vk.cmdPipelineBarrier(*secondaryCmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                              VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, (VkDependencyFlags)0u, 0u, nullptr, 1u,
                              &computeShaderWriteBarrier, 0u, nullptr);
    }
    vk.cmdEndQuery(*secondaryCmdBuffer, *queryPool, 0u);

    if (m_parameters[0].resetType == RESET_TYPE_BEFORE_COPY || m_parameters[0].resetType == RESET_TYPE_AFTER_COPY ||
        m_parameters[0].copyType == COPY_TYPE_CMD)
    {
        VkDeviceSize stride          = m_parameters[0].querySizeFlags() ? sizeof(uint64_t) : sizeof(uint32_t);
        vk::VkQueryResultFlags flags = m_parameters[0].querySizeFlags() | VK_QUERY_RESULT_WAIT_BIT;

        if (m_parameters[0].resetType == RESET_TYPE_HOST)
        {
            flags |= VK_QUERY_RESULT_WITH_AVAILABILITY_BIT;
            stride *= 2u;
        }

        if (m_parameters[0].resetType == RESET_TYPE_BEFORE_COPY)
        {
            vk.cmdResetQueryPool(*secondaryCmdBuffer, *queryPool, 0u, 1u);
            flags  = VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT;
            stride = sizeof(ValueAndAvailability);
        }

        VkDeviceSize dstOffsetQuery = (m_parameters[0].dstOffset) ? stride : 0;
        VkDeviceSize copyStride     = stride;
        if (m_parameters[0].strideType == STRIDE_TYPE_ZERO)
            copyStride = 0u;

        vk.cmdCopyQueryPoolResults(*secondaryCmdBuffer, *queryPool, 0, 1u, m_resetBuffer->object(), dstOffsetQuery,
                                   copyStride, flags);

        if (m_parameters[0].resetType == RESET_TYPE_AFTER_COPY)
            vk.cmdResetQueryPool(*secondaryCmdBuffer, *queryPool, 0u, 1u);

        const VkBufferMemoryBarrier barrier = {
            VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER, //  VkStructureType sType;
            nullptr,                                 //  const void* pNext;
            VK_ACCESS_TRANSFER_WRITE_BIT,            //  VkAccessFlags srcAccessMask;
            VK_ACCESS_HOST_READ_BIT,                 //  VkAccessFlags dstAccessMask;
            VK_QUEUE_FAMILY_IGNORED,                 //  uint32_t srcQueueFamilyIndex;
            VK_QUEUE_FAMILY_IGNORED,                 //  uint32_t destQueueFamilyIndex;
            m_resetBuffer->object(),                 //  VkBuffer buffer;
            0u,                                      //  VkDeviceSize offset;
            1u * stride + dstOffsetQuery,            //  VkDeviceSize size;
        };
        vk.cmdPipelineBarrier(*secondaryCmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                              (VkDependencyFlags)0, 0, nullptr, 1u, &barrier, 0, nullptr);
    }

    endCommandBuffer(vk, *secondaryCmdBuffer);

    beginCommandBuffer(vk, *primaryCmdBuffer);
    vk.cmdExecuteCommands(*primaryCmdBuffer, 1u, &secondaryCmdBuffer.get());

    vk.cmdPipelineBarrier(*primaryCmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                          (VkDependencyFlags)0u, 0u, nullptr, 1u, &computeFinishBarrier, 0u, nullptr);

    endCommandBuffer(vk, *primaryCmdBuffer);

    // Secondary buffer is emitted only once, so it is safe to reset the query pool here.
    if (m_parameters[0].resetType == RESET_TYPE_HOST)
        vk.resetQueryPool(device, *queryPool, 0u, 1u);

    // Wait for completion
    submitCommandsAndWait(vk, device, queue, *primaryCmdBuffer);
    return checkResult(buffer, *queryPool);
}

tcu::TestStatus ComputeInvocationsSecondaryTestInstance::checkResult(const BufferPtr buffer,
                                                                     const VkQueryPool queryPool)
{
    const DeviceInterface &vk = m_context.getDeviceInterface();
    const VkDevice device     = m_context.getDevice();
    {
        uint64_t expected = 0u;
        for (size_t parametersNdx = 0; parametersNdx < m_parameters.size(); ++parametersNdx)
            expected += getComputeExecution(m_parameters[parametersNdx]);

        if (m_parameters[0].resetType == RESET_TYPE_NORMAL || m_parameters[0].resetType == RESET_TYPE_AFTER_COPY)
        {
            ResultsVector results;
            if (m_parameters[0].copyType == COPY_TYPE_CMD)
            {
                const vk::Allocation &allocation = m_resetBuffer->getBoundMemory();
                cmdCopyQueryPoolResultsVector(results, vk, device, allocation, 1u,
                                              (VK_QUERY_RESULT_WAIT_BIT | m_parameters[0].querySizeFlags()),
                                              m_parameters[0].dstOffset);
            }
            else
            {
                VK_CHECK(GetQueryPoolResultsVector(results, vk, device, queryPool, 0u, 1u,
                                                   (VK_QUERY_RESULT_WAIT_BIT | m_parameters[0].querySizeFlags())));
            }

            if (expected != results[0])
                return tcu::TestStatus::fail("QueryPoolResults incorrect");
        }
        else if (m_parameters[0].resetType == RESET_TYPE_HOST)
        {
            ResultsVectorWithAvailability results;

            if (m_parameters[0].copyType == COPY_TYPE_CMD)
            {
                const vk::Allocation &allocation = m_resetBuffer->getBoundMemory();
                cmdCopyQueryPoolResultsVector(results, vk, device, allocation, 1u,
                                              (VK_QUERY_RESULT_WAIT_BIT | m_parameters[0].querySizeFlags() |
                                               VK_QUERY_RESULT_WITH_AVAILABILITY_BIT),
                                              m_parameters[0].dstOffset);
            }
            else
            {
                VK_CHECK(GetQueryPoolResultsVector(results, vk, device, queryPool, 0u, 1u,
                                                   (VK_QUERY_RESULT_WAIT_BIT | m_parameters[0].querySizeFlags() |
                                                    VK_QUERY_RESULT_WITH_AVAILABILITY_BIT)));
            }

            if (expected != results[0].first || results[0].second == 0u)
                return tcu::TestStatus::fail("QueryPoolResults incorrect");

            uint64_t temp = results[0].first;

            vk.resetQueryPool(device, queryPool, 0u, 1u);
            vk::VkResult res =
                GetQueryPoolResultsVector(results, vk, device, queryPool, 0u, 1u,
                                          (m_parameters[0].querySizeFlags() | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT));
            /* From Vulkan spec:
             *
             * If VK_QUERY_RESULT_WAIT_BIT and VK_QUERY_RESULT_PARTIAL_BIT are both not set then no result values are written to pData
             * for queries that are in the unavailable state at the time of the call, and vkGetQueryPoolResults returns VK_NOT_READY.
             * However, availability state is still written to pData for those queries if VK_QUERY_RESULT_WITH_AVAILABILITY_BIT is set.
             */
            if (res != vk::VK_NOT_READY || results[0].first != temp || results[0].second != 0u)
                return tcu::TestStatus::fail("QueryPoolResults incorrect reset");
        }
        else
        {
            // With RESET_TYPE_BEFORE_COPY, we only need to verify the result after the copy include an availability bit set as zero.
            return verifyUnavailable();
        }
    }

    {
        // Validate the results
        const Allocation &bufferAllocation = buffer->getBoundMemory();
        invalidateAlloc(vk, device, bufferAllocation);
        const uint32_t *bufferPtr = static_cast<uint32_t *>(bufferAllocation.getHostPtr());
        uint32_t minSize          = ~0u;
        for (size_t parametersNdx = 0; parametersNdx < m_parameters.size(); ++parametersNdx)
            minSize = deMinu32(minSize, getComputeExecution(m_parameters[parametersNdx]));
        for (uint32_t ndx = 0u; ndx < minSize; ++ndx)
        {
            if (bufferPtr[ndx] != ndx * m_parameters.size())
                return tcu::TestStatus::fail("Compute shader didn't write data to the buffer");
        }
    }
    return tcu::TestStatus::pass("Pass");
}

class ComputeInvocationsSecondaryInheritedTestInstance : public ComputeInvocationsSecondaryTestInstance
{
public:
    ComputeInvocationsSecondaryInheritedTestInstance(Context &context,
                                                     const std::vector<ParametersCompute> &parameters);

protected:
    virtual void checkExtensions(bool hostResetQueryEnabled);

    tcu::TestStatus executeTest(const VkCommandPool &cmdPool, const VkPipelineLayout pipelineLayout,
                                const VkDescriptorSet &descriptorSet, const BufferPtr buffer,
                                const VkDeviceSize bufferSizeBytes);
};

ComputeInvocationsSecondaryInheritedTestInstance::ComputeInvocationsSecondaryInheritedTestInstance(
    Context &context, const std::vector<ParametersCompute> &parameters)
    : ComputeInvocationsSecondaryTestInstance(context, parameters)
{
}

void ComputeInvocationsSecondaryInheritedTestInstance::checkExtensions(bool hostResetQueryEnabled)
{
    StatisticQueryTestInstance::checkExtensions(hostResetQueryEnabled);
    if (!m_context.getDeviceFeatures().inheritedQueries)
        throw tcu::NotSupportedError("Inherited queries are not supported");
}

tcu::TestStatus ComputeInvocationsSecondaryInheritedTestInstance::executeTest(const VkCommandPool &cmdPool,
                                                                              const VkPipelineLayout pipelineLayout,
                                                                              const VkDescriptorSet &descriptorSet,
                                                                              const BufferPtr buffer,
                                                                              const VkDeviceSize bufferSizeBytes)
{
    typedef de::SharedPtr<Unique<VkShaderModule>> VkShaderModuleSp;
    typedef de::SharedPtr<Unique<VkPipeline>> VkPipelineSp;

    const DeviceInterface &vk = m_context.getDeviceInterface();
    const VkDevice device     = m_context.getDevice();
    const VkQueue queue       = m_context.getDeviceQueueInfo(0u).queue;

    const VkBufferMemoryBarrier computeShaderWriteBarrier = {
        VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,                // VkStructureType sType;
        nullptr,                                                // const void* pNext;
        VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, // VkAccessFlags srcAccessMask;
        VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, // VkAccessFlags dstAccessMask;
        VK_QUEUE_FAMILY_IGNORED,                                // uint32_t srcQueueFamilyIndex;
        VK_QUEUE_FAMILY_IGNORED,                                // uint32_t destQueueFamilyIndex;
        buffer->object(),                                       // VkBuffer buffer;
        0ull,                                                   // VkDeviceSize offset;
        bufferSizeBytes,                                        // VkDeviceSize size;
    };

    const VkBufferMemoryBarrier computeFinishBarrier = {
        VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,                // VkStructureType sType;
        nullptr,                                                // const void* pNext;
        VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, // VkAccessFlags srcAccessMask;
        VK_ACCESS_HOST_READ_BIT,                                // VkAccessFlags dstAccessMask;
        VK_QUEUE_FAMILY_IGNORED,                                // uint32_t srcQueueFamilyIndex;
        VK_QUEUE_FAMILY_IGNORED,                                // uint32_t destQueueFamilyIndex;
        buffer->object(),                                       // VkBuffer buffer;
        0ull,                                                   // VkDeviceSize offset;
        bufferSizeBytes,                                        // VkDeviceSize size;
    };

    std::vector<VkShaderModuleSp> shaderModule;
    std::vector<VkPipelineSp> pipeline;
    for (size_t parametersNdx = 0u; parametersNdx < m_parameters.size(); ++parametersNdx)
    {
        shaderModule.push_back(VkShaderModuleSp(new Unique<VkShaderModule>(
            createShaderModule(vk, device, m_context.getBinaryCollection().get(m_parameters[parametersNdx].shaderName),
                               (VkShaderModuleCreateFlags)0u))));
        const VkPipelineShaderStageCreateInfo pipelineShaderStageParams = {
            VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, // VkStructureType sType;
            nullptr,                                             // const void* pNext;
            0u,                                                  // VkPipelineShaderStageCreateFlags flags;
            VK_SHADER_STAGE_COMPUTE_BIT,                         // VkShaderStageFlagBits stage;
            shaderModule.back().get()->get(),                    // VkShaderModule module;
            "main",                                              // const char* pName;
            nullptr,                                             // const VkSpecializationInfo* pSpecializationInfo;
        };

        const VkComputePipelineCreateInfo pipelineCreateInfo = {
            VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO, // VkStructureType sType;
            nullptr,                                        // const void* pNext;
            0u,                                             // VkPipelineCreateFlags flags;
            pipelineShaderStageParams,                      // VkPipelineShaderStageCreateInfo stage;
            pipelineLayout,                                 // VkPipelineLayout layout;
            VK_NULL_HANDLE,                                 // VkPipeline basePipelineHandle;
            0,                                              // int32_t basePipelineIndex;
        };
        pipeline.push_back(VkPipelineSp(
            new Unique<VkPipeline>(createComputePipeline(vk, device, VK_NULL_HANDLE, &pipelineCreateInfo))));
    }

    const Unique<VkCommandBuffer> primaryCmdBuffer(
        allocateCommandBuffer(vk, device, cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));
    const Unique<VkCommandBuffer> secondaryCmdBuffer(
        allocateCommandBuffer(vk, device, cmdPool, VK_COMMAND_BUFFER_LEVEL_SECONDARY));

    const Unique<VkQueryPool> queryPool(
        makeQueryPool(vk, device, 1u, VK_QUERY_PIPELINE_STATISTIC_COMPUTE_SHADER_INVOCATIONS_BIT));

    clearBuffer(vk, device, buffer, bufferSizeBytes);
    beginSecondaryCommandBuffer(vk, *secondaryCmdBuffer, VK_QUERY_PIPELINE_STATISTIC_COMPUTE_SHADER_INVOCATIONS_BIT);
    vk.cmdBindDescriptorSets(*secondaryCmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0u, 1u,
                             &descriptorSet, 0u, nullptr);
    for (size_t parametersNdx = 1; parametersNdx < m_parameters.size(); ++parametersNdx)
    {
        vk.cmdBindPipeline(*secondaryCmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline[parametersNdx].get()->get());
        vk.cmdDispatch(*secondaryCmdBuffer, m_parameters[parametersNdx].groupSize.x(),
                       m_parameters[parametersNdx].groupSize.y(), m_parameters[parametersNdx].groupSize.z());

        vk.cmdPipelineBarrier(*secondaryCmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                              VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, (VkDependencyFlags)0u, 0u, nullptr, 1u,
                              &computeShaderWriteBarrier, 0u, nullptr);
    }
    endCommandBuffer(vk, *secondaryCmdBuffer);

    beginCommandBuffer(vk, *primaryCmdBuffer);
    if (m_parameters[0].resetType != RESET_TYPE_HOST)
        vk.cmdResetQueryPool(*primaryCmdBuffer, *queryPool, 0u, 1u);
    vk.cmdBindDescriptorSets(*primaryCmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0u, 1u, &descriptorSet,
                             0u, nullptr);
    vk.cmdBindPipeline(*primaryCmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline[0].get()->get());

    vk.cmdBeginQuery(*primaryCmdBuffer, *queryPool, 0u, (VkQueryControlFlags)0u);
    vk.cmdDispatch(*primaryCmdBuffer, m_parameters[0].groupSize.x(), m_parameters[0].groupSize.y(),
                   m_parameters[0].groupSize.z());

    vk.cmdPipelineBarrier(*primaryCmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                          (VkDependencyFlags)0u, 0u, nullptr, 1u, &computeShaderWriteBarrier, 0u, nullptr);

    vk.cmdExecuteCommands(*primaryCmdBuffer, 1u, &secondaryCmdBuffer.get());

    vk.cmdPipelineBarrier(*primaryCmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                          (VkDependencyFlags)0u, 0u, nullptr, 1u, &computeFinishBarrier, 0u, nullptr);

    vk.cmdEndQuery(*primaryCmdBuffer, *queryPool, 0u);

    if (m_parameters[0].resetType == RESET_TYPE_BEFORE_COPY || m_parameters[0].resetType == RESET_TYPE_AFTER_COPY ||
        m_parameters[0].copyType == COPY_TYPE_CMD)
    {
        VkDeviceSize stride          = m_parameters[0].querySizeFlags() ? sizeof(uint64_t) : sizeof(uint32_t);
        vk::VkQueryResultFlags flags = m_parameters[0].querySizeFlags() | VK_QUERY_RESULT_WAIT_BIT;

        if (m_parameters[0].resetType == RESET_TYPE_HOST)
        {
            flags |= VK_QUERY_RESULT_WITH_AVAILABILITY_BIT;
            stride *= 2u;
        }

        if (m_parameters[0].resetType == RESET_TYPE_BEFORE_COPY)
        {
            vk.cmdResetQueryPool(*primaryCmdBuffer, *queryPool, 0u, 1u);
            flags  = VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT;
            stride = sizeof(ValueAndAvailability);
        }

        VkDeviceSize dstOffsetQuery = (m_parameters[0].dstOffset) ? stride : 0;
        VkDeviceSize copyStride     = stride;
        if (m_parameters[0].strideType == STRIDE_TYPE_ZERO)
            copyStride = 0u;

        vk.cmdCopyQueryPoolResults(*primaryCmdBuffer, *queryPool, 0, 1u, m_resetBuffer->object(), dstOffsetQuery,
                                   copyStride, flags);

        if (m_parameters[0].resetType == RESET_TYPE_AFTER_COPY)
            vk.cmdResetQueryPool(*primaryCmdBuffer, *queryPool, 0u, 1u);

        const VkBufferMemoryBarrier barrier = {
            VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER, //  VkStructureType sType;
            nullptr,                                 //  const void* pNext;
            VK_ACCESS_TRANSFER_WRITE_BIT,            //  VkAccessFlags srcAccessMask;
            VK_ACCESS_HOST_READ_BIT,                 //  VkAccessFlags dstAccessMask;
            VK_QUEUE_FAMILY_IGNORED,                 //  uint32_t srcQueueFamilyIndex;
            VK_QUEUE_FAMILY_IGNORED,                 //  uint32_t destQueueFamilyIndex;
            m_resetBuffer->object(),                 //  VkBuffer buffer;
            0u,                                      //  VkDeviceSize offset;
            1u * stride + dstOffsetQuery,            //  VkDeviceSize size;
        };
        vk.cmdPipelineBarrier(*primaryCmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                              (VkDependencyFlags)0, 0, nullptr, 1u, &barrier, 0, nullptr);
    }

    endCommandBuffer(vk, *primaryCmdBuffer);

    if (m_parameters[0].resetType == RESET_TYPE_HOST)
        vk.resetQueryPool(device, *queryPool, 0u, 1u);

    // Wait for completion
    submitCommandsAndWait(vk, device, queue, *primaryCmdBuffer);
    return checkResult(buffer, *queryPool);
}

class GraphicBasicTestInstance : public StatisticQueryTestInstance
{
public:
    struct VertexData
    {
        VertexData(const tcu::Vec4 position_, const tcu::Vec4 color_) : position(position_), color(color_)
        {
        }
        tcu::Vec4 position;
        tcu::Vec4 color;
    };

    struct ParametersGraphic : public GenericParameters
    {
        ParametersGraphic(const VkQueryPipelineStatisticFlags queryStatisticFlags_,
                          const VkPrimitiveTopology primitiveTopology_, const ResetType resetType_,
                          const CopyType copyType_, const bool query64Bits_, const bool vertexOnlyPipe_ = false,
                          const bool dstOffset_ = false, const ClearOperation clearOp_ = CLEAR_NOOP,
                          const bool noColorAttachments_ = false, const StrideType strideType_ = STRIDE_TYPE_VALID,
                          const bool hasTess_ = false, const uint32_t tessPatchSize_ = 0u,
                          const uint32_t numTessPrimitives_ = 1u, TessPrimitiveMode primMode_ = TESS_PRIM_QUADS,
                          const bool pointMode_ = false)
            : GenericParameters{resetType_, copyType_, query64Bits_, dstOffset_, strideType_}
            , queryStatisticFlags(queryStatisticFlags_)
            , primitiveTopology(primitiveTopology_)
            , vertexOnlyPipe(vertexOnlyPipe_)
            , clearOp(clearOp_)
            , noColorAttachments(noColorAttachments_)
            , hasTess(hasTess_)
            , tessPatchSize(tessPatchSize_)
            , numTessPrimitives(numTessPrimitives_)
            , primMode(primMode_)
            , pointMode(pointMode_)
        {
        }

        VkQueryPipelineStatisticFlags queryStatisticFlags;
        VkPrimitiveTopology primitiveTopology;
        bool vertexOnlyPipe;
        ClearOperation clearOp;
        bool noColorAttachments;
        bool hasTess;
        uint32_t tessPatchSize;
        uint32_t numTessPrimitives;
        TessPrimitiveMode primMode;
        bool pointMode;
    };
    GraphicBasicTestInstance(vkt::Context &context, const std::vector<VertexData> &data,
                             const ParametersGraphic &parametersGraphic, const std::vector<uint64_t> &drawRepeats);
    tcu::TestStatus iterate(void);

protected:
    BufferPtr creatAndFillVertexBuffer(void);
    virtual void createPipeline(void) = 0;
    void commandClearAttachment(const vk::DeviceInterface &vk, const vk::VkCommandBuffer commandBuffer);
    void creatColorAttachmentAndRenderPass(void);
    bool checkImage(void);
    virtual tcu::TestStatus executeTest(void)                  = 0;
    virtual tcu::TestStatus checkResult(VkQueryPool queryPool) = 0;
    virtual void draw(VkCommandBuffer cmdBuffer)               = 0;

    const VkFormat m_colorAttachmentFormat;
    de::SharedPtr<Image> m_colorAttachmentImage;
    de::SharedPtr<Image> m_depthImage;
    Move<VkImageView> m_attachmentView;
    Move<VkImageView> m_depthView;
    Move<VkRenderPass> m_renderPass;
    Move<VkFramebuffer> m_framebuffer;
    Move<VkPipeline> m_pipeline;
    Move<VkPipelineLayout> m_pipelineLayout;
    const std::vector<VertexData> &m_data;
    const ParametersGraphic &m_parametersGraphic;
    const std::vector<uint64_t> m_drawRepeats;
    const uint32_t m_blockCount;
    const uint32_t m_width;
    const uint32_t m_height;
};

GraphicBasicTestInstance::GraphicBasicTestInstance(vkt::Context &context, const std::vector<VertexData> &data,
                                                   const ParametersGraphic &parametersGraphic,
                                                   const std::vector<uint64_t> &drawRepeats)
    : StatisticQueryTestInstance(context, static_cast<uint32_t>(drawRepeats.size()), parametersGraphic.dstOffset, false)
    , m_colorAttachmentFormat(VK_FORMAT_R8G8B8A8_UNORM)
    , m_data(data)
    , m_parametersGraphic(parametersGraphic)
    , m_drawRepeats(drawRepeats)
    // For clear-skip, we'll use one framebuffer block for each draw instead of clearing.
    , m_blockCount((parametersGraphic.clearOp == CLEAR_SKIP) ?
                       static_cast<uint32_t>(std::accumulate(begin(drawRepeats), end(drawRepeats), uint64_t{0})) :
                       1u)
    , m_width(WIDTH)
    , m_height(HEIGHT * m_blockCount)
{
}

tcu::TestStatus GraphicBasicTestInstance::iterate(void)
{
    checkExtensions((m_parametersGraphic.resetType == RESET_TYPE_HOST) ? true : false);
    creatColorAttachmentAndRenderPass();
    createPipeline();
    return executeTest();
}

BufferPtr GraphicBasicTestInstance::creatAndFillVertexBuffer(void)
{
    const DeviceInterface &vk = m_context.getDeviceInterface();
    const VkDevice device     = m_context.getDevice();

    const VkDeviceSize dataSize = static_cast<VkDeviceSize>(
        deAlignSize(static_cast<size_t>(m_data.size() * sizeof(VertexData)),
                    static_cast<size_t>(m_context.getDeviceProperties().limits.nonCoherentAtomSize)));
    BufferPtr vertexBuffer =
        Buffer::createAndAlloc(vk, device, BufferCreateInfo(dataSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT),
                               m_context.getDefaultAllocator(), MemoryRequirement::HostVisible);

    uint8_t *ptr = reinterpret_cast<uint8_t *>(vertexBuffer->getBoundMemory().getHostPtr());
    deMemcpy(ptr, &m_data[0], static_cast<size_t>(m_data.size() * sizeof(VertexData)));

    flushMappedMemoryRange(vk, device, vertexBuffer->getBoundMemory().getMemory(),
                           vertexBuffer->getBoundMemory().getOffset(), dataSize);
    return vertexBuffer;
}

void GraphicBasicTestInstance::commandClearAttachment(const vk::DeviceInterface &vk,
                                                      const vk::VkCommandBuffer commandBuffer)
{
    const vk::VkOffset2D offset = vk::makeOffset2D(0, 0);
    const vk::VkExtent2D extent = vk::makeExtent2D(m_width, m_height);

    const vk::VkClearAttachment attachment = {
        m_parametersGraphic.clearOp == CLEAR_COLOR ?
            (vk::VkImageAspectFlags)vk::VK_IMAGE_ASPECT_COLOR_BIT :
            (vk::VkImageAspectFlags)vk::VK_IMAGE_ASPECT_DEPTH_BIT, // VkImageAspectFlags aspectMask;
        m_parametersGraphic.clearOp == CLEAR_COLOR ? 0u : 1u,      // uint32_t colorAttachment;
        m_parametersGraphic.clearOp == CLEAR_COLOR ?
            vk::makeClearValueColor(tcu::Vec4(0.0f, 0.0f, 0.0f, 0.0f)) :
            vk::makeClearValueDepthStencil(0.0f, 0u) // VkClearValue clearValue;
    };

    const vk::VkClearRect rect = {
        {offset, extent}, // VkRect2D rect;
        0u,               // uint32_t baseArrayLayer;
        1u,               // uint32_t layerCount;
    };

    vk.cmdClearAttachments(commandBuffer, 1u, &attachment, 1u, &rect);
}

void GraphicBasicTestInstance::creatColorAttachmentAndRenderPass(void)
{
    const DeviceInterface &vk = m_context.getDeviceInterface();
    const VkDevice device     = m_context.getDevice();

    VkExtent3D imageExtent = {
        m_width,  // width;
        m_height, // height;
        1u        // depth;
    };

    if (!m_parametersGraphic.noColorAttachments)
    {

        const ImageCreateInfo colorImageCreateInfo(
            VK_IMAGE_TYPE_2D, m_colorAttachmentFormat, imageExtent, 1, 1, VK_SAMPLE_COUNT_1_BIT,
            VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);

        m_colorAttachmentImage =
            Image::createAndAlloc(vk, device, colorImageCreateInfo, m_context.getDefaultAllocator(),
                                  m_context.getUniversalQueueFamilyIndex());

        const ImageViewCreateInfo attachmentViewInfo(m_colorAttachmentImage->object(), VK_IMAGE_VIEW_TYPE_2D,
                                                     m_colorAttachmentFormat);
        m_attachmentView = createImageView(vk, device, &attachmentViewInfo);
    }

    ImageCreateInfo depthImageCreateInfo(vk::VK_IMAGE_TYPE_2D, VK_FORMAT_D16_UNORM, imageExtent, 1, 1,
                                         vk::VK_SAMPLE_COUNT_1_BIT, vk::VK_IMAGE_TILING_OPTIMAL,
                                         vk::VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
    m_depthImage = Image::createAndAlloc(vk, device, depthImageCreateInfo, m_context.getDefaultAllocator(),
                                         m_context.getUniversalQueueFamilyIndex());

    // Construct a depth  view from depth image
    const ImageViewCreateInfo depthViewInfo(m_depthImage->object(), vk::VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_D16_UNORM);
    m_depthView = vk::createImageView(vk, device, &depthViewInfo);

    // Renderpass and Framebuffer
    if (m_parametersGraphic.noColorAttachments)
    {
        RenderPassCreateInfo renderPassCreateInfo;

        renderPassCreateInfo.addAttachment(
            AttachmentDescription(VK_FORMAT_D16_UNORM,                                    // format
                                  vk::VK_SAMPLE_COUNT_1_BIT,                              // samples
                                  vk::VK_ATTACHMENT_LOAD_OP_CLEAR,                        // loadOp
                                  vk::VK_ATTACHMENT_STORE_OP_DONT_CARE,                   // storeOp
                                  vk::VK_ATTACHMENT_LOAD_OP_DONT_CARE,                    // stencilLoadOp
                                  vk::VK_ATTACHMENT_STORE_OP_DONT_CARE,                   // stencilLoadOp
                                  vk::VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,   // initialLauout
                                  vk::VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)); // finalLayout

        const VkAttachmentReference depthAttachmentReference = {
            0u,                                                  // attachment
            vk::VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL // layout
        };

        renderPassCreateInfo.addSubpass(SubpassDescription(vk::VK_PIPELINE_BIND_POINT_GRAPHICS, // pipelineBindPoint
                                                           0,                                   // flags
                                                           0,                                   // inputCount
                                                           nullptr,                             // pInputAttachments
                                                           0,                                   // colorCount
                                                           nullptr,                             // pColorAttachments
                                                           nullptr,                             // pResolveAttachments
                                                           depthAttachmentReference, // depthStencilAttachment
                                                           0,                        // preserveCount
                                                           nullptr));                // preserveAttachments
        m_renderPass = vk::createRenderPass(vk, device, &renderPassCreateInfo);

        std::vector<vk::VkImageView> attachments(1);
        attachments[0] = *m_depthView;

        FramebufferCreateInfo framebufferCreateInfo(*m_renderPass, attachments, m_width, m_height, 1);
        m_framebuffer = vk::createFramebuffer(vk, device, &framebufferCreateInfo);
    }
    else
    {
        RenderPassCreateInfo renderPassCreateInfo;
        renderPassCreateInfo.addAttachment(
            AttachmentDescription(m_colorAttachmentFormat,                    // format
                                  VK_SAMPLE_COUNT_1_BIT,                      // samples
                                  VK_ATTACHMENT_LOAD_OP_CLEAR,                // loadOp
                                  VK_ATTACHMENT_STORE_OP_STORE,               // storeOp
                                  VK_ATTACHMENT_LOAD_OP_DONT_CARE,            // stencilLoadOp
                                  VK_ATTACHMENT_STORE_OP_STORE,               // stencilLoadOp
                                  VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,   // initialLauout
                                  VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)); // finalLayout

        renderPassCreateInfo.addAttachment(
            AttachmentDescription(VK_FORMAT_D16_UNORM,                                    // format
                                  vk::VK_SAMPLE_COUNT_1_BIT,                              // samples
                                  vk::VK_ATTACHMENT_LOAD_OP_CLEAR,                        // loadOp
                                  vk::VK_ATTACHMENT_STORE_OP_DONT_CARE,                   // storeOp
                                  vk::VK_ATTACHMENT_LOAD_OP_DONT_CARE,                    // stencilLoadOp
                                  vk::VK_ATTACHMENT_STORE_OP_DONT_CARE,                   // stencilLoadOp
                                  vk::VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,   // initialLauout
                                  vk::VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)); // finalLayout

        const VkAttachmentReference colorAttachmentReference = {
            0u,                                      // attachment
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL // layout
        };

        const VkAttachmentReference depthAttachmentReference = {
            1u,                                                  // attachment
            vk::VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL // layout
        };

        const VkSubpassDescription subpass = {
            (VkSubpassDescriptionFlags)0,    //VkSubpassDescriptionFlags flags;
            VK_PIPELINE_BIND_POINT_GRAPHICS, //VkPipelineBindPoint pipelineBindPoint;
            0u,                              //uint32_t inputAttachmentCount;
            nullptr,                         //const VkAttachmentReference* pInputAttachments;
            1u,                              //uint32_t colorAttachmentCount;
            &colorAttachmentReference,       //const VkAttachmentReference* pColorAttachments;
            nullptr,                         //const VkAttachmentReference* pResolveAttachments;
            &depthAttachmentReference,       //const VkAttachmentReference* pDepthStencilAttachment;
            0u,                              //uint32_t preserveAttachmentCount;
            nullptr,                         //const uint32_t* pPreserveAttachments;
        };

        renderPassCreateInfo.addSubpass(subpass);
        m_renderPass = createRenderPass(vk, device, &renderPassCreateInfo);

        std::vector<vk::VkImageView> attachments(2);
        attachments[0] = *m_attachmentView;
        attachments[1] = *m_depthView;

        FramebufferCreateInfo framebufferCreateInfo(*m_renderPass, attachments, m_width, m_height, 1);
        m_framebuffer = createFramebuffer(vk, device, &framebufferCreateInfo);
    }
}

bool GraphicBasicTestInstance::checkImage(void)
{
    const VkQueue queue         = m_context.getUniversalQueue();
    const VkOffset3D zeroOffset = {0, 0, 0};

    auto createCheckedImage = [this]() -> de::SharedPtr<Image>
    {
        const DeviceInterface &vk = m_context.getDeviceInterface();
        const VkDevice device     = m_context.getDevice();

        VkExtent3D imageExtent = {
            m_width,  // width;
            m_height, // height;
            1u        // depth;
        };

        const ImageCreateInfo colorImageCreateInfo(
            VK_IMAGE_TYPE_2D, m_colorAttachmentFormat, imageExtent, 1, 1, VK_SAMPLE_COUNT_1_BIT,
            VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);

        return Image::createAndAlloc(vk, device, colorImageCreateInfo, m_context.getDefaultAllocator(),
                                     m_context.getUniversalQueueFamilyIndex());
    };

    de::SharedPtr<Image> swapImage    = createCheckedImage();
    de::SharedPtr<Image> checkedImage = m_colorAttachmentImage.get() ? m_colorAttachmentImage : swapImage;

    const tcu::ConstPixelBufferAccess renderedFrame =
        checkedImage->readSurface(queue, m_context.getDefaultAllocator(), VK_IMAGE_LAYOUT_GENERAL, zeroOffset, m_width,
                                  m_height, VK_IMAGE_ASPECT_COLOR_BIT);

    if (m_parametersGraphic.vertexOnlyPipe || !m_colorAttachmentImage.get())
        return true;

    tcu::Texture2D referenceFrame(mapVkFormat(m_colorAttachmentFormat), m_width, m_height);
    referenceFrame.allocLevel(0);

    const auto iWidth  = static_cast<int>(m_width);
    const auto iHeight = static_cast<int>(m_height);

    if (m_parametersGraphic.tessPatchSize != 0)
    {
        const auto blue = tcu::RGBA::blue().toVec();
        if (m_parametersGraphic.primMode == TESS_PRIM_TRIANGLES)
        {
            // Triangle in top-left corner
            for (int y = 0; y < iHeight; ++y)
            {
                for (int x = 0; x < iWidth; ++x)
                {
                    if (x < (iWidth - y) - 1)
                        referenceFrame.getLevel(0).setPixel(blue, x, y);
                    else
                        referenceFrame.getLevel(0).setPixel(tcu::Vec4(0.0f), x, y);
                }
            }
        }
        else if (m_parametersGraphic.primMode == TESS_PRIM_ISOLINES)
        {
            // Fill with a single horizontal line at the bottom
            for (int y = 0; y < iHeight; ++y)
            {
                for (int x = 0; x < iWidth; ++x)
                {
                    if (y >= iHeight - 1)
                        referenceFrame.getLevel(0).setPixel(blue, x, y);
                    else
                        referenceFrame.getLevel(0).setPixel(tcu::Vec4(0.0f), x, y);
                }
            }
        }
        else // TESS_PRIM_QUADS (default)
        {
            for (int y = 0; y < iHeight; ++y)
                for (int x = 0; x < iWidth; ++x)
                    referenceFrame.getLevel(0).setPixel(blue, x, y);
        }
    }
    else
    {
        int colorNdx = 0;

        for (int y = 0; y < iHeight / 2; ++y)
            for (int x = 0; x < iWidth / 2; ++x)
                referenceFrame.getLevel(0).setPixel(m_data[colorNdx].color, x, y);

        colorNdx += 4;
        for (int y = iHeight / 2; y < iHeight; ++y)
            for (int x = 0; x < iWidth / 2; ++x)
                referenceFrame.getLevel(0).setPixel(m_data[colorNdx].color, x, y);

        colorNdx += 4;
        for (int y = 0; y < iHeight / 2; ++y)
            for (int x = iWidth / 2; x < iWidth; ++x)
                referenceFrame.getLevel(0).setPixel(m_data[colorNdx].color, x, y);

        colorNdx += 4;
        for (int y = iHeight / 2; y < iHeight; ++y)
            for (int x = iWidth / 2; x < iWidth; ++x)
                referenceFrame.getLevel(0).setPixel(m_data[colorNdx].color, x, y);
    }

    return tcu::floatThresholdCompare(m_context.getTestContext().getLog(), "Result", "Image comparison result",
                                      referenceFrame.getLevel(0), renderedFrame, tcu::Vec4(0.01f),
                                      tcu::COMPARE_LOG_ON_ERROR);
}

class VertexShaderTestInstance : public GraphicBasicTestInstance
{
public:
    VertexShaderTestInstance(vkt::Context &context, const std::vector<VertexData> &data,
                             const ParametersGraphic &parametersGraphic, const std::vector<uint64_t> &drawRepeats);

protected:
    virtual void createPipeline(void);
    virtual tcu::TestStatus executeTest(void);
    virtual tcu::TestStatus checkResult(VkQueryPool queryPool);
    void draw(VkCommandBuffer cmdBuffer);
};

VertexShaderTestInstance::VertexShaderTestInstance(vkt::Context &context, const std::vector<VertexData> &data,
                                                   const ParametersGraphic &parametersGraphic,
                                                   const std::vector<uint64_t> &drawRepeats)
    : GraphicBasicTestInstance(context, data, parametersGraphic, drawRepeats)
{
}

void VertexShaderTestInstance::createPipeline(void)
{
    const DeviceInterface &vk = m_context.getDeviceInterface();
    const VkDevice device     = m_context.getDevice();

    switch (m_parametersGraphic.primitiveTopology)
    {
    case VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY:
    case VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY:
    case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY:
    case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY:
        if (!m_context.getDeviceFeatures().geometryShader)
            throw tcu::NotSupportedError("Geometry shader are not supported");
        break;
    default:
        break;
    }

    // Pipeline
    Unique<VkShaderModule> vs(createShaderModule(vk, device, m_context.getBinaryCollection().get("vertex"), 0));
    Move<VkShaderModule> fs;

    if (!m_parametersGraphic.vertexOnlyPipe)
        fs = createShaderModule(vk, device, m_context.getBinaryCollection().get("fragment"), 0);

    const PipelineCreateInfo::ColorBlendState::Attachment attachmentState;

    std::vector<VkPushConstantRange> pcRanges;

    if (m_parametersGraphic.clearOp == CLEAR_SKIP)
        pcRanges.push_back(makePushConstantRange(VK_SHADER_STAGE_VERTEX_BIT, 0u, kFloatSize));

    if (m_parametersGraphic.noColorAttachments)
        pcRanges.push_back(makePushConstantRange(VK_SHADER_STAGE_FRAGMENT_BIT, kFloatSize, kFloatSize));

    const PipelineLayoutCreateInfo pipelineLayoutCreateInfo(std::vector<VkDescriptorSetLayout>(), de::sizeU32(pcRanges),
                                                            de::dataOrNull(pcRanges));
    m_pipelineLayout = createPipelineLayout(vk, device, &pipelineLayoutCreateInfo);

    const VkVertexInputBindingDescription vertexInputBindingDescription = {
        0,                                         // binding;
        static_cast<uint32_t>(sizeof(VertexData)), // stride;
        VK_VERTEX_INPUT_RATE_VERTEX                // inputRate
    };

    const VkVertexInputAttributeDescription vertexInputAttributeDescriptions[] = {
        {0u, 0u, VK_FORMAT_R32G32B32A32_SFLOAT, 0u}, // VertexElementData::position
        {1u, 0u, VK_FORMAT_R32G32B32A32_SFLOAT, static_cast<uint32_t>(sizeof(tcu::Vec4))}, // VertexElementData::color
    };

    const VkPipelineVertexInputStateCreateInfo vf_info = {
        // sType;
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, // pNext;
        NULL,                                                      // flags;
        0u,                                                        // vertexBindingDescriptionCount;
        1u,                                                        // pVertexBindingDescriptions;
        &vertexInputBindingDescription,                            // vertexAttributeDescriptionCount;
        2u,                                                        // pVertexAttributeDescriptions;
        vertexInputAttributeDescriptions};

    PipelineCreateInfo pipelineCreateInfo(*m_pipelineLayout, *m_renderPass, 0, (VkPipelineCreateFlags)0);
    pipelineCreateInfo.addShader(PipelineCreateInfo::PipelineShaderStage(*vs, "main", VK_SHADER_STAGE_VERTEX_BIT));
    if (!m_parametersGraphic.vertexOnlyPipe)
        pipelineCreateInfo.addShader(
            PipelineCreateInfo::PipelineShaderStage(*fs, "main", VK_SHADER_STAGE_FRAGMENT_BIT));
    const VkBool32 depthTestAndWrites = makeVkBool(m_parametersGraphic.noColorAttachments);
    pipelineCreateInfo.addState(PipelineCreateInfo::DepthStencilState(depthTestAndWrites, depthTestAndWrites));
    pipelineCreateInfo.addState(PipelineCreateInfo::InputAssemblerState(m_parametersGraphic.primitiveTopology));
    pipelineCreateInfo.addState(PipelineCreateInfo::ColorBlendState(1, &attachmentState));

    const VkViewport viewport = makeViewport(m_width, m_height);
    const VkRect2D scissor    = makeRect2D(m_width, m_height);
    pipelineCreateInfo.addState(
        PipelineCreateInfo::ViewportState(1u, std::vector<VkViewport>(1, viewport), std::vector<VkRect2D>(1, scissor)));
    pipelineCreateInfo.addState(PipelineCreateInfo::RasterizerState());
    pipelineCreateInfo.addState(PipelineCreateInfo::MultiSampleState());
    pipelineCreateInfo.addState(vf_info);
    m_pipeline = createGraphicsPipeline(vk, device, VK_NULL_HANDLE, &pipelineCreateInfo);
}

tcu::TestStatus VertexShaderTestInstance::executeTest(void)
{
    const DeviceInterface &vk       = m_context.getDeviceInterface();
    const VkDevice device           = m_context.getDevice();
    const VkQueue queue             = m_context.getUniversalQueue();
    const uint32_t queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();

    const CmdPoolCreateInfo cmdPoolCreateInfo(queueFamilyIndex);
    const Move<VkCommandPool> cmdPool = createCommandPool(vk, device, &cmdPoolCreateInfo);
    const uint32_t queryCount         = static_cast<uint32_t>(m_drawRepeats.size());
    const Unique<VkQueryPool> queryPool(makeQueryPool(vk, device, queryCount, m_parametersGraphic.queryStatisticFlags));

    const VkDeviceSize vertexBufferOffset = 0u;
    const BufferPtr vertexBufferSp        = creatAndFillVertexBuffer();
    const VkBuffer vertexBuffer           = vertexBufferSp->object();
    const bool useOffsetPC                = (m_parametersGraphic.clearOp == CLEAR_SKIP);
    const bool useFragDepthPC             = m_parametersGraphic.noColorAttachments;

    const Unique<VkCommandBuffer> cmdBuffer(
        allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

    beginCommandBuffer(vk, *cmdBuffer);
    {
        std::vector<VkClearValue> renderPassClearValues(2);
        deMemset(&renderPassClearValues[0], 0, static_cast<int>(renderPassClearValues.size()) * sizeof(VkClearValue));

        if (!m_parametersGraphic.noColorAttachments)
            initialTransitionColor2DImage(
                vk, *cmdBuffer, m_colorAttachmentImage->object(), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
        initialTransitionDepth2DImage(
            vk, *cmdBuffer, m_depthImage->object(), VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT);

        if (m_parametersGraphic.resetType != RESET_TYPE_HOST)
            vk.cmdResetQueryPool(*cmdBuffer, *queryPool, 0u, queryCount);

        beginRenderPass(vk, *cmdBuffer, *m_renderPass, *m_framebuffer, makeRect2D(0, 0, m_width, m_height),
                        (uint32_t)renderPassClearValues.size(), &renderPassClearValues[0]);

        const float offsetStep = 2.0f / static_cast<float>(m_blockCount);
        float currentOffset    = 0.0f;

        for (uint32_t i = 0; i < queryCount; ++i)
        {
            vk.cmdBeginQuery(*cmdBuffer, *queryPool, i, (VkQueryControlFlags)0u);
            vk.cmdBindVertexBuffers(*cmdBuffer, 0, 1, &vertexBuffer, &vertexBufferOffset);
            vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipeline);

            for (uint64_t j = 0; j < m_drawRepeats[i]; ++j)
            {
                if (useOffsetPC)
                {
                    vk.cmdPushConstants(*cmdBuffer, *m_pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0u, kFloatSize,
                                        &currentOffset);
                    currentOffset += offsetStep;
                }
                if (useFragDepthPC)
                {
                    static const float fragDepth = 1.0f;
                    vk.cmdPushConstants(*cmdBuffer, *m_pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, kFloatSize,
                                        kFloatSize, &fragDepth);
                }
                draw(*cmdBuffer);
            }

            if (m_parametersGraphic.clearOp != CLEAR_SKIP)
                commandClearAttachment(vk, *cmdBuffer);
            vk.cmdEndQuery(*cmdBuffer, *queryPool, i);
        }

        endRenderPass(vk, *cmdBuffer);

        if (m_parametersGraphic.resetType == RESET_TYPE_BEFORE_COPY ||
            m_parametersGraphic.resetType == RESET_TYPE_AFTER_COPY || m_parametersGraphic.copyType == COPY_TYPE_CMD)
        {
            VkDeviceSize stride          = m_parametersGraphic.querySizeFlags() ? sizeof(uint64_t) : sizeof(uint32_t);
            vk::VkQueryResultFlags flags = m_parametersGraphic.querySizeFlags() | VK_QUERY_RESULT_WAIT_BIT;

            if (m_parametersGraphic.resetType == RESET_TYPE_HOST)
            {
                flags |= VK_QUERY_RESULT_WITH_AVAILABILITY_BIT;
                stride *= 2u;
            }

            if (m_parametersGraphic.resetType == RESET_TYPE_BEFORE_COPY)
            {
                vk.cmdResetQueryPool(*cmdBuffer, *queryPool, 0u, queryCount);
                flags  = VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT;
                stride = sizeof(ValueAndAvailability);
            }

            VkDeviceSize dstOffsetQuery = (m_parametersGraphic.dstOffset) ? stride : 0;
            vk.cmdCopyQueryPoolResults(*cmdBuffer, *queryPool, 0, queryCount, m_resetBuffer->object(), dstOffsetQuery,
                                       stride, flags);

            if (m_parametersGraphic.resetType == RESET_TYPE_AFTER_COPY)
                vk.cmdResetQueryPool(*cmdBuffer, *queryPool, 0u, queryCount);

            const VkBufferMemoryBarrier barrier = {
                VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER, //  VkStructureType sType;
                nullptr,                                 //  const void* pNext;
                VK_ACCESS_TRANSFER_WRITE_BIT,            //  VkAccessFlags srcAccessMask;
                VK_ACCESS_HOST_READ_BIT,                 //  VkAccessFlags dstAccessMask;
                VK_QUEUE_FAMILY_IGNORED,                 //  uint32_t srcQueueFamilyIndex;
                VK_QUEUE_FAMILY_IGNORED,                 //  uint32_t destQueueFamilyIndex;
                m_resetBuffer->object(),                 //  VkBuffer buffer;
                0u,                                      //  VkDeviceSize offset;
                queryCount * stride + dstOffsetQuery,    //  VkDeviceSize size;
            };
            vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                                  (VkDependencyFlags)0, 0, nullptr, 1u, &barrier, 0, nullptr);
        }

        if (!m_parametersGraphic.noColorAttachments)
            transition2DImage(vk, *cmdBuffer, m_colorAttachmentImage->object(), VK_IMAGE_ASPECT_COLOR_BIT,
                              VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
                              VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
                              VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
    }
    endCommandBuffer(vk, *cmdBuffer);

    if (m_parametersGraphic.resetType == RESET_TYPE_HOST)
        vk.resetQueryPool(device, *queryPool, 0u, queryCount);

    // Wait for completion
    submitCommandsAndWait(vk, device, queue, *cmdBuffer);
    return checkResult(*queryPool);
}

tcu::TestStatus VertexShaderTestInstance::checkResult(VkQueryPool queryPool)
{
    const DeviceInterface &vk = m_context.getDeviceInterface();
    const VkDevice device     = m_context.getDevice();
    uint64_t expectedMin      = 0u;
    std::string errorMsg;

    switch (m_parametersGraphic.queryStatisticFlags)
    {
    case VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_VERTICES_BIT:
        expectedMin = 16u;
        break;
    case VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT:
        expectedMin = m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST             ? 15u :
                      m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY  ? 8u :
                      m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY ? 14u :
                      m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY ? 6u :
                      m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY ?
                                                                                                                    8u :
                                                                                                                    16u;
        break;
    case VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT:
        expectedMin = m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_POINT_LIST                ? 16u :
                      m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_LINE_LIST                 ? 8u :
                      m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_LINE_STRIP                ? 15u :
                      m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST             ? 5u :
                      m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP            ? 8u :
                      m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN              ? 8u :
                      m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY  ? 4u :
                      m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY ? 13u :
                      m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY ? 2u :
                      m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY ?
                                                                                                                    6u :
                                                                                                                    0u;
        break;
    case VK_QUERY_PIPELINE_STATISTIC_FRAGMENT_SHADER_INVOCATIONS_BIT:
        expectedMin =
            m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_POINT_LIST                    ? 9u :
            m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_LINE_LIST                     ? 192u :
            m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_LINE_STRIP                    ? 374u :
            m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST                 ? 4096u :
            m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP                ? 4096u :
            m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN                  ? 4096u :
            m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY      ? 128u :
            m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY     ? 374u :
            m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY  ? 992u :
            m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY ? 2048u :
                                                                                                           0u;
        break;
    case VK_QUERY_PIPELINE_STATISTIC_CLIPPING_INVOCATIONS_BIT:
        expectedMin = m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_POINT_LIST                ? 16u :
                      m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_LINE_LIST                 ? 8u :
                      m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_LINE_STRIP                ? 15u :
                      m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST             ? 5u :
                      m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP            ? 8u :
                      m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN              ? 8u :
                      m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY  ? 4u :
                      m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY ? 13u :
                      m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY ? 2u :
                      m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY ?
                                                                                                                    6u :
                                                                                                                    0u;
        break;
    case VK_QUERY_PIPELINE_STATISTIC_CLIPPING_PRIMITIVES_BIT:
        expectedMin = m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_POINT_LIST                ? 16u :
                      m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_LINE_LIST                 ? 8u :
                      m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_LINE_STRIP                ? 15u :
                      m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST             ? 5u :
                      m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP            ? 8u :
                      m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN              ? 8u :
                      m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY  ? 4u :
                      m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY ? 13u :
                      m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY ? 2u :
                      m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY ?
                                                                                                                    6u :
                                                                                                                    0u;
        break;
    default:
        DE_FATAL("Unexpected type of statistics query");
        break;
    }

    const uint32_t queryCount = static_cast<uint32_t>(m_drawRepeats.size());

    if (m_parametersGraphic.resetType == RESET_TYPE_NORMAL || m_parametersGraphic.resetType == RESET_TYPE_AFTER_COPY)
    {
        ResultsVector results(queryCount, 0u);

        if (m_parametersGraphic.copyType == COPY_TYPE_CMD)
        {
            const vk::Allocation &allocation = m_resetBuffer->getBoundMemory();
            cmdCopyQueryPoolResultsVector(results, vk, device, allocation, queryCount,
                                          (VK_QUERY_RESULT_WAIT_BIT | m_parametersGraphic.querySizeFlags()),
                                          m_parametersGraphic.dstOffset);
        }
        else
        {
            VK_CHECK(GetQueryPoolResultsVector(results, vk, device, queryPool, 0u, queryCount,
                                               (VK_QUERY_RESULT_WAIT_BIT | m_parametersGraphic.querySizeFlags())));
        }

        if (results[0] < expectedMin)
        {
            std::ostringstream msg;
            msg << "QueryPoolResults incorrect: expected at least " << expectedMin << " but got " << results[0];
            errorMsg = msg.str();
        }
        else if (queryCount > 1)
        {
            double pearson = calculatePearsonCorrelation(m_drawRepeats, results);
            if (fabs(pearson) < 0.8)
            {
                std::ostringstream msg;
                msg << "QueryPoolResults are nonlinear: Pearson " << pearson << " for";
                for (const auto &x : results)
                    msg << " " << x;
                errorMsg = msg.str();
            }
        }
    }
    else if (m_parametersGraphic.resetType == RESET_TYPE_HOST)
    {
        ResultsVectorWithAvailability results(queryCount, pair<uint64_t, uint64_t>(0u, 0u));

        if (m_parametersGraphic.copyType == COPY_TYPE_CMD)
        {
            const vk::Allocation &allocation = m_resetBuffer->getBoundMemory();
            cmdCopyQueryPoolResultsVector(results, vk, device, allocation, queryCount,
                                          (VK_QUERY_RESULT_WAIT_BIT | m_parametersGraphic.querySizeFlags() |
                                           VK_QUERY_RESULT_WITH_AVAILABILITY_BIT),
                                          m_parametersGraphic.dstOffset);
        }
        else
        {
            VK_CHECK(GetQueryPoolResultsVector(results, vk, device, queryPool, 0u, queryCount,
                                               (VK_QUERY_RESULT_WAIT_BIT | m_parametersGraphic.querySizeFlags() |
                                                VK_QUERY_RESULT_WITH_AVAILABILITY_BIT)));
        }

        if (results[0].first < expectedMin || results[0].second == 0)
        {
            std::ostringstream msg;
            msg << "QueryPoolResults incorrect: expected at least " << expectedMin << " with availability 1 but got "
                << results[0].first << " with availability " << results[0].second;
            errorMsg = msg.str();
        }
        else if (queryCount > 1)
        {
            double pearson = calculatePearsonCorrelation(m_drawRepeats, results);
            if (fabs(pearson) < 0.8)
            {
                std::ostringstream msg;
                msg << "QueryPoolResults are nonlinear: Pearson " << pearson << " for";
                for (const auto &x : results)
                    msg << " " << x.first;
                errorMsg = msg.str();
            }
        }
        else
        {
            uint64_t temp = results[0].first;

            vk.resetQueryPool(device, queryPool, 0, queryCount);
            vk::VkResult res = GetQueryPoolResultsVector(
                results, vk, device, queryPool, 0u, queryCount,
                (m_parametersGraphic.querySizeFlags() | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT));
            /* From Vulkan spec:
             *
             * If VK_QUERY_RESULT_WAIT_BIT and VK_QUERY_RESULT_PARTIAL_BIT are both not set then no result values are written to pData
             * for queries that are in the unavailable state at the time of the call, and vkGetQueryPoolResults returns VK_NOT_READY.
             * However, availability state is still written to pData for those queries if VK_QUERY_RESULT_WITH_AVAILABILITY_BIT is set.
             */
            if (res != vk::VK_NOT_READY || results[0].first != temp || results[0].second != 0)
                errorMsg = "QueryPoolResults incorrect reset";
        }
    }
    else
    {
        // With RESET_TYPE_BEFORE_COPY, we only need to verify the result after the copy include an availability bit set as zero.
        const auto result = verifyUnavailable();
        if (result.isFail())
            errorMsg = result.getDescription();
    }

    // Don't need to check the result image when clearing operations are executed.
    // Anyway, the result image must be known for Vulkan SC to correct resources allocation.
    const bool checkImageResult = checkImage();
    if (m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP &&
        m_parametersGraphic.clearOp == CLEAR_NOOP && !m_parametersGraphic.noColorAttachments && errorMsg.empty() &&
        !checkImageResult)
    {
        errorMsg = "Result image doesn't match expected image";
    }

    if (!errorMsg.empty())
        return tcu::TestStatus::fail(errorMsg);
    return tcu::TestStatus::pass("Pass");
}

void VertexShaderTestInstance::draw(VkCommandBuffer cmdBuffer)
{
    const DeviceInterface &vk = m_context.getDeviceInterface();
    switch (m_parametersGraphic.primitiveTopology)
    {
    case VK_PRIMITIVE_TOPOLOGY_POINT_LIST:
    case VK_PRIMITIVE_TOPOLOGY_LINE_LIST:
    case VK_PRIMITIVE_TOPOLOGY_LINE_STRIP:
    case VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY:
    case VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY:
    case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY:
    case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY:
    case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST:
        vk.cmdDraw(cmdBuffer, de::sizeU32(m_data), 1u, 0u, 0u);
        break;
    case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN:
    case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP:
        vk.cmdDraw(cmdBuffer, 4u, 1u, 0u, 0u);
        vk.cmdDraw(cmdBuffer, 4u, 1u, 4u, 1u);
        vk.cmdDraw(cmdBuffer, 4u, 1u, 8u, 2u);
        vk.cmdDraw(cmdBuffer, 4u, 1u, 12u, 3u);
        break;
    default:
        DE_ASSERT(0);
        break;
    }
}

class VertexShaderSecondaryTestInstance : public VertexShaderTestInstance
{
public:
    VertexShaderSecondaryTestInstance(vkt::Context &context, const std::vector<VertexData> &data,
                                      const ParametersGraphic &parametersGraphic,
                                      const std::vector<uint64_t> &drawRepeats);

protected:
    virtual tcu::TestStatus executeTest(void);
};

VertexShaderSecondaryTestInstance::VertexShaderSecondaryTestInstance(vkt::Context &context,
                                                                     const std::vector<VertexData> &data,
                                                                     const ParametersGraphic &parametersGraphic,
                                                                     const std::vector<uint64_t> &drawRepeats)
    : VertexShaderTestInstance(context, data, parametersGraphic, drawRepeats)
{
}

typedef de::SharedPtr<vk::Unique<VkCommandBuffer>> VkCommandBufferSp;

tcu::TestStatus VertexShaderSecondaryTestInstance::executeTest(void)
{
    const DeviceInterface &vk       = m_context.getDeviceInterface();
    const VkDevice device           = m_context.getDevice();
    const VkQueue queue             = m_context.getUniversalQueue();
    const uint32_t queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();

    const CmdPoolCreateInfo cmdPoolCreateInfo(queueFamilyIndex);
    const Move<VkCommandPool> cmdPool = createCommandPool(vk, device, &cmdPoolCreateInfo);
    const uint32_t queryCount         = static_cast<uint32_t>(m_drawRepeats.size());
    const Unique<VkQueryPool> queryPool(makeQueryPool(vk, device, queryCount, m_parametersGraphic.queryStatisticFlags));

    const VkDeviceSize vertexBufferOffset = 0u;
    const BufferPtr vertexBufferSp        = creatAndFillVertexBuffer();
    const VkBuffer vertexBuffer           = vertexBufferSp->object();
    const bool useOffsetPC                = (m_parametersGraphic.clearOp == CLEAR_SKIP);
    const bool useFragDepthPC             = m_parametersGraphic.noColorAttachments;

    const Unique<VkCommandBuffer> primaryCmdBuffer(
        allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));
    std::vector<VkCommandBufferSp> secondaryCmdBuffers(queryCount);

    for (uint32_t i = 0; i < queryCount; ++i)
        secondaryCmdBuffers[i] = VkCommandBufferSp(new vk::Unique<VkCommandBuffer>(
            allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_SECONDARY)));

    const float offsetStep = 2.0f / static_cast<float>(m_blockCount);
    float currentOffset    = 0.0f;

    for (uint32_t i = 0; i < queryCount; ++i)
    {
        beginSecondaryCommandBuffer(vk, secondaryCmdBuffers[i]->get(), m_parametersGraphic.queryStatisticFlags,
                                    *m_renderPass, *m_framebuffer, VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT);
        vk.cmdBeginQuery(secondaryCmdBuffers[i]->get(), *queryPool, i, (VkQueryControlFlags)0u);
        vk.cmdBindPipeline(secondaryCmdBuffers[i]->get(), VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipeline);
        vk.cmdBindVertexBuffers(secondaryCmdBuffers[i]->get(), 0u, 1u, &vertexBuffer, &vertexBufferOffset);
        for (uint32_t j = 0; j < m_drawRepeats[i]; ++j)
        {
            if (useOffsetPC)
            {
                vk.cmdPushConstants(secondaryCmdBuffers[i]->get(), *m_pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0u,
                                    kFloatSize, &currentOffset);
                currentOffset += offsetStep;
            }
            if (useFragDepthPC)
            {
                static const float fragDepth = 1.0f;
                vk.cmdPushConstants(secondaryCmdBuffers[i]->get(), *m_pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT,
                                    kFloatSize, kFloatSize, &fragDepth);
            }

            draw(secondaryCmdBuffers[i]->get());
        }
        if (m_parametersGraphic.clearOp != CLEAR_SKIP)
            commandClearAttachment(vk, secondaryCmdBuffers[i]->get());
        vk.cmdEndQuery(secondaryCmdBuffers[i]->get(), *queryPool, i);
        endCommandBuffer(vk, secondaryCmdBuffers[i]->get());
    }

    beginCommandBuffer(vk, *primaryCmdBuffer);
    {
        std::vector<VkClearValue> renderPassClearValues(2);
        deMemset(&renderPassClearValues[0], 0, static_cast<int>(renderPassClearValues.size()) * sizeof(VkClearValue));

        if (!m_parametersGraphic.noColorAttachments)
            initialTransitionColor2DImage(
                vk, *primaryCmdBuffer, m_colorAttachmentImage->object(), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                vk::VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, vk::VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

        initialTransitionDepth2DImage(
            vk, *primaryCmdBuffer, m_depthImage->object(), vk::VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            vk::VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            vk::VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | vk::VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT);

        if (m_parametersGraphic.resetType != RESET_TYPE_HOST)
            vk.cmdResetQueryPool(*primaryCmdBuffer, *queryPool, 0u, queryCount);

        beginRenderPass(vk, *primaryCmdBuffer, *m_renderPass, *m_framebuffer, makeRect2D(0, 0, m_width, m_height),
                        (uint32_t)renderPassClearValues.size(), &renderPassClearValues[0],
                        VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);
        for (uint32_t i = 0; i < queryCount; ++i)
            vk.cmdExecuteCommands(*primaryCmdBuffer, 1u, &(secondaryCmdBuffers[i]->get()));
        endRenderPass(vk, *primaryCmdBuffer);

        if (m_parametersGraphic.resetType == RESET_TYPE_BEFORE_COPY ||
            m_parametersGraphic.resetType == RESET_TYPE_AFTER_COPY || m_parametersGraphic.copyType == COPY_TYPE_CMD)
        {
            VkDeviceSize stride          = m_parametersGraphic.querySizeFlags() ? sizeof(uint64_t) : sizeof(uint32_t);
            vk::VkQueryResultFlags flags = m_parametersGraphic.querySizeFlags() | VK_QUERY_RESULT_WAIT_BIT;

            if (m_parametersGraphic.resetType == RESET_TYPE_HOST)
            {
                flags |= VK_QUERY_RESULT_WITH_AVAILABILITY_BIT;
                stride *= 2u;
            }

            if (m_parametersGraphic.resetType == RESET_TYPE_BEFORE_COPY)
            {
                vk.cmdResetQueryPool(*primaryCmdBuffer, *queryPool, 0u, queryCount);
                flags  = VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT;
                stride = sizeof(ValueAndAvailability);
            }

            VkDeviceSize dstOffsetQuery = (m_parametersGraphic.dstOffset) ? stride : 0;
            vk.cmdCopyQueryPoolResults(*primaryCmdBuffer, *queryPool, 0, queryCount, m_resetBuffer->object(),
                                       dstOffsetQuery, stride, flags);

            if (m_parametersGraphic.resetType == RESET_TYPE_AFTER_COPY)
                vk.cmdResetQueryPool(*primaryCmdBuffer, *queryPool, 0u, queryCount);

            const VkBufferMemoryBarrier barrier = {
                VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER, //  VkStructureType sType;
                nullptr,                                 //  const void* pNext;
                VK_ACCESS_TRANSFER_WRITE_BIT,            //  VkAccessFlags srcAccessMask;
                VK_ACCESS_HOST_READ_BIT,                 //  VkAccessFlags dstAccessMask;
                VK_QUEUE_FAMILY_IGNORED,                 //  uint32_t srcQueueFamilyIndex;
                VK_QUEUE_FAMILY_IGNORED,                 //  uint32_t destQueueFamilyIndex;
                m_resetBuffer->object(),                 //  VkBuffer buffer;
                0u,                                      //  VkDeviceSize offset;
                queryCount * stride + dstOffsetQuery,    //  VkDeviceSize size;
            };
            vk.cmdPipelineBarrier(*primaryCmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                                  (VkDependencyFlags)0, 0, nullptr, 1u, &barrier, 0, nullptr);
        }

        if (!m_parametersGraphic.noColorAttachments)
            transition2DImage(vk, *primaryCmdBuffer, m_colorAttachmentImage->object(), VK_IMAGE_ASPECT_COLOR_BIT,
                              VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
                              VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
                              VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
    }
    endCommandBuffer(vk, *primaryCmdBuffer);

    if (m_parametersGraphic.resetType == RESET_TYPE_HOST)
        vk.resetQueryPool(device, *queryPool, 0u, queryCount);

    // Wait for completion
    submitCommandsAndWait(vk, device, queue, *primaryCmdBuffer);
    return checkResult(*queryPool);
}

class VertexShaderSecondaryInheritedTestInstance : public VertexShaderTestInstance
{
public:
    VertexShaderSecondaryInheritedTestInstance(vkt::Context &context, const std::vector<VertexData> &data,
                                               const ParametersGraphic &parametersGraphic,
                                               const std::vector<uint64_t> &drawRepeats);

protected:
    virtual void checkExtensions(bool hostQueryResetEnabled);
    virtual tcu::TestStatus executeTest(void);
};

VertexShaderSecondaryInheritedTestInstance::VertexShaderSecondaryInheritedTestInstance(
    vkt::Context &context, const std::vector<VertexData> &data, const ParametersGraphic &parametersGraphic,
    const std::vector<uint64_t> &drawRepeats)
    : VertexShaderTestInstance(context, data, parametersGraphic, drawRepeats)
{
}

void VertexShaderSecondaryInheritedTestInstance::checkExtensions(bool hostQueryResetEnabled)
{
    StatisticQueryTestInstance::checkExtensions(hostQueryResetEnabled);
    if (!m_context.getDeviceFeatures().inheritedQueries)
        throw tcu::NotSupportedError("Inherited queries are not supported");
}

tcu::TestStatus VertexShaderSecondaryInheritedTestInstance::executeTest(void)
{
    const DeviceInterface &vk       = m_context.getDeviceInterface();
    const VkDevice device           = m_context.getDevice();
    const VkQueue queue             = m_context.getUniversalQueue();
    const uint32_t queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();

    const CmdPoolCreateInfo cmdPoolCreateInfo(queueFamilyIndex);
    const Move<VkCommandPool> cmdPool = createCommandPool(vk, device, &cmdPoolCreateInfo);
    const uint32_t queryCount         = static_cast<uint32_t>(m_drawRepeats.size());
    const Unique<VkQueryPool> queryPool(makeQueryPool(vk, device, queryCount, m_parametersGraphic.queryStatisticFlags));

    const VkDeviceSize vertexBufferOffset = 0u;
    const BufferPtr vertexBufferSp        = creatAndFillVertexBuffer();
    const VkBuffer vertexBuffer           = vertexBufferSp->object();
    const bool useOffsetPC                = (m_parametersGraphic.clearOp == CLEAR_SKIP);
    const bool useFragDepthPC             = m_parametersGraphic.noColorAttachments;

    const Unique<VkCommandBuffer> primaryCmdBuffer(
        allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));
    std::vector<VkCommandBufferSp> secondaryCmdBuffers(queryCount);

    for (uint32_t i = 0; i < queryCount; ++i)
        secondaryCmdBuffers[i] = VkCommandBufferSp(new vk::Unique<VkCommandBuffer>(
            allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_SECONDARY)));

    const float offsetStep = 2.0f / static_cast<float>(m_blockCount);
    float currentOffset    = 0.0f;

    for (uint32_t i = 0; i < queryCount; ++i)
    {
        beginSecondaryCommandBuffer(vk, secondaryCmdBuffers[i]->get(), m_parametersGraphic.queryStatisticFlags,
                                    *m_renderPass, *m_framebuffer, VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT);
        vk.cmdBindPipeline(secondaryCmdBuffers[i]->get(), VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipeline);
        vk.cmdBindVertexBuffers(secondaryCmdBuffers[i]->get(), 0u, 1u, &vertexBuffer, &vertexBufferOffset);
        for (uint32_t j = 0; j < m_drawRepeats[i]; ++j)
        {
            if (useOffsetPC)
            {
                vk.cmdPushConstants(secondaryCmdBuffers[i]->get(), *m_pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0u,
                                    kFloatSize, &currentOffset);
                currentOffset += offsetStep;
            }
            if (useFragDepthPC)
            {
                static const float fragDepth = 1.0f;
                vk.cmdPushConstants(secondaryCmdBuffers[i]->get(), *m_pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT,
                                    kFloatSize, kFloatSize, &fragDepth);
            }
            draw(secondaryCmdBuffers[i]->get());
        }
        endCommandBuffer(vk, secondaryCmdBuffers[i]->get());
    }

    beginCommandBuffer(vk, *primaryCmdBuffer);
    {
        std::vector<VkClearValue> renderPassClearValues(2);
        deMemset(&renderPassClearValues[0], 0, static_cast<int>(renderPassClearValues.size()) * sizeof(VkClearValue));

        if (!m_parametersGraphic.noColorAttachments)
            initialTransitionColor2DImage(
                vk, *primaryCmdBuffer, m_colorAttachmentImage->object(), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
        initialTransitionDepth2DImage(
            vk, *primaryCmdBuffer, m_depthImage->object(), VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT);

        if (m_parametersGraphic.resetType != RESET_TYPE_HOST)
            vk.cmdResetQueryPool(*primaryCmdBuffer, *queryPool, 0u, queryCount);

        for (uint32_t i = 0; i < queryCount; ++i)
        {
            vk.cmdBeginQuery(*primaryCmdBuffer, *queryPool, i, (VkQueryControlFlags)0u);
            beginRenderPass(vk, *primaryCmdBuffer, *m_renderPass, *m_framebuffer, makeRect2D(0, 0, m_width, m_height),
                            (uint32_t)renderPassClearValues.size(), &renderPassClearValues[0],
                            VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);
            vk.cmdExecuteCommands(*primaryCmdBuffer, 1u, &(secondaryCmdBuffers[i]->get()));
            endRenderPass(vk, *primaryCmdBuffer);
            vk.cmdEndQuery(*primaryCmdBuffer, *queryPool, i);
        }

        if (m_parametersGraphic.resetType == RESET_TYPE_BEFORE_COPY ||
            m_parametersGraphic.resetType == RESET_TYPE_AFTER_COPY || m_parametersGraphic.copyType == COPY_TYPE_CMD)
        {
            VkDeviceSize stride          = m_parametersGraphic.querySizeFlags() ? sizeof(uint64_t) : sizeof(uint32_t);
            vk::VkQueryResultFlags flags = m_parametersGraphic.querySizeFlags() | VK_QUERY_RESULT_WAIT_BIT;

            if (m_parametersGraphic.resetType == RESET_TYPE_HOST)
            {
                flags |= VK_QUERY_RESULT_WITH_AVAILABILITY_BIT;
                stride *= 2u;
            }

            if (m_parametersGraphic.resetType == RESET_TYPE_BEFORE_COPY)
            {
                vk.cmdResetQueryPool(*primaryCmdBuffer, *queryPool, 0u, queryCount);
                flags  = VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT;
                stride = sizeof(ValueAndAvailability);
            }

            VkDeviceSize dstOffsetQuery = (m_parametersGraphic.dstOffset) ? stride : 0;
            vk.cmdCopyQueryPoolResults(*primaryCmdBuffer, *queryPool, 0, queryCount, m_resetBuffer->object(),
                                       dstOffsetQuery, stride, flags);

            if (m_parametersGraphic.resetType == RESET_TYPE_AFTER_COPY)
                vk.cmdResetQueryPool(*primaryCmdBuffer, *queryPool, 0u, queryCount);

            const VkBufferMemoryBarrier barrier = {
                VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER, //  VkStructureType sType;
                nullptr,                                 //  const void* pNext;
                VK_ACCESS_TRANSFER_WRITE_BIT,            //  VkAccessFlags srcAccessMask;
                VK_ACCESS_HOST_READ_BIT,                 //  VkAccessFlags dstAccessMask;
                VK_QUEUE_FAMILY_IGNORED,                 //  uint32_t srcQueueFamilyIndex;
                VK_QUEUE_FAMILY_IGNORED,                 //  uint32_t destQueueFamilyIndex;
                m_resetBuffer->object(),                 //  VkBuffer buffer;
                0u,                                      //  VkDeviceSize offset;
                queryCount * stride + dstOffsetQuery,    //  VkDeviceSize size;
            };
            vk.cmdPipelineBarrier(*primaryCmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                                  (VkDependencyFlags)0, 0, nullptr, 1u, &barrier, 0, nullptr);
        }

        if (!m_parametersGraphic.noColorAttachments)
            transition2DImage(vk, *primaryCmdBuffer, m_colorAttachmentImage->object(), VK_IMAGE_ASPECT_COLOR_BIT,
                              VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
                              VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
                              VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
    }
    endCommandBuffer(vk, *primaryCmdBuffer);

    if (m_parametersGraphic.resetType == RESET_TYPE_HOST)
        vk.resetQueryPool(device, *queryPool, 0u, queryCount);

    // Wait for completion
    submitCommandsAndWait(vk, device, queue, *primaryCmdBuffer);
    return checkResult(*queryPool);
}

class GeometryShaderTestInstance : public GraphicBasicTestInstance
{
public:
    GeometryShaderTestInstance(vkt::Context &context, const std::vector<VertexData> &data,
                               const ParametersGraphic &parametersGraphic, const std::vector<uint64_t> &drawRepeats);

protected:
    virtual void checkExtensions(bool hostQueryResetEnabled);
    virtual void createPipeline(void);
    virtual tcu::TestStatus executeTest(void);
    tcu::TestStatus checkResult(VkQueryPool queryPool);
    void draw(VkCommandBuffer cmdBuffer);
};

GeometryShaderTestInstance::GeometryShaderTestInstance(vkt::Context &context, const std::vector<VertexData> &data,
                                                       const ParametersGraphic &parametersGraphic,
                                                       const std::vector<uint64_t> &drawRepeats)
    : GraphicBasicTestInstance(context, data, parametersGraphic, drawRepeats)
{
}

void GeometryShaderTestInstance::checkExtensions(bool hostQueryResetEnabled)
{
    StatisticQueryTestInstance::checkExtensions(hostQueryResetEnabled);
    if (!m_context.getDeviceFeatures().geometryShader)
        throw tcu::NotSupportedError("Geometry shader are not supported");
}

void GeometryShaderTestInstance::createPipeline(void)
{
    const DeviceInterface &vk       = m_context.getDeviceInterface();
    const VkDevice device           = m_context.getDevice();
    const VkBool32 useGeomPointSize = m_context.getDeviceFeatures().shaderTessellationAndGeometryPointSize &&
                                      (m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_POINT_LIST);

    // Pipeline
    Unique<VkShaderModule> vs(
        createShaderModule(vk, device, m_context.getBinaryCollection().get("vertex"), (VkShaderModuleCreateFlags)0));
    Unique<VkShaderModule> gs(createShaderModule(
        vk, device, m_context.getBinaryCollection().get(useGeomPointSize ? "geometry_point_size" : "geometry"),
        (VkShaderModuleCreateFlags)0));
    Unique<VkShaderModule> fs(
        createShaderModule(vk, device, m_context.getBinaryCollection().get("fragment"), (VkShaderModuleCreateFlags)0));

    const PipelineCreateInfo::ColorBlendState::Attachment attachmentState;

    std::vector<VkPushConstantRange> pcRanges;

    if (m_parametersGraphic.noColorAttachments)
        pcRanges.push_back(makePushConstantRange(VK_SHADER_STAGE_FRAGMENT_BIT, kFloatSize, kFloatSize));

    const PipelineLayoutCreateInfo pipelineLayoutCreateInfo(std::vector<VkDescriptorSetLayout>(), de::sizeU32(pcRanges),
                                                            de::dataOrNull(pcRanges));

    m_pipelineLayout = createPipelineLayout(vk, device, &pipelineLayoutCreateInfo);

    const VkVertexInputBindingDescription vertexInputBindingDescription = {
        0u,                                        // binding;
        static_cast<uint32_t>(sizeof(VertexData)), // stride;
        VK_VERTEX_INPUT_RATE_VERTEX                // inputRate
    };

    const VkVertexInputAttributeDescription vertexInputAttributeDescriptions[] = {
        {0u, 0u, VK_FORMAT_R32G32B32A32_SFLOAT, 0u}, // VertexElementData::position
        {1u, 0u, VK_FORMAT_R32G32B32A32_SFLOAT, static_cast<uint32_t>(sizeof(tcu::Vec4))}, // VertexElementData::color
    };

    const VkPipelineVertexInputStateCreateInfo vf_info = {
        // sType;
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, // pNext;
        NULL,                                                      // flags;
        0u,                                                        // vertexBindingDescriptionCount;
        1,                                                         // pVertexBindingDescriptions;
        &vertexInputBindingDescription,                            // vertexAttributeDescriptionCount;
        2,                                                         // pVertexAttributeDescriptions;
        vertexInputAttributeDescriptions};

    PipelineCreateInfo pipelineCreateInfo(*m_pipelineLayout, *m_renderPass, 0, (VkPipelineCreateFlags)0);
    pipelineCreateInfo.addShader(PipelineCreateInfo::PipelineShaderStage(*vs, "main", VK_SHADER_STAGE_VERTEX_BIT));
    pipelineCreateInfo.addShader(PipelineCreateInfo::PipelineShaderStage(*gs, "main", VK_SHADER_STAGE_GEOMETRY_BIT));
    pipelineCreateInfo.addShader(PipelineCreateInfo::PipelineShaderStage(*fs, "main", VK_SHADER_STAGE_FRAGMENT_BIT));
    pipelineCreateInfo.addState(PipelineCreateInfo::InputAssemblerState(m_parametersGraphic.primitiveTopology));
    pipelineCreateInfo.addState(PipelineCreateInfo::ColorBlendState(1, &attachmentState));

    const VkViewport viewport = makeViewport(m_width, m_height);
    const VkRect2D scissor    = makeRect2D(m_width, m_height);

    pipelineCreateInfo.addState(
        PipelineCreateInfo::ViewportState(1, std::vector<VkViewport>(1, viewport), std::vector<VkRect2D>(1, scissor)));

    if (m_context.getDeviceFeatures().depthBounds)
        pipelineCreateInfo.addState(
            PipelineCreateInfo::DepthStencilState(true, true, VK_COMPARE_OP_GREATER_OR_EQUAL, true));
    else
        pipelineCreateInfo.addState(PipelineCreateInfo::DepthStencilState());

    pipelineCreateInfo.addState(PipelineCreateInfo::RasterizerState(false));
    pipelineCreateInfo.addState(PipelineCreateInfo::MultiSampleState());
    pipelineCreateInfo.addState(vf_info);
    m_pipeline = createGraphicsPipeline(vk, device, VK_NULL_HANDLE, &pipelineCreateInfo);
}

tcu::TestStatus GeometryShaderTestInstance::executeTest(void)
{
    const DeviceInterface &vk       = m_context.getDeviceInterface();
    const VkDevice device           = m_context.getDevice();
    const VkQueue queue             = m_context.getUniversalQueue();
    const uint32_t queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();

    const CmdPoolCreateInfo cmdPoolCreateInfo(queueFamilyIndex);
    const Move<VkCommandPool> cmdPool = createCommandPool(vk, device, &cmdPoolCreateInfo);
    const uint32_t queryCount         = static_cast<uint32_t>(m_drawRepeats.size());
    const Unique<VkQueryPool> queryPool(makeQueryPool(vk, device, queryCount, m_parametersGraphic.queryStatisticFlags));

    const VkDeviceSize vertexBufferOffset = 0u;
    const BufferPtr vertexBufferSp        = creatAndFillVertexBuffer();
    const VkBuffer vertexBuffer           = vertexBufferSp->object();

    const Unique<VkCommandBuffer> cmdBuffer(
        allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

    beginCommandBuffer(vk, *cmdBuffer);
    {
        std::vector<VkClearValue> renderPassClearValues(2);
        deMemset(&renderPassClearValues[0], 0, static_cast<int>(renderPassClearValues.size()) * sizeof(VkClearValue));

        if (!m_parametersGraphic.noColorAttachments)
            initialTransitionColor2DImage(
                vk, *cmdBuffer, m_colorAttachmentImage->object(), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

        initialTransitionDepth2DImage(
            vk, *cmdBuffer, m_depthImage->object(), VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT);

        if (m_parametersGraphic.resetType != RESET_TYPE_HOST)
            vk.cmdResetQueryPool(*cmdBuffer, *queryPool, 0u, queryCount);

        beginRenderPass(vk, *cmdBuffer, *m_renderPass, *m_framebuffer, makeRect2D(0, 0, m_width, m_height),
                        (uint32_t)renderPassClearValues.size(), &renderPassClearValues[0]);

        for (uint32_t i = 0; i < queryCount; ++i)
        {
            vk.cmdBeginQuery(*cmdBuffer, *queryPool, i, (VkQueryControlFlags)0u);
            vk.cmdBindVertexBuffers(*cmdBuffer, 0, 1, &vertexBuffer, &vertexBufferOffset);
            vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipeline);

            for (uint64_t j = 0; j < m_drawRepeats[i]; ++j)
                draw(*cmdBuffer);

            vk.cmdEndQuery(*cmdBuffer, *queryPool, i);
        }

        endRenderPass(vk, *cmdBuffer);

        if (m_parametersGraphic.resetType == RESET_TYPE_BEFORE_COPY ||
            m_parametersGraphic.resetType == RESET_TYPE_AFTER_COPY || m_parametersGraphic.copyType == COPY_TYPE_CMD)
        {
            VkDeviceSize stride          = m_parametersGraphic.querySizeFlags() ? sizeof(uint64_t) : sizeof(uint32_t);
            vk::VkQueryResultFlags flags = m_parametersGraphic.querySizeFlags() | VK_QUERY_RESULT_WAIT_BIT;

            if (m_parametersGraphic.resetType == RESET_TYPE_HOST)
            {
                flags |= VK_QUERY_RESULT_WITH_AVAILABILITY_BIT;
                stride *= 2u;
            }

            if (m_parametersGraphic.resetType == RESET_TYPE_BEFORE_COPY)
            {
                vk.cmdResetQueryPool(*cmdBuffer, *queryPool, 0u, queryCount);
                flags  = VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT;
                stride = sizeof(ValueAndAvailability);
            }

            VkDeviceSize dstOffsetQuery = (m_parametersGraphic.dstOffset) ? stride : 0;
            vk.cmdCopyQueryPoolResults(*cmdBuffer, *queryPool, 0, queryCount, m_resetBuffer->object(), dstOffsetQuery,
                                       stride, flags);

            if (m_parametersGraphic.resetType == RESET_TYPE_AFTER_COPY)
                vk.cmdResetQueryPool(*cmdBuffer, *queryPool, 0u, queryCount);

            const VkBufferMemoryBarrier barrier = {
                VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER, //  VkStructureType sType;
                nullptr,                                 //  const void* pNext;
                VK_ACCESS_TRANSFER_WRITE_BIT,            //  VkAccessFlags srcAccessMask;
                VK_ACCESS_HOST_READ_BIT,                 //  VkAccessFlags dstAccessMask;
                VK_QUEUE_FAMILY_IGNORED,                 //  uint32_t srcQueueFamilyIndex;
                VK_QUEUE_FAMILY_IGNORED,                 //  uint32_t destQueueFamilyIndex;
                m_resetBuffer->object(),                 //  VkBuffer buffer;
                0u,                                      //  VkDeviceSize offset;
                queryCount * stride + dstOffsetQuery,    //  VkDeviceSize size;
            };
            vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                                  (VkDependencyFlags)0, 0, nullptr, 1u, &barrier, 0, nullptr);
        }

        if (!m_parametersGraphic.noColorAttachments)
            transition2DImage(vk, *cmdBuffer, m_colorAttachmentImage->object(), VK_IMAGE_ASPECT_COLOR_BIT,
                              VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
                              VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
                              VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
    }
    endCommandBuffer(vk, *cmdBuffer);

    if (m_parametersGraphic.resetType == RESET_TYPE_HOST)
        vk.resetQueryPool(device, *queryPool, 0u, queryCount);

    // Wait for completion
    submitCommandsAndWait(vk, device, queue, *cmdBuffer);
    return checkResult(*queryPool);
}

tcu::TestStatus GeometryShaderTestInstance::checkResult(VkQueryPool queryPool)
{
    const DeviceInterface &vk = m_context.getDeviceInterface();
    const VkDevice device     = m_context.getDevice();
    uint64_t expectedMin      = 0u;

    switch (m_parametersGraphic.queryStatisticFlags)
    {
    case VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_INVOCATIONS_BIT:
        expectedMin = m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_POINT_LIST                ? 16u :
                      m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_LINE_LIST                 ? 8u :
                      m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_LINE_STRIP                ? 15u :
                      m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST             ? 4u :
                      m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP            ? 4u :
                      m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN              ? 4u :
                      m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY  ? 4u :
                      m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY ? 13u :
                      m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY ? 2u :
                      m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY ?
                                                                                                                    6u :
                                                                                                                    0u;
        break;
    case VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_PRIMITIVES_BIT:
    case VK_QUERY_PIPELINE_STATISTIC_CLIPPING_INVOCATIONS_BIT:
    case VK_QUERY_PIPELINE_STATISTIC_CLIPPING_PRIMITIVES_BIT:
        expectedMin =
            m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_POINT_LIST                    ? 112u :
            m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_LINE_LIST                     ? 32u :
            m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_LINE_STRIP                    ? 60u :
            m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST                 ? 8u :
            m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP                ? 8u :
            m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN                  ? 8u :
            m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY      ? 16u :
            m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY     ? 52u :
            m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY  ? 4u :
            m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY ? 12u :
                                                                                                           0u;
        break;
    default:
        DE_FATAL("Unexpected type of statistics query");
        break;
    }

    const uint32_t queryCount   = static_cast<uint32_t>(m_drawRepeats.size());
    const bool checkImageResult = checkImage();

    bool failStatus        = false;
    tcu::TestStatus status = tcu::TestStatus::pass("Pass");

    try
    {
        if (m_parametersGraphic.resetType == RESET_TYPE_NORMAL ||
            m_parametersGraphic.resetType == RESET_TYPE_AFTER_COPY)
        {
            ResultsVector results(queryCount, 0u);

            if (m_parametersGraphic.copyType == COPY_TYPE_CMD)
            {
                const vk::Allocation &allocation = m_resetBuffer->getBoundMemory();
                cmdCopyQueryPoolResultsVector(results, vk, device, allocation, queryCount,
                                              (VK_QUERY_RESULT_WAIT_BIT | m_parametersGraphic.querySizeFlags()),
                                              m_parametersGraphic.dstOffset);
            }
            else
            {
                VK_CHECK(GetQueryPoolResultsVector(results, vk, device, queryPool, 0u, queryCount,
                                                   (VK_QUERY_RESULT_WAIT_BIT | m_parametersGraphic.querySizeFlags())));
            }

            if (results[0] < expectedMin)
                throw tcu::TestStatus::fail("QueryPoolResults incorrect");
            if (queryCount > 1)
            {
                double pearson = calculatePearsonCorrelation(m_drawRepeats, results);
                if (fabs(pearson) < 0.8)
                    throw tcu::TestStatus::fail("QueryPoolResults are nonlinear");
            }
        }
        else if (m_parametersGraphic.resetType == RESET_TYPE_HOST)
        {
            ResultsVectorWithAvailability results(queryCount, pair<uint64_t, uint64_t>(0u, 0u));
            if (m_parametersGraphic.copyType == COPY_TYPE_CMD)
            {
                const vk::Allocation &allocation = m_resetBuffer->getBoundMemory();
                cmdCopyQueryPoolResultsVector(results, vk, device, allocation, queryCount,
                                              (VK_QUERY_RESULT_WAIT_BIT | m_parametersGraphic.querySizeFlags() |
                                               VK_QUERY_RESULT_WITH_AVAILABILITY_BIT),
                                              m_parametersGraphic.dstOffset);
            }
            else
            {
                VK_CHECK(GetQueryPoolResultsVector(results, vk, device, queryPool, 0u, queryCount,
                                                   (VK_QUERY_RESULT_WAIT_BIT | m_parametersGraphic.querySizeFlags() |
                                                    VK_QUERY_RESULT_WITH_AVAILABILITY_BIT)));
            }

            if (results[0].first < expectedMin || results[0].second == 0u)
                throw tcu::TestStatus::fail("QueryPoolResults incorrect");

            if (queryCount > 1)
            {
                double pearson = calculatePearsonCorrelation(m_drawRepeats, results);
                if (fabs(pearson) < 0.8)
                    throw tcu::TestStatus::fail("QueryPoolResults are nonlinear");
            }

            uint64_t temp = results[0].first;

            vk.resetQueryPool(device, queryPool, 0, queryCount);
            vk::VkResult res = GetQueryPoolResultsVector(
                results, vk, device, queryPool, 0u, queryCount,
                (m_parametersGraphic.querySizeFlags() | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT));
            /* From Vulkan spec:
             *
             * If VK_QUERY_RESULT_WAIT_BIT and VK_QUERY_RESULT_PARTIAL_BIT are both not set then no result values are written to pData
             * for queries that are in the unavailable state at the time of the call, and vkGetQueryPoolResults returns VK_NOT_READY.
             * However, availability state is still written to pData for those queries if VK_QUERY_RESULT_WITH_AVAILABILITY_BIT is set.
             */
            if (res != vk::VK_NOT_READY || results[0].first != temp || results[0].second != 0u)
                throw tcu::TestStatus::fail("QueryPoolResults incorrect reset");
        }
        else
        {
            // With RESET_TYPE_BEFORE_COPY, we only need to verify the result after the copy include an availability bit set as zero.
            throw verifyUnavailable();
        }
    }
    catch (const tcu::TestStatus &ts)
    {
        failStatus = true;
        status     = ts;
    }
    catch (...)
    {
        throw;
    }

    if (failStatus)
    {
        return status;
    }

    if ((m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST ||
         m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP) &&
        !checkImageResult)
        return tcu::TestStatus::fail("Result image doesn't match expected image.");

    return status;
}

void GeometryShaderTestInstance::draw(VkCommandBuffer cmdBuffer)
{
    const DeviceInterface &vk = m_context.getDeviceInterface();
    if (m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP ||
        m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
    {
        vk.cmdDraw(cmdBuffer, 3u, 1u, 0u, 1u);
        vk.cmdDraw(cmdBuffer, 3u, 1u, 4u, 1u);
        vk.cmdDraw(cmdBuffer, 3u, 1u, 8u, 2u);
        vk.cmdDraw(cmdBuffer, 3u, 1u, 12u, 3u);
    }
    else
    {
        vk.cmdDraw(cmdBuffer, 16u, 1u, 0u, 0u);
    }
}

class GeometryShaderSecondaryTestInstance : public GeometryShaderTestInstance
{
public:
    GeometryShaderSecondaryTestInstance(vkt::Context &context, const std::vector<VertexData> &data,
                                        const ParametersGraphic &parametersGraphic,
                                        const std::vector<uint64_t> &drawRepeats);

protected:
    virtual tcu::TestStatus executeTest(void);
};

GeometryShaderSecondaryTestInstance::GeometryShaderSecondaryTestInstance(vkt::Context &context,
                                                                         const std::vector<VertexData> &data,
                                                                         const ParametersGraphic &parametersGraphic,
                                                                         const std::vector<uint64_t> &drawRepeats)
    : GeometryShaderTestInstance(context, data, parametersGraphic, drawRepeats)
{
}

tcu::TestStatus GeometryShaderSecondaryTestInstance::executeTest(void)
{
    const DeviceInterface &vk       = m_context.getDeviceInterface();
    const VkDevice device           = m_context.getDevice();
    const VkQueue queue             = m_context.getUniversalQueue();
    const uint32_t queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();

    const CmdPoolCreateInfo cmdPoolCreateInfo(queueFamilyIndex);
    const Move<VkCommandPool> cmdPool = createCommandPool(vk, device, &cmdPoolCreateInfo);
    const uint32_t queryCount         = static_cast<uint32_t>(m_drawRepeats.size());
    const Unique<VkQueryPool> queryPool(makeQueryPool(vk, device, queryCount, m_parametersGraphic.queryStatisticFlags));

    const VkDeviceSize vertexBufferOffset = 0;
    const BufferPtr vertexBufferSp        = creatAndFillVertexBuffer();
    const VkBuffer vertexBuffer           = vertexBufferSp->object();

    const Unique<VkCommandBuffer> primaryCmdBuffer(
        allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));
    std::vector<VkCommandBufferSp> secondaryCmdBuffers(queryCount);

    for (uint32_t i = 0; i < queryCount; ++i)
        secondaryCmdBuffers[i] = VkCommandBufferSp(new vk::Unique<VkCommandBuffer>(
            allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_SECONDARY)));

    for (uint32_t i = 0; i < queryCount; ++i)
    {
        beginSecondaryCommandBuffer(vk, secondaryCmdBuffers[i]->get(), m_parametersGraphic.queryStatisticFlags,
                                    *m_renderPass, *m_framebuffer, VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT);
        vk.cmdBeginQuery(secondaryCmdBuffers[i]->get(), *queryPool, i, (VkQueryControlFlags)0u);
        vk.cmdBindPipeline(secondaryCmdBuffers[i]->get(), VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipeline);
        vk.cmdBindVertexBuffers(secondaryCmdBuffers[i]->get(), 0u, 1u, &vertexBuffer, &vertexBufferOffset);
        for (uint32_t j = 0; j < m_drawRepeats[i]; ++j)
            draw(secondaryCmdBuffers[i]->get());
        vk.cmdEndQuery(secondaryCmdBuffers[i]->get(), *queryPool, i);
        endCommandBuffer(vk, secondaryCmdBuffers[i]->get());
    }

    beginCommandBuffer(vk, *primaryCmdBuffer);
    {
        std::vector<VkClearValue> renderPassClearValues(2);
        deMemset(&renderPassClearValues[0], 0, static_cast<int>(renderPassClearValues.size()) * sizeof(VkClearValue));

        if (!m_parametersGraphic.noColorAttachments)
            initialTransitionColor2DImage(
                vk, *primaryCmdBuffer, m_colorAttachmentImage->object(), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

        initialTransitionDepth2DImage(
            vk, *primaryCmdBuffer, m_depthImage->object(), VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT);

        if (m_parametersGraphic.resetType != RESET_TYPE_HOST)
            vk.cmdResetQueryPool(*primaryCmdBuffer, *queryPool, 0u, queryCount);
        beginRenderPass(vk, *primaryCmdBuffer, *m_renderPass, *m_framebuffer, makeRect2D(0, 0, m_width, m_height),
                        (uint32_t)renderPassClearValues.size(), &renderPassClearValues[0],
                        VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);
        for (uint32_t i = 0; i < queryCount; ++i)
            vk.cmdExecuteCommands(*primaryCmdBuffer, 1u, &(secondaryCmdBuffers[i]->get()));
        endRenderPass(vk, *primaryCmdBuffer);

        if (m_parametersGraphic.resetType == RESET_TYPE_BEFORE_COPY ||
            m_parametersGraphic.resetType == RESET_TYPE_AFTER_COPY || m_parametersGraphic.copyType == COPY_TYPE_CMD)
        {
            VkDeviceSize stride          = m_parametersGraphic.querySizeFlags() ? sizeof(uint64_t) : sizeof(uint32_t);
            vk::VkQueryResultFlags flags = m_parametersGraphic.querySizeFlags() | VK_QUERY_RESULT_WAIT_BIT;

            if (m_parametersGraphic.resetType == RESET_TYPE_HOST)
            {
                flags |= VK_QUERY_RESULT_WITH_AVAILABILITY_BIT;
                stride *= 2u;
            }

            if (m_parametersGraphic.resetType == RESET_TYPE_BEFORE_COPY)
            {
                vk.cmdResetQueryPool(*primaryCmdBuffer, *queryPool, 0u, queryCount);
                flags  = VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT;
                stride = sizeof(ValueAndAvailability);
            }

            VkDeviceSize dstOffsetQuery = (m_parametersGraphic.dstOffset) ? stride : 0;
            vk.cmdCopyQueryPoolResults(*primaryCmdBuffer, *queryPool, 0, queryCount, m_resetBuffer->object(),
                                       dstOffsetQuery, stride, flags);

            if (m_parametersGraphic.resetType == RESET_TYPE_AFTER_COPY)
                vk.cmdResetQueryPool(*primaryCmdBuffer, *queryPool, 0u, queryCount);

            const VkBufferMemoryBarrier barrier = {
                VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER, //  VkStructureType sType;
                nullptr,                                 //  const void* pNext;
                VK_ACCESS_TRANSFER_WRITE_BIT,            //  VkAccessFlags srcAccessMask;
                VK_ACCESS_HOST_READ_BIT,                 //  VkAccessFlags dstAccessMask;
                VK_QUEUE_FAMILY_IGNORED,                 //  uint32_t srcQueueFamilyIndex;
                VK_QUEUE_FAMILY_IGNORED,                 //  uint32_t destQueueFamilyIndex;
                m_resetBuffer->object(),                 //  VkBuffer buffer;
                0u,                                      //  VkDeviceSize offset;
                queryCount * stride + dstOffsetQuery,    //  VkDeviceSize size;
            };
            vk.cmdPipelineBarrier(*primaryCmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                                  (VkDependencyFlags)0, 0, nullptr, 1u, &barrier, 0, nullptr);
        }

        if (!m_parametersGraphic.noColorAttachments)
            transition2DImage(vk, *primaryCmdBuffer, m_colorAttachmentImage->object(), VK_IMAGE_ASPECT_COLOR_BIT,
                              VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
                              VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
                              VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
    }
    endCommandBuffer(vk, *primaryCmdBuffer);

    if (m_parametersGraphic.resetType == RESET_TYPE_HOST)
        vk.resetQueryPool(device, *queryPool, 0u, queryCount);

    // Wait for completion
    submitCommandsAndWait(vk, device, queue, *primaryCmdBuffer);
    return checkResult(*queryPool);
}

class GeometryShaderSecondaryInheritedTestInstance : public GeometryShaderTestInstance
{
public:
    GeometryShaderSecondaryInheritedTestInstance(vkt::Context &context, const std::vector<VertexData> &data,
                                                 const ParametersGraphic &parametersGraphic,
                                                 const std::vector<uint64_t> &drawRepeats);

protected:
    virtual void checkExtensions(bool hostQueryResetEnabled);
    virtual tcu::TestStatus executeTest(void);
};

GeometryShaderSecondaryInheritedTestInstance::GeometryShaderSecondaryInheritedTestInstance(
    vkt::Context &context, const std::vector<VertexData> &data, const ParametersGraphic &parametersGraphic,
    const std::vector<uint64_t> &drawRepeats)
    : GeometryShaderTestInstance(context, data, parametersGraphic, drawRepeats)
{
}

void GeometryShaderSecondaryInheritedTestInstance::checkExtensions(bool hostQueryResetEnabled)
{
    GeometryShaderTestInstance::checkExtensions(hostQueryResetEnabled);
    if (!m_context.getDeviceFeatures().inheritedQueries)
        throw tcu::NotSupportedError("Inherited queries are not supported");
}

tcu::TestStatus GeometryShaderSecondaryInheritedTestInstance::executeTest(void)
{
    const DeviceInterface &vk       = m_context.getDeviceInterface();
    const VkDevice device           = m_context.getDevice();
    const VkQueue queue             = m_context.getUniversalQueue();
    const uint32_t queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();

    const CmdPoolCreateInfo cmdPoolCreateInfo(queueFamilyIndex);
    const Move<VkCommandPool> cmdPool = createCommandPool(vk, device, &cmdPoolCreateInfo);
    const uint32_t queryCount         = static_cast<uint32_t>(m_drawRepeats.size());
    const Unique<VkQueryPool> queryPool(makeQueryPool(vk, device, queryCount, m_parametersGraphic.queryStatisticFlags));

    const VkDeviceSize vertexBufferOffset = 0u;
    const BufferPtr vertexBufferSp        = creatAndFillVertexBuffer();
    const VkBuffer vertexBuffer           = vertexBufferSp->object();

    const Unique<VkCommandBuffer> primaryCmdBuffer(
        allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));
    std::vector<VkCommandBufferSp> secondaryCmdBuffers(queryCount);

    for (uint32_t i = 0; i < queryCount; ++i)
        secondaryCmdBuffers[i] = VkCommandBufferSp(new vk::Unique<VkCommandBuffer>(
            allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_SECONDARY)));

    for (uint32_t i = 0; i < queryCount; ++i)
    {
        beginSecondaryCommandBuffer(vk, secondaryCmdBuffers[i]->get(), m_parametersGraphic.queryStatisticFlags,
                                    *m_renderPass, *m_framebuffer, VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT);
        vk.cmdBindPipeline(secondaryCmdBuffers[i]->get(), VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipeline);
        vk.cmdBindVertexBuffers(secondaryCmdBuffers[i]->get(), 0u, 1u, &vertexBuffer, &vertexBufferOffset);
        for (uint32_t j = 0; j < m_drawRepeats[i]; ++j)
            draw(secondaryCmdBuffers[i]->get());
        endCommandBuffer(vk, secondaryCmdBuffers[i]->get());
    }

    beginCommandBuffer(vk, *primaryCmdBuffer);
    {
        std::vector<VkClearValue> renderPassClearValues(2);
        deMemset(&renderPassClearValues[0], 0, static_cast<int>(renderPassClearValues.size()) * sizeof(VkClearValue));

        if (!m_parametersGraphic.noColorAttachments)
            initialTransitionColor2DImage(
                vk, *primaryCmdBuffer, m_colorAttachmentImage->object(), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

        initialTransitionDepth2DImage(
            vk, *primaryCmdBuffer, m_depthImage->object(), VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT);

        if (m_parametersGraphic.resetType != RESET_TYPE_HOST)
            vk.cmdResetQueryPool(*primaryCmdBuffer, *queryPool, 0u, queryCount);

        for (uint32_t i = 0; i < queryCount; ++i)
        {
            vk.cmdBeginQuery(*primaryCmdBuffer, *queryPool, i, (VkQueryControlFlags)0u);
            beginRenderPass(vk, *primaryCmdBuffer, *m_renderPass, *m_framebuffer, makeRect2D(0, 0, m_width, m_height),
                            (uint32_t)renderPassClearValues.size(), &renderPassClearValues[0],
                            VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);
            vk.cmdExecuteCommands(*primaryCmdBuffer, 1u, &(secondaryCmdBuffers[i]->get()));
            endRenderPass(vk, *primaryCmdBuffer);
            vk.cmdEndQuery(*primaryCmdBuffer, *queryPool, i);
        }

        if (m_parametersGraphic.resetType == RESET_TYPE_BEFORE_COPY ||
            m_parametersGraphic.resetType == RESET_TYPE_AFTER_COPY || m_parametersGraphic.copyType == COPY_TYPE_CMD)
        {
            VkDeviceSize stride          = m_parametersGraphic.querySizeFlags() ? sizeof(uint64_t) : sizeof(uint32_t);
            vk::VkQueryResultFlags flags = m_parametersGraphic.querySizeFlags() | VK_QUERY_RESULT_WAIT_BIT;

            if (m_parametersGraphic.resetType == RESET_TYPE_HOST)
            {
                flags |= VK_QUERY_RESULT_WITH_AVAILABILITY_BIT;
                stride *= 2u;
            }

            if (m_parametersGraphic.resetType == RESET_TYPE_BEFORE_COPY)
            {
                vk.cmdResetQueryPool(*primaryCmdBuffer, *queryPool, 0u, queryCount);
                flags  = VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT;
                stride = sizeof(ValueAndAvailability);
            }

            VkDeviceSize dstOffsetQuery = (m_parametersGraphic.dstOffset) ? stride : 0;
            vk.cmdCopyQueryPoolResults(*primaryCmdBuffer, *queryPool, 0, queryCount, m_resetBuffer->object(),
                                       dstOffsetQuery, stride, flags);

            if (m_parametersGraphic.resetType == RESET_TYPE_AFTER_COPY)
                vk.cmdResetQueryPool(*primaryCmdBuffer, *queryPool, 0u, queryCount);

            const VkBufferMemoryBarrier barrier = {
                VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER, //  VkStructureType sType;
                nullptr,                                 //  const void* pNext;
                VK_ACCESS_TRANSFER_WRITE_BIT,            //  VkAccessFlags srcAccessMask;
                VK_ACCESS_HOST_READ_BIT,                 //  VkAccessFlags dstAccessMask;
                VK_QUEUE_FAMILY_IGNORED,                 //  uint32_t srcQueueFamilyIndex;
                VK_QUEUE_FAMILY_IGNORED,                 //  uint32_t destQueueFamilyIndex;
                m_resetBuffer->object(),                 //  VkBuffer buffer;
                0u,                                      //  VkDeviceSize offset;
                queryCount * stride + dstOffsetQuery,    //  VkDeviceSize size;
            };
            vk.cmdPipelineBarrier(*primaryCmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                                  (VkDependencyFlags)0, 0, nullptr, 1u, &barrier, 0, nullptr);
        }

        if (!m_parametersGraphic.noColorAttachments)
            transition2DImage(vk, *primaryCmdBuffer, m_colorAttachmentImage->object(), VK_IMAGE_ASPECT_COLOR_BIT,
                              VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
                              VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
                              VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
    }
    endCommandBuffer(vk, *primaryCmdBuffer);

    if (m_parametersGraphic.resetType == RESET_TYPE_HOST)
        vk.resetQueryPool(device, *queryPool, 0u, queryCount);

    // Wait for completion
    submitCommandsAndWait(vk, device, queue, *primaryCmdBuffer);
    return checkResult(*queryPool);
}

class TessellationShaderTestInstance : public GraphicBasicTestInstance
{
public:
    TessellationShaderTestInstance(vkt::Context &context, const std::vector<VertexData> &data,
                                   const ParametersGraphic &parametersGraphic,
                                   const std::vector<uint64_t> &drawRepeats);

protected:
    virtual void checkExtensions(bool hostQueryResetEnabled);
    virtual void createPipeline(void);
    virtual tcu::TestStatus executeTest(void);
    virtual tcu::TestStatus checkResult(VkQueryPool queryPool);
    void draw(VkCommandBuffer cmdBuffer);
};

TessellationShaderTestInstance::TessellationShaderTestInstance(vkt::Context &context,
                                                               const std::vector<VertexData> &data,
                                                               const ParametersGraphic &parametersGraphic,
                                                               const std::vector<uint64_t> &drawRepeats)
    : GraphicBasicTestInstance(context, data, parametersGraphic, drawRepeats)
{
}

void TessellationShaderTestInstance::checkExtensions(bool hostQueryResetEnabled)
{
    StatisticQueryTestInstance::checkExtensions(hostQueryResetEnabled);
    if (!m_context.getDeviceFeatures().tessellationShader)
        throw tcu::NotSupportedError("Tessellation shader are not supported");
}

void TessellationShaderTestInstance::createPipeline(void)
{
    const DeviceInterface &vk = m_context.getDeviceInterface();
    const VkDevice device     = m_context.getDevice();

    // Pipeline
    Unique<VkShaderModule> vs(
        createShaderModule(vk, device, m_context.getBinaryCollection().get("vertex"), (VkShaderModuleCreateFlags)0));
    Unique<VkShaderModule> tc(createShaderModule(
        vk, device, m_context.getBinaryCollection().get("tessellation_control"), (VkShaderModuleCreateFlags)0));
    Unique<VkShaderModule> te(createShaderModule(
        vk, device, m_context.getBinaryCollection().get("tessellation_evaluation"), (VkShaderModuleCreateFlags)0));
    Unique<VkShaderModule> fs(
        createShaderModule(vk, device, m_context.getBinaryCollection().get("fragment"), (VkShaderModuleCreateFlags)0));

    const PipelineCreateInfo::ColorBlendState::Attachment attachmentState;

    std::vector<VkPushConstantRange> pcRanges;

    if (m_parametersGraphic.noColorAttachments)
        pcRanges.push_back(makePushConstantRange(VK_SHADER_STAGE_FRAGMENT_BIT, kFloatSize, kFloatSize));

    const PipelineLayoutCreateInfo pipelineLayoutCreateInfo(std::vector<VkDescriptorSetLayout>(), de::sizeU32(pcRanges),
                                                            de::dataOrNull(pcRanges));

    m_pipelineLayout = createPipelineLayout(vk, device, &pipelineLayoutCreateInfo);

    const VkVertexInputBindingDescription vertexInputBindingDescription = {
        0u,                                        // binding;
        static_cast<uint32_t>(sizeof(VertexData)), // stride;
        VK_VERTEX_INPUT_RATE_VERTEX                // inputRate
    };

    const VkVertexInputAttributeDescription vertexInputAttributeDescriptions[] = {
        {0u, 0u, VK_FORMAT_R32G32B32A32_SFLOAT, 0u}, // VertexElementData::position
        {1u, 0u, VK_FORMAT_R32G32B32A32_SFLOAT, static_cast<uint32_t>(sizeof(tcu::Vec4))}, // VertexElementData::color
    };

    const VkPipelineVertexInputStateCreateInfo vf_info = {
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, // sType;
        NULL,                                                      // pNext;
        0u,                                                        // flags;
        1u,                                                        // vertexBindingDescriptionCount;
        &vertexInputBindingDescription,                            // pVertexBindingDescriptions;
        2u,                                                        // vertexAttributeDescriptionCount;
        vertexInputAttributeDescriptions                           // pVertexAttributeDescriptions;
    };

    PipelineCreateInfo pipelineCreateInfo(*m_pipelineLayout, *m_renderPass, 0, (VkPipelineCreateFlags)0);
    pipelineCreateInfo.addShader(PipelineCreateInfo::PipelineShaderStage(*vs, "main", VK_SHADER_STAGE_VERTEX_BIT));
    pipelineCreateInfo.addShader(
        PipelineCreateInfo::PipelineShaderStage(*tc, "main", VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT));
    pipelineCreateInfo.addShader(
        PipelineCreateInfo::PipelineShaderStage(*te, "main", VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT));
    pipelineCreateInfo.addShader(PipelineCreateInfo::PipelineShaderStage(*fs, "main", VK_SHADER_STAGE_FRAGMENT_BIT));
    pipelineCreateInfo.addState(PipelineCreateInfo::TessellationState(
        m_parametersGraphic.tessPatchSize ? m_parametersGraphic.tessPatchSize : 4u));
    pipelineCreateInfo.addState(PipelineCreateInfo::InputAssemblerState(VK_PRIMITIVE_TOPOLOGY_PATCH_LIST));
    pipelineCreateInfo.addState(PipelineCreateInfo::ColorBlendState(1, &attachmentState));

    const VkViewport viewport = makeViewport(m_width, m_height);
    const VkRect2D scissor    = makeRect2D(m_width, m_height);

    pipelineCreateInfo.addState(
        PipelineCreateInfo::ViewportState(1, std::vector<VkViewport>(1, viewport), std::vector<VkRect2D>(1, scissor)));
    pipelineCreateInfo.addState(PipelineCreateInfo::DepthStencilState());
    pipelineCreateInfo.addState(PipelineCreateInfo::RasterizerState());
    pipelineCreateInfo.addState(PipelineCreateInfo::MultiSampleState());
    pipelineCreateInfo.addState(vf_info);
    m_pipeline = createGraphicsPipeline(vk, device, VK_NULL_HANDLE, &pipelineCreateInfo);
}

tcu::TestStatus TessellationShaderTestInstance::executeTest(void)
{
    const DeviceInterface &vk       = m_context.getDeviceInterface();
    const VkDevice device           = m_context.getDevice();
    const VkQueue queue             = m_context.getUniversalQueue();
    const uint32_t queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();

    const CmdPoolCreateInfo cmdPoolCreateInfo(queueFamilyIndex);
    const Move<VkCommandPool> cmdPool = createCommandPool(vk, device, &cmdPoolCreateInfo);
    const uint32_t queryCount         = static_cast<uint32_t>(m_drawRepeats.size());
    const Unique<VkQueryPool> queryPool(makeQueryPool(vk, device, queryCount, m_parametersGraphic.queryStatisticFlags));

    const VkDeviceSize vertexBufferOffset = 0u;
    const BufferPtr vertexBufferSp        = creatAndFillVertexBuffer();
    const VkBuffer vertexBuffer           = vertexBufferSp->object();

    const Unique<VkCommandBuffer> cmdBuffer(
        allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

    beginCommandBuffer(vk, *cmdBuffer);
    {
        std::vector<VkClearValue> renderPassClearValues(2);
        deMemset(&renderPassClearValues[0], 0, static_cast<int>(renderPassClearValues.size()) * sizeof(VkClearValue));

        if (!m_parametersGraphic.noColorAttachments)
            initialTransitionColor2DImage(
                vk, *cmdBuffer, m_colorAttachmentImage->object(), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

        initialTransitionDepth2DImage(
            vk, *cmdBuffer, m_depthImage->object(), VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT);

        if (m_parametersGraphic.resetType != RESET_TYPE_HOST)
            vk.cmdResetQueryPool(*cmdBuffer, *queryPool, 0u, queryCount);

        beginRenderPass(vk, *cmdBuffer, *m_renderPass, *m_framebuffer, makeRect2D(0, 0, m_width, m_height),
                        (uint32_t)renderPassClearValues.size(), &renderPassClearValues[0]);

        for (uint32_t i = 0; i < queryCount; ++i)
        {
            vk.cmdBeginQuery(*cmdBuffer, *queryPool, i, (VkQueryControlFlags)0u);
            vk.cmdBindVertexBuffers(*cmdBuffer, 0, 1, &vertexBuffer, &vertexBufferOffset);
            vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipeline);

            for (uint64_t j = 0; j < m_drawRepeats[i]; ++j)
                draw(*cmdBuffer);

            vk.cmdEndQuery(*cmdBuffer, *queryPool, i);
        }

        endRenderPass(vk, *cmdBuffer);

        if (m_parametersGraphic.resetType == RESET_TYPE_BEFORE_COPY ||
            m_parametersGraphic.resetType == RESET_TYPE_AFTER_COPY || m_parametersGraphic.copyType == COPY_TYPE_CMD)
        {
            VkDeviceSize stride          = m_parametersGraphic.querySizeFlags() ? sizeof(uint64_t) : sizeof(uint32_t);
            vk::VkQueryResultFlags flags = m_parametersGraphic.querySizeFlags() | VK_QUERY_RESULT_WAIT_BIT;

            if (m_parametersGraphic.resetType == RESET_TYPE_HOST)
            {
                flags |= VK_QUERY_RESULT_WITH_AVAILABILITY_BIT;
                stride *= 2u;
            }

            if (m_parametersGraphic.resetType == RESET_TYPE_BEFORE_COPY)
            {
                vk.cmdResetQueryPool(*cmdBuffer, *queryPool, 0u, queryCount);
                flags  = VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT;
                stride = sizeof(ValueAndAvailability);
            }

            VkDeviceSize dstOffsetQuery = (m_parametersGraphic.dstOffset) ? stride : 0;
            vk.cmdCopyQueryPoolResults(*cmdBuffer, *queryPool, 0, queryCount, m_resetBuffer->object(), dstOffsetQuery,
                                       stride, flags);

            if (m_parametersGraphic.resetType == RESET_TYPE_AFTER_COPY)
                vk.cmdResetQueryPool(*cmdBuffer, *queryPool, 0u, queryCount);

            const VkBufferMemoryBarrier barrier = {
                VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER, //  VkStructureType sType;
                nullptr,                                 //  const void* pNext;
                VK_ACCESS_TRANSFER_WRITE_BIT,            //  VkAccessFlags srcAccessMask;
                VK_ACCESS_HOST_READ_BIT,                 //  VkAccessFlags dstAccessMask;
                VK_QUEUE_FAMILY_IGNORED,                 //  uint32_t srcQueueFamilyIndex;
                VK_QUEUE_FAMILY_IGNORED,                 //  uint32_t destQueueFamilyIndex;
                m_resetBuffer->object(),                 //  VkBuffer buffer;
                0u,                                      //  VkDeviceSize offset;
                queryCount * stride + dstOffsetQuery,    //  VkDeviceSize size;
            };
            vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                                  (VkDependencyFlags)0, 0, nullptr, 1u, &barrier, 0, nullptr);
        }

        if (!m_parametersGraphic.noColorAttachments)
            transition2DImage(vk, *cmdBuffer, m_colorAttachmentImage->object(), VK_IMAGE_ASPECT_COLOR_BIT,
                              VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
                              VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
                              VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
    }
    endCommandBuffer(vk, *cmdBuffer);

    if (m_parametersGraphic.resetType == RESET_TYPE_HOST)
        vk.resetQueryPool(device, *queryPool, 0u, queryCount);

    // Wait for completion
    submitCommandsAndWait(vk, device, queue, *cmdBuffer);
    return checkResult(*queryPool);
}

tcu::TestStatus TessellationShaderTestInstance::checkResult(VkQueryPool queryPool)
{
    const DeviceInterface &vk = m_context.getDeviceInterface();
    const VkDevice device     = m_context.getDevice();
    uint64_t expectedMin      = 0u;

    switch (m_parametersGraphic.queryStatisticFlags)
    {
    case VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT:
        expectedMin = m_parametersGraphic.numTessPrimitives;
        break;

    case VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_CONTROL_SHADER_PATCHES_BIT:
        expectedMin = 4u;
        break;

    case VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_EVALUATION_SHADER_INVOCATIONS_BIT:
        if (m_parametersGraphic.primMode == TESS_PRIM_TRIANGLES)
        {
            expectedMin = 76;
        }
        else if (m_parametersGraphic.primMode == TESS_PRIM_ISOLINES)
        {
            expectedMin = 80;
        }
        else // TESS_PRIM_QUADS
        {
            expectedMin = 100;
        }
        break;
    case VK_QUERY_PIPELINE_STATISTIC_CLIPPING_INVOCATIONS_BIT:
        if (m_parametersGraphic.primMode == TESS_PRIM_TRIANGLES)
        {
            expectedMin = m_parametersGraphic.pointMode ? 15u : 16u;
        }
        else if (m_parametersGraphic.primMode == TESS_PRIM_ISOLINES)
        {
            expectedMin = m_parametersGraphic.pointMode ? 9u : 12u;
        }
        else // TESS_PRIM_QUADS
        {
            expectedMin = m_parametersGraphic.pointMode ? 25u : 32u;
        }
        break;

    case VK_QUERY_PIPELINE_STATISTIC_CLIPPING_PRIMITIVES_BIT:
        if (m_parametersGraphic.primMode == TESS_PRIM_TRIANGLES)
        {
            expectedMin = m_parametersGraphic.pointMode ? 15u : 16u;
        }
        else if (m_parametersGraphic.primMode == TESS_PRIM_ISOLINES)
        {
            expectedMin = m_parametersGraphic.pointMode ? 9u : 12u;
        }
        else // TESS_PRIM_QUADS
        {
            expectedMin = m_parametersGraphic.pointMode ? 25u : 32u;
        }
        break;

    default:
        DE_FATAL("Unexpected type of statistics query");
        break;
    }

    const uint32_t queryCount = static_cast<uint32_t>(m_drawRepeats.size());

    tcu::TestStatus status = tcu::TestStatus::pass("Pass");

    try
    {
        if (m_parametersGraphic.resetType == RESET_TYPE_NORMAL ||
            m_parametersGraphic.resetType == RESET_TYPE_AFTER_COPY)
        {
            const bool checkImageResult = checkImage();

            ResultsVector results(queryCount, 0u);
            if (m_parametersGraphic.copyType == COPY_TYPE_CMD)
            {
                const vk::Allocation &allocation = m_resetBuffer->getBoundMemory();
                cmdCopyQueryPoolResultsVector(results, vk, device, allocation, queryCount,
                                              (VK_QUERY_RESULT_WAIT_BIT | m_parametersGraphic.querySizeFlags()),
                                              m_parametersGraphic.dstOffset);
            }
            else
            {
                VK_CHECK(GetQueryPoolResultsVector(results, vk, device, queryPool, 0u, queryCount,
                                                   (VK_QUERY_RESULT_WAIT_BIT | m_parametersGraphic.querySizeFlags())));
            }

            if (results[0] < expectedMin)
            {
                throw tcu::TestStatus::fail("QueryPoolResults incorrect");
            }
            if (queryCount > 1)
            {
                double pearson = calculatePearsonCorrelation(m_drawRepeats, results);
                if (fabs(pearson) < 0.8)
                    throw tcu::TestStatus::fail("QueryPoolResults are nonlinear");
            }

            // Skip image verification for point mode tessellation tests
            if (!m_parametersGraphic.noColorAttachments && !m_parametersGraphic.pointMode && !checkImageResult)
                throw tcu::TestStatus::fail("Result image doesn't match expected image.");
        }
        else if (m_parametersGraphic.resetType == RESET_TYPE_HOST)
        {
            ResultsVectorWithAvailability results(queryCount, pair<uint64_t, uint64_t>(0u, 0u));
            if (m_parametersGraphic.copyType == COPY_TYPE_CMD)
            {
                const vk::Allocation &allocation = m_resetBuffer->getBoundMemory();
                cmdCopyQueryPoolResultsVector(results, vk, device, allocation, queryCount,
                                              (VK_QUERY_RESULT_WAIT_BIT | m_parametersGraphic.querySizeFlags() |
                                               VK_QUERY_RESULT_WITH_AVAILABILITY_BIT),
                                              m_parametersGraphic.dstOffset);
            }
            else
            {
                VK_CHECK(GetQueryPoolResultsVector(results, vk, device, queryPool, 0u, queryCount,
                                                   (VK_QUERY_RESULT_WAIT_BIT | m_parametersGraphic.querySizeFlags() |
                                                    VK_QUERY_RESULT_WITH_AVAILABILITY_BIT)));
            }

            if (results[0].first < expectedMin || results[0].second == 0u)
            {
                // return tcu::TestStatus::fail("QueryPoolResults incorrect");
                std::ostringstream msg;
                msg << "QueryPoolResults incorrect: expected at least " << expectedMin << " but got "
                    << results[0].first << " or the " << results[0].second << " was equal to 0u";
                return tcu::TestStatus::fail(msg.str());
            }

            if (queryCount > 1)
            {
                double pearson = calculatePearsonCorrelation(m_drawRepeats, results);
                if (fabs(pearson) < 0.8)
                    return tcu::TestStatus::fail("QueryPoolResults are nonlinear");
            }

            uint64_t temp = results[0].first;

            vk.resetQueryPool(device, queryPool, 0, queryCount);
            vk::VkResult res = GetQueryPoolResultsVector(
                results, vk, device, queryPool, 0u, queryCount,
                (m_parametersGraphic.querySizeFlags() | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT));
            /* From Vulkan spec:
             *
             * If VK_QUERY_RESULT_WAIT_BIT and VK_QUERY_RESULT_PARTIAL_BIT are both not set then no result values are written to pData
             * for queries that are in the unavailable state at the time of the call, and vkGetQueryPoolResults returns VK_NOT_READY.
             * However, availability state is still written to pData for those queries if VK_QUERY_RESULT_WITH_AVAILABILITY_BIT is set.
             */
            if (res != vk::VK_NOT_READY || results[0].first != temp || results[0].second != 0u)
                return tcu::TestStatus::fail("QueryPoolResults incorrect reset");
        }
        else
        {
            // With RESET_TYPE_BEFORE_COPY, we only need to verify the result after the copy include an availability bit set as zero.
            return verifyUnavailable();
        }
    }
    catch (const tcu::TestStatus &ts)
    {
        status = ts;
    }
    catch (...)
    {
        throw;
    }

    return status;
}

void TessellationShaderTestInstance::draw(VkCommandBuffer cmdBuffer)
{
    const DeviceInterface &vk = m_context.getDeviceInterface();
    vk.cmdDraw(cmdBuffer, static_cast<uint32_t>(m_data.size()), 1u, 0u, 0u);
}

class TessellationShaderSecondrayTestInstance : public TessellationShaderTestInstance
{
public:
    TessellationShaderSecondrayTestInstance(vkt::Context &context, const std::vector<VertexData> &data,
                                            const ParametersGraphic &parametersGraphic,
                                            const std::vector<uint64_t> &drawRepeats);

protected:
    virtual tcu::TestStatus executeTest(void);
};

TessellationShaderSecondrayTestInstance::TessellationShaderSecondrayTestInstance(
    vkt::Context &context, const std::vector<VertexData> &data, const ParametersGraphic &parametersGraphic,
    const std::vector<uint64_t> &drawRepeats)
    : TessellationShaderTestInstance(context, data, parametersGraphic, drawRepeats)
{
}

tcu::TestStatus TessellationShaderSecondrayTestInstance::executeTest(void)
{
    const DeviceInterface &vk       = m_context.getDeviceInterface();
    const VkDevice device           = m_context.getDevice();
    const VkQueue queue             = m_context.getUniversalQueue();
    const uint32_t queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();

    const CmdPoolCreateInfo cmdPoolCreateInfo(queueFamilyIndex);
    const Move<VkCommandPool> cmdPool = createCommandPool(vk, device, &cmdPoolCreateInfo);
    const uint32_t queryCount         = static_cast<uint32_t>(m_drawRepeats.size());
    const Unique<VkQueryPool> queryPool(makeQueryPool(vk, device, queryCount, m_parametersGraphic.queryStatisticFlags));

    const VkDeviceSize vertexBufferOffset = 0u;
    const BufferPtr vertexBufferSp        = creatAndFillVertexBuffer();
    const VkBuffer vertexBuffer           = vertexBufferSp->object();

    const Unique<VkCommandBuffer> primaryCmdBuffer(
        allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));
    std::vector<VkCommandBufferSp> secondaryCmdBuffers(queryCount);

    for (uint32_t i = 0; i < queryCount; ++i)
        secondaryCmdBuffers[i] = VkCommandBufferSp(new vk::Unique<VkCommandBuffer>(
            allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_SECONDARY)));

    for (uint32_t i = 0; i < queryCount; ++i)
    {
        beginSecondaryCommandBuffer(vk, secondaryCmdBuffers[i]->get(), m_parametersGraphic.queryStatisticFlags,
                                    *m_renderPass, *m_framebuffer, VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT);
        vk.cmdBeginQuery(secondaryCmdBuffers[i]->get(), *queryPool, i, (VkQueryControlFlags)0u);
        vk.cmdBindPipeline(secondaryCmdBuffers[i]->get(), VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipeline);
        vk.cmdBindVertexBuffers(secondaryCmdBuffers[i]->get(), 0u, 1u, &vertexBuffer, &vertexBufferOffset);
        for (uint32_t j = 0; j < m_drawRepeats[i]; ++j)
            draw(secondaryCmdBuffers[i]->get());
        vk.cmdEndQuery(secondaryCmdBuffers[i]->get(), *queryPool, i);
        endCommandBuffer(vk, secondaryCmdBuffers[i]->get());
    }

    beginCommandBuffer(vk, *primaryCmdBuffer);
    {
        std::vector<VkClearValue> renderPassClearValues(2);
        deMemset(&renderPassClearValues[0], 0, static_cast<int>(renderPassClearValues.size()) * sizeof(VkClearValue));

        if (!m_parametersGraphic.noColorAttachments)
            initialTransitionColor2DImage(
                vk, *primaryCmdBuffer, m_colorAttachmentImage->object(), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

        initialTransitionDepth2DImage(
            vk, *primaryCmdBuffer, m_depthImage->object(), VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT);

        vk.cmdBindVertexBuffers(*primaryCmdBuffer, 0u, 1u, &vertexBuffer, &vertexBufferOffset);
        vk.cmdBindPipeline(*primaryCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipeline);

        if (m_parametersGraphic.resetType != RESET_TYPE_HOST)
            vk.cmdResetQueryPool(*primaryCmdBuffer, *queryPool, 0u, queryCount);

        beginRenderPass(vk, *primaryCmdBuffer, *m_renderPass, *m_framebuffer, makeRect2D(0, 0, m_width, m_height),
                        (uint32_t)renderPassClearValues.size(), &renderPassClearValues[0],
                        VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);
        for (uint32_t i = 0; i < queryCount; ++i)
            vk.cmdExecuteCommands(*primaryCmdBuffer, 1u, &(secondaryCmdBuffers[i]->get()));
        endRenderPass(vk, *primaryCmdBuffer);

        if (m_parametersGraphic.resetType == RESET_TYPE_BEFORE_COPY ||
            m_parametersGraphic.resetType == RESET_TYPE_AFTER_COPY || m_parametersGraphic.copyType == COPY_TYPE_CMD)
        {
            VkDeviceSize stride          = m_parametersGraphic.querySizeFlags() ? sizeof(uint64_t) : sizeof(uint32_t);
            vk::VkQueryResultFlags flags = m_parametersGraphic.querySizeFlags() | VK_QUERY_RESULT_WAIT_BIT;
            uint32_t queryCountTess      = queryCount;

            if (m_parametersGraphic.resetType == RESET_TYPE_HOST)
            {
                flags |= VK_QUERY_RESULT_WITH_AVAILABILITY_BIT;
                stride *= 2u;
            }

            if (m_parametersGraphic.resetType == RESET_TYPE_BEFORE_COPY)
            {
                vk.cmdResetQueryPool(*primaryCmdBuffer, *queryPool, 0u, queryCount);
                flags          = VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT;
                stride         = sizeof(ValueAndAvailability);
                queryCountTess = 1u;
            }

            VkDeviceSize dstOffsetQuery = (m_parametersGraphic.dstOffset) ? stride : 0;
            vk.cmdCopyQueryPoolResults(*primaryCmdBuffer, *queryPool, 0, queryCountTess, m_resetBuffer->object(),
                                       dstOffsetQuery, stride, flags);

            if (m_parametersGraphic.resetType == RESET_TYPE_AFTER_COPY)
                vk.cmdResetQueryPool(*primaryCmdBuffer, *queryPool, 0u, queryCount);

            const VkBufferMemoryBarrier barrier = {
                VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,  //  VkStructureType sType;
                nullptr,                                  //  const void* pNext;
                VK_ACCESS_TRANSFER_WRITE_BIT,             //  VkAccessFlags srcAccessMask;
                VK_ACCESS_HOST_READ_BIT,                  //  VkAccessFlags dstAccessMask;
                VK_QUEUE_FAMILY_IGNORED,                  //  uint32_t srcQueueFamilyIndex;
                VK_QUEUE_FAMILY_IGNORED,                  //  uint32_t destQueueFamilyIndex;
                m_resetBuffer->object(),                  //  VkBuffer buffer;
                0u,                                       //  VkDeviceSize offset;
                queryCountTess * stride + dstOffsetQuery, //  VkDeviceSize size;
            };
            vk.cmdPipelineBarrier(*primaryCmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                                  (VkDependencyFlags)0, 0, nullptr, 1u, &barrier, 0, nullptr);
        }

        if (!m_parametersGraphic.noColorAttachments)
            transition2DImage(vk, *primaryCmdBuffer, m_colorAttachmentImage->object(), VK_IMAGE_ASPECT_COLOR_BIT,
                              VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
                              VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
                              VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
    }
    endCommandBuffer(vk, *primaryCmdBuffer);

    if (m_parametersGraphic.resetType == RESET_TYPE_HOST)
        vk.resetQueryPool(device, *queryPool, 0u, queryCount);

    // Wait for completion
    submitCommandsAndWait(vk, device, queue, *primaryCmdBuffer);
    return checkResult(*queryPool);
}

class TessellationShaderSecondrayInheritedTestInstance : public TessellationShaderTestInstance
{
public:
    TessellationShaderSecondrayInheritedTestInstance(vkt::Context &context, const std::vector<VertexData> &data,
                                                     const ParametersGraphic &parametersGraphic,
                                                     const std::vector<uint64_t> &drawRepeats);

protected:
    virtual void checkExtensions(bool hostQueryResetEnabled);
    virtual tcu::TestStatus executeTest(void);
};

TessellationShaderSecondrayInheritedTestInstance::TessellationShaderSecondrayInheritedTestInstance(
    vkt::Context &context, const std::vector<VertexData> &data, const ParametersGraphic &parametersGraphic,
    const std::vector<uint64_t> &drawRepeats)
    : TessellationShaderTestInstance(context, data, parametersGraphic, drawRepeats)
{
}

void TessellationShaderSecondrayInheritedTestInstance::checkExtensions(bool hostQueryResetEnabled)
{
    TessellationShaderTestInstance::checkExtensions(hostQueryResetEnabled);
    if (!m_context.getDeviceFeatures().inheritedQueries)
        throw tcu::NotSupportedError("Inherited queries are not supported");
}

tcu::TestStatus TessellationShaderSecondrayInheritedTestInstance::executeTest(void)
{
    const DeviceInterface &vk       = m_context.getDeviceInterface();
    const VkDevice device           = m_context.getDevice();
    const VkQueue queue             = m_context.getUniversalQueue();
    const uint32_t queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();

    const CmdPoolCreateInfo cmdPoolCreateInfo(queueFamilyIndex);
    const Move<VkCommandPool> cmdPool = createCommandPool(vk, device, &cmdPoolCreateInfo);
    const uint32_t queryCount         = static_cast<uint32_t>(m_drawRepeats.size());
    const Unique<VkQueryPool> queryPool(makeQueryPool(vk, device, queryCount, m_parametersGraphic.queryStatisticFlags));

    const VkDeviceSize vertexBufferOffset = 0u;
    const BufferPtr vertexBufferSp        = creatAndFillVertexBuffer();
    const VkBuffer vertexBuffer           = vertexBufferSp->object();

    const Unique<VkCommandBuffer> primaryCmdBuffer(
        allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));
    std::vector<VkCommandBufferSp> secondaryCmdBuffers(queryCount);

    for (uint32_t i = 0; i < queryCount; ++i)
        secondaryCmdBuffers[i] = VkCommandBufferSp(new vk::Unique<VkCommandBuffer>(
            allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_SECONDARY)));

    for (uint32_t i = 0; i < queryCount; ++i)
    {
        beginSecondaryCommandBuffer(vk, secondaryCmdBuffers[i]->get(), m_parametersGraphic.queryStatisticFlags,
                                    *m_renderPass, *m_framebuffer, VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT);
        vk.cmdBindPipeline(secondaryCmdBuffers[i]->get(), VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipeline);
        vk.cmdBindVertexBuffers(secondaryCmdBuffers[i]->get(), 0u, 1u, &vertexBuffer, &vertexBufferOffset);
        for (uint32_t j = 0; j < m_drawRepeats[i]; ++j)
            draw(secondaryCmdBuffers[i]->get());
        endCommandBuffer(vk, secondaryCmdBuffers[i]->get());
    }

    beginCommandBuffer(vk, *primaryCmdBuffer);
    {
        std::vector<VkClearValue> renderPassClearValues(2);
        deMemset(&renderPassClearValues[0], 0, static_cast<int>(renderPassClearValues.size()) * sizeof(VkClearValue));

        if (!m_parametersGraphic.noColorAttachments)
            initialTransitionColor2DImage(
                vk, *primaryCmdBuffer, m_colorAttachmentImage->object(), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

        initialTransitionDepth2DImage(
            vk, *primaryCmdBuffer, m_depthImage->object(), VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT);

        if (m_parametersGraphic.resetType != RESET_TYPE_HOST)
            vk.cmdResetQueryPool(*primaryCmdBuffer, *queryPool, 0u, queryCount);

        for (uint32_t i = 0; i < queryCount; ++i)
        {
            vk.cmdBeginQuery(*primaryCmdBuffer, *queryPool, i, (VkQueryControlFlags)0u);
            beginRenderPass(vk, *primaryCmdBuffer, *m_renderPass, *m_framebuffer, makeRect2D(0, 0, m_width, m_height),
                            (uint32_t)renderPassClearValues.size(), &renderPassClearValues[0],
                            VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);
            vk.cmdExecuteCommands(*primaryCmdBuffer, 1u, &(secondaryCmdBuffers[i]->get()));
            endRenderPass(vk, *primaryCmdBuffer);
            vk.cmdEndQuery(*primaryCmdBuffer, *queryPool, i);
        }

        if (m_parametersGraphic.resetType == RESET_TYPE_BEFORE_COPY ||
            m_parametersGraphic.resetType == RESET_TYPE_AFTER_COPY || m_parametersGraphic.copyType == COPY_TYPE_CMD)
        {
            VkDeviceSize stride          = m_parametersGraphic.querySizeFlags() ? sizeof(uint64_t) : sizeof(uint32_t);
            vk::VkQueryResultFlags flags = m_parametersGraphic.querySizeFlags() | VK_QUERY_RESULT_WAIT_BIT;

            if (m_parametersGraphic.resetType == RESET_TYPE_HOST)
            {
                flags |= VK_QUERY_RESULT_WITH_AVAILABILITY_BIT;
                stride *= 2u;
            }

            if (m_parametersGraphic.resetType == RESET_TYPE_BEFORE_COPY)
            {
                vk.cmdResetQueryPool(*primaryCmdBuffer, *queryPool, 0u, queryCount);
                flags  = VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT;
                stride = sizeof(ValueAndAvailability);
            }

            VkDeviceSize dstOffsetQuery = (m_parametersGraphic.dstOffset) ? stride : 0;
            vk.cmdCopyQueryPoolResults(*primaryCmdBuffer, *queryPool, 0, queryCount, m_resetBuffer->object(),
                                       dstOffsetQuery, stride, flags);

            if (m_parametersGraphic.resetType == RESET_TYPE_AFTER_COPY)
                vk.cmdResetQueryPool(*primaryCmdBuffer, *queryPool, 0u, queryCount);

            const VkBufferMemoryBarrier barrier = {
                VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER, //  VkStructureType sType;
                nullptr,                                 //  const void* pNext;
                VK_ACCESS_TRANSFER_WRITE_BIT,            //  VkAccessFlags srcAccessMask;
                VK_ACCESS_HOST_READ_BIT,                 //  VkAccessFlags dstAccessMask;
                VK_QUEUE_FAMILY_IGNORED,                 //  uint32_t srcQueueFamilyIndex;
                VK_QUEUE_FAMILY_IGNORED,                 //  uint32_t destQueueFamilyIndex;
                m_resetBuffer->object(),                 //  VkBuffer buffer;
                0u,                                      //  VkDeviceSize offset;
                queryCount * stride + dstOffsetQuery,    //  VkDeviceSize size;
            };
            vk.cmdPipelineBarrier(*primaryCmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                                  (VkDependencyFlags)0, 0, nullptr, 1u, &barrier, 0, nullptr);
        }

        if (!m_parametersGraphic.noColorAttachments)
            transition2DImage(vk, *primaryCmdBuffer, m_colorAttachmentImage->object(), VK_IMAGE_ASPECT_COLOR_BIT,
                              VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
                              VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
                              VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
    }
    endCommandBuffer(vk, *primaryCmdBuffer);

    if (m_parametersGraphic.resetType == RESET_TYPE_HOST)
        vk.resetQueryPool(device, *queryPool, 0u, queryCount);

    // Wait for completion
    submitCommandsAndWait(vk, device, queue, *primaryCmdBuffer);
    return checkResult(*queryPool);
}

template <class Instance>
class QueryPoolComputeStatsTest : public TestCase
{
public:
    QueryPoolComputeStatsTest(tcu::TestContext &context, const std::string &name, const ResetType resetType,
                              const CopyType copyType, bool query64Bits, const bool useComputeQueue,
                              bool dstOffset = false, const StrideType strideType = STRIDE_TYPE_VALID)
        : TestCase(context, name.c_str())
        , m_useComputeQueue(useComputeQueue)
        , m_cqInfo({VK_QUEUE_COMPUTE_BIT, VK_QUEUE_GRAPHICS_BIT, 1u, 1.0f})
    {
        const tcu::UVec3 localSize[] = {
            tcu::UVec3(2u, 2u, 2u),
            tcu::UVec3(1u, 1u, 1u),
            tcu::UVec3(WIDTH / (7u * 3u), 7u, 3u),
        };

        const tcu::UVec3 groupSize[] = {
            tcu::UVec3(2u, 2u, 2u),
            tcu::UVec3(WIDTH / (7u * 3u), 7u, 3u),
            tcu::UVec3(1u, 1u, 1u),
        };

        DE_ASSERT(DE_LENGTH_OF_ARRAY(localSize) == DE_LENGTH_OF_ARRAY(groupSize));

        for (int shaderNdx = 0; shaderNdx < DE_LENGTH_OF_ARRAY(localSize); ++shaderNdx)
        {
            std::ostringstream shaderName;
            shaderName << "compute_" << shaderNdx;
            const ComputeInvocationsTestInstance::ParametersCompute parameters(
                localSize[shaderNdx], groupSize[shaderNdx], shaderName.str(), resetType, copyType, query64Bits,
                dstOffset, strideType, m_useComputeQueue);
            m_parameters.push_back(parameters);
        }
    }

    vkt::TestInstance *createInstance(vkt::Context &context) const override
    {
        return new Instance(context, m_parameters);
    }

    bool inSubprocess() const
    {
        return getContextManager()->getCommandLine().isSubProcess();
    }

    void checkSupport(Context &context) const override
    {
        if (m_useComputeQueue)
        {
            const auto &vki           = context.getInstanceInterface();
            const auto physicalDevice = context.getPhysicalDevice();

            findQueueFamilyIndexWithCaps(vki, physicalDevice, m_cqInfo.required, m_cqInfo.excluded);
        }
    }

    void initPrograms(SourceCollections &sourceCollections) const override
    {
        std::ostringstream source;
        source << "layout(binding = 0) writeonly buffer Output {\n"
               << "    uint values[];\n"
               << "} sb_out;\n\n"
               << "void main (void) {\n"
               << "    uvec3 indexUvec3 = uvec3 (gl_GlobalInvocationID.x,\n"
               << "                              gl_GlobalInvocationID.y * gl_NumWorkGroups.x * gl_WorkGroupSize.x,\n"
               << "                              gl_GlobalInvocationID.z * gl_NumWorkGroups.x * gl_NumWorkGroups.y * "
                  "gl_WorkGroupSize.x * gl_WorkGroupSize.y);\n"
               << "    uint index = indexUvec3.x + indexUvec3.y + indexUvec3.z;\n"
               << "    sb_out.values[index] += index;\n"
               << "}\n";

        for (size_t shaderNdx = 0; shaderNdx < m_parameters.size(); ++shaderNdx)
        {
            std::ostringstream src;
            src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
                << "layout (local_size_x = " << m_parameters[shaderNdx].localSize.x()
                << ", local_size_y = " << m_parameters[shaderNdx].localSize.y()
                << ", local_size_z = " << m_parameters[shaderNdx].localSize.z() << ") in;\n"
                << source.str();
            sourceCollections.glslSources.add(m_parameters[shaderNdx].shaderName) << glu::ComputeSource(src.str());
        }
    }

    std::string getRequiredCapabilitiesId() const override
    {
        if (m_useComputeQueue)
        {
            return std::type_index(typeid(ComputeInvocationsTestInstance::ParametersCompute)).name();
        }
        return TestCase::getRequiredCapabilitiesId();
    }

    void initDeviceCapabilities(DevCaps &caps) override
    {
        DevCaps::QueueCreateInfo queueInfos[]{m_cqInfo};
        caps.resetQueues(queueInfos);

        caps.addExtension("VK_EXT_host_query_reset");
        caps.addExtension("VK_KHR_portability_subset");

        caps.addFeature<VkPhysicalDeviceHostQueryResetFeatures>();
        caps.addFeature(&VkPhysicalDeviceFeatures::inheritedQueries);
        caps.addFeature(&VkPhysicalDeviceFeatures::pipelineStatisticsQuery);

#ifndef CTS_USES_VULKANSC
        caps.addFeature<VkPhysicalDevicePortabilitySubsetFeaturesKHR>();
#endif
    }

private:
    std::vector<ComputeInvocationsTestInstance::ParametersCompute> m_parameters;
    const bool m_useComputeQueue;
    const DevCaps::QueueCreateInfo m_cqInfo;
};

template <class Instance>
class QueryPoolGraphicStatisticsTest : public TestCase
{
public:
    QueryPoolGraphicStatisticsTest(tcu::TestContext &context, const std::string &name,
                                   const GraphicBasicTestInstance::ParametersGraphic parametersGraphic,
                                   const std::vector<uint64_t> &drawRepeats)
        : TestCase(context, name.c_str())
        , m_parametersGraphic(parametersGraphic)
        , m_drawRepeats(drawRepeats)
        // For clear-skip, we'll use one framebuffer block for each draw instead of clearing.
        , m_blockCount((parametersGraphic.clearOp == CLEAR_SKIP) ?
                           static_cast<uint32_t>(std::accumulate(begin(drawRepeats), end(drawRepeats), uint64_t{0})) :
                           1u)
        , m_width(WIDTH)
        , m_height(HEIGHT * m_blockCount)
    {
        using VertexData = GraphicBasicTestInstance::VertexData;

        if ((m_parametersGraphic.hasTess) && (m_parametersGraphic.tessPatchSize != 0))
        {
            const auto blue = tcu::RGBA::blue().toVec();

            for (uint32_t primitiveCnt = 1; primitiveCnt <= m_parametersGraphic.numTessPrimitives; primitiveCnt++)
                for (uint32_t dataIdx = 0; dataIdx < m_parametersGraphic.tessPatchSize; dataIdx++)
                    m_data.push_back(VertexData(tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f), blue));
        }
        else
        {
            const bool isPoints = (m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_POINT_LIST);
            const bool isLineStripAdj =
                (m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY);
            const bool isLines =
                (m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_LINE_LIST ||
                 m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_LINE_STRIP || isLineStripAdj);
            const bool isTriFan       = (m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN);
            const float quarterWidth  = (2.0f / static_cast<float>(m_width)) * 0.25f;
            const float quarterHeight = (2.0f / static_cast<float>(m_height)) * 0.25f;
            const float marginW       = ((isPoints || isLines) ? quarterWidth : 0.0f);
            const float marginH       = (isPoints ? quarterHeight : 0.0f);

            // These coordinates will be used with different topologies, so we try to avoid drawing points on the edges.
            const float left   = -1.0f + marginW;
            const float right  = 1.0f - marginW;
            const float center = (left + right) / 2.0f;
            const float top    = -1.0f + marginH;
            const float bottom = -1.0f + 2.0f / static_cast<float>(m_blockCount) - marginH;
            const float middle = (top + bottom) / 2.0f;

            const auto red   = tcu::RGBA::red().toVec();
            const auto green = tcu::RGBA::green().toVec();
            const auto blue  = tcu::RGBA::blue().toVec();
            const auto gray  = tcu::RGBA::gray().toVec();

            const bool triListSkip = (m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST &&
                                      m_parametersGraphic.clearOp == CLEAR_SKIP);

            // --- TOP LEFT VERTICES ---
            // For line strips with adjacency, everything is drawn with a single draw call, but we add a first and a last
            // adjacency point so the strip looks like the non-adjacency case.
            if (isLineStripAdj)
                m_data.push_back(VertexData(tcu::Vec4(-2.0f, -2.0f, 1.0f, 1.0f), red));
            m_data.push_back(VertexData(tcu::Vec4(left, top, 1.0f, 1.0f), red));
            m_data.push_back(VertexData(tcu::Vec4(left, middle, 1.0f, 1.0f), red));
            // For triangle fans we'll revert the order of the first 2 vertices in each quadrant so they form a proper fan
            // covering the whole quadrant.
            if (isTriFan)
                std::swap(m_data.at(m_data.size() - 1), m_data.at(m_data.size() - 2));
            m_data.push_back(VertexData(tcu::Vec4(center, top, 1.0f, 1.0f), red));
            if (triListSkip)
            {
                m_data.push_back(VertexData(tcu::Vec4(center, top, 1.0f, 1.0f), red));
                m_data.push_back(VertexData(tcu::Vec4(left, middle, 1.0f, 1.0f), red));
            }
            m_data.push_back(VertexData(tcu::Vec4(center, middle, 1.0f, 1.0f), red));

            // --- BOTTOM LEFT VERTICES ---
            m_data.push_back(VertexData(tcu::Vec4(left, middle, 1.0f, 1.0f), green));
            m_data.push_back(VertexData(tcu::Vec4(left, bottom, 1.0f, 1.0f), green));
            if (isTriFan)
                std::swap(m_data.at(m_data.size() - 1), m_data.at(m_data.size() - 2));
            m_data.push_back(VertexData(tcu::Vec4(center, middle, 1.0f, 1.0f), green));
            if (triListSkip)
            {
                m_data.push_back(VertexData(tcu::Vec4(center, middle, 1.0f, 1.0f), green));
                m_data.push_back(VertexData(tcu::Vec4(left, bottom, 1.0f, 1.0f), green));
            }
            m_data.push_back(VertexData(tcu::Vec4(center, bottom, 1.0f, 1.0f), green));

            // --- TOP RIGHT VERTICES ---
            m_data.push_back(VertexData(tcu::Vec4(center, top, 1.0f, 1.0f), blue));
            m_data.push_back(VertexData(tcu::Vec4(center, middle, 1.0f, 1.0f), blue));
            if (isTriFan)
                std::swap(m_data.at(m_data.size() - 1), m_data.at(m_data.size() - 2));
            m_data.push_back(VertexData(tcu::Vec4(right, top, 1.0f, 1.0f), blue));
            if (triListSkip)
            {
                m_data.push_back(VertexData(tcu::Vec4(right, top, 1.0f, 1.0f), blue));
                m_data.push_back(VertexData(tcu::Vec4(center, middle, 1.0f, 1.0f), blue));
            }
            m_data.push_back(VertexData(tcu::Vec4(right, middle, 1.0f, 1.0f), blue));

            // --- BOTTOM RIGHT VERTICES ---
            m_data.push_back(VertexData(tcu::Vec4(center, middle, 1.0f, 1.0f), gray));
            m_data.push_back(VertexData(tcu::Vec4(center, bottom, 1.0f, 1.0f), gray));
            if (isTriFan)
                std::swap(m_data.at(m_data.size() - 1), m_data.at(m_data.size() - 2));
            m_data.push_back(VertexData(tcu::Vec4(right, middle, 1.0f, 1.0f), gray));
            if (triListSkip)
            {
                m_data.push_back(VertexData(tcu::Vec4(right, middle, 1.0f, 1.0f), gray));
                m_data.push_back(VertexData(tcu::Vec4(center, bottom, 1.0f, 1.0f), gray));
            }
            m_data.push_back(VertexData(tcu::Vec4(right, bottom, 1.0f, 1.0f), gray));
            if (isLineStripAdj)
                m_data.push_back(VertexData(tcu::Vec4(2.0f, 2.0f, 1.0f, 1.0f), red));
        }
    }

    void checkSupport(vkt::Context &context) const
    {
#ifndef CTS_USES_VULKANSC
        if (m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN &&
            context.isDeviceFunctionalitySupported("VK_KHR_portability_subset") &&
            !context.getPortabilitySubsetFeatures().triangleFans)
        {
            TCU_THROW(NotSupportedError,
                      "VK_KHR_portability_subset: Triangle fans are not supported by this implementation");
        }
#else
        DE_UNREF(context);
#endif // CTS_USES_VULKANSC
    }

    vkt::TestInstance *createInstance(vkt::Context &context) const
    {
        return new Instance(context, m_data, m_parametersGraphic, m_drawRepeats);
    }

    void initPrograms(SourceCollections &sourceCollections) const
    {
        { // Vertex Shader
            if (m_parametersGraphic.hasTess && m_parametersGraphic.tessPatchSize != 0)
            {
                // Test VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT with tessellation.
                // Position and color data from vertex buffer will be ignored.
                // Vertex shader provides position and color for the quad.
                std::ostringstream source;
                source << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
                       << "vec4 positions[4] = vec4[](\n"
                       << "    vec4(-1.0f, -1.0f, 0.0f, 1.0f),\n"
                       << "    vec4( 1.0f, -1.0f, 0.0f, 1.0f),\n"
                       << "    vec4(-1.0f,  1.0f, 0.0f, 1.0f),\n"
                       << "    vec4( 1.0f,  1.0f, 0.0f, 1.0f)\n"
                       << ");\n"
                       << "\n"
                       << "layout(location = 0) out vec4 out_color;\n"
                       << "\n"
                       << "void main() {\n"
                       << "    gl_Position = positions[gl_VertexIndex];\n"
                       << "    gl_PointSize = 1.0f;\n"
                       << "    out_color = vec4(0.0f, 0.0f, 1.0f, 1.0f); // blue\n"
                       << "}\n";
                sourceCollections.glslSources.add("vertex") << glu::VertexSource(source.str());
            }
            else
            {
                // For CLEAR_SKIP, we'll use different framebuffer regions with a vertical offset in each draw.
                const bool verticalOffset = (m_parametersGraphic.clearOp == CLEAR_SKIP);

                std::ostringstream source;
                source
                    << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
                    << "layout(location = 0) in highp vec4 in_position;\n"
                    << "layout(location = 1) in vec4 in_color;\n"
                    << "layout(location = 0) out vec4 out_color;\n"
                    << (verticalOffset ?
                            "layout(push_constant, std430) uniform PCBlock { float verticalOffset; } pc;\n" :
                            "")
                    << "void main (void)\n"
                    << "{\n"
                    << "    gl_PointSize = 1.0;\n"
                    << "    const float yOffset = " << (verticalOffset ? "pc.verticalOffset" : "0.0") << ";\n"
                    << "    gl_Position = vec4(in_position.x, in_position.y + yOffset, in_position.z, in_position.w);\n"
                    << "    out_color = in_color;\n"
                    << "}\n";
                sourceCollections.glslSources.add("vertex") << glu::VertexSource(source.str());
            }
        }
        if (m_parametersGraphic.hasTess)
        { // Tessellation control & evaluation
            std::ostringstream source_tc;
            source_tc << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
                      << "#extension GL_EXT_tessellation_shader : require\n";

            // Adjust the output vertices based on primitive mode
            if (m_parametersGraphic.primMode == TESS_PRIM_TRIANGLES)
                source_tc << "layout(vertices = 3) out;\n";
            else if (m_parametersGraphic.primMode == TESS_PRIM_ISOLINES)
                source_tc << "layout(vertices = 2) out;\n";
            else // TESS_PRIM_QUADS
                source_tc << "layout(vertices = 4) out;\n";

            // Define positions array based on primitive mode
            if (m_parametersGraphic.primMode == TESS_PRIM_TRIANGLES)
            {
                source_tc << "vec4 positions[3] = vec4[](\n"
                          << "    vec4(-0.5f, -0.5f, 0.0f, 1.0f),\n"
                          << "    vec4( 0.5f, -0.5f, 0.0f, 1.0f),\n"
                          << "    vec4( 0.0f,  0.5f, 0.0f, 1.0f)\n"
                          << ");\n";
            }
            else if (m_parametersGraphic.primMode == TESS_PRIM_ISOLINES)
            {
                source_tc << "vec4 positions[2] = vec4[](\n"
                          << "    vec4(-0.5f,  0.0f, 0.0f, 1.0f),\n"
                          << "    vec4( 0.5f,  0.0f, 0.0f, 1.0f)\n"
                          << ");\n";
            }
            else // TESS_PRIM_QUADS
            {
                source_tc << "vec4 positions[4] = vec4[](\n"
                          << "    vec4(-1.0f, -1.0f, 0.0f, 1.0f),\n"
                          << "    vec4( 1.0f, -1.0f, 0.0f, 1.0f),\n"
                          << "    vec4(-1.0f,  1.0f, 0.0f, 1.0f),\n"
                          << "    vec4( 1.0f,  1.0f, 0.0f, 1.0f)\n"
                          << ");\n";
            }

            source_tc << "layout(location = 0) in vec4 in_color[];\n"
                      << "layout(location = 0) out vec4 out_color[];\n"
                      << "\n"
                      << "void main (void)\n"
                      << "{\n"
                      << "    if( gl_InvocationID == 0 )\n"
                      << "    {\n";

            // Configure tessellation levels based on the primitive mode
            if (m_parametersGraphic.primMode == TESS_PRIM_TRIANGLES)
            {
                source_tc << "        gl_TessLevelInner[0] = 4.0f;\n"
                          << "        gl_TessLevelOuter[0] = 4.0f;\n"
                          << "        gl_TessLevelOuter[1] = 4.0f;\n"
                          << "        gl_TessLevelOuter[2] = 4.0f;\n";
            }
            else if (m_parametersGraphic.primMode == TESS_PRIM_ISOLINES)
            {
                source_tc << "        gl_TessLevelOuter[0] = 4.0f; // Number of lines\n"
                          << "        gl_TessLevelOuter[1] = 4.0f; // Number of segments per line\n";
            }
            else // TESS_PRIM_QUADS
            {
                source_tc << "        gl_TessLevelInner[0] = 4.0f;\n"
                          << "        gl_TessLevelInner[1] = 4.0f;\n"
                          << "        gl_TessLevelOuter[0] = 4.0f;\n"
                          << "        gl_TessLevelOuter[1] = 4.0f;\n"
                          << "        gl_TessLevelOuter[2] = 4.0f;\n"
                          << "        gl_TessLevelOuter[3] = 4.0f;\n";
            }

            source_tc << "    }\n";

            // Handle patch size customization
            uint32_t verticesNeeded = 4; // Default for quads
            if (m_parametersGraphic.primMode == TESS_PRIM_TRIANGLES)
                verticesNeeded = 3;
            else if (m_parametersGraphic.primMode == TESS_PRIM_ISOLINES)
                verticesNeeded = 2;

            if ((m_parametersGraphic.tessPatchSize > 0) && (m_parametersGraphic.tessPatchSize < verticesNeeded))
            {
                source_tc << "\n"
                          << "    if (gl_InvocationID < " << m_parametersGraphic.tessPatchSize << ")\n"
                          << "    {\n";
            }

            source_tc << "        out_color[gl_InvocationID] = in_color[gl_InvocationID];\n"
                      << "        gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;\n";

            // Provide position and color for missing data
            if ((m_parametersGraphic.tessPatchSize > 0) && (m_parametersGraphic.tessPatchSize < verticesNeeded))
            {
                source_tc << "    }\n"
                          << "    else\n"
                          << "    {\n"
                          << "        out_color[gl_InvocationID] = vec4(0.0f, 0.0f, 1.0f, 1.0f); // blue\n"
                          << "        gl_out[gl_InvocationID].gl_Position = positions[gl_InvocationID];\n"
                          << "    }\n";
            }

            source_tc << "}\n";
            sourceCollections.glslSources.add("tessellation_control")
                << glu::TessellationControlSource(source_tc.str());

            // Tessellation evaluation shader
            std::ostringstream source_te;
            source_te << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
                      << "#extension GL_EXT_tessellation_shader : require\n";

            // Set primitive mode, spacing, and winding
            if (m_parametersGraphic.primMode == TESS_PRIM_TRIANGLES)
                source_te << "layout(triangles, equal_spacing, ccw) in;\n";
            else if (m_parametersGraphic.primMode == TESS_PRIM_ISOLINES)
                source_te << "layout(isolines, equal_spacing) in;\n";
            else // TESS_PRIM_QUADS
                source_te << "layout(quads, equal_spacing, ccw) in;\n";

            // Add point_mode if enabled
            if (m_parametersGraphic.pointMode)
                source_te << "layout(point_mode) in;\n";

            source_te << "layout(location = 0) in vec4 in_color[];\n"
                      << "layout(location = 0) out vec4 out_color;\n"
                      << "void main (void)\n"
                      << "{\n";

            // Position calculation depends on the primitive mode
            if (m_parametersGraphic.primMode == TESS_PRIM_TRIANGLES)
            {
                source_te << "    const float u = gl_TessCoord.x;\n"
                          << "    const float v = gl_TessCoord.y;\n"
                          << "    const float w = gl_TessCoord.z;\n"
                          << "    gl_Position = u * gl_in[0].gl_Position + v * gl_in[1].gl_Position + w * "
                             "gl_in[2].gl_Position;\n";
            }
            else if (m_parametersGraphic.primMode == TESS_PRIM_ISOLINES)
            {
                source_te << "    const float u = gl_TessCoord.x; // Position along the line\n"
                          << "    const float v = gl_TessCoord.y; // Which line\n"
                          << "    gl_Position = mix(gl_in[0].gl_Position, gl_in[1].gl_Position, u);\n";
            }
            else // TESS_PRIM_QUADS
            {
                source_te
                    << "    const float u = gl_TessCoord.x;\n"
                    << "    const float v = gl_TessCoord.y;\n"
                    << "    gl_Position = (1 - u) * (1 - v) * gl_in[0].gl_Position + (1 - u) * v * "
                       "gl_in[1].gl_Position + u * (1 - v) * gl_in[2].gl_Position + u * v * gl_in[3].gl_Position;\n";
            }

            source_te << "    out_color = in_color[0];\n"
                      << "}\n";

            sourceCollections.glslSources.add("tessellation_evaluation")
                << glu::TessellationEvaluationSource(source_te.str());
        }
        if (m_parametersGraphic.queryStatisticFlags & (VK_QUERY_PIPELINE_STATISTIC_CLIPPING_INVOCATIONS_BIT |
                                                       VK_QUERY_PIPELINE_STATISTIC_CLIPPING_PRIMITIVES_BIT |
                                                       VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_INVOCATIONS_BIT |
                                                       VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_PRIMITIVES_BIT))
        { // Geometry Shader
            const bool isTopologyPointSize = m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
            std::ostringstream source;
            source << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
                   << "layout(" << inputTypeToGLString(m_parametersGraphic.primitiveTopology) << ") in;\n"
                   << "layout(" << outputTypeToGLString(m_parametersGraphic.primitiveTopology)
                   << ", max_vertices = 16) out;\n"
                   << "layout(location = 0) in vec4 in_color[];\n"
                   << "layout(location = 0) out vec4 out_color;\n"
                   << "void main (void)\n"
                   << "{\n"
                   << "    out_color = in_color[0];\n"
                   << (isTopologyPointSize ? "${pointSize}" : "") << "    gl_Position = gl_in[0].gl_Position;\n"
                   << "    EmitVertex();\n"
                   << "    EndPrimitive();\n"
                   << "\n"
                   << "    out_color = in_color[0];\n"
                   << (isTopologyPointSize ? "${pointSize}" : "") << "    gl_Position = vec4(1.0, 1.0, 1.0, 1.0);\n"
                   << "    EmitVertex();\n"
                   << "    out_color = in_color[0];\n"
                   << (isTopologyPointSize ? "${pointSize}" : "") << "    gl_Position = vec4(-1.0, -1.0, 1.0, 1.0);\n"
                   << "    EmitVertex();\n"
                   << "    EndPrimitive();\n"
                   << "\n";
            if (m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP ||
                m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
            {
                source << "\n"
                       << "    out_color = in_color[0];\n"
                       << "    gl_Position = gl_in[0].gl_Position;\n"
                       << "    EmitVertex();\n"
                       << "    out_color = in_color[0];\n"
                       << "    gl_Position = gl_in[1].gl_Position;\n"
                       << "    EmitVertex();\n"
                       << "    out_color = in_color[0];\n"
                       << "    gl_Position = gl_in[2].gl_Position;\n"
                       << "    EmitVertex();\n"
                       << "    out_color = in_color[0];\n"
                       << "    gl_Position = vec4(gl_in[2].gl_Position.x, gl_in[1].gl_Position.y, 1.0, 1.0);\n"
                       << "    EmitVertex();\n"
                       << "    EndPrimitive();\n";
            }
            else
            {
                source << "    out_color = in_color[0];\n"
                       << (isTopologyPointSize ? "${pointSize}" : "") << "    gl_Position = vec4(1.0, 1.0, 1.0, 1.0);\n"
                       << "    EmitVertex();\n"
                       << "    out_color = in_color[0];\n"
                       << (isTopologyPointSize ? "${pointSize}" : "")
                       << "    gl_Position = vec4(1.0, -1.0, 1.0, 1.0);\n"
                       << "    EmitVertex();\n"
                       << "    out_color = in_color[0];\n"
                       << (isTopologyPointSize ? "${pointSize}" : "")
                       << "    gl_Position = vec4(-1.0, 1.0, 1.0, 1.0);\n"
                       << "    EmitVertex();\n"
                       << "    out_color = in_color[0];\n"
                       << (isTopologyPointSize ? "${pointSize}" : "")
                       << "    gl_Position = vec4(-1.0, -1.0, 1.0, 1.0);\n"
                       << "    EmitVertex();\n"
                       << "    EndPrimitive();\n";
            }
            source << "}\n";

            if (isTopologyPointSize)
            {
                // Add geometry shader codes with and without gl_PointSize if the primitive topology is VK_PRIMITIVE_TOPOLOGY_POINT_LIST

                tcu::StringTemplate sourceTemplate(source.str());

                std::map<std::string, std::string> pointSize;
                std::map<std::string, std::string> noPointSize;

                pointSize["pointSize"]   = "    gl_PointSize = gl_in[0].gl_PointSize;\n";
                noPointSize["pointSize"] = "";

                sourceCollections.glslSources.add("geometry")
                    << glu::GeometrySource(sourceTemplate.specialize(noPointSize));
                sourceCollections.glslSources.add("geometry_point_size")
                    << glu::GeometrySource(sourceTemplate.specialize(pointSize));
            }
            else
            {
                sourceCollections.glslSources.add("geometry") << glu::GeometrySource(source.str());
            }
        }

        if (!m_parametersGraphic.vertexOnlyPipe)
        { // Fragment Shader
            std::ostringstream source;
            source
                << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
                << "layout(location = 0) in vec4 in_color;\n"
                << "layout(location = 0) out vec4 out_color;\n"
                << (m_parametersGraphic.noColorAttachments ?
                        "layout (push_constant, std430) uniform PCBlock { layout (offset=4) float fragDepth; } pc;\n" :
                        "")
                << "void main()\n"
                << "{\n"
                << "    out_color = in_color;\n"
                << (m_parametersGraphic.noColorAttachments ? "    gl_FragDepth = pc.fragDepth;\n" : "") << "}\n";
            sourceCollections.glslSources.add("fragment") << glu::FragmentSource(source.str());
        }
    }

private:
    std::vector<GraphicBasicTestInstance::VertexData> m_data;
    const GraphicBasicTestInstance::ParametersGraphic m_parametersGraphic;
    const std::vector<uint64_t> m_drawRepeats;
    const uint32_t m_blockCount;
    const uint32_t m_width;
    const uint32_t m_height;
};

#define NUM_QUERY_STATISTICS 4

class StatisticMultipleQueryTestInstance : public TestInstance
{
public:
    StatisticMultipleQueryTestInstance(Context &context, const uint32_t queryCount);

protected:
    BufferPtr m_queryBuffer;

    virtual void checkExtensions();
};

StatisticMultipleQueryTestInstance::StatisticMultipleQueryTestInstance(Context &context, const uint32_t queryCount)
    : TestInstance(context)
    , m_queryBuffer(Buffer::createAndAlloc(
          context.getDeviceInterface(), context.getDevice(),
          BufferCreateInfo(NUM_QUERY_STATISTICS * sizeof(uint64_t) * queryCount, VK_BUFFER_USAGE_TRANSFER_DST_BIT),
          context.getDefaultAllocator(), vk::MemoryRequirement::HostVisible))
{
    const vk::Allocation &allocation = m_queryBuffer->getBoundMemory();
    void *allocationData             = allocation.getHostPtr();
    deMemset(allocationData, 0xff, NUM_QUERY_STATISTICS * sizeof(uint64_t) * queryCount);
}

void StatisticMultipleQueryTestInstance::checkExtensions()
{
    if (!m_context.getDeviceFeatures().pipelineStatisticsQuery)
        throw tcu::NotSupportedError("Pipeline statistics queries are not supported");
}

class GraphicBasicMultipleQueryTestInstance : public StatisticMultipleQueryTestInstance
{
public:
    struct VertexData
    {
        VertexData(const tcu::Vec4 position_, const tcu::Vec4 color_) : position(position_), color(color_)
        {
        }
        tcu::Vec4 position;
        tcu::Vec4 color;
    };

    struct ParametersGraphic : public GenericParameters
    {
        ParametersGraphic(const VkQueryPipelineStatisticFlags queryStatisticFlags_,
                          const VkQueryResultFlags queryFlags_, const uint32_t queryCount_, const bool vertexOnlyPipe_,
                          const CopyType copyType_, const uint32_t dstOffset_, const StrideType strideType_,
                          const ClearOperation clearOp_ = CLEAR_NOOP)
            : GenericParameters{RESET_TYPE_NORMAL, copyType_, (queryFlags_ & VK_QUERY_RESULT_64_BIT) != 0u,
                                dstOffset_ != 0u, strideType_}
            , queryStatisticFlags(queryStatisticFlags_)
            , vertexOnlyPipe(vertexOnlyPipe_)
            , queryFlags(queryFlags_)
            , queryCount(queryCount_)
            , dstOffset(dstOffset_)
            , clearOp(clearOp_)
        {
        }

        VkQueryPipelineStatisticFlags queryStatisticFlags;
        VkPrimitiveTopology primitiveTopology;
        bool vertexOnlyPipe;
        VkQueryResultFlags queryFlags;
        uint32_t queryCount;
        uint32_t dstOffset;
        ClearOperation clearOp;
    };
    GraphicBasicMultipleQueryTestInstance(vkt::Context &context, const std::vector<VertexData> &data,
                                          const ParametersGraphic &parametersGraphic);
    tcu::TestStatus iterate(void);

protected:
    BufferPtr creatAndFillVertexBuffer(void);
    virtual void createPipeline(void) = 0;
    void creatColorAttachmentAndRenderPass(void);
    virtual tcu::TestStatus executeTest(void)                  = 0;
    virtual tcu::TestStatus checkResult(VkQueryPool queryPool) = 0;
    virtual void draw(VkCommandBuffer cmdBuffer)               = 0;

    const VkFormat m_colorAttachmentFormat;
    de::SharedPtr<Image> m_colorAttachmentImage;
    de::SharedPtr<Image> m_depthImage;
    Move<VkImageView> m_attachmentView;
    Move<VkImageView> m_depthView;
    Move<VkRenderPass> m_renderPass;
    Move<VkFramebuffer> m_framebuffer;
    Move<VkPipeline> m_pipeline;
    Move<VkPipelineLayout> m_pipelineLayout;
    const std::vector<VertexData> &m_data;
    const ParametersGraphic &m_parametersGraphic;
};

GraphicBasicMultipleQueryTestInstance::GraphicBasicMultipleQueryTestInstance(vkt::Context &context,
                                                                             const std::vector<VertexData> &data,
                                                                             const ParametersGraphic &parametersGraphic)
    : StatisticMultipleQueryTestInstance(context,
                                         (parametersGraphic.queryCount + (parametersGraphic.dstOffset != 0u ? 1u : 0u)))
    , m_colorAttachmentFormat(VK_FORMAT_R8G8B8A8_UNORM)
    , m_data(data)
    , m_parametersGraphic(parametersGraphic)
{
}

tcu::TestStatus GraphicBasicMultipleQueryTestInstance::iterate(void)
{
    checkExtensions();
    creatColorAttachmentAndRenderPass();
    createPipeline();
    return executeTest();
}

BufferPtr GraphicBasicMultipleQueryTestInstance::creatAndFillVertexBuffer(void)
{
    const DeviceInterface &vk = m_context.getDeviceInterface();
    const VkDevice device     = m_context.getDevice();

    const VkDeviceSize dataSize = static_cast<VkDeviceSize>(
        deAlignSize(static_cast<size_t>(m_data.size() * sizeof(VertexData)),
                    static_cast<size_t>(m_context.getDeviceProperties().limits.nonCoherentAtomSize)));
    BufferPtr vertexBuffer =
        Buffer::createAndAlloc(vk, device, BufferCreateInfo(dataSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT),
                               m_context.getDefaultAllocator(), MemoryRequirement::HostVisible);

    uint8_t *ptr = reinterpret_cast<uint8_t *>(vertexBuffer->getBoundMemory().getHostPtr());
    deMemcpy(ptr, &m_data[0], static_cast<size_t>(m_data.size() * sizeof(VertexData)));

    flushMappedMemoryRange(vk, device, vertexBuffer->getBoundMemory().getMemory(),
                           vertexBuffer->getBoundMemory().getOffset(), dataSize);
    return vertexBuffer;
}

void GraphicBasicMultipleQueryTestInstance::creatColorAttachmentAndRenderPass(void)
{
    const DeviceInterface &vk = m_context.getDeviceInterface();
    const VkDevice device     = m_context.getDevice();

    {
        DE_ASSERT(m_parametersGraphic.clearOp != CLEAR_SKIP);
        VkExtent3D imageExtent = {
            WIDTH,  // width;
            HEIGHT, // height;
            1u      // depth;
        };

        const ImageCreateInfo colorImageCreateInfo(
            VK_IMAGE_TYPE_2D, m_colorAttachmentFormat, imageExtent, 1, 1, VK_SAMPLE_COUNT_1_BIT,
            VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);

        m_colorAttachmentImage =
            Image::createAndAlloc(vk, device, colorImageCreateInfo, m_context.getDefaultAllocator(),
                                  m_context.getUniversalQueueFamilyIndex());

        const ImageViewCreateInfo attachmentViewInfo(m_colorAttachmentImage->object(), VK_IMAGE_VIEW_TYPE_2D,
                                                     m_colorAttachmentFormat);
        m_attachmentView = createImageView(vk, device, &attachmentViewInfo);

        ImageCreateInfo depthImageCreateInfo(vk::VK_IMAGE_TYPE_2D, VK_FORMAT_D16_UNORM, imageExtent, 1, 1,
                                             vk::VK_SAMPLE_COUNT_1_BIT, vk::VK_IMAGE_TILING_OPTIMAL,
                                             vk::VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);

        m_depthImage = Image::createAndAlloc(vk, device, depthImageCreateInfo, m_context.getDefaultAllocator(),
                                             m_context.getUniversalQueueFamilyIndex());

        // Construct a depth  view from depth image
        const ImageViewCreateInfo depthViewInfo(m_depthImage->object(), vk::VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_D16_UNORM);
        m_depthView = vk::createImageView(vk, device, &depthViewInfo);
    }

    {
        // Renderpass and Framebuffer
        RenderPassCreateInfo renderPassCreateInfo;
        renderPassCreateInfo.addAttachment(
            AttachmentDescription(m_colorAttachmentFormat,                    // format
                                  VK_SAMPLE_COUNT_1_BIT,                      // samples
                                  VK_ATTACHMENT_LOAD_OP_CLEAR,                // loadOp
                                  VK_ATTACHMENT_STORE_OP_STORE,               // storeOp
                                  VK_ATTACHMENT_LOAD_OP_DONT_CARE,            // stencilLoadOp
                                  VK_ATTACHMENT_STORE_OP_STORE,               // stencilLoadOp
                                  VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,   // initialLauout
                                  VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)); // finalLayout

        renderPassCreateInfo.addAttachment(
            AttachmentDescription(VK_FORMAT_D16_UNORM,                                    // format
                                  vk::VK_SAMPLE_COUNT_1_BIT,                              // samples
                                  vk::VK_ATTACHMENT_LOAD_OP_CLEAR,                        // loadOp
                                  vk::VK_ATTACHMENT_STORE_OP_DONT_CARE,                   // storeOp
                                  vk::VK_ATTACHMENT_LOAD_OP_DONT_CARE,                    // stencilLoadOp
                                  vk::VK_ATTACHMENT_STORE_OP_DONT_CARE,                   // stencilLoadOp
                                  vk::VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,   // initialLauout
                                  vk::VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)); // finalLayout

        const VkAttachmentReference colorAttachmentReference = {
            0u,                                      // attachment
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL // layout
        };

        const VkAttachmentReference depthAttachmentReference = {
            1u,                                                  // attachment
            vk::VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL // layout
        };

        const VkSubpassDescription subpass = {
            (VkSubpassDescriptionFlags)0,    //VkSubpassDescriptionFlags flags;
            VK_PIPELINE_BIND_POINT_GRAPHICS, //VkPipelineBindPoint pipelineBindPoint;
            0u,                              //uint32_t inputAttachmentCount;
            nullptr,                         //const VkAttachmentReference* pInputAttachments;
            1u,                              //uint32_t colorAttachmentCount;
            &colorAttachmentReference,       //const VkAttachmentReference* pColorAttachments;
            nullptr,                         //const VkAttachmentReference* pResolveAttachments;
            &depthAttachmentReference,       //const VkAttachmentReference* pDepthStencilAttachment;
            0u,                              //uint32_t preserveAttachmentCount;
            nullptr,                         //const uint32_t* pPreserveAttachments;
        };

        renderPassCreateInfo.addSubpass(subpass);
        m_renderPass = createRenderPass(vk, device, &renderPassCreateInfo);

        std::vector<vk::VkImageView> attachments(2);
        attachments[0] = *m_attachmentView;
        attachments[1] = *m_depthView;

        FramebufferCreateInfo framebufferCreateInfo(*m_renderPass, attachments, WIDTH, HEIGHT, 1);
        m_framebuffer = createFramebuffer(vk, device, &framebufferCreateInfo);
    }
}

class VertexShaderMultipleQueryTestInstance : public GraphicBasicMultipleQueryTestInstance
{
public:
    VertexShaderMultipleQueryTestInstance(vkt::Context &context, const std::vector<VertexData> &data,
                                          const ParametersGraphic &parametersGraphic);

protected:
    virtual void createPipeline(void);
    virtual tcu::TestStatus executeTest(void);
    virtual tcu::TestStatus checkResult(VkQueryPool queryPool);
    void draw(VkCommandBuffer cmdBuffer);
    uint64_t calculateExpectedMin(VkQueryResultFlags flag);
    uint64_t calculateExpectedMax(VkQueryResultFlags flag);
};

VertexShaderMultipleQueryTestInstance::VertexShaderMultipleQueryTestInstance(vkt::Context &context,
                                                                             const std::vector<VertexData> &data,
                                                                             const ParametersGraphic &parametersGraphic)
    : GraphicBasicMultipleQueryTestInstance(context, data, parametersGraphic)
{
}

void VertexShaderMultipleQueryTestInstance::createPipeline(void)
{
    const DeviceInterface &vk = m_context.getDeviceInterface();
    const VkDevice device     = m_context.getDevice();

    // Pipeline
    Unique<VkShaderModule> vs(createShaderModule(vk, device, m_context.getBinaryCollection().get("vertex"), 0));
    Move<VkShaderModule> fs;

    if (!m_parametersGraphic.vertexOnlyPipe)
        fs = createShaderModule(vk, device, m_context.getBinaryCollection().get("fragment"), 0);

    const PipelineCreateInfo::ColorBlendState::Attachment attachmentState;

    const PipelineLayoutCreateInfo pipelineLayoutCreateInfo;
    m_pipelineLayout = createPipelineLayout(vk, device, &pipelineLayoutCreateInfo);

    const VkVertexInputBindingDescription vertexInputBindingDescription = {
        0,                                         // binding;
        static_cast<uint32_t>(sizeof(VertexData)), // stride;
        VK_VERTEX_INPUT_RATE_VERTEX                // inputRate
    };

    const VkVertexInputAttributeDescription vertexInputAttributeDescriptions[] = {
        {0u, 0u, VK_FORMAT_R32G32B32A32_SFLOAT, 0u}, // VertexElementData::position
        {1u, 0u, VK_FORMAT_R32G32B32A32_SFLOAT, static_cast<uint32_t>(sizeof(tcu::Vec4))}, // VertexElementData::color
    };

    const VkPipelineVertexInputStateCreateInfo vf_info = {
        // sType;
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, // pNext;
        NULL,                                                      // flags;
        0u,                                                        // vertexBindingDescriptionCount;
        1u,                                                        // pVertexBindingDescriptions;
        &vertexInputBindingDescription,                            // vertexAttributeDescriptionCount;
        2u,                                                        // pVertexAttributeDescriptions;
        vertexInputAttributeDescriptions};

    PipelineCreateInfo pipelineCreateInfo(*m_pipelineLayout, *m_renderPass, 0, (VkPipelineCreateFlags)0);
    pipelineCreateInfo.addShader(PipelineCreateInfo::PipelineShaderStage(*vs, "main", VK_SHADER_STAGE_VERTEX_BIT));
    if (!m_parametersGraphic.vertexOnlyPipe)
        pipelineCreateInfo.addShader(
            PipelineCreateInfo::PipelineShaderStage(*fs, "main", VK_SHADER_STAGE_FRAGMENT_BIT));
    pipelineCreateInfo.addState(PipelineCreateInfo::DepthStencilState());
    pipelineCreateInfo.addState(PipelineCreateInfo::InputAssemblerState(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST));
    pipelineCreateInfo.addState(PipelineCreateInfo::ColorBlendState(1, &attachmentState));

    const VkViewport viewport = makeViewport(WIDTH, HEIGHT);
    const VkRect2D scissor    = makeRect2D(WIDTH, HEIGHT);
    pipelineCreateInfo.addState(
        PipelineCreateInfo::ViewportState(1u, std::vector<VkViewport>(1, viewport), std::vector<VkRect2D>(1, scissor)));
    pipelineCreateInfo.addState(PipelineCreateInfo::DepthStencilState());
    pipelineCreateInfo.addState(PipelineCreateInfo::RasterizerState());
    pipelineCreateInfo.addState(PipelineCreateInfo::MultiSampleState());
    pipelineCreateInfo.addState(vf_info);
    m_pipeline = createGraphicsPipeline(vk, device, VK_NULL_HANDLE, &pipelineCreateInfo);
}

tcu::TestStatus VertexShaderMultipleQueryTestInstance::executeTest(void)
{
    const DeviceInterface &vk       = m_context.getDeviceInterface();
    const VkDevice device           = m_context.getDevice();
    const VkQueue queue             = m_context.getUniversalQueue();
    const uint32_t queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();

    const CmdPoolCreateInfo cmdPoolCreateInfo(queueFamilyIndex);
    const Move<VkCommandPool> cmdPool = createCommandPool(vk, device, &cmdPoolCreateInfo);
    const Unique<VkQueryPool> queryPool(
        makeQueryPool(vk, device, m_parametersGraphic.queryCount, m_parametersGraphic.queryStatisticFlags));

    const VkDeviceSize vertexBufferOffset = 0u;
    const BufferPtr vertexBufferSp        = creatAndFillVertexBuffer();
    const VkBuffer vertexBuffer           = vertexBufferSp->object();

    const Unique<VkCommandBuffer> cmdBuffer(
        allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

    beginCommandBuffer(vk, *cmdBuffer);
    {
        std::vector<VkClearValue> renderPassClearValues(2);
        deMemset(&renderPassClearValues[0], 0, static_cast<int>(renderPassClearValues.size()) * sizeof(VkClearValue));

        initialTransitionColor2DImage(vk, *cmdBuffer, m_colorAttachmentImage->object(),
                                      VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                                      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
        initialTransitionDepth2DImage(
            vk, *cmdBuffer, m_depthImage->object(), VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT);

        vk.cmdResetQueryPool(*cmdBuffer, *queryPool, 0u, m_parametersGraphic.queryCount);

        beginRenderPass(vk, *cmdBuffer, *m_renderPass, *m_framebuffer, makeRect2D(0, 0, WIDTH, HEIGHT),
                        (uint32_t)renderPassClearValues.size(), &renderPassClearValues[0]);

        vk.cmdBeginQuery(*cmdBuffer, *queryPool, 0u, (VkQueryControlFlags)0u);
        vk.cmdBindVertexBuffers(*cmdBuffer, 0, 1, &vertexBuffer, &vertexBufferOffset);
        vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipeline);

        draw(*cmdBuffer);

        vk.cmdEndQuery(*cmdBuffer, *queryPool, 0u);

        endRenderPass(vk, *cmdBuffer);

        if (m_parametersGraphic.copyType == COPY_TYPE_CMD)
        {
            VkDeviceSize copyStride = NUM_QUERY_STATISTICS * sizeof(uint64_t);
            if (m_parametersGraphic.queryCount == 1u && m_parametersGraphic.strideType == STRIDE_TYPE_ZERO)
                copyStride = 0u;

            vk.cmdCopyQueryPoolResults(*cmdBuffer, *queryPool, 0, m_parametersGraphic.queryCount,
                                       m_queryBuffer->object(), m_parametersGraphic.dstOffset, copyStride,
                                       m_parametersGraphic.queryFlags);

            const VkDeviceSize bufferSize = NUM_QUERY_STATISTICS * sizeof(uint64_t) * m_parametersGraphic.queryCount;
            const VkBufferMemoryBarrier barrier = {
                VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER, //  VkStructureType sType;
                nullptr,                                 //  const void* pNext;
                VK_ACCESS_TRANSFER_WRITE_BIT,            //  VkAccessFlags srcAccessMask;
                VK_ACCESS_HOST_READ_BIT,                 //  VkAccessFlags dstAccessMask;
                VK_QUEUE_FAMILY_IGNORED,                 //  uint32_t srcQueueFamilyIndex;
                VK_QUEUE_FAMILY_IGNORED,                 //  uint32_t destQueueFamilyIndex;
                m_queryBuffer->object(),                 //  VkBuffer buffer;
                0u,                                      //  VkDeviceSize offset;
                bufferSize,                              //  VkDeviceSize size;
            };

            vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                                  (VkDependencyFlags)0, 0, nullptr, 1u, &barrier, 0, nullptr);
        }

        transition2DImage(vk, *cmdBuffer, m_colorAttachmentImage->object(), VK_IMAGE_ASPECT_COLOR_BIT,
                          VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
                          VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
                          VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
    }
    endCommandBuffer(vk, *cmdBuffer);

    // Wait for completion
    submitCommandsAndWait(vk, device, queue, *cmdBuffer);
    return checkResult(*queryPool);
}

uint64_t VertexShaderMultipleQueryTestInstance::calculateExpectedMin(VkQueryResultFlags flag)
{
    uint64_t expectedMin = 0u;
    switch (flag)
    {
    case VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_VERTICES_BIT:
        expectedMin = 15u;
        break;

    case VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT:
        expectedMin = 5u;
        break;

    case VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT:
        expectedMin = 15u;
        break;

    case VK_QUERY_PIPELINE_STATISTIC_FRAGMENT_SHADER_INVOCATIONS_BIT:
        expectedMin = 2016u;
        break;
    default:
        DE_FATAL("Unexpected type of statistics query");
        break;
    }
    return expectedMin;
}

/* This is just to check that driver doesn't return garbage for the partial, no wait case.
 * TODO: adjust the values accordingly, in case some driver returns higher values.
 */
uint64_t VertexShaderMultipleQueryTestInstance::calculateExpectedMax(VkQueryResultFlags flag)
{
    uint64_t expectedMax = 0u;
    switch (flag)
    {
    case VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_VERTICES_BIT:
        expectedMax = 16u;
        break;

    case VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT:
        expectedMax = 5u;
        break;

    case VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT:
        expectedMax = 15u;
        break;

    case VK_QUERY_PIPELINE_STATISTIC_FRAGMENT_SHADER_INVOCATIONS_BIT:
        expectedMax = 2304u;
        break;
    default:
        DE_FATAL("Unexpected type of statistics query");
        break;
    }
    return expectedMax;
}

tcu::TestStatus VertexShaderMultipleQueryTestInstance::checkResult(VkQueryPool queryPool)
{
    const DeviceInterface &vk = m_context.getDeviceInterface();
    const VkDevice device     = m_context.getDevice();
    uint32_t queryCount       = (m_parametersGraphic.queryCount + (m_parametersGraphic.dstOffset ? 1u : 0u));
    uint32_t size             = NUM_QUERY_STATISTICS * queryCount;
    std::vector<uint64_t> results(size, 0ull);

    bool hasPartialFlag = (bool)(m_parametersGraphic.queryFlags & VK_QUERY_RESULT_PARTIAL_BIT);
    bool hasWaitFlag    = (bool)(m_parametersGraphic.queryFlags & VK_QUERY_RESULT_WAIT_BIT);
    // Use the last value of each query to store the availability bit for the vertexOnlyPipe case.
    VkQueryResultFlags queryFlags = m_parametersGraphic.queryFlags;

    if (m_parametersGraphic.copyType == COPY_TYPE_CMD)
    {
        const vk::Allocation &allocation = m_queryBuffer->getBoundMemory();
        const void *allocationData       = allocation.getHostPtr();

        vk::invalidateAlloc(m_context.getDeviceInterface(), m_context.getDevice(), allocation);
        deMemcpy(results.data(), allocationData, size * sizeof(uint64_t));
    }
    else
    {
        VkResult result =
            vk.getQueryPoolResults(device, queryPool, 0u, m_parametersGraphic.queryCount, de::dataSize(results),
                                   de::dataOrNull(results), NUM_QUERY_STATISTICS * sizeof(uint64_t), queryFlags);

        if (!(result == VK_SUCCESS || (!hasWaitFlag && result == VK_NOT_READY)))
            return tcu::TestStatus::fail("Unexpected getQueryPoolResults() returned value: " +
                                         de::toString(getResultStr(result)));
    }

    for (uint32_t queryIdx = 0; queryIdx < queryCount; queryIdx++)
    {
        int32_t queryMask = m_parametersGraphic.queryStatisticFlags;
        uint32_t index    = queryIdx * NUM_QUERY_STATISTICS;
        // Last element of each query is the availability value for the vertexOnlyPipe case.
        bool availableQuery = results[index + (NUM_QUERY_STATISTICS - 1)] != 0u;

        // Check dstOffset values were not overwritten.
        if (m_parametersGraphic.dstOffset != 0u && queryIdx == 0u)
        {
            const uint64_t refVal = 0xfffffffffffffffful;
            for (; index < NUM_QUERY_STATISTICS; index++)
            {
                if (results[index] != refVal)
                    return tcu::TestStatus::fail("dstOffset values were overwritten");
            }
            continue;
        }

        if (hasWaitFlag && !hasPartialFlag && !availableQuery)
            return tcu::TestStatus::fail("Results should be available");

        while (queryMask)
        {
            int32_t statisticBit = deInt32BitScan(&queryMask);
            uint64_t expectedMin = calculateExpectedMin((1u << statisticBit));
            uint64_t expectedMax = calculateExpectedMax((1u << statisticBit));

            if (availableQuery && (results[index] < expectedMin))
                return tcu::TestStatus::fail("QueryPoolResults incorrect: wrong value (" +
                                             de::toString(results[index]) + ") is lower than expected (" +
                                             de::toString(expectedMin) + ")");

            /* From the spec:
             *
             *    If VK_QUERY_RESULT_PARTIAL_BIT is set, VK_QUERY_RESULT_WAIT_BIT is not set,
             *    and the query's status is unavailable, an intermediate result value between zero
             *    and the final result value is written to pData for that query.
             */
            if (hasPartialFlag && !hasWaitFlag && !availableQuery && results[index] > expectedMax)
                return tcu::TestStatus::fail("QueryPoolResults incorrect: wrong partial value (" +
                                             de::toString(results[index]) + ") is greater than expected (" +
                                             de::toString(expectedMax) + ")");

            index++;
        }
    }

    return tcu::TestStatus::pass("Pass");
}

void VertexShaderMultipleQueryTestInstance::draw(VkCommandBuffer cmdBuffer)
{
    const DeviceInterface &vk = m_context.getDeviceInterface();
    vk.cmdDraw(cmdBuffer, 16u, 1u, 0u, 0u);
}

template <class Instance>
class QueryPoolGraphicMultipleQueryStatisticsTest : public TestCase
{
public:
    QueryPoolGraphicMultipleQueryStatisticsTest(
        tcu::TestContext &context, const std::string &name,
        const GraphicBasicMultipleQueryTestInstance::ParametersGraphic parametersGraphic)
        : TestCase(context, name.c_str())
        , m_parametersGraphic(parametersGraphic)
    {
        using VertexData          = GraphicBasicMultipleQueryTestInstance::VertexData;
        const float quarterWidth  = (2.0f / static_cast<float>(WIDTH)) * 0.25f;
        const float quarterHeight = (2.0f / static_cast<float>(HEIGHT)) * 0.25f;

        // These coordinates will be used with different topologies, so we try to avoid drawing points on the edges.
        const float left   = -1.0f + quarterWidth;
        const float right  = 1.0f - quarterWidth;
        const float center = (left + right) / 2.0f;
        const float top    = -1.0f + quarterHeight;
        const float bottom = 1.0f - quarterHeight;
        const float middle = (top + bottom) / 2.0f;

        const auto red   = tcu::RGBA::red().toVec();
        const auto green = tcu::RGBA::green().toVec();
        const auto blue  = tcu::RGBA::blue().toVec();
        const auto gray  = tcu::RGBA::gray().toVec();

        m_data.push_back(VertexData(tcu::Vec4(left, top, 1.0f, 1.0f), red));
        m_data.push_back(VertexData(tcu::Vec4(left, middle, 1.0f, 1.0f), red));
        m_data.push_back(VertexData(tcu::Vec4(center, top, 1.0f, 1.0f), red));
        m_data.push_back(VertexData(tcu::Vec4(center, middle, 1.0f, 1.0f), red));

        m_data.push_back(VertexData(tcu::Vec4(left, middle, 1.0f, 1.0f), green));
        m_data.push_back(VertexData(tcu::Vec4(left, bottom, 1.0f, 1.0f), green));
        m_data.push_back(VertexData(tcu::Vec4(center, middle, 1.0f, 1.0f), green));
        m_data.push_back(VertexData(tcu::Vec4(center, bottom, 1.0f, 1.0f), green));

        m_data.push_back(VertexData(tcu::Vec4(center, top, 1.0f, 1.0f), blue));
        m_data.push_back(VertexData(tcu::Vec4(center, middle, 1.0f, 1.0f), blue));
        m_data.push_back(VertexData(tcu::Vec4(right, top, 1.0f, 1.0f), blue));
        m_data.push_back(VertexData(tcu::Vec4(right, middle, 1.0f, 1.0f), blue));

        m_data.push_back(VertexData(tcu::Vec4(center, middle, 1.0f, 1.0f), gray));
        m_data.push_back(VertexData(tcu::Vec4(center, bottom, 1.0f, 1.0f), gray));
        m_data.push_back(VertexData(tcu::Vec4(right, middle, 1.0f, 1.0f), gray));
        m_data.push_back(VertexData(tcu::Vec4(right, bottom, 1.0f, 1.0f), gray));
    }

    vkt::TestInstance *createInstance(vkt::Context &context) const
    {
        return new Instance(context, m_data, m_parametersGraphic);
    }

    void initPrograms(SourceCollections &sourceCollections) const
    {
        { // Vertex Shader
            std::ostringstream source;
            source << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
                   << "layout(location = 0) in highp vec4 in_position;\n"
                   << "layout(location = 1) in vec4 in_color;\n"
                   << "layout(location = 0) out vec4 out_color;\n"
                   << "void main (void)\n"
                   << "{\n"
                   << "    gl_PointSize = 1.0;\n"
                   << "    gl_Position = in_position;\n"
                   << "    out_color = in_color;\n"
                   << "}\n";
            sourceCollections.glslSources.add("vertex") << glu::VertexSource(source.str());
        }

        if (!m_parametersGraphic.vertexOnlyPipe)
        { // Fragment Shader
            std::ostringstream source;
            source << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
                   << "layout(location = 0) in vec4 in_color;\n"
                   << "layout(location = 0) out vec4 out_color;\n"
                   << "void main()\n"
                   << "{\n"
                   << "    out_color = in_color;\n"
                   << "}\n";
            sourceCollections.glslSources.add("fragment") << glu::FragmentSource(source.str());
        }
    }

private:
    std::vector<GraphicBasicMultipleQueryTestInstance::VertexData> m_data;
    const GraphicBasicMultipleQueryTestInstance::ParametersGraphic m_parametersGraphic;
};

struct MultipleGeomStatsParams
{
    const bool copy;         // true == copy, false == get
    const bool availability; // availability bit or not.
    const bool inheritance;  // secondary with inheritance.
};

class MultipleGeomStatsTestInstance : public vkt::TestInstance
{
public:
    MultipleGeomStatsTestInstance(Context &context, const MultipleGeomStatsParams &params)
        : vkt::TestInstance(context)
        , m_params(params)
    {
    }
    virtual ~MultipleGeomStatsTestInstance(void)
    {
    }

    tcu::TestStatus iterate(void) override;

protected:
    const MultipleGeomStatsParams m_params;
};

class MultipleGeomStatsTestCase : public vkt::TestCase
{
public:
    MultipleGeomStatsTestCase(tcu::TestContext &testCtx, const std::string &name, const MultipleGeomStatsParams &params)
        : vkt::TestCase(testCtx, name)
        , m_params(params)
    {
    }
    virtual ~MultipleGeomStatsTestCase(void)
    {
    }

    void initPrograms(vk::SourceCollections &programCollection) const override;
    TestInstance *createInstance(Context &context) const override;
    void checkSupport(Context &context) const override;

protected:
    const MultipleGeomStatsParams m_params;
};

void MultipleGeomStatsTestCase::initPrograms(vk::SourceCollections &programCollection) const
{
    std::ostringstream vert;
    vert << "#version 460\n"
         << "layout (location=0) in vec4 inPos;\n"
         << "out gl_PerVertex\n"
         << "{\n"
         << "    vec4 gl_Position;\n"
         << "};\n"
         << "void main (void) {\n"
         << "    gl_Position = inPos;\n"
         << "}\n";
    programCollection.glslSources.add("vert") << glu::VertexSource(vert.str());

    std::ostringstream geom;
    geom << "#version 450\n"
         << "layout (triangles) in;\n"
         << "layout (triangle_strip, max_vertices=" << kTriangleVertices << ") out;\n"
         << "in gl_PerVertex\n"
         << "{\n"
         << "    vec4 gl_Position;\n"
         << "} gl_in[" << kTriangleVertices << "];\n"
         << "out gl_PerVertex\n"
         << "{\n"
         << "    vec4 gl_Position;\n"
         << "};\n"
         << "void main() {\n";
    for (uint32_t i = 0; i < kTriangleVertices; ++i)
    {
        geom << "    gl_Position = gl_in[" << i << "].gl_Position;\n"
             << "    EmitVertex();\n";
    }
    geom << "}\n";
    programCollection.glslSources.add("geom") << glu::GeometrySource(geom.str());

    std::ostringstream frag;
    frag << "#version 460\n"
         << "layout (location=0) out vec4 outColor;\n"
         << "void main (void) {\n"
         << "    outColor = vec4(0.0, 0.0, 1.0, 1.0);\n"
         << "}\n";
    programCollection.glslSources.add("frag") << glu::FragmentSource(frag.str());
}

TestInstance *MultipleGeomStatsTestCase::createInstance(Context &context) const
{
    return new MultipleGeomStatsTestInstance(context, m_params);
}

void MultipleGeomStatsTestCase::checkSupport(Context &context) const
{
    context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_GEOMETRY_SHADER);
    context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_PIPELINE_STATISTICS_QUERY);

    if (m_params.inheritance)
        context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_INHERITED_QUERIES);
}

tcu::TestStatus MultipleGeomStatsTestInstance::iterate(void)
{
    const auto &ctx = m_context.getContextCommonData();
    const tcu::IVec3 fbExtent(16, 16, 1);
    const auto vkExtent  = makeExtent3D(fbExtent);
    const auto fbFormat  = VK_FORMAT_R8G8B8A8_UNORM;
    const auto tcuFormat = mapVkFormat(fbFormat);
    const auto fbUsage   = (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    const tcu::Vec4 clearColor(0.0f, 0.0f, 0.0f, 1.0f);
    const tcu::Vec4 geomColor(0.0f, 0.0f, 1.0f, 1.0f); // Must match frag shader.
    const tcu::Vec4 threshold(0.0f, 0.0f, 0.0f, 0.0f); // When using 0 and 1 only, we expect exact results.
    const auto bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

    // Color buffer with verification buffer.
    ImageWithBuffer colorBuffer(ctx.vkd, ctx.device, ctx.allocator, vkExtent, fbFormat, fbUsage, VK_IMAGE_TYPE_2D);

    // Vertices.
    std::vector<tcu::Vec4> vertices;
    const auto imageWidth  = static_cast<float>(fbExtent.x());
    const auto imageHeight = static_cast<float>(fbExtent.y());
    const auto pixelWidth  = 2.0f / imageWidth;
    const auto pixelHeight = 2.0f / imageHeight;
    const auto horMargin   = pixelWidth / 4.0f;
    const auto vertMargin  = pixelHeight / 4.0f;

    vertices.reserve(vkExtent.width * vkExtent.height * kTriangleVertices);

    for (uint32_t y = 0u; y < vkExtent.height; ++y)
        for (uint32_t x = 0u; x < vkExtent.width; ++x)
        {
            // Pixel center in normalized coordinates.
            const auto pixX = (static_cast<float>(x) + 0.5f) / imageWidth * 2.0f - 1.0f;
            const auto pixY = (static_cast<float>(y) + 0.5f) / imageHeight * 2.0f - 1.0f;

            // Triangle around pixel center.
            vertices.push_back(tcu::Vec4(pixX - horMargin, pixY + vertMargin, 0.0f, 1.0f));
            vertices.push_back(tcu::Vec4(pixX + horMargin, pixY + vertMargin, 0.0f, 1.0f));
            vertices.push_back(tcu::Vec4(pixX, pixY - vertMargin, 0.0f, 1.0f));
        }

    // Vertex buffer
    const auto vbSize = static_cast<VkDeviceSize>(de::dataSize(vertices));
    const auto vbInfo = makeBufferCreateInfo(vbSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    BufferWithMemory vertexBuffer(ctx.vkd, ctx.device, ctx.allocator, vbInfo, MemoryRequirement::HostVisible);
    const auto vbAlloc  = vertexBuffer.getAllocation();
    void *vbData        = vbAlloc.getHostPtr();
    const auto vbOffset = static_cast<VkDeviceSize>(0);

    deMemcpy(vbData, de::dataOrNull(vertices), de::dataSize(vertices));
    flushAlloc(ctx.vkd, ctx.device, vbAlloc); // strictly speaking, not needed.

    const auto pipelineLayout = makePipelineLayout(ctx.vkd, ctx.device);
    const auto renderPass     = makeRenderPass(ctx.vkd, ctx.device, fbFormat);
    const auto framebuffer =
        makeFramebuffer(ctx.vkd, ctx.device, *renderPass, colorBuffer.getImageView(), vkExtent.width, vkExtent.height);

    // Modules.
    const auto &binaries  = m_context.getBinaryCollection();
    const auto vertModule = createShaderModule(ctx.vkd, ctx.device, binaries.get("vert"));
    const auto geomModule = createShaderModule(ctx.vkd, ctx.device, binaries.get("geom"));
    const auto fragModule = createShaderModule(ctx.vkd, ctx.device, binaries.get("frag"));

    const std::vector<VkViewport> viewports(1u, makeViewport(vkExtent));
    const std::vector<VkRect2D> scissors(1u, makeRect2D(vkExtent));

    const auto pipeline = makeGraphicsPipeline(
        ctx.vkd, ctx.device, *pipelineLayout, *vertModule, VK_NULL_HANDLE, VK_NULL_HANDLE, *geomModule, *fragModule,
        *renderPass, viewports,
        scissors); // The default values works for the current setup, including the vertex input data format.

    CommandPoolWithBuffer cmd(ctx.vkd, ctx.device, ctx.qfIndex);
    const auto cmdBuffer = *cmd.cmdBuffer;

    const VkQueryPipelineStatisticFlags stats = (VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_INVOCATIONS_BIT |
                                                 VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_PRIMITIVES_BIT);

    const VkQueryPoolCreateInfo queryPoolCreateInfo = {
        VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO, // VkStructureType sType;
        nullptr,                                  // const void* pNext;
        0u,                                       // VkQueryPoolCreateFlags flags;
        VK_QUERY_TYPE_PIPELINE_STATISTICS,        // VkQueryType queryType;
        1u,                                       // uint32_t queryCount;
        stats,                                    // VkQueryPipelineStatisticFlags pipelineStatistics;
    };
    const auto queryPool         = createQueryPool(ctx.vkd, ctx.device, &queryPoolCreateInfo);
    const auto perQueryItemCount = (2u + (m_params.availability ? 1u : 0u));
    const VkQueryResultFlags resultFlags =
        (VK_QUERY_RESULT_WAIT_BIT | (m_params.availability ? VK_QUERY_RESULT_WITH_AVAILABILITY_BIT : 0));
    std::vector<uint32_t> queryResults(perQueryItemCount, 0);

    std::unique_ptr<BufferWithMemory> resultsBuffer;
    if (m_params.copy)
    {
        const auto resultsBufferCreateInfo = makeBufferCreateInfo(static_cast<VkDeviceSize>(de::dataSize(queryResults)),
                                                                  VK_BUFFER_USAGE_TRANSFER_DST_BIT);
        resultsBuffer.reset(new BufferWithMemory(ctx.vkd, ctx.device, ctx.allocator, resultsBufferCreateInfo,
                                                 MemoryRequirement::HostVisible));
        deMemset(resultsBuffer->getAllocation().getHostPtr(), 0xFF, de::dataSize(queryResults));
        flushAlloc(ctx.vkd, ctx.device, resultsBuffer->getAllocation());
    }

    Move<VkCommandBuffer> secCmdBuffer;
    if (m_params.inheritance)
    {
        secCmdBuffer = allocateCommandBuffer(ctx.vkd, ctx.device, *cmd.cmdPool, VK_COMMAND_BUFFER_LEVEL_SECONDARY);
        const auto usageFlags =
            (VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT | VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT);

        const VkCommandBufferInheritanceInfo inheritanceInfo = {
            VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO, // VkStructureType sType;
            nullptr,                                           // const void* pNext;
            *renderPass,                                       // VkRenderPass renderPass;
            0u,                                                // uint32_t subpass;
            *framebuffer,                                      // VkFramebuffer framebuffer;
            VK_FALSE,                                          // VkBool32 occlusionQueryEnable;
            0u,                                                // VkQueryControlFlags queryFlags;
            stats,                                             // VkQueryPipelineStatisticFlags pipelineStatistics;
        };
        const VkCommandBufferBeginInfo beginInfo = {
            VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, // VkStructureType sType;
            nullptr,                                     // const void* pNext;
            usageFlags,                                  // VkCommandBufferUsageFlags flags;
            &inheritanceInfo,                            // const VkCommandBufferInheritanceInfo* pInheritanceInfo;
        };
        ctx.vkd.beginCommandBuffer(*secCmdBuffer, &beginInfo);
    }

    // Render pass contents command buffer.
    const auto rpCmdBuffer = (m_params.inheritance ? *secCmdBuffer : *cmd.cmdBuffer);
    const auto subpassContents =
        (m_params.inheritance ? VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS : VK_SUBPASS_CONTENTS_INLINE);

    beginCommandBuffer(ctx.vkd, cmdBuffer);
    ctx.vkd.cmdResetQueryPool(cmdBuffer, *queryPool, 0u, 1u);
    ctx.vkd.cmdBeginQuery(cmdBuffer, *queryPool, 0u, 0u);
    beginRenderPass(ctx.vkd, cmdBuffer, *renderPass, *framebuffer, scissors.at(0u), clearColor, subpassContents);
    {
        ctx.vkd.cmdBindVertexBuffers(rpCmdBuffer, 0u, 1u, &vertexBuffer.get(), &vbOffset);
        ctx.vkd.cmdBindPipeline(rpCmdBuffer, bindPoint, *pipeline);
        ctx.vkd.cmdDraw(rpCmdBuffer, de::sizeU32(vertices), 1u, 0u, 0u);
    }
    if (m_params.inheritance)
    {
        endCommandBuffer(ctx.vkd, *secCmdBuffer);
        ctx.vkd.cmdExecuteCommands(*cmd.cmdBuffer, 1u, &secCmdBuffer.get());
    }
    endRenderPass(ctx.vkd, cmdBuffer);
    ctx.vkd.cmdEndQuery(cmdBuffer, *queryPool, 0u);
    copyImageToBuffer(ctx.vkd, cmdBuffer, colorBuffer.getImage(), colorBuffer.getBuffer(), fbExtent.swizzle(0, 1),
                      VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 1u,
                      VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_ASPECT_COLOR_BIT,
                      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
    if (m_params.copy)
    {
        ctx.vkd.cmdCopyQueryPoolResults(cmdBuffer, *queryPool, 0u, 1u, resultsBuffer->get(), 0ull, 0ull, resultFlags);
        const auto queryResultsBarrier = makeMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
        cmdPipelineMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                                 &queryResultsBarrier);
    }
    endCommandBuffer(ctx.vkd, cmdBuffer);
    submitCommandsAndWait(ctx.vkd, ctx.device, ctx.queue, cmdBuffer);

    // Verify query results.
    if (m_params.copy)
    {
        invalidateAlloc(ctx.vkd, ctx.device, resultsBuffer->getAllocation());
        deMemcpy(de::dataOrNull(queryResults), resultsBuffer->getAllocation().getHostPtr(), de::dataSize(queryResults));
    }
    else
    {
        ctx.vkd.getQueryPoolResults(ctx.device, *queryPool, 0u, 1u, de::dataSize(queryResults),
                                    de::dataOrNull(queryResults), 0ull, resultFlags);
    }

    for (uint32_t queryItem = 0u; queryItem < perQueryItemCount; ++queryItem)
    {
        const bool isAvailabilityBit = (m_params.availability && queryItem == perQueryItemCount - 1u);
        const auto minValue          = (isAvailabilityBit ? 1u : de::sizeU32(vertices) / kTriangleVertices);
        const auto maxValue          = std::numeric_limits<uint32_t>::max();
        const auto &value            = queryResults.at(queryItem);

        if (value < minValue || value > maxValue)
        {
            std::ostringstream msg;
            msg << "Unexpected value for query item " << queryItem << ": " << value << " out of expected range ["
                << minValue << ", " << maxValue << "]";
            TCU_FAIL(msg.str());
        }
    }

    // Verify color output.
    invalidateAlloc(ctx.vkd, ctx.device, colorBuffer.getBufferAllocation());
    tcu::PixelBufferAccess resultAccess(tcuFormat, fbExtent, colorBuffer.getBufferAllocation().getHostPtr());

    auto &log = m_context.getTestContext().getLog();
    if (!tcu::floatThresholdCompare(log, "Result", "", geomColor, resultAccess, threshold, tcu::COMPARE_LOG_ON_ERROR))
        return tcu::TestStatus::fail("Unexpected color in result buffer; check log for details");

    return tcu::TestStatus::pass("Pass");
}

class TessellationGeometryShaderTestInstance : public GraphicBasicTestInstance
{
public:
    TessellationGeometryShaderTestInstance(vkt::Context &context, const std::vector<VertexData> &data,
                                           const ParametersGraphic &parametersGraphic,
                                           const std::vector<uint64_t> &drawRepeats);

protected:
    virtual void checkExtensions(bool hostQueryResetEnabled);
    virtual void createPipeline(void);
    virtual tcu::TestStatus executeTest(void);
    virtual tcu::TestStatus checkResult(VkQueryPool queryPool);
    void draw(VkCommandBuffer cmdBuffer);
};

TessellationGeometryShaderTestInstance::TessellationGeometryShaderTestInstance(
    vkt::Context &context, const std::vector<VertexData> &data, const ParametersGraphic &parametersGraphic,
    const std::vector<uint64_t> &drawRepeats)
    : GraphicBasicTestInstance(context, data, parametersGraphic, drawRepeats)
{
}

void TessellationGeometryShaderTestInstance::checkExtensions(bool hostQueryResetEnabled)
{
    StatisticQueryTestInstance::checkExtensions(hostQueryResetEnabled);
    if (!m_context.getDeviceFeatures().tessellationShader)
        throw tcu::NotSupportedError("Tessellation shader are not supported");
    if (!m_context.getDeviceFeatures().geometryShader)
        throw tcu::NotSupportedError("Geometry shader are not supported");
}

void TessellationGeometryShaderTestInstance::createPipeline(void)
{
    const DeviceInterface &vk = m_context.getDeviceInterface();
    const VkDevice device     = m_context.getDevice();

    // Pipeline
    Unique<VkShaderModule> vs(
        createShaderModule(vk, device, m_context.getBinaryCollection().get("vertex"), (VkShaderModuleCreateFlags)0));
    Unique<VkShaderModule> tc(createShaderModule(
        vk, device, m_context.getBinaryCollection().get("tessellation_control"), (VkShaderModuleCreateFlags)0));
    Unique<VkShaderModule> te(createShaderModule(
        vk, device, m_context.getBinaryCollection().get("tessellation_evaluation"), (VkShaderModuleCreateFlags)0));
    Unique<VkShaderModule> gs(
        createShaderModule(vk, device, m_context.getBinaryCollection().get("geometry"), (VkShaderModuleCreateFlags)0));
    Unique<VkShaderModule> fs(
        createShaderModule(vk, device, m_context.getBinaryCollection().get("fragment"), (VkShaderModuleCreateFlags)0));

    const PipelineCreateInfo::ColorBlendState::Attachment attachmentState;

    std::vector<VkPushConstantRange> pcRanges;

    if (m_parametersGraphic.noColorAttachments)
        pcRanges.push_back(makePushConstantRange(VK_SHADER_STAGE_FRAGMENT_BIT, kFloatSize, kFloatSize));

    const PipelineLayoutCreateInfo pipelineLayoutCreateInfo(std::vector<VkDescriptorSetLayout>(), de::sizeU32(pcRanges),
                                                            de::dataOrNull(pcRanges));

    m_pipelineLayout = createPipelineLayout(vk, device, &pipelineLayoutCreateInfo);

    const VkVertexInputBindingDescription vertexInputBindingDescription = {
        0u,                                        // binding;
        static_cast<uint32_t>(sizeof(VertexData)), // stride;
        VK_VERTEX_INPUT_RATE_VERTEX                // inputRate
    };

    const VkVertexInputAttributeDescription vertexInputAttributeDescriptions[] = {
        {0u, 0u, VK_FORMAT_R32G32B32A32_SFLOAT, 0u}, // VertexElementData::position
        {1u, 0u, VK_FORMAT_R32G32B32A32_SFLOAT, static_cast<uint32_t>(sizeof(tcu::Vec4))}, // VertexElementData::color
    };

    const VkPipelineVertexInputStateCreateInfo vf_info = {
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, // sType;
        NULL,                                                      // pNext;
        0u,                                                        // flags;
        1u,                                                        // vertexBindingDescriptionCount;
        &vertexInputBindingDescription,                            // pVertexBindingDescriptions;
        2u,                                                        // vertexAttributeDescriptionCount;
        vertexInputAttributeDescriptions                           // pVertexAttributeDescriptions;
    };

    PipelineCreateInfo pipelineCreateInfo(*m_pipelineLayout, *m_renderPass, 0, (VkPipelineCreateFlags)0);
    pipelineCreateInfo.addShader(PipelineCreateInfo::PipelineShaderStage(*vs, "main", VK_SHADER_STAGE_VERTEX_BIT));
    pipelineCreateInfo.addShader(
        PipelineCreateInfo::PipelineShaderStage(*tc, "main", VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT));
    pipelineCreateInfo.addShader(
        PipelineCreateInfo::PipelineShaderStage(*te, "main", VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT));
    pipelineCreateInfo.addShader(PipelineCreateInfo::PipelineShaderStage(*gs, "main", VK_SHADER_STAGE_GEOMETRY_BIT));
    pipelineCreateInfo.addShader(PipelineCreateInfo::PipelineShaderStage(*fs, "main", VK_SHADER_STAGE_FRAGMENT_BIT));

    pipelineCreateInfo.addState(PipelineCreateInfo::TessellationState(
        m_parametersGraphic.tessPatchSize ? m_parametersGraphic.tessPatchSize : 4u));

    pipelineCreateInfo.addState(PipelineCreateInfo::InputAssemblerState(VK_PRIMITIVE_TOPOLOGY_PATCH_LIST));
    pipelineCreateInfo.addState(PipelineCreateInfo::ColorBlendState(1, &attachmentState));

    const VkViewport viewport = makeViewport(m_width, m_height);
    const VkRect2D scissor    = makeRect2D(m_width, m_height);

    pipelineCreateInfo.addState(
        PipelineCreateInfo::ViewportState(1, std::vector<VkViewport>(1, viewport), std::vector<VkRect2D>(1, scissor)));

    const VkBool32 depthTestAndWrites = makeVkBool(m_parametersGraphic.noColorAttachments);
    pipelineCreateInfo.addState(PipelineCreateInfo::DepthStencilState(depthTestAndWrites, depthTestAndWrites));

    pipelineCreateInfo.addState(PipelineCreateInfo::RasterizerState());
    pipelineCreateInfo.addState(PipelineCreateInfo::MultiSampleState());
    pipelineCreateInfo.addState(vf_info);
    m_pipeline = createGraphicsPipeline(vk, device, VK_NULL_HANDLE, &pipelineCreateInfo);
}

tcu::TestStatus TessellationGeometryShaderTestInstance::executeTest(void)
{
    const DeviceInterface &vk       = m_context.getDeviceInterface();
    const VkDevice device           = m_context.getDevice();
    const VkQueue queue             = m_context.getUniversalQueue();
    const uint32_t queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();

    const CmdPoolCreateInfo cmdPoolCreateInfo(queueFamilyIndex);
    const Move<VkCommandPool> cmdPool = createCommandPool(vk, device, &cmdPoolCreateInfo);
    const uint32_t queryCount         = static_cast<uint32_t>(m_drawRepeats.size());
    const Unique<VkQueryPool> queryPool(makeQueryPool(vk, device, queryCount, m_parametersGraphic.queryStatisticFlags));

    const VkDeviceSize vertexBufferOffset = 0u;
    const BufferPtr vertexBufferSp        = creatAndFillVertexBuffer();
    const VkBuffer vertexBuffer           = vertexBufferSp->object();

    const Unique<VkCommandBuffer> cmdBuffer(
        allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

    beginCommandBuffer(vk, *cmdBuffer);
    {
        std::vector<VkClearValue> renderPassClearValues(2);
        deMemset(&renderPassClearValues[0], 0, static_cast<int>(renderPassClearValues.size()) * sizeof(VkClearValue));

        if (!m_parametersGraphic.noColorAttachments)
            initialTransitionColor2DImage(
                vk, *cmdBuffer, m_colorAttachmentImage->object(), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

        initialTransitionDepth2DImage(
            vk, *cmdBuffer, m_depthImage->object(), VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT);

        if (m_parametersGraphic.resetType != RESET_TYPE_HOST)
            vk.cmdResetQueryPool(*cmdBuffer, *queryPool, 0u, queryCount);

        beginRenderPass(vk, *cmdBuffer, *m_renderPass, *m_framebuffer, makeRect2D(0, 0, m_width, m_height),
                        (uint32_t)renderPassClearValues.size(), &renderPassClearValues[0]);

        for (uint32_t i = 0; i < queryCount; ++i)
        {
            vk.cmdBeginQuery(*cmdBuffer, *queryPool, i, (VkQueryControlFlags)0u);
            vk.cmdBindVertexBuffers(*cmdBuffer, 0, 1, &vertexBuffer, &vertexBufferOffset);
            vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipeline);

            for (uint64_t j = 0; j < m_drawRepeats[i]; ++j)
                draw(*cmdBuffer);

            vk.cmdEndQuery(*cmdBuffer, *queryPool, i);
        }

        endRenderPass(vk, *cmdBuffer);

        if (m_parametersGraphic.resetType == RESET_TYPE_BEFORE_COPY ||
            m_parametersGraphic.resetType == RESET_TYPE_AFTER_COPY || m_parametersGraphic.copyType == COPY_TYPE_CMD)
        {
            VkDeviceSize stride          = m_parametersGraphic.querySizeFlags() ? sizeof(uint64_t) : sizeof(uint32_t);
            vk::VkQueryResultFlags flags = m_parametersGraphic.querySizeFlags() | VK_QUERY_RESULT_WAIT_BIT;

            if (m_parametersGraphic.resetType == RESET_TYPE_HOST)
            {
                flags |= VK_QUERY_RESULT_WITH_AVAILABILITY_BIT;
                stride *= 2u;
            }

            if (m_parametersGraphic.resetType == RESET_TYPE_BEFORE_COPY)
            {
                vk.cmdResetQueryPool(*cmdBuffer, *queryPool, 0u, queryCount);
                flags  = VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT;
                stride = sizeof(ValueAndAvailability);
            }

            VkDeviceSize dstOffsetQuery = (m_parametersGraphic.dstOffset) ? stride : 0;
            VkDeviceSize copyStride     = stride;
            if (m_parametersGraphic.strideType == STRIDE_TYPE_ZERO)
                copyStride = 0u;

            vk.cmdCopyQueryPoolResults(*cmdBuffer, *queryPool, 0, queryCount, m_resetBuffer->object(), dstOffsetQuery,
                                       copyStride, flags);

            if (m_parametersGraphic.resetType == RESET_TYPE_AFTER_COPY)
                vk.cmdResetQueryPool(*cmdBuffer, *queryPool, 0u, queryCount);

            const VkBufferMemoryBarrier barrier = {
                VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER, //  VkStructureType sType;
                nullptr,                                 //  const void* pNext;
                VK_ACCESS_TRANSFER_WRITE_BIT,            //  VkAccessFlags srcAccessMask;
                VK_ACCESS_HOST_READ_BIT,                 //  VkAccessFlags dstAccessMask;
                VK_QUEUE_FAMILY_IGNORED,                 //  uint32_t srcQueueFamilyIndex;
                VK_QUEUE_FAMILY_IGNORED,                 //  uint32_t destQueueFamilyIndex;
                m_resetBuffer->object(),                 //  VkBuffer buffer;
                0u,                                      //  VkDeviceSize offset;
                queryCount * stride + dstOffsetQuery,    //  VkDeviceSize size;
            };
            vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                                  (VkDependencyFlags)0, 0, nullptr, 1u, &barrier, 0, nullptr);
        }

        if (!m_parametersGraphic.noColorAttachments)
            transition2DImage(vk, *cmdBuffer, m_colorAttachmentImage->object(), VK_IMAGE_ASPECT_COLOR_BIT,
                              VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
                              VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
                              VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
    }
    endCommandBuffer(vk, *cmdBuffer);

    if (m_parametersGraphic.resetType == RESET_TYPE_HOST)
        vk.resetQueryPool(device, *queryPool, 0u, queryCount);

    // Wait for completion
    submitCommandsAndWait(vk, device, queue, *cmdBuffer);
    return checkResult(*queryPool);
}

tcu::TestStatus TessellationGeometryShaderTestInstance::checkResult(VkQueryPool queryPool)
{
    const DeviceInterface &vk = m_context.getDeviceInterface();
    const VkDevice device     = m_context.getDevice();
    uint64_t expectedMin      = 0u;
    std::string errorMsg;

    switch (m_parametersGraphic.queryStatisticFlags)
    {
    case VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT:
        expectedMin = m_parametersGraphic.numTessPrimitives;
        break;
    case VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_CONTROL_SHADER_PATCHES_BIT:
        expectedMin = 4u;
        break;
    case VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_EVALUATION_SHADER_INVOCATIONS_BIT:
        expectedMin = 100u;
        break;
    case VK_QUERY_PIPELINE_STATISTIC_CLIPPING_INVOCATIONS_BIT:
        expectedMin = 64u;
        break;
    case VK_QUERY_PIPELINE_STATISTIC_CLIPPING_PRIMITIVES_BIT:
        expectedMin = 64u;
        break;
    default:
        DE_FATAL("Unexpected type of statistics query");
        break;
    }

    const uint32_t queryCount = static_cast<uint32_t>(m_drawRepeats.size());

    if (m_parametersGraphic.resetType == RESET_TYPE_NORMAL || m_parametersGraphic.resetType == RESET_TYPE_AFTER_COPY)
    {
        ResultsVector results(queryCount, 0u);

        if (m_parametersGraphic.copyType == COPY_TYPE_CMD)
        {
            const vk::Allocation &allocation = m_resetBuffer->getBoundMemory();
            cmdCopyQueryPoolResultsVector(results, vk, device, allocation, queryCount,
                                          (VK_QUERY_RESULT_WAIT_BIT | m_parametersGraphic.querySizeFlags()),
                                          m_parametersGraphic.dstOffset);
        }
        else
        {
            VK_CHECK(GetQueryPoolResultsVector(results, vk, device, queryPool, 0u, queryCount,
                                               (VK_QUERY_RESULT_WAIT_BIT | m_parametersGraphic.querySizeFlags())));
        }

        if (results[0] < expectedMin)
        {
            std::ostringstream msg;
            msg << "QueryPoolResults incorrect: expected at least " << expectedMin << " but got " << results[0];
            errorMsg = msg.str();
        }
        else if (queryCount > 1)
        {
            double pearson = calculatePearsonCorrelation(m_drawRepeats, results);
            if (fabs(pearson) < 0.8)
            {
                std::ostringstream msg;
                msg << "QueryPoolResults are nonlinear: Pearson " << pearson << " for";
                for (const auto &x : results)
                    msg << " " << x;
                errorMsg = msg.str();
            }
        }
    }
    else if (m_parametersGraphic.resetType == RESET_TYPE_HOST)
    {
        ResultsVectorWithAvailability results(queryCount, pair<uint64_t, uint64_t>(0u, 0u));

        if (m_parametersGraphic.copyType == COPY_TYPE_CMD)
        {
            const vk::Allocation &allocation = m_resetBuffer->getBoundMemory();
            cmdCopyQueryPoolResultsVector(results, vk, device, allocation, queryCount,
                                          (VK_QUERY_RESULT_WAIT_BIT | m_parametersGraphic.querySizeFlags() |
                                           VK_QUERY_RESULT_WITH_AVAILABILITY_BIT),
                                          m_parametersGraphic.dstOffset);
        }
        else
        {
            VK_CHECK(GetQueryPoolResultsVector(results, vk, device, queryPool, 0u, queryCount,
                                               (VK_QUERY_RESULT_WAIT_BIT | m_parametersGraphic.querySizeFlags() |
                                                VK_QUERY_RESULT_WITH_AVAILABILITY_BIT)));
        }

        if (results[0].first < expectedMin || results[0].second == 0)
        {
            std::ostringstream msg;
            msg << "QueryPoolResults incorrect: expected at least " << expectedMin << " with availability 1 but got "
                << results[0].first << " with availability " << results[0].second;
            errorMsg = msg.str();
        }
        else if (queryCount > 1)
        {
            double pearson = calculatePearsonCorrelation(m_drawRepeats, results);
            if (fabs(pearson) < 0.8)
            {
                std::ostringstream msg;
                msg << "QueryPoolResults are nonlinear: Pearson " << pearson << " for";
                for (const auto &x : results)
                    msg << " " << x.first;
                errorMsg = msg.str();
            }
        }
        else
        {
            uint64_t temp = results[0].first;

            vk.resetQueryPool(device, queryPool, 0, queryCount);
            vk::VkResult res = GetQueryPoolResultsVector(
                results, vk, device, queryPool, 0u, queryCount,
                (m_parametersGraphic.querySizeFlags() | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT));

            /* From Vulkan spec:
             *
             * If VK_QUERY_RESULT_WAIT_BIT and VK_QUERY_RESULT_PARTIAL_BIT are both not set then no result values are written to pData
             * for queries that are in the unavailable state at the time of the call, and vkGetQueryPoolResults returns VK_NOT_READY.
             * However, availability state is still written to pData for those queries if VK_QUERY_RESULT_WITH_AVAILABILITY_BIT is set.
             */
            if (res != vk::VK_NOT_READY || results[0].first != temp || results[0].second != 0u)
                errorMsg = "QueryPoolResults incorrect reset";
        }
    }
    else
    {
        // With RESET_TYPE_BEFORE_COPY, we only need to verify the result after the copy include an availability bit set as zero.
        const auto result = verifyUnavailable();
        if (result.isFail())
            errorMsg = result.getDescription();
    }

    // Verify image output if needed
    if (!m_parametersGraphic.noColorAttachments && errorMsg.empty() && !checkImage())
        errorMsg = "Result image doesn't match expected image";

    if (!errorMsg.empty())
        return tcu::TestStatus::fail(errorMsg);
    return tcu::TestStatus::pass("Pass");
}

void TessellationGeometryShaderTestInstance::draw(VkCommandBuffer cmdBuffer)
{
    const DeviceInterface &vk = m_context.getDeviceInterface();
    vk.cmdDraw(cmdBuffer, m_parametersGraphic.tessPatchSize * m_parametersGraphic.numTessPrimitives, 1u, 0u, 0u);
}

class TessellationGeometryShaderSecondaryTestInstance : public TessellationGeometryShaderTestInstance
{
public:
    TessellationGeometryShaderSecondaryTestInstance(vkt::Context &context, const std::vector<VertexData> &data,
                                                    const ParametersGraphic &parametersGraphic,
                                                    const std::vector<uint64_t> &drawRepeats);

protected:
    virtual tcu::TestStatus executeTest(void);
};

TessellationGeometryShaderSecondaryTestInstance::TessellationGeometryShaderSecondaryTestInstance(
    vkt::Context &context, const std::vector<VertexData> &data, const ParametersGraphic &parametersGraphic,
    const std::vector<uint64_t> &drawRepeats)
    : TessellationGeometryShaderTestInstance(context, data, parametersGraphic, drawRepeats)
{
}

tcu::TestStatus TessellationGeometryShaderSecondaryTestInstance::executeTest(void)
{
    const DeviceInterface &vk       = m_context.getDeviceInterface();
    const VkDevice device           = m_context.getDevice();
    const VkQueue queue             = m_context.getUniversalQueue();
    const uint32_t queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();

    const CmdPoolCreateInfo cmdPoolCreateInfo(queueFamilyIndex);
    const Move<VkCommandPool> cmdPool = createCommandPool(vk, device, &cmdPoolCreateInfo);
    const uint32_t queryCount         = static_cast<uint32_t>(m_drawRepeats.size());
    const Unique<VkQueryPool> queryPool(makeQueryPool(vk, device, queryCount, m_parametersGraphic.queryStatisticFlags));

    const VkDeviceSize vertexBufferOffset = 0u;
    const BufferPtr vertexBufferSp        = creatAndFillVertexBuffer();
    const VkBuffer vertexBuffer           = vertexBufferSp->object();

    const Unique<VkCommandBuffer> primaryCmdBuffer(
        allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));
    std::vector<VkCommandBufferSp> secondaryCmdBuffers(queryCount);

    for (uint32_t i = 0; i < queryCount; ++i)
        secondaryCmdBuffers[i] = VkCommandBufferSp(new vk::Unique<VkCommandBuffer>(
            allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_SECONDARY)));

    for (uint32_t i = 0; i < queryCount; ++i)
    {
        beginSecondaryCommandBuffer(vk, secondaryCmdBuffers[i]->get(), m_parametersGraphic.queryStatisticFlags,
                                    *m_renderPass, *m_framebuffer, VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT);
        vk.cmdBeginQuery(secondaryCmdBuffers[i]->get(), *queryPool, i, (VkQueryControlFlags)0u);
        vk.cmdBindPipeline(secondaryCmdBuffers[i]->get(), VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipeline);
        vk.cmdBindVertexBuffers(secondaryCmdBuffers[i]->get(), 0u, 1u, &vertexBuffer, &vertexBufferOffset);
        for (uint32_t j = 0; j < m_drawRepeats[i]; ++j)
            draw(secondaryCmdBuffers[i]->get());
        vk.cmdEndQuery(secondaryCmdBuffers[i]->get(), *queryPool, i);
        endCommandBuffer(vk, secondaryCmdBuffers[i]->get());
    }

    beginCommandBuffer(vk, *primaryCmdBuffer);
    {
        std::vector<VkClearValue> renderPassClearValues(2);
        deMemset(&renderPassClearValues[0], 0, static_cast<int>(renderPassClearValues.size()) * sizeof(VkClearValue));

        if (!m_parametersGraphic.noColorAttachments)
            initialTransitionColor2DImage(
                vk, *primaryCmdBuffer, m_colorAttachmentImage->object(), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

        initialTransitionDepth2DImage(
            vk, *primaryCmdBuffer, m_depthImage->object(), VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT);

        if (m_parametersGraphic.resetType != RESET_TYPE_HOST)
            vk.cmdResetQueryPool(*primaryCmdBuffer, *queryPool, 0u, queryCount);

        beginRenderPass(vk, *primaryCmdBuffer, *m_renderPass, *m_framebuffer, makeRect2D(0, 0, m_width, m_height),
                        (uint32_t)renderPassClearValues.size(), &renderPassClearValues[0],
                        VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);
        for (uint32_t i = 0; i < queryCount; ++i)
            vk.cmdExecuteCommands(*primaryCmdBuffer, 1u, &(secondaryCmdBuffers[i]->get()));
        endRenderPass(vk, *primaryCmdBuffer);

        if (m_parametersGraphic.resetType == RESET_TYPE_BEFORE_COPY ||
            m_parametersGraphic.resetType == RESET_TYPE_AFTER_COPY || m_parametersGraphic.copyType == COPY_TYPE_CMD)
        {
            VkDeviceSize stride          = m_parametersGraphic.querySizeFlags() ? sizeof(uint64_t) : sizeof(uint32_t);
            vk::VkQueryResultFlags flags = m_parametersGraphic.querySizeFlags() | VK_QUERY_RESULT_WAIT_BIT;

            if (m_parametersGraphic.resetType == RESET_TYPE_HOST)
            {
                flags |= VK_QUERY_RESULT_WITH_AVAILABILITY_BIT;
                stride *= 2u;
            }

            if (m_parametersGraphic.resetType == RESET_TYPE_BEFORE_COPY)
            {
                vk.cmdResetQueryPool(*primaryCmdBuffer, *queryPool, 0u, queryCount);
                flags  = VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT;
                stride = sizeof(ValueAndAvailability);
            }

            VkDeviceSize dstOffsetQuery = (m_parametersGraphic.dstOffset) ? stride : 0;
            VkDeviceSize copyStride     = stride;
            if (m_parametersGraphic.strideType == STRIDE_TYPE_ZERO)
                copyStride = 0u;

            vk.cmdCopyQueryPoolResults(*primaryCmdBuffer, *queryPool, 0, queryCount, m_resetBuffer->object(),
                                       dstOffsetQuery, copyStride, flags);

            if (m_parametersGraphic.resetType == RESET_TYPE_AFTER_COPY)
                vk.cmdResetQueryPool(*primaryCmdBuffer, *queryPool, 0u, queryCount);

            const VkBufferMemoryBarrier barrier = {
                VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER, //  VkStructureType sType;
                nullptr,                                 //  const void* pNext;
                VK_ACCESS_TRANSFER_WRITE_BIT,            //  VkAccessFlags srcAccessMask;
                VK_ACCESS_HOST_READ_BIT,                 //  VkAccessFlags dstAccessMask;
                VK_QUEUE_FAMILY_IGNORED,                 //  uint32_t srcQueueFamilyIndex;
                VK_QUEUE_FAMILY_IGNORED,                 //  uint32_t destQueueFamilyIndex;
                m_resetBuffer->object(),                 //  VkBuffer buffer;
                0u,                                      //  VkDeviceSize offset;
                queryCount * stride + dstOffsetQuery,    //  VkDeviceSize size;
            };
            vk.cmdPipelineBarrier(*primaryCmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                                  (VkDependencyFlags)0, 0, nullptr, 1u, &barrier, 0, nullptr);
        }

        if (!m_parametersGraphic.noColorAttachments)
            transition2DImage(vk, *primaryCmdBuffer, m_colorAttachmentImage->object(), VK_IMAGE_ASPECT_COLOR_BIT,
                              VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
                              VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
                              VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
    }
    endCommandBuffer(vk, *primaryCmdBuffer);

    if (m_parametersGraphic.resetType == RESET_TYPE_HOST)
        vk.resetQueryPool(device, *queryPool, 0u, queryCount);

    // Wait for completion
    submitCommandsAndWait(vk, device, queue, *primaryCmdBuffer);
    return checkResult(*queryPool);
}

class TessellationGeometryShaderSecondaryInheritedTestInstance : public TessellationGeometryShaderTestInstance
{
public:
    TessellationGeometryShaderSecondaryInheritedTestInstance(vkt::Context &context, const std::vector<VertexData> &data,
                                                             const ParametersGraphic &parametersGraphic,
                                                             const std::vector<uint64_t> &drawRepeats);

protected:
    virtual void checkExtensions(bool hostQueryResetEnabled);
    virtual tcu::TestStatus executeTest(void);
};

TessellationGeometryShaderSecondaryInheritedTestInstance::TessellationGeometryShaderSecondaryInheritedTestInstance(
    vkt::Context &context, const std::vector<VertexData> &data, const ParametersGraphic &parametersGraphic,
    const std::vector<uint64_t> &drawRepeats)
    : TessellationGeometryShaderTestInstance(context, data, parametersGraphic, drawRepeats)
{
}

void TessellationGeometryShaderSecondaryInheritedTestInstance::checkExtensions(bool hostQueryResetEnabled)
{
    TessellationGeometryShaderTestInstance::checkExtensions(hostQueryResetEnabled);
    if (!m_context.getDeviceFeatures().inheritedQueries)
        throw tcu::NotSupportedError("Inherited queries are not supported");
}

tcu::TestStatus TessellationGeometryShaderSecondaryInheritedTestInstance::executeTest(void)
{
    const DeviceInterface &vk       = m_context.getDeviceInterface();
    const VkDevice device           = m_context.getDevice();
    const VkQueue queue             = m_context.getUniversalQueue();
    const uint32_t queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();

    const CmdPoolCreateInfo cmdPoolCreateInfo(queueFamilyIndex);
    const Move<VkCommandPool> cmdPool = createCommandPool(vk, device, &cmdPoolCreateInfo);
    const uint32_t queryCount         = static_cast<uint32_t>(m_drawRepeats.size());
    const Unique<VkQueryPool> queryPool(makeQueryPool(vk, device, queryCount, m_parametersGraphic.queryStatisticFlags));

    const VkDeviceSize vertexBufferOffset = 0u;
    const BufferPtr vertexBufferSp        = creatAndFillVertexBuffer();
    const VkBuffer vertexBuffer           = vertexBufferSp->object();

    const Unique<VkCommandBuffer> primaryCmdBuffer(
        allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));
    std::vector<VkCommandBufferSp> secondaryCmdBuffers(queryCount);

    for (uint32_t i = 0; i < queryCount; ++i)
        secondaryCmdBuffers[i] = VkCommandBufferSp(new vk::Unique<VkCommandBuffer>(
            allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_SECONDARY)));

    for (uint32_t i = 0; i < queryCount; ++i)
    {
        beginSecondaryCommandBuffer(vk, secondaryCmdBuffers[i]->get(), m_parametersGraphic.queryStatisticFlags,
                                    *m_renderPass, *m_framebuffer, VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT);
        vk.cmdBindPipeline(secondaryCmdBuffers[i]->get(), VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipeline);
        vk.cmdBindVertexBuffers(secondaryCmdBuffers[i]->get(), 0u, 1u, &vertexBuffer, &vertexBufferOffset);
        for (uint32_t j = 0; j < m_drawRepeats[i]; ++j)
            draw(secondaryCmdBuffers[i]->get());
        endCommandBuffer(vk, secondaryCmdBuffers[i]->get());
    }

    beginCommandBuffer(vk, *primaryCmdBuffer);
    {
        std::vector<VkClearValue> renderPassClearValues(2);
        deMemset(&renderPassClearValues[0], 0, static_cast<int>(renderPassClearValues.size()) * sizeof(VkClearValue));

        if (!m_parametersGraphic.noColorAttachments)
            initialTransitionColor2DImage(
                vk, *primaryCmdBuffer, m_colorAttachmentImage->object(), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

        initialTransitionDepth2DImage(
            vk, *primaryCmdBuffer, m_depthImage->object(), VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT);

        if (m_parametersGraphic.resetType != RESET_TYPE_HOST)
            vk.cmdResetQueryPool(*primaryCmdBuffer, *queryPool, 0u, queryCount);

        for (uint32_t i = 0; i < queryCount; ++i)
        {
            vk.cmdBeginQuery(*primaryCmdBuffer, *queryPool, i, (VkQueryControlFlags)0u);
            beginRenderPass(vk, *primaryCmdBuffer, *m_renderPass, *m_framebuffer, makeRect2D(0, 0, m_width, m_height),
                            (uint32_t)renderPassClearValues.size(), &renderPassClearValues[0],
                            VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);
            vk.cmdExecuteCommands(*primaryCmdBuffer, 1u, &(secondaryCmdBuffers[i]->get()));
            endRenderPass(vk, *primaryCmdBuffer);
            vk.cmdEndQuery(*primaryCmdBuffer, *queryPool, i);
        }

        if (m_parametersGraphic.resetType == RESET_TYPE_BEFORE_COPY ||
            m_parametersGraphic.resetType == RESET_TYPE_AFTER_COPY || m_parametersGraphic.copyType == COPY_TYPE_CMD)
        {
            VkDeviceSize stride          = m_parametersGraphic.querySizeFlags() ? sizeof(uint64_t) : sizeof(uint32_t);
            vk::VkQueryResultFlags flags = m_parametersGraphic.querySizeFlags() | VK_QUERY_RESULT_WAIT_BIT;

            if (m_parametersGraphic.resetType == RESET_TYPE_HOST)
            {
                flags |= VK_QUERY_RESULT_WITH_AVAILABILITY_BIT;
                stride *= 2u;
            }

            if (m_parametersGraphic.resetType == RESET_TYPE_BEFORE_COPY)
            {
                vk.cmdResetQueryPool(*primaryCmdBuffer, *queryPool, 0u, queryCount);
                flags  = VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT;
                stride = sizeof(ValueAndAvailability);
            }

            VkDeviceSize dstOffsetQuery = (m_parametersGraphic.dstOffset) ? stride : 0;
            VkDeviceSize copyStride     = stride;
            if (m_parametersGraphic.strideType == STRIDE_TYPE_ZERO)
                copyStride = 0u;

            vk.cmdCopyQueryPoolResults(*primaryCmdBuffer, *queryPool, 0, queryCount, m_resetBuffer->object(),
                                       dstOffsetQuery, copyStride, flags);

            if (m_parametersGraphic.resetType == RESET_TYPE_AFTER_COPY)
                vk.cmdResetQueryPool(*primaryCmdBuffer, *queryPool, 0u, queryCount);

            const VkBufferMemoryBarrier barrier = {
                VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER, //  VkStructureType sType;
                nullptr,                                 //  const void* pNext;
                VK_ACCESS_TRANSFER_WRITE_BIT,            //  VkAccessFlags srcAccessMask;
                VK_ACCESS_HOST_READ_BIT,                 //  VkAccessFlags dstAccessMask;
                VK_QUEUE_FAMILY_IGNORED,                 //  uint32_t srcQueueFamilyIndex;
                VK_QUEUE_FAMILY_IGNORED,                 //  uint32_t destQueueFamilyIndex;
                m_resetBuffer->object(),                 //  VkBuffer buffer;
                0u,                                      //  VkDeviceSize offset;
                queryCount * stride + dstOffsetQuery,    //  VkDeviceSize size;
            };
            vk.cmdPipelineBarrier(*primaryCmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                                  (VkDependencyFlags)0, 0, nullptr, 1u, &barrier, 0, nullptr);
        }

        if (!m_parametersGraphic.noColorAttachments)
            transition2DImage(vk, *primaryCmdBuffer, m_colorAttachmentImage->object(), VK_IMAGE_ASPECT_COLOR_BIT,
                              VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
                              VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
                              VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
    }
    endCommandBuffer(vk, *primaryCmdBuffer);

    if (m_parametersGraphic.resetType == RESET_TYPE_HOST)
        vk.resetQueryPool(device, *queryPool, 0u, queryCount);

    // Wait for completion
    submitCommandsAndWait(vk, device, queue, *primaryCmdBuffer);
    return checkResult(*queryPool);
}

} // namespace

QueryPoolStatisticsTests::QueryPoolStatisticsTests(tcu::TestContext &testCtx)
    : TestCaseGroup(testCtx, "statistics_query")
{
}

inline std::string bitPrefix(bool query64bits, bool dstOffset)
{
    std::string prefix = (query64bits ? "64bits_" : "32bits_");
    prefix += (dstOffset ? "dstoffset_" : "");
    return prefix;
}

void QueryPoolStatisticsTests::init(void)
{
    std::string topology_name[VK_PRIMITIVE_TOPOLOGY_LAST] = {"point_list",
                                                             "line_list",
                                                             "line_strip",
                                                             "triangle_list",
                                                             "triangle_strip",
                                                             "triangle_fan",
                                                             "line_list_with_adjacency",
                                                             "line_strip_with_adjacency",
                                                             "triangle_list_with_adjacency",
                                                             "triangle_strip_with_adjacency",
                                                             "patch_list"};

    std::vector<uint64_t> sixRepeats = {1, 3, 5, 8, 15, 24};

    auto addChilds = [&](de::MovePtr<TestCaseGroup> &group, const std::string &testName,
                         const GraphicBasicTestInstance::ParametersGraphic &baseParams, CommandBufferType cmdBufferType)
    {
        if (baseParams.primitiveTopology != VK_PRIMITIVE_TOPOLOGY_PATCH_LIST)
        {
            if (cmdBufferType == PRIMARY)
            {
                group->addChild(new QueryPoolGraphicStatisticsTest<GeometryShaderTestInstance>(
                    m_testCtx, testName + "_geometry", baseParams, sixRepeats));

                group->addChild(new QueryPoolGraphicStatisticsTest<VertexShaderTestInstance>(
                    m_testCtx, testName + "_vertex", baseParams, sixRepeats));
            }
            else if (cmdBufferType == SECONDARY)
            {
                group->addChild(new QueryPoolGraphicStatisticsTest<GeometryShaderSecondaryTestInstance>(
                    m_testCtx, testName + "_geometry", baseParams, sixRepeats));

                group->addChild(new QueryPoolGraphicStatisticsTest<VertexShaderSecondaryTestInstance>(
                    m_testCtx, testName + "_vertex", baseParams, sixRepeats));
            }
            else // SECONDARY_INHERITED
            {
                group->addChild(new QueryPoolGraphicStatisticsTest<GeometryShaderSecondaryInheritedTestInstance>(
                    m_testCtx, testName + "_geometry", baseParams, sixRepeats));

                group->addChild(new QueryPoolGraphicStatisticsTest<VertexShaderSecondaryInheritedTestInstance>(
                    m_testCtx, testName + "_vertex", baseParams, sixRepeats));
            }
        }
        else
        {
            GraphicBasicTestInstance::ParametersGraphic tessParams = baseParams;

            tessParams.hasTess = true;

            if (tessParams.tessPatchSize == 0)
                tessParams.tessPatchSize = 4; // Default patch size

            if (tessParams.numTessPrimitives == 0)
                tessParams.numTessPrimitives = 1; // Default primitive count

            if (tessParams.strideType == STRIDE_TYPE_ZERO)
                tessParams.strideType = STRIDE_TYPE_VALID;

            if (cmdBufferType == PRIMARY)
            {
                group->addChild(new QueryPoolGraphicStatisticsTest<TessellationShaderTestInstance>(
                    m_testCtx, testName + "_tessellation", tessParams, sixRepeats));

                group->addChild(new QueryPoolGraphicStatisticsTest<TessellationGeometryShaderTestInstance>(
                    m_testCtx, testName + "_tessellation_geometry", tessParams, sixRepeats));
            }
            else if (cmdBufferType == SECONDARY)
            {
                group->addChild(new QueryPoolGraphicStatisticsTest<TessellationGeometryShaderSecondaryTestInstance>(
                    m_testCtx, testName + "_tessellation", tessParams, sixRepeats));

                group->addChild(new QueryPoolGraphicStatisticsTest<TessellationGeometryShaderSecondaryTestInstance>(
                    m_testCtx, testName + "_tessellation_geometry", tessParams, sixRepeats));
            }
            else // SECONDARY_INHERITED
            {
                group->addChild(
                    new QueryPoolGraphicStatisticsTest<TessellationGeometryShaderSecondaryInheritedTestInstance>(
                        m_testCtx, testName + "_tessellation", tessParams, sixRepeats));

                group->addChild(
                    new QueryPoolGraphicStatisticsTest<TessellationGeometryShaderSecondaryInheritedTestInstance>(
                        m_testCtx, testName + "_tessellation_geometry", tessParams, sixRepeats));
            }
        }
    };

    de::MovePtr<TestCaseGroup> computeShaderInvocationsGroup(
        new TestCaseGroup(m_testCtx, "compute_shader_invocations"));
    de::MovePtr<TestCaseGroup> inputAssemblyVertices(new TestCaseGroup(m_testCtx, "input_assembly_vertices"));
    de::MovePtr<TestCaseGroup> inputAssemblyPrimitives(new TestCaseGroup(m_testCtx, "input_assembly_primitives"));
    de::MovePtr<TestCaseGroup> vertexShaderInvocations(new TestCaseGroup(m_testCtx, "vertex_shader_invocations"));
    de::MovePtr<TestCaseGroup> fragmentShaderInvocations(new TestCaseGroup(m_testCtx, "fragment_shader_invocations"));
    de::MovePtr<TestCaseGroup> geometryShaderInvocations(new TestCaseGroup(m_testCtx, "geometry_shader_invocations"));
    de::MovePtr<TestCaseGroup> geometryShaderPrimitives(new TestCaseGroup(m_testCtx, "geometry_shader_primitives"));
    de::MovePtr<TestCaseGroup> clippingInvocations(new TestCaseGroup(m_testCtx, "clipping_invocations"));
    de::MovePtr<TestCaseGroup> clippingPrimitives(new TestCaseGroup(m_testCtx, "clipping_primitives"));
    de::MovePtr<TestCaseGroup> tesControlPatches(new TestCaseGroup(m_testCtx, "tes_control_patches"));
    de::MovePtr<TestCaseGroup> tesEvaluationShaderInvocations(
        new TestCaseGroup(m_testCtx, "tes_evaluation_shader_invocations"));

    de::MovePtr<TestCaseGroup> vertexOnlyGroup(new TestCaseGroup(m_testCtx, "vertex_only"));
    de::MovePtr<TestCaseGroup> inputAssemblyVerticesVertexOnly(new TestCaseGroup(m_testCtx, "input_assembly_vertices"));
    de::MovePtr<TestCaseGroup> inputAssemblyPrimitivesVertexOnly(
        new TestCaseGroup(m_testCtx, "input_assembly_primitives"));
    de::MovePtr<TestCaseGroup> vertexShaderInvocationsVertexOnly(
        new TestCaseGroup(m_testCtx, "vertex_shader_invocations"));

    de::MovePtr<TestCaseGroup> hostQueryResetGroup(new TestCaseGroup(m_testCtx, "host_query_reset"));
    de::MovePtr<TestCaseGroup> computeShaderInvocationsGroupHostQueryReset(
        new TestCaseGroup(m_testCtx, "compute_shader_invocations"));
    de::MovePtr<TestCaseGroup> inputAssemblyVerticesHostQueryReset(
        new TestCaseGroup(m_testCtx, "input_assembly_vertices"));
    de::MovePtr<TestCaseGroup> inputAssemblyPrimitivesHostQueryReset(
        new TestCaseGroup(m_testCtx, "input_assembly_primitives"));
    de::MovePtr<TestCaseGroup> vertexShaderInvocationsHostQueryReset(
        new TestCaseGroup(m_testCtx, "vertex_shader_invocations"));
    de::MovePtr<TestCaseGroup> fragmentShaderInvocationsHostQueryReset(
        new TestCaseGroup(m_testCtx, "fragment_shader_invocations"));
    de::MovePtr<TestCaseGroup> geometryShaderInvocationsHostQueryReset(
        new TestCaseGroup(m_testCtx, "geometry_shader_invocations"));
    de::MovePtr<TestCaseGroup> geometryShaderPrimitivesHostQueryReset(
        new TestCaseGroup(m_testCtx, "geometry_shader_primitives"));
    de::MovePtr<TestCaseGroup> clippingInvocationsHostQueryReset(new TestCaseGroup(m_testCtx, "clipping_invocations"));
    de::MovePtr<TestCaseGroup> clippingPrimitivesHostQueryReset(new TestCaseGroup(m_testCtx, "clipping_primitives"));
    de::MovePtr<TestCaseGroup> tesControlPatchesHostQueryReset(new TestCaseGroup(m_testCtx, "tes_control_patches"));
    de::MovePtr<TestCaseGroup> tesEvaluationShaderInvocationsHostQueryReset(
        new TestCaseGroup(m_testCtx, "tes_evaluation_shader_invocations"));

    de::MovePtr<TestCaseGroup> resetBeforeCopyGroup(new TestCaseGroup(m_testCtx, "reset_before_copy"));
    de::MovePtr<TestCaseGroup> computeShaderInvocationsGroupResetBeforeCopy(
        new TestCaseGroup(m_testCtx, "compute_shader_invocations"));
    de::MovePtr<TestCaseGroup> inputAssemblyVerticesResetBeforeCopy(
        new TestCaseGroup(m_testCtx, "input_assembly_vertices"));
    de::MovePtr<TestCaseGroup> inputAssemblyPrimitivesResetBeforeCopy(
        new TestCaseGroup(m_testCtx, "input_assembly_primitives"));
    de::MovePtr<TestCaseGroup> vertexShaderInvocationsResetBeforeCopy(
        new TestCaseGroup(m_testCtx, "vertex_shader_invocations"));
    de::MovePtr<TestCaseGroup> fragmentShaderInvocationsResetBeforeCopy(
        new TestCaseGroup(m_testCtx, "fragment_shader_invocations"));
    de::MovePtr<TestCaseGroup> geometryShaderInvocationsResetBeforeCopy(
        new TestCaseGroup(m_testCtx, "geometry_shader_invocations"));
    de::MovePtr<TestCaseGroup> geometryShaderPrimitivesResetBeforeCopy(
        new TestCaseGroup(m_testCtx, "geometry_shader_primitives"));
    de::MovePtr<TestCaseGroup> clippingInvocationsResetBeforeCopy(new TestCaseGroup(m_testCtx, "clipping_invocations"));
    de::MovePtr<TestCaseGroup> clippingPrimitivesResetBeforeCopy(new TestCaseGroup(m_testCtx, "clipping_primitives"));
    de::MovePtr<TestCaseGroup> tesControlPatchesResetBeforeCopy(new TestCaseGroup(m_testCtx, "tes_control_patches"));
    de::MovePtr<TestCaseGroup> tesEvaluationShaderInvocationsResetBeforeCopy(
        new TestCaseGroup(m_testCtx, "tes_evaluation_shader_invocations"));

    de::MovePtr<TestCaseGroup> resetAfterCopyGroup(new TestCaseGroup(m_testCtx, "reset_after_copy"));
    de::MovePtr<TestCaseGroup> computeShaderInvocationsGroupResetAfterCopy(
        new TestCaseGroup(m_testCtx, "compute_shader_invocations"));
    de::MovePtr<TestCaseGroup> inputAssemblyVerticesResetAfterCopy(
        new TestCaseGroup(m_testCtx, "input_assembly_vertices"));
    de::MovePtr<TestCaseGroup> inputAssemblyPrimitivesResetAfterCopy(
        new TestCaseGroup(m_testCtx, "input_assembly_primitives"));
    de::MovePtr<TestCaseGroup> vertexShaderInvocationsResetAfterCopy(
        new TestCaseGroup(m_testCtx, "vertex_shader_invocations"));
    de::MovePtr<TestCaseGroup> fragmentShaderInvocationsResetAfterCopy(
        new TestCaseGroup(m_testCtx, "fragment_shader_invocations"));
    de::MovePtr<TestCaseGroup> geometryShaderInvocationsResetAfterCopy(
        new TestCaseGroup(m_testCtx, "geometry_shader_invocations"));
    de::MovePtr<TestCaseGroup> geometryShaderPrimitivesResetAfterCopy(
        new TestCaseGroup(m_testCtx, "geometry_shader_primitives"));
    de::MovePtr<TestCaseGroup> clippingInvocationsResetAfterCopy(new TestCaseGroup(m_testCtx, "clipping_invocations"));
    de::MovePtr<TestCaseGroup> clippingPrimitivesResetAfterCopy(new TestCaseGroup(m_testCtx, "clipping_primitives"));
    de::MovePtr<TestCaseGroup> tesControlPatchesResetAfterCopy(new TestCaseGroup(m_testCtx, "tes_control_patches"));
    de::MovePtr<TestCaseGroup> tesEvaluationShaderInvocationsResetAfterCopy(
        new TestCaseGroup(m_testCtx, "tes_evaluation_shader_invocations"));

    de::MovePtr<TestCaseGroup> vertexShaderMultipleQueries(new TestCaseGroup(m_testCtx, "multiple_queries"));
    de::MovePtr<TestCaseGroup> multipleGeomStats(new TestCaseGroup(m_testCtx, "multiple_geom_stats"));

    CopyType copyType[]       = {COPY_TYPE_GET, COPY_TYPE_CMD};
    std::string copyTypeStr[] = {"", "cmdcopyquerypoolresults_"};

    StrideType strideType[]     = {STRIDE_TYPE_VALID, STRIDE_TYPE_ZERO};
    std::string strideTypeStr[] = {"", "stride_zero_"};

    for (uint32_t copyTypeIdx = 0; copyTypeIdx < DE_LENGTH_OF_ARRAY(copyType); copyTypeIdx++)
    {
        for (uint32_t i = 0; i < 4; ++i)
        {
            bool query64Bits   = (i & 1);
            bool dstOffset     = (i & 2);
            std::string prefix = bitPrefix(query64Bits, dstOffset);

            // It makes no sense to use dstOffset with vkGetQueryPoolResults()
            if (copyType[copyTypeIdx] == COPY_TYPE_GET && dstOffset)
                continue;

            //VK_QUERY_PIPELINE_STATISTIC_COMPUTE_SHADER_INVOCATIONS_BIT

            for (const auto computeQueue : {false, true})
            {
                const std::string cqSuffix = (computeQueue ? "_cq" : "");

                for (uint32_t strideTypeIdx = 0; strideTypeIdx < DE_LENGTH_OF_ARRAY(strideType); strideTypeIdx++)
                {
                    if (strideType[strideTypeIdx] == STRIDE_TYPE_ZERO && copyType[copyTypeIdx] != COPY_TYPE_CMD)
                        continue;

                    computeShaderInvocationsGroup->addChild(
                        new QueryPoolComputeStatsTest<ComputeInvocationsTestInstance>(
                            m_testCtx,
                            prefix + copyTypeStr[copyTypeIdx] + strideTypeStr[strideTypeIdx] + "primary" + cqSuffix,
                            RESET_TYPE_NORMAL, copyType[copyTypeIdx], query64Bits, computeQueue, dstOffset,
                            strideType[strideTypeIdx]));
                    computeShaderInvocationsGroup->addChild(
                        new QueryPoolComputeStatsTest<ComputeInvocationsSecondaryTestInstance>(
                            m_testCtx,
                            prefix + copyTypeStr[copyTypeIdx] + strideTypeStr[strideTypeIdx] + "secondary" + cqSuffix,
                            RESET_TYPE_NORMAL, copyType[copyTypeIdx], query64Bits, computeQueue, dstOffset,
                            strideType[strideTypeIdx]));
                    computeShaderInvocationsGroup->addChild(
                        new QueryPoolComputeStatsTest<ComputeInvocationsSecondaryInheritedTestInstance>(
                            m_testCtx,
                            prefix + copyTypeStr[copyTypeIdx] + strideTypeStr[strideTypeIdx] + "secondary_inherited" +
                                cqSuffix,
                            RESET_TYPE_NORMAL, copyType[copyTypeIdx], query64Bits, computeQueue, dstOffset,
                            strideType[strideTypeIdx]));
                }

                computeShaderInvocationsGroupHostQueryReset->addChild(
                    new QueryPoolComputeStatsTest<ComputeInvocationsTestInstance>(
                        m_testCtx, prefix + copyTypeStr[copyTypeIdx] + "primary" + cqSuffix, RESET_TYPE_HOST,
                        copyType[copyTypeIdx], query64Bits, computeQueue, dstOffset));
                computeShaderInvocationsGroupHostQueryReset->addChild(
                    new QueryPoolComputeStatsTest<ComputeInvocationsSecondaryTestInstance>(
                        m_testCtx, prefix + copyTypeStr[copyTypeIdx] + "secondary" + cqSuffix, RESET_TYPE_HOST,
                        copyType[copyTypeIdx], query64Bits, computeQueue, dstOffset));
                computeShaderInvocationsGroupHostQueryReset->addChild(
                    new QueryPoolComputeStatsTest<ComputeInvocationsSecondaryInheritedTestInstance>(
                        m_testCtx, prefix + copyTypeStr[copyTypeIdx] + "secondary_inherited" + cqSuffix,
                        RESET_TYPE_HOST, copyType[copyTypeIdx], query64Bits, computeQueue, dstOffset));

                computeShaderInvocationsGroupResetBeforeCopy->addChild(
                    new QueryPoolComputeStatsTest<ComputeInvocationsTestInstance>(
                        m_testCtx, prefix + copyTypeStr[copyTypeIdx] + "primary" + cqSuffix, RESET_TYPE_BEFORE_COPY,
                        copyType[copyTypeIdx], query64Bits, computeQueue, dstOffset));
                computeShaderInvocationsGroupResetBeforeCopy->addChild(
                    new QueryPoolComputeStatsTest<ComputeInvocationsSecondaryTestInstance>(
                        m_testCtx, prefix + copyTypeStr[copyTypeIdx] + "secondary" + cqSuffix, RESET_TYPE_BEFORE_COPY,
                        copyType[copyTypeIdx], query64Bits, computeQueue, dstOffset));
                computeShaderInvocationsGroupResetBeforeCopy->addChild(
                    new QueryPoolComputeStatsTest<ComputeInvocationsSecondaryInheritedTestInstance>(
                        m_testCtx, prefix + copyTypeStr[copyTypeIdx] + "secondary_inherited" + cqSuffix,
                        RESET_TYPE_BEFORE_COPY, copyType[copyTypeIdx], query64Bits, computeQueue, dstOffset));

                if (copyType[copyTypeIdx] == COPY_TYPE_CMD)
                {
                    computeShaderInvocationsGroupResetAfterCopy->addChild(
                        new QueryPoolComputeStatsTest<ComputeInvocationsTestInstance>(
                            m_testCtx, prefix + copyTypeStr[copyTypeIdx] + "primary" + cqSuffix, RESET_TYPE_AFTER_COPY,
                            copyType[copyTypeIdx], query64Bits, computeQueue, dstOffset));
                    computeShaderInvocationsGroupResetAfterCopy->addChild(
                        new QueryPoolComputeStatsTest<ComputeInvocationsSecondaryTestInstance>(
                            m_testCtx, prefix + copyTypeStr[copyTypeIdx] + "secondary" + cqSuffix,
                            RESET_TYPE_AFTER_COPY, copyType[copyTypeIdx], query64Bits, computeQueue, dstOffset));
                    computeShaderInvocationsGroupResetAfterCopy->addChild(
                        new QueryPoolComputeStatsTest<ComputeInvocationsSecondaryInheritedTestInstance>(
                            m_testCtx, prefix + copyTypeStr[copyTypeIdx] + "secondary_inherited" + cqSuffix,
                            RESET_TYPE_AFTER_COPY, copyType[copyTypeIdx], query64Bits, computeQueue, dstOffset));
                }
            }

            //VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_VERTICES_BIT

            // Tests with no attachments for only primary command to reduce # of test cases.
            inputAssemblyVertices->addChild(new QueryPoolGraphicStatisticsTest<VertexShaderTestInstance>(
                m_testCtx, prefix + copyTypeStr[copyTypeIdx] + "primary_with_no_color_attachments",
                GraphicBasicTestInstance::ParametersGraphic(
                    VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_VERTICES_BIT, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
                    RESET_TYPE_NORMAL, copyType[copyTypeIdx], query64Bits, false, dstOffset, CLEAR_NOOP, true),
                sixRepeats));
            inputAssemblyVerticesVertexOnly->addChild(new QueryPoolGraphicStatisticsTest<VertexShaderTestInstance>(
                m_testCtx, prefix + copyTypeStr[copyTypeIdx] + "primary_with_no_color_attachments",
                GraphicBasicTestInstance::ParametersGraphic(
                    VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_VERTICES_BIT, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
                    RESET_TYPE_NORMAL, copyType[copyTypeIdx], query64Bits, true, dstOffset, CLEAR_NOOP, true),
                sixRepeats));
            inputAssemblyVerticesHostQueryReset->addChild(new QueryPoolGraphicStatisticsTest<VertexShaderTestInstance>(
                m_testCtx, prefix + copyTypeStr[copyTypeIdx] + "primary_with_no_color_attachments",
                GraphicBasicTestInstance::ParametersGraphic(
                    VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_VERTICES_BIT, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
                    RESET_TYPE_HOST, copyType[copyTypeIdx], query64Bits, false, dstOffset, CLEAR_NOOP, true),
                sixRepeats));
            inputAssemblyVerticesResetBeforeCopy->addChild(new QueryPoolGraphicStatisticsTest<VertexShaderTestInstance>(
                m_testCtx, prefix + copyTypeStr[copyTypeIdx] + "primary_with_no_color_attachments",
                GraphicBasicTestInstance::ParametersGraphic(
                    VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_VERTICES_BIT, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
                    RESET_TYPE_BEFORE_COPY, copyType[copyTypeIdx], query64Bits, false, dstOffset, CLEAR_NOOP, true),
                sixRepeats));

            if (copyType[copyTypeIdx] == COPY_TYPE_CMD)
                inputAssemblyVerticesResetAfterCopy->addChild(
                    new QueryPoolGraphicStatisticsTest<VertexShaderTestInstance>(
                        m_testCtx, prefix + copyTypeStr[copyTypeIdx] + "primary_with_no_color_attachments",
                        GraphicBasicTestInstance::ParametersGraphic(
                            VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_VERTICES_BIT,
                            VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, RESET_TYPE_AFTER_COPY, copyType[copyTypeIdx],
                            query64Bits, false, dstOffset, CLEAR_NOOP, true),
                        sixRepeats));

            /* Tests for clear operation within a statistics query activated.
             * The query shouldn't count internal driver operations relevant to the clear operations.
             */
            const ClearOperation clearOp[] = {CLEAR_NOOP, CLEAR_COLOR, CLEAR_DEPTH};
            const char *const clearOpStr[] = {"", "_clear_color", "_clear_depth"};

            for (int clearOpIdx = 0; clearOpIdx < DE_LENGTH_OF_ARRAY(clearOp); ++clearOpIdx)
            {
                inputAssemblyVertices->addChild(new QueryPoolGraphicStatisticsTest<VertexShaderTestInstance>(
                    m_testCtx, prefix + copyTypeStr[copyTypeIdx] + "primary" + clearOpStr[clearOpIdx],
                    GraphicBasicTestInstance::ParametersGraphic(
                        VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_VERTICES_BIT, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
                        RESET_TYPE_NORMAL, copyType[copyTypeIdx], query64Bits, false, dstOffset, clearOp[clearOpIdx]),
                    sixRepeats));
                inputAssemblyVertices->addChild(new QueryPoolGraphicStatisticsTest<VertexShaderSecondaryTestInstance>(
                    m_testCtx, prefix + copyTypeStr[copyTypeIdx] + "secondary" + clearOpStr[clearOpIdx],
                    GraphicBasicTestInstance::ParametersGraphic(
                        VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_VERTICES_BIT, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
                        RESET_TYPE_NORMAL, copyType[copyTypeIdx], query64Bits, false, dstOffset, clearOp[clearOpIdx]),
                    sixRepeats));

                inputAssemblyVerticesVertexOnly->addChild(new QueryPoolGraphicStatisticsTest<VertexShaderTestInstance>(
                    m_testCtx, prefix + copyTypeStr[copyTypeIdx] + "primary" + clearOpStr[clearOpIdx],
                    GraphicBasicTestInstance::ParametersGraphic(
                        VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_VERTICES_BIT, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
                        RESET_TYPE_NORMAL, copyType[copyTypeIdx], query64Bits, true, dstOffset, clearOp[clearOpIdx]),
                    sixRepeats));
                inputAssemblyVerticesVertexOnly->addChild(
                    new QueryPoolGraphicStatisticsTest<VertexShaderSecondaryTestInstance>(
                        m_testCtx, prefix + copyTypeStr[copyTypeIdx] + "secondary" + clearOpStr[clearOpIdx],
                        GraphicBasicTestInstance::ParametersGraphic(
                            VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_VERTICES_BIT,
                            VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, RESET_TYPE_NORMAL, copyType[copyTypeIdx], query64Bits,
                            true, dstOffset, clearOp[clearOpIdx]),
                        sixRepeats));

                inputAssemblyVerticesHostQueryReset->addChild(
                    new QueryPoolGraphicStatisticsTest<VertexShaderTestInstance>(
                        m_testCtx, prefix + copyTypeStr[copyTypeIdx] + "primary" + clearOpStr[clearOpIdx],
                        GraphicBasicTestInstance::ParametersGraphic(
                            VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_VERTICES_BIT,
                            VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, RESET_TYPE_HOST, copyType[copyTypeIdx], query64Bits,
                            false, dstOffset, clearOp[clearOpIdx]),
                        sixRepeats));
                inputAssemblyVerticesHostQueryReset->addChild(
                    new QueryPoolGraphicStatisticsTest<VertexShaderSecondaryTestInstance>(
                        m_testCtx, prefix + copyTypeStr[copyTypeIdx] + "secondary" + clearOpStr[clearOpIdx],
                        GraphicBasicTestInstance::ParametersGraphic(
                            VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_VERTICES_BIT,
                            VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, RESET_TYPE_HOST, copyType[copyTypeIdx], query64Bits,
                            false, dstOffset, clearOp[clearOpIdx]),
                        sixRepeats));

                inputAssemblyVerticesResetBeforeCopy->addChild(
                    new QueryPoolGraphicStatisticsTest<VertexShaderTestInstance>(
                        m_testCtx, prefix + copyTypeStr[copyTypeIdx] + "primary" + clearOpStr[clearOpIdx],
                        GraphicBasicTestInstance::ParametersGraphic(
                            VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_VERTICES_BIT,
                            VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, RESET_TYPE_BEFORE_COPY, copyType[copyTypeIdx],
                            query64Bits, false, dstOffset, clearOp[clearOpIdx]),
                        sixRepeats));
                inputAssemblyVerticesResetBeforeCopy->addChild(
                    new QueryPoolGraphicStatisticsTest<VertexShaderSecondaryTestInstance>(
                        m_testCtx, prefix + copyTypeStr[copyTypeIdx] + "secondary" + clearOpStr[clearOpIdx],
                        GraphicBasicTestInstance::ParametersGraphic(
                            VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_VERTICES_BIT,
                            VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, RESET_TYPE_BEFORE_COPY, copyType[copyTypeIdx],
                            query64Bits, false, dstOffset, clearOp[clearOpIdx]),
                        sixRepeats));

                if (copyType[copyTypeIdx] == COPY_TYPE_CMD)
                {
                    inputAssemblyVerticesResetAfterCopy->addChild(
                        new QueryPoolGraphicStatisticsTest<VertexShaderTestInstance>(
                            m_testCtx, prefix + copyTypeStr[copyTypeIdx] + "primary" + clearOpStr[clearOpIdx],
                            GraphicBasicTestInstance::ParametersGraphic(
                                VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_VERTICES_BIT,
                                VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, RESET_TYPE_AFTER_COPY, copyType[copyTypeIdx],
                                query64Bits, false, dstOffset, clearOp[clearOpIdx]),
                            sixRepeats));
                    inputAssemblyVerticesResetAfterCopy->addChild(
                        new QueryPoolGraphicStatisticsTest<VertexShaderSecondaryTestInstance>(
                            m_testCtx, prefix + copyTypeStr[copyTypeIdx] + "secondary" + clearOpStr[clearOpIdx],
                            GraphicBasicTestInstance::ParametersGraphic(
                                VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_VERTICES_BIT,
                                VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, RESET_TYPE_AFTER_COPY, copyType[copyTypeIdx],
                                query64Bits, false, dstOffset, clearOp[clearOpIdx]),
                            sixRepeats));
                }
            }

            inputAssemblyVertices->addChild(
                new QueryPoolGraphicStatisticsTest<VertexShaderSecondaryInheritedTestInstance>(
                    m_testCtx, prefix + copyTypeStr[copyTypeIdx] + "secondary_inherited",
                    GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_VERTICES_BIT,
                                                                VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, RESET_TYPE_NORMAL,
                                                                copyType[copyTypeIdx], query64Bits, false, dstOffset),
                    sixRepeats));

            inputAssemblyVerticesVertexOnly->addChild(
                new QueryPoolGraphicStatisticsTest<VertexShaderSecondaryInheritedTestInstance>(
                    m_testCtx, prefix + copyTypeStr[copyTypeIdx] + "secondary_inherited",
                    GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_VERTICES_BIT,
                                                                VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, RESET_TYPE_NORMAL,
                                                                copyType[copyTypeIdx], query64Bits, true, dstOffset),
                    sixRepeats));

            inputAssemblyVerticesHostQueryReset->addChild(
                new QueryPoolGraphicStatisticsTest<VertexShaderSecondaryInheritedTestInstance>(
                    m_testCtx, prefix + copyTypeStr[copyTypeIdx] + "secondary_inherited",
                    GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_VERTICES_BIT,
                                                                VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, RESET_TYPE_HOST,
                                                                copyType[copyTypeIdx], query64Bits, false, dstOffset),
                    sixRepeats));

            inputAssemblyVerticesResetBeforeCopy->addChild(
                new QueryPoolGraphicStatisticsTest<VertexShaderSecondaryInheritedTestInstance>(
                    m_testCtx, prefix + copyTypeStr[copyTypeIdx] + "secondary_inherited",
                    GraphicBasicTestInstance::ParametersGraphic(
                        VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_VERTICES_BIT, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
                        RESET_TYPE_BEFORE_COPY, copyType[copyTypeIdx], query64Bits, false, dstOffset),
                    sixRepeats));

            if (copyType[copyTypeIdx] == COPY_TYPE_CMD)
                inputAssemblyVerticesResetAfterCopy->addChild(
                    new QueryPoolGraphicStatisticsTest<VertexShaderSecondaryInheritedTestInstance>(
                        m_testCtx, prefix + copyTypeStr[copyTypeIdx] + "secondary_inherited",
                        GraphicBasicTestInstance::ParametersGraphic(
                            VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_VERTICES_BIT,
                            VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, RESET_TYPE_AFTER_COPY, copyType[copyTypeIdx],
                            query64Bits, false, dstOffset),
                        sixRepeats));
        }
    }

    //VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT
    {
        de::MovePtr<TestCaseGroup> primary(new TestCaseGroup(m_testCtx, "primary"));
        de::MovePtr<TestCaseGroup> secondary(new TestCaseGroup(m_testCtx, "secondary"));
        de::MovePtr<TestCaseGroup> secondaryInherited(new TestCaseGroup(m_testCtx, "secondary_inherited"));

        de::MovePtr<TestCaseGroup> primaryVertexOnly(new TestCaseGroup(m_testCtx, "primary"));
        de::MovePtr<TestCaseGroup> secondaryVertexOnly(new TestCaseGroup(m_testCtx, "secondary"));
        de::MovePtr<TestCaseGroup> secondaryInheritedVertexOnly(new TestCaseGroup(m_testCtx, "secondary_inherited"));

        de::MovePtr<TestCaseGroup> primaryHostQueryReset(new TestCaseGroup(m_testCtx, "primary"));
        de::MovePtr<TestCaseGroup> secondaryHostQueryReset(new TestCaseGroup(m_testCtx, "secondary"));
        de::MovePtr<TestCaseGroup> secondaryInheritedHostQueryReset(
            new TestCaseGroup(m_testCtx, "secondary_inherited"));

        de::MovePtr<TestCaseGroup> primaryResetBeforeCopy(new TestCaseGroup(m_testCtx, "primary"));
        de::MovePtr<TestCaseGroup> secondaryResetBeforeCopy(new TestCaseGroup(m_testCtx, "secondary"));
        de::MovePtr<TestCaseGroup> secondaryInheritedResetBeforeCopy(
            new TestCaseGroup(m_testCtx, "secondary_inherited"));

        de::MovePtr<TestCaseGroup> primaryResetAfterCopy(new TestCaseGroup(m_testCtx, "primary"));
        de::MovePtr<TestCaseGroup> secondaryResetAfterCopy(new TestCaseGroup(m_testCtx, "secondary"));
        de::MovePtr<TestCaseGroup> secondaryInheritedResetAfterCopy(
            new TestCaseGroup(m_testCtx, "secondary_inherited"));

        for (uint32_t copyTypeIdx = 0; copyTypeIdx < DE_LENGTH_OF_ARRAY(copyType); copyTypeIdx++)
        {
            for (int topologyNdx = VK_PRIMITIVE_TOPOLOGY_POINT_LIST; topologyNdx < VK_PRIMITIVE_TOPOLOGY_PATCH_LIST;
                 ++topologyNdx)
            {
                for (uint32_t i = 0; i < 4; ++i)
                {
                    bool query64Bits   = (i & 1);
                    bool dstOffset     = (i & 2);
                    std::string prefix = bitPrefix(query64Bits, dstOffset);

                    // It makes no sense to use dstOffset with vkGetQueryPoolResults()
                    if (copyType[copyTypeIdx] == COPY_TYPE_GET && dstOffset)
                        continue;

                    // Tests with no attachments for only primary command to reduce # of test cases.
                    primary->addChild(new QueryPoolGraphicStatisticsTest<VertexShaderTestInstance>(
                        m_testCtx,
                        prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] + "_with_no_color_attachments",
                        GraphicBasicTestInstance::ParametersGraphic(
                            VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT, (VkPrimitiveTopology)topologyNdx,
                            RESET_TYPE_NORMAL, copyType[copyTypeIdx], query64Bits, false, dstOffset, CLEAR_NOOP, true),
                        sixRepeats));

                    primaryHostQueryReset->addChild(new QueryPoolGraphicStatisticsTest<VertexShaderTestInstance>(
                        m_testCtx,
                        prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] + "_with_no_color_attachments",
                        GraphicBasicTestInstance::ParametersGraphic(
                            VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT, (VkPrimitiveTopology)topologyNdx,
                            RESET_TYPE_HOST, copyType[copyTypeIdx], query64Bits, false, dstOffset, CLEAR_NOOP, true),
                        sixRepeats));

                    primaryVertexOnly->addChild(new QueryPoolGraphicStatisticsTest<VertexShaderTestInstance>(
                        m_testCtx,
                        prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] + "_with_no_color_attachments",
                        GraphicBasicTestInstance::ParametersGraphic(
                            VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT, (VkPrimitiveTopology)topologyNdx,
                            RESET_TYPE_NORMAL, copyType[copyTypeIdx], query64Bits, true, dstOffset, CLEAR_NOOP, true),
                        sixRepeats));

                    primaryResetBeforeCopy->addChild(new QueryPoolGraphicStatisticsTest<VertexShaderTestInstance>(
                        m_testCtx,
                        prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] + "_with_no_color_attachments",
                        GraphicBasicTestInstance::ParametersGraphic(
                            VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT, (VkPrimitiveTopology)topologyNdx,
                            RESET_TYPE_BEFORE_COPY, copyType[copyTypeIdx], query64Bits, false, dstOffset, CLEAR_NOOP,
                            true),
                        sixRepeats));

                    if (copyType[copyTypeIdx] == COPY_TYPE_CMD)
                        primaryResetAfterCopy->addChild(new QueryPoolGraphicStatisticsTest<VertexShaderTestInstance>(
                            m_testCtx,
                            prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] +
                                "_with_no_color_attachments",
                            GraphicBasicTestInstance::ParametersGraphic(
                                VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT,
                                (VkPrimitiveTopology)topologyNdx, RESET_TYPE_AFTER_COPY, copyType[copyTypeIdx],
                                query64Bits, false, dstOffset, CLEAR_NOOP, true),
                            sixRepeats));

                    /* Tests for clear operation within a statistics query activated.
                     * Nothing for secondary_inherited cases can be done since it violates the specification.
                     *
                     * The query shouldn't count internal driver operations relevant to the clear operations.
                     */
                    const ClearOperation clearOp[] = {CLEAR_NOOP, CLEAR_COLOR, CLEAR_DEPTH};
                    const char *const clearOpStr[] = {"", "_clear_color", "_clear_depth"};

                    for (int clearOpIdx = 0; clearOpIdx < DE_LENGTH_OF_ARRAY(clearOp); ++clearOpIdx)
                    {
                        primary->addChild(new QueryPoolGraphicStatisticsTest<VertexShaderTestInstance>(
                            m_testCtx,
                            prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] + clearOpStr[clearOpIdx],
                            GraphicBasicTestInstance::ParametersGraphic(
                                VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT,
                                (VkPrimitiveTopology)topologyNdx, RESET_TYPE_NORMAL, copyType[copyTypeIdx], query64Bits,
                                false, dstOffset, clearOp[clearOpIdx]),
                            sixRepeats));
                        secondary->addChild(new QueryPoolGraphicStatisticsTest<VertexShaderSecondaryTestInstance>(
                            m_testCtx,
                            prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] + clearOpStr[clearOpIdx],
                            GraphicBasicTestInstance::ParametersGraphic(
                                VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT,
                                (VkPrimitiveTopology)topologyNdx, RESET_TYPE_NORMAL, copyType[copyTypeIdx], query64Bits,
                                false, dstOffset, clearOp[clearOpIdx]),
                            sixRepeats));

                        primaryHostQueryReset->addChild(new QueryPoolGraphicStatisticsTest<VertexShaderTestInstance>(
                            m_testCtx,
                            prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] + clearOpStr[clearOpIdx],
                            GraphicBasicTestInstance::ParametersGraphic(
                                VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT,
                                (VkPrimitiveTopology)topologyNdx, RESET_TYPE_HOST, copyType[copyTypeIdx], query64Bits,
                                false, dstOffset, clearOp[clearOpIdx]),
                            sixRepeats));
                        secondaryHostQueryReset->addChild(
                            new QueryPoolGraphicStatisticsTest<VertexShaderSecondaryTestInstance>(
                                m_testCtx,
                                prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] + clearOpStr[clearOpIdx],
                                GraphicBasicTestInstance::ParametersGraphic(
                                    VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT,
                                    (VkPrimitiveTopology)topologyNdx, RESET_TYPE_HOST, copyType[copyTypeIdx],
                                    query64Bits, false, dstOffset, clearOp[clearOpIdx]),
                                sixRepeats));

                        primaryVertexOnly->addChild(new QueryPoolGraphicStatisticsTest<VertexShaderTestInstance>(
                            m_testCtx,
                            prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] + clearOpStr[clearOpIdx],
                            GraphicBasicTestInstance::ParametersGraphic(
                                VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT,
                                (VkPrimitiveTopology)topologyNdx, RESET_TYPE_NORMAL, copyType[copyTypeIdx], query64Bits,
                                true, dstOffset, clearOp[clearOpIdx]),
                            sixRepeats));
                        secondaryVertexOnly->addChild(
                            new QueryPoolGraphicStatisticsTest<VertexShaderSecondaryTestInstance>(
                                m_testCtx,
                                prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] + clearOpStr[clearOpIdx],
                                GraphicBasicTestInstance::ParametersGraphic(
                                    VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT,
                                    (VkPrimitiveTopology)topologyNdx, RESET_TYPE_NORMAL, copyType[copyTypeIdx],
                                    query64Bits, true, dstOffset, clearOp[clearOpIdx]),
                                sixRepeats));

                        primaryResetBeforeCopy->addChild(new QueryPoolGraphicStatisticsTest<VertexShaderTestInstance>(
                            m_testCtx,
                            prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] + clearOpStr[clearOpIdx],
                            GraphicBasicTestInstance::ParametersGraphic(
                                VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT,
                                (VkPrimitiveTopology)topologyNdx, RESET_TYPE_BEFORE_COPY, copyType[copyTypeIdx],
                                query64Bits, false, dstOffset, clearOp[clearOpIdx]),
                            sixRepeats));
                        secondaryResetBeforeCopy->addChild(
                            new QueryPoolGraphicStatisticsTest<VertexShaderSecondaryTestInstance>(
                                m_testCtx,
                                prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] + clearOpStr[clearOpIdx],
                                GraphicBasicTestInstance::ParametersGraphic(
                                    VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT,
                                    (VkPrimitiveTopology)topologyNdx, RESET_TYPE_BEFORE_COPY, copyType[copyTypeIdx],
                                    query64Bits, false, dstOffset, clearOp[clearOpIdx]),
                                sixRepeats));

                        if (copyType[copyTypeIdx] == COPY_TYPE_CMD)
                        {
                            primaryResetAfterCopy->addChild(
                                new QueryPoolGraphicStatisticsTest<VertexShaderTestInstance>(
                                    m_testCtx,
                                    prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] +
                                        clearOpStr[clearOpIdx],
                                    GraphicBasicTestInstance::ParametersGraphic(
                                        VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT,
                                        (VkPrimitiveTopology)topologyNdx, RESET_TYPE_AFTER_COPY, copyType[copyTypeIdx],
                                        query64Bits, false, dstOffset, clearOp[clearOpIdx]),
                                    sixRepeats));
                            secondaryResetAfterCopy->addChild(
                                new QueryPoolGraphicStatisticsTest<VertexShaderSecondaryTestInstance>(
                                    m_testCtx,
                                    prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] +
                                        clearOpStr[clearOpIdx],
                                    GraphicBasicTestInstance::ParametersGraphic(
                                        VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT,
                                        (VkPrimitiveTopology)topologyNdx, RESET_TYPE_AFTER_COPY, copyType[copyTypeIdx],
                                        query64Bits, false, dstOffset, clearOp[clearOpIdx]),
                                    sixRepeats));
                        }
                    }

                    secondaryInherited->addChild(
                        new QueryPoolGraphicStatisticsTest<VertexShaderSecondaryInheritedTestInstance>(
                            m_testCtx, prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx],
                            GraphicBasicTestInstance::ParametersGraphic(
                                VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT,
                                (VkPrimitiveTopology)topologyNdx, RESET_TYPE_NORMAL, copyType[copyTypeIdx], query64Bits,
                                false, dstOffset),
                            sixRepeats));
                    secondaryInheritedHostQueryReset->addChild(
                        new QueryPoolGraphicStatisticsTest<VertexShaderSecondaryInheritedTestInstance>(
                            m_testCtx, prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx],
                            GraphicBasicTestInstance::ParametersGraphic(
                                VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT,
                                (VkPrimitiveTopology)topologyNdx, RESET_TYPE_HOST, copyType[copyTypeIdx], query64Bits,
                                false, dstOffset),
                            sixRepeats));
                    secondaryInheritedVertexOnly->addChild(
                        new QueryPoolGraphicStatisticsTest<VertexShaderSecondaryInheritedTestInstance>(
                            m_testCtx, prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx],
                            GraphicBasicTestInstance::ParametersGraphic(
                                VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT,
                                (VkPrimitiveTopology)topologyNdx, RESET_TYPE_NORMAL, copyType[copyTypeIdx], query64Bits,
                                true, dstOffset),
                            sixRepeats));
                    secondaryInheritedResetBeforeCopy->addChild(
                        new QueryPoolGraphicStatisticsTest<VertexShaderSecondaryInheritedTestInstance>(
                            m_testCtx, prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx],
                            GraphicBasicTestInstance::ParametersGraphic(
                                VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT,
                                (VkPrimitiveTopology)topologyNdx, RESET_TYPE_BEFORE_COPY, copyType[copyTypeIdx],
                                query64Bits, false, dstOffset),
                            sixRepeats));
                    if (copyType[copyTypeIdx] == COPY_TYPE_CMD)
                        secondaryInheritedResetAfterCopy->addChild(
                            new QueryPoolGraphicStatisticsTest<VertexShaderSecondaryInheritedTestInstance>(
                                m_testCtx, prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx],
                                GraphicBasicTestInstance::ParametersGraphic(
                                    VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT,
                                    (VkPrimitiveTopology)topologyNdx, RESET_TYPE_AFTER_COPY, copyType[copyTypeIdx],
                                    query64Bits, false, dstOffset),
                                sixRepeats));
                }
            }
        }

        // Test VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT with tessellation.
        {
            const VkPrimitiveTopology topologyIdx = VK_PRIMITIVE_TOPOLOGY_PATCH_LIST;
            const uint32_t testNumPrimitives      = 3u;
            const uint32_t lastTessPatchSize      = kMaxTessellationPatchSize - 4;

            for (uint32_t copyTypeIdx = 0; copyTypeIdx < DE_LENGTH_OF_ARRAY(copyType); copyTypeIdx++)
            {
                for (uint32_t i = 0; i < 4; ++i)
                {
                    bool query64Bits   = (i & 1);
                    bool dstOffset     = (i & 2);
                    std::string prefix = bitPrefix(query64Bits, dstOffset);

                    // It makes no sense to use dstOffset with vkGetQueryPoolResults()
                    if (copyType[copyTypeIdx] == COPY_TYPE_GET && dstOffset)
                        continue;

                    for (uint32_t primitiveCnt = 1; primitiveCnt <= testNumPrimitives; primitiveCnt++)
                    {
                        for (uint32_t tessPatchSize = 1, patchSizeCnt = 1; tessPatchSize < kMaxTessellationPatchSize;
                             tessPatchSize = patchSizeCnt * 4, patchSizeCnt++)
                        {
                            const std::string patchPrimitiveCombo =
                                "_v" + std::to_string(tessPatchSize) + "_p" + std::to_string(primitiveCnt);

                            // Tests with no attachments for only primary command to reduce # of test cases.
                            primary->addChild(new QueryPoolGraphicStatisticsTest<TessellationShaderTestInstance>(
                                m_testCtx,
                                prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyIdx] + patchPrimitiveCombo +
                                    "_with_no_color_attachments",
                                GraphicBasicTestInstance::ParametersGraphic(
                                    VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT, topologyIdx,
                                    RESET_TYPE_NORMAL, copyType[copyTypeIdx], query64Bits, false, dstOffset, CLEAR_NOOP,
                                    true, STRIDE_TYPE_VALID, true, tessPatchSize, primitiveCnt),
                                sixRepeats));

                            // Following tests will only run for patch size 28 and primitive count 3
                            if ((primitiveCnt < testNumPrimitives) || (tessPatchSize < lastTessPatchSize))
                                continue;

                            primaryHostQueryReset->addChild(
                                new QueryPoolGraphicStatisticsTest<TessellationShaderTestInstance>(
                                    m_testCtx,
                                    prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyIdx] +
                                        patchPrimitiveCombo + "_with_no_color_attachments",
                                    GraphicBasicTestInstance::ParametersGraphic(
                                        VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT, topologyIdx,
                                        RESET_TYPE_HOST, copyType[copyTypeIdx], query64Bits, false, dstOffset,
                                        CLEAR_NOOP, true, STRIDE_TYPE_VALID, true, tessPatchSize, primitiveCnt),
                                    sixRepeats));

                            primaryResetBeforeCopy->addChild(
                                new QueryPoolGraphicStatisticsTest<TessellationShaderTestInstance>(
                                    m_testCtx,
                                    prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyIdx] +
                                        patchPrimitiveCombo + "_with_no_color_attachments",
                                    GraphicBasicTestInstance::ParametersGraphic(
                                        VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT, topologyIdx,
                                        RESET_TYPE_BEFORE_COPY, copyType[copyTypeIdx], query64Bits, false, dstOffset,
                                        CLEAR_NOOP, true, STRIDE_TYPE_VALID, true, tessPatchSize, primitiveCnt),
                                    sixRepeats));

                            if (copyType[copyTypeIdx] == COPY_TYPE_CMD)
                                primaryResetAfterCopy->addChild(
                                    new QueryPoolGraphicStatisticsTest<TessellationShaderTestInstance>(
                                        m_testCtx,
                                        prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyIdx] +
                                            patchPrimitiveCombo + "_with_no_color_attachments",
                                        GraphicBasicTestInstance::ParametersGraphic(
                                            VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT, topologyIdx,
                                            RESET_TYPE_AFTER_COPY, copyType[copyTypeIdx], query64Bits, false, dstOffset,
                                            CLEAR_NOOP, true, STRIDE_TYPE_VALID, true, tessPatchSize, primitiveCnt),
                                        sixRepeats));

                            /* Tests for clear operation within a statistics query activated.
                             * Nothing for secondary_inherited cases can be done since it violates the specification.
                             *
                             * The query shouldn't count internal driver operations relevant to the clear operations.
                             */
                            const ClearOperation clearOp[] = {CLEAR_NOOP, CLEAR_COLOR, CLEAR_DEPTH};
                            const char *const clearOpStr[] = {"", "_clear_color", "_clear_depth"};

                            for (int clearOpIdx = 0; clearOpIdx < DE_LENGTH_OF_ARRAY(clearOp); ++clearOpIdx)
                            {
                                primary->addChild(new QueryPoolGraphicStatisticsTest<TessellationShaderTestInstance>(
                                    m_testCtx,
                                    prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyIdx] +
                                        patchPrimitiveCombo + clearOpStr[clearOpIdx],
                                    GraphicBasicTestInstance::ParametersGraphic(
                                        VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT,
                                        (VkPrimitiveTopology)topologyIdx, RESET_TYPE_NORMAL, copyType[copyTypeIdx],
                                        query64Bits, false, dstOffset, clearOp[clearOpIdx], false, STRIDE_TYPE_VALID,
                                        true, tessPatchSize, primitiveCnt),
                                    sixRepeats));

                                secondary->addChild(
                                    new QueryPoolGraphicStatisticsTest<TessellationShaderSecondrayTestInstance>(
                                        m_testCtx,
                                        prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyIdx] +
                                            patchPrimitiveCombo + clearOpStr[clearOpIdx],
                                        GraphicBasicTestInstance::ParametersGraphic(
                                            VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT,
                                            (VkPrimitiveTopology)topologyIdx, RESET_TYPE_NORMAL, copyType[copyTypeIdx],
                                            query64Bits, false, dstOffset, clearOp[clearOpIdx], false,
                                            STRIDE_TYPE_VALID, true, tessPatchSize, primitiveCnt),
                                        sixRepeats));

                                primaryHostQueryReset->addChild(
                                    new QueryPoolGraphicStatisticsTest<TessellationShaderTestInstance>(
                                        m_testCtx,
                                        prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyIdx] +
                                            patchPrimitiveCombo + clearOpStr[clearOpIdx],
                                        GraphicBasicTestInstance::ParametersGraphic(
                                            VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT,
                                            (VkPrimitiveTopology)topologyIdx, RESET_TYPE_HOST, copyType[copyTypeIdx],
                                            query64Bits, false, dstOffset, clearOp[clearOpIdx], false,
                                            STRIDE_TYPE_VALID, true, tessPatchSize, primitiveCnt),
                                        sixRepeats));

                                secondaryHostQueryReset->addChild(
                                    new QueryPoolGraphicStatisticsTest<TessellationShaderSecondrayTestInstance>(
                                        m_testCtx,
                                        prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyIdx] +
                                            patchPrimitiveCombo + clearOpStr[clearOpIdx],
                                        GraphicBasicTestInstance::ParametersGraphic(
                                            VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT,
                                            (VkPrimitiveTopology)topologyIdx, RESET_TYPE_HOST, copyType[copyTypeIdx],
                                            query64Bits, false, dstOffset, clearOp[clearOpIdx], false,
                                            STRIDE_TYPE_VALID, true, tessPatchSize, primitiveCnt),
                                        sixRepeats));

                                primaryResetBeforeCopy->addChild(
                                    new QueryPoolGraphicStatisticsTest<TessellationShaderTestInstance>(
                                        m_testCtx,
                                        prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyIdx] +
                                            patchPrimitiveCombo + clearOpStr[clearOpIdx],
                                        GraphicBasicTestInstance::ParametersGraphic(
                                            VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT,
                                            (VkPrimitiveTopology)topologyIdx, RESET_TYPE_BEFORE_COPY,
                                            copyType[copyTypeIdx], query64Bits, false, dstOffset, clearOp[clearOpIdx],
                                            false, STRIDE_TYPE_VALID, true, tessPatchSize, primitiveCnt),
                                        sixRepeats));

                                secondaryResetBeforeCopy->addChild(
                                    new QueryPoolGraphicStatisticsTest<TessellationShaderSecondrayTestInstance>(
                                        m_testCtx,
                                        prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyIdx] +
                                            patchPrimitiveCombo + clearOpStr[clearOpIdx],
                                        GraphicBasicTestInstance::ParametersGraphic(
                                            VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT,
                                            (VkPrimitiveTopology)topologyIdx, RESET_TYPE_BEFORE_COPY,
                                            copyType[copyTypeIdx], query64Bits, false, dstOffset, clearOp[clearOpIdx],
                                            false, STRIDE_TYPE_VALID, true, tessPatchSize, primitiveCnt),
                                        sixRepeats));

                                if (copyType[copyTypeIdx] == COPY_TYPE_CMD)
                                {
                                    primaryResetAfterCopy->addChild(
                                        new QueryPoolGraphicStatisticsTest<TessellationShaderTestInstance>(
                                            m_testCtx,
                                            prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyIdx] +
                                                patchPrimitiveCombo + clearOpStr[clearOpIdx],
                                            GraphicBasicTestInstance::ParametersGraphic(
                                                VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT,
                                                (VkPrimitiveTopology)topologyIdx, RESET_TYPE_AFTER_COPY,
                                                copyType[copyTypeIdx], query64Bits, false, dstOffset,
                                                clearOp[clearOpIdx], false, STRIDE_TYPE_VALID, true, tessPatchSize,
                                                primitiveCnt),
                                            sixRepeats));

                                    secondaryResetAfterCopy->addChild(
                                        new QueryPoolGraphicStatisticsTest<TessellationShaderSecondrayTestInstance>(
                                            m_testCtx,
                                            prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyIdx] +
                                                patchPrimitiveCombo + clearOpStr[clearOpIdx],
                                            GraphicBasicTestInstance::ParametersGraphic(
                                                VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT,
                                                (VkPrimitiveTopology)topologyIdx, RESET_TYPE_AFTER_COPY,
                                                copyType[copyTypeIdx], query64Bits, false, dstOffset,
                                                clearOp[clearOpIdx], false, STRIDE_TYPE_VALID, true, tessPatchSize,
                                                primitiveCnt),
                                            sixRepeats));
                                }
                            }

                            secondaryInherited->addChild(
                                new QueryPoolGraphicStatisticsTest<TessellationShaderSecondrayInheritedTestInstance>(
                                    m_testCtx,
                                    prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyIdx] +
                                        patchPrimitiveCombo,
                                    GraphicBasicTestInstance::ParametersGraphic(
                                        VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT,
                                        (VkPrimitiveTopology)topologyIdx, RESET_TYPE_NORMAL, copyType[copyTypeIdx],
                                        query64Bits, false, dstOffset, CLEAR_NOOP, false, STRIDE_TYPE_VALID, true,
                                        tessPatchSize, primitiveCnt),
                                    sixRepeats));

                            secondaryInheritedHostQueryReset->addChild(
                                new QueryPoolGraphicStatisticsTest<TessellationShaderSecondrayInheritedTestInstance>(
                                    m_testCtx,
                                    prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyIdx] +
                                        patchPrimitiveCombo,
                                    GraphicBasicTestInstance::ParametersGraphic(
                                        VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT,
                                        (VkPrimitiveTopology)topologyIdx, RESET_TYPE_HOST, copyType[copyTypeIdx],
                                        query64Bits, false, dstOffset, CLEAR_NOOP, false, STRIDE_TYPE_VALID, true,
                                        tessPatchSize, primitiveCnt),
                                    sixRepeats));

                            secondaryInheritedResetBeforeCopy->addChild(
                                new QueryPoolGraphicStatisticsTest<TessellationShaderSecondrayInheritedTestInstance>(
                                    m_testCtx,
                                    prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyIdx] +
                                        patchPrimitiveCombo,
                                    GraphicBasicTestInstance::ParametersGraphic(
                                        VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT,
                                        (VkPrimitiveTopology)topologyIdx, RESET_TYPE_BEFORE_COPY, copyType[copyTypeIdx],
                                        query64Bits, false, dstOffset, CLEAR_NOOP, false, STRIDE_TYPE_VALID, true,
                                        tessPatchSize, primitiveCnt),
                                    sixRepeats));

                            if (copyType[copyTypeIdx] == COPY_TYPE_CMD)
                                secondaryInheritedResetAfterCopy->addChild(
                                    new QueryPoolGraphicStatisticsTest<
                                        TessellationShaderSecondrayInheritedTestInstance>(
                                        m_testCtx,
                                        prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyIdx] +
                                            patchPrimitiveCombo,
                                        GraphicBasicTestInstance::ParametersGraphic(
                                            VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT,
                                            (VkPrimitiveTopology)topologyIdx, RESET_TYPE_AFTER_COPY,
                                            copyType[copyTypeIdx], query64Bits, false, dstOffset, CLEAR_NOOP, false,
                                            STRIDE_TYPE_VALID, true, tessPatchSize, primitiveCnt),
                                        sixRepeats));
                        }
                    }
                }
            }
        }

        inputAssemblyPrimitives->addChild(primary.release());
        inputAssemblyPrimitives->addChild(secondary.release());
        inputAssemblyPrimitives->addChild(secondaryInherited.release());

        inputAssemblyPrimitivesVertexOnly->addChild(primaryVertexOnly.release());
        inputAssemblyPrimitivesVertexOnly->addChild(secondaryVertexOnly.release());
        inputAssemblyPrimitivesVertexOnly->addChild(secondaryInheritedVertexOnly.release());

        inputAssemblyPrimitivesHostQueryReset->addChild(primaryHostQueryReset.release());
        inputAssemblyPrimitivesHostQueryReset->addChild(secondaryHostQueryReset.release());
        inputAssemblyPrimitivesHostQueryReset->addChild(secondaryInheritedHostQueryReset.release());

        inputAssemblyPrimitivesResetBeforeCopy->addChild(primaryResetBeforeCopy.release());
        inputAssemblyPrimitivesResetBeforeCopy->addChild(secondaryResetBeforeCopy.release());
        inputAssemblyPrimitivesResetBeforeCopy->addChild(secondaryInheritedResetBeforeCopy.release());

        inputAssemblyPrimitivesResetAfterCopy->addChild(primaryResetAfterCopy.release());
        inputAssemblyPrimitivesResetAfterCopy->addChild(secondaryResetAfterCopy.release());
        inputAssemblyPrimitivesResetAfterCopy->addChild(secondaryInheritedResetAfterCopy.release());
    }

    //VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT
    {
        de::MovePtr<TestCaseGroup> primary(new TestCaseGroup(m_testCtx, "primary"));
        de::MovePtr<TestCaseGroup> secondary(new TestCaseGroup(m_testCtx, "secondary"));
        de::MovePtr<TestCaseGroup> secondaryInherited(new TestCaseGroup(m_testCtx, "secondary_inherited"));

        de::MovePtr<TestCaseGroup> primaryVertexOnly(new TestCaseGroup(m_testCtx, "primary"));
        de::MovePtr<TestCaseGroup> secondaryVertexOnly(new TestCaseGroup(m_testCtx, "secondary"));
        de::MovePtr<TestCaseGroup> secondaryInheritedVertexOnly(new TestCaseGroup(m_testCtx, "secondary_inherited"));

        de::MovePtr<TestCaseGroup> primaryHostQueryReset(new TestCaseGroup(m_testCtx, "primary"));
        de::MovePtr<TestCaseGroup> secondaryHostQueryReset(new TestCaseGroup(m_testCtx, "secondary"));
        de::MovePtr<TestCaseGroup> secondaryInheritedHostQueryReset(
            new TestCaseGroup(m_testCtx, "secondary_inherited"));

        de::MovePtr<TestCaseGroup> primaryResetBeforeCopy(new TestCaseGroup(m_testCtx, "primary"));
        de::MovePtr<TestCaseGroup> secondaryResetBeforeCopy(new TestCaseGroup(m_testCtx, "secondary"));
        de::MovePtr<TestCaseGroup> secondaryInheritedResetBeforeCopy(
            new TestCaseGroup(m_testCtx, "secondary_inherited"));

        de::MovePtr<TestCaseGroup> primaryResetAfterCopy(new TestCaseGroup(m_testCtx, "primary"));
        de::MovePtr<TestCaseGroup> secondaryResetAfterCopy(new TestCaseGroup(m_testCtx, "secondary"));
        de::MovePtr<TestCaseGroup> secondaryInheritedResetAfterCopy(
            new TestCaseGroup(m_testCtx, "secondary_inherited"));

        for (uint32_t copyTypeIdx = 0; copyTypeIdx < DE_LENGTH_OF_ARRAY(copyType); copyTypeIdx++)
        {
            for (int topologyNdx = VK_PRIMITIVE_TOPOLOGY_POINT_LIST; topologyNdx < VK_PRIMITIVE_TOPOLOGY_PATCH_LIST;
                 ++topologyNdx)
            {
                for (uint32_t i = 0; i < 4; ++i)
                {
                    bool query64Bits   = (i & 1);
                    bool dstOffset     = (i & 2);
                    std::string prefix = bitPrefix(query64Bits, dstOffset);

                    // It makes no sense to use dstOffset with vkGetQueryPoolResults()
                    if (copyType[copyTypeIdx] == COPY_TYPE_GET && dstOffset)
                        continue;

                    // Tests with no attachments for only primary command to reduce # of test cases.
                    primary->addChild(new QueryPoolGraphicStatisticsTest<VertexShaderTestInstance>(
                        m_testCtx,
                        prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] + "_with_no_color_attachments",
                        GraphicBasicTestInstance::ParametersGraphic(
                            VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT, (VkPrimitiveTopology)topologyNdx,
                            RESET_TYPE_NORMAL, copyType[copyTypeIdx], query64Bits, false, dstOffset, CLEAR_NOOP, true),
                        sixRepeats));

                    primaryVertexOnly->addChild(new QueryPoolGraphicStatisticsTest<VertexShaderTestInstance>(
                        m_testCtx,
                        prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] + "_with_no_color_attachments",
                        GraphicBasicTestInstance::ParametersGraphic(
                            VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT, (VkPrimitiveTopology)topologyNdx,
                            RESET_TYPE_NORMAL, copyType[copyTypeIdx], query64Bits, true, dstOffset, CLEAR_NOOP, true),
                        sixRepeats));

                    primaryHostQueryReset->addChild(new QueryPoolGraphicStatisticsTest<VertexShaderTestInstance>(
                        m_testCtx,
                        prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] + "_with_no_color_attachments",
                        GraphicBasicTestInstance::ParametersGraphic(
                            VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT, (VkPrimitiveTopology)topologyNdx,
                            RESET_TYPE_HOST, copyType[copyTypeIdx], query64Bits, false, dstOffset, CLEAR_NOOP, true),
                        sixRepeats));

                    primaryResetBeforeCopy->addChild(new QueryPoolGraphicStatisticsTest<VertexShaderTestInstance>(
                        m_testCtx,
                        prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] + "_with_no_color_attachments",
                        GraphicBasicTestInstance::ParametersGraphic(
                            VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT, (VkPrimitiveTopology)topologyNdx,
                            RESET_TYPE_BEFORE_COPY, copyType[copyTypeIdx], query64Bits, false, dstOffset, CLEAR_NOOP,
                            true),
                        sixRepeats));

                    if (copyType[copyTypeIdx] == COPY_TYPE_CMD)
                        primaryResetAfterCopy->addChild(new QueryPoolGraphicStatisticsTest<VertexShaderTestInstance>(
                            m_testCtx,
                            prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] +
                                "_with_no_color_attachments",
                            GraphicBasicTestInstance::ParametersGraphic(
                                VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT,
                                (VkPrimitiveTopology)topologyNdx, RESET_TYPE_AFTER_COPY, copyType[copyTypeIdx],
                                query64Bits, false, dstOffset, CLEAR_NOOP, true),
                            sixRepeats));

                    /* Tests for clear operation within a statistics query activated.
                     * Nothing for secondary_inherited cases can be done since it violates the specification.
                     *
                     * The query shouldn't count internal driver operations relevant to the clear operations.
                     */
                    const ClearOperation clearOp[] = {CLEAR_NOOP, CLEAR_COLOR, CLEAR_DEPTH};
                    const char *const clearOpStr[] = {"", "_clear_color", "_clear_depth"};

                    for (int clearOpIdx = 0; clearOpIdx < DE_LENGTH_OF_ARRAY(clearOp); ++clearOpIdx)
                    {
                        primary->addChild(new QueryPoolGraphicStatisticsTest<VertexShaderTestInstance>(
                            m_testCtx,
                            prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] + clearOpStr[clearOpIdx],
                            GraphicBasicTestInstance::ParametersGraphic(
                                VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT,
                                (VkPrimitiveTopology)topologyNdx, RESET_TYPE_NORMAL, copyType[copyTypeIdx], query64Bits,
                                false, dstOffset, clearOp[clearOpIdx]),
                            sixRepeats));
                        secondary->addChild(new QueryPoolGraphicStatisticsTest<VertexShaderSecondaryTestInstance>(
                            m_testCtx,
                            prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] + clearOpStr[clearOpIdx],
                            GraphicBasicTestInstance::ParametersGraphic(
                                VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT,
                                (VkPrimitiveTopology)topologyNdx, RESET_TYPE_NORMAL, copyType[copyTypeIdx], query64Bits,
                                false, dstOffset, clearOp[clearOpIdx]),
                            sixRepeats));

                        primaryVertexOnly->addChild(new QueryPoolGraphicStatisticsTest<VertexShaderTestInstance>(
                            m_testCtx,
                            prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] + clearOpStr[clearOpIdx],
                            GraphicBasicTestInstance::ParametersGraphic(
                                VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT,
                                (VkPrimitiveTopology)topologyNdx, RESET_TYPE_NORMAL, copyType[copyTypeIdx], query64Bits,
                                true, dstOffset, clearOp[clearOpIdx]),
                            sixRepeats));
                        secondaryVertexOnly->addChild(
                            new QueryPoolGraphicStatisticsTest<VertexShaderSecondaryTestInstance>(
                                m_testCtx,
                                prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] + clearOpStr[clearOpIdx],
                                GraphicBasicTestInstance::ParametersGraphic(
                                    VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT,
                                    (VkPrimitiveTopology)topologyNdx, RESET_TYPE_NORMAL, copyType[copyTypeIdx],
                                    query64Bits, true, dstOffset, clearOp[clearOpIdx]),
                                sixRepeats));

                        primaryHostQueryReset->addChild(new QueryPoolGraphicStatisticsTest<VertexShaderTestInstance>(
                            m_testCtx,
                            prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] + clearOpStr[clearOpIdx],
                            GraphicBasicTestInstance::ParametersGraphic(
                                VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT,
                                (VkPrimitiveTopology)topologyNdx, RESET_TYPE_HOST, copyType[copyTypeIdx], query64Bits,
                                false, dstOffset, clearOp[clearOpIdx]),
                            sixRepeats));
                        secondaryHostQueryReset->addChild(
                            new QueryPoolGraphicStatisticsTest<VertexShaderSecondaryTestInstance>(
                                m_testCtx,
                                prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] + clearOpStr[clearOpIdx],
                                GraphicBasicTestInstance::ParametersGraphic(
                                    VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT,
                                    (VkPrimitiveTopology)topologyNdx, RESET_TYPE_HOST, copyType[copyTypeIdx],
                                    query64Bits, false, dstOffset, clearOp[clearOpIdx]),
                                sixRepeats));

                        primaryResetBeforeCopy->addChild(new QueryPoolGraphicStatisticsTest<VertexShaderTestInstance>(
                            m_testCtx,
                            prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] + clearOpStr[clearOpIdx],
                            GraphicBasicTestInstance::ParametersGraphic(
                                VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT,
                                (VkPrimitiveTopology)topologyNdx, RESET_TYPE_BEFORE_COPY, copyType[copyTypeIdx],
                                query64Bits, false, dstOffset, clearOp[clearOpIdx]),
                            sixRepeats));
                        secondaryResetBeforeCopy->addChild(
                            new QueryPoolGraphicStatisticsTest<VertexShaderSecondaryTestInstance>(
                                m_testCtx,
                                prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] + clearOpStr[clearOpIdx],
                                GraphicBasicTestInstance::ParametersGraphic(
                                    VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT,
                                    (VkPrimitiveTopology)topologyNdx, RESET_TYPE_BEFORE_COPY, copyType[copyTypeIdx],
                                    query64Bits, false, dstOffset, clearOp[clearOpIdx]),
                                sixRepeats));

                        if (copyType[copyTypeIdx] == COPY_TYPE_CMD)
                        {
                            primaryResetAfterCopy->addChild(
                                new QueryPoolGraphicStatisticsTest<VertexShaderTestInstance>(
                                    m_testCtx,
                                    prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] +
                                        clearOpStr[clearOpIdx],
                                    GraphicBasicTestInstance::ParametersGraphic(
                                        VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT,
                                        (VkPrimitiveTopology)topologyNdx, RESET_TYPE_AFTER_COPY, copyType[copyTypeIdx],
                                        query64Bits, false, dstOffset, clearOp[clearOpIdx]),
                                    sixRepeats));
                            secondaryResetAfterCopy->addChild(
                                new QueryPoolGraphicStatisticsTest<VertexShaderSecondaryTestInstance>(
                                    m_testCtx,
                                    prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] +
                                        clearOpStr[clearOpIdx],
                                    GraphicBasicTestInstance::ParametersGraphic(
                                        VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT,
                                        (VkPrimitiveTopology)topologyNdx, RESET_TYPE_AFTER_COPY, copyType[copyTypeIdx],
                                        query64Bits, false, dstOffset, clearOp[clearOpIdx]),
                                    sixRepeats));
                        }
                    }

                    secondaryInherited->addChild(
                        new QueryPoolGraphicStatisticsTest<VertexShaderSecondaryInheritedTestInstance>(
                            m_testCtx, prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx],
                            GraphicBasicTestInstance::ParametersGraphic(
                                VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT,
                                (VkPrimitiveTopology)topologyNdx, RESET_TYPE_NORMAL, copyType[copyTypeIdx], query64Bits,
                                false, dstOffset),
                            sixRepeats));
                    secondaryInheritedVertexOnly->addChild(
                        new QueryPoolGraphicStatisticsTest<VertexShaderSecondaryInheritedTestInstance>(
                            m_testCtx, prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx],
                            GraphicBasicTestInstance::ParametersGraphic(
                                VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT,
                                (VkPrimitiveTopology)topologyNdx, RESET_TYPE_NORMAL, copyType[copyTypeIdx], query64Bits,
                                true, dstOffset),
                            sixRepeats));
                    secondaryInheritedHostQueryReset->addChild(
                        new QueryPoolGraphicStatisticsTest<VertexShaderSecondaryInheritedTestInstance>(
                            m_testCtx, prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx],
                            GraphicBasicTestInstance::ParametersGraphic(
                                VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT,
                                (VkPrimitiveTopology)topologyNdx, RESET_TYPE_HOST, copyType[copyTypeIdx], query64Bits,
                                false, dstOffset),
                            sixRepeats));
                    secondaryInheritedResetBeforeCopy->addChild(
                        new QueryPoolGraphicStatisticsTest<VertexShaderSecondaryInheritedTestInstance>(
                            m_testCtx, prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx],
                            GraphicBasicTestInstance::ParametersGraphic(
                                VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT,
                                (VkPrimitiveTopology)topologyNdx, RESET_TYPE_BEFORE_COPY, copyType[copyTypeIdx],
                                query64Bits, false, dstOffset),
                            sixRepeats));
                    if (copyType[copyTypeIdx] == COPY_TYPE_CMD)
                        secondaryInheritedResetAfterCopy->addChild(
                            new QueryPoolGraphicStatisticsTest<VertexShaderSecondaryInheritedTestInstance>(
                                m_testCtx, prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx],
                                GraphicBasicTestInstance::ParametersGraphic(
                                    VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT,
                                    (VkPrimitiveTopology)topologyNdx, RESET_TYPE_AFTER_COPY, copyType[copyTypeIdx],
                                    query64Bits, false, dstOffset),
                                sixRepeats));
                }
            }
        }

        vertexShaderInvocations->addChild(primary.release());
        vertexShaderInvocations->addChild(secondary.release());
        vertexShaderInvocations->addChild(secondaryInherited.release());

        vertexShaderInvocationsVertexOnly->addChild(primaryVertexOnly.release());
        vertexShaderInvocationsVertexOnly->addChild(secondaryVertexOnly.release());
        vertexShaderInvocationsVertexOnly->addChild(secondaryInheritedVertexOnly.release());

        vertexShaderInvocationsHostQueryReset->addChild(primaryHostQueryReset.release());
        vertexShaderInvocationsHostQueryReset->addChild(secondaryHostQueryReset.release());
        vertexShaderInvocationsHostQueryReset->addChild(secondaryInheritedHostQueryReset.release());

        vertexShaderInvocationsResetBeforeCopy->addChild(primaryResetBeforeCopy.release());
        vertexShaderInvocationsResetBeforeCopy->addChild(secondaryResetBeforeCopy.release());
        vertexShaderInvocationsResetBeforeCopy->addChild(secondaryInheritedResetBeforeCopy.release());

        vertexShaderInvocationsResetAfterCopy->addChild(primaryResetAfterCopy.release());
        vertexShaderInvocationsResetAfterCopy->addChild(secondaryResetAfterCopy.release());
        vertexShaderInvocationsResetAfterCopy->addChild(secondaryInheritedResetAfterCopy.release());
    }

    //VK_QUERY_PIPELINE_STATISTIC_FRAGMENT_SHADER_INVOCATIONS_BIT
    {
        de::MovePtr<TestCaseGroup> primary(new TestCaseGroup(m_testCtx, "primary"));
        de::MovePtr<TestCaseGroup> secondary(new TestCaseGroup(m_testCtx, "secondary"));
        de::MovePtr<TestCaseGroup> secondaryInherited(new TestCaseGroup(m_testCtx, "secondary_inherited"));

        de::MovePtr<TestCaseGroup> primaryHostQueryReset(new TestCaseGroup(m_testCtx, "primary"));
        de::MovePtr<TestCaseGroup> secondaryHostQueryReset(new TestCaseGroup(m_testCtx, "secondary"));
        de::MovePtr<TestCaseGroup> secondaryInheritedHostQueryReset(
            new TestCaseGroup(m_testCtx, "secondary_inherited"));

        de::MovePtr<TestCaseGroup> primaryResetBeforeCopy(new TestCaseGroup(m_testCtx, "primary"));
        de::MovePtr<TestCaseGroup> secondaryResetBeforeCopy(new TestCaseGroup(m_testCtx, "secondary"));
        de::MovePtr<TestCaseGroup> secondaryInheritedResetBeforeCopy(
            new TestCaseGroup(m_testCtx, "secondary_inherited"));

        de::MovePtr<TestCaseGroup> primaryResetAfterCopy(new TestCaseGroup(m_testCtx, "primary"));
        de::MovePtr<TestCaseGroup> secondaryResetAfterCopy(new TestCaseGroup(m_testCtx, "secondary"));
        de::MovePtr<TestCaseGroup> secondaryInheritedResetAfterCopy(
            new TestCaseGroup(m_testCtx, "secondary_inherited"));

        for (uint32_t copyTypeIdx = 0; copyTypeIdx < DE_LENGTH_OF_ARRAY(copyType); copyTypeIdx++)
        {
            for (int topologyNdx = VK_PRIMITIVE_TOPOLOGY_POINT_LIST; topologyNdx < VK_PRIMITIVE_TOPOLOGY_PATCH_LIST;
                 ++topologyNdx)
            {
                for (uint32_t i = 0; i < 4; ++i)
                {
                    bool query64Bits   = (i & 1);
                    bool dstOffset     = (i & 2);
                    std::string prefix = bitPrefix(query64Bits, dstOffset);

                    // It makes no sense to use dstOffset with vkGetQueryPoolResults()
                    if (copyType[copyTypeIdx] == COPY_TYPE_GET && dstOffset)
                        continue;

                    // Tests with no attachments do not apply to fragment shader invocations, because they can be skipped:
                    // - No color writes.
                    // - No depth/stencil writes.
                    primary->addChild(new QueryPoolGraphicStatisticsTest<VertexShaderTestInstance>(
                        m_testCtx,
                        prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] + "_with_no_color_attachments",
                        GraphicBasicTestInstance::ParametersGraphic(
                            VK_QUERY_PIPELINE_STATISTIC_FRAGMENT_SHADER_INVOCATIONS_BIT,
                            (VkPrimitiveTopology)topologyNdx, RESET_TYPE_NORMAL, copyType[copyTypeIdx], query64Bits,
                            false, dstOffset, CLEAR_SKIP, true),
                        sixRepeats));

                    primaryHostQueryReset->addChild(new QueryPoolGraphicStatisticsTest<VertexShaderTestInstance>(
                        m_testCtx,
                        prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] + "_with_no_color_attachments",
                        GraphicBasicTestInstance::ParametersGraphic(
                            VK_QUERY_PIPELINE_STATISTIC_FRAGMENT_SHADER_INVOCATIONS_BIT,
                            (VkPrimitiveTopology)topologyNdx, RESET_TYPE_HOST, copyType[copyTypeIdx], query64Bits,
                            false, dstOffset, CLEAR_SKIP, true),
                        sixRepeats));

                    primaryResetBeforeCopy->addChild(new QueryPoolGraphicStatisticsTest<VertexShaderTestInstance>(
                        m_testCtx,
                        prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] + "_with_no_color_attachments",
                        GraphicBasicTestInstance::ParametersGraphic(
                            VK_QUERY_PIPELINE_STATISTIC_FRAGMENT_SHADER_INVOCATIONS_BIT,
                            (VkPrimitiveTopology)topologyNdx, RESET_TYPE_BEFORE_COPY, copyType[copyTypeIdx],
                            query64Bits, false, dstOffset, CLEAR_SKIP, true),
                        sixRepeats));

                    if (copyType[copyTypeIdx] == COPY_TYPE_CMD)
                        primaryResetAfterCopy->addChild(new QueryPoolGraphicStatisticsTest<VertexShaderTestInstance>(
                            m_testCtx,
                            prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] +
                                "_with_no_color_attachments",
                            GraphicBasicTestInstance::ParametersGraphic(
                                VK_QUERY_PIPELINE_STATISTIC_FRAGMENT_SHADER_INVOCATIONS_BIT,
                                (VkPrimitiveTopology)topologyNdx, RESET_TYPE_AFTER_COPY, copyType[copyTypeIdx],
                                query64Bits, false, dstOffset, CLEAR_SKIP, true),
                            sixRepeats));

                    {
                        primary->addChild(new QueryPoolGraphicStatisticsTest<VertexShaderTestInstance>(
                            m_testCtx, prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx],
                            GraphicBasicTestInstance::ParametersGraphic(
                                VK_QUERY_PIPELINE_STATISTIC_FRAGMENT_SHADER_INVOCATIONS_BIT,
                                (VkPrimitiveTopology)topologyNdx, RESET_TYPE_NORMAL, copyType[copyTypeIdx], query64Bits,
                                false, dstOffset, CLEAR_SKIP),
                            sixRepeats));
                        secondary->addChild(new QueryPoolGraphicStatisticsTest<VertexShaderSecondaryTestInstance>(
                            m_testCtx, prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx],
                            GraphicBasicTestInstance::ParametersGraphic(
                                VK_QUERY_PIPELINE_STATISTIC_FRAGMENT_SHADER_INVOCATIONS_BIT,
                                (VkPrimitiveTopology)topologyNdx, RESET_TYPE_NORMAL, copyType[copyTypeIdx], query64Bits,
                                false, dstOffset, CLEAR_SKIP),
                            sixRepeats));

                        primaryHostQueryReset->addChild(new QueryPoolGraphicStatisticsTest<VertexShaderTestInstance>(
                            m_testCtx, prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx],
                            GraphicBasicTestInstance::ParametersGraphic(
                                VK_QUERY_PIPELINE_STATISTIC_FRAGMENT_SHADER_INVOCATIONS_BIT,
                                (VkPrimitiveTopology)topologyNdx, RESET_TYPE_HOST, copyType[copyTypeIdx], query64Bits,
                                false, dstOffset, CLEAR_SKIP),
                            sixRepeats));
                        secondaryHostQueryReset->addChild(
                            new QueryPoolGraphicStatisticsTest<VertexShaderSecondaryTestInstance>(
                                m_testCtx, prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx],
                                GraphicBasicTestInstance::ParametersGraphic(
                                    VK_QUERY_PIPELINE_STATISTIC_FRAGMENT_SHADER_INVOCATIONS_BIT,
                                    (VkPrimitiveTopology)topologyNdx, RESET_TYPE_HOST, copyType[copyTypeIdx],
                                    query64Bits, false, dstOffset, CLEAR_SKIP),
                                sixRepeats));

                        primaryResetBeforeCopy->addChild(new QueryPoolGraphicStatisticsTest<VertexShaderTestInstance>(
                            m_testCtx, prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx],
                            GraphicBasicTestInstance::ParametersGraphic(
                                VK_QUERY_PIPELINE_STATISTIC_FRAGMENT_SHADER_INVOCATIONS_BIT,
                                (VkPrimitiveTopology)topologyNdx, RESET_TYPE_BEFORE_COPY, copyType[copyTypeIdx],
                                query64Bits, false, dstOffset, CLEAR_SKIP),
                            sixRepeats));
                        secondaryResetBeforeCopy->addChild(
                            new QueryPoolGraphicStatisticsTest<VertexShaderSecondaryTestInstance>(
                                m_testCtx, prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx],
                                GraphicBasicTestInstance::ParametersGraphic(
                                    VK_QUERY_PIPELINE_STATISTIC_FRAGMENT_SHADER_INVOCATIONS_BIT,
                                    (VkPrimitiveTopology)topologyNdx, RESET_TYPE_BEFORE_COPY, copyType[copyTypeIdx],
                                    query64Bits, false, dstOffset, CLEAR_SKIP),
                                sixRepeats));

                        if (copyType[copyTypeIdx] == COPY_TYPE_CMD)
                        {
                            primaryResetAfterCopy->addChild(
                                new QueryPoolGraphicStatisticsTest<VertexShaderTestInstance>(
                                    m_testCtx, prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx],
                                    GraphicBasicTestInstance::ParametersGraphic(
                                        VK_QUERY_PIPELINE_STATISTIC_FRAGMENT_SHADER_INVOCATIONS_BIT,
                                        (VkPrimitiveTopology)topologyNdx, RESET_TYPE_AFTER_COPY, copyType[copyTypeIdx],
                                        query64Bits, false, dstOffset, CLEAR_SKIP),
                                    sixRepeats));
                            secondaryResetAfterCopy->addChild(
                                new QueryPoolGraphicStatisticsTest<VertexShaderSecondaryTestInstance>(
                                    m_testCtx, prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx],
                                    GraphicBasicTestInstance::ParametersGraphic(
                                        VK_QUERY_PIPELINE_STATISTIC_FRAGMENT_SHADER_INVOCATIONS_BIT,
                                        (VkPrimitiveTopology)topologyNdx, RESET_TYPE_AFTER_COPY, copyType[copyTypeIdx],
                                        query64Bits, false, dstOffset, CLEAR_SKIP),
                                    sixRepeats));
                        }
                    }

                    secondaryInherited->addChild(
                        new QueryPoolGraphicStatisticsTest<VertexShaderSecondaryInheritedTestInstance>(
                            m_testCtx, prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx],
                            GraphicBasicTestInstance::ParametersGraphic(
                                VK_QUERY_PIPELINE_STATISTIC_FRAGMENT_SHADER_INVOCATIONS_BIT,
                                (VkPrimitiveTopology)topologyNdx, RESET_TYPE_NORMAL, copyType[copyTypeIdx], query64Bits,
                                false, dstOffset, CLEAR_SKIP),
                            sixRepeats));
                    secondaryInheritedHostQueryReset->addChild(
                        new QueryPoolGraphicStatisticsTest<VertexShaderSecondaryInheritedTestInstance>(
                            m_testCtx, prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx],
                            GraphicBasicTestInstance::ParametersGraphic(
                                VK_QUERY_PIPELINE_STATISTIC_FRAGMENT_SHADER_INVOCATIONS_BIT,
                                (VkPrimitiveTopology)topologyNdx, RESET_TYPE_HOST, copyType[copyTypeIdx], query64Bits,
                                false, dstOffset, CLEAR_SKIP),
                            sixRepeats));
                    secondaryInheritedResetBeforeCopy->addChild(
                        new QueryPoolGraphicStatisticsTest<VertexShaderSecondaryInheritedTestInstance>(
                            m_testCtx, prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx],
                            GraphicBasicTestInstance::ParametersGraphic(
                                VK_QUERY_PIPELINE_STATISTIC_FRAGMENT_SHADER_INVOCATIONS_BIT,
                                (VkPrimitiveTopology)topologyNdx, RESET_TYPE_BEFORE_COPY, copyType[copyTypeIdx],
                                query64Bits, false, dstOffset, CLEAR_SKIP),
                            sixRepeats));
                    if (copyType[copyTypeIdx] == COPY_TYPE_CMD)
                        secondaryInheritedResetAfterCopy->addChild(
                            new QueryPoolGraphicStatisticsTest<VertexShaderSecondaryInheritedTestInstance>(
                                m_testCtx, prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx],
                                GraphicBasicTestInstance::ParametersGraphic(
                                    VK_QUERY_PIPELINE_STATISTIC_FRAGMENT_SHADER_INVOCATIONS_BIT,
                                    (VkPrimitiveTopology)topologyNdx, RESET_TYPE_AFTER_COPY, copyType[copyTypeIdx],
                                    query64Bits, false, dstOffset, CLEAR_SKIP),
                                sixRepeats));
                }
            }
        }

        fragmentShaderInvocations->addChild(primary.release());
        fragmentShaderInvocations->addChild(secondary.release());
        fragmentShaderInvocations->addChild(secondaryInherited.release());

        fragmentShaderInvocationsHostQueryReset->addChild(primaryHostQueryReset.release());
        fragmentShaderInvocationsHostQueryReset->addChild(secondaryHostQueryReset.release());
        fragmentShaderInvocationsHostQueryReset->addChild(secondaryInheritedHostQueryReset.release());

        fragmentShaderInvocationsResetBeforeCopy->addChild(primaryResetBeforeCopy.release());
        fragmentShaderInvocationsResetBeforeCopy->addChild(secondaryResetBeforeCopy.release());
        fragmentShaderInvocationsResetBeforeCopy->addChild(secondaryInheritedResetBeforeCopy.release());

        fragmentShaderInvocationsResetAfterCopy->addChild(primaryResetAfterCopy.release());
        fragmentShaderInvocationsResetAfterCopy->addChild(secondaryResetAfterCopy.release());
        fragmentShaderInvocationsResetAfterCopy->addChild(secondaryInheritedResetAfterCopy.release());
    }

    //VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_INVOCATIONS_BIT
    {
        de::MovePtr<TestCaseGroup> primary(new TestCaseGroup(m_testCtx, "primary"));
        de::MovePtr<TestCaseGroup> secondary(new TestCaseGroup(m_testCtx, "secondary"));
        de::MovePtr<TestCaseGroup> secondaryInherited(new TestCaseGroup(m_testCtx, "secondary_inherited"));

        de::MovePtr<TestCaseGroup> primaryHostQueryReset(new TestCaseGroup(m_testCtx, "primary"));
        de::MovePtr<TestCaseGroup> secondaryHostQueryReset(new TestCaseGroup(m_testCtx, "secondary"));
        de::MovePtr<TestCaseGroup> secondaryInheritedHostQueryReset(
            new TestCaseGroup(m_testCtx, "secondary_inherited"));

        de::MovePtr<TestCaseGroup> primaryResetBeforeCopy(new TestCaseGroup(m_testCtx, "primary"));
        de::MovePtr<TestCaseGroup> secondaryResetBeforeCopy(new TestCaseGroup(m_testCtx, "secondary"));
        de::MovePtr<TestCaseGroup> secondaryInheritedResetBeforeCopy(
            new TestCaseGroup(m_testCtx, "secondary_inherited"));

        de::MovePtr<TestCaseGroup> primaryResetAfterCopy(new TestCaseGroup(m_testCtx, "primary"));
        de::MovePtr<TestCaseGroup> secondaryResetAfterCopy(new TestCaseGroup(m_testCtx, "secondary"));
        de::MovePtr<TestCaseGroup> secondaryInheritedResetAfterCopy(
            new TestCaseGroup(m_testCtx, "secondary_inherited"));

        for (uint32_t copyTypeIdx = 0; copyTypeIdx < DE_LENGTH_OF_ARRAY(copyType); copyTypeIdx++)
        {
            for (int topologyNdx = VK_PRIMITIVE_TOPOLOGY_POINT_LIST; topologyNdx < VK_PRIMITIVE_TOPOLOGY_PATCH_LIST;
                 ++topologyNdx)
            {
                for (uint32_t i = 0; i < 4; ++i)
                {
                    bool query64Bits   = (i & 1);
                    bool dstOffset     = (i & 2);
                    std::string prefix = bitPrefix(query64Bits, dstOffset);

                    // It makes no sense to use dstOffset with vkGetQueryPoolResults()
                    if (copyType[copyTypeIdx] == COPY_TYPE_GET && dstOffset)
                        continue;

                    // Tests with no attachments for only primary command to reduce # of test cases.
                    primary->addChild(new QueryPoolGraphicStatisticsTest<GeometryShaderTestInstance>(
                        m_testCtx,
                        prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] + "_with_no_color_attachments",
                        GraphicBasicTestInstance::ParametersGraphic(
                            VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_INVOCATIONS_BIT,
                            (VkPrimitiveTopology)topologyNdx, RESET_TYPE_NORMAL, copyType[copyTypeIdx], query64Bits,
                            false, dstOffset, CLEAR_NOOP, true),
                        sixRepeats));

                    primaryHostQueryReset->addChild(new QueryPoolGraphicStatisticsTest<GeometryShaderTestInstance>(
                        m_testCtx,
                        prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] + "_with_no_color_attachments",
                        GraphicBasicTestInstance::ParametersGraphic(
                            VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_INVOCATIONS_BIT,
                            (VkPrimitiveTopology)topologyNdx, RESET_TYPE_HOST, copyType[copyTypeIdx], query64Bits,
                            false, dstOffset, CLEAR_NOOP, true),
                        sixRepeats));

                    primaryResetBeforeCopy->addChild(new QueryPoolGraphicStatisticsTest<GeometryShaderTestInstance>(
                        m_testCtx,
                        prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] + "_with_no_color_attachments",
                        GraphicBasicTestInstance::ParametersGraphic(
                            VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_INVOCATIONS_BIT,
                            (VkPrimitiveTopology)topologyNdx, RESET_TYPE_BEFORE_COPY, copyType[copyTypeIdx],
                            query64Bits, false, dstOffset, CLEAR_NOOP, true),
                        sixRepeats));

                    if (copyType[copyTypeIdx] == COPY_TYPE_CMD)
                        primaryResetAfterCopy->addChild(new QueryPoolGraphicStatisticsTest<GeometryShaderTestInstance>(
                            m_testCtx,
                            prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] +
                                "_with_no_color_attachments",
                            GraphicBasicTestInstance::ParametersGraphic(
                                VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_INVOCATIONS_BIT,
                                (VkPrimitiveTopology)topologyNdx, RESET_TYPE_AFTER_COPY, copyType[copyTypeIdx],
                                query64Bits, false, dstOffset, CLEAR_NOOP, true),
                            sixRepeats));

                    /* Tests for clear operation within a statistics query activated.
                     * Nothing for secondary_inherited cases can be done since it violates the specification.
                     *
                     * The query shouldn't count internal driver operations relevant to the clear operations.
                     */
                    const ClearOperation clearOp[] = {CLEAR_NOOP, CLEAR_COLOR, CLEAR_DEPTH};
                    const char *const clearOpStr[] = {"", "_clear_color", "_clear_depth"};

                    for (int clearOpIdx = 0; clearOpIdx < DE_LENGTH_OF_ARRAY(clearOp); ++clearOpIdx)
                    {
                        primary->addChild(new QueryPoolGraphicStatisticsTest<GeometryShaderTestInstance>(
                            m_testCtx,
                            prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] + clearOpStr[clearOpIdx],
                            GraphicBasicTestInstance::ParametersGraphic(
                                VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_INVOCATIONS_BIT,
                                (VkPrimitiveTopology)topologyNdx, RESET_TYPE_NORMAL, copyType[copyTypeIdx], query64Bits,
                                false, dstOffset, clearOp[clearOpIdx]),
                            sixRepeats));
                        secondary->addChild(new QueryPoolGraphicStatisticsTest<GeometryShaderSecondaryTestInstance>(
                            m_testCtx,
                            prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] + clearOpStr[clearOpIdx],
                            GraphicBasicTestInstance::ParametersGraphic(
                                VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_INVOCATIONS_BIT,
                                (VkPrimitiveTopology)topologyNdx, RESET_TYPE_NORMAL, copyType[copyTypeIdx], query64Bits,
                                false, dstOffset, clearOp[clearOpIdx]),
                            sixRepeats));

                        primaryHostQueryReset->addChild(new QueryPoolGraphicStatisticsTest<GeometryShaderTestInstance>(
                            m_testCtx,
                            prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] + clearOpStr[clearOpIdx],
                            GraphicBasicTestInstance::ParametersGraphic(
                                VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_INVOCATIONS_BIT,
                                (VkPrimitiveTopology)topologyNdx, RESET_TYPE_HOST, copyType[copyTypeIdx], query64Bits,
                                false, dstOffset, clearOp[clearOpIdx]),
                            sixRepeats));
                        secondaryHostQueryReset->addChild(
                            new QueryPoolGraphicStatisticsTest<GeometryShaderSecondaryTestInstance>(
                                m_testCtx,
                                prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] + clearOpStr[clearOpIdx],
                                GraphicBasicTestInstance::ParametersGraphic(
                                    VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_INVOCATIONS_BIT,
                                    (VkPrimitiveTopology)topologyNdx, RESET_TYPE_HOST, copyType[copyTypeIdx],
                                    query64Bits, false, dstOffset, clearOp[clearOpIdx]),
                                sixRepeats));

                        primaryResetBeforeCopy->addChild(new QueryPoolGraphicStatisticsTest<GeometryShaderTestInstance>(
                            m_testCtx,
                            prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] + clearOpStr[clearOpIdx],
                            GraphicBasicTestInstance::ParametersGraphic(
                                VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_INVOCATIONS_BIT,
                                (VkPrimitiveTopology)topologyNdx, RESET_TYPE_BEFORE_COPY, copyType[copyTypeIdx],
                                query64Bits, false, dstOffset, clearOp[clearOpIdx]),
                            sixRepeats));
                        secondaryResetBeforeCopy->addChild(
                            new QueryPoolGraphicStatisticsTest<GeometryShaderSecondaryTestInstance>(
                                m_testCtx,
                                prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] + clearOpStr[clearOpIdx],
                                GraphicBasicTestInstance::ParametersGraphic(
                                    VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_INVOCATIONS_BIT,
                                    (VkPrimitiveTopology)topologyNdx, RESET_TYPE_BEFORE_COPY, copyType[copyTypeIdx],
                                    query64Bits, false, dstOffset, clearOp[clearOpIdx]),
                                sixRepeats));

                        if (copyType[copyTypeIdx] == COPY_TYPE_CMD)
                        {
                            primaryResetAfterCopy->addChild(
                                new QueryPoolGraphicStatisticsTest<GeometryShaderTestInstance>(
                                    m_testCtx,
                                    prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] +
                                        clearOpStr[clearOpIdx],
                                    GraphicBasicTestInstance::ParametersGraphic(
                                        VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_INVOCATIONS_BIT,
                                        (VkPrimitiveTopology)topologyNdx, RESET_TYPE_AFTER_COPY, copyType[copyTypeIdx],
                                        query64Bits, false, dstOffset, clearOp[clearOpIdx]),
                                    sixRepeats));
                            secondaryResetAfterCopy->addChild(
                                new QueryPoolGraphicStatisticsTest<GeometryShaderSecondaryTestInstance>(
                                    m_testCtx,
                                    prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] +
                                        clearOpStr[clearOpIdx],
                                    GraphicBasicTestInstance::ParametersGraphic(
                                        VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_INVOCATIONS_BIT,
                                        (VkPrimitiveTopology)topologyNdx, RESET_TYPE_AFTER_COPY, copyType[copyTypeIdx],
                                        query64Bits, false, dstOffset, clearOp[clearOpIdx]),
                                    sixRepeats));
                        }
                    }

                    secondaryInherited->addChild(
                        new QueryPoolGraphicStatisticsTest<GeometryShaderSecondaryInheritedTestInstance>(
                            m_testCtx, prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx],
                            GraphicBasicTestInstance::ParametersGraphic(
                                VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_INVOCATIONS_BIT,
                                (VkPrimitiveTopology)topologyNdx, RESET_TYPE_NORMAL, copyType[copyTypeIdx], query64Bits,
                                false, dstOffset),
                            sixRepeats));
                    secondaryInheritedHostQueryReset->addChild(
                        new QueryPoolGraphicStatisticsTest<GeometryShaderSecondaryInheritedTestInstance>(
                            m_testCtx, prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx],
                            GraphicBasicTestInstance::ParametersGraphic(
                                VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_INVOCATIONS_BIT,
                                (VkPrimitiveTopology)topologyNdx, RESET_TYPE_HOST, copyType[copyTypeIdx], query64Bits,
                                false, dstOffset),
                            sixRepeats));
                    secondaryInheritedResetBeforeCopy->addChild(
                        new QueryPoolGraphicStatisticsTest<GeometryShaderSecondaryInheritedTestInstance>(
                            m_testCtx, prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx],
                            GraphicBasicTestInstance::ParametersGraphic(
                                VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_INVOCATIONS_BIT,
                                (VkPrimitiveTopology)topologyNdx, RESET_TYPE_BEFORE_COPY, copyType[copyTypeIdx],
                                query64Bits, false, dstOffset),
                            sixRepeats));
                    if (copyType[copyTypeIdx] == COPY_TYPE_CMD)
                        secondaryInheritedResetAfterCopy->addChild(
                            new QueryPoolGraphicStatisticsTest<GeometryShaderSecondaryInheritedTestInstance>(
                                m_testCtx, prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx],
                                GraphicBasicTestInstance::ParametersGraphic(
                                    VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_INVOCATIONS_BIT,
                                    (VkPrimitiveTopology)topologyNdx, RESET_TYPE_AFTER_COPY, copyType[copyTypeIdx],
                                    query64Bits, false, dstOffset),
                                sixRepeats));
                }
            }
        }

        geometryShaderInvocations->addChild(primary.release());
        geometryShaderInvocations->addChild(secondary.release());
        geometryShaderInvocations->addChild(secondaryInherited.release());

        geometryShaderInvocationsHostQueryReset->addChild(primaryHostQueryReset.release());
        geometryShaderInvocationsHostQueryReset->addChild(secondaryHostQueryReset.release());
        geometryShaderInvocationsHostQueryReset->addChild(secondaryInheritedHostQueryReset.release());

        geometryShaderInvocationsResetBeforeCopy->addChild(primaryResetBeforeCopy.release());
        geometryShaderInvocationsResetBeforeCopy->addChild(secondaryResetBeforeCopy.release());
        geometryShaderInvocationsResetBeforeCopy->addChild(secondaryInheritedResetBeforeCopy.release());

        geometryShaderInvocationsResetAfterCopy->addChild(primaryResetAfterCopy.release());
        geometryShaderInvocationsResetAfterCopy->addChild(secondaryResetAfterCopy.release());
        geometryShaderInvocationsResetAfterCopy->addChild(secondaryInheritedResetAfterCopy.release());
    }

    //VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_PRIMITIVES_BIT
    {
        de::MovePtr<TestCaseGroup> primary(new TestCaseGroup(m_testCtx, "primary"));
        de::MovePtr<TestCaseGroup> secondary(new TestCaseGroup(m_testCtx, "secondary"));
        de::MovePtr<TestCaseGroup> secondaryInherited(new TestCaseGroup(m_testCtx, "secondary_inherited"));

        de::MovePtr<TestCaseGroup> primaryHostQueryReset(new TestCaseGroup(m_testCtx, "primary"));
        de::MovePtr<TestCaseGroup> secondaryHostQueryReset(new TestCaseGroup(m_testCtx, "secondary"));
        de::MovePtr<TestCaseGroup> secondaryInheritedHostQueryReset(
            new TestCaseGroup(m_testCtx, "secondary_inherited"));

        de::MovePtr<TestCaseGroup> primaryResetBeforeCopy(new TestCaseGroup(m_testCtx, "primary"));
        de::MovePtr<TestCaseGroup> secondaryResetBeforeCopy(new TestCaseGroup(m_testCtx, "secondary"));
        de::MovePtr<TestCaseGroup> secondaryInheritedResetBeforeCopy(
            new TestCaseGroup(m_testCtx, "secondary_inherited"));

        de::MovePtr<TestCaseGroup> primaryResetAfterCopy(new TestCaseGroup(m_testCtx, "primary"));
        de::MovePtr<TestCaseGroup> secondaryResetAfterCopy(new TestCaseGroup(m_testCtx, "secondary"));
        de::MovePtr<TestCaseGroup> secondaryInheritedResetAfterCopy(
            new TestCaseGroup(m_testCtx, "secondary_inherited"));

        for (uint32_t copyTypeIdx = 0; copyTypeIdx < DE_LENGTH_OF_ARRAY(copyType); copyTypeIdx++)
        {
            for (int topologyNdx = VK_PRIMITIVE_TOPOLOGY_POINT_LIST; topologyNdx < VK_PRIMITIVE_TOPOLOGY_PATCH_LIST;
                 ++topologyNdx)
            {
                for (uint32_t i = 0; i < 4; ++i)
                {
                    bool query64Bits   = (i & 1);
                    bool dstOffset     = (i & 2);
                    std::string prefix = bitPrefix(query64Bits, dstOffset);

                    // It makes no sense to use dstOffset with vkGetQueryPoolResults()
                    if (copyType[copyTypeIdx] == COPY_TYPE_GET && dstOffset)
                        continue;

                    // Tests with no attachments.
                    primary->addChild(new QueryPoolGraphicStatisticsTest<GeometryShaderTestInstance>(
                        m_testCtx,
                        prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] + "_with_no_color_attachments",
                        GraphicBasicTestInstance::ParametersGraphic(
                            VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_PRIMITIVES_BIT,
                            (VkPrimitiveTopology)topologyNdx, RESET_TYPE_NORMAL, copyType[copyTypeIdx], query64Bits,
                            false, dstOffset, CLEAR_NOOP, true),
                        sixRepeats));

                    primaryHostQueryReset->addChild(new QueryPoolGraphicStatisticsTest<GeometryShaderTestInstance>(
                        m_testCtx,
                        prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] + "_with_no_color_attachments",
                        GraphicBasicTestInstance::ParametersGraphic(
                            VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_PRIMITIVES_BIT,
                            (VkPrimitiveTopology)topologyNdx, RESET_TYPE_HOST, copyType[copyTypeIdx], query64Bits,
                            false, dstOffset, CLEAR_NOOP, true),
                        sixRepeats));

                    primaryResetBeforeCopy->addChild(new QueryPoolGraphicStatisticsTest<GeometryShaderTestInstance>(
                        m_testCtx,
                        prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] + "_with_no_color_attachments",
                        GraphicBasicTestInstance::ParametersGraphic(
                            VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_PRIMITIVES_BIT,
                            (VkPrimitiveTopology)topologyNdx, RESET_TYPE_BEFORE_COPY, copyType[copyTypeIdx],
                            query64Bits, false, dstOffset, CLEAR_NOOP, true),
                        sixRepeats));

                    if (copyType[copyTypeIdx] == COPY_TYPE_CMD)
                        primaryResetAfterCopy->addChild(new QueryPoolGraphicStatisticsTest<GeometryShaderTestInstance>(
                            m_testCtx,
                            prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] +
                                "_with_no_color_attachments",
                            GraphicBasicTestInstance::ParametersGraphic(
                                VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_PRIMITIVES_BIT,
                                (VkPrimitiveTopology)topologyNdx, RESET_TYPE_AFTER_COPY, copyType[copyTypeIdx],
                                query64Bits, false, dstOffset, CLEAR_NOOP, true),
                            sixRepeats));

                    /* Tests for clear operation within a statistics query activated.
                     * Nothing for secondary_inherited cases can be done since it violates the specification.
                     *
                     * The query shouldn't count internal driver operations relevant to the clear operations.
                     */
                    const ClearOperation clearOp[] = {CLEAR_NOOP, CLEAR_COLOR, CLEAR_DEPTH};
                    const char *const clearOpStr[] = {"", "_clear_color", "_clear_depth"};

                    for (int clearOpIdx = 0; clearOpIdx < DE_LENGTH_OF_ARRAY(clearOp); ++clearOpIdx)
                    {
                        primary->addChild(new QueryPoolGraphicStatisticsTest<GeometryShaderTestInstance>(
                            m_testCtx,
                            prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] + clearOpStr[clearOpIdx],
                            GraphicBasicTestInstance::ParametersGraphic(
                                VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_PRIMITIVES_BIT,
                                (VkPrimitiveTopology)topologyNdx, RESET_TYPE_NORMAL, copyType[copyTypeIdx], query64Bits,
                                false, dstOffset, clearOp[clearOpIdx]),
                            sixRepeats));
                        secondary->addChild(new QueryPoolGraphicStatisticsTest<GeometryShaderSecondaryTestInstance>(
                            m_testCtx,
                            prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] + clearOpStr[clearOpIdx],
                            GraphicBasicTestInstance::ParametersGraphic(
                                VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_PRIMITIVES_BIT,
                                (VkPrimitiveTopology)topologyNdx, RESET_TYPE_NORMAL, copyType[copyTypeIdx], query64Bits,
                                false, dstOffset, clearOp[clearOpIdx]),
                            sixRepeats));

                        primaryHostQueryReset->addChild(new QueryPoolGraphicStatisticsTest<GeometryShaderTestInstance>(
                            m_testCtx,
                            prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] + clearOpStr[clearOpIdx],
                            GraphicBasicTestInstance::ParametersGraphic(
                                VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_PRIMITIVES_BIT,
                                (VkPrimitiveTopology)topologyNdx, RESET_TYPE_HOST, copyType[copyTypeIdx], query64Bits,
                                false, dstOffset, clearOp[clearOpIdx]),
                            sixRepeats));
                        secondaryHostQueryReset->addChild(
                            new QueryPoolGraphicStatisticsTest<GeometryShaderSecondaryTestInstance>(
                                m_testCtx,
                                prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] + clearOpStr[clearOpIdx],
                                GraphicBasicTestInstance::ParametersGraphic(
                                    VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_PRIMITIVES_BIT,
                                    (VkPrimitiveTopology)topologyNdx, RESET_TYPE_HOST, copyType[copyTypeIdx],
                                    query64Bits, false, dstOffset, clearOp[clearOpIdx]),
                                sixRepeats));

                        primaryResetBeforeCopy->addChild(new QueryPoolGraphicStatisticsTest<GeometryShaderTestInstance>(
                            m_testCtx,
                            prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] + clearOpStr[clearOpIdx],
                            GraphicBasicTestInstance::ParametersGraphic(
                                VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_PRIMITIVES_BIT,
                                (VkPrimitiveTopology)topologyNdx, RESET_TYPE_BEFORE_COPY, copyType[copyTypeIdx],
                                query64Bits, false, dstOffset, clearOp[clearOpIdx]),
                            sixRepeats));
                        secondaryResetBeforeCopy->addChild(
                            new QueryPoolGraphicStatisticsTest<GeometryShaderSecondaryTestInstance>(
                                m_testCtx,
                                prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] + clearOpStr[clearOpIdx],
                                GraphicBasicTestInstance::ParametersGraphic(
                                    VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_PRIMITIVES_BIT,
                                    (VkPrimitiveTopology)topologyNdx, RESET_TYPE_BEFORE_COPY, copyType[copyTypeIdx],
                                    query64Bits, false, dstOffset, clearOp[clearOpIdx]),
                                sixRepeats));

                        if (copyType[copyTypeIdx] == COPY_TYPE_CMD)
                        {
                            primaryResetAfterCopy->addChild(
                                new QueryPoolGraphicStatisticsTest<GeometryShaderTestInstance>(
                                    m_testCtx,
                                    prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] +
                                        clearOpStr[clearOpIdx],
                                    GraphicBasicTestInstance::ParametersGraphic(
                                        VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_PRIMITIVES_BIT,
                                        (VkPrimitiveTopology)topologyNdx, RESET_TYPE_AFTER_COPY, copyType[copyTypeIdx],
                                        query64Bits, false, dstOffset, clearOp[clearOpIdx]),
                                    sixRepeats));
                            secondaryResetAfterCopy->addChild(
                                new QueryPoolGraphicStatisticsTest<GeometryShaderSecondaryTestInstance>(
                                    m_testCtx,
                                    prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] +
                                        clearOpStr[clearOpIdx],
                                    GraphicBasicTestInstance::ParametersGraphic(
                                        VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_PRIMITIVES_BIT,
                                        (VkPrimitiveTopology)topologyNdx, RESET_TYPE_AFTER_COPY, copyType[copyTypeIdx],
                                        query64Bits, false, dstOffset, clearOp[clearOpIdx]),
                                    sixRepeats));
                        }
                    }
                    secondaryInherited->addChild(
                        new QueryPoolGraphicStatisticsTest<GeometryShaderSecondaryInheritedTestInstance>(
                            m_testCtx, prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx],
                            GraphicBasicTestInstance::ParametersGraphic(
                                VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_PRIMITIVES_BIT,
                                (VkPrimitiveTopology)topologyNdx, RESET_TYPE_NORMAL, copyType[copyTypeIdx], query64Bits,
                                false, dstOffset),
                            sixRepeats));
                    secondaryInheritedHostQueryReset->addChild(
                        new QueryPoolGraphicStatisticsTest<GeometryShaderSecondaryInheritedTestInstance>(
                            m_testCtx, prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx],
                            GraphicBasicTestInstance::ParametersGraphic(
                                VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_PRIMITIVES_BIT,
                                (VkPrimitiveTopology)topologyNdx, RESET_TYPE_HOST, copyType[copyTypeIdx], query64Bits,
                                false, dstOffset),
                            sixRepeats));
                    secondaryInheritedResetBeforeCopy->addChild(
                        new QueryPoolGraphicStatisticsTest<GeometryShaderSecondaryInheritedTestInstance>(
                            m_testCtx, prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx],
                            GraphicBasicTestInstance::ParametersGraphic(
                                VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_PRIMITIVES_BIT,
                                (VkPrimitiveTopology)topologyNdx, RESET_TYPE_BEFORE_COPY, copyType[copyTypeIdx],
                                query64Bits, false, dstOffset),
                            sixRepeats));
                    if (copyType[copyTypeIdx] == COPY_TYPE_CMD)
                        secondaryInheritedResetAfterCopy->addChild(
                            new QueryPoolGraphicStatisticsTest<GeometryShaderSecondaryInheritedTestInstance>(
                                m_testCtx, prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx],
                                GraphicBasicTestInstance::ParametersGraphic(
                                    VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_PRIMITIVES_BIT,
                                    (VkPrimitiveTopology)topologyNdx, RESET_TYPE_AFTER_COPY, copyType[copyTypeIdx],
                                    query64Bits, false, dstOffset),
                                sixRepeats));
                }
            }
        }

        geometryShaderPrimitives->addChild(primary.release());
        geometryShaderPrimitives->addChild(secondary.release());
        geometryShaderPrimitives->addChild(secondaryInherited.release());

        geometryShaderPrimitivesHostQueryReset->addChild(primaryHostQueryReset.release());
        geometryShaderPrimitivesHostQueryReset->addChild(secondaryHostQueryReset.release());
        geometryShaderPrimitivesHostQueryReset->addChild(secondaryInheritedHostQueryReset.release());

        geometryShaderPrimitivesResetBeforeCopy->addChild(primaryResetBeforeCopy.release());
        geometryShaderPrimitivesResetBeforeCopy->addChild(secondaryResetBeforeCopy.release());
        geometryShaderPrimitivesResetBeforeCopy->addChild(secondaryInheritedResetBeforeCopy.release());

        geometryShaderPrimitivesResetAfterCopy->addChild(primaryResetAfterCopy.release());
        geometryShaderPrimitivesResetAfterCopy->addChild(secondaryResetAfterCopy.release());
        geometryShaderPrimitivesResetAfterCopy->addChild(secondaryInheritedResetAfterCopy.release());
    }

    //VK_QUERY_PIPELINE_STATISTIC_CLIPPING_INVOCATIONS_BIT
    {
        de::MovePtr<TestCaseGroup> primary(new TestCaseGroup(m_testCtx, "primary"));
        de::MovePtr<TestCaseGroup> secondary(new TestCaseGroup(m_testCtx, "secondary"));
        de::MovePtr<TestCaseGroup> secondaryInherited(new TestCaseGroup(m_testCtx, "secondary_inherited"));

        de::MovePtr<TestCaseGroup> primaryHostQueryReset(new TestCaseGroup(m_testCtx, "primary"));
        de::MovePtr<TestCaseGroup> secondaryHostQueryReset(new TestCaseGroup(m_testCtx, "secondary"));
        de::MovePtr<TestCaseGroup> secondaryInheritedHostQueryReset(
            new TestCaseGroup(m_testCtx, "secondary_inherited"));

        de::MovePtr<TestCaseGroup> primaryResetBeforeCopy(new TestCaseGroup(m_testCtx, "primary"));
        de::MovePtr<TestCaseGroup> secondaryResetBeforeCopy(new TestCaseGroup(m_testCtx, "secondary"));
        de::MovePtr<TestCaseGroup> secondaryInheritedResetBeforeCopy(
            new TestCaseGroup(m_testCtx, "secondary_inherited"));

        de::MovePtr<TestCaseGroup> primaryResetAfterCopy(new TestCaseGroup(m_testCtx, "primary"));
        de::MovePtr<TestCaseGroup> secondaryResetAfterCopy(new TestCaseGroup(m_testCtx, "secondary"));
        de::MovePtr<TestCaseGroup> secondaryInheritedResetAfterCopy(
            new TestCaseGroup(m_testCtx, "secondary_inherited"));

        for (uint32_t copyTypeIdx = 0; copyTypeIdx < DE_LENGTH_OF_ARRAY(copyType); copyTypeIdx++)
        {
            for (int topologyNdx = VK_PRIMITIVE_TOPOLOGY_POINT_LIST; topologyNdx <= VK_PRIMITIVE_TOPOLOGY_PATCH_LIST;
                 ++topologyNdx)
            {
                for (uint32_t i = 0; i < 4; ++i)
                {
                    bool query64Bits   = (i & 1);
                    bool dstOffset     = (i & 2);
                    std::string prefix = bitPrefix(query64Bits, dstOffset);

                    // It makes no sense to use dstOffset with vkGetQueryPoolResults()
                    if (copyType[copyTypeIdx] == COPY_TYPE_GET && dstOffset)
                        continue;

                    addChilds(
                        primary,
                        prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] + "_with_no_color_attachments",
                        GraphicBasicTestInstance::ParametersGraphic(
                            VK_QUERY_PIPELINE_STATISTIC_CLIPPING_INVOCATIONS_BIT, (VkPrimitiveTopology)topologyNdx,
                            RESET_TYPE_NORMAL, copyType[copyTypeIdx], query64Bits, false, dstOffset, CLEAR_NOOP, true),
                        PRIMARY);

                    addChilds(
                        primaryHostQueryReset,
                        prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] + "_with_no_color_attachments",
                        GraphicBasicTestInstance::ParametersGraphic(
                            VK_QUERY_PIPELINE_STATISTIC_CLIPPING_INVOCATIONS_BIT, (VkPrimitiveTopology)topologyNdx,
                            RESET_TYPE_HOST, copyType[copyTypeIdx], query64Bits, false, dstOffset, CLEAR_NOOP, true),
                        PRIMARY);

                    addChilds(primaryResetBeforeCopy,
                              prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] +
                                  "_with_no_color_attachments",
                              GraphicBasicTestInstance::ParametersGraphic(
                                  VK_QUERY_PIPELINE_STATISTIC_CLIPPING_INVOCATIONS_BIT,
                                  (VkPrimitiveTopology)topologyNdx, RESET_TYPE_BEFORE_COPY, copyType[copyTypeIdx],
                                  query64Bits, false, dstOffset, CLEAR_NOOP, true),
                              PRIMARY);

                    if (copyType[copyTypeIdx] == COPY_TYPE_CMD)
                        addChilds(primaryResetAfterCopy,
                                  prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] +
                                      "_with_no_color_attachments",
                                  GraphicBasicTestInstance::ParametersGraphic(
                                      VK_QUERY_PIPELINE_STATISTIC_CLIPPING_INVOCATIONS_BIT,
                                      (VkPrimitiveTopology)topologyNdx, RESET_TYPE_AFTER_COPY, copyType[copyTypeIdx],
                                      query64Bits, false, dstOffset, CLEAR_NOOP, true),
                                  PRIMARY);

                    /* Tests for clear operation within a statistics query activated.
                     * Nothing for secondary_inherited cases can be done since it violates the specification.
                     *
                     * The query shouldn't count internal driver operations relevant to the clear operations.
                     */
                    const ClearOperation clearOp[] = {CLEAR_NOOP, CLEAR_COLOR, CLEAR_DEPTH};
                    const char *const clearOpStr[] = {"", "_clear_color", "_clear_depth"};

                    for (int clearOpIdx = 0; clearOpIdx < DE_LENGTH_OF_ARRAY(clearOp); ++clearOpIdx)
                    {
                        addChilds(primary,
                                  prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] +
                                      clearOpStr[clearOpIdx],
                                  GraphicBasicTestInstance::ParametersGraphic(
                                      VK_QUERY_PIPELINE_STATISTIC_CLIPPING_INVOCATIONS_BIT,
                                      (VkPrimitiveTopology)topologyNdx, RESET_TYPE_NORMAL, copyType[copyTypeIdx],
                                      query64Bits, false, dstOffset, clearOp[clearOpIdx]),
                                  PRIMARY);
                        addChilds(secondary,
                                  prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] +
                                      clearOpStr[clearOpIdx],
                                  GraphicBasicTestInstance::ParametersGraphic(
                                      VK_QUERY_PIPELINE_STATISTIC_CLIPPING_INVOCATIONS_BIT,
                                      (VkPrimitiveTopology)topologyNdx, RESET_TYPE_NORMAL, copyType[copyTypeIdx],
                                      query64Bits, false, dstOffset, clearOp[clearOpIdx]),
                                  SECONDARY);

                        addChilds(primaryHostQueryReset,
                                  prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] +
                                      clearOpStr[clearOpIdx],
                                  GraphicBasicTestInstance::ParametersGraphic(
                                      VK_QUERY_PIPELINE_STATISTIC_CLIPPING_INVOCATIONS_BIT,
                                      (VkPrimitiveTopology)topologyNdx, RESET_TYPE_HOST, copyType[copyTypeIdx],
                                      query64Bits, false, dstOffset, clearOp[clearOpIdx]),
                                  PRIMARY);

                        addChilds(secondaryHostQueryReset,
                                  prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] +
                                      clearOpStr[clearOpIdx],
                                  GraphicBasicTestInstance::ParametersGraphic(
                                      VK_QUERY_PIPELINE_STATISTIC_CLIPPING_INVOCATIONS_BIT,
                                      (VkPrimitiveTopology)topologyNdx, RESET_TYPE_HOST, copyType[copyTypeIdx],
                                      query64Bits, false, dstOffset, clearOp[clearOpIdx]),
                                  SECONDARY);

                        addChilds(primaryResetBeforeCopy,
                                  prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] +
                                      clearOpStr[clearOpIdx],
                                  GraphicBasicTestInstance::ParametersGraphic(
                                      VK_QUERY_PIPELINE_STATISTIC_CLIPPING_INVOCATIONS_BIT,
                                      (VkPrimitiveTopology)topologyNdx, RESET_TYPE_BEFORE_COPY, copyType[copyTypeIdx],
                                      query64Bits, false, dstOffset, clearOp[clearOpIdx]),
                                  PRIMARY);

                        addChilds(secondaryResetBeforeCopy,
                                  prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] +
                                      clearOpStr[clearOpIdx],
                                  GraphicBasicTestInstance::ParametersGraphic(
                                      VK_QUERY_PIPELINE_STATISTIC_CLIPPING_INVOCATIONS_BIT,
                                      (VkPrimitiveTopology)topologyNdx, RESET_TYPE_BEFORE_COPY, copyType[copyTypeIdx],
                                      query64Bits, false, dstOffset, clearOp[clearOpIdx]),
                                  SECONDARY);

                        if (copyType[copyTypeIdx] == COPY_TYPE_CMD)
                        {
                            addChilds(primaryResetAfterCopy,
                                      prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] +
                                          clearOpStr[clearOpIdx],
                                      GraphicBasicTestInstance::ParametersGraphic(
                                          VK_QUERY_PIPELINE_STATISTIC_CLIPPING_INVOCATIONS_BIT,
                                          (VkPrimitiveTopology)topologyNdx, RESET_TYPE_AFTER_COPY,
                                          copyType[copyTypeIdx], query64Bits, false, dstOffset, clearOp[clearOpIdx]),
                                      PRIMARY);
                            addChilds(secondaryResetAfterCopy,
                                      prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] +
                                          clearOpStr[clearOpIdx],
                                      GraphicBasicTestInstance::ParametersGraphic(
                                          VK_QUERY_PIPELINE_STATISTIC_CLIPPING_INVOCATIONS_BIT,
                                          (VkPrimitiveTopology)topologyNdx, RESET_TYPE_AFTER_COPY,
                                          copyType[copyTypeIdx], query64Bits, false, dstOffset, clearOp[clearOpIdx]),
                                      SECONDARY);
                        }
                    }

                    addChilds(secondaryInherited, prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx],
                              GraphicBasicTestInstance::ParametersGraphic(
                                  VK_QUERY_PIPELINE_STATISTIC_CLIPPING_INVOCATIONS_BIT,
                                  (VkPrimitiveTopology)topologyNdx, RESET_TYPE_NORMAL, copyType[copyTypeIdx],
                                  query64Bits, false, dstOffset),
                              SECONDARY_INHERITED);
                    addChilds(secondaryInheritedHostQueryReset,
                              prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx],
                              GraphicBasicTestInstance::ParametersGraphic(
                                  VK_QUERY_PIPELINE_STATISTIC_CLIPPING_INVOCATIONS_BIT,
                                  (VkPrimitiveTopology)topologyNdx, RESET_TYPE_HOST, copyType[copyTypeIdx], query64Bits,
                                  false, dstOffset),
                              SECONDARY_INHERITED);
                    addChilds(secondaryInheritedResetBeforeCopy,
                              prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx],
                              GraphicBasicTestInstance::ParametersGraphic(
                                  VK_QUERY_PIPELINE_STATISTIC_CLIPPING_INVOCATIONS_BIT,
                                  (VkPrimitiveTopology)topologyNdx, RESET_TYPE_BEFORE_COPY, copyType[copyTypeIdx],
                                  query64Bits, false, dstOffset),
                              SECONDARY_INHERITED);
                    if (copyType[copyTypeIdx] == COPY_TYPE_CMD)
                        addChilds(secondaryInheritedResetAfterCopy,
                                  prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx],
                                  GraphicBasicTestInstance::ParametersGraphic(
                                      VK_QUERY_PIPELINE_STATISTIC_CLIPPING_INVOCATIONS_BIT,
                                      (VkPrimitiveTopology)topologyNdx, RESET_TYPE_AFTER_COPY, copyType[copyTypeIdx],
                                      query64Bits, false, dstOffset),
                                  SECONDARY_INHERITED);
                }
            }
        }

        clippingInvocations->addChild(primary.release());
        clippingInvocations->addChild(secondary.release());
        clippingInvocations->addChild(secondaryInherited.release());

        clippingInvocationsHostQueryReset->addChild(primaryHostQueryReset.release());
        clippingInvocationsHostQueryReset->addChild(secondaryHostQueryReset.release());
        clippingInvocationsHostQueryReset->addChild(secondaryInheritedHostQueryReset.release());

        clippingInvocationsResetBeforeCopy->addChild(primaryResetBeforeCopy.release());
        clippingInvocationsResetBeforeCopy->addChild(secondaryResetBeforeCopy.release());
        clippingInvocationsResetBeforeCopy->addChild(secondaryInheritedResetBeforeCopy.release());

        clippingInvocationsResetAfterCopy->addChild(primaryResetAfterCopy.release());
        clippingInvocationsResetAfterCopy->addChild(secondaryResetAfterCopy.release());
        clippingInvocationsResetAfterCopy->addChild(secondaryInheritedResetAfterCopy.release());
    }

    //VK_QUERY_PIPELINE_STATISTIC_CLIPPING_PRIMITIVES_BIT
    {
        de::MovePtr<TestCaseGroup> primary(new TestCaseGroup(m_testCtx, "primary"));
        de::MovePtr<TestCaseGroup> secondary(new TestCaseGroup(m_testCtx, "secondary"));
        de::MovePtr<TestCaseGroup> secondaryInherited(new TestCaseGroup(m_testCtx, "secondary_inherited"));

        de::MovePtr<TestCaseGroup> primaryHostQueryReset(new TestCaseGroup(m_testCtx, "primary"));
        de::MovePtr<TestCaseGroup> secondaryHostQueryReset(new TestCaseGroup(m_testCtx, "secondary"));
        de::MovePtr<TestCaseGroup> secondaryInheritedHostQueryReset(
            new TestCaseGroup(m_testCtx, "secondary_inherited"));

        de::MovePtr<TestCaseGroup> primaryResetBeforeCopy(new TestCaseGroup(m_testCtx, "primary"));
        de::MovePtr<TestCaseGroup> secondaryResetBeforeCopy(new TestCaseGroup(m_testCtx, "secondary"));
        de::MovePtr<TestCaseGroup> secondaryInheritedResetBeforeCopy(
            new TestCaseGroup(m_testCtx, "secondary_inherited"));

        de::MovePtr<TestCaseGroup> primaryResetAfterCopy(new TestCaseGroup(m_testCtx, "primary"));
        de::MovePtr<TestCaseGroup> secondaryResetAfterCopy(new TestCaseGroup(m_testCtx, "secondary"));
        de::MovePtr<TestCaseGroup> secondaryInheritedResetAfterCopy(
            new TestCaseGroup(m_testCtx, "secondary_inherited"));

        for (uint32_t copyTypeIdx = 0; copyTypeIdx < DE_LENGTH_OF_ARRAY(copyType); copyTypeIdx++)
        {
            for (int topologyNdx = VK_PRIMITIVE_TOPOLOGY_POINT_LIST; topologyNdx <= VK_PRIMITIVE_TOPOLOGY_PATCH_LIST;
                 ++topologyNdx)
            {
                for (uint32_t i = 0; i < 4; ++i)
                {
                    bool query64Bits   = (i & 1);
                    bool dstOffset     = (i & 2);
                    std::string prefix = bitPrefix(query64Bits, dstOffset);

                    // It makes no sense to use dstOffset with vkGetQueryPoolResults()
                    if (copyType[copyTypeIdx] == COPY_TYPE_GET && dstOffset)
                        continue;

                    // Tests with no attachments for only primary command to reduce # of test cases.
                    addChilds(
                        primary,
                        prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] + "_with_no_color_attachments",
                        GraphicBasicTestInstance::ParametersGraphic(
                            VK_QUERY_PIPELINE_STATISTIC_CLIPPING_PRIMITIVES_BIT, (VkPrimitiveTopology)topologyNdx,
                            RESET_TYPE_NORMAL, copyType[copyTypeIdx], query64Bits, false, dstOffset, CLEAR_NOOP, true),
                        PRIMARY);

                    addChilds(
                        primaryHostQueryReset,
                        prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] + "_with_no_color_attachments",
                        GraphicBasicTestInstance::ParametersGraphic(
                            VK_QUERY_PIPELINE_STATISTIC_CLIPPING_PRIMITIVES_BIT, (VkPrimitiveTopology)topologyNdx,
                            RESET_TYPE_HOST, copyType[copyTypeIdx], query64Bits, false, dstOffset, CLEAR_NOOP, true),
                        PRIMARY);

                    addChilds(
                        primaryResetBeforeCopy,
                        prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] + "_with_no_color_attachments",
                        GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_CLIPPING_PRIMITIVES_BIT,
                                                                    (VkPrimitiveTopology)topologyNdx,
                                                                    RESET_TYPE_BEFORE_COPY, copyType[copyTypeIdx],
                                                                    query64Bits, false, dstOffset, CLEAR_NOOP, true),
                        PRIMARY);

                    if (copyType[copyTypeIdx] == COPY_TYPE_CMD)
                        addChilds(primaryResetAfterCopy,
                                  prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] +
                                      "_with_no_color_attachments",
                                  GraphicBasicTestInstance::ParametersGraphic(
                                      VK_QUERY_PIPELINE_STATISTIC_CLIPPING_PRIMITIVES_BIT,
                                      (VkPrimitiveTopology)topologyNdx, RESET_TYPE_AFTER_COPY, copyType[copyTypeIdx],
                                      query64Bits, false, dstOffset, CLEAR_NOOP, true),
                                  PRIMARY);

                    /* Tests for clear operation within a statistics query activated.
                     * Nothing for secondary_inherited cases can be done since it violates the specification.
                     *
                     * The query shouldn't count internal driver operations relevant to the clear operations.
                     */
                    const ClearOperation clearOp[] = {CLEAR_NOOP, CLEAR_COLOR, CLEAR_DEPTH};
                    const char *const clearOpStr[] = {"", "_clear_color", "_clear_depth"};

                    for (int clearOpIdx = 0; clearOpIdx < DE_LENGTH_OF_ARRAY(clearOp); ++clearOpIdx)
                    {
                        addChilds(primary,
                                  prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] +
                                      clearOpStr[clearOpIdx],
                                  GraphicBasicTestInstance::ParametersGraphic(
                                      VK_QUERY_PIPELINE_STATISTIC_CLIPPING_PRIMITIVES_BIT,
                                      (VkPrimitiveTopology)topologyNdx, RESET_TYPE_NORMAL, copyType[copyTypeIdx],
                                      query64Bits, false, dstOffset, clearOp[clearOpIdx]),
                                  PRIMARY);

                        addChilds(secondary,
                                  prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] +
                                      clearOpStr[clearOpIdx],
                                  GraphicBasicTestInstance::ParametersGraphic(
                                      VK_QUERY_PIPELINE_STATISTIC_CLIPPING_PRIMITIVES_BIT,
                                      (VkPrimitiveTopology)topologyNdx, RESET_TYPE_NORMAL, copyType[copyTypeIdx],
                                      query64Bits, false, dstOffset, clearOp[clearOpIdx]),
                                  SECONDARY);

                        addChilds(primaryHostQueryReset,
                                  prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] +
                                      clearOpStr[clearOpIdx],
                                  GraphicBasicTestInstance::ParametersGraphic(
                                      VK_QUERY_PIPELINE_STATISTIC_CLIPPING_PRIMITIVES_BIT,
                                      (VkPrimitiveTopology)topologyNdx, RESET_TYPE_HOST, copyType[copyTypeIdx],
                                      query64Bits, false, dstOffset, clearOp[clearOpIdx]),
                                  PRIMARY);

                        addChilds(secondaryHostQueryReset,
                                  prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] +
                                      clearOpStr[clearOpIdx],
                                  GraphicBasicTestInstance::ParametersGraphic(
                                      VK_QUERY_PIPELINE_STATISTIC_CLIPPING_PRIMITIVES_BIT,
                                      (VkPrimitiveTopology)topologyNdx, RESET_TYPE_HOST, copyType[copyTypeIdx],
                                      query64Bits, false, dstOffset, clearOp[clearOpIdx]),
                                  SECONDARY);

                        addChilds(primaryResetBeforeCopy,
                                  prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] +
                                      clearOpStr[clearOpIdx],
                                  GraphicBasicTestInstance::ParametersGraphic(
                                      VK_QUERY_PIPELINE_STATISTIC_CLIPPING_PRIMITIVES_BIT,
                                      (VkPrimitiveTopology)topologyNdx, RESET_TYPE_BEFORE_COPY, copyType[copyTypeIdx],
                                      query64Bits, false, dstOffset, clearOp[clearOpIdx]),
                                  PRIMARY);

                        addChilds(secondaryResetBeforeCopy,
                                  prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] +
                                      clearOpStr[clearOpIdx],
                                  GraphicBasicTestInstance::ParametersGraphic(
                                      VK_QUERY_PIPELINE_STATISTIC_CLIPPING_PRIMITIVES_BIT,
                                      (VkPrimitiveTopology)topologyNdx, RESET_TYPE_BEFORE_COPY, copyType[copyTypeIdx],
                                      query64Bits, false, dstOffset, clearOp[clearOpIdx]),
                                  SECONDARY);

                        if (copyType[copyTypeIdx] == COPY_TYPE_CMD)
                        {
                            addChilds(primaryResetAfterCopy,
                                      prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] +
                                          clearOpStr[clearOpIdx],
                                      GraphicBasicTestInstance::ParametersGraphic(
                                          VK_QUERY_PIPELINE_STATISTIC_CLIPPING_PRIMITIVES_BIT,
                                          (VkPrimitiveTopology)topologyNdx, RESET_TYPE_AFTER_COPY,
                                          copyType[copyTypeIdx], query64Bits, false, dstOffset, clearOp[clearOpIdx]),
                                      PRIMARY);

                            addChilds(secondaryResetAfterCopy,
                                      prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] +
                                          clearOpStr[clearOpIdx],
                                      GraphicBasicTestInstance::ParametersGraphic(
                                          VK_QUERY_PIPELINE_STATISTIC_CLIPPING_PRIMITIVES_BIT,
                                          (VkPrimitiveTopology)topologyNdx, RESET_TYPE_AFTER_COPY,
                                          copyType[copyTypeIdx], query64Bits, false, dstOffset, clearOp[clearOpIdx]),
                                      SECONDARY);
                        }
                    }

                    addChilds(secondaryInherited, prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx],
                              GraphicBasicTestInstance::ParametersGraphic(
                                  VK_QUERY_PIPELINE_STATISTIC_CLIPPING_PRIMITIVES_BIT, (VkPrimitiveTopology)topologyNdx,
                                  RESET_TYPE_NORMAL, copyType[copyTypeIdx], query64Bits, false, dstOffset),
                              SECONDARY_INHERITED);

                    addChilds(secondaryInheritedHostQueryReset,
                              prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx],
                              GraphicBasicTestInstance::ParametersGraphic(
                                  VK_QUERY_PIPELINE_STATISTIC_CLIPPING_PRIMITIVES_BIT, (VkPrimitiveTopology)topologyNdx,
                                  RESET_TYPE_HOST, copyType[copyTypeIdx], query64Bits, false, dstOffset),
                              SECONDARY_INHERITED);

                    addChilds(secondaryInheritedResetBeforeCopy,
                              prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx],
                              GraphicBasicTestInstance::ParametersGraphic(
                                  VK_QUERY_PIPELINE_STATISTIC_CLIPPING_PRIMITIVES_BIT, (VkPrimitiveTopology)topologyNdx,
                                  RESET_TYPE_BEFORE_COPY, copyType[copyTypeIdx], query64Bits, false, dstOffset),
                              SECONDARY_INHERITED);

                    if (copyType[copyTypeIdx] == COPY_TYPE_CMD)
                        addChilds(secondaryInheritedResetAfterCopy,
                                  prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx],
                                  GraphicBasicTestInstance::ParametersGraphic(
                                      VK_QUERY_PIPELINE_STATISTIC_CLIPPING_PRIMITIVES_BIT,
                                      (VkPrimitiveTopology)topologyNdx, RESET_TYPE_AFTER_COPY, copyType[copyTypeIdx],
                                      query64Bits, false, dstOffset),
                                  SECONDARY_INHERITED);
                }
            }
        }

        clippingPrimitives->addChild(primary.release());
        clippingPrimitives->addChild(secondary.release());
        clippingPrimitives->addChild(secondaryInherited.release());

        clippingPrimitivesHostQueryReset->addChild(primaryHostQueryReset.release());
        clippingPrimitivesHostQueryReset->addChild(secondaryHostQueryReset.release());
        clippingPrimitivesHostQueryReset->addChild(secondaryInheritedHostQueryReset.release());

        clippingPrimitivesResetBeforeCopy->addChild(primaryResetBeforeCopy.release());
        clippingPrimitivesResetBeforeCopy->addChild(secondaryResetBeforeCopy.release());
        clippingPrimitivesResetBeforeCopy->addChild(secondaryInheritedResetBeforeCopy.release());

        clippingPrimitivesResetAfterCopy->addChild(primaryResetAfterCopy.release());
        clippingPrimitivesResetAfterCopy->addChild(secondaryResetAfterCopy.release());
        clippingPrimitivesResetAfterCopy->addChild(secondaryInheritedResetAfterCopy.release());
    }

    //VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_CONTROL_SHADER_PATCHES_BIT
    //VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_EVALUATION_SHADER_INVOCATIONS_BIT
    for (uint32_t copyTypeIdx = 0; copyTypeIdx < DE_LENGTH_OF_ARRAY(copyType); copyTypeIdx++)
    {
        const TessPrimitiveMode primModes[] = {TESS_PRIM_TRIANGLES, TESS_PRIM_ISOLINES, TESS_PRIM_QUADS};
        const std::string primModeStrs[]    = {"_triangles", "_isolines", "_quads"};
        const bool pointModes[]             = {false, true};
        const std::string pointModeStrs[]   = {"", "_point_mode"};

        const uint32_t patchSizes[] = {3, 2, 4};

        for (uint32_t i = 0; i < 4; ++i)
        {
            bool query64Bits   = (i & 1);
            bool dstOffset     = (i & 2);
            std::string prefix = bitPrefix(query64Bits, dstOffset);

            // It makes no sense to use dstOffset with vkGetQueryPoolResults()
            if (copyType[copyTypeIdx] == COPY_TYPE_GET && dstOffset)
                continue;

            for (uint32_t primModeIdx = 0; primModeIdx < DE_LENGTH_OF_ARRAY(primModes); primModeIdx++)
            {
                for (uint32_t pointModeIdx = 0; pointModeIdx < DE_LENGTH_OF_ARRAY(pointModes); pointModeIdx++)
                {
                    // Skip point mode for isolines to reduce test count
                    if (primModes[primModeIdx] == TESS_PRIM_ISOLINES && pointModes[pointModeIdx])
                        continue;

                    std::string tessModeStr = primModeStrs[primModeIdx] + pointModeStrs[pointModeIdx];

                    // Add tessellation control shader patches test
                    tesControlPatches->addChild(new QueryPoolGraphicStatisticsTest<TessellationShaderTestInstance>(
                        m_testCtx, prefix + copyTypeStr[copyTypeIdx] + "tes_control_patches" + tessModeStr,
                        GraphicBasicTestInstance::ParametersGraphic(
                            VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_CONTROL_SHADER_PATCHES_BIT,
                            VK_PRIMITIVE_TOPOLOGY_PATCH_LIST, RESET_TYPE_NORMAL, copyType[copyTypeIdx], query64Bits,
                            false, dstOffset, CLEAR_NOOP, false, STRIDE_TYPE_VALID, true, patchSizes[primModeIdx], 4,
                            primModes[primModeIdx], pointModes[pointModeIdx]),
                        sixRepeats));

                    // Add tessellation evaluation shader invocations test
                    tesEvaluationShaderInvocations->addChild(
                        new QueryPoolGraphicStatisticsTest<TessellationShaderTestInstance>(
                            m_testCtx,
                            prefix + copyTypeStr[copyTypeIdx] + "tes_evaluation_shader_invocations" + tessModeStr,
                            GraphicBasicTestInstance::ParametersGraphic(
                                VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_EVALUATION_SHADER_INVOCATIONS_BIT,
                                VK_PRIMITIVE_TOPOLOGY_PATCH_LIST, RESET_TYPE_NORMAL, copyType[copyTypeIdx], query64Bits,
                                false, dstOffset, CLEAR_NOOP, false, STRIDE_TYPE_VALID, true, patchSizes[primModeIdx],
                                4, primModes[primModeIdx], pointModes[pointModeIdx]),
                            sixRepeats));
                }
            }

            // Tests with no attachments for only primary command to reduce # of test cases.
            tesControlPatches->addChild(new QueryPoolGraphicStatisticsTest<TessellationShaderTestInstance>(
                m_testCtx, prefix + copyTypeStr[copyTypeIdx] + "tes_control_patches_with_no_color_attachments",
                GraphicBasicTestInstance::ParametersGraphic(
                    VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_CONTROL_SHADER_PATCHES_BIT,
                    VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, RESET_TYPE_NORMAL, copyType[copyTypeIdx], query64Bits, false,
                    dstOffset, CLEAR_NOOP, true, STRIDE_TYPE_VALID, true),
                sixRepeats));

            tesControlPatchesHostQueryReset->addChild(
                new QueryPoolGraphicStatisticsTest<TessellationShaderTestInstance>(
                    m_testCtx, prefix + copyTypeStr[copyTypeIdx] + "tes_control_patches_with_no_color_attachments",
                    GraphicBasicTestInstance::ParametersGraphic(
                        VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_CONTROL_SHADER_PATCHES_BIT,
                        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, RESET_TYPE_HOST, copyType[copyTypeIdx], query64Bits,
                        false, dstOffset, CLEAR_NOOP, true, STRIDE_TYPE_VALID, true),
                    sixRepeats));

            tesControlPatchesResetBeforeCopy->addChild(
                new QueryPoolGraphicStatisticsTest<TessellationShaderTestInstance>(
                    m_testCtx, prefix + copyTypeStr[copyTypeIdx] + "tes_control_patches_with_no_color_attachments",
                    GraphicBasicTestInstance::ParametersGraphic(
                        VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_CONTROL_SHADER_PATCHES_BIT,
                        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, RESET_TYPE_BEFORE_COPY, copyType[copyTypeIdx],
                        query64Bits, false, dstOffset, CLEAR_NOOP, true, STRIDE_TYPE_VALID, true),
                    sixRepeats));

            if (copyType[copyTypeIdx] == COPY_TYPE_CMD)
                tesControlPatchesResetAfterCopy->addChild(
                    new QueryPoolGraphicStatisticsTest<TessellationShaderTestInstance>(
                        m_testCtx, prefix + copyTypeStr[copyTypeIdx] + "tes_control_patches_with_no_color_attachments",
                        GraphicBasicTestInstance::ParametersGraphic(
                            VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_CONTROL_SHADER_PATCHES_BIT,
                            VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, RESET_TYPE_AFTER_COPY, copyType[copyTypeIdx],
                            query64Bits, false, dstOffset, CLEAR_NOOP, true, STRIDE_TYPE_VALID, true),
                        sixRepeats));

            tesEvaluationShaderInvocations->addChild(new QueryPoolGraphicStatisticsTest<TessellationShaderTestInstance>(
                m_testCtx,
                prefix + copyTypeStr[copyTypeIdx] + "tes_evaluation_shader_invocations_with_no_color_attachments",
                GraphicBasicTestInstance::ParametersGraphic(
                    VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_EVALUATION_SHADER_INVOCATIONS_BIT,
                    VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, RESET_TYPE_NORMAL, copyType[copyTypeIdx], query64Bits, false,
                    dstOffset, CLEAR_NOOP, true, STRIDE_TYPE_VALID, true),
                sixRepeats));

            tesEvaluationShaderInvocationsHostQueryReset->addChild(
                new QueryPoolGraphicStatisticsTest<TessellationShaderTestInstance>(
                    m_testCtx,
                    prefix + copyTypeStr[copyTypeIdx] + "tes_evaluation_shader_invocations_with_no_color_attachments",
                    GraphicBasicTestInstance::ParametersGraphic(
                        VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_EVALUATION_SHADER_INVOCATIONS_BIT,
                        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, RESET_TYPE_HOST, copyType[copyTypeIdx], query64Bits,
                        false, dstOffset, CLEAR_NOOP, true, STRIDE_TYPE_VALID, true),
                    sixRepeats));

            tesEvaluationShaderInvocationsResetBeforeCopy->addChild(
                new QueryPoolGraphicStatisticsTest<TessellationShaderTestInstance>(
                    m_testCtx,
                    prefix + copyTypeStr[copyTypeIdx] + "tes_evaluation_shader_invocations_with_no_color_attachments",
                    GraphicBasicTestInstance::ParametersGraphic(
                        VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_EVALUATION_SHADER_INVOCATIONS_BIT,
                        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, RESET_TYPE_BEFORE_COPY, copyType[copyTypeIdx],
                        query64Bits, false, dstOffset, CLEAR_NOOP, true, STRIDE_TYPE_VALID, true),
                    sixRepeats));

            if (copyType[copyTypeIdx] == COPY_TYPE_CMD)
                tesEvaluationShaderInvocationsResetAfterCopy->addChild(
                    new QueryPoolGraphicStatisticsTest<TessellationShaderTestInstance>(
                        m_testCtx,
                        prefix + copyTypeStr[copyTypeIdx] +
                            "tes_evaluation_shader_invocations_with_no_color_attachments",
                        GraphicBasicTestInstance::ParametersGraphic(
                            VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_EVALUATION_SHADER_INVOCATIONS_BIT,
                            VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, RESET_TYPE_AFTER_COPY, copyType[copyTypeIdx],
                            query64Bits, false, dstOffset, CLEAR_NOOP, true, STRIDE_TYPE_VALID, true),
                        sixRepeats));

            /* Tests for clear operation within a statistics query activated.
             * Nothing for secondary_inherited cases can be done since it violates the specification.
             *
             * The query shouldn't count internal driver operations relevant to the clear operations.
             */
            const ClearOperation clearOp[] = {CLEAR_NOOP, CLEAR_COLOR, CLEAR_DEPTH};
            const char *const clearOpStr[] = {"", "_clear_color", "_clear_depth"};

            for (int clearOpIdx = 0; clearOpIdx < DE_LENGTH_OF_ARRAY(clearOp); ++clearOpIdx)
            {
                tesControlPatches->addChild(new QueryPoolGraphicStatisticsTest<TessellationShaderTestInstance>(
                    m_testCtx, prefix + copyTypeStr[copyTypeIdx] + "tes_control_patches" + clearOpStr[clearOpIdx],
                    GraphicBasicTestInstance::ParametersGraphic(
                        VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_CONTROL_SHADER_PATCHES_BIT,
                        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, RESET_TYPE_NORMAL, copyType[copyTypeIdx], query64Bits,
                        false, dstOffset, clearOp[clearOpIdx], false, STRIDE_TYPE_VALID, true),
                    sixRepeats));
                tesControlPatches->addChild(new QueryPoolGraphicStatisticsTest<TessellationShaderSecondrayTestInstance>(
                    m_testCtx,
                    prefix + copyTypeStr[copyTypeIdx] + "tes_control_patches_secondary" + clearOpStr[clearOpIdx],
                    GraphicBasicTestInstance::ParametersGraphic(
                        VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_CONTROL_SHADER_PATCHES_BIT,
                        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, RESET_TYPE_NORMAL, copyType[copyTypeIdx], query64Bits,
                        false, dstOffset, clearOp[clearOpIdx], false, STRIDE_TYPE_VALID, true),
                    sixRepeats));

                tesControlPatchesHostQueryReset->addChild(
                    new QueryPoolGraphicStatisticsTest<TessellationShaderTestInstance>(
                        m_testCtx, prefix + copyTypeStr[copyTypeIdx] + "tes_control_patches" + clearOpStr[clearOpIdx],
                        GraphicBasicTestInstance::ParametersGraphic(
                            VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_CONTROL_SHADER_PATCHES_BIT,
                            VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, RESET_TYPE_HOST, copyType[copyTypeIdx], query64Bits,
                            false, dstOffset, clearOp[clearOpIdx], false, STRIDE_TYPE_VALID, true),
                        sixRepeats));
                tesControlPatchesHostQueryReset->addChild(
                    new QueryPoolGraphicStatisticsTest<TessellationShaderSecondrayTestInstance>(
                        m_testCtx,
                        prefix + copyTypeStr[copyTypeIdx] + "tes_control_patches_secondary" + clearOpStr[clearOpIdx],
                        GraphicBasicTestInstance::ParametersGraphic(
                            VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_CONTROL_SHADER_PATCHES_BIT,
                            VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, RESET_TYPE_HOST, copyType[copyTypeIdx], query64Bits,
                            false, dstOffset, clearOp[clearOpIdx], false, STRIDE_TYPE_VALID, true),
                        sixRepeats));

                tesControlPatchesResetBeforeCopy->addChild(
                    new QueryPoolGraphicStatisticsTest<TessellationShaderTestInstance>(
                        m_testCtx, prefix + copyTypeStr[copyTypeIdx] + "tes_control_patches" + clearOpStr[clearOpIdx],
                        GraphicBasicTestInstance::ParametersGraphic(
                            VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_CONTROL_SHADER_PATCHES_BIT,
                            VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, RESET_TYPE_BEFORE_COPY, copyType[copyTypeIdx],
                            query64Bits, false, dstOffset, clearOp[clearOpIdx], false, STRIDE_TYPE_VALID, true),
                        sixRepeats));
                tesControlPatchesResetBeforeCopy->addChild(
                    new QueryPoolGraphicStatisticsTest<TessellationShaderSecondrayTestInstance>(
                        m_testCtx,
                        prefix + copyTypeStr[copyTypeIdx] + "tes_control_patches_secondary" + clearOpStr[clearOpIdx],
                        GraphicBasicTestInstance::ParametersGraphic(
                            VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_CONTROL_SHADER_PATCHES_BIT,
                            VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, RESET_TYPE_BEFORE_COPY, copyType[copyTypeIdx],
                            query64Bits, false, dstOffset, clearOp[clearOpIdx], false, STRIDE_TYPE_VALID, true),
                        sixRepeats));

                if (copyType[copyTypeIdx] == COPY_TYPE_CMD)
                {
                    tesControlPatchesResetAfterCopy->addChild(
                        new QueryPoolGraphicStatisticsTest<TessellationShaderTestInstance>(
                            m_testCtx,
                            prefix + copyTypeStr[copyTypeIdx] + "tes_control_patches" + clearOpStr[clearOpIdx],
                            GraphicBasicTestInstance::ParametersGraphic(
                                VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_CONTROL_SHADER_PATCHES_BIT,
                                VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, RESET_TYPE_AFTER_COPY, copyType[copyTypeIdx],
                                query64Bits, false, dstOffset, clearOp[clearOpIdx], false, STRIDE_TYPE_VALID, true),
                            sixRepeats));
                    tesControlPatchesResetAfterCopy->addChild(
                        new QueryPoolGraphicStatisticsTest<TessellationShaderSecondrayTestInstance>(
                            m_testCtx,
                            prefix + copyTypeStr[copyTypeIdx] + "tes_control_patches_secondary" +
                                clearOpStr[clearOpIdx],
                            GraphicBasicTestInstance::ParametersGraphic(
                                VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_CONTROL_SHADER_PATCHES_BIT,
                                VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, RESET_TYPE_AFTER_COPY, copyType[copyTypeIdx],
                                query64Bits, false, dstOffset, clearOp[clearOpIdx], false, STRIDE_TYPE_VALID, true),
                            sixRepeats));
                }

                tesEvaluationShaderInvocations->addChild(
                    new QueryPoolGraphicStatisticsTest<TessellationShaderTestInstance>(
                        m_testCtx,
                        prefix + copyTypeStr[copyTypeIdx] + "tes_evaluation_shader_invocations" +
                            clearOpStr[clearOpIdx],
                        GraphicBasicTestInstance::ParametersGraphic(
                            VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_EVALUATION_SHADER_INVOCATIONS_BIT,
                            VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, RESET_TYPE_NORMAL, copyType[copyTypeIdx], query64Bits,
                            false, dstOffset, clearOp[clearOpIdx], false, STRIDE_TYPE_VALID, true),
                        sixRepeats));
                tesEvaluationShaderInvocations->addChild(
                    new QueryPoolGraphicStatisticsTest<TessellationShaderSecondrayTestInstance>(
                        m_testCtx,
                        prefix + copyTypeStr[copyTypeIdx] + "tes_evaluation_shader_invocations_secondary" +
                            clearOpStr[clearOpIdx],
                        GraphicBasicTestInstance::ParametersGraphic(
                            VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_EVALUATION_SHADER_INVOCATIONS_BIT,
                            VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, RESET_TYPE_NORMAL, copyType[copyTypeIdx], query64Bits,
                            false, dstOffset, clearOp[clearOpIdx], false, STRIDE_TYPE_VALID, true),
                        sixRepeats));

                tesEvaluationShaderInvocationsHostQueryReset->addChild(
                    new QueryPoolGraphicStatisticsTest<TessellationShaderTestInstance>(
                        m_testCtx,
                        prefix + copyTypeStr[copyTypeIdx] + "tes_evaluation_shader_invocations" +
                            clearOpStr[clearOpIdx],
                        GraphicBasicTestInstance::ParametersGraphic(
                            VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_EVALUATION_SHADER_INVOCATIONS_BIT,
                            VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, RESET_TYPE_HOST, copyType[copyTypeIdx], query64Bits,
                            false, dstOffset, clearOp[clearOpIdx], false, STRIDE_TYPE_VALID, true),
                        sixRepeats));
                tesEvaluationShaderInvocationsHostQueryReset->addChild(
                    new QueryPoolGraphicStatisticsTest<TessellationShaderSecondrayTestInstance>(
                        m_testCtx,
                        prefix + copyTypeStr[copyTypeIdx] + "tes_evaluation_shader_invocations_secondary" +
                            clearOpStr[clearOpIdx],
                        GraphicBasicTestInstance::ParametersGraphic(
                            VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_EVALUATION_SHADER_INVOCATIONS_BIT,
                            VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, RESET_TYPE_HOST, copyType[copyTypeIdx], query64Bits,
                            false, dstOffset, clearOp[clearOpIdx], false, STRIDE_TYPE_VALID, true),
                        sixRepeats));

                tesEvaluationShaderInvocationsResetBeforeCopy->addChild(
                    new QueryPoolGraphicStatisticsTest<TessellationShaderTestInstance>(
                        m_testCtx,
                        prefix + copyTypeStr[copyTypeIdx] + "tes_evaluation_shader_invocations" +
                            clearOpStr[clearOpIdx],
                        GraphicBasicTestInstance::ParametersGraphic(
                            VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_EVALUATION_SHADER_INVOCATIONS_BIT,
                            VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, RESET_TYPE_BEFORE_COPY, copyType[copyTypeIdx],
                            query64Bits, false, dstOffset, clearOp[clearOpIdx], false, STRIDE_TYPE_VALID, true),
                        sixRepeats));
                tesEvaluationShaderInvocationsResetBeforeCopy->addChild(
                    new QueryPoolGraphicStatisticsTest<TessellationShaderSecondrayTestInstance>(
                        m_testCtx,
                        prefix + copyTypeStr[copyTypeIdx] + "tes_evaluation_shader_invocations_secondary" +
                            clearOpStr[clearOpIdx],
                        GraphicBasicTestInstance::ParametersGraphic(
                            VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_EVALUATION_SHADER_INVOCATIONS_BIT,
                            VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, RESET_TYPE_BEFORE_COPY, copyType[copyTypeIdx],
                            query64Bits, false, dstOffset, clearOp[clearOpIdx], false, STRIDE_TYPE_VALID, true),
                        sixRepeats));

                if (copyType[copyTypeIdx] == COPY_TYPE_CMD)
                {
                    tesEvaluationShaderInvocationsResetAfterCopy->addChild(
                        new QueryPoolGraphicStatisticsTest<TessellationShaderTestInstance>(
                            m_testCtx,
                            prefix + copyTypeStr[copyTypeIdx] + "tes_evaluation_shader_invocations" +
                                clearOpStr[clearOpIdx],
                            GraphicBasicTestInstance::ParametersGraphic(
                                VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_EVALUATION_SHADER_INVOCATIONS_BIT,
                                VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, RESET_TYPE_AFTER_COPY, copyType[copyTypeIdx],
                                query64Bits, false, dstOffset, clearOp[clearOpIdx], false, STRIDE_TYPE_VALID, true),
                            sixRepeats));
                    tesEvaluationShaderInvocationsResetAfterCopy->addChild(
                        new QueryPoolGraphicStatisticsTest<TessellationShaderSecondrayTestInstance>(
                            m_testCtx,
                            prefix + copyTypeStr[copyTypeIdx] + "tes_evaluation_shader_invocations_secondary" +
                                clearOpStr[clearOpIdx],
                            GraphicBasicTestInstance::ParametersGraphic(
                                VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_EVALUATION_SHADER_INVOCATIONS_BIT,
                                VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, RESET_TYPE_AFTER_COPY, copyType[copyTypeIdx],
                                query64Bits, false, dstOffset, clearOp[clearOpIdx], false, STRIDE_TYPE_VALID, true),
                            sixRepeats));
                }
            }

            tesControlPatches->addChild(
                new QueryPoolGraphicStatisticsTest<TessellationShaderSecondrayInheritedTestInstance>(
                    m_testCtx, prefix + copyTypeStr[copyTypeIdx] + "tes_control_patches_secondary_inherited",
                    GraphicBasicTestInstance::ParametersGraphic(
                        VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_CONTROL_SHADER_PATCHES_BIT,
                        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, RESET_TYPE_NORMAL, copyType[copyTypeIdx], query64Bits,
                        false, dstOffset, CLEAR_NOOP, false, STRIDE_TYPE_VALID, true),
                    sixRepeats));
            tesControlPatchesHostQueryReset->addChild(
                new QueryPoolGraphicStatisticsTest<TessellationShaderSecondrayInheritedTestInstance>(
                    m_testCtx, prefix + copyTypeStr[copyTypeIdx] + "tes_control_patches_secondary_inherited",
                    GraphicBasicTestInstance::ParametersGraphic(
                        VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_CONTROL_SHADER_PATCHES_BIT,
                        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, RESET_TYPE_HOST, copyType[copyTypeIdx], query64Bits,
                        false, dstOffset, CLEAR_NOOP, false, STRIDE_TYPE_VALID, true),
                    sixRepeats));
            tesControlPatchesResetBeforeCopy->addChild(
                new QueryPoolGraphicStatisticsTest<TessellationShaderSecondrayInheritedTestInstance>(
                    m_testCtx, prefix + copyTypeStr[copyTypeIdx] + "tes_control_patches_secondary_inherited",
                    GraphicBasicTestInstance::ParametersGraphic(
                        VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_CONTROL_SHADER_PATCHES_BIT,
                        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, RESET_TYPE_BEFORE_COPY, copyType[copyTypeIdx],
                        query64Bits, false, dstOffset, CLEAR_NOOP, false, STRIDE_TYPE_VALID, true),
                    sixRepeats));
            if (copyType[copyTypeIdx] == COPY_TYPE_CMD)
                tesControlPatchesResetAfterCopy->addChild(
                    new QueryPoolGraphicStatisticsTest<TessellationShaderSecondrayInheritedTestInstance>(
                        m_testCtx, prefix + copyTypeStr[copyTypeIdx] + "tes_control_patches_secondary_inherited",
                        GraphicBasicTestInstance::ParametersGraphic(
                            VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_CONTROL_SHADER_PATCHES_BIT,
                            VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, RESET_TYPE_AFTER_COPY, copyType[copyTypeIdx],
                            query64Bits, false, dstOffset, CLEAR_NOOP, false, STRIDE_TYPE_VALID, true),
                        sixRepeats));

            tesEvaluationShaderInvocations->addChild(
                new QueryPoolGraphicStatisticsTest<TessellationShaderSecondrayInheritedTestInstance>(
                    m_testCtx,
                    prefix + copyTypeStr[copyTypeIdx] + "tes_evaluation_shader_invocations_secondary_inherited",
                    GraphicBasicTestInstance::ParametersGraphic(
                        VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_EVALUATION_SHADER_INVOCATIONS_BIT,
                        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, RESET_TYPE_NORMAL, copyType[copyTypeIdx], query64Bits,
                        false, dstOffset, CLEAR_NOOP, false, STRIDE_TYPE_VALID, true),
                    sixRepeats));
            tesEvaluationShaderInvocationsHostQueryReset->addChild(
                new QueryPoolGraphicStatisticsTest<TessellationShaderSecondrayInheritedTestInstance>(
                    m_testCtx,
                    prefix + copyTypeStr[copyTypeIdx] + "tes_evaluation_shader_invocations_secondary_inherited",
                    GraphicBasicTestInstance::ParametersGraphic(
                        VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_EVALUATION_SHADER_INVOCATIONS_BIT,
                        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, RESET_TYPE_HOST, copyType[copyTypeIdx], query64Bits,
                        false, dstOffset, CLEAR_NOOP, false, STRIDE_TYPE_VALID, true),
                    sixRepeats));
            tesEvaluationShaderInvocationsResetBeforeCopy->addChild(
                new QueryPoolGraphicStatisticsTest<TessellationShaderSecondrayInheritedTestInstance>(
                    m_testCtx,
                    prefix + copyTypeStr[copyTypeIdx] + "tes_evaluation_shader_invocations_secondary_inherited",
                    GraphicBasicTestInstance::ParametersGraphic(
                        VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_EVALUATION_SHADER_INVOCATIONS_BIT,
                        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, RESET_TYPE_BEFORE_COPY, copyType[copyTypeIdx],
                        query64Bits, false, dstOffset, CLEAR_NOOP, false, STRIDE_TYPE_VALID, true),
                    sixRepeats));
            if (copyType[copyTypeIdx] == COPY_TYPE_CMD)
                tesEvaluationShaderInvocationsResetAfterCopy->addChild(
                    new QueryPoolGraphicStatisticsTest<TessellationShaderSecondrayInheritedTestInstance>(
                        m_testCtx,
                        prefix + copyTypeStr[copyTypeIdx] + "tes_evaluation_shader_invocations_secondary_inherited",
                        GraphicBasicTestInstance::ParametersGraphic(
                            VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_EVALUATION_SHADER_INVOCATIONS_BIT,
                            VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, RESET_TYPE_AFTER_COPY, copyType[copyTypeIdx],
                            query64Bits, false, dstOffset, CLEAR_NOOP, false, STRIDE_TYPE_VALID, true),
                        sixRepeats));
        }
    }

    // Multiple statistics query flags enabled
    {
        VkQueryResultFlags partialFlags[]   = {0u, VK_QUERY_RESULT_PARTIAL_BIT};
        const char *const partialFlagsStr[] = {"", "_partial"};
        VkQueryResultFlags waitFlags[]      = {0u, VK_QUERY_RESULT_WAIT_BIT};
        const char *const waitFlagsStr[]    = {"", "_wait"};

        const CopyType copyTypes[]       = {COPY_TYPE_GET, COPY_TYPE_CMD, COPY_TYPE_CMD};
        const char *const copyTypesStr[] = {"", "_cmdcopy", "_cmdcopy_dstoffset"};

        const StrideType strideTypes[]     = {STRIDE_TYPE_VALID, STRIDE_TYPE_ZERO};
        const char *const strideTypesStr[] = {"", "_stride_zero"};

        const VkQueryPipelineStatisticFlags statisticsFlags = VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_VERTICES_BIT |
                                                              VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT;

        for (uint32_t partialFlagsIdx = 0u; partialFlagsIdx < DE_LENGTH_OF_ARRAY(partialFlags); partialFlagsIdx++)
        {
            for (uint32_t waitFlagsIdx = 0u; waitFlagsIdx < DE_LENGTH_OF_ARRAY(waitFlags); waitFlagsIdx++)
            {
                for (uint32_t copyTypesIdx = 0u; copyTypesIdx < DE_LENGTH_OF_ARRAY(copyTypes); copyTypesIdx++)
                {
                    for (uint32_t strideTypesIdx = 0u; strideTypesIdx < DE_LENGTH_OF_ARRAY(strideTypes);
                         strideTypesIdx++)
                    {
                        uint32_t dstOffset =
                            copyTypesIdx == 2u ? uint32_t(NUM_QUERY_STATISTICS * sizeof(uint64_t)) : uint32_t(0u);
                        /* Avoid waiting infinite time for the queries, when one of them is not going to be issued in
                         * the partial case.
                         */
                        if ((bool)(partialFlags[partialFlagsIdx] & VK_QUERY_RESULT_PARTIAL_BIT) &&
                            (bool)(waitFlags[waitFlagsIdx] & VK_QUERY_RESULT_WAIT_BIT))
                            continue;

                        // Skip stride bogus tests when there are more than one query count.
                        if (partialFlags[partialFlagsIdx] && strideTypes[strideTypesIdx] == STRIDE_TYPE_ZERO)
                            continue;

                        if (strideTypes[strideTypesIdx] == STRIDE_TYPE_ZERO && copyTypes[copyTypesIdx] != COPY_TYPE_CMD)
                            continue;

                        VkQueryResultFlags queryFlags = VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT |
                                                        partialFlags[partialFlagsIdx] | waitFlags[waitFlagsIdx];
                        uint32_t queryCount = partialFlagsIdx ? 2u : 1u;
                        {
                            std::ostringstream testName;
                            testName << "input_assembly_vertex_fragment" << partialFlagsStr[partialFlagsIdx]
                                     << waitFlagsStr[waitFlagsIdx] << copyTypesStr[copyTypesIdx]
                                     << strideTypesStr[strideTypesIdx];
                            GraphicBasicMultipleQueryTestInstance::ParametersGraphic param(
                                statisticsFlags | VK_QUERY_PIPELINE_STATISTIC_FRAGMENT_SHADER_INVOCATIONS_BIT,
                                queryFlags, queryCount, false, copyTypes[copyTypesIdx], dstOffset,
                                strideType[strideTypesIdx]);
                            vertexShaderMultipleQueries->addChild(
                                new QueryPoolGraphicMultipleQueryStatisticsTest<VertexShaderMultipleQueryTestInstance>(
                                    m_testCtx, testName.str().c_str(), param));
                        }

                        {
                            // No fragment shader case
                            std::ostringstream testName;
                            testName << "input_assembly_vertex" << partialFlagsStr[partialFlagsIdx]
                                     << waitFlagsStr[waitFlagsIdx] << copyTypesStr[copyTypesIdx]
                                     << strideTypesStr[strideTypesIdx];
                            GraphicBasicMultipleQueryTestInstance::ParametersGraphic param(
                                statisticsFlags | VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT, queryFlags,
                                queryCount, true, copyTypes[copyTypesIdx], dstOffset, strideType[strideTypesIdx]);
                            vertexShaderMultipleQueries->addChild(
                                new QueryPoolGraphicMultipleQueryStatisticsTest<VertexShaderMultipleQueryTestInstance>(
                                    m_testCtx, testName.str().c_str(), param));
                        }
                    }
                }
            }
        }
    }

    {
        for (const auto useCopy : {false, true})
            for (const auto useAvailability : {false, true})
                for (const auto useInheritance : {false, true})
                {
                    const std::string name = std::string(useCopy ? "copy" : "get") +
                                             (useAvailability ? "_with_availability" : "") +
                                             (useInheritance ? "_and_inheritance" : "");

                    const MultipleGeomStatsParams params{
                        useCopy,
                        useAvailability,
                        useInheritance,
                    };
                    multipleGeomStats->addChild(new MultipleGeomStatsTestCase(m_testCtx, name, params));
                }
    }

    addChild(computeShaderInvocationsGroup.release());
    addChild(inputAssemblyVertices.release());
    addChild(inputAssemblyPrimitives.release());
    addChild(vertexShaderInvocations.release());
    addChild(fragmentShaderInvocations.release());
    addChild(geometryShaderInvocations.release());
    addChild(geometryShaderPrimitives.release());
    addChild(clippingInvocations.release());
    addChild(clippingPrimitives.release());
    addChild(tesControlPatches.release());
    addChild(tesEvaluationShaderInvocations.release());

    vertexOnlyGroup->addChild(inputAssemblyVerticesVertexOnly.release());
    vertexOnlyGroup->addChild(inputAssemblyPrimitivesVertexOnly.release());
    vertexOnlyGroup->addChild(vertexShaderInvocationsVertexOnly.release());
    addChild(vertexOnlyGroup.release());

    hostQueryResetGroup->addChild(computeShaderInvocationsGroupHostQueryReset.release());
    hostQueryResetGroup->addChild(inputAssemblyVerticesHostQueryReset.release());
    hostQueryResetGroup->addChild(inputAssemblyPrimitivesHostQueryReset.release());
    hostQueryResetGroup->addChild(vertexShaderInvocationsHostQueryReset.release());
    hostQueryResetGroup->addChild(fragmentShaderInvocationsHostQueryReset.release());
    hostQueryResetGroup->addChild(geometryShaderInvocationsHostQueryReset.release());
    hostQueryResetGroup->addChild(geometryShaderPrimitivesHostQueryReset.release());
    hostQueryResetGroup->addChild(clippingInvocationsHostQueryReset.release());
    hostQueryResetGroup->addChild(clippingPrimitivesHostQueryReset.release());
    hostQueryResetGroup->addChild(tesControlPatchesHostQueryReset.release());
    hostQueryResetGroup->addChild(tesEvaluationShaderInvocationsHostQueryReset.release());
    addChild(hostQueryResetGroup.release());

    resetBeforeCopyGroup->addChild(computeShaderInvocationsGroupResetBeforeCopy.release());
    resetBeforeCopyGroup->addChild(inputAssemblyVerticesResetBeforeCopy.release());
    resetBeforeCopyGroup->addChild(inputAssemblyPrimitivesResetBeforeCopy.release());
    resetBeforeCopyGroup->addChild(vertexShaderInvocationsResetBeforeCopy.release());
    resetBeforeCopyGroup->addChild(fragmentShaderInvocationsResetBeforeCopy.release());
    resetBeforeCopyGroup->addChild(geometryShaderInvocationsResetBeforeCopy.release());
    resetBeforeCopyGroup->addChild(geometryShaderPrimitivesResetBeforeCopy.release());
    resetBeforeCopyGroup->addChild(clippingInvocationsResetBeforeCopy.release());
    resetBeforeCopyGroup->addChild(clippingPrimitivesResetBeforeCopy.release());
    resetBeforeCopyGroup->addChild(tesControlPatchesResetBeforeCopy.release());
    resetBeforeCopyGroup->addChild(tesEvaluationShaderInvocationsResetBeforeCopy.release());
    addChild(resetBeforeCopyGroup.release());

    resetAfterCopyGroup->addChild(computeShaderInvocationsGroupResetAfterCopy.release());
    resetAfterCopyGroup->addChild(inputAssemblyVerticesResetAfterCopy.release());
    resetAfterCopyGroup->addChild(inputAssemblyPrimitivesResetAfterCopy.release());
    resetAfterCopyGroup->addChild(vertexShaderInvocationsResetAfterCopy.release());
    resetAfterCopyGroup->addChild(fragmentShaderInvocationsResetAfterCopy.release());
    resetAfterCopyGroup->addChild(geometryShaderInvocationsResetAfterCopy.release());
    resetAfterCopyGroup->addChild(geometryShaderPrimitivesResetAfterCopy.release());
    resetAfterCopyGroup->addChild(clippingInvocationsResetAfterCopy.release());
    resetAfterCopyGroup->addChild(clippingPrimitivesResetAfterCopy.release());
    resetAfterCopyGroup->addChild(tesControlPatchesResetAfterCopy.release());
    resetAfterCopyGroup->addChild(tesEvaluationShaderInvocationsResetAfterCopy.release());
    addChild(resetAfterCopyGroup.release());

    addChild(vertexShaderMultipleQueries.release());
    addChild(multipleGeomStats.release());
}

} // namespace QueryPool
} // namespace vkt
