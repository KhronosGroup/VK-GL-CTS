/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2025 Arm Ltd.
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
    auto to_str   = [](uint64_t value) -> std::string { return std::to_string(value); };
    auto to_const = [this, fmt](uint64_t value) -> std::string { return "%" + spirvConstant(fmt, value); };

    std::string composite_name  = label.empty() ? (varName + "_VALUES") : label;
    std::string composite_value = "OpConstantComposite %" + varName + " VALUES";

    replaceAll(composite_name, "VALUES", spirvJoin(values, size, "_", to_str));
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

std::string TosaSpirv::typeTensor(const TosaSpirv::format fmt, const int64_t *dims, const uint32_t &rank)
{
    auto to_str = [](uint64_t value) -> std::string { return std::to_string(value); };

    std::string tensor_name  = "TYPE_SHAPE_tensor";
    std::string tensor_value = "OpTypeTensorARM %TYPE %RANK %SHAPE";

    replaceAll(tensor_name, "TYPE", typeToString(fmt));
    replaceAll(tensor_name, "SHAPE", spirvJoin(dims, rank, "_", to_str));
    replaceAll(tensor_value, "TYPE", typeToString(fmt));
    replaceAll(tensor_value, "RANK", spirvConstant(TosaSpirv::format::i32_t, rank));
    replaceAll(tensor_value, "SHAPE", constantCompositeArray(TosaSpirv::format::i32_t, dims, rank));
    m_spirvBlocks[TENSOR_TYPES].push_back(spirvAssignment(tensor_name, tensor_value));

    return tensor_name;
}

std::string TosaSpirv::typeTensor(const VkFormat &fmt, const int64_t *dims, const uint32_t &rank)
{
    return typeTensor(tosaSpirvFormat(fmt), dims, rank);
}

std::string TosaSpirv::typeTensorPointer(const ResourceInformation &resInfo)
{
    std::string ptr_value = typeTensor(resInfo);
    std::string ptr_name  = ptr_value + "_ptr";

    m_spirvBlocks[POINTER_TYPES].push_back(spirvAssignment(ptr_name, "OpTypePointer UniformConstant %" + ptr_value));
    return ptr_name;
}

std::string TosaSpirv::spirvVariable(const ResourceInformation &resInfo)
{
    std::string var_name  = (resInfo.type == RESOURCE_TYPE_INPUT) ? "main_arg_ID" : "main_res_ID";
    std::string var_value = "OpVariable %TENSOR_PTR UniformConstant";

    replaceAll(var_name, "ID", std::to_string(resInfo.id));
    replaceAll(var_value, "TENSOR_PTR", typeTensorPointer(resInfo));
    m_spirvBlocks[OP_VARIABLES].push_back(spirvAssignment(var_name, var_value));

    return var_name;
}

std::string TosaSpirv::spirvGraphParam(const ResourceInformation &resInfo)
{
    std::string op_name       = "OpName %VAR_NAME \"VAR_NAME\"";
    std::string op_binding    = "OpDecorate %VAR_NAME Binding BINDING";
    std::string op_descriptor = "OpDecorate %VAR_NAME DescriptorSet DESCRIPTOR";

    std::string var_name = spirvVariable(resInfo);

    replaceAll(op_name, "VAR_NAME", var_name);
    m_spirvBlocks[OP_NAMES].push_back(spirvDeclaration(op_name));

    replaceAll(op_binding, "VAR_NAME", var_name);
    replaceAll(op_binding, "BINDING", std::to_string(resInfo.binding));
    m_spirvBlocks[OP_DECORATORS].push_back(spirvDeclaration(op_binding));

    replaceAll(op_descriptor, "VAR_NAME", var_name);
    replaceAll(op_descriptor, "DESCRIPTOR", std::to_string(resInfo.descriptorSet));
    m_spirvBlocks[OP_DECORATORS].push_back(spirvDeclaration(op_descriptor));

    return var_name;
}

std::string TosaSpirv::graphInput(const ResourceInformation &resInfo)
{
    std::string param_name  = "in_ID";
    std::string param_value = "OpGraphInputARM %TENSOR %INDEX";

    replaceAll(param_name, "ID", std::to_string(resInfo.id));
    replaceAll(param_value, "TENSOR", typeTensor(resInfo));
    replaceAll(param_value, "INDEX", spirvConstant(TosaSpirv::format::i32_t, resInfo.id));
    m_spirvBlocks[OP_GRAPH].push_back(spirvAssignment(param_name, param_value));

    return param_name;
}

std::string TosaSpirv::graphConstant(const ResourceInformation &resInfo)
{
    std::string param_name  = resInfo.label.empty() ? "const_input_ID" : resInfo.label;
    std::string param_value = "OpGraphConstantARM %TENSOR ID";

    replaceAll(param_name, "ID", std::to_string(resInfo.id));
    replaceAll(param_value, "TENSOR", typeTensor(resInfo));
    replaceAll(param_value, "ID", std::to_string(resInfo.id));
    m_spirvBlocks[OP_GRAPH_CONSTANTS].push_back(spirvAssignment(param_name, param_value));

    return param_name;
}

