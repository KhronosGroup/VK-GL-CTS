/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2019 The Khronos Group Inc.
 * Copyright (c) 2019 Google Inc.
 * Copyright (c) 2017 Codeplay Software Ltd.
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

#include "vktSubgroupsBuiltinMaskVarTests.hpp"
#include "vktSubgroupsTestsUtils.hpp"

#include <string>
#include <vector>

using namespace tcu;
using namespace std;
using namespace vk;

namespace vkt
{
namespace subgroups
{

static bool checkVertexPipelineStages(const void* internalData, std::vector<const void*> datas,
									  deUint32 width, deUint32)
{
	DE_UNREF(internalData);
	return check(datas, width, 1);
}

static bool checkComputeStage(const void* internalData, std::vector<const void*> datas,
						 const deUint32 numWorkgroups[3], const deUint32 localSize[3],
						 deUint32)
{
	DE_UNREF(internalData);
	return checkCompute(datas, numWorkgroups, localSize, 1);
}

namespace
{
struct CaseDefinition
{
	std::string			varName;
	VkShaderStageFlags	shaderStage;
	de::SharedPtr<bool>	geometryPointSizeSupported;
	deBool				requiredSubgroupSize;
};
}

std::string subgroupComparison (const CaseDefinition& caseDef)
{
	if ("gl_SubgroupEqMask" == caseDef.varName)
	{
		if (VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT == caseDef.shaderStage)
			return "%56 = OpIEqual %11 %53 %55\n";
		else
			return "%38 = OpIEqual %16 %35 %37\n";
	}
	else if ("gl_SubgroupGeMask" == caseDef.varName)
	{
		if (VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT == caseDef.shaderStage)
			return "%56 = OpUGreaterThanEqual %11 %53 %55\n";
		else
			return "%38 = OpUGreaterThanEqual %16 %35 %37\n";
	}
	else if ("gl_SubgroupGtMask" == caseDef.varName)
	{
		if (VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT == caseDef.shaderStage)
			return "%56 = OpUGreaterThan %11 %53 %55\n";
		else
			return "%38 = OpUGreaterThan %16 %35 %37\n";
	}
	else if ("gl_SubgroupLeMask" == caseDef.varName)
	{
		if (VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT == caseDef.shaderStage)
			return "%56 = OpULessThanEqual %11 %53 %55\n";
		else
			return "%38 = OpULessThanEqual %16 %35 %37\n";
	}
	else if ("gl_SubgroupLtMask" == caseDef.varName)
	{
		if (VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT == caseDef.shaderStage)
			return "%56 = OpULessThan %11 %53 %55\n";
		else
			return "%38 = OpULessThan %16 %35 %37\n";
	}
	return "";
}

std::string varSubgroupMask (const CaseDefinition& caseDef)
{
	if ("gl_SubgroupEqMask" == caseDef.varName)
	{
		if (VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT == caseDef.shaderStage)
			return "OpDecorate %40 BuiltIn SubgroupEqMask\n";
		else
			return "OpDecorate %22 BuiltIn SubgroupEqMask\n";
	}
	else if ("gl_SubgroupGeMask" == caseDef.varName)
	{
		if (VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT == caseDef.shaderStage)
			return "OpDecorate %40 BuiltIn SubgroupGeMask\n";
		else
			return "OpDecorate %22 BuiltIn SubgroupGeMask\n";
	}
	else if ("gl_SubgroupGtMask" == caseDef.varName)
	{
		if (VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT == caseDef.shaderStage)
			return "OpDecorate %40 BuiltIn SubgroupGtMask\n";
		else
			return "OpDecorate %22 BuiltIn SubgroupGtMask\n";
	}
	else if ("gl_SubgroupLeMask" == caseDef.varName)
	{
		if (VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT == caseDef.shaderStage)
			return "OpDecorate %40 BuiltIn SubgroupLeMask\n";
		else
			return "OpDecorate %22 BuiltIn SubgroupLeMask\n";
	}
	else if ("gl_SubgroupLtMask" == caseDef.varName)
	{
		if (VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT == caseDef.shaderStage)
			return "OpDecorate %40 BuiltIn SubgroupLtMask\n";
		else
			return "OpDecorate %22 BuiltIn SubgroupLtMask\n";
	}
	return "";
}

std::string subgroupMask (const CaseDefinition& caseDef)
{
	std::ostringstream bdy;

	bdy << "  uint tempResult = 0x1;\n"
		<< "  uint bit        = 0x1;\n"
		<< "  uint bitCount   = 0x0;\n"
		<< "  uvec4 mask = subgroupBallot(true);\n"
		<< "  const uvec4 var = " << caseDef.varName << ";\n"
		<< "  for (uint i = 0; i < gl_SubgroupSize; i++)\n"
		<< "  {\n";

	if ("gl_SubgroupEqMask" == caseDef.varName)
	{
		bdy << "    if ((i == gl_SubgroupInvocationID) ^^ subgroupBallotBitExtract(var, i))\n"
			<< "    {\n"
			<< "      tempResult = 0;\n"
			<< "    }\n";
	}
	else if ("gl_SubgroupGeMask" == caseDef.varName)
	{
		bdy << "    if ((i >= gl_SubgroupInvocationID) ^^ subgroupBallotBitExtract(var, i))\n"
			<< "    {\n"
			<< "      tempResult = 0;\n"
			<< "    }\n";
	}
	else if ("gl_SubgroupGtMask" == caseDef.varName)
	{
		bdy << "    if ((i > gl_SubgroupInvocationID) ^^ subgroupBallotBitExtract(var, i))\n"
			<< "    {\n"
			<< "      tempResult = 0;\n"
			<< "    }\n";
	}
	else if ("gl_SubgroupLeMask" == caseDef.varName)
	{
		bdy << "    if ((i <= gl_SubgroupInvocationID) ^^ subgroupBallotBitExtract(var, i))\n"
			<< "    {\n"
			<< "      tempResult = 0;\n"
			<< "    }\n";
	}
	else if ("gl_SubgroupLtMask" == caseDef.varName)
	{
		bdy << "    if ((i < gl_SubgroupInvocationID) ^^ subgroupBallotBitExtract(var, i))\n"
			<< "    {\n"
			<< "      tempResult = 0;\n"
			<< "    }\n";
	}

	bdy << "  }\n"
		<< "  for (uint i = 0; i < 32; i++)\n"
		<< "  {\n"
		<< "    if ((var.x & bit) > 0)\n"
		<< "    {\n"
		<< "      bitCount++;\n"
		<< "    }\n"
		<< "    if ((var.y & bit) > 0)\n"
		<< "    {\n"
		<< "      bitCount++;\n"
		<< "    }\n"
		<< "    if ((var.z & bit) > 0)\n"
		<< "    {\n"
		<< "      bitCount++;\n"
		<< "    }\n"
		<< "    if ((var.w & bit) > 0)\n"
		<< "    {\n"
		<< "      bitCount++;\n"
		<< "    }\n"
		<< "    bit = bit<<1;\n"
		<< "  }\n"
		<< "  if (subgroupBallotBitCount(var) != bitCount)\n"
		<< "  {\n"
		<< "    tempResult = 0;\n"
		<< "  }\n";
	return bdy.str();
}

void initFrameBufferPrograms(SourceCollections& programCollection, CaseDefinition caseDef)
{
	const vk::SpirVAsmBuildOptions	buildOptionsSpr	(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_3);
	const string					comparison		= subgroupComparison(caseDef);
	const string					mask			= varSubgroupMask(caseDef);

	subgroups::setFragmentShaderFrameBuffer(programCollection);

	if (VK_SHADER_STAGE_VERTEX_BIT != caseDef.shaderStage)
		subgroups::setVertexShaderFrameBuffer(programCollection);

	if (VK_SHADER_STAGE_VERTEX_BIT == caseDef.shaderStage)
	{
		/*
			const string bdy = subgroupMask(caseDef);
			const string vertex =
			"#version 450\n"
			"#extension GL_KHR_shader_subgroup_ballot: enable\n"
			"layout(location = 0) out float out_color;\n"
			"layout(location = 0) in highp vec4 in_position;\n"
			"\n"
			"void main (void)\n"
			"{\n"
			+ bdy +
			"  out_color = float(tempResult);\n"
			"  gl_Position = in_position;\n"
			"  gl_PointSize = 1.0f;\n"
			"}\n";
			programCollection.glslSources.add("vert")
				<< glu::VertexSource(vertex) << vk::ShaderBuildOptions(vk::SPIRV_VERSION_1_3, 0u);
		*/

		const string vertex =
			"; SPIR-V\n"
			"; Version: 1.3\n"
			"; Generator: Khronos Glslang Reference Front End; 2\n"
			"; Bound: 123\n"
			"; Schema: 0\n"
			"OpCapability Shader\n"
			"OpCapability GroupNonUniform\n"
			"OpCapability GroupNonUniformBallot\n"
			"%1 = OpExtInstImport \"GLSL.std.450\"\n"
			"OpMemoryModel Logical GLSL450\n"
			"OpEntryPoint Vertex %4 \"main\" %22 %32 %36 %107 %114 %117\n"
			+ mask +
			"OpDecorate %32 RelaxedPrecision\n"
			"OpDecorate %32 BuiltIn SubgroupSize\n"
			"OpDecorate %33 RelaxedPrecision\n"
			"OpDecorate %36 RelaxedPrecision\n"
			"OpDecorate %36 BuiltIn SubgroupLocalInvocationId\n"
			"OpDecorate %37 RelaxedPrecision\n"
			"OpDecorate %107 Location 0\n"
			"OpMemberDecorate %112 0 BuiltIn Position\n"
			"OpMemberDecorate %112 1 BuiltIn PointSize\n"
			"OpMemberDecorate %112 2 BuiltIn ClipDistance\n"
			"OpMemberDecorate %112 3 BuiltIn CullDistance\n"
			"OpDecorate %112 Block\n"
			"OpDecorate %117 Location 0\n"
			"%2 = OpTypeVoid\n"
			"%3 = OpTypeFunction %2\n"
			"%6 = OpTypeInt 32 0\n"
			"%7 = OpTypePointer Function %6\n"
			"%9 = OpConstant %6 1\n"
			"%12 = OpConstant %6 0\n"
			"%13 = OpTypeVector %6 4\n"
			"%14 = OpTypePointer Function %13\n"
			"%16 = OpTypeBool\n"
			"%17 = OpConstantTrue %16\n"
			"%18 = OpConstant %6 3\n"
			"%21 = OpTypePointer Input %13\n"
			"%22 = OpVariable %21 Input\n"
			"%31 = OpTypePointer Input %6\n"
			"%32 = OpVariable %31 Input\n"
			"%36 = OpVariable %31 Input\n"
			"%46 = OpTypeInt 32 1\n"
			"%47 = OpConstant %46 1\n"
			"%56 = OpConstant %6 32\n"
			"%76 = OpConstant %6 2\n"
			"%105 = OpTypeFloat 32\n"
			"%106 = OpTypePointer Output %105\n"
			"%107 = OpVariable %106 Output\n"
			"%110 = OpTypeVector %105 4\n"
			"%111 = OpTypeArray %105 %9\n"
			"%112 = OpTypeStruct %110 %105 %111 %111\n"
			"%113 = OpTypePointer Output %112\n"
			"%114 = OpVariable %113 Output\n"
			"%115 = OpConstant %46 0\n"
			"%116 = OpTypePointer Input %110\n"
			"%117 = OpVariable %116 Input\n"
			"%119 = OpTypePointer Output %110\n"
			"%121 = OpConstant %105 1\n"
			"%4 = OpFunction %2 None %3\n"
			"%5 = OpLabel\n"
			"%8 = OpVariable %7 Function\n"
			"%10 = OpVariable %7 Function\n"
			"%11 = OpVariable %7 Function\n"
			"%15 = OpVariable %14 Function\n"
			"%20 = OpVariable %14 Function\n"
			"%24 = OpVariable %7 Function\n"
			"%49 = OpVariable %7 Function\n"
			"OpStore %8 %9\n"
			"OpStore %10 %9\n"
			"OpStore %11 %12\n"
			"%19 = OpGroupNonUniformBallot %13 %18 %17\n"
			"OpStore %15 %19\n"
			"%23 = OpLoad %13 %22\n"
			"OpStore %20 %23\n"
			"OpStore %24 %12\n"
			"OpBranch %25\n"
			"%25 = OpLabel\n"
			"OpLoopMerge %27 %28 None\n"
			"OpBranch %29\n"
			"%29 = OpLabel\n"
			"%30 = OpLoad %6 %24\n"
			"%33 = OpLoad %6 %32\n"
			"%34 = OpULessThan %16 %30 %33\n"
			"OpBranchConditional %34 %26 %27\n"
			"%26 = OpLabel\n"
			"%35 = OpLoad %6 %24\n"
			"%37 = OpLoad %6 %36\n"
			+ comparison +
			"%39 = OpLoad %13 %20\n"
			"%40 = OpLoad %6 %24\n"
			"%41 = OpGroupNonUniformBallotBitExtract %16 %18 %39 %40\n"
			"%42 = OpLogicalNotEqual %16 %38 %41\n"
			"OpSelectionMerge %44 None\n"
			"OpBranchConditional %42 %43 %44\n"
			"%43 = OpLabel\n"
			"OpStore %8 %12\n"
			"OpBranch %44\n"
			"%44 = OpLabel\n"
			"OpBranch %28\n"
			"%28 = OpLabel\n"
			"%45 = OpLoad %6 %24\n"
			"%48 = OpIAdd %6 %45 %47\n"
			"OpStore %24 %48\n"
			"OpBranch %25\n"
			"%27 = OpLabel\n"
			"OpStore %49 %12\n"
			"OpBranch %50\n"
			"%50 = OpLabel\n"
			"OpLoopMerge %52 %53 None\n"
			"OpBranch %54\n"
			"%54 = OpLabel\n"
			"%55 = OpLoad %6 %49\n"
			"%57 = OpULessThan %16 %55 %56\n"
			"OpBranchConditional %57 %51 %52\n"
			"%51 = OpLabel\n"
			"%58 = OpAccessChain %7 %20 %12\n"
			"%59 = OpLoad %6 %58\n"
			"%60 = OpLoad %6 %10\n"
			"%61 = OpBitwiseAnd %6 %59 %60\n"
			"%62 = OpUGreaterThan %16 %61 %12\n"
			"OpSelectionMerge %64 None\n"
			"OpBranchConditional %62 %63 %64\n"
			"%63 = OpLabel\n"
			"%65 = OpLoad %6 %11\n"
			"%66 = OpIAdd %6 %65 %47\n"
			"OpStore %11 %66\n"
			"OpBranch %64\n"
			"%64 = OpLabel\n"
			"%67 = OpAccessChain %7 %20 %9\n"
			"%68 = OpLoad %6 %67\n"
			"%69 = OpLoad %6 %10\n"
			"%70 = OpBitwiseAnd %6 %68 %69\n"
			"%71 = OpUGreaterThan %16 %70 %12\n"
			"OpSelectionMerge %73 None\n"
			"OpBranchConditional %71 %72 %73\n"
			"%72 = OpLabel\n"
			"%74 = OpLoad %6 %11\n"
			"%75 = OpIAdd %6 %74 %47\n"
			"OpStore %11 %75\n"
			"OpBranch %73\n"
			"%73 = OpLabel\n"
			"%77 = OpAccessChain %7 %20 %76\n"
			"%78 = OpLoad %6 %77\n"
			"%79 = OpLoad %6 %10\n"
			"%80 = OpBitwiseAnd %6 %78 %79\n"
			"%81 = OpUGreaterThan %16 %80 %12\n"
			"OpSelectionMerge %83 None\n"
			"OpBranchConditional %81 %82 %83\n"
			"%82 = OpLabel\n"
			"%84 = OpLoad %6 %11\n"
			"%85 = OpIAdd %6 %84 %47\n"
			"OpStore %11 %85\n"
			"OpBranch %83\n"
			"%83 = OpLabel\n"
			"%86 = OpAccessChain %7 %20 %18\n"
			"%87 = OpLoad %6 %86\n"
			"%88 = OpLoad %6 %10\n"
			"%89 = OpBitwiseAnd %6 %87 %88\n"
			"%90 = OpUGreaterThan %16 %89 %12\n"
			"OpSelectionMerge %92 None\n"
			"OpBranchConditional %90 %91 %92\n"
			"%91 = OpLabel\n"
			"%93 = OpLoad %6 %11\n"
			"%94 = OpIAdd %6 %93 %47\n"
			"OpStore %11 %94\n"
			"OpBranch %92\n"
			"%92 = OpLabel\n"
			"%95 = OpLoad %6 %10\n"
			"%96 = OpShiftLeftLogical %6 %95 %47\n"
			"OpStore %10 %96\n"
			"OpBranch %53\n"
			"%53 = OpLabel\n"
			"%97 = OpLoad %6 %49\n"
			"%98 = OpIAdd %6 %97 %47\n"
			"OpStore %49 %98\n"
			"OpBranch %50\n"
			"%52 = OpLabel\n"
			"%99 = OpLoad %13 %20\n"
			"%100 = OpGroupNonUniformBallotBitCount %6 %18 Reduce %99\n"
			"%101 = OpLoad %6 %11\n"
			"%102 = OpINotEqual %16 %100 %101\n"
			"OpSelectionMerge %104 None\n"
			"OpBranchConditional %102 %103 %104\n"
			"%103 = OpLabel\n"
			"OpStore %8 %12\n"
			"OpBranch %104\n"
			"%104 = OpLabel\n"
			"%108 = OpLoad %6 %8\n"
			"%109 = OpConvertUToF %105 %108\n"
			"OpStore %107 %109\n"
			"%118 = OpLoad %110 %117\n"
			"%120 = OpAccessChain %119 %114 %115\n"
			"OpStore %120 %118\n"
			"%122 = OpAccessChain %106 %114 %47\n"
			"OpStore %122 %121\n"
			"OpReturn\n"
			"OpFunctionEnd\n";
		programCollection.spirvAsmSources.add("vert") << vertex << buildOptionsSpr;
	}
	else if (VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT == caseDef.shaderStage)
	{
		/*
			const string bdy = subgroupMask(caseDef);
			const string  evaluationSource =
			"#version 450\n"
			"#extension GL_KHR_shader_subgroup_ballot: enable\n"
			"#extension GL_EXT_tessellation_shader : require\n"
			"layout(isolines, equal_spacing, ccw ) in;\n"
			"layout(location = 0) out float out_color;\n"
			"\n"
			"void main (void)\n"
			"{\n"
			+ bdy +
			"  out_color = float(tempResult);\n"
			"  gl_Position = mix(gl_in[0].gl_Position, gl_in[1].gl_Position, gl_TessCoord.x);\n"
			"}\n";
			programCollection.glslSources.add("tese")
				<< glu::TessellationEvaluationSource(evaluationSource) << vk::ShaderBuildOptions(vk::SPIRV_VERSION_1_3, 0u);
		*/
		const string evaluationSource =
			"; SPIR-V\n"
			"; Version: 1.3\n"
			"; Generator: Khronos Glslang Reference Front End; 2\n"
			"; Bound: 136\n"
			"; Schema: 0\n"
			"OpCapability Tessellation\n"
			"OpCapability GroupNonUniform\n"
			"OpCapability GroupNonUniformBallot\n"
			"%1 = OpExtInstImport \"GLSL.std.450\"\n"
			"OpMemoryModel Logical GLSL450\n"
			"OpEntryPoint TessellationEvaluation %4 \"main\" %22 %32 %36 %107 %114 %120 %128\n"
			"OpExecutionMode %4 Isolines\n"
			"OpExecutionMode %4 SpacingEqual\n"
			"OpExecutionMode %4 VertexOrderCcw\n"
			+ mask +
			"OpDecorate %32 RelaxedPrecision\n"
			"OpDecorate %32 BuiltIn SubgroupSize\n"
			"OpDecorate %33 RelaxedPrecision\n"
			"OpDecorate %36 RelaxedPrecision\n"
			"OpDecorate %36 BuiltIn SubgroupLocalInvocationId\n"
			"OpDecorate %37 RelaxedPrecision\n"
			"OpDecorate %107 Location 0\n"
			"OpMemberDecorate %112 0 BuiltIn Position\n"
			"OpMemberDecorate %112 1 BuiltIn PointSize\n"
			"OpMemberDecorate %112 2 BuiltIn ClipDistance\n"
			"OpMemberDecorate %112 3 BuiltIn CullDistance\n"
			"OpDecorate %112 Block\n"
			"OpMemberDecorate %116 0 BuiltIn Position\n"
			"OpMemberDecorate %116 1 BuiltIn PointSize\n"
			"OpMemberDecorate %116 2 BuiltIn ClipDistance\n"
			"OpMemberDecorate %116 3 BuiltIn CullDistance\n"
			"OpDecorate %116 Block\n"
			"OpDecorate %128 BuiltIn TessCoord\n"
			"%2 = OpTypeVoid\n"
			"%3 = OpTypeFunction %2\n"
			"%6 = OpTypeInt 32 0\n"
			"%7 = OpTypePointer Function %6\n"
			"%9 = OpConstant %6 1\n"
			"%12 = OpConstant %6 0\n"
			"%13 = OpTypeVector %6 4\n"
			"%14 = OpTypePointer Function %13\n"
			"%16 = OpTypeBool\n"
			"%17 = OpConstantTrue %16\n"
			"%18 = OpConstant %6 3\n"
			"%21 = OpTypePointer Input %13\n"
			"%22 = OpVariable %21 Input\n"
			"%31 = OpTypePointer Input %6\n"
			"%32 = OpVariable %31 Input\n"
			"%36 = OpVariable %31 Input\n"
			"%46 = OpTypeInt 32 1\n"
			"%47 = OpConstant %46 1\n"
			"%56 = OpConstant %6 32\n"
			"%76 = OpConstant %6 2\n"
			"%105 = OpTypeFloat 32\n"
			"%106 = OpTypePointer Output %105\n"
			"%107 = OpVariable %106 Output\n"
			"%110 = OpTypeVector %105 4\n"
			"%111 = OpTypeArray %105 %9\n"
			"%112 = OpTypeStruct %110 %105 %111 %111\n"
			"%113 = OpTypePointer Output %112\n"
			"%114 = OpVariable %113 Output\n"
			"%115 = OpConstant %46 0\n"
			"%116 = OpTypeStruct %110 %105 %111 %111\n"
			"%117 = OpConstant %6 32\n"
			"%118 = OpTypeArray %116 %117\n"
			"%119 = OpTypePointer Input %118\n"
			"%120 = OpVariable %119 Input\n"
			"%121 = OpTypePointer Input %110\n"
			"%126 = OpTypeVector %105 3\n"
			"%127 = OpTypePointer Input %126\n"
			"%128 = OpVariable %127 Input\n"
			"%129 = OpTypePointer Input %105\n"
			"%134 = OpTypePointer Output %110\n"
			"%4 = OpFunction %2 None %3\n"
			"%5 = OpLabel\n"
			"%8 = OpVariable %7 Function\n"
			"%10 = OpVariable %7 Function\n"
			"%11 = OpVariable %7 Function\n"
			"%15 = OpVariable %14 Function\n"
			"%20 = OpVariable %14 Function\n"
			"%24 = OpVariable %7 Function\n"
			"%49 = OpVariable %7 Function\n"
			"OpStore %8 %9\n"
			"OpStore %10 %9\n"
			"OpStore %11 %12\n"
			"%19 = OpGroupNonUniformBallot %13 %18 %17\n"
			"OpStore %15 %19\n"
			"%23 = OpLoad %13 %22\n"
			"OpStore %20 %23\n"
			"OpStore %24 %12\n"
			"OpBranch %25\n"
			"%25 = OpLabel\n"
			"OpLoopMerge %27 %28 None\n"
			"OpBranch %29\n"
			"%29 = OpLabel\n"
			"%30 = OpLoad %6 %24\n"
			"%33 = OpLoad %6 %32\n"
			"%34 = OpULessThan %16 %30 %33\n"
			"OpBranchConditional %34 %26 %27\n"
			"%26 = OpLabel\n"
			"%35 = OpLoad %6 %24\n"
			"%37 = OpLoad %6 %36\n"
			+ comparison +
			"%39 = OpLoad %13 %20\n"
			"%40 = OpLoad %6 %24\n"
			"%41 = OpGroupNonUniformBallotBitExtract %16 %18 %39 %40\n"
			"%42 = OpLogicalNotEqual %16 %38 %41\n"
			"OpSelectionMerge %44 None\n"
			"OpBranchConditional %42 %43 %44\n"
			"%43 = OpLabel\n"
			"OpStore %8 %12\n"
			"OpBranch %44\n"
			"%44 = OpLabel\n"
			"OpBranch %28\n"
			"%28 = OpLabel\n"
			"%45 = OpLoad %6 %24\n"
			"%48 = OpIAdd %6 %45 %47\n"
			"OpStore %24 %48\n"
			"OpBranch %25\n"
			"%27 = OpLabel\n"
			"OpStore %49 %12\n"
			"OpBranch %50\n"
			"%50 = OpLabel\n"
			"OpLoopMerge %52 %53 None\n"
			"OpBranch %54\n"
			"%54 = OpLabel\n"
			"%55 = OpLoad %6 %49\n"
			"%57 = OpULessThan %16 %55 %56\n"
			"OpBranchConditional %57 %51 %52\n"
			"%51 = OpLabel\n"
			"%58 = OpAccessChain %7 %20 %12\n"
			"%59 = OpLoad %6 %58\n"
			"%60 = OpLoad %6 %10\n"
			"%61 = OpBitwiseAnd %6 %59 %60\n"
			"%62 = OpUGreaterThan %16 %61 %12\n"
			"OpSelectionMerge %64 None\n"
			"OpBranchConditional %62 %63 %64\n"
			"%63 = OpLabel\n"
			"%65 = OpLoad %6 %11\n"
			"%66 = OpIAdd %6 %65 %47\n"
			"OpStore %11 %66\n"
			"OpBranch %64\n"
			"%64 = OpLabel\n"
			"%67 = OpAccessChain %7 %20 %9\n"
			"%68 = OpLoad %6 %67\n"
			"%69 = OpLoad %6 %10\n"
			"%70 = OpBitwiseAnd %6 %68 %69\n"
			"%71 = OpUGreaterThan %16 %70 %12\n"
			"OpSelectionMerge %73 None\n"
			"OpBranchConditional %71 %72 %73\n"
			"%72 = OpLabel\n"
			"%74 = OpLoad %6 %11\n"
			"%75 = OpIAdd %6 %74 %47\n"
			"OpStore %11 %75\n"
			"OpBranch %73\n"
			"%73 = OpLabel\n"
			"%77 = OpAccessChain %7 %20 %76\n"
			"%78 = OpLoad %6 %77\n"
			"%79 = OpLoad %6 %10\n"
			"%80 = OpBitwiseAnd %6 %78 %79\n"
			"%81 = OpUGreaterThan %16 %80 %12\n"
			"OpSelectionMerge %83 None\n"
			"OpBranchConditional %81 %82 %83\n"
			"%82 = OpLabel\n"
			"%84 = OpLoad %6 %11\n"
			"%85 = OpIAdd %6 %84 %47\n"
			"OpStore %11 %85\n"
			"OpBranch %83\n"
			"%83 = OpLabel\n"
			"%86 = OpAccessChain %7 %20 %18\n"
			"%87 = OpLoad %6 %86\n"
			"%88 = OpLoad %6 %10\n"
			"%89 = OpBitwiseAnd %6 %87 %88\n"
			"%90 = OpUGreaterThan %16 %89 %12\n"
			"OpSelectionMerge %92 None\n"
			"OpBranchConditional %90 %91 %92\n"
			"%91 = OpLabel\n"
			"%93 = OpLoad %6 %11\n"
			"%94 = OpIAdd %6 %93 %47\n"
			"OpStore %11 %94\n"
			"OpBranch %92\n"
			"%92 = OpLabel\n"
			"%95 = OpLoad %6 %10\n"
			"%96 = OpShiftLeftLogical %6 %95 %47\n"
			"OpStore %10 %96\n"
			"OpBranch %53\n"
			"%53 = OpLabel\n"
			"%97 = OpLoad %6 %49\n"
			"%98 = OpIAdd %6 %97 %47\n"
			"OpStore %49 %98\n"
			"OpBranch %50\n"
			"%52 = OpLabel\n"
			"%99 = OpLoad %13 %20\n"
			"%100 = OpGroupNonUniformBallotBitCount %6 %18 Reduce %99\n"
			"%101 = OpLoad %6 %11\n"
			"%102 = OpINotEqual %16 %100 %101\n"
			"OpSelectionMerge %104 None\n"
			"OpBranchConditional %102 %103 %104\n"
			"%103 = OpLabel\n"
			"OpStore %8 %12\n"
			"OpBranch %104\n"
			"%104 = OpLabel\n"
			"%108 = OpLoad %6 %8\n"
			"%109 = OpConvertUToF %105 %108\n"
			"OpStore %107 %109\n"
			"%122 = OpAccessChain %121 %120 %115 %115\n"
			"%123 = OpLoad %110 %122\n"
			"%124 = OpAccessChain %121 %120 %47 %115\n"
			"%125 = OpLoad %110 %124\n"
			"%130 = OpAccessChain %129 %128 %12\n"
			"%131 = OpLoad %105 %130\n"
			"%132 = OpCompositeConstruct %110 %131 %131 %131 %131\n"
			"%133 = OpExtInst %110 %1 FMix %123 %125 %132\n"
			"%135 = OpAccessChain %134 %114 %115\n"
			"OpStore %135 %133\n"
			"OpReturn\n"
			"OpFunctionEnd\n";
		programCollection.spirvAsmSources.add("tese") << evaluationSource << buildOptionsSpr;
		subgroups::setTesCtrlShaderFrameBuffer(programCollection);
	}
	else if (VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT == caseDef.shaderStage)
	{
		/*
			const string bdy = subgroupMask(caseDef);
			const string  controlSource =
			"#version 450\n"
			"#extension GL_EXT_tessellation_shader : require\n"
			"#extension GL_KHR_shader_subgroup_ballot: enable\n"
			"layout(vertices = 2) out;\n"
			"layout(location = 0) out float out_color[];\n"
			"void main (void)\n"
			"{\n"
			"  if (gl_InvocationID == 0)\n"
			"  {\n"
			"    gl_TessLevelOuter[0] = 1.0f;\n"
			"    gl_TessLevelOuter[1] = 1.0f;\n"
			"  }\n"
			+ bdy +
			"  out_color[gl_InvocationID] = float(tempResult);\n"
			"  gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;\n"
			"}\n";
			programCollection.glslSources.add("tesc")
			<< glu::TessellationControlSource(controlSource) << vk::ShaderBuildOptions(vk::SPIRV_VERSION_1_3, 0u);
		*/
		const string controlSource =
			"; SPIR-V\n"
			"; Version: 1.3\n"
			"; Generator: Khronos Glslang Reference Front End; 2\n"
			"; Bound: 146\n"
			"; Schema: 0\n"
			"OpCapability Tessellation\n"
			"OpCapability GroupNonUniform\n"
			"OpCapability GroupNonUniformBallot\n"
			"%1 = OpExtInstImport \"GLSL.std.450\"\n"
			"OpMemoryModel Logical GLSL450\n"
			"OpEntryPoint TessellationControl %4 \"main\" %8 %20 %40 %50 %54 %123 %133 %139\n"
			"OpExecutionMode %4 OutputVertices 2\n"
			"OpDecorate %8 BuiltIn InvocationId\n"
			"OpDecorate %20 Patch\n"
			"OpDecorate %20 BuiltIn TessLevelOuter\n"
			+ mask +
			"OpDecorate %50 RelaxedPrecision\n"
			"OpDecorate %50 BuiltIn SubgroupSize\n"
			"OpDecorate %51 RelaxedPrecision\n"
			"OpDecorate %54 RelaxedPrecision\n"
			"OpDecorate %54 BuiltIn SubgroupLocalInvocationId\n"
			"OpDecorate %55 RelaxedPrecision\n"
			"OpDecorate %123 Location 0\n"
			"OpMemberDecorate %130 0 BuiltIn Position\n"
			"OpMemberDecorate %130 1 BuiltIn PointSize\n"
			"OpMemberDecorate %130 2 BuiltIn ClipDistance\n"
			"OpMemberDecorate %130 3 BuiltIn CullDistance\n"
			"OpDecorate %130 Block\n"
			"OpMemberDecorate %135 0 BuiltIn Position\n"
			"OpMemberDecorate %135 1 BuiltIn PointSize\n"
			"OpMemberDecorate %135 2 BuiltIn ClipDistance\n"
			"OpMemberDecorate %135 3 BuiltIn CullDistance\n"
			"OpDecorate %135 Block\n"
			"%2 = OpTypeVoid\n"
			"%3 = OpTypeFunction %2\n"
			"%6 = OpTypeInt 32 1\n"
			"%7 = OpTypePointer Input %6\n"
			"%8 = OpVariable %7 Input\n"
			"%10 = OpConstant %6 0\n"
			"%11 = OpTypeBool\n"
			"%15 = OpTypeFloat 32\n"
			"%16 = OpTypeInt 32 0\n"
			"%17 = OpConstant %16 4\n"
			"%18 = OpTypeArray %15 %17\n"
			"%19 = OpTypePointer Output %18\n"
			"%20 = OpVariable %19 Output\n"
			"%21 = OpConstant %15 1\n"
			"%22 = OpTypePointer Output %15\n"
			"%24 = OpConstant %6 1\n"
			"%26 = OpTypePointer Function %16\n"
			"%28 = OpConstant %16 1\n"
			"%31 = OpConstant %16 0\n"
			"%32 = OpTypeVector %16 4\n"
			"%33 = OpTypePointer Function %32\n"
			"%35 = OpConstantTrue %11\n"
			"%36 = OpConstant %16 3\n"
			"%39 = OpTypePointer Input %32\n"
			"%40 = OpVariable %39 Input\n"
			"%49 = OpTypePointer Input %16\n"
			"%50 = OpVariable %49 Input\n"
			"%54 = OpVariable %49 Input\n"
			"%72 = OpConstant %16 32\n"
			"%92 = OpConstant %16 2\n"
			"%121 = OpTypeArray %15 %92\n"
			"%122 = OpTypePointer Output %121\n"
			"%123 = OpVariable %122 Output\n"
			"%128 = OpTypeVector %15 4\n"
			"%129 = OpTypeArray %15 %28\n"
			"%130 = OpTypeStruct %128 %15 %129 %129\n"
			"%131 = OpTypeArray %130 %92\n"
			"%132 = OpTypePointer Output %131\n"
			"%133 = OpVariable %132 Output\n"
			"%135 = OpTypeStruct %128 %15 %129 %129\n"
			"%136 = OpConstant %16 32\n"
			"%137 = OpTypeArray %135 %136\n"
			"%138 = OpTypePointer Input %137\n"
			"%139 = OpVariable %138 Input\n"
			"%141 = OpTypePointer Input %128\n"
			"%144 = OpTypePointer Output %128\n"
			"%4 = OpFunction %2 None %3\n"
			"%5 = OpLabel\n"
			"%27 = OpVariable %26 Function\n"
			"%29 = OpVariable %26 Function\n"
			"%30 = OpVariable %26 Function\n"
			"%34 = OpVariable %33 Function\n"
			"%38 = OpVariable %33 Function\n"
			"%42 = OpVariable %26 Function\n"
			"%65 = OpVariable %26 Function\n"
			"%9 = OpLoad %6 %8\n"
			"%12 = OpIEqual %11 %9 %10\n"
			"OpSelectionMerge %14 None\n"
			"OpBranchConditional %12 %13 %14\n"
			"%13 = OpLabel\n"
			"%23 = OpAccessChain %22 %20 %10\n"
			"OpStore %23 %21\n"
			"%25 = OpAccessChain %22 %20 %24\n"
			"OpStore %25 %21\n"
			"OpBranch %14\n"
			"%14 = OpLabel\n"
			"OpStore %27 %28\n"
			"OpStore %29 %28\n"
			"OpStore %30 %31\n"
			"%37 = OpGroupNonUniformBallot %32 %36 %35\n"
			"OpStore %34 %37\n"
			"%41 = OpLoad %32 %40\n"
			"OpStore %38 %41\n"
			"OpStore %42 %31\n"
			"OpBranch %43\n"
			"%43 = OpLabel\n"
			"OpLoopMerge %45 %46 None\n"
			"OpBranch %47\n"
			"%47 = OpLabel\n"
			"%48 = OpLoad %16 %42\n"
			"%51 = OpLoad %16 %50\n"
			"%52 = OpULessThan %11 %48 %51\n"
			"OpBranchConditional %52 %44 %45\n"
			"%44 = OpLabel\n"
			"%53 = OpLoad %16 %42\n"
			"%55 = OpLoad %16 %54\n"
			+ comparison +
			"%57 = OpLoad %32 %38\n"
			"%58 = OpLoad %16 %42\n"
			"%59 = OpGroupNonUniformBallotBitExtract %11 %36 %57 %58\n"
			"%60 = OpLogicalNotEqual %11 %56 %59\n"
			"OpSelectionMerge %62 None\n"
			"OpBranchConditional %60 %61 %62\n"
			"%61 = OpLabel\n"
			"OpStore %27 %31\n"
			"OpBranch %62\n"
			"%62 = OpLabel\n"
			"OpBranch %46\n"
			"%46 = OpLabel\n"
			"%63 = OpLoad %16 %42\n"
			"%64 = OpIAdd %16 %63 %24\n"
			"OpStore %42 %64\n"
			"OpBranch %43\n"
			"%45 = OpLabel\n"
			"OpStore %65 %31\n"
			"OpBranch %66\n"
			"%66 = OpLabel\n"
			"OpLoopMerge %68 %69 None\n"
			"OpBranch %70\n"
			"%70 = OpLabel\n"
			"%71 = OpLoad %16 %65\n"
			"%73 = OpULessThan %11 %71 %72\n"
			"OpBranchConditional %73 %67 %68\n"
			"%67 = OpLabel\n"
			"%74 = OpAccessChain %26 %38 %31\n"
			"%75 = OpLoad %16 %74\n"
			"%76 = OpLoad %16 %29\n"
			"%77 = OpBitwiseAnd %16 %75 %76\n"
			"%78 = OpUGreaterThan %11 %77 %31\n"
			"OpSelectionMerge %80 None\n"
			"OpBranchConditional %78 %79 %80\n"
			"%79 = OpLabel\n"
			"%81 = OpLoad %16 %30\n"
			"%82 = OpIAdd %16 %81 %24\n"
			"OpStore %30 %82\n"
			"OpBranch %80\n"
			"%80 = OpLabel\n"
			"%83 = OpAccessChain %26 %38 %28\n"
			"%84 = OpLoad %16 %83\n"
			"%85 = OpLoad %16 %29\n"
			"%86 = OpBitwiseAnd %16 %84 %85\n"
			"%87 = OpUGreaterThan %11 %86 %31\n"
			"OpSelectionMerge %89 None\n"
			"OpBranchConditional %87 %88 %89\n"
			"%88 = OpLabel\n"
			"%90 = OpLoad %16 %30\n"
			"%91 = OpIAdd %16 %90 %24\n"
			"OpStore %30 %91\n"
			"OpBranch %89\n"
			"%89 = OpLabel\n"
			"%93 = OpAccessChain %26 %38 %92\n"
			"%94 = OpLoad %16 %93\n"
			"%95 = OpLoad %16 %29\n"
			"%96 = OpBitwiseAnd %16 %94 %95\n"
			"%97 = OpUGreaterThan %11 %96 %31\n"
			"OpSelectionMerge %99 None\n"
			"OpBranchConditional %97 %98 %99\n"
			"%98 = OpLabel\n"
			"%100 = OpLoad %16 %30\n"
			"%101 = OpIAdd %16 %100 %24\n"
			"OpStore %30 %101\n"
			"OpBranch %99\n"
			"%99 = OpLabel\n"
			"%102 = OpAccessChain %26 %38 %36\n"
			"%103 = OpLoad %16 %102\n"
			"%104 = OpLoad %16 %29\n"
			"%105 = OpBitwiseAnd %16 %103 %104\n"
			"%106 = OpUGreaterThan %11 %105 %31\n"
			"OpSelectionMerge %108 None\n"
			"OpBranchConditional %106 %107 %108\n"
			"%107 = OpLabel\n"
			"%109 = OpLoad %16 %30\n"
			"%110 = OpIAdd %16 %109 %24\n"
			"OpStore %30 %110\n"
			"OpBranch %108\n"
			"%108 = OpLabel\n"
			"%111 = OpLoad %16 %29\n"
			"%112 = OpShiftLeftLogical %16 %111 %24\n"
			"OpStore %29 %112\n"
			"OpBranch %69\n"
			"%69 = OpLabel\n"
			"%113 = OpLoad %16 %65\n"
			"%114 = OpIAdd %16 %113 %24\n"
			"OpStore %65 %114\n"
			"OpBranch %66\n"
			"%68 = OpLabel\n"
			"%115 = OpLoad %32 %38\n"
			"%116 = OpGroupNonUniformBallotBitCount %16 %36 Reduce %115\n"
			"%117 = OpLoad %16 %30\n"
			"%118 = OpINotEqual %11 %116 %117\n"
			"OpSelectionMerge %120 None\n"
			"OpBranchConditional %118 %119 %120\n"
			"%119 = OpLabel\n"
			"OpStore %27 %31\n"
			"OpBranch %120\n"
			"%120 = OpLabel\n"
			"%124 = OpLoad %6 %8\n"
			"%125 = OpLoad %16 %27\n"
			"%126 = OpConvertUToF %15 %125\n"
			"%127 = OpAccessChain %22 %123 %124\n"
			"OpStore %127 %126\n"
			"%134 = OpLoad %6 %8\n"
			"%140 = OpLoad %6 %8\n"
			"%142 = OpAccessChain %141 %139 %140 %10\n"
			"%143 = OpLoad %128 %142\n"
			"%145 = OpAccessChain %144 %133 %134 %10\n"
			"OpStore %145 %143\n"
			"OpReturn\n"
			"OpFunctionEnd\n";
		programCollection.spirvAsmSources.add("tesc") << controlSource << buildOptionsSpr;
		subgroups::setTesEvalShaderFrameBuffer(programCollection);
	}
	else if (VK_SHADER_STAGE_GEOMETRY_BIT == caseDef.shaderStage)
	{
		/*
		const string bdy = subgroupMask(caseDef);
		const string geometry =
		"#version 450\n"
		"#extension GL_KHR_shader_subgroup_ballot: enable\n"
		"layout(points) in;\n"
		"layout(points, max_vertices = 1) out;\n"
		"layout(location = 0) out float out_color;\n"
		"\n"
		"void main (void)\n"
		"{\n"
		+ bdy +
		"  out_color = float(tempResult);\n"
		"  gl_Position = gl_in[0].gl_Position;\n"
		"  gl_PointSize = gl_in[0].gl_PointSize;\n"
		"  EmitVertex();\n"
		"  EndPrimitive();\n"
		"}\n";
		programCollection.glslSources.add("geometry")
			<< glu::GeometrySource(geometry) << vk::ShaderBuildOptions(vk::SPIRV_VERSION_1_3, 0u);
		*/

		std::ostringstream geometry;
		geometry
		<< "; SPIR-V\n"
		<< "; Version: 1.3\n"
		<< "; Generator: Khronos Glslang Reference Front End; 2\n"
		<< "; Bound: 125\n"
		<< "; Schema: 0\n"
		<< "OpCapability Geometry\n"
		<< (*caseDef.geometryPointSizeSupported ?
			"OpCapability GeometryPointSize\n" : "")
		<< "OpCapability GroupNonUniform\n"
		<< "OpCapability GroupNonUniformBallot\n"
		<< "%1 = OpExtInstImport \"GLSL.std.450\"\n"
		<< "OpMemoryModel Logical GLSL450\n"
		<< "OpEntryPoint Geometry %4 \"main\" %22 %32 %36 %107 %114 %119\n"
		<< "OpExecutionMode %4 InputPoints\n"
		<< "OpExecutionMode %4 Invocations 1\n"
		<< "OpExecutionMode %4 OutputPoints\n"
		<< "OpExecutionMode %4 OutputVertices 1\n"
		<< mask
		<< "OpDecorate %32 RelaxedPrecision\n"
		<< "OpDecorate %32 BuiltIn SubgroupSize\n"
		<< "OpDecorate %33 RelaxedPrecision\n"
		<< "OpDecorate %36 RelaxedPrecision\n"
		<< "OpDecorate %36 BuiltIn SubgroupLocalInvocationId\n"
		<< "OpDecorate %37 RelaxedPrecision\n"
		<< "OpDecorate %107 Location 0\n"
		<< "OpMemberDecorate %112 0 BuiltIn Position\n"
		<< "OpMemberDecorate %112 1 BuiltIn PointSize\n"
		<< "OpMemberDecorate %112 2 BuiltIn ClipDistance\n"
		<< "OpMemberDecorate %112 3 BuiltIn CullDistance\n"
		<< "OpDecorate %112 Block\n"
		<< "OpMemberDecorate %116 0 BuiltIn Position\n"
		<< "OpMemberDecorate %116 1 BuiltIn PointSize\n"
		<< "OpMemberDecorate %116 2 BuiltIn ClipDistance\n"
		<< "OpMemberDecorate %116 3 BuiltIn CullDistance\n"
		<< "OpDecorate %116 Block\n"
		<< "%2 = OpTypeVoid\n"
		<< "%3 = OpTypeFunction %2\n"
		<< "%6 = OpTypeInt 32 0\n"
		<< "%7 = OpTypePointer Function %6\n"
		<< "%9 = OpConstant %6 1\n"
		<< "%12 = OpConstant %6 0\n"
		<< "%13 = OpTypeVector %6 4\n"
		<< "%14 = OpTypePointer Function %13\n"
		<< "%16 = OpTypeBool\n"
		<< "%17 = OpConstantTrue %16\n"
		<< "%18 = OpConstant %6 3\n"
		<< "%21 = OpTypePointer Input %13\n"
		<< "%22 = OpVariable %21 Input\n"
		<< "%31 = OpTypePointer Input %6\n"
		<< "%32 = OpVariable %31 Input\n"
		<< "%36 = OpVariable %31 Input\n"
		<< "%46 = OpTypeInt 32 1\n"
		<< "%47 = OpConstant %46 1\n"
		<< "%56 = OpConstant %6 32\n"
		<< "%76 = OpConstant %6 2\n"
		<< "%105 = OpTypeFloat 32\n"
		<< "%106 = OpTypePointer Output %105\n"
		<< "%107 = OpVariable %106 Output\n"
		<< "%110 = OpTypeVector %105 4\n"
		<< "%111 = OpTypeArray %105 %9\n"
		<< "%112 = OpTypeStruct %110 %105 %111 %111\n"
		<< "%113 = OpTypePointer Output %112\n"
		<< "%114 = OpVariable %113 Output\n"
		<< "%115 = OpConstant %46 0\n"
		<< "%116 = OpTypeStruct %110 %105 %111 %111\n"
		<< "%117 = OpTypeArray %116 %9\n"
		<< "%118 = OpTypePointer Input %117\n"
		<< "%119 = OpVariable %118 Input\n"
		<< "%120 = OpTypePointer Input %110\n"
		<< "%123 = OpTypePointer Output %110\n"
		<< (*caseDef.geometryPointSizeSupported ?
			"%125 = OpTypePointer Input %105\n"
			"%126 = OpTypePointer Output %105\n" : "" )
		<< "%4 = OpFunction %2 None %3\n"
		<< "%5 = OpLabel\n"
		<< "%8 = OpVariable %7 Function\n"
		<< "%10 = OpVariable %7 Function\n"
		<< "%11 = OpVariable %7 Function\n"
		<< "%15 = OpVariable %14 Function\n"
		<< "%20 = OpVariable %14 Function\n"
		<< "%24 = OpVariable %7 Function\n"
		<< "%49 = OpVariable %7 Function\n"
		<< "OpStore %8 %9\n"
		<< "OpStore %10 %9\n"
		<< "OpStore %11 %12\n"
		<< "%19 = OpGroupNonUniformBallot %13 %18 %17\n"
		<< "OpStore %15 %19\n"
		<< "%23 = OpLoad %13 %22\n"
		<< "OpStore %20 %23\n"
		<< "OpStore %24 %12\n"
		<< "OpBranch %25\n"
		<< "%25 = OpLabel\n"
		<< "OpLoopMerge %27 %28 None\n"
		<< "OpBranch %29\n"
		<< "%29 = OpLabel\n"
		<< "%30 = OpLoad %6 %24\n"
		<< "%33 = OpLoad %6 %32\n"
		<< "%34 = OpULessThan %16 %30 %33\n"
		<< "OpBranchConditional %34 %26 %27\n"
		<< "%26 = OpLabel\n"
		<< "%35 = OpLoad %6 %24\n"
		<< "%37 = OpLoad %6 %36\n"
		<< comparison
		<< "%39 = OpLoad %13 %20\n"
		<< "%40 = OpLoad %6 %24\n"
		<< "%41 = OpGroupNonUniformBallotBitExtract %16 %18 %39 %40\n"
		<< "%42 = OpLogicalNotEqual %16 %38 %41\n"
		<< "OpSelectionMerge %44 None\n"
		<< "OpBranchConditional %42 %43 %44\n"
		<< "%43 = OpLabel\n"
		<< "OpStore %8 %12\n"
		<< "OpBranch %44\n"
		<< "%44 = OpLabel\n"
		<< "OpBranch %28\n"
		<< "%28 = OpLabel\n"
		<< "%45 = OpLoad %6 %24\n"
		<< "%48 = OpIAdd %6 %45 %47\n"
		<< "OpStore %24 %48\n"
		<< "OpBranch %25\n"
		<< "%27 = OpLabel\n"
		<< "OpStore %49 %12\n"
		<< "OpBranch %50\n"
		<< "%50 = OpLabel\n"
		<< "OpLoopMerge %52 %53 None\n"
		<< "OpBranch %54\n"
		<< "%54 = OpLabel\n"
		<< "%55 = OpLoad %6 %49\n"
		<< "%57 = OpULessThan %16 %55 %56\n"
		<< "OpBranchConditional %57 %51 %52\n"
		<< "%51 = OpLabel\n"
		<< "%58 = OpAccessChain %7 %20 %12\n"
		<< "%59 = OpLoad %6 %58\n"
		<< "%60 = OpLoad %6 %10\n"
		<< "%61 = OpBitwiseAnd %6 %59 %60\n"
		<< "%62 = OpUGreaterThan %16 %61 %12\n"
		<< "OpSelectionMerge %64 None\n"
		<< "OpBranchConditional %62 %63 %64\n"
		<< "%63 = OpLabel\n"
		<< "%65 = OpLoad %6 %11\n"
		<< "%66 = OpIAdd %6 %65 %47\n"
		<< "OpStore %11 %66\n"
		<< "OpBranch %64\n"
		<< "%64 = OpLabel\n"
		<< "%67 = OpAccessChain %7 %20 %9\n"
		<< "%68 = OpLoad %6 %67\n"
		<< "%69 = OpLoad %6 %10\n"
		<< "%70 = OpBitwiseAnd %6 %68 %69\n"
		<< "%71 = OpUGreaterThan %16 %70 %12\n"
		<< "OpSelectionMerge %73 None\n"
		<< "OpBranchConditional %71 %72 %73\n"
		<< "%72 = OpLabel\n"
		<< "%74 = OpLoad %6 %11\n"
		<< "%75 = OpIAdd %6 %74 %47\n"
		<< "OpStore %11 %75\n"
		<< "OpBranch %73\n"
		<< "%73 = OpLabel\n"
		<< "%77 = OpAccessChain %7 %20 %76\n"
		<< "%78 = OpLoad %6 %77\n"
		<< "%79 = OpLoad %6 %10\n"
		<< "%80 = OpBitwiseAnd %6 %78 %79\n"
		<< "%81 = OpUGreaterThan %16 %80 %12\n"
		<< "OpSelectionMerge %83 None\n"
		<< "OpBranchConditional %81 %82 %83\n"
		<< "%82 = OpLabel\n"
		<< "%84 = OpLoad %6 %11\n"
		<< "%85 = OpIAdd %6 %84 %47\n"
		<< "OpStore %11 %85\n"
		<< "OpBranch %83\n"
		<< "%83 = OpLabel\n"
		<< "%86 = OpAccessChain %7 %20 %18\n"
		<< "%87 = OpLoad %6 %86\n"
		<< "%88 = OpLoad %6 %10\n"
		<< "%89 = OpBitwiseAnd %6 %87 %88\n"
		<< "%90 = OpUGreaterThan %16 %89 %12\n"
		<< "OpSelectionMerge %92 None\n"
		<< "OpBranchConditional %90 %91 %92\n"
		<< "%91 = OpLabel\n"
		<< "%93 = OpLoad %6 %11\n"
		<< "%94 = OpIAdd %6 %93 %47\n"
		<< "OpStore %11 %94\n"
		<< "OpBranch %92\n"
		<< "%92 = OpLabel\n"
		<< "%95 = OpLoad %6 %10\n"
		<< "%96 = OpShiftLeftLogical %6 %95 %47\n"
		<< "OpStore %10 %96\n"
		<< "OpBranch %53\n"
		<< "%53 = OpLabel\n"
		<< "%97 = OpLoad %6 %49\n"
		<< "%98 = OpIAdd %6 %97 %47\n"
		<< "OpStore %49 %98\n"
		<< "OpBranch %50\n"
		<< "%52 = OpLabel\n"
		<< "%99 = OpLoad %13 %20\n"
		<< "%100 = OpGroupNonUniformBallotBitCount %6 %18 Reduce %99\n"
		<< "%101 = OpLoad %6 %11\n"
		<< "%102 = OpINotEqual %16 %100 %101\n"
		<< "OpSelectionMerge %104 None\n"
		<< "OpBranchConditional %102 %103 %104\n"
		<< "%103 = OpLabel\n"
		<< "OpStore %8 %12\n"
		<< "OpBranch %104\n"
		<< "%104 = OpLabel\n"
		<< "%108 = OpLoad %6 %8\n"
		<< "%109 = OpConvertUToF %105 %108\n"
		<< "OpStore %107 %109\n"
		<< "%121 = OpAccessChain %120 %119 %115 %115\n"
		<< "%122 = OpLoad %110 %121\n"
		<< "%124 = OpAccessChain %123 %114 %115\n"
		<< "OpStore %124 %122\n"
		<< (*caseDef.geometryPointSizeSupported ?
			"%127 = OpAccessChain %125 %119 %115 %47\n"
			"%128 = OpLoad %105 %127\n"
			"%129 = OpAccessChain %126 %114 %47\n"
			"OpStore %129 %128\n" : "")
		<< "OpEmitVertex\n"
		<< "OpEndPrimitive\n"
		<< "OpReturn\n"
		<< "OpFunctionEnd\n";
	programCollection.spirvAsmSources.add("geometry") << geometry.str() << buildOptionsSpr;
	}
	else
	{
		DE_FATAL("Unsupported shader stage");
	}
}


void initPrograms(SourceCollections& programCollection, CaseDefinition caseDef)
{
	const string bdy = subgroupMask(caseDef);

	if (VK_SHADER_STAGE_COMPUTE_BIT == caseDef.shaderStage)
	{
		std::ostringstream src;

		src << "#version 450\n"
			<< "#extension GL_KHR_shader_subgroup_ballot: enable\n"
			<< "layout (local_size_x_id = 0, local_size_y_id = 1, "
			"local_size_z_id = 2) in;\n"
			<< "layout(set = 0, binding = 0, std430) buffer Output\n"
			<< "{\n"
			<< "  uint result[];\n"
			<< "};\n"
			<< "\n"
			<< "void main (void)\n"
			<< "{\n"
			<< "  uvec3 globalSize = gl_NumWorkGroups * gl_WorkGroupSize;\n"
			<< "  highp uint offset = globalSize.x * ((globalSize.y * "
			"gl_GlobalInvocationID.z) + gl_GlobalInvocationID.y) + "
			"gl_GlobalInvocationID.x;\n"
			<< bdy
			<< "  result[offset] = tempResult;\n"
			<< "}\n";

		programCollection.glslSources.add("comp")
				<< glu::ComputeSource(src.str()) << vk::ShaderBuildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_3, 0u);
	}
	else
	{
		{
			const string vertex =
				"#version 450\n"
				"#extension GL_KHR_shader_subgroup_ballot: enable\n"
				"layout(set = 0, binding = 0, std430) buffer Output\n"
				"{\n"
				"  uint result[];\n"
				"};\n"
				"\n"
				"void main (void)\n"
				"{\n"
				+ bdy +
				"  result[gl_VertexIndex] = tempResult;\n"
				"  float pixelSize = 2.0f/1024.0f;\n"
				"  float pixelPosition = pixelSize/2.0f - 1.0f;\n"
				"  gl_Position = vec4(float(gl_VertexIndex) * pixelSize + pixelPosition, 0.0f, 0.0f, 1.0f);\n"
				"  gl_PointSize = 1.0f;\n"
				"}\n";
			programCollection.glslSources.add("vert")
				<< glu::VertexSource(vertex) << vk::ShaderBuildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_3, 0u);
		}

