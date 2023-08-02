/*------------------------------------------------------------------------
 * OpenGL Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2017-2019 The Khronos Group Inc.
 * Copyright (c) 2017 Codeplay Software Ltd.
 * Copyright (c) 2019 NVIDIA Corporation.
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

#include "glcSubgroupsBuiltinVarTests.hpp"
#include "glcSubgroupsTestsUtils.hpp"

#include <string>
#include <vector>

using namespace tcu;
using namespace std;

namespace glc
{
namespace subgroups
{

bool checkVertexPipelineStagesSubgroupSize(std::vector<const void*> datas,
		deUint32 width, deUint32 subgroupSize)
{
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

bool checkVertexPipelineStagesSubgroupInvocationID(std::vector<const void*> datas,
		deUint32 width, deUint32 subgroupSize)
{
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

static bool checkComputeSubgroupSize(std::vector<const void*> datas,
									 const deUint32 numWorkgroups[3], const deUint32 localSize[3],
									 deUint32 subgroupSize)
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

static bool checkComputeSubgroupInvocationID(std::vector<const void*> datas,
		const deUint32 numWorkgroups[3], const deUint32 localSize[3],
		deUint32 subgroupSize)
{
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

static bool checkComputeNumSubgroups	(std::vector<const void*>	datas,
										const deUint32				numWorkgroups[3],
										const deUint32				localSize[3],
										deUint32)
{
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

static bool checkComputeSubgroupID	(std::vector<const void*>	datas,
									const deUint32				numWorkgroups[3],
									const deUint32				localSize[3],
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
	std::string varName;
	ShaderStageFlags shaderStage;
};
}

void initFrameBufferPrograms (SourceCollections& programCollection, CaseDefinition caseDef)
{
	{
		const string fragmentGLSL =
			"${VERSION_DECL}\n"
			"precision highp int;\n"
			"layout(location = 0) in highp vec4 in_color;\n"
			"layout(location = 0) out uvec4 out_color;\n"
			"void main()\n"
			"{\n"
			"	out_color = uvec4(in_color);\n"
			"}\n";
		programCollection.add("fragment") << glu::FragmentSource(fragmentGLSL);
	}

	if (SHADER_STAGE_VERTEX_BIT != caseDef.shaderStage)
		subgroups::setVertexShaderFrameBuffer(programCollection);

	if (SHADER_STAGE_VERTEX_BIT == caseDef.shaderStage)
	{
		const string vertexGLSL =
			"${VERSION_DECL}\n"
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
		programCollection.add("vert") << glu::VertexSource(vertexGLSL);
	}
	else if (SHADER_STAGE_TESS_EVALUATION_BIT == caseDef.shaderStage)
	{
		const string controlSourceGLSL =
			"${VERSION_DECL}\n"
			"${TESS_EXTENSION}\n"
			"layout(vertices = 2) out;\n"
			"layout(location = 0) out vec4 out_color[];\n"
			"void main (void)\n"
			"{\n"
			"  if (gl_InvocationID == 0)\n"
			"  {\n"
			"    gl_TessLevelOuter[0] = 1.0f;\n"
			"    gl_TessLevelOuter[1] = 1.0f;\n"
			"  }\n"
			"  out_color[gl_InvocationID] = vec4(0.0f);\n"
			"  gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;\n"
			"}\n";
		programCollection.add("tesc") << glu::TessellationControlSource(controlSourceGLSL);

		const string evaluationSourceGLSL =
			"${VERSION_DECL}\n"
			"#extension GL_KHR_shader_subgroup_basic: enable\n"
			"${TESS_EXTENSION}\n"
			"layout(isolines, equal_spacing, ccw ) in;\n"
			"layout(location = 0) in vec4 in_color[];\n"
			"layout(location = 0) out vec4 out_color;\n"
			"\n"
			"void main (void)\n"
			"{\n"
			"  gl_Position = mix(gl_in[0].gl_Position, gl_in[1].gl_Position, gl_TessCoord.x);\n"
			"  out_color = vec4(gl_SubgroupSize, gl_SubgroupInvocationID, 0.0f, 0.0f);\n"
			"}\n";
		programCollection.add("tese") << glu::TessellationEvaluationSource(evaluationSourceGLSL);
	}
	else if (SHADER_STAGE_TESS_CONTROL_BIT == caseDef.shaderStage)
	{
		const string controlSourceGLSL =
			"${VERSION_DECL}\n"
			"${TESS_EXTENSION}\n"
			"#extension GL_KHR_shader_subgroup_basic: enable\n"
			"layout(vertices = 2) out;\n"
			"layout(location = 0) out vec4 out_color[];\n"
			"void main (void)\n"
			"{\n"
			"  if (gl_InvocationID == 0)\n"
			"  {\n"
			"    gl_TessLevelOuter[0] = 1.0f;\n"
			"    gl_TessLevelOuter[1] = 1.0f;\n"
			"  }\n"
			"  out_color[gl_InvocationID] = vec4(gl_SubgroupSize, gl_SubgroupInvocationID, 0, 0);\n"
			"  gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;\n"
			"}\n";
		programCollection.add("tesc") << glu::TessellationControlSource(controlSourceGLSL);

		const string  evaluationSourceGLSL =
			"${VERSION_DECL}\n"
			"#extension GL_KHR_shader_subgroup_basic: enable\n"
			"${TESS_EXTENSION}\n"
			"layout(isolines, equal_spacing, ccw ) in;\n"
			"layout(location = 0) in vec4 in_color[];\n"
			"layout(location = 0) out vec4 out_color;\n"
			"\n"
			"void main (void)\n"
			"{\n"
			"  gl_Position = mix(gl_in[0].gl_Position, gl_in[1].gl_Position, gl_TessCoord.x);\n"
			"  out_color = in_color[0];\n"
			"}\n";
		programCollection.add("tese") << glu::TessellationEvaluationSource(evaluationSourceGLSL);
	}
	else if (SHADER_STAGE_GEOMETRY_BIT == caseDef.shaderStage)
	{
		const string geometryGLSL =
			"${VERSION_DECL}\n"
			"#extension GL_KHR_shader_subgroup_basic: enable\n"
			"layout(points) in;\n"
			"layout(points, max_vertices = 1) out;\n"
			"layout(location = 0) out vec4 out_color;\n"
			"void main (void)\n"
			"{\n"
			"  out_color = vec4(gl_SubgroupSize, gl_SubgroupInvocationID, 0, 0);\n"
			"  gl_Position = gl_in[0].gl_Position;\n"
			"  EmitVertex();\n"
			"  EndPrimitive();\n"
			"}\n";
		programCollection.add("geometry") << glu::GeometrySource(geometryGLSL);
	}
	else
	{
		DE_FATAL("Unsupported shader stage");
	}
}

void initPrograms(SourceCollections& programCollection, CaseDefinition caseDef)
{
	if (SHADER_STAGE_COMPUTE_BIT == caseDef.shaderStage)
	{
		std::ostringstream src;

		src << "${VERSION_DECL}\n"
			<< "#extension GL_KHR_shader_subgroup_basic: enable\n"
			<< "layout (${LOCAL_SIZE_X}, ${LOCAL_SIZE_Y}, ${LOCAL_SIZE_Z}) in;\n"
			<< "layout(binding = 0, std430) buffer Output\n"
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

		programCollection.add("comp") << glu::ComputeSource(src.str());
	}
	else
	{
		{
			const string vertexGLSL =
				"${VERSION_DECL}\n"
				"#extension GL_KHR_shader_subgroup_basic: enable\n"
				"layout(binding = 0, std430) buffer Output0\n"
				"{\n"
				"  uvec4 result[];\n"
				"} b0;\n"
				"\n"
				"void main (void)\n"
				"{\n"
				"  b0.result[gl_VertexID] = uvec4(gl_SubgroupSize, gl_SubgroupInvocationID, 0, 0);\n"
				"  float pixelSize = 2.0f/1024.0f;\n"
				"  float pixelPosition = pixelSize/2.0f - 1.0f;\n"
				"  gl_Position = vec4(float(gl_VertexID) * pixelSize + pixelPosition, 0.0f, 0.0f, 1.0f);\n"
				"  gl_PointSize = 1.0f;\n"
				"}\n";
			programCollection.add("vert") << glu::VertexSource(vertexGLSL);
		}

		{
			const string tescGLSL =
				"${VERSION_DECL}\n"
				"#extension GL_KHR_shader_subgroup_basic: enable\n"
				"layout(vertices=1) out;\n"
				"layout(binding = 1, std430) buffer Output1\n"
				"{\n"
				"  uvec4 result[];\n"
				"} b1;\n"
				"\n"
				"void main (void)\n"
				"{\n"
				"  b1.result[gl_PrimitiveID] = uvec4(gl_SubgroupSize, gl_SubgroupInvocationID, 0, 0);\n"
				"  if (gl_InvocationID == 0)\n"
				"  {\n"
				"    gl_TessLevelOuter[0] = 1.0f;\n"
				"    gl_TessLevelOuter[1] = 1.0f;\n"
				"  }\n"
				"  gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;\n"
				"}\n";
			programCollection.add("tesc") << glu::TessellationControlSource(tescGLSL);
		}

		{
			const string teseGLSL =
				"${VERSION_DECL}\n"
				"#extension GL_KHR_shader_subgroup_basic: enable\n"
				"layout(isolines) in;\n"
				"layout(binding = 2, std430) buffer Output2\n"
				"{\n"
				"  uvec4 result[];\n"
				"} b2;\n"
				"\n"
				"void main (void)\n"
				"{\n"
				"  b2.result[gl_PrimitiveID * 2 + int(gl_TessCoord.x + 0.5)] = uvec4(gl_SubgroupSize, gl_SubgroupInvocationID, 0, 0);\n"
				"  float pixelSize = 2.0f/1024.0f;\n"
				"  gl_Position = gl_in[0].gl_Position + gl_TessCoord.x * pixelSize / 2.0f;\n"
				"}\n";
			programCollection.add("tese") << glu::TessellationEvaluationSource(teseGLSL);
		}

		{
			const string geometryGLSL =
				// version string is added by addGeometryShadersFromTemplate
				"#extension GL_KHR_shader_subgroup_basic: enable\n"
				"layout(${TOPOLOGY}) in;\n"
				"layout(points, max_vertices = 1) out;\n"
				"layout(binding = 3, std430) buffer Output3\n"
				"{\n"
				"  uvec4 result[];\n"
				"} b3;\n"
				"\n"
				"void main (void)\n"
				"{\n"
				"  b3.result[gl_PrimitiveIDIn] = uvec4(gl_SubgroupSize, gl_SubgroupInvocationID, 0, 0);\n"
				"  gl_Position = gl_in[0].gl_Position;\n"
				"  EmitVertex();\n"
				"  EndPrimitive();\n"
				"}\n";
			addGeometryShadersFromTemplate(geometryGLSL, programCollection);
		}

		{
			const string fragmentGLSL =
				"${VERSION_DECL}\n"
				"#extension GL_KHR_shader_subgroup_basic: enable\n"
				"precision highp int;\n"
				"layout(location = 0) out uvec4 data;\n"
				"void main (void)\n"
				"{\n"
				"  data = uvec4(gl_SubgroupSize, gl_SubgroupInvocationID, 0, 0);\n"
				"}\n";
			programCollection.add("fragment") << glu::FragmentSource(fragmentGLSL);
		}

		subgroups::addNoSubgroupShader(programCollection);
	}
}

void supportedCheck (Context& context, CaseDefinition caseDef)
{
	DE_UNREF(caseDef);
	if (!subgroups::isSubgroupSupported(context))
		TCU_THROW(NotSupportedError, "Subgroup operations are not supported");
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

	if (SHADER_STAGE_VERTEX_BIT == caseDef.shaderStage)
	{
		if ("gl_SubgroupSize" == caseDef.varName)
		{
			return makeVertexFrameBufferTest(
					   context, FORMAT_R32G32B32A32_UINT, DE_NULL, 0, checkVertexPipelineStagesSubgroupSize);
		}
		else if ("gl_SubgroupInvocationID" == caseDef.varName)
		{
			return makeVertexFrameBufferTest(
					   context, FORMAT_R32G32B32A32_UINT, DE_NULL, 0, checkVertexPipelineStagesSubgroupInvocationID);
		}
		else
		{
			return tcu::TestStatus::fail(
					   caseDef.varName + " failed (unhandled error checking case " +
					   caseDef.varName + ")!");
		}
	}
	else if ((SHADER_STAGE_TESS_EVALUATION_BIT | SHADER_STAGE_TESS_CONTROL_BIT) & caseDef.shaderStage )
	{
		if ("gl_SubgroupSize" == caseDef.varName)
		{
			return makeTessellationEvaluationFrameBufferTest(
					context, FORMAT_R32G32B32A32_UINT, DE_NULL, 0, checkVertexPipelineStagesSubgroupSize);
		}
		else if ("gl_SubgroupInvocationID" == caseDef.varName)
		{
			return makeTessellationEvaluationFrameBufferTest(
					context, FORMAT_R32G32B32A32_UINT, DE_NULL, 0, checkVertexPipelineStagesSubgroupInvocationID);
		}
		else
		{
			return tcu::TestStatus::fail(
					caseDef.varName + " failed (unhandled error checking case " +
					caseDef.varName + ")!");
		}
	}
	else if (SHADER_STAGE_GEOMETRY_BIT & caseDef.shaderStage )
	{
		if ("gl_SubgroupSize" == caseDef.varName)
		{
			return makeGeometryFrameBufferTest(
					context, FORMAT_R32G32B32A32_UINT, DE_NULL, 0, checkVertexPipelineStagesSubgroupSize);
		}
		else if ("gl_SubgroupInvocationID" == caseDef.varName)
		{
			return makeGeometryFrameBufferTest(
					context, FORMAT_R32G32B32A32_UINT, DE_NULL, 0, checkVertexPipelineStagesSubgroupInvocationID);
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
	if (SHADER_STAGE_COMPUTE_BIT == caseDef.shaderStage)
	{
		if (!areSubgroupOperationsSupportedForStage(context, caseDef.shaderStage))
		{
			return tcu::TestStatus::fail(
					   "Shader stage " + getShaderStageName(caseDef.shaderStage) +
					   " is required to support subgroup operations!");
		}

		if ("gl_SubgroupSize" == caseDef.varName)
		{
			return makeComputeTest(context, FORMAT_R32G32B32A32_UINT, DE_NULL, 0, checkComputeSubgroupSize);
		}
		else if ("gl_SubgroupInvocationID" == caseDef.varName)
		{
			return makeComputeTest(context, FORMAT_R32G32B32A32_UINT, DE_NULL, 0, checkComputeSubgroupInvocationID);
		}
		else if ("gl_NumSubgroups" == caseDef.varName)
		{
			return makeComputeTest(context, FORMAT_R32G32B32A32_UINT, DE_NULL, 0, checkComputeNumSubgroups);
		}
		else if ("gl_SubgroupID" == caseDef.varName)
		{
			return makeComputeTest(context, FORMAT_R32G32B32A32_UINT, DE_NULL, 0, checkComputeSubgroupID);
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
		int supportedStages = context.getDeqpContext().getContextInfo().getInt(GL_SUBGROUP_SUPPORTED_STAGES_KHR);

		subgroups::ShaderStageFlags stages = (subgroups::ShaderStageFlags)(caseDef.shaderStage & supportedStages);

		if (SHADER_STAGE_FRAGMENT_BIT != stages && !subgroups::isVertexSSBOSupportedForDevice(context))
		{
			if ( (stages & SHADER_STAGE_FRAGMENT_BIT) == 0)
				TCU_THROW(NotSupportedError, "Device does not support vertex stage SSBO writes");
			else
				stages = SHADER_STAGE_FRAGMENT_BIT;
		}

		if ((ShaderStageFlags)0u == stages)
			TCU_THROW(NotSupportedError, "Subgroup operations are not supported for any graphic shader");

		if ("gl_SubgroupSize" == caseDef.varName)
		{
			return subgroups::allStages(context, FORMAT_R32G32B32A32_UINT, DE_NULL, 0, checkVertexPipelineStagesSubgroupSize, stages);
		}
		else if ("gl_SubgroupInvocationID" == caseDef.varName)
		{
			return subgroups::allStages(context, FORMAT_R32G32B32A32_UINT, DE_NULL, 0, checkVertexPipelineStagesSubgroupInvocationID, stages);
		}
		else
		{
			return tcu::TestStatus::fail(
					   caseDef.varName + " failed (unhandled error checking case " +
					   caseDef.varName + ")!");
		}
	}
}

deqp::TestCaseGroup* createSubgroupsBuiltinVarTests(deqp::Context& testCtx)
{
	de::MovePtr<deqp::TestCaseGroup> graphicGroup(new deqp::TestCaseGroup(
		testCtx, "graphics", "Subgroup builtin variable tests: graphics"));
	de::MovePtr<deqp::TestCaseGroup> computeGroup(new deqp::TestCaseGroup(
		testCtx, "compute", "Subgroup builtin variable tests: compute"));
	de::MovePtr<deqp::TestCaseGroup> framebufferGroup(new deqp::TestCaseGroup(
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

	const ShaderStageFlags stages[] =
	{
		SHADER_STAGE_VERTEX_BIT,
		SHADER_STAGE_TESS_EVALUATION_BIT,
		SHADER_STAGE_TESS_CONTROL_BIT,
		SHADER_STAGE_GEOMETRY_BIT,
	};

	for (int a = 0; a < DE_LENGTH_OF_ARRAY(all_stages_vars); ++a)
	{
		const std::string var = all_stages_vars[a];
		const std::string varLower = de::toLower(var);

		{
			const CaseDefinition caseDef = { "gl_" + var, SHADER_STAGE_ALL_GRAPHICS};

			SubgroupFactory<CaseDefinition>::addFunctionCaseWithPrograms(graphicGroup.get(),
										varLower, "",
										supportedCheck, initPrograms, test, caseDef);
		}

		{
			const CaseDefinition caseDef = {"gl_" + var, SHADER_STAGE_COMPUTE_BIT};
			SubgroupFactory<CaseDefinition>::addFunctionCaseWithPrograms(computeGroup.get(),
						varLower + "_" + getShaderStageName(caseDef.shaderStage), "",
						supportedCheck, initPrograms, test, caseDef);
		}

		for (int stageIndex = 0; stageIndex < DE_LENGTH_OF_ARRAY(stages); ++stageIndex)
		{
			const CaseDefinition caseDef = {"gl_" + var, stages[stageIndex]};
			SubgroupFactory<CaseDefinition>::addFunctionCaseWithPrograms(framebufferGroup.get(),
						varLower + "_" + getShaderStageName(caseDef.shaderStage), "",
						supportedCheck, initFrameBufferPrograms, noSSBOtest, caseDef);
		}
	}

	for (int a = 0; a < DE_LENGTH_OF_ARRAY(compute_only_vars); ++a)
	{
		const std::string var = compute_only_vars[a];

		const CaseDefinition caseDef = {"gl_" + var, SHADER_STAGE_COMPUTE_BIT};

		SubgroupFactory<CaseDefinition>::addFunctionCaseWithPrograms(computeGroup.get(), de::toLower(var), "",
									supportedCheck, initPrograms, test, caseDef);
	}

	de::MovePtr<deqp::TestCaseGroup> group(new deqp::TestCaseGroup(
		testCtx, "builtin_var", "Subgroup builtin variable tests"));

	group->addChild(graphicGroup.release());
	group->addChild(computeGroup.release());
	group->addChild(framebufferGroup.release());

	return group.release();
}

} // subgroups
} // glc
