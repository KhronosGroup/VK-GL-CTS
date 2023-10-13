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
	OPTYPE_ALL			= 0,
	OPTYPE_ANY			= 1,
	OPTYPE_ALLEQUAL		= 2,
	OPTYPE_LAST_NON_ARB	= 3,
	OPTYPE_ALL_ARB		= 4,
	OPTYPE_ANY_ARB		= 5,
	OPTYPE_ALLEQUAL_ARB	= 6,
	OPTYPE_LAST
};

struct CaseDefinition
{
	OpType				opType;
	VkShaderStageFlags	shaderStage;
	VkFormat			format;
	de::SharedPtr<bool>	geometryPointSizeSupported;
	deBool				requiredSubgroupSize;
	deBool				requires8BitUniformBuffer;
	deBool				requires16BitUniformBuffer;
};

static bool checkVertexPipelineStages (const void*			internalData,
									   vector<const void*>	datas,
									   deUint32				width,
									   deUint32)
{
	DE_UNREF(internalData);

	return subgroups::check(datas, width, 0x1F);
}

static bool checkFragmentPipelineStages (const void*			internalData,
										 vector<const void*>	datas,
										 deUint32				width,
										 deUint32				height,
										 deUint32)
{
	DE_UNREF(internalData);

	const deUint32* data = reinterpret_cast<const deUint32*>(datas[0]);

	for (deUint32 x = 0u; x < width; ++x)
	{
		for (deUint32 y = 0u; y < height; ++y)
		{
			const deUint32 ndx = (x * height + y);
			const deUint32 val = data[ndx] & 0x1F;

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

static bool checkComputeOrMesh (const void*			internalData,
								vector<const void*>	datas,
								const deUint32		numWorkgroups[3],
								const deUint32		localSize[3],
								deUint32)
{
	DE_UNREF(internalData);

	return subgroups::checkComputeOrMesh(datas, numWorkgroups, localSize, 0x1F);
}

string getOpTypeName (int opType)
{
	switch (opType)
	{
		case OPTYPE_ALL:			return "subgroupAll";
		case OPTYPE_ANY:			return "subgroupAny";
		case OPTYPE_ALLEQUAL:		return "subgroupAllEqual";
		case OPTYPE_ALL_ARB:		return "allInvocationsARB";
		case OPTYPE_ANY_ARB:		return "anyInvocationARB";
		case OPTYPE_ALLEQUAL_ARB:	return "allInvocationsEqualARB";
		default:					TCU_THROW(InternalError, "Unsupported op type");
	}
}

bool fmtIsBoolean (VkFormat format)
{
	// For reasons unknown, the tests use R8_USCALED as the boolean format
	return	format == VK_FORMAT_R8_USCALED || format == VK_FORMAT_R8G8_USCALED ||
			format == VK_FORMAT_R8G8B8_USCALED || format == VK_FORMAT_R8G8B8A8_USCALED;
}

const string getExtensions (bool arbFunctions)
{
	return arbFunctions	?	"#extension GL_ARB_shader_group_vote: enable\n"
							"#extension GL_KHR_shader_subgroup_basic: enable\n"
						:	"#extension GL_KHR_shader_subgroup_vote: enable\n";
}

const string getStageTestSource (const CaseDefinition& caseDef)
{
	const bool		formatIsBoolean	= fmtIsBoolean(caseDef.format);
	const string	op				= getOpTypeName(caseDef.opType);
	const string	fmt				= subgroups::getFormatNameForGLSL(caseDef.format);
	const string	computePart		= isAllComputeStages(caseDef.shaderStage)
									? op + "(data[gl_SubgroupInvocationID] > 0) ? 0x4 : 0x0"
									: "0x4";

	return
		(OPTYPE_ALL == caseDef.opType || OPTYPE_ALL_ARB == caseDef.opType) ?
			"  tempRes = " + op + "(true) ? 0x1 : 0;\n"
			"  tempRes |= " + op + "(false) ? 0 : 0x1A;\n"
			"  tempRes |= " + computePart + ";\n"
		: (OPTYPE_ANY == caseDef.opType || OPTYPE_ANY_ARB == caseDef.opType) ?
			"  tempRes = " + op + "(true) ? 0x1 : 0;\n"
			"  tempRes |= " + op + "(false) ? 0 : 0x1A;\n"
			"  tempRes |= " + computePart + ";\n"
		: (OPTYPE_ALLEQUAL == caseDef.opType || OPTYPE_ALLEQUAL_ARB == caseDef.opType) ?
			"  " + fmt + " valueEqual = " + fmt + "(1.25 * float(data[gl_SubgroupInvocationID]) + 5.0);\n" +
			"  " + fmt + " valueNoEqual = " + fmt + (formatIsBoolean ? "(subgroupElect());\n" : "(gl_SubgroupInvocationID);\n") +
			"  tempRes = " + op + "(" + fmt + "(1)) ? 0x1 : 0;\n"
			"  tempRes |= "
				+ (formatIsBoolean ? "0x2" : op + "(" + fmt + "(gl_SubgroupInvocationID)) ? 0 : 0x2")
				+ ";\n"
			"  tempRes |= " + op + "(data[0]) ? 0x4 : 0;\n"
			"  tempRes |= " + op + "(valueEqual) ? 0x8 : 0x0;\n"
			"  tempRes |= " + op + "(valueNoEqual) ? 0x0 : 0x10;\n"
			"  if (subgroupElect()) tempRes |= 0x2 | 0x10;\n"
		: "";
}

void initFrameBufferPrograms (SourceCollections& programCollection, CaseDefinition caseDef)
{
#ifndef CTS_USES_VULKANSC
	const bool					spirv14required	= isAllRayTracingStages(caseDef.shaderStage);
#else
	const bool					spirv14required	= false;
#endif // CTS_USES_VULKANSC
	const SpirvVersion			spirvVersion	= spirv14required ? SPIRV_VERSION_1_4 : SPIRV_VERSION_1_3;
	const ShaderBuildOptions	buildOptions	(programCollection.usedVulkanVersion, spirvVersion, 0u);
	const bool					arbFunctions	= caseDef.opType > OPTYPE_LAST_NON_ARB;
	const string				extensions		= getExtensions(arbFunctions) + subgroups::getAdditionalExtensionForFormat(caseDef.format);
	const bool					pointSize		= *caseDef.geometryPointSizeSupported;

	subgroups::initStdFrameBufferPrograms(programCollection, buildOptions, caseDef.shaderStage, caseDef.format, pointSize, extensions, getStageTestSource(caseDef), "");
}

const string getStageTestSourceFrag (const CaseDefinition& caseDef)
{
	const bool		formatIsBoolean	= fmtIsBoolean(caseDef.format);
	const string	op				= getOpTypeName(caseDef.opType);
	const string	fmt				= subgroups::getFormatNameForGLSL(caseDef.format);

	return
		(OPTYPE_ALL == caseDef.opType || OPTYPE_ALL_ARB == caseDef.opType) ?
			"  tempRes |= " + op + "(!gl_HelperInvocation) ? 0x0 : 0x1;\n"
			"  tempRes |= " + op + "(false) ? 0 : 0x1A;\n"
			"  tempRes |= 0x4;\n"
		: (OPTYPE_ANY == caseDef.opType || OPTYPE_ANY_ARB == caseDef.opType) ?
			"  tempRes |= " + op + "(gl_HelperInvocation) ? 0x1 : 0x0;\n"
			"  tempRes |= " + op + "(false) ? 0 : 0x1A;\n"
			"  tempRes |= 0x4;\n"
		: (OPTYPE_ALLEQUAL == caseDef.opType || OPTYPE_ALLEQUAL_ARB == caseDef.opType) ?
			"  " + fmt + " valueEqual = " + fmt + "(1.25 * float(data[gl_SubgroupInvocationID]) + 5.0);\n" +
			"  " + fmt + " valueNoEqual = " + fmt + (formatIsBoolean ? "(subgroupElect());\n" : "(gl_SubgroupInvocationID);\n") +
			"  tempRes |= " + getOpTypeName(caseDef.opType) + "("
			+ fmt + "(1)) ? 0x10 : 0;\n"
			"  tempRes |= "
				+ (formatIsBoolean ? "0x2" : op + "(" + fmt + "(gl_SubgroupInvocationID)) ? 0 : 0x2")
				+ ";\n"
			"  tempRes |= " + op + "(data[0]) ? 0x4 : 0;\n"
			"  tempRes |= " + op + "(valueEqual) ? 0x8 : 0x0;\n"
			"  tempRes |= " + op + "(gl_HelperInvocation) ? 0x0 : 0x1;\n"
			"  if (subgroupElect()) tempRes |= 0x2 | 0x10;\n"
		: "";
}

void initFrameBufferProgramsFrag (SourceCollections& programCollection, CaseDefinition caseDef)
{
#ifndef CTS_USES_VULKANSC
	const bool					spirv14required	= isAllRayTracingStages(caseDef.shaderStage);
#else
	const bool					spirv14required	= false;
#endif // CTS_USES_VULKANSC
	const SpirvVersion			spirvVersion	= spirv14required ? SPIRV_VERSION_1_4 : SPIRV_VERSION_1_3;
	const ShaderBuildOptions	buildOptions	(programCollection.usedVulkanVersion, spirvVersion, 0u);
	const bool					arbFunctions	= caseDef.opType > OPTYPE_LAST_NON_ARB;
	const string				extensions		= getExtensions(arbFunctions) + subgroups::getAdditionalExtensionForFormat(caseDef.format);

	DE_ASSERT(VK_SHADER_STAGE_FRAGMENT_BIT == caseDef.shaderStage);

	{
		const string	vertex	=
			"#version 450\n"
			"void main (void)\n"
			"{\n"
			"  vec2 uv = vec2(float(gl_VertexIndex & 1), float((gl_VertexIndex >> 1) & 1));\n"
			"  gl_Position = vec4(uv * 4.0f -2.0f, 0.0f, 1.0f);\n"
			"  gl_PointSize = 1.0f;\n"
			"}\n";

		programCollection.glslSources.add("vert") << glu::VertexSource(vertex) << buildOptions;
	}

	{
		ostringstream	fragmentSource;

		fragmentSource << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450)<<"\n"
			<< extensions
			<< "layout(location = 0) out uint out_color;\n"
			<< "layout(set = 0, binding = 0) uniform Buffer1\n"
			<< "{\n"
			<< "  " << subgroups::getFormatNameForGLSL(caseDef.format) << " data[" << subgroups::maxSupportedSubgroupSize() << "];\n"
			<< "};\n"
			<< ""
			<< "void main()\n"
			<< "{\n"
			<< "  uint tempRes = 0u;\n"
			<< "  if (dFdx(gl_SubgroupInvocationID * gl_FragCoord.x * gl_FragCoord.y) - dFdy(gl_SubgroupInvocationID * gl_FragCoord.x * gl_FragCoord.y) > 0.0f)\n"
			<< "  {\n"
			<< "    tempRes |= 0x20;\n" // to be sure that compiler doesn't remove dFdx and dFdy executions
			<< "  }\n"
			<< (arbFunctions ?
				"  bool helper = anyInvocationARB(gl_HelperInvocation);\n" :
				"  bool helper = subgroupAny(gl_HelperInvocation);\n")
			<< "  if (helper)\n"
			<< "  {\n"
			<< "    tempRes |= 0x40;\n"
			<< "  }\n"
			<< getStageTestSourceFrag(caseDef)
			<< "  out_color = tempRes;\n"
			<< "}\n";

		programCollection.glslSources.add("fragment") << glu::FragmentSource(fragmentSource.str())<< buildOptions;
	}
}

void initPrograms (SourceCollections& programCollection, CaseDefinition caseDef)
{
#ifndef CTS_USES_VULKANSC
	const bool					spirv14required	= (isAllRayTracingStages(caseDef.shaderStage) || isAllMeshShadingStages(caseDef.shaderStage));
#else
	const bool					spirv14required	= false;
#endif // CTS_USES_VULKANSC
	const SpirvVersion			spirvVersion	= spirv14required ? SPIRV_VERSION_1_4 : SPIRV_VERSION_1_3;
	const ShaderBuildOptions	buildOptions	(programCollection.usedVulkanVersion, spirvVersion, 0u, spirv14required);
	const bool					arbFunctions	= caseDef.opType > OPTYPE_LAST_NON_ARB;
	const string				extensions		= getExtensions(arbFunctions) + subgroups::getAdditionalExtensionForFormat(caseDef.format);
	const bool					pointSize		= *caseDef.geometryPointSizeSupported;

	subgroups::initStdPrograms(programCollection, buildOptions, caseDef.shaderStage, caseDef.format, pointSize, extensions, getStageTestSource(caseDef), "");
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
		context.requireDeviceFunctionality("VK_EXT_subgroup_size_control");

#ifndef CTS_USES_VULKANSC
		const VkPhysicalDeviceSubgroupSizeControlFeatures&		subgroupSizeControlFeatures		= context.getSubgroupSizeControlFeatures();
		const VkPhysicalDeviceSubgroupSizeControlProperties&	subgroupSizeControlProperties	= context.getSubgroupSizeControlProperties();
#else
		const VkPhysicalDeviceSubgroupSizeControlFeaturesEXT&		subgroupSizeControlFeatures	= context.getSubgroupSizeControlFeaturesEXT();
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
	if (caseDef.opType > OPTYPE_LAST_NON_ARB)
	{
		context.requireDeviceFunctionality("VK_EXT_shader_subgroup_vote");
	}

	const subgroups::SSBOData::InputDataInitializeType	initializeType	= (OPTYPE_ALLEQUAL == caseDef.opType || OPTYPE_ALLEQUAL_ARB == caseDef.opType)
																		? subgroups::SSBOData::InitializeZero
																		: subgroups::SSBOData::InitializeNonZero;
	const subgroups::SSBOData							inputData
	{
		initializeType,							//  InputDataInitializeType		initializeType;
		subgroups::SSBOData::LayoutStd140,		//  InputDataLayoutType			layout;
		caseDef.format,							//  vk::VkFormat				format;
		subgroups::maxSupportedSubgroupSize(),	//  vk::VkDeviceSize			numElements;
		subgroups::SSBOData::BindingUBO,		//  BindingType					bindingType;
	};

	switch (caseDef.shaderStage)
	{
		case VK_SHADER_STAGE_VERTEX_BIT:					return subgroups::makeVertexFrameBufferTest(context, VK_FORMAT_R32_UINT, &inputData, 1, DE_NULL, checkVertexPipelineStages);
		case VK_SHADER_STAGE_GEOMETRY_BIT:					return subgroups::makeGeometryFrameBufferTest(context, VK_FORMAT_R32_UINT, &inputData, 1, DE_NULL, checkVertexPipelineStages);
		case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:		return subgroups::makeTessellationEvaluationFrameBufferTest(context, VK_FORMAT_R32_UINT, &inputData, 1, DE_NULL, checkVertexPipelineStages, caseDef.shaderStage);
		case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT:	return subgroups::makeTessellationEvaluationFrameBufferTest(context, VK_FORMAT_R32_UINT, &inputData, 1, DE_NULL, checkVertexPipelineStages, caseDef.shaderStage);
		case VK_SHADER_STAGE_FRAGMENT_BIT:					return subgroups::makeFragmentFrameBufferTest(context, VK_FORMAT_R32_UINT, &inputData, 1, DE_NULL, checkFragmentPipelineStages);
		default:											TCU_THROW(InternalError, "Unhandled shader stage");
	}
}

TestStatus test (Context& context, const CaseDefinition caseDef)
{
	const subgroups::SSBOData::InputDataInitializeType	initializeType	= (OPTYPE_ALLEQUAL == caseDef.opType || OPTYPE_ALLEQUAL_ARB == caseDef.opType)
																		? subgroups::SSBOData::InitializeZero
																		: subgroups::SSBOData::InitializeNonZero;

	const bool isCompute	= isAllComputeStages(caseDef.shaderStage);
#ifndef CTS_USES_VULKANSC
	const bool isMesh		= isAllMeshShadingStages(caseDef.shaderStage);
#else
	const bool isMesh		= false;
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
		const subgroups::SSBOData								inputData
		{
			initializeType,							//  InputDataInitializeType		initializeType;
			subgroups::SSBOData::LayoutStd430,		//  InputDataLayoutType			layout;
			caseDef.format,							//  vk::VkFormat				format;
			subgroups::maxSupportedSubgroupSize(),	//  vk::VkDeviceSize			numElements;
		};

		if (caseDef.requiredSubgroupSize == DE_FALSE)
		{
			if (isCompute)
				return subgroups::makeComputeTest(context, VK_FORMAT_R32_UINT, &inputData, 1, DE_NULL, checkComputeOrMesh);
			else
				return subgroups::makeMeshTest(context, VK_FORMAT_R32_UINT, &inputData, 1, DE_NULL, checkComputeOrMesh);
		}

		log << TestLog::Message << "Testing required subgroup size range [" <<  subgroupSizeControlProperties.minSubgroupSize << ", "
			<< subgroupSizeControlProperties.maxSubgroupSize << "]" << TestLog::EndMessage;

		// According to the spec, requiredSubgroupSize must be a power-of-two integer.
		for (deUint32 size = subgroupSizeControlProperties.minSubgroupSize; size <= subgroupSizeControlProperties.maxSubgroupSize; size *= 2)
		{
			TestStatus result (QP_TEST_RESULT_INTERNAL_ERROR, "Internal Error");

			if (isCompute)
				result = subgroups::makeComputeTest(context, VK_FORMAT_R32_UINT, &inputData, 1, DE_NULL, checkComputeOrMesh, size);
			else
				result = subgroups::makeMeshTest(context, VK_FORMAT_R32_UINT, &inputData, 1, DE_NULL, checkComputeOrMesh, size);

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
		const VkShaderStageFlags	stages		= subgroups::getPossibleGraphicsSubgroupStages(context, caseDef.shaderStage);
		const subgroups::SSBOData	inputData	=
		{
			initializeType,							//  InputDataInitializeType		initializeType;
			subgroups::SSBOData::LayoutStd430,		//  InputDataLayoutType			layout;
			caseDef.format,							//  vk::VkFormat				format;
			subgroups::maxSupportedSubgroupSize(),	//  vk::VkDeviceSize			numElements;
			subgroups::SSBOData::BindingSSBO,		//  bool						isImage;
			4u,										//  deUint32					binding;
			stages,									//  vk::VkShaderStageFlags		stages;
		};

		return subgroups::allStages(context, VK_FORMAT_R32_UINT, &inputData, 1, DE_NULL, checkVertexPipelineStages, stages);
	}
#ifndef CTS_USES_VULKANSC
	else if (isAllRayTracingStages(caseDef.shaderStage))
	{
		const VkShaderStageFlags	stages		= subgroups::getPossibleRayTracingSubgroupStages(context, caseDef.shaderStage);
		const subgroups::SSBOData	inputData	=
		{
			initializeType,							//  InputDataInitializeType		initializeType;
			subgroups::SSBOData::LayoutStd430,		//  InputDataLayoutType			layout;
			caseDef.format,							//  vk::VkFormat				format;
			subgroups::maxSupportedSubgroupSize(),	//  vk::VkDeviceSize			numElements;
			subgroups::SSBOData::BindingSSBO,		//  bool						isImage;
			6u,										//  deUint32					binding;
			stages,									//  vk::VkShaderStageFlags		stages;
		};

		return subgroups::allRayTracingStages(context, VK_FORMAT_R32_UINT, &inputData, 1, DE_NULL, checkVertexPipelineStages, stages);
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
TestCaseGroup* createSubgroupsVoteTests (TestContext& testCtx)
{
	de::MovePtr<TestCaseGroup>	group				(new TestCaseGroup(testCtx, "vote", "Subgroup vote category tests"));
	de::MovePtr<TestCaseGroup>	graphicGroup		(new TestCaseGroup(testCtx, "graphics", "Subgroup vote category tests: graphics"));
	de::MovePtr<TestCaseGroup>	computeGroup		(new TestCaseGroup(testCtx, "compute", "Subgroup vote category tests: compute"));
	de::MovePtr<TestCaseGroup>	framebufferGroup	(new TestCaseGroup(testCtx, "framebuffer", "Subgroup vote category tests: framebuffer"));
	de::MovePtr<TestCaseGroup>	fragHelperGroup		(new TestCaseGroup(testCtx, "frag_helper", "Subgroup vote category tests: fragment helper invocation"));
#ifndef CTS_USES_VULKANSC
	de::MovePtr<TestCaseGroup>	raytracingGroup		(new TestCaseGroup(testCtx, "ray_tracing", "Subgroup vote category tests: raytracing"));
	de::MovePtr<TestCaseGroup>	meshGroup			(new TestCaseGroup(testCtx, "mesh", "Subgroup vote category tests: mesh shading"));
	de::MovePtr<TestCaseGroup>	meshGroupARB		(new TestCaseGroup(testCtx, "mesh", "Subgroup vote category tests: mesh shading"));
#endif // CTS_USES_VULKANSC

	de::MovePtr<TestCaseGroup>	groupARB			(new TestCaseGroup(testCtx, "ext_shader_subgroup_vote", "VK_EXT_shader_subgroup_vote category tests"));
	de::MovePtr<TestCaseGroup>	graphicGroupARB		(new TestCaseGroup(testCtx, "graphics", "Subgroup vote category tests: graphics"));
	de::MovePtr<TestCaseGroup>	computeGroupARB		(new TestCaseGroup(testCtx, "compute", "Subgroup vote category tests: compute"));
	de::MovePtr<TestCaseGroup>	framebufferGroupARB	(new TestCaseGroup(testCtx, "framebuffer", "Subgroup vote category tests: framebuffer"));
	de::MovePtr<TestCaseGroup>	fragHelperGroupARB	(new TestCaseGroup(testCtx, "frag_helper", "Subgroup vote category tests: fragment helper invocation"));
	const deBool				boolValues[]		=
	{
		DE_FALSE,
		DE_TRUE
	};

	{
		const VkShaderStageFlags	fbStages[]		=
		{
			VK_SHADER_STAGE_VERTEX_BIT,
			VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,
			VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,
			VK_SHADER_STAGE_GEOMETRY_BIT,
		};
#ifndef CTS_USES_VULKANSC
		const VkShaderStageFlags	meshStages[]	=
		{
			VK_SHADER_STAGE_MESH_BIT_EXT,
			VK_SHADER_STAGE_TASK_BIT_EXT,
		};
#endif // CTS_USES_VULKANSC
		const vector<VkFormat>		formats		= subgroups::getAllFormats();

		for (size_t formatIndex = 0; formatIndex < formats.size(); ++formatIndex)
		{
			const VkFormat	format					= formats[formatIndex];
			const bool		needs8BitUBOStorage		= isFormat8bitTy(format);
			const bool		needs16BitUBOStorage	= isFormat16BitTy(format);
			const deBool	formatIsNotVector		=  format == VK_FORMAT_R8_USCALED
													|| format == VK_FORMAT_R32_UINT
													|| format == VK_FORMAT_R32_SINT
													|| format == VK_FORMAT_R32_SFLOAT
													|| format == VK_FORMAT_R64_SFLOAT;

			for (int opTypeIndex = 0; opTypeIndex < OPTYPE_LAST; ++opTypeIndex)
			{
				const OpType	opType	= static_cast<OpType>(opTypeIndex);

				// Skip OPTYPE_LAST_NON_ARB because it is not a real op type.
				if (opType == OPTYPE_LAST_NON_ARB)
					continue;

				// Skip the non-nonvector tests because VK_EXT_shader_subgroup_vote functions only supports boolean scalar arguments.
				if (opType > OPTYPE_LAST_NON_ARB && !formatIsNotVector)
					continue;

				// Skip non-boolean formats when testing allInvocationsEqualARB(bool value), because it requires a boolean
				// argument that should have the same value for all invocations. For the rest of formats, it won't be a boolean argument,
				// so it may give wrong results when converting to bool.
				if (opType == OPTYPE_ALLEQUAL_ARB && format != VK_FORMAT_R8_USCALED)
					continue;

				// Skip the typed tests for all but subgroupAllEqual() and allInvocationsEqualARB()
				if ((VK_FORMAT_R32_UINT != format) && (OPTYPE_ALLEQUAL != opType) && (OPTYPE_ALLEQUAL_ARB != opType))
				{
					continue;
				}

				const string	op					= de::toLower(getOpTypeName(opType));
				const string	name				= op + "_" + subgroups::getFormatNameForGLSL(format);
				const bool		opNonARB			= (opType < OPTYPE_LAST_NON_ARB);
				TestCaseGroup*	computeGroupPtr		= opNonARB ? computeGroup.get() : computeGroupARB.get();
				TestCaseGroup*	graphicGroupPtr		= opNonARB ? graphicGroup.get() : graphicGroupARB.get();
				TestCaseGroup*	framebufferGroupPtr	= opNonARB ? framebufferGroup.get() : framebufferGroupARB.get();
				TestCaseGroup*	fragHelperGroupPtr	= opNonARB ? fragHelperGroup.get() : fragHelperGroupARB.get();
#ifndef CTS_USES_VULKANSC
				TestCaseGroup*	meshGroupPtr		= opNonARB ? meshGroup.get() : meshGroupARB.get();
#endif // CTS_USES_VULKANSC

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
						deBool(false),					//  deBool				requires8BitUniformBuffer;
						deBool(false)					//  deBool				requires16BitUniformBuffer;
					};

					addFunctionCaseWithPrograms(computeGroupPtr, testName, "", supportedCheck, initPrograms, test, caseDef);
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
							deBool(false),					//  deBool				requires8BitUniformBuffer;
							deBool(false)					//  deBool				requires16BitUniformBuffer;
						};

						addFunctionCaseWithPrograms(meshGroupPtr, testName, "", supportedCheck, initPrograms, test, caseDef);
					}
				}
#endif // CTS_USES_VULKANSC

				{
					const CaseDefinition	caseDef		=
					{
						opType,							//  OpType				opType;
						VK_SHADER_STAGE_ALL_GRAPHICS,	//  VkShaderStageFlags	shaderStage;
						format,							//  VkFormat			format;
						de::SharedPtr<bool>(new bool),	//  de::SharedPtr<bool>	geometryPointSizeSupported;
						DE_FALSE,						//  deBool				requiredSubgroupSize;
						deBool(false),					//  deBool				requires8BitUniformBuffer;
						deBool(false)					//  deBool				requires16BitUniformBuffer;
					};

					addFunctionCaseWithPrograms(graphicGroupPtr, name, "", supportedCheck, initPrograms, test, caseDef);
				}

				for (int stageIndex = 0; stageIndex < DE_LENGTH_OF_ARRAY(fbStages); ++stageIndex)
				{
					const CaseDefinition	caseDef		=
					{
						opType,							//  OpType				opType;
						fbStages[stageIndex],			//  VkShaderStageFlags	shaderStage;
						format,							//  VkFormat			format;
						de::SharedPtr<bool>(new bool),	//  de::SharedPtr<bool>	geometryPointSizeSupported;
						DE_FALSE,						//  deBool				requiredSubgroupSize;
						deBool(needs8BitUBOStorage),	//  deBool				requires8BitUniformBuffer;
						deBool(needs16BitUBOStorage)	//  deBool				requires16BitUniformBuffer;
					};
					const string			testName	= name + "_" + getShaderStageName(caseDef.shaderStage);

					addFunctionCaseWithPrograms(framebufferGroupPtr, testName, "", supportedCheck, initFrameBufferPrograms, noSSBOtest, caseDef);
				}

				{
					const CaseDefinition	caseDef		=
					{
						opType,							//  OpType				opType;
						VK_SHADER_STAGE_FRAGMENT_BIT,	//  VkShaderStageFlags	shaderStage;
						format,							//  VkFormat			format;
						de::SharedPtr<bool>(new bool),	//  de::SharedPtr<bool>	geometryPointSizeSupported;
						DE_FALSE,						//  deBool				requiredSubgroupSize;
						deBool(needs8BitUBOStorage),	//  deBool				requires8BitUniformBuffer;
						deBool(needs16BitUBOStorage)	//  deBool				requires16BitUniformBuffer;
					};
					const string			testName	= name + "_" + getShaderStageName(caseDef.shaderStage);

					addFunctionCaseWithPrograms(fragHelperGroupPtr, testName, "", supportedCheck, initFrameBufferProgramsFrag, noSSBOtest, caseDef);
				}
			}
		}
	}

#ifndef CTS_USES_VULKANSC
	{
		const vector<VkFormat>	formats		= subgroups::getAllRayTracingFormats();

		for (size_t formatIndex = 0; formatIndex < formats.size(); ++formatIndex)
		{
			const VkFormat	format	= formats[formatIndex];

			for (int opTypeIndex = 0; opTypeIndex < OPTYPE_LAST_NON_ARB; ++opTypeIndex)
			{
				const OpType	opType	= static_cast<OpType>(opTypeIndex);

				// Skip the typed tests for all but subgroupAllEqual()
				if ((VK_FORMAT_R32_UINT != format) && (OPTYPE_ALLEQUAL != opType))
				{
					continue;
				}

				const string			op		= de::toLower(getOpTypeName(opType));
				const string			name	= op + "_" + subgroups::getFormatNameForGLSL(format);
				const CaseDefinition	caseDef	=
				{
					opType,							//  OpType				opType;
					SHADER_STAGE_ALL_RAY_TRACING,	//  VkShaderStageFlags	shaderStage;
					format,							//  VkFormat			format;
					de::SharedPtr<bool>(new bool),	//  de::SharedPtr<bool>	geometryPointSizeSupported;
					DE_FALSE,						//  deBool				requiredSubgroupSize;
					DE_FALSE,						//  deBool				requires8BitUniformBuffer;
					DE_FALSE						//  deBool				requires16BitUniformBuffer;
				};

				addFunctionCaseWithPrograms(raytracingGroup.get(), name, "", supportedCheck, initPrograms, test, caseDef);
			}
		}
	}
#endif // CTS_USES_VULKANSC

	groupARB->addChild(graphicGroupARB.release());
	groupARB->addChild(computeGroupARB.release());
	groupARB->addChild(framebufferGroupARB.release());
	groupARB->addChild(fragHelperGroupARB.release());

	group->addChild(graphicGroup.release());
	group->addChild(computeGroup.release());
	group->addChild(framebufferGroup.release());
	group->addChild(fragHelperGroup.release());
#ifndef CTS_USES_VULKANSC
	group->addChild(raytracingGroup.release());
	group->addChild(meshGroup.release());
	groupARB->addChild(meshGroupARB.release());
#endif // CTS_USES_VULKANSC

	group->addChild(groupARB.release());

	return group.release();
}

} // subgroups
} // vkt
