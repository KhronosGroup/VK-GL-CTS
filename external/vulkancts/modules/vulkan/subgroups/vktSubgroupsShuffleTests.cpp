/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2019, 2021-2023 The Khronos Group Inc.
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
	OPTYPE_ROTATE,
	OPTYPE_CLUSTERED_ROTATE,
	OPTYPE_LAST
};

// For the second arguments of Xor, Up and Down.
enum class ArgType
{
	DYNAMIC = 0,
	DYNAMICALLY_UNIFORM,
	CONSTANT
};

struct CaseDefinition
{
	OpType				opType;
	VkShaderStageFlags	shaderStage;
	VkFormat			format;
	de::SharedPtr<bool>	geometryPointSizeSupported;
	deBool				requiredSubgroupSize;
	ArgType				argType;
	deBool				requires8BitUniformBuffer;
	deBool				requires16BitUniformBuffer;
};

static bool checkVertexPipelineStages (const void*			internalData,
									   vector<const void*>	datas,
									   deUint32				width,
									   deUint32)
{
	DE_UNREF(internalData);

	return subgroups::check(datas, width, 1);
}

static bool checkComputeOrMesh (const void*			internalData,
								vector<const void*>	datas,
								const deUint32		numWorkgroups[3],
								const deUint32		localSize[3],
								deUint32)
{
	DE_UNREF(internalData);

	return subgroups::checkComputeOrMesh(datas, numWorkgroups, localSize, 1);
}

string getOpTypeName (OpType opType)
{
	switch (opType)
	{
		case OPTYPE_SHUFFLE:			return "subgroupShuffle";
		case OPTYPE_SHUFFLE_XOR:		return "subgroupShuffleXor";
		case OPTYPE_SHUFFLE_UP:			return "subgroupShuffleUp";
		case OPTYPE_SHUFFLE_DOWN:		return "subgroupShuffleDown";
		case OPTYPE_ROTATE:				return "subgroupRotate";
		case OPTYPE_CLUSTERED_ROTATE:	return "subgroupClusteredRotate";
		default:					TCU_THROW(InternalError, "Unsupported op type");
	}
}

string getExtensionForOpType (OpType opType)
{
	switch (opType)
	{
		case OPTYPE_SHUFFLE:			return "GL_KHR_shader_subgroup_shuffle";
		case OPTYPE_SHUFFLE_XOR:		return "GL_KHR_shader_subgroup_shuffle";
		case OPTYPE_SHUFFLE_UP:			return "GL_KHR_shader_subgroup_shuffle_relative";
		case OPTYPE_SHUFFLE_DOWN:		return "GL_KHR_shader_subgroup_shuffle_relative";
		case OPTYPE_ROTATE:				return "GL_KHR_shader_subgroup_rotate";
		case OPTYPE_CLUSTERED_ROTATE:	return "GL_KHR_shader_subgroup_rotate";
		default:						TCU_THROW(InternalError, "Unsupported op type");
	}
}

string getExtHeader (const CaseDefinition& caseDef)
{
	return	string("#extension ") + getExtensionForOpType(caseDef.opType) + ": enable\n"
			"#extension GL_KHR_shader_subgroup_ballot: enable\n"
			+ subgroups::getAdditionalExtensionForFormat(caseDef.format);
}

vector<string> getPerStageHeadDeclarations (const CaseDefinition& caseDef)
{
	const string	formatName	= subgroups::getFormatNameForGLSL(caseDef.format);
	const deUint32	stageCount	= subgroups::getStagesCount(caseDef.shaderStage);
	const bool		fragment	= (caseDef.shaderStage & VK_SHADER_STAGE_FRAGMENT_BIT) != 0;
	const size_t	resultSize	= stageCount + (fragment ? 1 : 0);
	vector<string>	result		(resultSize, string());

	for (deUint32 i = 0; i < result.size(); ++i)
	{
		const deUint32	binding0	= i;
		const deUint32	binding1	= stageCount;
		const deUint32	binding2	= stageCount + 1;
		const string	buffer1		= (i == stageCount)
									? "layout(location = 0) out uint result;\n"
									: "layout(set = 0, binding = " + de::toString(binding0) + ", std430) buffer Buffer1\n"
									  "{\n"
									  "  uint result[];\n"
									  "};\n";

		const string	b2Layout	= ((caseDef.argType == ArgType::DYNAMIC) ? "std430"				: "std140");
		const string	b2Type		= ((caseDef.argType == ArgType::DYNAMIC) ? "readonly buffer"	: "uniform");

		result[i] =
			buffer1 +
			"layout(set = 0, binding = " + de::toString(binding1) + ", std430) readonly buffer Buffer2\n"
			"{\n"
			"  " + formatName + " data1[];\n"
			"};\n"
			"layout(set = 0, binding = " + de::toString(binding2) + ", " + b2Layout + ") " + b2Type + " Buffer3\n"
			"{\n"
			"  uint data2[];\n"
			"};\n";
	}

	return result;
}

