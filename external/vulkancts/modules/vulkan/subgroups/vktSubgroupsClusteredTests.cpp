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

#include "vktSubgroupsClusteredTests.hpp"
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
	OPTYPE_CLUSTERED_ADD = 0,
	OPTYPE_CLUSTERED_MUL,
	OPTYPE_CLUSTERED_MIN,
	OPTYPE_CLUSTERED_MAX,
	OPTYPE_CLUSTERED_AND,
	OPTYPE_CLUSTERED_OR,
	OPTYPE_CLUSTERED_XOR,
	OPTYPE_CLUSTERED_LAST
};

static bool checkVertexPipelineStages(const void* internalData, std::vector<const void*> datas,
									  deUint32 width, deUint32)
{
	DE_UNREF(internalData);
	return vkt::subgroups::check(datas, width, 1);
}

static bool checkCompute(const void* internalData, std::vector<const void*> datas,
						 const deUint32 numWorkgroups[3], const deUint32 localSize[3],
						 deUint32)
{
	DE_UNREF(internalData);
	return vkt::subgroups::checkCompute(datas, numWorkgroups, localSize, 1);
}

std::string getOpTypeName(int opType)
{
	switch (opType)
	{
		default:
			DE_FATAL("Unsupported op type");
			return "";
		case OPTYPE_CLUSTERED_ADD:
			return "subgroupClusteredAdd";
		case OPTYPE_CLUSTERED_MUL:
			return "subgroupClusteredMul";
		case OPTYPE_CLUSTERED_MIN:
			return "subgroupClusteredMin";
		case OPTYPE_CLUSTERED_MAX:
			return "subgroupClusteredMax";
		case OPTYPE_CLUSTERED_AND:
			return "subgroupClusteredAnd";
		case OPTYPE_CLUSTERED_OR:
			return "subgroupClusteredOr";
		case OPTYPE_CLUSTERED_XOR:
			return "subgroupClusteredXor";
	}
}

