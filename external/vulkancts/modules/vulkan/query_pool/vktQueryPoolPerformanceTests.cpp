/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2018 The Khronos Group Inc.
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
 * \brief Vulkan Performance Query Tests
 *//*--------------------------------------------------------------------*/

#include "vktQueryPoolPerformanceTests.hpp"
#include "vktTestCase.hpp"

#include "vktDrawImageObjectUtil.hpp"
#include "vktDrawBufferObjectUtil.hpp"
#include "vktDrawCreateInfoUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkRefUtil.hpp"
#include "vkPrograms.hpp"
#include "vkTypeUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkBarrierUtil.hpp"

#include "deMath.h"
#include "deRandom.hpp"

#include "tcuTestLog.hpp"
#include "tcuResource.hpp"
#include "tcuImageCompare.hpp"
#include "vkImageUtil.hpp"
#include "tcuCommandLine.hpp"
#include "tcuRGBA.hpp"

#include <algorithm>
#include <iterator>

namespace vkt
{
namespace QueryPool
{
namespace
{

using namespace vk;
using namespace Draw;

std::string uuidToHex(const uint8_t uuid[])
{
    const size_t bytesPerPart[] = {4, 2, 2, 2, 6};
    const uint8_t *ptr          = &uuid[0];
    const size_t stringSize     = VK_UUID_SIZE * 2 + DE_LENGTH_OF_ARRAY(bytesPerPart) - 1;
    std::string result;

    result.reserve(stringSize);

    for (size_t partNdx = 0; partNdx < DE_LENGTH_OF_ARRAY(bytesPerPart); ++partNdx)
    {
        const size_t bytesInPart   = bytesPerPart[partNdx];
        const size_t symbolsInPart = 2 * bytesInPart;
        uint64_t part              = 0;
        std::string partString;

        for (size_t byteInPartNdx = 0; byteInPartNdx < bytesInPart; ++byteInPartNdx)
        {
            part = (part << 8) | *ptr;
            ++ptr;
        }

        partString = tcu::toHex(part).toString();

        DE_ASSERT(partString.size() > symbolsInPart);

        result +=
            (symbolsInPart >= partString.size()) ? partString : partString.substr(partString.size() - symbolsInPart);

        if (partNdx + 1 != DE_LENGTH_OF_ARRAY(bytesPerPart))
            result += '-';
    }

    DE_ASSERT(ptr == &uuid[VK_UUID_SIZE]);
    DE_ASSERT(result.size() == stringSize);

    return result;
}

// Helper class to acquire and release the profiling lock in an orderly manner.
// If an exception is thrown from a test (e.g. from VK_CHECK), the profiling lock is still released.
class ProfilingLockGuard
{
public:
    ProfilingLockGuard(const DeviceInterface &vkd, const VkDevice device) : m_vkd(vkd), m_device(device)
    {
        const auto timeout                           = std::numeric_limits<uint64_t>::max(); // Must always succeed.
        const VkAcquireProfilingLockInfoKHR lockInfo = {
            VK_STRUCTURE_TYPE_ACQUIRE_PROFILING_LOCK_INFO_KHR,
            NULL,
            0,
            timeout,
        };

        VK_CHECK(m_vkd.acquireProfilingLockKHR(m_device, &lockInfo));
    }

