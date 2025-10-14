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
using namespace glu;
using vkt::subgroups::VecType;

string getScanOpName(string prefix, string suffix, Operator op, ScanType scanType)
{
    string n;
    switch (scanType)
    {
    case SCAN_REDUCE:
        n = "";
        break;
    case SCAN_INCLUSIVE:
        n = "Inclusive";
        break;
    case SCAN_EXCLUSIVE:
        n = "Exclusive";
        break;
    }
    switch (op)
    {
    case OPERATOR_ADD:
        n += "Add";
        break;
    case OPERATOR_MUL:
        n += "Mul";
        break;
    case OPERATOR_MIN:
        n += "Min";
        break;
    case OPERATOR_MAX:
        n += "Max";
        break;
    case OPERATOR_AND:
        n += "And";
        break;
    case OPERATOR_OR:
        n += "Or";
        break;
    case OPERATOR_XOR:
        n += "Xor";
        break;
    }
    return prefix + n + suffix;
}

string getOpOperation(Operator op, VecType vecType, string lhs, string rhs)
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
        switch (vecType.type)
        {
        default:
            return "min(" + lhs + ", " + rhs + ")";
        case TYPE_FLOAT16:
        case TYPE_FLOAT:
        case TYPE_DOUBLE:
        {
            if (vecType.components == 1)
            {
                return "(isnan(" + lhs + ") ? " + rhs + " : (isnan(" + rhs + ") ? " + lhs + " : min(" + lhs + ", " +
                       rhs + ")))";
            }
            else
            {
                return "mix(mix(min(" + lhs + ", " + rhs + "), " + lhs + ", isnan(" + rhs + ")), " + rhs + ", isnan(" +
                       lhs + "))";
            }
        }
        }
    case OPERATOR_MAX:
        switch (vecType.type)
        {
        default:
            return "max(" + lhs + ", " + rhs + ")";

        case TYPE_FLOAT16:
        case TYPE_FLOAT:
        case TYPE_DOUBLE:
            if (vecType.components == 1)
            {
                return "(isnan(" + lhs + ") ? " + rhs + " : (isnan(" + rhs + ") ? " + lhs + " : max(" + lhs + ", " +
                       rhs + ")))";
            }
            else
            {
                return "mix(mix(max(" + lhs + ", " + rhs + "), " + lhs + ", isnan(" + rhs + ")), " + rhs + ", isnan(" +
                       lhs + "))";
            }
        }
    case OPERATOR_AND:
        switch (vecType.type)
        {
        default:
            return lhs + " & " + rhs;
        case TYPE_BOOL:
            switch (vecType.components)
            {
            case 1:
                return lhs + " && " + rhs;
            case 2:
                return "bvec2(" + lhs + ".x && " + rhs + ".x, " + lhs + ".y && " + rhs + ".y)";
            case 3:
                return "bvec3(" + lhs + ".x && " + rhs + ".x, " + lhs + ".y && " + rhs + ".y, " + lhs + ".z && " + rhs +
                       ".z)";
            case 4:
                return "bvec4(" + lhs + ".x && " + rhs + ".x, " + lhs + ".y && " + rhs + ".y, " + lhs + ".z && " + rhs +
                       ".z, " + lhs + ".w && " + rhs + ".w)";
            case 8:
                return "vector<bool, 8>(" + lhs + "[0] && " + rhs + "[0], " + lhs + "[1] && " + rhs + "[1], " + lhs +
                       "[2] && " + rhs + "[2], " + lhs + "[3] && " + rhs + "[3], " + lhs + "[4] && " + rhs + "[4], " +
                       lhs + "[5] && " + rhs + "[5], " + lhs + "[6] && " + rhs + "[6], " + lhs + "[7] && " + rhs +
                       "[7])";
            }
            break;
        }
        break;
    case OPERATOR_OR:
        switch (vecType.type)
        {
        default:
            return lhs + " | " + rhs;
        case TYPE_BOOL:
            switch (vecType.components)
            {
            case 1:
                return lhs + " || " + rhs;
            case 2:
                return "bvec2(" + lhs + ".x || " + rhs + ".x, " + lhs + ".y || " + rhs + ".y)";
            case 3:
                return "bvec3(" + lhs + ".x || " + rhs + ".x, " + lhs + ".y || " + rhs + ".y, " + lhs + ".z || " + rhs +
                       ".z)";
            case 4:
                return "bvec4(" + lhs + ".x || " + rhs + ".x, " + lhs + ".y || " + rhs + ".y, " + lhs + ".z || " + rhs +
                       ".z, " + lhs + ".w || " + rhs + ".w)";
            case 8:
                return "vector<bool, 8>(" + lhs + "[0] || " + rhs + "[0], " + lhs + "[1] || " + rhs + "[1], " + lhs +
                       "[2] || " + rhs + "[2], " + lhs + "[3] || " + rhs + "[3], " + lhs + "[4] || " + rhs + "[4], " +
                       lhs + "[5] || " + rhs + "[5], " + lhs + "[6] || " + rhs + "[6], " + lhs + "[7] || " + rhs +
                       "[7])";
            }
            break;
        }
        break;
    case OPERATOR_XOR:
        switch (vecType.type)
        {
        default:
            return lhs + " ^ " + rhs;
        case TYPE_BOOL:
            switch (vecType.components)
            {
            case 1:
                return lhs + " ^^ " + rhs;
            case 2:
                return "bvec2(" + lhs + ".x ^^ " + rhs + ".x, " + lhs + ".y ^^ " + rhs + ".y)";
            case 3:
                return "bvec3(" + lhs + ".x ^^ " + rhs + ".x, " + lhs + ".y ^^ " + rhs + ".y, " + lhs + ".z ^^ " + rhs +
                       ".z)";
            case 4:
                return "bvec4(" + lhs + ".x ^^ " + rhs + ".x, " + lhs + ".y ^^ " + rhs + ".y, " + lhs + ".z ^^ " + rhs +
                       ".z, " + lhs + ".w ^^ " + rhs + ".w)";
            case 8:
                return "vector<bool, 8>(" + lhs + "[0] ^^ " + rhs + "[0], " + lhs + "[1] ^^ " + rhs + "[1], " + lhs +
                       "[2] ^^ " + rhs + "[2], " + lhs + "[3] ^^ " + rhs + "[3], " + lhs + "[4] ^^ " + rhs + "[4], " +
                       lhs + "[5] ^^ " + rhs + "[5], " + lhs + "[6] ^^ " + rhs + "[6], " + lhs + "[7] ^^ " + rhs +
                       "[7])";
            }
            break;
        }
        break;
    }
    return "";
}

