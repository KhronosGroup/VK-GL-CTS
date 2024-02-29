/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2019 The Khronos Group Inc.
 * Copyright (c) 2019 Valve Corporation.
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
 */ /*!
 * \file
 * \brief Subgroups Tests
 */ /*--------------------------------------------------------------------*/

#include "vktSubgroupsBallotMasksTests.hpp"
#include "vktSubgroupsTestsUtils.hpp"

#include <string>
#include <vector>

using namespace tcu;
using namespace std;
using namespace vk;
using namespace vkt;

namespace
{

enum MaskType
{
	MASKTYPE_EQ = 0,
	MASKTYPE_GE,
	MASKTYPE_GT,
	MASKTYPE_LE,
	MASKTYPE_LT,
	MASKTYPE_LAST
};

struct CaseDefinition
{
	MaskType			maskType;
	VkShaderStageFlags	shaderStage;
	de::SharedPtr<bool>	geometryPointSizeSupported;
	deBool				requiredSubgroupSize;
};

static bool checkVertexPipelineStages (const void*			internalData,
									   vector<const void*>	datas,
									   deUint32				width,
									   deUint32)
{
	DE_UNREF(internalData);

	return subgroups::check(datas, width, 0xf);
}

static bool checkComputeOrMesh (const void*			internalData,
								vector<const void*>	datas,
								const deUint32		numWorkgroups[3],
								const deUint32		localSize[3],
								deUint32)
{
	DE_UNREF(internalData);

	return subgroups::checkComputeOrMesh(datas, numWorkgroups, localSize, 0xf);
}

string getMaskTypeName (const MaskType maskType)
{
	switch (maskType)
	{
		case MASKTYPE_EQ:	return "gl_SubGroupEqMaskARB";
		case MASKTYPE_GE:	return "gl_SubGroupGeMaskARB";
		case MASKTYPE_GT:	return "gl_SubGroupGtMaskARB";
		case MASKTYPE_LE:	return "gl_SubGroupLeMaskARB";
		case MASKTYPE_LT:	return "gl_SubGroupLtMaskARB";
		default:			TCU_THROW(InternalError, "Unsupported mask type");
	}
}

string getBodySource (const CaseDefinition& caseDef)
{
	string	body	=
		"  uint64_t value = " + getMaskTypeName(caseDef.maskType) + ";\n"
		"  bool temp = true;\n";

	switch(caseDef.maskType)
	{
		case MASKTYPE_EQ:
			body += "  uint64_t mask = uint64_t(1) << gl_SubGroupInvocationARB;\n"
					"  temp = (value & mask) != 0;\n";
			break;
		case MASKTYPE_GE:
			body += "  for (uint i = 0; i < gl_SubGroupSizeARB; i++) {\n"
					"    uint64_t mask = uint64_t(1) << i;\n"
					"    if (i >= gl_SubGroupInvocationARB && (value & mask) == 0)\n"
					"       temp = false;\n"
					"    if (i < gl_SubGroupInvocationARB && (value & mask) != 0)\n"
					"       temp = false;\n"
					"  };\n";
			break;
		case MASKTYPE_GT:
			body += "  for (uint i = 0; i < gl_SubGroupSizeARB; i++) {\n"
					"    uint64_t mask = uint64_t(1) << i;\n"
					"    if (i > gl_SubGroupInvocationARB && (value & mask) == 0)\n"
					"       temp = false;\n"
					"    if (i <= gl_SubGroupInvocationARB && (value & mask) != 0)\n"
					"       temp = false;\n"
					"  };\n";
			break;
		case MASKTYPE_LE:
			body += "  for (uint i = 0; i < gl_SubGroupSizeARB; i++) {\n"
					"    uint64_t mask = uint64_t(1) << i;\n"
					"    if (i <= gl_SubGroupInvocationARB && (value & mask) == 0)\n"
					"       temp = false;\n"
					"    if (i > gl_SubGroupInvocationARB && (value & mask) != 0)\n"
					"       temp = false;\n"
					"  };\n";
			break;
		case MASKTYPE_LT:
			body += "  for (uint i = 0; i < gl_SubGroupSizeARB; i++) {\n"
					"    uint64_t mask = uint64_t(1) << i;\n"
					"    if (i < gl_SubGroupInvocationARB && (value & mask) == 0)\n"
					"       temp = false;\n"
					"    if (i >= gl_SubGroupInvocationARB && (value & mask) != 0)\n"
					"       temp = false;\n"
					"  };\n";
			break;
		default:
			TCU_THROW(InternalError, "Unknown mask type");
	}

	body += "  uint tempResult = temp ? 0xf : 0x2;\n";
	body += "  tempRes = tempResult;\n";

	return body;
}

string getExtHeader (const CaseDefinition&)
{
	return
		"#extension GL_ARB_shader_ballot: enable\n"
		"#extension GL_ARB_gpu_shader_int64: enable\n";
}

vector<string> getPerStageHeadDeclarations (const CaseDefinition& caseDef)
{
	const deUint32	stageCount	= subgroups::getStagesCount(caseDef.shaderStage);
	const bool		fragment	= (caseDef.shaderStage & VK_SHADER_STAGE_FRAGMENT_BIT) != 0;
	vector<string>	result		(stageCount, string());

	if (fragment)
		result.reserve(result.size() + 1);

	for (size_t i = 0; i < result.size(); ++i)
	{
		result[i] =
			"layout(set = 0, binding = " + de::toString(i) + ", std430) buffer Buffer1\n"
			"{\n"
			"  uint result[];\n"
			"};\n";
	}

	if (fragment)
	{
		const string	fragPart	=
			"layout(location = 0) out uint result;\n";

		result.push_back(fragPart);
	}

	return result;
}

vector<string> getFramebufferPerStageHeadDeclarations (const CaseDefinition& caseDef)
{
	vector<string>	result;

	DE_UNREF(caseDef);

	result.push_back("layout(location = 0) out float result;\n");
	result.push_back("layout(location = 0) out float out_color;\n");
	result.push_back("layout(location = 0) out float out_color[];\n");
	result.push_back("layout(location = 0) out float out_color;\n");

	return result;
}

void initFrameBufferPrograms (SourceCollections& programCollection, CaseDefinition caseDef)
{
	const ShaderBuildOptions	buildOptions		(programCollection.usedVulkanVersion, SPIRV_VERSION_1_3, 0u);
	const string				extHeader			= getExtHeader(caseDef);
	const string				testSrc				= getBodySource(caseDef);
	const vector<string>		headDeclarations	= getFramebufferPerStageHeadDeclarations(caseDef);
	const bool					pointSizeSupported	= *caseDef.geometryPointSizeSupported;

	subgroups::initStdFrameBufferPrograms(programCollection, buildOptions, caseDef.shaderStage, VK_FORMAT_R32_UINT, pointSizeSupported, extHeader, testSrc, "", headDeclarations);
}

void initPrograms (SourceCollections& programCollection, CaseDefinition caseDef)
{
#ifndef CTS_USES_VULKANSC
	const bool					spirv14required		= (isAllRayTracingStages(caseDef.shaderStage) || isAllMeshShadingStages(caseDef.shaderStage));
#else
	const bool					spirv14required		= false;
#endif // CTS_USES_VULKANSC
	const SpirvVersion			spirvVersion		= (spirv14required ? SPIRV_VERSION_1_4 : SPIRV_VERSION_1_3);
	const ShaderBuildOptions	buildOptions		(programCollection.usedVulkanVersion, spirvVersion, 0u, spirv14required);
	const string				extHeader			= getExtHeader(caseDef);
	const string				testSrc				= getBodySource(caseDef);
	const vector<string>		headDeclarations	= getPerStageHeadDeclarations(caseDef);
	const bool					pointSizeSupport	= *caseDef.geometryPointSizeSupported;
	const SpirVAsmBuildOptions	buildOptionsSpr     (programCollection.usedVulkanVersion, SPIRV_VERSION_1_3);

	if (isAllComputeStages(caseDef.shaderStage))
	{
		string compute = "";
		switch (caseDef.maskType)
		{
			case MASKTYPE_EQ:
				compute +=
					"; SPIR-V\n"
					"; Version: 1.6\n"
					"; Generator: Khronos SPIR-V Tools Assembler; 0\n"
					"; Bound: 98\n"
					"; Schema: 0\n"
					"OpCapability Shader\n"
					"OpCapability Int64\n"
					"OpCapability SubgroupBallotKHR\n"
					"OpExtension \"SPV_KHR_shader_ballot\"\n"
					"%1 = OpExtInstImport \"GLSL.std.450\"\n"
					"OpMemoryModel Logical GLSL450\n"
					"OpEntryPoint GLCompute %main \"main\" %gl_NumWorkGroups %gl_GlobalInvocationID %gl_SubGroupEqMaskARB %gl_SubGroupInvocationARB\n"
					"OpExecutionMode %main LocalSize 1 1 1\n"
					"OpSource GLSL 450\n"
					"OpSourceExtension \"GL_ARB_gpu_shader_int64\"\n"
					"OpSourceExtension \"GL_ARB_shader_ballot\"\n"
					"OpName %main \"main\"\n"
					"OpName %globalSize \"globalSize\"\n"
					"OpName %gl_NumWorkGroups \"gl_NumWorkGroups\"\n"
					"OpName %offset \"offset\"\n"
					"OpName %gl_GlobalInvocationID \"gl_GlobalInvocationID\"\n"
					"OpName %bitmask \"bitmask\"\n"
					"OpName %gl_SubGroupEqMaskARB \"gl_SubGroupEqMaskARB\"\n"
					"OpName %temp \"temp\"\n"
					"OpName %elementIndex \"elementIndex\"\n"
					"OpName %gl_SubGroupInvocationARB \"gl_SubGroupInvocationARB\"\n"
					"OpName %bitPosition \"bitPosition\"\n"
					"OpName %mask \"mask\"\n"
					"OpName %element \"element\"\n"
					"OpName %tempResult \"tempResult\"\n"
					"OpName %tempRes \"tempRes\"\n"
					"OpName %Buffer1 \"Buffer1\"\n"
					"OpMemberName %Buffer1 0 \"result\"\n"
					"OpName %_ \"\"\n"
					"OpDecorate %gl_NumWorkGroups BuiltIn NumWorkgroups\n"
					"OpDecorate %19 SpecId 0\n"
					"OpDecorate %20 SpecId 1\n"
					"OpDecorate %21 SpecId 2\n"
					"OpDecorate %gl_WorkGroupSize BuiltIn WorkgroupSize\n"
					"OpDecorate %gl_GlobalInvocationID BuiltIn GlobalInvocationId\n"
					"OpDecorate %gl_SubGroupEqMaskARB BuiltIn SubgroupEqMask\n"
					"OpDecorate %gl_SubGroupInvocationARB BuiltIn SubgroupLocalInvocationId\n"
					"OpDecorate %_runtimearr_uint ArrayStride 4\n"
					"OpMemberDecorate %Buffer1 0 Offset 0\n"
					"OpDecorate %Buffer1 BufferBlock\n"
					"OpDecorate %_ DescriptorSet 0\n"
					"OpDecorate %_ Binding 0\n"
					"%void = OpTypeVoid\n"
					"%25 = OpTypeFunction %void\n"
					"%uint = OpTypeInt 32 0\n"
					"%v3uint = OpTypeVector %uint 3\n"
					"%_ptr_Function_v3uint = OpTypePointer Function %v3uint\n"
					"%_ptr_Input_v3uint = OpTypePointer Input %v3uint\n"
					"%gl_NumWorkGroups = OpVariable %_ptr_Input_v3uint Input\n"
					"%19 = OpSpecConstant %uint 1\n"
					"%20 = OpSpecConstant %uint 1\n"
					"%21 = OpSpecConstant %uint 1\n"
					"%gl_WorkGroupSize = OpSpecConstantComposite %v3uint %19 %20 %21\n"
					"%_ptr_Function_uint = OpTypePointer Function %uint\n"
					"%uint_0 = OpConstant %uint 0\n"
					"%uint_1 = OpConstant %uint 1\n"
					"%gl_GlobalInvocationID = OpVariable %_ptr_Input_v3uint Input\n"
					"%uint_2 = OpConstant %uint 2\n"
					"%_ptr_Input_uint = OpTypePointer Input %uint\n"
					"%int = OpTypeInt 32 1\n"
					"%v4uint = OpTypeVector %uint 4\n"
					"%uint_4 = OpConstant %uint 4\n"
					"%_arr_uint_uint_4 = OpTypeArray %uint %uint_4\n"
					"%_ptr_Function_v4uint = OpTypePointer Function %v4uint\n"
					"%_ptr_Function__arr_uint_uint_4 = OpTypePointer Function %_arr_uint_uint_4\n"
					"%ulong = OpTypeInt 64 0\n"
					"%_ptr_Input_ulong = OpTypePointer Input %ulong\n"
					"%_ptr_Input_v4uint = OpTypePointer Input %v4uint\n"
					"%gl_SubGroupEqMaskARB = OpVariable %_ptr_Input_v4uint Input\n"
					"%bool = OpTypeBool\n"
					"%_ptr_Function_bool = OpTypePointer Function %bool\n"
					"%true = OpConstantTrue %bool\n"
					"%gl_SubGroupInvocationARB = OpVariable %_ptr_Input_uint Input\n"
					"%uint_32 = OpConstant %uint 32\n"
					"%_ptr_Function_int = OpTypePointer Function %int\n"
					"%int_15 = OpConstant %int 15\n"
					"%int_2 = OpConstant %int 2\n"
					"%_runtimearr_uint = OpTypeRuntimeArray %uint\n"
					"%Buffer1 = OpTypeStruct %_runtimearr_uint\n"
					"%_ptr_Uniform_Buffer1 = OpTypePointer Uniform %Buffer1\n"
					"%_ = OpVariable %_ptr_Uniform_Buffer1 Uniform\n"
					"%int_0 = OpConstant %int 0\n"
					"%_ptr_Uniform_uint = OpTypePointer Uniform %uint\n"
					"%main = OpFunction %void None %25\n"
					"%54 = OpLabel\n"
					"%globalSize = OpVariable %_ptr_Function_v3uint Function\n"
					"%offset = OpVariable %_ptr_Function_uint Function\n"
					"%bitmask = OpVariable %_ptr_Function__arr_uint_uint_4 Function\n"
					"%temp = OpVariable %_ptr_Function_bool Function\n"
					"%elementIndex = OpVariable %_ptr_Function_uint Function\n"
					"%bitPosition = OpVariable %_ptr_Function_uint Function\n"
					"%mask = OpVariable %_ptr_Function_uint Function\n"
					"%element = OpVariable %_ptr_Function_uint Function\n"
					"%tempResult = OpVariable %_ptr_Function_uint Function\n"
					"%tempRes = OpVariable %_ptr_Function_uint Function\n"
					"%55 = OpLoad %v3uint %gl_NumWorkGroups\n"
					"%56 = OpIMul %v3uint %55 %gl_WorkGroupSize\n"
					"OpStore %globalSize %56\n"
					"%57 = OpAccessChain %_ptr_Function_uint %globalSize %uint_0\n"
					"%58 = OpLoad %uint %57\n"
					"%59 = OpAccessChain %_ptr_Function_uint %globalSize %uint_1\n"
					"%60 = OpLoad %uint %59\n"
					"%61 = OpAccessChain %_ptr_Input_uint %gl_GlobalInvocationID %uint_2\n"
					"%62 = OpLoad %uint %61\n"
					"%63 = OpIMul %uint %60 %62\n"
					"%64 = OpAccessChain %_ptr_Input_uint %gl_GlobalInvocationID %uint_1\n"
					"%65 = OpLoad %uint %64\n"
					"%66 = OpIAdd %uint %63 %65\n"
					"%67 = OpIMul %uint %58 %66\n"
					"%68 = OpAccessChain %_ptr_Input_uint %gl_GlobalInvocationID %uint_0\n"
					"%69 = OpLoad %uint %68\n"
					"%70 = OpIAdd %uint %67 %69\n"
					"OpStore %offset %70\n"
					"%71 = OpLoad %v4uint %gl_SubGroupEqMaskARB\n"
					"%72 = OpCompositeExtract %uint %71 0\n"
					"%73 = OpCompositeExtract %uint %71 1\n"
					"%74 = OpCompositeExtract %uint %71 2\n"
					"%75 = OpCompositeExtract %uint %71 3\n"
					"%76 = OpCompositeConstruct %_arr_uint_uint_4 %72 %73 %74 %75\n"
					"OpStore %bitmask %76\n"
					"OpStore %temp %true\n"
					"%77 = OpLoad %uint %gl_SubGroupInvocationARB\n"
					"%78 = OpUDiv %uint %77 %uint_32\n"
					"OpStore %elementIndex %78\n"
					"%79 = OpLoad %uint %gl_SubGroupInvocationARB\n"
					"%80 = OpUMod %uint %79 %uint_32\n"
					"OpStore %bitPosition %80\n"
					"%81 = OpLoad %uint %bitPosition\n"
					"%82 = OpShiftLeftLogical %uint %uint_1 %81\n"
					"OpStore %mask %82\n"
					"%83 = OpLoad %uint %elementIndex\n"
					"%84 = OpAccessChain %_ptr_Function_uint %bitmask %83\n"
					"%85 = OpLoad %uint %84\n"
					"OpStore %element %85\n"
					"%87 = OpLoad %uint %element\n"
					"%88 = OpLoad %uint %mask\n"
					"%89 = OpBitwiseAnd %uint %87 %88\n"
					"%90 = OpINotEqual %bool %89 %uint_0\n"
					"OpStore %temp %90\n"
					"%91 = OpLoad %bool %temp\n"
					"%92 = OpSelect %int %91 %int_15 %int_2\n"
					"%93 = OpBitcast %uint %92\n"
					"OpStore %tempResult %93\n"
					"%94 = OpLoad %uint %tempResult\n"
					"OpStore %tempRes %94\n"
					"%95 = OpLoad %uint %offset\n"
					"%96 = OpLoad %uint %tempRes\n"
					"%97 = OpAccessChain %_ptr_Uniform_uint %_ %int_0 %95\n"
					"OpStore %97 %96\n"
					"OpReturn\n"
					"OpFunctionEnd\n";
				break;
			case MASKTYPE_GE:
				compute +=
					"; SPIR-V\n"
					"; Version: 1.6\n"
					"; Generator: Khronos SPIR-V Tools Assembler; 0\n"
					"; Bound: 128\n"
					"; Schema: 0\n"
					"OpCapability Shader\n"
					"OpCapability Int64\n"
					"OpCapability SubgroupBallotKHR\n"
					"OpExtension \"SPV_KHR_shader_ballot\"\n"
					"%1 = OpExtInstImport \"GLSL.std.450\"\n"
					"OpMemoryModel Logical GLSL450\n"
					"OpEntryPoint GLCompute %main \"main\" %gl_NumWorkGroups %gl_GlobalInvocationID %gl_SubGroupGeMaskARB %gl_SubGroupSizeARB %gl_SubGroupInvocationARB\n"
					"OpExecutionMode %main LocalSize 1 1 1\n"
					"OpSource GLSL 450\n"
					"OpSourceExtension \"GL_ARB_gpu_shader_int64\"\n"
					"OpSourceExtension \"GL_ARB_shader_ballot\"\n"
					"OpName %main \"main\"\n"
					"OpName %globalSize \"globalSize\"\n"
					"OpName %gl_NumWorkGroups \"gl_NumWorkGroups\"\n"
					"OpName %offset \"offset\"\n"
					"OpName %gl_GlobalInvocationID \"gl_GlobalInvocationID\"\n"
					"OpName %bitmask \"bitmask\"\n"
					"OpName %gl_SubGroupGeMaskARB \"gl_SubGroupGeMaskARB\"\n"
					"OpName %temp \"temp\"\n"
					"OpName %i \"i\"\n"
					"OpName %gl_SubGroupSizeARB \"gl_SubGroupSizeARB\"\n"
					"OpName %elementIndex \"elementIndex\"\n"
					"OpName %bitPosition \"bitPosition\"\n"
					"OpName %mask \"mask\"\n"
					"OpName %element \"element\"\n"
					"OpName %gl_SubGroupInvocationARB \"gl_SubGroupInvocationARB\"\n"
					"OpName %tempResult \"tempResult\"\n"
					"OpName %tempRes \"tempRes\"\n"
					"OpName %Buffer1 \"Buffer1\"\n"
					"OpMemberName %Buffer1 0 \"result\"\n"
					"OpName %_ \"\"\n"
					"OpDecorate %gl_NumWorkGroups BuiltIn NumWorkgroups\n"
					"OpDecorate %21 SpecId 0\n"
					"OpDecorate %22 SpecId 1\n"
					"OpDecorate %23 SpecId 2\n"
					"OpDecorate %gl_WorkGroupSize BuiltIn WorkgroupSize\n"
					"OpDecorate %gl_GlobalInvocationID BuiltIn GlobalInvocationId\n"
					"OpDecorate %gl_SubGroupGeMaskARB BuiltIn SubgroupGeMask\n"
					"OpDecorate %gl_SubGroupSizeARB BuiltIn SubgroupSize\n"
					"OpDecorate %gl_SubGroupInvocationARB BuiltIn SubgroupLocalInvocationId\n"
					"OpDecorate %_runtimearr_uint ArrayStride 4\n"
					"OpMemberDecorate %Buffer1 0 Offset 0\n"
					"OpDecorate %Buffer1 BufferBlock\n"
					"OpDecorate %_ DescriptorSet 0\n"
					"OpDecorate %_ Binding 0\n"
					"%void = OpTypeVoid\n"
					"%27 = OpTypeFunction %void\n"
					"%uint = OpTypeInt 32 0\n"
					"%v3uint = OpTypeVector %uint 3\n"
					"%_ptr_Function_v3uint = OpTypePointer Function %v3uint\n"
					"%_ptr_Input_v3uint = OpTypePointer Input %v3uint\n"
					"%gl_NumWorkGroups = OpVariable %_ptr_Input_v3uint Input\n"
					"%21 = OpSpecConstant %uint 1\n"
					"%22 = OpSpecConstant %uint 1\n"
					"%23 = OpSpecConstant %uint 1\n"
					"%gl_WorkGroupSize = OpSpecConstantComposite %v3uint %21 %22 %23\n"
					"%_ptr_Function_uint = OpTypePointer Function %uint\n"
					"%uint_0 = OpConstant %uint 0\n"
					"%uint_1 = OpConstant %uint 1\n"
					"%gl_GlobalInvocationID = OpVariable %_ptr_Input_v3uint Input\n"
					"%uint_2 = OpConstant %uint 2\n"
					"%_ptr_Input_uint = OpTypePointer Input %uint\n"
					"%v4uint = OpTypeVector %uint 4\n"
					"%uint_4 = OpConstant %uint 4\n"
					"%_arr_uint_uint_4 = OpTypeArray %uint %uint_4\n"
					"%_ptr_Function_v4uint = OpTypePointer Function %v4uint\n"
					"%_ptr_Function__arr_uint_uint_4 = OpTypePointer Function %_arr_uint_uint_4\n"
					"%_ptr_Input_v4uint = OpTypePointer Input %v4uint\n"
					"%gl_SubGroupGeMaskARB = OpVariable %_ptr_Input_v4uint Input\n"
					"%bool = OpTypeBool\n"
					"%_ptr_Function_bool = OpTypePointer Function %bool\n"
					"%true = OpConstantTrue %bool\n"
					"%gl_SubGroupSizeARB = OpVariable %_ptr_Input_uint Input\n"
					"%uint_32 = OpConstant %uint 32\n"
					"%gl_SubGroupInvocationARB = OpVariable %_ptr_Input_uint Input\n"
					"%false = OpConstantFalse %bool\n"
					"%int = OpTypeInt 32 1\n"
					"%int_1 = OpConstant %int 1\n"
					"%int_15 = OpConstant %int 15\n"
					"%int_2 = OpConstant %int 2\n"
					"%_runtimearr_uint = OpTypeRuntimeArray %uint\n"
					"%Buffer1 = OpTypeStruct %_runtimearr_uint\n"
					"%_ptr_Uniform_Buffer1 = OpTypePointer Uniform %Buffer1\n"
					"%_ = OpVariable %_ptr_Uniform_Buffer1 Uniform\n"
					"%int_0 = OpConstant %int 0\n"
					"%_ptr_Uniform_uint = OpTypePointer Uniform %uint\n"
					"%main = OpFunction %void None %27\n"
					"%55 = OpLabel\n"
					"%globalSize = OpVariable %_ptr_Function_v3uint Function\n"
					"%offset = OpVariable %_ptr_Function_uint Function\n"
					"%bitmask = OpVariable %_ptr_Function__arr_uint_uint_4 Function\n"
					"%temp = OpVariable %_ptr_Function_bool Function\n"
					"%i = OpVariable %_ptr_Function_uint Function\n"
					"%elementIndex = OpVariable %_ptr_Function_uint Function\n"
					"%bitPosition = OpVariable %_ptr_Function_uint Function\n"
					"%mask = OpVariable %_ptr_Function_uint Function\n"
					"%element = OpVariable %_ptr_Function_uint Function\n"
					"%tempResult = OpVariable %_ptr_Function_uint Function\n"
					"%tempRes = OpVariable %_ptr_Function_uint Function\n"
					"%56 = OpLoad %v3uint %gl_NumWorkGroups\n"
					"%57 = OpIMul %v3uint %56 %gl_WorkGroupSize\n"
					"OpStore %globalSize %57\n"
					"%58 = OpAccessChain %_ptr_Function_uint %globalSize %uint_0\n"
					"%59 = OpLoad %uint %58\n"
					"%60 = OpAccessChain %_ptr_Function_uint %globalSize %uint_1\n"
					"%61 = OpLoad %uint %60\n"
					"%62 = OpAccessChain %_ptr_Input_uint %gl_GlobalInvocationID %uint_2\n"
					"%63 = OpLoad %uint %62\n"
					"%64 = OpIMul %uint %61 %63\n"
					"%65 = OpAccessChain %_ptr_Input_uint %gl_GlobalInvocationID %uint_1\n"
					"%66 = OpLoad %uint %65\n"
					"%67 = OpIAdd %uint %64 %66\n"
					"%68 = OpIMul %uint %59 %67\n"
					"%69 = OpAccessChain %_ptr_Input_uint %gl_GlobalInvocationID %uint_0\n"
					"%70 = OpLoad %uint %69\n"
					"%71 = OpIAdd %uint %68 %70\n"
					"OpStore %offset %71\n"
					"%72 = OpLoad %v4uint %gl_SubGroupGeMaskARB\n"
					"%73 = OpCompositeExtract %uint %72 0\n"
					"%74 = OpCompositeExtract %uint %72 1\n"
					"%75 = OpCompositeExtract %uint %72 2\n"
					"%76 = OpCompositeExtract %uint %72 3\n"
					"%77 = OpCompositeConstruct %_arr_uint_uint_4 %73 %74 %75 %76\n"
					"OpStore %bitmask %77\n"
					"OpStore %temp %true\n"
					"OpStore %i %uint_0\n"
					"OpBranch %78\n"
					"%78 = OpLabel\n"
					"OpLoopMerge %79 %80 None\n"
					"OpBranch %81\n"
					"%81 = OpLabel\n"
					"%82 = OpLoad %uint %i\n"
					"%83 = OpLoad %uint %gl_SubGroupSizeARB\n"
					"%84 = OpULessThan %bool %82 %83\n"
					"OpBranchConditional %84 %85 %79\n"
					"%85 = OpLabel\n"
					"%86 = OpLoad %uint %i\n"
					"%87 = OpUDiv %uint %86 %uint_32\n"
					"OpStore %elementIndex %87\n"
					"%88 = OpLoad %uint %i\n"
					"%89 = OpUMod %uint %88 %uint_32\n"
					"OpStore %bitPosition %89\n"
					"%90 = OpLoad %uint %bitPosition\n"
					"%91 = OpShiftLeftLogical %uint %uint_1 %90\n"
					"OpStore %mask %91\n"
					"%92 = OpLoad %uint %elementIndex\n"
					"%93 = OpAccessChain %_ptr_Function_uint %bitmask %92\n"
					"%94 = OpLoad %uint %93\n"
					"OpStore %element %94\n"
					"%95 = OpLoad %uint %i\n"
					"%96 = OpLoad %uint %gl_SubGroupInvocationARB\n"
					"%97 = OpUGreaterThanEqual %bool %95 %96\n"
					"OpSelectionMerge %98 None\n"
					"OpBranchConditional %97 %99 %98\n"
					"%99 = OpLabel\n"
					"%100 = OpLoad %uint %element\n"
					"%101 = OpLoad %uint %mask\n"
					"%102 = OpBitwiseAnd %uint %100 %101\n"
					"%103 = OpIEqual %bool %102 %uint_0\n"
					"OpBranch %98\n"
					"%98 = OpLabel\n"
					"%104 = OpPhi %bool %97 %85 %103 %99\n"
					"OpSelectionMerge %105 None\n"
					"OpBranchConditional %104 %106 %105\n"
					"%106 = OpLabel\n"
					"OpStore %temp %false\n"
					"OpBranch %105\n"
					"%105 = OpLabel\n"
					"%107 = OpLoad %uint %i\n"
					"%108 = OpLoad %uint %gl_SubGroupInvocationARB\n"
					"%109 = OpULessThan %bool %107 %108\n"
					"OpSelectionMerge %110 None\n"
					"OpBranchConditional %109 %111 %110\n"
					"%111 = OpLabel\n"
					"%112 = OpLoad %uint %element\n"
					"%113 = OpLoad %uint %mask\n"
					"%114 = OpBitwiseAnd %uint %112 %113\n"
					"%115 = OpINotEqual %bool %114 %uint_0\n"
					"OpBranch %110\n"
					"%110 = OpLabel\n"
					"%116 = OpPhi %bool %109 %105 %115 %111\n"
					"OpSelectionMerge %117 None\n"
					"OpBranchConditional %116 %118 %117\n"
					"%118 = OpLabel\n"
					"OpStore %temp %false\n"
					"OpBranch %117\n"
					"%117 = OpLabel\n"
					"OpBranch %80\n"
					"%80 = OpLabel\n"
					"%119 = OpLoad %uint %i\n"
					"%120 = OpIAdd %uint %119 %int_1\n"
					"OpStore %i %120\n"
					"OpBranch %78\n"
					"%79 = OpLabel\n"
					"%121 = OpLoad %bool %temp\n"
					"%122 = OpSelect %int %121 %int_15 %int_2\n"
					"%123 = OpBitcast %uint %122\n"
					"OpStore %tempResult %123\n"
					"%124 = OpLoad %uint %tempResult\n"
					"OpStore %tempRes %124\n"
					"%125 = OpLoad %uint %offset\n"
					"%126 = OpLoad %uint %tempRes\n"
					"%127 = OpAccessChain %_ptr_Uniform_uint %_ %int_0 %125\n"
					"OpStore %127 %126\n"
					"OpReturn\n"
					"OpFunctionEnd\n";
				break;
			case MASKTYPE_GT:
				compute +=
					"; SPIR-V\n"
					"; Version: 1.6\n"
					"; Generator: Khronos SPIR-V Tools Assembler; 0\n"
					"; Bound: 130\n"
					"; Schema: 0\n"
					"OpCapability Shader\n"
					"OpCapability Int64\n"
					"OpCapability SubgroupBallotKHR\n"
					"OpExtension \"SPV_KHR_shader_ballot\"\n"
					"%1 = OpExtInstImport \"GLSL.std.450\"\n"
					"OpMemoryModel Logical GLSL450\n"
					"OpEntryPoint GLCompute %main \"main\" %gl_NumWorkGroups %gl_GlobalInvocationID %gl_SubGroupGtMaskARB %gl_SubGroupSizeARB %gl_SubGroupInvocationARB\n"
					"OpExecutionMode %main LocalSize 1 1 1\n"
					"OpSource GLSL 450\n"
					"OpSourceExtension \"GL_ARB_gpu_shader_int64\"\n"
					"OpSourceExtension \"GL_ARB_shader_ballot\"\n"
					"OpName %main \"main\"\n"
					"OpName %globalSize \"globalSize\"\n"
					"OpName %gl_NumWorkGroups \"gl_NumWorkGroups\"\n"
					"OpName %offset \"offset\"\n"
					"OpName %gl_GlobalInvocationID \"gl_GlobalInvocationID\"\n"
					"OpName %bitmask \"bitmask\"\n"
					"OpName %gl_SubGroupGtMaskARB \"gl_SubGroupGtMaskARB\"\n"
					"OpName %temp \"temp\"\n"
					"OpName %i \"i\"\n"
					"OpName %gl_SubGroupSizeARB \"gl_SubGroupSizeARB\"\n"
					"OpName %elementIndex \"elementIndex\"\n"
					"OpName %bitPosition \"bitPosition\"\n"
					"OpName %mask \"mask\"\n"
					"OpName %element \"element\"\n"
					"OpName %gl_SubGroupInvocationARB \"gl_SubGroupInvocationARB\"\n"
					"OpName %tempResult \"tempResult\"\n"
					"OpName %tempRes \"tempRes\"\n"
					"OpName %Buffer1 \"Buffer1\"\n"
					"OpMemberName %Buffer1 0 \"result\"\n"
					"OpName %_ \"\"\n"
					"OpDecorate %gl_NumWorkGroups BuiltIn NumWorkgroups\n"
					"OpDecorate %21 SpecId 0\n"
					"OpDecorate %22 SpecId 1\n"
					"OpDecorate %23 SpecId 2\n"
					"OpDecorate %gl_WorkGroupSize BuiltIn WorkgroupSize\n"
					"OpDecorate %gl_GlobalInvocationID BuiltIn GlobalInvocationId\n"
					"OpDecorate %gl_SubGroupGtMaskARB BuiltIn SubgroupGtMask\n"
					"OpDecorate %gl_SubGroupSizeARB BuiltIn SubgroupSize\n"
					"OpDecorate %gl_SubGroupInvocationARB BuiltIn SubgroupLocalInvocationId\n"
					"OpDecorate %_runtimearr_uint ArrayStride 4\n"
					"OpMemberDecorate %Buffer1 0 Offset 0\n"
					"OpDecorate %Buffer1 BufferBlock\n"
					"OpDecorate %_ DescriptorSet 0\n"
					"OpDecorate %_ Binding 0\n"
					"%void = OpTypeVoid\n"
					"%27 = OpTypeFunction %void\n"
					"%uint = OpTypeInt 32 0\n"
					"%v3uint = OpTypeVector %uint 3\n"
					"%_ptr_Function_v3uint = OpTypePointer Function %v3uint\n"
					"%_ptr_Input_v3uint = OpTypePointer Input %v3uint\n"
					"%gl_NumWorkGroups = OpVariable %_ptr_Input_v3uint Input\n"
					"%21 = OpSpecConstant %uint 1\n"
					"%22 = OpSpecConstant %uint 1\n"
					"%23 = OpSpecConstant %uint 1\n"
					"%gl_WorkGroupSize = OpSpecConstantComposite %v3uint %21 %22 %23\n"
					"%_ptr_Function_uint = OpTypePointer Function %uint\n"
					"%uint_0 = OpConstant %uint 0\n"
					"%uint_1 = OpConstant %uint 1\n"
					"%gl_GlobalInvocationID = OpVariable %_ptr_Input_v3uint Input\n"
					"%uint_2 = OpConstant %uint 2\n"
					"%_ptr_Input_uint = OpTypePointer Input %uint\n"
					"%v4uint = OpTypeVector %uint 4\n"
					"%uint_4 = OpConstant %uint 4\n"
					"%_arr_uint_uint_4 = OpTypeArray %uint %uint_4\n"
					"%_ptr_Function_v4uint = OpTypePointer Function %v4uint\n"
					"%_ptr_Function__arr_uint_uint_4 = OpTypePointer Function %_arr_uint_uint_4\n"
					"%ulong = OpTypeInt 64 0\n"
					"%_ptr_Input_ulong = OpTypePointer Input %ulong\n"
					"%_ptr_Input_v4uint = OpTypePointer Input %v4uint\n"
					"%gl_SubGroupGtMaskARB = OpVariable %_ptr_Input_v4uint Input\n"
					"%bool = OpTypeBool\n"
					"%_ptr_Function_bool = OpTypePointer Function %bool\n"
					"%true = OpConstantTrue %bool\n"
					"%gl_SubGroupSizeARB = OpVariable %_ptr_Input_uint Input\n"
					"%uint_32 = OpConstant %uint 32\n"
					"%gl_SubGroupInvocationARB = OpVariable %_ptr_Input_uint Input\n"
					"%false = OpConstantFalse %bool\n"
					"%int = OpTypeInt 32 1\n"
					"%int_1 = OpConstant %int 1\n"
					"%int_15 = OpConstant %int 15\n"
					"%int_2 = OpConstant %int 2\n"
					"%_runtimearr_uint = OpTypeRuntimeArray %uint\n"
					"%Buffer1 = OpTypeStruct %_runtimearr_uint\n"
					"%_ptr_Uniform_Buffer1 = OpTypePointer Uniform %Buffer1\n"
					"%_ = OpVariable %_ptr_Uniform_Buffer1 Uniform\n"
					"%int_0 = OpConstant %int 0\n"
					"%_ptr_Uniform_uint = OpTypePointer Uniform %uint\n"
					"%main = OpFunction %void None %27\n"
					"%57 = OpLabel\n"
					"%globalSize = OpVariable %_ptr_Function_v3uint Function\n"
					"%offset = OpVariable %_ptr_Function_uint Function\n"
					"%bitmask = OpVariable %_ptr_Function__arr_uint_uint_4 Function\n"
					"%temp = OpVariable %_ptr_Function_bool Function\n"
					"%i = OpVariable %_ptr_Function_uint Function\n"
					"%elementIndex = OpVariable %_ptr_Function_uint Function\n"
					"%bitPosition = OpVariable %_ptr_Function_uint Function\n"
					"%mask = OpVariable %_ptr_Function_uint Function\n"
					"%element = OpVariable %_ptr_Function_uint Function\n"
					"%tempResult = OpVariable %_ptr_Function_uint Function\n"
					"%tempRes = OpVariable %_ptr_Function_uint Function\n"
					"%58 = OpLoad %v3uint %gl_NumWorkGroups\n"
					"%59 = OpIMul %v3uint %58 %gl_WorkGroupSize\n"
					"OpStore %globalSize %59\n"
					"%60 = OpAccessChain %_ptr_Function_uint %globalSize %uint_0\n"
					"%61 = OpLoad %uint %60\n"
					"%62 = OpAccessChain %_ptr_Function_uint %globalSize %uint_1\n"
					"%63 = OpLoad %uint %62\n"
					"%64 = OpAccessChain %_ptr_Input_uint %gl_GlobalInvocationID %uint_2\n"
					"%65 = OpLoad %uint %64\n"
					"%66 = OpIMul %uint %63 %65\n"
					"%67 = OpAccessChain %_ptr_Input_uint %gl_GlobalInvocationID %uint_1\n"
					"%68 = OpLoad %uint %67\n"
					"%69 = OpIAdd %uint %66 %68\n"
					"%70 = OpIMul %uint %61 %69\n"
					"%71 = OpAccessChain %_ptr_Input_uint %gl_GlobalInvocationID %uint_0\n"
					"%72 = OpLoad %uint %71\n"
					"%73 = OpIAdd %uint %70 %72\n"
					"OpStore %offset %73\n"
					"%74 = OpLoad %v4uint %gl_SubGroupGtMaskARB\n"
					"%75 = OpCompositeExtract %uint %74 0\n"
					"%76 = OpCompositeExtract %uint %74 1\n"
					"%77 = OpCompositeExtract %uint %74 2\n"
					"%78 = OpCompositeExtract %uint %74 3\n"
					"%79 = OpCompositeConstruct %_arr_uint_uint_4 %75 %76 %77 %78\n"
					"OpStore %bitmask %79\n"
					"OpStore %temp %true\n"
					"OpStore %i %uint_0\n"
					"OpBranch %80\n"
					"%80 = OpLabel\n"
					"OpLoopMerge %81 %82 None\n"
					"OpBranch %83\n"
					"%83 = OpLabel\n"
					"%84 = OpLoad %uint %i\n"
					"%85 = OpLoad %uint %gl_SubGroupSizeARB\n"
					"%86 = OpULessThan %bool %84 %85\n"
					"OpBranchConditional %86 %87 %81\n"
					"%87 = OpLabel\n"
					"%88 = OpLoad %uint %i\n"
					"%89 = OpUDiv %uint %88 %uint_32\n"
					"OpStore %elementIndex %89\n"
					"%90 = OpLoad %uint %i\n"
					"%91 = OpUMod %uint %90 %uint_32\n"
					"OpStore %bitPosition %91\n"
					"%92 = OpLoad %uint %bitPosition\n"
					"%93 = OpShiftLeftLogical %uint %uint_1 %92\n"
					"OpStore %mask %93\n"
					"%94 = OpLoad %uint %elementIndex\n"
					"%95 = OpAccessChain %_ptr_Function_uint %bitmask %94\n"
					"%96 = OpLoad %uint %95\n"
					"OpStore %element %96\n"
					"%97 = OpLoad %uint %i\n"
					"%98 = OpLoad %uint %gl_SubGroupInvocationARB\n"
					"%99 = OpUGreaterThan %bool %97 %98\n"
					"OpSelectionMerge %100 None\n"
					"OpBranchConditional %99 %101 %100\n"
					"%101 = OpLabel\n"
					"%102 = OpLoad %uint %element\n"
					"%103 = OpLoad %uint %mask\n"
					"%104 = OpBitwiseAnd %uint %102 %103\n"
					"%105 = OpIEqual %bool %104 %uint_0\n"
					"OpBranch %100\n"
					"%100 = OpLabel\n"
					"%106 = OpPhi %bool %99 %87 %105 %101\n"
					"OpSelectionMerge %107 None\n"
					"OpBranchConditional %106 %108 %107\n"
					"%108 = OpLabel\n"
					"OpStore %temp %false\n"
					"OpBranch %107\n"
					"%107 = OpLabel\n"
					"%109 = OpLoad %uint %i\n"
					"%110 = OpLoad %uint %gl_SubGroupInvocationARB\n"
					"%111 = OpULessThanEqual %bool %109 %110\n"
					"OpSelectionMerge %112 None\n"
					"OpBranchConditional %111 %113 %112\n"
					"%113 = OpLabel\n"
					"%114 = OpLoad %uint %element\n"
					"%115 = OpLoad %uint %mask\n"
					"%116 = OpBitwiseAnd %uint %114 %115\n"
					"%117 = OpINotEqual %bool %116 %uint_0\n"
					"OpBranch %112\n"
					"%112 = OpLabel\n"
					"%118 = OpPhi %bool %111 %107 %117 %113\n"
					"OpSelectionMerge %119 None\n"
					"OpBranchConditional %118 %120 %119\n"
					"%120 = OpLabel\n"
					"OpStore %temp %false\n"
					"OpBranch %119\n"
					"%119 = OpLabel\n"
					"OpBranch %82\n"
					"%82 = OpLabel\n"
					"%121 = OpLoad %uint %i\n"
					"%122 = OpIAdd %uint %121 %int_1\n"
					"OpStore %i %122\n"
					"OpBranch %80\n"
					"%81 = OpLabel\n"
					"%123 = OpLoad %bool %temp\n"
					"%124 = OpSelect %int %123 %int_15 %int_2\n"
					"%125 = OpBitcast %uint %124\n"
					"OpStore %tempResult %125\n"
					"%126 = OpLoad %uint %tempResult\n"
					"OpStore %tempRes %126\n"
					"%127 = OpLoad %uint %offset\n"
					"%128 = OpLoad %uint %tempRes\n"
					"%129 = OpAccessChain %_ptr_Uniform_uint %_ %int_0 %127\n"
					"OpStore %129 %128\n"
					"OpReturn\n"
					"OpFunctionEnd\n";
				break;
			case MASKTYPE_LE:
				compute +=
					"; SPIR-V\n"
					"; Version: 1.6\n"
					"; Generator: Khronos SPIR-V Tools Assembler; 0\n"
					"; Bound: 130\n"
					"; Schema: 0\n"
					"OpCapability Shader\n"
					"OpCapability Int64\n"
					"OpCapability SubgroupBallotKHR\n"
					"OpExtension \"SPV_KHR_shader_ballot\"\n"
					"%1 = OpExtInstImport \"GLSL.std.450\"\n"
					"OpMemoryModel Logical GLSL450\n"
					"OpEntryPoint GLCompute %main \"main\" %gl_NumWorkGroups %gl_GlobalInvocationID %gl_SubGroupLeMaskARB %gl_SubGroupSizeARB %gl_SubGroupInvocationARB\n"
					"OpExecutionMode %main LocalSize 1 1 1\n"
					"OpSource GLSL 450\n"
					"OpSourceExtension \"GL_ARB_gpu_shader_int64\"\n"
					"OpSourceExtension \"GL_ARB_shader_ballot\"\n"
					"OpName %main \"main\"\n"
					"OpName %globalSize \"globalSize\"\n"
					"OpName %gl_NumWorkGroups \"gl_NumWorkGroups\"\n"
					"OpName %offset \"offset\"\n"
					"OpName %gl_GlobalInvocationID \"gl_GlobalInvocationID\"\n"
					"OpName %bitmask \"bitmask\"\n"
					"OpName %gl_SubGroupLeMaskARB \"gl_SubGroupLeMaskARB\"\n"
					"OpName %temp \"temp\"\n"
					"OpName %i \"i\"\n"
					"OpName %gl_SubGroupSizeARB \"gl_SubGroupSizeARB\"\n"
					"OpName %elementIndex \"elementIndex\"\n"
					"OpName %bitPosition \"bitPosition\"\n"
					"OpName %mask \"mask\"\n"
					"OpName %element \"element\"\n"
					"OpName %gl_SubGroupInvocationARB \"gl_SubGroupInvocationARB\"\n"
					"OpName %tempResult \"tempResult\"\n"
					"OpName %tempRes \"tempRes\"\n"
					"OpName %Buffer1 \"Buffer1\"\n"
					"OpMemberName %Buffer1 0 \"result\"\n"
					"OpName %_ \"\"\n"
					"OpDecorate %gl_NumWorkGroups BuiltIn NumWorkgroups\n"
					"OpDecorate %21 SpecId 0\n"
					"OpDecorate %22 SpecId 1\n"
					"OpDecorate %23 SpecId 2\n"
					"OpDecorate %gl_WorkGroupSize BuiltIn WorkgroupSize\n"
					"OpDecorate %gl_GlobalInvocationID BuiltIn GlobalInvocationId\n"
					"OpDecorate %gl_SubGroupLeMaskARB BuiltIn SubgroupLeMask\n"
					"OpDecorate %gl_SubGroupSizeARB BuiltIn SubgroupSize\n"
					"OpDecorate %gl_SubGroupInvocationARB BuiltIn SubgroupLocalInvocationId\n"
					"OpDecorate %_runtimearr_uint ArrayStride 4\n"
					"OpMemberDecorate %Buffer1 0 Offset 0\n"
					"OpDecorate %Buffer1 BufferBlock\n"
					"OpDecorate %_ DescriptorSet 0\n"
					"OpDecorate %_ Binding 0\n"
					"%void = OpTypeVoid\n"
					"%27 = OpTypeFunction %void\n"
					"%uint = OpTypeInt 32 0\n"
					"%v3uint = OpTypeVector %uint 3\n"
					"%_ptr_Function_v3uint = OpTypePointer Function %v3uint\n"
					"%_ptr_Input_v3uint = OpTypePointer Input %v3uint\n"
					"%gl_NumWorkGroups = OpVariable %_ptr_Input_v3uint Input\n"
					"%21 = OpSpecConstant %uint 1\n"
					"%22 = OpSpecConstant %uint 1\n"
					"%23 = OpSpecConstant %uint 1\n"
					"%gl_WorkGroupSize = OpSpecConstantComposite %v3uint %21 %22 %23\n"
					"%_ptr_Function_uint = OpTypePointer Function %uint\n"
					"%uint_0 = OpConstant %uint 0\n"
					"%uint_1 = OpConstant %uint 1\n"
					"%gl_GlobalInvocationID = OpVariable %_ptr_Input_v3uint Input\n"
					"%uint_2 = OpConstant %uint 2\n"
					"%_ptr_Input_uint = OpTypePointer Input %uint\n"
					"%v4uint = OpTypeVector %uint 4\n"
					"%uint_4 = OpConstant %uint 4\n"
					"%_arr_uint_uint_4 = OpTypeArray %uint %uint_4\n"
					"%_ptr_Function_v4uint = OpTypePointer Function %v4uint\n"
					"%_ptr_Function__arr_uint_uint_4 = OpTypePointer Function %_arr_uint_uint_4\n"
					"%ulong = OpTypeInt 64 0\n"
					"%_ptr_Input_ulong = OpTypePointer Input %ulong\n"
					"%_ptr_Input_v4uint = OpTypePointer Input %v4uint\n"
					"%gl_SubGroupLeMaskARB = OpVariable %_ptr_Input_v4uint Input\n"
					"%bool = OpTypeBool\n"
					"%_ptr_Function_bool = OpTypePointer Function %bool\n"
					"%true = OpConstantTrue %bool\n"
					"%gl_SubGroupSizeARB = OpVariable %_ptr_Input_uint Input\n"
					"%uint_32 = OpConstant %uint 32\n"
					"%gl_SubGroupInvocationARB = OpVariable %_ptr_Input_uint Input\n"
					"%false = OpConstantFalse %bool\n"
					"%int = OpTypeInt 32 1\n"
					"%int_1 = OpConstant %int 1\n"
					"%int_15 = OpConstant %int 15\n"
					"%int_2 = OpConstant %int 2\n"
					"%_runtimearr_uint = OpTypeRuntimeArray %uint\n"
					"%Buffer1 = OpTypeStruct %_runtimearr_uint\n"
					"%_ptr_Uniform_Buffer1 = OpTypePointer Uniform %Buffer1\n"
					"%_ = OpVariable %_ptr_Uniform_Buffer1 Uniform\n"
					"%int_0 = OpConstant %int 0\n"
					"%_ptr_Uniform_uint = OpTypePointer Uniform %uint\n"
					"%main = OpFunction %void None %27\n"
					"%57 = OpLabel\n"
					"%globalSize = OpVariable %_ptr_Function_v3uint Function\n"
					"%offset = OpVariable %_ptr_Function_uint Function\n"
					"%bitmask = OpVariable %_ptr_Function__arr_uint_uint_4 Function\n"
					"%temp = OpVariable %_ptr_Function_bool Function\n"
					"%i = OpVariable %_ptr_Function_uint Function\n"
					"%elementIndex = OpVariable %_ptr_Function_uint Function\n"
					"%bitPosition = OpVariable %_ptr_Function_uint Function\n"
					"%mask = OpVariable %_ptr_Function_uint Function\n"
					"%element = OpVariable %_ptr_Function_uint Function\n"
					"%tempResult = OpVariable %_ptr_Function_uint Function\n"
					"%tempRes = OpVariable %_ptr_Function_uint Function\n"
					"%58 = OpLoad %v3uint %gl_NumWorkGroups\n"
					"%59 = OpIMul %v3uint %58 %gl_WorkGroupSize\n"
					"OpStore %globalSize %59\n"
					"%60 = OpAccessChain %_ptr_Function_uint %globalSize %uint_0\n"
					"%61 = OpLoad %uint %60\n"
					"%62 = OpAccessChain %_ptr_Function_uint %globalSize %uint_1\n"
					"%63 = OpLoad %uint %62\n"
					"%64 = OpAccessChain %_ptr_Input_uint %gl_GlobalInvocationID %uint_2\n"
					"%65 = OpLoad %uint %64\n"
					"%66 = OpIMul %uint %63 %65\n"
					"%67 = OpAccessChain %_ptr_Input_uint %gl_GlobalInvocationID %uint_1\n"
					"%68 = OpLoad %uint %67\n"
					"%69 = OpIAdd %uint %66 %68\n"
					"%70 = OpIMul %uint %61 %69\n"
					"%71 = OpAccessChain %_ptr_Input_uint %gl_GlobalInvocationID %uint_0\n"
					"%72 = OpLoad %uint %71\n"
					"%73 = OpIAdd %uint %70 %72\n"
					"OpStore %offset %73\n"
					"%74 = OpLoad %v4uint %gl_SubGroupLeMaskARB\n"
					"%75 = OpCompositeExtract %uint %74 0\n"
					"%76 = OpCompositeExtract %uint %74 1\n"
					"%77 = OpCompositeExtract %uint %74 2\n"
					"%78 = OpCompositeExtract %uint %74 3\n"
					"%79 = OpCompositeConstruct %_arr_uint_uint_4 %75 %76 %77 %78\n"
					"OpStore %bitmask %79\n"
					"OpStore %temp %true\n"
					"OpStore %i %uint_0\n"
					"OpBranch %80\n"
					"%80 = OpLabel\n"
					"OpLoopMerge %81 %82 None\n"
					"OpBranch %83\n"
					"%83 = OpLabel\n"
					"%84 = OpLoad %uint %i\n"
					"%85 = OpLoad %uint %gl_SubGroupSizeARB\n"
					"%86 = OpULessThan %bool %84 %85\n"
					"OpBranchConditional %86 %87 %81\n"
					"%87 = OpLabel\n"
					"%88 = OpLoad %uint %i\n"
					"%89 = OpUDiv %uint %88 %uint_32\n"
					"OpStore %elementIndex %89\n"
					"%90 = OpLoad %uint %i\n"
					"%91 = OpUMod %uint %90 %uint_32\n"
					"OpStore %bitPosition %91\n"
					"%92 = OpLoad %uint %bitPosition\n"
					"%93 = OpShiftLeftLogical %uint %uint_1 %92\n"
					"OpStore %mask %93\n"
					"%94 = OpLoad %uint %elementIndex\n"
					"%95 = OpAccessChain %_ptr_Function_uint %bitmask %94\n"
					"%96 = OpLoad %uint %95\n"
					"OpStore %element %96\n"
					"%97 = OpLoad %uint %i\n"
					"%98 = OpLoad %uint %gl_SubGroupInvocationARB\n"
					"%99 = OpULessThanEqual %bool %97 %98\n"
					"OpSelectionMerge %100 None\n"
					"OpBranchConditional %99 %101 %100\n"
					"%101 = OpLabel\n"
					"%102 = OpLoad %uint %element\n"
					"%103 = OpLoad %uint %mask\n"
					"%104 = OpBitwiseAnd %uint %102 %103\n"
					"%105 = OpIEqual %bool %104 %uint_0\n"
					"OpBranch %100\n"
					"%100 = OpLabel\n"
					"%106 = OpPhi %bool %99 %87 %105 %101\n"
					"OpSelectionMerge %107 None\n"
					"OpBranchConditional %106 %108 %107\n"
					"%108 = OpLabel\n"
					"OpStore %temp %false\n"
					"OpBranch %107\n"
					"%107 = OpLabel\n"
					"%109 = OpLoad %uint %i\n"
					"%110 = OpLoad %uint %gl_SubGroupInvocationARB\n"
					"%111 = OpUGreaterThan %bool %109 %110\n"
					"OpSelectionMerge %112 None\n"
					"OpBranchConditional %111 %113 %112\n"
					"%113 = OpLabel\n"
					"%114 = OpLoad %uint %element\n"
					"%115 = OpLoad %uint %mask\n"
					"%116 = OpBitwiseAnd %uint %114 %115\n"
					"%117 = OpINotEqual %bool %116 %uint_0\n"
					"OpBranch %112\n"
					"%112 = OpLabel\n"
					"%118 = OpPhi %bool %111 %107 %117 %113\n"
					"OpSelectionMerge %119 None\n"
					"OpBranchConditional %118 %120 %119\n"
					"%120 = OpLabel\n"
					"OpStore %temp %false\n"
					"OpBranch %119\n"
					"%119 = OpLabel\n"
					"OpBranch %82\n"
					"%82 = OpLabel\n"
					"%121 = OpLoad %uint %i\n"
					"%122 = OpIAdd %uint %121 %int_1\n"
					"OpStore %i %122\n"
					"OpBranch %80\n"
					"%81 = OpLabel\n"
					"%123 = OpLoad %bool %temp\n"
					"%124 = OpSelect %int %123 %int_15 %int_2\n"
					"%125 = OpBitcast %uint %124\n"
					"OpStore %tempResult %125\n"
					"%126 = OpLoad %uint %tempResult\n"
					"OpStore %tempRes %126\n"
					"%127 = OpLoad %uint %offset\n"
					"%128 = OpLoad %uint %tempRes\n"
					"%129 = OpAccessChain %_ptr_Uniform_uint %_ %int_0 %127\n"
					"OpStore %129 %128\n"
					"OpReturn\n"
					"OpFunctionEnd\n";
				break;
			case MASKTYPE_LT:
				compute +=
					"; SPIR-V\n"
					"; Version: 1.6\n"
					"; Generator: Khronos SPIR-V Tools Assembler; 0\n"
					"; Bound: 130\n"
					"; Schema: 0\n"
					"OpCapability Shader\n"
					"OpCapability Int64\n"
					"OpCapability SubgroupBallotKHR\n"
					"OpExtension \"SPV_KHR_shader_ballot\"\n"
					"%1 = OpExtInstImport \"GLSL.std.450\"\n"
					"OpMemoryModel Logical GLSL450\n"
					"OpEntryPoint GLCompute %main \"main\" %gl_NumWorkGroups %gl_GlobalInvocationID %gl_SubGroupLtMaskARB %gl_SubGroupSizeARB %gl_SubGroupInvocationARB\n"
					"OpExecutionMode %main LocalSize 1 1 1\n"
					"OpSource GLSL 450\n"
					"OpSourceExtension \"GL_ARB_gpu_shader_int64\"\n"
					"OpSourceExtension \"GL_ARB_shader_ballot\"\n"
					"OpName %main \"main\"\n"
					"OpName %globalSize \"globalSize\"\n"
					"OpName %gl_NumWorkGroups \"gl_NumWorkGroups\"\n"
					"OpName %offset \"offset\"\n"
					"OpName %gl_GlobalInvocationID \"gl_GlobalInvocationID\"\n"
					"OpName %bitmask \"bitmask\"\n"
					"OpName %gl_SubGroupLtMaskARB \"gl_SubGroupLtMaskARB\"\n"
					"OpName %temp \"temp\"\n"
					"OpName %i \"i\"\n"
					"OpName %gl_SubGroupSizeARB \"gl_SubGroupSizeARB\"\n"
					"OpName %elementIndex \"elementIndex\"\n"
					"OpName %bitPosition \"bitPosition\"\n"
					"OpName %mask \"mask\"\n"
					"OpName %element \"element\"\n"
					"OpName %gl_SubGroupInvocationARB \"gl_SubGroupInvocationARB\"\n"
					"OpName %tempResult \"tempResult\"\n"
					"OpName %tempRes \"tempRes\"\n"
					"OpName %Buffer1 \"Buffer1\"\n"
					"OpMemberName %Buffer1 0 \"result\"\n"
					"OpName %_ \"\"\n"
					"OpDecorate %gl_NumWorkGroups BuiltIn NumWorkgroups\n"
					"OpDecorate %21 SpecId 0\n"
					"OpDecorate %22 SpecId 1\n"
					"OpDecorate %23 SpecId 2\n"
					"OpDecorate %gl_WorkGroupSize BuiltIn WorkgroupSize\n"
					"OpDecorate %gl_GlobalInvocationID BuiltIn GlobalInvocationId\n"
					"OpDecorate %gl_SubGroupLtMaskARB BuiltIn SubgroupLtMask\n"
					"OpDecorate %gl_SubGroupSizeARB BuiltIn SubgroupSize\n"
					"OpDecorate %gl_SubGroupInvocationARB BuiltIn SubgroupLocalInvocationId\n"
					"OpDecorate %_runtimearr_uint ArrayStride 4\n"
					"OpMemberDecorate %Buffer1 0 Offset 0\n"
					"OpDecorate %Buffer1 BufferBlock\n"
					"OpDecorate %_ DescriptorSet 0\n"
					"OpDecorate %_ Binding 0\n"
					"%void = OpTypeVoid\n"
					"%27 = OpTypeFunction %void\n"
					"%uint = OpTypeInt 32 0\n"
					"%v3uint = OpTypeVector %uint 3\n"
					"%_ptr_Function_v3uint = OpTypePointer Function %v3uint\n"
					"%_ptr_Input_v3uint = OpTypePointer Input %v3uint\n"
					"%gl_NumWorkGroups = OpVariable %_ptr_Input_v3uint Input\n"
					"%21 = OpSpecConstant %uint 1\n"
					"%22 = OpSpecConstant %uint 1\n"
					"%23 = OpSpecConstant %uint 1\n"
					"%gl_WorkGroupSize = OpSpecConstantComposite %v3uint %21 %22 %23\n"
					"%_ptr_Function_uint = OpTypePointer Function %uint\n"
					"%uint_0 = OpConstant %uint 0\n"
					"%uint_1 = OpConstant %uint 1\n"
					"%gl_GlobalInvocationID = OpVariable %_ptr_Input_v3uint Input\n"
					"%uint_2 = OpConstant %uint 2\n"
					"%_ptr_Input_uint = OpTypePointer Input %uint\n"
					"%v4uint = OpTypeVector %uint 4\n"
					"%uint_4 = OpConstant %uint 4\n"
					"%_arr_uint_uint_4 = OpTypeArray %uint %uint_4\n"
					"%_ptr_Function_v4uint = OpTypePointer Function %v4uint\n"
					"%_ptr_Function__arr_uint_uint_4 = OpTypePointer Function %_arr_uint_uint_4\n"
					"%ulong = OpTypeInt 64 0\n"
					"%_ptr_Input_ulong = OpTypePointer Input %ulong\n"
					"%_ptr_Input_v4uint = OpTypePointer Input %v4uint\n"
					"%gl_SubGroupLtMaskARB = OpVariable %_ptr_Input_v4uint Input\n"
					"%bool = OpTypeBool\n"
					"%_ptr_Function_bool = OpTypePointer Function %bool\n"
					"%true = OpConstantTrue %bool\n"
					"%gl_SubGroupSizeARB = OpVariable %_ptr_Input_uint Input\n"
					"%uint_32 = OpConstant %uint 32\n"
					"%gl_SubGroupInvocationARB = OpVariable %_ptr_Input_uint Input\n"
					"%false = OpConstantFalse %bool\n"
					"%int = OpTypeInt 32 1\n"
					"%int_1 = OpConstant %int 1\n"
					"%int_15 = OpConstant %int 15\n"
					"%int_2 = OpConstant %int 2\n"
					"%_runtimearr_uint = OpTypeRuntimeArray %uint\n"
					"%Buffer1 = OpTypeStruct %_runtimearr_uint\n"
					"%_ptr_Uniform_Buffer1 = OpTypePointer Uniform %Buffer1\n"
					"%_ = OpVariable %_ptr_Uniform_Buffer1 Uniform\n"
					"%int_0 = OpConstant %int 0\n"
					"%_ptr_Uniform_uint = OpTypePointer Uniform %uint\n"
					"%main = OpFunction %void None %27\n"
					"%57 = OpLabel\n"
					"%globalSize = OpVariable %_ptr_Function_v3uint Function\n"
					"%offset = OpVariable %_ptr_Function_uint Function\n"
					"%bitmask = OpVariable %_ptr_Function__arr_uint_uint_4 Function\n"
					"%temp = OpVariable %_ptr_Function_bool Function\n"
					"%i = OpVariable %_ptr_Function_uint Function\n"
					"%elementIndex = OpVariable %_ptr_Function_uint Function\n"
					"%bitPosition = OpVariable %_ptr_Function_uint Function\n"
					"%mask = OpVariable %_ptr_Function_uint Function\n"
					"%element = OpVariable %_ptr_Function_uint Function\n"
					"%tempResult = OpVariable %_ptr_Function_uint Function\n"
					"%tempRes = OpVariable %_ptr_Function_uint Function\n"
					"%58 = OpLoad %v3uint %gl_NumWorkGroups\n"
					"%59 = OpIMul %v3uint %58 %gl_WorkGroupSize\n"
					"OpStore %globalSize %59\n"
					"%60 = OpAccessChain %_ptr_Function_uint %globalSize %uint_0\n"
					"%61 = OpLoad %uint %60\n"
					"%62 = OpAccessChain %_ptr_Function_uint %globalSize %uint_1\n"
					"%63 = OpLoad %uint %62\n"
					"%64 = OpAccessChain %_ptr_Input_uint %gl_GlobalInvocationID %uint_2\n"
					"%65 = OpLoad %uint %64\n"
					"%66 = OpIMul %uint %63 %65\n"
					"%67 = OpAccessChain %_ptr_Input_uint %gl_GlobalInvocationID %uint_1\n"
					"%68 = OpLoad %uint %67\n"
					"%69 = OpIAdd %uint %66 %68\n"
					"%70 = OpIMul %uint %61 %69\n"
					"%71 = OpAccessChain %_ptr_Input_uint %gl_GlobalInvocationID %uint_0\n"
					"%72 = OpLoad %uint %71\n"
					"%73 = OpIAdd %uint %70 %72\n"
					"OpStore %offset %73\n"
					"%74 = OpLoad %v4uint %gl_SubGroupLtMaskARB\n"
					"%75 = OpCompositeExtract %uint %74 0\n"
					"%76 = OpCompositeExtract %uint %74 1\n"
					"%77 = OpCompositeExtract %uint %74 2\n"
					"%78 = OpCompositeExtract %uint %74 3\n"
					"%79 = OpCompositeConstruct %_arr_uint_uint_4 %75 %76 %77 %78\n"
					"OpStore %bitmask %79\n"
					"OpStore %temp %true\n"
					"OpStore %i %uint_0\n"
					"OpBranch %80\n"
					"%80 = OpLabel\n"
					"OpLoopMerge %81 %82 None\n"
					"OpBranch %83\n"
					"%83 = OpLabel\n"
					"%84 = OpLoad %uint %i\n"
					"%85 = OpLoad %uint %gl_SubGroupSizeARB\n"
					"%86 = OpULessThan %bool %84 %85\n"
					"OpBranchConditional %86 %87 %81\n"
					"%87 = OpLabel\n"
					"%88 = OpLoad %uint %i\n"
					"%89 = OpUDiv %uint %88 %uint_32\n"
					"OpStore %elementIndex %89\n"
					"%90 = OpLoad %uint %i\n"
					"%91 = OpUMod %uint %90 %uint_32\n"
					"OpStore %bitPosition %91\n"
					"%92 = OpLoad %uint %bitPosition\n"
					"%93 = OpShiftLeftLogical %uint %uint_1 %92\n"
					"OpStore %mask %93\n"
					"%94 = OpLoad %uint %elementIndex\n"
					"%95 = OpAccessChain %_ptr_Function_uint %bitmask %94\n"
					"%96 = OpLoad %uint %95\n"
					"OpStore %element %96\n"
					"%97 = OpLoad %uint %i\n"
					"%98 = OpLoad %uint %gl_SubGroupInvocationARB\n"
					"%99 = OpULessThan %bool %97 %98\n"
					"OpSelectionMerge %100 None\n"
					"OpBranchConditional %99 %101 %100\n"
					"%101 = OpLabel\n"
					"%102 = OpLoad %uint %element\n"
					"%103 = OpLoad %uint %mask\n"
					"%104 = OpBitwiseAnd %uint %102 %103\n"
					"%105 = OpIEqual %bool %104 %uint_0\n"
					"OpBranch %100\n"
					"%100 = OpLabel\n"
					"%106 = OpPhi %bool %99 %87 %105 %101\n"
					"OpSelectionMerge %107 None\n"
					"OpBranchConditional %106 %108 %107\n"
					"%108 = OpLabel\n"
					"OpStore %temp %false\n"
					"OpBranch %107\n"
					"%107 = OpLabel\n"
					"%109 = OpLoad %uint %i\n"
					"%110 = OpLoad %uint %gl_SubGroupInvocationARB\n"
					"%111 = OpUGreaterThanEqual %bool %109 %110\n"
					"OpSelectionMerge %112 None\n"
					"OpBranchConditional %111 %113 %112\n"
					"%113 = OpLabel\n"
					"%114 = OpLoad %uint %element\n"
					"%115 = OpLoad %uint %mask\n"
					"%116 = OpBitwiseAnd %uint %114 %115\n"
					"%117 = OpINotEqual %bool %116 %uint_0\n"
					"OpBranch %112\n"
					"%112 = OpLabel\n"
					"%118 = OpPhi %bool %111 %107 %117 %113\n"
					"OpSelectionMerge %119 None\n"
					"OpBranchConditional %118 %120 %119\n"
					"%120 = OpLabel\n"
					"OpStore %temp %false\n"
					"OpBranch %119\n"
					"%119 = OpLabel\n"
					"OpBranch %82\n"
					"%82 = OpLabel\n"
					"%121 = OpLoad %uint %i\n"
					"%122 = OpIAdd %uint %121 %int_1\n"
					"OpStore %i %122\n"
					"OpBranch %80\n"
					"%81 = OpLabel\n"
					"%123 = OpLoad %bool %temp\n"
					"%124 = OpSelect %int %123 %int_15 %int_2\n"
					"%125 = OpBitcast %uint %124\n"
					"OpStore %tempResult %125\n"
					"%126 = OpLoad %uint %tempResult\n"
					"OpStore %tempRes %126\n"
					"%127 = OpLoad %uint %offset\n"
					"%128 = OpLoad %uint %tempRes\n"
					"%129 = OpAccessChain %_ptr_Uniform_uint %_ %int_0 %127\n"
					"OpStore %129 %128\n"
					"OpReturn\n"
					"OpFunctionEnd\n";
				break;
			default:
				TCU_THROW(InternalError, "Unknown mask type");
		}
		programCollection.spirvAsmSources.add("comp") << compute << buildOptionsSpr;
	}
	else
	{
		subgroups::initStdPrograms(programCollection, buildOptions, caseDef.shaderStage, VK_FORMAT_R32_UINT, pointSizeSupport, extHeader, testSrc, "", headDeclarations);
	}
}

void supportedCheck (Context& context, CaseDefinition caseDef)
{
	if (!subgroups::isSubgroupSupported(context))
		TCU_THROW(NotSupportedError, "Subgroup operations are not supported");

	if (!context.requireDeviceFunctionality("VK_EXT_shader_subgroup_ballot"))
	{
		TCU_THROW(NotSupportedError, "Device does not support VK_EXT_shader_subgroup_ballot extension");
	}

	if (!subgroups::isInt64SupportedForDevice(context))
		TCU_THROW(NotSupportedError, "Int64 is not supported");

	if (caseDef.requiredSubgroupSize)
	{
		context.requireDeviceFunctionality("VK_EXT_subgroup_size_control");

#ifndef CTS_USES_VULKANSC
		const VkPhysicalDeviceSubgroupSizeControlFeatures&		subgroupSizeControlFeatures		= context.getSubgroupSizeControlFeatures();
		const VkPhysicalDeviceSubgroupSizeControlProperties&	subgroupSizeControlProperties	= context.getSubgroupSizeControlProperties();
#else
		const VkPhysicalDeviceSubgroupSizeControlFeaturesEXT&		subgroupSizeControlFeatures	= context.getSubgroupSizeControlFeaturesEXT();
		const VkPhysicalDeviceSubgroupSizeControlPropertiesEXT&	subgroupSizeControlProperties	= context.getSubgroupSizeControlPropertiesEXT();
#endif // CTS_USES_VULKANSC

		if (subgroupSizeControlFeatures.subgroupSizeControl == DE_FALSE)
			TCU_THROW(NotSupportedError, "Device does not support varying subgroup sizes nor required subgroup size");

		if (subgroupSizeControlFeatures.computeFullSubgroups == DE_FALSE)
			TCU_THROW(NotSupportedError, "Device does not support full subgroups in compute shaders");

		if ((subgroupSizeControlProperties.requiredSubgroupSizeStages & caseDef.shaderStage) != caseDef.shaderStage)
			TCU_THROW(NotSupportedError, "Required subgroup size is not supported for shader stage");
	}

	*caseDef.geometryPointSizeSupported = subgroups::isTessellationAndGeometryPointSizeSupported(context);

#ifndef CTS_USES_VULKANSC
	if (isAllRayTracingStages(caseDef.shaderStage))
	{
		context.requireDeviceFunctionality("VK_KHR_ray_tracing_pipeline");
	}
	else if (isAllMeshShadingStages(caseDef.shaderStage))
	{
		context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_VERTEX_PIPELINE_STORES_AND_ATOMICS);
		context.requireDeviceFunctionality("VK_EXT_mesh_shader");

		if ((caseDef.shaderStage & VK_SHADER_STAGE_TASK_BIT_EXT) != 0u)
		{
			const auto& features = context.getMeshShaderFeaturesEXT();
			if (!features.taskShader)
				TCU_THROW(NotSupportedError, "Task shaders not supported");
		}
	}
#endif // CTS_USES_VULKANSC