std::string getOpTypeOperation(int opType, vk::VkFormat format, std::string lhs, std::string rhs)
{
	switch (opType)
	{
		default:
			DE_FATAL("Unsupported op type");
			return "";
		case OPTYPE_CLUSTERED_ADD:
			return lhs + " + " + rhs;
		case OPTYPE_CLUSTERED_MUL:
			return lhs + " * " + rhs;
		case OPTYPE_CLUSTERED_MIN:
			switch (format)
			{
				default:
					return "min(" + lhs + ", " + rhs + ")";
				case VK_FORMAT_R16_SFLOAT:
				case VK_FORMAT_R32_SFLOAT:
				case VK_FORMAT_R64_SFLOAT:
					return "(isnan(" + lhs + ") ? " + rhs + " : (isnan(" + rhs + ") ? " + lhs + " : min(" + lhs + ", " + rhs + ")))";
				case VK_FORMAT_R16G16_SFLOAT:
				case VK_FORMAT_R16G16B16_SFLOAT:
				case VK_FORMAT_R16G16B16A16_SFLOAT:
				case VK_FORMAT_R32G32_SFLOAT:
				case VK_FORMAT_R32G32B32_SFLOAT:
				case VK_FORMAT_R32G32B32A32_SFLOAT:
				case VK_FORMAT_R64G64_SFLOAT:
				case VK_FORMAT_R64G64B64_SFLOAT:
				case VK_FORMAT_R64G64B64A64_SFLOAT:
					return "mix(mix(min(" + lhs + ", " + rhs + "), " + lhs + ", isnan(" + rhs + ")), " + rhs + ", isnan(" + lhs + "))";
			}
		case OPTYPE_CLUSTERED_MAX:
			switch (format)
			{
				default:
					return "max(" + lhs + ", " + rhs + ")";
				case VK_FORMAT_R16_SFLOAT:
				case VK_FORMAT_R32_SFLOAT:
				case VK_FORMAT_R64_SFLOAT:
					return "(isnan(" + lhs + ") ? " + rhs + " : (isnan(" + rhs + ") ? " + lhs + " : max(" + lhs + ", " + rhs + ")))";
				case VK_FORMAT_R16G16_SFLOAT:
				case VK_FORMAT_R16G16B16_SFLOAT:
				case VK_FORMAT_R16G16B16A16_SFLOAT:
				case VK_FORMAT_R32G32_SFLOAT:
				case VK_FORMAT_R32G32B32_SFLOAT:
				case VK_FORMAT_R32G32B32A32_SFLOAT:
				case VK_FORMAT_R64G64_SFLOAT:
				case VK_FORMAT_R64G64B64_SFLOAT:
				case VK_FORMAT_R64G64B64A64_SFLOAT:
					return "mix(mix(max(" + lhs + ", " + rhs + "), " + lhs + ", isnan(" + rhs + ")), " + rhs + ", isnan(" + lhs + "))";
			}
		case OPTYPE_CLUSTERED_AND:
			switch (format)
			{
				default:
					return lhs + " & " + rhs;
				case VK_FORMAT_R8_USCALED:
					return lhs + " && " + rhs;
				case VK_FORMAT_R8G8_USCALED:
					return "bvec2(" + lhs + ".x && " + rhs + ".x, " + lhs + ".y && " + rhs + ".y)";
				case VK_FORMAT_R8G8B8_USCALED:
					return "bvec3(" + lhs + ".x && " + rhs + ".x, " + lhs + ".y && " + rhs + ".y, " + lhs + ".z && " + rhs + ".z)";
				case VK_FORMAT_R8G8B8A8_USCALED:
					return "bvec4(" + lhs + ".x && " + rhs + ".x, " + lhs + ".y && " + rhs + ".y, " + lhs + ".z && " + rhs + ".z, " + lhs + ".w && " + rhs + ".w)";
			}
		case OPTYPE_CLUSTERED_OR:
			switch (format)
			{
				default:
					return lhs + " | " + rhs;
				case VK_FORMAT_R8_USCALED:
					return lhs + " || " + rhs;
				case VK_FORMAT_R8G8_USCALED:
					return "bvec2(" + lhs + ".x || " + rhs + ".x, " + lhs + ".y || " + rhs + ".y)";
				case VK_FORMAT_R8G8B8_USCALED:
					return "bvec3(" + lhs + ".x || " + rhs + ".x, " + lhs + ".y || " + rhs + ".y, " + lhs + ".z || " + rhs + ".z)";
				case VK_FORMAT_R8G8B8A8_USCALED:
					return "bvec4(" + lhs + ".x || " + rhs + ".x, " + lhs + ".y || " + rhs + ".y, " + lhs + ".z || " + rhs + ".z, " + lhs + ".w || " + rhs + ".w)";
			}
		case OPTYPE_CLUSTERED_XOR:
			switch (format)
			{
				default:
					return lhs + " ^ " + rhs;
				case VK_FORMAT_R8_USCALED:
					return lhs + " ^^ " + rhs;
				case VK_FORMAT_R8G8_USCALED:
					return "bvec2(" + lhs + ".x ^^ " + rhs + ".x, " + lhs + ".y ^^ " + rhs + ".y)";
				case VK_FORMAT_R8G8B8_USCALED:
					return "bvec3(" + lhs + ".x ^^ " + rhs + ".x, " + lhs + ".y ^^ " + rhs + ".y, " + lhs + ".z ^^ " + rhs + ".z)";
				case VK_FORMAT_R8G8B8A8_USCALED:
					return "bvec4(" + lhs + ".x ^^ " + rhs + ".x, " + lhs + ".y ^^ " + rhs + ".y, " + lhs + ".z ^^ " + rhs + ".z, " + lhs + ".w ^^ " + rhs + ".w)";
			}
	}
}