		{
			const string tesc =
				"#version 450\n"
				"#extension GL_KHR_shader_subgroup_ballot: enable\n"
				"layout(vertices=1) out;\n"
				"layout(set = 0, binding = 1, std430) buffer Output\n"
				"{\n"
				"  uint result[];\n"
				"};\n"
				"\n"
				"void main (void)\n"
				"{\n"
				+ bdy +
				"  result[gl_PrimitiveID] = tempResult;\n"
				"  if (gl_InvocationID == 0)\n"
				"  {\n"
				"    gl_TessLevelOuter[0] = 1.0f;\n"
				"    gl_TessLevelOuter[1] = 1.0f;\n"
				"  }\n"
				"  gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;\n"
				+ (*caseDef.geometryPointSizeSupported ? "  gl_out[gl_InvocationID].gl_PointSize = gl_in[gl_InvocationID].gl_PointSize;\n" : "") +
				"}\n";
			programCollection.glslSources.add("tesc")
					<< glu::TessellationControlSource(tesc) << vk::ShaderBuildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_3, 0u);
		}

		{
			const string tese =
				"#version 450\n"
				"#extension GL_KHR_shader_subgroup_ballot: enable\n"
				"layout(isolines) in;\n"
				"layout(set = 0, binding = 2, std430) buffer Output\n"
				"{\n"
				"  uint result[];\n"
				"};\n"
				"\n"
				"void main (void)\n"
				"{\n"
				+ bdy +
				"  result[gl_PrimitiveID * 2 + uint(gl_TessCoord.x + 0.5)] = tempResult;\n"
				"  float pixelSize = 2.0f/1024.0f;\n"
				"  gl_Position = gl_in[0].gl_Position + gl_TessCoord.x * pixelSize / 2.0f;\n"
				+ (*caseDef.geometryPointSizeSupported ? "  gl_PointSize = gl_in[0].gl_PointSize;\n" : "") +
				"}\n";

			programCollection.glslSources.add("tese")
					<< glu::TessellationEvaluationSource(tese) << vk::ShaderBuildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_3, 0u);
		}

		{
			const string geometry =
				"#version 450\n"
				"#extension GL_KHR_shader_subgroup_ballot: enable\n"
				"layout(${TOPOLOGY}) in;\n"
				"layout(points, max_vertices = 1) out;\n"
				"layout(set = 0, binding = 3, std430) buffer Output\n"
				"{\n"
				"  uint result[];\n"
				"};\n"
				"\n"
				"void main (void)\n"
				"{\n"
				+ bdy +
				"  result[gl_PrimitiveIDIn] = tempResult;\n"
				"  gl_Position = gl_in[0].gl_Position;\n"
				+ (*caseDef.geometryPointSizeSupported ? "  gl_PointSize = gl_in[0].gl_PointSize;\n" : "") +
				"  EmitVertex();\n"
				"  EndPrimitive();\n"
				"}\n";

			subgroups::addGeometryShadersFromTemplate(geometry, vk::ShaderBuildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_3, 0u),
													  programCollection.glslSources);
		}

		{
			const string fragment =
				"#version 450\n"
				"#extension GL_KHR_shader_subgroup_ballot: enable\n"
				"layout(location = 0) out uint result;\n"
				"void main (void)\n"
				"{\n"
				+ bdy +
				"  result = tempResult;\n"
				"}\n";

			programCollection.glslSources.add("fragment")
				<< glu::FragmentSource(fragment)<< vk::ShaderBuildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_3, 0u);
		}

		subgroups::addNoSubgroupShader(programCollection);
	}
}

