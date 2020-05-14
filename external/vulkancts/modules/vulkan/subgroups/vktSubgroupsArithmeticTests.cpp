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

static bool checkVertexPipelineStages(std::vector<const void*> datas,
									  deUint32 width, deUint32)
{
	return vkt::subgroups::check(datas, width, 0x3);
}

static bool checkCompute(std::vector<const void*> datas,
						 const deUint32 numWorkgroups[3], const deUint32 localSize[3],
						 deUint32)
{
	return vkt::subgroups::checkCompute(datas, numWorkgroups, localSize, 0x3);
}

std::string getOpTypeName(int opType)
{
	switch (opType)
	{
		default:
			DE_FATAL("Unsupported op type");
			return "";
		case OPTYPE_ADD:
			return "subgroupAdd";
		case OPTYPE_MUL:
			return "subgroupMul";
		case OPTYPE_MIN:
			return "subgroupMin";
		case OPTYPE_MAX:
			return "subgroupMax";
		case OPTYPE_AND:
			return "subgroupAnd";
		case OPTYPE_OR:
			return "subgroupOr";
		case OPTYPE_XOR:
			return "subgroupXor";
		case OPTYPE_INCLUSIVE_ADD:
			return "subgroupInclusiveAdd";
		case OPTYPE_INCLUSIVE_MUL:
			return "subgroupInclusiveMul";
		case OPTYPE_INCLUSIVE_MIN:
			return "subgroupInclusiveMin";
		case OPTYPE_INCLUSIVE_MAX:
			return "subgroupInclusiveMax";
		case OPTYPE_INCLUSIVE_AND:
			return "subgroupInclusiveAnd";
		case OPTYPE_INCLUSIVE_OR:
			return "subgroupInclusiveOr";
		case OPTYPE_INCLUSIVE_XOR:
			return "subgroupInclusiveXor";
		case OPTYPE_EXCLUSIVE_ADD:
			return "subgroupExclusiveAdd";
		case OPTYPE_EXCLUSIVE_MUL:
			return "subgroupExclusiveMul";
		case OPTYPE_EXCLUSIVE_MIN:
			return "subgroupExclusiveMin";
		case OPTYPE_EXCLUSIVE_MAX:
			return "subgroupExclusiveMax";
		case OPTYPE_EXCLUSIVE_AND:
			return "subgroupExclusiveAnd";
		case OPTYPE_EXCLUSIVE_OR:
			return "subgroupExclusiveOr";
		case OPTYPE_EXCLUSIVE_XOR:
			return "subgroupExclusiveXor";
	}
}