std::string getIdentity(int opType, vk::VkFormat format)
{
	const bool isFloat = subgroups::isFormatFloat(format);
	const bool isInt = subgroups::isFormatSigned(format);
	const bool isUnsigned = subgroups::isFormatUnsigned(format);

	switch (opType)
	{
		default:
			DE_FATAL("Unsupported op type");
			return "";
		case OPTYPE_CLUSTERED_ADD:
			return subgroups::getFormatNameForGLSL(format) + "(0)";
		case OPTYPE_CLUSTERED_MUL:
			return subgroups::getFormatNameForGLSL(format) + "(1)";
		case OPTYPE_CLUSTERED_MIN:
			if (isFloat)
			{
				return subgroups::getFormatNameForGLSL(format) + "(intBitsToFloat(0x7f800000))";
			}
			else if (isInt)
			{
				switch (format)
				{
					default:
						return subgroups::getFormatNameForGLSL(format) + "(0x7fffffff)";
					case VK_FORMAT_R8_SINT:
					case VK_FORMAT_R8G8_SINT:
					case VK_FORMAT_R8G8B8_SINT:
					case VK_FORMAT_R8G8B8A8_SINT:
					case VK_FORMAT_R8_UINT:
					case VK_FORMAT_R8G8_UINT:
					case VK_FORMAT_R8G8B8_UINT:
					case VK_FORMAT_R8G8B8A8_UINT:
						return subgroups::getFormatNameForGLSL(format) + "(0x7f)";
					case VK_FORMAT_R16_SINT:
					case VK_FORMAT_R16G16_SINT:
					case VK_FORMAT_R16G16B16_SINT:
					case VK_FORMAT_R16G16B16A16_SINT:
					case VK_FORMAT_R16_UINT:
					case VK_FORMAT_R16G16_UINT:
					case VK_FORMAT_R16G16B16_UINT:
					case VK_FORMAT_R16G16B16A16_UINT:
						return subgroups::getFormatNameForGLSL(format) + "(0x7fff)";
			        case VK_FORMAT_R64_SINT:
			        case VK_FORMAT_R64G64_SINT:
			        case VK_FORMAT_R64G64B64_SINT:
			        case VK_FORMAT_R64G64B64A64_SINT:
			        case VK_FORMAT_R64_UINT:
			        case VK_FORMAT_R64G64_UINT:
			        case VK_FORMAT_R64G64B64_UINT:
			        case VK_FORMAT_R64G64B64A64_UINT:
						return subgroups::getFormatNameForGLSL(format) + "(0x7fffffffffffffffUL)";
				}
			}
			else if (isUnsigned)
			{
				return subgroups::getFormatNameForGLSL(format) + "(-1)";
			}
			else
			{
				DE_FATAL("Unhandled case");
				return "";
			}
		case OPTYPE_CLUSTERED_MAX:
			if (isFloat)
			{
				return subgroups::getFormatNameForGLSL(format) + "(intBitsToFloat(0xff800000))";
			}
			else if (isInt)
			{
				switch (format)
				{
					default:
						return subgroups::getFormatNameForGLSL(format) + "(0x80000000)";
					case VK_FORMAT_R8_SINT:
					case VK_FORMAT_R8G8_SINT:
					case VK_FORMAT_R8G8B8_SINT:
					case VK_FORMAT_R8G8B8A8_SINT:
					case VK_FORMAT_R8_UINT:
					case VK_FORMAT_R8G8_UINT:
					case VK_FORMAT_R8G8B8_UINT:
					case VK_FORMAT_R8G8B8A8_UINT:
						return subgroups::getFormatNameForGLSL(format) + "(0x80)";
					case VK_FORMAT_R16_SINT:
					case VK_FORMAT_R16G16_SINT:
					case VK_FORMAT_R16G16B16_SINT:
					case VK_FORMAT_R16G16B16A16_SINT:
					case VK_FORMAT_R16_UINT:
					case VK_FORMAT_R16G16_UINT:
					case VK_FORMAT_R16G16B16_UINT:
					case VK_FORMAT_R16G16B16A16_UINT:
						return subgroups::getFormatNameForGLSL(format) + "(0x8000)";
			        case VK_FORMAT_R64_SINT:
			        case VK_FORMAT_R64G64_SINT:
			        case VK_FORMAT_R64G64B64_SINT:
			        case VK_FORMAT_R64G64B64A64_SINT:
			        case VK_FORMAT_R64_UINT:
			        case VK_FORMAT_R64G64_UINT:
			        case VK_FORMAT_R64G64B64_UINT:
			        case VK_FORMAT_R64G64B64A64_UINT:
						return subgroups::getFormatNameForGLSL(format) + "(0x8000000000000000UL)";
				}
			}
			else if (isUnsigned)
			{
				return subgroups::getFormatNameForGLSL(format) + "(0)";
			}
			else
			{
				DE_FATAL("Unhandled case");
				return "";
			}
		case OPTYPE_CLUSTERED_AND:
			return subgroups::getFormatNameForGLSL(format) + "(~0)";
		case OPTYPE_CLUSTERED_OR:
			return subgroups::getFormatNameForGLSL(format) + "(0)";
		case OPTYPE_CLUSTERED_XOR:
			return subgroups::getFormatNameForGLSL(format) + "(0)";
	}
}

