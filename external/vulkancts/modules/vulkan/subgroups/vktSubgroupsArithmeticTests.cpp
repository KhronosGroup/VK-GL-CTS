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

#include "vktSubgroupsArithmeticTests.hpp"
#include "vktSubgroupsScanHelpers.hpp"
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
	OPTYPE_ADD = 0,
	OPTYPE_MUL,
	OPTYPE_MIN,
	OPTYPE_MAX,
	OPTYPE_AND,
	OPTYPE_OR,
	OPTYPE_XOR,
	OPTYPE_INCLUSIVE_ADD,
	OPTYPE_INCLUSIVE_MUL,
	OPTYPE_INCLUSIVE_MIN,
	OPTYPE_INCLUSIVE_MAX,
	OPTYPE_INCLUSIVE_AND,
	OPTYPE_INCLUSIVE_OR,
	OPTYPE_INCLUSIVE_XOR,
	OPTYPE_EXCLUSIVE_ADD,
	OPTYPE_EXCLUSIVE_MUL,
	OPTYPE_EXCLUSIVE_MIN,
	OPTYPE_EXCLUSIVE_MAX,
	OPTYPE_EXCLUSIVE_AND,
	OPTYPE_EXCLUSIVE_OR,
	OPTYPE_EXCLUSIVE_XOR,
	OPTYPE_LAST
};

struct CaseDefinition
{
	Operator			op;
	ScanType			scanType;
	VkShaderStageFlags	shaderStage;
	VkFormat			format;
	de::SharedPtr<bool>	geometryPointSizeSupported;
	deBool				requiredSubgroupSize;
	deBool				requires8BitUniformBuffer;
	deBool				requires16BitUniformBuffer;
};

static Operator getOperator (OpType opType)
{
	switch (opType)
	{
		case OPTYPE_ADD:
		case OPTYPE_INCLUSIVE_ADD:
		case OPTYPE_EXCLUSIVE_ADD:
			return OPERATOR_ADD;
		case OPTYPE_MUL:
		case OPTYPE_INCLUSIVE_MUL:
		case OPTYPE_EXCLUSIVE_MUL:
			return OPERATOR_MUL;
		case OPTYPE_MIN:
		case OPTYPE_INCLUSIVE_MIN:
		case OPTYPE_EXCLUSIVE_MIN:
			return OPERATOR_MIN;
		case OPTYPE_MAX:
		case OPTYPE_INCLUSIVE_MAX:
		case OPTYPE_EXCLUSIVE_MAX:
			return OPERATOR_MAX;
		case OPTYPE_AND:
		case OPTYPE_INCLUSIVE_AND:
		case OPTYPE_EXCLUSIVE_AND:
			return OPERATOR_AND;
		case OPTYPE_OR:
		case OPTYPE_INCLUSIVE_OR:
		case OPTYPE_EXCLUSIVE_OR:
			return OPERATOR_OR;
		case OPTYPE_XOR:
		case OPTYPE_INCLUSIVE_XOR:
		case OPTYPE_EXCLUSIVE_XOR:
			return OPERATOR_XOR;
		default:
			DE_FATAL("Unsupported op type");
			return OPERATOR_ADD;
	}
}

static ScanType getScanType(OpType opType)
{
	switch (opType)
	{
		case OPTYPE_ADD:
		case OPTYPE_MUL:
		case OPTYPE_MIN:
		case OPTYPE_MAX:
		case OPTYPE_AND:
		case OPTYPE_OR:
		case OPTYPE_XOR:
			return SCAN_REDUCE;
		case OPTYPE_INCLUSIVE_ADD:
		case OPTYPE_INCLUSIVE_MUL:
		case OPTYPE_INCLUSIVE_MIN:
		case OPTYPE_INCLUSIVE_MAX:
		case OPTYPE_INCLUSIVE_AND:
		case OPTYPE_INCLUSIVE_OR:
		case OPTYPE_INCLUSIVE_XOR:
			return SCAN_INCLUSIVE;
		case OPTYPE_EXCLUSIVE_ADD:
		case OPTYPE_EXCLUSIVE_MUL:
		case OPTYPE_EXCLUSIVE_MIN:
		case OPTYPE_EXCLUSIVE_MAX:
		case OPTYPE_EXCLUSIVE_AND:
		case OPTYPE_EXCLUSIVE_OR:
		case OPTYPE_EXCLUSIVE_XOR:
			return SCAN_EXCLUSIVE;
		default:
			DE_FATAL("Unsupported op type");
			return SCAN_REDUCE;
	}
}

