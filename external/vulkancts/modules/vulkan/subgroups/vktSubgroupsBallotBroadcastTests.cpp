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

#include "vktSubgroupsBallotBroadcastTests.hpp"
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
	OPTYPE_BROADCAST = 0,
	OPTYPE_BROADCAST_NONCONST,
	OPTYPE_BROADCAST_FIRST,
	OPTYPE_LAST
};

struct CaseDefinition
{
	OpType				opType;
	VkShaderStageFlags	shaderStage;
	VkFormat			format;
	de::SharedPtr<bool>	geometryPointSizeSupported;
	deBool				extShaderSubGroupBallotTests;
	deBool				subgroupSizeControl;
	deUint32			requiredSubgroupSize;
	deBool				requires8BitUniformBuffer;
	deBool				requires16BitUniformBuffer;
};

bool checkVertexPipelineStages (const void*			internalData,
								vector<const void*>	datas,
								deUint32			width,
								deUint32)
{
	DE_UNREF(internalData);

	return subgroups::check(datas, width, 3);
}

bool checkComputeOrMesh (const void*			internalData,
						 vector<const void*>	datas,
						 const deUint32			numWorkgroups[3],
						 const deUint32			localSize[3],
						 deUint32)
{
	DE_UNREF(internalData);

	return subgroups::checkComputeOrMesh(datas, numWorkgroups, localSize, 3);
}

string getOpTypeCaseName (OpType opType)
{
	switch (opType)
	{
		case OPTYPE_BROADCAST:			return "subgroupbroadcast";
		case OPTYPE_BROADCAST_NONCONST:	return "subgroupbroadcast_nonconst";
		case OPTYPE_BROADCAST_FIRST:	return "subgroupbroadcastfirst";
		default:						TCU_THROW(InternalError, "Unsupported op type");
	}
}

string getExtHeader (const CaseDefinition& caseDef)
{
	return (caseDef.extShaderSubGroupBallotTests ?	"#extension GL_ARB_shader_ballot: enable\n"
													"#extension GL_KHR_shader_subgroup_basic: enable\n"
													"#extension GL_ARB_gpu_shader_int64: enable\n"
												:	"#extension GL_KHR_shader_subgroup_ballot: enable\n")
				+ subgroups::getAdditionalExtensionForFormat(caseDef.format);
}