vector<string> getFramebufferPerStageHeadDeclarations (const CaseDefinition& caseDef)
{
	const string	formatName	= subgroups::getFormatNameForGLSL(caseDef.format);
	const deUint32	stageCount	= subgroups::getStagesCount(caseDef.shaderStage);
	vector<string>	result		(stageCount, string());
	const auto		b2Len		= ((caseDef.argType == ArgType::DYNAMIC) ? subgroups::maxSupportedSubgroupSize() : 1u);
	const string	buffer2
	{
		"layout(set = 0, binding = 0) uniform Buffer1\n"
		"{\n"
		"  " + formatName + " data1[" + de::toString(subgroups::maxSupportedSubgroupSize()) + "];\n"
		"};\n"
		"layout(set = 0, binding = 1) uniform Buffer2\n"
		"{\n"
		"  uint data2[" + de::toString(b2Len) + "];\n"
		"};\n"
	};

	for (size_t i = 0; i < result.size(); ++i)
	{
		switch (i)
		{
			case 0: result[i] = "layout(location = 0) out float result;\n" + buffer2;		break;
			case 1: result[i] = "layout(location = 0) out float out_color;\n" + buffer2;	break;
			case 2: result[i] = "layout(location = 0) out float out_color[];\n" + buffer2;	break;
			case 3: result[i] = "layout(location = 0) out float out_color;\n" + buffer2;	break;
			default: TCU_THROW(InternalError, "Unknown stage");
		}
	}

	return result;
}

const string getNonClusteredTestSource (const CaseDefinition& caseDef)
{
	const string	id			= caseDef.opType == OPTYPE_SHUFFLE		? "id_in"
								: caseDef.opType == OPTYPE_SHUFFLE_XOR	? "gl_SubgroupInvocationID ^ id_in"
								: caseDef.opType == OPTYPE_SHUFFLE_UP	? "gl_SubgroupInvocationID - id_in"
								: caseDef.opType == OPTYPE_SHUFFLE_DOWN	? "gl_SubgroupInvocationID + id_in"
								: caseDef.opType == OPTYPE_ROTATE		? "(gl_SubgroupInvocationID + id_in) & (gl_SubgroupSize - 1)"
								: "";
	const string	idInSource	= caseDef.argType == ArgType::DYNAMIC				? "data2[gl_SubgroupInvocationID] & (gl_SubgroupSize - 1)"
								: caseDef.argType == ArgType::DYNAMICALLY_UNIFORM	? (
									  caseDef.opType == OPTYPE_ROTATE	? "data2[0] & (gl_SubgroupSize * 2 - 1)"
									: "data2[0] % 32")
								: caseDef.argType == ArgType::CONSTANT				? "5"
								: "";
	const string	testSource	=
		"  uint temp_res;\n"
		"  uvec4 mask = subgroupBallot(true);\n"
		"  uint id_in = " + idInSource + ";\n"
		"  " + subgroups::getFormatNameForGLSL(caseDef.format) + " op = "
		+ getOpTypeName(caseDef.opType) + "(data1[gl_SubgroupInvocationID], id_in);\n"
		"  uint id = " + id + ";\n"
		"  if ((id < gl_SubgroupSize) && subgroupBallotBitExtract(mask, id))\n"
		"  {\n"
		"    temp_res = (op == data1[id]) ? 1 : 0;\n"
		"  }\n"
		"  else\n"
		"  {\n"
		"    temp_res = 1; // Invocation we read from was inactive, so we can't verify results!\n"
		"  }\n"
		"  tempRes = temp_res;\n";

	return testSource;
}