std::string getOpTypeOperation(int opType, vk::VkFormat format, std::string lhs, std::string rhs)
{
	switch (opType)
	{
		default:
			DE_FATAL("Unsupported op type");
			return "";
		case OPTYPE_ADD:
		case OPTYPE_INCLUSIVE_ADD:
		case OPTYPE_EXCLUSIVE_ADD:
			return lhs + " + " + rhs;
		case OPTYPE_MUL:
		case OPTYPE_INCLUSIVE_MUL:
		case OPTYPE_EXCLUSIVE_MUL:
			return lhs + " * " + rhs;
		case OPTYPE_MIN:
		case OPTYPE_INCLUSIVE_MIN:
		case OPTYPE_EXCLUSIVE_MIN:
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
		case OPTYPE_MAX:
		case OPTYPE_INCLUSIVE_MAX:
		case OPTYPE_EXCLUSIVE_MAX:
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
		case OPTYPE_AND:
		case OPTYPE_INCLUSIVE_AND:
		case OPTYPE_EXCLUSIVE_AND:
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
		case OPTYPE_OR:
		case OPTYPE_INCLUSIVE_OR:
		case OPTYPE_EXCLUSIVE_OR:
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
		case OPTYPE_XOR:
		case OPTYPE_INCLUSIVE_XOR:
		case OPTYPE_EXCLUSIVE_XOR:
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
		case OPTYPE_ADD:
		case OPTYPE_INCLUSIVE_ADD:
		case OPTYPE_EXCLUSIVE_ADD:
			return subgroups::getFormatNameForGLSL(format) + "(0)";
		case OPTYPE_MUL:
		case OPTYPE_INCLUSIVE_MUL:
		case OPTYPE_EXCLUSIVE_MUL:
			return subgroups::getFormatNameForGLSL(format) + "(1)";
		case OPTYPE_MIN:
		case OPTYPE_INCLUSIVE_MIN:
		case OPTYPE_EXCLUSIVE_MIN:
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
		case OPTYPE_MAX:
		case OPTYPE_INCLUSIVE_MAX:
		case OPTYPE_EXCLUSIVE_MAX:
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
		case OPTYPE_AND:
		case OPTYPE_INCLUSIVE_AND:
		case OPTYPE_EXCLUSIVE_AND:
			return subgroups::getFormatNameForGLSL(format) + "(~0)";
		case OPTYPE_OR:
		case OPTYPE_INCLUSIVE_OR:
		case OPTYPE_EXCLUSIVE_OR:
			return subgroups::getFormatNameForGLSL(format) + "(0)";
		case OPTYPE_XOR:
		case OPTYPE_INCLUSIVE_XOR:
		case OPTYPE_EXCLUSIVE_XOR:
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
					return "(abs(" + lhs + " - " + rhs + ") < " + formatName + "(0.1))";
				case OPTYPE_MIN:
				case OPTYPE_INCLUSIVE_MIN:
				case OPTYPE_EXCLUSIVE_MIN:
				case OPTYPE_MAX:
				case OPTYPE_INCLUSIVE_MAX:
				case OPTYPE_EXCLUSIVE_MAX:
					return "(" + lhs + " == " + rhs + ")";
			}
		case VK_FORMAT_R32_SFLOAT:
		case VK_FORMAT_R64_SFLOAT:
			switch (opType)
			{
				default:
					return "(abs(" + lhs + " - " + rhs + ") < 0.00001)";
				case OPTYPE_MIN:
				case OPTYPE_INCLUSIVE_MIN:
				case OPTYPE_EXCLUSIVE_MIN:
				case OPTYPE_MAX:
				case OPTYPE_INCLUSIVE_MAX:
				case OPTYPE_EXCLUSIVE_MAX:
					return "(" + lhs + " == " + rhs + ")";
			}
		case VK_FORMAT_R16G16_SFLOAT:
		case VK_FORMAT_R16G16B16_SFLOAT:
		case VK_FORMAT_R16G16B16A16_SFLOAT:
			switch (opType)
			{
				default:
					return "all(lessThan(abs(" + lhs + " - " + rhs + "), " + formatName + "(0.1)))";
				case OPTYPE_MIN:
				case OPTYPE_INCLUSIVE_MIN:
				case OPTYPE_EXCLUSIVE_MIN:
				case OPTYPE_MAX:
				case OPTYPE_INCLUSIVE_MAX:
				case OPTYPE_EXCLUSIVE_MAX:
					return "all(equal(" + lhs + ", " + rhs + "))";
			}
			break;
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
				case OPTYPE_MIN:
				case OPTYPE_INCLUSIVE_MIN:
				case OPTYPE_EXCLUSIVE_MIN:
				case OPTYPE_MAX:
				case OPTYPE_INCLUSIVE_MAX:
				case OPTYPE_EXCLUSIVE_MAX:
					return "all(equal(" + lhs + ", " + rhs + "))";
			}
	}
	DE_FATAL("Unhandled case");
	return "";
}

struct CaseDefinition
{
	int					opType;
	VkShaderStageFlags	shaderStage;
	VkFormat			format;
	de::SharedPtr<bool>	geometryPointSizeSupported;
};

std::string getExtHeader(CaseDefinition caseDef)
{
	return	"#extension GL_KHR_shader_subgroup_arithmetic: enable\n"
			"#extension GL_KHR_shader_subgroup_ballot: enable\n" +
			subgroups::getAdditionalExtensionForFormat(caseDef.format);
}

