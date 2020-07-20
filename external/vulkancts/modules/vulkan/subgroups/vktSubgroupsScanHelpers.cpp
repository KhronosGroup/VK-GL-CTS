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

#include "vktSubgroupsScanHelpers.hpp"
#include "vktSubgroupsTestsUtils.hpp"

#include <string>
#include <vector>

using namespace tcu;
using namespace std;
using namespace vk;
using namespace vkt;

string getScanOpName(string prefix, string suffix, Operator op, ScanType scanType)
{
	string n;
	switch (scanType)
	{
		case SCAN_REDUCE:		n = "";				break;
		case SCAN_INCLUSIVE:	n = "Inclusive";	break;
		case SCAN_EXCLUSIVE:	n = "Exclusive";	break;
	}
	switch (op)
	{
		case OPERATOR_ADD:	n += "Add";	break;
		case OPERATOR_MUL:	n += "Mul";	break;
		case OPERATOR_MIN:	n += "Min";	break;
		case OPERATOR_MAX:	n += "Max";	break;
		case OPERATOR_AND:	n += "And";	break;
		case OPERATOR_OR:	n += "Or";	break;
		case OPERATOR_XOR:	n += "Xor";	break;
	}
	return prefix + n + suffix;
}

string getOpOperation(Operator op, VkFormat format, string lhs, string rhs)
{
	switch (op)
	{
		default:
			DE_FATAL("Unsupported op type");
			return "";
		case OPERATOR_ADD:
			return lhs + " + " + rhs;
		case OPERATOR_MUL:
			return lhs + " * " + rhs;
		case OPERATOR_MIN:
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
		case OPERATOR_MAX:
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
		case OPERATOR_AND:
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
		case OPERATOR_OR:
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
		case OPERATOR_XOR:
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

string getIdentity(Operator op, VkFormat format)
{
	const bool isFloat = subgroups::isFormatFloat(format);
	const bool isInt = subgroups::isFormatSigned(format);
	const bool isUnsigned = subgroups::isFormatUnsigned(format);

	switch (op)
	{
		default:
			DE_FATAL("Unsupported op type");
			return "";
		case OPERATOR_ADD:
			return subgroups::getFormatNameForGLSL(format) + "(0)";
		case OPERATOR_MUL:
			return subgroups::getFormatNameForGLSL(format) + "(1)";
		case OPERATOR_MIN:
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
		case OPERATOR_MAX:
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
		case OPERATOR_AND:
			return subgroups::getFormatNameForGLSL(format) + "(~0)";
		case OPERATOR_OR:
			return subgroups::getFormatNameForGLSL(format) + "(0)";
		case OPERATOR_XOR:
			return subgroups::getFormatNameForGLSL(format) + "(0)";
	}
}

string getCompare(Operator op, VkFormat format, string lhs, string rhs)
{
	string formatName = subgroups::getFormatNameForGLSL(format);
	bool isMinMax = (op == OPERATOR_MIN || op == OPERATOR_MAX);

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
			if (isMinMax)
				return "(" + lhs + " == " + rhs + ")";
			else
				return "(abs(" + lhs + " - " + rhs + ") < " + formatName + "(gl_SubgroupSize==128 ? 0.2: 0.1))";
		case VK_FORMAT_R32_SFLOAT:
		case VK_FORMAT_R64_SFLOAT:
			if (isMinMax)
				return "(" + lhs + " == " + rhs + ")";
			else
				return "(abs(" + lhs + " - " + rhs + ") < (gl_SubgroupSize==128 ? 0.00002:0.00001))";
		case VK_FORMAT_R16G16_SFLOAT:
		case VK_FORMAT_R16G16B16_SFLOAT:
		case VK_FORMAT_R16G16B16A16_SFLOAT:
			if (isMinMax)
				return "all(equal(" + lhs + ", " + rhs + "))";
			else
				return "all(lessThan(abs(" + lhs + " - " + rhs + "), " + formatName + "(gl_SubgroupSize==128 ? 0.2: 0.1)))";
		case VK_FORMAT_R32G32_SFLOAT:
		case VK_FORMAT_R32G32B32_SFLOAT:
		case VK_FORMAT_R32G32B32A32_SFLOAT:
		case VK_FORMAT_R64G64_SFLOAT:
		case VK_FORMAT_R64G64B64_SFLOAT:
		case VK_FORMAT_R64G64B64A64_SFLOAT:
			if (isMinMax)
				return "all(equal(" + lhs + ", " + rhs + "))";
			else
				return "all(lessThan(abs(" + lhs + " - " + rhs + "), " + formatName + "(gl_SubgroupSize==128 ? 0.00002: 0.00001)))";
	}
}