static bool checkVertexPipelineStages (const void*			internalData,
									   vector<const void*>	datas,
									   deUint32				width,
									   deUint32)
{
	DE_UNREF(internalData);

	return subgroups::check(datas, width, 0x3);
}

static bool checkComputeOrMesh (const void*			internalData,
								vector<const void*>	datas,
								const deUint32		numWorkgroups[3],
								const deUint32		localSize[3],
								deUint32)
{
	DE_UNREF(internalData);

	return subgroups::checkComputeOrMesh(datas, numWorkgroups, localSize, 0x3);
}

string getOpTypeName (Operator op, ScanType scanType)
{
	return getScanOpName("subgroup", "", op, scanType);
}

string getExtHeader (const CaseDefinition& caseDef)
{
	return	"#extension GL_KHR_shader_subgroup_arithmetic: enable\n"
			"#extension GL_KHR_shader_subgroup_ballot: enable\n" +
			subgroups::getAdditionalExtensionForFormat(caseDef.format);
}

string getIndexVars (const CaseDefinition& caseDef)
{
	switch (caseDef.scanType)
	{
		case SCAN_REDUCE:		return "  uint start = 0, end = gl_SubgroupSize;\n";
		case SCAN_INCLUSIVE:	return "  uint start = 0, end = gl_SubgroupInvocationID + 1;\n";
		case SCAN_EXCLUSIVE:	return "  uint start = 0, end = gl_SubgroupInvocationID;\n";
		default:				TCU_THROW(InternalError, "Unreachable");
	}
}

string getTestSrc (const CaseDefinition& caseDef)
{
	const string indexVars = getIndexVars(caseDef);

	return	"  uvec4 mask = subgroupBallot(true);\n"
			+ indexVars +
			"  " + subgroups::getFormatNameForGLSL(caseDef.format) + " ref = "
			+ getIdentity(caseDef.op, caseDef.format) + ";\n"
			"  tempRes = 0;\n"
			"  for (uint index = start; index < end; index++)\n"
			"  {\n"
			"    if (subgroupBallotBitExtract(mask, index))\n"
			"    {\n"
			"      ref = " + getOpOperation(caseDef.op, caseDef.format, "ref", "data[index]") + ";\n"
			"    }\n"
			"  }\n"
			"  tempRes = " + getCompare(caseDef.op, caseDef.format, "ref", getOpTypeName(caseDef.op, caseDef.scanType) + "(data[gl_SubgroupInvocationID])") + " ? 0x1 : 0;\n"
			"  if (1 == (gl_SubgroupInvocationID % 2))\n"
			"  {\n"
			"    mask = subgroupBallot(true);\n"
			"    ref = " + getIdentity(caseDef.op, caseDef.format) + ";\n"
			"    for (uint index = start; index < end; index++)\n"
			"    {\n"
			"      if (subgroupBallotBitExtract(mask, index))\n"
			"      {\n"
			"        ref = " + getOpOperation(caseDef.op, caseDef.format, "ref", "data[index]") + ";\n"
			"      }\n"
			"    }\n"
			"    tempRes |= " + getCompare(caseDef.op, caseDef.format, "ref", getOpTypeName(caseDef.op, caseDef.scanType) + "(data[gl_SubgroupInvocationID])") + " ? 0x2 : 0;\n"
			"  }\n"
			"  else\n"
			"  {\n"
			"    tempRes |= 0x2;\n"
			"  }\n";
}

