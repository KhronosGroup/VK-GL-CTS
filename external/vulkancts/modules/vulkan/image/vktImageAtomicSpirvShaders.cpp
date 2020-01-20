/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2020 Valve Corporation.
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
 * \brief Helper SPIR-V shaders for some image atomic operations.
 *//*--------------------------------------------------------------------*/

#include "vktImageAtomicSpirvShaders.hpp"

#include <array>
#include <map>

namespace vkt
{
namespace image
{

namespace
{

const std::string kShader_1d_r32ui_end_result = R"(
; The SPIR-V shader below is based on the following GLSL shader, but OpAtomicIAdd has been
; replaced with a template parameter and the last argument for it has been made optional.
;
; #version 440
; precision highp uimage1D;
;
; layout (local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
; layout (r32ui, binding=0) coherent uniform uimage1D u_resultImage;
;
; void main (void)
; {
;     int gx = int(gl_GlobalInvocationID.x);
;     int gy = int(gl_GlobalInvocationID.y);
;     int gz = int(gl_GlobalInvocationID.z);
;     imageAtomicAdd(u_resultImage, gx % 64, uint(gx*gx + gy*gy + gz*gz));
; }
;
; SPIR-V
; Version: 1.0
; Generator: Khronos Glslang Reference Front End; 7
; Bound: 50
; Schema: 0
OpCapability Shader
OpCapability Image1D
%1 = OpExtInstImport "GLSL.std.450"
OpMemoryModel Logical GLSL450
OpEntryPoint GLCompute %4 "main" %12
OpExecutionMode %4 LocalSize 1 1 1
OpDecorate %12 BuiltIn GlobalInvocationId
OpDecorate %30 DescriptorSet 0
OpDecorate %30 Binding 0
OpDecorate %30 Coherent
OpDecorate %49 BuiltIn WorkgroupSize
%2 = OpTypeVoid
%3 = OpTypeFunction %2
%6 = OpTypeInt 32 1
%7 = OpTypePointer Function %6
%9 = OpTypeInt 32 0
%10 = OpTypeVector %9 3
%11 = OpTypePointer Input %10
%12 = OpVariable %11 Input
%13 = OpConstant %9 0
%14 = OpTypePointer Input %9
%19 = OpConstant %9 1
%24 = OpConstant %9 2
%28 = OpTypeImage %9 1D 0 0 0 2 R32ui
%29 = OpTypePointer UniformConstant %28
%30 = OpVariable %29 UniformConstant
%32 = OpConstant %6 64
%46 = OpTypePointer Image %9
%49 = OpConstantComposite %10 %19 %19 %19
%4 = OpFunction %2 None %3
%5 = OpLabel
%8 = OpVariable %7 Function
%18 = OpVariable %7 Function
%23 = OpVariable %7 Function
%15 = OpAccessChain %14 %12 %13
%16 = OpLoad %9 %15
%17 = OpBitcast %6 %16
OpStore %8 %17
%20 = OpAccessChain %14 %12 %19
%21 = OpLoad %9 %20
%22 = OpBitcast %6 %21
OpStore %18 %22
%25 = OpAccessChain %14 %12 %24
%26 = OpLoad %9 %25
%27 = OpBitcast %6 %26
OpStore %23 %27
%31 = OpLoad %6 %8
%33 = OpSMod %6 %31 %32
%34 = OpLoad %6 %8
%35 = OpLoad %6 %8
%36 = OpIMul %6 %34 %35
%37 = OpLoad %6 %18
%38 = OpLoad %6 %18
%39 = OpIMul %6 %37 %38
%40 = OpIAdd %6 %36 %39
%41 = OpLoad %6 %23
%42 = OpLoad %6 %23
%43 = OpIMul %6 %41 %42
%44 = OpIAdd %6 %40 %43
%45 = OpBitcast %9 %44
%47 = OpImageTexelPointer %46 %30 %33 %13
%48 = ${OPNAME} %9 %47 %19 %13 ${LASTARG:default=%45}
OpReturn
OpFunctionEnd
)";

const std::string kShader_1d_r32ui_intermediate_values = R"(
; The SPIR-V shader below is based on the following GLSL shader, but OpAtomicIAdd has been
; replaced with a template parameter and the last argument for it has been made optional.
;
; #version 440
; precision highp uimage1D;
;
; layout (local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
; layout (r32ui, binding=0) coherent uniform uimage1D u_resultImage;
; layout (r32ui, binding=1) writeonly uniform uimage1D u_intermValuesImage;
;
; void main (void)
; {
;     int gx = int(gl_GlobalInvocationID.x);
;     int gy = int(gl_GlobalInvocationID.y);
;     int gz = int(gl_GlobalInvocationID.z);
;     imageStore(u_intermValuesImage, gx, uvec4(imageAtomicAdd(u_resultImage, gx % 64, uint(gx*gx + gy*gy + gz*gz))));
; }
;
; SPIR-V
; Version: 1.0
; Generator: Khronos Glslang Reference Front End; 7
; Bound: 55
; Schema: 0
OpCapability Shader
OpCapability Image1D
%1 = OpExtInstImport "GLSL.std.450"
OpMemoryModel Logical GLSL450
OpEntryPoint GLCompute %4 "main" %12
OpExecutionMode %4 LocalSize 1 1 1
OpDecorate %12 BuiltIn GlobalInvocationId
OpDecorate %30 DescriptorSet 0
OpDecorate %30 Binding 1
OpDecorate %30 NonReadable
OpDecorate %33 DescriptorSet 0
OpDecorate %33 Binding 0
OpDecorate %33 Coherent
OpDecorate %54 BuiltIn WorkgroupSize
%2 = OpTypeVoid
%3 = OpTypeFunction %2
%6 = OpTypeInt 32 1
%7 = OpTypePointer Function %6
%9 = OpTypeInt 32 0
%10 = OpTypeVector %9 3
%11 = OpTypePointer Input %10
%12 = OpVariable %11 Input
%13 = OpConstant %9 0
%14 = OpTypePointer Input %9
%19 = OpConstant %9 1
%24 = OpConstant %9 2
%28 = OpTypeImage %9 1D 0 0 0 2 R32ui
%29 = OpTypePointer UniformConstant %28
%30 = OpVariable %29 UniformConstant
%33 = OpVariable %29 UniformConstant
%35 = OpConstant %6 64
%49 = OpTypePointer Image %9
%52 = OpTypeVector %9 4
%54 = OpConstantComposite %10 %19 %19 %19
%4 = OpFunction %2 None %3
%5 = OpLabel
%8 = OpVariable %7 Function
%18 = OpVariable %7 Function
%23 = OpVariable %7 Function
%15 = OpAccessChain %14 %12 %13
%16 = OpLoad %9 %15
%17 = OpBitcast %6 %16
OpStore %8 %17
%20 = OpAccessChain %14 %12 %19
%21 = OpLoad %9 %20
%22 = OpBitcast %6 %21
OpStore %18 %22
%25 = OpAccessChain %14 %12 %24
%26 = OpLoad %9 %25
%27 = OpBitcast %6 %26
OpStore %23 %27
%31 = OpLoad %28 %30
%32 = OpLoad %6 %8
%34 = OpLoad %6 %8
%36 = OpSMod %6 %34 %35
%37 = OpLoad %6 %8
%38 = OpLoad %6 %8
%39 = OpIMul %6 %37 %38
%40 = OpLoad %6 %18
%41 = OpLoad %6 %18
%42 = OpIMul %6 %40 %41
%43 = OpIAdd %6 %39 %42
%44 = OpLoad %6 %23
%45 = OpLoad %6 %23
%46 = OpIMul %6 %44 %45
%47 = OpIAdd %6 %43 %46
%48 = OpBitcast %9 %47
%50 = OpImageTexelPointer %49 %33 %36 %13
%51 = ${OPNAME} %9 %50 %19 %13 ${LASTARG:default=%48}
%53 = OpCompositeConstruct %52 %51 %51 %51 %51
OpImageWrite %31 %32 %53
OpReturn
OpFunctionEnd
)";

const std::string kShader_1d_r32i_end_result = R"(
; The SPIR-V shader below is based on the following GLSL shader, but OpAtomicIAdd has been
; replaced with a template parameter and the last argument for it has been made optional.
;
; #version 440
; precision highp iimage1D;
;
; layout (local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
; layout (r32i, binding=0) coherent uniform iimage1D u_resultImage;
;
; void main (void)
; {
;     int gx = int(gl_GlobalInvocationID.x);
;     int gy = int(gl_GlobalInvocationID.y);
;     int gz = int(gl_GlobalInvocationID.z);
;     imageAtomicAdd(u_resultImage, gx % 64, int(gx*gx + gy*gy + gz*gz));
; }
;
; SPIR-V
; Version: 1.0
; Generator: Khronos Glslang Reference Front End; 7
; Bound: 49
; Schema: 0
OpCapability Shader
OpCapability Image1D
%1 = OpExtInstImport "GLSL.std.450"
OpMemoryModel Logical GLSL450
OpEntryPoint GLCompute %4 "main" %12
OpExecutionMode %4 LocalSize 1 1 1
OpDecorate %12 BuiltIn GlobalInvocationId
OpDecorate %30 DescriptorSet 0
OpDecorate %30 Binding 0
OpDecorate %30 Coherent
OpDecorate %48 BuiltIn WorkgroupSize
%2 = OpTypeVoid
%3 = OpTypeFunction %2
%6 = OpTypeInt 32 1
%7 = OpTypePointer Function %6
%9 = OpTypeInt 32 0
%10 = OpTypeVector %9 3
%11 = OpTypePointer Input %10
%12 = OpVariable %11 Input
%13 = OpConstant %9 0
%14 = OpTypePointer Input %9
%19 = OpConstant %9 1
%24 = OpConstant %9 2
%28 = OpTypeImage %6 1D 0 0 0 2 R32i
%29 = OpTypePointer UniformConstant %28
%30 = OpVariable %29 UniformConstant
%32 = OpConstant %6 64
%45 = OpTypePointer Image %6
%48 = OpConstantComposite %10 %19 %19 %19
%4 = OpFunction %2 None %3
%5 = OpLabel
%8 = OpVariable %7 Function
%18 = OpVariable %7 Function
%23 = OpVariable %7 Function
%15 = OpAccessChain %14 %12 %13
%16 = OpLoad %9 %15
%17 = OpBitcast %6 %16
OpStore %8 %17
%20 = OpAccessChain %14 %12 %19
%21 = OpLoad %9 %20
%22 = OpBitcast %6 %21
OpStore %18 %22
%25 = OpAccessChain %14 %12 %24
%26 = OpLoad %9 %25
%27 = OpBitcast %6 %26
OpStore %23 %27
%31 = OpLoad %6 %8
%33 = OpSMod %6 %31 %32
%34 = OpLoad %6 %8
%35 = OpLoad %6 %8
%36 = OpIMul %6 %34 %35
%37 = OpLoad %6 %18
%38 = OpLoad %6 %18
%39 = OpIMul %6 %37 %38
%40 = OpIAdd %6 %36 %39
%41 = OpLoad %6 %23
%42 = OpLoad %6 %23
%43 = OpIMul %6 %41 %42
%44 = OpIAdd %6 %40 %43
%46 = OpImageTexelPointer %45 %30 %33 %13
%47 = ${OPNAME} %6 %46 %19 %13 ${LASTARG:default=%44}
OpReturn
OpFunctionEnd
)";

const std::string kShader_1d_r32i_intermediate_values = R"(
; The SPIR-V shader below is based on the following GLSL shader, but OpAtomicIAdd has been
; replaced with a template parameter and the last argument for it has been made optional.
;
; #version 440
; precision highp iimage1D;
;
; layout (local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
; layout (r32i, binding=0) coherent uniform iimage1D u_resultImage;
; layout (r32i, binding=1) writeonly uniform iimage1D u_intermValuesImage;
;
; void main (void)
; {
;     int gx = int(gl_GlobalInvocationID.x);
;     int gy = int(gl_GlobalInvocationID.y);
;     int gz = int(gl_GlobalInvocationID.z);
;     imageStore(u_intermValuesImage, gx, ivec4(imageAtomicAdd(u_resultImage, gx % 64, int(gx*gx + gy*gy + gz*gz))));
; }
;
; SPIR-V
; Version: 1.0
; Generator: Khronos Glslang Reference Front End; 7
; Bound: 54
; Schema: 0
OpCapability Shader
OpCapability Image1D
%1 = OpExtInstImport "GLSL.std.450"
OpMemoryModel Logical GLSL450
OpEntryPoint GLCompute %4 "main" %12
OpExecutionMode %4 LocalSize 1 1 1
OpDecorate %12 BuiltIn GlobalInvocationId
OpDecorate %30 DescriptorSet 0
OpDecorate %30 Binding 1
OpDecorate %30 NonReadable
OpDecorate %33 DescriptorSet 0
OpDecorate %33 Binding 0
OpDecorate %33 Coherent
OpDecorate %53 BuiltIn WorkgroupSize
%2 = OpTypeVoid
%3 = OpTypeFunction %2
%6 = OpTypeInt 32 1
%7 = OpTypePointer Function %6
%9 = OpTypeInt 32 0
%10 = OpTypeVector %9 3
%11 = OpTypePointer Input %10
%12 = OpVariable %11 Input
%13 = OpConstant %9 0
%14 = OpTypePointer Input %9
%19 = OpConstant %9 1
%24 = OpConstant %9 2
%28 = OpTypeImage %6 1D 0 0 0 2 R32i
%29 = OpTypePointer UniformConstant %28
%30 = OpVariable %29 UniformConstant
%33 = OpVariable %29 UniformConstant
%35 = OpConstant %6 64
%48 = OpTypePointer Image %6
%51 = OpTypeVector %6 4
%53 = OpConstantComposite %10 %19 %19 %19
%4 = OpFunction %2 None %3
%5 = OpLabel
%8 = OpVariable %7 Function
%18 = OpVariable %7 Function
%23 = OpVariable %7 Function
%15 = OpAccessChain %14 %12 %13
%16 = OpLoad %9 %15
%17 = OpBitcast %6 %16
OpStore %8 %17
%20 = OpAccessChain %14 %12 %19
%21 = OpLoad %9 %20
%22 = OpBitcast %6 %21
OpStore %18 %22
%25 = OpAccessChain %14 %12 %24
%26 = OpLoad %9 %25
%27 = OpBitcast %6 %26
OpStore %23 %27
%31 = OpLoad %28 %30
%32 = OpLoad %6 %8
%34 = OpLoad %6 %8
%36 = OpSMod %6 %34 %35
%37 = OpLoad %6 %8
%38 = OpLoad %6 %8
%39 = OpIMul %6 %37 %38
%40 = OpLoad %6 %18
%41 = OpLoad %6 %18
%42 = OpIMul %6 %40 %41
%43 = OpIAdd %6 %39 %42
%44 = OpLoad %6 %23
%45 = OpLoad %6 %23
%46 = OpIMul %6 %44 %45
%47 = OpIAdd %6 %43 %46
%49 = OpImageTexelPointer %48 %33 %36 %13
%50 = ${OPNAME} %6 %49 %19 %13 ${LASTARG:default=%47}
%52 = OpCompositeConstruct %51 %50 %50 %50 %50
OpImageWrite %31 %32 %52
OpReturn
OpFunctionEnd
)";

const std::string kShader_1d_array_r32ui_end_result = R"(
; The SPIR-V shader below is based on the following GLSL shader, but OpAtomicIAdd has been
; replaced with a template parameter and the last argument for it has been made optional.
;
; #version 440
; precision highp uimage1DArray;
;
; layout (local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
; layout (r32ui, binding=0) coherent uniform uimage1DArray u_resultImage;
;
; void main (void)
; {
;     int gx = int(gl_GlobalInvocationID.x);
;     int gy = int(gl_GlobalInvocationID.y);
;     int gz = int(gl_GlobalInvocationID.z);
;     imageAtomicAdd(u_resultImage, ivec2(gx % 64,gy), uint(gx*gx + gy*gy + gz*gz));
; }
;
; SPIR-V
; Version: 1.0
; Generator: Khronos Glslang Reference Front End; 7
; Bound: 53
; Schema: 0
OpCapability Shader
OpCapability Image1D
%1 = OpExtInstImport "GLSL.std.450"
OpMemoryModel Logical GLSL450
OpEntryPoint GLCompute %4 "main" %12
OpExecutionMode %4 LocalSize 1 1 1
OpDecorate %12 BuiltIn GlobalInvocationId
OpDecorate %30 DescriptorSet 0
OpDecorate %30 Binding 0
OpDecorate %30 Coherent
OpDecorate %52 BuiltIn WorkgroupSize
%2 = OpTypeVoid
%3 = OpTypeFunction %2
%6 = OpTypeInt 32 1
%7 = OpTypePointer Function %6
%9 = OpTypeInt 32 0
%10 = OpTypeVector %9 3
%11 = OpTypePointer Input %10
%12 = OpVariable %11 Input
%13 = OpConstant %9 0
%14 = OpTypePointer Input %9
%19 = OpConstant %9 1
%24 = OpConstant %9 2
%28 = OpTypeImage %9 1D 0 1 0 2 R32ui
%29 = OpTypePointer UniformConstant %28
%30 = OpVariable %29 UniformConstant
%32 = OpConstant %6 64
%35 = OpTypeVector %6 2
%49 = OpTypePointer Image %9
%52 = OpConstantComposite %10 %19 %19 %19
%4 = OpFunction %2 None %3
%5 = OpLabel
%8 = OpVariable %7 Function
%18 = OpVariable %7 Function
%23 = OpVariable %7 Function
%15 = OpAccessChain %14 %12 %13
%16 = OpLoad %9 %15
%17 = OpBitcast %6 %16
OpStore %8 %17
%20 = OpAccessChain %14 %12 %19
%21 = OpLoad %9 %20
%22 = OpBitcast %6 %21
OpStore %18 %22
%25 = OpAccessChain %14 %12 %24
%26 = OpLoad %9 %25
%27 = OpBitcast %6 %26
OpStore %23 %27
%31 = OpLoad %6 %8
%33 = OpSMod %6 %31 %32
%34 = OpLoad %6 %18
%36 = OpCompositeConstruct %35 %33 %34
%37 = OpLoad %6 %8
%38 = OpLoad %6 %8
%39 = OpIMul %6 %37 %38
%40 = OpLoad %6 %18
%41 = OpLoad %6 %18
%42 = OpIMul %6 %40 %41
%43 = OpIAdd %6 %39 %42
%44 = OpLoad %6 %23
%45 = OpLoad %6 %23
%46 = OpIMul %6 %44 %45
%47 = OpIAdd %6 %43 %46
%48 = OpBitcast %9 %47
%50 = OpImageTexelPointer %49 %30 %36 %13
%51 = ${OPNAME} %9 %50 %19 %13 ${LASTARG:default=%48}
OpReturn
OpFunctionEnd
)";

const std::string kShader_1d_array_r32ui_intermediate_values = R"(
; The SPIR-V shader below is based on the following GLSL shader, but OpAtomicIAdd has been
; replaced with a template parameter and the last argument for it has been made optional.
;
; #version 440
; precision highp uimage1DArray;
;
; layout (local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
; layout (r32ui, binding=0) coherent uniform uimage1DArray u_resultImage;
; layout (r32ui, binding=1) writeonly uniform uimage1DArray u_intermValuesImage;
;
; void main (void)
; {
;     int gx = int(gl_GlobalInvocationID.x);
;     int gy = int(gl_GlobalInvocationID.y);
;     int gz = int(gl_GlobalInvocationID.z);
;     imageStore(u_intermValuesImage, ivec2(gx,gy), uvec4(imageAtomicAdd(u_resultImage, ivec2(gx % 64,gy), uint(gx*gx + gy*gy + gz*gz))));
; }
;
; SPIR-V
; Version: 1.0
; Generator: Khronos Glslang Reference Front End; 7
; Bound: 60
; Schema: 0
OpCapability Shader
OpCapability Image1D
%1 = OpExtInstImport "GLSL.std.450"
OpMemoryModel Logical GLSL450
OpEntryPoint GLCompute %4 "main" %12
OpExecutionMode %4 LocalSize 1 1 1
OpDecorate %12 BuiltIn GlobalInvocationId
OpDecorate %30 DescriptorSet 0
OpDecorate %30 Binding 1
OpDecorate %30 NonReadable
OpDecorate %36 DescriptorSet 0
OpDecorate %36 Binding 0
OpDecorate %36 Coherent
OpDecorate %59 BuiltIn WorkgroupSize
%2 = OpTypeVoid
%3 = OpTypeFunction %2
%6 = OpTypeInt 32 1
%7 = OpTypePointer Function %6
%9 = OpTypeInt 32 0
%10 = OpTypeVector %9 3
%11 = OpTypePointer Input %10
%12 = OpVariable %11 Input
%13 = OpConstant %9 0
%14 = OpTypePointer Input %9
%19 = OpConstant %9 1
%24 = OpConstant %9 2
%28 = OpTypeImage %9 1D 0 1 0 2 R32ui
%29 = OpTypePointer UniformConstant %28
%30 = OpVariable %29 UniformConstant
%34 = OpTypeVector %6 2
%36 = OpVariable %29 UniformConstant
%38 = OpConstant %6 64
%54 = OpTypePointer Image %9
%57 = OpTypeVector %9 4
%59 = OpConstantComposite %10 %19 %19 %19
%4 = OpFunction %2 None %3
%5 = OpLabel
%8 = OpVariable %7 Function
%18 = OpVariable %7 Function
%23 = OpVariable %7 Function
%15 = OpAccessChain %14 %12 %13
%16 = OpLoad %9 %15
%17 = OpBitcast %6 %16
OpStore %8 %17
%20 = OpAccessChain %14 %12 %19
%21 = OpLoad %9 %20
%22 = OpBitcast %6 %21
OpStore %18 %22
%25 = OpAccessChain %14 %12 %24
%26 = OpLoad %9 %25
%27 = OpBitcast %6 %26
OpStore %23 %27
%31 = OpLoad %28 %30
%32 = OpLoad %6 %8
%33 = OpLoad %6 %18
%35 = OpCompositeConstruct %34 %32 %33
%37 = OpLoad %6 %8
%39 = OpSMod %6 %37 %38
%40 = OpLoad %6 %18
%41 = OpCompositeConstruct %34 %39 %40
%42 = OpLoad %6 %8
%43 = OpLoad %6 %8
%44 = OpIMul %6 %42 %43
%45 = OpLoad %6 %18
%46 = OpLoad %6 %18
%47 = OpIMul %6 %45 %46
%48 = OpIAdd %6 %44 %47
%49 = OpLoad %6 %23
%50 = OpLoad %6 %23
%51 = OpIMul %6 %49 %50
%52 = OpIAdd %6 %48 %51
%53 = OpBitcast %9 %52
%55 = OpImageTexelPointer %54 %36 %41 %13
%56 = ${OPNAME} %9 %55 %19 %13 ${LASTARG:default=%53}
%58 = OpCompositeConstruct %57 %56 %56 %56 %56
OpImageWrite %31 %35 %58
OpReturn
OpFunctionEnd
)";

const std::string kShader_1d_array_r32i_end_result = R"(
; The SPIR-V shader below is based on the following GLSL shader, but OpAtomicIAdd has been
; replaced with a template parameter and the last argument for it has been made optional.
;
; #version 440
; precision highp iimage1DArray;
;
; layout (local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
; layout (r32i, binding=0) coherent uniform iimage1DArray u_resultImage;
;
; void main (void)
; {
;     int gx = int(gl_GlobalInvocationID.x);
;     int gy = int(gl_GlobalInvocationID.y);
;     int gz = int(gl_GlobalInvocationID.z);
;     imageAtomicAdd(u_resultImage, ivec2(gx % 64,gy), int(gx*gx + gy*gy + gz*gz));
; }
;
; SPIR-V
; Version: 1.0
; Generator: Khronos Glslang Reference Front End; 7
; Bound: 52
; Schema: 0
OpCapability Shader
OpCapability Image1D
%1 = OpExtInstImport "GLSL.std.450"
OpMemoryModel Logical GLSL450
OpEntryPoint GLCompute %4 "main" %12
OpExecutionMode %4 LocalSize 1 1 1
OpDecorate %12 BuiltIn GlobalInvocationId
OpDecorate %30 DescriptorSet 0
OpDecorate %30 Binding 0
OpDecorate %30 Coherent
OpDecorate %51 BuiltIn WorkgroupSize
%2 = OpTypeVoid
%3 = OpTypeFunction %2
%6 = OpTypeInt 32 1
%7 = OpTypePointer Function %6
%9 = OpTypeInt 32 0
%10 = OpTypeVector %9 3
%11 = OpTypePointer Input %10
%12 = OpVariable %11 Input
%13 = OpConstant %9 0
%14 = OpTypePointer Input %9
%19 = OpConstant %9 1
%24 = OpConstant %9 2
%28 = OpTypeImage %6 1D 0 1 0 2 R32i
%29 = OpTypePointer UniformConstant %28
%30 = OpVariable %29 UniformConstant
%32 = OpConstant %6 64
%35 = OpTypeVector %6 2
%48 = OpTypePointer Image %6
%51 = OpConstantComposite %10 %19 %19 %19
%4 = OpFunction %2 None %3
%5 = OpLabel
%8 = OpVariable %7 Function
%18 = OpVariable %7 Function
%23 = OpVariable %7 Function
%15 = OpAccessChain %14 %12 %13
%16 = OpLoad %9 %15
%17 = OpBitcast %6 %16
OpStore %8 %17
%20 = OpAccessChain %14 %12 %19
%21 = OpLoad %9 %20
%22 = OpBitcast %6 %21
OpStore %18 %22
%25 = OpAccessChain %14 %12 %24
%26 = OpLoad %9 %25
%27 = OpBitcast %6 %26
OpStore %23 %27
%31 = OpLoad %6 %8
%33 = OpSMod %6 %31 %32
%34 = OpLoad %6 %18
%36 = OpCompositeConstruct %35 %33 %34
%37 = OpLoad %6 %8
%38 = OpLoad %6 %8
%39 = OpIMul %6 %37 %38
%40 = OpLoad %6 %18
%41 = OpLoad %6 %18
%42 = OpIMul %6 %40 %41
%43 = OpIAdd %6 %39 %42
%44 = OpLoad %6 %23
%45 = OpLoad %6 %23
%46 = OpIMul %6 %44 %45
%47 = OpIAdd %6 %43 %46
%49 = OpImageTexelPointer %48 %30 %36 %13
%50 = ${OPNAME} %6 %49 %19 %13 ${LASTARG:default=%47}
OpReturn
OpFunctionEnd
)";

const std::string kShader_1d_array_r32i_intermediate_values = R"(
; The SPIR-V shader below is based on the following GLSL shader, but OpAtomicIAdd has been
; replaced with a template parameter and the last argument for it has been made optional.
;
; #version 440
; precision highp iimage1DArray;
;
; layout (local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
; layout (r32i, binding=0) coherent uniform iimage1DArray u_resultImage;
; layout (r32i, binding=1) writeonly uniform iimage1DArray u_intermValuesImage;
;
; void main (void)
; {
;     int gx = int(gl_GlobalInvocationID.x);
;     int gy = int(gl_GlobalInvocationID.y);
;     int gz = int(gl_GlobalInvocationID.z);
;     imageStore(u_intermValuesImage, ivec2(gx,gy), ivec4(imageAtomicAdd(u_resultImage, ivec2(gx % 64,gy), int(gx*gx + gy*gy + gz*gz))));
; }
;
; SPIR-V
; Version: 1.0
; Generator: Khronos Glslang Reference Front End; 7
; Bound: 59
; Schema: 0
OpCapability Shader
OpCapability Image1D
%1 = OpExtInstImport "GLSL.std.450"
OpMemoryModel Logical GLSL450
OpEntryPoint GLCompute %4 "main" %12
OpExecutionMode %4 LocalSize 1 1 1
OpDecorate %12 BuiltIn GlobalInvocationId
OpDecorate %30 DescriptorSet 0
OpDecorate %30 Binding 1
OpDecorate %30 NonReadable
OpDecorate %36 DescriptorSet 0
OpDecorate %36 Binding 0
OpDecorate %36 Coherent
OpDecorate %58 BuiltIn WorkgroupSize
%2 = OpTypeVoid
%3 = OpTypeFunction %2
%6 = OpTypeInt 32 1
%7 = OpTypePointer Function %6
%9 = OpTypeInt 32 0
%10 = OpTypeVector %9 3
%11 = OpTypePointer Input %10
%12 = OpVariable %11 Input
%13 = OpConstant %9 0
%14 = OpTypePointer Input %9
%19 = OpConstant %9 1
%24 = OpConstant %9 2
%28 = OpTypeImage %6 1D 0 1 0 2 R32i
%29 = OpTypePointer UniformConstant %28
%30 = OpVariable %29 UniformConstant
%34 = OpTypeVector %6 2
%36 = OpVariable %29 UniformConstant
%38 = OpConstant %6 64
%53 = OpTypePointer Image %6
%56 = OpTypeVector %6 4
%58 = OpConstantComposite %10 %19 %19 %19
%4 = OpFunction %2 None %3
%5 = OpLabel
%8 = OpVariable %7 Function
%18 = OpVariable %7 Function
%23 = OpVariable %7 Function
%15 = OpAccessChain %14 %12 %13
%16 = OpLoad %9 %15
%17 = OpBitcast %6 %16
OpStore %8 %17
%20 = OpAccessChain %14 %12 %19
%21 = OpLoad %9 %20
%22 = OpBitcast %6 %21
OpStore %18 %22
%25 = OpAccessChain %14 %12 %24
%26 = OpLoad %9 %25
%27 = OpBitcast %6 %26
OpStore %23 %27
%31 = OpLoad %28 %30
%32 = OpLoad %6 %8
%33 = OpLoad %6 %18
%35 = OpCompositeConstruct %34 %32 %33
%37 = OpLoad %6 %8
%39 = OpSMod %6 %37 %38
%40 = OpLoad %6 %18
%41 = OpCompositeConstruct %34 %39 %40
%42 = OpLoad %6 %8
%43 = OpLoad %6 %8
%44 = OpIMul %6 %42 %43
%45 = OpLoad %6 %18
%46 = OpLoad %6 %18
%47 = OpIMul %6 %45 %46
%48 = OpIAdd %6 %44 %47
%49 = OpLoad %6 %23
%50 = OpLoad %6 %23
%51 = OpIMul %6 %49 %50
%52 = OpIAdd %6 %48 %51
%54 = OpImageTexelPointer %53 %36 %41 %13
%55 = ${OPNAME} %6 %54 %19 %13 ${LASTARG:default=%52}
%57 = OpCompositeConstruct %56 %55 %55 %55 %55
OpImageWrite %31 %35 %57
OpReturn
OpFunctionEnd
)";

const std::string kShader_2d_r32ui_end_result = R"(
; The SPIR-V shader below is based on the following GLSL shader, but OpAtomicIAdd has been
; replaced with a template parameter and the last argument for it has been made optional.
;
; #version 440
; precision highp uimage2D;
;
; layout (local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
; layout (r32ui, binding=0) coherent uniform uimage2D u_resultImage;
;
; void main (void)
; {
;     int gx = int(gl_GlobalInvocationID.x);
;     int gy = int(gl_GlobalInvocationID.y);
;     int gz = int(gl_GlobalInvocationID.z);
;     imageAtomicAdd(u_resultImage, ivec2(gx % 64,gy), uint(gx*gx + gy*gy + gz*gz));
; }
;
; SPIR-V
; Version: 1.0
; Generator: Khronos Glslang Reference Front End; 7
; Bound: 53
; Schema: 0
OpCapability Shader
%1 = OpExtInstImport "GLSL.std.450"
OpMemoryModel Logical GLSL450
OpEntryPoint GLCompute %4 "main" %12
OpExecutionMode %4 LocalSize 1 1 1
OpDecorate %12 BuiltIn GlobalInvocationId
OpDecorate %30 DescriptorSet 0
OpDecorate %30 Binding 0
OpDecorate %30 Coherent
OpDecorate %52 BuiltIn WorkgroupSize
%2 = OpTypeVoid
%3 = OpTypeFunction %2
%6 = OpTypeInt 32 1
%7 = OpTypePointer Function %6
%9 = OpTypeInt 32 0
%10 = OpTypeVector %9 3
%11 = OpTypePointer Input %10
%12 = OpVariable %11 Input
%13 = OpConstant %9 0
%14 = OpTypePointer Input %9
%19 = OpConstant %9 1
%24 = OpConstant %9 2
%28 = OpTypeImage %9 2D 0 0 0 2 R32ui
%29 = OpTypePointer UniformConstant %28
%30 = OpVariable %29 UniformConstant
%32 = OpConstant %6 64
%35 = OpTypeVector %6 2
%49 = OpTypePointer Image %9
%52 = OpConstantComposite %10 %19 %19 %19
%4 = OpFunction %2 None %3
%5 = OpLabel
%8 = OpVariable %7 Function
%18 = OpVariable %7 Function
%23 = OpVariable %7 Function
%15 = OpAccessChain %14 %12 %13
%16 = OpLoad %9 %15
%17 = OpBitcast %6 %16
OpStore %8 %17
%20 = OpAccessChain %14 %12 %19
%21 = OpLoad %9 %20
%22 = OpBitcast %6 %21
OpStore %18 %22
%25 = OpAccessChain %14 %12 %24
%26 = OpLoad %9 %25
%27 = OpBitcast %6 %26
OpStore %23 %27
%31 = OpLoad %6 %8
%33 = OpSMod %6 %31 %32
%34 = OpLoad %6 %18
%36 = OpCompositeConstruct %35 %33 %34
%37 = OpLoad %6 %8
%38 = OpLoad %6 %8
%39 = OpIMul %6 %37 %38
%40 = OpLoad %6 %18
%41 = OpLoad %6 %18
%42 = OpIMul %6 %40 %41
%43 = OpIAdd %6 %39 %42
%44 = OpLoad %6 %23
%45 = OpLoad %6 %23
%46 = OpIMul %6 %44 %45
%47 = OpIAdd %6 %43 %46
%48 = OpBitcast %9 %47
%50 = OpImageTexelPointer %49 %30 %36 %13
%51 = ${OPNAME} %9 %50 %19 %13 ${LASTARG:default=%48}
OpReturn
OpFunctionEnd
)";

const std::string kShader_2d_r32ui_intermediate_values = R"(
; The SPIR-V shader below is based on the following GLSL shader, but OpAtomicIAdd has been
; replaced with a template parameter and the last argument for it has been made optional.
;
; #version 440
; precision highp uimage2D;
;
; layout (local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
; layout (r32ui, binding=0) coherent uniform uimage2D u_resultImage;
; layout (r32ui, binding=1) writeonly uniform uimage2D u_intermValuesImage;
;
; void main (void)
; {
;     int gx = int(gl_GlobalInvocationID.x);
;     int gy = int(gl_GlobalInvocationID.y);
;     int gz = int(gl_GlobalInvocationID.z);
;     imageStore(u_intermValuesImage, ivec2(gx,gy), uvec4(imageAtomicAdd(u_resultImage, ivec2(gx % 64,gy), uint(gx*gx + gy*gy + gz*gz))));
; }
;
; SPIR-V
; Version: 1.0
; Generator: Khronos Glslang Reference Front End; 7
; Bound: 60
; Schema: 0
OpCapability Shader
%1 = OpExtInstImport "GLSL.std.450"
OpMemoryModel Logical GLSL450
OpEntryPoint GLCompute %4 "main" %12
OpExecutionMode %4 LocalSize 1 1 1
OpDecorate %12 BuiltIn GlobalInvocationId
OpDecorate %30 DescriptorSet 0
OpDecorate %30 Binding 1
OpDecorate %30 NonReadable
OpDecorate %36 DescriptorSet 0
OpDecorate %36 Binding 0
OpDecorate %36 Coherent
OpDecorate %59 BuiltIn WorkgroupSize
%2 = OpTypeVoid
%3 = OpTypeFunction %2
%6 = OpTypeInt 32 1
%7 = OpTypePointer Function %6
%9 = OpTypeInt 32 0
%10 = OpTypeVector %9 3
%11 = OpTypePointer Input %10
%12 = OpVariable %11 Input
%13 = OpConstant %9 0
%14 = OpTypePointer Input %9
%19 = OpConstant %9 1
%24 = OpConstant %9 2
%28 = OpTypeImage %9 2D 0 0 0 2 R32ui
%29 = OpTypePointer UniformConstant %28
%30 = OpVariable %29 UniformConstant
%34 = OpTypeVector %6 2
%36 = OpVariable %29 UniformConstant
%38 = OpConstant %6 64
%54 = OpTypePointer Image %9
%57 = OpTypeVector %9 4
%59 = OpConstantComposite %10 %19 %19 %19
%4 = OpFunction %2 None %3
%5 = OpLabel
%8 = OpVariable %7 Function
%18 = OpVariable %7 Function
%23 = OpVariable %7 Function
%15 = OpAccessChain %14 %12 %13
%16 = OpLoad %9 %15
%17 = OpBitcast %6 %16
OpStore %8 %17
%20 = OpAccessChain %14 %12 %19
%21 = OpLoad %9 %20
%22 = OpBitcast %6 %21
OpStore %18 %22
%25 = OpAccessChain %14 %12 %24
%26 = OpLoad %9 %25
%27 = OpBitcast %6 %26
OpStore %23 %27
%31 = OpLoad %28 %30
%32 = OpLoad %6 %8
%33 = OpLoad %6 %18
%35 = OpCompositeConstruct %34 %32 %33
%37 = OpLoad %6 %8
%39 = OpSMod %6 %37 %38
%40 = OpLoad %6 %18
%41 = OpCompositeConstruct %34 %39 %40
%42 = OpLoad %6 %8
%43 = OpLoad %6 %8
%44 = OpIMul %6 %42 %43
%45 = OpLoad %6 %18
%46 = OpLoad %6 %18
%47 = OpIMul %6 %45 %46
%48 = OpIAdd %6 %44 %47
%49 = OpLoad %6 %23
%50 = OpLoad %6 %23
%51 = OpIMul %6 %49 %50
%52 = OpIAdd %6 %48 %51
%53 = OpBitcast %9 %52
%55 = OpImageTexelPointer %54 %36 %41 %13
%56 = ${OPNAME} %9 %55 %19 %13 ${LASTARG:default=%53}
%58 = OpCompositeConstruct %57 %56 %56 %56 %56
OpImageWrite %31 %35 %58
OpReturn
OpFunctionEnd
)";

const std::string kShader_2d_r32i_end_result = R"(
; The SPIR-V shader below is based on the following GLSL shader, but OpAtomicIAdd has been
; replaced with a template parameter and the last argument for it has been made optional.
;
; #version 440
; precision highp iimage2D;
;
; layout (local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
; layout (r32i, binding=0) coherent uniform iimage2D u_resultImage;
;
; void main (void)
; {
;     int gx = int(gl_GlobalInvocationID.x);
;     int gy = int(gl_GlobalInvocationID.y);
;     int gz = int(gl_GlobalInvocationID.z);
;     imageAtomicAdd(u_resultImage, ivec2(gx % 64,gy), int(gx*gx + gy*gy + gz*gz));
; }
;
; SPIR-V
; Version: 1.0
; Generator: Khronos Glslang Reference Front End; 7
; Bound: 52
; Schema: 0
OpCapability Shader
%1 = OpExtInstImport "GLSL.std.450"
OpMemoryModel Logical GLSL450
OpEntryPoint GLCompute %4 "main" %12
OpExecutionMode %4 LocalSize 1 1 1
OpDecorate %12 BuiltIn GlobalInvocationId
OpDecorate %30 DescriptorSet 0
OpDecorate %30 Binding 0
OpDecorate %30 Coherent
OpDecorate %51 BuiltIn WorkgroupSize
%2 = OpTypeVoid
%3 = OpTypeFunction %2
%6 = OpTypeInt 32 1
%7 = OpTypePointer Function %6
%9 = OpTypeInt 32 0
%10 = OpTypeVector %9 3
%11 = OpTypePointer Input %10
%12 = OpVariable %11 Input
%13 = OpConstant %9 0
%14 = OpTypePointer Input %9
%19 = OpConstant %9 1
%24 = OpConstant %9 2
%28 = OpTypeImage %6 2D 0 0 0 2 R32i
%29 = OpTypePointer UniformConstant %28
%30 = OpVariable %29 UniformConstant
%32 = OpConstant %6 64
%35 = OpTypeVector %6 2
%48 = OpTypePointer Image %6
%51 = OpConstantComposite %10 %19 %19 %19
%4 = OpFunction %2 None %3
%5 = OpLabel
%8 = OpVariable %7 Function
%18 = OpVariable %7 Function
%23 = OpVariable %7 Function
%15 = OpAccessChain %14 %12 %13
%16 = OpLoad %9 %15
%17 = OpBitcast %6 %16
OpStore %8 %17
%20 = OpAccessChain %14 %12 %19
%21 = OpLoad %9 %20
%22 = OpBitcast %6 %21
OpStore %18 %22
%25 = OpAccessChain %14 %12 %24
%26 = OpLoad %9 %25
%27 = OpBitcast %6 %26
OpStore %23 %27
%31 = OpLoad %6 %8
%33 = OpSMod %6 %31 %32
%34 = OpLoad %6 %18
%36 = OpCompositeConstruct %35 %33 %34
%37 = OpLoad %6 %8
%38 = OpLoad %6 %8
%39 = OpIMul %6 %37 %38
%40 = OpLoad %6 %18
%41 = OpLoad %6 %18
%42 = OpIMul %6 %40 %41
%43 = OpIAdd %6 %39 %42
%44 = OpLoad %6 %23
%45 = OpLoad %6 %23
%46 = OpIMul %6 %44 %45
%47 = OpIAdd %6 %43 %46
%49 = OpImageTexelPointer %48 %30 %36 %13
%50 = ${OPNAME} %6 %49 %19 %13 ${LASTARG:default=%47}
OpReturn
OpFunctionEnd
)";

const std::string kShader_2d_r32i_intermediate_values = R"(
; The SPIR-V shader below is based on the following GLSL shader, but OpAtomicIAdd has been
; replaced with a template parameter and the last argument for it has been made optional.
;
; #version 440
; precision highp iimage2D;
;
; layout (local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
; layout (r32i, binding=0) coherent uniform iimage2D u_resultImage;
; layout (r32i, binding=1) writeonly uniform iimage2D u_intermValuesImage;
;
; void main (void)
; {
;     int gx = int(gl_GlobalInvocationID.x);
;     int gy = int(gl_GlobalInvocationID.y);
;     int gz = int(gl_GlobalInvocationID.z);
;     imageStore(u_intermValuesImage, ivec2(gx,gy), ivec4(imageAtomicAdd(u_resultImage, ivec2(gx % 64,gy), int(gx*gx + gy*gy + gz*gz))));
; }
;
; SPIR-V
; Version: 1.0
; Generator: Khronos Glslang Reference Front End; 7
; Bound: 59
; Schema: 0
OpCapability Shader
%1 = OpExtInstImport "GLSL.std.450"
OpMemoryModel Logical GLSL450
OpEntryPoint GLCompute %4 "main" %12
OpExecutionMode %4 LocalSize 1 1 1
OpDecorate %12 BuiltIn GlobalInvocationId
OpDecorate %30 DescriptorSet 0
OpDecorate %30 Binding 1
OpDecorate %30 NonReadable
OpDecorate %36 DescriptorSet 0
OpDecorate %36 Binding 0
OpDecorate %36 Coherent
OpDecorate %58 BuiltIn WorkgroupSize
%2 = OpTypeVoid
%3 = OpTypeFunction %2
%6 = OpTypeInt 32 1
%7 = OpTypePointer Function %6
%9 = OpTypeInt 32 0
%10 = OpTypeVector %9 3
%11 = OpTypePointer Input %10
%12 = OpVariable %11 Input
%13 = OpConstant %9 0
%14 = OpTypePointer Input %9
%19 = OpConstant %9 1
%24 = OpConstant %9 2
%28 = OpTypeImage %6 2D 0 0 0 2 R32i
%29 = OpTypePointer UniformConstant %28
%30 = OpVariable %29 UniformConstant
%34 = OpTypeVector %6 2
%36 = OpVariable %29 UniformConstant
%38 = OpConstant %6 64
%53 = OpTypePointer Image %6
%56 = OpTypeVector %6 4
%58 = OpConstantComposite %10 %19 %19 %19
%4 = OpFunction %2 None %3
%5 = OpLabel
%8 = OpVariable %7 Function
%18 = OpVariable %7 Function
%23 = OpVariable %7 Function
%15 = OpAccessChain %14 %12 %13
%16 = OpLoad %9 %15
%17 = OpBitcast %6 %16
OpStore %8 %17
%20 = OpAccessChain %14 %12 %19
%21 = OpLoad %9 %20
%22 = OpBitcast %6 %21
OpStore %18 %22
%25 = OpAccessChain %14 %12 %24
%26 = OpLoad %9 %25
%27 = OpBitcast %6 %26
OpStore %23 %27
%31 = OpLoad %28 %30
%32 = OpLoad %6 %8
%33 = OpLoad %6 %18
%35 = OpCompositeConstruct %34 %32 %33
%37 = OpLoad %6 %8
%39 = OpSMod %6 %37 %38
%40 = OpLoad %6 %18
%41 = OpCompositeConstruct %34 %39 %40
%42 = OpLoad %6 %8
%43 = OpLoad %6 %8
%44 = OpIMul %6 %42 %43
%45 = OpLoad %6 %18
%46 = OpLoad %6 %18
%47 = OpIMul %6 %45 %46
%48 = OpIAdd %6 %44 %47
%49 = OpLoad %6 %23
%50 = OpLoad %6 %23
%51 = OpIMul %6 %49 %50
%52 = OpIAdd %6 %48 %51
%54 = OpImageTexelPointer %53 %36 %41 %13
%55 = ${OPNAME} %6 %54 %19 %13 ${LASTARG:default=%52}
%57 = OpCompositeConstruct %56 %55 %55 %55 %55
OpImageWrite %31 %35 %57
OpReturn
OpFunctionEnd
)";

const std::string kShader_2d_array_r32ui_end_result = R"(
; The SPIR-V shader below is based on the following GLSL shader, but OpAtomicIAdd has been
; replaced with a template parameter and the last argument for it has been made optional.
;
; #version 440
; precision highp uimage2DArray;
;
; layout (local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
; layout (r32ui, binding=0) coherent uniform uimage2DArray u_resultImage;
;
; void main (void)
; {
;     int gx = int(gl_GlobalInvocationID.x);
;     int gy = int(gl_GlobalInvocationID.y);
;     int gz = int(gl_GlobalInvocationID.z);
;     imageAtomicAdd(u_resultImage, ivec3(gx % 64,gy,gz), uint(gx*gx + gy*gy + gz*gz));
; }
;
; SPIR-V
; Version: 1.0
; Generator: Khronos Glslang Reference Front End; 7
; Bound: 54
; Schema: 0
OpCapability Shader
%1 = OpExtInstImport "GLSL.std.450"
OpMemoryModel Logical GLSL450
OpEntryPoint GLCompute %4 "main" %12
OpExecutionMode %4 LocalSize 1 1 1
OpDecorate %12 BuiltIn GlobalInvocationId
OpDecorate %30 DescriptorSet 0
OpDecorate %30 Binding 0
OpDecorate %30 Coherent
OpDecorate %53 BuiltIn WorkgroupSize
%2 = OpTypeVoid
%3 = OpTypeFunction %2
%6 = OpTypeInt 32 1
%7 = OpTypePointer Function %6
%9 = OpTypeInt 32 0
%10 = OpTypeVector %9 3
%11 = OpTypePointer Input %10
%12 = OpVariable %11 Input
%13 = OpConstant %9 0
%14 = OpTypePointer Input %9
%19 = OpConstant %9 1
%24 = OpConstant %9 2
%28 = OpTypeImage %9 2D 0 1 0 2 R32ui
%29 = OpTypePointer UniformConstant %28
%30 = OpVariable %29 UniformConstant
%32 = OpConstant %6 64
%36 = OpTypeVector %6 3
%50 = OpTypePointer Image %9
%53 = OpConstantComposite %10 %19 %19 %19
%4 = OpFunction %2 None %3
%5 = OpLabel
%8 = OpVariable %7 Function
%18 = OpVariable %7 Function
%23 = OpVariable %7 Function
%15 = OpAccessChain %14 %12 %13
%16 = OpLoad %9 %15
%17 = OpBitcast %6 %16
OpStore %8 %17
%20 = OpAccessChain %14 %12 %19
%21 = OpLoad %9 %20
%22 = OpBitcast %6 %21
OpStore %18 %22
%25 = OpAccessChain %14 %12 %24
%26 = OpLoad %9 %25
%27 = OpBitcast %6 %26
OpStore %23 %27
%31 = OpLoad %6 %8
%33 = OpSMod %6 %31 %32
%34 = OpLoad %6 %18
%35 = OpLoad %6 %23
%37 = OpCompositeConstruct %36 %33 %34 %35
%38 = OpLoad %6 %8
%39 = OpLoad %6 %8
%40 = OpIMul %6 %38 %39
%41 = OpLoad %6 %18
%42 = OpLoad %6 %18
%43 = OpIMul %6 %41 %42
%44 = OpIAdd %6 %40 %43
%45 = OpLoad %6 %23
%46 = OpLoad %6 %23
%47 = OpIMul %6 %45 %46
%48 = OpIAdd %6 %44 %47
%49 = OpBitcast %9 %48
%51 = OpImageTexelPointer %50 %30 %37 %13
%52 = ${OPNAME} %9 %51 %19 %13 ${LASTARG:default=%49}
OpReturn
OpFunctionEnd
)";

const std::string kShader_2d_array_r32ui_intermediate_values = R"(
; The SPIR-V shader below is based on the following GLSL shader, but OpAtomicIAdd has been
; replaced with a template parameter and the last argument for it has been made optional.
;
; #version 440
; precision highp uimage2DArray;
;
; layout (local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
; layout (r32ui, binding=0) coherent uniform uimage2DArray u_resultImage;
; layout (r32ui, binding=1) writeonly uniform uimage2DArray u_intermValuesImage;
;
; void main (void)
; {
;     int gx = int(gl_GlobalInvocationID.x);
;     int gy = int(gl_GlobalInvocationID.y);
;     int gz = int(gl_GlobalInvocationID.z);
;     imageStore(u_intermValuesImage, ivec3(gx,gy,gz), uvec4(imageAtomicAdd(u_resultImage, ivec3(gx % 64,gy,gz), uint(gx*gx + gy*gy + gz*gz))));
; }
;
; SPIR-V
; Version: 1.0
; Generator: Khronos Glslang Reference Front End; 7
; Bound: 62
; Schema: 0
OpCapability Shader
%1 = OpExtInstImport "GLSL.std.450"
OpMemoryModel Logical GLSL450
OpEntryPoint GLCompute %4 "main" %12
OpExecutionMode %4 LocalSize 1 1 1
OpDecorate %12 BuiltIn GlobalInvocationId
OpDecorate %30 DescriptorSet 0
OpDecorate %30 Binding 1
OpDecorate %30 NonReadable
OpDecorate %37 DescriptorSet 0
OpDecorate %37 Binding 0
OpDecorate %37 Coherent
OpDecorate %61 BuiltIn WorkgroupSize
%2 = OpTypeVoid
%3 = OpTypeFunction %2
%6 = OpTypeInt 32 1
%7 = OpTypePointer Function %6
%9 = OpTypeInt 32 0
%10 = OpTypeVector %9 3
%11 = OpTypePointer Input %10
%12 = OpVariable %11 Input
%13 = OpConstant %9 0
%14 = OpTypePointer Input %9
%19 = OpConstant %9 1
%24 = OpConstant %9 2
%28 = OpTypeImage %9 2D 0 1 0 2 R32ui
%29 = OpTypePointer UniformConstant %28
%30 = OpVariable %29 UniformConstant
%35 = OpTypeVector %6 3
%37 = OpVariable %29 UniformConstant
%39 = OpConstant %6 64
%56 = OpTypePointer Image %9
%59 = OpTypeVector %9 4
%61 = OpConstantComposite %10 %19 %19 %19
%4 = OpFunction %2 None %3
%5 = OpLabel
%8 = OpVariable %7 Function
%18 = OpVariable %7 Function
%23 = OpVariable %7 Function
%15 = OpAccessChain %14 %12 %13
%16 = OpLoad %9 %15
%17 = OpBitcast %6 %16
OpStore %8 %17
%20 = OpAccessChain %14 %12 %19
%21 = OpLoad %9 %20
%22 = OpBitcast %6 %21
OpStore %18 %22
%25 = OpAccessChain %14 %12 %24
%26 = OpLoad %9 %25
%27 = OpBitcast %6 %26
OpStore %23 %27
%31 = OpLoad %28 %30
%32 = OpLoad %6 %8
%33 = OpLoad %6 %18
%34 = OpLoad %6 %23
%36 = OpCompositeConstruct %35 %32 %33 %34
%38 = OpLoad %6 %8
%40 = OpSMod %6 %38 %39
%41 = OpLoad %6 %18
%42 = OpLoad %6 %23
%43 = OpCompositeConstruct %35 %40 %41 %42
%44 = OpLoad %6 %8
%45 = OpLoad %6 %8
%46 = OpIMul %6 %44 %45
%47 = OpLoad %6 %18
%48 = OpLoad %6 %18
%49 = OpIMul %6 %47 %48
%50 = OpIAdd %6 %46 %49
%51 = OpLoad %6 %23
%52 = OpLoad %6 %23
%53 = OpIMul %6 %51 %52
%54 = OpIAdd %6 %50 %53
%55 = OpBitcast %9 %54
%57 = OpImageTexelPointer %56 %37 %43 %13
%58 = ${OPNAME} %9 %57 %19 %13 ${LASTARG:default=%55}
%60 = OpCompositeConstruct %59 %58 %58 %58 %58
OpImageWrite %31 %36 %60
OpReturn
OpFunctionEnd
)";

const std::string kShader_2d_array_r32i_end_result = R"(
; The SPIR-V shader below is based on the following GLSL shader, but OpAtomicIAdd has been
; replaced with a template parameter and the last argument for it has been made optional.
;
; #version 440
; precision highp iimage2DArray;
;
; layout (local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
; layout (r32i, binding=0) coherent uniform iimage2DArray u_resultImage;
;
; void main (void)
; {
;     int gx = int(gl_GlobalInvocationID.x);
;     int gy = int(gl_GlobalInvocationID.y);
;     int gz = int(gl_GlobalInvocationID.z);
;     imageAtomicAdd(u_resultImage, ivec3(gx % 64,gy,gz), int(gx*gx + gy*gy + gz*gz));
; }
;
; SPIR-V
; Version: 1.0
; Generator: Khronos Glslang Reference Front End; 7
; Bound: 53
; Schema: 0
OpCapability Shader
%1 = OpExtInstImport "GLSL.std.450"
OpMemoryModel Logical GLSL450
OpEntryPoint GLCompute %4 "main" %12
OpExecutionMode %4 LocalSize 1 1 1
OpDecorate %12 BuiltIn GlobalInvocationId
OpDecorate %30 DescriptorSet 0
OpDecorate %30 Binding 0
OpDecorate %30 Coherent
OpDecorate %52 BuiltIn WorkgroupSize
%2 = OpTypeVoid
%3 = OpTypeFunction %2
%6 = OpTypeInt 32 1
%7 = OpTypePointer Function %6
%9 = OpTypeInt 32 0
%10 = OpTypeVector %9 3
%11 = OpTypePointer Input %10
%12 = OpVariable %11 Input
%13 = OpConstant %9 0
%14 = OpTypePointer Input %9
%19 = OpConstant %9 1
%24 = OpConstant %9 2
%28 = OpTypeImage %6 2D 0 1 0 2 R32i
%29 = OpTypePointer UniformConstant %28
%30 = OpVariable %29 UniformConstant
%32 = OpConstant %6 64
%36 = OpTypeVector %6 3
%49 = OpTypePointer Image %6
%52 = OpConstantComposite %10 %19 %19 %19
%4 = OpFunction %2 None %3
%5 = OpLabel
%8 = OpVariable %7 Function
%18 = OpVariable %7 Function
%23 = OpVariable %7 Function
%15 = OpAccessChain %14 %12 %13
%16 = OpLoad %9 %15
%17 = OpBitcast %6 %16
OpStore %8 %17
%20 = OpAccessChain %14 %12 %19
%21 = OpLoad %9 %20
%22 = OpBitcast %6 %21
OpStore %18 %22
%25 = OpAccessChain %14 %12 %24
%26 = OpLoad %9 %25
%27 = OpBitcast %6 %26
OpStore %23 %27
%31 = OpLoad %6 %8
%33 = OpSMod %6 %31 %32
%34 = OpLoad %6 %18
%35 = OpLoad %6 %23
%37 = OpCompositeConstruct %36 %33 %34 %35
%38 = OpLoad %6 %8
%39 = OpLoad %6 %8
%40 = OpIMul %6 %38 %39
%41 = OpLoad %6 %18
%42 = OpLoad %6 %18
%43 = OpIMul %6 %41 %42
%44 = OpIAdd %6 %40 %43
%45 = OpLoad %6 %23
%46 = OpLoad %6 %23
%47 = OpIMul %6 %45 %46
%48 = OpIAdd %6 %44 %47
%50 = OpImageTexelPointer %49 %30 %37 %13
%51 = ${OPNAME} %6 %50 %19 %13 ${LASTARG:default=%48}
OpReturn
OpFunctionEnd
)";

const std::string kShader_2d_array_r32i_intermediate_values = R"(
; The SPIR-V shader below is based on the following GLSL shader, but OpAtomicIAdd has been
; replaced with a template parameter and the last argument for it has been made optional.
;
; #version 440
; precision highp iimage2DArray;
;
; layout (local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
; layout (r32i, binding=0) coherent uniform iimage2DArray u_resultImage;
; layout (r32i, binding=1) writeonly uniform iimage2DArray u_intermValuesImage;
;
; void main (void)
; {
;     int gx = int(gl_GlobalInvocationID.x);
;     int gy = int(gl_GlobalInvocationID.y);
;     int gz = int(gl_GlobalInvocationID.z);
;     imageStore(u_intermValuesImage, ivec3(gx,gy,gz), ivec4(imageAtomicAdd(u_resultImage, ivec3(gx % 64,gy,gz), int(gx*gx + gy*gy + gz*gz))));
; }
;
; SPIR-V
; Version: 1.0
; Generator: Khronos Glslang Reference Front End; 7
; Bound: 61
; Schema: 0
OpCapability Shader
%1 = OpExtInstImport "GLSL.std.450"
OpMemoryModel Logical GLSL450
OpEntryPoint GLCompute %4 "main" %12
OpExecutionMode %4 LocalSize 1 1 1
OpDecorate %12 BuiltIn GlobalInvocationId
OpDecorate %30 DescriptorSet 0
OpDecorate %30 Binding 1
OpDecorate %30 NonReadable
OpDecorate %37 DescriptorSet 0
OpDecorate %37 Binding 0
OpDecorate %37 Coherent
OpDecorate %60 BuiltIn WorkgroupSize
%2 = OpTypeVoid
%3 = OpTypeFunction %2
%6 = OpTypeInt 32 1
%7 = OpTypePointer Function %6
%9 = OpTypeInt 32 0
%10 = OpTypeVector %9 3
%11 = OpTypePointer Input %10
%12 = OpVariable %11 Input
%13 = OpConstant %9 0
%14 = OpTypePointer Input %9
%19 = OpConstant %9 1
%24 = OpConstant %9 2
%28 = OpTypeImage %6 2D 0 1 0 2 R32i
%29 = OpTypePointer UniformConstant %28
%30 = OpVariable %29 UniformConstant
%35 = OpTypeVector %6 3
%37 = OpVariable %29 UniformConstant
%39 = OpConstant %6 64
%55 = OpTypePointer Image %6
%58 = OpTypeVector %6 4
%60 = OpConstantComposite %10 %19 %19 %19
%4 = OpFunction %2 None %3
%5 = OpLabel
%8 = OpVariable %7 Function
%18 = OpVariable %7 Function
%23 = OpVariable %7 Function
%15 = OpAccessChain %14 %12 %13
%16 = OpLoad %9 %15
%17 = OpBitcast %6 %16
OpStore %8 %17
%20 = OpAccessChain %14 %12 %19
%21 = OpLoad %9 %20
%22 = OpBitcast %6 %21
OpStore %18 %22
%25 = OpAccessChain %14 %12 %24
%26 = OpLoad %9 %25
%27 = OpBitcast %6 %26
OpStore %23 %27
%31 = OpLoad %28 %30
%32 = OpLoad %6 %8
%33 = OpLoad %6 %18
%34 = OpLoad %6 %23
%36 = OpCompositeConstruct %35 %32 %33 %34
%38 = OpLoad %6 %8
%40 = OpSMod %6 %38 %39
%41 = OpLoad %6 %18
%42 = OpLoad %6 %23
%43 = OpCompositeConstruct %35 %40 %41 %42
%44 = OpLoad %6 %8
%45 = OpLoad %6 %8
%46 = OpIMul %6 %44 %45
%47 = OpLoad %6 %18
%48 = OpLoad %6 %18
%49 = OpIMul %6 %47 %48
%50 = OpIAdd %6 %46 %49
%51 = OpLoad %6 %23
%52 = OpLoad %6 %23
%53 = OpIMul %6 %51 %52
%54 = OpIAdd %6 %50 %53
%56 = OpImageTexelPointer %55 %37 %43 %13
%57 = ${OPNAME} %6 %56 %19 %13 ${LASTARG:default=%54}
%59 = OpCompositeConstruct %58 %57 %57 %57 %57
OpImageWrite %31 %36 %59
OpReturn
OpFunctionEnd
)";

const std::string kShader_3d_r32ui_end_result = R"(
; The SPIR-V shader below is based on the following GLSL shader, but OpAtomicIAdd has been
; replaced with a template parameter and the last argument for it has been made optional.
;
; #version 440
; precision highp uimage3D;
;
; layout (local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
; layout (r32ui, binding=0) coherent uniform uimage3D u_resultImage;
;
; void main (void)
; {
;     int gx = int(gl_GlobalInvocationID.x);
;     int gy = int(gl_GlobalInvocationID.y);
;     int gz = int(gl_GlobalInvocationID.z);
;     imageAtomicAdd(u_resultImage, ivec3(gx % 64,gy,gz), uint(gx*gx + gy*gy + gz*gz));
; }
;
; SPIR-V
; Version: 1.0
; Generator: Khronos Glslang Reference Front End; 7
; Bound: 54
; Schema: 0
OpCapability Shader
%1 = OpExtInstImport "GLSL.std.450"
OpMemoryModel Logical GLSL450
OpEntryPoint GLCompute %4 "main" %12
OpExecutionMode %4 LocalSize 1 1 1
OpDecorate %12 BuiltIn GlobalInvocationId
OpDecorate %30 DescriptorSet 0
OpDecorate %30 Binding 0
OpDecorate %30 Coherent
OpDecorate %53 BuiltIn WorkgroupSize
%2 = OpTypeVoid
%3 = OpTypeFunction %2
%6 = OpTypeInt 32 1
%7 = OpTypePointer Function %6
%9 = OpTypeInt 32 0
%10 = OpTypeVector %9 3
%11 = OpTypePointer Input %10
%12 = OpVariable %11 Input
%13 = OpConstant %9 0
%14 = OpTypePointer Input %9
%19 = OpConstant %9 1
%24 = OpConstant %9 2
%28 = OpTypeImage %9 3D 0 0 0 2 R32ui
%29 = OpTypePointer UniformConstant %28
%30 = OpVariable %29 UniformConstant
%32 = OpConstant %6 64
%36 = OpTypeVector %6 3
%50 = OpTypePointer Image %9
%53 = OpConstantComposite %10 %19 %19 %19
%4 = OpFunction %2 None %3
%5 = OpLabel
%8 = OpVariable %7 Function
%18 = OpVariable %7 Function
%23 = OpVariable %7 Function
%15 = OpAccessChain %14 %12 %13
%16 = OpLoad %9 %15
%17 = OpBitcast %6 %16
OpStore %8 %17
%20 = OpAccessChain %14 %12 %19
%21 = OpLoad %9 %20
%22 = OpBitcast %6 %21
OpStore %18 %22
%25 = OpAccessChain %14 %12 %24
%26 = OpLoad %9 %25
%27 = OpBitcast %6 %26
OpStore %23 %27
%31 = OpLoad %6 %8
%33 = OpSMod %6 %31 %32
%34 = OpLoad %6 %18
%35 = OpLoad %6 %23
%37 = OpCompositeConstruct %36 %33 %34 %35
%38 = OpLoad %6 %8
%39 = OpLoad %6 %8
%40 = OpIMul %6 %38 %39
%41 = OpLoad %6 %18
%42 = OpLoad %6 %18
%43 = OpIMul %6 %41 %42
%44 = OpIAdd %6 %40 %43
%45 = OpLoad %6 %23
%46 = OpLoad %6 %23
%47 = OpIMul %6 %45 %46
%48 = OpIAdd %6 %44 %47
%49 = OpBitcast %9 %48
%51 = OpImageTexelPointer %50 %30 %37 %13
%52 = ${OPNAME} %9 %51 %19 %13 ${LASTARG:default=%49}
OpReturn
OpFunctionEnd
)";

const std::string kShader_3d_r32ui_intermediate_values = R"(
; The SPIR-V shader below is based on the following GLSL shader, but OpAtomicIAdd has been
; replaced with a template parameter and the last argument for it has been made optional.
;
; #version 440
; precision highp uimage3D;
;
; layout (local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
; layout (r32ui, binding=0) coherent uniform uimage3D u_resultImage;
; layout (r32ui, binding=1) writeonly uniform uimage3D u_intermValuesImage;
;
; void main (void)
; {
;     int gx = int(gl_GlobalInvocationID.x);
;     int gy = int(gl_GlobalInvocationID.y);
;     int gz = int(gl_GlobalInvocationID.z);
;     imageStore(u_intermValuesImage, ivec3(gx,gy,gz), uvec4(imageAtomicAdd(u_resultImage, ivec3(gx % 64,gy,gz), uint(gx*gx + gy*gy + gz*gz))));
; }
;
; SPIR-V
; Version: 1.0
; Generator: Khronos Glslang Reference Front End; 7
; Bound: 62
; Schema: 0
OpCapability Shader
%1 = OpExtInstImport "GLSL.std.450"
OpMemoryModel Logical GLSL450
OpEntryPoint GLCompute %4 "main" %12
OpExecutionMode %4 LocalSize 1 1 1
OpDecorate %12 BuiltIn GlobalInvocationId
OpDecorate %30 DescriptorSet 0
OpDecorate %30 Binding 1
OpDecorate %30 NonReadable
OpDecorate %37 DescriptorSet 0
OpDecorate %37 Binding 0
OpDecorate %37 Coherent
OpDecorate %61 BuiltIn WorkgroupSize
%2 = OpTypeVoid
%3 = OpTypeFunction %2
%6 = OpTypeInt 32 1
%7 = OpTypePointer Function %6
%9 = OpTypeInt 32 0
%10 = OpTypeVector %9 3
%11 = OpTypePointer Input %10
%12 = OpVariable %11 Input
%13 = OpConstant %9 0
%14 = OpTypePointer Input %9
%19 = OpConstant %9 1
%24 = OpConstant %9 2
%28 = OpTypeImage %9 3D 0 0 0 2 R32ui
%29 = OpTypePointer UniformConstant %28
%30 = OpVariable %29 UniformConstant
%35 = OpTypeVector %6 3
%37 = OpVariable %29 UniformConstant
%39 = OpConstant %6 64
%56 = OpTypePointer Image %9
%59 = OpTypeVector %9 4
%61 = OpConstantComposite %10 %19 %19 %19
%4 = OpFunction %2 None %3
%5 = OpLabel
%8 = OpVariable %7 Function
%18 = OpVariable %7 Function
%23 = OpVariable %7 Function
%15 = OpAccessChain %14 %12 %13
%16 = OpLoad %9 %15
%17 = OpBitcast %6 %16
OpStore %8 %17
%20 = OpAccessChain %14 %12 %19
%21 = OpLoad %9 %20
%22 = OpBitcast %6 %21
OpStore %18 %22
%25 = OpAccessChain %14 %12 %24
%26 = OpLoad %9 %25
%27 = OpBitcast %6 %26
OpStore %23 %27
%31 = OpLoad %28 %30
%32 = OpLoad %6 %8
%33 = OpLoad %6 %18
%34 = OpLoad %6 %23
%36 = OpCompositeConstruct %35 %32 %33 %34
%38 = OpLoad %6 %8
%40 = OpSMod %6 %38 %39
%41 = OpLoad %6 %18
%42 = OpLoad %6 %23
%43 = OpCompositeConstruct %35 %40 %41 %42
%44 = OpLoad %6 %8
%45 = OpLoad %6 %8
%46 = OpIMul %6 %44 %45
%47 = OpLoad %6 %18
%48 = OpLoad %6 %18
%49 = OpIMul %6 %47 %48
%50 = OpIAdd %6 %46 %49
%51 = OpLoad %6 %23
%52 = OpLoad %6 %23
%53 = OpIMul %6 %51 %52
%54 = OpIAdd %6 %50 %53
%55 = OpBitcast %9 %54
%57 = OpImageTexelPointer %56 %37 %43 %13
%58 = ${OPNAME} %9 %57 %19 %13 ${LASTARG:default=%55}
%60 = OpCompositeConstruct %59 %58 %58 %58 %58
OpImageWrite %31 %36 %60
OpReturn
OpFunctionEnd
)";

const std::string kShader_3d_r32i_end_result = R"(
; The SPIR-V shader below is based on the following GLSL shader, but OpAtomicIAdd has been
; replaced with a template parameter and the last argument for it has been made optional.
;
; #version 440
; precision highp iimage3D;
;
; layout (local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
; layout (r32i, binding=0) coherent uniform iimage3D u_resultImage;
;
; void main (void)
; {
;     int gx = int(gl_GlobalInvocationID.x);
;     int gy = int(gl_GlobalInvocationID.y);
;     int gz = int(gl_GlobalInvocationID.z);
;     imageAtomicAdd(u_resultImage, ivec3(gx % 64,gy,gz), int(gx*gx + gy*gy + gz*gz));
; }
;
; SPIR-V
; Version: 1.0
; Generator: Khronos Glslang Reference Front End; 7
; Bound: 53
; Schema: 0
OpCapability Shader
%1 = OpExtInstImport "GLSL.std.450"
OpMemoryModel Logical GLSL450
OpEntryPoint GLCompute %4 "main" %12
OpExecutionMode %4 LocalSize 1 1 1
OpDecorate %12 BuiltIn GlobalInvocationId
OpDecorate %30 DescriptorSet 0
OpDecorate %30 Binding 0
OpDecorate %30 Coherent
OpDecorate %52 BuiltIn WorkgroupSize
%2 = OpTypeVoid
%3 = OpTypeFunction %2
%6 = OpTypeInt 32 1
%7 = OpTypePointer Function %6
%9 = OpTypeInt 32 0
%10 = OpTypeVector %9 3
%11 = OpTypePointer Input %10
%12 = OpVariable %11 Input
%13 = OpConstant %9 0
%14 = OpTypePointer Input %9
%19 = OpConstant %9 1
%24 = OpConstant %9 2
%28 = OpTypeImage %6 3D 0 0 0 2 R32i
%29 = OpTypePointer UniformConstant %28
%30 = OpVariable %29 UniformConstant
%32 = OpConstant %6 64
%36 = OpTypeVector %6 3
%49 = OpTypePointer Image %6
%52 = OpConstantComposite %10 %19 %19 %19
%4 = OpFunction %2 None %3
%5 = OpLabel
%8 = OpVariable %7 Function
%18 = OpVariable %7 Function
%23 = OpVariable %7 Function
%15 = OpAccessChain %14 %12 %13
%16 = OpLoad %9 %15
%17 = OpBitcast %6 %16
OpStore %8 %17
%20 = OpAccessChain %14 %12 %19
%21 = OpLoad %9 %20
%22 = OpBitcast %6 %21
OpStore %18 %22
%25 = OpAccessChain %14 %12 %24
%26 = OpLoad %9 %25
%27 = OpBitcast %6 %26
OpStore %23 %27
%31 = OpLoad %6 %8
%33 = OpSMod %6 %31 %32
%34 = OpLoad %6 %18
%35 = OpLoad %6 %23
%37 = OpCompositeConstruct %36 %33 %34 %35
%38 = OpLoad %6 %8
%39 = OpLoad %6 %8
%40 = OpIMul %6 %38 %39
%41 = OpLoad %6 %18
%42 = OpLoad %6 %18
%43 = OpIMul %6 %41 %42
%44 = OpIAdd %6 %40 %43
%45 = OpLoad %6 %23
%46 = OpLoad %6 %23
%47 = OpIMul %6 %45 %46
%48 = OpIAdd %6 %44 %47
%50 = OpImageTexelPointer %49 %30 %37 %13
%51 = ${OPNAME} %6 %50 %19 %13 ${LASTARG:default=%48}
OpReturn
OpFunctionEnd
)";

const std::string kShader_3d_r32i_intermediate_values = R"(
; The SPIR-V shader below is based on the following GLSL shader, but OpAtomicIAdd has been
; replaced with a template parameter and the last argument for it has been made optional.
;
; #version 440
; precision highp iimage3D;
;
; layout (local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
; layout (r32i, binding=0) coherent uniform iimage3D u_resultImage;
; layout (r32i, binding=1) writeonly uniform iimage3D u_intermValuesImage;
;
; void main (void)
; {
;     int gx = int(gl_GlobalInvocationID.x);
;     int gy = int(gl_GlobalInvocationID.y);
;     int gz = int(gl_GlobalInvocationID.z);
;     imageStore(u_intermValuesImage, ivec3(gx,gy,gz), ivec4(imageAtomicAdd(u_resultImage, ivec3(gx % 64,gy,gz), int(gx*gx + gy*gy + gz*gz))));
; }
;
; SPIR-V
; Version: 1.0
; Generator: Khronos Glslang Reference Front End; 7
; Bound: 61
; Schema: 0
OpCapability Shader
%1 = OpExtInstImport "GLSL.std.450"
OpMemoryModel Logical GLSL450
OpEntryPoint GLCompute %4 "main" %12
OpExecutionMode %4 LocalSize 1 1 1
OpDecorate %12 BuiltIn GlobalInvocationId
OpDecorate %30 DescriptorSet 0
OpDecorate %30 Binding 1
OpDecorate %30 NonReadable
OpDecorate %37 DescriptorSet 0
OpDecorate %37 Binding 0
OpDecorate %37 Coherent
OpDecorate %60 BuiltIn WorkgroupSize
%2 = OpTypeVoid
%3 = OpTypeFunction %2
%6 = OpTypeInt 32 1
%7 = OpTypePointer Function %6
%9 = OpTypeInt 32 0
%10 = OpTypeVector %9 3
%11 = OpTypePointer Input %10
%12 = OpVariable %11 Input
%13 = OpConstant %9 0
%14 = OpTypePointer Input %9
%19 = OpConstant %9 1
%24 = OpConstant %9 2
%28 = OpTypeImage %6 3D 0 0 0 2 R32i
%29 = OpTypePointer UniformConstant %28
%30 = OpVariable %29 UniformConstant
%35 = OpTypeVector %6 3
%37 = OpVariable %29 UniformConstant
%39 = OpConstant %6 64
%55 = OpTypePointer Image %6
%58 = OpTypeVector %6 4
%60 = OpConstantComposite %10 %19 %19 %19
%4 = OpFunction %2 None %3
%5 = OpLabel
%8 = OpVariable %7 Function
%18 = OpVariable %7 Function
%23 = OpVariable %7 Function
%15 = OpAccessChain %14 %12 %13
%16 = OpLoad %9 %15
%17 = OpBitcast %6 %16
OpStore %8 %17
%20 = OpAccessChain %14 %12 %19
%21 = OpLoad %9 %20
%22 = OpBitcast %6 %21
OpStore %18 %22
%25 = OpAccessChain %14 %12 %24
%26 = OpLoad %9 %25
%27 = OpBitcast %6 %26
OpStore %23 %27
%31 = OpLoad %28 %30
%32 = OpLoad %6 %8
%33 = OpLoad %6 %18
%34 = OpLoad %6 %23
%36 = OpCompositeConstruct %35 %32 %33 %34
%38 = OpLoad %6 %8
%40 = OpSMod %6 %38 %39
%41 = OpLoad %6 %18
%42 = OpLoad %6 %23
%43 = OpCompositeConstruct %35 %40 %41 %42
%44 = OpLoad %6 %8
%45 = OpLoad %6 %8
%46 = OpIMul %6 %44 %45
%47 = OpLoad %6 %18
%48 = OpLoad %6 %18
%49 = OpIMul %6 %47 %48
%50 = OpIAdd %6 %46 %49
%51 = OpLoad %6 %23
%52 = OpLoad %6 %23
%53 = OpIMul %6 %51 %52
%54 = OpIAdd %6 %50 %53
%56 = OpImageTexelPointer %55 %37 %43 %13
%57 = ${OPNAME} %6 %56 %19 %13 ${LASTARG:default=%54}
%59 = OpCompositeConstruct %58 %57 %57 %57 %57
OpImageWrite %31 %36 %59
OpReturn
OpFunctionEnd
)";

const std::string kShader_cube_r32ui_end_result = R"(
; The SPIR-V shader below is based on the following GLSL shader, but OpAtomicIAdd has been
; replaced with a template parameter and the last argument for it has been made optional.
;
; #version 440
; precision highp uimageCube;
;
; layout (local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
; layout (r32ui, binding=0) coherent uniform uimageCube u_resultImage;
;
; void main (void)
; {
;     int gx = int(gl_GlobalInvocationID.x);
;     int gy = int(gl_GlobalInvocationID.y);
;     int gz = int(gl_GlobalInvocationID.z);
;     imageAtomicAdd(u_resultImage, ivec3(gx % 64,gy,gz), uint(gx*gx + gy*gy + gz*gz));
; }
;
; SPIR-V
; Version: 1.0
; Generator: Khronos Glslang Reference Front End; 7
; Bound: 54
; Schema: 0
OpCapability Shader
%1 = OpExtInstImport "GLSL.std.450"
OpMemoryModel Logical GLSL450
OpEntryPoint GLCompute %4 "main" %12
OpExecutionMode %4 LocalSize 1 1 1
OpDecorate %12 BuiltIn GlobalInvocationId
OpDecorate %30 DescriptorSet 0
OpDecorate %30 Binding 0
OpDecorate %30 Coherent
OpDecorate %53 BuiltIn WorkgroupSize
%2 = OpTypeVoid
%3 = OpTypeFunction %2
%6 = OpTypeInt 32 1
%7 = OpTypePointer Function %6
%9 = OpTypeInt 32 0
%10 = OpTypeVector %9 3
%11 = OpTypePointer Input %10
%12 = OpVariable %11 Input
%13 = OpConstant %9 0
%14 = OpTypePointer Input %9
%19 = OpConstant %9 1
%24 = OpConstant %9 2
%28 = OpTypeImage %9 Cube 0 0 0 2 R32ui
%29 = OpTypePointer UniformConstant %28
%30 = OpVariable %29 UniformConstant
%32 = OpConstant %6 64
%36 = OpTypeVector %6 3
%50 = OpTypePointer Image %9
%53 = OpConstantComposite %10 %19 %19 %19
%4 = OpFunction %2 None %3
%5 = OpLabel
%8 = OpVariable %7 Function
%18 = OpVariable %7 Function
%23 = OpVariable %7 Function
%15 = OpAccessChain %14 %12 %13
%16 = OpLoad %9 %15
%17 = OpBitcast %6 %16
OpStore %8 %17
%20 = OpAccessChain %14 %12 %19
%21 = OpLoad %9 %20
%22 = OpBitcast %6 %21
OpStore %18 %22
%25 = OpAccessChain %14 %12 %24
%26 = OpLoad %9 %25
%27 = OpBitcast %6 %26
OpStore %23 %27
%31 = OpLoad %6 %8
%33 = OpSMod %6 %31 %32
%34 = OpLoad %6 %18
%35 = OpLoad %6 %23
%37 = OpCompositeConstruct %36 %33 %34 %35
%38 = OpLoad %6 %8
%39 = OpLoad %6 %8
%40 = OpIMul %6 %38 %39
%41 = OpLoad %6 %18
%42 = OpLoad %6 %18
%43 = OpIMul %6 %41 %42
%44 = OpIAdd %6 %40 %43
%45 = OpLoad %6 %23
%46 = OpLoad %6 %23
%47 = OpIMul %6 %45 %46
%48 = OpIAdd %6 %44 %47
%49 = OpBitcast %9 %48
%51 = OpImageTexelPointer %50 %30 %37 %13
%52 = ${OPNAME} %9 %51 %19 %13 ${LASTARG:default=%49}
OpReturn
OpFunctionEnd
)";

const std::string kShader_cube_r32ui_intermediate_values = R"(
; The SPIR-V shader below is based on the following GLSL shader, but OpAtomicIAdd has been
; replaced with a template parameter and the last argument for it has been made optional.
;
; #version 440
; precision highp uimageCube;
;
; layout (local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
; layout (r32ui, binding=0) coherent uniform uimageCube u_resultImage;
; layout (r32ui, binding=1) writeonly uniform uimageCube u_intermValuesImage;
;
; void main (void)
; {
;     int gx = int(gl_GlobalInvocationID.x);
;     int gy = int(gl_GlobalInvocationID.y);
;     int gz = int(gl_GlobalInvocationID.z);
;     imageStore(u_intermValuesImage, ivec3(gx,gy,gz), uvec4(imageAtomicAdd(u_resultImage, ivec3(gx % 64,gy,gz), uint(gx*gx + gy*gy + gz*gz))));
; }
;
; SPIR-V
; Version: 1.0
; Generator: Khronos Glslang Reference Front End; 7
; Bound: 62
; Schema: 0
OpCapability Shader
%1 = OpExtInstImport "GLSL.std.450"
OpMemoryModel Logical GLSL450
OpEntryPoint GLCompute %4 "main" %12
OpExecutionMode %4 LocalSize 1 1 1
OpDecorate %12 BuiltIn GlobalInvocationId
OpDecorate %30 DescriptorSet 0
OpDecorate %30 Binding 1
OpDecorate %30 NonReadable
OpDecorate %37 DescriptorSet 0
OpDecorate %37 Binding 0
OpDecorate %37 Coherent
OpDecorate %61 BuiltIn WorkgroupSize
%2 = OpTypeVoid
%3 = OpTypeFunction %2
%6 = OpTypeInt 32 1
%7 = OpTypePointer Function %6
%9 = OpTypeInt 32 0
%10 = OpTypeVector %9 3
%11 = OpTypePointer Input %10
%12 = OpVariable %11 Input
%13 = OpConstant %9 0
%14 = OpTypePointer Input %9
%19 = OpConstant %9 1
%24 = OpConstant %9 2
%28 = OpTypeImage %9 Cube 0 0 0 2 R32ui
%29 = OpTypePointer UniformConstant %28
%30 = OpVariable %29 UniformConstant
%35 = OpTypeVector %6 3
%37 = OpVariable %29 UniformConstant
%39 = OpConstant %6 64
%56 = OpTypePointer Image %9
%59 = OpTypeVector %9 4
%61 = OpConstantComposite %10 %19 %19 %19
%4 = OpFunction %2 None %3
%5 = OpLabel
%8 = OpVariable %7 Function
%18 = OpVariable %7 Function
%23 = OpVariable %7 Function
%15 = OpAccessChain %14 %12 %13
%16 = OpLoad %9 %15
%17 = OpBitcast %6 %16
OpStore %8 %17
%20 = OpAccessChain %14 %12 %19
%21 = OpLoad %9 %20
%22 = OpBitcast %6 %21
OpStore %18 %22
%25 = OpAccessChain %14 %12 %24
%26 = OpLoad %9 %25
%27 = OpBitcast %6 %26
OpStore %23 %27
%31 = OpLoad %28 %30
%32 = OpLoad %6 %8
%33 = OpLoad %6 %18
%34 = OpLoad %6 %23
%36 = OpCompositeConstruct %35 %32 %33 %34
%38 = OpLoad %6 %8
%40 = OpSMod %6 %38 %39
%41 = OpLoad %6 %18
%42 = OpLoad %6 %23
%43 = OpCompositeConstruct %35 %40 %41 %42
%44 = OpLoad %6 %8
%45 = OpLoad %6 %8
%46 = OpIMul %6 %44 %45
%47 = OpLoad %6 %18
%48 = OpLoad %6 %18
%49 = OpIMul %6 %47 %48
%50 = OpIAdd %6 %46 %49
%51 = OpLoad %6 %23
%52 = OpLoad %6 %23
%53 = OpIMul %6 %51 %52
%54 = OpIAdd %6 %50 %53
%55 = OpBitcast %9 %54
%57 = OpImageTexelPointer %56 %37 %43 %13
%58 = ${OPNAME} %9 %57 %19 %13 ${LASTARG:default=%55}
%60 = OpCompositeConstruct %59 %58 %58 %58 %58
OpImageWrite %31 %36 %60
OpReturn
OpFunctionEnd
)";

const std::string kShader_cube_r32i_end_result = R"(
; The SPIR-V shader below is based on the following GLSL shader, but OpAtomicIAdd has been
; replaced with a template parameter and the last argument for it has been made optional.
;
; #version 440
; precision highp iimageCube;
;
; layout (local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
; layout (r32i, binding=0) coherent uniform iimageCube u_resultImage;
;
; void main (void)
; {
;     int gx = int(gl_GlobalInvocationID.x);
;     int gy = int(gl_GlobalInvocationID.y);
;     int gz = int(gl_GlobalInvocationID.z);
;     imageAtomicAdd(u_resultImage, ivec3(gx % 64,gy,gz), int(gx*gx + gy*gy + gz*gz));
; }
;
; SPIR-V
; Version: 1.0
; Generator: Khronos Glslang Reference Front End; 7
; Bound: 53
; Schema: 0
OpCapability Shader
%1 = OpExtInstImport "GLSL.std.450"
OpMemoryModel Logical GLSL450
OpEntryPoint GLCompute %4 "main" %12
OpExecutionMode %4 LocalSize 1 1 1
OpDecorate %12 BuiltIn GlobalInvocationId
OpDecorate %30 DescriptorSet 0
OpDecorate %30 Binding 0
OpDecorate %30 Coherent
OpDecorate %52 BuiltIn WorkgroupSize
%2 = OpTypeVoid
%3 = OpTypeFunction %2
%6 = OpTypeInt 32 1
%7 = OpTypePointer Function %6
%9 = OpTypeInt 32 0
%10 = OpTypeVector %9 3
%11 = OpTypePointer Input %10
%12 = OpVariable %11 Input
%13 = OpConstant %9 0
%14 = OpTypePointer Input %9
%19 = OpConstant %9 1
%24 = OpConstant %9 2
%28 = OpTypeImage %6 Cube 0 0 0 2 R32i
%29 = OpTypePointer UniformConstant %28
%30 = OpVariable %29 UniformConstant
%32 = OpConstant %6 64
%36 = OpTypeVector %6 3
%49 = OpTypePointer Image %6
%52 = OpConstantComposite %10 %19 %19 %19
%4 = OpFunction %2 None %3
%5 = OpLabel
%8 = OpVariable %7 Function
%18 = OpVariable %7 Function
%23 = OpVariable %7 Function
%15 = OpAccessChain %14 %12 %13
%16 = OpLoad %9 %15
%17 = OpBitcast %6 %16
OpStore %8 %17
%20 = OpAccessChain %14 %12 %19
%21 = OpLoad %9 %20
%22 = OpBitcast %6 %21
OpStore %18 %22
%25 = OpAccessChain %14 %12 %24
%26 = OpLoad %9 %25
%27 = OpBitcast %6 %26
OpStore %23 %27
%31 = OpLoad %6 %8
%33 = OpSMod %6 %31 %32
%34 = OpLoad %6 %18
%35 = OpLoad %6 %23
%37 = OpCompositeConstruct %36 %33 %34 %35
%38 = OpLoad %6 %8
%39 = OpLoad %6 %8
%40 = OpIMul %6 %38 %39
%41 = OpLoad %6 %18
%42 = OpLoad %6 %18
%43 = OpIMul %6 %41 %42
%44 = OpIAdd %6 %40 %43
%45 = OpLoad %6 %23
%46 = OpLoad %6 %23
%47 = OpIMul %6 %45 %46
%48 = OpIAdd %6 %44 %47
%50 = OpImageTexelPointer %49 %30 %37 %13
%51 = ${OPNAME} %6 %50 %19 %13 ${LASTARG:default=%48}
OpReturn
OpFunctionEnd
)";

const std::string kShader_cube_r32i_intermediate_values = R"(
; The SPIR-V shader below is based on the following GLSL shader, but OpAtomicIAdd has been
; replaced with a template parameter and the last argument for it has been made optional.
;
; #version 440
; precision highp iimageCube;
;
; layout (local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
; layout (r32i, binding=0) coherent uniform iimageCube u_resultImage;
; layout (r32i, binding=1) writeonly uniform iimageCube u_intermValuesImage;
;
; void main (void)
; {
;     int gx = int(gl_GlobalInvocationID.x);
;     int gy = int(gl_GlobalInvocationID.y);
;     int gz = int(gl_GlobalInvocationID.z);
;     imageStore(u_intermValuesImage, ivec3(gx,gy,gz), ivec4(imageAtomicAdd(u_resultImage, ivec3(gx % 64,gy,gz), int(gx*gx + gy*gy + gz*gz))));
; }
;
; SPIR-V
; Version: 1.0
; Generator: Khronos Glslang Reference Front End; 7
; Bound: 61
; Schema: 0
OpCapability Shader
%1 = OpExtInstImport "GLSL.std.450"
OpMemoryModel Logical GLSL450
OpEntryPoint GLCompute %4 "main" %12
OpExecutionMode %4 LocalSize 1 1 1
OpDecorate %12 BuiltIn GlobalInvocationId
OpDecorate %30 DescriptorSet 0
OpDecorate %30 Binding 1
OpDecorate %30 NonReadable
OpDecorate %37 DescriptorSet 0
OpDecorate %37 Binding 0
OpDecorate %37 Coherent
OpDecorate %60 BuiltIn WorkgroupSize
%2 = OpTypeVoid
%3 = OpTypeFunction %2
%6 = OpTypeInt 32 1
%7 = OpTypePointer Function %6
%9 = OpTypeInt 32 0
%10 = OpTypeVector %9 3
%11 = OpTypePointer Input %10
%12 = OpVariable %11 Input
%13 = OpConstant %9 0
%14 = OpTypePointer Input %9
%19 = OpConstant %9 1
%24 = OpConstant %9 2
%28 = OpTypeImage %6 Cube 0 0 0 2 R32i
%29 = OpTypePointer UniformConstant %28
%30 = OpVariable %29 UniformConstant
%35 = OpTypeVector %6 3
%37 = OpVariable %29 UniformConstant
%39 = OpConstant %6 64
%55 = OpTypePointer Image %6
%58 = OpTypeVector %6 4
%60 = OpConstantComposite %10 %19 %19 %19
%4 = OpFunction %2 None %3
%5 = OpLabel
%8 = OpVariable %7 Function
%18 = OpVariable %7 Function
%23 = OpVariable %7 Function
%15 = OpAccessChain %14 %12 %13
%16 = OpLoad %9 %15
%17 = OpBitcast %6 %16
OpStore %8 %17
%20 = OpAccessChain %14 %12 %19
%21 = OpLoad %9 %20
%22 = OpBitcast %6 %21
OpStore %18 %22
%25 = OpAccessChain %14 %12 %24
%26 = OpLoad %9 %25
%27 = OpBitcast %6 %26
OpStore %23 %27
%31 = OpLoad %28 %30
%32 = OpLoad %6 %8
%33 = OpLoad %6 %18
%34 = OpLoad %6 %23
%36 = OpCompositeConstruct %35 %32 %33 %34
%38 = OpLoad %6 %8
%40 = OpSMod %6 %38 %39
%41 = OpLoad %6 %18
%42 = OpLoad %6 %23
%43 = OpCompositeConstruct %35 %40 %41 %42
%44 = OpLoad %6 %8
%45 = OpLoad %6 %8
%46 = OpIMul %6 %44 %45
%47 = OpLoad %6 %18
%48 = OpLoad %6 %18
%49 = OpIMul %6 %47 %48
%50 = OpIAdd %6 %46 %49
%51 = OpLoad %6 %23
%52 = OpLoad %6 %23
%53 = OpIMul %6 %51 %52
%54 = OpIAdd %6 %50 %53
%56 = OpImageTexelPointer %55 %37 %43 %13
%57 = ${OPNAME} %6 %56 %19 %13 ${LASTARG:default=%54}
%59 = OpCompositeConstruct %58 %57 %57 %57 %57
OpImageWrite %31 %36 %59
OpReturn
OpFunctionEnd
)";

const std::string kShader_cube_array_r32ui_end_result = R"(
; The SPIR-V shader below is based on the following GLSL shader, but OpAtomicIAdd has been
; replaced with a template parameter and the last argument for it has been made optional.
;
; #version 440
; precision highp uimageCubeArray;
;
; layout (local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
; layout (r32ui, binding=0) coherent uniform uimageCubeArray u_resultImage;
;
; void main (void)
; {
;     int gx = int(gl_GlobalInvocationID.x);
;     int gy = int(gl_GlobalInvocationID.y);
;     int gz = int(gl_GlobalInvocationID.z);
;     imageAtomicAdd(u_resultImage, ivec3(gx % 64,gy,gz), uint(gx*gx + gy*gy + gz*gz));
; }
;
; SPIR-V
; Version: 1.0
; Generator: Khronos Glslang Reference Front End; 7
; Bound: 54
; Schema: 0
OpCapability Shader
OpCapability ImageCubeArray
%1 = OpExtInstImport "GLSL.std.450"
OpMemoryModel Logical GLSL450
OpEntryPoint GLCompute %4 "main" %12
OpExecutionMode %4 LocalSize 1 1 1
OpDecorate %12 BuiltIn GlobalInvocationId
OpDecorate %30 DescriptorSet 0
OpDecorate %30 Binding 0
OpDecorate %30 Coherent
OpDecorate %53 BuiltIn WorkgroupSize
%2 = OpTypeVoid
%3 = OpTypeFunction %2
%6 = OpTypeInt 32 1
%7 = OpTypePointer Function %6
%9 = OpTypeInt 32 0
%10 = OpTypeVector %9 3
%11 = OpTypePointer Input %10
%12 = OpVariable %11 Input
%13 = OpConstant %9 0
%14 = OpTypePointer Input %9
%19 = OpConstant %9 1
%24 = OpConstant %9 2
%28 = OpTypeImage %9 Cube 0 1 0 2 R32ui
%29 = OpTypePointer UniformConstant %28
%30 = OpVariable %29 UniformConstant
%32 = OpConstant %6 64
%36 = OpTypeVector %6 3
%50 = OpTypePointer Image %9
%53 = OpConstantComposite %10 %19 %19 %19
%4 = OpFunction %2 None %3
%5 = OpLabel
%8 = OpVariable %7 Function
%18 = OpVariable %7 Function
%23 = OpVariable %7 Function
%15 = OpAccessChain %14 %12 %13
%16 = OpLoad %9 %15
%17 = OpBitcast %6 %16
OpStore %8 %17
%20 = OpAccessChain %14 %12 %19
%21 = OpLoad %9 %20
%22 = OpBitcast %6 %21
OpStore %18 %22
%25 = OpAccessChain %14 %12 %24
%26 = OpLoad %9 %25
%27 = OpBitcast %6 %26
OpStore %23 %27
%31 = OpLoad %6 %8
%33 = OpSMod %6 %31 %32
%34 = OpLoad %6 %18
%35 = OpLoad %6 %23
%37 = OpCompositeConstruct %36 %33 %34 %35
%38 = OpLoad %6 %8
%39 = OpLoad %6 %8
%40 = OpIMul %6 %38 %39
%41 = OpLoad %6 %18
%42 = OpLoad %6 %18
%43 = OpIMul %6 %41 %42
%44 = OpIAdd %6 %40 %43
%45 = OpLoad %6 %23
%46 = OpLoad %6 %23
%47 = OpIMul %6 %45 %46
%48 = OpIAdd %6 %44 %47
%49 = OpBitcast %9 %48
%51 = OpImageTexelPointer %50 %30 %37 %13
%52 = ${OPNAME} %9 %51 %19 %13 ${LASTARG:default=%49}
OpReturn
OpFunctionEnd
)";

const std::string kShader_cube_array_r32ui_intermediate_values = R"(
; The SPIR-V shader below is based on the following GLSL shader, but OpAtomicIAdd has been
; replaced with a template parameter and the last argument for it has been made optional.
;
; #version 440
; precision highp uimageCubeArray;
;
; layout (local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
; layout (r32ui, binding=0) coherent uniform uimageCubeArray u_resultImage;
; layout (r32ui, binding=1) writeonly uniform uimageCubeArray u_intermValuesImage;
;
; void main (void)
; {
;     int gx = int(gl_GlobalInvocationID.x);
;     int gy = int(gl_GlobalInvocationID.y);
;     int gz = int(gl_GlobalInvocationID.z);
;     imageStore(u_intermValuesImage, ivec3(gx,gy,gz), uvec4(imageAtomicAdd(u_resultImage, ivec3(gx % 64,gy,gz), uint(gx*gx + gy*gy + gz*gz))));
; }
;
; SPIR-V
; Version: 1.0
; Generator: Khronos Glslang Reference Front End; 7
; Bound: 62
; Schema: 0
OpCapability Shader
OpCapability ImageCubeArray
%1 = OpExtInstImport "GLSL.std.450"
OpMemoryModel Logical GLSL450
OpEntryPoint GLCompute %4 "main" %12
OpExecutionMode %4 LocalSize 1 1 1
OpDecorate %12 BuiltIn GlobalInvocationId
OpDecorate %30 DescriptorSet 0
OpDecorate %30 Binding 1
OpDecorate %30 NonReadable
OpDecorate %37 DescriptorSet 0
OpDecorate %37 Binding 0
OpDecorate %37 Coherent
OpDecorate %61 BuiltIn WorkgroupSize
%2 = OpTypeVoid
%3 = OpTypeFunction %2
%6 = OpTypeInt 32 1
%7 = OpTypePointer Function %6
%9 = OpTypeInt 32 0
%10 = OpTypeVector %9 3
%11 = OpTypePointer Input %10
%12 = OpVariable %11 Input
%13 = OpConstant %9 0
%14 = OpTypePointer Input %9
%19 = OpConstant %9 1
%24 = OpConstant %9 2
%28 = OpTypeImage %9 Cube 0 1 0 2 R32ui
%29 = OpTypePointer UniformConstant %28
%30 = OpVariable %29 UniformConstant
%35 = OpTypeVector %6 3
%37 = OpVariable %29 UniformConstant
%39 = OpConstant %6 64
%56 = OpTypePointer Image %9
%59 = OpTypeVector %9 4
%61 = OpConstantComposite %10 %19 %19 %19
%4 = OpFunction %2 None %3
%5 = OpLabel
%8 = OpVariable %7 Function
%18 = OpVariable %7 Function
%23 = OpVariable %7 Function
%15 = OpAccessChain %14 %12 %13
%16 = OpLoad %9 %15
%17 = OpBitcast %6 %16
OpStore %8 %17
%20 = OpAccessChain %14 %12 %19
%21 = OpLoad %9 %20
%22 = OpBitcast %6 %21
OpStore %18 %22
%25 = OpAccessChain %14 %12 %24
%26 = OpLoad %9 %25
%27 = OpBitcast %6 %26
OpStore %23 %27
%31 = OpLoad %28 %30
%32 = OpLoad %6 %8
%33 = OpLoad %6 %18
%34 = OpLoad %6 %23
%36 = OpCompositeConstruct %35 %32 %33 %34
%38 = OpLoad %6 %8
%40 = OpSMod %6 %38 %39
%41 = OpLoad %6 %18
%42 = OpLoad %6 %23
%43 = OpCompositeConstruct %35 %40 %41 %42
%44 = OpLoad %6 %8
%45 = OpLoad %6 %8
%46 = OpIMul %6 %44 %45
%47 = OpLoad %6 %18
%48 = OpLoad %6 %18
%49 = OpIMul %6 %47 %48
%50 = OpIAdd %6 %46 %49
%51 = OpLoad %6 %23
%52 = OpLoad %6 %23
%53 = OpIMul %6 %51 %52
%54 = OpIAdd %6 %50 %53
%55 = OpBitcast %9 %54
%57 = OpImageTexelPointer %56 %37 %43 %13
%58 = ${OPNAME} %9 %57 %19 %13 ${LASTARG:default=%55}
%60 = OpCompositeConstruct %59 %58 %58 %58 %58
OpImageWrite %31 %36 %60
OpReturn
OpFunctionEnd
)";

const std::string kShader_cube_array_r32i_end_result = R"(
; The SPIR-V shader below is based on the following GLSL shader, but OpAtomicIAdd has been
; replaced with a template parameter and the last argument for it has been made optional.
;
; #version 440
; precision highp iimageCubeArray;
;
; layout (local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
; layout (r32i, binding=0) coherent uniform iimageCubeArray u_resultImage;
;
; void main (void)
; {
;     int gx = int(gl_GlobalInvocationID.x);
;     int gy = int(gl_GlobalInvocationID.y);
;     int gz = int(gl_GlobalInvocationID.z);
;     imageAtomicAdd(u_resultImage, ivec3(gx % 64,gy,gz), int(gx*gx + gy*gy + gz*gz));
; }
;
; SPIR-V
; Version: 1.0
; Generator: Khronos Glslang Reference Front End; 7
; Bound: 53
; Schema: 0
OpCapability Shader
OpCapability ImageCubeArray
%1 = OpExtInstImport "GLSL.std.450"
OpMemoryModel Logical GLSL450
OpEntryPoint GLCompute %4 "main" %12
OpExecutionMode %4 LocalSize 1 1 1
OpDecorate %12 BuiltIn GlobalInvocationId
OpDecorate %30 DescriptorSet 0
OpDecorate %30 Binding 0
OpDecorate %30 Coherent
OpDecorate %52 BuiltIn WorkgroupSize
%2 = OpTypeVoid
%3 = OpTypeFunction %2
%6 = OpTypeInt 32 1
%7 = OpTypePointer Function %6
%9 = OpTypeInt 32 0
%10 = OpTypeVector %9 3
%11 = OpTypePointer Input %10
%12 = OpVariable %11 Input
%13 = OpConstant %9 0
%14 = OpTypePointer Input %9
%19 = OpConstant %9 1
%24 = OpConstant %9 2
%28 = OpTypeImage %6 Cube 0 1 0 2 R32i
%29 = OpTypePointer UniformConstant %28
%30 = OpVariable %29 UniformConstant
%32 = OpConstant %6 64
%36 = OpTypeVector %6 3
%49 = OpTypePointer Image %6
%52 = OpConstantComposite %10 %19 %19 %19
%4 = OpFunction %2 None %3
%5 = OpLabel
%8 = OpVariable %7 Function
%18 = OpVariable %7 Function
%23 = OpVariable %7 Function
%15 = OpAccessChain %14 %12 %13
%16 = OpLoad %9 %15
%17 = OpBitcast %6 %16
OpStore %8 %17
%20 = OpAccessChain %14 %12 %19
%21 = OpLoad %9 %20
%22 = OpBitcast %6 %21
OpStore %18 %22
%25 = OpAccessChain %14 %12 %24
%26 = OpLoad %9 %25
%27 = OpBitcast %6 %26
OpStore %23 %27
%31 = OpLoad %6 %8
%33 = OpSMod %6 %31 %32
%34 = OpLoad %6 %18
%35 = OpLoad %6 %23
%37 = OpCompositeConstruct %36 %33 %34 %35
%38 = OpLoad %6 %8
%39 = OpLoad %6 %8
%40 = OpIMul %6 %38 %39
%41 = OpLoad %6 %18
%42 = OpLoad %6 %18
%43 = OpIMul %6 %41 %42
%44 = OpIAdd %6 %40 %43
%45 = OpLoad %6 %23
%46 = OpLoad %6 %23
%47 = OpIMul %6 %45 %46
%48 = OpIAdd %6 %44 %47
%50 = OpImageTexelPointer %49 %30 %37 %13
%51 = ${OPNAME} %6 %50 %19 %13 ${LASTARG:default=%48}
OpReturn
OpFunctionEnd
)";

const std::string kShader_cube_array_r32i_intermediate_values = R"(
; The SPIR-V shader below is based on the following GLSL shader, but OpAtomicIAdd has been
; replaced with a template parameter and the last argument for it has been made optional.
;
; #version 440
; precision highp iimageCubeArray;
;
; layout (local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
; layout (r32i, binding=0) coherent uniform iimageCubeArray u_resultImage;
; layout (r32i, binding=1) writeonly uniform iimageCubeArray u_intermValuesImage;
;
; void main (void)
; {
;     int gx = int(gl_GlobalInvocationID.x);
;     int gy = int(gl_GlobalInvocationID.y);
;     int gz = int(gl_GlobalInvocationID.z);
;     imageStore(u_intermValuesImage, ivec3(gx,gy,gz), ivec4(imageAtomicAdd(u_resultImage, ivec3(gx % 64,gy,gz), int(gx*gx + gy*gy + gz*gz))));
; }
;
; SPIR-V
; Version: 1.0
; Generator: Khronos Glslang Reference Front End; 7
; Bound: 61
; Schema: 0
OpCapability Shader
OpCapability ImageCubeArray
%1 = OpExtInstImport "GLSL.std.450"
OpMemoryModel Logical GLSL450
OpEntryPoint GLCompute %4 "main" %12
OpExecutionMode %4 LocalSize 1 1 1
OpDecorate %12 BuiltIn GlobalInvocationId
OpDecorate %30 DescriptorSet 0
OpDecorate %30 Binding 1
OpDecorate %30 NonReadable
OpDecorate %37 DescriptorSet 0
OpDecorate %37 Binding 0
OpDecorate %37 Coherent
OpDecorate %60 BuiltIn WorkgroupSize
%2 = OpTypeVoid
%3 = OpTypeFunction %2
%6 = OpTypeInt 32 1
%7 = OpTypePointer Function %6
%9 = OpTypeInt 32 0
%10 = OpTypeVector %9 3
%11 = OpTypePointer Input %10
%12 = OpVariable %11 Input
%13 = OpConstant %9 0
%14 = OpTypePointer Input %9
%19 = OpConstant %9 1
%24 = OpConstant %9 2
%28 = OpTypeImage %6 Cube 0 1 0 2 R32i
%29 = OpTypePointer UniformConstant %28
%30 = OpVariable %29 UniformConstant
%35 = OpTypeVector %6 3
%37 = OpVariable %29 UniformConstant
%39 = OpConstant %6 64
%55 = OpTypePointer Image %6
%58 = OpTypeVector %6 4
%60 = OpConstantComposite %10 %19 %19 %19
%4 = OpFunction %2 None %3
%5 = OpLabel
%8 = OpVariable %7 Function
%18 = OpVariable %7 Function
%23 = OpVariable %7 Function
%15 = OpAccessChain %14 %12 %13
%16 = OpLoad %9 %15
%17 = OpBitcast %6 %16
OpStore %8 %17
%20 = OpAccessChain %14 %12 %19
%21 = OpLoad %9 %20
%22 = OpBitcast %6 %21
OpStore %18 %22
%25 = OpAccessChain %14 %12 %24
%26 = OpLoad %9 %25
%27 = OpBitcast %6 %26
OpStore %23 %27
%31 = OpLoad %28 %30
%32 = OpLoad %6 %8
%33 = OpLoad %6 %18
%34 = OpLoad %6 %23
%36 = OpCompositeConstruct %35 %32 %33 %34
%38 = OpLoad %6 %8
%40 = OpSMod %6 %38 %39
%41 = OpLoad %6 %18
%42 = OpLoad %6 %23
%43 = OpCompositeConstruct %35 %40 %41 %42
%44 = OpLoad %6 %8
%45 = OpLoad %6 %8
%46 = OpIMul %6 %44 %45
%47 = OpLoad %6 %18
%48 = OpLoad %6 %18
%49 = OpIMul %6 %47 %48
%50 = OpIAdd %6 %46 %49
%51 = OpLoad %6 %23
%52 = OpLoad %6 %23
%53 = OpIMul %6 %51 %52
%54 = OpIAdd %6 %50 %53
%56 = OpImageTexelPointer %55 %37 %43 %13
%57 = ${OPNAME} %6 %56 %19 %13 ${LASTARG:default=%54}
%59 = OpCompositeConstruct %58 %57 %57 %57 %57
OpImageWrite %31 %36 %59
OpReturn
OpFunctionEnd
)";

} // anonymous namespace

bool CaseVariant::operator< (const CaseVariant& other) const
{
	// Simple lexicographical comparison using the struct members.
	const std::array<int, 4> thisMembers =
	{{
		static_cast<int>(imageType),
		static_cast<int>(textureFormat.order),
		static_cast<int>(textureFormat.type),
		static_cast<int>(checkType),
	}};

	const std::array<int, 4> otherMembers =
	{{
		static_cast<int>(other.imageType),
		static_cast<int>(other.textureFormat.order),
		static_cast<int>(other.textureFormat.type),
		static_cast<int>(other.checkType),
	}};

	return thisMembers < otherMembers;
}

CaseVariant::CaseVariant (ImageType imgtype, tcu::TextureFormat::ChannelOrder order, tcu::TextureFormat::ChannelType chtype, CheckType cktype)
	: imageType{imgtype}, textureFormat{order, chtype}, checkType{cktype}
{}

std::string getSpirvAtomicOpShader (const CaseVariant& caseVariant)
{
	using ShadersMapT	= std::map<CaseVariant, const std::string*>;
	using ValueType		= ShadersMapT::value_type;

	static const ShadersMapT kShadersMap =
	{
		ValueType{CaseVariant{IMAGE_TYPE_1D,			tcu::TextureFormat::R,	tcu::TextureFormat::UNSIGNED_INT32,	CaseVariant::CHECK_TYPE_END_RESULTS},			&kShader_1d_r32ui_end_result},
		ValueType{CaseVariant{IMAGE_TYPE_1D,			tcu::TextureFormat::R,	tcu::TextureFormat::UNSIGNED_INT32,	CaseVariant::CHECK_TYPE_INTERMEDIATE_RESULTS},	&kShader_1d_r32ui_intermediate_values},
		ValueType{CaseVariant{IMAGE_TYPE_1D,			tcu::TextureFormat::R,	tcu::TextureFormat::SIGNED_INT32,	CaseVariant::CHECK_TYPE_END_RESULTS},			&kShader_1d_r32i_end_result},
		ValueType{CaseVariant{IMAGE_TYPE_1D,			tcu::TextureFormat::R,	tcu::TextureFormat::SIGNED_INT32,	CaseVariant::CHECK_TYPE_INTERMEDIATE_RESULTS},	&kShader_1d_r32i_intermediate_values},
		ValueType{CaseVariant{IMAGE_TYPE_1D_ARRAY,		tcu::TextureFormat::R,	tcu::TextureFormat::UNSIGNED_INT32,	CaseVariant::CHECK_TYPE_END_RESULTS},			&kShader_1d_array_r32ui_end_result},
		ValueType{CaseVariant{IMAGE_TYPE_1D_ARRAY,		tcu::TextureFormat::R,	tcu::TextureFormat::UNSIGNED_INT32,	CaseVariant::CHECK_TYPE_INTERMEDIATE_RESULTS},	&kShader_1d_array_r32ui_intermediate_values},
		ValueType{CaseVariant{IMAGE_TYPE_1D_ARRAY,		tcu::TextureFormat::R,	tcu::TextureFormat::SIGNED_INT32,	CaseVariant::CHECK_TYPE_END_RESULTS},			&kShader_1d_array_r32i_end_result},
		ValueType{CaseVariant{IMAGE_TYPE_1D_ARRAY,		tcu::TextureFormat::R,	tcu::TextureFormat::SIGNED_INT32,	CaseVariant::CHECK_TYPE_INTERMEDIATE_RESULTS},	&kShader_1d_array_r32i_intermediate_values},
		ValueType{CaseVariant{IMAGE_TYPE_2D,			tcu::TextureFormat::R,	tcu::TextureFormat::UNSIGNED_INT32,	CaseVariant::CHECK_TYPE_END_RESULTS},			&kShader_2d_r32ui_end_result},
		ValueType{CaseVariant{IMAGE_TYPE_2D,			tcu::TextureFormat::R,	tcu::TextureFormat::UNSIGNED_INT32,	CaseVariant::CHECK_TYPE_INTERMEDIATE_RESULTS},	&kShader_2d_r32ui_intermediate_values},
		ValueType{CaseVariant{IMAGE_TYPE_2D,			tcu::TextureFormat::R,	tcu::TextureFormat::SIGNED_INT32,	CaseVariant::CHECK_TYPE_END_RESULTS},			&kShader_2d_r32i_end_result},
		ValueType{CaseVariant{IMAGE_TYPE_2D,			tcu::TextureFormat::R,	tcu::TextureFormat::SIGNED_INT32,	CaseVariant::CHECK_TYPE_INTERMEDIATE_RESULTS},	&kShader_2d_r32i_intermediate_values},
		ValueType{CaseVariant{IMAGE_TYPE_2D_ARRAY,		tcu::TextureFormat::R,	tcu::TextureFormat::UNSIGNED_INT32,	CaseVariant::CHECK_TYPE_END_RESULTS},			&kShader_2d_array_r32ui_end_result},
		ValueType{CaseVariant{IMAGE_TYPE_2D_ARRAY,		tcu::TextureFormat::R,	tcu::TextureFormat::UNSIGNED_INT32,	CaseVariant::CHECK_TYPE_INTERMEDIATE_RESULTS},	&kShader_2d_array_r32ui_intermediate_values},
		ValueType{CaseVariant{IMAGE_TYPE_2D_ARRAY,		tcu::TextureFormat::R,	tcu::TextureFormat::SIGNED_INT32,	CaseVariant::CHECK_TYPE_END_RESULTS},			&kShader_2d_array_r32i_end_result},
		ValueType{CaseVariant{IMAGE_TYPE_2D_ARRAY,		tcu::TextureFormat::R,	tcu::TextureFormat::SIGNED_INT32,	CaseVariant::CHECK_TYPE_INTERMEDIATE_RESULTS},	&kShader_2d_array_r32i_intermediate_values},
		ValueType{CaseVariant{IMAGE_TYPE_3D,			tcu::TextureFormat::R,	tcu::TextureFormat::UNSIGNED_INT32,	CaseVariant::CHECK_TYPE_END_RESULTS},			&kShader_3d_r32ui_end_result},
		ValueType{CaseVariant{IMAGE_TYPE_3D,			tcu::TextureFormat::R,	tcu::TextureFormat::UNSIGNED_INT32,	CaseVariant::CHECK_TYPE_INTERMEDIATE_RESULTS},	&kShader_3d_r32ui_intermediate_values},
		ValueType{CaseVariant{IMAGE_TYPE_3D,			tcu::TextureFormat::R,	tcu::TextureFormat::SIGNED_INT32,	CaseVariant::CHECK_TYPE_END_RESULTS},			&kShader_3d_r32i_end_result},
		ValueType{CaseVariant{IMAGE_TYPE_3D,			tcu::TextureFormat::R,	tcu::TextureFormat::SIGNED_INT32,	CaseVariant::CHECK_TYPE_INTERMEDIATE_RESULTS},	&kShader_3d_r32i_intermediate_values},
		ValueType{CaseVariant{IMAGE_TYPE_CUBE,			tcu::TextureFormat::R,	tcu::TextureFormat::UNSIGNED_INT32,	CaseVariant::CHECK_TYPE_END_RESULTS},			&kShader_cube_r32ui_end_result},
		ValueType{CaseVariant{IMAGE_TYPE_CUBE,			tcu::TextureFormat::R,	tcu::TextureFormat::UNSIGNED_INT32,	CaseVariant::CHECK_TYPE_INTERMEDIATE_RESULTS},	&kShader_cube_r32ui_intermediate_values},
		ValueType{CaseVariant{IMAGE_TYPE_CUBE,			tcu::TextureFormat::R,	tcu::TextureFormat::SIGNED_INT32,	CaseVariant::CHECK_TYPE_END_RESULTS},			&kShader_cube_r32i_end_result},
		ValueType{CaseVariant{IMAGE_TYPE_CUBE,			tcu::TextureFormat::R,	tcu::TextureFormat::SIGNED_INT32,	CaseVariant::CHECK_TYPE_INTERMEDIATE_RESULTS},	&kShader_cube_r32i_intermediate_values},
		ValueType{CaseVariant{IMAGE_TYPE_CUBE_ARRAY,	tcu::TextureFormat::R,	tcu::TextureFormat::UNSIGNED_INT32,	CaseVariant::CHECK_TYPE_END_RESULTS},			&kShader_cube_array_r32ui_end_result},
		ValueType{CaseVariant{IMAGE_TYPE_CUBE_ARRAY,	tcu::TextureFormat::R,	tcu::TextureFormat::UNSIGNED_INT32,	CaseVariant::CHECK_TYPE_INTERMEDIATE_RESULTS},	&kShader_cube_array_r32ui_intermediate_values},
		ValueType{CaseVariant{IMAGE_TYPE_CUBE_ARRAY,	tcu::TextureFormat::R,	tcu::TextureFormat::SIGNED_INT32,	CaseVariant::CHECK_TYPE_END_RESULTS},			&kShader_cube_array_r32i_end_result},
		ValueType{CaseVariant{IMAGE_TYPE_CUBE_ARRAY,	tcu::TextureFormat::R,	tcu::TextureFormat::SIGNED_INT32,	CaseVariant::CHECK_TYPE_INTERMEDIATE_RESULTS},	&kShader_cube_array_r32i_intermediate_values},
	};

	const auto iter = kShadersMap.find(caseVariant);
	DE_ASSERT(iter != kShadersMap.end());
	return *(iter->second);
}

} // namespace image
} // namespace vkt

