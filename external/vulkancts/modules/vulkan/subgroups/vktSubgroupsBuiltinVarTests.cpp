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

bool checkVertexPipelineStagesSubgroupSize(std::vector<const void*> datas,
		deUint32 width, deUint32 subgroupSize)
{
	const deUint32* data =
		reinterpret_cast<const deUint32*>(datas[0]);
	for (deUint32 x = 0; x < width; ++x)
	{
		deUint32 val = data[x * 4];

		if (subgroupSize != val)
		{
			return false;
		}
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
		{
			return false;
		}

		subgroupInvocationHits[subgroupInvocationID]++;
	}

	const deUint32 totalSize = width;

	deUint32 totalInvocationsRun = 0;
	for (deUint32 i = 0; i < subgroupSize; ++i)
	{
		totalInvocationsRun += subgroupInvocationHits[i];
	}

	if (totalInvocationsRun != totalSize)
	{
		return false;
	}

	return true;
}

static bool checkFragmentSubgroupSize(std::vector<const void*> datas,
									  deUint32 width, deUint32 height, deUint32 subgroupSize)
{
	const deUint32* data =
		reinterpret_cast<const deUint32*>(datas[0]);
	for (deUint32 x = 0; x < width; ++x)
	{
		for (deUint32 y = 0; y < height; ++y)
		{
			deUint32 val = data[(x * height + y) * 4];

			if (subgroupSize != val)
			{
				return false;
			}
		}
	}

	return true;
}

static bool checkFragmentSubgroupInvocationID(
	std::vector<const void*> datas, deUint32 width, deUint32 height,
	deUint32 subgroupSize)
{
	const deUint32* data =
		reinterpret_cast<const deUint32*>(datas[0]);
	vector<deUint32> subgroupInvocationHits(subgroupSize, 0);

	for (deUint32 x = 0; x < width; ++x)
	{
		for (deUint32 y = 0; y < height; ++y)
		{
			deUint32 subgroupInvocationID = data[((x * height + y) * 4) + 1];

			if (subgroupInvocationID >= subgroupSize)
			{
				return false;
			}

			subgroupInvocationHits[subgroupInvocationID]++;
		}
	}

	const deUint32 totalSize = width * height;

	deUint32 totalInvocationsRun = 0;
	for (deUint32 i = 0; i < subgroupSize; ++i)
	{
		totalInvocationsRun += subgroupInvocationHits[i];
	}

	if (totalInvocationsRun != totalSize)
	{
		return false;
	}

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
							{
								return false;
							}

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
				{
					return false;
				}
			}
		}
	}

	return true;
}

static bool checkComputeNumSubgroups(std::vector<const void*> datas,
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

static bool checkComputeSubgroupID(std::vector<const void*> datas,
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

							deUint32 numSubgroups = data[(offset * 4) + 2];
							deUint32 subgroupID = data[(offset * 4) + 3];

							if (subgroupID >= numSubgroups)
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
	std::string varName;
	VkShaderStageFlags shaderStage;
	bool noSSBO;
};
}

