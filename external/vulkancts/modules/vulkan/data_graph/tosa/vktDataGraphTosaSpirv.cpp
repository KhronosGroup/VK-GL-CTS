/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2025-2026 Arm Ltd.
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
 */
/*!
 * \file
 * \brief Tosa instruction set SPIRV generator
 */
/*--------------------------------------------------------------------*/

#include "vktDataGraphTosaSpirv.hpp"
#include "vktDataGraphTosaUtil.hpp"

#include "tcuStringTemplate.hpp"

#include <unordered_set>

namespace vkt
{

namespace dataGraph
{

void TosaSpirv::replaceAll(std::string &str, const std::string &what, const std::string &replacement)
{
    std::size_t pos    = 0;
    size_t what_length = what.length();
    while (true)
    {
        pos = str.find(what, pos);
        if (pos == std::string::npos)
        {
            break;
        }
        str.replace(pos, what_length, replacement);
        pos += what_length;
    }
}

std::string TosaSpirv::spirvAssignment(const std::string &name, const std::string &value)
{
    std::string padding = "";
    if (sourcePadding > name.size())
    {
        padding.insert(0, sourcePadding - name.size(), ' ');
    }
    return padding + "%" + name + " = " + value;
}

std::string TosaSpirv::spirvDeclaration(const std::string &declaration)
{
    std::string padding = "";
    padding.insert(0, sourcePadding + 4, ' ');
    return padding + declaration;
}

TosaSpirv::format TosaSpirv::tosaSpirvFormat(VkFormat vulkanFormat)
{
    switch (vulkanFormat)
    {
    case VK_FORMAT_R64_SINT:
    {
        return TosaSpirv::format::i48_t;
    }
    case VK_FORMAT_R32_SINT:
    case VK_FORMAT_R32_UINT:
    {
        return TosaSpirv::format::i32_t;
    }
    case VK_FORMAT_R32_SFLOAT:
    {
        return TosaSpirv::format::fp32_t;
    }
    case VK_FORMAT_R16_SINT:
    case VK_FORMAT_R16_UINT:
    {
        return TosaSpirv::format::i16_t;
    }
    case VK_FORMAT_R16_SFLOAT:
    {
        return TosaSpirv::format::fp16_t;
    }
    case VK_FORMAT_R8_SINT:
    case VK_FORMAT_R8_UINT:
    {
        return TosaSpirv::format::i8_t;
    }
    case VK_FORMAT_R8_BOOL_ARM:
    {
        return TosaSpirv::format::bool_t;
    }
    default:
        return TosaSpirv::format::invalid;
    }
}

std::string TosaSpirv::typeToString(TosaSpirv::format fmt)
{
    std::string type_name;
    std::string type_value;

    switch (fmt)
    {
    case TosaSpirv::format::i48_t:
    {
        type_name  = "i64";
        type_value = "OpTypeInt 64 0";
        break;
    }
    case TosaSpirv::format::i32_t:
    {
        type_name  = "i32";
        type_value = "OpTypeInt 32 0";
        break;
    }
    case TosaSpirv::format::i16_t:
    {
        type_name  = "i16";
        type_value = "OpTypeInt 16 0";
        break;
    }
    case TosaSpirv::format::i8_t:
    {
        type_name  = "i8";
        type_value = "OpTypeInt 8 0";
        break;
    }
    case TosaSpirv::format::fp32_t:
    {
        type_name  = "fp32";
        type_value = "OpTypeFloat 32";
        break;
    }
    case TosaSpirv::format::fp16_t:
    {
        type_name  = "fp16";
        type_value = "OpTypeFloat 16";
        break;
    }
    case TosaSpirv::format::bool_t:
    {
        type_name  = "bool";
        type_value = "OpTypeBool";
        break;
    }
    default:
    {
        return "";
    }
    }

    m_spirvBlocks[BASIC_TYPES].push_back(spirvAssignment(type_name, type_value));
    return type_name;
}

std::string TosaSpirv::typeArray(TosaSpirv::format fmt, int64_t size)
{
    std::string type_name  = "TYPE_arr_SIZE";
    std::string type_value = "OpTypeArray %TYPE %SIZE";

    replaceAll(type_name, "TYPE", typeToString(fmt));
    replaceAll(type_name, "SIZE", std::to_string(size));
    replaceAll(type_value, "TYPE", typeToString(fmt));
    replaceAll(type_value, "SIZE", spirvConstant(TosaSpirv::format::i32_t, size));
    m_spirvBlocks[COMPOSITE_TYPES].push_back(spirvAssignment(type_name, type_value));

    return type_name;
}

std::string TosaSpirv::typeVector(TosaSpirv::format fmt, int64_t size)
{
    std::string type_name  = "TYPE_vec_SIZE";
    std::string type_value = "OpTypeVector %TYPE SIZE";

    replaceAll(type_name, "TYPE", typeToString(fmt));
    replaceAll(type_name, "SIZE", std::to_string(size));
    replaceAll(type_value, "TYPE", typeToString(fmt));
    replaceAll(type_value, "SIZE", std::to_string(size));
    m_spirvBlocks[COMPOSITE_TYPES].push_back(spirvAssignment(type_name, type_value));

    return type_name;
}

std::string TosaSpirv::spirvConstant(TosaSpirv::format fmt, std::string value, std::string label)
{
    std::string const_name  = label.empty() ? "TYPE_VALUE" : label;
    std::string const_value = "OpConstant %TYPE VALUE";

    /* Bool values are handled differently */
    if (TosaSpirv::format::bool_t == fmt)
    {
        const_value = "OpConstantVALUE %TYPE";
        value       = ('0' == value[0]) ? "False" : "True";
    }

    replaceAll(const_name, "TYPE", typeToString(fmt));
    replaceAll(const_name, "VALUE", value);
    replaceAll(const_value, "TYPE", typeToString(fmt));
    replaceAll(const_value, "VALUE", value);
    m_spirvBlocks[BASIC_CONSTANTS].push_back(spirvAssignment(const_name, const_value));

    return const_name;
}

std::string TosaSpirv::spirvConstant(TosaSpirv::format fmt, uint64_t value, std::string label)
{
    return spirvConstant(fmt, std::to_string(value), label);
}

std::string TosaSpirv::constantComposite(std::string varName, TosaSpirv::format fmt, spirvOrder order,
                                         const int64_t *values, int64_t size, std::string label)
{
    auto toStr    = [](uint64_t value) -> std::string { return std::to_string(value); };
    auto to_const = [this, fmt](uint64_t value) -> std::string { return "%" + spirvConstant(fmt, value); };

    std::string composite_name  = label.empty() ? (varName + "_VALUES") : label;
    std::string composite_value = "OpConstantComposite %" + varName + " VALUES";

    replaceAll(composite_name, "VALUES", spirvJoin(values, size, "_", toStr));
    replaceAll(composite_value, "VALUES", spirvJoin(values, size, " ", to_const));
    m_spirvBlocks[order].push_back(spirvAssignment(composite_name, composite_value));

    return composite_name;
}

std::string TosaSpirv::constantCompositeArray(TosaSpirv::format fmt, const int64_t *values, int64_t size,
                                              std::string label)
{
    return constantComposite(typeArray(fmt, size), fmt, COMPOSITE_CONSTANTS, values, size, label);
}

std::string TosaSpirv::constantCompositeTensor(TosaSpirv::format fmt, const int64_t *values, int64_t size,
                                               std::string label)
{
    return constantComposite(typeTensor(fmt, &size, 1UL), fmt, COMPOSITE_TENSORS, values, size, label);
}

std::string TosaSpirv::constantCompositeVector(TosaSpirv::format fmt, const int64_t *values, int64_t size,
                                               std::string label)
{
    return constantComposite(typeVector(fmt, size), fmt, COMPOSITE_CONSTANTS, values, size, label);
}

std::string TosaSpirv::typeTensor(const ResourceInformation &resInfo)
{
    const auto &fmt  = resInfo.params.format;
    const auto &dims = resInfo.params.dimensions.data();
    const auto &rank = resInfo.params.dimensions.size();

    return typeTensor(fmt, dims, static_cast<uint32_t>(rank));
}

std::string TosaSpirv::typeTensor(const TosaSpirv::format fmt, const int64_t *dims, uint32_t rank)
{
    auto toStr = [](uint64_t value) -> std::string { return std::to_string(value); };

    std::string tensorName  = "TYPE_SHAPE_tensor";
    std::string tensorValue = "OpTypeTensorARM %TYPE %RANK %SHAPE";

    replaceAll(tensorName, "TYPE", typeToString(fmt));
    replaceAll(tensorName, "SHAPE", spirvJoin(dims, rank, "_", toStr));
    replaceAll(tensorValue, "TYPE", typeToString(fmt));
    replaceAll(tensorValue, "RANK", spirvConstant(TosaSpirv::format::i32_t, rank));
    replaceAll(tensorValue, "SHAPE", constantCompositeArray(TosaSpirv::format::i32_t, dims, rank));
    m_spirvBlocks[TENSOR_TYPES].push_back(spirvAssignment(tensorName, tensorValue));

    return tensorName;
}

std::string TosaSpirv::typeTensor(VkFormat fmt, const int64_t *dims, uint32_t rank)
{
    return typeTensor(tosaSpirvFormat(fmt), dims, rank);
}

std::string TosaSpirv::typeTensorPointer(const ResourceInformation &resInfo)
{
    std::string ptrValue = typeTensor(resInfo);
    std::string ptrName  = ptrValue + "_ptr";

    m_spirvBlocks[POINTER_TYPES].push_back(spirvAssignment(ptrName, "OpTypePointer UniformConstant %" + ptrValue));
    return ptrName;
}

std::string TosaSpirv::spirvVariable(const ResourceInformation &resInfo)
{
    std::string varName;

    if (resInfo.type == RESOURCE_TYPE_INPUT)
    {
        varName = "main_arg_ID";
    }
    else
    {
        varName = "main_res_ID";
    }
    std::string varValue = "OpVariable %TENSOR_PTR UniformConstant";

    replaceAll(varName, "ID", std::to_string(resInfo.id));
    replaceAll(varValue, "TENSOR_PTR", typeTensorPointer(resInfo));
    m_spirvBlocks[OP_VARIABLES].push_back(spirvAssignment(varName, varValue));

    return varName;
}

std::string TosaSpirv::spirvGraphParam(const ResourceInformation &resInfo)
{
    std::string opName       = "OpName %VAR_NAME \"VAR_NAME\"";
    std::string opBinding    = "OpDecorate %VAR_NAME Binding BINDING";
    std::string opDescriptor = "OpDecorate %VAR_NAME DescriptorSet DESCRIPTOR";

    std::string varName = spirvVariable(resInfo);

    replaceAll(opName, "VAR_NAME", varName);
    m_spirvBlocks[OP_NAMES].push_back(spirvDeclaration(opName));

    replaceAll(opBinding, "VAR_NAME", varName);
    replaceAll(opBinding, "BINDING", std::to_string(resInfo.binding));
    m_spirvBlocks[OP_DECORATORS].push_back(spirvDeclaration(opBinding));

    replaceAll(opDescriptor, "VAR_NAME", varName);
    replaceAll(opDescriptor, "DESCRIPTOR", std::to_string(resInfo.descriptorSet));
    m_spirvBlocks[OP_DECORATORS].push_back(spirvDeclaration(opDescriptor));

    return varName;
}

std::string TosaSpirv::graphInput(const ResourceInformation &resInfo)
{
    std::string paramName  = "in_ID";
    std::string paramValue = "OpGraphInputARM %TENSOR %INDEX";
    replaceAll(paramName, "ID", std::to_string(resInfo.id));
    {
        // force typeTensor to ignore array types
        const auto &fmt  = resInfo.params.format;
        const auto &dims = resInfo.params.dimensions.data();
        const auto &rank = resInfo.params.dimensions.size();
        replaceAll(paramValue, "TENSOR", typeTensor(fmt, dims, static_cast<uint32_t>(rank)));
    }
    replaceAll(paramValue, "INDEX", spirvConstant(TosaSpirv::format::i32_t, resInfo.id));

    m_spirvBlocks[OP_GRAPH].push_back(spirvAssignment(paramName, paramValue));
    return paramName;
}

std::string TosaSpirv::graphConstant(const ResourceInformation &resInfo)
{
    std::string paramName  = resInfo.label.empty() ? "const_input_ID" : resInfo.label;
    std::string paramValue = "OpGraphConstantARM %TENSOR ID";

    replaceAll(paramName, "ID", std::to_string(resInfo.id));
    replaceAll(paramValue, "TENSOR", typeTensor(resInfo));
    replaceAll(paramValue, "ID", std::to_string(resInfo.id));
    m_spirvBlocks[OP_GRAPH_CONSTANTS].push_back(spirvAssignment(paramName, paramValue));

    return paramName;
}

std::string TosaSpirv::typeGraph(const std::vector<ResourceInformation> &resInfoInputTensors,
                                 const std::vector<ResourceInformation> &resInfoOutputTensors)
{
    auto toStr = [this](const ResourceInformation &resInfo) -> std::string { return "%" + this->typeTensor(resInfo); };

    std::string type_name  = "graph_type";
    std::string type_value = "OpTypeGraphARM NUM_INPUTS IN_TENSORS OUT_TENSORS";

    type_value = std::regex_replace(type_value, std::regex("NUM_INPUTS"), std::to_string(resInfoInputTensors.size()));
    type_value = std::regex_replace(type_value, std::regex("IN_TENSORS"),
                                    spirvJoin(resInfoInputTensors.data(), resInfoInputTensors.size(), " ", toStr));
    type_value = std::regex_replace(type_value, std::regex("OUT_TENSORS"),
                                    spirvJoin(resInfoOutputTensors.data(), resInfoOutputTensors.size(), " ", toStr));

    m_spirvBlocks[OP_GRAPH_TYPES].push_back(spirvAssignment(type_name, type_value));
    return type_name;
}

std::string TosaSpirv::spirvGraphObject(const std::vector<ResourceInformation> &resInfoInputTensors,
                                        const std::vector<ResourceInformation> &resInfoOutputTensors)
{
    std::string graphName  = "graph_0";
    std::string graphValue = "OpGraphARM %GRAPH_TYPE";

    replaceAll(graphValue, "GRAPH_TYPE", typeGraph(resInfoInputTensors, resInfoOutputTensors));
    m_spirvBlocks[OP_GRAPH_VARS].push_back(spirvAssignment(graphName, graphValue));

    return graphName;
}

TosaSpirv::TosaSpirv()
{
    // Add capabilities and extension headers
    for (const auto &name : capabilities)
    {
        m_spirvBlocks[OP_CAPABILITY].push_back(spirvDeclaration("OpCapability " + name));
    }

    for (const auto &name : extensions)
    {
        m_spirvBlocks[OP_EXTENSION].push_back(spirvDeclaration("OpExtension \"" + name + "\""));
    }

    // Add hardcoded blocks
    m_spirvBlocks[OP_INIT].push_back(spirvAssignment(tosa_ext_name, "OpExtInstImport \"TOSA.001000.1\""));
    m_spirvBlocks[OP_INIT].push_back(spirvDeclaration("OpMemoryModel Logical Vulkan"));

    // Inject comments and spaces to improve readability of the spirv source
    m_spirvBlocks[OP_NAMES].push_back(
        "; ---- Interface names ---------------------------------------------------------");
    m_spirvBlocks[OP_DECORATORS].push_back(
        "; ---- Descriptor bindings & spec ids -----------------------------------------");
    m_spirvBlocks[BASIC_TYPES].push_back("; ---- Basic Types ------------------------------------------------");
    m_spirvBlocks[BASIC_CONSTANTS].push_back("; ---- Basic Constants ------------------------------------------------");
    m_spirvBlocks[COMPOSITE_TYPES].push_back("; ---- Composite Types ------------------------------------------------");
    m_spirvBlocks[COMPOSITE_CONSTANTS].push_back(
        "; ---- Composite Constants ------------------------------------------------");
    m_spirvBlocks[TENSOR_TYPES].push_back(
        "; ---- Tensor types ------------------------------------------------------------");
    m_spirvBlocks[COMPOSITE_TENSORS].push_back(
        "; ---- Composite Tensors ------------------------------------------------");
    m_spirvBlocks[OP_GRAPH_CONSTANTS].push_back(
        "; ---- Graph Constants ------------------------------------------------");
    m_spirvBlocks[POINTER_TYPES].push_back(
        "; ---- Pointers for interface variables ---------------------------------------");
    m_spirvBlocks[OP_VARIABLES].push_back(
        "; ---- Interface variables (descriptors) ---------------------------------------");
    m_spirvBlocks[OP_GRAPH_TYPES].push_back(
        "; ---- Graph signature ---------------------------------------------------------");
    m_spirvBlocks[OP_GRAPH_VARS].push_back(
        "; ---- Graph body --------------------------------------------------------------");

    // Add hardcoded blocks
    m_spirvBlocks[OP_GRAPH_END].push_back(spirvDeclaration("OpGraphEndARM"));
}

std::string TosaSpirv::bake(std::string entry_point)
{
    auto toStr = [this](const ResourceInformation &t) -> std::string { return "%" + this->spirvGraphParam(t); };

    // Add graph object (this will recursively trigger all fmt and variable definitions)

    std::string graphDeclaration = "OpGraphEntryPointARM %GRAPH_OBJECT \"ENTRY_POINT\" IN_TENSORS OUT_TENSORS";

    replaceAll(graphDeclaration, "GRAPH_OBJECT", spirvGraphObject(m_inputs, m_outputs));
    replaceAll(graphDeclaration, "ENTRY_POINT", entry_point);
    replaceAll(graphDeclaration, "IN_TENSORS", spirvJoin(m_inputs.data(), m_inputs.size(), " ", toStr));
    replaceAll(graphDeclaration, "OUT_TENSORS", spirvJoin(m_outputs.data(), m_outputs.size(), " ", toStr));
    m_spirvBlocks[OP_GRAPH_TYPES].push_back(spirvDeclaration(graphDeclaration));

    return entry_point;
}

std::string TosaSpirv::addAttributeTensor(TosaSpirv::format fmt, const std::vector<int64_t> &values, std::string label)
{
    return constantCompositeTensor(fmt, values.data(), values.size(), label);
}

std::string TosaSpirv::addAttribute(TosaSpirv::format fmt, uint64_t value, std::string label)
{
    return spirvConstant(fmt, value, label);
}

std::string TosaSpirv::addAttributeTensor(VkFormat fmt, const std::vector<int64_t> &values, std::string label)
{
    return addAttributeTensor(tosaSpirvFormat(fmt), values, label);
}

std::string TosaSpirv::addAttribute(VkFormat fmt, uint64_t value, std::string label)
{
    return addAttribute(tosaSpirvFormat(fmt), value, label);
}

std::string TosaSpirv::addResource(const ResourceInformation &resInfo)
{
    if (resInfo.isInput())
    {
        m_inputs.push_back(resInfo);
        return graphInput(resInfo);
    }
    else if (resInfo.isOutput())
    {
        m_outputs.push_back(resInfo);
        return typeTensor(resInfo);
    }
    else if (resInfo.isConstant())
    {
        return graphConstant(resInfo);
    }

    return "";
}

std::string TosaSpirv::defineTensor(VkFormat fmt, const int64_t *dims, uint32_t rank)
{
    return typeTensor(fmt, dims, rank);
}

std::string TosaSpirv::addSpirvOp(const std::string &op, const std::vector<std::string> &inputs,
                                  const std::string &output, const std::vector<std::string> &attributes)
{
    static uint64_t opId = 0;
    auto toStr           = [](std::string value) -> std::string { return "%" + value; };

    std::string opName  = "op_ID";
    std::string opValue = "OpExtInst %OUTPUT %TOSA_EXT_NAME OPERATION ATTRIBUTES INPUTS";

    opName = std::regex_replace(opName, std::regex("ID"), std::to_string(opId++));
    replaceAll(opValue, "OUTPUT", output);
    replaceAll(opValue, "TOSA_EXT_NAME", tosa_ext_name);
    replaceAll(opValue, "OPERATION", op);

    opValue = std::regex_replace(opValue, std::regex("INPUTS"), spirvJoin(inputs.data(), inputs.size(), " ", toStr));

    opValue = std::regex_replace(opValue, std::regex("ATTRIBUTES"),
                                 spirvJoin(attributes.data(), attributes.size(), " ", toStr));

    m_spirvBlocks[OP_GRAPH].push_back(spirvAssignment(opName, opValue));

    return opName;
}

void TosaSpirv::setOutput(const std::string &op, const ResourceInformation &resInfo)
{
    std::string graphDeclaration = "OpGraphSetOutputARM %OP %INDEX";
    replaceAll(graphDeclaration, "OP", op);
    replaceAll(graphDeclaration, "INDEX", spirvConstant(TosaSpirv::format::i32_t, resInfo.id));

    m_spirvBlocks[OP_GRAPH].push_back(spirvDeclaration(graphDeclaration));
}

void TosaSpirv::removeDuplicates(std::vector<std::string> &spirvLines)
{
    std::unordered_set<std::string> seen;

    auto unique_end = std::remove_if(spirvLines.begin(), spirvLines.end(),
                                     [&seen](const std::string &value)
                                     {
                                         if (seen.find(value) != seen.end())
                                         {
                                             return true;
                                         }
                                         else
                                         {
                                             seen.insert(value);
                                             return false;
                                         }
                                     });

    spirvLines.erase(unique_end, spirvLines.end());
}

std::string TosaSpirv::source()
{
    std::string result = "";

    for (uint64_t block_idx = 0; block_idx < NUM_SPIRV_SECTIONS; block_idx++)
    {
        removeDuplicates(m_spirvBlocks[block_idx]);
        for (const auto &spirvLine : m_spirvBlocks[block_idx])
        {
            result += spirvLine + "\n";
        }
    }

    return result;
}

} /* end namespace dataGraph */

} /* end namespace vkt */