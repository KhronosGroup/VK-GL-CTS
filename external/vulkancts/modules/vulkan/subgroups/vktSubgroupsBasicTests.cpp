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

#include "vktSubgroupsBasicTests.hpp"
#include "vktSubgroupsTestsUtils.hpp"

#include "tcuStringTemplate.hpp"

#include <string>
#include <vector>

using namespace tcu;
using namespace std;
using namespace vk;
using namespace vkt;

namespace
{
enum OpType
{
	OPTYPE_ELECT = 0,
	OPTYPE_SUBGROUP_BARRIER,
	OPTYPE_SUBGROUP_MEMORY_BARRIER,
	OPTYPE_SUBGROUP_MEMORY_BARRIER_BUFFER,
	OPTYPE_SUBGROUP_MEMORY_BARRIER_SHARED,
	OPTYPE_SUBGROUP_MEMORY_BARRIER_IMAGE,
	OPTYPE_LAST
};

struct CaseDefinition
{
	OpType				opType;
	VkShaderStageFlags	shaderStage;
	de::SharedPtr<bool>	geometryPointSizeSupported;
	deBool				requiredSubgroupSize;
};

static const deUint32		ELECTED_VALUE		= 42u;
static const deUint32		UNELECTED_VALUE		= 13u;
static const VkDeviceSize	SHADER_BUFFER_SIZE	= 4096ull; // min(maxUniformBufferRange, maxImageDimension1D)

static bool _checkFragmentSubgroupBarriersNoSSBO (vector<const void*>	datas,
												  deUint32				width,
												  deUint32				height,
												  bool					withImage)
{
	const float* const	resultData	= reinterpret_cast<const float*>(datas[0]);

	for (deUint32 x = 0u; x < width; ++x)
	{
		for (deUint32 y = 0u; y < height; ++y)
		{
			const deUint32 ndx = (x * height + y) * 4u;

			if (!withImage && 0.0f == resultData[ndx])
			{
				return false;
			}
			else if (1.0f == resultData[ndx +2])
			{
				if(resultData[ndx] != resultData[ndx +1])
				{
					return false;
				}
			}
			else if (resultData[ndx] != resultData[ndx +3])
			{
				return false;
			}
		}
	}

	return true;
}

static bool checkFragmentSubgroupBarriersNoSSBO (const void				*internalData,
												 vector<const void*>	datas,
												 deUint32				width,
												 deUint32				height,
												 deUint32)
{
	DE_UNREF(internalData);

	return _checkFragmentSubgroupBarriersNoSSBO(datas, width, height, false);
}

static bool checkFragmentSubgroupBarriersWithImageNoSSBO (const void*			internalData,
														  vector<const void*>	datas,
														  deUint32				width,
														  deUint32				height,
														  deUint32)
{
	DE_UNREF(internalData);

	return _checkFragmentSubgroupBarriersNoSSBO(datas, width, height, true);
}

static bool checkVertexPipelineStagesSubgroupElectNoSSBO (const void*			internalData,
														  vector<const void*>	datas,
														  deUint32				width,
														  deUint32)
{
	DE_UNREF(internalData);

	const float* const	resultData			= reinterpret_cast<const float*>(datas[0]);
	float				poisonValuesFound	= 0.0f;
	float				numSubgroupsUsed	= 0.0f;

	for (deUint32 x = 0; x < width; ++x)
	{
		deUint32 val = static_cast<deUint32>(resultData[x * 2]);
		numSubgroupsUsed += resultData[x * 2 + 1];

		switch (val)
		{
			default:
				// some garbage value was found!
				return false;
			case UNELECTED_VALUE:
				break;
			case ELECTED_VALUE:
				poisonValuesFound += 1.0f;
				break;
		}
	}

	return numSubgroupsUsed == poisonValuesFound;
}

static bool checkVertexPipelineStagesSubgroupElect (const void*				internalData,
													vector<const void*>		datas,
													deUint32				width,
													deUint32,
													bool					multipleCallsPossible)
{
	DE_UNREF(internalData);

	const deUint32* const	resultData			= reinterpret_cast<const deUint32*>(datas[0]);
	deUint32				poisonValuesFound	= 0;

	for (deUint32 x = 0; x < width; ++x)
	{
		deUint32 val = resultData[x];

		switch (val)
		{
			default:
				// some garbage value was found!
				return false;
			case UNELECTED_VALUE:
				break;
			case ELECTED_VALUE:
				poisonValuesFound++;
				break;
		}
	}

	// we used an atomicly incremented counter to note how many subgroups we used for the vertex shader
	const deUint32	numSubgroupsUsed	= *reinterpret_cast<const deUint32*>(datas[1]);

	return (multipleCallsPossible ? (numSubgroupsUsed >= poisonValuesFound) : (numSubgroupsUsed == poisonValuesFound));
}

static bool checkVertexPipelineStagesSubgroupBarriers (const void*				internalData,
													   vector<const void*>		datas,
													   deUint32					width,
													   deUint32)
{
	DE_UNREF(internalData);

	const deUint32* const resultData = reinterpret_cast<const deUint32*>(datas[0]);

	// We used this SSBO to generate our unique value!
	const deUint32 ref = *reinterpret_cast<const deUint32*>(datas[3]);

	for (deUint32 x = 0; x < width; ++x)
	{
		deUint32 val = resultData[x];

		if (val != ref)
			return false;
	}

	return true;
}

static bool _checkVertexPipelineStagesSubgroupBarriersNoSSBO (vector<const void*>	datas,
															  deUint32				width,
															  bool					withImage)
{
	const float* const	resultData	= reinterpret_cast<const float*>(datas[0]);

	for (deUint32 x = 0u; x < width; ++x)
	{
		const deUint32 ndx = x*4u;
		if (!withImage && 0.0f == resultData[ndx])
		{
			return false;
		}
		else if (1.0f == resultData[ndx +2])
		{
			if(resultData[ndx] != resultData[ndx +1])
				return false;
		}
		else if (resultData[ndx] != resultData[ndx +3])
		{
			return false;
		}
	}

	return true;
}

static bool checkVertexPipelineStagesSubgroupBarriersNoSSBO (const void*			internalData,
															 vector<const void*>	datas,
															 deUint32				width,
															 deUint32)
{
	DE_UNREF(internalData);

	return _checkVertexPipelineStagesSubgroupBarriersNoSSBO(datas, width, false);
}

static bool checkVertexPipelineStagesSubgroupBarriersWithImageNoSSBO (const void*			internalData,
																	  vector<const void*>	datas,
																	  deUint32				width,
																	  deUint32)
{
	DE_UNREF(internalData);

	return _checkVertexPipelineStagesSubgroupBarriersNoSSBO(datas, width, true);
}

static bool _checkTessellationEvaluationSubgroupBarriersNoSSBO (vector<const void*>	datas,
																deUint32			width,
																deUint32,
																bool				withImage)
{
	const float* const	resultData	= reinterpret_cast<const float*>(datas[0]);

	for (deUint32 x = 0u; x < width; ++x)
	{
		const deUint32 ndx = x*4u;

		if (!withImage && 0.0f == resultData[ndx])
		{
			return false;
		}
		else if (0.0f == resultData[ndx +2] && resultData[ndx] != resultData[ndx +3])
		{
			return false;
		}
	}

	return true;
}

static bool checkTessellationEvaluationSubgroupBarriersWithImageNoSSBO (const void*			internalData,
																		vector<const void*>	datas,
																		deUint32			width,
																		deUint32			height)
{
	DE_UNREF(internalData);

	return _checkTessellationEvaluationSubgroupBarriersNoSSBO(datas, width, height, true);
}

static bool checkTessellationEvaluationSubgroupBarriersNoSSBO (const void*				internalData,
															   vector<const void*>		datas,
															   deUint32					width,
															   deUint32					height)
{
	DE_UNREF(internalData);

	return _checkTessellationEvaluationSubgroupBarriersNoSSBO (datas, width, height, false);
}

static bool checkComputeOrMeshSubgroupElect (const void*			internalData,
											 vector<const void*>	datas,
											 const deUint32			numWorkgroups[3],
											 const deUint32			localSize[3],
											 deUint32)
{
	DE_UNREF(internalData);

	return subgroups::checkComputeOrMesh(datas, numWorkgroups, localSize, 1);
}

static bool checkComputeOrMeshSubgroupBarriers (const void*			internalData,
												vector<const void*>	datas,
												const deUint32		numWorkgroups[3],
												const deUint32		localSize[3],
												deUint32)
{
	DE_UNREF(internalData);

	// We used this SSBO to generate our unique value!
	const deUint32 ref = *reinterpret_cast<const deUint32*>(datas[2]);

	return subgroups::checkComputeOrMesh(datas, numWorkgroups, localSize, ref);
}

string getOpTypeName (OpType opType)
{
	switch (opType)
	{
		case OPTYPE_ELECT:							return "subgroupElect";
		case OPTYPE_SUBGROUP_BARRIER:				return "subgroupBarrier";
		case OPTYPE_SUBGROUP_MEMORY_BARRIER:		return "subgroupMemoryBarrier";
		case OPTYPE_SUBGROUP_MEMORY_BARRIER_BUFFER:	return "subgroupMemoryBarrierBuffer";
		case OPTYPE_SUBGROUP_MEMORY_BARRIER_SHARED:	return "subgroupMemoryBarrierShared";
		case OPTYPE_SUBGROUP_MEMORY_BARRIER_IMAGE:	return "subgroupMemoryBarrierImage";
		default:									TCU_THROW(InternalError, "Unsupported op type");
	}
}

void initFrameBufferPrograms (SourceCollections& programCollection, CaseDefinition caseDef)
{
	const ShaderBuildOptions	buildOptions	(programCollection.usedVulkanVersion, SPIRV_VERSION_1_3, 0u);
	const SpirVAsmBuildOptions	buildOptionsSpr	(programCollection.usedVulkanVersion, SPIRV_VERSION_1_3);

	if (VK_SHADER_STAGE_FRAGMENT_BIT != caseDef.shaderStage)
	{
		/*
			"layout(location = 0) in vec4 in_color;\n"
			"layout(location = 0) out vec4 out_color;\n"
			"void main()\n"
			{\n"
			"	out_color = in_color;\n"
			"}\n";
		*/
		const string fragment =
			"; SPIR-V\n"
			"; Version: 1.3\n"
			"; Generator: Khronos Glslang Reference Front End; 2\n"
			"; Bound: 13\n"
			"; Schema: 0\n"
			"OpCapability Shader\n"
			"%1 = OpExtInstImport \"GLSL.std.450\"\n"
			"OpMemoryModel Logical GLSL450\n"
			"OpEntryPoint Fragment %4 \"main\" %9 %11\n"
			"OpExecutionMode %4 OriginUpperLeft\n"
			"OpDecorate %9 Location 0\n"
			"OpDecorate %11 Location 0\n"
			"%2 = OpTypeVoid\n"
			"%3 = OpTypeFunction %2\n"
			"%6 = OpTypeFloat 32\n"
			"%7 = OpTypeVector %6 4\n"
			"%8 = OpTypePointer Output %7\n"
			"%9 = OpVariable %8 Output\n"
			"%10 = OpTypePointer Input %7\n"
			"%11 = OpVariable %10 Input\n"
			"%4 = OpFunction %2 None %3\n"
			"%5 = OpLabel\n"
			"%12 = OpLoad %7 %11\n"
			"OpStore %9 %12\n"
			"OpReturn\n"
			"OpFunctionEnd\n";

		programCollection.spirvAsmSources.add("fragment") << fragment;
	}
	if (VK_SHADER_STAGE_FRAGMENT_BIT == caseDef.shaderStage)
	{
		/*
			"#version 450\n"
			"void main (void)\n"
			"{\n"
			"  vec2 uv = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);\n"
			"  gl_Position = vec4(uv * 2.0f + -1.0f, 0.0f, 1.0f);\n"
			"  gl_PointSize = 1.0f;\n"
			"}\n";
		*/
		const string vertex =
			"; SPIR-V\n"
			"; Version: 1.3\n"
			"; Generator: Khronos Glslang Reference Front End; 2\n"
			"; Bound: 44\n"
			"; Schema: 0\n"
			"OpCapability Shader\n"
			"%1 = OpExtInstImport \"GLSL.std.450\"\n"
			"OpMemoryModel Logical GLSL450\n"
			"OpEntryPoint Vertex %4 \"main\" %12 %29\n"
			"OpDecorate %12 BuiltIn VertexIndex\n"
			"OpMemberDecorate %27 0 BuiltIn Position\n"
			"OpMemberDecorate %27 1 BuiltIn PointSize\n"
			"OpMemberDecorate %27 2 BuiltIn ClipDistance\n"
			"OpMemberDecorate %27 3 BuiltIn CullDistance\n"
			"OpDecorate %27 Block\n"
			"%2 = OpTypeVoid\n"
			"%3 = OpTypeFunction %2\n"
			"%6 = OpTypeFloat 32\n"
			"%7 = OpTypeVector %6 2\n"
			"%8 = OpTypePointer Function %7\n"
			"%10 = OpTypeInt 32 1\n"
			"%11 = OpTypePointer Input %10\n"
			"%12 = OpVariable %11 Input\n"
			"%14 = OpConstant %10 1\n"
			"%16 = OpConstant %10 2\n"
			"%23 = OpTypeVector %6 4\n"
			"%24 = OpTypeInt 32 0\n"
			"%25 = OpConstant %24 1\n"
			"%26 = OpTypeArray %6 %25\n"
			"%27 = OpTypeStruct %23 %6 %26 %26\n"
			"%28 = OpTypePointer Output %27\n"
			"%29 = OpVariable %28 Output\n"
			"%30 = OpConstant %10 0\n"
			"%32 = OpConstant %6 2\n"
			"%34 = OpConstant %6 -1\n"
			"%37 = OpConstant %6 0\n"
			"%38 = OpConstant %6 1\n"
			"%42 = OpTypePointer Output %23\n"
			"%44 = OpTypePointer Output %6\n"
			"%4 = OpFunction %2 None %3\n"
			"%5 = OpLabel\n"
			"%9 = OpVariable %8 Function\n"
			"%13 = OpLoad %10 %12\n"
			"%15 = OpShiftLeftLogical %10 %13 %14\n"
			"%17 = OpBitwiseAnd %10 %15 %16\n"
			"%18 = OpConvertSToF %6 %17\n"
			"%19 = OpLoad %10 %12\n"
			"%20 = OpBitwiseAnd %10 %19 %16\n"
			"%21 = OpConvertSToF %6 %20\n"
			"%22 = OpCompositeConstruct %7 %18 %21\n"
			"OpStore %9 %22\n"
			"%31 = OpLoad %7 %9\n"
			"%33 = OpVectorTimesScalar %7 %31 %32\n"
			"%35 = OpCompositeConstruct %7 %34 %34\n"
			"%36 = OpFAdd %7 %33 %35\n"
			"%39 = OpCompositeExtract %6 %36 0\n"
			"%40 = OpCompositeExtract %6 %36 1\n"
			"%41 = OpCompositeConstruct %23 %39 %40 %37 %38\n"
			"%43 = OpAccessChain %42 %29 %30\n"
			"OpStore %43 %41\n"
			"%45 = OpAccessChain %44 %29 %14\n"
			"OpStore %45 %38\n"
			"OpReturn\n"
			"OpFunctionEnd\n";

		programCollection.spirvAsmSources.add("vert") << vertex;
	}
	else if (VK_SHADER_STAGE_VERTEX_BIT != caseDef.shaderStage)
	{
		subgroups::setVertexShaderFrameBuffer(programCollection);
	}

	if (OPTYPE_ELECT == caseDef.opType)
	{
		ostringstream electedValue ;
		ostringstream unelectedValue;

		electedValue << ELECTED_VALUE;
		unelectedValue << UNELECTED_VALUE;

		if (VK_SHADER_STAGE_VERTEX_BIT == caseDef.shaderStage)
		{
			/*
				"#extension GL_KHR_shader_subgroup_basic: enable\n"
				"layout(location = 0) out vec4 out_color;\n"
				"layout(location = 0) in highp vec4 in_position;\n"
				"\n"
				"void main (void)\n"
				"{\n"
				"  if (subgroupElect())\n"
				"  {\n"
				"    out_color.r = " << ELECTED_VALUE << ";\n"
				"    out_color.g = 1.0f;\n"
				"  }\n"
				"  else\n"
				"  {\n"
				"    out_color.r = " << UNELECTED_VALUE << ";\n"
				"    out_color.g = 0.0f;\n"
				"  }\n"
				"  gl_Position = in_position;\n"
				"  gl_PointSize = 1.0f;\n"
				"}\n";
			*/
			const string vertex =
				"; SPIR-V\n"
				"; Version: 1.3\n"
				"; Generator: Khronos Glslang Reference Front End; 2\n"
				"; Bound: 38\n"
				"; Schema: 0\n"
				"OpCapability Shader\n"
				"OpCapability GroupNonUniform\n"
				"%1 = OpExtInstImport \"GLSL.std.450\"\n"
				"OpMemoryModel Logical GLSL450\n"
				"OpEntryPoint Vertex %4 \"main\" %15 %31 %35\n"
				"OpDecorate %15 Location 0\n"
				"OpMemberDecorate %29 0 BuiltIn Position\n"
				"OpMemberDecorate %29 1 BuiltIn PointSize\n"
				"OpMemberDecorate %29 2 BuiltIn ClipDistance\n"
				"OpMemberDecorate %29 3 BuiltIn CullDistance\n"
				"OpDecorate %29 Block\n"
				"OpDecorate %35 Location 0\n"
				"%2 = OpTypeVoid\n"
				"%3 = OpTypeFunction %2\n"
				"%6 = OpTypeBool\n"
				"%7 = OpTypeInt 32 0\n"
				"%8 = OpConstant %7 3\n"
				"%12 = OpTypeFloat 32\n"
				"%13 = OpTypeVector %12 4\n"
				"%14 = OpTypePointer Output %13\n"
				"%15 = OpVariable %14 Output\n"
				"%16 = OpConstant %12 " + electedValue.str() + "\n"
				"%17 = OpConstant %7 0\n"
				"%18 = OpTypePointer Output %12\n"
				"%20 = OpConstant %12 1\n"
				"%21 = OpConstant %7 1\n"
				"%24 = OpConstant %12 " + unelectedValue.str() + "\n"
				"%26 = OpConstant %12 0\n"
				"%28 = OpTypeArray %12 %21\n"
				"%29 = OpTypeStruct %13 %12 %28 %28\n"
				"%30 = OpTypePointer Output %29\n"
				"%31 = OpVariable %30 Output\n"
				"%32 = OpTypeInt 32 1\n"
				"%33 = OpConstant %32 0\n"
				"%34 = OpTypePointer Input %13\n"
				"%35 = OpVariable %34 Input\n"
				"%38 = OpConstant %32 1\n"
				"%4 = OpFunction %2 None %3\n"
				"%5 = OpLabel\n"
				"%9 = OpGroupNonUniformElect %6 %8\n"
				"OpSelectionMerge %11 None\n"
				"OpBranchConditional %9 %10 %23\n"
				"%10 = OpLabel\n"
				"%19 = OpAccessChain %18 %15 %17\n"
				"OpStore %19 %16\n"
				"%22 = OpAccessChain %18 %15 %21\n"
				"OpStore %22 %20\n"
				"OpBranch %11\n"
				"%23 = OpLabel\n"
				"%25 = OpAccessChain %18 %15 %17\n"
				"OpStore %25 %24\n"
				"%27 = OpAccessChain %18 %15 %21\n"
				"OpStore %27 %26\n"
				"OpBranch %11\n"
				"%11 = OpLabel\n"
				"%36 = OpLoad %13 %35\n"
				"%37 = OpAccessChain %14 %31 %33\n"
				"OpStore %37 %36\n"
				"%39 = OpAccessChain %18 %31 %38\n"
				"OpStore %39 %20\n"
				"OpReturn\n"
				"OpFunctionEnd\n";

			programCollection.spirvAsmSources.add("vert") << vertex << buildOptionsSpr;
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
				"  if (subgroupElect())\n"
				"  {\n"
				"    out_color.r = " << ELECTED_VALUE << ";\n"
				"    out_color.g = 1.0f;\n"
				"  }\n"
				"  else\n"
				"  {\n"
				"    out_color.r = " << UNELECTED_VALUE << ";\n"
				"    out_color.g = 0.0f;\n"
				"  }\n"
				"  gl_Position = gl_in[0].gl_Position;\n"
				"  gl_PointSize = gl_in[0].gl_PointSize;\n"
				"  EmitVertex();\n"
				"  EndPrimitive();\n"
				"}\n";
			*/
			ostringstream geometry;

			geometry
				<< "; SPIR-V\n"
				<< "; Version: 1.3\n"
				<< "; Generator: Khronos Glslang Reference Front End; 2\n"
				<< "; Bound: 42\n"
				<< "; Schema: 0\n"
				<< "OpCapability Geometry\n"
				<< (*caseDef.geometryPointSizeSupported ?
					"OpCapability GeometryPointSize\n" : "")
				<< "OpCapability GroupNonUniform\n"
				<< "%1 = OpExtInstImport \"GLSL.std.450\"\n"
				<< "OpMemoryModel Logical GLSL450\n"
				<< "OpEntryPoint Geometry %4 \"main\" %15 %31 %37\n"
				<< "OpExecutionMode %4 InputPoints\n"
				<< "OpExecutionMode %4 Invocations 1\n"
				<< "OpExecutionMode %4 OutputPoints\n"
				<< "OpExecutionMode %4 OutputVertices 1\n"
				<< "OpDecorate %15 Location 0\n"
				<< "OpMemberDecorate %29 0 BuiltIn Position\n"
				<< "OpMemberDecorate %29 1 BuiltIn PointSize\n"
				<< "OpMemberDecorate %29 2 BuiltIn ClipDistance\n"
				<< "OpMemberDecorate %29 3 BuiltIn CullDistance\n"
				<< "OpDecorate %29 Block\n"
				<< "OpMemberDecorate %34 0 BuiltIn Position\n"
				<< "OpMemberDecorate %34 1 BuiltIn PointSize\n"
				<< "OpMemberDecorate %34 2 BuiltIn ClipDistance\n"
				<< "OpMemberDecorate %34 3 BuiltIn CullDistance\n"
				<< "OpDecorate %34 Block\n"
				<< "%2 = OpTypeVoid\n"
				<< "%3 = OpTypeFunction %2\n"
				<< "%6 = OpTypeBool\n"
				<< "%7 = OpTypeInt 32 0\n"
				<< "%8 = OpConstant %7 3\n"
				<< "%12 = OpTypeFloat 32\n"
				<< "%13 = OpTypeVector %12 4\n"
				<< "%14 = OpTypePointer Output %13\n"
				<< "%15 = OpVariable %14 Output\n"
				<< "%16 = OpConstant %12 " << electedValue.str() << "\n"
				<< "%17 = OpConstant %7 0\n"
				<< "%18 = OpTypePointer Output %12\n"
				<< "%20 = OpConstant %12 1\n"
				<< "%21 = OpConstant %7 1\n"
				<< "%24 = OpConstant %12 " << unelectedValue.str() << "\n"
				<< "%26 = OpConstant %12 0\n"
				<< "%28 = OpTypeArray %12 %21\n"
				<< "%29 = OpTypeStruct %13 %12 %28 %28\n"
				<< "%30 = OpTypePointer Output %29\n"
				<< "%31 = OpVariable %30 Output\n"
				<< "%32 = OpTypeInt 32 1\n"
				<< "%33 = OpConstant %32 0\n"
				<< "%34 = OpTypeStruct %13 %12 %28 %28\n"
				<< "%35 = OpTypeArray %34 %21\n"
				<< "%36 = OpTypePointer Input %35\n"
				<< "%37 = OpVariable %36 Input\n"
				<< "%38 = OpTypePointer Input %13\n"
				<< (*caseDef.geometryPointSizeSupported ?
					"%42 = OpConstant %32 1\n"
					"%43 = OpTypePointer Input %12\n"
					"%44 = OpTypePointer Output %12\n" : "")
				<< "%4 = OpFunction %2 None %3\n"
				<< "%5 = OpLabel\n"
				<< "%9 = OpGroupNonUniformElect %6 %8\n"
				<< "OpSelectionMerge %11 None\n"
				<< "OpBranchConditional %9 %10 %23\n"
				<< "%10 = OpLabel\n"
				<< "%19 = OpAccessChain %18 %15 %17\n"
				<< "OpStore %19 %16\n"
				<< "%22 = OpAccessChain %18 %15 %21\n"
				<< "OpStore %22 %20\n"
				<< "OpBranch %11\n"
				<< "%23 = OpLabel\n"
				<< "%25 = OpAccessChain %18 %15 %17\n"
				<< "OpStore %25 %24\n"
				<< "%27 = OpAccessChain %18 %15 %21\n"
				<< "OpStore %27 %26\n"
				<< "OpBranch %11\n"
				<< "%11 = OpLabel\n"
				<< "%39 = OpAccessChain %38 %37 %33 %33\n"
				<< "%40 = OpLoad %13 %39\n"
				<< "%41 = OpAccessChain %14 %31 %33\n"
				<< "OpStore %41 %40\n"
				<< (*caseDef.geometryPointSizeSupported ?
					"%45 = OpAccessChain %43 %37 %33 %42\n"
					"%46 = OpLoad %12 %45\n"
					"%47 = OpAccessChain %44 %31 %42\n"
					"OpStore %47 %46\n" : "" )
				<< "OpEmitVertex\n"
				<< "OpEndPrimitive\n"
				<< "OpReturn\n"
				<< "OpFunctionEnd\n";

			programCollection.spirvAsmSources.add("geometry") << geometry.str() << buildOptionsSpr;
		}
		else if (VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT == caseDef.shaderStage)
		{
			/*
				<< "#extension GL_KHR_shader_subgroup_basic: enable\n"
				<< "#extension GL_EXT_tessellation_shader : require\n"
				<< "layout(vertices = 2) out;\n"
				<< "void main (void)\n"
				<< "{\n"
				<< "  if (gl_InvocationID == 0)\n"
				<< "  {\n"
				<< "    gl_TessLevelOuter[0] = 1.0f;\n"
				<< "    gl_TessLevelOuter[1] = 1.0f;\n"
				<< "  }\n"
				<< "  gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;\n"
				<< "}\n";
			*/
			const string controlSource =
				"; SPIR-V\n"
				"; Version: 1.3\n"
				"; Generator: Khronos Glslang Reference Front End; 2\n"
				"; Bound: 46\n"
				"; Schema: 0\n"
				"OpCapability Tessellation\n"
				"%1 = OpExtInstImport \"GLSL.std.450\"\n"
				"OpMemoryModel Logical GLSL450\n"
				"OpEntryPoint TessellationControl %4 \"main\" %8 %20 %33 %39\n"
				"OpExecutionMode %4 OutputVertices 2\n"
				"OpDecorate %8 BuiltIn InvocationId\n"
				"OpDecorate %20 Patch\n"
				"OpDecorate %20 BuiltIn TessLevelOuter\n"
				"OpMemberDecorate %29 0 BuiltIn Position\n"
				"OpMemberDecorate %29 1 BuiltIn PointSize\n"
				"OpMemberDecorate %29 2 BuiltIn ClipDistance\n"
				"OpMemberDecorate %29 3 BuiltIn CullDistance\n"
				"OpDecorate %29 Block\n"
				"OpMemberDecorate %35 0 BuiltIn Position\n"
				"OpMemberDecorate %35 1 BuiltIn PointSize\n"
				"OpMemberDecorate %35 2 BuiltIn ClipDistance\n"
				"OpMemberDecorate %35 3 BuiltIn CullDistance\n"
				"OpDecorate %35 Block\n"
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
				"%27 = OpConstant %16 1\n"
				"%28 = OpTypeArray %15 %27\n"
				"%29 = OpTypeStruct %26 %15 %28 %28\n"
				"%30 = OpConstant %16 2\n"
				"%31 = OpTypeArray %29 %30\n"
				"%32 = OpTypePointer Output %31\n"
				"%33 = OpVariable %32 Output\n"
				"%35 = OpTypeStruct %26 %15 %28 %28\n"
				"%36 = OpConstant %16 32\n"
				"%37 = OpTypeArray %35 %36\n"
				"%38 = OpTypePointer Input %37\n"
				"%39 = OpVariable %38 Input\n"
				"%41 = OpTypePointer Input %26\n"
				"%44 = OpTypePointer Output %26\n"
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
				"%34 = OpLoad %6 %8\n"
				"%40 = OpLoad %6 %8\n"
				"%42 = OpAccessChain %41 %39 %40 %10\n"
				"%43 = OpLoad %26 %42\n"
				"%45 = OpAccessChain %44 %33 %34 %10\n"
				"OpStore %45 %43\n"
				"OpReturn\n"
				"OpFunctionEnd\n";

			programCollection.spirvAsmSources.add("tesc") << controlSource << buildOptionsSpr;

			/*
				"#extension GL_KHR_shader_subgroup_basic: enable\n"
				"#extension GL_EXT_tessellation_shader : require\n"
				"layout(isolines, equal_spacing, ccw ) in;\n"
				"layout(location = 0) out vec4 out_color;\n"
				"\n"
				"void main (void)\n"
				"{\n"
				"  if (subgroupElect())\n"
				"  {\n"
				"    out_color.r = " << 2 * ELECTED_VALUE - UNELECTED_VALUE << ";\n"
				"    out_color.g = 2.0f;\n"
				"  }\n"
				"  else\n"
				"  {\n"
				"    out_color.r = " << UNELECTED_VALUE << ";\n"
				"    out_color.g = 0.0f;\n"
				"  }\n"
				"  gl_Position = mix(gl_in[0].gl_Position, gl_in[1].gl_Position, gl_TessCoord.x);\n"
				"}\n";
			*/

			const string evaluationSource =
				"; SPIR-V\n"
				"; Version: 1.3\n"
				"; Generator: Khronos Glslang Reference Front End; 2\n"
				"; Bound: 54\n"
				"; Schema: 0\n"
				"OpCapability Tessellation\n"
				"OpCapability GroupNonUniform\n"
				"%1 = OpExtInstImport \"GLSL.std.450\"\n"
				"OpMemoryModel Logical GLSL450\n"
				"OpEntryPoint TessellationEvaluation %4 \"main\" %15 %31 %38 %47\n"
				"OpExecutionMode %4 Isolines\n"
				"OpExecutionMode %4 SpacingEqual\n"
				"OpExecutionMode %4 VertexOrderCcw\n"
				"OpDecorate %15 Location 0\n"
				"OpMemberDecorate %29 0 BuiltIn Position\n"
				"OpMemberDecorate %29 1 BuiltIn PointSize\n"
				"OpMemberDecorate %29 2 BuiltIn ClipDistance\n"
				"OpMemberDecorate %29 3 BuiltIn CullDistance\n"
				"OpDecorate %29 Block\n"
				"OpMemberDecorate %34 0 BuiltIn Position\n"
				"OpMemberDecorate %34 1 BuiltIn PointSize\n"
				"OpMemberDecorate %34 2 BuiltIn ClipDistance\n"
				"OpMemberDecorate %34 3 BuiltIn CullDistance\n"
				"OpDecorate %34 Block\n"
				"OpDecorate %47 BuiltIn TessCoord\n"
				"%2 = OpTypeVoid\n"
				"%3 = OpTypeFunction %2\n"
				"%6 = OpTypeBool\n"
				"%7 = OpTypeInt 32 0\n"
				"%8 = OpConstant %7 3\n"
				"%12 = OpTypeFloat 32\n"
				"%13 = OpTypeVector %12 4\n"
				"%14 = OpTypePointer Output %13\n"
				"%15 = OpVariable %14 Output\n"
				"%16 = OpConstant %12 71\n"//electedValue
				"%17 = OpConstant %7 0\n"
				"%18 = OpTypePointer Output %12\n"
				"%20 = OpConstant %12 2\n"
				"%21 = OpConstant %7 1\n"
				"%24 = OpConstant %12 " + unelectedValue.str() + "\n"
				"%26 = OpConstant %12 0\n"
				"%28 = OpTypeArray %12 %21\n"
				"%29 = OpTypeStruct %13 %12 %28 %28\n"
				"%30 = OpTypePointer Output %29\n"
				"%31 = OpVariable %30 Output\n"
				"%32 = OpTypeInt 32 1\n"
				"%33 = OpConstant %32 0\n"
				"%34 = OpTypeStruct %13 %12 %28 %28\n"
				"%35 = OpConstant %7 32\n"
				"%36 = OpTypeArray %34 %35\n"
				"%37 = OpTypePointer Input %36\n"
				"%38 = OpVariable %37 Input\n"
				"%39 = OpTypePointer Input %13\n"
				"%42 = OpConstant %32 1\n"
				"%45 = OpTypeVector %12 3\n"
				"%46 = OpTypePointer Input %45\n"
				"%47 = OpVariable %46 Input\n"
				"%48 = OpTypePointer Input %12\n"
				"%4 = OpFunction %2 None %3\n"
				"%5 = OpLabel\n"
				"%9 = OpGroupNonUniformElect %6 %8\n"
				"OpSelectionMerge %11 None\n"
				"OpBranchConditional %9 %10 %23\n"
				"%10 = OpLabel\n"
				"%19 = OpAccessChain %18 %15 %17\n"
				"OpStore %19 %16\n"
				"%22 = OpAccessChain %18 %15 %21\n"
				"OpStore %22 %20\n"
				"OpBranch %11\n"
				"%23 = OpLabel\n"
				"%25 = OpAccessChain %18 %15 %17\n"
				"OpStore %25 %24\n"
				"%27 = OpAccessChain %18 %15 %21\n"
				"OpStore %27 %26\n"
				"OpBranch %11\n"
				"%11 = OpLabel\n"
				"%40 = OpAccessChain %39 %38 %33 %33\n"
				"%41 = OpLoad %13 %40\n"
				"%43 = OpAccessChain %39 %38 %42 %33\n"
				"%44 = OpLoad %13 %43\n"
				"%49 = OpAccessChain %48 %47 %17\n"
				"%50 = OpLoad %12 %49\n"
				"%51 = OpCompositeConstruct %13 %50 %50 %50 %50\n"
				"%52 = OpExtInst %13 %1 FMix %41 %44 %51\n"
				"%53 = OpAccessChain %14 %31 %33\n"
				"OpStore %53 %52\n"
				"OpReturn\n"
				"OpFunctionEnd\n";

			programCollection.spirvAsmSources.add("tese") << evaluationSource << buildOptionsSpr;
		}
		else if (VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT == caseDef.shaderStage)
		{
			/*
				"#extension GL_KHR_shader_subgroup_basic: enable\n"
				"#extension GL_EXT_tessellation_shader : require\n"
				"layout(vertices = 2) out;\n"
				"layout(location = 0) out vec4 out_color[];\n"
				"void main (void)\n"
				"{\n"
				"  if (gl_InvocationID == 0)\n"
				"  {\n"
				"    gl_TessLevelOuter[0] = 1.0f;\n"
				"    gl_TessLevelOuter[1] = 1.0f;\n"
				"  }\n"
				"  if (subgroupElect())\n"
				"  {\n"
				"    out_color[gl_InvocationID].r = " << ELECTED_VALUE << ";\n"
				"    out_color[gl_InvocationID].g = 1.0f;\n"
				"  }\n"
				"  else\n"
				"  {\n"
				"    out_color[gl_InvocationID].r = " << UNELECTED_VALUE << ";\n"
				"    out_color[gl_InvocationID].g = 0.0f;\n"
				"  }\n"
				"  gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;\n"
				"}\n";
			*/
			const string  controlSource =
				"; SPIR-V\n"
				"; Version: 1.3\n"
				"; Generator: Khronos Glslang Reference Front End; 2\n"
				"; Bound: 66\n"
				"; Schema: 0\n"
				"OpCapability Tessellation\n"
				"OpCapability GroupNonUniform\n"
				"%1 = OpExtInstImport \"GLSL.std.450\"\n"
				"OpMemoryModel Logical GLSL450\n"
				"OpEntryPoint TessellationControl %4 \"main\" %8 %20 %34 %53 %59\n"
				"OpExecutionMode %4 OutputVertices 2\n"
				"OpDecorate %8 BuiltIn InvocationId\n"
				"OpDecorate %20 Patch\n"
				"OpDecorate %20 BuiltIn TessLevelOuter\n"
				"OpDecorate %34 Location 0\n"
				"OpMemberDecorate %50 0 BuiltIn Position\n"
				"OpMemberDecorate %50 1 BuiltIn PointSize\n"
				"OpMemberDecorate %50 2 BuiltIn ClipDistance\n"
				"OpMemberDecorate %50 3 BuiltIn CullDistance\n"
				"OpDecorate %50 Block\n"
				"OpMemberDecorate %55 0 BuiltIn Position\n"
				"OpMemberDecorate %55 1 BuiltIn PointSize\n"
				"OpMemberDecorate %55 2 BuiltIn ClipDistance\n"
				"OpMemberDecorate %55 3 BuiltIn CullDistance\n"
				"OpDecorate %55 Block\n"
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
				"%26 = OpConstant %16 3\n"
				"%30 = OpTypeVector %15 4\n"
				"%31 = OpConstant %16 2\n"
				"%32 = OpTypeArray %30 %31\n"
				"%33 = OpTypePointer Output %32\n"
				"%34 = OpVariable %33 Output\n"
				"%36 = OpConstant %15 " + electedValue.str() + "\n"
				"%37 = OpConstant %16 0\n"
				"%40 = OpConstant %16 1\n"
				"%44 = OpConstant %15 " + unelectedValue.str() + "\n"
				"%47 = OpConstant %15 0\n"
				"%49 = OpTypeArray %15 %40\n"
				"%50 = OpTypeStruct %30 %15 %49 %49\n"
				"%51 = OpTypeArray %50 %31\n"
				"%52 = OpTypePointer Output %51\n"
				"%53 = OpVariable %52 Output\n"
				"%55 = OpTypeStruct %30 %15 %49 %49\n"
				"%56 = OpConstant %16 32\n"
				"%57 = OpTypeArray %55 %56\n"
				"%58 = OpTypePointer Input %57\n"
				"%59 = OpVariable %58 Input\n"
				"%61 = OpTypePointer Input %30\n"
				"%64 = OpTypePointer Output %30\n"
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
				"%27 = OpGroupNonUniformElect %11 %26\n"
				"OpSelectionMerge %29 None\n"
				"OpBranchConditional %27 %28 %42\n"
				"%28 = OpLabel\n"
				"%35 = OpLoad %6 %8\n"
				"%38 = OpAccessChain %22 %34 %35 %37\n"
				"OpStore %38 %36\n"
				"%39 = OpLoad %6 %8\n"
				"%41 = OpAccessChain %22 %34 %39 %40\n"
				"OpStore %41 %21\n"
				"OpBranch %29\n"
				"%42 = OpLabel\n"
				"%43 = OpLoad %6 %8\n"
				"%45 = OpAccessChain %22 %34 %43 %37\n"
				"OpStore %45 %44\n"
				"%46 = OpLoad %6 %8\n"
				"%48 = OpAccessChain %22 %34 %46 %40\n"
				"OpStore %48 %47\n"
				"OpBranch %29\n"
				"%29 = OpLabel\n"
				"%54 = OpLoad %6 %8\n"
				"%60 = OpLoad %6 %8\n"
				"%62 = OpAccessChain %61 %59 %60 %10\n"
				"%63 = OpLoad %30 %62\n"
				"%65 = OpAccessChain %64 %53 %54 %10\n"
				"OpStore %65 %63\n"
				"OpReturn\n"
				"OpFunctionEnd\n";

			programCollection.spirvAsmSources.add("tesc") << controlSource << buildOptionsSpr;

			/*
				"#extension GL_KHR_shader_subgroup_ballot: enable\n"
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

			const string evaluationSource =
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
		else
			TCU_THROW(InternalError, "Unsupported shader stage");
	}
	else
	{
		const string	color = (VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT == caseDef.shaderStage) ? "out_color[gl_InvocationID].b = 1.0f;\n" : "out_color.b = 1.0f;\n";
		ostringstream	bdy;

		switch (caseDef.opType)
		{
			case OPTYPE_SUBGROUP_BARRIER:
			case OPTYPE_SUBGROUP_MEMORY_BARRIER:
			case OPTYPE_SUBGROUP_MEMORY_BARRIER_BUFFER:
			{
				bdy << " tempResult2 = tempBuffer[id];\n"
					<< "  if (subgroupElect())\n"
					<< "  {\n"
					<< "    tempResult = value;\n"
					<< "    " << color
					<< "  }\n"
					 << "  else\n"
					<< "  {\n"
					<< "    tempResult = tempBuffer[id];\n"
					<< "  }\n"
					<< "  " << getOpTypeName(caseDef.opType) << "();\n";
				break;
			}

			case OPTYPE_SUBGROUP_MEMORY_BARRIER_IMAGE:
				bdy <<"tempResult2 = imageLoad(tempImage, ivec2(id, 0)).x;\n"
					<< "  if (subgroupElect())\n"
					<< "  {\n"
					<< "    tempResult = value;\n"
					<< "     " << color
					<< "  }\n"
					<< "  else\n"
					<< "  {\n"
					<< "    tempResult = imageLoad(tempImage, ivec2(id, 0)).x;\n"
					<< "  }\n"
					<< "  subgroupMemoryBarrierImage();\n";
				break;

			default:
				TCU_THROW(InternalError, "Unhandled op type");
		}

		if (VK_SHADER_STAGE_FRAGMENT_BIT == caseDef.shaderStage)
		{
			ostringstream	fragment;

			fragment	<< glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450)<<"\n"
				<< "#extension GL_KHR_shader_subgroup_basic: enable\n"
				<< "#extension GL_KHR_shader_subgroup_ballot: enable\n"
				<< "layout(location = 0) out vec4 out_color;\n"
				<< "\n"
				<< "layout(set = 0, binding = 0) uniform Buffer1\n"
				<< "{\n"
				<< "  uint tempBuffer["<<SHADER_BUFFER_SIZE/4ull<<"];\n"
				<< "};\n"
				<< "\n"
				<< "layout(set = 0, binding = 1) uniform Buffer2\n"
				<< "{\n"
				<< "  uint value;\n"
				<< "};\n"
				<< (OPTYPE_SUBGROUP_MEMORY_BARRIER_IMAGE == caseDef.opType ? "layout(set = 0, binding = 2, r32ui) readonly uniform highp uimage2D tempImage;\n" : "\n")
				<< "void main (void)\n"
				<< "{\n"
				<< "  if (gl_HelperInvocation) return;\n"
				<< "  uint id = 0;\n"
				<< "  if (subgroupElect())\n"
				<< "  {\n"
				<< "    id = uint(gl_FragCoord.x);\n"
				<< "  }\n"
				<< "  id = subgroupBroadcastFirst(id);\n"
				<< "  uint localId = id;\n"
				<< "  uint tempResult = 0u;\n"
				<< "  uint tempResult2 = 0u;\n"
				<< "  out_color.b = 0.0f;\n"
				<< bdy.str()
				<< "  out_color.r = float(tempResult);\n"
				<< "  out_color.g = float(value);\n"
				<< "  out_color.a = float(tempResult2);\n"
				<< "}\n";

			programCollection.glslSources.add("fragment") << glu::FragmentSource(fragment.str()) << buildOptions;
		}
		else if (VK_SHADER_STAGE_VERTEX_BIT == caseDef.shaderStage)
		{
			ostringstream	vertex;

			vertex	<< glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450)<<"\n"
				<< "#extension GL_KHR_shader_subgroup_basic: enable\n"
				<< "#extension GL_KHR_shader_subgroup_ballot: enable\n"
				<<"\n"
				<< "layout(location = 0) out vec4 out_color;\n"
				<< "layout(location = 0) in highp vec4 in_position;\n"
				<< "\n"
				<< "layout(set = 0, binding = 0) uniform Buffer1\n"
				<< "{\n"
				<< "  uint tempBuffer["<<SHADER_BUFFER_SIZE/4ull<<"];\n"
				<< "};\n"
				<< "\n"
				<< "layout(set = 0, binding = 1) uniform Buffer2\n"
				<< "{\n"
				<< "  uint value;\n"
				<< "};\n"
				<< (OPTYPE_SUBGROUP_MEMORY_BARRIER_IMAGE == caseDef.opType ? "layout(set = 0, binding = 2, r32ui) readonly uniform highp uimage2D tempImage;\n" : "\n")
				<< "void main (void)\n"
				<< "{\n"
				<< "  uint id = 0;\n"
				<< "  if (subgroupElect())\n"
				<< "  {\n"
				<< "    id = gl_VertexIndex;\n"
				<< "  }\n"
				<< "  id = subgroupBroadcastFirst(id);\n"
				<< "  uint tempResult = 0u;\n"
				<< "  uint tempResult2 = 0u;\n"
				<< "  out_color.b = 0.0f;\n"
				<< bdy.str()
				<< "  out_color.r = float(tempResult);\n"
				<< "  out_color.g = float(value);\n"
				<< "  out_color.a = float(tempResult2);\n"
				<< "  gl_Position = in_position;\n"
				<< "  gl_PointSize = 1.0f;\n"
				<< "}\n";

			programCollection.glslSources.add("vert") << glu::VertexSource(vertex.str()) << buildOptions;
		}
		else if (VK_SHADER_STAGE_GEOMETRY_BIT == caseDef.shaderStage)
		{
			ostringstream geometry;

			geometry << "#version 450\n"
					<< "#extension GL_KHR_shader_subgroup_ballot: enable\n"
					<< "#extension GL_KHR_shader_subgroup_basic: enable\n"
					<< "layout(points) in;\n"
					<< "layout(points, max_vertices = 1) out;\n"
					<< "layout(location = 0) out vec4 out_color;\n"
					<< "layout(set = 0, binding = 0) uniform Buffer1\n"
					<< "{\n"
					<< "  uint tempBuffer["<<SHADER_BUFFER_SIZE/4ull<<"];\n"
					<< "};\n"
					<< "\n"
					<< "layout(set = 0, binding = 1) uniform Buffer2\n"
					<< "{\n"
					<< "  uint value;\n"
					<< "};\n"
					<< (OPTYPE_SUBGROUP_MEMORY_BARRIER_IMAGE == caseDef.opType ? "layout(set = 0, binding = 2, r32ui) readonly uniform highp uimage2D tempImage;\n" : "\n")
					<< "void main (void)\n"
					<< "{\n"
					<< "  uint id = 0;\n"
					<< "  if (subgroupElect())\n"
					<< "  {\n"
					<< "    id = gl_InvocationID;\n"
					<< "  }\n"
					<< "  id = subgroupBroadcastFirst(id);\n"
					<< "  uint tempResult = 0u;\n"
					<< "  uint tempResult2 = 0u;\n"
					<< "  out_color.b = 0.0f;\n"
					<< bdy.str()
					<< "  out_color.r = float(tempResult);\n"
					<< "  out_color.g = float(value);\n"
					<< "  out_color.a = float(tempResult2);\n"
					<< "  gl_Position = gl_in[0].gl_Position;\n"
					<< (*caseDef.geometryPointSizeSupported ? "  gl_PointSize = gl_in[0].gl_PointSize;\n" : "" )
					<< "  EmitVertex();\n"
					<< "  EndPrimitive();\n"
					<< "}\n";

			programCollection.glslSources.add("geometry") << glu::GeometrySource(geometry.str()) << buildOptions;
		}
		else if (VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT == caseDef.shaderStage)
		{
			ostringstream controlSource;
			ostringstream evaluationSource;

			controlSource << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450)<<"\n"
				<< "#extension GL_EXT_tessellation_shader : require\n"
				<< "layout(vertices = 2) out;\n"
				<< "void main (void)\n"
				<< "{\n"
				<< "  if (gl_InvocationID == 0)\n"
				<<"  {\n"
				<< "    gl_TessLevelOuter[0] = 1.0f;\n"
				<< "    gl_TessLevelOuter[1] = 1.0f;\n"
				<< "  }\n"
				<< "  gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;\n"
				<< (*caseDef.geometryPointSizeSupported ? "  gl_out[gl_InvocationID].gl_PointSize = gl_in[gl_InvocationID].gl_PointSize;\n" : "" )
				<< "}\n";

			evaluationSource << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450)<<"\n"
				<< "#extension GL_KHR_shader_subgroup_basic: enable\n"
				<< "#extension GL_KHR_shader_subgroup_ballot: enable\n"
				<< "#extension GL_EXT_tessellation_shader : require\n"
				<< "layout(isolines, equal_spacing, ccw ) in;\n"
				<< "layout(location = 0) out vec4 out_color;\n"
				<< "layout(set = 0, binding = 0) uniform Buffer1\n"
				<< "{\n"
				<< "  uint tempBuffer["<<SHADER_BUFFER_SIZE/4ull<<"];\n"
				<< "};\n"
				<< "\n"
				<< "layout(set = 0, binding = 1) uniform Buffer2\n"
				<< "{\n"
				<< "  uint value;\n"
				<< "};\n"
				<< (OPTYPE_SUBGROUP_MEMORY_BARRIER_IMAGE == caseDef.opType ? "layout(set = 0, binding = 2, r32ui) readonly uniform highp uimage2D tempImage;\n" : "\n")
				<< "void main (void)\n"
				<< "{\n"
				<< "  uint id = 0;\n"
				<< "  if (subgroupElect())\n"
				<< "  {\n"
				<< "    id = gl_PrimitiveID;\n"
				<< "  }\n"
				<< "  id = subgroupBroadcastFirst(id);\n"
				<< "  uint tempResult = 0u;\n"
				<< "  uint tempResult2 = 0u;\n"
				<< "  out_color.b = 0.0f;\n"
				<< bdy.str()
				<< "  out_color.r = float(tempResult);\n"
				<< "  out_color.g = float(value);\n"
				<< "  out_color.a = float(tempResult2);\n"
				<< "  gl_Position = mix(gl_in[0].gl_Position, gl_in[1].gl_Position, gl_TessCoord.x);\n"
				<< (*caseDef.geometryPointSizeSupported ? "  gl_PointSize = gl_in[0].gl_PointSize;\n" : "" )
				<< "}\n";

			programCollection.glslSources.add("tesc") << glu::TessellationControlSource(controlSource.str()) << buildOptions;
			programCollection.glslSources.add("tese") << glu::TessellationEvaluationSource(evaluationSource.str()) << buildOptions;
		}
		else if (VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT == caseDef.shaderStage)
		{
			ostringstream controlSource;
			ostringstream evaluationSource;

			controlSource  << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450)<<"\n"
				<< "#extension GL_KHR_shader_subgroup_basic: enable\n"
				<< "#extension GL_KHR_shader_subgroup_ballot: enable\n"
				<< "#extension GL_EXT_tessellation_shader : require\n"
				<< "layout(vertices = 2) out;\n"
				<< "layout(location = 0) out vec4 out_color[];\n"
				<< "layout(set = 0, binding = 0) uniform Buffer1\n"
				<< "{\n"
				<< "  uint tempBuffer["<<SHADER_BUFFER_SIZE/4ull<<"];\n"
				<< "};\n"
				<< "\n"
				<< "layout(set = 0, binding = 1) uniform Buffer2\n"
				<< "{\n"
				<< "  uint value;\n"
				<< "};\n"
				<< (OPTYPE_SUBGROUP_MEMORY_BARRIER_IMAGE == caseDef.opType ? "layout(set = 0, binding = 2, r32ui) readonly uniform highp uimage2D tempImage;\n" : "\n")
				<< "void main (void)\n"
				<< "{\n"
				<< "  uint id = 0;\n"
				<< "  if (gl_InvocationID == 0)\n"
				<<"  {\n"
				<< "    gl_TessLevelOuter[0] = 1.0f;\n"
				<< "    gl_TessLevelOuter[1] = 1.0f;\n"
				<< "  }\n"
				<< "  if (subgroupElect())\n"
				<< "  {\n"
				<< "    id = gl_InvocationID;\n"
				<< "  }\n"
				<< "  id = subgroupBroadcastFirst(id);\n"
				<< "  uint tempResult = 0u;\n"
				<< "  uint tempResult2 = 0u;\n"
				<< "  out_color[gl_InvocationID].b = 0.0f;\n"
				<< bdy.str()
				<< "  out_color[gl_InvocationID].r = float(tempResult);\n"
				<< "  out_color[gl_InvocationID].g = float(value);\n"
				<< "  out_color[gl_InvocationID].a = float(tempResult2);\n"
				<< "  gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;\n"
				<< (*caseDef.geometryPointSizeSupported ? "  gl_out[gl_InvocationID].gl_PointSize = gl_in[gl_InvocationID].gl_PointSize;\n" : "" )
				<< "}\n";

			evaluationSource << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450)<<"\n"
				<< "#extension GL_KHR_shader_subgroup_ballot: enable\n"
				<< "#extension GL_EXT_tessellation_shader : require\n"
				<< "layout(isolines, equal_spacing, ccw ) in;\n"
				<< "layout(location = 0) in vec4 in_color[];\n"
				<< "layout(location = 0) out vec4 out_color;\n"
				<< "\n"
				<< "void main (void)\n"
				<< "{\n"
				<< "  gl_Position = mix(gl_in[0].gl_Position, gl_in[1].gl_Position, gl_TessCoord.x);\n"
				<< (*caseDef.geometryPointSizeSupported ? "  gl_PointSize = gl_in[0].gl_PointSize;\n" : "" )
				<< "  out_color = in_color[0];\n"
				<< "}\n";

			programCollection.glslSources.add("tesc") << glu::TessellationControlSource(controlSource.str()) << buildOptions;
			programCollection.glslSources.add("tese") << glu::TessellationEvaluationSource(evaluationSource.str()) << buildOptions;
		}
		else
			TCU_THROW(InternalError, "Unsupported shader stage");
	}
}

vector<string> getPerStageHeadDeclarations (const CaseDefinition& caseDef)
{
	const deUint32	stageCount	= subgroups::getStagesCount(caseDef.shaderStage);
	const bool		fragment	= (caseDef.shaderStage & VK_SHADER_STAGE_FRAGMENT_BIT) != 0;
	vector<string>	result		(stageCount, string());

	if (fragment)
		result.resize(result.size() + 1);

	if (caseDef.opType == OPTYPE_ELECT)
	{
		for (size_t i = 0; i < result.size(); ++i)
		{
			const bool		frag		= (i == stageCount);
			const size_t	binding1	= i;
			const size_t	binding2	= stageCount + i;

			if (frag)
			{
				result[i] += "layout(location = 0) out uint result;\n";
			}
			else
			{
				result[i] +=
					"layout(set = 0, binding = " + de::toString(binding1) + ", std430) buffer Buffer1\n"
					"{\n"
					"  uint result[];\n"
					"};\n";
			}

			result[i] +=
				"layout(set = 0, binding = " + de::toString(binding2) + ", std430) buffer Buffer2\n"
				"{\n"
				"  uint numSubgroupsExecuted;\n"
				"};\n";
		}
	}
	else
	{
		for (size_t i = 0; i < result.size(); ++i)
		{
			const bool		frag		= (i == stageCount);
			const size_t	binding1	= i;
			const size_t	binding2	= stageCount + 4 * i;
			const size_t	binding3	= stageCount + 4 * i + 1;
			const size_t	binding4	= stageCount + 4 * i + 2;
			const size_t	binding5	= stageCount + 4 * i + 3;

			if (frag)
			{
				result[i] = "layout(location = 0) out uint result;\n";
			}
			else
			{
				result[i] +=
					"layout(set = 0, binding = " + de::toString(binding1) + ", std430) buffer Buffer1\n"
					"{\n"
					"  uint result[];\n"
					"};\n";
			}

			result[i] +=
				"layout(set = 0, binding = " + de::toString(binding2) + ", std430) buffer Buffer2\n"
				"{\n"
				"  uint tempBuffer[];\n"
				"};\n"
				"layout(set = 0, binding = " + de::toString(binding3) + ", std430) buffer Buffer3\n"
				"{\n"
				"  uint subgroupID;\n"
				"};\n"
				"layout(set = 0, binding = " + de::toString(binding4) + ", std430) buffer Buffer4\n"
				"{\n"
				"  uint value;\n"
				"};\n"
				"layout(set = 0, binding = " + de::toString(binding5) + ", r32ui) uniform uimage2D tempImage;\n";
		}
	}

	return result;
}

string getTestString (const CaseDefinition& caseDef)
{
	stringstream	body;

#ifndef CTS_USES_VULKANSC
	if (caseDef.opType != OPTYPE_ELECT && (isAllGraphicsStages(caseDef.shaderStage) || isAllRayTracingStages(caseDef.shaderStage)))
#else
	if (caseDef.opType != OPTYPE_ELECT && (isAllGraphicsStages(caseDef.shaderStage)))
#endif // CTS_USES_VULKANSC
	{
		body << "  uint id = 0;\n"
				"  if (subgroupElect())\n"
				"  {\n"
				"    id = atomicAdd(subgroupID, 1);\n"
				"  }\n"
				"  id = subgroupBroadcastFirst(id);\n"
				"  uint localId = id;\n"
				"  uint tempResult = 0;\n";
	}

	switch (caseDef.opType)
	{
		case OPTYPE_ELECT:
			if (isAllComputeStages(caseDef.shaderStage))
			{
				body << "  uint value = " << UNELECTED_VALUE << ";\n"
						"  if (subgroupElect())\n"
						"  {\n"
						"    value = " << ELECTED_VALUE << ";\n"
						"  }\n"
						"  uvec4 bits = bitCount(sharedMemoryBallot(value == " << ELECTED_VALUE << "));\n"
						"  tempRes = bits.x + bits.y + bits.z + bits.w;\n";
			}
			else
			{
				body << "  if (subgroupElect())\n"
						"  {\n"
						"    tempRes = " << ELECTED_VALUE << ";\n"
						"    atomicAdd(numSubgroupsExecuted, 1);\n"
						"  }\n"
						"  else\n"
						"  {\n"
						"    tempRes = " << UNELECTED_VALUE << ";\n"
						"  }\n";
			}
			break;

		case OPTYPE_SUBGROUP_BARRIER:
		case OPTYPE_SUBGROUP_MEMORY_BARRIER:
		case OPTYPE_SUBGROUP_MEMORY_BARRIER_BUFFER:
			body << "  if (subgroupElect())\n"
					"  {\n"
					"    tempBuffer[id] = value;\n"
					"  }\n"
					"  " << getOpTypeName(caseDef.opType) << "();\n"
					"  tempResult = tempBuffer[id];\n";
			break;

		case OPTYPE_SUBGROUP_MEMORY_BARRIER_SHARED:
			body <<  "  if (subgroupElect())\n"
					"  {\n"
					"    tempShared[localId] = value;\n"
					"  }\n"
					"  subgroupMemoryBarrierShared();\n"
					"  tempResult = tempShared[localId];\n";
			break;

		case OPTYPE_SUBGROUP_MEMORY_BARRIER_IMAGE:
			body << "  if (subgroupElect())\n"
					"  {\n"
					"    imageStore(tempImage, ivec2(id, 0), ivec4(value));\n"
					"  }\n"
					"  subgroupMemoryBarrierImage();\n"
					"  tempResult = imageLoad(tempImage, ivec2(id, 0)).x;\n";
			break;

		default:
			TCU_THROW(InternalError, "Unhandled op type!");
	}

#ifndef CTS_USES_VULKANSC
	if (caseDef.opType != OPTYPE_ELECT && (isAllGraphicsStages(caseDef.shaderStage) || isAllRayTracingStages(caseDef.shaderStage)))
#else
	if (caseDef.opType != OPTYPE_ELECT && (isAllGraphicsStages(caseDef.shaderStage)))
#endif // CTS_USES_VULKANSC
	{
		body << "  tempRes = tempResult;\n";
	}

	return body.str();
}

string getExtHeader (const CaseDefinition& caseDef)
{
	const string	extensions	= (caseDef.opType == OPTYPE_ELECT)
								? "#extension GL_KHR_shader_subgroup_basic: enable\n"
								: "#extension GL_KHR_shader_subgroup_basic: enable\n"
								  "#extension GL_KHR_shader_subgroup_ballot: enable\n";
	return extensions;
}

void initComputeOrMeshPrograms (SourceCollections&			programCollection,
								CaseDefinition&				caseDef,
								const string&				extensions,
								const string&				testSrc,
								const ShaderBuildOptions&	buildOptions)
{
	std::ostringstream electTemplateStream;
	electTemplateStream
		<< "#version 450\n"
		<< extensions
		<< "${EXTENSIONS:opt}"
		<< "layout (local_size_x_id = 0, local_size_y_id = 1, local_size_z_id = 2) in;\n"
		<< "${LAYOUTS:opt}"
		<< "layout (set = 0, binding = 0, std430) buffer Buffer1\n"
		<< "{\n"
		<< "  uint result[];\n"
		<< "};\n"
		<< "\n"
		<< subgroups::getSharedMemoryBallotHelper()
		<< "void main (void)\n"
		<< "{\n"
		<< "  uvec3 globalSize = gl_NumWorkGroups * gl_WorkGroupSize;\n"
		<< "  highp uint offset = globalSize.x * ((globalSize.y * gl_GlobalInvocationID.z) + gl_GlobalInvocationID.y) + gl_GlobalInvocationID.x;\n"
		<< "  uint value = " << UNELECTED_VALUE << ";\n"
		<< "  if (subgroupElect())\n"
		<< "  {\n"
		<< "    value = " << ELECTED_VALUE << ";\n"
		<< "  }\n"
		<< "  uvec4 bits = bitCount(sharedMemoryBallot(value == " << ELECTED_VALUE << "));\n"
		<< "  result[offset] = bits.x + bits.y + bits.z + bits.w;\n"
		<< "${BODY:opt}"
		<< "}\n";
		;
	const tcu::StringTemplate electTemplate(electTemplateStream.str());

	std::ostringstream nonElectTemplateStream;
	nonElectTemplateStream
		<< "#version 450\n"
		<< "#extension GL_KHR_shader_subgroup_basic: enable\n"
		<< "${EXTENSIONS:opt}"
		<< "layout (local_size_x_id = 0, local_size_y_id = 1, local_size_z_id = 2) in;\n"
		<< "${LAYOUTS:opt}"
		<< "layout (set = 0, binding = 0, std430) buffer Buffer1\n"
		<< "{\n"
		<< "  uint result[];\n"
		<< "};\n"
		<< "layout (set = 0, binding = 1, std430) buffer Buffer2\n"
		<< "{\n"
		<< "  uint tempBuffer[];\n"
		<< "};\n"
		<< "layout (set = 0, binding = 2, std430) buffer Buffer3\n"
		<< "{\n"
		<< "  uint value;\n"
		<< "};\n"
		<< "layout (set = 0, binding = 3, r32ui) uniform uimage2D tempImage;\n"
		<< "shared uint tempShared[gl_WorkGroupSize.x * gl_WorkGroupSize.y * gl_WorkGroupSize.z];\n"
		<< "\n"
		<< "void main (void)\n"
		<< "{\n"
		<< "  uvec3 globalSize = gl_NumWorkGroups * gl_WorkGroupSize;\n"
		<< "  highp uint offset = globalSize.x * ((globalSize.y * gl_GlobalInvocationID.z) + gl_GlobalInvocationID.y) + gl_GlobalInvocationID.x;\n"
		<< "  uint localId = gl_SubgroupID;\n"
		<< "  uint id = globalSize.x * ((globalSize.y * gl_WorkGroupID.z) + gl_WorkGroupID.y) + gl_WorkGroupID.x + localId;\n"
		<< "  uint tempResult = 0;\n"
		<< testSrc
		<< "  result[offset] = tempResult;\n"
		<< "${BODY:opt}"
		<< "}\n";
		;
	const tcu::StringTemplate nonElectTemplate(nonElectTemplateStream.str());

	if (isAllComputeStages(caseDef.shaderStage))
	{
		const std::map<std::string, std::string> emptyMap;

		if (OPTYPE_ELECT == caseDef.opType)
		{
			const auto programSource = electTemplate.specialize(emptyMap);
			programCollection.glslSources.add("comp") << glu::ComputeSource(programSource) << buildOptions;
		}
		else
		{
			const auto programSource = nonElectTemplate.specialize(emptyMap);
			programCollection.glslSources.add("comp") << glu::ComputeSource(programSource) << buildOptions;
		}
	}
#ifndef CTS_USES_VULKANSC
	else if (isAllMeshShadingStages(caseDef.shaderStage))
	{
		const bool			testMesh = ((caseDef.shaderStage & VK_SHADER_STAGE_MESH_BIT_EXT) != 0u);
		const bool			testTask = ((caseDef.shaderStage & VK_SHADER_STAGE_TASK_BIT_EXT) != 0u);
		const tcu::UVec3	emitSize = (testMesh ? tcu::UVec3(1u, 1u, 1u) : tcu::UVec3(0u, 0u, 0u));

		const std::map<std::string, std::string> meshMap
		{
			std::make_pair("EXTENSIONS",	"#extension GL_EXT_mesh_shader : enable\n"),
			std::make_pair("LAYOUTS",		"layout (points) out;\nlayout (max_vertices=1, max_primitives=1) out;\n"),
			std::make_pair("BODY",			"  SetMeshOutputsEXT(0u, 0u);\n")
		};

		const std::map<std::string, std::string> taskMap
		{
			std::make_pair("EXTENSIONS",	"#extension GL_EXT_mesh_shader : enable\n"),
			std::make_pair("BODY",			"  EmitMeshTasksEXT(" + std::to_string(emitSize.x()) + ", " + std::to_string(emitSize.y()) + ", " + std::to_string(emitSize.z()) + ");\n")
		};

		if (testMesh)
		{
			if (OPTYPE_ELECT == caseDef.opType)
			{
				const auto programSource = electTemplate.specialize(meshMap);
				programCollection.glslSources.add("mesh") << glu::MeshSource(programSource) << buildOptions;
			}
			else
			{
				const auto programSource = nonElectTemplate.specialize(meshMap);
				programCollection.glslSources.add("mesh") << glu::MeshSource(programSource) << buildOptions;
			}
		}
		else
		{
			const std::string meshShaderNoSubgroups =
				"#version 450\n"
				"#extension GL_EXT_mesh_shader : enable\n"
				"\n"
				"layout (local_size_x = 1, local_size_y = 1, local_size_z = 1) in;\n"
				"layout (points) out;\n"
				"layout (max_vertices = 1, max_primitives = 1) out;\n"
				"\n"
				"void main (void)\n"
				"{\n"
				"  SetMeshOutputsEXT(0u, 0u);\n"
				"}\n"
				;
			programCollection.glslSources.add("mesh") << glu::MeshSource(meshShaderNoSubgroups) << buildOptions;
		}

		if (testTask)
		{
			if (OPTYPE_ELECT == caseDef.opType)
			{
				const auto programSource = electTemplate.specialize(taskMap);
				programCollection.glslSources.add("task") << glu::TaskSource(programSource) << buildOptions;
			}
			else
			{
				const auto programSource = nonElectTemplate.specialize(taskMap);
				programCollection.glslSources.add("task") << glu::TaskSource(programSource) << buildOptions;
			}
		}
	}
#endif // CTS_USES_VULKANSC
	else
	{
		DE_ASSERT(false);
	}
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
	const string				testSrc				= getTestString(caseDef);
	const vector<string>		headDeclarations	= getPerStageHeadDeclarations(caseDef);
	const bool					pointSizeSupport	= *caseDef.geometryPointSizeSupported;
	const bool					isComp				= isAllComputeStages(caseDef.shaderStage);
#ifndef CTS_USES_VULKANSC
	const bool					isMesh				= isAllMeshShadingStages(caseDef.shaderStage);
#else
	const bool					isMesh				= false;
#endif // CTS_USES_VULKANSC

	if (isComp || isMesh)
		initComputeOrMeshPrograms(programCollection, caseDef, extHeader, testSrc, buildOptions);
	else
		subgroups::initStdPrograms(programCollection, buildOptions, caseDef.shaderStage, VK_FORMAT_R32_UINT, pointSizeSupport, extHeader, testSrc, "", headDeclarations, true);
}

void supportedCheck (Context& context, CaseDefinition caseDef)
{
	if (!subgroups::isSubgroupSupported(context))
		TCU_THROW(NotSupportedError, "Subgroup operations are not supported");

	if (!subgroups::isSubgroupFeatureSupportedForDevice(context, VK_SUBGROUP_FEATURE_BASIC_BIT))
		TCU_FAIL("supportedOperations will have the VK_SUBGROUP_FEATURE_BASIC_BIT bit set if any of the physical device's queues support VK_QUEUE_GRAPHICS_BIT or VK_QUEUE_COMPUTE_BIT.");

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

	subgroups::supportedCheckShader(context, caseDef.shaderStage);

	if (OPTYPE_ELECT != caseDef.opType && VK_SHADER_STAGE_COMPUTE_BIT != caseDef.shaderStage)
	{
		if (!subgroups::isSubgroupFeatureSupportedForDevice(context, VK_SUBGROUP_FEATURE_BALLOT_BIT))
		{
			TCU_THROW(NotSupportedError, "Subgroup basic operation non-compute stage test required that ballot operations are supported!");
		}
	}

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
}

TestStatus noSSBOtest (Context& context, const CaseDefinition caseDef)
{
	const deUint32				inputDatasCount	= OPTYPE_SUBGROUP_MEMORY_BARRIER_IMAGE == caseDef.opType ? 3u : 2u;
	vector<subgroups::SSBOData>	inputDatas		(inputDatasCount);

	inputDatas[0].format = VK_FORMAT_R32_UINT;
	inputDatas[0].layout = subgroups::SSBOData::LayoutStd140;
	inputDatas[0].numElements = SHADER_BUFFER_SIZE/4ull;
	inputDatas[0].initializeType = subgroups::SSBOData::InitializeNonZero;
	inputDatas[0].bindingType = subgroups::SSBOData::BindingUBO;

	inputDatas[1].format = VK_FORMAT_R32_UINT;
	inputDatas[1].layout = subgroups::SSBOData::LayoutStd140;
	inputDatas[1].numElements = 1ull;
	inputDatas[1].initializeType = subgroups::SSBOData::InitializeNonZero;
	inputDatas[1].bindingType = subgroups::SSBOData::BindingUBO;

	if(OPTYPE_SUBGROUP_MEMORY_BARRIER_IMAGE == caseDef.opType )
	{
		inputDatas[2].format = VK_FORMAT_R32_UINT;
		inputDatas[2].layout = subgroups::SSBOData::LayoutPacked;
		inputDatas[2].numElements = SHADER_BUFFER_SIZE;
		inputDatas[2].initializeType = subgroups::SSBOData::InitializeNone;
		inputDatas[2].bindingType = subgroups::SSBOData::BindingImage;
	}

	if (VK_SHADER_STAGE_VERTEX_BIT == caseDef.shaderStage)
	{
		if (OPTYPE_ELECT == caseDef.opType)
			return subgroups::makeVertexFrameBufferTest(context, VK_FORMAT_R32G32_SFLOAT, DE_NULL, 0u, DE_NULL, checkVertexPipelineStagesSubgroupElectNoSSBO);
		else
			return subgroups::makeVertexFrameBufferTest(context, VK_FORMAT_R32G32B32A32_SFLOAT, &inputDatas[0], inputDatasCount, DE_NULL,
				(OPTYPE_SUBGROUP_MEMORY_BARRIER_IMAGE == caseDef.opType) ?
					checkVertexPipelineStagesSubgroupBarriersWithImageNoSSBO :
					checkVertexPipelineStagesSubgroupBarriersNoSSBO
			);
	}
	else if (VK_SHADER_STAGE_FRAGMENT_BIT == caseDef.shaderStage)
	{
		return subgroups::makeFragmentFrameBufferTest(context, VK_FORMAT_R32G32B32A32_SFLOAT, &inputDatas[0], inputDatasCount, DE_NULL,
			(OPTYPE_SUBGROUP_MEMORY_BARRIER_IMAGE == caseDef.opType) ?
				checkFragmentSubgroupBarriersWithImageNoSSBO :
				checkFragmentSubgroupBarriersNoSSBO
		);
	}
	else if (VK_SHADER_STAGE_GEOMETRY_BIT == caseDef.shaderStage)
	{
		if (OPTYPE_ELECT == caseDef.opType)
			return subgroups::makeGeometryFrameBufferTest(context, VK_FORMAT_R32G32_SFLOAT, DE_NULL, 0u, DE_NULL, checkVertexPipelineStagesSubgroupElectNoSSBO);
		else
			return subgroups::makeGeometryFrameBufferTest(context, VK_FORMAT_R32G32B32A32_SFLOAT, &inputDatas[0], inputDatasCount, DE_NULL,
				(OPTYPE_SUBGROUP_MEMORY_BARRIER_IMAGE == caseDef.opType) ?
					checkVertexPipelineStagesSubgroupBarriersWithImageNoSSBO :
					checkVertexPipelineStagesSubgroupBarriersNoSSBO
			);
	}

	if (OPTYPE_ELECT == caseDef.opType)
		return subgroups::makeTessellationEvaluationFrameBufferTest(context, VK_FORMAT_R32G32_SFLOAT, DE_NULL, 0u, DE_NULL, checkVertexPipelineStagesSubgroupElectNoSSBO, caseDef.shaderStage);

	return subgroups::makeTessellationEvaluationFrameBufferTest(context, VK_FORMAT_R32G32B32A32_SFLOAT, &inputDatas[0], inputDatasCount, DE_NULL,
		(VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT == caseDef.shaderStage) ?
			((OPTYPE_SUBGROUP_MEMORY_BARRIER_IMAGE == caseDef.opType) ?
				checkVertexPipelineStagesSubgroupBarriersWithImageNoSSBO :
				checkVertexPipelineStagesSubgroupBarriersNoSSBO) :
			((OPTYPE_SUBGROUP_MEMORY_BARRIER_IMAGE == caseDef.opType) ?
				checkTessellationEvaluationSubgroupBarriersWithImageNoSSBO :
				checkTessellationEvaluationSubgroupBarriersNoSSBO),
		caseDef.shaderStage);
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

		if (OPTYPE_ELECT == caseDef.opType)
		{
			if (caseDef.requiredSubgroupSize == DE_FALSE)
			{
				if (isCompute)
					return subgroups::makeComputeTest(context, VK_FORMAT_R32_UINT, DE_NULL, 0, DE_NULL, checkComputeOrMeshSubgroupElect);
				else
					return subgroups::makeMeshTest(context, VK_FORMAT_R32_UINT, nullptr, 0, nullptr, checkComputeOrMeshSubgroupElect);
			}

			log << TestLog::Message << "Testing required subgroup size range [" <<  subgroupSizeControlProperties.minSubgroupSize << ", "
				<< subgroupSizeControlProperties.maxSubgroupSize << "]" << TestLog::EndMessage;

			// According to the spec, requiredSubgroupSize must be a power-of-two integer.
			for (deUint32 size = subgroupSizeControlProperties.minSubgroupSize; size <= subgroupSizeControlProperties.maxSubgroupSize; size *= 2)
			{
				TestStatus result (QP_TEST_RESULT_INTERNAL_ERROR, "Internal Error");

				if (isCompute)
					result = subgroups::makeComputeTest(context, VK_FORMAT_R32_UINT, DE_NULL, 0u, DE_NULL, checkComputeOrMeshSubgroupElect, size);
				else
					result = subgroups::makeMeshTest(context, VK_FORMAT_R32_UINT, nullptr, 0u, nullptr, checkComputeOrMeshSubgroupElect, size);

				if (result.getCode() != QP_TEST_RESULT_PASS)
				{
					log << TestLog::Message << "subgroupSize " << size << " failed" << TestLog::EndMessage;
					return result;
				}
			}

			return TestStatus::pass("OK");
		}
		else
		{
			const deUint32				inputDatasCount					= 3;
			const subgroups::SSBOData	inputDatas[inputDatasCount]		=
			{
				{
					subgroups::SSBOData::InitializeNone,	//  InputDataInitializeType		initializeType;
					subgroups::SSBOData::LayoutStd430,		//  InputDataLayoutType			layout;
					VK_FORMAT_R32_UINT,						//  vk::VkFormat				format;
					SHADER_BUFFER_SIZE,						//  vk::VkDeviceSize			numElements;
				},
				{
					subgroups::SSBOData::InitializeNonZero,	//  InputDataInitializeType		initializeType;
					subgroups::SSBOData::LayoutStd430,		//  InputDataLayoutType			layout;
					VK_FORMAT_R32_UINT,						//  vk::VkFormat				format;
					1,										//  vk::VkDeviceSize			numElements;
				},
				{
					subgroups::SSBOData::InitializeNone,	//  InputDataInitializeType		initializeType;
					subgroups::SSBOData::LayoutPacked,		//  InputDataLayoutType			layout;
					VK_FORMAT_R32_UINT,						//  vk::VkFormat				format;
					SHADER_BUFFER_SIZE,						//  vk::VkDeviceSize			numElements;
					subgroups::SSBOData::BindingImage,		//  bool						isImage;
				},
			};

			if (caseDef.requiredSubgroupSize == DE_FALSE)
			{
				if (isCompute)
					return subgroups::makeComputeTest(context, VK_FORMAT_R32_UINT, inputDatas, inputDatasCount, DE_NULL, checkComputeOrMeshSubgroupBarriers);
				else
					return subgroups::makeMeshTest(context, VK_FORMAT_R32_UINT, inputDatas, inputDatasCount, nullptr, checkComputeOrMeshSubgroupBarriers);
			}

			log << TestLog::Message << "Testing required subgroup size range [" <<  subgroupSizeControlProperties.minSubgroupSize << ", "
				<< subgroupSizeControlProperties.maxSubgroupSize << "]" << TestLog::EndMessage;

			// According to the spec, requiredSubgroupSize must be a power-of-two integer.
			for (deUint32 size = subgroupSizeControlProperties.minSubgroupSize; size <= subgroupSizeControlProperties.maxSubgroupSize; size *= 2)
			{
				TestStatus result (QP_TEST_RESULT_INTERNAL_ERROR, "Internal Error");

				if (isCompute)
					result = subgroups::makeComputeTest(context, VK_FORMAT_R32_UINT, inputDatas, inputDatasCount, DE_NULL, checkComputeOrMeshSubgroupBarriers, size);
				else
					result = subgroups::makeMeshTest(context, VK_FORMAT_R32_UINT, inputDatas, inputDatasCount, nullptr, checkComputeOrMeshSubgroupBarriers, size);

				if (result.getCode() != QP_TEST_RESULT_PASS)
				{
					log << TestLog::Message << "subgroupSize " << size << " failed" << TestLog::EndMessage;
					return result;
				}
			}

			return TestStatus::pass("OK");
		}
	}
	else if (isAllGraphicsStages(caseDef.shaderStage))
	{
		if (!subgroups::isFragmentSSBOSupportedForDevice(context))
		{
			TCU_THROW(NotSupportedError, "Subgroup basic operation require that the fragment stage be able to write to SSBOs!");
		}

		const VkShaderStageFlags	stages	= subgroups::getPossibleGraphicsSubgroupStages(context, caseDef.shaderStage);
		const VkShaderStageFlags	stagesBits[] =
		{
			VK_SHADER_STAGE_VERTEX_BIT,
			VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,
			VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,
			VK_SHADER_STAGE_GEOMETRY_BIT,
			VK_SHADER_STAGE_FRAGMENT_BIT,
		};

		if (OPTYPE_ELECT == caseDef.opType)
		{
			const deUint32		inputCount				= DE_LENGTH_OF_ARRAY(stagesBits);
			subgroups::SSBOData	inputData[inputCount];

			for (deUint32 ndx = 0; ndx < DE_LENGTH_OF_ARRAY(stagesBits); ++ndx)
			{
				inputData[ndx]	=
				{
					subgroups::SSBOData::InitializeZero,	//  InputDataInitializeType		initializeType;
					subgroups::SSBOData::LayoutStd430,		//  InputDataLayoutType			layout;
					VK_FORMAT_R32_UINT,						//  vk::VkFormat				format;
					1,										//  vk::VkDeviceSize			numElements;
					subgroups::SSBOData::BindingSSBO,		//  bool						isImage;
					4 + ndx,								//  deUint32					binding;
					stagesBits[ndx],						//  vk::VkShaderStageFlags		stages;
				};
			}

			return subgroups::allStages(context, VK_FORMAT_R32_UINT, inputData, inputCount, DE_NULL, checkVertexPipelineStagesSubgroupElect, stages);
		}
		else
		{
			const deUint32		inputDatasCount					= DE_LENGTH_OF_ARRAY(stagesBits) * 4u;
			subgroups::SSBOData	inputDatas[inputDatasCount];

			for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(stagesBits); ++ndx)
			{
				const deUint32 index = ndx*4;

				inputDatas[index].format				= VK_FORMAT_R32_UINT;
				inputDatas[index].layout				= subgroups::SSBOData::LayoutStd430;
				inputDatas[index].numElements			= SHADER_BUFFER_SIZE;
				inputDatas[index].initializeType		= subgroups::SSBOData::InitializeNonZero;
				inputDatas[index].binding				= index + 4u;
				inputDatas[index].stages				= stagesBits[ndx];

				inputDatas[index + 1].format			= VK_FORMAT_R32_UINT;
				inputDatas[index + 1].layout			= subgroups::SSBOData::LayoutStd430;
				inputDatas[index + 1].numElements		= 1;
				inputDatas[index + 1].initializeType	= subgroups::SSBOData::InitializeZero;
				inputDatas[index + 1].binding			= index + 5u;
				inputDatas[index + 1].stages			= stagesBits[ndx];

				inputDatas[index + 2].format			= VK_FORMAT_R32_UINT;
				inputDatas[index + 2].layout			= subgroups::SSBOData::LayoutStd430;
				inputDatas[index + 2].numElements		= 1;
				inputDatas[index + 2].initializeType	= subgroups::SSBOData::InitializeNonZero;
				inputDatas[index + 2].binding			= index + 6u;
				inputDatas[index + 2].stages			= stagesBits[ndx];

				inputDatas[index + 3].format			= VK_FORMAT_R32_UINT;
				inputDatas[index + 3].layout			= subgroups::SSBOData::LayoutStd430;
				inputDatas[index + 3].numElements		= SHADER_BUFFER_SIZE;
				inputDatas[index + 3].initializeType	= subgroups::SSBOData::InitializeNone;
				inputDatas[index + 3].bindingType		= subgroups::SSBOData::BindingImage;
				inputDatas[index + 3].binding			= index + 7u;
				inputDatas[index + 3].stages			= stagesBits[ndx];
			}

			return subgroups::allStages(context, VK_FORMAT_R32_UINT, inputDatas, inputDatasCount, DE_NULL, checkVertexPipelineStagesSubgroupBarriers, stages);
		}
	}
#ifndef CTS_USES_VULKANSC
	else if (isAllRayTracingStages(caseDef.shaderStage))
	{
		const VkShaderStageFlags	stages			= subgroups::getPossibleRayTracingSubgroupStages(context, caseDef.shaderStage);
		const VkShaderStageFlags	stagesBits[]	=
		{
			VK_SHADER_STAGE_RAYGEN_BIT_KHR,
			VK_SHADER_STAGE_ANY_HIT_BIT_KHR,
			VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
			VK_SHADER_STAGE_MISS_BIT_KHR,
			VK_SHADER_STAGE_INTERSECTION_BIT_KHR,
			VK_SHADER_STAGE_CALLABLE_BIT_KHR,
		};
		const deUint32				stagesCount		= DE_LENGTH_OF_ARRAY(stagesBits);

		if (OPTYPE_ELECT == caseDef.opType)
		{
			const deUint32		inputDataCount				= stagesCount;
			subgroups::SSBOData	inputData[inputDataCount];

			for (deUint32 ndx = 0; ndx < inputDataCount; ++ndx)
			{
				inputData[ndx].format			= VK_FORMAT_R32_UINT;
				inputData[ndx].layout			= subgroups::SSBOData::LayoutStd430;
				inputData[ndx].numElements		= 1;
				inputData[ndx].initializeType	= subgroups::SSBOData::InitializeZero;
				inputData[ndx].binding			= stagesCount + ndx;
				inputData[ndx].stages			= stagesBits[ndx];
			}

			return subgroups::allRayTracingStages(context, VK_FORMAT_R32_UINT, inputData, inputDataCount, DE_NULL, checkVertexPipelineStagesSubgroupElect, stages);
		}
		else
		{
			const deUint32		datasPerStage				= 4u;
			const deUint32		inputDatasCount				= datasPerStage * stagesCount;
			subgroups::SSBOData	inputDatas[inputDatasCount];

			for (deUint32 ndx = 0; ndx < stagesCount; ++ndx)
			{
				const deUint32 index = datasPerStage * ndx;

				for (deUint32 perStageNdx = 0; perStageNdx < datasPerStage; ++perStageNdx)
				{
					inputDatas[index + perStageNdx].format		= VK_FORMAT_R32_UINT;
					inputDatas[index + perStageNdx].layout		= subgroups::SSBOData::LayoutStd430;
					inputDatas[index + perStageNdx].stages		= stagesBits[ndx];
					inputDatas[index + perStageNdx].bindingType	= subgroups::SSBOData::BindingSSBO;
				}

				inputDatas[index + 0].numElements		= SHADER_BUFFER_SIZE;
				inputDatas[index + 0].initializeType	= subgroups::SSBOData::InitializeNonZero;
				inputDatas[index + 0].binding			= index + stagesCount;

				inputDatas[index + 1].numElements		= 1;
				inputDatas[index + 1].initializeType	= subgroups::SSBOData::InitializeZero;
				inputDatas[index + 1].binding			= index + stagesCount + 1u;

				inputDatas[index + 2].numElements		= 1;
				inputDatas[index + 2].initializeType	= subgroups::SSBOData::InitializeNonZero;
				inputDatas[index + 2].binding			= index + stagesCount + 2u;

				inputDatas[index + 3].numElements		= SHADER_BUFFER_SIZE;
				inputDatas[index + 3].initializeType	= subgroups::SSBOData::InitializeNone;
				inputDatas[index + 3].bindingType		= subgroups::SSBOData::BindingImage;
				inputDatas[index + 3].binding			= index + stagesCount + 3u;
			}

			return subgroups::allRayTracingStages(context, VK_FORMAT_R32_UINT, inputDatas, inputDatasCount, DE_NULL, checkVertexPipelineStagesSubgroupBarriers, stages);
		}
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
TestCaseGroup* createSubgroupsBasicTests (TestContext& testCtx)
{
	de::MovePtr<TestCaseGroup>	group				(new TestCaseGroup(testCtx, "basic", "Subgroup basic category tests"));
	de::MovePtr<TestCaseGroup>	graphicGroup		(new TestCaseGroup(testCtx, "graphics", "Subgroup basic category tests: graphics"));
	de::MovePtr<TestCaseGroup>	computeGroup		(new TestCaseGroup(testCtx, "compute", "Subgroup basic category tests: compute"));
	de::MovePtr<TestCaseGroup>	meshGroup			(new TestCaseGroup(testCtx, "mesh", "Subgroup basic category tests: mesh shading"));
	de::MovePtr<TestCaseGroup>	framebufferGroup	(new TestCaseGroup(testCtx, "framebuffer", "Subgroup basic category tests: framebuffer"));
	de::MovePtr<TestCaseGroup>	raytracingGroup		(new TestCaseGroup(testCtx, "ray_tracing", "Subgroup basic category tests: ray tracing"));
	const VkShaderStageFlags	fbStages[]			=
	{
		VK_SHADER_STAGE_FRAGMENT_BIT,
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

	for (int opTypeIndex = 0; opTypeIndex < OPTYPE_LAST; ++opTypeIndex)
	{
		const OpType	opType	= static_cast<OpType>(opTypeIndex);
		const string	op		= de::toLower(getOpTypeName(opType));

		for (size_t groupSizeNdx = 0; groupSizeNdx < DE_LENGTH_OF_ARRAY(boolValues); ++groupSizeNdx)
		{
			const deBool			requiredSubgroupSize	= boolValues[groupSizeNdx];
			const string			testNameSuffix			= requiredSubgroupSize ? "_requiredsubgroupsize" : "";
			const CaseDefinition	caseDef					=
			{
				opType,							//  OpType				opType;
				VK_SHADER_STAGE_COMPUTE_BIT,	//  VkShaderStageFlags	shaderStage;
				de::SharedPtr<bool>(new bool),	//  de::SharedPtr<bool>	geometryPointSizeSupported;
				requiredSubgroupSize			//  deBool				requiredSubgroupSize;
			};
			const string			testName				= op + testNameSuffix;

			addFunctionCaseWithPrograms(computeGroup.get(), testName, "", supportedCheck, initPrograms, test, caseDef);
		}

#ifndef CTS_USES_VULKANSC
		for (size_t groupSizeNdx = 0; groupSizeNdx < DE_LENGTH_OF_ARRAY(boolValues); ++groupSizeNdx)
		{
			for (const auto& stage : meshStages)
			{
				const deBool			requiredSubgroupSize	= boolValues[groupSizeNdx];
				const string			testNameSuffix			= requiredSubgroupSize ? "_requiredsubgroupsize" : "";
				const CaseDefinition	caseDef					=
				{
					opType,							//  OpType				opType;
					stage,							//  VkShaderStageFlags	shaderStage;
					de::SharedPtr<bool>(new bool),	//  de::SharedPtr<bool>	geometryPointSizeSupported;
					requiredSubgroupSize			//  deBool				requiredSubgroupSize;
				};
				const string			testName				= op + testNameSuffix + "_" + getShaderStageName(stage);

				addFunctionCaseWithPrograms(meshGroup.get(), testName, "", supportedCheck, initPrograms, test, caseDef);
			}
		}
#endif // CTS_USES_VULKANSC

		if (OPTYPE_SUBGROUP_MEMORY_BARRIER_SHARED == opType)
		{
			// Shared isn't available in non compute shaders.
			continue;
		}

		{
			const CaseDefinition caseDef =
			{
				opType,							//  OpType				opType;
				VK_SHADER_STAGE_ALL_GRAPHICS,	//  VkShaderStageFlags	shaderStage;
				de::SharedPtr<bool>(new bool),	//  de::SharedPtr<bool>	geometryPointSizeSupported;
				DE_FALSE						//  deBool				requiredSubgroupSize;
			};

			addFunctionCaseWithPrograms(graphicGroup.get(), op, "", supportedCheck, initPrograms, test, caseDef);
		}

#ifndef CTS_USES_VULKANSC
		{
			const CaseDefinition caseDef =
			{
				opType,							//  OpType				opType;
				SHADER_STAGE_ALL_RAY_TRACING,	//  VkShaderStageFlags	shaderStage;
				de::SharedPtr<bool>(new bool),	//  de::SharedPtr<bool>	geometryPointSizeSupported;
				DE_FALSE						//  deBool				requiredSubgroupSize;
			};

			addFunctionCaseWithPrograms(raytracingGroup.get(), op, "", supportedCheck, initPrograms, test, caseDef);
		}
#endif // CTS_USES_VULKANSC

		for (int stageIndex = 0; stageIndex < DE_LENGTH_OF_ARRAY(fbStages); ++stageIndex)
		{
			if (OPTYPE_ELECT == opType && fbStages[stageIndex] == VK_SHADER_STAGE_FRAGMENT_BIT)
				continue;		// This is not tested. I don't know why.

			const CaseDefinition	caseDef		=
			{
				opType,							//  OpType				opType;
				fbStages[stageIndex],			//  VkShaderStageFlags	shaderStage;
				de::SharedPtr<bool>(new bool),	//  de::SharedPtr<bool>	geometryPointSizeSupported;
				DE_FALSE						//  deBool				requiredSubgroupSize;
			};
			const string			testName	= op + "_" + getShaderStageName(caseDef.shaderStage);

			addFunctionCaseWithPrograms(framebufferGroup.get(), testName, "", supportedCheck, initFrameBufferPrograms, noSSBOtest, caseDef);
		}
	}

	group->addChild(graphicGroup.release());
	group->addChild(computeGroup.release());
	group->addChild(framebufferGroup.release());
	group->addChild(raytracingGroup.release());
	group->addChild(meshGroup.release());

	return group.release();
}

} // subgroups
} // vkt
