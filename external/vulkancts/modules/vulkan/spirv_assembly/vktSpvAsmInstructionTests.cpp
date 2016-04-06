/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2015 Google Inc.
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
 * \brief SPIR-V Assembly Tests for Instructions (special opcode/operand)
 *//*--------------------------------------------------------------------*/

#include "vktSpvAsmInstructionTests.hpp"

#include "tcuCommandLine.hpp"
#include "tcuFormatUtil.hpp"
#include "tcuRGBA.hpp"
#include "tcuStringTemplate.hpp"
#include "tcuTestLog.hpp"
#include "tcuVectorUtil.hpp"

#include "vkDefs.hpp"
#include "vkDeviceUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkPlatform.hpp"
#include "vkPrograms.hpp"
#include "vkQueryUtil.hpp"
#include "vkRef.hpp"
#include "vkRefUtil.hpp"
#include "vkStrUtil.hpp"
#include "vkTypeUtil.hpp"

#include "deRandom.hpp"
#include "deStringUtil.hpp"
#include "deUniquePtr.hpp"
#include "tcuStringTemplate.hpp"

#include <cmath>
#include "vktSpvAsmComputeShaderCase.hpp"
#include "vktSpvAsmComputeShaderTestUtil.hpp"
#include "vktTestCaseUtil.hpp"

#include <cmath>
#include <limits>
#include <map>
#include <string>
#include <sstream>

namespace vkt
{
namespace SpirVAssembly
{

namespace
{

using namespace vk;
using std::map;
using std::string;
using std::vector;
using tcu::IVec3;
using tcu::IVec4;
using tcu::RGBA;
using tcu::TestLog;
using tcu::TestStatus;
using tcu::Vec4;
using de::UniquePtr;
using tcu::StringTemplate;
using tcu::Vec4;

typedef Unique<VkShaderModule>			ModuleHandleUp;
typedef de::SharedPtr<ModuleHandleUp>	ModuleHandleSp;

template<typename T>	T			randomScalar	(de::Random& rnd, T minValue, T maxValue);
template<> inline		float		randomScalar	(de::Random& rnd, float minValue, float maxValue)		{ return rnd.getFloat(minValue, maxValue);	}
template<> inline		deInt32		randomScalar	(de::Random& rnd, deInt32 minValue, deInt32 maxValue)	{ return rnd.getInt(minValue, maxValue);	}

template<typename T>
static void fillRandomScalars (de::Random& rnd, T minValue, T maxValue, void* dst, int numValues, int offset = 0)
{
	T* const typedPtr = (T*)dst;
	for (int ndx = 0; ndx < numValues; ndx++)
		typedPtr[offset + ndx] = randomScalar<T>(rnd, minValue, maxValue);
}

static void floorAll (vector<float>& values)
{
	for (size_t i = 0; i < values.size(); i++)
		values[i] = deFloatFloor(values[i]);
}

static void floorAll (vector<Vec4>& values)
{
	for (size_t i = 0; i < values.size(); i++)
		values[i] = floor(values[i]);
}

struct CaseParameter
{
	const char*		name;
	string			param;

	CaseParameter	(const char* case_, const string& param_) : name(case_), param(param_) {}
};

// Assembly code used for testing OpNop, OpConstant{Null|Composite}, Op[No]Line, OpSource[Continued], OpSourceExtension, OpUndef is based on GLSL source code:
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

static const char* const s_ShaderPreamble =
	"OpCapability Shader\n"
	"OpMemoryModel Logical GLSL450\n"
	"OpEntryPoint GLCompute %main \"main\" %id\n"
	"OpExecutionMode %main LocalSize 1 1 1\n";

static const char* const s_CommonTypes =
	"%bool      = OpTypeBool\n"
	"%void      = OpTypeVoid\n"
	"%voidf     = OpTypeFunction %void\n"
	"%u32       = OpTypeInt 32 0\n"
	"%i32       = OpTypeInt 32 1\n"
	"%f32       = OpTypeFloat 32\n"
	"%uvec3     = OpTypeVector %u32 3\n"
	"%fvec3     = OpTypeVector %f32 3\n"
	"%uvec3ptr  = OpTypePointer Input %uvec3\n"
	"%f32ptr    = OpTypePointer Uniform %f32\n"
	"%f32arr    = OpTypeRuntimeArray %f32\n";

// Declares two uniform variables (indata, outdata) of type "struct { float[] }". Depends on type "f32arr" (for "float[]").
static const char* const s_InputOutputBuffer =
	"%buf     = OpTypeStruct %f32arr\n"
	"%bufptr  = OpTypePointer Uniform %buf\n"
	"%indata    = OpVariable %bufptr Uniform\n"
	"%outdata   = OpVariable %bufptr Uniform\n";

// Declares buffer type and layout for uniform variables indata and outdata. Both of them are SSBO bounded to descriptor set 0.
// indata is at binding point 0, while outdata is at 1.
static const char* const s_InputOutputBufferTraits =
	"OpDecorate %buf BufferBlock\n"
	"OpDecorate %indata DescriptorSet 0\n"
	"OpDecorate %indata Binding 0\n"
	"OpDecorate %outdata DescriptorSet 0\n"
	"OpDecorate %outdata Binding 1\n"
	"OpDecorate %f32arr ArrayStride 4\n"
	"OpMemberDecorate %buf 0 Offset 0\n";

tcu::TestCaseGroup* createOpNopGroup (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup>	group			(new tcu::TestCaseGroup(testCtx, "opnop", "Test the OpNop instruction"));
	ComputeShaderSpec				spec;
	de::Random						rnd				(deStringHash(group->getName()));
	const int						numElements		= 100;
	vector<float>					positiveFloats	(numElements, 0);
	vector<float>					negativeFloats	(numElements, 0);

	fillRandomScalars(rnd, 1.f, 100.f, &positiveFloats[0], numElements);

	for (size_t ndx = 0; ndx < numElements; ++ndx)
		negativeFloats[ndx] = -positiveFloats[ndx];

	spec.assembly =
		string(s_ShaderPreamble) +

		"OpSource GLSL 430\n"
		"OpName %main           \"main\"\n"
		"OpName %id             \"gl_GlobalInvocationID\"\n"

		"OpDecorate %id BuiltIn GlobalInvocationId\n"

		+ string(s_InputOutputBufferTraits) + string(s_CommonTypes)

		+ string(s_InputOutputBuffer) +

		"%id        = OpVariable %uvec3ptr Input\n"
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
	spec.inputs.push_back(BufferSp(new Float32Buffer(positiveFloats)));
	spec.outputs.push_back(BufferSp(new Float32Buffer(negativeFloats)));
	spec.numWorkGroups = IVec3(numElements, 1, 1);

	group->addChild(new SpvAsmComputeShaderCase(testCtx, "all", "OpNop appearing at different places", spec));

	return group.release();
}

tcu::TestCaseGroup* createOpLineGroup (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup>	group			(new tcu::TestCaseGroup(testCtx, "opline", "Test the OpLine instruction"));
	ComputeShaderSpec				spec;
	de::Random						rnd				(deStringHash(group->getName()));
	const int						numElements		= 100;
	vector<float>					positiveFloats	(numElements, 0);
	vector<float>					negativeFloats	(numElements, 0);

	fillRandomScalars(rnd, 1.f, 100.f, &positiveFloats[0], numElements);

	for (size_t ndx = 0; ndx < numElements; ++ndx)
		negativeFloats[ndx] = -positiveFloats[ndx];

	spec.assembly =
		string(s_ShaderPreamble) +

		"%fname1 = OpString \"negateInputs.comp\"\n"
		"%fname2 = OpString \"negateInputs\"\n"

		"OpSource GLSL 430\n"
		"OpName %main           \"main\"\n"
		"OpName %id             \"gl_GlobalInvocationID\"\n"

		"OpDecorate %id BuiltIn GlobalInvocationId\n"

		+ string(s_InputOutputBufferTraits) +

		"OpLine %fname1 0 0\n" // At the earliest possible position

		+ string(s_CommonTypes) + string(s_InputOutputBuffer) +

		"OpLine %fname1 0 1\n" // Multiple OpLines in sequence
		"OpLine %fname2 1 0\n" // Different filenames
		"OpLine %fname1 1000 100000\n"

		"%id        = OpVariable %uvec3ptr Input\n"
		"%zero      = OpConstant %i32 0\n"

		"OpLine %fname1 1 1\n" // Before a function

		"%main      = OpFunction %void None %voidf\n"
		"%label     = OpLabel\n"

		"OpLine %fname1 1 1\n" // In a function

		"%idval     = OpLoad %uvec3 %id\n"
		"%x         = OpCompositeExtract %u32 %idval 0\n"
		"%inloc     = OpAccessChain %f32ptr %indata %zero %x\n"
		"%inval     = OpLoad %f32 %inloc\n"
		"%neg       = OpFNegate %f32 %inval\n"
		"%outloc    = OpAccessChain %f32ptr %outdata %zero %x\n"
		"             OpStore %outloc %neg\n"
		"             OpReturn\n"
		"             OpFunctionEnd\n";
	spec.inputs.push_back(BufferSp(new Float32Buffer(positiveFloats)));
	spec.outputs.push_back(BufferSp(new Float32Buffer(negativeFloats)));
	spec.numWorkGroups = IVec3(numElements, 1, 1);

	group->addChild(new SpvAsmComputeShaderCase(testCtx, "all", "OpLine appearing at different places", spec));

	return group.release();
}

tcu::TestCaseGroup* createOpNoLineGroup (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup>	group			(new tcu::TestCaseGroup(testCtx, "opnoline", "Test the OpNoLine instruction"));
	ComputeShaderSpec				spec;
	de::Random						rnd				(deStringHash(group->getName()));
	const int						numElements		= 100;
	vector<float>					positiveFloats	(numElements, 0);
	vector<float>					negativeFloats	(numElements, 0);

	fillRandomScalars(rnd, 1.f, 100.f, &positiveFloats[0], numElements);

	for (size_t ndx = 0; ndx < numElements; ++ndx)
		negativeFloats[ndx] = -positiveFloats[ndx];

	spec.assembly =
		string(s_ShaderPreamble) +

		"%fname = OpString \"negateInputs.comp\"\n"

		"OpSource GLSL 430\n"
		"OpName %main           \"main\"\n"
		"OpName %id             \"gl_GlobalInvocationID\"\n"

		"OpDecorate %id BuiltIn GlobalInvocationId\n"

		+ string(s_InputOutputBufferTraits) +

		"OpNoLine\n" // At the earliest possible position, without preceding OpLine

		+ string(s_CommonTypes) + string(s_InputOutputBuffer) +

		"OpLine %fname 0 1\n"
		"OpNoLine\n" // Immediately following a preceding OpLine

		"OpLine %fname 1000 1\n"

		"%id        = OpVariable %uvec3ptr Input\n"
		"%zero      = OpConstant %i32 0\n"

		"OpNoLine\n" // Contents after the previous OpLine

		"%main      = OpFunction %void None %voidf\n"
		"%label     = OpLabel\n"
		"%idval     = OpLoad %uvec3 %id\n"
		"%x         = OpCompositeExtract %u32 %idval 0\n"

		"OpNoLine\n" // Multiple OpNoLine
		"OpNoLine\n"
		"OpNoLine\n"

		"%inloc     = OpAccessChain %f32ptr %indata %zero %x\n"
		"%inval     = OpLoad %f32 %inloc\n"
		"%neg       = OpFNegate %f32 %inval\n"
		"%outloc    = OpAccessChain %f32ptr %outdata %zero %x\n"
		"             OpStore %outloc %neg\n"
		"             OpReturn\n"
		"             OpFunctionEnd\n";
	spec.inputs.push_back(BufferSp(new Float32Buffer(positiveFloats)));
	spec.outputs.push_back(BufferSp(new Float32Buffer(negativeFloats)));
	spec.numWorkGroups = IVec3(numElements, 1, 1);

	group->addChild(new SpvAsmComputeShaderCase(testCtx, "all", "OpNoLine appearing at different places", spec));

	return group.release();
}

// Compare instruction for the contraction compute case.
// Returns true if the output is what is expected from the test case.
bool compareNoContractCase(const std::vector<BufferSp>&, const vector<AllocationSp>& outputAllocs, const std::vector<BufferSp>& expectedOutputs)
{
	if (outputAllocs.size() != 1)
		return false;

	// We really just need this for size because we are not comparing the exact values.
	const BufferSp&	expectedOutput	= expectedOutputs[0];
	const float*	outputAsFloat	= static_cast<const float*>(outputAllocs[0]->getHostPtr());;

	for(size_t i = 0; i < expectedOutput->getNumBytes() / sizeof(float); ++i) {
		if (outputAsFloat[i] != 0.f &&
			outputAsFloat[i] != -ldexp(1, -24)) {
			return false;
		}
	}

	return true;
}

tcu::TestCaseGroup* createNoContractionGroup (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup>	group			(new tcu::TestCaseGroup(testCtx, "nocontraction", "Test the NoContraction decoration"));
	vector<CaseParameter>			cases;
	const int						numElements		= 100;
	vector<float>					inputFloats1	(numElements, 0);
	vector<float>					inputFloats2	(numElements, 0);
	vector<float>					outputFloats	(numElements, 0);
	const StringTemplate			shaderTemplate	(
		string(s_ShaderPreamble) +

		"OpName %main           \"main\"\n"
		"OpName %id             \"gl_GlobalInvocationID\"\n"

		"OpDecorate %id BuiltIn GlobalInvocationId\n"

		"${DECORATION}\n"

		"OpDecorate %buf BufferBlock\n"
		"OpDecorate %indata1 DescriptorSet 0\n"
		"OpDecorate %indata1 Binding 0\n"
		"OpDecorate %indata2 DescriptorSet 0\n"
		"OpDecorate %indata2 Binding 1\n"
		"OpDecorate %outdata DescriptorSet 0\n"
		"OpDecorate %outdata Binding 2\n"
		"OpDecorate %f32arr ArrayStride 4\n"
		"OpMemberDecorate %buf 0 Offset 0\n"

		+ string(s_CommonTypes) +

		"%buf        = OpTypeStruct %f32arr\n"
		"%bufptr     = OpTypePointer Uniform %buf\n"
		"%indata1    = OpVariable %bufptr Uniform\n"
		"%indata2    = OpVariable %bufptr Uniform\n"
		"%outdata    = OpVariable %bufptr Uniform\n"

		"%id         = OpVariable %uvec3ptr Input\n"
		"%zero       = OpConstant %i32 0\n"
		"%c_f_m1     = OpConstant %f32 -1.\n"

		"%main       = OpFunction %void None %voidf\n"
		"%label      = OpLabel\n"
		"%idval      = OpLoad %uvec3 %id\n"
		"%x          = OpCompositeExtract %u32 %idval 0\n"
		"%inloc1     = OpAccessChain %f32ptr %indata1 %zero %x\n"
		"%inval1     = OpLoad %f32 %inloc1\n"
		"%inloc2     = OpAccessChain %f32ptr %indata2 %zero %x\n"
		"%inval2     = OpLoad %f32 %inloc2\n"
		"%mul        = OpFMul %f32 %inval1 %inval2\n"
		"%add        = OpFAdd %f32 %mul %c_f_m1\n"
		"%outloc     = OpAccessChain %f32ptr %outdata %zero %x\n"
		"              OpStore %outloc %add\n"
		"              OpReturn\n"
		"              OpFunctionEnd\n");

	cases.push_back(CaseParameter("multiplication",	"OpDecorate %mul NoContraction"));
	cases.push_back(CaseParameter("addition",		"OpDecorate %add NoContraction"));
	cases.push_back(CaseParameter("both",			"OpDecorate %mul NoContraction\nOpDecorate %add NoContraction"));

	for (size_t ndx = 0; ndx < numElements; ++ndx)
	{
		inputFloats1[ndx]	= 1.f + std::ldexp(1.f, -23); // 1 + 2^-23.
		inputFloats2[ndx]	= 1.f - std::ldexp(1.f, -23); // 1 - 2^-23.
		// Result for (1 + 2^-23) * (1 - 2^-23) - 1. With NoContraction, the multiplication will be
		// conducted separately and the result is rounded to 1, or 0x1.fffffcp-1
		// So the final result will be 0.f or 0x1p-24.
		// If the operation is combined into a precise fused multiply-add, then the result would be
		// 2^-46 (0xa8800000).
		outputFloats[ndx]	= 0.f;
	}

	for (size_t caseNdx = 0; caseNdx < cases.size(); ++caseNdx)
	{
		map<string, string>		specializations;
		ComputeShaderSpec		spec;

		specializations["DECORATION"] = cases[caseNdx].param;
		spec.assembly = shaderTemplate.specialize(specializations);
		spec.inputs.push_back(BufferSp(new Float32Buffer(inputFloats1)));
		spec.inputs.push_back(BufferSp(new Float32Buffer(inputFloats2)));
		spec.outputs.push_back(BufferSp(new Float32Buffer(outputFloats)));
		spec.numWorkGroups = IVec3(numElements, 1, 1);
		// Check against the two possible answers based on rounding mode.
		spec.verifyIO = &compareNoContractCase;

		group->addChild(new SpvAsmComputeShaderCase(testCtx, cases[caseNdx].name, cases[caseNdx].name, spec));
	}
	return group.release();
}

bool compareFRem(const std::vector<BufferSp>&, const vector<AllocationSp>& outputAllocs, const std::vector<BufferSp>& expectedOutputs)
{
	if (outputAllocs.size() != 1)
		return false;

	const BufferSp& expectedOutput = expectedOutputs[0];
	const float *expectedOutputAsFloat = static_cast<const float*>(expectedOutput->data());
	const float* outputAsFloat = static_cast<const float*>(outputAllocs[0]->getHostPtr());;

	for (size_t idx = 0; idx < expectedOutput->getNumBytes() / sizeof(float); ++idx)
	{
		const float f0 = expectedOutputAsFloat[idx];
		const float f1 = outputAsFloat[idx];
		// \todo relative error needs to be fairly high because FRem may be implemented as
		// (roughly) frac(a/b)*b, so LSB errors can be magnified. But this should be fine for now.
		if (deFloatAbs((f1 - f0) / f0) > 0.02)
			return false;
	}

	return true;
}

tcu::TestCaseGroup* createOpFRemGroup (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup>	group			(new tcu::TestCaseGroup(testCtx, "opfrem", "Test the OpFRem instruction"));
	ComputeShaderSpec				spec;
	de::Random						rnd				(deStringHash(group->getName()));
	const int						numElements		= 200;
	vector<float>					inputFloats1	(numElements, 0);
	vector<float>					inputFloats2	(numElements, 0);
	vector<float>					outputFloats	(numElements, 0);

	fillRandomScalars(rnd, -10000.f, 10000.f, &inputFloats1[0], numElements);
	fillRandomScalars(rnd, -100.f, 100.f, &inputFloats2[0], numElements);

	for (size_t ndx = 0; ndx < numElements; ++ndx)
	{
		// Guard against divisors near zero.
		if (std::fabs(inputFloats2[ndx]) < 1e-3)
			inputFloats2[ndx] = 8.f;

		// The return value of std::fmod() has the same sign as its first operand, which is how OpFRem spec'd.
		outputFloats[ndx] = std::fmod(inputFloats1[ndx], inputFloats2[ndx]);
	}

	spec.assembly =
		string(s_ShaderPreamble) +

		"OpName %main           \"main\"\n"
		"OpName %id             \"gl_GlobalInvocationID\"\n"

		"OpDecorate %id BuiltIn GlobalInvocationId\n"

		"OpDecorate %buf BufferBlock\n"
		"OpDecorate %indata1 DescriptorSet 0\n"
		"OpDecorate %indata1 Binding 0\n"
		"OpDecorate %indata2 DescriptorSet 0\n"
		"OpDecorate %indata2 Binding 1\n"
		"OpDecorate %outdata DescriptorSet 0\n"
		"OpDecorate %outdata Binding 2\n"
		"OpDecorate %f32arr ArrayStride 4\n"
		"OpMemberDecorate %buf 0 Offset 0\n"

		+ string(s_CommonTypes) +

		"%buf        = OpTypeStruct %f32arr\n"
		"%bufptr     = OpTypePointer Uniform %buf\n"
		"%indata1    = OpVariable %bufptr Uniform\n"
		"%indata2    = OpVariable %bufptr Uniform\n"
		"%outdata    = OpVariable %bufptr Uniform\n"

		"%id        = OpVariable %uvec3ptr Input\n"
		"%zero      = OpConstant %i32 0\n"

		"%main      = OpFunction %void None %voidf\n"
		"%label     = OpLabel\n"
		"%idval     = OpLoad %uvec3 %id\n"
		"%x         = OpCompositeExtract %u32 %idval 0\n"
		"%inloc1    = OpAccessChain %f32ptr %indata1 %zero %x\n"
		"%inval1    = OpLoad %f32 %inloc1\n"
		"%inloc2    = OpAccessChain %f32ptr %indata2 %zero %x\n"
		"%inval2    = OpLoad %f32 %inloc2\n"
		"%rem       = OpFRem %f32 %inval1 %inval2\n"
		"%outloc    = OpAccessChain %f32ptr %outdata %zero %x\n"
		"             OpStore %outloc %rem\n"
		"             OpReturn\n"
		"             OpFunctionEnd\n";

	spec.inputs.push_back(BufferSp(new Float32Buffer(inputFloats1)));
	spec.inputs.push_back(BufferSp(new Float32Buffer(inputFloats2)));
	spec.outputs.push_back(BufferSp(new Float32Buffer(outputFloats)));
	spec.numWorkGroups = IVec3(numElements, 1, 1);
	spec.verifyIO = &compareFRem;

	group->addChild(new SpvAsmComputeShaderCase(testCtx, "all", "", spec));

	return group.release();
}

// Copy contents in the input buffer to the output buffer.
tcu::TestCaseGroup* createOpCopyMemoryGroup (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup>	group			(new tcu::TestCaseGroup(testCtx, "opcopymemory", "Test the OpCopyMemory instruction"));
	de::Random						rnd				(deStringHash(group->getName()));
	const int						numElements		= 100;

	// The following case adds vec4(0., 0.5, 1.5, 2.5) to each of the elements in the input buffer and writes output to the output buffer.
	ComputeShaderSpec				spec1;
	vector<Vec4>					inputFloats1	(numElements);
	vector<Vec4>					outputFloats1	(numElements);

	fillRandomScalars(rnd, -200.f, 200.f, &inputFloats1[0], numElements * 4);

	// CPU might not use the same rounding mode as the GPU. Use whole numbers to avoid rounding differences.
	floorAll(inputFloats1);

	for (size_t ndx = 0; ndx < numElements; ++ndx)
		outputFloats1[ndx] = inputFloats1[ndx] + Vec4(0.f, 0.5f, 1.5f, 2.5f);

	spec1.assembly =
		string(s_ShaderPreamble) +

		"OpName %main           \"main\"\n"
		"OpName %id             \"gl_GlobalInvocationID\"\n"

		"OpDecorate %id BuiltIn GlobalInvocationId\n"
		"OpDecorate %vec4arr ArrayStride 16\n"

		+ string(s_InputOutputBufferTraits) + string(s_CommonTypes) +

		"%vec4       = OpTypeVector %f32 4\n"
		"%vec4ptr_u  = OpTypePointer Uniform %vec4\n"
		"%vec4ptr_f  = OpTypePointer Function %vec4\n"
		"%vec4arr    = OpTypeRuntimeArray %vec4\n"
		"%buf        = OpTypeStruct %vec4arr\n"
		"%bufptr     = OpTypePointer Uniform %buf\n"
		"%indata     = OpVariable %bufptr Uniform\n"
		"%outdata    = OpVariable %bufptr Uniform\n"

		"%id         = OpVariable %uvec3ptr Input\n"
		"%zero       = OpConstant %i32 0\n"
		"%c_f_0      = OpConstant %f32 0.\n"
		"%c_f_0_5    = OpConstant %f32 0.5\n"
		"%c_f_1_5    = OpConstant %f32 1.5\n"
		"%c_f_2_5    = OpConstant %f32 2.5\n"
		"%c_vec4     = OpConstantComposite %vec4 %c_f_0 %c_f_0_5 %c_f_1_5 %c_f_2_5\n"

		"%main       = OpFunction %void None %voidf\n"
		"%label      = OpLabel\n"
		"%v_vec4     = OpVariable %vec4ptr_f Function\n"
		"%idval      = OpLoad %uvec3 %id\n"
		"%x          = OpCompositeExtract %u32 %idval 0\n"
		"%inloc      = OpAccessChain %vec4ptr_u %indata %zero %x\n"
		"%outloc     = OpAccessChain %vec4ptr_u %outdata %zero %x\n"
		"              OpCopyMemory %v_vec4 %inloc\n"
		"%v_vec4_val = OpLoad %vec4 %v_vec4\n"
		"%add        = OpFAdd %vec4 %v_vec4_val %c_vec4\n"
		"              OpStore %outloc %add\n"
		"              OpReturn\n"
		"              OpFunctionEnd\n";

	spec1.inputs.push_back(BufferSp(new Vec4Buffer(inputFloats1)));
	spec1.outputs.push_back(BufferSp(new Vec4Buffer(outputFloats1)));
	spec1.numWorkGroups = IVec3(numElements, 1, 1);

	group->addChild(new SpvAsmComputeShaderCase(testCtx, "vector", "OpCopyMemory elements of vector type", spec1));

	// The following case copies a float[100] variable from the input buffer to the output buffer.
	ComputeShaderSpec				spec2;
	vector<float>					inputFloats2	(numElements);
	vector<float>					outputFloats2	(numElements);

	fillRandomScalars(rnd, -200.f, 200.f, &inputFloats2[0], numElements);

	for (size_t ndx = 0; ndx < numElements; ++ndx)
		outputFloats2[ndx] = inputFloats2[ndx];

	spec2.assembly =
		string(s_ShaderPreamble) +

		"OpName %main           \"main\"\n"
		"OpName %id             \"gl_GlobalInvocationID\"\n"

		"OpDecorate %id BuiltIn GlobalInvocationId\n"
		"OpDecorate %f32arr100 ArrayStride 4\n"

		+ string(s_InputOutputBufferTraits) + string(s_CommonTypes) +

		"%hundred        = OpConstant %u32 100\n"
		"%f32arr100      = OpTypeArray %f32 %hundred\n"
		"%f32arr100ptr_f = OpTypePointer Function %f32arr100\n"
		"%f32arr100ptr_u = OpTypePointer Uniform %f32arr100\n"
		"%buf            = OpTypeStruct %f32arr100\n"
		"%bufptr         = OpTypePointer Uniform %buf\n"
		"%indata         = OpVariable %bufptr Uniform\n"
		"%outdata        = OpVariable %bufptr Uniform\n"

		"%id             = OpVariable %uvec3ptr Input\n"
		"%zero           = OpConstant %i32 0\n"

		"%main           = OpFunction %void None %voidf\n"
		"%label          = OpLabel\n"
		"%var            = OpVariable %f32arr100ptr_f Function\n"
		"%inarr          = OpAccessChain %f32arr100ptr_u %indata %zero\n"
		"%outarr         = OpAccessChain %f32arr100ptr_u %outdata %zero\n"
		"                  OpCopyMemory %var %inarr\n"
		"                  OpCopyMemory %outarr %var\n"
		"                  OpReturn\n"
		"                  OpFunctionEnd\n";

	spec2.inputs.push_back(BufferSp(new Float32Buffer(inputFloats2)));
	spec2.outputs.push_back(BufferSp(new Float32Buffer(outputFloats2)));
	spec2.numWorkGroups = IVec3(1, 1, 1);

	group->addChild(new SpvAsmComputeShaderCase(testCtx, "array", "OpCopyMemory elements of array type", spec2));

	// The following case copies a struct{vec4, vec4, vec4, vec4} variable from the input buffer to the output buffer.
	ComputeShaderSpec				spec3;
	vector<float>					inputFloats3	(16);
	vector<float>					outputFloats3	(16);

	fillRandomScalars(rnd, -200.f, 200.f, &inputFloats3[0], 16);

	for (size_t ndx = 0; ndx < 16; ++ndx)
		outputFloats3[ndx] = inputFloats3[ndx];

	spec3.assembly =
		string(s_ShaderPreamble) +

		"OpName %main           \"main\"\n"
		"OpName %id             \"gl_GlobalInvocationID\"\n"

		"OpDecorate %id BuiltIn GlobalInvocationId\n"
		"OpMemberDecorate %buf 0 Offset 0\n"
		"OpMemberDecorate %buf 1 Offset 16\n"
		"OpMemberDecorate %buf 2 Offset 32\n"
		"OpMemberDecorate %buf 3 Offset 48\n"

		+ string(s_InputOutputBufferTraits) + string(s_CommonTypes) +

		"%vec4      = OpTypeVector %f32 4\n"
		"%buf       = OpTypeStruct %vec4 %vec4 %vec4 %vec4\n"
		"%bufptr    = OpTypePointer Uniform %buf\n"
		"%indata    = OpVariable %bufptr Uniform\n"
		"%outdata   = OpVariable %bufptr Uniform\n"
		"%vec4stptr = OpTypePointer Function %buf\n"

		"%id        = OpVariable %uvec3ptr Input\n"
		"%zero      = OpConstant %i32 0\n"

		"%main      = OpFunction %void None %voidf\n"
		"%label     = OpLabel\n"
		"%var       = OpVariable %vec4stptr Function\n"
		"             OpCopyMemory %var %indata\n"
		"             OpCopyMemory %outdata %var\n"
		"             OpReturn\n"
		"             OpFunctionEnd\n";

	spec3.inputs.push_back(BufferSp(new Float32Buffer(inputFloats3)));
	spec3.outputs.push_back(BufferSp(new Float32Buffer(outputFloats3)));
	spec3.numWorkGroups = IVec3(1, 1, 1);

	group->addChild(new SpvAsmComputeShaderCase(testCtx, "struct", "OpCopyMemory elements of struct type", spec3));

	// The following case negates multiple float variables from the input buffer and stores the results to the output buffer.
	ComputeShaderSpec				spec4;
	vector<float>					inputFloats4	(numElements);
	vector<float>					outputFloats4	(numElements);

	fillRandomScalars(rnd, -200.f, 200.f, &inputFloats4[0], numElements);

	for (size_t ndx = 0; ndx < numElements; ++ndx)
		outputFloats4[ndx] = -inputFloats4[ndx];

	spec4.assembly =
		string(s_ShaderPreamble) +

		"OpName %main           \"main\"\n"
		"OpName %id             \"gl_GlobalInvocationID\"\n"

		"OpDecorate %id BuiltIn GlobalInvocationId\n"

		+ string(s_InputOutputBufferTraits) + string(s_CommonTypes) + string(s_InputOutputBuffer) +

		"%f32ptr_f  = OpTypePointer Function %f32\n"
		"%id        = OpVariable %uvec3ptr Input\n"
		"%zero      = OpConstant %i32 0\n"

		"%main      = OpFunction %void None %voidf\n"
		"%label     = OpLabel\n"
		"%var       = OpVariable %f32ptr_f Function\n"
		"%idval     = OpLoad %uvec3 %id\n"
		"%x         = OpCompositeExtract %u32 %idval 0\n"
		"%inloc     = OpAccessChain %f32ptr %indata %zero %x\n"
		"%outloc    = OpAccessChain %f32ptr %outdata %zero %x\n"
		"             OpCopyMemory %var %inloc\n"
		"%val       = OpLoad %f32 %var\n"
		"%neg       = OpFNegate %f32 %val\n"
		"             OpStore %outloc %neg\n"
		"             OpReturn\n"
		"             OpFunctionEnd\n";

	spec4.inputs.push_back(BufferSp(new Float32Buffer(inputFloats4)));
	spec4.outputs.push_back(BufferSp(new Float32Buffer(outputFloats4)));
	spec4.numWorkGroups = IVec3(numElements, 1, 1);

	group->addChild(new SpvAsmComputeShaderCase(testCtx, "float", "OpCopyMemory elements of float type", spec4));

	return group.release();
}

tcu::TestCaseGroup* createOpCopyObjectGroup (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup>	group			(new tcu::TestCaseGroup(testCtx, "opcopyobject", "Test the OpCopyObject instruction"));
	ComputeShaderSpec				spec;
	de::Random						rnd				(deStringHash(group->getName()));
	const int						numElements		= 100;
	vector<float>					inputFloats		(numElements, 0);
	vector<float>					outputFloats	(numElements, 0);

	fillRandomScalars(rnd, -200.f, 200.f, &inputFloats[0], numElements);

	// CPU might not use the same rounding mode as the GPU. Use whole numbers to avoid rounding differences.
	floorAll(inputFloats);

	for (size_t ndx = 0; ndx < numElements; ++ndx)
		outputFloats[ndx] = inputFloats[ndx] + 7.5f;

	spec.assembly =
		string(s_ShaderPreamble) +

		"OpName %main           \"main\"\n"
		"OpName %id             \"gl_GlobalInvocationID\"\n"

		"OpDecorate %id BuiltIn GlobalInvocationId\n"

		+ string(s_InputOutputBufferTraits) + string(s_CommonTypes) +

		"%fmat     = OpTypeMatrix %fvec3 3\n"
		"%three    = OpConstant %u32 3\n"
		"%farr     = OpTypeArray %f32 %three\n"
		"%fst      = OpTypeStruct %f32 %f32\n"

		+ string(s_InputOutputBuffer) +

		"%id            = OpVariable %uvec3ptr Input\n"
		"%zero          = OpConstant %i32 0\n"
		"%c_f           = OpConstant %f32 1.5\n"
		"%c_fvec3       = OpConstantComposite %fvec3 %c_f %c_f %c_f\n"
		"%c_fmat        = OpConstantComposite %fmat %c_fvec3 %c_fvec3 %c_fvec3\n"
		"%c_farr        = OpConstantComposite %farr %c_f %c_f %c_f\n"
		"%c_fst         = OpConstantComposite %fst %c_f %c_f\n"

		"%main          = OpFunction %void None %voidf\n"
		"%label         = OpLabel\n"
		"%c_f_copy      = OpCopyObject %f32   %c_f\n"
		"%c_fvec3_copy  = OpCopyObject %fvec3 %c_fvec3\n"
		"%c_fmat_copy   = OpCopyObject %fmat  %c_fmat\n"
		"%c_farr_copy   = OpCopyObject %farr  %c_farr\n"
		"%c_fst_copy    = OpCopyObject %fst   %c_fst\n"
		"%fvec3_elem    = OpCompositeExtract %f32 %c_fvec3_copy 0\n"
		"%fmat_elem     = OpCompositeExtract %f32 %c_fmat_copy 1 2\n"
		"%farr_elem     = OpCompositeExtract %f32 %c_farr_copy 2\n"
		"%fst_elem      = OpCompositeExtract %f32 %c_fst_copy 1\n"
		// Add up. 1.5 * 5 = 7.5.
		"%add1          = OpFAdd %f32 %c_f_copy %fvec3_elem\n"
		"%add2          = OpFAdd %f32 %add1     %fmat_elem\n"
		"%add3          = OpFAdd %f32 %add2     %farr_elem\n"
		"%add4          = OpFAdd %f32 %add3     %fst_elem\n"

		"%idval         = OpLoad %uvec3 %id\n"
		"%x             = OpCompositeExtract %u32 %idval 0\n"
		"%inloc         = OpAccessChain %f32ptr %indata %zero %x\n"
		"%outloc        = OpAccessChain %f32ptr %outdata %zero %x\n"
		"%inval         = OpLoad %f32 %inloc\n"
		"%add           = OpFAdd %f32 %add4 %inval\n"
		"                 OpStore %outloc %add\n"
		"                 OpReturn\n"
		"                 OpFunctionEnd\n";
	spec.inputs.push_back(BufferSp(new Float32Buffer(inputFloats)));
	spec.outputs.push_back(BufferSp(new Float32Buffer(outputFloats)));
	spec.numWorkGroups = IVec3(numElements, 1, 1);

	group->addChild(new SpvAsmComputeShaderCase(testCtx, "spotcheck", "OpCopyObject on different types", spec));

	return group.release();
}
// Assembly code used for testing OpUnreachable is based on GLSL source code:
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
// void not_called_func() {
//   // place OpUnreachable here
// }
//
// uint modulo4(uint val) {
//   switch (val % uint(4)) {
//     case 0:  return 3;
//     case 1:  return 2;
//     case 2:  return 1;
//     case 3:  return 0;
//     default: return 100; // place OpUnreachable here
//   }
// }
//
// uint const5() {
//   return 5;
//   // place OpUnreachable here
// }
//
// void main() {
//   uint x = gl_GlobalInvocationID.x;
//   if (const5() > modulo4(1000)) {
//     output_data.elements[x] = -input_data.elements[x];
//   } else {
//     // place OpUnreachable here
//     output_data.elements[x] = input_data.elements[x];
//   }
// }

tcu::TestCaseGroup* createOpUnreachableGroup (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup>	group			(new tcu::TestCaseGroup(testCtx, "opunreachable", "Test the OpUnreachable instruction"));
	ComputeShaderSpec				spec;
	de::Random						rnd				(deStringHash(group->getName()));
	const int						numElements		= 100;
	vector<float>					positiveFloats	(numElements, 0);
	vector<float>					negativeFloats	(numElements, 0);

	fillRandomScalars(rnd, 1.f, 100.f, &positiveFloats[0], numElements);

	for (size_t ndx = 0; ndx < numElements; ++ndx)
		negativeFloats[ndx] = -positiveFloats[ndx];

	spec.assembly =
		string(s_ShaderPreamble) +

		"OpSource GLSL 430\n"
		"OpName %main            \"main\"\n"
		"OpName %func_not_called_func \"not_called_func(\"\n"
		"OpName %func_modulo4         \"modulo4(u1;\"\n"
		"OpName %func_const5          \"const5(\"\n"
		"OpName %id                   \"gl_GlobalInvocationID\"\n"

		"OpDecorate %id BuiltIn GlobalInvocationId\n"

		+ string(s_InputOutputBufferTraits) + string(s_CommonTypes) +

		"%u32ptr    = OpTypePointer Function %u32\n"
		"%uintfuint = OpTypeFunction %u32 %u32ptr\n"
		"%unitf     = OpTypeFunction %u32\n"

		"%id        = OpVariable %uvec3ptr Input\n"
		"%zero      = OpConstant %u32 0\n"
		"%one       = OpConstant %u32 1\n"
		"%two       = OpConstant %u32 2\n"
		"%three     = OpConstant %u32 3\n"
		"%four      = OpConstant %u32 4\n"
		"%five      = OpConstant %u32 5\n"
		"%hundred   = OpConstant %u32 100\n"
		"%thousand  = OpConstant %u32 1000\n"

		+ string(s_InputOutputBuffer) +

		// Main()
		"%main   = OpFunction %void None %voidf\n"
		"%main_entry  = OpLabel\n"
		"%v_thousand  = OpVariable %u32ptr Function %thousand\n"
		"%idval       = OpLoad %uvec3 %id\n"
		"%x           = OpCompositeExtract %u32 %idval 0\n"
		"%inloc       = OpAccessChain %f32ptr %indata %zero %x\n"
		"%inval       = OpLoad %f32 %inloc\n"
		"%outloc      = OpAccessChain %f32ptr %outdata %zero %x\n"
		"%ret_const5  = OpFunctionCall %u32 %func_const5\n"
		"%ret_modulo4 = OpFunctionCall %u32 %func_modulo4 %v_thousand\n"
		"%cmp_gt      = OpUGreaterThan %bool %ret_const5 %ret_modulo4\n"
		"               OpSelectionMerge %if_end None\n"
		"               OpBranchConditional %cmp_gt %if_true %if_false\n"
		"%if_true     = OpLabel\n"
		"%negate      = OpFNegate %f32 %inval\n"
		"               OpStore %outloc %negate\n"
		"               OpBranch %if_end\n"
		"%if_false    = OpLabel\n"
		"               OpUnreachable\n" // Unreachable else branch for if statement
		"%if_end      = OpLabel\n"
		"               OpReturn\n"
		"               OpFunctionEnd\n"

		// not_called_function()
		"%func_not_called_func  = OpFunction %void None %voidf\n"
		"%not_called_func_entry = OpLabel\n"
		"                         OpUnreachable\n" // Unreachable entry block in not called static function
		"                         OpFunctionEnd\n"

		// modulo4()
		"%func_modulo4  = OpFunction %u32 None %uintfuint\n"
		"%valptr        = OpFunctionParameter %u32ptr\n"
		"%modulo4_entry = OpLabel\n"
		"%val           = OpLoad %u32 %valptr\n"
		"%modulo        = OpUMod %u32 %val %four\n"
		"                 OpSelectionMerge %switch_merge None\n"
		"                 OpSwitch %modulo %default 0 %case0 1 %case1 2 %case2 3 %case3\n"
		"%case0         = OpLabel\n"
		"                 OpReturnValue %three\n"
		"%case1         = OpLabel\n"
		"                 OpReturnValue %two\n"
		"%case2         = OpLabel\n"
		"                 OpReturnValue %one\n"
		"%case3         = OpLabel\n"
		"                 OpReturnValue %zero\n"
		"%default       = OpLabel\n"
		"                 OpUnreachable\n" // Unreachable default case for switch statement
		"%switch_merge  = OpLabel\n"
		"                 OpUnreachable\n" // Unreachable merge block for switch statement
		"                 OpFunctionEnd\n"

		// const5()
		"%func_const5  = OpFunction %u32 None %unitf\n"
		"%const5_entry = OpLabel\n"
		"                OpReturnValue %five\n"
		"%unreachable  = OpLabel\n"
		"                OpUnreachable\n" // Unreachable block in function
		"                OpFunctionEnd\n";
	spec.inputs.push_back(BufferSp(new Float32Buffer(positiveFloats)));
	spec.outputs.push_back(BufferSp(new Float32Buffer(negativeFloats)));
	spec.numWorkGroups = IVec3(numElements, 1, 1);

	group->addChild(new SpvAsmComputeShaderCase(testCtx, "all", "OpUnreachable appearing at different places", spec));

	return group.release();
}

// Assembly code used for testing decoration group is based on GLSL source code:
//
// #version 430
//
// layout(std140, set = 0, binding = 0) readonly buffer Input0 {
//   float elements[];
// } input_data0;
// layout(std140, set = 0, binding = 1) readonly buffer Input1 {
//   float elements[];
// } input_data1;
// layout(std140, set = 0, binding = 2) readonly buffer Input2 {
//   float elements[];
// } input_data2;
// layout(std140, set = 0, binding = 3) readonly buffer Input3 {
//   float elements[];
// } input_data3;
// layout(std140, set = 0, binding = 4) readonly buffer Input4 {
//   float elements[];
// } input_data4;
// layout(std140, set = 0, binding = 5) writeonly buffer Output {
//   float elements[];
// } output_data;
//
// void main() {
//   uint x = gl_GlobalInvocationID.x;
//   output_data.elements[x] = input_data0.elements[x] + input_data1.elements[x] + input_data2.elements[x] + input_data3.elements[x] + input_data4.elements[x];
// }
tcu::TestCaseGroup* createDecorationGroupGroup (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup>	group			(new tcu::TestCaseGroup(testCtx, "decoration_group", "Test the OpDecorationGroup & OpGroupDecorate instruction"));
	ComputeShaderSpec				spec;
	de::Random						rnd				(deStringHash(group->getName()));
	const int						numElements		= 100;
	vector<float>					inputFloats0	(numElements, 0);
	vector<float>					inputFloats1	(numElements, 0);
	vector<float>					inputFloats2	(numElements, 0);
	vector<float>					inputFloats3	(numElements, 0);
	vector<float>					inputFloats4	(numElements, 0);
	vector<float>					outputFloats	(numElements, 0);

	fillRandomScalars(rnd, -300.f, 300.f, &inputFloats0[0], numElements);
	fillRandomScalars(rnd, -300.f, 300.f, &inputFloats1[0], numElements);
	fillRandomScalars(rnd, -300.f, 300.f, &inputFloats2[0], numElements);
	fillRandomScalars(rnd, -300.f, 300.f, &inputFloats3[0], numElements);
	fillRandomScalars(rnd, -300.f, 300.f, &inputFloats4[0], numElements);

	// CPU might not use the same rounding mode as the GPU. Use whole numbers to avoid rounding differences.
	floorAll(inputFloats0);
	floorAll(inputFloats1);
	floorAll(inputFloats2);
	floorAll(inputFloats3);
	floorAll(inputFloats4);

	for (size_t ndx = 0; ndx < numElements; ++ndx)
		outputFloats[ndx] = inputFloats0[ndx] + inputFloats1[ndx] + inputFloats2[ndx] + inputFloats3[ndx] + inputFloats4[ndx];

	spec.assembly =
		string(s_ShaderPreamble) +

		"OpSource GLSL 430\n"
		"OpName %main \"main\"\n"
		"OpName %id \"gl_GlobalInvocationID\"\n"

		// Not using group decoration on variable.
		"OpDecorate %id BuiltIn GlobalInvocationId\n"
		// Not using group decoration on type.
		"OpDecorate %f32arr ArrayStride 4\n"

		"OpDecorate %groups BufferBlock\n"
		"OpDecorate %groupm Offset 0\n"
		"%groups = OpDecorationGroup\n"
		"%groupm = OpDecorationGroup\n"

		// Group decoration on multiple structs.
		"OpGroupDecorate %groups %outbuf %inbuf0 %inbuf1 %inbuf2 %inbuf3 %inbuf4\n"
		// Group decoration on multiple struct members.
		"OpGroupMemberDecorate %groupm %outbuf 0 %inbuf0 0 %inbuf1 0 %inbuf2 0 %inbuf3 0 %inbuf4 0\n"

		"OpDecorate %group1 DescriptorSet 0\n"
		"OpDecorate %group3 DescriptorSet 0\n"
		"OpDecorate %group3 NonWritable\n"
		"OpDecorate %group3 Restrict\n"
		"%group0 = OpDecorationGroup\n"
		"%group1 = OpDecorationGroup\n"
		"%group3 = OpDecorationGroup\n"

		// Applying the same decoration group multiple times.
		"OpGroupDecorate %group1 %outdata\n"
		"OpGroupDecorate %group1 %outdata\n"
		"OpGroupDecorate %group1 %outdata\n"
		"OpDecorate %outdata DescriptorSet 0\n"
		"OpDecorate %outdata Binding 5\n"
		// Applying decoration group containing nothing.
		"OpGroupDecorate %group0 %indata0\n"
		"OpDecorate %indata0 DescriptorSet 0\n"
		"OpDecorate %indata0 Binding 0\n"
		// Applying decoration group containing one decoration.
		"OpGroupDecorate %group1 %indata1\n"
		"OpDecorate %indata1 Binding 1\n"
		// Applying decoration group containing multiple decorations.
		"OpGroupDecorate %group3 %indata2 %indata3\n"
		"OpDecorate %indata2 Binding 2\n"
		"OpDecorate %indata3 Binding 3\n"
		// Applying multiple decoration groups (with overlapping).
		"OpGroupDecorate %group0 %indata4\n"
		"OpGroupDecorate %group1 %indata4\n"
		"OpGroupDecorate %group3 %indata4\n"
		"OpDecorate %indata4 Binding 4\n"

		+ string(s_CommonTypes) +

		"%id   = OpVariable %uvec3ptr Input\n"
		"%zero = OpConstant %i32 0\n"

		"%outbuf    = OpTypeStruct %f32arr\n"
		"%outbufptr = OpTypePointer Uniform %outbuf\n"
		"%outdata   = OpVariable %outbufptr Uniform\n"
		"%inbuf0    = OpTypeStruct %f32arr\n"
		"%inbuf0ptr = OpTypePointer Uniform %inbuf0\n"
		"%indata0   = OpVariable %inbuf0ptr Uniform\n"
		"%inbuf1    = OpTypeStruct %f32arr\n"
		"%inbuf1ptr = OpTypePointer Uniform %inbuf1\n"
		"%indata1   = OpVariable %inbuf1ptr Uniform\n"
		"%inbuf2    = OpTypeStruct %f32arr\n"
		"%inbuf2ptr = OpTypePointer Uniform %inbuf2\n"
		"%indata2   = OpVariable %inbuf2ptr Uniform\n"
		"%inbuf3    = OpTypeStruct %f32arr\n"
		"%inbuf3ptr = OpTypePointer Uniform %inbuf3\n"
		"%indata3   = OpVariable %inbuf3ptr Uniform\n"
		"%inbuf4    = OpTypeStruct %f32arr\n"
		"%inbufptr  = OpTypePointer Uniform %inbuf4\n"
		"%indata4   = OpVariable %inbufptr Uniform\n"

		"%main   = OpFunction %void None %voidf\n"
		"%label  = OpLabel\n"
		"%idval  = OpLoad %uvec3 %id\n"
		"%x      = OpCompositeExtract %u32 %idval 0\n"
		"%inloc0 = OpAccessChain %f32ptr %indata0 %zero %x\n"
		"%inloc1 = OpAccessChain %f32ptr %indata1 %zero %x\n"
		"%inloc2 = OpAccessChain %f32ptr %indata2 %zero %x\n"
		"%inloc3 = OpAccessChain %f32ptr %indata3 %zero %x\n"
		"%inloc4 = OpAccessChain %f32ptr %indata4 %zero %x\n"
		"%outloc = OpAccessChain %f32ptr %outdata %zero %x\n"
		"%inval0 = OpLoad %f32 %inloc0\n"
		"%inval1 = OpLoad %f32 %inloc1\n"
		"%inval2 = OpLoad %f32 %inloc2\n"
		"%inval3 = OpLoad %f32 %inloc3\n"
		"%inval4 = OpLoad %f32 %inloc4\n"
		"%add0   = OpFAdd %f32 %inval0 %inval1\n"
		"%add1   = OpFAdd %f32 %add0 %inval2\n"
		"%add2   = OpFAdd %f32 %add1 %inval3\n"
		"%add    = OpFAdd %f32 %add2 %inval4\n"
		"          OpStore %outloc %add\n"
		"          OpReturn\n"
		"          OpFunctionEnd\n";
	spec.inputs.push_back(BufferSp(new Float32Buffer(inputFloats0)));
	spec.inputs.push_back(BufferSp(new Float32Buffer(inputFloats1)));
	spec.inputs.push_back(BufferSp(new Float32Buffer(inputFloats2)));
	spec.inputs.push_back(BufferSp(new Float32Buffer(inputFloats3)));
	spec.inputs.push_back(BufferSp(new Float32Buffer(inputFloats4)));
	spec.outputs.push_back(BufferSp(new Float32Buffer(outputFloats)));
	spec.numWorkGroups = IVec3(numElements, 1, 1);

	group->addChild(new SpvAsmComputeShaderCase(testCtx, "all", "decoration group cases", spec));

	return group.release();
}

struct SpecConstantTwoIntCase
{
	const char*		caseName;
	const char*		scDefinition0;
	const char*		scDefinition1;
	const char*		scResultType;
	const char*		scOperation;
	deInt32			scActualValue0;
	deInt32			scActualValue1;
	const char*		resultOperation;
	vector<deInt32>	expectedOutput;

					SpecConstantTwoIntCase (const char* name,
											const char* definition0,
											const char* definition1,
											const char* resultType,
											const char* operation,
											deInt32 value0,
											deInt32 value1,
											const char* resultOp,
											const vector<deInt32>& output)
						: caseName			(name)
						, scDefinition0		(definition0)
						, scDefinition1		(definition1)
						, scResultType		(resultType)
						, scOperation		(operation)
						, scActualValue0	(value0)
						, scActualValue1	(value1)
						, resultOperation	(resultOp)
						, expectedOutput	(output) {}
};

tcu::TestCaseGroup* createSpecConstantGroup (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup>	group			(new tcu::TestCaseGroup(testCtx, "opspecconstantop", "Test the OpSpecConstantOp instruction"));
	vector<SpecConstantTwoIntCase>	cases;
	de::Random						rnd				(deStringHash(group->getName()));
	const int						numElements		= 100;
	vector<deInt32>					inputInts		(numElements, 0);
	vector<deInt32>					outputInts1		(numElements, 0);
	vector<deInt32>					outputInts2		(numElements, 0);
	vector<deInt32>					outputInts3		(numElements, 0);
	vector<deInt32>					outputInts4		(numElements, 0);
	const StringTemplate			shaderTemplate	(
		string(s_ShaderPreamble) +

		"OpName %main           \"main\"\n"
		"OpName %id             \"gl_GlobalInvocationID\"\n"

		"OpDecorate %id BuiltIn GlobalInvocationId\n"
		"OpDecorate %sc_0  SpecId 0\n"
		"OpDecorate %sc_1  SpecId 1\n"
		"OpDecorate %i32arr ArrayStride 4\n"

		+ string(s_InputOutputBufferTraits) + string(s_CommonTypes) +

		"%i32ptr    = OpTypePointer Uniform %i32\n"
		"%i32arr    = OpTypeRuntimeArray %i32\n"
		"%boolptr   = OpTypePointer Uniform %bool\n"
		"%boolarr   = OpTypeRuntimeArray %bool\n"
		"%buf     = OpTypeStruct %i32arr\n"
		"%bufptr  = OpTypePointer Uniform %buf\n"
		"%indata    = OpVariable %bufptr Uniform\n"
		"%outdata   = OpVariable %bufptr Uniform\n"

		"%id        = OpVariable %uvec3ptr Input\n"
		"%zero      = OpConstant %i32 0\n"

		"%sc_0      = OpSpecConstant${SC_DEF0}\n"
		"%sc_1      = OpSpecConstant${SC_DEF1}\n"
		"%sc_final  = OpSpecConstantOp ${SC_RESULT_TYPE} ${SC_OP}\n"

		"%main      = OpFunction %void None %voidf\n"
		"%label     = OpLabel\n"
		"%idval     = OpLoad %uvec3 %id\n"
		"%x         = OpCompositeExtract %u32 %idval 0\n"
		"%inloc     = OpAccessChain %i32ptr %indata %zero %x\n"
		"%inval     = OpLoad %i32 %inloc\n"
		"%final     = ${GEN_RESULT}\n"
		"%outloc    = OpAccessChain %i32ptr %outdata %zero %x\n"
		"             OpStore %outloc %final\n"
		"             OpReturn\n"
		"             OpFunctionEnd\n");

	fillRandomScalars(rnd, -65536, 65536, &inputInts[0], numElements);

	for (size_t ndx = 0; ndx < numElements; ++ndx)
	{
		outputInts1[ndx] = inputInts[ndx] + 42;
		outputInts2[ndx] = inputInts[ndx];
		outputInts3[ndx] = inputInts[ndx] - 11200;
		outputInts4[ndx] = inputInts[ndx] + 1;
	}

	const char addScToInput[]		= "OpIAdd %i32 %inval %sc_final";
	const char selectTrueUsingSc[]	= "OpSelect %i32 %sc_final %inval %zero";
	const char selectFalseUsingSc[]	= "OpSelect %i32 %sc_final %zero %inval";

	cases.push_back(SpecConstantTwoIntCase("iadd",					" %i32 0",		" %i32 0",		"%i32",		"IAdd                 %sc_0 %sc_1",			62,		-20,	addScToInput,		outputInts1));
	cases.push_back(SpecConstantTwoIntCase("isub",					" %i32 0",		" %i32 0",		"%i32",		"ISub                 %sc_0 %sc_1",			100,	58,		addScToInput,		outputInts1));
	cases.push_back(SpecConstantTwoIntCase("imul",					" %i32 0",		" %i32 0",		"%i32",		"IMul                 %sc_0 %sc_1",			-2,		-21,	addScToInput,		outputInts1));
	cases.push_back(SpecConstantTwoIntCase("sdiv",					" %i32 0",		" %i32 0",		"%i32",		"SDiv                 %sc_0 %sc_1",			-126,	-3,		addScToInput,		outputInts1));
	cases.push_back(SpecConstantTwoIntCase("udiv",					" %i32 0",		" %i32 0",		"%i32",		"UDiv                 %sc_0 %sc_1",			126,	3,		addScToInput,		outputInts1));
	cases.push_back(SpecConstantTwoIntCase("srem",					" %i32 0",		" %i32 0",		"%i32",		"SRem                 %sc_0 %sc_1",			7,		3,		addScToInput,		outputInts4));
	cases.push_back(SpecConstantTwoIntCase("smod",					" %i32 0",		" %i32 0",		"%i32",		"SMod                 %sc_0 %sc_1",			7,		3,		addScToInput,		outputInts4));
	cases.push_back(SpecConstantTwoIntCase("umod",					" %i32 0",		" %i32 0",		"%i32",		"UMod                 %sc_0 %sc_1",			342,	50,		addScToInput,		outputInts1));
	cases.push_back(SpecConstantTwoIntCase("bitwiseand",			" %i32 0",		" %i32 0",		"%i32",		"BitwiseAnd           %sc_0 %sc_1",			42,		63,		addScToInput,		outputInts1));
	cases.push_back(SpecConstantTwoIntCase("bitwiseor",				" %i32 0",		" %i32 0",		"%i32",		"BitwiseOr            %sc_0 %sc_1",			34,		8,		addScToInput,		outputInts1));
	cases.push_back(SpecConstantTwoIntCase("bitwisexor",			" %i32 0",		" %i32 0",		"%i32",		"BitwiseXor           %sc_0 %sc_1",			18,		56,		addScToInput,		outputInts1));
	cases.push_back(SpecConstantTwoIntCase("shiftrightlogical",		" %i32 0",		" %i32 0",		"%i32",		"ShiftRightLogical    %sc_0 %sc_1",			168,	2,		addScToInput,		outputInts1));
	cases.push_back(SpecConstantTwoIntCase("shiftrightarithmetic",	" %i32 0",		" %i32 0",		"%i32",		"ShiftRightArithmetic %sc_0 %sc_1",			168,	2,		addScToInput,		outputInts1));
	cases.push_back(SpecConstantTwoIntCase("shiftleftlogical",		" %i32 0",		" %i32 0",		"%i32",		"ShiftLeftLogical     %sc_0 %sc_1",			21,		1,		addScToInput,		outputInts1));
	cases.push_back(SpecConstantTwoIntCase("slessthan",				" %i32 0",		" %i32 0",		"%bool",	"SLessThan            %sc_0 %sc_1",			-20,	-10,	selectTrueUsingSc,	outputInts2));
	cases.push_back(SpecConstantTwoIntCase("ulessthan",				" %i32 0",		" %i32 0",		"%bool",	"ULessThan            %sc_0 %sc_1",			10,		20,		selectTrueUsingSc,	outputInts2));
	cases.push_back(SpecConstantTwoIntCase("sgreaterthan",			" %i32 0",		" %i32 0",		"%bool",	"SGreaterThan         %sc_0 %sc_1",			-1000,	50,		selectFalseUsingSc,	outputInts2));
	cases.push_back(SpecConstantTwoIntCase("ugreaterthan",			" %i32 0",		" %i32 0",		"%bool",	"UGreaterThan         %sc_0 %sc_1",			10,		5,		selectTrueUsingSc,	outputInts2));
	cases.push_back(SpecConstantTwoIntCase("slessthanequal",		" %i32 0",		" %i32 0",		"%bool",	"SLessThanEqual       %sc_0 %sc_1",			-10,	-10,	selectTrueUsingSc,	outputInts2));
	cases.push_back(SpecConstantTwoIntCase("ulessthanequal",		" %i32 0",		" %i32 0",		"%bool",	"ULessThanEqual       %sc_0 %sc_1",			50,		100,	selectTrueUsingSc,	outputInts2));
	cases.push_back(SpecConstantTwoIntCase("sgreaterthanequal",		" %i32 0",		" %i32 0",		"%bool",	"SGreaterThanEqual    %sc_0 %sc_1",			-1000,	50,		selectFalseUsingSc,	outputInts2));
	cases.push_back(SpecConstantTwoIntCase("ugreaterthanequal",		" %i32 0",		" %i32 0",		"%bool",	"UGreaterThanEqual    %sc_0 %sc_1",			10,		10,		selectTrueUsingSc,	outputInts2));
	cases.push_back(SpecConstantTwoIntCase("iequal",				" %i32 0",		" %i32 0",		"%bool",	"IEqual               %sc_0 %sc_1",			42,		24,		selectFalseUsingSc,	outputInts2));
	cases.push_back(SpecConstantTwoIntCase("logicaland",			"True %bool",	"True %bool",	"%bool",	"LogicalAnd           %sc_0 %sc_1",			0,		1,		selectFalseUsingSc,	outputInts2));
	cases.push_back(SpecConstantTwoIntCase("logicalor",				"False %bool",	"False %bool",	"%bool",	"LogicalOr            %sc_0 %sc_1",			1,		0,		selectTrueUsingSc,	outputInts2));
	cases.push_back(SpecConstantTwoIntCase("logicalequal",			"True %bool",	"True %bool",	"%bool",	"LogicalEqual         %sc_0 %sc_1",			0,		1,		selectFalseUsingSc,	outputInts2));
	cases.push_back(SpecConstantTwoIntCase("logicalnotequal",		"False %bool",	"False %bool",	"%bool",	"LogicalNotEqual      %sc_0 %sc_1",			1,		0,		selectTrueUsingSc,	outputInts2));
	cases.push_back(SpecConstantTwoIntCase("snegate",				" %i32 0",		" %i32 0",		"%i32",		"SNegate              %sc_0",				-42,	0,		addScToInput,		outputInts1));
	cases.push_back(SpecConstantTwoIntCase("not",					" %i32 0",		" %i32 0",		"%i32",		"Not                  %sc_0",				-43,	0,		addScToInput,		outputInts1));
	cases.push_back(SpecConstantTwoIntCase("logicalnot",			"False %bool",	"False %bool",	"%bool",	"LogicalNot           %sc_0",				1,		0,		selectFalseUsingSc,	outputInts2));
	cases.push_back(SpecConstantTwoIntCase("select",				"False %bool",	" %i32 0",		"%i32",		"Select               %sc_0 %sc_1 %zero",	1,		42,		addScToInput,		outputInts1));
	// OpSConvert, OpFConvert: these two instructions involve ints/floats of different bitwidths.

	for (size_t caseNdx = 0; caseNdx < cases.size(); ++caseNdx)
	{
		map<string, string>		specializations;
		ComputeShaderSpec		spec;

		specializations["SC_DEF0"]			= cases[caseNdx].scDefinition0;
		specializations["SC_DEF1"]			= cases[caseNdx].scDefinition1;
		specializations["SC_RESULT_TYPE"]	= cases[caseNdx].scResultType;
		specializations["SC_OP"]			= cases[caseNdx].scOperation;
		specializations["GEN_RESULT"]		= cases[caseNdx].resultOperation;

		spec.assembly = shaderTemplate.specialize(specializations);
		spec.inputs.push_back(BufferSp(new Int32Buffer(inputInts)));
		spec.outputs.push_back(BufferSp(new Int32Buffer(cases[caseNdx].expectedOutput)));
		spec.numWorkGroups = IVec3(numElements, 1, 1);
		spec.specConstants.push_back(cases[caseNdx].scActualValue0);
		spec.specConstants.push_back(cases[caseNdx].scActualValue1);

		group->addChild(new SpvAsmComputeShaderCase(testCtx, cases[caseNdx].caseName, cases[caseNdx].caseName, spec));
	}

	ComputeShaderSpec				spec;

	spec.assembly =
		string(s_ShaderPreamble) +

		"OpName %main           \"main\"\n"
		"OpName %id             \"gl_GlobalInvocationID\"\n"

		"OpDecorate %id BuiltIn GlobalInvocationId\n"
		"OpDecorate %sc_0  SpecId 0\n"
		"OpDecorate %sc_1  SpecId 1\n"
		"OpDecorate %sc_2  SpecId 2\n"
		"OpDecorate %i32arr ArrayStride 4\n"

		+ string(s_InputOutputBufferTraits) + string(s_CommonTypes) +

		"%ivec3     = OpTypeVector %i32 3\n"
		"%i32ptr    = OpTypePointer Uniform %i32\n"
		"%i32arr    = OpTypeRuntimeArray %i32\n"
		"%boolptr   = OpTypePointer Uniform %bool\n"
		"%boolarr   = OpTypeRuntimeArray %bool\n"
		"%buf     = OpTypeStruct %i32arr\n"
		"%bufptr  = OpTypePointer Uniform %buf\n"
		"%indata    = OpVariable %bufptr Uniform\n"
		"%outdata   = OpVariable %bufptr Uniform\n"

		"%id        = OpVariable %uvec3ptr Input\n"
		"%zero      = OpConstant %i32 0\n"
		"%ivec3_0   = OpConstantComposite %ivec3 %zero %zero %zero\n"

		"%sc_0        = OpSpecConstant %i32 0\n"
		"%sc_1        = OpSpecConstant %i32 0\n"
		"%sc_2        = OpSpecConstant %i32 0\n"
		"%sc_vec3_0   = OpSpecConstantOp %ivec3 CompositeInsert  %sc_0        %ivec3_0   0\n"     // (sc_0, 0, 0)
		"%sc_vec3_1   = OpSpecConstantOp %ivec3 CompositeInsert  %sc_1        %ivec3_0   1\n"     // (0, sc_1, 0)
		"%sc_vec3_2   = OpSpecConstantOp %ivec3 CompositeInsert  %sc_2        %ivec3_0   2\n"     // (0, 0, sc_2)
		"%sc_vec3_01  = OpSpecConstantOp %ivec3 VectorShuffle    %sc_vec3_0   %sc_vec3_1 1 0 4\n" // (0,    sc_0, sc_1)
		"%sc_vec3_012 = OpSpecConstantOp %ivec3 VectorShuffle    %sc_vec3_01  %sc_vec3_2 5 1 2\n" // (sc_2, sc_0, sc_1)
		"%sc_ext_0    = OpSpecConstantOp %i32   CompositeExtract %sc_vec3_012            0\n"     // sc_2
		"%sc_ext_1    = OpSpecConstantOp %i32   CompositeExtract %sc_vec3_012            1\n"     // sc_0
		"%sc_ext_2    = OpSpecConstantOp %i32   CompositeExtract %sc_vec3_012            2\n"     // sc_1
		"%sc_sub      = OpSpecConstantOp %i32   ISub             %sc_ext_0    %sc_ext_1\n"        // (sc_2 - sc_0)
		"%sc_final    = OpSpecConstantOp %i32   IMul             %sc_sub      %sc_ext_2\n"        // (sc_2 - sc_0) * sc_1

		"%main      = OpFunction %void None %voidf\n"
		"%label     = OpLabel\n"
		"%idval     = OpLoad %uvec3 %id\n"
		"%x         = OpCompositeExtract %u32 %idval 0\n"
		"%inloc     = OpAccessChain %i32ptr %indata %zero %x\n"
		"%inval     = OpLoad %i32 %inloc\n"
		"%final     = OpIAdd %i32 %inval %sc_final\n"
		"%outloc    = OpAccessChain %i32ptr %outdata %zero %x\n"
		"             OpStore %outloc %final\n"
		"             OpReturn\n"
		"             OpFunctionEnd\n";
	spec.inputs.push_back(BufferSp(new Int32Buffer(inputInts)));
	spec.outputs.push_back(BufferSp(new Int32Buffer(outputInts3)));
	spec.numWorkGroups = IVec3(numElements, 1, 1);
	spec.specConstants.push_back(123);
	spec.specConstants.push_back(56);
	spec.specConstants.push_back(-77);

	group->addChild(new SpvAsmComputeShaderCase(testCtx, "vector_related", "VectorShuffle, CompositeExtract, & CompositeInsert", spec));

	return group.release();
}

tcu::TestCaseGroup* createOpPhiGroup (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup>	group			(new tcu::TestCaseGroup(testCtx, "opphi", "Test the OpPhi instruction"));
	ComputeShaderSpec				spec1;
	ComputeShaderSpec				spec2;
	ComputeShaderSpec				spec3;
	de::Random						rnd				(deStringHash(group->getName()));
	const int						numElements		= 100;
	vector<float>					inputFloats		(numElements, 0);
	vector<float>					outputFloats1	(numElements, 0);
	vector<float>					outputFloats2	(numElements, 0);
	vector<float>					outputFloats3	(numElements, 0);

	fillRandomScalars(rnd, -300.f, 300.f, &inputFloats[0], numElements);

	// CPU might not use the same rounding mode as the GPU. Use whole numbers to avoid rounding differences.
	floorAll(inputFloats);

	for (size_t ndx = 0; ndx < numElements; ++ndx)
	{
		switch (ndx % 3)
		{
			case 0:		outputFloats1[ndx] = inputFloats[ndx] + 5.5f;	break;
			case 1:		outputFloats1[ndx] = inputFloats[ndx] + 20.5f;	break;
			case 2:		outputFloats1[ndx] = inputFloats[ndx] + 1.75f;	break;
			default:	break;
		}
		outputFloats2[ndx] = inputFloats[ndx] + 6.5f * 3;
		outputFloats3[ndx] = 8.5f - inputFloats[ndx];
	}

	spec1.assembly =
		string(s_ShaderPreamble) +

		"OpSource GLSL 430\n"
		"OpName %main \"main\"\n"
		"OpName %id \"gl_GlobalInvocationID\"\n"

		"OpDecorate %id BuiltIn GlobalInvocationId\n"

		+ string(s_InputOutputBufferTraits) + string(s_CommonTypes) + string(s_InputOutputBuffer) +

		"%id = OpVariable %uvec3ptr Input\n"
		"%zero       = OpConstant %i32 0\n"
		"%three      = OpConstant %u32 3\n"
		"%constf5p5  = OpConstant %f32 5.5\n"
		"%constf20p5 = OpConstant %f32 20.5\n"
		"%constf1p75 = OpConstant %f32 1.75\n"
		"%constf8p5  = OpConstant %f32 8.5\n"
		"%constf6p5  = OpConstant %f32 6.5\n"

		"%main     = OpFunction %void None %voidf\n"
		"%entry    = OpLabel\n"
		"%idval    = OpLoad %uvec3 %id\n"
		"%x        = OpCompositeExtract %u32 %idval 0\n"
		"%selector = OpUMod %u32 %x %three\n"
		"            OpSelectionMerge %phi None\n"
		"            OpSwitch %selector %default 0 %case0 1 %case1 2 %case2\n"

		// Case 1 before OpPhi.
		"%case1    = OpLabel\n"
		"            OpBranch %phi\n"

		"%default  = OpLabel\n"
		"            OpUnreachable\n"

		"%phi      = OpLabel\n"
		"%operand  = OpPhi %f32   %constf1p75 %case2   %constf20p5 %case1   %constf5p5 %case0\n" // not in the order of blocks
		"%inloc    = OpAccessChain %f32ptr %indata %zero %x\n"
		"%inval    = OpLoad %f32 %inloc\n"
		"%add      = OpFAdd %f32 %inval %operand\n"
		"%outloc   = OpAccessChain %f32ptr %outdata %zero %x\n"
		"            OpStore %outloc %add\n"
		"            OpReturn\n"

		// Case 0 after OpPhi.
		"%case0    = OpLabel\n"
		"            OpBranch %phi\n"


		// Case 2 after OpPhi.
		"%case2    = OpLabel\n"
		"            OpBranch %phi\n"

		"            OpFunctionEnd\n";
	spec1.inputs.push_back(BufferSp(new Float32Buffer(inputFloats)));
	spec1.outputs.push_back(BufferSp(new Float32Buffer(outputFloats1)));
	spec1.numWorkGroups = IVec3(numElements, 1, 1);

	group->addChild(new SpvAsmComputeShaderCase(testCtx, "block", "out-of-order and unreachable blocks for OpPhi", spec1));

	spec2.assembly =
		string(s_ShaderPreamble) +

		"OpName %main \"main\"\n"
		"OpName %id \"gl_GlobalInvocationID\"\n"

		"OpDecorate %id BuiltIn GlobalInvocationId\n"

		+ string(s_InputOutputBufferTraits) + string(s_CommonTypes) + string(s_InputOutputBuffer) +

		"%id         = OpVariable %uvec3ptr Input\n"
		"%zero       = OpConstant %i32 0\n"
		"%one        = OpConstant %i32 1\n"
		"%three      = OpConstant %i32 3\n"
		"%constf6p5  = OpConstant %f32 6.5\n"

		"%main       = OpFunction %void None %voidf\n"
		"%entry      = OpLabel\n"
		"%idval      = OpLoad %uvec3 %id\n"
		"%x          = OpCompositeExtract %u32 %idval 0\n"
		"%inloc      = OpAccessChain %f32ptr %indata %zero %x\n"
		"%outloc     = OpAccessChain %f32ptr %outdata %zero %x\n"
		"%inval      = OpLoad %f32 %inloc\n"
		"              OpBranch %phi\n"

		"%phi        = OpLabel\n"
		"%step       = OpPhi %i32 %zero  %entry %step_next  %phi\n"
		"%accum      = OpPhi %f32 %inval %entry %accum_next %phi\n"
		"%step_next  = OpIAdd %i32 %step %one\n"
		"%accum_next = OpFAdd %f32 %accum %constf6p5\n"
		"%still_loop = OpSLessThan %bool %step %three\n"
		"              OpLoopMerge %exit %phi None\n"
		"              OpBranchConditional %still_loop %phi %exit\n"

		"%exit       = OpLabel\n"
		"              OpStore %outloc %accum\n"
		"              OpReturn\n"
		"              OpFunctionEnd\n";
	spec2.inputs.push_back(BufferSp(new Float32Buffer(inputFloats)));
	spec2.outputs.push_back(BufferSp(new Float32Buffer(outputFloats2)));
	spec2.numWorkGroups = IVec3(numElements, 1, 1);

	group->addChild(new SpvAsmComputeShaderCase(testCtx, "induction", "The usual way induction variables are handled in LLVM IR", spec2));

	spec3.assembly =
		string(s_ShaderPreamble) +

		"OpName %main \"main\"\n"
		"OpName %id \"gl_GlobalInvocationID\"\n"

		"OpDecorate %id BuiltIn GlobalInvocationId\n"

		+ string(s_InputOutputBufferTraits) + string(s_CommonTypes) + string(s_InputOutputBuffer) +

		"%f32ptr_f   = OpTypePointer Function %f32\n"
		"%id         = OpVariable %uvec3ptr Input\n"
		"%true       = OpConstantTrue %bool\n"
		"%false      = OpConstantFalse %bool\n"
		"%zero       = OpConstant %i32 0\n"
		"%constf8p5  = OpConstant %f32 8.5\n"

		"%main       = OpFunction %void None %voidf\n"
		"%entry      = OpLabel\n"
		"%b          = OpVariable %f32ptr_f Function %constf8p5\n"
		"%idval      = OpLoad %uvec3 %id\n"
		"%x          = OpCompositeExtract %u32 %idval 0\n"
		"%inloc      = OpAccessChain %f32ptr %indata %zero %x\n"
		"%outloc     = OpAccessChain %f32ptr %outdata %zero %x\n"
		"%a_init     = OpLoad %f32 %inloc\n"
		"%b_init     = OpLoad %f32 %b\n"
		"              OpBranch %phi\n"

		"%phi        = OpLabel\n"
		"%still_loop = OpPhi %bool %true   %entry %false  %phi\n"
		"%a_next     = OpPhi %f32  %a_init %entry %b_next %phi\n"
		"%b_next     = OpPhi %f32  %b_init %entry %a_next %phi\n"
		"              OpLoopMerge %exit %phi None\n"
		"              OpBranchConditional %still_loop %phi %exit\n"

		"%exit       = OpLabel\n"
		"%sub        = OpFSub %f32 %a_next %b_next\n"
		"              OpStore %outloc %sub\n"
		"              OpReturn\n"
		"              OpFunctionEnd\n";
	spec3.inputs.push_back(BufferSp(new Float32Buffer(inputFloats)));
	spec3.outputs.push_back(BufferSp(new Float32Buffer(outputFloats3)));
	spec3.numWorkGroups = IVec3(numElements, 1, 1);

	group->addChild(new SpvAsmComputeShaderCase(testCtx, "swap", "Swap the values of two variables using OpPhi", spec3));

	return group.release();
}

// Assembly code used for testing block order is based on GLSL source code:
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
// void main() {
//   uint x = gl_GlobalInvocationID.x;
//   output_data.elements[x] = input_data.elements[x];
//   if (x > uint(50)) {
//     switch (x % uint(3)) {
//       case 0: output_data.elements[x] += 1.5f; break;
//       case 1: output_data.elements[x] += 42.f; break;
//       case 2: output_data.elements[x] -= 27.f; break;
//       default: break;
//     }
//   } else {
//     output_data.elements[x] = -input_data.elements[x];
//   }
// }
tcu::TestCaseGroup* createBlockOrderGroup (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup>	group			(new tcu::TestCaseGroup(testCtx, "block_order", "Test block orders"));
	ComputeShaderSpec				spec;
	de::Random						rnd				(deStringHash(group->getName()));
	const int						numElements		= 100;
	vector<float>					inputFloats		(numElements, 0);
	vector<float>					outputFloats	(numElements, 0);

	fillRandomScalars(rnd, -100.f, 100.f, &inputFloats[0], numElements);

	// CPU might not use the same rounding mode as the GPU. Use whole numbers to avoid rounding differences.
	floorAll(inputFloats);

	for (size_t ndx = 0; ndx <= 50; ++ndx)
		outputFloats[ndx] = -inputFloats[ndx];

	for (size_t ndx = 51; ndx < numElements; ++ndx)
	{
		switch (ndx % 3)
		{
			case 0:		outputFloats[ndx] = inputFloats[ndx] + 1.5f; break;
			case 1:		outputFloats[ndx] = inputFloats[ndx] + 42.f; break;
			case 2:		outputFloats[ndx] = inputFloats[ndx] - 27.f; break;
			default:	break;
		}
	}

	spec.assembly =
		string(s_ShaderPreamble) +

		"OpSource GLSL 430\n"
		"OpName %main \"main\"\n"
		"OpName %id \"gl_GlobalInvocationID\"\n"

		"OpDecorate %id BuiltIn GlobalInvocationId\n"

		+ string(s_InputOutputBufferTraits) + string(s_CommonTypes) +

		"%u32ptr       = OpTypePointer Function %u32\n"
		"%u32ptr_input = OpTypePointer Input %u32\n"

		+ string(s_InputOutputBuffer) +

		"%id        = OpVariable %uvec3ptr Input\n"
		"%zero      = OpConstant %i32 0\n"
		"%const3    = OpConstant %u32 3\n"
		"%const50   = OpConstant %u32 50\n"
		"%constf1p5 = OpConstant %f32 1.5\n"
		"%constf27  = OpConstant %f32 27.0\n"
		"%constf42  = OpConstant %f32 42.0\n"

		"%main = OpFunction %void None %voidf\n"

		// entry block.
		"%entry    = OpLabel\n"

		// Create a temporary variable to hold the value of gl_GlobalInvocationID.x.
		"%xvar     = OpVariable %u32ptr Function\n"
		"%xptr     = OpAccessChain %u32ptr_input %id %zero\n"
		"%x        = OpLoad %u32 %xptr\n"
		"            OpStore %xvar %x\n"

		"%cmp      = OpUGreaterThan %bool %x %const50\n"
		"            OpSelectionMerge %if_merge None\n"
		"            OpBranchConditional %cmp %if_true %if_false\n"

		// Merge block for switch-statement: placed at the beginning.
		"%switch_merge = OpLabel\n"
		"                OpBranch %if_merge\n"

		// Case 1 for switch-statement.
		"%case1    = OpLabel\n"
		"%x_1      = OpLoad %u32 %xvar\n"
		"%inloc_1  = OpAccessChain %f32ptr %indata %zero %x_1\n"
		"%inval_1  = OpLoad %f32 %inloc_1\n"
		"%addf42   = OpFAdd %f32 %inval_1 %constf42\n"
		"%outloc_1 = OpAccessChain %f32ptr %outdata %zero %x_1\n"
		"            OpStore %outloc_1 %addf42\n"
		"            OpBranch %switch_merge\n"

		// False branch for if-statement: placed in the middle of switch cases and before true branch.
		"%if_false = OpLabel\n"
		"%x_f      = OpLoad %u32 %xvar\n"
		"%inloc_f  = OpAccessChain %f32ptr %indata %zero %x_f\n"
		"%inval_f  = OpLoad %f32 %inloc_f\n"
		"%negate   = OpFNegate %f32 %inval_f\n"
		"%outloc_f = OpAccessChain %f32ptr %outdata %zero %x_f\n"
		"            OpStore %outloc_f %negate\n"
		"            OpBranch %if_merge\n"

		// Merge block for if-statement: placed in the middle of true and false branch.
		"%if_merge = OpLabel\n"
		"            OpReturn\n"

		// True branch for if-statement: placed in the middle of swtich cases and after the false branch.
		"%if_true  = OpLabel\n"
		"%xval_t   = OpLoad %u32 %xvar\n"
		"%mod      = OpUMod %u32 %xval_t %const3\n"
		"            OpSelectionMerge %switch_merge None\n"
		"            OpSwitch %mod %default 0 %case0 1 %case1 2 %case2\n"

		// Case 2 for switch-statement.
		"%case2    = OpLabel\n"
		"%x_2      = OpLoad %u32 %xvar\n"
		"%inloc_2  = OpAccessChain %f32ptr %indata %zero %x_2\n"
		"%inval_2  = OpLoad %f32 %inloc_2\n"
		"%subf27   = OpFSub %f32 %inval_2 %constf27\n"
		"%outloc_2 = OpAccessChain %f32ptr %outdata %zero %x_2\n"
		"            OpStore %outloc_2 %subf27\n"
		"            OpBranch %switch_merge\n"

		// Default case for switch-statement: placed in the middle of normal cases.
		"%default = OpLabel\n"
		"           OpBranch %switch_merge\n"

		// Case 0 for switch-statement: out of order.
		"%case0    = OpLabel\n"
		"%x_0      = OpLoad %u32 %xvar\n"
		"%inloc_0  = OpAccessChain %f32ptr %indata %zero %x_0\n"
		"%inval_0  = OpLoad %f32 %inloc_0\n"
		"%addf1p5  = OpFAdd %f32 %inval_0 %constf1p5\n"
		"%outloc_0 = OpAccessChain %f32ptr %outdata %zero %x_0\n"
		"            OpStore %outloc_0 %addf1p5\n"
		"            OpBranch %switch_merge\n"

		"            OpFunctionEnd\n";
	spec.inputs.push_back(BufferSp(new Float32Buffer(inputFloats)));
	spec.outputs.push_back(BufferSp(new Float32Buffer(outputFloats)));
	spec.numWorkGroups = IVec3(numElements, 1, 1);

	group->addChild(new SpvAsmComputeShaderCase(testCtx, "all", "various out-of-order blocks", spec));

	return group.release();
}

tcu::TestCaseGroup* createMultipleShaderGroup (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup>	group			(new tcu::TestCaseGroup(testCtx, "multiple_shaders", "Test multiple shaders in the same module"));
	ComputeShaderSpec				spec1;
	ComputeShaderSpec				spec2;
	de::Random						rnd				(deStringHash(group->getName()));
	const int						numElements		= 100;
	vector<float>					inputFloats		(numElements, 0);
	vector<float>					outputFloats1	(numElements, 0);
	vector<float>					outputFloats2	(numElements, 0);
	fillRandomScalars(rnd, -500.f, 500.f, &inputFloats[0], numElements);

	for (size_t ndx = 0; ndx < numElements; ++ndx)
	{
		outputFloats1[ndx] = inputFloats[ndx] + inputFloats[ndx];
		outputFloats2[ndx] = -inputFloats[ndx];
	}

	const string assembly(
		"OpCapability Shader\n"
		"OpCapability ClipDistance\n"
		"OpMemoryModel Logical GLSL450\n"
		"OpEntryPoint GLCompute %comp_main1 \"entrypoint1\" %id\n"
		"OpEntryPoint GLCompute %comp_main2 \"entrypoint2\" %id\n"
		// A module cannot have two OpEntryPoint instructions with the same Execution Model and the same Name string.
		"OpEntryPoint Vertex    %vert_main  \"entrypoint2\" %vert_builtins %vertexIndex %instanceIndex\n"
		"OpExecutionMode %comp_main1 LocalSize 1 1 1\n"
		"OpExecutionMode %comp_main2 LocalSize 1 1 1\n"

		"OpName %comp_main1              \"entrypoint1\"\n"
		"OpName %comp_main2              \"entrypoint2\"\n"
		"OpName %vert_main               \"entrypoint2\"\n"
		"OpName %id                      \"gl_GlobalInvocationID\"\n"
		"OpName %vert_builtin_st         \"gl_PerVertex\"\n"
		"OpName %vertexIndex             \"gl_VertexIndex\"\n"
		"OpName %instanceIndex           \"gl_InstanceIndex\"\n"
		"OpMemberName %vert_builtin_st 0 \"gl_Position\"\n"
		"OpMemberName %vert_builtin_st 1 \"gl_PointSize\"\n"
		"OpMemberName %vert_builtin_st 2 \"gl_ClipDistance\"\n"

		"OpDecorate %id                      BuiltIn GlobalInvocationId\n"
		"OpDecorate %vertexIndex             BuiltIn VertexIndex\n"
		"OpDecorate %instanceIndex           BuiltIn InstanceIndex\n"
		"OpDecorate %vert_builtin_st         Block\n"
		"OpMemberDecorate %vert_builtin_st 0 BuiltIn Position\n"
		"OpMemberDecorate %vert_builtin_st 1 BuiltIn PointSize\n"
		"OpMemberDecorate %vert_builtin_st 2 BuiltIn ClipDistance\n"

		+ string(s_InputOutputBufferTraits) + string(s_CommonTypes) + string(s_InputOutputBuffer) +

		"%zero       = OpConstant %i32 0\n"
		"%one        = OpConstant %u32 1\n"
		"%c_f32_1    = OpConstant %f32 1\n"

		"%i32ptr              = OpTypePointer Input %i32\n"
		"%vec4                = OpTypeVector %f32 4\n"
		"%vec4ptr             = OpTypePointer Output %vec4\n"
		"%f32arr1             = OpTypeArray %f32 %one\n"
		"%vert_builtin_st     = OpTypeStruct %vec4 %f32 %f32arr1\n"
		"%vert_builtin_st_ptr = OpTypePointer Output %vert_builtin_st\n"
		"%vert_builtins       = OpVariable %vert_builtin_st_ptr Output\n"

		"%id         = OpVariable %uvec3ptr Input\n"
		"%vertexIndex = OpVariable %i32ptr Input\n"
		"%instanceIndex = OpVariable %i32ptr Input\n"
		"%c_vec4_1   = OpConstantComposite %vec4 %c_f32_1 %c_f32_1 %c_f32_1 %c_f32_1\n"

		// gl_Position = vec4(1.);
		"%vert_main  = OpFunction %void None %voidf\n"
		"%vert_entry = OpLabel\n"
		"%position   = OpAccessChain %vec4ptr %vert_builtins %zero\n"
		"              OpStore %position %c_vec4_1\n"
		"              OpReturn\n"
		"              OpFunctionEnd\n"

		// Double inputs.
		"%comp_main1  = OpFunction %void None %voidf\n"
		"%comp1_entry = OpLabel\n"
		"%idval1      = OpLoad %uvec3 %id\n"
		"%x1          = OpCompositeExtract %u32 %idval1 0\n"
		"%inloc1      = OpAccessChain %f32ptr %indata %zero %x1\n"
		"%inval1      = OpLoad %f32 %inloc1\n"
		"%add         = OpFAdd %f32 %inval1 %inval1\n"
		"%outloc1     = OpAccessChain %f32ptr %outdata %zero %x1\n"
		"               OpStore %outloc1 %add\n"
		"               OpReturn\n"
		"               OpFunctionEnd\n"

		// Negate inputs.
		"%comp_main2  = OpFunction %void None %voidf\n"
		"%comp2_entry = OpLabel\n"
		"%idval2      = OpLoad %uvec3 %id\n"
		"%x2          = OpCompositeExtract %u32 %idval2 0\n"
		"%inloc2      = OpAccessChain %f32ptr %indata %zero %x2\n"
		"%inval2      = OpLoad %f32 %inloc2\n"
		"%neg         = OpFNegate %f32 %inval2\n"
		"%outloc2     = OpAccessChain %f32ptr %outdata %zero %x2\n"
		"               OpStore %outloc2 %neg\n"
		"               OpReturn\n"
		"               OpFunctionEnd\n");

	spec1.assembly = assembly;
	spec1.inputs.push_back(BufferSp(new Float32Buffer(inputFloats)));
	spec1.outputs.push_back(BufferSp(new Float32Buffer(outputFloats1)));
	spec1.numWorkGroups = IVec3(numElements, 1, 1);
	spec1.entryPoint = "entrypoint1";

	spec2.assembly = assembly;
	spec2.inputs.push_back(BufferSp(new Float32Buffer(inputFloats)));
	spec2.outputs.push_back(BufferSp(new Float32Buffer(outputFloats2)));
	spec2.numWorkGroups = IVec3(numElements, 1, 1);
	spec2.entryPoint = "entrypoint2";

	group->addChild(new SpvAsmComputeShaderCase(testCtx, "shader1", "multiple shaders in the same module", spec1));
	group->addChild(new SpvAsmComputeShaderCase(testCtx, "shader2", "multiple shaders in the same module", spec2));

	return group.release();
}

inline std::string makeLongUTF8String (size_t num4ByteChars)
{
	// An example of a longest valid UTF-8 character.  Be explicit about the
	// character type because Microsoft compilers can otherwise interpret the
	// character string as being over wide (16-bit) characters. Ideally, we
	// would just use a C++11 UTF-8 string literal, but we want to support older
	// Microsoft compilers.
	const std::basic_string<char> earthAfrica("\xF0\x9F\x8C\x8D");
	std::string longString;
	longString.reserve(num4ByteChars * 4);
	for (size_t count = 0; count < num4ByteChars; count++)
	{
		longString += earthAfrica;
	}
	return longString;
}

tcu::TestCaseGroup* createOpSourceGroup (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup>	group			(new tcu::TestCaseGroup(testCtx, "opsource", "Tests the OpSource & OpSourceContinued instruction"));
	vector<CaseParameter>			cases;
	de::Random						rnd				(deStringHash(group->getName()));
	const int						numElements		= 100;
	vector<float>					positiveFloats	(numElements, 0);
	vector<float>					negativeFloats	(numElements, 0);
	const StringTemplate			shaderTemplate	(
		"OpCapability Shader\n"
		"OpMemoryModel Logical GLSL450\n"

		"OpEntryPoint GLCompute %main \"main\" %id\n"
		"OpExecutionMode %main LocalSize 1 1 1\n"

		"${SOURCE}\n"

		"OpName %main           \"main\"\n"
		"OpName %id             \"gl_GlobalInvocationID\"\n"

		"OpDecorate %id BuiltIn GlobalInvocationId\n"

		+ string(s_InputOutputBufferTraits) + string(s_CommonTypes) + string(s_InputOutputBuffer) +

		"%id        = OpVariable %uvec3ptr Input\n"
		"%zero      = OpConstant %i32 0\n"

		"%main      = OpFunction %void None %voidf\n"
		"%label     = OpLabel\n"
		"%idval     = OpLoad %uvec3 %id\n"
		"%x         = OpCompositeExtract %u32 %idval 0\n"
		"%inloc     = OpAccessChain %f32ptr %indata %zero %x\n"
		"%inval     = OpLoad %f32 %inloc\n"
		"%neg       = OpFNegate %f32 %inval\n"
		"%outloc    = OpAccessChain %f32ptr %outdata %zero %x\n"
		"             OpStore %outloc %neg\n"
		"             OpReturn\n"
		"             OpFunctionEnd\n");

	cases.push_back(CaseParameter("unknown_source",							"OpSource Unknown 0"));
	cases.push_back(CaseParameter("wrong_source",							"OpSource OpenCL_C 210"));
	cases.push_back(CaseParameter("normal_filename",						"%fname = OpString \"filename\"\n"
																			"OpSource GLSL 430 %fname"));
	cases.push_back(CaseParameter("empty_filename",							"%fname = OpString \"\"\n"
																			"OpSource GLSL 430 %fname"));
	cases.push_back(CaseParameter("normal_source_code",						"%fname = OpString \"filename\"\n"
																			"OpSource GLSL 430 %fname \"#version 430\nvoid main() {}\""));
	cases.push_back(CaseParameter("empty_source_code",						"%fname = OpString \"filename\"\n"
																			"OpSource GLSL 430 %fname \"\""));
	cases.push_back(CaseParameter("long_source_code",						"%fname = OpString \"filename\"\n"
																			"OpSource GLSL 430 %fname \"" + makeLongUTF8String(65530) + "ccc\"")); // word count: 65535
	cases.push_back(CaseParameter("utf8_source_code",						"%fname = OpString \"filename\"\n"
																			"OpSource GLSL 430 %fname \"\xE2\x98\x82\xE2\x98\x85\"")); // umbrella & black star symbol
	cases.push_back(CaseParameter("normal_sourcecontinued",					"%fname = OpString \"filename\"\n"
																			"OpSource GLSL 430 %fname \"#version 430\nvo\"\n"
																			"OpSourceContinued \"id main() {}\""));
	cases.push_back(CaseParameter("empty_sourcecontinued",					"%fname = OpString \"filename\"\n"
																			"OpSource GLSL 430 %fname \"#version 430\nvoid main() {}\"\n"
																			"OpSourceContinued \"\""));
	cases.push_back(CaseParameter("long_sourcecontinued",					"%fname = OpString \"filename\"\n"
																			"OpSource GLSL 430 %fname \"#version 430\nvoid main() {}\"\n"
																			"OpSourceContinued \"" + makeLongUTF8String(65533) + "ccc\"")); // word count: 65535
	cases.push_back(CaseParameter("utf8_sourcecontinued",					"%fname = OpString \"filename\"\n"
																			"OpSource GLSL 430 %fname \"#version 430\nvoid main() {}\"\n"
																			"OpSourceContinued \"\xE2\x98\x8E\xE2\x9A\x91\"")); // white telephone & black flag symbol
	cases.push_back(CaseParameter("multi_sourcecontinued",					"%fname = OpString \"filename\"\n"
																			"OpSource GLSL 430 %fname \"#version 430\n\"\n"
																			"OpSourceContinued \"void\"\n"
																			"OpSourceContinued \"main()\"\n"
																			"OpSourceContinued \"{}\""));
	cases.push_back(CaseParameter("empty_source_before_sourcecontinued",	"%fname = OpString \"filename\"\n"
																			"OpSource GLSL 430 %fname \"\"\n"
																			"OpSourceContinued \"#version 430\nvoid main() {}\""));

	fillRandomScalars(rnd, 1.f, 100.f, &positiveFloats[0], numElements);

	for (size_t ndx = 0; ndx < numElements; ++ndx)
		negativeFloats[ndx] = -positiveFloats[ndx];

	for (size_t caseNdx = 0; caseNdx < cases.size(); ++caseNdx)
	{
		map<string, string>		specializations;
		ComputeShaderSpec		spec;

		specializations["SOURCE"] = cases[caseNdx].param;
		spec.assembly = shaderTemplate.specialize(specializations);
		spec.inputs.push_back(BufferSp(new Float32Buffer(positiveFloats)));
		spec.outputs.push_back(BufferSp(new Float32Buffer(negativeFloats)));
		spec.numWorkGroups = IVec3(numElements, 1, 1);

		group->addChild(new SpvAsmComputeShaderCase(testCtx, cases[caseNdx].name, cases[caseNdx].name, spec));
	}

	return group.release();
}

tcu::TestCaseGroup* createOpSourceExtensionGroup (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup>	group			(new tcu::TestCaseGroup(testCtx, "opsourceextension", "Tests the OpSource instruction"));
	vector<CaseParameter>			cases;
	de::Random						rnd				(deStringHash(group->getName()));
	const int						numElements		= 100;
	vector<float>					inputFloats		(numElements, 0);
	vector<float>					outputFloats	(numElements, 0);
	const StringTemplate			shaderTemplate	(
		string(s_ShaderPreamble) +

		"OpSourceExtension \"${EXTENSION}\"\n"

		"OpName %main           \"main\"\n"
		"OpName %id             \"gl_GlobalInvocationID\"\n"

		"OpDecorate %id BuiltIn GlobalInvocationId\n"

		+ string(s_InputOutputBufferTraits) + string(s_CommonTypes) + string(s_InputOutputBuffer) +

		"%id        = OpVariable %uvec3ptr Input\n"
		"%zero      = OpConstant %i32 0\n"

		"%main      = OpFunction %void None %voidf\n"
		"%label     = OpLabel\n"
		"%idval     = OpLoad %uvec3 %id\n"
		"%x         = OpCompositeExtract %u32 %idval 0\n"
		"%inloc     = OpAccessChain %f32ptr %indata %zero %x\n"
		"%inval     = OpLoad %f32 %inloc\n"
		"%neg       = OpFNegate %f32 %inval\n"
		"%outloc    = OpAccessChain %f32ptr %outdata %zero %x\n"
		"             OpStore %outloc %neg\n"
		"             OpReturn\n"
		"             OpFunctionEnd\n");

	cases.push_back(CaseParameter("empty_extension",	""));
	cases.push_back(CaseParameter("real_extension",		"GL_ARB_texture_rectangle"));
	cases.push_back(CaseParameter("fake_extension",		"GL_ARB_im_the_ultimate_extension"));
	cases.push_back(CaseParameter("utf8_extension",		"GL_ARB_\xE2\x98\x82\xE2\x98\x85"));
	cases.push_back(CaseParameter("long_extension",		makeLongUTF8String(65533) + "ccc")); // word count: 65535

	fillRandomScalars(rnd, -200.f, 200.f, &inputFloats[0], numElements);

	for (size_t ndx = 0; ndx < numElements; ++ndx)
		outputFloats[ndx] = -inputFloats[ndx];

	for (size_t caseNdx = 0; caseNdx < cases.size(); ++caseNdx)
	{
		map<string, string>		specializations;
		ComputeShaderSpec		spec;

		specializations["EXTENSION"] = cases[caseNdx].param;
		spec.assembly = shaderTemplate.specialize(specializations);
		spec.inputs.push_back(BufferSp(new Float32Buffer(inputFloats)));
		spec.outputs.push_back(BufferSp(new Float32Buffer(outputFloats)));
		spec.numWorkGroups = IVec3(numElements, 1, 1);

		group->addChild(new SpvAsmComputeShaderCase(testCtx, cases[caseNdx].name, cases[caseNdx].name, spec));
	}

	return group.release();
}

// Checks that a compute shader can generate a constant null value of various types, without exercising a computation on it.
tcu::TestCaseGroup* createOpConstantNullGroup (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup>	group			(new tcu::TestCaseGroup(testCtx, "opconstantnull", "Tests the OpConstantNull instruction"));
	vector<CaseParameter>			cases;
	de::Random						rnd				(deStringHash(group->getName()));
	const int						numElements		= 100;
	vector<float>					positiveFloats	(numElements, 0);
	vector<float>					negativeFloats	(numElements, 0);
	const StringTemplate			shaderTemplate	(
		string(s_ShaderPreamble) +

		"OpSource GLSL 430\n"
		"OpName %main           \"main\"\n"
		"OpName %id             \"gl_GlobalInvocationID\"\n"

		"OpDecorate %id BuiltIn GlobalInvocationId\n"

		+ string(s_InputOutputBufferTraits) + string(s_CommonTypes) + string(s_InputOutputBuffer) +

		"${TYPE}\n"
		"%null      = OpConstantNull %type\n"

		"%id        = OpVariable %uvec3ptr Input\n"
		"%zero      = OpConstant %i32 0\n"

		"%main      = OpFunction %void None %voidf\n"
		"%label     = OpLabel\n"
		"%idval     = OpLoad %uvec3 %id\n"
		"%x         = OpCompositeExtract %u32 %idval 0\n"
		"%inloc     = OpAccessChain %f32ptr %indata %zero %x\n"
		"%inval     = OpLoad %f32 %inloc\n"
		"%neg       = OpFNegate %f32 %inval\n"
		"%outloc    = OpAccessChain %f32ptr %outdata %zero %x\n"
		"             OpStore %outloc %neg\n"
		"             OpReturn\n"
		"             OpFunctionEnd\n");

	cases.push_back(CaseParameter("bool",			"%type = OpTypeBool"));
	cases.push_back(CaseParameter("sint32",			"%type = OpTypeInt 32 1"));
	cases.push_back(CaseParameter("uint32",			"%type = OpTypeInt 32 0"));
	cases.push_back(CaseParameter("float32",		"%type = OpTypeFloat 32"));
	cases.push_back(CaseParameter("vec4float32",	"%type = OpTypeVector %f32 4"));
	cases.push_back(CaseParameter("vec3bool",		"%type = OpTypeVector %bool 3"));
	cases.push_back(CaseParameter("vec2uint32",		"%type = OpTypeVector %u32 2"));
	cases.push_back(CaseParameter("matrix",			"%type = OpTypeMatrix %fvec3 3"));
	cases.push_back(CaseParameter("array",			"%100 = OpConstant %u32 100\n"
													"%type = OpTypeArray %i32 %100"));
	cases.push_back(CaseParameter("struct",			"%type = OpTypeStruct %f32 %i32 %u32"));
	cases.push_back(CaseParameter("pointer",		"%type = OpTypePointer Function %i32"));

	fillRandomScalars(rnd, 1.f, 100.f, &positiveFloats[0], numElements);

	for (size_t ndx = 0; ndx < numElements; ++ndx)
		negativeFloats[ndx] = -positiveFloats[ndx];

	for (size_t caseNdx = 0; caseNdx < cases.size(); ++caseNdx)
	{
		map<string, string>		specializations;
		ComputeShaderSpec		spec;

		specializations["TYPE"] = cases[caseNdx].param;
		spec.assembly = shaderTemplate.specialize(specializations);
		spec.inputs.push_back(BufferSp(new Float32Buffer(positiveFloats)));
		spec.outputs.push_back(BufferSp(new Float32Buffer(negativeFloats)));
		spec.numWorkGroups = IVec3(numElements, 1, 1);

		group->addChild(new SpvAsmComputeShaderCase(testCtx, cases[caseNdx].name, cases[caseNdx].name, spec));
	}

	return group.release();
}

// Checks that a compute shader can generate a constant composite value of various types, without exercising a computation on it.
tcu::TestCaseGroup* createOpConstantCompositeGroup (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup>	group			(new tcu::TestCaseGroup(testCtx, "opconstantcomposite", "Tests the OpConstantComposite instruction"));
	vector<CaseParameter>			cases;
	de::Random						rnd				(deStringHash(group->getName()));
	const int						numElements		= 100;
	vector<float>					positiveFloats	(numElements, 0);
	vector<float>					negativeFloats	(numElements, 0);
	const StringTemplate			shaderTemplate	(
		string(s_ShaderPreamble) +

		"OpSource GLSL 430\n"
		"OpName %main           \"main\"\n"
		"OpName %id             \"gl_GlobalInvocationID\"\n"

		"OpDecorate %id BuiltIn GlobalInvocationId\n"

		+ string(s_InputOutputBufferTraits) + string(s_CommonTypes) + string(s_InputOutputBuffer) +

		"%id        = OpVariable %uvec3ptr Input\n"
		"%zero      = OpConstant %i32 0\n"

		"${CONSTANT}\n"

		"%main      = OpFunction %void None %voidf\n"
		"%label     = OpLabel\n"
		"%idval     = OpLoad %uvec3 %id\n"
		"%x         = OpCompositeExtract %u32 %idval 0\n"
		"%inloc     = OpAccessChain %f32ptr %indata %zero %x\n"
		"%inval     = OpLoad %f32 %inloc\n"
		"%neg       = OpFNegate %f32 %inval\n"
		"%outloc    = OpAccessChain %f32ptr %outdata %zero %x\n"
		"             OpStore %outloc %neg\n"
		"             OpReturn\n"
		"             OpFunctionEnd\n");

	cases.push_back(CaseParameter("vector",			"%five = OpConstant %u32 5\n"
													"%const = OpConstantComposite %uvec3 %five %zero %five"));
	cases.push_back(CaseParameter("matrix",			"%m3fvec3 = OpTypeMatrix %fvec3 3\n"
													"%ten = OpConstant %f32 10.\n"
													"%fzero = OpConstant %f32 0.\n"
													"%vec = OpConstantComposite %fvec3 %ten %fzero %ten\n"
													"%mat = OpConstantComposite %m3fvec3 %vec %vec %vec"));
	cases.push_back(CaseParameter("struct",			"%m2vec3 = OpTypeMatrix %fvec3 2\n"
													"%struct = OpTypeStruct %i32 %f32 %fvec3 %m2vec3\n"
													"%fzero = OpConstant %f32 0.\n"
													"%one = OpConstant %f32 1.\n"
													"%point5 = OpConstant %f32 0.5\n"
													"%vec = OpConstantComposite %fvec3 %one %one %fzero\n"
													"%mat = OpConstantComposite %m2vec3 %vec %vec\n"
													"%const = OpConstantComposite %struct %zero %point5 %vec %mat"));
	cases.push_back(CaseParameter("nested_struct",	"%st1 = OpTypeStruct %u32 %f32\n"
													"%st2 = OpTypeStruct %i32 %i32\n"
													"%struct = OpTypeStruct %st1 %st2\n"
													"%point5 = OpConstant %f32 0.5\n"
													"%one = OpConstant %u32 1\n"
													"%ten = OpConstant %i32 10\n"
													"%st1val = OpConstantComposite %st1 %one %point5\n"
													"%st2val = OpConstantComposite %st2 %ten %ten\n"
													"%const = OpConstantComposite %struct %st1val %st2val"));

	fillRandomScalars(rnd, 1.f, 100.f, &positiveFloats[0], numElements);

	for (size_t ndx = 0; ndx < numElements; ++ndx)
		negativeFloats[ndx] = -positiveFloats[ndx];

	for (size_t caseNdx = 0; caseNdx < cases.size(); ++caseNdx)
	{
		map<string, string>		specializations;
		ComputeShaderSpec		spec;

		specializations["CONSTANT"] = cases[caseNdx].param;
		spec.assembly = shaderTemplate.specialize(specializations);
		spec.inputs.push_back(BufferSp(new Float32Buffer(positiveFloats)));
		spec.outputs.push_back(BufferSp(new Float32Buffer(negativeFloats)));
		spec.numWorkGroups = IVec3(numElements, 1, 1);

		group->addChild(new SpvAsmComputeShaderCase(testCtx, cases[caseNdx].name, cases[caseNdx].name, spec));
	}

	return group.release();
}

// Creates a floating point number with the given exponent, and significand
// bits set. It can only create normalized numbers. Only the least significant
// 24 bits of the significand will be examined. The final bit of the
// significand will also be ignored. This allows alignment to be written
// similarly to C99 hex-floats.
// For example if you wanted to write 0x1.7f34p-12 you would call
// constructNormalizedFloat(-12, 0x7f3400)
float constructNormalizedFloat (deInt32 exponent, deUint32 significand)
{
	float f = 1.0f;

	for (deInt32 idx = 0; idx < 23; ++idx)
	{
		f += ((significand & 0x800000) == 0) ? 0.f : std::ldexp(1.0f, -(idx + 1));
		significand <<= 1;
	}

	return std::ldexp(f, exponent);
}

// Compare instruction for the OpQuantizeF16 compute exact case.
// Returns true if the output is what is expected from the test case.
bool compareOpQuantizeF16ComputeExactCase (const std::vector<BufferSp>&, const vector<AllocationSp>& outputAllocs, const std::vector<BufferSp>& expectedOutputs)
{
	if (outputAllocs.size() != 1)
		return false;

	// We really just need this for size because we cannot compare Nans.
	const BufferSp&	expectedOutput	= expectedOutputs[0];
	const float*	outputAsFloat	= static_cast<const float*>(outputAllocs[0]->getHostPtr());;

	if (expectedOutput->getNumBytes() != 4*sizeof(float)) {
		return false;
	}

	if (*outputAsFloat != constructNormalizedFloat(8, 0x304000) &&
		*outputAsFloat != constructNormalizedFloat(8, 0x300000)) {
		return false;
	}
	outputAsFloat++;

	if (*outputAsFloat != -constructNormalizedFloat(-7, 0x600000) &&
		*outputAsFloat != -constructNormalizedFloat(-7, 0x604000)) {
		return false;
	}
	outputAsFloat++;

	if (*outputAsFloat != constructNormalizedFloat(2, 0x01C000) &&
		*outputAsFloat != constructNormalizedFloat(2, 0x020000)) {
		return false;
	}
	outputAsFloat++;

	if (*outputAsFloat != constructNormalizedFloat(1, 0xFFC000) &&
		*outputAsFloat != constructNormalizedFloat(2, 0x000000)) {
		return false;
	}

	return true;
}

// Checks that every output from a test-case is a float NaN.
bool compareNan (const std::vector<BufferSp>&, const vector<AllocationSp>& outputAllocs, const std::vector<BufferSp>& expectedOutputs)
{
	if (outputAllocs.size() != 1)
		return false;

	// We really just need this for size because we cannot compare Nans.
	const BufferSp& expectedOutput		= expectedOutputs[0];
	const float* output_as_float		= static_cast<const float*>(outputAllocs[0]->getHostPtr());;

	for (size_t idx = 0; idx < expectedOutput->getNumBytes() / sizeof(float); ++idx)
	{
		if (!isnan(output_as_float[idx]))
		{
			return false;
		}
	}

	return true;
}

// Checks that a compute shader can generate a constant composite value of various types, without exercising a computation on it.
tcu::TestCaseGroup* createOpQuantizeToF16Group (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup>	group			(new tcu::TestCaseGroup(testCtx, "opquantize", "Tests the OpQuantizeToF16 instruction"));

	const std::string shader (
		string(s_ShaderPreamble) +

		"OpSource GLSL 430\n"
		"OpName %main           \"main\"\n"
		"OpName %id             \"gl_GlobalInvocationID\"\n"

		"OpDecorate %id BuiltIn GlobalInvocationId\n"

		+ string(s_InputOutputBufferTraits) + string(s_CommonTypes) + string(s_InputOutputBuffer) +

		"%id        = OpVariable %uvec3ptr Input\n"
		"%zero      = OpConstant %i32 0\n"

		"%main      = OpFunction %void None %voidf\n"
		"%label     = OpLabel\n"
		"%idval     = OpLoad %uvec3 %id\n"
		"%x         = OpCompositeExtract %u32 %idval 0\n"
		"%inloc     = OpAccessChain %f32ptr %indata %zero %x\n"
		"%inval     = OpLoad %f32 %inloc\n"
		"%quant     = OpQuantizeToF16 %f32 %inval\n"
		"%outloc    = OpAccessChain %f32ptr %outdata %zero %x\n"
		"             OpStore %outloc %quant\n"
		"             OpReturn\n"
		"             OpFunctionEnd\n");

	{
		ComputeShaderSpec	spec;
		const deUint32		numElements		= 100;
		vector<float>		infinities;
		vector<float>		results;

		infinities.reserve(numElements);
		results.reserve(numElements);

		for (size_t idx = 0; idx < numElements; ++idx)
		{
			switch(idx % 4)
			{
				case 0:
					infinities.push_back(std::numeric_limits<float>::infinity());
					results.push_back(std::numeric_limits<float>::infinity());
					break;
				case 1:
					infinities.push_back(-std::numeric_limits<float>::infinity());
					results.push_back(-std::numeric_limits<float>::infinity());
					break;
				case 2:
					infinities.push_back(std::ldexp(1.0f, 16));
					results.push_back(std::numeric_limits<float>::infinity());
					break;
				case 3:
					infinities.push_back(std::ldexp(-1.0f, 32));
					results.push_back(-std::numeric_limits<float>::infinity());
					break;
			}
		}

		spec.assembly = shader;
		spec.inputs.push_back(BufferSp(new Float32Buffer(infinities)));
		spec.outputs.push_back(BufferSp(new Float32Buffer(results)));
		spec.numWorkGroups = IVec3(numElements, 1, 1);

		group->addChild(new SpvAsmComputeShaderCase(
			testCtx, "infinities", "Check that infinities propagated and created", spec));
	}

	{
		ComputeShaderSpec	spec;
		vector<float>		nans;
		const deUint32		numElements		= 100;

		nans.reserve(numElements);

		for (size_t idx = 0; idx < numElements; ++idx)
		{
			if (idx % 2 == 0)
			{
				nans.push_back(std::numeric_limits<float>::quiet_NaN());
			}
			else
			{
				nans.push_back(-std::numeric_limits<float>::quiet_NaN());
			}
		}

		spec.assembly = shader;
		spec.inputs.push_back(BufferSp(new Float32Buffer(nans)));
		spec.outputs.push_back(BufferSp(new Float32Buffer(nans)));
		spec.numWorkGroups = IVec3(numElements, 1, 1);
		spec.verifyIO = &compareNan;

		group->addChild(new SpvAsmComputeShaderCase(
			testCtx, "propagated_nans", "Check that nans are propagated", spec));
	}

	{
		ComputeShaderSpec	spec;
		vector<float>		small;
		vector<float>		zeros;
		const deUint32		numElements		= 100;

		small.reserve(numElements);
		zeros.reserve(numElements);

		for (size_t idx = 0; idx < numElements; ++idx)
		{
			switch(idx % 6)
			{
				case 0:
					small.push_back(0.f);
					zeros.push_back(0.f);
					break;
				case 1:
					small.push_back(-0.f);
					zeros.push_back(-0.f);
					break;
				case 2:
					small.push_back(std::ldexp(1.0f, -16));
					zeros.push_back(0.f);
					break;
				case 3:
					small.push_back(std::ldexp(-1.0f, -32));
					zeros.push_back(-0.f);
					break;
				case 4:
					small.push_back(std::ldexp(1.0f, -127));
					zeros.push_back(0.f);
					break;
				case 5:
					small.push_back(-std::ldexp(1.0f, -128));
					zeros.push_back(-0.f);
					break;
			}
		}

		spec.assembly = shader;
		spec.inputs.push_back(BufferSp(new Float32Buffer(small)));
		spec.outputs.push_back(BufferSp(new Float32Buffer(zeros)));
		spec.numWorkGroups = IVec3(numElements, 1, 1);

		group->addChild(new SpvAsmComputeShaderCase(
			testCtx, "flush_to_zero", "Check that values are zeroed correctly", spec));
	}

	{
		ComputeShaderSpec	spec;
		vector<float>		exact;
		const deUint32		numElements		= 200;

		exact.reserve(numElements);

		for (size_t idx = 0; idx < numElements; ++idx)
			exact.push_back(static_cast<float>(static_cast<int>(idx) - 100));

		spec.assembly = shader;
		spec.inputs.push_back(BufferSp(new Float32Buffer(exact)));
		spec.outputs.push_back(BufferSp(new Float32Buffer(exact)));
		spec.numWorkGroups = IVec3(numElements, 1, 1);

		group->addChild(new SpvAsmComputeShaderCase(
			testCtx, "exact", "Check that values exactly preserved where appropriate", spec));
	}

	{
		ComputeShaderSpec	spec;
		vector<float>		inputs;
		const deUint32		numElements		= 4;

		inputs.push_back(constructNormalizedFloat(8,	0x300300));
		inputs.push_back(-constructNormalizedFloat(-7,	0x600800));
		inputs.push_back(constructNormalizedFloat(2,	0x01E000));
		inputs.push_back(constructNormalizedFloat(1,	0xFFE000));

		spec.assembly = shader;
		spec.verifyIO = &compareOpQuantizeF16ComputeExactCase;
		spec.inputs.push_back(BufferSp(new Float32Buffer(inputs)));
		spec.outputs.push_back(BufferSp(new Float32Buffer(inputs)));
		spec.numWorkGroups = IVec3(numElements, 1, 1);

		group->addChild(new SpvAsmComputeShaderCase(
			testCtx, "rounded", "Check that are rounded when needed", spec));
	}

	return group.release();
}

// Performs a bitwise copy of source to the destination type Dest.
template <typename Dest, typename Src>
Dest bitwiseCast(Src source)
{
  Dest dest;
  DE_STATIC_ASSERT(sizeof(source) == sizeof(dest));
  deMemcpy(&dest, &source, sizeof(dest));
  return dest;
}

tcu::TestCaseGroup* createSpecConstantOpQuantizeToF16Group (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup>	group			(new tcu::TestCaseGroup(testCtx, "opspecconstantop_opquantize", "Tests the OpQuantizeToF16 opcode for the OpSpecConstantOp instruction"));

	const std::string shader (
		string(s_ShaderPreamble) +

		"OpName %main           \"main\"\n"
		"OpName %id             \"gl_GlobalInvocationID\"\n"

		"OpDecorate %id BuiltIn GlobalInvocationId\n"

		"OpDecorate %sc_0  SpecId 0\n"
		"OpDecorate %sc_1  SpecId 1\n"
		"OpDecorate %sc_2  SpecId 2\n"
		"OpDecorate %sc_3  SpecId 3\n"
		"OpDecorate %sc_4  SpecId 4\n"
		"OpDecorate %sc_5  SpecId 5\n"

		+ string(s_InputOutputBufferTraits) + string(s_CommonTypes) + string(s_InputOutputBuffer) +

		"%id        = OpVariable %uvec3ptr Input\n"
		"%zero      = OpConstant %i32 0\n"
		"%c_u32_6   = OpConstant %u32 6\n"

		"%sc_0      = OpSpecConstant %f32 0.\n"
		"%sc_1      = OpSpecConstant %f32 0.\n"
		"%sc_2      = OpSpecConstant %f32 0.\n"
		"%sc_3      = OpSpecConstant %f32 0.\n"
		"%sc_4      = OpSpecConstant %f32 0.\n"
		"%sc_5      = OpSpecConstant %f32 0.\n"

		"%sc_0_quant = OpSpecConstantOp %f32 QuantizeToF16 %sc_0\n"
		"%sc_1_quant = OpSpecConstantOp %f32 QuantizeToF16 %sc_1\n"
		"%sc_2_quant = OpSpecConstantOp %f32 QuantizeToF16 %sc_2\n"
		"%sc_3_quant = OpSpecConstantOp %f32 QuantizeToF16 %sc_3\n"
		"%sc_4_quant = OpSpecConstantOp %f32 QuantizeToF16 %sc_4\n"
		"%sc_5_quant = OpSpecConstantOp %f32 QuantizeToF16 %sc_5\n"

		"%main      = OpFunction %void None %voidf\n"
		"%label     = OpLabel\n"
		"%idval     = OpLoad %uvec3 %id\n"
		"%x         = OpCompositeExtract %u32 %idval 0\n"
		"%outloc    = OpAccessChain %f32ptr %outdata %zero %x\n"
		"%selector  = OpUMod %u32 %x %c_u32_6\n"
		"            OpSelectionMerge %exit None\n"
		"            OpSwitch %selector %exit 0 %case0 1 %case1 2 %case2 3 %case3 4 %case4 5 %case5\n"

		"%case0     = OpLabel\n"
		"             OpStore %outloc %sc_0_quant\n"
		"             OpBranch %exit\n"

		"%case1     = OpLabel\n"
		"             OpStore %outloc %sc_1_quant\n"
		"             OpBranch %exit\n"

		"%case2     = OpLabel\n"
		"             OpStore %outloc %sc_2_quant\n"
		"             OpBranch %exit\n"

		"%case3     = OpLabel\n"
		"             OpStore %outloc %sc_3_quant\n"
		"             OpBranch %exit\n"

		"%case4     = OpLabel\n"
		"             OpStore %outloc %sc_4_quant\n"
		"             OpBranch %exit\n"

		"%case5     = OpLabel\n"
		"             OpStore %outloc %sc_5_quant\n"
		"             OpBranch %exit\n"

		"%exit      = OpLabel\n"
		"             OpReturn\n"

		"             OpFunctionEnd\n");

	{
		ComputeShaderSpec	spec;
		const deUint8		numCases	= 4;
		vector<float>		inputs		(numCases, 0.f);
		vector<float>		outputs;

		spec.assembly		= shader;
		spec.numWorkGroups	= IVec3(numCases, 1, 1);

		spec.specConstants.push_back(bitwiseCast<deUint32>(std::numeric_limits<float>::infinity()));
		spec.specConstants.push_back(bitwiseCast<deUint32>(-std::numeric_limits<float>::infinity()));
		spec.specConstants.push_back(bitwiseCast<deUint32>(std::ldexp(1.0f, 16)));
		spec.specConstants.push_back(bitwiseCast<deUint32>(std::ldexp(-1.0f, 32)));

		outputs.push_back(std::numeric_limits<float>::infinity());
		outputs.push_back(-std::numeric_limits<float>::infinity());
		outputs.push_back(std::numeric_limits<float>::infinity());
		outputs.push_back(-std::numeric_limits<float>::infinity());

		spec.inputs.push_back(BufferSp(new Float32Buffer(inputs)));
		spec.outputs.push_back(BufferSp(new Float32Buffer(outputs)));

		group->addChild(new SpvAsmComputeShaderCase(
			testCtx, "infinities", "Check that infinities propagated and created", spec));
	}

	{
		ComputeShaderSpec	spec;
		const deUint8		numCases	= 2;
		vector<float>		inputs		(numCases, 0.f);
		vector<float>		outputs;

		spec.assembly		= shader;
		spec.numWorkGroups	= IVec3(numCases, 1, 1);
		spec.verifyIO		= &compareNan;

		outputs.push_back(std::numeric_limits<float>::quiet_NaN());
		outputs.push_back(-std::numeric_limits<float>::quiet_NaN());

		for (deUint8 idx = 0; idx < numCases; ++idx)
			spec.specConstants.push_back(bitwiseCast<deUint32>(outputs[idx]));

		spec.inputs.push_back(BufferSp(new Float32Buffer(inputs)));
		spec.outputs.push_back(BufferSp(new Float32Buffer(outputs)));

		group->addChild(new SpvAsmComputeShaderCase(
			testCtx, "propagated_nans", "Check that nans are propagated", spec));
	}

	{
		ComputeShaderSpec	spec;
		const deUint8		numCases	= 6;
		vector<float>		inputs		(numCases, 0.f);
		vector<float>		outputs;

		spec.assembly		= shader;
		spec.numWorkGroups	= IVec3(numCases, 1, 1);

		spec.specConstants.push_back(bitwiseCast<deUint32>(0.f));
		spec.specConstants.push_back(bitwiseCast<deUint32>(-0.f));
		spec.specConstants.push_back(bitwiseCast<deUint32>(std::ldexp(1.0f, -16)));
		spec.specConstants.push_back(bitwiseCast<deUint32>(std::ldexp(-1.0f, -32)));
		spec.specConstants.push_back(bitwiseCast<deUint32>(std::ldexp(1.0f, -127)));
		spec.specConstants.push_back(bitwiseCast<deUint32>(-std::ldexp(1.0f, -128)));

		outputs.push_back(0.f);
		outputs.push_back(-0.f);
		outputs.push_back(0.f);
		outputs.push_back(-0.f);
		outputs.push_back(0.f);
		outputs.push_back(-0.f);

		spec.inputs.push_back(BufferSp(new Float32Buffer(inputs)));
		spec.outputs.push_back(BufferSp(new Float32Buffer(outputs)));

		group->addChild(new SpvAsmComputeShaderCase(
			testCtx, "flush_to_zero", "Check that values are zeroed correctly", spec));
	}

	{
		ComputeShaderSpec	spec;
		const deUint8		numCases	= 6;
		vector<float>		inputs		(numCases, 0.f);
		vector<float>		outputs;

		spec.assembly		= shader;
		spec.numWorkGroups	= IVec3(numCases, 1, 1);

		for (deUint8 idx = 0; idx < 6; ++idx)
		{
			const float f = static_cast<float>(idx * 10 - 30) / 4.f;
			spec.specConstants.push_back(bitwiseCast<deUint32>(f));
			outputs.push_back(f);
		}

		spec.inputs.push_back(BufferSp(new Float32Buffer(inputs)));
		spec.outputs.push_back(BufferSp(new Float32Buffer(outputs)));

		group->addChild(new SpvAsmComputeShaderCase(
			testCtx, "exact", "Check that values exactly preserved where appropriate", spec));
	}

	{
		ComputeShaderSpec	spec;
		const deUint8		numCases	= 4;
		vector<float>		inputs		(numCases, 0.f);
		vector<float>		outputs;

		spec.assembly		= shader;
		spec.numWorkGroups	= IVec3(numCases, 1, 1);
		spec.verifyIO		= &compareOpQuantizeF16ComputeExactCase;

		outputs.push_back(constructNormalizedFloat(8, 0x300300));
		outputs.push_back(-constructNormalizedFloat(-7, 0x600800));
		outputs.push_back(constructNormalizedFloat(2, 0x01E000));
		outputs.push_back(constructNormalizedFloat(1, 0xFFE000));

		for (deUint8 idx = 0; idx < numCases; ++idx)
			spec.specConstants.push_back(bitwiseCast<deUint32>(outputs[idx]));

		spec.inputs.push_back(BufferSp(new Float32Buffer(inputs)));
		spec.outputs.push_back(BufferSp(new Float32Buffer(outputs)));

		group->addChild(new SpvAsmComputeShaderCase(
			testCtx, "rounded", "Check that are rounded when needed", spec));
	}

	return group.release();
}

// Checks that constant null/composite values can be used in computation.
tcu::TestCaseGroup* createOpConstantUsageGroup (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup>	group			(new tcu::TestCaseGroup(testCtx, "opconstantnullcomposite", "Spotcheck the OpConstantNull & OpConstantComposite instruction"));
	ComputeShaderSpec				spec;
	de::Random						rnd				(deStringHash(group->getName()));
	const int						numElements		= 100;
	vector<float>					positiveFloats	(numElements, 0);
	vector<float>					negativeFloats	(numElements, 0);

	fillRandomScalars(rnd, 1.f, 100.f, &positiveFloats[0], numElements);

	for (size_t ndx = 0; ndx < numElements; ++ndx)
		negativeFloats[ndx] = -positiveFloats[ndx];

	spec.assembly =
		"OpCapability Shader\n"
		"%std450 = OpExtInstImport \"GLSL.std.450\"\n"
		"OpMemoryModel Logical GLSL450\n"
		"OpEntryPoint GLCompute %main \"main\" %id\n"
		"OpExecutionMode %main LocalSize 1 1 1\n"

		"OpSource GLSL 430\n"
		"OpName %main           \"main\"\n"
		"OpName %id             \"gl_GlobalInvocationID\"\n"

		"OpDecorate %id BuiltIn GlobalInvocationId\n"

		+ string(s_InputOutputBufferTraits) + string(s_CommonTypes) +

		"%fmat      = OpTypeMatrix %fvec3 3\n"
		"%ten       = OpConstant %u32 10\n"
		"%f32arr10  = OpTypeArray %f32 %ten\n"
		"%fst       = OpTypeStruct %f32 %f32\n"

		+ string(s_InputOutputBuffer) +

		"%id        = OpVariable %uvec3ptr Input\n"
		"%zero      = OpConstant %i32 0\n"

		// Create a bunch of null values
		"%unull     = OpConstantNull %u32\n"
		"%fnull     = OpConstantNull %f32\n"
		"%vnull     = OpConstantNull %fvec3\n"
		"%mnull     = OpConstantNull %fmat\n"
		"%anull     = OpConstantNull %f32arr10\n"
		"%snull     = OpConstantComposite %fst %fnull %fnull\n"

		"%main      = OpFunction %void None %voidf\n"
		"%label     = OpLabel\n"
		"%idval     = OpLoad %uvec3 %id\n"
		"%x         = OpCompositeExtract %u32 %idval 0\n"
		"%inloc     = OpAccessChain %f32ptr %indata %zero %x\n"
		"%inval     = OpLoad %f32 %inloc\n"
		"%neg       = OpFNegate %f32 %inval\n"

		// Get the abs() of (a certain element of) those null values
		"%unull_cov = OpConvertUToF %f32 %unull\n"
		"%unull_abs = OpExtInst %f32 %std450 FAbs %unull_cov\n"
		"%fnull_abs = OpExtInst %f32 %std450 FAbs %fnull\n"
		"%vnull_0   = OpCompositeExtract %f32 %vnull 0\n"
		"%vnull_abs = OpExtInst %f32 %std450 FAbs %vnull_0\n"
		"%mnull_12  = OpCompositeExtract %f32 %mnull 1 2\n"
		"%mnull_abs = OpExtInst %f32 %std450 FAbs %mnull_12\n"
		"%anull_3   = OpCompositeExtract %f32 %anull 3\n"
		"%anull_abs = OpExtInst %f32 %std450 FAbs %anull_3\n"
		"%snull_1   = OpCompositeExtract %f32 %snull 1\n"
		"%snull_abs = OpExtInst %f32 %std450 FAbs %snull_1\n"

		// Add them all
		"%add1      = OpFAdd %f32 %neg  %unull_abs\n"
		"%add2      = OpFAdd %f32 %add1 %fnull_abs\n"
		"%add3      = OpFAdd %f32 %add2 %vnull_abs\n"
		"%add4      = OpFAdd %f32 %add3 %mnull_abs\n"
		"%add5      = OpFAdd %f32 %add4 %anull_abs\n"
		"%final     = OpFAdd %f32 %add5 %snull_abs\n"

		"%outloc    = OpAccessChain %f32ptr %outdata %zero %x\n"
		"             OpStore %outloc %final\n" // write to output
		"             OpReturn\n"
		"             OpFunctionEnd\n";
	spec.inputs.push_back(BufferSp(new Float32Buffer(positiveFloats)));
	spec.outputs.push_back(BufferSp(new Float32Buffer(negativeFloats)));
	spec.numWorkGroups = IVec3(numElements, 1, 1);

	group->addChild(new SpvAsmComputeShaderCase(testCtx, "spotcheck", "Check that values constructed via OpConstantNull & OpConstantComposite can be used", spec));

	return group.release();
}

// Assembly code used for testing loop control is based on GLSL source code:
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
//   uint x = gl_GlobalInvocationID.x;
//   output_data.elements[x] = input_data.elements[x];
//   for (uint i = 0; i < 4; ++i)
//     output_data.elements[x] += 1.f;
// }
tcu::TestCaseGroup* createLoopControlGroup (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup>	group			(new tcu::TestCaseGroup(testCtx, "loop_control", "Tests loop control cases"));
	vector<CaseParameter>			cases;
	de::Random						rnd				(deStringHash(group->getName()));
	const int						numElements		= 100;
	vector<float>					inputFloats		(numElements, 0);
	vector<float>					outputFloats	(numElements, 0);
	const StringTemplate			shaderTemplate	(
		string(s_ShaderPreamble) +

		"OpSource GLSL 430\n"
		"OpName %main \"main\"\n"
		"OpName %id \"gl_GlobalInvocationID\"\n"

		"OpDecorate %id BuiltIn GlobalInvocationId\n"

		+ string(s_InputOutputBufferTraits) + string(s_CommonTypes) + string(s_InputOutputBuffer) +

		"%u32ptr      = OpTypePointer Function %u32\n"

		"%id          = OpVariable %uvec3ptr Input\n"
		"%zero        = OpConstant %i32 0\n"
		"%uzero       = OpConstant %u32 0\n"
		"%one         = OpConstant %i32 1\n"
		"%constf1     = OpConstant %f32 1.0\n"
		"%four        = OpConstant %u32 4\n"

		"%main        = OpFunction %void None %voidf\n"
		"%entry       = OpLabel\n"
		"%i           = OpVariable %u32ptr Function\n"
		"               OpStore %i %uzero\n"

		"%idval       = OpLoad %uvec3 %id\n"
		"%x           = OpCompositeExtract %u32 %idval 0\n"
		"%inloc       = OpAccessChain %f32ptr %indata %zero %x\n"
		"%inval       = OpLoad %f32 %inloc\n"
		"%outloc      = OpAccessChain %f32ptr %outdata %zero %x\n"
		"               OpStore %outloc %inval\n"
		"               OpBranch %loop_entry\n"

		"%loop_entry  = OpLabel\n"
		"%i_val       = OpLoad %u32 %i\n"
		"%cmp_lt      = OpULessThan %bool %i_val %four\n"
		"               OpLoopMerge %loop_merge %loop_entry ${CONTROL}\n"
		"               OpBranchConditional %cmp_lt %loop_body %loop_merge\n"
		"%loop_body   = OpLabel\n"
		"%outval      = OpLoad %f32 %outloc\n"
		"%addf1       = OpFAdd %f32 %outval %constf1\n"
		"               OpStore %outloc %addf1\n"
		"%new_i       = OpIAdd %u32 %i_val %one\n"
		"               OpStore %i %new_i\n"
		"               OpBranch %loop_entry\n"
		"%loop_merge  = OpLabel\n"
		"               OpReturn\n"
		"               OpFunctionEnd\n");

	cases.push_back(CaseParameter("none",				"None"));
	cases.push_back(CaseParameter("unroll",				"Unroll"));
	cases.push_back(CaseParameter("dont_unroll",		"DontUnroll"));
	cases.push_back(CaseParameter("unroll_dont_unroll",	"Unroll|DontUnroll"));

	fillRandomScalars(rnd, -100.f, 100.f, &inputFloats[0], numElements);

	for (size_t ndx = 0; ndx < numElements; ++ndx)
		outputFloats[ndx] = inputFloats[ndx] + 4.f;

	for (size_t caseNdx = 0; caseNdx < cases.size(); ++caseNdx)
	{
		map<string, string>		specializations;
		ComputeShaderSpec		spec;

		specializations["CONTROL"] = cases[caseNdx].param;
		spec.assembly = shaderTemplate.specialize(specializations);
		spec.inputs.push_back(BufferSp(new Float32Buffer(inputFloats)));
		spec.outputs.push_back(BufferSp(new Float32Buffer(outputFloats)));
		spec.numWorkGroups = IVec3(numElements, 1, 1);

		group->addChild(new SpvAsmComputeShaderCase(testCtx, cases[caseNdx].name, cases[caseNdx].name, spec));
	}

	return group.release();
}

// Assembly code used for testing selection control is based on GLSL source code:
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
//   uint x = gl_GlobalInvocationID.x;
//   float val = input_data.elements[x];
//   if (val > 10.f)
//     output_data.elements[x] = val + 1.f;
//   else
//     output_data.elements[x] = val - 1.f;
// }
tcu::TestCaseGroup* createSelectionControlGroup (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup>	group			(new tcu::TestCaseGroup(testCtx, "selection_control", "Tests selection control cases"));
	vector<CaseParameter>			cases;
	de::Random						rnd				(deStringHash(group->getName()));
	const int						numElements		= 100;
	vector<float>					inputFloats		(numElements, 0);
	vector<float>					outputFloats	(numElements, 0);
	const StringTemplate			shaderTemplate	(
		string(s_ShaderPreamble) +

		"OpSource GLSL 430\n"
		"OpName %main \"main\"\n"
		"OpName %id \"gl_GlobalInvocationID\"\n"

		"OpDecorate %id BuiltIn GlobalInvocationId\n"

		+ string(s_InputOutputBufferTraits) + string(s_CommonTypes) + string(s_InputOutputBuffer) +

		"%id       = OpVariable %uvec3ptr Input\n"
		"%zero     = OpConstant %i32 0\n"
		"%constf1  = OpConstant %f32 1.0\n"
		"%constf10 = OpConstant %f32 10.0\n"

		"%main     = OpFunction %void None %voidf\n"
		"%entry    = OpLabel\n"
		"%idval    = OpLoad %uvec3 %id\n"
		"%x        = OpCompositeExtract %u32 %idval 0\n"
		"%inloc    = OpAccessChain %f32ptr %indata %zero %x\n"
		"%inval    = OpLoad %f32 %inloc\n"
		"%outloc   = OpAccessChain %f32ptr %outdata %zero %x\n"
		"%cmp_gt   = OpFOrdGreaterThan %bool %inval %constf10\n"

		"            OpSelectionMerge %if_end ${CONTROL}\n"
		"            OpBranchConditional %cmp_gt %if_true %if_false\n"
		"%if_true  = OpLabel\n"
		"%addf1    = OpFAdd %f32 %inval %constf1\n"
		"            OpStore %outloc %addf1\n"
		"            OpBranch %if_end\n"
		"%if_false = OpLabel\n"
		"%subf1    = OpFSub %f32 %inval %constf1\n"
		"            OpStore %outloc %subf1\n"
		"            OpBranch %if_end\n"
		"%if_end   = OpLabel\n"
		"            OpReturn\n"
		"            OpFunctionEnd\n");

	cases.push_back(CaseParameter("none",					"None"));
	cases.push_back(CaseParameter("flatten",				"Flatten"));
	cases.push_back(CaseParameter("dont_flatten",			"DontFlatten"));
	cases.push_back(CaseParameter("flatten_dont_flatten",	"DontFlatten|Flatten"));

	fillRandomScalars(rnd, -100.f, 100.f, &inputFloats[0], numElements);

	// CPU might not use the same rounding mode as the GPU. Use whole numbers to avoid rounding differences.
	floorAll(inputFloats);

	for (size_t ndx = 0; ndx < numElements; ++ndx)
		outputFloats[ndx] = inputFloats[ndx] + (inputFloats[ndx] > 10.f ? 1.f : -1.f);

	for (size_t caseNdx = 0; caseNdx < cases.size(); ++caseNdx)
	{
		map<string, string>		specializations;
		ComputeShaderSpec		spec;

		specializations["CONTROL"] = cases[caseNdx].param;
		spec.assembly = shaderTemplate.specialize(specializations);
		spec.inputs.push_back(BufferSp(new Float32Buffer(inputFloats)));
		spec.outputs.push_back(BufferSp(new Float32Buffer(outputFloats)));
		spec.numWorkGroups = IVec3(numElements, 1, 1);

		group->addChild(new SpvAsmComputeShaderCase(testCtx, cases[caseNdx].name, cases[caseNdx].name, spec));
	}

	return group.release();
}

// Assembly code used for testing function control is based on GLSL source code:
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
// float const10() { return 10.f; }
//
// void main() {
//   uint x = gl_GlobalInvocationID.x;
//   output_data.elements[x] = input_data.elements[x] + const10();
// }
tcu::TestCaseGroup* createFunctionControlGroup (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup>	group			(new tcu::TestCaseGroup(testCtx, "function_control", "Tests function control cases"));
	vector<CaseParameter>			cases;
	de::Random						rnd				(deStringHash(group->getName()));
	const int						numElements		= 100;
	vector<float>					inputFloats		(numElements, 0);
	vector<float>					outputFloats	(numElements, 0);
	const StringTemplate			shaderTemplate	(
		string(s_ShaderPreamble) +

		"OpSource GLSL 430\n"
		"OpName %main \"main\"\n"
		"OpName %func_const10 \"const10(\"\n"
		"OpName %id \"gl_GlobalInvocationID\"\n"

		"OpDecorate %id BuiltIn GlobalInvocationId\n"

		+ string(s_InputOutputBufferTraits) + string(s_CommonTypes) + string(s_InputOutputBuffer) +

		"%f32f = OpTypeFunction %f32\n"
		"%id = OpVariable %uvec3ptr Input\n"
		"%zero = OpConstant %i32 0\n"
		"%constf10 = OpConstant %f32 10.0\n"

		"%main         = OpFunction %void None %voidf\n"
		"%entry        = OpLabel\n"
		"%idval        = OpLoad %uvec3 %id\n"
		"%x            = OpCompositeExtract %u32 %idval 0\n"
		"%inloc        = OpAccessChain %f32ptr %indata %zero %x\n"
		"%inval        = OpLoad %f32 %inloc\n"
		"%ret_10       = OpFunctionCall %f32 %func_const10\n"
		"%fadd         = OpFAdd %f32 %inval %ret_10\n"
		"%outloc       = OpAccessChain %f32ptr %outdata %zero %x\n"
		"                OpStore %outloc %fadd\n"
		"                OpReturn\n"
		"                OpFunctionEnd\n"

		"%func_const10 = OpFunction %f32 ${CONTROL} %f32f\n"
		"%label        = OpLabel\n"
		"                OpReturnValue %constf10\n"
		"                OpFunctionEnd\n");

	cases.push_back(CaseParameter("none",						"None"));
	cases.push_back(CaseParameter("inline",						"Inline"));
	cases.push_back(CaseParameter("dont_inline",				"DontInline"));
	cases.push_back(CaseParameter("pure",						"Pure"));
	cases.push_back(CaseParameter("const",						"Const"));
	cases.push_back(CaseParameter("inline_pure",				"Inline|Pure"));
	cases.push_back(CaseParameter("const_dont_inline",			"Const|DontInline"));
	cases.push_back(CaseParameter("inline_dont_inline",			"Inline|DontInline"));
	cases.push_back(CaseParameter("pure_inline_dont_inline",	"Pure|Inline|DontInline"));

	fillRandomScalars(rnd, -100.f, 100.f, &inputFloats[0], numElements);

	// CPU might not use the same rounding mode as the GPU. Use whole numbers to avoid rounding differences.
	floorAll(inputFloats);

	for (size_t ndx = 0; ndx < numElements; ++ndx)
		outputFloats[ndx] = inputFloats[ndx] + 10.f;

	for (size_t caseNdx = 0; caseNdx < cases.size(); ++caseNdx)
	{
		map<string, string>		specializations;
		ComputeShaderSpec		spec;

		specializations["CONTROL"] = cases[caseNdx].param;
		spec.assembly = shaderTemplate.specialize(specializations);
		spec.inputs.push_back(BufferSp(new Float32Buffer(inputFloats)));
		spec.outputs.push_back(BufferSp(new Float32Buffer(outputFloats)));
		spec.numWorkGroups = IVec3(numElements, 1, 1);

		group->addChild(new SpvAsmComputeShaderCase(testCtx, cases[caseNdx].name, cases[caseNdx].name, spec));
	}

	return group.release();
}

tcu::TestCaseGroup* createMemoryAccessGroup (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup>	group			(new tcu::TestCaseGroup(testCtx, "memory_access", "Tests memory access cases"));
	vector<CaseParameter>			cases;
	de::Random						rnd				(deStringHash(group->getName()));
	const int						numElements		= 100;
	vector<float>					inputFloats		(numElements, 0);
	vector<float>					outputFloats	(numElements, 0);
	const StringTemplate			shaderTemplate	(
		string(s_ShaderPreamble) +

		"OpSource GLSL 430\n"
		"OpName %main           \"main\"\n"
		"OpName %id             \"gl_GlobalInvocationID\"\n"

		"OpDecorate %id BuiltIn GlobalInvocationId\n"

		+ string(s_InputOutputBufferTraits) + string(s_CommonTypes) + string(s_InputOutputBuffer) +

		"%f32ptr_f  = OpTypePointer Function %f32\n"

		"%id        = OpVariable %uvec3ptr Input\n"
		"%zero      = OpConstant %i32 0\n"
		"%four      = OpConstant %i32 4\n"

		"%main      = OpFunction %void None %voidf\n"
		"%label     = OpLabel\n"
		"%copy      = OpVariable %f32ptr_f Function\n"
		"%idval     = OpLoad %uvec3 %id ${ACCESS}\n"
		"%x         = OpCompositeExtract %u32 %idval 0\n"
		"%inloc     = OpAccessChain %f32ptr %indata  %zero %x\n"
		"%outloc    = OpAccessChain %f32ptr %outdata %zero %x\n"
		"             OpCopyMemory %copy %inloc ${ACCESS}\n"
		"%val1      = OpLoad %f32 %copy\n"
		"%val2      = OpLoad %f32 %inloc\n"
		"%add       = OpFAdd %f32 %val1 %val2\n"
		"             OpStore %outloc %add ${ACCESS}\n"
		"             OpReturn\n"
		"             OpFunctionEnd\n");

	cases.push_back(CaseParameter("null",					""));
	cases.push_back(CaseParameter("none",					"None"));
	cases.push_back(CaseParameter("volatile",				"Volatile"));
	cases.push_back(CaseParameter("aligned",				"Aligned 4"));
	cases.push_back(CaseParameter("nontemporal",			"Nontemporal"));
	cases.push_back(CaseParameter("aligned_nontemporal",	"Aligned|Nontemporal 4"));
	cases.push_back(CaseParameter("aligned_volatile",		"Volatile|Aligned 4"));

	fillRandomScalars(rnd, -100.f, 100.f, &inputFloats[0], numElements);

	for (size_t ndx = 0; ndx < numElements; ++ndx)
		outputFloats[ndx] = inputFloats[ndx] + inputFloats[ndx];

	for (size_t caseNdx = 0; caseNdx < cases.size(); ++caseNdx)
	{
		map<string, string>		specializations;
		ComputeShaderSpec		spec;

		specializations["ACCESS"] = cases[caseNdx].param;
		spec.assembly = shaderTemplate.specialize(specializations);
		spec.inputs.push_back(BufferSp(new Float32Buffer(inputFloats)));
		spec.outputs.push_back(BufferSp(new Float32Buffer(outputFloats)));
		spec.numWorkGroups = IVec3(numElements, 1, 1);

		group->addChild(new SpvAsmComputeShaderCase(testCtx, cases[caseNdx].name, cases[caseNdx].name, spec));
	}

	return group.release();
}

// Checks that we can get undefined values for various types, without exercising a computation with it.
tcu::TestCaseGroup* createOpUndefGroup (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup>	group			(new tcu::TestCaseGroup(testCtx, "opundef", "Tests the OpUndef instruction"));
	vector<CaseParameter>			cases;
	de::Random						rnd				(deStringHash(group->getName()));
	const int						numElements		= 100;
	vector<float>					positiveFloats	(numElements, 0);
	vector<float>					negativeFloats	(numElements, 0);
	const StringTemplate			shaderTemplate	(
		string(s_ShaderPreamble) +

		"OpSource GLSL 430\n"
		"OpName %main           \"main\"\n"
		"OpName %id             \"gl_GlobalInvocationID\"\n"

		"OpDecorate %id BuiltIn GlobalInvocationId\n"

		+ string(s_InputOutputBufferTraits) + string(s_CommonTypes) + string(s_InputOutputBuffer) +

		"${TYPE}\n"

		"%id        = OpVariable %uvec3ptr Input\n"
		"%zero      = OpConstant %i32 0\n"

		"%main      = OpFunction %void None %voidf\n"
		"%label     = OpLabel\n"

		"%undef     = OpUndef %type\n"

		"%idval     = OpLoad %uvec3 %id\n"
		"%x         = OpCompositeExtract %u32 %idval 0\n"

		"%inloc     = OpAccessChain %f32ptr %indata %zero %x\n"
		"%inval     = OpLoad %f32 %inloc\n"
		"%neg       = OpFNegate %f32 %inval\n"
		"%outloc    = OpAccessChain %f32ptr %outdata %zero %x\n"
		"             OpStore %outloc %neg\n"
		"             OpReturn\n"
		"             OpFunctionEnd\n");

	cases.push_back(CaseParameter("bool",			"%type = OpTypeBool"));
	cases.push_back(CaseParameter("sint32",			"%type = OpTypeInt 32 1"));
	cases.push_back(CaseParameter("uint32",			"%type = OpTypeInt 32 0"));
	cases.push_back(CaseParameter("float32",		"%type = OpTypeFloat 32"));
	cases.push_back(CaseParameter("vec4float32",	"%type = OpTypeVector %f32 4"));
	cases.push_back(CaseParameter("vec2uint32",		"%type = OpTypeVector %u32 2"));
	cases.push_back(CaseParameter("matrix",			"%type = OpTypeMatrix %fvec3 3"));
	cases.push_back(CaseParameter("image",			"%type = OpTypeImage %f32 2D 0 0 0 1 Unknown"));
	cases.push_back(CaseParameter("sampler",		"%type = OpTypeSampler"));
	cases.push_back(CaseParameter("sampledimage",	"%img = OpTypeImage %f32 2D 0 0 0 1 Unknown\n"
													"%type = OpTypeSampledImage %img"));
	cases.push_back(CaseParameter("array",			"%100 = OpConstant %u32 100\n"
													"%type = OpTypeArray %i32 %100"));
	cases.push_back(CaseParameter("runtimearray",	"%type = OpTypeRuntimeArray %f32"));
	cases.push_back(CaseParameter("struct",			"%type = OpTypeStruct %f32 %i32 %u32"));
	cases.push_back(CaseParameter("pointer",		"%type = OpTypePointer Function %i32"));

	fillRandomScalars(rnd, 1.f, 100.f, &positiveFloats[0], numElements);

	for (size_t ndx = 0; ndx < numElements; ++ndx)
		negativeFloats[ndx] = -positiveFloats[ndx];

	for (size_t caseNdx = 0; caseNdx < cases.size(); ++caseNdx)
	{
		map<string, string>		specializations;
		ComputeShaderSpec		spec;

		specializations["TYPE"] = cases[caseNdx].param;
		spec.assembly = shaderTemplate.specialize(specializations);
		spec.inputs.push_back(BufferSp(new Float32Buffer(positiveFloats)));
		spec.outputs.push_back(BufferSp(new Float32Buffer(negativeFloats)));
		spec.numWorkGroups = IVec3(numElements, 1, 1);

		group->addChild(new SpvAsmComputeShaderCase(testCtx, cases[caseNdx].name, cases[caseNdx].name, spec));
	}

		return group.release();
}
typedef std::pair<std::string, VkShaderStageFlagBits>	EntryToStage;
typedef map<string, vector<EntryToStage> >				ModuleMap;
typedef map<VkShaderStageFlagBits, vector<deInt32> >	StageToSpecConstantMap;

// Context for a specific test instantiation. For example, an instantiation
// may test colors yellow/magenta/cyan/mauve in a tesselation shader
// with an entry point named 'main_to_the_main'
struct InstanceContext
{
	// Map of modules to what entry_points we care to use from those modules.
	ModuleMap				moduleMap;
	RGBA					inputColors[4];
	RGBA					outputColors[4];
	// Concrete SPIR-V code to test via boilerplate specialization.
	map<string, string>		testCodeFragments;
	StageToSpecConstantMap	specConstants;
	bool					hasTessellation;
	VkShaderStageFlagBits	requiredStages;

	InstanceContext (const RGBA (&inputs)[4], const RGBA (&outputs)[4], const map<string, string>& testCodeFragments_, const StageToSpecConstantMap& specConstants_)
		: testCodeFragments		(testCodeFragments_)
		, specConstants			(specConstants_)
		, hasTessellation		(false)
		, requiredStages		(static_cast<VkShaderStageFlagBits>(0))
	{
		inputColors[0]		= inputs[0];
		inputColors[1]		= inputs[1];
		inputColors[2]		= inputs[2];
		inputColors[3]		= inputs[3];

		outputColors[0]		= outputs[0];
		outputColors[1]		= outputs[1];
		outputColors[2]		= outputs[2];
		outputColors[3]		= outputs[3];
	}

	InstanceContext (const InstanceContext& other)
		: moduleMap			(other.moduleMap)
		, testCodeFragments	(other.testCodeFragments)
		, specConstants		(other.specConstants)
		, hasTessellation	(other.hasTessellation)
		, requiredStages    (other.requiredStages)
	{
		inputColors[0]		= other.inputColors[0];
		inputColors[1]		= other.inputColors[1];
		inputColors[2]		= other.inputColors[2];
		inputColors[3]		= other.inputColors[3];

		outputColors[0]		= other.outputColors[0];
		outputColors[1]		= other.outputColors[1];
		outputColors[2]		= other.outputColors[2];
		outputColors[3]		= other.outputColors[3];
	}
};

// A description of a shader to be used for a single stage of the graphics pipeline.
struct ShaderElement
{
	// The module that contains this shader entrypoint.
	string					moduleName;

	// The name of the entrypoint.
	string					entryName;

	// Which shader stage this entry point represents.
	VkShaderStageFlagBits	stage;

	ShaderElement (const string& moduleName_, const string& entryPoint_, VkShaderStageFlagBits shaderStage_)
		: moduleName(moduleName_)
		, entryName(entryPoint_)
		, stage(shaderStage_)
	{
	}
};

void getDefaultColors (RGBA (&colors)[4])
{
	colors[0] = RGBA::white();
	colors[1] = RGBA::red();
	colors[2] = RGBA::green();
	colors[3] = RGBA::blue();
}

void getHalfColorsFullAlpha (RGBA (&colors)[4])
{
	colors[0] = RGBA(127, 127, 127, 255);
	colors[1] = RGBA(127, 0,   0,	255);
	colors[2] = RGBA(0,	  127, 0,	255);
	colors[3] = RGBA(0,	  0,   127, 255);
}

void getInvertedDefaultColors (RGBA (&colors)[4])
{
	colors[0] = RGBA(0,		0,		0,		255);
	colors[1] = RGBA(0,		255,	255,	255);
	colors[2] = RGBA(255,	0,		255,	255);
	colors[3] = RGBA(255,	255,	0,		255);
}

// Turns a statically sized array of ShaderElements into an instance-context
// by setting up the mapping of modules to their contained shaders and stages.
// The inputs and expected outputs are given by inputColors and outputColors
template<size_t N>
InstanceContext createInstanceContext (const ShaderElement (&elements)[N], const RGBA (&inputColors)[4], const RGBA (&outputColors)[4], const map<string, string>& testCodeFragments, const StageToSpecConstantMap& specConstants)
{
	InstanceContext ctx (inputColors, outputColors, testCodeFragments, specConstants);
	for (size_t i = 0; i < N; ++i)
	{
		ctx.moduleMap[elements[i].moduleName].push_back(std::make_pair(elements[i].entryName, elements[i].stage));
		ctx.requiredStages = static_cast<VkShaderStageFlagBits>(ctx.requiredStages | elements[i].stage);
	}
	return ctx;
}

template<size_t N>
inline InstanceContext createInstanceContext (const ShaderElement (&elements)[N], RGBA (&inputColors)[4], const RGBA (&outputColors)[4], const map<string, string>& testCodeFragments)
{
	return createInstanceContext(elements, inputColors, outputColors, testCodeFragments, StageToSpecConstantMap());
}

// The same as createInstanceContext above, but with default colors.
template<size_t N>
InstanceContext createInstanceContext (const ShaderElement (&elements)[N], const map<string, string>& testCodeFragments)
{
	RGBA defaultColors[4];
	getDefaultColors(defaultColors);
	return createInstanceContext(elements, defaultColors, defaultColors, testCodeFragments);
}

// For the current InstanceContext, constructs the required modules and shader stage create infos.
void createPipelineShaderStages (const DeviceInterface& vk, const VkDevice vkDevice, InstanceContext& instance, Context& context, vector<ModuleHandleSp>& modules, vector<VkPipelineShaderStageCreateInfo>& createInfos)
{
	for (ModuleMap::const_iterator moduleNdx = instance.moduleMap.begin(); moduleNdx != instance.moduleMap.end(); ++moduleNdx)
	{
		const ModuleHandleSp mod(new Unique<VkShaderModule>(createShaderModule(vk, vkDevice, context.getBinaryCollection().get(moduleNdx->first), 0)));
		modules.push_back(ModuleHandleSp(mod));
		for (vector<EntryToStage>::const_iterator shaderNdx = moduleNdx->second.begin(); shaderNdx != moduleNdx->second.end(); ++shaderNdx)
		{
			const EntryToStage&						stage			= *shaderNdx;
			const VkPipelineShaderStageCreateInfo	shaderParam		=
			{
				VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,	//	VkStructureType			sType;
				DE_NULL,												//	const void*				pNext;
				(VkPipelineShaderStageCreateFlags)0,
				stage.second,											//	VkShaderStageFlagBits	stage;
				**modules.back(),										//	VkShaderModule			module;
				stage.first.c_str(),									//	const char*				pName;
				(const VkSpecializationInfo*)DE_NULL,
			};
			createInfos.push_back(shaderParam);
		}
	}
}

#define SPIRV_ASSEMBLY_TYPES																	\
	"%void = OpTypeVoid\n"																		\
	"%bool = OpTypeBool\n"																		\
																								\
	"%i32 = OpTypeInt 32 1\n"																	\
	"%u32 = OpTypeInt 32 0\n"																	\
																								\
	"%f32 = OpTypeFloat 32\n"																	\
	"%v3f32 = OpTypeVector %f32 3\n"															\
	"%v4f32 = OpTypeVector %f32 4\n"															\
	"%v4bool = OpTypeVector %bool 4\n"															\
																								\
	"%v4f32_function = OpTypeFunction %v4f32 %v4f32\n"											\
	"%fun = OpTypeFunction %void\n"																\
																								\
	"%ip_f32 = OpTypePointer Input %f32\n"														\
	"%ip_i32 = OpTypePointer Input %i32\n"														\
	"%ip_v3f32 = OpTypePointer Input %v3f32\n"													\
	"%ip_v4f32 = OpTypePointer Input %v4f32\n"													\
																								\
	"%op_f32 = OpTypePointer Output %f32\n"														\
	"%op_v4f32 = OpTypePointer Output %v4f32\n"													\
																								\
	"%fp_f32   = OpTypePointer Function %f32\n"													\
	"%fp_i32   = OpTypePointer Function %i32\n"													\
	"%fp_v4f32 = OpTypePointer Function %v4f32\n"

#define SPIRV_ASSEMBLY_CONSTANTS																\
	"%c_f32_1 = OpConstant %f32 1.0\n"															\
	"%c_f32_0 = OpConstant %f32 0.0\n"															\
	"%c_f32_0_5 = OpConstant %f32 0.5\n"														\
	"%c_f32_n1  = OpConstant %f32 -1.\n"														\
	"%c_f32_7 = OpConstant %f32 7.0\n"															\
	"%c_f32_8 = OpConstant %f32 8.0\n"															\
	"%c_i32_0 = OpConstant %i32 0\n"															\
	"%c_i32_1 = OpConstant %i32 1\n"															\
	"%c_i32_2 = OpConstant %i32 2\n"															\
	"%c_i32_3 = OpConstant %i32 3\n"															\
	"%c_i32_4 = OpConstant %i32 4\n"															\
	"%c_u32_0 = OpConstant %u32 0\n"															\
	"%c_u32_1 = OpConstant %u32 1\n"															\
	"%c_u32_2 = OpConstant %u32 2\n"															\
	"%c_u32_3 = OpConstant %u32 3\n"															\
	"%c_u32_32 = OpConstant %u32 32\n"															\
	"%c_u32_4 = OpConstant %u32 4\n"															\
	"%c_u32_31_bits = OpConstant %u32 0x7FFFFFFF\n"												\
	"%c_v4f32_1_1_1_1 = OpConstantComposite %v4f32 %c_f32_1 %c_f32_1 %c_f32_1 %c_f32_1\n"		\
	"%c_v4f32_1_0_0_1 = OpConstantComposite %v4f32 %c_f32_1 %c_f32_0 %c_f32_0 %c_f32_1\n"		\
	"%c_v4f32_0_5_0_5_0_5_0_5 = OpConstantComposite %v4f32 %c_f32_0_5 %c_f32_0_5 %c_f32_0_5 %c_f32_0_5\n"

#define SPIRV_ASSEMBLY_ARRAYS																	\
	"%a1f32 = OpTypeArray %f32 %c_u32_1\n"														\
	"%a2f32 = OpTypeArray %f32 %c_u32_2\n"														\
	"%a3v4f32 = OpTypeArray %v4f32 %c_u32_3\n"													\
	"%a4f32 = OpTypeArray %f32 %c_u32_4\n"														\
	"%a32v4f32 = OpTypeArray %v4f32 %c_u32_32\n"												\
	"%ip_a3v4f32 = OpTypePointer Input %a3v4f32\n"												\
	"%ip_a32v4f32 = OpTypePointer Input %a32v4f32\n"											\
	"%op_a2f32 = OpTypePointer Output %a2f32\n"													\
	"%op_a3v4f32 = OpTypePointer Output %a3v4f32\n"												\
	"%op_a4f32 = OpTypePointer Output %a4f32\n"

// Creates vertex-shader assembly by specializing a boilerplate StringTemplate
// on fragments, which must (at least) map "testfun" to an OpFunction definition
// for %test_code that takes and returns a %v4f32.  Boilerplate IDs are prefixed
// with "BP_" to avoid collisions with fragments.
//
// It corresponds roughly to this GLSL:
//;
// layout(location = 0) in vec4 position;
// layout(location = 1) in vec4 color;
// layout(location = 1) out highp vec4 vtxColor;
// void main (void) { gl_Position = position; vtxColor = test_func(color); }
string makeVertexShaderAssembly(const map<string, string>& fragments)
{
// \todo [2015-11-23 awoloszyn] Remove OpName once these have stabalized
	static const char vertexShaderBoilerplate[] =
		"OpCapability Shader\n"
		"OpCapability ClipDistance\n"
		"OpCapability CullDistance\n"
		"OpMemoryModel Logical GLSL450\n"
		"OpEntryPoint Vertex %main \"main\" %BP_stream %BP_position %BP_vtx_color %BP_color %BP_gl_VertexIndex %BP_gl_InstanceIndex\n"
		"${debug:opt}\n"
		"OpName %main \"main\"\n"
		"OpName %BP_gl_PerVertex \"gl_PerVertex\"\n"
		"OpMemberName %BP_gl_PerVertex 0 \"gl_Position\"\n"
		"OpMemberName %BP_gl_PerVertex 1 \"gl_PointSize\"\n"
		"OpMemberName %BP_gl_PerVertex 2 \"gl_ClipDistance\"\n"
		"OpMemberName %BP_gl_PerVertex 3 \"gl_CullDistance\"\n"
		"OpName %test_code \"testfun(vf4;\"\n"
		"OpName %BP_stream \"\"\n"
		"OpName %BP_position \"position\"\n"
		"OpName %BP_vtx_color \"vtxColor\"\n"
		"OpName %BP_color \"color\"\n"
		"OpName %BP_gl_VertexIndex \"gl_VertexIndex\"\n"
		"OpName %BP_gl_InstanceIndex \"gl_InstanceIndex\"\n"
		"OpMemberDecorate %BP_gl_PerVertex 0 BuiltIn Position\n"
		"OpMemberDecorate %BP_gl_PerVertex 1 BuiltIn PointSize\n"
		"OpMemberDecorate %BP_gl_PerVertex 2 BuiltIn ClipDistance\n"
		"OpMemberDecorate %BP_gl_PerVertex 3 BuiltIn CullDistance\n"
		"OpDecorate %BP_gl_PerVertex Block\n"
		"OpDecorate %BP_position Location 0\n"
		"OpDecorate %BP_vtx_color Location 1\n"
		"OpDecorate %BP_color Location 1\n"
		"OpDecorate %BP_gl_VertexIndex BuiltIn VertexIndex\n"
		"OpDecorate %BP_gl_InstanceIndex BuiltIn InstanceIndex\n"
		"${decoration:opt}\n"
		SPIRV_ASSEMBLY_TYPES
		SPIRV_ASSEMBLY_CONSTANTS
		SPIRV_ASSEMBLY_ARRAYS
		"%BP_gl_PerVertex = OpTypeStruct %v4f32 %f32 %a1f32 %a1f32\n"
		"%BP_op_gl_PerVertex = OpTypePointer Output %BP_gl_PerVertex\n"
		"%BP_stream = OpVariable %BP_op_gl_PerVertex Output\n"
		"%BP_position = OpVariable %ip_v4f32 Input\n"
		"%BP_vtx_color = OpVariable %op_v4f32 Output\n"
		"%BP_color = OpVariable %ip_v4f32 Input\n"
		"%BP_gl_VertexIndex = OpVariable %ip_i32 Input\n"
		"%BP_gl_InstanceIndex = OpVariable %ip_i32 Input\n"
		"${pre_main:opt}\n"
		"%main = OpFunction %void None %fun\n"
		"%BP_label = OpLabel\n"
		"%BP_pos = OpLoad %v4f32 %BP_position\n"
		"%BP_gl_pos = OpAccessChain %op_v4f32 %BP_stream %c_i32_0\n"
		"OpStore %BP_gl_pos %BP_pos\n"
		"%BP_col = OpLoad %v4f32 %BP_color\n"
		"%BP_col_transformed = OpFunctionCall %v4f32 %test_code %BP_col\n"
		"OpStore %BP_vtx_color %BP_col_transformed\n"
		"OpReturn\n"
		"OpFunctionEnd\n"
		"${testfun}\n";
	return tcu::StringTemplate(vertexShaderBoilerplate).specialize(fragments);
}

// Creates tess-control-shader assembly by specializing a boilerplate
// StringTemplate on fragments, which must (at least) map "testfun" to an
// OpFunction definition for %test_code that takes and returns a %v4f32.
// Boilerplate IDs are prefixed with "BP_" to avoid collisions with fragments.
//
// It roughly corresponds to the following GLSL.
//
// #version 450
// layout(vertices = 3) out;
// layout(location = 1) in vec4 in_color[];
// layout(location = 1) out vec4 out_color[];
//
// void main() {
//   out_color[gl_InvocationID] = testfun(in_color[gl_InvocationID]);
//   gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;
//   if (gl_InvocationID == 0) {
//     gl_TessLevelOuter[0] = 1.0;
//     gl_TessLevelOuter[1] = 1.0;
//     gl_TessLevelOuter[2] = 1.0;
//     gl_TessLevelInner[0] = 1.0;
//   }
// }
string makeTessControlShaderAssembly (const map<string, string>& fragments)
{
	static const char tessControlShaderBoilerplate[] =
		"OpCapability Tessellation\n"
		"OpCapability ClipDistance\n"
		"OpCapability CullDistance\n"
		"OpMemoryModel Logical GLSL450\n"
		"OpEntryPoint TessellationControl %BP_main \"main\" %BP_out_color %BP_gl_InvocationID %BP_in_color %BP_gl_out %BP_gl_in %BP_gl_TessLevelOuter %BP_gl_TessLevelInner\n"
		"OpExecutionMode %BP_main OutputVertices 3\n"
		"${debug:opt}\n"
		"OpName %BP_main \"main\"\n"
		"OpName %test_code \"testfun(vf4;\"\n"
		"OpName %BP_out_color \"out_color\"\n"
		"OpName %BP_gl_InvocationID \"gl_InvocationID\"\n"
		"OpName %BP_in_color \"in_color\"\n"
		"OpName %BP_gl_PerVertex \"gl_PerVertex\"\n"
		"OpMemberName %BP_gl_PerVertex 0 \"gl_Position\"\n"
		"OpMemberName %BP_gl_PerVertex 1 \"gl_PointSize\"\n"
		"OpMemberName %BP_gl_PerVertex 2 \"gl_ClipDistance\"\n"
		"OpMemberName %BP_gl_PerVertex 3 \"gl_CullDistance\"\n"
		"OpName %BP_gl_out \"gl_out\"\n"
		"OpName %BP_gl_PVOut \"gl_PerVertex\"\n"
		"OpMemberName %BP_gl_PVOut 0 \"gl_Position\"\n"
		"OpMemberName %BP_gl_PVOut 1 \"gl_PointSize\"\n"
		"OpMemberName %BP_gl_PVOut 2 \"gl_ClipDistance\"\n"
		"OpMemberName %BP_gl_PVOut 3 \"gl_CullDistance\"\n"
		"OpName %BP_gl_in \"gl_in\"\n"
		"OpName %BP_gl_TessLevelOuter \"gl_TessLevelOuter\"\n"
		"OpName %BP_gl_TessLevelInner \"gl_TessLevelInner\"\n"
		"OpDecorate %BP_out_color Location 1\n"
		"OpDecorate %BP_gl_InvocationID BuiltIn InvocationId\n"
		"OpDecorate %BP_in_color Location 1\n"
		"OpMemberDecorate %BP_gl_PerVertex 0 BuiltIn Position\n"
		"OpMemberDecorate %BP_gl_PerVertex 1 BuiltIn PointSize\n"
		"OpMemberDecorate %BP_gl_PerVertex 2 BuiltIn ClipDistance\n"
		"OpMemberDecorate %BP_gl_PerVertex 3 BuiltIn CullDistance\n"
		"OpDecorate %BP_gl_PerVertex Block\n"
		"OpMemberDecorate %BP_gl_PVOut 0 BuiltIn Position\n"
		"OpMemberDecorate %BP_gl_PVOut 1 BuiltIn PointSize\n"
		"OpMemberDecorate %BP_gl_PVOut 2 BuiltIn ClipDistance\n"
		"OpMemberDecorate %BP_gl_PVOut 3 BuiltIn CullDistance\n"
		"OpDecorate %BP_gl_PVOut Block\n"
		"OpDecorate %BP_gl_TessLevelOuter Patch\n"
		"OpDecorate %BP_gl_TessLevelOuter BuiltIn TessLevelOuter\n"
		"OpDecorate %BP_gl_TessLevelInner Patch\n"
		"OpDecorate %BP_gl_TessLevelInner BuiltIn TessLevelInner\n"
		"${decoration:opt}\n"
		SPIRV_ASSEMBLY_TYPES
		SPIRV_ASSEMBLY_CONSTANTS
		SPIRV_ASSEMBLY_ARRAYS
		"%BP_out_color = OpVariable %op_a3v4f32 Output\n"
		"%BP_gl_InvocationID = OpVariable %ip_i32 Input\n"
		"%BP_in_color = OpVariable %ip_a32v4f32 Input\n"
		"%BP_gl_PerVertex = OpTypeStruct %v4f32 %f32 %a1f32 %a1f32\n"
		"%BP_a3_gl_PerVertex = OpTypeArray %BP_gl_PerVertex %c_u32_3\n"
		"%BP_op_a3_gl_PerVertex = OpTypePointer Output %BP_a3_gl_PerVertex\n"
		"%BP_gl_out = OpVariable %BP_op_a3_gl_PerVertex Output\n"
		"%BP_gl_PVOut = OpTypeStruct %v4f32 %f32 %a1f32 %a1f32\n"
		"%BP_a32_gl_PVOut = OpTypeArray %BP_gl_PVOut %c_u32_32\n"
		"%BP_ip_a32_gl_PVOut = OpTypePointer Input %BP_a32_gl_PVOut\n"
		"%BP_gl_in = OpVariable %BP_ip_a32_gl_PVOut Input\n"
		"%BP_gl_TessLevelOuter = OpVariable %op_a4f32 Output\n"
		"%BP_gl_TessLevelInner = OpVariable %op_a2f32 Output\n"
		"${pre_main:opt}\n"

		"%BP_main = OpFunction %void None %fun\n"
		"%BP_label = OpLabel\n"

		"%BP_gl_Invoc = OpLoad %i32 %BP_gl_InvocationID\n"

		"%BP_in_col_loc = OpAccessChain %ip_v4f32 %BP_in_color %BP_gl_Invoc\n"
		"%BP_out_col_loc = OpAccessChain %op_v4f32 %BP_out_color %BP_gl_Invoc\n"
		"%BP_in_col_val = OpLoad %v4f32 %BP_in_col_loc\n"
		"%BP_clr_transformed = OpFunctionCall %v4f32 %test_code %BP_in_col_val\n"
		"OpStore %BP_out_col_loc %BP_clr_transformed\n"

		"%BP_in_pos_loc = OpAccessChain %ip_v4f32 %BP_gl_in %BP_gl_Invoc %c_i32_0\n"
		"%BP_out_pos_loc = OpAccessChain %op_v4f32 %BP_gl_out %BP_gl_Invoc %c_i32_0\n"
		"%BP_in_pos_val = OpLoad %v4f32 %BP_in_pos_loc\n"
		"OpStore %BP_out_pos_loc %BP_in_pos_val\n"

		"%BP_cmp = OpIEqual %bool %BP_gl_Invoc %c_i32_0\n"
		"OpSelectionMerge %BP_merge_label None\n"
		"OpBranchConditional %BP_cmp %BP_if_label %BP_merge_label\n"
		"%BP_if_label = OpLabel\n"
		"%BP_gl_TessLevelOuterPos_0 = OpAccessChain %op_f32 %BP_gl_TessLevelOuter %c_i32_0\n"
		"%BP_gl_TessLevelOuterPos_1 = OpAccessChain %op_f32 %BP_gl_TessLevelOuter %c_i32_1\n"
		"%BP_gl_TessLevelOuterPos_2 = OpAccessChain %op_f32 %BP_gl_TessLevelOuter %c_i32_2\n"
		"%BP_gl_TessLevelInnerPos_0 = OpAccessChain %op_f32 %BP_gl_TessLevelInner %c_i32_0\n"
		"OpStore %BP_gl_TessLevelOuterPos_0 %c_f32_1\n"
		"OpStore %BP_gl_TessLevelOuterPos_1 %c_f32_1\n"
		"OpStore %BP_gl_TessLevelOuterPos_2 %c_f32_1\n"
		"OpStore %BP_gl_TessLevelInnerPos_0 %c_f32_1\n"
		"OpBranch %BP_merge_label\n"
		"%BP_merge_label = OpLabel\n"
		"OpReturn\n"
		"OpFunctionEnd\n"
		"${testfun}\n";
	return tcu::StringTemplate(tessControlShaderBoilerplate).specialize(fragments);
}

// Creates tess-evaluation-shader assembly by specializing a boilerplate
// StringTemplate on fragments, which must (at least) map "testfun" to an
// OpFunction definition for %test_code that takes and returns a %v4f32.
// Boilerplate IDs are prefixed with "BP_" to avoid collisions with fragments.
//
// It roughly corresponds to the following glsl.
//
// #version 450
//
// layout(triangles, equal_spacing, ccw) in;
// layout(location = 1) in vec4 in_color[];
// layout(location = 1) out vec4 out_color;
//
// #define interpolate(val)
//   vec4(gl_TessCoord.x) * val[0] + vec4(gl_TessCoord.y) * val[1] +
//          vec4(gl_TessCoord.z) * val[2]
//
// void main() {
//   gl_Position = vec4(gl_TessCoord.x) * gl_in[0].gl_Position +
//                  vec4(gl_TessCoord.y) * gl_in[1].gl_Position +
//                  vec4(gl_TessCoord.z) * gl_in[2].gl_Position;
//   out_color = testfun(interpolate(in_color));
// }
string makeTessEvalShaderAssembly(const map<string, string>& fragments)
{
	static const char tessEvalBoilerplate[] =
		"OpCapability Tessellation\n"
		"OpCapability ClipDistance\n"
		"OpCapability CullDistance\n"
		"OpMemoryModel Logical GLSL450\n"
		"OpEntryPoint TessellationEvaluation %BP_main \"main\" %BP_stream %BP_gl_TessCoord %BP_gl_in %BP_out_color %BP_in_color\n"
		"OpExecutionMode %BP_main Triangles\n"
		"OpExecutionMode %BP_main SpacingEqual\n"
		"OpExecutionMode %BP_main VertexOrderCcw\n"
		"${debug:opt}\n"
		"OpName %BP_main \"main\"\n"
		"OpName %test_code \"testfun(vf4;\"\n"
		"OpName %BP_gl_PerVertexOut \"gl_PerVertex\"\n"
		"OpMemberName %BP_gl_PerVertexOut 0 \"gl_Position\"\n"
		"OpMemberName %BP_gl_PerVertexOut 1 \"gl_PointSize\"\n"
		"OpMemberName %BP_gl_PerVertexOut 2 \"gl_ClipDistance\"\n"
		"OpMemberName %BP_gl_PerVertexOut 3 \"gl_CullDistance\"\n"
		"OpName %BP_stream \"\"\n"
		"OpName %BP_gl_TessCoord \"gl_TessCoord\"\n"
		"OpName %BP_gl_PerVertexIn \"gl_PerVertex\"\n"
		"OpMemberName %BP_gl_PerVertexIn 0 \"gl_Position\"\n"
		"OpMemberName %BP_gl_PerVertexIn 1 \"gl_PointSize\"\n"
		"OpMemberName %BP_gl_PerVertexIn 2 \"gl_ClipDistance\"\n"
		"OpMemberName %BP_gl_PerVertexIn 3 \"gl_CullDistance\"\n"
		"OpName %BP_gl_in \"gl_in\"\n"
		"OpName %BP_out_color \"out_color\"\n"
		"OpName %BP_in_color \"in_color\"\n"
		"OpMemberDecorate %BP_gl_PerVertexOut 0 BuiltIn Position\n"
		"OpMemberDecorate %BP_gl_PerVertexOut 1 BuiltIn PointSize\n"
		"OpMemberDecorate %BP_gl_PerVertexOut 2 BuiltIn ClipDistance\n"
		"OpMemberDecorate %BP_gl_PerVertexOut 3 BuiltIn CullDistance\n"
		"OpDecorate %BP_gl_PerVertexOut Block\n"
		"OpDecorate %BP_gl_TessCoord BuiltIn TessCoord\n"
		"OpMemberDecorate %BP_gl_PerVertexIn 0 BuiltIn Position\n"
		"OpMemberDecorate %BP_gl_PerVertexIn 1 BuiltIn PointSize\n"
		"OpMemberDecorate %BP_gl_PerVertexIn 2 BuiltIn ClipDistance\n"
		"OpMemberDecorate %BP_gl_PerVertexIn 3 BuiltIn CullDistance\n"
		"OpDecorate %BP_gl_PerVertexIn Block\n"
		"OpDecorate %BP_out_color Location 1\n"
		"OpDecorate %BP_in_color Location 1\n"
		"${decoration:opt}\n"
		SPIRV_ASSEMBLY_TYPES
		SPIRV_ASSEMBLY_CONSTANTS
		SPIRV_ASSEMBLY_ARRAYS
		"%BP_gl_PerVertexOut = OpTypeStruct %v4f32 %f32 %a1f32 %a1f32\n"
		"%BP_op_gl_PerVertexOut = OpTypePointer Output %BP_gl_PerVertexOut\n"
		"%BP_stream = OpVariable %BP_op_gl_PerVertexOut Output\n"
		"%BP_gl_TessCoord = OpVariable %ip_v3f32 Input\n"
		"%BP_gl_PerVertexIn = OpTypeStruct %v4f32 %f32 %a1f32 %a1f32\n"
		"%BP_a32_gl_PerVertexIn = OpTypeArray %BP_gl_PerVertexIn %c_u32_32\n"
		"%BP_ip_a32_gl_PerVertexIn = OpTypePointer Input %BP_a32_gl_PerVertexIn\n"
		"%BP_gl_in = OpVariable %BP_ip_a32_gl_PerVertexIn Input\n"
		"%BP_out_color = OpVariable %op_v4f32 Output\n"
		"%BP_in_color = OpVariable %ip_a32v4f32 Input\n"
		"${pre_main:opt}\n"
		"%BP_main = OpFunction %void None %fun\n"
		"%BP_label = OpLabel\n"
		"%BP_gl_TC_0 = OpAccessChain %ip_f32 %BP_gl_TessCoord %c_u32_0\n"
		"%BP_gl_TC_1 = OpAccessChain %ip_f32 %BP_gl_TessCoord %c_u32_1\n"
		"%BP_gl_TC_2 = OpAccessChain %ip_f32 %BP_gl_TessCoord %c_u32_2\n"
		"%BP_gl_in_gl_Pos_0 = OpAccessChain %ip_v4f32 %BP_gl_in %c_i32_0 %c_i32_0\n"
		"%BP_gl_in_gl_Pos_1 = OpAccessChain %ip_v4f32 %BP_gl_in %c_i32_1 %c_i32_0\n"
		"%BP_gl_in_gl_Pos_2 = OpAccessChain %ip_v4f32 %BP_gl_in %c_i32_2 %c_i32_0\n"

		"%BP_gl_OPos = OpAccessChain %op_v4f32 %BP_stream %c_i32_0\n"
		"%BP_in_color_0 = OpAccessChain %ip_v4f32 %BP_in_color %c_i32_0\n"
		"%BP_in_color_1 = OpAccessChain %ip_v4f32 %BP_in_color %c_i32_1\n"
		"%BP_in_color_2 = OpAccessChain %ip_v4f32 %BP_in_color %c_i32_2\n"

		"%BP_TC_W_0 = OpLoad %f32 %BP_gl_TC_0\n"
		"%BP_TC_W_1 = OpLoad %f32 %BP_gl_TC_1\n"
		"%BP_TC_W_2 = OpLoad %f32 %BP_gl_TC_2\n"
		"%BP_v4f32_TC_0 = OpCompositeConstruct %v4f32 %BP_TC_W_0 %BP_TC_W_0 %BP_TC_W_0 %BP_TC_W_0\n"
		"%BP_v4f32_TC_1 = OpCompositeConstruct %v4f32 %BP_TC_W_1 %BP_TC_W_1 %BP_TC_W_1 %BP_TC_W_1\n"
		"%BP_v4f32_TC_2 = OpCompositeConstruct %v4f32 %BP_TC_W_2 %BP_TC_W_2 %BP_TC_W_2 %BP_TC_W_2\n"

		"%BP_gl_IP_0 = OpLoad %v4f32 %BP_gl_in_gl_Pos_0\n"
		"%BP_gl_IP_1 = OpLoad %v4f32 %BP_gl_in_gl_Pos_1\n"
		"%BP_gl_IP_2 = OpLoad %v4f32 %BP_gl_in_gl_Pos_2\n"

		"%BP_IP_W_0 = OpFMul %v4f32 %BP_v4f32_TC_0 %BP_gl_IP_0\n"
		"%BP_IP_W_1 = OpFMul %v4f32 %BP_v4f32_TC_1 %BP_gl_IP_1\n"
		"%BP_IP_W_2 = OpFMul %v4f32 %BP_v4f32_TC_2 %BP_gl_IP_2\n"

		"%BP_pos_sum_0 = OpFAdd %v4f32 %BP_IP_W_0 %BP_IP_W_1\n"
		"%BP_pos_sum_1 = OpFAdd %v4f32 %BP_pos_sum_0 %BP_IP_W_2\n"

		"OpStore %BP_gl_OPos %BP_pos_sum_1\n"

		"%BP_IC_0 = OpLoad %v4f32 %BP_in_color_0\n"
		"%BP_IC_1 = OpLoad %v4f32 %BP_in_color_1\n"
		"%BP_IC_2 = OpLoad %v4f32 %BP_in_color_2\n"

		"%BP_IC_W_0 = OpFMul %v4f32 %BP_v4f32_TC_0 %BP_IC_0\n"
		"%BP_IC_W_1 = OpFMul %v4f32 %BP_v4f32_TC_1 %BP_IC_1\n"
		"%BP_IC_W_2 = OpFMul %v4f32 %BP_v4f32_TC_2 %BP_IC_2\n"

		"%BP_col_sum_0 = OpFAdd %v4f32 %BP_IC_W_0 %BP_IC_W_1\n"
		"%BP_col_sum_1 = OpFAdd %v4f32 %BP_col_sum_0 %BP_IC_W_2\n"

		"%BP_clr_transformed = OpFunctionCall %v4f32 %test_code %BP_col_sum_1\n"

		"OpStore %BP_out_color %BP_clr_transformed\n"
		"OpReturn\n"
		"OpFunctionEnd\n"
		"${testfun}\n";
	return tcu::StringTemplate(tessEvalBoilerplate).specialize(fragments);
}

// Creates geometry-shader assembly by specializing a boilerplate StringTemplate
// on fragments, which must (at least) map "testfun" to an OpFunction definition
// for %test_code that takes and returns a %v4f32.  Boilerplate IDs are prefixed
// with "BP_" to avoid collisions with fragments.
//
// Derived from this GLSL:
//
// #version 450
// layout(triangles) in;
// layout(triangle_strip, max_vertices = 3) out;
//
// layout(location = 1) in vec4 in_color[];
// layout(location = 1) out vec4 out_color;
//
// void main() {
//   gl_Position = gl_in[0].gl_Position;
//   out_color = test_fun(in_color[0]);
//   EmitVertex();
//   gl_Position = gl_in[1].gl_Position;
//   out_color = test_fun(in_color[1]);
//   EmitVertex();
//   gl_Position = gl_in[2].gl_Position;
//   out_color = test_fun(in_color[2]);
//   EmitVertex();
//   EndPrimitive();
// }
string makeGeometryShaderAssembly(const map<string, string>& fragments)
{
	static const char geometryShaderBoilerplate[] =
		"OpCapability Geometry\n"
		"OpCapability ClipDistance\n"
		"OpCapability CullDistance\n"
		"OpMemoryModel Logical GLSL450\n"
		"OpEntryPoint Geometry %BP_main \"main\" %BP_out_gl_position %BP_gl_in %BP_out_color %BP_in_color\n"
		"OpExecutionMode %BP_main Triangles\n"
		"OpExecutionMode %BP_main OutputTriangleStrip\n"
		"OpExecutionMode %BP_main OutputVertices 3\n"
		"${debug:opt}\n"
		"OpName %BP_main \"main\"\n"
		"OpName %BP_per_vertex_in \"gl_PerVertex\"\n"
		"OpMemberName %BP_per_vertex_in 0 \"gl_Position\"\n"
		"OpMemberName %BP_per_vertex_in 1 \"gl_PointSize\"\n"
		"OpMemberName %BP_per_vertex_in 2 \"gl_ClipDistance\"\n"
		"OpMemberName %BP_per_vertex_in 3 \"gl_CullDistance\"\n"
		"OpName %BP_gl_in \"gl_in\"\n"
		"OpName %BP_out_color \"out_color\"\n"
		"OpName %BP_in_color \"in_color\"\n"
		"OpName %test_code \"testfun(vf4;\"\n"
		"OpDecorate %BP_out_gl_position BuiltIn Position\n"
		"OpMemberDecorate %BP_per_vertex_in 0 BuiltIn Position\n"
		"OpMemberDecorate %BP_per_vertex_in 1 BuiltIn PointSize\n"
		"OpMemberDecorate %BP_per_vertex_in 2 BuiltIn ClipDistance\n"
		"OpMemberDecorate %BP_per_vertex_in 3 BuiltIn CullDistance\n"
		"OpDecorate %BP_per_vertex_in Block\n"
		"OpDecorate %BP_out_color Location 1\n"
		"OpDecorate %BP_in_color Location 1\n"
		"${decoration:opt}\n"
		SPIRV_ASSEMBLY_TYPES
		SPIRV_ASSEMBLY_CONSTANTS
		SPIRV_ASSEMBLY_ARRAYS
		"%BP_per_vertex_in = OpTypeStruct %v4f32 %f32 %a1f32 %a1f32\n"
		"%BP_a3_per_vertex_in = OpTypeArray %BP_per_vertex_in %c_u32_3\n"
		"%BP_ip_a3_per_vertex_in = OpTypePointer Input %BP_a3_per_vertex_in\n"

		"%BP_gl_in = OpVariable %BP_ip_a3_per_vertex_in Input\n"
		"%BP_out_color = OpVariable %op_v4f32 Output\n"
		"%BP_in_color = OpVariable %ip_a3v4f32 Input\n"
		"%BP_out_gl_position = OpVariable %op_v4f32 Output\n"
		"${pre_main:opt}\n"

		"%BP_main = OpFunction %void None %fun\n"
		"%BP_label = OpLabel\n"
		"%BP_gl_in_0_gl_position = OpAccessChain %ip_v4f32 %BP_gl_in %c_i32_0 %c_i32_0\n"
		"%BP_gl_in_1_gl_position = OpAccessChain %ip_v4f32 %BP_gl_in %c_i32_1 %c_i32_0\n"
		"%BP_gl_in_2_gl_position = OpAccessChain %ip_v4f32 %BP_gl_in %c_i32_2 %c_i32_0\n"

		"%BP_in_position_0 = OpLoad %v4f32 %BP_gl_in_0_gl_position\n"
		"%BP_in_position_1 = OpLoad %v4f32 %BP_gl_in_1_gl_position\n"
		"%BP_in_position_2 = OpLoad %v4f32 %BP_gl_in_2_gl_position \n"

		"%BP_in_color_0_ptr = OpAccessChain %ip_v4f32 %BP_in_color %c_i32_0\n"
		"%BP_in_color_1_ptr = OpAccessChain %ip_v4f32 %BP_in_color %c_i32_1\n"
		"%BP_in_color_2_ptr = OpAccessChain %ip_v4f32 %BP_in_color %c_i32_2\n"

		"%BP_in_color_0 = OpLoad %v4f32 %BP_in_color_0_ptr\n"
		"%BP_in_color_1 = OpLoad %v4f32 %BP_in_color_1_ptr\n"
		"%BP_in_color_2 = OpLoad %v4f32 %BP_in_color_2_ptr\n"

		"%BP_transformed_in_color_0 = OpFunctionCall %v4f32 %test_code %BP_in_color_0\n"
		"%BP_transformed_in_color_1 = OpFunctionCall %v4f32 %test_code %BP_in_color_1\n"
		"%BP_transformed_in_color_2 = OpFunctionCall %v4f32 %test_code %BP_in_color_2\n"


		"OpStore %BP_out_gl_position %BP_in_position_0\n"
		"OpStore %BP_out_color %BP_transformed_in_color_0\n"
		"OpEmitVertex\n"

		"OpStore %BP_out_gl_position %BP_in_position_1\n"
		"OpStore %BP_out_color %BP_transformed_in_color_1\n"
		"OpEmitVertex\n"

		"OpStore %BP_out_gl_position %BP_in_position_2\n"
		"OpStore %BP_out_color %BP_transformed_in_color_2\n"
		"OpEmitVertex\n"

		"OpEndPrimitive\n"
		"OpReturn\n"
		"OpFunctionEnd\n"
		"${testfun}\n";
	return tcu::StringTemplate(geometryShaderBoilerplate).specialize(fragments);
}

// Creates fragment-shader assembly by specializing a boilerplate StringTemplate
// on fragments, which must (at least) map "testfun" to an OpFunction definition
// for %test_code that takes and returns a %v4f32.  Boilerplate IDs are prefixed
// with "BP_" to avoid collisions with fragments.
//
// Derived from this GLSL:
//
// layout(location = 1) in highp vec4 vtxColor;
// layout(location = 0) out highp vec4 fragColor;
// highp vec4 testfun(highp vec4 x) { return x; }
// void main(void) { fragColor = testfun(vtxColor); }
//
// with modifications including passing vtxColor by value and ripping out
// testfun() definition.
string makeFragmentShaderAssembly(const map<string, string>& fragments)
{
	static const char fragmentShaderBoilerplate[] =
		"OpCapability Shader\n"
		"OpMemoryModel Logical GLSL450\n"
		"OpEntryPoint Fragment %BP_main \"main\" %BP_vtxColor %BP_fragColor\n"
		"OpExecutionMode %BP_main OriginUpperLeft\n"
		"${debug:opt}\n"
		"OpName %BP_main \"main\"\n"
		"OpName %BP_fragColor \"fragColor\"\n"
		"OpName %BP_vtxColor \"vtxColor\"\n"
		"OpName %test_code \"testfun(vf4;\"\n"
		"OpDecorate %BP_fragColor Location 0\n"
		"OpDecorate %BP_vtxColor Location 1\n"
		"${decoration:opt}\n"
		SPIRV_ASSEMBLY_TYPES
		SPIRV_ASSEMBLY_CONSTANTS
		SPIRV_ASSEMBLY_ARRAYS
		"%BP_fragColor = OpVariable %op_v4f32 Output\n"
		"%BP_vtxColor = OpVariable %ip_v4f32 Input\n"
		"${pre_main:opt}\n"
		"%BP_main = OpFunction %void None %fun\n"
		"%BP_label_main = OpLabel\n"
		"%BP_tmp1 = OpLoad %v4f32 %BP_vtxColor\n"
		"%BP_tmp2 = OpFunctionCall %v4f32 %test_code %BP_tmp1\n"
		"OpStore %BP_fragColor %BP_tmp2\n"
		"OpReturn\n"
		"OpFunctionEnd\n"
		"${testfun}\n";
	return tcu::StringTemplate(fragmentShaderBoilerplate).specialize(fragments);
}

// Creates fragments that specialize into a simple pass-through shader (of any kind).
map<string, string> passthruFragments(void)
{
	map<string, string> fragments;
	fragments["testfun"] =
		// A %test_code function that returns its argument unchanged.
		"%test_code = OpFunction %v4f32 None %v4f32_function\n"
		"%param1 = OpFunctionParameter %v4f32\n"
		"%label_testfun = OpLabel\n"
		"OpReturnValue %param1\n"
		"OpFunctionEnd\n";
	return fragments;
}

// Adds shader assembly text to dst.spirvAsmSources for all shader kinds.
// Vertex shader gets custom code from context, the rest are pass-through.
void addShaderCodeCustomVertex(vk::SourceCollections& dst, InstanceContext context)
{
	map<string, string> passthru = passthruFragments();
	dst.spirvAsmSources.add("vert") << makeVertexShaderAssembly(context.testCodeFragments);
	dst.spirvAsmSources.add("frag") << makeFragmentShaderAssembly(passthru);
}

// Adds shader assembly text to dst.spirvAsmSources for all shader kinds.
// Tessellation control shader gets custom code from context, the rest are
// pass-through.
void addShaderCodeCustomTessControl(vk::SourceCollections& dst, InstanceContext context)
{
	map<string, string> passthru = passthruFragments();
	dst.spirvAsmSources.add("vert") << makeVertexShaderAssembly(passthru);
	dst.spirvAsmSources.add("tessc") << makeTessControlShaderAssembly(context.testCodeFragments);
	dst.spirvAsmSources.add("tesse") << makeTessEvalShaderAssembly(passthru);
	dst.spirvAsmSources.add("frag") << makeFragmentShaderAssembly(passthru);
}

// Adds shader assembly text to dst.spirvAsmSources for all shader kinds.
// Tessellation evaluation shader gets custom code from context, the rest are
// pass-through.
void addShaderCodeCustomTessEval(vk::SourceCollections& dst, InstanceContext context)
{
	map<string, string> passthru = passthruFragments();
	dst.spirvAsmSources.add("vert") << makeVertexShaderAssembly(passthru);
	dst.spirvAsmSources.add("tessc") << makeTessControlShaderAssembly(passthru);
	dst.spirvAsmSources.add("tesse") << makeTessEvalShaderAssembly(context.testCodeFragments);
	dst.spirvAsmSources.add("frag") << makeFragmentShaderAssembly(passthru);
}

// Adds shader assembly text to dst.spirvAsmSources for all shader kinds.
// Geometry shader gets custom code from context, the rest are pass-through.
void addShaderCodeCustomGeometry(vk::SourceCollections& dst, InstanceContext context)
{
	map<string, string> passthru = passthruFragments();
	dst.spirvAsmSources.add("vert") << makeVertexShaderAssembly(passthru);
	dst.spirvAsmSources.add("geom") << makeGeometryShaderAssembly(context.testCodeFragments);
	dst.spirvAsmSources.add("frag") << makeFragmentShaderAssembly(passthru);
}

// Adds shader assembly text to dst.spirvAsmSources for all shader kinds.
// Fragment shader gets custom code from context, the rest are pass-through.
void addShaderCodeCustomFragment(vk::SourceCollections& dst, InstanceContext context)
{
	map<string, string> passthru = passthruFragments();
	dst.spirvAsmSources.add("vert") << makeVertexShaderAssembly(passthru);
	dst.spirvAsmSources.add("frag") << makeFragmentShaderAssembly(context.testCodeFragments);
}

void createCombinedModule(vk::SourceCollections& dst, InstanceContext)
{
	// \todo [2015-12-07 awoloszyn] Make tessellation / geometry conditional
	// \todo [2015-12-07 awoloszyn] Remove OpName and OpMemberName at some point
	dst.spirvAsmSources.add("module") <<
		"OpCapability Shader\n"
		"OpCapability ClipDistance\n"
		"OpCapability CullDistance\n"
		"OpCapability Geometry\n"
		"OpCapability Tessellation\n"
		"OpMemoryModel Logical GLSL450\n"

		"OpEntryPoint Vertex %vert_main \"main\" %vert_Position %vert_vtxColor %vert_color %vert_vtxPosition %vert_vertex_id %vert_instance_id\n"
		"OpEntryPoint Geometry %geom_main \"main\" %geom_out_gl_position %geom_gl_in %geom_out_color %geom_in_color\n"
		"OpEntryPoint TessellationControl %tessc_main \"main\" %tessc_out_color %tessc_gl_InvocationID %tessc_in_color %tessc_out_position %tessc_in_position %tessc_gl_TessLevelOuter %tessc_gl_TessLevelInner\n"
		"OpEntryPoint TessellationEvaluation %tesse_main \"main\" %tesse_stream %tesse_gl_tessCoord %tesse_in_position %tesse_out_color %tesse_in_color \n"
		"OpEntryPoint Fragment %frag_main \"main\" %frag_vtxColor %frag_fragColor\n"

		"OpExecutionMode %geom_main Triangles\n"
		"OpExecutionMode %geom_main OutputTriangleStrip\n"
		"OpExecutionMode %geom_main OutputVertices 3\n"

		"OpExecutionMode %tessc_main OutputVertices 3\n"

		"OpExecutionMode %tesse_main Triangles\n"

		"OpExecutionMode %frag_main OriginUpperLeft\n"

		"OpName %vert_main \"main\"\n"
		"OpName %vert_vtxPosition \"vtxPosition\"\n"
		"OpName %vert_Position \"position\"\n"
		"OpName %vert_vtxColor \"vtxColor\"\n"
		"OpName %vert_color \"color\"\n"
		"OpName %vert_vertex_id \"gl_VertexIndex\"\n"
		"OpName %vert_instance_id \"gl_InstanceIndex\"\n"
		"OpName %geom_main \"main\"\n"
		"OpName %geom_per_vertex_in \"gl_PerVertex\"\n"
		"OpMemberName %geom_per_vertex_in 0 \"gl_Position\"\n"
		"OpMemberName %geom_per_vertex_in 1 \"gl_PointSize\"\n"
		"OpMemberName %geom_per_vertex_in 2 \"gl_ClipDistance\"\n"
		"OpMemberName %geom_per_vertex_in 3 \"gl_CullDistance\"\n"
		"OpName %geom_gl_in \"gl_in\"\n"
		"OpName %geom_out_color \"out_color\"\n"
		"OpName %geom_in_color \"in_color\"\n"
		"OpName %tessc_main \"main\"\n"
		"OpName %tessc_out_color \"out_color\"\n"
		"OpName %tessc_gl_InvocationID \"gl_InvocationID\"\n"
		"OpName %tessc_in_color \"in_color\"\n"
		"OpName %tessc_out_position \"out_position\"\n"
		"OpName %tessc_in_position \"in_position\"\n"
		"OpName %tessc_gl_TessLevelOuter \"gl_TessLevelOuter\"\n"
		"OpName %tessc_gl_TessLevelInner \"gl_TessLevelInner\"\n"
		"OpName %tesse_main \"main\"\n"
		"OpName %tesse_per_vertex_out \"gl_PerVertex\"\n"
		"OpMemberName %tesse_per_vertex_out 0 \"gl_Position\"\n"
		"OpMemberName %tesse_per_vertex_out 1 \"gl_PointSize\"\n"
		"OpMemberName %tesse_per_vertex_out 2 \"gl_ClipDistance\"\n"
		"OpMemberName %tesse_per_vertex_out 3 \"gl_CullDistance\"\n"
		"OpName %tesse_stream \"\"\n"
		"OpName %tesse_gl_tessCoord \"gl_TessCoord\"\n"
		"OpName %tesse_in_position \"in_position\"\n"
		"OpName %tesse_out_color \"out_color\"\n"
		"OpName %tesse_in_color \"in_color\"\n"
		"OpName %frag_main \"main\"\n"
		"OpName %frag_fragColor \"fragColor\"\n"
		"OpName %frag_vtxColor \"vtxColor\"\n"

		"; Vertex decorations\n"
		"OpDecorate %vert_vtxPosition Location 2\n"
		"OpDecorate %vert_Position Location 0\n"
		"OpDecorate %vert_vtxColor Location 1\n"
		"OpDecorate %vert_color Location 1\n"
		"OpDecorate %vert_vertex_id BuiltIn VertexIndex\n"
		"OpDecorate %vert_instance_id BuiltIn InstanceIndex\n"

		"; Geometry decorations\n"
		"OpDecorate %geom_out_gl_position BuiltIn Position\n"
		"OpMemberDecorate %geom_per_vertex_in 0 BuiltIn Position\n"
		"OpMemberDecorate %geom_per_vertex_in 1 BuiltIn PointSize\n"
		"OpMemberDecorate %geom_per_vertex_in 2 BuiltIn ClipDistance\n"
		"OpMemberDecorate %geom_per_vertex_in 3 BuiltIn CullDistance\n"
		"OpDecorate %geom_per_vertex_in Block\n"
		"OpDecorate %geom_out_color Location 1\n"
		"OpDecorate %geom_in_color Location 1\n"

		"; Tessellation Control decorations\n"
		"OpDecorate %tessc_out_color Location 1\n"
		"OpDecorate %tessc_gl_InvocationID BuiltIn InvocationId\n"
		"OpDecorate %tessc_in_color Location 1\n"
		"OpDecorate %tessc_out_position Location 2\n"
		"OpDecorate %tessc_in_position Location 2\n"
		"OpDecorate %tessc_gl_TessLevelOuter Patch\n"
		"OpDecorate %tessc_gl_TessLevelOuter BuiltIn TessLevelOuter\n"
		"OpDecorate %tessc_gl_TessLevelInner Patch\n"
		"OpDecorate %tessc_gl_TessLevelInner BuiltIn TessLevelInner\n"

		"; Tessellation Evaluation decorations\n"
		"OpMemberDecorate %tesse_per_vertex_out 0 BuiltIn Position\n"
		"OpMemberDecorate %tesse_per_vertex_out 1 BuiltIn PointSize\n"
		"OpMemberDecorate %tesse_per_vertex_out 2 BuiltIn ClipDistance\n"
		"OpMemberDecorate %tesse_per_vertex_out 3 BuiltIn CullDistance\n"
		"OpDecorate %tesse_per_vertex_out Block\n"
		"OpDecorate %tesse_gl_tessCoord BuiltIn TessCoord\n"
		"OpDecorate %tesse_in_position Location 2\n"
		"OpDecorate %tesse_out_color Location 1\n"
		"OpDecorate %tesse_in_color Location 1\n"

		"; Fragment decorations\n"
		"OpDecorate %frag_fragColor Location 0\n"
		"OpDecorate %frag_vtxColor Location 1\n"

		SPIRV_ASSEMBLY_TYPES
		SPIRV_ASSEMBLY_CONSTANTS
		SPIRV_ASSEMBLY_ARRAYS

		"; Vertex Variables\n"
		"%vert_vtxPosition = OpVariable %op_v4f32 Output\n"
		"%vert_Position = OpVariable %ip_v4f32 Input\n"
		"%vert_vtxColor = OpVariable %op_v4f32 Output\n"
		"%vert_color = OpVariable %ip_v4f32 Input\n"
		"%vert_vertex_id = OpVariable %ip_i32 Input\n"
		"%vert_instance_id = OpVariable %ip_i32 Input\n"

		"; Geometry Variables\n"
		"%geom_per_vertex_in = OpTypeStruct %v4f32 %f32 %a1f32 %a1f32\n"
		"%geom_a3_per_vertex_in = OpTypeArray %geom_per_vertex_in %c_u32_3\n"
		"%geom_ip_a3_per_vertex_in = OpTypePointer Input %geom_a3_per_vertex_in\n"
		"%geom_gl_in = OpVariable %geom_ip_a3_per_vertex_in Input\n"
		"%geom_out_color = OpVariable %op_v4f32 Output\n"
		"%geom_in_color = OpVariable %ip_a3v4f32 Input\n"
		"%geom_out_gl_position = OpVariable %op_v4f32 Output\n"

		"; Tessellation Control Variables\n"
		"%tessc_out_color = OpVariable %op_a3v4f32 Output\n"
		"%tessc_gl_InvocationID = OpVariable %ip_i32 Input\n"
		"%tessc_in_color = OpVariable %ip_a32v4f32 Input\n"
		"%tessc_out_position = OpVariable %op_a3v4f32 Output\n"
		"%tessc_in_position = OpVariable %ip_a32v4f32 Input\n"
		"%tessc_gl_TessLevelOuter = OpVariable %op_a4f32 Output\n"
		"%tessc_gl_TessLevelInner = OpVariable %op_a2f32 Output\n"

		"; Tessellation Evaluation Decorations\n"
		"%tesse_per_vertex_out = OpTypeStruct %v4f32 %f32 %a1f32 %a1f32\n"
		"%tesse_op_per_vertex_out = OpTypePointer Output %tesse_per_vertex_out\n"
		"%tesse_stream = OpVariable %tesse_op_per_vertex_out Output\n"
		"%tesse_gl_tessCoord = OpVariable %ip_v3f32 Input\n"
		"%tesse_in_position = OpVariable %ip_a32v4f32 Input\n"
		"%tesse_out_color = OpVariable %op_v4f32 Output\n"
		"%tesse_in_color = OpVariable %ip_a32v4f32 Input\n"

		"; Fragment Variables\n"
		"%frag_fragColor = OpVariable %op_v4f32 Output\n"
		"%frag_vtxColor = OpVariable %ip_v4f32 Input\n"

		"; Vertex Entry\n"
		"%vert_main = OpFunction %void None %fun\n"
		"%vert_label = OpLabel\n"
		"%vert_tmp_position = OpLoad %v4f32 %vert_Position\n"
		"OpStore %vert_vtxPosition %vert_tmp_position\n"
		"%vert_tmp_color = OpLoad %v4f32 %vert_color\n"
		"OpStore %vert_vtxColor %vert_tmp_color\n"
		"OpReturn\n"
		"OpFunctionEnd\n"

		"; Geometry Entry\n"
		"%geom_main = OpFunction %void None %fun\n"
		"%geom_label = OpLabel\n"
		"%geom_gl_in_0_gl_position = OpAccessChain %ip_v4f32 %geom_gl_in %c_i32_0 %c_i32_0\n"
		"%geom_gl_in_1_gl_position = OpAccessChain %ip_v4f32 %geom_gl_in %c_i32_1 %c_i32_0\n"
		"%geom_gl_in_2_gl_position = OpAccessChain %ip_v4f32 %geom_gl_in %c_i32_2 %c_i32_0\n"
		"%geom_in_position_0 = OpLoad %v4f32 %geom_gl_in_0_gl_position\n"
		"%geom_in_position_1 = OpLoad %v4f32 %geom_gl_in_1_gl_position\n"
		"%geom_in_position_2 = OpLoad %v4f32 %geom_gl_in_2_gl_position \n"
		"%geom_in_color_0_ptr = OpAccessChain %ip_v4f32 %geom_in_color %c_i32_0\n"
		"%geom_in_color_1_ptr = OpAccessChain %ip_v4f32 %geom_in_color %c_i32_1\n"
		"%geom_in_color_2_ptr = OpAccessChain %ip_v4f32 %geom_in_color %c_i32_2\n"
		"%geom_in_color_0 = OpLoad %v4f32 %geom_in_color_0_ptr\n"
		"%geom_in_color_1 = OpLoad %v4f32 %geom_in_color_1_ptr\n"
		"%geom_in_color_2 = OpLoad %v4f32 %geom_in_color_2_ptr\n"
		"OpStore %geom_out_gl_position %geom_in_position_0\n"
		"OpStore %geom_out_color %geom_in_color_0\n"
		"OpEmitVertex\n"
		"OpStore %geom_out_gl_position %geom_in_position_1\n"
		"OpStore %geom_out_color %geom_in_color_1\n"
		"OpEmitVertex\n"
		"OpStore %geom_out_gl_position %geom_in_position_2\n"
		"OpStore %geom_out_color %geom_in_color_2\n"
		"OpEmitVertex\n"
		"OpEndPrimitive\n"
		"OpReturn\n"
		"OpFunctionEnd\n"

		"; Tessellation Control Entry\n"
		"%tessc_main = OpFunction %void None %fun\n"
		"%tessc_label = OpLabel\n"
		"%tessc_invocation_id = OpLoad %i32 %tessc_gl_InvocationID\n"
		"%tessc_in_color_ptr = OpAccessChain %ip_v4f32 %tessc_in_color %tessc_invocation_id\n"
		"%tessc_in_position_ptr = OpAccessChain %ip_v4f32 %tessc_in_position %tessc_invocation_id\n"
		"%tessc_in_color_val = OpLoad %v4f32 %tessc_in_color_ptr\n"
		"%tessc_in_position_val = OpLoad %v4f32 %tessc_in_position_ptr\n"
		"%tessc_out_color_ptr = OpAccessChain %op_v4f32 %tessc_out_color %tessc_invocation_id\n"
		"%tessc_out_position_ptr = OpAccessChain %op_v4f32 %tessc_out_position %tessc_invocation_id\n"
		"OpStore %tessc_out_color_ptr %tessc_in_color_val\n"
		"OpStore %tessc_out_position_ptr %tessc_in_position_val\n"
		"%tessc_is_first_invocation = OpIEqual %bool %tessc_invocation_id %c_i32_0\n"
		"OpSelectionMerge %tessc_merge_label None\n"
		"OpBranchConditional %tessc_is_first_invocation %tessc_first_invocation %tessc_merge_label\n"
		"%tessc_first_invocation = OpLabel\n"
		"%tessc_tess_outer_0 = OpAccessChain %op_f32 %tessc_gl_TessLevelOuter %c_i32_0\n"
		"%tessc_tess_outer_1 = OpAccessChain %op_f32 %tessc_gl_TessLevelOuter %c_i32_1\n"
		"%tessc_tess_outer_2 = OpAccessChain %op_f32 %tessc_gl_TessLevelOuter %c_i32_2\n"
		"%tessc_tess_inner = OpAccessChain %op_f32 %tessc_gl_TessLevelInner %c_i32_0\n"
		"OpStore %tessc_tess_outer_0 %c_f32_1\n"
		"OpStore %tessc_tess_outer_1 %c_f32_1\n"
		"OpStore %tessc_tess_outer_2 %c_f32_1\n"
		"OpStore %tessc_tess_inner %c_f32_1\n"
		"OpBranch %tessc_merge_label\n"
		"%tessc_merge_label = OpLabel\n"
		"OpReturn\n"
		"OpFunctionEnd\n"

		"; Tessellation Evaluation Entry\n"
		"%tesse_main = OpFunction %void None %fun\n"
		"%tesse_label = OpLabel\n"
		"%tesse_tc_0_ptr = OpAccessChain %ip_f32 %tesse_gl_tessCoord %c_u32_0\n"
		"%tesse_tc_1_ptr = OpAccessChain %ip_f32 %tesse_gl_tessCoord %c_u32_1\n"
		"%tesse_tc_2_ptr = OpAccessChain %ip_f32 %tesse_gl_tessCoord %c_u32_2\n"
		"%tesse_tc_0 = OpLoad %f32 %tesse_tc_0_ptr\n"
		"%tesse_tc_1 = OpLoad %f32 %tesse_tc_1_ptr\n"
		"%tesse_tc_2 = OpLoad %f32 %tesse_tc_2_ptr\n"
		"%tesse_in_pos_0_ptr = OpAccessChain %ip_v4f32 %tesse_in_position %c_i32_0\n"
		"%tesse_in_pos_1_ptr = OpAccessChain %ip_v4f32 %tesse_in_position %c_i32_1\n"
		"%tesse_in_pos_2_ptr = OpAccessChain %ip_v4f32 %tesse_in_position %c_i32_2\n"
		"%tesse_in_pos_0 = OpLoad %v4f32 %tesse_in_pos_0_ptr\n"
		"%tesse_in_pos_1 = OpLoad %v4f32 %tesse_in_pos_1_ptr\n"
		"%tesse_in_pos_2 = OpLoad %v4f32 %tesse_in_pos_2_ptr\n"
		"%tesse_in_pos_0_weighted = OpVectorTimesScalar %v4f32 %tesse_tc_0 %tesse_in_pos_0\n"
		"%tesse_in_pos_1_weighted = OpVectorTimesScalar %v4f32 %tesse_tc_1 %tesse_in_pos_1\n"
		"%tesse_in_pos_2_weighted = OpVectorTimesScalar %v4f32 %tesse_tc_2 %tesse_in_pos_2\n"
		"%tesse_out_pos_ptr = OpAccessChain %op_v4f32 %tesse_stream %c_i32_0\n"
		"%tesse_in_pos_0_plus_pos_1 = OpFAdd %v4f32 %tesse_in_pos_0_weighted %tesse_in_pos_1_weighted\n"
		"%tesse_computed_out = OpFAdd %v4f32 %tesse_in_pos_0_plus_pos_1 %tesse_in_pos_2_weighted\n"
		"OpStore %tesse_out_pos_ptr %tesse_computed_out\n"
		"%tesse_in_clr_0_ptr = OpAccessChain %ip_v4f32 %tesse_in_color %c_i32_0\n"
		"%tesse_in_clr_1_ptr = OpAccessChain %ip_v4f32 %tesse_in_color %c_i32_1\n"
		"%tesse_in_clr_2_ptr = OpAccessChain %ip_v4f32 %tesse_in_color %c_i32_2\n"
		"%tesse_in_clr_0 = OpLoad %v4f32 %tesse_in_clr_0_ptr\n"
		"%tesse_in_clr_1 = OpLoad %v4f32 %tesse_in_clr_1_ptr\n"
		"%tesse_in_clr_2 = OpLoad %v4f32 %tesse_in_clr_2_ptr\n"
		"%tesse_in_clr_0_weighted = OpVectorTimesScalar %v4f32 %tesse_tc_0 %tesse_in_clr_0\n"
		"%tesse_in_clr_1_weighted = OpVectorTimesScalar %v4f32 %tesse_tc_1 %tesse_in_clr_1\n"
		"%tesse_in_clr_2_weighted = OpVectorTimesScalar %v4f32 %tesse_tc_2 %tesse_in_clr_2\n"
		"%tesse_in_clr_0_plus_col_1 = OpFAdd %v4f32 %tesse_in_clr_0_weighted %tesse_in_clr_1_weighted\n"
		"%tesse_computed_clr = OpFAdd %v4f32 %tesse_in_clr_0_plus_col_1 %tesse_in_clr_2_weighted\n"
		"OpStore %tesse_out_color %tesse_computed_clr\n"
		"OpReturn\n"
		"OpFunctionEnd\n"

		"; Fragment Entry\n"
		"%frag_main = OpFunction %void None %fun\n"
		"%frag_label_main = OpLabel\n"
		"%frag_tmp1 = OpLoad %v4f32 %frag_vtxColor\n"
		"OpStore %frag_fragColor %frag_tmp1\n"
		"OpReturn\n"
		"OpFunctionEnd\n";
}

// This has two shaders of each stage. The first
// is a passthrough, the second inverts the color.
void createMultipleEntries(vk::SourceCollections& dst, InstanceContext)
{
	dst.spirvAsmSources.add("vert") <<
	// This module contains 2 vertex shaders. One that is a passthrough
	// and a second that inverts the color of the output (1.0 - color).
		"OpCapability Shader\n"
		"OpMemoryModel Logical GLSL450\n"
		"OpEntryPoint Vertex %main \"vert1\" %Position %vtxColor %color %vtxPosition %vertex_id %instance_id\n"
		"OpEntryPoint Vertex %main2 \"vert2\" %Position %vtxColor %color %vtxPosition %vertex_id %instance_id\n"

		"OpName %main \"vert1\"\n"
		"OpName %main2 \"vert2\"\n"
		"OpName %vtxPosition \"vtxPosition\"\n"
		"OpName %Position \"position\"\n"
		"OpName %vtxColor \"vtxColor\"\n"
		"OpName %color \"color\"\n"
		"OpName %vertex_id \"gl_VertexIndex\"\n"
		"OpName %instance_id \"gl_InstanceIndex\"\n"

		"OpDecorate %vtxPosition Location 2\n"
		"OpDecorate %Position Location 0\n"
		"OpDecorate %vtxColor Location 1\n"
		"OpDecorate %color Location 1\n"
		"OpDecorate %vertex_id BuiltIn VertexIndex\n"
		"OpDecorate %instance_id BuiltIn InstanceIndex\n"
		SPIRV_ASSEMBLY_TYPES
		SPIRV_ASSEMBLY_CONSTANTS
		SPIRV_ASSEMBLY_ARRAYS
		"%cval = OpConstantComposite %v4f32 %c_f32_1 %c_f32_1 %c_f32_1 %c_f32_0\n"
		"%vtxPosition = OpVariable %op_v4f32 Output\n"
		"%Position = OpVariable %ip_v4f32 Input\n"
		"%vtxColor = OpVariable %op_v4f32 Output\n"
		"%color = OpVariable %ip_v4f32 Input\n"
		"%vertex_id = OpVariable %ip_i32 Input\n"
		"%instance_id = OpVariable %ip_i32 Input\n"

		"%main = OpFunction %void None %fun\n"
		"%label = OpLabel\n"
		"%tmp_position = OpLoad %v4f32 %Position\n"
		"OpStore %vtxPosition %tmp_position\n"
		"%tmp_color = OpLoad %v4f32 %color\n"
		"OpStore %vtxColor %tmp_color\n"
		"OpReturn\n"
		"OpFunctionEnd\n"

		"%main2 = OpFunction %void None %fun\n"
		"%label2 = OpLabel\n"
		"%tmp_position2 = OpLoad %v4f32 %Position\n"
		"OpStore %vtxPosition %tmp_position2\n"
		"%tmp_color2 = OpLoad %v4f32 %color\n"
		"%tmp_color3 = OpFSub %v4f32 %cval %tmp_color2\n"
		"%tmp_color4 = OpVectorInsertDynamic %v4f32 %tmp_color3 %c_f32_1 %c_i32_3\n"
		"OpStore %vtxColor %tmp_color4\n"
		"OpReturn\n"
		"OpFunctionEnd\n";

	dst.spirvAsmSources.add("frag") <<
		// This is a single module that contains 2 fragment shaders.
		// One that passes color through and the other that inverts the output
		// color (1.0 - color).
		"OpCapability Shader\n"
		"OpMemoryModel Logical GLSL450\n"
		"OpEntryPoint Fragment %main \"frag1\" %vtxColor %fragColor\n"
		"OpEntryPoint Fragment %main2 \"frag2\" %vtxColor %fragColor\n"
		"OpExecutionMode %main OriginUpperLeft\n"
		"OpExecutionMode %main2 OriginUpperLeft\n"

		"OpName %main \"frag1\"\n"
		"OpName %main2 \"frag2\"\n"
		"OpName %fragColor \"fragColor\"\n"
		"OpName %vtxColor \"vtxColor\"\n"
		"OpDecorate %fragColor Location 0\n"
		"OpDecorate %vtxColor Location 1\n"
		SPIRV_ASSEMBLY_TYPES
		SPIRV_ASSEMBLY_CONSTANTS
		SPIRV_ASSEMBLY_ARRAYS
		"%cval = OpConstantComposite %v4f32 %c_f32_1 %c_f32_1 %c_f32_1 %c_f32_0\n"
		"%fragColor = OpVariable %op_v4f32 Output\n"
		"%vtxColor = OpVariable %ip_v4f32 Input\n"

		"%main = OpFunction %void None %fun\n"
		"%label_main = OpLabel\n"
		"%tmp1 = OpLoad %v4f32 %vtxColor\n"
		"OpStore %fragColor %tmp1\n"
		"OpReturn\n"
		"OpFunctionEnd\n"

		"%main2 = OpFunction %void None %fun\n"
		"%label_main2 = OpLabel\n"
		"%tmp2 = OpLoad %v4f32 %vtxColor\n"
		"%tmp3 = OpFSub %v4f32 %cval %tmp2\n"
		"%tmp4 = OpVectorInsertDynamic %v4f32 %tmp3 %c_f32_1 %c_i32_3\n"
		"OpStore %fragColor %tmp4\n"
		"OpReturn\n"
		"OpFunctionEnd\n";

	dst.spirvAsmSources.add("geom") <<
		"OpCapability Geometry\n"
		"OpCapability ClipDistance\n"
		"OpCapability CullDistance\n"
		"OpMemoryModel Logical GLSL450\n"
		"OpEntryPoint Geometry %geom1_main \"geom1\" %out_gl_position %gl_in %out_color %in_color\n"
		"OpEntryPoint Geometry %geom2_main \"geom2\" %out_gl_position %gl_in %out_color %in_color\n"
		"OpExecutionMode %geom1_main Triangles\n"
		"OpExecutionMode %geom2_main Triangles\n"
		"OpExecutionMode %geom1_main OutputTriangleStrip\n"
		"OpExecutionMode %geom2_main OutputTriangleStrip\n"
		"OpExecutionMode %geom1_main OutputVertices 3\n"
		"OpExecutionMode %geom2_main OutputVertices 3\n"
		"OpName %geom1_main \"geom1\"\n"
		"OpName %geom2_main \"geom2\"\n"
		"OpName %per_vertex_in \"gl_PerVertex\"\n"
		"OpMemberName %per_vertex_in 0 \"gl_Position\"\n"
		"OpMemberName %per_vertex_in 1 \"gl_PointSize\"\n"
		"OpMemberName %per_vertex_in 2 \"gl_ClipDistance\"\n"
		"OpMemberName %per_vertex_in 3 \"gl_CullDistance\"\n"
		"OpName %gl_in \"gl_in\"\n"
		"OpName %out_color \"out_color\"\n"
		"OpName %in_color \"in_color\"\n"
		"OpDecorate %out_gl_position BuiltIn Position\n"
		"OpMemberDecorate %per_vertex_in 0 BuiltIn Position\n"
		"OpMemberDecorate %per_vertex_in 1 BuiltIn PointSize\n"
		"OpMemberDecorate %per_vertex_in 2 BuiltIn ClipDistance\n"
		"OpMemberDecorate %per_vertex_in 3 BuiltIn CullDistance\n"
		"OpDecorate %per_vertex_in Block\n"
		"OpDecorate %out_color Location 1\n"
		"OpDecorate %in_color Location 1\n"
		SPIRV_ASSEMBLY_TYPES
		SPIRV_ASSEMBLY_CONSTANTS
		SPIRV_ASSEMBLY_ARRAYS
		"%cval = OpConstantComposite %v4f32 %c_f32_1 %c_f32_1 %c_f32_1 %c_f32_0\n"
		"%per_vertex_in = OpTypeStruct %v4f32 %f32 %a1f32 %a1f32\n"
		"%a3_per_vertex_in = OpTypeArray %per_vertex_in %c_u32_3\n"
		"%ip_a3_per_vertex_in = OpTypePointer Input %a3_per_vertex_in\n"
		"%gl_in = OpVariable %ip_a3_per_vertex_in Input\n"
		"%out_color = OpVariable %op_v4f32 Output\n"
		"%in_color = OpVariable %ip_a3v4f32 Input\n"
		"%out_gl_position = OpVariable %op_v4f32 Output\n"

		"%geom1_main = OpFunction %void None %fun\n"
		"%geom1_label = OpLabel\n"
		"%geom1_gl_in_0_gl_position = OpAccessChain %ip_v4f32 %gl_in %c_i32_0 %c_i32_0\n"
		"%geom1_gl_in_1_gl_position = OpAccessChain %ip_v4f32 %gl_in %c_i32_1 %c_i32_0\n"
		"%geom1_gl_in_2_gl_position = OpAccessChain %ip_v4f32 %gl_in %c_i32_2 %c_i32_0\n"
		"%geom1_in_position_0 = OpLoad %v4f32 %geom1_gl_in_0_gl_position\n"
		"%geom1_in_position_1 = OpLoad %v4f32 %geom1_gl_in_1_gl_position\n"
		"%geom1_in_position_2 = OpLoad %v4f32 %geom1_gl_in_2_gl_position \n"
		"%geom1_in_color_0_ptr = OpAccessChain %ip_v4f32 %in_color %c_i32_0\n"
		"%geom1_in_color_1_ptr = OpAccessChain %ip_v4f32 %in_color %c_i32_1\n"
		"%geom1_in_color_2_ptr = OpAccessChain %ip_v4f32 %in_color %c_i32_2\n"
		"%geom1_in_color_0 = OpLoad %v4f32 %geom1_in_color_0_ptr\n"
		"%geom1_in_color_1 = OpLoad %v4f32 %geom1_in_color_1_ptr\n"
		"%geom1_in_color_2 = OpLoad %v4f32 %geom1_in_color_2_ptr\n"
		"OpStore %out_gl_position %geom1_in_position_0\n"
		"OpStore %out_color %geom1_in_color_0\n"
		"OpEmitVertex\n"
		"OpStore %out_gl_position %geom1_in_position_1\n"
		"OpStore %out_color %geom1_in_color_1\n"
		"OpEmitVertex\n"
		"OpStore %out_gl_position %geom1_in_position_2\n"
		"OpStore %out_color %geom1_in_color_2\n"
		"OpEmitVertex\n"
		"OpEndPrimitive\n"
		"OpReturn\n"
		"OpFunctionEnd\n"

		"%geom2_main = OpFunction %void None %fun\n"
		"%geom2_label = OpLabel\n"
		"%geom2_gl_in_0_gl_position = OpAccessChain %ip_v4f32 %gl_in %c_i32_0 %c_i32_0\n"
		"%geom2_gl_in_1_gl_position = OpAccessChain %ip_v4f32 %gl_in %c_i32_1 %c_i32_0\n"
		"%geom2_gl_in_2_gl_position = OpAccessChain %ip_v4f32 %gl_in %c_i32_2 %c_i32_0\n"
		"%geom2_in_position_0 = OpLoad %v4f32 %geom2_gl_in_0_gl_position\n"
		"%geom2_in_position_1 = OpLoad %v4f32 %geom2_gl_in_1_gl_position\n"
		"%geom2_in_position_2 = OpLoad %v4f32 %geom2_gl_in_2_gl_position \n"
		"%geom2_in_color_0_ptr = OpAccessChain %ip_v4f32 %in_color %c_i32_0\n"
		"%geom2_in_color_1_ptr = OpAccessChain %ip_v4f32 %in_color %c_i32_1\n"
		"%geom2_in_color_2_ptr = OpAccessChain %ip_v4f32 %in_color %c_i32_2\n"
		"%geom2_in_color_0 = OpLoad %v4f32 %geom2_in_color_0_ptr\n"
		"%geom2_in_color_1 = OpLoad %v4f32 %geom2_in_color_1_ptr\n"
		"%geom2_in_color_2 = OpLoad %v4f32 %geom2_in_color_2_ptr\n"
		"%geom2_transformed_in_color_0 = OpFSub %v4f32 %cval %geom2_in_color_0\n"
		"%geom2_transformed_in_color_1 = OpFSub %v4f32 %cval %geom2_in_color_1\n"
		"%geom2_transformed_in_color_2 = OpFSub %v4f32 %cval %geom2_in_color_2\n"
		"%geom2_transformed_in_color_0_a = OpVectorInsertDynamic %v4f32 %geom2_transformed_in_color_0 %c_f32_1 %c_i32_3\n"
		"%geom2_transformed_in_color_1_a = OpVectorInsertDynamic %v4f32 %geom2_transformed_in_color_1 %c_f32_1 %c_i32_3\n"
		"%geom2_transformed_in_color_2_a = OpVectorInsertDynamic %v4f32 %geom2_transformed_in_color_2 %c_f32_1 %c_i32_3\n"
		"OpStore %out_gl_position %geom2_in_position_0\n"
		"OpStore %out_color %geom2_transformed_in_color_0_a\n"
		"OpEmitVertex\n"
		"OpStore %out_gl_position %geom2_in_position_1\n"
		"OpStore %out_color %geom2_transformed_in_color_1_a\n"
		"OpEmitVertex\n"
		"OpStore %out_gl_position %geom2_in_position_2\n"
		"OpStore %out_color %geom2_transformed_in_color_2_a\n"
		"OpEmitVertex\n"
		"OpEndPrimitive\n"
		"OpReturn\n"
		"OpFunctionEnd\n";

	dst.spirvAsmSources.add("tessc") <<
		"OpCapability Tessellation\n"
		"OpMemoryModel Logical GLSL450\n"
		"OpEntryPoint TessellationControl %tessc1_main \"tessc1\" %out_color %gl_InvocationID %in_color %out_position %in_position %gl_TessLevelOuter %gl_TessLevelInner\n"
		"OpEntryPoint TessellationControl %tessc2_main \"tessc2\" %out_color %gl_InvocationID %in_color %out_position %in_position %gl_TessLevelOuter %gl_TessLevelInner\n"
		"OpExecutionMode %tessc1_main OutputVertices 3\n"
		"OpExecutionMode %tessc2_main OutputVertices 3\n"
		"OpName %tessc1_main \"tessc1\"\n"
		"OpName %tessc2_main \"tessc2\"\n"
		"OpName %out_color \"out_color\"\n"
		"OpName %gl_InvocationID \"gl_InvocationID\"\n"
		"OpName %in_color \"in_color\"\n"
		"OpName %out_position \"out_position\"\n"
		"OpName %in_position \"in_position\"\n"
		"OpName %gl_TessLevelOuter \"gl_TessLevelOuter\"\n"
		"OpName %gl_TessLevelInner \"gl_TessLevelInner\"\n"
		"OpDecorate %out_color Location 1\n"
		"OpDecorate %gl_InvocationID BuiltIn InvocationId\n"
		"OpDecorate %in_color Location 1\n"
		"OpDecorate %out_position Location 2\n"
		"OpDecorate %in_position Location 2\n"
		"OpDecorate %gl_TessLevelOuter Patch\n"
		"OpDecorate %gl_TessLevelOuter BuiltIn TessLevelOuter\n"
		"OpDecorate %gl_TessLevelInner Patch\n"
		"OpDecorate %gl_TessLevelInner BuiltIn TessLevelInner\n"
		SPIRV_ASSEMBLY_TYPES
		SPIRV_ASSEMBLY_CONSTANTS
		SPIRV_ASSEMBLY_ARRAYS
		"%cval = OpConstantComposite %v4f32 %c_f32_1 %c_f32_1 %c_f32_1 %c_f32_0\n"
		"%out_color = OpVariable %op_a3v4f32 Output\n"
		"%gl_InvocationID = OpVariable %ip_i32 Input\n"
		"%in_color = OpVariable %ip_a32v4f32 Input\n"
		"%out_position = OpVariable %op_a3v4f32 Output\n"
		"%in_position = OpVariable %ip_a32v4f32 Input\n"
		"%gl_TessLevelOuter = OpVariable %op_a4f32 Output\n"
		"%gl_TessLevelInner = OpVariable %op_a2f32 Output\n"

		"%tessc1_main = OpFunction %void None %fun\n"
		"%tessc1_label = OpLabel\n"
		"%tessc1_invocation_id = OpLoad %i32 %gl_InvocationID\n"
		"%tessc1_in_color_ptr = OpAccessChain %ip_v4f32 %in_color %tessc1_invocation_id\n"
		"%tessc1_in_position_ptr = OpAccessChain %ip_v4f32 %in_position %tessc1_invocation_id\n"
		"%tessc1_in_color_val = OpLoad %v4f32 %tessc1_in_color_ptr\n"
		"%tessc1_in_position_val = OpLoad %v4f32 %tessc1_in_position_ptr\n"
		"%tessc1_out_color_ptr = OpAccessChain %op_v4f32 %out_color %tessc1_invocation_id\n"
		"%tessc1_out_position_ptr = OpAccessChain %op_v4f32 %out_position %tessc1_invocation_id\n"
		"OpStore %tessc1_out_color_ptr %tessc1_in_color_val\n"
		"OpStore %tessc1_out_position_ptr %tessc1_in_position_val\n"
		"%tessc1_is_first_invocation = OpIEqual %bool %tessc1_invocation_id %c_i32_0\n"
		"OpSelectionMerge %tessc1_merge_label None\n"
		"OpBranchConditional %tessc1_is_first_invocation %tessc1_first_invocation %tessc1_merge_label\n"
		"%tessc1_first_invocation = OpLabel\n"
		"%tessc1_tess_outer_0 = OpAccessChain %op_f32 %gl_TessLevelOuter %c_i32_0\n"
		"%tessc1_tess_outer_1 = OpAccessChain %op_f32 %gl_TessLevelOuter %c_i32_1\n"
		"%tessc1_tess_outer_2 = OpAccessChain %op_f32 %gl_TessLevelOuter %c_i32_2\n"
		"%tessc1_tess_inner = OpAccessChain %op_f32 %gl_TessLevelInner %c_i32_0\n"
		"OpStore %tessc1_tess_outer_0 %c_f32_1\n"
		"OpStore %tessc1_tess_outer_1 %c_f32_1\n"
		"OpStore %tessc1_tess_outer_2 %c_f32_1\n"
		"OpStore %tessc1_tess_inner %c_f32_1\n"
		"OpBranch %tessc1_merge_label\n"
		"%tessc1_merge_label = OpLabel\n"
		"OpReturn\n"
		"OpFunctionEnd\n"

		"%tessc2_main = OpFunction %void None %fun\n"
		"%tessc2_label = OpLabel\n"
		"%tessc2_invocation_id = OpLoad %i32 %gl_InvocationID\n"
		"%tessc2_in_color_ptr = OpAccessChain %ip_v4f32 %in_color %tessc2_invocation_id\n"
		"%tessc2_in_position_ptr = OpAccessChain %ip_v4f32 %in_position %tessc2_invocation_id\n"
		"%tessc2_in_color_val = OpLoad %v4f32 %tessc2_in_color_ptr\n"
		"%tessc2_in_position_val = OpLoad %v4f32 %tessc2_in_position_ptr\n"
		"%tessc2_out_color_ptr = OpAccessChain %op_v4f32 %out_color %tessc2_invocation_id\n"
		"%tessc2_out_position_ptr = OpAccessChain %op_v4f32 %out_position %tessc2_invocation_id\n"
		"%tessc2_transformed_color = OpFSub %v4f32 %cval %tessc2_in_color_val\n"
		"%tessc2_transformed_color_a = OpVectorInsertDynamic %v4f32 %tessc2_transformed_color %c_f32_1 %c_i32_3\n"
		"OpStore %tessc2_out_color_ptr %tessc2_transformed_color_a\n"
		"OpStore %tessc2_out_position_ptr %tessc2_in_position_val\n"
		"%tessc2_is_first_invocation = OpIEqual %bool %tessc2_invocation_id %c_i32_0\n"
		"OpSelectionMerge %tessc2_merge_label None\n"
		"OpBranchConditional %tessc2_is_first_invocation %tessc2_first_invocation %tessc2_merge_label\n"
		"%tessc2_first_invocation = OpLabel\n"
		"%tessc2_tess_outer_0 = OpAccessChain %op_f32 %gl_TessLevelOuter %c_i32_0\n"
		"%tessc2_tess_outer_1 = OpAccessChain %op_f32 %gl_TessLevelOuter %c_i32_1\n"
		"%tessc2_tess_outer_2 = OpAccessChain %op_f32 %gl_TessLevelOuter %c_i32_2\n"
		"%tessc2_tess_inner = OpAccessChain %op_f32 %gl_TessLevelInner %c_i32_0\n"
		"OpStore %tessc2_tess_outer_0 %c_f32_1\n"
		"OpStore %tessc2_tess_outer_1 %c_f32_1\n"
		"OpStore %tessc2_tess_outer_2 %c_f32_1\n"
		"OpStore %tessc2_tess_inner %c_f32_1\n"
		"OpBranch %tessc2_merge_label\n"
		"%tessc2_merge_label = OpLabel\n"
		"OpReturn\n"
		"OpFunctionEnd\n";

	dst.spirvAsmSources.add("tesse") <<
		"OpCapability Tessellation\n"
		"OpCapability ClipDistance\n"
		"OpCapability CullDistance\n"
		"OpMemoryModel Logical GLSL450\n"
		"OpEntryPoint TessellationEvaluation %tesse1_main \"tesse1\" %stream %gl_tessCoord %in_position %out_color %in_color \n"
		"OpEntryPoint TessellationEvaluation %tesse2_main \"tesse2\" %stream %gl_tessCoord %in_position %out_color %in_color \n"
		"OpExecutionMode %tesse1_main Triangles\n"
		"OpExecutionMode %tesse2_main Triangles\n"
		"OpName %tesse1_main \"tesse1\"\n"
		"OpName %tesse2_main \"tesse2\"\n"
		"OpName %per_vertex_out \"gl_PerVertex\"\n"
		"OpMemberName %per_vertex_out 0 \"gl_Position\"\n"
		"OpMemberName %per_vertex_out 1 \"gl_PointSize\"\n"
		"OpMemberName %per_vertex_out 2 \"gl_ClipDistance\"\n"
		"OpMemberName %per_vertex_out 3 \"gl_CullDistance\"\n"
		"OpName %stream \"\"\n"
		"OpName %gl_tessCoord \"gl_TessCoord\"\n"
		"OpName %in_position \"in_position\"\n"
		"OpName %out_color \"out_color\"\n"
		"OpName %in_color \"in_color\"\n"
		"OpMemberDecorate %per_vertex_out 0 BuiltIn Position\n"
		"OpMemberDecorate %per_vertex_out 1 BuiltIn PointSize\n"
		"OpMemberDecorate %per_vertex_out 2 BuiltIn ClipDistance\n"
		"OpMemberDecorate %per_vertex_out 3 BuiltIn CullDistance\n"
		"OpDecorate %per_vertex_out Block\n"
		"OpDecorate %gl_tessCoord BuiltIn TessCoord\n"
		"OpDecorate %in_position Location 2\n"
		"OpDecorate %out_color Location 1\n"
		"OpDecorate %in_color Location 1\n"
		SPIRV_ASSEMBLY_TYPES
		SPIRV_ASSEMBLY_CONSTANTS
		SPIRV_ASSEMBLY_ARRAYS
		"%cval = OpConstantComposite %v4f32 %c_f32_1 %c_f32_1 %c_f32_1 %c_f32_0\n"
		"%per_vertex_out = OpTypeStruct %v4f32 %f32 %a1f32 %a1f32\n"
		"%op_per_vertex_out = OpTypePointer Output %per_vertex_out\n"
		"%stream = OpVariable %op_per_vertex_out Output\n"
		"%gl_tessCoord = OpVariable %ip_v3f32 Input\n"
		"%in_position = OpVariable %ip_a32v4f32 Input\n"
		"%out_color = OpVariable %op_v4f32 Output\n"
		"%in_color = OpVariable %ip_a32v4f32 Input\n"

		"%tesse1_main = OpFunction %void None %fun\n"
		"%tesse1_label = OpLabel\n"
		"%tesse1_tc_0_ptr = OpAccessChain %ip_f32 %gl_tessCoord %c_u32_0\n"
		"%tesse1_tc_1_ptr = OpAccessChain %ip_f32 %gl_tessCoord %c_u32_1\n"
		"%tesse1_tc_2_ptr = OpAccessChain %ip_f32 %gl_tessCoord %c_u32_2\n"
		"%tesse1_tc_0 = OpLoad %f32 %tesse1_tc_0_ptr\n"
		"%tesse1_tc_1 = OpLoad %f32 %tesse1_tc_1_ptr\n"
		"%tesse1_tc_2 = OpLoad %f32 %tesse1_tc_2_ptr\n"
		"%tesse1_in_pos_0_ptr = OpAccessChain %ip_v4f32 %in_position %c_i32_0\n"
		"%tesse1_in_pos_1_ptr = OpAccessChain %ip_v4f32 %in_position %c_i32_1\n"
		"%tesse1_in_pos_2_ptr = OpAccessChain %ip_v4f32 %in_position %c_i32_2\n"
		"%tesse1_in_pos_0 = OpLoad %v4f32 %tesse1_in_pos_0_ptr\n"
		"%tesse1_in_pos_1 = OpLoad %v4f32 %tesse1_in_pos_1_ptr\n"
		"%tesse1_in_pos_2 = OpLoad %v4f32 %tesse1_in_pos_2_ptr\n"
		"%tesse1_in_pos_0_weighted = OpVectorTimesScalar %v4f32 %tesse1_tc_0 %tesse1_in_pos_0\n"
		"%tesse1_in_pos_1_weighted = OpVectorTimesScalar %v4f32 %tesse1_tc_1 %tesse1_in_pos_1\n"
		"%tesse1_in_pos_2_weighted = OpVectorTimesScalar %v4f32 %tesse1_tc_2 %tesse1_in_pos_2\n"
		"%tesse1_out_pos_ptr = OpAccessChain %op_v4f32 %stream %c_i32_0\n"
		"%tesse1_in_pos_0_plus_pos_1 = OpFAdd %v4f32 %tesse1_in_pos_0_weighted %tesse1_in_pos_1_weighted\n"
		"%tesse1_computed_out = OpFAdd %v4f32 %tesse1_in_pos_0_plus_pos_1 %tesse1_in_pos_2_weighted\n"
		"OpStore %tesse1_out_pos_ptr %tesse1_computed_out\n"
		"%tesse1_in_clr_0_ptr = OpAccessChain %ip_v4f32 %in_color %c_i32_0\n"
		"%tesse1_in_clr_1_ptr = OpAccessChain %ip_v4f32 %in_color %c_i32_1\n"
		"%tesse1_in_clr_2_ptr = OpAccessChain %ip_v4f32 %in_color %c_i32_2\n"
		"%tesse1_in_clr_0 = OpLoad %v4f32 %tesse1_in_clr_0_ptr\n"
		"%tesse1_in_clr_1 = OpLoad %v4f32 %tesse1_in_clr_1_ptr\n"
		"%tesse1_in_clr_2 = OpLoad %v4f32 %tesse1_in_clr_2_ptr\n"
		"%tesse1_in_clr_0_weighted = OpVectorTimesScalar %v4f32 %tesse1_tc_0 %tesse1_in_clr_0\n"
		"%tesse1_in_clr_1_weighted = OpVectorTimesScalar %v4f32 %tesse1_tc_1 %tesse1_in_clr_1\n"
		"%tesse1_in_clr_2_weighted = OpVectorTimesScalar %v4f32 %tesse1_tc_2 %tesse1_in_clr_2\n"
		"%tesse1_in_clr_0_plus_col_1 = OpFAdd %v4f32 %tesse1_in_clr_0_weighted %tesse1_in_clr_1_weighted\n"
		"%tesse1_computed_clr = OpFAdd %v4f32 %tesse1_in_clr_0_plus_col_1 %tesse1_in_clr_2_weighted\n"
		"OpStore %out_color %tesse1_computed_clr\n"
		"OpReturn\n"
		"OpFunctionEnd\n"

		"%tesse2_main = OpFunction %void None %fun\n"
		"%tesse2_label = OpLabel\n"
		"%tesse2_tc_0_ptr = OpAccessChain %ip_f32 %gl_tessCoord %c_u32_0\n"
		"%tesse2_tc_1_ptr = OpAccessChain %ip_f32 %gl_tessCoord %c_u32_1\n"
		"%tesse2_tc_2_ptr = OpAccessChain %ip_f32 %gl_tessCoord %c_u32_2\n"
		"%tesse2_tc_0 = OpLoad %f32 %tesse2_tc_0_ptr\n"
		"%tesse2_tc_1 = OpLoad %f32 %tesse2_tc_1_ptr\n"
		"%tesse2_tc_2 = OpLoad %f32 %tesse2_tc_2_ptr\n"
		"%tesse2_in_pos_0_ptr = OpAccessChain %ip_v4f32 %in_position %c_i32_0\n"
		"%tesse2_in_pos_1_ptr = OpAccessChain %ip_v4f32 %in_position %c_i32_1\n"
		"%tesse2_in_pos_2_ptr = OpAccessChain %ip_v4f32 %in_position %c_i32_2\n"
		"%tesse2_in_pos_0 = OpLoad %v4f32 %tesse2_in_pos_0_ptr\n"
		"%tesse2_in_pos_1 = OpLoad %v4f32 %tesse2_in_pos_1_ptr\n"
		"%tesse2_in_pos_2 = OpLoad %v4f32 %tesse2_in_pos_2_ptr\n"
		"%tesse2_in_pos_0_weighted = OpVectorTimesScalar %v4f32 %tesse2_tc_0 %tesse2_in_pos_0\n"
		"%tesse2_in_pos_1_weighted = OpVectorTimesScalar %v4f32 %tesse2_tc_1 %tesse2_in_pos_1\n"
		"%tesse2_in_pos_2_weighted = OpVectorTimesScalar %v4f32 %tesse2_tc_2 %tesse2_in_pos_2\n"
		"%tesse2_out_pos_ptr = OpAccessChain %op_v4f32 %stream %c_i32_0\n"
		"%tesse2_in_pos_0_plus_pos_1 = OpFAdd %v4f32 %tesse2_in_pos_0_weighted %tesse2_in_pos_1_weighted\n"
		"%tesse2_computed_out = OpFAdd %v4f32 %tesse2_in_pos_0_plus_pos_1 %tesse2_in_pos_2_weighted\n"
		"OpStore %tesse2_out_pos_ptr %tesse2_computed_out\n"
		"%tesse2_in_clr_0_ptr = OpAccessChain %ip_v4f32 %in_color %c_i32_0\n"
		"%tesse2_in_clr_1_ptr = OpAccessChain %ip_v4f32 %in_color %c_i32_1\n"
		"%tesse2_in_clr_2_ptr = OpAccessChain %ip_v4f32 %in_color %c_i32_2\n"
		"%tesse2_in_clr_0 = OpLoad %v4f32 %tesse2_in_clr_0_ptr\n"
		"%tesse2_in_clr_1 = OpLoad %v4f32 %tesse2_in_clr_1_ptr\n"
		"%tesse2_in_clr_2 = OpLoad %v4f32 %tesse2_in_clr_2_ptr\n"
		"%tesse2_in_clr_0_weighted = OpVectorTimesScalar %v4f32 %tesse2_tc_0 %tesse2_in_clr_0\n"
		"%tesse2_in_clr_1_weighted = OpVectorTimesScalar %v4f32 %tesse2_tc_1 %tesse2_in_clr_1\n"
		"%tesse2_in_clr_2_weighted = OpVectorTimesScalar %v4f32 %tesse2_tc_2 %tesse2_in_clr_2\n"
		"%tesse2_in_clr_0_plus_col_1 = OpFAdd %v4f32 %tesse2_in_clr_0_weighted %tesse2_in_clr_1_weighted\n"
		"%tesse2_computed_clr = OpFAdd %v4f32 %tesse2_in_clr_0_plus_col_1 %tesse2_in_clr_2_weighted\n"
		"%tesse2_clr_transformed = OpFSub %v4f32 %cval %tesse2_computed_clr\n"
		"%tesse2_clr_transformed_a = OpVectorInsertDynamic %v4f32 %tesse2_clr_transformed %c_f32_1 %c_i32_3\n"
		"OpStore %out_color %tesse2_clr_transformed_a\n"
		"OpReturn\n"
		"OpFunctionEnd\n";
}

// Sets up and runs a Vulkan pipeline, then spot-checks the resulting image.
// Feeds the pipeline a set of colored triangles, which then must occur in the
// rendered image.  The surface is cleared before executing the pipeline, so
// whatever the shaders draw can be directly spot-checked.
TestStatus runAndVerifyDefaultPipeline (Context& context, InstanceContext instance)
{
	const VkDevice								vkDevice				= context.getDevice();
	const DeviceInterface&						vk						= context.getDeviceInterface();
	const VkQueue								queue					= context.getUniversalQueue();
	const deUint32								queueFamilyIndex		= context.getUniversalQueueFamilyIndex();
	const tcu::UVec2							renderSize				(256, 256);
	vector<ModuleHandleSp>						modules;
	map<VkShaderStageFlagBits, VkShaderModule>	moduleByStage;
	const int									testSpecificSeed		= 31354125;
	const int									seed					= context.getTestContext().getCommandLine().getBaseSeed() ^ testSpecificSeed;
	bool										supportsGeometry		= false;
	bool										supportsTessellation	= false;
	bool										hasTessellation         = false;

	const VkPhysicalDeviceFeatures&				features				= context.getDeviceFeatures();
	supportsGeometry		= features.geometryShader == VK_TRUE;
	supportsTessellation	= features.tessellationShader == VK_TRUE;
	hasTessellation			= (instance.requiredStages & VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT) ||
								(instance.requiredStages & VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT);

	if (hasTessellation && !supportsTessellation)
	{
		throw tcu::NotSupportedError(std::string("Tessellation not supported"));
	}

	if ((instance.requiredStages & VK_SHADER_STAGE_GEOMETRY_BIT) &&
		!supportsGeometry)
	{
		throw tcu::NotSupportedError(std::string("Geometry not supported"));
	}

	de::Random(seed).shuffle(instance.inputColors, instance.inputColors+4);
	de::Random(seed).shuffle(instance.outputColors, instance.outputColors+4);
	const Vec4								vertexData[]			=
	{
		// Upper left corner:
		Vec4(-1.0f, -1.0f, 0.0f, 1.0f), instance.inputColors[0].toVec(),
		Vec4(-0.5f, -1.0f, 0.0f, 1.0f), instance.inputColors[0].toVec(),
		Vec4(-1.0f, -0.5f, 0.0f, 1.0f), instance.inputColors[0].toVec(),

		// Upper right corner:
		Vec4(+0.5f, -1.0f, 0.0f, 1.0f), instance.inputColors[1].toVec(),
		Vec4(+1.0f, -1.0f, 0.0f, 1.0f), instance.inputColors[1].toVec(),
		Vec4(+1.0f, -0.5f, 0.0f, 1.0f), instance.inputColors[1].toVec(),

		// Lower left corner:
		Vec4(-1.0f, +0.5f, 0.0f, 1.0f), instance.inputColors[2].toVec(),
		Vec4(-0.5f, +1.0f, 0.0f, 1.0f), instance.inputColors[2].toVec(),
		Vec4(-1.0f, +1.0f, 0.0f, 1.0f), instance.inputColors[2].toVec(),

		// Lower right corner:
		Vec4(+1.0f, +0.5f, 0.0f, 1.0f), instance.inputColors[3].toVec(),
		Vec4(+1.0f, +1.0f, 0.0f, 1.0f), instance.inputColors[3].toVec(),
		Vec4(+0.5f, +1.0f, 0.0f, 1.0f), instance.inputColors[3].toVec()
	};
	const size_t							singleVertexDataSize	= 2 * sizeof(Vec4);
	const size_t							vertexCount				= sizeof(vertexData) / singleVertexDataSize;

	const VkBufferCreateInfo				vertexBufferParams		=
	{
		VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,	//	VkStructureType		sType;
		DE_NULL,								//	const void*			pNext;
		0u,										//	VkBufferCreateFlags	flags;
		(VkDeviceSize)sizeof(vertexData),		//	VkDeviceSize		size;
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,		//	VkBufferUsageFlags	usage;
		VK_SHARING_MODE_EXCLUSIVE,				//	VkSharingMode		sharingMode;
		1u,										//	deUint32			queueFamilyCount;
		&queueFamilyIndex,						//	const deUint32*		pQueueFamilyIndices;
	};
	const Unique<VkBuffer>					vertexBuffer			(createBuffer(vk, vkDevice, &vertexBufferParams));
	const UniquePtr<Allocation>				vertexBufferMemory		(context.getDefaultAllocator().allocate(getBufferMemoryRequirements(vk, vkDevice, *vertexBuffer), MemoryRequirement::HostVisible));

	VK_CHECK(vk.bindBufferMemory(vkDevice, *vertexBuffer, vertexBufferMemory->getMemory(), vertexBufferMemory->getOffset()));

	const VkDeviceSize						imageSizeBytes			= (VkDeviceSize)(sizeof(deUint32)*renderSize.x()*renderSize.y());
	const VkBufferCreateInfo				readImageBufferParams	=
	{
		VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,		//	VkStructureType		sType;
		DE_NULL,									//	const void*			pNext;
		0u,											//	VkBufferCreateFlags	flags;
		imageSizeBytes,								//	VkDeviceSize		size;
		VK_BUFFER_USAGE_TRANSFER_DST_BIT,			//	VkBufferUsageFlags	usage;
		VK_SHARING_MODE_EXCLUSIVE,					//	VkSharingMode		sharingMode;
		1u,											//	deUint32			queueFamilyCount;
		&queueFamilyIndex,							//	const deUint32*		pQueueFamilyIndices;
	};
	const Unique<VkBuffer>					readImageBuffer			(createBuffer(vk, vkDevice, &readImageBufferParams));
	const UniquePtr<Allocation>				readImageBufferMemory	(context.getDefaultAllocator().allocate(getBufferMemoryRequirements(vk, vkDevice, *readImageBuffer), MemoryRequirement::HostVisible));

	VK_CHECK(vk.bindBufferMemory(vkDevice, *readImageBuffer, readImageBufferMemory->getMemory(), readImageBufferMemory->getOffset()));

	const VkImageCreateInfo					imageParams				=
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,									//	VkStructureType		sType;
		DE_NULL,																//	const void*			pNext;
		0u,																		//	VkImageCreateFlags	flags;
		VK_IMAGE_TYPE_2D,														//	VkImageType			imageType;
		VK_FORMAT_R8G8B8A8_UNORM,												//	VkFormat			format;
		{ renderSize.x(), renderSize.y(), 1 },									//	VkExtent3D			extent;
		1u,																		//	deUint32			mipLevels;
		1u,																		//	deUint32			arraySize;
		VK_SAMPLE_COUNT_1_BIT,													//	deUint32			samples;
		VK_IMAGE_TILING_OPTIMAL,												//	VkImageTiling		tiling;
		VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT,	//	VkImageUsageFlags	usage;
		VK_SHARING_MODE_EXCLUSIVE,												//	VkSharingMode		sharingMode;
		1u,																		//	deUint32			queueFamilyCount;
		&queueFamilyIndex,														//	const deUint32*		pQueueFamilyIndices;
		VK_IMAGE_LAYOUT_UNDEFINED,												//	VkImageLayout		initialLayout;
	};

	const Unique<VkImage>					image					(createImage(vk, vkDevice, &imageParams));
	const UniquePtr<Allocation>				imageMemory				(context.getDefaultAllocator().allocate(getImageMemoryRequirements(vk, vkDevice, *image), MemoryRequirement::Any));

	VK_CHECK(vk.bindImageMemory(vkDevice, *image, imageMemory->getMemory(), imageMemory->getOffset()));

	const VkAttachmentDescription			colorAttDesc			=
	{
		0u,												//	VkAttachmentDescriptionFlags	flags;
		VK_FORMAT_R8G8B8A8_UNORM,						//	VkFormat						format;
		VK_SAMPLE_COUNT_1_BIT,							//	deUint32						samples;
		VK_ATTACHMENT_LOAD_OP_CLEAR,					//	VkAttachmentLoadOp				loadOp;
		VK_ATTACHMENT_STORE_OP_STORE,					//	VkAttachmentStoreOp				storeOp;
		VK_ATTACHMENT_LOAD_OP_DONT_CARE,				//	VkAttachmentLoadOp				stencilLoadOp;
		VK_ATTACHMENT_STORE_OP_DONT_CARE,				//	VkAttachmentStoreOp				stencilStoreOp;
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,		//	VkImageLayout					initialLayout;
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,		//	VkImageLayout					finalLayout;
	};
	const VkAttachmentReference				colorAttRef				=
	{
		0u,												//	deUint32		attachment;
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,		//	VkImageLayout	layout;
	};
	const VkSubpassDescription				subpassDesc				=
	{
		0u,												//	VkSubpassDescriptionFlags		flags;
		VK_PIPELINE_BIND_POINT_GRAPHICS,				//	VkPipelineBindPoint				pipelineBindPoint;
		0u,												//	deUint32						inputCount;
		DE_NULL,										//	const VkAttachmentReference*	pInputAttachments;
		1u,												//	deUint32						colorCount;
		&colorAttRef,									//	const VkAttachmentReference*	pColorAttachments;
		DE_NULL,										//	const VkAttachmentReference*	pResolveAttachments;
		DE_NULL,										//	const VkAttachmentReference*	pDepthStencilAttachment;
		0u,												//	deUint32						preserveCount;
		DE_NULL,										//	const VkAttachmentReference*	pPreserveAttachments;

	};
	const VkRenderPassCreateInfo			renderPassParams		=
	{
		VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,		//	VkStructureType					sType;
		DE_NULL,										//	const void*						pNext;
		(VkRenderPassCreateFlags)0,
		1u,												//	deUint32						attachmentCount;
		&colorAttDesc,									//	const VkAttachmentDescription*	pAttachments;
		1u,												//	deUint32						subpassCount;
		&subpassDesc,									//	const VkSubpassDescription*		pSubpasses;
		0u,												//	deUint32						dependencyCount;
		DE_NULL,										//	const VkSubpassDependency*		pDependencies;
	};
	const Unique<VkRenderPass>				renderPass				(createRenderPass(vk, vkDevice, &renderPassParams));

	const VkImageViewCreateInfo				colorAttViewParams		=
	{
		VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,		//	VkStructureType				sType;
		DE_NULL,										//	const void*					pNext;
		0u,												//	VkImageViewCreateFlags		flags;
		*image,											//	VkImage						image;
		VK_IMAGE_VIEW_TYPE_2D,							//	VkImageViewType				viewType;
		VK_FORMAT_R8G8B8A8_UNORM,						//	VkFormat					format;
		{
			VK_COMPONENT_SWIZZLE_R,
			VK_COMPONENT_SWIZZLE_G,
			VK_COMPONENT_SWIZZLE_B,
			VK_COMPONENT_SWIZZLE_A
		},												//	VkChannelMapping			channels;
		{
			VK_IMAGE_ASPECT_COLOR_BIT,						//	VkImageAspectFlags	aspectMask;
			0u,												//	deUint32			baseMipLevel;
			1u,												//	deUint32			mipLevels;
			0u,												//	deUint32			baseArrayLayer;
			1u,												//	deUint32			arraySize;
		},												//	VkImageSubresourceRange		subresourceRange;
	};
	const Unique<VkImageView>				colorAttView			(createImageView(vk, vkDevice, &colorAttViewParams));


	// Pipeline layout
	const VkPipelineLayoutCreateInfo		pipelineLayoutParams	=
	{
		VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,			//	VkStructureType					sType;
		DE_NULL,												//	const void*						pNext;
		(VkPipelineLayoutCreateFlags)0,
		0u,														//	deUint32						descriptorSetCount;
		DE_NULL,												//	const VkDescriptorSetLayout*	pSetLayouts;
		0u,														//	deUint32						pushConstantRangeCount;
		DE_NULL,												//	const VkPushConstantRange*		pPushConstantRanges;
	};
	const Unique<VkPipelineLayout>			pipelineLayout			(createPipelineLayout(vk, vkDevice, &pipelineLayoutParams));

	// Pipeline
	vector<VkPipelineShaderStageCreateInfo>		shaderStageParams;
	// We need these vectors to make sure that information about specialization constants for each stage can outlive createGraphicsPipeline().
	vector<vector<VkSpecializationMapEntry> >	specConstantEntries;
	vector<VkSpecializationInfo>				specializationInfos;
	createPipelineShaderStages(vk, vkDevice, instance, context, modules, shaderStageParams);

	// And we don't want the reallocation of these vectors to invalidate pointers pointing to their contents.
	specConstantEntries.reserve(shaderStageParams.size());
	specializationInfos.reserve(shaderStageParams.size());

	// Patch the specialization info field in PipelineShaderStageCreateInfos.
	for (vector<VkPipelineShaderStageCreateInfo>::iterator stageInfo = shaderStageParams.begin(); stageInfo != shaderStageParams.end(); ++stageInfo)
	{
		const StageToSpecConstantMap::const_iterator stageIt = instance.specConstants.find(stageInfo->stage);

		if (stageIt != instance.specConstants.end())
		{
			const size_t						numSpecConstants	= stageIt->second.size();
			vector<VkSpecializationMapEntry>	entries;
			VkSpecializationInfo				specInfo;

			entries.resize(numSpecConstants);

			// Only support 32-bit integers as spec constants now. And their constant IDs are numbered sequentially starting from 0.
			for (size_t ndx = 0; ndx < numSpecConstants; ++ndx)
			{
				entries[ndx].constantID	= (deUint32)ndx;
				entries[ndx].offset		= deUint32(ndx * sizeof(deInt32));
				entries[ndx].size		= sizeof(deInt32);
			}

			specConstantEntries.push_back(entries);

			specInfo.mapEntryCount	= (deUint32)numSpecConstants;
			specInfo.pMapEntries	= specConstantEntries.back().data();
			specInfo.dataSize		= numSpecConstants * sizeof(deInt32);
			specInfo.pData			= stageIt->second.data();
			specializationInfos.push_back(specInfo);

			stageInfo->pSpecializationInfo = &specializationInfos.back();
		}
	}
	const VkPipelineDepthStencilStateCreateInfo	depthStencilParams		=
	{
		VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,	//	VkStructureType		sType;
		DE_NULL,													//	const void*			pNext;
		(VkPipelineDepthStencilStateCreateFlags)0,
		DE_FALSE,													//	deUint32			depthTestEnable;
		DE_FALSE,													//	deUint32			depthWriteEnable;
		VK_COMPARE_OP_ALWAYS,										//	VkCompareOp			depthCompareOp;
		DE_FALSE,													//	deUint32			depthBoundsTestEnable;
		DE_FALSE,													//	deUint32			stencilTestEnable;
		{
			VK_STENCIL_OP_KEEP,											//	VkStencilOp	stencilFailOp;
			VK_STENCIL_OP_KEEP,											//	VkStencilOp	stencilPassOp;
			VK_STENCIL_OP_KEEP,											//	VkStencilOp	stencilDepthFailOp;
			VK_COMPARE_OP_ALWAYS,										//	VkCompareOp	stencilCompareOp;
			0u,															//	deUint32	stencilCompareMask;
			0u,															//	deUint32	stencilWriteMask;
			0u,															//	deUint32	stencilReference;
		},															//	VkStencilOpState	front;
		{
			VK_STENCIL_OP_KEEP,											//	VkStencilOp	stencilFailOp;
			VK_STENCIL_OP_KEEP,											//	VkStencilOp	stencilPassOp;
			VK_STENCIL_OP_KEEP,											//	VkStencilOp	stencilDepthFailOp;
			VK_COMPARE_OP_ALWAYS,										//	VkCompareOp	stencilCompareOp;
			0u,															//	deUint32	stencilCompareMask;
			0u,															//	deUint32	stencilWriteMask;
			0u,															//	deUint32	stencilReference;
		},															//	VkStencilOpState	back;
		-1.0f,														//	float				minDepthBounds;
		+1.0f,														//	float				maxDepthBounds;
	};
	const VkViewport						viewport0				=
	{
		0.0f,														//	float	originX;
		0.0f,														//	float	originY;
		(float)renderSize.x(),										//	float	width;
		(float)renderSize.y(),										//	float	height;
		0.0f,														//	float	minDepth;
		1.0f,														//	float	maxDepth;
	};
	const VkRect2D							scissor0				=
	{
		{
			0u,															//	deInt32	x;
			0u,															//	deInt32	y;
		},															//	VkOffset2D	offset;
		{
			renderSize.x(),												//	deInt32	width;
			renderSize.y(),												//	deInt32	height;
		},															//	VkExtent2D	extent;
	};
	const VkPipelineViewportStateCreateInfo		viewportParams			=
	{
		VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,		//	VkStructureType		sType;
		DE_NULL,													//	const void*			pNext;
		(VkPipelineViewportStateCreateFlags)0,
		1u,															//	deUint32			viewportCount;
		&viewport0,
		1u,
		&scissor0
	};
	const VkSampleMask							sampleMask				= ~0u;
	const VkPipelineMultisampleStateCreateInfo	multisampleParams		=
	{
		VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,	//	VkStructureType			sType;
		DE_NULL,													//	const void*				pNext;
		(VkPipelineMultisampleStateCreateFlags)0,
		VK_SAMPLE_COUNT_1_BIT,										//	VkSampleCountFlagBits	rasterSamples;
		DE_FALSE,													//	deUint32				sampleShadingEnable;
		0.0f,														//	float					minSampleShading;
		&sampleMask,												//	const VkSampleMask*		pSampleMask;
		DE_FALSE,													//	VkBool32				alphaToCoverageEnable;
		DE_FALSE,													//	VkBool32				alphaToOneEnable;
	};
	const VkPipelineRasterizationStateCreateInfo	rasterParams		=
	{
		VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,	//	VkStructureType	sType;
		DE_NULL,													//	const void*		pNext;
		(VkPipelineRasterizationStateCreateFlags)0,
		DE_TRUE,													//	deUint32		depthClipEnable;
		DE_FALSE,													//	deUint32		rasterizerDiscardEnable;
		VK_POLYGON_MODE_FILL,										//	VkFillMode		fillMode;
		VK_CULL_MODE_NONE,											//	VkCullMode		cullMode;
		VK_FRONT_FACE_COUNTER_CLOCKWISE,							//	VkFrontFace		frontFace;
		VK_FALSE,													//	VkBool32		depthBiasEnable;
		0.0f,														//	float			depthBias;
		0.0f,														//	float			depthBiasClamp;
		0.0f,														//	float			slopeScaledDepthBias;
		1.0f,														//	float			lineWidth;
	};
	const VkPrimitiveTopology topology = hasTessellation? VK_PRIMITIVE_TOPOLOGY_PATCH_LIST: VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	const VkPipelineInputAssemblyStateCreateInfo	inputAssemblyParams	=
	{
		VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,	//	VkStructureType		sType;
		DE_NULL,														//	const void*			pNext;
		(VkPipelineInputAssemblyStateCreateFlags)0,
		topology,														//	VkPrimitiveTopology	topology;
		DE_FALSE,														//	deUint32			primitiveRestartEnable;
	};
	const VkVertexInputBindingDescription		vertexBinding0 =
	{
		0u,									// deUint32					binding;
		deUint32(singleVertexDataSize),		// deUint32					strideInBytes;
		VK_VERTEX_INPUT_RATE_VERTEX			// VkVertexInputStepRate	stepRate;
	};
	const VkVertexInputAttributeDescription		vertexAttrib0[2] =
	{
		{
			0u,									// deUint32	location;
			0u,									// deUint32	binding;
			VK_FORMAT_R32G32B32A32_SFLOAT,		// VkFormat	format;
			0u									// deUint32	offsetInBytes;
		},
		{
			1u,									// deUint32	location;
			0u,									// deUint32	binding;
			VK_FORMAT_R32G32B32A32_SFLOAT,		// VkFormat	format;
			sizeof(Vec4),						// deUint32	offsetInBytes;
		}
	};

	const VkPipelineVertexInputStateCreateInfo	vertexInputStateParams	=
	{
		VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,	//	VkStructureType								sType;
		DE_NULL,													//	const void*									pNext;
		(VkPipelineVertexInputStateCreateFlags)0,
		1u,															//	deUint32									bindingCount;
		&vertexBinding0,											//	const VkVertexInputBindingDescription*		pVertexBindingDescriptions;
		2u,															//	deUint32									attributeCount;
		vertexAttrib0,												//	const VkVertexInputAttributeDescription*	pVertexAttributeDescriptions;
	};
	const VkPipelineColorBlendAttachmentState	attBlendParams			=
	{
		DE_FALSE,													//	deUint32		blendEnable;
		VK_BLEND_FACTOR_ONE,										//	VkBlend			srcBlendColor;
		VK_BLEND_FACTOR_ZERO,										//	VkBlend			destBlendColor;
		VK_BLEND_OP_ADD,											//	VkBlendOp		blendOpColor;
		VK_BLEND_FACTOR_ONE,										//	VkBlend			srcBlendAlpha;
		VK_BLEND_FACTOR_ZERO,										//	VkBlend			destBlendAlpha;
		VK_BLEND_OP_ADD,											//	VkBlendOp		blendOpAlpha;
		(VK_COLOR_COMPONENT_R_BIT|
		 VK_COLOR_COMPONENT_G_BIT|
		 VK_COLOR_COMPONENT_B_BIT|
		 VK_COLOR_COMPONENT_A_BIT),									//	VkChannelFlags	channelWriteMask;
	};
	const VkPipelineColorBlendStateCreateInfo	blendParams				=
	{
		VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,	//	VkStructureType								sType;
		DE_NULL,													//	const void*									pNext;
		(VkPipelineColorBlendStateCreateFlags)0,
		DE_FALSE,													//	VkBool32									logicOpEnable;
		VK_LOGIC_OP_COPY,											//	VkLogicOp									logicOp;
		1u,															//	deUint32									attachmentCount;
		&attBlendParams,											//	const VkPipelineColorBlendAttachmentState*	pAttachments;
		{ 0.0f, 0.0f, 0.0f, 0.0f },									//	float										blendConst[4];
	};
	const VkPipelineTessellationStateCreateInfo	tessellationState	=
	{
		VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO,
		DE_NULL,
		(VkPipelineTessellationStateCreateFlags)0,
		3u
	};

	const VkPipelineTessellationStateCreateInfo* tessellationInfo	=	hasTessellation ? &tessellationState: DE_NULL;
	const VkGraphicsPipelineCreateInfo		pipelineParams			=
	{
		VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,		//	VkStructureType									sType;
		DE_NULL,												//	const void*										pNext;
		0u,														//	VkPipelineCreateFlags							flags;
		(deUint32)shaderStageParams.size(),						//	deUint32										stageCount;
		&shaderStageParams[0],									//	const VkPipelineShaderStageCreateInfo*			pStages;
		&vertexInputStateParams,								//	const VkPipelineVertexInputStateCreateInfo*		pVertexInputState;
		&inputAssemblyParams,									//	const VkPipelineInputAssemblyStateCreateInfo*	pInputAssemblyState;
		tessellationInfo,										//	const VkPipelineTessellationStateCreateInfo*	pTessellationState;
		&viewportParams,										//	const VkPipelineViewportStateCreateInfo*		pViewportState;
		&rasterParams,											//	const VkPipelineRasterStateCreateInfo*			pRasterState;
		&multisampleParams,										//	const VkPipelineMultisampleStateCreateInfo*		pMultisampleState;
		&depthStencilParams,									//	const VkPipelineDepthStencilStateCreateInfo*	pDepthStencilState;
		&blendParams,											//	const VkPipelineColorBlendStateCreateInfo*		pColorBlendState;
		(const VkPipelineDynamicStateCreateInfo*)DE_NULL,		//	const VkPipelineDynamicStateCreateInfo*			pDynamicState;
		*pipelineLayout,										//	VkPipelineLayout								layout;
		*renderPass,											//	VkRenderPass									renderPass;
		0u,														//	deUint32										subpass;
		DE_NULL,												//	VkPipeline										basePipelineHandle;
		0u,														//	deInt32											basePipelineIndex;
	};

	const Unique<VkPipeline>				pipeline				(createGraphicsPipeline(vk, vkDevice, DE_NULL, &pipelineParams));

	// Framebuffer
	const VkFramebufferCreateInfo			framebufferParams		=
	{
		VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,				//	VkStructureType		sType;
		DE_NULL,												//	const void*			pNext;
		(VkFramebufferCreateFlags)0,
		*renderPass,											//	VkRenderPass		renderPass;
		1u,														//	deUint32			attachmentCount;
		&*colorAttView,											//	const VkImageView*	pAttachments;
		(deUint32)renderSize.x(),								//	deUint32			width;
		(deUint32)renderSize.y(),								//	deUint32			height;
		1u,														//	deUint32			layers;
	};
	const Unique<VkFramebuffer>				framebuffer				(createFramebuffer(vk, vkDevice, &framebufferParams));

	const VkCommandPoolCreateInfo			cmdPoolParams			=
	{
		VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,					//	VkStructureType			sType;
		DE_NULL,													//	const void*				pNext;
		VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,				//	VkCmdPoolCreateFlags	flags;
		queueFamilyIndex,											//	deUint32				queueFamilyIndex;
	};
	const Unique<VkCommandPool>				cmdPool					(createCommandPool(vk, vkDevice, &cmdPoolParams));

	// Command buffer
	const VkCommandBufferAllocateInfo		cmdBufParams			=
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,			//	VkStructureType			sType;
		DE_NULL,												//	const void*				pNext;
		*cmdPool,												//	VkCmdPool				pool;
		VK_COMMAND_BUFFER_LEVEL_PRIMARY,						//	VkCmdBufferLevel		level;
		1u,														//	deUint32				count;
	};
	const Unique<VkCommandBuffer>			cmdBuf					(allocateCommandBuffer(vk, vkDevice, &cmdBufParams));

	const VkCommandBufferBeginInfo			cmdBufBeginParams		=
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,			//	VkStructureType				sType;
		DE_NULL,												//	const void*					pNext;
		(VkCommandBufferUsageFlags)0,
		(const VkCommandBufferInheritanceInfo*)DE_NULL,
	};

	// Record commands
	VK_CHECK(vk.beginCommandBuffer(*cmdBuf, &cmdBufBeginParams));

	{
		const VkMemoryBarrier		vertFlushBarrier	=
		{
			VK_STRUCTURE_TYPE_MEMORY_BARRIER,			//	VkStructureType		sType;
			DE_NULL,									//	const void*			pNext;
			VK_ACCESS_HOST_WRITE_BIT,					//	VkMemoryOutputFlags	outputMask;
			VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT,		//	VkMemoryInputFlags	inputMask;
		};
		const VkImageMemoryBarrier	colorAttBarrier		=
		{
			VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,		//	VkStructureType			sType;
			DE_NULL,									//	const void*				pNext;
			0u,											//	VkMemoryOutputFlags		outputMask;
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,		//	VkMemoryInputFlags		inputMask;
			VK_IMAGE_LAYOUT_UNDEFINED,					//	VkImageLayout			oldLayout;
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,	//	VkImageLayout			newLayout;
			queueFamilyIndex,							//	deUint32				srcQueueFamilyIndex;
			queueFamilyIndex,							//	deUint32				destQueueFamilyIndex;
			*image,										//	VkImage					image;
			{
				VK_IMAGE_ASPECT_COLOR_BIT,					//	VkImageAspect	aspect;
				0u,											//	deUint32		baseMipLevel;
				1u,											//	deUint32		mipLevels;
				0u,											//	deUint32		baseArraySlice;
				1u,											//	deUint32		arraySize;
			}											//	VkImageSubresourceRange	subresourceRange;
		};
		vk.cmdPipelineBarrier(*cmdBuf, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, (VkDependencyFlags)0, 1, &vertFlushBarrier, 0, (const VkBufferMemoryBarrier*)DE_NULL, 1, &colorAttBarrier);
	}

	{
		const VkClearValue			clearValue		= makeClearValueColorF32(0.125f, 0.25f, 0.75f, 1.0f);
		const VkRenderPassBeginInfo	passBeginParams	=
		{
			VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,			//	VkStructureType		sType;
			DE_NULL,											//	const void*			pNext;
			*renderPass,										//	VkRenderPass		renderPass;
			*framebuffer,										//	VkFramebuffer		framebuffer;
			{ { 0, 0 }, { renderSize.x(), renderSize.y() } },	//	VkRect2D			renderArea;
			1u,													//	deUint32			clearValueCount;
			&clearValue,										//	const VkClearValue*	pClearValues;
		};
		vk.cmdBeginRenderPass(*cmdBuf, &passBeginParams, VK_SUBPASS_CONTENTS_INLINE);
	}

	vk.cmdBindPipeline(*cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline);
	{
		const VkDeviceSize bindingOffset = 0;
		vk.cmdBindVertexBuffers(*cmdBuf, 0u, 1u, &vertexBuffer.get(), &bindingOffset);
	}
	vk.cmdDraw(*cmdBuf, deUint32(vertexCount), 1u /*run pipeline once*/, 0u /*first vertex*/, 0u /*first instanceIndex*/);
	vk.cmdEndRenderPass(*cmdBuf);

	{
		const VkImageMemoryBarrier	renderFinishBarrier	=
		{
			VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,		//	VkStructureType			sType;
			DE_NULL,									//	const void*				pNext;
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,		//	VkMemoryOutputFlags		outputMask;
			VK_ACCESS_TRANSFER_READ_BIT,				//	VkMemoryInputFlags		inputMask;
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,	//	VkImageLayout			oldLayout;
			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,		//	VkImageLayout			newLayout;
			queueFamilyIndex,							//	deUint32				srcQueueFamilyIndex;
			queueFamilyIndex,							//	deUint32				destQueueFamilyIndex;
			*image,										//	VkImage					image;
			{
				VK_IMAGE_ASPECT_COLOR_BIT,					//	VkImageAspectFlags	aspectMask;
				0u,											//	deUint32			baseMipLevel;
				1u,											//	deUint32			mipLevels;
				0u,											//	deUint32			baseArraySlice;
				1u,											//	deUint32			arraySize;
			}											//	VkImageSubresourceRange	subresourceRange;
		};
		vk.cmdPipelineBarrier(*cmdBuf, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 0, (const VkBufferMemoryBarrier*)DE_NULL, 1, &renderFinishBarrier);
	}

	{
		const VkBufferImageCopy	copyParams	=
		{
			(VkDeviceSize)0u,						//	VkDeviceSize			bufferOffset;
			(deUint32)renderSize.x(),				//	deUint32				bufferRowLength;
			(deUint32)renderSize.y(),				//	deUint32				bufferImageHeight;
			{
				VK_IMAGE_ASPECT_COLOR_BIT,				//	VkImageAspect		aspect;
				0u,										//	deUint32			mipLevel;
				0u,										//	deUint32			arrayLayer;
				1u,										//	deUint32			arraySize;
			},										//	VkImageSubresourceCopy	imageSubresource;
			{ 0u, 0u, 0u },							//	VkOffset3D				imageOffset;
			{ renderSize.x(), renderSize.y(), 1u }	//	VkExtent3D				imageExtent;
		};
		vk.cmdCopyImageToBuffer(*cmdBuf, *image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, *readImageBuffer, 1u, &copyParams);
	}

	{
		const VkBufferMemoryBarrier	copyFinishBarrier	=
		{
			VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,	//	VkStructureType		sType;
			DE_NULL,									//	const void*			pNext;
			VK_ACCESS_TRANSFER_WRITE_BIT,				//	VkMemoryOutputFlags	outputMask;
			VK_ACCESS_HOST_READ_BIT,					//	VkMemoryInputFlags	inputMask;
			queueFamilyIndex,							//	deUint32			srcQueueFamilyIndex;
			queueFamilyIndex,							//	deUint32			destQueueFamilyIndex;
			*readImageBuffer,							//	VkBuffer			buffer;
			0u,											//	VkDeviceSize		offset;
			imageSizeBytes								//	VkDeviceSize		size;
		};
		vk.cmdPipelineBarrier(*cmdBuf, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 1, &copyFinishBarrier, 0, (const VkImageMemoryBarrier*)DE_NULL);
	}

	VK_CHECK(vk.endCommandBuffer(*cmdBuf));

	// Upload vertex data
	{
		const VkMappedMemoryRange	range			=
		{
			VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,	//	VkStructureType	sType;
			DE_NULL,								//	const void*		pNext;
			vertexBufferMemory->getMemory(),		//	VkDeviceMemory	mem;
			0,										//	VkDeviceSize	offset;
			(VkDeviceSize)sizeof(vertexData),		//	VkDeviceSize	size;
		};
		void*						vertexBufPtr	= vertexBufferMemory->getHostPtr();

		deMemcpy(vertexBufPtr, &vertexData[0], sizeof(vertexData));
		VK_CHECK(vk.flushMappedMemoryRanges(vkDevice, 1u, &range));
	}

	// Submit & wait for completion
	{
		const VkFenceCreateInfo	fenceParams	=
		{
			VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,	//	VkStructureType		sType;
			DE_NULL,								//	const void*			pNext;
			0u,										//	VkFenceCreateFlags	flags;
		};
		const Unique<VkFence>	fence		(createFence(vk, vkDevice, &fenceParams));
		const VkSubmitInfo		submitInfo	=
		{
			VK_STRUCTURE_TYPE_SUBMIT_INFO,
			DE_NULL,
			0u,
			(const VkSemaphore*)DE_NULL,
			(const VkPipelineStageFlags*)DE_NULL,
			1u,
			&cmdBuf.get(),
			0u,
			(const VkSemaphore*)DE_NULL,
		};

		VK_CHECK(vk.queueSubmit(queue, 1u, &submitInfo, *fence));
		VK_CHECK(vk.waitForFences(vkDevice, 1u, &fence.get(), DE_TRUE, ~0ull));
	}

	const void* imagePtr	= readImageBufferMemory->getHostPtr();
	const tcu::ConstPixelBufferAccess pixelBuffer(tcu::TextureFormat(tcu::TextureFormat::RGBA, tcu::TextureFormat::UNORM_INT8),
												  renderSize.x(), renderSize.y(), 1, imagePtr);
	// Log image
	{
		const VkMappedMemoryRange	range		=
		{
			VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,	//	VkStructureType	sType;
			DE_NULL,								//	const void*		pNext;
			readImageBufferMemory->getMemory(),		//	VkDeviceMemory	mem;
			0,										//	VkDeviceSize	offset;
			imageSizeBytes,							//	VkDeviceSize	size;
		};

		VK_CHECK(vk.invalidateMappedMemoryRanges(vkDevice, 1u, &range));
		context.getTestContext().getLog() << TestLog::Image("Result", "Result", pixelBuffer);
	}

	const RGBA threshold(1, 1, 1, 1);
	const RGBA upperLeft(pixelBuffer.getPixel(1, 1));
	if (!tcu::compareThreshold(upperLeft, instance.outputColors[0], threshold))
		return TestStatus::fail("Upper left corner mismatch");

	const RGBA upperRight(pixelBuffer.getPixel(pixelBuffer.getWidth() - 1, 1));
	if (!tcu::compareThreshold(upperRight, instance.outputColors[1], threshold))
		return TestStatus::fail("Upper right corner mismatch");

	const RGBA lowerLeft(pixelBuffer.getPixel(1, pixelBuffer.getHeight() - 1));
	if (!tcu::compareThreshold(lowerLeft, instance.outputColors[2], threshold))
		return TestStatus::fail("Lower left corner mismatch");

	const RGBA lowerRight(pixelBuffer.getPixel(pixelBuffer.getWidth() - 1, pixelBuffer.getHeight() - 1));
	if (!tcu::compareThreshold(lowerRight, instance.outputColors[3], threshold))
		return TestStatus::fail("Lower right corner mismatch");

	return TestStatus::pass("Rendered output matches input");
}

void createTestsForAllStages (const std::string& name, const RGBA (&inputColors)[4], const RGBA (&outputColors)[4], const map<string, string>& testCodeFragments, const vector<deInt32>& specConstants, tcu::TestCaseGroup* tests)
{
	const ShaderElement		vertFragPipelineStages[]		=
	{
		ShaderElement("vert", "main", VK_SHADER_STAGE_VERTEX_BIT),
		ShaderElement("frag", "main", VK_SHADER_STAGE_FRAGMENT_BIT),
	};

	const ShaderElement		tessPipelineStages[]			=
	{
		ShaderElement("vert", "main", VK_SHADER_STAGE_VERTEX_BIT),
		ShaderElement("tessc", "main", VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT),
		ShaderElement("tesse", "main", VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT),
		ShaderElement("frag", "main", VK_SHADER_STAGE_FRAGMENT_BIT),
	};

	const ShaderElement		geomPipelineStages[]				=
	{
		ShaderElement("vert", "main", VK_SHADER_STAGE_VERTEX_BIT),
		ShaderElement("geom", "main", VK_SHADER_STAGE_GEOMETRY_BIT),
		ShaderElement("frag", "main", VK_SHADER_STAGE_FRAGMENT_BIT),
	};

	StageToSpecConstantMap	specConstantMap;

	specConstantMap[VK_SHADER_STAGE_VERTEX_BIT] = specConstants;
	addFunctionCaseWithPrograms<InstanceContext>(tests, name + "_vert", "", addShaderCodeCustomVertex, runAndVerifyDefaultPipeline,
												 createInstanceContext(vertFragPipelineStages, inputColors, outputColors, testCodeFragments, specConstantMap));

	specConstantMap.clear();
	specConstantMap[VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT] = specConstants;
	addFunctionCaseWithPrograms<InstanceContext>(tests, name + "_tessc", "", addShaderCodeCustomTessControl, runAndVerifyDefaultPipeline,
												 createInstanceContext(tessPipelineStages, inputColors, outputColors, testCodeFragments, specConstantMap));

	specConstantMap.clear();
	specConstantMap[VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT] = specConstants;
	addFunctionCaseWithPrograms<InstanceContext>(tests, name + "_tesse", "", addShaderCodeCustomTessEval, runAndVerifyDefaultPipeline,
												 createInstanceContext(tessPipelineStages, inputColors, outputColors, testCodeFragments, specConstantMap));

	specConstantMap.clear();
	specConstantMap[VK_SHADER_STAGE_GEOMETRY_BIT] = specConstants;
	addFunctionCaseWithPrograms<InstanceContext>(tests, name + "_geom", "", addShaderCodeCustomGeometry, runAndVerifyDefaultPipeline,
												 createInstanceContext(geomPipelineStages, inputColors, outputColors, testCodeFragments, specConstantMap));

	specConstantMap.clear();
	specConstantMap[VK_SHADER_STAGE_FRAGMENT_BIT] = specConstants;
	addFunctionCaseWithPrograms<InstanceContext>(tests, name + "_frag", "", addShaderCodeCustomFragment, runAndVerifyDefaultPipeline,
												 createInstanceContext(vertFragPipelineStages, inputColors, outputColors, testCodeFragments, specConstantMap));
}

inline void createTestsForAllStages (const std::string& name, const RGBA (&inputColors)[4], const RGBA (&outputColors)[4], const map<string, string>& testCodeFragments, tcu::TestCaseGroup* tests)
{
	vector<deInt32> noSpecConstants;
	createTestsForAllStages(name, inputColors, outputColors, testCodeFragments, noSpecConstants, tests);
}

} // anonymous

tcu::TestCaseGroup* createOpSourceTests (tcu::TestContext& testCtx)
{
	struct NameCodePair { string name, code; };
	RGBA							defaultColors[4];
	de::MovePtr<tcu::TestCaseGroup> opSourceTests			(new tcu::TestCaseGroup(testCtx, "opsource", "OpSource instruction"));
	const std::string				opsourceGLSLWithFile	= "%opsrcfile = OpString \"foo.vert\"\nOpSource GLSL 450 %opsrcfile ";
	map<string, string>				fragments				= passthruFragments();
	const NameCodePair				tests[]					=
	{
		{"unknown", "OpSource Unknown 321"},
		{"essl", "OpSource ESSL 310"},
		{"glsl", "OpSource GLSL 450"},
		{"opencl_cpp", "OpSource OpenCL_CPP 120"},
		{"opencl_c", "OpSource OpenCL_C 120"},
		{"multiple", "OpSource GLSL 450\nOpSource GLSL 450"},
		{"file", opsourceGLSLWithFile},
		{"source", opsourceGLSLWithFile + "\"void main(){}\""},
		// Longest possible source string: SPIR-V limits instructions to 65535
		// words, of which the first 4 are opsourceGLSLWithFile; the rest will
		// contain 65530 UTF8 characters (one word each) plus one last word
		// containing 3 ASCII characters and \0.
		{"longsource", opsourceGLSLWithFile + '"' + makeLongUTF8String(65530) + "ccc" + '"'}
	};

	getDefaultColors(defaultColors);
	for (size_t testNdx = 0; testNdx < sizeof(tests) / sizeof(NameCodePair); ++testNdx)
	{
		fragments["debug"] = tests[testNdx].code;
		createTestsForAllStages(tests[testNdx].name, defaultColors, defaultColors, fragments, opSourceTests.get());
	}

	return opSourceTests.release();
}

tcu::TestCaseGroup* createOpSourceContinuedTests (tcu::TestContext& testCtx)
{
	struct NameCodePair { string name, code; };
	RGBA								defaultColors[4];
	de::MovePtr<tcu::TestCaseGroup>		opSourceTests		(new tcu::TestCaseGroup(testCtx, "opsourcecontinued", "OpSourceContinued instruction"));
	map<string, string>					fragments			= passthruFragments();
	const std::string					opsource			= "%opsrcfile = OpString \"foo.vert\"\nOpSource GLSL 450 %opsrcfile \"void main(){}\"\n";
	const NameCodePair					tests[]				=
	{
		{"empty", opsource + "OpSourceContinued \"\""},
		{"short", opsource + "OpSourceContinued \"abcde\""},
		{"multiple", opsource + "OpSourceContinued \"abcde\"\nOpSourceContinued \"fghij\""},
		// Longest possible source string: SPIR-V limits instructions to 65535
		// words, of which the first one is OpSourceContinued/length; the rest
		// will contain 65533 UTF8 characters (one word each) plus one last word
		// containing 3 ASCII characters and \0.
		{"long", opsource + "OpSourceContinued \"" + makeLongUTF8String(65533) + "ccc\""}
	};

	getDefaultColors(defaultColors);
	for (size_t testNdx = 0; testNdx < sizeof(tests) / sizeof(NameCodePair); ++testNdx)
	{
		fragments["debug"] = tests[testNdx].code;
		createTestsForAllStages(tests[testNdx].name, defaultColors, defaultColors, fragments, opSourceTests.get());
	}

	return opSourceTests.release();
}

tcu::TestCaseGroup* createOpNoLineTests(tcu::TestContext& testCtx)
{
	RGBA								 defaultColors[4];
	de::MovePtr<tcu::TestCaseGroup>		 opLineTests		 (new tcu::TestCaseGroup(testCtx, "opnoline", "OpNoLine instruction"));
	map<string, string>					 fragments;
	getDefaultColors(defaultColors);
	fragments["debug"]			=
		"%name = OpString \"name\"\n";

	fragments["pre_main"]	=
		"OpNoLine\n"
		"OpNoLine\n"
		"OpLine %name 1 1\n"
		"OpNoLine\n"
		"OpLine %name 1 1\n"
		"OpLine %name 1 1\n"
		"%second_function = OpFunction %v4f32 None %v4f32_function\n"
		"OpNoLine\n"
		"OpLine %name 1 1\n"
		"OpNoLine\n"
		"OpLine %name 1 1\n"
		"OpLine %name 1 1\n"
		"%second_param1 = OpFunctionParameter %v4f32\n"
		"OpNoLine\n"
		"OpNoLine\n"
		"%label_secondfunction = OpLabel\n"
		"OpNoLine\n"
		"OpReturnValue %second_param1\n"
		"OpFunctionEnd\n"
		"OpNoLine\n"
		"OpNoLine\n";

	fragments["testfun"]		=
		// A %test_code function that returns its argument unchanged.
		"OpNoLine\n"
		"OpNoLine\n"
		"OpLine %name 1 1\n"
		"%test_code = OpFunction %v4f32 None %v4f32_function\n"
		"OpNoLine\n"
		"%param1 = OpFunctionParameter %v4f32\n"
		"OpNoLine\n"
		"OpNoLine\n"
		"%label_testfun = OpLabel\n"
		"OpNoLine\n"
		"%val1 = OpFunctionCall %v4f32 %second_function %param1\n"
		"OpReturnValue %val1\n"
		"OpFunctionEnd\n"
		"OpLine %name 1 1\n"
		"OpNoLine\n";

	createTestsForAllStages("opnoline", defaultColors, defaultColors, fragments, opLineTests.get());

	return opLineTests.release();
}


tcu::TestCaseGroup* createOpLineTests(tcu::TestContext& testCtx)
{
	RGBA													defaultColors[4];
	de::MovePtr<tcu::TestCaseGroup>							opLineTests			(new tcu::TestCaseGroup(testCtx, "opline", "OpLine instruction"));
	map<string, string>										fragments;
	std::vector<std::pair<std::string, std::string> >		problemStrings;

	problemStrings.push_back(std::make_pair<std::string, std::string>("empty_name", ""));
	problemStrings.push_back(std::make_pair<std::string, std::string>("short_name", "short_name"));
	problemStrings.push_back(std::make_pair<std::string, std::string>("long_name", makeLongUTF8String(65530) + "ccc"));
	getDefaultColors(defaultColors);

	fragments["debug"]			=
		"%other_name = OpString \"other_name\"\n";

	fragments["pre_main"]	=
		"OpLine %file_name 32 0\n"
		"OpLine %file_name 32 32\n"
		"OpLine %file_name 32 40\n"
		"OpLine %other_name 32 40\n"
		"OpLine %other_name 0 100\n"
		"OpLine %other_name 0 4294967295\n"
		"OpLine %other_name 4294967295 0\n"
		"OpLine %other_name 32 40\n"
		"OpLine %file_name 0 0\n"
		"%second_function = OpFunction %v4f32 None %v4f32_function\n"
		"OpLine %file_name 1 0\n"
		"%second_param1 = OpFunctionParameter %v4f32\n"
		"OpLine %file_name 1 3\n"
		"OpLine %file_name 1 2\n"
		"%label_secondfunction = OpLabel\n"
		"OpLine %file_name 0 2\n"
		"OpReturnValue %second_param1\n"
		"OpFunctionEnd\n"
		"OpLine %file_name 0 2\n"
		"OpLine %file_name 0 2\n";

	fragments["testfun"]		=
		// A %test_code function that returns its argument unchanged.
		"OpLine %file_name 1 0\n"
		"%test_code = OpFunction %v4f32 None %v4f32_function\n"
		"OpLine %file_name 16 330\n"
		"%param1 = OpFunctionParameter %v4f32\n"
		"OpLine %file_name 14 442\n"
		"%label_testfun = OpLabel\n"
		"OpLine %file_name 11 1024\n"
		"%val1 = OpFunctionCall %v4f32 %second_function %param1\n"
		"OpLine %file_name 2 97\n"
		"OpReturnValue %val1\n"
		"OpFunctionEnd\n"
		"OpLine %file_name 5 32\n";

	for (size_t i = 0; i < problemStrings.size(); ++i)
	{
		map<string, string> testFragments = fragments;
		testFragments["debug"] += "%file_name = OpString \"" + problemStrings[i].second + "\"\n";
		createTestsForAllStages(string("opline") + "_" + problemStrings[i].first, defaultColors, defaultColors, testFragments, opLineTests.get());
	}

	return opLineTests.release();
}

tcu::TestCaseGroup* createOpConstantNullTests(tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> opConstantNullTests		(new tcu::TestCaseGroup(testCtx, "opconstantnull", "OpConstantNull instruction"));
	RGBA							colors[4];


	const char						functionStart[] =
		"%test_code = OpFunction %v4f32 None %v4f32_function\n"
		"%param1 = OpFunctionParameter %v4f32\n"
		"%lbl    = OpLabel\n";

	const char						functionEnd[]	=
		"OpReturnValue %transformed_param\n"
		"OpFunctionEnd\n";

	struct NameConstantsCode
	{
		string name;
		string constants;
		string code;
	};

	NameConstantsCode tests[] =
	{
		{
			"vec4",
			"%cnull = OpConstantNull %v4f32\n",
			"%transformed_param = OpFAdd %v4f32 %param1 %cnull\n"
		},
		{
			"float",
			"%cnull = OpConstantNull %f32\n",
			"%vp = OpVariable %fp_v4f32 Function\n"
			"%v  = OpLoad %v4f32 %vp\n"
			"%v0 = OpVectorInsertDynamic %v4f32 %v %cnull %c_i32_0\n"
			"%v1 = OpVectorInsertDynamic %v4f32 %v0 %cnull %c_i32_1\n"
			"%v2 = OpVectorInsertDynamic %v4f32 %v1 %cnull %c_i32_2\n"
			"%v3 = OpVectorInsertDynamic %v4f32 %v2 %cnull %c_i32_3\n"
			"%transformed_param = OpFAdd %v4f32 %param1 %v3\n"
		},
		{
			"bool",
			"%cnull             = OpConstantNull %bool\n",
			"%v                 = OpVariable %fp_v4f32 Function\n"
			"                     OpStore %v %param1\n"
			"                     OpSelectionMerge %false_label None\n"
			"                     OpBranchConditional %cnull %true_label %false_label\n"
			"%true_label        = OpLabel\n"
			"                     OpStore %v %c_v4f32_0_5_0_5_0_5_0_5\n"
			"                     OpBranch %false_label\n"
			"%false_label       = OpLabel\n"
			"%transformed_param = OpLoad %v4f32 %v\n"
		},
		{
			"i32",
			"%cnull             = OpConstantNull %i32\n",
			"%v                 = OpVariable %fp_v4f32 Function %c_v4f32_0_5_0_5_0_5_0_5\n"
			"%b                 = OpIEqual %bool %cnull %c_i32_0\n"
			"                     OpSelectionMerge %false_label None\n"
			"                     OpBranchConditional %b %true_label %false_label\n"
			"%true_label        = OpLabel\n"
			"                     OpStore %v %param1\n"
			"                     OpBranch %false_label\n"
			"%false_label       = OpLabel\n"
			"%transformed_param = OpLoad %v4f32 %v\n"
		},
		{
			"struct",
			"%stype             = OpTypeStruct %f32 %v4f32\n"
			"%fp_stype          = OpTypePointer Function %stype\n"
			"%cnull             = OpConstantNull %stype\n",
			"%v                 = OpVariable %fp_stype Function %cnull\n"
			"%f                 = OpAccessChain %fp_v4f32 %v %c_i32_1\n"
			"%f_val             = OpLoad %v4f32 %f\n"
			"%transformed_param = OpFAdd %v4f32 %param1 %f_val\n"
		},
		{
			"array",
			"%a4_v4f32          = OpTypeArray %v4f32 %c_u32_4\n"
			"%fp_a4_v4f32       = OpTypePointer Function %a4_v4f32\n"
			"%cnull             = OpConstantNull %a4_v4f32\n",
			"%v                 = OpVariable %fp_a4_v4f32 Function %cnull\n"
			"%f                 = OpAccessChain %fp_v4f32 %v %c_u32_0\n"
			"%f1                = OpAccessChain %fp_v4f32 %v %c_u32_1\n"
			"%f2                = OpAccessChain %fp_v4f32 %v %c_u32_2\n"
			"%f3                = OpAccessChain %fp_v4f32 %v %c_u32_3\n"
			"%f_val             = OpLoad %v4f32 %f\n"
			"%f1_val            = OpLoad %v4f32 %f1\n"
			"%f2_val            = OpLoad %v4f32 %f2\n"
			"%f3_val            = OpLoad %v4f32 %f3\n"
			"%t0                = OpFAdd %v4f32 %param1 %f_val\n"
			"%t1                = OpFAdd %v4f32 %t0 %f1_val\n"
			"%t2                = OpFAdd %v4f32 %t1 %f2_val\n"
			"%transformed_param = OpFAdd %v4f32 %t2 %f3_val\n"
		},
		{
			"matrix",
			"%mat4x4_f32        = OpTypeMatrix %v4f32 4\n"
			"%cnull             = OpConstantNull %mat4x4_f32\n",
			// Our null matrix * any vector should result in a zero vector.
			"%v                 = OpVectorTimesMatrix %v4f32 %param1 %cnull\n"
			"%transformed_param = OpFAdd %v4f32 %param1 %v\n"
		}
	};

	getHalfColorsFullAlpha(colors);

	for (size_t testNdx = 0; testNdx < sizeof(tests) / sizeof(NameConstantsCode); ++testNdx)
	{
		map<string, string> fragments;
		fragments["pre_main"] = tests[testNdx].constants;
		fragments["testfun"] = string(functionStart) + tests[testNdx].code + functionEnd;
		createTestsForAllStages(tests[testNdx].name, colors, colors, fragments, opConstantNullTests.get());
	}
	return opConstantNullTests.release();
}
tcu::TestCaseGroup* createOpConstantCompositeTests(tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> opConstantCompositeTests		(new tcu::TestCaseGroup(testCtx, "opconstantcomposite", "OpConstantComposite instruction"));
	RGBA							inputColors[4];
	RGBA							outputColors[4];


	const char						functionStart[]	 =
		"%test_code = OpFunction %v4f32 None %v4f32_function\n"
		"%param1 = OpFunctionParameter %v4f32\n"
		"%lbl    = OpLabel\n";

	const char						functionEnd[]		=
		"OpReturnValue %transformed_param\n"
		"OpFunctionEnd\n";

	struct NameConstantsCode
	{
		string name;
		string constants;
		string code;
	};

	NameConstantsCode tests[] =
	{
		{
			"vec4",

			"%cval              = OpConstantComposite %v4f32 %c_f32_0_5 %c_f32_0_5 %c_f32_0_5 %c_f32_0\n",
			"%transformed_param = OpFAdd %v4f32 %param1 %cval\n"
		},
		{
			"struct",

			"%stype             = OpTypeStruct %v4f32 %f32\n"
			"%fp_stype          = OpTypePointer Function %stype\n"
			"%f32_n_1           = OpConstant %f32 -1.0\n"
			"%f32_1_5           = OpConstant %f32 !0x3fc00000\n" // +1.5
			"%cvec              = OpConstantComposite %v4f32 %f32_1_5 %f32_1_5 %f32_1_5 %c_f32_1\n"
			"%cval              = OpConstantComposite %stype %cvec %f32_n_1\n",

			"%v                 = OpVariable %fp_stype Function %cval\n"
			"%vec_ptr           = OpAccessChain %fp_v4f32 %v %c_u32_0\n"
			"%f32_ptr           = OpAccessChain %fp_f32 %v %c_u32_1\n"
			"%vec_val           = OpLoad %v4f32 %vec_ptr\n"
			"%f32_val           = OpLoad %f32 %f32_ptr\n"
			"%tmp1              = OpVectorTimesScalar %v4f32 %c_v4f32_1_1_1_1 %f32_val\n" // vec4(-1)
			"%tmp2              = OpFAdd %v4f32 %tmp1 %param1\n" // param1 + vec4(-1)
			"%transformed_param = OpFAdd %v4f32 %tmp2 %vec_val\n" // param1 + vec4(-1) + vec4(1.5, 1.5, 1.5, 1.0)
		},
		{
			// [1|0|0|0.5] [x] = x + 0.5
			// [0|1|0|0.5] [y] = y + 0.5
			// [0|0|1|0.5] [z] = z + 0.5
			// [0|0|0|1  ] [1] = 1
			"matrix",

			"%mat4x4_f32          = OpTypeMatrix %v4f32 4\n"
		    "%v4f32_1_0_0_0       = OpConstantComposite %v4f32 %c_f32_1 %c_f32_0 %c_f32_0 %c_f32_0\n"
		    "%v4f32_0_1_0_0       = OpConstantComposite %v4f32 %c_f32_0 %c_f32_1 %c_f32_0 %c_f32_0\n"
		    "%v4f32_0_0_1_0       = OpConstantComposite %v4f32 %c_f32_0 %c_f32_0 %c_f32_1 %c_f32_0\n"
		    "%v4f32_0_5_0_5_0_5_1 = OpConstantComposite %v4f32 %c_f32_0_5 %c_f32_0_5 %c_f32_0_5 %c_f32_1\n"
			"%cval                = OpConstantComposite %mat4x4_f32 %v4f32_1_0_0_0 %v4f32_0_1_0_0 %v4f32_0_0_1_0 %v4f32_0_5_0_5_0_5_1\n",

			"%transformed_param   = OpMatrixTimesVector %v4f32 %cval %param1\n"
		},
		{
			"array",

			"%c_v4f32_1_1_1_0     = OpConstantComposite %v4f32 %c_f32_1 %c_f32_1 %c_f32_1 %c_f32_0\n"
			"%fp_a4f32            = OpTypePointer Function %a4f32\n"
			"%f32_n_1             = OpConstant %f32 -1.0\n"
			"%f32_1_5             = OpConstant %f32 !0x3fc00000\n" // +1.5
			"%carr                = OpConstantComposite %a4f32 %c_f32_0 %f32_n_1 %f32_1_5 %c_f32_0\n",

			"%v                   = OpVariable %fp_a4f32 Function %carr\n"
			"%f                   = OpAccessChain %fp_f32 %v %c_u32_0\n"
			"%f1                  = OpAccessChain %fp_f32 %v %c_u32_1\n"
			"%f2                  = OpAccessChain %fp_f32 %v %c_u32_2\n"
			"%f3                  = OpAccessChain %fp_f32 %v %c_u32_3\n"
			"%f_val               = OpLoad %f32 %f\n"
			"%f1_val              = OpLoad %f32 %f1\n"
			"%f2_val              = OpLoad %f32 %f2\n"
			"%f3_val              = OpLoad %f32 %f3\n"
			"%ftot1               = OpFAdd %f32 %f_val %f1_val\n"
			"%ftot2               = OpFAdd %f32 %ftot1 %f2_val\n"
			"%ftot3               = OpFAdd %f32 %ftot2 %f3_val\n"  // 0 - 1 + 1.5 + 0
			"%add_vec             = OpVectorTimesScalar %v4f32 %c_v4f32_1_1_1_0 %ftot3\n"
			"%transformed_param   = OpFAdd %v4f32 %param1 %add_vec\n"
		},
		{
			//
			// [
			//   {
			//      0.0,
			//      [ 1.0, 1.0, 1.0, 1.0]
			//   },
			//   {
			//      1.0,
			//      [ 0.0, 0.5, 0.0, 0.0]
			//   }, //     ^^^
			//   {
			//      0.0,
			//      [ 1.0, 1.0, 1.0, 1.0]
			//   }
			// ]
			"array_of_struct_of_array",

			"%c_v4f32_1_1_1_0     = OpConstantComposite %v4f32 %c_f32_1 %c_f32_1 %c_f32_1 %c_f32_0\n"
			"%fp_a4f32            = OpTypePointer Function %a4f32\n"
			"%stype               = OpTypeStruct %f32 %a4f32\n"
			"%a3stype             = OpTypeArray %stype %c_u32_3\n"
			"%fp_a3stype          = OpTypePointer Function %a3stype\n"
			"%ca4f32_0            = OpConstantComposite %a4f32 %c_f32_0 %c_f32_0_5 %c_f32_0 %c_f32_0\n"
			"%ca4f32_1            = OpConstantComposite %a4f32 %c_f32_1 %c_f32_1 %c_f32_1 %c_f32_1\n"
			"%cstype1             = OpConstantComposite %stype %c_f32_0 %ca4f32_1\n"
			"%cstype2             = OpConstantComposite %stype %c_f32_1 %ca4f32_0\n"
			"%carr                = OpConstantComposite %a3stype %cstype1 %cstype2 %cstype1",

			"%v                   = OpVariable %fp_a3stype Function %carr\n"
			"%f                   = OpAccessChain %fp_f32 %v %c_u32_1 %c_u32_1 %c_u32_1\n"
			"%f_l                 = OpLoad %f32 %f\n"
			"%add_vec             = OpVectorTimesScalar %v4f32 %c_v4f32_1_1_1_0 %f_l\n"
			"%transformed_param   = OpFAdd %v4f32 %param1 %add_vec\n"
		}
	};

	getHalfColorsFullAlpha(inputColors);
	outputColors[0] = RGBA(255, 255, 255, 255);
	outputColors[1] = RGBA(255, 127, 127, 255);
	outputColors[2] = RGBA(127, 255, 127, 255);
	outputColors[3] = RGBA(127, 127, 255, 255);

	for (size_t testNdx = 0; testNdx < sizeof(tests) / sizeof(NameConstantsCode); ++testNdx)
	{
		map<string, string> fragments;
		fragments["pre_main"] = tests[testNdx].constants;
		fragments["testfun"] = string(functionStart) + tests[testNdx].code + functionEnd;
		createTestsForAllStages(tests[testNdx].name, inputColors, outputColors, fragments, opConstantCompositeTests.get());
	}
	return opConstantCompositeTests.release();
}

tcu::TestCaseGroup* createSelectionBlockOrderTests(tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> group				(new tcu::TestCaseGroup(testCtx, "selection_block_order", "Out-of-order blocks for selection"));
	RGBA							inputColors[4];
	RGBA							outputColors[4];
	map<string, string>				fragments;

	// vec4 test_code(vec4 param) {
	//   vec4 result = param;
	//   for (int i = 0; i < 4; ++i) {
	//     if (i == 0) result[i] = 0.;
	//     else        result[i] = 1. - result[i];
	//   }
	//   return result;
	// }
	const char						function[]			=
		"%test_code = OpFunction %v4f32 None %v4f32_function\n"
		"%param1    = OpFunctionParameter %v4f32\n"
		"%lbl       = OpLabel\n"
		"%iptr      = OpVariable %fp_i32 Function\n"
		"%result    = OpVariable %fp_v4f32 Function\n"
		"             OpStore %iptr %c_i32_0\n"
		"             OpStore %result %param1\n"
		"             OpBranch %loop\n"

		// Loop entry block.
		"%loop      = OpLabel\n"
		"%ival      = OpLoad %i32 %iptr\n"
		"%lt_4      = OpSLessThan %bool %ival %c_i32_4\n"
		"             OpLoopMerge %exit %loop None\n"
		"             OpBranchConditional %lt_4 %if_entry %exit\n"

		// Merge block for loop.
		"%exit      = OpLabel\n"
		"%ret       = OpLoad %v4f32 %result\n"
		"             OpReturnValue %ret\n"

		// If-statement entry block.
		"%if_entry  = OpLabel\n"
		"%loc       = OpAccessChain %fp_f32 %result %ival\n"
		"%eq_0      = OpIEqual %bool %ival %c_i32_0\n"
		"             OpSelectionMerge %if_exit None\n"
		"             OpBranchConditional %eq_0 %if_true %if_false\n"

		// False branch for if-statement.
		"%if_false  = OpLabel\n"
		"%val       = OpLoad %f32 %loc\n"
		"%sub       = OpFSub %f32 %c_f32_1 %val\n"
		"             OpStore %loc %sub\n"
		"             OpBranch %if_exit\n"

		// Merge block for if-statement.
		"%if_exit   = OpLabel\n"
		"%ival_next = OpIAdd %i32 %ival %c_i32_1\n"
		"             OpStore %iptr %ival_next\n"
		"             OpBranch %loop\n"

		// True branch for if-statement.
		"%if_true   = OpLabel\n"
		"             OpStore %loc %c_f32_0\n"
		"             OpBranch %if_exit\n"

		"             OpFunctionEnd\n";

	fragments["testfun"]	= function;

	inputColors[0]			= RGBA(127, 127, 127, 0);
	inputColors[1]			= RGBA(127, 0,   0,   0);
	inputColors[2]			= RGBA(0,   127, 0,   0);
	inputColors[3]			= RGBA(0,   0,   127, 0);

	outputColors[0]			= RGBA(0, 128, 128, 255);
	outputColors[1]			= RGBA(0, 255, 255, 255);
	outputColors[2]			= RGBA(0, 128, 255, 255);
	outputColors[3]			= RGBA(0, 255, 128, 255);

	createTestsForAllStages("out_of_order", inputColors, outputColors, fragments, group.get());

	return group.release();
}

tcu::TestCaseGroup* createSwitchBlockOrderTests(tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> group				(new tcu::TestCaseGroup(testCtx, "switch_block_order", "Out-of-order blocks for switch"));
	RGBA							inputColors[4];
	RGBA							outputColors[4];
	map<string, string>				fragments;

	const char						typesAndConstants[]	=
		"%c_f32_p2  = OpConstant %f32 0.2\n"
		"%c_f32_p4  = OpConstant %f32 0.4\n"
		"%c_f32_p6  = OpConstant %f32 0.6\n"
		"%c_f32_p8  = OpConstant %f32 0.8\n";

	// vec4 test_code(vec4 param) {
	//   vec4 result = param;
	//   for (int i = 0; i < 4; ++i) {
	//     switch (i) {
	//       case 0: result[i] += .2; break;
	//       case 1: result[i] += .6; break;
	//       case 2: result[i] += .4; break;
	//       case 3: result[i] += .8; break;
	//       default: break; // unreachable
	//     }
	//   }
	//   return result;
	// }
	const char						function[]			=
		"%test_code = OpFunction %v4f32 None %v4f32_function\n"
		"%param1    = OpFunctionParameter %v4f32\n"
		"%lbl       = OpLabel\n"
		"%iptr      = OpVariable %fp_i32 Function\n"
		"%result    = OpVariable %fp_v4f32 Function\n"
		"             OpStore %iptr %c_i32_0\n"
		"             OpStore %result %param1\n"
		"             OpBranch %loop\n"

		// Loop entry block.
		"%loop      = OpLabel\n"
		"%ival      = OpLoad %i32 %iptr\n"
		"%lt_4      = OpSLessThan %bool %ival %c_i32_4\n"
		"             OpLoopMerge %exit %loop None\n"
		"             OpBranchConditional %lt_4 %switch_entry %exit\n"

		// Merge block for loop.
		"%exit      = OpLabel\n"
		"%ret       = OpLoad %v4f32 %result\n"
		"             OpReturnValue %ret\n"

		// Switch-statement entry block.
		"%switch_entry   = OpLabel\n"
		"%loc            = OpAccessChain %fp_f32 %result %ival\n"
		"%val            = OpLoad %f32 %loc\n"
		"                  OpSelectionMerge %switch_exit None\n"
		"                  OpSwitch %ival %switch_default 0 %case0 1 %case1 2 %case2 3 %case3\n"

		"%case2          = OpLabel\n"
		"%addp4          = OpFAdd %f32 %val %c_f32_p4\n"
		"                  OpStore %loc %addp4\n"
		"                  OpBranch %switch_exit\n"

		"%switch_default = OpLabel\n"
		"                  OpUnreachable\n"

		"%case3          = OpLabel\n"
		"%addp8          = OpFAdd %f32 %val %c_f32_p8\n"
		"                  OpStore %loc %addp8\n"
		"                  OpBranch %switch_exit\n"

		"%case0          = OpLabel\n"
		"%addp2          = OpFAdd %f32 %val %c_f32_p2\n"
		"                  OpStore %loc %addp2\n"
		"                  OpBranch %switch_exit\n"

		// Merge block for switch-statement.
		"%switch_exit    = OpLabel\n"
		"%ival_next      = OpIAdd %i32 %ival %c_i32_1\n"
		"                  OpStore %iptr %ival_next\n"
		"                  OpBranch %loop\n"

		"%case1          = OpLabel\n"
		"%addp6          = OpFAdd %f32 %val %c_f32_p6\n"
		"                  OpStore %loc %addp6\n"
		"                  OpBranch %switch_exit\n"

		"                  OpFunctionEnd\n";

	fragments["pre_main"]	= typesAndConstants;
	fragments["testfun"]	= function;

	inputColors[0]			= RGBA(127, 27,  127, 51);
	inputColors[1]			= RGBA(127, 0,   0,   51);
	inputColors[2]			= RGBA(0,   27,  0,   51);
	inputColors[3]			= RGBA(0,   0,   127, 51);

	outputColors[0]			= RGBA(178, 180, 229, 255);
	outputColors[1]			= RGBA(178, 153, 102, 255);
	outputColors[2]			= RGBA(51,  180, 102, 255);
	outputColors[3]			= RGBA(51,  153, 229, 255);

	createTestsForAllStages("out_of_order", inputColors, outputColors, fragments, group.get());

	return group.release();
}

tcu::TestCaseGroup* createDecorationGroupTests(tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> group				(new tcu::TestCaseGroup(testCtx, "decoration_group", "Decoration group tests"));
	RGBA							inputColors[4];
	RGBA							outputColors[4];
	map<string, string>				fragments;

	const char						decorations[]		=
		"OpDecorate %array_group         ArrayStride 4\n"
		"OpDecorate %struct_member_group Offset 0\n"
		"%array_group         = OpDecorationGroup\n"
		"%struct_member_group = OpDecorationGroup\n"

		"OpDecorate %group1 RelaxedPrecision\n"
		"OpDecorate %group3 RelaxedPrecision\n"
		"OpDecorate %group3 Invariant\n"
		"OpDecorate %group3 Restrict\n"
		"%group0 = OpDecorationGroup\n"
		"%group1 = OpDecorationGroup\n"
		"%group3 = OpDecorationGroup\n";

	const char						typesAndConstants[]	=
		"%a3f32     = OpTypeArray %f32 %c_u32_3\n"
		"%struct1   = OpTypeStruct %a3f32\n"
		"%struct2   = OpTypeStruct %a3f32\n"
		"%fp_struct1 = OpTypePointer Function %struct1\n"
		"%fp_struct2 = OpTypePointer Function %struct2\n"
		"%c_f32_2    = OpConstant %f32 2.\n"
		"%c_f32_n2   = OpConstant %f32 -2.\n"

		"%c_a3f32_1 = OpConstantComposite %a3f32 %c_f32_1 %c_f32_2 %c_f32_1\n"
		"%c_a3f32_2 = OpConstantComposite %a3f32 %c_f32_n1 %c_f32_n2 %c_f32_n1\n"
		"%c_struct1 = OpConstantComposite %struct1 %c_a3f32_1\n"
		"%c_struct2 = OpConstantComposite %struct2 %c_a3f32_2\n";

	const char						function[]			=
		"%test_code = OpFunction %v4f32 None %v4f32_function\n"
		"%param     = OpFunctionParameter %v4f32\n"
		"%entry     = OpLabel\n"
		"%result    = OpVariable %fp_v4f32 Function\n"
		"%v_struct1 = OpVariable %fp_struct1 Function\n"
		"%v_struct2 = OpVariable %fp_struct2 Function\n"
		"             OpStore %result %param\n"
		"             OpStore %v_struct1 %c_struct1\n"
		"             OpStore %v_struct2 %c_struct2\n"
		"%ptr1      = OpAccessChain %fp_f32 %v_struct1 %c_i32_0 %c_i32_2\n"
		"%val1      = OpLoad %f32 %ptr1\n"
		"%ptr2      = OpAccessChain %fp_f32 %v_struct2 %c_i32_0 %c_i32_2\n"
		"%val2      = OpLoad %f32 %ptr2\n"
		"%addvalues = OpFAdd %f32 %val1 %val2\n"
		"%ptr       = OpAccessChain %fp_f32 %result %c_i32_1\n"
		"%val       = OpLoad %f32 %ptr\n"
		"%addresult = OpFAdd %f32 %addvalues %val\n"
		"             OpStore %ptr %addresult\n"
		"%ret       = OpLoad %v4f32 %result\n"
		"             OpReturnValue %ret\n"
		"             OpFunctionEnd\n";

	struct CaseNameDecoration
	{
		string name;
		string decoration;
	};

	CaseNameDecoration tests[] =
	{
		{
			"same_decoration_group_on_multiple_types",
			"OpGroupMemberDecorate %struct_member_group %struct1 0 %struct2 0\n"
		},
		{
			"empty_decoration_group",
			"OpGroupDecorate %group0      %a3f32\n"
			"OpGroupDecorate %group0      %result\n"
		},
		{
			"one_element_decoration_group",
			"OpGroupDecorate %array_group %a3f32\n"
		},
		{
			"multiple_elements_decoration_group",
			"OpGroupDecorate %group3      %v_struct1\n"
		},
		{
			"multiple_decoration_groups_on_same_variable",
			"OpGroupDecorate %group0      %v_struct2\n"
			"OpGroupDecorate %group1      %v_struct2\n"
			"OpGroupDecorate %group3      %v_struct2\n"
		},
		{
			"same_decoration_group_multiple_times",
			"OpGroupDecorate %group1      %addvalues\n"
			"OpGroupDecorate %group1      %addvalues\n"
			"OpGroupDecorate %group1      %addvalues\n"
		},

	};

	getHalfColorsFullAlpha(inputColors);
	getHalfColorsFullAlpha(outputColors);

	for (size_t idx = 0; idx < (sizeof(tests) / sizeof(tests[0])); ++idx)
	{
		fragments["decoration"]	= decorations + tests[idx].decoration;
		fragments["pre_main"]	= typesAndConstants;
		fragments["testfun"]	= function;

		createTestsForAllStages(tests[idx].name, inputColors, outputColors, fragments, group.get());
	}

	return group.release();
}

struct SpecConstantTwoIntGraphicsCase
{
	const char*		caseName;
	const char*		scDefinition0;
	const char*		scDefinition1;
	const char*		scResultType;
	const char*		scOperation;
	deInt32			scActualValue0;
	deInt32			scActualValue1;
	const char*		resultOperation;
	RGBA			expectedColors[4];

					SpecConstantTwoIntGraphicsCase (const char* name,
											const char* definition0,
											const char* definition1,
											const char* resultType,
											const char* operation,
											deInt32		value0,
											deInt32		value1,
											const char* resultOp,
											const RGBA	(&output)[4])
						: caseName			(name)
						, scDefinition0		(definition0)
						, scDefinition1		(definition1)
						, scResultType		(resultType)
						, scOperation		(operation)
						, scActualValue0	(value0)
						, scActualValue1	(value1)
						, resultOperation	(resultOp)
	{
		expectedColors[0] = output[0];
		expectedColors[1] = output[1];
		expectedColors[2] = output[2];
		expectedColors[3] = output[3];
	}
};

tcu::TestCaseGroup* createSpecConstantTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> group				(new tcu::TestCaseGroup(testCtx, "opspecconstantop", "Test the OpSpecConstantOp instruction"));
	vector<SpecConstantTwoIntGraphicsCase>	cases;
	RGBA							inputColors[4];
	RGBA							outputColors0[4];
	RGBA							outputColors1[4];
	RGBA							outputColors2[4];

	const char	decorations1[]			=
		"OpDecorate %sc_0  SpecId 0\n"
		"OpDecorate %sc_1  SpecId 1\n";

	const char	typesAndConstants1[]	=
		"%sc_0      = OpSpecConstant${SC_DEF0}\n"
		"%sc_1      = OpSpecConstant${SC_DEF1}\n"
		"%sc_op     = OpSpecConstantOp ${SC_RESULT_TYPE} ${SC_OP}\n";

	const char	function1[]				=
		"%test_code = OpFunction %v4f32 None %v4f32_function\n"
		"%param     = OpFunctionParameter %v4f32\n"
		"%label     = OpLabel\n"
		"%result    = OpVariable %fp_v4f32 Function\n"
		"             OpStore %result %param\n"
		"%gen       = ${GEN_RESULT}\n"
		"%index     = OpIAdd %i32 %gen %c_i32_1\n"
		"%loc       = OpAccessChain %fp_f32 %result %index\n"
		"%val       = OpLoad %f32 %loc\n"
		"%add       = OpFAdd %f32 %val %c_f32_0_5\n"
		"             OpStore %loc %add\n"
		"%ret       = OpLoad %v4f32 %result\n"
		"             OpReturnValue %ret\n"
		"             OpFunctionEnd\n";

	inputColors[0] = RGBA(127, 127, 127, 255);
	inputColors[1] = RGBA(127, 0,   0,   255);
	inputColors[2] = RGBA(0,   127, 0,   255);
	inputColors[3] = RGBA(0,   0,   127, 255);

	// Derived from inputColors[x] by adding 128 to inputColors[x][0].
	outputColors0[0] = RGBA(255, 127, 127, 255);
	outputColors0[1] = RGBA(255, 0,   0,   255);
	outputColors0[2] = RGBA(128, 127, 0,   255);
	outputColors0[3] = RGBA(128, 0,   127, 255);

	// Derived from inputColors[x] by adding 128 to inputColors[x][1].
	outputColors1[0] = RGBA(127, 255, 127, 255);
	outputColors1[1] = RGBA(127, 128, 0,   255);
	outputColors1[2] = RGBA(0,   255, 0,   255);
	outputColors1[3] = RGBA(0,   128, 127, 255);

	// Derived from inputColors[x] by adding 128 to inputColors[x][2].
	outputColors2[0] = RGBA(127, 127, 255, 255);
	outputColors2[1] = RGBA(127, 0,   128, 255);
	outputColors2[2] = RGBA(0,   127, 128, 255);
	outputColors2[3] = RGBA(0,   0,   255, 255);

	const char addZeroToSc[]		= "OpIAdd %i32 %c_i32_0 %sc_op";
	const char selectTrueUsingSc[]	= "OpSelect %i32 %sc_op %c_i32_1 %c_i32_0";
	const char selectFalseUsingSc[]	= "OpSelect %i32 %sc_op %c_i32_0 %c_i32_1";

	cases.push_back(SpecConstantTwoIntGraphicsCase("iadd",					" %i32 0",		" %i32 0",		"%i32",		"IAdd                 %sc_0 %sc_1",				19,		-20,	addZeroToSc,		outputColors0));
	cases.push_back(SpecConstantTwoIntGraphicsCase("isub",					" %i32 0",		" %i32 0",		"%i32",		"ISub                 %sc_0 %sc_1",				19,		20,		addZeroToSc,		outputColors0));
	cases.push_back(SpecConstantTwoIntGraphicsCase("imul",					" %i32 0",		" %i32 0",		"%i32",		"IMul                 %sc_0 %sc_1",				-1,		-1,		addZeroToSc,		outputColors2));
	cases.push_back(SpecConstantTwoIntGraphicsCase("sdiv",					" %i32 0",		" %i32 0",		"%i32",		"SDiv                 %sc_0 %sc_1",				-126,	126,	addZeroToSc,		outputColors0));
	cases.push_back(SpecConstantTwoIntGraphicsCase("udiv",					" %i32 0",		" %i32 0",		"%i32",		"UDiv                 %sc_0 %sc_1",				126,	126,	addZeroToSc,		outputColors2));
	cases.push_back(SpecConstantTwoIntGraphicsCase("srem",					" %i32 0",		" %i32 0",		"%i32",		"SRem                 %sc_0 %sc_1",				3,		2,		addZeroToSc,		outputColors2));
	cases.push_back(SpecConstantTwoIntGraphicsCase("smod",					" %i32 0",		" %i32 0",		"%i32",		"SMod                 %sc_0 %sc_1",				3,		2,		addZeroToSc,		outputColors2));
	cases.push_back(SpecConstantTwoIntGraphicsCase("umod",					" %i32 0",		" %i32 0",		"%i32",		"UMod                 %sc_0 %sc_1",				1001,	500,	addZeroToSc,		outputColors2));
	cases.push_back(SpecConstantTwoIntGraphicsCase("bitwiseand",			" %i32 0",		" %i32 0",		"%i32",		"BitwiseAnd           %sc_0 %sc_1",				0x33,	0x0d,	addZeroToSc,		outputColors2));
	cases.push_back(SpecConstantTwoIntGraphicsCase("bitwiseor",				" %i32 0",		" %i32 0",		"%i32",		"BitwiseOr            %sc_0 %sc_1",				0,		1,		addZeroToSc,		outputColors2));
	cases.push_back(SpecConstantTwoIntGraphicsCase("bitwisexor",			" %i32 0",		" %i32 0",		"%i32",		"BitwiseXor           %sc_0 %sc_1",				0x2e,	0x2f,	addZeroToSc,		outputColors2));
	cases.push_back(SpecConstantTwoIntGraphicsCase("shiftrightlogical",		" %i32 0",		" %i32 0",		"%i32",		"ShiftRightLogical    %sc_0 %sc_1",				2,		1,		addZeroToSc,		outputColors2));
	cases.push_back(SpecConstantTwoIntGraphicsCase("shiftrightarithmetic",	" %i32 0",		" %i32 0",		"%i32",		"ShiftRightArithmetic %sc_0 %sc_1",				-4,		2,		addZeroToSc,		outputColors0));
	cases.push_back(SpecConstantTwoIntGraphicsCase("shiftleftlogical",		" %i32 0",		" %i32 0",		"%i32",		"ShiftLeftLogical     %sc_0 %sc_1",				1,		0,		addZeroToSc,		outputColors2));
	cases.push_back(SpecConstantTwoIntGraphicsCase("slessthan",				" %i32 0",		" %i32 0",		"%bool",	"SLessThan            %sc_0 %sc_1",				-20,	-10,	selectTrueUsingSc,	outputColors2));
	cases.push_back(SpecConstantTwoIntGraphicsCase("ulessthan",				" %i32 0",		" %i32 0",		"%bool",	"ULessThan            %sc_0 %sc_1",				10,		20,		selectTrueUsingSc,	outputColors2));
	cases.push_back(SpecConstantTwoIntGraphicsCase("sgreaterthan",			" %i32 0",		" %i32 0",		"%bool",	"SGreaterThan         %sc_0 %sc_1",				-1000,	50,		selectFalseUsingSc,	outputColors2));
	cases.push_back(SpecConstantTwoIntGraphicsCase("ugreaterthan",			" %i32 0",		" %i32 0",		"%bool",	"UGreaterThan         %sc_0 %sc_1",				10,		5,		selectTrueUsingSc,	outputColors2));
	cases.push_back(SpecConstantTwoIntGraphicsCase("slessthanequal",		" %i32 0",		" %i32 0",		"%bool",	"SLessThanEqual       %sc_0 %sc_1",				-10,	-10,	selectTrueUsingSc,	outputColors2));
	cases.push_back(SpecConstantTwoIntGraphicsCase("ulessthanequal",		" %i32 0",		" %i32 0",		"%bool",	"ULessThanEqual       %sc_0 %sc_1",				50,		100,	selectTrueUsingSc,	outputColors2));
	cases.push_back(SpecConstantTwoIntGraphicsCase("sgreaterthanequal",		" %i32 0",		" %i32 0",		"%bool",	"SGreaterThanEqual    %sc_0 %sc_1",				-1000,	50,		selectFalseUsingSc,	outputColors2));
	cases.push_back(SpecConstantTwoIntGraphicsCase("ugreaterthanequal",		" %i32 0",		" %i32 0",		"%bool",	"UGreaterThanEqual    %sc_0 %sc_1",				10,		10,		selectTrueUsingSc,	outputColors2));
	cases.push_back(SpecConstantTwoIntGraphicsCase("iequal",				" %i32 0",		" %i32 0",		"%bool",	"IEqual               %sc_0 %sc_1",				42,		24,		selectFalseUsingSc,	outputColors2));
	cases.push_back(SpecConstantTwoIntGraphicsCase("logicaland",			"True %bool",	"True %bool",	"%bool",	"LogicalAnd           %sc_0 %sc_1",				0,		1,		selectFalseUsingSc,	outputColors2));
	cases.push_back(SpecConstantTwoIntGraphicsCase("logicalor",				"False %bool",	"False %bool",	"%bool",	"LogicalOr            %sc_0 %sc_1",				1,		0,		selectTrueUsingSc,	outputColors2));
	cases.push_back(SpecConstantTwoIntGraphicsCase("logicalequal",			"True %bool",	"True %bool",	"%bool",	"LogicalEqual         %sc_0 %sc_1",				0,		1,		selectFalseUsingSc,	outputColors2));
	cases.push_back(SpecConstantTwoIntGraphicsCase("logicalnotequal",		"False %bool",	"False %bool",	"%bool",	"LogicalNotEqual      %sc_0 %sc_1",				1,		0,		selectTrueUsingSc,	outputColors2));
	cases.push_back(SpecConstantTwoIntGraphicsCase("snegate",				" %i32 0",		" %i32 0",		"%i32",		"SNegate              %sc_0",					-1,		0,		addZeroToSc,		outputColors2));
	cases.push_back(SpecConstantTwoIntGraphicsCase("not",					" %i32 0",		" %i32 0",		"%i32",		"Not                  %sc_0",					-2,		0,		addZeroToSc,		outputColors2));
	cases.push_back(SpecConstantTwoIntGraphicsCase("logicalnot",			"False %bool",	"False %bool",	"%bool",	"LogicalNot           %sc_0",					1,		0,		selectFalseUsingSc,	outputColors2));
	cases.push_back(SpecConstantTwoIntGraphicsCase("select",				"False %bool",	" %i32 0",		"%i32",		"Select               %sc_0 %sc_1 %c_i32_0",	1,		1,		addZeroToSc,		outputColors2));
	// OpSConvert, OpFConvert: these two instructions involve ints/floats of different bitwidths.
	// \todo[2015-12-1 antiagainst] OpQuantizeToF16

	for (size_t caseNdx = 0; caseNdx < cases.size(); ++caseNdx)
	{
		map<string, string>	specializations;
		map<string, string>	fragments;
		vector<deInt32>		specConstants;

		specializations["SC_DEF0"]			= cases[caseNdx].scDefinition0;
		specializations["SC_DEF1"]			= cases[caseNdx].scDefinition1;
		specializations["SC_RESULT_TYPE"]	= cases[caseNdx].scResultType;
		specializations["SC_OP"]			= cases[caseNdx].scOperation;
		specializations["GEN_RESULT"]		= cases[caseNdx].resultOperation;

		fragments["decoration"]				= tcu::StringTemplate(decorations1).specialize(specializations);
		fragments["pre_main"]				= tcu::StringTemplate(typesAndConstants1).specialize(specializations);
		fragments["testfun"]				= tcu::StringTemplate(function1).specialize(specializations);

		specConstants.push_back(cases[caseNdx].scActualValue0);
		specConstants.push_back(cases[caseNdx].scActualValue1);

		createTestsForAllStages(cases[caseNdx].caseName, inputColors, cases[caseNdx].expectedColors, fragments, specConstants, group.get());
	}

	const char	decorations2[]			=
		"OpDecorate %sc_0  SpecId 0\n"
		"OpDecorate %sc_1  SpecId 1\n"
		"OpDecorate %sc_2  SpecId 2\n";

	const char	typesAndConstants2[]	=
		"%v3i32     = OpTypeVector %i32 3\n"

		"%sc_0      = OpSpecConstant %i32 0\n"
		"%sc_1      = OpSpecConstant %i32 0\n"
		"%sc_2      = OpSpecConstant %i32 0\n"

		"%vec3_0      = OpConstantComposite %v3i32 %c_i32_0 %c_i32_0 %c_i32_0\n"
		"%sc_vec3_0   = OpSpecConstantOp %v3i32 CompositeInsert  %sc_0        %vec3_0    0\n"     // (sc_0, 0, 0)
		"%sc_vec3_1   = OpSpecConstantOp %v3i32 CompositeInsert  %sc_1        %vec3_0    1\n"     // (0, sc_1, 0)
		"%sc_vec3_2   = OpSpecConstantOp %v3i32 CompositeInsert  %sc_2        %vec3_0    2\n"     // (0, 0, sc_2)
		"%sc_vec3_01  = OpSpecConstantOp %v3i32 VectorShuffle    %sc_vec3_0   %sc_vec3_1 1 0 4\n" // (0,    sc_0, sc_1)
		"%sc_vec3_012 = OpSpecConstantOp %v3i32 VectorShuffle    %sc_vec3_01  %sc_vec3_2 5 1 2\n" // (sc_2, sc_0, sc_1)
		"%sc_ext_0    = OpSpecConstantOp %i32   CompositeExtract %sc_vec3_012            0\n"     // sc_2
		"%sc_ext_1    = OpSpecConstantOp %i32   CompositeExtract %sc_vec3_012            1\n"     // sc_0
		"%sc_ext_2    = OpSpecConstantOp %i32   CompositeExtract %sc_vec3_012            2\n"     // sc_1
		"%sc_sub      = OpSpecConstantOp %i32   ISub             %sc_ext_0    %sc_ext_1\n"        // (sc_2 - sc_0)
		"%sc_final    = OpSpecConstantOp %i32   IMul             %sc_sub      %sc_ext_2\n";       // (sc_2 - sc_0) * sc_1

	const char	function2[]				=
		"%test_code = OpFunction %v4f32 None %v4f32_function\n"
		"%param     = OpFunctionParameter %v4f32\n"
		"%label     = OpLabel\n"
		"%result    = OpVariable %fp_v4f32 Function\n"
		"             OpStore %result %param\n"
		"%loc       = OpAccessChain %fp_f32 %result %sc_final\n"
		"%val       = OpLoad %f32 %loc\n"
		"%add       = OpFAdd %f32 %val %c_f32_0_5\n"
		"             OpStore %loc %add\n"
		"%ret       = OpLoad %v4f32 %result\n"
		"             OpReturnValue %ret\n"
		"             OpFunctionEnd\n";

	map<string, string>	fragments;
	vector<deInt32>		specConstants;

	fragments["decoration"]	= decorations2;
	fragments["pre_main"]	= typesAndConstants2;
	fragments["testfun"]	= function2;

	specConstants.push_back(56789);
	specConstants.push_back(-2);
	specConstants.push_back(56788);

	createTestsForAllStages("vector_related", inputColors, outputColors2, fragments, specConstants, group.get());

	return group.release();
}

tcu::TestCaseGroup* createOpPhiTests(tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> group				(new tcu::TestCaseGroup(testCtx, "opphi", "Test the OpPhi instruction"));
	RGBA							inputColors[4];
	RGBA							outputColors1[4];
	RGBA							outputColors2[4];
	RGBA							outputColors3[4];
	map<string, string>				fragments1;
	map<string, string>				fragments2;
	map<string, string>				fragments3;

	const char	typesAndConstants1[]	=
		"%c_f32_p2  = OpConstant %f32 0.2\n"
		"%c_f32_p4  = OpConstant %f32 0.4\n"
		"%c_f32_p5  = OpConstant %f32 0.5\n"
		"%c_f32_p8  = OpConstant %f32 0.8\n";

	// vec4 test_code(vec4 param) {
	//   vec4 result = param;
	//   for (int i = 0; i < 4; ++i) {
	//     float operand;
	//     switch (i) {
	//       case 0: operand = .2; break;
	//       case 1: operand = .5; break;
	//       case 2: operand = .4; break;
	//       case 3: operand = .0; break;
	//       default: break; // unreachable
	//     }
	//     result[i] += operand;
	//   }
	//   return result;
	// }
	const char	function1[]				=
		"%test_code = OpFunction %v4f32 None %v4f32_function\n"
		"%param1    = OpFunctionParameter %v4f32\n"
		"%lbl       = OpLabel\n"
		"%iptr      = OpVariable %fp_i32 Function\n"
		"%result    = OpVariable %fp_v4f32 Function\n"
		"             OpStore %iptr %c_i32_0\n"
		"             OpStore %result %param1\n"
		"             OpBranch %loop\n"

		"%loop      = OpLabel\n"
		"%ival      = OpLoad %i32 %iptr\n"
		"%lt_4      = OpSLessThan %bool %ival %c_i32_4\n"
		"             OpLoopMerge %exit %loop None\n"
		"             OpBranchConditional %lt_4 %entry %exit\n"

		"%entry     = OpLabel\n"
		"%loc       = OpAccessChain %fp_f32 %result %ival\n"
		"%val       = OpLoad %f32 %loc\n"
		"             OpSelectionMerge %phi None\n"
		"             OpSwitch %ival %default 0 %case0 1 %case1 2 %case2 3 %case3\n"

		"%case0     = OpLabel\n"
		"             OpBranch %phi\n"
		"%case1     = OpLabel\n"
		"             OpBranch %phi\n"
		"%case2     = OpLabel\n"
		"             OpBranch %phi\n"
		"%case3     = OpLabel\n"
		"             OpBranch %phi\n"

		"%default   = OpLabel\n"
		"             OpUnreachable\n"

		"%phi       = OpLabel\n"
		"%operand   = OpPhi %f32 %c_f32_p4 %case2 %c_f32_p5 %case1 %c_f32_p2 %case0 %c_f32_0 %case3\n" // not in the order of blocks
		"%add       = OpFAdd %f32 %val %operand\n"
		"             OpStore %loc %add\n"
		"%ival_next = OpIAdd %i32 %ival %c_i32_1\n"
		"             OpStore %iptr %ival_next\n"
		"             OpBranch %loop\n"

		"%exit      = OpLabel\n"
		"%ret       = OpLoad %v4f32 %result\n"
		"             OpReturnValue %ret\n"

		"             OpFunctionEnd\n";

	fragments1["pre_main"]	= typesAndConstants1;
	fragments1["testfun"]	= function1;

	getHalfColorsFullAlpha(inputColors);

	outputColors1[0]		= RGBA(178, 255, 229, 255);
	outputColors1[1]		= RGBA(178, 127, 102, 255);
	outputColors1[2]		= RGBA(51,  255, 102, 255);
	outputColors1[3]		= RGBA(51,  127, 229, 255);

	createTestsForAllStages("out_of_order", inputColors, outputColors1, fragments1, group.get());

	const char	typesAndConstants2[]	=
		"%c_f32_p2  = OpConstant %f32 0.2\n";

	// Add .4 to the second element of the given parameter.
	const char	function2[]				=
		"%test_code = OpFunction %v4f32 None %v4f32_function\n"
		"%param     = OpFunctionParameter %v4f32\n"
		"%entry     = OpLabel\n"
		"%result    = OpVariable %fp_v4f32 Function\n"
		"             OpStore %result %param\n"
		"%loc       = OpAccessChain %fp_f32 %result %c_i32_1\n"
		"%val       = OpLoad %f32 %loc\n"
		"             OpBranch %phi\n"

		"%phi        = OpLabel\n"
		"%step       = OpPhi %i32 %c_i32_0  %entry %step_next  %phi\n"
		"%accum      = OpPhi %f32 %val      %entry %accum_next %phi\n"
		"%step_next  = OpIAdd %i32 %step  %c_i32_1\n"
		"%accum_next = OpFAdd %f32 %accum %c_f32_p2\n"
		"%still_loop = OpSLessThan %bool %step %c_i32_2\n"
		"              OpLoopMerge %exit %phi None\n"
		"              OpBranchConditional %still_loop %phi %exit\n"

		"%exit       = OpLabel\n"
		"              OpStore %loc %accum\n"
		"%ret        = OpLoad %v4f32 %result\n"
		"              OpReturnValue %ret\n"

		"              OpFunctionEnd\n";

	fragments2["pre_main"]	= typesAndConstants2;
	fragments2["testfun"]	= function2;

	outputColors2[0]			= RGBA(127, 229, 127, 255);
	outputColors2[1]			= RGBA(127, 102, 0,   255);
	outputColors2[2]			= RGBA(0,   229, 0,   255);
	outputColors2[3]			= RGBA(0,   102, 127, 255);

	createTestsForAllStages("induction", inputColors, outputColors2, fragments2, group.get());

	const char	typesAndConstants3[]	=
		"%true      = OpConstantTrue %bool\n"
		"%false     = OpConstantFalse %bool\n"
		"%c_f32_p2  = OpConstant %f32 0.2\n";

	// Swap the second and the third element of the given parameter.
	const char	function3[]				=
		"%test_code = OpFunction %v4f32 None %v4f32_function\n"
		"%param     = OpFunctionParameter %v4f32\n"
		"%entry     = OpLabel\n"
		"%result    = OpVariable %fp_v4f32 Function\n"
		"             OpStore %result %param\n"
		"%a_loc     = OpAccessChain %fp_f32 %result %c_i32_1\n"
		"%a_init    = OpLoad %f32 %a_loc\n"
		"%b_loc     = OpAccessChain %fp_f32 %result %c_i32_2\n"
		"%b_init    = OpLoad %f32 %b_loc\n"
		"             OpBranch %phi\n"

		"%phi        = OpLabel\n"
		"%still_loop = OpPhi %bool %true   %entry %false  %phi\n"
		"%a_next     = OpPhi %f32  %a_init %entry %b_next %phi\n"
		"%b_next     = OpPhi %f32  %b_init %entry %a_next %phi\n"
		"              OpLoopMerge %exit %phi None\n"
		"              OpBranchConditional %still_loop %phi %exit\n"

		"%exit       = OpLabel\n"
		"              OpStore %a_loc %a_next\n"
		"              OpStore %b_loc %b_next\n"
		"%ret        = OpLoad %v4f32 %result\n"
		"              OpReturnValue %ret\n"

		"              OpFunctionEnd\n";

	fragments3["pre_main"]	= typesAndConstants3;
	fragments3["testfun"]	= function3;

	outputColors3[0]			= RGBA(127, 127, 127, 255);
	outputColors3[1]			= RGBA(127, 0,   0,   255);
	outputColors3[2]			= RGBA(0,   0,   127, 255);
	outputColors3[3]			= RGBA(0,   127, 0,   255);

	createTestsForAllStages("swap", inputColors, outputColors3, fragments3, group.get());

	return group.release();
}

tcu::TestCaseGroup* createNoContractionTests(tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> group			(new tcu::TestCaseGroup(testCtx, "nocontraction", "Test the NoContraction decoration"));
	RGBA							inputColors[4];
	RGBA							outputColors[4];

	// With NoContraction, (1 + 2^-23) * (1 - 2^-23) - 1 should be conducted as a multiplication and an addition separately.
	// For the multiplication, the result is 1 - 2^-46, which is out of the precision range for 32-bit float. (32-bit float
	// only have 23-bit fraction.) So it will be rounded to 1. Or 0x1.fffffc. Then the final result is 0 or -0x1p-24.
	// On the contrary, the result will be 2^-46, which is a normalized number perfectly representable as 32-bit float.
	const char						constantsAndTypes[]	 =
		"%c_vec4_0       = OpConstantComposite %v4f32 %c_f32_0 %c_f32_0 %c_f32_0 %c_f32_1\n"
		"%c_vec4_1       = OpConstantComposite %v4f32 %c_f32_1 %c_f32_1 %c_f32_1 %c_f32_1\n"
		"%c_f32_1pl2_23  = OpConstant %f32 0x1.000002p+0\n" // 1 + 2^-23
		"%c_f32_1mi2_23  = OpConstant %f32 0x1.fffffcp-1\n" // 1 - 2^-23
		"%c_f32_n1pn24   = OpConstant %f32 -0x1p-24\n"
		;

	const char						function[]	 =
		"%test_code      = OpFunction %v4f32 None %v4f32_function\n"
		"%param          = OpFunctionParameter %v4f32\n"
		"%label          = OpLabel\n"
		"%var1           = OpVariable %fp_f32 Function %c_f32_1pl2_23\n"
		"%var2           = OpVariable %fp_f32 Function\n"
		"%red            = OpCompositeExtract %f32 %param 0\n"
		"%plus_red       = OpFAdd %f32 %c_f32_1mi2_23 %red\n"
		"                  OpStore %var2 %plus_red\n"
		"%val1           = OpLoad %f32 %var1\n"
		"%val2           = OpLoad %f32 %var2\n"
		"%mul            = OpFMul %f32 %val1 %val2\n"
		"%add            = OpFAdd %f32 %mul %c_f32_n1\n"
		"%is0            = OpFOrdEqual %bool %add %c_f32_0\n"
		"%isn1n24         = OpFOrdEqual %bool %add %c_f32_n1pn24\n"
		"%success        = OpLogicalOr %bool %is0 %isn1n24\n"
		"%v4success      = OpCompositeConstruct %v4bool %success %success %success %success\n"
		"%ret            = OpSelect %v4f32 %v4success %c_vec4_0 %c_vec4_1\n"
		"                  OpReturnValue %ret\n"
		"                  OpFunctionEnd\n";

	struct CaseNameDecoration
	{
		string name;
		string decoration;
	};


	CaseNameDecoration tests[] = {
		{"multiplication",	"OpDecorate %mul NoContraction"},
		{"addition",		"OpDecorate %add NoContraction"},
		{"both",			"OpDecorate %mul NoContraction\nOpDecorate %add NoContraction"},
	};

	getHalfColorsFullAlpha(inputColors);

	for (deUint8 idx = 0; idx < 4; ++idx)
	{
		inputColors[idx].setRed(0);
		outputColors[idx] = RGBA(0, 0, 0, 255);
	}

	for (size_t testNdx = 0; testNdx < sizeof(tests) / sizeof(CaseNameDecoration); ++testNdx)
	{
		map<string, string> fragments;

		fragments["decoration"] = tests[testNdx].decoration;
		fragments["pre_main"] = constantsAndTypes;
		fragments["testfun"] = function;

		createTestsForAllStages(tests[testNdx].name, inputColors, outputColors, fragments, group.get());
	}

	return group.release();
}

tcu::TestCaseGroup* createMemoryAccessTests(tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> memoryAccessTests (new tcu::TestCaseGroup(testCtx, "opmemoryaccess", "Memory Semantics"));
	RGBA							colors[4];

	const char						constantsAndTypes[]	 =
		"%c_a2f32_1         = OpConstantComposite %a2f32 %c_f32_1 %c_f32_1\n"
		"%fp_a2f32          = OpTypePointer Function %a2f32\n"
		"%stype             = OpTypeStruct  %v4f32 %a2f32 %f32\n"
		"%fp_stype          = OpTypePointer Function %stype\n";

	const char						function[]	 =
		"%test_code         = OpFunction %v4f32 None %v4f32_function\n"
		"%param1            = OpFunctionParameter %v4f32\n"
		"%lbl               = OpLabel\n"
		"%v1                = OpVariable %fp_v4f32 Function\n"
		"%v2                = OpVariable %fp_a2f32 Function\n"
		"%v3                = OpVariable %fp_f32 Function\n"
		"%v                 = OpVariable %fp_stype Function\n"
		"%vv                = OpVariable %fp_stype Function\n"
		"%vvv               = OpVariable %fp_f32 Function\n"

		"                     OpStore %v1 %c_v4f32_1_1_1_1\n"
		"                     OpStore %v2 %c_a2f32_1\n"
		"                     OpStore %v3 %c_f32_1\n"

		"%p_v4f32          = OpAccessChain %fp_v4f32 %v %c_u32_0\n"
		"%p_a2f32          = OpAccessChain %fp_a2f32 %v %c_u32_1\n"
		"%p_f32            = OpAccessChain %fp_f32 %v %c_u32_2\n"
		"%v1_v             = OpLoad %v4f32 %v1 ${access_type}\n"
		"%v2_v             = OpLoad %a2f32 %v2 ${access_type}\n"
		"%v3_v             = OpLoad %f32 %v3 ${access_type}\n"

		"                    OpStore %p_v4f32 %v1_v ${access_type}\n"
		"                    OpStore %p_a2f32 %v2_v ${access_type}\n"
		"                    OpStore %p_f32 %v3_v ${access_type}\n"

		"                    OpCopyMemory %vv %v ${access_type}\n"
		"                    OpCopyMemory %vvv %p_f32 ${access_type}\n"

		"%p_f32_2          = OpAccessChain %fp_f32 %vv %c_u32_2\n"
		"%v_f32_2          = OpLoad %f32 %p_f32_2\n"
		"%v_f32_3          = OpLoad %f32 %vvv\n"

		"%ret1             = OpVectorTimesScalar %v4f32 %param1 %v_f32_2\n"
		"%ret2             = OpVectorTimesScalar %v4f32 %ret1 %v_f32_3\n"
		"                    OpReturnValue %ret2\n"
		"                    OpFunctionEnd\n";

	struct NameMemoryAccess
	{
		string name;
		string accessType;
	};


	NameMemoryAccess tests[] =
	{
		{ "none", "" },
		{ "volatile", "Volatile" },
		{ "aligned",  "Aligned 1" },
		{ "volatile_aligned",  "Volatile|Aligned 1" },
		{ "nontemporal_aligned",  "Nontemporal|Aligned 1" },
		{ "volatile_nontemporal",  "Volatile|Nontemporal" },
		{ "volatile_nontermporal_aligned",  "Volatile|Nontemporal|Aligned 1" },
	};

	getHalfColorsFullAlpha(colors);

	for (size_t testNdx = 0; testNdx < sizeof(tests) / sizeof(NameMemoryAccess); ++testNdx)
	{
		map<string, string> fragments;
		map<string, string> memoryAccess;
		memoryAccess["access_type"] = tests[testNdx].accessType;

		fragments["pre_main"] = constantsAndTypes;
		fragments["testfun"] = tcu::StringTemplate(function).specialize(memoryAccess);
		createTestsForAllStages(tests[testNdx].name, colors, colors, fragments, memoryAccessTests.get());
	}
	return memoryAccessTests.release();
}
tcu::TestCaseGroup* createOpUndefTests(tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup>		opUndefTests		 (new tcu::TestCaseGroup(testCtx, "opundef", "Test OpUndef"));
	RGBA								defaultColors[4];
	map<string, string>					fragments;
	getDefaultColors(defaultColors);

	// First, simple cases that don't do anything with the OpUndef result.
	fragments["testfun"] =
		"%test_code = OpFunction %v4f32 None %v4f32_function\n"
		"%param1 = OpFunctionParameter %v4f32\n"
		"%label_testfun = OpLabel\n"
		"%undef = OpUndef %type\n"
		"OpReturnValue %param1\n"
		"OpFunctionEnd\n"
		;
	struct NameCodePair { string name, code; };
	const NameCodePair tests[] =
	{
		{"bool", "%type = OpTypeBool"},
		{"vec2uint32", "%type = OpTypeVector %u32 2"},
		{"image", "%type = OpTypeImage %f32 2D 0 0 0 1 Unknown"},
		{"sampler", "%type = OpTypeSampler"},
		{"sampledimage", "%img = OpTypeImage %f32 2D 0 0 0 1 Unknown\n" "%type = OpTypeSampledImage %img"},
		{"pointer", "%type = OpTypePointer Function %i32"},
		{"runtimearray", "%type = OpTypeRuntimeArray %f32"},
		{"array", "%c_u32_100 = OpConstant %u32 100\n" "%type = OpTypeArray %i32 %c_u32_100"},
		{"struct", "%type = OpTypeStruct %f32 %i32 %u32"}};
	for (size_t testNdx = 0; testNdx < sizeof(tests) / sizeof(NameCodePair); ++testNdx)
	{
		fragments["pre_main"] = tests[testNdx].code;
		createTestsForAllStages(tests[testNdx].name, defaultColors, defaultColors, fragments, opUndefTests.get());
	}
	fragments.clear();

	fragments["testfun"] =
		"%test_code = OpFunction %v4f32 None %v4f32_function\n"
		"%param1 = OpFunctionParameter %v4f32\n"
		"%label_testfun = OpLabel\n"
		"%undef = OpUndef %f32\n"
		"%zero = OpFMul %f32 %undef %c_f32_0\n"
		"%is_nan = OpIsNan %bool %zero\n" //OpUndef may result in NaN which may turn %zero into Nan.
		"%actually_zero = OpSelect %f32 %is_nan %c_f32_0 %zero\n"
		"%a = OpVectorExtractDynamic %f32 %param1 %c_i32_0\n"
		"%b = OpFAdd %f32 %a %actually_zero\n"
		"%ret = OpVectorInsertDynamic %v4f32 %param1 %b %c_i32_0\n"
		"OpReturnValue %ret\n"
		"OpFunctionEnd\n"
		;
	createTestsForAllStages("float32", defaultColors, defaultColors, fragments, opUndefTests.get());

	fragments["testfun"] =
		"%test_code = OpFunction %v4f32 None %v4f32_function\n"
		"%param1 = OpFunctionParameter %v4f32\n"
		"%label_testfun = OpLabel\n"
		"%undef = OpUndef %i32\n"
		"%zero = OpIMul %i32 %undef %c_i32_0\n"
		"%a = OpVectorExtractDynamic %f32 %param1 %zero\n"
		"%ret = OpVectorInsertDynamic %v4f32 %param1 %a %c_i32_0\n"
		"OpReturnValue %ret\n"
		"OpFunctionEnd\n"
		;
	createTestsForAllStages("sint32", defaultColors, defaultColors, fragments, opUndefTests.get());

	fragments["testfun"] =
		"%test_code = OpFunction %v4f32 None %v4f32_function\n"
		"%param1 = OpFunctionParameter %v4f32\n"
		"%label_testfun = OpLabel\n"
		"%undef = OpUndef %u32\n"
		"%zero = OpIMul %u32 %undef %c_i32_0\n"
		"%a = OpVectorExtractDynamic %f32 %param1 %zero\n"
		"%ret = OpVectorInsertDynamic %v4f32 %param1 %a %c_i32_0\n"
		"OpReturnValue %ret\n"
		"OpFunctionEnd\n"
		;
	createTestsForAllStages("uint32", defaultColors, defaultColors, fragments, opUndefTests.get());

	fragments["testfun"] =
		"%test_code = OpFunction %v4f32 None %v4f32_function\n"
		"%param1 = OpFunctionParameter %v4f32\n"
		"%label_testfun = OpLabel\n"
		"%undef = OpUndef %v4f32\n"
		"%vzero = OpVectorTimesScalar %v4f32 %undef %c_f32_0\n"
		"%zero_0 = OpVectorExtractDynamic %f32 %vzero %c_i32_0\n"
		"%zero_1 = OpVectorExtractDynamic %f32 %vzero %c_i32_1\n"
		"%zero_2 = OpVectorExtractDynamic %f32 %vzero %c_i32_2\n"
		"%zero_3 = OpVectorExtractDynamic %f32 %vzero %c_i32_3\n"
		"%is_nan_0 = OpIsNan %bool %zero_0\n"
		"%is_nan_1 = OpIsNan %bool %zero_1\n"
		"%is_nan_2 = OpIsNan %bool %zero_2\n"
		"%is_nan_3 = OpIsNan %bool %zero_3\n"
		"%actually_zero_0 = OpSelect %f32 %is_nan_0 %c_f32_0 %zero_0\n"
		"%actually_zero_1 = OpSelect %f32 %is_nan_0 %c_f32_0 %zero_1\n"
		"%actually_zero_2 = OpSelect %f32 %is_nan_0 %c_f32_0 %zero_2\n"
		"%actually_zero_3 = OpSelect %f32 %is_nan_0 %c_f32_0 %zero_3\n"
		"%param1_0 = OpVectorExtractDynamic %f32 %param1 %c_i32_0\n"
		"%param1_1 = OpVectorExtractDynamic %f32 %param1 %c_i32_1\n"
		"%param1_2 = OpVectorExtractDynamic %f32 %param1 %c_i32_2\n"
		"%param1_3 = OpVectorExtractDynamic %f32 %param1 %c_i32_3\n"
		"%sum_0 = OpFAdd %f32 %param1_0 %actually_zero_0\n"
		"%sum_1 = OpFAdd %f32 %param1_1 %actually_zero_1\n"
		"%sum_2 = OpFAdd %f32 %param1_2 %actually_zero_2\n"
		"%sum_3 = OpFAdd %f32 %param1_3 %actually_zero_3\n"
		"%ret3 = OpVectorInsertDynamic %v4f32 %param1 %sum_3 %c_i32_3\n"
		"%ret2 = OpVectorInsertDynamic %v4f32 %ret3 %sum_2 %c_i32_2\n"
		"%ret1 = OpVectorInsertDynamic %v4f32 %ret2 %sum_1 %c_i32_1\n"
		"%ret = OpVectorInsertDynamic %v4f32 %ret1 %sum_0 %c_i32_0\n"
		"OpReturnValue %ret\n"
		"OpFunctionEnd\n"
		;
	createTestsForAllStages("vec4float32", defaultColors, defaultColors, fragments, opUndefTests.get());

	fragments["pre_main"] =
		"%v2f32 = OpTypeVector %f32 2\n"
		"%m2x2f32 = OpTypeMatrix %v2f32 2\n";
	fragments["testfun"] =
		"%test_code = OpFunction %v4f32 None %v4f32_function\n"
		"%param1 = OpFunctionParameter %v4f32\n"
		"%label_testfun = OpLabel\n"
		"%undef = OpUndef %m2x2f32\n"
		"%mzero = OpMatrixTimesScalar %m2x2f32 %undef %c_f32_0\n"
		"%zero_0 = OpCompositeExtract %f32 %mzero 0 0\n"
		"%zero_1 = OpCompositeExtract %f32 %mzero 0 1\n"
		"%zero_2 = OpCompositeExtract %f32 %mzero 1 0\n"
		"%zero_3 = OpCompositeExtract %f32 %mzero 1 1\n"
		"%is_nan_0 = OpIsNan %bool %zero_0\n"
		"%is_nan_1 = OpIsNan %bool %zero_1\n"
		"%is_nan_2 = OpIsNan %bool %zero_2\n"
		"%is_nan_3 = OpIsNan %bool %zero_3\n"
		"%actually_zero_0 = OpSelect %f32 %is_nan_0 %c_f32_0 %zero_0\n"
		"%actually_zero_1 = OpSelect %f32 %is_nan_0 %c_f32_0 %zero_1\n"
		"%actually_zero_2 = OpSelect %f32 %is_nan_0 %c_f32_0 %zero_2\n"
		"%actually_zero_3 = OpSelect %f32 %is_nan_0 %c_f32_0 %zero_3\n"
		"%param1_0 = OpVectorExtractDynamic %f32 %param1 %c_i32_0\n"
		"%param1_1 = OpVectorExtractDynamic %f32 %param1 %c_i32_1\n"
		"%param1_2 = OpVectorExtractDynamic %f32 %param1 %c_i32_2\n"
		"%param1_3 = OpVectorExtractDynamic %f32 %param1 %c_i32_3\n"
		"%sum_0 = OpFAdd %f32 %param1_0 %actually_zero_0\n"
		"%sum_1 = OpFAdd %f32 %param1_1 %actually_zero_1\n"
		"%sum_2 = OpFAdd %f32 %param1_2 %actually_zero_2\n"
		"%sum_3 = OpFAdd %f32 %param1_3 %actually_zero_3\n"
		"%ret3 = OpVectorInsertDynamic %v4f32 %param1 %sum_3 %c_i32_3\n"
		"%ret2 = OpVectorInsertDynamic %v4f32 %ret3 %sum_2 %c_i32_2\n"
		"%ret1 = OpVectorInsertDynamic %v4f32 %ret2 %sum_1 %c_i32_1\n"
		"%ret = OpVectorInsertDynamic %v4f32 %ret1 %sum_0 %c_i32_0\n"
		"OpReturnValue %ret\n"
		"OpFunctionEnd\n"
		;
	createTestsForAllStages("matrix", defaultColors, defaultColors, fragments, opUndefTests.get());

	return opUndefTests.release();
}

void createOpQuantizeSingleOptionTests(tcu::TestCaseGroup* testCtx)
{
	const RGBA		inputColors[4]		=
	{
		RGBA(0,		0,		0,		255),
		RGBA(0,		0,		255,	255),
		RGBA(0,		255,	0,		255),
		RGBA(0,		255,	255,	255)
	};

	const RGBA		expectedColors[4]	=
	{
		RGBA(255,	 0,		 0,		 255),
		RGBA(255,	 0,		 0,		 255),
		RGBA(255,	 0,		 0,		 255),
		RGBA(255,	 0,		 0,		 255)
	};

	const struct SingleFP16Possibility
	{
		const char* name;
		const char* constant;  // Value to assign to %test_constant.
		float		valueAsFloat;
		const char* condition; // Must assign to %cond an expression that evaluates to true after %c = OpQuantizeToF16(%test_constant + 0).
	}				tests[]				=
	{
		{
			"negative",
			"-0x1.3p1\n",
			-constructNormalizedFloat(1, 0x300000),
			"%cond = OpFOrdEqual %bool %c %test_constant\n"
		}, // -19
		{
			"positive",
			"0x1.0p7\n",
			constructNormalizedFloat(7, 0x000000),
			"%cond = OpFOrdEqual %bool %c %test_constant\n"
		},  // +128
		// SPIR-V requires that OpQuantizeToF16 flushes
		// any numbers that would end up denormalized in F16 to zero.
		{
			"denorm",
			"0x0.0006p-126\n",
			std::ldexp(1.5f, -140),
			"%cond = OpFOrdEqual %bool %c %c_f32_0\n"
		},  // denorm
		{
			"negative_denorm",
			"-0x0.0006p-126\n",
			-std::ldexp(1.5f, -140),
			"%cond = OpFOrdEqual %bool %c %c_f32_0\n"
		}, // -denorm
		{
			"too_small",
			"0x1.0p-16\n",
			std::ldexp(1.0f, -16),
			"%cond = OpFOrdEqual %bool %c %c_f32_0\n"
		},     // too small positive
		{
			"negative_too_small",
			"-0x1.0p-32\n",
			-std::ldexp(1.0f, -32),
			"%cond = OpFOrdEqual %bool %c %c_f32_0\n"
		},      // too small negative
		{
			"negative_inf",
			"-0x1.0p128\n",
			-std::ldexp(1.0f, 128),

			"%gz = OpFOrdLessThan %bool %c %c_f32_0\n"
			"%inf = OpIsInf %bool %c\n"
			"%cond = OpLogicalAnd %bool %gz %inf\n"
		},     // -inf to -inf
		{
			"inf",
			"0x1.0p128\n",
			std::ldexp(1.0f, 128),

			"%gz = OpFOrdGreaterThan %bool %c %c_f32_0\n"
			"%inf = OpIsInf %bool %c\n"
			"%cond = OpLogicalAnd %bool %gz %inf\n"
		},     // +inf to +inf
		{
			"round_to_negative_inf",
			"-0x1.0p32\n",
			-std::ldexp(1.0f, 32),

			"%gz = OpFOrdLessThan %bool %c %c_f32_0\n"
			"%inf = OpIsInf %bool %c\n"
			"%cond = OpLogicalAnd %bool %gz %inf\n"
		},     // round to -inf
		{
			"round_to_inf",
			"0x1.0p16\n",
			std::ldexp(1.0f, 16),

			"%gz = OpFOrdGreaterThan %bool %c %c_f32_0\n"
			"%inf = OpIsInf %bool %c\n"
			"%cond = OpLogicalAnd %bool %gz %inf\n"
		},     // round to +inf
		{
			"nan",
			"0x1.1p128\n",
			std::numeric_limits<float>::quiet_NaN(),

			// Test for any NaN value, as NaNs are not preserved
			"%direct_quant = OpQuantizeToF16 %f32 %test_constant\n"
			"%cond = OpIsNan %bool %direct_quant\n"
		}, // nan
		{
			"negative_nan",
			"-0x1.0001p128\n",
			std::numeric_limits<float>::quiet_NaN(),

			// Test for any NaN value, as NaNs are not preserved
			"%direct_quant = OpQuantizeToF16 %f32 %test_constant\n"
			"%cond = OpIsNan %bool %direct_quant\n"
		} // -nan
	};
	const char*		constants			=
		"%test_constant = OpConstant %f32 ";  // The value will be test.constant.

	StringTemplate	function			(
		"%test_code     = OpFunction %v4f32 None %v4f32_function\n"
		"%param1        = OpFunctionParameter %v4f32\n"
		"%label_testfun = OpLabel\n"
		"%a             = OpVectorExtractDynamic %f32 %param1 %c_i32_0\n"
		"%b             = OpFAdd %f32 %test_constant %a\n"
		"%c             = OpQuantizeToF16 %f32 %b\n"
		"${condition}\n"
		"%v4cond        = OpCompositeConstruct %v4bool %cond %cond %cond %cond\n"
		"%retval        = OpSelect %v4f32 %v4cond %c_v4f32_1_0_0_1 %param1\n"
		"                 OpReturnValue %retval\n"
		"OpFunctionEnd\n"
	);

	const char*		specDecorations		= "OpDecorate %test_constant SpecId 0\n";
	const char*		specConstants		=
			"%test_constant = OpSpecConstant %f32 0.\n"
			"%c             = OpSpecConstantOp %f32 QuantizeToF16 %test_constant\n";

	StringTemplate	specConstantFunction(
		"%test_code     = OpFunction %v4f32 None %v4f32_function\n"
		"%param1        = OpFunctionParameter %v4f32\n"
		"%label_testfun = OpLabel\n"
		"${condition}\n"
		"%v4cond        = OpCompositeConstruct %v4bool %cond %cond %cond %cond\n"
		"%retval        = OpSelect %v4f32 %v4cond %c_v4f32_1_0_0_1 %param1\n"
		"                 OpReturnValue %retval\n"
		"OpFunctionEnd\n"
	);

	for (size_t idx = 0; idx < (sizeof(tests)/sizeof(tests[0])); ++idx)
	{
		map<string, string>								codeSpecialization;
		map<string, string>								fragments;
		codeSpecialization["condition"]					= tests[idx].condition;
		fragments["testfun"]							= function.specialize(codeSpecialization);
		fragments["pre_main"]							= string(constants) + tests[idx].constant + "\n";
		createTestsForAllStages(tests[idx].name, inputColors, expectedColors, fragments, testCtx);
	}

	for (size_t idx = 0; idx < (sizeof(tests)/sizeof(tests[0])); ++idx)
	{
		map<string, string>								codeSpecialization;
		map<string, string>								fragments;
		vector<deInt32>									passConstants;
		deInt32											specConstant;

		codeSpecialization["condition"]					= tests[idx].condition;
		fragments["testfun"]							= specConstantFunction.specialize(codeSpecialization);
		fragments["decoration"]							= specDecorations;
		fragments["pre_main"]							= specConstants;

		memcpy(&specConstant, &tests[idx].valueAsFloat, sizeof(float));
		passConstants.push_back(specConstant);

		createTestsForAllStages(string("spec_const_") + tests[idx].name, inputColors, expectedColors, fragments, passConstants, testCtx);
	}
}

void createOpQuantizeTwoPossibilityTests(tcu::TestCaseGroup* testCtx)
{
	RGBA inputColors[4] =  {
		RGBA(0,		0,		0,		255),
		RGBA(0,		0,		255,	255),
		RGBA(0,		255,	0,		255),
		RGBA(0,		255,	255,	255)
	};

	RGBA expectedColors[4] =
	{
		RGBA(255,	 0,		 0,		 255),
		RGBA(255,	 0,		 0,		 255),
		RGBA(255,	 0,		 0,		 255),
		RGBA(255,	 0,		 0,		 255)
	};

	struct DualFP16Possibility
	{
		const char* name;
		const char* input;
		float		inputAsFloat;
		const char* possibleOutput1;
		const char* possibleOutput2;
	} tests[] = {
		{
			"positive_round_up_or_round_down",
			"0x1.3003p8",
			constructNormalizedFloat(8, 0x300300),
			"0x1.304p8",
			"0x1.3p8"
		},
		{
			"negative_round_up_or_round_down",
			"-0x1.6008p-7",
			-constructNormalizedFloat(-7, 0x600800),
			"-0x1.6p-7",
			"-0x1.604p-7"
		},
		{
			"carry_bit",
			"0x1.01ep2",
			constructNormalizedFloat(2, 0x01e000),
			"0x1.01cp2",
			"0x1.02p2"
		},
		{
			"carry_to_exponent",
			"0x1.ffep1",
			constructNormalizedFloat(1, 0xffe000),
			"0x1.ffcp1",
			"0x1.0p2"
		},
	};
	StringTemplate constants (
		"%input_const = OpConstant %f32 ${input}\n"
		"%possible_solution1 = OpConstant %f32 ${output1}\n"
		"%possible_solution2 = OpConstant %f32 ${output2}\n"
		);

	StringTemplate specConstants (
		"%input_const = OpSpecConstant %f32 0.\n"
		"%possible_solution1 = OpConstant %f32 ${output1}\n"
		"%possible_solution2 = OpConstant %f32 ${output2}\n"
	);

	const char* specDecorations = "OpDecorate %input_const  SpecId 0\n";

	const char* function  =
		"%test_code     = OpFunction %v4f32 None %v4f32_function\n"
		"%param1        = OpFunctionParameter %v4f32\n"
		"%label_testfun = OpLabel\n"
		"%a             = OpVectorExtractDynamic %f32 %param1 %c_i32_0\n"
		// For the purposes of this test we assume that 0.f will always get
		// faithfully passed through the pipeline stages.
		"%b             = OpFAdd %f32 %input_const %a\n"
		"%c             = OpQuantizeToF16 %f32 %b\n"
		"%eq_1          = OpFOrdEqual %bool %c %possible_solution1\n"
		"%eq_2          = OpFOrdEqual %bool %c %possible_solution2\n"
		"%cond          = OpLogicalOr %bool %eq_1 %eq_2\n"
		"%v4cond        = OpCompositeConstruct %v4bool %cond %cond %cond %cond\n"
		"%retval        = OpSelect %v4f32 %v4cond %c_v4f32_1_0_0_1 %param1"
		"                 OpReturnValue %retval\n"
		"OpFunctionEnd\n";

	for(size_t idx = 0; idx < (sizeof(tests)/sizeof(tests[0])); ++idx) {
		map<string, string>									fragments;
		map<string, string>									constantSpecialization;

		constantSpecialization["input"]						= tests[idx].input;
		constantSpecialization["output1"]					= tests[idx].possibleOutput1;
		constantSpecialization["output2"]					= tests[idx].possibleOutput2;
		fragments["testfun"]								= function;
		fragments["pre_main"]								= constants.specialize(constantSpecialization);
		createTestsForAllStages(tests[idx].name, inputColors, expectedColors, fragments, testCtx);
	}

	for(size_t idx = 0; idx < (sizeof(tests)/sizeof(tests[0])); ++idx) {
		map<string, string>									fragments;
		map<string, string>									constantSpecialization;
		vector<deInt32>										passConstants;
		deInt32												specConstant;

		constantSpecialization["output1"]					= tests[idx].possibleOutput1;
		constantSpecialization["output2"]					= tests[idx].possibleOutput2;
		fragments["testfun"]								= function;
		fragments["decoration"]								= specDecorations;
		fragments["pre_main"]								= specConstants.specialize(constantSpecialization);

		memcpy(&specConstant, &tests[idx].inputAsFloat, sizeof(float));
		passConstants.push_back(specConstant);

		createTestsForAllStages(string("spec_const_") + tests[idx].name, inputColors, expectedColors, fragments, passConstants, testCtx);
	}
}

tcu::TestCaseGroup* createOpQuantizeTests(tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> opQuantizeTests (new tcu::TestCaseGroup(testCtx, "opquantize", "Test OpQuantizeToF16"));
	createOpQuantizeSingleOptionTests(opQuantizeTests.get());
	createOpQuantizeTwoPossibilityTests(opQuantizeTests.get());
	return opQuantizeTests.release();
}

struct ShaderPermutation
{
	deUint8 vertexPermutation;
	deUint8 geometryPermutation;
	deUint8 tesscPermutation;
	deUint8 tessePermutation;
	deUint8 fragmentPermutation;
};

ShaderPermutation getShaderPermutation(deUint8 inputValue)
{
	ShaderPermutation	permutation =
	{
		static_cast<deUint8>(inputValue & 0x10? 1u: 0u),
		static_cast<deUint8>(inputValue & 0x08? 1u: 0u),
		static_cast<deUint8>(inputValue & 0x04? 1u: 0u),
		static_cast<deUint8>(inputValue & 0x02? 1u: 0u),
		static_cast<deUint8>(inputValue & 0x01? 1u: 0u)
	};
	return permutation;
}

tcu::TestCaseGroup* createModuleTests(tcu::TestContext& testCtx)
{
	RGBA								defaultColors[4];
	RGBA								invertedColors[4];
	de::MovePtr<tcu::TestCaseGroup>		moduleTests			(new tcu::TestCaseGroup(testCtx, "module", "Multiple entry points into shaders"));

	const ShaderElement					combinedPipeline[]	=
	{
		ShaderElement("module", "main", VK_SHADER_STAGE_VERTEX_BIT),
		ShaderElement("module", "main", VK_SHADER_STAGE_GEOMETRY_BIT),
		ShaderElement("module", "main", VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT),
		ShaderElement("module", "main", VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT),
		ShaderElement("module", "main", VK_SHADER_STAGE_FRAGMENT_BIT)
	};

	getDefaultColors(defaultColors);
	getInvertedDefaultColors(invertedColors);
	addFunctionCaseWithPrograms<InstanceContext>(moduleTests.get(), "same_module", "", createCombinedModule, runAndVerifyDefaultPipeline, createInstanceContext(combinedPipeline, map<string, string>()));

	const char* numbers[] =
	{
		"1", "2"
	};

	for (deInt8 idx = 0; idx < 32; ++idx)
	{
		ShaderPermutation			permutation		= getShaderPermutation(idx);
		string						name			= string("vert") + numbers[permutation.vertexPermutation] + "_geom" + numbers[permutation.geometryPermutation] + "_tessc" + numbers[permutation.tesscPermutation] + "_tesse" + numbers[permutation.tessePermutation] + "_frag" + numbers[permutation.fragmentPermutation];
		const ShaderElement			pipeline[]		=
		{
			ShaderElement("vert",	string("vert") +	numbers[permutation.vertexPermutation],		VK_SHADER_STAGE_VERTEX_BIT),
			ShaderElement("geom",	string("geom") +	numbers[permutation.geometryPermutation],	VK_SHADER_STAGE_GEOMETRY_BIT),
			ShaderElement("tessc",	string("tessc") +	numbers[permutation.tesscPermutation],		VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT),
			ShaderElement("tesse",	string("tesse") +	numbers[permutation.tessePermutation],		VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT),
			ShaderElement("frag",	string("frag") +	numbers[permutation.fragmentPermutation],	VK_SHADER_STAGE_FRAGMENT_BIT)
		};

		// If there are an even number of swaps, then it should be no-op.
		// If there are an odd number, the color should be flipped.
		if ((permutation.vertexPermutation + permutation.geometryPermutation + permutation.tesscPermutation + permutation.tessePermutation + permutation.fragmentPermutation) % 2 == 0)
		{
			addFunctionCaseWithPrograms<InstanceContext>(moduleTests.get(), name, "", createMultipleEntries, runAndVerifyDefaultPipeline, createInstanceContext(pipeline, defaultColors, defaultColors, map<string, string>()));
		}
		else
		{
			addFunctionCaseWithPrograms<InstanceContext>(moduleTests.get(), name, "", createMultipleEntries, runAndVerifyDefaultPipeline, createInstanceContext(pipeline, defaultColors, invertedColors, map<string, string>()));
		}
	}
	return moduleTests.release();
}

tcu::TestCaseGroup* createLoopTests(tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> testGroup(new tcu::TestCaseGroup(testCtx, "loop", "Looping control flow"));
	RGBA defaultColors[4];
	getDefaultColors(defaultColors);
	map<string, string> fragments;
	fragments["pre_main"] =
		"%c_f32_5 = OpConstant %f32 5.\n";

	// A loop with a single block. The Continue Target is the loop block
	// itself. In SPIR-V terms, the "loop construct" contains no blocks at all
	// -- the "continue construct" forms the entire loop.
	fragments["testfun"] =
		"%test_code = OpFunction %v4f32 None %v4f32_function\n"
		"%param1 = OpFunctionParameter %v4f32\n"

		"%entry = OpLabel\n"
		"%val0 = OpVectorExtractDynamic %f32 %param1 %c_i32_0\n"
		"OpBranch %loop\n"

		";adds and subtracts 1.0 to %val in alternate iterations\n"
		"%loop = OpLabel\n"
		"%count = OpPhi %i32 %c_i32_4 %entry %count__ %loop\n"
		"%delta = OpPhi %f32 %c_f32_1 %entry %minus_delta %loop\n"
		"%val1 = OpPhi %f32 %val0 %entry %val %loop\n"
		"%val = OpFAdd %f32 %val1 %delta\n"
		"%minus_delta = OpFSub %f32 %c_f32_0 %delta\n"
		"%count__ = OpISub %i32 %count %c_i32_1\n"
		"%again = OpSGreaterThan %bool %count__ %c_i32_0\n"
		"OpLoopMerge %exit %loop None\n"
		"OpBranchConditional %again %loop %exit\n"

		"%exit = OpLabel\n"
		"%result = OpVectorInsertDynamic %v4f32 %param1 %val %c_i32_0\n"
		"OpReturnValue %result\n"

		"OpFunctionEnd\n"
		;
	createTestsForAllStages("single_block", defaultColors, defaultColors, fragments, testGroup.get());

	// Body comprised of multiple basic blocks.
	const StringTemplate multiBlock(
		"%test_code = OpFunction %v4f32 None %v4f32_function\n"
		"%param1 = OpFunctionParameter %v4f32\n"

		"%entry = OpLabel\n"
		"%val0 = OpVectorExtractDynamic %f32 %param1 %c_i32_0\n"
		"OpBranch %loop\n"

		";adds and subtracts 1.0 to %val in alternate iterations\n"
		"%loop = OpLabel\n"
		"%count = OpPhi %i32 %c_i32_4 %entry %count__ %gather\n"
		"%delta = OpPhi %f32 %c_f32_1 %entry %delta_next %gather\n"
		"%val1 = OpPhi %f32 %val0 %entry %val %gather\n"
		// There are several possibilities for the Continue Target below.  Each
		// will be specialized into a separate test case.
		"OpLoopMerge %exit ${continue_target} None\n"
		"OpBranch %if\n"

		"%if = OpLabel\n"
		";delta_next = (delta > 0) ? -1 : 1;\n"
		"%gt0 = OpFOrdGreaterThan %bool %delta %c_f32_0\n"
		"OpSelectionMerge %gather DontFlatten\n"
		"OpBranchConditional %gt0 %even %odd ;tells us if %count is even or odd\n"

		"%odd = OpLabel\n"
		"OpBranch %gather\n"

		"%even = OpLabel\n"
		"OpBranch %gather\n"

		"%gather = OpLabel\n"
		"%delta_next = OpPhi %f32 %c_f32_n1 %even %c_f32_1 %odd\n"
		"%val = OpFAdd %f32 %val1 %delta\n"
		"%count__ = OpISub %i32 %count %c_i32_1\n"
		"%again = OpSGreaterThan %bool %count__ %c_i32_0\n"
		"OpBranchConditional %again %loop %exit\n"

		"%exit = OpLabel\n"
		"%result = OpVectorInsertDynamic %v4f32 %param1 %val %c_i32_0\n"
		"OpReturnValue %result\n"

		"OpFunctionEnd\n");

	map<string, string> continue_target;

	// The Continue Target is the loop block itself.
	continue_target["continue_target"] = "%loop";
	fragments["testfun"] = multiBlock.specialize(continue_target);
	createTestsForAllStages("multi_block_continue_construct", defaultColors, defaultColors, fragments, testGroup.get());

	// The Continue Target is at the end of the loop.
	continue_target["continue_target"] = "%gather";
	fragments["testfun"] = multiBlock.specialize(continue_target);
	createTestsForAllStages("multi_block_loop_construct", defaultColors, defaultColors, fragments, testGroup.get());

	// A loop with continue statement.
	fragments["testfun"] =
		"%test_code = OpFunction %v4f32 None %v4f32_function\n"
		"%param1 = OpFunctionParameter %v4f32\n"

		"%entry = OpLabel\n"
		"%val0 = OpVectorExtractDynamic %f32 %param1 %c_i32_0\n"
		"OpBranch %loop\n"

		";adds 4, 3, and 1 to %val0 (skips 2)\n"
		"%loop = OpLabel\n"
		"%count = OpPhi %i32 %c_i32_4 %entry %count__ %continue\n"
		"%val1 = OpPhi %f32 %val0 %entry %val %continue\n"
		"OpLoopMerge %exit %continue None\n"
		"OpBranch %if\n"

		"%if = OpLabel\n"
		";skip if %count==2\n"
		"%eq2 = OpIEqual %bool %count %c_i32_2\n"
		"OpSelectionMerge %continue DontFlatten\n"
		"OpBranchConditional %eq2 %continue %body\n"

		"%body = OpLabel\n"
		"%fcount = OpConvertSToF %f32 %count\n"
		"%val2 = OpFAdd %f32 %val1 %fcount\n"
		"OpBranch %continue\n"

		"%continue = OpLabel\n"
		"%val = OpPhi %f32 %val2 %body %val1 %if\n"
		"%count__ = OpISub %i32 %count %c_i32_1\n"
		"%again = OpSGreaterThan %bool %count__ %c_i32_0\n"
		"OpBranchConditional %again %loop %exit\n"

		"%exit = OpLabel\n"
		"%same = OpFSub %f32 %val %c_f32_8\n"
		"%result = OpVectorInsertDynamic %v4f32 %param1 %same %c_i32_0\n"
		"OpReturnValue %result\n"
		"OpFunctionEnd\n";
	createTestsForAllStages("continue", defaultColors, defaultColors, fragments, testGroup.get());

	// A loop with break.
	fragments["testfun"] =
		"%test_code = OpFunction %v4f32 None %v4f32_function\n"
		"%param1 = OpFunctionParameter %v4f32\n"

		"%entry = OpLabel\n"
		";param1 components are between 0 and 1, so dot product is 4 or less\n"
		"%dot = OpDot %f32 %param1 %param1\n"
		"%div = OpFDiv %f32 %dot %c_f32_5\n"
		"%zero = OpConvertFToU %u32 %div\n"
		"%two = OpIAdd %i32 %zero %c_i32_2\n"
		"%val0 = OpVectorExtractDynamic %f32 %param1 %c_i32_0\n"
		"OpBranch %loop\n"

		";adds 4 and 3 to %val0 (exits early)\n"
		"%loop = OpLabel\n"
		"%count = OpPhi %i32 %c_i32_4 %entry %count__ %continue\n"
		"%val1 = OpPhi %f32 %val0 %entry %val2 %continue\n"
		"OpLoopMerge %exit %continue None\n"
		"OpBranch %if\n"

		"%if = OpLabel\n"
		";end loop if %count==%two\n"
		"%above2 = OpSGreaterThan %bool %count %two\n"
		"OpSelectionMerge %continue DontFlatten\n"
		"OpBranchConditional %above2 %body %exit\n"

		"%body = OpLabel\n"
		"%fcount = OpConvertSToF %f32 %count\n"
		"%val2 = OpFAdd %f32 %val1 %fcount\n"
		"OpBranch %continue\n"

		"%continue = OpLabel\n"
		"%count__ = OpISub %i32 %count %c_i32_1\n"
		"%again = OpSGreaterThan %bool %count__ %c_i32_0\n"
		"OpBranchConditional %again %loop %exit\n"

		"%exit = OpLabel\n"
		"%val_post = OpPhi %f32 %val2 %continue %val1 %if\n"
		"%same = OpFSub %f32 %val_post %c_f32_7\n"
		"%result = OpVectorInsertDynamic %v4f32 %param1 %same %c_i32_0\n"
		"OpReturnValue %result\n"
		"OpFunctionEnd\n";
	createTestsForAllStages("break", defaultColors, defaultColors, fragments, testGroup.get());

	// A loop with return.
	fragments["testfun"] =
		"%test_code = OpFunction %v4f32 None %v4f32_function\n"
		"%param1 = OpFunctionParameter %v4f32\n"

		"%entry = OpLabel\n"
		";param1 components are between 0 and 1, so dot product is 4 or less\n"
		"%dot = OpDot %f32 %param1 %param1\n"
		"%div = OpFDiv %f32 %dot %c_f32_5\n"
		"%zero = OpConvertFToU %u32 %div\n"
		"%two = OpIAdd %i32 %zero %c_i32_2\n"
		"%val0 = OpVectorExtractDynamic %f32 %param1 %c_i32_0\n"
		"OpBranch %loop\n"

		";returns early without modifying %param1\n"
		"%loop = OpLabel\n"
		"%count = OpPhi %i32 %c_i32_4 %entry %count__ %continue\n"
		"%val1 = OpPhi %f32 %val0 %entry %val2 %continue\n"
		"OpLoopMerge %exit %continue None\n"
		"OpBranch %if\n"

		"%if = OpLabel\n"
		";return if %count==%two\n"
		"%above2 = OpSGreaterThan %bool %count %two\n"
		"OpSelectionMerge %continue DontFlatten\n"
		"OpBranchConditional %above2 %body %early_exit\n"

		"%early_exit = OpLabel\n"
		"OpReturnValue %param1\n"

		"%body = OpLabel\n"
		"%fcount = OpConvertSToF %f32 %count\n"
		"%val2 = OpFAdd %f32 %val1 %fcount\n"
		"OpBranch %continue\n"

		"%continue = OpLabel\n"
		"%count__ = OpISub %i32 %count %c_i32_1\n"
		"%again = OpSGreaterThan %bool %count__ %c_i32_0\n"
		"OpBranchConditional %again %loop %exit\n"

		"%exit = OpLabel\n"
		";should never get here, so return an incorrect result\n"
		"%result = OpVectorInsertDynamic %v4f32 %param1 %val2 %c_i32_0\n"
		"OpReturnValue %result\n"
		"OpFunctionEnd\n";
	createTestsForAllStages("return", defaultColors, defaultColors, fragments, testGroup.get());

	return testGroup.release();
}

// Adds a new test to group using custom fragments for the tessellation-control
// stage and passthrough fragments for all other stages.  Uses default colors
// for input and expected output.
void addTessCtrlTest(tcu::TestCaseGroup* group, const char* name, const map<string, string>& fragments)
{
	RGBA defaultColors[4];
	getDefaultColors(defaultColors);
	const ShaderElement pipelineStages[] =
	{
		ShaderElement("vert", "main", VK_SHADER_STAGE_VERTEX_BIT),
		ShaderElement("tessc", "main", VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT),
		ShaderElement("tesse", "main", VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT),
		ShaderElement("frag", "main", VK_SHADER_STAGE_FRAGMENT_BIT),
	};

	addFunctionCaseWithPrograms<InstanceContext>(group, name, "", addShaderCodeCustomTessControl,
												 runAndVerifyDefaultPipeline, createInstanceContext(
													 pipelineStages, defaultColors, defaultColors, fragments, StageToSpecConstantMap()));
}

// A collection of tests putting OpControlBarrier in places GLSL forbids but SPIR-V allows.
tcu::TestCaseGroup* createBarrierTests(tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> testGroup(new tcu::TestCaseGroup(testCtx, "barrier", "OpControlBarrier"));
	map<string, string> fragments;

	// A barrier inside a function body.
	fragments["pre_main"] =
		"%Workgroup = OpConstant %i32 2\n"
		"%SequentiallyConsistent = OpConstant %i32 0x10\n";
	fragments["testfun"] =
		"%test_code = OpFunction %v4f32 None %v4f32_function\n"
		"%param1 = OpFunctionParameter %v4f32\n"
		"%label_testfun = OpLabel\n"
		"OpControlBarrier %Workgroup %Workgroup %SequentiallyConsistent\n"
		"OpReturnValue %param1\n"
		"OpFunctionEnd\n";
	addTessCtrlTest(testGroup.get(), "in_function", fragments);

	// Common setup code for the following tests.
	fragments["pre_main"] =
		"%Workgroup = OpConstant %i32 2\n"
		"%SequentiallyConsistent = OpConstant %i32 0x10\n"
		"%c_f32_5 = OpConstant %f32 5.\n";
	const string setupPercentZero =	 // Begins %test_code function with code that sets %zero to 0u but cannot be optimized away.
		"%test_code = OpFunction %v4f32 None %v4f32_function\n"
		"%param1 = OpFunctionParameter %v4f32\n"
		"%entry = OpLabel\n"
		";param1 components are between 0 and 1, so dot product is 4 or less\n"
		"%dot = OpDot %f32 %param1 %param1\n"
		"%div = OpFDiv %f32 %dot %c_f32_5\n"
		"%zero = OpConvertFToU %u32 %div\n";

	// Barriers inside OpSwitch branches.
	fragments["testfun"] =
		setupPercentZero +
		"OpSelectionMerge %switch_exit None\n"
		"OpSwitch %zero %switch_default 0 %case0 1 %case1 ;should always go to %case0\n"

		"%case1 = OpLabel\n"
		";This barrier should never be executed, but its presence makes test failure more likely when there's a bug.\n"
		"OpControlBarrier %Workgroup %Workgroup %SequentiallyConsistent\n"
		"%wrong_branch_alert1 = OpVectorInsertDynamic %v4f32 %param1 %c_f32_0_5 %c_i32_0\n"
		"OpBranch %switch_exit\n"

		"%switch_default = OpLabel\n"
		"%wrong_branch_alert2 = OpVectorInsertDynamic %v4f32 %param1 %c_f32_0_5 %c_i32_0\n"
		";This barrier should never be executed, but its presence makes test failure more likely when there's a bug.\n"
		"OpControlBarrier %Workgroup %Workgroup %SequentiallyConsistent\n"
		"OpBranch %switch_exit\n"

		"%case0 = OpLabel\n"
		"OpControlBarrier %Workgroup %Workgroup %SequentiallyConsistent\n"
		"OpBranch %switch_exit\n"

		"%switch_exit = OpLabel\n"
		"%ret = OpPhi %v4f32 %param1 %case0 %wrong_branch_alert1 %case1 %wrong_branch_alert2 %switch_default\n"
		"OpReturnValue %ret\n"
		"OpFunctionEnd\n";
	addTessCtrlTest(testGroup.get(), "in_switch", fragments);

	// Barriers inside if-then-else.
	fragments["testfun"] =
		setupPercentZero +
		"%eq0 = OpIEqual %bool %zero %c_u32_0\n"
		"OpSelectionMerge %exit DontFlatten\n"
		"OpBranchConditional %eq0 %then %else\n"

		"%else = OpLabel\n"
		";This barrier should never be executed, but its presence makes test failure more likely when there's a bug.\n"
		"OpControlBarrier %Workgroup %Workgroup %SequentiallyConsistent\n"
		"%wrong_branch_alert = OpVectorInsertDynamic %v4f32 %param1 %c_f32_0_5 %c_i32_0\n"
		"OpBranch %exit\n"

		"%then = OpLabel\n"
		"OpControlBarrier %Workgroup %Workgroup %SequentiallyConsistent\n"
		"OpBranch %exit\n"

		"%exit = OpLabel\n"
		"%ret = OpPhi %v4f32 %param1 %then %wrong_branch_alert %else\n"
		"OpReturnValue %ret\n"
		"OpFunctionEnd\n";
	addTessCtrlTest(testGroup.get(), "in_if", fragments);

	// A barrier after control-flow reconvergence, tempting the compiler to attempt something like this:
	// http://lists.llvm.org/pipermail/llvm-dev/2009-October/026317.html.
	fragments["testfun"] =
		setupPercentZero +
		"%thread_id = OpLoad %i32 %BP_gl_InvocationID\n"
		"%thread0 = OpIEqual %bool %thread_id %c_i32_0\n"
		"OpSelectionMerge %exit DontFlatten\n"
		"OpBranchConditional %thread0 %then %else\n"

		"%else = OpLabel\n"
		"%val0 = OpVectorExtractDynamic %f32 %param1 %c_i32_0\n"
		"OpBranch %exit\n"

		"%then = OpLabel\n"
		"%val1 = OpVectorExtractDynamic %f32 %param1 %zero\n"
		"OpBranch %exit\n"

		"%exit = OpLabel\n"
		"%val = OpPhi %f32 %val0 %else %val1 %then\n"
		"OpControlBarrier %Workgroup %Workgroup %SequentiallyConsistent\n"
		"%ret = OpVectorInsertDynamic %v4f32 %param1 %val %zero\n"
		"OpReturnValue %ret\n"
		"OpFunctionEnd\n";
	addTessCtrlTest(testGroup.get(), "after_divergent_if", fragments);

	// A barrier inside a loop.
	fragments["pre_main"] =
		"%Workgroup = OpConstant %i32 2\n"
		"%SequentiallyConsistent = OpConstant %i32 0x10\n"
		"%c_f32_10 = OpConstant %f32 10.\n";
	fragments["testfun"] =
		"%test_code = OpFunction %v4f32 None %v4f32_function\n"
		"%param1 = OpFunctionParameter %v4f32\n"
		"%entry = OpLabel\n"
		"%val0 = OpVectorExtractDynamic %f32 %param1 %c_i32_0\n"
		"OpBranch %loop\n"

		";adds 4, 3, 2, and 1 to %val0\n"
		"%loop = OpLabel\n"
		"%count = OpPhi %i32 %c_i32_4 %entry %count__ %loop\n"
		"%val1 = OpPhi %f32 %val0 %entry %val %loop\n"
		"OpControlBarrier %Workgroup %Workgroup %SequentiallyConsistent\n"
		"%fcount = OpConvertSToF %f32 %count\n"
		"%val = OpFAdd %f32 %val1 %fcount\n"
		"%count__ = OpISub %i32 %count %c_i32_1\n"
		"%again = OpSGreaterThan %bool %count__ %c_i32_0\n"
		"OpLoopMerge %exit %loop None\n"
		"OpBranchConditional %again %loop %exit\n"

		"%exit = OpLabel\n"
		"%same = OpFSub %f32 %val %c_f32_10\n"
		"%ret = OpVectorInsertDynamic %v4f32 %param1 %same %c_i32_0\n"
		"OpReturnValue %ret\n"
		"OpFunctionEnd\n";
	addTessCtrlTest(testGroup.get(), "in_loop", fragments);

	return testGroup.release();
}

// Test for the OpFRem instruction.
tcu::TestCaseGroup* createFRemTests(tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup>		testGroup(new tcu::TestCaseGroup(testCtx, "frem", "OpFRem"));
	map<string, string>					fragments;
	RGBA								inputColors[4];
	RGBA								outputColors[4];

	fragments["pre_main"]				 =
		"%c_f32_3 = OpConstant %f32 3.0\n"
		"%c_f32_n3 = OpConstant %f32 -3.0\n"
		"%c_f32_4 = OpConstant %f32 4.0\n"
		"%c_f32_p75 = OpConstant %f32 0.75\n"
		"%c_v4f32_p75_p75_p75_p75 = OpConstantComposite %v4f32 %c_f32_p75 %c_f32_p75 %c_f32_p75 %c_f32_p75 \n"
		"%c_v4f32_4_4_4_4 = OpConstantComposite %v4f32 %c_f32_4 %c_f32_4 %c_f32_4 %c_f32_4\n"
		"%c_v4f32_3_n3_3_n3 = OpConstantComposite %v4f32 %c_f32_3 %c_f32_n3 %c_f32_3 %c_f32_n3\n";

	// The test does the following.
	// vec4 result = (param1 * 8.0) - 4.0;
	// return (frem(result.x,3) + 0.75, frem(result.y, -3) + 0.75, 0, 1)
	fragments["testfun"]				 =
		"%test_code = OpFunction %v4f32 None %v4f32_function\n"
		"%param1 = OpFunctionParameter %v4f32\n"
		"%label_testfun = OpLabel\n"
		"%v_times_8 = OpVectorTimesScalar %v4f32 %param1 %c_f32_8\n"
		"%minus_4 = OpFSub %v4f32 %v_times_8 %c_v4f32_4_4_4_4\n"
		"%frem = OpFRem %v4f32 %minus_4 %c_v4f32_3_n3_3_n3\n"
		"%added = OpFAdd %v4f32 %frem %c_v4f32_p75_p75_p75_p75\n"
		"%xyz_1 = OpVectorInsertDynamic %v4f32 %added %c_f32_1 %c_i32_3\n"
		"%xy_0_1 = OpVectorInsertDynamic %v4f32 %xyz_1 %c_f32_0 %c_i32_2\n"
		"OpReturnValue %xy_0_1\n"
		"OpFunctionEnd\n";


	inputColors[0]		= RGBA(16,	16,		0, 255);
	inputColors[1]		= RGBA(232, 232,	0, 255);
	inputColors[2]		= RGBA(232, 16,		0, 255);
	inputColors[3]		= RGBA(16,	232,	0, 255);

	outputColors[0]		= RGBA(64,	64,		0, 255);
	outputColors[1]		= RGBA(255, 255,	0, 255);
	outputColors[2]		= RGBA(255, 64,		0, 255);
	outputColors[3]		= RGBA(64,	255,	0, 255);

	createTestsForAllStages("frem", inputColors, outputColors, fragments, testGroup.get());
	return testGroup.release();
}

tcu::TestCaseGroup* createInstructionTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> instructionTests	(new tcu::TestCaseGroup(testCtx, "instruction", "Instructions with special opcodes/operands"));
	de::MovePtr<tcu::TestCaseGroup> computeTests		(new tcu::TestCaseGroup(testCtx, "compute", "Compute Instructions with special opcodes/operands"));
	de::MovePtr<tcu::TestCaseGroup> graphicsTests		(new tcu::TestCaseGroup(testCtx, "graphics", "Graphics Instructions with special opcodes/operands"));

	computeTests->addChild(createOpNopGroup(testCtx));
	computeTests->addChild(createOpLineGroup(testCtx));
	computeTests->addChild(createOpNoLineGroup(testCtx));
	computeTests->addChild(createOpConstantNullGroup(testCtx));
	computeTests->addChild(createOpConstantCompositeGroup(testCtx));
	computeTests->addChild(createOpConstantUsageGroup(testCtx));
	computeTests->addChild(createSpecConstantGroup(testCtx));
	computeTests->addChild(createOpSourceGroup(testCtx));
	computeTests->addChild(createOpSourceExtensionGroup(testCtx));
	computeTests->addChild(createDecorationGroupGroup(testCtx));
	computeTests->addChild(createOpPhiGroup(testCtx));
	computeTests->addChild(createLoopControlGroup(testCtx));
	computeTests->addChild(createFunctionControlGroup(testCtx));
	computeTests->addChild(createSelectionControlGroup(testCtx));
	computeTests->addChild(createBlockOrderGroup(testCtx));
	computeTests->addChild(createMultipleShaderGroup(testCtx));
	computeTests->addChild(createMemoryAccessGroup(testCtx));
	computeTests->addChild(createOpCopyMemoryGroup(testCtx));
	computeTests->addChild(createOpCopyObjectGroup(testCtx));
	computeTests->addChild(createNoContractionGroup(testCtx));
	computeTests->addChild(createOpUndefGroup(testCtx));
	computeTests->addChild(createOpUnreachableGroup(testCtx));
	computeTests ->addChild(createOpQuantizeToF16Group(testCtx));
	computeTests ->addChild(createOpFRemGroup(testCtx));

	RGBA defaultColors[4];
	getDefaultColors(defaultColors);

	de::MovePtr<tcu::TestCaseGroup> opnopTests (new tcu::TestCaseGroup(testCtx, "opnop", "Test OpNop"));
	map<string, string> opNopFragments;
	opNopFragments["testfun"] =
		"%test_code = OpFunction %v4f32 None %v4f32_function\n"
		"%param1 = OpFunctionParameter %v4f32\n"
		"%label_testfun = OpLabel\n"
		"OpNop\n"
		"OpNop\n"
		"OpNop\n"
		"OpNop\n"
		"OpNop\n"
		"OpNop\n"
		"OpNop\n"
		"OpNop\n"
		"%a = OpVectorExtractDynamic %f32 %param1 %c_i32_0\n"
		"%b = OpFAdd %f32 %a %a\n"
		"OpNop\n"
		"%c = OpFSub %f32 %b %a\n"
		"%ret = OpVectorInsertDynamic %v4f32 %param1 %c %c_i32_0\n"
		"OpNop\n"
		"OpNop\n"
		"OpReturnValue %ret\n"
		"OpFunctionEnd\n"
		;
	createTestsForAllStages("opnop", defaultColors, defaultColors, opNopFragments, opnopTests.get());


	graphicsTests->addChild(opnopTests.release());
	graphicsTests->addChild(createOpSourceTests(testCtx));
	graphicsTests->addChild(createOpSourceContinuedTests(testCtx));
	graphicsTests->addChild(createOpLineTests(testCtx));
	graphicsTests->addChild(createOpNoLineTests(testCtx));
	graphicsTests->addChild(createOpConstantNullTests(testCtx));
	graphicsTests->addChild(createOpConstantCompositeTests(testCtx));
	graphicsTests->addChild(createMemoryAccessTests(testCtx));
	graphicsTests->addChild(createOpUndefTests(testCtx));
	graphicsTests->addChild(createSelectionBlockOrderTests(testCtx));
	graphicsTests->addChild(createModuleTests(testCtx));
	graphicsTests->addChild(createSwitchBlockOrderTests(testCtx));
	graphicsTests->addChild(createOpPhiTests(testCtx));
	graphicsTests->addChild(createNoContractionTests(testCtx));
	graphicsTests->addChild(createOpQuantizeTests(testCtx));
	graphicsTests->addChild(createLoopTests(testCtx));
	graphicsTests->addChild(createSpecConstantTests(testCtx));
	graphicsTests->addChild(createSpecConstantOpQuantizeToF16Group(testCtx));
	graphicsTests->addChild(createBarrierTests(testCtx));
	graphicsTests->addChild(createDecorationGroupTests(testCtx));
	graphicsTests->addChild(createFRemTests(testCtx));

	instructionTests->addChild(computeTests.release());
	instructionTests->addChild(graphicsTests.release());

	return instructionTests.release();
}

} // SpirVAssembly
} // vkt
