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
        DE_NULL,
        static_cast<VkPipelineShaderStageCreateFlags>(0u),
        VK_SHADER_STAGE_COMPUTE_BIT,
        *shaderModule,
        "mainA",
        DE_NULL,
    };
    const VkPipelineShaderStageCreateInfo pipelineBShaderStageParams{
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        DE_NULL,
        static_cast<VkPipelineShaderStageCreateFlags>(0u),
        VK_SHADER_STAGE_COMPUTE_BIT,
        *shaderModule,
        "mainB",
        DE_NULL,
    };
    VkComputePipelineCreateInfo pipelineCreateInfo{
        VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        DE_NULL,
        static_cast<VkPipelineCreateFlags>(0u),
        pipelineBShaderStageParams,
        *pipelineLayout,
        DE_NULL,
        0,
    };

    Unique<VkPipeline> pipelineB(createComputePipeline(vk, device, DE_NULL, &pipelineCreateInfo));
    pipelineCreateInfo.stage = pipelineAShaderStageParams;
    Unique<VkPipeline> pipelineA(createComputePipeline(vk, device, DE_NULL, &pipelineCreateInfo));

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
    }
};

void checkSupport(vkt::Context &context, TestConfig testConfig)
{
    if (testConfig.type == TestType::TWO_ENTRY_POINTS_EXECUTION_MODE_ID)
        context.requireDeviceFunctionality("VK_KHR_maintenance4");
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

    return mainGroup.release();
}

} // namespace SpirVAssembly
} // namespace vkt
