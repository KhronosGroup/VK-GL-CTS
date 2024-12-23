/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2026 The Khronos Group Inc.
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
 * \brief VK_KHR_shader_abort extension tests
 *//*--------------------------------------------------------------------*/

#include "vktTestCase.hpp"
#include "vktCustomInstancesDevices.hpp"
#include "vktPostmortemTestsUtils.hpp"
#include "vktPostmortemUtil.hpp"

#include "vkDefs.hpp"
#include "vkRef.hpp"
#include "vkRefUtil.hpp"
#include "vkPlatform.hpp"
#include "vkPrograms.hpp"
#include "vkRefUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkBarrierUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkDeviceUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkPipelineBinaryUtil.hpp"

#include "tcuTestLog.hpp"

#include <vector>
#include <cstring>
#include <string>
#include <sstream>
#include <iomanip>

using namespace vk;

namespace vkt
{
namespace postmortem
{
namespace
{

CustomDevice createPostmortemDevice(Context &context, const InstanceWrapper &instance,
                                    const std::vector<const char *> &extensions, const void *pNext)
{
    const float queuePriority = 1.0f;

    const VkDeviceQueueCreateInfo queueParams = {
        VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, // VkStructureType sType;
        nullptr,                                    // const void* pNext;
        0u,                                         // VkDeviceQueueCreateFlags flags;
        context.getUniversalQueueFamilyIndex(),     // uint32_t queueFamilyIndex;
        1u,                                         // uint32_t queueCount;
        &queuePriority                              // const float* pQueuePriorities;
    };

    const VkDeviceCreateInfo deviceParams = {
        VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,             // VkStructureType sType;
        pNext,                                            // const void* pNext;
        0u,                                               // VkDeviceCreateFlags flags;
        1u,                                               // uint32_t queueCreateInfoCount;
        &queueParams,                                     // const VkDeviceQueueCreateInfo* pQueueCreateInfos;
        0u,                                               // uint32_t enabledLayerCount;
        nullptr,                                          // const char* const* ppEnabledLayerNames;
        static_cast<uint32_t>(extensions.size()),         // uint32_t enabledExtensionCount;
        extensions.empty() ? nullptr : extensions.data(), // const char* const* ppEnabledExtensionNames;
        nullptr                                           // const VkPhysicalDeviceFeatures* pEnabledFeatures;
    };

    return instance.createCustomDevice(&deviceParams);
}

enum class TestType
{
    TERMINATION,          // Basic termination test
    EMPTY_MESSAGE,        // Test with empty message struct
    LARGE_BUFFER_MESSAGE, // Test with large buffer data
    LARGE_CONST_MESSAGE,  // Test with large constant data
    PIPELINE_CACHE,       // Pipeline cache is deterministic across fresh devices
    PIPELINE_BINARY,      // VK_KHR_pipeline_binary blobs are bit-identical across fresh devices
    SHADER_OBJECT,        // VK_EXT_shader_object binary data is bit-identical across fresh devices
    MAX_MESSAGE_SIZE,     // Test VkPhysicalDeviceShaderAbortPropertiesKHR::maxShaderAbortMessageSize
};

struct TestParams
{
    TestType type;
    uint32_t invocationCount; // Number of shader invocations
    uint32_t messageSize;     // Size of the message payload
};

class ShaderAbortInstance : public TestInstance
{
public:
    ShaderAbortInstance(Context &context, const TestParams &params) : TestInstance(context), m_params(params)
    {
    }
    ~ShaderAbortInstance() = default;
    virtual tcu::TestStatus iterate() override;

protected:
    tcu::TestStatus iteratePipelineCache();
    tcu::TestStatus iteratePipelineBinary();
    tcu::TestStatus iterateShaderObject();
    bool checkDeviceFault(const VkDevice device, const DeviceInterface &deviceInterface);
    void verifyAbortMessage(const std::vector<uint8_t> &messageData);
    void parseMessageComponents(const std::vector<uint8_t> &messageData, std::string &formatString,
                                std::vector<uint32_t> &arguments);

    const TestParams m_params;
    uint64_t m_maxShaderAbortMessageSize = 0;
};

class ShaderAbortCase : public TestCase
{
public:
    ShaderAbortCase(tcu::TestContext &testCtx, const std::string &name, const TestParams &params)
        : TestCase(testCtx, name)
        , m_params(params)
    {
    }
    TestInstance *createInstance(Context &context) const override;
    virtual ~ShaderAbortCase() = default;
    virtual void checkSupport(Context &context) const override;
    void initPrograms(vk::SourceCollections &sourceCollections) const override;

private:
    static std::string constantDataWords(const std::string &bytes);

