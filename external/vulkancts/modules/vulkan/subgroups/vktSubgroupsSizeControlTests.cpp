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
#include <algorithm>

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
	deUint32			pipelineShaderStageCreateFlags;
	VkShaderStageFlags	shaderStage;
	deBool				requiresBallot;
	deUint32			requiredSubgroupSizeMode;
	de::SharedPtr<bool>	geometryPointSizeSupported;
	SpirvVersion		spirvVersion;

	bool hasFullSubgroupsFlag (void) const
	{
		return ((pipelineShaderStageCreateFlags & VK_PIPELINE_SHADER_STAGE_CREATE_REQUIRE_FULL_SUBGROUPS_BIT_EXT) != 0u);
	}

	bool shaderUsesFullSubgroups (void) const
	{
		return (hasFullSubgroupsFlag() || (spirvVersion >= SPIRV_VERSION_1_6));
	}
};

struct TestParams
{
	deBool	useSpirv16;
	deBool	flagsEnabled;
	string	postfix;
};

struct internalDataStruct
{
	const Context*			context;
	struct CaseDefinition	caseDef;
	const deUint32			requiredSubgroupSize;
	const bool				isRequiredSubgroupSize;	// Indicates if the test uses VkPipelineShaderStageRequiredSubgroupSizeCreateInfo.
};

inline deBool makeDeBool (bool value)
{
	return (value ? DE_TRUE : DE_FALSE);
}

UVec3	getLocalSizes (const uint32_t maxWorkGroupSize[3],
					   uint32_t	maxWorkGroupInvocations,
					   uint32_t	numWorkGroupInvocations)
{
	DE_ASSERT(numWorkGroupInvocations <= maxWorkGroupInvocations);
	DE_UNREF(maxWorkGroupInvocations); // For release builds.

	const deUint32 localSizeX = de::gcd(numWorkGroupInvocations, maxWorkGroupSize[0]);
	const deUint32 localSizeY = de::gcd(std::max(numWorkGroupInvocations / localSizeX, 1u), maxWorkGroupSize[1]);
	const deUint32 localSizeZ = std::max(numWorkGroupInvocations / (localSizeX * localSizeY), 1u);

	return UVec3(localSizeX, localSizeY, localSizeZ);
}

deUint32 getRequiredSubgroupSizeFromMode (Context&													context,
										  const CaseDefinition&										caseDef,
#ifndef CTS_USES_VULKANSC
										  const VkPhysicalDeviceSubgroupSizeControlProperties&		subgroupSizeControlProperties)
#else
										  const VkPhysicalDeviceSubgroupSizeControlPropertiesEXT&		subgroupSizeControlProperties)
#endif // CTS_USES_VULKANSC
{
	switch (caseDef.requiredSubgroupSizeMode)
	{
		case REQUIRED_SUBGROUP_SIZE_MAX:	return subgroupSizeControlProperties.maxSubgroupSize;
		case REQUIRED_SUBGROUP_SIZE_MIN:	return subgroupSizeControlProperties.minSubgroupSize;
		case REQUIRED_SUBGROUP_SIZE_NONE:	return subgroups::getSubgroupSize(context);
		default:							TCU_THROW(NotSupportedError, "Unsupported Subgroup size");
	}
}

static bool checkVertexPipelineStages (const void*			internalData,
									   vector<const void*>	datas,
									   deUint32				width,
									   deUint32)
{
	const struct internalDataStruct*						checkInternalData				= reinterpret_cast<const struct internalDataStruct *>(internalData);
	const Context*											context							= checkInternalData->context;
#ifndef CTS_USES_VULKANSC
	const VkPhysicalDeviceSubgroupSizeControlProperties&	subgroupSizeControlProperties	= context->getSubgroupSizeControlProperties();
#else
	const VkPhysicalDeviceSubgroupSizeControlPropertiesEXT&	subgroupSizeControlProperties	= context->getSubgroupSizeControlPropertiesEXT();
#endif // CTS_USES_VULKANSC

	TestLog&												log								= context->getTestContext().getLog();
	const deUint32*											data							= reinterpret_cast<const deUint32*>(datas[0]);

	for (deUint32 i = 0; i < width; i++)
	{
		if (data[i] > subgroupSizeControlProperties.maxSubgroupSize ||
			data[i] < subgroupSizeControlProperties.minSubgroupSize)
		{
			log << TestLog::Message << "gl_SubgroupSize (" << data[i] << ") value is outside limits (" << subgroupSizeControlProperties.minSubgroupSize << ", " << subgroupSizeControlProperties.maxSubgroupSize << ")" << TestLog::EndMessage;

			return DE_FALSE;
		}

		if (checkInternalData->isRequiredSubgroupSize && data[i] != checkInternalData->requiredSubgroupSize)
		{
			log << TestLog::Message << "gl_SubgroupSize (" << data[i] << ") is not equal to the required subgroup size value (" << checkInternalData->requiredSubgroupSize << ")" << TestLog::EndMessage;

			return DE_FALSE;
		}
	}

	return DE_TRUE;
}

static bool checkFragmentPipelineStages (const void*			internalData,
										 vector<const void*>	datas,
										 deUint32				width,
										 deUint32				height,
										 deUint32)
{
	const struct internalDataStruct*						checkInternalData				= reinterpret_cast<const struct internalDataStruct *>(internalData);
	const Context*											context							= checkInternalData->context;
#ifndef CTS_USES_VULKANSC
	const VkPhysicalDeviceSubgroupSizeControlProperties&	subgroupSizeControlProperties	= context->getSubgroupSizeControlProperties();
#else
	const VkPhysicalDeviceSubgroupSizeControlPropertiesEXT&	subgroupSizeControlProperties	= context->getSubgroupSizeControlPropertiesEXT();
#endif // CTS_USES_VULKANSC
	TestLog&												log								= context->getTestContext().getLog();
	const deUint32*											data							= reinterpret_cast<const deUint32*>(datas[0]);

	for (deUint32 x = 0u; x < width; ++x)
	{
		for (deUint32 y = 0u; y < height; ++y)
		{
			const deUint32 ndx = (x * height + y);

			if (data[ndx] > subgroupSizeControlProperties.maxSubgroupSize ||
				data[ndx] < subgroupSizeControlProperties.minSubgroupSize)
			{
				log << TestLog::Message << "gl_SubgroupSize (" << data[ndx] << ") value is outside limits (" << subgroupSizeControlProperties.minSubgroupSize << ", " << subgroupSizeControlProperties.maxSubgroupSize << ")" << TestLog::EndMessage;

				return DE_FALSE;
			}

			if (checkInternalData->isRequiredSubgroupSize && data[ndx] != checkInternalData->requiredSubgroupSize)
			{
				log << TestLog::Message << "gl_SubgroupSize (" << data[ndx] << ") is not equal to the required subgroup size value (" << checkInternalData->requiredSubgroupSize << ")" << TestLog::EndMessage;

				return DE_FALSE;
			}
		}
	}
	return true;
}