	subgroups::supportedCheckShader(context, caseDef.shaderStage);
}

TestStatus noSSBOtest (Context& context, const CaseDefinition caseDef)
{
	switch (caseDef.shaderStage)
	{
		case VK_SHADER_STAGE_VERTEX_BIT:					return subgroups::makeVertexFrameBufferTest(context, VK_FORMAT_R32_UINT, DE_NULL, 0, DE_NULL, checkVertexPipelineStages);
		case VK_SHADER_STAGE_GEOMETRY_BIT:					return subgroups::makeGeometryFrameBufferTest(context, VK_FORMAT_R32_UINT, DE_NULL, 0, DE_NULL, checkVertexPipelineStages);
		case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:		return subgroups::makeTessellationEvaluationFrameBufferTest(context, VK_FORMAT_R32_UINT, DE_NULL, 0, DE_NULL, checkVertexPipelineStages);
		case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT:	return subgroups::makeTessellationEvaluationFrameBufferTest(context, VK_FORMAT_R32_UINT, DE_NULL, 0, DE_NULL, checkVertexPipelineStages);
		default:											TCU_THROW(InternalError, "Unhandled shader stage");
	}
}

TestStatus test (Context& context, const CaseDefinition caseDef)
{
	const bool isCompute	= isAllComputeStages(caseDef.shaderStage);
#ifndef CTS_USES_VULKANSC
	const bool isMesh		= isAllMeshShadingStages(caseDef.shaderStage);
#else
	const bool isMesh		= false;
#endif // CTS_USES_VULKANSC
	DE_ASSERT(!(isCompute && isMesh));

	if (isCompute || isMesh)
	{
#ifndef CTS_USES_VULKANSC
		const VkPhysicalDeviceSubgroupSizeControlProperties&	subgroupSizeControlProperties	= context.getSubgroupSizeControlProperties();
#else
		const VkPhysicalDeviceSubgroupSizeControlPropertiesEXT&	subgroupSizeControlProperties	= context.getSubgroupSizeControlPropertiesEXT();
#endif // CTS_USES_VULKANSC
		TestLog&												log								= context.getTestContext().getLog();

		if (caseDef.requiredSubgroupSize == DE_FALSE)
		{
			if (isCompute)
				return subgroups::makeComputeTest(context, VK_FORMAT_R32_UINT, DE_NULL, 0, DE_NULL, checkComputeOrMesh);
			else
				return subgroups::makeMeshTest(context, VK_FORMAT_R32_UINT, nullptr, 0, nullptr, checkComputeOrMesh);
		}

		log << TestLog::Message << "Testing required subgroup size range [" <<  subgroupSizeControlProperties.minSubgroupSize << ", "
			<< subgroupSizeControlProperties.maxSubgroupSize << "]" << TestLog::EndMessage;

		// According to the spec, requiredSubgroupSize must be a power-of-two integer.
		for (deUint32 size = subgroupSizeControlProperties.minSubgroupSize; size <= subgroupSizeControlProperties.maxSubgroupSize; size *= 2)
		{
			TestStatus result (QP_TEST_RESULT_INTERNAL_ERROR, "Internal Error");

			if (isCompute)
				result = subgroups::makeComputeTest(context, VK_FORMAT_R32_UINT, DE_NULL, 0u, DE_NULL, checkComputeOrMesh, size);
			else
				result = subgroups::makeMeshTest(context, VK_FORMAT_R32_UINT, nullptr, 0u, nullptr, checkComputeOrMesh, size);

			if (result.getCode() != QP_TEST_RESULT_PASS)
			{
				log << TestLog::Message << "subgroupSize " << size << " failed" << TestLog::EndMessage;
				return result;
			}
		}

		return TestStatus::pass("OK");
	}
	else if (isAllGraphicsStages(caseDef.shaderStage))
	{
		const VkShaderStageFlags	stages	= subgroups::getPossibleGraphicsSubgroupStages(context, caseDef.shaderStage);

		return subgroups::allStages(context, VK_FORMAT_R32_UINT, DE_NULL, 0, DE_NULL, checkVertexPipelineStages, stages);
	}
#ifndef CTS_USES_VULKANSC
	else if (isAllRayTracingStages(caseDef.shaderStage))
	{
		const VkShaderStageFlags	stages	= subgroups::getPossibleRayTracingSubgroupStages(context, caseDef.shaderStage);

		return subgroups::allRayTracingStages(context, VK_FORMAT_R32_UINT, DE_NULL, 0, DE_NULL, checkVertexPipelineStages, stages);
	}
#endif // CTS_USES_VULKANSC
	else
		TCU_THROW(InternalError, "Unknown stage or invalid stage set");
}
}