void supportedCheck (Context& context, CaseDefinition caseDef)
{
	DE_UNREF(caseDef);
	if (!subgroups::isSubgroupSupported(context))
		TCU_THROW(NotSupportedError, "Subgroup operations are not supported");

	if (caseDef.requiredSubgroupSize)
	{
		if (!context.requireDeviceFunctionality("VK_EXT_subgroup_size_control"))
			TCU_THROW(NotSupportedError, "Device does not support VK_EXT_subgroup_size_control extension");
		VkPhysicalDeviceSubgroupSizeControlFeaturesEXT subgroupSizeControlFeatures;
		subgroupSizeControlFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_SIZE_CONTROL_FEATURES_EXT;
		subgroupSizeControlFeatures.pNext = DE_NULL;

		VkPhysicalDeviceFeatures2 features;
		features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
		features.pNext = &subgroupSizeControlFeatures;

		context.getInstanceInterface().getPhysicalDeviceFeatures2(context.getPhysicalDevice(), &features);

		if (subgroupSizeControlFeatures.subgroupSizeControl == DE_FALSE)
			TCU_THROW(NotSupportedError, "Device does not support varying subgroup sizes nor required subgroup size");

		if (subgroupSizeControlFeatures.computeFullSubgroups == DE_FALSE)
			TCU_THROW(NotSupportedError, "Device does not support full subgroups in compute shaders");

		VkPhysicalDeviceSubgroupSizeControlPropertiesEXT subgroupSizeControlProperties;
		subgroupSizeControlProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_SIZE_CONTROL_PROPERTIES_EXT;
		subgroupSizeControlProperties.pNext = DE_NULL;

		VkPhysicalDeviceProperties2 properties;
		properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
		properties.pNext = &subgroupSizeControlProperties;

		context.getInstanceInterface().getPhysicalDeviceProperties2(context.getPhysicalDevice(), &properties);

		if ((subgroupSizeControlProperties.requiredSubgroupSizeStages & caseDef.shaderStage) != caseDef.shaderStage)
			TCU_THROW(NotSupportedError, "Required subgroup size is not supported for shader stage");
	}

	*caseDef.geometryPointSizeSupported = subgroups::isTessellationAndGeometryPointSizeSupported(context);
}

