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

#include "vktSubgroupsVoteTests.hpp"
#include "vktSubgroupsTestsUtils.hpp"

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
	OPTYPE_ALL = 0,
	OPTYPE_ANY,
	OPTYPE_ALLEQUAL,
	OPTYPE_LAST
};

static bool checkVertexPipelineStages(std::vector<const void*> datas,
									  deUint32 width, deUint32)
{
	const deUint32* data =
		reinterpret_cast<const deUint32*>(datas[0]);
	for (deUint32 x = 0; x < width; ++x)
	{
		deUint32 val = data[x];

		if (0x1F != val)
		{
			return false;
		}
	}

	return true;
}

static bool checkFragmentPipelineStages(std::vector<const void*> datas,
									  deUint32 width, deUint32 height, deUint32)
{
	const deUint32* data =
		reinterpret_cast<const deUint32*>(datas[0]);
	for (deUint32 x = 0u; x < width; ++x)
	{
		for (deUint32 y = 0u; y < height; ++y)
		{
			const deUint32 ndx = (x * height + y);
			deUint32 val = data[ndx] & 0x1F;

			if (data[ndx] & 0x40) //Helper fragment shader invocation was executed
			{
				if(val != 0x1F)
					return false;
			}
			else //Helper fragment shader invocation was not executed yet
			{
				if (val != 0x1E)
					return false;
			}
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

							// The data should look (in binary) 0b111
							if (0x1F != data[offset])
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

static bool checkComputeAllEqual(std::vector<const void*> datas,
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

							// The data should look (in binary) 0b11111
							if (0x1F != data[offset])
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

std::string getOpTypeName(int opType)
{
	switch (opType)
	{
		default:
			DE_FATAL("Unsupported op type");
			return "";
		case OPTYPE_ALL:
			return "subgroupAll";
		case OPTYPE_ANY:
			return "subgroupAny";
		case OPTYPE_ALLEQUAL:
			return "subgroupAllEqual";
	}
}

struct CaseDefinition
{
	int					opType;
	VkShaderStageFlags	shaderStage;
	VkFormat			format;
};

void initFrameBufferPrograms (SourceCollections& programCollection, CaseDefinition caseDef)
{
	const vk::ShaderBuildOptions buildOptions	(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_3, 0u);
	const bool formatIsBoolean =
		VK_FORMAT_R8_USCALED == caseDef.format || VK_FORMAT_R8G8_USCALED == caseDef.format || VK_FORMAT_R8G8B8_USCALED == caseDef.format || VK_FORMAT_R8G8B8A8_USCALED == caseDef.format;

	if (VK_SHADER_STAGE_FRAGMENT_BIT != caseDef.shaderStage)
		subgroups::setFragmentShaderFrameBuffer(programCollection);

	if (VK_SHADER_STAGE_FRAGMENT_BIT == caseDef.shaderStage)
	{
		const string vertex	= "#version 450\n"
			"void main (void)\n"
			"{\n"
			"  vec2 uv = vec2(float(gl_VertexIndex & 1), float((gl_VertexIndex >> 1) & 1));\n"
			"  gl_Position = vec4(uv * 4.0f -2.0f, 0.0f, 1.0f);\n"
			"  gl_PointSize = 1.0f;\n"
			"}\n";
		programCollection.glslSources.add("vert") << glu::VertexSource(vertex) << vk::ShaderBuildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_3, 0u);
	}
	else if (VK_SHADER_STAGE_VERTEX_BIT != caseDef.shaderStage)
		subgroups::setVertexShaderFrameBuffer(programCollection);

	const string source =
		(OPTYPE_ALL == caseDef.opType) ?
			"  result = " + getOpTypeName(caseDef.opType) +
			"(true) ? 0x1 : 0;\n"
			"  result |= " + getOpTypeName(caseDef.opType) +
			"(false) ? 0 : 0x1A;\n"
			"  result |= 0x4;\n"
		: (OPTYPE_ANY == caseDef.opType) ?
				"  result = " + getOpTypeName(caseDef.opType) +
				"(true) ? 0x1 : 0;\n"
				"  result |= " + getOpTypeName(caseDef.opType) +
				"(false) ? 0 : 0x1A;\n"
				"  result |= 0x4;\n"
		: (OPTYPE_ALLEQUAL == caseDef.opType) ?
				"  " + subgroups::getFormatNameForGLSL(caseDef.format) + " valueEqual = " + subgroups::getFormatNameForGLSL(caseDef.format) + "(1.25 * float(data[gl_SubgroupInvocationID]) + 5.0);\n" +
				"  " + subgroups::getFormatNameForGLSL(caseDef.format) + " valueNoEqual = " + subgroups::getFormatNameForGLSL(caseDef.format) + (formatIsBoolean ? "(subgroupElect())\n;" : "(12.0 * float(data[gl_SubgroupInvocationID]) + gl_SubgroupInvocationID);\n") +
				"  result = " + getOpTypeName(caseDef.opType) + "("
				+ subgroups::getFormatNameForGLSL(caseDef.format) + "(1)) ? 0x1 : 0;\n"
				"  result |= " + getOpTypeName(caseDef.opType) +
				"(gl_SubgroupInvocationID) ? 0 : 0x2;\n"
				"  result |= " + getOpTypeName(caseDef.opType) +
				"(data[0]) ? 0x4 : 0;\n"
				"  result |= " + getOpTypeName(caseDef.opType) +
				"(valueEqual) ? 0x8 : 0x0;\n"
				"  result |= " + getOpTypeName(caseDef.opType) +
				"(valueNoEqual) ? 0x0 : 0x10;\n"
				"  if (subgroupElect()) result |= 0x2 | 0x10;\n"
		: "";

	if (VK_SHADER_STAGE_VERTEX_BIT == caseDef.shaderStage)
	{
		std::ostringstream vertexSrc;
		vertexSrc << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450)<<"\n"
			<< "#extension GL_KHR_shader_subgroup_vote: enable\n"
			<< "layout(location = 0) out vec4 out_color;\n"
			<< "layout(location = 0) in highp vec4 in_position;\n"
			<< "layout(set = 0, binding = 0) uniform Buffer1\n"
			<< "{\n"
			<< "  " << subgroups::getFormatNameForGLSL(caseDef.format) << " data[" << subgroups::maxSupportedSubgroupSize() << "];\n"
			<< "};\n"
			<< "\n"
			<< "void main (void)\n"
			<< "{\n"
			<< "  uint result;\n"
			<< source
			<< "  out_color.r = float(result);\n"
			<< "  gl_Position = in_position;\n"
			<< "  gl_PointSize = 1.0f;\n"
			<< "}\n";

		programCollection.glslSources.add("vert") << glu::VertexSource(vertexSrc.str()) << vk::ShaderBuildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_3, 0u);
	}
	else if (VK_SHADER_STAGE_GEOMETRY_BIT == caseDef.shaderStage)
	{
		std::ostringstream geometry;

		geometry << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450)<<"\n"
			<< "#extension GL_KHR_shader_subgroup_vote: enable\n"
			<< "layout(points) in;\n"
			<< "layout(points, max_vertices = 1) out;\n"
			<< "layout(location = 0) out float out_color;\n"
			<< "layout(set = 0, binding = 0) uniform Buffer1\n"
			<< "{\n"
			<< "  " << subgroups::getFormatNameForGLSL(caseDef.format) << " data[" << subgroups::maxSupportedSubgroupSize() << "];\n"
			<< "};\n"
			<< "\n"
			<< "void main (void)\n"
			<< "{\n"
			<< "  uint result;\n"
			<< source
			<< "  out_color = float(result);\n"
			<< "  gl_Position = gl_in[0].gl_Position;\n"
			<< "  EmitVertex();\n"
			<< "  EndPrimitive();\n"
			<< "}\n";

		programCollection.glslSources.add("geometry")
			<< glu::GeometrySource(geometry.str()) << buildOptions;
	}
	else if (VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT == caseDef.shaderStage)
	{
		std::ostringstream controlSource;
		controlSource << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450)<<"\n"
			<< "#extension GL_KHR_shader_subgroup_vote: enable\n"
			<< "layout(vertices = 2) out;\n"
			<< "layout(location = 0) out float out_color[];\n"
			<< "layout(set = 0, binding = 0) uniform Buffer1\n"
			<< "{\n"
			<< "  " << subgroups::getFormatNameForGLSL(caseDef.format) << " data[" << subgroups::maxSupportedSubgroupSize() << "];\n"
			<< "};\n"
			<< "\n"
			<< "void main (void)\n"
			<< "{\n"
			<< "  uint result;\n"
			<< "  if (gl_InvocationID == 0)\n"
			<<"  {\n"
			<< "    gl_TessLevelOuter[0] = 1.0f;\n"
			<< "    gl_TessLevelOuter[1] = 1.0f;\n"
			<< "  }\n"
			<< source
			<< "  out_color[gl_InvocationID] = float(result);"
			<< "  gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;\n"
			<< "}\n";

		programCollection.glslSources.add("tesc")
			<< glu::TessellationControlSource(controlSource.str()) << buildOptions;
		subgroups::setTesEvalShaderFrameBuffer(programCollection);
	}
	else if (VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT == caseDef.shaderStage)
	{
		std::ostringstream evaluationSource;
		evaluationSource << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450)<<"\n"
			<< "#extension GL_KHR_shader_subgroup_vote: enable\n"
			<< "#extension GL_EXT_tessellation_shader : require\n"
			<< "layout(isolines, equal_spacing, ccw ) in;\n"
			<< "layout(location = 0) out float out_color;\n"
			<< "layout(set = 0, binding = 0) uniform Buffer1\n"
			<< "{\n"
			<< "  " << subgroups::getFormatNameForGLSL(caseDef.format) << " data[" << subgroups::maxSupportedSubgroupSize() << "];\n"
			<< "};\n"
			<< "\n"
			<< "void main (void)\n"
			<< "{\n"
			<< "  uint result;\n"
			<< "  highp uint offset = gl_PrimitiveID * 2 + uint(gl_TessCoord.x + 0.5);\n"
			<< source
			<< "  out_color = float(result);\n"
			<< "  gl_Position = mix(gl_in[0].gl_Position, gl_in[1].gl_Position, gl_TessCoord.x);\n"
			<< "}\n";

		subgroups::setTesCtrlShaderFrameBuffer(programCollection);
		programCollection.glslSources.add("tese")
				<< glu::TessellationEvaluationSource(evaluationSource.str()) << buildOptions;
	}
	else if (VK_SHADER_STAGE_FRAGMENT_BIT == caseDef.shaderStage)
	{
		const string sourceFragment =
		(OPTYPE_ALL == caseDef.opType) ?
			"  result |= " + getOpTypeName(caseDef.opType) +
			"(!gl_HelperInvocation) ? 0x0 : 0x1;\n"
			"  result |= " + getOpTypeName(caseDef.opType) +
			"(false) ? 0 : 0x1A;\n"
			"  result |= 0x4;\n"
		: (OPTYPE_ANY == caseDef.opType) ?
				"  result |= " + getOpTypeName(caseDef.opType) +
				"(gl_HelperInvocation) ? 0x1 : 0x0;\n"
				"  result |= " + getOpTypeName(caseDef.opType) +
				"(false) ? 0 : 0x1A;\n"
				"  result |= 0x4;\n"
		: (OPTYPE_ALLEQUAL == caseDef.opType) ?
				"  " + subgroups::getFormatNameForGLSL(caseDef.format) + " valueEqual = " + subgroups::getFormatNameForGLSL(caseDef.format) + "(1.25 * float(data[gl_SubgroupInvocationID]) + 5.0);\n" +
				"  " + subgroups::getFormatNameForGLSL(caseDef.format) + " valueNoEqual = " + subgroups::getFormatNameForGLSL(caseDef.format) + (formatIsBoolean ? "(subgroupElect());\n" : "(12.0 * float(data[gl_SubgroupInvocationID]) + int(gl_FragCoord.x*gl_SubgroupInvocationID));\n") +
				"  result |= " + getOpTypeName(caseDef.opType) + "("
				+ subgroups::getFormatNameForGLSL(caseDef.format) + "(1)) ? 0x10 : 0;\n"
				"  result |= " + getOpTypeName(caseDef.opType) +
				"(gl_SubgroupInvocationID) ? 0 : 0x2;\n"
				"  result |= " + getOpTypeName(caseDef.opType) +
				"(data[0]) ? 0x4 : 0;\n"
				"  result |= " + getOpTypeName(caseDef.opType) +
				"(valueEqual) ? 0x8 : 0x0;\n"
				"  result |= " + getOpTypeName(caseDef.opType) +
				"(gl_HelperInvocation) ? 0x0 : 0x1;\n"
				"  if (subgroupElect()) result |= 0x2 | 0x10;\n"
		: "";

		std::ostringstream fragmentSource;
		fragmentSource << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450)<<"\n"
		<< "#extension GL_KHR_shader_subgroup_vote: enable\n"
		<< "layout(location = 0) out uint out_color;\n"
		<< "layout(set = 0, binding = 0) uniform Buffer1\n"
		<< "{\n"
		<< "  " << subgroups::getFormatNameForGLSL(caseDef.format) << " data[" << subgroups::maxSupportedSubgroupSize() << "];\n"
		<< "};\n"
		<< ""
		<< "void main()\n"
		<< "{\n"
		<< "  uint result = 0u;\n"
		<< "  if (dFdx(gl_SubgroupInvocationID * gl_FragCoord.x * gl_FragCoord.y) - dFdy(gl_SubgroupInvocationID * gl_FragCoord.x * gl_FragCoord.y) > 0.0f)\n"
		<< "  {\n"
		<< "    result |= 0x20;\n" // to be sure that compiler doesn't remove dFdx and dFdy executions
		<< "  }\n"
		<< "  bool helper = subgroupAny(gl_HelperInvocation);\n"
		<< "  if (helper)\n"
		<< "  {\n"
		<< "    result |= 0x40;\n"
		<< "  }\n"
		<< sourceFragment
		<< "  out_color = result;\n"
		<< "}\n";

		programCollection.glslSources.add("fragment")
			<< glu::FragmentSource(fragmentSource.str())<< vk::ShaderBuildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_3, 0u);
	}
	else
	{
		DE_FATAL("Unsupported shader stage");
	}
}

