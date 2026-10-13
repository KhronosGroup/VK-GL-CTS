/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2023 The Khronos Group Inc.
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
 * \brief Test multiple entry points.
 *//*--------------------------------------------------------------------*/

#include "vktSpvAsmMultipleShadersTests.hpp"
#include "vktTestCaseUtil.hpp"
#include "vkPrograms.hpp"
#include "vkObjUtil.hpp"
#include "vkRefUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkBarrierUtil.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkCmdUtil.hpp"
#include "tcuTestLog.hpp"

namespace vkt
{
namespace SpirVAssembly
{

namespace
{

using namespace vk;

using BufferWithMemorySp = de::SharedPtr<BufferWithMemory>;

enum class TestType
{
    // two entry points where each OpEntryPoint has associated OpExectionModeId
    TWO_ENTRY_POINTS_EXECUTION_MODE_ID = 0,

    // two entry points where each has different interfaces
    TWO_ENTRY_POINTS_DIFFERENT_INTERFACES,

    // Four entry points each using shared memory, combined over maxComputeSharedMemorySize
    FOUR_ENTRY_POINTS_LARGE_SHARED_MEMORY_SPIRV_1_4,
};

struct TestConfig
{
    TestType type;
};

class EntryPointsTest : public TestInstance
{
public:
    EntryPointsTest(Context &context, TestConfig config);
    virtual ~EntryPointsTest(void) = default;

    tcu::TestStatus iterate(void);

private:
    TestConfig m_config;
};

EntryPointsTest::EntryPointsTest(Context &context, TestConfig config) : TestInstance(context), m_config(config)
{
}

tcu::TestStatus EntryPointsTest::iterate(void)
{
    const DeviceInterface &vk       = m_context.getDeviceInterface();
    const VkDevice device           = m_context.getDevice();
    const VkQueue queue             = m_context.getUniversalQueue();
    const uint32_t queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();
    Allocator &memAlloc             = m_context.getDefaultAllocator();

    // Create test buffers
    const uint32_t bufferItems    = 24u;
    const VkDeviceSize bufferSize = static_cast<VkDeviceSize>(bufferItems * sizeof(uint32_t));
    const VkBufferUsageFlags bufferusage =
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    const bool useTwoBuffers            = (m_config.type == TestType::TWO_ENTRY_POINTS_DIFFERENT_INTERFACES);
    VkBufferCreateInfo bufferCreateInfo = makeBufferCreateInfo(bufferSize, bufferusage);
    BufferWithMemorySp bufferA          = BufferWithMemorySp(
        new BufferWithMemory(vk, device, memAlloc, bufferCreateInfo, MemoryRequirement::HostVisible));
    ;
    BufferWithMemorySp bufferB = BufferWithMemorySp(
        new BufferWithMemory(vk, device, memAlloc, bufferCreateInfo, MemoryRequirement::HostVisible));
    ;

    // Write data to test buffers
    int dataASrc[bufferItems];
    int dataBSrc[bufferItems];
    for (int i = 0; i < DE_LENGTH_OF_ARRAY(dataASrc); ++i)
    {
        dataASrc[i] = deAbs32(9 * deAbs32(int(i / 6) - 1) - (i % 6)) + (i == 6);
        dataBSrc[i] = 1 + i * 2;
    }
    auto fillBuffer = [&](BufferWithMemorySp buffer, int *dataSrc)
    {
        Allocation &allocation = buffer->getAllocation();
        int *bufferPtr         = static_cast<int *>(allocation.getHostPtr());
        deMemcpy(bufferPtr, dataSrc, bufferSize);
        flushAlloc(vk, device, allocation);
    };
    fillBuffer(bufferA, dataASrc);
    fillBuffer(bufferB, dataBSrc);

    // Create descriptor set
    const VkDescriptorType descType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    DescriptorSetLayoutBuilder dsLayoutBuilder;
    dsLayoutBuilder.addSingleBinding(descType, VK_SHADER_STAGE_COMPUTE_BIT);
    if (useTwoBuffers)
        dsLayoutBuilder.addSingleBinding(descType, VK_SHADER_STAGE_COMPUTE_BIT);
    const Unique<VkDescriptorSetLayout> descriptorSetLayout(dsLayoutBuilder.build(vk, device));

    const Unique<VkDescriptorPool> descriptorPool(
        DescriptorPoolBuilder()
            .addType(descType, 1 + useTwoBuffers)
            .build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u));

    const Unique<VkDescriptorSet> descriptorSet(makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout));

