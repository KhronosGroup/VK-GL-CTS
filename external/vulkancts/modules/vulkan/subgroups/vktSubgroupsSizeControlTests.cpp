/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2019 The Khronos Group Inc.
 * Copyright (c) 2019 Valve Corporation.
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
 * \brief VK_EXT_subgroup_size_control Tests
 */ /*--------------------------------------------------------------------*/

#include "vktSubgroupsSizeControlTests.hpp"
#include "vktSubgroupsTestsUtils.hpp"
#include "vktTestCaseUtil.hpp"
#include "tcuTestLog.hpp"

#include <string>
#include <vector>

using namespace tcu;
using namespace std;
using namespace vk;
using namespace vkt;

namespace
{

enum RequiredSubgroupSizeMode
{
	REQUIRED_SUBGROUP_SIZE_NONE	= 0,
	REQUIRED_SUBGROUP_SIZE_MIN	= 1,
	REQUIRED_SUBGROUP_SIZE_MAX	= 2,
};

struct CaseDefinition
{
	deUint32							pipelineShaderStageCreateFlags;
	VkShaderStageFlags					shaderStage;
	deBool								requiresBallot;
	deUint32							requiredSubgroupSizeMode;
};

struct internalDataStruct
{
	const Context*			context;
	struct CaseDefinition	caseDef;
	deUint32				requiredSubgroupSize;
};

// Find greatest common divisor for a and b
deUint32 gcd (deUint32 a, deUint32 b)
{
	if ((0 != a) && (0 == b))
	{
		return a;
	}
	else
	{
		deUint32 greater = std::max(a, b);
		deUint32 lesser  = std::min(a, b);

		return gcd(lesser, greater % lesser);
	}
}

void getLocalSizes (VkPhysicalDeviceProperties physicalDeviceProperties, deUint32 numWorkGroupInvocations,
				   deUint32& localSizeX, deUint32& localSizeY, deUint32& localSizeZ)
{
	DE_ASSERT(numWorkGroupInvocations <= physicalDeviceProperties.limits.maxComputeWorkGroupInvocations);
	localSizeX = gcd(numWorkGroupInvocations, physicalDeviceProperties.limits.maxComputeWorkGroupSize[0]);
	localSizeY = gcd(deMax32(numWorkGroupInvocations / localSizeX, 1u), physicalDeviceProperties.limits.maxComputeWorkGroupSize[1]);
	localSizeZ = deMax32(numWorkGroupInvocations / (localSizeX * localSizeY), 1u);
}

deUint32 getRequiredSubgroupSizeFromMode (Context &context, const CaseDefinition caseDef,
										  VkPhysicalDeviceSubgroupSizeControlPropertiesEXT subgroupSizeControlProperties)
{
	switch (caseDef.requiredSubgroupSizeMode)
	{
	case REQUIRED_SUBGROUP_SIZE_MAX:	return subgroupSizeControlProperties.maxSubgroupSize;
	case REQUIRED_SUBGROUP_SIZE_MIN:	return subgroupSizeControlProperties.minSubgroupSize;
	case REQUIRED_SUBGROUP_SIZE_NONE:	return vkt::subgroups::getSubgroupSize(context);
	default:							TCU_THROW(NotSupportedError, "Unsupported Subgroup size");
	}
}

static bool checkVertexPipelineStages (const void* internalData, std::vector<const void*> datas,
									   deUint32 width, deUint32)
{
	const struct internalDataStruct *checkInternalData = reinterpret_cast<const struct internalDataStruct *>(internalData);
	const Context *context = checkInternalData->context;
	tcu::TestLog& log		= context->getTestContext().getLog();

	VkPhysicalDeviceSubgroupSizeControlPropertiesEXT subgroupSizeControlProperties;
	subgroupSizeControlProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_SIZE_CONTROL_PROPERTIES_EXT;
	subgroupSizeControlProperties.pNext = DE_NULL;
	VkPhysicalDeviceProperties2 properties;
	properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
	properties.pNext = &subgroupSizeControlProperties;

	context->getInstanceInterface().getPhysicalDeviceProperties2(context->getPhysicalDevice(), &properties);
	const deUint32* data = reinterpret_cast<const deUint32*>(datas[0]);

	for (deUint32 i = 0; i < width; i++)
	{
		if (data[i] > subgroupSizeControlProperties.maxSubgroupSize ||
			data[i] < subgroupSizeControlProperties.minSubgroupSize)
		{
			log << tcu::TestLog::Message << "gl_SubgroupSize (" << data[i] << ") value is outside limits (" << subgroupSizeControlProperties.minSubgroupSize << ", " << subgroupSizeControlProperties.maxSubgroupSize << ")" << tcu::TestLog::EndMessage;
			return DE_FALSE;
		}

		if (checkInternalData->caseDef.requiredSubgroupSizeMode != REQUIRED_SUBGROUP_SIZE_NONE && data[i] != checkInternalData->requiredSubgroupSize)
		{
			log << tcu::TestLog::Message << "gl_SubgroupSize (" << data[i] << ") is not equal to the required subgroup size value (" << checkInternalData->requiredSubgroupSize << ")" << tcu::TestLog::EndMessage;
			return DE_FALSE;
		}
	}

	return DE_TRUE;
}

static bool checkFragmentPipelineStages (const void* internalData, std::vector<const void*> datas,
										 deUint32 width, deUint32 height, deUint32)
{
	const struct internalDataStruct *checkInternalData = reinterpret_cast<const struct internalDataStruct *>(internalData);
	const Context *context = checkInternalData->context;
	tcu::TestLog& log		= context->getTestContext().getLog();

	VkPhysicalDeviceSubgroupSizeControlPropertiesEXT subgroupSizeControlProperties;
	subgroupSizeControlProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_SIZE_CONTROL_PROPERTIES_EXT;
	subgroupSizeControlProperties.pNext = DE_NULL;
	VkPhysicalDeviceProperties2 properties;
	properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
	properties.pNext = &subgroupSizeControlProperties;
	context->getInstanceInterface().getPhysicalDeviceProperties2(context->getPhysicalDevice(), &properties);

	const deUint32* data = reinterpret_cast<const deUint32*>(datas[0]);

	for (deUint32 x = 0u; x < width; ++x)
	{
		for (deUint32 y = 0u; y < height; ++y)
		{
			const deUint32 ndx = (x * height + y);

			if (data[ndx] > subgroupSizeControlProperties.maxSubgroupSize ||
				data[ndx] < subgroupSizeControlProperties.minSubgroupSize)
			{
				log << tcu::TestLog::Message << "gl_SubgroupSize (" << data[ndx] << ") value is outside limits (" << subgroupSizeControlProperties.minSubgroupSize << ", " << subgroupSizeControlProperties.maxSubgroupSize << ")" << tcu::TestLog::EndMessage;
				return DE_FALSE;
			}

			if (checkInternalData->caseDef.requiredSubgroupSizeMode != REQUIRED_SUBGROUP_SIZE_NONE &&
				data[ndx] != checkInternalData->requiredSubgroupSize)
			{
				log << tcu::TestLog::Message << "gl_SubgroupSize (" << data[ndx] << ") is not equal to the required subgroup size value (" << checkInternalData->requiredSubgroupSize << ")" << tcu::TestLog::EndMessage;
				return DE_FALSE;
			}
		}
	}
	return true;
}

static bool checkCompute (const void* internalData, std::vector<const void*> datas,
						  const deUint32 numWorkgroups[3], const deUint32 localSize[3],
						  deUint32)
{
	const struct internalDataStruct *checkInternalData = reinterpret_cast<const struct internalDataStruct *>(internalData);
	const Context *context = checkInternalData->context;
	tcu::TestLog& log		= context->getTestContext().getLog();

	VkPhysicalDeviceSubgroupSizeControlPropertiesEXT subgroupSizeControlProperties;
	subgroupSizeControlProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_SIZE_CONTROL_PROPERTIES_EXT;
	subgroupSizeControlProperties.pNext = DE_NULL;
	VkPhysicalDeviceProperties2 properties;
	properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
	properties.pNext = &subgroupSizeControlProperties;
	context->getInstanceInterface().getPhysicalDeviceProperties2(context->getPhysicalDevice(), &properties);

	const deUint32 globalSizeX = numWorkgroups[0] * localSize[0];
	const deUint32 globalSizeY = numWorkgroups[1] * localSize[1];
	const deUint32 globalSizeZ = numWorkgroups[2] * localSize[2];
	const deUint32 width = globalSizeX * globalSizeY * globalSizeZ;
	const deUint32* data = reinterpret_cast<const deUint32*>(datas[0]);

	for (deUint32 i = 0; i < width; i++)
	{
		if (data[i] > subgroupSizeControlProperties.maxSubgroupSize ||
			data[i] < subgroupSizeControlProperties.minSubgroupSize)
		{
			log << tcu::TestLog::Message << "[" << localSize[0] << ", " << localSize[1] << ", " << localSize[2] << "] "
				<< "gl_SubgroupSize (" << data[i] << ") value is outside limits (" << subgroupSizeControlProperties.minSubgroupSize << ", " << subgroupSizeControlProperties.maxSubgroupSize << ")" << tcu::TestLog::EndMessage;
			return DE_FALSE;
		}

		if (checkInternalData->caseDef.requiredSubgroupSizeMode != REQUIRED_SUBGROUP_SIZE_NONE &&
			data[i] != checkInternalData->requiredSubgroupSize)
		{
			log << tcu::TestLog::Message << "[" << localSize[0] << ", " << localSize[1] << ", " << localSize[2] << "] "
				<< "gl_SubgroupSize (" << data[i] << ") is not equal to the required subgroup size value (" << checkInternalData->requiredSubgroupSize << ")" << tcu::TestLog::EndMessage;
			return DE_FALSE;
		}
	}

	return DE_TRUE;
}

static bool checkComputeRequireFull (const void* internalData, std::vector<const void*> datas,
									 const deUint32 numWorkgroups[3], const deUint32 localSize[3],
									 deUint32)
{
	const struct internalDataStruct *checkInternalData = reinterpret_cast<const struct internalDataStruct *>(internalData);
	const Context *context = checkInternalData->context;
	tcu::TestLog& log		= context->getTestContext().getLog();

	VkPhysicalDeviceSubgroupSizeControlPropertiesEXT subgroupSizeControlProperties;
	subgroupSizeControlProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_SIZE_CONTROL_PROPERTIES_EXT;
	subgroupSizeControlProperties.pNext = DE_NULL;

	VkPhysicalDeviceSubgroupProperties subgroupProperties;
	subgroupProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES;
	subgroupProperties.pNext = &subgroupSizeControlProperties;

	VkPhysicalDeviceProperties2 properties;
	properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
	properties.pNext = &subgroupProperties;

	context->getInstanceInterface().getPhysicalDeviceProperties2(context->getPhysicalDevice(), &properties);

	const deUint32 globalSizeX = numWorkgroups[0] * localSize[0];
	const deUint32 globalSizeY = numWorkgroups[1] * localSize[1];
	const deUint32 globalSizeZ = numWorkgroups[2] * localSize[2];
	const deUint32 width = globalSizeX * globalSizeY * globalSizeZ;
	const UVec4* data = reinterpret_cast<const UVec4*>(datas[0]);

	deUint32 numSubgroups = (localSize[0] * localSize[1] * localSize[2]) / checkInternalData->requiredSubgroupSize;

	for (deUint32 i = 0; i < width; i++)
	{
		if (data[i].x() > subgroupSizeControlProperties.maxSubgroupSize ||
			data[i].x() < subgroupSizeControlProperties.minSubgroupSize)
		{
			log << tcu::TestLog::Message << "[" << localSize[0] << ", " << localSize[1] << ", " << localSize[2] << "] "
				<< "gl_SubgroupSize value ( " << data[i].x() << ") is outside limits [" << subgroupSizeControlProperties.minSubgroupSize << ", " << subgroupSizeControlProperties.maxSubgroupSize << "]" << tcu::TestLog::EndMessage;
			return DE_FALSE;
		}

		if (data[i].x() != data[i].y())
		{
			log << tcu::TestLog::Message << "[" << localSize[0] << ", " << localSize[1] << ", " << localSize[2] << "] "
				<< "gl_SubgroupSize ( " << data[i].x() << ") does not match the active number of subgroup invocations (" << data[i].y() << ")" << tcu::TestLog::EndMessage;
			return DE_FALSE;
		}

		if (checkInternalData->caseDef.pipelineShaderStageCreateFlags == VK_PIPELINE_SHADER_STAGE_CREATE_REQUIRE_FULL_SUBGROUPS_BIT_EXT &&
			data[i].x() != checkInternalData->requiredSubgroupSize)
		{
			log << tcu::TestLog::Message << "[" << localSize[0] << ", " << localSize[1] << ", " << localSize[2] << "] "
				<< "expected subgroupSize (" << checkInternalData->requiredSubgroupSize << ") doesn't match gl_SubgroupSize ( " << data[i].x() << ")" << tcu::TestLog::EndMessage;
			return DE_FALSE;
		}

		if (checkInternalData->caseDef.pipelineShaderStageCreateFlags == VK_PIPELINE_SHADER_STAGE_CREATE_REQUIRE_FULL_SUBGROUPS_BIT_EXT && data[i].z() != numSubgroups)
		{
			log << tcu::TestLog::Message << "[" << localSize[0] << ", " << localSize[1] << ", " << localSize[2] << "] "
				<< "expected number of subgroups dispatched (" << numSubgroups << ") doesn't match gl_NumSubgroups (" << data[i].z() << ")" << tcu::TestLog::EndMessage;
			return DE_FALSE;
		}
	}

	return DE_TRUE;
}

void initFrameBufferPrograms (SourceCollections& programCollection, CaseDefinition caseDef)
{
	const vk::ShaderBuildOptions	buildOptions	(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_3, 0u);

	if (VK_SHADER_STAGE_FRAGMENT_BIT != caseDef.shaderStage)
		subgroups::setFragmentShaderFrameBuffer(programCollection);

	if (VK_SHADER_STAGE_VERTEX_BIT != caseDef.shaderStage && VK_SHADER_STAGE_FRAGMENT_BIT != caseDef.shaderStage)
		subgroups::setVertexShaderFrameBuffer(programCollection);

	std::string bdyStr = "uint tempResult = gl_SubgroupSize;\n";

	if (VK_SHADER_STAGE_VERTEX_BIT == caseDef.shaderStage)
	{
		std::ostringstream				vertex;
		vertex << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450)<<"\n"
			<< "#extension GL_KHR_shader_subgroup_basic: enable\n"
			<< "layout(location = 0) in highp vec4 in_position;\n"
			<< "layout(location = 0) out float out_color;\n"
			<< "\n"
			<< "void main (void)\n"
			<< "{\n"
			<< bdyStr
			<< "  out_color = float(tempResult);\n"
			<< "  gl_Position = in_position;\n"
			<< "  gl_PointSize = 1.0f;\n"
			<< "}\n";
		programCollection.glslSources.add("vert")
			<< glu::VertexSource(vertex.str()) << buildOptions;
	}
	else if (VK_SHADER_STAGE_GEOMETRY_BIT == caseDef.shaderStage)
	{
		std::ostringstream geometry;

		geometry << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450)<<"\n"
			<< "#extension GL_KHR_shader_subgroup_basic: enable\n"
			<< "layout(points) in;\n"
			<< "layout(points, max_vertices = 1) out;\n"
			<< "layout(location = 0) out float out_color;\n"
			<< "void main (void)\n"
			<< "{\n"
			<< bdyStr
			<< "  out_color = float(tempResult);\n"
			<< "  gl_Position = gl_in[0].gl_Position;\n"
			<< "  gl_PointSize = 1.0f;"
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
			<< "#extension GL_KHR_shader_subgroup_basic: enable\n"
			<< "layout(vertices = 2) out;\n"
			<< "layout(location = 0) out float out_color[];\n"
			<< "\n"
			<< "void main (void)\n"
			<< "{\n"
			<< "  if (gl_InvocationID == 0)\n"
			<< "  {\n"
			<< "    gl_TessLevelOuter[0] = 1.0f;\n"
			<< "    gl_TessLevelOuter[1] = 1.0f;\n"
			<< "  }\n"
			<< bdyStr
			<< "  out_color[gl_InvocationID ] = float(tempResult);\n"
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
			<< "#extension GL_KHR_shader_subgroup_basic: enable\n"
			<< "layout(isolines, equal_spacing, ccw ) in;\n"
			<< "layout(location = 0) out float out_color;\n"
			<< "void main (void)\n"
			<< "{\n"
			<< bdyStr
			<< "  out_color  = float(tempResult);\n"
			<< "  gl_Position = mix(gl_in[0].gl_Position, gl_in[1].gl_Position, gl_TessCoord.x);\n"
			<< "}\n";

