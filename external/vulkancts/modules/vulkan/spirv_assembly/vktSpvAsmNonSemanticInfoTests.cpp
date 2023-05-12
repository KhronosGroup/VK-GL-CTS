/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
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
 *//*!
 * \file
 * \brief SPIR-V non semantic info tests
 *//*--------------------------------------------------------------------*/

#include "vkApiVersion.hpp"

#include "vktSpvAsmNonSemanticInfoTests.hpp"
#include "vktTestCase.hpp"
#include "vktSpvAsmComputeShaderCase.hpp"
#include "vktSpvAsmGraphicsShaderTestUtil.hpp"

#include <limits>

namespace vkt
{
namespace SpirVAssembly
{

using namespace vk;

enum TestType
{
	TT_BASIC					= 0,
	TT_NONEXISTING_INSTRUCTION_SET,
	TT_LARGE_INSTRUCTION_NUMBER,
	TT_MANY_PARAMETERS,
	TT_ANY_CONSTANT_TYPE,
	TT_ANY_CONSTANT_TYPE_USED,
	TT_ANY_NON_CONSTANT_TYPE,
	TT_PLACEMENT
};

static ComputeShaderSpec getComputeShaderSpec ()
{
	deUint32 numElements = 10;
	std::vector<float>	inoutFloats	(10, 0);
	for (size_t ndx = 0; ndx < numElements; ++ndx)
		inoutFloats[ndx] = 1.0f * static_cast<float>(ndx);

	// in one of tests we need to do imageLoad
	// we don't need any special values in here
	std::vector<int> inputInts(256, 0);

	ComputeShaderSpec spec;
	spec.extensions.push_back("VK_KHR_shader_non_semantic_info");
	spec.inputs.push_back(BufferSp(new Float32Buffer(inoutFloats)));
	spec.inputs.push_back(Resource(BufferSp(new Int32Buffer(inputInts)), vk::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE));
	spec.outputs.push_back(BufferSp(new Float32Buffer(inoutFloats)));
	spec.numWorkGroups = tcu::IVec3(numElements, 1, 1);
	return spec;
}

class SpvAsmSpirvNonSemanticInfoBasicInstance : public ComputeShaderSpec, public SpvAsmComputeShaderInstance
{
public:
	SpvAsmSpirvNonSemanticInfoBasicInstance		(Context& ctx, TestType type);

	tcu::TestStatus			iterate				(void);

protected:
	TestType m_testType;
};

SpvAsmSpirvNonSemanticInfoBasicInstance::SpvAsmSpirvNonSemanticInfoBasicInstance(Context& ctx, TestType type)
	: ComputeShaderSpec(getComputeShaderSpec())
	, SpvAsmComputeShaderInstance(ctx, *this)
	, m_testType(type)
{
}

tcu::TestStatus SpvAsmSpirvNonSemanticInfoBasicInstance::iterate (void)
{
	return SpvAsmComputeShaderInstance::iterate();
}

class SpvAsmSpirvNonSemanticInfoBasicCase : public TestCase
{
public:
	SpvAsmSpirvNonSemanticInfoBasicCase		(tcu::TestContext& testCtx, const char* name, TestType type);

