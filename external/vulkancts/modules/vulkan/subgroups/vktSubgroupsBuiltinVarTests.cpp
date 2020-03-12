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

#include "vktSubgroupsBuiltinVarTests.hpp"
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

bool checkVertexPipelineStagesSubgroupSize(const void* internalData, std::vector<const void*> datas,
		deUint32 width, deUint32 subgroupSize)
{
	DE_UNREF(internalData);
	const deUint32* data =
		reinterpret_cast<const deUint32*>(datas[0]);
	for (deUint32 x = 0; x < width; ++x)
	{
		deUint32 val = data[x * 4];

		if (subgroupSize != val)
			return false;
	}

	return true;
}

bool checkVertexPipelineStagesSubgroupInvocationID(const void* internalData, std::vector<const void*> datas,
		deUint32 width, deUint32 subgroupSize)
{
	DE_UNREF(internalData);
	const deUint32* data =
		reinterpret_cast<const deUint32*>(datas[0]);
	vector<deUint32> subgroupInvocationHits(subgroupSize, 0);

	for (deUint32 x = 0; x < width; ++x)
	{
		deUint32 subgroupInvocationID = data[(x * 4) + 1];

		if (subgroupInvocationID >= subgroupSize)
			return false;
		subgroupInvocationHits[subgroupInvocationID]++;
	}

	const deUint32 totalSize = width;

	deUint32 totalInvocationsRun = 0;
	for (deUint32 i = 0; i < subgroupSize; ++i)
	{
		totalInvocationsRun += subgroupInvocationHits[i];
	}

	if (totalInvocationsRun != totalSize)
		return false;

	return true;
}

static bool checkComputeSubgroupSize(const void* internalData, std::vector<const void*> datas,
									 const deUint32 numWorkgroups[3], const deUint32 localSize[3],
									 deUint32 subgroupSize)
{
	DE_UNREF(internalData);
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

							if (subgroupSize != data[offset * 4])
								return false;
						}
					}
				}
			}
		}
	}

	return true;
}

static bool checkComputeSubgroupInvocationID(const void* internalData, std::vector<const void*> datas,
		const deUint32 numWorkgroups[3], const deUint32 localSize[3],
		deUint32 subgroupSize)
{
	DE_UNREF(internalData);
	const deUint32* data = reinterpret_cast<const deUint32*>(datas[0]);

	for (deUint32 nX = 0; nX < numWorkgroups[0]; ++nX)
	{
		for (deUint32 nY = 0; nY < numWorkgroups[1]; ++nY)
		{
			for (deUint32 nZ = 0; nZ < numWorkgroups[2]; ++nZ)
			{
				const deUint32 totalLocalSize =
					localSize[0] * localSize[1] * localSize[2];
				vector<deUint32> subgroupInvocationHits(subgroupSize, 0);

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

							deUint32 subgroupInvocationID = data[(offset * 4) + 1];

							if (subgroupInvocationID >= subgroupSize)
								return false;

							subgroupInvocationHits[subgroupInvocationID]++;
						}
					}
				}

				deUint32 totalInvocationsRun = 0;
				for (deUint32 i = 0; i < subgroupSize; ++i)
				{
					totalInvocationsRun += subgroupInvocationHits[i];
				}

				if (totalInvocationsRun != totalLocalSize)
					return false;
			}
		}
	}

	return true;
}

static bool checkComputeNumSubgroups	(const void*				internalData,
										std::vector<const void*>	datas,
										const deUint32				numWorkgroups[3],
										const deUint32				localSize[3],
										deUint32)
{
	DE_UNREF(internalData);
	const deUint32* data = reinterpret_cast<const deUint32*>(datas[0]);

	for (deUint32 nX = 0; nX < numWorkgroups[0]; ++nX)
	{
		for (deUint32 nY = 0; nY < numWorkgroups[1]; ++nY)
		{
			for (deUint32 nZ = 0; nZ < numWorkgroups[2]; ++nZ)
			{
				const deUint32 totalLocalSize =
					localSize[0] * localSize[1] * localSize[2];

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

							deUint32 numSubgroups = data[(offset * 4) + 2];

							if (numSubgroups > totalLocalSize)
								return false;
						}
					}
				}
			}
		}
	}

	return true;
}

static bool checkComputeSubgroupID	(const void*				internalData,
									std::vector<const void*>	datas,
									const deUint32				numWorkgroups[3],
									const deUint32				localSize[3],
									deUint32)
{
	DE_UNREF(internalData);
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

							deUint32 numSubgroups = data[(offset * 4) + 2];
							deUint32 subgroupID = data[(offset * 4) + 3];

							if (subgroupID >= numSubgroups)
								return false;
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
	de::SharedPtr<bool>	geometryPointSizeSupported;
	deBool				requiredSubgroupSize;
};
}