const string getClusteredTestSource (const CaseDefinition& caseDef)
{
	const string	idInSource	= caseDef.argType == ArgType::DYNAMICALLY_UNIFORM	? "data2[0] & (gl_SubgroupSize * 2 - 1)"
								: caseDef.argType == ArgType::CONSTANT				? "5"
								: "";
	const string	testSource	=
		"  uint temp_res = 1;\n"
		"  uvec4 mask = subgroupBallot(true);\n"
		"  uint cluster_size;\n"
		"  for (cluster_size = 1; cluster_size <= gl_SubgroupSize; cluster_size *= 2)\n"
		"  {\n"
		"    uint id_in = " + idInSource + ";\n"
		"    uint cluster_res;\n"
		"    " + subgroups::getFormatNameForGLSL(caseDef.format) + " data1_val = data1[gl_SubgroupInvocationID];\n"
		"    " + subgroups::getFormatNameForGLSL(caseDef.format) + " op;\n"
		"    switch (cluster_size)\n"
		"    {\n"
		"      case 1: op = " + getOpTypeName(caseDef.opType) + "(data1_val, id_in, 1u); break;\n"
		"      case 2: op = " + getOpTypeName(caseDef.opType) + "(data1_val, id_in, 2u); break;\n"
		"      case 4: op = " + getOpTypeName(caseDef.opType) + "(data1_val, id_in, 4u); break;\n"
		"      case 8: op = " + getOpTypeName(caseDef.opType) + "(data1_val, id_in, 8u); break;\n"
		"      case 16: op = " + getOpTypeName(caseDef.opType) + "(data1_val, id_in, 16u); break;\n"
		"      case 32: op = " + getOpTypeName(caseDef.opType) + "(data1_val, id_in, 32u); break;\n"
		"      case 64: op = " + getOpTypeName(caseDef.opType) + "(data1_val, id_in, 64u); break;\n"
		"      case 128: op = " + getOpTypeName(caseDef.opType) + "(data1_val, id_in, 128u); break;\n"
		"    }\n"
		"    uint id = ((gl_SubgroupInvocationID + id_in) & (cluster_size - 1)) | (gl_SubgroupInvocationID & ~(cluster_size - 1));\n"
		"    if ((id < gl_SubgroupSize) && subgroupBallotBitExtract(mask, id))\n"
		"    {\n"
		"      cluster_res = (op == data1[id]) ? 1 : 0;\n"
		"    }\n"
		"    else\n"
		"    {\n"
		"      cluster_res = 1; // Invocation we read from was inactive, so we can't verify results!\n"
		"    }\n"
		"    temp_res = (temp_res & cluster_res);\n"
		"  }\n"
		"  tempRes = temp_res;\n";

	return testSource;
}

const string getTestSource (const CaseDefinition& caseDef)
{
	if (caseDef.opType == OPTYPE_CLUSTERED_ROTATE)
	{
		return getClusteredTestSource(caseDef);
	}
	return getNonClusteredTestSource(caseDef);
}

void initFrameBufferPrograms (SourceCollections& programCollection, CaseDefinition caseDef)
{
	const ShaderBuildOptions	buildOptions		(programCollection.usedVulkanVersion, SPIRV_VERSION_1_3, 0u);
	const string				extHeader			= getExtHeader(caseDef);
	const string				testSrc				= getTestSource(caseDef);
	const vector<string>		headDeclarations	= getFramebufferPerStageHeadDeclarations(caseDef);
	const bool					pointSizeSupported	= *caseDef.geometryPointSizeSupported;

	subgroups::initStdFrameBufferPrograms(programCollection, buildOptions, caseDef.shaderStage, VK_FORMAT_R32_UINT, pointSizeSupported, extHeader, testSrc, "", headDeclarations);
}

void initPrograms (SourceCollections& programCollection, CaseDefinition caseDef)
{
#ifndef CTS_USES_VULKANSC
	const bool					spirv14required		= (isAllRayTracingStages(caseDef.shaderStage) || isAllMeshShadingStages(caseDef.shaderStage));
#else
	const bool					spirv14required		= false;
#endif // CTS_USES_VULKANSC
	const SpirvVersion			spirvVersion		= spirv14required ? SPIRV_VERSION_1_4 : SPIRV_VERSION_1_3;
	const ShaderBuildOptions	buildOptions		(programCollection.usedVulkanVersion, spirvVersion, 0u, spirv14required);
	const string				extHeader			= getExtHeader(caseDef);
	const string				testSrc				= getTestSource(caseDef);
	const vector<string>		headDeclarations	= getPerStageHeadDeclarations(caseDef);
	const bool					pointSizeSupported	= *caseDef.geometryPointSizeSupported;

	subgroups::initStdPrograms(programCollection, buildOptions, caseDef.shaderStage, VK_FORMAT_R32_UINT, pointSizeSupported, extHeader, testSrc, "", headDeclarations);
}