string getTestSrc (const CaseDefinition &caseDef)
{
	ostringstream	bdy;
	string			broadcast;
	string			broadcastFirst;
	string			mask;
	int				max;
	const string	fmt					= subgroups::getFormatNameForGLSL(caseDef.format);

	if (caseDef.extShaderSubGroupBallotTests)
	{
		broadcast		= "readInvocationARB";
		broadcastFirst	= "readFirstInvocationARB";
		mask			= "mask = ballotARB(true);\n";
		max				= 64;

		bdy << "  uint64_t mask;\n"
			<< mask
			<< "  uint sgSize = gl_SubGroupSizeARB;\n"
			<< "  uint sgInvocation = gl_SubGroupInvocationARB;\n";
	}
	else
	{
		broadcast		= "subgroupBroadcast";
		broadcastFirst	= "subgroupBroadcastFirst";
		mask			= "mask = subgroupBallot(true);\n";

		if (caseDef.subgroupSizeControl)
			max = caseDef.requiredSubgroupSize;
		else
			max = (int)subgroups::maxSupportedSubgroupSize();

		bdy << "  uvec4 mask = subgroupBallot(true);\n"
			<< "  uint sgSize = gl_SubgroupSize;\n"
			<< "  uint sgInvocation = gl_SubgroupInvocationID;\n";
	}

	if (caseDef.opType == OPTYPE_BROADCAST)
	{
		bdy	<< "  tempRes = 0x3;\n"
			<< "  " << fmt << " ops[" << max << "];\n"
			<< "  " << fmt << " d = data[sgInvocation];\n";

		for (int i = 0; i < max; i++)
			bdy << "  ops[" << i << "] = " << broadcast << "(d, " << i << "u);\n";

		bdy << "  for(int id = 0; id < sgSize; id++)\n"
			<< "  {\n"
			<< "    if (subgroupBallotBitExtract(mask, id) && ops[id] != data[id])\n"
			<< "    {\n"
			<< "      tempRes = 0;\n"
			<< "    }\n"
			<< "  };\n";
	}
	else if (caseDef.opType == OPTYPE_BROADCAST_NONCONST)
	{
		const string validate =	"    if (subgroupBallotBitExtract(mask, id) && op != data[id])\n"
								"        tempRes = 0;\n";

		bdy	<< "  tempRes= 0x3;\n"
			<< "  for (uint id = 0; id < sgSize; id++)\n"
			<< "  {\n"
			<< "    " << fmt << " op = " << broadcast << "(data[sgInvocation], id);\n"
			<< validate
			<< "  }\n"
			<< "  // Test lane id that is only uniform across active lanes\n"
			<< "  if (sgInvocation >= sgSize / 2)\n"
			<< "  {\n"
			<< "    uint id = sgInvocation & ~((sgSize / 2) - 1);\n"
			<< "    " << fmt << " op = " << broadcast << "(data[sgInvocation], id);\n"
			<< validate
			<< "  }\n";
	}
	else if (caseDef.opType == OPTYPE_BROADCAST_FIRST)
	{
		bdy << "  tempRes = 0;\n"
			<< "  uint firstActive = 0;\n"
			<< "  for (uint i = 0; i < sgSize; i++)\n"
			<< "  {\n"
			<< "    if (subgroupBallotBitExtract(mask, i))\n"
			<< "    {\n"
			<< "      firstActive = i;\n"
			<< "      break;\n"
			<< "    }\n"
			<< "  }\n"
			<< "  tempRes |= (" << broadcastFirst << "(data[sgInvocation]) == data[firstActive]) ? 0x1 : 0;\n"
			<< "  // make the firstActive invocation inactive now\n"
			<< "  if (firstActive != sgInvocation)\n"
			<< "  {\n"
			<< mask
			<< "    for (uint i = 0; i < sgSize; i++)\n"
			<< "    {\n"
			<< "      if (subgroupBallotBitExtract(mask, i))\n"
			<< "      {\n"
			<< "        firstActive = i;\n"
			<< "        break;\n"
			<< "      }\n"
			<< "    }\n"
			<< "    tempRes |= (" << broadcastFirst << "(data[sgInvocation]) == data[firstActive]) ? 0x2 : 0;\n"
			<< "  }\n"
			<< "  else\n"
			<< "  {\n"
			<< "    // the firstActive invocation didn't partake in the second result so set it to true\n"
			<< "    tempRes |= 0x2;\n"
			<< "  }\n";
	}
	else
		TCU_THROW(InternalError, "Unknown operation type");

	return bdy.str();
}

string getHelperFunctionARB (const CaseDefinition &caseDef)
{
	ostringstream bdy;

	if (caseDef.extShaderSubGroupBallotTests == DE_FALSE)
		return "";

	bdy << "bool subgroupBallotBitExtract(uint64_t value, uint index)\n";
	bdy << "{\n";
	bdy << "    if (index > 63)\n";
	bdy << "        return false;\n";
	bdy << "    uint64_t mask = 1ul << index;\n";
	bdy << "    if (bool((value & mask)) == true)\n";
	bdy << "        return true;\n";
	bdy << "    return false;\n";
	bdy << "}\n";

	return bdy.str();
}

void initFrameBufferPrograms (SourceCollections& programCollection, CaseDefinition caseDef)
{
	const SpirvVersion			spirvVersion	= (caseDef.opType == OPTYPE_BROADCAST_NONCONST) ? SPIRV_VERSION_1_5 : SPIRV_VERSION_1_3;
	const ShaderBuildOptions	buildOptions	(programCollection.usedVulkanVersion, spirvVersion, 0u);
	const string				extHeader		= getExtHeader(caseDef);
	const string				testSrc			= getTestSrc(caseDef);
	const string				helperStr		= getHelperFunctionARB(caseDef);

	subgroups::initStdFrameBufferPrograms(programCollection, buildOptions, caseDef.shaderStage, caseDef.format, *caseDef.geometryPointSizeSupported, extHeader, testSrc, helperStr);
}