tcu::TestStatus noSSBOtest(Context& context, const CaseDefinition caseDef)
{
	if (!areSubgroupOperationsSupportedForStage(
				context, caseDef.shaderStage))
	{
		if (areSubgroupOperationsRequiredForStage(caseDef.shaderStage))
		{
			return tcu::TestStatus::fail(
					   "Shader stage " + getShaderStageName(caseDef.shaderStage) +
					   " is required to support subgroup operations!");
		}
		else
		{
			TCU_THROW(NotSupportedError, "Device does not support subgroup operations for this stage");
		}
	}

	if (!subgroups::isSubgroupFeatureSupportedForDevice(context, VK_SUBGROUP_FEATURE_BALLOT_BIT))
	{
		TCU_THROW(NotSupportedError, "Device does not support subgroup ballot operations");
	}

	if (VK_SHADER_STAGE_VERTEX_BIT == caseDef.shaderStage)
		return makeVertexFrameBufferTest(context, VK_FORMAT_R32_UINT, DE_NULL, 0, DE_NULL, checkVertexPipelineStages);
	else if ((VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT | VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT) & caseDef.shaderStage )
		return makeTessellationEvaluationFrameBufferTest(context, VK_FORMAT_R32_UINT, DE_NULL, 0, DE_NULL, checkVertexPipelineStages);

	return makeGeometryFrameBufferTest(context, VK_FORMAT_R32_UINT, DE_NULL, 0, DE_NULL, checkVertexPipelineStages);
}