void supportedCheck (Context& context, CaseDefinition caseDef)
{
	if (!subgroups::isSubgroupSupported(context))
		TCU_THROW(NotSupportedError, "Subgroup operations are not supported");

	switch (caseDef.opType)
	{
		case OPTYPE_SHUFFLE:
		case OPTYPE_SHUFFLE_XOR:
			if (!subgroups::isSubgroupFeatureSupportedForDevice(context, VK_SUBGROUP_FEATURE_SHUFFLE_BIT))
			{
				TCU_THROW(NotSupportedError, "Device does not support subgroup shuffle operations");
			}
			break;
#ifndef CTS_USES_VULKANSC
		case OPTYPE_ROTATE:
			if (!context.getShaderSubgroupRotateFeatures().shaderSubgroupRotate)
			{
				TCU_THROW(NotSupportedError, "Device does not support shaderSubgroupRotate");
			}
			if (!subgroups::isSubgroupRotateSpecVersionValid(context))
			{
				TCU_THROW(NotSupportedError, "VK_KHR_shader_subgroup_rotate is version 1. Need version 2 or higher");
			}
			break;
		case OPTYPE_CLUSTERED_ROTATE:
			if (!context.getShaderSubgroupRotateFeatures().shaderSubgroupRotateClustered)
			{
				TCU_THROW(NotSupportedError, "Device does not support shaderSubgroupRotateClustered");
			}
			if (!subgroups::isSubgroupRotateSpecVersionValid(context))
			{
				TCU_THROW(NotSupportedError, "VK_KHR_shader_subgroup_rotate is version 1. Need version 2 or higher");
			}
			break;
#endif // CTS_USES_VULKANSC
		default:
			if (!subgroups::isSubgroupFeatureSupportedForDevice(context, VK_SUBGROUP_FEATURE_SHUFFLE_RELATIVE_BIT))
			{
				TCU_THROW(NotSupportedError, "Device does not support subgroup shuffle relative operations");
			}
			break;
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

	if (caseDef.requiredSubgroupSize)
	{
		context.requireDeviceFunctionality("VK_EXT_subgroup_size_control");

#ifndef CTS_USES_VULKANSC
		const VkPhysicalDeviceSubgroupSizeControlFeatures&		subgroupSizeControlFeatures		= context.getSubgroupSizeControlFeatures();
		const VkPhysicalDeviceSubgroupSizeControlProperties&	subgroupSizeControlProperties	= context.getSubgroupSizeControlProperties();
#else
		const VkPhysicalDeviceSubgroupSizeControlFeaturesEXT&	subgroupSizeControlFeatures		= context.getSubgroupSizeControlFeaturesEXT();
		const VkPhysicalDeviceSubgroupSizeControlPropertiesEXT&	subgroupSizeControlProperties	= context.getSubgroupSizeControlPropertiesEXT();
#endif // CTS_USES_VULKANSC

		if (subgroupSizeControlFeatures.subgroupSizeControl == DE_FALSE)
			TCU_THROW(NotSupportedError, "Device does not support varying subgroup sizes nor required subgroup size");

		if (subgroupSizeControlFeatures.computeFullSubgroups == DE_FALSE)
			TCU_THROW(NotSupportedError, "Device does not support full subgroups in compute shaders");

		if ((subgroupSizeControlProperties.requiredSubgroupSizeStages & caseDef.shaderStage) != caseDef.shaderStage)
			TCU_THROW(NotSupportedError, "Required subgroup size is not supported for shader stage");
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

	subgroups::supportedCheckShader(context, caseDef.shaderStage);
}

TestStatus noSSBOtest (Context& context, const CaseDefinition caseDef)
{
	const VkDeviceSize			secondBufferSize = ((caseDef.argType == ArgType::DYNAMIC) ? subgroups::maxSupportedSubgroupSize() : 1u);
	const subgroups::SSBOData	inputData[2]
	{
		{
			subgroups::SSBOData::InitializeNonZero,	//  InputDataInitializeType		initializeType;
			subgroups::SSBOData::LayoutStd140,		//  InputDataLayoutType			layout;
			caseDef.format,							//  vk::VkFormat				format;
			subgroups::maxSupportedSubgroupSize(),	//  vk::VkDeviceSize			numElements;
			subgroups::SSBOData::BindingUBO,		//  BindingType					bindingType;
		},
		{
			subgroups::SSBOData::InitializeNonZero,	//  InputDataInitializeType		initializeType;
			subgroups::SSBOData::LayoutStd140,		//  InputDataLayoutType			layout;
			VK_FORMAT_R32_UINT,						//  vk::VkFormat				format;
			secondBufferSize,						//  vk::VkDeviceSize			numElements;
			subgroups::SSBOData::BindingUBO,		//  BindingType					bindingType;
		}
	};

	switch (caseDef.shaderStage)
	{
		case VK_SHADER_STAGE_VERTEX_BIT:					return subgroups::makeVertexFrameBufferTest(context, VK_FORMAT_R32_UINT, inputData, 2, DE_NULL, checkVertexPipelineStages);
		case VK_SHADER_STAGE_GEOMETRY_BIT:					return subgroups::makeGeometryFrameBufferTest(context, VK_FORMAT_R32_UINT, inputData, 2, DE_NULL, checkVertexPipelineStages);
		case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:		return subgroups::makeTessellationEvaluationFrameBufferTest(context, VK_FORMAT_R32_UINT, inputData, 2, DE_NULL, checkVertexPipelineStages, caseDef.shaderStage);
		case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT:	return subgroups::makeTessellationEvaluationFrameBufferTest(context,  VK_FORMAT_R32_UINT, inputData, 2, DE_NULL, checkVertexPipelineStages, caseDef.shaderStage);
		default:											TCU_THROW(InternalError, "Unhandled shader stage");
	}
}

TestStatus test (Context& context, const CaseDefinition caseDef)
{
	const auto			secondBufferLayout	= ((caseDef.argType == ArgType::DYNAMIC)
											? subgroups::SSBOData::LayoutStd430
											: subgroups::SSBOData::LayoutStd140);
	const VkDeviceSize	secondBufferElems	= ((caseDef.argType == ArgType::DYNAMIC)
											? subgroups::maxSupportedSubgroupSize()
											: 1u);
	const auto			secondBufferType	= ((caseDef.argType == ArgType::DYNAMIC)
											? subgroups::SSBOData::BindingSSBO
											: subgroups::SSBOData::BindingUBO);

	const bool			isCompute			= isAllComputeStages(caseDef.shaderStage);
#ifndef CTS_USES_VULKANSC
	const bool			isMesh				= isAllMeshShadingStages(caseDef.shaderStage);
#else
	const bool			isMesh				= false;
#endif // CTS_USES_VULKANSC
	DE_ASSERT(!(isCompute && isMesh));

	if (isCompute || isMesh)
	{
#ifndef CTS_USES_VULKANSC
		const VkPhysicalDeviceSubgroupSizeControlProperties&	subgroupSizeControlProperties	= context.getSubgroupSizeControlProperties();
#else
		const VkPhysicalDeviceSubgroupSizeControlPropertiesEXT&	subgroupSizeControlProperties	= context.getSubgroupSizeControlPropertiesEXT();
#endif // CTS_USES_VULKANSC
		TestLog&												log								= context.getTestContext().getLog();
		const subgroups::SSBOData								inputData[2]
		{
			{
				subgroups::SSBOData::InitializeNonZero,	//  InputDataInitializeType		initializeType;
				subgroups::SSBOData::LayoutStd430,		//  InputDataLayoutType			layout;
				caseDef.format,							//  vk::VkFormat				format;
				subgroups::maxSupportedSubgroupSize(),	//  vk::VkDeviceSize			numElements;
			},
			{
				subgroups::SSBOData::InitializeNonZero,	//  InputDataInitializeType		initializeType;
				secondBufferLayout,						//  InputDataLayoutType			layout;
				VK_FORMAT_R32_UINT,						//  vk::VkFormat				format;
				secondBufferElems,						//  vk::VkDeviceSize			numElements;
				secondBufferType,
			},
		};

		if (caseDef.requiredSubgroupSize == DE_FALSE)
		{
			if (isCompute)
				return subgroups::makeComputeTest(context, VK_FORMAT_R32_UINT, inputData, 2, DE_NULL, checkComputeOrMesh);
			else
				return subgroups::makeMeshTest(context, VK_FORMAT_R32_UINT, inputData, 2, DE_NULL, checkComputeOrMesh);
		}

		log << TestLog::Message << "Testing required subgroup size range [" <<  subgroupSizeControlProperties.minSubgroupSize << ", "
			<< subgroupSizeControlProperties.maxSubgroupSize << "]" << TestLog::EndMessage;

		// According to the spec, requiredSubgroupSize must be a power-of-two integer.
		for (deUint32 size = subgroupSizeControlProperties.minSubgroupSize; size <= subgroupSizeControlProperties.maxSubgroupSize; size *= 2)
		{
			TestStatus result (QP_TEST_RESULT_INTERNAL_ERROR, "Internal Error");

			if (isCompute)
				result = subgroups::makeComputeTest(context, VK_FORMAT_R32_UINT, inputData, 2, DE_NULL, checkComputeOrMesh, size);
			else
				result = subgroups::makeMeshTest(context, VK_FORMAT_R32_UINT, inputData, 2, DE_NULL, checkComputeOrMesh, size);

			if (result.getCode() != QP_TEST_RESULT_PASS)
			{
				log << TestLog::Message << "subgroupSize " << size << " failed" << TestLog::EndMessage;
				return result;
			}
		}

		return TestStatus::pass("OK");
	}
	else if (isAllGraphicsStages(caseDef.shaderStage))
	{
		const VkShaderStageFlags	stages			= subgroups::getPossibleGraphicsSubgroupStages(context, caseDef.shaderStage);
		const subgroups::SSBOData	inputData[2]
		{
			{
				subgroups::SSBOData::InitializeNonZero,	//  InputDataInitializeType		initializeType;
				subgroups::SSBOData::LayoutStd430,		//  InputDataLayoutType			layout;
				caseDef.format,							//  vk::VkFormat				format;
				subgroups::maxSupportedSubgroupSize(),	//  vk::VkDeviceSize			numElements;
				subgroups::SSBOData::BindingSSBO,		//  bool						isImage;
				4u,										//  deUint32					binding;
				stages,									//  vk::VkShaderStageFlags		stages;
			},
			{
				subgroups::SSBOData::InitializeNonZero,	//  InputDataInitializeType		initializeType;
				secondBufferLayout,						//  InputDataLayoutType			layout;
				VK_FORMAT_R32_UINT,						//  vk::VkFormat				format;
				secondBufferElems,						//  vk::VkDeviceSize			numElements;
				secondBufferType,						//  bool						isImage;
				5u,										//  deUint32					binding;
				stages,									//  vk::VkShaderStageFlags		stages;
			},
		};

		return subgroups::allStages(context, VK_FORMAT_R32_UINT, inputData, 2, DE_NULL, checkVertexPipelineStages, stages);
	}
#ifndef CTS_USES_VULKANSC
	else if (isAllRayTracingStages(caseDef.shaderStage))
	{
		const VkShaderStageFlags	stages			= subgroups::getPossibleRayTracingSubgroupStages(context, caseDef.shaderStage);
		const subgroups::SSBOData	inputData[2]
		{
			{
				subgroups::SSBOData::InitializeNonZero,	//  InputDataInitializeType		initializeType;
				subgroups::SSBOData::LayoutStd430,		//  InputDataLayoutType			layout;
				caseDef.format,							//  vk::VkFormat				format;
				subgroups::maxSupportedSubgroupSize(),	//  vk::VkDeviceSize			numElements;
				subgroups::SSBOData::BindingSSBO,		//  bool						isImage;
				6u,										//  deUint32					binding;
				stages,									//  vk::VkShaderStageFlags		stages;
			},
			{
				subgroups::SSBOData::InitializeNonZero,	//  InputDataInitializeType		initializeType;
				secondBufferLayout,						//  InputDataLayoutType			layout;
				VK_FORMAT_R32_UINT,						//  vk::VkFormat				format;
				secondBufferElems,						//  vk::VkDeviceSize			numElements;
				secondBufferType,						//  bool						isImage;
				7u,										//  deUint32					binding;
				stages,									//  vk::VkShaderStageFlags		stages;
			},
		};

		return subgroups::allRayTracingStages(context, VK_FORMAT_R32_UINT, inputData, 2, DE_NULL, checkVertexPipelineStages, stages);
	}
#endif // CTS_USES_VULKANSC
	else
		TCU_THROW(InternalError, "Unknown stage or invalid stage set");
}
}

namespace vkt
{
namespace subgroups
{
TestCaseGroup* createSubgroupsShuffleTests (TestContext& testCtx)
{
	de::MovePtr<TestCaseGroup>	group				(new TestCaseGroup(testCtx, "shuffle"));

	de::MovePtr<TestCaseGroup>	graphicGroup		(new TestCaseGroup(testCtx, "graphics"));
	de::MovePtr<TestCaseGroup>	computeGroup		(new TestCaseGroup(testCtx, "compute"));
	de::MovePtr<TestCaseGroup>	framebufferGroup	(new TestCaseGroup(testCtx, "framebuffer"));
#ifndef CTS_USES_VULKANSC
	de::MovePtr<TestCaseGroup>	raytracingGroup		(new TestCaseGroup(testCtx, "ray_tracing"));
	de::MovePtr<TestCaseGroup>	meshGroup			(new TestCaseGroup(testCtx, "mesh"));
#endif // CTS_USES_VULKANSC

	const VkShaderStageFlags	fbStages[]			=
	{
		VK_SHADER_STAGE_VERTEX_BIT,
		VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,
		VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,
		VK_SHADER_STAGE_GEOMETRY_BIT,
	};

#ifndef CTS_USES_VULKANSC
	const VkShaderStageFlags	meshStages[]		=
	{
		VK_SHADER_STAGE_MESH_BIT_EXT,
		VK_SHADER_STAGE_TASK_BIT_EXT,
	};
#endif // CTS_USES_VULKANSC

	const deBool				boolValues[]		=
	{
		DE_FALSE,
		DE_TRUE
	};

	const struct
	{
		ArgType		argType;
		const char*	suffix;
	} argCases[] =
	{
		{	ArgType::DYNAMIC,				""						},
		{	ArgType::DYNAMICALLY_UNIFORM,	"_dynamically_uniform"	},
		{	ArgType::CONSTANT,				"_constant"				},
	};

	{
		const vector<VkFormat>	formats	= subgroups::getAllFormats();

		for (size_t formatIndex = 0; formatIndex < formats.size(); ++formatIndex)
		{
			const VkFormat	format					= formats[formatIndex];
			const string	formatName				= subgroups::getFormatNameForGLSL(format);
			const bool		needs8BitUBOStorage		= isFormat8bitTy(format);
			const bool		needs16BitUBOStorage	= isFormat16BitTy(format);

			for (int opTypeIndex = 0; opTypeIndex < OPTYPE_LAST; ++opTypeIndex)
			{
				for (const auto& argCase : argCases)
				{
					const OpType opType = static_cast<OpType>(opTypeIndex);

					if (opType == OPTYPE_SHUFFLE && argCase.argType != ArgType::DYNAMIC)
						continue;

					if ((opType == OPTYPE_ROTATE || opType == OPTYPE_CLUSTERED_ROTATE) &&
							argCase.argType == ArgType::DYNAMIC)
						continue;

					const string name = de::toLower(getOpTypeName(opType)) + "_" + formatName + argCase.suffix;

					{
						const CaseDefinition	caseDef		=
						{
							opType,							//  OpType				opType;
							VK_SHADER_STAGE_ALL_GRAPHICS,	//  VkShaderStageFlags	shaderStage;
							format,							//  VkFormat			format;
							de::SharedPtr<bool>(new bool),	//  de::SharedPtr<bool>	geometryPointSizeSupported;
							DE_FALSE,						//  deBool				requiredSubgroupSize;
							argCase.argType,				//  ArgType				argType;
							DE_FALSE,						//  deBool				requires8BitUniformBuffer;
							DE_FALSE						//  deBool				requires16BitUniformBuffer;
						};

						addFunctionCaseWithPrograms(graphicGroup.get(), name, supportedCheck, initPrograms, test, caseDef);
					}

					for (size_t groupSizeNdx = 0; groupSizeNdx < DE_LENGTH_OF_ARRAY(boolValues); ++groupSizeNdx)
					{
						const deBool			requiredSubgroupSize	= boolValues[groupSizeNdx];
						const string			testName				= name + (requiredSubgroupSize ? "_requiredsubgroupsize" : "");
						const CaseDefinition	caseDef					=
						{
							opType,							//  OpType				opType;
							VK_SHADER_STAGE_COMPUTE_BIT,	//  VkShaderStageFlags	shaderStage;
							format,							//  VkFormat			format;
							de::SharedPtr<bool>(new bool),	//  de::SharedPtr<bool>	geometryPointSizeSupported;
							requiredSubgroupSize,			//  deBool				requiredSubgroupSize;
							argCase.argType,				//  ArgType				argType;
							DE_FALSE,						//  deBool				requires8BitUniformBuffer;
							DE_FALSE						//  deBool				requires16BitUniformBuffer;
						};

						addFunctionCaseWithPrograms(computeGroup.get(), testName,supportedCheck, initPrograms, test, caseDef);
					}

#ifndef CTS_USES_VULKANSC
					for (size_t groupSizeNdx = 0; groupSizeNdx < DE_LENGTH_OF_ARRAY(boolValues); ++groupSizeNdx)
					{
						for (const auto& stage : meshStages)
						{
							const deBool			requiredSubgroupSize	= boolValues[groupSizeNdx];
							const string			testName				= name + (requiredSubgroupSize ? "_requiredsubgroupsize" : "") + "_" + getShaderStageName(stage);
							const CaseDefinition	caseDef					=
							{
								opType,							//  OpType				opType;
								stage,							//  VkShaderStageFlags	shaderStage;
								format,							//  VkFormat			format;
								de::SharedPtr<bool>(new bool),	//  de::SharedPtr<bool>	geometryPointSizeSupported;
								requiredSubgroupSize,			//  deBool				requiredSubgroupSize;
								argCase.argType,				//  ArgType				argType;
								DE_FALSE,						//  deBool				requires8BitUniformBuffer;
								DE_FALSE,						//  deBool				requires16BitUniformBuffer;
							};

							addFunctionCaseWithPrograms(meshGroup.get(), testName,supportedCheck, initPrograms, test, caseDef);
						}
					}
#endif // CTS_USES_VULKANSC

					for (int stageIndex = 0; stageIndex < DE_LENGTH_OF_ARRAY(fbStages); ++stageIndex)
					{
						const CaseDefinition	caseDef		=
						{
							opType,							//  OpType				opType;
							fbStages[stageIndex],			//  VkShaderStageFlags	shaderStage;
							format,							//  VkFormat			format;
							de::SharedPtr<bool>(new bool),	//  de::SharedPtr<bool>	geometryPointSizeSupported;
							DE_FALSE,						//  deBool				requiredSubgroupSize;
							argCase.argType,				//  ArgType				argType;
							deBool(needs8BitUBOStorage),	//  deBool				requires8BitUniformBuffer;
							deBool(needs16BitUBOStorage)	//  deBool				requires16BitUniformBuffer;
						};
						const string			testName	= name + "_" + getShaderStageName(caseDef.shaderStage);

						addFunctionCaseWithPrograms(framebufferGroup.get(), testName,supportedCheck, initFrameBufferPrograms, noSSBOtest, caseDef);
					}
				}
			}
		}
	}

#ifndef CTS_USES_VULKANSC
	{
		const vector<VkFormat>	formats	= subgroups::getAllRayTracingFormats();

		for (size_t formatIndex = 0; formatIndex < formats.size(); ++formatIndex)
		{
			const VkFormat	format		= formats[formatIndex];
			const string	formatName	= subgroups::getFormatNameForGLSL(format);

			for (int opTypeIndex = 0; opTypeIndex < OPTYPE_LAST; ++opTypeIndex)
			{
				for (const auto& argCase : argCases)
				{
					const OpType opType = static_cast<OpType>(opTypeIndex);

					if (opType == OPTYPE_SHUFFLE && argCase.argType != ArgType::DYNAMIC)
						continue;

					if ((opType == OPTYPE_ROTATE || opType == OPTYPE_CLUSTERED_ROTATE) &&
							argCase.argType == ArgType::DYNAMIC)
						continue;

					const string			name	= de::toLower(getOpTypeName(opType)) + "_" + formatName + argCase.suffix;
					const CaseDefinition	caseDef	=
					{
						opType,							//  OpType				opType;
						SHADER_STAGE_ALL_RAY_TRACING,	//  VkShaderStageFlags	shaderStage;
						format,							//  VkFormat			format;
						de::SharedPtr<bool>(new bool),	//  de::SharedPtr<bool>	geometryPointSizeSupported;
						DE_FALSE,						//  deBool				requiredSubgroupSize;
						argCase.argType,				//  ArgType				argType;
						DE_FALSE,						//  deBool				requires8BitUniformBuffer;
						DE_FALSE						//  deBool				requires16BitUniformBuffer;
					};

					addFunctionCaseWithPrograms(raytracingGroup.get(), name, supportedCheck, initPrograms, test, caseDef);
				}
			}
		}
	}
#endif // CTS_USES_VULKANSC

	group->addChild(graphicGroup.release());
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