    const VkDescriptorBufferInfo bufferADescriptorInfo = makeDescriptorBufferInfo(**bufferA, 0ull, bufferSize);
    const VkDescriptorBufferInfo bufferBDescriptorInfo = makeDescriptorBufferInfo(**bufferB, 0ull, bufferSize);
    DescriptorSetUpdateBuilder descriptorSetUpdateBuilder;
    descriptorSetUpdateBuilder.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u), descType,
                                           &bufferADescriptorInfo);
    if (useTwoBuffers)
        descriptorSetUpdateBuilder.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(1u),
                                               descType, &bufferBDescriptorInfo);
    descriptorSetUpdateBuilder.update(vk, device);

    // Perform the computation
    const Unique<VkShaderModule> shaderModule(
        createShaderModule(vk, device, m_context.getBinaryCollection().get("comp"), 0u));
    const Unique<VkPipelineLayout> pipelineLayout(makePipelineLayout(vk, device, *descriptorSetLayout));

    const VkPipelineShaderStageCreateInfo pipelineAShaderStageParams{
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        nullptr,
        static_cast<VkPipelineShaderStageCreateFlags>(0u),
        VK_SHADER_STAGE_COMPUTE_BIT,
        *shaderModule,
        "mainA",
        nullptr,
    };
    const VkPipelineShaderStageCreateInfo pipelineBShaderStageParams{
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        nullptr,
        static_cast<VkPipelineShaderStageCreateFlags>(0u),
        VK_SHADER_STAGE_COMPUTE_BIT,
        *shaderModule,
        "mainB",
        nullptr,
    };
    VkComputePipelineCreateInfo pipelineCreateInfo{
        VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        nullptr,
        static_cast<VkPipelineCreateFlags>(0u),
        pipelineBShaderStageParams,
        *pipelineLayout,
        VK_NULL_HANDLE,
        0,
    };

    Unique<VkPipeline> pipelineB(createComputePipeline(vk, device, VK_NULL_HANDLE, &pipelineCreateInfo));
    pipelineCreateInfo.stage = pipelineAShaderStageParams;
    Unique<VkPipeline> pipelineA(createComputePipeline(vk, device, VK_NULL_HANDLE, &pipelineCreateInfo));

    const VkMemoryBarrier hostWriteBarrier(makeMemoryBarrier(VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT));
    const Unique<VkCommandPool> cmdPool(makeCommandPool(vk, device, queueFamilyIndex));
    const Unique<VkCommandBuffer> cmdBuffer(
        allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

    // Start recording commands
    beginCommandBuffer(vk, *cmdBuffer);

    vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1,
                          &hostWriteBarrier, 0, 0, 0, 0);

    vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineB);
    vk.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayout, 0u, 1u, &*descriptorSet, 0u,
                             0);
    vk.cmdDispatch(*cmdBuffer, 1, 1, 1);

    vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineA);
    vk.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayout, 0u, 1u, &*descriptorSet, 0u,
                             0);
    vk.cmdDispatch(*cmdBuffer, 1, 1, 1);

    endCommandBuffer(vk, *cmdBuffer);

    // Wait for completion
    submitCommandsAndWait(vk, device, queue, *cmdBuffer);

    invalidateAlloc(vk, device, bufferA->getAllocation());

    // Validate the results
    if (m_config.type == TestType::TWO_ENTRY_POINTS_EXECUTION_MODE_ID)
    {
        int *bufferPtr = static_cast<int *>(bufferA->getAllocation().getHostPtr());
        for (int i = 0; i < 6; i++)
        {
            if ((bufferPtr[12 + i] != (dataASrc[i] - dataASrc[6 + i])) ||
                (bufferPtr[18 + i] != (dataASrc[i] * dataASrc[6 + i])))
                return tcu::TestStatus::fail("Fail");
        }
    }
    else if (m_config.type == TestType::TWO_ENTRY_POINTS_DIFFERENT_INTERFACES)
    {
        invalidateAlloc(vk, device, bufferB->getAllocation());
        int *bufferAPtr = static_cast<int *>(bufferA->getAllocation().getHostPtr());
        int *bufferBPtr = static_cast<int *>(bufferB->getAllocation().getHostPtr());
        for (int i = 0; i < 6; i++)
        {
            if ((bufferAPtr[12 + i] != (dataASrc[i] + dataASrc[6 + i])) ||
                (bufferBPtr[12 + i] != (dataBSrc[5 - i] * dataBSrc[11 - i])))
                return tcu::TestStatus::fail("Fail");
        }
    }

    return tcu::TestStatus::pass("Pass");
}

class LargeSharedMemoryTestInstance : public TestInstance
{
public:
    LargeSharedMemoryTestInstance(Context &context, const TestConfig &) : TestInstance(context)
    {
    }
    virtual ~LargeSharedMemoryTestInstance() = default;

    virtual tcu::TestStatus iterate() override;
};

