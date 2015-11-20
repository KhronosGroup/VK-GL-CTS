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
	"%uvec3ptr  = OpTypePointer Input %uvec3\n"
	"%f32ptr    = OpTypePointer Uniform %f32\n"
	"%f32arr    = OpTypeRuntimeArray %f32\n";

// Declares two uniform variables (indata, outdata) of type "struct { float[] }". Depends on type "f32arr" (for "float[]").
static const char* const s_InputOutputBuffer =
	"%inbuf     = OpTypeStruct %f32arr\n"
	"%inbufptr  = OpTypePointer Uniform %inbuf\n"
	"%indata    = OpVariable %inbufptr Uniform\n"
	"%outbuf    = OpTypeStruct %f32arr\n"
	"%outbufptr = OpTypePointer Uniform %outbuf\n"
	"%outdata   = OpVariable %outbufptr Uniform\n";

// Declares buffer type and layout for uniform variables indata and outdata. Both of them are SSBO bounded to descriptor set 0.
// indata is at binding point 0, while outdata is at 1.
static const char* const s_InputOutputBufferTraits =
		"OpDecorate %inbuf BufferBlock\n"
		"OpDecorate %indata DescriptorSet 0\n"
		"OpDecorate %indata Binding 0\n"
		"OpDecorate %outbuf BufferBlock\n"
		"OpDecorate %outdata DescriptorSet 0\n"
		"OpDecorate %outdata Binding 1\n"
		"OpDecorate %f32arr ArrayStride 4\n"
		"OpMemberDecorate %inbuf 0 Offset 0\n"
	"OpMemberDecorate %outbuf 0 Offset 0\n";

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
	spec.entryPoint = "main";

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
	spec.entryPoint = "main";

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
	spec.entryPoint = "main";

	group->addChild(new SpvAsmComputeShaderCase(testCtx, "all", "OpNoLine appearing at different places", spec));

	return group.release();
}

