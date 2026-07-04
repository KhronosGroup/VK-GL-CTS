/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2016 The Khronos Group Inc.
 * Copyright (c) 2016 The Android Open Source Project
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
 * \brief Indirect Compute Dispatch tests
 *//*--------------------------------------------------------------------*/

#include "vktComputeIndirectComputeDispatchTests.hpp"
#include "vktComputeTestsUtil.hpp"
#include "vkSafetyCriticalUtil.hpp"

#include <string>
#include <map>
#include <vector>

#include "vkDefs.hpp"
#include "vkRef.hpp"
#include "vkRefUtil.hpp"
#include "vktTestCase.hpp"
#include "vktTestCaseUtil.hpp"
#include "vkPlatform.hpp"
#include "vkPrograms.hpp"
#include "vkMemUtil.hpp"
#include "vkBarrierUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkDeviceUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkBufferWithMemory.hpp"

#include "tcuVector.hpp"
#include "tcuVectorUtil.hpp"
#include "tcuTestLog.hpp"
#include "tcuRGBA.hpp"
#include "tcuStringTemplate.hpp"

#include "deUniquePtr.hpp"
#include "deSharedPtr.hpp"
#include "deStringUtil.hpp"
#include "deArrayUtil.hpp"

#include "gluShaderUtil.hpp"

#include <set>

namespace vkt::compute
{
namespace
{

enum
{
    RESULT_BLOCK_BASE_SIZE         = 4 * (int)sizeof(uint32_t), // uvec3 + uint
    RESULT_BLOCK_NUM_PASSED_OFFSET = 3 * (int)sizeof(uint32_t),
    INDIRECT_COMMAND_OFFSET        = 3 * (int)sizeof(uint32_t),
};

vk::VkDeviceSize getResultBlockAlignedSize(const vk::InstanceInterface &instance_interface,
                                           const vk::VkPhysicalDevice physicalDevice, const vk::VkDeviceSize baseSize)
{
    // TODO getPhysicalDeviceProperties() was added to vkQueryUtil in 41-image-load-store-tests. Use it once it's merged.
    vk::VkPhysicalDeviceProperties deviceProperties;
    instance_interface.getPhysicalDeviceProperties(physicalDevice, &deviceProperties);
    vk::VkDeviceSize alignment = deviceProperties.limits.minStorageBufferOffsetAlignment;

    if (alignment == 0 || (baseSize % alignment == 0))
        return baseSize;
    else
        return (baseSize / alignment + 1) * alignment;
}

struct DispatchCommand
{
    DispatchCommand(const intptr_t offset, const tcu::UVec3 &numWorkGroups)
        : m_offset(offset)
        , m_numWorkGroups(numWorkGroups)
    {
    }

    intptr_t m_offset;
    tcu::UVec3 m_numWorkGroups;
};

typedef std::vector<DispatchCommand> DispatchCommandsVec;

struct TestParams
{
    TestParams(const char *name_, const uintptr_t bufferSize_, const tcu::UVec3 workGroupSize_,
               const DispatchCommandsVec &dispatchCommands_, const bool useDeviceAddressCommands_ = false)
        : name(name_)
        , bufferSize(bufferSize_)
        , workGroupSize(workGroupSize_)
        , dispatchCommands(dispatchCommands_)
        , useDeviceAddressCommands(useDeviceAddressCommands_)
    {
        // If one of the work group counts is zero, all of them must be and vice versa. This is because on some tests
        // the compute shader will be different for the zero count cases, and both types of dispatches cannot coexist.
        bool hasNullDispatches    = false;
        bool hasNonNullDispatches = false;

        DE_ASSERT(!dispatchCommands.empty());

        for (size_t i = 0u; i < dispatchCommands.size(); ++i)
        {
            const auto &wgCount = dispatchCommands.at(i).m_numWorkGroups;
            const auto totalWGs = wgCount.x() * wgCount.y() * wgCount.z();
            if (totalWGs == 0)
                hasNullDispatches = true;
            else
                hasNonNullDispatches = true;
        }

        if (hasNullDispatches)
            DE_ASSERT(!hasNonNullDispatches);

        // For release builds.
        DE_UNREF(hasNullDispatches);
        DE_UNREF(hasNonNullDispatches);
    }

    bool nullDispatch() const
    {
        // Check only the first dispatch. This should be enough with the constructor check.
        const auto &wgCount = dispatchCommands.front().m_numWorkGroups;
        const auto total    = wgCount.x() * wgCount.y() * wgCount.z();
        return (total == 0);
    }