std::string getCompare(int opType, vk::VkFormat format, std::string lhs, std::string rhs)
{
	std::string formatName = subgroups::getFormatNameForGLSL(format);
	switch (format)
	{
		default:
			return "all(equal(" + lhs + ", " + rhs + "))";
		case VK_FORMAT_R8_USCALED:
		case VK_FORMAT_R8_UINT:
		case VK_FORMAT_R8_SINT:
		case VK_FORMAT_R16_UINT:
		case VK_FORMAT_R16_SINT:
		case VK_FORMAT_R32_UINT:
		case VK_FORMAT_R32_SINT:
		case VK_FORMAT_R64_UINT:
		case VK_FORMAT_R64_SINT:
			return "(" + lhs + " == " + rhs + ")";
		case VK_FORMAT_R16_SFLOAT:
			switch (opType)
			{
				default:
					return "(abs(" + lhs + " - " + rhs + ") < 0.1)";
				case OPTYPE_CLUSTERED_MIN:
				case OPTYPE_CLUSTERED_MAX:
					return "(" + lhs + " == " + rhs + ")";
			}
		case VK_FORMAT_R32_SFLOAT:
		case VK_FORMAT_R64_SFLOAT:
			switch (opType)
			{
				default:
					return "(abs(" + lhs + " - " + rhs + ") < 0.00001)";
				case OPTYPE_CLUSTERED_MIN:
				case OPTYPE_CLUSTERED_MAX:
					return "(" + lhs + " == " + rhs + ")";
			}
		case VK_FORMAT_R16G16_SFLOAT:
		case VK_FORMAT_R16G16B16_SFLOAT:
		case VK_FORMAT_R16G16B16A16_SFLOAT:
			switch (opType)
			{
				default:
					return "all(lessThan(abs(" + lhs + " - " + rhs + "), " + formatName + "(0.1)))";
				case OPTYPE_CLUSTERED_MIN:
				case OPTYPE_CLUSTERED_MAX:
					return "all(equal(" + lhs + ", " + rhs + "))";
			}
		case VK_FORMAT_R32G32_SFLOAT:
		case VK_FORMAT_R32G32B32_SFLOAT:
		case VK_FORMAT_R32G32B32A32_SFLOAT:
		case VK_FORMAT_R64G64_SFLOAT:
		case VK_FORMAT_R64G64B64_SFLOAT:
		case VK_FORMAT_R64G64B64A64_SFLOAT:
			switch (opType)
			{
				default:
					return "all(lessThan(abs(" + lhs + " - " + rhs + "), " + formatName + "(0.00001)))";
				case OPTYPE_CLUSTERED_MIN:
				case OPTYPE_CLUSTERED_MAX:
					return "all(equal(" + lhs + ", " + rhs + "))";
			}
	}
}

struct CaseDefinition
{
	int					opType;
	VkShaderStageFlags	shaderStage;
	VkFormat			format;
	de::SharedPtr<bool>	geometryPointSizeSupported;
	deBool				requiredSubgroupSize;
};

std::string getExtHeader(CaseDefinition caseDef)
{
	return	"#extension GL_KHR_shader_subgroup_clustered: enable\n"
			"#extension GL_KHR_shader_subgroup_ballot: enable\n"
			+ subgroups::getAdditionalExtensionForFormat(caseDef.format);
}

