/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2017 The Khronos Group Inc.
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

static bool checkVertexPipelineStages(std::vector<const void*> datas,
									  deUint32 width, deUint32)
{
	const deUint32* data =
		reinterpret_cast<const deUint32*>(datas[0]);
	for (deUint32 x = 0; x < width; ++x)
	{
		deUint32 val = data[x];

		if (0x1 != val)
		{
			return false;
		}
	}

	return true;
}

static bool checkCompute(std::vector<const void*> datas,
						 const deUint32 numWorkgroups[3], const deUint32 localSize[3],
						 deUint32)
{
	const deUint32* data = reinterpret_cast<const deUint32*>(datas[0]);

	for (deUint32 nX = 0; nX < numWorkgroups[0]; ++nX)
	{
		for (deUint32 nY = 0; nY < numWorkgroups[1]; ++nY)
		{
			for (deUint32 nZ = 0; nZ < numWorkgroups[2]; ++nZ)
			{
				for (deUint32 lX = 0; lX < localSize[0]; ++lX)
				{
					for (deUint32 lY = 0; lY < localSize[1]; ++lY)
					{
						for (deUint32 lZ = 0; lZ < localSize[2];
								++lZ)
						{
							const deUint32 globalInvocationX =
								nX * localSize[0] + lX;
							const deUint32 globalInvocationY =
								nY * localSize[1] + lY;
							const deUint32 globalInvocationZ =
								nZ * localSize[2] + lZ;

							const deUint32 globalSizeX =
								numWorkgroups[0] * localSize[0];
							const deUint32 globalSizeY =
								numWorkgroups[1] * localSize[1];

							const deUint32 offset =
								globalSizeX *
								((globalSizeY *
								  globalInvocationZ) +
								 globalInvocationY) +
								globalInvocationX;

							if (0x1 != data[offset])
							{
								return false;
							}
						}
					}
				}
			}
		}
	}

	return true;
}

namespace
{
struct CaseDefinition
{
	std::string			varName;
	VkShaderStageFlags	shaderStage;
};
}

std::string subgroupComparison (const CaseDefinition& caseDef)
{
	if ("gl_SubgroupEqMask" == caseDef.varName)
	{
		if (VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT == caseDef.shaderStage)
			return "%54 = OpIEqual %11 %51 %53\n";
		else
			return "%36 = OpIEqual %13 %33 %35\n";
	}
	else if ("gl_SubgroupGeMask" == caseDef.varName)
	{
		if (VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT == caseDef.shaderStage)
			return "%54 = OpUGreaterThanEqual %11 %51 %53\n";
		else
			return "%36 = OpUGreaterThanEqual %13 %33 %35\n";
	}
	else if ("gl_SubgroupGtMask" == caseDef.varName)
	{
		if (VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT == caseDef.shaderStage)
			return "%54 = OpUGreaterThan %11 %51 %53\n";
		else
			return "%36 = OpUGreaterThan %13 %33 %35\n";
	}
	else if ("gl_SubgroupLeMask" == caseDef.varName)
	{
		if (VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT == caseDef.shaderStage)
			return "%54 = OpULessThanEqual %11 %51 %53\n";
		else
			return "%36 = OpULessThanEqual %13 %33 %35\n";
	}
	else if ("gl_SubgroupLtMask" == caseDef.varName)
	{
		if (VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT == caseDef.shaderStage)
			return "%54 = OpULessThan %11 %51 %53\n";
		else
			return "%36 = OpULessThan %13 %33 %35\n";
	}
	return "";
}

std::string varSubgroupMask (const CaseDefinition& caseDef)
{
	if ("gl_SubgroupEqMask" == caseDef.varName)
	{
		if (VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT == caseDef.shaderStage)
			return "OpDecorate %37 BuiltIn SubgroupEqMask\n";
		else
			return "OpDecorate %19 BuiltIn SubgroupEqMask\n";
	}
	else if ("gl_SubgroupGeMask" == caseDef.varName)
	{
		if (VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT == caseDef.shaderStage)
			return "OpDecorate %37 BuiltIn SubgroupGeMask\n";
		else
			return "OpDecorate %19 BuiltIn SubgroupGeMask\n";
	}
	else if ("gl_SubgroupGtMask" == caseDef.varName)
	{
		if (VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT == caseDef.shaderStage)
			return "OpDecorate %37 BuiltIn SubgroupGtMask\n";
		else
			return "OpDecorate %19 BuiltIn SubgroupGtMask\n";
	}
	else if ("gl_SubgroupLeMask" == caseDef.varName)
	{
		if (VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT == caseDef.shaderStage)
			return "OpDecorate %37 BuiltIn SubgroupLeMask\n";
		else
			return "OpDecorate %19 BuiltIn SubgroupLeMask\n";
	}
	else if ("gl_SubgroupLtMask" == caseDef.varName)
	{
		if (VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT == caseDef.shaderStage)
			return "OpDecorate %37 BuiltIn SubgroupLtMask\n";
		else
			return "OpDecorate %19 BuiltIn SubgroupLtMask\n";
	}
	return "";
}

