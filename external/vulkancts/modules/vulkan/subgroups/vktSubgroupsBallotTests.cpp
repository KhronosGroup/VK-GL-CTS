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

#include "vktSubgroupsBallotTests.hpp"
#include "vktSubgroupsTestsUtils.hpp"

#include <string>
#include <vector>

using namespace tcu;
using namespace std;
using namespace vk;
using namespace vkt;

namespace
{
static bool checkVertexPipelineStages(std::vector<const void*> datas,
									  deUint32 width, deUint32)
{
	const deUint32* data =
		reinterpret_cast<const deUint32*>(datas[0]);
	for (deUint32 x = 0; x < width; ++x)
	{
		deUint32 val = data[x];

		if (0x7 != val)
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

			if (0x7 != val)
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

							if (0x7 != data[offset])
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

struct CaseDefinition
{
	VkShaderStageFlags	shaderStage;
	bool				noSSBO;
};

void initFrameBufferPrograms(SourceCollections& programCollection, CaseDefinition caseDef)
{
	std::ostringstream	fragmentSrc;

	if (VK_SHADER_STAGE_VERTEX_BIT == caseDef.shaderStage)
	{
		std::ostringstream	vertexSrc;

		vertexSrc << "#version 450\n"
			<< "#extension GL_KHR_shader_subgroup_ballot: enable\n"
			<< "layout(location = 0) in highp vec4 in_position;\n"
			<< "layout(location = 0) out float out_color;\n"
			<< "layout(set = 0, binding = 0) uniform Buffer1\n"
			<< "{\n"
			<< "  uint data[" << subgroups::maxSupportedSubgroupSize() << "];\n"
			<< "};\n"
			<< "\n"
			<< "void main (void)\n"
			<< "{\n"
			<< "  uint tempResult = 0;\n"
			<< "  tempResult |= !bool(uvec4(0) == subgroupBallot(true)) ? 0x1 : 0;\n"
			<< "  bool bData = data[gl_SubgroupInvocationID] != 0;\n"
			<< "  tempResult |= !bool(uvec4(0) == subgroupBallot(bData)) ? 0x2 : 0;\n"
			<< "  tempResult |= uvec4(0) == subgroupBallot(false) ? 0x4 : 0;\n"
			<< "  out_color = float(tempResult);\n"
			<< "  gl_Position = in_position;\n"
			<< "}\n";

		programCollection.glslSources.add("vert") << glu::VertexSource(vertexSrc.str()) << vk::ShaderBuildOptions(vk::SPIRV_VERSION_1_3, 0u);

		fragmentSrc << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450)<<"\n"
			<< "layout(location = 0) in float in_color;\n"
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
	if (VK_SHADER_STAGE_COMPUTE_BIT == caseDef.shaderStage)
	{
		std::ostringstream src;

		src << "#version 450\n"
			<< "#extension GL_KHR_shader_subgroup_ballot: enable\n"
			<< "layout (local_size_x_id = 0, local_size_y_id = 1, "
			"local_size_z_id = 2) in;\n"
			<< "layout(set = 0, binding = 0, std430) buffer Buffer1\n"
			<< "{\n"
			<< "  uint result[];\n"
			<< "};\n"
			<< "layout(set = 0, binding = 1, std430) buffer Buffer2\n"
			<< "{\n"
			<< "  uint data[];\n"
			<< "};\n"
			<< "\n"
			<< subgroups::getSharedMemoryBallotHelper()
			<< "void main (void)\n"
			<< "{\n"
			<< "  uvec3 globalSize = gl_NumWorkGroups * gl_WorkGroupSize;\n"
			<< "  highp uint offset = globalSize.x * ((globalSize.y * "
			"gl_GlobalInvocationID.z) + gl_GlobalInvocationID.y) + "
			"gl_GlobalInvocationID.x;\n"
			<< "  uint tempResult = 0;\n"
			<< "  tempResult |= sharedMemoryBallot(true) == subgroupBallot(true) ? 0x1 : 0;\n"
			<< "  bool bData = data[gl_SubgroupInvocationID] != 0;\n"
			<< "  tempResult |= sharedMemoryBallot(bData) == subgroupBallot(bData) ? 0x2 : 0;\n"
			<< "  tempResult |= uvec4(0) == subgroupBallot(false) ? 0x4 : 0;\n"
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
			 << "layout(set = 0, binding = 0, std430) readonly buffer Buffer1\n"
			 << "{\n"
			 << "  uint data[];\n"
			 << "};\n"
			 << "void main (void)\n"
			 << "{\n"
			 << "  uint tempResult = 0;\n"
			 << "  tempResult |= !bool(uvec4(0) == subgroupBallot(true)) ? 0x1 : 0;\n"
			 << "  bool bData = data[gl_SubgroupInvocationID] != 0;\n"
			 << "  tempResult |= !bool(uvec4(0) == subgroupBallot(bData)) ? 0x2 : 0;\n"
			 << "  tempResult |= uvec4(0) == subgroupBallot(false) ? 0x4 : 0;\n"
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
			<< "layout(set = 0, binding = 0, std430) buffer Buffer1\n"
			<< "{\n"
			<< "  uint result[];\n"
			<< "};\n"
			<< "layout(set = 0, binding = 1, std430) buffer Buffer2\n"
			<< "{\n"
			<< "  uint data[];\n"
			<< "};\n"
			<< "\n"
			<< "void main (void)\n"
			<< "{\n"
			<< "  uint tempResult = 0;\n"
			<< "  tempResult |= !bool(uvec4(0) == subgroupBallot(true)) ? 0x1 : 0;\n"
			<< "  bool bData = data[gl_SubgroupInvocationID] != 0;\n"
			<< "  tempResult |= !bool(uvec4(0) == subgroupBallot(bData)) ? 0x2 : 0;\n"
			<< "  tempResult |= uvec4(0) == subgroupBallot(false) ? 0x4 : 0;\n"
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
			<< "layout(set = 0, binding = 0, std430) buffer Buffer1\n"
			<< "{\n"
			<< "  uint result[];\n"
			<< "};\n"
			<< "layout(set = 0, binding = 1, std430) buffer Buffer2\n"
			<< "{\n"
			<< "  uint data[];\n"
			<< "};\n"
			<< "\n"
			<< "void main (void)\n"
			<< "{\n"
			<< "  uint tempResult = 0;\n"
			<< "  tempResult |= !bool(uvec4(0) == subgroupBallot(true)) ? 0x1 : 0;\n"
			<< "  bool bData = data[gl_SubgroupInvocationID] != 0;\n"
			<< "  tempResult |= !bool(uvec4(0) == subgroupBallot(bData)) ? 0x2 : 0;\n"
			<< "  tempResult |= uvec4(0) == subgroupBallot(false) ? 0x4 : 0;\n"
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
			<< "layout(set = 0, binding = 0, std430) buffer Buffer1\n"
			<< "{\n"
			<< "  uint result[];\n"
			<< "};\n"
			<< "layout(set = 0, binding = 1, std430) buffer Buffer2\n"
			<< "{\n"
			<< "  uint data[];\n"
			<< "};\n"
			<< "\n"
			<< "void main (void)\n"
			<< "{\n"
			<< "  uint tempResult = 0;\n"
			<< "  tempResult |= !bool(uvec4(0) == subgroupBallot(true)) ? 0x1 : 0;\n"
			<< "  bool bData = data[gl_SubgroupInvocationID] != 0;\n"
			<< "  tempResult |= !bool(uvec4(0) == subgroupBallot(bData)) ? 0x2 : 0;\n"
			<< "  tempResult |= uvec4(0) == subgroupBallot(false) ? 0x4 : 0;\n"
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
			<< "layout(set = 0, binding = 0, std430) buffer Buffer1\n"
			<< "{\n"
			<< "  uint result[];\n"
			<< "};\n"
			<< "layout(set = 0, binding = 1, std430) buffer Buffer2\n"
			<< "{\n"
			<< "  uint data[];\n"
			<< "};\n"
			<< "\n"
			<< "void main (void)\n"
			<< "{\n"
			<< "  uint tempResult = 0;\n"
			<< "  tempResult |= !bool(uvec4(0) == subgroupBallot(true)) ? 0x1 : 0;\n"
			<< "  bool bData = data[gl_SubgroupInvocationID] != 0;\n"
			<< "  tempResult |= !bool(uvec4(0) == subgroupBallot(bData)) ? 0x2 : 0;\n"
			<< "  tempResult |= uvec4(0) == subgroupBallot(false) ? 0x4 : 0;\n"
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

	if (!subgroups::isSubgroupFeatureSupportedForDevice(context, VK_SUBGROUP_FEATURE_BALLOT_BIT))
	{
		TCU_THROW(NotSupportedError, "Device does not support subgroup ballot operations");
	}

	//Tests which don't use the SSBO
	if (caseDef.noSSBO && VK_SHADER_STAGE_VERTEX_BIT == caseDef.shaderStage)
	{
		subgroups::SSBOData inputData[1];
		inputData[0].format = VK_FORMAT_R32_UINT;
		inputData[0].numElements = subgroups::maxSupportedSubgroupSize();
		inputData[0].initializeType = subgroups::SSBOData::InitializeNonZero;

		return subgroups::makeVertexFrameBufferTest(context, VK_FORMAT_R32_UINT, inputData, 1, checkVertexPipelineStages);
	}

	if ((VK_SHADER_STAGE_FRAGMENT_BIT != caseDef.shaderStage) &&
			(VK_SHADER_STAGE_COMPUTE_BIT != caseDef.shaderStage))
	{
		if (!subgroups::isVertexSSBOSupportedForDevice(context))
		{
			TCU_THROW(NotSupportedError, "Device does not support vertex stage SSBO writes");
		}
	}

	if (VK_SHADER_STAGE_FRAGMENT_BIT == caseDef.shaderStage)
	{
		subgroups::SSBOData inputData[1];
		inputData[0].format = VK_FORMAT_R32_UINT;
		inputData[0].numElements = subgroups::maxSupportedSubgroupSize();
		inputData[0].initializeType = subgroups::SSBOData::InitializeNonZero;

		return subgroups::makeFragmentTest(context, VK_FORMAT_R32_UINT,
										   inputData, 1, checkFragment);
	}
	else if (VK_SHADER_STAGE_COMPUTE_BIT == caseDef.shaderStage)
	{
		subgroups::SSBOData inputData[1];
		inputData[0].format = VK_FORMAT_R32_UINT;
		inputData[0].numElements = subgroups::maxSupportedSubgroupSize();
		inputData[0].initializeType = subgroups::SSBOData::InitializeNonZero;

		return subgroups::makeComputeTest(context, VK_FORMAT_R32_UINT,
										  inputData, 1, checkCompute);
	}
	else if (VK_SHADER_STAGE_VERTEX_BIT == caseDef.shaderStage)
	{
		subgroups::SSBOData inputData[1];
		inputData[0].format = VK_FORMAT_R32_UINT;
		inputData[0].numElements = subgroups::maxSupportedSubgroupSize();
		inputData[0].initializeType = subgroups::SSBOData::InitializeNonZero;

		return subgroups::makeVertexTest(context, VK_FORMAT_R32_UINT,
										 inputData, 1, checkVertexPipelineStages);
	}
	else if (VK_SHADER_STAGE_GEOMETRY_BIT == caseDef.shaderStage)
	{
		subgroups::SSBOData inputData[1];
		inputData[0].format = VK_FORMAT_R32_UINT;
		inputData[0].numElements = subgroups::maxSupportedSubgroupSize();
		inputData[0].initializeType = subgroups::SSBOData::InitializeNonZero;

		return subgroups::makeGeometryTest(context, VK_FORMAT_R32_UINT,
										   inputData, 1, checkVertexPipelineStages);
	}
	else if (VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT == caseDef.shaderStage)
	{
		subgroups::SSBOData inputData[1];
		inputData[0].format = VK_FORMAT_R32_UINT;
		inputData[0].numElements = subgroups::maxSupportedSubgroupSize();
		inputData[0].initializeType = subgroups::SSBOData::InitializeNonZero;

		return subgroups::makeTessellationControlTest(context, VK_FORMAT_R32_UINT,
				inputData, 1, checkVertexPipelineStages);
	}
	else if (VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT == caseDef.shaderStage)
	{
		subgroups::SSBOData inputData[1];
		inputData[0].format = VK_FORMAT_R32_UINT;
		inputData[0].numElements = subgroups::maxSupportedSubgroupSize();
		inputData[0].initializeType = subgroups::SSBOData::InitializeNonZero;

		return subgroups::makeTessellationEvaluationTest(context, VK_FORMAT_R32_UINT,
				inputData, 1, checkVertexPipelineStages);
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
tcu::TestCaseGroup* createSubgroupsBallotTests(tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(
			testCtx, "ballot", "Subgroup ballot category tests"));

	const VkShaderStageFlags stages[] =
	{
		VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,
		VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,
		VK_SHADER_STAGE_GEOMETRY_BIT,
		VK_SHADER_STAGE_VERTEX_BIT,
		VK_SHADER_STAGE_FRAGMENT_BIT,
		VK_SHADER_STAGE_COMPUTE_BIT
	};

	for (int stageIndex = 0; stageIndex < DE_LENGTH_OF_ARRAY(stages); ++stageIndex)
	{
		const VkShaderStageFlags stage = stages[stageIndex];

		CaseDefinition caseDef = {stage, false};

		std::ostringstream name;

		name << getShaderStageName(stage);

		addFunctionCaseWithPrograms(group.get(), name.str(),
									"", initPrograms, test, caseDef);

		if (VK_SHADER_STAGE_VERTEX_BIT == stage )
		{
			caseDef.noSSBO = true;
			addFunctionCaseWithPrograms(group.get(), name.str()+"_framebuffer", "",
						initFrameBufferPrograms, test, caseDef);
		}
	}

	return group.release();
}

} // subgroups
} // vkt