    const TestParams m_params;
};

TestInstance *ShaderAbortCase::createInstance(Context &context) const
{
    return new ShaderAbortInstance(context, m_params);
}

std::string ShaderAbortCase::constantDataWords(const std::string &bytes)
{
    std::ostringstream out;
    const size_t wordCount = (bytes.size() + 3) / sizeof(uint32_t);
    for (size_t w = 0; w < wordCount; ++w)
    {
        uint32_t word = 0;
        for (size_t b = 0; b < sizeof(uint32_t); ++b)
        {
            const size_t idx = w * sizeof(uint32_t) + b;
            if (idx < bytes.size())
                word |= static_cast<uint32_t>(static_cast<uint8_t>(bytes[idx])) << (8u * b);
        }
        out << (w ? " " : "") << word;
    }
    return out.str();
}

void ShaderAbortCase::initPrograms(SourceCollections &sourceCollections) const
{
    std::ostringstream src;
    switch (m_params.type)
    {
    case TestType::TERMINATION:
    {
        // Basic termination test with formatted message
        src << "#version 450\n"
            << "#extension GL_EXT_abort : require\n"
            << "layout(local_size_x = 1) in;\n"
            << "\n"
            << "layout(push_constant) uniform PushConstants {\n"
            << "    uint maxIterations;\n"
            << "};\n"
            << "\n"
            << "layout(set = 0, binding = 0) buffer Output {\n"
            << "    uint value;\n"
            << "} outputBuffer;\n"
            << "\n"
            << "void main() {\n"
            << "    for (uint i = 0; i < maxIterations; i++) {\n"
            << "        if (i >= 3) {\n"
            << "            abortEXT(\"Iteration exceeded limit: %u in invocation %u\", i, gl_GlobalInvocationID.x);\n"
            << "        }\n"
            << "    }\n"
            << "    outputBuffer.value = 1;\n"
            << "}\n";
        break;
    }
    case TestType::EMPTY_MESSAGE:
    {
        // Empty message variant of the basic termination test
        src << "#version 450\n"
            << "#extension GL_EXT_abort : require\n"
            << "layout(local_size_x = 1) in;\n"
            << "\n"
            << "layout(push_constant) uniform PushConstants {\n"
            << "    uint maxIterations;\n"
            << "};\n"
            << "\n"
            << "layout(set = 0, binding = 0) buffer Output {\n"
            << "    uint value;\n"
            << "} outputBuffer;\n"
            << "\n"
            << "void main() {\n"
            << "    for (uint i = 0; i < maxIterations; i++) {\n"
            << "        if (i >= 3) {\n"
            << "            abortEXT();\n"
            << "        }\n"
            << "    }\n"
            << "    outputBuffer.value = 1;\n"
            << "}\n";
        break;
    }
    case TestType::LARGE_CONST_MESSAGE:
    {
        // 1 KB constant payload variant of LARGE_BUFFER_MESSAGE, embedding
        // the whole message as compile-time data via OpConstantDataKHR

        const uint32_t lengthPrefixSize = 8;
        const uint32_t formatStringSize = 48;
        const uint32_t numElements = (m_params.messageSize - lengthPrefixSize - formatStringSize) / sizeof(uint32_t);

        // Fill every element with the 0xDEADBEEF marker
        std::string dataBytes;
        dataBytes.reserve(static_cast<size_t>(numElements) * sizeof(uint32_t));
        for (uint32_t i = 0; i < numElements; ++i)
        {
            dataBytes.push_back(static_cast<char>(0xEFu));
            dataBytes.push_back(static_cast<char>(0xBEu));
            dataBytes.push_back(static_cast<char>(0xADu));
            dataBytes.push_back(static_cast<char>(0xDEu));
        }

        std::string formatBytes("Large constant abort message payload:");
        formatBytes.resize(formatStringSize, '\0');

        std::ostringstream spirv;
        spirv << "; SPIR-V\n"
              << "; Version: 1.0\n"
              << "; Generator: Hand-written\n"
              << "; Schema: 0\n"
              << "OpCapability Shader\n"
              << "OpCapability Int8\n"
              << "OpCapability AbortKHR\n"
              << "OpCapability ConstantDataKHR\n"
              << "OpExtension \"SPV_KHR_abort\"\n"
              << "OpExtension \"SPV_KHR_constant_data\"\n"
              << "%glsl = OpExtInstImport \"GLSL.std.450\"\n"
              << "OpMemoryModel Logical GLSL450\n"
              << "OpEntryPoint GLCompute %main \"main\"\n"
              << "OpExecutionMode %main LocalSize 1 1 1\n"
              << "OpDecorate %fmt_arr UTFEncodedKHR\n"
              << "OpDecorate %fmt_arr ArrayStride 1\n"
              << "OpDecorate %data_arr ArrayStride 4\n"
              << "OpDecorate %fmt_arr_no_stride UTFEncodedKHR\n"
              << "OpMemberDecorate %AbortMsgLayout 0 Offset 0\n"
              << "OpMemberDecorate %AbortMsgLayout 1 Offset " << formatStringSize << "\n"
              << "OpDecorate %OutBuf BufferBlock\n"
              << "OpMemberDecorate %OutBuf 0 Offset 0\n"
              << "OpDecorate %outputBuffer Binding 0\n"
              << "OpDecorate %outputBuffer DescriptorSet 0\n"
              << "%void = OpTypeVoid\n"
              << "%char = OpTypeInt 8 1\n"
              << "%uint = OpTypeInt 32 0\n"
              << "%int = OpTypeInt 32 1\n"
              << "%uint_1 = OpConstant %uint 1\n"
              << "%int_0 = OpConstant %int 0\n"
              << "%uint_" << formatStringSize << " = OpConstant %uint " << formatStringSize << "\n"
              << "%uint_" << numElements << " = OpConstant %uint " << numElements << "\n"
              << "%fmt_arr = OpTypeArray %char %uint_" << formatStringSize << "\n"
              << "%data_arr = OpTypeArray %uint %uint_" << numElements << "\n"
              << "%fmt_arr_no_stride = OpTypeArray %char %uint_" << formatStringSize << "\n"
              << "%data_arr_no_stride = OpTypeArray %uint %uint_" << numElements << "\n"
              << "%fmt_str = OpConstantDataKHR %fmt_arr_no_stride " << constantDataWords(formatBytes) << "\n"
              << "%data_const = OpConstantDataKHR %data_arr_no_stride " << constantDataWords(dataBytes) << "\n"
              << "%AbortMsg = OpTypeStruct %fmt_arr_no_stride %data_arr_no_stride\n"
              << "%AbortMsgLayout = OpTypeStruct %fmt_arr %data_arr\n"
              << "%OutBuf = OpTypeStruct %uint\n"
              << "%ptr_Uniform_OutBuf = OpTypePointer Uniform %OutBuf\n"
              << "%ptr_Uniform_uint = OpTypePointer Uniform %uint\n"
              << "%outputBuffer = OpVariable %ptr_Uniform_OutBuf Uniform\n"
              << "%void_func = OpTypeFunction %void\n"
              << "%main = OpFunction %void None %void_func\n"
              << "%entry = OpLabel\n"
              << "%msg = OpCompositeConstruct %AbortMsg %fmt_str %data_const\n"
              << "OpAbortKHR %AbortMsgLayout %msg\n"
              << "OpFunctionEnd\n";

        const SpirvVersion baselineSpirvVersion = getBaselineSpirvVersion(sourceCollections.usedVulkanVersion);
        SpirVAsmBuildOptions buildOptions(sourceCollections.usedVulkanVersion, baselineSpirvVersion);

        sourceCollections.spirvAsmSources.add("comp", &buildOptions) << spirv.str();
        return;
    }
    case TestType::PIPELINE_CACHE:
    case TestType::PIPELINE_BINARY:
    case TestType::SHADER_OBJECT:
    {
        // Minimal abort shader shared by the pipeline cache, pipeline binary and shader object cases
        // None of these flows executes the shader or inspects the abort payload
        // the shaders only job is to embed an OpAbortKHR in the pipeline
        src << "#version 450\n"
            << "#extension GL_EXT_abort : require\n"
            << "layout(local_size_x = 1) in;\n"
            << "\n"
            << "layout(set = 0, binding = 0) buffer Output {\n"
            << "    uint value;\n"
            << "} outputBuffer;\n"
            << "\n"
            << "void main() {\n"
            << "    abortEXT(\"Pipeline cache test\");\n"
            << "    outputBuffer.value = 1;\n"
            << "}\n";
        break;
    }
    case TestType::LARGE_BUFFER_MESSAGE:
    {
        // Test that aborts handle large data copies from a uniform buffer (~64KiB payload)

        const uint32_t lengthPrefixSize = 8;
        const uint32_t formatStringSize = 48;
        const uint32_t numElements = (m_params.messageSize - lengthPrefixSize - formatStringSize) / sizeof(uint32_t);

        std::string formatBytes("Large buffer abort message - 64KiB payload:");
        formatBytes.resize(formatStringSize, '\0');

        std::ostringstream spirv;
        spirv << "; SPIR-V\n"
              << "; Version: 1.0\n"
              << "; Generator: Hand-written\n"
              << "; Bound: 30\n"
              << "; Schema: 0\n"
              << "OpCapability Shader\n"
              << "OpCapability Int8\n"
              << "OpCapability AbortKHR\n"
              << "OpCapability ConstantDataKHR\n"
              << "OpExtension \"SPV_KHR_abort\"\n"
              << "OpExtension \"SPV_KHR_constant_data\"\n"
              << "%glsl = OpExtInstImport \"GLSL.std.450\"\n"
              << "OpMemoryModel Logical GLSL450\n"
              << "OpEntryPoint GLCompute %main \"main\"\n"
              << "OpExecutionMode %main LocalSize 1 1 1\n"
              << "OpDecorate %fmt_arr UTFEncodedKHR\n"
              << "OpDecorate %fmt_arr ArrayStride 1\n"
              << "OpDecorate %fmt_arr_no_stride UTFEncodedKHR\n"
              << "OpDecorate %data_arr ArrayStride 4\n"
              << "OpDecorate %UniformBuf Block\n"
              << "OpMemberDecorate %UniformBuf 0 Offset 0\n"
              << "OpDecorate %uniformBuffer Binding 1\n"
              << "OpDecorate %uniformBuffer DescriptorSet 0\n"
              << "OpMemberDecorate %AbortMsgLayout 0 Offset 0\n"
              << "OpMemberDecorate %AbortMsgLayout 1 Offset " << formatStringSize << "\n"
              << "OpDecorate %OutBuf BufferBlock\n"
              << "OpMemberDecorate %OutBuf 0 Offset 0\n"
              << "OpDecorate %outputBuffer Binding 0\n"
              << "OpDecorate %outputBuffer DescriptorSet 0\n"
              << "%void = OpTypeVoid\n"
              << "%char = OpTypeInt 8 1\n"
              << "%uint = OpTypeInt 32 0\n"
              << "%int = OpTypeInt 32 1\n"
              << "%uint_0 = OpConstant %uint 0\n"
              << "%uint_1 = OpConstant %uint 1\n"
              << "%int_0 = OpConstant %int 0\n"
              << "%uint_" << formatStringSize << " = OpConstant %uint " << formatStringSize << "\n"
              << "%uint_" << numElements << " = OpConstant %uint " << numElements << "\n"
              << "%fmt_arr = OpTypeArray %char %uint_" << formatStringSize << "\n"
              << "%fmt_arr_no_stride = OpTypeArray %char %uint_" << formatStringSize << "\n"
              << "%data_arr = OpTypeArray %uint %uint_" << numElements << "\n"
              << "%fmt_str = OpConstantDataKHR %fmt_arr_no_stride " << constantDataWords(formatBytes) << "\n"
              << "%UniformBuf = OpTypeStruct %data_arr\n"
              << "%AbortMsg = OpTypeStruct %fmt_arr_no_stride %data_arr\n"
              << "%AbortMsgLayout = OpTypeStruct %fmt_arr %data_arr\n"
              << "%OutBuf = OpTypeStruct %uint\n"
              << "%ptr_Uniform_UniformBuf = OpTypePointer Uniform %UniformBuf\n"
              << "%ptr_Uniform_data_arr = OpTypePointer Uniform %data_arr\n"
              << "%ptr_Uniform_OutBuf = OpTypePointer Uniform %OutBuf\n"
              << "%ptr_Uniform_uint = OpTypePointer Uniform %uint\n"
              << "%uniformBuffer = OpVariable %ptr_Uniform_UniformBuf Uniform\n"
              << "%outputBuffer = OpVariable %ptr_Uniform_OutBuf Uniform\n"
              << "%void_func = OpTypeFunction %void\n"
              << "%main = OpFunction %void None %void_func\n"
              << "%entry = OpLabel\n"
              << "%arr_ptr = OpAccessChain %ptr_Uniform_data_arr %uniformBuffer %int_0\n"
              << "%arr_data = OpLoad %data_arr %arr_ptr\n"
              << "%msg = OpCompositeConstruct %AbortMsg %fmt_str %arr_data\n"
              << "OpAbortKHR %AbortMsgLayout %msg\n"
              << "OpFunctionEnd\n";

        const SpirvVersion baselineSpirvVersion = getBaselineSpirvVersion(sourceCollections.usedVulkanVersion);
        SpirVAsmBuildOptions buildOptions(sourceCollections.usedVulkanVersion, baselineSpirvVersion);
        SpirvValidatorOptions validatorOptions(sourceCollections.usedVulkanVersion,
                                               SpirvValidatorOptions::kScalarBlockLayout);
        buildOptions << validatorOptions;

        sourceCollections.spirvAsmSources.add("comp", &buildOptions) << spirv.str();
        return;
    }
    case TestType::MAX_MESSAGE_SIZE:
    {
        // Stress test maxShaderAbortMessageSize with a spec-constant-sized uniform buffer payload

        const uint32_t lengthPrefixSize   = 8;
        const uint32_t formatStringSize   = 48;
        const uint32_t defaultFrameSize   = 65536;
        const uint32_t defaultNumElements = (defaultFrameSize - lengthPrefixSize - formatStringSize) / sizeof(uint32_t);

        std::string formatBytes("MAX_MESSAGE_SIZE stress test - payload:");
        formatBytes.resize(formatStringSize, '\0');

        std::ostringstream spirv;
        spirv << "; SPIR-V\n"
              << "; Version: 1.0\n"
              << "; Generator: Hand-written\n"
              << "; Schema: 0\n"
              << "OpCapability Shader\n"
              << "OpCapability Int8\n"
              << "OpCapability AbortKHR\n"
              << "OpCapability ConstantDataKHR\n"
              << "OpExtension \"SPV_KHR_abort\"\n"
              << "OpExtension \"SPV_KHR_constant_data\"\n"
              << "%glsl = OpExtInstImport \"GLSL.std.450\"\n"
              << "OpMemoryModel Logical GLSL450\n"
              << "OpEntryPoint GLCompute %main \"main\"\n"
              << "OpExecutionMode %main LocalSize 1 1 1\n"
              << "OpDecorate %numElements SpecId 0\n"
              << "OpDecorate %fmt_arr UTFEncodedKHR\n"
              << "OpDecorate %fmt_arr ArrayStride 1\n"
              << "OpDecorate %fmt_arr_no_stride UTFEncodedKHR\n"
              << "OpDecorate %data_arr ArrayStride 4\n"
              << "OpDecorate %DataBuf Block\n"
              << "OpMemberDecorate %DataBuf 0 Offset 0\n"
              << "OpDecorate %dataBuffer Binding 1\n"
              << "OpDecorate %dataBuffer DescriptorSet 0\n"
              << "OpMemberDecorate %AbortMsgLayout 0 Offset 0\n"
              << "OpMemberDecorate %AbortMsgLayout 1 Offset " << formatStringSize << "\n"
              << "OpDecorate %OutBuf BufferBlock\n"
              << "OpMemberDecorate %OutBuf 0 Offset 0\n"
              << "OpDecorate %outputBuffer Binding 0\n"
              << "OpDecorate %outputBuffer DescriptorSet 0\n"
              << "%void = OpTypeVoid\n"
              << "%char = OpTypeInt 8 1\n"
              << "%uint = OpTypeInt 32 0\n"
              << "%int = OpTypeInt 32 1\n"
              << "%uint_1 = OpConstant %uint 1\n"
              << "%int_0 = OpConstant %int 0\n"
              << "%uint_" << formatStringSize << " = OpConstant %uint " << formatStringSize << "\n"
              << "%numElements = OpSpecConstant %uint " << defaultNumElements << "\n"
              << "%fmt_arr = OpTypeArray %char %uint_" << formatStringSize << "\n"
              << "%fmt_arr_no_stride = OpTypeArray %char %uint_" << formatStringSize << "\n"
              << "%data_arr = OpTypeArray %uint %numElements\n"
              << "%fmt_str = OpConstantDataKHR %fmt_arr_no_stride " << constantDataWords(formatBytes) << "\n"
              << "%DataBuf = OpTypeStruct %data_arr\n"
              << "%AbortMsg = OpTypeStruct %fmt_arr_no_stride %data_arr\n"
              << "%AbortMsgLayout = OpTypeStruct %fmt_arr %data_arr\n"
              << "%OutBuf = OpTypeStruct %uint\n"
              << "%ptr_Uniform_DataBuf = OpTypePointer Uniform %DataBuf\n"
              << "%ptr_Uniform_data_arr = OpTypePointer Uniform %data_arr\n"
              << "%ptr_Uniform_OutBuf = OpTypePointer Uniform %OutBuf\n"
              << "%ptr_Uniform_uint = OpTypePointer Uniform %uint\n"
              << "%dataBuffer = OpVariable %ptr_Uniform_DataBuf Uniform\n"
              << "%outputBuffer = OpVariable %ptr_Uniform_OutBuf Uniform\n"
              << "%void_func = OpTypeFunction %void\n"
              << "%main = OpFunction %void None %void_func\n"
              << "%entry = OpLabel\n"
              << "%arr_ptr = OpAccessChain %ptr_Uniform_data_arr %dataBuffer %int_0\n"
              << "%arr_data = OpLoad %data_arr %arr_ptr\n"
              << "%msg = OpCompositeConstruct %AbortMsg %fmt_str %arr_data\n"
              << "OpAbortKHR %AbortMsgLayout %msg\n"
              << "OpFunctionEnd\n";

        const SpirvVersion baselineSpirvVersion = getBaselineSpirvVersion(sourceCollections.usedVulkanVersion);
        SpirVAsmBuildOptions buildOptions(sourceCollections.usedVulkanVersion, baselineSpirvVersion);
        SpirvValidatorOptions validatorOptions(sourceCollections.usedVulkanVersion,
                                               SpirvValidatorOptions::kScalarBlockLayout);
        buildOptions << validatorOptions;

        sourceCollections.spirvAsmSources.add("comp", &buildOptions) << spirv.str();
        return;
    }
    }

    sourceCollections.glslSources.add("comp") << glu::ComputeSource(src.str());
}

void ShaderAbortCase::checkSupport(Context &context) const
{
    context.requireDeviceFunctionality("VK_KHR_device_fault");
    context.requireDeviceFunctionality("VK_KHR_shader_abort");
    context.requireDeviceFunctionality("VK_KHR_shader_constant_data");

    VkPhysicalDeviceShaderAbortFeaturesKHR shaderAbortFeatures = initVulkanStructure();
    VkPhysicalDeviceShaderConstantDataFeaturesKHR shaderConstantDataFeatures =
        initVulkanStructure(&shaderAbortFeatures);
    VkPhysicalDeviceVulkan12Features vulkan12Features = initVulkanStructure(&shaderConstantDataFeatures);

    VkPhysicalDeviceFeatures2 deviceFeatures2 = initVulkanStructure();
    deviceFeatures2.pNext                     = &vulkan12Features;

    context.getInstanceInterface().getPhysicalDeviceFeatures2(context.getPhysicalDevice(), &deviceFeatures2);

    if (!shaderAbortFeatures.shaderAbort)
        TCU_THROW(NotSupportedError, "Shader abort feature not supported");

    if (!shaderConstantDataFeatures.shaderConstantData)
        TCU_THROW(NotSupportedError, "Shader constant data feature not supported");

    if (!vulkan12Features.shaderInt8)
        TCU_THROW(NotSupportedError, "shaderInt8 feature not supported");

    if (m_params.type == TestType::LARGE_BUFFER_MESSAGE)
    {
        context.requireDeviceFunctionality("VK_EXT_scalar_block_layout");

        const VkPhysicalDeviceProperties properties =
            getPhysicalDeviceProperties(context.getInstanceInterface(), context.getPhysicalDevice());

        if (properties.limits.maxUniformBufferRange < m_params.messageSize)
            TCU_THROW(NotSupportedError, "maxUniformBufferRange too small for test payload");

        if (!context.getScalarBlockLayoutFeatures().scalarBlockLayout)
            TCU_THROW(NotSupportedError, "scalarBlockLayout not supported");
    }
    else if (m_params.type == TestType::PIPELINE_BINARY)
    {
        context.requireDeviceFunctionality("VK_KHR_pipeline_binary");
        context.requireDeviceFunctionality("VK_KHR_maintenance5");
    }
    else if (m_params.type == TestType::SHADER_OBJECT)
    {
        context.requireDeviceFunctionality("VK_EXT_shader_object");
    }
    else if (m_params.type == TestType::MAX_MESSAGE_SIZE)
    {
        context.requireDeviceFunctionality("VK_EXT_scalar_block_layout");

        VkPhysicalDeviceShaderAbortPropertiesKHR shaderAbortProperties = initVulkanStructure();
        VkPhysicalDeviceProperties2 properties2                        = initVulkanStructure();
        properties2.pNext                                              = &shaderAbortProperties;

        context.getInstanceInterface().getPhysicalDeviceProperties2(context.getPhysicalDevice(), &properties2);

        const uint64_t frameOverhead = 8u + 48u; // length prefix + format string
        const uint64_t reportedSize  = shaderAbortProperties.maxShaderAbortMessageSize;
        const uint64_t payloadSize   = (reportedSize > frameOverhead) ? (reportedSize - frameOverhead) : 0u;

        if (properties2.properties.limits.maxUniformBufferRange < payloadSize)
            TCU_THROW(NotSupportedError, "maxUniformBufferRange too small for the abort message payload");

        if (!context.getScalarBlockLayoutFeatures().scalarBlockLayout)
            TCU_THROW(NotSupportedError, "scalarBlockLayout not supported");
    }
}

void ShaderAbortInstance::parseMessageComponents(const std::vector<uint8_t> &messageData, std::string &formatString,
                                                 std::vector<uint32_t> &arguments)
{
    if (messageData.empty())
        return;

    // Find the null terminator to separate format string from arguments
    size_t nullPos = messageData.size(); // Default to end if no null terminator found
    for (size_t i = 0; i < messageData.size(); i++)
    {
        if (messageData[i] == 0)
        {
            nullPos = i;
            break;
        }
    }

    // Extract format string
    if (nullPos > 0)
    {
        formatString = std::string(reinterpret_cast<const char *>(messageData.data()), nullPos);
    }

    if (nullPos >= messageData.size())
        return;

    // The variable arguments are 4-byte uints packed at the tail of the message
    // Any bytes between the null terminator and the first argument are scalar alignment
    const size_t massageTail = messageData.size() - (nullPos + 1);
    const size_t argCount    = massageTail / sizeof(uint32_t);
    size_t argOffset         = messageData.size() - argCount * sizeof(uint32_t);

    for (size_t i = 0; i < argCount; ++i)
    {
        uint32_t arg;
        std::memcpy(&arg, &messageData[argOffset], sizeof(uint32_t));
        arguments.push_back(arg);
        argOffset += sizeof(uint32_t);
    }
}

void ShaderAbortInstance::verifyAbortMessage(const std::vector<uint8_t> &messageData)
{
    tcu::TestLog &log = m_context.getTestContext().getLog();

    const auto formatAbortMessage = [](const std::string &fmt, const std::vector<uint32_t> &args) -> std::string
    {
        std::ostringstream out;
        size_t argIdx = 0;

        for (size_t i = 0; i < fmt.size(); ++i)
        {
            if (fmt[i] != '%' || i + 1 == fmt.size())
            {
                out << fmt[i];
                continue;
            }

            const char spec = fmt[++i];
            if (spec == '%')
                out << '%';
            else if (spec == 'u' && argIdx < args.size())
                out << args[argIdx++];
            else
                out << '%' << spec;
        }

        return out.str();
    };

    // Parse messages
    std::vector<std::vector<uint8_t>> messages;
    size_t offset = 0;

    log << tcu::TestLog::Message << "Parsing abort fault buffer: " << messageData.size() << " bytes total"
        << tcu::TestLog::EndMessage;

    while (offset < messageData.size())
    {
        if (offset + sizeof(uint64_t) > messageData.size())
        {
            log << tcu::TestLog::Message << "Stopped parsing at offset " << offset << ": "
                << (messageData.size() - offset) << " trailing byte(s), too few for an 8-byte length header"
                << tcu::TestLog::EndMessage;
            break;
        }

        uint64_t messageLength = *reinterpret_cast<const uint64_t *>(&messageData[offset]);
        offset += sizeof(uint64_t);

        // Read message payload
        if (offset + static_cast<size_t>(messageLength) > messageData.size())
        {
            log << tcu::TestLog::Message << "Stopped parsing at offset " << offset << ": message claims length "
                << messageLength << " but only " << (messageData.size() - offset) << " byte(s) remain in the "
                << messageData.size() << "-byte fault buffer" << tcu::TestLog::EndMessage;
            break;
        }

        const size_t msgLen = static_cast<size_t>(messageLength);
        std::vector<uint8_t> message(msgLen);
        if (msgLen > 0)
            std::memcpy(message.data(), messageData.data() + offset, msgLen);
        messages.push_back(message);
        offset += msgLen;

        log << tcu::TestLog::Message << "Parsed abort message " << (messages.size() - 1) << ": " << msgLen
            << " byte payload" << tcu::TestLog::EndMessage;

        // Align offset to 8 bytes for the next message
        offset = (offset + 7) & ~static_cast<size_t>(7);
    }

    log << tcu::TestLog::Message << "Received " << messages.size() << " abort messages" << tcu::TestLog::EndMessage;

    switch (m_params.type)
    {
    case TestType::TERMINATION:
    {
        // For basic termination test, verify that all messages have correct format and iteration count >= 3
        if (messages.empty())
            TCU_FAIL("No abort message received");

        for (size_t msgIdx = 0; msgIdx < messages.size(); msgIdx++)
        {
            std::string formatString;
            std::vector<uint32_t> arguments;
            parseMessageComponents(messages[msgIdx], formatString, arguments);

            // Verify we have the expected number of arguments
            if (arguments.size() != 2)
                TCU_FAIL("Expected exactly 2 arguments in abort message");

            uint32_t iteration    = arguments[0];
            uint32_t invocationId = arguments[1];

            if (iteration < 3)
                TCU_FAIL("Unexpected iteration count in abort message");

            if (invocationId >= m_params.invocationCount)
                TCU_FAIL("Invocation ID out of range");

            const std::string formatted    = formatAbortMessage(formatString, arguments);
            const std::string expectedText = "Iteration exceeded limit: " + std::to_string(iteration) +
                                             " in invocation " + std::to_string(invocationId);

            if (formatted != expectedText)
                TCU_FAIL("Formatted abort message does not match expected text");

            log << tcu::TestLog::Message << "Verified abort message[" << msgIdx << "] - raw: \"" << formatString
                << "\", formatted: \"" << formatted << "\"" << tcu::TestLog::EndMessage;
        }
        break;
    }

    case TestType::EMPTY_MESSAGE:
    {
        // For empty message test, verify that all messages are empty or contain only an empty format string
        if (messages.empty())
            TCU_FAIL("No abort message received");

        for (size_t msgIdx = 0; msgIdx < messages.size(); msgIdx++)
        {
            // An empty abortEXT() produces a single null format string padded to
            // 4 bytes and no arguments, so the payload is at most 4 all-zero bytes.
            if (messages[msgIdx].size() > sizeof(uint32_t))
                TCU_FAIL("Expected empty or minimal message");

            for (const uint8_t byte : messages[msgIdx])
                if (byte != 0)
                    TCU_FAIL("Expected empty (all-zero) abort message");

            log << tcu::TestLog::Message << "Verified empty abort message[" << msgIdx << "]"
                << tcu::TestLog::EndMessage;
        }
        break;
    }

    case TestType::LARGE_BUFFER_MESSAGE:
    {
        // The abort payload is the whole uniform buffer captured at abort time
        // A copy scheduled after the dispatch overwrites the buffer with the staging pattern
        // the retrieved message must still hold the pre-copy values (data[i] == i)
        if (messages.empty())
            TCU_FAIL("No abort message received");

        const size_t lengthPrefixSize = 8;
        const size_t formatStringSize = 48;
        const size_t numElements      = (m_params.messageSize - lengthPrefixSize - formatStringSize) / sizeof(uint32_t);

        const std::vector<uint8_t> &mainMessage = messages[0];

        // The payload must exactly match the size the shader generated.
        const size_t expectedSize = formatStringSize + numElements * sizeof(uint32_t);
        if (mainMessage.size() != expectedSize)
        {
            std::ostringstream err;
            err << "Abort message size (" << mainMessage.size() << ") does not match the expected payload size ("
                << expectedSize << ")";
            TCU_FAIL(err.str());
        }

        // Verify the format string was delivered intact
        const std::string expectedFormat = "Large buffer abort message - 64KiB payload:";
        const std::string formatString(reinterpret_cast<const char *>(mainMessage.data()), expectedFormat.size());
        if (formatString != expectedFormat || mainMessage[expectedFormat.size()] != 0)
            TCU_FAIL("Abort message format string was not delivered intact");

        // Verify every element holds the pre-copy pattern (data[i] == i)
        for (size_t i = 0; i < numElements; ++i)
        {
            uint32_t value;
            std::memcpy(&value, &mainMessage[formatStringSize + i * sizeof(uint32_t)], sizeof(uint32_t));
            if (value != static_cast<uint32_t>(i))
            {
                std::ostringstream err;
                err << "Abort message element " << i << " is " << value << ", expected " << i
                    << " - the post-dispatch buffer copy leaked into the captured payload";
                TCU_FAIL(err.str());
            }
        }

        log << tcu::TestLog::Message << "Verified 64KiB buffer abort payload (" << mainMessage.size()
            << " bytes) captured the pre-copy data" << tcu::TestLog::EndMessage;
        break;
    }

    case TestType::PIPELINE_CACHE:
    case TestType::PIPELINE_BINARY:
    case TestType::SHADER_OBJECT:
        // These cases never reach verifyAbortMessage routes them to their own iterate () methods
        // which do not dispatch the shader
        break;

    case TestType::LARGE_CONST_MESSAGE:
    {
        // Verifies the full abort payload landed in the fault buffer intact
        //  Format string followed by numElements copies of the 0xDEADBEEF marker embedded via OpConstantDataKHR
        if (messages.empty())
            TCU_FAIL("No abort message received");

        const size_t lengthPrefixSize = 8;
        const size_t formatStringSize = 48;
        const size_t numElements      = (m_params.messageSize - lengthPrefixSize - formatStringSize) / sizeof(uint32_t);

        const std::vector<uint8_t> &mainMessage = messages[0];

        const size_t expectedSize = formatStringSize + numElements * sizeof(uint32_t);
        if (mainMessage.size() != expectedSize)
        {
            std::ostringstream err;
            err << "Abort message size (" << mainMessage.size() << ") does not match the expected payload size ("
                << expectedSize << ")";
            TCU_FAIL(err.str());
        }

        const std::string expectedFormat = "Large constant abort message payload:";
        const std::string formatString(reinterpret_cast<const char *>(mainMessage.data()), expectedFormat.size());
        if (formatString != expectedFormat || mainMessage[expectedFormat.size()] != 0)
            TCU_FAIL("Abort message format string was not delivered intact");

        for (size_t i = 0; i < numElements; ++i)
        {
            uint32_t value;
            std::memcpy(&value, &mainMessage[formatStringSize + i * sizeof(uint32_t)], sizeof(uint32_t));

            if (value != 0xDEADBEEFu)
            {
                std::ostringstream err;
                err << "Abort message constant element " << i << " is 0x" << std::hex << value
                    << ", expected 0xDEADBEEF - OpConstantDataKHR payload was not delivered intact";
                TCU_FAIL(err.str());
            }
        }

        log << tcu::TestLog::Message << "Verified OpConstantDataKHR abort payload (" << mainMessage.size()
            << " bytes) was delivered intact" << tcu::TestLog::EndMessage;
        break;
    }

    case TestType::MAX_MESSAGE_SIZE:
    {
        // The abort message argument array was sized so the whole fault buffer frame fills
        // the device-reported maxShaderAbortMessageSize
        if (messages.empty())
            TCU_FAIL("No abort message received");

        const std::vector<uint8_t> &mainMessage = messages[0];

        const size_t lengthPrefixSize = 8;
        const size_t formatStringSize = 48;
        const size_t numElements =
            static_cast<size_t>((m_maxShaderAbortMessageSize - lengthPrefixSize - formatStringSize) / sizeof(uint32_t));
        const size_t expectedSize = formatStringSize + numElements * sizeof(uint32_t);

        // The payload must exactly match the size the shader generated
        if (mainMessage.size() != expectedSize)
        {
            std::ostringstream err;
            err << "Abort message size (" << mainMessage.size() << ") does not match the expected payload size ("
                << expectedSize << ") - the message buffer was not filled exactly";
            TCU_FAIL(err.str());
        }

        // The whole frame (8-byte length prefix + payload) must fit within the
        // implementations reported maxShaderAbortMessageSize
        const uint64_t frameSize = static_cast<uint64_t>(lengthPrefixSize) + mainMessage.size();
        if (frameSize > m_maxShaderAbortMessageSize)
        {
            std::ostringstream err;
            err << "Abort message frame size (" << frameSize << " = " << lengthPrefixSize << "-byte length prefix + "
                << mainMessage.size() << "-byte payload) exceeds maxShaderAbortMessageSize ("
                << m_maxShaderAbortMessageSize << ")";
            TCU_FAIL(err.str());
        }

        // Verify the format string was delivered intact
        const std::string expectedFormat = "MAX_MESSAGE_SIZE stress test - payload:";
        const std::string formatString(reinterpret_cast<const char *>(mainMessage.data()), expectedFormat.size());
        if (formatString != expectedFormat || mainMessage[expectedFormat.size()] != 0)
            TCU_FAIL("Abort message format string was not delivered intact");

        // Verify every argument was filled out, argument i must equal its buffer index
        for (size_t i = 0; i < numElements; ++i)
        {
            uint32_t arg;
            std::memcpy(&arg, &mainMessage[formatStringSize + i * sizeof(uint32_t)], sizeof(uint32_t));
            if (arg != static_cast<uint32_t>(i))
            {
                std::ostringstream err;
                err << "Abort message argument " << i << " is " << arg << ", expected " << i
                    << " - the message buffer was not fully filled";
                TCU_FAIL(err.str());
            }
        }

        log << tcu::TestLog::Message << "Verified abort message filled the frame (" << frameSize
            << " bytes = " << lengthPrefixSize << "-byte length prefix + " << mainMessage.size()
            << "-byte payload, maxShaderAbortMessageSize = " << m_maxShaderAbortMessageSize << " bytes)"
            << tcu::TestLog::EndMessage;
        break;
    }

    default:
        TCU_FAIL("Unknown test type");
    }
}

bool ShaderAbortInstance::checkDeviceFault(const VkDevice device, const DeviceInterface &deviceInterface)
{

    VkDeviceFaultShaderAbortMessageInfoKHR abortInfo = initVulkanStructure();
    abortInfo.messageDataSize                        = 0;
    abortInfo.pMessageData                           = nullptr;

    VkDeviceFaultDebugInfoKHR debugInfo = initVulkanStructure();
    debugInfo.pNext                     = &abortInfo;

    deviceInterface.getDeviceFaultDebugInfoKHR(device, &debugInfo);

    if (abortInfo.messageDataSize == 0)
        return false;

    std::vector<uint8_t> messageData(static_cast<size_t>(abortInfo.messageDataSize));
    abortInfo.pMessageData = messageData.data();

    deviceInterface.getDeviceFaultDebugInfoKHR(device, &debugInfo);

    verifyAbortMessage(messageData);

    return true;
}

tcu::TestStatus ShaderAbortInstance::iteratePipelineCache()
{
    const std::vector<const char *> extensions = {"VK_KHR_shader_abort", "VK_KHR_device_fault",
                                                  "VK_KHR_shader_constant_data"};

    VkPhysicalDeviceFaultFeaturesKHR deviceFaultFeatures       = initVulkanStructure();
    deviceFaultFeatures.deviceFault                            = VK_TRUE;
    VkPhysicalDeviceShaderAbortFeaturesKHR shaderAbortFeatures = initVulkanStructure(&deviceFaultFeatures);
    shaderAbortFeatures.shaderAbort                            = VK_TRUE;
    VkPhysicalDeviceShaderConstantDataFeaturesKHR shaderConstantDataFeatures =
        initVulkanStructure(&shaderAbortFeatures);
    shaderConstantDataFeatures.shaderConstantData     = VK_TRUE;
    VkPhysicalDeviceVulkan12Features vulkan12Features = initVulkanStructure(&shaderConstantDataFeatures);
    vulkan12Features.shaderInt8                       = VK_TRUE;
    VkPhysicalDeviceFeatures2 features2               = initVulkanStructure();
    features2.pNext                                   = &vulkan12Features;

    auto buildOnFreshDevice = [&](const VkPipelineCacheCreateInfo &cacheCreateInfo, std::vector<uint8_t> &outCacheBlob)
    {
        const InstanceWrapper instance(m_context);
        const DeviceWrapper logicalDevice(createPostmortemDevice(m_context, instance, extensions, &features2));
        const DeviceInterface &vk = logicalDevice.getDriver();
        const VkDevice device     = *logicalDevice;

        DescriptorSetLayoutBuilder dslBuilder;
        dslBuilder.addBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1u, VK_SHADER_STAGE_COMPUTE_BIT, nullptr);
        const Unique<VkDescriptorSetLayout> dsl(dslBuilder.build(vk, device));

        const Unique<VkPipelineLayout> pipelineLayout(makePipelineLayout(vk, device, *dsl));
        const Unique<VkShaderModule> shaderModule(
            createShaderModule(vk, device, m_context.getBinaryCollection().get("comp"), 0u));

        const Unique<VkPipelineCache> cache(createPipelineCache(vk, device, &cacheCreateInfo));
        const Unique<VkPipeline> pipeline(
            makeComputePipeline(vk, device, *pipelineLayout, 0u, nullptr, *shaderModule, 0u, nullptr, *cache));

        size_t cacheSize = 0;
        VK_CHECK(vk.getPipelineCacheData(device, *cache, &cacheSize, nullptr));
        outCacheBlob.assign(cacheSize, 0u);
        if (cacheSize > 0)
            VK_CHECK(vk.getPipelineCacheData(device, *cache, &cacheSize, outCacheBlob.data()));
        outCacheBlob.resize(cacheSize);
    };

