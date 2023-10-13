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

#include "vktSubgroupsQuadTests.hpp"
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
	OPTYPE_QUAD_BROADCAST = 0,
	OPTYPE_QUAD_BROADCAST_NONCONST,
	OPTYPE_QUAD_SWAP_HORIZONTAL,
	OPTYPE_QUAD_SWAP_VERTICAL,
	OPTYPE_QUAD_SWAP_DIAGONAL,
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
		case OPTYPE_QUAD_BROADCAST:				return "subgroupQuadBroadcast";
		case OPTYPE_QUAD_BROADCAST_NONCONST:	return "subgroupQuadBroadcast";
		case OPTYPE_QUAD_SWAP_HORIZONTAL:		return "subgroupQuadSwapHorizontal";
		case OPTYPE_QUAD_SWAP_VERTICAL:			return "subgroupQuadSwapVertical";
		case OPTYPE_QUAD_SWAP_DIAGONAL:			return "subgroupQuadSwapDiagonal";
		default:								TCU_THROW(InternalError, "Unsupported op type");
	}
}

string getOpTypeCaseName (OpType opType)
{
	switch (opType)
	{
		case OPTYPE_QUAD_BROADCAST:				return "subgroupquadbroadcast";
		case OPTYPE_QUAD_BROADCAST_NONCONST:	return "subgroupquadbroadcast_nonconst";
		case OPTYPE_QUAD_SWAP_HORIZONTAL:		return "subgroupquadswaphorizontal";
		case OPTYPE_QUAD_SWAP_VERTICAL:			return "subgroupquadswapvertical";
		case OPTYPE_QUAD_SWAP_DIAGONAL:			return "subgroupquadswapdiagonal";
		default:								TCU_THROW(InternalError, "Unsupported op type");
	}
}

string getExtHeader (VkFormat format)
{
	return	"#extension GL_KHR_shader_subgroup_quad: enable\n"
			"#extension GL_KHR_shader_subgroup_ballot: enable\n" +
			subgroups::getAdditionalExtensionForFormat(format);
}

string getTestSrc (const CaseDefinition &caseDef)
{
	const string	swapTable[OPTYPE_LAST]	=
	{
		"",
		"",
		"  const uint swapTable[4] = {1, 0, 3, 2};\n",
		"  const uint swapTable[4] = {2, 3, 0, 1};\n",
		"  const uint swapTable[4] = {3, 2, 1, 0};\n",
	};
	const string	validate				=
		"  if (subgroupBallotBitExtract(mask, otherID) && op !=data[otherID])\n"
		"    tempRes = 0;\n";
	const string	fmt						= subgroups::getFormatNameForGLSL(caseDef.format);
	const string	op						= getOpTypeName(caseDef.opType);
	ostringstream	testSrc;

	testSrc	<< "  uvec4 mask = subgroupBallot(true);\n"
			<< swapTable[caseDef.opType]
			<< "  tempRes = 1;\n";

	if (caseDef.opType == OPTYPE_QUAD_BROADCAST)
	{
		for (int i=0; i<4; i++)
		{
			testSrc << "  {\n"
					<< "  " << fmt << " op = " << op << "(data[gl_SubgroupInvocationID], " << i << ");\n"
					<< "  uint otherID = (gl_SubgroupInvocationID & ~0x3) + " << i << ";\n"
					<< validate
					<< "  }\n";
		}
	}
	else if (caseDef.opType == OPTYPE_QUAD_BROADCAST_NONCONST)
	{
		testSrc << "  for (int i=0; i<4; i++)"
				<< "  {\n"
				<< "  " << fmt << " op = " << op << "(data[gl_SubgroupInvocationID], i);\n"
				<< "  uint otherID = (gl_SubgroupInvocationID & ~0x3) + i;\n"
				<< validate
				<< "  }\n"
				<< "  uint quadID = gl_SubgroupInvocationID >> 2;\n"
				<< "  uint quadInvocation = gl_SubgroupInvocationID & 0x3;\n"
				<< "  // Test lane ID that is only uniform in active lanes\n"
				<< "  if (quadInvocation >= 2)\n"
				<< "  {\n"
				<< "    uint id = quadInvocation & ~1;\n"
				<< "    " << fmt << " op = " << op << "(data[gl_SubgroupInvocationID], id);\n"
				<< "    uint otherID = 4*quadID + id;\n"
				<< validate
				<< "  }\n"
				<< "  // Test lane ID that is only quad uniform, not subgroup uniform\n"
				<< "  {\n"
				<< "    uint id = quadID & 0x3;\n"
				<< "    " << fmt << " op = " << op << "(data[gl_SubgroupInvocationID], id);\n"
				<< "    uint otherID = 4*quadID + id;\n"
				<< validate
				<< "  }\n";
	}
	else
	{
		testSrc << "  " << fmt << " op = " << op << "(data[gl_SubgroupInvocationID]);\n"
				<< "  uint otherID = (gl_SubgroupInvocationID & ~0x3) + swapTable[gl_SubgroupInvocationID & 0x3];\n"
				<< validate;
	}

	return testSrc.str();
}