string getIdentity(Operator op, VecType vecType)
{
    const bool isFloat    = subgroups::isFormatFloat(vecType);
    const bool isInt      = subgroups::isFormatSigned(vecType);
    const bool isUnsigned = subgroups::isFormatUnsigned(vecType);

    switch (op)
    {
    default:
        DE_FATAL("Unsupported op type");
        return "";
    case OPERATOR_ADD:
        return subgroups::getFormatNameForGLSL(vecType) + "(0)";
    case OPERATOR_MUL:
        return subgroups::getFormatNameForGLSL(vecType) + "(1)";
    case OPERATOR_MIN:
        if (isFloat)
        {
            return subgroups::getFormatNameForGLSL(vecType) + "(intBitsToFloat(0x7f800000))";
        }
        else if (isInt)
        {
            switch (vecType.type)
            {
            default:
                return subgroups::getFormatNameForGLSL(vecType) + "(0x7fffffff)";
            case TYPE_INT8:
            case TYPE_UINT8:
                return subgroups::getFormatNameForGLSL(vecType) + "(0x7f)";
            case TYPE_INT16:
            case TYPE_UINT16:
                return subgroups::getFormatNameForGLSL(vecType) + "(0x7fff)";
            case TYPE_INT64:
            case TYPE_UINT64:
                return subgroups::getFormatNameForGLSL(vecType) + "(0x7fffffffffffffffUL)";
            }
        }
        else if (isUnsigned)
        {
            return subgroups::getFormatNameForGLSL(vecType) + "(-1)";
        }
        else
        {
            DE_FATAL("Unhandled case");
            return "";
        }
    case OPERATOR_MAX:
        if (isFloat)
        {
            return subgroups::getFormatNameForGLSL(vecType) + "(intBitsToFloat(0xff800000))";
        }
        else if (isInt)
        {
            switch (vecType.type)
            {
            default:
                return subgroups::getFormatNameForGLSL(vecType) + "(0x80000000)";
            case TYPE_INT8:
            case TYPE_UINT8:
                return subgroups::getFormatNameForGLSL(vecType) + "(0x80)";
            case TYPE_INT16:
            case TYPE_UINT16:
                return subgroups::getFormatNameForGLSL(vecType) + "(0x8000)";
            case TYPE_INT64:
            case TYPE_UINT64:
                return subgroups::getFormatNameForGLSL(vecType) + "(0x8000000000000000UL)";
            }
        }
        else if (isUnsigned)
        {
            return subgroups::getFormatNameForGLSL(vecType) + "(0)";
        }
        else
        {
            DE_FATAL("Unhandled case");
            return "";
        }
    case OPERATOR_AND:
        return subgroups::getFormatNameForGLSL(vecType) + "(~0)";
    case OPERATOR_OR:
        return subgroups::getFormatNameForGLSL(vecType) + "(0)";
    case OPERATOR_XOR:
        return subgroups::getFormatNameForGLSL(vecType) + "(0)";
    }
}

string getCompare(Operator op, VecType vecType, string lhs, string rhs)
{
    const string formatName = subgroups::getFormatNameForGLSL(vecType);
    const bool isMinMax     = (op == OPERATOR_MIN || op == OPERATOR_MAX);

    if (vecType.components == 1)
    {
        switch (vecType.type)
        {
        default:
            return "(" + lhs + " == " + rhs + ")";
        case TYPE_FLOAT16:
            if (isMinMax)
                return "(" + lhs + " == " + rhs + ")";
            else
                return "(abs(" + lhs + " - " + rhs + ") < " + formatName + "(gl_SubgroupSize==128 ? 0.2: 0.1))";
        case TYPE_FLOAT:
        case TYPE_DOUBLE:
            if (isMinMax)
                return "(" + lhs + " == " + rhs + ")";
            else
                return "(abs(" + lhs + " - " + rhs + ") < (gl_SubgroupSize==128 ? 0.00002:0.00001))";
        }
    }
    else
    {
        switch (vecType.type)
        {
        default:
            return "all(equal(" + lhs + ", " + rhs + "))";
        case TYPE_FLOAT16:
            if (isMinMax)
                return "all(equal(" + lhs + ", " + rhs + "))";
            else
                return "all(lessThan(abs(" + lhs + " - " + rhs + "), " + formatName +
                       "(gl_SubgroupSize==128 ? 0.2: 0.1)))";
        case TYPE_FLOAT:
        case TYPE_DOUBLE:
            if (isMinMax)
                return "all(equal(" + lhs + ", " + rhs + "))";
            else
                return "all(lessThan(abs(" + lhs + " - " + rhs + "), " + formatName +
                       "(gl_SubgroupSize==128 ? 0.00002: 0.00001)))";
        }
    }
}