void initPrograms(SourceCollections& programCollection, CaseDefinition caseDef)
{
	const bool formatIsBoolean =
		VK_FORMAT_R8_USCALED == caseDef.format || VK_FORMAT_R8G8_USCALED == caseDef.format || VK_FORMAT_R8G8B8_USCALED == caseDef.format || VK_FORMAT_R8G8B8A8_USCALED == caseDef.format;
	if (VK_SHADER_STAGE_COMPUTE_BIT == caseDef.shaderStage)
	{
		std::ostringstream src;

		src << "#version 450\n"
			<< "#extension GL_KHR_shader_subgroup_vote: enable\n"
			<< "layout (local_size_x_id = 0, local_size_y_id = 1, "
			"local_size_z_id = 2) in;\n"
			<< "layout(set = 0, binding = 0, std430) buffer Buffer1\n"
			<< "{\n"
			<< "  uint result[];\n"
			<< "};\n"
			<< "layout(set = 0, binding = 1, std430) buffer Buffer2\n"
			<< "{\n"
			<< "  " << subgroups::getFormatNameForGLSL(caseDef.format) << " data[];\n"
			<< "};\n"
			<< "\n"
			<< "void main (void)\n"
			<< "{\n"
			<< "  uvec3 globalSize = gl_NumWorkGroups * gl_WorkGroupSize;\n"
			<< "  highp uint offset = globalSize.x * ((globalSize.y * "
			"gl_GlobalInvocationID.z) + gl_GlobalInvocationID.y) + "
			"gl_GlobalInvocationID.x;\n";
		if (OPTYPE_ALL == caseDef.opType)
		{
			src << "  result[offset] = " << getOpTypeName(caseDef.opType)
				<< "(true) ? 0x1 : 0;\n"
				<< "  result[offset] |= " << getOpTypeName(caseDef.opType)
				<< "(false) ? 0 : 0x1A;\n"
				<< "  result[offset] |= " << getOpTypeName(caseDef.opType)
				<< "(data[gl_SubgroupInvocationID] > 0) ? 0x4 : 0;\n";
		}
		else if (OPTYPE_ANY == caseDef.opType)
		{
			src << "  result[offset] = " << getOpTypeName(caseDef.opType)
				<< "(true) ? 0x1 : 0;\n"
				<< "  result[offset] |= " << getOpTypeName(caseDef.opType)
				<< "(false) ? 0 : 0x1A;\n"
				<< "  result[offset] |= " << getOpTypeName(caseDef.opType)
				<< "(data[gl_SubgroupInvocationID] == data[0]) ? 0x4 : 0;\n";
		}

		else if (OPTYPE_ALLEQUAL == caseDef.opType)
		{
			src << "  " << subgroups::getFormatNameForGLSL(caseDef.format) <<" valueEqual = " << subgroups::getFormatNameForGLSL(caseDef.format) << "(1.25 * float(data[gl_SubgroupInvocationID]) + 5.0);\n"
				<< "  " << subgroups::getFormatNameForGLSL(caseDef.format) <<" valueNoEqual = " << subgroups::getFormatNameForGLSL(caseDef.format) << (formatIsBoolean ? "(subgroupElect());\n" : "(12.0 * float(data[gl_SubgroupInvocationID]) + offset);\n")
				<<"  result[offset] = " << getOpTypeName(caseDef.opType) << "("
				<< subgroups::getFormatNameForGLSL(caseDef.format) << "(1)) ? 0x1 : 0x0;\n"
				<< "  result[offset] |= " << getOpTypeName(caseDef.opType)
				<< "(gl_SubgroupInvocationID) ? 0x0 : 0x2;\n"
				<< "  result[offset] |= " << getOpTypeName(caseDef.opType)
				<< "(data[0]) ? 0x4 : 0x0;\n"
				<< "  result[offset] |= "<< getOpTypeName(caseDef.opType)
				<< "(valueEqual) ? 0x8 : 0x0;\n"
				<< "  result[offset] |= "<< getOpTypeName(caseDef.opType)
				<< "(valueNoEqual) ? 0x0 : 0x10;\n"
				<< "  if (subgroupElect()) result[offset] |= 0x2 | 0x10;\n";
		}

		src << "}\n";

		programCollection.glslSources.add("comp")
				<< glu::ComputeSource(src.str()) << vk::ShaderBuildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_3, 0u);
	}
	else
	{
		const string source =
		(OPTYPE_ALL == caseDef.opType) ?
			"  result[offset] = " + getOpTypeName(caseDef.opType) +
			"(true) ? 0x1 : 0;\n"
			"  result[offset] |= " + getOpTypeName(caseDef.opType) +
			"(false) ? 0 : 0x1A;\n"
			"  result[offset] |= 0x4;\n"
		: (OPTYPE_ANY == caseDef.opType) ?
				"  result[offset] = " + getOpTypeName(caseDef.opType) +
				"(true) ? 0x1 : 0;\n"
				"  result[offset] |= " + getOpTypeName(caseDef.opType) +
				"(false) ? 0 : 0x1A;\n"
				"  result[offset] |= 0x4;\n"
		: (OPTYPE_ALLEQUAL == caseDef.opType) ?
				"  " + subgroups::getFormatNameForGLSL(caseDef.format) + " valueEqual = " + subgroups::getFormatNameForGLSL(caseDef.format) + "(1.25 * float(data[gl_SubgroupInvocationID]) + 5.0);\n" +
				"  " + subgroups::getFormatNameForGLSL(caseDef.format) + " valueNoEqual = " + subgroups::getFormatNameForGLSL(caseDef.format) + (formatIsBoolean ? "(subgroupElect());\n" : "(12.0 * float(data[gl_SubgroupInvocationID]) + gl_SubgroupInvocationID);\n") +
				"  result[offset] = " + getOpTypeName(caseDef.opType) + "("
				+ subgroups::getFormatNameForGLSL(caseDef.format) + "(1)) ? 0x1 : 0;\n"
				"  result[offset] |= " + getOpTypeName(caseDef.opType) +
				"(gl_SubgroupInvocationID) ? 0 : 0x2;\n"
				"  result[offset] |= " + getOpTypeName(caseDef.opType) +
				"(data[0]) ? 0x4 : 0;\n"
				"  result[offset] |= " + getOpTypeName(caseDef.opType) +
				"(valueEqual) ? 0x8 : 0x0;\n"
				"  result[offset] |= " + getOpTypeName(caseDef.opType) +
				"(valueNoEqual) ? 0x0 : 0x10;\n"
				"  if (subgroupElect()) result[offset] |= 0x2 | 0x10;\n"
		: "";

		const string formatString = subgroups::getFormatNameForGLSL(caseDef.format);

		{
			const string vertex =
				"#version 450\n"
				"#extension GL_KHR_shader_subgroup_vote: enable\n"
				"layout(set = 0, binding = 0, std430) buffer Buffer1\n"
				"{\n"
				"  uint result[];\n"
				"};\n"
				"layout(set = 0, binding = 4, std430) readonly buffer Buffer2\n"
				"{\n"
				"  " + formatString + " data[];\n"
				"};\n"
				"\n"
				"void main (void)\n"
				"{\n"
				"  highp uint offset = gl_VertexIndex;\n"
				+ source +
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
				"#extension GL_KHR_shader_subgroup_vote: enable\n"
				"layout(vertices=1) out;\n"
				"layout(set = 0, binding = 1, std430) buffer Buffer1\n"
				"{\n"
				"  uint result[];\n"
				"};\n"
				"layout(set = 0, binding = 4, std430) readonly buffer Buffer2\n"
				"{\n"
				"  " + formatString + " data[];\n"
				"};\n"
				"\n"
				"void main (void)\n"
				"{\n"
				"  highp uint offset = gl_PrimitiveID;\n"
				+ source +
				"  if (gl_InvocationID == 0)\n"
				"  {\n"
				"    gl_TessLevelOuter[0] = 1.0f;\n"
				"    gl_TessLevelOuter[1] = 1.0f;\n"
				"  }\n"
				"  gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;\n"
				"}\n";

			programCollection.glslSources.add("tesc")
					<< glu::TessellationControlSource(tesc) << vk::ShaderBuildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_3, 0u);
		}

		{
			const string tese =
				"#version 450\n"
				"#extension GL_KHR_shader_subgroup_vote: enable\n"
				"layout(isolines) in;\n"
				"layout(set = 0, binding = 2, std430) buffer Buffer1\n"
				"{\n"
				"  uint result[];\n"
				"};\n"
				"layout(set = 0, binding = 4, std430) readonly buffer Buffer2\n"
				"{\n"
				"  " + formatString + " data[];\n"
				"};\n"
				"\n"
				"void main (void)\n"
				"{\n"
				"  highp uint offset = gl_PrimitiveID * 2 + uint(gl_TessCoord.x + 0.5);\n"
				+ source +
				"  float pixelSize = 2.0f/1024.0f;\n"
				"  gl_Position = gl_in[0].gl_Position + gl_TessCoord.x * pixelSize / 2.0f;\n"
				"}\n";

			programCollection.glslSources.add("tese")
					<< glu::TessellationEvaluationSource(tese) << vk::ShaderBuildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_3, 0u);
		}

		{
			const string geometry =
				"#version 450\n"
				"#extension GL_KHR_shader_subgroup_vote: enable\n"
				"layout(${TOPOLOGY}) in;\n"
				"layout(points, max_vertices = 1) out;\n"
				"layout(set = 0, binding = 3, std430) buffer Buffer1\n"
				"{\n"
				"  uint result[];\n"
				"};\n"
				"layout(set = 0, binding = 4, std430) readonly buffer Buffer2\n"
				"{\n"
				"  " + formatString + " data[];\n"
				"};\n"
				"\n"
				"void main (void)\n"
				"{\n"
				"  highp uint offset = gl_PrimitiveIDIn;\n"
				+ source +
				"  gl_Position = gl_in[0].gl_Position;\n"
				"  EmitVertex();\n"
				"  EndPrimitive();\n"
				"}\n";

			subgroups::addGeometryShadersFromTemplate(geometry, vk::ShaderBuildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_3, 0u),
													  programCollection.glslSources);
		}

		{
			const string sourceFragment =
			(OPTYPE_ALL == caseDef.opType) ?
				"  result = " + getOpTypeName(caseDef.opType) +
				"(true) ? 0x1 : 0;\n"
				"  result |= " + getOpTypeName(caseDef.opType) +
				"(false) ? 0 : 0x1A;\n"
				"  result |= 0x4;\n"
			: (OPTYPE_ANY == caseDef.opType) ?
					"  result = " + getOpTypeName(caseDef.opType) +
					"(true) ? 0x1 : 0;\n"
					"  result |= " + getOpTypeName(caseDef.opType) +
					"(false) ? 0 : 0x1A;\n"
					"  result |= 0x4;\n"
			: (OPTYPE_ALLEQUAL == caseDef.opType) ?
					"  " + subgroups::getFormatNameForGLSL(caseDef.format) + " valueEqual = " + subgroups::getFormatNameForGLSL(caseDef.format) + "(1.25 * float(data[gl_SubgroupInvocationID]) + 5.0);\n" +
					"  " + subgroups::getFormatNameForGLSL(caseDef.format) + " valueNoEqual = " + subgroups::getFormatNameForGLSL(caseDef.format) + (formatIsBoolean ? "(subgroupElect());\n" : "(12.0 * float(data[gl_SubgroupInvocationID]) + int(gl_FragCoord.x*gl_SubgroupInvocationID));\n") +
					"  result = " + getOpTypeName(caseDef.opType) + "("
					+ subgroups::getFormatNameForGLSL(caseDef.format) + "(1)) ? 0x1 : 0;\n"
					"  result |= " + getOpTypeName(caseDef.opType) +
					"(gl_SubgroupInvocationID) ? 0 : 0x2;\n"
					"  result |= " + getOpTypeName(caseDef.opType) +
					"(data[0]) ? 0x4 : 0;\n"
					"  result |= " + getOpTypeName(caseDef.opType) +
					"(valueEqual) ? 0x8 : 0x0;\n"
					"  result |= " + getOpTypeName(caseDef.opType) +
					"(valueNoEqual) ? 0x0 : 0x10;\n"
					"  if (subgroupElect()) result |= 0x2 | 0x10;\n"
			: "";
			const string fragment =
				"#version 450\n"
				"#extension GL_KHR_shader_subgroup_vote: enable\n"
				"layout(location = 0) out uint result;\n"
				"layout(set = 0, binding = 4, std430) readonly buffer Buffer2\n"
				"{\n"
				"  " + formatString + " data[];\n"
				"};\n"
				"void main (void)\n"
				"{\n"
				+ sourceFragment +
				"}\n";

			programCollection.glslSources.add("fragment")
				<< glu::FragmentSource(fragment)<< vk::ShaderBuildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_3, 0u);
		}

		subgroups::addNoSubgroupShader(programCollection);
	}
}