tcu::TestStatus test(Context& context, const CaseDefinition caseDef)
{
	if (!subgroups::isSubgroupFeatureSupportedForDevice(context, VK_SUBGROUP_FEATURE_BALLOT_BIT))
	{
		TCU_THROW(NotSupportedError, "Device does not support subgroup ballot operations");
	}

	if (VK_SHADER_STAGE_COMPUTE_BIT == caseDef.shaderStage)
	{
		if (!areSubgroupOperationsSupportedForStage(context, caseDef.shaderStage))
		{
				return tcu::TestStatus::fail(
						   "Shader stage " + getShaderStageName(caseDef.shaderStage) +
						   " is required to support subgroup operations!");
		}

		if (caseDef.requiredSubgroupSize == DE_FALSE)
			return makeComputeTest(context, VK_FORMAT_R32_UINT, DE_NULL, 0, DE_NULL, checkComputeStage);

		tcu::TestLog& log	= context.getTestContext().getLog();
		VkPhysicalDeviceSubgroupSizeControlPropertiesEXT subgroupSizeControlProperties;
		subgroupSizeControlProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_SIZE_CONTROL_PROPERTIES_EXT;
		subgroupSizeControlProperties.pNext = DE_NULL;
		VkPhysicalDeviceProperties2 properties;
		properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
		properties.pNext = &subgroupSizeControlProperties;

		context.getInstanceInterface().getPhysicalDeviceProperties2(context.getPhysicalDevice(), &properties);

		log << tcu::TestLog::Message << "Testing required subgroup size range [" <<  subgroupSizeControlProperties.minSubgroupSize << ", "
			<< subgroupSizeControlProperties.maxSubgroupSize << "]" << tcu::TestLog::EndMessage;

		// According to the spec, requiredSubgroupSize must be a power-of-two integer.
		for (deUint32 size = subgroupSizeControlProperties.minSubgroupSize; size <= subgroupSizeControlProperties.maxSubgroupSize; size *= 2)
		{
			tcu::TestStatus result = subgroups::makeComputeTest(context, VK_FORMAT_R32_UINT, DE_NULL, 0, DE_NULL, checkComputeStage,
																size, VK_PIPELINE_SHADER_STAGE_CREATE_REQUIRE_FULL_SUBGROUPS_BIT_EXT);
			if (result.getCode() != QP_TEST_RESULT_PASS)
			{
				log << tcu::TestLog::Message << "subgroupSize " << size << " failed" << tcu::TestLog::EndMessage;
				return result;
			}
		}

		return tcu::TestStatus::pass("OK");
	}
	else
	{
		VkPhysicalDeviceSubgroupProperties subgroupProperties;
		subgroupProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES;
		subgroupProperties.pNext = DE_NULL;

		VkPhysicalDeviceProperties2 properties;
		properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
		properties.pNext = &subgroupProperties;

		context.getInstanceInterface().getPhysicalDeviceProperties2(context.getPhysicalDevice(), &properties);

		VkShaderStageFlagBits stages = (VkShaderStageFlagBits)(caseDef.shaderStage  & subgroupProperties.supportedStages);

		if ( VK_SHADER_STAGE_FRAGMENT_BIT != stages && !subgroups::isVertexSSBOSupportedForDevice(context))
		{
			if ( (stages & VK_SHADER_STAGE_FRAGMENT_BIT) == 0)
				TCU_THROW(NotSupportedError, "Device does not support vertex stage SSBO writes");
			else
				stages = VK_SHADER_STAGE_FRAGMENT_BIT;
		}

		if ((VkShaderStageFlagBits)0u == stages)
			TCU_THROW(NotSupportedError, "Subgroup operations are not supported for any graphic shader");

		return subgroups::allStages(context, VK_FORMAT_R32_UINT, DE_NULL, 0, DE_NULL, checkVertexPipelineStages, stages);
	}
}

