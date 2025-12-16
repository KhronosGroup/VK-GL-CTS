#ifndef _VKTDATAGRAPHTOSASPIRV_HPP
#define _VKTDATAGRAPHTOSASPIRV_HPP

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

#include "../../tensor/vktTensorTestsUtil.hpp"
#include "../vktDataGraphTestUtil.hpp"
#include "spirv-tools/libspirv.hpp"

namespace vkt
{
namespace dataGraph
{

enum spirv_rounding_mode
{
    SINGLE_ROUND  = 1,
    INEXACT_ROUND = 2,
    DOUBLE_ROUND  = 3
};
enum spirv_acc_type
{
    INT32 = 1,
    FP16  = 2,
    FP32  = 3,
    INT48 = 4,
};
enum spirv_nan_mode
{
    PROPAGATE = 1,
    IGNORE    = 2,
};
enum spirv_resize_mode
{
    NEAREST_NEIGHBOR = 1,
    BILINEAR         = 2,
};

/**
 * @brief Utility class for the generation of spirv source code describing a neural graph.
 */
class TosaSpirv
{
public:
    static uint32_t spirvAccType(VkFormat format)
    {
        switch (format)
        {
        case VK_FORMAT_R32_UINT:
        case VK_FORMAT_R32_SINT:
            return spirv_acc_type::INT32;
        case VK_FORMAT_R16_SFLOAT:
            return spirv_acc_type::FP16;
        case VK_FORMAT_R32_SFLOAT:
            return spirv_acc_type::FP32;
        case VK_FORMAT_R64_UINT:
        case VK_FORMAT_R64_SINT:
            return spirv_acc_type::INT48;
        default:
            TCU_THROW(InternalError, "Unsupported format");
        }
        return 0;
    };

    static void spirvMessageConsumer(spv_message_level_t level, const spv_position_t &position, const char *message,
                                     std::string &errors)
    {
        switch (level)
        {
        case SPV_MSG_FATAL:
        case SPV_MSG_INTERNAL_ERROR:
        case SPV_MSG_ERROR:
            errors += "error: line " + std::to_string(position.index) + ": " + message + "\n";
            break;
        case SPV_MSG_WARNING:
            errors += "warning: line " + std::to_string(position.index) + ": " + message + "\n";
            break;
        case SPV_MSG_INFO:
            errors += "info: line " + std::to_string(position.index) + ": " + message + "\n";
            break;
        case SPV_MSG_DEBUG:
            break;
        }
    };

    /**
     * @brief Formats used in the spirv source code
     *
     */
    enum class format
    {
        invalid,
        bool_t,
        i8_t,
        i16_t,
        i32_t,
        i48_t,
        fp16_t,
        fp32_t,
    };

    /**
     * @brief Constructor.
     */
    TosaSpirv();

    /**
     * @brief Destructor: destroys the spirv code generator.
     */
    ~TosaSpirv() = default;

    /**
     * @brief Add an attribute tensor to the graph
     *
     * @param format  Format for the attributes
     * @param values  Values of the attribute
     * @param label   Optional label for the attribute
     *
     * @return the id of the attribute vector
     *
     */
    std::string addAttributeTensor(VkFormat format, const std::vector<int64_t> &values, std::string label = "");

    /**
     * @brief Add an attribute to the graph
     *
     * @param format  Format for the attribute
     * @param value   Value of the attribute
     * @param label   Optional label for the attribute
     *
     * @return the id of the attribute
     *
     */
    std::string addAttribute(VkFormat format, uint64_t value, std::string label = "");

    /**
     * @brief Add an attribute tensor to the graph
     *
     * @param fmt    Spirv fmt for the attributes
     * @param values  Values of the attribute
     * @param label   Optional label for the attribute
     *
     * @return the id of the attribute vector
     *
     */
    std::string addAttributeTensor(TosaSpirv::format fmt, const std::vector<int64_t> &values, std::string label = "");