    const VkPipelineCacheCreateInfo emptyCacheInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO, // sType
        nullptr,                                      // pNext
        0u,                                           // flags
        0u,                                           // initialDataSize
        nullptr,                                      // pInitialData
    };
    std::vector<uint8_t> blobA;
    buildOnFreshDevice(emptyCacheInfo, blobA);

    auto &log = m_context.getTestContext().getLog();
    log << tcu::TestLog::Message << "Pipeline cache size after first build (empty cache, device A): " << blobA.size()
        << " bytes" << tcu::TestLog::EndMessage;

    const VkPipelineCacheCreateInfo prePopulatedCacheInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO,                      // sType
        nullptr,                                                           // pNext
        0u,                                                                // flags
        blobA.size(),                                                      // initialDataSize
        blobA.empty() ? nullptr : static_cast<const void *>(blobA.data()), // pInitialData
    };
    std::vector<uint8_t> blobB;
    buildOnFreshDevice(prePopulatedCacheInfo, blobB);

    log << tcu::TestLog::Message
        << "Pipeline cache size after second build (pre-populated cache, device B): " << blobB.size() << " bytes"
        << tcu::TestLog::EndMessage;

    if (blobB.size() > blobA.size())
    {
        std::ostringstream warn;
        warn << "Pipeline cache grew while rebuilding the same abort pipeline: " << blobA.size() << " -> "
             << blobB.size() << " bytes. A deterministic cache should not grow for identical pipelines.";
        return tcu::TestStatus(QP_TEST_RESULT_QUALITY_WARNING, warn.str());
    }

