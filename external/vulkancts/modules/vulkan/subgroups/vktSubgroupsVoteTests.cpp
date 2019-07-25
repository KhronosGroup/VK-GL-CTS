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
	OPTYPE_ANY = 1,
	OPTYPE_ALLEQUAL = 2,
	OPTYPE_LAST_NON_ARB = 3,
	OPTYPE_ALL_ARB = 4,
	OPTYPE_ANY_ARB = 5,
	OPTYPE_ALLEQUAL_ARB = 6,
	OPTYPE_LAST
};

static bool checkVertexPipelineStages(const void* internalData, std::vector<const void*> datas,
									  deUint32 width, deUint32)
{
	DE_UNREF(internalData);
	return vkt::subgroups::check(datas, width, 0x1F);
}

static bool checkFragmentPipelineStages(const void* internalData, std::vector<const void*> datas,
									  deUint32 width, deUint32 height, deUint32)
{
	DE_UNREF(internalData);
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

static bool checkCompute(const void* internalData, std::vector<const void*> datas,
						 const deUint32 numWorkgroups[3], const deUint32 localSize[3],
						 deUint32)
{
	DE_UNREF(internalData);
	return vkt::subgroups::checkCompute(datas, numWorkgroups, localSize, 0x1F);
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
		case OPTYPE_ALL_ARB:
			return "allInvocationsARB";
		case OPTYPE_ANY_ARB:
			return "anyInvocationARB";
		case OPTYPE_ALLEQUAL_ARB:
			return "allInvocationsEqualARB";
	}
}

struct CaseDefinition
{
	int					opType;
	VkShaderStageFlags	shaderStage;
	VkFormat			format;
	de::SharedPtr<bool>	geometryPointSizeSupported;
	deBool				requiredSubgroupSize;
	deBool              requires8BitUniformBuffer;
	deBool              requires16BitUniformBuffer;
};

bool fmtIsBoolean(VkFormat format)
{
	// For reasons unknown, the tests use R8_USCALED as the boolean format
	return	format == VK_FORMAT_R8_USCALED || format == VK_FORMAT_R8G8_USCALED ||
			format == VK_FORMAT_R8G8B8_USCALED || format == VK_FORMAT_R8G8B8A8_USCALED;
}

const string extHeader(bool arbFunctions)
{
	return arbFunctions	?	"#extension GL_ARB_shader_group_vote: enable\n"
							"#extension GL_KHR_shader_subgroup_basic: enable\n"
						:	"#extension GL_KHR_shader_subgroup_vote: enable\n";
}

// The test source to use in a generic stage. Fragment and compute sources are different
const string stageTestSource(CaseDefinition caseDef)
{
	const bool formatIsBoolean = fmtIsBoolean(caseDef.format);

	const string op = getOpTypeName(caseDef.opType);
	const string fmt = subgroups::getFormatNameForGLSL(caseDef.format);

	return
		(OPTYPE_ALL == caseDef.opType || OPTYPE_ALL_ARB == caseDef.opType) ?
			"  result = " + op + "(true) ? 0x1 : 0;\n"
			"  result |= " + op + "(false) ? 0 : 0x1A;\n"
			"  result |= 0x4;\n"
		: (OPTYPE_ANY == caseDef.opType || OPTYPE_ANY_ARB == caseDef.opType) ?
			"  result = " + op + "(true) ? 0x1 : 0;\n"
			"  result |= " + op + "(false) ? 0 : 0x1A;\n"
			"  result |= 0x4;\n"
		: (OPTYPE_ALLEQUAL == caseDef.opType || OPTYPE_ALLEQUAL_ARB == caseDef.opType) ?
			"  " + fmt + " valueEqual = " + fmt + "(1.25 * float(data[gl_SubgroupInvocationID]) + 5.0);\n" +
			"  " + fmt + " valueNoEqual = " + fmt + (formatIsBoolean ? "(subgroupElect());\n" : "(gl_SubgroupInvocationID);\n") +
			"  result = " + op + "(" + fmt + "(1)) ? 0x1 : 0;\n"
			"  result |= "
				+ (formatIsBoolean ? "0x2" : op + "(" + fmt + "(gl_SubgroupInvocationID)) ? 0 : 0x2")
				+ ";\n"
			"  result |= " + op + "(data[0]) ? 0x4 : 0;\n"
			"  result |= " + op + "(valueEqual) ? 0x8 : 0x0;\n"
			"  result |= " + op + "(valueNoEqual) ? 0x0 : 0x10;\n"
			"  if (subgroupElect()) result |= 0x2 | 0x10;\n"
		: "";
}