void initFrameBufferPrograms (SourceCollections& programCollection, CaseDefinition caseDef)
{
	const ShaderBuildOptions	buildOptions	(programCollection.usedVulkanVersion, SPIRV_VERSION_1_3, 0u);
	const string				extHeader		= getExtHeader(caseDef);
	const string				testSrc			= getTestSrc(caseDef);

	subgroups::initStdFrameBufferPrograms(programCollection, buildOptions, caseDef.shaderStage, caseDef.format, *caseDef.geometryPointSizeSupported, extHeader, testSrc, "");
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
	const string				extHeader		= getExtHeader(caseDef);
	const string				testSrc			= getTestSrc(caseDef);

	subgroups::initStdPrograms(programCollection, buildOptions, caseDef.shaderStage, caseDef.format, *caseDef.geometryPointSizeSupported, extHeader, testSrc, "");
}

void supportedCheck (Context& context, CaseDefinition caseDef)
{
	if (!subgroups::isSubgroupSupported(context))
		TCU_THROW(NotSupportedError, "Subgroup operations are not supported");

	if (!subgroups::isSubgroupFeatureSupportedForDevice(context, VK_SUBGROUP_FEATURE_ARITHMETIC_BIT))
		TCU_THROW(NotSupportedError, "Device does not support subgroup arithmetic operations");

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
	const subgroups::SSBOData	inputData	=
	{
		subgroups::SSBOData::InitializeNonZero,	//  InputDataInitializeType		initializeType;
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
		case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT:	return subgroups::makeTessellationEvaluationFrameBufferTest(context,  VK_FORMAT_R32_UINT, &inputData, 1, DE_NULL, checkVertexPipelineStages, caseDef.shaderStage);
		default:											TCU_THROW(InternalError, "Unhandled shader stage");
	}
}

