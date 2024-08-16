/*------------------------------------------------------------------------
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
 *--------------------------------------------------------------------*/

#include "vktSpvAsmRawAccessChainTests.hpp"

#include "deSharedPtr.hpp"
#include "deSTLUtil.hpp"

#include "vktSpvAsmUtils.hpp"

#include "vkBuilderUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkPlatform.hpp"
#include "vkRefUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkImageUtil.hpp"

#include <string>
#include <vector>
#include <set>

namespace vkt
{
namespace SpirVAssembly
{

using std::set;
using std::string;
using std::to_string;
using std::vector;
using vkt::SpirVAssembly::AllocationSp;
using namespace vk;

namespace
{

struct Spec
{
    string shaderBody;
    vector<uint8_t> inputData;
    vector<uint8_t> outputData;
    vector<uint8_t> expectedOutput;
    VkDeviceSize inputDescriptorRange;
    VkDeviceSize outputDescriptorRange;
    bool usesVariablePointers;
    bool usesDescriptorIndexing;
    bool usesPhysicalBuffers;
    bool usesInt8;
    bool usesInt16;
    bool usesInt64;
};

enum BoundsCheck
{
    NO_BOUNDS_CHECK,
    BOUNDS_CHECK_PER_COMPONENT,
    BOUNDS_CHECK_PER_ELEMENT,
};

enum Qualifiers
{
    QUALIFIER_NONE               = 0,
    QUALIFIER_LOAD_NON_WRITABLE  = 1 << 0,
    QUALIFIER_LOAD_VOLATILE      = 1 << 1,
    QUALIFIER_LOAD_COHERENT      = 1 << 2,
    QUALIFIER_STORE_NON_READABLE = 1 << 3,
    QUALIFIER_STORE_VOLATILE     = 1 << 4,
    QUALIFIER_STORE_COHERENT     = 1 << 5,
};

struct Parameters
{
    const char *name;

    int inputSize;
    int inputComponents;
    int inputPrePadding;
    int inputPostPadding;
    int inputAlignment;

    int outputSize;
    int outputComponents;
    int outputPrePadding;
    int outputPostPadding;
    int outputAlignment;

    bool strideLoad;
    bool strideStore;

    bool variablePointers;
    bool descriptorIndexing;
    bool physicalBuffers;

    BoundsCheck inputBoundsCheck;
    BoundsCheck outputBoundsCheck;
    int qualifiers;

    VkDeviceSize inputDescriptorRange;
    VkDeviceSize outputDescriptorRange;
};

#ifndef CTS_USES_VULKANSC
class SpvAsmRawAccessChainInstance : public TestInstance
{
public:
    SpvAsmRawAccessChainInstance(Context &ctx, const Spec &spec);
    tcu::TestStatus iterate(void);

private:
    const Spec &m_spec;
};

class SpvAsmRawAccessChainTestCase : public TestCase
{
public:
    SpvAsmRawAccessChainTestCase(tcu::TestContext &testCtx, const char *name, const Spec &spec);
    void checkSupport(Context &context) const;
    void initPrograms(vk::SourceCollections &programCollection) const;
    TestInstance *createInstance(Context &ctx) const;

private:
    Spec m_spec;
};

const int NUM_DESCRIPTORS_ELEMENTS = 8;

SpvAsmRawAccessChainInstance::SpvAsmRawAccessChainInstance(Context &ctx, const Spec &spec)
    : TestInstance(ctx)
    , m_spec(spec)
{
}

Move<VkDescriptorSetLayout> createDescriptorSetLayout(const DeviceInterface &vkdi, const VkDevice &device,
                                                      int numDescriptors)
{
    DescriptorSetLayoutBuilder builder;

    builder.addArrayBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, numDescriptors, VK_SHADER_STAGE_COMPUTE_BIT);

    return builder.build(vkdi, device);
}

Move<VkDescriptorPool> createDescriptorPool(const DeviceInterface &vkdi, const VkDevice &device, int numDescriptors)
{
    DescriptorPoolBuilder builder;

    builder.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, numDescriptors);

    return builder.build(vkdi, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, /* maxSets = */ 2);
}

Move<VkDescriptorSet> createDescriptorSet(const DeviceInterface &vkdi, const VkDevice &device, VkDescriptorPool pool,
                                          VkDescriptorSetLayout layout)
{
    const VkDescriptorSetAllocateInfo allocInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, nullptr, pool, 1u,
                                                   &layout};

    return allocateDescriptorSet(vkdi, device, &allocInfo);
}

Move<VkPipelineLayout> createPipelineLayout(const DeviceInterface &vkdi, const VkDevice &device,
                                            VkDescriptorSetLayout descriptorSetLayout, bool usesPhysicalBuffers)
{
    const uint32_t pushConstantSize = (usesPhysicalBuffers ? 4 : 1) * sizeof(int);

    const VkPushConstantRange pushConstantRange = {
        VK_SHADER_STAGE_COMPUTE_BIT, // stageFlags
        0,                           // offset
        pushConstantSize,            // size
    };

    const VkDescriptorSetLayout descriptorSetLayouts[] = {
        descriptorSetLayout,
        descriptorSetLayout,
    };

    const VkPipelineLayoutCreateInfo createInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, // sType
        nullptr,                                       // pNext
        (VkPipelineLayoutCreateFlags)0,                // flags
        usesPhysicalBuffers ? 0u : 2u,                 // descriptorSetCount
        descriptorSetLayouts,                          // pSetLayouts
        1u,                                            // pushConstantRangeCount
        &pushConstantRange,                            // pPushConstantRanges
    };

    return createPipelineLayout(vkdi, device, &createInfo);
}