static bool checkCompute (const void*			internalData,
						  vector<const void*>	datas,
						  const deUint32		numWorkgroups[3],
						  const deUint32		localSize[3],
						  deUint32)
{
	const struct internalDataStruct*						checkInternalData				= reinterpret_cast<const struct internalDataStruct *>(internalData);
	const Context*											context							= checkInternalData->context;
#ifndef CTS_USES_VULKANSC
	const VkPhysicalDeviceSubgroupSizeControlProperties&	subgroupSizeControlProperties	= context->getSubgroupSizeControlProperties();
#else
	const VkPhysicalDeviceSubgroupSizeControlPropertiesEXT&	subgroupSizeControlProperties	= context->getSubgroupSizeControlPropertiesEXT();
#endif // CTS_USES_VULKANSC
	TestLog&												log								= context->getTestContext().getLog();
	const deUint32											globalSizeX						= numWorkgroups[0] * localSize[0];
	const deUint32											globalSizeY						= numWorkgroups[1] * localSize[1];
	const deUint32											globalSizeZ						= numWorkgroups[2] * localSize[2];
	const deUint32											width							= globalSizeX * globalSizeY * globalSizeZ;
	const deUint32*											data							= reinterpret_cast<const deUint32*>(datas[0]);

	for (deUint32 i = 0; i < width; i++)
	{
		if (data[i] > subgroupSizeControlProperties.maxSubgroupSize ||
			data[i] < subgroupSizeControlProperties.minSubgroupSize)
		{
			log << TestLog::Message << "[" << localSize[0] << ", " << localSize[1] << ", " << localSize[2] << "] "
				<< "gl_SubgroupSize (" << data[i] << ") value is outside limits (" << subgroupSizeControlProperties.minSubgroupSize << ", " << subgroupSizeControlProperties.maxSubgroupSize << ")" << TestLog::EndMessage;

			return DE_FALSE;
		}

		if (checkInternalData->isRequiredSubgroupSize && data[i] != checkInternalData->requiredSubgroupSize)
		{
			log << TestLog::Message << "[" << localSize[0] << ", " << localSize[1] << ", " << localSize[2] << "] "
				<< "gl_SubgroupSize (" << data[i] << ") is not equal to the required subgroup size value (" << checkInternalData->requiredSubgroupSize << ")" << TestLog::EndMessage;

			return DE_FALSE;
		}
	}

	return DE_TRUE;
}

static bool checkComputeRequireFull (const void*			internalData,
									 vector<const void*>	datas,
									 const deUint32			numWorkgroups[3],
									 const deUint32			localSize[3],
									 deUint32)
{
	const struct internalDataStruct*						checkInternalData				= reinterpret_cast<const struct internalDataStruct *>(internalData);
	const Context*											context							= checkInternalData->context;
#ifndef CTS_USES_VULKANSC
	const VkPhysicalDeviceSubgroupSizeControlProperties&	subgroupSizeControlProperties	= context->getSubgroupSizeControlProperties();
#else
	const VkPhysicalDeviceSubgroupSizeControlPropertiesEXT&	subgroupSizeControlProperties	= context->getSubgroupSizeControlPropertiesEXT();
#endif // CTS_USES_VULKANSC
	TestLog&												log								= context->getTestContext().getLog();
	const deUint32											globalSizeX						= numWorkgroups[0] * localSize[0];
	const deUint32											globalSizeY						= numWorkgroups[1] * localSize[1];
	const deUint32											globalSizeZ						= numWorkgroups[2] * localSize[2];
	const deUint32											width							= globalSizeX * globalSizeY * globalSizeZ;
	const UVec4*											data							= reinterpret_cast<const UVec4*>(datas[0]);
	const deUint32											numSubgroups					= (localSize[0] * localSize[1] * localSize[2]) / checkInternalData->requiredSubgroupSize;
	const bool												exactSubgroupSize				= (checkInternalData->caseDef.shaderUsesFullSubgroups() && checkInternalData->isRequiredSubgroupSize);

	for (deUint32 i = 0; i < width; i++)
	{
		if (data[i].x() > subgroupSizeControlProperties.maxSubgroupSize ||
			data[i].x() < subgroupSizeControlProperties.minSubgroupSize)
		{
			log << TestLog::Message << "[" << localSize[0] << ", " << localSize[1] << ", " << localSize[2] << "] "
				<< "gl_SubgroupSize value ( " << data[i].x() << ") is outside limits [" << subgroupSizeControlProperties.minSubgroupSize << ", " << subgroupSizeControlProperties.maxSubgroupSize << "]" << TestLog::EndMessage;
			return DE_FALSE;
		}

		if (data[i].x() != data[i].y())
		{
			log << TestLog::Message << "[" << localSize[0] << ", " << localSize[1] << ", " << localSize[2] << "] "
				<< "gl_SubgroupSize ( " << data[i].x() << ") does not match the active number of subgroup invocations (" << data[i].y() << ")" << TestLog::EndMessage;
			return DE_FALSE;
		}

		if (exactSubgroupSize && data[i].x() != checkInternalData->requiredSubgroupSize)
		{
			log << TestLog::Message << "[" << localSize[0] << ", " << localSize[1] << ", " << localSize[2] << "] "
				<< "expected subgroupSize (" << checkInternalData->requiredSubgroupSize << ") doesn't match gl_SubgroupSize ( " << data[i].x() << ")" << TestLog::EndMessage;
			return DE_FALSE;
		}

		if (exactSubgroupSize && data[i].z() != numSubgroups)
		{
			log << TestLog::Message << "[" << localSize[0] << ", " << localSize[1] << ", " << localSize[2] << "] "
				<< "expected number of subgroups dispatched (" << numSubgroups << ") doesn't match gl_NumSubgroups (" << data[i].z() << ")" << TestLog::EndMessage;
			return DE_FALSE;
		}
	}

	return DE_TRUE;
}

void initFrameBufferPrograms (SourceCollections& programCollection, CaseDefinition caseDef)
{
	const ShaderBuildOptions	buildOptions	(programCollection.usedVulkanVersion, caseDef.spirvVersion, 0u);

	if (VK_SHADER_STAGE_FRAGMENT_BIT != caseDef.shaderStage)
		subgroups::setFragmentShaderFrameBuffer(programCollection);

	if (VK_SHADER_STAGE_VERTEX_BIT != caseDef.shaderStage && VK_SHADER_STAGE_FRAGMENT_BIT != caseDef.shaderStage)
		subgroups::setVertexShaderFrameBuffer(programCollection);

	string bdyStr = "uint tempResult = gl_SubgroupSize;\n";

	if (VK_SHADER_STAGE_VERTEX_BIT == caseDef.shaderStage)
	{
		ostringstream vertex;

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

		programCollection.glslSources.add("vert") << glu::VertexSource(vertex.str()) << buildOptions;
	}
	else if (VK_SHADER_STAGE_GEOMETRY_BIT == caseDef.shaderStage)
	{
		ostringstream geometry;

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

		programCollection.glslSources.add("geometry") << glu::GeometrySource(geometry.str()) << buildOptions;
	}
	else if (VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT == caseDef.shaderStage)
	{
		ostringstream controlSource;

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

		programCollection.glslSources.add("tesc") << glu::TessellationControlSource(controlSource.str()) << buildOptions;
		subgroups::setTesEvalShaderFrameBuffer(programCollection);
	}
	else if (VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT == caseDef.shaderStage)
	{
		ostringstream evaluationSource;
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
		programCollection.glslSources.add("tese") << glu::TessellationEvaluationSource(evaluationSource.str()) << buildOptions;
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

		ostringstream fragmentSource;

		fragmentSource << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450)<<"\n"
					   << "precision highp int;\n"
						<< "#extension GL_KHR_shader_subgroup_basic: enable\n"
					   << "layout(location = 0) out uint out_color;\n"
					   << "void main()\n"
					   << "{\n"
					   << bdyStr
					   << "	 out_color = tempResult;\n"
					   << "}\n";

		programCollection.glslSources.add("fragment") << glu::FragmentSource(fragmentSource.str()) << buildOptions;
	}
	else
	{
		DE_FATAL("Unsupported shader stage");
	}
}