    const char *name;
    const uintptr_t bufferSize;
    const tcu::UVec3 workGroupSize;
    const DispatchCommandsVec dispatchCommands;
    const bool useDeviceAddressCommands;
};

class IndirectDispatchInstanceBufferUpload : public vkt::MultiQueueRunnerTestInstance
{
public:
    IndirectDispatchInstanceBufferUpload(Context &context, const std::string &name, const TestParams &testParams,
                                         const vk::ComputePipelineConstructionType computePipelineConstructionType);

    virtual ~IndirectDispatchInstanceBufferUpload(void) = default;

    virtual tcu::TestStatus queuePass(const vkt::QueueData &queueData) override;

protected:
    virtual void fillIndirectBufferData(const vk::VkCommandBuffer commandBuffer, const vk::DeviceInterface &vkdi,
                                        const vk::BufferWithMemory &indirectBuffer);

    bool verifyResultBuffer(const vk::BufferWithMemory &resultBuffer, const vk::DeviceInterface &vkdi,
                            const vk::VkDeviceSize resultBlockSize) const;

    Context &m_context;
    const std::string m_name;

    vk::VkDevice m_device;

    const TestParams m_params;

    vk::ComputePipelineConstructionType m_computePipelineConstructionType;

private:
    IndirectDispatchInstanceBufferUpload(const vkt::TestInstance &);
    IndirectDispatchInstanceBufferUpload &operator=(const vkt::TestInstance &);
};

IndirectDispatchInstanceBufferUpload::IndirectDispatchInstanceBufferUpload(
    Context &context, const std::string &name, const TestParams &testParams,
    const vk::ComputePipelineConstructionType computePipelineConstructionType)
    : vkt::MultiQueueRunnerTestInstance(context, vkt::COMPUTE_QUEUE)
    , m_context(context)
    , m_name(name)
    , m_device(context.getDevice())
    , m_params(testParams)
    , m_computePipelineConstructionType(computePipelineConstructionType)
{
}

void IndirectDispatchInstanceBufferUpload::fillIndirectBufferData(const vk::VkCommandBuffer commandBuffer,
                                                                  const vk::DeviceInterface &vkdi,
                                                                  const vk::BufferWithMemory &indirectBuffer)
{
    DE_UNREF(commandBuffer);

    const vk::Allocation &alloc = indirectBuffer.getAllocation();
    uint8_t *indirectDataPtr    = reinterpret_cast<uint8_t *>(alloc.getHostPtr());

    for (const auto &cmd : m_params.dispatchCommands)
    {
        DE_ASSERT(cmd.m_offset >= 0);
        DE_ASSERT(cmd.m_offset % sizeof(uint32_t) == 0);
        DE_ASSERT(cmd.m_offset + INDIRECT_COMMAND_OFFSET <= (intptr_t)m_params.bufferSize);

        uint32_t *const dstPtr = (uint32_t *)&indirectDataPtr[cmd.m_offset];

        dstPtr[0] = cmd.m_numWorkGroups[0];
        dstPtr[1] = cmd.m_numWorkGroups[1];
        dstPtr[2] = cmd.m_numWorkGroups[2];
    }

    vk::flushAlloc(vkdi, m_device, alloc);
}

tcu::TestStatus IndirectDispatchInstanceBufferUpload::queuePass(const vkt::QueueData &queueData)
{
    const vk::InstanceInterface &vki = m_context.getInstanceInterface();
    tcu::TestContext &testCtx        = m_context.getTestContext();
    const vk::DeviceInterface &vkdi  = m_context.getDeviceInterface();
    vk::Allocator &allocator         = m_context.getDefaultAllocator();

    testCtx.getLog() << tcu::TestLog::Message << "GL_DISPATCH_INDIRECT_BUFFER size = " << m_params.bufferSize
                     << tcu::TestLog::EndMessage;
    {
        tcu::ScopedLogSection section(testCtx.getLog(), "Commands",
                                      "Indirect Dispatch Commands (" + de::toString(m_params.dispatchCommands.size()) +
                                          " in total)");

        for (uint32_t cmdNdx = 0; cmdNdx < m_params.dispatchCommands.size(); ++cmdNdx)
        {
            testCtx.getLog() << tcu::TestLog::Message << cmdNdx << ": "
                             << "offset = " << m_params.dispatchCommands[cmdNdx].m_offset
                             << ", numWorkGroups = " << m_params.dispatchCommands[cmdNdx].m_numWorkGroups
                             << tcu::TestLog::EndMessage;
        }
    }

    // Create result buffer
    const vk::VkDeviceSize resultBlockSize =
        getResultBlockAlignedSize(vki, m_context.getPhysicalDevice(), RESULT_BLOCK_BASE_SIZE);
    const vk::VkDeviceSize resultBufferSize = resultBlockSize * (uint32_t)m_params.dispatchCommands.size();

    vk::BufferWithMemory resultBuffer(
        vkdi, m_device, allocator, vk::makeBufferCreateInfo(resultBufferSize, vk::VK_BUFFER_USAGE_STORAGE_BUFFER_BIT),
        vk::MemoryRequirement::HostVisible);

    {
        const vk::Allocation &alloc = resultBuffer.getAllocation();
        uint8_t *resultDataPtr      = reinterpret_cast<uint8_t *>(alloc.getHostPtr());

        for (uint32_t cmdNdx = 0; cmdNdx < m_params.dispatchCommands.size(); ++cmdNdx)
        {
            uint8_t *const dstPtr       = &resultDataPtr[resultBlockSize * cmdNdx];
            const auto &dispatchCommand = m_params.dispatchCommands[cmdNdx];

            *(uint32_t *)(dstPtr + 0 * sizeof(uint32_t))           = dispatchCommand.m_numWorkGroups[0];
            *(uint32_t *)(dstPtr + 1 * sizeof(uint32_t))           = dispatchCommand.m_numWorkGroups[1];
            *(uint32_t *)(dstPtr + 2 * sizeof(uint32_t))           = dispatchCommand.m_numWorkGroups[2];
            *(uint32_t *)(dstPtr + RESULT_BLOCK_NUM_PASSED_OFFSET) = 0;
        }

        vk::flushAlloc(vkdi, m_device, alloc);
    }

    // Create descriptorSetLayout
    vk::DescriptorSetLayoutBuilder layoutBuilder;
    layoutBuilder.addSingleBinding(vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, vk::VK_SHADER_STAGE_COMPUTE_BIT);
    vk::Unique<vk::VkDescriptorSetLayout> descriptorSetLayout(layoutBuilder.build(vkdi, m_device));

    // Create compute pipeline
    vk::ComputePipelineWrapper computePipeline(
        vkdi, m_device, m_computePipelineConstructionType,
        m_context.getBinaryCollection().get("indirect_dispatch_" + m_name + "_verify"));
    computePipeline.setDescriptorSetLayout(descriptorSetLayout.get());
    computePipeline.buildPipeline();

    // Create descriptor pool
    const vk::Unique<vk::VkDescriptorPool> descriptorPool(
        vk::DescriptorPoolBuilder()
            .addType(vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, (uint32_t)m_params.dispatchCommands.size())
            .build(vkdi, m_device, vk::VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
                   static_cast<uint32_t>(m_params.dispatchCommands.size())));

    const vk::VkBufferMemoryBarrier ssboPostBarrier = makeBufferMemoryBarrier(
        vk::VK_ACCESS_SHADER_WRITE_BIT, vk::VK_ACCESS_HOST_READ_BIT, *resultBuffer, 0ull, resultBufferSize);

    // Create command buffer
    const vk::Unique<vk::VkCommandPool> cmdPool(makeCommandPool(vkdi, m_device, queueData.familyIndex));
    const vk::Unique<vk::VkCommandBuffer> cmdBuffer(
        allocateCommandBuffer(vkdi, m_device, *cmdPool, vk::VK_COMMAND_BUFFER_LEVEL_PRIMARY));

    // Begin recording commands
    beginCommandBuffer(vkdi, *cmdBuffer);

    // Create indirect buffer
    vk::VkBufferUsageFlags usage = vk::VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | vk::VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    if (m_params.useDeviceAddressCommands)
        usage |= vk::VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    vk::MemoryRequirement memReq = m_params.useDeviceAddressCommands ?
                                       (vk::MemoryRequirement::DeviceAddress | vk::MemoryRequirement::HostVisible) :
                                       vk::MemoryRequirement::HostVisible;
    vk::BufferWithMemory indirectBuffer(vkdi, m_device, allocator, vk::makeBufferCreateInfo(m_params.bufferSize, usage),
                                        memReq);
    fillIndirectBufferData(*cmdBuffer, vkdi, indirectBuffer);

    vk::VkDeviceAddress indirectBufferAddress = 0ull;
    if (m_params.useDeviceAddressCommands)
        indirectBufferAddress = getBufferDeviceAddress(vkdi, m_device, *indirectBuffer);

    // Bind compute pipeline
    computePipeline.bind(*cmdBuffer);

    // Allocate descriptor sets
    typedef de::SharedPtr<vk::Unique<vk::VkDescriptorSet>> SharedVkDescriptorSet;
    std::vector<SharedVkDescriptorSet> descriptorSets(m_params.dispatchCommands.size());

    vk::VkDeviceSize curOffset = 0;

    // Create descriptor sets
    for (uint32_t cmdNdx = 0; cmdNdx < m_params.dispatchCommands.size(); ++cmdNdx)
    {
        descriptorSets[cmdNdx] = SharedVkDescriptorSet(new vk::Unique<vk::VkDescriptorSet>(
            makeDescriptorSet(vkdi, m_device, *descriptorPool, *descriptorSetLayout)));

        const vk::VkDescriptorBufferInfo resultDescriptorInfo =
            makeDescriptorBufferInfo(*resultBuffer, curOffset, resultBlockSize);

        vk::DescriptorSetUpdateBuilder descriptorSetBuilder;
        descriptorSetBuilder.writeSingle(**descriptorSets[cmdNdx],
                                         vk::DescriptorSetUpdateBuilder::Location::binding(0u),
                                         vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &resultDescriptorInfo);
        descriptorSetBuilder.update(vkdi, m_device);

        // Bind descriptor set
        vkdi.cmdBindDescriptorSets(*cmdBuffer, vk::VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline.getPipelineLayout(),
                                   0u, 1u, &(**descriptorSets[cmdNdx]), 0u, nullptr);

        // Dispatch indirect compute command
        const auto offset = static_cast<vk::VkDeviceSize>(m_params.dispatchCommands[cmdNdx].m_offset);
        if (!m_params.useDeviceAddressCommands)
            vkdi.cmdDispatchIndirect(*cmdBuffer, *indirectBuffer, offset);

#ifndef CTS_USES_VULKANSC
        if (m_params.useDeviceAddressCommands)
        {
            vk::VkDispatchIndirect2InfoKHR dispatchIndirect2Info = vk::initVulkanStructure();
            dispatchIndirect2Info.addressRange = {indirectBufferAddress + offset, m_params.bufferSize - offset};
            dispatchIndirect2Info.addressFlags = vk::VK_ADDRESS_COMMAND_UNKNOWN_STORAGE_BUFFER_USAGE_BIT_KHR;

            // use different valid addressFlags in some cases to test them
            if (m_params.workGroupSize.x() > 1)
                dispatchIndirect2Info.addressFlags |=
                    vk::VK_ADDRESS_COMMAND_UNKNOWN_TRANSFORM_FEEDBACK_BUFFER_USAGE_BIT_KHR;
            if (m_params.workGroupSize.y() > 1)
                dispatchIndirect2Info.addressFlags |= vk::VK_ADDRESS_COMMAND_FULLY_BOUND_BIT_KHR;

            vkdi.cmdDispatchIndirect2KHR(*cmdBuffer, &dispatchIndirect2Info);
        }
#else
        DE_UNREF(indirectBufferAddress);
#endif

        curOffset += resultBlockSize;
    }

    // Insert memory barrier
    vkdi.cmdPipelineBarrier(*cmdBuffer, vk::VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, vk::VK_PIPELINE_STAGE_HOST_BIT,
                            (vk::VkDependencyFlags)0, 0, nullptr, 1, &ssboPostBarrier, 0, nullptr);

    // End recording commands
    endCommandBuffer(vkdi, *cmdBuffer);

    // Wait for command buffer execution finish
    submitCommandsAndWait(vkdi, m_device, queueData.handle, *cmdBuffer);

    // Check if result buffer contains valid values
    if (verifyResultBuffer(resultBuffer, vkdi, resultBlockSize))
        return tcu::TestStatus(QP_TEST_RESULT_PASS, "Pass");
    else
        return tcu::TestStatus(QP_TEST_RESULT_FAIL, "Invalid values in result buffer");
}

bool IndirectDispatchInstanceBufferUpload::verifyResultBuffer(const vk::BufferWithMemory &resultBuffer,
                                                              const vk::DeviceInterface &vkdi,
                                                              const vk::VkDeviceSize resultBlockSize) const
{
    bool allOk                  = true;
    const vk::Allocation &alloc = resultBuffer.getAllocation();
    vk::invalidateAlloc(vkdi, m_device, alloc);

    const uint8_t *const resultDataPtr = reinterpret_cast<uint8_t *>(alloc.getHostPtr());

    for (uint32_t cmdNdx = 0; cmdNdx < m_params.dispatchCommands.size(); cmdNdx++)
    {
        const DispatchCommand &cmd            = m_params.dispatchCommands[cmdNdx];
        const uint8_t *const srcPtr           = (const uint8_t *)resultDataPtr + cmdNdx * resultBlockSize;
        const uint32_t numPassed              = *(const uint32_t *)(srcPtr + RESULT_BLOCK_NUM_PASSED_OFFSET);
        const auto &workGroupSize             = m_params.workGroupSize;
        const uint32_t numInvocationsPerGroup = workGroupSize[0] * workGroupSize[1] * workGroupSize[2];
        const uint32_t numGroups     = cmd.m_numWorkGroups[0] * cmd.m_numWorkGroups[1] * cmd.m_numWorkGroups[2];
        const uint32_t expectedCount = numInvocationsPerGroup * numGroups;

        if (numPassed != expectedCount)
        {
            tcu::TestContext &testCtx = m_context.getTestContext();

            testCtx.getLog() << tcu::TestLog::Message << "ERROR: got invalid result for invocation " << cmdNdx
                             << ": got numPassed = " << numPassed << ", expected " << expectedCount
                             << tcu::TestLog::EndMessage;

            allOk = false;
        }
    }

    return allOk;
}

class IndirectDispatchCaseBufferUpload : public vkt::TestCase
{
public:
    IndirectDispatchCaseBufferUpload(tcu::TestContext &testCtx, const TestParams &testParams,
                                     const vk::ComputePipelineConstructionType computePipelineConstructionType);

