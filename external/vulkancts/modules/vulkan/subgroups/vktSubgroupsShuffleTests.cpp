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

#include "vktSubgroupsShuffleTests.hpp"
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
	OPTYPE_SHUFFLE = 0,
	OPTYPE_SHUFFLE_XOR,
	OPTYPE_SHUFFLE_UP,
	OPTYPE_SHUFFLE_DOWN,
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

		if (0x1 != val)
		{
			return false;
		}
	}

	return true;
}

static bool checkFragment(std::vector<const void*> datas,
						  deUint32 width, deUint32 height, deUint32)
{
	const deUint32* data =
		reinterpret_cast<const deUint32*>(datas[0]);
	for (deUint32 x = 0; x < width; ++x)
	{
		for (deUint32 y = 0; y < height; ++y)
		{
			deUint32 val = data[x * height + y];

			if (0x1 != val)
			{
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
	const deUint32* data =
		reinterpret_cast<const deUint32*>(datas[0]);

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

std::string getOpTypeName(int opType)
{
	switch (opType)
	{
		default:
			DE_FATAL("Unsupported op type");
		case OPTYPE_SHUFFLE:
			return "subgroupShuffle";
		case OPTYPE_SHUFFLE_XOR:
			return "subgroupShuffleXor";
		case OPTYPE_SHUFFLE_UP:
			return "subgroupShuffleUp";
		case OPTYPE_SHUFFLE_DOWN:
			return "subgroupShuffleDown";
	}
}

struct CaseDefinition
{
	int					opType;
	VkShaderStageFlags	shaderStage;
	VkFormat			format;
	bool				noSSBO;
};

void initFrameBufferPrograms (SourceCollections& programCollection, CaseDefinition caseDef)
{
	std::string idTable[OPTYPE_LAST];
	idTable[OPTYPE_SHUFFLE]			= "data2[gl_SubgroupInvocationID]";
	idTable[OPTYPE_SHUFFLE_XOR]		= "gl_SubgroupInvocationID ^ data2[gl_SubgroupInvocationID]";
	idTable[OPTYPE_SHUFFLE_UP]		= "gl_SubgroupInvocationID - data2[gl_SubgroupInvocationID]";
	idTable[OPTYPE_SHUFFLE_DOWN]	= "gl_SubgroupInvocationID + data2[gl_SubgroupInvocationID]";

	if (VK_SHADER_STAGE_VERTEX_BIT == caseDef.shaderStage)
	{
		std::ostringstream	src;
		std::ostringstream	fragmentSrc;

		src << "#version 450\n"
			<< "layout(location = 0) in highp vec4 in_position;\n"
			<< "layout(location = 0) out float result;\n";

		switch (caseDef.opType)
		{
			case OPTYPE_SHUFFLE:
			case OPTYPE_SHUFFLE_XOR:
				src << "#extension GL_KHR_shader_subgroup_shuffle: enable\n";
				break;
			default:
				src << "#extension GL_KHR_shader_subgroup_shuffle_relative: enable\n";
				break;
		}

		src	<< "#extension GL_KHR_shader_subgroup_ballot: enable\n"
			<< "layout(set = 0, binding = 0) uniform Buffer1\n"
			<< "{\n"
			<< "  " << subgroups::getFormatNameForGLSL(caseDef.format) << " data1[" << subgroups::maxSupportedSubgroupSize() << "];\n"
			<< "};\n"
			<< "layout(set = 0, binding = 1) uniform Buffer2\n"
			<< "{\n"
			<< "  uint data2[" << subgroups::maxSupportedSubgroupSize() << "];\n"
			<< "};\n"
			<< "\n"
			<< "void main (void)\n"
			<< "{\n"
			<< "  uvec4 mask = subgroupBallot(true);\n"
			<< "  " << subgroups::getFormatNameForGLSL(caseDef.format) << " op = "
			<< getOpTypeName(caseDef.opType) << "(data1[gl_SubgroupInvocationID], data2[gl_SubgroupInvocationID]);\n"
			<< "  uint id = " << idTable[caseDef.opType] << ";\n"
			<< "  if ((0 <= id) && (id < gl_SubgroupSize) && subgroupBallotBitExtract(mask, id))\n"
			<< "  {\n"
			<< "    result = (op == data1[id]) ? 1.0f : 0.0f;\n"
			<< "  }\n"
			<< "  else\n"
			<< "  {\n"
			<< "    result = 1.0f; // Invocation we read from was inactive, so we can't verify results!\n"
			<< "  }\n"
			<< "  gl_Position = in_position;\n"
			<< "  gl_PointSize = 1.0f;\n"
			<< "}\n";

		programCollection.glslSources.add("vert") << glu::VertexSource(src.str()) << vk::ShaderBuildOptions(vk::SPIRV_VERSION_1_3, 0u);

		fragmentSrc << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450)<<"\n"
			<< "layout(location = 0) in float result;\n"
			<< "layout(location = 0) out uint out_color;\n"
			<< "void main()\n"
			<<"{\n"
			<< "	out_color = uint(result);\n"
			<< "}\n";
		programCollection.glslSources.add("fragment") << glu::FragmentSource(fragmentSrc.str()) << vk::ShaderBuildOptions(vk::SPIRV_VERSION_1_3, 0u);
	}
	else
	{
		DE_FATAL("Unsupported shader stage");
	}
}

void initPrograms(SourceCollections& programCollection, CaseDefinition caseDef)
{
	std::string idTable[OPTYPE_LAST];
	idTable[OPTYPE_SHUFFLE] = "data2[gl_SubgroupInvocationID]";
	idTable[OPTYPE_SHUFFLE_XOR] = "gl_SubgroupInvocationID ^ data2[gl_SubgroupInvocationID]";
	idTable[OPTYPE_SHUFFLE_UP] = "gl_SubgroupInvocationID - data2[gl_SubgroupInvocationID]";
	idTable[OPTYPE_SHUFFLE_DOWN] = "gl_SubgroupInvocationID + data2[gl_SubgroupInvocationID]";

	if (VK_SHADER_STAGE_COMPUTE_BIT == caseDef.shaderStage)
	{
		std::ostringstream src;

		src << "#version 450\n";

		switch (caseDef.opType)
		{
			case OPTYPE_SHUFFLE:
			case OPTYPE_SHUFFLE_XOR:
				src << "#extension GL_KHR_shader_subgroup_shuffle: enable\n";
				break;
			default:
				src << "#extension GL_KHR_shader_subgroup_shuffle_relative: enable\n";
				break;
		}

		src	<< "#extension GL_KHR_shader_subgroup_ballot: enable\n"
			<< "layout (local_size_x_id = 0, local_size_y_id = 1, "
			"local_size_z_id = 2) in;\n"
			<< "layout(set = 0, binding = 0, std430) buffer Buffer1\n"
			<< "{\n"
			<< "  uint result[];\n"
			<< "};\n"
			<< "layout(set = 0, binding = 1, std430) buffer Buffer2\n"
			<< "{\n"
			<< "  " << subgroups::getFormatNameForGLSL(caseDef.format) << " data1[];\n"
			<< "};\n"
			<< "layout(set = 0, binding = 2, std430) buffer Buffer3\n"
			<< "{\n"
			<< "  uint data2[];\n"
			<< "};\n"
			<< "\n"
			<< "void main (void)\n"
			<< "{\n"
			<< "  uvec3 globalSize = gl_NumWorkGroups * gl_WorkGroupSize;\n"
			<< "  highp uint offset = globalSize.x * ((globalSize.y * "
			"gl_GlobalInvocationID.z) + gl_GlobalInvocationID.y) + "
			"gl_GlobalInvocationID.x;\n"
			<< "  uvec4 mask = subgroupBallot(true);\n"
			<< "  " << subgroups::getFormatNameForGLSL(caseDef.format) << " op = "
			<< getOpTypeName(caseDef.opType) << "(data1[gl_SubgroupInvocationID], data2[gl_SubgroupInvocationID]);\n"
			<< "  uint id = " << idTable[caseDef.opType] << ";\n"
			<< "  if ((0 <= id) && (id < gl_SubgroupSize) && subgroupBallotBitExtract(mask, id))\n"
			<< "  {\n"
			<< "    result[offset] = (op == data1[id]) ? 1 : 0;\n"
			<< "  }\n"
			<< "  else\n"
			<< "  {\n"
			<< "    result[offset] = 1; // Invocation we read from was inactive, so we can't verify results!\n"
			<< "  }\n"
			<< "}\n";

		programCollection.glslSources.add("comp")
				<< glu::ComputeSource(src.str()) << vk::ShaderBuildOptions(vk::SPIRV_VERSION_1_3, 0u);
	}
	else if (VK_SHADER_STAGE_FRAGMENT_BIT == caseDef.shaderStage)
	{
		programCollection.glslSources.add("vert")
				<< glu::VertexSource(subgroups::getVertShaderForStage(caseDef.shaderStage)) << vk::ShaderBuildOptions(vk::SPIRV_VERSION_1_3, 0u);

		std::ostringstream frag;

		frag << "#version 450\n";

		switch (caseDef.opType)
		{
			case OPTYPE_SHUFFLE:
			case OPTYPE_SHUFFLE_XOR:
				frag << "#extension GL_KHR_shader_subgroup_shuffle: enable\n";
				break;
			default:
				frag << "#extension GL_KHR_shader_subgroup_shuffle_relative: enable\n";
				break;
		}

		frag << "#extension GL_KHR_shader_subgroup_ballot: enable\n"
			 << "layout(location = 0) out uint result;\n"
			 << "layout(set = 0, binding = 0, std430) readonly buffer Buffer1\n"
			 << "{\n"
			 << "  " << subgroups::getFormatNameForGLSL(caseDef.format) << " data1[];\n"
			 << "};\n"
			 << "layout(set = 0, binding = 1, std430) readonly buffer Buffer2\n"
			 << "{\n"
			 << "  uint data2[];\n"
			 << "};\n"
			 << "void main (void)\n"
			 << "{\n"
			 << "  uvec4 mask = subgroupBallot(true);\n"
			 << "  " << subgroups::getFormatNameForGLSL(caseDef.format) << " op = "
			 << getOpTypeName(caseDef.opType) << "(data1[gl_SubgroupInvocationID], data2[gl_SubgroupInvocationID]);\n"
			 << "  uint id = " << idTable[caseDef.opType] << ";\n"
			 << "  if ((0 <= id) && (id < gl_SubgroupSize) && subgroupBallotBitExtract(mask, id))\n"
			 << "  {\n"
			 << "    result = (op == data1[id]) ? 1 : 0;\n"
			 << "  }\n"
			 << "  else\n"
			 << "  {\n"
			 << "    result = 1; // Invocation we read from was inactive, so we can't verify results!\n"
			 << "  }\n"
			 << "}\n";

		programCollection.glslSources.add("frag")
				<< glu::FragmentSource(frag.str()) << vk::ShaderBuildOptions(vk::SPIRV_VERSION_1_3, 0u);
	}
	else if (VK_SHADER_STAGE_VERTEX_BIT == caseDef.shaderStage)
	{
		std::ostringstream src;

		src << "#version 450\n";

		switch (caseDef.opType)
		{
			case OPTYPE_SHUFFLE:
			case OPTYPE_SHUFFLE_XOR:
				src << "#extension GL_KHR_shader_subgroup_shuffle: enable\n";
				break;
			default:
				src << "#extension GL_KHR_shader_subgroup_shuffle_relative: enable\n";
				break;
		}

		src	<< "#extension GL_KHR_shader_subgroup_ballot: enable\n"
			<< "layout(set = 0, binding = 0, std430) buffer Buffer1\n"
			<< "{\n"
			<< "  uint result[];\n"
			<< "};\n"
			<< "layout(set = 0, binding = 1, std430) buffer Buffer2\n"
			<< "{\n"
			<< "  " << subgroups::getFormatNameForGLSL(caseDef.format) << " data1[];\n"
			<< "};\n"
			<< "layout(set = 0, binding = 2, std430) buffer Buffer3\n"
			<< "{\n"
			<< "  uint data2[];\n"
			<< "};\n"
			<< "\n"
			<< "void main (void)\n"
			<< "{\n"
			<< "  uvec4 mask = subgroupBallot(true);\n"
			<< "  " << subgroups::getFormatNameForGLSL(caseDef.format) << " op = "
			<< getOpTypeName(caseDef.opType) << "(data1[gl_SubgroupInvocationID], data2[gl_SubgroupInvocationID]);\n"
			<< "  uint id = " << idTable[caseDef.opType] << ";\n"
			<< "  if ((0 <= id) && (id < gl_SubgroupSize) && subgroupBallotBitExtract(mask, id))\n"
			<< "  {\n"
			<< "    result[gl_VertexIndex] = (op == data1[id]) ? 1 : 0;\n"
			<< "  }\n"
			<< "  else\n"
			<< "  {\n"
			<< "    result[gl_VertexIndex] = 1; // Invocation we read from was inactive, so we can't verify results!\n"
			<< "  }\n"
			<< "  gl_PointSize = 1.0f;\n"
			<< "}\n";

		programCollection.glslSources.add("vert")
				<< glu::VertexSource(src.str()) << vk::ShaderBuildOptions(vk::SPIRV_VERSION_1_3, 0u);
	}
	else if (VK_SHADER_STAGE_GEOMETRY_BIT == caseDef.shaderStage)
	{
		programCollection.glslSources.add("vert")
				<< glu::VertexSource(subgroups::getVertShaderForStage(caseDef.shaderStage)) << vk::ShaderBuildOptions(vk::SPIRV_VERSION_1_3, 0u);

		std::ostringstream src;

		src << "#version 450\n";

		switch (caseDef.opType)
		{
			case OPTYPE_SHUFFLE:
			case OPTYPE_SHUFFLE_XOR:
				src << "#extension GL_KHR_shader_subgroup_shuffle: enable\n";
				break;
			default:
				src << "#extension GL_KHR_shader_subgroup_shuffle_relative: enable\n";
				break;
		}

		src	<< "#extension GL_KHR_shader_subgroup_ballot: enable\n"
			<< "layout(points) in;\n"
			<< "layout(points, max_vertices = 1) out;\n"
			<< "layout(set = 0, binding = 0, std430) buffer Buffer1\n"
			<< "{\n"
			<< "  uint result[];\n"
			<< "};\n"
			<< "layout(set = 0, binding = 1, std430) buffer Buffer2\n"
			<< "{\n"
			<< "  " << subgroups::getFormatNameForGLSL(caseDef.format) << " data1[];\n"
			<< "};\n"
			<< "layout(set = 0, binding = 2, std430) buffer Buffer3\n"
			<< "{\n"
			<< "  uint data2[];\n"
			<< "};\n"
			<< "\n"
			<< "void main (void)\n"
			<< "{\n"
			<< "  uvec4 mask = subgroupBallot(true);\n"
			<< "  " << subgroups::getFormatNameForGLSL(caseDef.format) << " op = "
			<< getOpTypeName(caseDef.opType) << "(data1[gl_SubgroupInvocationID], data2[gl_SubgroupInvocationID]);\n"
			<< "  uint id = " << idTable[caseDef.opType] << ";\n"
			<< "  if ((0 <= id) && (id < gl_SubgroupSize) && subgroupBallotBitExtract(mask, id))\n"
			<< "  {\n"
			<< "    result[gl_PrimitiveIDIn] = (op == data1[id]) ? 1 : 0;\n"
			<< "  }\n"
			<< "  else\n"
			<< "  {\n"
			<< "    result[gl_PrimitiveIDIn] = 1; // Invocation we read from was inactive, so we can't verify results!\n"
			<< "  }\n"
			<< "}\n";

		programCollection.glslSources.add("geom")
				<< glu::GeometrySource(src.str()) << vk::ShaderBuildOptions(vk::SPIRV_VERSION_1_3, 0u);
	}
	else if (VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT == caseDef.shaderStage)
	{
		programCollection.glslSources.add("vert")
				<< glu::VertexSource(subgroups::getVertShaderForStage(caseDef.shaderStage)) << vk::ShaderBuildOptions(vk::SPIRV_VERSION_1_3, 0u);

		programCollection.glslSources.add("tese")
				<< glu::TessellationEvaluationSource("#version 450\nlayout(isolines) in;\nvoid main (void) {}\n");

		std::ostringstream src;

		src << "#version 450\n";

		switch (caseDef.opType)
		{
			case OPTYPE_SHUFFLE:
			case OPTYPE_SHUFFLE_XOR:
				src << "#extension GL_KHR_shader_subgroup_shuffle: enable\n";
				break;
			default:
				src << "#extension GL_KHR_shader_subgroup_shuffle_relative: enable\n";
				break;
		}

		src	<< "#extension GL_KHR_shader_subgroup_ballot: enable\n"
			<< "layout(vertices=1) out;\n"
			<< "layout(set = 0, binding = 0, std430) buffer Buffer1\n"
			<< "{\n"
			<< "  uint result[];\n"
			<< "};\n"
			<< "layout(set = 0, binding = 1, std430) buffer Buffer2\n"
			<< "{\n"
			<< "  " << subgroups::getFormatNameForGLSL(caseDef.format) << " data1[];\n"
			<< "};\n"
			<< "layout(set = 0, binding = 2, std430) buffer Buffer3\n"
			<< "{\n"
			<< "  uint data2[];\n"
			<< "};\n"
			<< "\n"
			<< "void main (void)\n"
			<< "{\n"
			<< "  uvec4 mask = subgroupBallot(true);\n"
			<< "  " << subgroups::getFormatNameForGLSL(caseDef.format) << " op = "
			<< getOpTypeName(caseDef.opType) << "(data1[gl_SubgroupInvocationID], data2[gl_SubgroupInvocationID]);\n"
			<< "  uint id = " << idTable[caseDef.opType] << ";\n"
			<< "  if ((0 <= id) && (id < gl_SubgroupSize) && subgroupBallotBitExtract(mask, id))\n"
			<< "  {\n"
			<< "    result[gl_PrimitiveID] = (op == data1[id]) ? 1 : 0;\n"
			<< "  }\n"
			<< "  else\n"
			<< "  {\n"
			<< "    result[gl_PrimitiveID] = 1; // Invocation we read from was inactive, so we can't verify results!\n"
			<< "  }\n"
			<< "}\n";

		programCollection.glslSources.add("tesc")
				<< glu::TessellationControlSource(src.str()) << vk::ShaderBuildOptions(vk::SPIRV_VERSION_1_3, 0u);
	}
	else if (VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT == caseDef.shaderStage)
	{
		programCollection.glslSources.add("vert")
				<< glu::VertexSource(subgroups::getVertShaderForStage(caseDef.shaderStage)) << vk::ShaderBuildOptions(vk::SPIRV_VERSION_1_3, 0u);

		programCollection.glslSources.add("tesc")
				<< glu::TessellationControlSource("#version 450\nlayout(vertices=1) out;\nvoid main (void) { for(uint i = 0; i < 4; i++) { gl_TessLevelOuter[i] = 1.0f; } }\n");

		std::ostringstream src;

		src << "#version 450\n";

		switch (caseDef.opType)
		{
			case OPTYPE_SHUFFLE:
			case OPTYPE_SHUFFLE_XOR:
				src << "#extension GL_KHR_shader_subgroup_shuffle: enable\n";
				break;
			default:
				src << "#extension GL_KHR_shader_subgroup_shuffle_relative: enable\n";
				break;
		}

		src	<< "#extension GL_KHR_shader_subgroup_ballot: enable\n"
			<< "layout(isolines) in;\n"
			<< "layout(set = 0, binding = 0, std430) buffer Buffer1\n"
			<< "{\n"
			<< "  uint result[];\n"
			<< "};\n"
			<< "layout(set = 0, binding = 1, std430) buffer Buffer2\n"
			<< "{\n"
			<< "  " << subgroups::getFormatNameForGLSL(caseDef.format) << " data1[];\n"
			<< "};\n"
			<< "layout(set = 0, binding = 2, std430) buffer Buffer3\n"
			<< "{\n"
			<< "  uint data2[];\n"
			<< "};\n"
			<< "\n"
			<< "void main (void)\n"
			<< "{\n"
			<< "  uvec4 mask = subgroupBallot(true);\n"
			<< "  " << subgroups::getFormatNameForGLSL(caseDef.format) << " op = "
			<< getOpTypeName(caseDef.opType) << "(data1[gl_SubgroupInvocationID], data2[gl_SubgroupInvocationID]);\n"
			<< "  uint id = " << idTable[caseDef.opType] << ";\n"
			<< "  if ((0 <= id) && (id < gl_SubgroupSize) && subgroupBallotBitExtract(mask, id))\n"
			<< "  {\n"
			<< "    result[gl_PrimitiveID * 2 + uint(gl_TessCoord.x + 0.5)] = (op == data1[id]) ? 1 : 0;\n"
			<< "  }\n"
			<< "  else\n"
			<< "  {\n"
			<< "    result[gl_PrimitiveID * 2 + uint(gl_TessCoord.x + 0.5)] = 1; // Invocation we read from was inactive, so we can't verify results!\n"
			<< "  }\n"
			<< "}\n";

		programCollection.glslSources.add("tese")
				<< glu::TessellationEvaluationSource(src.str()) << vk::ShaderBuildOptions(vk::SPIRV_VERSION_1_3, 0u);
	}
	else
	{
		DE_FATAL("Unsupported shader stage");
	}
}

tcu::TestStatus test(Context& context, const CaseDefinition caseDef)
{
	if (!subgroups::isSubgroupSupported(context))
		TCU_THROW(NotSupportedError, "Subgroup operations are not supported");

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

	switch (caseDef.opType)
	{
		case OPTYPE_SHUFFLE:
		case OPTYPE_SHUFFLE_XOR:
			if (!subgroups::isSubgroupFeatureSupportedForDevice(context, VK_SUBGROUP_FEATURE_SHUFFLE_BIT))
			{
				TCU_THROW(NotSupportedError, "Device does not support subgroup shuffle operations");
			}
			break;
		default:
			if (!subgroups::isSubgroupFeatureSupportedForDevice(context, VK_SUBGROUP_FEATURE_SHUFFLE_RELATIVE_BIT))
			{
				TCU_THROW(NotSupportedError, "Device does not support subgroup shuffle relative operations");
			}
			break;
	}

	if (subgroups::isDoubleFormat(caseDef.format) &&
			!subgroups::isDoubleSupportedForDevice(context))
	{
		TCU_THROW(NotSupportedError, "Device does not support subgroup double operations");
	}

	//Tests which don't use the SSBO
	if (caseDef.noSSBO && VK_SHADER_STAGE_VERTEX_BIT == caseDef.shaderStage)
	{
		subgroups::SSBOData inputData[2];
		inputData[0].format = caseDef.format;
		inputData[0].numElements = subgroups::maxSupportedSubgroupSize();
		inputData[0].initializeType = subgroups::SSBOData::InitializeNonZero;

		inputData[1].format = VK_FORMAT_R32_UINT;
		inputData[1].numElements = inputData[0].numElements;
		inputData[1].initializeType = subgroups::SSBOData::InitializeNonZero;

		return subgroups::makeVertexFrameBufferTest(context, VK_FORMAT_R32_UINT,  inputData, 2, checkVertexPipelineStages);
	}

	if ((VK_SHADER_STAGE_FRAGMENT_BIT != caseDef.shaderStage) &&
			(VK_SHADER_STAGE_COMPUTE_BIT != caseDef.shaderStage))
	{
		if (!subgroups::isVertexSSBOSupportedForDevice(context))
		{
			TCU_THROW(NotSupportedError, "Device does not support vertex stage SSBO writes");
		}
	}

	subgroups::SSBOData inputData[2];
	inputData[0].format = caseDef.format;
	inputData[0].numElements = subgroups::maxSupportedSubgroupSize();
	inputData[0].initializeType = subgroups::SSBOData::InitializeNonZero;

	inputData[1].format = VK_FORMAT_R32_UINT;
	inputData[1].numElements = inputData[0].numElements;
	inputData[1].initializeType = subgroups::SSBOData::InitializeNonZero;

	if (VK_SHADER_STAGE_FRAGMENT_BIT == caseDef.shaderStage)
	{
		return subgroups::makeFragmentTest(context, VK_FORMAT_R32_UINT,
										   inputData, 2, checkFragment);
	}
	else if (VK_SHADER_STAGE_COMPUTE_BIT == caseDef.shaderStage)
	{
		return subgroups::makeComputeTest(context, VK_FORMAT_R32_UINT,
										  inputData, 2, checkCompute);
	}
	else if (VK_SHADER_STAGE_VERTEX_BIT == caseDef.shaderStage)
	{
		return subgroups::makeVertexTest(context, VK_FORMAT_R32_UINT,
										 inputData, 2, checkVertexPipelineStages);
	}
	else if (VK_SHADER_STAGE_GEOMETRY_BIT == caseDef.shaderStage)
	{
		return subgroups::makeGeometryTest(context, VK_FORMAT_R32_UINT,
										   inputData, 2, checkVertexPipelineStages);
	}
	else if (VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT == caseDef.shaderStage)
	{
		return subgroups::makeTessellationControlTest(context, VK_FORMAT_R32_UINT,
				inputData, 2, checkVertexPipelineStages);
	}
	else if (VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT == caseDef.shaderStage)
	{
		return subgroups::makeTessellationEvaluationTest(context, VK_FORMAT_R32_UINT,
				inputData, 2, checkVertexPipelineStages);
	}
	else
	{
		TCU_THROW(InternalError, "Unhandled shader stage");
	}
}
}

namespace vkt
{
namespace subgroups
{
tcu::TestCaseGroup* createSubgroupsShuffleTests(tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(
			testCtx, "shuffle", "Subgroup shuffle category tests"));

	const VkShaderStageFlags stages[] =
	{
		VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,
		VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,
		VK_SHADER_STAGE_GEOMETRY_BIT,
		VK_SHADER_STAGE_VERTEX_BIT,
		VK_SHADER_STAGE_FRAGMENT_BIT,
		VK_SHADER_STAGE_COMPUTE_BIT
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

	for (int stageIndex = 0; stageIndex < DE_LENGTH_OF_ARRAY(stages); ++stageIndex)
	{
		const VkShaderStageFlags stage = stages[stageIndex];

		for (int formatIndex = 0; formatIndex < DE_LENGTH_OF_ARRAY(formats); ++formatIndex)
		{
			const VkFormat format = formats[formatIndex];

			for (int opTypeIndex = 0; opTypeIndex < OPTYPE_LAST; ++opTypeIndex)
			{
				CaseDefinition caseDef = {opTypeIndex, stage, format, false};

				std::ostringstream name;

				std::string op = getOpTypeName(opTypeIndex);

				name << de::toLower(op)
					 << "_" << subgroups::getFormatNameForGLSL(format)
					 << "_" << getShaderStageName(stage);

				addFunctionCaseWithPrograms(group.get(), name.str(),
											"", initPrograms, test, caseDef);

				if (VK_SHADER_STAGE_VERTEX_BIT == stage )
				{
					caseDef.noSSBO = true;
					addFunctionCaseWithPrograms(group.get(), name.str()+"_framebuffer", "",
												initFrameBufferPrograms, test, caseDef);
				}
			}
		}
	}

	return group.release();
}

} // subgroups
} // vkt