tcu::TestCaseGroup* createNoContractionGroup (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup>	group			(new tcu::TestCaseGroup(testCtx, "nocontraction", "Test the NoContraction decoration"));
	vector<CaseParameter>			cases;
	de::Random						rnd				(deStringHash(group->getName()));
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

		"OpDecorate %inbuf1 BufferBlock\n"
		"OpDecorate %indata1 DescriptorSet 0\n"
		"OpDecorate %indata1 Binding 0\n"
		"OpDecorate %inbuf2 BufferBlock\n"
		"OpDecorate %indata2 DescriptorSet 0\n"
		"OpDecorate %indata2 Binding 1\n"
		"OpDecorate %outbuf BufferBlock\n"
		"OpDecorate %outdata DescriptorSet 0\n"
		"OpDecorate %outdata Binding 2\n"
		"OpDecorate %f32arr ArrayStride 4\n"
		"OpMemberDecorate %inbuf1 0 Offset 0\n"
		"OpMemberDecorate %inbuf2 0 Offset 0\n"
		"OpMemberDecorate %outbuf 0 Offset 0\n"

		+ string(s_CommonTypes) +

		"%inbuf1     = OpTypeStruct %f32arr\n"
		"%inbufptr1  = OpTypePointer Uniform %inbuf1\n"
		"%indata1    = OpVariable %inbufptr1 Uniform\n"
		"%inbuf2     = OpTypeStruct %f32arr\n"
		"%inbufptr2  = OpTypePointer Uniform %inbuf2\n"
		"%indata2    = OpVariable %inbufptr2 Uniform\n"
		"%outbuf     = OpTypeStruct %f32arr\n"
		"%outbufptr  = OpTypePointer Uniform %outbuf\n"
		"%outdata    = OpVariable %outbufptr Uniform\n"

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
		// conducted separately and the result is rounded to 1. So the final result will be 0.f.
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
		spec.entryPoint = "main";

		group->addChild(new SpvAsmComputeShaderCase(testCtx, cases[caseNdx].name, cases[caseNdx].name, spec));
	}
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

	for (size_t ndx = 0; ndx < numElements; ++ndx)
		outputFloats1[ndx] = inputFloats1[ndx] + Vec4(0.f, 0.5f, 1.5f, 2.5f);

	spec1.assembly =
		string(s_ShaderPreamble) +

		"OpName %main           \"main\"\n"
		"OpName %id             \"gl_GlobalInvocationID\"\n"

		"OpDecorate %id BuiltIn GlobalInvocationId\n"

		+ string(s_InputOutputBufferTraits) + string(s_CommonTypes) +

		"%vec4       = OpTypeVector %f32 4\n"
		"%vec4ptr_u  = OpTypePointer Uniform %vec4\n"
		"%vec4ptr_f  = OpTypePointer Function %vec4\n"
		"%vec4arr    = OpTypeRuntimeArray %vec4\n"
		"%inbuf      = OpTypeStruct %vec4arr\n"
		"%inbufptr   = OpTypePointer Uniform %inbuf\n"
		"%indata     = OpVariable %inbufptr Uniform\n"
		"%outbuf     = OpTypeStruct %vec4arr\n"
		"%outbufptr  = OpTypePointer Uniform %outbuf\n"
		"%outdata    = OpVariable %outbufptr Uniform\n"

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
	spec1.entryPoint = "main";

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

		+ string(s_InputOutputBufferTraits) + string(s_CommonTypes) +

		"%hundred        = OpConstant %u32 100\n"
		"%f32arr100      = OpTypeArray %f32 %hundred\n"
		"%f32arr100ptr_f = OpTypePointer Function %f32arr100\n"
		"%f32arr100ptr_u = OpTypePointer Uniform %f32arr100\n"
		"%inbuf          = OpTypeStruct %f32arr100\n"
		"%inbufptr       = OpTypePointer Uniform %inbuf\n"
		"%indata         = OpVariable %inbufptr Uniform\n"
		"%outbuf         = OpTypeStruct %f32arr100\n"
		"%outbufptr      = OpTypePointer Uniform %outbuf\n"
		"%outdata        = OpVariable %outbufptr Uniform\n"

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
	spec2.entryPoint = "main";

	group->addChild(new SpvAsmComputeShaderCase(testCtx, "array", "OpCopyMemory elements of array type", spec2));

	// The following case copies a struct{vec4, vec4, vec4, vec4} variable from the input buffer to the output buffer.
	ComputeShaderSpec				spec3;
	vector<float>					inputFloats3	(16);
	vector<float>					outputFloats3	(16);

	fillRandomScalars(rnd, -200.f, 200.f, &inputFloats3[0], 16);

	for (size_t ndx = 0; ndx < 16; ++ndx)
		outputFloats3[ndx] = -inputFloats3[ndx];

	spec3.assembly =
		string(s_ShaderPreamble) +

		"OpName %main           \"main\"\n"
		"OpName %id             \"gl_GlobalInvocationID\"\n"

		"OpDecorate %id BuiltIn GlobalInvocationId\n"

		+ string(s_InputOutputBufferTraits) + string(s_CommonTypes) +

		"%vec4      = OpTypeVector %f32 4\n"
		"%inbuf     = OpTypeStruct %vec4 %vec4 %vec4 %vec4\n"
		"%inbufptr  = OpTypePointer Uniform %inbuf\n"
		"%indata    = OpVariable %inbufptr Uniform\n"
		"%outbuf    = OpTypeStruct %vec4 %vec4 %vec4 %vec4\n"
		"%outbufptr = OpTypePointer Uniform %outbuf\n"
		"%outdata   = OpVariable %outbufptr Uniform\n"
		"%vec4stptr = OpTypePointer Function %inbuf\n"

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
	spec3.entryPoint = "main";

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
	spec4.entryPoint = "main";

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

	for (size_t ndx = 0; ndx < numElements; ++ndx)
		outputFloats[ndx] = inputFloats[ndx] + 7.5f;

	spec.assembly =
		string(s_ShaderPreamble) +

		"OpName %main           \"main\"\n"
		"OpName %id             \"gl_GlobalInvocationID\"\n"

		"OpDecorate %id BuiltIn GlobalInvocationId\n"

		+ string(s_InputOutputBufferTraits) + string(s_CommonTypes) +

		"%fvec3    = OpTypeVector %f32 3\n"
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
		"%farr_elem     = OpCompositeExtract %f32 %c_fmat_copy 2\n"
		"%fst_elem      = OpCompositeExtract %f32 %c_fmat_copy 1\n"
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
	spec.entryPoint = "main";

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
		"OpName %func_main            \"main\"\n"
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
		"%func_main   = OpFunction %void None %voidf\n"
		"%main_entry  = OpLabel\n"
		"%idval       = OpLoad %uvec3 %id\n"
		"%x           = OpCompositeExtract %u32 %idval 0\n"
		"%inloc       = OpAccessChain %f32ptr %indata %zero %x\n"
		"%inval       = OpLoad %f32 %inloc\n"
		"%outloc      = OpAccessChain %f32ptr %outdata %zero %x\n"
		"%ret_const5  = OpFunctionCall %u32 %func_const5\n"
		"%ret_modulo4 = OpFunctionCall %u32 %func_modulo4 %thousand\n"
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
	spec.entryPoint = "main";

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
	spec.entryPoint = "main";

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
	vector<deInt32>					outputInts5		(numElements, 0);
	const StringTemplate			shaderTemplate	(
		string(s_ShaderPreamble) +

		"OpName %main           \"main\"\n"
		"OpName %id             \"gl_GlobalInvocationID\"\n"

		"OpDecorate %id BuiltIn GlobalInvocationId\n"
		"OpDecorate %sc_0  SpecId 0\n"
		"OpDecorate %sc_1  SpecId 1\n"

		+ string(s_InputOutputBufferTraits) + string(s_CommonTypes) +

		"%i32ptr    = OpTypePointer Uniform %i32\n"
		"%i32arr    = OpTypeRuntimeArray %i32\n"
		"%boolptr   = OpTypePointer Uniform %bool\n"
		"%boolarr   = OpTypeRuntimeArray %bool\n"
		"%inbuf     = OpTypeStruct %i32arr\n"
		"%inbufptr  = OpTypePointer Uniform %inbuf\n"
		"%indata    = OpVariable %inbufptr Uniform\n"
		"%outbuf    = OpTypeStruct %i32arr\n"
		"%outbufptr = OpTypePointer Uniform %outbuf\n"
		"%outdata   = OpVariable %outbufptr Uniform\n"

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
		outputInts5[ndx] = inputInts[ndx] - 2;
	}

	const char addScToInput[]		= "OpIAdd %i32 %inval %sc_final";
	const char selectTrueUsingSc[]	= "OpSelect %i32 %sc_final %inval %zero";
	const char selectFalseUsingSc[]	= "OpSelect %i32 %sc_final %zero %inval";

	cases.push_back(SpecConstantTwoIntCase("iadd",					" %i32 0",		" %i32 0",		"%i32",		"IAdd                 %sc_0 %sc_1",			62,		-20,	addScToInput,		outputInts1));
	cases.push_back(SpecConstantTwoIntCase("isub",					" %i32 0",		" %i32 0",		"%i32",		"ISub                 %sc_0 %sc_1",			100,	58,		addScToInput,		outputInts1));
	cases.push_back(SpecConstantTwoIntCase("imul",					" %i32 0",		" %i32 0",		"%i32",		"IMul                 %sc_0 %sc_1",			-2,		-21,	addScToInput,		outputInts1));
	cases.push_back(SpecConstantTwoIntCase("sdiv",					" %i32 0",		" %i32 0",		"%i32",		"SDiv                 %sc_0 %sc_1",			-126,	-3,		addScToInput,		outputInts1));
	cases.push_back(SpecConstantTwoIntCase("udiv",					" %i32 0",		" %i32 0",		"%i32",		"UDiv                 %sc_0 %sc_1",			126,	3,		addScToInput,		outputInts1));
	cases.push_back(SpecConstantTwoIntCase("srem",					" %i32 0",		" %i32 0",		"%i32",		"SRem                 %sc_0 %sc_1",			7,		-3,		addScToInput,		outputInts4));
	cases.push_back(SpecConstantTwoIntCase("smod",					" %i32 0",		" %i32 0",		"%i32",		"SMod                 %sc_0 %sc_1",			7,		-3,		addScToInput,		outputInts5));
	cases.push_back(SpecConstantTwoIntCase("umod",					" %i32 0",		" %i32 0",		"%i32",		"UMod                 %sc_0 %sc_1",			342,	50,		addScToInput,		outputInts1));
	cases.push_back(SpecConstantTwoIntCase("bitwiseand",			" %i32 0",		" %i32 0",		"%i32",		"BitwiseAnd           %sc_0 %sc_1",			42,		63,		addScToInput,		outputInts1));
	cases.push_back(SpecConstantTwoIntCase("bitwiseor",				" %i32 0",		" %i32 0",		"%i32",		"BitwiseOr            %sc_0 %sc_1",			34,		8,		addScToInput,		outputInts1));
	cases.push_back(SpecConstantTwoIntCase("bitwisexor",			" %i32 0",		" %i32 0",		"%i32",		"BitwiseAnd           %sc_0 %sc_1",			18,		56,		addScToInput,		outputInts1));
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
	// \todo[2015-12-1 antiagainst] OpQuantizeToF16

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
		spec.entryPoint = "main";
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

		+ string(s_InputOutputBufferTraits) + string(s_CommonTypes) +

		"%ivec3     = OpTypeVector %i32 3\n"
		"%i32ptr    = OpTypePointer Uniform %i32\n"
		"%i32arr    = OpTypeRuntimeArray %i32\n"
		"%boolptr   = OpTypePointer Uniform %bool\n"
		"%boolarr   = OpTypeRuntimeArray %bool\n"
		"%inbuf     = OpTypeStruct %i32arr\n"
		"%inbufptr  = OpTypePointer Uniform %inbuf\n"
		"%indata    = OpVariable %inbufptr Uniform\n"
		"%outbuf    = OpTypeStruct %i32arr\n"
		"%outbufptr = OpTypePointer Uniform %outbuf\n"
		"%outdata   = OpVariable %outbufptr Uniform\n"

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
	spec.entryPoint = "main";
	spec.specConstants.push_back(123);
	spec.specConstants.push_back(56);
	spec.specConstants.push_back(-77);

	group->addChild(new SpvAsmComputeShaderCase(testCtx, "vector_related", "VectorShuffle, CompositeExtract, & CompositeInsert", spec));

	return group.release();
} tcu::TestCaseGroup* createOpPhiGroup (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup>	group			(new tcu::TestCaseGroup(testCtx, "opphi", "Test the OpPhi instruction"));
	ComputeShaderSpec				spec;
	de::Random						rnd				(deStringHash(group->getName()));
	const int						numElements		= 100;
	vector<float>					inputFloats		(numElements, 0);
	vector<float>					outputFloats	(numElements, 0);

	fillRandomScalars(rnd, -300.f, 300.f, &inputFloats[0], numElements);

	for (size_t ndx = 0; ndx < numElements; ++ndx)
	{
		switch (ndx % 3)
		{
			case 0:		outputFloats[ndx] = inputFloats[ndx] + 5.5f;	break;
			case 1:		outputFloats[ndx] = inputFloats[ndx] + 20.5f;	break;
			case 2:		outputFloats[ndx] = inputFloats[ndx] + 1.75f;	break;
			default:	break;
		}
	}

	spec.assembly =
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
	spec.inputs.push_back(BufferSp(new Float32Buffer(inputFloats)));
	spec.outputs.push_back(BufferSp(new Float32Buffer(outputFloats)));
	spec.numWorkGroups = IVec3(numElements, 1, 1);
	spec.entryPoint = "main";

	group->addChild(new SpvAsmComputeShaderCase(testCtx, "all", "OpPhi corner cases", spec));

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
	spec.entryPoint = "main";

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

	const string assembly =
		"OpCapability Shader\n"
		"OpMemoryModel Logical GLSL450\n"
		"OpEntryPoint GLCompute %comp_main1 \"entrypoint1\" %id\n"
		"OpEntryPoint GLCompute %comp_main2 \"entrypoint2\" %id\n"
		// A module cannot have two OpEntryPoint instructions with the same Execution Model and the same Name string.
		"OpEntryPoint Vertex    %vert_main  \"entrypoint2\" %vert_builtins %vertexID %instanceID\n"
		"OpExecutionMode %main LocalSize 1 1 1\n";

		"OpName %comp_main1              \"entrypoint1\"\n"
		"OpName %comp_main2              \"entrypoint2\"\n"
		"OpName %vert_main               \"entrypoint2\"\n"
		"OpName %id                      \"gl_GlobalInvocationID\"\n"
		"OpName %vert_builtin_st         \"gl_PerVertex\"\n"
		"OpName %vertexID                \"gl_VertexID\"\n"
		"OpName %instanceID              \"gl_InstanceID\"\n"
		"OpMemberName %vert_builtin_st 0 \"gl_Position\"\n"
		"OpMemberName %vert_builtin_st 1 \"gl_PointSize\"\n"
		"OpMemberName %vert_builtin_st 2 \"gl_ClipDistance\"\n"

		"OpDecorate %id                      BuiltIn GlobalInvocationId\n"
		"OpDecorate %vertexID                BuiltIn VertexId\n"
		"OpDecorate %instanceID              BuiltIn InstanceId\n"
		"OpDecorate %vert_builtin_st         Block\n"
		"OpMemberDecorate %vert_builtin_st 0 BuiltIn Position\n"
		"OpMemberDecorate %vert_builtin_st 1 BuiltIn PointSize\n"
		"OpMemberDecorate %vert_builtin_st 2 BuiltIn ClipDistance\n"

		+ string(s_InputOutputBufferTraits) + string(s_CommonTypes) + string(s_InputOutputBuffer) +

		"%i32ptr              = OpTypePointer Input %i32\n"
		"%vec4                = OpTypeVector %f32 4\n"
		"%vec4ptr             = OpTypePointer Output %vec4\n"
		"%f32arr1             = OpTypeArray %f32 %one\n"
		"%vert_builtin_st     = OpTypeStruct %vec4 %f32 %f32arr1\n"
		"%vert_builtin_st_ptr = OpTypePointer Output %vert_builtin_st\n"
		"%vert_builtins       = OpVariable %vert_builtin_st_ptr Output\n"

		"%id         = OpVariable %uvec3ptr Input\n"
		"%vertexID   = OpVariable %i32ptr Input\n"
		"%instanceID = OpVariable %i32ptr Input\n"
		"%zero       = OpConstant %i32 0\n"
		"%one        = OpConstant %u32 1\n"
		"%c_f32_1    = OpConstant %f32 1\n"
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
		"               OpFunctionEnd\n";

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
																			"OpSource GLSL 430 %fname \"" + string(65530, 'x') + "\"")); // word count: 65535
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
																			"OpSourceContinued \"" + string(65533, 'x') + "\"")); // word count: 65535
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
		spec.entryPoint = "main";

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
	cases.push_back(CaseParameter("long_extension",		string(65533, 'e'))); // word count: 65535

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
		spec.entryPoint = "main";

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
	cases.push_back(CaseParameter("matrix",			"%type = OpTypeMatrix %uvec3 3"));
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
		spec.entryPoint = "main";

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
	cases.push_back(CaseParameter("matrix",			"%m3uvec3 = OpTypeMatrix %uvec3 3\n"
													"%ten = OpConstant %u32 10\n"
													"%vec = OpConstantComposite %uvec3 %ten %zero %ten\n"
													"%mat = OpConstantComposite %m3uvec3 %vec %vec %vec"));
	cases.push_back(CaseParameter("struct",			"%m2vec3 = OpTypeMatrix %uvec3 2\n"
													"%struct = OpTypeStruct %u32 %f32 %uvec3 %m2vec3\n"
													"%one = OpConstant %u32 1\n"
													"%point5 = OpConstant %f32 0.5\n"
													"%vec = OpConstantComposite %uvec3 %one %one %zero\n"
													"%mat = OpConstantComposite %m2vec3 %vec %vec\n"
													"%const = OpConstantComposite %struct %one %point5 %vec %mat"));
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
		spec.entryPoint = "main";

		group->addChild(new SpvAsmComputeShaderCase(testCtx, cases[caseNdx].name, cases[caseNdx].name, spec));
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

		"%fvec3     = OpTypeVector %f32 3\n"
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
	spec.entryPoint = "main";

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
		"%one         = OpConstant %i32 1\n"
		"%constf1     = OpConstant %f32 1.0\n"
		"%four        = OpConstant %u32 4\n"

		"%main        = OpFunction %void None %voidf\n"
		"%entry       = OpLabel\n"
		"%i           = OpVariable %u32ptr Function\n"
		"               OpStore %i %zero\n"

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
		spec.entryPoint = "main";

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
		spec.entryPoint = "main";

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
		spec.entryPoint = "main";

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
		spec.entryPoint = "main";

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
	cases.push_back(CaseParameter("matrix",			"%type = OpTypeMatrix %uvec3 3"));
	cases.push_back(CaseParameter("image",			"%type = OpTypeImage %f32 2D 0 0 0 0 Unknown"));
	cases.push_back(CaseParameter("sampler",		"%type = OpTypeSampler"));
	cases.push_back(CaseParameter("sampledimage",	"%img = OpTypeImage %f32 2D 0 0 0 0 Unknown\n"
													"%type = OpTypeSampledImage %img"));
	cases.push_back(CaseParameter("array",			"%100 = OpConstant %u32 100\n"
													"%type = OpTypeArray %i32 %100"));
	cases.push_back(CaseParameter("runtimearray",	"%type = OpTypeRuntimeArray %f32"));
	cases.push_back(CaseParameter("struct",			"%type = OpTypeStruct %f32 %i32 %u32"));
	cases.push_back(CaseParameter("pointer",		"%type = OpTypePointer Function %i32"));
	cases.push_back(CaseParameter("function",		"%type = OpTypeFunction %void %i32 %f32"));

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
		spec.entryPoint = "main";

		group->addChild(new SpvAsmComputeShaderCase(testCtx, cases[caseNdx].name, cases[caseNdx].name, spec));
	}

		return group.release();
}