Move<VkBuffer> createBufferAndBindMemory(const DeviceInterface &vkdi, const VkDevice &device, Allocator &allocator,
                                         size_t numBytes, AllocationMp *outMemory, bool physStorageBuffer)
{
    VkBufferUsageFlags usageFlags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

    const VkBufferCreateInfo bufferCreateInfo = {
        VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, // sType
        nullptr,                              // pNext
        0u,                                   // flags
        numBytes,                             // size
        usageFlags,                           // usage
        VK_SHARING_MODE_EXCLUSIVE,            // sharingMode
        0u,                                   // queueFamilyCount
        nullptr,                              // pQueueFamilyIndices
    };

    Move<VkBuffer> buffer(createBuffer(vkdi, device, &bufferCreateInfo));
    MemoryRequirement physicalBufferRequirement =
        (physStorageBuffer ? MemoryRequirement::DeviceAddress : MemoryRequirement::Any);
    const VkMemoryRequirements requirements = getBufferMemoryRequirements(vkdi, device, *buffer);
    AllocationMp bufferMemory               = allocator.allocate(
        requirements, MemoryRequirement::Coherent | MemoryRequirement::HostVisible | physicalBufferRequirement);

    VK_CHECK(vkdi.bindBufferMemory(device, *buffer, bufferMemory->getMemory(), bufferMemory->getOffset()));
    *outMemory = bufferMemory;

    return buffer;
}

Move<VkPipeline> createComputePipeline(const DeviceInterface &vkdi, const VkDevice &device,
                                       VkPipelineLayout pipelineLayout, VkShaderModule shader)
{
    const VkPipelineShaderStageCreateInfo pipelineShaderStageCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, // sType
        nullptr,                                             // pNext
        (VkPipelineShaderStageCreateFlags)0,                 // flags
        VK_SHADER_STAGE_COMPUTE_BIT,                         // stage
        shader,                                              // module
        "main",                                              // pName
        nullptr,                                             // pSpecializationInfo
    };
    const VkComputePipelineCreateInfo pipelineCreateInfo = {
        VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO, // sType
        nullptr,                                        // pNext
        (VkPipelineCreateFlags)0,                       // flags
        pipelineShaderStageCreateInfo,                  // cs
        pipelineLayout,                                 // layout
        VK_NULL_HANDLE,                                 // basePipelineHandle
        0u,                                             // basePipelineIndex
    };

    return createComputePipeline(vkdi, device, VK_NULL_HANDLE, &pipelineCreateInfo);
}