    virtual ~IndirectDispatchCaseBufferUpload(void) = default;

    virtual void initPrograms(vk::SourceCollections &programCollection) const;
    virtual TestInstance *createInstance(Context &context) const;
    virtual void checkSupport(Context &context) const;

protected:
    const TestParams m_params;
    const glu::GLSLVersion m_glslVersion;
    vk::ComputePipelineConstructionType m_computePipelineConstructionType;

private:
    IndirectDispatchCaseBufferUpload(const vkt::TestCase &);
    IndirectDispatchCaseBufferUpload &operator=(const vkt::TestCase &);
};

IndirectDispatchCaseBufferUpload::IndirectDispatchCaseBufferUpload(
    tcu::TestContext &testCtx, const TestParams &testParams,
    const vk::ComputePipelineConstructionType computePipelineConstructionType)
    : vkt::TestCase(testCtx, testParams.name)
    , m_params(testParams)
    , m_glslVersion(glu::GLSL_VERSION_310_ES)
    , m_computePipelineConstructionType(computePipelineConstructionType)
{
}

void IndirectDispatchCaseBufferUpload::initPrograms(vk::SourceCollections &programCollection) const
{
    const char *const versionDecl = glu::getGLSLVersionDeclaration(m_glslVersion);

    std::ostringstream verifyBuffer;

    verifyBuffer << versionDecl << "\n"
                 << "layout(local_size_x = ${LOCAL_SIZE_X}, local_size_y = ${LOCAL_SIZE_Y}, local_size_z = "
                    "${LOCAL_SIZE_Z}) in;\n"
                 << "layout(set = 0, binding = 0, std430) buffer Result\n"
                 << "{\n"
                 << "    uvec3           expectedGroupCount;\n"
                 << "    coherent uint   numPassed;\n"
                 << "} result;\n"
                 << "void main (void)\n"
                 << "{\n"
                 << (m_params.nullDispatch() ? "" :
                                               "    if (all(equal(result.expectedGroupCount, gl_NumWorkGroups)))\n")
                 << "        atomicAdd(result.numPassed, 1u);\n"
                 << "}\n";

    std::map<std::string, std::string> args;

    args["LOCAL_SIZE_X"] = de::toString(m_params.workGroupSize.x());
    args["LOCAL_SIZE_Y"] = de::toString(m_params.workGroupSize.y());
    args["LOCAL_SIZE_Z"] = de::toString(m_params.workGroupSize.z());

    std::string verifyProgramString = tcu::StringTemplate(verifyBuffer.str()).specialize(args);

    programCollection.glslSources.add("indirect_dispatch_" + m_name + "_verify")
        << glu::ComputeSource(verifyProgramString);
}

TestInstance *IndirectDispatchCaseBufferUpload::createInstance(Context &context) const
{
    return new IndirectDispatchInstanceBufferUpload(context, m_name, m_params, m_computePipelineConstructionType);
}

void IndirectDispatchCaseBufferUpload::checkSupport(Context &context) const
{
    if (m_params.useDeviceAddressCommands)
        context.requireDeviceFunctionality("VK_KHR_device_address_commands");

    checkShaderObjectRequirements(context.getInstanceInterface(), context.getPhysicalDevice(),
                                  m_computePipelineConstructionType);
}

class IndirectDispatchInstanceBufferGenerate : public IndirectDispatchInstanceBufferUpload
{
public:
    IndirectDispatchInstanceBufferGenerate(Context &context, const std::string &name, const TestParams &testParams,
                                           const vk::ComputePipelineConstructionType computePipelineConstructionType)

