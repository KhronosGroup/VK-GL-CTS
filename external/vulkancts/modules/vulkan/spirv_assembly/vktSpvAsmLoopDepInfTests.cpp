/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2017 The Khronos Group Inc.
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
 * \brief SPIR-V Loop Control for DependencyInfinite qualifier tests
 *//*--------------------------------------------------------------------*/

#include "vkApiVersion.hpp"

#include "vktSpvAsmLoopDepInfTests.hpp"
#include "vktTestCase.hpp"
#include "vktSpvAsmComputeShaderCase.hpp"

#include "deRandom.hpp"

namespace vkt
{
namespace SpirVAssembly
{

using namespace vk;
using std::map;
using std::string;
using std::vector;

// Assembly code used for testing loop control with dependencies is based on GLSL source code:
// #version 430
//
// layout(std140, set = 0, binding = 0) readonly buffer Input {
//   float elements[];
// } input_data;
// layout(std140, set = 0, binding = 1) writeonly buffer Output {
//   float elements[];
// } output_data;
//
// void main() {
//   const uint n = 12;
//   float c[n];
//   uint x = gl_GlobalInvocationID.x;
//
//   for (uint i = 0; i < n; ++i)
//     c[i] = float(i) * input_data.elements[x];
//
//   output_data.elements[x] = 0.0f;
//   for (uint i = 0; i < n; ++i)
//     output_data.elements[x] += c[i];
// }
static void getComputeSourceCode (std::string& computeSourceCode)
{
	computeSourceCode =
		string(getComputeAsmShaderPreamble()) +

		"OpSource GLSL 430\n"
		"OpName %main \"main\"\n"
		"OpName %id \"gl_GlobalInvocationID\"\n"

		"OpDecorate %id BuiltIn GlobalInvocationId\n"

		+ string(getComputeAsmInputOutputBufferTraits()) + string(getComputeAsmCommonTypes()) + string(getComputeAsmInputOutputBuffer()) +

		"%u32ptr        = OpTypePointer Function %u32\n"

		"%id            = OpVariable %uvec3ptr Input\n"
		"%zero          = OpConstant %i32 0\n"
		"%uzero         = OpConstant %u32 0\n"
		"%fzero         = OpConstant %f32 0\n"
		"%one           = OpConstant %i32 1\n"
		"%twelve        = OpConstant %u32 12\n"
                "%f32arr12_t    = OpTypeArray %f32 %twelve\n"
                "%f32arr12ptr_t = OpTypePointer Function %f32arr12_t\n"
		"%f32funcptr    = OpTypePointer Function %f32\n"
		"%main          = OpFunction %void None %voidf\n"
		"%entry         = OpLabel\n"

		"%f32arr12      = OpVariable %f32arr12ptr_t Function\n"

		"%i1            = OpVariable %u32ptr Function\n"
		"%i2            = OpVariable %u32ptr Function\n"
		"                 OpStore %i1 %uzero\n"
		"                 OpStore %i2 %uzero\n"

		"%idval         = OpLoad %uvec3 %id\n"
		"%x             = OpCompositeExtract %u32 %idval 0\n"
		"%inloc         = OpAccessChain %f32ptr %indata %zero %x\n"
		"%inval         = OpLoad %f32 %inloc\n"

		// for (uint i = 0; i < 12; ++i) c[i] = float(i) * input_data.elements[x];
		"                 OpBranch %loop1_entry\n"
		"%loop1_entry   = OpLabel\n"
		"%i1_val        = OpLoad %u32 %i1\n"
		"%cmp1_lt       = OpULessThan %bool %i1_val %twelve\n"
		"                 OpLoopMerge %loop1_merge %loop1_body DependencyInfinite\n"
		"                 OpBranchConditional %cmp1_lt %loop1_body %loop1_merge\n"
		"%loop1_body    = OpLabel\n"
		"%i1_valf32     = OpConvertUToF %f32 %i1_val\n"
		"%mulf1         = OpFMul %f32 %i1_valf32 %inval\n"
		"%outloc1       = OpAccessChain %f32funcptr %f32arr12 %i1_val\n"
		"                 OpStore %outloc1 %mulf1\n"
		"%new1_i        = OpIAdd %u32 %i1_val %one\n"
		"                 OpStore %i1 %new1_i\n"
		"                 OpBranch %loop1_entry\n"
		"%loop1_merge   = OpLabel\n"

		//   output_data.elements[x] = 0.0f;
		"%outloc        = OpAccessChain %f32ptr %outdata %zero %x\n"
		"                 OpStore %outloc %fzero\n"
		"                 OpBranch %loop2_entry\n"

		//   for (uint i = 0; i < n; ++i) output_data.elements[x] += c[i];
		"%loop2_entry   = OpLabel\n"
		"%i2_val        = OpLoad %u32 %i2\n"
		"%cmp2_lt       = OpULessThan %bool %i2_val %twelve\n"
		"                 OpLoopMerge %loop2_merge %loop2_body None\n"
		"                 OpBranchConditional %cmp2_lt %loop2_body %loop2_merge\n"
		"%loop2_body    = OpLabel\n"
		"%arr1_i2loc    = OpAccessChain %f32funcptr %f32arr12 %i2_val\n"
		"%arr1_i2val    = OpLoad %f32 %arr1_i2loc\n"
		"%outval        = OpLoad %f32 %outloc\n"
		"%addf1         = OpFAdd %f32 %outval %arr1_i2val\n"
		"                 OpStore %outloc %addf1\n"
		"%new_i2        = OpIAdd %u32 %i2_val %one\n"
		"                 OpStore %i2 %new_i2\n"
		"                 OpBranch %loop2_entry\n"
		"%loop2_merge   = OpLabel\n"

		"                 OpReturn\n"
		"                 OpFunctionEnd\n";
}

static ComputeShaderSpec getComputeShaderSpec ()
{
	de::Random			rnd				(0xABC);
	const int			numElements		= 100;
	vector<float>		inputFloats		(numElements, 0);
	vector<float>		outputFloats	(numElements, 0);
	ComputeShaderSpec	spec;

	for (size_t ndx = 0; ndx < numElements; ++ndx)
		inputFloats[ndx] = deFloatFloor(rnd.getFloat(1.0f, 100.0f));

	for (size_t ndx = 0; ndx < numElements; ++ndx)
	{
		const deUint32 n = 12;
		float c[n];
		float result = 0.0f;

		for (deUint32 i = 0; i < n; ++i)
			c[i] = float(i) * inputFloats[ndx];

		for (deUint32 i = 0; i < n; ++i)
			result += c[i];

		outputFloats[ndx] = result;
	}

	// Shader source code can be retrieved to complete definition of ComputeShaderSpec, though it is not required at this stage
	// getComputeSourceCode (spec.assembly);

	spec.inputs.push_back(BufferSp(new Float32Buffer(inputFloats)));
	spec.outputs.push_back(BufferSp(new Float32Buffer(outputFloats)));
	spec.numWorkGroups	= tcu::IVec3(numElements, 1, 1);
	spec.verifyIO		= &verifyOutput;

	return spec;
}


class SpvAsmLoopControlDependencyInfiniteInstance : public ComputeShaderSpec, public SpvAsmComputeShaderInstance
{
public:
	SpvAsmLoopControlDependencyInfiniteInstance	(Context& ctx);
};

SpvAsmLoopControlDependencyInfiniteInstance::SpvAsmLoopControlDependencyInfiniteInstance (Context& ctx)
	: ComputeShaderSpec(getComputeShaderSpec())
	, SpvAsmComputeShaderInstance(ctx, *this)
{
}

SpvAsmLoopControlDependencyInfiniteCase::SpvAsmLoopControlDependencyInfiniteCase (tcu::TestContext& testCtx, const char* name, const char* description)
	: TestCase			(testCtx, name, description)
{
}

void SpvAsmLoopControlDependencyInfiniteCase::initPrograms (SourceCollections& programCollection) const
{
	std::string comp;

	getComputeSourceCode(comp);

	programCollection.spirvAsmSources.add("compute") << SpirVAsmBuildOptions(programCollection.usedVulkanVersion, SPIRV_VERSION_1_3) << comp;
}

TestInstance* SpvAsmLoopControlDependencyInfiniteCase::createInstance (Context& context) const
{
	if (!context.contextSupports(vk::ApiVersion(0, 1, 1, 0)))
		TCU_THROW(NotSupportedError, "SPIR-V higher than 1.3 is required for this test to run");

	return new SpvAsmLoopControlDependencyInfiniteInstance(context);
}

} // SpirVAssembly
} // vkt
