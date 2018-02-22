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

#include "vktSubgroupsBasicTests.hpp"
#include "vktSubgroupsTestsUtils.hpp"

#include <string>
#include <vector>

using namespace tcu;
using namespace std;
using namespace vk;
using namespace vkt;

namespace
{
static const deUint32			ELECTED_VALUE		= 42u;
static const deUint32			UNELECTED_VALUE		= 13u;
static const vk::VkDeviceSize	SHADER_BUFFER_SIZE	= 4096ull; // min(maxUniformBufferRange, maxImageDimension1D)

static bool checkFragmentSubgroupBarriersNoSSBO(std::vector<const void*> datas,
		deUint32 width, deUint32 height, deUint32)
{
	const float* const	resultData	= reinterpret_cast<const float*>(datas[0]);

	for (deUint32 x = 0u; x < width; ++x)
	{
		for (deUint32 y = 0u; y < height; ++y)
		{
			const deUint32 ndx = (x * height + y) * 4u;
			if (1.0f == resultData[ndx +2])
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

static bool checkVertexPipelineStagesSubgroupElectNoSSBO(std::vector<const void*> datas,
		deUint32 width, deUint32)
{
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

static bool checkVertexPipelineStagesSubgroupElect(std::vector<const void*> datas,
		deUint32 width, deUint32)
{
	const deUint32* const resultData =
		reinterpret_cast<const deUint32*>(datas[0]);
	deUint32 poisonValuesFound = 0;

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
	const deUint32 numSubgroupsUsed =
		*reinterpret_cast<const deUint32*>(datas[1]);

	return numSubgroupsUsed == poisonValuesFound;
}

static bool checkVertexPipelineStagesSubgroupBarriers(std::vector<const void*> datas,
		deUint32 width, deUint32)
{
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

static bool checkVertexPipelineStagesSubgroupBarriersNoSSBO(std::vector<const void*> datas,
		deUint32 width, deUint32)
{
	const float* const	resultData	= reinterpret_cast<const float*>(datas[0]);

	for (deUint32 x = 0u; x < width; ++x)
	{
		const deUint32 ndx = x*4u;
		if (1.0f == resultData[ndx +2])
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

static bool checkComputeSubgroupElect(std::vector<const void*> datas,
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
							if (1 != data[offset])
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

static bool checkComputeSubgroupBarriers(std::vector<const void*> datas,
		const deUint32 numWorkgroups[3], const deUint32 localSize[3],
		deUint32)
{
	const deUint32* data = reinterpret_cast<const deUint32*>(datas[0]);

	// We used this SSBO to generate our unique value!
	const deUint32 ref = *reinterpret_cast<const deUint32*>(datas[2]);

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

							if (ref != data[offset])
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

std::string getOpTypeName(int opType)
{
	switch (opType)
	{
		default:
			DE_FATAL("Unsupported op type");
		case OPTYPE_ELECT:
			return "subgroupElect";
		case OPTYPE_SUBGROUP_BARRIER:
			return "subgroupBarrier";
		case OPTYPE_SUBGROUP_MEMORY_BARRIER:
			return "subgroupMemoryBarrier";
		case OPTYPE_SUBGROUP_MEMORY_BARRIER_BUFFER:
			return "subgroupMemoryBarrierBuffer";
		case OPTYPE_SUBGROUP_MEMORY_BARRIER_SHARED:
			return "subgroupMemoryBarrierShared";
		case OPTYPE_SUBGROUP_MEMORY_BARRIER_IMAGE:
			return "subgroupMemoryBarrierImage";
	}
}

struct CaseDefinition
{
	int					opType;
	VkShaderStageFlags	shaderStage;
	bool				noSSBO;
};

void initFrameBufferPrograms(SourceCollections& programCollection, CaseDefinition caseDef)
{
	std::ostringstream			vertexSrc;
	std::ostringstream			fragmentSrc;
	if(VK_SHADER_STAGE_VERTEX_BIT == caseDef.shaderStage)
	{
		fragmentSrc	<< glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450)<<"\n"
			<< "layout(location = 0) in vec4 in_color;\n"
			<< "layout(location = 0) out vec4 out_color;\n"
			<< "void main()\n"
			<<"{\n"
			<< "	out_color = in_color;\n"
			<< "}\n";
		programCollection.glslSources.add("fragment") << glu::FragmentSource(fragmentSrc.str()) << vk::ShaderBuildOptions(vk::SPIRV_VERSION_1_3, 0u);
	}
	else if (VK_SHADER_STAGE_FRAGMENT_BIT == caseDef.shaderStage)
	{
		programCollection.glslSources.add("vert") << glu::VertexSource(subgroups::getVertShaderForStage(caseDef.shaderStage)) << vk::ShaderBuildOptions(vk::SPIRV_VERSION_1_3, 0u);
	}

	if (OPTYPE_ELECT == caseDef.opType)
	{
		if (VK_SHADER_STAGE_VERTEX_BIT == caseDef.shaderStage)
		{
			vertexSrc	<< glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450)<<"\n"
				<< "#extension GL_KHR_shader_subgroup_basic: enable\n"
				<< "layout(location = 0) out vec4 out_color;\n"
				<< "layout(location = 0) in highp vec4 in_position;\n"
				<< "\n"
				<< "void main (void)\n"
				<< "{\n"
				<< "  if (subgroupElect())\n"
				<< "  {\n"
				<< "    out_color.r = " << ELECTED_VALUE << ";\n"
				<< "    out_color.g = 1.0f;\n"
				<< "  }\n"
				<< "  else\n"
				<< "  {\n"
				<< "    out_color.r = " << UNELECTED_VALUE << ";\n"
				<< "    out_color.g = 0.0f;\n"
				<< "  }\n"
				<< "  gl_Position = in_position;\n"
				<< "  gl_PointSize = 1.0f;\n"
				<< "}\n";
			programCollection.glslSources.add("vert")
					<< glu::VertexSource(vertexSrc.str()) << vk::ShaderBuildOptions(vk::SPIRV_VERSION_1_3, 0u);
		}
		else
		{
			DE_FATAL("Unsupported shader stage");
		}
	}
	else
	{
		std::ostringstream bdy;
		switch (caseDef.opType)
		{
			default:
				DE_FATAL("Unhandled op type!");
			case OPTYPE_SUBGROUP_BARRIER:
			case OPTYPE_SUBGROUP_MEMORY_BARRIER:
			case OPTYPE_SUBGROUP_MEMORY_BARRIER_BUFFER:
				bdy << " tempResult2 = tempBuffer[id];\n"
					<< "  if (subgroupElect())\n"
					<< "  {\n"
					<< "    tempResult = value;\n"
					<< "    out_color.b = 1.0f;\n"
					<< "  }\n"
					 << "  else\n"
					<< "  {\n"
					<< "    tempResult = tempBuffer[id];\n"
					<< "  }\n"
					<< "  " << getOpTypeName(caseDef.opType) << "();\n";
				break;
			case OPTYPE_SUBGROUP_MEMORY_BARRIER_IMAGE:
				bdy <<"tempResult2 = imageLoad(tempImage, ivec2(id, 0)).x;\n"
					<< "  if (subgroupElect())\n"
					<< "  {\n"
					<< "    tempResult = value;\n"
					<< "    out_color.b = 1.0f;\n"
					<< "  }\n"
					<< "  else\n"
					<< "  {\n"
					<< "    tempResult = imageLoad(tempImage, ivec2(id, 0)).x;\n"
					<< "  }\n"
					<< "  subgroupMemoryBarrierImage();\n";

				break;
		}

		if (VK_SHADER_STAGE_FRAGMENT_BIT == caseDef.shaderStage)
		{
			fragmentSrc	<< glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450)<<"\n"
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

			programCollection.glslSources.add("fragment") << glu::FragmentSource(fragmentSrc.str()) << vk::ShaderBuildOptions(vk::SPIRV_VERSION_1_3, 0u);
		}
		else if (VK_SHADER_STAGE_VERTEX_BIT == caseDef.shaderStage)
		{
			vertexSrc	<< glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450)<<"\n"
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

			programCollection.glslSources.add("vert") << glu::VertexSource(vertexSrc.str()) << vk::ShaderBuildOptions(vk::SPIRV_VERSION_1_3, 0u);
		}
		else
		{
			DE_FATAL("Unsupported shader stage");
		}
	}
}

void initPrograms(SourceCollections& programCollection, CaseDefinition caseDef)
{
	if (OPTYPE_ELECT == caseDef.opType)
	{
		if (VK_SHADER_STAGE_COMPUTE_BIT == caseDef.shaderStage)
		{
			std::ostringstream src;

			src << "#version 450\n"
				<< "#extension GL_KHR_shader_subgroup_basic: enable\n"
				<< "layout (local_size_x_id = 0, local_size_y_id = 1, "
				"local_size_z_id = 2) in;\n"
				<< "layout(set = 0, binding = 0, std430) buffer Buffer1\n"
				<< "{\n"
				<< "  uint result[];\n"
				<< "};\n"
				<< "\n"
				<< subgroups::getSharedMemoryBallotHelper()
				<< "void main (void)\n"
				<< "{\n"
				<< "  uvec3 globalSize = gl_NumWorkGroups * gl_WorkGroupSize;\n"
				<< "  highp uint offset = globalSize.x * ((globalSize.y * "
				"gl_GlobalInvocationID.z) + gl_GlobalInvocationID.y) + "
				"gl_GlobalInvocationID.x;\n"
				<< "  uint value = " << UNELECTED_VALUE << ";\n"
				<< "  if (subgroupElect())\n"
				<< "  {\n"
				<< "    value = " << ELECTED_VALUE << ";\n"
				<< "  }\n"
				<< "  uvec4 bits = bitCount(sharedMemoryBallot(value == " << ELECTED_VALUE << "));\n"
				<< "  result[offset] = bits.x + bits.y + bits.z + bits.w;\n"
				<< "}\n";

			programCollection.glslSources.add("comp")
					<< glu::ComputeSource(src.str()) << vk::ShaderBuildOptions(vk::SPIRV_VERSION_1_3, 0u);
		}
		else
		{
			{
				std::ostringstream  vertex;
				vertex	<< "#version 450\n"
						<< "#extension GL_KHR_shader_subgroup_basic: enable\n"
						<< "layout(set = 0, binding = 0, std430) buffer Buffer1\n"
						<< "{\n"
						<< "  uint result[];\n"
						<< "};\n"
						<< "layout(set = 0, binding = 4, std430) buffer Buffer2\n"
						<< "{\n"
						<< "  uint numSubgroupsExecuted;\n"
						<< "};\n"
						<< "\n"
						<< "void main (void)\n"
						<< "{\n"
						<< "  if (subgroupElect())\n"
						<< "  {\n"
						<< "    result[gl_VertexIndex] = " << ELECTED_VALUE << ";\n"
						<< "    atomicAdd(numSubgroupsExecuted, 1);\n"
						<< "  }\n"
						<< "  else\n"
						<< "  {\n"
						<< "    result[gl_VertexIndex] = " << UNELECTED_VALUE << ";\n"
						<< "  }\n"
						<< "  float pixelSize = 2.0f/1024.0f;\n"
						<< "  float pixelPosition = pixelSize/2.0f - 1.0f;\n"
						<< "  gl_Position = vec4(float(gl_VertexIndex) * pixelSize + pixelPosition, 0.0f, 0.0f, 1.0f);\n"
						<< "  gl_PointSize = 1.0f;\n"
						<< "}\n";
				programCollection.glslSources.add("vert")
					<< glu::VertexSource(vertex.str()) << vk::ShaderBuildOptions(vk::SPIRV_VERSION_1_3, 0u);
			}

			{
				std::ostringstream tesc;
				tesc	<< "#version 450\n"
						<< "#extension GL_KHR_shader_subgroup_basic: enable\n"
						<< "layout(vertices=1) out;\n"
						<< "layout(set = 0, binding = 1, std430) buffer Buffer1\n"
						<< "{\n"
						<< "  uint result[];\n"
						<< "};\n"
						<< "layout(set = 0, binding = 5, std430) buffer Buffer2\n"
						<< "{\n"
						<< "  uint numSubgroupsExecuted;\n"
						<< "};\n"
						<< "\n"
						<< "void main (void)\n"
						<< "{\n"
						<< "  if (subgroupElect())\n"
						<< "  {\n"
						<< "    result[gl_PrimitiveID] = " << ELECTED_VALUE << ";\n"
						<< "    atomicAdd(numSubgroupsExecuted, 1);\n"
						<< "  }\n"
						<< "  else\n"
						<< "  {\n"
						<< "    result[gl_PrimitiveID] = " << UNELECTED_VALUE << ";\n"
						<< "  }\n"
						<< "  if (gl_InvocationID == 0)\n"
						<< "  {\n"
						<< "    gl_TessLevelOuter[0] = 1.0f;\n"
						<< "    gl_TessLevelOuter[1] = 1.0f;\n"
						<< "  }\n"
						<< "  gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;\n"
						<< "}\n";
				programCollection.glslSources.add("tesc")
					<< glu::TessellationControlSource(tesc.str()) << vk::ShaderBuildOptions(vk::SPIRV_VERSION_1_3, 0u);
			}

			{
				std::ostringstream tese;
				tese	<< "#version 450\n"
						<< "#extension GL_KHR_shader_subgroup_basic: enable\n"
						<< "layout(isolines) in;\n"
						<< "layout(set = 0, binding = 2, std430) buffer Buffer1\n"
						<< "{\n"
						<< "  uint result[];\n"
						<< "};\n"
						<< "layout(set = 0, binding = 6, std430) buffer Buffer2\n"
						<< "{\n"
						<< "  uint numSubgroupsExecuted;\n"
						<< "};\n"
						<< "\n"
						<< "void main (void)\n"
						<< "{\n"
						<< "  if (subgroupElect())\n"
						<< "  {\n"
						<< "    result[gl_PrimitiveID * 2 + uint(gl_TessCoord.x + 0.5)] = " << ELECTED_VALUE << ";\n"
						<< "    atomicAdd(numSubgroupsExecuted, 1);\n"
						<< "  }\n"
						<< "  else\n"
						<< "  {\n"
						<< "    result[gl_PrimitiveID * 2 + uint(gl_TessCoord.x + 0.5)] = " << UNELECTED_VALUE << ";\n"
						<< "  }\n"
						<< "  float pixelSize = 2.0f/1024.0f;\n"
						<< "  gl_Position = gl_in[0].gl_Position + gl_TessCoord.x * pixelSize / 2.0f;\n"
						<< "}\n";
				programCollection.glslSources.add("tese")
					<< glu::TessellationEvaluationSource(tese.str()) << vk::ShaderBuildOptions(vk::SPIRV_VERSION_1_3, 0u);
			}

			{
				std::ostringstream geometry;
				geometry	<< "#version 450\n"
							<< "#extension GL_KHR_shader_subgroup_basic: enable\n"
							<< "layout(${TOPOLOGY}) in;\n"
							<< "layout(points, max_vertices = 1) out;\n"
							<< "layout(set = 0, binding = 3, std430) buffer Buffer1\n"
							<< "{\n"
							<< "  uint result[];\n"
							<< "};\n"
							<< "layout(set = 0, binding = 7, std430) buffer Buffer2\n"
							<< "{\n"
							<< "  uint numSubgroupsExecuted;\n"
							<< "};\n"
							<< "\n"
							<< "void main (void)\n"
							<< "{\n"
							<< "  if (subgroupElect())\n"
							<< "  {\n"
							<< "    result[gl_PrimitiveIDIn] = " << ELECTED_VALUE << ";\n"
							<< "    atomicAdd(numSubgroupsExecuted, 1);\n"
							<< "  }\n"
							<< "  else\n"
							<< "  {\n"
							<< "    result[gl_PrimitiveIDIn] = " << UNELECTED_VALUE << ";\n"
							<< "  }\n"
							<< "  gl_Position = gl_in[0].gl_Position;\n"
							<< "  EmitVertex();\n"
							<< "  EndPrimitive();\n"
							<< "}\n";
				subgroups::addGeometryShadersFromTemplate(geometry.str(), vk::ShaderBuildOptions(vk::SPIRV_VERSION_1_3, 0u),
														  programCollection.glslSources);
			}

			{
				std::ostringstream fragment;
				fragment	<< "#version 450\n"
							<< "#extension GL_KHR_shader_subgroup_basic: enable\n"
							<< "layout(location = 0) out uint data;\n"
							<< "layout(set = 0, binding = 8, std430) buffer Buffer\n"
							<< "{\n"
							<< "  uint numSubgroupsExecuted;\n"
							<< "};\n"
							<< "void main (void)\n"
							<< "{\n"
							<< "  if (gl_HelperInvocation) return;\n"
							<< "  if (subgroupElect())\n"
							<< "  {\n"
							<< "    data = " << ELECTED_VALUE << ";\n"
							<< "    atomicAdd(numSubgroupsExecuted, 1);\n"
							<< "  }\n"
							<< "  else\n"
							<< "  {\n"
							<< "    data = " << UNELECTED_VALUE << ";\n"
							<< "  }\n"
							<< "}\n";
				programCollection.glslSources.add("fragment")
					<< glu::FragmentSource(fragment.str())<< vk::ShaderBuildOptions(vk::SPIRV_VERSION_1_3, 0u);
			}
			subgroups::addNoSubgroupShader(programCollection);
		}
	}
	else
	{
		std::ostringstream bdy;

		switch (caseDef.opType)
		{
			default:
				DE_FATAL("Unhandled op type!");
			case OPTYPE_SUBGROUP_BARRIER:
			case OPTYPE_SUBGROUP_MEMORY_BARRIER:
			case OPTYPE_SUBGROUP_MEMORY_BARRIER_BUFFER:
				bdy << "  if (subgroupElect())\n"
					<< "  {\n"
					<< "    tempBuffer[id] = value;\n"
					<< "  }\n"
					<< "  " << getOpTypeName(caseDef.opType) << "();\n"
					<< "  tempResult = tempBuffer[id];\n";
				break;
			case OPTYPE_SUBGROUP_MEMORY_BARRIER_SHARED:
				bdy << "  if (subgroupElect())\n"
					<< "  {\n"
					<< "    tempShared[localId] = value;\n"
					<< "  }\n"
					<< "  subgroupMemoryBarrierShared();\n"
					<< "  tempResult = tempShared[localId];\n";
				break;
			case OPTYPE_SUBGROUP_MEMORY_BARRIER_IMAGE:
				bdy << "  if (subgroupElect())\n"
					<< "  {\n"
					<< "    imageStore(tempImage, ivec2(id, 0), ivec4(value));\n"
					<< "  }\n"
					<< "  subgroupMemoryBarrierImage();\n"
					<< "  tempResult = imageLoad(tempImage, ivec2(id, 0)).x;\n";
				break;
		}

		if (VK_SHADER_STAGE_COMPUTE_BIT == caseDef.shaderStage)
		{
			std::ostringstream src;

			src << "#version 450\n"
				<< "#extension GL_KHR_shader_subgroup_basic: enable\n"
				<< "layout (local_size_x_id = 0, local_size_y_id = 1, "
				"local_size_z_id = 2) in;\n"
				<< "layout(set = 0, binding = 0, std430) buffer Buffer1\n"
				<< "{\n"
				<< "  uint result[];\n"
				<< "};\n"
				<< "layout(set = 0, binding = 1, std430) buffer Buffer2\n"
				<< "{\n"
				<< "  uint tempBuffer[];\n"
				<< "};\n"
				<< "layout(set = 0, binding = 2, std430) buffer Buffer3\n"
				<< "{\n"
				<< "  uint value;\n"
				<< "};\n"
				<< "layout(set = 0, binding = 3, r32ui) uniform uimage2D tempImage;\n"
				<< "shared uint tempShared[gl_WorkGroupSize.x * gl_WorkGroupSize.y * gl_WorkGroupSize.z];\n"
				<< "\n"
				<< "void main (void)\n"
				<< "{\n"
				<< "  uvec3 globalSize = gl_NumWorkGroups * gl_WorkGroupSize;\n"
				<< "  highp uint offset = globalSize.x * ((globalSize.y * "
				"gl_GlobalInvocationID.z) + gl_GlobalInvocationID.y) + "
				"gl_GlobalInvocationID.x;\n"
				<< "  uint localId = gl_SubgroupID;\n"
				<< "  uint id = globalSize.x * ((globalSize.y * "
				"gl_WorkGroupID.z) + gl_WorkGroupID.y) + "
				"gl_WorkGroupID.x + localId;\n"
				<< "  uint tempResult = 0;\n"
				<< bdy.str()
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
					"#extension GL_KHR_shader_subgroup_basic: enable\n"
					"#extension GL_KHR_shader_subgroup_ballot: enable\n"
					"layout(set = 0, binding = 0, std430) buffer Buffer1\n"
					"{\n"
					"  uint result[];\n"
					"};\n"
					"layout(set = 0, binding = 4, std430) buffer Buffer2\n"
					"{\n"
					"  uint tempBuffer[];\n"
					"};\n"
					"layout(set = 0, binding = 5, std430) buffer Buffer3\n"
					"{\n"
					"  uint subgroupID;\n"
					"};\n"
					"layout(set = 0, binding = 6, std430) buffer Buffer4\n"
					"{\n"
					"  uint value;\n"
					"};\n"
					"layout(set = 0, binding = 7, r32ui) uniform uimage2D tempImage;\n"
					"void main (void)\n"
					"{\n"
					"  uint id = 0;\n"
					"  if (subgroupElect())\n"
					"  {\n"
					"    id = atomicAdd(subgroupID, 1);\n"
					"  }\n"
					"  id = subgroupBroadcastFirst(id);\n"
					"  uint localId = id;\n"
					"  uint tempResult = 0;\n"
					+ bdy.str() +
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
					"#extension GL_KHR_shader_subgroup_basic: enable\n"
					"#extension GL_KHR_shader_subgroup_ballot: enable\n"
					"layout(vertices=1) out;\n"
					"layout(set = 0, binding = 1, std430) buffer Buffer1\n"
					"{\n"
					"  uint result[];\n"
					"};\n"
					"layout(set = 0, binding = 8, std430) buffer Buffer2\n"
					"{\n"
					"  uint tempBuffer[];\n"
					"};\n"
					"layout(set = 0, binding = 9, std430) buffer Buffer3\n"
					"{\n"
					"  uint subgroupID;\n"
					"};\n"
					"layout(set = 0, binding = 10, std430) buffer Buffer4\n"
					"{\n"
					"  uint value;\n"
					"};\n"
					"layout(set = 0, binding = 11, r32ui) uniform uimage2D tempImage;\n"
					"void main (void)\n"
					"{\n"
					"  uint id = 0;\n"
					"  if (subgroupElect())\n"
					"  {\n"
					"    id = atomicAdd(subgroupID, 1);\n"
					"  }\n"
					"  id = subgroupBroadcastFirst(id);\n"
					"  uint localId = id;\n"
					"  uint tempResult = 0;\n"
					+ bdy.str() +
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
					"#extension GL_KHR_shader_subgroup_basic: enable\n"
					"#extension GL_KHR_shader_subgroup_ballot: enable\n"
					"layout(isolines) in;\n"
					"layout(set = 0, binding = 2, std430) buffer Buffer1\n"
					"{\n"
					"  uint result[];\n"
					"};\n"
					"layout(set = 0, binding = 12, std430) buffer Buffer2\n"
					"{\n"
					"  uint tempBuffer[];\n"
					"};\n"
					"layout(set = 0, binding = 13, std430) buffer Buffer3\n"
					"{\n"
					"  uint subgroupID;\n"
					"};\n"
					"layout(set = 0, binding = 14, std430) buffer Buffer4\n"
					"{\n"
					"  uint value;\n"
					"};\n"
					"layout(set = 0, binding = 15, r32ui) uniform uimage2D tempImage;\n"
					"void main (void)\n"
					"{\n"
					"  uint id = 0;\n"
					"  if (subgroupElect())\n"
					"  {\n"
					"    id = atomicAdd(subgroupID, 1);\n"
					"  }\n"
					"  id = subgroupBroadcastFirst(id);\n"
					"  uint localId = id;\n"
					"  uint tempResult = 0;\n"
					+ bdy.str() +
					"  result[gl_PrimitiveID * 2 + uint(gl_TessCoord.x + 0.5)] = tempResult;\n"
					"  float pixelSize = 2.0f/1024.0f;\n""  gl_Position = gl_in[0].gl_Position + gl_TessCoord.x * pixelSize / 2.0f;\n"
					"}\n";
				programCollection.glslSources.add("tese")
					<< glu::TessellationEvaluationSource(tese) << vk::ShaderBuildOptions(vk::SPIRV_VERSION_1_3, 0u);
			}

			{
				const string geometry =
					"#version 450\n"
					"#extension GL_KHR_shader_subgroup_basic: enable\n"
					"#extension GL_KHR_shader_subgroup_ballot: enable\n"
					"layout(${TOPOLOGY}) in;\n"
					"layout(points, max_vertices = 1) out;\n"
					"layout(set = 0, binding = 3, std430) buffer Buffer1\n"
					"{\n"
					"  uint result[];\n"
					"};\n"
					"layout(set = 0, binding = 16, std430) buffer Buffer2\n"
					"{\n"
					"  uint tempBuffer[];\n"
					"};\n"
					"layout(set = 0, binding = 17, std430) buffer Buffer3\n"
					"{\n"
					"  uint subgroupID;\n"
					"};\n"
					"layout(set = 0, binding = 18, std430) buffer Buffer4\n"
					"{\n"
					"  uint value;\n"
					"};\n"
					"layout(set = 0, binding = 19, r32ui) uniform uimage2D tempImage;\n"
					"void main (void)\n"
					"{\n"
					"  uint id = 0;\n"
					"  if (subgroupElect())\n"
					"  {\n"
					"    id = atomicAdd(subgroupID, 1);\n"
					"  }\n"
					"  id = subgroupBroadcastFirst(id);\n"
					"  uint localId = id;\n"
					"  uint tempResult = 0;\n"
					 + bdy.str() +
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
					"#extension GL_KHR_shader_subgroup_basic: enable\n"
					"#extension GL_KHR_shader_subgroup_ballot: enable\n"
					"layout(location = 0) out uint result;\n"
					"layout(set = 0, binding = 20, std430) buffer Buffer1\n"
					"{\n"
					"  uint tempBuffer[];\n"
					"};\n"
					"layout(set = 0, binding = 21, std430) buffer Buffer2\n"
					"{\n"
					"  uint subgroupID;\n"
					"};\n"
					"layout(set = 0, binding = 22, std430) buffer Buffer3\n"
					"{\n"
					"  uint value;\n"
					"};\n"
					"layout(set = 0, binding = 23, r32ui) uniform uimage2D tempImage;\n"
					"void main (void)\n"
					"{\n"
					"  if (gl_HelperInvocation) return;\n"
					"  uint id = 0;\n"
					"  if (subgroupElect())\n"
					"  {\n"
					"    id = atomicAdd(subgroupID, 1);\n"
					"  }\n"
					"  id = subgroupBroadcastFirst(id);\n"
					"  uint localId = id;\n"
					"  uint tempResult = 0;\n"
					+ bdy.str() +
					"  result = tempResult;\n"
					"}\n";
				programCollection.glslSources.add("fragment")
					<< glu::FragmentSource(fragment)<< vk::ShaderBuildOptions(vk::SPIRV_VERSION_1_3, 0u);
			}

		subgroups::addNoSubgroupShader(programCollection);
		}
	}
}

tcu::TestStatus test(Context& context, const CaseDefinition caseDef)
{
	if (!subgroups::isSubgroupSupported(context))
		TCU_THROW(NotSupportedError, "Subgroup operations are not supported");

	if (!subgroups::isSubgroupFeatureSupportedForDevice(context, VK_SUBGROUP_FEATURE_BASIC_BIT))
	{
		return tcu::TestStatus::fail(
					"Subgroup feature " +
					subgroups::getSubgroupFeatureName(VK_SUBGROUP_FEATURE_BASIC_BIT) +
					" is a required capability!");
	}

	if (OPTYPE_ELECT != caseDef.opType && VK_SHADER_STAGE_COMPUTE_BIT != caseDef.shaderStage)
	{
		if (!subgroups::isSubgroupFeatureSupportedForDevice(context, VK_SUBGROUP_FEATURE_BALLOT_BIT))
		{
			TCU_THROW(NotSupportedError, "Subgroup basic operation non-compute stage test required that ballot operations are supported!");
		}
	}

	//Tests which don't use the SSBO
	if(caseDef.noSSBO)
	{
		if (!subgroups::areSubgroupOperationsSupportedForStage(context, caseDef.shaderStage))
		{
			TCU_THROW(NotSupportedError, "Device does not support subgroup operations for this stage");
		}

		if (VK_SHADER_STAGE_VERTEX_BIT == caseDef.shaderStage)
		{
			if (OPTYPE_ELECT == caseDef.opType)
			{
				return subgroups::makeVertexFrameBufferTest(context, VK_FORMAT_R32G32_SFLOAT, DE_NULL, 0u, checkVertexPipelineStagesSubgroupElectNoSSBO);
			}
			else
			{
				const deUint32						inputDatasCount	= OPTYPE_SUBGROUP_MEMORY_BARRIER_IMAGE == caseDef.opType ? 3u : 2u;
				std::vector<subgroups::SSBOData>	inputDatas		(inputDatasCount);

				inputDatas[0].format = VK_FORMAT_R32_UINT;
				inputDatas[0].numElements = SHADER_BUFFER_SIZE/4ull;
				inputDatas[0].initializeType = subgroups::SSBOData::InitializeNonZero;

				inputDatas[1].format = VK_FORMAT_R32_UINT;
				inputDatas[1].numElements = 1ull;
				inputDatas[1].initializeType = subgroups::SSBOData::InitializeNonZero;

				if(OPTYPE_SUBGROUP_MEMORY_BARRIER_IMAGE == caseDef.opType )
				{
					inputDatas[2].format = VK_FORMAT_R32_UINT;
					inputDatas[2].numElements = SHADER_BUFFER_SIZE;
					inputDatas[2].initializeType = subgroups::SSBOData::InitializeNone;
					inputDatas[2].isImage = true;
				}

				DE_ASSERT(SHADER_BUFFER_SIZE/4ull > subgroups::getSubgroupSize(context));
				return subgroups::makeVertexFrameBufferTest(context, VK_FORMAT_R32G32B32A32_SFLOAT,
												&inputDatas[0], inputDatasCount, checkVertexPipelineStagesSubgroupBarriersNoSSBO);
			}
		}

		if (VK_SHADER_STAGE_FRAGMENT_BIT == caseDef.shaderStage)
		{
				const deUint32						inputDatasCount	= OPTYPE_SUBGROUP_MEMORY_BARRIER_IMAGE == caseDef.opType ? 3u : 2u;
				std::vector<subgroups::SSBOData>	inputDatas		(inputDatasCount);

				inputDatas[0].format = VK_FORMAT_R32_UINT;
				inputDatas[0].numElements = SHADER_BUFFER_SIZE/4ull;
				inputDatas[0].initializeType = subgroups::SSBOData::InitializeNonZero;

				inputDatas[1].format = VK_FORMAT_R32_UINT;
				inputDatas[1].numElements = 1ull;
				inputDatas[1].initializeType = subgroups::SSBOData::InitializeNonZero;

				if(OPTYPE_SUBGROUP_MEMORY_BARRIER_IMAGE == caseDef.opType )
				{
					inputDatas[2].format = VK_FORMAT_R32_UINT;
					inputDatas[2].numElements = SHADER_BUFFER_SIZE;
					inputDatas[2].initializeType = subgroups::SSBOData::InitializeNone;
					inputDatas[2].isImage = true;
				}

			return subgroups::makeFragmentFrameBufferTest(context, VK_FORMAT_R32G32B32A32_SFLOAT, &inputDatas[0], inputDatasCount, checkFragmentSubgroupBarriersNoSSBO);
		}
	}

	if (VK_SHADER_STAGE_COMPUTE_BIT == caseDef.shaderStage)
	{
		if (!subgroups::areSubgroupOperationsSupportedForStage(context, caseDef.shaderStage))
		{
				return tcu::TestStatus::fail("Shader stage " +
										subgroups::getShaderStageName(caseDef.shaderStage) +
										" is required to support subgroup operations!");
		}

		if (OPTYPE_ELECT == caseDef.opType)
		{
			return subgroups::makeComputeTest(context, VK_FORMAT_R32_UINT, DE_NULL, 0, checkComputeSubgroupElect);
		}
		else
		{
			const deUint32 inputDatasCount = 3;
			subgroups::SSBOData inputDatas[inputDatasCount];
			inputDatas[0].format = VK_FORMAT_R32_UINT;
			inputDatas[0].numElements = SHADER_BUFFER_SIZE;
			inputDatas[0].initializeType = subgroups::SSBOData::InitializeNone;

			inputDatas[1].format = VK_FORMAT_R32_UINT;
			inputDatas[1].numElements = 1;
			inputDatas[1].initializeType = subgroups::SSBOData::InitializeNonZero;

			inputDatas[2].format = VK_FORMAT_R32_UINT;
			inputDatas[2].numElements = SHADER_BUFFER_SIZE;
			inputDatas[2].initializeType = subgroups::SSBOData::InitializeNone;
			inputDatas[2].isImage = true;

			return subgroups::makeComputeTest(context, VK_FORMAT_R32_UINT, inputDatas, inputDatasCount, checkComputeSubgroupBarriers);
		}
	}
	else
	{
		if (!subgroups::isFragmentSSBOSupportedForDevice(context))
		{
			TCU_THROW(NotSupportedError, "Subgroup basic operation require that the fragment stage be able to write to SSBOs!");
		}

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

		if (OPTYPE_ELECT == caseDef.opType)
		{
			const deUint32 inputCount = 5u;
			subgroups::SSBOData inputData[inputCount];

			inputData[0].format			= VK_FORMAT_R32_UINT;
			inputData[0].numElements	= 1;
			inputData[0].initializeType	= subgroups::SSBOData::InitializeZero;
			inputData[0].binding		= 4u;
			inputData[0].stages			= VK_SHADER_STAGE_VERTEX_BIT;

			inputData[1].format			= VK_FORMAT_R32_UINT;
			inputData[1].numElements	= 1;
			inputData[1].initializeType	= subgroups::SSBOData::InitializeZero;
			inputData[1].binding		= 5u;
			inputData[1].stages			= VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;

			inputData[2].format			= VK_FORMAT_R32_UINT;
			inputData[2].numElements	= 1;
			inputData[2].initializeType	= subgroups::SSBOData::InitializeZero;
			inputData[2].binding		= 6u;
			inputData[2].stages			= VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;

			inputData[3].format			= VK_FORMAT_R32_UINT;
			inputData[3].numElements	= 1;
			inputData[3].initializeType	= subgroups::SSBOData::InitializeZero;
			inputData[3].binding		= 7u;
			inputData[3].stages			= VK_SHADER_STAGE_GEOMETRY_BIT;

			inputData[4].format			= VK_FORMAT_R32_UINT;
			inputData[4].numElements	= 1;
			inputData[4].initializeType	= subgroups::SSBOData::InitializeZero;
			inputData[4].binding		= 8u;
			inputData[4].stages			= VK_SHADER_STAGE_FRAGMENT_BIT;

			return subgroups::allStages(context, VK_FORMAT_R32_UINT, inputData, inputCount, checkVertexPipelineStagesSubgroupElect, stages);
		}
		else
		{
			const VkShaderStageFlagBits stagesBits[] =
			{
				VK_SHADER_STAGE_VERTEX_BIT,
				VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,
				VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,
				VK_SHADER_STAGE_GEOMETRY_BIT,
				VK_SHADER_STAGE_FRAGMENT_BIT,
			};

			const deUint32 inputDatasCount = DE_LENGTH_OF_ARRAY(stagesBits) * 4u;
			subgroups::SSBOData inputDatas[inputDatasCount];

			for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(stagesBits); ++ndx)
			{
				const deUint32 index = ndx*4;
				inputDatas[index].format				= VK_FORMAT_R32_UINT;
				inputDatas[index].numElements			= SHADER_BUFFER_SIZE;
				inputDatas[index].initializeType		= subgroups::SSBOData::InitializeNonZero;
				inputDatas[index].binding				= index + 4u;
				inputDatas[index].stages				= stagesBits[ndx];

				inputDatas[index + 1].format			= VK_FORMAT_R32_UINT;
				inputDatas[index + 1].numElements		= 1;
				inputDatas[index + 1].initializeType	= subgroups::SSBOData::InitializeZero;
				inputDatas[index + 1].binding			= index + 5u;
				inputDatas[index + 1].stages			= stagesBits[ndx];

				inputDatas[index + 2].format			= VK_FORMAT_R32_UINT;
				inputDatas[index + 2].numElements		= 1;
				inputDatas[index + 2].initializeType	= subgroups::SSBOData::InitializeNonZero;
				inputDatas[index + 2].binding			= index + 6u;
				inputDatas[index + 2].stages			= stagesBits[ndx];

				inputDatas[index + 3].format			= VK_FORMAT_R32_UINT;
				inputDatas[index + 3].numElements		= SHADER_BUFFER_SIZE;
				inputDatas[index + 3].initializeType	= subgroups::SSBOData::InitializeNone;
				inputDatas[index + 3].isImage			= true;
				inputDatas[index + 3].binding			= index + 7u;
				inputDatas[index + 3].stages			= stagesBits[ndx];
			}

			return subgroups::allStages(context, VK_FORMAT_R32_UINT, inputDatas, inputDatasCount, checkVertexPipelineStagesSubgroupBarriers, stages);
		}
	}
}
}

namespace vkt
{
namespace subgroups
{
tcu::TestCaseGroup* createSubgroupsBasicTests(tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(
			testCtx, "basic", "Subgroup basic category tests"));

	for (int opTypeIndex = 0; opTypeIndex < OPTYPE_LAST; ++opTypeIndex)
	{
		const std::string op = de::toLower(getOpTypeName(opTypeIndex));

		{
			const CaseDefinition caseDef = {opTypeIndex, VK_SHADER_STAGE_COMPUTE_BIT, false};
			addFunctionCaseWithPrograms(group.get(),
										op + "_" + getShaderStageName(caseDef.shaderStage), "",
										initPrograms, test, caseDef);
		}

		if (OPTYPE_SUBGROUP_MEMORY_BARRIER_SHARED == opTypeIndex)
		{
			// Shared isn't available in non compute shaders.
			continue;
		}

		{
			const CaseDefinition caseDef = {opTypeIndex, VK_SHADER_STAGE_ALL_GRAPHICS, false};
			addFunctionCaseWithPrograms(group.get(),
										op + "_graphic", "",
										initPrograms, test, caseDef);
		}

		if (OPTYPE_ELECT == opTypeIndex)
		{
			const CaseDefinition caseDef = {opTypeIndex, VK_SHADER_STAGE_VERTEX_BIT, true};
			addFunctionCaseWithPrograms(group.get(),
						op + "_" + getShaderStageName(caseDef.shaderStage)+"_framebuffer", "",
						initFrameBufferPrograms, test, caseDef);
		}
		else
		{
			const CaseDefinition caseDefFrag = {opTypeIndex, VK_SHADER_STAGE_FRAGMENT_BIT, true};
			addFunctionCaseWithPrograms(group.get(),
						op + "_" + getShaderStageName(caseDefFrag.shaderStage)+"_framebuffer", "",
						initFrameBufferPrograms, test, caseDefFrag);

			const CaseDefinition caseDefVer = {opTypeIndex, VK_SHADER_STAGE_VERTEX_BIT, true};
			addFunctionCaseWithPrograms(group.get(),
						op + "_" + getShaderStageName(caseDefVer.shaderStage)+"_framebuffer", "",
						initFrameBufferPrograms, test, caseDefVer);
		}

	}

	return group.release();
}

} // subgroups
} // vkt
