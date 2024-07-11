/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2024 Google Inc.
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
 * \brief SPIR-V Assembly Tests for non-semantic forward references.
 *//*--------------------------------------------------------------------*/

#include "vkApiVersion.hpp"

#include "vktSpvAsmRelaxedWithForwardReferenceTests.hpp"
#include "vktSpvAsmComputeShaderCase.hpp"
#include "vktSpvAsmComputeShaderTestUtil.hpp"
#include "vktSpvAsmGraphicsShaderTestUtil.hpp"

namespace vkt
{
namespace SpirVAssembly
{

using namespace vk;
using std::map;
using std::string;
using std::vector;
using tcu::RGBA;

namespace
{

static ComputeShaderSpec getComputeShaderSpec()
{
    std::vector<float> inoutFloats(10, 0);
    std::vector<int> inputInts(10, 0);

    // in one of tests we need to do imageLoad
    // we don't need any special values in here

    ComputeShaderSpec spec;
    spec.spirvVersion = SPIRV_VERSION_1_6;
    spec.extensions.push_back("VK_KHR_shader_non_semantic_info");
    spec.numWorkGroups = tcu::IVec3(1, 1, 1);
    spec.inputs.push_back(BufferSp(new Float32Buffer(inoutFloats)));
    spec.outputs.push_back(BufferSp(new Float32Buffer(inoutFloats)));
    return spec;
}

class SpvAsmSpirvRelaxedForwardReferenceBasicInstance : public ComputeShaderSpec, public SpvAsmComputeShaderInstance
{
public:
    SpvAsmSpirvRelaxedForwardReferenceBasicInstance(Context &ctx, const std::string &shader);

    tcu::TestStatus iterate(void);

protected:
    std::string m_shaderCode;
};

SpvAsmSpirvRelaxedForwardReferenceBasicInstance::SpvAsmSpirvRelaxedForwardReferenceBasicInstance(
    Context &ctx, const std::string &shader)
    : ComputeShaderSpec(getComputeShaderSpec())
    , SpvAsmComputeShaderInstance(ctx, *this)
    , m_shaderCode(shader)
{
}

tcu::TestStatus SpvAsmSpirvRelaxedForwardReferenceBasicInstance::iterate(void)
{
    return SpvAsmComputeShaderInstance::iterate();
}

class SpvAsmSpirvRelaxedForwardReferenceBasicCase : public TestCase
{
public:
    SpvAsmSpirvRelaxedForwardReferenceBasicCase(tcu::TestContext &testCtx, const char *name, const std::string &shader);