string getExtHeader (const CaseDefinition&)
{
	return "#extension GL_KHR_shader_subgroup_basic: enable\n";
}

vector<string> getPerStageHeadDeclarations (const CaseDefinition& caseDef)
{
	const deUint32	stageCount	= subgroups::getStagesCount(caseDef.shaderStage);
	const bool		fragment	= (caseDef.shaderStage & VK_SHADER_STAGE_FRAGMENT_BIT) != 0;
	vector<string>	result		(stageCount, string());

	if (fragment)
		result.reserve(result.size() + 1);

	for (size_t i = 0; i < result.size(); ++i)
	{
		result[i] =
			"layout(set = 0, binding = " + de::toString(i) + ", std430) buffer Buffer1\n"
			"{\n"
			"  uint result[];\n"
			"};\n";
	}

	if (fragment)
	{
		const string	fragPart	=
			"layout(location = 0) out uint result;\n";

		result.push_back(fragPart);
	}

	return result;
}

string getTestSource (const CaseDefinition&)
{
	return
		"  uint tempResult = gl_SubgroupSize;\n"
		"  tempRes = tempResult;\n";
}

void initPrograms (SourceCollections& programCollection, CaseDefinition caseDef)
{
	ShaderBuildOptions		buildOptions		(programCollection.usedVulkanVersion, caseDef.spirvVersion, 0u, (caseDef.spirvVersion == vk::SPIRV_VERSION_1_4));
	const string			extHeader			= getExtHeader(caseDef);
	const string			testSrc				= getTestSource(caseDef);
	const vector<string>	headDeclarations	= getPerStageHeadDeclarations(caseDef);

	subgroups::initStdPrograms(programCollection, buildOptions, caseDef.shaderStage, VK_FORMAT_R32_UINT, *caseDef.geometryPointSizeSupported, extHeader, testSrc, "", headDeclarations);
}

void initProgramsRequireFull (SourceCollections& programCollection, CaseDefinition caseDef)
{
	if (VK_SHADER_STAGE_COMPUTE_BIT != caseDef.shaderStage)
		DE_FATAL("Unsupported shader stage");

	ostringstream src;

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
		<< "   result[offset].x = gl_SubgroupSize;\n" // save the subgroup size value
		<< "   uint numActive = subgroupBallotBitCount(subgroupBallot(true));\n"
		<< "   result[offset].y = numActive;\n" // save the number of active subgroup invocations
		<< "   result[offset].z = gl_NumSubgroups;\n" // save the number of subgroups dispatched.
		<< "}\n";

	programCollection.glslSources.add("comp") << glu::ComputeSource(src.str()) << ShaderBuildOptions(programCollection.usedVulkanVersion, caseDef.spirvVersion, 0u);
}

void supportedCheck (Context& context)
{
	if (!subgroups::isSubgroupSupported(context))
		TCU_THROW(NotSupportedError, "Subgroup operations are not supported");

	context.requireDeviceFunctionality("VK_EXT_subgroup_size_control");
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
		const VkPhysicalDeviceFeatures&		features	= context.getDeviceFeatures();

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
#ifndef CTS_USES_VULKANSC
		const VkPhysicalDeviceSubgroupSizeControlFeatures&		subgroupSizeControlFeatures		= context.getSubgroupSizeControlFeatures();
#else
		const VkPhysicalDeviceSubgroupSizeControlFeaturesEXT&		subgroupSizeControlFeatures	= context.getSubgroupSizeControlFeaturesEXT();
#endif // CTS_USES_VULKANSC

		if (subgroupSizeControlFeatures.subgroupSizeControl == DE_FALSE)
			TCU_THROW(NotSupportedError, "Device does not support varying subgroup sizes nor required subgroup size");

		if (caseDef.requiredSubgroupSizeMode != REQUIRED_SUBGROUP_SIZE_NONE)
		{
#ifndef CTS_USES_VULKANSC
			const VkPhysicalDeviceSubgroupSizeControlProperties&	subgroupSizeControlProperties	= context.getSubgroupSizeControlProperties();
#else
			const VkPhysicalDeviceSubgroupSizeControlPropertiesEXT&	subgroupSizeControlProperties	= context.getSubgroupSizeControlPropertiesEXT();
#endif // CTS_USES_VULKANSC

			if ((subgroupSizeControlProperties.requiredSubgroupSizeStages & caseDef.shaderStage) != caseDef.shaderStage)
				TCU_THROW(NotSupportedError, "Device does not support setting required subgroup size for the stages selected");
		}
	}

	if (caseDef.hasFullSubgroupsFlag())
	{
#ifndef CTS_USES_VULKANSC
		const VkPhysicalDeviceSubgroupSizeControlFeatures&	subgroupSizeControlFeatures		= context.getSubgroupSizeControlFeatures();
#else
		const VkPhysicalDeviceSubgroupSizeControlFeaturesEXT&	subgroupSizeControlFeatures = context.getSubgroupSizeControlFeaturesEXT();
#endif // CTS_USES_VULKANSC

		if (subgroupSizeControlFeatures.computeFullSubgroups == DE_FALSE)
			TCU_THROW(NotSupportedError, "Device does not support full subgroups in compute shaders");
	}

	*caseDef.geometryPointSizeSupported = subgroups::isTessellationAndGeometryPointSizeSupported(context);

#ifndef CTS_USES_VULKANSC
	if (isAllRayTracingStages(caseDef.shaderStage))
	{
		context.requireDeviceFunctionality("VK_KHR_ray_tracing_pipeline");
	}
	else if (isAllMeshShadingStages(caseDef.shaderStage))
	{
		context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_VERTEX_PIPELINE_STORES_AND_ATOMICS);
		context.requireDeviceFunctionality("VK_EXT_mesh_shader");

		if ((caseDef.shaderStage & VK_SHADER_STAGE_TASK_BIT_EXT) != 0u)
		{
			const auto& features = context.getMeshShaderFeaturesEXT();
			if (!features.taskShader)
				TCU_THROW(NotSupportedError, "Task shaders not supported");
		}
	}
#endif // CTS_USES_VULKANSC

	if (caseDef.spirvVersion > vk::getMaxSpirvVersionForVulkan(context.getUsedApiVersion()))
		TCU_THROW(NotSupportedError, "Shader requires SPIR-V version higher than available");
}

void supportedCheckFeaturesShader (Context& context, CaseDefinition caseDef)
{
	supportedCheckFeatures(context, caseDef);

	subgroups::supportedCheckShader(context, caseDef.shaderStage);
}