	void			checkSupport			(Context& context) const;
	void			initPrograms			(vk::SourceCollections& programCollection) const;
	TestInstance*	createInstance			(Context& context) const;

protected:
	TestType m_testType;
};

SpvAsmSpirvNonSemanticInfoBasicCase::SpvAsmSpirvNonSemanticInfoBasicCase(tcu::TestContext& testCtx, const char* name, TestType type)
	: TestCase (testCtx, name, "")
	, m_testType(type)
{
}

void SpvAsmSpirvNonSemanticInfoBasicCase::checkSupport(Context& context) const
{
	context.requireDeviceFunctionality("VK_KHR_shader_non_semantic_info");
}

void SpvAsmSpirvNonSemanticInfoBasicCase::initPrograms (SourceCollections& programCollection) const
{
	std::string extendedInstructions	= "%extInstSet = OpExtInstImport \"NonSemantic.KHR.DebugInfo\"\n";
	std::string additionalDecorations	= "";
	std::string additionalPreamble		= "";
	std::string additionalTypesAndConst = "";
	std::string beginningOfMain			= "";
	std::string middleOfMain			= "";

	switch (m_testType)
	{
	case TT_BASIC:
		// Minimal test of basic functionality

		additionalPreamble +=
			"%fileStr = OpString \"path\\to\\source.file\"\n"
			"OpSource GLSL 430 %fileStr\n";
		middleOfMain +=
			"%tmp = OpExtInst %void %extInstSet 1 %main %fileStr\n";
		break;

	case TT_NONEXISTING_INSTRUCTION_SET:
		// Testing non existing instruction set

		extendedInstructions =
			"%extInstSet = OpExtInstImport \"NonSemantic.P.B.NonexistingSet\"\n";
		additionalPreamble +=
			"%testStrA = OpString \"this.is.test\"\n"
			"%testStrB = OpString \"yet another test\"\n";
		middleOfMain +=
			"%tmpA = OpExtInst %void %extInstSet 55 %id %testStrA %testStrB\n"
			"OpLine %testStrA 1 1\n"
			"%tmpB = OpExtInst %void %extInstSet 99 %testStrA %main %testStrA\n"
			"OpLine %testStrB 2 2\n"
			"OpNoLine\n";
		break;

	case TT_LARGE_INSTRUCTION_NUMBER:
	{
		// Any instruction number should work - testing large values near uint::max

		deUint32 instNr = std::numeric_limits<deUint32>::max() - 1;
		middleOfMain +=
			"%tmpA = OpExtInst %void %extInstSet " + std::to_string(instNr) + " %main\n" +
			"%tmpB = OpExtInst %void %extInstSet 4294967290 %main\n";
		break;
	}

	case TT_MANY_PARAMETERS:
		// Many parameters should work - testing 100 parameters

		middleOfMain +=
			"%tmp = OpExtInst %void %extInstSet 1234";
		for (deUint32 parameterIndex = 0; parameterIndex < 100; parameterIndex++)
		{
			std::string iStr = std::to_string(parameterIndex);
			std::string strVarName = std::string("%testStr") + iStr;
			additionalPreamble += strVarName + " = OpString \"" + iStr +"\"\n";
			middleOfMain += std::string(" ") + strVarName;
		}
		middleOfMain += "\n";
		break;

	case TT_ANY_CONSTANT_TYPE:
	case TT_ANY_CONSTANT_TYPE_USED:
	{
		// Any type of constant parameter should work - testing undef,
		// int, uint, float, struct, vector, array, string, matrix

		additionalDecorations =
			"OpMemberDecorate %struct 0 Offset 0\n"
			"OpMemberDecorate %struct 1 Offset 4\n"
			"OpMemberDecorate %struct 2 Offset 16\n";

		std::string types =
			"%struct             = OpTypeStruct %f32 %fvec3 %i32\n"
			"%c_array_size       = OpConstant %u32 4\n"
			"%array4             = OpTypeArray %f32 %c_array_size\n"
			"%matrix3x3          = OpTypeMatrix %fvec3 3\n";

		std::string constans =
			"%undef      = OpUndef %i32\n"
			"%c_i32      = OpConstant %i32 -45\n"
			"%c_u32      = OpConstant %u32 99\n"
			"%c_f32      = OpConstant %f32 0.0\n"
			"%c_fvec3    = OpConstantComposite %fvec3 %c_f32 %c_f32 %c_f32\n"
			"%c_struct   = OpConstantComposite %struct %c_f32 %c_fvec3 %undef\n"
			"%c_array    = OpConstantComposite %array4 %c_f32 %c_f32 %c_f32 %c_f32\n"
			"%c_matrix   = OpConstantComposite %matrix3x3 %c_fvec3 %c_fvec3 %c_fvec3\n";

		additionalPreamble +=
			"%testStr = OpString \"\"\n";
		additionalTypesAndConst = types + constans;
		middleOfMain += "%tmp = OpExtInst %void %extInstSet 999 %main %undef %c_i32 %c_u32 %c_f32 %c_struct %c_fvec3 %c_array %testStr %c_matrix\n";
		if (m_testType == TT_ANY_CONSTANT_TYPE)
			break;

		// use all constans outside of OpExtInst
		middleOfMain +=
			"%tmp01      = OpCompositeExtract %f32 %c_fvec3 2\n"
			"%tmp02      = OpFAdd %f32 %tmp01 %c_f32\n"
			"%tmp03      = OpCompositeExtract %f32 %c_struct 0\n"
			"%tmp04      = OpFAdd %f32 %tmp02 %tmp03\n"
			"%tmp05      = OpCompositeExtract %f32 %c_array 1\n"
			"%tmp06      = OpFAdd %f32 %tmp04 %tmp05\n"
			"%tmp07      = OpCompositeExtract %fvec3 %c_matrix 1\n"
			"%tmp08      = OpCompositeExtract %f32 %tmp07 1\n"
			"%tmp09      = OpFMul %f32 %tmp06 %tmp08\n"
			"%tmp10      = OpConvertSToF %f32 %c_i32\n"
			"%tmp11      = OpFMul %f32 %tmp09 %tmp10\n"
			"              OpStore %outloc %tmp11\n";
		break;
	}

	case TT_ANY_NON_CONSTANT_TYPE:
	{
		// Any type of existing semantic result ID should be referencable. Testing
		// the result of a semantic OpExtInst, an entry point, variables of different types,
		// result IDs of buffer and texture loads, result IDs of arithmetic instructions,
		// result of an OpLoad, result of a comparison / logical instruction.

		additionalDecorations =
			"OpMemberDecorate %struct 0 Offset 0\n"
			"OpMemberDecorate %struct 1 Offset 4\n"
			"OpMemberDecorate %struct 2 Offset 16\n";
		extendedInstructions +=
			"%std450 = OpExtInstImport \"GLSL.std.450\"\n";
		additionalTypesAndConst =
			"%struct             = OpTypeStruct %f32 %fvec3 %f32\n"
			"%struct_ptr         = OpTypePointer Function %struct\n"
			"%c_array_size       = OpConstant %u32 4\n"
			"%array4             = OpTypeArray %f32 %c_array_size\n"
			"%array4_ptr         = OpTypePointer Function %array4\n"
			"%matrix3x3          = OpTypeMatrix %fvec3 3\n"
			"%matrix3x3_ptr      = OpTypePointer Function %matrix3x3\n"
			"%ivec2              = OpTypeVector %i32 2\n"
			"%fvec4              = OpTypeVector %f32 4\n"
			"%uv                 = OpConstantComposite %ivec2 %zero %zero\n";

		beginningOfMain =
			"%struct_var = OpVariable %struct_ptr Function\n"
			"%array_var  = OpVariable %array4_ptr Function\n"
			"%matrix_var = OpVariable %matrix3x3_ptr Function\n";
		middleOfMain =
			"%tmp01      = OpExtInst %void %extInstSet 486 %main %id %x %idval %struct_var %array_var %matrix_var %uvec3ptr %indata\n"
			"%arithmRes  = OpIAdd %u32 %x %x\n"
			"%extInstRes = OpExtInst %f32 %std450 FAbs %inval\n"
			"%logicRes   = OpIsNan %bool %inval\n"
			"%imgLoadRes = OpLoad %image_type %image\n"
			"%tmp02      = OpExtInst %void %extInstSet 963 %tmp01 %arithmRes %inloc %outloc %inval %extInstRes %logicRes %imgLoadRes %std450\n";
		break;
	}

	case TT_PLACEMENT:
		// The instructions should be able to be placed at global scope,
		// in the types/constants section and between function definitions

		additionalTypesAndConst =
			"%extInstA   = OpExtInst %void %extInstSet 1 %id\n"			// at global scope
			"%floatf     = OpTypeFunction %f32 %f32\n"
			"%funDefA    = OpFunction %f32 None %floatf\n"
			"%funApa     = OpFunctionParameter %f32\n"
			"%funA       = OpLabel\n"
			"              OpReturnValue %funApa\n"
			"              OpFunctionEnd\n"
			"%extInstB  = OpExtInst %void %extInstSet 3 %id\n";			// between definitions
		middleOfMain +=
			"%aRes       = OpFunctionCall %f32 %funDefA %inval\n"
			"%extInstC   = OpExtInst %void %extInstSet 4 %aRes\n"		// within a block
			"              OpStore %outloc %aRes\n";
		break;
	}

	std::string source =
		getComputeAsmShaderPreamble("", "OpExtension \"SPV_KHR_non_semantic_info\"\n" + extendedInstructions) +
		additionalPreamble +
		"OpDecorate %id BuiltIn GlobalInvocationId\n" +
		"OpDecorate %buf BufferBlock\n"
		"OpDecorate %indata DescriptorSet 0\n"
		"OpDecorate %indata Binding 0\n"
		"OpDecorate %image DescriptorSet 0\n"
		"OpDecorate %image Binding 1\n"
		"OpDecorate %image NonWritable\n"
		"OpDecorate %outdata DescriptorSet 0\n"
		"OpDecorate %outdata Binding 2\n"
		"OpDecorate %f32arr ArrayStride 4\n"
		"OpMemberDecorate %buf 0 Offset 0\n" +
		additionalDecorations +
		std::string(getComputeAsmCommonTypes()) +
		std::string(getComputeAsmInputOutputBuffer()) +
		"%id         = OpVariable %uvec3ptr Input\n"
		"%image_type = OpTypeImage %f32 2D 0 0 0 2 Rgba8\n"
		"%image_ptr  = OpTypePointer UniformConstant %image_type\n"
		"%image      = OpVariable %image_ptr UniformConstant\n"
		"%zero       = OpConstant %i32 0\n" +
		additionalTypesAndConst +
		"%main       = OpFunction %void None %voidf\n"
		"%label      = OpLabel\n" +
		beginningOfMain +
		"%idval      = OpLoad %uvec3 %id\n"
		"%x          = OpCompositeExtract %u32 %idval 0\n"
		"%inloc      = OpAccessChain %f32ptr %indata %zero %x\n"
		"%outloc     = OpAccessChain %f32ptr %outdata %zero %x\n"
		"%inval      = OpLoad %f32 %inloc\n" +
		middleOfMain +
		"             OpStore %outloc %inval\n"
		"             OpReturn\n"
		"             OpFunctionEnd\n";

	programCollection.spirvAsmSources.add("compute") << source;
}

TestInstance* SpvAsmSpirvNonSemanticInfoBasicCase::createInstance (Context& context) const
{
	return new SpvAsmSpirvNonSemanticInfoBasicInstance(context, m_testType);
}

tcu::TestCaseGroup* createNonSemanticInfoGroup(tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> group (new tcu::TestCaseGroup(testCtx, "non_semantic_info", "Test for VK_KHR_shader_non_semantic_info"));

	struct TestData
	{
		const char*	name;
		TestType	type;
	};
	std::vector<TestData> testList =
	{
		{ "basic",						TT_BASIC },
		{ "dummy_instruction_set",		TT_NONEXISTING_INSTRUCTION_SET },
		{ "large_instruction_number",	TT_LARGE_INSTRUCTION_NUMBER },
		{ "many_parameters",			TT_MANY_PARAMETERS },
		{ "any_constant_type",			TT_ANY_CONSTANT_TYPE },
		{ "any_constant_type_used",		TT_ANY_CONSTANT_TYPE_USED },
		{ "any_non_constant_type",		TT_ANY_NON_CONSTANT_TYPE },
		{ "placement",					TT_PLACEMENT },
	};

	for (const auto& item : testList)
		group->addChild(new SpvAsmSpirvNonSemanticInfoBasicCase(testCtx, item.name, item.type));

	return group.release();
}

} // SpirVAssembly
} // vkt