namespace vkt
{
namespace subgroups
{
TestCaseGroup* createSubgroupsBallotMasksTests (TestContext& testCtx)
{
	de::MovePtr<TestCaseGroup>	group				(new TestCaseGroup(testCtx, "ballot_mask", "VK_EXT_shader_subgroup_ballot mask category tests"));
	de::MovePtr<TestCaseGroup>	groupARB			(new TestCaseGroup(testCtx, "ext_shader_subgroup_ballot", "VK_EXT_shader_subgroup_ballot masks category tests"));
	de::MovePtr<TestCaseGroup>	graphicGroup		(new TestCaseGroup(testCtx, "graphics", "VK_EXT_shader_subgroup_ballot masks category tests: graphics"));
	de::MovePtr<TestCaseGroup>	computeGroup		(new TestCaseGroup(testCtx, "compute", "VK_EXT_shader_subgroup_ballot masks category tests: compute"));
	de::MovePtr<TestCaseGroup>	framebufferGroup	(new TestCaseGroup(testCtx, "framebuffer", "VK_EXT_shader_subgroup_ballot masks category tests: framebuffer"));
#ifndef CTS_USES_VULKANSC
	de::MovePtr<TestCaseGroup>	raytracingGroup		(new TestCaseGroup(testCtx, "ray_tracing", "VK_EXT_shader_subgroup_ballot masks category tests: ray tracing"));
	de::MovePtr<TestCaseGroup>	meshGroup			(new TestCaseGroup(testCtx, "mesh", "VK_EXT_shader_subgroup_ballot masks category tests: mesh shaders"));
#endif // CTS_USES_VULKANSC
	const VkShaderStageFlags	fbStages[]			=
	{
		VK_SHADER_STAGE_VERTEX_BIT,
		VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,
		VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,
		VK_SHADER_STAGE_GEOMETRY_BIT,
	};
#ifndef CTS_USES_VULKANSC
	const VkShaderStageFlags	meshStages[]		=
	{
		VK_SHADER_STAGE_MESH_BIT_EXT,
		VK_SHADER_STAGE_TASK_BIT_EXT,
	};
#endif // CTS_USES_VULKANSC
	const deBool				boolValues[]		=
	{
		DE_FALSE,
		DE_TRUE
	};

	for (int maskTypeIndex = 0; maskTypeIndex < MASKTYPE_LAST; ++maskTypeIndex)
	{
		const MaskType	maskType	= static_cast<MaskType>(maskTypeIndex);
		const string	mask		= de::toLower(getMaskTypeName(maskType));

		for (size_t groupSizeNdx = 0; groupSizeNdx < DE_LENGTH_OF_ARRAY(boolValues); ++groupSizeNdx)
		{
			const deBool			requiredSubgroupSize	= boolValues[groupSizeNdx];
			const string			testName				= mask + (requiredSubgroupSize ? "_requiredsubgroupsize" : "");
			const CaseDefinition	caseDef					=
			{
				maskType,						//  MaskType			maskType;
				VK_SHADER_STAGE_COMPUTE_BIT,	//  VkShaderStageFlags	shaderStage;
				de::SharedPtr<bool>(new bool),	//  de::SharedPtr<bool>	geometryPointSizeSupported;
				requiredSubgroupSize,			//  deBool				requiredSubgroupSize;
			};

			addFunctionCaseWithPrograms(computeGroup.get(), testName,supportedCheck, initPrograms, test, caseDef);
		}

#ifndef CTS_USES_VULKANSC
		for (size_t groupSizeNdx = 0; groupSizeNdx < DE_LENGTH_OF_ARRAY(boolValues); ++groupSizeNdx)
		{
			for (const auto& stage : meshStages)
			{
				const deBool			requiredSubgroupSize	= boolValues[groupSizeNdx];
				const string			testName				= mask + (requiredSubgroupSize ? "_requiredsubgroupsize" : "");
				const CaseDefinition	caseDef					=
				{
					maskType,						//  MaskType			maskType;
					stage,							//  VkShaderStageFlags	shaderStage;
					de::SharedPtr<bool>(new bool),	//  de::SharedPtr<bool>	geometryPointSizeSupported;
					requiredSubgroupSize,			//  deBool				requiredSubgroupSize;
				};

				addFunctionCaseWithPrograms(meshGroup.get(), testName + "_" + getShaderStageName(stage),  supportedCheck, initPrograms, test, caseDef);
			}
		}
#endif // CTS_USES_VULKANSC

		{
			const CaseDefinition	caseDef		=
			{
				maskType,						//  MaskType			maskType;
				VK_SHADER_STAGE_ALL_GRAPHICS,	//  VkShaderStageFlags	shaderStage;
				de::SharedPtr<bool>(new bool),	//  de::SharedPtr<bool>	geometryPointSizeSupported;
				DE_FALSE						//  deBool				requiredSubgroupSize;
			};

			addFunctionCaseWithPrograms(graphicGroup.get(), mask,  supportedCheck, initPrograms, test, caseDef);
		}

#ifndef CTS_USES_VULKANSC
		{
			const CaseDefinition	caseDef		=
			{
				maskType,						//  MaskType			maskType;
				SHADER_STAGE_ALL_RAY_TRACING,	//  VkShaderStageFlags	shaderStage;
				de::SharedPtr<bool>(new bool),	//  de::SharedPtr<bool>	geometryPointSizeSupported;
				DE_FALSE						//  deBool				requiredSubgroupSize;
			};

			addFunctionCaseWithPrograms(raytracingGroup.get(), mask,  supportedCheck, initPrograms, test, caseDef);
		}
#endif // CTS_USES_VULKANSC

		for (int stageIndex = 0; stageIndex < DE_LENGTH_OF_ARRAY(fbStages); ++stageIndex)
		{
			const CaseDefinition	caseDef		=
			{
				maskType,						//  MaskType			maskType;
				fbStages[stageIndex],			//  VkShaderStageFlags	shaderStage;
				de::SharedPtr<bool>(new bool),	//  de::SharedPtr<bool>	geometryPointSizeSupported;
				DE_FALSE						//  deBool				requiredSubgroupSize;
			};
			const string			testName	= mask + "_" + getShaderStageName(caseDef.shaderStage);

			addFunctionCaseWithPrograms(framebufferGroup.get(), testName,supportedCheck, initFrameBufferPrograms, noSSBOtest, caseDef);
		}
	}

	groupARB->addChild(graphicGroup.release());
	groupARB->addChild(computeGroup.release());
	groupARB->addChild(framebufferGroup.release());
#ifndef CTS_USES_VULKANSC
	groupARB->addChild(raytracingGroup.release());
	groupARB->addChild(meshGroup.release());
#endif // CTS_USES_VULKANSC
	group->addChild(groupARB.release());

	return group.release();
}

} // subgroups
} // vkt
