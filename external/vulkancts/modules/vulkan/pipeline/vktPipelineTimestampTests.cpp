/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2015 The Khronos Group Inc.
 * Copyright (c) 2015 ARM Ltd.
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
 * \brief Timestamp Tests
 *//*--------------------------------------------------------------------*/

#include "vktPipelineTimestampTests.hpp"
#include "vktPipelineClearUtil.hpp"
#include "vktPipelineImageUtil.hpp"
#include "vktPipelineVertexUtil.hpp"
#include "vktPipelineReferenceRenderer.hpp"
#include "vktCustomInstancesDevices.hpp"
#include "vktTestCaseUtil.hpp"
#include "vkSafetyCriticalUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkPrograms.hpp"
#include "vkBuilderUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkRef.hpp"
#include "vkRefUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkDeviceUtil.hpp"
#include "tcuImageCompare.hpp"
#include "tcuCommandLine.hpp"
#include "deUniquePtr.hpp"
#include "deStringUtil.hpp"
#include "deMemory.h"

#include <sstream>
#include <vector>
#include <set>
#include <cctype>
#include <locale>
#include <limits>
#include <thread>
#include <chrono>
#include <time.h>
#include <algorithm>

#ifdef CTS_USES_VULKANSC
// VulkanSC supports VK_EXT_calibrated_timestamps but not VK_KHR_calibrated_timestamps
#define VkCalibratedTimestampInfoKHR VkCalibratedTimestampInfoEXT
#define VkTimeDomainKHR VkTimeDomainEXT
#endif // CTS_USES_VULKANSC

#if (DE_OS == DE_OS_WIN32)
#define VC_EXTRALEAN
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

