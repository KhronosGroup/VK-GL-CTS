/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2018 The Khronos Group Inc.
 * Copyright (c) 2018 Danylo Piliaiev <danylo.piliaiev@gmail.com>
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
 * \brief Test for conditional rendering of vkCmdDispatch* functions
 *//*--------------------------------------------------------------------*/

#include "vktConditionalDispatchTests.hpp"
#include "vktConditionalRenderingTestUtil.hpp"

#include "vkDefs.hpp"
#include "vkCmdUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkBufferWithMemory.hpp"

namespace vkt
{
namespace conditional
{
namespace
{

enum DispatchCommandType
{
    DISPATCH_COMMAND_TYPE_DISPATCH = 0,
    DISPATCH_COMMAND_TYPE_DISPATCH_INDIRECT,
    DISPATCH_COMMAND_TYPE_DISPATCH_BASE,
    DISPATCH_COMMAND_TYPE_DISPATCH_LAST
};

const char *getDispatchCommandTypeName(DispatchCommandType command)
{
    switch (command)
    {
    case DISPATCH_COMMAND_TYPE_DISPATCH:
        return "dispatch";
    case DISPATCH_COMMAND_TYPE_DISPATCH_INDIRECT:
        return "dispatch_indirect";
    case DISPATCH_COMMAND_TYPE_DISPATCH_BASE:
        return "dispatch_base";
    default:
        DE_ASSERT(false);
    }
    return "";
}

struct ConditionalTestSpec
{
    ConditionalTestSpec()
    {
        deMemset(this, 0, sizeof(*this));
    }

    ConditionalTestSpec(DispatchCommandType command_, int numCalls_, ConditionalData conditionalData_,
                        bool computeQueue_)
        : command(command_)
        , numCalls(numCalls_)
        , conditionalData(conditionalData_)
        , computeQueue(computeQueue_)
    {
    }

    DispatchCommandType command;
    int numCalls;
    ConditionalData conditionalData;
    bool computeQueue;
};

class ConditionalDispatchTest : public vkt::TestCase
{
public:
    ConditionalDispatchTest(tcu::TestContext &testCtx, const std::string &name, const ConditionalTestSpec &testSpec);

    void initPrograms(vk::SourceCollections &sourceCollections) const;
    void checkSupport(Context &context) const;
    TestInstance *createInstance(Context &context) const;

private:
    const ConditionalTestSpec m_testSpec;
};

class ConditionalDispatchTestInstance : public TestInstance
{
public:
    ConditionalDispatchTestInstance(Context &context, ConditionalTestSpec testSpec);

