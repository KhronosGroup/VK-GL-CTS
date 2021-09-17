/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2018 Google Inc.
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
 * \brief SPIR-V Assembly Tests for pointers as function parameters.
 *//*--------------------------------------------------------------------*/

#include "vktSpvAsmPointerParameterTests.hpp"
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
using tcu::IVec3;
using tcu::Vec4;
using tcu::RGBA;

namespace
{

void addComputePointerParamToParamTest (tcu::TestCaseGroup* group)
{
	tcu::TestContext&	testCtx				= group->getTestContext();
	const int			numFloats			= 128;
	ComputeShaderSpec	spec;
	vector<float>		expectedOutput;

	// Implements the following pseudo GLSL shader:
	//
	//	float func(alias float* f, alias float* g)
	//	{
	//		*g = 5.0;
	//		*f = 2.0;
	//		return *g;
	//	}
	//
	//	void main()
	//	{
	//		float a = 0.0;
	//		o = func(&a, &a);  // should return 2.0
	//		float b = 0.0;
	//		o += func(&a, &b); // should return 5.0
	//	}
	const string		shaderSource		=
			"                          OpCapability Shader\n"
			"                     %1 = OpExtInstImport \"GLSL.std.450\"\n"
			"                          OpMemoryModel Logical GLSL450\n"
			"                          OpEntryPoint GLCompute %main \"main\" %gl_GlobalInvocationID\n"
			"                          OpExecutionMode %main LocalSize 1 1 1\n"
			"                          OpSource GLSL 430\n"
			"                          OpDecorate %_arr_float_uint_128 ArrayStride 4\n"
			"                          OpMemberDecorate %Output 0 Offset 0\n"
			"                          OpDecorate %Output BufferBlock\n"
			"                          OpDecorate %dataOutput DescriptorSet 0\n"
			"                          OpDecorate %dataOutput Binding 0\n"
			"                          OpDecorate %f Aliased\n"
			"                          OpDecorate %g Aliased\n"
			"                          OpDecorate %gl_GlobalInvocationID BuiltIn GlobalInvocationId\n"
			"                  %void = OpTypeVoid\n"
			"             %void_func = OpTypeFunction %void\n"
			"                 %float = OpTypeFloat 32\n"
			"   %_ptr_Function_float = OpTypePointer Function %float\n"
			"            %func0_decl = OpTypeFunction %float %_ptr_Function_float %_ptr_Function_float\n"
			"               %float_0 = OpConstant %float 0\n"
			"               %float_5 = OpConstant %float 5\n"
			"               %float_2 = OpConstant %float 2\n"
			"                  %uint = OpTypeInt 32 0\n"
			"              %uint_128 = OpConstant %uint 128\n"
			"   %_arr_float_uint_128 = OpTypeArray %float %uint_128\n"
			"                %Output = OpTypeStruct %_arr_float_uint_128\n"
			"   %_ptr_Uniform_Output = OpTypePointer Uniform %Output\n"
			"            %dataOutput = OpVariable %_ptr_Uniform_Output Uniform\n"
			"                   %int = OpTypeInt 32 1\n"
			"                 %int_0 = OpConstant %int 0\n"
			"                %v3uint = OpTypeVector %uint 3\n"
			"     %_ptr_Input_v3uint = OpTypePointer Input %v3uint\n"
			" %gl_GlobalInvocationID = OpVariable %_ptr_Input_v3uint Input\n"
			"                %uint_0 = OpConstant %uint 0\n"
			"       %_ptr_Input_uint = OpTypePointer Input %uint\n"
			"    %_ptr_Uniform_float = OpTypePointer Uniform %float\n"
			"                  %main = OpFunction %void None %void_func\n"
			"                 %entry = OpLabel\n"
			"                     %a = OpVariable %_ptr_Function_float Function %float_0\n"
			"                     %b = OpVariable %_ptr_Function_float Function %float_0\n"
			"                     %o = OpVariable %_ptr_Function_float Function %float_0\n"
			"                  %ret0 = OpFunctionCall %float %func %a %a\n"
			"                          OpStore %o %ret0\n"
			"                  %ret1 = OpFunctionCall %float %func %a %b\n"
			"                 %o_val = OpLoad %float %o\n"
			"                   %sum = OpFAdd %float %o_val %ret1\n"
			"            %inv_id_ptr = OpAccessChain %_ptr_Input_uint %gl_GlobalInvocationID %uint_0\n"
			"                %inv_id = OpLoad %uint %inv_id_ptr\n"
			"               %out_ptr = OpAccessChain %_ptr_Uniform_float %dataOutput %int_0 %inv_id\n"
			"                          OpStore %out_ptr %sum\n"
			"                          OpReturn\n"
			"                          OpFunctionEnd\n"
			"                  %func = OpFunction %float None %func0_decl\n"
			"                     %f = OpFunctionParameter %_ptr_Function_float\n"
			"                     %g = OpFunctionParameter %_ptr_Function_float\n"
			"            %func_entry = OpLabel\n"
			"                          OpStore %g %float_5\n"
			"                          OpStore %f %float_2\n"
			"                   %ret = OpLoad %float %g\n"
			"                          OpReturnValue %ret\n"
			"                          OpFunctionEnd\n";

	expectedOutput.reserve(numFloats);
	for (deUint32 numIdx = 0; numIdx < numFloats; ++numIdx)
		expectedOutput.push_back(7.0f);

	spec.outputs.push_back(BufferSp(new Float32Buffer(expectedOutput)));

	spec.assembly				= shaderSource;
	spec.numWorkGroups			= IVec3(numFloats, 1, 1);

	group->addChild(new SpvAsmComputeShaderCase(testCtx, "param_to_param", "", spec));
}

void addComputePointerParamToGlobalTest (tcu::TestCaseGroup* group)
{
	tcu::TestContext&	testCtx				= group->getTestContext();
	const int			numFloats			= 128;
	ComputeShaderSpec	spec;
	vector<float>		expectedOutput;

	// Implements the following pseudo GLSL shader:
	//
	//	alias float a = 0.0;
	//
	//	float func0(alias float* f0) // f in Private storage class
	//	{
	//		*a = 5.0;
	//		*f0 = 2.0;
	//		return *a;
	//	}
	//
	//	float func1(alias float* f1) // f in Function storage class
	//	{
	//		*a = 5.0;
	//		*f1 = 2.0;
	//		return *a;
	//	}
	//
	//	void main()
	//	{
	//		o = func0(&a);  // should return 2.0
	//		float b = 0.0;
	//		o += func1(&b); // should return 5.0
	//	}
	const string		shaderSource		=
			"                          OpCapability Shader\n"
			"                     %1 = OpExtInstImport \"GLSL.std.450\"\n"
			"                          OpMemoryModel Logical GLSL450\n"
			"                          OpEntryPoint GLCompute %main \"main\" %gl_GlobalInvocationID\n"
			"                          OpExecutionMode %main LocalSize 1 1 1\n"
			"                          OpSource GLSL 430\n"
			"                          OpDecorate %_arr_float_uint_128 ArrayStride 4\n"
			"                          OpMemberDecorate %Output 0 Offset 0\n"
			"                          OpDecorate %Output BufferBlock\n"
			"                          OpDecorate %dataOutput DescriptorSet 0\n"
			"                          OpDecorate %dataOutput Binding 0\n"
			"                          OpDecorate %f0 Aliased\n"
			"                          OpDecorate %f1 Aliased\n"
			"                          OpDecorate %a Aliased\n"
			"                          OpDecorate %gl_GlobalInvocationID BuiltIn GlobalInvocationId\n"
			"                  %void = OpTypeVoid\n"
			"             %void_func = OpTypeFunction %void\n"
			"                 %float = OpTypeFloat 32\n"
			"   %_ptr_Function_float = OpTypePointer Function %float\n"
			"    %_ptr_Private_float = OpTypePointer Private %float\n"
			"            %func0_decl = OpTypeFunction %float %_ptr_Private_float\n"
			"            %func1_decl = OpTypeFunction %float %_ptr_Function_float\n"
			"               %float_0 = OpConstant %float 0\n"
			"               %float_5 = OpConstant %float 5\n"
			"               %float_2 = OpConstant %float 2\n"
			"                  %uint = OpTypeInt 32 0\n"
			"              %uint_128 = OpConstant %uint 128\n"
			"   %_arr_float_uint_128 = OpTypeArray %float %uint_128\n"
			"                %Output = OpTypeStruct %_arr_float_uint_128\n"
			"   %_ptr_Uniform_Output = OpTypePointer Uniform %Output\n"
			"            %dataOutput = OpVariable %_ptr_Uniform_Output Uniform\n"
			"                   %int = OpTypeInt 32 1\n"
			"                 %int_0 = OpConstant %int 0\n"
			"                %v3uint = OpTypeVector %uint 3\n"
			"     %_ptr_Input_v3uint = OpTypePointer Input %v3uint\n"
			" %gl_GlobalInvocationID = OpVariable %_ptr_Input_v3uint Input\n"
			"                %uint_0 = OpConstant %uint 0\n"
			"       %_ptr_Input_uint = OpTypePointer Input %uint\n"
			"    %_ptr_Uniform_float = OpTypePointer Uniform %float\n"
			"                     %a = OpVariable %_ptr_Private_float Private %float_0\n"
			"                  %main = OpFunction %void None %void_func\n"
			"                 %entry = OpLabel\n"
			"                     %b = OpVariable %_ptr_Function_float Function %float_0\n"
			"                     %o = OpVariable %_ptr_Function_float Function %float_0\n"
			"                  %ret0 = OpFunctionCall %float %func0 %a\n"
			"                          OpStore %o %ret0\n"
			"                  %ret1 = OpFunctionCall %float %func1 %b\n"
			"                 %o_val = OpLoad %float %o\n"
			"                   %sum = OpFAdd %float %o_val %ret1\n"
			"            %inv_id_ptr = OpAccessChain %_ptr_Input_uint %gl_GlobalInvocationID %uint_0\n"
			"                %inv_id = OpLoad %uint %inv_id_ptr\n"
			"               %out_ptr = OpAccessChain %_ptr_Uniform_float %dataOutput %int_0 %inv_id\n"
			"                          OpStore %out_ptr %sum\n"
			"                          OpReturn\n"
			"                          OpFunctionEnd\n"
			"                 %func0 = OpFunction %float None %func0_decl\n"
			"                    %f0 = OpFunctionParameter %_ptr_Private_float\n"
			"           %func0_entry = OpLabel\n"
			"                          OpStore %a %float_5\n"
			"                          OpStore %f0 %float_2\n"
			"             %func0_ret = OpLoad %float %a\n"
			"                          OpReturnValue %func0_ret\n"
			"                          OpFunctionEnd\n"
			"                 %func1 = OpFunction %float None %func1_decl\n"
			"                    %f1 = OpFunctionParameter %_ptr_Function_float\n"
			"           %func1_entry = OpLabel\n"
			"                          OpStore %a %float_5\n"
			"                          OpStore %f1 %float_2\n"
			"             %func1_ret = OpLoad %float %a\n"
			"                          OpReturnValue %func1_ret\n"
			"                          OpFunctionEnd\n";

	expectedOutput.reserve(numFloats);
	for (deUint32 numIdx = 0; numIdx < numFloats; ++numIdx)
		expectedOutput.push_back(7.0f);

	spec.outputs.push_back(BufferSp(new Float32Buffer(expectedOutput)));

	spec.assembly				= shaderSource;
	spec.numWorkGroups			= IVec3(numFloats, 1, 1);

	group->addChild(new SpvAsmComputeShaderCase(testCtx, "param_to_global", "", spec));
}

void addComputePointerBufferMemoryTest (tcu::TestCaseGroup* group)
{
	tcu::TestContext&	testCtx				= group->getTestContext();
	const int			numFloats			= 128;
	ComputeShaderSpec	spec;
	vector<float>		expectedOutput;
	VulkanFeatures		requiredFeatures;

	// Implements the following pseudo GLSL shader:
	//
	//	layout (binding = 0) buffer Output
	//	{
	//		vec4 arr0[16];
	//		vec4 arr1[];
	//	} dataOutput;
	//
	//	void func0(vec4* f0[16], uint i)
	//	{
	//		f0[i] = vec4(5.0);
	//	}
	//
	//	void func1(vec4* f1[], uint i)
	//	{
	//		f1[i] = vec4(2.0);
	//	}
	//
	//	void main()
	//	{
	//		uint idx = gl_GlobalInvocationID.x;
	//		func0(dataOutput.arr0, idx);
	//		func1(dataOutput.arr1, idx);
	//	}
	const string		shaderSource		=
			"                          OpCapability Shader\n"
			"                          OpCapability VariablePointersStorageBuffer\n"
			"                          OpExtension \"SPV_KHR_storage_buffer_storage_class\"\n"
			"                          OpExtension \"SPV_KHR_variable_pointers\"\n"
			"                     %1 = OpExtInstImport \"GLSL.std.450\"\n"
			"                          OpMemoryModel Logical GLSL450\n"
			"                          OpEntryPoint GLCompute %main \"main\" %gl_GlobalInvocationID\n"
			"                          OpExecutionMode %main LocalSize 1 1 1\n"
			"                          OpSource GLSL 430\n"
			"                          OpMemberDecorate %Output 0 Offset 0\n"
			"                          OpMemberDecorate %Output 1 Offset 256\n"
			"                          OpDecorate %arr_vec4_16 ArrayStride 16\n"
			"                          OpDecorate %arr_vec4_rt ArrayStride 16\n"
			"                          OpDecorate %Output Block\n"
			"                          OpDecorate %dataOutput DescriptorSet 0\n"
			"                          OpDecorate %dataOutput Binding 0\n"
			"                          OpDecorate %gl_GlobalInvocationID BuiltIn GlobalInvocationId\n"
			"                  %void = OpTypeVoid\n"
			"             %void_func = OpTypeFunction %void\n"
			"                 %float = OpTypeFloat 32\n"
			"   %_ptr_Function_float = OpTypePointer Function %float\n"
			"               %float_5 = OpConstant %float 5\n"
			"               %float_2 = OpConstant %float 2\n"
			"                  %uint = OpTypeInt 32 0\n"
			"    %_ptr_Function_uint = OpTypePointer Function %uint\n"
			"               %uint_16 = OpConstant %uint 16\n"
			"                  %vec4 = OpTypeVector %float 4\n"
			"                %vec4_5 = OpConstantComposite %vec4 %float_5 %float_5 %float_5 %float_5\n"
			"                %vec4_2 = OpConstantComposite %vec4 %float_2 %float_2 %float_2 %float_2\n"
			"           %arr_vec4_16 = OpTypeArray %vec4 %uint_16\n"
			"           %arr_vec4_rt = OpTypeRuntimeArray %vec4\n"
			"       %arr_vec4_16_ptr = OpTypePointer StorageBuffer %arr_vec4_16\n"
			"       %arr_vec4_rt_ptr = OpTypePointer StorageBuffer %arr_vec4_rt\n"
			"            %func0_decl = OpTypeFunction %void %arr_vec4_16_ptr %_ptr_Function_uint\n"
			"            %func1_decl = OpTypeFunction %void %arr_vec4_rt_ptr %_ptr_Function_uint\n"
			"                %Output = OpTypeStruct %arr_vec4_16 %arr_vec4_rt\n"
			"        %_ptr_sb_Output = OpTypePointer StorageBuffer %Output\n"
			"            %dataOutput = OpVariable %_ptr_sb_Output StorageBuffer\n"
			"                   %int = OpTypeInt 32 1\n"
			"                 %int_0 = OpConstant %int 0\n"
			"                 %int_1 = OpConstant %int 1\n"
			"                %v3uint = OpTypeVector %uint 3\n"
			"     %_ptr_Input_v3uint = OpTypePointer Input %v3uint\n"
			" %gl_GlobalInvocationID = OpVariable %_ptr_Input_v3uint Input\n"
			"                %uint_0 = OpConstant %uint 0\n"
			"       %_ptr_Input_uint = OpTypePointer Input %uint\n"
			"          %_ptr_sb_vec4 = OpTypePointer StorageBuffer %vec4\n"
			"                  %main = OpFunction %void None %void_func\n"
			"                 %entry = OpLabel\n"
			"                   %idx = OpVariable %_ptr_Function_uint Function\n"
			"            %inv_id_ptr = OpAccessChain %_ptr_Input_uint %gl_GlobalInvocationID %uint_0\n"
			"                %inv_id = OpLoad %uint %inv_id_ptr\n"
			"                          OpStore %idx %inv_id\n"
			"                  %ptr0 = OpAccessChain %arr_vec4_16_ptr %dataOutput %int_0\n"
			"                  %ptr1 = OpAccessChain %arr_vec4_rt_ptr %dataOutput %int_1\n"
			"                  %ret0 = OpFunctionCall %void %func0 %ptr0 %idx\n"
			"                  %ret1 = OpFunctionCall %void %func1 %ptr1 %idx\n"
			"                          OpReturn\n"
			"                          OpFunctionEnd\n"
			"                 %func0 = OpFunction %void None %func0_decl\n"
			"                    %f0 = OpFunctionParameter %arr_vec4_16_ptr\n"
			"                    %i0 = OpFunctionParameter %_ptr_Function_uint\n"
			"           %func0_entry = OpLabel\n"
			"                  %idx0 = OpLoad %uint %i0\n"
			"              %out_ptr0 = OpAccessChain %_ptr_sb_vec4 %f0 %idx0\n"
			"                          OpStore %out_ptr0 %vec4_5\n"
			"                          OpReturn\n"
			"                          OpFunctionEnd\n"
			"                 %func1 = OpFunction %void None %func1_decl\n"
			"                    %f1 = OpFunctionParameter %arr_vec4_rt_ptr\n"
			"                    %i1 = OpFunctionParameter %_ptr_Function_uint\n"
			"           %func1_entry = OpLabel\n"
			"                  %idx1 = OpLoad %uint %i1\n"
			"              %out_ptr1 = OpAccessChain %_ptr_sb_vec4 %f1 %idx1\n"
			"                          OpStore %out_ptr1 %vec4_2\n"
			"                          OpReturn\n"
			"                          OpFunctionEnd\n";

	expectedOutput.reserve(numFloats);
	for (deUint32 numIdx = 0; numIdx < numFloats / 2; ++numIdx)
		expectedOutput.push_back(5.0f);
	for (deUint32 numIdx = 0; numIdx < numFloats / 2; ++numIdx)
		expectedOutput.push_back(2.0f);

	requiredFeatures.extVariablePointers = EXTVARIABLEPOINTERSFEATURES_VARIABLE_POINTERS_STORAGEBUFFER;

	spec.outputs.push_back(BufferSp(new Float32Buffer(expectedOutput)));

	spec.assembly					= shaderSource;
	spec.numWorkGroups				= IVec3(16, 1, 1);
	spec.requestedVulkanFeatures	= requiredFeatures;
	spec.extensions.push_back("VK_KHR_variable_pointers");

	group->addChild(new SpvAsmComputeShaderCase(testCtx, "buffer_memory", "", spec));
}

void addComputePointerBufferMemoryVariablePointersTest (tcu::TestCaseGroup* group)
{
	tcu::TestContext&	testCtx				= group->getTestContext();
	const int			numFloats			= 128;
	ComputeShaderSpec	spec;
	VulkanFeatures		requiredFeatures;
	vector<float>		expectedOutput;

	// Implements the following pseudo GLSL shader:
	//
	//	layout (binding = 0) buffer Output
	//	{
	//		vec4 arr0[16];
	//		vec4 arr1[];
	//	} dataOutput;
	//
	//	void func0(vec4* f0[16], uint i)
	//	{
	//		f0[i] = vec4(5.0);
	//	}
	//
	//	void func1(vec4* f1[], uint i)
	//	{
	//		f1[i] = vec4(2.0);
	//	}
	//
	//	void main()
	//	{
	//		uint idx = gl_GlobalInvocationID.x;
	//		func0(dataOutput.arr0, idx);
	//		func1(dataOutput.arr1, idx);
	//	}
	const string		shaderSource		=
			"                          OpCapability Shader\n"
			"                          OpCapability VariablePointersStorageBuffer\n"
			"                          OpExtension \"SPV_KHR_variable_pointers\"\n"
			"                          OpExtension \"SPV_KHR_storage_buffer_storage_class\"\n"
			"                     %1 = OpExtInstImport \"GLSL.std.450\"\n"
			"                          OpMemoryModel Logical GLSL450\n"
			"                          OpEntryPoint GLCompute %main \"main\" %gl_GlobalInvocationID\n"
			"                          OpExecutionMode %main LocalSize 1 1 1\n"
			"                          OpSource GLSL 430\n"
			"                          OpMemberDecorate %Output 0 Offset 0\n"
			"                          OpMemberDecorate %Output 1 Offset 256\n"
			"                          OpDecorate %arr_vec4_16 ArrayStride 16\n"
			"                          OpDecorate %arr_vec4_rt ArrayStride 16\n"
			"                          OpDecorate %Output Block\n"
			"                          OpDecorate %dataOutput DescriptorSet 0\n"
			"                          OpDecorate %dataOutput Binding 0\n"
			"                          OpDecorate %gl_GlobalInvocationID BuiltIn GlobalInvocationId\n"
			"                  %void = OpTypeVoid\n"
			"             %void_func = OpTypeFunction %void\n"
			"                 %float = OpTypeFloat 32\n"
			"   %_ptr_Function_float = OpTypePointer Function %float\n"
			"               %float_5 = OpConstant %float 5\n"
			"               %float_2 = OpConstant %float 2\n"
			"                  %uint = OpTypeInt 32 0\n"
			"    %_ptr_Function_uint = OpTypePointer Function %uint\n"
			"               %uint_16 = OpConstant %uint 16\n"
			"                  %vec4 = OpTypeVector %float 4\n"
			"                %vec4_5 = OpConstantComposite %vec4 %float_5 %float_5 %float_5 %float_5\n"
			"                %vec4_2 = OpConstantComposite %vec4 %float_2 %float_2 %float_2 %float_2\n"
			"           %arr_vec4_16 = OpTypeArray %vec4 %uint_16\n"
			"           %arr_vec4_rt = OpTypeRuntimeArray %vec4\n"
			"       %arr_vec4_16_ptr = OpTypePointer StorageBuffer %arr_vec4_16\n"
			"       %arr_vec4_rt_ptr = OpTypePointer StorageBuffer %arr_vec4_rt\n"
			"            %func0_decl = OpTypeFunction %void %arr_vec4_16_ptr %_ptr_Function_uint\n"
			"            %func1_decl = OpTypeFunction %void %arr_vec4_rt_ptr %_ptr_Function_uint\n"
			"                %Output = OpTypeStruct %arr_vec4_16 %arr_vec4_rt\n"
			"        %_ptr_sb_Output = OpTypePointer StorageBuffer %Output\n"
			"            %dataOutput = OpVariable %_ptr_sb_Output StorageBuffer\n"
			"                   %int = OpTypeInt 32 1\n"
			"                 %int_0 = OpConstant %int 0\n"
			"                 %int_1 = OpConstant %int 1\n"
			"                %v3uint = OpTypeVector %uint 3\n"
			"     %_ptr_Input_v3uint = OpTypePointer Input %v3uint\n"
			" %gl_GlobalInvocationID = OpVariable %_ptr_Input_v3uint Input\n"
			"                %uint_0 = OpConstant %uint 0\n"
			"       %_ptr_Input_uint = OpTypePointer Input %uint\n"
			"          %_ptr_sb_vec4 = OpTypePointer StorageBuffer %vec4\n"
			"                  %main = OpFunction %void None %void_func\n"
			"                 %entry = OpLabel\n"
			"                   %idx = OpVariable %_ptr_Function_uint Function\n"
			"            %inv_id_ptr = OpAccessChain %_ptr_Input_uint %gl_GlobalInvocationID %uint_0\n"
			"                %inv_id = OpLoad %uint %inv_id_ptr\n"
			"                          OpStore %idx %inv_id\n"
			"                  %ptr0 = OpAccessChain %arr_vec4_16_ptr %dataOutput %int_0\n"
			"                  %ptr1 = OpAccessChain %arr_vec4_rt_ptr %dataOutput %int_1\n"
			"                  %ret0 = OpFunctionCall %void %func0 %ptr0 %idx\n"
			"                  %ret1 = OpFunctionCall %void %func1 %ptr1 %idx\n"
			"                          OpReturn\n"
			"                          OpFunctionEnd\n"
			"                 %func0 = OpFunction %void None %func0_decl\n"
			"                    %f0 = OpFunctionParameter %arr_vec4_16_ptr\n"
			"                    %i0 = OpFunctionParameter %_ptr_Function_uint\n"
			"           %func0_entry = OpLabel\n"
			"                  %idx0 = OpLoad %uint %i0\n"
			"              %out_ptr0 = OpAccessChain %_ptr_sb_vec4 %f0 %idx0\n"
			"                          OpStore %out_ptr0 %vec4_5\n"
			"                          OpReturn\n"
			"                          OpFunctionEnd\n"
			"                 %func1 = OpFunction %void None %func1_decl\n"
			"                    %f1 = OpFunctionParameter %arr_vec4_rt_ptr\n"
			"                    %i1 = OpFunctionParameter %_ptr_Function_uint\n"
			"           %func1_entry = OpLabel\n"
			"                  %idx1 = OpLoad %uint %i1\n"
			"              %out_ptr1 = OpAccessChain %_ptr_sb_vec4 %f1 %idx1\n"
			"                          OpStore %out_ptr1 %vec4_2\n"
			"                          OpReturn\n"
			"                          OpFunctionEnd\n";

	expectedOutput.reserve(numFloats);
	for (deUint32 numIdx = 0; numIdx < numFloats / 2; ++numIdx)
		expectedOutput.push_back(5.0f);
	for (deUint32 numIdx = 0; numIdx < numFloats / 2; ++numIdx)
		expectedOutput.push_back(2.0f);

	requiredFeatures.extVariablePointers = EXTVARIABLEPOINTERSFEATURES_VARIABLE_POINTERS_STORAGEBUFFER;
	spec.outputs.push_back(BufferSp(new Float32Buffer(expectedOutput)));
	spec.extensions.push_back("VK_KHR_variable_pointers");

	spec.assembly					= shaderSource;
	spec.numWorkGroups				= IVec3(16, 1, 1);
	spec.requestedVulkanFeatures	= requiredFeatures;

	group->addChild(new SpvAsmComputeShaderCase(testCtx, "buffer_memory_variable_pointers", "", spec));
}

void addComputePointerWorkgroupMemoryVariablePointersTest (tcu::TestCaseGroup* group)
{
	tcu::TestContext&	testCtx				= group->getTestContext();
	const int			numFloats			= 128;
	ComputeShaderSpec	spec;
	VulkanFeatures		requiredFeatures;
	vector<float>		expectedOutput;

	// Implements the following pseudo GLSL shader:
	//
	//	layout (local_size_x = 16, local_size_y = 1, local_size_z = 1) in;
	//
	//	layout (binding = 0) buffer Output
	//	{
	//		vec4 arr0[16];
	//		vec4 arr1[];
	//	} dataOutput;
	//
	//	shared struct
	//	{
	//		vec4 arr0[16];
	//		vec4 arr1[16];
	//	} sharedData;
	//
	//	void func0(vec4* f0[16], uint i)
	//	{
	//		f0[i] = vec4(i);
	//	}
	//
	//	void func1(vec4* f1[16], uint i)
	//	{
	//		f1[i] = vec4(i+5);
	//	}
	//
	//	void main()
	//	{
	//		uint idx = gl_LocalInvocationID.x;
	//		func0(sharedData.arr0, idx);
	//		func1(sharedData.arr1, idx);
	//		barier();
	//		dataOutput.arr0[idx] = sharedData.arr1[(idx+1) % 16];
	//		dataOutput.arr1[idx] = sharedData.arr0[(idx+1) % 16];
	//	}
	const string		shaderSource		=
			"                          OpCapability Shader\n"
			"                          OpCapability VariablePointers\n"
			"                          OpExtension \"SPV_KHR_variable_pointers\"\n"
			"                          OpExtension \"SPV_KHR_storage_buffer_storage_class\"\n"
			"                     %1 = OpExtInstImport \"GLSL.std.450\"\n"
			"                          OpMemoryModel Logical GLSL450\n"
			"                          OpEntryPoint GLCompute %main \"main\" %gl_LocalInvocationID\n"
			"                          OpExecutionMode %main LocalSize 16 1 1\n"
			"                          OpSource GLSL 430\n"
			"                          OpMemberDecorate %Output 0 Offset 0\n"
			"                          OpMemberDecorate %Output 1 Offset 256\n"
			"                          OpMemberDecorate %struct 0 Offset 0\n"
			"                          OpMemberDecorate %struct 1 Offset 256\n"
			"                          OpDecorate %arr_vec4_16 ArrayStride 16\n"
			"                          OpDecorate %arr_vec4_rt ArrayStride 16\n"
			"                          OpDecorate %Output Block\n"
			"                          OpDecorate %dataOutput DescriptorSet 0\n"
			"                          OpDecorate %dataOutput Binding 0\n"
			"                          OpDecorate %gl_LocalInvocationID BuiltIn LocalInvocationId\n"
			"                  %void = OpTypeVoid\n"
			"             %void_func = OpTypeFunction %void\n"
			"                 %float = OpTypeFloat 32\n"
			"   %_ptr_Function_float = OpTypePointer Function %float\n"
			"                  %uint = OpTypeInt 32 0\n"
			"    %_ptr_Function_uint = OpTypePointer Function %uint\n"
			"                %uint_1 = OpConstant %uint 1\n"
			"                %uint_2 = OpConstant %uint 2\n"
			"                %uint_5 = OpConstant %uint 5\n"
			"               %uint_16 = OpConstant %uint 16\n"
			"              %uint_264 = OpConstant %uint 264\n"
			"                  %vec4 = OpTypeVector %float 4\n"
			"           %arr_vec4_16 = OpTypeArray %vec4 %uint_16\n"
			"           %arr_vec4_rt = OpTypeRuntimeArray %vec4\n"
			"    %arr_vec4_16_sb_ptr = OpTypePointer StorageBuffer %arr_vec4_16\n"
			"    %arr_vec4_rt_sb_ptr = OpTypePointer StorageBuffer %arr_vec4_rt\n"
			"    %arr_vec4_16_wg_ptr = OpTypePointer Workgroup %arr_vec4_16\n"
			"             %func_decl = OpTypeFunction %void %arr_vec4_16_wg_ptr %_ptr_Function_uint\n"
			"                %Output = OpTypeStruct %arr_vec4_16 %arr_vec4_rt\n"
			"                %struct = OpTypeStruct %arr_vec4_16 %arr_vec4_16\n"
			"        %_ptr_sb_struct = OpTypePointer StorageBuffer %Output\n"
			"        %_ptr_wg_struct = OpTypePointer Workgroup %struct\n"
			"            %dataOutput = OpVariable %_ptr_sb_struct StorageBuffer\n"
			"            %sharedData = OpVariable %_ptr_wg_struct Workgroup\n"
			"                   %int = OpTypeInt 32 1\n"
			"                 %int_0 = OpConstant %int 0\n"
			"                 %int_1 = OpConstant %int 1\n"
			"                %v3uint = OpTypeVector %uint 3\n"
			"     %_ptr_Input_v3uint = OpTypePointer Input %v3uint\n"
			"  %gl_LocalInvocationID = OpVariable %_ptr_Input_v3uint Input\n"
			"                %uint_0 = OpConstant %uint 0\n"
			"       %_ptr_Input_uint = OpTypePointer Input %uint\n"
			"          %_ptr_sb_vec4 = OpTypePointer StorageBuffer %vec4\n"
			"          %_ptr_wg_vec4 = OpTypePointer Workgroup %vec4\n"
			"                  %main = OpFunction %void None %void_func\n"
			"                 %entry = OpLabel\n"
			"                   %idx = OpVariable %_ptr_Function_uint Function\n"
			"            %inv_id_ptr = OpAccessChain %_ptr_Input_uint %gl_LocalInvocationID %uint_0\n"
			"                %inv_id = OpLoad %uint %inv_id_ptr\n"
			"                          OpStore %idx %inv_id\n"
			"                  %ptr0 = OpAccessChain %arr_vec4_16_wg_ptr %sharedData %int_0\n"
			"                  %ptr1 = OpAccessChain %arr_vec4_16_wg_ptr %sharedData %int_1\n"
			"                  %ret0 = OpFunctionCall %void %func0 %ptr0 %idx\n"
			"                  %ret1 = OpFunctionCall %void %func1 %ptr1 %idx\n"
			"                          OpControlBarrier %uint_2 %uint_2 %uint_264\n"
			"          %inv_id_plus1 = OpIAdd %uint %inv_id %uint_1\n"
			"            %inv_id_mod = OpUMod %uint %inv_id_plus1 %uint_16\n"
			"       %shared_arr1_ptr = OpAccessChain %_ptr_wg_vec4 %sharedData %int_1 %inv_id_mod\n"
			"      %shared_arr1_data = OpLoad %vec4 %shared_arr1_ptr\n"
			"               %outPtr0 = OpAccessChain %_ptr_sb_vec4 %dataOutput %int_0 %inv_id\n"
			"                          OpStore %outPtr0 %shared_arr1_data\n"
			"       %shared_arr0_ptr = OpAccessChain %_ptr_wg_vec4 %sharedData %int_0 %inv_id_mod\n"
			"      %shared_arr0_data = OpLoad %vec4 %shared_arr0_ptr\n"
			"               %outPtr1 = OpAccessChain %_ptr_sb_vec4 %dataOutput %int_1 %inv_id\n"
			"                          OpStore %outPtr1 %shared_arr0_data\n"
			"                          OpReturn\n"
			"                          OpFunctionEnd\n"
			"                 %func0 = OpFunction %void None %func_decl\n"
			"                    %f0 = OpFunctionParameter %arr_vec4_16_wg_ptr\n"
			"                    %i0 = OpFunctionParameter %_ptr_Function_uint\n"
			"           %func0_entry = OpLabel\n"
			"                  %idx0 = OpLoad %uint %i0\n"
			"              %out_ptr0 = OpAccessChain %_ptr_wg_vec4 %f0 %idx0\n"
			"             %idxFloat0 = OpConvertUToF %float %idx0\n"
			"              %outData0 = OpCompositeConstruct %vec4 %idxFloat0 %idxFloat0 %idxFloat0 %idxFloat0\n"
			"                          OpStore %out_ptr0 %outData0\n"
			"                          OpReturn\n"
			"                          OpFunctionEnd\n"
			"                 %func1 = OpFunction %void None %func_decl\n"
			"                    %f1 = OpFunctionParameter %arr_vec4_16_wg_ptr\n"
			"                    %i1 = OpFunctionParameter %_ptr_Function_uint\n"
			"           %func1_entry = OpLabel\n"
			"                  %idx1 = OpLoad %uint %i1\n"
			"              %out_ptr1 = OpAccessChain %_ptr_wg_vec4 %f1 %idx1\n"
			"              %idxPlus5 = OpIAdd %uint %idx1 %uint_5\n"
			"             %idxFloat1 = OpConvertUToF %float %idxPlus5\n"
			"              %outData1 = OpCompositeConstruct %vec4 %idxFloat1 %idxFloat1 %idxFloat1 %idxFloat1\n"
			"                          OpStore %out_ptr1 %outData1\n"
			"                          OpReturn\n"
			"                          OpFunctionEnd\n";

	expectedOutput.reserve(numFloats);
	for (deUint32 vecIdx = 0; vecIdx < numFloats / 8; ++vecIdx)
	{
		const deUint32	shuffleIdx	= (vecIdx + 1) % 16;
		const float		val			= (float)(shuffleIdx + 5);
		for (deUint32 i = 0; i < 4; ++i)
			expectedOutput.push_back(val);
	}
	for (deUint32 vecIdx = 0; vecIdx < numFloats / 8; ++vecIdx)
	{
		const deUint32	shuffleIdx	= (vecIdx + 1) % 16;
		const float		val			= (float)shuffleIdx;
		for (deUint32 i = 0; i < 4; ++i)
			expectedOutput.push_back(val);
	}

	spec.outputs.push_back(BufferSp(new Float32Buffer(expectedOutput)));
	requiredFeatures.extVariablePointers = EXTVARIABLEPOINTERSFEATURES_VARIABLE_POINTERS;
	spec.extensions.push_back("VK_KHR_variable_pointers");

	spec.assembly					= shaderSource;
	spec.numWorkGroups				= IVec3(1, 1, 1);
	spec.requestedVulkanFeatures	= requiredFeatures;

	group->addChild(new SpvAsmComputeShaderCase(testCtx, "workgroup_memory_variable_pointers", "", spec));
}

void addGraphicsPointerParamToParamTest (tcu::TestCaseGroup* group)
{
	map<string, string>	fragments;
	RGBA				defaultColors[4];
	GraphicsResources	resources;
	vector<string>		extensions;
	vector<float>		expectedOutput;
	VulkanFeatures		requiredFeatures;

	// Implements the following pseudo GLSL shader:
	//
	//	float func(alias float* f, alias float* g)
	//	{
	//		*g = 5.0;
	//		*f = 2.0;
	//		return *g;
	//	}
	//
	//	vec4 test_code(vec4 param)
	//	{
	//		float a = 0.0;
	//		o = func(&a, &a);  // should return 2.0
	//		float b = 0.0;
	//		o += func(&a, &b); // should return 5.0
	//		return param;
	//	}
	fragments["pre_main"]	=
		"            %func0_decl = OpTypeFunction %f32 %fp_f32 %fp_f32\n"
		"               %c_f32_5 = OpConstant %f32 5\n"
		"               %c_f32_2 = OpConstant %f32 2\n"
		"                %Output = OpTypeStruct %f32\n"
		"   %_ptr_Uniform_Output = OpTypePointer Uniform %Output\n"
		"            %dataOutput = OpVariable %_ptr_Uniform_Output Uniform\n"
		"      %_ptr_Uniform_f32 = OpTypePointer Uniform %f32\n"
		"                  %func = OpFunction %f32 None %func0_decl\n"
		"                     %f = OpFunctionParameter %fp_f32\n"
		"                     %g = OpFunctionParameter %fp_f32\n"
		"            %func_entry = OpLabel\n"
		"                          OpStore %g %c_f32_5\n"
		"                          OpStore %f %c_f32_2\n"
		"                   %ret = OpLoad %f32 %g\n"
		"                          OpReturnValue %ret\n"
		"                          OpFunctionEnd\n";

	fragments["decoration"]	=
		"                          OpMemberDecorate %Output 0 Offset 0\n"
		"                          OpDecorate %Output BufferBlock\n"
		"                          OpDecorate %dataOutput DescriptorSet 0\n"
		"                          OpDecorate %dataOutput Binding 0\n"
		"                          OpDecorate %f Aliased\n"
		"                          OpDecorate %g Aliased\n";

	fragments["testfun"]	=
		"             %test_code = OpFunction %v4f32 None %v4f32_v4f32_function\n"
		"                 %param = OpFunctionParameter %v4f32\n"
		"                 %entry = OpLabel\n"
		"                     %a = OpVariable %fp_f32 Function %c_f32_0\n"
		"                     %b = OpVariable %fp_f32 Function %c_f32_0\n"
		"                     %o = OpVariable %fp_f32 Function %c_f32_0\n"
		"                  %ret0 = OpFunctionCall %f32 %func %a %a\n"
		"                          OpStore %o %ret0\n"
		"                  %ret1 = OpFunctionCall %f32 %func %a %b\n"
		"                 %o_val = OpLoad %f32 %o\n"
		"                   %sum = OpFAdd %f32 %o_val %ret1\n"
		"               %out_ptr = OpAccessChain %_ptr_Uniform_f32 %dataOutput %c_i32_0\n"
		"                          OpStore %out_ptr %sum\n"
		"                          OpReturnValue %param\n"
		"                          OpFunctionEnd\n";

	getDefaultColors(defaultColors);
	expectedOutput.push_back(7.0f);
	requiredFeatures.coreFeatures.vertexPipelineStoresAndAtomics	= true;
	requiredFeatures.coreFeatures.fragmentStoresAndAtomics			= true;
	resources.outputs.push_back(Resource(BufferSp(new Float32Buffer(expectedOutput)), vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER));

	createTestsForAllStages("global_to_param", defaultColors, defaultColors, fragments, resources, extensions, group, requiredFeatures);
}

void addGraphicsPointerParamToGlobalTest (tcu::TestCaseGroup* group)
{
	map<string, string>	fragments;
	RGBA				defaultColors[4];
	GraphicsResources	resources;
	vector<string>		extensions;
	vector<float>		expectedOutput;
	VulkanFeatures		requiredFeatures;

	// Implements the following pseudo GLSL shader:
	//
	//	alias float a = 0.0;
	//
	//	float func0(alias float* f0) // f in Private storage class
	//	{
	//		*a = 5.0;
	//		*f0 = 2.0;
	//		return *a;
	//	}
	//
	//	float func1(alias float* f1) // f in Function storage class
	//	{
	//		*a = 5.0;
	//		*f1 = 2.0;
	//		return *a;
	//	}
	//
	//	vec4 test_code(vec4 param)
	//	{
	//		o = func0(&a);  // should return 2.0
	//		float b = 0.0;
	//		o += func1(&b); // should return 5.0
	//		return param;
	//	}
	fragments["pre_main"] =
		"                %pp_f32 = OpTypePointer Private %f32\n"
		"            %func0_decl = OpTypeFunction %f32 %pp_f32\n"
		"            %func1_decl = OpTypeFunction %f32 %fp_f32\n"
		"               %c_f32_5 = OpConstant %f32 5\n"
		"               %c_f32_2 = OpConstant %f32 2\n"
		"                %Output = OpTypeStruct %f32\n"
		"   %_ptr_Uniform_Output = OpTypePointer Uniform %Output\n"
		"            %dataOutput = OpVariable %_ptr_Uniform_Output Uniform\n"
		"      %_ptr_Uniform_f32 = OpTypePointer Uniform %f32\n"
		"                     %a = OpVariable %pp_f32 Private %c_f32_0\n"
		"                 %func0 = OpFunction %f32 None %func0_decl\n"
		"                    %f0 = OpFunctionParameter %pp_f32\n"
		"           %func0_entry = OpLabel\n"
		"                          OpStore %a %c_f32_5\n"
		"                          OpStore %f0 %c_f32_2\n"
		"             %func0_ret = OpLoad %f32 %a\n"
		"                          OpReturnValue %func0_ret\n"
		"                          OpFunctionEnd\n"
		"                 %func1 = OpFunction %f32 None %func1_decl\n"
		"                    %f1 = OpFunctionParameter %fp_f32\n"
		"           %func1_entry = OpLabel\n"
		"                          OpStore %a %c_f32_5\n"
		"                          OpStore %f1 %c_f32_2\n"
		"             %func1_ret = OpLoad %f32 %a\n"
		"                          OpReturnValue %func1_ret\n"
		"                          OpFunctionEnd\n";

	fragments["decoration"] =
		"                          OpMemberDecorate %Output 0 Offset 0\n"
		"                          OpDecorate %Output BufferBlock\n"
		"                          OpDecorate %dataOutput DescriptorSet 0\n"
		"                          OpDecorate %dataOutput Binding 0\n"
		"                          OpDecorate %f0 Aliased\n"
		"                          OpDecorate %f1 Aliased\n"
		"                          OpDecorate %a Aliased\n";

	fragments["testfun"] =
		"             %test_code = OpFunction %v4f32 None %v4f32_v4f32_function\n"
		"                 %param = OpFunctionParameter %v4f32\n"
		"                 %entry = OpLabel\n"
		"                     %b = OpVariable %fp_f32 Function %c_f32_0\n"
		"                     %o = OpVariable %fp_f32 Function %c_f32_0\n"
		"                  %ret0 = OpFunctionCall %f32 %func0 %a\n"
		"                          OpStore %o %ret0\n"
		"                  %ret1 = OpFunctionCall %f32 %func1 %b\n"
		"                 %o_val = OpLoad %f32 %o\n"
		"                   %sum = OpFAdd %f32 %o_val %ret1\n"
		"               %out_ptr = OpAccessChain %_ptr_Uniform_f32 %dataOutput %c_i32_0\n"
		"                          OpStore %out_ptr %sum\n"
		"                          OpReturnValue %param\n"
		"                          OpFunctionEnd\n";

	getDefaultColors(defaultColors);
	expectedOutput.push_back(7.0f);
	requiredFeatures.coreFeatures.vertexPipelineStoresAndAtomics	= true;
	requiredFeatures.coreFeatures.fragmentStoresAndAtomics			= true;
	resources.outputs.push_back(Resource(BufferSp(new Float32Buffer(expectedOutput)), vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER));

	createTestsForAllStages("param_to_global", defaultColors, defaultColors, fragments, resources, extensions, group, requiredFeatures);
}

void addGraphicsPointerBufferMemoryTest (tcu::TestCaseGroup* group)
{
	const int			numFloats			= 16;
	map<string, string>	fragments;
	RGBA				defaultColors[4];
	GraphicsResources	resources;
	vector<string>		extensions;
	vector<float>		expectedOutput;
	VulkanFeatures		requiredFeatures;

	// Implements the following pseudo GLSL shader:
	//
	//	layout (binding = 0) buffer Output
	//	{
	//		vec4 arr0[2];
	//		vec4 arr1[];
	//	} dataOutput;
	//
	//	void func0(vec4* f0[2], uint i)
	//	{
	//		f0[i] = vec4(5.0);
	//	}
	//
	//	void func1(vec4* f1[], uint i)
	//	{
	//		f1[i] = vec4(2.0);
	//	}
	//
	//	vec4 test_code(vec4 param)
	//	{
	//		func0(dataOutput.arr0, idx);
	//		func1(dataOutput.arr1, idx);
	//		return param;
	//	}
	fragments["pre_main"]	=
		"           %arr_v4f32_2 = OpTypeArray %v4f32 %c_u32_2\n"
		"          %arr_v4f32_rt = OpTypeRuntimeArray %v4f32\n"
		"       %arr_v4f32_2_ptr = OpTypePointer StorageBuffer %arr_v4f32_2\n"
		"      %arr_v4f32_rt_ptr = OpTypePointer StorageBuffer %arr_v4f32_rt\n"
		"            %func0_decl = OpTypeFunction %void %arr_v4f32_2_ptr\n"
		"            %func1_decl = OpTypeFunction %void %arr_v4f32_rt_ptr\n"
		"               %c_f32_5 = OpConstant %f32 5\n"
		"               %c_f32_2 = OpConstant %f32 2\n"
		"             %c_v4f32_5 = OpConstantComposite %v4f32 %c_f32_5 %c_f32_5 %c_f32_5 %c_f32_5\n"
		"             %c_v4f32_2 = OpConstantComposite %v4f32 %c_f32_2 %c_f32_2 %c_f32_2 %c_f32_2\n"
		"                %Output = OpTypeStruct %arr_v4f32_2 %arr_v4f32_rt\n"
		"        %_ptr_sb_Output = OpTypePointer StorageBuffer %Output\n"
		"            %dataOutput = OpVariable %_ptr_sb_Output StorageBuffer\n"
		"         %_ptr_sb_v4f32 = OpTypePointer StorageBuffer %v4f32\n"
		"                 %func0 = OpFunction %void None %func0_decl\n"
		"                    %f0 = OpFunctionParameter %arr_v4f32_2_ptr\n"
		"            %func0Entry = OpLabel\n"
		"              %out_ptr0 = OpAccessChain %_ptr_sb_v4f32 %f0 %c_i32_0\n"
		"                          OpStore %out_ptr0 %c_v4f32_5\n"
		"              %out_ptr1 = OpAccessChain %_ptr_sb_v4f32 %f0 %c_i32_1\n"
		"                          OpStore %out_ptr1 %c_v4f32_5\n"
		"                          OpReturn\n"
		"                          OpFunctionEnd\n"
		"                 %func1 = OpFunction %void None %func1_decl\n"
		"                    %f1 = OpFunctionParameter %arr_v4f32_rt_ptr\n"
		"            %func1Entry = OpLabel\n"
		"              %out_ptr2 = OpAccessChain %_ptr_sb_v4f32 %f1 %c_i32_0\n"
		"                          OpStore %out_ptr2 %c_v4f32_2\n"
		"              %out_ptr3 = OpAccessChain %_ptr_sb_v4f32 %f1 %c_i32_1\n"
		"                          OpStore %out_ptr3 %c_v4f32_2\n"
		"                          OpReturn\n"
		"                          OpFunctionEnd\n";

	fragments["decoration"]	=
		"                          OpMemberDecorate %Output 0 Offset 0\n"
		"                          OpMemberDecorate %Output 1 Offset 32\n"
		"                          OpDecorate %Output Block\n"
		"                          OpDecorate %dataOutput DescriptorSet 0\n"
		"                          OpDecorate %dataOutput Binding 0\n"
		"                          OpDecorate %arr_v4f32_2 ArrayStride 16\n"
		"                          OpDecorate %arr_v4f32_rt ArrayStride 16\n";

	fragments["testfun"]	=
		"             %test_code = OpFunction %v4f32 None %v4f32_v4f32_function\n"
		"                 %param = OpFunctionParameter %v4f32\n"
		"                 %entry = OpLabel\n"
		"                  %ptr0 = OpAccessChain %arr_v4f32_2_ptr %dataOutput %c_i32_0\n"
		"                  %ptr1 = OpAccessChain %arr_v4f32_rt_ptr %dataOutput %c_i32_1\n"
		"                  %ret0 = OpFunctionCall %void %func0 %ptr0\n"
		"                  %ret1 = OpFunctionCall %void %func1 %ptr1\n"
		"                          OpReturnValue %param\n"
		"                          OpFunctionEnd\n";

	fragments["extension"]	=
		"OpExtension \"SPV_KHR_storage_buffer_storage_class\"\n"
		"OpExtension \"SPV_KHR_variable_pointers\"\n";

	fragments["capability"] =
		"OpCapability VariablePointersStorageBuffer\n";

	getDefaultColors(defaultColors);

	for (deUint32 numIdx = 0; numIdx < numFloats / 2; ++numIdx)
		expectedOutput.push_back(5.0f);
	for (deUint32 numIdx = 0; numIdx < numFloats / 2; ++numIdx)
		expectedOutput.push_back(2.0f);

	extensions.push_back("VK_KHR_variable_pointers");
	requiredFeatures.coreFeatures.vertexPipelineStoresAndAtomics	= true;
	requiredFeatures.coreFeatures.fragmentStoresAndAtomics			= true;
	requiredFeatures.extVariablePointers							= EXTVARIABLEPOINTERSFEATURES_VARIABLE_POINTERS_STORAGEBUFFER;
	resources.outputs.push_back(Resource(BufferSp(new Float32Buffer(expectedOutput)), vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER));

	createTestsForAllStages("buffer_memory", defaultColors, defaultColors, fragments, resources, extensions, group, requiredFeatures);
}

void addGraphicsPointerBufferMemoryVariablePointersTest (tcu::TestCaseGroup* group)
{
	const int			numFloats			= 16;
	map<string, string>	fragments;
	RGBA				defaultColors[4];
	GraphicsResources	resources;
	vector<string>		extensions;
	vector<float>		expectedOutput;
	VulkanFeatures		requiredFeatures;

	// Implements the following pseudo GLSL shader:
	//
	//	layout (binding = 0) buffer Output
	//	{
	//		vec4 arr0[2];
	//		vec4 arr1[];
	//	} dataOutput;
	//
	//	void func0(vec4* f0[2], uint i)
	//	{
	//		f0[i] = vec4(5.0);
	//	}
	//
	//	void func1(vec4* f1[], uint i)
	//	{
	//		f1[i] = vec4(2.0);
	//	}
	//
	//	vec4 test_code(vec4 param)
	//	{
	//		func0(dataOutput.arr0, idx);
	//		func1(dataOutput.arr1, idx);
	//		return param;
	//	}
	fragments["pre_main"]	=
		"           %arr_v4f32_2 = OpTypeArray %v4f32 %c_u32_2\n"
		"          %arr_v4f32_rt = OpTypeRuntimeArray %v4f32\n"
		"       %arr_v4f32_2_ptr = OpTypePointer StorageBuffer %arr_v4f32_2\n"
		"      %arr_v4f32_rt_ptr = OpTypePointer StorageBuffer %arr_v4f32_rt\n"
		"            %func0_decl = OpTypeFunction %void %arr_v4f32_2_ptr\n"
		"            %func1_decl = OpTypeFunction %void %arr_v4f32_rt_ptr\n"
		"               %c_f32_5 = OpConstant %f32 5\n"
		"               %c_f32_2 = OpConstant %f32 2\n"
		"             %c_v4f32_5 = OpConstantComposite %v4f32 %c_f32_5 %c_f32_5 %c_f32_5 %c_f32_5\n"
		"             %c_v4f32_2 = OpConstantComposite %v4f32 %c_f32_2 %c_f32_2 %c_f32_2 %c_f32_2\n"
		"                %Output = OpTypeStruct %arr_v4f32_2 %arr_v4f32_rt\n"
		"        %_ptr_sb_Output = OpTypePointer StorageBuffer %Output\n"
		"            %dataOutput = OpVariable %_ptr_sb_Output StorageBuffer\n"
		"         %_ptr_sb_v4f32 = OpTypePointer StorageBuffer %v4f32\n"
		"                 %func0 = OpFunction %void None %func0_decl\n"
		"                    %f0 = OpFunctionParameter %arr_v4f32_2_ptr\n"
		"            %func0Entry = OpLabel\n"
		"              %out_ptr0 = OpAccessChain %_ptr_sb_v4f32 %f0 %c_i32_0\n"
		"                          OpStore %out_ptr0 %c_v4f32_5\n"
		"              %out_ptr1 = OpAccessChain %_ptr_sb_v4f32 %f0 %c_i32_1\n"
		"                          OpStore %out_ptr1 %c_v4f32_5\n"
		"                          OpReturn\n"
		"                          OpFunctionEnd\n"
		"                 %func1 = OpFunction %void None %func1_decl\n"
		"                    %f1 = OpFunctionParameter %arr_v4f32_rt_ptr\n"
		"            %func1Entry = OpLabel\n"
		"              %out_ptr2 = OpAccessChain %_ptr_sb_v4f32 %f1 %c_i32_0\n"
		"                          OpStore %out_ptr2 %c_v4f32_2\n"
		"              %out_ptr3 = OpAccessChain %_ptr_sb_v4f32 %f1 %c_i32_1\n"
		"                          OpStore %out_ptr3 %c_v4f32_2\n"
		"                          OpReturn\n"
		"                          OpFunctionEnd\n";

	fragments["decoration"]	=
		"                          OpMemberDecorate %Output 0 Offset 0\n"
		"                          OpMemberDecorate %Output 1 Offset 32\n"
		"                          OpDecorate %Output Block\n"
		"                          OpDecorate %dataOutput DescriptorSet 0\n"
		"                          OpDecorate %dataOutput Binding 0\n"
		"                          OpDecorate %arr_v4f32_2 ArrayStride 16\n"
		"                          OpDecorate %arr_v4f32_rt ArrayStride 16\n";

	fragments["testfun"]	=
		"             %test_code = OpFunction %v4f32 None %v4f32_v4f32_function\n"
		"                 %param = OpFunctionParameter %v4f32\n"
		"                 %entry = OpLabel\n"
		"                  %ptr0 = OpAccessChain %arr_v4f32_2_ptr %dataOutput %c_i32_0\n"
		"                  %ptr1 = OpAccessChain %arr_v4f32_rt_ptr %dataOutput %c_i32_1\n"
		"                  %ret0 = OpFunctionCall %void %func0 %ptr0\n"
		"                  %ret1 = OpFunctionCall %void %func1 %ptr1\n"
		"                          OpReturnValue %param\n"
		"                          OpFunctionEnd\n";

	fragments["extension"]	=
		"OpExtension \"SPV_KHR_variable_pointers\"\n"
		"OpExtension \"SPV_KHR_storage_buffer_storage_class\"\n";

	fragments["capability"]	=
		"OpCapability VariablePointersStorageBuffer\n";

	getDefaultColors(defaultColors);

	for (deUint32 numIdx = 0; numIdx < numFloats / 2; ++numIdx)
		expectedOutput.push_back(5.0f);
	for (deUint32 numIdx = 0; numIdx < numFloats / 2; ++numIdx)
		expectedOutput.push_back(2.0f);

	extensions.push_back("VK_KHR_variable_pointers");
	requiredFeatures.coreFeatures.fragmentStoresAndAtomics			= true;
	requiredFeatures.extVariablePointers = EXTVARIABLEPOINTERSFEATURES_VARIABLE_POINTERS_STORAGEBUFFER;
	requiredFeatures.coreFeatures.vertexPipelineStoresAndAtomics = DE_TRUE;
	resources.outputs.push_back(Resource(BufferSp(new Float32Buffer(expectedOutput)), vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER));

	createTestsForAllStages("buffer_memory_variable_pointers", defaultColors, defaultColors, fragments, resources, extensions, group, requiredFeatures);
}

} // anonymous

tcu::TestCaseGroup* createPointerParameterComputeGroup (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(testCtx, "pointer_parameter", "Compute tests for pointer as function parameter."));
	addComputePointerParamToParamTest(group.get());
	addComputePointerParamToGlobalTest(group.get());
	addComputePointerBufferMemoryTest(group.get());
	addComputePointerBufferMemoryVariablePointersTest(group.get());
	addComputePointerWorkgroupMemoryVariablePointersTest(group.get());

	return group.release();
}

tcu::TestCaseGroup* createPointerParameterGraphicsGroup (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(testCtx, "pointer_parameter", "Graphics tests for pointer as function parameter."));
	addGraphicsPointerParamToParamTest(group.get());
	addGraphicsPointerParamToGlobalTest(group.get());
	addGraphicsPointerBufferMemoryTest(group.get());
	addGraphicsPointerBufferMemoryVariablePointersTest(group.get());

	return group.release();
}

} // SpirVAssembly
} // vkt