		subgroups::setTesCtrlShaderFrameBuffer(programCollection);
		programCollection.glslSources.add("tese")
			<< glu::TessellationEvaluationSource(evaluationSource.str()) << buildOptions;
	}
	else if (VK_SHADER_STAGE_FRAGMENT_BIT == caseDef.shaderStage)
	{
		const string vertex	= string(glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450)) + "\n"
			"void main (void)\n"
			"{\n"
			"  vec2 uv = vec2(float(gl_VertexIndex & 1), float((gl_VertexIndex >> 1) & 1));\n"
			"  gl_Position = vec4(uv * 4.0f -2.0f, 0.0f, 1.0f);\n"
			"  gl_PointSize = 1.0f;\n"
			"}\n";
		programCollection.glslSources.add("vert") << glu::VertexSource(vertex) << buildOptions;

		std::ostringstream fragmentSource;
		fragmentSource << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450)<<"\n"
					   << "precision highp int;\n"
						<< "#extension GL_KHR_shader_subgroup_basic: enable\n"
					   << "layout(location = 0) out uint out_color;\n"
					   << "void main()\n"
					   << "{\n"
					   << bdyStr
					   << "	 out_color = tempResult;\n"
					   << "}\n";
		programCollection.glslSources.add("fragment")
			<< glu::FragmentSource(fragmentSource.str()) << buildOptions;
	}
	else
	{
		DE_FATAL("Unsupported shader stage");
	}
}

