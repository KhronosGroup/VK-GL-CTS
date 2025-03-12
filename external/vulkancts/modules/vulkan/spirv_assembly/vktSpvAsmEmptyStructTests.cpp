/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2020 The Khronos Group Inc.
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
 * \brief Test copying struct which contains an empty struct.
          Test pointer comparisons of empty struct members.
 *//*--------------------------------------------------------------------*/

#include "vktSpvAsmComputeShaderTestUtil.hpp"
#include "vktSpvAsmComputeShaderCase.hpp"
#include "vktSpvAsmEmptyStructTests.hpp"
#include "tcuStringTemplate.hpp"
#include "vktTestGroupUtil.hpp"
#include "vktSpvAsmUtils.hpp"

namespace vkt
{
namespace SpirVAssembly
{
namespace
{

bool verifyResult(const std::vector<Resource> &, const std::vector<AllocationSp> &outputAllocs,
                  const std::vector<Resource> &expectedOutputs, tcu::TestLog &)
{
    for (uint32_t outputNdx = 0; outputNdx < static_cast<uint32_t>(outputAllocs.size()); ++outputNdx)
    {
        std::vector<uint8_t> expectedBytes;
        expectedOutputs[outputNdx].getBytes(expectedBytes);

        const uint32_t itemCount = static_cast<uint32_t>(expectedBytes.size()) / 4u;
        const uint32_t *returned = static_cast<const uint32_t *>(outputAllocs[outputNdx]->getHostPtr());
        const uint32_t *expected = reinterpret_cast<const uint32_t *>(&expectedBytes.front());

        for (uint32_t i = 0; i < itemCount; ++i)
        {
            // skip items with 0 as this is used to mark empty structure
            if (expected[i] == 0)
                continue;
            if (expected[i] != returned[i])
                return false;
        }
    }
    return true;
}

void addCopyingComputeGroup(tcu::TestCaseGroup *group)
{
    const tcu::StringTemplate shaderTemplate(
        "OpCapability Shader\n"

        "OpExtension \"SPV_KHR_storage_buffer_storage_class\"\n"

        "OpMemoryModel Logical GLSL450\n"
        "OpEntryPoint GLCompute %main \"main\" %var_id\n"
        "OpExecutionMode %main LocalSize 1 1 1\n"

        "OpDecorate %var_id BuiltIn GlobalInvocationId\n"
        "OpDecorate %var_input Binding 0\n"
        "OpDecorate %var_input DescriptorSet 0\n"

        "OpDecorate %var_outdata Binding 1\n"
        "OpDecorate %var_outdata DescriptorSet 0\n"

        "OpMemberDecorate %type_container_struct 0 Offset 0\n"
        "OpMemberDecorate %type_container_struct 1 Offset ${OFFSET_1}\n"
        "OpMemberDecorate %type_container_struct 2 Offset ${OFFSET_2}\n"
        "OpMemberDecorate %type_container_struct 3 Offset ${OFFSET_3}\n"
        "OpDecorate %type_container_struct Block\n"

        + std::string(getComputeAsmCommonTypes()) +

        //struct EmptyStruct {};
        //struct ContainerStruct {
        //  int i;
        //  A a1;
        //  A a2;
        //  int j;
        //};
        //layout(set=, binding = ) buffer block B b;

        // types
        "%type_empty_struct = OpTypeStruct\n"
        "%type_container_struct = OpTypeStruct %i32 %type_empty_struct %type_empty_struct %i32\n"

        "%type_container_struct_ubo_ptr = OpTypePointer Uniform %type_container_struct\n"
        "%type_container_struct_ssbo_ptr = OpTypePointer StorageBuffer %type_container_struct\n"

        // variables
        "%var_id = OpVariable %uvec3ptr Input\n"
        "${VARIABLES}\n"

        // void main function
        "%main = OpFunction %void None %voidf\n"
        "%label = OpLabel\n"

        "${COPYING_METHOD}"

        "OpReturn\n"
        "OpFunctionEnd\n");

    struct BufferType
    {
        std::string name;
        VkDescriptorType descriptorType;
        std::vector<uint32_t> offsets;
        std::vector<int> input;
        std::vector<int> expectedOutput;
        std::string spirvVariables;
        std::string spirvCopyObject;

        BufferType(const std::string &name_, VkDescriptorType descriptorType_, const std::vector<uint32_t> &offsets_,
                   const std::vector<int> &input_, const std::vector<int> &expectedOutput_,
                   const std::string &spirvVariables_, const std::string &spirvCopyObject_)
            : name(name_)
            , descriptorType(descriptorType_)
            , offsets(offsets_)
            , input(input_)
            , expectedOutput(expectedOutput_)
            , spirvVariables(spirvVariables_)
            , spirvCopyObject(spirvCopyObject_)
        {
        }
    };
    const std::vector<BufferType> bufferTypes{
        {"ubo",
         VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,

         // structure decorated as Block for variable in Uniform storage class
         // must follow relaxed uniform buffer layout rules and be aligned to 16
         {0, 16, 32, 48},
         {2, 0, 0, 0, 3, 0, 0, 0, 5, 0, 0, 0, 7, 0, 0, 0},
         {2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 7, 0, 0, 0},

         "%var_input = OpVariable %type_container_struct_ubo_ptr Uniform\n"
         "%var_outdata = OpVariable %type_container_struct_ssbo_ptr StorageBuffer\n",

         "%input_copy = OpCopyObject %type_container_struct_ubo_ptr %var_input\n"},
        {"ssbo",
         VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,

         {0, 4, 8, 12},
         {2, 3, 5, 7},
         {2, 0, 0, 7},

         "%var_input = OpVariable %type_container_struct_ssbo_ptr StorageBuffer\n"
         "%var_outdata = OpVariable %type_container_struct_ssbo_ptr StorageBuffer\n",

         "%input_copy = OpCopyObject %type_container_struct_ssbo_ptr %var_input\n"}};

    struct CopyingMethod
    {
        std::string name;
        std::string spirvCopyCode;

        CopyingMethod(const std::string &name_, const std::string &spirvCopyCode_)
            : name(name_)
            , spirvCopyCode(spirvCopyCode_)
        {
        }
    };
    const std::vector<CopyingMethod> copyingMethods{{"copy_object",

                                                     "%result = OpLoad %type_container_struct %input_copy\n"
                                                     "OpStore %var_outdata %result\n"},
                                                    {"copy_memory",

                                                     "OpCopyMemory %var_outdata %var_input\n"}};

    for (const auto &bufferType : bufferTypes)
    {
        for (const auto &copyingMethod : copyingMethods)
        {
            std::string name = copyingMethod.name + "_" + bufferType.name;

            std::map<std::string, std::string> specializationMap{
                {"OFFSET_1", de::toString(bufferType.offsets[1])},
                {"OFFSET_2", de::toString(bufferType.offsets[2])},
                {"OFFSET_3", de::toString(bufferType.offsets[3])},
                {"VARIABLES", bufferType.spirvVariables},

                // NOTE: to simlify code spirvCopyObject is added also when OpCopyMemory is used
                {"COPYING_METHOD", bufferType.spirvCopyObject + copyingMethod.spirvCopyCode},
            };

            ComputeShaderSpec spec;
            spec.assembly      = shaderTemplate.specialize(specializationMap);
            spec.numWorkGroups = tcu::IVec3(1, 1, 1);
            spec.verifyIO      = verifyResult;
            spec.inputs.push_back(Resource(BufferSp(new Int32Buffer(bufferType.input)), bufferType.descriptorType));
            spec.outputs.push_back(Resource(BufferSp(new Int32Buffer(bufferType.expectedOutput))));
            group->addChild(new SpvAsmComputeShaderCase(group->getTestContext(), name.c_str(), spec));
        }
    }
}

void addPointerComparisionComputeGroup(tcu::TestCaseGroup *group)
{
    // NOTE: pointer comparison is possible only for StorageBuffer storage class

    std::string computeSource = "OpCapability Shader\n"
                                "OpCapability VariablePointersStorageBuffer\n"

                                "OpMemoryModel Logical GLSL450\n"
                                "OpEntryPoint GLCompute %main \"main\" %var_id %var_input %var_outdata\n"
                                "OpExecutionMode %main LocalSize 1 1 1\n"

                                "OpDecorate %var_id BuiltIn GlobalInvocationId\n"
                                "OpDecorate %var_input Binding 0\n"
                                "OpDecorate %var_input DescriptorSet 0\n"

                                "OpDecorate %var_outdata Binding 1\n"
                                "OpDecorate %var_outdata DescriptorSet 0\n"

                                "OpMemberDecorate %type_container_struct 0 Offset 0\n"
                                "OpMemberDecorate %type_container_struct 1 Offset 4\n"
                                "OpMemberDecorate %type_container_struct 2 Offset 8\n"
                                "OpMemberDecorate %type_container_struct 3 Offset 12\n"
                                "OpDecorate %type_container_struct Block\n"

                                "OpMemberDecorate %type_i32_struct 0 Offset 0\n"
                                "OpDecorate %type_i32_struct Block\n"

                                + std::string(getComputeAsmCommonTypes("StorageBuffer")) +

                                // struct EmptyStruct {};
                                // struct ContainerStruct {
                                //  int i;
                                //  A a1;
                                //  A a2;
                                //  int j;
                                //};
                                // layout(set=, binding = ) buffer block B b;

                                // types
                                "%type_empty_struct = "
                                "OpTypeStruct\n"
                                "%type_container_struct = OpTypeStruct %i32 "
                                "%type_empty_struct %type_empty_struct %i32\n"
                                "%type_i32_struct = OpTypeStruct %i32\n"

                                // constants
                                "%c_i32_0 = OpConstant "
                                "%i32 0\n"
                                "%c_i32_1 = OpConstant "
                                "%i32 1\n"
                                "%c_i32_2 = OpConstant "
                                "%i32 2\n"

                                "%type_container_struct_in_ptr = OpTypePointer StorageBuffer "
                                "%type_container_struct\n"
                                "%type_i32_struct_out_ptr = OpTypePointer StorageBuffer "
                                "%type_i32_struct\n"

                                "%type_func_struct_ptr_ptr = OpTypePointer "
                                "StorageBuffer %type_empty_struct\n"

                                // variables
                                "%var_id = OpVariable "
                                "%uvec3ptr Input\n"
                                "%var_input = "
                                "OpVariable %type_container_struct_in_ptr StorageBuffer\n"
                                "%var_outdata = OpVariable "
                                "%type_i32_struct_out_ptr StorageBuffer\n"

                                // void main function
                                "%main = "
                                "OpFunction %void None %voidf\n"
                                "%label = "
                                "OpLabel\n"

                                // compare pointers to empty structures
                                "%ptr_to_first = "
                                "OpAccessChain %type_func_struct_ptr_ptr %var_input %c_i32_1\n"
                                "%ptr_to_second = "
                                "OpAccessChain %type_func_struct_ptr_ptr %var_input %c_i32_2\n"
                                "%pointers_not_equal = OpPtrNotEqual %bool "
                                "%ptr_to_first %ptr_to_second\n"
                                "%result = OpSelect "
                                "%i32 %pointers_not_equal %c_i32_1 %c_i32_0\n"
                                "%outloc = "
                                "OpAccessChain %i32ptr %var_outdata %c_i32_0\n"
                                "OpStore %outloc %result\n"

                                "OpReturn\n"
                                "OpFunctionEnd\n";

    tcu::TestContext &testCtx       = group->getTestContext();
    std::vector<int> input          = {2, 3, 5, 7};
    std::vector<int> expectedOutput = {1};

    ComputeShaderSpec spec;
    spec.assembly                                                                  = computeSource;
    spec.numWorkGroups                                                             = tcu::IVec3(1, 1, 1);
    spec.spirvVersion                                                              = SPIRV_VERSION_1_4;
    spec.requestedVulkanFeatures.extVariablePointers.variablePointersStorageBuffer = true;
    spec.inputs.push_back(Resource(BufferSp(new Int32Buffer(input))));
    spec.outputs.push_back(Resource(BufferSp(new Int32Buffer(expectedOutput))));
    spec.extensions.push_back("VK_KHR_spirv_1_4");
    group->addChild(new SpvAsmComputeShaderCase(testCtx, "ssbo", spec));
}

void addFunctionArgumentReturnValueGroup(tcu::TestCaseGroup *group)
{
    const tcu::StringTemplate shaderTemplate(
        "      OpCapability Shader\n"
        " %1 = OpExtInstImport \"GLSL.std.450\"\n"
        "      OpMemoryModel Logical GLSL450\n"
        "      OpEntryPoint GLCompute %4 \"main\" %29 %42 %51 ${GLOBAL_VARIABLE} %79\n"
        "      OpExecutionMode %4 LocalSize 2 1 1\n"
        "      OpSource GLSL 460\n"
        "      OpDecorate %29 BuiltIn LocalInvocationId\n"
        "      OpMemberDecorate %40 0 Offset 0\n"
        "      OpMemberDecorate %40 1 Offset 4\n"
        "      OpMemberDecorate %40 2 Offset 8\n"
        "      OpDecorate %40 Block\n"
        "      OpDecorate %42 DescriptorSet 0\n"
        "      OpDecorate %42 Binding 1\n"
        "      OpMemberDecorate %49 0 Offset 0\n"
        "      OpDecorate %49 Block\n"
        "      OpDecorate %51 DescriptorSet 0\n"
        "      OpDecorate %51 Binding 0\n"
        "      OpMemberDecorate %77 0 Offset 0\n"
        "      OpMemberDecorate %77 1 Offset 4\n"
        "      OpMemberDecorate %77 2 Offset 8\n"
        "      OpDecorate %77 Block\n"
        "      OpDecorate %79 DescriptorSet 0\n"
        "      OpDecorate %79 Binding 2\n"
        "      OpDecorate %96 BuiltIn WorkgroupSize\n"
        " %2 = OpTypeVoid\n"
        " %3 = OpTypeFunction %2\n"
        " %7 = OpTypeStruct\n"
        " %8 = OpTypePointer Function %7\n"
        " %9 = OpTypeBool\n"
        "%10 = OpTypePointer Function %9\n"
        "%11 = OpTypeFunction %7 %8 %8 %10\n"
        "%26 = OpTypeInt 32 0\n"
        "%27 = OpTypeVector %26 3\n"
        "%28 = OpTypePointer Input %27\n"
        "%29 = OpVariable %28 Input\n"
        "%30 = OpConstant %26 0\n"
        "%31 = OpTypePointer Input %26\n"
        "%34 = OpConstant %26 2\n"
        "%39 = OpTypeStruct\n"
        "%40 = OpTypeStruct %26 %39 %26\n"
        "%41 = OpTypePointer StorageBuffer %40\n"
        "%42 = OpVariable %41 StorageBuffer\n"
        "%43 = OpTypeInt 32 1\n"
        "%44 = OpConstant %43 0\n"
        "%45 = OpConstant %26 1\n"
        "%46 = OpTypePointer StorageBuffer %26\n"
        "%48 = OpConstant %43 1\n"
        "%49 = OpTypeStruct %39\n"
        "%50 = OpTypePointer StorageBuffer %49\n"
        "%51 = OpVariable %50 StorageBuffer\n"

        "${VARIABLE_DEFINITION}\n"

        "%59 = OpTypePointer StorageBuffer %39\n"
        "%69 = OpConstant %43 2\n"
        "%77 = OpTypeStruct %26 %39 %26\n"
        "%78 = OpTypePointer StorageBuffer %77\n"
        "%79 = OpVariable %78 StorageBuffer\n"
        "%96 = OpConstantComposite %27 %34 %45 %45\n"
        " %4 = OpFunction %2 None %3\n"
        " %5 = OpLabel\n"

        "${VARIABLE_FUNCTION_DEFINITION}\n"

        "%58 = OpVariable %8 Function\n"
        "%63 = OpVariable %8 Function\n"
        "%65 = OpVariable %10 Function\n"
        "%85 = OpVariable %8 Function\n"
        "%89 = OpVariable %8 Function\n"
        "%91 = OpVariable %10 Function\n"
        "%32 = OpAccessChain %31 %29 %30\n"
        "%33 = OpLoad %26 %32\n"
        "%35 = OpUMod %26 %33 %34\n"
        "%36 = OpIEqual %9 %35 %30\n"
        "      OpSelectionMerge %38 None\n"
        "      OpBranchConditional %36 %37 %38\n"
        "%37 = OpLabel\n"
        "%47 = OpAccessChain %46 %42 %44\n"
        "      OpStore %47 %45\n"
        "%54 = OpAccessChain %31 %29 %30\n"
        "%55 = OpLoad %26 %54\n"
        "%56 = OpUMod %26 %55 %34\n"
        "%57 = OpIEqual %9 %56 %30\n"
        "%60 = OpAccessChain %59 %51 %44\n"
        "%61 = OpLoad %39 %60\n"
        "%62 = OpCopyLogical %7 %61\n"
        "      OpStore %58 %62\n"
        "%64 = OpLoad %7 %53\n"
        "      OpStore %63 %64\n"
        "      OpStore %65 %57\n"
        "%66 = OpFunctionCall %7 %15 %58 %63 %65\n"
        "%67 = OpAccessChain %59 %42 %48\n"
        "%68 = OpCopyLogical %39 %66\n"
        "      OpStore %67 %68\n"
        "%70 = OpAccessChain %46 %42 %69\n"
        "      OpStore %70 %45\n"
        "      OpBranch %38\n"
        "%38 = OpLabel\n"
        "%71 = OpAccessChain %31 %29 %30\n"
        "%72 = OpLoad %26 %71\n"
        "%73 = OpUMod %26 %72 %34\n"
        "%74 = OpIEqual %9 %73 %45\n"
        "      OpSelectionMerge %76 None\n"
        "      OpBranchConditional %74 %75 %76\n"
        "%75 = OpLabel\n"
        "%80 = OpAccessChain %46 %79 %44\n"
        "      OpStore %80 %45\n"
        "%81 = OpAccessChain %31 %29 %30\n"
        "%82 = OpLoad %26 %81\n"
        "%83 = OpUMod %26 %82 %34\n"
        "%84 = OpIEqual %9 %83 %45\n"
        "%86 = OpAccessChain %59 %51 %44\n"
        "%87 = OpLoad %39 %86\n"
        "%88 = OpCopyLogical %7 %87\n"
        "      OpStore %85 %88\n"
        "%90 = OpLoad %7 %53\n"
        "      OpStore %89 %90\n"
        "      OpStore %91 %84\n"
        "%92 = OpFunctionCall %7 %15 %85 %89 %91\n"
        "%93 = OpAccessChain %59 %79 %48\n"
        "%94 = OpCopyLogical %39 %92\n"
        "      OpStore %93 %94\n"
        "%95 = OpAccessChain %46 %79 %69\n"
        "      OpStore %95 %45\n"
        "      OpBranch %76\n"
        "%76 = OpLabel\n"
        "      OpReturn\n"
        "      OpFunctionEnd\n"
        "%15 = OpFunction %7 None %11\n"
        "%12 = OpFunctionParameter %8\n"
        "%13 = OpFunctionParameter %8\n"
        "%14 = OpFunctionParameter %10\n"
        "%16 = OpLabel\n"
        "%17 = OpLoad %9 %14\n"
        "      OpSelectionMerge %19 None\n"
        "      OpBranchConditional %17 %18 %22\n"
        "%18 = OpLabel\n"
        "%20 = OpLoad %7 %12\n"
        "      OpReturnValue %20\n"
        "%22 = OpLabel\n"
        "%23 = OpLoad %7 %13\n"
        "      OpReturnValue %23\n"
        "%19 = OpLabel\n"
        "      OpUnreachable\n"
        "      OpFunctionEnd\n");

    struct VariableDefinition
    {
        std::string name;
        std::string globalVariable;
        std::string spirvVariableDefinitionCode;
        std::string spirvVariableFunctionDefinitionCode;
    };

    std::vector<VariableDefinition> variableDefinitions{
        {"global_variable_private",

         "%53",

         "%52 = OpTypePointer Private %7\n"
         "%53 = OpVariable %52 Private\n",

         ""},

        {"global_variable_shared",

         "%53",

         "%52 = OpTypePointer Workgroup %7\n"
         "%53 = OpVariable %52 Workgroup\n",

         ""},

        {"local_variable",

         "",

         "",

         "%53 = OpVariable %8 Function\n"},
    };

    tcu::TestContext &testCtx = group->getTestContext();
    std::vector<int> input    = {2};

    std::vector<uint32_t> expectedOutput = {1, 0xffffffff, 1};

    for (const auto &variableDefinition : variableDefinitions)
    {
        std::map<std::string, std::string> specializationMap{
            {"GLOBAL_VARIABLE", variableDefinition.globalVariable},
            {"VARIABLE_DEFINITION", variableDefinition.spirvVariableDefinitionCode},
            {"VARIABLE_FUNCTION_DEFINITION", variableDefinition.spirvVariableFunctionDefinitionCode},
        };

        ComputeShaderSpec spec;
        spec.assembly      = shaderTemplate.specialize(specializationMap);
        spec.numWorkGroups = tcu::IVec3(2, 1, 1);
        spec.spirvVersion  = SPIRV_VERSION_1_4;
        spec.requestedVulkanFeatures.extVariablePointers.variablePointersStorageBuffer = true;
        spec.inputs.push_back(Resource(BufferSp(new Int32Buffer(input))));
        spec.outputs.push_back(Resource(BufferSp(new Uint32Buffer(expectedOutput))));
        spec.outputs.push_back(Resource(BufferSp(new Uint32Buffer(expectedOutput))));
        spec.extensions.push_back("VK_KHR_spirv_1_4");
        group->addChild(new SpvAsmComputeShaderCase(testCtx, variableDefinition.name.c_str(), spec));
    }
}

} // namespace

tcu::TestCaseGroup *createEmptyStructComputeGroup(tcu::TestContext &testCtx)
{
    // Tests empty structs in UBOs and SSBOs
    de::MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(testCtx, "empty_struct"));

    // Test copying struct which contains an empty struct
    addTestGroup(group.get(), "copying", addCopyingComputeGroup);
    // Test pointer comparisons of empty struct members
    addTestGroup(group.get(), "pointer_comparison", addPointerComparisionComputeGroup);
    // Test empty structs as function arguments or return type
    addTestGroup(group.get(), "function", addFunctionArgumentReturnValueGroup);

    return group.release();
}

} // namespace SpirVAssembly
} // namespace vkt