    /**
     * @brief Add an attribute to the graph
     *
     * @param fmt    Spirv fmt for the attribute
     * @param value   Value of the attribute
     * @param label   Optional label for the attribute
     *
     * @return the id of the attribute
     *
     */
    std::string addAttribute(TosaSpirv::format fmt, uint64_t value, std::string label = "");

    /**
     * @brief Add an resource (input, output, constant) to the graph
     *
     * @param resInfo         The resource information
     *
     * @return the string id of the resource if valid. An empty string otherwise
     *
     */
    std::string addResource(const ResourceInformation &resInfo);

    /**
     * @brief Adds definition of resInfo to graph
     *
     * @param format  Format of the resInfo
     * @param dims    Dimensions of the resInfo
     * @param rank    Rank of the resInfo
     *
     * @return the string id of the resInfo definition
     *
     */
    std::string defineTensor(const VkFormat &format, const int64_t *dims, const uint32_t &rank);

    /**
     * @brief Add a TOSA operator to the graph
     *
     * @param op            The TOSA operator
     * @param inputs        List of string IDs for the operator inputs
     * @param output        string ID for the operator output
     * @param attributes    List of string IDs for the operator attributes
     *
     * @return the string id of the operator
     *
     */
    std::string addSpirvOp(const std::string &op, const std::vector<std::string> &inputs, const std::string &output,
                           const std::vector<std::string> &attributes = {});

    /**
     * @brief Set the output of the neural graph
     *
     * @param output  String ID representing the output of the neural graph
     *
     */
    void setOutput(const std::string &output);

    /**
     * @brief Set the outputs of the neural graph
     *
     * @param output  Vector of string IDs representing the output of the neural graph
     *
     */
    void setOutputs(const std::vector<std::string> &output);

    /**
     * @brief Based on the added tensors, constants and operators, prepare for the spirv source generation
     *
     * @return the name of the entrypoint for the neural graph
     */
    std::string bake(std::string entry_point = "main");

    /**
     * @brief Generate the spirv code for the neural graph
     *
     * @return a string containing the spirv source
     */
    std::string source();

private:
    /**
     * @brief List of the different blocks of spirv source code for a neural graph
     *
     * @note the enum spirvOrder will reflects the block order in the generated spirv source, e.g. lines in the BASIC_TYPES block will appear before the lines in the BASIC_CONSTANTS block
     *
     */
    enum spirvOrder
    {
        OP_CAPABILITY,
        OP_EXTENSION,
        OP_INIT,
        OP_NAMES,
        OP_DECORATORS,
        BASIC_TYPES,
        BASIC_CONSTANTS,
        COMPOSITE_TYPES,
        COMPOSITE_CONSTANTS,
        TENSOR_TYPES,
        COMPOSITE_TENSORS,
        OP_GRAPH_CONSTANTS,
        POINTER_TYPES,
        OP_VARIABLES,
        OP_GRAPH_TYPES,
        OP_GRAPH_VARS,
        OP_GRAPH,
        OP_GRAPH_END,
        NUM_SPIRV_SECTIONS,
    };

    /* Internal utilities to convert SPIRV TosaSpirv::format to strings */

    void replaceAll(std::string &str, const std::string &what, const std::string &replacement);

    std::string typeToString(TosaSpirv::format fmt);
    std::string typeArray(TosaSpirv::format fmt, int64_t size);
    std::string typeVector(TosaSpirv::format fmt, int64_t size);
    std::string spirvConstant(TosaSpirv::format fmt, std::string value, std::string label = "");
    std::string spirvConstant(TosaSpirv::format fmt, uint64_t value, std::string label = "");
    std::string constantCompositeArray(TosaSpirv::format fmt, const int64_t *values, int64_t size,
                                       std::string label = "");
    std::string constantCompositeVector(TosaSpirv::format fmt, const int64_t *values, int64_t size,
                                        std::string label = "");
    std::string constantCompositeTensor(TosaSpirv::format fmt, const int64_t *values, int64_t size,
                                        std::string label = "");
    std::string constantComposite(std::string varName, TosaSpirv::format fmt, spirvOrder spirvOrder,
                                  const int64_t *values, int64_t size, std::string label = "");
    std::string typeTensor(const ResourceInformation &resInfo);
    std::string typeTensor(const TosaSpirv::format fmt, const int64_t *dims, const uint32_t &rank);
    std::string typeTensor(const VkFormat &format, const int64_t *dims, const uint32_t &rank);
    std::string typeTensorPointer(const ResourceInformation &resInfo);