void initPrograms (SourceCollections& programCollection, CaseDefinition caseDef)
{
	std::string bdyStr = "  uint tempResult = gl_SubgroupSize;\n";

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
			<< "void main (void)\n"
			<< "{\n"
			<< "  uvec3 globalSize = gl_NumWorkGroups * gl_WorkGroupSize;\n"
			<< "  highp uint offset = globalSize.x * ((globalSize.y * "
			"gl_GlobalInvocationID.z) + gl_GlobalInvocationID.y) + "
			"gl_GlobalInvocationID.x;\n"
			<< bdyStr
			<< "  result[offset] = tempResult;\n"
			<< "}\n";

		programCollection.glslSources.add("comp")
				<< glu::ComputeSource(src.str()) << vk::ShaderBuildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_3, 0u);
	}
	else
	{
		const string vertex =
			"#version 450\n"
			"#extension GL_KHR_shader_subgroup_basic: enable\n"
			"layout(set = 0, binding = 0, std430) buffer Buffer1\n"
			"{\n"
			"  uint result[];\n"
			"};\n"
			"\n"
			"void main (void)\n"
			"{\n"
			+ bdyStr +
			"  result[gl_VertexIndex] = tempResult;\n"
			"  float pixelSize = 2.0f/1024.0f;\n"
			"  float pixelPosition = pixelSize/2.0f - 1.0f;\n"
			"  gl_Position = vec4(float(gl_VertexIndex) * pixelSize + pixelPosition, 0.0f, 0.0f, 1.0f);\n"
			"  gl_PointSize = 1.0f;\n"
			"}\n";

		const string tesc =
			"#version 450\n"
			"#extension GL_KHR_shader_subgroup_basic: enable\n"
			"layout(vertices=1) out;\n"
			"layout(set = 0, binding = 1, std430) buffer Buffer1\n"
			"{\n"
			"  uint result[];\n"
			"};\n"
			"\n"
			"void main (void)\n"
			"{\n"
			+ bdyStr +
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
			"#extension GL_KHR_shader_subgroup_basic: enable\n"
			"layout(isolines) in;\n"
			"layout(set = 0, binding = 2, std430) buffer Buffer1\n"
			"{\n"
			"  uint result[];\n"
			"};\n"
			"\n"
			"void main (void)\n"
			"{\n"
			+ bdyStr +
			"  result[gl_PrimitiveID * 2 + uint(gl_TessCoord.x + 0.5)] = tempResult;\n"
			"  float pixelSize = 2.0f/1024.0f;\n"
			"  gl_Position = gl_in[0].gl_Position + gl_TessCoord.x * pixelSize / 2.0f;\n"
			"}\n";

		const string geometry =
			"#version 450\n"
			"#extension GL_KHR_shader_subgroup_basic: enable\n"
			"layout(${TOPOLOGY}) in;\n"
			"layout(points, max_vertices = 1) out;\n"
			"layout(set = 0, binding = 3, std430) buffer Buffer1\n"
			"{\n"
			"  uint result[];\n"
			"};\n"
			"\n"
			"void main (void)\n"
			"{\n"
			+ bdyStr +
			"  result[gl_PrimitiveIDIn] = tempResult;\n"
			"  gl_Position = gl_in[0].gl_Position;\n"
			"  gl_PointSize = 1.0f;\n"
			"  EmitVertex();\n"
			"  EndPrimitive();\n"
			"}\n";

		const string fragment =
			"#version 450\n"
			"#extension GL_KHR_shader_subgroup_basic: enable\n"
			"layout(location = 0) out uint result;\n"
			"void main (void)\n"
			"{\n"
			+ bdyStr +
			"  result = tempResult;\n"
			"}\n";

		subgroups::addNoSubgroupShader(programCollection);

		programCollection.glslSources.add("vert")
				<< glu::VertexSource(vertex) << vk::ShaderBuildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_3, 0u);
		programCollection.glslSources.add("tesc")
				<< glu::TessellationControlSource(tesc) << vk::ShaderBuildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_3, 0u);
		programCollection.glslSources.add("tese")
				<< glu::TessellationEvaluationSource(tese) << vk::ShaderBuildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_3, 0u);
		subgroups::addGeometryShadersFromTemplate(geometry, vk::ShaderBuildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_3, 0u),
												  programCollection.glslSources);
		programCollection.glslSources.add("fragment")
				<< glu::FragmentSource(fragment)<< vk::ShaderBuildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_3, 0u);
	}
}