void initFrameBufferPrograms (SourceCollections& programCollection, CaseDefinition caseDef)
{
	const vk::ShaderBuildOptions	buildOptions	(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_3, 0u);
	const vk::SpirVAsmBuildOptions	buildOptionsSpr	(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_3);

	{
		/*
			"layout(location = 0) in vec4 in_color;\n"
			"layout(location = 0) out uvec4 out_color;\n"
			"void main()\n"
			"{\n"
			 "	out_color = uvec4(in_color);\n"
			 "}\n";
		*/
		const string fragment =
			"; SPIR-V\n"
			"; Version: 1.3\n"
			"; Generator: Khronos Glslang Reference Front End; 2\n"
			"; Bound: 16\n"
			"; Schema: 0\n"
			"OpCapability Shader\n"
			"%1 = OpExtInstImport \"GLSL.std.450\"\n"
			"OpMemoryModel Logical GLSL450\n"
			"OpEntryPoint Fragment %4 \"main\" %9 %13\n"
			"OpExecutionMode %4 OriginUpperLeft\n"
			"OpDecorate %9 Location 0\n"
			"OpDecorate %13 Location 0\n"
			"%2 = OpTypeVoid\n"
			"%3 = OpTypeFunction %2\n"
			"%6 = OpTypeInt 32 0\n"
			"%7 = OpTypeVector %6 4\n"
			"%8 = OpTypePointer Output %7\n"
			"%9 = OpVariable %8 Output\n"
			"%10 = OpTypeFloat 32\n"
			"%11 = OpTypeVector %10 4\n"
			"%12 = OpTypePointer Input %11\n"
			"%13 = OpVariable %12 Input\n"
			"%4 = OpFunction %2 None %3\n"
			"%5 = OpLabel\n"
			"%14 = OpLoad %11 %13\n"
			"%15 = OpConvertFToU %7 %14\n"
			"OpStore %9 %15\n"
			"OpReturn\n"
			"OpFunctionEnd\n";
		programCollection.spirvAsmSources.add("fragment") << fragment << buildOptionsSpr;
	}

	if (VK_SHADER_STAGE_VERTEX_BIT != caseDef.shaderStage)
		subgroups::setVertexShaderFrameBuffer(programCollection);

	if (VK_SHADER_STAGE_VERTEX_BIT == caseDef.shaderStage)
	{
		/*
			"#extension GL_KHR_shader_subgroup_basic: enable\n"
			"layout(location = 0) out vec4 out_color;\n"
			"layout(location = 0) in highp vec4 in_position;\n"
			"\n"
			"void main (void)\n"
			"{\n"
			"  out_color = vec4(gl_SubgroupSize, gl_SubgroupInvocationID, 1.0f, 1.0f);\n"
			"  gl_Position = in_position;\n"
			"  gl_PointSize = 1.0f;\n"
			"}\n";
		*/
		const string vertex =
			"; SPIR-V\n"
			"; Version: 1.3\n"
			"; Generator: Khronos Glslang Reference Front End; 2\n"
			"; Bound: 31\n"
			"; Schema: 0\n"
			"OpCapability Shader\n"
			"OpCapability GroupNonUniform\n"
			"%1 = OpExtInstImport \"GLSL.std.450\"\n"
			"OpMemoryModel Logical GLSL450\n"
			"OpEntryPoint Vertex %4 \"main\" %9 %12 %15 %24 %28\n"
			"OpDecorate %9 Location 0\n"
			"OpDecorate %12 RelaxedPrecision\n"
			"OpDecorate %12 BuiltIn SubgroupSize\n"
			"OpDecorate %13 RelaxedPrecision\n"
			"OpDecorate %15 RelaxedPrecision\n"
			"OpDecorate %15 BuiltIn SubgroupLocalInvocationId\n"
			"OpDecorate %16 RelaxedPrecision\n"
			"OpMemberDecorate %22 0 BuiltIn Position\n"
			"OpMemberDecorate %22 1 BuiltIn PointSize\n"
			"OpMemberDecorate %22 2 BuiltIn ClipDistance\n"
			"OpMemberDecorate %22 3 BuiltIn CullDistance\n"
			"OpDecorate %22 Block\n"
			"OpDecorate %28 Location 0\n"
			"%2 = OpTypeVoid\n"
			"%3 = OpTypeFunction %2\n"
			"%6 = OpTypeFloat 32\n"
			"%7 = OpTypeVector %6 4\n"
			"%8 = OpTypePointer Output %7\n"
			"%9 = OpVariable %8 Output\n"
			"%10 = OpTypeInt 32 0\n"
			"%11 = OpTypePointer Input %10\n"
			"%12 = OpVariable %11 Input\n"
			"%15 = OpVariable %11 Input\n"
			"%18 = OpConstant %6 1\n"
			"%20 = OpConstant %10 1\n"
			"%21 = OpTypeArray %6 %20\n"
			"%22 = OpTypeStruct %7 %6 %21 %21\n"
			"%23 = OpTypePointer Output %22\n"
			"%24 = OpVariable %23 Output\n"
			"%25 = OpTypeInt 32 1\n"
			"%26 = OpConstant %25 0\n"
			"%27 = OpTypePointer Input %7\n"
			"%28 = OpVariable %27 Input\n"
			"%31 = OpConstant %25 1\n"
			"%32 = OpTypePointer Output %6\n"
			"%4 = OpFunction %2 None %3\n"
			"%5 = OpLabel\n"
			"%13 = OpLoad %10 %12\n"
			"%14 = OpConvertUToF %6 %13\n"
			"%16 = OpLoad %10 %15\n"
			"%17 = OpConvertUToF %6 %16\n"
			"%19 = OpCompositeConstruct %7 %14 %17 %18 %18\n"
			"OpStore %9 %19\n"
			"%29 = OpLoad %7 %28\n"
			"%30 = OpAccessChain %8 %24 %26\n"
			"OpStore %30 %29\n"
			"%33 = OpAccessChain %32 %24 %31\n"
			"OpStore %33 %18\n"
			"OpReturn\n"
			"OpFunctionEnd\n";
		programCollection.spirvAsmSources.add("vert") << vertex << buildOptionsSpr;
	}
	else if (VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT == caseDef.shaderStage)
	{
		/*
			"#extension GL_EXT_tessellation_shader : require\n"
			"layout(vertices = 2) out;\n"
			"layout(location = 0) out vec4 out_color[];\n"
			"void main (void)\n"
			"{\n"
			"  if (gl_InvocationID == 0)\n"
			  {\n"
			"    gl_TessLevelOuter[0] = 1.0f;\n"
			"    gl_TessLevelOuter[1] = 1.0f;\n"
			"  }\n"
			"  out_color[gl_InvocationID] = vec4(0.0f);\n"
			"  gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;\n"
			"}\n";
		*/
		const string controlSource =
			"; SPIR-V\n"
			"; Version: 1.3\n"
			"; Generator: Khronos Glslang Reference Front End; 2\n"
			"; Bound: 53\n"
			"; Schema: 0\n"
			"OpCapability Tessellation\n"
			"%1 = OpExtInstImport \"GLSL.std.450\"\n"
			"OpMemoryModel Logical GLSL450\n"
			"OpEntryPoint TessellationControl %4 \"main\" %8 %20 %30 %41 %47\n"
			"OpExecutionMode %4 OutputVertices 2\n"
			"OpDecorate %8 BuiltIn InvocationId\n"
			"OpDecorate %20 Patch\n"
			"OpDecorate %20 BuiltIn TessLevelOuter\n"
			"OpDecorate %30 Location 0\n"
			"OpMemberDecorate %38 0 BuiltIn Position\n"
			"OpMemberDecorate %38 1 BuiltIn PointSize\n"
			"OpMemberDecorate %38 2 BuiltIn ClipDistance\n"
			"OpMemberDecorate %38 3 BuiltIn CullDistance\n"
			"OpDecorate %38 Block\n"
			"OpMemberDecorate %43 0 BuiltIn Position\n"
			"OpMemberDecorate %43 1 BuiltIn PointSize\n"
			"OpMemberDecorate %43 2 BuiltIn ClipDistance\n"
			"OpMemberDecorate %43 3 BuiltIn CullDistance\n"
			"OpDecorate %43 Block\n"
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
			"%26 = OpTypeVector %15 4\n"
			"%27 = OpConstant %16 2\n"
			"%28 = OpTypeArray %26 %27\n"
			"%29 = OpTypePointer Output %28\n"
			"%30 = OpVariable %29 Output\n"
			"%32 = OpConstant %15 0\n"
			"%33 = OpConstantComposite %26 %32 %32 %32 %32\n"
			"%34 = OpTypePointer Output %26\n"
			"%36 = OpConstant %16 1\n"
			"%37 = OpTypeArray %15 %36\n"
			"%38 = OpTypeStruct %26 %15 %37 %37\n"
			"%39 = OpTypeArray %38 %27\n"
			"%40 = OpTypePointer Output %39\n"
			"%41 = OpVariable %40 Output\n"
			"%43 = OpTypeStruct %26 %15 %37 %37\n"
			"%44 = OpConstant %16 32\n"
			"%45 = OpTypeArray %43 %44\n"
			"%46 = OpTypePointer Input %45\n"
			"%47 = OpVariable %46 Input\n"
			"%49 = OpTypePointer Input %26\n"
			"%4 = OpFunction %2 None %3\n"
			"%5 = OpLabel\n"
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
			"%31 = OpLoad %6 %8\n"
			"%35 = OpAccessChain %34 %30 %31\n"
			"OpStore %35 %33\n"
			"%42 = OpLoad %6 %8\n"
			"%48 = OpLoad %6 %8\n"
			"%50 = OpAccessChain %49 %47 %48 %10\n"
			"%51 = OpLoad %26 %50\n"
			"%52 = OpAccessChain %34 %41 %42 %10\n"
			"OpStore %52 %51\n"
			"OpReturn\n"
			"OpFunctionEnd\n";
		programCollection.spirvAsmSources.add("tesc") << controlSource << buildOptionsSpr;

		/*
			"#extension GL_KHR_shader_subgroup_basic: enable\n"
			"#extension GL_EXT_tessellation_shader : require\n"
			"layout(isolines, equal_spacing, ccw ) in;\n"
			"layout(location = 0) in vec4 in_color[];\n"
			"layout(location = 0) out vec4 out_color;\n"
			"\n"
			"void main (void)\n"
			"{\n"
			"  gl_Position = mix(gl_in[0].gl_Position, gl_in[1].gl_Position, gl_TessCoord.x);\n"
			"  out_color = vec4(gl_SubgroupSize, gl_SubgroupInvocationID, 0.0f, 0.0f);\n"
			"}\n";
		*/
		const string evaluationSource =
			"; SPIR-V\n"
			"; Version: 1.3\n"
			"; Generator: Khronos Glslang Reference Front End; 2\n"
			"; Bound: 51\n"
			"; Schema: 0\n"
			"OpCapability Tessellation\n"
			"OpCapability GroupNonUniform\n"
			"%1 = OpExtInstImport \"GLSL.std.450\"\n"
			"OpMemoryModel Logical GLSL450\n"
			"OpEntryPoint TessellationEvaluation %4 \"main\" %13 %20 %29 %38 %40 %43 %50\n"
			"OpExecutionMode %4 Isolines\n"
			"OpExecutionMode %4 SpacingEqual\n"
			"OpExecutionMode %4 VertexOrderCcw\n"
			"OpMemberDecorate %11 0 BuiltIn Position\n"
			"OpMemberDecorate %11 1 BuiltIn PointSize\n"
			"OpMemberDecorate %11 2 BuiltIn ClipDistance\n"
			"OpMemberDecorate %11 3 BuiltIn CullDistance\n"
			"OpDecorate %11 Block\n"
			"OpMemberDecorate %16 0 BuiltIn Position\n"
			"OpMemberDecorate %16 1 BuiltIn PointSize\n"
			"OpMemberDecorate %16 2 BuiltIn ClipDistance\n"
			"OpMemberDecorate %16 3 BuiltIn CullDistance\n"
			"OpDecorate %16 Block\n"
			"OpDecorate %29 BuiltIn TessCoord\n"
			"OpDecorate %38 Location 0\n"
			"OpDecorate %40 RelaxedPrecision\n"
			"OpDecorate %40 BuiltIn SubgroupSize\n"
			"OpDecorate %41 RelaxedPrecision\n"
			"OpDecorate %43 RelaxedPrecision\n"
			"OpDecorate %43 BuiltIn SubgroupLocalInvocationId\n"
			"OpDecorate %44 RelaxedPrecision\n"
			"OpDecorate %50 Location 0\n"
			"%2 = OpTypeVoid\n"
			"%3 = OpTypeFunction %2\n"
			"%6 = OpTypeFloat 32\n"
			"%7 = OpTypeVector %6 4\n"
			"%8 = OpTypeInt 32 0\n"
			"%9 = OpConstant %8 1\n"
			"%10 = OpTypeArray %6 %9\n"
			"%11 = OpTypeStruct %7 %6 %10 %10\n"
			"%12 = OpTypePointer Output %11\n"
			"%13 = OpVariable %12 Output\n"
			"%14 = OpTypeInt 32 1\n"
			"%15 = OpConstant %14 0\n"
			"%16 = OpTypeStruct %7 %6 %10 %10\n"
			"%17 = OpConstant %8 32\n"
			"%18 = OpTypeArray %16 %17\n"
			"%19 = OpTypePointer Input %18\n"
			"%20 = OpVariable %19 Input\n"
			"%21 = OpTypePointer Input %7\n"
			"%24 = OpConstant %14 1\n"
			"%27 = OpTypeVector %6 3\n"
			"%28 = OpTypePointer Input %27\n"
			"%29 = OpVariable %28 Input\n"
			"%30 = OpConstant %8 0\n"
			"%31 = OpTypePointer Input %6\n"
			"%36 = OpTypePointer Output %7\n"
			"%38 = OpVariable %36 Output\n"
			"%39 = OpTypePointer Input %8\n"
			"%40 = OpVariable %39 Input\n"
			"%43 = OpVariable %39 Input\n"
			"%46 = OpConstant %6 0\n"
			"%48 = OpTypeArray %7 %17\n"
			"%49 = OpTypePointer Input %48\n"
			"%50 = OpVariable %49 Input\n"
			"%4 = OpFunction %2 None %3\n"
			"%5 = OpLabel\n"
			"%22 = OpAccessChain %21 %20 %15 %15\n"
			"%23 = OpLoad %7 %22\n"
			"%25 = OpAccessChain %21 %20 %24 %15\n"
			"%26 = OpLoad %7 %25\n"
			"%32 = OpAccessChain %31 %29 %30\n"
			"%33 = OpLoad %6 %32\n"
			"%34 = OpCompositeConstruct %7 %33 %33 %33 %33\n"
			"%35 = OpExtInst %7 %1 FMix %23 %26 %34\n"
			"%37 = OpAccessChain %36 %13 %15\n"
			"OpStore %37 %35\n"
			"%41 = OpLoad %8 %40\n"
			"%42 = OpConvertUToF %6 %41\n"
			"%44 = OpLoad %8 %43\n"
			"%45 = OpConvertUToF %6 %44\n"
			"%47 = OpCompositeConstruct %7 %42 %45 %46 %46\n"
			"OpStore %38 %47\n"
			"OpReturn\n"
			"OpFunctionEnd\n";

		programCollection.spirvAsmSources.add("tese") << evaluationSource << buildOptionsSpr;
	}
	else if (VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT == caseDef.shaderStage)
	{
		/*
			"#extension GL_EXT_tessellation_shader : require\n"
			"#extension GL_KHR_shader_subgroup_basic: enable\n"
			"layout(vertices = 2) out;\n"
			"layout(location = 0) out vec4 out_color[];\n"
			"void main (void)\n"
			"{\n"
			"  if (gl_InvocationID == 0)\n"
			  {\n"
			"    gl_TessLevelOuter[0] = 1.0f;\n"
			"    gl_TessLevelOuter[1] = 1.0f;\n"
			"  }\n"
			"  out_color[gl_InvocationID] = vec4(gl_SubgroupSize, gl_SubgroupInvocationID, 0, 0);\n"
			"  gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;\n"
			"}\n";
		*/
		const string controlSource =
			"; SPIR-V\n"
			"; Version: 1.3\n"
			"; Generator: Khronos Glslang Reference Front End; 2\n"
			"; Bound: 60\n"
			"; Schema: 0\n"
			"OpCapability Tessellation\n"
			"OpCapability GroupNonUniform\n"
			"%1 = OpExtInstImport \"GLSL.std.450\"\n"
			"OpMemoryModel Logical GLSL450\n"
			"OpEntryPoint TessellationControl %4 \"main\" %8 %20 %30 %33 %36 %48 %54\n"
			"OpExecutionMode %4 OutputVertices 2\n"
			"OpDecorate %8 BuiltIn InvocationId\n"
			"OpDecorate %20 Patch\n"
			"OpDecorate %20 BuiltIn TessLevelOuter\n"
			"OpDecorate %30 Location 0\n"
			"OpDecorate %33 RelaxedPrecision\n"
			"OpDecorate %33 BuiltIn SubgroupSize\n"
			"OpDecorate %34 RelaxedPrecision\n"
			"OpDecorate %36 RelaxedPrecision\n"
			"OpDecorate %36 BuiltIn SubgroupLocalInvocationId\n"
			"OpDecorate %37 RelaxedPrecision\n"
			"OpMemberDecorate %45 0 BuiltIn Position\n"
			"OpMemberDecorate %45 1 BuiltIn PointSize\n"
			"OpMemberDecorate %45 2 BuiltIn ClipDistance\n"
			"OpMemberDecorate %45 3 BuiltIn CullDistance\n"
			"OpDecorate %45 Block\n"
			"OpMemberDecorate %50 0 BuiltIn Position\n"
			"OpMemberDecorate %50 1 BuiltIn PointSize\n"
			"OpMemberDecorate %50 2 BuiltIn ClipDistance\n"
			"OpMemberDecorate %50 3 BuiltIn CullDistance\n"
			"OpDecorate %50 Block\n"
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
			"%26 = OpTypeVector %15 4\n"
			"%27 = OpConstant %16 2\n"
			"%28 = OpTypeArray %26 %27\n"
			"%29 = OpTypePointer Output %28\n"
			"%30 = OpVariable %29 Output\n"
			"%32 = OpTypePointer Input %16\n"
			"%33 = OpVariable %32 Input\n"
			"%36 = OpVariable %32 Input\n"
			"%39 = OpConstant %15 0\n"
			"%41 = OpTypePointer Output %26\n"
			"%43 = OpConstant %16 1\n"
			"%44 = OpTypeArray %15 %43\n"
			"%45 = OpTypeStruct %26 %15 %44 %44\n"
			"%46 = OpTypeArray %45 %27\n"
			"%47 = OpTypePointer Output %46\n"
			"%48 = OpVariable %47 Output\n"
			"%50 = OpTypeStruct %26 %15 %44 %44\n"
			"%51 = OpConstant %16 32\n"
			"%52 = OpTypeArray %50 %51\n"
			"%53 = OpTypePointer Input %52\n"
			"%54 = OpVariable %53 Input\n"
			"%56 = OpTypePointer Input %26\n"
			"%4 = OpFunction %2 None %3\n"
			"%5 = OpLabel\n"
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
			"%31 = OpLoad %6 %8\n"
			"%34 = OpLoad %16 %33\n"
			"%35 = OpConvertUToF %15 %34\n"
			"%37 = OpLoad %16 %36\n"
			"%38 = OpConvertUToF %15 %37\n"
			"%40 = OpCompositeConstruct %26 %35 %38 %39 %39\n"
			"%42 = OpAccessChain %41 %30 %31\n"
			"OpStore %42 %40\n"
			"%49 = OpLoad %6 %8\n"
			"%55 = OpLoad %6 %8\n"
			"%57 = OpAccessChain %56 %54 %55 %10\n"
			"%58 = OpLoad %26 %57\n"
			"%59 = OpAccessChain %41 %48 %49 %10\n"
			"OpStore %59 %58\n"
			"OpReturn\n"
			"OpFunctionEnd\n";
		programCollection.spirvAsmSources.add("tesc") << controlSource << buildOptionsSpr;

		/*
			"#extension GL_KHR_shader_subgroup_basic: enable\n"
			"#extension GL_EXT_tessellation_shader : require\n"
			"layout(isolines, equal_spacing, ccw ) in;\n"
			"layout(location = 0) in vec4 in_color[];\n"
			"layout(location = 0) out vec4 out_color;\n"
			"\n"
			"void main (void)\n"
			"{\n"
			"  gl_Position = mix(gl_in[0].gl_Position, gl_in[1].gl_Position, gl_TessCoord.x);\n"
			"  out_color = in_color[0];\n"
			"}\n";
		*/
		const string  evaluationSource =
			"; SPIR-V\n"
			"; Version: 1.3\n"
			"; Generator: Khronos Glslang Reference Front End; 2\n"
			"; Bound: 44\n"
			"; Schema: 0\n"
			"OpCapability Tessellation\n"
			"%1 = OpExtInstImport \"GLSL.std.450\"\n"
			"OpMemoryModel Logical GLSL450\n"
			"OpEntryPoint TessellationEvaluation %4 \"main\" %13 %20 %29 %38 %41\n"
			"OpExecutionMode %4 Isolines\n"
			"OpExecutionMode %4 SpacingEqual\n"
			"OpExecutionMode %4 VertexOrderCcw\n"
			"OpMemberDecorate %11 0 BuiltIn Position\n"
			"OpMemberDecorate %11 1 BuiltIn PointSize\n"
			"OpMemberDecorate %11 2 BuiltIn ClipDistance\n"
			"OpMemberDecorate %11 3 BuiltIn CullDistance\n"
			"OpDecorate %11 Block\n"
			"OpMemberDecorate %16 0 BuiltIn Position\n"
			"OpMemberDecorate %16 1 BuiltIn PointSize\n"
			"OpMemberDecorate %16 2 BuiltIn ClipDistance\n"
			"OpMemberDecorate %16 3 BuiltIn CullDistance\n"
			"OpDecorate %16 Block\n"
			"OpDecorate %29 BuiltIn TessCoord\n"
			"OpDecorate %38 Location 0\n"
			"OpDecorate %41 Location 0\n"
			"%2 = OpTypeVoid\n"
			"%3 = OpTypeFunction %2\n"
			"%6 = OpTypeFloat 32\n"
			"%7 = OpTypeVector %6 4\n"
			"%8 = OpTypeInt 32 0\n"
			"%9 = OpConstant %8 1\n"
			"%10 = OpTypeArray %6 %9\n"
			"%11 = OpTypeStruct %7 %6 %10 %10\n"
			"%12 = OpTypePointer Output %11\n"
			"%13 = OpVariable %12 Output\n"
			"%14 = OpTypeInt 32 1\n"
			"%15 = OpConstant %14 0\n"
			"%16 = OpTypeStruct %7 %6 %10 %10\n"
			"%17 = OpConstant %8 32\n"
			"%18 = OpTypeArray %16 %17\n"
			"%19 = OpTypePointer Input %18\n"
			"%20 = OpVariable %19 Input\n"
			"%21 = OpTypePointer Input %7\n"
			"%24 = OpConstant %14 1\n"
			"%27 = OpTypeVector %6 3\n"
			"%28 = OpTypePointer Input %27\n"
			"%29 = OpVariable %28 Input\n"
			"%30 = OpConstant %8 0\n"
			"%31 = OpTypePointer Input %6\n"
			"%36 = OpTypePointer Output %7\n"
			"%38 = OpVariable %36 Output\n"
			"%39 = OpTypeArray %7 %17\n"
			"%40 = OpTypePointer Input %39\n"
			"%41 = OpVariable %40 Input\n"
			"%4 = OpFunction %2 None %3\n"
			"%5 = OpLabel\n"
			"%22 = OpAccessChain %21 %20 %15 %15\n"
			"%23 = OpLoad %7 %22\n"
			"%25 = OpAccessChain %21 %20 %24 %15\n"
			"%26 = OpLoad %7 %25\n"
			"%32 = OpAccessChain %31 %29 %30\n"
			"%33 = OpLoad %6 %32\n"
			"%34 = OpCompositeConstruct %7 %33 %33 %33 %33\n"
			"%35 = OpExtInst %7 %1 FMix %23 %26 %34\n"
			"%37 = OpAccessChain %36 %13 %15\n"
			"OpStore %37 %35\n"
			"%42 = OpAccessChain %21 %41 %15\n"
			"%43 = OpLoad %7 %42\n"
			"OpStore %38 %43\n"
			"OpReturn\n"
			"OpFunctionEnd\n";
		programCollection.spirvAsmSources.add("tese") << evaluationSource << buildOptionsSpr;
	}
	else if (VK_SHADER_STAGE_GEOMETRY_BIT == caseDef.shaderStage)
	{
		/*
			"#version 450\n"
			"#extension GL_KHR_shader_subgroup_basic: enable\n"
			"layout(points) in;\n"
			"layout(points, max_vertices = 1) out;\n"
			"layout(location = 0) out vec4 out_color;\n"
			"void main (void)\n"
			"{\n"
			"  out_color = vec4(gl_SubgroupSize, gl_SubgroupInvocationID, 0, 0);\n"
			"  gl_Position = gl_in[0].gl_Position;\n"
			"  gl_PointSize = gl_in[0].gl_PointSize;\n"
			"  EmitVertex();\n"
			"  EndPrimitive();\n"
			"}\n";
		*/
		std::ostringstream geometry;
		geometry
			<< "; SPIR-V\n"
			<< "; Version: 1.3\n"
			<< "; Generator: Khronos Glslang Reference Front End; 7\n"
			<< "; Bound: 41\n"
			<< "; Schema: 0\n"
			<< "OpCapability Geometry\n"
			<< (*caseDef.geometryPointSizeSupported ?
				"OpCapability GeometryPointSize\n" : "" )
			<< "OpCapability GroupNonUniform\n"
			<< "%1 = OpExtInstImport \"GLSL.std.450\"\n"
			<< "OpMemoryModel Logical GLSL450\n"
			<< "OpEntryPoint Geometry %4 \"main\" %9 %12 %15 %24 %30\n"
			<< "OpExecutionMode %4 InputPoints\n"
			<< "OpExecutionMode %4 Invocations 1\n"
			<< "OpExecutionMode %4 OutputPoints\n"
			<< "OpExecutionMode %4 OutputVertices 1\n"
			<< "OpDecorate %9 Location 0\n"
			<< "OpDecorate %12 RelaxedPrecision\n"
			<< "OpDecorate %12 BuiltIn SubgroupSize\n"
			<< "OpDecorate %13 RelaxedPrecision\n"
			<< "OpDecorate %15 RelaxedPrecision\n"
			<< "OpDecorate %15 BuiltIn SubgroupLocalInvocationId\n"
			<< "OpDecorate %16 RelaxedPrecision\n"
			<< "OpMemberDecorate %22 0 BuiltIn Position\n"
			<< "OpMemberDecorate %22 1 BuiltIn PointSize\n"
			<< "OpMemberDecorate %22 2 BuiltIn ClipDistance\n"
			<< "OpMemberDecorate %22 3 BuiltIn CullDistance\n"
			<< "OpDecorate %22 Block\n"
			<< "OpMemberDecorate %27 0 BuiltIn Position\n"
			<< "OpMemberDecorate %27 1 BuiltIn PointSize\n"
			<< "OpMemberDecorate %27 2 BuiltIn ClipDistance\n"
			<< "OpMemberDecorate %27 3 BuiltIn CullDistance\n"
			<< "OpDecorate %27 Block\n"
			<< "%2 = OpTypeVoid\n"
			<< "%3 = OpTypeFunction %2\n"
			<< "%6 = OpTypeFloat 32\n"
			<< "%7 = OpTypeVector %6 4\n"
			<< "%8 = OpTypePointer Output %7\n"
			<< "%9 = OpVariable %8 Output\n"
			<< "%10 = OpTypeInt 32 0\n"
			<< "%11 = OpTypePointer Input %10\n"
			<< "%12 = OpVariable %11 Input\n"
			<< "%15 = OpVariable %11 Input\n"
			<< "%18 = OpConstant %6 0\n"
			<< "%20 = OpConstant %10 1\n"
			<< "%21 = OpTypeArray %6 %20\n"
			<< "%22 = OpTypeStruct %7 %6 %21 %21\n"
			<< "%23 = OpTypePointer Output %22\n"
			<< "%24 = OpVariable %23 Output\n"
			<< "%25 = OpTypeInt 32 1\n"
			<< "%26 = OpConstant %25 0\n"
			<< "%27 = OpTypeStruct %7 %6 %21 %21\n"
			<< "%28 = OpTypeArray %27 %20\n"
			<< "%29 = OpTypePointer Input %28\n"
			<< "%30 = OpVariable %29 Input\n"
			<< "%31 = OpTypePointer Input %7\n"
			<< (*caseDef.geometryPointSizeSupported ?
				"%35 = OpConstant %25 1\n"
				"%36 = OpTypePointer Input %6\n"
				"%39 = OpTypePointer Output %6\n" : "")
			<< "%4 = OpFunction %2 None %3\n"
			<< "%5 = OpLabel\n"
			<< "%13 = OpLoad %10 %12\n"
			<< "%14 = OpConvertUToF %6 %13\n"
			<< "%16 = OpLoad %10 %15\n"
			<< "%17 = OpConvertUToF %6 %16\n"
			<< "%19 = OpCompositeConstruct %7 %14 %17 %18 %18\n"
			<< "OpStore %9 %19\n"
			<< "%32 = OpAccessChain %31 %30 %26 %26\n"
			<< "%33 = OpLoad %7 %32\n"
			<< "%34 = OpAccessChain %8 %24 %26\n"
			<< "OpStore %34 %33\n"
			<< (*caseDef.geometryPointSizeSupported ?
				"%37 = OpAccessChain %36 %30 %26 %35\n"
				"%38 = OpLoad %6 %37\n"
				"%40 = OpAccessChain %39 %24 %35\n"
				"OpStore %40 %38\n" : "")
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
	if (VK_SHADER_STAGE_COMPUTE_BIT == caseDef.shaderStage)
	{
		std::ostringstream src;

		src << "#version 450\n"
			<< "#extension GL_KHR_shader_subgroup_basic: enable\n"
			<< "layout (local_size_x_id = 0, local_size_y_id = 1, "
			"local_size_z_id = 2) in;\n"
			<< "layout(set = 0, binding = 0, std430) buffer Output\n"
			<< "{\n"
			<< "  uvec4 result[];\n"
			<< "};\n"
			<< "\n"
			<< "void main (void)\n"
			<< "{\n"
			<< "  uvec3 globalSize = gl_NumWorkGroups * gl_WorkGroupSize;\n"
			<< "  highp uint offset = globalSize.x * ((globalSize.y * "
			"gl_GlobalInvocationID.z) + gl_GlobalInvocationID.y) + "
			"gl_GlobalInvocationID.x;\n"
			<< "  result[offset] = uvec4(gl_SubgroupSize, gl_SubgroupInvocationID, gl_NumSubgroups, gl_SubgroupID);\n"
			<< "}\n";

		programCollection.glslSources.add("comp")
				<< glu::ComputeSource(src.str()) << vk::ShaderBuildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_3, 0u);
	}
	else
	{
		{
			/*
				"#version 450\n"
				"#extension GL_KHR_shader_subgroup_basic: enable\n"
				"layout(set = 0, binding = 0, std430) buffer Output\n"
				"{\n"
				"  uvec4 result[];\n"
				"};\n"
				"\n"
				"void main (void)\n"
				"{\n"
				"  result[gl_VertexIndex] = uvec4(gl_SubgroupSize, gl_SubgroupInvocationID, 0, 0);\n"
				"  float pixelSize = 2.0f/1024.0f;\n"
				"  float pixelPosition = pixelSize/2.0f - 1.0f;\n"
				"  gl_Position = vec4(float(gl_VertexIndex) * pixelSize + pixelPosition, 0.0f, 0.0f, 1.0f);\n"
				"  gl_PointSize = 1.0f;\n"
				"}\n";
			*/
			const string vertex =
				"; SPIR-V\n"
				"; Version: 1.3\n"
				"; Generator: Khronos Glslang Reference Front End; 1\n"
				"; Bound: 52\n"
				"; Schema: 0\n"
				"OpCapability Shader\n"
				"OpCapability GroupNonUniform\n"
				"%1 = OpExtInstImport \"GLSL.std.450\"\n"
				"OpMemoryModel Logical GLSL450\n"
				"OpEntryPoint Vertex %4 \"main\" %15 %18 %20 %41\n"
				"OpDecorate %8 ArrayStride 16\n"
				"OpMemberDecorate %9 0 Offset 0\n"
				"OpDecorate %9 BufferBlock\n"
				"OpDecorate %11 DescriptorSet 0\n"
				"OpDecorate %11 Binding 0\n"
				"OpDecorate %15 BuiltIn VertexIndex\n"
				"OpDecorate %18 RelaxedPrecision\n"
				"OpDecorate %18 BuiltIn SubgroupSize\n"
				"OpDecorate %19 RelaxedPrecision\n"
				"OpDecorate %20 RelaxedPrecision\n"
				"OpDecorate %20 BuiltIn SubgroupLocalInvocationId\n"
				"OpDecorate %21 RelaxedPrecision\n"
				"OpMemberDecorate %39 0 BuiltIn Position\n"
				"OpMemberDecorate %39 1 BuiltIn PointSize\n"
				"OpMemberDecorate %39 2 BuiltIn ClipDistance\n"
				"OpMemberDecorate %39 3 BuiltIn CullDistance\n"
				"OpDecorate %39 Block\n"
				"%2 = OpTypeVoid\n"
				"%3 = OpTypeFunction %2\n"
				"%6 = OpTypeInt 32 0\n"
				"%7 = OpTypeVector %6 4\n"
				"%8 = OpTypeRuntimeArray %7\n"
				"%9 = OpTypeStruct %8\n"
				"%10 = OpTypePointer Uniform %9\n"
				"%11 = OpVariable %10 Uniform\n"
				"%12 = OpTypeInt 32 1\n"
				"%13 = OpConstant %12 0\n"
				"%14 = OpTypePointer Input %12\n"
				"%15 = OpVariable %14 Input\n"
				"%17 = OpTypePointer Input %6\n"
				"%18 = OpVariable %17 Input\n"
				"%20 = OpVariable %17 Input\n"
				"%22 = OpConstant %6 0\n"
				"%24 = OpTypePointer Uniform %7\n"
				"%26 = OpTypeFloat 32\n"
				"%27 = OpTypePointer Function %26\n"
				"%29 = OpConstant %26 0.00195313\n"
				"%32 = OpConstant %26 2\n"
				"%34 = OpConstant %26 1\n"
				"%36 = OpTypeVector %26 4\n"
				"%37 = OpConstant %6 1\n"
				"%38 = OpTypeArray %26 %37\n"
				"%39 = OpTypeStruct %36 %26 %38 %38\n"
				"%40 = OpTypePointer Output %39\n"
				"%41 = OpVariable %40 Output\n"
				"%48 = OpConstant %26 0\n"
				"%50 = OpTypePointer Output %36\n"
				"%52 = OpConstant %12 1\n"
				"%53 = OpTypePointer Output %26\n"
				"%4 = OpFunction %2 None %3\n"
				"%5 = OpLabel\n"
				"%28 = OpVariable %27 Function\n"
				"%30 = OpVariable %27 Function\n"
				"%16 = OpLoad %12 %15\n"
				"%19 = OpLoad %6 %18\n"
				"%21 = OpLoad %6 %20\n"
				"%23 = OpCompositeConstruct %7 %19 %21 %22 %22\n"
				"%25 = OpAccessChain %24 %11 %13 %16\n"
				"OpStore %25 %23\n"
				"OpStore %28 %29\n"
				"%31 = OpLoad %26 %28\n"
				"%33 = OpFDiv %26 %31 %32\n"
				"%35 = OpFSub %26 %33 %34\n"
				"OpStore %30 %35\n"
				"%42 = OpLoad %12 %15\n"
				"%43 = OpConvertSToF %26 %42\n"
				"%44 = OpLoad %26 %28\n"
				"%45 = OpFMul %26 %43 %44\n"
				"%46 = OpLoad %26 %30\n"
				"%47 = OpFAdd %26 %45 %46\n"
				"%49 = OpCompositeConstruct %36 %47 %48 %48 %34\n"
				"%51 = OpAccessChain %50 %41 %13\n"
				"OpStore %51 %49\n"
				"%54 = OpAccessChain %53 %41 %52\n"
				"OpStore %54 %34\n"
				"OpReturn\n"
				"OpFunctionEnd\n";
				programCollection.spirvAsmSources.add("vert") << vertex << SpirVAsmBuildOptions(programCollection.usedVulkanVersion, SPIRV_VERSION_1_3);
		}

		{
			/*
				"#version 450\n"
				"#extension GL_KHR_shader_subgroup_basic: enable\n"
				"layout(vertices=1) out;\n"
				"layout(set = 0, binding = 1, std430) buffer Output\n"
				"{\n"
				"  uvec4 result[];\n"
				"};\n"
				"\n"
				"void main (void)\n"
				"{\n"
				"  result[gl_PrimitiveID] = uvec4(gl_SubgroupSize, gl_SubgroupInvocationID, 0, 0);\n"
				"  if (gl_InvocationID == 0)\n"
				"  {\n"
				"    gl_TessLevelOuter[0] = 1.0f;\n"
				"    gl_TessLevelOuter[1] = 1.0f;\n"
				"  }\n"
				"  gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;\n"
				#if GEOMETRY_POINT_SIZE_SUPPORTED
				"  gl_out[gl_InvocationID].gl_PointSize = gl_in[gl_InvocationID].gl_PointSize;\n"
				#endif
				"}\n";
			*/

			const string pointSizeCapability = (*caseDef.geometryPointSizeSupported ? "OpCapability TessellationPointSize\n" : "");

			const string tesc =
				"; SPIR-V\n"
				"; Version: 1.3\n"
				"; Generator: Khronos Glslang Reference Front End; 1\n"
				"; Bound: 61\n"
				"; Schema: 0\n"
				"OpCapability Tessellation\n"
				"OpCapability GroupNonUniform\n"
				+ pointSizeCapability +
				"%1 = OpExtInstImport \"GLSL.std.450\"\n"
				"OpMemoryModel Logical GLSL450\n"
				"OpEntryPoint TessellationControl %4 \"main\" %15 %18 %20 %26 %36 %48 %54\n"
				"OpExecutionMode %4 OutputVertices 1\n"
				"OpDecorate %8 ArrayStride 16\n"
				"OpMemberDecorate %9 0 Offset 0\n"
				"OpDecorate %9 BufferBlock\n"
				"OpDecorate %11 DescriptorSet 0\n"
				"OpDecorate %11 Binding 1\n"
				"OpDecorate %15 BuiltIn PrimitiveId\n"
				"OpDecorate %18 RelaxedPrecision\n"
				"OpDecorate %18 BuiltIn SubgroupSize\n"
				"OpDecorate %19 RelaxedPrecision\n"
				"OpDecorate %20 RelaxedPrecision\n"
				"OpDecorate %20 BuiltIn SubgroupLocalInvocationId\n"
				"OpDecorate %21 RelaxedPrecision\n"
				"OpDecorate %26 BuiltIn InvocationId\n"
				"OpDecorate %36 Patch\n"
				"OpDecorate %36 BuiltIn TessLevelOuter\n"
				"OpMemberDecorate %45 0 BuiltIn Position\n"
				"OpMemberDecorate %45 1 BuiltIn PointSize\n"
				"OpMemberDecorate %45 2 BuiltIn ClipDistance\n"
				"OpMemberDecorate %45 3 BuiltIn CullDistance\n"
				"OpDecorate %45 Block\n"
				"OpMemberDecorate %50 0 BuiltIn Position\n"
				"OpMemberDecorate %50 1 BuiltIn PointSize\n"
				"OpMemberDecorate %50 2 BuiltIn ClipDistance\n"
				"OpMemberDecorate %50 3 BuiltIn CullDistance\n"
				"OpDecorate %50 Block\n"
				"%2 = OpTypeVoid\n"
				"%3 = OpTypeFunction %2\n"
				"%6 = OpTypeInt 32 0\n"
				"%7 = OpTypeVector %6 4\n"
				"%8 = OpTypeRuntimeArray %7\n"
				"%9 = OpTypeStruct %8\n"
				"%10 = OpTypePointer Uniform %9\n"
				"%11 = OpVariable %10 Uniform\n"
				"%12 = OpTypeInt 32 1\n"
				"%13 = OpConstant %12 0\n"
				"%14 = OpTypePointer Input %12\n"
				"%15 = OpVariable %14 Input\n"
				"%17 = OpTypePointer Input %6\n"
				"%18 = OpVariable %17 Input\n"
				"%20 = OpVariable %17 Input\n"
				"%22 = OpConstant %6 0\n"
				"%24 = OpTypePointer Uniform %7\n"
				"%26 = OpVariable %14 Input\n"
				"%28 = OpTypeBool\n"
				"%32 = OpTypeFloat 32\n"
				"%33 = OpConstant %6 4\n"
				"%34 = OpTypeArray %32 %33\n"
				"%35 = OpTypePointer Output %34\n"
				"%36 = OpVariable %35 Output\n"
				"%37 = OpConstant %32 1\n"
				"%38 = OpTypePointer Output %32\n"
				"%40 = OpConstant %12 1\n"
				"%42 = OpTypeVector %32 4\n"
				"%43 = OpConstant %6 1\n"
				"%44 = OpTypeArray %32 %43\n"
				"%45 = OpTypeStruct %42 %32 %44 %44\n"
				"%46 = OpTypeArray %45 %43\n"
				"%47 = OpTypePointer Output %46\n"
				"%48 = OpVariable %47 Output\n"
				"%50 = OpTypeStruct %42 %32 %44 %44\n"
				"%51 = OpConstant %6 32\n"
				"%52 = OpTypeArray %50 %51\n"
				"%53 = OpTypePointer Input %52\n"
				"%54 = OpVariable %53 Input\n"
				"%56 = OpTypePointer Input %42\n"
				"%59 = OpTypePointer Output %42\n"
				+ (*caseDef.geometryPointSizeSupported ?
					"%61 = OpTypePointer Input %32\n"
					"%62 = OpTypePointer Output %32\n"
					"%63 = OpConstant %12 1\n" : "") +
				"%4 = OpFunction %2 None %3\n"
				"%5 = OpLabel\n"
				"%16 = OpLoad %12 %15\n"
				"%19 = OpLoad %6 %18\n"
				"%21 = OpLoad %6 %20\n"
				"%23 = OpCompositeConstruct %7 %19 %21 %22 %22\n"
				"%25 = OpAccessChain %24 %11 %13 %16\n"
				"OpStore %25 %23\n"
				"%27 = OpLoad %12 %26\n"
				"%29 = OpIEqual %28 %27 %13\n"
				"OpSelectionMerge %31 None\n"
				"OpBranchConditional %29 %30 %31\n"
				"%30 = OpLabel\n"
				"%39 = OpAccessChain %38 %36 %13\n"
				"OpStore %39 %37\n"
				"%41 = OpAccessChain %38 %36 %40\n"
				"OpStore %41 %37\n"
				"OpBranch %31\n"
				"%31 = OpLabel\n"
				"%49 = OpLoad %12 %26\n"
				"%55 = OpLoad %12 %26\n"
				"%57 = OpAccessChain %56 %54 %55 %13\n"
				"%58 = OpLoad %42 %57\n"
				"%60 = OpAccessChain %59 %48 %49 %13\n"
				"OpStore %60 %58\n"
				+ (*caseDef.geometryPointSizeSupported ?
					"%64 = OpAccessChain %61 %54 %49 %63\n"
					"%65 = OpLoad %32 %64\n"
					"%66 = OpAccessChain %62 %48 %49 %63\n"
					"OpStore %66 %65\n" : "") +
				"OpReturn\n"
				"OpFunctionEnd\n";
				programCollection.spirvAsmSources.add("tesc") << tesc << SpirVAsmBuildOptions(programCollection.usedVulkanVersion, SPIRV_VERSION_1_3);
		}

		{
			/*
				"#version 450\n"
				"#extension GL_KHR_shader_subgroup_basic: enable\n"
				"layout(isolines) in;\n"
				"layout(set = 0, binding = 2, std430) buffer Output\n"
				"{\n"
				"  uvec4 result[];\n"
				"};\n"
				"\n"
				"void main (void)\n"
				"{\n"
				"  result[gl_PrimitiveID * 2 + uint(gl_TessCoord.x + 0.5)] = uvec4(gl_SubgroupSize, gl_SubgroupInvocationID, 0, 0);\n"
				"  float pixelSize = 2.0f/1024.0f;\n"
				"  gl_Position = gl_in[0].gl_Position + gl_TessCoord.x * pixelSize / 2.0f;\n"
				#if GEOMETRY_POINT_SIZE_SUPPORTED
				"  gl_PointSize = gl_in[0].gl_PointSize;\n"
				#endif
				"}\n";
			*/

			const string pointSizeCapability = (*caseDef.geometryPointSizeSupported ? "OpCapability TessellationPointSize\n" : "");

			const string tese =
				"; SPIR - V\n"
				"; Version: 1.3\n"
				"; Generator: Khronos Glslang Reference Front End; 2\n"
				"; Bound: 67\n"
				"; Schema: 0\n"
				"OpCapability Tessellation\n"
				"OpCapability GroupNonUniform\n"
				+ pointSizeCapability +
				"%1 = OpExtInstImport \"GLSL.std.450\"\n"
				"OpMemoryModel Logical GLSL450\n"
				"OpEntryPoint TessellationEvaluation %4 \"main\" %15 %23 %33 %35 %48 %53\n"
				"OpExecutionMode %4 Isolines\n"
				"OpExecutionMode %4 SpacingEqual\n"
				"OpExecutionMode %4 VertexOrderCcw\n"
				"OpDecorate %8 ArrayStride 16\n"
				"OpMemberDecorate %9 0 Offset 0\n"
				"OpDecorate %9 BufferBlock\n"
				"OpDecorate %11 DescriptorSet 0\n"
				"OpDecorate %11 Binding 2\n"
				"OpDecorate %15 BuiltIn PrimitiveId\n"
				"OpDecorate %23 BuiltIn TessCoord\n"
				"OpDecorate %33 RelaxedPrecision\n"
				"OpDecorate %33 BuiltIn SubgroupSize\n"
				"OpDecorate %34 RelaxedPrecision\n"
				"OpDecorate %35 RelaxedPrecision\n"
				"OpDecorate %35 BuiltIn SubgroupLocalInvocationId\n"
				"OpDecorate %36 RelaxedPrecision\n"
				"OpMemberDecorate %46 0 BuiltIn Position\n"
				"OpMemberDecorate %46 1 BuiltIn PointSize\n"
				"OpMemberDecorate %46 2 BuiltIn ClipDistance\n"
				"OpMemberDecorate %46 3 BuiltIn CullDistance\n"
				"OpDecorate %46 Block\n"
				"OpMemberDecorate %49 0 BuiltIn Position\n"
				"OpMemberDecorate %49 1 BuiltIn PointSize\n"
				"OpMemberDecorate %49 2 BuiltIn ClipDistance\n"
				"OpMemberDecorate %49 3 BuiltIn CullDistance\n"
				"OpDecorate %49 Block\n"
				"%2 = OpTypeVoid\n"
				"%3 = OpTypeFunction %2\n"
				"%6 = OpTypeInt 32 0\n"
				"%7 = OpTypeVector %6 4\n"
				"%8 = OpTypeRuntimeArray %7\n"
				"%9 = OpTypeStruct %8\n"
				"%10 = OpTypePointer Uniform %9\n"
				"%11 = OpVariable %10 Uniform\n"
				"%12 = OpTypeInt 32 1\n"
				"%13 = OpConstant %12 0\n"
				"%14 = OpTypePointer Input %12\n"
				"%15 = OpVariable %14 Input\n"
				"%17 = OpConstant %12 2\n"
				"%20 = OpTypeFloat 32\n"
				"%21 = OpTypeVector %20 3\n"
				"%22 = OpTypePointer Input %21\n"
				"%23 = OpVariable %22 Input\n"
				"%24 = OpConstant %6 0\n"
				"%25 = OpTypePointer Input %20\n"
				"%28 = OpConstant %20 0.5\n"
				"%32 = OpTypePointer Input %6\n"
				"%33 = OpVariable %32 Input\n"
				"%35 = OpVariable %32 Input\n"
				"%38 = OpTypePointer Uniform %7\n"
				"%40 = OpTypePointer Function %20\n"
				"%42 = OpConstant %20 0.00195313\n"
				"%43 = OpTypeVector %20 4\n"
				"%44 = OpConstant %6 1\n"
				"%45 = OpTypeArray %20 %44\n"
				"%46 = OpTypeStruct %43 %20 %45 %45\n"
				"%47 = OpTypePointer Output %46\n"
				"%48 = OpVariable %47 Output\n"
				"%49 = OpTypeStruct %43 %20 %45 %45\n"
				"%50 = OpConstant %6 32\n"
				"%51 = OpTypeArray %49 %50\n"
				"%52 = OpTypePointer Input %51\n"
				"%53 = OpVariable %52 Input\n"
				"%54 = OpTypePointer Input %43\n"
				"%61 = OpConstant %20 2\n"
				"%65 = OpTypePointer Output %43\n"
				+ (*caseDef.geometryPointSizeSupported ?
					"%67 = OpTypePointer Input %20\n"
					"%68 = OpTypePointer Output %20\n"
					"%69 = OpConstant %12 1\n" : "") +
				"%4 = OpFunction %2 None %3\n"
				"%5 = OpLabel\n"
				"%41 = OpVariable %40 Function\n"
				"%16 = OpLoad %12 %15\n"
				"%18 = OpIMul %12 %16 %17\n"
				"%19 = OpBitcast %6 %18\n"
				"%26 = OpAccessChain %25 %23 %24\n"
				"%27 = OpLoad %20 %26\n"
				"%29 = OpFAdd %20 %27 %28\n"
				"%30 = OpConvertFToU %6 %29\n"
				"%31 = OpIAdd %6 %19 %30\n"
				"%34 = OpLoad %6 %33\n"
				"%36 = OpLoad %6 %35\n"
				"%37 = OpCompositeConstruct %7 %34 %36 %24 %24\n"
				"%39 = OpAccessChain %38 %11 %13 %31\n"
				"OpStore %39 %37\n"
				"OpStore %41 %42\n"
				"%55 = OpAccessChain %54 %53 %13 %13\n"
				"%56 = OpLoad %43 %55\n"
				"%57 = OpAccessChain %25 %23 %24\n"
				"%58 = OpLoad %20 %57\n"
				"%59 = OpLoad %20 %41\n"
				"%60 = OpFMul %20 %58 %59\n"
				"%62 = OpFDiv %20 %60 %61\n"
				"%63 = OpCompositeConstruct %43 %62 %62 %62 %62\n"
				"%64 = OpFAdd %43 %56 %63\n"
				"%66 = OpAccessChain %65 %48 %13\n"
				"OpStore %66 %64\n"
				+ (*caseDef.geometryPointSizeSupported ?
					"%70 = OpAccessChain %67 %53 %13 %69\n"
					"%71 = OpLoad %20 %70\n"
					"%72 = OpAccessChain %68 %48 %69\n"
					"OpStore %72 %71\n" : "") +
				"OpReturn\n"
				"OpFunctionEnd\n";
				programCollection.spirvAsmSources.add("tese") << tese << SpirVAsmBuildOptions(programCollection.usedVulkanVersion, SPIRV_VERSION_1_3);
		}

		{
			/*
				"#version 450\n"
				"#extension GL_KHR_shader_subgroup_basic: enable\n"
				"// Note: ${TOPOLOGY} variable is substituted manually at SPIR-V ASM level"
				"layout(${TOPOLOGY}) in;\n"
				"layout(points, max_vertices = 1) out;\n"
				"layout(set = 0, binding = 3, std430) buffer Output\n"
				"{\n"
				"  uvec4 result[];\n"
				"};\n"
				"\n"
				"void main (void)\n"
				"{\n"
				"  result[gl_PrimitiveIDIn] = uvec4(gl_SubgroupSize, gl_SubgroupInvocationID, 0, 0);\n"
				"  gl_Position = gl_in[0].gl_Position;\n"
				#if GEOMETRY_POINT_SIZE_SUPPORTED
				"  gl_PointSize = gl_in[0].gl_PointSize;\n"
				#endif
				"  EmitVertex();\n"
				"  EndPrimitive();\n"
				"}\n";
			*/

			const string pointSizeCapability = (*caseDef.geometryPointSizeSupported ? "OpCapability GeometryPointSize\n" : "");

			const string geometry =
			"; SPIR-V\n"
			"; Version: 1.3\n"
			"; Generator: Khronos Glslang Reference Front End; 1\n"
			"; Bound: 42\n"
			"; Schema: 0\n"
			"OpCapability Geometry\n"
			"OpCapability GroupNonUniform\n"
			+ pointSizeCapability +
			"%1 = OpExtInstImport \"GLSL.std.450\"\n"
			"OpMemoryModel Logical GLSL450\n"
			"OpEntryPoint Geometry %4 \"main\" %15 %18 %20 %32 %36\n"
			"OpExecutionMode %4 ${TOPOLOGY}\n"
			"OpExecutionMode %4 Invocations 1\n"
			"OpExecutionMode %4 OutputPoints\n"
			"OpExecutionMode %4 OutputVertices 1\n"
			"OpDecorate %8 ArrayStride 16\n"
			"OpMemberDecorate %9 0 Offset 0\n"
			"OpDecorate %9 BufferBlock\n"
			"OpDecorate %11 DescriptorSet 0\n"
			"OpDecorate %11 Binding 3\n"
			"OpDecorate %15 BuiltIn PrimitiveId\n"
			"OpDecorate %18 RelaxedPrecision\n"
			"OpDecorate %18 BuiltIn SubgroupSize\n"
			"OpDecorate %19 RelaxedPrecision\n"
			"OpDecorate %20 RelaxedPrecision\n"
			"OpDecorate %20 BuiltIn SubgroupLocalInvocationId\n"
			"OpDecorate %21 RelaxedPrecision\n"
			"OpMemberDecorate %30 0 BuiltIn Position\n"
			"OpMemberDecorate %30 1 BuiltIn PointSize\n"
			"OpMemberDecorate %30 2 BuiltIn ClipDistance\n"
			"OpMemberDecorate %30 3 BuiltIn CullDistance\n"
			"OpDecorate %30 Block\n"
			"OpMemberDecorate %33 0 BuiltIn Position\n"
			"OpMemberDecorate %33 1 BuiltIn PointSize\n"
			"OpMemberDecorate %33 2 BuiltIn ClipDistance\n"
			"OpMemberDecorate %33 3 BuiltIn CullDistance\n"
			"OpDecorate %33 Block\n"
			"%2 = OpTypeVoid\n"
			"%3 = OpTypeFunction %2\n"
			"%6 = OpTypeInt 32 0\n"
			"%7 = OpTypeVector %6 4\n"
			"%8 = OpTypeRuntimeArray %7\n"
			"%9 = OpTypeStruct %8\n"
			"%10 = OpTypePointer Uniform %9\n"
			"%11 = OpVariable %10 Uniform\n"
			"%12 = OpTypeInt 32 1\n"
			"%13 = OpConstant %12 0\n"
			"%14 = OpTypePointer Input %12\n"
			"%15 = OpVariable %14 Input\n"
			"%17 = OpTypePointer Input %6\n"
			"%18 = OpVariable %17 Input\n"
			"%20 = OpVariable %17 Input\n"
			"%22 = OpConstant %6 0\n"
			"%24 = OpTypePointer Uniform %7\n"
			"%26 = OpTypeFloat 32\n"
			"%27 = OpTypeVector %26 4\n"
			"%28 = OpConstant %6 1\n"
			"%29 = OpTypeArray %26 %28\n"
			"%30 = OpTypeStruct %27 %26 %29 %29\n"
			"%31 = OpTypePointer Output %30\n"
			"%32 = OpVariable %31 Output\n"
			"%33 = OpTypeStruct %27 %26 %29 %29\n"
			"%34 = OpTypeArray %33 %28\n"
			"%35 = OpTypePointer Input %34\n"
			"%36 = OpVariable %35 Input\n"
			"%37 = OpTypePointer Input %27\n"
			"%40 = OpTypePointer Output %27\n"
			+ (*caseDef.geometryPointSizeSupported ?
				"%42 = OpTypePointer Input %26\n"
				"%43 = OpTypePointer Output %26\n"
				"%44 = OpConstant %12 1\n" : "") +
			"%4 = OpFunction %2 None %3\n"
			"%5 = OpLabel\n"
			"%16 = OpLoad %12 %15\n"
			"%19 = OpLoad %6 %18\n"
			"%21 = OpLoad %6 %20\n"
			"%23 = OpCompositeConstruct %7 %19 %21 %22 %22\n"
			"%25 = OpAccessChain %24 %11 %13 %16\n"
			"OpStore %25 %23\n"
			"%38 = OpAccessChain %37 %36 %13 %13\n"
			"%39 = OpLoad %27 %38\n"
			"%41 = OpAccessChain %40 %32 %13\n"
			"OpStore %41 %39\n"
			+ (*caseDef.geometryPointSizeSupported ?
				"%45 = OpAccessChain %42 %36 %13 %44\n"
				"%46 = OpLoad %26 %45\n"
				"%47 = OpAccessChain %43 %32 %44\n"
				"OpStore %47 %46\n" : "") +
			"OpEmitVertex\n"
			"OpEndPrimitive\n"
			"OpReturn\n"
			"OpFunctionEnd\n";

			addGeometryShadersFromTemplate(geometry, SpirVAsmBuildOptions(programCollection.usedVulkanVersion, SPIRV_VERSION_1_3), programCollection.spirvAsmSources);
		}

		{
			/*
				"#version 450\n"
				"#extension GL_KHR_shader_subgroup_basic: enable\n"
				"layout(location = 0) out uvec4 data;\n"
				"void main (void)\n"
				"{\n"
				"  data = uvec4(gl_SubgroupSize, gl_SubgroupInvocationID, 0, 0);\n"
				"}\n";
			*/
			const string fragment =
			"; SPIR-V\n"
			"; Version: 1.3\n"
			"; Generator: Khronos Glslang Reference Front End; 1\n"
			"; Bound: 17\n"
			"; Schema: 0\n"
			"OpCapability Shader\n"
			"OpCapability GroupNonUniform\n"
			"%1 = OpExtInstImport \"GLSL.std.450\"\n"
			"OpMemoryModel Logical GLSL450\n"
			"OpEntryPoint Fragment %4 \"main\" %9 %11 %13\n"
			"OpExecutionMode %4 OriginUpperLeft\n"
			"OpDecorate %9 Location 0\n"
			"OpDecorate %11 RelaxedPrecision\n"
			"OpDecorate %11 Flat\n"
			"OpDecorate %11 BuiltIn SubgroupSize\n"
			"OpDecorate %12 RelaxedPrecision\n"
			"OpDecorate %13 RelaxedPrecision\n"
			"OpDecorate %13 Flat\n"
			"OpDecorate %13 BuiltIn SubgroupLocalInvocationId\n"
			"OpDecorate %14 RelaxedPrecision\n"
			"%2 = OpTypeVoid\n"
			"%3 = OpTypeFunction %2\n"
			"%6 = OpTypeInt 32 0\n"
			"%7 = OpTypeVector %6 4\n"
			"%8 = OpTypePointer Output %7\n"
			"%9 = OpVariable %8 Output\n"
			"%10 = OpTypePointer Input %6\n"
			"%11 = OpVariable %10 Input\n"
			"%13 = OpVariable %10 Input\n"
			"%15 = OpConstant %6 0\n"
			"%4 = OpFunction %2 None %3\n"
			"%5 = OpLabel\n"
			"%12 = OpLoad %6 %11\n"
			"%14 = OpLoad %6 %13\n"
			"%16 = OpCompositeConstruct %7 %12 %14 %15 %15\n"
			"OpStore %9 %16\n"
			"OpReturn\n"
			"OpFunctionEnd\n";

			programCollection.spirvAsmSources.add("fragment") << fragment << SpirVAsmBuildOptions(programCollection.usedVulkanVersion, SPIRV_VERSION_1_3);
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

	vkt::subgroups::supportedCheckShader(context, caseDef.shaderStage);
}

tcu::TestStatus noSSBOtest (Context& context, const CaseDefinition caseDef)
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

	if (VK_SHADER_STAGE_VERTEX_BIT == caseDef.shaderStage)
	{
		if ("gl_SubgroupSize" == caseDef.varName)
		{
			return makeVertexFrameBufferTest(
					   context, VK_FORMAT_R32G32B32A32_UINT, DE_NULL, 0, DE_NULL, checkVertexPipelineStagesSubgroupSize);
		}
		else if ("gl_SubgroupInvocationID" == caseDef.varName)
		{
			return makeVertexFrameBufferTest(
					   context, VK_FORMAT_R32G32B32A32_UINT, DE_NULL, 0, DE_NULL, checkVertexPipelineStagesSubgroupInvocationID);
		}
		else
		{
			return tcu::TestStatus::fail(
					   caseDef.varName + " failed (unhandled error checking case " +
					   caseDef.varName + ")!");
		}
	}
	else if ((VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT | VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT) & caseDef.shaderStage )
	{
		if ("gl_SubgroupSize" == caseDef.varName)
		{
			return makeTessellationEvaluationFrameBufferTest(
					   context, VK_FORMAT_R32G32B32A32_UINT, DE_NULL, 0, DE_NULL, checkVertexPipelineStagesSubgroupSize);
		}
		else if ("gl_SubgroupInvocationID" == caseDef.varName)
		{
			return makeTessellationEvaluationFrameBufferTest(
					   context, VK_FORMAT_R32G32B32A32_UINT, DE_NULL, 0, DE_NULL, checkVertexPipelineStagesSubgroupInvocationID);
		}
		else
		{
			return tcu::TestStatus::fail(
					caseDef.varName + " failed (unhandled error checking case " +
					caseDef.varName + ")!");
		}
	}
	else if (VK_SHADER_STAGE_GEOMETRY_BIT & caseDef.shaderStage )
	{
		if ("gl_SubgroupSize" == caseDef.varName)
		{
			return makeGeometryFrameBufferTest(
				    context, VK_FORMAT_R32G32B32A32_UINT, DE_NULL, 0, DE_NULL, checkVertexPipelineStagesSubgroupSize);
		}
		else if ("gl_SubgroupInvocationID" == caseDef.varName)
		{
			return makeGeometryFrameBufferTest(
					context, VK_FORMAT_R32G32B32A32_UINT, DE_NULL, 0, DE_NULL, checkVertexPipelineStagesSubgroupInvocationID);
		}
		else
		{
			return tcu::TestStatus::fail(
					caseDef.varName + " failed (unhandled error checking case " +
					caseDef.varName + ")!");
		}
	}
	else
	{
		TCU_THROW(InternalError, "Unhandled shader stage");
	}
}


tcu::TestStatus test(Context& context, const CaseDefinition caseDef)
{
	if (VK_SHADER_STAGE_COMPUTE_BIT == caseDef.shaderStage)
	{
		if (!areSubgroupOperationsSupportedForStage(context, caseDef.shaderStage))
		{
			return tcu::TestStatus::fail(
					   "Shader stage " + getShaderStageName(caseDef.shaderStage) +
					   " is required to support subgroup operations!");
		}

		if ("gl_SubgroupSize" == caseDef.varName)
		{
			if (caseDef.requiredSubgroupSize == DE_FALSE)
				return makeComputeTest(context, VK_FORMAT_R32G32B32A32_UINT, DE_NULL, 0, DE_NULL, checkComputeSubgroupSize);

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
				tcu::TestStatus result = subgroups::makeComputeTest(context, VK_FORMAT_R32_UINT, DE_NULL, 0, DE_NULL, checkComputeSubgroupSize,
																		size, VK_PIPELINE_SHADER_STAGE_CREATE_REQUIRE_FULL_SUBGROUPS_BIT_EXT);
				if (result.getCode() != QP_TEST_RESULT_PASS)
				{
					log << tcu::TestLog::Message << "subgroupSize " << size << " failed" << tcu::TestLog::EndMessage;
					return result;
				}
			}

			return tcu::TestStatus::pass("OK");
		}
		else if ("gl_SubgroupInvocationID" == caseDef.varName)
		{
			if (caseDef.requiredSubgroupSize == DE_FALSE)
				return makeComputeTest(context, VK_FORMAT_R32G32B32A32_UINT, DE_NULL, 0, DE_NULL, checkComputeSubgroupInvocationID);

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
				tcu::TestStatus result = subgroups::makeComputeTest(context, VK_FORMAT_R32_UINT, DE_NULL, 0, DE_NULL, checkComputeSubgroupInvocationID,
																		size, VK_PIPELINE_SHADER_STAGE_CREATE_REQUIRE_FULL_SUBGROUPS_BIT_EXT);
				if (result.getCode() != QP_TEST_RESULT_PASS)
				{
					log << tcu::TestLog::Message << "subgroupSize " << size << " failed" << tcu::TestLog::EndMessage;
					return result;
				}
			}

			return tcu::TestStatus::pass("OK");
		}
		else if ("gl_NumSubgroups" == caseDef.varName)
		{
			if (caseDef.requiredSubgroupSize == DE_FALSE)
				return makeComputeTest(context, VK_FORMAT_R32G32B32A32_UINT, DE_NULL, 0, DE_NULL, checkComputeNumSubgroups);

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
				tcu::TestStatus result = subgroups::makeComputeTest(context, VK_FORMAT_R32_UINT, DE_NULL, 0, DE_NULL, checkComputeNumSubgroups,
																		size, VK_PIPELINE_SHADER_STAGE_CREATE_REQUIRE_FULL_SUBGROUPS_BIT_EXT);
				if (result.getCode() != QP_TEST_RESULT_PASS)
				{
					log << tcu::TestLog::Message << "subgroupSize " << size << " failed" << tcu::TestLog::EndMessage;
					return result;
				}
			}

			return tcu::TestStatus::pass("OK");
		}
		else if ("gl_SubgroupID" == caseDef.varName)
		{
			if (caseDef.requiredSubgroupSize == DE_FALSE)
				return makeComputeTest(context, VK_FORMAT_R32G32B32A32_UINT, DE_NULL, 0, DE_NULL, checkComputeSubgroupID);

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
				tcu::TestStatus result = subgroups::makeComputeTest(context, VK_FORMAT_R32_UINT, DE_NULL, 0, DE_NULL, checkComputeSubgroupID,
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
			return tcu::TestStatus::fail(
					caseDef.varName + " failed (unhandled error checking case " +
					caseDef.varName + ")!");
		}
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

		if (VK_SHADER_STAGE_FRAGMENT_BIT != stages && !subgroups::isVertexSSBOSupportedForDevice(context))
		{
			if ( (stages & VK_SHADER_STAGE_FRAGMENT_BIT) == 0)
				TCU_THROW(NotSupportedError, "Device does not support vertex stage SSBO writes");
			else
				stages = VK_SHADER_STAGE_FRAGMENT_BIT;
		}

		if ((VkShaderStageFlagBits)0u == stages)
			TCU_THROW(NotSupportedError, "Subgroup operations are not supported for any graphic shader");

		if ("gl_SubgroupSize" == caseDef.varName)
		{
			return subgroups::allStages(context, VK_FORMAT_R32G32B32A32_UINT, DE_NULL, 0, DE_NULL, checkVertexPipelineStagesSubgroupSize, stages);
		}
		else if ("gl_SubgroupInvocationID" == caseDef.varName)
		{
			return subgroups::allStages(context, VK_FORMAT_R32G32B32A32_UINT, DE_NULL, 0, DE_NULL, checkVertexPipelineStagesSubgroupInvocationID, stages);
		}
		else
		{
			return tcu::TestStatus::fail(
					   caseDef.varName + " failed (unhandled error checking case " +
					   caseDef.varName + ")!");
		}
	}
}