tcu::TestStatus SpvAsmRawAccessChainInstance::iterate(void)
{
    if (m_spec.expectedOutput.size() > m_spec.outputData.size())
    {
        return tcu::TestStatus(QP_TEST_RESULT_INTERNAL_ERROR, "Expected output is larger than actual output");
    }

    const int descriptorIndex = m_spec.usesDescriptorIndexing ? 6 : 0;
    const int numDescriptors  = m_spec.usesDescriptorIndexing ? NUM_DESCRIPTORS_ELEMENTS : 1;

    const uint32_t queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();
    const VkDevice &device          = m_context.getDevice();
    const DeviceInterface &vkdi     = m_context.getDeviceInterface();
    Allocator &allocator            = m_context.getDefaultAllocator();
    const VkQueue queue             = m_context.getUniversalQueue();

    // Create memory allocations and buffers.

    AllocationMp inputAlloc;
    AllocationMp outputAlloc;
    Unique<VkBuffer> inputBuffer  = Unique<VkBuffer>(createBufferAndBindMemory(
        vkdi, device, allocator, m_spec.inputData.size(), &inputAlloc, m_spec.usesPhysicalBuffers));
    Unique<VkBuffer> outputBuffer = Unique<VkBuffer>(createBufferAndBindMemory(
        vkdi, device, allocator, m_spec.outputData.size(), &outputAlloc, m_spec.usesPhysicalBuffers));
    deMemcpy(inputAlloc->getHostPtr(), m_spec.inputData.data(), m_spec.inputData.size());
    deMemcpy(outputAlloc->getHostPtr(), m_spec.outputData.data(), m_spec.outputData.size());

    // Query memory addresses
    const VkBufferDeviceAddressInfo inputBufferDeviceAddressInfo = {
        VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, // sType
        nullptr,                                      // pNext
        inputBuffer.get(),                            // buffer
    };
    const VkBufferDeviceAddressInfo outputBufferDeviceAddressInfo = {
        VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, // sType
        nullptr,                                      // pNext
        outputBuffer.get(),                           // buffer
    };
    const VkDeviceAddress descriptorAddresses[] = {
        vkdi.getBufferDeviceAddress(device, &inputBufferDeviceAddressInfo),
        vkdi.getBufferDeviceAddress(device, &outputBufferDeviceAddressInfo),
    };

    // Create layouts and descriptor set.

    Unique<VkDescriptorSetLayout> descriptorSetLayout(createDescriptorSetLayout(vkdi, device, numDescriptors));
    Unique<VkPipelineLayout> pipelineLayout(
        createPipelineLayout(vkdi, device, *descriptorSetLayout, m_spec.usesPhysicalBuffers));
    Unique<VkDescriptorPool> descriptorPool(createDescriptorPool(vkdi, device, numDescriptors));
    Unique<VkDescriptorSet> descriptorSet1(createDescriptorSet(vkdi, device, *descriptorPool, *descriptorSetLayout));
    Unique<VkDescriptorSet> descriptorSet2(createDescriptorSet(vkdi, device, *descriptorPool, *descriptorSetLayout));

    DescriptorSetUpdateBuilder descriptorSetBuilder;

    const VkDescriptorBufferInfo inputDescriptorInfo = {
        *inputBuffer,                // buffer
        0,                           // offset
        m_spec.inputDescriptorRange, // range
    };
    const VkDescriptorBufferInfo outputDescriptorInfo = {
        *outputBuffer,                // buffer
        0,                            // offset
        m_spec.outputDescriptorRange, // range
    };

    const DescriptorSetUpdateBuilder::Location location =
        DescriptorSetUpdateBuilder::Location::bindingArrayElement(0, descriptorIndex);

    descriptorSetBuilder.writeSingle(*descriptorSet1, location, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                     &inputDescriptorInfo);
    descriptorSetBuilder.writeSingle(*descriptorSet2, location, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                     &outputDescriptorInfo);

    descriptorSetBuilder.update(vkdi, device);

    // Create compute shader and pipeline.

    const ProgramBinary &binary = m_context.getBinaryCollection().get("compute");
    Unique<VkShaderModule> module(createShaderModule(vkdi, device, binary, (VkShaderModuleCreateFlags)0u));

    Unique<VkPipeline> computePipeline(createComputePipeline(vkdi, device, *pipelineLayout, *module));

    // Create command pool and command buffer

    const Unique<VkCommandPool> cmdPool(
        createCommandPool(vkdi, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex));
    Unique<VkCommandBuffer> cmdBuffer(allocateCommandBuffer(vkdi, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

    // Create command buffer and record commands

    beginCommandBuffer(vkdi, *cmdBuffer);
    vkdi.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *computePipeline);
    if (m_spec.usesPhysicalBuffers)
    {
        vkdi.cmdPushConstants(*cmdBuffer, *pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(descriptorAddresses),
                              &descriptorAddresses);
    }
    else
    {
        vkdi.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayout, 0, 1,
                                   &descriptorSet1.get(), 0, nullptr);
        vkdi.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayout, 1, 1,
                                   &descriptorSet2.get(), 0, nullptr);
        vkdi.cmdPushConstants(*cmdBuffer, *pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(descriptorIndex),
                              &descriptorIndex);
    }
    vkdi.cmdDispatch(*cmdBuffer, 1, 1, 1);

    // Insert a barrier so data written by the shader is available to the host
    const VkMemoryBarrier memory_barrier = {
        VK_STRUCTURE_TYPE_MEMORY_BARRIER, // sType
        nullptr,                          // pNext
        VK_ACCESS_SHADER_WRITE_BIT,       // srcAccessMask
        VK_ACCESS_HOST_READ_BIT,          // dstAccessMask
    };

    vkdi.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 1,
                            &memory_barrier, 0, nullptr, 0, nullptr);

    endCommandBuffer(vkdi, *cmdBuffer);

    submitCommandsAndWait(vkdi, device, queue, *cmdBuffer);
    m_context.resetCommandPoolForVKSC(device, *cmdPool);

    if (deMemCmp(outputAlloc->getHostPtr(), m_spec.expectedOutput.data(), m_spec.expectedOutput.size()) != 0)
    {
        const size_t errorsMax     = 16u;
        const uint8_t *ptrHost     = static_cast<const uint8_t *>(outputAlloc->getHostPtr());
        const uint8_t *ptrExpected = static_cast<const uint8_t *>(m_spec.expectedOutput.data());
        size_t errors              = 0u;
        size_t ndx                 = 0u;

        for (; ndx < m_spec.expectedOutput.size(); ++ndx)
        {
            if (ptrHost[ndx] != ptrExpected[ndx])
                break;
        }

        for (; ndx < m_spec.expectedOutput.size(); ++ndx)
        {
            if (ptrHost[ndx] != ptrExpected[ndx])
            {
                m_context.getTestContext().getLog()
                    << tcu::TestLog::Message << "OutputBuffer: "
                    << " got:" << ((uint32_t)ptrHost[ndx]) << " expected:" << ((uint32_t)ptrExpected[ndx])
                    << " at byte " << ndx << tcu::TestLog::EndMessage;
                errors++;

                if (errors >= errorsMax)
                {
                    m_context.getTestContext().getLog() << tcu::TestLog::Message << "Maximum error count reached ("
                                                        << errors << "). Stop output." << tcu::TestLog::EndMessage;
                    break;
                }
            }
        }

        return tcu::TestStatus(QP_TEST_RESULT_FAIL, "Output doesn't match with expected");
    }

    return tcu::TestStatus::pass("OK");
}

SpvAsmRawAccessChainTestCase::SpvAsmRawAccessChainTestCase(tcu::TestContext &testCtx, const char *name,
                                                           const Spec &spec)
    : TestCase(testCtx, name)
    , m_spec(spec)
{
}

void SpvAsmRawAccessChainTestCase::checkSupport(Context &context) const
{
    context.requireDeviceFunctionality("VK_NV_raw_access_chains");

    if (context.getRawAccessChainsFeaturesNV().shaderRawAccessChains == VK_FALSE)
        TCU_THROW(NotSupportedError, "shaderRawAccessChains feature is not supported");

    if (m_spec.usesVariablePointers)
    {
        context.requireDeviceFunctionality("VK_KHR_variable_pointers");

        if (context.getVariablePointersFeatures().variablePointers == VK_FALSE)
            TCU_THROW(NotSupportedError, "variablePointers feature is not supported");

        if (context.getVariablePointersFeatures().variablePointersStorageBuffer == VK_FALSE)
            TCU_THROW(NotSupportedError, "variablePointersStorageBuffer feature is not supported");
    }

    if (m_spec.usesPhysicalBuffers)
    {
        context.requireDeviceFunctionality("VK_KHR_buffer_device_address");

        if (context.getBufferDeviceAddressFeatures().bufferDeviceAddress == VK_FALSE)
            TCU_THROW(NotSupportedError, "bufferDeviceAddress feature is not supported");
    }

    if (m_spec.usesInt8)
    {
        context.requireDeviceFunctionality("VK_KHR_shader_float16_int8");

        if (context.getShaderFloat16Int8Features().shaderInt8 == VK_FALSE)
            TCU_THROW(NotSupportedError, "shaderInt8 feature is not supported");
    }

    if (m_spec.usesInt16)
        context.requireDeviceCoreFeature(vkt::DEVICE_CORE_FEATURE_SHADER_INT16);

    if (m_spec.usesInt64)
        context.requireDeviceCoreFeature(vkt::DEVICE_CORE_FEATURE_SHADER_INT64);
}