void initProgramsRequireFull (SourceCollections& programCollection, CaseDefinition caseDef)
{
	if (VK_SHADER_STAGE_COMPUTE_BIT != caseDef.shaderStage)
		DE_FATAL("Unsupported shader stage");

	std::string bdyStr = "  uint tempResult = gl_SubgroupSize;\n";

	std::ostringstream src;

	src << "#version 450\n"
		<< "#extension GL_KHR_shader_subgroup_basic: enable\n"
		<< "#extension GL_KHR_shader_subgroup_ballot: enable\n"
		<< "layout (local_size_x_id = 0, local_size_y_id = 1, "
		"local_size_z_id = 2) in;\n"
		<< "layout(set = 0, binding = 0, std430) buffer Buffer1\n"
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
		<< "   result[offset].x = gl_SubgroupSize;" // save the subgroup size value
		<< "   uint numActive = subgroupBallotBitCount(subgroupBallot(true));\n"
		<< "   result[offset].y = numActive;\n" // save the number of active subgroup invocations
		<< "   result[offset].z = gl_NumSubgroups;" // save the number of subgroups dispatched.
		<< "}\n";

	programCollection.glslSources.add("comp")
		<< glu::ComputeSource(src.str()) << vk::ShaderBuildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_3, 0u);
}

void supportedCheck (Context& context)
{
	if (!subgroups::isSubgroupSupported(context))
		TCU_THROW(NotSupportedError, "Subgroup operations are not supported");

	if (!context.requireDeviceFunctionality("VK_EXT_subgroup_size_control"))
	{
		TCU_THROW(NotSupportedError, "Device does not support VK_EXT_subgroups_size_control extension");
	}
}

void supportedCheckFeatures (Context& context, CaseDefinition caseDef)
{
	supportedCheck(context);

	if (!subgroups::areSubgroupOperationsSupportedForStage(context, caseDef.shaderStage))
	{
		TCU_THROW(NotSupportedError, "Shader stage is required to support subgroup operations!");
	}

	if (caseDef.shaderStage == VK_SHADER_STAGE_ALL_GRAPHICS)
	{
		VkPhysicalDeviceFeatures features;
		context.getInstanceInterface().getPhysicalDeviceFeatures(context.getPhysicalDevice(), &features);
		if (!features.tessellationShader || !features.geometryShader)
			TCU_THROW(NotSupportedError, "Device does not support tessellation or geometry shaders");
	}

	if (caseDef.requiresBallot && !subgroups::isSubgroupFeatureSupportedForDevice(context, VK_SUBGROUP_FEATURE_BALLOT_BIT))
	{
		TCU_THROW(NotSupportedError, "Device does not support subgroup ballot operations");
	}

	if (caseDef.requiredSubgroupSizeMode != REQUIRED_SUBGROUP_SIZE_NONE ||
		caseDef.pipelineShaderStageCreateFlags == VK_PIPELINE_SHADER_STAGE_CREATE_ALLOW_VARYING_SUBGROUP_SIZE_BIT_EXT)
	{
		VkPhysicalDeviceSubgroupSizeControlFeaturesEXT subgroupSizeControlFeatures;
		subgroupSizeControlFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_SIZE_CONTROL_FEATURES_EXT;
		subgroupSizeControlFeatures.pNext = DE_NULL;

		VkPhysicalDeviceFeatures2 features;
		features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
		features.pNext = &subgroupSizeControlFeatures;

		context.getInstanceInterface().getPhysicalDeviceFeatures2(context.getPhysicalDevice(), &features);

		if (subgroupSizeControlFeatures.subgroupSizeControl == DE_FALSE)
			TCU_THROW(NotSupportedError, "Device does not support varying subgroup sizes nor required subgroup size");

		if (caseDef.requiredSubgroupSizeMode != REQUIRED_SUBGROUP_SIZE_NONE)
		{
			VkPhysicalDeviceSubgroupSizeControlPropertiesEXT subgroupSizeControlProperties;
			subgroupSizeControlProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_SIZE_CONTROL_PROPERTIES_EXT;
			subgroupSizeControlProperties.pNext = DE_NULL;

			VkPhysicalDeviceProperties2 properties;
			properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
			properties.pNext = &subgroupSizeControlProperties;

			context.getInstanceInterface().getPhysicalDeviceProperties2(context.getPhysicalDevice(), &properties);

			if ((subgroupSizeControlProperties.requiredSubgroupSizeStages & caseDef.shaderStage) != caseDef.shaderStage)
				TCU_THROW(NotSupportedError, "Device does not support setting required subgroup size for the stages selected");
		}
	}

	if (caseDef.pipelineShaderStageCreateFlags == VK_PIPELINE_SHADER_STAGE_CREATE_REQUIRE_FULL_SUBGROUPS_BIT_EXT)
	{
		VkPhysicalDeviceSubgroupSizeControlFeaturesEXT subgroupSizeControlFeatures;
		subgroupSizeControlFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_SIZE_CONTROL_FEATURES_EXT;
		subgroupSizeControlFeatures.pNext = DE_NULL;

		VkPhysicalDeviceFeatures2 features;
		features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
		features.pNext = &subgroupSizeControlFeatures;

		context.getInstanceInterface().getPhysicalDeviceFeatures2(context.getPhysicalDevice(), &features);

		if (subgroupSizeControlFeatures.computeFullSubgroups == DE_FALSE)
			TCU_THROW(NotSupportedError, "Device does not support full subgroups in compute shaders");
	}
}