    return tcu::TestStatus::pass("Pass");
}

tcu::TestStatus ShaderAbortInstance::iteratePipelineBinary()
{
    // VK_KHR_pipeline_binary determinism check for a compute pipeline that contains OpAbortKHR
    const std::vector<const char *> extensions = {
        "VK_KHR_shader_abort",    "VK_KHR_device_fault",         "VK_KHR_maintenance5",
        "VK_KHR_pipeline_binary", "VK_KHR_shader_constant_data",
    };

    VkPhysicalDeviceFaultFeaturesKHR deviceFaultFeatures          = initVulkanStructure();
    deviceFaultFeatures.deviceFault                               = VK_TRUE;
    VkPhysicalDeviceShaderAbortFeaturesKHR shaderAbortFeatures    = initVulkanStructure(&deviceFaultFeatures);
    shaderAbortFeatures.shaderAbort                               = VK_TRUE;
    VkPhysicalDeviceMaintenance5FeaturesKHR maintenance5Features  = initVulkanStructure(&shaderAbortFeatures);
    maintenance5Features.maintenance5                             = VK_TRUE;
    VkPhysicalDevicePipelineBinaryFeaturesKHR pipelineBinaryFeats = initVulkanStructure(&maintenance5Features);
    pipelineBinaryFeats.pipelineBinaries                          = VK_TRUE;
    VkPhysicalDeviceShaderConstantDataFeaturesKHR shaderConstantDataFeatures =
        initVulkanStructure(&pipelineBinaryFeats);
    shaderConstantDataFeatures.shaderConstantData     = VK_TRUE;
    VkPhysicalDeviceVulkan12Features vulkan12Features = initVulkanStructure(&shaderConstantDataFeatures);
    vulkan12Features.shaderInt8                       = VK_TRUE;
    VkPhysicalDeviceFeatures2 features2               = initVulkanStructure();
    features2.pNext                                   = &vulkan12Features;

    auto extractOnFreshDevice = [&](std::vector<std::vector<uint8_t>> &outBlobs)
    {
        const InstanceWrapper instance(m_context);
        const DeviceWrapper logicalDevice(createPostmortemDevice(m_context, instance, extensions, &features2));
        const DeviceInterface &vk = logicalDevice.getDriver();
        const VkDevice device     = *logicalDevice;

        DescriptorSetLayoutBuilder dslBuilder;
        dslBuilder.addBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1u, VK_SHADER_STAGE_COMPUTE_BIT, nullptr);
        const Unique<VkDescriptorSetLayout> dsl(dslBuilder.build(vk, device));
        const Unique<VkPipelineLayout> pipelineLayout(makePipelineLayout(vk, device, *dsl));
        const Unique<VkShaderModule> shaderModule(
            createShaderModule(vk, device, m_context.getBinaryCollection().get("comp"), 0u));

        VkPipelineCreateFlags2CreateInfoKHR flags2 = initVulkanStructure();
        flags2.flags                               = VK_PIPELINE_CREATE_2_CAPTURE_DATA_BIT_KHR;

        VkComputePipelineCreateInfo pipelineCreateInfo = initVulkanStructure();
        pipelineCreateInfo.pNext                       = &flags2;
        pipelineCreateInfo.stage                       = initVulkanStructure();
        pipelineCreateInfo.stage.stage                 = VK_SHADER_STAGE_COMPUTE_BIT;
        pipelineCreateInfo.stage.pName                 = "main";
        pipelineCreateInfo.stage.module                = *shaderModule;
        pipelineCreateInfo.layout                      = *pipelineLayout;

        const Unique<VkPipeline> pipeline(createComputePipeline(vk, device, VK_NULL_HANDLE, &pipelineCreateInfo));

        PipelineBinaryWrapper wrapper(vk, device);
        VK_CHECK(wrapper.createPipelineBinariesFromPipeline(*pipeline));

        std::vector<VkPipelineBinaryDataKHR> dataInfo;
        wrapper.getPipelineBinaryData(dataInfo, outBlobs);
    };

    std::vector<std::vector<uint8_t>> blobsA;
    std::vector<std::vector<uint8_t>> blobsB;
    extractOnFreshDevice(blobsA);
    extractOnFreshDevice(blobsB);

    auto &log = m_context.getTestContext().getLog();
    log << tcu::TestLog::Message << "Pipeline binary count: device A = " << blobsA.size()
        << ", device B = " << blobsB.size() << tcu::TestLog::EndMessage;

    if (blobsA.size() != blobsB.size())
    {
        std::ostringstream warn;
        warn << "Pipeline binary count differs between fresh devices: " << blobsA.size() << " vs " << blobsB.size();
        return tcu::TestStatus(QP_TEST_RESULT_QUALITY_WARNING, warn.str());
    }

    for (size_t i = 0; i < blobsA.size(); ++i)
    {
        if (blobsA[i] != blobsB[i])
        {
            std::ostringstream warn;
            warn << "Pipeline binary [" << i << "] differs between fresh devices (sizes: " << blobsA[i].size() << " vs "
                 << blobsB[i].size() << ")";
            return tcu::TestStatus(QP_TEST_RESULT_QUALITY_WARNING, warn.str());
        }
    }

    return tcu::TestStatus::pass("Pass");
}

