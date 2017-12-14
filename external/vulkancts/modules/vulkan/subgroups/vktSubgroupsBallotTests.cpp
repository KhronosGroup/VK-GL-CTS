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
			<< "  gl_PointSize = 1.0f;\n"
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
	else
	{
		const string vertex =
			"#version 450\n"
			"#extension GL_KHR_shader_subgroup_ballot: enable\n"
			"layout(set = 0, binding = 0, std430) buffer Buffer1\n"
			"{\n"
			"  uint result[];\n"
			"};\n"
			"layout(set = 0, binding = 4, std430) readonly buffer Buffer2\n"
			"{\n"
			"  uint data[];\n"
			"};\n"
			"\n"
			"void main (void)\n"
			"{\n"
			"  uint tempResult = 0;\n"
			"  tempResult |= !bool(uvec4(0) == subgroupBallot(true)) ? 0x1 : 0;\n"
			"  bool bData = data[gl_SubgroupInvocationID] != 0;\n"
			"  tempResult |= !bool(uvec4(0) == subgroupBallot(bData)) ? 0x2 : 0;\n"
			"  tempResult |= uvec4(0) == subgroupBallot(false) ? 0x4 : 0;\n"
			"  result[gl_VertexIndex] = tempResult;\n"
			"  float pixelSize = 2.0f/1024.0f;\n"
			"  float pixelPosition = pixelSize/2.0f - 1.0f;\n"
			"  gl_Position = vec4(float(gl_VertexIndex) * pixelSize + pixelPosition, 0.0f, 0.0f, 1.0f);\n"
			"}\n";

		const string tesc =
			"#version 450\n"
			"#extension GL_KHR_shader_subgroup_ballot: enable\n"
			"layout(vertices=1) out;\n"
			"layout(set = 0, binding = 1, std430) buffer Buffer1\n"
			"{\n"
			"  uint result[];\n"
			"};\n"
			"layout(set = 0, binding = 4, std430) readonly buffer Buffer2\n"
			"{\n"
			"  uint data[];\n"
			"};\n"
			"\n"
			"void main (void)\n"
			"{\n"
			"  uint tempResult = 0;\n"
			"  tempResult |= !bool(uvec4(0) == subgroupBallot(true)) ? 0x1 : 0;\n"
			"  bool bData = data[gl_SubgroupInvocationID] != 0;\n"
			"  tempResult |= !bool(uvec4(0) == subgroupBallot(bData)) ? 0x2 : 0;\n"
			"  tempResult |= uvec4(0) == subgroupBallot(false) ? 0x4 : 0;\n"
			"  result[gl_PrimitiveID] = tempResult;\n"
			"  if (gl_InvocationID == 0)\n"
			"  {\n"
			"    gl_TessLevelOuter[0] = 1.0f;\n"
			"    gl_TessLevelOuter[1] = 1.0f;\n"
			"  }\n"
			"  gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;\n"
			"}\n";

		const string tese =
			"#version 450\n"
			"#extension GL_KHR_shader_subgroup_ballot: enable\n"
			"layout(isolines) in;\n"
			"layout(set = 0, binding = 2, std430) buffer Buffer1\n"
			"{\n"
			"  uint result[];\n"
			"};\n"
			"layout(set = 0, binding = 4, std430) readonly buffer Buffer2\n"
			"{\n"
			"  uint data[];\n"
			"};\n"
			"\n"
			"void main (void)\n"
			"{\n"
			"  uint tempResult = 0;\n"
			"  tempResult |= !bool(uvec4(0) == subgroupBallot(true)) ? 0x1 : 0;\n"
			"  bool bData = data[gl_SubgroupInvocationID] != 0;\n"
			"  tempResult |= !bool(uvec4(0) == subgroupBallot(bData)) ? 0x2 : 0;\n"
			"  tempResult |= uvec4(0) == subgroupBallot(false) ? 0x4 : 0;\n"
			"  result[gl_PrimitiveID * 2 + uint(gl_TessCoord.x + 0.5)] = tempResult;\n"
			"  gl_Position = mix(gl_in[0].gl_Position, gl_in[1].gl_Position, gl_TessCoord.x);\n"
			"}\n";

		const string geometry =
			"#version 450\n"
			"#extension GL_KHR_shader_subgroup_ballot: enable\n"
			"layout(points) in;\n"
			"layout(points, max_vertices = 1) out;\n"
			"layout(set = 0, binding = 3, std430) buffer Buffer1\n"
			"{\n"
			"  uint result[];\n"
			"};\n"
			"layout(set = 0, binding = 4, std430) readonly buffer Buffer2\n"
			"{\n"
			"  uint data[];\n"
			"};\n"
			"\n"
			"void main (void)\n"
			"{\n"
			"  uint tempResult = 0;\n"
			"  tempResult |= !bool(uvec4(0) == subgroupBallot(true)) ? 0x1 : 0;\n"
			"  bool bData = data[gl_SubgroupInvocationID] != 0;\n"
			"  tempResult |= !bool(uvec4(0) == subgroupBallot(bData)) ? 0x2 : 0;\n"
			"  tempResult |= uvec4(0) == subgroupBallot(false) ? 0x4 : 0;\n"
			"  result[gl_PrimitiveIDIn] = tempResult;\n"
			"  gl_Position = gl_in[0].gl_Position;\n"
			"  EmitVertex();\n"
			"  EndPrimitive();\n"
			"}\n";

		const string fragment =
			"#version 450\n"
			"#extension GL_KHR_shader_subgroup_ballot: enable\n"
			"layout(location = 0) out uint result;\n"
			"layout(set = 0, binding = 4, std430) readonly buffer Buffer1\n"
			"{\n"
			"  uint data[];\n"
			"};\n"
			"void main (void)\n"
			"{\n"
			"  uint tempResult = 0;\n"
			"  tempResult |= !bool(uvec4(0) == subgroupBallot(true)) ? 0x1 : 0;\n"
			"  bool bData = data[gl_SubgroupInvocationID] != 0;\n"
			"  tempResult |= !bool(uvec4(0) == subgroupBallot(bData)) ? 0x2 : 0;\n"
			"  tempResult |= uvec4(0) == subgroupBallot(false) ? 0x4 : 0;\n"
			"  result = tempResult;\n"
			"}\n";

		subgroups::addNoSubgroupShader(programCollection);

		programCollection.glslSources.add("vert")
				<< glu::VertexSource(vertex) << vk::ShaderBuildOptions(vk::SPIRV_VERSION_1_3, 0u);
		programCollection.glslSources.add("tesc")
				<< glu::TessellationControlSource(tesc) << vk::ShaderBuildOptions(vk::SPIRV_VERSION_1_3, 0u);
		programCollection.glslSources.add("tese")
				<< glu::TessellationEvaluationSource(tese) << vk::ShaderBuildOptions(vk::SPIRV_VERSION_1_3, 0u);
		programCollection.glslSources.add("geometry")
				<< glu::GeometrySource(geometry) << vk::ShaderBuildOptions(vk::SPIRV_VERSION_1_3, 0u);
		programCollection.glslSources.add("fragment")
				<< glu::FragmentSource(fragment)<< vk::ShaderBuildOptions(vk::SPIRV_VERSION_1_3, 0u);
	}
}