void supportedCheckFeaturesShader (Context& context, CaseDefinition caseDef)
{
	supportedCheckFeatures(context, caseDef);

	vkt::subgroups::supportedCheckShader(context, caseDef.shaderStage);
}

tcu::TestStatus noSSBOtest (Context& context, const CaseDefinition caseDef)
{
	struct internalDataStruct internalData =
	{
		&context,
		caseDef,
		0u,
	};

	if (VK_SHADER_STAGE_VERTEX_BIT == caseDef.shaderStage)
		return subgroups::makeVertexFrameBufferTestRequiredSubgroupSize(context, VK_FORMAT_R32_UINT, DE_NULL, 0, &internalData, checkVertexPipelineStages, caseDef.pipelineShaderStageCreateFlags, 0u);
	else if (VK_SHADER_STAGE_GEOMETRY_BIT == caseDef.shaderStage)
		return subgroups::makeGeometryFrameBufferTestRequiredSubgroupSize(context, VK_FORMAT_R32_UINT, DE_NULL, 0, &internalData, checkVertexPipelineStages, caseDef.pipelineShaderStageCreateFlags, 0u);
	else if ((VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT | VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT) & caseDef.shaderStage)
		return subgroups::makeTessellationEvaluationFrameBufferTestRequiredSubgroupSize(context, VK_FORMAT_R32_UINT, DE_NULL, 0, &internalData, checkVertexPipelineStages, caseDef.shaderStage, caseDef.pipelineShaderStageCreateFlags, 0u);
	else if (VK_SHADER_STAGE_FRAGMENT_BIT == caseDef.shaderStage)
		return subgroups::makeFragmentFrameBufferTestRequiredSubgroupSize(context, VK_FORMAT_R32_UINT, DE_NULL, 0, &internalData, checkFragmentPipelineStages, caseDef.pipelineShaderStageCreateFlags, 0u);

	else
		TCU_THROW(InternalError, "Unhandled shader stage");
}

tcu::TestStatus test (Context& context, const CaseDefinition caseDef)
{
	if (VK_SHADER_STAGE_COMPUTE_BIT == caseDef.shaderStage)
	{
		const deUint32 numWorkgroups[3] = {1, 1, 1};
		deUint32 subgroupSize = vkt::subgroups::getSubgroupSize(context);

		VkPhysicalDeviceProperties physicalDeviceProperties;
		context.getInstanceInterface().getPhysicalDeviceProperties(context.getPhysicalDevice(), &physicalDeviceProperties);
		deUint32 localSizeX, localSizeY, localSizeZ;
		// Calculate the local workgroup sizes to exercise the maximum supported by the driver
		getLocalSizes(physicalDeviceProperties, physicalDeviceProperties.limits.maxComputeWorkGroupInvocations, localSizeX, localSizeY, localSizeZ);

		const deUint32 localSizesToTestCount = 16;
		deUint32 localSizesToTest[localSizesToTestCount][3] =
		{
			{1, 1, 1},
			{32, 4, 1},
			{32, 1, 4},
			{1, 32, 4},
			{1, 4, 32},
			{4, 1, 32},
			{4, 32, 1},
			{subgroupSize, 1, 1},
			{1, subgroupSize, 1},
			{1, 1, subgroupSize},
			{3, 5, 7},
			{128, 1, 1},
			{1, 128, 1},
			{1, 1, 64},
			{localSizeX, localSizeY, localSizeZ},
			{1, 1, 1} // Isn't used, just here to make double buffering checks easier
		};

		struct internalDataStruct internalData =
		{
			&context,
			caseDef,
			subgroupSize,
		};

		return subgroups::makeComputeTestRequiredSubgroupSize(context, VK_FORMAT_R32_UINT, DE_NULL, 0, &internalData, checkCompute,
															  caseDef.pipelineShaderStageCreateFlags, numWorkgroups, DE_FALSE, subgroupSize,
															  localSizesToTest, localSizesToTestCount);
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

		struct internalDataStruct internalData =
		{
			&context,
			caseDef,
			0u,
		};

		return subgroups::allStagesRequiredSubgroupSize(context, VK_FORMAT_R32_UINT, DE_NULL, 0, &internalData, checkVertexPipelineStages, stages,
														caseDef.pipelineShaderStageCreateFlags, caseDef.pipelineShaderStageCreateFlags, caseDef.pipelineShaderStageCreateFlags,
														caseDef.pipelineShaderStageCreateFlags, caseDef.pipelineShaderStageCreateFlags, DE_NULL);
	}
	return tcu::TestStatus::pass("OK");
}