void initFrameBufferPrograms (SourceCollections& programCollection, CaseDefinition caseDef)
{
	const vk::ShaderBuildOptions buildOptions	(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_3, 0u);
	const bool formatIsBoolean = fmtIsBoolean(caseDef.format);
	const bool arbFunctions = caseDef.opType > OPTYPE_LAST_NON_ARB;
	const string extensionHeader = extHeader(arbFunctions);

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

	const string source = stageTestSource(caseDef);

	const string fmt = subgroups::getFormatNameForGLSL(caseDef.format);

	if (VK_SHADER_STAGE_VERTEX_BIT == caseDef.shaderStage)
	{
		std::ostringstream vertexSrc;
		vertexSrc << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450)<<"\n"
			<< extensionHeader.c_str()
			<< subgroups::getAdditionalExtensionForFormat(caseDef.format)
			<< "layout(location = 0) out vec4 out_color;\n"
			<< "layout(location = 0) in highp vec4 in_position;\n"
			<< "layout(set = 0, binding = 0) uniform Buffer1\n"
			<< "{\n"
			<< "  " << fmt << " data[" << subgroups::maxSupportedSubgroupSize() << "];\n"
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
			<< extensionHeader.c_str()
			<< subgroups::getAdditionalExtensionForFormat(caseDef.format)
			<< "layout(points) in;\n"
			<< "layout(points, max_vertices = 1) out;\n"
			<< "layout(location = 0) out float out_color;\n"
			<< "layout(set = 0, binding = 0) uniform Buffer1\n"
			<< "{\n"
			<< "  " << fmt << " data[" << subgroups::maxSupportedSubgroupSize() << "];\n"
			<< "};\n"
			<< "\n"
			<< "void main (void)\n"
			<< "{\n"
			<< "  uint result;\n"
			<< source
			<< "  out_color = float(result);\n"
			<< "  gl_Position = gl_in[0].gl_Position;\n"
			<< (*caseDef.geometryPointSizeSupported ? "  gl_PointSize = gl_in[0].gl_PointSize;\n" : "")
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
			<< extensionHeader.c_str()
			<< subgroups::getAdditionalExtensionForFormat(caseDef.format)
			<< "layout(vertices = 2) out;\n"
			<< "layout(location = 0) out float out_color[];\n"
			<< "layout(set = 0, binding = 0) uniform Buffer1\n"
			<< "{\n"
			<< "  " << fmt << " data[" << subgroups::maxSupportedSubgroupSize() << "];\n"
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
			<< (*caseDef.geometryPointSizeSupported ? "  gl_out[gl_InvocationID].gl_PointSize = gl_in[gl_InvocationID].gl_PointSize;\n" : "")
			<< "}\n";

		programCollection.glslSources.add("tesc")
			<< glu::TessellationControlSource(controlSource.str()) << buildOptions;
		subgroups::setTesEvalShaderFrameBuffer(programCollection);
	}
	else if (VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT == caseDef.shaderStage)
	{
		std::ostringstream evaluationSource;
		evaluationSource << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450)<<"\n"
			<< extensionHeader.c_str()
			<< "#extension GL_EXT_tessellation_shader : require\n"
			<< subgroups::getAdditionalExtensionForFormat(caseDef.format)
			<< "layout(isolines, equal_spacing, ccw ) in;\n"
			<< "layout(location = 0) out float out_color;\n"
			<< "layout(set = 0, binding = 0) uniform Buffer1\n"
			<< "{\n"
			<< "  " << fmt << " data[" << subgroups::maxSupportedSubgroupSize() << "];\n"
			<< "};\n"
			<< "\n"
			<< "void main (void)\n"
			<< "{\n"
			<< "  uint result;\n"
			<< "  highp uint offset = gl_PrimitiveID * 2 + uint(gl_TessCoord.x + 0.5);\n"
			<< source
			<< "  out_color = float(result);\n"
			<< "  gl_Position = mix(gl_in[0].gl_Position, gl_in[1].gl_Position, gl_TessCoord.x);\n"
			<< (*caseDef.geometryPointSizeSupported ? "  gl_PointSize = gl_in[0].gl_PointSize;\n" : "")
			<< "}\n";

		subgroups::setTesCtrlShaderFrameBuffer(programCollection);
		programCollection.glslSources.add("tese")
				<< glu::TessellationEvaluationSource(evaluationSource.str()) << buildOptions;
	}
	else if (VK_SHADER_STAGE_FRAGMENT_BIT == caseDef.shaderStage)
	{
		const string op = getOpTypeName(caseDef.opType);
		const string sourceFragment =
		(OPTYPE_ALL == caseDef.opType || OPTYPE_ALL_ARB == caseDef.opType) ?
			"  result |= " + op + "(!gl_HelperInvocation) ? 0x0 : 0x1;\n"
			"  result |= " + op + "(false) ? 0 : 0x1A;\n"
			"  result |= 0x4;\n"
		: (OPTYPE_ANY == caseDef.opType || OPTYPE_ANY_ARB == caseDef.opType) ?
			"  result |= " + op + "(gl_HelperInvocation) ? 0x1 : 0x0;\n"
			"  result |= " + op + "(false) ? 0 : 0x1A;\n"
			"  result |= 0x4;\n"
		: (OPTYPE_ALLEQUAL == caseDef.opType || OPTYPE_ALLEQUAL_ARB == caseDef.opType) ?
			"  " + subgroups::getFormatNameForGLSL(caseDef.format) + " valueEqual = " + subgroups::getFormatNameForGLSL(caseDef.format) + "(1.25 * float(data[gl_SubgroupInvocationID]) + 5.0);\n" +
			"  " + subgroups::getFormatNameForGLSL(caseDef.format) + " valueNoEqual = " + subgroups::getFormatNameForGLSL(caseDef.format) + (formatIsBoolean ? "(subgroupElect());\n" : "(gl_SubgroupInvocationID);\n") +
			"  result |= " + getOpTypeName(caseDef.opType) + "("
			+ subgroups::getFormatNameForGLSL(caseDef.format) + "(1)) ? 0x10 : 0;\n"
			"  result |= "
				+ (formatIsBoolean ? "0x2" : op + "(" + fmt + "(gl_SubgroupInvocationID)) ? 0 : 0x2")
				+ ";\n"
			"  result |= " + op + "(data[0]) ? 0x4 : 0;\n"
			"  result |= " + op + "(valueEqual) ? 0x8 : 0x0;\n"
			"  result |= " + op + "(gl_HelperInvocation) ? 0x0 : 0x1;\n"
			"  if (subgroupElect()) result |= 0x2 | 0x10;\n"
		: "";

		std::ostringstream fragmentSource;
		fragmentSource << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450)<<"\n"
		<< extensionHeader.c_str()
		<< subgroups::getAdditionalExtensionForFormat(caseDef.format)
		<< "layout(location = 0) out uint out_color;\n"
		<< "layout(set = 0, binding = 0) uniform Buffer1\n"
		<< "{\n"
		<< "  " << fmt << " data[" << subgroups::maxSupportedSubgroupSize() << "];\n"
		<< "};\n"
		<< ""
		<< "void main()\n"
		<< "{\n"
		<< "  uint result = 0u;\n"
		<< "  if (dFdx(gl_SubgroupInvocationID * gl_FragCoord.x * gl_FragCoord.y) - dFdy(gl_SubgroupInvocationID * gl_FragCoord.x * gl_FragCoord.y) > 0.0f)\n"
		<< "  {\n"
		<< "    result |= 0x20;\n" // to be sure that compiler doesn't remove dFdx and dFdy executions
		<< "  }\n"
		<< (arbFunctions ?
			"  bool helper = anyInvocationARB(gl_HelperInvocation);\n" :
			"  bool helper = subgroupAny(gl_HelperInvocation);\n")
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
	const bool formatIsBoolean = fmtIsBoolean(caseDef.format);
	const bool arbFunctions = caseDef.opType > OPTYPE_LAST_NON_ARB;
	const string extensionHeader = extHeader(arbFunctions);

	const string op = getOpTypeName(caseDef.opType);
	const string fmt = subgroups::getFormatNameForGLSL(caseDef.format);

	if (VK_SHADER_STAGE_COMPUTE_BIT == caseDef.shaderStage)
	{
		std::ostringstream src;

		const string source =
		(OPTYPE_ALL == caseDef.opType || OPTYPE_ALL_ARB == caseDef.opType) ?
			"  result = " + op + "(true) ? 0x1 : 0;\n"
			"  result |= " + op + "(false) ? 0 : 0x1A;\n"
			"  result |= " + op + "(data[gl_SubgroupInvocationID] > 0) ? 0x4 : 0;\n"
		: (OPTYPE_ANY == caseDef.opType || OPTYPE_ANY_ARB == caseDef.opType) ?
			"  result = " + op + "(true) ? 0x1 : 0;\n"
			"  result |= " + op + "(false) ? 0 : 0x1A;\n"
			"  result |= " + op + "(data[gl_SubgroupInvocationID] == data[0]) ? 0x4 : 0;\n"
		: (OPTYPE_ALLEQUAL == caseDef.opType || OPTYPE_ALLEQUAL_ARB == caseDef.opType) ?
			"  " + subgroups::getFormatNameForGLSL(caseDef.format) + " valueEqual = " + subgroups::getFormatNameForGLSL(caseDef.format) + "(1.25 * float(data[gl_SubgroupInvocationID]) + 5.0);\n"
			"  " + subgroups::getFormatNameForGLSL(caseDef.format) + " valueNoEqual = " + subgroups::getFormatNameForGLSL(caseDef.format) + (formatIsBoolean ? "(subgroupElect());\n" : "(gl_SubgroupInvocationID);\n") +
			"  result = " + getOpTypeName(caseDef.opType) + "("
			+ subgroups::getFormatNameForGLSL(caseDef.format) + "(1)) ? 0x1 : 0;\n"
			"  result |= "
				+ (formatIsBoolean ? "0x2" : op + "(" + fmt + "(gl_SubgroupInvocationID)) ? 0 : 0x2")
				+ ";\n"
			"  result |= " + op + "(data[0]) ? 0x4 : 0x0;\n"
			"  result |= " + op + "(valueEqual) ? 0x8 : 0x0;\n"
			"  result |= " + op + "(valueNoEqual) ? 0x0 : 0x10;\n"
			"  if (subgroupElect()) result |= 0x2 | 0x10;\n"
		: "";

		src << "#version 450\n"
			<< extensionHeader.c_str()
			<< subgroups::getAdditionalExtensionForFormat(caseDef.format)
			<< "layout (local_size_x_id = 0, local_size_y_id = 1, "
			"local_size_z_id = 2) in;\n"
			<< "layout(set = 0, binding = 0, std430) buffer Buffer1\n"
			<< "{\n"
			<< "  uint res[];\n"
			<< "};\n"
			<< "layout(set = 0, binding = 1, std430) buffer Buffer2\n"
			<< "{\n"
			<< "  " << fmt << " data[];\n"
			<< "};\n"
			<< "\n"
			<< "void main (void)\n"
			<< "{\n"
			<< "  uint result;\n"
			<< "  uvec3 globalSize = gl_NumWorkGroups * gl_WorkGroupSize;\n"
			<< "  highp uint offset = globalSize.x * ((globalSize.y * "
			"gl_GlobalInvocationID.z) + gl_GlobalInvocationID.y) + "
			"gl_GlobalInvocationID.x;\n"
			<< source
			<< "  res[offset] = result;\n"
			<< "}\n";

		programCollection.glslSources.add("comp")
				<< glu::ComputeSource(src.str()) << vk::ShaderBuildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_3, 0u);
	}
	else
	{
		const string source = stageTestSource(caseDef);

		{
			const string vertex =
				"#version 450\n"
				+ extensionHeader
				+ subgroups::getAdditionalExtensionForFormat(caseDef.format) +
				"layout(set = 0, binding = 0, std430) buffer Buffer1\n"
				"{\n"
				"  uint res[];\n"
				"};\n"
				"layout(set = 0, binding = 4, std430) readonly buffer Buffer2\n"
				"{\n"
				"  " + fmt + " data[];\n"
				"};\n"
				"\n"
				"void main (void)\n"
				"{\n"
				"  uint result;\n"
				"  highp uint offset = gl_VertexIndex;\n"
				+ source +
				"  res[offset] = result;\n"
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
				+ extensionHeader
				+ subgroups::getAdditionalExtensionForFormat(caseDef.format) +
				"layout(vertices=1) out;\n"
				"layout(set = 0, binding = 1, std430) buffer Buffer1\n"
				"{\n"
				"  uint res[];\n"
				"};\n"
				"layout(set = 0, binding = 4, std430) readonly buffer Buffer2\n"
				"{\n"
				"  " + fmt + " data[];\n"
				"};\n"
				"\n"
				"void main (void)\n"
				"{\n"
				"  uint result;\n"
				"  highp uint offset = gl_PrimitiveID;\n"
				+ source +
				"  res[offset] = result;\n"
				"  if (gl_InvocationID == 0)\n"
				"  {\n"
				"    gl_TessLevelOuter[0] = 1.0f;\n"
				"    gl_TessLevelOuter[1] = 1.0f;\n"
				"  }\n"
				"  gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;\n"
				+ (*caseDef.geometryPointSizeSupported ? "  gl_out[gl_InvocationID].gl_PointSize = gl_in[gl_InvocationID].gl_PointSize;\n" : "") +
				"}\n";

			programCollection.glslSources.add("tesc")
					<< glu::TessellationControlSource(tesc) << vk::ShaderBuildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_3, 0u);
		}

		{
			const string tese =
				"#version 450\n"
				+ extensionHeader
				+ subgroups::getAdditionalExtensionForFormat(caseDef.format) +
				"layout(isolines) in;\n"
				"layout(set = 0, binding = 2, std430) buffer Buffer1\n"
				"{\n"
				"  uint res[];\n"
				"};\n"
				"layout(set = 0, binding = 4, std430) readonly buffer Buffer2\n"
				"{\n"
				"  " + fmt + " data[];\n"
				"};\n"
				"\n"
				"void main (void)\n"
				"{\n"
				"  uint result;\n"
				"  highp uint offset = gl_PrimitiveID * 2 + uint(gl_TessCoord.x + 0.5);\n"
				+ source +
				"  res[offset] = result;\n"
				"  float pixelSize = 2.0f/1024.0f;\n"
				"  gl_Position = gl_in[0].gl_Position + gl_TessCoord.x * pixelSize / 2.0f;\n"
				+ (*caseDef.geometryPointSizeSupported ? "  gl_PointSize = gl_in[0].gl_PointSize;\n" : "") +
				"}\n";

			programCollection.glslSources.add("tese")
					<< glu::TessellationEvaluationSource(tese) << vk::ShaderBuildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_3, 0u);
		}

		{
			const string geometry =
				"#version 450\n"
				+ extensionHeader
				+ subgroups::getAdditionalExtensionForFormat(caseDef.format) +
				"layout(${TOPOLOGY}) in;\n"
				"layout(points, max_vertices = 1) out;\n"
				"layout(set = 0, binding = 3, std430) buffer Buffer1\n"
				"{\n"
				"  uint res[];\n"
				"};\n"
				"layout(set = 0, binding = 4, std430) readonly buffer Buffer2\n"
				"{\n"
				"  " + fmt + " data[];\n"
				"};\n"
				"\n"
				"void main (void)\n"
				"{\n"
				"  uint result;\n"
				"  highp uint offset = gl_PrimitiveIDIn;\n"
				+ source +
				"  res[offset] = result;\n"
				"  gl_Position = gl_in[0].gl_Position;\n"
				+ (*caseDef.geometryPointSizeSupported ? "  gl_PointSize = gl_in[0].gl_PointSize;\n" : "") +
				"  EmitVertex();\n"
				"  EndPrimitive();\n"
				"}\n";

			subgroups::addGeometryShadersFromTemplate(geometry, vk::ShaderBuildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_3, 0u),
													  programCollection.glslSources);
		}

		{
			const string sourceFragment =
			(OPTYPE_ALL == caseDef.opType || OPTYPE_ALL_ARB == caseDef.opType) ?
				"  result = " + op + "(true) ? 0x1 : 0;\n"
				"  result |= " + op + "(false) ? 0 : 0x1A;\n"
				"  result |= 0x4;\n"
			: (OPTYPE_ANY == caseDef.opType || OPTYPE_ANY_ARB == caseDef.opType) ?
				"  result = " + op + "(true) ? 0x1 : 0;\n"
				"  result |= " + op + "(false) ? 0 : 0x1A;\n"
				"  result |= 0x4;\n"
			: (OPTYPE_ALLEQUAL == caseDef.opType || OPTYPE_ALLEQUAL_ARB == caseDef.opType) ?
				"  " + subgroups::getFormatNameForGLSL(caseDef.format) + " valueEqual = " + subgroups::getFormatNameForGLSL(caseDef.format) + "(1.25 * float(data[gl_SubgroupInvocationID]) + 5.0);\n" +
				"  " + subgroups::getFormatNameForGLSL(caseDef.format) + " valueNoEqual = " + subgroups::getFormatNameForGLSL(caseDef.format) + (formatIsBoolean ? "(subgroupElect());\n" : "(gl_SubgroupInvocationID);\n") +
				"  result = " + getOpTypeName(caseDef.opType) + "("
				+ subgroups::getFormatNameForGLSL(caseDef.format) + "(1)) ? 0x1 : 0;\n"
				"  result |= "
					+ (formatIsBoolean ? "0x2" : op + "(" + fmt + "(gl_SubgroupInvocationID)) ? 0 : 0x2")
					+ ";\n"
				"  result |= " + op + "(data[0]) ? 0x4 : 0;\n"
				"  result |= " + op + "(valueEqual) ? 0x8 : 0x0;\n"
				"  result |= " + op + "(valueNoEqual) ? 0x0 : 0x10;\n"
				"  if (subgroupElect()) result |= 0x2 | 0x10;\n"
			: "";
			const string fragment =
				"#version 450\n"
				+ extensionHeader
				+ subgroups::getAdditionalExtensionForFormat(caseDef.format) +
				"layout(location = 0) out uint result;\n"
				"layout(set = 0, binding = 4, std430) readonly buffer Buffer2\n"
				"{\n"
				"  " + fmt + " data[];\n"
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

	if (!subgroups::isFormatSupportedForDevice(context, caseDef.format))
		TCU_THROW(NotSupportedError, "Device does not support the specified format in subgroup operations");

	if (caseDef.requires16BitUniformBuffer)
	{
		if (!subgroups::is16BitUBOStorageSupported(context))
		{
			TCU_THROW(NotSupportedError, "Device does not support the specified format in subgroup operations");
		}
	}

	if (caseDef.requires8BitUniformBuffer)
	{
		if (!subgroups::is8BitUBOStorageSupported(context))
		{
			TCU_THROW(NotSupportedError, "Device does not support the specified format in subgroup operations");
		}
	}

	if (caseDef.opType > OPTYPE_LAST_NON_ARB)
	{
		context.requireDeviceFunctionality("VK_EXT_shader_subgroup_vote");
	}

	if (caseDef.requiredSubgroupSize)
	{
		if (!context.requireDeviceFunctionality("VK_EXT_subgroup_size_control"))
			TCU_THROW(NotSupportedError, "Device does not support VK_EXT_subgroup_size_control extension");
		VkPhysicalDeviceSubgroupSizeControlFeaturesEXT subgroupSizeControlFeatures;
		subgroupSizeControlFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_SIZE_CONTROL_FEATURES_EXT;
		subgroupSizeControlFeatures.pNext = DE_NULL;

		VkPhysicalDeviceFeatures2 features;
		features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
		features.pNext = &subgroupSizeControlFeatures;

		context.getInstanceInterface().getPhysicalDeviceFeatures2(context.getPhysicalDevice(), &features);

		if (subgroupSizeControlFeatures.subgroupSizeControl == DE_FALSE)
			TCU_THROW(NotSupportedError, "Device does not support varying subgroup sizes nor required subgroup size");

		if (subgroupSizeControlFeatures.computeFullSubgroups == DE_FALSE)
			TCU_THROW(NotSupportedError, "Device does not support full subgroups in compute shaders");
	}

	*caseDef.geometryPointSizeSupported = subgroups::isTessellationAndGeometryPointSizeSupported(context);
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

	if (caseDef.opType > OPTYPE_LAST_NON_ARB)
	{
		context.requireDeviceFunctionality("VK_EXT_shader_subgroup_vote");
	}

	subgroups::SSBOData inputData;
	inputData.format = caseDef.format;
	inputData.layout = subgroups::SSBOData::LayoutStd140;
	inputData.numElements = subgroups::maxSupportedSubgroupSize();
	inputData.initializeType = (OPTYPE_ALLEQUAL == caseDef.opType || OPTYPE_ALLEQUAL_ARB == caseDef.opType) ? subgroups::SSBOData::InitializeZero : subgroups::SSBOData::InitializeNonZero;

	if (VK_SHADER_STAGE_VERTEX_BIT == caseDef.shaderStage)
		return subgroups::makeVertexFrameBufferTest(context, VK_FORMAT_R32_UINT, &inputData, 1, DE_NULL, checkVertexPipelineStages);
	else if (VK_SHADER_STAGE_GEOMETRY_BIT == caseDef.shaderStage)
		return subgroups::makeGeometryFrameBufferTest(context, VK_FORMAT_R32_UINT, &inputData, 1, DE_NULL, checkVertexPipelineStages);
	else if (VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT == caseDef.shaderStage)
		return subgroups::makeTessellationEvaluationFrameBufferTest(context, VK_FORMAT_R32_UINT, &inputData, 1, DE_NULL, checkVertexPipelineStages, VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT);
	else if (VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT == caseDef.shaderStage)
		return subgroups::makeTessellationEvaluationFrameBufferTest(context, VK_FORMAT_R32_UINT, &inputData, 1, DE_NULL, checkVertexPipelineStages, VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT);
	else if (VK_SHADER_STAGE_FRAGMENT_BIT == caseDef.shaderStage)
		return subgroups::makeFragmentFrameBufferTest(context, VK_FORMAT_R32_UINT, &inputData, 1, DE_NULL, checkFragmentPipelineStages);
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
		inputData.layout = subgroups::SSBOData::LayoutStd430;
		inputData.numElements = subgroups::maxSupportedSubgroupSize();
		inputData.initializeType = (OPTYPE_ALLEQUAL == caseDef.opType || OPTYPE_ALLEQUAL_ARB == caseDef.opType) ? subgroups::SSBOData::InitializeZero : subgroups::SSBOData::InitializeNonZero;

		if (caseDef.requiredSubgroupSize == DE_FALSE)
			return subgroups::makeComputeTest(context, VK_FORMAT_R32_UINT, &inputData,
											  1, DE_NULL, checkCompute);

		tcu::TestLog& log	= context.getTestContext().getLog();
		VkPhysicalDeviceSubgroupSizeControlPropertiesEXT subgroupSizeControlProperties;
		subgroupSizeControlProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_SIZE_CONTROL_PROPERTIES_EXT;
		subgroupSizeControlProperties.pNext = DE_NULL;
		VkPhysicalDeviceProperties2 properties;
		properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
		properties.pNext = &subgroupSizeControlProperties;

		context.getInstanceInterface().getPhysicalDeviceProperties2(context.getPhysicalDevice(), &properties);

		log << tcu::TestLog::Message << "Testing required subgroup size range [" <<  subgroupSizeControlProperties.minSubgroupSize << ", "
			<< subgroupSizeControlProperties.maxSubgroupSize << "]" << tcu::TestLog::EndMessage;

		// According to the spec, requiredSubgroupSize must be a power-of-two integer.
		for (deUint32 size = subgroupSizeControlProperties.minSubgroupSize; size <= subgroupSizeControlProperties.maxSubgroupSize; size *= 2)
		{
			tcu::TestStatus result = subgroups::makeComputeTest(context, VK_FORMAT_R32_UINT, &inputData, 1, DE_NULL, checkCompute,
																size, VK_PIPELINE_SHADER_STAGE_CREATE_REQUIRE_FULL_SUBGROUPS_BIT_EXT);
			if (result.getCode() != QP_TEST_RESULT_PASS)
			{
				log << tcu::TestLog::Message << "subgroupSize " << size << " failed" << tcu::TestLog::EndMessage;
				return result;
			}
		}
		return tcu::TestStatus::pass("OK");
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
		inputData.layout			= subgroups::SSBOData::LayoutStd430;
		inputData.numElements		= subgroups::maxSupportedSubgroupSize();
		inputData.initializeType	= (OPTYPE_ALLEQUAL == caseDef.opType || OPTYPE_ALLEQUAL_ARB == caseDef.opType) ? subgroups::SSBOData::InitializeZero : subgroups::SSBOData::InitializeNonZero;
		inputData.binding			= 4u;
		inputData.stages			= stages;

		return subgroups::allStages(context, VK_FORMAT_R32_UINT, &inputData, 1, DE_NULL, checkVertexPipelineStages, stages);
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

	de::MovePtr<tcu::TestCaseGroup> graphicGroupARB(new tcu::TestCaseGroup(
		testCtx, "graphics", "Subgroup arithmetic category tests: graphics"));
	de::MovePtr<tcu::TestCaseGroup> computeGroupARB(new tcu::TestCaseGroup(
		testCtx, "compute", "Subgroup arithmetic category tests: compute"));
	de::MovePtr<tcu::TestCaseGroup> framebufferGroupARB(new tcu::TestCaseGroup(
		testCtx, "framebuffer", "Subgroup arithmetic category tests: framebuffer"));

	de::MovePtr<tcu::TestCaseGroup> fragHelperGroupARB(new tcu::TestCaseGroup(
		testCtx, "frag_helper", "Subgroup arithmetic category tests: fragment helper invocation"));

	const VkShaderStageFlags stages[] =
	{
		VK_SHADER_STAGE_VERTEX_BIT,
		VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,
		VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,
		VK_SHADER_STAGE_GEOMETRY_BIT,
	};

	const std::vector<VkFormat> formats = subgroups::getAllFormats();

	for (size_t formatIndex = 0; formatIndex < formats.size(); ++formatIndex)
	{
		const VkFormat format = formats[formatIndex];
		const deBool formatIsNotVector = VK_FORMAT_R8_USCALED == format || VK_FORMAT_R32_UINT == format ||
			VK_FORMAT_R32_SINT == format || VK_FORMAT_R32_SFLOAT == format || VK_FORMAT_R64_SFLOAT == format;

		for (int opTypeIndex = 0; opTypeIndex < OPTYPE_LAST; ++opTypeIndex)
		{
			// Skip OPTYPE_LAST_NON_ARB because it is not a real op type.
			if (opTypeIndex == OPTYPE_LAST_NON_ARB)
				continue;

			// Skip the non-nonvector tests because VK_EXT_shader_subgroup_vote functions only supports boolean scalar arguments.
			if (opTypeIndex > OPTYPE_LAST_NON_ARB && !formatIsNotVector)
				continue;

			// Skip non-boolean formats when testing allInvocationsEqualARB(bool value), because it requires a boolean
			// argument that should have the same value for all invocations. For the rest of formats, it won't be a boolean argument,
			// so it may give wrong results when converting to bool.
			if (opTypeIndex == OPTYPE_ALLEQUAL_ARB && format != VK_FORMAT_R8_USCALED)
				continue;

			// Skip the typed tests for all but subgroupAllEqual() and allInvocationsEqualARB()
			if ((VK_FORMAT_R32_UINT != format) && (OPTYPE_ALLEQUAL != opTypeIndex) && (OPTYPE_ALLEQUAL_ARB != opTypeIndex))
			{
				continue;
			}

			const std::string op = de::toLower(getOpTypeName(opTypeIndex));

			{
				CaseDefinition caseDef = {opTypeIndex, VK_SHADER_STAGE_COMPUTE_BIT, format, de::SharedPtr<bool>(new bool), deBool(false), deBool(false), DE_FALSE};
				if (opTypeIndex < OPTYPE_LAST_NON_ARB)
				{
					addFunctionCaseWithPrograms(computeGroup.get(),
												op + "_" + subgroups::getFormatNameForGLSL(format),
												"", supportedCheck, initPrograms, test, caseDef);
				}
				else
				{
					addFunctionCaseWithPrograms(computeGroupARB.get(),
												op + "_" + subgroups::getFormatNameForGLSL(format),
												"", supportedCheck, initPrograms, test, caseDef);
				}
			}

			{
				const CaseDefinition caseDef = {opTypeIndex, VK_SHADER_STAGE_ALL_GRAPHICS, format, de::SharedPtr<bool>(new bool), deBool(false), deBool(false), DE_FALSE};
				if (opTypeIndex < OPTYPE_LAST_NON_ARB)
				{
					addFunctionCaseWithPrograms(graphicGroup.get(),
												op + "_" + subgroups::getFormatNameForGLSL(format),
												"", supportedCheck, initPrograms, test, caseDef);
				}
				else
				{
					addFunctionCaseWithPrograms(graphicGroupARB.get(),
												op + "_" + subgroups::getFormatNameForGLSL(format),
												"", supportedCheck, initPrograms, test, caseDef);
				}
			}

			for (int stageIndex = 0; stageIndex < DE_LENGTH_OF_ARRAY(stages); ++stageIndex)
			{
				const CaseDefinition caseDef = {opTypeIndex, stages[stageIndex], format, de::SharedPtr<bool>(new bool), deBool(false), deBool(false), DE_FALSE};
				if (opTypeIndex < OPTYPE_LAST_NON_ARB)
				{
					addFunctionCaseWithPrograms(framebufferGroup.get(),
												op + "_" +
												subgroups::getFormatNameForGLSL(format)
												+ "_" + getShaderStageName(caseDef.shaderStage), "",
												supportedCheck, initFrameBufferPrograms, noSSBOtest, caseDef);
				}
				else
				{
					addFunctionCaseWithPrograms(framebufferGroupARB.get(),
												op + "_" +
												subgroups::getFormatNameForGLSL(format)
												+ "_" + getShaderStageName(caseDef.shaderStage), "",
												supportedCheck, initFrameBufferPrograms, noSSBOtest, caseDef);
				}
			}
			bool needs8BitUBOStorage = isFormat8bitTy(format);
			bool needs16BitUBOStorage = isFormat16BitTy(format);
			const CaseDefinition caseDef = {opTypeIndex, VK_SHADER_STAGE_FRAGMENT_BIT, format, de::SharedPtr<bool>(new bool),deBool(needs8BitUBOStorage), deBool(needs16BitUBOStorage), DE_FALSE };
			if (opTypeIndex < OPTYPE_LAST_NON_ARB)
			{
				addFunctionCaseWithPrograms(fragHelperGroup.get(),
											op + "_" +
											subgroups::getFormatNameForGLSL(format)
											+ "_" + getShaderStageName(caseDef.shaderStage), "",
											supportedCheck, initFrameBufferPrograms, noSSBOtest, caseDef);
			}
			else
			{
				addFunctionCaseWithPrograms(fragHelperGroupARB.get(),
											op + "_" +
											subgroups::getFormatNameForGLSL(format)
											+ "_" + getShaderStageName(caseDef.shaderStage), "",
											supportedCheck, initFrameBufferPrograms, noSSBOtest, caseDef);
			}
		}
	}

	de::MovePtr<tcu::TestCaseGroup> groupARB(new tcu::TestCaseGroup(
		testCtx, "ext_shader_subgroup_vote", "VK_EXT_shader_subgroup_vote category tests"));

	groupARB->addChild(graphicGroupARB.release());
	groupARB->addChild(computeGroupARB.release());
	groupARB->addChild(framebufferGroupARB.release());
	groupARB->addChild(fragHelperGroupARB.release());

	de::MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(
		testCtx, "vote", "Subgroup vote category tests"));

	group->addChild(graphicGroup.release());
	group->addChild(computeGroup.release());
	group->addChild(framebufferGroup.release());
	group->addChild(fragHelperGroup.release());

	group->addChild(groupARB.release());

	return group.release();
}

} // subgroups
} // vkt