        : IndirectDispatchInstanceBufferUpload(context, name, testParams, computePipelineConstructionType)
    {
    }

    virtual ~IndirectDispatchInstanceBufferGenerate(void) = default;

protected:
    virtual void fillIndirectBufferData(const vk::VkCommandBuffer commandBuffer, const vk::DeviceInterface &vkdi,
                                        const vk::BufferWithMemory &indirectBuffer);

    vk::Move<vk::VkDescriptorSetLayout> m_descriptorSetLayout;
    vk::Move<vk::VkDescriptorPool> m_descriptorPool;
    vk::Move<vk::VkDescriptorSet> m_descriptorSet;
    vk::Move<vk::VkPipelineLayout> m_pipelineLayout;
    vk::Move<vk::VkPipeline> m_computePipeline;

private:
    IndirectDispatchInstanceBufferGenerate(const vkt::TestInstance &);
    IndirectDispatchInstanceBufferGenerate &operator=(const vkt::TestInstance &);
};

void IndirectDispatchInstanceBufferGenerate::fillIndirectBufferData(const vk::VkCommandBuffer commandBuffer,
                                                                    const vk::DeviceInterface &vkdi,
                                                                    const vk::BufferWithMemory &indirectBuffer)
{
    // Create compute shader that generates data for indirect buffer
    const vk::Unique<vk::VkShaderModule> genIndirectBufferDataShader(createShaderModule(
        vkdi, m_device, m_context.getBinaryCollection().get("indirect_dispatch_" + m_name + "_generate"), 0u));

    // Create descriptorSetLayout
    m_descriptorSetLayout =
        vk::DescriptorSetLayoutBuilder()
            .addSingleBinding(vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, vk::VK_SHADER_STAGE_COMPUTE_BIT)
            .build(vkdi, m_device);

    // Create compute pipeline
    m_pipelineLayout  = makePipelineLayout(vkdi, m_device, *m_descriptorSetLayout);
    m_computePipeline = makeComputePipeline(vkdi, m_device, *m_pipelineLayout, *genIndirectBufferDataShader);

    // Release old descriptor set before replacing the pool
    m_descriptorSet = vk::Move<vk::VkDescriptorSet>();

    // Create descriptor pool
    m_descriptorPool = vk::DescriptorPoolBuilder()
                           .addType(vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
                           .build(vkdi, m_device, vk::VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);

    // Create descriptor set
    m_descriptorSet = makeDescriptorSet(vkdi, m_device, *m_descriptorPool, *m_descriptorSetLayout);

    const vk::VkDescriptorBufferInfo indirectDescriptorInfo =
        makeDescriptorBufferInfo(*indirectBuffer, 0ull, m_params.bufferSize);

    vk::DescriptorSetUpdateBuilder descriptorSetBuilder;
    descriptorSetBuilder.writeSingle(*m_descriptorSet, vk::DescriptorSetUpdateBuilder::Location::binding(0u),
                                     vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &indirectDescriptorInfo);
    descriptorSetBuilder.update(vkdi, m_device);

    const vk::VkBufferMemoryBarrier bufferBarrier =
        makeBufferMemoryBarrier(vk::VK_ACCESS_SHADER_WRITE_BIT, vk::VK_ACCESS_INDIRECT_COMMAND_READ_BIT,
                                *indirectBuffer, 0ull, m_params.bufferSize);

    // Bind compute pipeline
    vkdi.cmdBindPipeline(commandBuffer, vk::VK_PIPELINE_BIND_POINT_COMPUTE, *m_computePipeline);

    // Bind descriptor set
    vkdi.cmdBindDescriptorSets(commandBuffer, vk::VK_PIPELINE_BIND_POINT_COMPUTE, *m_pipelineLayout, 0u, 1u,
                               &m_descriptorSet.get(), 0u, nullptr);

    // Dispatch compute command
    vkdi.cmdDispatch(commandBuffer, 1u, 1u, 1u);

    // Insert memory barrier
    vkdi.cmdPipelineBarrier(commandBuffer, vk::VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                            vk::VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT, (vk::VkDependencyFlags)0, 0, nullptr, 1,
                            &bufferBarrier, 0, nullptr);
}

class IndirectDispatchCaseBufferGenerate : public IndirectDispatchCaseBufferUpload
{
public:
    IndirectDispatchCaseBufferGenerate(tcu::TestContext &testCtx, const TestParams &caseDesc,
                                       const vk::ComputePipelineConstructionType computePipelineConstructionType)
        : IndirectDispatchCaseBufferUpload(testCtx, caseDesc, computePipelineConstructionType)
    {
    }

    virtual ~IndirectDispatchCaseBufferGenerate(void) = default;

    virtual void initPrograms(vk::SourceCollections &programCollection) const;
    virtual TestInstance *createInstance(Context &context) const;

private:
    IndirectDispatchCaseBufferGenerate(const vkt::TestCase &);
    IndirectDispatchCaseBufferGenerate &operator=(const vkt::TestCase &);
};

void IndirectDispatchCaseBufferGenerate::initPrograms(vk::SourceCollections &programCollection) const
{
    IndirectDispatchCaseBufferUpload::initPrograms(programCollection);

    const char *const versionDecl = glu::getGLSLVersionDeclaration(m_glslVersion);

    std::ostringstream computeBuffer;

    // Header
    computeBuffer << versionDecl << "\n"
                  << "layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;\n"
                  << "layout(set = 0, binding = 0, std430) buffer Out\n"
                  << "{\n"
                  << "    highp uint data[];\n"
                  << "};\n"
                  << "void writeCmd (uint offset, uvec3 numWorkGroups)\n"
                  << "{\n"
                  << "    data[offset+0u] = numWorkGroups.x;\n"
                  << "    data[offset+1u] = numWorkGroups.y;\n"
                  << "    data[offset+2u] = numWorkGroups.z;\n"
                  << "}\n"
                  << "void main (void)\n"
                  << "{\n";

    // Dispatch commands
    for (const auto &cmd : m_params.dispatchCommands)
    {
        const uint32_t offs = (uint32_t)(cmd.m_offset / sizeof(uint32_t));
        DE_ASSERT((size_t)offs * sizeof(uint32_t) == (size_t)cmd.m_offset);

        computeBuffer << "\twriteCmd(" << offs << "u, uvec3(" << cmd.m_numWorkGroups.x() << "u, "
                      << cmd.m_numWorkGroups.y() << "u, " << cmd.m_numWorkGroups.z() << "u));\n";
    }

    // Ending
    computeBuffer << "}\n";

    std::string computeString = computeBuffer.str();

    programCollection.glslSources.add("indirect_dispatch_" + m_name + "_generate") << glu::ComputeSource(computeString);
}

TestInstance *IndirectDispatchCaseBufferGenerate::createInstance(Context &context) const
{
    return new IndirectDispatchInstanceBufferGenerate(context, m_name, m_params, m_computePipelineConstructionType);
}

} // namespace

tcu::TestCaseGroup *createIndirectComputeDispatchTests(
    tcu::TestContext &testCtx, vk::ComputePipelineConstructionType computePipelineConstructionType)
{
    static const TestParams s_dispatchCases[] = {
        // Single invocation only from offset 0
        TestParams("single_invocation", INDIRECT_COMMAND_OFFSET, tcu::UVec3(1, 1, 1),
                   {DispatchCommand(0, tcu::UVec3(1, 1, 1))}),
        // Multiple groups dispatched from offset 0
        TestParams("multiple_groups", INDIRECT_COMMAND_OFFSET, tcu::UVec3(1, 1, 1),
                   {DispatchCommand(0, tcu::UVec3(2, 3, 5))}),
        // Multiple groups of size 2x3x1 from offset 0
        TestParams("multiple_groups_multiple_invocations", INDIRECT_COMMAND_OFFSET, tcu::UVec3(2, 3, 1),
                   {DispatchCommand(0, tcu::UVec3(1, 2, 3))}),
        TestParams("small_offset", 16 + INDIRECT_COMMAND_OFFSET, tcu::UVec3(1, 1, 1),
                   {DispatchCommand(16, tcu::UVec3(1, 1, 1))}),
        TestParams("large_offset", (2 << 20), tcu::UVec3(1, 1, 1),
                   {DispatchCommand((1 << 20) + 12, tcu::UVec3(1, 1, 1))}),
        TestParams("large_offset_multiple_invocations", (2 << 20), tcu::UVec3(2, 3, 1),
                   {DispatchCommand((1 << 20) + 12, tcu::UVec3(1, 2, 3))}),
        TestParams("empty_command", INDIRECT_COMMAND_OFFSET, tcu::UVec3(1, 1, 1),
                   {DispatchCommand(0, tcu::UVec3(0, 0, 0))}),
        TestParams("empty_command_x", INDIRECT_COMMAND_OFFSET, tcu::UVec3(1, 1, 1),
                   {DispatchCommand(0, tcu::UVec3(0, 1, 1))}),
        TestParams("empty_command_y", INDIRECT_COMMAND_OFFSET, tcu::UVec3(1, 1, 1),
                   {DispatchCommand(0, tcu::UVec3(1, 0, 1))}),
        TestParams("empty_command_z", INDIRECT_COMMAND_OFFSET, tcu::UVec3(1, 1, 1),
                   {DispatchCommand(0, tcu::UVec3(1, 1, 0))}),
        // Dispatch multiple compute commands from single buffer
        TestParams("multi_dispatch", 1 << 10, tcu::UVec3(3, 1, 2),
                   {DispatchCommand(0, tcu::UVec3(1, 1, 1)),
                    DispatchCommand(INDIRECT_COMMAND_OFFSET, tcu::UVec3(2, 1, 1)),
                    DispatchCommand(104, tcu::UVec3(1, 3, 1)), DispatchCommand(40, tcu::UVec3(1, 1, 7)),
                    DispatchCommand(52, tcu::UVec3(1, 1, 4))}),
        // Dispatch multiple compute commands from single buffer
        TestParams("multi_dispatch_reuse_command", 1 << 10, tcu::UVec3(3, 1, 2),
                   {DispatchCommand(0, tcu::UVec3(1, 1, 1)), DispatchCommand(0, tcu::UVec3(1, 1, 1)),
                    DispatchCommand(0, tcu::UVec3(1, 1, 1)), DispatchCommand(104, tcu::UVec3(1, 3, 1)),
                    DispatchCommand(104, tcu::UVec3(1, 3, 1)), DispatchCommand(52, tcu::UVec3(1, 1, 4)),
                    DispatchCommand(52, tcu::UVec3(1, 1, 4))}),
    };

    de::MovePtr<tcu::TestCaseGroup> indirectComputeDispatchTests(new tcu::TestCaseGroup(testCtx, "indirect_dispatch"));

    tcu::TestCaseGroup *const groupBufferUpload = new tcu::TestCaseGroup(testCtx, "upload_buffer");
    indirectComputeDispatchTests->addChild(groupBufferUpload);

    tcu::TestCaseGroup *const groupBufferGenerate = new tcu::TestCaseGroup(testCtx, "gen_in_compute");
    indirectComputeDispatchTests->addChild(groupBufferGenerate);

    for (uint32_t ndx = 0; ndx < DE_LENGTH_OF_ARRAY(s_dispatchCases); ndx++)
    {
        const TestParams &caseParams = s_dispatchCases[ndx];

        groupBufferUpload->addChild(
            new IndirectDispatchCaseBufferUpload(testCtx, caseParams, computePipelineConstructionType));

        groupBufferGenerate->addChild(
            new IndirectDispatchCaseBufferGenerate(testCtx, caseParams, computePipelineConstructionType));

#ifndef CTS_USES_VULKANSC
        std::string computeName = std::string(caseParams.name) + std::string("_device_address");
        TestParams addressCommandsCaseDesc(computeName.c_str(), caseParams.bufferSize, caseParams.workGroupSize,
                                           caseParams.dispatchCommands, true);

        // limit number of tests repeated for device_address_commands - skip every other case
        // in upload_buffer group but run previously skiped cases in gen_in_compute group;
        if ((ndx % 2) == uint32_t(computePipelineConstructionType % 2))
        {
            groupBufferUpload->addChild(new IndirectDispatchCaseBufferUpload(testCtx, addressCommandsCaseDesc,
                                                                             computePipelineConstructionType));
        }
        else
        {
            groupBufferGenerate->addChild(new IndirectDispatchCaseBufferGenerate(testCtx, addressCommandsCaseDesc,
                                                                                 computePipelineConstructionType));
        }
#endif
    }

    return indirectComputeDispatchTests.release();
}

} // namespace vkt::compute