tcu::TestStatus testRequireFullSubgroups (Context& context, const CaseDefinition caseDef)
{
	DE_ASSERT(VK_SHADER_STAGE_COMPUTE_BIT == caseDef.shaderStage);
	DE_ASSERT(caseDef.requiredSubgroupSizeMode == REQUIRED_SUBGROUP_SIZE_NONE);

	const deUint32 numWorkgroups[3] = {1, 1, 1};

	VkPhysicalDeviceSubgroupSizeControlPropertiesEXT subgroupSizeControlProperties;
	subgroupSizeControlProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_SIZE_CONTROL_PROPERTIES_EXT;
	subgroupSizeControlProperties.pNext = DE_NULL;

	VkPhysicalDeviceProperties2 properties;
	properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
	properties.pNext = &subgroupSizeControlProperties;

	context.getInstanceInterface().getPhysicalDeviceProperties2(context.getPhysicalDevice(), &properties);

	VkPhysicalDeviceProperties physicalDeviceProperties;
	context.getInstanceInterface().getPhysicalDeviceProperties(context.getPhysicalDevice(), &physicalDeviceProperties);

	deUint32 localSizeX, localSizeY, localSizeZ;
	// Calculate the local workgroup sizes to exercise the maximum supported by the driver
	getLocalSizes(physicalDeviceProperties, physicalDeviceProperties.limits.maxComputeWorkGroupInvocations, localSizeX, localSizeY, localSizeZ);

	const deUint32 subgroupSize = vkt::subgroups::getSubgroupSize(context);

	// For full subgroups and allow varying subgroup size, localsize X must be a multiple of maxSubgroupSize.
	// We set local size X for this test to the maximum, regardless if allow varying subgroup size is enabled or not.
	const deUint32 localSizesToTestCount = 7;
	deUint32 localSizesToTest[localSizesToTestCount][3] =
	{
		{subgroupSizeControlProperties.maxSubgroupSize, 1, 1},
		{subgroupSizeControlProperties.maxSubgroupSize, 4, 1},
		{subgroupSizeControlProperties.maxSubgroupSize, 1, 4},
		{subgroupSizeControlProperties.maxSubgroupSize * 2, 1, 2},
		{subgroupSizeControlProperties.maxSubgroupSize * 4, 1, 1},
		{localSizeX, localSizeY, localSizeZ},
		{1, 1, 1} // Isn't used, just here to make double buffering checks easier
	};

	struct internalDataStruct internalData =
	{
		&context,
		caseDef,
		subgroupSize,
	};

	return subgroups::makeComputeTestRequiredSubgroupSize(context, VK_FORMAT_R32G32B32A32_UINT, DE_NULL, 0, &internalData, checkComputeRequireFull,
														  caseDef.pipelineShaderStageCreateFlags, numWorkgroups, DE_FALSE, subgroupSize,
														  localSizesToTest, localSizesToTestCount);
}

tcu::TestStatus testRequireSubgroupSize (Context& context, const CaseDefinition caseDef)
{
	if (VK_SHADER_STAGE_COMPUTE_BIT == caseDef.shaderStage)
	{
		const deUint32 numWorkgroups[3] = {1, 1, 1};

		VkPhysicalDeviceSubgroupSizeControlPropertiesEXT subgroupSizeControlProperties;
		subgroupSizeControlProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_SIZE_CONTROL_PROPERTIES_EXT;
		subgroupSizeControlProperties.pNext = DE_NULL;

		VkPhysicalDeviceProperties2 properties2;
		properties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
		properties2.pNext = &subgroupSizeControlProperties;
		context.getInstanceInterface().getPhysicalDeviceProperties2(context.getPhysicalDevice(), &properties2);

		VkPhysicalDeviceProperties physicalDeviceProperties;
		context.getInstanceInterface().getPhysicalDeviceProperties(context.getPhysicalDevice(), &physicalDeviceProperties);

		deUint32 requiredSubgroupSize = getRequiredSubgroupSizeFromMode(context, caseDef, subgroupSizeControlProperties);

		const deUint64 maxSubgroupLimitSize = (deUint64)requiredSubgroupSize * subgroupSizeControlProperties.maxComputeWorkgroupSubgroups;
		const deUint32 maxTotalLocalSize = (deUint32)std::min<deUint64>(maxSubgroupLimitSize, physicalDeviceProperties.limits.maxComputeWorkGroupInvocations);
		deUint32 localSizeX, localSizeY, localSizeZ;
		getLocalSizes(physicalDeviceProperties, maxTotalLocalSize, localSizeX, localSizeY, localSizeZ);

		deUint32 localSizesToTest[5][3] =
		{
			{localSizeX, localSizeY, localSizeZ},
			{requiredSubgroupSize, 1, 1},
			{1, requiredSubgroupSize, 1},
			{1, 1, requiredSubgroupSize},
			{1, 1, 1} // Isn't used, just here to make double buffering checks easier
		};

		deUint32 localSizesToTestCount = 5;
		if (caseDef.pipelineShaderStageCreateFlags & VK_PIPELINE_SHADER_STAGE_CREATE_REQUIRE_FULL_SUBGROUPS_BIT_EXT)
			localSizesToTestCount = 3;

		struct internalDataStruct internalData =
		{
			&context,
			caseDef,
			requiredSubgroupSize,
		};

		// Depending on the flag we need to run one verification function or another.
		return subgroups::makeComputeTestRequiredSubgroupSize(context, VK_FORMAT_R32G32B32A32_UINT, DE_NULL, 0, &internalData,
															  caseDef.pipelineShaderStageCreateFlags == VK_PIPELINE_SHADER_STAGE_CREATE_REQUIRE_FULL_SUBGROUPS_BIT_EXT ? checkComputeRequireFull : checkCompute,
															  caseDef.pipelineShaderStageCreateFlags, numWorkgroups, DE_TRUE, requiredSubgroupSize,
															  localSizesToTest, localSizesToTestCount);
	}
	else
	{
		VkPhysicalDeviceSubgroupSizeControlPropertiesEXT subgroupSizeControlProperties;
		subgroupSizeControlProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_SIZE_CONTROL_PROPERTIES_EXT;
		subgroupSizeControlProperties.pNext = DE_NULL;

		VkPhysicalDeviceSubgroupProperties subgroupProperties;
		subgroupProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES;
		subgroupProperties.pNext = &subgroupSizeControlProperties;

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

		deUint32 requiredSubgroupSize = getRequiredSubgroupSizeFromMode(context, caseDef, subgroupSizeControlProperties);
		const deUint32 requiredSubgroupSizes[5] = { requiredSubgroupSize, requiredSubgroupSize, requiredSubgroupSize, requiredSubgroupSize, requiredSubgroupSize};
		struct internalDataStruct internalData =
		{
			&context,
			caseDef,
			requiredSubgroupSize,
		};
		return subgroups::allStagesRequiredSubgroupSize(context, VK_FORMAT_R32_UINT, DE_NULL, 0, &internalData, checkVertexPipelineStages, stages,
														caseDef.pipelineShaderStageCreateFlags, caseDef.pipelineShaderStageCreateFlags, caseDef.pipelineShaderStageCreateFlags,
														caseDef.pipelineShaderStageCreateFlags, caseDef.pipelineShaderStageCreateFlags, requiredSubgroupSizes);
	}
	return tcu::TestStatus::pass("OK");
}