void supportedCheck (Context& context, CaseDefinition caseDef)
{
	if (!subgroups::isSubgroupSupported(context))
		TCU_THROW(NotSupportedError, "Subgroup operations are not supported");

	if (!subgroups::isSubgroupFeatureSupportedForDevice(context, VK_SUBGROUP_FEATURE_VOTE_BIT))
	{
		TCU_THROW(NotSupportedError, "Device does not support subgroup vote operations");
	}

	if (subgroups::isDoubleFormat(caseDef.format) &&
			!subgroups::isDoubleSupportedForDevice(context))
	{
		TCU_THROW(NotSupportedError, "Device does not support subgroup double operations");
	}
}

tcu::TestStatus noSSBOtest (Context& context, const CaseDefinition caseDef)
{
	if (!subgroups::areSubgroupOperationsSupportedForStage(
				context, caseDef.shaderStage))
	{
		if (subgroups::areSubgroupOperationsRequiredForStage(
					caseDef.shaderStage))
		{
			return tcu::TestStatus::fail(
					   "Shader stage " +
					   subgroups::getShaderStageName(caseDef.shaderStage) +
					   " is required to support subgroup operations!");
		}
		else
		{
			TCU_THROW(NotSupportedError, "Device does not support subgroup operations for this stage");
		}
	}

	subgroups::SSBOData inputData;
	inputData.format = caseDef.format;
	inputData.numElements = subgroups::maxSupportedSubgroupSize();
	inputData.initializeType = OPTYPE_ALLEQUAL == caseDef.opType ? subgroups::SSBOData::InitializeZero : subgroups::SSBOData::InitializeNonZero;

	if (VK_SHADER_STAGE_VERTEX_BIT == caseDef.shaderStage)
		return subgroups::makeVertexFrameBufferTest(context, VK_FORMAT_R32_UINT, &inputData, 1, checkVertexPipelineStages);
	else if (VK_SHADER_STAGE_GEOMETRY_BIT == caseDef.shaderStage)
		return subgroups::makeGeometryFrameBufferTest(context, VK_FORMAT_R32_UINT, &inputData, 1, checkVertexPipelineStages);
	else if (VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT == caseDef.shaderStage)
		return subgroups::makeTessellationEvaluationFrameBufferTest(context, VK_FORMAT_R32_UINT, &inputData, 1, checkVertexPipelineStages, VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT);
	else if (VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT == caseDef.shaderStage)
		return subgroups::makeTessellationEvaluationFrameBufferTest(context, VK_FORMAT_R32_UINT, &inputData, 1, checkVertexPipelineStages, VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT);
	else if (VK_SHADER_STAGE_FRAGMENT_BIT == caseDef.shaderStage)
		return subgroups::makeFragmentFrameBufferTest(context, VK_FORMAT_R32_UINT, &inputData, 1, checkFragmentPipelineStages);
	else
		TCU_THROW(InternalError, "Unhandled shader stage");
}