tcu::TestStatus LargeSharedMemoryTestInstance::iterate()
{
    const auto &vk              = m_context.getDeviceInterface();
    const VkDevice device       = m_context.getDevice();
    const auto queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();
    const auto queue            = m_context.getUniversalQueue();
    auto &alloc                 = m_context.getDefaultAllocator();
    tcu::TestLog &log           = m_context.getTestContext().getLog();

    constexpr uint32_t entryPointCount                 = 4u;
    const char *const entryPointNames[entryPointCount] = {"main_a", "main_b", "main_c", "main_d"};

    const uint32_t maxComputeSharedMemorySize = m_context.getDeviceProperties().limits.maxComputeSharedMemorySize;
    const uint32_t uint_size                  = static_cast<uint32_t>(sizeof(uint32_t));
    uint32_t arraySizes[entryPointCount];
    for (uint32_t i = 0u; i < entryPointCount; ++i)
        arraySizes[i] = maxComputeSharedMemorySize / uint_size - entryPointCount + i;

    const VkDeviceSize outputBufferSize = static_cast<VkDeviceSize>(2u * entryPointCount * sizeof(uint32_t));
    BufferWithMemory outputBuffer(vk, device, alloc,
                                  makeBufferCreateInfo(outputBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT),
                                  MemoryRequirement::HostVisible);
    const auto bufferData = static_cast<uint32_t *>(outputBuffer.getAllocation().getHostPtr());
    for (uint32_t i = 0u; i < entryPointCount; ++i)
    {
        bufferData[i]                   = 0u;
        bufferData[entryPointCount + i] = arraySizes[i] - 1u;
    }
    flushAlloc(vk, device, outputBuffer.getAllocation());

    const auto descriptorSetLayout =
        DescriptorSetLayoutBuilder()
            .addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
            .build(vk, device);
    const auto pipelineLayout = makePipelineLayout(vk, device, *descriptorSetLayout);
    const auto descriptorPool = DescriptorPoolBuilder()
                                    .addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
                                    .build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);
    const auto descriptorSet = makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout);

    const VkDescriptorBufferInfo bufferInfo = makeDescriptorBufferInfo(*outputBuffer, 0u, outputBufferSize);
    DescriptorSetUpdateBuilder()
        .writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u),
                     VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &bufferInfo)
        .update(vk, device);

    VkSpecializationMapEntry specializationMapEntries[entryPointCount];
    for (uint32_t i = 0u; i < entryPointCount; ++i)
        specializationMapEntries[i] = makeSpecializationMapEntry(i, i * uint_size, sizeof(uint32_t));

    VkSpecializationInfo specializationInfo;
    specializationInfo.mapEntryCount = entryPointCount;
    specializationInfo.pMapEntries   = specializationMapEntries;
    specializationInfo.dataSize      = entryPointCount * sizeof(uint32_t);
    specializationInfo.pData         = arraySizes;

    auto shaderModule = createShaderModule(vk, device, m_context.getBinaryCollection().get("comp"));

    Move<VkPipeline> pipelines[entryPointCount];
    for (uint32_t i = 0u; i < entryPointCount; ++i)
    {
        VkComputePipelineCreateInfo pipelineCreateInfo = initVulkanStructure();
        pipelineCreateInfo.stage                       = initVulkanStructure();
        pipelineCreateInfo.stage.stage                 = VK_SHADER_STAGE_COMPUTE_BIT;
        pipelineCreateInfo.stage.module                = *shaderModule;
        pipelineCreateInfo.stage.pName                 = entryPointNames[i];
        pipelineCreateInfo.stage.pSpecializationInfo   = &specializationInfo;
        pipelineCreateInfo.layout                      = *pipelineLayout;
        pipelines[i] = createComputePipeline(vk, device, VK_NULL_HANDLE, &pipelineCreateInfo);
    }

    const auto cmdPool   = createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, queueFamilyIndex);
    const auto cmdBuffer = allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

    beginCommandBuffer(vk, *cmdBuffer);
    vk.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayout, 0u, 1u, &*descriptorSet, 0u,
                             nullptr);
    for (uint32_t i = 0u; i < entryPointCount; ++i)
    {
        vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelines[i]);
        vk.cmdDispatch(*cmdBuffer, 1u, 1u, 1u);
    }
    auto bufferBarrier = makeMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
    vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0u, 1u,
                          &bufferBarrier, 0u, nullptr, 0u, nullptr);
    endCommandBuffer(vk, *cmdBuffer);
    submitCommandsAndWait(vk, device, queue, *cmdBuffer);

    invalidateAlloc(vk, device, outputBuffer.getAllocation());

    for (uint32_t i = 0u; i < entryPointCount; ++i)
    {
        if (bufferData[i] != arraySizes[i])
        {
            log << tcu::TestLog::Message << "Entry point " << entryPointNames[i] << " expected " << arraySizes[i]
                << " but was " << bufferData[i] << tcu::TestLog::EndMessage;
            return tcu::TestStatus::fail("Fail");
        }
    }

    return tcu::TestStatus::pass("Pass");
}