tcu::TestStatus test(Context& context, const CaseDefinition caseDef)
{
	if (!subgroups::isSubgroupSupported(context))
		TCU_THROW(NotSupportedError, "Subgroup operations are not supported");

	if (!subgroups::isSubgroupFeatureSupportedForDevice(context, VK_SUBGROUP_FEATURE_BALLOT_BIT))
	{
		TCU_THROW(NotSupportedError, "Device does not support subgroup ballot operations");
	}

	//Tests which don't use the SSBO
	if (caseDef.noSSBO && VK_SHADER_STAGE_VERTEX_BIT == caseDef.shaderStage)
	{
		if (!subgroups::areSubgroupOperationsSupportedForStage(context, caseDef.shaderStage))
		{
			TCU_THROW(NotSupportedError, "Device does not support subgroup operations for this stage");
		}
		subgroups::SSBOData inputData[1];
		inputData[0].format = VK_FORMAT_R32_UINT;
		inputData[0].numElements = subgroups::maxSupportedSubgroupSize();
		inputData[0].initializeType = subgroups::SSBOData::InitializeNonZero;

		return subgroups::makeVertexFrameBufferTest(context, VK_FORMAT_R32_UINT, inputData, 1, checkVertexPipelineStages);
	}

	if (VK_SHADER_STAGE_COMPUTE_BIT == caseDef.shaderStage)
	{
		if (!subgroups::areSubgroupOperationsSupportedForStage(context, caseDef.shaderStage))
		{
				return tcu::TestStatus::fail(
						   "Shader stage " +
						   subgroups::getShaderStageName(caseDef.shaderStage) +
						   " is required to support subgroup operations!");
		}
		subgroups::SSBOData inputData[1];
		inputData[0].format = VK_FORMAT_R32_UINT;
		inputData[0].numElements = subgroups::maxSupportedSubgroupSize();
		inputData[0].initializeType = subgroups::SSBOData::InitializeNonZero;

		return subgroups::makeComputeTest(context, VK_FORMAT_R32_UINT, inputData, 1, checkCompute);
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

		subgroups::SSBOData inputData;
		inputData.format			= VK_FORMAT_R32_UINT;
		inputData.numElements		= subgroups::maxSupportedSubgroupSize();
		inputData.initializeType	= subgroups::SSBOData::InitializeNonZero;
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
tcu::TestCaseGroup* createSubgroupsBallotTests(tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(
			testCtx, "ballot", "Subgroup ballot category tests"));

	{
		const CaseDefinition caseDef = {VK_SHADER_STAGE_COMPUTE_BIT, false};
		addFunctionCaseWithPrograms(group.get(), getShaderStageName(caseDef.shaderStage), "", initPrograms, test, caseDef);
	}

	{
			const CaseDefinition caseDef = {VK_SHADER_STAGE_ALL_GRAPHICS, false};
			addFunctionCaseWithPrograms(group.get(), "graphic", "", initPrograms, test, caseDef);
	}

	{
		const CaseDefinition caseDef = {VK_SHADER_STAGE_VERTEX_BIT, true};
		addFunctionCaseWithPrograms(group.get(), getShaderStageName(caseDef.shaderStage)+"_framebuffer", "",
					initFrameBufferPrograms, test, caseDef);
	}

	return group.release();
}

} // subgroups
} // vkt