void initFrameBufferPrograms (SourceCollections& programCollection, CaseDefinition caseDef)
{
	const SpirvVersion			spirvVersion	= (caseDef.opType == OPTYPE_QUAD_BROADCAST_NONCONST) ? SPIRV_VERSION_1_5 : SPIRV_VERSION_1_3;
	const ShaderBuildOptions	buildOptions	(programCollection.usedVulkanVersion, spirvVersion, 0u);

	subgroups::initStdFrameBufferPrograms(programCollection, buildOptions, caseDef.shaderStage, caseDef.format, *caseDef.geometryPointSizeSupported, getExtHeader(caseDef.format), getTestSrc(caseDef), "");
}

void initPrograms (SourceCollections& programCollection, CaseDefinition caseDef)
{
	const bool					spirv15required	= caseDef.opType == OPTYPE_QUAD_BROADCAST_NONCONST;
#ifndef CTS_USES_VULKANSC
	const bool					spirv14required	= (isAllRayTracingStages(caseDef.shaderStage) || isAllMeshShadingStages(caseDef.shaderStage));
#else
	const bool					spirv14required	= false;
#endif // CTS_USES_VULKANSC
	const SpirvVersion			spirvVersion	= spirv15required ? SPIRV_VERSION_1_5
												: spirv14required ? SPIRV_VERSION_1_4
												: SPIRV_VERSION_1_3;
	const ShaderBuildOptions	buildOptions	(programCollection.usedVulkanVersion, spirvVersion, 0u, (spirv14required && !spirv15required));
	const string				extHeader		= getExtHeader(caseDef.format);
	const string				testSrc			= getTestSrc(caseDef);

	subgroups::initStdPrograms(programCollection, buildOptions, caseDef.shaderStage, caseDef.format, *caseDef.geometryPointSizeSupported, extHeader, testSrc, "");
}

