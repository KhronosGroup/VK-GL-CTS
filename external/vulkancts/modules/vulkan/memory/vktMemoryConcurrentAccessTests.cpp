/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2024 The Khronos Group Inc.
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
 * \brief Concurrent access across device and host tests.
 *//*--------------------------------------------------------------------*/

#include "vktMemoryConcurrentAccessTests.hpp"
#include "vkBufferWithMemory.hpp"
#include "vktTestCaseUtil.hpp"
#include "vkBarrierUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkObjUtil.hpp"
#include <deDefs.h>
#include <deMath.h>
#include <thread>
#include <mutex>

using namespace vk;
using BufferWithMemorySp = de::SharedPtr<BufferWithMemory>;

namespace vkt::memory
{
namespace
{

static std::mutex mtx;

enum class ResultType
{
    SUCCESS = 0,
    WRONG_INITIAL_VALUE_DURING_COMPUTE_SHADER,
    WRONG_INITIAL_VALUE_AFTER_COMPUTE_SHADER,
    WRONG_SHADER_VALUE_AFTER_COMPUTE_SHADER,
};

struct ResultInfo
{
    ResultType resultType;
    uint32_t itemIndex;
    uint32_t itemValue;
};

template <typename DataType>
void secondThreadFunction(BufferWithMemorySp buffer, uint32_t itemsCount, DataType initialValue, DataType shaderValue,
                          ResultInfo &resultInfo)
{
    const auto bufferAllocation = buffer->getAllocation();
    DataType *bufferHostPtr     = reinterpret_cast<DataType *>(bufferAllocation.getHostPtr());

    // create helper arrays that will simplify final validation code
    DataType expectedlValues[]{shaderValue, initialValue};
    ResultType afterComputeErrors[]{ResultType::WRONG_SHADER_VALUE_AFTER_COMPUTE_SHADER,
                                    ResultType::WRONG_INITIAL_VALUE_AFTER_COMPUTE_SHADER};

    // read every value that is not currently accessed by the compute shader
    // and check that it matches the original bit pattern
    for (uint32_t itemIndex = 1; itemIndex < itemsCount; itemIndex += 2)
    {
        DataType value = bufferHostPtr[itemIndex];
        if (bufferHostPtr[itemIndex] != initialValue)
        {
            resultInfo = {ResultType::WRONG_INITIAL_VALUE_DURING_COMPUTE_SHADER, itemIndex, (uint32_t)value};
            return;
        }
    }

    // wait for singnal from main thread before starting final validation
    std::lock_guard<std::mutex> lock(mtx);

    // validate whole buffer
    for (uint32_t itemIndex = 0; itemIndex < itemsCount; ++itemIndex)
    {
        // we alternately compare between the initial value and the value that was written in the shader
        uint32_t expectedValueIndex = itemIndex % 2;
        DataType value              = bufferHostPtr[itemIndex];
        if (value != expectedlValues[expectedValueIndex])
        {
            resultInfo = {afterComputeErrors[expectedValueIndex], itemIndex, (uint32_t)value};
            return;
        }
    }

    resultInfo = {ResultType::SUCCESS, 0, 0};
}

tcu::TestStatus testShaderAndHostAccess(Context &context)
{
    const DeviceInterface &vk = context.getDeviceInterface();
    const VkDevice device     = context.getDevice();
    Allocator &allocator      = context.getDefaultAllocator();

    // define byte patterns used by test; for uint32 type pattern is just repeated 4 times, and for uin16 2 times
    const uint8_t initialBytePattern = 0b01011011;
    const uint8_t shaderBytePattern  = 0b11001010;

    // create buffer with shader access usage, at least 500 bytes in size, odd size value preferred
    const VkDeviceSize bufferSize = 501;
    const auto bufferUsage =
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    VkBufferCreateInfo bufferCreateInfo = makeBufferCreateInfo(bufferSize, bufferUsage);
    BufferWithMemorySp buffer           = BufferWithMemorySp(new BufferWithMemory(
        vk, device, allocator, bufferCreateInfo, MemoryRequirement::HostVisible | MemoryRequirement::Coherent));
    const auto bufferAllocation         = buffer->getAllocation();
    uint8_t *bufferHostPtr              = reinterpret_cast<uint8_t *>(bufferAllocation.getHostPtr());

    // find smallest supported integer type
    VkDeviceSize smallestIntBytes = context.isDeviceFunctionalitySupported("VK_KHR_16bit_storage") ? 2 : 4;
    smallestIntBytes = context.isDeviceFunctionalitySupported("VK_KHR_8bit_storage") ? 1 : smallestIntBytes;

    // clear the buffer to a known bit pattern in each byte (not 0)
    for (VkDeviceSize b = 0; b < bufferSize; ++b)
        bufferHostPtr[b] = initialBytePattern;

    // create descriptor set
    const VkDescriptorType descType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    const Unique<VkDescriptorPool> descriptorPool(
        DescriptorPoolBuilder()
            .addType(descType, 1)
            .build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u));
    const Unique<VkDescriptorSetLayout> descriptorSetLayout(
        DescriptorSetLayoutBuilder().addSingleBinding(descType, VK_SHADER_STAGE_COMPUTE_BIT).build(vk, device));
    const Unique<VkDescriptorSet> descriptorSet(makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout));
    VkDescriptorBufferInfo bufferDescriptorInfo = makeDescriptorBufferInfo(**buffer, 0ull, bufferSize);
    DescriptorSetUpdateBuilder()
        .writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u), descType, &bufferDescriptorInfo)
        .update(vk, device);

    // create a compute pipeline in which we read smallest supported integer
    // from buffer and replace it with different pattern when the read value is correct
    BinaryCollection &binaryCollection = context.getBinaryCollection();
    std::string shaderName             = std::string("comp_") + std::to_string(smallestIntBytes);
    const auto shaderModule            = createShaderModule(vk, device, binaryCollection.get(shaderName));
    const auto pipelineLayout          = makePipelineLayout(vk, device, *descriptorSetLayout);
    const VkComputePipelineCreateInfo pipelineCreateInfo{
        VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO, // VkStructureType                 sType
        nullptr,                                        // const void*                     pNext
        0,                                              // VkPipelineCreateFlags           flags
        {
            // VkPipelineShaderStageCreateInfo stage
            VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, // VkStructureType                  sType
            nullptr,                                             // const void*                      pNext
            0u,                                                  // VkPipelineShaderStageCreateFlags flags
            VK_SHADER_STAGE_COMPUTE_BIT,                         // VkShaderStageFlagBits            stage
            *shaderModule,                                       // VkShaderModule                   module
            "main",                                              // const char*                      pName
            nullptr,                                             // const VkSpecializationInfo*      pSi
        },
        *pipelineLayout, // VkPipelineLayout               layout
        VK_NULL_HANDLE,  // VkPipeline                     basePipelineHandle
        0,               // int32_t                        basePipelineIndex
    };
    const auto pipeline = createComputePipeline(vk, device, VK_NULL_HANDLE, &pipelineCreateInfo);
    const auto memoryBarrier =
        makeMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_HOST_READ_BIT);
    auto cmdPool   = makeCommandPool(vk, device, context.getUniversalQueueFamilyIndex());
    auto cmdBuffer = allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

    // we leave a gap of one integer that is unread between each invocation
    const uint32_t invocationsCount = (uint32_t)deFloatCeil(float(bufferSize / smallestIntBytes) / 2.0f);

    beginCommandBuffer(vk, *cmdBuffer);

    vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipeline);
    vk.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayout, 0u, 1u, &*descriptorSet, 0u,
                             0);
    vk.cmdDispatch(*cmdBuffer, invocationsCount, 1, 1);

    // include a pipeline barrier from SHADER ACCESS to HOST ACCESS
    vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 1,
                          &memoryBarrier, 0, 0, 0, 0);

    endCommandBuffer(vk, *cmdBuffer);

    // make sure second thread does not start final validation befre submit
    std::unique_lock<std::mutex> validationLock(mtx);

    // launch the second thread
    std::thread secondThread;
    ResultInfo resultInfo{ResultType::SUCCESS, 0, 0};
    uint32_t itemsCount = static_cast<uint32_t>(bufferSize / smallestIntBytes);
    if (smallestIntBytes == 1)
        secondThread = std::thread(
            [&]
            { secondThreadFunction<uint8_t>(buffer, itemsCount, initialBytePattern, shaderBytePattern, resultInfo); });
    else if (smallestIntBytes == 2)
        secondThread =
            std::thread([&] { secondThreadFunction<uint16_t>(buffer, itemsCount, 23387u, 51914u, resultInfo); });
    else
        secondThread = std::thread(
            [&] { secondThreadFunction<uint32_t>(buffer, itemsCount, 1532713819u, 3402287818u, resultInfo); });

    // submit and wait for all commands
    submitCommandsAndWait(vk, device, context.getUniversalQueue(), *cmdBuffer);

    // signal the second thread so that it can validate whole buffer
    validationLock.unlock();

    // wait till validation finishes on the second thread
    secondThread.join();

    if (resultInfo.resultType == ResultType::SUCCESS)
        return tcu::TestStatus::pass("Pass");

    auto &log = context.getTestContext().getLog();
    switch (resultInfo.resultType)
    {
    case ResultType::SUCCESS:
        return tcu::TestStatus::pass("Pass");
    case ResultType::WRONG_INITIAL_VALUE_DURING_COMPUTE_SHADER:
        log << tcu::TestLog::Message << "Compute shader should not change initial value at index "
            << resultInfo.itemIndex << ", got " << resultInfo.itemValue << tcu::TestLog::EndMessage;
        return tcu::TestStatus::fail("Wrong initial value in compute shader");
    case ResultType::WRONG_INITIAL_VALUE_AFTER_COMPUTE_SHADER:
        log << tcu::TestLog::Message << "After execution of compute shader finished at index " << resultInfo.itemIndex
            << " there should be initial value, got " << resultInfo.itemValue << tcu::TestLog::EndMessage;
        return tcu::TestStatus::fail("Wrong initial value after compute shader");
    case ResultType::WRONG_SHADER_VALUE_AFTER_COMPUTE_SHADER:
        log << tcu::TestLog::Message << "After execution of compute shader finished at index " << resultInfo.itemIndex
            << " there should be shader written value, got " << resultInfo.itemValue << tcu::TestLog::EndMessage;
        return tcu::TestStatus::fail("Wrong shader written value");
    };

    return tcu::TestStatus::fail("Fail");
}