tcu::TestStatus ShaderAbortInstance::iterateShaderObject()
{
    // VK_EXT_shader_object determinism check for a compute shader that contains OpAbortKHR
    const std::vector<const char *> extensions = {
        "VK_KHR_shader_abort",
        "VK_KHR_device_fault",
        "VK_EXT_shader_object",
        "VK_KHR_shader_constant_data",
    };

    VkPhysicalDeviceFaultFeaturesKHR deviceFaultFeatures         = initVulkanStructure();
    deviceFaultFeatures.deviceFault                              = VK_TRUE;
    VkPhysicalDeviceShaderAbortFeaturesKHR shaderAbortFeatures   = initVulkanStructure(&deviceFaultFeatures);
    shaderAbortFeatures.shaderAbort                              = VK_TRUE;
    VkPhysicalDeviceShaderObjectFeaturesEXT shaderObjectFeatures = initVulkanStructure(&shaderAbortFeatures);
    shaderObjectFeatures.shaderObject                            = VK_TRUE;
    VkPhysicalDeviceShaderConstantDataFeaturesKHR shaderConstantDataFeatures =
        initVulkanStructure(&shaderObjectFeatures);
    shaderConstantDataFeatures.shaderConstantData     = VK_TRUE;
    VkPhysicalDeviceVulkan12Features vulkan12Features = initVulkanStructure(&shaderConstantDataFeatures);
    vulkan12Features.shaderInt8                       = VK_TRUE;
    VkPhysicalDeviceFeatures2 features2               = initVulkanStructure();
    features2.pNext                                   = &vulkan12Features;

    auto extractOnFreshDevice = [&](std::vector<uint8_t> &outBlob)
    {
        const InstanceWrapper instance(m_context);
        const DeviceWrapper logicalDevice(createPostmortemDevice(m_context, instance, extensions, &features2));
        const DeviceInterface &vk = logicalDevice.getDriver();
        const VkDevice device     = *logicalDevice;

        DescriptorSetLayoutBuilder dslBuilder;
        dslBuilder.addBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1u, VK_SHADER_STAGE_COMPUTE_BIT, nullptr);
        const Unique<VkDescriptorSetLayout> dsl(dslBuilder.build(vk, device));
        const VkDescriptorSetLayout dslHandle = *dsl;

        const ProgramBinary &binary = m_context.getBinaryCollection().get("comp");

        const VkShaderCreateInfoEXT shaderCreateInfo = {
            VK_STRUCTURE_TYPE_SHADER_CREATE_INFO_EXT, // sType
            nullptr,                                  // pNext
            0u,                                       // flags
            VK_SHADER_STAGE_COMPUTE_BIT,              // stage
            0u,                                       // nextStage
            VK_SHADER_CODE_TYPE_SPIRV_EXT,            // codeType
            binary.getSize(),                         // codeSize
            binary.getBinary(),                       // pCode
            "main",                                   // pName
            1u,                                       // setLayoutCount
            &dslHandle,                               // pSetLayouts
            0u,                                       // pushConstantRangeCount
            nullptr,                                  // pPushConstantRanges
            nullptr,                                  // pSpecializationInfo
        };

        VkShaderEXT shader = VK_NULL_HANDLE;
        VK_CHECK(vk.createShadersEXT(device, 1u, &shaderCreateInfo, nullptr, &shader));

        size_t dataSize = 0;
        VK_CHECK(vk.getShaderBinaryDataEXT(device, shader, &dataSize, nullptr));
        outBlob.assign(dataSize, 0u);
        if (dataSize > 0)
            VK_CHECK(vk.getShaderBinaryDataEXT(device, shader, &dataSize, outBlob.data()));
        outBlob.resize(dataSize);

        vk.destroyShaderEXT(device, shader, nullptr);
    };

    std::vector<uint8_t> blobA;
    std::vector<uint8_t> blobB;
    extractOnFreshDevice(blobA);
    extractOnFreshDevice(blobB);

    auto &log = m_context.getTestContext().getLog();
    log << tcu::TestLog::Message << "Shader object binary size: device A = " << blobA.size()
        << ", device B = " << blobB.size() << tcu::TestLog::EndMessage;

    if (blobA != blobB)
    {
        std::ostringstream err;
        err << "Shader object binary data differs between fresh devices (sizes: " << blobA.size() << " vs "
            << blobB.size() << ")";
        return tcu::TestStatus::fail(err.str());
    }

    return tcu::TestStatus::pass("Pass");
}