void SpvAsmRawAccessChainTestCase::initPrograms(vk::SourceCollections &programCollection) const
{
    string shaderAssembly = m_spec.shaderBody;

    programCollection.spirvAsmSources.add("compute")
        << shaderAssembly.c_str()
        << vk::SpirVAsmBuildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_6);
}

TestInstance *SpvAsmRawAccessChainTestCase::createInstance(Context &ctx) const
{
    return new SpvAsmRawAccessChainInstance(ctx, m_spec);
}

template <typename T>
vector<uint8_t> castVector(const vector<T> &input)
{
    vector<uint8_t> result(input.size() * sizeof(T));
    deMemcpy(result.data(), input.data(), result.size());
    return result;
}

class CodeGen
{
public:
    void AddCapability(const string &text)
    {
        m_capabilities += text;
    }

    void AddExtension(const string &text)
    {
        m_extensions += text;
    }

    void AddHeader(const string &text)
    {
        m_header += text;
    }

    void AddDecoration(const string &text)
    {
        m_decorations += text;
    }

    void AddDeclaration(const string &text)
    {
        m_declarations += text;
    }

    void AddBody(const string &text)
    {
        m_body += text;
    }

    string Uint32(uint32_t value)
    {
        string valueText  = to_string(value);
        string definition = "%uint_" + valueText;
        if (m_definedInt32.insert(value).second)
        {
            AddDeclaration(definition + " = OpConstant %uint " + valueText + '\n');
        }
        return definition;
    }

    string Result()
    {
        return m_capabilities + m_extensions + m_header + m_decorations + m_declarations + m_body;
    }

private:
    string m_capabilities;
    string m_extensions;
    string m_header;
    string m_decorations;
    string m_declarations;
    string m_body;
    set<int> m_definedInt32;
};

const char *GetRobustnessOperand(BoundsCheck boundsCheck)
{
    switch (boundsCheck)
    {
    default:
    case NO_BOUNDS_CHECK:
        return "";
    case BOUNDS_CHECK_PER_COMPONENT:
        return " RobustnessPerComponentNV";
    case BOUNDS_CHECK_PER_ELEMENT:
        return " RobustnessPerElementNV";
    }
}

void SetLoadDecorations(const Parameters &p, CodeGen &gen, const string &ptr)
{
    if (p.qualifiers & QUALIFIER_LOAD_NON_WRITABLE)
        gen.AddDecoration("OpDecorate " + ptr + " NonWritable\n");
    if (p.qualifiers & QUALIFIER_LOAD_VOLATILE)
        gen.AddDecoration("OpDecorate " + ptr + " Volatile\n");
    if (p.qualifiers & QUALIFIER_LOAD_COHERENT)
        gen.AddDecoration("OpDecorate " + ptr + " Coherent\n");
}

void SetStoreDecorations(const Parameters &p, CodeGen &gen, const string &ptr)
{
    if (p.qualifiers & QUALIFIER_STORE_NON_READABLE)
        gen.AddDecoration("OpDecorate " + ptr + " NonReadable\n");
    if (p.qualifiers & QUALIFIER_STORE_VOLATILE)
        gen.AddDecoration("OpDecorate " + ptr + " Volatile\n");
    if (p.qualifiers & QUALIFIER_STORE_COHERENT)
        gen.AddDecoration("OpDecorate " + ptr + " Coherent\n");
}
#endif // !CTS_USES_VULKANSC