std::string getBodySource(CaseDefinition caseDef)
{
	std::ostringstream bdy;
	bdy << "  bool tempResult = true;\n"
		<< "  uvec4 mask = subgroupBallot(true);\n";

	for (deUint32 i = 1; i <= subgroups::maxSupportedSubgroupSize(); i *= 2)
	{
		bdy	<< "  {\n"
			<< "    const uint clusterSize = " << i << ";\n"
			<< "    if (clusterSize <= gl_SubgroupSize)\n"
			<< "    {\n"
			<< "      " << subgroups::getFormatNameForGLSL(caseDef.format) << " op = "
			<< getOpTypeName(caseDef.opType) + "(data[gl_SubgroupInvocationID], clusterSize);\n"
			<< "      for (uint clusterOffset = 0; clusterOffset < gl_SubgroupSize; clusterOffset += clusterSize)\n"
			<< "      {\n"
			<< "        " << subgroups::getFormatNameForGLSL(caseDef.format) << " ref = "
			<< getIdentity(caseDef.opType, caseDef.format) << ";\n"
			<< "        for (uint index = clusterOffset; index < (clusterOffset + clusterSize); index++)\n"
			<< "        {\n"
			<< "          if (subgroupBallotBitExtract(mask, index))\n"
			<< "          {\n"
			<< "            ref = " << getOpTypeOperation(caseDef.opType, caseDef.format, "ref", "data[index]") << ";\n"
			<< "          }\n"
			<< "        }\n"
			<< "        if ((clusterOffset <= gl_SubgroupInvocationID) && (gl_SubgroupInvocationID < (clusterOffset + clusterSize)))\n"
			<< "        {\n"
			<< "          if (!" << getCompare(caseDef.opType, caseDef.format, "ref", "op") << ")\n"
			<< "          {\n"
			<< "            tempResult = false;\n"
			<< "          }\n"
			<< "        }\n"
			<< "      }\n"
			<< "    }\n"
			<< "  }\n"
			<< "  tempRes = tempResult ? 1 : 0;\n";
	}
	return bdy.str();
}

void initFrameBufferPrograms(SourceCollections& programCollection, CaseDefinition caseDef)
{
	const vk::ShaderBuildOptions	buildOptions	(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_3, 0u);

	subgroups::initStdFrameBufferPrograms(programCollection, buildOptions, caseDef.shaderStage, caseDef.format, *caseDef.geometryPointSizeSupported, getExtHeader(caseDef), getBodySource(caseDef), "");
}

void initPrograms(SourceCollections& programCollection, CaseDefinition caseDef)
{
	const vk::ShaderBuildOptions	buildOptions	(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_3, 0u);

	std::string extHeader = getExtHeader(caseDef);
	std::string testSrc = getBodySource(caseDef);

	subgroups::initStdPrograms(programCollection, buildOptions, caseDef.shaderStage, caseDef.format, *caseDef.geometryPointSizeSupported, extHeader, testSrc, "");
}