struct Programs
{
    void init(vk::SourceCollections &dst, TestConfig config) const
    {
        const SpirVAsmBuildOptions buildOptionsSpr(dst.usedVulkanVersion, SPIRV_VERSION_1_5, false, true);
        std::string compSrc = "OpCapability Shader\n"
                              "%1 = OpExtInstImport \"GLSL.std.450\"\n"
                              "OpMemoryModel Logical GLSL450\n";

        if (config.type == TestType::TWO_ENTRY_POINTS_EXECUTION_MODE_ID)
        {
            // #version 450
            // layout(local_size_x = 2, local_size_y = 3) in;
            // layout(binding = 0, std430) buffer InOut { int v[]; } inOut;
            // void mainA()
            // {
            //   uint id = gl_LocalInvocationIndex;
            //   inOut.v[12+id] = inOut.v[id] - inOut.v[6+id];
            // }
            // void mainB()
            // {
            //   uint id = gl_LocalInvocationIndex;
            //   inOut.v[18+id] = inOut.v[id] * inOut.v[6+id];
            // }

            compSrc += "OpEntryPoint GLCompute %mainA \"mainA\" %inOutVar %gl_LocalInvocationIndex\n"
                       "OpEntryPoint GLCompute %mainB \"mainB\" %inOutVar %gl_LocalInvocationIndex\n"
                       "OpExecutionModeId %mainA LocalSizeId %uint_2 %uint_3 %uint_1\n"
                       "OpExecutionModeId %mainB LocalSizeId %uint_2 %uint_3 %uint_1\n"

                       "OpDecorate %runtimearr_int ArrayStride 4\n"
                       "OpMemberDecorate %InOut 0 Offset 0\n"
                       "OpDecorate %InOut Block\n"
                       "OpDecorate %inOutVar DescriptorSet 0\n"
                       "OpDecorate %inOutVar Binding 0\n"
                       "OpDecorate %gl_LocalInvocationIndex BuiltIn LocalInvocationIndex\n"
                       "OpDecorate %gl_WorkGroupSize BuiltIn WorkgroupSize\n"

                       "%void = OpTypeVoid\n"
                       "%int = OpTypeInt 32 1\n"
                       "%uint = OpTypeInt 32 0\n"
                       "%v3uint = OpTypeVector %uint 3\n"
                       "%void_fun = OpTypeFunction %void\n"
                       "%uint_fun = OpTypeFunction %uint\n"
                       "%runtimearr_int = OpTypeRuntimeArray %int\n"
                       "%InOut = OpTypeStruct %runtimearr_int\n"
                       "%ptr_Uniform_InOut = OpTypePointer StorageBuffer %InOut\n"
                       "%ptr_Uniform_int = OpTypePointer StorageBuffer %int\n"
                       "%ptr_uint_fun = OpTypePointer Function %uint\n"
                       "%ptr_v3uint_input = OpTypePointer Input %v3uint\n"
                       "%ptr_uint_input = OpTypePointer Input %uint\n"

                       "%int_0 = OpConstant %int 0\n"
                       "%uint_1 = OpConstant %uint 1\n"
                       "%uint_2 = OpConstant %uint 2\n"
                       "%uint_3 = OpConstant %uint 3\n"
                       "%uint_6 = OpConstant %uint 6\n"
                       "%uint_12 = OpConstant %uint 12\n"
                       "%uint_18 = OpConstant %uint 18\n"

                       "%gl_WorkGroupSize = OpConstantComposite %v3uint %uint_2 %uint_3 %uint_1\n"
                       "%gl_LocalInvocationIndex = OpVariable %ptr_uint_input Input\n"
                       "%inOutVar = OpVariable %ptr_Uniform_InOut StorageBuffer\n"

                       "%mainA = OpFunction %void None %void_fun\n"
                       "%labelA = OpLabel\n"
                       "%idxA = OpLoad %uint %gl_LocalInvocationIndex\n"
                       "%30 = OpIAdd %uint %uint_12 %idxA\n"
                       "%33 = OpAccessChain %ptr_Uniform_int %inOutVar %int_0 %idxA\n"
                       "%34 = OpLoad %int %33\n"
                       "%37 = OpIAdd %uint %uint_6 %idxA\n"
                       "%38 = OpAccessChain %ptr_Uniform_int %inOutVar %int_0 %37\n"
                       "%39 = OpLoad %int %38\n"
                       "%40 = OpISub %int %34 %39\n"
                       "%41 = OpAccessChain %ptr_Uniform_int %inOutVar %int_0 %30\n"
                       "OpStore %41 %40\n"
                       "OpReturn\n"
                       "OpFunctionEnd\n"

                       "%mainB = OpFunction %void None %void_fun\n"
                       "%labelB = OpLabel\n"
                       "%idxB = OpLoad %uint %gl_LocalInvocationIndex\n"
                       "%60 = OpIAdd %uint %uint_18 %idxB\n"
                       "%63 = OpAccessChain %ptr_Uniform_int %inOutVar %int_0 %idxB\n"
                       "%64 = OpLoad %int %63\n"
                       "%67 = OpIAdd %uint %uint_6 %idxB\n"
                       "%68 = OpAccessChain %ptr_Uniform_int %inOutVar %int_0 %67\n"
                       "%69 = OpLoad %int %68\n"
                       "%70 = OpIMul %int %64 %69\n"
                       "%71 = OpAccessChain %ptr_Uniform_int %inOutVar %int_0 %60\n"
                       "OpStore %71 %70\n"
                       "OpReturn\n"
                       "OpFunctionEnd\n";

            dst.spirvAsmSources.add("comp") << compSrc << buildOptionsSpr;
        }
        else if (config.type == TestType::TWO_ENTRY_POINTS_DIFFERENT_INTERFACES)
        {
            // #version 450
            // layout(local_size_x = 3, local_size_y = 2) in;
            // layout(binding = 0, std430) buffer BufferA { int v[]; } bufferA;
            // layout(binding = 1, std430) buffer BufferB { int v[]; } bufferB;
            // void mainA()
            // {
            //   uint idx = gl_LocalInvocationIndex;
            //   bufferA.v[12+idx] = bufferA.v[idx] - bufferA.v[6+idx];
            // }
            // void mainB()
            // {
            //   uint idxOut = 2 * gl_LocalInvocationID.x + gl_LocalInvocationID.y;
            //   uint idxIn  = 6 - gl_NumWorkGroups.x - idxOut;
            //   bufferB.v[12+idxOut] = bufferB.v[idxIn] * bufferB.v[6+idxIn];
            // }

            compSrc += "OpEntryPoint GLCompute %mainA \"mainA\" %gl_LocalInvocationIndex\n"
                       "OpEntryPoint GLCompute %mainB \"mainB\" %gl_NumWorkGroups %gl_LocalInvocationId\n"
                       "OpExecutionMode %mainA LocalSize 3 2 1\n"
                       "OpExecutionMode %mainB LocalSize 3 2 1\n"
                       "OpDecorate %gl_NumWorkGroups BuiltIn NumWorkgroups\n"
                       "OpDecorate %gl_LocalInvocationIndex BuiltIn LocalInvocationIndex\n"
                       "OpDecorate %gl_LocalInvocationId BuiltIn LocalInvocationId\n"
                       "OpDecorate %int_runtime_array ArrayStride 4\n"
                       "OpMemberDecorate %struct_type 0 Offset 0\n"
                       "OpDecorate %struct_type BufferBlock\n"
                       "OpDecorate %var_BufferA DescriptorSet 0\n"
                       "OpDecorate %var_BufferA Binding 0\n"
                       "OpDecorate %gl_WorkGroupSize BuiltIn WorkgroupSize\n"
                       "OpDecorate %var_BufferB DescriptorSet 0\n"
                       "OpDecorate %var_BufferB Binding 1\n"

                       "%void = OpTypeVoid\n"
                       "%void_fun = OpTypeFunction %void\n"
                       "%uint = OpTypeInt 32 0\n"
                       "%int = OpTypeInt 32 1\n"
                       "%ptr_uint_fun = OpTypePointer Function %uint\n"
                       "%v3uint = OpTypeVector %uint 3\n"
                       "%ptr_uint_input = OpTypePointer Input %uint\n"
                       "%ptr_v3uint_input = OpTypePointer Input %v3uint\n"
                       "%int_runtime_array = OpTypeRuntimeArray %int\n"
                       "%struct_type = OpTypeStruct %int_runtime_array\n"
                       "%25 = OpTypePointer Uniform %struct_type\n"
                       "%ptr_uniform_int = OpTypePointer Uniform %int\n"

                       "%int_0 = OpConstant %int 0\n"
                       "%uint_0 = OpConstant %uint 0\n"
                       "%uint_1 = OpConstant %uint 1\n"
                       "%uint_2 = OpConstant %uint 2\n"
                       "%uint_3 = OpConstant %uint 3\n"
                       "%uint_6 = OpConstant %uint 6\n"
                       "%uint_12 = OpConstant %uint 12\n"
                       "%gl_WorkGroupSize = OpConstantComposite %v3uint %uint_3 %uint_2 %uint_1\n"

                       "%gl_LocalInvocationIndex = OpVariable %ptr_uint_input Input\n"
                       "%gl_NumWorkGroups = OpVariable %ptr_v3uint_input Input\n"
                       "%gl_LocalInvocationId = OpVariable %ptr_v3uint_input Input\n"
                       "%var_BufferA = OpVariable %25 Uniform\n"
                       "%var_BufferB = OpVariable %25 Uniform\n"

                       "%mainA = OpFunction %void None %void_fun\n"
                       "%labelA = OpLabel\n"
                       "%idxA = OpLoad %uint %gl_LocalInvocationIndex\n"
                       "%inA1_location = OpAccessChain %ptr_uniform_int %var_BufferA %int_0 %idxA\n"
                       "%inA1 = OpLoad %int %inA1_location\n"
                       "%inA2_index = OpIAdd %uint %uint_6 %idxA\n"
                       "%inA2_location = OpAccessChain %ptr_uniform_int %var_BufferA %int_0 %inA2_index\n"
                       "%inA2 = OpLoad %int %inA2_location\n"
                       "%outA_index = OpIAdd %uint %uint_12 %idxA\n"
                       "%add_result = OpIAdd %int %inA1 %inA2\n"
                       "%outA_location = OpAccessChain %ptr_uniform_int %var_BufferA %int_0 %outA_index\n"
                       "OpStore %outA_location %add_result\n"
                       "OpReturn\n"
                       "OpFunctionEnd\n"

                       "%mainB = OpFunction %void None %void_fun\n"
                       "%labelB = OpLabel\n"

                       "%local_x_location = OpAccessChain %ptr_uint_input %gl_LocalInvocationId %uint_0\n"
                       "%local_x = OpLoad %uint %local_x_location\n"
                       "%local_x_times_2 = OpIMul %uint %local_x %uint_2\n"
                       "%local_y_location = OpAccessChain %ptr_uint_input %gl_LocalInvocationId %uint_1\n"
                       "%local_y = OpLoad %uint %local_y_location\n"
                       "%idxOut = OpIAdd %int %local_x_times_2 %local_y\n"

                       "%group_count_location = OpAccessChain %ptr_uint_input %gl_NumWorkGroups %uint_0\n"
                       "%group_count = OpLoad %uint %group_count_location\n"
                       "%sub_result = OpISub %int %uint_6 %group_count\n"
                       "%idxIn = OpISub %int %sub_result %idxOut\n"

                       "%inB1_location = OpAccessChain %ptr_uniform_int %var_BufferB %int_0 %idxIn\n"
                       "%inB1 = OpLoad %int %inB1_location\n"
                       "%inB2_index = OpIAdd %uint %uint_6 %idxIn\n"
                       "%inB2_location = OpAccessChain %ptr_uniform_int %var_BufferB %int_0 %inB2_index\n"
                       "%inB2 = OpLoad %int %inB2_location\n"
                       "%outB_index = OpIAdd %uint %uint_12 %idxOut\n"
                       "%mul_result = OpIMul %int %inB1 %inB2\n"
                       "%outB_location = OpAccessChain %ptr_uniform_int %var_BufferB %int_0 %outB_index\n"
                       "OpStore %outB_location %mul_result\n"

                       "OpReturn\n"
                       "OpFunctionEnd\n";

            dst.spirvAsmSources.add("comp") << compSrc;
        }
        else if (config.type == TestType::FOUR_ENTRY_POINTS_LARGE_SHARED_MEMORY_SPIRV_1_4)
        {
            // #version 450
            // layout(local_size_x = 1) in;
            //
            // layout(set = 0, binding = 0) buffer SSBO {
            //     uint result[4];
            //     uint lastIndex[4];
            // };
            //
            // layout(constant_id = IDX) const uint SIZE = 1u;
            // shared uint arr[SIZE];
            //
            // void main()
            // {
            //     uint last = lastIndex[IDX];
            //     arr[0] = 1u;
            //     arr[last] = last;
            //     result[IDX] = arr[0] + arr[last];
            // }
            //
            // Compiled once per entry point and linked into a single module
            const std::string comp = R"(
               OpCapability Shader
          %1 = OpExtInstImport "GLSL.std.450"
               OpMemoryModel Logical GLSL450
               OpEntryPoint GLCompute %main_a "main_a" %_ %arr
               OpEntryPoint GLCompute %main_b "main_b" %__0 %arr_0
               OpEntryPoint GLCompute %main_c "main_c" %__1 %arr_1
               OpEntryPoint GLCompute %main_d "main_d" %__2 %arr_2
               OpExecutionMode %main_a LocalSize 1 1 1
               OpExecutionMode %main_b LocalSize 1 1 1
               OpExecutionMode %main_c LocalSize 1 1 1
               OpExecutionMode %main_d LocalSize 1 1 1
               OpSource GLSL 450
               OpSource GLSL 450
               OpSource GLSL 450
               OpSource GLSL 450
               OpName %main_a "main_a"
               OpName %last "last"
               OpName %SSBO "SSBO"
               OpMemberName %SSBO 0 "result"
               OpMemberName %SSBO 1 "lastIndex"
               OpName %_ ""
               OpName %SIZE "SIZE"
               OpName %arr "arr"
               OpName %main_b "main_b"
               OpName %last_0 "last"
               OpName %__0 ""
               OpName %SIZE_0 "SIZE"
               OpName %arr_0 "arr"
               OpName %main_c "main_c"
               OpName %last_1 "last"
               OpName %__1 ""
               OpName %SIZE_1 "SIZE"
               OpName %arr_1 "arr"
               OpName %main_d "main_d"
               OpName %last_2 "last"
               OpName %__2 ""
               OpName %SIZE_2 "SIZE"
               OpName %arr_2 "arr"
               OpModuleProcessed "Linked by SPIR-V Tools Linker"
               OpDecorate %_arr_uint_uint_4 ArrayStride 4
               OpDecorate %SSBO Block
               OpMemberDecorate %SSBO 0 Offset 0
               OpMemberDecorate %SSBO 1 Offset 16
               OpDecorate %_ Binding 0
               OpDecorate %_ DescriptorSet 0
               OpDecorate %SIZE SpecId 0
               OpDecorate %gl_WorkGroupSize BuiltIn WorkgroupSize
               OpDecorate %__0 Binding 0
               OpDecorate %__0 DescriptorSet 0
               OpDecorate %SIZE_0 SpecId 1
               OpDecorate %gl_WorkGroupSize_0 BuiltIn WorkgroupSize
               OpDecorate %__1 Binding 0
               OpDecorate %__1 DescriptorSet 0
               OpDecorate %SIZE_1 SpecId 2
               OpDecorate %gl_WorkGroupSize_1 BuiltIn WorkgroupSize
               OpDecorate %__2 Binding 0
               OpDecorate %__2 DescriptorSet 0
               OpDecorate %SIZE_2 SpecId 3
               OpDecorate %gl_WorkGroupSize_2 BuiltIn WorkgroupSize
       %void = OpTypeVoid
         %29 = OpTypeFunction %void
       %uint = OpTypeInt 32 0
%_ptr_Function_uint = OpTypePointer Function %uint
     %uint_4 = OpConstant %uint 4
%_arr_uint_uint_4 = OpTypeArray %uint %uint_4
       %SSBO = OpTypeStruct %_arr_uint_uint_4 %_arr_uint_uint_4
%_ptr_StorageBuffer_SSBO = OpTypePointer StorageBuffer %SSBO
          %_ = OpVariable %_ptr_StorageBuffer_SSBO StorageBuffer
        %int = OpTypeInt 32 1
      %int_1 = OpConstant %int 1
      %int_0 = OpConstant %int 0
%_ptr_StorageBuffer_uint = OpTypePointer StorageBuffer %uint
       %SIZE = OpSpecConstant %uint 1
%_arr_uint_SIZE = OpTypeArray %uint %SIZE
%_ptr_Workgroup__arr_uint_SIZE = OpTypePointer Workgroup %_arr_uint_SIZE
        %arr = OpVariable %_ptr_Workgroup__arr_uint_SIZE Workgroup
     %uint_1 = OpConstant %uint 1
%_ptr_Workgroup_uint = OpTypePointer Workgroup %uint
     %v3uint = OpTypeVector %uint 3
%gl_WorkGroupSize = OpConstantComposite %v3uint %uint_1 %uint_1 %uint_1
   %uint_4_0 = OpConstant %uint 4
        %__0 = OpVariable %_ptr_StorageBuffer_SSBO StorageBuffer
    %int_1_0 = OpConstant %int 1
     %SIZE_0 = OpSpecConstant %uint 1
%_arr_uint_SIZE_0 = OpTypeArray %uint %SIZE_0
%_ptr_Workgroup__arr_uint_SIZE_0 = OpTypePointer Workgroup %_arr_uint_SIZE_0
      %arr_0 = OpVariable %_ptr_Workgroup__arr_uint_SIZE_0 Workgroup
    %int_0_0 = OpConstant %int 0
   %uint_1_0 = OpConstant %uint 1
%gl_WorkGroupSize_0 = OpConstantComposite %v3uint %uint_1_0 %uint_1_0 %uint_1_0
   %uint_4_1 = OpConstant %uint 4
        %__1 = OpVariable %_ptr_StorageBuffer_SSBO StorageBuffer
    %int_1_1 = OpConstant %int 1
      %int_2 = OpConstant %int 2
     %SIZE_1 = OpSpecConstant %uint 1
%_arr_uint_SIZE_1 = OpTypeArray %uint %SIZE_1
%_ptr_Workgroup__arr_uint_SIZE_1 = OpTypePointer Workgroup %_arr_uint_SIZE_1
      %arr_1 = OpVariable %_ptr_Workgroup__arr_uint_SIZE_1 Workgroup
    %int_0_1 = OpConstant %int 0
   %uint_1_1 = OpConstant %uint 1
%gl_WorkGroupSize_1 = OpConstantComposite %v3uint %uint_1_1 %uint_1_1 %uint_1_1
   %uint_4_2 = OpConstant %uint 4
        %__2 = OpVariable %_ptr_StorageBuffer_SSBO StorageBuffer
    %int_1_2 = OpConstant %int 1
      %int_3 = OpConstant %int 3
     %SIZE_2 = OpSpecConstant %uint 1
%_arr_uint_SIZE_2 = OpTypeArray %uint %SIZE_2
%_ptr_Workgroup__arr_uint_SIZE_2 = OpTypePointer Workgroup %_arr_uint_SIZE_2
      %arr_2 = OpVariable %_ptr_Workgroup__arr_uint_SIZE_2 Workgroup
    %int_0_2 = OpConstant %int 0
   %uint_1_2 = OpConstant %uint 1
%gl_WorkGroupSize_2 = OpConstantComposite %v3uint %uint_1_2 %uint_1_2 %uint_1_2
     %main_a = OpFunction %void None %29
         %63 = OpLabel
       %last = OpVariable %_ptr_Function_uint Function
         %64 = OpAccessChain %_ptr_StorageBuffer_uint %_ %int_1 %int_0
         %65 = OpLoad %uint %64
               OpStore %last %65
         %66 = OpAccessChain %_ptr_Workgroup_uint %arr %int_0
               OpStore %66 %uint_1
         %67 = OpLoad %uint %last
         %68 = OpLoad %uint %last
         %69 = OpAccessChain %_ptr_Workgroup_uint %arr %67
               OpStore %69 %68
         %70 = OpAccessChain %_ptr_Workgroup_uint %arr %int_0
         %71 = OpLoad %uint %70
         %72 = OpLoad %uint %last
         %73 = OpAccessChain %_ptr_Workgroup_uint %arr %72
         %74 = OpLoad %uint %73
         %75 = OpIAdd %uint %71 %74
         %76 = OpAccessChain %_ptr_StorageBuffer_uint %_ %int_0 %int_0
               OpStore %76 %75
               OpReturn
               OpFunctionEnd
     %main_b = OpFunction %void None %29
         %77 = OpLabel
     %last_0 = OpVariable %_ptr_Function_uint Function
         %78 = OpAccessChain %_ptr_StorageBuffer_uint %__0 %int_1_0 %int_1_0
         %79 = OpLoad %uint %78
               OpStore %last_0 %79
         %80 = OpAccessChain %_ptr_Workgroup_uint %arr_0 %int_0_0
               OpStore %80 %uint_1_0
         %81 = OpLoad %uint %last_0
         %82 = OpLoad %uint %last_0
         %83 = OpAccessChain %_ptr_Workgroup_uint %arr_0 %81
               OpStore %83 %82
         %84 = OpAccessChain %_ptr_Workgroup_uint %arr_0 %int_0_0
         %85 = OpLoad %uint %84
         %86 = OpLoad %uint %last_0
         %87 = OpAccessChain %_ptr_Workgroup_uint %arr_0 %86
         %88 = OpLoad %uint %87
         %89 = OpIAdd %uint %85 %88
         %90 = OpAccessChain %_ptr_StorageBuffer_uint %__0 %int_0_0 %int_1_0
               OpStore %90 %89
               OpReturn
               OpFunctionEnd
     %main_c = OpFunction %void None %29
         %91 = OpLabel
     %last_1 = OpVariable %_ptr_Function_uint Function
         %92 = OpAccessChain %_ptr_StorageBuffer_uint %__1 %int_1_1 %int_2
         %93 = OpLoad %uint %92
               OpStore %last_1 %93
         %94 = OpAccessChain %_ptr_Workgroup_uint %arr_1 %int_0_1
               OpStore %94 %uint_1_1
         %95 = OpLoad %uint %last_1
         %96 = OpLoad %uint %last_1
         %97 = OpAccessChain %_ptr_Workgroup_uint %arr_1 %95
               OpStore %97 %96
         %98 = OpAccessChain %_ptr_Workgroup_uint %arr_1 %int_0_1
         %99 = OpLoad %uint %98
        %100 = OpLoad %uint %last_1
        %101 = OpAccessChain %_ptr_Workgroup_uint %arr_1 %100
        %102 = OpLoad %uint %101
        %103 = OpIAdd %uint %99 %102
        %104 = OpAccessChain %_ptr_StorageBuffer_uint %__1 %int_0_1 %int_2
               OpStore %104 %103
               OpReturn
               OpFunctionEnd
     %main_d = OpFunction %void None %29
        %105 = OpLabel
     %last_2 = OpVariable %_ptr_Function_uint Function
        %106 = OpAccessChain %_ptr_StorageBuffer_uint %__2 %int_1_2 %int_3
        %107 = OpLoad %uint %106
               OpStore %last_2 %107
        %108 = OpAccessChain %_ptr_Workgroup_uint %arr_2 %int_0_2
               OpStore %108 %uint_1_2
        %109 = OpLoad %uint %last_2
        %110 = OpLoad %uint %last_2
        %111 = OpAccessChain %_ptr_Workgroup_uint %arr_2 %109
               OpStore %111 %110
        %112 = OpAccessChain %_ptr_Workgroup_uint %arr_2 %int_0_2
        %113 = OpLoad %uint %112
        %114 = OpLoad %uint %last_2
        %115 = OpAccessChain %_ptr_Workgroup_uint %arr_2 %114
        %116 = OpLoad %uint %115
        %117 = OpIAdd %uint %113 %116
        %118 = OpAccessChain %_ptr_StorageBuffer_uint %__2 %int_0_2 %int_3
               OpStore %118 %117
               OpReturn
               OpFunctionEnd
)";

            dst.spirvAsmSources.add("comp")
                << comp << SpirVAsmBuildOptions(dst.usedVulkanVersion, SPIRV_VERSION_1_4, true);
        }
    }
};