TestStatus noSSBOtest (Context& context, const CaseDefinition caseDef)
{
	const VkFormat					format			= VK_FORMAT_R32_UINT;
	const deUint32&					flags			= caseDef.pipelineShaderStageCreateFlags;
	const struct internalDataStruct	internalData	=
	{
		&context,
		caseDef,
		0u,
		false,
	};

	switch (caseDef.shaderStage)
	{
		case VK_SHADER_STAGE_VERTEX_BIT:					return subgroups::makeVertexFrameBufferTestRequiredSubgroupSize(context, format, DE_NULL, 0, &internalData, checkVertexPipelineStages, flags, 0u);
		case VK_SHADER_STAGE_GEOMETRY_BIT:					return subgroups::makeGeometryFrameBufferTestRequiredSubgroupSize(context, format, DE_NULL, 0, &internalData, checkVertexPipelineStages, flags, 0u);
		case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:		return subgroups::makeTessellationEvaluationFrameBufferTestRequiredSubgroupSize(context, format, DE_NULL, 0, &internalData, checkVertexPipelineStages, caseDef.shaderStage, flags, 0u);
		case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT:	return subgroups::makeTessellationEvaluationFrameBufferTestRequiredSubgroupSize(context, format, DE_NULL, 0, &internalData, checkVertexPipelineStages, caseDef.shaderStage, flags, 0u);
		case VK_SHADER_STAGE_FRAGMENT_BIT:					return subgroups::makeFragmentFrameBufferTestRequiredSubgroupSize(context, format, DE_NULL, 0, &internalData, checkFragmentPipelineStages, flags, 0u);
		default:											TCU_THROW(InternalError, "Unhandled shader stage");
	}
}

TestStatus test (Context& context, const CaseDefinition caseDef)
{
	if (isAllComputeStages(caseDef.shaderStage))
	{
		const deUint32	numWorkgroups[3]							= {1, 1, 1};
		const deUint32	subgroupSize								= subgroups::getSubgroupSize(context);
		const auto&		physicalDeviceProperties					= context.getDeviceProperties();
		const auto&		maxWorkGroupSize							= physicalDeviceProperties.limits.maxComputeWorkGroupSize;
		const auto&		maxInvocations								= physicalDeviceProperties.limits.maxComputeWorkGroupInvocations;
		// Calculate the local workgroup sizes to exercise the maximum supported by the driver
		const UVec3		localSize									= getLocalSizes(maxWorkGroupSize, maxInvocations, maxInvocations);
		const deUint32	localSizesToTestCount						= 16;
		const deUint32	localSizesToTest[localSizesToTestCount][3]	=
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
			{localSize.x(), localSize.y(), localSize.z()},
			{1, 1, 1} // Isn't used, just here to make double buffering checks easier
		};
		const struct internalDataStruct		internalData								=
		{
			&context,
			caseDef,
			subgroupSize,
			false,
		};

		return subgroups::makeComputeTestRequiredSubgroupSize(context,
															  VK_FORMAT_R32_UINT,
															  DE_NULL,
															  0,
															  &internalData,
															  checkCompute,
															  caseDef.pipelineShaderStageCreateFlags,
															  numWorkgroups,
															  makeDeBool(internalData.isRequiredSubgroupSize),
															  subgroupSize,
															  localSizesToTest,
															  localSizesToTestCount);
	}
#ifndef CTS_USES_VULKANSC
	else if (isAllMeshShadingStages(caseDef.shaderStage))
	{
		const bool		isMesh										= ((caseDef.shaderStage & VK_SHADER_STAGE_MESH_BIT_EXT) != 0u);
		const bool		isTask										= ((caseDef.shaderStage & VK_SHADER_STAGE_TASK_BIT_EXT) != 0u);

		DE_ASSERT(isMesh != isTask);
		DE_UNREF(isTask); // For release builds.

		const deUint32	numWorkgroups[3]							= {1, 1, 1};
		const deUint32	subgroupSize								= subgroups::getSubgroupSize(context);
		const auto&		meshProperties								= context.getMeshShaderPropertiesEXT();
		const auto&		maxWorkGroupSize							= (isMesh ? meshProperties.maxMeshWorkGroupSize : meshProperties.maxTaskWorkGroupSize);
		const auto&		maxInvocations								= (isMesh ? meshProperties.maxMeshWorkGroupInvocations : meshProperties.maxTaskWorkGroupInvocations);
		// Calculate the local workgroup sizes to exercise the maximum supported by the driver
		const UVec3		localSize									= getLocalSizes(maxWorkGroupSize, maxInvocations, maxInvocations);
		const deUint32	localSizesToTestCount						= 16;
		const deUint32	localSizesToTest[localSizesToTestCount][3]	=
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
			{localSize.x(), localSize.y(), localSize.z()},
			{1, 1, 1} // Isn't used, just here to make double buffering checks easier
		};
		const struct internalDataStruct		internalData								=
		{
			&context,
			caseDef,
			subgroupSize,
			false,
		};

		return subgroups::makeMeshTestRequiredSubgroupSize(context,
														   VK_FORMAT_R32_UINT,
														   nullptr,
														   0,
														   &internalData,
														   checkCompute,
														   caseDef.pipelineShaderStageCreateFlags,
														   numWorkgroups,
														   makeDeBool(internalData.isRequiredSubgroupSize),
														   subgroupSize,
														   localSizesToTest,
														   localSizesToTestCount);
	}
#endif // CTS_USES_VULKANSC
	else if (isAllGraphicsStages(caseDef.shaderStage))
	{
		const VkShaderStageFlags	stages			= subgroups::getPossibleGraphicsSubgroupStages(context, caseDef.shaderStage);
		struct internalDataStruct	internalData	=
		{
			&context,
			caseDef,
			0u,
			false,
		};

		return subgroups::allStagesRequiredSubgroupSize(context,
														VK_FORMAT_R32_UINT,
														nullptr,
														0,
														&internalData,
														checkVertexPipelineStages,
														stages,
														caseDef.pipelineShaderStageCreateFlags,
														caseDef.pipelineShaderStageCreateFlags,
														caseDef.pipelineShaderStageCreateFlags,
														caseDef.pipelineShaderStageCreateFlags,
														caseDef.pipelineShaderStageCreateFlags,
														nullptr);
	}
#ifndef CTS_USES_VULKANSC
	else if (isAllRayTracingStages(caseDef.shaderStage))
	{
		const VkShaderStageFlags		stages			= subgroups::getPossibleRayTracingSubgroupStages(context, caseDef.shaderStage);
		const vector<deUint32>			flags			(6, caseDef.pipelineShaderStageCreateFlags);
		const struct internalDataStruct	internalData	=
		{
			&context,
			caseDef,
			0u,
			false,
		};

		return subgroups::allRayTracingStagesRequiredSubgroupSize(context,
																  VK_FORMAT_R32_UINT,
																  nullptr,
																  0,
																  &internalData,
																  checkVertexPipelineStages,
																  stages,
																  flags.data(),
																  nullptr);
	}
#endif // CTS_USES_VULKANSC
	else
		TCU_THROW(InternalError, "Unknown stage or invalid stage set");
}