void addTest(tcu::TestCaseGroup *group, const Parameters &p)
{
#ifndef CTS_USES_VULKANSC
    tcu::TestContext &testCtx = group->getTestContext();

    const int localSize             = 32;
    const int inputComponentStride  = p.inputComponents * p.inputSize + p.inputPrePadding + p.inputPostPadding;
    const int outputComponentStride = p.outputComponents * p.outputSize + p.outputPrePadding + p.outputPostPadding;

    de::Random random(434);
    vector<uint8_t> inputData(inputComponentStride * localSize, 0xcc);
    vector<uint8_t> outputData(outputComponentStride * localSize, 0xcc);
    vector<uint8_t> expectedOutput = outputData;

    // Fill input with garbage first
    for (size_t i = 0; i < inputData.size(); i++)
        inputData[i] = random.getUint8();

    uint8_t *inputIterator          = inputData.data();
    uint8_t *expectedOutputIterator = expectedOutput.data();
    for (VkDeviceSize i = 0; i < static_cast<VkDeviceSize>(localSize); i++)
    {
        const uint64_t values[4] = {random.getUint64(), random.getUint64(), random.getUint64(), random.getUint64()};
        uint64_t expectedResult  = 0;
        for (int j = 0; j < p.inputComponents; j++)
        {
            uint64_t value = 0;
            memcpy(&value, reinterpret_cast<const uint8_t *>(&values[0]) + j * p.inputSize, p.inputSize);

            if (p.inputBoundsCheck == BOUNDS_CHECK_PER_ELEMENT)
            {
                if (i * inputComponentStride >= p.inputDescriptorRange)
                    value = 0;
            }
            if (p.inputBoundsCheck == BOUNDS_CHECK_PER_COMPONENT)
            {
                if (i * inputComponentStride + j * p.inputSize + p.inputPrePadding >= p.inputDescriptorRange)
                    value = 0;
            }

            expectedResult += value;
        }

        // Truncate the expected result because the shader operates with the input type.
        uint64_t result = 0;
        memcpy(&result, &expectedResult, p.inputSize);

        memcpy(inputIterator + p.inputPrePadding, &values[0], p.inputComponents * p.inputSize);

        for (int j = 0; j < p.outputComponents; j++)
        {
            bool checked = false;

            if (p.outputBoundsCheck == BOUNDS_CHECK_PER_ELEMENT)
                checked = i * outputComponentStride >= p.outputDescriptorRange;

            if (p.outputBoundsCheck == BOUNDS_CHECK_PER_COMPONENT)
                checked = i * outputComponentStride + j * p.outputSize + p.outputPrePadding >= p.outputDescriptorRange;

            if (!checked)
                memcpy(expectedOutputIterator + p.outputPrePadding + j * p.outputSize, &result, p.outputSize);
            result += 1;
        }

        inputIterator += inputComponentStride;
        expectedOutputIterator += outputComponentStride;
    }

    CodeGen gen;
    gen.AddCapability("OpCapability Shader\n"
                      "OpCapability RawAccessChainsNV\n");

    gen.AddExtension("OpExtension \"SPV_NV_raw_access_chains\"\n");

    gen.AddHeader("%glslExt = OpExtInstImport \"GLSL.std.450\"\n");

    if (p.physicalBuffers)
    {
        gen.AddCapability("OpCapability PhysicalStorageBufferAddresses\n");
        gen.AddExtension("OpExtension \"SPV_KHR_physical_storage_buffer\"\n");
        gen.AddHeader("OpMemoryModel PhysicalStorageBuffer64 GLSL450\n");
    }
    else
    {
        gen.AddHeader("OpMemoryModel Logical GLSL450\n");
    }

    if (p.physicalBuffers)
    {
        gen.AddHeader("OpEntryPoint GLCompute %main \"main\" %gl_LocalInvocationID %pushConstants\n");
    }
    else if (p.descriptorIndexing)
    {
        gen.AddHeader("OpEntryPoint GLCompute %main \"main\" %gl_LocalInvocationID %pushConstants %inputBuffers "
                      "%outputBuffers\n");
    }
    else
    {
        gen.AddHeader(
            "OpEntryPoint GLCompute %main \"main\" %gl_LocalInvocationID %pushConstants %inputBuffer %outputBuffer\n");
    }

    gen.AddHeader("OpExecutionMode %main LocalSize " + to_string(localSize) + " 1 1\n");

    gen.AddDecoration("OpDecorate %gl_LocalInvocationID BuiltIn LocalInvocationId\n");

    switch (p.inputSize)
    {
    case 8:
        gen.AddCapability("OpCapability Int64\n");
        gen.AddDeclaration("%type = OpTypeInt 64 1\n");
        break;
    case 4:
        gen.AddDeclaration("%type = OpTypeInt 32 1\n");
        break;
    case 2:
        gen.AddCapability("OpCapability Int16\n");
        gen.AddDeclaration("%type = OpTypeInt 16 1\n");
        break;
    case 1:
        gen.AddCapability("OpCapability Int8\n");
        gen.AddDeclaration("%type = OpTypeInt 8 1\n");
        break;
    }

    if (p.variablePointers)
        gen.AddCapability("OpCapability VariablePointers\n");

    gen.AddDeclaration("%void = OpTypeVoid\n"
                       "%v2type = OpTypeVector %type 2\n"
                       "%v3type = OpTypeVector %type 3\n"
                       "%v4type = OpTypeVector %type 4\n"
                       "%uint = OpTypeInt 32 0\n"
                       "%v2uint = OpTypeVector %uint 2\n"
                       "%v3uint = OpTypeVector %uint 3\n"
                       "%v4uint = OpTypeVector %uint 4\n"
                       "%TypeFunctionMain = OpTypeFunction %void\n"
                       "%_ptr_Input_v3uint = OpTypePointer Input %v3uint\n"
                       "%_ptr_Input_uint = OpTypePointer Input %uint\n"
                       "%gl_LocalInvocationID = OpVariable %_ptr_Input_v3uint Input\n");
    if (p.physicalBuffers)
    {
        gen.AddDeclaration("%TypeStructBDAs = OpTypeStruct %v2uint %v2uint\n"
                           "%_ptr_Storage_type = OpTypePointer PhysicalStorageBuffer %type\n"
                           "%_ptr_Storage_v2type = OpTypePointer PhysicalStorageBuffer %v2type\n"
                           "%_ptr_Storage_v3type = OpTypePointer PhysicalStorageBuffer %v3type\n"
                           "%_ptr_Storage_v4type = OpTypePointer PhysicalStorageBuffer %v4type\n"
                           "%_ptr_PushConstant_v2uint = OpTypePointer PushConstant %v2uint\n"
                           "%_ptr_PushConstant_BDAs = OpTypePointer PushConstant %TypeStructBDAs\n"
                           "%pushConstants = OpVariable %_ptr_PushConstant_BDAs PushConstant\n");
        gen.AddDecoration("OpDecorate %TypeStructBDAs Block\n"
                          "OpMemberDecorate %TypeStructBDAs 0 Offset 0\n"
                          "OpMemberDecorate %TypeStructBDAs 1 Offset 8\n");
    }
    else
    {
        gen.AddDeclaration("%TypeStructUint = OpTypeStruct %uint\n"
                           "%TypeStructPushConstant = OpTypeStruct %uint\n"
                           "%TypePointerBuffer = OpTypePointer StorageBuffer %TypeStructUint\n"
                           "%_ptr_Storage_type = OpTypePointer StorageBuffer %type\n"
                           "%_ptr_Storage_v2type = OpTypePointer StorageBuffer %v2type\n"
                           "%_ptr_Storage_v3type = OpTypePointer StorageBuffer %v3type\n"
                           "%_ptr_Storage_v4type = OpTypePointer StorageBuffer %v4type\n"
                           "%_ptr_PushConstant_uint = OpTypePointer PushConstant %uint\n"
                           "%_ptr_PushConstant = OpTypePointer PushConstant %TypeStructPushConstant\n"
                           "%pushConstants = OpVariable %_ptr_PushConstant PushConstant\n");
        gen.AddDecoration("OpDecorate %TypeStructUint Block\n"
                          "OpMemberDecorate %TypeStructUint 0 Offset 0\n"
                          "OpDecorate %TypeStructPushConstant Block\n"
                          "OpMemberDecorate %TypeStructPushConstant 0 Offset 0\n");

        if (p.descriptorIndexing)
        {
            gen.AddCapability("OpCapability RuntimeDescriptorArray\n");
            gen.AddDecoration("OpDecorate %outputBuffers DescriptorSet 1\n"
                              "OpDecorate %outputBuffers Binding 0\n"
                              "OpDecorate %inputBuffers DescriptorSet 0\n"
                              "OpDecorate %inputBuffers Binding 0\n");
            gen.AddDeclaration("%TypeRuntimeArrayStructUint = OpTypeRuntimeArray %TypeStructUint\n"
                               "%TypePointerBufferArray = OpTypePointer StorageBuffer %TypeRuntimeArrayStructUint\n"
                               "%outputBuffers = OpVariable %TypePointerBufferArray StorageBuffer\n"
                               "%inputBuffers = OpVariable %TypePointerBufferArray StorageBuffer\n");
        }
        else
        {
            gen.AddDecoration("OpDecorate %outputBuffer DescriptorSet 1\n"
                              "OpDecorate %outputBuffer Binding 0\n"
                              "OpDecorate %inputBuffer DescriptorSet 0\n"
                              "OpDecorate %inputBuffer Binding 0\n");
            gen.AddDeclaration("%outputBuffer = OpVariable %TypePointerBuffer StorageBuffer\n"
                               "%inputBuffer = OpVariable %TypePointerBuffer StorageBuffer\n");
        }
    }

    // Generate output types
    string scalarOutputType = "uint";
    if (p.outputSize != 4)
    {
        string bits = to_string(p.outputSize * 8);
        scalarOutputType += bits;

        gen.AddCapability("OpCapability Int" + bits + '\n');
        gen.AddDeclaration('%' + scalarOutputType + " = OpTypeInt " + bits + " 0\n");
    }

    string outputType = scalarOutputType;
    if (p.outputComponents != 1)
    {
        outputType = 'v' + to_string(p.outputComponents) + scalarOutputType;

        if (p.outputSize != 4)
            gen.AddDeclaration('%' + outputType + " = OpTypeVector %" + scalarOutputType + ' ' +
                               to_string(p.outputComponents) + '\n');
    }
    gen.AddDeclaration("%_ptr_Storage_" + outputType + " = OpTypePointer StorageBuffer %" + outputType + '\n');

    int inputAlignment  = p.inputAlignment;
    int outputAlignment = p.outputAlignment;

    // Remove explicit alignment when it is naturally aligned.
    if (inputAlignment == p.inputSize * p.inputComponents)
        inputAlignment = 0;
    if (outputAlignment == p.outputSize * p.outputComponents)
        outputAlignment = 0;

    // When using physical buffers, always specify an alignment.
    if (p.physicalBuffers)
    {
        if (inputAlignment == 0)
            inputAlignment = p.inputSize;
        if (outputAlignment == 0)
            outputAlignment = p.outputSize;
    }

    const string inputStrideText     = gen.Uint32(inputComponentStride);
    const string outputStrideText    = gen.Uint32(outputComponentStride);
    const string inputAlignmentText  = inputAlignment != 0 ? (" Aligned " + to_string(inputAlignment)) : "";
    const string outputAlignmentText = outputAlignment != 0 ? (" Aligned " + to_string(outputAlignment)) : "";

    const char *const inputRobustness  = GetRobustnessOperand(p.inputBoundsCheck);
    const char *const outputRobustness = GetRobustnessOperand(p.outputBoundsCheck);

    gen.AddBody("%main = OpFunction %void None %TypeFunctionMain\n"
                "%mainLabel = OpLabel\n"
                "%localInvocationPtr = OpAccessChain %_ptr_Input_uint %gl_LocalInvocationID " +
                gen.Uint32(0) +
                "\n"
                "%localInvocation = OpLoad %uint %localInvocationPtr\n");

    const string result = p.inputComponents > 1 ? "%value" : "%result";
    const string type   = p.inputComponents > 1 ? ('v' + to_string(p.inputComponents) + "type") : "type";
    SetLoadDecorations(p, gen, "%pointer");

    string stride;
    string elementIndex;
    string offset;
    if (p.strideLoad)
    {
        stride       = inputStrideText;
        elementIndex = "%localInvocation";
        offset       = gen.Uint32(p.inputPrePadding);
    }
    else
    {
        stride = elementIndex = gen.Uint32(0);
        offset                = "%loadOffset";
        gen.AddBody("%elementOffset = OpIMul %uint %localInvocation " + inputStrideText +
                    "\n"
                    "%loadOffset = OpIAdd %uint %elementOffset " +
                    gen.Uint32(p.inputPrePadding) + '\n');
    }

    if (p.physicalBuffers)
    {
        gen.AddBody("%inputBufferPointer = OpAccessChain %_ptr_PushConstant_v2uint %pushConstants " + gen.Uint32(0) +
                    "\n"
                    "%inputBufferAddress = OpLoad %v2uint %inputBufferPointer\n"
                    "%inputBuffer = OpBitcast %_ptr_Storage_type %inputBufferAddress\n"
                    "%outputBufferPointer = OpAccessChain %_ptr_PushConstant_v2uint %pushConstants " +
                    gen.Uint32(1) +
                    "\n"
                    "%outputBufferAddress = OpLoad %v2uint %outputBufferPointer\n"
                    "%outputBuffer = OpBitcast %_ptr_Storage_type %outputBufferAddress\n");
    }
    if (p.descriptorIndexing)
    {
        gen.AddBody("%descriptorIndexPointer = OpAccessChain %_ptr_PushConstant_uint %pushConstants " + gen.Uint32(0) +
                    "\n"
                    "%descriptorIndex = OpLoad %uint %descriptorIndexPointer\n"
                    "%inputBuffer = OpAccessChain %TypePointerBuffer %inputBuffers %descriptorIndex\n"
                    "%outputBuffer = OpAccessChain %TypePointerBuffer %outputBuffers %descriptorIndex\n");
    }

    gen.AddBody("%pointer = OpRawAccessChainNV %_ptr_Storage_" + type + " %inputBuffer " + stride + ' ' + elementIndex +
                ' ' + offset + inputRobustness + '\n' + result + " = OpLoad %" + type + " %pointer" +
                inputAlignmentText + '\n');

    for (int i = 0; i < p.inputComponents; i++)
    {
        const char id                = static_cast<char>('1' + i);
        const string componentResult = p.inputComponents > 1 ? (string("%value") + id) : "%result";

        if (p.inputComponents > 1)
            gen.AddBody(componentResult + " = OpCompositeExtract %type %value " + to_string(i) + '\n');
    }
    switch (p.inputComponents)
    {
    case 1:
        break;
    case 2:
        gen.AddBody("%result = OpIAdd %type %value1 %value2\n");
        break;
    case 3:
        gen.AddBody("%value12 = OpIAdd %type %value1 %value2\n"
                    "%result = OpIAdd %type %value12 %value3\n");
        break;
    case 4:
        gen.AddBody("%value12 = OpIAdd %type %value1 %value2\n"
                    "%value34 = OpIAdd %type %value3 %value4\n"
                    "%result = OpIAdd %type %value12 %value34\n");
        break;
    }
    if (p.inputSize == p.outputSize)
        gen.AddBody("%storeValue = OpBitcast %uint %result\n");
    else
        gen.AddBody("%storeValue = OpUConvert %" + scalarOutputType + " %result\n");

    SetStoreDecorations(p, gen, "%storePointer");

    if (p.strideStore)
    {
        gen.AddBody("%storePointer = OpRawAccessChainNV %_ptr_Storage_" + outputType + " %outputBuffer " +
                    outputStrideText + " %localInvocation " + gen.Uint32(p.outputPrePadding) + outputRobustness + '\n');
    }
    else
    {
        gen.AddBody("%storeElementOffset = OpIMul %uint %localInvocation " + outputStrideText +
                    "\n"
                    "%storeOffset = OpIAdd %uint %storeElementOffset " +
                    gen.Uint32(p.outputPrePadding) +
                    "\n"
                    "%storePointer = OpRawAccessChainNV %_ptr_Storage_" +
                    outputType + " %outputBuffer " + gen.Uint32(0) + ' ' + gen.Uint32(0) + " %storeOffset" +
                    outputRobustness + '\n');
    }

    if (p.outputComponents == 1)
    {
        gen.AddBody("OpStore %storePointer %storeValue" + outputAlignmentText + '\n');
    }
    else
    {
        string composites = "%storeValue";
        for (int i = 1; i < p.outputComponents; i++)
        {
            string delta  = "%delta" + to_string(i);
            string scalar = "%storeValue" + to_string(i);
            gen.AddDeclaration(delta + " = OpConstant %" + scalarOutputType + ' ' + to_string(i) + '\n');
            gen.AddBody(scalar + " = OpIAdd %" + scalarOutputType + " %storeValue " + delta + '\n');
            composites += ' ' + scalar;
        }
        gen.AddBody("%storeVector = OpCompositeConstruct %" + outputType + ' ' + composites +
                    "\n"
                    "OpStore %storePointer %storeVector" +
                    outputAlignmentText + '\n');
    }
    gen.AddBody("OpReturn\n"
                "OpFunctionEnd\n");

    Spec spec;
    spec.shaderBody             = gen.Result();
    spec.inputData              = castVector(inputData);
    spec.outputData             = castVector(outputData);
    spec.expectedOutput         = castVector(expectedOutput);
    spec.inputDescriptorRange   = p.inputDescriptorRange;
    spec.outputDescriptorRange  = p.outputDescriptorRange;
    spec.usesVariablePointers   = p.variablePointers;
    spec.usesDescriptorIndexing = p.descriptorIndexing;
    spec.usesPhysicalBuffers    = p.physicalBuffers;
    spec.usesInt8               = (p.inputSize == 1) || (p.outputSize == 1);
    spec.usesInt16              = (p.inputSize == 2) || (p.outputSize == 2);
    spec.usesInt64              = (p.inputSize == 8) || (p.outputSize == 8);

    group->addChild(new SpvAsmRawAccessChainTestCase(testCtx, p.name, spec));
#else
    DE_UNREF(group);
    DE_UNREF(p);
#endif
}