    ~ProfilingLockGuard(void)
    {
        m_vkd.releaseProfilingLockKHR(m_device);
    }

protected:
    const DeviceInterface &m_vkd;
    const VkDevice m_device;
};

using PerformanceCounterVec = std::vector<VkPerformanceCounterKHR>;

class EnumerateAndValidateTest : public TestInstance
{
public:
    EnumerateAndValidateTest(vkt::Context &context, VkQueueFlagBits queueFlagBits);
    tcu::TestStatus iterate(void);

protected:
    void basicValidateCounter(const uint32_t familyIndex);

private:
    VkQueueFlagBits m_queueFlagBits;
    bool m_requiredExtensionsPresent;
};

EnumerateAndValidateTest::EnumerateAndValidateTest(vkt::Context &context, VkQueueFlagBits queueFlagBits)
    : TestInstance(context)
    , m_queueFlagBits(queueFlagBits)
    , m_requiredExtensionsPresent(context.requireDeviceFunctionality("VK_KHR_performance_query"))
{
}

tcu::TestStatus EnumerateAndValidateTest::iterate(void)
{
    const InstanceInterface &vki          = m_context.getInstanceInterface();
    const VkPhysicalDevice physicalDevice = m_context.getPhysicalDevice();
    const std::vector<VkQueueFamilyProperties> queueProperties =
        getPhysicalDeviceQueueFamilyProperties(vki, physicalDevice);

    for (uint32_t queueNdx = 0; queueNdx < queueProperties.size(); queueNdx++)
    {
        if ((queueProperties[queueNdx].queueFlags & m_queueFlagBits) == 0)
            continue;

        uint32_t counterCount = 0;
        VK_CHECK(vki.enumeratePhysicalDeviceQueueFamilyPerformanceQueryCountersKHR(physicalDevice, queueNdx,
                                                                                   &counterCount, DE_NULL, DE_NULL));

        if (counterCount == 0)
            continue;

        {
            const VkPerformanceCounterKHR defaultCounterVal = initVulkanStructure();
            PerformanceCounterVec counters(counterCount, defaultCounterVal);
            uint32_t counterCountRead = counterCount;
            std::map<std::string, size_t> uuidValidator;

            if (counterCount > 1)
            {
                uint32_t incompleteCounterCount = counterCount - 1;
                VkResult result;

                result = vki.enumeratePhysicalDeviceQueueFamilyPerformanceQueryCountersKHR(
                    physicalDevice, queueNdx, &incompleteCounterCount, &counters[0], DE_NULL);
                if (result != VK_INCOMPLETE)
                    TCU_FAIL("VK_INCOMPLETE not returned");
            }

            VK_CHECK(vki.enumeratePhysicalDeviceQueueFamilyPerformanceQueryCountersKHR(
                physicalDevice, queueNdx, &counterCountRead, &counters[0], DE_NULL));

            if (counterCountRead != counterCount)
                TCU_FAIL("Number of counters read (" + de::toString(counterCountRead) +
                         ") is not equal to number of counters reported (" + de::toString(counterCount) + ")");

            for (size_t counterNdx = 0; counterNdx < counters.size(); ++counterNdx)
            {
                const VkPerformanceCounterKHR &counter = counters[counterNdx];
                const std::string uuidStr              = uuidToHex(counter.uuid);

                if (uuidValidator.find(uuidStr) != uuidValidator.end())
                    TCU_FAIL("Duplicate counter UUID detected " + uuidStr);
                else
                    uuidValidator[uuidStr] = counterNdx;

                if (counter.scope >= VK_PERFORMANCE_COUNTER_SCOPE_KHR_LAST)
                    TCU_FAIL("Counter scope is invalid " + de::toString(static_cast<size_t>(counter.scope)));

                if (counter.storage >= VK_PERFORMANCE_COUNTER_STORAGE_KHR_LAST)
                    TCU_FAIL("Counter storage is invalid " + de::toString(static_cast<size_t>(counter.storage)));

                if (counter.unit >= VK_PERFORMANCE_COUNTER_UNIT_KHR_LAST)
                    TCU_FAIL("Counter unit is invalid " + de::toString(static_cast<size_t>(counter.unit)));
            }
        }
        {
            const VkPerformanceCounterDescriptionKHR defaultDescription = initVulkanStructure();
            std::vector<VkPerformanceCounterDescriptionKHR> counterDescriptors(counterCount, defaultDescription);
            uint32_t counterCountRead = counterCount;

            VK_CHECK(vki.enumeratePhysicalDeviceQueueFamilyPerformanceQueryCountersKHR(
                physicalDevice, queueNdx, &counterCountRead, DE_NULL, &counterDescriptors[0]));

            if (counterCountRead != counterCount)
                TCU_FAIL("Number of counters read (" + de::toString(counterCountRead) +
                         ") is not equal to number of counters reported (" + de::toString(counterCount) + ")");

            for (size_t counterNdx = 0; counterNdx < counterDescriptors.size(); ++counterNdx)
            {
                const VkPerformanceCounterDescriptionKHR &counterDescriptor = counterDescriptors[counterNdx];
                const VkPerformanceCounterDescriptionFlagsKHR allowedFlags =
                    VK_PERFORMANCE_COUNTER_DESCRIPTION_PERFORMANCE_IMPACTING_KHR |
                    VK_PERFORMANCE_COUNTER_DESCRIPTION_CONCURRENTLY_IMPACTED_KHR;

                if ((counterDescriptor.flags & ~allowedFlags) != 0)
                    TCU_FAIL("Invalid flags present in VkPerformanceCounterDescriptionFlagsKHR");
            }
        }
    }

    return tcu::TestStatus::pass("Pass");
}

using ResultsVec          = std::vector<VkPerformanceCounterResultKHR>;
using BufferWithMemoryPtr = std::unique_ptr<BufferWithMemory>;

class QueryTestBase : public TestInstance
{
public:
    QueryTestBase(vkt::Context &context, bool copyResults, uint32_t seed);

protected:
    void setupCounters(void);
    Move<VkQueryPool> createQueryPool(uint32_t enabledCounterOffset, uint32_t enabledCounterStride);
    ResultsVec createResultsVector(const VkQueryPool pool) const;
    BufferWithMemoryPtr createResultsBuffer(const ResultsVec &resultsVector) const;
    void verifyQueryResults(uint32_t qfIndex, VkQueue queue, VkQueryPool queryPool) const;
    uint32_t getRequiredPassCount(void) const;

private:
    const bool m_copyResults;
    const uint32_t m_seed;
    bool m_requiredExtensionsPresent;
    uint32_t m_requiredNumerOfPasses;
    std::map<uint64_t, uint32_t> m_enabledCountersCountMap; // number of counters that were enabled per query pool
    PerformanceCounterVec m_counters;                       // counters provided by the device
};

QueryTestBase::QueryTestBase(vkt::Context &context, bool copyResults, uint32_t seed)
    : TestInstance(context)
    , m_copyResults(copyResults)
    , m_seed(seed)
    , m_requiredExtensionsPresent(context.requireDeviceFunctionality("VK_KHR_performance_query"))
    , m_requiredNumerOfPasses(0)
{
}

void QueryTestBase::setupCounters()
{
    const InstanceInterface &vki          = m_context.getInstanceInterface();
    const VkPhysicalDevice physicalDevice = m_context.getPhysicalDevice();
    const auto queueFamilyIndex           = m_context.getUniversalQueueFamilyIndex();
    const CmdPoolCreateInfo cmdPoolCreateInfo(queueFamilyIndex);
    uint32_t counterCount;

    // Get the number of supported counters.
    VK_CHECK(vki.enumeratePhysicalDeviceQueueFamilyPerformanceQueryCountersKHR(physicalDevice, queueFamilyIndex,
                                                                               &counterCount, nullptr, nullptr));

    // Get supported counters.
    const VkPerformanceCounterKHR defaultCounterVal = initVulkanStructure();
    m_counters.resize(counterCount, defaultCounterVal);
    VK_CHECK(vki.enumeratePhysicalDeviceQueueFamilyPerformanceQueryCountersKHR(
        physicalDevice, queueFamilyIndex, &counterCount, de::dataOrNull(m_counters), nullptr));

    // Filter out all counters with scope
    // VK_PERFORMANCE_COUNTER_SCOPE_COMMAND_BUFFER_KHR. For these counters, the
    // begin and end command must be at the beginning/end of the command buffer,
    // which does not match what these tests do.
    const auto scopeIsNotCmdBuffer = [](const VkPerformanceCounterKHR &c)
    { return (c.scope != VK_PERFORMANCE_COUNTER_SCOPE_COMMAND_BUFFER_KHR); };
    PerformanceCounterVec filteredCounters;

    filteredCounters.reserve(m_counters.size());
    std::copy_if(begin(m_counters), end(m_counters), std::back_inserter(filteredCounters), scopeIsNotCmdBuffer);
    m_counters.swap(filteredCounters);

    if (m_counters.empty())
        TCU_THROW(NotSupportedError, "No counters without command buffer scope found");
}

Move<VkQueryPool> QueryTestBase::createQueryPool(uint32_t enabledCounterOffset, uint32_t enabledCounterStride)
{
    const InstanceInterface &vki              = m_context.getInstanceInterface();
    const DeviceInterface &vkd                = m_context.getDeviceInterface();
    const VkPhysicalDevice physicalDevice     = m_context.getPhysicalDevice();
    const VkDevice device                     = m_context.getDevice();
    const CmdPoolCreateInfo cmdPoolCreateInfo = m_context.getUniversalQueueFamilyIndex();
    const uint32_t counterCount               = (uint32_t)m_counters.size();
    uint32_t enabledIndex                     = enabledCounterOffset ? 0 : enabledCounterStride;
    std::vector<uint32_t> enabledCounters;

    // enable every <enabledCounterStride> counter that has command or render pass scope
    for (uint32_t i = 0; i < counterCount; i++)
    {
        // handle offset
        if (enabledCounterOffset)
        {
            if (enabledCounterOffset == enabledIndex)
            {
                // disable handling offset
                enabledCounterOffset = 0;

                // eneble next index in stride condition
                enabledIndex = enabledCounterStride;
            }
            else
            {
                ++enabledIndex;
                continue;
            }
        }

        // handle stride
        if (enabledIndex == enabledCounterStride)
        {
            enabledCounters.push_back(i);
            enabledIndex = 0;
        }
        else
            ++enabledIndex;
    }

    // Get number of counters that were enabled for this query pool.
    if (enabledCounters.empty())
        TCU_THROW(NotSupportedError, "No suitable performance counters found for this test");

    const auto enabledCountersCount = de::sizeU32(enabledCounters);

    // define performance query
    const VkQueryPoolPerformanceCreateInfoKHR performanceQueryCreateInfo = {
        VK_STRUCTURE_TYPE_QUERY_POOL_PERFORMANCE_CREATE_INFO_KHR, NULL,
        cmdPoolCreateInfo.queueFamilyIndex, // queue family that this performance query is performed on
        enabledCountersCount,               // number of counters to enable
        &enabledCounters[0]                 // array of indices of counters to enable
    };

    // get the number of passes counters will require
    vki.getPhysicalDeviceQueueFamilyPerformanceQueryPassesKHR(physicalDevice, &performanceQueryCreateInfo,
                                                              &m_requiredNumerOfPasses);

    // create query pool
    const VkQueryPoolCreateInfo queryPoolCreateInfo = {VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
                                                       &performanceQueryCreateInfo,
                                                       0,                                   // flags
                                                       VK_QUERY_TYPE_PERFORMANCE_QUERY_KHR, // new query type
                                                       1,                                   // queryCount
                                                       0};

    Move<VkQueryPool> queryPool = vk::createQueryPool(vkd, device, &queryPoolCreateInfo);

    // memorize number of enabled counters for this query pool
    m_enabledCountersCountMap[queryPool.get().getInternal()] = enabledCountersCount;

    return queryPool;
}

ResultsVec QueryTestBase::createResultsVector(const VkQueryPool pool) const
{
    const auto itemCount = m_enabledCountersCountMap.at(pool.getInternal());
    ResultsVec resultsVector(itemCount);
    const auto byteSize = de::dataSize(resultsVector);
    const auto contents = reinterpret_cast<uint8_t *>(resultsVector.data());
    de::Random rnd(m_seed);

    // Fill vector with random bytes.
    for (size_t i = 0u; i < byteSize; ++i)
    {
        const auto byte = rnd.getInt(1, 255); // Do not use zeros.
        contents[i]     = static_cast<uint8_t>(byte);
    }

    return resultsVector;
}

BufferWithMemoryPtr QueryTestBase::createResultsBuffer(const ResultsVec &resultsVector) const
{
    const auto &vkd       = m_context.getDeviceInterface();
    const auto device     = m_context.getDevice();
    auto &alloc           = m_context.getDefaultAllocator();
    const auto bufferSize = static_cast<VkDeviceSize>(de::dataSize(resultsVector));
    const auto createInfo = makeBufferCreateInfo(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT);

    BufferWithMemoryPtr resultBuffer(
        new BufferWithMemory(vkd, device, alloc, createInfo, MemoryRequirement::HostVisible));
    auto &bufferAlloc = resultBuffer->getAllocation();
    void *bufferData  = bufferAlloc.getHostPtr();

    deMemcpy(bufferData, resultsVector.data(), de::dataSize(resultsVector));
    flushAlloc(vkd, device, bufferAlloc);

    return resultBuffer;
}

void QueryTestBase::verifyQueryResults(uint32_t qfIndex, VkQueue queue, VkQueryPool queryPool) const
{
    const DeviceInterface &vkd = m_context.getDeviceInterface();
    const VkDevice device      = m_context.getDevice();

    const auto initialVector = createResultsVector(queryPool);
    const auto resultsBuffer = createResultsBuffer(initialVector);
    auto &resultsBufferAlloc = resultsBuffer->getAllocation();
    void *resultsBufferData  = resultsBufferAlloc.getHostPtr();

    const auto resultsStride =
        static_cast<VkDeviceSize>(sizeof(decltype(initialVector)::value_type) * initialVector.size());
    const auto hostBufferSize = de::dataSize(initialVector);
    const auto resultFlags    = static_cast<VkQueryResultFlags>(VK_QUERY_RESULT_WAIT_BIT);

    // Get or copy query pool results.
    if (m_copyResults)
    {
        const auto cmdPool   = createCommandPool(vkd, device, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, qfIndex);
        const auto cmdBuffer = allocateCommandBuffer(vkd, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
        const auto barrier   = makeMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);

        beginCommandBuffer(vkd, *cmdBuffer);
        vkd.cmdCopyQueryPoolResults(*cmdBuffer, queryPool, 0u, 1u, resultsBuffer->get(), 0ull, resultsStride,
                                    resultFlags);
        cmdPipelineMemoryBarrier(vkd, *cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, &barrier);
        endCommandBuffer(vkd, *cmdBuffer);
        submitCommandsAndWait(vkd, device, queue, *cmdBuffer);
        invalidateAlloc(vkd, device, resultsBufferAlloc);
    }
    else
    {
        VK_CHECK(vkd.getQueryPoolResults(device, queryPool, 0u, 1u, hostBufferSize, resultsBufferData, resultsStride,
                                         resultFlags));
    }

    // Check that the buffer was modified without analyzing result semantics.
    ResultsVec resultsVector(initialVector.size());
    deMemcpy(de::dataOrNull(resultsVector), resultsBufferData, hostBufferSize);

    for (size_t i = 0u; i < initialVector.size(); ++i)
    {
        if (deMemCmp(&initialVector[i], &resultsVector[i], sizeof(resultsVector[i])) == 0)
        {
            std::ostringstream msg;
            msg << "Result " << i << " was not modified by the implementation";
            TCU_FAIL(msg.str());
        }
    }
}

uint32_t QueryTestBase::getRequiredPassCount() const
{
    return m_requiredNumerOfPasses;
}

// Base class for all graphic tests
class GraphicQueryTestBase : public QueryTestBase
{
public:
    GraphicQueryTestBase(vkt::Context &context, bool copyResults, uint32_t seed);

protected:
    void initStateObjects(void);

protected:
    Move<VkPipeline> m_pipeline;
    Move<VkPipelineLayout> m_pipelineLayout;