void initFrameBufferPrograms(SourceCollections& programCollection, CaseDefinition caseDef)
{
	std::ostringstream src;
	if (VK_SHADER_STAGE_VERTEX_BIT == caseDef.shaderStage)
	{
		src << "#version 450\n"
			<< "#extension GL_KHR_shader_subgroup_basic: enable\n"
			<< "layout(location = 0) out vec4 out_color;\n"
			<< "layout(location = 0) in highp vec4 in_position;\n"
			<< "\n"
			<< "void main (void)\n"
			<< "{\n"
			<< "  out_color = uvec4(gl_SubgroupSize, gl_SubgroupInvocationID, 1.0f, 1.0f);\n"
			<< "  gl_Position = in_position;\n"
			<< "  gl_PointSize = 1.0f;\n"
			<< "}\n";

		programCollection.glslSources.add("vert") << glu::VertexSource(src.str()) << vk::ShaderBuildOptions(vk::SPIRV_VERSION_1_3, 0u);

		std::ostringstream source;
		source	<< glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450)<<"\n"
				<< "layout(location = 0) in vec4 in_color;\n"
				<< "layout(location = 0) out uvec4 out_color;\n"
				<< "void main()\n"
				<<"{\n"
				<< "	out_color = uvec4(in_color);\n"
				<< "}\n";
		programCollection.glslSources.add("fragment") << glu::FragmentSource(source.str()) << vk::ShaderBuildOptions(vk::SPIRV_VERSION_1_3, 0u);
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
				<< glu::ComputeSource(src.str()) << vk::ShaderBuildOptions(vk::SPIRV_VERSION_1_3, 0u);
	}
	else if (VK_SHADER_STAGE_FRAGMENT_BIT == caseDef.shaderStage)
	{
		programCollection.glslSources.add("vert")
				<< glu::VertexSource(subgroups::getVertShaderForStage(caseDef.shaderStage)) << vk::ShaderBuildOptions(vk::SPIRV_VERSION_1_3, 0u);

		std::ostringstream frag;

		frag << "#version 450\n"
			 << "#extension GL_KHR_shader_subgroup_basic: enable\n"
			 << "layout(location = 0) out uvec4 data;\n"
			 << "void main (void)\n"
			 << "{\n"
			 << "  data = uvec4(gl_SubgroupSize, gl_SubgroupInvocationID, 0, 0);\n"
			 << "}\n";

		programCollection.glslSources.add("frag")
				<< glu::FragmentSource(frag.str()) << vk::ShaderBuildOptions(vk::SPIRV_VERSION_1_3, 0u);
	}
	else if (VK_SHADER_STAGE_VERTEX_BIT == caseDef.shaderStage)
	{
		std::ostringstream src;

		src << "#version 450\n"
			<< "#extension GL_KHR_shader_subgroup_basic: enable\n"
			<< "layout(set = 0, binding = 0, std430) buffer Output\n"
			<< "{\n"
			<< "  uvec4 result[];\n"
			<< "};\n"
			<< "\n"
			<< "void main (void)\n"
			<< "{\n"
			<< "  result[gl_VertexIndex] = uvec4(gl_SubgroupSize, gl_SubgroupInvocationID, 0, 0);\n"
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

		src << "#version 450\n"
			<< "#extension GL_KHR_shader_subgroup_basic: enable\n"
			<< "layout(points) in;\n"
			<< "layout(points, max_vertices = 1) out;\n"
			<< "layout(set = 0, binding = 0, std430) buffer Output\n"
			<< "{\n"
			<< "  uvec4 result[];\n"
			<< "};\n"
			<< "\n"
			<< "void main (void)\n"
			<< "{\n"
			<< "  result[gl_PrimitiveIDIn] = uvec4(gl_SubgroupSize, gl_SubgroupInvocationID, 0, 0);\n"
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

		src << "#version 450\n"
			<< "#extension GL_KHR_shader_subgroup_basic: enable\n"
			<< "layout(vertices=1) out;\n"
			<< "layout(set = 0, binding = 0, std430) buffer Output\n"
			<< "{\n"
			<< "  uvec4 result[];\n"
			<< "};\n"
			<< "\n"
			<< "void main (void)\n"
			<< "{\n"
			<< "  result[gl_PrimitiveID] = uvec4(gl_SubgroupSize, gl_SubgroupInvocationID, 0, 0);\n"
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

		src << "#version 450\n"
			<< "#extension GL_KHR_shader_subgroup_basic: enable\n"
			<< "layout(isolines) in;\n"
			<< "layout(set = 0, binding = 0, std430) buffer Output\n"
			<< "{\n"
			<< "  uvec4 result[];\n"
			<< "};\n"
			<< "\n"
			<< "void main (void)\n"
			<< "{\n"
			<< "  result[gl_PrimitiveID * 2 + uint(gl_TessCoord.x + 0.5)] = uvec4(gl_SubgroupSize, gl_SubgroupInvocationID, 0, 0);\n"
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

	//Tests which don't use the SSBO
	if (caseDef.noSSBO && VK_SHADER_STAGE_VERTEX_BIT == caseDef.shaderStage)
	{
		if ("gl_SubgroupSize" == caseDef.varName)
		{
			return makeVertexFrameBufferTest(
					   context, VK_FORMAT_R32G32B32A32_UINT, DE_NULL, 0, checkVertexPipelineStagesSubgroupSize);
		}
		else if ("gl_SubgroupInvocationID" == caseDef.varName)
		{
			return makeVertexFrameBufferTest(
					   context, VK_FORMAT_R32G32B32A32_UINT, DE_NULL, 0, checkVertexPipelineStagesSubgroupInvocationID);
		}
	}

	if ((VK_SHADER_STAGE_FRAGMENT_BIT != caseDef.shaderStage) &&
			(VK_SHADER_STAGE_COMPUTE_BIT != caseDef.shaderStage))
	{
		if (!subgroups::isVertexSSBOSupportedForDevice(context))
		{
			TCU_THROW(NotSupportedError, "Device does not support vertex stage SSBO writes");
		}
	}

	if (VK_SHADER_STAGE_COMPUTE_BIT == caseDef.shaderStage)
	{
		if ("gl_SubgroupSize" == caseDef.varName)
		{
			return makeComputeTest(
					   context, VK_FORMAT_R32G32B32A32_UINT, DE_NULL, 0, checkComputeSubgroupSize);
		}
		else if ("gl_SubgroupInvocationID" == caseDef.varName)
		{
			return makeComputeTest(
					   context, VK_FORMAT_R32G32B32A32_UINT, DE_NULL, 0, checkComputeSubgroupInvocationID);
		}
		else if ("gl_NumSubgroups" == caseDef.varName)
		{
			return makeComputeTest(
					   context, VK_FORMAT_R32G32B32A32_UINT, DE_NULL, 0, checkComputeNumSubgroups);
		}
		else if ("gl_SubgroupID" == caseDef.varName)
		{
			return makeComputeTest(
					   context, VK_FORMAT_R32G32B32A32_UINT, DE_NULL, 0, checkComputeSubgroupID);
		}
		else
		{
			return tcu::TestStatus::fail(
					   caseDef.varName + " failed (unhandled error checking case " +
					   caseDef.varName + ")!");
		}
	}
	else if (VK_SHADER_STAGE_FRAGMENT_BIT == caseDef.shaderStage)
	{
		if ("gl_SubgroupSize" == caseDef.varName)
		{
			return makeFragmentTest(
					   context, VK_FORMAT_R32G32B32A32_UINT, DE_NULL, 0, checkFragmentSubgroupSize);
		}
		else if ("gl_SubgroupInvocationID" == caseDef.varName)
		{
			return makeFragmentTest(
					   context, VK_FORMAT_R32G32B32A32_UINT, DE_NULL, 0, checkFragmentSubgroupInvocationID);
		}
		else
		{
			return tcu::TestStatus::fail(
					   caseDef.varName + " failed (unhandled error checking case " +
					   caseDef.varName + ")!");
		}
	}
	else if (VK_SHADER_STAGE_VERTEX_BIT == caseDef.shaderStage)
	{
		if ("gl_SubgroupSize" == caseDef.varName)
		{
			return makeVertexTest(
					   context, VK_FORMAT_R32G32B32A32_UINT, DE_NULL, 0, checkVertexPipelineStagesSubgroupSize);
		}
		else if ("gl_SubgroupInvocationID" == caseDef.varName)
		{
			return makeVertexTest(
					   context, VK_FORMAT_R32G32B32A32_UINT, DE_NULL, 0, checkVertexPipelineStagesSubgroupInvocationID);
		}
		else
		{
			return tcu::TestStatus::fail(
					   caseDef.varName + " failed (unhandled error checking case " +
					   caseDef.varName + ")!");
		}
	}
	else if (VK_SHADER_STAGE_GEOMETRY_BIT == caseDef.shaderStage)
	{
		if ("gl_SubgroupSize" == caseDef.varName)
		{
			return makeGeometryTest(
					   context, VK_FORMAT_R32G32B32A32_UINT, DE_NULL, 0, checkVertexPipelineStagesSubgroupSize);
		}
		else if ("gl_SubgroupInvocationID" == caseDef.varName)
		{
			return makeGeometryTest(
					   context, VK_FORMAT_R32G32B32A32_UINT, DE_NULL, 0, checkVertexPipelineStagesSubgroupInvocationID);
		}
		else
		{
			return tcu::TestStatus::fail(
					   caseDef.varName + " failed (unhandled error checking case " +
					   caseDef.varName + ")!");
		}
	}
	else if (VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT == caseDef.shaderStage)
	{
		if ("gl_SubgroupSize" == caseDef.varName)
		{
			return makeTessellationControlTest(
					   context, VK_FORMAT_R32G32B32A32_UINT, DE_NULL, 0, checkVertexPipelineStagesSubgroupSize);
		}
		else if ("gl_SubgroupInvocationID" == caseDef.varName)
		{
			return makeTessellationControlTest(
					   context, VK_FORMAT_R32G32B32A32_UINT, DE_NULL, 0, checkVertexPipelineStagesSubgroupInvocationID);
		}
		else
		{
			return tcu::TestStatus::fail(
					   caseDef.varName + " failed (unhandled error checking case " +
					   caseDef.varName + ")!");
		}
	}
	else if (VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT == caseDef.shaderStage)
	{
		if ("gl_SubgroupSize" == caseDef.varName)
		{
			return makeTessellationEvaluationTest(
					   context, VK_FORMAT_R32G32B32A32_UINT, DE_NULL, 0, checkVertexPipelineStagesSubgroupSize);
		}
		else if ("gl_SubgroupInvocationID" == caseDef.varName)
		{
			return makeTessellationEvaluationTest(
					   context, VK_FORMAT_R32G32B32A32_UINT, DE_NULL, 0, checkVertexPipelineStagesSubgroupInvocationID);
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

tcu::TestCaseGroup* createSubgroupsBuiltinVarTests(tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(
			testCtx, "builtin_var", "Subgroup builtin variable tests"));

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
		VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,
		VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,
		VK_SHADER_STAGE_GEOMETRY_BIT,
		VK_SHADER_STAGE_VERTEX_BIT,
		VK_SHADER_STAGE_FRAGMENT_BIT,
		VK_SHADER_STAGE_COMPUTE_BIT,
	};

	for (int stageIndex = 0; stageIndex < DE_LENGTH_OF_ARRAY(stages); ++stageIndex)
	{
		const VkShaderStageFlags stage = stages[stageIndex];

		for (int a = 0; a < DE_LENGTH_OF_ARRAY(all_stages_vars); ++a)
		{
			const std::string var = all_stages_vars[a];

			CaseDefinition caseDef = {"gl_" + var, stage, false};

			addFunctionCaseWithPrograms(group.get(),
										de::toLower(var) + "_" +
										getShaderStageName(stage), "",
										initPrograms, test, caseDef);

			if (VK_SHADER_STAGE_VERTEX_BIT == stage)
			{
				caseDef.noSSBO = true;
				addFunctionCaseWithPrograms(group.get(),
							de::toLower(var) + "_" +
							getShaderStageName(stage)+"_framebuffer", "",
							initFrameBufferPrograms, test, caseDef);
			}
		}
	}

	for (int a = 0; a < DE_LENGTH_OF_ARRAY(compute_only_vars); ++a)
	{
		const VkShaderStageFlags stage = VK_SHADER_STAGE_COMPUTE_BIT;
		const std::string var = compute_only_vars[a];

		CaseDefinition caseDef = {"gl_" + var, stage, false};

		addFunctionCaseWithPrograms(group.get(), de::toLower(var) +
									"_" + getShaderStageName(stage), "",
									initPrograms, test, caseDef);
	}

	return group.release();
}

} // subgroups
} // vkt