std::string TosaSpirv::typeGraph(const std::vector<ResourceInformation> &resInfoInputs,
                                 const std::vector<ResourceInformation> &resInfoOutputs)
{
    auto to_str = [this](const ResourceInformation &resInfo) -> std::string { return "%" + this->typeTensor(resInfo); };

    std::string type_name  = "graph_type";
    std::string type_value = "OpTypeGraphARM DIM IN_TENSORS OUT_TENSORS";

    type_value = std::regex_replace(type_value, std::regex("DIM"), std::to_string(resInfoInputs.size()));
    type_value = std::regex_replace(type_value, std::regex("IN_TENSORS"),
                                    spirvJoin(resInfoInputs.data(), resInfoInputs.size(), " ", to_str));
    type_value = std::regex_replace(type_value, std::regex("OUT_TENSORS"),
                                    spirvJoin(resInfoOutputs.data(), resInfoOutputs.size(), " ", to_str));
    m_spirvBlocks[OP_GRAPH_TYPES].push_back(spirvAssignment(type_name, type_value));

    return type_name;
}

std::string TosaSpirv::spirvGraphObject(const std::vector<ResourceInformation> &resInfoInputs,
                                        const std::vector<ResourceInformation> &resInfoOutputs)
{
    std::string graph_name  = "graph_0";
    std::string graph_value = "OpGraphARM %GRAPH_TYPE";

    replaceAll(graph_value, "GRAPH_TYPE", typeGraph(resInfoInputs, resInfoOutputs));
    m_spirvBlocks[OP_GRAPH_VARS].push_back(spirvAssignment(graph_name, graph_value));

    return graph_name;
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
    m_spirvBlocks[OP_GRAPH_END].push_back(spirvDeclaration("OpGraphEndARM"));
}

std::string TosaSpirv::bake(std::string entry_point)
{
    auto params_to_str = [this](const ResourceInformation &t) -> std::string { return "%" + this->spirvGraphParam(t); };

    // Add graph object (this will recursively trigger all fmt and variable definitions)

    std::string graph_declaration = "OpGraphEntryPointARM %GRAPH_OBJECT \"ENTRY_POINT\" IN_PARAMS OUT_PARAMS";

    replaceAll(graph_declaration, "GRAPH_OBJECT", spirvGraphObject(m_inputs, m_outputs));
    replaceAll(graph_declaration, "ENTRY_POINT", entry_point);
    replaceAll(graph_declaration, "IN_PARAMS", spirvJoin(m_inputs.data(), m_inputs.size(), " ", params_to_str));
    replaceAll(graph_declaration, "OUT_PARAMS", spirvJoin(m_outputs.data(), m_outputs.size(), " ", params_to_str));
    m_spirvBlocks[OP_GRAPH_TYPES].push_back(spirvDeclaration(graph_declaration));

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

std::string TosaSpirv::defineTensor(const VkFormat &fmt, const int64_t *dims, const uint32_t &rank)
{
    return typeTensor(fmt, dims, rank);
}

std::string TosaSpirv::addSpirvOp(const std::string &op, const std::vector<std::string> &inputs,
                                  const std::string &output, const std::vector<std::string> &attributes)
{
    static uint64_t op_id = 0;
    auto to_str           = [](std::string value) -> std::string { return "%" + value; };

    std::string op_name  = "op_ID";
    std::string op_value = "OpExtInst %OUTPUT %TOSA_EXT_NAME OPERATION ATTRIBUTES INPUTS";

    op_name = std::regex_replace(op_name, std::regex("ID"), std::to_string(op_id++));
    replaceAll(op_value, "OUTPUT", output);
    replaceAll(op_value, "TOSA_EXT_NAME", tosa_ext_name);
    replaceAll(op_value, "OPERATION", op);

    op_value = std::regex_replace(op_value, std::regex("INPUTS"), spirvJoin(inputs.data(), inputs.size(), " ", to_str));

    op_value = std::regex_replace(op_value, std::regex("ATTRIBUTES"),
                                  spirvJoin(attributes.data(), attributes.size(), " ", to_str));

    m_spirvBlocks[OP_GRAPH].push_back(spirvAssignment(op_name, op_value));

    return op_name;
}

void TosaSpirv::setOutput(const std::string &output)
{
    setOutputs({output});
}

void TosaSpirv::setOutputs(const std::vector<std::string> &output)
{
    /* add one output per line with increasing index */
    uint32_t index = 0;
    for (const std::string &out : output)
    {
        std::string spirvGraph_output = "OpGraphSetOutputARM";
        spirvGraph_output += " %" + out + " %" + spirvConstant(TosaSpirv::format::i32_t, index++);
        m_spirvBlocks[OP_GRAPH].push_back(spirvDeclaration(spirvGraph_output));
    }
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