tcu::TestStatus test(Context& context, const CaseDefinition caseDef)
{
	if (VK_SHADER_STAGE_COMPUTE_BIT == caseDef.shaderStage)
	{
		if (!subgroups::areSubgroupOperationsSupportedForStage(context, caseDef.shaderStage))
		{
			return tcu::TestStatus::fail(
					   "Shader stage " +
					   subgroups::getShaderStageName(caseDef.shaderStage) +
					   " is required to support subgroup operations!");
		}

		subgroups::SSBOData inputData;
		inputData.format = caseDef.format;
		inputData.numElements = subgroups::maxSupportedSubgroupSize();
		inputData.initializeType = OPTYPE_ALLEQUAL == caseDef.opType ? subgroups::SSBOData::InitializeZero : subgroups::SSBOData::InitializeNonZero;

		return subgroups::makeComputeTest(context, VK_FORMAT_R32_UINT, &inputData,
										  1, (OPTYPE_ALLEQUAL == caseDef.opType) ? checkComputeAllEqual : checkCompute);
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

		subgroups::SSBOData inputData;
		inputData.format			= caseDef.format;
		inputData.numElements		= subgroups::maxSupportedSubgroupSize();
		inputData.initializeType	= OPTYPE_ALLEQUAL == caseDef.opType ? subgroups::SSBOData::InitializeZero : subgroups::SSBOData::InitializeNonZero;
		inputData.binding			= 4u;
		inputData.stages			= stages;

		return subgroups::allStages(context, VK_FORMAT_R32_UINT, &inputData, 1, checkVertexPipelineStages, stages);
	}
}
}