TestStatus test (Context& context, const CaseDefinition caseDef)
{
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
		const subgroups::SSBOData								inputData						=
		{
			subgroups::SSBOData::InitializeNonZero,	//  InputDataInitializeType		initializeType;
			subgroups::SSBOData::LayoutStd430,		//  InputDataLayoutType			layout;
			caseDef.format,							//  vk::VkFormat				format;
			subgroups::maxSupportedSubgroupSize(),	//  vk::VkDeviceSize			numElements;
		};

		if (caseDef.requiredSubgroupSize == DE_FALSE)
		{
			if (isMesh)
				return subgroups::makeMeshTest(context, VK_FORMAT_R32_UINT, &inputData, 1, DE_NULL, checkComputeOrMesh);
			else
				return subgroups::makeComputeTest(context, VK_FORMAT_R32_UINT, &inputData, 1, DE_NULL, checkComputeOrMesh);
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
			subgroups::SSBOData::InitializeNonZero,	//  InputDataInitializeType		initializeType;
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
			subgroups::SSBOData::InitializeNonZero,	//  InputDataInitializeType		initializeType;
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
TestCaseGroup* createSubgroupsArithmeticTests (TestContext& testCtx)
{
	de::MovePtr<TestCaseGroup>	group				(new TestCaseGroup(testCtx, "arithmetic", "Subgroup arithmetic category tests"));

	de::MovePtr<TestCaseGroup>	graphicGroup		(new TestCaseGroup(testCtx, "graphics", "Subgroup arithmetic category tests: graphics"));
	de::MovePtr<TestCaseGroup>	computeGroup		(new TestCaseGroup(testCtx, "compute", "Subgroup arithmetic category tests: compute"));
	de::MovePtr<TestCaseGroup>	framebufferGroup	(new TestCaseGroup(testCtx, "framebuffer", "Subgroup arithmetic category tests: framebuffer"));
#ifndef CTS_USES_VULKANSC
	de::MovePtr<TestCaseGroup>	raytracingGroup		(new TestCaseGroup(testCtx, "ray_tracing", "Subgroup arithmetic category tests: ray tracing"));
	de::MovePtr<TestCaseGroup>	meshGroup			(new TestCaseGroup(testCtx, "mesh", "Subgroup arithmetic category tests: mesh"));
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

	{
		const vector<VkFormat>		formats		= subgroups::getAllFormats();

		for (size_t formatIndex = 0; formatIndex < formats.size(); ++formatIndex)
		{
			const VkFormat	format					= formats[formatIndex];
			const string	formatName				= subgroups::getFormatNameForGLSL(format);
			const bool		isBool					= subgroups::isFormatBool(format);
			const bool		isFloat					= subgroups::isFormatFloat(format);
			const bool		needs8BitUBOStorage		= isFormat8bitTy(format);
			const bool		needs16BitUBOStorage	= isFormat16BitTy(format);

			for (int opTypeIndex = 0; opTypeIndex < OPTYPE_LAST; ++opTypeIndex)
			{
				const OpType	opType		= static_cast<OpType>(opTypeIndex);
				const Operator	op			= getOperator(opType);
				const ScanType	st			= getScanType(opType);
				const bool		isBitwiseOp	= (op == OPERATOR_AND || op == OPERATOR_OR || op == OPERATOR_XOR);

				// Skip float with bitwise category.
				if (isFloat && isBitwiseOp)
					continue;

				// Skip bool when its not the bitwise category.
				if (isBool && !isBitwiseOp)
					continue;

				const string	name	= de::toLower(getOpTypeName(op, st)) + "_" + formatName;

				for (size_t groupSizeNdx = 0; groupSizeNdx < DE_LENGTH_OF_ARRAY(boolValues); ++groupSizeNdx)
				{
					const deBool			requiredSubgroupSize	= boolValues[groupSizeNdx];
					const string			testName				= name + (requiredSubgroupSize ? "_requiredsubgroupsize" : "");
					const CaseDefinition	caseDef					=
					{
						op,								//  Operator			op;
						st,								//  ScanType			scanType;
						VK_SHADER_STAGE_COMPUTE_BIT,	//  VkShaderStageFlags	shaderStage;
						format,							//  VkFormat			format;
						de::SharedPtr<bool>(new bool),	//  de::SharedPtr<bool>	geometryPointSizeSupported;
						requiredSubgroupSize,			//  deBool				requiredSubgroupSize;
						DE_FALSE,						//  deBool				requires8BitUniformBuffer;
						DE_FALSE						//  deBool				requires16BitUniformBuffer;
					};

					addFunctionCaseWithPrograms(computeGroup.get(), testName, "", supportedCheck, initPrograms, test, caseDef);
				}

#ifndef CTS_USES_VULKANSC
				for (size_t groupSizeNdx = 0; groupSizeNdx < DE_LENGTH_OF_ARRAY(boolValues); ++groupSizeNdx)
				{
					for (const auto& meshStage : meshStages)
					{
						const deBool			requiredSubgroupSize	= boolValues[groupSizeNdx];
						const string			testName				= name + "_" + getShaderStageName(meshStage) + (requiredSubgroupSize ? "_requiredsubgroupsize" : "");
						const CaseDefinition	caseDef					=
						{
							op,								//  Operator			op;
							st,								//  ScanType			scanType;
							meshStage,						//  VkShaderStageFlags	shaderStage;
							format,							//  VkFormat			format;
							de::SharedPtr<bool>(new bool),	//  de::SharedPtr<bool>	geometryPointSizeSupported;
							requiredSubgroupSize,			//  deBool				requiredSubgroupSize;
							DE_FALSE,						//  deBool				requires8BitUniformBuffer;
							DE_FALSE						//  deBool				requires16BitUniformBuffer;
						};

						addFunctionCaseWithPrograms(meshGroup.get(), testName, "", supportedCheck, initPrograms, test, caseDef);
					}
				}
#endif // CTS_USES_VULKANSC

				{
					const CaseDefinition	caseDef	=
					{
						op,								//  Operator			op;
						st,								//  ScanType			scanType;
						VK_SHADER_STAGE_ALL_GRAPHICS,	//  VkShaderStageFlags	shaderStage;
						format,							//  VkFormat			format;
						de::SharedPtr<bool>(new bool),	//  de::SharedPtr<bool>	geometryPointSizeSupported;
						DE_FALSE,						//  deBool				requiredSubgroupSize;
						DE_FALSE,						//  deBool				requires8BitUniformBuffer;
						DE_FALSE						//  deBool				requires16BitUniformBuffer;
					};

					addFunctionCaseWithPrograms(graphicGroup.get(), name, "", supportedCheck, initPrograms, test, caseDef);
				}

				for (int stageIndex = 0; stageIndex < DE_LENGTH_OF_ARRAY(fbStages); ++stageIndex)
				{
					const CaseDefinition	caseDef		=
					{
						op,								//  Operator			op;
						st,								//  ScanType			scanType;
						fbStages[stageIndex],			//  VkShaderStageFlags	shaderStage;
						format,							//  VkFormat			format;
						de::SharedPtr<bool>(new bool),	//  de::SharedPtr<bool>	geometryPointSizeSupported;
						DE_FALSE,						//  deBool				requiredSubgroupSize;
						deBool(needs8BitUBOStorage),	//  deBool				requires8BitUniformBuffer;
						deBool(needs16BitUBOStorage)	//  deBool				requires16BitUniformBuffer;
					};
					const string			testName	= name + "_" + getShaderStageName(caseDef.shaderStage);

					addFunctionCaseWithPrograms(framebufferGroup.get(), testName, "", supportedCheck, initFrameBufferPrograms, noSSBOtest, caseDef);
				}
			}
		}
	}

#ifndef CTS_USES_VULKANSC
	{
		const vector<VkFormat>		formats		= subgroups::getAllRayTracingFormats();

		for (size_t formatIndex = 0; formatIndex < formats.size(); ++formatIndex)
		{
			const VkFormat	format		= formats[formatIndex];
			const string	formatName	= subgroups::getFormatNameForGLSL(format);
			const bool		isBool		= subgroups::isFormatBool(format);
			const bool		isFloat		= subgroups::isFormatFloat(format);

			for (int opTypeIndex = 0; opTypeIndex < OPTYPE_LAST; ++opTypeIndex)
			{
				const OpType	opType		= static_cast<OpType>(opTypeIndex);
				const Operator	op			= getOperator(opType);
				const ScanType	st			= getScanType(opType);
				const bool		isBitwiseOp	= (op == OPERATOR_AND || op == OPERATOR_OR || op == OPERATOR_XOR);

				// Skip float with bitwise category.
				if (isFloat && isBitwiseOp)
					continue;

				// Skip bool when its not the bitwise category.
				if (isBool && !isBitwiseOp)
					continue;

				{
					const CaseDefinition	caseDef	=
					{
						op,								//  Operator			op;
						st,								//  ScanType			scanType;
						SHADER_STAGE_ALL_RAY_TRACING,	//  VkShaderStageFlags	shaderStage;
						format,							//  VkFormat			format;
						de::SharedPtr<bool>(new bool),	//  de::SharedPtr<bool>	geometryPointSizeSupported;
						DE_FALSE,						//  deBool				requiredSubgroupSize;
						DE_FALSE,						//  deBool				requires8BitUniformBuffer;
						DE_FALSE						//  deBool				requires16BitUniformBuffer;
					};
					const string			name	= de::toLower(getOpTypeName(op, st)) + "_" + formatName;

					addFunctionCaseWithPrograms(raytracingGroup.get(), name, "", supportedCheck, initPrograms, test, caseDef);
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