void initFrameBufferPrograms(SourceCollections& programCollection, CaseDefinition caseDef)
{
	const vk::ShaderBuildOptions	buildOptions	(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_3, 0u);
	std::string						indexVars;
	std::ostringstream				bdy;

	switch (caseDef.opType)
	{
		default:
			indexVars = "  uint start = 0, end = gl_SubgroupSize;\n";
			break;
		case OPTYPE_INCLUSIVE_ADD:
		case OPTYPE_INCLUSIVE_MUL:
		case OPTYPE_INCLUSIVE_MIN:
		case OPTYPE_INCLUSIVE_MAX:
		case OPTYPE_INCLUSIVE_AND:
		case OPTYPE_INCLUSIVE_OR:
		case OPTYPE_INCLUSIVE_XOR:
			indexVars = "  uint start = 0, end = gl_SubgroupInvocationID + 1;\n";
			break;
		case OPTYPE_EXCLUSIVE_ADD:
		case OPTYPE_EXCLUSIVE_MUL:
		case OPTYPE_EXCLUSIVE_MIN:
		case OPTYPE_EXCLUSIVE_MAX:
		case OPTYPE_EXCLUSIVE_AND:
		case OPTYPE_EXCLUSIVE_OR:
		case OPTYPE_EXCLUSIVE_XOR:
			indexVars = "  uint start = 0, end = gl_SubgroupInvocationID;\n";
			break;
	}

	bdy << "  uvec4 mask = subgroupBallot(true);\n"
		<< indexVars
		<< "  " << subgroups::getFormatNameForGLSL(caseDef.format) << " ref = "
		<< getIdentity(caseDef.opType, caseDef.format) << ";\n"
		<< "  tempRes = 0;\n"
		<< "  for (uint index = start; index < end; index++)\n"
		<< "  {\n"
		<< "    if (subgroupBallotBitExtract(mask, index))\n"
		<< "    {\n"
		<< "      ref = " << getOpTypeOperation(caseDef.opType, caseDef.format, "ref", "data[index]") << ";\n"
		<< "    }\n"
		<< "  }\n"
		<< "  tempRes = " << getCompare(caseDef.opType, caseDef.format, "ref",
											getOpTypeName(caseDef.opType) + "(data[gl_SubgroupInvocationID])") << " ? 0x1 : 0;\n"
		<< "  if (1 == (gl_SubgroupInvocationID % 2))\n"
		<< "  {\n"
		<< "    mask = subgroupBallot(true);\n"
		<< "    ref = " << getIdentity(caseDef.opType, caseDef.format) << ";\n"
		<< "    for (uint index = start; index < end; index++)\n"
		<< "    {\n"
		<< "      if (subgroupBallotBitExtract(mask, index))\n"
		<< "      {\n"
		<< "        ref = " << getOpTypeOperation(caseDef.opType, caseDef.format, "ref", "data[index]") << ";\n"
		<< "      }\n"
		<< "    }\n"
		<< "    tempRes |= " << getCompare(caseDef.opType, caseDef.format, "ref",
				getOpTypeName(caseDef.opType) + "(data[gl_SubgroupInvocationID])") << " ? 0x2 : 0;\n"
		<< "  }\n"
		<< "  else\n"
		<< "  {\n"
		<< "    tempRes |= 0x2;\n"
		<< "  }\n";

	subgroups::initStdFrameBufferPrograms(programCollection, buildOptions, caseDef.shaderStage, caseDef.format, *caseDef.geometryPointSizeSupported, getExtHeader(caseDef), bdy.str(), "");
}

