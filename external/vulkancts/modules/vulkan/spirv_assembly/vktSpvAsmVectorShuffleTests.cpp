/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2019 Google LLC
 * Copyright (c) 2019 The Khronos Group Inc.
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

#include <string>

#include "vktTestGroupUtil.hpp"
#include "vktAmberTestCase.hpp"
#include "vktSpvAsmComputeShaderCase.hpp"
#include "vktSpvAsmComputeShaderTestUtil.hpp"
#include "vktSpvAsmVectorShuffleTests.hpp"

namespace vkt
{
namespace SpirVAssembly
{
namespace
{

void createTests(tcu::TestCaseGroup *tests, const char *data_dir)
{
#ifndef CTS_USES_VULKANSC
    tcu::TestContext &testCtx = tests->getTestContext();

    // Shader test files are saved in <path>/external/vulkancts/data/vulkan/amber/<data_dir>/<basename>.amber
    struct Case
    {
        const char *basename;
        std::vector<std::string> requirements;
    };
    const Case cases[] = {
        // OpVectorShuffle with indices including -1
        {"vector_shuffle", {"VariablePointerFeatures.variablePointers"}},
        {"long_vector_shuffle", {"VariablePointerFeatures.variablePointers", "ShaderLongVectorFeaturesEXT.longVector"}},
    };

    for (unsigned i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i)
    {
        std::string file = std::string(cases[i].basename) + ".amber";
        cts_amber::AmberTestCase *testCase =
            cts_amber::createAmberTestCase(testCtx, cases[i].basename, data_dir, file, cases[i].requirements);

        tests->addChild(testCase);
    }

    {
        ComputeShaderSpec spec;
        const std::vector<uint32_t> inputData  = {0x00018000u, 0u};
        const std::vector<uint32_t> outputData = {0x80000001u};

        spec.assembly = "OpCapability Shader\n"
                        "OpCapability Float16\n"
                        "OpMemoryModel Logical GLSL450\n"
                        "OpEntryPoint GLCompute %main \"main\"\n"
                        "OpExecutionMode %main LocalSize 1 1 1\n"
                        "OpDecorate %u32_arr ArrayStride 4\n"
                        "OpMemberDecorate %buf 0 Offset 0\n"
                        "OpDecorate %buf BufferBlock\n"
                        "OpDecorate %indata DescriptorSet 0\n"
                        "OpDecorate %indata Binding 0\n"
                        "OpDecorate %outdata DescriptorSet 0\n"
                        "OpDecorate %outdata Binding 1\n"
                        "%void = OpTypeVoid\n"
                        "%voidf = OpTypeFunction %void\n"
                        "%u32 = OpTypeInt 32 0\n"
                        "%f16 = OpTypeFloat 16\n"
                        "%v2f16 = OpTypeVector %f16 2\n"
                        "%u32_0 = OpConstant %u32 0\n"
                        "%u32_1 = OpConstant %u32 1\n"
                        "%u32_2 = OpConstant %u32 2\n"
                        "%undef = OpUndef %v2f16\n"
                        "%u32_arr = OpTypeRuntimeArray %u32\n"
                        "%buf = OpTypeStruct %u32_arr\n"
                        "%ptr_buf = OpTypePointer Uniform %buf\n"
                        "%ptr_u32 = OpTypePointer Uniform %u32\n"
                        "%indata = OpVariable %ptr_buf Uniform\n"
                        "%outdata = OpVariable %ptr_buf Uniform\n"
                        "%wg_arr = OpTypeArray %v2f16 %u32_2\n"
                        "%ptr_wg_arr = OpTypePointer Workgroup %wg_arr\n"
                        "%ptr_wg_v2f16 = OpTypePointer Workgroup %v2f16\n"
                        "%wg = OpVariable %ptr_wg_arr Workgroup\n"
                        "%main = OpFunction %void None %voidf\n"
                        "%label = OpLabel\n"
                        "%in0_p = OpAccessChain %ptr_u32 %indata %u32_0 %u32_0\n"
                        "%in1_p = OpAccessChain %ptr_u32 %indata %u32_0 %u32_1\n"
                        "%raw_u32 = OpLoad %u32 %in0_p\n"
                        "%idx = OpLoad %u32 %in1_p\n"
                        "%val = OpBitcast %v2f16 %raw_u32\n"
                        "%swz = OpVectorShuffle %v2f16 %val %undef 1 0\n"
                        "%wg_p0 = OpAccessChain %ptr_wg_v2f16 %wg %u32_0\n"
                        "OpStore %wg_p0 %swz\n"
                        "%wg_p1 = OpAccessChain %ptr_wg_v2f16 %wg %idx\n"
                        "%loaded = OpLoad %v2f16 %wg_p1\n"
                        "%res_u = OpBitcast %u32 %loaded\n"
                        "%out0_p = OpAccessChain %ptr_u32 %outdata %u32_0 %u32_0\n"
                        "OpStore %out0_p %res_u\n"
                        "OpReturn\n"
                        "OpFunctionEnd\n";

        spec.numWorkGroups = tcu::IVec3(1, 1, 1);
        spec.inputs.push_back(BufferSp(new Uint32Buffer(inputData)));
        spec.outputs.push_back(BufferSp(new Uint32Buffer(outputData)));
        spec.extensions.push_back("VK_KHR_shader_float16_int8");
        spec.requestedVulkanFeatures.extFloat16Int8.shaderFloat16 = true;

        tests->addChild(new SpvAsmComputeShaderCase(testCtx, "float16_denorm", spec));
    }
#else
    DE_UNREF(tests);
    DE_UNREF(data_dir);
#endif
}

} // namespace

tcu::TestCaseGroup *createVectorShuffleGroup(tcu::TestContext &testCtx)
{
    // Location of the Amber script files under the data/vulkan/amber source tree.
    const char *data_dir = "spirv_assembly/instruction/compute/vector_shuffle";
    return createTestGroup(testCtx, "vector_shuffle", createTests, data_dir);
}

} // namespace SpirVAssembly
} // namespace vkt