TestStatus testRequireFullSubgroups (Context& context, const CaseDefinition caseDef)
{
	DE_ASSERT(VK_SHADER_STAGE_COMPUTE_BIT == caseDef.shaderStage);
	DE_ASSERT(caseDef.requiredSubgroupSizeMode == REQUIRED_SUBGROUP_SIZE_NONE);

	const deUint32												numWorkgroups[3]							= {1, 1, 1};
#ifndef CTS_USES_VULKANSC
	const VkPhysicalDeviceSubgroupSizeControlProperties&	subgroupSizeControlProperties					= context.getSubgroupSizeControlProperties();
#else
	const VkPhysicalDeviceSubgroupSizeControlPropertiesEXT&	subgroupSizeControlProperties					= context.getSubgroupSizeControlPropertiesEXT();
#endif // CTS_USES_VULKANSC
	const VkPhysicalDeviceProperties&							physicalDeviceProperties					= context.getDeviceProperties();
	// Calculate the local workgroup sizes to exercise the maximum supported by the driver
	const auto&													maxWorkGroupSize							= physicalDeviceProperties.limits.maxComputeWorkGroupSize;
	const auto&													maxInvocations								= physicalDeviceProperties.limits.maxComputeWorkGroupInvocations;
	const UVec3													localSize									= getLocalSizes(maxWorkGroupSize, maxInvocations, maxInvocations);
	const deUint32												subgroupSize								= subgroups::getSubgroupSize(context);
	// For full subgroups and allow varying subgroup size, localsize X must be a multiple of maxSubgroupSize.
	// We set local size X for this test to the maximum, regardless if allow varying subgroup size is enabled or not.
	const deUint32												localSizesToTestCount						= 7;
	const deUint32												localSizesToTest[localSizesToTestCount][3]	=
	{
		{subgroupSizeControlProperties.maxSubgroupSize, 1, 1},
		{subgroupSizeControlProperties.maxSubgroupSize, 4, 1},
		{subgroupSizeControlProperties.maxSubgroupSize, 1, 4},
		{subgroupSizeControlProperties.maxSubgroupSize * 2, 1, 2},
		{subgroupSizeControlProperties.maxSubgroupSize * 4, 1, 1},
		{localSize.x(), localSize.y(), localSize.z()},
		{1, 1, 1} // Isn't used, just here to make double buffering checks easier
	};
	const struct internalDataStruct								internalData								=
	{
		&context,
		caseDef,
		subgroupSize,
		false,
	};

	DE_ASSERT(caseDef.requiredSubgroupSizeMode == REQUIRED_SUBGROUP_SIZE_NONE);

	return subgroups::makeComputeTestRequiredSubgroupSize(context,
														  VK_FORMAT_R32G32B32A32_UINT,
														  nullptr,
														  0,
														  &internalData,
														  checkComputeRequireFull,
														  caseDef.pipelineShaderStageCreateFlags,
														  numWorkgroups,
														  makeDeBool(internalData.isRequiredSubgroupSize),
														  subgroupSize,
														  localSizesToTest,
														  localSizesToTestCount);
}

TestStatus testRequireSubgroupSize (Context& context, const CaseDefinition caseDef)
{
	if (isAllComputeStages(caseDef.shaderStage))
	{
		const deUint32											numWorkgroups[3]							= {1, 1, 1};
#ifndef CTS_USES_VULKANSC
		const VkPhysicalDeviceSubgroupSizeControlProperties&	subgroupSizeControlProperties				= context.getSubgroupSizeControlProperties();
#else
		const VkPhysicalDeviceSubgroupSizeControlPropertiesEXT&	subgroupSizeControlProperties				= context.getSubgroupSizeControlPropertiesEXT();
#endif // CTS_USES_VULKANSC
		const VkPhysicalDeviceProperties&						physicalDeviceProperties					= context.getDeviceProperties();
		const deUint32											requiredSubgroupSize						= getRequiredSubgroupSizeFromMode(context, caseDef, subgroupSizeControlProperties);
		const deUint64											maxSubgroupLimitSize						= (deUint64)requiredSubgroupSize * subgroupSizeControlProperties.maxComputeWorkgroupSubgroups;
		const deUint32											maxTotalLocalSize							= (deUint32)min<deUint64>(maxSubgroupLimitSize, physicalDeviceProperties.limits.maxComputeWorkGroupInvocations);
		const auto&												maxWorkGroupSize							= physicalDeviceProperties.limits.maxComputeWorkGroupSize;
		const auto&												maxInvocations								= physicalDeviceProperties.limits.maxComputeWorkGroupInvocations;
		const UVec3												localSize									= getLocalSizes(maxWorkGroupSize, maxInvocations, maxTotalLocalSize);
		const bool												shaderUsesFullSubgroups						= caseDef.shaderUsesFullSubgroups();
		const deUint32											localSizesToTest[5][3]	=
		{
			{localSize.x(), localSize.y(), localSize.z()},
			{requiredSubgroupSize, 1, 1},
			{1, requiredSubgroupSize, 1},
			{1, 1, requiredSubgroupSize},
			{1, 1, 1} // Isn't used, just here to make double buffering checks easier
		};

		// If the shader uses full subgroups, use only the first two entries so the local size in X is a multiple of the requested
		// subgroup size, as required by the spec.
		deUint32 localSizesToTestCount = 5;
		if (shaderUsesFullSubgroups)
			localSizesToTestCount = 3;

		const internalDataStruct internalData =
		{
			&context,				//  const Context*			context;
			caseDef,				//  struct CaseDefinition	caseDef;
			requiredSubgroupSize,	//  deUint32				requiredSubgroupSize;
			true,					//	bool					isRequiredSubgroupSize;
		};

		// Depending on the flag and SPIR-V version we need to run one verification function or another.
		const auto checkFunction = (shaderUsesFullSubgroups ? checkComputeRequireFull : checkCompute);

		return subgroups::makeComputeTestRequiredSubgroupSize(context,
															  VK_FORMAT_R32G32B32A32_UINT,
															  nullptr,
															  0,
															  &internalData,
															  checkFunction,
															  caseDef.pipelineShaderStageCreateFlags,
															  numWorkgroups,
															  makeDeBool(internalData.isRequiredSubgroupSize),
															  requiredSubgroupSize,
															  localSizesToTest,
															  localSizesToTestCount);
	}
#ifndef CTS_USES_VULKANSC
	else if (isAllMeshShadingStages(caseDef.shaderStage))
	{
		const auto		isMesh										= ((caseDef.shaderStage & VK_SHADER_STAGE_MESH_BIT_EXT) != 0u);
		const auto		isTask										= ((caseDef.shaderStage & VK_SHADER_STAGE_TASK_BIT_EXT) != 0u);

		DE_ASSERT(isMesh != isTask);
		DE_UNREF(isTask); // For release builds.

		const uint32_t	numWorkgroups[3]							= {1, 1, 1};
		const auto&		subgroupSizeControlProperties				= context.getSubgroupSizeControlProperties();
		const auto&		meshProperties								= context.getMeshShaderPropertiesEXT();
		const uint32_t	requiredSubgroupSize						= getRequiredSubgroupSizeFromMode(context, caseDef, subgroupSizeControlProperties);
		const auto&		maxWorkGroupSize							= (isMesh ? meshProperties.maxMeshWorkGroupSize			: meshProperties.maxTaskWorkGroupSize);
		const auto&		maxInvocations								= (isMesh ? meshProperties.maxMeshWorkGroupInvocations	: meshProperties.maxTaskWorkGroupInvocations);
		const UVec3		localSize									= getLocalSizes(maxWorkGroupSize, maxInvocations, maxInvocations);
		const bool		shaderUsesFullSubgroups						= caseDef.shaderUsesFullSubgroups();
		const uint32_t	localSizesToTest[5][3]	=
		{
			{requiredSubgroupSize, 1, 1},
			{1, requiredSubgroupSize, 1},
			{1, 1, requiredSubgroupSize},
			{localSize.x(), localSize.y(), localSize.z()},
			{1, 1, 1} // Isn't used, just here to make double buffering checks easier
		};

		// If the shader uses full subgroups, use only the first two entries so the local size in X is a multiple of the requested
		// subgroup size, as required by the spec.
		deUint32 localSizesToTestCount = 5;
		if (shaderUsesFullSubgroups)
			localSizesToTestCount = 3;

		const internalDataStruct internalData =
		{
			&context,				//  const Context*			context;
			caseDef,				//  struct CaseDefinition	caseDef;
			requiredSubgroupSize,	//  uint32_t				requiredSubgroupSize;
			true,					//  bool					isRequiredSubgroupSize;
		};

		// Depending on the flag and SPIR-V version we need to run one verification function or another.
		const auto checkFunction = (shaderUsesFullSubgroups ? checkComputeRequireFull : checkCompute);

		return subgroups::makeMeshTestRequiredSubgroupSize(context,
														   VK_FORMAT_R32G32B32A32_UINT,
														   nullptr,
														   0,
														   &internalData,
														   checkFunction,
														   caseDef.pipelineShaderStageCreateFlags,
														   numWorkgroups,
														   makeDeBool(internalData.isRequiredSubgroupSize),
														   requiredSubgroupSize,
														   localSizesToTest,
														   localSizesToTestCount);
	}
#endif // CTS_USES_VULKANSC
	else if (isAllGraphicsStages(caseDef.shaderStage))
	{
		const VkShaderStageFlags								stages							= subgroups::getPossibleGraphicsSubgroupStages(context, caseDef.shaderStage);
#ifndef CTS_USES_VULKANSC
		const VkPhysicalDeviceSubgroupSizeControlProperties&	subgroupSizeControlProperties	= context.getSubgroupSizeControlProperties();
#else
		const VkPhysicalDeviceSubgroupSizeControlPropertiesEXT&	subgroupSizeControlProperties	= context.getSubgroupSizeControlPropertiesEXT();
#endif // CTS_USES_VULKANSC
		const deUint32											requiredSubgroupSize			= getRequiredSubgroupSizeFromMode(context, caseDef, subgroupSizeControlProperties);
		const deUint32											requiredSubgroupSizes[5]		= { requiredSubgroupSize, requiredSubgroupSize, requiredSubgroupSize, requiredSubgroupSize, requiredSubgroupSize};
		const internalDataStruct								internalData					=
		{
			&context,				//  const Context*			context;
			caseDef,				//  struct CaseDefinition	caseDef;
			requiredSubgroupSize,	//  deUint32				requiredSubgroupSize;
			true,					//  bool					isRequiredSubgroupSize;
		};

		return subgroups::allStagesRequiredSubgroupSize(context,
														VK_FORMAT_R32_UINT,
														DE_NULL,
														0,
														&internalData,
														checkVertexPipelineStages,
														stages,
														caseDef.pipelineShaderStageCreateFlags,
														caseDef.pipelineShaderStageCreateFlags,
														caseDef.pipelineShaderStageCreateFlags,
														caseDef.pipelineShaderStageCreateFlags,
														caseDef.pipelineShaderStageCreateFlags,
														requiredSubgroupSizes);
	}
#ifndef CTS_USES_VULKANSC
	else if (isAllRayTracingStages(caseDef.shaderStage))
	{
		const VkShaderStageFlags								stages							= subgroups::getPossibleRayTracingSubgroupStages(context, caseDef.shaderStage);
		const VkPhysicalDeviceSubgroupSizeControlProperties&	subgroupSizeControlProperties	= context.getSubgroupSizeControlProperties();
		const deUint32											requiredSubgroupSize			= getRequiredSubgroupSizeFromMode(context, caseDef, subgroupSizeControlProperties);
		const vector<deUint32>									flags							(6, caseDef.pipelineShaderStageCreateFlags);
		const vector<deUint32>									requiredSubgroupSizes			(6, requiredSubgroupSize);
		const struct internalDataStruct							internalData					=
		{
			&context,				//  const Context*			context;
			caseDef,				//  struct CaseDefinition	caseDef;
			requiredSubgroupSize,	//  deUint32				requiredSubgroupSize;
			true,					//  bool					isRequiredSubgroupSize;
		};

		return subgroups::allRayTracingStagesRequiredSubgroupSize(context,
																  VK_FORMAT_R32_UINT,
																  DE_NULL,
																  0,
																  &internalData,
																  checkVertexPipelineStages,
																  stages,
																  flags.data(),
																  requiredSubgroupSizes.data());
	}
#endif // CTS_USES_VULKANSC
	else
		TCU_THROW(InternalError, "Unknown stage or invalid stage set");
}