tcu::TestStatus noSSBOtestRequireSubgroupSize (Context& context, const CaseDefinition caseDef)
{
	VkPhysicalDeviceSubgroupSizeControlPropertiesEXT subgroupSizeControlProperties;
	subgroupSizeControlProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_SIZE_CONTROL_PROPERTIES_EXT;
	subgroupSizeControlProperties.pNext = DE_NULL;

	VkPhysicalDeviceSubgroupProperties subgroupProperties;
	subgroupProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES;
	subgroupProperties.pNext = &subgroupSizeControlProperties;

	VkPhysicalDeviceProperties2 properties;
	properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
	properties.pNext = &subgroupProperties;

	context.getInstanceInterface().getPhysicalDeviceProperties2(context.getPhysicalDevice(), &properties);

	VkShaderStageFlagBits stages = (VkShaderStageFlagBits)(caseDef.shaderStage  & subgroupProperties.supportedStages);
	if ((VkShaderStageFlagBits)0u == stages)
		TCU_THROW(NotSupportedError, "Subgroup operations are not supported for any graphic shader");

	deUint32 requiredSubgroupSize = getRequiredSubgroupSizeFromMode(context, caseDef, subgroupSizeControlProperties);
	struct internalDataStruct internalData =
	{
		&context,
		caseDef,
		requiredSubgroupSize,
	};

	if (VK_SHADER_STAGE_VERTEX_BIT == caseDef.shaderStage)
		return subgroups::makeVertexFrameBufferTestRequiredSubgroupSize(context, VK_FORMAT_R32_UINT, DE_NULL, 0, &internalData, checkVertexPipelineStages, caseDef.pipelineShaderStageCreateFlags, requiredSubgroupSize);
	else if (VK_SHADER_STAGE_GEOMETRY_BIT == caseDef.shaderStage)
		return subgroups::makeGeometryFrameBufferTestRequiredSubgroupSize(context, VK_FORMAT_R32_UINT, DE_NULL, 0, &internalData, checkVertexPipelineStages, caseDef.pipelineShaderStageCreateFlags, requiredSubgroupSize);
	else if ((VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT | VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT) & caseDef.shaderStage)
		return subgroups::makeTessellationEvaluationFrameBufferTestRequiredSubgroupSize(context, VK_FORMAT_R32_UINT, DE_NULL, 0, &internalData, checkVertexPipelineStages, caseDef.shaderStage, caseDef.pipelineShaderStageCreateFlags, requiredSubgroupSize);
	else if (VK_SHADER_STAGE_FRAGMENT_BIT & caseDef.shaderStage)
		return subgroups::makeFragmentFrameBufferTestRequiredSubgroupSize(context, VK_FORMAT_R32_UINT, DE_NULL, 0, &internalData, checkFragmentPipelineStages, caseDef.pipelineShaderStageCreateFlags, requiredSubgroupSize);
	else
		TCU_THROW(InternalError, "Unhandled shader stage");
}

tcu::TestStatus testSanitySubgroupSizeProperties (Context& context)
{
	VkPhysicalDeviceSubgroupSizeControlPropertiesEXT subgroupSizeControlProperties;
	subgroupSizeControlProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_SIZE_CONTROL_PROPERTIES_EXT;
	subgroupSizeControlProperties.pNext = DE_NULL;

	VkPhysicalDeviceSubgroupProperties subgroupProperties;
	subgroupProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES;
	subgroupProperties.pNext = &subgroupSizeControlProperties;

	VkPhysicalDeviceProperties2 properties;
	properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
	properties.pNext = &subgroupProperties;

	context.getInstanceInterface().getPhysicalDeviceProperties2(context.getPhysicalDevice(), &properties);

	if (subgroupProperties.subgroupSize > subgroupSizeControlProperties.maxSubgroupSize ||
		subgroupProperties.subgroupSize < subgroupSizeControlProperties.minSubgroupSize)
	{
		std::ostringstream error;
		error << "subgroupSize (" << subgroupProperties.subgroupSize << ") is not between maxSubgroupSize (";
		error << subgroupSizeControlProperties.maxSubgroupSize << ") and minSubgroupSize (";
		error << subgroupSizeControlProperties.minSubgroupSize << ")";
		return tcu::TestStatus::fail(error.str().c_str());
	}

	return tcu::TestStatus::pass("OK");
}
}

namespace vkt
{
namespace subgroups
{
tcu::TestCaseGroup* createSubgroupsSizeControlTests (tcu::TestContext& testCtx)
{
	const VkShaderStageFlags stages[] =
	{
		VK_SHADER_STAGE_VERTEX_BIT,
		VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,
		VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,
		VK_SHADER_STAGE_GEOMETRY_BIT,
		VK_SHADER_STAGE_FRAGMENT_BIT,
	};

	de::MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(
		testCtx, "size_control", "VK_EXT_subgroup_size_control tests"));

	de::MovePtr<tcu::TestCaseGroup> framebufferGroup(new tcu::TestCaseGroup(
		testCtx, "framebuffer", "Subgroup size control category tests: framebuffer"));

	de::MovePtr<tcu::TestCaseGroup> computeGroup(new tcu::TestCaseGroup(
		testCtx, "compute", "Subgroup size control category tests: compute"));

	de::MovePtr<tcu::TestCaseGroup> graphicsGroup(new tcu::TestCaseGroup(
		testCtx, "graphics", "Subgroup size control category tests: graphics"));

	de::MovePtr<tcu::TestCaseGroup> genericGroup(new tcu::TestCaseGroup(
		testCtx, "generic", "Subgroup size control category tests: generic"));

	// Test sanity of the subgroup size properties.
	{
		addFunctionCase(genericGroup.get(), "subgroup_size_properties", "", supportedCheck, testSanitySubgroupSizeProperties);
	}