tcu::TestCaseGroup* createSubgroupsBuiltinVarTests(tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> graphicGroup(new tcu::TestCaseGroup(
		testCtx, "graphics", "Subgroup builtin variable tests: graphics"));
	de::MovePtr<tcu::TestCaseGroup> computeGroup(new tcu::TestCaseGroup(
		testCtx, "compute", "Subgroup builtin variable tests: compute"));
	de::MovePtr<tcu::TestCaseGroup> framebufferGroup(new tcu::TestCaseGroup(
		testCtx, "framebuffer", "Subgroup builtin variable tests: framebuffer"));

	const char* const all_stages_vars[] =
	{
		"SubgroupSize",
		"SubgroupInvocationID"
	};

	const char* const compute_only_vars[] =
	{
		"NumSubgroups",
		"SubgroupID"
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
			const CaseDefinition caseDef = { "gl_" + var, VK_SHADER_STAGE_ALL_GRAPHICS, de::SharedPtr<bool>(new bool), DE_FALSE};

			addFunctionCaseWithPrograms(graphicGroup.get(),
										varLower, "",
										supportedCheck, initPrograms, test, caseDef);
		}

		{
			CaseDefinition caseDef = {"gl_" + var, VK_SHADER_STAGE_COMPUTE_BIT, de::SharedPtr<bool>(new bool), DE_FALSE};
			addFunctionCaseWithPrograms(computeGroup.get(),
						varLower + "_" + getShaderStageName(caseDef.shaderStage), "",
						supportedCheck, initPrograms, test, caseDef);
			caseDef.requiredSubgroupSize = DE_TRUE;
			addFunctionCaseWithPrograms(computeGroup.get(),
						varLower + "_" + getShaderStageName(caseDef.shaderStage) + "_requiredsubgroupsize", "",
						supportedCheck, initPrograms, test, caseDef);
		}

		for (int stageIndex = 0; stageIndex < DE_LENGTH_OF_ARRAY(stages); ++stageIndex)
		{
			const CaseDefinition caseDef = {"gl_" + var, stages[stageIndex], de::SharedPtr<bool>(new bool), DE_FALSE};
			addFunctionCaseWithPrograms(framebufferGroup.get(),
						varLower + "_" + getShaderStageName(caseDef.shaderStage), "",
						supportedCheck, initFrameBufferPrograms, noSSBOtest, caseDef);
		}
	}

	for (int a = 0; a < DE_LENGTH_OF_ARRAY(compute_only_vars); ++a)
	{
		const std::string var = compute_only_vars[a];

		CaseDefinition caseDef = {"gl_" + var, VK_SHADER_STAGE_COMPUTE_BIT, de::SharedPtr<bool>(new bool), DE_FALSE};

		addFunctionCaseWithPrograms(computeGroup.get(), de::toLower(var), "",
									supportedCheck, initPrograms, test, caseDef);
		caseDef.requiredSubgroupSize = DE_TRUE;
		addFunctionCaseWithPrograms(computeGroup.get(), de::toLower(var) + "_requiredsubgroupsize", "",
									supportedCheck, initPrograms, test, caseDef);
	}

	de::MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(
		testCtx, "builtin_var", "Subgroup builtin variable tests"));

	group->addChild(graphicGroup.release());
	group->addChild(computeGroup.release());
	group->addChild(framebufferGroup.release());

	return group.release();
}

} // subgroups
} // vkt