    virtual tcu::TestStatus iterate(void);
    void recordDispatch(const vk::DeviceInterface &vk, vk::VkCommandBuffer cmdBuffer,
                        vk::BufferWithMemory &indirectBuffer);

protected:
    const ConditionalTestSpec m_testSpec;
};

ConditionalDispatchTest::ConditionalDispatchTest(tcu::TestContext &testCtx, const std::string &name,
                                                 const ConditionalTestSpec &testSpec)
    : TestCase(testCtx, name)
    , m_testSpec(testSpec)
{
}

void ConditionalDispatchTest::initPrograms(vk::SourceCollections &sourceCollections) const
{
    std::ostringstream src;
    src << "#version 310 es\n"
        << "layout(local_size_x = 1u, local_size_y = 1u, local_size_z = 1u) in;\n"
        << "layout(set = 0, binding = 0, std140) buffer Out\n"
        << "{\n"
        << "    coherent uint count;\n"
        << "};\n"
        << "void main(void)\n"
        << "{\n"
        << "    atomicAdd(count, 1u);\n"
        << "}\n";

    sourceCollections.glslSources.add("comp") << glu::ComputeSource(src.str());
}

void ConditionalDispatchTest::checkSupport(Context &context) const
{
    checkConditionalRenderingCapabilities(context, m_testSpec.conditionalData);

    if (m_testSpec.computeQueue)
        context.getComputeQueue(); // Will throw NotSupportedError if not found.

    if (m_testSpec.command == DISPATCH_COMMAND_TYPE_DISPATCH_BASE)
        context.requireDeviceFunctionality("VK_KHR_device_group");
}

TestInstance *ConditionalDispatchTest::createInstance(Context &context) const
{
    return new ConditionalDispatchTestInstance(context, m_testSpec);
}

ConditionalDispatchTestInstance::ConditionalDispatchTestInstance(Context &context, ConditionalTestSpec testSpec)
    : TestInstance(context)
    , m_testSpec(testSpec)
{
}

void ConditionalDispatchTestInstance::recordDispatch(const vk::DeviceInterface &vk, vk::VkCommandBuffer cmdBuffer,
                                                     vk::BufferWithMemory &indirectBuffer)
{
    for (int i = 0; i < m_testSpec.numCalls; i++)
    {
        switch (m_testSpec.command)
        {
        case DISPATCH_COMMAND_TYPE_DISPATCH:
        {
            vk.cmdDispatch(cmdBuffer, 1, 1, 1);
            break;
        }
        case DISPATCH_COMMAND_TYPE_DISPATCH_INDIRECT:
        {
            vk.cmdDispatchIndirect(cmdBuffer, *indirectBuffer, 0);
            break;
        }
        case DISPATCH_COMMAND_TYPE_DISPATCH_BASE:
        {
            vk.cmdDispatchBase(cmdBuffer, 0, 0, 0, 1, 1, 1);
            break;
        }
        default:
            DE_ASSERT(false);
        }
    }
}

tcu::TestStatus ConditionalDispatchTestInstance::iterate(void)
{
    const vk::DeviceInterface &vk = m_context.getDeviceInterface();
    const vk::VkDevice device     = m_context.getDevice();
    const vk::VkQueue queue = (m_testSpec.computeQueue ? m_context.getComputeQueue() : m_context.getUniversalQueue());
    const uint32_t queueFamilyIndex =
        (m_testSpec.computeQueue ? m_context.getComputeQueueFamilyIndex() : m_context.getUniversalQueueFamilyIndex());
    vk::Allocator &allocator    = m_context.getDefaultAllocator();
    const auto &conditionalData = m_testSpec.conditionalData;

    // Create a buffer and host-visible memory for it

    const vk::VkDeviceSize bufferSizeBytes = sizeof(uint32_t);
    const vk::BufferWithMemory outputBuffer(
        vk, device, allocator, vk::makeBufferCreateInfo(bufferSizeBytes, vk::VK_BUFFER_USAGE_STORAGE_BUFFER_BIT),
        vk::MemoryRequirement::HostVisible);

    {
        const vk::Allocation &alloc    = outputBuffer.getAllocation();
        uint8_t *outputBufferPtr       = reinterpret_cast<uint8_t *>(alloc.getHostPtr());
        *(uint32_t *)(outputBufferPtr) = 0;
        vk::flushAlloc(vk, device, alloc);
    }

    // Create descriptor set

    const vk::Unique<vk::VkDescriptorSetLayout> descriptorSetLayout(
        vk::DescriptorSetLayoutBuilder()
            .addSingleBinding(vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, vk::VK_SHADER_STAGE_COMPUTE_BIT)
            .build(vk, device));

    const vk::Unique<vk::VkDescriptorPool> descriptorPool(
        vk::DescriptorPoolBuilder()
            .addType(vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
            .build(vk, device, vk::VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u));

    const vk::Unique<vk::VkDescriptorSet> descriptorSet(
        makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout));

    const vk::VkDescriptorBufferInfo descriptorInfo =
        vk::makeDescriptorBufferInfo(*outputBuffer, 0ull, bufferSizeBytes);
    vk::DescriptorSetUpdateBuilder()
        .writeSingle(*descriptorSet, vk::DescriptorSetUpdateBuilder::Location::binding(0u),
                     vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &descriptorInfo)
        .update(vk, device);

    // Setup pipeline

    const vk::Unique<vk::VkShaderModule> shaderModule(
        createShaderModule(vk, device, m_context.getBinaryCollection().get("comp"), 0u));
    const vk::Unique<vk::VkPipelineLayout> pipelineLayout(makePipelineLayout(vk, device, *descriptorSetLayout));
    const vk::Unique<vk::VkPipeline> pipeline(makeComputePipeline(vk, device, *pipelineLayout, *shaderModule));

    const vk::Unique<vk::VkCommandPool> cmdPool(makeCommandPool(vk, device, queueFamilyIndex));
    const vk::Unique<vk::VkCommandBuffer> cmdBuffer(
        vk::allocateCommandBuffer(vk, device, *cmdPool, vk::VK_COMMAND_BUFFER_LEVEL_PRIMARY));
    const vk::Unique<vk::VkCommandBuffer> secondaryCmdBuffer(
        vk::allocateCommandBuffer(vk, device, *cmdPool, vk::VK_COMMAND_BUFFER_LEVEL_SECONDARY));
    const vk::Unique<vk::VkCommandBuffer> nestedCmdBuffer(
        vk::allocateCommandBuffer(vk, device, *cmdPool, vk::VK_COMMAND_BUFFER_LEVEL_SECONDARY));

    // Create indirect buffer
    const vk::VkDispatchIndirectCommand dispatchCommands[] = {{1u, 1u, 1u}};

    vk::BufferWithMemory indirectBuffer(
        vk, device, allocator,
        vk::makeBufferCreateInfo(sizeof(dispatchCommands),
                                 vk::VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | vk::VK_BUFFER_USAGE_STORAGE_BUFFER_BIT),
        vk::MemoryRequirement::HostVisible);

    uint8_t *indirectBufferPtr = reinterpret_cast<uint8_t *>(indirectBuffer.getAllocation().getHostPtr());
    deMemcpy(indirectBufferPtr, &dispatchCommands[0], sizeof(dispatchCommands));

    vk::flushAlloc(vk, device, indirectBuffer.getAllocation());

    // Start recording commands

    beginCommandBuffer(vk, *cmdBuffer);

    vk::VkCommandBuffer targetCmdBuffer = *cmdBuffer;

    const bool useSecondaryCmdBuffer =
        conditionalData.conditionInherited || conditionalData.conditionInSecondaryCommandBuffer;

    if (useSecondaryCmdBuffer)
    {
        const vk::VkCommandBufferInheritanceConditionalRenderingInfoEXT conditionalRenderingInheritanceInfo = {
            vk::VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_CONDITIONAL_RENDERING_INFO_EXT, DE_NULL,
            (conditionalData.conditionInherited ? VK_TRUE : VK_FALSE), // conditionalRenderingEnable
        };

        const vk::VkCommandBufferInheritanceInfo inheritanceInfo = {
            vk::VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO,
            &conditionalRenderingInheritanceInfo,
            VK_NULL_HANDLE,                        // renderPass
            0u,                                    // subpass
            VK_NULL_HANDLE,                        // framebuffer
            VK_FALSE,                              // occlusionQueryEnable
            (vk::VkQueryControlFlags)0u,           // queryFlags
            (vk::VkQueryPipelineStatisticFlags)0u, // pipelineStatistics
        };

        const vk::VkCommandBufferBeginInfo commandBufferBeginInfo = {
            vk::VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, DE_NULL, vk::VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
            &inheritanceInfo};

        if (conditionalData.secondaryCommandBufferNested)
        {
            VK_CHECK(vk.beginCommandBuffer(*nestedCmdBuffer, &commandBufferBeginInfo));
        }

        VK_CHECK(vk.beginCommandBuffer(*secondaryCmdBuffer, &commandBufferBeginInfo));

        targetCmdBuffer = *secondaryCmdBuffer;
    }

    vk.cmdBindPipeline(targetCmdBuffer, vk::VK_PIPELINE_BIND_POINT_COMPUTE, *pipeline);
    vk.cmdBindDescriptorSets(targetCmdBuffer, vk::VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayout, 0u, 1u,
                             &descriptorSet.get(), 0u, DE_NULL);

    de::SharedPtr<Draw::Buffer> conditionalBuffer =
        createConditionalRenderingBuffer(m_context, conditionalData, m_testSpec.computeQueue);

    if (conditionalData.conditionInSecondaryCommandBuffer)
    {
        beginConditionalRendering(vk, *secondaryCmdBuffer, *conditionalBuffer, conditionalData);
        recordDispatch(vk, *secondaryCmdBuffer, indirectBuffer);
        vk.cmdEndConditionalRenderingEXT(*secondaryCmdBuffer);
        vk.endCommandBuffer(*secondaryCmdBuffer);
        if (conditionalData.secondaryCommandBufferNested)
        {
            vk.cmdExecuteCommands(*nestedCmdBuffer, 1, &secondaryCmdBuffer.get());
            vk.endCommandBuffer(*nestedCmdBuffer);
        }
    }
    else if (conditionalData.conditionInherited)
    {
        recordDispatch(vk, *secondaryCmdBuffer, indirectBuffer);
        vk.endCommandBuffer(*secondaryCmdBuffer);
        if (conditionalData.secondaryCommandBufferNested)
        {
            vk.cmdExecuteCommands(*nestedCmdBuffer, 1, &secondaryCmdBuffer.get());
            vk.endCommandBuffer(*nestedCmdBuffer);
        }
    }

    if (conditionalData.conditionInPrimaryCommandBuffer)
    {
        beginConditionalRendering(vk, *cmdBuffer, *conditionalBuffer, conditionalData);

        if (conditionalData.conditionInherited)
        {
            if (conditionalData.secondaryCommandBufferNested)
            {
                vk.cmdExecuteCommands(*cmdBuffer, 1, &nestedCmdBuffer.get());
            }
            else
            {
                vk.cmdExecuteCommands(*cmdBuffer, 1, &secondaryCmdBuffer.get());
            }
        }
        else
        {
            recordDispatch(vk, *cmdBuffer, indirectBuffer);
        }

        vk.cmdEndConditionalRenderingEXT(*cmdBuffer);
    }
    else if (useSecondaryCmdBuffer)
    {
        if (conditionalData.secondaryCommandBufferNested)
        {
            vk.cmdExecuteCommands(*cmdBuffer, 1, &nestedCmdBuffer.get());
        }
        else
        {
            vk.cmdExecuteCommands(*cmdBuffer, 1, &secondaryCmdBuffer.get());
        }
    }

    const vk::VkBufferMemoryBarrier outputBufferMemoryBarrier = {vk::VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
                                                                 DE_NULL,
                                                                 vk::VK_ACCESS_SHADER_WRITE_BIT,
                                                                 vk::VK_ACCESS_HOST_READ_BIT,
                                                                 VK_QUEUE_FAMILY_IGNORED,
                                                                 VK_QUEUE_FAMILY_IGNORED,
                                                                 outputBuffer.get(),
                                                                 0u,
                                                                 VK_WHOLE_SIZE};

    vk.cmdPipelineBarrier(*cmdBuffer, vk::VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, vk::VK_PIPELINE_STAGE_HOST_BIT, 0u, 0u,
                          DE_NULL, 1u, &outputBufferMemoryBarrier, 0u, DE_NULL);

    endCommandBuffer(vk, *cmdBuffer);

    submitCommandsAndWait(vk, device, queue, *cmdBuffer);

    // Check result

    qpTestResult res = QP_TEST_RESULT_PASS;

    const vk::Allocation &outputBufferAllocation = outputBuffer.getAllocation();
    invalidateAlloc(vk, device, outputBufferAllocation);

    const uint32_t *bufferPtr = static_cast<uint32_t *>(outputBufferAllocation.getHostPtr());

    const uint32_t expectedResult = conditionalData.expectCommandExecution ? m_testSpec.numCalls : 0u;
    if (bufferPtr[0] != expectedResult)
    {
        res = QP_TEST_RESULT_FAIL;
    }

    return tcu::TestStatus(res, qpGetTestResultName(res));
}

} // namespace

ConditionalDispatchTests::ConditionalDispatchTests(tcu::TestContext &testCtx) : TestCaseGroup(testCtx, "dispatch")
{
    /* Left blank on purpose */
}

ConditionalDispatchTests::~ConditionalDispatchTests(void)
{
}

void ConditionalDispatchTests::init(void)
{
    for (int conditionNdx = 0; conditionNdx < DE_LENGTH_OF_ARRAY(conditional::s_testsData); conditionNdx++)
    {
        const ConditionalData &conditionData = conditional::s_testsData[conditionNdx];

        if (conditionData.clearInRenderPass)
            continue;

        de::MovePtr<tcu::TestCaseGroup> conditionalDrawRootGroup(
            new tcu::TestCaseGroup(m_testCtx, de::toString(conditionData).c_str()));

        for (uint32_t commandTypeIdx = 0; commandTypeIdx < DISPATCH_COMMAND_TYPE_DISPATCH_LAST; ++commandTypeIdx)
        {
            const DispatchCommandType command = DispatchCommandType(commandTypeIdx);

            ConditionalTestSpec testSpec;
            testSpec.command         = command;
            testSpec.numCalls        = 3;
            testSpec.conditionalData = conditionData;

            conditionalDrawRootGroup->addChild(
                new ConditionalDispatchTest(m_testCtx, getDispatchCommandTypeName(command), testSpec));
        }

        addChild(conditionalDrawRootGroup.release());
    }

    enum class ConditionLocation
    {
        PRIMARY_FLAT = 0,
        PRIMARY_WITH_SECONDARY,
        SECONDARY_NORMAL,
        SECONDARY_INHERITED,
    };

    // Tests verifying the condition is interpreted as a 32-bit value.
    {
        de::MovePtr<tcu::TestCaseGroup> conditionSizeGroup(new tcu::TestCaseGroup(m_testCtx, "condition_size"));

        struct ValuePaddingExecution
        {
            uint32_t value;
            bool padding;
            bool execution;
            const char *name;
        };

        const ValuePaddingExecution kConditionValueResults[] = {
            {0x00000001u, false, true, "first_byte"}, {0x00000100u, false, true, "second_byte"},
            {0x00010000u, false, true, "third_byte"}, {0x01000000u, false, true, "fourth_byte"},
            {0u, true, false, "padded_zero"},
        };

        struct ConditionLocationSubcase
        {
            ConditionLocation location;
            const char *name;
        };

        const ConditionLocationSubcase kConditionLocationSubcase[] = {
            {ConditionLocation::PRIMARY_FLAT, "primary"},
            {ConditionLocation::PRIMARY_WITH_SECONDARY, "inherited"},
            {ConditionLocation::SECONDARY_NORMAL, "secondary"},
            {ConditionLocation::SECONDARY_INHERITED, "secondary_inherited"},
        };

        for (int subcaseNdx = 0; subcaseNdx < DE_LENGTH_OF_ARRAY(kConditionLocationSubcase); ++subcaseNdx)
        {
            const auto &subcase = kConditionLocationSubcase[subcaseNdx];

            de::MovePtr<tcu::TestCaseGroup> subcaseGroup(new tcu::TestCaseGroup(m_testCtx, subcase.name));

            ConditionalData conditionalData   = {};
            conditionalData.conditionInverted = false;

            switch (subcase.location)
            {
            case ConditionLocation::PRIMARY_FLAT:
                conditionalData.conditionInPrimaryCommandBuffer   = true;
                conditionalData.conditionInSecondaryCommandBuffer = false;
                conditionalData.conditionInherited                = false;
                break;

            case ConditionLocation::PRIMARY_WITH_SECONDARY:
                conditionalData.conditionInPrimaryCommandBuffer   = true;
                conditionalData.conditionInSecondaryCommandBuffer = false;
                conditionalData.conditionInherited                = true;
                break;

            case ConditionLocation::SECONDARY_NORMAL:
                conditionalData.conditionInPrimaryCommandBuffer   = false;
                conditionalData.conditionInSecondaryCommandBuffer = true;
                conditionalData.conditionInherited                = false;
                break;

            case ConditionLocation::SECONDARY_INHERITED:
                conditionalData.conditionInPrimaryCommandBuffer   = false;
                conditionalData.conditionInSecondaryCommandBuffer = true;
                conditionalData.conditionInherited                = true;
                break;

            default:
                DE_ASSERT(false);
                break;
            }

            for (int valueNdx = 0; valueNdx < DE_LENGTH_OF_ARRAY(kConditionValueResults); ++valueNdx)
            {
                const auto &valueResults = kConditionValueResults[valueNdx];

                conditionalData.conditionValue         = valueResults.value;
                conditionalData.padConditionValue      = valueResults.padding;
                conditionalData.expectCommandExecution = valueResults.execution;

                ConditionalTestSpec spec;
                spec.command         = DISPATCH_COMMAND_TYPE_DISPATCH;
                spec.numCalls        = 1;
                spec.conditionalData = conditionalData;

                subcaseGroup->addChild(new ConditionalDispatchTest(m_testCtx, valueResults.name, spec));
            }

            conditionSizeGroup->addChild(subcaseGroup.release());
        }

        addChild(conditionSizeGroup.release());
    }

    // Tests checking the buffer allocation offset is applied correctly when reading the condition.
    {
        de::MovePtr<tcu::TestCaseGroup> allocOffsetGroup(new tcu::TestCaseGroup(m_testCtx, "alloc_offset"));

        const struct
        {
            ConditionLocation location;
            const char *name;
        } kLocationCases[] = {
            {ConditionLocation::PRIMARY_FLAT, "primary"},
            {ConditionLocation::PRIMARY_WITH_SECONDARY, "inherited"},
            {ConditionLocation::SECONDARY_NORMAL, "secondary"},
            {ConditionLocation::SECONDARY_INHERITED, "secondary_inherited"},
        };

        const struct
        {
            bool active;
            const char *name;
        } kActiveCases[] = {
            {false, "zero"},
            {true, "nonzero"},
        };

        const struct
        {
            ConditionalBufferMemory memoryType;
            const char *name;
        } kMemoryTypeCases[] = {
            {LOCAL, "device_local"},
            {HOST, "host_visible"},
        };

        for (const auto &locationCase : kLocationCases)
        {
            de::MovePtr<tcu::TestCaseGroup> locationSubGroup(new tcu::TestCaseGroup(m_testCtx, locationCase.name));

            for (const auto &activeCase : kActiveCases)
            {
                de::MovePtr<tcu::TestCaseGroup> activeSubGroup(new tcu::TestCaseGroup(m_testCtx, activeCase.name));

                for (const auto &memoryTypeCase : kMemoryTypeCases)
                {
                    ConditionalData conditionalData = {
                        false,                     // bool conditionInPrimaryCommandBuffer;
                        false,                     // bool conditionInSecondaryCommandBuffer;
                        false,                     // bool conditionInverted;
                        false,                     // bool conditionInherited;
                        0u,                        // uint32_t conditionValue;
                        false,                     // bool padConditionValue;
                        true,                      // bool allocationOffset;
                        false,                     // bool clearInRenderPass;
                        false,                     // bool expectCommandExecution;
                        false,                     // bool secondaryCommandBufferNested;
                        memoryTypeCase.memoryType, // ConditionalBufferMemory memoryType;
                    };

                    switch (locationCase.location)
                    {
                    case ConditionLocation::PRIMARY_FLAT:
                        conditionalData.conditionInPrimaryCommandBuffer   = true;
                        conditionalData.conditionInSecondaryCommandBuffer = false;
                        conditionalData.conditionInherited                = false;
                        break;

                    case ConditionLocation::PRIMARY_WITH_SECONDARY:
                        conditionalData.conditionInPrimaryCommandBuffer   = true;
                        conditionalData.conditionInSecondaryCommandBuffer = false;
                        conditionalData.conditionInherited                = true;
                        break;

                    case ConditionLocation::SECONDARY_NORMAL:
                        conditionalData.conditionInPrimaryCommandBuffer   = false;
                        conditionalData.conditionInSecondaryCommandBuffer = true;
                        conditionalData.conditionInherited                = false;
                        break;

                    case ConditionLocation::SECONDARY_INHERITED:
                        conditionalData.conditionInPrimaryCommandBuffer   = false;
                        conditionalData.conditionInSecondaryCommandBuffer = true;
                        conditionalData.conditionInherited                = true;
                        break;

                    default:
                        DE_ASSERT(false);
                        break;
                    }

                    conditionalData.conditionValue         = (activeCase.active ? 1u : 0u);
                    conditionalData.expectCommandExecution = activeCase.active;

                    const ConditionalTestSpec spec(DISPATCH_COMMAND_TYPE_DISPATCH, // DispatchCommandType command;
                                                   1,                              // int numCalls;
                                                   conditionalData,                // ConditionalData conditionalData;
                                                   false                           // bool computeQueue;
                    );

                    activeSubGroup->addChild(new ConditionalDispatchTest(m_testCtx, memoryTypeCase.name, spec));
                }

                locationSubGroup->addChild(activeSubGroup.release());
            }

            allocOffsetGroup->addChild(locationSubGroup.release());
        }

        addChild(allocOffsetGroup.release());
    }

    // Compute queue tests.
    {
        de::MovePtr<tcu::TestCaseGroup> computeQueueGroup(new tcu::TestCaseGroup(m_testCtx, "compute_queue"));

        struct ValueInvertedExecution
        {
            uint32_t value;
            bool inverted;
            bool executionExpected;
            const char *name;
        };

        const ValueInvertedExecution kConditionValueResults[] = {
            {0u, false, false, "condition_zero"},
            {0u, true, true, "condition_one"},
            {1u, false, true, "condition_inv_zero"},
            {1u, true, false, "condition_inv_one"},
        };

        struct ConditionLocationSubcase
        {
            ConditionLocation location;
            const char *name;
        };

        const ConditionLocationSubcase kConditionLocationSubcase[] = {
            {ConditionLocation::PRIMARY_FLAT, "primary"},
            {ConditionLocation::PRIMARY_WITH_SECONDARY, "inherited"},
            {ConditionLocation::SECONDARY_NORMAL, "secondary"},
            {ConditionLocation::SECONDARY_INHERITED, "secondary_inherited"},
        };

        for (int subcaseNdx = 0; subcaseNdx < DE_LENGTH_OF_ARRAY(kConditionLocationSubcase); ++subcaseNdx)
        {
            const auto &subcase = kConditionLocationSubcase[subcaseNdx];

            de::MovePtr<tcu::TestCaseGroup> subcaseGroup(new tcu::TestCaseGroup(m_testCtx, subcase.name));

            ConditionalData conditionalData              = {};
            conditionalData.padConditionValue            = false;
            conditionalData.allocationOffset             = false;
            conditionalData.clearInRenderPass            = false;
            conditionalData.secondaryCommandBufferNested = false;

            switch (subcase.location)
            {
            case ConditionLocation::PRIMARY_FLAT:
                conditionalData.conditionInPrimaryCommandBuffer   = true;
                conditionalData.conditionInSecondaryCommandBuffer = false;
                conditionalData.conditionInherited                = false;
                break;

            case ConditionLocation::PRIMARY_WITH_SECONDARY:
                conditionalData.conditionInPrimaryCommandBuffer   = true;
                conditionalData.conditionInSecondaryCommandBuffer = false;
                conditionalData.conditionInherited                = true;
                break;

            case ConditionLocation::SECONDARY_NORMAL:
                conditionalData.conditionInPrimaryCommandBuffer   = false;
                conditionalData.conditionInSecondaryCommandBuffer = true;
                conditionalData.conditionInherited                = false;
                break;

            case ConditionLocation::SECONDARY_INHERITED:
                conditionalData.conditionInPrimaryCommandBuffer   = false;
                conditionalData.conditionInSecondaryCommandBuffer = true;
                conditionalData.conditionInherited                = true;
                break;

            default:
                DE_ASSERT(false);
                break;
            }

            for (const bool deviceLocal : {false, true})
                for (const bool indirect : {false, true})
                    for (int valueNdx = 0; valueNdx < DE_LENGTH_OF_ARRAY(kConditionValueResults); ++valueNdx)
                    {
                        const auto &valueResults = kConditionValueResults[valueNdx];

                        conditionalData.conditionValue         = valueResults.value;
                        conditionalData.conditionInverted      = valueResults.inverted;
                        conditionalData.expectCommandExecution = valueResults.executionExpected;
                        conditionalData.memoryType =
                            (deviceLocal ? ConditionalBufferMemory::LOCAL : ConditionalBufferMemory::HOST);

                        ConditionalTestSpec spec;
                        spec.command =
                            (indirect ? DISPATCH_COMMAND_TYPE_DISPATCH_INDIRECT : DISPATCH_COMMAND_TYPE_DISPATCH);
                        spec.numCalls        = 1;
                        spec.conditionalData = conditionalData;
                        spec.computeQueue    = true;

                        const auto testName = std::string(valueResults.name) + (indirect ? "_indirect_dispatch" : "") +
                                              (deviceLocal ? "_device_local" : "");
                        subcaseGroup->addChild(new ConditionalDispatchTest(m_testCtx, testName, spec));
                    }

            computeQueueGroup->addChild(subcaseGroup.release());
        }

        addChild(computeQueueGroup.release());
    }
}

} // namespace conditional
} // namespace vkt