typedef std::pair<std::string, VkShaderStageFlagBits>	EntryToStage;
typedef map<string, vector<EntryToStage> >				ModuleMap;

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

	InstanceContext (const RGBA (&inputs)[4], const RGBA (&outputs)[4], const map<string, string>& testCodeFragments_)
		: testCodeFragments		(testCodeFragments_)
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
	const char*				moduleName;

	// The name of the entrypoint.
	const char*				entryName;

	// Which shader stage this entry point represents.
	VkShaderStageFlagBits	stage;

	ShaderElement (const char* moduleName_, const char* entryPoint_, VkShaderStageFlagBits shaderStage_)
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
	colors[2] = RGBA::blue();
	colors[3] = RGBA::green();
}

// Turns a statically sized array of ShaderElements into an instance-context
// by setting up the mapping of modules to their contained shaders and stages.
// The inputs and expected outputs are given by inputColors and outputColors
template<size_t N>
InstanceContext createInstanceContext (const ShaderElement (&elements)[N], const RGBA (&inputColors)[4], const RGBA (&outputColors)[4], const map<string, string>& testCodeFragments)
{
	InstanceContext ctx (inputColors, outputColors, testCodeFragments);
	for (size_t i = 0; i < N; ++i)
	{
		ctx.moduleMap[elements[i].moduleName].push_back(std::make_pair(elements[i].entryName, elements[i].stage));
	}
	return ctx;
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

#define SPIRV_ASSEMBLY_TYPES												\
	"%void = OpTypeVoid\n"													\
	"%bool = OpTypeBool\n"													\
																			\
	"%i32 = OpTypeInt 32 1\n"												\
	"%u32 = OpTypeInt 32 0\n"												\
																			\
	"%f32 = OpTypeFloat 32\n"												\
	"%v3f32 = OpTypeVector %f32 3\n"										\
	"%v4f32 = OpTypeVector %f32 4\n"										\
																			\
	"%v4f32_function = OpTypeFunction %v4f32 %v4f32\n"						\
	"%fun = OpTypeFunction %void\n"											\
																			\
	"%ip_f32 = OpTypePointer Input %f32\n"									\
	"%ip_i32 = OpTypePointer Input %i32\n"									\
	"%ip_v3f32 = OpTypePointer Input %v3f32\n"								\
	"%ip_v4f32 = OpTypePointer Input %v4f32\n"								\
																			\
	"%op_f32 = OpTypePointer Output %f32\n"									\
	"%op_v4f32 = OpTypePointer Output %v4f32\n"

#define SPIRV_ASSEMBLY_CONSTANTS											\
	"%c_f32_1 = OpConstant %f32 1\n"										\
	"%c_i32_0 = OpConstant %i32 0\n"										\
	"%c_i32_1 = OpConstant %i32 1\n"										\
	"%c_i32_2 = OpConstant %i32 2\n"										\
	"%c_u32_0 = OpConstant %u32 0\n"										\
	"%c_u32_1 = OpConstant %u32 1\n"										\
	"%c_u32_2 = OpConstant %u32 2\n"										\
	"%c_u32_3 = OpConstant %u32 3\n"										\
	"%c_u32_32 = OpConstant %u32 32\n"										\
	"%c_u32_4 = OpConstant %u32 4\n"

#define SPIRV_ASSEMBLY_ARRAYS												\
	"%a1f32 = OpTypeArray %f32 %c_u32_1\n"									\
	"%a2f32 = OpTypeArray %f32 %c_u32_2\n"									\
	"%a3v4f32 = OpTypeArray %v4f32 %c_u32_3\n"								\
	"%a4f32 = OpTypeArray %f32 %c_u32_4\n"									\
	"%a32v4f32 = OpTypeArray %v4f32 %c_u32_32\n"							\
	"%ip_a3v4f32 = OpTypePointer Input %a3v4f32\n"							\
	"%ip_a32v4f32 = OpTypePointer Input %a32v4f32\n"						\
	"%op_a2f32 = OpTypePointer Output %a2f32\n"								\
	"%op_a3v4f32 = OpTypePointer Output %a3v4f32\n"							\
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
		"OpMemoryModel Logical GLSL450\n"
		"OpEntryPoint Vertex %4 \"main\" %BP_Position %BP_vtxColor %BP_color "
		"%BP_vtxPosition %BP_vertex_id %BP_instance_id\n"
		"${debug:opt}\n"
		"OpName %main \"main\"\n"
		"OpName %BP_vtxPosition \"vtxPosition\"\n"
		"OpName %BP_Position \"position\"\n"
		"OpName %BP_vtxColor \"vtxColor\"\n"
		"OpName %BP_color \"color\"\n"
		"OpName %vertex_id \"gl_VertexID\"\n"
		"OpName %instance_id \"gl_InstanceID\"\n"
		"OpName %test_code \"testfun(vf4;\"\n"
		"OpDecorate %BP_vtxPosition Location 2\n"
		"OpDecorate %BP_Position Location 0\n"
		"OpDecorate %BP_vtxColor Location 1\n"
		"OpDecorate %BP_color Location 1\n"
		"OpDecorate %BP_vertex_id BuiltIn VertexId\n"
		"OpDecorate %BP_instance_id BuiltIn InstanceId\n"
		SPIRV_ASSEMBLY_TYPES
		SPIRV_ASSEMBLY_CONSTANTS
		SPIRV_ASSEMBLY_ARRAYS
		"%BP_vtxPosition = OpVariable %op_v4f32 Output\n"
		"%BP_Position = OpVariable %ip_v4f32 Input\n"
		"%BP_vtxColor = OpVariable %op_v4f32 Output\n"
		"%BP_color = OpVariable %ip_v4f32 Input\n"
		"%BP_vertex_id = OpVariable %ip_i32 Input\n"
		"%BP_instance_id = OpVariable %ip_i32 Input\n"
		"%main = OpFunction %void None %fun\n"
		"%BP_label = OpLabel\n"
		"%BP_tmp_position = OpLoad %v4f32 %BP_Position\n"
		"OpStore %BP_vtxPosition %BP_tmp_position\n"
		"%BP_tmp_color = OpLoad %v4f32 %BP_color\n"
		"%BP_clr_transformed = OpFunctionCall %v4f32 %test_code %BP_tmp_color\n"
		"OpStore %BP_vtxColor %BP_clr_transformed\n"
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
// layout(location = 2) in vec4 in_position[];
// layout(location = 1) out vec4 out_color[];
// layout(location = 2) out vec4 out_position[];
//
// void main() {
//   out_color[gl_InvocationID] = testfun(in_color[gl_InvocationID]);
//   out_position[gl_InvocationID] = in_position[gl_InvocationID];
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
		"OpMemoryModel Logical GLSL450\n"
		"OpEntryPoint TessellationControl %BP_main \"main\" %BP_out_color %BP_gl_InvocationID %BP_in_color %BP_out_position %BP_in_position %BP_gl_TessLevelOuter %BP_gl_TessLevelInner\n"
		"OpExecutionMode %BP_main OutputVertices 3\n"
		"${debug:opt}\n"
		"OpName %BP_main \"main\"\n"
		"OpName %BP_out_color \"out_color\"\n"
		"OpName %BP_gl_InvocationID \"gl_InvocationID\"\n"
		"OpName %BP_in_color \"in_color\"\n"
		"OpName %BP_out_position \"out_position\"\n"
		"OpName %BP_in_position \"in_position\"\n"
		"OpName %BP_gl_TessLevelOuter \"gl_TessLevelOuter\"\n"
		"OpName %BP_gl_TessLevelInner \"gl_TessLevelInner\"\n"
		"OpName %test_code \"testfun(vf4;\"\n"
		"OpDecorate %BP_out_color Location 1\n"
		"OpDecorate %BP_gl_InvocationID BuiltIn InvocationId\n"
		"OpDecorate %BP_in_color Location 1\n"
		"OpDecorate %BP_out_position Location 2\n"
		"OpDecorate %BP_in_position Location 2\n"
		"OpDecorate %BP_gl_TessLevelOuter Patch\n"
		"OpDecorate %BP_gl_TessLevelOuter BuiltIn TessLevelOuter\n"
		"OpDecorate %BP_gl_TessLevelInner Patch\n"
		"OpDecorate %BP_gl_TessLevelInner BuiltIn TessLevelInner\n"
		SPIRV_ASSEMBLY_TYPES
		SPIRV_ASSEMBLY_CONSTANTS
		SPIRV_ASSEMBLY_ARRAYS
		"%BP_out_color = OpVariable %op_a3v4f32 Output\n"
		"%BP_gl_InvocationID = OpVariable %ip_i32 Input\n"
		"%BP_in_color = OpVariable %ip_a32v4f32 Input\n"
		"%BP_out_position = OpVariable %op_a3v4f32 Output\n"
		"%BP_in_position = OpVariable %ip_a32v4f32 Input\n"
		"%BP_gl_TessLevelOuter = OpVariable %op_a4f32 Output\n"
		"%BP_gl_TessLevelInner = OpVariable %op_a2f32 Output\n"

		"%BP_main = OpFunction %void None %fun\n"
		"%BP_label = OpLabel\n"

		"%BP_invocation_id = OpLoad %i32 %BP_gl_InvocationID\n"

		"%BP_in_color_ptr = OpAccessChain %ip_v4f32 %BP_in_color %BP_invocation_id\n"
		"%BP_in_position_ptr = OpAccessChain %ip_v4f32 %BP_in_position %BP_invocation_id\n"

		"%BP_in_color_val = OpLoad %v4f32 %BP_in_color_ptr\n"
		"%BP_in_position_val = OpLoad %v4f32 %BP_in_position_ptr\n"

		"%BP_clr_transformed = OpFunctionCall %v4f32 %test_code %BP_in_color_val\n"

		"%BP_out_color_ptr = OpAccessChain %op_v4f32 %BP_out_color %BP_invocation_id\n"
		"%BP_out_position_ptr = OpAccessChain %op_v4f32 %BP_out_position %BP_invocation_id\n"

		"OpStore %BP_out_color_ptr %BP_clr_transformed\n"
		"OpStore %BP_out_position_ptr %BP_in_position_val\n"

		"%BP_is_first_invocation = OpIEqual %bool %BP_invocation_id %c_i32_0\n"
		"OpSelectionMerge %BP_merge_label None\n"
		"OpBranchConditional %BP_is_first_invocation %BP_first_invocation %BP_merge_label\n"

		"%BP_first_invocation = OpLabel\n"
		"%BP_tess_outer_0 = OpAccessChain %op_f32 %BP_gl_TessLevelOuter %c_i32_0\n"
		"%BP_tess_outer_1 = OpAccessChain %op_f32 %BP_gl_TessLevelOuter %c_i32_1\n"
		"%BP_tess_outer_2 = OpAccessChain %op_f32 %BP_gl_TessLevelOuter %c_i32_2\n"
		"%BP_tess_inner = OpAccessChain %op_f32 %BP_gl_TessLevelInner %c_i32_0\n"

		"OpStore %BP_tess_outer_0 %c_f32_1\n"
		"OpStore %BP_tess_outer_1 %c_f32_1\n"
		"OpStore %BP_tess_outer_2 %c_f32_1\n"
		"OpStore %BP_tess_inner %c_f32_1\n"

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
// layout(location = 2) in vec4 in_position[];
// layout(location = 1) out vec4 out_color;
//
// #define interpolate(val)
//   vec4(gl_TessCoord.x) * val[0] + vec4(gl_TessCoord.y) * val[1] +
//          vec4(gl_TessCoord.z) * val[2]
//
// void main() {
//   gl_Position = vec4(gl_TessCoord.x) * in_position[0] +
//                  vec4(gl_TessCoord.y) * in_position[1] +
//                  vec4(gl_TessCoord.z) * in_position[2];
//   out_color = testfun(interpolate(in_color));
// }
string makeTessEvalShaderAssembly(const map<string, string>& fragments)
{
	static const char tessEvalBoilerplate[] =
		"OpCapability Tessellation\n"
		"OpMemoryModel Logical GLSL450\n"
		"OpEntryPoint TessellationEvaluation %BP_main \"main\" %BP_stream %BP_gl_tessCoord %BP_in_position %BP_out_color %BP_in_color \n"
		"OpExecutionMode %BP_main Triangles\n"
		"${debug:opt}\n"
		"OpName %BP_main \"main\"\n"
		"OpName %BP_per_vertex_out \"gl_PerVertex\"\n"
		"OpMemberName %BP_per_vertex_out 0 \"gl_Position\"\n"
		"OpMemberName %BP_per_vertex_out 1 \"gl_PointSize\"\n"
		"OpMemberName %BP_per_vertex_out 2 \"gl_ClipDistance\"\n"
		"OpMemberName %BP_per_vertex_out 3 \"gl_CullDistance\"\n"
		"OpName %BP_stream \"\"\n"
		"OpName %BP_gl_tessCoord \"gl_TessCoord\"\n"
		"OpName %BP_in_position \"in_position\"\n"
		"OpName %BP_out_color \"out_color\"\n"
		"OpName %BP_in_color \"in_color\"\n"
		"OpName %test_code \"testfun(vf4;\"\n"
		"OpMemberDecorate %BP_per_vertex_out 0 BuiltIn Position\n"
		"OpMemberDecorate %BP_per_vertex_out 1 BuiltIn PointSize\n"
		"OpMemberDecorate %BP_per_vertex_out 2 BuiltIn ClipDistance\n"
		"OpMemberDecorate %BP_per_vertex_out 3 BuiltIn CullDistance\n"
		"OpDecorate %BP_per_vertex_out Block\n"
		"OpDecorate %BP_gl_tessCoord BuiltIn TessCoord\n"
		"OpDecorate %BP_in_position Location 2\n"
		"OpDecorate %BP_out_color Location 1\n"
		"OpDecorate %BP_in_color Location 1\n"
		SPIRV_ASSEMBLY_TYPES
		SPIRV_ASSEMBLY_CONSTANTS
		SPIRV_ASSEMBLY_ARRAYS
		"%BP_per_vertex_out = OpTypeStruct %v4f32 %f32 %a1f32 %a1f32\n"
		"%BP_op_per_vertex_out = OpTypePointer Output %BP_per_vertex_out\n"
		"%BP_stream = OpVariable %BP_op_per_vertex_out Output\n"
		"%BP_gl_tessCoord = OpVariable %ip_v3f32 Input\n"
		"%BP_in_position = OpVariable %ip_a32v4f32 Input\n"
		"%BP_out_color = OpVariable %op_v4f32 Output\n"
		"%BP_in_color = OpVariable %ip_a32v4f32 Input\n"
		"%BP_main = OpFunction %void None %fun\n"
		"%BP_label = OpLabel\n"
		"%BP_tc_0_ptr = OpAccessChain %ip_f32 %BP_gl_tessCoord %c_u32_0\n"
		"%BP_tc_1_ptr = OpAccessChain %ip_f32 %BP_gl_tessCoord %c_u32_1\n"
		"%BP_tc_2_ptr = OpAccessChain %ip_f32 %BP_gl_tessCoord %c_u32_2\n"

		"%BP_tc_0 = OpLoad %f32 %BP_tc_0_ptr\n"
		"%BP_tc_1 = OpLoad %f32 %BP_tc_1_ptr\n"
		"%BP_tc_2 = OpLoad %f32 %BP_tc_2_ptr\n"

		"%BP_in_pos_0_ptr = OpAccessChain %ip_v4f32 %BP_in_position %c_i32_0\n"
		"%BP_in_pos_1_ptr = OpAccessChain %ip_v4f32 %BP_in_position %c_i32_1\n"
		"%BP_in_pos_2_ptr = OpAccessChain %ip_v4f32 %BP_in_position %c_i32_2\n"

		"%BP_in_pos_0 = OpLoad %v4f32 %BP_in_pos_0_ptr\n"
		"%BP_in_pos_1 = OpLoad %v4f32 %BP_in_pos_1_ptr\n"
		"%BP_in_pos_2 = OpLoad %v4f32 %BP_in_pos_2_ptr\n"

		"%BP_in_pos_0_weighted = OpVectorTimesScalar %v4f32 %BP_tc_0 %BP_in_pos_0\n"
		"%BP_in_pos_1_weighted = OpVectorTimesScalar %v4f32 %BP_tc_1 %BP_in_pos_1\n"
		"%BP_in_pos_2_weighted = OpVectorTimesScalar %v4f32 %BP_tc_2 %BP_in_pos_2\n"

		"%BP_out_pos_ptr = OpAccessChain %op_v4f32 %BP_stream %c_i32_0\n"

		"%BP_in_pos_0_plus_pos_1 = OpFAdd %v4f32 %BP_in_pos_0_weighted %BP_in_pos_1_weighted\n"
		"%BP_computed_out = OpFAdd %v4f32 %BP_in_pos_0_plus_pos_1 %BP_in_pos_2_weighted\n"
		"OpStore %BP_out_pos_ptr %BP_computed_out\n"

		"%BP_in_clr_0_ptr = OpAccessChain %ip_v4f32 %BP_in_color %c_i32_0\n"
		"%BP_in_clr_1_ptr = OpAccessChain %ip_v4f32 %BP_in_color %c_i32_1\n"
		"%BP_in_clr_2_ptr = OpAccessChain %ip_v4f32 %BP_in_color %c_i32_2\n"

		"%BP_in_clr_0 = OpLoad %v4f32 %BP_in_clr_0_ptr\n"
		"%BP_in_clr_1 = OpLoad %v4f32 %BP_in_clr_1_ptr\n"
		"%BP_in_clr_2 = OpLoad %v4f32 %BP_in_clr_2_ptr\n"

		"%BP_in_clr_0_weighted = OpVectorTimesScalar %v4f32 %BP_tc_0 %BP_in_clr_0\n"
		"%BP_in_clr_1_weighted = OpVectorTimesScalar %v4f32 %BP_tc_1 %BP_in_clr_1\n"
		"%BP_in_clr_2_weighted = OpVectorTimesScalar %v4f32 %BP_tc_2 %BP_in_clr_2\n"

		"%BP_in_clr_0_plus_col_1 = OpFAdd %v4f32 %BP_in_clr_0_weighted %BP_in_clr_1_weighted\n"
		"%BP_computed_clr = OpFAdd %v4f32 %BP_in_clr_0_plus_col_1 %BP_in_clr_2_weighted\n"
		"%BP_clr_transformed = OpFunctionCall %v4f32 %test_code %BP_computed_clr\n"

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
		"OpMemoryModel Logical GLSL450\n"
		"OpEntryPoint Geometry %BP_main \"main\" %BP_out_gl_position %BP_gl_in %BP_out_color %BP_in_color\n"
		"OpExecutionMode %BP_main Triangles\n"
		"OpExecutionMode %BP_main Invocations 0\n"
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
		"OpDecorate %BP_out_color Stream 0\n"
		"OpDecorate %BP_in_color Location 1\n"
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
		SPIRV_ASSEMBLY_TYPES
		SPIRV_ASSEMBLY_CONSTANTS
		SPIRV_ASSEMBLY_ARRAYS
		"%BP_fragColor = OpVariable %op_v4f32 Output\n"
		"%BP_vtxColor = OpVariable %ip_v4f32 Input\n"
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
void addShaderCodeCustomVertex(vk::SourceCollections& dst, InstanceContext context) {
	map<string, string> passthru = passthruFragments();
	dst.spirvAsmSources.add("vert") << makeVertexShaderAssembly(context.testCodeFragments);
	dst.spirvAsmSources.add("tessc") << makeTessControlShaderAssembly(passthru);
	dst.spirvAsmSources.add("tesse") << makeTessEvalShaderAssembly(passthru);
	dst.spirvAsmSources.add("geom") << makeGeometryShaderAssembly(passthru);
	dst.spirvAsmSources.add("frag") << makeFragmentShaderAssembly(passthru);
}

// Adds shader assembly text to dst.spirvAsmSources for all shader kinds.
// Tessellation control shader gets custom code from context, the rest are
// pass-through.
void addShaderCodeCustomTessControl(vk::SourceCollections& dst, InstanceContext context) {
	map<string, string> passthru = passthruFragments();
	dst.spirvAsmSources.add("vert") << makeVertexShaderAssembly(passthru);
	dst.spirvAsmSources.add("tessc") << makeTessControlShaderAssembly(context.testCodeFragments);
	dst.spirvAsmSources.add("tesse") << makeTessEvalShaderAssembly(passthru);
	dst.spirvAsmSources.add("geom") << makeGeometryShaderAssembly(passthru);
	dst.spirvAsmSources.add("frag") << makeFragmentShaderAssembly(passthru);
}

// Adds shader assembly text to dst.spirvAsmSources for all shader kinds.
// Tessellation evaluation shader gets custom code from context, the rest are
// pass-through.
void addShaderCodeCustomTessEval(vk::SourceCollections& dst, InstanceContext context) {
	map<string, string> passthru = passthruFragments();
	dst.spirvAsmSources.add("vert") << makeVertexShaderAssembly(passthru);
	dst.spirvAsmSources.add("tessc") << makeTessControlShaderAssembly(passthru);
	dst.spirvAsmSources.add("tesse") << makeTessEvalShaderAssembly(context.testCodeFragments);
	dst.spirvAsmSources.add("geom") << makeGeometryShaderAssembly(passthru);
	dst.spirvAsmSources.add("frag") << makeFragmentShaderAssembly(passthru);
}

// Adds shader assembly text to dst.spirvAsmSources for all shader kinds.
// Geometry shader gets custom code from context, the rest are pass-through.
void addShaderCodeCustomGeometry(vk::SourceCollections& dst, InstanceContext context) {
	map<string, string> passthru = passthruFragments();
	dst.spirvAsmSources.add("vert") << makeVertexShaderAssembly(passthru);
	dst.spirvAsmSources.add("tessc") << makeTessControlShaderAssembly(passthru);
	dst.spirvAsmSources.add("tesse") << makeTessEvalShaderAssembly(passthru);
	dst.spirvAsmSources.add("geom") << makeGeometryShaderAssembly(context.testCodeFragments);
	dst.spirvAsmSources.add("frag") << makeFragmentShaderAssembly(passthru);
}

// Adds shader assembly text to dst.spirvAsmSources for all shader kinds.
// Fragment shader gets custom code from context, the rest are pass-through.
void addShaderCodeCustomFragment(vk::SourceCollections& dst, InstanceContext context) {
	map<string, string> passthru = passthruFragments();
	dst.spirvAsmSources.add("vert") << makeVertexShaderAssembly(passthru);
	dst.spirvAsmSources.add("tessc") << makeTessControlShaderAssembly(passthru);
	dst.spirvAsmSources.add("tesse") << makeTessEvalShaderAssembly(passthru);
	dst.spirvAsmSources.add("geom") << makeGeometryShaderAssembly(passthru);
	dst.spirvAsmSources.add("frag") << makeFragmentShaderAssembly(context.testCodeFragments);
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
	const tcu::IVec2							renderSize				(256, 256);
	vector<ModuleHandleSp>						modules;
	map<VkShaderStageFlagBits, VkShaderModule>	moduleByStage;
	const int									testSpecificSeed		= 31354125;
	const int									seed					= context.getTestContext().getCommandLine().getBaseSeed() ^ testSpecificSeed;
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
	vector<VkPipelineShaderStageCreateInfo> shaderStageParams;

	createPipelineShaderStages(vk, vkDevice, instance, context, modules, shaderStageParams);

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
	const VkPipelineInputAssemblyStateCreateInfo	inputAssemblyParams	=
	{
		VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,	//	VkStructureType		sType;
		DE_NULL,														//	const void*			pNext;
		(VkPipelineInputAssemblyStateCreateFlags)0,
		VK_PRIMITIVE_TOPOLOGY_PATCH_LIST,								//	VkPrimitiveTopology	topology;
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
	const VkPipelineDynamicStateCreateInfo	dynamicStateInfo		=
	{
		VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,	//	VkStructureType			sType;
		DE_NULL,												//	const void*				pNext;
		(VkPipelineDynamicStateCreateFlags)0,
		0u,														//	deUint32				dynamicStateCount;
		DE_NULL													//	const VkDynamicState*	pDynamicStates;
	};

	const VkPipelineTessellationStateCreateInfo	tessellationState	=
	{
		VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO,
		DE_NULL,
		(VkPipelineTesselationStateCreateFlags)0,
		3u
	};

	const VkGraphicsPipelineCreateInfo		pipelineParams			=
	{
		VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,		//	VkStructureType									sType;
		DE_NULL,												//	const void*										pNext;
		0u,														//	VkPipelineCreateFlags							flags;
		(deUint32)shaderStageParams.size(),						//	deUint32										stageCount;
		&shaderStageParams[0],									//	const VkPipelineShaderStageCreateInfo*			pStages;
		&vertexInputStateParams,								//	const VkPipelineVertexInputStateCreateInfo*		pVertexInputState;
		&inputAssemblyParams,									//	const VkPipelineInputAssemblyStateCreateInfo*	pInputAssemblyState;
		&tessellationState,										//	const VkPipelineTessellationStateCreateInfo*	pTessellationState;
		&viewportParams,										//	const VkPipelineViewportStateCreateInfo*		pViewportState;
		&rasterParams,											//	const VkPipelineRasterStateCreateInfo*			pRasterState;
		&multisampleParams,										//	const VkPipelineMultisampleStateCreateInfo*		pMultisampleState;
		&depthStencilParams,									//	const VkPipelineDepthStencilStateCreateInfo*	pDepthStencilState;
		&blendParams,											//	const VkPipelineColorBlendStateCreateInfo*		pColorBlendState;
		&dynamicStateInfo,										//	const VkPipelineDynamicStateCreateInfo*			pDynamicState;
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
		DE_NULL,												//	VkRenderPass				renderPass;
		0u,														//	deUint32					subpass;
		DE_NULL,												//	VkFramebuffer				framebuffer;
		VK_FALSE,												//	VkBool32					occlusionQueryEnable;
		(VkQueryControlFlags)0,
		(VkQueryPipelineStatisticFlags)0,
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
		const void*				barriers[]				= { &vertFlushBarrier, &colorAttBarrier };
		vk.cmdPipelineBarrier(*cmdBuf, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, DE_FALSE, (deUint32)DE_LENGTH_OF_ARRAY(barriers), barriers);
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
		const void*				barriers[]				= { &renderFinishBarrier };
		vk.cmdPipelineBarrier(*cmdBuf, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, DE_FALSE, (deUint32)DE_LENGTH_OF_ARRAY(barriers), barriers);
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
		const void*				barriers[]				= { &copyFinishBarrier };
		vk.cmdPipelineBarrier(*cmdBuf, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, DE_FALSE, (deUint32)DE_LENGTH_OF_ARRAY(barriers), barriers);
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

void createTestsForAllStages(const std::string& name,
							 const RGBA (&inputColors)[4],
							 const RGBA (&outputColors)[4],
							 const map<string, string>& testCodeFragments,
							 tcu::TestCaseGroup* tests)
{
	const ShaderElement		pipelineStages[]				=
	{
		ShaderElement("vert", "main", VK_SHADER_STAGE_VERTEX_BIT),
		ShaderElement("tessc", "main", VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT),
		ShaderElement("tesse", "main", VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT),
		ShaderElement("geom", "main", VK_SHADER_STAGE_GEOMETRY_BIT),
		ShaderElement("frag", "main", VK_SHADER_STAGE_FRAGMENT_BIT),
	};

	addFunctionCaseWithPrograms<InstanceContext>(tests, name + "-vert", "", addShaderCodeCustomVertex, runAndVerifyDefaultPipeline,
												 createInstanceContext(pipelineStages, inputColors, outputColors, testCodeFragments));

	addFunctionCaseWithPrograms<InstanceContext>(tests, name + "-tessc", "", addShaderCodeCustomTessControl, runAndVerifyDefaultPipeline,
												 createInstanceContext(pipelineStages, inputColors, outputColors, testCodeFragments));

	addFunctionCaseWithPrograms<InstanceContext>(tests, name + "-tesse", "", addShaderCodeCustomTessEval, runAndVerifyDefaultPipeline,
												 createInstanceContext(pipelineStages, inputColors, outputColors, testCodeFragments));

	addFunctionCaseWithPrograms<InstanceContext>(tests, name + "-geom", "", addShaderCodeCustomGeometry, runAndVerifyDefaultPipeline,
												 createInstanceContext(pipelineStages, inputColors, outputColors, testCodeFragments));

	addFunctionCaseWithPrograms<InstanceContext>(tests, name + "-frag", "", addShaderCodeCustomFragment, runAndVerifyDefaultPipeline,
												 createInstanceContext(pipelineStages, inputColors, outputColors, testCodeFragments));
}
} // anonymous

tcu::TestCaseGroup* createInstructionTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> instructionTests (new tcu::TestCaseGroup(testCtx, "instruction", "Instructions with special opcodes/operands"));

	instructionTests->addChild(createOpNopGroup(testCtx));
	instructionTests->addChild(createOpLineGroup(testCtx));
	instructionTests->addChild(createOpNoLineGroup(testCtx));
	instructionTests->addChild(createOpConstantNullGroup(testCtx));
	instructionTests->addChild(createOpConstantCompositeGroup(testCtx));
	instructionTests->addChild(createOpConstantUsageGroup(testCtx));
	instructionTests->addChild(createSpecConstantGroup(testCtx));
	instructionTests->addChild(createOpSourceGroup(testCtx));
	instructionTests->addChild(createOpSourceExtensionGroup(testCtx));
	instructionTests->addChild(createDecorationGroupGroup(testCtx));
	instructionTests->addChild(createOpPhiGroup(testCtx));
	instructionTests->addChild(createLoopControlGroup(testCtx));
	instructionTests->addChild(createFunctionControlGroup(testCtx));
	instructionTests->addChild(createSelectionControlGroup(testCtx));
	instructionTests->addChild(createBlockOrderGroup(testCtx));
	instructionTests->addChild(createMultipleShaderGroup(testCtx));
	instructionTests->addChild(createMemoryAccessGroup(testCtx));
	instructionTests->addChild(createOpCopyMemoryGroup(testCtx));
	instructionTests->addChild(createOpCopyObjectGroup(testCtx));
	instructionTests->addChild(createNoContractionGroup(testCtx));
	instructionTests->addChild(createOpUndefGroup(testCtx));
	instructionTests->addChild(createOpUnreachableGroup(testCtx));

    RGBA defaultColors[4];
	getDefaultColors(defaultColors);
	de::MovePtr<tcu::TestCaseGroup>	group	(new tcu::TestCaseGroup(testCtx, "graphics-assembly", "Test the graphics pipeline"));
	createTestsForAllStages("passthru", defaultColors, defaultColors, passthruFragments(), group.get());
	instructionTests->addChild(group.release());
	return instructionTests.release();
}

} // SpirVAssembly
} // vkt