void initPrograms(SourceCollections& programCollection, CaseDefinition caseDef)
{
	const vk::ShaderBuildOptions	buildOptions	(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_3, 0u);

	std::string indexVars;
	switch (caseDef.opType)
	{
		default:
			indexVars = "  uint start = 0, end = gl_SubgroupSize;\n";
			break;
		case OPTYPE_INCLUSIVE_ADD:
		case OPTYPE_INCLUSIVE_MUL:
		case OPTYPE_INCLUSIVE_MIN:
		case OPTYPE_INCLUSIVE_MAX:
		case OPTYPE_INCLUSIVE_AND:
		case OPTYPE_INCLUSIVE_OR:
		case OPTYPE_INCLUSIVE_XOR:
			indexVars = "  uint start = 0, end = gl_SubgroupInvocationID + 1;\n";
			break;
		case OPTYPE_EXCLUSIVE_ADD:
		case OPTYPE_EXCLUSIVE_MUL:
		case OPTYPE_EXCLUSIVE_MIN:
		case OPTYPE_EXCLUSIVE_MAX:
		case OPTYPE_EXCLUSIVE_AND:
		case OPTYPE_EXCLUSIVE_OR:
		case OPTYPE_EXCLUSIVE_XOR:
			indexVars = "  uint start = 0, end = gl_SubgroupInvocationID;\n";
			break;
	}

	const string testSrc =
		"  uvec4 mask = subgroupBallot(true);\n"
		+ indexVars +
		"  " + subgroups::getFormatNameForGLSL(caseDef.format) + " ref = "
		+ getIdentity(caseDef.opType, caseDef.format) + ";\n"
		"  tempRes = 0;\n"
		"  for (uint index = start; index < end; index++)\n"
		"  {\n"
		"    if (subgroupBallotBitExtract(mask, index))\n"
		"    {\n"
		"      ref = " + getOpTypeOperation(caseDef.opType, caseDef.format, "ref", "data[index]") + ";\n"
		"    }\n"
		"  }\n"
		"  tempRes = " + getCompare(caseDef.opType, caseDef.format, "ref", getOpTypeName(caseDef.opType) + "(data[gl_SubgroupInvocationID])") + " ? 0x1 : 0;\n"
		"  if (1 == (gl_SubgroupInvocationID % 2))\n"
		"  {\n"
		"    mask = subgroupBallot(true);\n"
		"    ref = " + getIdentity(caseDef.opType, caseDef.format) + ";\n"
		"    for (uint index = start; index < end; index++)\n"
		"    {\n"
		"      if (subgroupBallotBitExtract(mask, index))\n"
		"      {\n"
		"        ref = " + getOpTypeOperation(caseDef.opType, caseDef.format, "ref", "data[index]") + ";\n"
		"      }\n"
		"    }\n"
		"    tempRes |= " + getCompare(caseDef.opType, caseDef.format, "ref", getOpTypeName(caseDef.opType) + "(data[gl_SubgroupInvocationID])") + " ? 0x2 : 0;\n"
		"  }\n"
		"  else\n"
		"  {\n"
		"    tempRes |= 0x2;\n"
		"  }\n";

	std::string extHeader = getExtHeader(caseDef);

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

	subgroups::SSBOData inputData;
	inputData.format = caseDef.format;
	inputData.layout = subgroups::SSBOData::LayoutStd140;
	inputData.numElements = subgroups::maxSupportedSubgroupSize();
	inputData.initializeType = subgroups::SSBOData::InitializeNonZero;

	if (VK_SHADER_STAGE_VERTEX_BIT == caseDef.shaderStage)
		return subgroups::makeVertexFrameBufferTest(context, VK_FORMAT_R32_UINT, &inputData, 1, checkVertexPipelineStages);
	else if (VK_SHADER_STAGE_GEOMETRY_BIT == caseDef.shaderStage)
		return subgroups::makeGeometryFrameBufferTest(context, VK_FORMAT_R32_UINT, &inputData, 1, checkVertexPipelineStages);
	else if (VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT == caseDef.shaderStage)
		return subgroups::makeTessellationEvaluationFrameBufferTest(context, VK_FORMAT_R32_UINT, &inputData, 1, checkVertexPipelineStages, VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT);
	else if (VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT == caseDef.shaderStage)
		return subgroups::makeTessellationEvaluationFrameBufferTest(context, VK_FORMAT_R32_UINT, &inputData, 1, checkVertexPipelineStages, VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT);
	else
		TCU_THROW(InternalError, "Unhandled shader stage");
}