    void checkSupport(Context &context) const;
    void initPrograms(vk::SourceCollections &programCollection) const;
    TestInstance *createInstance(Context &context) const;

protected:
    std::string m_shaderSource;
};

SpvAsmSpirvRelaxedForwardReferenceBasicCase::SpvAsmSpirvRelaxedForwardReferenceBasicCase(tcu::TestContext &testCtx,
                                                                                         const char *name,
                                                                                         const std::string &shader)
    : TestCase(testCtx, name)
    , m_shaderSource(shader)
{
}

void SpvAsmSpirvRelaxedForwardReferenceBasicCase::checkSupport(Context &context) const
{
    context.requireDeviceFunctionality("VK_KHR_shader_non_semantic_info");
    context.requireDeviceFunctionality("VK_KHR_shader_relaxed_extended_instruction");
}

void SpvAsmSpirvRelaxedForwardReferenceBasicCase::initPrograms(SourceCollections &programCollection) const
{
    programCollection.spirvAsmSources.add("compute")
        << SpirVAsmBuildOptions(programCollection.usedVulkanVersion, SPIRV_VERSION_1_6) << m_shaderSource;
}

TestInstance *SpvAsmSpirvRelaxedForwardReferenceBasicCase::createInstance(Context &context) const
{
    return new SpvAsmSpirvRelaxedForwardReferenceBasicInstance(context, m_shaderSource);
}

} // namespace

/* HLSL Shader, compiled with:
 * `dxc -T cs_6_0 -fspv-target-env=vulkan1.3 -fspv-debug=vulkan-with-source -spirv -Od`
 *
 * ```hlsl
 * class A {
 *   static A method() {
 *     A a;
 *     return a;
 *   }
 * };
 *
 * StructuredBuffer<uint> input;
 * RWStructuredBuffer<uint> output;
 *
 * [numthreads(10, 1, 1)]
 * void main(uint3 id : SV_DispatchThreadID) {
 *   output[id.x] = input[id.x];
 *   A::method();
 * }
 * ```
 */
const char *kStaticMethodShader = R"(
           OpCapability Shader
           OpExtension "SPV_KHR_non_semantic_info"
           OpExtension "SPV_KHR_relaxed_extended_instruction"
      %1 = OpExtInstImport "NonSemantic.Shader.DebugInfo.100"
           OpMemoryModel Logical GLSL450
           OpEntryPoint GLCompute %main "main" %gl_GlobalInvocationID %input %output
           OpExecutionMode %main LocalSize 10 1 1
      %6 = OpString "repro.hlsl"
      %7 = OpString "source"
      %8 = OpString "A.method"
      %9 = OpString ""
     %10 = OpString "A"
     %11 = OpString "a"
     %12 = OpString "uint"
     %13 = OpString "main"
     %14 = OpString "id"
     %15 = OpString "fb39af55"
     %16 = OpString " -E main -T cs_6_0 -fspv-target-env=vulkan1.3 -fspv-debug=vulkan-with-source -spirv -Qembed_debug"
     %17 = OpString "@type.RWStructuredBuffer.uint"
     %18 = OpString "type.RWStructuredBuffer.uint"
     %19 = OpString "TemplateParam"
     %20 = OpString "output"
     %21 = OpString "@type.StructuredBuffer.uint"
     %22 = OpString "type.StructuredBuffer.uint"
     %23 = OpString "input"
           OpName %type_StructuredBuffer_uint "type.StructuredBuffer.uint"
           OpName %input "input"
           OpName %type_RWStructuredBuffer_uint "type.RWStructuredBuffer.uint"
           OpName %output "output"
           OpName %main "main"
           OpName %A "A"
           OpDecorate %gl_GlobalInvocationID BuiltIn GlobalInvocationId
           OpDecorate %input DescriptorSet 0
           OpDecorate %input Binding 0
           OpDecorate %output DescriptorSet 0
           OpDecorate %output Binding 1
           OpDecorate %_runtimearr_uint ArrayStride 4
           OpMemberDecorate %type_StructuredBuffer_uint 0 Offset 0
           OpMemberDecorate %type_StructuredBuffer_uint 0 NonWritable
           OpDecorate %type_StructuredBuffer_uint Block
           OpMemberDecorate %type_RWStructuredBuffer_uint 0 Offset 0
           OpDecorate %type_RWStructuredBuffer_uint Block
    %int = OpTypeInt 32 1
  %int_0 = OpConstant %int 0
   %uint = OpTypeInt 32 0
%uint_32 = OpConstant %uint 32
%_runtimearr_uint = OpTypeRuntimeArray %uint
%type_StructuredBuffer_uint = OpTypeStruct %_runtimearr_uint
%_ptr_StorageBuffer_type_StructuredBuffer_uint = OpTypePointer StorageBuffer %type_StructuredBuffer_uint
%type_RWStructuredBuffer_uint = OpTypeStruct %_runtimearr_uint
%_ptr_StorageBuffer_type_RWStructuredBuffer_uint = OpTypePointer StorageBuffer %type_RWStructuredBuffer_uint
 %v3uint = OpTypeVector %uint 3
%_ptr_Input_v3uint = OpTypePointer Input %v3uint
   %void = OpTypeVoid
 %uint_1 = OpConstant %uint 1
 %uint_4 = OpConstant %uint 4
 %uint_5 = OpConstant %uint 5
 %uint_3 = OpConstant %uint 3
 %uint_2 = OpConstant %uint 2
 %uint_0 = OpConstant %uint 0
 %uint_7 = OpConstant %uint 7
%uint_21 = OpConstant %uint 21
 %uint_6 = OpConstant %uint 6
%uint_12 = OpConstant %uint 12
%uint_43 = OpConstant %uint 43
%uint_17 = OpConstant %uint 17
 %uint_9 = OpConstant %uint 9
%uint_26 = OpConstant %uint 26
 %uint_8 = OpConstant %uint 8
%uint_24 = OpConstant %uint 24
     %79 = OpTypeFunction %void
%uint_15 = OpConstant %uint 15
%uint_13 = OpConstant %uint 13
%uint_27 = OpConstant %uint 27
%_ptr_StorageBuffer_uint = OpTypePointer StorageBuffer %uint
%uint_18 = OpConstant %uint 18
%uint_28 = OpConstant %uint 28
%A = OpTypeStruct
%uint_14 = OpConstant %uint 14
%_ptr_Function_A = OpTypePointer Function %A
%input = OpVariable %_ptr_StorageBuffer_type_StructuredBuffer_uint StorageBuffer
%output = OpVariable %_ptr_StorageBuffer_type_RWStructuredBuffer_uint StorageBuffer
%gl_GlobalInvocationID = OpVariable %_ptr_Input_v3uint Input
%28 = OpExtInst %void %1 DebugInfoNone
%29 = OpExtInst %void %1 DebugExpression
%30 = OpExtInst %void %1 DebugSource %6 %7
%31 = OpExtInst %void %1 DebugCompilationUnit %uint_1 %uint_4 %30 %uint_5
%35 = OpExtInstWithForwardRefs %void %1 DebugTypeFunction %uint_3 %37
%38 = OpExtInstWithForwardRefs %void %1 DebugFunction %8 %35 %30 %uint_2 %uint_3 %37 %9 %uint_3 %uint_2
%37 = OpExtInst %void %1 DebugTypeComposite %10 %uint_0 %30 %uint_1 %uint_7 %31 %10 %uint_0 %uint_3 %38
%42 = OpExtInst %void %1 DebugLexicalBlock %30 %uint_2 %uint_21 %38
%44 = OpExtInst %void %1 DebugLocalVariable %11 %37 %30 %uint_3 %uint_7 %42 %uint_4
%45 = OpExtInst %void %1 DebugTypeBasic %12 %uint_32 %uint_6 %uint_0
%48 = OpExtInst %void %1 DebugTypeVector %45 %uint_3
%49 = OpExtInst %void %1 DebugTypeFunction %uint_3 %void %48
%50 = OpExtInst %void %1 DebugFunction %13 %49 %30 %uint_12 %uint_1 %31 %9 %uint_3 %uint_12
%52 = OpExtInst %void %1 DebugLexicalBlock %30 %uint_12 %uint_43 %50
%54 = OpExtInst %void %1 DebugLocalVariable %14 %48 %30 %uint_12 %uint_17 %50 %uint_4 %uint_1
%56 = OpExtInst %void %1 DebugTypeComposite %17 %uint_0 %30 %uint_0 %uint_0 %31 %18 %28 %uint_3
%57 = OpExtInst %void %1 DebugTypeTemplateParameter %19 %45 %28 %30 %uint_0 %uint_0
%58 = OpExtInst %void %1 DebugTypeTemplate %56 %57
%59 = OpExtInst %void %1 DebugGlobalVariable %20 %58 %30 %uint_9 %uint_26 %31 %20 %output %uint_8
%63 = OpExtInst %void %1 DebugTypeComposite %21 %uint_0 %30 %uint_0 %uint_0 %31 %22 %28 %uint_3
%64 = OpExtInst %void %1 DebugTypeTemplateParameter %19 %45 %28 %30 %uint_0 %uint_0
%65 = OpExtInst %void %1 DebugTypeTemplate %63 %64
%66 = OpExtInst %void %1 DebugGlobalVariable %23 %65 %30 %uint_8 %uint_24 %31 %23 %input %uint_8
%68 = OpExtInst %void %1 DebugEntryPoint %50 %31 %15 %16
%69 = OpExtInst %void %1 DebugInlinedAt %uint_14 %52
%main = OpFunction %void None %79
%87 = OpLabel
%88 = OpVariable %_ptr_Function_A Function
%89 = OpExtInst %void %1 DebugFunctionDefinition %50 %main
%90 = OpLoad %v3uint %gl_GlobalInvocationID
%91 = OpExtInst %void %1 DebugValue %54 %90 %29
%111 = OpExtInst %void %1 DebugScope %52
%92 = OpExtInst %void %1 DebugLine %30 %uint_13 %uint_13 %uint_24 %uint_27
%93 = OpCompositeExtract %uint %90 0
%94 = OpExtInst %void %1 DebugLine %30 %uint_13 %uint_13 %uint_18 %uint_28
%95 = OpAccessChain %_ptr_StorageBuffer_uint %input %int_0 %93
%97 = OpLoad %uint %95
%98 = OpExtInst %void %1 DebugLine %30 %uint_13 %uint_13 %uint_3 %uint_28
%99 = OpAccessChain %_ptr_StorageBuffer_uint %output %int_0 %93
OpStore %99 %97
%112 = OpExtInst %void %1 DebugScope %42 %69
%101 = OpExtInst %void %1 DebugLine %30 %uint_3 %uint_3 %uint_5 %uint_7
%102 = OpExtInst %void %1 DebugDeclare %44 %88 %29
%113 = OpExtInst %void %1 DebugNoScope
%103 = OpExtInst %void %1 DebugLine %30 %uint_15 %uint_15 %uint_1 %uint_1
OpReturn
OpFunctionEnd
)";

tcu::TestCaseGroup *createRelaxedWithForwardReferenceGraphicsGroup(tcu::TestContext &testCtx)
{
    de::MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(testCtx, "relaxed_with_forward_reference"));

    struct TestData
    {
        const char *name;
        std::string shader;
    };
    std::vector<TestData> testList = {{"static_method_shader", kStaticMethodShader}};

    for (const auto &item : testList)
        group->addChild(new SpvAsmSpirvRelaxedForwardReferenceBasicCase(testCtx, item.name, item.shader));
    return group.release();
}

} // namespace SpirVAssembly
} // namespace vkt