void initPrograms (SourceCollections& programCollection, CaseDefinition caseDef)
{
	const bool					spirv15required	= caseDef.opType == OPTYPE_BROADCAST_NONCONST;
#ifndef CTS_USES_VULKANSC
	const bool					spirv14required	= (isAllRayTracingStages(caseDef.shaderStage) || isAllMeshShadingStages(caseDef.shaderStage));
#else
	const bool					spirv14required	= false;
#endif // CTS_USES_VULKANSC
	const SpirvVersion			spirvVersion	= spirv15required ? SPIRV_VERSION_1_5
												: spirv14required ? SPIRV_VERSION_1_4
												: SPIRV_VERSION_1_3;
	const ShaderBuildOptions	buildOptions	(programCollection.usedVulkanVersion, spirvVersion, 0u, (spirv14required && !spirv15required));
	const string				extHeader		= getExtHeader(caseDef);
	const string				testSrc			= getTestSrc(caseDef);
	const string				helperStr		= getHelperFunctionARB(caseDef);

	subgroups::initStdPrograms(programCollection, buildOptions, caseDef.shaderStage, caseDef.format, *caseDef.geometryPointSizeSupported, extHeader, testSrc, helperStr);
}

void supportedCheck (Context& context, CaseDefinition caseDef)
{
	if (!subgroups::isSubgroupSupported(context))
		TCU_THROW(NotSupportedError, "Subgroup operations are not supported");

	if (!subgroups::isSubgroupFeatureSupportedForDevice(context, VK_SUBGROUP_FEATURE_BALLOT_BIT))
		TCU_THROW(NotSupportedError, "Device does not support subgroup ballot operations");

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

	if (caseDef.extShaderSubGroupBallotTests)
	{
		context.requireDeviceFunctionality("VK_EXT_shader_subgroup_ballot");

		if (!subgroups::isInt64SupportedForDevice(context))
			TCU_THROW(NotSupportedError, "Device does not support int64 data types");
	}

	if ((caseDef.opType == OPTYPE_BROADCAST_NONCONST) && !subgroups::isSubgroupBroadcastDynamicIdSupported(context))
		TCU_THROW(NotSupportedError, "Device does not support SubgroupBroadcastDynamicId");

	if (caseDef.subgroupSizeControl)
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

		if (caseDef.requiredSubgroupSize < subgroupSizeControlProperties.minSubgroupSize
			|| caseDef.requiredSubgroupSize > subgroupSizeControlProperties.maxSubgroupSize)
		{
			TCU_THROW(NotSupportedError, "Unsupported subgroup size");
		}

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
	const VkDeviceSize			numElements	=	caseDef.extShaderSubGroupBallotTests ? 64u : subgroups::maxSupportedSubgroupSize();
	const subgroups::SSBOData	inputData	=
	{
		subgroups::SSBOData::InitializeNonZero,	//  InputDataInitializeType		initializeType;
		subgroups::SSBOData::LayoutStd140,		//  InputDataLayoutType			layout;
		caseDef.format,							//  vk::VkFormat				format;
		numElements,							//  vk::VkDeviceSize			numElements;
		subgroups::SSBOData::BindingUBO,		//  BindingType					bindingType;
	};

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
	const VkDeviceSize	numElements	= caseDef.extShaderSubGroupBallotTests ? 64u : subgroups::maxSupportedSubgroupSize();
	const bool			isCompute	= isAllComputeStages(caseDef.shaderStage);
#ifndef CTS_USES_VULKANSC
	const bool			isMesh		= isAllMeshShadingStages(caseDef.shaderStage);
#else
	const bool			isMesh		= false;
#endif // CTS_USES_VULKANSC

	DE_ASSERT(!(isCompute && isMesh));

	if (isCompute || isMesh)
	{
		const subgroups::SSBOData	inputData	=
		{
			subgroups::SSBOData::InitializeNonZero,	//  InputDataInitializeType		initializeType;
			subgroups::SSBOData::LayoutStd430,		//  InputDataLayoutType			layout;
			caseDef.format,							//  vk::VkFormat				format;
			numElements,							//  vk::VkDeviceSize			numElements;
		};

		if (isCompute)
		{
			if (caseDef.subgroupSizeControl)
				return subgroups::makeComputeTest(context, VK_FORMAT_R32_UINT, &inputData, 1, DE_NULL, checkComputeOrMesh, caseDef.requiredSubgroupSize);
			else
				return subgroups::makeComputeTest(context, VK_FORMAT_R32_UINT, &inputData, 1, DE_NULL, checkComputeOrMesh);
		}
		else
		{
			if (caseDef.subgroupSizeControl)
				return subgroups::makeMeshTest(context, VK_FORMAT_R32_UINT, &inputData, 1, nullptr, checkComputeOrMesh, caseDef.requiredSubgroupSize);
			else
				return subgroups::makeMeshTest(context, VK_FORMAT_R32_UINT, &inputData, 1, nullptr, checkComputeOrMesh);
		}
	}
	else if (isAllGraphicsStages(caseDef.shaderStage))
	{
		const VkShaderStageFlags	stages		= subgroups::getPossibleGraphicsSubgroupStages(context, caseDef.shaderStage);
		const subgroups::SSBOData	inputData	=
		{
			subgroups::SSBOData::InitializeNonZero,	//  InputDataInitializeType		initializeType;
			subgroups::SSBOData::LayoutStd430,		//  InputDataLayoutType			layout;
			caseDef.format,							//  vk::VkFormat				format;
			numElements,							//  vk::VkDeviceSize			numElements;
			subgroups::SSBOData::BindingSSBO,		//  bool						isImage;
			4u,										//  deUint32					binding;
			stages,									//  vk::VkShaderStageFlagBits	stages;
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
			numElements,							//  vk::VkDeviceSize			numElements;
			subgroups::SSBOData::BindingSSBO,		//  bool						isImage;
			6u,										//  deUint32					binding;
			stages,									//  vk::VkShaderStageFlagBits	stages;
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
TestCaseGroup* createSubgroupsBallotBroadcastTests (TestContext& testCtx)
{
	de::MovePtr<TestCaseGroup>	group				(new TestCaseGroup(testCtx, "ballot_broadcast", "Subgroup ballot broadcast category tests"));
	de::MovePtr<TestCaseGroup>	graphicGroup		(new TestCaseGroup(testCtx, "graphics", "Subgroup ballot broadcast category tests: graphics"));
	de::MovePtr<TestCaseGroup>	computeGroup		(new TestCaseGroup(testCtx, "compute", "Subgroup ballot broadcast category tests: compute"));
	de::MovePtr<TestCaseGroup>	framebufferGroup	(new TestCaseGroup(testCtx, "framebuffer", "Subgroup ballot broadcast category tests: framebuffer"));
#ifndef CTS_USES_VULKANSC
	de::MovePtr<TestCaseGroup>	raytracingGroup		(new TestCaseGroup(testCtx, "ray_tracing", "Subgroup ballot broadcast category tests: ray tracing"));
	de::MovePtr<TestCaseGroup>	meshGroup			(new TestCaseGroup(testCtx, "mesh", "Subgroup ballot broadcast category tests: mesh"));
	de::MovePtr<TestCaseGroup>	meshGroupARB		(new TestCaseGroup(testCtx, "mesh", "Subgroup ballot broadcast category tests: mesh"));
#endif // CTS_USES_VULKANSC

	de::MovePtr<TestCaseGroup>	groupARB			(new TestCaseGroup(testCtx, "ext_shader_subgroup_ballot", "VK_EXT_shader_subgroup_ballot category tests"));
	de::MovePtr<TestCaseGroup>	graphicGroupARB		(new TestCaseGroup(testCtx, "graphics", "Subgroup ballot broadcast category tests: graphics"));
	de::MovePtr<TestCaseGroup>	computeGroupARB		(new TestCaseGroup(testCtx, "compute", "Subgroup ballot broadcast category tests: compute"));
	de::MovePtr<TestCaseGroup>	framebufferGroupARB	(new TestCaseGroup(testCtx, "framebuffer", "Subgroup ballot broadcast category tests: framebuffer"));

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
		const vector<VkFormat>	formats		= subgroups::getAllFormats();

		for (size_t formatIndex = 0; formatIndex < formats.size(); ++formatIndex)
		{
			const VkFormat	format						= formats[formatIndex];
			// Vector, boolean and double types are not supported by functions defined in VK_EXT_shader_subgroup_ballot.
			const bool		formatTypeIsSupportedARB	= format == VK_FORMAT_R32_SINT || format == VK_FORMAT_R32_UINT || format == VK_FORMAT_R32_SFLOAT;
			const bool		needs8BitUBOStorage			= isFormat8bitTy(format);
			const bool		needs16BitUBOStorage		= isFormat16BitTy(format);

			for (int opTypeIndex = 0; opTypeIndex < OPTYPE_LAST; ++opTypeIndex)
			{
				const OpType	opType	= static_cast<OpType>(opTypeIndex);
				const string	name	= getOpTypeCaseName(opType) + "_" + subgroups::getFormatNameForGLSL(format);

				for (size_t extNdx = 0; extNdx < DE_LENGTH_OF_ARRAY(boolValues); ++extNdx)
				{
					const deBool	extShaderSubGroupBallotTests	= boolValues[extNdx];

					if (extShaderSubGroupBallotTests && !formatTypeIsSupportedARB)
						continue;

					{
						TestCaseGroup*		testGroup	= extShaderSubGroupBallotTests ? computeGroupARB.get() : computeGroup.get();
						{
							const CaseDefinition	caseDef		=
							{
								opType,							//  OpType				opType;
								VK_SHADER_STAGE_COMPUTE_BIT,	//  VkShaderStageFlags	shaderStage;
								format,							//  VkFormat			format;
								de::SharedPtr<bool>(new bool),	//  de::SharedPtr<bool>	geometryPointSizeSupported;
								extShaderSubGroupBallotTests,	//  deBool				extShaderSubGroupBallotTests;
								DE_FALSE,						//  deBool				subgroupSizeControl;
								0u,								//  deUint32			requiredSubgroupSize;
								DE_FALSE,						//  deBool				requires8BitUniformBuffer;
								DE_FALSE,						//  deBool				requires16BitUniformBuffer;
							};

							addFunctionCaseWithPrograms(testGroup, name, "", supportedCheck, initPrograms, test, caseDef);
						}

						for (deUint32 subgroupSize = 1; subgroupSize <= subgroups::maxSupportedSubgroupSize(); subgroupSize *= 2)
						{
							const CaseDefinition	caseDef		=
							{
								opType,							//  OpType				opType;
								VK_SHADER_STAGE_COMPUTE_BIT,	//  VkShaderStageFlags	shaderStage;
								format,							//  VkFormat			format;
								de::SharedPtr<bool>(new bool),	//  de::SharedPtr<bool>	geometryPointSizeSupported;
								extShaderSubGroupBallotTests,	//  deBool				extShaderSubGroupBallotTests;
								DE_TRUE,						//  deBool				subgroupSizeControl;
								subgroupSize,					//  deUint32			requiredSubgroupSize;
								DE_FALSE,						//  deBool				requires8BitUniformBuffer;
								DE_FALSE						//  deBool				requires16BitUniformBuffer;
							};
							const string			testName	= name + "_requiredsubgroupsize" + de::toString(subgroupSize);

							addFunctionCaseWithPrograms(testGroup, testName, "", supportedCheck, initPrograms, test, caseDef);
						}
					}

#ifndef CTS_USES_VULKANSC
					for (const auto& stage : meshStages)
					{
						const auto			stageName	= "_" + getShaderStageName(stage);

						TestCaseGroup*		testGroup	= extShaderSubGroupBallotTests ? meshGroupARB.get() : meshGroup.get();
						{
							const CaseDefinition	caseDef		=
							{
								opType,							//  OpType				opType;
								stage,							//  VkShaderStageFlags	shaderStage;
								format,							//  VkFormat			format;
								de::SharedPtr<bool>(new bool),	//  de::SharedPtr<bool>	geometryPointSizeSupported;
								extShaderSubGroupBallotTests,	//  deBool				extShaderSubGroupBallotTests;
								DE_FALSE,						//  deBool				subgroupSizeControl;
								0u,								//  deUint32			requiredSubgroupSize;
								DE_FALSE,						//  deBool				requires8BitUniformBuffer;
								DE_FALSE,						//  deBool				requires16BitUniformBuffer;
							};

							addFunctionCaseWithPrograms(testGroup, name + stageName, "", supportedCheck, initPrograms, test, caseDef);
						}

						for (deUint32 subgroupSize = 1; subgroupSize <= subgroups::maxSupportedSubgroupSize(); subgroupSize *= 2)
						{
							const CaseDefinition	caseDef		=
							{
								opType,							//  OpType				opType;
								stage,							//  VkShaderStageFlags	shaderStage;
								format,							//  VkFormat			format;
								de::SharedPtr<bool>(new bool),	//  de::SharedPtr<bool>	geometryPointSizeSupported;
								extShaderSubGroupBallotTests,	//  deBool				extShaderSubGroupBallotTests;
								DE_TRUE,						//  deBool				subgroupSizeControl;
								subgroupSize,					//  deUint32			requiredSubgroupSize;
								DE_FALSE,						//  deBool				requires8BitUniformBuffer;
								DE_FALSE,						//  deBool				requires16BitUniformBuffer;
							};
							const string			testName	= name + "_requiredsubgroupsize" + de::toString(subgroupSize) + stageName;

							addFunctionCaseWithPrograms(testGroup, testName, "", supportedCheck, initPrograms, test, caseDef);
						}
					}
#endif // CTS_USES_VULKANSC

					{
						TestCaseGroup*			testGroup	= extShaderSubGroupBallotTests ? graphicGroupARB.get() : graphicGroup.get();
						const CaseDefinition	caseDef		=
						{
							opType,							//  OpType				opType;
							VK_SHADER_STAGE_ALL_GRAPHICS,	//  VkShaderStageFlags	shaderStage;
							format,							//  VkFormat			format;
							de::SharedPtr<bool>(new bool),	//  de::SharedPtr<bool>	geometryPointSizeSupported;
							extShaderSubGroupBallotTests,	//  deBool				extShaderSubGroupBallotTests;
							DE_FALSE,						//  deBool				subgroupSizeControl;
							0u,								//  deUint32			requiredSubgroupSize;
							DE_FALSE,						//  deBool				requires8BitUniformBuffer;
							DE_FALSE						//  deBool				requires16BitUniformBuffer;
						};

						addFunctionCaseWithPrograms(testGroup, name, "", supportedCheck, initPrograms, test, caseDef);
					}

					{
						TestCaseGroup*	testGroup	= extShaderSubGroupBallotTests ? framebufferGroupARB.get() : framebufferGroup.get();

						for (int stageIndex = 0; stageIndex < DE_LENGTH_OF_ARRAY(fbStages); ++stageIndex)
						{
							const CaseDefinition	caseDef		=
							{
								opType,							//  OpType				opType;
								fbStages[stageIndex],			//  VkShaderStageFlags	shaderStage;
								format,							//  VkFormat			format;
								de::SharedPtr<bool>(new bool),	//  de::SharedPtr<bool>	geometryPointSizeSupported;
								extShaderSubGroupBallotTests,	//  deBool				extShaderSubGroupBallotTests;
								DE_FALSE,						//  deBool				subgroupSizeControl;
								0u,								//  deUint32			requiredSubgroupSize;
								deBool(needs8BitUBOStorage),	//  deBool				requires8BitUniformBuffer;
								deBool(needs16BitUBOStorage)	//  deBool				requires16BitUniformBuffer;
							};

							addFunctionCaseWithPrograms(testGroup, name + getShaderStageName(caseDef.shaderStage), "", supportedCheck, initFrameBufferPrograms, noSSBOtest, caseDef);
						}
					}
				}
			}
		}
	}

#ifndef CTS_USES_VULKANSC
	{
		const vector<VkFormat>	formats		= subgroups::getAllRayTracingFormats();

		for (size_t formatIndex = 0; formatIndex < formats.size(); ++formatIndex)
		{
			const VkFormat	format		= formats[formatIndex];
			const string	formatName	= subgroups::getFormatNameForGLSL(format);

			for (int opTypeIndex = 0; opTypeIndex < OPTYPE_LAST; ++opTypeIndex)
			{
				const OpType			opType		= static_cast<OpType>(opTypeIndex);
				const string			name		= getOpTypeCaseName(opType) + "_" + formatName;
				const CaseDefinition	caseDef		=
				{
					opType,							//  OpType				opType;
					SHADER_STAGE_ALL_RAY_TRACING,	//  VkShaderStageFlags	shaderStage;
					format,							//  VkFormat			format;
					de::SharedPtr<bool>(new bool),	//  de::SharedPtr<bool>	geometryPointSizeSupported;
					DE_FALSE,						//  deBool				extShaderSubGroupBallotTests;
					DE_FALSE,						//  deBool				subgroupSizeControl;
					0,								//  int					requiredSubgroupSize;
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

	group->addChild(graphicGroup.release());
	group->addChild(computeGroup.release());
	group->addChild(framebufferGroup.release());
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