void initPrograms(SourceCollections &programCollection)
{
    // prepare shaders for all possible uint types - test will pick smallest supported one
    programCollection.glslSources.add("comp_1")
        << glu::ComputeSource("#version 460\n"
                              "#extension GL_EXT_shader_8bit_storage : require\n"
                              "layout(local_size_x = 1) in;\n"
                              "layout(binding = 0, std430) buffer InOutBuf { uint8_t v[]; } inOutBuf;\n"
                              "void main()\n"
                              "{\n"
                              "  uint index = gl_WorkGroupID.x * 2;\n"
                              "  if (int(inOutBuf.v[index]) == 91)\n"   // 91  = 0b01011011
                              "    inOutBuf.v[index] = uint8_t(202);\n" // 202 = 0b11001010
                              "}\n");
    programCollection.glslSources.add("comp_2")
        << glu::ComputeSource("#version 460\n"
                              "#extension GL_EXT_shader_16bit_storage : require\n"
                              "layout(local_size_x = 1) in;\n"
                              "layout(binding = 0, std430) buffer InOutBuf { uint16_t v[]; } inOutBuf;\n"
                              "void main()\n"
                              "{\n"
                              "  uint index = gl_WorkGroupID.x * 2;\n"
                              "  if (int(inOutBuf.v[index]) == 23387)\n"
                              "    inOutBuf.v[index] = uint16_t(51914);\n"
                              "}\n");
    programCollection.glslSources.add("comp_4")
        << glu::ComputeSource("#version 460\n"
                              "layout(local_size_x = 1) in;\n"
                              "layout(binding = 0, std430) buffer InOutBuf { uint v[]; } inOutBuf;\n"
                              "void main()\n"
                              "{\n"
                              "  uint index = gl_WorkGroupID.x * 2;\n"
                              "  if (int(inOutBuf.v[index]) == 1532713819u)\n"
                              "    inOutBuf.v[index] = 3402287818u;\n"
                              "}\n");
}

} // namespace

tcu::TestCaseGroup *createConcurrentAccessTests(tcu::TestContext &testCtx)
{
    de::MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(testCtx, "concurrent_access"));

    addFunctionCaseWithPrograms(group.get(), "shader_and_host", initPrograms, testShaderAndHostAccess);

    return group.release();
}

} // namespace vkt::memory