	// Allow varying subgroup case.
	{
		const CaseDefinition caseDefCompute = {VK_PIPELINE_SHADER_STAGE_CREATE_ALLOW_VARYING_SUBGROUP_SIZE_BIT_EXT, VK_SHADER_STAGE_COMPUTE_BIT, DE_FALSE, REQUIRED_SUBGROUP_SIZE_NONE};
		addFunctionCaseWithPrograms(computeGroup.get(), "allow_varying_subgroup_size", "", supportedCheckFeatures, initPrograms, test, caseDefCompute);
		const CaseDefinition caseDefAllGraphics = {VK_PIPELINE_SHADER_STAGE_CREATE_ALLOW_VARYING_SUBGROUP_SIZE_BIT_EXT, VK_SHADER_STAGE_ALL_GRAPHICS, DE_FALSE, REQUIRED_SUBGROUP_SIZE_NONE};
		addFunctionCaseWithPrograms(graphicsGroup.get(), "allow_varying_subgroup_size", "", supportedCheckFeaturesShader, initPrograms, test, caseDefAllGraphics);

		for (int stageIndex = 0; stageIndex < DE_LENGTH_OF_ARRAY(stages); ++stageIndex)
		{
			const CaseDefinition caseDefStage = {VK_PIPELINE_SHADER_STAGE_CREATE_ALLOW_VARYING_SUBGROUP_SIZE_BIT_EXT, stages[stageIndex], DE_FALSE, REQUIRED_SUBGROUP_SIZE_NONE};
			addFunctionCaseWithPrograms(framebufferGroup.get(),  getShaderStageName(caseDefStage.shaderStage) + "_allow_varying_subgroup_size", "", supportedCheckFeaturesShader, initFrameBufferPrograms, noSSBOtest, caseDefStage);
		}
	}

	// Require full subgroups case (only compute shaders).
	{
		const CaseDefinition caseDef = {VK_PIPELINE_SHADER_STAGE_CREATE_REQUIRE_FULL_SUBGROUPS_BIT_EXT, VK_SHADER_STAGE_COMPUTE_BIT, DE_TRUE, REQUIRED_SUBGROUP_SIZE_NONE};
		addFunctionCaseWithPrograms(computeGroup.get(), "require_full_subgroups", "", supportedCheckFeatures, initProgramsRequireFull, testRequireFullSubgroups, caseDef);
	}

	// Require full subgroups together with allow varying subgroup (only compute shaders).
	{
		deUint32 flags = VK_PIPELINE_SHADER_STAGE_CREATE_REQUIRE_FULL_SUBGROUPS_BIT_EXT | VK_PIPELINE_SHADER_STAGE_CREATE_ALLOW_VARYING_SUBGROUP_SIZE_BIT_EXT;
		const CaseDefinition caseDef = {flags, VK_SHADER_STAGE_COMPUTE_BIT, DE_TRUE, REQUIRED_SUBGROUP_SIZE_NONE};
		addFunctionCaseWithPrograms(computeGroup.get(), "require_full_subgroups_allow_varying_subgroup_size", "", supportedCheckFeatures, initProgramsRequireFull, testRequireFullSubgroups, caseDef);
	}

	// Tests to check setting a required subgroup size value.
	{
		const CaseDefinition caseDefAllGraphicsMax = {0u, VK_SHADER_STAGE_ALL_GRAPHICS, DE_FALSE, REQUIRED_SUBGROUP_SIZE_MAX};
		addFunctionCaseWithPrograms(graphicsGroup.get(), "required_subgroup_size_max", "", supportedCheckFeaturesShader, initPrograms, testRequireSubgroupSize, caseDefAllGraphicsMax);
		const CaseDefinition caseDefComputeMax = {0u, VK_SHADER_STAGE_COMPUTE_BIT, DE_FALSE, REQUIRED_SUBGROUP_SIZE_MAX};
		addFunctionCaseWithPrograms(computeGroup.get(), "required_subgroup_size_max", "", supportedCheckFeatures, initPrograms, testRequireSubgroupSize, caseDefComputeMax);

		const CaseDefinition caseDefAllGraphicsMin = {0u, VK_SHADER_STAGE_ALL_GRAPHICS, DE_FALSE, REQUIRED_SUBGROUP_SIZE_MIN};
		addFunctionCaseWithPrograms(graphicsGroup.get(), "required_subgroup_size_min", "", supportedCheckFeaturesShader, initPrograms, testRequireSubgroupSize, caseDefAllGraphicsMin);
		const CaseDefinition caseDefComputeMin = {0u, VK_SHADER_STAGE_COMPUTE_BIT, DE_FALSE, REQUIRED_SUBGROUP_SIZE_MIN};
		addFunctionCaseWithPrograms(computeGroup.get(), "required_subgroup_size_min", "", supportedCheckFeatures, initPrograms, testRequireSubgroupSize, caseDefComputeMin);
		for (int stageIndex = 0; stageIndex < DE_LENGTH_OF_ARRAY(stages); ++stageIndex)
		{
			const CaseDefinition caseDefStageMax = {0u, stages[stageIndex], DE_FALSE, REQUIRED_SUBGROUP_SIZE_MAX};
			addFunctionCaseWithPrograms(framebufferGroup.get(),  getShaderStageName(caseDefStageMax.shaderStage) + "_required_subgroup_size_max", "", supportedCheckFeaturesShader, initFrameBufferPrograms, noSSBOtestRequireSubgroupSize, caseDefStageMax);
			const CaseDefinition caseDefStageMin = {0u, stages[stageIndex], DE_FALSE, REQUIRED_SUBGROUP_SIZE_MIN};
			addFunctionCaseWithPrograms(framebufferGroup.get(),  getShaderStageName(caseDefStageMin.shaderStage) + "_required_subgroup_size_min", "", supportedCheckFeaturesShader, initFrameBufferPrograms, noSSBOtestRequireSubgroupSize, caseDefStageMin);
		}
	}

	// Tests to check setting a required subgroup size value, together with require full subgroups (only compute shaders).
	{
		deUint32 flags = VK_PIPELINE_SHADER_STAGE_CREATE_REQUIRE_FULL_SUBGROUPS_BIT_EXT;
		const CaseDefinition caseDefMax = {flags, VK_SHADER_STAGE_COMPUTE_BIT, DE_TRUE, REQUIRED_SUBGROUP_SIZE_MAX};
		addFunctionCaseWithPrograms(computeGroup.get(), "required_subgroup_size_max_require_full_subgroups", "", supportedCheckFeatures, initProgramsRequireFull, testRequireSubgroupSize, caseDefMax);
		const CaseDefinition caseDefMin = {flags, VK_SHADER_STAGE_COMPUTE_BIT, DE_TRUE, REQUIRED_SUBGROUP_SIZE_MIN};
		addFunctionCaseWithPrograms(computeGroup.get(), "required_subgroup_size_min_require_full_subgroups", "", supportedCheckFeatures, initProgramsRequireFull, testRequireSubgroupSize, caseDefMin);
	}

	group->addChild(genericGroup.release());
	group->addChild(graphicsGroup.release());
	group->addChild(computeGroup.release());
	group->addChild(framebufferGroup.release());

	return group.release();
}

} // subgroups
} // vkt