// Note: the SPIR-V shaders above were created using the atomic addition shaders as a base, replacing OpAtomicIAdd with a string
// template and making the last operation argument optional. Because the atomic addition shaders are generated, the final version of
// each shader was obtained from TestResults.qpa after running the whole atomic addition group, using the Python script documented
// below.
#if 0
import html
import re
import sys

STATE_OUT = 0
STATE_GLSL = 1
STATE_SPIRV = 2

state = STATE_OUT
test_name = None
glsl_lines = []
spirv_lines = []
header_printed = False

for line in sys.stdin:
	if line.startswith("#beginTestCaseResult"):
		test_name = line.strip().split()[1]
		test_name = "_".join(test_name.split(".")[-2:])

	if "<ShaderSource>" in line:
		line = re.sub(r".*<ShaderSource>", "", line)
		state = STATE_GLSL

	if "</ShaderSource>" in line:
		state = STATE_OUT

	if "<SpirVAssemblySource>" in line:
		line = re.sub(r".*<SpirVAssemblySource>", "", line)
		state = STATE_SPIRV

	if "</SpirVAssemblySource>" in line:
		state = STATE_OUT
		if not header_printed:
			print("#include <string>")
			print()
			header_printed = True
		print("const std::string kShader_%s = R\"(" % (test_name,))
		print("; The SPIR-V shader below is based on the following GLSL shader, but OpAtomicIAdd has been")
		print("; replaced with a template parameter and the last argument for it has been made optional.")
		print(";")
		for glsl_line in glsl_lines:
			glsl_line = html.unescape(glsl_line)
			print("; %s" % (glsl_line,), end="")
		print(";")
		for spirv_line in spirv_lines:
			spirv_line = html.unescape(spirv_line)
			if "OpAtomicIAdd" in spirv_line:
				words = spirv_line.strip().split()
				for i in range(len(words)):
					if words[i] == "OpAtomicIAdd":
						words[i] = r"${OPNAME}"
				words[-1] = r"${LASTARG:default=%s}" % (words[-1], )
				spirv_line = " ".join(words) + "\n"
			print("%s" % (spirv_line, ), end="")
		print(")\";")
		print()

		test_name = None
		glsl_lines = []
		spirv_lines = []

	if state == STATE_GLSL:
		glsl_lines.append(line)
	elif state == STATE_SPIRV:
		spirv_lines.append(line)
#endif
