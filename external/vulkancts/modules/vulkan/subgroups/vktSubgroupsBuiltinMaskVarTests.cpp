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

static bool checkFragment(std::vector<const void*> datas,
						  deUint32 width, deUint32 height, deUint32)
{
	const deUint32* data =
		reinterpret_cast<const deUint32*>(datas[0]);
	for (deUint32 x = 0; x < width; ++x)
	{
		for (deUint32 y = 0; y < height; ++y)
		{
			deUint32 val = data[(x * height + y)];

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
	bool				noSSBO;
};
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
	std::ostringstream	vertexSrc;
	std::ostringstream	fragmentSrc;
	std::ostringstream	bdy;
	bdy << subgroupMask(caseDef);

	if (VK_SHADER_STAGE_VERTEX_BIT == caseDef.shaderStage)
	{
		vertexSrc	<< glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450)<<"\n"
					<< "#extension GL_KHR_shader_subgroup_ballot: enable\n"
					<< "layout(location = 0) out float out_color;\n"
					<< "layout(location = 0) in highp vec4 in_position;\n"
					<< "\n"
					<< "void main (void)\n"
					<< "{\n"
					<< bdy.str()
					<< "  out_color = float(tempResult);\n"
					<< "  gl_Position = in_position;\n"
					<< "}\n";
		programCollection.glslSources.add("vert") << glu::VertexSource(vertexSrc.str()) << vk::ShaderBuildOptions(vk::SPIRV_VERSION_1_3, 0u);

		fragmentSrc	<< glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450)<<"\n"
					<< "layout(location = 0) in highp float in_color;\n"
					<< "layout(location = 0) out uint out_color;\n"
					<< "void main()\n"
					<<"{\n"
					<< "	out_color = uint(in_color);\n"
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
	std::ostringstream bdy;
	bdy << subgroupMask(caseDef);

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
			<< bdy.str()
			<< "  result[offset] = tempResult;\n"
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
			 << "#extension GL_KHR_shader_subgroup_ballot: enable\n"
			 << "layout(location = 0) out uint result;\n"
			 << "void main (void)\n"
			 << "{\n"
			 << bdy.str()
			 << "  result = tempResult;\n"
			 << "}\n";

		programCollection.glslSources.add("frag")
				<< glu::FragmentSource(frag.str()) << vk::ShaderBuildOptions(vk::SPIRV_VERSION_1_3, 0u);
	}
	else if (VK_SHADER_STAGE_VERTEX_BIT == caseDef.shaderStage)
	{
		std::ostringstream src;

		src << "#version 450\n"
			<< "#extension GL_KHR_shader_subgroup_ballot: enable\n"
			<< "layout(set = 0, binding = 0, std430) buffer Output\n"
			<< "{\n"
			<< "  uint result[];\n"
			<< "};\n"
			<< "\n"
			<< "void main (void)\n"
			<< "{\n"
			<< bdy.str()
			<< "  result[gl_VertexIndex] = tempResult;\n"
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
			<< "#extension GL_KHR_shader_subgroup_ballot: enable\n"
			<< "layout(points) in;\n"
			<< "layout(points, max_vertices = 1) out;\n"
			<< "layout(set = 0, binding = 0, std430) buffer Output\n"
			<< "{\n"
			<< "  uint result[];\n"
			<< "};\n"
			<< "\n"
			<< "void main (void)\n"
			<< "{\n"
			<< bdy.str()
			<< "  result[gl_PrimitiveIDIn] = tempResult;\n"
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
			<< "#extension GL_KHR_shader_subgroup_ballot: enable\n"
			<< "layout(vertices=1) out;\n"
			<< "layout(set = 0, binding = 0, std430) buffer Output\n"
			<< "{\n"
			<< "  uint result[];\n"
			<< "};\n"
			<< "\n"
			<< "void main (void)\n"
			<< "{\n"
			<< bdy.str()
			<< "  result[gl_PrimitiveID] = tempResult;\n"
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
			<< "#extension GL_KHR_shader_subgroup_ballot: enable\n"
			<< "layout(isolines) in;\n"
			<< "layout(set = 0, binding = 0, std430) buffer Output\n"
			<< "{\n"
			<< "  uint result[];\n"
			<< "};\n"
			<< "\n"
			<< "void main (void)\n"
			<< "{\n"
			<< bdy.str()
			<< "  result[gl_PrimitiveID * 2 + uint(gl_TessCoord.x + 0.5)] = tempResult;\n"
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

	if (!subgroups::isSubgroupFeatureSupportedForDevice(context, VK_SUBGROUP_FEATURE_BALLOT_BIT))
	{
		TCU_THROW(NotSupportedError, "Device does not support subgroup ballot operations");
	}

	//Tests which don't use the SSBO
	if (caseDef.noSSBO && VK_SHADER_STAGE_VERTEX_BIT == caseDef.shaderStage)
	{
		return makeVertexFrameBufferTest(context, VK_FORMAT_R32_UINT, DE_NULL, 0, checkVertexPipelineStages);
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
		return makeComputeTest(
				   context, VK_FORMAT_R32_UINT, DE_NULL, 0, checkCompute);
	}
	else if (VK_SHADER_STAGE_FRAGMENT_BIT == caseDef.shaderStage)
	{
		return makeFragmentTest(
				   context, VK_FORMAT_R32_UINT, DE_NULL, 0, checkFragment);
	}
	else if (VK_SHADER_STAGE_VERTEX_BIT == caseDef.shaderStage)
	{
		return makeVertexTest(
				   context, VK_FORMAT_R32_UINT, DE_NULL, 0, checkVertexPipelineStages);
	}
	else if (VK_SHADER_STAGE_GEOMETRY_BIT == caseDef.shaderStage)
	{
		return makeGeometryTest(
				   context, VK_FORMAT_R32_UINT, DE_NULL, 0, checkVertexPipelineStages);
	}
	else if (VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT == caseDef.shaderStage)
	{
		return makeTessellationControlTest(
				   context, VK_FORMAT_R32_UINT, DE_NULL, 0, checkVertexPipelineStages);
	}
	else if (VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT == caseDef.shaderStage)
	{
		return makeTessellationEvaluationTest(
				   context, VK_FORMAT_R32_UINT, DE_NULL, 0, checkVertexPipelineStages);
	}
	else
	{
		TCU_THROW(InternalError, "Unhandled shader stage");
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

	return group.release();
}

} // subgroups
} // vkt