bool checkShaderStages (Context& context, const CaseDefinition& caseDef)
{
	if (!subgroups::areSubgroupOperationsSupportedForStage(
				context, caseDef.shaderStage))
	{
		if (subgroups::areSubgroupOperationsRequiredForStage(
					caseDef.shaderStage))
		{
			return false;
		}
		else
		{
			TCU_THROW(NotSupportedError, "Device does not support subgroup operations for this stage");
		}
	}
	return true;
}

tcu::TestStatus test(Context& context, const CaseDefinition caseDef)
{
	if (VK_SHADER_STAGE_COMPUTE_BIT == caseDef.shaderStage)
	{
		if (!checkShaderStages(context,caseDef))
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

		return subgroups::makeComputeTest(context, VK_FORMAT_R32_UINT, &inputData, 1, checkCompute);
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

		return subgroups::allStages(context, VK_FORMAT_R32_UINT, &inputData, 1, checkVertexPipelineStages, stages);
	}
}
}

namespace vkt
{
namespace subgroups
{
tcu::TestCaseGroup* createSubgroupsArithmeticTests(tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> graphicGroup(new tcu::TestCaseGroup(
		testCtx, "graphics", "Subgroup arithmetic category tests: graphics"));
	de::MovePtr<tcu::TestCaseGroup> computeGroup(new tcu::TestCaseGroup(
		testCtx, "compute", "Subgroup arithmetic category tests: compute"));
	de::MovePtr<tcu::TestCaseGroup> framebufferGroup(new tcu::TestCaseGroup(
		testCtx, "framebuffer", "Subgroup arithmetic category tests: framebuffer"));

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

		for (int opTypeIndex = 0; opTypeIndex < OPTYPE_LAST; ++opTypeIndex)
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
				case OPTYPE_AND:
				case OPTYPE_INCLUSIVE_AND:
				case OPTYPE_EXCLUSIVE_AND:
				case OPTYPE_OR:
				case OPTYPE_INCLUSIVE_OR:
				case OPTYPE_EXCLUSIVE_OR:
				case OPTYPE_XOR:
				case OPTYPE_INCLUSIVE_XOR:
				case OPTYPE_EXCLUSIVE_XOR:
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
			std::string op = getOpTypeName(opTypeIndex);

			{
				const CaseDefinition caseDef = {opTypeIndex, VK_SHADER_STAGE_COMPUTE_BIT, format, de::SharedPtr<bool>(new bool)};
				addFunctionCaseWithPrograms(computeGroup.get(),
											de::toLower(op) + "_" +
											subgroups::getFormatNameForGLSL(format),
											"", supportedCheck, initPrograms, test, caseDef);
			}

			{
				const CaseDefinition caseDef = {opTypeIndex, VK_SHADER_STAGE_ALL_GRAPHICS, format, de::SharedPtr<bool>(new bool)};
				addFunctionCaseWithPrograms(graphicGroup.get(),
											de::toLower(op) + "_" +
											subgroups::getFormatNameForGLSL(format),
											"", supportedCheck, initPrograms, test, caseDef);
			}

			for (int stageIndex = 0; stageIndex < DE_LENGTH_OF_ARRAY(stages); ++stageIndex)
			{
				const CaseDefinition caseDef = {opTypeIndex, stages[stageIndex], format, de::SharedPtr<bool>(new bool)};
				addFunctionCaseWithPrograms(framebufferGroup.get(), de::toLower(op) + "_" + subgroups::getFormatNameForGLSL(format) +
											"_" + getShaderStageName(caseDef.shaderStage), "",
											supportedCheck, initFrameBufferPrograms, noSSBOtest, caseDef);
			}
		}
	}

	de::MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(
		testCtx, "arithmetic", "Subgroup arithmetic category tests"));

	group->addChild(graphicGroup.release());
	group->addChild(computeGroup.release());
	group->addChild(framebufferGroup.release());

	return group.release();
}

} // subgroups
} // vkt