namespace vkt
{
namespace subgroups
{
tcu::TestCaseGroup* createSubgroupsVoteTests(tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> graphicGroup(new tcu::TestCaseGroup(
		testCtx, "graphics", "Subgroup arithmetic category tests: graphics"));
	de::MovePtr<tcu::TestCaseGroup> computeGroup(new tcu::TestCaseGroup(
		testCtx, "compute", "Subgroup arithmetic category tests: compute"));
	de::MovePtr<tcu::TestCaseGroup> framebufferGroup(new tcu::TestCaseGroup(
		testCtx, "framebuffer", "Subgroup arithmetic category tests: framebuffer"));

	de::MovePtr<tcu::TestCaseGroup> fragHelperGroup(new tcu::TestCaseGroup(
		testCtx, "frag_helper", "Subgroup arithmetic category tests: fragment helper invocation"));

	const VkShaderStageFlags stages[] =
	{
		VK_SHADER_STAGE_VERTEX_BIT,
		VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,
		VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,
		VK_SHADER_STAGE_GEOMETRY_BIT,
	};

	const VkFormat formats[] =
	{
		VK_FORMAT_R32_SINT, VK_FORMAT_R32G32_SINT, VK_FORMAT_R32G32B32_SINT,
		VK_FORMAT_R32G32B32A32_SINT, VK_FORMAT_R32_UINT, VK_FORMAT_R32G32_UINT,
		VK_FORMAT_R32G32B32_UINT, VK_FORMAT_R32G32B32A32_UINT,
		VK_FORMAT_R32_SFLOAT, VK_FORMAT_R32G32_SFLOAT,
		VK_FORMAT_R32G32B32_SFLOAT, VK_FORMAT_R32G32B32A32_SFLOAT,
		VK_FORMAT_R64_SFLOAT, VK_FORMAT_R64G64_SFLOAT,
		VK_FORMAT_R64G64B64_SFLOAT, VK_FORMAT_R64G64B64A64_SFLOAT,
		VK_FORMAT_R8_USCALED, VK_FORMAT_R8G8_USCALED,
		VK_FORMAT_R8G8B8_USCALED, VK_FORMAT_R8G8B8A8_USCALED,
	};

	for (int formatIndex = 0; formatIndex < DE_LENGTH_OF_ARRAY(formats); ++formatIndex)
	{
		const VkFormat format = formats[formatIndex];

		for (int opTypeIndex = 0; opTypeIndex < OPTYPE_LAST; ++opTypeIndex)
		{
			// Skip the typed tests for all but subgroupAllEqual()
			if ((VK_FORMAT_R32_UINT != format) && (OPTYPE_ALLEQUAL != opTypeIndex))
			{
				continue;
			}

			const std::string op = de::toLower(getOpTypeName(opTypeIndex));

			{
				const CaseDefinition caseDef = {opTypeIndex, VK_SHADER_STAGE_COMPUTE_BIT, format};
				addFunctionCaseWithPrograms(computeGroup.get(),
											op + "_" + subgroups::getFormatNameForGLSL(format),
											"", supportedCheck, initPrograms, test, caseDef);
			}

			{
				const CaseDefinition caseDef = {opTypeIndex, VK_SHADER_STAGE_ALL_GRAPHICS, format};
				addFunctionCaseWithPrograms(graphicGroup.get(),
											op + "_" + subgroups::getFormatNameForGLSL(format),
											"", supportedCheck, initPrograms, test, caseDef);
			}

			for (int stageIndex = 0; stageIndex < DE_LENGTH_OF_ARRAY(stages); ++stageIndex)
			{
				const CaseDefinition caseDef = {opTypeIndex, stages[stageIndex], format};
				addFunctionCaseWithPrograms(framebufferGroup.get(),
							op + "_" +
							subgroups::getFormatNameForGLSL(format)
							+ "_" + getShaderStageName(caseDef.shaderStage), "",
							supportedCheck, initFrameBufferPrograms, noSSBOtest, caseDef);
			}

			const CaseDefinition caseDef = {opTypeIndex, VK_SHADER_STAGE_FRAGMENT_BIT, format};
			addFunctionCaseWithPrograms(fragHelperGroup.get(),
						op + "_" +
						subgroups::getFormatNameForGLSL(format)
						+ "_" + getShaderStageName(caseDef.shaderStage), "",
						supportedCheck, initFrameBufferPrograms, noSSBOtest, caseDef);
		}
	}

	de::MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(
		testCtx, "vote", "Subgroup vote category tests"));

	group->addChild(graphicGroup.release());
	group->addChild(computeGroup.release());
	group->addChild(framebufferGroup.release());
	group->addChild(fragHelperGroup.release());

	return group.release();
}

} // subgroups
} // vkt