namespace vkt
{
namespace pipeline
{

using namespace vk;

namespace
{
typedef std::vector<VkPipelineStageFlagBits> StageFlagVector;

// helper functions
#define GEN_DESC_STRING(name, postfix)                                   \
    do                                                                   \
    {                                                                    \
        for (std::string::size_type ndx = 0; ndx < strlen(#name); ++ndx) \
            if (isDescription && #name[ndx] == '_')                      \
                desc << " ";                                             \
            else                                                         \
                desc << std::tolower(#name[ndx], loc);                   \
        if (isDescription)                                               \
            desc << " " << #postfix;                                     \
        else                                                             \
            desc << "_" << #postfix;                                     \
    } while (false)

std::string getPipelineStageFlagStr(const VkPipelineStageFlagBits stage, bool isDescription)
{
    std::ostringstream desc;
    std::locale loc;
    switch (stage)
    {
#define STAGE_CASE(p)                 \
    case VK_PIPELINE_STAGE_##p##_BIT: \
    {                                 \
        GEN_DESC_STRING(p, stage);    \
        break;                        \
    }
        STAGE_CASE(TOP_OF_PIPE)
        STAGE_CASE(DRAW_INDIRECT)
        STAGE_CASE(VERTEX_INPUT)
        STAGE_CASE(VERTEX_SHADER)
        STAGE_CASE(TESSELLATION_CONTROL_SHADER)
        STAGE_CASE(TESSELLATION_EVALUATION_SHADER)
        STAGE_CASE(GEOMETRY_SHADER)
        STAGE_CASE(FRAGMENT_SHADER)
        STAGE_CASE(EARLY_FRAGMENT_TESTS)
        STAGE_CASE(LATE_FRAGMENT_TESTS)
        STAGE_CASE(COLOR_ATTACHMENT_OUTPUT)
        STAGE_CASE(COMPUTE_SHADER)
        STAGE_CASE(TRANSFER)
        STAGE_CASE(HOST)
        STAGE_CASE(ALL_GRAPHICS)
        STAGE_CASE(ALL_COMMANDS)
#undef STAGE_CASE
    default:
        desc << "unknown stage!";
        DE_FATAL("Unknown Stage!");
        break;
    }

    return desc.str();
}

enum TransferMethod
{
    TRANSFER_METHOD_COPY_BUFFER = 0,
    TRANSFER_METHOD_COPY_IMAGE,
    TRANSFER_METHOD_BLIT_IMAGE,
    TRANSFER_METHOD_COPY_BUFFER_TO_IMAGE,
    TRANSFER_METHOD_COPY_IMAGE_TO_BUFFER,
    TRANSFER_METHOD_UPDATE_BUFFER,
    TRANSFER_METHOD_FILL_BUFFER,
    TRANSFER_METHOD_CLEAR_COLOR_IMAGE,
    TRANSFER_METHOD_CLEAR_DEPTH_STENCIL_IMAGE,
    TRANSFER_METHOD_RESOLVE_IMAGE,
    TRANSFER_METHOD_COPY_QUERY_POOL_RESULTS,
    TRANSFER_METHOD_COPY_QUERY_POOL_RESULTS_STRIDE_ZERO,
    TRANSFER_METHOD_LAST
};

std::string getTransferMethodStr(const TransferMethod method, bool isDescription)
{
    std::ostringstream desc;
    std::locale loc;

    switch (method)
    {
#define METHOD_CASE(p)              \
    case TRANSFER_METHOD_##p:       \
    {                               \
        GEN_DESC_STRING(p, method); \
        break;                      \
    }
        METHOD_CASE(COPY_BUFFER)
        METHOD_CASE(COPY_IMAGE)
        METHOD_CASE(BLIT_IMAGE)
        METHOD_CASE(COPY_BUFFER_TO_IMAGE)
        METHOD_CASE(COPY_IMAGE_TO_BUFFER)
        METHOD_CASE(UPDATE_BUFFER)
        METHOD_CASE(FILL_BUFFER)
        METHOD_CASE(CLEAR_COLOR_IMAGE)
        METHOD_CASE(CLEAR_DEPTH_STENCIL_IMAGE)
        METHOD_CASE(RESOLVE_IMAGE)
        METHOD_CASE(COPY_QUERY_POOL_RESULTS)
        METHOD_CASE(COPY_QUERY_POOL_RESULTS_STRIDE_ZERO)
#undef METHOD_CASE
    default:
        desc << "unknown method!";
        DE_FATAL("Unknown method!");
        break;
    }

    return desc.str();
}

constexpr uint32_t MIN_TIMESTAMP_VALID_BITS = 36;
constexpr uint32_t MAX_TIMESTAMP_VALID_BITS = 64;

// Checks the number of valid bits for the given queue meets the spec requirements.
void checkValidBits(uint32_t validBits, uint32_t queueFamilyIndex)
{
    if (validBits < MIN_TIMESTAMP_VALID_BITS || validBits > MAX_TIMESTAMP_VALID_BITS)
    {
        std::ostringstream msg;
        msg << "Invalid value for timestampValidBits (" << validBits << ") in queue index " << queueFamilyIndex;
        TCU_FAIL(msg.str());
    }
}

// Returns the timestamp mask given the number of valid timestamp bits.
uint64_t timestampMaskFromValidBits(uint32_t validBits)
{
    return ((validBits == MAX_TIMESTAMP_VALID_BITS) ? std::numeric_limits<uint64_t>::max() : ((1ULL << validBits) - 1));
}

// Checks support for timestamps and returns the timestamp mask.
uint64_t checkTimestampsSupported(const InstanceInterface &vki, const VkPhysicalDevice physDevice,
                                  const uint32_t queueFamilyIndex)
{
    const std::vector<VkQueueFamilyProperties> queueProperties =
        vk::getPhysicalDeviceQueueFamilyProperties(vki, physDevice);
    DE_ASSERT(queueFamilyIndex < queueProperties.size());
    const uint32_t &validBits = queueProperties[queueFamilyIndex].timestampValidBits;

    if (validBits == 0)
        throw tcu::NotSupportedError("Queue does not support timestamps");

    checkValidBits(validBits, queueFamilyIndex);
    return timestampMaskFromValidBits(validBits);
}

void checkTimestampBits(uint64_t timestamp, uint64_t mask)
{
    // The spec says:
    // timestampValidBits is the unsigned integer count of meaningful bits in
    // the timestamps written via vkCmdWriteTimestamp. The valid range for the
    // count is 36..64 bits, or a value of 0, indicating no support for
    // timestamps. Bits outside the valid range are guaranteed to be zeros.
    if (timestamp > mask)
    {
        std::ostringstream msg;
        msg << std::hex << "Invalid device timestamp value 0x" << timestamp << " according to device timestamp mask 0x"
            << mask;
        TCU_FAIL(msg.str());
    }
}

// helper classes
class TimestampTestParam
{
public:
    TimestampTestParam(const PipelineConstructionType pipelineConstructionType, const VkPipelineStageFlagBits *stages,
                       const uint32_t stageCount, const bool inRenderPass, const bool hostQueryReset,
                       const bool transferOnlyQueue, const VkQueryResultFlags queryResultFlags);
    virtual ~TimestampTestParam(void);
    virtual const std::string generateTestName(void) const;
    PipelineConstructionType getPipelineConstructionType(void) const
    {
        return m_pipelineConstructionType;
    }
    StageFlagVector getStageVector(void) const
    {
        return m_stageVec;
    }
    bool getInRenderPass(void) const
    {
        return m_inRenderPass;
    }
    bool getHostQueryReset(void) const
    {
        return m_hostQueryReset;
    }
    bool getTransferOnlyQueue(void) const
    {
        return m_transferOnlyQueue;
    }
    VkQueryResultFlags getQueryResultFlags(void) const
    {
        return m_queryResultFlags;
    }
    void toggleInRenderPass(void)
    {
        m_inRenderPass = !m_inRenderPass;
    }
    void toggleHostQueryReset(void)
    {
        m_hostQueryReset = !m_hostQueryReset;
    }

    void setQueryResultFlags(VkQueryResultFlags flags)
    {
        m_queryResultFlags = flags;
    }

protected:
    const PipelineConstructionType m_pipelineConstructionType;
    StageFlagVector m_stageVec;
    bool m_inRenderPass;
    bool m_hostQueryReset;
    bool m_transferOnlyQueue;
    VkQueryResultFlags m_queryResultFlags;
};

TimestampTestParam::TimestampTestParam(const PipelineConstructionType pipelineConstructionType,
                                       const VkPipelineStageFlagBits *stages, const uint32_t stageCount,
                                       const bool inRenderPass, const bool hostQueryReset, const bool transferOnlyQueue,
                                       const VkQueryResultFlags queryResultFlags)
    : m_pipelineConstructionType(pipelineConstructionType)
    , m_inRenderPass(inRenderPass)
    , m_hostQueryReset(hostQueryReset)
    , m_transferOnlyQueue(transferOnlyQueue)
    , m_queryResultFlags(queryResultFlags)
{
    for (uint32_t ndx = 0; ndx < stageCount; ndx++)
    {
        m_stageVec.push_back(stages[ndx]);
    }
}

TimestampTestParam::~TimestampTestParam(void)
{
}

const std::string TimestampTestParam::generateTestName(void) const
{
    std::string result("");

    for (StageFlagVector::const_iterator it = m_stageVec.begin(); it != m_stageVec.end(); it++)
    {
        if (*it != VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT)
        {
            result += getPipelineStageFlagStr(*it, false) + '_';
        }
    }
    if (m_inRenderPass)
        result += "in_render_pass";
    else
        result += "out_of_render_pass";

    if (m_hostQueryReset)
        result += "_host_query_reset";

    if (m_transferOnlyQueue)
        result += "_transfer_queue";

    if (m_queryResultFlags & VK_QUERY_RESULT_WITH_AVAILABILITY_BIT)
        result += "_with_availability_bit";

    return result;
}

class TransferTimestampTestParam : public TimestampTestParam
{
public:
    TransferTimestampTestParam(const PipelineConstructionType pipelineConstructionType,
                               const VkPipelineStageFlagBits *stages, const uint32_t stageCount,
                               const bool inRenderPass, const bool hostQueryReset, const bool transferOnlyQueue,
                               const uint32_t methodNdx, const VkQueryResultFlags flags);
    ~TransferTimestampTestParam(void)
    {
    }
    const std::string generateTestName(void) const;
    const std::string generateTestDescription(void) const;
    TransferMethod getMethod(void) const
    {
        return m_method;
    }

protected:
    TransferMethod m_method;
};

TransferTimestampTestParam::TransferTimestampTestParam(const PipelineConstructionType pipelineConstructionType,
                                                       const VkPipelineStageFlagBits *stages, const uint32_t stageCount,
                                                       const bool inRenderPass, const bool hostQueryReset,
                                                       const bool transferOnlyQueue, const uint32_t methodNdx,
                                                       const VkQueryResultFlags flags)
    : TimestampTestParam(pipelineConstructionType, stages, stageCount, inRenderPass, hostQueryReset, transferOnlyQueue,
                         flags)
{
    DE_ASSERT(methodNdx < (uint32_t)TRANSFER_METHOD_LAST);

    m_method = (TransferMethod)methodNdx;
}

const std::string TransferTimestampTestParam::generateTestName(void) const
{
    std::string result("");

    for (StageFlagVector::const_iterator it = m_stageVec.begin(); it != m_stageVec.end(); it++)
    {
        if (*it != VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT)
        {
            result += getPipelineStageFlagStr(*it, false) + '_';
        }
    }

    result += "with_" + getTransferMethodStr(m_method, false);

    if (m_hostQueryReset)
        result += "_host_query_reset";

    if (m_transferOnlyQueue)
        result += "_transfer_queue";

    if (m_queryResultFlags & VK_QUERY_RESULT_WITH_AVAILABILITY_BIT)
        result += "_with_availability_bit";

    return result;
}

class TwoCmdBuffersTestParam : public TimestampTestParam
{
public:
    TwoCmdBuffersTestParam(const PipelineConstructionType pipelineConstructionType,
                           const VkPipelineStageFlagBits *stages, const uint32_t stageCount, const bool inRenderPass,
                           const bool hostQueryReset, const bool transferOnlyQueue,
                           const VkCommandBufferLevel cmdBufferLevel, const VkQueryResultFlags queryPoolResultFlags);
    ~TwoCmdBuffersTestParam(void)
    {
    }
    VkCommandBufferLevel getCmdBufferLevel(void) const
    {
        return m_cmdBufferLevel;
    }

protected:
    VkCommandBufferLevel m_cmdBufferLevel;
};

TwoCmdBuffersTestParam::TwoCmdBuffersTestParam(const PipelineConstructionType pipelineConstructionType,
                                               const VkPipelineStageFlagBits *stages, const uint32_t stageCount,
                                               const bool inRenderPass, const bool hostQueryReset,
                                               const bool transferOnlyQueue, const VkCommandBufferLevel cmdBufferLevel,
                                               const VkQueryResultFlags queryPoolResultFlags)
    : TimestampTestParam(pipelineConstructionType, stages, stageCount, inRenderPass, hostQueryReset, transferOnlyQueue,
                         queryPoolResultFlags)
    , m_cmdBufferLevel(cmdBufferLevel)
{
}

template <class Test>
vkt::TestCase *newTestCase(tcu::TestContext &testContext, TimestampTestParam *testParam)
{
    return new Test(testContext, testParam->generateTestName().c_str(), testParam);
}

// Test Classes
class TimestampTest : public vkt::TestCase
{
public:
    enum
    {
        ENTRY_COUNT = 8
    };

    TimestampTest(tcu::TestContext &testContext, const std::string &name, const TimestampTestParam *param)
        : vkt::TestCase(testContext, name)
        , m_pipelineConstructionType(param->getPipelineConstructionType())
        , m_stages(param->getStageVector())
        , m_inRenderPass(param->getInRenderPass())
        , m_hostQueryReset(param->getHostQueryReset())
        , m_transferOnlyQueue(param->getTransferOnlyQueue())
        , m_queryResultFlags(param->getQueryResultFlags())
    {
    }
    virtual ~TimestampTest(void)
    {
    }
    virtual void initPrograms(SourceCollections &programCollection) const;
    virtual TestInstance *createInstance(Context &context) const;
    virtual void checkSupport(Context &context) const;

protected:
    const PipelineConstructionType m_pipelineConstructionType;
    const StageFlagVector m_stages;
    const bool m_inRenderPass;
    const bool m_hostQueryReset;
    const bool m_transferOnlyQueue;
    const VkQueryResultFlags m_queryResultFlags;
};

class TimestampTestInstance : public vkt::TestInstance
{
public:
    TimestampTestInstance(Context &context, const StageFlagVector &stages, const bool inRenderPass,
                          const bool hostQueryReset, const bool transferOnlyQueue,
                          const VkQueryResultFlags queryResultFlags);

    virtual ~TimestampTestInstance(void);
    virtual tcu::TestStatus iterate(void);

protected:
    virtual tcu::TestStatus verifyTimestamp(void);
    virtual void buildPipeline(void);
    virtual void configCommandBuffer(void);

    Move<VkBuffer> createBufferAndBindMemory(VkDeviceSize size, VkBufferUsageFlags usage,
                                             de::MovePtr<Allocation> *pAlloc);

    Move<VkImage> createImage2DAndBindMemory(VkFormat format, uint32_t width, uint32_t height, VkImageUsageFlags usage,
                                             VkSampleCountFlagBits sampleCount, de::MovePtr<Allocation> *pAlloc);

    // Creates a device that has transfer only operations
    void createCustomDeviceWithTransferOnlyQueue(void);

protected:
#ifdef CTS_USES_VULKANSC
    const CustomInstance m_customInstance;
#endif // CTS_USES_VULKANSC
    Move<VkDevice> m_customDevice;
    de::MovePtr<Allocator> m_customAllocator;

    VkDevice m_device;
    Allocator *m_allocator;
    uint32_t m_queueFamilyIndex;

    const StageFlagVector m_stages;
    bool m_inRenderPass;
    bool m_hostQueryReset;
    bool m_transferOnlyQueue;
    VkQueryResultFlags m_queryResultFlags;

    Move<VkCommandPool> m_cmdPool;
    Move<VkCommandBuffer> m_cmdBuffer;
    Move<VkQueryPool> m_queryPool;
    uint64_t *m_timestampValues;
    uint64_t *m_timestampValuesHostQueryReset;
    uint64_t m_timestampMask;
};

void TimestampTest::initPrograms(SourceCollections &programCollection) const
{
    vkt::TestCase::initPrograms(programCollection);
}

void TimestampTest::checkSupport(Context &context) const
{
    const InstanceInterface &vki          = context.getInstanceInterface();
    const VkPhysicalDevice physicalDevice = context.getPhysicalDevice();
    uint32_t queueFamilyIndex             = context.getUniversalQueueFamilyIndex();

    if (m_transferOnlyQueue)
        queueFamilyIndex = findQueueFamilyIndexWithCaps(vki, physicalDevice, VK_QUEUE_TRANSFER_BIT,
                                                        VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT);

    // Check support for timestamp queries
    const std::vector<VkQueueFamilyProperties> queueProperties =
        vk::getPhysicalDeviceQueueFamilyProperties(context.getInstanceInterface(), context.getPhysicalDevice());
    DE_ASSERT(queueFamilyIndex < (uint32_t)queueProperties.size());
    if (!queueProperties[queueFamilyIndex].timestampValidBits)
        throw tcu::NotSupportedError("Universal queue does not support timestamps");

    if (m_hostQueryReset)
    {
        // Check VK_EXT_host_query_reset is supported
        context.requireDeviceFunctionality("VK_EXT_host_query_reset");

        if (context.getHostQueryResetFeatures().hostQueryReset == VK_FALSE)
            throw tcu::NotSupportedError("Implementation doesn't support resetting queries from the host");
    }
    checkPipelineConstructionRequirements(context.getInstanceInterface(), context.getPhysicalDevice(),
                                          m_pipelineConstructionType);
}

TestInstance *TimestampTest::createInstance(Context &context) const
{
    return new TimestampTestInstance(context, m_stages, m_inRenderPass, m_hostQueryReset, false, m_queryResultFlags);
}

TimestampTestInstance::TimestampTestInstance(Context &context, const StageFlagVector &stages, const bool inRenderPass,
                                             const bool hostQueryReset, const bool transferOnlyQueue,
                                             const VkQueryResultFlags queryResultFlags)
    : TestInstance(context)
#ifdef CTS_USES_VULKANSC
    , m_customInstance(createCustomInstanceFromContext(context))
#endif // CTS_USES_VULKANSC
    , m_customDevice()
    , m_customAllocator()
    , m_device(VK_NULL_HANDLE)
    , m_allocator(nullptr)
    , m_queueFamilyIndex(std::numeric_limits<uint32_t>::max())
    , m_stages(stages)
    , m_inRenderPass(inRenderPass)
    , m_hostQueryReset(hostQueryReset)
    , m_transferOnlyQueue(transferOnlyQueue)
    , m_queryResultFlags(queryResultFlags)
{
    const DeviceInterface &vk = context.getDeviceInterface();

    m_device           = context.getDevice();
    m_allocator        = &context.getDefaultAllocator();
    m_queueFamilyIndex = context.getUniversalQueueFamilyIndex();

    // when needed create custom device and overwrite m_device, m_allocator and m_queueFamilyIndex
    if (m_transferOnlyQueue)
        createCustomDeviceWithTransferOnlyQueue();

    m_timestampMask =
        checkTimestampsSupported(context.getInstanceInterface(), context.getPhysicalDevice(), m_queueFamilyIndex);

    // Create Query Pool
    {
        const VkQueryPoolCreateInfo queryPoolParams = {
            VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO, // VkStructureType               sType;
            nullptr,                                  // const void*                   pNext;
            0u,                                       // VkQueryPoolCreateFlags        flags;
            VK_QUERY_TYPE_TIMESTAMP,                  // VkQueryType                   queryType;
            TimestampTest::ENTRY_COUNT,               // uint32_t                      entryCount;
            0u,                                       // VkQueryPipelineStatisticFlags pipelineStatistics;
        };

        m_queryPool = createQueryPool(vk, m_device, &queryPoolParams);
    }

    // Create command pool
    m_cmdPool = createCommandPool(vk, m_device, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, m_queueFamilyIndex);

    // Create command buffer
    m_cmdBuffer = allocateCommandBuffer(vk, m_device, *m_cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

    // alloc timestamp values
    m_timestampValues =
        new uint64_t[m_stages.size() * ((m_queryResultFlags & VK_QUERY_RESULT_WITH_AVAILABILITY_BIT) ? 2u : 1u)];

    if (m_hostQueryReset)
        m_timestampValuesHostQueryReset = new uint64_t[m_stages.size() * 2];
    else
        m_timestampValuesHostQueryReset = nullptr;
}

TimestampTestInstance::~TimestampTestInstance(void)
{
    delete[] m_timestampValues;
    m_timestampValues = NULL;

    delete[] m_timestampValuesHostQueryReset;
    m_timestampValuesHostQueryReset = NULL;
}

void TimestampTestInstance::buildPipeline(void)
{
}

void TimestampTestInstance::configCommandBuffer(void)
{
    const DeviceInterface &vk = m_context.getDeviceInterface();

    beginCommandBuffer(vk, *m_cmdBuffer, 0u);

    if (!m_hostQueryReset)
        vk.cmdResetQueryPool(*m_cmdBuffer, *m_queryPool, 0u, TimestampTest::ENTRY_COUNT);

    uint32_t timestampEntry = 0;
    for (const auto &stage : m_stages)
    {
        vk.cmdWriteTimestamp(*m_cmdBuffer, stage, *m_queryPool, timestampEntry++);
    }

    endCommandBuffer(vk, *m_cmdBuffer);
}

tcu::TestStatus TimestampTestInstance::iterate(void)
{
    const DeviceInterface &vk    = m_context.getDeviceInterface();
    const VkQueue queue          = getDeviceQueue(vk, m_device, m_queueFamilyIndex, 0);
    const bool availabilityBit   = m_queryResultFlags & VK_QUERY_RESULT_WITH_AVAILABILITY_BIT;
    const uint32_t stageSize     = (uint32_t)m_stages.size();
    const uint32_t queryDataSize = uint32_t(sizeof(uint64_t) * (availabilityBit ? 2u : 1u));

    buildPipeline();
    configCommandBuffer();
    if (m_hostQueryReset)
    {
        vk.resetQueryPool(m_device, *m_queryPool, 0u, TimestampTest::ENTRY_COUNT);
    }
    submitCommandsAndWait(vk, m_device, queue, m_cmdBuffer.get());

    // Get timestamp value from query pool
    VK_CHECK(vk.getQueryPoolResults(m_device, *m_queryPool, 0u, stageSize, queryDataSize * stageSize,
                                    (void *)m_timestampValues, queryDataSize, m_queryResultFlags));

    for (uint32_t ndx = 0; ndx < stageSize; ndx++)
    {
        m_timestampValues[ndx] &= m_timestampMask;
        if (availabilityBit)
            ndx++;
    }

    if (m_hostQueryReset)
    {
        // Initialize timestampValuesHostQueryReset values
        deMemset(m_timestampValuesHostQueryReset, 0, sizeof(uint64_t) * stageSize * 2);

        for (uint32_t ndx = 0; ndx < stageSize; ndx++)
        {
            const uint32_t ndxTimestampValue         = ndx * (availabilityBit ? 2u : 1u);
            m_timestampValuesHostQueryReset[2 * ndx] = m_timestampValues[ndxTimestampValue];
        }

        // Host resets the query pool
        vk.resetQueryPool(m_device, *m_queryPool, 0u, stageSize);
        // Get timestamp value from query pool
        vk::VkResult res =
            vk.getQueryPoolResults(m_device, *m_queryPool, 0u, stageSize, sizeof(uint64_t) * stageSize * 2,
                                   (void *)m_timestampValuesHostQueryReset, sizeof(uint64_t) * 2,
                                   VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT);

        /* From Vulkan spec:
         *
         * If VK_QUERY_RESULT_WAIT_BIT and VK_QUERY_RESULT_PARTIAL_BIT are both not set then no result values are written to pData
         * for queries that are in the unavailable state at the time of the call, and vkGetQueryPoolResults returns VK_NOT_READY.
         * However, availability state is still written to pData for those queries if VK_QUERY_RESULT_WITH_AVAILABILITY_BIT is set.
         */
        if (res != vk::VK_NOT_READY)
            return tcu::TestStatus::fail("QueryPoolResults incorrect reset");

        for (uint32_t ndx = 0; ndx < stageSize; ndx++)
        {
            const uint32_t ndxTimestampValue = ndx * (availabilityBit ? 2u : 1u);
            if ((m_timestampValuesHostQueryReset[2 * ndx] & m_timestampMask) != m_timestampValues[ndxTimestampValue])
                return tcu::TestStatus::fail("QueryPoolResults returned value was modified");
            if (m_timestampValuesHostQueryReset[2 * ndx + 1] != 0u)
                return tcu::TestStatus::fail("QueryPoolResults availability status is not zero");
        }
    }

    return verifyTimestamp();
}

tcu::TestStatus TimestampTestInstance::verifyTimestamp(void)
{
    bool availabilityBit = m_queryResultFlags & VK_QUERY_RESULT_WITH_AVAILABILITY_BIT;
    uint32_t increment   = availabilityBit ? 2u : 1u;
    for (uint32_t first = 0u; first < m_stages.size(); first += increment)
    {
        for (uint32_t second = 0u; second < first; second += increment)
        {
            if (availabilityBit && (m_timestampValues[first + 1u] == 0u || m_timestampValues[second + 1u] == 0u))
            {
                return tcu::TestStatus::fail("Timestamp query not available");
            }

            if (m_timestampValues[first] < m_timestampValues[second])
            {
                return tcu::TestStatus::fail("Latter stage timestamp is smaller than the former stage timestamp.");
            }
        }
    }

    return tcu::TestStatus::pass("Timestamp increases steadily.");
}

Move<VkBuffer> TimestampTestInstance::createBufferAndBindMemory(VkDeviceSize size, VkBufferUsageFlags usage,
                                                                de::MovePtr<Allocation> *pAlloc)
{
    const DeviceInterface &vk                   = m_context.getDeviceInterface();
    const VkBufferCreateInfo vertexBufferParams = {
        VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, // VkStructureType      sType;
        nullptr,                              // const void*          pNext;
        0u,                                   // VkBufferCreateFlags  flags;
        size,                                 // VkDeviceSize         size;
        usage,                                // VkBufferUsageFlags   usage;
        VK_SHARING_MODE_EXCLUSIVE,            // VkSharingMode        sharingMode;
        1u,                                   // uint32_t             queueFamilyCount;
        &m_queueFamilyIndex                   // const uint32_t*      pQueueFamilyIndices;
    };

    Move<VkBuffer> vertexBuffer = createBuffer(vk, m_device, &vertexBufferParams);
    de::MovePtr<Allocation> vertexBufferAlloc =
        m_allocator->allocate(getBufferMemoryRequirements(vk, m_device, *vertexBuffer), MemoryRequirement::HostVisible);

    VK_CHECK(
        vk.bindBufferMemory(m_device, *vertexBuffer, vertexBufferAlloc->getMemory(), vertexBufferAlloc->getOffset()));

    DE_ASSERT(pAlloc);
    *pAlloc = vertexBufferAlloc;

    return vertexBuffer;
}

Move<VkImage> TimestampTestInstance::createImage2DAndBindMemory(VkFormat format, uint32_t width, uint32_t height,
                                                                VkImageUsageFlags usage,
                                                                VkSampleCountFlagBits sampleCount,
                                                                de::details::MovePtr<Allocation> *pAlloc)
{
    const DeviceInterface &vk = m_context.getDeviceInterface();

    // Optimal tiling feature check
    VkFormatProperties formatProperty;

    m_context.getInstanceInterface().getPhysicalDeviceFormatProperties(m_context.getPhysicalDevice(), format,
                                                                       &formatProperty);

    if ((usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) &&
        !(formatProperty.optimalTilingFeatures & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT))
    {
        // Remove color attachment usage if the optimal tiling feature does not support it
        usage &= ~VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    }
    if ((usage & VK_IMAGE_USAGE_STORAGE_BIT) &&
        !(formatProperty.optimalTilingFeatures & VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT))
    {
        // Remove storage usage if the optimal tiling feature does not support it
        usage &= ~VK_IMAGE_USAGE_STORAGE_BIT;
    }

    const VkImageCreateInfo colorImageParams = {
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, // VkStructureType      sType;
        nullptr,                             // const void*          pNext;
        0u,                                  // VkImageCreateFlags   flags;
        VK_IMAGE_TYPE_2D,                    // VkImageType          imageType;
        format,                              // VkFormat             format;
        {width, height, 1u},                 // VkExtent3D           extent;
        1u,                                  // uint32_t             mipLevels;
        1u,                                  // uint32_t             arraySize;
        sampleCount,                         // uint32_t             samples;
        VK_IMAGE_TILING_OPTIMAL,             // VkImageTiling        tiling;
        usage,                               // VkImageUsageFlags    usage;
        VK_SHARING_MODE_EXCLUSIVE,           // VkSharingMode        sharingMode;
        1u,                                  // uint32_t             queueFamilyCount;
        &m_queueFamilyIndex,                 // const uint32_t*      pQueueFamilyIndices;
        VK_IMAGE_LAYOUT_UNDEFINED,           // VkImageLayout        initialLayout;
    };

    Move<VkImage> image = createImage(vk, m_device, &colorImageParams);

    // Allocate and bind image memory
    de::MovePtr<Allocation> colorImageAlloc =
        m_allocator->allocate(getImageMemoryRequirements(vk, m_device, *image), MemoryRequirement::Any);
    VK_CHECK(vk.bindImageMemory(m_device, *image, colorImageAlloc->getMemory(), colorImageAlloc->getOffset()));

    DE_ASSERT(pAlloc);
    *pAlloc = colorImageAlloc;

    return image;
}

void TimestampTestInstance::createCustomDeviceWithTransferOnlyQueue(void)
{
#ifdef CTS_USES_VULKANSC
    vk::VkInstance instance          = m_customInstance;
    const vk::InstanceInterface &vki = m_customInstance.getDriver();
    const VkPhysicalDevice physicalDevice =
        chooseDevice(vki, m_customInstance, m_context.getTestContext().getCommandLine());
#else
    vk::VkInstance instance               = m_context.getInstance();
    const vk::InstanceInterface &vki      = m_context.getInstanceInterface();
    const VkPhysicalDevice physicalDevice = m_context.getPhysicalDevice();
#endif // CTS_USES_VULKANSC

    const DeviceInterface &vk = m_context.getDeviceInterface();

    m_queueFamilyIndex = findQueueFamilyIndexWithCaps(vki, physicalDevice, VK_QUEUE_TRANSFER_BIT,
                                                      VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT);

    const std::vector<VkQueueFamilyProperties> queueFamilies =
        getPhysicalDeviceQueueFamilyProperties(vki, physicalDevice);

    // This must be found, findQueueFamilyIndexWithCaps would have
    // thrown a NotSupported exception if the requested queue type did
    // not exist. Similarly, this was written with the assumption the
    // "alternative" queue would be different to the universal queue.
    DE_ASSERT(m_queueFamilyIndex < queueFamilies.size() &&
              m_queueFamilyIndex != m_context.getUniversalQueueFamilyIndex());
    const float queuePriority = 1.0f;
    const VkDeviceQueueCreateInfo deviceQueueCreateInfos{
        VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, // VkStructureType sType;
        nullptr,                                    // const void* pNext;
        (VkDeviceQueueCreateFlags)0u,               // VkDeviceQueueCreateFlags flags;
        m_queueFamilyIndex,                         // uint32_t queueFamilyIndex;
        1u,                                         // uint32_t queueCount;
        &queuePriority,                             // const float* pQueuePriorities;
    };

    // Replicate default device extension list.
    const auto extensionNames = m_context.getDeviceCreationExtensions();
    auto queryResetFeatures   = m_context.getHostQueryResetFeatures();
    auto deviceFeatures2      = m_context.getDeviceFeatures2();

    const void *pNext = &deviceFeatures2;
    if (m_context.getUsedApiVersion() < VK_API_VERSION_1_2)
    {
        queryResetFeatures.pNext = &deviceFeatures2;
        pNext                    = &queryResetFeatures;
    }

#ifdef CTS_USES_VULKANSC
    VkDeviceObjectReservationCreateInfo memReservationInfo =
        m_context.getTestContext().getCommandLine().isSubProcess() ? m_context.getResourceInterface()->getStatMax() :
                                                                     resetDeviceObjectReservationCreateInfo();
    memReservationInfo.pNext = pNext;
    pNext                    = &memReservationInfo;

    VkPipelineCacheCreateInfo pcCI;
    std::vector<VkPipelinePoolSize> poolSizes;
    if (m_context.getTestContext().getCommandLine().isSubProcess())
    {
        if (m_context.getResourceInterface()->getCacheDataSize() > 0)
        {
            pcCI = {
                VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO, // VkStructureType sType;
                nullptr,                                      // const void* pNext;
                VK_PIPELINE_CACHE_CREATE_READ_ONLY_BIT |
                    VK_PIPELINE_CACHE_CREATE_USE_APPLICATION_STORAGE_BIT, // VkPipelineCacheCreateFlags flags;
                m_context.getResourceInterface()->getCacheDataSize(),     // uintptr_t initialDataSize;
                m_context.getResourceInterface()->getCacheData()          // const void* pInitialData;
            };
            memReservationInfo.pipelineCacheCreateInfoCount = 1;
            memReservationInfo.pPipelineCacheCreateInfos    = &pcCI;
        }
        poolSizes = m_context.getResourceInterface()->getPipelinePoolSizes();
        if (!poolSizes.empty())
        {
            memReservationInfo.pipelinePoolSizeCount = uint32_t(poolSizes.size());
            memReservationInfo.pPipelinePoolSizes    = poolSizes.data();
        }
    }
#endif // CTS_USES_VULKANSC

    const VkDeviceCreateInfo deviceCreateInfo{
        VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,         // VkStructureType sType;
        pNext,                                        // const void* pNext;
        (VkDeviceCreateFlags)0u,                      // VkDeviceCreateFlags flags;
        1u,                                           // uint32_t queueCreateInfoCount;
        &deviceQueueCreateInfos,                      // const VkDeviceQueueCreateInfo* pQueueCreateInfos;
        0u,                                           // uint32_t enabledLayerCount;
        nullptr,                                      // const char* const* ppEnabledLayerNames;
        static_cast<uint32_t>(extensionNames.size()), // uint32_t enabledExtensionCount;
        extensionNames.data(),                        // const char* const* ppEnabledExtensionNames;
        nullptr,                                      // const VkPhysicalDeviceFeatures* pEnabledFeatures;
    };

    m_customDevice =
        vkt::createCustomDevice(m_context.getTestContext().getCommandLine().isValidationEnabled(),
                                m_context.getPlatformInterface(), instance, vki, physicalDevice, &deviceCreateInfo);
    m_customAllocator = de::MovePtr<Allocator>(
        new SimpleAllocator(vk, *m_customDevice, getPhysicalDeviceMemoryProperties(vki, physicalDevice)));

    m_device    = *m_customDevice;
    m_allocator = &(*m_customAllocator);
}

template <class T>
class CalibratedTimestampTest : public vkt::TestCase
{
public:
    CalibratedTimestampTest(tcu::TestContext &testContext, const std::string &name) : vkt::TestCase{testContext, name}
    {
    }

    virtual ~CalibratedTimestampTest(void) override
    {
    }
    virtual void initPrograms(SourceCollections &programCollection) const override;
    virtual void checkSupport(Context &context) const override;
    virtual vkt::TestInstance *createInstance(Context &context) const override;
};

class CalibratedTimestampTestInstance : public vkt::TestInstance
{
public:
    CalibratedTimestampTestInstance(Context &context);
    virtual ~CalibratedTimestampTestInstance(void) override
    {
    }
    virtual tcu::TestStatus iterate(void) override;
    virtual tcu::TestStatus runTest(void) = 0;

protected:
    struct CalibratedTimestamp
    {
        CalibratedTimestamp(uint64_t timestamp_, uint64_t deviation_) : timestamp{timestamp_}, deviation(deviation_)
        {
        }
        CalibratedTimestamp() : timestamp{}, deviation{}
        {
        }
        uint64_t timestamp;
        uint64_t deviation;
    };

    std::vector<VkTimeDomainKHR> getDomainSubset(const std::vector<VkTimeDomainKHR> &available,
                                                 const std::vector<VkTimeDomainKHR> &interesting) const;
    std::string domainName(VkTimeDomainKHR domain) const;
    uint64_t getHostNativeTimestamp(VkTimeDomainKHR hostDomain) const;
    uint64_t getHostNanoseconds(uint64_t hostTimestamp) const;
    uint64_t getDeviceNanoseconds(uint64_t devTicksDelta) const;
    std::vector<CalibratedTimestamp> getCalibratedTimestamps(const std::vector<VkTimeDomainKHR> &domains);
    CalibratedTimestamp getCalibratedTimestamp(VkTimeDomainKHR domain);
    void appendQualityMessage(const std::string &message);

    void verifyDevTimestampMask(uint64_t value) const;
    uint64_t absDiffWithOverflow(uint64_t a, uint64_t b, uint64_t mask = std::numeric_limits<uint64_t>::max()) const;
    uint64_t positiveDiffWithOverflow(uint64_t before, uint64_t after,
                                      uint64_t mask = std::numeric_limits<uint64_t>::max()) const;
    bool outOfRange(uint64_t begin, uint64_t middle, uint64_t end) const;

    static constexpr uint64_t kBatchTimeLimitNanos        = 1000000000u; // 1 sec.
    static constexpr uint64_t kDeviationErrorLimitNanos   = 100000000u;  // 100 ms.
    static constexpr uint64_t kDeviationWarningLimitNanos = 50000000u;   // 50 ms.
    static constexpr uint64_t kDefaultToleranceNanos      = 100000000u;  // 100 ms.

#if (DE_OS == DE_OS_WIN32)
    // Preprocessor used to avoid warning about unused variable.
    static constexpr uint64_t kNanosecondsPerSecond = 1000000000u;
#endif
    static constexpr uint64_t kNanosecondsPerMillisecond = 1000000u;

    std::string m_qualityMessage;
    float m_timestampPeriod;
    std::vector<VkTimeDomainKHR> m_devDomains;
    std::vector<VkTimeDomainKHR> m_hostDomains;
#if (DE_OS == DE_OS_WIN32)
    uint64_t m_frequency;
#endif

    Move<VkCommandPool> m_cmdPool;
    Move<VkCommandBuffer> m_cmdBuffer;
    Move<VkQueryPool> m_queryPool;
    uint64_t m_devTimestampMask;
};

class CalibratedTimestampDevDomainTestInstance : public CalibratedTimestampTestInstance
{
public:
    CalibratedTimestampDevDomainTestInstance(Context &context) : CalibratedTimestampTestInstance{context}
    {
    }

    virtual ~CalibratedTimestampDevDomainTestInstance(void)
    {
    }
    virtual tcu::TestStatus runTest(void) override;
};

class CalibratedTimestampHostDomainTestInstance : public CalibratedTimestampTestInstance
{
public:
    CalibratedTimestampHostDomainTestInstance(Context &context) : CalibratedTimestampTestInstance{context}
    {
    }

    virtual ~CalibratedTimestampHostDomainTestInstance(void)
    {
    }
    virtual tcu::TestStatus runTest(void) override;
};

class CalibratedTimestampCalibrationTestInstance : public CalibratedTimestampTestInstance
{
public:
    CalibratedTimestampCalibrationTestInstance(Context &context) : CalibratedTimestampTestInstance{context}
    {
    }

    virtual ~CalibratedTimestampCalibrationTestInstance(void)
    {
    }
    virtual tcu::TestStatus runTest(void) override;
};

template <class T>
void CalibratedTimestampTest<T>::initPrograms(SourceCollections &programCollection) const
{
    vkt::TestCase::initPrograms(programCollection);
}

template <class T>
vkt::TestInstance *CalibratedTimestampTest<T>::createInstance(Context &context) const
{
    return new T{context};
}

template <class T>
void CalibratedTimestampTest<T>::checkSupport(Context &context) const
{
#ifdef CTS_USES_VULKANSC
    context.requireDeviceFunctionality("VK_EXT_calibrated_timestamps");
#else
    if (!context.isDeviceFunctionalitySupported("VK_KHR_calibrated_timestamps") &&
        !context.isDeviceFunctionalitySupported("VK_EXT_calibrated_timestamps"))
        TCU_THROW(NotSupportedError, "VK_KHR_calibrated_timestamps and VK_EXT_calibrated_timestamps are not supported");
#endif
}

CalibratedTimestampTestInstance::CalibratedTimestampTestInstance(Context &context) : TestInstance{context}
{
#if (DE_OS == DE_OS_WIN32)
    LARGE_INTEGER freq;
    if (!QueryPerformanceFrequency(&freq))
    {
        throw tcu::ResourceError("Unable to get clock frequency with QueryPerformanceFrequency");
    }
    if (freq.QuadPart <= 0)
    {
        throw tcu::ResourceError("QueryPerformanceFrequency did not return a positive number");
    }
    m_frequency = static_cast<uint64_t>(freq.QuadPart);
#endif

    const InstanceInterface &vki      = context.getInstanceInterface();
    const VkPhysicalDevice physDevice = context.getPhysicalDevice();
    const uint32_t queueFamilyIndex   = context.getUniversalQueueFamilyIndex();

    // Get timestamp mask.
    m_devTimestampMask = checkTimestampsSupported(vki, physDevice, queueFamilyIndex);

    // Get calibreatable time domains.
    m_timestampPeriod = getPhysicalDeviceProperties(vki, physDevice).limits.timestampPeriod;

    uint32_t domainCount;
    VK_CHECK(vki.getPhysicalDeviceCalibrateableTimeDomainsKHR(physDevice, &domainCount, nullptr));
    if (domainCount == 0)
    {
        throw tcu::NotSupportedError("No calibrateable time domains found");
    }

    std::vector<VkTimeDomainKHR> domains;
    domains.resize(domainCount);
    VK_CHECK(vki.getPhysicalDeviceCalibrateableTimeDomainsKHR(physDevice, &domainCount, domains.data()));

    // Find the dev domain.
    std::vector<VkTimeDomainKHR> preferredDevDomains;
    preferredDevDomains.push_back(VK_TIME_DOMAIN_DEVICE_KHR);
    m_devDomains = getDomainSubset(domains, preferredDevDomains);

    // Find the host domain.
    std::vector<VkTimeDomainKHR> preferredHostDomains;
#if (DE_OS == DE_OS_WIN32)
    preferredHostDomains.push_back(VK_TIME_DOMAIN_QUERY_PERFORMANCE_COUNTER_KHR);
#else
    preferredHostDomains.push_back(VK_TIME_DOMAIN_CLOCK_MONOTONIC_RAW_KHR);
    preferredHostDomains.push_back(VK_TIME_DOMAIN_CLOCK_MONOTONIC_KHR);
#endif
    m_hostDomains = getDomainSubset(domains, preferredHostDomains);

    // Initialize command buffers and queries.
    const DeviceInterface &vk = context.getDeviceInterface();
    const VkDevice vkDevice   = context.getDevice();

    const VkQueryPoolCreateInfo queryPoolParams = {
        VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO, // VkStructureType               sType;
        nullptr,                                  // const void*                   pNext;
        0u,                                       // VkQueryPoolCreateFlags        flags;
        VK_QUERY_TYPE_TIMESTAMP,                  // VkQueryType                   queryType;
        1u,                                       // uint32_t                      entryCount;
        0u,                                       // VkQueryPipelineStatisticFlags pipelineStatistics;
    };

    m_queryPool = createQueryPool(vk, vkDevice, &queryPoolParams);
    m_cmdPool   = createCommandPool(vk, vkDevice, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, queueFamilyIndex);
    m_cmdBuffer = allocateCommandBuffer(vk, vkDevice, *m_cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

    beginCommandBuffer(vk, *m_cmdBuffer, 0u);
    vk.cmdResetQueryPool(*m_cmdBuffer, *m_queryPool, 0u, 1u);
    vk.cmdWriteTimestamp(*m_cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, *m_queryPool, 0u);
    endCommandBuffer(vk, *m_cmdBuffer);
}

std::vector<VkTimeDomainKHR> CalibratedTimestampTestInstance::getDomainSubset(
    const std::vector<VkTimeDomainKHR> &available, const std::vector<VkTimeDomainKHR> &interesting) const
{
    const std::set<VkTimeDomainKHR> availableSet(begin(available), end(available));
    const std::set<VkTimeDomainKHR> interestingSet(begin(interesting), end(interesting));

    std::vector<VkTimeDomainKHR> subset;
    std::set_intersection(begin(availableSet), end(availableSet), begin(interestingSet), end(interestingSet),
                          std::back_inserter(subset));
    return subset;
}

std::string CalibratedTimestampTestInstance::domainName(VkTimeDomainKHR domain) const
{
    switch (domain)
    {
    case VK_TIME_DOMAIN_DEVICE_KHR:
        return "Device Domain";
    case VK_TIME_DOMAIN_CLOCK_MONOTONIC_KHR:
        return "Monotonic Clock";
    case VK_TIME_DOMAIN_CLOCK_MONOTONIC_RAW_KHR:
        return "Raw Monotonic Clock";
    case VK_TIME_DOMAIN_QUERY_PERFORMANCE_COUNTER_KHR:
        return "Query Performance Counter";
    default:
        DE_ASSERT(false);
        return "Unknown Time Domain";
    }
}

uint64_t CalibratedTimestampTestInstance::getHostNativeTimestamp(VkTimeDomainKHR hostDomain) const
{
#if (DE_OS == DE_OS_WIN32)
    DE_ASSERT(hostDomain == VK_TIME_DOMAIN_QUERY_PERFORMANCE_COUNTER_KHR);
    LARGE_INTEGER result;
    if (!QueryPerformanceCounter(&result))
    {
        throw tcu::ResourceError("Unable to obtain host native timestamp for Win32");
    }
    if (result.QuadPart < 0)
    {
        throw tcu::ResourceError("Host-native timestamp for Win32 less than zero");
    }
    return static_cast<uint64_t>(result.QuadPart);
#else
    DE_ASSERT(hostDomain == VK_TIME_DOMAIN_CLOCK_MONOTONIC_KHR || hostDomain == VK_TIME_DOMAIN_CLOCK_MONOTONIC_RAW_KHR);

#if defined(CLOCK_MONOTONIC_RAW)
    clockid_t id = ((hostDomain == VK_TIME_DOMAIN_CLOCK_MONOTONIC_KHR) ? CLOCK_MONOTONIC : CLOCK_MONOTONIC_RAW);
#else
    clockid_t id = CLOCK_MONOTONIC;
#endif
    struct timespec ts;
    if (clock_gettime(id, &ts) != 0)
    {
        throw tcu::ResourceError("Unable to obtain host native timestamp for POSIX");
    }
    return (static_cast<uint64_t>(ts.tv_sec) * 1000000000ULL + ts.tv_nsec);
#endif
}

uint64_t CalibratedTimestampTestInstance::getHostNanoseconds(uint64_t hostTimestamp) const
{
#if (DE_OS == DE_OS_WIN32)
    uint64_t secs  = hostTimestamp / m_frequency;
    uint64_t nanos = ((hostTimestamp % m_frequency) * kNanosecondsPerSecond) / m_frequency;

    return ((secs * kNanosecondsPerSecond) + nanos);
#else
    return hostTimestamp;
#endif
}

// This method will be used when devTicksDelta is (supposedly) a small amount of ticks between two events. We will check
// devTicksDelta is reasonably small for the calculation below to succeed without losing precision.
uint64_t CalibratedTimestampTestInstance::getDeviceNanoseconds(uint64_t devTicksDelta) const
{
    if (devTicksDelta > static_cast<uint64_t>(std::numeric_limits<uint32_t>::max()))
    {
        std::ostringstream msg;
        msg << "Number of device ticks too big for conversion to nanoseconds: " << devTicksDelta;
        throw tcu::InternalError(msg.str());
    }
    return static_cast<uint64_t>(static_cast<double>(devTicksDelta) * m_timestampPeriod);
}

tcu::TestStatus CalibratedTimestampTestInstance::iterate(void)
{
    // Notes:
    //    1) Clocks may overflow.
    //    2) Because m_timestampPeriod is a floating point value, there may be less than one nano per tick.

    const tcu::TestStatus result = runTest();
    if (result.getCode() != QP_TEST_RESULT_PASS)
        return result;

    if (!m_qualityMessage.empty())
    {
        const std::string msg = "Warnings found: " + m_qualityMessage;
        return tcu::TestStatus(QP_TEST_RESULT_QUALITY_WARNING, msg);
    }
    return tcu::TestStatus::pass("Pass");
}

// Verify all invalid timestamp bits are zero.
void CalibratedTimestampTestInstance::verifyDevTimestampMask(uint64_t value) const
{
    checkTimestampBits(value, m_devTimestampMask);
}

// Absolute difference between two timestamps A and B taking overflow into account. Pick the smallest difference between the two
// possibilities. We don't know beforehand if B > A or vice versa. Take the valid bit mask into account.
uint64_t CalibratedTimestampTestInstance::absDiffWithOverflow(uint64_t a, uint64_t b, uint64_t mask) const
{
    //    <---------+ range +-------->
    //
    //    +--------------------------+
    //    |         uint64_t         |
    //    +------^-----------^-------+
    //           +           +
    //           a           b
    //           +----------->
    //           ccccccccccccc
    //    ------>             +-------
    //    ddddddd             dddddddd

    DE_ASSERT(a <= mask);
    DE_ASSERT(b <= mask);

    const uint64_t c = ((a >= b) ? (a - b) : (b - a));

    if (c == 0u)
        return c;

    const uint64_t d = (mask - c) + 1;

    return ((c < d) ? c : d);
}

// Positive difference between both marks, advancing from before to after, taking overflow and the valid bit mask into account.
uint64_t CalibratedTimestampTestInstance::positiveDiffWithOverflow(uint64_t before, uint64_t after, uint64_t mask) const
{
    DE_ASSERT(before <= mask);
    DE_ASSERT(after <= mask);

    return ((before <= after) ? (after - before) : ((mask - (before - after)) + 1));
}

// Return true if middle is not between begin and end, taking overflow into account.
bool CalibratedTimestampTestInstance::outOfRange(uint64_t begin, uint64_t middle, uint64_t end) const
{
    return (((begin <= end) && (middle < begin || middle > end)) ||
            ((begin > end) && (middle > end && middle < begin)));
}

std::vector<CalibratedTimestampTestInstance::CalibratedTimestamp> CalibratedTimestampTestInstance::
    getCalibratedTimestamps(const std::vector<VkTimeDomainKHR> &domains)
{
    std::vector<VkCalibratedTimestampInfoKHR> infos;

    for (auto domain : domains)
    {
        VkCalibratedTimestampInfoKHR info;
        info.sType      = getStructureType<VkCalibratedTimestampInfoKHR>();
        info.pNext      = nullptr;
        info.timeDomain = domain;
        infos.push_back(info);
    }

    std::vector<uint64_t> timestamps(domains.size());
    uint64_t deviation;

    const DeviceInterface &vk = m_context.getDeviceInterface();
    const VkDevice vkDevice   = m_context.getDevice();

    VK_CHECK(vk.getCalibratedTimestampsKHR(vkDevice, static_cast<uint32_t>(domains.size()), infos.data(),
                                           timestamps.data(), &deviation));

    if (deviation > kDeviationErrorLimitNanos)
    {
        throw tcu::InternalError("Calibrated maximum deviation too big");
    }
    else if (deviation > kDeviationWarningLimitNanos)
    {
        appendQualityMessage("Calibrated maximum deviation beyond desirable limits");
    }
    else if (deviation == 0 && domains.size() > 1)
    {
        appendQualityMessage("Calibrated maximum deviation reported as zero");
    }

    // Pack results.
    std::vector<CalibratedTimestamp> results;

    for (size_t i = 0; i < domains.size(); ++i)
    {
        if (domains[i] == VK_TIME_DOMAIN_DEVICE_KHR)
            verifyDevTimestampMask(timestamps[i]);
        results.emplace_back(timestamps[i], deviation);
    }

    return results;
}

CalibratedTimestampTestInstance::CalibratedTimestamp CalibratedTimestampTestInstance::getCalibratedTimestamp(
    VkTimeDomainKHR domain)
{
    // Single domain, single result.
    return getCalibratedTimestamps(std::vector<VkTimeDomainKHR>(1, domain))[0];
}

void CalibratedTimestampTestInstance::appendQualityMessage(const std::string &message)
{
    if (!m_qualityMessage.empty())
        m_qualityMessage += "; ";

    m_qualityMessage += message;
}

// Test device domain makes sense and is consistent with vkCmdWriteTimestamp().
tcu::TestStatus CalibratedTimestampDevDomainTestInstance::runTest(void)
{
    if (m_devDomains.empty())
        throw tcu::NotSupportedError("No suitable device time domains found");

    const DeviceInterface &vk = m_context.getDeviceInterface();
    const VkDevice vkDevice   = m_context.getDevice();
    const VkQueue queue       = m_context.getUniversalQueue();

    for (const auto devDomain : m_devDomains)
    {
        const CalibratedTimestamp before = getCalibratedTimestamp(devDomain);
        submitCommandsAndWait(vk, vkDevice, queue, m_cmdBuffer.get());
        const CalibratedTimestamp after = getCalibratedTimestamp(devDomain);
        const uint64_t diffNanos =
            getDeviceNanoseconds(positiveDiffWithOverflow(before.timestamp, after.timestamp, m_devTimestampMask));
        uint64_t written;
        VK_CHECK(vk.getQueryPoolResults(vkDevice, *m_queryPool, 0u, 1u, sizeof(written), &written, sizeof(written),
                                        (VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT)));
        verifyDevTimestampMask(written);

        if (diffNanos > kBatchTimeLimitNanos)
        {
            return tcu::TestStatus::fail(domainName(devDomain) + ": Batch of work took too long to execute");
        }

        if (outOfRange(before.timestamp, written, after.timestamp))
        {
            return tcu::TestStatus::fail(domainName(devDomain) +
                                         ": vkCmdWriteTimestamp() inconsistent with vkGetCalibratedTimestampsKHR()");
        }
    }

    return tcu::TestStatus::pass("Pass");
}

// Test host domain makes sense and is consistent with native host values.
tcu::TestStatus CalibratedTimestampHostDomainTestInstance::runTest(void)
{
    if (m_hostDomains.empty())
        throw tcu::NotSupportedError("No suitable host time domains found");

    for (const auto hostDomain : m_hostDomains)
    {
        const uint64_t before          = getHostNativeTimestamp(hostDomain);
        const CalibratedTimestamp vkTS = getCalibratedTimestamp(hostDomain);
        const uint64_t after           = getHostNativeTimestamp(hostDomain);
        const uint64_t diffNanos       = getHostNanoseconds(positiveDiffWithOverflow(before, after));

        if (diffNanos > kBatchTimeLimitNanos)
        {
            return tcu::TestStatus::fail(domainName(hostDomain) + ": Querying host domain took too long to execute");
        }

        if (outOfRange(before, vkTS.timestamp, after))
        {
            return tcu::TestStatus::fail(domainName(hostDomain) +
                                         ": vkGetCalibratedTimestampsKHR() inconsistent with native host API");
        }
    }

    return tcu::TestStatus::pass("Pass");
}

// Verify predictable timestamps and calibration possible.
tcu::TestStatus CalibratedTimestampCalibrationTestInstance::runTest(void)
{
    if (m_devDomains.empty())
        throw tcu::NotSupportedError("No suitable device time domains found");
    if (m_hostDomains.empty())
        throw tcu::NotSupportedError("No suitable host time domains found");

    // Sleep time.
    constexpr uint32_t kSleepMilliseconds = 200;
    constexpr uint32_t kSleepNanoseconds  = kSleepMilliseconds * kNanosecondsPerMillisecond;

    for (const auto devDomain : m_devDomains)
        for (const auto hostDomain : m_hostDomains)
        {
            std::vector<VkTimeDomainKHR> domains;
            domains.push_back(devDomain);  // Device results at index 0.
            domains.push_back(hostDomain); // Host results at index 1.

            // Measure time.
            const std::vector<CalibratedTimestamp> before = getCalibratedTimestamps(domains);
            std::this_thread::sleep_for(std::chrono::nanoseconds(kSleepNanoseconds));
            const std::vector<CalibratedTimestamp> after = getCalibratedTimestamps(domains);

            // Check device timestamp is as expected.
            const uint64_t devBeforeTicks = before[0].timestamp;
            const uint64_t devAfterTicks  = after[0].timestamp;
            const uint64_t devExpectedTicks =
                ((devBeforeTicks + static_cast<uint64_t>(static_cast<double>(kSleepNanoseconds) / m_timestampPeriod)) &
                 m_devTimestampMask);
            const uint64_t devDiffNanos =
                getDeviceNanoseconds(absDiffWithOverflow(devAfterTicks, devExpectedTicks, m_devTimestampMask));
            const uint64_t maxDevDiffNanos =
                std::max({kDefaultToleranceNanos, before[0].deviation + after[0].deviation});

            if (devDiffNanos > maxDevDiffNanos)
            {
                std::ostringstream msg;
                msg << "[" << domainName(devDomain) << "] Device expected timestamp differs " << devDiffNanos
                    << " nanoseconds (expect value <= " << maxDevDiffNanos << ")";
                return tcu::TestStatus::fail(msg.str());
            }

            // Check host timestamp is as expected.
            const uint64_t hostBefore   = getHostNanoseconds(before[1].timestamp);
            const uint64_t hostAfter    = getHostNanoseconds(after[1].timestamp);
            const uint64_t hostExpected = hostBefore + kSleepNanoseconds;
            const uint64_t hostDiff     = absDiffWithOverflow(hostAfter, hostExpected);
            const uint64_t maxHostDiff  = std::max({kDefaultToleranceNanos, before[1].deviation + after[1].deviation});

            if (hostDiff > maxHostDiff)
            {
                std::ostringstream msg;
                msg << "[" << domainName(hostDomain) << "] Host expected timestamp differs " << hostDiff
                    << " nanoseconds (expected value <= " << maxHostDiff << ")";
                return tcu::TestStatus::fail(msg.str());
            }
        }

    return tcu::TestStatus::pass("Pass");
}

class BasicGraphicsTest : public TimestampTest
{
public:
    BasicGraphicsTest(tcu::TestContext &testContext, const std::string &name, const TimestampTestParam *param)
        : TimestampTest(testContext, name, param)
    {
    }
    virtual ~BasicGraphicsTest(void)
    {
    }
    virtual void initPrograms(SourceCollections &programCollection) const;
    virtual TestInstance *createInstance(Context &context) const;
};

class BasicGraphicsTestInstance : public TimestampTestInstance
{
public:
    enum
    {
        VK_MAX_SHADER_STAGES = 6,
    };
    BasicGraphicsTestInstance(Context &context, const PipelineConstructionType pipelineConstructionType,
                              const StageFlagVector stages, const bool inRenderPass, const bool hostQueryReset,
                              const VkQueryResultFlags queryResultFlags);

    virtual ~BasicGraphicsTestInstance(void);

protected:
    virtual void buildPipeline(void);
    virtual void configCommandBuffer(void);
    virtual void buildVertexBuffer(void);
    virtual void buildRenderPass(VkFormat colorFormat, VkFormat depthFormat);

    virtual void buildFrameBuffer(tcu::UVec2 renderSize, VkFormat colorFormat, VkFormat depthFormat);

protected:
    const PipelineConstructionType m_pipelineConstructionType;
    const tcu::UVec2 m_renderSize;
    const VkFormat m_colorFormat;
    const VkFormat m_depthFormat;

    Move<VkImage> m_colorImage;
    de::MovePtr<Allocation> m_colorImageAlloc;
    Move<VkImage> m_depthImage;
    de::MovePtr<Allocation> m_depthImageAlloc;
    Move<VkImageView> m_colorAttachmentView;
    Move<VkImageView> m_depthAttachmentView;
    RenderPassWrapper m_renderPass;
    Move<VkFramebuffer> m_framebuffer;
    VkImageMemoryBarrier m_imageLayoutBarriers[2];

    de::MovePtr<Allocation> m_vertexBufferAlloc;
    Move<VkBuffer> m_vertexBuffer;
    std::vector<Vertex4RGBA> m_vertices;

    PipelineLayoutWrapper m_pipelineLayout;
    GraphicsPipelineWrapper m_graphicsPipeline;
};

void BasicGraphicsTest::initPrograms(SourceCollections &programCollection) const
{
    programCollection.glslSources.add("color_vert")
        << glu::VertexSource("#version 310 es\n"
                             "layout(location = 0) in vec4 position;\n"
                             "layout(location = 1) in vec4 color;\n"
                             "layout(location = 0) out highp vec4 vtxColor;\n"
                             "void main (void)\n"
                             "{\n"
                             "  gl_Position = position;\n"
                             "  vtxColor = color;\n"
                             "}\n");

    programCollection.glslSources.add("color_frag")
        << glu::FragmentSource("#version 310 es\n"
                               "layout(location = 0) in highp vec4 vtxColor;\n"
                               "layout(location = 0) out highp vec4 fragColor;\n"
                               "void main (void)\n"
                               "{\n"
                               "  fragColor = vtxColor;\n"
                               "}\n");
}

TestInstance *BasicGraphicsTest::createInstance(Context &context) const
{
    return new BasicGraphicsTestInstance(context, m_pipelineConstructionType, m_stages, m_inRenderPass,
                                         m_hostQueryReset, m_queryResultFlags);
}

void BasicGraphicsTestInstance::buildVertexBuffer(void)
{
    const DeviceInterface &vk = m_context.getDeviceInterface();

    // Create vertex buffer
    {
        m_vertexBuffer = createBufferAndBindMemory(1024u, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, &m_vertexBufferAlloc);
        m_vertices     = createOverlappingQuads();

        // Load vertices into vertex buffer
        deMemcpy(m_vertexBufferAlloc->getHostPtr(), m_vertices.data(), m_vertices.size() * sizeof(Vertex4RGBA));
        flushAlloc(vk, m_device, *m_vertexBufferAlloc);
    }
}

void BasicGraphicsTestInstance::buildRenderPass(VkFormat colorFormat, VkFormat depthFormat)
{
    const DeviceInterface &vk = m_context.getDeviceInterface();

    // Create render pass
    m_renderPass = RenderPassWrapper(m_pipelineConstructionType, vk, m_device, colorFormat, depthFormat);
}

void BasicGraphicsTestInstance::buildFrameBuffer(tcu::UVec2 renderSize, VkFormat colorFormat, VkFormat depthFormat)
{
    const DeviceInterface &vk                     = m_context.getDeviceInterface();
    const VkComponentMapping ComponentMappingRGBA = {VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G,
                                                     VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A};

    // Create color image
    {
        m_colorImage = createImage2DAndBindMemory(colorFormat, renderSize.x(), renderSize.y(),
                                                  VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                                                  VK_SAMPLE_COUNT_1_BIT, &m_colorImageAlloc);
    }

    // Create depth image
    {
        m_depthImage = createImage2DAndBindMemory(depthFormat, renderSize.x(), renderSize.y(),
                                                  VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_SAMPLE_COUNT_1_BIT,
                                                  &m_depthImageAlloc);
    }

    // Set up image layout transition barriers
    {
        const VkImageMemoryBarrier colorImageBarrier = {
            VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,      // VkStructureType sType;
            nullptr,                                     // const void* pNext;
            0u,                                          // VkAccessFlags srcAccessMask;
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,        // VkAccessFlags dstAccessMask;
            VK_IMAGE_LAYOUT_UNDEFINED,                   // VkImageLayout oldLayout;
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,    // VkImageLayout newLayout;
            VK_QUEUE_FAMILY_IGNORED,                     // uint32_t srcQueueFamilyIndex;
            VK_QUEUE_FAMILY_IGNORED,                     // uint32_t dstQueueFamilyIndex;
            *m_colorImage,                               // VkImage image;
            {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u}, // VkImageSubresourceRange subresourceRange;
        };
        const VkImageMemoryBarrier depthImageBarrier = {
            VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,           // VkStructureType sType;
            nullptr,                                          // const void* pNext;
            0u,                                               // VkAccessFlags srcAccessMask;
            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,     // VkAccessFlags dstAccessMask;
            VK_IMAGE_LAYOUT_UNDEFINED,                        // VkImageLayout oldLayout;
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, // VkImageLayout newLayout;
            VK_QUEUE_FAMILY_IGNORED,                          // uint32_t srcQueueFamilyIndex;
            VK_QUEUE_FAMILY_IGNORED,                          // uint32_t dstQueueFamilyIndex;
            *m_depthImage,                                    // VkImage image;
            {VK_IMAGE_ASPECT_DEPTH_BIT, 0u, 1u, 0u, 1u},      // VkImageSubresourceRange subresourceRange;
        };

        m_imageLayoutBarriers[0] = colorImageBarrier;
        m_imageLayoutBarriers[1] = depthImageBarrier;
    }

    // Create color attachment view
    {
        const VkImageViewCreateInfo colorAttachmentViewParams = {
            VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,    // VkStructureType          sType;
            nullptr,                                     // const void*              pNext;
            0u,                                          // VkImageViewCreateFlags   flags;
            *m_colorImage,                               // VkImage                  image;
            VK_IMAGE_VIEW_TYPE_2D,                       // VkImageViewType          viewType;
            colorFormat,                                 // VkFormat                 format;
            ComponentMappingRGBA,                        // VkComponentMapping       components;
            {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u}, // VkImageSubresourceRange  subresourceRange;
        };

        m_colorAttachmentView = createImageView(vk, m_device, &colorAttachmentViewParams);
    }

    // Create depth attachment view
    {
        const VkImageViewCreateInfo depthAttachmentViewParams = {
            VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,    // VkStructureType          sType;
            nullptr,                                     // const void*              pNext;
            0u,                                          // VkImageViewCreateFlags   flags;
            *m_depthImage,                               // VkImage                  image;
            VK_IMAGE_VIEW_TYPE_2D,                       // VkImageViewType          viewType;
            depthFormat,                                 // VkFormat                 format;
            ComponentMappingRGBA,                        // VkComponentMapping       components;
            {VK_IMAGE_ASPECT_DEPTH_BIT, 0u, 1u, 0u, 1u}, // VkImageSubresourceRange  subresourceRange;
        };

        m_depthAttachmentView = createImageView(vk, m_device, &depthAttachmentViewParams);
    }

    // Create framebuffer
    {
        const std::vector<VkImage> images = {
            *m_colorImage,
            *m_depthImage,
        };
        const VkImageView attachmentBindInfos[2] = {
            *m_colorAttachmentView,
            *m_depthAttachmentView,
        };

        const VkFramebufferCreateInfo framebufferParams = {
            VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO, // VkStructureType              sType;
            nullptr,                                   // const void*                  pNext;
            0u,                                        // VkFramebufferCreateFlags     flags;
            *m_renderPass,                             // VkRenderPass                 renderPass;
            2u,                                        // uint32_t                     attachmentCount;
            attachmentBindInfos,                       // const VkImageView*           pAttachments;
            (uint32_t)renderSize.x(),                  // uint32_t                     width;
            (uint32_t)renderSize.y(),                  // uint32_t                     height;
            1u,                                        // uint32_t                     layers;
        };

        m_renderPass.createFramebuffer(vk, m_device, &framebufferParams, images);
    }
}

BasicGraphicsTestInstance::BasicGraphicsTestInstance(Context &context,
                                                     const PipelineConstructionType pipelineConstructionType,
                                                     const StageFlagVector stages, const bool inRenderPass,
                                                     const bool hostQueryReset,
                                                     const VkQueryResultFlags queryResultFlags)
    : TimestampTestInstance(context, stages, inRenderPass, hostQueryReset, false, queryResultFlags)
    , m_pipelineConstructionType(pipelineConstructionType)
    , m_renderSize(32, 32)
    , m_colorFormat(VK_FORMAT_R8G8B8A8_UNORM)
    , m_depthFormat(VK_FORMAT_D16_UNORM)
    , m_graphicsPipeline(context.getInstanceInterface(), context.getDeviceInterface(), context.getPhysicalDevice(),
                         context.getDevice(), context.getDeviceExtensions(), pipelineConstructionType)
{
    buildVertexBuffer();

    buildRenderPass(m_colorFormat, m_depthFormat);

    buildFrameBuffer(m_renderSize, m_colorFormat, m_depthFormat);

    // Create pipeline layout
    const DeviceInterface &vk                             = m_context.getDeviceInterface();
    const VkPipelineLayoutCreateInfo pipelineLayoutParams = initVulkanStructure();
    m_pipelineLayout = PipelineLayoutWrapper(pipelineConstructionType, vk, m_device, &pipelineLayoutParams);
}

BasicGraphicsTestInstance::~BasicGraphicsTestInstance(void)
{
}

static const VkVertexInputBindingDescription defaultVertexInputBindingDescription{
    0u,                          // uint32_t binding;
    sizeof(Vertex4RGBA),         // uint32_t strideInBytes;
    VK_VERTEX_INPUT_RATE_VERTEX, // VkVertexInputRate inputRate;
};

static const VkVertexInputAttributeDescription defaultVertexInputAttributeDescriptions[2]{
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

static const VkPipelineVertexInputStateCreateInfo defaultVertexInputStateParams{
    VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, // VkStructureType sType;
    nullptr,                                                   // const void* pNext;
    0u,                                                        // VkPipelineVertexInputStateCreateFlags flags;
    1u,                                                        // uint32_t vertexBindingDescriptionCount;
    &defaultVertexInputBindingDescription,   // const VkVertexInputBindingDescription* pVertexBindingDescriptions;
    2u,                                      // uint32_t vertexAttributeDescriptionCount;
    defaultVertexInputAttributeDescriptions, // const VkVertexInputAttributeDescription* pVertexAttributeDescriptions;
};

static const VkPipelineDepthStencilStateCreateInfo defaultDepthStencilStateParams{
    VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO, // VkStructureType sType;
    nullptr,                                                    // const void* pNext;
    0u,                                                         // VkPipelineDepthStencilStateCreateFlags flags;
    VK_TRUE,                                                    // VkBool32 depthTestEnable;
    VK_TRUE,                                                    // VkBool32 depthWriteEnable;
    VK_COMPARE_OP_LESS_OR_EQUAL,                                // VkCompareOp depthCompareOp;
    VK_FALSE,                                                   // VkBool32 depthBoundsTestEnable;
    VK_FALSE,                                                   // VkBool32 stencilTestEnable;
    {
        // VkStencilOpState front;
        VK_STENCIL_OP_KEEP,  // VkStencilOp                                failOp
        VK_STENCIL_OP_KEEP,  // VkStencilOp                                passOp
        VK_STENCIL_OP_KEEP,  // VkStencilOp                                depthFailOp
        VK_COMPARE_OP_NEVER, // VkCompareOp                                compareOp
        0u,                  // uint32_t                                    compareMask
        0u,                  // uint32_t                                    writeMask
        0u                   // uint32_t                                    reference
    },
    {
        // VkStencilOpState back;
        VK_STENCIL_OP_KEEP,  // VkStencilOp                                failOp
        VK_STENCIL_OP_KEEP,  // VkStencilOp                                passOp
        VK_STENCIL_OP_KEEP,  // VkStencilOp                                depthFailOp
        VK_COMPARE_OP_NEVER, // VkCompareOp                                compareOp
        0u,                  // uint32_t                                    compareMask
        0u,                  // uint32_t                                    writeMask
        0u                   // uint32_t                                    reference
    },
    0.0f, // float minDepthBounds;
    1.0f, // float maxDepthBounds;
};

void BasicGraphicsTestInstance::buildPipeline(void)
{
    const DeviceInterface &vk = m_context.getDeviceInterface();

    auto vertexShaderModule   = ShaderWrapper(vk, m_device, m_context.getBinaryCollection().get("color_vert"), 0);
    auto fragmentShaderModule = ShaderWrapper(vk, m_device, m_context.getBinaryCollection().get("color_frag"), 0);

    const std::vector<VkViewport> viewports{makeViewport(m_renderSize)};
    const std::vector<VkRect2D> scissors{makeRect2D(m_renderSize)};

    // Create pipeline
    m_graphicsPipeline.setDefaultRasterizationState()
        .setDefaultColorBlendState()
        .setDefaultMultisampleState()
        .setupVertexInputState(&defaultVertexInputStateParams)
        .setupPreRasterizationShaderState(viewports, scissors, m_pipelineLayout, *m_renderPass, 0u, vertexShaderModule)
        .setupFragmentShaderState(m_pipelineLayout, *m_renderPass, 0u, fragmentShaderModule,
                                  &defaultDepthStencilStateParams)
        .setupFragmentOutputState(*m_renderPass)
        .setMonolithicPipelineLayout(m_pipelineLayout)
        .buildPipeline();
}

void BasicGraphicsTestInstance::configCommandBuffer(void)
{
    const DeviceInterface &vk = m_context.getDeviceInterface();

    const VkClearValue attachmentClearValues[2] = {
        defaultClearValue(m_colorFormat),
        defaultClearValue(m_depthFormat),
    };

    beginCommandBuffer(vk, *m_cmdBuffer, 0u);

    vk.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                          VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
                          (VkDependencyFlags)0, 0u, nullptr, 0u, nullptr, DE_LENGTH_OF_ARRAY(m_imageLayoutBarriers),
                          m_imageLayoutBarriers);

    if (!m_hostQueryReset)
        vk.cmdResetQueryPool(*m_cmdBuffer, *m_queryPool, 0u, TimestampTest::ENTRY_COUNT);

    m_renderPass.begin(vk, *m_cmdBuffer, makeRect2D(0, 0, m_renderSize.x(), m_renderSize.y()), 2u,
                       attachmentClearValues);

    m_graphicsPipeline.bind(*m_cmdBuffer);
    VkDeviceSize offsets = 0u;
    vk.cmdBindVertexBuffers(*m_cmdBuffer, 0u, 1u, &m_vertexBuffer.get(), &offsets);
    vk.cmdDraw(*m_cmdBuffer, (uint32_t)m_vertices.size(), 1u, 0u, 0u);

    if (m_inRenderPass)
    {
        uint32_t timestampEntry = 0u;

        for (StageFlagVector::const_iterator it = m_stages.begin(); it != m_stages.end(); it++)
        {
            vk.cmdWriteTimestamp(*m_cmdBuffer, *it, *m_queryPool, timestampEntry++);
        }
    }

    m_renderPass.end(vk, *m_cmdBuffer);

    if (!m_inRenderPass)
    {
        uint32_t timestampEntry = 0u;

        for (StageFlagVector::const_iterator it = m_stages.begin(); it != m_stages.end(); it++)
        {
            vk.cmdWriteTimestamp(*m_cmdBuffer, *it, *m_queryPool, timestampEntry++);
        }
    }

    endCommandBuffer(vk, *m_cmdBuffer);
}

class AdvGraphicsTest : public BasicGraphicsTest
{
public:
    AdvGraphicsTest(tcu::TestContext &testContext, const std::string &name, const TimestampTestParam *param)
        : BasicGraphicsTest(testContext, name, param)
    {
    }

    virtual ~AdvGraphicsTest(void)
    {
    }
    virtual void initPrograms(SourceCollections &programCollection) const;
    virtual TestInstance *createInstance(Context &context) const;
};

class AdvGraphicsTestInstance : public BasicGraphicsTestInstance
{
public:
    AdvGraphicsTestInstance(Context &context, const PipelineConstructionType pipelineConstructionType,
                            const StageFlagVector stages, const bool inRenderPass, const bool hostQueryReset,
                            const VkQueryResultFlags queryResultFlags);

    virtual ~AdvGraphicsTestInstance(void);
    virtual void buildPipeline(void);
    virtual void configCommandBuffer(void);

protected:
    virtual void featureSupportCheck(void);

protected:
    VkPhysicalDeviceFeatures m_features;
    uint32_t m_draw_count;
    de::MovePtr<Allocation> m_indirectBufferAlloc;
    Move<VkBuffer> m_indirectBuffer;
};

void AdvGraphicsTest::initPrograms(SourceCollections &programCollection) const
{
    BasicGraphicsTest::initPrograms(programCollection);

    programCollection.glslSources.add("unused_geo")
        << glu::GeometrySource("#version 310 es\n"
                               "#extension GL_EXT_geometry_shader : enable\n"
                               "layout(triangles) in;\n"
                               "layout(triangle_strip, max_vertices = 3) out;\n"
                               "layout(location = 0) in highp vec4 in_vtxColor[];\n"
                               "layout(location = 0) out highp vec4 vtxColor;\n"
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

    programCollection.glslSources.add("basic_tcs") << glu::TessellationControlSource(
        "#version 310 es\n"
        "#extension GL_EXT_tessellation_shader : enable\n"
        "layout(vertices = 3) out;\n"
        "layout(location = 0) in highp vec4 color[];\n"
        "layout(location = 0) out highp vec4 vtxColor[];\n"
        "void main()\n"
        "{\n"
        "  gl_TessLevelOuter[0] = 4.0;\n"
        "  gl_TessLevelOuter[1] = 4.0;\n"
        "  gl_TessLevelOuter[2] = 4.0;\n"
        "  gl_TessLevelInner[0] = 4.0;\n"
        "  gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;\n"
        "  vtxColor[gl_InvocationID] = color[gl_InvocationID];\n"
        "}\n");

    programCollection.glslSources.add("basic_tes")
        << glu::TessellationEvaluationSource("#version 310 es\n"
                                             "#extension GL_EXT_tessellation_shader : enable\n"
                                             "layout(triangles, fractional_even_spacing, ccw) in;\n"
                                             "layout(location = 0) in highp vec4 colors[];\n"
                                             "layout(location = 0) out highp vec4 vtxColor;\n"
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

TestInstance *AdvGraphicsTest::createInstance(Context &context) const
{
    return new AdvGraphicsTestInstance(context, m_pipelineConstructionType, m_stages, m_inRenderPass, m_hostQueryReset,
                                       m_queryResultFlags);
}

void AdvGraphicsTestInstance::featureSupportCheck(void)
{
    for (StageFlagVector::const_iterator it = m_stages.begin(); it != m_stages.end(); it++)
    {
        switch (*it)
        {
        case VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT:
            if (m_features.geometryShader == VK_FALSE)
            {
                TCU_THROW(NotSupportedError, "Geometry Shader Not Supported");
            }
            break;
        case VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT:
        case VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT:
            if (m_features.tessellationShader == VK_FALSE)
            {
                TCU_THROW(NotSupportedError, "Tessellation Not Supported");
            }
            break;
        case VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT:
        default:
            break;
        }
    }
}

AdvGraphicsTestInstance::AdvGraphicsTestInstance(Context &context,
                                                 const PipelineConstructionType pipelineConstructionType,
                                                 const StageFlagVector stages, const bool inRenderPass,
                                                 const bool hostQueryReset, const VkQueryResultFlags queryResultFlags)
    : BasicGraphicsTestInstance(context, pipelineConstructionType, stages, inRenderPass, hostQueryReset,
                                queryResultFlags)
    , m_features(context.getDeviceFeatures())
{

    const DeviceInterface &vk = m_context.getDeviceInterface();

    // If necessary feature is not supported, throw error and fail current test
    featureSupportCheck();

    // Prepare the indirect draw buffer
    if (m_features.multiDrawIndirect == VK_TRUE)
    {
        m_draw_count = 2;
    }
    else
    {
        m_draw_count = 1;
    }

    m_indirectBuffer = createBufferAndBindMemory(32u, VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT, &m_indirectBufferAlloc);

    const VkDrawIndirectCommand indirectCmds[] = {
        {
            12u, // uint32_t    vertexCount;
            1u,  // uint32_t    instanceCount;
            0u,  // uint32_t    firstVertex;
            0u,  // uint32_t    firstInstance;
        },
        {
            12u, // uint32_t    vertexCount;
            1u,  // uint32_t    instanceCount;
            11u, // uint32_t    firstVertex;
            0u,  // uint32_t    firstInstance;
        },
    };

    // Load data into indirect draw buffer
    deMemcpy(m_indirectBufferAlloc->getHostPtr(), indirectCmds, m_draw_count * sizeof(VkDrawIndirectCommand));
    flushAlloc(vk, m_device, *m_indirectBufferAlloc);
}

AdvGraphicsTestInstance::~AdvGraphicsTestInstance(void)
{
}

void AdvGraphicsTestInstance::buildPipeline(void)
{
    const DeviceInterface &vk = m_context.getDeviceInterface();

    const std::vector<VkViewport> viewports{makeViewport(m_renderSize)};
    const std::vector<VkRect2D> scissors{makeRect2D(m_renderSize)};

    ShaderWrapper vertShaderModule = ShaderWrapper(vk, m_device, m_context.getBinaryCollection().get("color_vert"), 0);
    ShaderWrapper fragShaderModule = ShaderWrapper(vk, m_device, m_context.getBinaryCollection().get("color_frag"), 0);
    ShaderWrapper tescShaderModule;
    ShaderWrapper teseShaderModule;
    ShaderWrapper geomShaderModule;

    if (m_features.tessellationShader)
    {
        tescShaderModule = ShaderWrapper(vk, m_device, m_context.getBinaryCollection().get("basic_tcs"), 0);
        teseShaderModule = ShaderWrapper(vk, m_device, m_context.getBinaryCollection().get("basic_tes"), 0);
    }

    if (m_features.geometryShader)
        geomShaderModule = ShaderWrapper(vk, m_device, m_context.getBinaryCollection().get("unused_geo"), 0);

    // Create pipeline
    m_graphicsPipeline
        .setDefaultTopology(m_features.tessellationShader ? VK_PRIMITIVE_TOPOLOGY_PATCH_LIST :
                                                            VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
        .setDefaultRasterizationState()
        .setDefaultColorBlendState()
        .setDefaultMultisampleState()
        .setupVertexInputState(&defaultVertexInputStateParams)
        .setupPreRasterizationShaderState(viewports, scissors, m_pipelineLayout, *m_renderPass, 0u, vertShaderModule,
                                          nullptr, tescShaderModule, teseShaderModule, geomShaderModule)
        .setupFragmentShaderState(m_pipelineLayout, *m_renderPass, 0u, fragShaderModule,
                                  &defaultDepthStencilStateParams)
        .setupFragmentOutputState(*m_renderPass)
        .setMonolithicPipelineLayout(m_pipelineLayout)
        .buildPipeline();
}

void AdvGraphicsTestInstance::configCommandBuffer(void)
{
    const DeviceInterface &vk = m_context.getDeviceInterface();

    const VkClearValue attachmentClearValues[2] = {
        defaultClearValue(m_colorFormat),
        defaultClearValue(m_depthFormat),
    };

    beginCommandBuffer(vk, *m_cmdBuffer, 0u);

    vk.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                          VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
                          (VkDependencyFlags)0, 0u, nullptr, 0u, nullptr, DE_LENGTH_OF_ARRAY(m_imageLayoutBarriers),
                          m_imageLayoutBarriers);

    if (!m_hostQueryReset)
        vk.cmdResetQueryPool(*m_cmdBuffer, *m_queryPool, 0u, TimestampTest::ENTRY_COUNT);

    m_renderPass.begin(vk, *m_cmdBuffer, makeRect2D(0, 0, m_renderSize.x(), m_renderSize.y()), 2u,
                       attachmentClearValues);

    m_graphicsPipeline.bind(*m_cmdBuffer);

    VkDeviceSize offsets = 0u;
    vk.cmdBindVertexBuffers(*m_cmdBuffer, 0u, 1u, &m_vertexBuffer.get(), &offsets);

    vk.cmdDrawIndirect(*m_cmdBuffer, *m_indirectBuffer, 0u, m_draw_count, sizeof(VkDrawIndirectCommand));

    if (m_inRenderPass)
    {
        uint32_t timestampEntry = 0u;
        for (StageFlagVector::const_iterator it = m_stages.begin(); it != m_stages.end(); it++)
        {
            vk.cmdWriteTimestamp(*m_cmdBuffer, *it, *m_queryPool, timestampEntry++);
        }
    }

    m_renderPass.end(vk, *m_cmdBuffer);

    if (!m_inRenderPass)
    {
        uint32_t timestampEntry = 0u;
        for (StageFlagVector::const_iterator it = m_stages.begin(); it != m_stages.end(); it++)
        {
            vk.cmdWriteTimestamp(*m_cmdBuffer, *it, *m_queryPool, timestampEntry++);
        }
    }

    endCommandBuffer(vk, *m_cmdBuffer);
}

class BasicComputeTest : public TimestampTest
{
public:
    BasicComputeTest(tcu::TestContext &testContext, const std::string &name, const TimestampTestParam *param)
        : TimestampTest(testContext, name, param)
    {
    }

    virtual ~BasicComputeTest(void)
    {
    }
    virtual void initPrograms(SourceCollections &programCollection) const;
    virtual TestInstance *createInstance(Context &context) const;
};

class BasicComputeTestInstance : public TimestampTestInstance
{
public:
    BasicComputeTestInstance(Context &context, const StageFlagVector stages, const bool inRenderPass,
                             const bool hostQueryReset, VkQueryResultFlags VkQueryResultFlags);

    virtual ~BasicComputeTestInstance(void);
    virtual void configCommandBuffer(void);

protected:
    de::MovePtr<Allocation> m_inputBufAlloc;
    Move<VkBuffer> m_inputBuf;
    de::MovePtr<Allocation> m_outputBufAlloc;
    Move<VkBuffer> m_outputBuf;

    Move<VkDescriptorPool> m_descriptorPool;
    Move<VkDescriptorSet> m_descriptorSet;
    Move<VkDescriptorSetLayout> m_descriptorSetLayout;

    Move<VkPipelineLayout> m_pipelineLayout;
    Move<VkShaderModule> m_computeShaderModule;
    Move<VkPipeline> m_computePipelines;
};

void BasicComputeTest::initPrograms(SourceCollections &programCollection) const
{
    TimestampTest::initPrograms(programCollection);

    programCollection.glslSources.add("basic_compute") << glu::ComputeSource(
        "#version 310 es\n"
        "layout(local_size_x = 128) in;\n"
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
}

TestInstance *BasicComputeTest::createInstance(Context &context) const
{
    return new BasicComputeTestInstance(context, m_stages, m_inRenderPass, m_hostQueryReset, m_queryResultFlags);
}

BasicComputeTestInstance::BasicComputeTestInstance(Context &context, const StageFlagVector stages,
                                                   const bool inRenderPass, const bool hostQueryReset,
                                                   VkQueryResultFlags VkQueryResultFlags)
    : TimestampTestInstance(context, stages, inRenderPass, hostQueryReset, false, VkQueryResultFlags)
{
    const DeviceInterface &vk = context.getDeviceInterface();
    const VkDevice vkDevice   = context.getDevice();

    // Create buffer object, allocate storage, and generate input data
    const VkDeviceSize size = sizeof(tcu::Vec4) * 128u * 128u;

    m_inputBuf = createBufferAndBindMemory(size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, &m_inputBufAlloc);

    // Load vertices into buffer
    tcu::Vec4 *pVec = reinterpret_cast<tcu::Vec4 *>(m_inputBufAlloc->getHostPtr());

    for (uint32_t ndx = 0u; ndx < (128u * 128u); ndx++)
    {
        for (uint32_t component = 0u; component < 4u; component++)
        {
            pVec[ndx][component] = (float)(ndx * (component + 1u));
        }
    }

    flushAlloc(vk, vkDevice, *m_inputBufAlloc);

    m_outputBuf = createBufferAndBindMemory(size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, &m_outputBufAlloc);

    std::vector<VkDescriptorBufferInfo> descriptorInfos;

    descriptorInfos.push_back(makeDescriptorBufferInfo(*m_inputBuf, 0u, size));
    descriptorInfos.push_back(makeDescriptorBufferInfo(*m_outputBuf, 0u, size));

    // Create descriptor set layout
    DescriptorSetLayoutBuilder descLayoutBuilder;

    for (uint32_t bindingNdx = 0u; bindingNdx < 2u; bindingNdx++)
    {
        descLayoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
    }

    m_descriptorSetLayout = descLayoutBuilder.build(vk, vkDevice);

    // Create descriptor pool
    m_descriptorPool = DescriptorPoolBuilder()
                           .addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2)
                           .build(vk, vkDevice, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);

    // Create descriptor set
    const VkDescriptorSetAllocateInfo descriptorSetAllocInfo = {
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, // VkStructureType                 sType;
        nullptr,                                        // const void*                     pNext;
        *m_descriptorPool,                              // VkDescriptorPool                descriptorPool;
        1u,                                             // uint32_t                        setLayoutCount;
        &m_descriptorSetLayout.get(),                   // const VkDescriptorSetLayout*    pSetLayouts;
    };
    m_descriptorSet = allocateDescriptorSet(vk, vkDevice, &descriptorSetAllocInfo);

    DescriptorSetUpdateBuilder builder;
    for (uint32_t descriptorNdx = 0u; descriptorNdx < 2u; descriptorNdx++)
    {
        builder.writeSingle(*m_descriptorSet, DescriptorSetUpdateBuilder::Location::binding(descriptorNdx),
                            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &descriptorInfos[descriptorNdx]);
    }
    builder.update(vk, vkDevice);

    // Create compute pipeline layout
    const VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, // VkStructureType                 sType;
        nullptr,                                       // const void*                     pNext;
        0u,                                            // VkPipelineLayoutCreateFlags     flags;
        1u,                                            // uint32_t                        setLayoutCount;
        &m_descriptorSetLayout.get(),                  // const VkDescriptorSetLayout*    pSetLayouts;
        0u,                                            // uint32_t                        pushConstantRangeCount;
        nullptr,                                       // const VkPushConstantRange*      pPushConstantRanges;
    };

    m_pipelineLayout = createPipelineLayout(vk, vkDevice, &pipelineLayoutCreateInfo);

    // Create compute shader
    VkShaderModuleCreateInfo shaderModuleCreateInfo = {
        VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,                    // VkStructureType             sType;
        nullptr,                                                        // const void*                 pNext;
        0u,                                                             // VkShaderModuleCreateFlags   flags;
        m_context.getBinaryCollection().get("basic_compute").getSize(), // uintptr_t                   codeSize;
        (uint32_t *)m_context.getBinaryCollection()
            .get("basic_compute")
            .getBinary(), // const uint32_t*             pCode;

    };

    m_computeShaderModule = createShaderModule(vk, vkDevice, &shaderModuleCreateInfo);

    // Create compute pipeline
    const VkPipelineShaderStageCreateInfo stageCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, // VkStructureType                     sType;
        nullptr,                                             // const void*                         pNext;
        0u,                                                  // VkPipelineShaderStageCreateFlags    flags;
        VK_SHADER_STAGE_COMPUTE_BIT,                         // VkShaderStageFlagBits               stage;
        *m_computeShaderModule,                              // VkShaderModule                      module;
        "main",                                              // const char*                         pName;
        nullptr,                                             // const VkSpecializationInfo*         pSpecializationInfo;
    };

    const VkComputePipelineCreateInfo pipelineCreateInfo = {
        VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO, // VkStructureType                 sType;
        nullptr,                                        // const void*                     pNext;
        0u,                                             // VkPipelineCreateFlags           flags;
        stageCreateInfo,                                // VkPipelineShaderStageCreateInfo stage;
        *m_pipelineLayout,                              // VkPipelineLayout                layout;
        VK_NULL_HANDLE,                                 // VkPipeline                      basePipelineHandle;
        0u,                                             // int32_t                         basePipelineIndex;
    };

    m_computePipelines = createComputePipeline(vk, vkDevice, VK_NULL_HANDLE, &pipelineCreateInfo);
}

BasicComputeTestInstance::~BasicComputeTestInstance(void)
{
}

void BasicComputeTestInstance::configCommandBuffer(void)
{
    const DeviceInterface &vk = m_context.getDeviceInterface();

    beginCommandBuffer(vk, *m_cmdBuffer, 0u);

    if (!m_hostQueryReset)
        vk.cmdResetQueryPool(*m_cmdBuffer, *m_queryPool, 0u, TimestampTest::ENTRY_COUNT);

    vk.cmdBindPipeline(*m_cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *m_computePipelines);
    vk.cmdBindDescriptorSets(*m_cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *m_pipelineLayout, 0u, 1u,
                             &m_descriptorSet.get(), 0u, nullptr);
    vk.cmdDispatch(*m_cmdBuffer, 128u, 1u, 1u);

    uint32_t timestampEntry = 0u;
    for (StageFlagVector::const_iterator it = m_stages.begin(); it != m_stages.end(); it++)
    {
        vk.cmdWriteTimestamp(*m_cmdBuffer, *it, *m_queryPool, timestampEntry++);
    }

    endCommandBuffer(vk, *m_cmdBuffer);
}

class TransferTest : public TimestampTest
{
public:
    TransferTest(tcu::TestContext &testContext, const std::string &name, const TimestampTestParam *param);

    virtual ~TransferTest(void)
    {
    }
    virtual void initPrograms(SourceCollections &programCollection) const;
    virtual TestInstance *createInstance(Context &context) const;

protected:
    TransferMethod m_method;
};

class TransferTestInstance : public TimestampTestInstance
{
public:
    TransferTestInstance(Context &context, const StageFlagVector stages, const bool inRenderPass,
                         const bool hostQueryReset, const bool transferOnlyQueue, const TransferMethod method,
                         const VkQueryResultFlags queryResultFlags);

    virtual ~TransferTestInstance(void);
    virtual void configCommandBuffer(void);
    virtual void initialImageTransition(VkCommandBuffer cmdBuffer, VkImage image, VkImageSubresourceRange subRange,
                                        VkImageLayout layout);

protected:
    TransferMethod m_method;

    VkDeviceSize m_bufSize;
    Move<VkBuffer> m_srcBuffer;
    Move<VkBuffer> m_dstBuffer;
    de::MovePtr<Allocation> m_srcBufferAlloc;
    de::MovePtr<Allocation> m_dstBufferAlloc;

    VkFormat m_imageFormat;
    int32_t m_imageWidth;
    int32_t m_imageHeight;
    VkDeviceSize m_imageSize;
    Move<VkImage> m_srcImage;
    Move<VkImage> m_dstImage;
    Move<VkImage> m_depthImage;
    Move<VkImage> m_msImage;
    de::MovePtr<Allocation> m_srcImageAlloc;
    de::MovePtr<Allocation> m_dstImageAlloc;
    de::MovePtr<Allocation> m_depthImageAlloc;
    de::MovePtr<Allocation> m_msImageAlloc;
};

TransferTest::TransferTest(tcu::TestContext &testContext, const std::string &name, const TimestampTestParam *param)
    : TimestampTest(testContext, name, param)
{
    const TransferTimestampTestParam *transferParam = dynamic_cast<const TransferTimestampTestParam *>(param);
    m_method                                        = transferParam->getMethod();
}

void TransferTest::initPrograms(SourceCollections &programCollection) const
{
    TimestampTest::initPrograms(programCollection);
}

TestInstance *TransferTest::createInstance(Context &context) const
{
    return new TransferTestInstance(context, m_stages, m_inRenderPass, m_hostQueryReset, m_transferOnlyQueue, m_method,
                                    m_queryResultFlags);
}

TransferTestInstance::TransferTestInstance(Context &context, const StageFlagVector stages, const bool inRenderPass,
                                           const bool hostQueryReset, const bool transferOnlyQueue,
                                           const TransferMethod method, const VkQueryResultFlags queryResultFlags)
    : TimestampTestInstance(context, stages, inRenderPass, hostQueryReset, transferOnlyQueue, queryResultFlags)
    , m_method(method)
    , m_bufSize((queryResultFlags & VK_QUERY_RESULT_WITH_AVAILABILITY_BIT) ? 512u : 256u)
    , m_imageFormat(VK_FORMAT_R8G8B8A8_UNORM)
    , m_imageWidth(4u)
    , m_imageHeight(4u)
    , m_imageSize(256u)
{
    const DeviceInterface &vk = context.getDeviceInterface();

    // Create src buffer
    m_srcBuffer = createBufferAndBindMemory(
        m_bufSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT, &m_srcBufferAlloc);

    // Init the source buffer memory
    char *pBuf = reinterpret_cast<char *>(m_srcBufferAlloc->getHostPtr());
    deMemset(pBuf, 0xFF, sizeof(char) * (size_t)m_bufSize);
    flushAlloc(vk, m_device, *m_srcBufferAlloc);

    // Create dst buffer
    m_dstBuffer = createBufferAndBindMemory(
        m_bufSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, &m_dstBufferAlloc);

    // Create src/dst/depth image
    m_srcImage = createImage2DAndBindMemory(m_imageFormat, m_imageWidth, m_imageHeight,
                                            VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                                            VK_SAMPLE_COUNT_1_BIT, &m_srcImageAlloc);
    m_dstImage = createImage2DAndBindMemory(m_imageFormat, m_imageWidth, m_imageHeight,
                                            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                                            VK_SAMPLE_COUNT_1_BIT, &m_dstImageAlloc);
    m_depthImage =
        createImage2DAndBindMemory(VK_FORMAT_D16_UNORM, m_imageWidth, m_imageHeight, VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                                   VK_SAMPLE_COUNT_1_BIT, &m_depthImageAlloc);
    m_msImage = createImage2DAndBindMemory(m_imageFormat, m_imageWidth, m_imageHeight,
                                           VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                                               VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                                           VK_SAMPLE_COUNT_4_BIT, &m_msImageAlloc);
}

TransferTestInstance::~TransferTestInstance(void)
{
}

void TransferTestInstance::configCommandBuffer(void)
{
    const DeviceInterface &vk = m_context.getDeviceInterface();

    beginCommandBuffer(vk, *m_cmdBuffer, 0u);

    // Initialize buffer/image
    vk.cmdFillBuffer(*m_cmdBuffer, *m_dstBuffer, 0u, m_bufSize, 0x0);

    const VkClearColorValue srcClearValue              = {{1.0f, 1.0f, 1.0f, 1.0f}};
    const VkClearColorValue dstClearValue              = {{0.0f, 0.0f, 0.0f, 0.0f}};
    const struct VkImageSubresourceRange subRangeColor = {
        VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags  aspectMask;
        0u,                        // uint32_t            baseMipLevel;
        1u,                        // uint32_t            mipLevels;
        0u,                        // uint32_t            baseArrayLayer;
        1u,                        // uint32_t            arraySize;
    };
    const struct VkImageSubresourceRange subRangeDepth = {
        VK_IMAGE_ASPECT_DEPTH_BIT, // VkImageAspectFlags  aspectMask;
        0u,                        // uint32_t            baseMipLevel;
        1u,                        // uint32_t            mipLevels;
        0u,                        // uint32_t            baseArrayLayer;
        1u,                        // uint32_t            arraySize;
    };

    initialImageTransition(*m_cmdBuffer, *m_srcImage, subRangeColor, VK_IMAGE_LAYOUT_GENERAL);
    initialImageTransition(*m_cmdBuffer, *m_dstImage, subRangeColor, VK_IMAGE_LAYOUT_GENERAL);

    if (!m_transferOnlyQueue)
    {
        vk.cmdClearColorImage(*m_cmdBuffer, *m_srcImage, VK_IMAGE_LAYOUT_GENERAL, &srcClearValue, 1u, &subRangeColor);
        vk.cmdClearColorImage(*m_cmdBuffer, *m_dstImage, VK_IMAGE_LAYOUT_GENERAL, &dstClearValue, 1u, &subRangeColor);
    }

    // synchronize the Clear commands before starting any copy
    const vk::VkMemoryBarrier barrier = {
        vk::VK_STRUCTURE_TYPE_MEMORY_BARRIER,                           // VkStructureType sType;
        nullptr,                                                        // const void* pNext;
        vk::VK_ACCESS_TRANSFER_WRITE_BIT,                               // VkAccessFlags srcAccessMask;
        vk::VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT, // VkAccessFlags dstAccessMask;
    };
    vk.cmdPipelineBarrier(*m_cmdBuffer, vk::VK_PIPELINE_STAGE_TRANSFER_BIT, vk::VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 1u,
                          &barrier, 0u, nullptr, 0u, nullptr);

    if (!m_hostQueryReset)
        vk.cmdResetQueryPool(*m_cmdBuffer, *m_queryPool, 0u, TimestampTest::ENTRY_COUNT);

    // Copy Operations
    const VkImageSubresourceLayers imgSubResCopy = {
        VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags  aspectMask;
        0u,                        // uint32_t            mipLevel;
        0u,                        // uint32_t            baseArrayLayer;
        1u,                        // uint32_t            layerCount;
    };

    const VkOffset3D nullOffset  = {0u, 0u, 0u};
    const VkExtent3D imageExtent = {(uint32_t)m_imageWidth, (uint32_t)m_imageHeight, 1u};
    const VkOffset3D imageOffset = {(int)m_imageWidth, (int)m_imageHeight, 1};

    switch (m_method)
    {
    case TRANSFER_METHOD_COPY_BUFFER:
    {
        const VkBufferCopy copyBufRegion = {
            0u,        // VkDeviceSize    srcOffset;
            0u,        // VkDeviceSize    destOffset;
            m_bufSize, // VkDeviceSize    copySize;
        };

        vk.cmdCopyBuffer(*m_cmdBuffer, *m_srcBuffer, *m_dstBuffer, 1u, &copyBufRegion);
        break;
    }
    case TRANSFER_METHOD_COPY_IMAGE:
    {
        const VkImageCopy copyImageRegion = {
            imgSubResCopy, // VkImageSubresourceCopy  srcSubresource;
            nullOffset,    // VkOffset3D              srcOffset;
            imgSubResCopy, // VkImageSubresourceCopy  destSubresource;
            nullOffset,    // VkOffset3D              destOffset;
            imageExtent,   // VkExtent3D              extent;

        };

        vk.cmdCopyImage(*m_cmdBuffer, *m_srcImage, VK_IMAGE_LAYOUT_GENERAL, *m_dstImage, VK_IMAGE_LAYOUT_GENERAL, 1u,
                        &copyImageRegion);
        break;
    }
    case TRANSFER_METHOD_COPY_BUFFER_TO_IMAGE:
    {
        const VkBufferImageCopy bufImageCopy = {
            0u,                      // VkDeviceSize            bufferOffset;
            (uint32_t)m_imageWidth,  // uint32_t                bufferRowLength;
            (uint32_t)m_imageHeight, // uint32_t                bufferImageHeight;
            imgSubResCopy,           // VkImageSubresourceCopy  imageSubresource;
            nullOffset,              // VkOffset3D              imageOffset;
            imageExtent,             // VkExtent3D              imageExtent;
        };

        vk.cmdCopyBufferToImage(*m_cmdBuffer, *m_srcBuffer, *m_dstImage, VK_IMAGE_LAYOUT_GENERAL, 1u, &bufImageCopy);
        break;
    }
    case TRANSFER_METHOD_COPY_IMAGE_TO_BUFFER:
    {
        const VkBufferImageCopy imgBufferCopy = {
            0u,                      // VkDeviceSize            bufferOffset;
            (uint32_t)m_imageWidth,  // uint32_t                bufferRowLength;
            (uint32_t)m_imageHeight, // uint32_t                bufferImageHeight;
            imgSubResCopy,           // VkImageSubresourceCopy  imageSubresource;
            nullOffset,              // VkOffset3D              imageOffset;
            imageExtent,             // VkExtent3D              imageExtent;
        };

        vk.cmdCopyImageToBuffer(*m_cmdBuffer, *m_srcImage, VK_IMAGE_LAYOUT_GENERAL, *m_dstBuffer, 1u, &imgBufferCopy);
        break;
    }
    case TRANSFER_METHOD_BLIT_IMAGE:
    {
        const VkImageBlit imageBlt = {imgSubResCopy, // VkImageSubresourceCopy  srcSubresource;
                                      {
                                          nullOffset,
                                          imageOffset,
                                      },
                                      imgSubResCopy, // VkImageSubresourceCopy  destSubresource;
                                      {
                                          nullOffset,
                                          imageOffset,
                                      }};

        vk.cmdBlitImage(*m_cmdBuffer, *m_srcImage, VK_IMAGE_LAYOUT_GENERAL, *m_dstImage, VK_IMAGE_LAYOUT_GENERAL, 1u,
                        &imageBlt, VK_FILTER_NEAREST);
        break;
    }
    case TRANSFER_METHOD_CLEAR_COLOR_IMAGE:
    {
        vk.cmdClearColorImage(*m_cmdBuffer, *m_dstImage, VK_IMAGE_LAYOUT_GENERAL, &srcClearValue, 1u, &subRangeColor);
        break;
    }
    case TRANSFER_METHOD_CLEAR_DEPTH_STENCIL_IMAGE:
    {
        initialImageTransition(*m_cmdBuffer, *m_depthImage, subRangeDepth, VK_IMAGE_LAYOUT_GENERAL);

        const VkClearDepthStencilValue clearDSValue = {
            1.0f, // float       depth;
            0u,   // uint32_t    stencil;
        };

        vk.cmdClearDepthStencilImage(*m_cmdBuffer, *m_depthImage, VK_IMAGE_LAYOUT_GENERAL, &clearDSValue, 1u,
                                     &subRangeDepth);
        break;
    }
    case TRANSFER_METHOD_FILL_BUFFER:
    {
        vk.cmdFillBuffer(*m_cmdBuffer, *m_dstBuffer, 0u, m_bufSize, 0x0);
        break;
    }
    case TRANSFER_METHOD_UPDATE_BUFFER:
    {
        const uint32_t data[] = {0xdeadbeef, 0xabcdef00, 0x12345678};

        vk.cmdUpdateBuffer(*m_cmdBuffer, *m_dstBuffer, 0x10, sizeof(data), data);
        break;
    }
    case TRANSFER_METHOD_COPY_QUERY_POOL_RESULTS:
    case TRANSFER_METHOD_COPY_QUERY_POOL_RESULTS_STRIDE_ZERO:
    {
        vk.cmdWriteTimestamp(*m_cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, *m_queryPool, 0u);
        VkDeviceSize copyStride = m_method == TRANSFER_METHOD_COPY_QUERY_POOL_RESULTS_STRIDE_ZERO ? 0u : 8u;
        vk.cmdCopyQueryPoolResults(*m_cmdBuffer, *m_queryPool, 0u, 1u, *m_dstBuffer, 0u, copyStride,
                                   VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);

        const vk::VkBufferMemoryBarrier bufferBarrier = {
            vk::VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER, // VkStructureType sType;
            nullptr,                                     // const void* pNext;
            vk::VK_ACCESS_TRANSFER_WRITE_BIT,            // VkAccessFlags srcAccessMask;
            vk::VK_ACCESS_HOST_READ_BIT,                 // VkAccessFlags dstAccessMask;
            VK_QUEUE_FAMILY_IGNORED,                     // uint32_t srcQueueFamilyIndex;
            VK_QUEUE_FAMILY_IGNORED,                     // uint32_t dstQueueFamilyIndex;
            *m_dstBuffer,                                // VkBuffer buffer;
            0ull,                                        // VkDeviceSize offset;
            VK_WHOLE_SIZE                                // VkDeviceSize size;
        };

        vk.cmdPipelineBarrier(*m_cmdBuffer, vk::VK_PIPELINE_STAGE_TRANSFER_BIT, vk::VK_PIPELINE_STAGE_HOST_BIT, 0u, 0u,
                              nullptr, 1u, &bufferBarrier, 0u, nullptr);

        vk.cmdResetQueryPool(*m_cmdBuffer, *m_queryPool, 0u, 1u);
        break;
    }
    case TRANSFER_METHOD_RESOLVE_IMAGE:
    {
        const VkImageResolve imageResolve = {
            imgSubResCopy, // VkImageSubresourceLayers  srcSubresource;
            nullOffset,    // VkOffset3D                srcOffset;
            imgSubResCopy, // VkImageSubresourceLayers  destSubresource;
            nullOffset,    // VkOffset3D                destOffset;
            imageExtent,   // VkExtent3D                extent;
        };

        initialImageTransition(*m_cmdBuffer, *m_msImage, subRangeColor, VK_IMAGE_LAYOUT_GENERAL);
        vk.cmdClearColorImage(*m_cmdBuffer, *m_msImage, VK_IMAGE_LAYOUT_GENERAL, &srcClearValue, 1u, &subRangeColor);
        vk.cmdPipelineBarrier(*m_cmdBuffer, vk::VK_PIPELINE_STAGE_TRANSFER_BIT, vk::VK_PIPELINE_STAGE_TRANSFER_BIT, 0u,
                              1u, &barrier, 0u, nullptr, 0u, nullptr);
        vk.cmdResolveImage(*m_cmdBuffer, *m_msImage, VK_IMAGE_LAYOUT_GENERAL, *m_dstImage, VK_IMAGE_LAYOUT_GENERAL, 1u,
                           &imageResolve);
        break;
    }
    default:
        DE_FATAL("Unknown Transfer Method!");
        break;
    }

    uint32_t timestampEntry = 0u;

    for (StageFlagVector::const_iterator it = m_stages.begin(); it != m_stages.end(); it++)
    {
        vk.cmdWriteTimestamp(*m_cmdBuffer, *it, *m_queryPool, timestampEntry++);
    }

    endCommandBuffer(vk, *m_cmdBuffer);
}

void TransferTestInstance::initialImageTransition(VkCommandBuffer cmdBuffer, VkImage image,
                                                  VkImageSubresourceRange subRange, VkImageLayout layout)
{
    const DeviceInterface &vk = m_context.getDeviceInterface();

    const VkImageMemoryBarrier imageMemBarrier = {
        VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, // VkStructureType          sType;
        nullptr,                                // const void*              pNext;
        0u,                                     // VkAccessFlags            srcAccessMask;
        VK_ACCESS_TRANSFER_WRITE_BIT,           // VkAccessFlags            dstAccessMask;
        VK_IMAGE_LAYOUT_UNDEFINED,              // VkImageLayout            oldLayout;
        layout,                                 // VkImageLayout            newLayout;
        VK_QUEUE_FAMILY_IGNORED,                // uint32_t                 srcQueueFamilyIndex;
        VK_QUEUE_FAMILY_IGNORED,                // uint32_t                 dstQueueFamilyIndex;
        image,                                  // VkImage                  image;
        subRange                                // VkImageSubresourceRange  subresourceRange;
    };

    vk.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0,
                          nullptr, 1, &imageMemBarrier);
}

class FillBufferBeforeCopyTest : public vkt::TestCase
{
public:
    FillBufferBeforeCopyTest(tcu::TestContext &testContext, const std::string &name) : vkt::TestCase(testContext, name)
    {
    }
    virtual ~FillBufferBeforeCopyTest(void)
    {
    }
    virtual void initPrograms(SourceCollections &programCollection) const;
    virtual TestInstance *createInstance(Context &context) const;
};

class FillBufferBeforeCopyTestInstance : public vkt::TestInstance
{
public:
    FillBufferBeforeCopyTestInstance(Context &context);
    virtual ~FillBufferBeforeCopyTestInstance(void)
    {
    }
    virtual tcu::TestStatus iterate(void);

protected:
    struct TimestampWithAvailability
    {
        uint64_t timestamp;
        uint64_t availability;
    };

    Move<VkCommandPool> m_cmdPool;
    Move<VkCommandBuffer> m_cmdBuffer;
    Move<VkQueryPool> m_queryPool;

    Move<VkBuffer> m_resultBuffer;
    de::MovePtr<Allocation> m_resultBufferMemory;
};

void FillBufferBeforeCopyTest::initPrograms(SourceCollections &programCollection) const
{
    vkt::TestCase::initPrograms(programCollection);
}

TestInstance *FillBufferBeforeCopyTest::createInstance(Context &context) const
{
    return new FillBufferBeforeCopyTestInstance(context);
}

FillBufferBeforeCopyTestInstance::FillBufferBeforeCopyTestInstance(Context &context) : vkt::TestInstance(context)
{
    const DeviceInterface &vk       = context.getDeviceInterface();
    const VkDevice vkDevice         = context.getDevice();
    const uint32_t queueFamilyIndex = context.getUniversalQueueFamilyIndex();
    Allocator &allocator            = m_context.getDefaultAllocator();

    // Check support for timestamp queries
    checkTimestampsSupported(context.getInstanceInterface(), context.getPhysicalDevice(), queueFamilyIndex);

    const VkQueryPoolCreateInfo queryPoolParams = {
        VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO, // VkStructureType               sType;
        nullptr,                                  // const void*                   pNext;
        0u,                                       // VkQueryPoolCreateFlags        flags;
        VK_QUERY_TYPE_TIMESTAMP,                  // VkQueryType                   queryType;
        1u,                                       // uint32_t                      entryCount;
        0u,                                       // VkQueryPipelineStatisticFlags pipelineStatistics;
    };

    m_queryPool = createQueryPool(vk, vkDevice, &queryPoolParams);
    m_cmdPool   = createCommandPool(vk, vkDevice, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, queueFamilyIndex);
    m_cmdBuffer = allocateCommandBuffer(vk, vkDevice, *m_cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

    // Create results buffer.
    const VkBufferCreateInfo bufferCreateInfo = {
        VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, // VkStructureType sType;
        nullptr,                              // const void* pNext;
        0u,                                   // VkBufferCreateFlags flags;
        sizeof(TimestampWithAvailability),    // VkDeviceSize size;
        VK_BUFFER_USAGE_TRANSFER_DST_BIT,     // VkBufferUsageFlags usage;
        VK_SHARING_MODE_EXCLUSIVE,            // VkSharingMode sharingMode;
        1u,                                   // uint32_t queueFamilyIndexCount;
        &queueFamilyIndex                     // const uint32_t* pQueueFamilyIndices;
    };

    m_resultBuffer = createBuffer(vk, vkDevice, &bufferCreateInfo);
    m_resultBufferMemory =
        allocator.allocate(getBufferMemoryRequirements(vk, vkDevice, *m_resultBuffer), MemoryRequirement::HostVisible);
    VK_CHECK(vk.bindBufferMemory(vkDevice, *m_resultBuffer, m_resultBufferMemory->getMemory(),
                                 m_resultBufferMemory->getOffset()));

    const vk::VkBufferMemoryBarrier fillBufferBarrier = {
        vk::VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER, // VkStructureType sType;
        nullptr,                                     // const void* pNext;
        vk::VK_ACCESS_TRANSFER_WRITE_BIT,            // VkAccessFlags srcAccessMask;
        vk::VK_ACCESS_TRANSFER_WRITE_BIT,            // VkAccessFlags dstAccessMask;
        VK_QUEUE_FAMILY_IGNORED,                     // uint32_t srcQueueFamilyIndex;
        VK_QUEUE_FAMILY_IGNORED,                     // uint32_t dstQueueFamilyIndex;
        *m_resultBuffer,                             // VkBuffer buffer;
        0ull,                                        // VkDeviceSize offset;
        VK_WHOLE_SIZE                                // VkDeviceSize size;
    };

    const vk::VkBufferMemoryBarrier bufferBarrier = {
        vk::VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER, // VkStructureType sType;
        nullptr,                                     // const void* pNext;
        vk::VK_ACCESS_TRANSFER_WRITE_BIT,            // VkAccessFlags srcAccessMask;
        vk::VK_ACCESS_HOST_READ_BIT,                 // VkAccessFlags dstAccessMask;
        VK_QUEUE_FAMILY_IGNORED,                     // uint32_t srcQueueFamilyIndex;
        VK_QUEUE_FAMILY_IGNORED,                     // uint32_t dstQueueFamilyIndex;
        *m_resultBuffer,                             // VkBuffer buffer;
        0ull,                                        // VkDeviceSize offset;
        VK_WHOLE_SIZE                                // VkDeviceSize size;
    };

    // Prepare command buffer.
    beginCommandBuffer(vk, *m_cmdBuffer, 0u);
    vk.cmdResetQueryPool(*m_cmdBuffer, *m_queryPool, 0u, 1u);
    vk.cmdFillBuffer(*m_cmdBuffer, *m_resultBuffer, 0u, bufferCreateInfo.size, 0u);
    vk.cmdPipelineBarrier(*m_cmdBuffer, vk::VK_PIPELINE_STAGE_TRANSFER_BIT, vk::VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u,
                          nullptr, 1u, &fillBufferBarrier, 0u, nullptr);
    vk.cmdWriteTimestamp(*m_cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, *m_queryPool, 0u);
    vk.cmdCopyQueryPoolResults(*m_cmdBuffer, *m_queryPool, 0u, 1u, *m_resultBuffer, 0u,
                               sizeof(TimestampWithAvailability),
                               (VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT));
    vk.cmdPipelineBarrier(*m_cmdBuffer, vk::VK_PIPELINE_STAGE_TRANSFER_BIT, vk::VK_PIPELINE_STAGE_HOST_BIT, 0u, 0u,
                          nullptr, 1u, &bufferBarrier, 0u, nullptr);
    endCommandBuffer(vk, *m_cmdBuffer);
}

tcu::TestStatus FillBufferBeforeCopyTestInstance::iterate(void)
{
    const DeviceInterface &vk = m_context.getDeviceInterface();
    const VkDevice vkDevice   = m_context.getDevice();
    const VkQueue queue       = m_context.getUniversalQueue();
    TimestampWithAvailability ta;

    submitCommandsAndWait(vk, vkDevice, queue, m_cmdBuffer.get());
    invalidateAlloc(vk, vkDevice, *m_resultBufferMemory);
    deMemcpy(&ta, m_resultBufferMemory->getHostPtr(), sizeof(ta));
    if (ta.availability)
    {
        if (ta.timestamp == 0)
        {
            return tcu::TestStatus::fail("Timestamp not written");
        }
    }
    return tcu::TestStatus::pass("Pass");
}

class ResetTimestampQueryBeforeCopyTest : public vkt::TestCase
{
public:
    ResetTimestampQueryBeforeCopyTest(tcu::TestContext &testContext, const std::string &name)
        : vkt::TestCase(testContext, name)
    {
    }
    virtual ~ResetTimestampQueryBeforeCopyTest(void)
    {
    }
    virtual void initPrograms(SourceCollections &programCollection) const;
    virtual TestInstance *createInstance(Context &context) const;
};

class ResetTimestampQueryBeforeCopyTestInstance : public vkt::TestInstance
{
public:
    ResetTimestampQueryBeforeCopyTestInstance(Context &context);
    virtual ~ResetTimestampQueryBeforeCopyTestInstance(void)
    {
    }
    virtual tcu::TestStatus iterate(void);

protected:
    struct TimestampWithAvailability
    {
        uint64_t timestamp;
        uint64_t availability;
    };

    Move<VkCommandPool> m_cmdPool;
    Move<VkCommandBuffer> m_cmdBuffer;
    Move<VkQueryPool> m_queryPool;

    Move<VkBuffer> m_resultBuffer;
    de::MovePtr<Allocation> m_resultBufferMemory;
};

void ResetTimestampQueryBeforeCopyTest::initPrograms(SourceCollections &programCollection) const
{
    vkt::TestCase::initPrograms(programCollection);
}

TestInstance *ResetTimestampQueryBeforeCopyTest::createInstance(Context &context) const
{
    return new ResetTimestampQueryBeforeCopyTestInstance(context);
}

ResetTimestampQueryBeforeCopyTestInstance::ResetTimestampQueryBeforeCopyTestInstance(Context &context)
    : vkt::TestInstance(context)
{
    const DeviceInterface &vk       = context.getDeviceInterface();
    const VkDevice vkDevice         = context.getDevice();
    const uint32_t queueFamilyIndex = context.getUniversalQueueFamilyIndex();
    Allocator &allocator            = m_context.getDefaultAllocator();

    // Check support for timestamp queries
    checkTimestampsSupported(context.getInstanceInterface(), context.getPhysicalDevice(), queueFamilyIndex);

    const VkQueryPoolCreateInfo queryPoolParams = {
        VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO, // VkStructureType               sType;
        nullptr,                                  // const void*                   pNext;
        0u,                                       // VkQueryPoolCreateFlags        flags;
        VK_QUERY_TYPE_TIMESTAMP,                  // VkQueryType                   queryType;
        1u,                                       // uint32_t                      entryCount;
        0u,                                       // VkQueryPipelineStatisticFlags pipelineStatistics;
    };

    m_queryPool = createQueryPool(vk, vkDevice, &queryPoolParams);
    m_cmdPool   = createCommandPool(vk, vkDevice, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, queueFamilyIndex);
    m_cmdBuffer = allocateCommandBuffer(vk, vkDevice, *m_cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

    // Create results buffer.
    const VkBufferCreateInfo bufferCreateInfo = {
        VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, // VkStructureType sType;
        nullptr,                              // const void* pNext;
        0u,                                   // VkBufferCreateFlags flags;
        sizeof(TimestampWithAvailability),    // VkDeviceSize size;
        VK_BUFFER_USAGE_TRANSFER_DST_BIT,     // VkBufferUsageFlags usage;
        VK_SHARING_MODE_EXCLUSIVE,            // VkSharingMode sharingMode;
        1u,                                   // uint32_t queueFamilyIndexCount;
        &queueFamilyIndex                     // const uint32_t* pQueueFamilyIndices;
    };

    m_resultBuffer = createBuffer(vk, vkDevice, &bufferCreateInfo);
    m_resultBufferMemory =
        allocator.allocate(getBufferMemoryRequirements(vk, vkDevice, *m_resultBuffer), MemoryRequirement::HostVisible);
    VK_CHECK(vk.bindBufferMemory(vkDevice, *m_resultBuffer, m_resultBufferMemory->getMemory(),
                                 m_resultBufferMemory->getOffset()));

    const vk::VkBufferMemoryBarrier bufferBarrier = {
        vk::VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER, // VkStructureType sType;
        nullptr,                                     // const void* pNext;
        vk::VK_ACCESS_TRANSFER_WRITE_BIT,            // VkAccessFlags srcAccessMask;
        vk::VK_ACCESS_HOST_READ_BIT,                 // VkAccessFlags dstAccessMask;
        VK_QUEUE_FAMILY_IGNORED,                     // uint32_t srcQueueFamilyIndex;
        VK_QUEUE_FAMILY_IGNORED,                     // uint32_t dstQueueFamilyIndex;
        *m_resultBuffer,                             // VkBuffer buffer;
        0ull,                                        // VkDeviceSize offset;
        VK_WHOLE_SIZE                                // VkDeviceSize size;
    };

    // Prepare command buffer.
    beginCommandBuffer(vk, *m_cmdBuffer, 0u);
    vk.cmdResetQueryPool(*m_cmdBuffer, *m_queryPool, 0u, 1u);
    vk.cmdWriteTimestamp(*m_cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, *m_queryPool, 0u);
    vk.cmdResetQueryPool(*m_cmdBuffer, *m_queryPool, 0u, 1u);
    vk.cmdCopyQueryPoolResults(*m_cmdBuffer, *m_queryPool, 0u, 1u, *m_resultBuffer, 0u,
                               sizeof(TimestampWithAvailability),
                               (VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT));
    vk.cmdPipelineBarrier(*m_cmdBuffer, vk::VK_PIPELINE_STAGE_TRANSFER_BIT, vk::VK_PIPELINE_STAGE_HOST_BIT, 0u, 0u,
                          nullptr, 1u, &bufferBarrier, 0u, nullptr);
    endCommandBuffer(vk, *m_cmdBuffer);
}

tcu::TestStatus ResetTimestampQueryBeforeCopyTestInstance::iterate(void)
{
    const DeviceInterface &vk = m_context.getDeviceInterface();
    const VkDevice vkDevice   = m_context.getDevice();
    const VkQueue queue       = m_context.getUniversalQueue();
    TimestampWithAvailability ta;

    submitCommandsAndWait(vk, vkDevice, queue, m_cmdBuffer.get());
    invalidateAlloc(vk, vkDevice, *m_resultBufferMemory);
    deMemcpy(&ta, m_resultBufferMemory->getHostPtr(), sizeof(ta));
    return ((ta.availability != 0) ? tcu::TestStatus::fail("Availability bit nonzero after resetting query") :
                                     tcu::TestStatus::pass("Pass"));
}

class TwoCmdBuffersTest : public TimestampTest
{
public:
    TwoCmdBuffersTest(tcu::TestContext &testContext, const std::string &name, const TwoCmdBuffersTestParam *param)
        : TimestampTest(testContext, name, param)
        , m_cmdBufferLevel(param->getCmdBufferLevel())
    {
    }
    virtual ~TwoCmdBuffersTest(void)
    {
    }
    virtual TestInstance *createInstance(Context &context) const;
    virtual void checkSupport(Context &context) const;

protected:
    VkCommandBufferLevel m_cmdBufferLevel;
};

class TwoCmdBuffersTestInstance : public TimestampTestInstance
{
public:
    TwoCmdBuffersTestInstance(Context &context, const StageFlagVector stages, const bool inRenderPass,
                              const bool hostQueryReset, const bool transferOnlyQueue,
                              VkCommandBufferLevel cmdBufferLevel, VkQueryResultFlags queryResultFlags);
    virtual ~TwoCmdBuffersTestInstance(void);
    virtual tcu::TestStatus iterate(void);

protected:
    virtual void configCommandBuffer(void);

protected:
    Move<VkCommandBuffer> m_secondCmdBuffer;
    Move<VkBuffer> m_dstBuffer;
    de::MovePtr<Allocation> m_dstBufferAlloc;
    VkCommandBufferLevel m_cmdBufferLevel;
};

TestInstance *TwoCmdBuffersTest::createInstance(Context &context) const
{
    return new TwoCmdBuffersTestInstance(context, m_stages, m_inRenderPass, m_hostQueryReset, m_transferOnlyQueue,
                                         m_cmdBufferLevel, m_queryResultFlags);
}

void TwoCmdBuffersTest::checkSupport(Context &context) const
{
    TimestampTest::checkSupport(context);
#ifdef CTS_USES_VULKANSC
    if (m_cmdBufferLevel == VK_COMMAND_BUFFER_LEVEL_SECONDARY &&
        context.getDeviceVulkanSC10Properties().secondaryCommandBufferNullOrImagelessFramebuffer == VK_FALSE)
        TCU_THROW(NotSupportedError, "secondaryCommandBufferNullFramebuffer is not supported");
#endif
}

TwoCmdBuffersTestInstance::TwoCmdBuffersTestInstance(Context &context, const StageFlagVector stages,
                                                     const bool inRenderPass, const bool hostQueryReset,
                                                     const bool transferOnlyQueue, VkCommandBufferLevel cmdBufferLevel,
                                                     VkQueryResultFlags queryResultFlags)
    : TimestampTestInstance(context, stages, inRenderPass, hostQueryReset, transferOnlyQueue, queryResultFlags)
    , m_cmdBufferLevel(cmdBufferLevel)
{
    const DeviceInterface &vk = context.getDeviceInterface();

    m_secondCmdBuffer = allocateCommandBuffer(vk, m_device, *m_cmdPool, cmdBufferLevel);
    m_dstBuffer = createBufferAndBindMemory(1024, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                            &m_dstBufferAlloc);
}

TwoCmdBuffersTestInstance::~TwoCmdBuffersTestInstance(void)
{
}

void TwoCmdBuffersTestInstance::configCommandBuffer(void)
{
    const DeviceInterface &vk = m_context.getDeviceInterface();

    const VkCommandBufferBeginInfo cmdBufferBeginInfo = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, // VkStructureType                          sType;
        nullptr,                                     // const void*                              pNext;
        0u,                                          // VkCommandBufferUsageFlags                flags;
        nullptr                                      // const VkCommandBufferInheritanceInfo*    pInheritanceInfo;
    };

    const vk::VkBufferMemoryBarrier bufferBarrier = {
        vk::VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER, // VkStructureType sType;
        nullptr,                                     // const void* pNext;
        vk::VK_ACCESS_TRANSFER_WRITE_BIT,            // VkAccessFlags srcAccessMask;
        vk::VK_ACCESS_HOST_READ_BIT,                 // VkAccessFlags dstAccessMask;
        VK_QUEUE_FAMILY_IGNORED,                     // uint32_t srcQueueFamilyIndex;
        VK_QUEUE_FAMILY_IGNORED,                     // uint32_t dstQueueFamilyIndex;
        *m_dstBuffer,                                // VkBuffer buffer;
        0ull,                                        // VkDeviceSize offset;
        VK_WHOLE_SIZE                                // VkDeviceSize size;
    };

    if (m_cmdBufferLevel == VK_COMMAND_BUFFER_LEVEL_PRIMARY)
    {
        VK_CHECK(vk.beginCommandBuffer(*m_cmdBuffer, &cmdBufferBeginInfo));
        if (!m_hostQueryReset)
            vk.cmdResetQueryPool(*m_cmdBuffer, *m_queryPool, 0u, TimestampTest::ENTRY_COUNT);
        vk.cmdWriteTimestamp(*m_cmdBuffer, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, *m_queryPool, 0);
        VK_CHECK(vk.endCommandBuffer(*m_cmdBuffer));
        VK_CHECK(vk.beginCommandBuffer(*m_secondCmdBuffer, &cmdBufferBeginInfo));
        vk.cmdCopyQueryPoolResults(*m_secondCmdBuffer, *m_queryPool, 0u, 1u, *m_dstBuffer, 0u, 0u, m_queryResultFlags);
        vk.cmdPipelineBarrier(*m_secondCmdBuffer, vk::VK_PIPELINE_STAGE_TRANSFER_BIT, vk::VK_PIPELINE_STAGE_HOST_BIT,
                              0u, 0u, nullptr, 1u, &bufferBarrier, 0u, nullptr);
        VK_CHECK(vk.endCommandBuffer(*m_secondCmdBuffer));
    }
    else
    {
        const VkCommandBufferInheritanceInfo inheritanceInfo = {
            VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO, // VkStructureType                  sType;
            nullptr,                                           // const void*                      pNext;
            VK_NULL_HANDLE,                                    // VkRenderPass                     renderPass;
            0u,                                                // uint32_t                         subpass;
            VK_NULL_HANDLE,                                    // VkFramebuffer                    framebuffer;
            VK_FALSE,                                          // VkBool32                         occlusionQueryEnable;
            0u,                                                // VkQueryControlFlags              queryFlags;
            0u                                                 // VkQueryPipelineStatisticFlags    pipelineStatistics;
        };

        const VkCommandBufferBeginInfo cmdBufferBeginInfoSecondary = {
            VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, // VkStructureType                          sType;
            nullptr,                                     // const void*                              pNext;
            0u,                                          // VkCommandBufferUsageFlags                flags;
            &inheritanceInfo                             // const VkCommandBufferInheritanceInfo*    pInheritanceInfo;
        };

        VK_CHECK(vk.beginCommandBuffer(*m_secondCmdBuffer, &cmdBufferBeginInfoSecondary));
        if (!m_hostQueryReset)
            vk.cmdResetQueryPool(*m_secondCmdBuffer, *m_queryPool, 0u, TimestampTest::ENTRY_COUNT);
        vk::VkPipelineStageFlagBits pipelineStage =
            m_transferOnlyQueue ? VK_PIPELINE_STAGE_TRANSFER_BIT : VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT;
        vk.cmdWriteTimestamp(*m_secondCmdBuffer, pipelineStage, *m_queryPool, 0);
        VK_CHECK(vk.endCommandBuffer(*m_secondCmdBuffer));
        VK_CHECK(vk.beginCommandBuffer(*m_cmdBuffer, &cmdBufferBeginInfo));
        vk.cmdExecuteCommands(m_cmdBuffer.get(), 1u, &m_secondCmdBuffer.get());
        if (!m_transferOnlyQueue)
            vk.cmdCopyQueryPoolResults(*m_cmdBuffer, *m_queryPool, 0u, 1u, *m_dstBuffer, 0u, 0u, m_queryResultFlags);
        vk.cmdPipelineBarrier(*m_cmdBuffer, vk::VK_PIPELINE_STAGE_TRANSFER_BIT, vk::VK_PIPELINE_STAGE_HOST_BIT, 0u, 0u,
                              nullptr, 1u, &bufferBarrier, 0u, nullptr);
        VK_CHECK(vk.endCommandBuffer(*m_cmdBuffer));
    }
}

tcu::TestStatus TwoCmdBuffersTestInstance::iterate(void)
{
    const DeviceInterface &vk = m_context.getDeviceInterface();
    const VkQueue queue       = getDeviceQueue(vk, m_device, m_queueFamilyIndex, 0);

    configCommandBuffer();

    const VkCommandBuffer cmdBuffers[] = {m_cmdBuffer.get(), m_secondCmdBuffer.get()};

    const VkSubmitInfo submitInfo = {
        VK_STRUCTURE_TYPE_SUBMIT_INFO, // VkStructureType                sType;
        nullptr,                       // const void*                    pNext;
        0u,                            // uint32_t                       waitSemaphoreCount;
        nullptr,                       // const VkSemaphore*             pWaitSemaphores;
        nullptr,                       // const VkPipelineStageFlags*    pWaitDstStageMask;
        m_cmdBufferLevel == VK_COMMAND_BUFFER_LEVEL_PRIMARY ? 2u :
                                                              1u, // uint32_t                       commandBufferCount;
        cmdBuffers,                                               // const VkCommandBuffer*         pCommandBuffers;
        0u,      // uint32_t                       signalSemaphoreCount;
        nullptr, // const VkSemaphore*             pSignalSemaphores;
    };

    if (m_hostQueryReset)
    {
        // Only reset the pool for the primary command buffer, the secondary command buffer will reset the pool by itself.
        vk.resetQueryPool(m_device, *m_queryPool, 0u, TimestampTest::ENTRY_COUNT);
    }

    VK_CHECK(vk.queueSubmit(queue, 1u, &submitInfo, VK_NULL_HANDLE));
    VK_CHECK(vk.queueWaitIdle(queue));

    // Always pass in case no crash occurred.
    return tcu::TestStatus::pass("Pass");
}

class ConsistentQueryResultsTest : public vkt::TestCase
{
public:
    ConsistentQueryResultsTest(tcu::TestContext &testContext, const std::string &name)
        : vkt::TestCase(testContext, name)
    {
    }
    virtual ~ConsistentQueryResultsTest(void)
    {
    }
    virtual void initPrograms(SourceCollections &programCollection) const;
    virtual TestInstance *createInstance(Context &context) const;
};

class ConsistentQueryResultsTestInstance : public vkt::TestInstance
{
public:
    ConsistentQueryResultsTestInstance(Context &context);
    virtual ~ConsistentQueryResultsTestInstance(void)
    {
    }
    virtual tcu::TestStatus iterate(void);

protected:
    Move<VkCommandPool> m_cmdPool;
    Move<VkCommandBuffer> m_cmdBuffer;
    Move<VkQueryPool> m_queryPool;

    uint64_t m_timestampMask;
    Move<VkBuffer> m_resultBuffer32Bits;
    Move<VkBuffer> m_resultBuffer64Bits;
    de::MovePtr<Allocation> m_resultBufferMemory32Bits;
    de::MovePtr<Allocation> m_resultBufferMemory64Bits;
};

void ConsistentQueryResultsTest::initPrograms(SourceCollections &programCollection) const
{
    vkt::TestCase::initPrograms(programCollection);
}

TestInstance *ConsistentQueryResultsTest::createInstance(Context &context) const
{
    return new ConsistentQueryResultsTestInstance(context);
}

ConsistentQueryResultsTestInstance::ConsistentQueryResultsTestInstance(Context &context) : vkt::TestInstance(context)
{
    const DeviceInterface &vk       = context.getDeviceInterface();
    const VkDevice vkDevice         = context.getDevice();
    const uint32_t queueFamilyIndex = context.getUniversalQueueFamilyIndex();
    Allocator &allocator            = m_context.getDefaultAllocator();

    // Check support for timestamp queries
    m_timestampMask =
        checkTimestampsSupported(context.getInstanceInterface(), context.getPhysicalDevice(), queueFamilyIndex);

    const VkQueryPoolCreateInfo queryPoolParams = {
        VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO, // VkStructureType               sType;
        nullptr,                                  // const void*                   pNext;
        0u,                                       // VkQueryPoolCreateFlags        flags;
        VK_QUERY_TYPE_TIMESTAMP,                  // VkQueryType                   queryType;
        1u,                                       // uint32_t                      entryCount;
        0u,                                       // VkQueryPipelineStatisticFlags pipelineStatistics;
    };

    m_queryPool = createQueryPool(vk, vkDevice, &queryPoolParams);
    m_cmdPool   = createCommandPool(vk, vkDevice, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, queueFamilyIndex);
    m_cmdBuffer = allocateCommandBuffer(vk, vkDevice, *m_cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

    // Create results buffer.
    VkBufferCreateInfo bufferCreateInfo = {
        VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, // VkStructureType sType;
        nullptr,                              // const void* pNext;
        0u,                                   // VkBufferCreateFlags flags;
        0u,                                   // VkDeviceSize size;
        VK_BUFFER_USAGE_TRANSFER_DST_BIT,     // VkBufferUsageFlags usage;
        VK_SHARING_MODE_EXCLUSIVE,            // VkSharingMode sharingMode;
        1u,                                   // uint32_t queueFamilyIndexCount;
        &queueFamilyIndex                     // const uint32_t* pQueueFamilyIndices;
    };

    // 32 bits.
    bufferCreateInfo.size      = sizeof(uint32_t);
    m_resultBuffer32Bits       = createBuffer(vk, vkDevice, &bufferCreateInfo);
    m_resultBufferMemory32Bits = allocator.allocate(getBufferMemoryRequirements(vk, vkDevice, *m_resultBuffer32Bits),
                                                    MemoryRequirement::HostVisible);
    VK_CHECK(vk.bindBufferMemory(vkDevice, *m_resultBuffer32Bits, m_resultBufferMemory32Bits->getMemory(),
                                 m_resultBufferMemory32Bits->getOffset()));

    // 64 bits.
    bufferCreateInfo.size      = sizeof(uint64_t);
    m_resultBuffer64Bits       = createBuffer(vk, vkDevice, &bufferCreateInfo);
    m_resultBufferMemory64Bits = allocator.allocate(getBufferMemoryRequirements(vk, vkDevice, *m_resultBuffer64Bits),
                                                    MemoryRequirement::HostVisible);
    VK_CHECK(vk.bindBufferMemory(vkDevice, *m_resultBuffer64Bits, m_resultBufferMemory64Bits->getMemory(),
                                 m_resultBufferMemory64Bits->getOffset()));

    vk::VkBufferMemoryBarrier bufferBarrier = {
        vk::VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER, // VkStructureType sType;
        nullptr,                                     // const void* pNext;
        vk::VK_ACCESS_TRANSFER_WRITE_BIT,            // VkAccessFlags srcAccessMask;
        vk::VK_ACCESS_HOST_READ_BIT,                 // VkAccessFlags dstAccessMask;
        VK_QUEUE_FAMILY_IGNORED,                     // uint32_t srcQueueFamilyIndex;
        VK_QUEUE_FAMILY_IGNORED,                     // uint32_t dstQueueFamilyIndex;
        VK_NULL_HANDLE,                              // VkBuffer buffer;
        0ull,                                        // VkDeviceSize offset;
        VK_WHOLE_SIZE                                // VkDeviceSize size;
    };

    // Prepare command buffer.
    beginCommandBuffer(vk, *m_cmdBuffer, 0u);
    vk.cmdResetQueryPool(*m_cmdBuffer, *m_queryPool, 0u, 1u);
    vk.cmdWriteTimestamp(*m_cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, *m_queryPool, 0u);

    // 32 bits.
    bufferBarrier.buffer = *m_resultBuffer32Bits;
    vk.cmdCopyQueryPoolResults(*m_cmdBuffer, *m_queryPool, 0u, 1u, *m_resultBuffer32Bits, 0u, sizeof(uint32_t),
                               VK_QUERY_RESULT_WAIT_BIT);
    vk.cmdPipelineBarrier(*m_cmdBuffer, vk::VK_PIPELINE_STAGE_TRANSFER_BIT, vk::VK_PIPELINE_STAGE_HOST_BIT, 0u, 0u,
                          nullptr, 1u, &bufferBarrier, 0u, nullptr);

    // 64 bits.
    bufferBarrier.buffer = *m_resultBuffer64Bits;
    vk.cmdCopyQueryPoolResults(*m_cmdBuffer, *m_queryPool, 0u, 1u, *m_resultBuffer64Bits, 0u, sizeof(uint64_t),
                               (VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT));
    vk.cmdPipelineBarrier(*m_cmdBuffer, vk::VK_PIPELINE_STAGE_TRANSFER_BIT, vk::VK_PIPELINE_STAGE_HOST_BIT, 0u, 0u,
                          nullptr, 1u, &bufferBarrier, 0u, nullptr);

    endCommandBuffer(vk, *m_cmdBuffer);
}

tcu::TestStatus ConsistentQueryResultsTestInstance::iterate(void)
{
    const DeviceInterface &vk = m_context.getDeviceInterface();
    const VkDevice vkDevice   = m_context.getDevice();
    const VkQueue queue       = m_context.getUniversalQueue();

    uint32_t tsBuffer32Bits;
    uint64_t tsBuffer64Bits;
    uint32_t tsGet32Bits;
    uint64_t tsGet64Bits;

    constexpr uint32_t maxDeUint32Value = std::numeric_limits<uint32_t>::max();

    submitCommandsAndWait(vk, vkDevice, queue, m_cmdBuffer.get());

    // Get results from buffers.
    invalidateAlloc(vk, vkDevice, *m_resultBufferMemory32Bits);
    invalidateAlloc(vk, vkDevice, *m_resultBufferMemory64Bits);
    deMemcpy(&tsBuffer32Bits, m_resultBufferMemory32Bits->getHostPtr(), sizeof(tsBuffer32Bits));
    deMemcpy(&tsBuffer64Bits, m_resultBufferMemory64Bits->getHostPtr(), sizeof(tsBuffer64Bits));

    // Get results with vkGetQueryPoolResults().
    VK_CHECK(vk.getQueryPoolResults(vkDevice, *m_queryPool, 0u, 1u, sizeof(tsGet32Bits), &tsGet32Bits,
                                    sizeof(tsGet32Bits), VK_QUERY_RESULT_WAIT_BIT));
    VK_CHECK(vk.getQueryPoolResults(vkDevice, *m_queryPool, 0u, 1u, sizeof(tsGet64Bits), &tsGet64Bits,
                                    sizeof(tsGet64Bits), (VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT)));

    // Check timestamp mask for both 64-bit results.
    checkTimestampBits(tsBuffer64Bits, m_timestampMask);
    checkTimestampBits(tsGet64Bits, m_timestampMask);

#ifndef CTS_USES_VULKANSC
    const bool hasMaint7 = m_context.getMaintenance7Features().maintenance7;
#else
    const bool hasMaint7 = false;
#endif

    // Check results are consistent.
    if (hasMaint7)
    {
        // If maintenance7 is supported, 32 bit queries _must_ be equivalent to the lower 32 bits of the 64 bit query
        if (tsBuffer32Bits == tsGet32Bits && tsBuffer64Bits == tsGet64Bits &&
            (((tsGet64Bits & maxDeUint32Value) == tsGet32Bits)))
        {
            return tcu::TestStatus::pass("Pass");
        }
    }
    else
    {
        if (tsBuffer32Bits == tsGet32Bits && tsBuffer64Bits == tsGet64Bits &&
            (((tsGet64Bits & maxDeUint32Value) == tsGet32Bits) ||
             ((tsGet64Bits > maxDeUint32Value) && (maxDeUint32Value == tsGet32Bits))))
        {
            return tcu::TestStatus::pass("Pass");
        }
    }

    std::ostringstream msg;
    msg << std::hex << "Results are inconsistent:"
        << " B32=0x" << tsBuffer32Bits << " B64=0x" << tsBuffer64Bits << " G32=0x" << tsGet32Bits << " G64=0x"
        << tsGet64Bits;
    return tcu::TestStatus::fail(msg.str());
}

class CheckTimestampComputeAndGraphicsTest : public vkt::TestCase
{
public:
    CheckTimestampComputeAndGraphicsTest(tcu::TestContext &testContext, const std::string &name)
        : vkt::TestCase(testContext, name)
    {
    }
    virtual ~CheckTimestampComputeAndGraphicsTest(void) = default;
    void checkSupport(Context &context) const;
    TestInstance *createInstance(Context &context) const;
};

class CheckTimestampComputeAndGraphicsTestInstance : public vkt::TestInstance
{
public:
    CheckTimestampComputeAndGraphicsTestInstance(Context &context) : vkt::TestInstance(context)
    {
    }
    virtual ~CheckTimestampComputeAndGraphicsTestInstance(void) = default;
    virtual tcu::TestStatus iterate(void);
};

void CheckTimestampComputeAndGraphicsTest::checkSupport(Context &context) const
{
    if (!context.getDeviceProperties().limits.timestampComputeAndGraphics)
        TCU_THROW(NotSupportedError, "timestampComputeAndGraphics is not supported");
}

TestInstance *CheckTimestampComputeAndGraphicsTest::createInstance(Context &context) const
{
    return new CheckTimestampComputeAndGraphicsTestInstance(context);
}

tcu::TestStatus CheckTimestampComputeAndGraphicsTestInstance::iterate(void)
{
    const InstanceInterface &vki          = m_context.getInstanceInterface();
    const VkPhysicalDevice physicalDevice = m_context.getPhysicalDevice();
    const VkQueueFlags gcQueueFlags       = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT;

    // When timestampComputeAndGraphics is true then all queues that advertise
    // the VK_QUEUE_GRAPHICS_BIT or VK_QUEUE_COMPUTE_BIT in the queueFlags support
    // VkQueueFamilyProperties::timestampValidBits of at least 36.

    const auto queueProps = getPhysicalDeviceQueueFamilyProperties(vki, physicalDevice);
    for (const auto &qp : queueProps)
    {
        if ((qp.queueFlags & gcQueueFlags) && (qp.timestampValidBits < 36))
            return tcu::TestStatus::fail("Fail");
    }

    return tcu::TestStatus::pass("Pass");
}

} // namespace

tcu::TestCaseGroup *createTimestampTests(tcu::TestContext &testCtx, PipelineConstructionType pipelineConstructionType)
{
    de::MovePtr<tcu::TestCaseGroup> timestampTests(new tcu::TestCaseGroup(testCtx, "timestamp"));
    const VkQueryResultFlags queryResultFlagsTimestampTest[] = {
        VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT,
        VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT,
    };

    // Basic Graphics Tests
    {
        de::MovePtr<tcu::TestCaseGroup> basicGraphicsTests(new tcu::TestCaseGroup(testCtx, "basic_graphics_tests"));

        const VkPipelineStageFlagBits basicGraphicsStages0[][2] = {
            {VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT},
            {VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT},
            {VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT},
            {VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT},
            {VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT},
            {VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT},
            {VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT},
            {VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT},
        };
        for (uint32_t stageNdx = 0u; stageNdx < DE_LENGTH_OF_ARRAY(basicGraphicsStages0); stageNdx++)
        {
            for (uint32_t flagsIdx = 0u; flagsIdx < DE_LENGTH_OF_ARRAY(queryResultFlagsTimestampTest); flagsIdx++)
            {
                TimestampTestParam param(pipelineConstructionType, basicGraphicsStages0[stageNdx], 2u, true, false,
                                         false, queryResultFlagsTimestampTest[flagsIdx]);
                basicGraphicsTests->addChild(newTestCase<BasicGraphicsTest>(testCtx, &param));
                param.toggleInRenderPass();
                basicGraphicsTests->addChild(newTestCase<BasicGraphicsTest>(testCtx, &param));
                // Host Query reset tests
                param.toggleHostQueryReset();
                basicGraphicsTests->addChild(newTestCase<BasicGraphicsTest>(testCtx, &param));
                param.toggleInRenderPass();
                basicGraphicsTests->addChild(newTestCase<BasicGraphicsTest>(testCtx, &param));
            }
        }

        const VkPipelineStageFlagBits basicGraphicsStages1[][3] = {
            {VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
             VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT},
            {VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
             VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT},
        };
        for (uint32_t stageNdx = 0u; stageNdx < DE_LENGTH_OF_ARRAY(basicGraphicsStages1); stageNdx++)
        {
            for (uint32_t flagsIdx = 0u; flagsIdx < DE_LENGTH_OF_ARRAY(queryResultFlagsTimestampTest); flagsIdx++)
            {
                TimestampTestParam param(pipelineConstructionType, basicGraphicsStages1[stageNdx], 3u, true, false,
                                         false, queryResultFlagsTimestampTest[flagsIdx]);
                basicGraphicsTests->addChild(newTestCase<BasicGraphicsTest>(testCtx, &param));
                param.toggleInRenderPass();
                basicGraphicsTests->addChild(newTestCase<BasicGraphicsTest>(testCtx, &param));
                // Host Query reset tests
                param.toggleHostQueryReset();
                basicGraphicsTests->addChild(newTestCase<BasicGraphicsTest>(testCtx, &param));
                param.toggleInRenderPass();
                basicGraphicsTests->addChild(newTestCase<BasicGraphicsTest>(testCtx, &param));
            }
        }

        timestampTests->addChild(basicGraphicsTests.release());
    }

    // Advanced Graphics Tests
    {
        // Record timestamp in different pipeline stages of advanced graphics tests
        de::MovePtr<tcu::TestCaseGroup> advGraphicsTests(new tcu::TestCaseGroup(testCtx, "advanced_graphics_tests"));

        const VkPipelineStageFlagBits advGraphicsStages[][2] = {
            {VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT},
            {VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT},
            {VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT},
            {VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT},
        };
        for (uint32_t stageNdx = 0u; stageNdx < DE_LENGTH_OF_ARRAY(advGraphicsStages); stageNdx++)
        {
            for (uint32_t flagsIdx = 0u; flagsIdx < DE_LENGTH_OF_ARRAY(queryResultFlagsTimestampTest); flagsIdx++)
            {
                TimestampTestParam param(pipelineConstructionType, advGraphicsStages[stageNdx], 2u, true, false, false,
                                         queryResultFlagsTimestampTest[flagsIdx]);
                advGraphicsTests->addChild(newTestCase<AdvGraphicsTest>(testCtx, &param));
                param.toggleInRenderPass();
                advGraphicsTests->addChild(newTestCase<AdvGraphicsTest>(testCtx, &param));
                // Host Query reset tests
                param.toggleHostQueryReset();
                advGraphicsTests->addChild(newTestCase<AdvGraphicsTest>(testCtx, &param));
                param.toggleInRenderPass();
                advGraphicsTests->addChild(newTestCase<AdvGraphicsTest>(testCtx, &param));
            }
        }

        timestampTests->addChild(advGraphicsTests.release());
    }

    // Basic Compute Tests - don't repeat those tests for graphics pipeline library
    if (pipelineConstructionType == PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC)
    {
        // Record timestamp for compute stages
        de::MovePtr<tcu::TestCaseGroup> basicComputeTests(new tcu::TestCaseGroup(testCtx, "basic_compute_tests"));

        const VkPipelineStageFlagBits basicComputeStages[][2] = {
            {VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT},
            {VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT},
        };
        for (uint32_t stageNdx = 0u; stageNdx < DE_LENGTH_OF_ARRAY(basicComputeStages); stageNdx++)
        {
            for (uint32_t flagsIdx = 0u; flagsIdx < DE_LENGTH_OF_ARRAY(queryResultFlagsTimestampTest); flagsIdx++)
            {
                TimestampTestParam param(pipelineConstructionType, basicComputeStages[stageNdx], 2u, false, false,
                                         false, queryResultFlagsTimestampTest[flagsIdx]);
                basicComputeTests->addChild(newTestCase<BasicComputeTest>(testCtx, &param));
                // Host Query reset test
                param.toggleHostQueryReset();
                basicComputeTests->addChild(newTestCase<BasicComputeTest>(testCtx, &param));
            }
        }

        timestampTests->addChild(basicComputeTests.release());
    }

    // Transfer Tests - don't repeat those tests for graphics pipeline library
    if (pipelineConstructionType == PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC)
    {
        de::MovePtr<tcu::TestCaseGroup> transferTests(new tcu::TestCaseGroup(testCtx, "transfer_tests"));

        const VkPipelineStageFlagBits transferStages[][2] = {
            {VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT},
            {VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_HOST_BIT},
        };

        for (uint32_t transferOnlyQueue = 0u; transferOnlyQueue < 2; transferOnlyQueue++)
        {
            for (uint32_t stageNdx = 0u; stageNdx < DE_LENGTH_OF_ARRAY(transferStages); stageNdx++)
            {
                for (uint32_t method = 0u; method < TRANSFER_METHOD_LAST; method++)
                {
                    if (transferOnlyQueue)
                    {
                        // skip tests that use commands not supported on transfer only queue
                        if (method == TRANSFER_METHOD_BLIT_IMAGE || method == TRANSFER_METHOD_CLEAR_COLOR_IMAGE ||
                            method == TRANSFER_METHOD_CLEAR_DEPTH_STENCIL_IMAGE ||
                            method == TRANSFER_METHOD_RESOLVE_IMAGE ||
                            method == TRANSFER_METHOD_COPY_QUERY_POOL_RESULTS ||
                            method == TRANSFER_METHOD_COPY_QUERY_POOL_RESULTS_STRIDE_ZERO)
                            continue;
                    }

                    for (uint32_t flagsIdx = 0u; flagsIdx < DE_LENGTH_OF_ARRAY(queryResultFlagsTimestampTest);
                         flagsIdx++)
                    {
                        TransferTimestampTestParam param(pipelineConstructionType, transferStages[stageNdx], 2u, false,
                                                         false, transferOnlyQueue, method,
                                                         queryResultFlagsTimestampTest[flagsIdx]);

                        // execute tests that use cmdResetQueryPool only on default device
                        if (!transferOnlyQueue)
                            transferTests->addChild(newTestCase<TransferTest>(testCtx, &param));

                        // Host Query reset test
                        param.toggleHostQueryReset();
                        transferTests->addChild(newTestCase<TransferTest>(testCtx, &param));
                    }
                }
            }
        }

        timestampTests->addChild(transferTests.release());
    }

    // Calibrated Timestamp Tests - don't repeat those tests for graphics pipeline library
    if (pipelineConstructionType == PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC)
    {
        de::MovePtr<tcu::TestCaseGroup> calibratedTimestampTests(new tcu::TestCaseGroup(testCtx, "calibrated"));

        calibratedTimestampTests->addChild(
            new CalibratedTimestampTest<CalibratedTimestampDevDomainTestInstance>(testCtx, "dev_domain_test"));
        calibratedTimestampTests->addChild(
            new CalibratedTimestampTest<CalibratedTimestampHostDomainTestInstance>(testCtx, "host_domain_test"));
        // Test calibration using device and host domains
        calibratedTimestampTests->addChild(
            new CalibratedTimestampTest<CalibratedTimestampCalibrationTestInstance>(testCtx, "calibration_test"));

        timestampTests->addChild(calibratedTimestampTests.release());
    }

    // Misc Tests - don't repeat those tests for graphics pipeline library
    if (pipelineConstructionType == PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC)
    {
        const VkQueryResultFlags queryResultFlagsMiscTests[] = {
            VK_QUERY_RESULT_WAIT_BIT,
            VK_QUERY_RESULT_WAIT_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT,
        };

        const std::string queryResultsFlagsMiscTestsStr[] = {"", "_with_availability_bit"};

        de::MovePtr<tcu::TestCaseGroup> miscTests(new tcu::TestCaseGroup(testCtx, "misc_tests"));

        for (uint32_t flagsIdx = 0u; flagsIdx < DE_LENGTH_OF_ARRAY(queryResultFlagsMiscTests); flagsIdx++)
        {
            const VkPipelineStageFlagBits miscStages[] = {VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT};
            TimestampTestParam param(pipelineConstructionType, miscStages, 1u, false, false, false,
                                     queryResultFlagsTimestampTest[flagsIdx]);
            // Only write timestamp command in the commmand buffer
            miscTests->addChild(
                new TimestampTest(testCtx, "timestamp_only" + queryResultsFlagsMiscTestsStr[flagsIdx], &param));

            TwoCmdBuffersTestParam twoCmdBuffersParamPrimary(pipelineConstructionType, miscStages, 1u, false, false,
                                                             false, VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                                                             queryResultFlagsMiscTests[flagsIdx]);
            // Issue query in a command buffer and copy it on another primary command buffer
            miscTests->addChild(
                new TwoCmdBuffersTest(testCtx, "two_cmd_buffers_primary" + queryResultsFlagsMiscTestsStr[flagsIdx],
                                      &twoCmdBuffersParamPrimary));

            TwoCmdBuffersTestParam twoCmdBuffersParamSecondary(pipelineConstructionType, miscStages, 1u, false, false,
                                                               false, VK_COMMAND_BUFFER_LEVEL_SECONDARY,
                                                               queryResultFlagsMiscTests[flagsIdx]);
            // Issue query in a secondary command buffer and copy it on a primary command buffer
            miscTests->addChild(
                new TwoCmdBuffersTest(testCtx, "two_cmd_buffers_secondary" + queryResultsFlagsMiscTestsStr[flagsIdx],
                                      &twoCmdBuffersParamSecondary));
            // Misc: Host Query Reset tests
            param.toggleHostQueryReset();
            // Only write timestamp command in the commmand buffer
            miscTests->addChild(new TimestampTest(
                testCtx, "timestamp_only_host_query_reset" + queryResultsFlagsMiscTestsStr[flagsIdx], &param));
            TwoCmdBuffersTestParam twoCmdBuffersParamPrimaryHostQueryReset(
                pipelineConstructionType, miscStages, 1u, false, true, false, VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                queryResultFlagsMiscTests[flagsIdx]);
            // Issue query in a command buffer and copy it on another primary command buffer
            miscTests->addChild(new TwoCmdBuffersTest(
                testCtx, "two_cmd_buffers_primary_host_query_reset" + queryResultsFlagsMiscTestsStr[flagsIdx],
                &twoCmdBuffersParamPrimaryHostQueryReset));

            TwoCmdBuffersTestParam twoCmdBuffersParamSecondaryHostQueryReset(
                pipelineConstructionType, miscStages, 1u, false, true, false, VK_COMMAND_BUFFER_LEVEL_SECONDARY,
                queryResultFlagsMiscTests[flagsIdx]);
            // Issue query in a secondary command buffer and copy it on a primary command buffer
            miscTests->addChild(new TwoCmdBuffersTest(
                testCtx, "two_cmd_buffers_secondary_host_query_reset" + queryResultsFlagsMiscTestsStr[flagsIdx],
                &twoCmdBuffersParamSecondaryHostQueryReset));
            TwoCmdBuffersTestParam twoCmdBuffersParamSecondaryTransferQueue(
                pipelineConstructionType, miscStages, 1u, false, true, true, VK_COMMAND_BUFFER_LEVEL_SECONDARY,
                queryResultFlagsMiscTests[flagsIdx]);
            // Issue query in a secondary command buffer and copy it on a primary command buffer
            miscTests->addChild(new TwoCmdBuffersTest(
                testCtx, "two_cmd_buffers_secondary_transfer_queue" + queryResultsFlagsMiscTestsStr[flagsIdx],
                &twoCmdBuffersParamSecondaryTransferQueue));
        }
        // Reset timestamp query before copying results.
        miscTests->addChild(new ResetTimestampQueryBeforeCopyTest(testCtx, "reset_query_before_copy"));

        // Fill buffer with 0s before copying results.
        miscTests->addChild(new FillBufferBeforeCopyTest(testCtx, "fill_buffer_before_copy"));

        // Check consistency between 32 and 64 bits.
        miscTests->addChild(new ConsistentQueryResultsTest(testCtx, "consistent_results"));

        // Check if timestamps are supported by every queue family that supports either graphics or compute operations
        if (pipelineConstructionType == PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC)
            miscTests->addChild(
                new CheckTimestampComputeAndGraphicsTest(testCtx, "check_timestamp_compute_and_graphics"));

        timestampTests->addChild(miscTests.release());
    }

    return timestampTests.release();
}

} // namespace pipeline

} // namespace vkt