std::string subgroupMask (const CaseDefinition& caseDef)
{
	std::ostringstream bdy;

	bdy << "  uint tempResult = 0x1;\n"
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

	bdy << "  }\n";
	return bdy.str();
}

void initFrameBufferPrograms(SourceCollections& programCollection, CaseDefinition caseDef)
{
	const vk::SpirVAsmBuildOptions	buildOptionsSpr	(vk::SPIRV_VERSION_1_3);
	const string					comparison		= subgroupComparison(caseDef);
	const string					mask			= varSubgroupMask(caseDef);

	subgroups::setFragmentShaderFrameBuffer(programCollection);

	if (VK_SHADER_STAGE_VERTEX_BIT != caseDef.shaderStage)
		subgroups::setVertexShaderFrameBuffer(programCollection);

	if (VK_SHADER_STAGE_VERTEX_BIT == caseDef.shaderStage)
	{
		/*
			"#extension GL_KHR_shader_subgroup_ballot: enable\n"
			"layout(location = 0) out float out_color;\n"
			"layout(location = 0) in highp vec4 in_position;\n"
			"\n"
			"void main (void)\n"
			"{\n"
			bdy.str()
			"  out_color = float(tempResult);\n"
			"  gl_Position = in_position;\n"
			"  gl_PointSize = 1.0f;\n"
			"}\n";
		*/
		const string vertex =
			"; SPIR-V\n"
			"; Version: 1.3\n"
			"; Generator: Khronos Glslang Reference Front End; 2\n"
			"; Bound: 63\n"
			"; Schema: 0\n"
			"OpCapability Shader\n"
			"OpCapability GroupNonUniform\n"
			"OpCapability GroupNonUniformBallot\n"
			"%1 = OpExtInstImport \"GLSL.std.450\"\n"
			"OpMemoryModel Logical GLSL450\n"
			"OpEntryPoint Vertex %4 \"main\" %19 %30 %34 %49 %56 %59\n"
			+ mask +
			"OpDecorate %30 RelaxedPrecision\n"
			"OpDecorate %30 BuiltIn SubgroupSize\n"
			"OpDecorate %31 RelaxedPrecision\n"
			"OpDecorate %34 RelaxedPrecision\n"
			"OpDecorate %34 BuiltIn SubgroupLocalInvocationId\n"
			"OpDecorate %35 RelaxedPrecision\n"
			"OpDecorate %49 Location 0\n"
			"OpMemberDecorate %54 0 BuiltIn Position\n"
			"OpMemberDecorate %54 1 BuiltIn PointSize\n"
			"OpMemberDecorate %54 2 BuiltIn ClipDistance\n"
			"OpMemberDecorate %54 3 BuiltIn CullDistance\n"
			"OpDecorate %54 Block\n"
			"OpDecorate %59 Location 0\n"
			"%2 = OpTypeVoid\n"
			"%3 = OpTypeFunction %2\n"
			"%6 = OpTypeInt 32 0\n"
			"%7 = OpTypePointer Function %6\n"
			"%9 = OpConstant %6 1\n"
			"%10 = OpTypeVector %6 4\n"
			"%11 = OpTypePointer Function %10\n"
			"%13 = OpTypeBool\n"
			"%14 = OpConstantTrue %13\n"
			"%15 = OpConstant %6 3\n"
			"%18 = OpTypePointer Input %10\n"
			"%19 = OpVariable %18 Input\n"
			"%22 = OpConstant %6 0\n"
			"%29 = OpTypePointer Input %6\n"
			"%30 = OpVariable %29 Input\n"
			"%34 = OpVariable %29 Input\n"
			"%44 = OpTypeInt 32 1\n"
			"%45 = OpConstant %44 1\n"
			"%47 = OpTypeFloat 32\n"
			"%48 = OpTypePointer Output %47\n"
			"%49 = OpVariable %48 Output\n"
			"%52 = OpTypeVector %47 4\n"
			"%53 = OpTypeArray %47 %9\n"
			"%54 = OpTypeStruct %52 %47 %53 %53\n"
			"%55 = OpTypePointer Output %54\n"
			"%56 = OpVariable %55 Output\n"
			"%57 = OpConstant %44 0\n"
			"%63 = OpConstant %47 1\n"
			"%58 = OpTypePointer Input %52\n"
			"%59 = OpVariable %58 Input\n"
			"%61 = OpTypePointer Output %52\n"
			"%4 = OpFunction %2 None %3\n"
			"%5 = OpLabel\n"
			"%8 = OpVariable %7 Function\n"
			"%12 = OpVariable %11 Function\n"
			"%17 = OpVariable %11 Function\n"
			"%21 = OpVariable %7 Function\n"
			"OpStore %8 %9\n"
			"%16 = OpGroupNonUniformBallot %10 %15 %14\n"
			"OpStore %12 %16\n"
			"%20 = OpLoad %10 %19\n"
			"OpStore %17 %20\n"
			"OpStore %21 %22\n"
			"OpBranch %23\n"
			"%23 = OpLabel\n"
			"OpLoopMerge %25 %26 None\n"
			"OpBranch %27\n"
			"%27 = OpLabel\n"
			"%28 = OpLoad %6 %21\n"
			"%31 = OpLoad %6 %30\n"
			"%32 = OpULessThan %13 %28 %31\n"
			"OpBranchConditional %32 %24 %25\n"
			"%24 = OpLabel\n"
			"%33 = OpLoad %6 %21\n"
			"%35 = OpLoad %6 %34\n"
			+ comparison +
			"%37 = OpLoad %10 %17\n"
			"%38 = OpLoad %6 %21\n"
			"%39 = OpGroupNonUniformBallotBitExtract %13 %15 %37 %38\n"
			"%40 = OpLogicalNotEqual %13 %36 %39\n"
			"OpSelectionMerge %42 None\n"
			"OpBranchConditional %40 %41 %42\n"
			"%41 = OpLabel\n"
			"OpStore %8 %22\n"
			"OpBranch %42\n"
			"%42 = OpLabel\n"
			"OpBranch %26\n"
			"%26 = OpLabel\n"
			"%43 = OpLoad %6 %21\n"
			"%46 = OpIAdd %6 %43 %45\n"
			"OpStore %21 %46\n"
			"OpBranch %23\n"
			"%25 = OpLabel\n"
			"%50 = OpLoad %6 %8\n"
			"%51 = OpConvertUToF %47 %50\n"
			"OpStore %49 %51\n"
			"%60 = OpLoad %52 %59\n"
			"%62 = OpAccessChain %61 %56 %57\n"
			"OpStore %62 %60\n"
			"%64 = OpAccessChain %48 %56 %45\n"
			"OpStore %64 %63\n"
			"OpReturn\n"
			"OpFunctionEnd\n";
		programCollection.spirvAsmSources.add("vert") << vertex << buildOptionsSpr;
	}
	else if (VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT == caseDef.shaderStage)
	{
		const string evaluationSource =
			"; SPIR-V\n"
			"; Version: 1.3\n"
			"; Generator: Khronos Glslang Reference Front End; 2\n"
			"; Bound: 81\n"
			"; Schema: 0\n"
			"OpCapability Tessellation\n"
			"OpCapability GroupNonUniform\n"
			"OpCapability GroupNonUniformBallot\n"
			"%1 = OpExtInstImport \"GLSL.std.450\"\n"
			"OpMemoryModel Logical GLSL450\n"
			"OpEntryPoint TessellationEvaluation %4 \"main\" %19 %30 %34 %49 %56 %62 %70 %80\n"
			"OpExecutionMode %4 Isolines\n"
			"OpExecutionMode %4 SpacingEqual\n"
			"OpExecutionMode %4 VertexOrderCcw\n"
			+ mask +
			"OpDecorate %30 RelaxedPrecision\n"
			"OpDecorate %30 BuiltIn SubgroupSize\n"
			"OpDecorate %31 RelaxedPrecision\n"
			"OpDecorate %34 RelaxedPrecision\n"
			"OpDecorate %34 BuiltIn SubgroupLocalInvocationId\n"
			"OpDecorate %35 RelaxedPrecision\n"
			"OpDecorate %49 Location 0\n"
			"OpMemberDecorate %54 0 BuiltIn Position\n"
			"OpMemberDecorate %54 1 BuiltIn PointSize\n"
			"OpMemberDecorate %54 2 BuiltIn ClipDistance\n"
			"OpMemberDecorate %54 3 BuiltIn CullDistance\n"
			"OpDecorate %54 Block\n"
			"OpMemberDecorate %58 0 BuiltIn Position\n"
			"OpMemberDecorate %58 1 BuiltIn PointSize\n"
			"OpMemberDecorate %58 2 BuiltIn ClipDistance\n"
			"OpMemberDecorate %58 3 BuiltIn CullDistance\n"
			"OpDecorate %58 Block\n"
			"OpDecorate %70 BuiltIn TessCoord\n"
			"OpDecorate %80 Location 0\n"
			"%2 = OpTypeVoid\n"
			"%3 = OpTypeFunction %2\n"
			"%6 = OpTypeInt 32 0\n"
			"%7 = OpTypePointer Function %6\n"
			"%9 = OpConstant %6 1\n"
			"%10 = OpTypeVector %6 4\n"
			"%11 = OpTypePointer Function %10\n"
			"%13 = OpTypeBool\n"
			"%14 = OpConstantTrue %13\n"
			"%15 = OpConstant %6 3\n"
			"%18 = OpTypePointer Input %10\n"
			"%19 = OpVariable %18 Input\n"
			"%22 = OpConstant %6 0\n"
			"%29 = OpTypePointer Input %6\n"
			"%30 = OpVariable %29 Input\n"
			"%34 = OpVariable %29 Input\n"
			"%44 = OpTypeInt 32 1\n"
			"%45 = OpConstant %44 1\n"
			"%47 = OpTypeFloat 32\n"
			"%48 = OpTypePointer Output %47\n"
			"%49 = OpVariable %48 Output\n"
			"%52 = OpTypeVector %47 4\n"
			"%53 = OpTypeArray %47 %9\n"
			"%54 = OpTypeStruct %52 %47 %53 %53\n"
			"%55 = OpTypePointer Output %54\n"
			"%56 = OpVariable %55 Output\n"
			"%57 = OpConstant %44 0\n"
			"%58 = OpTypeStruct %52 %47 %53 %53\n"
			"%59 = OpConstant %6 32\n"
			"%60 = OpTypeArray %58 %59\n"
			"%61 = OpTypePointer Input %60\n"
			"%62 = OpVariable %61 Input\n"
			"%63 = OpTypePointer Input %52\n"
			"%68 = OpTypeVector %47 3\n"
			"%69 = OpTypePointer Input %68\n"
			"%70 = OpVariable %69 Input\n"
			"%71 = OpTypePointer Input %47\n"
			"%76 = OpTypePointer Output %52\n"
			"%78 = OpTypeArray %47 %59\n"
			"%79 = OpTypePointer Input %78\n"
			"%80 = OpVariable %79 Input\n"
			"%4 = OpFunction %2 None %3\n"
			"%5 = OpLabel\n"
			"%8 = OpVariable %7 Function\n"
			"%12 = OpVariable %11 Function\n"
			"%17 = OpVariable %11 Function\n"
			"%21 = OpVariable %7 Function\n"
			"OpStore %8 %9\n"
			"%16 = OpGroupNonUniformBallot %10 %15 %14\n"
			"OpStore %12 %16\n"
			"%20 = OpLoad %10 %19\n"
			"OpStore %17 %20\n"
			"OpStore %21 %22\n"
			"OpBranch %23\n"
			"%23 = OpLabel\n"
			"OpLoopMerge %25 %26 None\n"
			"OpBranch %27\n"
			"%27 = OpLabel\n"
			"%28 = OpLoad %6 %21\n"
			"%31 = OpLoad %6 %30\n"
			"%32 = OpULessThan %13 %28 %31\n"
			"OpBranchConditional %32 %24 %25\n"
			"%24 = OpLabel\n"
			"%33 = OpLoad %6 %21\n"
			"%35 = OpLoad %6 %34\n"
			+ comparison +
			"%37 = OpLoad %10 %17\n"
			"%38 = OpLoad %6 %21\n"
			"%39 = OpGroupNonUniformBallotBitExtract %13 %15 %37 %38\n"
			"%40 = OpLogicalNotEqual %13 %36 %39\n"
			"OpSelectionMerge %42 None\n"
			"OpBranchConditional %40 %41 %42\n"
			"%41 = OpLabel\n"
			"OpStore %8 %22\n"
			"OpBranch %42\n"
			"%42 = OpLabel\n"
			"OpBranch %26\n"
			"%26 = OpLabel\n"
			"%43 = OpLoad %6 %21\n"
			"%46 = OpIAdd %6 %43 %45\n"
			"OpStore %21 %46\n"
			"OpBranch %23\n"
			"%25 = OpLabel\n"
			"%50 = OpLoad %6 %8\n"
			"%51 = OpConvertUToF %47 %50\n"
			"OpStore %49 %51\n"
			"%64 = OpAccessChain %63 %62 %57 %57\n"
			"%65 = OpLoad %52 %64\n"
			"%66 = OpAccessChain %63 %62 %45 %57\n"
			"%67 = OpLoad %52 %66\n"
			"%72 = OpAccessChain %71 %70 %22\n"
			"%73 = OpLoad %47 %72\n"
			"%74 = OpCompositeConstruct %52 %73 %73 %73 %73\n"
			"%75 = OpExtInst %52 %1 FMix %65 %67 %74\n"
			"%77 = OpAccessChain %76 %56 %57\n"
			"OpStore %77 %75\n"
			"OpReturn\n"
			"OpFunctionEnd\n";
		programCollection.spirvAsmSources.add("tese") << evaluationSource << buildOptionsSpr;
		subgroups::setTesCtrlShaderFrameBuffer(programCollection);
	}
	else if (VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT == caseDef.shaderStage)
	{
		/*
			"#extension GL_EXT_tessellation_shader : require\n"
			"#extension GL_KHR_shader_subgroup_ballot: enable\n"
			"layout(vertices = 2) out;\n"
			"layout(location = 0) out float out_color[];\n"
			"void main (void)\n"
			"{\n"
			"  if (gl_InvocationID == 0)\n"
			  {\n"
			"    gl_TessLevelOuter[0] = 1.0f;\n"
			"    gl_TessLevelOuter[1] = 1.0f;\n"
			"  }\n"
			bdy.str()
			"  out_color[gl_InvocationID] = float(tempResult);\n"
			"  gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;\n"
			"}\n";;
		*/

		const string controlSource =
			"; SPIR-V\n"
			"; Version: 1.3\n"
			"; Generator: Khronos Glslang Reference Front End; 2\n"
			"; Bound: 89\n"
			"; Schema: 0\n"
			"OpCapability Tessellation\n"
			"OpCapability GroupNonUniform\n"
			"OpCapability GroupNonUniformBallot\n"
			"%1 = OpExtInstImport \"GLSL.std.450\"\n"
			"OpMemoryModel Logical GLSL450\n"
			"OpEntryPoint TessellationControl %4 \"main\" %8 %20 %37 %48 %52 %66 %76 %82\n"
			"OpExecutionMode %4 OutputVertices 2\n"
			"OpDecorate %8 BuiltIn InvocationId\n"
			"OpDecorate %20 Patch\n"
			"OpDecorate %20 BuiltIn TessLevelOuter\n"
			+ mask +
			"OpDecorate %48 RelaxedPrecision\n"
			"OpDecorate %48 BuiltIn SubgroupSize\n"
			"OpDecorate %49 RelaxedPrecision\n"
			"OpDecorate %52 RelaxedPrecision\n"
			"OpDecorate %52 BuiltIn SubgroupLocalInvocationId\n"
			"OpDecorate %53 RelaxedPrecision\n"
			"OpDecorate %66 Location 0\n"
			"OpMemberDecorate %73 0 BuiltIn Position\n"
			"OpMemberDecorate %73 1 BuiltIn PointSize\n"
			"OpMemberDecorate %73 2 BuiltIn ClipDistance\n"
			"OpMemberDecorate %73 3 BuiltIn CullDistance\n"
			"OpDecorate %73 Block\n"
			"OpMemberDecorate %78 0 BuiltIn Position\n"
			"OpMemberDecorate %78 1 BuiltIn PointSize\n"
			"OpMemberDecorate %78 2 BuiltIn ClipDistance\n"
			"OpMemberDecorate %78 3 BuiltIn CullDistance\n"
			"OpDecorate %78 Block\n"
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
			"%29 = OpTypeVector %16 4\n"
			"%30 = OpTypePointer Function %29\n"
			"%32 = OpConstantTrue %11\n"
			"%33 = OpConstant %16 3\n"
			"%36 = OpTypePointer Input %29\n"
			"%37 = OpVariable %36 Input\n"
			"%40 = OpConstant %16 0\n"
			"%47 = OpTypePointer Input %16\n"
			"%48 = OpVariable %47 Input\n"
			"%52 = OpVariable %47 Input\n"
			"%63 = OpConstant %16 2\n"
			"%64 = OpTypeArray %15 %63\n"
			"%65 = OpTypePointer Output %64\n"
			"%66 = OpVariable %65 Output\n"
			"%71 = OpTypeVector %15 4\n"
			"%72 = OpTypeArray %15 %28\n"
			"%73 = OpTypeStruct %71 %15 %72 %72\n"
			"%74 = OpTypeArray %73 %63\n"
			"%75 = OpTypePointer Output %74\n"
			"%76 = OpVariable %75 Output\n"
			"%78 = OpTypeStruct %71 %15 %72 %72\n"
			"%79 = OpConstant %16 32\n"
			"%80 = OpTypeArray %78 %79\n"
			"%81 = OpTypePointer Input %80\n"
			"%82 = OpVariable %81 Input\n"
			"%84 = OpTypePointer Input %71\n"
			"%87 = OpTypePointer Output %71\n"
			"%4 = OpFunction %2 None %3\n"
			"%5 = OpLabel\n"
			"%27 = OpVariable %26 Function\n"
			"%31 = OpVariable %30 Function\n"
			"%35 = OpVariable %30 Function\n"
			"%39 = OpVariable %26 Function\n"
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
			"%34 = OpGroupNonUniformBallot %29 %33 %32\n"
			"OpStore %31 %34\n"
			"%38 = OpLoad %29 %37\n"
			"OpStore %35 %38\n"
			"OpStore %39 %40\n"
			"OpBranch %41\n"
			"%41 = OpLabel\n"
			"OpLoopMerge %43 %44 None\n"
			"OpBranch %45\n"
			"%45 = OpLabel\n"
			"%46 = OpLoad %16 %39\n"
			"%49 = OpLoad %16 %48\n"
			"%50 = OpULessThan %11 %46 %49\n"
			"OpBranchConditional %50 %42 %43\n"
			"%42 = OpLabel\n"
			"%51 = OpLoad %16 %39\n"
			"%53 = OpLoad %16 %52\n"
			+ comparison +
			"%55 = OpLoad %29 %35\n"
			"%56 = OpLoad %16 %39\n"
			"%57 = OpGroupNonUniformBallotBitExtract %11 %33 %55 %56\n"
			"%58 = OpLogicalNotEqual %11 %54 %57\n"
			"OpSelectionMerge %60 None\n"
			"OpBranchConditional %58 %59 %60\n"
			"%59 = OpLabel\n"
			"OpStore %27 %40\n"
			"OpBranch %60\n"
			"%60 = OpLabel\n"
			"OpBranch %44\n"
			"%44 = OpLabel\n"
			"%61 = OpLoad %16 %39\n"
			"%62 = OpIAdd %16 %61 %24\n"
			"OpStore %39 %62\n"
			"OpBranch %41\n"
			"%43 = OpLabel\n"
			"%67 = OpLoad %6 %8\n"
			"%68 = OpLoad %16 %27\n"
			"%69 = OpConvertUToF %15 %68\n"
			"%70 = OpAccessChain %22 %66 %67\n"
			"OpStore %70 %69\n"
			"%77 = OpLoad %6 %8\n"
			"%83 = OpLoad %6 %8\n"
			"%85 = OpAccessChain %84 %82 %83 %10\n"
			"%86 = OpLoad %71 %85\n"
			"%88 = OpAccessChain %87 %76 %77 %10\n"
			"OpStore %88 %86\n"
			"OpReturn\n"
			"OpFunctionEnd\n";
		programCollection.spirvAsmSources.add("tesc") << controlSource << buildOptionsSpr;
		subgroups::setTesEvalShaderFrameBuffer(programCollection);
	}
	else if (VK_SHADER_STAGE_GEOMETRY_BIT == caseDef.shaderStage)
	{
		const string geometry =
			"; SPIR-V\n"
			"; Version: 1.3\n"
			"; Generator: Khronos Glslang Reference Front End; 2\n"
			"; Bound: 67\n"
			"; Schema: 0\n"
			"OpCapability Geometry\n"
			"OpCapability GroupNonUniform\n"
			"OpCapability GroupNonUniformBallot\n"
			"%1 = OpExtInstImport \"GLSL.std.450\"\n"
			"OpMemoryModel Logical GLSL450\n"
			"OpEntryPoint Geometry %4 \"main\" %19 %30 %34 %49 %56 %61\n"
			"OpExecutionMode %4 InputPoints\n"
			"OpExecutionMode %4 Invocations 1\n"
			"OpExecutionMode %4 OutputPoints\n"
			"OpExecutionMode %4 OutputVertices 1\n"
			+ mask +
			"OpDecorate %30 RelaxedPrecision\n"
			"OpDecorate %30 BuiltIn SubgroupSize\n"
			"OpDecorate %31 RelaxedPrecision\n"
			"OpDecorate %34 RelaxedPrecision\n"
			"OpDecorate %34 BuiltIn SubgroupLocalInvocationId\n"
			"OpDecorate %35 RelaxedPrecision\n"
			"OpDecorate %49 Location 0\n"
			"OpMemberDecorate %54 0 BuiltIn Position\n"
			"OpMemberDecorate %54 1 BuiltIn PointSize\n"
			"OpMemberDecorate %54 2 BuiltIn ClipDistance\n"
			"OpMemberDecorate %54 3 BuiltIn CullDistance\n"
			"OpDecorate %54 Block\n"
			"OpMemberDecorate %58 0 BuiltIn Position\n"
			"OpMemberDecorate %58 1 BuiltIn PointSize\n"
			"OpMemberDecorate %58 2 BuiltIn ClipDistance\n"
			"OpMemberDecorate %58 3 BuiltIn CullDistance\n"
			"OpDecorate %58 Block\n"
			"%2 = OpTypeVoid\n"
			"%3 = OpTypeFunction %2\n"
			"%6 = OpTypeInt 32 0\n"
			"%7 = OpTypePointer Function %6\n"
			"%9 = OpConstant %6 1\n"
			"%10 = OpTypeVector %6 4\n"
			"%11 = OpTypePointer Function %10\n"
			"%13 = OpTypeBool\n"
			"%14 = OpConstantTrue %13\n"
			"%15 = OpConstant %6 3\n"
			"%18 = OpTypePointer Input %10\n"
			"%19 = OpVariable %18 Input\n"
			"%22 = OpConstant %6 0\n"
			"%29 = OpTypePointer Input %6\n"
			"%30 = OpVariable %29 Input\n"
			"%34 = OpVariable %29 Input\n"
			"%44 = OpTypeInt 32 1\n"
			"%45 = OpConstant %44 1\n"
			"%47 = OpTypeFloat 32\n"
			"%48 = OpTypePointer Output %47\n"
			"%49 = OpVariable %48 Output\n"
			"%52 = OpTypeVector %47 4\n"
			"%53 = OpTypeArray %47 %9\n"
			"%54 = OpTypeStruct %52 %47 %53 %53\n"
			"%55 = OpTypePointer Output %54\n"
			"%56 = OpVariable %55 Output\n"
			"%57 = OpConstant %44 0\n"
			"%58 = OpTypeStruct %52 %47 %53 %53\n"
			"%59 = OpTypeArray %58 %9\n"
			"%60 = OpTypePointer Input %59\n"
			"%61 = OpVariable %60 Input\n"
			"%62 = OpTypePointer Input %52\n"
			"%65 = OpTypePointer Output %52\n"
			"%4 = OpFunction %2 None %3\n"
			"%5 = OpLabel\n"
			"%8 = OpVariable %7 Function\n"
			"%12 = OpVariable %11 Function\n"
			"%17 = OpVariable %11 Function\n"
			"%21 = OpVariable %7 Function\n"
			"OpStore %8 %9\n"
			"%16 = OpGroupNonUniformBallot %10 %15 %14\n"
			"OpStore %12 %16\n"
			"%20 = OpLoad %10 %19\n"
			"OpStore %17 %20\n"
			"OpStore %21 %22\n"
			"OpBranch %23\n"
			"%23 = OpLabel\n"
			"OpLoopMerge %25 %26 None\n"
			"OpBranch %27\n"
			"%27 = OpLabel\n"
			"%28 = OpLoad %6 %21\n"
			"%31 = OpLoad %6 %30\n"
			"%32 = OpULessThan %13 %28 %31\n"
			"OpBranchConditional %32 %24 %25\n"
			"%24 = OpLabel\n"
			"%33 = OpLoad %6 %21\n"
			"%35 = OpLoad %6 %34\n"
			+ comparison +
			"%37 = OpLoad %10 %17\n"
			"%38 = OpLoad %6 %21\n"
			"%39 = OpGroupNonUniformBallotBitExtract %13 %15 %37 %38\n"
			"%40 = OpLogicalNotEqual %13 %36 %39\n"
			"OpSelectionMerge %42 None\n"
			"OpBranchConditional %40 %41 %42\n"
			"%41 = OpLabel\n"
			"OpStore %8 %22\n"
			"OpBranch %42\n"
			"%42 = OpLabel\n"
			"OpBranch %26\n"
			"%26 = OpLabel\n"
			"%43 = OpLoad %6 %21\n"
			"%46 = OpIAdd %6 %43 %45\n"
			"OpStore %21 %46\n"
			"OpBranch %23\n"
			"%25 = OpLabel\n"
			"%50 = OpLoad %6 %8\n"
			"%51 = OpConvertUToF %47 %50\n"
			"OpStore %49 %51\n"
			"%63 = OpAccessChain %62 %61 %57 %57\n"
			"%64 = OpLoad %52 %63\n"
			"%66 = OpAccessChain %65 %56 %57\n"
			"OpStore %66 %64\n"
			"OpEmitVertex\n"
			"OpEndPrimitive\n"
			"OpReturn\n"
			"OpFunctionEnd\n";
	programCollection.spirvAsmSources.add("geometry") << geometry << buildOptionsSpr;
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
				<< glu::ComputeSource(src.str()) << vk::ShaderBuildOptions(vk::SPIRV_VERSION_1_3, 0u);
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
				<< glu::VertexSource(vertex) << vk::ShaderBuildOptions(vk::SPIRV_VERSION_1_3, 0u);
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
				"}\n";
			programCollection.glslSources.add("tesc")
					<< glu::TessellationControlSource(tesc) << vk::ShaderBuildOptions(vk::SPIRV_VERSION_1_3, 0u);
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
				"}\n";

			programCollection.glslSources.add("tese")
					<< glu::TessellationEvaluationSource(tese) << vk::ShaderBuildOptions(vk::SPIRV_VERSION_1_3, 0u);
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
				"  EmitVertex();\n"
				"  EndPrimitive();\n"
				"}\n";

			subgroups::addGeometryShadersFromTemplate(geometry, vk::ShaderBuildOptions(vk::SPIRV_VERSION_1_3, 0u),
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
				<< glu::FragmentSource(fragment)<< vk::ShaderBuildOptions(vk::SPIRV_VERSION_1_3, 0u);
		}

		subgroups::addNoSubgroupShader(programCollection);
	}
}