tcu::TestCaseGroup* createSubgroupsBuiltinMaskVarTests(tcu::TestContext& testCtx)
{
de::MovePtr<tcu::TestCaseGroup> graphicGroup(new tcu::TestCaseGroup(
		testCtx, "graphics", "Subgroup builtin mask category	tests: graphics"));
	de::MovePtr<tcu::TestCaseGroup> computeGroup(new tcu::TestCaseGroup(
		testCtx, "compute", "Subgroup builtin mask category tests: compute"));
	de::MovePtr<tcu::TestCaseGroup> framebufferGroup(new tcu::TestCaseGroup(
		testCtx, "framebuffer", "Subgroup builtin mask category tests: framebuffer"));

	const char* const all_stages_vars[] =
	{
		"SubgroupEqMask",
		"SubgroupGeMask",
		"SubgroupGtMask",
		"SubgroupLeMask",
		"SubgroupLtMask",
	};

	const VkShaderStageFlags stages[] =
	{
		VK_SHADER_STAGE_VERTEX_BIT,
		VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,
		VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,
		VK_SHADER_STAGE_GEOMETRY_BIT,
	};


	for (int a = 0; a < DE_LENGTH_OF_ARRAY(all_stages_vars); ++a)
	{
		const std::string var = all_stages_vars[a];
		const std::string varLower = de::toLower(var);

		{
			const CaseDefinition caseDef = {"gl_" + var, VK_SHADER_STAGE_ALL_GRAPHICS, de::SharedPtr<bool>(new bool), DE_FALSE};
			addFunctionCaseWithPrograms(graphicGroup.get(),
										varLower, "",
										supportedCheck, initPrograms, test, caseDef);
		}

		{
			CaseDefinition caseDef = {"gl_" + var, VK_SHADER_STAGE_COMPUTE_BIT, de::SharedPtr<bool>(new bool), DE_FALSE};
			addFunctionCaseWithPrograms(computeGroup.get(),
										varLower, "",
										supportedCheck, initPrograms, test, caseDef);
			caseDef.requiredSubgroupSize = DE_TRUE;
			addFunctionCaseWithPrograms(computeGroup.get(),
										varLower + "_requiredsubgroupsize", "",
										supportedCheck, initPrograms, test, caseDef);
		}

		for (int stageIndex = 0; stageIndex < DE_LENGTH_OF_ARRAY(stages); ++stageIndex)
		{
			const CaseDefinition caseDef = {"gl_" + var, stages[stageIndex], de::SharedPtr<bool>(new bool), DE_FALSE};
			addFunctionCaseWithPrograms(framebufferGroup.get(),
						varLower + "_" +
						getShaderStageName(caseDef.shaderStage), "",
						supportedCheck, initFrameBufferPrograms, noSSBOtest, caseDef);
		}
	}

	de::MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(
		testCtx, "builtin_mask_var", "Subgroup builtin mask variable tests"));

	group->addChild(graphicGroup.release());
	group->addChild(computeGroup.release());
	group->addChild(framebufferGroup.release());

	return group.release();
}
} // subgroups
} // vkt