void supportedCheck (Context& context, CaseDefinition caseDef)
{
	if (!subgroups::isSubgroupSupported(context))
		TCU_THROW(NotSupportedError, "Subgroup operations are not supported");

	if (!subgroups::isSubgroupFeatureSupportedForDevice(context, VK_SUBGROUP_FEATURE_CLUSTERED_BIT))
		TCU_THROW(NotSupportedError, "Device does not support subgroup clustered operations");

	if (!subgroups::isFormatSupportedForDevice(context, caseDef.format))
		TCU_THROW(NotSupportedError, "Device does not support the specified format in subgroup operations");

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
	if (!subgroups::areSubgroupOperationsSupportedForStage(context, caseDef.shaderStage))
	{
		if (subgroups::areSubgroupOperationsRequiredForStage(caseDef.shaderStage))
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

	subgroups::SSBOData inputData;
	inputData.format = caseDef.format;
	inputData.layout = subgroups::SSBOData::LayoutStd140;
	inputData.numElements = subgroups::maxSupportedSubgroupSize();
	inputData.initializeType = subgroups::SSBOData::InitializeNonZero;

	if (VK_SHADER_STAGE_VERTEX_BIT == caseDef.shaderStage)
		return subgroups::makeVertexFrameBufferTest(context, VK_FORMAT_R32_UINT, &inputData, 1, DE_NULL, checkVertexPipelineStages);
	else if (VK_SHADER_STAGE_GEOMETRY_BIT == caseDef.shaderStage)
		return subgroups::makeGeometryFrameBufferTest(context, VK_FORMAT_R32_UINT, &inputData, 1, DE_NULL, checkVertexPipelineStages);
	else if (VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT == caseDef.shaderStage)
		return subgroups::makeTessellationEvaluationFrameBufferTest(context, VK_FORMAT_R32_UINT, &inputData, 1, DE_NULL, checkVertexPipelineStages, VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT);
	else if (VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT == caseDef.shaderStage)
		return subgroups::makeTessellationEvaluationFrameBufferTest(context, VK_FORMAT_R32_UINT, &inputData, 1, DE_NULL, checkVertexPipelineStages, VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT);
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
		inputData.initializeType = subgroups::SSBOData::InitializeNonZero;

		if (caseDef.requiredSubgroupSize == DE_FALSE)
			return subgroups::makeComputeTest(context, VK_FORMAT_R32_UINT, &inputData, 1, DE_NULL, checkCompute);

		tcu::TestLog& log = context.getTestContext().getLog();
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
		inputData.initializeType	= subgroups::SSBOData::InitializeNonZero;
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
tcu::TestCaseGroup* createSubgroupsClusteredTests(tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> graphicGroup(new tcu::TestCaseGroup(
		testCtx, "graphics", "Subgroup clustered category tests: graphics"));
	de::MovePtr<tcu::TestCaseGroup> computeGroup(new tcu::TestCaseGroup(
		testCtx, "compute", "Subgroup clustered category tests: compute"));
	de::MovePtr<tcu::TestCaseGroup> framebufferGroup(new tcu::TestCaseGroup(
		testCtx, "framebuffer", "Subgroup clustered category tests: framebuffer"));

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

		for (int opTypeIndex = 0; opTypeIndex < OPTYPE_CLUSTERED_LAST; ++opTypeIndex)
		{
			bool isBool = false;
			bool isFloat = false;

			switch (format)
			{
				default:
					break;
				case VK_FORMAT_R16_SFLOAT:
				case VK_FORMAT_R16G16_SFLOAT:
				case VK_FORMAT_R16G16B16_SFLOAT:
				case VK_FORMAT_R16G16B16A16_SFLOAT:
				case VK_FORMAT_R32_SFLOAT:
				case VK_FORMAT_R32G32_SFLOAT:
				case VK_FORMAT_R32G32B32_SFLOAT:
				case VK_FORMAT_R32G32B32A32_SFLOAT:
				case VK_FORMAT_R64_SFLOAT:
				case VK_FORMAT_R64G64_SFLOAT:
				case VK_FORMAT_R64G64B64_SFLOAT:
				case VK_FORMAT_R64G64B64A64_SFLOAT:
					isFloat = true;
					break;
				case VK_FORMAT_R8_USCALED:
				case VK_FORMAT_R8G8_USCALED:
				case VK_FORMAT_R8G8B8_USCALED:
				case VK_FORMAT_R8G8B8A8_USCALED:
					isBool = true;
					break;
			}

			bool isBitwiseOp = false;

			switch (opTypeIndex)
			{
				default:
					break;
				case OPTYPE_CLUSTERED_AND:
				case OPTYPE_CLUSTERED_OR:
				case OPTYPE_CLUSTERED_XOR:
					isBitwiseOp = true;
					break;
			}

			if (isFloat && isBitwiseOp)
			{
				// Skip float with bitwise category.
				continue;
			}

			if (isBool && !isBitwiseOp)
			{
				// Skip bool when its not the bitwise category.
				continue;
			}

			const std::string name = de::toLower(getOpTypeName(opTypeIndex))
				+"_" + subgroups::getFormatNameForGLSL(format);

			{
				CaseDefinition caseDef = {opTypeIndex, VK_SHADER_STAGE_COMPUTE_BIT, format, de::SharedPtr<bool>(new bool), DE_FALSE};
				addFunctionCaseWithPrograms(computeGroup.get(), name, "", supportedCheck, initPrograms, test, caseDef);
			}

			{
				const CaseDefinition caseDef = {opTypeIndex, VK_SHADER_STAGE_ALL_GRAPHICS, format, de::SharedPtr<bool>(new bool), DE_FALSE};
				addFunctionCaseWithPrograms(graphicGroup.get(), name,
										"", supportedCheck, initPrograms, test, caseDef);
			}

			for (int stageIndex = 0; stageIndex < DE_LENGTH_OF_ARRAY(stages); ++stageIndex)
			{
				const CaseDefinition caseDef = {opTypeIndex, stages[stageIndex], format, de::SharedPtr<bool>(new bool), DE_FALSE};
				addFunctionCaseWithPrograms(framebufferGroup.get(), name +"_" + getShaderStageName(caseDef.shaderStage), "",
							supportedCheck, initFrameBufferPrograms, noSSBOtest, caseDef);
			}
		}
	}
	de::MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(
		testCtx, "clustered", "Subgroup clustered category tests"));

	group->addChild(graphicGroup.release());
	group->addChild(computeGroup.release());
	group->addChild(framebufferGroup.release());

	return group.release();
}
} // subgroups
} // vkt