void addTests(tcu::TestCaseGroup *group)
{
    static const int qualifiersCombinations[] = {
        QUALIFIER_NONE,
        QUALIFIER_LOAD_NON_WRITABLE,
        QUALIFIER_LOAD_VOLATILE,
        QUALIFIER_LOAD_COHERENT,
        QUALIFIER_LOAD_VOLATILE | QUALIFIER_LOAD_COHERENT,
        QUALIFIER_STORE_NON_READABLE,
        QUALIFIER_STORE_VOLATILE,
        QUALIFIER_STORE_COHERENT,
    };

    for (const bool testingStore : {false, true})
        for (const bool variablePointers : {false, true})
            for (const bool descriptorIndexing : {false, true})
                for (const bool physicalBuffers : {false, true})
                    for (const BoundsCheck boundsCheck :
                         {NO_BOUNDS_CHECK, BOUNDS_CHECK_PER_COMPONENT, BOUNDS_CHECK_PER_ELEMENT})
                        for (const int qualifiers : qualifiersCombinations)
                            for (const bool stride : {true, false})
                                for (const int size : {4, 8, 2, 1})
                                    for (const int components : {1, 2, 3, 4})
                                        for (const int alignmentDiv : {1, 4, 2, 3})
                                        {
                                            // Skip illegal combinations.
                                            if (!stride && boundsCheck == BOUNDS_CHECK_PER_ELEMENT)
                                                continue;

                                            // Skip alignments that don't match the number of components.
                                            if (components < alignmentDiv)
                                                continue;
                                            if (components % alignmentDiv != 0)
                                                continue;

                                            // Skip physical pointer related tests.
                                            if (physicalBuffers)
                                            {
                                                if (variablePointers)
                                                    continue;
                                                if (descriptorIndexing)
                                                    continue;
                                                if (boundsCheck != NO_BOUNDS_CHECK)
                                                    continue;
                                            }

                                            // Skip complex qualifiers mixed with other complex configurations.
                                            if (qualifiers != QUALIFIER_NONE &&
                                                qualifiers != QUALIFIER_LOAD_NON_WRITABLE)
                                            {
                                                if (size != 4)
                                                    continue;
                                                if (components != 4)
                                                    continue;
                                                if (alignmentDiv != 1)
                                                    continue;
                                            }

                                            // Add padding to test instruction offsets.
                                            int prePadding = components * size;
                                            while (!deIsPowerOfTwo32(prePadding))
                                                prePadding += size;

                                            // Add misalignment when requested.
                                            int alignment = 0;
                                            if (alignmentDiv > 1)
                                            {
                                                alignment = (components * size) / alignmentDiv;
                                                prePadding += alignment;
                                            }

                                            string name;

                                            name += testingStore ? "store_" : "load_";

                                            if (physicalBuffers)
                                                name += "physical_buffers_";

                                            if (variablePointers)
                                                name += "variable_pointers_";

                                            if (descriptorIndexing)
                                                name += "descriptor_indexing_";

                                            if (components > 1)
                                                name += 'v' + to_string(components);
                                            name += "int";
                                            name += to_string(size * 8);

                                            if (alignment != 0)
                                            {
                                                name += "_align_";
                                                name += to_string(alignment);
                                            }

                                            name += stride ? "_stride" : "_no_stride";

                                            switch (boundsCheck)
                                            {
                                            case NO_BOUNDS_CHECK:
                                                name += "_no_bounds";
                                                break;
                                            case BOUNDS_CHECK_PER_COMPONENT:
                                                name += "_per_component";
                                                break;
                                            case BOUNDS_CHECK_PER_ELEMENT:
                                                name += "_per_element";
                                                break;
                                            }

                                            // Add qualifiers to the name
                                            if (qualifiers & QUALIFIER_LOAD_NON_WRITABLE)
                                                name += "_load_non_writable";
                                            if (qualifiers & QUALIFIER_LOAD_VOLATILE)
                                                name += "_load_volatile";
                                            if (qualifiers & QUALIFIER_LOAD_COHERENT)
                                                name += "_load_coherent";
                                            if (qualifiers & QUALIFIER_STORE_NON_READABLE)
                                                name += "_store_non_readable";
                                            if (qualifiers & QUALIFIER_STORE_VOLATILE)
                                                name += "_store_volatile";
                                            if (qualifiers & QUALIFIER_STORE_COHERENT)
                                                name += "_store_coherent";

                                            int inputAlignment  = testingStore ? 0 : alignment;
                                            int outputAlignment = testingStore ? alignment : 0;

                                            int inputPrePadding  = testingStore ? 0 : prePadding;
                                            int outputPrePadding = testingStore ? prePadding : 0;

                                            int inputComponents  = testingStore ? 4 : components;
                                            int outputComponents = testingStore ? components : 4;

                                            int inputSize  = testingStore ? 4 : size;
                                            int outputSize = testingStore ? size : 4;

                                            BoundsCheck loadBoundsCheck  = testingStore ? NO_BOUNDS_CHECK : boundsCheck;
                                            BoundsCheck storeBoundsCheck = testingStore ? boundsCheck : NO_BOUNDS_CHECK;

                                            bool strideLoad  = stride || testingStore;
                                            bool strideStore = stride || !testingStore;

                                            // Align structures to a power of two with post padding.
                                            int inputPostPadding = 0;
                                            while (!deIsPowerOfTwo32(inputComponents * inputSize + inputPrePadding +
                                                                     inputPostPadding))
                                                inputPostPadding += inputSize;

                                            int outputPostPadding = 0;
                                            while (!deIsPowerOfTwo32(outputComponents * outputSize + outputPrePadding +
                                                                     outputPostPadding))
                                                outputPostPadding += outputSize;

                                            // Set an arbitrary descriptor range.
                                            VkDeviceSize descriptorRange = VK_WHOLE_SIZE;
                                            if (boundsCheck != NO_BOUNDS_CHECK)
                                            {
                                                // Bind 11 structures to test bounds checking.
                                                int postPadding = testingStore ? outputPostPadding : inputPostPadding;
                                                descriptorRange = 11 * (components * size + prePadding + postPadding);
                                            }
                                            if (boundsCheck == BOUNDS_CHECK_PER_COMPONENT)
                                            {
                                                // In the case of per component bounds checking, skip one component too.
                                                if (components > 1)
                                                    descriptorRange -= size;
                                            }

                                            VkDeviceSize inputDescriptorRange =
                                                testingStore ? VK_WHOLE_SIZE : descriptorRange;
                                            VkDeviceSize outputDescriptorRange =
                                                testingStore ? descriptorRange : VK_WHOLE_SIZE;

                                            const Parameters parameters = {
                                                name.c_str(),
                                                inputSize,
                                                inputComponents,
                                                inputPrePadding,
                                                inputPostPadding,
                                                inputAlignment,
                                                outputSize,
                                                outputComponents,
                                                outputPrePadding,
                                                outputPostPadding,
                                                outputAlignment,
                                                strideLoad,
                                                strideStore,
                                                variablePointers,
                                                descriptorIndexing,
                                                physicalBuffers,
                                                loadBoundsCheck,
                                                storeBoundsCheck,
                                                qualifiers,
                                                inputDescriptorRange,
                                                outputDescriptorRange,
                                            };
                                            addTest(group, parameters);
                                        }
}

} // namespace

tcu::TestCaseGroup *createRawAccessChainGroup(tcu::TestContext &testCtx)
{
    de::MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(testCtx, "raw_access_chain", "OpRawAccessChain"));

    addTests(group.get());

    return group.release();
}

} // namespace SpirVAssembly
} // namespace vkt