void supportedCheck (Context& context, CaseDefinition caseDef)
{
	if (!subgroups::isSubgroupSupported(context))
		TCU_THROW(NotSupportedError, "Subgroup operations are not supported");

	if (!subgroups::areQuadOperationsSupportedForStages(context, caseDef.shaderStage))
		TCU_THROW(NotSupportedError, "Device does not support subgroup quad operations in this shader stage");

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

	if ((caseDef.opType == OPTYPE_QUAD_BROADCAST_NONCONST) && !subgroups::isSubgroupBroadcastDynamicIdSupported(context))
		TCU_THROW(NotSupportedError, "Device does not support SubgroupBroadcastDynamicId");

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
	subgroups::SSBOData inputData;
	inputData.format = caseDef.format;
	inputData.layout = subgroups::SSBOData::LayoutStd140;
	inputData.numElements = subgroups::maxSupportedSubgroupSize();
	inputData.initializeType = subgroups::SSBOData::InitializeNonZero;
	inputData.bindingType = subgroups::SSBOData::BindingUBO;

	switch (caseDef.shaderStage)
	{
		case VK_SHADER_STAGE_VERTEX_BIT:					return subgroups::makeVertexFrameBufferTest(context, VK_FORMAT_R32_UINT, &inputData, 1, DE_NULL, checkVertexPipelineStages);
		case VK_SHADER_STAGE_GEOMETRY_BIT:					return subgroups::makeGeometryFrameBufferTest(context, VK_FORMAT_R32_UINT, &inputData, 1, DE_NULL, checkVertexPipelineStages);
		case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:		return subgroups::makeTessellationEvaluationFrameBufferTest(context, VK_FORMAT_R32_UINT, &inputData, 1, DE_NULL, checkVertexPipelineStages, caseDef.shaderStage);
		case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT:	return subgroups::makeTessellationEvaluationFrameBufferTest(context, VK_FORMAT_R32_UINT, &inputData, 1, DE_NULL, checkVertexPipelineStages, caseDef.shaderStage);
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
		const subgroups::SSBOData								inputData
		{
			subgroups::SSBOData::InitializeNonZero,	// InputDataInitializeType		initializeType;
			subgroups::SSBOData::LayoutStd430,		// InputDataLayoutType			layout;
			caseDef.format,							// vk::VkFormat				format;
			subgroups::maxSupportedSubgroupSize(),	// vk::VkDeviceSize			numElements;
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
		subgroups::SSBOData			inputData;

		inputData.format			= caseDef.format;
		inputData.layout			= subgroups::SSBOData::LayoutStd430;
		inputData.numElements		= subgroups::maxSupportedSubgroupSize();
		inputData.initializeType	= subgroups::SSBOData::InitializeNonZero;
		inputData.binding			= 4u;
		inputData.stages			= stages;

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
TestCaseGroup* createSubgroupsQuadTests (TestContext& testCtx)
{
	de::MovePtr<TestCaseGroup>	group				(new TestCaseGroup(testCtx, "quad", "Subgroup quad category tests"));
	de::MovePtr<TestCaseGroup>	graphicGroup		(new TestCaseGroup(testCtx, "graphics", "Subgroup arithmetic category tests: graphics"));
	de::MovePtr<TestCaseGroup>	computeGroup		(new TestCaseGroup(testCtx, "compute", "Subgroup arithmetic category tests: compute"));
	de::MovePtr<TestCaseGroup>	framebufferGroup	(new TestCaseGroup(testCtx, "framebuffer", "Subgroup arithmetic category tests: framebuffer"));
#ifndef CTS_USES_VULKANSC
	de::MovePtr<TestCaseGroup>	raytracingGroup		(new TestCaseGroup(testCtx, "ray_tracing", "Subgroup arithmetic category tests: ray tracing"));
	de::MovePtr<TestCaseGroup>	meshGroup			(new TestCaseGroup(testCtx, "mesh", "Subgroup arithmetic category tests: mesh shading"));
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
		const vector<VkFormat>	formats	= subgroups::getAllFormats();

		for (size_t formatIndex = 0; formatIndex < formats.size(); ++formatIndex)
		{
			const VkFormat	format					= formats[formatIndex];
			const string	formatName				= subgroups::getFormatNameForGLSL(format);
			const bool		needs8BitUBOStorage		= isFormat8bitTy(format);
			const bool		needs16BitUBOStorage	= isFormat16BitTy(format);

			for (int opTypeIndex = 0; opTypeIndex < OPTYPE_LAST; ++opTypeIndex)
			{
				const OpType	opType	= static_cast<OpType>(opTypeIndex);
				const string	name	= getOpTypeCaseName(opType) + "_" + formatName;

				for (size_t groupSizeNdx = 0; groupSizeNdx < DE_LENGTH_OF_ARRAY(boolValues); ++groupSizeNdx)
				{
					const deBool			requiredSubgroupSize	= boolValues[groupSizeNdx];
					const string			testNameSuffix			= requiredSubgroupSize ? "_requiredsubgroupsize" : "";
					const string			testName				= name + testNameSuffix;
					const CaseDefinition	caseDef					=
					{
						opType,							//  OpType				opType;
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
					for (const auto& stage : meshStages)
					{
						const deBool			requiredSubgroupSize	= boolValues[groupSizeNdx];
						const string			testNameSuffix			= requiredSubgroupSize ? "_requiredsubgroupsize" : "";
						const string			testName				= name + testNameSuffix + "_" + getShaderStageName(stage);
						const CaseDefinition	caseDef					=
						{
							opType,							//  OpType				opType;
							stage,							//  VkShaderStageFlags	shaderStage;
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
					const CaseDefinition	caseDef		=
					{
						opType,							//  OpType				opType;
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
						opType,							//  OpType				opType;
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
		const vector<VkFormat>	formats	= subgroups::getAllRayTracingFormats();

		for (size_t formatIndex = 0; formatIndex < formats.size(); ++formatIndex)
		{
			const VkFormat	format		= formats[formatIndex];
			const string	formatName	= subgroups::getFormatNameForGLSL(format);

			for (int opTypeIndex = 0; opTypeIndex < OPTYPE_LAST; ++opTypeIndex)
			{
				const OpType			opType		= static_cast<OpType>(opTypeIndex);
				const string			testName	= getOpTypeCaseName(opType) + "_" + formatName;
				const CaseDefinition	caseDef		=
				{
					opType,							//  OpType				opType;
					SHADER_STAGE_ALL_RAY_TRACING,	//  VkShaderStageFlags	shaderStage;
					format,							//  VkFormat			format;
					de::SharedPtr<bool>(new bool),	//  de::SharedPtr<bool>	geometryPointSizeSupported;
					DE_FALSE,						//  deBool				requiredSubgroupSize;
					DE_FALSE,						//  deBool				requires8BitUniformBuffer;
					DE_FALSE						//  deBool				requires16BitUniformBuffer;
				};

				addFunctionCaseWithPrograms(raytracingGroup.get(), testName, "", supportedCheck, initPrograms, test, caseDef);
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
