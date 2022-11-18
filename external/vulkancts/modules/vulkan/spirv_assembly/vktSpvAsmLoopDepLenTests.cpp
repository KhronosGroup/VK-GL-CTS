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
 * \brief SPIR-V Loop Control for DependencyLength qualifier tests
 *//*--------------------------------------------------------------------*/

#include "vkApiVersion.hpp"

#include "vktSpvAsmLoopDepLenTests.hpp"
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
//   for (uint i = 0; i < 6; ++i)
//     c[i] = float(i) * input_data.elements[x];
//
//   for (uint i = 6; i < n; ++i)
//     c[i] = c[i - 4] + c[i - 5] + c[i - 6];
//
//   output_data.elements[x] = c[n - 1];
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
		"%one           = OpConstant %i32 1\n"

		"%four          = OpConstant %u32 4\n"
		"%five          = OpConstant %u32 5\n"
		"%six           = OpConstant %u32 6\n"
		"%elleven       = OpConstant %u32 11\n"
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
		"                 OpStore %i2 %six\n"

		"%idval         = OpLoad %uvec3 %id\n"
		"%x             = OpCompositeExtract %u32 %idval 0\n"
		"%inloc         = OpAccessChain %f32ptr %indata %zero %x\n"
		"%inval         = OpLoad %f32 %inloc\n"

		// for (uint i = 0; i < 6; ++i) c[i] = float(i) * input_data.elements[x];
		"                 OpBranch %loop1_entry\n"
		"%loop1_entry   = OpLabel\n"
		"%i1_val        = OpLoad %u32 %i1\n"
		"%cmp1_lt       = OpULessThan %bool %i1_val %six\n"
		"                 OpLoopMerge %loop1_merge %loop1_body None\n"
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

		//   for (uint i = 6; i < n; ++i) c[i] = c[i - 4] + c[i - 5] + c[i - 6];
		"                 OpBranch %loop2_entry\n"
		"%loop2_entry   = OpLabel\n"
		"%i2_val        = OpLoad %u32 %i2\n"
		"%cmp2_lt       = OpULessThan %bool %i2_val %twelve\n"
		"                 OpLoopMerge %loop2_merge %loop2_body DependencyLength 3\n"
		"                 OpBranchConditional %cmp2_lt %loop2_body %loop2_merge\n"
		"%loop2_body    = OpLabel\n"
		"%i2_m4         = OpISub %u32 %i2_val %four\n"
		"%arr1_i2m4loc  = OpAccessChain %f32funcptr %f32arr12 %i2_m4\n"
		"%arr1_i2m4val  = OpLoad %f32 %arr1_i2m4loc\n"
		"%i2_m5         = OpISub %u32 %i2_val %five\n"
		"%arr1_i2m5loc  = OpAccessChain %f32funcptr %f32arr12 %i2_m5\n"
		"%arr1_i2m5val  = OpLoad %f32 %arr1_i2m5loc\n"
		"%f32add1       = OpFAdd %f32 %arr1_i2m4val %arr1_i2m5val\n"
		"%i2_m6         = OpISub %u32 %i2_val %six\n"
		"%arr1_i2m6loc  = OpAccessChain %f32funcptr %f32arr12 %i2_m6\n"
		"%arr1_i2m6val  = OpLoad %f32 %arr1_i2m6loc\n"
		"%f32add2       = OpFAdd %f32 %f32add1 %arr1_i2m6val\n"
		"%outloc2       = OpAccessChain %f32funcptr %f32arr12 %i2_val\n"
		"                 OpStore %outloc2 %f32add2\n"
		"%new_i2        = OpIAdd %u32 %i2_val %one\n"
		"                 OpStore %i2 %new_i2\n"
		"                 OpBranch %loop2_entry\n"
		"%loop2_merge   = OpLabel\n"

		//   output_data.elements[x] = c[n - 1];
		"%arr1locq      = OpAccessChain %f32funcptr %f32arr12 %elleven\n"
		"%arr1valq      = OpLoad %f32 %arr1locq\n"
		"%outlocq       = OpAccessChain %f32ptr %outdata %zero %x\n"
		"                 OpStore %outlocq %arr1valq\n"
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
		inputFloats[ndx] = rnd.getFloat(1.0f, 100.0f);

	for (size_t ndx = 0; ndx < numElements; ++ndx)
	{
		const deUint32 n = 12;
		float c[n];

		for (deUint32 i = 0; i < 6; ++i)
			c[i] = float(i) * inputFloats[ndx];

		for (deUint32 i = 6; i < n; ++i)
			c[i] = c[i - 4] + c[i - 5] + c[i - 6];

		outputFloats[ndx] = c[n - 1];
	}

	// Shader source code can be retrieved to complete definition of ComputeShaderSpec, though it is not required at this stage
	// getComputeSourceCode (spec.assembly);

	spec.inputs.push_back(BufferSp(new Float32Buffer(inputFloats)));
	spec.outputs.push_back(BufferSp(new Float32Buffer(outputFloats)));
	spec.numWorkGroups	= tcu::IVec3(numElements, 1, 1);
	spec.verifyIO		= &verifyOutput;

	return spec;
}


class SpvAsmLoopControlDependencyLengthInstance : public ComputeShaderSpec, public SpvAsmComputeShaderInstance
{
public:
	SpvAsmLoopControlDependencyLengthInstance	(Context& ctx);
};

SpvAsmLoopControlDependencyLengthInstance::SpvAsmLoopControlDependencyLengthInstance (Context& ctx)
	: ComputeShaderSpec(getComputeShaderSpec())
	, SpvAsmComputeShaderInstance(ctx, *this)
{
}

SpvAsmLoopControlDependencyLengthCase::SpvAsmLoopControlDependencyLengthCase (tcu::TestContext& testCtx, const char* name, const char* description)
	: TestCase			(testCtx, name, description)
{
}

void SpvAsmLoopControlDependencyLengthCase::initPrograms (SourceCollections& programCollection) const
{
	std::string comp;

	getComputeSourceCode(comp);

	programCollection.spirvAsmSources.add("compute") << SpirVAsmBuildOptions(programCollection.usedVulkanVersion, SPIRV_VERSION_1_3) << comp;
}

TestInstance* SpvAsmLoopControlDependencyLengthCase::createInstance (Context& context) const
{
	if (!context.contextSupports(vk::ApiVersion(0, 1, 1, 0)))
		TCU_THROW(NotSupportedError, "SPIR-V higher than 1.3 is required for this test to run");

	return new SpvAsmLoopControlDependencyLengthInstance(context);
}

} // SpirVAssembly
} // vkt