TestStatus noSSBOtestRequireSubgroupSize (Context& context, const CaseDefinition caseDef)
{
#ifndef CTS_USES_VULKANSC
	const VkPhysicalDeviceSubgroupSizeControlProperties&	subgroupSizeControlProperties	= context.getSubgroupSizeControlProperties();
#else
	const VkPhysicalDeviceSubgroupSizeControlPropertiesEXT&	subgroupSizeControlProperties	= context.getSubgroupSizeControlPropertiesEXT();
#endif // CTS_USES_VULKANSC
	const deUint32											requiredSubgroupSize			= getRequiredSubgroupSizeFromMode(context, caseDef, subgroupSizeControlProperties);
	const VkFormat											format							= VK_FORMAT_R32_UINT;
	const deUint32&											flags							= caseDef.pipelineShaderStageCreateFlags;
	const deUint32&											size							= requiredSubgroupSize;
	struct internalDataStruct								internalData					=
	{
		&context,
		caseDef,
		requiredSubgroupSize,
		true,
	};

	switch (caseDef.shaderStage)
	{
		case VK_SHADER_STAGE_VERTEX_BIT:					return subgroups::makeVertexFrameBufferTestRequiredSubgroupSize(context, format, DE_NULL, 0, &internalData, checkVertexPipelineStages, flags, size);
		case VK_SHADER_STAGE_GEOMETRY_BIT:					return subgroups::makeGeometryFrameBufferTestRequiredSubgroupSize(context, format, DE_NULL, 0, &internalData, checkVertexPipelineStages, flags, size);
		case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:		return subgroups::makeTessellationEvaluationFrameBufferTestRequiredSubgroupSize(context, format, DE_NULL, 0, &internalData, checkVertexPipelineStages, caseDef.shaderStage, flags, size);
		case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT:	return subgroups::makeTessellationEvaluationFrameBufferTestRequiredSubgroupSize(context, format, DE_NULL, 0, &internalData, checkVertexPipelineStages, caseDef.shaderStage, flags, size);
		case VK_SHADER_STAGE_FRAGMENT_BIT:					return subgroups::makeFragmentFrameBufferTestRequiredSubgroupSize(context, format, DE_NULL, 0, &internalData, checkFragmentPipelineStages, flags, size);
		default:											TCU_THROW(InternalError, "Unhandled shader stage");
	}
}

TestStatus testSanitySubgroupSizeProperties (Context& context)
{
#ifndef CTS_USES_VULKANSC
	VkPhysicalDeviceSubgroupSizeControlProperties subgroupSizeControlProperties;
	subgroupSizeControlProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_SIZE_CONTROL_PROPERTIES;
#else
	VkPhysicalDeviceSubgroupSizeControlPropertiesEXT subgroupSizeControlProperties;
	subgroupSizeControlProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_SIZE_CONTROL_PROPERTIES_EXT;
#endif // CTS_USES_VULKANSC

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
		ostringstream error;
		error << "subgroupSize (" << subgroupProperties.subgroupSize << ") is not between maxSubgroupSize (";
		error << subgroupSizeControlProperties.maxSubgroupSize << ") and minSubgroupSize (";
		error << subgroupSizeControlProperties.minSubgroupSize << ")";

		return TestStatus::fail(error.str().c_str());
	}

	return TestStatus::pass("OK");
}
}