    de::SharedPtr<Image> m_colorAttachmentImage;
    Move<VkImageView> m_attachmentView;

    Move<VkRenderPass> m_renderPass;
    Move<VkFramebuffer> m_framebuffer;

    de::SharedPtr<Buffer> m_vertexBuffer;

    VkFormat m_colorAttachmentFormat;
    uint32_t m_size;
};

GraphicQueryTestBase::GraphicQueryTestBase(vkt::Context &context, bool copyResults, uint32_t seed)
    : QueryTestBase(context, copyResults, seed)
    , m_colorAttachmentFormat(VK_FORMAT_R8G8B8A8_UNORM)
    , m_size(32)
{
}

void GraphicQueryTestBase::initStateObjects(void)
{
    const VkDevice device      = m_context.getDevice();
    const DeviceInterface &vkd = m_context.getDeviceInterface();

    //attachment images and views
    {
        VkExtent3D imageExtent = {
            m_size, // width
            m_size, // height
            1       // depth
        };

        const ImageCreateInfo colorImageCreateInfo(
            VK_IMAGE_TYPE_2D, m_colorAttachmentFormat, imageExtent, 1, 1, VK_SAMPLE_COUNT_1_BIT,
            VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);

        m_colorAttachmentImage =
            Image::createAndAlloc(vkd, device, colorImageCreateInfo, m_context.getDefaultAllocator(),
                                  m_context.getUniversalQueueFamilyIndex());

        const ImageViewCreateInfo attachmentViewInfo(m_colorAttachmentImage->object(), VK_IMAGE_VIEW_TYPE_2D,
                                                     m_colorAttachmentFormat);
        m_attachmentView = createImageView(vkd, device, &attachmentViewInfo);
    }

    // renderpass and framebuffer
    {
        RenderPassCreateInfo renderPassCreateInfo;
        renderPassCreateInfo.addAttachment(AttachmentDescription(m_colorAttachmentFormat,          // format
                                                                 VK_SAMPLE_COUNT_1_BIT,            // samples
                                                                 VK_ATTACHMENT_LOAD_OP_CLEAR,      // loadOp
                                                                 VK_ATTACHMENT_STORE_OP_DONT_CARE, // storeOp
                                                                 VK_ATTACHMENT_LOAD_OP_DONT_CARE,  // stencilLoadOp
                                                                 VK_ATTACHMENT_STORE_OP_DONT_CARE, // stencilLoadOp
                                                                 VK_IMAGE_LAYOUT_GENERAL,          // initialLauout
                                                                 VK_IMAGE_LAYOUT_GENERAL));        // finalLayout

        const VkAttachmentReference colorAttachmentReference = {
            0,                      // attachment
            VK_IMAGE_LAYOUT_GENERAL // layout
        };

        renderPassCreateInfo.addSubpass(SubpassDescription(VK_PIPELINE_BIND_POINT_GRAPHICS, // pipelineBindPoint
                                                           0,                               // flags
                                                           0,                               // inputCount
                                                           DE_NULL,                         // pInputAttachments
                                                           1,                               // colorCount
                                                           &colorAttachmentReference,       // pColorAttachments
                                                           DE_NULL,                         // pResolveAttachments
                                                           AttachmentReference(),           // depthStencilAttachment
                                                           0,                               // preserveCount
                                                           DE_NULL));                       // preserveAttachments

        m_renderPass = createRenderPass(vkd, device, &renderPassCreateInfo);

        std::vector<VkImageView> attachments(1);
        attachments[0] = *m_attachmentView;

        FramebufferCreateInfo framebufferCreateInfo(*m_renderPass, attachments, m_size, m_size, 1);
        m_framebuffer = createFramebuffer(vkd, device, &framebufferCreateInfo);
    }

    // pipeline
    {
        Unique<VkShaderModule> vs(createShaderModule(vkd, device, m_context.getBinaryCollection().get("vert"), 0));
        Unique<VkShaderModule> fs(createShaderModule(vkd, device, m_context.getBinaryCollection().get("frag"), 0));

        const PipelineCreateInfo::ColorBlendState::Attachment attachmentState;

        const PipelineLayoutCreateInfo pipelineLayoutCreateInfo;
        m_pipelineLayout = createPipelineLayout(vkd, device, &pipelineLayoutCreateInfo);

        const VkVertexInputBindingDescription vf_binding_desc = {
            0,                           // binding
            4 * (uint32_t)sizeof(float), // stride
            VK_VERTEX_INPUT_RATE_VERTEX  // inputRate
        };

        const VkVertexInputAttributeDescription vf_attribute_desc = {
            0,                             // location
            0,                             // binding
            VK_FORMAT_R32G32B32A32_SFLOAT, // format
            0                              // offset
        };

        const VkPipelineVertexInputStateCreateInfo vf_info = {
            VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, // sType
            NULL,                                                      // pNext
            0u,                                                        // flags
            1,                                                         // vertexBindingDescriptionCount
            &vf_binding_desc,                                          // pVertexBindingDescriptions
            1,                                                         // vertexAttributeDescriptionCount
            &vf_attribute_desc                                         // pVertexAttributeDescriptions
        };

        PipelineCreateInfo pipelineCreateInfo(*m_pipelineLayout, *m_renderPass, 0, 0);
        pipelineCreateInfo.addShader(PipelineCreateInfo::PipelineShaderStage(*vs, "main", VK_SHADER_STAGE_VERTEX_BIT));
        pipelineCreateInfo.addShader(
            PipelineCreateInfo::PipelineShaderStage(*fs, "main", VK_SHADER_STAGE_FRAGMENT_BIT));
        pipelineCreateInfo.addState(PipelineCreateInfo::InputAssemblerState(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST));
        pipelineCreateInfo.addState(PipelineCreateInfo::ColorBlendState(1, &attachmentState));
        const VkViewport viewport = makeViewport(m_size, m_size);
        const VkRect2D scissor    = makeRect2D(m_size, m_size);
        pipelineCreateInfo.addState(PipelineCreateInfo::ViewportState(1, std::vector<VkViewport>(1, viewport),
                                                                      std::vector<VkRect2D>(1, scissor)));
        pipelineCreateInfo.addState(
            PipelineCreateInfo::DepthStencilState(false, false, VK_COMPARE_OP_GREATER_OR_EQUAL));
        pipelineCreateInfo.addState(PipelineCreateInfo::RasterizerState());
        pipelineCreateInfo.addState(PipelineCreateInfo::MultiSampleState());
        pipelineCreateInfo.addState(vf_info);
        m_pipeline = createGraphicsPipeline(vkd, device, VK_NULL_HANDLE, &pipelineCreateInfo);
    }

    // vertex buffer
    {
        std::vector<tcu::Vec4> vertices(3);
        vertices[0] = tcu::Vec4(0.5, 0.5, 0.0, 1.0);
        vertices[1] = tcu::Vec4(0.5, 0.0, 0.0, 1.0);
        vertices[2] = tcu::Vec4(0.0, 0.5, 0.0, 1.0);

        const size_t kBufferSize = vertices.size() * sizeof(tcu::Vec4);
        m_vertexBuffer =
            Buffer::createAndAlloc(vkd, device, BufferCreateInfo(kBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT),
                                   m_context.getDefaultAllocator(), MemoryRequirement::HostVisible);

        tcu::Vec4 *ptr = reinterpret_cast<tcu::Vec4 *>(m_vertexBuffer->getBoundMemory().getHostPtr());
        deMemcpy(ptr, &vertices[0], kBufferSize);

        flushAlloc(vkd, device, m_vertexBuffer->getBoundMemory());
    }
}

class GraphicQueryTest : public GraphicQueryTestBase
{
public:
    GraphicQueryTest(vkt::Context &context, bool copyResults, uint32_t seed);
    tcu::TestStatus iterate(void);
};

GraphicQueryTest::GraphicQueryTest(vkt::Context &context, bool copyResults, uint32_t seed)
    : GraphicQueryTestBase(context, copyResults, seed)
{
}

tcu::TestStatus GraphicQueryTest::iterate(void)
{
    const DeviceInterface &vkd                = m_context.getDeviceInterface();
    const VkDevice device                     = m_context.getDevice();
    const VkQueue queue                       = m_context.getUniversalQueue();
    const auto qfIndex                        = m_context.getUniversalQueueFamilyIndex();
    const CmdPoolCreateInfo cmdPoolCreateInfo = qfIndex;
    Unique<VkCommandPool> cmdPool(createCommandPool(vkd, device, &cmdPoolCreateInfo));
    Unique<VkCommandBuffer> cmdBuffer(allocateCommandBuffer(vkd, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

    initStateObjects();
    setupCounters();

    vk::Unique<VkQueryPool> queryPool(createQueryPool(0, 1));

    {
        const ProfilingLockGuard guard(vkd, device);

        // reset query pool
        {
            Unique<VkCommandBuffer> resetCmdBuffer(
                allocateCommandBuffer(vkd, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));
            const Unique<VkFence> fence(createFence(vkd, device));
            const VkSubmitInfo submitInfo = {
                VK_STRUCTURE_TYPE_SUBMIT_INFO,         // sType
                DE_NULL,                               // pNext
                0u,                                    // waitSemaphoreCount
                DE_NULL,                               // pWaitSemaphores
                (const VkPipelineStageFlags *)DE_NULL, // pWaitDstStageMask
                1u,                                    // commandBufferCount
                &resetCmdBuffer.get(),                 // pCommandBuffers
                0u,                                    // signalSemaphoreCount
                DE_NULL,                               // pSignalSemaphores
            };

            beginCommandBuffer(vkd, *resetCmdBuffer);
            vkd.cmdResetQueryPool(*resetCmdBuffer, *queryPool, 0u, 1u);
            endCommandBuffer(vkd, *resetCmdBuffer);

            VK_CHECK(vkd.queueSubmit(queue, 1u, &submitInfo, *fence));
            VK_CHECK(vkd.waitForFences(device, 1u, &fence.get(), true, ~0ull));
        }

        // begin command buffer
        beginCommandBuffer(vkd, *cmdBuffer, 0u);

        initialTransitionColor2DImage(vkd, *cmdBuffer, m_colorAttachmentImage->object(), VK_IMAGE_LAYOUT_GENERAL,
                                      VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                                      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

        // begin render pass
        VkClearValue renderPassClearValue;
        deMemset(&renderPassClearValue, 0, sizeof(VkClearValue));

        // perform query during triangle draw
        vkd.cmdBeginQuery(*cmdBuffer, *queryPool, 0, 0u);

        beginRenderPass(vkd, *cmdBuffer, *m_renderPass, *m_framebuffer, makeRect2D(0, 0, m_size, m_size), 1,
                        &renderPassClearValue);

        // bind pipeline
        vkd.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipeline);

        // bind vertex buffer
        VkBuffer vertexBuffer                 = m_vertexBuffer->object();
        const VkDeviceSize vertexBufferOffset = 0;
        vkd.cmdBindVertexBuffers(*cmdBuffer, 0, 1, &vertexBuffer, &vertexBufferOffset);

        vkd.cmdDraw(*cmdBuffer, 3, 1, 0, 0);

        endRenderPass(vkd, *cmdBuffer);

        vkd.cmdEndQuery(*cmdBuffer, *queryPool, 0);

        transition2DImage(vkd, *cmdBuffer, m_colorAttachmentImage->object(), VK_IMAGE_ASPECT_COLOR_BIT,
                          VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                          VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
                          VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

        endCommandBuffer(vkd, *cmdBuffer);

        // submit command buffer for each pass and wait for its completion
        const auto requiredPassCount = getRequiredPassCount();
        for (uint32_t passIndex = 0; passIndex < requiredPassCount; passIndex++)
        {
            const Unique<VkFence> fence(createFence(vkd, device));

            VkPerformanceQuerySubmitInfoKHR performanceQuerySubmitInfo = {
                VK_STRUCTURE_TYPE_PERFORMANCE_QUERY_SUBMIT_INFO_KHR, NULL, passIndex};

            const VkSubmitInfo submitInfo = {
                VK_STRUCTURE_TYPE_SUBMIT_INFO,         // sType
                &performanceQuerySubmitInfo,           // pNext
                0u,                                    // waitSemaphoreCount
                DE_NULL,                               // pWaitSemaphores
                (const VkPipelineStageFlags *)DE_NULL, // pWaitDstStageMask
                1u,                                    // commandBufferCount
                &cmdBuffer.get(),                      // pCommandBuffers
                0u,                                    // signalSemaphoreCount
                DE_NULL,                               // pSignalSemaphores
            };

            VK_CHECK(vkd.queueSubmit(queue, 1u, &submitInfo, *fence));
            VK_CHECK(vkd.waitForFences(device, 1u, &fence.get(), true, ~0ull));
        }
    }

    VK_CHECK(vkd.resetCommandBuffer(*cmdBuffer, 0));

    verifyQueryResults(qfIndex, queue, *queryPool);
    return tcu::TestStatus::pass("Pass");
}

class GraphicMultiplePoolsTest : public GraphicQueryTestBase
{
public:
    GraphicMultiplePoolsTest(vkt::Context &context, bool copyResults, uint32_t seed);
    tcu::TestStatus iterate(void);
};

GraphicMultiplePoolsTest::GraphicMultiplePoolsTest(vkt::Context &context, bool copyResults, uint32_t seed)
    : GraphicQueryTestBase(context, copyResults, seed)
{
}

tcu::TestStatus GraphicMultiplePoolsTest::iterate(void)
{
    const DeviceInterface &vkd                = m_context.getDeviceInterface();
    const VkDevice device                     = m_context.getDevice();
    const VkQueue queue                       = m_context.getUniversalQueue();
    const auto qfIndex                        = m_context.getUniversalQueueFamilyIndex();
    const CmdPoolCreateInfo cmdPoolCreateInfo = qfIndex;
    Unique<VkCommandPool> cmdPool(createCommandPool(vkd, device, &cmdPoolCreateInfo));
    Unique<VkCommandBuffer> cmdBuffer(allocateCommandBuffer(vkd, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

    initStateObjects();
    setupCounters();

    vk::Unique<VkQueryPool> queryPool1(createQueryPool(0, 2)), queryPool2(createQueryPool(1, 2));

    {
        const ProfilingLockGuard guard(vkd, device);

        // reset query pools
        {
            Unique<VkCommandBuffer> resetCmdBuffer(
                allocateCommandBuffer(vkd, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));
            const Unique<VkFence> fence(createFence(vkd, device));
            const VkSubmitInfo submitInfo = {
                VK_STRUCTURE_TYPE_SUBMIT_INFO,         // sType
                DE_NULL,                               // pNext
                0u,                                    // waitSemaphoreCount
                DE_NULL,                               // pWaitSemaphores
                (const VkPipelineStageFlags *)DE_NULL, // pWaitDstStageMask
                1u,                                    // commandBufferCount
                &resetCmdBuffer.get(),                 // pCommandBuffers
                0u,                                    // signalSemaphoreCount
                DE_NULL,                               // pSignalSemaphores
            };

            beginCommandBuffer(vkd, *resetCmdBuffer);
            vkd.cmdResetQueryPool(*resetCmdBuffer, *queryPool1, 0u, 1u);
            vkd.cmdResetQueryPool(*resetCmdBuffer, *queryPool2, 0u, 1u);
            endCommandBuffer(vkd, *resetCmdBuffer);

            VK_CHECK(vkd.queueSubmit(queue, 1u, &submitInfo, *fence));
            VK_CHECK(vkd.waitForFences(device, 1u, &fence.get(), true, ~0ull));
        }

        // begin command buffer
        beginCommandBuffer(vkd, *cmdBuffer, 0u);

        initialTransitionColor2DImage(vkd, *cmdBuffer, m_colorAttachmentImage->object(), VK_IMAGE_LAYOUT_GENERAL,
                                      VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                                      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

        // begin render pass
        VkClearValue renderPassClearValue;
        deMemset(&renderPassClearValue, 0, sizeof(VkClearValue));

        VkBuffer vertexBuffer                 = m_vertexBuffer->object();
        const VkDeviceSize vertexBufferOffset = 0;
        const VkQueryPool queryPools[]        = {*queryPool1, *queryPool2};

        // perform two queries during triangle draw
        for (uint32_t loop = 0; loop < DE_LENGTH_OF_ARRAY(queryPools); ++loop)
        {
            const VkQueryPool queryPool = queryPools[loop];
            vkd.cmdBeginQuery(*cmdBuffer, queryPool, 0u, (VkQueryControlFlags)0u);
            beginRenderPass(vkd, *cmdBuffer, *m_renderPass, *m_framebuffer, makeRect2D(0, 0, m_size, m_size), 1,
                            &renderPassClearValue);

            vkd.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipeline);
            vkd.cmdBindVertexBuffers(*cmdBuffer, 0, 1, &vertexBuffer, &vertexBufferOffset);
            vkd.cmdDraw(*cmdBuffer, 3, 1, 0, 0);

            endRenderPass(vkd, *cmdBuffer);
            vkd.cmdEndQuery(*cmdBuffer, queryPool, 0u);
        }

        transition2DImage(vkd, *cmdBuffer, m_colorAttachmentImage->object(), VK_IMAGE_ASPECT_COLOR_BIT,
                          VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                          VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
                          VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

        endCommandBuffer(vkd, *cmdBuffer);

        // submit command buffer for each pass and wait for its completion
        const auto requiredPassCount = getRequiredPassCount();
        for (uint32_t passIndex = 0; passIndex < requiredPassCount; passIndex++)
        {
            const Unique<VkFence> fence(createFence(vkd, device));

            VkPerformanceQuerySubmitInfoKHR performanceQuerySubmitInfo = {
                VK_STRUCTURE_TYPE_PERFORMANCE_QUERY_SUBMIT_INFO_KHR, NULL, passIndex};

            const VkSubmitInfo submitInfo = {
                VK_STRUCTURE_TYPE_SUBMIT_INFO,         // sType
                &performanceQuerySubmitInfo,           // pNext
                0u,                                    // waitSemaphoreCount
                DE_NULL,                               // pWaitSemaphores
                (const VkPipelineStageFlags *)DE_NULL, // pWaitDstStageMask
                1u,                                    // commandBufferCount
                &cmdBuffer.get(),                      // pCommandBuffers
                0u,                                    // signalSemaphoreCount
                DE_NULL,                               // pSignalSemaphores
            };

            VK_CHECK(vkd.queueSubmit(queue, 1u, &submitInfo, *fence));
            VK_CHECK(vkd.waitForFences(device, 1u, &fence.get(), true, ~0ull));
        }
    }

    VK_CHECK(vkd.resetCommandBuffer(*cmdBuffer, 0));

    verifyQueryResults(qfIndex, queue, *queryPool1);
    verifyQueryResults(qfIndex, queue, *queryPool2);
    return tcu::TestStatus::pass("Pass");
}

// Base class for all compute tests
class ComputeQueryTestBase : public QueryTestBase
{
public:
    ComputeQueryTestBase(vkt::Context &context, bool copyResults, uint32_t seed);

protected:
    void initStateObjects(void);

protected:
    Move<VkPipeline> m_pipeline;
    Move<VkPipelineLayout> m_pipelineLayout;
    de::SharedPtr<Buffer> m_buffer;
    Move<VkDescriptorPool> m_descriptorPool;
    Move<VkDescriptorSet> m_descriptorSet;
    VkDescriptorBufferInfo m_descriptorBufferInfo;
    VkBufferMemoryBarrier m_computeFinishBarrier;
};

ComputeQueryTestBase::ComputeQueryTestBase(vkt::Context &context, bool copyResults, uint32_t seed)
    : QueryTestBase(context, copyResults, seed)
{
}

void ComputeQueryTestBase::initStateObjects(void)
{
    const DeviceInterface &vkd    = m_context.getDeviceInterface();
    const VkDevice device         = m_context.getDevice();
    const VkDeviceSize bufferSize = 32 * sizeof(uint32_t);
    const CmdPoolCreateInfo cmdPoolCreateInfo(m_context.getUniversalQueueFamilyIndex());
    const Unique<VkCommandPool> cmdPool(createCommandPool(vkd, device, &cmdPoolCreateInfo));
    const Unique<VkCommandBuffer> cmdBuffer(
        allocateCommandBuffer(vkd, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

    const Unique<VkDescriptorSetLayout> descriptorSetLayout(
        DescriptorSetLayoutBuilder()
            .addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
            .build(vkd, device));

    // create pipeline layout
    {
        const VkPipelineLayoutCreateInfo pipelineLayoutParams = {
            VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, // sType
            DE_NULL,                                       // pNext
            0u,                                            // flags
            1u,                                            // setLayoutCount
            &(*descriptorSetLayout),                       // pSetLayouts
            0u,                                            // pushConstantRangeCount
            DE_NULL,                                       // pPushConstantRanges
        };
        m_pipelineLayout = createPipelineLayout(vkd, device, &pipelineLayoutParams);
    }

    // create compute pipeline
    {
        const Unique<VkShaderModule> cs(
            createShaderModule(vkd, device, m_context.getBinaryCollection().get("comp"), 0u));
        const VkPipelineShaderStageCreateInfo pipelineShaderStageParams = {
            VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, // sType
            DE_NULL,                                             // pNext
            (VkPipelineShaderStageCreateFlags)0u,                // flags
            VK_SHADER_STAGE_COMPUTE_BIT,                         // stage
            *cs,                                                 // module
            "main",                                              // pName
            DE_NULL,                                             // pSpecializationInfo
        };
        const VkComputePipelineCreateInfo pipelineCreateInfo = {
            VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO, // sType
            DE_NULL,                                        // pNext
            (VkPipelineCreateFlags)0u,                      // flags
            pipelineShaderStageParams,                      // stage
            *m_pipelineLayout,                              // layout
            VK_NULL_HANDLE,                                 // basePipelineHandle
            0,                                              // basePipelineIndex
        };
        m_pipeline = createComputePipeline(vkd, device, VK_NULL_HANDLE, &pipelineCreateInfo);
    }

    m_buffer = Buffer::createAndAlloc(vkd, device, BufferCreateInfo(bufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT),
                                      m_context.getDefaultAllocator(), MemoryRequirement::HostVisible);
    m_descriptorPool = DescriptorPoolBuilder()
                           .addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
                           .build(vkd, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);
    const VkDescriptorSetAllocateInfo allocateParams = {
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, // sType
        DE_NULL,                                        // pNext
        *m_descriptorPool,                              // descriptorPool
        1u,                                             // setLayoutCount
        &(*descriptorSetLayout),                        // pSetLayouts
    };

    m_descriptorSet                             = allocateDescriptorSet(vkd, device, &allocateParams);
    const VkDescriptorBufferInfo descriptorInfo = {
        m_buffer->object(), // buffer
        0ull,               // offset
        bufferSize,         // range
    };

    DescriptorSetUpdateBuilder()
        .writeSingle(*m_descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u),
                     VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &descriptorInfo)
        .update(vkd, device);

    // clear buffer
    const std::vector<uint8_t> data((size_t)bufferSize, 0u);
    const Allocation &allocation = m_buffer->getBoundMemory();
    void *allocationData         = allocation.getHostPtr();
    invalidateAlloc(vkd, device, allocation);
    deMemcpy(allocationData, &data[0], (size_t)bufferSize);

    const VkBufferMemoryBarrier barrier = {
        VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,                // sType
        DE_NULL,                                                // pNext
        VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, // srcAccessMask
        VK_ACCESS_HOST_READ_BIT,                                // dstAccessMask
        VK_QUEUE_FAMILY_IGNORED,                                // srcQueueFamilyIndex
        VK_QUEUE_FAMILY_IGNORED,                                // destQueueFamilyIndex
        m_buffer->object(),                                     // buffer
        0ull,                                                   // offset
        bufferSize,                                             // size
    };
    m_computeFinishBarrier = barrier;
}

class ComputeQueryTest : public ComputeQueryTestBase
{
public:
    ComputeQueryTest(vkt::Context &context, bool copyResults, uint32_t seed);
    tcu::TestStatus iterate(void);
};

ComputeQueryTest::ComputeQueryTest(vkt::Context &context, bool copyResults, uint32_t seed)
    : ComputeQueryTestBase(context, copyResults, seed)
{
}

tcu::TestStatus ComputeQueryTest::iterate(void)
{
    const DeviceInterface &vkd = m_context.getDeviceInterface();
    const VkDevice device      = m_context.getDevice();
    const VkQueue queue        = m_context.getUniversalQueue();
    const auto qfIndex         = m_context.getUniversalQueueFamilyIndex();
    const CmdPoolCreateInfo cmdPoolCreateInfo(qfIndex);
    const Unique<VkCommandPool> cmdPool(createCommandPool(vkd, device, &cmdPoolCreateInfo));
    const Unique<VkCommandBuffer> resetCmdBuffer(
        allocateCommandBuffer(vkd, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));
    const Unique<VkCommandBuffer> cmdBuffer(
        allocateCommandBuffer(vkd, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

    initStateObjects();
    setupCounters();

    vk::Unique<VkQueryPool> queryPool(createQueryPool(0, 1));

    {
        const ProfilingLockGuard guard(vkd, device);

        beginCommandBuffer(vkd, *resetCmdBuffer);
        vkd.cmdResetQueryPool(*resetCmdBuffer, *queryPool, 0u, 1u);
        endCommandBuffer(vkd, *resetCmdBuffer);

        beginCommandBuffer(vkd, *cmdBuffer, 0u);
        vkd.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *m_pipeline);
        vkd.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *m_pipelineLayout, 0u, 1u,
                                  &(m_descriptorSet.get()), 0u, DE_NULL);

        vkd.cmdBeginQuery(*cmdBuffer, *queryPool, 0u, (VkQueryControlFlags)0u);
        vkd.cmdDispatch(*cmdBuffer, 2, 2, 2);
        vkd.cmdEndQuery(*cmdBuffer, *queryPool, 0u);

        vkd.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                               (VkDependencyFlags)0u, 0u, (const VkMemoryBarrier *)DE_NULL, 1u, &m_computeFinishBarrier,
                               0u, (const VkImageMemoryBarrier *)DE_NULL);
        endCommandBuffer(vkd, *cmdBuffer);

        // submit reset of queries only once
        {
            const VkSubmitInfo submitInfo = {
                VK_STRUCTURE_TYPE_SUBMIT_INFO,         // sType
                DE_NULL,                               // pNext
                0u,                                    // waitSemaphoreCount
                DE_NULL,                               // pWaitSemaphores
                (const VkPipelineStageFlags *)DE_NULL, // pWaitDstStageMask
                1u,                                    // commandBufferCount
                &resetCmdBuffer.get(),                 // pCommandBuffers
                0u,                                    // signalSemaphoreCount
                DE_NULL,                               // pSignalSemaphores
            };

            VK_CHECK(vkd.queueSubmit(queue, 1u, &submitInfo, VK_NULL_HANDLE));
        }

        // submit command buffer for each pass and wait for its completion
        const auto requiredPassCount = getRequiredPassCount();
        for (uint32_t passIndex = 0; passIndex < requiredPassCount; passIndex++)
        {
            const Unique<VkFence> fence(createFence(vkd, device));

            VkPerformanceQuerySubmitInfoKHR performanceQuerySubmitInfo = {
                VK_STRUCTURE_TYPE_PERFORMANCE_QUERY_SUBMIT_INFO_KHR, NULL, passIndex};

            const VkSubmitInfo submitInfo = {
                VK_STRUCTURE_TYPE_SUBMIT_INFO,         // sType
                &performanceQuerySubmitInfo,           // pNext
                0u,                                    // waitSemaphoreCount
                DE_NULL,                               // pWaitSemaphores
                (const VkPipelineStageFlags *)DE_NULL, // pWaitDstStageMask
                1u,                                    // commandBufferCount
                &cmdBuffer.get(),                      // pCommandBuffers
                0u,                                    // signalSemaphoreCount
                DE_NULL,                               // pSignalSemaphores
            };

            VK_CHECK(vkd.queueSubmit(queue, 1u, &submitInfo, *fence));
            VK_CHECK(vkd.waitForFences(device, 1u, &fence.get(), true, ~0ull));
        }
    }

    VK_CHECK(vkd.resetCommandBuffer(*cmdBuffer, 0));

    verifyQueryResults(qfIndex, queue, *queryPool);
    return tcu::TestStatus::pass("Pass");
}

class ComputeMultiplePoolsTest : public ComputeQueryTestBase
{
public:
    ComputeMultiplePoolsTest(vkt::Context &context, bool copyResults, uint32_t seed);
    tcu::TestStatus iterate(void);
};

ComputeMultiplePoolsTest::ComputeMultiplePoolsTest(vkt::Context &context, bool copyResults, uint32_t seed)
    : ComputeQueryTestBase(context, copyResults, seed)
{
}

tcu::TestStatus ComputeMultiplePoolsTest::iterate(void)
{
    const DeviceInterface &vkd = m_context.getDeviceInterface();
    const VkDevice device      = m_context.getDevice();
    const VkQueue queue        = m_context.getUniversalQueue();
    const auto qfIndex         = m_context.getUniversalQueueFamilyIndex();
    const CmdPoolCreateInfo cmdPoolCreateInfo(qfIndex);
    const Unique<VkCommandPool> cmdPool(createCommandPool(vkd, device, &cmdPoolCreateInfo));
    const Unique<VkCommandBuffer> resetCmdBuffer(
        allocateCommandBuffer(vkd, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));
    const Unique<VkCommandBuffer> cmdBuffer(
        allocateCommandBuffer(vkd, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

    initStateObjects();
    setupCounters();

    vk::Unique<VkQueryPool> queryPool1(createQueryPool(0, 2)), queryPool2(createQueryPool(1, 2));

    {
        const ProfilingLockGuard guard(vkd, device);

        const VkQueryPool queryPools[] = {*queryPool1, *queryPool2};

        beginCommandBuffer(vkd, *resetCmdBuffer);
        vkd.cmdResetQueryPool(*resetCmdBuffer, queryPools[0], 0u, 1u);
        vkd.cmdResetQueryPool(*resetCmdBuffer, queryPools[1], 0u, 1u);
        endCommandBuffer(vkd, *resetCmdBuffer);

        beginCommandBuffer(vkd, *cmdBuffer, 0u);
        vkd.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *m_pipeline);
        vkd.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *m_pipelineLayout, 0u, 1u,
                                  &(m_descriptorSet.get()), 0u, DE_NULL);

        // perform two queries
        for (uint32_t loop = 0; loop < DE_LENGTH_OF_ARRAY(queryPools); ++loop)
        {
            const VkQueryPool queryPool = queryPools[loop];
            vkd.cmdBeginQuery(*cmdBuffer, queryPool, 0u, (VkQueryControlFlags)0u);
            vkd.cmdDispatch(*cmdBuffer, 2, 2, 2);
            vkd.cmdEndQuery(*cmdBuffer, queryPool, 0u);
        }

        vkd.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                               (VkDependencyFlags)0u, 0u, (const VkMemoryBarrier *)DE_NULL, 1u, &m_computeFinishBarrier,
                               0u, (const VkImageMemoryBarrier *)DE_NULL);
        endCommandBuffer(vkd, *cmdBuffer);

        // submit reset of queries only once
        {
            const VkSubmitInfo submitInfo = {
                VK_STRUCTURE_TYPE_SUBMIT_INFO,         // sType
                DE_NULL,                               // pNext
                0u,                                    // waitSemaphoreCount
                DE_NULL,                               // pWaitSemaphores
                (const VkPipelineStageFlags *)DE_NULL, // pWaitDstStageMask
                1u,                                    // commandBufferCount
                &resetCmdBuffer.get(),                 // pCommandBuffers
                0u,                                    // signalSemaphoreCount
                DE_NULL,                               // pSignalSemaphores
            };

            VK_CHECK(vkd.queueSubmit(queue, 1u, &submitInfo, VK_NULL_HANDLE));
        }

        // submit command buffer for each pass and wait for its completion
        const auto requiredPassCount = getRequiredPassCount();
        for (uint32_t passIndex = 0; passIndex < requiredPassCount; passIndex++)
        {
            const Unique<VkFence> fence(createFence(vkd, device));

            VkPerformanceQuerySubmitInfoKHR performanceQuerySubmitInfo = {
                VK_STRUCTURE_TYPE_PERFORMANCE_QUERY_SUBMIT_INFO_KHR, NULL, passIndex};

            const VkSubmitInfo submitInfo = {
                VK_STRUCTURE_TYPE_SUBMIT_INFO,         // sType
                &performanceQuerySubmitInfo,           // pNext
                0u,                                    // waitSemaphoreCount
                DE_NULL,                               // pWaitSemaphores
                (const VkPipelineStageFlags *)DE_NULL, // pWaitDstStageMask
                1u,                                    // commandBufferCount
                &cmdBuffer.get(),                      // pCommandBuffers
                0u,                                    // signalSemaphoreCount
                DE_NULL,                               // pSignalSemaphores
            };

            VK_CHECK(vkd.queueSubmit(queue, 1u, &submitInfo, *fence));
            VK_CHECK(vkd.waitForFences(device, 1u, &fence.get(), true, ~0ull));
        }
    }

    VK_CHECK(vkd.resetCommandBuffer(*cmdBuffer, 0));

    verifyQueryResults(qfIndex, queue, *queryPool1);
    verifyQueryResults(qfIndex, queue, *queryPool2);
    return tcu::TestStatus::pass("Pass");
}

enum TestType
{
    TT_ENUMERATE_AND_VALIDATE = 0,
    TT_QUERY,
    TT_MULTIPLE_POOLS
};

class QueryPoolPerformanceTest : public TestCase
{
public:
    QueryPoolPerformanceTest(tcu::TestContext &context, TestType testType, VkQueueFlagBits queueFlagBits,
                             bool copyResults, uint32_t seed, const std::string &name)
        : TestCase(context, name)
        , m_testType(testType)
        , m_queueFlagBits(queueFlagBits)
        , m_copyResults(copyResults)
        , m_seed(seed)
    {
    }

    vkt::TestInstance *createInstance(vkt::Context &context) const override
    {
        if (m_testType == TT_ENUMERATE_AND_VALIDATE)
            return new EnumerateAndValidateTest(context, m_queueFlagBits);

        if (m_queueFlagBits == VK_QUEUE_GRAPHICS_BIT)
        {
            if (m_testType == TT_QUERY)
                return new GraphicQueryTest(context, m_copyResults, m_seed);
            return new GraphicMultiplePoolsTest(context, m_copyResults, m_seed);
        }

        // tests for VK_QUEUE_COMPUTE_BIT
        if (m_testType == TT_QUERY)
            return new ComputeQueryTest(context, m_copyResults, m_seed);
        return new ComputeMultiplePoolsTest(context, m_copyResults, m_seed);
    }

    void initPrograms(SourceCollections &programCollection) const override
    {
        // validation test do not need programs
        if (m_testType == TT_ENUMERATE_AND_VALIDATE)
            return;

        if (m_queueFlagBits == VK_QUEUE_COMPUTE_BIT)
        {
            programCollection.glslSources.add("comp")
                << glu::ComputeSource("#version 430\n"
                                      "layout (local_size_x = 1) in;\n"
                                      "layout(binding = 0) writeonly buffer Output {\n"
                                      "        uint values[];\n"
                                      "} sb_out;\n\n"
                                      "void main (void) {\n"
                                      "        uint index = uint(gl_GlobalInvocationID.x);\n"
                                      "        sb_out.values[index] += gl_GlobalInvocationID.y*2;\n"
                                      "}\n");
            return;
        }

        programCollection.glslSources.add("frag")
            << glu::FragmentSource("#version 430\n"
                                   "layout(location = 0) out vec4 out_FragColor;\n"
                                   "void main()\n"
                                   "{\n"
                                   "    out_FragColor = vec4(1.0, 0.0, 0.0, 1.0);\n"
                                   "}\n");

        programCollection.glslSources.add("vert")
            << glu::VertexSource("#version 430\n"
                                 "layout(location = 0) in vec4 in_Position;\n"
                                 "out gl_PerVertex { vec4 gl_Position; float gl_PointSize; };\n"
                                 "void main() {\n"
                                 "    gl_Position  = in_Position;\n"
                                 "    gl_PointSize = 1.0;\n"
                                 "}\n");
    }

    void checkSupport(Context &context) const override
    {
        const auto &perfQueryFeatures = context.getPerformanceQueryFeatures();

        if (!perfQueryFeatures.performanceCounterQueryPools)
            TCU_THROW(NotSupportedError, "performanceCounterQueryPools not supported");

        if (m_testType == TT_MULTIPLE_POOLS && !perfQueryFeatures.performanceCounterMultipleQueryPools)
            TCU_THROW(NotSupportedError, "performanceCounterMultipleQueryPools not supported");

        const auto &vki           = context.getInstanceInterface();
        const auto physicalDevice = context.getPhysicalDevice();
        const auto qfIndex        = context.getUniversalQueueFamilyIndex();

        // Get the number of supported counters;
        uint32_t counterCount;
        VK_CHECK(vki.enumeratePhysicalDeviceQueueFamilyPerformanceQueryCountersKHR(physicalDevice, qfIndex,
                                                                                   &counterCount, NULL, NULL));

        if (!counterCount)
            TCU_THROW(QualityWarning, "There are no performance counters");

        if (m_copyResults && !context.getPerformanceQueryProperties().allowCommandBufferQueryCopies)
            TCU_THROW(NotSupportedError,
                      "VkPhysicalDevicePerformanceQueryPropertiesKHR::allowCommandBufferQueryCopies not supported");
    }

private:
    TestType m_testType;
    VkQueueFlagBits m_queueFlagBits;
    const bool m_copyResults;
    const uint32_t m_seed;
};

} // namespace

QueryPoolPerformanceTests::QueryPoolPerformanceTests(tcu::TestContext &testCtx)
    : TestCaseGroup(testCtx, "performance_query")
{
}

void QueryPoolPerformanceTests::init(void)
{

    const struct
    {
        const bool copyResults;
        const std::string suffix;
    } copyCases[]{
        {false, ""},
        {true, "_copy"},
    };

    uint32_t seed = 1692187611u;
    for (const auto &copyCase : copyCases)
    {
        addChild(new QueryPoolPerformanceTest(m_testCtx, TT_ENUMERATE_AND_VALIDATE, VK_QUEUE_GRAPHICS_BIT,
                                              copyCase.copyResults, seed++,
                                              "enumerate_and_validate_graphic" + copyCase.suffix));
        addChild(new QueryPoolPerformanceTest(m_testCtx, TT_ENUMERATE_AND_VALIDATE, VK_QUEUE_COMPUTE_BIT,
                                              copyCase.copyResults, seed++,
                                              "enumerate_and_validate_compute" + copyCase.suffix));
        addChild(new QueryPoolPerformanceTest(m_testCtx, TT_QUERY, VK_QUEUE_GRAPHICS_BIT, copyCase.copyResults, seed++,
                                              "query_graphic" + copyCase.suffix));
        addChild(new QueryPoolPerformanceTest(m_testCtx, TT_QUERY, VK_QUEUE_COMPUTE_BIT, copyCase.copyResults, seed++,
                                              "query_compute" + copyCase.suffix));
        addChild(new QueryPoolPerformanceTest(m_testCtx, TT_MULTIPLE_POOLS, VK_QUEUE_GRAPHICS_BIT, copyCase.copyResults,
                                              seed++, "multiple_pools_graphic" + copyCase.suffix));
        addChild(new QueryPoolPerformanceTest(m_testCtx, TT_MULTIPLE_POOLS, VK_QUEUE_COMPUTE_BIT, copyCase.copyResults,
                                              seed++, "multiple_pools_compute" + copyCase.suffix));
    }
}

} // namespace QueryPool
} // namespace vkt
