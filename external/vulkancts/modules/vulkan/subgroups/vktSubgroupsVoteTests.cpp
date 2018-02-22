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

		if (0x7 != val)
		{
			return false;
		}
	}

	return true;
}

static bool checkVertexPipelineStagesNoSSBO(std::vector<const void*> datas,
									  deUint32 width, deUint32)
{
	const float* data =
		reinterpret_cast<const float*>(datas[0]);
	for (deUint32 x = 0; x < width; ++x)
	{
		deUint32 val = static_cast<deUint32>(data[x]);

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

							// The data should look (in binary) 0b111
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

std::string getOpTypeName(int opType)
{
	switch (opType)
	{
		default:
			DE_FATAL("Unsupported op type");
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
	bool				noSSBO;
};

void initFrameBufferPrograms(SourceCollections& programCollection, CaseDefinition caseDef)
{
	std::ostringstream	vertexSrc;
	std::ostringstream	fragmentSrc;

	if (VK_SHADER_STAGE_VERTEX_BIT == caseDef.shaderStage)
	{
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
			<< "  uint result;\n";
		if (OPTYPE_ALL == caseDef.opType)
		{
			vertexSrc << " result = " << getOpTypeName(caseDef.opType)
				<< "(true) ? 0x1 : 0;\n"
				<< "  result |= " << getOpTypeName(caseDef.opType)
				<< "(false) ? 0 : 0x2;\n"
				<< "  result |= 0x4;\n"
				<< "  out_color.r = float(result);\n";
		}
		else if (OPTYPE_ANY == caseDef.opType)
		{
			vertexSrc << "  result = " << getOpTypeName(caseDef.opType)
				<< "(true) ? 0x1 : 0;\n"
				<< "  result |= " << getOpTypeName(caseDef.opType)
				<< "(false) ? 0 : 0x2;\n"
				<< "  result |= 0x4;\n"
				<< "out_color.r = float(result);\n";
		}
		else if (OPTYPE_ALLEQUAL == caseDef.opType)
		{
			vertexSrc << "  result = " << getOpTypeName(caseDef.opType) << "("
				<< subgroups::getFormatNameForGLSL(caseDef.format) << "(1)) ? 0x1 : 0;\n"
				<< "  result |= " << getOpTypeName(caseDef.opType)
				<< "(gl_SubgroupInvocationID) ? 0 : 0x2;\n"
				<< "  if (subgroupElect()) result |= 0x2;\n"
				<< "  result |= " << getOpTypeName(caseDef.opType)
				<< "(data[0]) ? 0x4 : 0;\n"
				<< "  out_color.x = float(result);\n";
		}

		vertexSrc << "  gl_Position = in_position;\n"
			<< "  gl_PointSize = 1.0f;\n"
			<< "}\n";

		programCollection.glslSources.add("vert") << glu::VertexSource(vertexSrc.str()) << vk::ShaderBuildOptions(vk::SPIRV_VERSION_1_3, 0u);

		fragmentSrc << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450)<<"\n"
			<< "layout(location = 0) in vec4 in_color;\n"
			<< "layout(location = 0) out vec4 out_color;\n"
			<< "void main()\n"
			<<"{\n"
			<< "	out_color = in_color;\n"
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
				<< "(false) ? 0 : 0x2;\n"
				<< "  result[offset] |= " << getOpTypeName(caseDef.opType)
				<< "(data[gl_SubgroupInvocationID] > 0) ? 0x4 : 0;\n";
		}
		else if (OPTYPE_ANY == caseDef.opType)
		{
			src << "  result[offset] = " << getOpTypeName(caseDef.opType)
				<< "(true) ? 0x1 : 0;\n"
				<< "  result[offset] |= " << getOpTypeName(caseDef.opType)
				<< "(false) ? 0 : 0x2;\n"
				<< "  result[offset] |= " << getOpTypeName(caseDef.opType)
				<< "(data[gl_SubgroupInvocationID] == data[0]) ? 0x4 : 0;\n";
		}
		else if (OPTYPE_ALLEQUAL == caseDef.opType)
		{
			src << "  result[offset] = " << getOpTypeName(caseDef.opType) << "("
				<< subgroups::getFormatNameForGLSL(caseDef.format) << "(1)) ? 0x1 : 0;\n"
				<< "  result[offset] |= " << getOpTypeName(caseDef.opType)
				<< "(gl_SubgroupInvocationID) ? 0 : 0x2;\n"
				<< "  if (subgroupElect()) result[offset] |= 0x2;\n"
				<< "  result[offset] |= " << getOpTypeName(caseDef.opType)
				<< "(data[0]) ? 0x4 : 0;\n";
		}

		src << "}\n";

		programCollection.glslSources.add("comp")
				<< glu::ComputeSource(src.str()) << vk::ShaderBuildOptions(vk::SPIRV_VERSION_1_3, 0u);
	}
	else
	{
		const string source =
		(OPTYPE_ALL == caseDef.opType) ?
			"  result[offset] = " + getOpTypeName(caseDef.opType) +
			"(true) ? 0x1 : 0;\n"
			"  result[offset] |= " + getOpTypeName(caseDef.opType) +
			"(false) ? 0 : 0x2;\n"
			"  result[offset] |= 0x4;\n"
		: (OPTYPE_ANY == caseDef.opType) ?
				"  result[offset] = " + getOpTypeName(caseDef.opType) +
				"(true) ? 0x1 : 0;\n"
				"  result[offset] |= " + getOpTypeName(caseDef.opType) +
				"(false) ? 0 : 0x2;\n"
				"  result[offset] |= 0x4;\n"
		: (OPTYPE_ALLEQUAL == caseDef.opType) ?
				"  result[offset] = " + getOpTypeName(caseDef.opType) + "("
				+ subgroups::getFormatNameForGLSL(caseDef.format) + "(1)) ? 0x1 : 0;\n"
				"  result[offset] |= " + getOpTypeName(caseDef.opType) +
				"(gl_SubgroupInvocationID) ? 0 : 0x2;\n"
				"  if (subgroupElect()) result[offset] |= 0x2;\n"
				"  result[offset] |= " + getOpTypeName(caseDef.opType) +
				"(data[0]) ? 0x4 : 0;\n"
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
				<< glu::VertexSource(vertex) << vk::ShaderBuildOptions(vk::SPIRV_VERSION_1_3, 0u);
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
					<< glu::TessellationControlSource(tesc) << vk::ShaderBuildOptions(vk::SPIRV_VERSION_1_3, 0u);
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
					<< glu::TessellationEvaluationSource(tese) << vk::ShaderBuildOptions(vk::SPIRV_VERSION_1_3, 0u);
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

			subgroups::addGeometryShadersFromTemplate(geometry, vk::ShaderBuildOptions(vk::SPIRV_VERSION_1_3, 0u),
													  programCollection.glslSources);
		}

		{
			const string sourceFragment =
			(OPTYPE_ALL == caseDef.opType) ?
				"  result = " + getOpTypeName(caseDef.opType) +
				"(true) ? 0x1 : 0;\n"
				"  result |= " + getOpTypeName(caseDef.opType) +
				"(false) ? 0 : 0x2;\n"
				"  result |= 0x4;\n"
			: (OPTYPE_ANY == caseDef.opType) ?
					"  result = " + getOpTypeName(caseDef.opType) +
					"(true) ? 0x1 : 0;\n"
					"  result |= " + getOpTypeName(caseDef.opType) +
					"(false) ? 0 : 0x2;\n"
					"  result |= 0x4;\n"
			: (OPTYPE_ALLEQUAL == caseDef.opType) ?
					"  result = " + getOpTypeName(caseDef.opType) + "("
					+ subgroups::getFormatNameForGLSL(caseDef.format) + "(1)) ? 0x1 : 0;\n"
					"  result |= " + getOpTypeName(caseDef.opType) +
					"(gl_SubgroupInvocationID) ? 0 : 0x2;\n"
					"  if (subgroupElect()) result |= 0x2;\n"
					"  result |= " + getOpTypeName(caseDef.opType) +
					"(data[0]) ? 0x4 : 0;\n"
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

	if (!subgroups::isSubgroupFeatureSupportedForDevice(context, VK_SUBGROUP_FEATURE_VOTE_BIT))
	{
		TCU_THROW(NotSupportedError, "Device does not support subgroup vote operations");
	}

	if (subgroups::isDoubleFormat(caseDef.format) && !subgroups::isDoubleSupportedForDevice(context))
	{
		TCU_THROW(NotSupportedError, "Device does not support subgroup double operations");
	}

	//Tests which don't use the SSBO
	if (caseDef.noSSBO)
	{
		if (!subgroups::areSubgroupOperationsSupportedForStage(context, caseDef.shaderStage))
		{
			TCU_THROW(NotSupportedError, "Device does not support subgroup operations for this stage");
		}

		if (VK_SHADER_STAGE_VERTEX_BIT == caseDef.shaderStage)
		{
			subgroups::SSBOData inputData;
			inputData.format = caseDef.format;
			inputData.numElements = subgroups::maxSupportedSubgroupSize();
			inputData.initializeType = subgroups::SSBOData::InitializeNonZero;

			return subgroups::makeVertexFrameBufferTest(context, VK_FORMAT_R32_SFLOAT, &inputData, 1, checkVertexPipelineStagesNoSSBO);
		}
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

		subgroups::SSBOData inputData;
		inputData.format = caseDef.format;
		inputData.numElements = subgroups::maxSupportedSubgroupSize();
		inputData.initializeType = subgroups::SSBOData::InitializeNonZero;

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
		inputData.initializeType	= subgroups::SSBOData::InitializeNonZero;
		inputData.binding			= 4u;
		inputData.stages			= stages;

		return subgroups::allStages(context, VK_FORMAT_R32_UINT, &inputData, 1, checkVertexPipelineStages, stages);
	}
}

namespace vkt
{
namespace subgroups
{
tcu::TestCaseGroup* createSubgroupsVoteTests(tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(
			testCtx, "vote", "Subgroup vote category tests"));

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
				const CaseDefinition caseDef = {opTypeIndex, VK_SHADER_STAGE_COMPUTE_BIT, format, false};
				addFunctionCaseWithPrograms(group.get(),
											op + "_" + subgroups::getFormatNameForGLSL(format) + "_" + getShaderStageName(caseDef.shaderStage),
											"", initPrograms, test, caseDef);
			}

			{
				const CaseDefinition caseDef = {opTypeIndex, VK_SHADER_STAGE_ALL_GRAPHICS, format, false};
				addFunctionCaseWithPrograms(group.get(),
											op + "_" + subgroups::getFormatNameForGLSL(format) + "_graphic",
											"", initPrograms, test, caseDef);
			}

			{
				const CaseDefinition caseDef = {opTypeIndex, VK_SHADER_STAGE_VERTEX_BIT, format, true};
				addFunctionCaseWithPrograms(group.get(),
							op + "_" +
							subgroups::getFormatNameForGLSL(format)
							+ "_" + getShaderStageName(caseDef.shaderStage)+"_framebuffer", "",
							initFrameBufferPrograms, test, caseDef);
			}

		}
	}

	return group.release();
}

} // subgroups
} // vkt