namespace vkt
{
namespace subgroups
{
TestCaseGroup* createSubgroupsSizeControlTests (TestContext& testCtx)
{
	de::MovePtr<TestCaseGroup>	group				(new TestCaseGroup(testCtx, "size_control", "VK_EXT_subgroup_size_control tests"));
	de::MovePtr<TestCaseGroup>	framebufferGroup	(new TestCaseGroup(testCtx, "framebuffer", "Subgroup size control category tests: framebuffer"));
	de::MovePtr<TestCaseGroup>	computeGroup		(new TestCaseGroup(testCtx, "compute", "Subgroup size control category tests: compute"));
	de::MovePtr<TestCaseGroup>	graphicsGroup		(new TestCaseGroup(testCtx, "graphics", "Subgroup size control category tests: graphics"));
#ifndef CTS_USES_VULKANSC
	de::MovePtr<TestCaseGroup>	raytracingGroup		(new TestCaseGroup(testCtx, "ray_tracing", "Subgroup size control category tests: ray tracing"));
	de::MovePtr<TestCaseGroup>	meshGroup			(new TestCaseGroup(testCtx, "mesh", "Subgroup size control category tests: mesh shading"));
#endif // CTS_USES_VULKANSC
	de::MovePtr<TestCaseGroup>	genericGroup		(new TestCaseGroup(testCtx, "generic", "Subgroup size control category tests: generic"));
	const VkShaderStageFlags	fbStages[]			=
	{
		VK_SHADER_STAGE_VERTEX_BIT,
		VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,
		VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,
		VK_SHADER_STAGE_GEOMETRY_BIT,
		VK_SHADER_STAGE_FRAGMENT_BIT,
	};
#ifndef CTS_USES_VULKANSC
	const VkShaderStageFlags	meshStages[]		=
	{
		VK_SHADER_STAGE_MESH_BIT_EXT,
		VK_SHADER_STAGE_TASK_BIT_EXT,
	};
#endif // CTS_USES_VULKANSC

	// Test sanity of the subgroup size properties.
	{
		addFunctionCase(genericGroup.get(), "subgroup_size_properties", "", supportedCheck, testSanitySubgroupSizeProperties);
	}

	const TestParams			testParams[]		= {{false, true, ""}, {true, false, "_spirv16"}, {true, true, "_flags_spirv16"}};

	for (const auto& params : testParams)
	{
		// Allow varying subgroup cases.
		const deUint32			flagsVary				= VK_PIPELINE_SHADER_STAGE_CREATE_ALLOW_VARYING_SUBGROUP_SIZE_BIT_EXT;
		const CaseDefinition	caseDefVary				= {params.flagsEnabled ? flagsVary : 0u, VK_SHADER_STAGE_COMPUTE_BIT, DE_FALSE, REQUIRED_SUBGROUP_SIZE_NONE,
														   de::SharedPtr<bool>(new bool), params.useSpirv16 ? SPIRV_VERSION_1_6 : SPIRV_VERSION_1_3};

		addFunctionCaseWithPrograms(computeGroup.get(), "allow_varying_subgroup_size" + params.postfix, "", supportedCheckFeatures,
									initPrograms, test, caseDefVary);
		addFunctionCaseWithPrograms(graphicsGroup.get(), "allow_varying_subgroup_size" + params.postfix, "",
									supportedCheckFeaturesShader, initPrograms, test, caseDefVary);

		for (int stageIndex = 0; stageIndex < DE_LENGTH_OF_ARRAY(fbStages); ++stageIndex)
		{
			const CaseDefinition	caseDefStage	= {params.flagsEnabled ? flagsVary : 0u, fbStages[stageIndex], DE_FALSE, REQUIRED_SUBGROUP_SIZE_NONE,
													   de::SharedPtr<bool>(new bool), params.useSpirv16 ? SPIRV_VERSION_1_6 : SPIRV_VERSION_1_3};

			string					name			= getShaderStageName(caseDefStage.shaderStage) + "_allow_varying_subgroup_size" + params.postfix;
			addFunctionCaseWithPrograms(framebufferGroup.get(), name, "", supportedCheckFeaturesShader, initFrameBufferPrograms,
										noSSBOtest, caseDefStage);
		}

#ifndef CTS_USES_VULKANSC
		for (const auto& stage : meshStages)
		{
			const CaseDefinition	caseDefMesh	= {(params.flagsEnabled ? flagsVary : 0u), stage, DE_FALSE, REQUIRED_SUBGROUP_SIZE_NONE,
													de::SharedPtr<bool>(new bool), (params.useSpirv16 ? SPIRV_VERSION_1_6 : SPIRV_VERSION_1_4)};
			const std::string		name		= getShaderStageName(stage) + "_allow_varying_subgroup_size" + params.postfix;
			addFunctionCaseWithPrograms(meshGroup.get(), name, "", supportedCheckFeatures, initPrograms, test, caseDefMesh);
		}
#endif // CTS_USES_VULKANSC

		// Require full subgroups together with allow varying subgroup (only compute shaders).
		const deUint32			flagsFullVary			= VK_PIPELINE_SHADER_STAGE_CREATE_REQUIRE_FULL_SUBGROUPS_BIT_EXT
														  | VK_PIPELINE_SHADER_STAGE_CREATE_ALLOW_VARYING_SUBGROUP_SIZE_BIT_EXT;
		const CaseDefinition	caseDefFullVary			= {params.flagsEnabled ? flagsFullVary : 0u, VK_SHADER_STAGE_COMPUTE_BIT, DE_TRUE, REQUIRED_SUBGROUP_SIZE_NONE, de::SharedPtr<bool>(new bool),
														   params.useSpirv16 ? SPIRV_VERSION_1_6 : SPIRV_VERSION_1_3};
		addFunctionCaseWithPrograms(computeGroup.get(), "require_full_subgroups_allow_varying_subgroup_size" + params.postfix, "",
									supportedCheckFeatures, initProgramsRequireFull, testRequireFullSubgroups, caseDefFullVary);

		// Require full subgroups cases (only compute shaders).
		const deUint32			flagsFull				= VK_PIPELINE_SHADER_STAGE_CREATE_REQUIRE_FULL_SUBGROUPS_BIT_EXT;
		const CaseDefinition	caseDefFull				= {params.flagsEnabled ? flagsFull : 0u, VK_SHADER_STAGE_COMPUTE_BIT, DE_TRUE, REQUIRED_SUBGROUP_SIZE_NONE, de::SharedPtr<bool>(new bool),
														   params.useSpirv16 ? SPIRV_VERSION_1_6 : SPIRV_VERSION_1_3};
		addFunctionCaseWithPrograms(computeGroup.get(), "require_full_subgroups" + params.postfix, "", supportedCheckFeatures, initProgramsRequireFull,
									testRequireFullSubgroups, caseDefFull);

		// Tests to check setting a required subgroup size value, together with require full subgroups (only compute shaders).
		const CaseDefinition	caseDefMaxFull			= {params.flagsEnabled ? flagsFull : 0u, VK_SHADER_STAGE_COMPUTE_BIT, DE_TRUE, REQUIRED_SUBGROUP_SIZE_MAX, de::SharedPtr<bool>(new bool),
														   params.useSpirv16 ? SPIRV_VERSION_1_6 : SPIRV_VERSION_1_3};
		addFunctionCaseWithPrograms(computeGroup.get(), "required_subgroup_size_max_require_full_subgroups" + params.postfix, "", supportedCheckFeatures,
									initProgramsRequireFull, testRequireSubgroupSize, caseDefMaxFull);

		const CaseDefinition	caseDefMinFull			= {params.flagsEnabled ? flagsFull : 0u, VK_SHADER_STAGE_COMPUTE_BIT, DE_TRUE, REQUIRED_SUBGROUP_SIZE_MIN, de::SharedPtr<bool>(new bool),
														   params.useSpirv16 ? SPIRV_VERSION_1_6 : SPIRV_VERSION_1_3};
		addFunctionCaseWithPrograms(computeGroup.get(), "required_subgroup_size_min_require_full_subgroups" + params.postfix, "", supportedCheckFeatures,
									initProgramsRequireFull, testRequireSubgroupSize, caseDefMinFull);

		// Ray tracing cases with allow varying subgroup.
#ifndef CTS_USES_VULKANSC
		const deUint32			flagsRayTracing			= VK_PIPELINE_SHADER_STAGE_CREATE_ALLOW_VARYING_SUBGROUP_SIZE_BIT_EXT;
		const CaseDefinition	caseDefAllRaytracing	= {params.flagsEnabled ? flagsRayTracing : 0u, SHADER_STAGE_ALL_RAY_TRACING, DE_FALSE, REQUIRED_SUBGROUP_SIZE_NONE,
														   de::SharedPtr<bool>(new bool), params.useSpirv16 ? SPIRV_VERSION_1_6 : SPIRV_VERSION_1_4};
		addFunctionCaseWithPrograms(raytracingGroup.get(), "allow_varying_subgroup_size" + params.postfix, "", supportedCheckFeaturesShader,
									initPrograms, test, caseDefAllRaytracing);
#endif // CTS_USES_VULKANSC
	}

	// Tests to check setting a required subgroup size value.
	{
		const CaseDefinition caseDefAllGraphicsMax = {0u, VK_SHADER_STAGE_ALL_GRAPHICS, DE_FALSE, REQUIRED_SUBGROUP_SIZE_MAX, de::SharedPtr<bool>(new bool), SPIRV_VERSION_1_3};
		addFunctionCaseWithPrograms(graphicsGroup.get(), "required_subgroup_size_max", "", supportedCheckFeaturesShader, initPrograms, testRequireSubgroupSize, caseDefAllGraphicsMax);
		const CaseDefinition caseDefComputeMax = {0u, VK_SHADER_STAGE_COMPUTE_BIT, DE_FALSE, REQUIRED_SUBGROUP_SIZE_MAX, de::SharedPtr<bool>(new bool), SPIRV_VERSION_1_3};
		addFunctionCaseWithPrograms(computeGroup.get(), "required_subgroup_size_max", "", supportedCheckFeatures, initPrograms, testRequireSubgroupSize, caseDefComputeMax);
#ifndef CTS_USES_VULKANSC
		const CaseDefinition caseDefAllRaytracingMax = {0u, SHADER_STAGE_ALL_RAY_TRACING, DE_FALSE, REQUIRED_SUBGROUP_SIZE_MAX, de::SharedPtr<bool>(new bool), SPIRV_VERSION_1_4};
		addFunctionCaseWithPrograms(raytracingGroup.get(), "required_subgroup_size_max", "", supportedCheckFeaturesShader, initPrograms, testRequireSubgroupSize, caseDefAllRaytracingMax);
#endif // CTS_USES_VULKANSC

		const CaseDefinition caseDefAllGraphicsMin = {0u, VK_SHADER_STAGE_ALL_GRAPHICS, DE_FALSE, REQUIRED_SUBGROUP_SIZE_MIN, de::SharedPtr<bool>(new bool), SPIRV_VERSION_1_3};
		addFunctionCaseWithPrograms(graphicsGroup.get(), "required_subgroup_size_min", "", supportedCheckFeaturesShader, initPrograms, testRequireSubgroupSize, caseDefAllGraphicsMin);
		const CaseDefinition caseDefComputeMin = {0u, VK_SHADER_STAGE_COMPUTE_BIT, DE_FALSE, REQUIRED_SUBGROUP_SIZE_MIN, de::SharedPtr<bool>(new bool), SPIRV_VERSION_1_3};
		addFunctionCaseWithPrograms(computeGroup.get(), "required_subgroup_size_min", "", supportedCheckFeatures, initPrograms, testRequireSubgroupSize, caseDefComputeMin);
#ifndef CTS_USES_VULKANSC
		const CaseDefinition caseDefAllRaytracingMin = {0u, SHADER_STAGE_ALL_RAY_TRACING, DE_FALSE, REQUIRED_SUBGROUP_SIZE_MIN, de::SharedPtr<bool>(new bool), SPIRV_VERSION_1_4};
		addFunctionCaseWithPrograms(raytracingGroup.get(), "required_subgroup_size_min", "", supportedCheckFeaturesShader, initPrograms, testRequireSubgroupSize, caseDefAllRaytracingMin);
#endif // CTS_USES_VULKANSC
		for (int stageIndex = 0; stageIndex < DE_LENGTH_OF_ARRAY(fbStages); ++stageIndex)
		{
			const CaseDefinition caseDefStageMax = {0u, fbStages[stageIndex], DE_FALSE, REQUIRED_SUBGROUP_SIZE_MAX, de::SharedPtr<bool>(new bool), SPIRV_VERSION_1_3};
			addFunctionCaseWithPrograms(framebufferGroup.get(),  getShaderStageName(caseDefStageMax.shaderStage) + "_required_subgroup_size_max", "", supportedCheckFeaturesShader, initFrameBufferPrograms, noSSBOtestRequireSubgroupSize, caseDefStageMax);
			const CaseDefinition caseDefStageMin = {0u, fbStages[stageIndex], DE_FALSE, REQUIRED_SUBGROUP_SIZE_MIN, de::SharedPtr<bool>(new bool), SPIRV_VERSION_1_3};
			addFunctionCaseWithPrograms(framebufferGroup.get(),  getShaderStageName(caseDefStageMin.shaderStage) + "_required_subgroup_size_min", "", supportedCheckFeaturesShader, initFrameBufferPrograms, noSSBOtestRequireSubgroupSize, caseDefStageMin);
		}

#ifndef CTS_USES_VULKANSC
		for (const auto& stage : meshStages)
		{
			const auto stageName = getShaderStageName(stage);

			const CaseDefinition caseDefMeshMax = {0u, stage, DE_FALSE, REQUIRED_SUBGROUP_SIZE_MAX, de::SharedPtr<bool>(new bool), SPIRV_VERSION_1_4};
			addFunctionCaseWithPrograms(meshGroup.get(), "required_subgroup_size_max_" + stageName, "", supportedCheckFeatures, initPrograms, testRequireSubgroupSize, caseDefMeshMax);
			const CaseDefinition caseDefMeshMin = {0u, stage, DE_FALSE, REQUIRED_SUBGROUP_SIZE_MIN, de::SharedPtr<bool>(new bool), SPIRV_VERSION_1_4};
			addFunctionCaseWithPrograms(meshGroup.get(), "required_subgroup_size_min_" + stageName, "", supportedCheckFeatures, initPrograms, testRequireSubgroupSize, caseDefMeshMin);
		}
#endif // CTS_USES_VULKANSC
	}

	group->addChild(genericGroup.release());
	group->addChild(graphicsGroup.release());
	group->addChild(computeGroup.release());
	group->addChild(framebufferGroup.release());
#ifndef CTS_USES_VULKANSC
	group->addChild(raytracingGroup.release());
	group->addChild(meshGroup.release());
#endif // CTS_USES_VULKANSC

	return group.release();
}

} // subgroups
} // vkt