tcu::TestStatus noSSBOtest(Context& context, const CaseDefinition caseDef)
{
	if (!subgroups::isSubgroupSupported(context))
		TCU_THROW(NotSupportedError, "Subgroup operations are not supported");

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
		return makeVertexFrameBufferTest(context, VK_FORMAT_R32_UINT, DE_NULL, 0, checkVertexPipelineStages);
	else if ((VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT | VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT) & caseDef.shaderStage )
		return makeTessellationEvaluationFrameBufferTest(context, VK_FORMAT_R32_UINT, DE_NULL, 0, checkVertexPipelineStages);

	return makeGeometryFrameBufferTest(context, VK_FORMAT_R32_UINT, DE_NULL, 0, checkVertexPipelineStages);
}


tcu::TestStatus test(Context& context, const CaseDefinition caseDef)
{
	if (!subgroups::isSubgroupSupported(context))
		TCU_THROW(NotSupportedError, "Subgroup operations are not supported");

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
		return makeComputeTest(context, VK_FORMAT_R32_UINT, DE_NULL, 0, checkCompute);
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

		return subgroups::allStages(context, VK_FORMAT_R32_UINT, DE_NULL, 0, checkVertexPipelineStages, stages);
	}
}

tcu::TestCaseGroup* createSubgroupsBuiltinMaskVarTests(tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(
			testCtx, "builtin_mask_var", "Subgroup builtin mask variable tests"));

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
			const CaseDefinition caseDef = {"gl_" + var, VK_SHADER_STAGE_ALL_GRAPHICS};
			addFunctionCaseWithPrograms(group.get(),
										varLower + "_graphic" , "",
										initPrograms, test, caseDef);
		}

		{
			const CaseDefinition caseDef = {"gl_" + var, VK_SHADER_STAGE_COMPUTE_BIT};
			addFunctionCaseWithPrograms(group.get(),
										varLower + "_" +
										getShaderStageName(caseDef.shaderStage), "",
										initPrograms, test, caseDef);
		}

		for (int stageIndex = 0; stageIndex < DE_LENGTH_OF_ARRAY(stages); ++stageIndex)
		{
			const CaseDefinition caseDef = {"gl_" + var, stages[stageIndex]};
			addFunctionCaseWithPrograms(group.get(),
						varLower + "_" +
						getShaderStageName(caseDef.shaderStage)+"_framebuffer", "",
						initFrameBufferPrograms, noSSBOtest, caseDef);
		}
	}

	return group.release();
}
} // subgroups
} // vkt