void checkSupport(vkt::Context &context, TestConfig testConfig)
{
    if (testConfig.type == TestType::TWO_ENTRY_POINTS_EXECUTION_MODE_ID)
        context.requireDeviceFunctionality("VK_KHR_maintenance4");
    else if (testConfig.type == TestType::FOUR_ENTRY_POINTS_LARGE_SHARED_MEMORY_SPIRV_1_4)
        context.requireDeviceFunctionality("VK_KHR_spirv_1_4");
}

} // namespace

tcu::TestCaseGroup *createMultipleShaderExtendedGroup(tcu::TestContext &testCtx)
{
    typedef InstanceFactory1WithSupport<EntryPointsTest, TestConfig, FunctionSupport1<TestConfig>, Programs>
        EntryPointsTestCase;
    de::MovePtr<tcu::TestCaseGroup> mainGroup(new tcu::TestCaseGroup(testCtx, "multiple_shaders_extended"));

    TestConfig testConfig = {TestType::TWO_ENTRY_POINTS_EXECUTION_MODE_ID};
    mainGroup->addChild(new EntryPointsTestCase(testCtx, "two_entry_points_execution_mode_id", testConfig,
                                                typename FunctionSupport1<TestConfig>::Args(checkSupport, testConfig)));
    testConfig = {TestType::TWO_ENTRY_POINTS_DIFFERENT_INTERFACES};
    mainGroup->addChild(new EntryPointsTestCase(testCtx, "two_entry_points_different_interfaces", testConfig,
                                                typename FunctionSupport1<TestConfig>::Args(checkSupport, testConfig)));

    typedef InstanceFactory1WithSupport<LargeSharedMemoryTestInstance, TestConfig, FunctionSupport1<TestConfig>,
                                        Programs>
        LargeSharedMemoryTestCase;
    testConfig = {TestType::FOUR_ENTRY_POINTS_LARGE_SHARED_MEMORY_SPIRV_1_4};
    mainGroup->addChild(
        new LargeSharedMemoryTestCase(testCtx, "four_entry_points_large_shared_memory_spirv1p4", testConfig,
                                      typename FunctionSupport1<TestConfig>::Args(checkSupport, testConfig)));

    return mainGroup.release();
}

} // namespace SpirVAssembly
} // namespace vkt
