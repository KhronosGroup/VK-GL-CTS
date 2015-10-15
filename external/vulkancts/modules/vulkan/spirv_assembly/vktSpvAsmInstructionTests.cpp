/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2015 Google Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and/or associated documentation files (the
 * "Materials"), to deal in the Materials without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Materials, and to
 * permit persons to whom the Materials are furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice(s) and this permission notice shall be
 * included in all copies or substantial portions of the Materials.
 *
 * The Materials are Confidential Information as defined by the
 * Khronos Membership Agreement until designated non-confidential by
 * Khronos, at which point this condition clause shall be removed.
 *
 * THE MATERIALS ARE PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * MATERIALS OR THE USE OR OTHER DEALINGS IN THE MATERIALS.
 *
 *//*!
 * \file
 * \brief SPIR-V Assembly Tests for Instructions (special opcode/operand)
 *//*--------------------------------------------------------------------*/

#include "vktSpvAsmInstructionTests.hpp"

#include "deUniquePtr.hpp"

#include "vktSpvAsmComputeShaderCase.hpp"
#include "vktSpvAsmComputeShaderTestUtil.hpp"

namespace vkt
{
namespace SpirVAssembly
{

namespace
{

using std::vector;
using tcu::IVec3;

SpvAsmComputeShaderCase* createOpNopTestCase (tcu::TestContext& testCtx)
{
	ComputeShaderSpec spec;
	// Based on GLSL source code:
	//
	// #version 430
	//
	// layout(std140, set = 0, binding = 0) readonly buffer Input {
	//   float elements[];
	// } input_data;
	// layout(std140, set = 0, binding = 1) writeonly buffer Output {
	//   float elements[];
	// } output_data;
	//
	// layout (local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
	//
	// void main() {
	//   uint x = gl_GlobalInvocationID.x;
	//   output_data.elements[x] = -input_data.elements[x];
	// }
	spec.assembly =
		"OpNop\n" // As the first instruction

		"OpSource GLSL 430\n"
		"OpCapability Shader\n"
		"%std450 = OpExtInstImport \"GLSL.std.450\"\n"
		"OpMemoryModel Logical GLSL450\n"

		"OpEntryPoint GLCompute %main \"main\" %id\n"

		"OpNop\n" // After OpEntryPoint but before any type definitions

		"OpExecutionMode %main LocalSize 1 1 1\n"

		"OpName %main           \"main\"\n"
		"OpName %id             \"gl_GlobalInvocationID\"\n"
		"OpName %outbuf         \"Output\"\n"
		"OpMemberName %outbuf 0 \"elements\"\n"
		"OpName %outdata        \"output_data\"\n"
		"OpName %inbuf          \"Input\"\n"
		"OpMemberName %inbuf 0  \"elements\"\n"
		"OpName %indata         \"input_data\"\n"

		"OpDecorate %id BuiltIn GlobalInvocationId\n"
		"OpDecorate %inbuf BufferBlock\n"
		"OpDecorate %indata DescriptorSet 0\n"
		"OpDecorate %indata Binding 0\n"
		"OpDecorate %outbuf BufferBlock\n"
		"OpDecorate %outdata DescriptorSet 0\n"
		"OpDecorate %outdata Binding 1\n"
		"OpDecorate %f32arr ArrayStride 4\n"
		"OpMemberDecorate %inbuf 0 Offset 0\n"
		"OpMemberDecorate %outbuf 0 Offset 0\n"

		"%void      = OpTypeVoid\n"
		"%voidf     = OpTypeFunction %void\n"

		"             OpNop\n" // In the middle of type definitions

		"%u32       = OpTypeInt 32 0\n"
		"%uvec3     = OpTypeVector %u32 3\n"
		"%uvec3ptr  = OpTypePointer Input %uvec3\n"
		"%f32       = OpTypeFloat 32\n"
		"%f32ptr    = OpTypePointer Uniform %f32\n"
		"%f32arr    = OpTypeRuntimeArray %f32\n"
		"%outbuf    = OpTypeStruct %f32arr\n"
		"%outbufptr = OpTypePointer Uniform %outbuf\n"
		"%inbuf     = OpTypeStruct %f32arr\n"
		"%inbufptr  = OpTypePointer Uniform %inbuf\n"
		"%i32       = OpTypeInt 32 1\n"

		"%id        = OpVariable %uvec3ptr Input\n"
		"%indata    = OpVariable %inbufptr Uniform\n"
		"%outdata   = OpVariable %outbufptr Uniform\n"
		"%zero      = OpConstant %i32 0\n"

		"%main      = OpFunction %void None %voidf\n"
		"%label     = OpLabel\n"
		"%idval     = OpLoad %uvec3 %id\n"
		"%x         = OpCompositeExtract %u32 %idval 0\n"

		"             OpNop\n" // Inside a function body

		"%inloc     = OpAccessChain %f32ptr %indata %zero %x\n"
		"%inval     = OpLoad %f32 %inloc\n"
		"%neg       = OpFNegate %f32 %inval\n"
		"%outloc    = OpAccessChain %f32ptr %outdata %zero %x\n"
		"             OpStore %outloc %neg\n"
		"             OpReturn\n"
		"             OpFunctionEnd\n";

	const int numElements = 100;
	vector<float> positiveFloats(numElements, 42.42f);
	vector<float> negativeFloats(numElements, -42.42f);

	spec.inputs.push_back(BufferSp(new Float32Buffer(positiveFloats)));
	spec.outputs.push_back(BufferSp(new Float32Buffer(negativeFloats)));

	spec.numWorkGroups = IVec3(numElements, 1, 1);

	return new SpvAsmComputeShaderCase(testCtx, "opnop", "Test the OpNop instruction", spec);
}

} // anonymous

tcu::TestCaseGroup* createInstructionTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> instructionTests (new tcu::TestCaseGroup(testCtx, "instruction", "Instructions with special opcodes/operands"));

	instructionTests->addChild(createOpNopTestCase(testCtx));

	return instructionTests.release();
}

} // SpirVAssembly
} // vkt