tcu::TestStatus ShaderAbortInstance::iterate()
{
    if (m_params.type == TestType::PIPELINE_CACHE)
        return iteratePipelineCache();
    if (m_params.type == TestType::PIPELINE_BINARY)
        return iteratePipelineBinary();
    if (m_params.type == TestType::SHADER_OBJECT)
        return iterateShaderObject();

    if (m_params.type == TestType::MAX_MESSAGE_SIZE)
    {
        VkPhysicalDeviceShaderAbortPropertiesKHR shaderAbortProperties = initVulkanStructure();
        VkPhysicalDeviceProperties2 properties2                        = initVulkanStructure();
        properties2.pNext                                              = &shaderAbortProperties;

        m_context.getInstanceInterface().getPhysicalDeviceProperties2(m_context.getPhysicalDevice(), &properties2);

        m_maxShaderAbortMessageSize = shaderAbortProperties.maxShaderAbortMessageSize;

        m_context.getTestContext().getLog()
            << tcu::TestLog::Message
            << "VkPhysicalDeviceShaderAbortPropertiesKHR::maxShaderAbortMessageSize = " << m_maxShaderAbortMessageSize
            << tcu::TestLog::EndMessage;

        // VK_KHR_shader_abort proposal: maxShaderAbortMessageSize must be at least 65536 bytes.
        if (m_maxShaderAbortMessageSize < 65536u)
        {
            std::ostringstream err;
            err << "maxShaderAbortMessageSize (" << m_maxShaderAbortMessageSize
                << ") is less than the spec minimum (65536 bytes)";
            return tcu::TestStatus::fail(err.str());
        }
    }

    uint32_t maxMsgNumElements = 0;
    if (m_params.type == TestType::MAX_MESSAGE_SIZE)
        maxMsgNumElements = static_cast<uint32_t>((m_maxShaderAbortMessageSize - 8u - 48u) / sizeof(uint32_t));

    std::vector<const char *> extensions = {"VK_KHR_shader_abort", "VK_KHR_device_fault",
                                            "VK_KHR_shader_constant_data"};

    VkPhysicalDeviceFaultFeaturesKHR deviceFaultFeatures = initVulkanStructure();
    deviceFaultFeatures.deviceFault                      = VK_TRUE;

    VkPhysicalDeviceShaderAbortFeaturesKHR shaderAbortFeatures = initVulkanStructure(&deviceFaultFeatures);
    shaderAbortFeatures.shaderAbort                            = VK_TRUE;

    VkPhysicalDeviceShaderConstantDataFeaturesKHR shaderConstantDataFeatures =
        initVulkanStructure(&shaderAbortFeatures);
    shaderConstantDataFeatures.shaderConstantData = VK_TRUE;

    void *pChainHead = &shaderConstantDataFeatures;

    VkPhysicalDeviceVulkan12Features vulkan12Features = initVulkanStructure(pChainHead);
    vulkan12Features.shaderInt8                       = VK_TRUE;
    if (m_params.type == TestType::LARGE_BUFFER_MESSAGE || m_params.type == TestType::MAX_MESSAGE_SIZE)
        vulkan12Features.scalarBlockLayout = VK_TRUE;
    pChainHead = &vulkan12Features;

    VkPhysicalDeviceFeatures2 features2 = initVulkanStructure();
    features2.pNext                     = pChainHead;

    const InstanceWrapper instance(m_context);
    const DeviceWrapper logicalDevice(createPostmortemDevice(m_context, instance, extensions, &features2));
    uint32_t queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();
    vk::VkQueue queue(getDeviceQueue(logicalDevice.getDriver(), *logicalDevice, queueFamilyIndex, 0));
    vk::Allocator &allocator = logicalDevice.getAllocator();

    const DeviceInterface &vk = logicalDevice.getDriver();
    const VkDevice device     = *logicalDevice;

    const VkDeviceSize outputBufferSize = sizeof(uint32_t);
    BufferWithMemory outputBuffer(vk, device, allocator,
                                  makeBufferCreateInfo(outputBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT),
                                  MemoryRequirement::HostVisible);

    // Test marker must survive abort
    // the post-abort store should never execute
    constexpr uint32_t kPostAbortMarker = 0xCAFEBABEu;
    {
        uint32_t *outputPtr = static_cast<uint32_t *>(outputBuffer.getAllocation().getHostPtr());
        *outputPtr          = kPostAbortMarker;
        flushAlloc(vk, device, outputBuffer.getAllocation());
    }

    // For LARGE_BUFFER_MESSAGE and MAX_MESSAGE_SIZE tests, create and fill the uniform buffer
    de::MovePtr<BufferWithMemory> uniformBuffer;
    // LARGE_BUFFER_MESSAGE also modifies the uniform buffer with a copy after the dispatch
    de::MovePtr<BufferWithMemory> stagingBuffer;

    if (m_params.type == TestType::LARGE_BUFFER_MESSAGE)
    {
        uniformBuffer = de::MovePtr<BufferWithMemory>(
            new BufferWithMemory(vk, device, allocator,
                                 makeBufferCreateInfo(m_params.messageSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
                                                                                VK_BUFFER_USAGE_TRANSFER_DST_BIT),
                                 MemoryRequirement::HostVisible));

        uint32_t *bufferData = static_cast<uint32_t *>(uniformBuffer->getAllocation().getHostPtr());
        for (uint32_t i = 0; i < m_params.messageSize / sizeof(uint32_t); i++)
            bufferData[i] = i;

        flushAlloc(vk, device, uniformBuffer->getAllocation());

        stagingBuffer = de::MovePtr<BufferWithMemory>(new BufferWithMemory(
            vk, device, allocator, makeBufferCreateInfo(m_params.messageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT),
            MemoryRequirement::HostVisible));

        uint32_t *stagingData = static_cast<uint32_t *>(stagingBuffer->getAllocation().getHostPtr());
        for (uint32_t i = 0; i < m_params.messageSize / sizeof(uint32_t); i++)
            stagingData[i] = 0xDEADBEEFu;

        flushAlloc(vk, device, stagingBuffer->getAllocation());
    }
    else if (m_params.type == TestType::MAX_MESSAGE_SIZE)
    {
        const VkDeviceSize bufferSize = static_cast<VkDeviceSize>(maxMsgNumElements) * sizeof(uint32_t);

        uniformBuffer = de::MovePtr<BufferWithMemory>(new BufferWithMemory(
            vk, device, allocator, makeBufferCreateInfo(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT),
            MemoryRequirement::HostVisible));

        // Fill buffer with the test pattern data[i] = i
        uint32_t *bufferData = static_cast<uint32_t *>(uniformBuffer->getAllocation().getHostPtr());
        for (uint32_t i = 0; i < maxMsgNumElements; i++)
            bufferData[i] = i;

        flushAlloc(vk, device, uniformBuffer->getAllocation());
    }

    DescriptorSetLayoutBuilder layoutBuilder;
    layoutBuilder.addBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1u, VK_SHADER_STAGE_COMPUTE_BIT, nullptr);

    DescriptorPoolBuilder poolBuilder;
    poolBuilder.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);

    if (m_params.type == TestType::LARGE_BUFFER_MESSAGE || m_params.type == TestType::MAX_MESSAGE_SIZE)
    {
        layoutBuilder.addBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1u, VK_SHADER_STAGE_COMPUTE_BIT, nullptr);
        poolBuilder.addType(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    }

    const Unique<VkDescriptorSetLayout> descriptorSetLayout(layoutBuilder.build(vk, device));

    const Unique<VkDescriptorPool> descriptorPool(
        poolBuilder.build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u));

    const Unique<VkDescriptorSet> descriptorSet(makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout));

    const VkDescriptorBufferInfo outputBufferDescriptorInfo =
        makeDescriptorBufferInfo(outputBuffer.get(), 0ull, outputBufferSize);

    DescriptorSetUpdateBuilder updateBuilder;

    updateBuilder.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u),
                              VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &outputBufferDescriptorInfo);

    if (m_params.type == TestType::LARGE_BUFFER_MESSAGE)
    {
        const VkDescriptorBufferInfo uniformBufferDescriptorInfo =
            makeDescriptorBufferInfo(uniformBuffer->get(), 0ull, static_cast<VkDeviceSize>(m_params.messageSize));

        updateBuilder.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(1u),
                                  VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &uniformBufferDescriptorInfo);
    }
    else if (m_params.type == TestType::MAX_MESSAGE_SIZE)
    {
        const VkDeviceSize bufferSize = static_cast<VkDeviceSize>(maxMsgNumElements) * sizeof(uint32_t);

        const VkDescriptorBufferInfo uniformBufferDescriptorInfo =
            makeDescriptorBufferInfo(uniformBuffer->get(), 0ull, bufferSize);

        updateBuilder.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(1u),
                                  VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &uniformBufferDescriptorInfo);
    }

    updateBuilder.update(vk, device);

    const VkPushConstantRange pushConstantRange = {
        VK_SHADER_STAGE_COMPUTE_BIT, // stageFlags
        0u,                          // offset
        sizeof(uint32_t)             // size
    };

    const Unique<VkPipelineLayout> pipelineLayout(
        makePipelineLayout(vk, device, *descriptorSetLayout, &pushConstantRange));

    const Unique<VkShaderModule> shaderModule(
        createShaderModule(vk, device, m_context.getBinaryCollection().get("comp"), 0u));

    // MAX_MESSAGE_SIZE feeds the abort message array length to the shader via specialization constant
    const VkSpecializationMapEntry specMapEntry = {
        0u,               // constantID - matches "OpDecorate %numElements SpecId 0"
        0u,               // offset
        sizeof(uint32_t), // size
    };
    const VkSpecializationInfo specializationInfo = {
        1u,                 // mapEntryCount
        &specMapEntry,      // pMapEntries
        sizeof(uint32_t),   // dataSize
        &maxMsgNumElements, // pData
    };

    const Unique<VkPipeline> pipeline(
        (m_params.type == TestType::MAX_MESSAGE_SIZE) ?
            makeComputePipeline(vk, device, *pipelineLayout, 0u, nullptr, *shaderModule, 0u, &specializationInfo) :
            makeComputePipeline(vk, device, *pipelineLayout, *shaderModule));

    const Unique<VkCommandPool> cmdPool(makeCommandPool(vk, device, queueFamilyIndex));
    const Unique<VkCommandBuffer> cmdBuffer(
        allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

    beginCommandBuffer(vk, *cmdBuffer);

    vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipeline);
    vk.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayout, 0u, 1u, &*descriptorSet, 0u,
                             nullptr);

    if (m_params.type == TestType::TERMINATION || m_params.type == TestType::EMPTY_MESSAGE)
    {
        const uint32_t maxIterations = 4u;
        vk.cmdPushConstants(*cmdBuffer, *pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0u, sizeof(maxIterations),
                            &maxIterations);
    }

    vk.cmdDispatch(*cmdBuffer, m_params.invocationCount, 1u, 1u);

    if (m_params.type == TestType::LARGE_BUFFER_MESSAGE)
    {
        const VkBufferMemoryBarrier preCopyBarrier = makeBufferMemoryBarrier(
            VK_ACCESS_UNIFORM_READ_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, uniformBuffer->get(), 0ull, VK_WHOLE_SIZE);
        vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u,
                              nullptr, 1u, &preCopyBarrier, 0u, nullptr);

        const VkBufferCopy copyRegion = {0ull, 0ull, static_cast<VkDeviceSize>(m_params.messageSize)};
        vk.cmdCopyBuffer(*cmdBuffer, stagingBuffer->get(), uniformBuffer->get(), 1u, &copyRegion);
    }

    endCommandBuffer(vk, *cmdBuffer);

    const VkSubmitInfo submitInfo = {
        VK_STRUCTURE_TYPE_SUBMIT_INFO, // sType
        nullptr,                       // pNext
        0u,                            // waitSemaphoreCount
        nullptr,                       // pWaitSemaphores
        nullptr,                       // pWaitDstStageMask
        1u,                            // commandBufferCount
        &cmdBuffer.get(),              // pCommandBuffers
        0u,                            // signalSemaphoreCount
        nullptr                        // pSignalSemaphores
    };

    const Unique<VkFence> fence(createFence(vk, device));
    VK_CHECK(vk.queueSubmit(queue, 1u, &submitInfo, *fence));

    (void)vk.waitForFences(device, 1u, &*fence, VK_TRUE, ~0ull);

    if (m_params.type == TestType::TERMINATION || m_params.type == TestType::EMPTY_MESSAGE)
    {
        invalidateAlloc(vk, device, outputBuffer.getAllocation());
        const uint32_t observed = *static_cast<const uint32_t *>(outputBuffer.getAllocation().getHostPtr());

        if (observed == 1u)
            return tcu::TestStatus::fail(
                "Post-abort store executed: outputBuffer.value == 1, OpAbortKHR did not terminate the shader");
        if (observed != kPostAbortMarker)
        {
            std::ostringstream err;
            err << "outputBuffer.value clobbered to 0x" << std::hex << observed << " (expected marker 0x"
                << kPostAbortMarker << " preserved by aborted shader)";
            return tcu::TestStatus::fail(err.str());
        }
    }

    const VkSubmitInfo noOp = {
        VK_STRUCTURE_TYPE_SUBMIT_INFO, nullptr, 0u, nullptr, nullptr, 0u, nullptr, 0u, nullptr,
    };
    const VkResult probe = vk.queueSubmit(queue, 1u, &noOp, VK_NULL_HANDLE);

    if (probe != VK_ERROR_DEVICE_LOST)
        return tcu::TestStatus::fail("Device did not enter lost state after OpAbortKHR");

    if (!checkDeviceFault(device, vk))
        return tcu::TestStatus::fail("No shader abort message received");

    return tcu::TestStatus::pass("Pass");
}

} // anonymous namespace

tcu::TestCaseGroup *createShaderAbortTests(tcu::TestContext &testCtx, const std::string &name)
{
    de::MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(testCtx, name.c_str()));

    const struct
    {
        TestType type;
        const char *name;
        uint32_t invocCount;
        uint32_t msgSize;
    } tests[] = {{TestType::TERMINATION, "basic_termination", 4, 0},
                 {TestType::EMPTY_MESSAGE, "empty_message", 4, 0},
                 {TestType::LARGE_BUFFER_MESSAGE, "large_buffer", 2, 65536},
                 {TestType::LARGE_CONST_MESSAGE, "large_constant", 2, 1024},
                 {TestType::PIPELINE_CACHE, "pipeline_cache", 0, 0},
                 {TestType::PIPELINE_BINARY, "pipeline_binary", 0, 0},
                 {TestType::SHADER_OBJECT, "shader_object", 0, 0},
                 {TestType::MAX_MESSAGE_SIZE, "max_message_size", 1, 0}};

    for (const auto &test : tests)
    {
        const TestParams params = {test.type, test.invocCount, test.msgSize};
        group->addChild(new ShaderAbortCase(testCtx, test.name, params));
    }

    return group.release();
}

} // namespace postmortem
} // namespace vkt