    std::string spirvVariable(const ResourceInformation &resInfo);
    std::string graphInput(const ResourceInformation &resInfo);
    std::string graphConstant(const ResourceInformation &resInfo);

    std::string typeGraph(const std::vector<ResourceInformation> &resInfoInputs,
                          const std::vector<ResourceInformation> &resInfoOutputs);
    std::string spirvGraphObject(const std::vector<ResourceInformation> &resInfoInputs,
                                 const std::vector<ResourceInformation> &resInfoOutputs);

    std::string spirvGraphParam(const ResourceInformation &resInfo);

    /**
     * @brief Generate a padded spirv assignment
     *
     * @param name   Name of the variable
     * @param value  Value to assign to the variable
     *
     * @return The string corresponding to the spirv assignment, e.g. `%name = value`
     *
     */
    std::string spirvAssignment(const std::string &name, const std::string &value);

    /**
     * @brief Generate a padded spirv declaration
     *
     * @param declaration  Declaration to output
     *
     * @return The string corresponding to the spirv declaration
     *
     */
    std::string spirvDeclaration(const std::string &declaration);

    /**
     * @brief Return the TosaSpirv::format correspondent to a Vulkan format
     *
     * @param format  Vulkan format
     *
     * @return The corresponding TosaSpirv::format if exists. Otherwise, spirvInvalid.
     *
     */
    TosaSpirv::format tosaSpirvFormat(VkFormat format);

    /**
     * @brief Remove duplicate source lines from vector, preserving the order
     *
     * @param spirvLines  Vector of spirv source lines from which to remove duplicates
     *
     */
    void removeDuplicates(std::vector<std::string> &spirvLines);

    /**
     * @brief Join all values into a string using a string as separator.
     *
     * @param values     Pointer to the values to be joined
     * @param size       Number of valued to join
     * @param separator  Separator to use between values
     * @param decorator  Function to pre-process each value before being joined
     *
     * @return A string with all values processed and joined together
     *
     * Before being joined, each value gets processed by a decorator function.
     *
     */
    template <typename T, typename Func>
    std::string spirvJoin(T *values, int64_t size, std::string separator, Func decorator)
    {
        std::string result = "";
        for (uint64_t idx = 0; idx < static_cast<uint64_t>(size); idx++)
        {
            result += (idx != 0) ? separator : "";
            result += decorator(values[idx]);
        }
        return result;
    }

    /**
     * @brief Padding to use in the spirv code generation (only to improve readability)
     */
    static constexpr uint64_t sourcePadding = 30;

    /**
     * @brief Name of the tosa variable in the spirv code
     */
    const std::string tosa_ext_name = "tosa";

    /**
     * @brief list of source code blocks (each string in a block is a code line)
     *
     * @note The code block order output is dicated by the `spirvOrder` enum.
     */
    std::vector<std::string> m_spirvBlocks[NUM_SPIRV_SECTIONS];

    /**
     * @brief Graph inputs
     */
    std::vector<ResourceInformation> m_inputs;

    /**
     * @brief Graph outputs
     */
    std::vector<ResourceInformation> m_outputs;

    /**
     * @brief Spirv capabilities needed by a neural graph
     */
    const std::vector<std::string> capabilities = {
        "GraphARM", "TensorsARM", "Int8", "Int16", "Int64", "Float16", "Shader", "VulkanMemoryModel", "Matrix",
    };

    /**
     * @brief Spirv extensions needed by a neural graph
     */
    const std::vector<std::string> extensions = {
        "SPV_ARM_graph",
        "SPV_ARM_tensors",
        "SPV_KHR_vulkan_memory_model",
    };
};

} // namespace dataGraph
} // namespace vkt

#endif // _VKTDATAGRAPHTOSASPIRV_HPP
