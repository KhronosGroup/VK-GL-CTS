/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2023 The Khronos Group Inc.
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
 *//*!
 * \file
 * \brief SPIR-V Assembly Tests for the SPV_KHR_shader_untyped_pointers extension
 *//*--------------------------------------------------------------------*/

#include "vktSpvAsmUntypedPointersTests.hpp"

#include "tcuTestCase.hpp"
#include "tcuStringTemplate.hpp"

#include "vktSpvAsmComputeShaderCase.hpp"
#include "vktSpvAsmComputeShaderTestUtil.hpp"
#include "vktTestGroupUtil.hpp"

#include "deUniquePtr.hpp"
#include "vkPrograms.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkObjUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkBuilderUtil.hpp"

#include <algorithm>
#include <map>
#include <string>
#include <vector>

#define DE_ENUM_COUNT(enumClass) static_cast<uint32_t>((enumClass::_ENUM_COUNT))
#define DE_ENUM_INDEX(enumVal) static_cast<uint32_t>((enumVal))

namespace vkt
{
namespace SpirVAssembly
{
namespace Constants
{
constexpr uint32_t numThreads         = 64;
constexpr uint32_t uniformAlignment   = 16;
constexpr uint32_t pushConstArraySize = 4;
} // namespace Constants

enum class DataTypes : uint8_t
{
    UINT8 = 0,
    INT8,
    UINT16,
    INT16,
    FLOAT16,
    UINT32,
    INT32,
    FLOAT32,
    UINT64,
    INT64,
    FLOAT64,
    _ENUM_COUNT,
};
using DATA_TYPE = DataTypes;

enum class CompositeDataTypes : uint8_t
{
    VEC2_UINT8 = 0,
    VEC3_UINT8,
    VEC4_UINT8,
    VEC2_INT8,
    VEC3_INT8,
    VEC4_INT8,
    VEC2_UINT16,
    VEC3_UINT16,
    VEC4_UINT16,
    VEC2_INT16,
    VEC3_INT16,
    VEC4_INT16,
    VEC2_FLOAT16,
    VEC3_FLOAT16,
    VEC4_FLOAT16,
    VEC2_UINT32,
    VEC3_UINT32,
    VEC4_UINT32,
    VEC2_INT32,
    VEC3_INT32,
    VEC4_INT32,
    VEC2_FLOAT32,
    VEC3_FLOAT32,
    VEC4_FLOAT32,
    VEC2_UINT64,
    VEC3_UINT64,
    VEC4_UINT64,
    VEC2_INT64,
    VEC3_INT64,
    VEC4_INT64,
    VEC2_FLOAT64,
    VEC3_FLOAT64,
    VEC4_FLOAT64,
    _ENUM_COUNT,
};
using COMPOSITE_DATA_TYPE = CompositeDataTypes;

enum class OperationTypes : uint8_t
{
    NORMAL = 0,
    ATOMIC,
    _ENUM_COUNT,
};
using OPERATION_TYPE = OperationTypes;

enum class ContainerTypes : uint8_t
{
    STORAGE_BUFFER = 0,
    UNIFORM,
    PUSH_CONSTANT,
    WORKGROUP,
    _ENUM_COUNT,
};
using CONTAINER_TYPE = ContainerTypes;

enum class MemoryModelTypes : uint8_t
{
    GLSL = 0,
    VULKAN,
    _ENUM_COUNT,
};
using MEMORY_MODEL_TYPE = MemoryModelTypes;

enum class CopyOperationTypes : uint8_t
{
    COPY_OBJECT = 0,
    COPY_MEMORY,
    COPY_MEMORY_SIZED,
    _ENUM_COUNT,
};
using COPY_OPERATION_TYPE = CopyOperationTypes;

enum class BaseTestCases : uint8_t
{
    LOAD = 0,
    STORE,
    COPY_FROM,
    COPY_TO,
    ARRAY_LENGTH,
    DESCRIPTOR_ARRAY,
    _ENUM_COUNT,
};
using BASE_TEST_CASE = BaseTestCases;

enum class TypePunningTestCases
{
    LOAD_SAME_SIZE_TYPES = 0,
    LOAD_SCALAR_VECTOR,
    LOAD_VECTOR_SCALAR,
    STORE_SAME_SIZE_TYPES,
    STORE_SCALAR_VECTOR,
    STORE_VECTOR_SCALAR,
    COPY_FROM_SAME_SIZE_TYPES,
    COPY_FROM_SCALAR_VECTOR,
    COPY_FROM_VECTOR_SCALAR,
    COPY_TO_SAME_SIZE_TYPES,
    COPY_TO_SCALAR_VECTOR,
    COPY_TO_VECTOR_SCALAR,
    MULTIPLE_ACCESS_CHAINS,
    CUSTOM_STRUCT_TYPE,
    _ENUM_COUNT,
};
using TYPE_PUNNING_TEST_CASE = TypePunningTestCases;

enum class AtomicTestCases : uint8_t
{
    OP_ATOMIC_LOAD = 0,
    OP_ATOMIC_STORE,
    OP_ATOMIC_EXCHANGE,
    OP_ATOMIC_COMPARE_EXCHANGE,
    OP_ATOMIC_INCREMENT,
    OP_ATOMIC_DECREMENT,
    OP_ATOMIC_ADD,
    OP_ATOMIC_SUB,
    OP_ATOMIC_MIN,
    OP_ATOMIC_MAX,
    OP_ATOMIC_AND,
    OP_ATOMIC_OR,
    OP_ATOMIC_XOR,
    _ENUM_COUNT,
};
using ATOMIC_TEST_CASE = AtomicTestCases;

enum class PointerTestCases : uint8_t
{
    OP_BITCAST_FROM_UNTYPED_PHYSICAL_STORAGE = 0,
    OP_BITCAST_TO_UNTYPED_PHYSICAL_STORAGE,
    OP_SELECT_PHYSICAL_STORAGE,
    OP_PHI_PHYSICAL_STORAGE,
    OP_PTR_ACCESS_CHAIN_PHYSICAL_STORAGE,
    OP_FUNCTION_CALL_PHYSICAL_STORAGE,
    OP_SELECT_VARIABLE_PTR,
    OP_PHI_VARIABLE_PTR,
    OP_PTR_ACCESS_CHAIN_VARIABLE_PTR,
    OP_PTR_EQUAL_VARIABLE_PTR,
    OP_PTR_NOT_EQUAL_VARIABLE_PTR,
    OP_PTR_DIFF_VARIABLE_PTR,
    OP_FUNCTION_CALL_VARIABLE_PTR,
    FUNCTION_VARIABLE_VARIABLE_PTR,
    PRIVATE_VARIABLE_VARIABLE_PTR,
    MULTIPLE_ACCESS_CHAINS_VARIABLE_PTR,
    WORKGROUP_MEMORY_VARIABLE_PTR,
    _ENUM_COUNT,
};
using POINTER_TEST_CASE = PointerTestCases;

enum class MemoryInterpretationTestCases : uint8_t
{
    LARGE_ARRAY_STRIDE = 0,
    NON_ZERO_OFFSET,
    MIXED_OFFSETS,
    MULTIPLE_ACCESS_CHAINS,
    SHORT2_NO_STORAGE_CAP,
    CHAR4_NO_STORAGE_CAP,
    CHAR2_16BIT_STORAGE_CAP,
    UNTYPED_FROM_TYPED_VAR,
    UNTYPED_FROM_TYPED_ACCESS_CHAIN,
    _ENUM_COUNT,
};
using MEMORY_INTERPRETATION_TEST_CASE = MemoryInterpretationTestCases;

enum class BlockArrayTestCases : uint8_t
{
    BASIC = 0,
    REINTERPRET_BLOCK_NORMAL_ACCESS_CHAIN,
    REINTERPRET_BLOCK_NORMAL_PTR_ACCESS_CHAIN,
    REINTERPRET_BLOCK_UNTYPED_ACCESS_CHAIN,
    REINTERPRET_BLOCK_UNTYPED_PTR_ACCESS_CHAIN,
    SELECT_BLOCK_NORMAL_ACCESS_CHAIN,
    SELECT_BLOCK_NORMAL_PTR_ACCESS_CHAIN,
    SELECT_BLOCK_UNTYPED_ACCESS_CHAIN,
    SELECT_BLOCK_UNTYPED_PTR_ACCESS_CHAIN,
    _ENUM_COUNT,
};
using BLOCK_ARRAY_TEST_CASE = BlockArrayTestCases;

enum class WorkgroupTestCases : uint8_t
{
    ALIASED = 0,
    NOT_ALIASED,
    _ENUM_COUNT,
};
using WORKGROUP_TEST_CASE = WorkgroupTestCases;

enum class CooperativeMatrixTestCases
{
    BASIC_LOAD = 0,
    BASIC_STORE,
    TYPE_PUNNING_LOAD,
    TYPE_PUNNING_STORE,
    MIXED_LOAD,
    MIXED_STORE,
    _ENUM_COUNT,
};
using COOPERATIVE_MATRIX_TEST_CASE = CooperativeMatrixTestCases;

enum class MatrixLayouts
{
    ROW_MAJOR = 0,
    COL_MAJOR,
    _ENUM_COUNT,
};
using MATRIX_LAYOUT = MatrixLayouts;

enum class MatrixTypes
{
    A = 0,
    B,
    ACCUMULATOR,
    _ENUM_COUNT,
};
using MATRIX_TYPE = MatrixTypes;

struct Operation
{
    const char *pOperation; // SPIR-V operation
    const char *pArgs;      // Additional arguments
    OPERATION_TYPE type;    // Operation type
};

struct CopyOperation
{
    const char *pCopyOp;      // SPIR-V copy operation
    COPY_OPERATION_TYPE type; // Copy operation type
};

static const DATA_TYPE BASE_DATA_TYPE_CASES[] = {
    DataTypes::UINT8, DataTypes::INT8,    DataTypes::UINT16, DataTypes::INT16, DataTypes::FLOAT16, DataTypes::UINT32,
    DataTypes::INT32, DataTypes::FLOAT32, DataTypes::UINT64, DataTypes::INT64, DataTypes::FLOAT64,
};

// 8 and 16 bit atomic int operations are not available on known devices
static const DATA_TYPE ATOMIC_DATA_TYPE_CASES[] = {
    DataTypes::FLOAT16, DataTypes::UINT32, DataTypes::INT32,   DataTypes::FLOAT32,
    DataTypes::UINT64,  DataTypes::INT64,  DataTypes::FLOAT64,
};

static const DATA_TYPE ATOMIC_INT_DATA_TYPE_CASES[] = {
    DataTypes::UINT32,
    DataTypes::INT32,
    DataTypes::UINT64,
    DataTypes::INT64,
};

static const COMPOSITE_DATA_TYPE COMPOSITE_DATA_TYPE_CASES[] = {
    CompositeDataTypes::VEC2_UINT8,   CompositeDataTypes::VEC3_UINT8,   CompositeDataTypes::VEC4_UINT8,
    CompositeDataTypes::VEC2_INT8,    CompositeDataTypes::VEC3_INT8,    CompositeDataTypes::VEC4_INT8,
    CompositeDataTypes::VEC2_UINT16,  CompositeDataTypes::VEC3_UINT16,  CompositeDataTypes::VEC4_UINT16,
    CompositeDataTypes::VEC2_INT16,   CompositeDataTypes::VEC3_INT16,   CompositeDataTypes::VEC4_INT16,
    CompositeDataTypes::VEC2_FLOAT16, CompositeDataTypes::VEC3_FLOAT16, CompositeDataTypes::VEC4_FLOAT16,
    CompositeDataTypes::VEC2_UINT32,  CompositeDataTypes::VEC3_UINT32,  CompositeDataTypes::VEC4_UINT32,
    CompositeDataTypes::VEC2_INT32,   CompositeDataTypes::VEC3_INT32,   CompositeDataTypes::VEC4_INT32,
    CompositeDataTypes::VEC2_FLOAT32, CompositeDataTypes::VEC3_FLOAT32, CompositeDataTypes::VEC4_FLOAT32,
    CompositeDataTypes::VEC2_UINT64,  CompositeDataTypes::VEC3_UINT64,  CompositeDataTypes::VEC4_UINT64,
    CompositeDataTypes::VEC2_INT64,   CompositeDataTypes::VEC3_INT64,   CompositeDataTypes::VEC4_INT64,
    CompositeDataTypes::VEC2_FLOAT64, CompositeDataTypes::VEC3_FLOAT64, CompositeDataTypes::VEC4_FLOAT64,
};

static const CONTAINER_TYPE LOAD_CONTAINER_TYPE_CASES[] = {
    ContainerTypes::STORAGE_BUFFER,
    ContainerTypes::UNIFORM,
    ContainerTypes::PUSH_CONSTANT,
};

static const Operation LOAD_OPERATION_CASES[] = {
    {"OpLoad", "", OperationTypes::NORMAL},
    {"OpAtomicLoad", "%c_uint32_1 %c_uint32_0", OperationTypes::ATOMIC},
};

static const Operation STORE_OPERATION_CASES[] = {
    {"OpStore", "", OperationTypes::NORMAL},
    {"OpAtomicStore", "%c_uint32_1 %c_uint32_0", OperationTypes::ATOMIC},
};

static const CopyOperation COPY_OPERATION_CASES[] = {
    {"%object_loc         = OpLoad       %${copyType} %input_data_var_loc\n"
     "%coppied_object_loc = OpCopyObject %${copyType} %object_loc\n"
     "                      OpStore %output_data_var_loc %coppied_object_loc\n",
     CopyOperationTypes::COPY_OBJECT},
    {"OpCopyMemory          %output_data_var_loc %input_data_var_loc", CopyOperationTypes::COPY_MEMORY},
    {"OpCopyMemorySized     %output_data_var_loc %input_data_var_loc %c_uint32_data_size",
     CopyOperationTypes::COPY_MEMORY_SIZED},
};

static const MATRIX_TYPE MATRIX_USE_CASES[] = {
    MatrixTypes::A,
    MatrixTypes::B,
    MatrixTypes::ACCUMULATOR,
};

static const MATRIX_LAYOUT MATRIX_LAYOUT_CASES[] = {
    MatrixLayouts::ROW_MAJOR,
    MatrixLayouts::COL_MAJOR,
};

static uint32_t getSizeInBytes(DATA_TYPE type)
{
    static const uint32_t sizeTable[DE_ENUM_COUNT(DataTypes)] = {
        1, // UINT8
        1, // INT8
        2, // UINT16
        2, // INT16
        2, // FLOAT16
        4, // UINT32
        4, // INT32
        4, // FLOAT32
        8, // UINT64
        8, // INT64
        8, // FLOAT64
    };

    return sizeTable[DE_ENUM_INDEX(type)];
}

static uint32_t getSizeInBytes(COMPOSITE_DATA_TYPE type)
{
    static const uint32_t sizeTable[DE_ENUM_COUNT(CompositeDataTypes)] = {
        2,  // VEC2_UINT8
        3,  // VEC3_UINT8
        4,  // VEC4_UINT8
        2,  // VEC2_INT8
        3,  // VEC3_INT8
        4,  // VEC4_INT8
        4,  // VEC2_UINT16
        6,  // VEC3_UINT16
        8,  // VEC4_UINT16
        4,  // VEC2_INT16
        6,  // VEC3_INT16
        8,  // VEC4_INT16
        4,  // VEC2_FLOAT16
        6,  // VEC3_FLOAT16
        8,  // VEC4_FLOAT16
        8,  // VEC2_UINT32
        12, // VEC3_UINT32
        16, // VEC4_UINT32
        8,  // VEC2_INT32
        12, // VEC3_INT32
        16, // VEC4_INT32
        8,  // VEC2_FLOAT32
        12, // VEC3_FLOAT32
        16, // VEC4_FLOAT32
        16, // VEC2_UINT64
        24, // VEC3_UINT64
        32, // VEC4_UINT64
        16, // VEC2_INT64
        24, // VEC3_INT64
        32, // VEC4_INT64
        16, // VEC2_FLOAT64
        24, // VEC3_FLOAT64
        32, // VEC4_FLOAT64
    };

    return sizeTable[DE_ENUM_INDEX(type)];
}

static uint32_t getElementCount(COMPOSITE_DATA_TYPE type)
{
    static const uint32_t elemCountTable[DE_ENUM_COUNT(CompositeDataTypes)] = {
        2, // VEC2_UINT8
        3, // VEC3_UINT8
        4, // VEC4_UINT8
        2, // VEC2_INT8
        3, // VEC3_INT8
        4, // VEC4_INT8
        2, // VEC2_UINT16
        3, // VEC3_UINT16
        4, // VEC4_UINT16
        2, // VEC2_INT16
        3, // VEC3_INT16
        4, // VEC4_INT16
        2, // VEC2_FLOAT16
        3, // VEC3_FLOAT16
        4, // VEC4_FLOAT16
        2, // VEC2_UINT32
        3, // VEC3_UINT32
        4, // VEC4_UINT32
        2, // VEC2_INT32
        3, // VEC3_INT32
        4, // VEC4_INT32
        2, // VEC2_FLOAT32
        3, // VEC3_FLOAT32
        4, // VEC4_FLOAT32
        2, // VEC2_UINT64
        3, // VEC3_UINT64
        4, // VEC4_UINT64
        2, // VEC2_INT64
        3, // VEC3_INT64
        4, // VEC4_INT64
        2, // VEC2_FLOAT64
        3, // VEC3_FLOAT64
        4, // VEC4_FLOAT64
    };

    return elemCountTable[DE_ENUM_INDEX(type)];
}

static DATA_TYPE getCompositeBaseDataType(COMPOSITE_DATA_TYPE type)
{
    static const DATA_TYPE typeTable[DE_ENUM_COUNT(CompositeDataTypes)] = {
        DataTypes::UINT8,   // VEC2_UINT8
        DataTypes::UINT8,   // VEC3_UINT8
        DataTypes::UINT8,   // VEC4_UINT8
        DataTypes::INT8,    // VEC2_INT8
        DataTypes::INT8,    // VEC3_INT8
        DataTypes::INT8,    // VEC4_INT8
        DataTypes::UINT16,  // VEC2_UINT16
        DataTypes::UINT16,  // VEC3_UINT16
        DataTypes::UINT16,  // VEC4_UINT16
        DataTypes::INT16,   // VEC2_INT16
        DataTypes::INT16,   // VEC3_INT16
        DataTypes::INT16,   // VEC4_INT16
        DataTypes::FLOAT16, // VEC2_FLOAT16
        DataTypes::FLOAT16, // VEC3_FLOAT16
        DataTypes::FLOAT16, // VEC4_FLOAT16
        DataTypes::UINT32,  // VEC2_UINT32
        DataTypes::UINT32,  // VEC3_UINT32
        DataTypes::UINT32,  // VEC4_UINT32
        DataTypes::INT32,   // VEC2_INT32
        DataTypes::INT32,   // VEC3_INT32
        DataTypes::INT32,   // VEC4_INT32
        DataTypes::FLOAT32, // VEC2_FLOAT32
        DataTypes::FLOAT32, // VEC3_FLOAT32
        DataTypes::FLOAT32, // VEC4_FLOAT32
        DataTypes::UINT64,  // VEC2_UINT64
        DataTypes::UINT64,  // VEC3_UINT64
        DataTypes::UINT64,  // VEC4_UINT64
        DataTypes::INT64,   // VEC2_INT64
        DataTypes::INT64,   // VEC3_INT64
        DataTypes::INT64,   // VEC4_INT64
        DataTypes::FLOAT64, // VEC2_FLOAT64
        DataTypes::FLOAT64, // VEC3_FLOAT64
        DataTypes::FLOAT64, // VEC4_FLOAT64
    };

    return typeTable[DE_ENUM_INDEX(type)];
}

static std::vector<DATA_TYPE> getSameSizeBaseDataType(DATA_TYPE type)
{
    static const std::vector<DATA_TYPE> sameSizeDataTable[DE_ENUM_COUNT(DataTypes)] = {
        {DataTypes::INT8},                       // UINT8
        {DataTypes::UINT8},                      // INT8
        {DataTypes::INT16, DataTypes::FLOAT16},  // UINT16
        {DataTypes::UINT16, DataTypes::FLOAT16}, // INT16
        {DataTypes::UINT16, DataTypes::INT16},   // FLOAT16
        {DataTypes::INT32, DataTypes::FLOAT32},  // UINT32
        {DataTypes::UINT32, DataTypes::FLOAT32}, // INT32
        {DataTypes::UINT32, DataTypes::INT32},   // FLOAT32
        {DataTypes::INT64, DataTypes::FLOAT64},  // UINT64
        {DataTypes::UINT64, DataTypes::FLOAT64}, // INT64
        {DataTypes::UINT64, DataTypes::INT64},   // FLOAT64
    };

    return sameSizeDataTable[DE_ENUM_INDEX(type)];
}

static std::vector<DATA_TYPE> getSameSizeBaseDataType(COMPOSITE_DATA_TYPE type)
{
    static const std::vector<DATA_TYPE> sameSizeDataTable[DE_ENUM_COUNT(CompositeDataTypes)] = {
        {DataTypes::UINT16, DataTypes::INT16, DataTypes::FLOAT16}, // VEC2_UINT8
        {},                                                        // VEC3_UINT8
        {DataTypes::UINT32, DataTypes::INT32, DataTypes::FLOAT32}, // VEC4_UINT8
        {DataTypes::UINT16, DataTypes::INT16, DataTypes::FLOAT16}, // VEC2_INT8
        {},                                                        // VEC3_INT8
        {DataTypes::UINT32, DataTypes::INT32, DataTypes::FLOAT32}, // VEC4_INT8
        {DataTypes::UINT32, DataTypes::INT32, DataTypes::FLOAT32}, // VEC2_UINT16
        {},                                                        // VEC3_UINT16
        {DataTypes::UINT64, DataTypes::INT64, DataTypes::FLOAT64}, // VEC4_UINT16
        {DataTypes::UINT32, DataTypes::INT32, DataTypes::FLOAT32}, // VEC2_INT16
        {},                                                        // VEC3_INT16
        {DataTypes::UINT64, DataTypes::INT64, DataTypes::FLOAT64}, // VEC4_INT16
        {DataTypes::UINT32, DataTypes::INT32, DataTypes::FLOAT32}, // VEC2_FLOAT16
        {},                                                        // VEC3_FLOAT16
        {DataTypes::UINT64, DataTypes::INT64, DataTypes::FLOAT64}, // VEC4_FLOAT16
        {DataTypes::UINT64, DataTypes::INT64, DataTypes::FLOAT64}, // VEC2_UINT32
        {},                                                        // VEC3_UINT32
        {},                                                        // VEC4_UINT32
        {DataTypes::UINT64, DataTypes::INT64, DataTypes::FLOAT64}, // VEC2_INT32
        {},                                                        // VEC3_INT32
        {},                                                        // VEC4_INT32
        {DataTypes::UINT64, DataTypes::INT64, DataTypes::FLOAT64}, // VEC2_FLOAT32
        {},                                                        // VEC3_FLOAT32
        {},                                                        // VEC4_FLOAT32
        {},                                                        // VEC2_UINT64
        {},                                                        // VEC3_UINT64
        {},                                                        // VEC4_UINT64
        {},                                                        // VEC2_INT64
        {},                                                        // VEC3_INT64
        {},                                                        // VEC4_INT64
        {},                                                        // VEC2_FLOAT64
        {},                                                        // VEC3_FLOAT64
        {},                                                        // VEC4_FLOAT64
    };

    return sameSizeDataTable[DE_ENUM_INDEX(type)];
}

static std::vector<COMPOSITE_DATA_TYPE> getSameSizeCompositeType(DATA_TYPE type)
{
    static std::vector<COMPOSITE_DATA_TYPE> sameSizeDataTable[DE_ENUM_COUNT(DataTypes)] = {
        {},                                                              // UINT8
        {},                                                              // INT8
        {CompositeDataTypes::VEC2_UINT8, CompositeDataTypes::VEC2_INT8}, // UINT16
        {CompositeDataTypes::VEC2_UINT8, CompositeDataTypes::VEC2_INT8}, // INT16
        {CompositeDataTypes::VEC2_UINT8, CompositeDataTypes::VEC2_INT8}, // FLOAT16
        {CompositeDataTypes::VEC4_UINT8, CompositeDataTypes::VEC4_INT8, CompositeDataTypes::VEC2_UINT16,
         CompositeDataTypes::VEC2_INT16, CompositeDataTypes::VEC2_FLOAT16}, // UINT32
        {CompositeDataTypes::VEC4_UINT8, CompositeDataTypes::VEC4_INT8, CompositeDataTypes::VEC2_UINT16,
         CompositeDataTypes::VEC2_INT16, CompositeDataTypes::VEC2_FLOAT16}, // INT32
        {CompositeDataTypes::VEC4_UINT8, CompositeDataTypes::VEC4_INT8, CompositeDataTypes::VEC2_UINT16,
         CompositeDataTypes::VEC2_INT16, CompositeDataTypes::VEC2_FLOAT16}, // FLOAT32
        {CompositeDataTypes::VEC4_UINT16, CompositeDataTypes::VEC4_INT16, CompositeDataTypes::VEC4_FLOAT16,
         CompositeDataTypes::VEC2_UINT32, CompositeDataTypes::VEC2_INT32, CompositeDataTypes::VEC2_FLOAT32}, // UINT64
        {CompositeDataTypes::VEC4_UINT16, CompositeDataTypes::VEC4_INT16, CompositeDataTypes::VEC4_FLOAT16,
         CompositeDataTypes::VEC2_UINT32, CompositeDataTypes::VEC2_INT32, CompositeDataTypes::VEC2_FLOAT32}, // INT64
        {CompositeDataTypes::VEC4_UINT16, CompositeDataTypes::VEC4_INT16, CompositeDataTypes::VEC4_FLOAT16,
         CompositeDataTypes::VEC2_UINT32, CompositeDataTypes::VEC2_INT32, CompositeDataTypes::VEC2_FLOAT32}, // FLOAT64
    };

    return sameSizeDataTable[DE_ENUM_INDEX(type)];
}

bool isReadOnly(CONTAINER_TYPE type)
{
    static const bool translateTable[DE_ENUM_COUNT(ContainerTypes)] = {
        false, // STORAGE_BUFFER,
        true,  // UNIFORM,
        true,  // PUSH_CONSTANT,
        false, // WORKGROUP,
    };

    return translateTable[DE_ENUM_INDEX(type)];
}

const char *toString(DATA_TYPE type)
{
    static const char *const translateTable[DE_ENUM_COUNT(DataTypes)] = {
        "uint8",   // UINT8
        "int8",    // INT8
        "uint16",  // UINT16
        "int16",   // INT16
        "float16", // FLOAT16
        "uint32",  // UINT32
        "int32",   // INT32
        "float32", // FLOAT32
        "uint64",  // UINT64
        "int64",   // INT64
        "float64", // FLOAT64
    };

    return translateTable[DE_ENUM_INDEX(type)];
}

const char *toString(COMPOSITE_DATA_TYPE type)
{
    static const char *const translateTable[DE_ENUM_COUNT(CompositeDataTypes)] = {
        "vec2_uint8",   // VEC2_UINT8
        "vec3_uint8",   // VEC3_UINT8
        "vec4_uint8",   // VEC4_UINT8
        "vec2_int8",    // VEC2_INT8
        "vec3_int8",    // VEC3_INT8
        "vec4_int8",    // VEC4_INT8
        "vec2_uint16",  // VEC2_UINT16
        "vec3_uint16",  // VEC3_UINT16
        "vec4_uint16",  // VEC4_UINT16
        "vec2_int16",   // VEC2_INT16
        "vec3_int16",   // VEC3_INT16
        "vec4_int16",   // VEC4_INT16
        "vec2_float16", // VEC2_FLOAT16
        "vec3_float16", // VEC3_FLOAT16
        "vec4_float16", // VEC4_FLOAT16
        "vec2_uint32",  // VEC2_UINT32
        "vec3_uint32",  // VEC3_UINT32
        "vec4_uint32",  // VEC4_UINT32
        "vec2_int32",   // VEC2_INT32
        "vec3_int32",   // VEC3_INT32
        "vec4_int32",   // VEC4_INT32
        "vec2_float32", // VEC2_FLOAT32
        "vec3_float32", // VEC3_FLOAT32
        "vec4_float32", // VEC4_FLOAT32
        "vec2_uint64",  // VEC2_UINT64
        "vec3_uint64",  // VEC3_UINT64
        "vec4_uint64",  // VEC4_UINT64
        "vec2_int64",   // VEC2_INT64
        "vec3_int64",   // VEC3_INT64
        "vec4_int64",   // VEC4_INT64
        "vec2_float64", // VEC2_FLOAT64
        "vec3_float64", // VEC3_FLOAT64
        "vec4_float64", // VEC4_FLOAT64
    };

    return translateTable[DE_ENUM_INDEX(type)];
}

const char *toString(ATOMIC_TEST_CASE testCase)
{
    static const char *const translateTable[DE_ENUM_COUNT(AtomicTestCases)] = {
        "op_atomic_load",             // OP_ATOMIC_LOAD
        "op_atomic_store",            // OP_ATOMIC_STORE
        "op_atomic_exchange",         // OP_ATOMIC_EXCHANGE
        "op_atomic_compare_exchange", // OP_ATOMIC_COMPARE_EXCHANGE
        "op_atomic_increment",        // OP_ATOMIC_INCREMENT
        "op_atomic_decrement",        // OP_ATOMIC_DECREMENT
        "op_atomic_add",              // OP_ATOMIC_ADD
        "op_atomic_sub",              // OP_ATOMIC_SUB
        "op_atomic_min",              // OP_ATOMIC_MIN
        "op_atomic_max",              // OP_ATOMIC_MAX
        "op_atomic_and",              // OP_ATOMIC_AND
        "op_atomic_or",               // OP_ATOMIC_OR
        "op_atomic_xor",              // OP_ATOMIC_XOR
    };

    return translateTable[DE_ENUM_INDEX(testCase)];
}

const char *toString(POINTER_TEST_CASE testCase)
{
    static const char *const translateTable[DE_ENUM_COUNT(PointerTestCases)] = {
        "op_bitcast_form_untyped", // OP_BITCAST_FROM_UNTYPED_PHYSICAL_STORAGE
        "op_bitcast_to_untyped",   // OP_BITCAST_TO_UNTYPED_PHYSICAL_STORAGE
        "op_select",               // OP_SELECT_PHYSICAL_STORAGE
        "op_phi",                  // OP_PHI_PHYSICAL_STORAGE
        "op_ptr_access_chain",     // OP_PTR_ACCESS_CHAIN_PHYSICAL_STORAGE
        "op_function_call",        // FUNCTION_PARAMETERS_PHYSICAL_STORAGE
        "op_select",               // OP_SELECT_VARIABLE_PTR
        "op_phi",                  // OP_PHI_VARIABLE_PTR
        "op_ptr_access_chain",     // OP_PTR_ACCESS_CHAIN_VARIABLE_PTR
        "op_ptr_equal",            // OP_PTR_EQUAL_VARIABLE_PTR
        "op_ptr_not_equal",        // OP_PTR_NOT_EQUAL_VARIABLE_PTR
        "op_ptr_diff",             // OP_PTR_DIFF_VARIABLE_PTR
        "op_function_call",        // OP_FUNCTION_CALL_VARIABLE_PTR
        "function_variable",       // FUNCTION_VARIABLE_VARIABLE_PTR
        "private_variable",        // PRIVATE_VARIABLE_VARIABLE_PTR
        "multiple_access_chains",  // MULTIPLE_ACCESS_CHAINS_VARIABLE_PTR
        "workgroup_memory",        // WORKGROUP_MEMORY_VARIABLE_PTR
    };

    return translateTable[DE_ENUM_INDEX(testCase)];
}

const char *toString(MEMORY_INTERPRETATION_TEST_CASE testCase)
{
    static const char *const translateTable[DE_ENUM_COUNT(MemoryInterpretationTestCases)] = {
        "large_array_stride",              // LARGE_ARRAY_STRIDE
        "non_zero_offset",                 // NON_ZERO_OFFSET
        "mixed_offsets",                   // MIXED_OFFSETS
        "multiple_access_chains",          // MULTIPLE_ACCESS_CHAINS
        "short2_no_storage_cap",           // SHORT2_NO_STORAGE_CAP
        "char4_no_storage_cap",            // CHAR4_NO_STORAGE_CAP
        "char2_16bit_storage_cap",         // CHAR2_16BIT_STORAGE_CAP
        "untyped_from_typed_var",          // UNTYPED_FROM_TYPED_VAR
        "untyped_from_typed_access_chain", // UNTYPED_FROM_TYPED_ACCESS_CHAIN
    };

    return translateTable[DE_ENUM_INDEX(testCase)];
}

const char *toString(BLOCK_ARRAY_TEST_CASE testCase)
{
    static const char *const translateTable[DE_ENUM_COUNT(BlockArrayTestCases)] = {
        "basic",                                      // BASIC
        "reinterpret_block_normal_access_chain",      // REINTERPRET_BLOCK_NORMAL
        "reinterpret_block_normal_ptr_access_chain",  // REINTERPRET_BLOCK_NORMAL
        "reinterpret_block_untyped_access_chain",     // REINTERPRET_BLOCK_UNTYPED
        "reinterpret_block_untyped_ptr_access_chain", // REINTERPRET_BLOCK_UNTYPED
        "select_block_normal_access_chain",           // SELECT_BLOCK_NORMAL
        "select_block_normal_ptr_access_chain",       // SELECT_BLOCK_NORMAL
        "select_block_untyped_access_chain",          // SELECT_BLOCK_UNTYPED
        "select_block_untyped_ptr_access_chain",      // SELECT_BLOCK_UNTYPED
    };

    return translateTable[DE_ENUM_INDEX(testCase)];
}

const char *toString(WORKGROUP_TEST_CASE testCase)
{
    static const char *const translateTable[DE_ENUM_COUNT(WorkgroupTestCases)] = {
        "aliased",     // ALIASED
        "not_aliased", // NOT_ALIASED
    };

    return translateTable[DE_ENUM_INDEX(testCase)];
}

const char *toString(OPERATION_TYPE opType)
{
    static const char *const translateTable[DE_ENUM_COUNT(OperationTypes)] = {
        "normal", // NORMAL
        "atomic", // ATOMIC
    };

    return translateTable[DE_ENUM_INDEX(opType)];
}

const char *toString(CONTAINER_TYPE contType)
{
    static const char *const translateTable[DE_ENUM_COUNT(ContainerTypes)] = {
        "storage_buffer", // STORAGE_BUFFER
        "uniform",        // UNIFORM
        "push_constant",  // PUSH_CONSTANT
        "workgroup",      // WORKGROUP
    };

    return translateTable[DE_ENUM_INDEX(contType)];
}

const char *toString(COPY_OPERATION_TYPE type)
{
    static const char *const translateTable[DE_ENUM_COUNT(CopyOperationTypes)] = {
        "copy_object",       // COPY_OBJECT
        "copy_memory",       // COPY_MEMORY
        "copy_memory_sized", // COPY_MEMORY_SIZED
    };

    return translateTable[DE_ENUM_INDEX(type)];
}

const char *toString(MATRIX_TYPE type)
{
    static const char *const translateTable[DE_ENUM_COUNT(MatrixTypes)] = {
        "a",           // A
        "b",           // B
        "accumulator", // ACCUMULATOR
    };

    return translateTable[DE_ENUM_INDEX(type)];
}

const char *toString(MATRIX_LAYOUT layout)
{
    static const char *const translateTable[DE_ENUM_COUNT(MatrixLayouts)] = {
        "row_major", // ROW_MAJOR
        "col_major", // COL_MAJOR
    };

    return translateTable[DE_ENUM_INDEX(layout)];
}

const char *getCapability(DATA_TYPE type)
{
    static const char *const translateTable[DE_ENUM_COUNT(DataTypes)] = {
        "OpCapability Int8",    // UINT8
        "OpCapability Int8",    // INT8
        "OpCapability Int16",   // UINT16
        "OpCapability Int16",   // INT16
        "OpCapability Float16", // FLOAT16
        "",                     // UINT32
        "",                     // INT32
        "",                     // FLOAT32
        "OpCapability Int64",   // UINT64
        "OpCapability Int64",   // INT64
        "OpCapability Float64", // FLOAT64
    };

    return translateTable[DE_ENUM_INDEX(type)];
}

const char *getCapability(COMPOSITE_DATA_TYPE type)
{
    static const char *const translateTable[DE_ENUM_COUNT(CompositeDataTypes)] = {
        "OpCapability Int8",    // VEC2_UINT8
        "OpCapability Int8",    // VEC3_UINT8
        "OpCapability Int8",    // VEC4_UINT8
        "OpCapability Int8",    // VEC2_INT8
        "OpCapability Int8",    // VEC3_INT8
        "OpCapability Int8",    // VEC4_INT8
        "OpCapability Int16",   // VEC2_UINT16
        "OpCapability Int16",   // VEC3_UINT16
        "OpCapability Int16",   // VEC4_UINT16
        "OpCapability Int16",   // VEC2_INT16
        "OpCapability Int16",   // VEC3_INT16
        "OpCapability Int16",   // VEC4_INT16
        "OpCapability Float16", // VEC2_FLOAT16
        "OpCapability Float16", // VEC3_FLOAT16
        "OpCapability Float16", // VEC4_FLOAT16
        "",                     // VEC2_UINT32
        "",                     // VEC3_UINT32
        "",                     // VEC4_UINT32
        "",                     // VEC2_INT32
        "",                     // VEC3_INT32
        "",                     // VEC4_INT32
        "",                     // VEC2_FLOAT32
        "",                     // VEC3_FLOAT32
        "",                     // VEC4_FLOAT32
        "OpCapability Int64",   // VEC2_UINT64
        "OpCapability Int64",   // VEC3_UINT64
        "OpCapability Int64",   // VEC4_UINT64
        "OpCapability Int64",   // VEC2_INT64
        "OpCapability Int64",   // VEC3_INT64
        "OpCapability Int64",   // VEC4_INT64
        "OpCapability Float64", // VEC2_FLOAT64
        "OpCapability Float64", // VEC3_FLOAT64
        "OpCapability Float64", // VEC4_FLOAT64
    };

    return translateTable[DE_ENUM_INDEX(type)];
}

const char *getDeclaration(DATA_TYPE type)
{
    static const char *const translateTable[DE_ENUM_COUNT(DataTypes)] = {
        "OpTypeInt    8 0", // UINT8
        "OpTypeInt    8 1", // INT8
        "OpTypeInt   16 0", // UINT16
        "OpTypeInt   16 1", // INT16
        "OpTypeFloat 16",   // FLOAT16
        "OpTypeInt   32 0", // UINT32
        "OpTypeInt   32 1", // INT32
        "OpTypeFloat 32",   // FLOAT32
        "OpTypeInt   64 0", // UINT64
        "OpTypeInt   64 1", // INT64
        "OpTypeFloat 64",   // FLOAT64
    };

    return translateTable[DE_ENUM_INDEX(type)];
}

const char *getDeclaration(COMPOSITE_DATA_TYPE type)
{
    static const char *const translateTable[DE_ENUM_COUNT(CompositeDataTypes)] = {
        "OpTypeVector %uint8   2", // VEC2_UINT8
        "OpTypeVector %uint8   3", // VEC3_UINT8
        "OpTypeVector %uint8   4", // VEC4_UINT8
        "OpTypeVector %int8    2", // VEC2_INT8
        "OpTypeVector %int8    3", // VEC3_INT8
        "OpTypeVector %int8    4", // VEC4_INT8
        "OpTypeVector %uint16  2", // VEC2_UINT16
        "OpTypeVector %uint16  3", // VEC3_UINT16
        "OpTypeVector %uint16  4", // VEC4_UINT16
        "OpTypeVector %int16   2", // VEC2_INT16
        "OpTypeVector %int16   3", // VEC3_INT16
        "OpTypeVector %int16   4", // VEC4_INT16
        "OpTypeVector %float16 2", // VEC2_FLOAT16
        "OpTypeVector %float16 3", // VEC3_FLOAT16
        "OpTypeVector %float16 4", // VEC4_FLOAT16
        "OpTypeVector %uint32  2", // VEC2_UINT32
        "OpTypeVector %uint32  3", // VEC3_UINT32
        "OpTypeVector %uint32  4", // VEC4_UINT32
        "OpTypeVector %int32   2", // VEC2_INT32
        "OpTypeVector %int32   3", // VEC3_INT32
        "OpTypeVector %int32   4", // VEC4_INT32
        "OpTypeVector %float32 2", // VEC2_FLOAT32
        "OpTypeVector %float32 3", // VEC3_FLOAT32
        "OpTypeVector %float32 4", // VEC4_FLOAT32
        "OpTypeVector %uint64  2", // VEC2_UINT64
        "OpTypeVector %uint64  3", // VEC3_UINT64
        "OpTypeVector %uint64  4", // VEC4_UINT64
        "OpTypeVector %int64   2", // VEC2_INT64
        "OpTypeVector %int64   3", // VEC3_INT64
        "OpTypeVector %int64   4", // VEC4_INT64
        "OpTypeVector %float64 2", // VEC2_FLOAT64
        "OpTypeVector %float64 3", // VEC3_FLOAT64
        "OpTypeVector %float64 4", // VEC4_FLOAT64
    };

    return translateTable[DE_ENUM_INDEX(type)];
}

const char *getStorageClass(CONTAINER_TYPE type)
{
    static const char *const translateTable[DE_ENUM_COUNT(ContainerTypes)] = {
        "StorageBuffer", // STORAGE_BUFFER
        "Uniform",       // UNIFORM
        "PushConstant",  // PUSH_CONSTANT
        "Workgroup",     // WORKGROUP
    };

    return translateTable[DE_ENUM_INDEX(type)];
}

const char *getAtomicAddOperator(DATA_TYPE type)
{
    static const char *const translateTable[DE_ENUM_COUNT(DataTypes)] = {
        "OpAtomicIAdd",    // UINT8
        "OpAtomicIAdd",    // INT8
        "OpAtomicIAdd",    // UINT16
        "OpAtomicIAdd",    // INT16
        "OpAtomicFAddEXT", // FLOAT16
        "OpAtomicIAdd",    // UINT32
        "OpAtomicIAdd",    // INT32
        "OpAtomicFAddEXT", // FLOAT32
        "OpAtomicIAdd",    // UINT64
        "OpAtomicIAdd",    // INT64
        "OpAtomicFAddEXT", // FLOAT64
    };

    return translateTable[DE_ENUM_INDEX(type)];
}

const char *getAtomicSubtractOperator(DATA_TYPE type)
{
    static const char *const translateTable[DE_ENUM_COUNT(DataTypes)] = {
        "OpAtomicISub", // UINT8
        "OpAtomicISub", // INT8
        "OpAtomicISub", // UINT16
        "OpAtomicISub", // INT16
        "",             // FLOAT16
        "OpAtomicISub", // UINT32
        "OpAtomicISub", // INT32
        "",             // FLOAT32
        "OpAtomicISub", // UINT64
        "OpAtomicISub", // INT64
        "",             // FLOAT64
    };

    return translateTable[DE_ENUM_INDEX(type)];
}

const char *getAtomicIncrementOperator(DATA_TYPE type)
{
    static const char *const translateTable[DE_ENUM_COUNT(DataTypes)] = {
        "OpAtomicIIncrement", // UINT8
        "OpAtomicIIncrement", // INT8
        "OpAtomicIIncrement", // UINT16
        "OpAtomicIIncrement", // INT16
        "",                   // FLOAT16
        "OpAtomicIIncrement", // UINT32
        "OpAtomicIIncrement", // INT32
        "",                   // FLOAT32
        "OpAtomicIIncrement", // UINT64
        "OpAtomicIIncrement", // INT64
        "",                   // FLOAT64
    };

    return translateTable[DE_ENUM_INDEX(type)];
}

const char *getAtomicDecrementOperator(DATA_TYPE type)
{
    static const char *const translateTable[DE_ENUM_COUNT(DataTypes)] = {
        "OpAtomicIDecrement", // UINT8
        "OpAtomicIDecrement", // INT8
        "OpAtomicIDecrement", // UINT16
        "OpAtomicIDecrement", // INT16
        "",                   // FLOAT16
        "OpAtomicIDecrement", // UINT32
        "OpAtomicIDecrement", // INT32
        "",                   // FLOAT32
        "OpAtomicIDecrement", // UINT64
        "OpAtomicIDecrement", // INT64
        "",                   // FLOAT64
    };

    return translateTable[DE_ENUM_INDEX(type)];
}

const char *getAtomicMinOperator(DATA_TYPE type)
{
    static const char *const translateTable[DE_ENUM_COUNT(DataTypes)] = {
        "OpAtomicUMin",    // UINT8
        "OpAtomicSMin",    // INT8
        "OpAtomicUMin",    // UINT16
        "OpAtomicSMin",    // INT16
        "OpAtomicFMinEXT", // FLOAT16
        "OpAtomicUMin",    // UINT32
        "OpAtomicSMin",    // INT32
        "OpAtomicFMinEXT", // FLOAT32
        "OpAtomicUMin",    // UINT64
        "OpAtomicSMin",    // INT64
        "OpAtomicFMinEXT", // FLOAT64
    };

    return translateTable[DE_ENUM_INDEX(type)];
}

const char *getAtomicMaxOperator(DATA_TYPE type)
{
    static const char *const translateTable[DE_ENUM_COUNT(DataTypes)] = {
        "OpAtomicUMax",    // UINT8
        "OpAtomicSMax",    // INT8
        "OpAtomicUMax",    // UINT16
        "OpAtomicSMax",    // INT16
        "OpAtomicFMaxEXT", // FLOAT16
        "OpAtomicUMax",    // UINT32
        "OpAtomicSMax",    // INT32
        "OpAtomicFMaxEXT", // FLOAT32
        "OpAtomicUMax",    // UINT64
        "OpAtomicSMax",    // INT64
        "OpAtomicFMaxEXT", // FLOAT64
    };

    return translateTable[DE_ENUM_INDEX(type)];
}

const char *getAtomicAndOperator(DATA_TYPE type)
{
    static const char *const translateTable[DE_ENUM_COUNT(DataTypes)] = {
        "OpAtomicAnd", // UINT8
        "OpAtomicAnd", // INT8
        "OpAtomicAnd", // UINT16
        "OpAtomicAnd", // INT16
        "",            // FLOAT16
        "OpAtomicAnd", // UINT32
        "OpAtomicAnd", // INT32
        "",            // FLOAT32
        "OpAtomicAnd", // UINT64
        "OpAtomicAnd", // INT64
        "",            // FLOAT64
    };

    return translateTable[DE_ENUM_INDEX(type)];
}

const char *getAtomicOrOperator(DATA_TYPE type)
{
    static const char *const translateTable[DE_ENUM_COUNT(DataTypes)] = {
        "OpAtomicOr", // UINT8
        "OpAtomicOr", // INT8
        "OpAtomicOr", // UINT16
        "OpAtomicOr", // INT16
        "",           // FLOAT16
        "OpAtomicOr", // UINT32
        "OpAtomicOr", // INT32
        "",           // FLOAT32
        "OpAtomicOr", // UINT64
        "OpAtomicOr", // INT64
        "",           // FLOAT64
    };

    return translateTable[DE_ENUM_INDEX(type)];
}

const char *getAtomicXorOperator(DATA_TYPE type)
{
    static const char *const translateTable[DE_ENUM_COUNT(DataTypes)] = {
        "OpAtomicXor", // UINT8
        "OpAtomicXor", // INT8
        "OpAtomicXor", // UINT16
        "OpAtomicXor", // INT16
        "",            // FLOAT16
        "OpAtomicXor", // UINT32
        "OpAtomicXor", // INT32
        "",            // FLOAT32
        "OpAtomicXor", // UINT64
        "OpAtomicXor", // INT64
        "",            // FLOAT64
    };

    return translateTable[DE_ENUM_INDEX(type)];
}

const char *getAtomicExchangeOperator(DATA_TYPE type)
{
    static const char *const translateTable[DE_ENUM_COUNT(DataTypes)] = {
        "OpAtomicExchange", // UINT8
        "OpAtomicExchange", // INT8
        "OpAtomicExchange", // UINT16
        "OpAtomicExchange", // INT16
        "OpAtomicExchange", // FLOAT16
        "OpAtomicExchange", // UINT32
        "OpAtomicExchange", // INT32
        "OpAtomicExchange", // FLOAT32
        "OpAtomicExchange", // UINT64
        "OpAtomicExchange", // INT64
        "OpAtomicExchange", // FLOAT64
    };

    return translateTable[DE_ENUM_INDEX(type)];
}

const char *getAtomicCompareExchangeOperator(DATA_TYPE type)
{
    static const char *const translateTable[DE_ENUM_COUNT(DataTypes)] = {
        "OpAtomicCompareExchange", // UINT8
        "OpAtomicCompareExchange", // INT8
        "OpAtomicCompareExchange", // UINT16
        "OpAtomicCompareExchange", // INT16
        "",                        // FLOAT16
        "OpAtomicCompareExchange", // UINT32
        "OpAtomicCompareExchange", // INT32
        "",                        // FLOAT32
        "OpAtomicCompareExchange", // UINT64
        "OpAtomicCompareExchange", // INT64
        "",                        // FLOAT64
    };

    return translateTable[DE_ENUM_INDEX(type)];
}

static int32_t getSignedUnsignedMinMaxTestValue(DATA_TYPE type)
{
    static const int32_t translateTable[DE_ENUM_COUNT(DataTypes)] = {
        1,  // UINT8
        -1, // INT8
        1,  // UINT16
        -1, // INT16
        1,  // FLOAT16
        1,  // UINT32
        -1, // INT32
        -1, // FLOAT32
        1,  // UINT64
        -1, // INT64
        1,  // FLOAT64
    };

    return translateTable[DE_ENUM_INDEX(type)];
}

std::string getResourceDecorations(CONTAINER_TYPE containerType, DATA_TYPE dataType, uint32_t numWorkgroup)
{
    std::string decorations = "OpDecorate %array_";
    decorations += toString(dataType);
    decorations += "_";
    decorations += std::to_string(numWorkgroup);
    decorations += " ArrayStride ";
    decorations +=
        containerType == ContainerTypes::UNIFORM ? std::to_string(16) : std::to_string(getSizeInBytes(dataType));
    decorations += "\n";

    if (containerType == ContainerTypes::PUSH_CONSTANT)
    {
        decorations += std::string("OpDecorate %output_data_var DescriptorSet 0\n"
                                   "OpDecorate %output_data_var Binding       0\n");
    }
    else
    {
        decorations += std::string("OpDecorate %input_data_untyped_var DescriptorSet 0\n"
                                   "OpDecorate %input_data_untyped_var Binding       0\n"

                                   "OpDecorate %output_data_var        DescriptorSet 0\n"
                                   "OpDecorate %output_data_var        Binding       1\n");
    }

    return decorations;
}

std::string getSameSizeResourceDecorations(CONTAINER_TYPE containerType, DATA_TYPE dataType1, DATA_TYPE dataType2,
                                           uint32_t numWorkgroup)
{
    std::string decorations = "OpDecorate %array_";
    decorations += toString(dataType1);
    decorations += "_";
    decorations += std::to_string(numWorkgroup);
    decorations += " ArrayStride ";
    decorations +=
        containerType == ContainerTypes::UNIFORM ? std::to_string(16) : std::to_string(getSizeInBytes(dataType1));
    decorations += "\n";

    decorations += "OpDecorate %array_";
    decorations += toString(dataType2);
    decorations += "_";
    decorations += std::to_string(numWorkgroup);
    decorations += " ArrayStride ";
    decorations +=
        containerType == ContainerTypes::UNIFORM ? std::to_string(16) : std::to_string(getSizeInBytes(dataType1));
    decorations += "\n";

    if (containerType == ContainerTypes::PUSH_CONSTANT)
    {
        decorations += std::string("OpDecorate %output_data_var DescriptorSet 0\n"
                                   "OpDecorate %output_data_var Binding       0\n");
    }
    else
    {
        decorations += std::string("OpDecorate %input_data_untyped_var DescriptorSet 0\n"
                                   "OpDecorate %input_data_untyped_var Binding       0\n"

                                   "OpDecorate %output_data_var        DescriptorSet 0\n"
                                   "OpDecorate %output_data_var        Binding       1\n");
    }

    return decorations;
}

std::string getScalarVectorResourceDecorations(CONTAINER_TYPE containerType)
{
    std::string decorations = "";

    if (containerType == ContainerTypes::PUSH_CONSTANT)
    {
        decorations += std::string("OpDecorate %output_data_var DescriptorSet 0\n"
                                   "OpDecorate %output_data_var Binding       0\n");
    }
    else
    {
        decorations += std::string("OpDecorate %input_data_untyped_var DescriptorSet 0\n"
                                   "OpDecorate %input_data_untyped_var Binding       0\n"

                                   "OpDecorate %output_data_var        DescriptorSet 0\n"
                                   "OpDecorate %output_data_var        Binding       1\n");
    }

    return decorations;
}

const char *getNameStrForVarPtrs(DATA_TYPE type)
{
    static const char *const translateTable[DE_ENUM_COUNT(DataTypes)] = {
        "_to_int16", // UINT8
        "_to_int16", // INT8
        "_to_int32", // UINT16
        "_to_int32", // INT16
        "_to_int32", // FLOAT16
        "_to_int16", // UINT32
        "_to_int16", // INT32
        "_to_int16", // FLOAT32
        "_to_int32", // UINT64
        "_to_int32", // INT64
        "_to_int32", // FLOAT64
    };

    return translateTable[DE_ENUM_INDEX(type)];
}

const char *getSecondTypeDefinitionForVarPtrs(DATA_TYPE type)
{
    static const char *const translateTable[DE_ENUM_COUNT(DataTypes)] = {
        "%int16 = OpTypeInt 16 1", // UINT8
        "%int16 = OpTypeInt 16 1", // INT8
        "%int32 = OpTypeInt 32 1", // UINT16
        "%int32 = OpTypeInt 32 1", // INT16
        "%int32 = OpTypeInt 32 1", // FLOAT16
        "%int16 = OpTypeInt 16 1", // UINT32
        "%int16 = OpTypeInt 16 1", // INT32
        "%int16 = OpTypeInt 16 1", // FLOAT32
        "%int32 = OpTypeInt 32 1", // UINT64
        "%int32 = OpTypeInt 32 1", // INT64
        "%int32 = OpTypeInt 32 1", // FLOAT64
    };

    return translateTable[DE_ENUM_INDEX(type)];
}

static DATA_TYPE getSecondTypeForVarPtrs(DATA_TYPE type)
{
    static const DATA_TYPE translateTable[DE_ENUM_COUNT(DataTypes)] = {
        DataTypes::INT16, // UINT8
        DataTypes::INT16, // INT8
        DataTypes::INT32, // UINT16
        DataTypes::INT32, // INT16
        DataTypes::INT32, // FLOAT16
        DataTypes::INT16, // UINT32
        DataTypes::INT16, // INT32
        DataTypes::INT16, // FLOAT32
        DataTypes::INT32, // UINT64
        DataTypes::INT32, // INT64
        DataTypes::INT32, // FLOAT64
    };

    return translateTable[DE_ENUM_INDEX(type)];
}

const char *getSecondArrayDefinitionForVarPtrs(DATA_TYPE type)
{
    static const char *const translateTable[DE_ENUM_COUNT(DataTypes)] = {
        "%array_second_32 = OpTypeArray %int16 %c_uint32_32", // UINT8
        "%array_second_32 = OpTypeArray %int16 %c_uint32_32", // INT8
        "%array_second_32 = OpTypeArray %int32 %c_uint32_32", // UINT16
        "%array_second_32 = OpTypeArray %int32 %c_uint32_32", // INT16
        "%array_second_32 = OpTypeArray %int32 %c_uint32_32", // FLOAT16
        "%array_second_32 = OpTypeArray %int16 %c_uint32_32", // UINT32
        "%array_second_32 = OpTypeArray %int16 %c_uint32_32", // INT32
        "%array_second_32 = OpTypeArray %int16 %c_uint32_32", // FLOAT32
        "%array_second_32 = OpTypeArray %int32 %c_uint32_32", // UINT64
        "%array_second_32 = OpTypeArray %int32 %c_uint32_32", // INT64
        "%array_second_32 = OpTypeArray %int32 %c_uint32_32", // FLOAT64
    };

    return translateTable[DE_ENUM_INDEX(type)];
}

const char *getSecondArrayDecorationForVarPtrs(DATA_TYPE type)
{
    static const char *const translateTable[DE_ENUM_COUNT(DataTypes)] = {
        "OpDecorate       %array_second_32   ArrayStride   2\n", // UINT8
        "OpDecorate       %array_second_32   ArrayStride   2\n", // INT8
        "OpDecorate       %array_second_32   ArrayStride   4\n", // UINT16
        "OpDecorate       %array_second_32   ArrayStride   4\n", // INT16
        "OpDecorate       %array_second_32   ArrayStride   4\n", // FLOAT16
        "OpDecorate       %array_second_32   ArrayStride   2\n", // UINT32
        "OpDecorate       %array_second_32   ArrayStride   2\n", // INT32
        "OpDecorate       %array_second_32   ArrayStride   2\n", // FLOAT32
        "OpDecorate       %array_second_32   ArrayStride   4\n", // UINT64
        "OpDecorate       %array_second_32   ArrayStride   4\n", // INT64
        "OpDecorate       %array_second_32   ArrayStride   4\n", // FLOAT64
    };

    return translateTable[DE_ENUM_INDEX(type)];
}

static uint32_t getSecondAlignmentForVarPtrs(DATA_TYPE type)
{
    static const uint32_t translateTable[DE_ENUM_COUNT(DataTypes)] = {
        2, // UINT8
        2, // INT8
        4, // UINT16
        4, // INT16
        4, // FLOAT16
        2, // UINT32
        2, // INT32
        2, // FLOAT32
        4, // UINT64
        4, // INT64
        4, // FLOAT64
    };

    return translateTable[DE_ENUM_INDEX(type)];
}

const char *getSameByteIndexForVarPtrs(DATA_TYPE type)
{
    static const char *const translateTable[DE_ENUM_COUNT(DataTypes)] = {
        "%c_uint32_2\n", // UINT8
        "%c_uint32_2\n", // INT8
        "%c_uint32_2\n", // UINT16
        "%c_uint32_2\n", // INT16
        "%c_uint32_2\n", // FLOAT16
        "%c_uint32_8\n", // UINT32
        "%c_uint32_8\n", // INT32
        "%c_uint32_8\n", // FLOAT32
        "%c_uint32_8\n", // UINT64
        "%c_uint32_8\n", // INT64
        "%c_uint32_8\n", // FLOAT64
    };

    return translateTable[DE_ENUM_INDEX(type)];
}

static uint32_t getMatrixBinaryUse(MATRIX_TYPE type)
{
    return DE_ENUM_INDEX(type);
}

static uint32_t getMatrixBinaryLayout(MATRIX_LAYOUT layout)
{
    return DE_ENUM_INDEX(layout);
}

static void adjustSpecForMemoryModel(MEMORY_MODEL_TYPE memModel, ComputeShaderSpec &spec, std::string &memModelOp,
                                     std::vector<const char *> &spvExtensions,
                                     std::vector<const char *> &spvCapabilities)
{
    switch (memModel)
    {
    case MemoryModelTypes::VULKAN:
    {
        spvCapabilities.push_back("OpCapability VulkanMemoryModel\n");
        spvCapabilities.push_back("OpCapability VulkanMemoryModelDeviceScopeKHR\n");
        spvExtensions.push_back("OpExtension \"SPV_KHR_vulkan_memory_model\"\n");
        memModelOp = "OpMemoryModel Logical Vulkan";

        spec.extensions.push_back("VK_KHR_vulkan_memory_model");
        spec.spirvVersion = SPIRV_VERSION_1_3; // SPIR-V 1.3 or higher is required for VulkanMemoryModel

        break;
    }
    case MemoryModelTypes::GLSL:
    {
        memModelOp = "OpMemoryModel Logical GLSL450";

        break;
    }
    default:
    {
        DE_ASSERT(0);
        DE_FATAL("Unsupported memory model");
        break;
    }
    }
}

static void adjustSpecForDataTypes(DATA_TYPE dataType, ComputeShaderSpec &spec,
                                   std::vector<const char *> &spvExtensions, std::vector<const char *> &spvCapabilities)
{
    DE_UNREF(spvExtensions);

    switch (dataType)
    {
    case DataTypes::UINT8:
    case DataTypes::INT8:
    {
        spvCapabilities.push_back("OpCapability Int8\n");
        spec.requestedVulkanFeatures.extFloat16Int8.shaderInt8 = VK_TRUE;
        break;
    }
    case DataTypes::UINT16:
    case DataTypes::INT16:
    {
        spvCapabilities.push_back("OpCapability Int16\n");
        spec.requestedVulkanFeatures.coreFeatures.shaderInt16 = VK_TRUE;
        break;
    }
    case DataTypes::FLOAT16:
    {
        spvCapabilities.push_back("OpCapability Float16\n");
        spec.requestedVulkanFeatures.extFloat16Int8.shaderFloat16 = VK_TRUE;
        break;
    }
    case DataTypes::UINT32:
    case DataTypes::INT32:
    case DataTypes::FLOAT32:
        break;
    case DataTypes::FLOAT64:
    {
        spvCapabilities.push_back("OpCapability Float64\n");
        spec.requestedVulkanFeatures.coreFeatures.shaderFloat64 = VK_TRUE;
        break;
    }
    case DataTypes::UINT64:
    case DataTypes::INT64:
    {
        spvCapabilities.push_back("OpCapability Int64\n");
        spec.requestedVulkanFeatures.coreFeatures.shaderInt64 = VK_TRUE;
        break;
    }
    default:
    {
        DE_ASSERT(0);
        DE_FATAL("Unknown data type");
        break;
    }
    }
}

static void adjustSpecForAtomicOperations(DATA_TYPE dataType, ComputeShaderSpec &spec,
                                          std::vector<const char *> &spvExtensions,
                                          std::vector<const char *> &spvCapabilities)
{
    DE_UNREF(spvExtensions);

    switch (dataType)
    {
    case DataTypes::UINT8:
    case DataTypes::INT8:
    case DataTypes::UINT16:
    case DataTypes::INT16:
    case DataTypes::UINT32:
    case DataTypes::INT32:
        break;
    case DataTypes::FLOAT16:
    {
        spec.extensions.push_back("VK_EXT_shader_atomic_float");
        spec.extensions.push_back(
            "VK_EXT_shader_atomic_float2"); // VK_EXT_shader_atomic_float2 requires VK_EXT_shader_atomic_float to be enabled
        spec.requestedVulkanFeatures.extShaderAtomicFloat2.shaderBufferFloat16Atomics = VK_TRUE;
        break;
    }
    case DataTypes::FLOAT32:
    {
        spec.extensions.push_back("VK_EXT_shader_atomic_float");
        spec.requestedVulkanFeatures.extShaderAtomicFloat.shaderBufferFloat32Atomics = VK_TRUE;
        break;
    }
    case DataTypes::FLOAT64:
    {
        spec.extensions.push_back("VK_EXT_shader_atomic_float");
        spec.requestedVulkanFeatures.extShaderAtomicFloat.shaderBufferFloat64Atomics = VK_TRUE;
        break;
    }
    case DataTypes::UINT64:
    case DataTypes::INT64:
    {
        spvCapabilities.push_back("OpCapability Int64Atomics\n");
        spec.extensions.push_back("VK_KHR_shader_atomic_int64");
        spec.requestedVulkanFeatures.extShaderAtomicInt64.shaderBufferInt64Atomics = VK_TRUE;
        break;
    }
    default:
    {
        DE_ASSERT(0);
        DE_FATAL("Unknown data type");
        break;
    }
    }
}

static void adjustSpecForAtomicAddOperations(DATA_TYPE dataType, ComputeShaderSpec &spec,
                                             std::vector<const char *> &spvExtensions,
                                             std::vector<const char *> &spvCapabilities)
{
    switch (dataType)
    {
    case DataTypes::UINT8:
    case DataTypes::INT8:
    case DataTypes::UINT16:
    case DataTypes::INT16:
    case DataTypes::UINT32:
    case DataTypes::INT32:
    case DataTypes::UINT64:
    case DataTypes::INT64:
        break;
    case DataTypes::FLOAT16:
    {
        spvExtensions.push_back("OpExtension \"SPV_EXT_shader_atomic_float16_add\"\n");
        spvCapabilities.push_back("OpCapability AtomicFloat16AddEXT\n");
        spec.requestedVulkanFeatures.extShaderAtomicFloat2.shaderBufferFloat16AtomicAdd = VK_TRUE;
        break;
    }
    case DataTypes::FLOAT32:
    {
        spvExtensions.push_back("OpExtension \"SPV_EXT_shader_atomic_float_add\"\n");
        spvCapabilities.push_back("OpCapability AtomicFloat32AddEXT\n");
        spec.requestedVulkanFeatures.extShaderAtomicFloat.shaderBufferFloat32AtomicAdd = VK_TRUE;
        break;
    }
    case DataTypes::FLOAT64:
    {
        spvExtensions.push_back("OpExtension \"SPV_EXT_shader_atomic_float_add\"\n");
        spvCapabilities.push_back("OpCapability AtomicFloat64AddEXT\n");
        spec.requestedVulkanFeatures.extShaderAtomicFloat.shaderBufferFloat64AtomicAdd = VK_TRUE;
        break;
    }
    default:
    {
        DE_ASSERT(0);
        DE_FATAL("Unknown data type");
        break;
    }
    }
}

static void adjustSpecForAtomicMinMaxOperations(DATA_TYPE dataType, ComputeShaderSpec &spec,
                                                std::vector<const char *> &spvExtensions,
                                                std::vector<const char *> &spvCapabilities)
{
    switch (dataType)
    {
    case DataTypes::UINT8:
    case DataTypes::INT8:
    case DataTypes::UINT16:
    case DataTypes::INT16:
    case DataTypes::UINT32:
    case DataTypes::INT32:
    case DataTypes::UINT64:
    case DataTypes::INT64:
        break;
    case DataTypes::FLOAT16:
    {
        spvExtensions.push_back("OpExtension \"SPV_EXT_shader_atomic_float_min_max\"\n");
        spvCapabilities.push_back("OpCapability AtomicFloat16MinMaxEXT\n");
        spec.requestedVulkanFeatures.extShaderAtomicFloat2.shaderBufferFloat16AtomicMinMax = VK_TRUE;

        break;
    }
    case DataTypes::FLOAT32:
    {
        spvExtensions.push_back("OpExtension \"SPV_EXT_shader_atomic_float_min_max\"\n");
        spvCapabilities.push_back("OpCapability AtomicFloat32MinMaxEXT\n");
        spec.requestedVulkanFeatures.extShaderAtomicFloat2.shaderBufferFloat32AtomicMinMax = VK_TRUE;
        spec.extensions.push_back("VK_EXT_shader_atomic_float2");
        break;
    }
    case DataTypes::FLOAT64:
    {
        spvExtensions.push_back("OpExtension \"SPV_EXT_shader_atomic_float_min_max\"\n");
        spvCapabilities.push_back("OpCapability AtomicFloat64MinMaxEXT\n");
        spec.requestedVulkanFeatures.extShaderAtomicFloat2.shaderBufferFloat64AtomicMinMax = VK_TRUE;
        spec.extensions.push_back("VK_EXT_shader_atomic_float2");
        break;
    }
    default:
    {
        DE_ASSERT(0);
        DE_FATAL("Unknown data type");
        break;
    }
    }
}

static void adjustSpecForUntypedPointers(ComputeShaderSpec &spec, std::vector<const char *> &spvExtensions,
                                         std::vector<const char *> &spvCapabilities)
{
    spvExtensions.push_back("OpExtension \"SPV_KHR_storage_buffer_storage_class\"\n");
    spvExtensions.push_back("OpExtension \"SPV_KHR_untyped_pointers\"\n");
    spvCapabilities.push_back("OpCapability UntypedPointersKHR\n");
    spec.requestedVulkanFeatures.extShaderUntypedPointers.shaderUntypedPointers = VK_TRUE;
    spec.extensions.push_back("VK_KHR_shader_untyped_pointers");
}

static void adjustSpecForMemoryInterpretation(ComputeShaderSpec &spec, std::vector<const char *> & /*spvExtensions*/,
                                              std::vector<const char *> &spvCapabilities,
                                              MEMORY_INTERPRETATION_TEST_CASE testCase)
{
    switch (testCase)
    {
    case MemoryInterpretationTestCases::SHORT2_NO_STORAGE_CAP:
    {
        spvCapabilities.push_back("OpCapability Int16\n");
        spec.requestedVulkanFeatures.coreFeatures.shaderInt16 = VK_TRUE;

        break;
    }
    case MemoryInterpretationTestCases::CHAR4_NO_STORAGE_CAP:
    {
        spvCapabilities.push_back("OpCapability Int8\n");
        spec.requestedVulkanFeatures.extFloat16Int8.shaderInt8 = VK_TRUE;

        break;
    }
    case MemoryInterpretationTestCases::CHAR2_16BIT_STORAGE_CAP:
    {
        spvCapabilities.push_back("OpCapability Int8\n");
        spvCapabilities.push_back("OpCapability Int16\n");
        spec.requestedVulkanFeatures.ext16BitStorage.storageBuffer16BitAccess = VK_TRUE;
        spec.requestedVulkanFeatures.extFloat16Int8.shaderInt8                = VK_TRUE;
        spec.requestedVulkanFeatures.coreFeatures.shaderInt16                 = VK_TRUE;

        break;
    }
    default:
    {
        break;
    }
    }
}

static void adjustSpecForBlockArray(ComputeShaderSpec &spec, std::vector<const char *> &spvExtensions,
                                    std::vector<const char *> &spvCapabilities, BLOCK_ARRAY_TEST_CASE testCase)
{
    spvExtensions.push_back("OpExtension \"SPV_EXT_descriptor_indexing\"\n");
    spvCapabilities.push_back("OpCapability StorageBufferArrayDynamicIndexing\n");
    spec.requestedVulkanFeatures.coreFeatures.shaderStorageBufferArrayDynamicIndexing = VK_TRUE;
    spec.extensions.push_back("VK_KHR_storage_buffer_storage_class");
    spec.extensions.push_back("VK_EXT_descriptor_indexing");
    switch (testCase)
    {
    case BlockArrayTestCases::REINTERPRET_BLOCK_NORMAL_PTR_ACCESS_CHAIN:
    case BlockArrayTestCases::REINTERPRET_BLOCK_UNTYPED_PTR_ACCESS_CHAIN:
    case BlockArrayTestCases::SELECT_BLOCK_NORMAL_ACCESS_CHAIN:
    case BlockArrayTestCases::SELECT_BLOCK_NORMAL_PTR_ACCESS_CHAIN:
    case BlockArrayTestCases::SELECT_BLOCK_UNTYPED_ACCESS_CHAIN:
    case BlockArrayTestCases::SELECT_BLOCK_UNTYPED_PTR_ACCESS_CHAIN:
    {
        spvExtensions.push_back("OpExtension \"SPV_KHR_variable_pointers\"\n");
        spvCapabilities.push_back("OpCapability VariablePointersStorageBuffer\n");
        spec.requestedVulkanFeatures.extVariablePointers.variablePointersStorageBuffer = VK_TRUE;
        spec.extensions.push_back("VK_KHR_variable_pointers");
    }
    default:
    {
        break;
    }
    }
}

static void adjustSpecForSmallContainerType(CONTAINER_TYPE containerType, DATA_TYPE dataType, ComputeShaderSpec &spec,
                                            std::vector<const char *> &spvExtensions,
                                            std::vector<const char *> &spvCapabilities)
{
    switch (dataType)
    {
    case DataTypes::UINT8:
    case DataTypes::INT8:
    {
        spvExtensions.push_back("OpExtension \"SPV_KHR_8bit_storage\"\n");

        switch (containerType)
        {
        case ContainerTypes::STORAGE_BUFFER:
        {
            spvCapabilities.push_back("OpCapability StorageBuffer8BitAccess\n");
            spec.requestedVulkanFeatures.ext8BitStorage.storageBuffer8BitAccess = VK_TRUE;
            break;
        }
        case ContainerTypes::UNIFORM:
        {
            spvCapabilities.push_back("OpCapability UniformAndStorageBuffer8BitAccess\n");
            spec.requestedVulkanFeatures.ext8BitStorage.uniformAndStorageBuffer8BitAccess = VK_TRUE;
            break;
        }
        case ContainerTypes::PUSH_CONSTANT:
        {
            spvCapabilities.push_back("OpCapability StoragePushConstant8\n");
            spec.requestedVulkanFeatures.ext8BitStorage.storagePushConstant8 = VK_TRUE;
            break;
        }
        case ContainerTypes::WORKGROUP:
            break;
        default:
        {
            DE_ASSERT(0);
            DE_FATAL("Unknown container type");
            break;
        }
        }
        break;
    }
    case DataTypes::UINT16:
    case DataTypes::INT16:
    case DataTypes::FLOAT16:
    {
        spvExtensions.push_back("OpExtension \"SPV_KHR_16bit_storage\"\n");

        switch (containerType)
        {
        case ContainerTypes::STORAGE_BUFFER:
        {
            spvCapabilities.push_back("OpCapability StorageBuffer16BitAccess\n");
            spec.requestedVulkanFeatures.ext16BitStorage.storageBuffer16BitAccess = VK_TRUE;
            break;
        }
        case ContainerTypes::UNIFORM:
        {
            spvCapabilities.push_back("OpCapability UniformAndStorageBuffer16BitAccess\n");
            spec.requestedVulkanFeatures.ext16BitStorage.uniformAndStorageBuffer16BitAccess = VK_TRUE;
            break;
        }
        case ContainerTypes::PUSH_CONSTANT:
        {
            spvCapabilities.push_back("OpCapability StoragePushConstant16\n");
            spec.requestedVulkanFeatures.ext16BitStorage.storagePushConstant16 = VK_TRUE;
            break;
        }
        default:
        {
            DE_ASSERT(0);
            DE_FATAL("Unknown container type");
            break;
        }
        }
        break;
    }
    case DataTypes::UINT32:
    case DataTypes::INT32:
    case DataTypes::FLOAT32:
    case DataTypes::UINT64:
    case DataTypes::INT64:
    case DataTypes::FLOAT64:
        break;
    default:
    {
        DE_ASSERT(0);
        DE_FATAL("Unknown data type");
        break;
    }
    }
}

static void adjustSpecForVariablePointers(ComputeShaderSpec &spec, std::vector<const char *> &spvExtensions,
                                          std::vector<const char *> &spvCapabilities)
{
    spec.requestedVulkanFeatures.extVariablePointers.variablePointers              = VK_TRUE;
    spec.requestedVulkanFeatures.extVariablePointers.variablePointersStorageBuffer = VK_TRUE;
    spec.extensions.push_back("VK_KHR_variable_pointers");

    spvCapabilities.push_back("OpCapability VariablePointersStorageBuffer\n");
    spvCapabilities.push_back("OpCapability VariablePointers\n");
    spvExtensions.push_back("OpExtension \"SPV_KHR_variable_pointers\"\n");
}

static void adjustSpecForPhysicalStorageBuffer(MEMORY_MODEL_TYPE memModel, ComputeShaderSpec &spec,
                                               std::string &memModelOp, std::vector<const char *> &spvExtensions,
                                               std::vector<const char *> &spvCapabilities)
{
    spvCapabilities.push_back("OpCapability PhysicalStorageBufferAddresses\n");
    spvExtensions.push_back("OpExtension \"SPV_KHR_physical_storage_buffer\"\n");
    spec.extensions.push_back("VK_KHR_buffer_device_address");

    // Memory model spec needs to be overwritten.
    switch (memModel)
    {
    case MemoryModelTypes::VULKAN:
    {
        memModelOp = "OpMemoryModel PhysicalStorageBuffer64 Vulkan";
        break;
    }
    case MemoryModelTypes::GLSL:
    {
        memModelOp = "OpMemoryModel PhysicalStorageBuffer64 GLSL450";
        break;
    }
    default:
    {
        DE_ASSERT(0);
        DE_FATAL("Unsupported memory model");
        break;
    }
    }
}

static void adjustSpecForWorkgroupMemoryExplicitLayout(DATA_TYPE dataType, ComputeShaderSpec &spec,
                                                       std::vector<const char *> &spvExtensions,
                                                       std::vector<const char *> &spvCapabilities)
{
    switch (dataType)
    {
    case DataTypes::UINT8:
    case DataTypes::INT8:
    {
        spvCapabilities.push_back("OpCapability WorkgroupMemoryExplicitLayout8BitAccessKHR\n");
        spec.requestedVulkanFeatures.extWorkgroupMemoryExplicitLayout.workgroupMemoryExplicitLayout8BitAccess = VK_TRUE;
        break;
    }
    case DataTypes::UINT16:
    case DataTypes::INT16:
    case DataTypes::FLOAT16:
    {
        spvCapabilities.push_back("OpCapability WorkgroupMemoryExplicitLayout16BitAccessKHR\n");
        spec.requestedVulkanFeatures.extWorkgroupMemoryExplicitLayout.workgroupMemoryExplicitLayout16BitAccess =
            VK_TRUE;
        break;
    }
    case DataTypes::UINT32:
    case DataTypes::INT32:
    case DataTypes::FLOAT32:
    case DataTypes::UINT64:
    case DataTypes::INT64:
    case DataTypes::FLOAT64:
    {
        break;
    }
    default:
    {
        DE_ASSERT(0);
        DE_FATAL("Unknown data type");
        break;
    }
    }

    spec.requestedVulkanFeatures.extWorkgroupMemoryExplicitLayout.workgroupMemoryExplicitLayout = VK_TRUE;
    spec.extensions.push_back("VK_KHR_workgroup_memory_explicit_layout");

    spvCapabilities.push_back("OpCapability WorkgroupMemoryExplicitLayoutKHR\n");
    spvExtensions.push_back("OpExtension \"SPV_KHR_workgroup_memory_explicit_layout\"\n");
}

static void adjustSpecForCooperativeMatrix(ComputeShaderSpec &spec, std::vector<const char *> &spvExtensions,
                                           std::vector<const char *> &spvCapabilities)
{
    spvCapabilities.push_back("OpCapability CooperativeMatrixKHR\n");
    spvExtensions.push_back("OpExtension \"SPV_KHR_cooperative_matrix\"\n");
    spec.extensions.push_back("VK_KHR_cooperative_matrix");
    spec.requestedVulkanFeatures.extCooperativeMatrix.cooperativeMatrix = VK_TRUE;
}

std::string toString(const std::vector<const char *> &vec)
{
    std::string result;
    for (const auto &str : vec)
    {
        result += str;
    }
    return result;
}

std::vector<uint32_t> getOffsets(MEMORY_INTERPRETATION_TEST_CASE testCase)
{
    const uint32_t numOffsets = 4;
    std::vector<uint32_t> offsets(numOffsets, 0);
    switch (testCase)
    {
    case MemoryInterpretationTestCases::LARGE_ARRAY_STRIDE:
    {
        // The large array stride is 32 bytes or 8 uint32s.
        offsets[0] = 16; // offset = 64 bytes
        offsets[1] = 24; // offset = 96 bytes
        offsets[2] = 32; // offset = 128 bytes
        offsets[3] = 40; // offset = 160 bytes

        break;
    }
    case MemoryInterpretationTestCases::NON_ZERO_OFFSET:
    {
        // Struct members start at offset 16 and are strided every 24 bytes.
        offsets[0] = 5;  // offset = 20 bytes
        offsets[1] = 17; // offset = 68 bytes
        offsets[2] = 29; // offset = 116 bytes
        offsets[3] = 41; // offset = 164 bytes

        break;
    }
    case MemoryInterpretationTestCases::MIXED_OFFSETS:
    {
        offsets[0] = 16; // offset = 64 bytes
        offsets[1] = 2;  // offset = 8 bytes
        offsets[2] = 12; // offset = 48 bytes
        offsets[3] = 0;  // offset = 0 bytes

        break;
    }
    case MemoryInterpretationTestCases::MULTIPLE_ACCESS_CHAINS:
    {
        offsets[0] = 15; // offset = 60 bytes
        offsets[1] = 27; // offset = 108 bytes
        offsets[2] = 33; // offset = 132 bytes
        offsets[3] = 39; // offset = 156 bytes

        break;
    }
    case MemoryInterpretationTestCases::SHORT2_NO_STORAGE_CAP:
    case MemoryInterpretationTestCases::CHAR4_NO_STORAGE_CAP:
    case MemoryInterpretationTestCases::CHAR2_16BIT_STORAGE_CAP:
    case MemoryInterpretationTestCases::UNTYPED_FROM_TYPED_VAR:
    case MemoryInterpretationTestCases::UNTYPED_FROM_TYPED_ACCESS_CHAIN:
    {
        offsets[0] = 1; // offset = 4 bytes
        offsets[1] = 3; // offset = 12 bytes
        offsets[2] = 5; // offset = 20 bytes
        offsets[3] = 7; // offset = 28 bytes

        break;
    }
    default:
    {
        DE_ASSERT(0);
        DE_FATAL("Unknown test case.");
        break;
    }
    }

    return offsets;
}

std::vector<uint32_t> getIndices(MEMORY_INTERPRETATION_TEST_CASE testCase)
{
    const uint32_t numOffsets = 4;
    std::vector<uint32_t> indices(numOffsets, 0);
    switch (testCase)
    {
    case MemoryInterpretationTestCases::LARGE_ARRAY_STRIDE:
    {
        indices[0] = 2;
        indices[1] = 3;
        indices[2] = 4;
        indices[3] = 5;

        break;
    }
    case MemoryInterpretationTestCases::NON_ZERO_OFFSET:
    {
        indices[0] = 0;
        indices[1] = 2;
        indices[2] = 4;
        indices[3] = 6;
        break;
    }
    case MemoryInterpretationTestCases::MIXED_OFFSETS:
    {
        indices[0] = 0;
        indices[1] = 1;
        indices[2] = 2;
        indices[3] = 3;

        break;
    }
    case MemoryInterpretationTestCases::MULTIPLE_ACCESS_CHAINS:
    {
        indices[0] = 2;
        indices[1] = 4;
        indices[2] = 5;
        indices[3] = 6;

        break;
    }
    case MemoryInterpretationTestCases::SHORT2_NO_STORAGE_CAP:
    case MemoryInterpretationTestCases::CHAR4_NO_STORAGE_CAP:
    case MemoryInterpretationTestCases::CHAR2_16BIT_STORAGE_CAP:
    case MemoryInterpretationTestCases::UNTYPED_FROM_TYPED_VAR:
    case MemoryInterpretationTestCases::UNTYPED_FROM_TYPED_ACCESS_CHAIN:
    {
        // The char2 case internally doubles the index.
        indices[0] = 1;
        indices[1] = 3;
        indices[2] = 5;
        indices[3] = 7;

        break;
    }
    default:
    {
        DE_ASSERT(0);
        DE_FATAL("Unknown test case.");
        break;
    }
    }

    return indices;
}

enum class FillingTypes : uint8_t
{
    RANDOM,
    VALUE,
    INCREMENTED,
    _ENUM_COUNT
};
using FILLING_TYPE = FillingTypes;

struct FilledBufferDesc
{
    union
    {
        uint32_t seed;
        double value;
    };
    uint32_t elemCount;
    uint32_t padding;
    DATA_TYPE dataType;
    FILLING_TYPE fillType;
};

struct AtomicResourceDesc
{
    DATA_TYPE dataType;
    uint32_t elemCount;
};

static BufferSp createFilledBuffer(const FilledBufferDesc &desc)
{
    switch (desc.dataType)
    {
    case DataTypes::UINT8:
    {
        if (desc.fillType == FillingTypes::VALUE)
        {
            const uint8_t converted = static_cast<uint8_t>(desc.value);
            return BufferSp(new Buffer<uint8_t>(std::vector<uint8_t>(desc.elemCount, converted), desc.padding));
        }
        else if (desc.fillType == FillingTypes::INCREMENTED)
        {
            std::vector<uint8_t> data;
            data.resize(desc.elemCount);
            for (uint32_t ndx = 0; ndx < desc.elemCount; ++ndx)
            {
                data[ndx] = static_cast<uint8_t>(ndx);
            }
            return BufferSp(new Buffer<uint8_t>(data, desc.padding));
        }
        else
        {
            de::Random rnd(desc.seed);
            std::vector<uint8_t> randoms;
            if (desc.elemCount <= 24)
            {
                randoms = getUint8s(rnd, 24);
                randoms.resize(desc.elemCount);
            }
            else
            {
                randoms = getUint8s(rnd, desc.elemCount);
            }
            return BufferSp(new Buffer<uint8_t>(randoms, desc.padding));
        }
    }
    case DataTypes::INT8:
    {
        if (desc.fillType == FillingTypes::VALUE)
        {
            const int8_t converted = static_cast<int8_t>(desc.value);
            return BufferSp(new Buffer<int8_t>(std::vector<int8_t>(desc.elemCount, converted), desc.padding));
        }
        else if (desc.fillType == FillingTypes::INCREMENTED)
        {
            std::vector<int8_t> data;
            data.resize(desc.elemCount);
            for (uint32_t ndx = 0; ndx < desc.elemCount; ++ndx)
            {
                data[ndx] = static_cast<int8_t>(ndx);
            }
            return BufferSp(new Buffer<int8_t>(data, desc.padding));
        }
        else
        {
            de::Random rnd(desc.seed);
            std::vector<int8_t> randoms;
            if (desc.elemCount <= 24)
            {
                randoms = getInt8s(rnd, 24);
                randoms.resize(desc.elemCount);
            }
            else
            {
                randoms = getInt8s(rnd, desc.elemCount);
            }
            return BufferSp(new Buffer<int8_t>(randoms, desc.padding));
        }
    }
    case DataTypes::UINT16:
    {
        if (desc.fillType == FillingTypes::VALUE)
        {
            const uint16_t converted = static_cast<uint16_t>(desc.value);
            return BufferSp(new Buffer<uint16_t>(std::vector<uint16_t>(desc.elemCount, converted), desc.padding));
        }
        else if (desc.fillType == FillingTypes::INCREMENTED)
        {
            std::vector<uint16_t> data;
            data.resize(desc.elemCount);
            for (uint32_t ndx = 0; ndx < desc.elemCount; ++ndx)
            {
                data[ndx] = static_cast<uint16_t>(ndx);
            }
            return BufferSp(new Buffer<uint16_t>(data, desc.padding));
        }
        else
        {
            de::Random rnd(desc.seed);
            std::vector<uint16_t> randoms;
            if (desc.elemCount <= 24)
            {
                randoms = getUint16s(rnd, 24);
                randoms.resize(desc.elemCount);
            }
            else
            {
                randoms = getUint16s(rnd, desc.elemCount);
            }
            return BufferSp(new Buffer<uint16_t>(randoms, desc.padding));
        }
    }
    case DataTypes::INT16:
    {
        if (desc.fillType == FillingTypes::VALUE)
        {
            const int16_t converted = static_cast<int16_t>(desc.value);
            return BufferSp(new Buffer<int16_t>(std::vector<int16_t>(desc.elemCount, converted), desc.padding));
        }
        else if (desc.fillType == FillingTypes::INCREMENTED)
        {
            std::vector<int16_t> data;
            data.resize(desc.elemCount);
            for (uint32_t ndx = 0; ndx < desc.elemCount; ++ndx)
            {
                data[ndx] = static_cast<int16_t>(ndx);
            }
            return BufferSp(new Buffer<int16_t>(data, desc.padding));
        }
        else
        {
            de::Random rnd(desc.seed);
            std::vector<int16_t> randoms;
            if (desc.elemCount <= 24)
            {
                randoms = getInt16s(rnd, 24);
                randoms.resize(desc.elemCount);
            }
            else
            {
                randoms = getInt16s(rnd, desc.elemCount);
            }
            return BufferSp(new Buffer<int16_t>(randoms, desc.padding));
        }
    }
    case DataTypes::FLOAT16:
    {
        if (desc.fillType == FillingTypes::VALUE)
        {
            const deFloat16 converted = static_cast<deFloat16>(desc.value);
            return BufferSp(new Buffer<deFloat16>(std::vector<deFloat16>(desc.elemCount, converted), desc.padding));
        }
        else if (desc.fillType == FillingTypes::INCREMENTED)
        {
            std::vector<deFloat16> data;
            data.resize(desc.elemCount);
            for (uint32_t ndx = 0; ndx < desc.elemCount; ++ndx)
            {
                data[ndx] = static_cast<deFloat16>(ndx);
            }
            return BufferSp(new Buffer<deFloat16>(data, desc.padding));
        }
        else
        {
            de::Random rnd(desc.seed);
            std::vector<deFloat16> randoms;
            if (desc.elemCount <= 24)
            {
                randoms = getFloat16s(rnd, 24);
                randoms.resize(desc.elemCount);
            }
            else
            {
                randoms = getFloat16s(rnd, desc.elemCount);
            }
            return BufferSp(new Buffer<deFloat16>(randoms, desc.padding));
        }
    }
    case DataTypes::UINT32:
    {
        if (desc.fillType == FillingTypes::VALUE)
        {
            const uint32_t converted = static_cast<uint32_t>(desc.value);
            return BufferSp(new Buffer<uint32_t>(std::vector<uint32_t>(desc.elemCount, converted), desc.padding));
        }
        else if (desc.fillType == FillingTypes::INCREMENTED)
        {
            std::vector<uint32_t> data;
            data.resize(desc.elemCount);
            for (uint32_t ndx = 0; ndx < desc.elemCount; ++ndx)
            {
                data[ndx] = static_cast<uint32_t>(ndx);
            }
            return BufferSp(new Buffer<uint32_t>(data, desc.padding));
        }
        else
        {
            de::Random rnd(desc.seed);
            std::vector<uint32_t> randoms;
            if (desc.elemCount <= 24)
            {
                randoms = getUint32s(rnd, 24);
                randoms.resize(desc.elemCount);
            }
            else
            {
                randoms = getUint32s(rnd, desc.elemCount);
            }
            return BufferSp(new Buffer<uint32_t>(randoms, desc.padding));
        }
    }
    case DataTypes::INT32:
    {
        if (desc.fillType == FillingTypes::VALUE)
        {
            const int32_t converted = static_cast<int32_t>(desc.value);
            return BufferSp(new Buffer<int32_t>(std::vector<int32_t>(desc.elemCount, converted), desc.padding));
        }
        else if (desc.fillType == FillingTypes::INCREMENTED)
        {
            std::vector<int32_t> data;
            data.resize(desc.elemCount);
            for (uint32_t ndx = 0; ndx < desc.elemCount; ++ndx)
            {
                data[ndx] = static_cast<int32_t>(ndx);
            }
            return BufferSp(new Buffer<int32_t>(data, desc.padding));
        }
        else
        {
            de::Random rnd(desc.seed);
            std::vector<int32_t> randoms;
            if (desc.elemCount <= 24)
            {
                randoms = getInt32s(rnd, 24);
                randoms.resize(desc.elemCount);
            }
            else
            {
                randoms = getInt32s(rnd, desc.elemCount);
            }
            return BufferSp(new Buffer<int32_t>(randoms, desc.padding));
        }
    }
    case DataTypes::FLOAT32:
    {
        if (desc.fillType == FillingTypes::VALUE)
        {
            const float converted = static_cast<float>(desc.value);
            return BufferSp(new Buffer<float>(std::vector<float>(desc.elemCount, converted), desc.padding));
        }
        else if (desc.fillType == FillingTypes::INCREMENTED)
        {
            std::vector<float> data;
            data.resize(desc.elemCount);
            for (uint32_t ndx = 0; ndx < desc.elemCount; ++ndx)
            {
                data[ndx] = static_cast<float>(ndx);
            }
            return BufferSp(new Buffer<float>(data, desc.padding));
        }
        else
        {
            de::Random rnd(desc.seed);
            std::vector<float> randoms;
            if (desc.elemCount <= 24)
            {
                randoms = getFloat32s(rnd, 24);
                randoms.resize(desc.elemCount);
            }
            else
            {
                randoms = getFloat32s(rnd, desc.elemCount);
            }
            return BufferSp(new Buffer<float>(randoms, desc.padding));
        }
    }
    case DataTypes::UINT64:
    {
        if (desc.fillType == FillingTypes::VALUE)
        {
            const uint64_t converted = static_cast<uint64_t>(desc.value);
            return BufferSp(new Buffer<uint64_t>(std::vector<uint64_t>(desc.elemCount, converted), desc.padding));
        }
        else if (desc.fillType == FillingTypes::INCREMENTED)
        {
            std::vector<uint64_t> data;
            data.resize(desc.elemCount);
            for (uint32_t ndx = 0; ndx < desc.elemCount; ++ndx)
            {
                data[ndx] = static_cast<uint64_t>(ndx);
            }
            return BufferSp(new Buffer<uint64_t>(data, desc.padding));
        }
        else
        {
            de::Random rnd(desc.seed);
            std::vector<uint64_t> randoms;
            if (desc.elemCount <= 24)
            {
                randoms = getUint64s(rnd, 24);
                randoms.resize(desc.elemCount);
            }
            else
            {
                randoms = getUint64s(rnd, desc.elemCount);
            }
            return BufferSp(new Buffer<uint64_t>(randoms, desc.padding));
        }
    }
    case DataTypes::INT64:
    {
        if (desc.fillType == FillingTypes::VALUE)
        {
            const int64_t converted = static_cast<int64_t>(desc.value);
            return BufferSp(new Buffer<int64_t>(std::vector<int64_t>(desc.elemCount, converted), desc.padding));
        }
        else if (desc.fillType == FillingTypes::INCREMENTED)
        {
            std::vector<int64_t> data;
            data.resize(desc.elemCount);
            for (uint32_t ndx = 0; ndx < desc.elemCount; ++ndx)
            {
                data[ndx] = static_cast<int64_t>(ndx);
            }
            return BufferSp(new Buffer<int64_t>(data, desc.padding));
        }
        else
        {
            de::Random rnd(desc.seed);
            std::vector<int64_t> randoms;
            if (desc.elemCount <= 24)
            {
                randoms = getInt64s(rnd, 24);
                randoms.resize(desc.elemCount);
            }
            else
            {
                randoms = getInt64s(rnd, desc.elemCount);
            }
            return BufferSp(new Buffer<int64_t>(randoms, desc.padding));
        }
    }
    case DataTypes::FLOAT64:
    {
        if (desc.fillType == FillingTypes::VALUE)
        {
            return BufferSp(new Buffer<double>(std::vector<double>(desc.elemCount, desc.value), desc.padding));
        }
        else if (desc.fillType == FillingTypes::INCREMENTED)
        {
            std::vector<double> data;
            data.resize(desc.elemCount);
            for (uint32_t ndx = 0; ndx < desc.elemCount; ++ndx)
            {
                data[ndx] = static_cast<double>(ndx);
            }
            return BufferSp(new Buffer<double>(data, desc.padding));
        }
        else
        {
            de::Random rnd(desc.seed);
            std::vector<double> randoms;
            if (desc.elemCount <= 24)
            {
                randoms = getFloat64s(rnd, 24);
                randoms.resize(desc.elemCount);
            }
            else
            {
                randoms = getFloat64s(rnd, desc.elemCount);
            }
            return BufferSp(new Buffer<double>(randoms, desc.padding));
        }
    }
    default:
        DE_ASSERT(0);
        DE_FATAL("Unsupported data type");
        return BufferSp(nullptr);
    }
}

static Resource createFilledResource(VkDescriptorType type, const FilledBufferDesc &desc)
{
    return Resource(createFilledBuffer(desc), type);
}

static Resource createAtomicResource(const AtomicResourceDesc &desc, const std::vector<AtomicOpDesc> &atomicOpDescs)
{
    const DATA_TYPE type = desc.dataType;

    switch (type)
    {
    case DataTypes::UINT8:
    {
        return Resource(BufferSp(new AtomicBuffer<uint8_t>(std::vector<uint8_t>(desc.elemCount), atomicOpDescs)));
    }
    case DataTypes::INT8:
    {
        return Resource(BufferSp(new AtomicBuffer<int8_t>(std::vector<int8_t>(desc.elemCount), atomicOpDescs)));
    }
    case DataTypes::UINT16:
    {
        return Resource(BufferSp(new AtomicBuffer<uint16_t>(std::vector<uint16_t>(desc.elemCount), atomicOpDescs)));
    }
    case DataTypes::INT16:
    {
        return Resource(BufferSp(new AtomicBuffer<int16_t>(std::vector<int16_t>(desc.elemCount), atomicOpDescs)));
    }
    case DataTypes::FLOAT16:
    {
        return Resource(
            BufferSp(new AtomicBuffer<tcu::Float16>(std::vector<tcu::Float16>(desc.elemCount), atomicOpDescs)));
    }
    case DataTypes::UINT32:
    {
        return Resource(BufferSp(new AtomicBuffer<uint32_t>(std::vector<uint32_t>(desc.elemCount), atomicOpDescs)));
    }
    case DataTypes::INT32:
    {
        return Resource(BufferSp(new AtomicBuffer<int32_t>(std::vector<int32_t>(desc.elemCount), atomicOpDescs)));
    }
    case DataTypes::FLOAT32:
    {
        return Resource(BufferSp(new AtomicBuffer<float>(std::vector<float>(desc.elemCount), atomicOpDescs)));
    }
    case DataTypes::UINT64:
    {
        return Resource(BufferSp(new AtomicBuffer<uint64_t>(std::vector<uint64_t>(desc.elemCount), atomicOpDescs)));
    }
    case DataTypes::INT64:
    {
        return Resource(BufferSp(new AtomicBuffer<int64_t>(std::vector<int64_t>(desc.elemCount), atomicOpDescs)));
    }
    case DataTypes::FLOAT64:
    {
        return Resource(BufferSp(new AtomicBuffer<double>(std::vector<double>(desc.elemCount), atomicOpDescs)));
    }
    default:
        DE_ASSERT(0);
        DE_FATAL("Unsupported data type");
        return Resource(BufferSp(new AtomicBuffer<uint8_t>(std::vector<uint8_t>(), atomicOpDescs)));
    }

    return Resource(BufferSp(new AtomicBuffer<uint8_t>(std::vector<uint8_t>(), atomicOpDescs)));
}

std::string createShaderHeader(const char *pInterfaces = "", const char *pLocalWrkGrpSize = "1 1 1")
{
    std::string header = std::string("OpCapability Shader\n"
                                     "${capabilities}\n"
                                     "${extensions}\n"
                                     "${memModelOp}\n"
                                     "OpEntryPoint GLCompute %main \"main\" %id ");

    header += pInterfaces;
    header += " \n"
              "OpExecutionMode %main LocalSize " +
              std::string(pLocalWrkGrpSize) + "\n";

    return header;
}

std::string createShaderAnnotations(BASE_TEST_CASE testCase)
{
    std::string annotations = std::string("OpDecorate       %id            BuiltIn GlobalInvocationId\n");

    switch (testCase)
    {
    case BaseTestCases::DESCRIPTOR_ARRAY:
    {
        annotations += std::string("OpDecorate       %array                  ArrayStride   ${stride}\n"

                                   "OpMemberDecorate %block_data             0             Offset       ${offset0}\n"
                                   "OpMemberDecorate %block_data             1             Offset       ${offset1}\n"
                                   "OpMemberDecorate %block_data             2             Offset       ${offset2}\n"
                                   "OpMemberDecorate %block_data             3             Offset       ${offset3}\n"
                                   "OpDecorate       %block_data             Block\n"

                                   "OpMemberDecorate %data                   0             Offset       ${offset0}\n"
                                   "OpMemberDecorate %data                   1             Offset       ${offset1}\n"
                                   "OpMemberDecorate %data                   2             Offset       ${offset2}\n"
                                   "OpMemberDecorate %data                   3             Offset       ${offset3}\n"

                                   "OpDecorate       %input_data_untyped_var DescriptorSet 0\n"
                                   "OpDecorate       %input_data_untyped_var Binding       0\n"

                                   "OpMemberDecorate %output_buffer          0             Offset       0\n"
                                   "OpDecorate       %output_buffer          Block\n"
                                   "OpDecorate       %output_data_var        DescriptorSet 0\n"
                                   "OpDecorate       %output_data_var        Binding       1\n");

        break;
    }
    case BaseTestCases::ARRAY_LENGTH:
    {
        annotations += std::string("OpDecorate       %${baseType}_rta        ArrayStride   ${alignment}\n"

                                   "OpMemberDecorate %input_buffer           0             Offset       0\n"
                                   "OpDecorate       %input_buffer           Block\n"
                                   "OpDecorate       %input_data_untyped_var DescriptorSet 0\n"
                                   "OpDecorate       %input_data_untyped_var Binding       0\n"

                                   "OpMemberDecorate %output_buffer          0             Offset       0\n"
                                   "OpDecorate       %output_buffer          Block\n"
                                   "OpDecorate       %output_data_var        DescriptorSet 0\n"
                                   "OpDecorate       %output_data_var        Binding       1\n");

        break;
    }
    case BaseTestCases::LOAD:
    {
        annotations += std::string("OpMemberDecorate %input_buffer  0       Offset 0\n"
                                   "OpDecorate       %input_buffer  Block\n"

                                   "OpMemberDecorate %output_buffer 0       Offset 0\n"
                                   "OpDecorate       %output_buffer Block\n"

                                   "${storageDecorations}\n");
        break;
    }
    case BaseTestCases::COPY_FROM:
    {
        annotations += std::string("OpDecorate       %array_${baseType}_${threadCount}     ArrayStride ${alignment}\n"

                                   "OpMemberDecorate %input_buffer           0             Offset 0\n"
                                   "OpDecorate       %input_buffer           Block\n"
                                   "OpDecorate       %input_data_untyped_var DescriptorSet 0\n"
                                   "OpDecorate       %input_data_untyped_var Binding       0\n"

                                   "OpMemberDecorate %output_buffer          0             Offset 0\n"
                                   "OpDecorate       %output_buffer          Block\n"
                                   "OpDecorate       %output_data_var        DescriptorSet 0\n"
                                   "OpDecorate       %output_data_var        Binding       1\n");
        break;
    }
    case BaseTestCases::STORE:
    case BaseTestCases::COPY_TO:
    {
        annotations += std::string("OpDecorate %array_${baseType}_${threadCount} ArrayStride ${alignment}\n"

                                   "OpMemberDecorate %input_buffer            0             Offset 0\n"
                                   "OpDecorate       %input_buffer            Block\n"
                                   "OpDecorate       %input_data_var          DescriptorSet 0\n"
                                   "OpDecorate       %input_data_var          Binding       0\n"

                                   "OpMemberDecorate %output_buffer           0             Offset 0\n"
                                   "OpDecorate       %output_buffer           Block\n"
                                   "OpDecorate       %output_data_untyped_var DescriptorSet 0\n"
                                   "OpDecorate       %output_data_untyped_var Binding       1\n");
        break;
    }
    default:
    {
        DE_ASSERT(0);
        DE_FATAL("Unknown test case.");
        break;
    }
    }

    return annotations;
}

std::string createShaderAnnotations(ATOMIC_TEST_CASE testCase)
{
    std::string annotations = std::string("OpDecorate       %id            BuiltIn GlobalInvocationId\n");

    switch (testCase)
    {
    case AtomicTestCases::OP_ATOMIC_INCREMENT:
    case AtomicTestCases::OP_ATOMIC_DECREMENT:
    case AtomicTestCases::OP_ATOMIC_ADD:
    case AtomicTestCases::OP_ATOMIC_SUB:
    case AtomicTestCases::OP_ATOMIC_MIN:
    case AtomicTestCases::OP_ATOMIC_MAX:
    case AtomicTestCases::OP_ATOMIC_AND:
    case AtomicTestCases::OP_ATOMIC_OR:
    case AtomicTestCases::OP_ATOMIC_XOR:
    case AtomicTestCases::OP_ATOMIC_EXCHANGE:
    case AtomicTestCases::OP_ATOMIC_COMPARE_EXCHANGE:
    {
        annotations += std::string("OpMemberDecorate %output_buffer           0             Offset 0\n"
                                   "OpDecorate       %output_buffer           Block\n"
                                   "OpDecorate       %output_data_untyped_var DescriptorSet 0\n"
                                   "OpDecorate       %output_data_untyped_var Binding       0\n");
        break;
    }
    case AtomicTestCases::OP_ATOMIC_LOAD:
    {
        annotations += std::string("OpDecorate       %array_${baseType}_${threadCount}     ArrayStride ${alignment}\n"

                                   "OpMemberDecorate %input_buffer           0             Offset 0\n"
                                   "OpDecorate       %input_buffer           Block\n"
                                   "OpDecorate       %input_data_untyped_var DescriptorSet 0\n"
                                   "OpDecorate       %input_data_untyped_var Binding       0\n"

                                   "OpMemberDecorate %output_buffer          0             Offset 0\n"
                                   "OpDecorate       %output_buffer          Block\n"
                                   "OpDecorate       %output_data_var        DescriptorSet 0\n"
                                   "OpDecorate       %output_data_var        Binding       1\n");

        break;
    }
    case AtomicTestCases::OP_ATOMIC_STORE:
    {
        annotations += std::string("OpDecorate       %array_${baseType}_${threadCount}      ArrayStride ${alignment}\n"

                                   "OpMemberDecorate %input_buffer            0             Offset 0\n"
                                   "OpDecorate       %input_buffer            Block\n"
                                   "OpDecorate       %input_data_var          DescriptorSet 0\n"
                                   "OpDecorate       %input_data_var          Binding       0\n"

                                   "OpMemberDecorate %output_buffer           0             Offset 0\n"
                                   "OpDecorate       %output_buffer           Block\n"
                                   "OpDecorate       %output_data_untyped_var DescriptorSet 0\n"
                                   "OpDecorate       %output_data_untyped_var Binding       1\n"

                                   "OpDecorate       %input_data_var          Aliased\n"
                                   "OpDecorate       %output_data_untyped_var Aliased\n");

        break;
    }
    default:
    {
        DE_ASSERT(0);
        DE_FATAL("Unknown test case.");
        break;
    }
    }

    return annotations;
}

std::string createShaderAnnotations(TYPE_PUNNING_TEST_CASE testCase)
{
    std::string annotations = std::string("OpDecorate       %id            BuiltIn GlobalInvocationId\n");

    switch (testCase)
    {
    case TypePunningTestCases::COPY_FROM_SAME_SIZE_TYPES:
    {
        annotations += std::string("OpDecorate       %array_${baseType}_${threadCount}     ArrayStride ${alignment}\n"
                                   "OpDecorate       %array_${sameSizeType}_${threadCount} ArrayStride ${alignment}\n"

                                   "OpMemberDecorate %input_buffer           0             Offset 0\n"
                                   "OpDecorate       %input_buffer           Block\n"
                                   "OpDecorate       %input_data_untyped_var DescriptorSet 0\n"
                                   "OpDecorate       %input_data_untyped_var Binding       0\n"

                                   "OpMemberDecorate %output_buffer          0             Offset 0\n"
                                   "OpDecorate       %output_buffer          Block\n"
                                   "OpDecorate       %output_data_var        DescriptorSet 0\n"
                                   "OpDecorate       %output_data_var        Binding       1\n");

        break;
    }
    case TypePunningTestCases::LOAD_SAME_SIZE_TYPES:
    {
        annotations += std::string("OpMemberDecorate %input_buffer  0      Offset 0\n"
                                   "OpDecorate       %input_buffer  Block\n"

                                   "OpMemberDecorate %output_buffer 0       Offset 0\n"
                                   "OpDecorate       %output_buffer Block\n"

                                   "${storageDecorations}\n");

        break;
    }
    case TypePunningTestCases::LOAD_SCALAR_VECTOR:
    case TypePunningTestCases::LOAD_VECTOR_SCALAR:
    {
        annotations += std::string("OpMemberDecorate %input_buffer  0      Offset 0\n"
                                   "OpMemberDecorate %input_buffer  1      Offset ${alignment}\n"
                                   "OpDecorate       %input_buffer  Block\n"

                                   "OpMemberDecorate %output_buffer 0       Offset 0\n"
                                   "OpDecorate       %output_buffer Block\n"

                                   "${storageDecorations}\n");

        break;
    }
    case TypePunningTestCases::COPY_FROM_SCALAR_VECTOR:
    case TypePunningTestCases::COPY_FROM_VECTOR_SCALAR:
    {
        annotations += std::string("OpMemberDecorate %input_buffer           0             Offset 0\n"
                                   "OpDecorate       %input_buffer           Block\n"
                                   "OpDecorate       %input_data_untyped_var DescriptorSet 0\n"
                                   "OpDecorate       %input_data_untyped_var Binding       0\n"

                                   "OpMemberDecorate %output_buffer          0             Offset 0\n"
                                   "OpDecorate       %output_buffer          Block\n"
                                   "OpDecorate       %output_data_var        DescriptorSet 0\n"
                                   "OpDecorate       %output_data_var        Binding       1\n");

        break;
    }
    case TypePunningTestCases::COPY_TO_SAME_SIZE_TYPES:
    case TypePunningTestCases::STORE_SAME_SIZE_TYPES:
    {
        annotations += std::string("OpDecorate %array_${baseType}_${threadCount}     ArrayStride ${alignment}\n"
                                   "OpDecorate %array_${sameSizeType}_${threadCount} ArrayStride ${alignment}\n"

                                   "OpMemberDecorate %input_buffer            0             Offset 0\n"
                                   "OpDecorate       %input_buffer            Block\n"
                                   "OpDecorate       %input_data_var          DescriptorSet 0\n"
                                   "OpDecorate       %input_data_var          Binding       0\n"

                                   "OpMemberDecorate %output_buffer           0             Offset 0\n"
                                   "OpDecorate       %output_buffer           Block\n"
                                   "OpDecorate       %output_data_untyped_var DescriptorSet 0\n"
                                   "OpDecorate       %output_data_untyped_var Binding       1\n");
        break;
    }
    case TypePunningTestCases::COPY_TO_SCALAR_VECTOR:
    case TypePunningTestCases::COPY_TO_VECTOR_SCALAR:
    case TypePunningTestCases::STORE_SCALAR_VECTOR:
    case TypePunningTestCases::STORE_VECTOR_SCALAR:
    {
        annotations += std::string("OpMemberDecorate %input_buffer            0             Offset 0\n"
                                   "OpDecorate       %input_buffer            Block\n"
                                   "OpDecorate       %input_data_var          DescriptorSet 0\n"
                                   "OpDecorate       %input_data_var          Binding       0\n"

                                   "OpMemberDecorate %output_buffer           0              Offset 0\n"
                                   "OpDecorate       %output_buffer           Block\n"
                                   "OpDecorate       %output_data_untyped_var DescriptorSet  0\n"
                                   "OpDecorate       %output_data_untyped_var Binding        1\n");
        break;
    }
    case TypePunningTestCases::MULTIPLE_ACCESS_CHAINS:
    {
        annotations += std::string("OpMemberDecorate %data_buffer              0             Offset 0\n"
                                   "OpMemberDecorate %data_buffer              1             Offset ${size}\n"

                                   "OpMemberDecorate %input_buffer             0             Offset 0\n"
                                   "OpDecorate       %input_buffer             Block\n"
                                   "OpDecorate       %input_data_untyped_var   DescriptorSet 0\n"
                                   "OpDecorate       %input_data_untyped_var   Binding       0\n"

                                   "OpMemberDecorate %output_buffer            0             Offset 0\n"
                                   "OpDecorate       %output_buffer            Block\n"
                                   "OpDecorate       %output_data_var          DescriptorSet 0\n"
                                   "OpDecorate       %output_data_var          Binding       1\n");
        break;
    }
    case TypePunningTestCases::CUSTOM_STRUCT_TYPE:
    {
        annotations += std::string("OpMemberDecorate %input_buffer    0             Offset 0\n"
                                   "${inputOffsets:opt}\n"
                                   "OpDecorate       %input_buffer            Block\n"
                                   "OpDecorate       %input_data_untyped_var  DescriptorSet 0\n"
                                   "OpDecorate       %input_data_untyped_var  Binding       0\n"

                                   "OpMemberDecorate %output_buffer   0             Offset 0\n"
                                   "${outputOffsets:opt}\n"
                                   "OpDecorate       %output_buffer   Block\n"
                                   "OpDecorate       %output_data_var DescriptorSet 0\n"
                                   "OpDecorate       %output_data_var Binding       1\n");

        break;
    }
    default:
    {
        DE_ASSERT(0);
        DE_FATAL("Unknown test case.");
        break;
    }
    }

    return annotations;
}

std::string createShaderAnnotations(POINTER_TEST_CASE testCase)
{
    std::string annotations = std::string("OpDecorate %id BuiltIn GlobalInvocationId\n");

    switch (testCase)
    {
    case PointerTestCases::OP_FUNCTION_CALL_PHYSICAL_STORAGE:
    {
        annotations += std::string("OpDecorate       %return_ptr       Restrict\n"
                                   "OpDecorate       %untyped_phys_ptr ArrayStride ${alignment}\n"

                                   "OpDecorate       %data_buffer      Block\n"
                                   "OpMemberDecorate %data_buffer      0 Offset 0\n"

                                   "OpDecorate       %phys_ptrs_struct Block\n"
                                   "OpMemberDecorate %phys_ptrs_struct 0 Offset 0\n"
                                   "OpMemberDecorate %phys_ptrs_struct 1 Offset 8\n"

                                   "OpDecorate       %all_data_var     DescriptorSet 0\n"
                                   "OpDecorate       %all_data_var     Binding       0\n");
        break;
    }
    case PointerTestCases::OP_BITCAST_FROM_UNTYPED_PHYSICAL_STORAGE:
    {
        annotations += std::string("OpDecorate       %untyped_phys_ptr ArrayStride ${alignment}\n"

                                   "OpDecorate       %data_buffer      Block\n"
                                   "OpMemberDecorate %data_buffer      0 Offset 0\n"

                                   "OpDecorate       %phys_ptrs_struct Block\n"
                                   "OpMemberDecorate %phys_ptrs_struct 0 Offset 0\n"
                                   "OpMemberDecorate %phys_ptrs_struct 1 Offset 8\n"

                                   "OpDecorate       %all_data_var     DescriptorSet 0\n"
                                   "OpDecorate       %all_data_var     Binding       0\n");
        break;
    }
    case PointerTestCases::OP_BITCAST_TO_UNTYPED_PHYSICAL_STORAGE:
    {
        annotations += std::string("OpDecorate       %untyped_phys_ptr ArrayStride ${alignment}\n"

                                   "OpDecorate       %data_buffer      Block\n"
                                   "OpMemberDecorate %data_buffer      0 Offset 0\n"

                                   "OpDecorate       %phys_ptrs_struct Block\n"
                                   "OpMemberDecorate %phys_ptrs_struct 0 Offset 0\n"
                                   "OpMemberDecorate %phys_ptrs_struct 1 Offset 8\n"

                                   "OpDecorate       %all_data_var     DescriptorSet 0\n"
                                   "OpDecorate       %all_data_var     Binding       0\n");
        break;
    }
    case PointerTestCases::OP_PHI_PHYSICAL_STORAGE:
    case PointerTestCases::OP_SELECT_PHYSICAL_STORAGE:
    {
        annotations += std::string("OpDecorate       %untyped_phys_ptr ArrayStride ${alignment}\n"

                                   "OpDecorate       %data_buffer      Block\n"
                                   "OpMemberDecorate %data_buffer      0 Offset 0\n"

                                   "OpMemberDecorate %push_constant    0        Offset 0\n"
                                   "OpDecorate       %push_constant    Block\n"

                                   "OpDecorate       %phys_ptrs_struct Block\n"
                                   "OpMemberDecorate %phys_ptrs_struct 0 Offset 0\n"
                                   "OpMemberDecorate %phys_ptrs_struct 1 Offset 8\n"
                                   "OpMemberDecorate %phys_ptrs_struct 2 Offset 16\n"

                                   "OpDecorate       %all_data_var     DescriptorSet 0\n"
                                   "OpDecorate       %all_data_var     Binding       0\n");
        break;
    }
    case PointerTestCases::OP_PTR_ACCESS_CHAIN_PHYSICAL_STORAGE:
    {
        annotations += std::string("OpDecorate       %array_${baseType}_${threadCount} ArrayStride ${alignment}\n"
                                   "OpDecorate       %untyped_phys_ptr                 ArrayStride ${alignment}\n"

                                   "OpDecorate       %data_buffer      Block\n"
                                   "OpMemberDecorate %data_buffer      0 Offset 0\n"

                                   "OpDecorate       %phys_ptrs_struct Block\n"
                                   "OpMemberDecorate %phys_ptrs_struct 0 Offset 0\n"
                                   "OpMemberDecorate %phys_ptrs_struct 1 Offset 8\n"

                                   "OpDecorate       %all_data_var     DescriptorSet 0\n"
                                   "OpDecorate       %all_data_var     Binding       0\n");
        break;
    }
    case PointerTestCases::OP_SELECT_VARIABLE_PTR:
    case PointerTestCases::OP_PHI_VARIABLE_PTR:
    case PointerTestCases::FUNCTION_VARIABLE_VARIABLE_PTR:
    case PointerTestCases::PRIVATE_VARIABLE_VARIABLE_PTR:
    {
        annotations += std::string("OpMemberDecorate %push_constant            0             Offset 0\n"
                                   "OpDecorate       %push_constant            Block\n"

                                   "OpMemberDecorate %input_buffer_0           0             Offset 0\n"
                                   "OpDecorate       %input_buffer_0           Block\n"
                                   "OpDecorate       %input_data_0_untyped_var DescriptorSet 0\n"
                                   "OpDecorate       %input_data_0_untyped_var Binding       0\n"

                                   "OpMemberDecorate %input_buffer_1           0             Offset 0\n"
                                   "OpDecorate       %input_buffer_1           Block\n"
                                   "OpDecorate       %input_data_1_untyped_var DescriptorSet 0\n"
                                   "OpDecorate       %input_data_1_untyped_var Binding       1\n"

                                   "OpMemberDecorate %output_buffer            0             Offset 0\n"
                                   "OpDecorate       %output_buffer            Block\n"
                                   "OpDecorate       %output_data_var          DescriptorSet 0\n"
                                   "OpDecorate       %output_data_var          Binding       2\n");
        break;
    }
    case PointerTestCases::OP_PTR_EQUAL_VARIABLE_PTR:
    case PointerTestCases::OP_PTR_NOT_EQUAL_VARIABLE_PTR:
    {
        annotations += std::string("OpDecorate       %array_first_32         ArrayStride   ${alignment}\n"
                                   "${secondArrayDecoration:opt}\n"

                                   "OpMemberDecorate %input_buffer           0             Offset 0\n"
                                   "OpDecorate       %input_buffer           Block\n"
                                   "OpDecorate       %input_data_var         DescriptorSet 0\n"
                                   "OpDecorate       %input_data_var         Binding       0\n"

                                   "OpMemberDecorate %output_buffer          0             Offset 0\n"
                                   "OpDecorate       %output_buffer          Block\n"
                                   "OpDecorate       %output_data_var        DescriptorSet 0\n"
                                   "OpDecorate       %output_data_var        Binding       1\n");
        break;
    }
    case PointerTestCases::OP_PTR_DIFF_VARIABLE_PTR:
    {
        annotations += std::string("OpDecorate       %array_first_32           ArrayStride   ${alignment}\n"
                                   "${secondArrayDecoration:opt}\n"

                                   "OpMemberDecorate %input_buffer             0             Offset 0\n"
                                   "OpDecorate       %input_buffer             Block\n"
                                   "OpDecorate       %input_data_var           DescriptorSet 0\n"
                                   "OpDecorate       %input_data_var           Binding       0\n"

                                   "OpMemberDecorate %output_buffer            0             Offset 0\n"
                                   "OpDecorate       %output_buffer            Block\n"
                                   "OpDecorate       %output_data_var          DescriptorSet 0\n"
                                   "OpDecorate       %output_data_var          Binding       1\n");
        break;
    }
    case PointerTestCases::OP_PTR_ACCESS_CHAIN_VARIABLE_PTR:
    {
        annotations += std::string("OpDecorate       %array_${baseType}_${threadCount}       ArrayStride ${alignment}\n"
                                   "OpDecorate       %strided_storage_buffer_untyped_ptr     ArrayStride ${alignment}\n"

                                   "OpMemberDecorate %input_buffer             0             Offset 0\n"
                                   "OpDecorate       %input_buffer             Block\n"
                                   "OpDecorate       %input_data_untyped_var   DescriptorSet 0\n"
                                   "OpDecorate       %input_data_untyped_var   Binding       0\n"

                                   "OpMemberDecorate %output_buffer            0             Offset 0\n"
                                   "OpDecorate       %output_buffer            Block\n"
                                   "OpDecorate       %output_data_var          DescriptorSet 0\n"
                                   "OpDecorate       %output_data_var          Binding       1\n");
        break;
    }
    case PointerTestCases::MULTIPLE_ACCESS_CHAINS_VARIABLE_PTR:
    {
        annotations += std::string("OpDecorate       %array_first_32           ArrayStride   ${alignment0}\n"
                                   "OpDecorate       %array_second_32          ArrayStride   ${alignment1}\n"

                                   "OpMemberDecorate %input_buffer             0             Offset 0\n"
                                   "OpDecorate       %input_buffer             Block\n"
                                   "OpDecorate       %input_data_var           DescriptorSet 0\n"
                                   "OpDecorate       %input_data_var           Binding       0\n"

                                   "OpMemberDecorate %output_buffer            0             Offset 0\n"
                                   "OpDecorate       %output_buffer            Block\n"
                                   "OpDecorate       %output_data_var          DescriptorSet 0\n"
                                   "OpDecorate       %output_data_var          Binding       1\n");
        break;
    }
    case PointerTestCases::OP_FUNCTION_CALL_VARIABLE_PTR:
    {
        annotations += std::string("OpDecorate       %array_32               ArrayStride   ${alignment}\n"

                                   "OpMemberDecorate %input_buffer           0             Offset 0\n"
                                   "OpDecorate       %input_buffer           Block\n"
                                   "OpDecorate       %input_data_untyped_var DescriptorSet 0\n"
                                   "OpDecorate       %input_data_untyped_var Binding       0\n"

                                   "OpMemberDecorate %output_buffer          0             Offset 0\n"
                                   "OpDecorate       %output_buffer          Block\n"
                                   "OpDecorate       %output_data_var        DescriptorSet 0\n"
                                   "OpDecorate       %output_data_var        Binding       1\n");
        break;
    }
    case PointerTestCases::WORKGROUP_MEMORY_VARIABLE_PTR:
    {
        annotations += std::string("OpDecorate       %array_base               ArrayStride   ${alignment}\n"

                                   "OpMemberDecorate %input_buffer             0             Offset 0\n"
                                   "OpDecorate       %input_buffer             Block\n"
                                   "OpDecorate       %input_data_var           DescriptorSet 0\n"
                                   "OpDecorate       %input_data_var           Binding       0\n"

                                   "OpMemberDecorate %output_buffer            0             Offset 0\n"
                                   "OpDecorate       %output_buffer            Block\n"
                                   "OpDecorate       %output_data_var          DescriptorSet 0\n"
                                   "OpDecorate       %output_data_var          Binding       1\n"

                                   "OpMemberDecorate %shared_buffer             0             Offset 0\n"
                                   "OpDecorate       %shared_buffer             Block\n");
        break;
    }
    default:
    {
        DE_ASSERT(0);
        DE_FATAL("Unknown test case.");
        break;
    }
    }

    return annotations;
}

std::string createShaderAnnotations(MEMORY_INTERPRETATION_TEST_CASE testCase, bool /* read */)
{
    std::string annotations = std::string("OpDecorate %id BuiltIn GlobalInvocationId\n"
                                          "OpDecorate %array ArrayStride 4\n"
                                          "OpDecorate %block Block\n"
                                          "OpMemberDecorate %block 0 Offset 0\n"

                                          "OpDecorate %in_var DescriptorSet 0\n"
                                          "OpDecorate %in_var Binding 0\n"
                                          "OpDecorate %indices_var DescriptorSet 0\n"
                                          "OpDecorate %indices_var Binding 1\n"
                                          "OpDecorate %out_var DescriptorSet 0\n"
                                          "OpDecorate %out_var Binding 2\n");

    switch (testCase)
    {
    case MemoryInterpretationTestCases::LARGE_ARRAY_STRIDE:
    {
        annotations += std::string("OpDecorate %large_array ArrayStride 32\n");

        break;
    }
    case MemoryInterpretationTestCases::NON_ZERO_OFFSET:
    {
        annotations += std::string("OpMemberDecorate %test_struct 0 Offset 16\n"
                                   "OpMemberDecorate %test_struct 1 Offset 20\n"
                                   "OpDecorate %test_array ArrayStride 24\n");

        break;
    }
    case MemoryInterpretationTestCases::MIXED_OFFSETS:
    {
        annotations += std::string("OpMemberDecorate %test_struct 0 Offset 64\n"
                                   "OpMemberDecorate %test_struct 1 Offset 8\n"
                                   "OpMemberDecorate %test_struct 2 Offset 48\n"
                                   "OpMemberDecorate %test_struct 3 Offset 0\n");

        break;
    }
    case MemoryInterpretationTestCases::MULTIPLE_ACCESS_CHAINS:
    {
        annotations += std::string("OpDecorate %type_1 ArrayStride 8\n"
                                   "OpDecorate %type_2 Block\n"
                                   "OpMemberDecorate %type_2 0 Offset 0\n"
                                   "OpMemberDecorate %type_2 1 Offset 12\n"
                                   "OpDecorate %type_2_array ArrayStride 4\n"
                                   "OpDecorate %type_3 ArrayStride 12\n");

        break;
    }
    case MemoryInterpretationTestCases::SHORT2_NO_STORAGE_CAP:
    case MemoryInterpretationTestCases::CHAR4_NO_STORAGE_CAP:
    {
        break;
    }
    case MemoryInterpretationTestCases::CHAR2_16BIT_STORAGE_CAP:
    {
        annotations += std::string("OpDecorate %out_array ArrayStride 4\n"
                                   "OpDecorate %uchar2_array ArrayStride 2\n"
                                   "OpDecorate %out_block Block\n"
                                   "OpMemberDecorate %out_block 0 Offset 0\n");

        break;
    }
    case MemoryInterpretationTestCases::UNTYPED_FROM_TYPED_VAR:
    case MemoryInterpretationTestCases::UNTYPED_FROM_TYPED_ACCESS_CHAIN:
    {
        break;
    }
    default:
    {
        DE_ASSERT(0);
        DE_FATAL("Unknown test case.");
        break;
    }
    }

    return annotations;
}

std::string createShaderAnnotations(BLOCK_ARRAY_TEST_CASE /* testCase */)
{
    std::string annotations = std::string("OpDecorate %id BuiltIn GlobalInvocationId\n"
                                          "OpDecorate       %uni_var       DescriptorSet 0\n"
                                          "OpDecorate       %uni_var       Binding       0\n"
                                          "OpDecorate       %in_var        DescriptorSet 0\n"
                                          "OpDecorate       %in_var        Binding       1\n"
                                          "OpDecorate       %out_var       DescriptorSet 0\n"
                                          "OpDecorate       %out_var       Binding       2\n"
                                          "OpDecorate       %in_block_0    Block\n"
                                          "OpMemberDecorate %in_block_0    0             Offset 0\n"
                                          "OpDecorate       %in_block_1    Block\n"
                                          "OpMemberDecorate %in_block_1    0             Offset 0\n"
                                          "OpDecorate       %out_block     Block\n"
                                          "OpMemberDecorate %out_block     0             Offset 0\n"
                                          "OpDecorate       %uni_block     Block\n"
                                          "OpMemberDecorate %uni_block     0             Offset 0\n"
                                          "OpDecorate       %float_array   ArrayStride   4\n"
                                          "OpDecorate       %int_array     ArrayStride   4\n"
                                          "OpDecorate       %int4_array    ArrayStride   16\n"
                                          "OpDecorate       %uni_array     ArrayStride   4\n"
                                          "OpDecorate       %ptr_4_stride  ArrayStride   4\n"
                                          "OpDecorate       %ptr_16_stride ArrayStride   16\n");

    return annotations;
}

std::string createShaderAnnotations(WORKGROUP_TEST_CASE testCase)
{
    std::string annotations = std::string("OpDecorate %id BuiltIn GlobalInvocationId\n"

                                          "OpMemberDecorate %input_buffer_0    0             Offset 0\n"
                                          "OpMemberDecorate %input_buffer_0    1             Offset ${vecOffset}\n"
                                          "OpDecorate       %input_buffer_0    Block\n"
                                          "OpDecorate       %input_data_0_var  DescriptorSet 0\n"
                                          "OpDecorate       %input_data_0_var  Binding       0\n"

                                          "OpMemberDecorate %input_buffer_1    0             Offset 0\n"
                                          "OpMemberDecorate %input_buffer_1    1             Offset ${vecOffset}\n"
                                          "OpDecorate       %input_buffer_1    Block\n"
                                          "OpDecorate       %input_data_1_var  DescriptorSet 0\n"
                                          "OpDecorate       %input_data_1_var  Binding       1\n"

                                          "OpMemberDecorate %output_buffer_0   0             Offset 0\n"
                                          "OpMemberDecorate %output_buffer_0   1             Offset ${vecOffset}\n"
                                          "OpDecorate       %output_buffer_0   Block\n"
                                          "OpDecorate       %output_data_0_var DescriptorSet 0\n"
                                          "OpDecorate       %output_data_0_var Binding       2\n"

                                          "OpMemberDecorate %output_buffer_1   0             Offset 0\n"
                                          "OpMemberDecorate %output_buffer_1   1             Offset ${vecOffset}\n"
                                          "OpDecorate       %output_buffer_1   Block\n"
                                          "OpDecorate       %output_data_1_var DescriptorSet 0\n"
                                          "OpDecorate       %output_data_1_var Binding       3\n"

                                          "OpMemberDecorate %data_buffer       0             Offset 0\n"
                                          "OpMemberDecorate %data_buffer       1             Offset ${vecOffset}\n"
                                          "OpDecorate       %data_buffer       Block\n");

    switch (testCase)
    {
    case WorkgroupTestCases::ALIASED:
    {
        annotations += std::string(

            "OpDecorate       %data_buffer_0_untyped_var     Aliased\n"
            "OpDecorate       %data_buffer_1_untyped_var     Aliased\n");
        break;
    }
    case WorkgroupTestCases::NOT_ALIASED:
    {
        break;
    }
    default:
    {
        DE_ASSERT(0);
        DE_FATAL("Unknown test case.");
        break;
    }
    }

    return annotations;
}

std::string createShaderAnnotations(COOPERATIVE_MATRIX_TEST_CASE testCase)
{
    std::string annotations = std::string("OpDecorate %id BuiltIn GlobalInvocationId\n");

    switch (testCase)
    {
    case CooperativeMatrixTestCases::BASIC_LOAD:
    {
        annotations += std::string("OpDecorate       %${baseType}_rta        ArrayStride   ${typeSize}\n"

                                   "OpMemberDecorate %input_buffer           0             Offset 0\n"
                                   "OpDecorate       %input_buffer           Block\n"
                                   "OpDecorate       %input_data_untyped_var DescriptorSet 0\n"
                                   "OpDecorate       %input_data_untyped_var Binding       0\n"

                                   "OpMemberDecorate %output_buffer          0             Offset 0\n"
                                   "OpDecorate       %output_buffer          Block\n"
                                   "OpDecorate       %output_data_var        DescriptorSet 0\n"
                                   "OpDecorate       %output_data_var        Binding       1\n"

                                   "OpDecorate       %rows                   SpecId        0\n"
                                   "OpDecorate       %cols                   SpecId        1\n");
        break;
    }
    case CooperativeMatrixTestCases::BASIC_STORE:
    {
        annotations += std::string("OpDecorate       %${baseType}_rta         ArrayStride   ${typeSize}\n"

                                   "OpMemberDecorate %input_buffer            0             Offset 0\n"
                                   "OpDecorate       %input_buffer            Block\n"
                                   "OpDecorate       %input_data_var          DescriptorSet 0\n"
                                   "OpDecorate       %input_data_var          Binding       0\n"

                                   "OpMemberDecorate %output_buffer           0             Offset 0\n"
                                   "OpDecorate       %output_buffer           Block\n"
                                   "OpDecorate       %output_data_untyped_var DescriptorSet 0\n"
                                   "OpDecorate       %output_data_untyped_var Binding       1\n"

                                   "OpDecorate       %rows                   SpecId        0\n"
                                   "OpDecorate       %cols                   SpecId        1\n");
        break;
    }
    case CooperativeMatrixTestCases::TYPE_PUNNING_LOAD:
    {
        annotations += std::string("OpDecorate       %${baseType}_rta        ArrayStride   ${typeSize}\n"
                                   "OpDecorate       %${sameSizeType}_rta    ArrayStride   ${typeSize}\n"

                                   "OpMemberDecorate %input_buffer           0             Offset 0\n"
                                   "OpDecorate       %input_buffer           Block\n"
                                   "OpDecorate       %input_data_untyped_var DescriptorSet 0\n"
                                   "OpDecorate       %input_data_untyped_var Binding       0\n"

                                   "OpMemberDecorate %output_buffer          0             Offset 0\n"
                                   "OpDecorate       %output_buffer          Block\n"
                                   "OpDecorate       %output_data_var        DescriptorSet 0\n"
                                   "OpDecorate       %output_data_var        Binding       1\n"

                                   "OpDecorate       %rows                   SpecId        0\n"
                                   "OpDecorate       %cols                   SpecId        1\n");
        break;
    }
    case CooperativeMatrixTestCases::TYPE_PUNNING_STORE:
    {
        annotations += std::string("OpDecorate       %${baseType}_rta         ArrayStride   ${typeSize}\n"
                                   "OpDecorate       %${sameSizeType}_rta     ArrayStride   ${typeSize}\n"

                                   "OpMemberDecorate %input_buffer            0             Offset 0\n"
                                   "OpDecorate       %input_buffer            Block\n"
                                   "OpDecorate       %input_data_var          DescriptorSet 0\n"
                                   "OpDecorate       %input_data_var          Binding       0\n"

                                   "OpMemberDecorate %output_buffer           0             Offset 0\n"
                                   "OpDecorate       %output_buffer           Block\n"
                                   "OpDecorate       %output_data_untyped_var DescriptorSet 0\n"
                                   "OpDecorate       %output_data_untyped_var Binding       1\n"

                                   "OpDecorate       %rows                   SpecId        0\n"
                                   "OpDecorate       %cols                   SpecId        1\n");
        break;
    }
    case CooperativeMatrixTestCases::MIXED_LOAD:
    {
        annotations += std::string("OpDecorate       %${baseType}_rta        ArrayStride   ${typeSize}\n"

                                   "OpMemberDecorate %input_buffer           0             Offset 0\n"
                                   "OpDecorate       %input_buffer           Block\n"
                                   "OpDecorate       %input_data_untyped_var DescriptorSet 0\n"
                                   "OpDecorate       %input_data_untyped_var Binding       0\n"

                                   "OpMemberDecorate %output_buffer          0             Offset 0\n"
                                   "OpDecorate       %output_buffer          Block\n"
                                   "OpDecorate       %output_data_var        DescriptorSet 0\n"
                                   "OpDecorate       %output_data_var        Binding       1\n"

                                   "OpDecorate       %rows                   SpecId        0\n"
                                   "OpDecorate       %cols                   SpecId        1\n");
        break;
    }
    case CooperativeMatrixTestCases::MIXED_STORE:
    {
        annotations += std::string("OpDecorate       %${baseType}_rta         ArrayStride   ${typeSize}\n"

                                   "OpMemberDecorate %input_buffer            0             Offset 0\n"
                                   "OpDecorate       %input_buffer            Block\n"
                                   "OpDecorate       %input_data_var          DescriptorSet 0\n"
                                   "OpDecorate       %input_data_var          Binding       0\n"

                                   "OpMemberDecorate %output_buffer           0             Offset 0\n"
                                   "OpDecorate       %output_buffer           Block\n"
                                   "OpDecorate       %output_data_untyped_var DescriptorSet 0\n"
                                   "OpDecorate       %output_data_untyped_var Binding       1\n"

                                   "OpDecorate       %rows                   SpecId        0\n"
                                   "OpDecorate       %cols                   SpecId        1\n");
        break;
    }
    default:
    {
        DE_ASSERT(0);
        DE_FATAL("Unknown test case.");
        break;
    }
    }

    return annotations;
}

std::string createShaderVariables(BASE_TEST_CASE testCase)
{
    std::string variables("");

    switch (testCase)
    {
    case BaseTestCases::DESCRIPTOR_ARRAY:
    {
        variables += std::string(
            /* Base types */
            "%void             = OpTypeVoid\n"
            "%${baseType}      = ${baseDecl}\n"
            "%vec3_uint32      = OpTypeVector %uint32      3\n"

            /* Function types*/
            "%void_func   = OpTypeFunction %void\n"

            /* Constants */
            "%c_uint32_0      = OpConstant %uint32      0\n"
            "%c_uint32_16     = OpConstant %uint32      16\n"
            "%c_uint32_64     = OpConstant %uint32      64\n"

            /* Structs */
            "%block_data      = OpTypeStruct %${baseType} %${baseType} %${baseType} %${baseType}\n"
            "%data            = OpTypeStruct %${baseType} %${baseType} %${baseType} %${baseType}\n"

            /* Arrays */
            "%array_of_blocks  = OpTypeArray %block_data %c_uint32_16\n"
            "%array            = OpTypeArray %data       %c_uint32_16\n"

            /* Structs */
            "%output_buffer   = OpTypeStruct %array\n"

            /* Pointers */
            "%uint32_input_ptr                 = OpTypePointer           Input         %uint32\n"
            "%vec3_uint32_input_ptr            = OpTypePointer           Input         %vec3_uint32\n"
            "%${baseType}_storage_buffer_ptr   = OpTypePointer           StorageBuffer %${baseType}\n"
            "%storage_buffer_untyped_ptr       = OpTypeUntypedPointerKHR StorageBuffer\n"
            "%output_buffer_storage_buffer_ptr = OpTypePointer           StorageBuffer %output_buffer\n"
            "%data_storage_buffer_ptr          = OpTypePointer           StorageBuffer %data\n"

            /* Objects */
            "%id                                 = OpVariable              %vec3_uint32_input_ptr            Input\n"
            "%input_data_untyped_var             = OpUntypedVariableKHR    %storage_buffer_untyped_ptr       "
            "StorageBuffer %array_of_blocks\n"
            "%output_data_var                    = OpVariable              %output_buffer_storage_buffer_ptr "
            "StorageBuffer\n");

        break;
    }
    case BaseTestCases::ARRAY_LENGTH:
    {
        variables += std::string(
            /* Base types */
            "%void        = OpTypeVoid\n"
            "%${baseType} = ${baseDecl}\n"
            "%bool        = OpTypeBool\n"
            "%vec3_uint32 = OpTypeVector %uint32 3\n"

            /* Function types*/
            "%void_func   = OpTypeFunction %void\n"

            /* Constants */
            "%c_uint32_0      = OpConstant %uint32      0\n"
            "%c_${baseType}_1 = OpConstant %${baseType} 1\n"

            /* Arrays */
            "%${baseType}_rta = OpTypeRuntimeArray %${baseType}\n"

            /* Structs */
            "%input_buffer    = OpTypeStruct %${baseType}_rta\n"
            "%output_buffer   = OpTypeStruct %uint32 \n"

            /* Pointers */
            "%uint32_input_ptr                   = OpTypePointer           Input         %uint32\n"
            "%vec3_uint32_input_ptr              = OpTypePointer           Input         %vec3_uint32\n"
            "%${baseType}_storage_buffer_ptr     = OpTypePointer           StorageBuffer %${baseType}\n"
            "%storage_buffer_untyped_ptr         = OpTypeUntypedPointerKHR StorageBuffer\n"
            "%input_buffer_storage_buffer_ptr    = OpTypePointer           StorageBuffer %input_buffer\n"
            "%output_buffer_storage_buffer_ptr   = OpTypePointer           StorageBuffer %output_buffer\n"
            "%uint32_function_ptr                = OpTypePointer           Function      %uint32\n"

            /* Objects */
            "%id                                 = OpVariable              %vec3_uint32_input_ptr            Input\n"
            "%input_data_untyped_var             = OpUntypedVariableKHR    %storage_buffer_untyped_ptr       "
            "StorageBuffer %input_buffer\n"
            "%output_data_var                    = OpVariable              %output_buffer_storage_buffer_ptr "
            "StorageBuffer\n");

        break;
    }
    case BaseTestCases::COPY_FROM:
    {
        variables += std::string(
            /* Base types */
            "%void        = OpTypeVoid\n"
            "%${baseType} = ${baseDecl}\n"
            "%vec3_uint32 = OpTypeVector %uint32 3\n"

            /* Function types*/
            "%void_func = OpTypeFunction %void\n"

            /* Constants */
            "%c_uint32_0              = OpConstant %uint32 0\n"
            "%c_uint32_data_size      = OpConstant %uint32 ${size}\n"
            "%c_uint32_${threadCount} = OpConstant %uint32 ${threadCount}\n"

            /* Arrays */
            "%array_${baseType}_${threadCount} = OpTypeArray %${baseType} %c_uint32_${threadCount}\n"

            /* Structs */
            "%input_buffer  = OpTypeStruct %array_${baseType}_${threadCount}\n"
            "%output_buffer = OpTypeStruct %array_${baseType}_${threadCount}\n"

            /* Pointers */
            "%uint32_input_ptr                 = OpTypePointer           Input         %uint32\n"
            "%vec3_uint32_input_ptr            = OpTypePointer           Input         %vec3_uint32\n"
            "%storage_buffer_${baseType}_ptr   = OpTypePointer           StorageBuffer %${baseType}\n"
            "%storage_buffer_untyped_ptr       = OpTypeUntypedPointerKHR StorageBuffer\n"
            "%output_buffer_storage_buffer_ptr = OpTypePointer           StorageBuffer %output_buffer\n"

            /* Objects */
            "%id                               = OpVariable              %vec3_uint32_input_ptr             Input\n"
            "%input_data_untyped_var           = OpUntypedVariableKHR    %storage_buffer_untyped_ptr        "
            "StorageBuffer %input_buffer\n"
            "%output_data_var                  = OpVariable              %output_buffer_storage_buffer_ptr  "
            "StorageBuffer\n");
        break;
    }
    case BaseTestCases::LOAD:
    {
        variables += std::string(
            /* Base types */
            "%void        = OpTypeVoid\n"
            "%${baseType} = ${baseDecl}\n"
            "%vec3_uint32 = OpTypeVector %uint32 3\n"

            /* Function types*/
            "%void_func = OpTypeFunction %void\n"

            /* Constants */
            "%c_uint32_0              = OpConstant %uint32 0\n"
            "%c_uint32_${threadCount} = OpConstant %uint32 ${threadCount}\n"

            /* Arrays */
            "%array_${baseType}_${threadCount} = OpTypeArray %${baseType} %c_uint32_${threadCount}\n"

            /* Structs */
            "%input_buffer  = OpTypeStruct %array_${baseType}_${threadCount}\n"
            "%output_buffer = OpTypeStruct %array_${baseType}_${threadCount}\n"

            /* Pointers */
            "%uint32_input_ptr                 = OpTypePointer           Input         %uint32\n"
            "%vec3_uint32_input_ptr            = OpTypePointer           Input         %vec3_uint32\n"
            "%storage_buffer_${baseType}_ptr   = OpTypePointer           StorageBuffer %${baseType}\n"
            "%storage_buffer_untyped_ptr       = OpTypeUntypedPointerKHR ${storageClass}\n"
            "%output_buffer_storage_buffer_ptr = OpTypePointer           StorageBuffer %output_buffer\n"

            /* Objects */
            "%id                               = OpVariable              %vec3_uint32_input_ptr            Input\n"
            "%input_data_untyped_var           = OpUntypedVariableKHR    %storage_buffer_untyped_ptr       "
            "${storageClass} %input_buffer\n"
            "%output_data_var                  = OpVariable              %output_buffer_storage_buffer_ptr "
            "StorageBuffer\n");
        break;
    }
    case BaseTestCases::COPY_TO:
    {
        variables += std::string(
            /* Base types */
            "%void        = OpTypeVoid\n"
            "%${baseType} = ${baseDecl}\n"
            "%vec3_uint32 = OpTypeVector %uint32 3\n"

            /* Function types*/
            "%void_func = OpTypeFunction %void\n"

            /* Constants */
            "%c_uint32_0              = OpConstant %uint32 0\n"
            "%c_uint32_${threadCount} = OpConstant %uint32 ${threadCount}\n"
            "%c_uint32_data_size      = OpConstant %uint32 ${size}\n"

            /* Arrays */
            "%array_${baseType}_${threadCount} = OpTypeArray %${baseType} %c_uint32_${threadCount}\n"

            /* Structs */
            "%input_buffer  = OpTypeStruct %array_${baseType}_${threadCount}\n"
            "%output_buffer = OpTypeStruct %array_${baseType}_${threadCount}\n"

            /* Pointers */
            "%uint32_input_ptr                = OpTypePointer           Input         %uint32\n"
            "%vec3_uint32_input_ptr           = OpTypePointer           Input         %vec3_uint32\n"
            "%storage_buffer_${baseType}_ptr  = OpTypePointer           StorageBuffer %${baseType}\n"
            "%storage_buffer_untyped_ptr      = OpTypeUntypedPointerKHR StorageBuffer\n"
            "%input_buffer_storage_buffer_ptr = OpTypePointer           StorageBuffer %input_buffer\n"

            /* Objects */
            "%id                              = OpVariable              %vec3_uint32_input_ptr           Input\n"
            "%input_data_var                  = OpVariable              %input_buffer_storage_buffer_ptr "
            "StorageBuffer\n"
            "%output_data_untyped_var         = OpUntypedVariableKHR    %storage_buffer_untyped_ptr      StorageBuffer "
            "%output_buffer\n");

        break;
    }
    case BaseTestCases::STORE:
    {
        variables += std::string(
            /* Base types */
            "%void        = OpTypeVoid\n"
            "%${baseType} = ${baseDecl}\n"
            "%vec3_uint32 = OpTypeVector %uint32 3\n"

            /* Function types*/
            "%void_func = OpTypeFunction %void\n"

            /* Constants */
            "%c_uint32_0              = OpConstant %uint32 0\n"
            "%c_uint32_${threadCount} = OpConstant %uint32 ${threadCount}\n"

            /* Arrays */
            "%array_${baseType}_${threadCount} = OpTypeArray %${baseType} %c_uint32_${threadCount}\n"

            /* Structs */
            "%input_buffer  = OpTypeStruct %array_${baseType}_${threadCount}\n"
            "%output_buffer = OpTypeStruct %array_${baseType}_${threadCount}\n"

            /* Pointers */
            "%uint32_input_ptr                = OpTypePointer           Input         %uint32\n"
            "%vec3_uint32_input_ptr           = OpTypePointer           Input         %vec3_uint32\n"
            "%storage_buffer_${baseType}_ptr  = OpTypePointer           StorageBuffer %${baseType}\n"
            "%storage_buffer_untyped_ptr      = OpTypeUntypedPointerKHR StorageBuffer\n"
            "%input_buffer_storage_buffer_ptr = OpTypePointer           StorageBuffer %input_buffer\n"

            /* Objects */
            "%id                              = OpVariable              %vec3_uint32_input_ptr           Input\n"
            "%input_data_var                  = OpVariable              %input_buffer_storage_buffer_ptr "
            "StorageBuffer\n"
            "%output_data_untyped_var         = OpUntypedVariableKHR    %storage_buffer_untyped_ptr      StorageBuffer "
            "%output_buffer\n");
        break;
    }
    default:
    {
        DE_ASSERT(0);
        DE_FATAL("Unknown test case.");
        break;
    }
    }

    return variables;
}

std::string createShaderVariables(ATOMIC_TEST_CASE testCase)
{
    std::string variables = std::string(
        /* Base types */
        "%void                = OpTypeVoid\n"
        "%${baseType}         = ${baseDecl}\n");

    switch (testCase)
    {
    case AtomicTestCases::OP_ATOMIC_INCREMENT:
    case AtomicTestCases::OP_ATOMIC_DECREMENT:
    {
        variables += std::string(
            /* Base types */
            "%vec3_uint32 = OpTypeVector %uint32 3\n"

            /* Function types*/
            "%void_func = OpTypeFunction %void\n"

            /* Constants */
            "%c_base_0   = OpConstant %${baseType} 0\n"
            "%c_uint32_0 = OpConstant %uint32      0\n"
            "%c_uint32_1 = OpConstant %uint32      1\n"

            /* Structs */
            "%output_buffer = OpTypeStruct %${baseType}\n"

            /* Pointers */
            "%uint32_input_ptr                = OpTypePointer           Input         %uint32\n"
            "%vec3_uint32_input_ptr           = OpTypePointer           Input         %vec3_uint32\n"
            "%storage_buffer_untyped_ptr      = OpTypeUntypedPointerKHR StorageBuffer\n"

            /* Objects */
            "%id                              = OpVariable              %vec3_uint32_input_ptr      Input\n"
            "%output_data_untyped_var         = OpUntypedVariableKHR    %storage_buffer_untyped_ptr StorageBuffer "
            "%output_buffer\n");
        break;
    }
    case AtomicTestCases::OP_ATOMIC_ADD:
    case AtomicTestCases::OP_ATOMIC_SUB:
    case AtomicTestCases::OP_ATOMIC_MIN:
    case AtomicTestCases::OP_ATOMIC_MAX:
    case AtomicTestCases::OP_ATOMIC_AND:
    case AtomicTestCases::OP_ATOMIC_OR:
    case AtomicTestCases::OP_ATOMIC_XOR:
    case AtomicTestCases::OP_ATOMIC_EXCHANGE:
    {
        variables += std::string(
            /* Base types */
            "%vec3_uint32 = OpTypeVector %uint32 3\n"

            /* Function types*/
            "%void_func = OpTypeFunction %void\n"

            /* Constants */
            "%c_base_0        = OpConstant %${baseType} 0\n"
            "%c_uint32_0      = OpConstant %uint32      0\n"
            "%c_${baseType}_1 = OpConstant %${baseType} 1\n"
            "%op_value        = OpConstant %${baseType} ${opValue}\n"

            /* Structs */
            "%output_buffer = OpTypeStruct %${baseType}\n"

            /* Pointers */
            "%uint32_input_ptr                = OpTypePointer           Input         %uint32\n"
            "%vec3_uint32_input_ptr           = OpTypePointer           Input         %vec3_uint32\n"
            "%storage_buffer_untyped_ptr      = OpTypeUntypedPointerKHR StorageBuffer\n"

            /* Objects */
            "%id                              = OpVariable              %vec3_uint32_input_ptr      Input\n"
            "%output_data_untyped_var         = OpUntypedVariableKHR    %storage_buffer_untyped_ptr StorageBuffer "
            "%output_buffer\n");
        break;
    }
    case AtomicTestCases::OP_ATOMIC_COMPARE_EXCHANGE:
    {
        variables += std::string(
            /* Base types */
            "%vec3_uint32 = OpTypeVector %uint32 3\n"

            /* Function types*/
            "%void_func = OpTypeFunction %void\n"

            /* Constants */
            "%c_base_0        = OpConstant %${baseType} 0\n"
            "%c_uint32_0      = OpConstant %uint32      0\n"
            "%c_uint32_1      = OpConstant %uint32      1\n"
            "%op_value        = OpConstant %${baseType} ${opValue}\n"
            "%comp            = OpConstant %${baseType} ${compValue}\n"

            /* Structs */
            "%output_buffer = OpTypeStruct %${baseType}\n"

            /* Pointers */
            "%uint32_input_ptr                = OpTypePointer           Input         %uint32\n"
            "%vec3_uint32_input_ptr           = OpTypePointer           Input         %vec3_uint32\n"
            "%storage_buffer_untyped_ptr      = OpTypeUntypedPointerKHR StorageBuffer\n"

            /* Objects */
            "%id                              = OpVariable              %vec3_uint32_input_ptr      Input\n"
            "%output_data_untyped_var         = OpUntypedVariableKHR    %storage_buffer_untyped_ptr StorageBuffer "
            "%output_buffer\n");
        break;
    }
    case AtomicTestCases::OP_ATOMIC_LOAD:
    {
        variables += std::string(
            /* Base types */
            "%vec3_uint32 = OpTypeVector %uint32 3\n"

            /* Function types*/
            "%void_func = OpTypeFunction %void\n"

            /* Constants */
            "%c_uint32_0              = OpConstant %uint32 0\n"
            "%c_uint32_1              = OpConstant %uint32 1\n"
            "%c_uint32_${threadCount} = OpConstant %uint32 ${threadCount}\n"

            /* Arrays */
            "%array_${baseType}_${threadCount} = OpTypeArray %${baseType} %c_uint32_${threadCount}\n"

            /* Structs */
            "%input_buffer  = OpTypeStruct %array_${baseType}_${threadCount}\n"
            "%output_buffer = OpTypeStruct %array_${baseType}_${threadCount}\n"

            /* Pointers */
            "%uint32_input_ptr                 = OpTypePointer           Input         %uint32\n"
            "%vec3_uint32_input_ptr            = OpTypePointer           Input         %vec3_uint32\n"
            "%storage_buffer_${baseType}_ptr   = OpTypePointer           StorageBuffer %${baseType}\n"
            "%storage_buffer_untyped_ptr       = OpTypeUntypedPointerKHR StorageBuffer\n"
            "%output_buffer_storage_buffer_ptr = OpTypePointer           StorageBuffer %output_buffer\n"

            /* Objects */
            "%id                               = OpVariable              %vec3_uint32_input_ptr            Input\n"
            "%input_data_untyped_var           = OpUntypedVariableKHR    %storage_buffer_untyped_ptr       "
            "StorageBuffer %input_buffer\n"
            "%output_data_var                  = OpVariable              %output_buffer_storage_buffer_ptr "
            "StorageBuffer\n");

        break;
    }
    case AtomicTestCases::OP_ATOMIC_STORE:
    {
        variables += std::string(
            /* Base types */
            "%vec3_uint32 = OpTypeVector %uint32 3\n"

            /* Function types*/
            "%void_func = OpTypeFunction %void\n"

            /* Constants */
            "%c_uint32_0              = OpConstant %uint32 0\n"
            "%c_uint32_1              = OpConstant %uint32 1\n"
            "%c_uint32_${threadCount} = OpConstant %uint32 ${threadCount}\n"

            /* Arrays */
            "%array_${baseType}_${threadCount} = OpTypeArray %${baseType} %c_uint32_${threadCount}\n"

            /* Structs */
            "%input_buffer  = OpTypeStruct %array_${baseType}_${threadCount}\n"
            "%output_buffer = OpTypeStruct %array_${baseType}_${threadCount}\n"

            /* Pointers */
            "%uint32_input_ptr                = OpTypePointer           Input         %uint32\n"
            "%vec3_uint32_input_ptr           = OpTypePointer           Input         %vec3_uint32\n"
            "%storage_buffer_${baseType}_ptr  = OpTypePointer           StorageBuffer %${baseType}\n"
            "%storage_buffer_untyped_ptr      = OpTypeUntypedPointerKHR StorageBuffer\n"
            "%input_buffer_storage_buffer_ptr = OpTypePointer           StorageBuffer %input_buffer\n"

            /* Objects */
            "%id                              = OpVariable              %vec3_uint32_input_ptr           Input\n"
            "%input_data_var                  = OpVariable              %input_buffer_storage_buffer_ptr "
            "StorageBuffer\n"
            "%output_data_untyped_var         = OpUntypedVariableKHR    %storage_buffer_untyped_ptr      StorageBuffer "
            "%output_buffer\n");

        break;
    }
    default:
    {
        DE_ASSERT(0);
        DE_FATAL("Unknown test case.");
        break;
    }
    }

    return variables;
}

std::string createShaderVariables(TYPE_PUNNING_TEST_CASE testCase)
{
    std::string variables("");

    switch (testCase)
    {
    case TypePunningTestCases::COPY_FROM_SAME_SIZE_TYPES:
    {
        variables += std::string(
            /* Base types */
            "%void            = OpTypeVoid\n"
            "%${baseType}     = ${baseDecl}\n"
            "%${sameSizeType} = ${sameSizeDecl}\n"
            "%vec3_uint32     = OpTypeVector %uint32 3\n"

            /* Function types*/
            "%void_func = OpTypeFunction %void\n"

            /* Constants */
            "%c_uint32_0              = OpConstant %uint32 0\n"
            "%c_uint32_${threadCount} = OpConstant %uint32 ${threadCount}\n"
            "%c_uint32_data_size      = OpConstant %uint32 ${size}\n"

            /* Arrays */
            "%array_${baseType}_${threadCount}     = OpTypeArray %${baseType}     %c_uint32_${threadCount}\n"
            "%array_${sameSizeType}_${threadCount} = OpTypeArray %${sameSizeType} %c_uint32_${threadCount}\n"

            /* Structs */
            "%input_buffer  = OpTypeStruct %array_${baseType}_${threadCount}\n"
            "%output_buffer = OpTypeStruct %array_${sameSizeType}_${threadCount}\n"

            /* Pointers */
            "%uint32_input_ptr                   = OpTypePointer           Input         %uint32\n"
            "%vec3_uint32_input_ptr              = OpTypePointer           Input         %vec3_uint32\n"
            "%storage_buffer_${baseType}_ptr     = OpTypePointer           StorageBuffer %${baseType}\n"
            "%storage_buffer_${sameSizeType}_ptr = OpTypePointer           StorageBuffer %${sameSizeType}\n"
            "%storage_buffer_untyped_ptr         = OpTypeUntypedPointerKHR StorageBuffer\n"
            "%output_buffer_storage_buffer_ptr   = OpTypePointer           StorageBuffer %output_buffer\n"

            /* Objects */
            "%id                                 = OpVariable              %vec3_uint32_input_ptr            Input\n"
            "%input_data_untyped_var             = OpUntypedVariableKHR    %storage_buffer_untyped_ptr       "
            "StorageBuffer %input_buffer\n"
            "%output_data_var                    = OpVariable              %output_buffer_storage_buffer_ptr "
            "StorageBuffer\n");
        break;
    }
    case TypePunningTestCases::COPY_FROM_SCALAR_VECTOR:
    {
        variables += std::string(
            /* Base types */
            "%void         = OpTypeVoid\n"
            "%${baseType}  = ${baseDecl}\n"
            "%${otherType} = ${otherTypeDecl}\n"
            "%${otherVec}  = ${otherVecDecl}\n"

            "${inputVec:opt}\n"

            /* Function types*/
            "%void_func = OpTypeFunction %void\n"

            /* Constants */
            "%c_uint32_0         = OpConstant %uint32 0\n"
            "%c_uint32_data_size = OpConstant %uint32 ${size}\n"

            /* Structs */
            "%input_buffer  = OpTypeStruct %${baseType}\n"
            "%output_buffer = OpTypeStruct %${otherVec}\n"

            /* Pointers */
            "%uint32_input_ptr                 = OpTypePointer           Input         %uint32\n"
            "%vec3_uint32_input_ptr            = OpTypePointer           Input         %vec3_uint32\n"
            "%storage_buffer_untyped_ptr       = OpTypeUntypedPointerKHR StorageBuffer\n"
            "%storage_buffer_${otherVec}_ptr   = OpTypePointer           StorageBuffer %${otherVec}\n"
            "%storage_buffer_output_buffer_ptr = OpTypePointer           StorageBuffer %output_buffer\n"

            /* Objects */
            "%id                               = OpVariable              %vec3_uint32_input_ptr            Input\n"
            "%input_data_untyped_var           = OpUntypedVariableKHR    %storage_buffer_untyped_ptr       "
            "StorageBuffer %input_buffer\n"
            "%output_data_var                  = OpVariable              %storage_buffer_output_buffer_ptr "
            "StorageBuffer\n");
        break;
    }
    case TypePunningTestCases::LOAD_SCALAR_VECTOR:
    {
        variables += std::string(
            /* Base types */
            "%void         = OpTypeVoid\n"
            "%${baseType}  = ${baseDecl}\n"
            "%${otherType} = ${otherTypeDecl}\n"
            "%${otherVec}  = ${otherVecDecl}\n"

            "${inputVec:opt}\n"

            /* Function types*/
            "%void_func = OpTypeFunction %void\n"

            /* Constants */
            "%c_uint32_0    = OpConstant %uint32 0\n"

            /* Structs */
            "%input_buffer  = OpTypeStruct %${baseType} %${baseType}\n"
            "%output_buffer = OpTypeStruct %${otherVec}\n"

            /* Pointers */
            "%uint32_input_ptr                 = OpTypePointer           Input %uint32\n"
            "%vec3_uint32_input_ptr            = OpTypePointer           Input %vec3_uint32\n"
            "%storage_buffer_untyped_ptr       = OpTypeUntypedPointerKHR ${storageClass}\n"
            "%storage_buffer_${otherVec}_ptr   = OpTypePointer           StorageBuffer %${otherVec}\n"
            "%storage_buffer_output_buffer_ptr = OpTypePointer           StorageBuffer %output_buffer\n"

            /* Objects */
            "%id                               = OpVariable             %vec3_uint32_input_ptr            Input\n"
            "%input_data_untyped_var           = OpUntypedVariableKHR   %storage_buffer_untyped_ptr       "
            "${storageClass} %input_buffer\n"
            "%output_data_var                  = OpVariable             %storage_buffer_output_buffer_ptr "
            "StorageBuffer\n");
        break;
    }
    case TypePunningTestCases::COPY_FROM_VECTOR_SCALAR:
    {
        variables += std::string(
            /* Base types */
            "%void         = OpTypeVoid\n"
            "%${baseType}  = ${baseDecl}\n"
            "%${otherType} = ${otherTypeDecl}\n"
            "%${baseVec}   = ${baseVecDecl}\n"

            "${inputVec:opt}\n"

            /* Function types*/
            "%void_func    = OpTypeFunction %void\n"

            /* Constants */
            "%c_uint32_0         = OpConstant %uint32 0\n"
            "%c_uint32_data_size = OpConstant %uint32 ${size}\n"

            /* Structs */
            "%input_buffer  = OpTypeStruct %${baseVec}\n"
            "%output_buffer = OpTypeStruct %${otherType}\n"

            /* Pointers */
            "%uint32_input_ptr                 = OpTypePointer           Input         %uint32\n"
            "%vec3_uint32_input_ptr            = OpTypePointer           Input         %vec3_uint32\n"
            "%storage_buffer_untyped_ptr       = OpTypeUntypedPointerKHR StorageBuffer\n"
            "%storage_buffer_${otherType}_ptr  = OpTypePointer           StorageBuffer %${otherType}\n"
            "%storage_buffer_output_buffer_ptr = OpTypePointer           StorageBuffer %output_buffer\n"

            /* Objects */
            "%id                               = OpVariable              %vec3_uint32_input_ptr            Input\n"
            "%input_data_untyped_var           = OpUntypedVariableKHR    %storage_buffer_untyped_ptr       "
            "StorageBuffer %input_buffer\n"
            "%output_data_var                  = OpVariable              %storage_buffer_output_buffer_ptr "
            "StorageBuffer\n");
        break;
    }
    case TypePunningTestCases::LOAD_VECTOR_SCALAR:
    {
        variables += std::string(
            /* Base types */
            "%void         = OpTypeVoid\n"
            "%${baseType}  = ${baseDecl}\n"
            "%${otherType} = ${otherTypeDecl}\n"
            "%${baseVec}   = ${baseVecDecl}\n"

            "${inputVec:opt}\n"

            /* Function types*/
            "%void_func = OpTypeFunction %void\n"

            /* Constants */
            "%c_uint32_0    = OpConstant %uint32 0\n"

            /* Structs */
            "%input_buffer  = OpTypeStruct %${baseVec} %${baseVec}\n"
            "%output_buffer = OpTypeStruct %${otherType}\n"

            /* Pointers */
            "%uint32_input_ptr                 = OpTypePointer           Input         %uint32\n"
            "%vec3_uint32_input_ptr            = OpTypePointer           Input         %vec3_uint32\n"
            "%storage_buffer_untyped_ptr       = OpTypeUntypedPointerKHR ${storageClass}\n"
            "%storage_buffer_${otherType}_ptr  = OpTypePointer           StorageBuffer %${otherType}\n"
            "%storage_buffer_output_buffer_ptr = OpTypePointer           StorageBuffer %output_buffer\n"

            /* Objects */
            "%id                               = OpVariable              %vec3_uint32_input_ptr            Input\n"
            "%input_data_untyped_var           = OpUntypedVariableKHR    %storage_buffer_untyped_ptr       "
            "${storageClass} %input_buffer\n"
            "%output_data_var                  = OpVariable              %storage_buffer_output_buffer_ptr "
            "StorageBuffer\n");
        break;
    }
    case TypePunningTestCases::LOAD_SAME_SIZE_TYPES:
    {
        variables += std::string(
            /* Base types */
            "%void            = OpTypeVoid\n"
            "%${baseType}     = ${baseDecl}\n"
            "%${sameSizeType} = ${sameSizeDecl}\n"

            "%vec3_uint32 = OpTypeVector %uint32 3\n"

            /* Function types*/
            "%void_func = OpTypeFunction %void\n"

            /* Constants */
            "%c_uint32_0              = OpConstant %uint32 0\n"
            "%c_uint32_${threadCount} = OpConstant %uint32 ${threadCount}\n"

            /* Arrays */
            "%array_${baseType}_${threadCount}     = OpTypeArray %${baseType}     %c_uint32_${threadCount}\n"
            "%array_${sameSizeType}_${threadCount} = OpTypeArray %${sameSizeType} %c_uint32_${threadCount}\n"

            /* Structs */
            "%input_buffer  = OpTypeStruct %array_${baseType}_${threadCount}\n"
            "%output_buffer = OpTypeStruct %array_${sameSizeType}_${threadCount}\n"

            /* Pointers */
            "%uint32_input_ptr                   = OpTypePointer           Input         %uint32\n"
            "%vec3_uint32_input_ptr              = OpTypePointer           Input         %vec3_uint32\n"
            "%storage_buffer_${baseType}_ptr     = OpTypePointer           StorageBuffer %${baseType}\n"
            "%storage_buffer_${sameSizeType}_ptr = OpTypePointer           StorageBuffer %${sameSizeType}\n"
            "%storage_buffer_untyped_ptr         = OpTypeUntypedPointerKHR ${storageClass}\n"
            "%output_buffer_storage_buffer_ptr   = OpTypePointer           StorageBuffer %output_buffer\n"

            /* Objects */
            "%id                                 = OpVariable              %vec3_uint32_input_ptr            Input\n"
            "%input_data_untyped_var             = OpUntypedVariableKHR    %storage_buffer_untyped_ptr       "
            "${storageClass} %input_buffer\n"
            "%output_data_var                    = OpVariable              %output_buffer_storage_buffer_ptr "
            "StorageBuffer\n");
        break;
    }
    case TypePunningTestCases::COPY_TO_SAME_SIZE_TYPES:
    {
        variables += std::string(
            /* Base types */
            "%void            = OpTypeVoid\n"
            "%${baseType}     = ${baseDecl}\n"
            "%${sameSizeType} = ${sameSizeDecl}\n"

            "%vec3_uint32 = OpTypeVector %uint32 3\n"

            /* Function types*/
            "%void_func = OpTypeFunction %void\n"

            /* Constants */
            "%c_uint32_0              = OpConstant %uint32 0\n"
            "%c_uint32_data_size      = OpConstant %uint32 ${size}\n"
            "%c_uint32_${threadCount} = OpConstant %uint32 ${threadCount}\n"

            /* Arrays */
            "%array_${baseType}_${threadCount}     = OpTypeArray %${baseType} %c_uint32_${threadCount}\n"
            "%array_${sameSizeType}_${threadCount} = OpTypeArray %${sameSizeType} %c_uint32_${threadCount}\n"

            /* Structs */
            "%input_buffer  = OpTypeStruct %array_${baseType}_${threadCount}\n"
            "%output_buffer = OpTypeStruct %array_${sameSizeType}_${threadCount}\n"

            /* Pointers */
            "%uint32_input_ptr                   = OpTypePointer           Input         %uint32\n"
            "%vec3_uint32_input_ptr              = OpTypePointer           Input         %vec3_uint32\n"
            "%storage_buffer_${baseType}_ptr     = OpTypePointer           StorageBuffer %${baseType}\n"
            "%storage_buffer_${sameSizeType}_ptr = OpTypePointer           StorageBuffer %${sameSizeType}\n"
            "%storage_buffer_untyped_ptr         = OpTypeUntypedPointerKHR StorageBuffer\n"
            "%input_buffer_storage_buffer_ptr    = OpTypePointer           StorageBuffer %input_buffer\n"

            /* Objects */
            "%id                                 = OpVariable              %vec3_uint32_input_ptr           Input\n"
            "%input_data_var                     = OpVariable              %input_buffer_storage_buffer_ptr "
            "StorageBuffer\n"
            "%output_data_untyped_var            = OpUntypedVariableKHR    %storage_buffer_untyped_ptr      "
            "StorageBuffer %output_buffer\n");
        break;
    }
    case TypePunningTestCases::STORE_SAME_SIZE_TYPES:
    {
        variables += std::string(
            /* Base types */
            "%void            = OpTypeVoid\n"
            "%${baseType}     = ${baseDecl}\n"
            "%${sameSizeType} = ${sameSizeDecl}\n"

            "%vec3_uint32 = OpTypeVector %uint32 3\n"

            /* Function types*/
            "%void_func = OpTypeFunction %void\n"

            /* Constants */
            "%c_uint32_0              = OpConstant %uint32 0\n"
            "%c_uint32_${threadCount} = OpConstant %uint32 ${threadCount}\n"

            /* Arrays */
            "%array_${baseType}_${threadCount}     = OpTypeArray %${baseType}     %c_uint32_${threadCount}\n"
            "%array_${sameSizeType}_${threadCount} = OpTypeArray %${sameSizeType} %c_uint32_${threadCount}\n"

            /* Structs */
            "%input_buffer  = OpTypeStruct %array_${baseType}_${threadCount}\n"
            "%output_buffer = OpTypeStruct %array_${sameSizeType}_${threadCount}\n"

            /* Pointers */
            "%uint32_input_ptr                   = OpTypePointer           Input         %uint32\n"
            "%vec3_uint32_input_ptr              = OpTypePointer           Input         %vec3_uint32\n"
            "%storage_buffer_${baseType}_ptr     = OpTypePointer           StorageBuffer %${baseType}\n"
            "%storage_buffer_${sameSizeType}_ptr = OpTypePointer           StorageBuffer %${sameSizeType}\n"
            "%storage_buffer_untyped_ptr         = OpTypeUntypedPointerKHR StorageBuffer\n"
            "%input_buffer_storage_buffer_ptr    = OpTypePointer           StorageBuffer %input_buffer\n"

            /* Objects */
            "%id                                 = OpVariable              %vec3_uint32_input_ptr           Input\n"
            "%input_data_var                     = OpVariable              %input_buffer_storage_buffer_ptr "
            "StorageBuffer\n"
            "%output_data_untyped_var            = OpUntypedVariableKHR    %storage_buffer_untyped_ptr      "
            "StorageBuffer %output_buffer\n");
        break;
    }
    case TypePunningTestCases::COPY_TO_SCALAR_VECTOR:
    {
        variables += std::string(
            /* Base types */
            "%void         = OpTypeVoid\n"
            "%${baseType}  = ${baseDecl}\n"
            "%${otherType} = ${otherTypeDecl}\n"
            "%${otherVec}  = ${otherVecDecl}\n"
            "${inputVec:opt}\n"

            /* Function types*/
            "%void_func = OpTypeFunction %void\n"

            /* Constants */
            "%c_uint32_0         = OpConstant %uint32 0\n"
            "%c_uint32_data_size = OpConstant %uint32 ${size}\n"

            /* Structs */
            "%input_buffer  = OpTypeStruct %${baseType}\n"
            "%output_buffer = OpTypeStruct %${otherVec}\n"

            /* Pointers */
            "%uint32_input_ptr                = OpTypePointer           Input         %uint32\n"
            "%vec3_uint32_input_ptr           = OpTypePointer           Input         %vec3_uint32\n"
            "%storage_buffer_${baseType}_ptr  = OpTypePointer           StorageBuffer %${baseType}\n"
            "%storage_buffer_untyped_ptr      = OpTypeUntypedPointerKHR StorageBuffer\n"
            "%input_buffer_storage_buffer_ptr = OpTypePointer           StorageBuffer %input_buffer\n"

            /* Objects */
            "%id                              = OpVariable              %vec3_uint32_input_ptr           Input\n"
            "%input_data_var                  = OpVariable              %input_buffer_storage_buffer_ptr "
            "StorageBuffer\n"
            "%output_data_untyped_var         = OpUntypedVariableKHR    %storage_buffer_untyped_ptr      StorageBuffer "
            "%output_buffer\n");
        break;
    }
    case TypePunningTestCases::STORE_SCALAR_VECTOR:
    {
        variables += std::string(
            /* Base types */
            "%void         = OpTypeVoid\n"
            "%${baseType}  = ${baseDecl}\n"
            "%${otherType} = ${otherTypeDecl}\n"
            "%${otherVec}  = ${otherVecDecl}\n"
            "${inputVec:opt}\n"

            /* Function types*/
            "%void_func = OpTypeFunction %void\n"

            /* Constants */
            "%c_uint32_0 = OpConstant %uint32 0\n"

            /* Structs */
            "%input_buffer  = OpTypeStruct %${baseType}\n"
            "%output_buffer = OpTypeStruct %${otherVec}\n"

            /* Pointers */
            "%uint32_input_ptr                = OpTypePointer           Input         %uint32\n"
            "%vec3_uint32_input_ptr           = OpTypePointer           Input         %vec3_uint32\n"
            "%storage_buffer_${baseType}_ptr  = OpTypePointer           StorageBuffer %${baseType}\n"
            "%storage_buffer_untyped_ptr      = OpTypeUntypedPointerKHR StorageBuffer\n"
            "%input_buffer_storage_buffer_ptr = OpTypePointer           StorageBuffer %input_buffer\n"

            /* Objects */
            "%id                              = OpVariable              %vec3_uint32_input_ptr           Input\n"
            "%input_data_var                  = OpVariable              %input_buffer_storage_buffer_ptr "
            "StorageBuffer\n"
            "%output_data_untyped_var         = OpUntypedVariableKHR    %storage_buffer_untyped_ptr      StorageBuffer "
            "%output_buffer\n");
        break;
    }
    case TypePunningTestCases::COPY_TO_VECTOR_SCALAR:
    {
        variables += std::string(
            /* Base types */
            "%void         = OpTypeVoid\n"
            "%${baseType}  = ${baseDecl}\n"
            "%${otherType} = ${otherTypeDecl}\n"
            "%${baseVec}   = ${baseVecDecl}\n"
            "${inputVec:opt}\n"

            /* Function types*/
            "%void_func = OpTypeFunction %void\n"

            /* Constants */
            "%c_uint32_0         = OpConstant %uint32 0\n"
            "%c_uint32_data_size = OpConstant %uint32 ${size}\n"

            /* Structs */
            "%input_buffer  = OpTypeStruct %${baseVec}\n"
            "%output_buffer = OpTypeStruct %${otherType}\n"

            /* Pointers */
            "%uint32_input_ptr                = OpTypePointer           Input         %uint32\n"
            "%vec3_uint32_input_ptr           = OpTypePointer           Input         %vec3_uint32\n"
            "%storage_buffer_${baseVec}_ptr   = OpTypePointer           StorageBuffer %${baseVec}\n"
            "%storage_buffer_untyped_ptr      = OpTypeUntypedPointerKHR StorageBuffer\n"
            "%input_buffer_storage_buffer_ptr = OpTypePointer           StorageBuffer %input_buffer\n"

            /* Objects */
            "%id                              = OpVariable              %vec3_uint32_input_ptr           Input\n"
            "%input_data_var                  = OpVariable              %input_buffer_storage_buffer_ptr "
            "StorageBuffer\n"
            "%output_data_untyped_var         = OpUntypedVariableKHR    %storage_buffer_untyped_ptr      StorageBuffer "
            "%output_buffer\n");
        break;
    }
    case TypePunningTestCases::STORE_VECTOR_SCALAR:
    {
        variables += std::string(
            /* Base types */
            "%void         = OpTypeVoid\n"
            "%${baseType}  = ${baseDecl}\n"
            "%${otherType} = ${otherTypeDecl}\n"
            "%${baseVec}   = ${baseVecDecl}\n"
            "${inputVec:opt}\n"

            /* Function types*/
            "%void_func = OpTypeFunction %void\n"

            /* Constants */
            "%c_uint32_0 = OpConstant %uint32 0\n"

            /* Structs */
            "%input_buffer  = OpTypeStruct %${baseVec}\n"
            "%output_buffer = OpTypeStruct %${otherType}\n"

            /* Pointers */
            "%uint32_input_ptr                = OpTypePointer           Input         %uint32\n"
            "%vec3_uint32_input_ptr           = OpTypePointer           Input         %vec3_uint32\n"
            "%storage_buffer_${baseVec}_ptr   = OpTypePointer           StorageBuffer %${baseVec}\n"
            "%storage_buffer_untyped_ptr      = OpTypeUntypedPointerKHR StorageBuffer\n"
            "%input_buffer_storage_buffer_ptr = OpTypePointer           StorageBuffer %input_buffer\n"

            /* Objects */
            "%id                              = OpVariable              %vec3_uint32_input_ptr           Input\n"
            "%input_data_var                  = OpVariable              %input_buffer_storage_buffer_ptr "
            "StorageBuffer\n"
            "%output_data_untyped_var         = OpUntypedVariableKHR    %storage_buffer_untyped_ptr      StorageBuffer "
            "%output_buffer\n");
        break;
    }
    case TypePunningTestCases::MULTIPLE_ACCESS_CHAINS:
    {
        variables += std::string(
            /* Base types */
            "%void                  = OpTypeVoid\n"
            "%bool                  = OpTypeBool\n"
            "%${baseType}           = ${baseDecl}\n"
            "%vec2_${baseType}      = OpTypeVector %${baseType} 2\n"
            "%vec3_uint32           = OpTypeVector %uint32      3\n"

            /* Function types */
            "%void_func             = OpTypeFunction %void\n"

            /* Constants */
            "%c_uint32_0            = OpConstant %uint32 0\n"

            /* Pointers */
            "%uint32_input_ptr      = OpTypePointer Input %uint32\n"
            "%vec3_uint32_input_ptr = OpTypePointer Input %vec3_uint32\n"

            /* Struct */
            "%data_buffer                         = OpTypeStruct            %${baseType}      %${baseType}\n"
            "%input_buffer                        = OpTypeStruct            %vec2_${baseType}\n"
            "%output_buffer                       = OpTypeStruct            %data_buffer\n"

            /* Pointers */
            "%input_buffer_storage_buffer_ptr     = OpTypePointer           StorageBuffer     %input_buffer\n"
            "%output_buffer_storage_buffer_ptr    = OpTypePointer           StorageBuffer     %output_buffer\n"
            "%storage_buffer_untyped_ptr          = OpTypeUntypedPointerKHR StorageBuffer\n"

            /* Objects */
            "%input_data_untyped_var              = OpUntypedVariableKHR    %storage_buffer_untyped_ptr       "
            "StorageBuffer %input_buffer\n"
            "%output_data_var                     = OpVariable              %output_buffer_storage_buffer_ptr "
            "StorageBuffer\n"
            "%id                                  = OpVariable              %vec3_uint32_input_ptr            Input\n");
        break;
    }
    case TypePunningTestCases::CUSTOM_STRUCT_TYPE:
    {
        variables += std::string(
            /* Base types */
            "%void          = OpTypeVoid\n"
            "%bool          = OpTypeBool\n"
            "%uint32        = OpTypeInt   32 0\n"
            "%vec3_uint32   = OpTypeVector %uint32  3\n"
            "${baseTypes}\n"

            /* Function types */
            "%void_func     = OpTypeFunction %void\n"

            /* Structs */
            "%input_buffer  = OpTypeStruct ${inputLayout}\n"
            "%output_buffer = OpTypeStruct ${outputLayout}\n"

            /* Pointers */
            "%uint32_input_ptr                 = OpTypePointer           Input                            %uint32\n"
            "%vec3_uint32_input_ptr            = OpTypePointer           Input                            "
            "%vec3_uint32\n"
            "%input_buffer_storage_buffer_ptr  = OpTypePointer           StorageBuffer                    "
            "%input_buffer\n"
            "%output_buffer_storage_buffer_ptr = OpTypePointer           StorageBuffer                    "
            "%output_buffer\n"
            "%storage_buffer_untyped_ptr       = OpTypeUntypedPointerKHR StorageBuffer\n"

            /* Objects */
            "%output_data_var                  = OpVariable              %output_buffer_storage_buffer_ptr "
            "StorageBuffer\n"
            "%input_data_untyped_var           = OpUntypedVariableKHR    %storage_buffer_untyped_ptr       "
            "StorageBuffer %input_buffer\n"
            "%id                               = OpVariable              %vec3_uint32_input_ptr            Input\n");

        break;
    }
    default:
    {
        DE_ASSERT(0);
        DE_FATAL("Unknown test case.");
        break;
    }
    }

    return variables;
}

std::string createShaderVariables(POINTER_TEST_CASE testCase)
{
    std::string variables = std::string(
        /* Base types */
        "%void         = OpTypeVoid\n"
        "%bool         = OpTypeBool\n"
        "%${baseType}  = ${baseDecl}\n"
        "${secondType:opt}\n"
        "%vec3_uint32  = OpTypeVector %uint32 3\n"

        /* Function types */
        "%void_func   = OpTypeFunction %void\n"

        /* Constants */
        "%c_uint32_0   = OpConstant %uint32 0\n"
        "%c_uint32_1   = OpConstant %uint32 1\n"
        "%c_uint32_2   = OpConstant %uint32 2\n"
        "%c_uint32_4   = OpConstant %uint32 4\n"
        "%c_uint32_8   = OpConstant %uint32 8\n"
        "%c_uint32_16  = OpConstant %uint32 16\n"
        "%c_uint32_32  = OpConstant %uint32 32\n"
        "%c_uint32_64  = OpConstant %uint32 64\n"
        "%c_uint32_264 = OpConstant %uint32 264\n"
        "${boolConst:opt}\n"

        /* Pointers */
        "%uint32_input_ptr      = OpTypePointer Input %uint32\n"
        "%vec3_uint32_input_ptr = OpTypePointer Input %vec3_uint32\n");

    switch (testCase)
    {
    case PointerTestCases::OP_PHI_PHYSICAL_STORAGE:
    case PointerTestCases::OP_SELECT_PHYSICAL_STORAGE:
    {
        variables += std::string(
            /* Structs */
            "%push_constant = OpTypeStruct %uint32\n"
            "%data_buffer   = OpTypeStruct %${baseType}\n"

            /* Pointers */
            "%push_constant_ptr                = OpTypePointer           PushConstant          %push_constant\n"
            "%uint32_push_constant_ptr         = OpTypePointer           PushConstant          %uint32\n"
            "%untyped_phys_ptr                 = OpTypeUntypedPointerKHR PhysicalStorageBuffer\n"
            "%data_buffer_phys_ptr             = OpTypePointer           PhysicalStorageBuffer %data_buffer\n"
            "%data_buffer_phys_ptr_ptr         = OpTypePointer           StorageBuffer         %data_buffer_phys_ptr\n"

            /* Structs cd. */
            "%phys_ptrs_struct = OpTypeStruct %data_buffer_phys_ptr %data_buffer_phys_ptr %data_buffer_phys_ptr\n"

            /* Pointers cd. */
            "%phys_ptrs_struct_ptr = OpTypePointer StorageBuffer %phys_ptrs_struct\n"

            /* Variables */
            "%push_constant_var                = OpVariable              %push_constant_ptr                "
            "PushConstant\n"
            "%all_data_var = OpVariable %phys_ptrs_struct_ptr  StorageBuffer\n");

        break;
    }
    case PointerTestCases::OP_BITCAST_FROM_UNTYPED_PHYSICAL_STORAGE:
    {
        variables += std::string(
            /* Structs */
            "%data_buffer   = OpTypeStruct %${baseType}\n"

            /* Pointers */
            "%untyped_phys_ptr                 = OpTypeUntypedPointerKHR PhysicalStorageBuffer\n"
            "%${baseType}_phys_ptr             = OpTypePointer           PhysicalStorageBuffer %${baseType}\n"
            "%data_buffer_phys_ptr             = OpTypePointer           PhysicalStorageBuffer %data_buffer\n"
            "%data_buffer_phys_ptr_ptr         = OpTypePointer           StorageBuffer         %data_buffer_phys_ptr\n"

            /* Structs cd. */
            "%phys_ptrs_struct = OpTypeStruct %data_buffer_phys_ptr %data_buffer_phys_ptr\n"

            /* Pointers cd. */
            "%phys_ptrs_struct_ptr = OpTypePointer StorageBuffer %phys_ptrs_struct\n"

            /* Variables */
            "%all_data_var = OpVariable %phys_ptrs_struct_ptr  StorageBuffer\n");
        break;
    }
    case PointerTestCases::OP_BITCAST_TO_UNTYPED_PHYSICAL_STORAGE:
    {
        variables += std::string(
            /* Structs */
            "%data_buffer   = OpTypeStruct %${baseType}\n"

            /* Pointers */
            "%untyped_phys_ptr                 = OpTypeUntypedPointerKHR PhysicalStorageBuffer\n"
            "%${baseType}_phys_ptr             = OpTypePointer           PhysicalStorageBuffer %${baseType}\n"
            "%data_buffer_phys_ptr             = OpTypePointer           PhysicalStorageBuffer %data_buffer\n"
            "%data_buffer_phys_ptr_ptr         = OpTypePointer           StorageBuffer         %data_buffer_phys_ptr\n"

            /* Structs cd. */
            "%phys_ptrs_struct = OpTypeStruct %data_buffer_phys_ptr %data_buffer_phys_ptr\n"

            /* Pointers cd. */
            "%phys_ptrs_struct_ptr = OpTypePointer StorageBuffer %phys_ptrs_struct\n"

            /* Variables */
            "%all_data_var = OpVariable %phys_ptrs_struct_ptr  StorageBuffer\n");
        break;
    }
    case PointerTestCases::OP_PTR_ACCESS_CHAIN_PHYSICAL_STORAGE:
    {
        variables += std::string(
            /* Arrays */
            "%array_${baseType}_${threadCount} = OpTypeArray %${baseType} %c_uint32_${threadCount}\n"

            /* Structs */
            "%data_buffer   = OpTypeStruct %array_${baseType}_${threadCount}\n"

            /* Pointers */
            "%untyped_phys_ptr                 = OpTypeUntypedPointerKHR PhysicalStorageBuffer\n"
            "%data_buffer_phys_ptr             = OpTypePointer           PhysicalStorageBuffer %data_buffer\n"
            "%data_buffer_phys_ptr_ptr         = OpTypePointer           StorageBuffer         %data_buffer_phys_ptr\n"

            /* Structs cd. */
            "%phys_ptrs_struct = OpTypeStruct %data_buffer_phys_ptr %data_buffer_phys_ptr\n"

            /* Pointers cd. */
            "%phys_ptrs_struct_ptr = OpTypePointer StorageBuffer %phys_ptrs_struct\n"

            /* Variables */
            "%all_data_var = OpVariable %phys_ptrs_struct_ptr  StorageBuffer\n");
        break;
    }
    case PointerTestCases::OP_FUNCTION_CALL_PHYSICAL_STORAGE:
    {
        variables += std::string(
            /* Structs */
            "%data_buffer   = OpTypeStruct %${baseType}\n"

            /* Pointers */
            "%untyped_phys_ptr                 = OpTypeUntypedPointerKHR PhysicalStorageBuffer\n"
            "%data_buffer_phys_ptr             = OpTypePointer           PhysicalStorageBuffer %data_buffer\n"
            "%data_buffer_phys_ptr_ptr         = OpTypePointer           StorageBuffer         %data_buffer_phys_ptr\n"

            /* Structs cd. */
            "%phys_ptrs_struct = OpTypeStruct %data_buffer_phys_ptr %data_buffer_phys_ptr\n"

            /* Pointers cd. */
            "%phys_ptrs_struct_ptr = OpTypePointer StorageBuffer %phys_ptrs_struct\n"

            /* Variables */
            "%all_data_var = OpVariable %phys_ptrs_struct_ptr  StorageBuffer\n");
        break;
    }
    case PointerTestCases::OP_SELECT_VARIABLE_PTR:
    case PointerTestCases::OP_PHI_VARIABLE_PTR:
    {
        variables += std::string(
            /* Structs */
            "%push_constant                    = OpTypeStruct %uint32\n"
            "%input_buffer_0                   = OpTypeStruct %${baseType}\n"
            "%input_buffer_1                   = OpTypeStruct %${baseType}\n"
            "%output_buffer                    = OpTypeStruct %${baseType}\n"

            /* Pointers */
            "%push_constant_ptr                = OpTypePointer           PushConstant  %push_constant\n"
            "%uint32_push_constant_ptr         = OpTypePointer           PushConstant  %uint32\n"
            "%${baseType}_storage_buffer_ptr   = OpTypePointer           StorageBuffer %${baseType}\n"
            "%output_buffer_storage_buffer_ptr = OpTypePointer           StorageBuffer %output_buffer\n"
            "%storage_buffer_untyped_ptr       = OpTypeUntypedPointerKHR StorageBuffer\n"

            /* Objects */
            "%push_constant_var                = OpVariable              %push_constant_ptr                "
            "PushConstant\n"
            "%input_data_0_untyped_var         = OpUntypedVariableKHR    %storage_buffer_untyped_ptr       "
            "StorageBuffer %input_buffer_0\n"
            "%input_data_1_untyped_var         = OpUntypedVariableKHR    %storage_buffer_untyped_ptr       "
            "StorageBuffer %input_buffer_1\n"
            "%output_data_var                  = OpVariable              %output_buffer_storage_buffer_ptr "
            "StorageBuffer\n");
        break;
    }
    case PointerTestCases::OP_PTR_EQUAL_VARIABLE_PTR:
    case PointerTestCases::OP_PTR_NOT_EQUAL_VARIABLE_PTR:
    {
        variables += std::string(
            /* Arrays */
            "%array_first_32 = OpTypeArray %${baseType} %c_uint32_32\n"
            "${secondArray:opt}\n"

            /* Structs */
            "%input_buffer                   = OpTypeStruct %array_first_32\n"
            "%output_buffer                  = OpTypeStruct %uint32\n"

            /* Pointers */
            "%${baseType}_storage_buffer_ptr   = OpTypePointer           StorageBuffer                     "
            "%${baseType}\n"
            "%output_uint32_storage_buffer_ptr = OpTypePointer           StorageBuffer                     "
            "%uint32\n"
            "%input_buffer_storage_buffer_ptr  = OpTypePointer           StorageBuffer                     "
            "%input_buffer\n"
            "%output_buffer_storage_buffer_ptr = OpTypePointer           StorageBuffer                     "
            "%output_buffer\n"
            "%storage_buffer_untyped_ptr       = OpTypeUntypedPointerKHR StorageBuffer\n"

            /* Constants */
            "%c_null_untyped_ptr               = OpConstantNull          %storage_buffer_untyped_ptr\n     "

            /* Objects */
            "%input_data_var                   = OpVariable              %input_buffer_storage_buffer_ptr  "
            "StorageBuffer\n"
            "%output_data_var                  = OpVariable              %output_buffer_storage_buffer_ptr "
            "StorageBuffer\n");
        break;
    }
    case PointerTestCases::OP_PTR_DIFF_VARIABLE_PTR:
    {
        variables += std::string(
            /* Arrays */
            "%array_first_32                   = OpTypeArray %${baseType} %c_uint32_32\n"
            "${secondArray:opt}\n"

            /* Struct */
            "%input_buffer                     = OpTypeStruct             %array_first_32\n"
            "%output_buffer                    = OpTypeStruct             %uint32\n"

            /* Pointers */
            "%uint32_storage_buffer_ptr        = OpTypePointer            StorageBuffer     %uint32\n"
            "%input_buffer_storage_buffer_ptr  = OpTypePointer            StorageBuffer     %input_buffer\n"
            "%output_buffer_storage_buffer_ptr = OpTypePointer            StorageBuffer     %output_buffer\n"
            "%storage_buffer_untyped_ptr       = OpTypeUntypedPointerKHR  StorageBuffer\n"

            /* Objects */
            "%input_data_var                   = OpVariable              %input_buffer_storage_buffer_ptr  "
            "StorageBuffer\n"
            "%output_data_var                  = OpVariable              %output_buffer_storage_buffer_ptr "
            "StorageBuffer\n");
        break;
    }
    case PointerTestCases::OP_PTR_ACCESS_CHAIN_VARIABLE_PTR:
    {
        variables += std::string(
            /* Arrays */
            "%array_${baseType}_${threadCount} = OpTypeArray %${baseType} %c_uint32_${threadCount}\n"

            /* Struct */
            "%input_buffer                     = OpTypeStruct            %array_${baseType}_${threadCount}\n"
            "%output_buffer                    = OpTypeStruct            %array_${baseType}_${threadCount}\n"

            /* Pointers */
            "%${baseType}_storage_buffer_ptr     = OpTypePointer           StorageBuffer                     "
            "%${baseType}\n"
            "%output_buffer_storage_buffer_ptr   = OpTypePointer           StorageBuffer                     "
            "%output_buffer\n"
            "%strided_storage_buffer_untyped_ptr = OpTypeUntypedPointerKHR StorageBuffer\n"
            "%storage_buffer_untyped_ptr =         OpTypeUntypedPointerKHR StorageBuffer\n"

            /* Objects */
            "%input_data_untyped_var           = OpUntypedVariableKHR    %storage_buffer_untyped_ptr       "
            "StorageBuffer %input_buffer\n"
            "%output_data_var                  = OpVariable              %output_buffer_storage_buffer_ptr "
            "StorageBuffer\n");
        break;
    }
    case PointerTestCases::OP_FUNCTION_CALL_VARIABLE_PTR:
    {
        variables += std::string(
            /* Arrays */
            "%array_32 = OpTypeArray %${baseType} %c_uint32_32\n"

            /* Struct */
            "%input_buffer                     = OpTypeStruct %array_32\n"
            "%output_buffer                    = OpTypeStruct %${baseType}\n"

            /* Pointers */
            "%${baseType}_storage_buffer_ptr   = OpTypePointer           StorageBuffer                     "
            "%${baseType}\n"
            "%output_buffer_storage_buffer_ptr = OpTypePointer           StorageBuffer                     "
            "%output_buffer\n"
            "%storage_buffer_untyped_ptr       = OpTypeUntypedPointerKHR StorageBuffer\n"

            /* Objects */
            "%input_data_untyped_var           = OpUntypedVariableKHR    %storage_buffer_untyped_ptr       "
            "StorageBuffer %input_buffer\n"
            "%output_data_var                  = OpVariable              %output_buffer_storage_buffer_ptr "
            "StorageBuffer\n");
        break;
    }
    case PointerTestCases::MULTIPLE_ACCESS_CHAINS_VARIABLE_PTR:
    {
        variables += std::string(
            /* Arrays */
            "%array_first_32                   = OpTypeArray %${baseType}  %c_uint32_32\n"
            "${secondArray}\n"

            /* Struct */
            "%input_buffer                     = OpTypeStruct             %array_first_32\n"
            "%output_buffer                    = OpTypeStruct             %${otherType}\n"

            /* Pointers */
            "%other_type_storage_buffer_ptr    = OpTypePointer            StorageBuffer     %${otherType}\n"
            "%input_buffer_storage_buffer_ptr  = OpTypePointer            StorageBuffer     %input_buffer\n"
            "%output_buffer_storage_buffer_ptr = OpTypePointer            StorageBuffer     %output_buffer\n"
            "%storage_buffer_untyped_ptr       = OpTypeUntypedPointerKHR  StorageBuffer\n"

            /* Objects */
            "%input_data_var                   = OpVariable              %input_buffer_storage_buffer_ptr  "
            "StorageBuffer\n"
            "%output_data_var                  = OpVariable              %output_buffer_storage_buffer_ptr "
            "StorageBuffer\n");
        break;
    }
    case PointerTestCases::FUNCTION_VARIABLE_VARIABLE_PTR:
    {
        variables += std::string(
            /* Struct */
            "%push_constant                           = OpTypeStruct %uint32\n"
            "%input_buffer_0                          = OpTypeStruct %${baseType}\n"
            "%input_buffer_1                          = OpTypeStruct %${baseType}\n"
            "%output_buffer                           = OpTypeStruct %${baseType}\n"

            /* Pointers */
            "%push_constant_ptr                       = OpTypePointer           PushConstant  %push_constant\n"
            "%uint32_push_constant_ptr                = OpTypePointer           PushConstant  %uint32\n"
            "%${baseType}_storage_buffer_ptr          = OpTypePointer           StorageBuffer                     "
            "%${baseType}\n"
            "%output_buffer_storage_buffer_ptr        = OpTypePointer           StorageBuffer                     "
            "%output_buffer\n"
            "%storage_buffer_untyped_ptr              = OpTypeUntypedPointerKHR StorageBuffer\n"
            "%storage_buffer_untyped_ptr_function_ptr = OpTypePointer           Function                          "
            "%storage_buffer_untyped_ptr\n"

            /* Objects */
            "%push_constant_var                       = OpVariable              %push_constant_ptr                "
            "PushConstant\n"
            "%input_data_0_untyped_var                = OpUntypedVariableKHR    %storage_buffer_untyped_ptr       "
            "StorageBuffer %input_buffer_0\n"
            "%input_data_1_untyped_var                = OpUntypedVariableKHR    %storage_buffer_untyped_ptr       "
            "StorageBuffer %input_buffer_1\n"
            "%output_data_var                         = OpVariable              %output_buffer_storage_buffer_ptr "
            "StorageBuffer\n");
        break;
    }
    case PointerTestCases::PRIVATE_VARIABLE_VARIABLE_PTR:
    {
        variables += std::string(
            /* Struct */
            "%push_constant                           = OpTypeStruct %uint32\n"
            "%input_buffer_0                          = OpTypeStruct %${baseType}\n"
            "%input_buffer_1                          = OpTypeStruct %${baseType}\n"
            "%output_buffer                           = OpTypeStruct %${baseType}\n"

            /* Pointers */
            "%push_constant_ptr                       = OpTypePointer           PushConstant  %push_constant\n"
            "%uint32_push_constant_ptr                = OpTypePointer           PushConstant  %uint32\n"
            "%${baseType}_storage_buffer_ptr          = OpTypePointer           StorageBuffer                     "
            "%${baseType}\n"
            "%output_buffer_storage_buffer_ptr        = OpTypePointer           StorageBuffer                     "
            "%output_buffer\n"
            "%storage_buffer_untyped_ptr              = OpTypeUntypedPointerKHR StorageBuffer\n"
            "%storage_buffer_untyped_ptr_private_ptr  = OpTypePointer           Private                           "
            "%storage_buffer_untyped_ptr\n"

            /* Objects */
            "%push_constant_var                       = OpVariable              %push_constant_ptr                "
            "PushConstant\n"
            "%input_data_0_untyped_var                = OpUntypedVariableKHR    %storage_buffer_untyped_ptr            "
            " StorageBuffer %input_buffer_0\n"
            "%input_data_1_untyped_var                = OpUntypedVariableKHR    %storage_buffer_untyped_ptr            "
            " StorageBuffer %input_buffer_1\n"
            "%output_data_var                         = OpVariable              %output_buffer_storage_buffer_ptr      "
            " StorageBuffer\n"
            "%output_copy_private_var                 = OpVariable              "
            "%storage_buffer_untyped_ptr_private_ptr Private\n");
        break;
    }
    case PointerTestCases::WORKGROUP_MEMORY_VARIABLE_PTR:
    {
        variables += std::string(
            /* Arrays */
            "%array_base                      = OpTypeArray %${baseType}  %c_uint32_64\n"

            /* Struct */
            "%input_buffer                     = OpTypeStruct             %array_base\n"
            "%output_buffer                    = OpTypeStruct             %array_base\n"
            "%shared_buffer                    = OpTypeStruct             %array_base\n"

            /* Pointers */
            "%${baseType}_storage_buffer_ptr   = OpTypePointer            StorageBuffer     %${baseType}\n"
            "%input_buffer_storage_buffer_ptr  = OpTypePointer            StorageBuffer     %input_buffer\n"
            "%output_buffer_storage_buffer_ptr = OpTypePointer            StorageBuffer     %output_buffer\n"
            "%storage_buffer_untyped_ptr       = OpTypeUntypedPointerKHR  StorageBuffer\n"
            "%workgroup_untyped_ptr            = OpTypeUntypedPointerKHR  Workgroup\n"

            /* Objects */
            "%input_data_var        = OpVariable %input_buffer_storage_buffer_ptr  StorageBuffer\n"
            "%output_data_var       = OpVariable %output_buffer_storage_buffer_ptr StorageBuffer\n"
            "%workgroup_untyped_var = OpUntypedVariableKHR  %workgroup_untyped_ptr Workgroup %shared_buffer\n");
        break;
    }
    default:
    {
        DE_ASSERT(0);
        DE_FATAL("Unknown test case.");
        break;
    }
    }

    variables += std::string(
        /* Objects */
        "%id              = OpVariable           %vec3_uint32_input_ptr                            Input\n");

    return variables;
}

std::string createShaderVariables(MEMORY_INTERPRETATION_TEST_CASE testCase, bool read)
{
    std::string variables = std::string(
        /* Base types */
        "%void                  = OpTypeVoid\n"
        "%bool                  = OpTypeBool\n"
        "%uint32                = OpTypeInt 32 0\n"
        "%vec2_uint32           = OpTypeVector %uint32      2\n"
        "%vec3_uint32           = OpTypeVector %uint32      3\n"
        "%vec4_uint32           = OpTypeVector %uint32      4\n"
        "%array                 = OpTypeRuntimeArray %uint32\n"
        "%block                 = OpTypeStruct %array\n"

        /* Function types */
        "%void_func             = OpTypeFunction %void\n"

        /* Constants */
        "%c_uint32_0            = OpConstant %uint32 0\n"
        "%c_uint32_1            = OpConstant %uint32 1\n"
        "%c_uint32_2            = OpConstant %uint32 2\n"
        "%c_uint32_3            = OpConstant %uint32 3\n"
        "%c_uint32_64           = OpConstant %uint32 64\n"

        /* Pointers */
        "%uint32_storage_ptr    = OpTypePointer StorageBuffer %uint32\n"
        "%ptr_struct_block      = OpTypePointer StorageBuffer %block\n"
        "%untyped_ptr           = OpTypeUntypedPointerKHR StorageBuffer\n"
        "%uint32_input_ptr      = OpTypePointer Input %uint32\n"
        "%vec3_uint32_input_ptr = OpTypePointer Input %vec3_uint32\n"

        /* Variables */
        "%id                    = OpVariable %vec3_uint32_input_ptr Input\n"
        "%indices_var           = OpVariable %ptr_struct_block StorageBuffer\n");

    bool skipVars = false;

    // Case-specific types
    switch (testCase)
    {
    case MemoryInterpretationTestCases::LARGE_ARRAY_STRIDE:
    {
        variables += std::string("%large_array = OpTypeRuntimeArray %uint32\n");

        break;
    }
    case MemoryInterpretationTestCases::NON_ZERO_OFFSET:
    {
        variables += std::string("%test_struct = OpTypeStruct %uint32 %uint32\n"
                                 "%test_array  = OpTypeRuntimeArray %test_struct\n");

        break;
    }
    case MemoryInterpretationTestCases::MIXED_OFFSETS:
    {
        variables += std::string("%test_struct = OpTypeStruct %uint32 %uint32 %uint32 %uint32\n");

        break;
    }
    case MemoryInterpretationTestCases::MULTIPLE_ACCESS_CHAINS:
    {
        variables += std::string("%type_1       = OpTypeArray %uint32 %c_uint32_64\n"
                                 "%type_2_array = OpTypeRuntimeArray %uint32\n"
                                 "%type_2       = OpTypeStruct %uint32 %type_2_array\n"
                                 "%type_3       = OpTypeArray %uint32 %c_uint32_64\n");

        break;
    }
    case MemoryInterpretationTestCases::SHORT2_NO_STORAGE_CAP:
    {
        variables += std::string("%short        = OpTypeInt 16 1\n"
                                 "%short2       = OpTypeVector %short 2\n");

        break;
    }
    case MemoryInterpretationTestCases::CHAR4_NO_STORAGE_CAP:
    {
        variables += std::string("%uchar        = OpTypeInt 8 0\n"
                                 "%uchar4       = OpTypeVector %uchar 4\n");

        break;
    }
    case MemoryInterpretationTestCases::CHAR2_16BIT_STORAGE_CAP:
    {
        skipVars = true;
        variables += std::string("%uchar        = OpTypeInt 8 0\n"
                                 "%uchar2       = OpTypeVector %uchar 2\n"
                                 "%uchar2_array = OpTypeRuntimeArray %uchar2\n"
                                 "%ushort       = OpTypeInt 16 0\n"
                                 "%out_array    = OpTypeRuntimeArray %ushort\n"
                                 "%out_block    = OpTypeStruct %out_array\n"
                                 "%ptr_struct_out_block = OpTypePointer StorageBuffer %out_block\n"
                                 "%ushort_storage_ptr   = OpTypePointer StorageBuffer %ushort\n");

        if (read)
        {
            variables += std::string("%out_var = OpVariable %ptr_struct_out_block StorageBuffer\n"

                                     "%in_var  = OpUntypedVariableKHR %untyped_ptr StorageBuffer %block\n");
        }
        else
        {
            variables += std::string("%out_var = OpUntypedVariableKHR %untyped_ptr StorageBuffer %block\n"

                                     "%in_var  = OpVariable %ptr_struct_out_block StorageBuffer\n");
        }

        break;
    }
    case MemoryInterpretationTestCases::UNTYPED_FROM_TYPED_VAR:
    case MemoryInterpretationTestCases::UNTYPED_FROM_TYPED_ACCESS_CHAIN:
    {
        skipVars = true;
        variables += std::string("%ptr_array_storage = OpTypePointer StorageBuffer %array\n");
        if (read)
        {
            variables += std::string("%out_var = OpVariable %ptr_struct_block StorageBuffer\n"

                                     "%in_var  = OpVariable %ptr_struct_block StorageBuffer\n");
        }
        else
        {
            variables += std::string("%out_var = OpVariable %ptr_struct_block StorageBuffer\n"

                                     "%in_var  = OpVariable %ptr_struct_block StorageBuffer\n");
        }

        break;
    }
    default:
    {
        DE_ASSERT(0);
        DE_FATAL("Unknown test case.");
        break;
    }
    }

    // Variables
    if (!skipVars)
    {
        if (read)
        {
            variables += std::string("%out_var = OpVariable %ptr_struct_block StorageBuffer\n"

                                     "%in_var  = OpUntypedVariableKHR %untyped_ptr StorageBuffer %block\n");
        }
        else
        {
            variables += std::string("%out_var = OpUntypedVariableKHR %untyped_ptr StorageBuffer %block\n"

                                     "%in_var  = OpVariable %ptr_struct_block StorageBuffer\n");
        }
    }

    return variables;
}

std::string createShaderVariables(BLOCK_ARRAY_TEST_CASE /* testCase */)
{
    std::string variables = std::string(
        /* Base types */
        "%void                  = OpTypeVoid\n"
        "%bool                  = OpTypeBool\n"
        "%uint32                = OpTypeInt 32 0\n"
        "%float                 = OpTypeFloat 32\n"
        "%vec3_uint32           = OpTypeVector %uint32      3\n"
        "%vec4_uint32           = OpTypeVector %uint32      4\n"
        "%int_array             = OpTypeRuntimeArray %uint32\n"
        "%float_array           = OpTypeRuntimeArray %float\n"
        "%int4_array            = OpTypeRuntimeArray %vec4_uint32\n"

        /* Function types */
        "%void_func             = OpTypeFunction %void\n"

        /* Constants */
        "%c_uint32_0            = OpConstant %uint32 0\n"
        "%c_uint32_1            = OpConstant %uint32 1\n"
        "%c_uint32_2            = OpConstant %uint32 2\n"
        "%c_uint32_3            = OpConstant %uint32 3\n"
        "%c_uint32_4            = OpConstant %uint32 4\n"
        "%c_uint32_64           = OpConstant %uint32 64\n"

        /* Uniform buffer */
        "%uni_array             = OpTypeArray %uint32 ${threads_const}\n"
        "%uni_block             = OpTypeStruct %uni_array\n"
        "%ptr_uni_block         = OpTypePointer StorageBuffer %uni_block\n"
        "%uni_var               = OpVariable %ptr_uni_block StorageBuffer\n"

        /* Output buffer */
        "%out_block             = OpTypeStruct %int_array\n"
        "%ptr_out_block         = OpTypePointer StorageBuffer %out_block\n"
        "%out_var               = OpVariable %ptr_out_block StorageBuffer\n"

        /* Input buffer */
        "%in_block_0            = OpTypeStruct %float_array\n"
        "%in_block_1            = OpTypeStruct %int4_array\n"
        "%block_array           = OpTypeArray %in_block_0 ${threads_const}\n"
        "%ptr_storage_block     = OpTypePointer StorageBuffer %in_block_0\n"
        "%ptr_storage_block_arr = OpTypePointer StorageBuffer %block_array\n"
        "%in_var                = OpVariable %ptr_storage_block_arr StorageBuffer\n"

        /* Pointers */
        "%uint32_input_ptr      = OpTypePointer Input %uint32\n"
        "%vec3_uint32_input_ptr = OpTypePointer Input %vec3_uint32\n"
        "%ptr_no_stride         = OpTypeUntypedPointerKHR StorageBuffer\n"
        "%ptr_4_stride          = OpTypeUntypedPointerKHR StorageBuffer\n"
        "%ptr_16_stride         = OpTypeUntypedPointerKHR StorageBuffer\n"
        "%uint32_storage_ptr    = OpTypePointer StorageBuffer %uint32\n"
        "%block0_storage_ptr    = OpTypePointer StorageBuffer %in_block_0\n"
        "%uint32_func_ptr       = OpTypePointer Function %uint32\n"

        /* Variables */
        "%id                    = OpVariable %vec3_uint32_input_ptr Input\n");

    return variables;
}

std::string createShaderVariables(WORKGROUP_TEST_CASE testCase)
{
    std::string variables = std::string(
        /* Base types */
        "%void                  = OpTypeVoid\n"
        "%bool                  = OpTypeBool\n"
        "%${baseType}           = ${baseDecl}\n"
        "%vec4_${baseType}      = OpTypeVector %${baseType} 4\n"
        "%vec3_uint32           = OpTypeVector %uint32      3\n"

        /* Function types */
        "%void_func             = OpTypeFunction %void\n"

        /* Constants */
        "%c_uint32_0            = OpConstant %uint32 0\n"
        "%c_uint32_1            = OpConstant %uint32 1\n"
        "%c_uint32_2            = OpConstant %uint32 2\n"
        "%c_uint32_264          = OpConstant %uint32 264\n"

        /* Pointers */
        "%uint32_input_ptr      = OpTypePointer Input %uint32\n"
        "%vec3_uint32_input_ptr = OpTypePointer Input %vec3_uint32\n");

    switch (testCase)
    {
    case WorkgroupTestCases::NOT_ALIASED:
    case WorkgroupTestCases::ALIASED:
    {
        variables += std::string(
            /* Struct */
            "%input_buffer_0                        = OpTypeStruct            %vec4_${baseType} %${baseType}\n"
            "%input_buffer_1                        = OpTypeStruct            %vec4_${baseType} %${baseType}\n"
            "%output_buffer_0                       = OpTypeStruct            %vec4_${baseType} %${baseType}\n"
            "%output_buffer_1                       = OpTypeStruct            %vec4_${baseType} %${baseType}\n"
            "%data_buffer                           = OpTypeStruct            %vec4_${baseType} %${baseType}\n"

            /* Pointers */
            "%${baseType}_storage_buffer_ptr      = OpTypePointer           StorageBuffer     %${baseType}\n"
            "%vec4_${baseType}_storage_buffer_ptr = OpTypePointer           StorageBuffer     %vec4_${baseType}\n"
            "%${baseType}_workgroup_ptr           = OpTypePointer           Workgroup         %${baseType}\n"
            "%vec4_${baseType}_workgroup_ptr      = OpTypePointer           Workgroup         %vec4_${baseType}\n"
            "%input_buffer_0_storage_buffer_ptr     = OpTypePointer           StorageBuffer     %input_buffer_0\n"
            "%input_buffer_1_storage_buffer_ptr     = OpTypePointer           StorageBuffer     %input_buffer_0\n"
            "%output_buffer_0_storage_buffer_ptr    = OpTypePointer           StorageBuffer     %output_buffer_0\n"
            "%output_buffer_1_storage_buffer_ptr    = OpTypePointer           StorageBuffer     %output_buffer_1\n"
            "%workgroup_untyped_ptr               = OpTypeUntypedPointerKHR Workgroup\n"

            /* Objects */
            "%input_data_0_var                      = OpVariable              %input_buffer_0_storage_buffer_ptr  "
            "StorageBuffer\n"
            "%input_data_1_var                      = OpVariable              %input_buffer_1_storage_buffer_ptr  "
            "StorageBuffer\n"
            "%output_data_0_var                     = OpVariable              %output_buffer_0_storage_buffer_ptr "
            "StorageBuffer\n"
            "%output_data_1_var                     = OpVariable              %output_buffer_1_storage_buffer_ptr "
            "StorageBuffer\n"
            "%data_buffer_0_untyped_var           = OpUntypedVariableKHR    %workgroup_untyped_ptr            "
            "Workgroup     %data_buffer\n"
            "%data_buffer_1_untyped_var           = OpUntypedVariableKHR    %workgroup_untyped_ptr            "
            "Workgroup     %data_buffer\n");
        break;
    }
    default:
    {
        DE_ASSERT(0);
        DE_FATAL("Unknown test case.");
        break;
    }
    }

    variables += std::string(
        /* Objects */
        "%id              = OpVariable           %vec3_uint32_input_ptr                            Input\n");

    return variables;
}

std::string createShaderVariables(COOPERATIVE_MATRIX_TEST_CASE testCase)
{
    std::string variables = std::string("");

    switch (testCase)
    {
    case CooperativeMatrixTestCases::BASIC_LOAD:
    {
        variables += std::string(
            /* Base types */
            "%void                   = OpTypeVoid\n"
            "%bool                   = OpTypeBool\n"
            "%${baseType}            = ${baseDecl}\n"
            "%${baseType}_rta        = OpTypeRuntimeArray %${baseType}\n"
            "%vec3_uint32            = OpTypeVector       %uint32      3\n"

            /* Function types */
            "%void_func              = OpTypeFunction %void\n"

            /* Constants */
            "%c_uint32_0             = OpConstant %uint32 0\n"
            "%c_uint32_1             = OpConstant %uint32 1\n"
            "%c_uint32_scope         = OpConstant %uint32 3\n" // Subgroup scope
            "%c_uint32_2             = OpConstant %uint32 2\n"
            "%c_matrix_use           = OpConstant %uint32 ${matrixUse}\n"
            "%c_matrix_layout        = OpConstant %uint32 ${matrixLayout}\n"
            "%c_type_size            = OpConstant %uint32 ${typeSize}\n"

            /* Spec constants */
            "%rows = OpSpecConstant %uint32 0\n"
            "%cols = OpSpecConstant %uint32 0\n"
            "%stride = OpSpecConstantOp %uint32 IMul %cols %c_type_size\n"

            /* Cooperative matrix */
            "%${baseType}_matrix = OpTypeCooperativeMatrixKHR %${baseType} %c_uint32_scope %rows %cols %c_matrix_use\n"

            /* Struct */
            "%input_buffer                        = OpTypeStruct            %${baseType}_rta\n"
            "%output_buffer                       = OpTypeStruct            %${baseType}_rta\n"

            /* Pointers */
            "%uint32_input_ptr                    = OpTypePointer           Input             %uint32\n"
            "%vec3_uint32_input_ptr               = OpTypePointer           Input             %vec3_uint32\n"
            "%${baseType}_storage_buffer_ptr      = OpTypePointer           StorageBuffer     %${baseType}\n"
            "%output_buffer_storage_buffer_ptr    = OpTypePointer           StorageBuffer     %output_buffer\n"
            "%storage_buffer_untyped_ptr          = OpTypeUntypedPointerKHR StorageBuffer\n"

            /* Objects */
            "%input_data_untyped_var              = OpUntypedVariableKHR    %storage_buffer_untyped_ptr       "
            "StorageBuffer %input_buffer\n"
            "%output_data_var                     = OpVariable              %output_buffer_storage_buffer_ptr "
            "StorageBuffer\n"
            "%id                                  = OpVariable              %vec3_uint32_input_ptr            Input\n");
        break;
    }
    case CooperativeMatrixTestCases::BASIC_STORE:
    {
        variables += std::string(
            /* Base types */
            "%void                   = OpTypeVoid\n"
            "%bool                   = OpTypeBool\n"
            "%${baseType}            = ${baseDecl}\n"
            "%${baseType}_rta        = OpTypeRuntimeArray %${baseType}\n"
            "%vec3_uint32            = OpTypeVector       %uint32      3\n"

            /* Function types */
            "%void_func              = OpTypeFunction %void\n"

            /* Constants */
            "%c_uint32_0             = OpConstant %uint32 0\n"
            "%c_uint32_1             = OpConstant %uint32 1\n"
            "%c_uint32_scope         = OpConstant %uint32 3\n" // Subgroup scope
            "%c_uint32_2             = OpConstant %uint32 2\n"
            "%c_matrix_use           = OpConstant %uint32 ${matrixUse}\n"
            "%c_matrix_layout        = OpConstant %uint32 ${matrixLayout}\n"
            "%c_type_size            = OpConstant %uint32 ${typeSize}\n"

            /* Spec constants */
            "%rows = OpSpecConstant %uint32 0\n"
            "%cols = OpSpecConstant %uint32 0\n"
            "%stride = OpSpecConstantOp %uint32 IMul %cols %c_type_size\n"

            /* Cooperative matrix */
            "%${baseType}_matrix = OpTypeCooperativeMatrixKHR %${baseType} %c_uint32_scope %rows %cols %c_matrix_use\n"

            /* Struct */
            "%input_buffer                        = OpTypeStruct            %${baseType}_rta\n"
            "%output_buffer                       = OpTypeStruct            %${baseType}_rta\n"

            /* Pointers */
            "%uint32_input_ptr                    = OpTypePointer           Input             %uint32\n"
            "%vec3_uint32_input_ptr               = OpTypePointer           Input             %vec3_uint32\n"
            "%${baseType}_storage_buffer_ptr      = OpTypePointer           StorageBuffer     %${baseType}\n"
            "%input_buffer_storage_buffer_ptr     = OpTypePointer           StorageBuffer     %input_buffer\n"
            "%storage_buffer_untyped_ptr          = OpTypeUntypedPointerKHR StorageBuffer\n"

            /* Objects */
            "%input_data_var                      = OpVariable              %input_buffer_storage_buffer_ptr "
            "StorageBuffer\n"
            "%output_data_untyped_var             = OpUntypedVariableKHR    %storage_buffer_untyped_ptr      "
            "StorageBuffer %output_buffer\n"
            "%id                                  = OpVariable              %vec3_uint32_input_ptr           Input\n");
        break;
    }
    case CooperativeMatrixTestCases::TYPE_PUNNING_LOAD:
    {
        variables += std::string(
            /* Base types */
            "%void                   = OpTypeVoid\n"
            "%bool                   = OpTypeBool\n"
            "%${baseType}            = ${baseDecl}\n"
            "%${sameSizeType}        = ${sameSizeDecl}\n"
            "%${baseType}_rta        = OpTypeRuntimeArray %${baseType}\n"
            "%${sameSizeType}_rta    = OpTypeRuntimeArray %${sameSizeType}\n"
            "%vec3_uint32            = OpTypeVector       %uint32      3\n"

            /* Function types */
            "%void_func              = OpTypeFunction %void\n"

            /* Constants */
            "%c_uint32_0             = OpConstant %uint32 0\n"
            "%c_uint32_1             = OpConstant %uint32 1\n"
            "%c_uint32_scope         = OpConstant %uint32 3\n" // Subgroup scope
            "%c_uint32_2             = OpConstant %uint32 2\n"
            "%c_matrix_use           = OpConstant %uint32 ${matrixUse}\n"
            "%c_matrix_layout        = OpConstant %uint32 ${matrixLayout}\n"
            "%c_type_size            = OpConstant %uint32 ${typeSize}\n"

            /* Spec constants */
            "%rows = OpSpecConstant %uint32 0\n"
            "%cols = OpSpecConstant %uint32 0\n"
            "%stride = OpSpecConstantOp %uint32 IMul %cols %c_type_size\n"

            /* Cooperative matrix */
            "%${sameSizeType}_matrix = OpTypeCooperativeMatrixKHR %${sameSizeType} %c_uint32_scope %rows %cols "
            "%c_matrix_use\n"

            /* Struct */
            "%input_buffer               = OpTypeStruct            %${baseType}_rta\n"
            "%output_buffer              = OpTypeStruct            %${sameSizeType}_rta\n"

            /* Pointers */
            "%uint32_input_ptr                       = OpTypePointer           Input             %uint32\n"
            "%vec3_uint32_input_ptr                  = OpTypePointer           Input             %vec3_uint32\n"
            "%${sameSizeType}_storage_buffer_ptr     = OpTypePointer           StorageBuffer     %${sameSizeType}\n"
            "%output_buffer_storage_buffer_ptr       = OpTypePointer           StorageBuffer     %output_buffer\n"
            "%storage_buffer_untyped_ptr             = OpTypeUntypedPointerKHR StorageBuffer\n"

            /* Objects */
            "%input_data_untyped_var              = OpUntypedVariableKHR    %storage_buffer_untyped_ptr       "
            "StorageBuffer %input_buffer\n"
            "%output_data_var                     = OpVariable              %output_buffer_storage_buffer_ptr "
            "StorageBuffer\n"
            "%id                                  = OpVariable              %vec3_uint32_input_ptr            Input\n");
        break;
    }
    case CooperativeMatrixTestCases::TYPE_PUNNING_STORE:
    {
        variables += std::string(
            /* Base types */
            "%void                   = OpTypeVoid\n"
            "%bool                   = OpTypeBool\n"
            "%${baseType}            = ${baseDecl}\n"
            "%${sameSizeType}        = ${sameSizeDecl}\n"
            "%${baseType}_rta        = OpTypeRuntimeArray %${baseType}\n"
            "%${sameSizeType}_rta    = OpTypeRuntimeArray %${sameSizeType}\n"
            "%vec3_uint32            = OpTypeVector       %uint32      3\n"

            /* Function types */
            "%void_func              = OpTypeFunction %void\n"

            /* Constants */
            "%c_uint32_0             = OpConstant %uint32 0\n"
            "%c_uint32_1             = OpConstant %uint32 1\n"
            "%c_uint32_scope         = OpConstant %uint32 3\n" // Subgroup scope
            "%c_uint32_2             = OpConstant %uint32 2\n"
            "%c_matrix_use           = OpConstant %uint32 ${matrixUse}\n"
            "%c_matrix_layout        = OpConstant %uint32 ${matrixLayout}\n"
            "%c_type_size            = OpConstant %uint32 ${typeSize}\n"

            /* Spec constants */
            "%rows = OpSpecConstant %uint32 0\n"
            "%cols = OpSpecConstant %uint32 0\n"
            "%stride = OpSpecConstantOp %uint32 IMul %cols %c_type_size\n"

            /* Cooperative matrix */
            "%${baseType}_matrix = OpTypeCooperativeMatrixKHR %${baseType} %c_uint32_scope %rows %cols %c_matrix_use\n"

            /* Struct */
            "%input_buffer                        = OpTypeStruct            %${baseType}_rta\n"
            "%output_buffer                       = OpTypeStruct            %${sameSizeType}_rta\n"

            /* Pointers */
            "%uint32_input_ptr                    = OpTypePointer           Input             %uint32\n"
            "%vec3_uint32_input_ptr               = OpTypePointer           Input             %vec3_uint32\n"
            "%${baseType}_storage_buffer_ptr      = OpTypePointer           StorageBuffer     %${baseType}\n"
            "%input_buffer_storage_buffer_ptr     = OpTypePointer           StorageBuffer     %input_buffer\n"
            "%storage_buffer_untyped_ptr          = OpTypeUntypedPointerKHR StorageBuffer\n"

            /* Objects */
            "%input_data_var                      = OpVariable              %input_buffer_storage_buffer_ptr "
            "StorageBuffer\n"
            "%output_data_untyped_var             = OpUntypedVariableKHR    %storage_buffer_untyped_ptr      "
            "StorageBuffer %output_buffer\n"
            "%id                                  = OpVariable              %vec3_uint32_input_ptr           Input\n");
        break;
    }
    case CooperativeMatrixTestCases::MIXED_LOAD:
    {
        variables += std::string(
            /* Base types */
            "%void                   = OpTypeVoid\n"
            "%bool                   = OpTypeBool\n"
            "%${baseType}            = ${baseDecl}\n"
            "%${baseType}_rta        = OpTypeRuntimeArray %${baseType}\n"
            "%vec3_uint32            = OpTypeVector       %uint32      3\n"

            /* Function types */
            "%void_func              = OpTypeFunction %void\n"

            /* Constants */
            "%c_uint32_0             = OpConstant %uint32 0\n"
            "%c_uint32_1             = OpConstant %uint32 1\n"
            "%c_uint32_scope         = OpConstant %uint32 3\n" // Subgroup scope
            "%c_uint32_2             = OpConstant %uint32 2\n"
            "%c_matrix_use           = OpConstant %uint32 ${matrixUse}\n"
            "%c_matrix_layout        = OpConstant %uint32 ${matrixLayout}\n"
            "%c_type_size            = OpConstant %uint32 ${typeSize}\n"

            /* Spec constants */
            "%rows = OpSpecConstant %uint32 0\n"
            "%cols = OpSpecConstant %uint32 0\n"
            "%stride = OpSpecConstantOp %uint32 IMul %cols %c_type_size\n"

            /* Cooperative matrix */
            "%${baseType}_matrix = OpTypeCooperativeMatrixKHR %${baseType} %c_uint32_scope %rows %cols %c_matrix_use\n"

            /* Struct */
            "%input_buffer                        = OpTypeStruct            %${baseType}_rta\n"
            "%output_buffer                       = OpTypeStruct            %${baseType}_rta\n"

            /* Pointers */
            "%uint32_input_ptr                    = OpTypePointer           Input             %uint32\n"
            "%vec3_uint32_input_ptr               = OpTypePointer           Input             %vec3_uint32\n"
            "%${baseType}_storage_buffer_ptr      = OpTypePointer           StorageBuffer     %${baseType}\n"
            "%output_buffer_storage_buffer_ptr    = OpTypePointer           StorageBuffer     %output_buffer\n"
            "%storage_buffer_untyped_ptr          = OpTypeUntypedPointerKHR StorageBuffer\n"

            /* Objects */
            "%input_data_untyped_var              = OpUntypedVariableKHR    %storage_buffer_untyped_ptr       "
            "StorageBuffer %input_buffer\n"
            "%output_data_var                     = OpVariable              %output_buffer_storage_buffer_ptr "
            "StorageBuffer\n"
            "%id                                  = OpVariable              %vec3_uint32_input_ptr            Input\n");
        break;
    }
    case CooperativeMatrixTestCases::MIXED_STORE:
    {
        variables += std::string(
            /* Base types */
            "%void                   = OpTypeVoid\n"
            "%bool                   = OpTypeBool\n"
            "%${baseType}            = ${baseDecl}\n"
            "%${baseType}_rta        = OpTypeRuntimeArray %${baseType}\n"
            "%vec3_uint32            = OpTypeVector       %uint32      3\n"

            /* Function types */
            "%void_func              = OpTypeFunction %void\n"

            /* Constants */
            "%c_uint32_0             = OpConstant %uint32 0\n"
            "%c_uint32_1             = OpConstant %uint32 1\n"
            "%c_uint32_scope         = OpConstant %uint32 3\n" // Subgroup scope
            "%c_uint32_2             = OpConstant %uint32 2\n"
            "%c_matrix_use           = OpConstant %uint32 ${matrixUse}\n"
            "%c_matrix_layout        = OpConstant %uint32 ${matrixLayout}\n"
            "%c_type_size            = OpConstant %uint32 ${typeSize}\n"

            /* Spec constants */
            "%rows = OpSpecConstant %uint32 0\n"
            "%cols = OpSpecConstant %uint32 0\n"
            "%stride = OpSpecConstantOp %uint32 IMul %cols %c_type_size\n"

            /* Cooperative matrix */
            "%${baseType}_matrix = OpTypeCooperativeMatrixKHR %${baseType} %c_uint32_scope %rows %cols %c_matrix_use\n"

            /* Struct */
            "%input_buffer                        = OpTypeStruct            %${baseType}_rta\n"
            "%output_buffer                       = OpTypeStruct            %${baseType}_rta\n"

            /* Pointers */
            "%uint32_input_ptr                    = OpTypePointer           Input             %uint32\n"
            "%vec3_uint32_input_ptr               = OpTypePointer           Input             %vec3_uint32\n"
            "%${baseType}_storage_buffer_ptr      = OpTypePointer           StorageBuffer     %${baseType}\n"
            "%input_buffer_storage_buffer_ptr     = OpTypePointer           StorageBuffer     %input_buffer\n"
            "%storage_buffer_untyped_ptr          = OpTypeUntypedPointerKHR StorageBuffer\n"

            /* Objects */
            "%input_data_var                      = OpVariable              %input_buffer_storage_buffer_ptr "
            "StorageBuffer\n"
            "%output_data_untyped_var             = OpUntypedVariableKHR    %storage_buffer_untyped_ptr      "
            "StorageBuffer %output_buffer\n"
            "%id                                  = OpVariable              %vec3_uint32_input_ptr           Input\n");
        break;
    }
    default:
    {
        DE_ASSERT(0);
        DE_FATAL("Unknown test case.");
        break;
    }
    }

    return variables;
}

std::string createSimpleFunction(POINTER_TEST_CASE opType)
{
    std::string function = "";

    if (opType == PointerTestCases::OP_FUNCTION_CALL_PHYSICAL_STORAGE)
    {
        function += std::string("%simple_function_type  = OpTypeFunction %untyped_phys_ptr %untyped_phys_ptr\n"
                                "%simple_function       = OpFunction     %untyped_phys_ptr None %simple_function_type\n"
                                "%return_ptr            = OpFunctionParameter %untyped_phys_ptr\n"
                                "%label_simple_function = OpLabel\n"
                                "                         OpReturnValue       %return_ptr\n"
                                "                         OpFunctionEnd\n");
    }
    else // opType == PointerTestCases::FUNCTION_PARAMETERS_VARIABLE_PTR
    {
        function += std::string(
            "%simple_function_type  = OpTypeFunction      %storage_buffer_untyped_ptr %storage_buffer_untyped_ptr\n"
            "%simple_function       = OpFunction          %storage_buffer_untyped_ptr None %simple_function_type\n"
            "%input_ptr             = OpFunctionParameter %storage_buffer_untyped_ptr\n"
            "%label_simple_function = OpLabel\n"
            "%offseted_ptr          = OpUntypedAccessChainKHR %storage_buffer_untyped_ptr   %array_32    "
            "                                                 %input_ptr                    %c_uint32_4\n"
            "                         OpReturnValue           %offseted_ptr\n"
            "                         OpFunctionEnd\n");
    }

    return function;
}

std::string createShaderMain(BASE_TEST_CASE testCase)
{
    std::string main = std::string("%main               = OpFunction %void None %void_func\n"
                                   "%label_main         = OpLabel\n");

    switch (testCase)
    {
    case BaseTestCases::DESCRIPTOR_ARRAY:
    {
        main +=
            std::string("%id_loc = OpAccessChain %uint32_input_ptr %id      %c_uint32_0\n"
                        "%ndx    = OpLoad        %uint32           %id_loc\n"

                        "%block_loc_x = OpUntypedAccessChainKHR %storage_buffer_untyped_ptr %array_of_blocks "
                        "%input_data_untyped_var %ndx\n"
                        "%temp_loc        = OpLoad        %data                    %block_loc_x\n"
                        "%output_elem_loc = OpAccessChain %data_storage_buffer_ptr %output_data_var %c_uint32_0 %ndx\n"
                        "                   OpStore       %output_elem_loc         %temp_loc\n");

        break;
    }
    case BaseTestCases::ARRAY_LENGTH:
    {
        main += std::string(
            "%ndx                 = OpVariable    %uint32_function_ptr   Function\n"

            // Loop code
            "%thread_count_loc    = OpAccessChain %uint32_input_ptr      %id            %c_uint32_0\n"
            "%thread_count        = OpLoad        %uint32                %thread_count_loc\n"
            "                       OpStore       %ndx                   %c_uint32_0\n"
            "                       OpBranch      %label_0\n"
            "%label_0             = OpLabel\n"
            "                       OpLoopMerge   %label_4 %label_3 None\n"
            "                       OpBranch      %label_1\n"
            "%label_1             = OpLabel\n"
            "%curr_ndx            = OpLoad        %uint32  %ndx\n"
            "%iterate             = OpULessThan   %bool    %curr_ndx %thread_count\n"
            "                       OpBranchConditional    %iterate  %label_2      %label_4\n"
            "%label_2             = OpLabel\n"
            "%rta_elem            = OpUntypedAccessChainKHR %storage_buffer_untyped_ptr %input_buffer "
            "%input_data_untyped_var %c_uint32_0 %curr_ndx\n"
            "                       OpStore       %rta_elem                        %c_${baseType}_1\n"
            "                       OpBranch      %label_3\n"
            "%label_3             = OpLabel\n"
            "%new_ndx             = OpIAdd        %uint32   %curr_ndx %c_uint32_1\n"
            "                       OpStore       %ndx      %new_ndx\n"
            "                       OpBranch      %label_0\n"
            "%label_4             = OpLabel\n"

            "%runtime_size        = OpUntypedArrayLengthKHR %uint32                    %input_buffer    "
            "%input_data_untyped_var 0\n"
            "%array_size_loc      = OpAccessChain           %uint32_storage_buffer_ptr %output_data_var %c_uint32_0\n"

            "                       OpStore                 %array_size_loc            %runtime_size\n");
        break;
    }
    case BaseTestCases::LOAD:
    {
        main += std::string(
            "%id_loc              = OpAccessChain           %uint32_input_ptr               %id                 "
            "%c_uint32_0\n"
            "%x                   = OpLoad                  %uint32                         %id_loc\n"

            "%input_data_var_loc  = OpUntypedAccessChainKHR %storage_buffer_untyped_ptr     %input_buffer       "
            "%input_data_untyped_var %c_uint32_0 %x\n"
            "%output_data_var_loc = OpAccessChain           %storage_buffer_${baseType}_ptr %output_data_var           "
            "                 %c_uint32_0 %x\n"

            "%temp_data_var_loc   = ${loadOp}               %${baseType}                    %input_data_var_loc "
            "${args}\n"
            "                       OpStore                 %output_data_var_loc            %temp_data_var_loc\n");
        break;
    }
    case BaseTestCases::COPY_FROM:
    {
        main += std::string(
            "%id_loc              = OpAccessChain           %uint32_input_ptr               %id           %c_uint32_0\n"
            "%x                   = OpLoad                  %uint32                         %id_loc\n"

            "%input_data_var_loc  = OpUntypedAccessChainKHR %storage_buffer_untyped_ptr     %input_buffer "
            "%input_data_untyped_var %c_uint32_0 %x\n"
            "%output_data_var_loc = OpAccessChain           %storage_buffer_${baseType}_ptr               "
            "%output_data_var        %c_uint32_0 %x\n"

            "${copyOp}\n");
        break;
    }
    case BaseTestCases::STORE:
    {
        main += std::string(
            "%id_loc              = OpAccessChain           %uint32_input_ptr               %id                 "
            "%c_uint32_0\n"
            "%x                   = OpLoad                  %uint32                         %id_loc\n"

            "%input_data_var_loc  = OpAccessChain           %storage_buffer_${baseType}_ptr                     "
            "%input_data_var          %c_uint32_0 %x\n"
            "%output_data_var_loc = OpUntypedAccessChainKHR %storage_buffer_untyped_ptr     %output_buffer      "
            "%output_data_untyped_var %c_uint32_0 %x\n"

            "%temp_data_var_loc   = OpLoad                  %${baseType}                    %input_data_var_loc\n"
            "                       ${storeOp}              %output_data_var_loc  ${args}   %temp_data_var_loc\n");
        break;
    }
    case BaseTestCases::COPY_TO:
    {
        main += std::string("%id_loc              = OpAccessChain           %uint32_input_ptr               %id        "
                            "    %c_uint32_0\n"
                            "%x                   = OpLoad                  %uint32                         %id_loc\n"

                            "%input_data_var_loc  = OpAccessChain           %storage_buffer_${baseType}_ptr            "
                            "    %input_data_var          %c_uint32_0 %x\n"
                            "%output_data_var_loc = OpUntypedAccessChainKHR %storage_buffer_untyped_ptr     "
                            "%output_buffer %output_data_untyped_var %c_uint32_0 %x\n"

                            "${copyOp}\n");
        break;
    }
    default:
    {
        DE_ASSERT(0);
        DE_FATAL("Unknown test case.");
        break;
    }
    }

    main += std::string(

        "                     OpReturn\n"
        "                     OpFunctionEnd\n");

    return main;
}

std::string createShaderMain(ATOMIC_TEST_CASE testCase)
{
    std::string main = std::string("%main       = OpFunction %void None %void_func\n"
                                   "%label_main = OpLabel\n");

    switch (testCase)
    {
    case AtomicTestCases::OP_ATOMIC_INCREMENT:
    case AtomicTestCases::OP_ATOMIC_DECREMENT:
    {
        main += std::string("%output_data_var_loc = OpUntypedAccessChainKHR %storage_buffer_untyped_ptr %output_buffer "
                            "%output_data_untyped_var %c_uint32_0\n"
                            "                       OpStore   %output_data_var_loc %c_base_0\n"
                            "%return_val          = ${opType}               %${baseType}                               "
                            "%output_data_var_loc     %c_uint32_1 %c_uint32_0\n");
        break;
    }
    case AtomicTestCases::OP_ATOMIC_ADD:
    case AtomicTestCases::OP_ATOMIC_SUB:
    case AtomicTestCases::OP_ATOMIC_MIN:
    case AtomicTestCases::OP_ATOMIC_MAX:
    case AtomicTestCases::OP_ATOMIC_EXCHANGE:
    {
        main += std::string("%output_data_var_loc = OpUntypedAccessChainKHR %storage_buffer_untyped_ptr %output_buffer "
                            "%output_data_untyped_var %c_uint32_0\n"
                            "                       OpStore   %output_data_var_loc %c_base_0\n"
                            "%return_val          = ${opType}               %${baseType}                               "
                            "%output_data_var_loc     %c_uint32_1 %c_uint32_0 %op_value\n");
        break;
    }
    case AtomicTestCases::OP_ATOMIC_AND:
    case AtomicTestCases::OP_ATOMIC_OR:
    case AtomicTestCases::OP_ATOMIC_XOR:
    {
        main += std::string("%output_data_var_loc = OpUntypedAccessChainKHR %storage_buffer_untyped_ptr %output_buffer "
                            "%output_data_untyped_var %c_uint32_0\n"
                            "                       OpStore   %output_data_var_loc %c_base_0\n"
                            "%return_val          = ${opType}               %${baseType}                               "
                            "%output_data_var_loc     %c_uint32_1 %c_uint32_0 %op_value\n");
        break;
    }
    case AtomicTestCases::OP_ATOMIC_COMPARE_EXCHANGE:
    {
        main += std::string("%output_data_var_loc = OpUntypedAccessChainKHR %storage_buffer_untyped_ptr %output_buffer "
                            "%output_data_untyped_var %c_uint32_0\n"
                            "                       OpStore   %output_data_var_loc %c_${baseType}_1\n"
                            "%return_val          = ${opType}               %${baseType}                               "
                            "%output_data_var_loc     %c_uint32_1 %c_uint32_0 %c_uint32_0 %op_value %comp\n");
        break;
    }
    case AtomicTestCases::OP_ATOMIC_LOAD:
    {
        main += std::string("%id_loc              = OpAccessChain %uint32_input_ptr %id     %c_uint32_0\n"
                            "%x                   = OpLoad        %uint32           %id_loc\n"

                            "%input_data_var_loc  = OpUntypedAccessChainKHR %storage_buffer_untyped_ptr     "
                            "%input_buffer    %input_data_untyped_var %c_uint32_0 %x\n"
                            "%output_data_var_loc = OpAccessChain           %storage_buffer_${baseType}_ptr "
                            "%output_data_var                         %c_uint32_0 %x\n"

                            "%temp_data_var_loc   = ${loadOp} %${baseType} %input_data_var_loc ${args}\n"
                            "                       OpStore   %output_data_var_loc %temp_data_var_loc\n");
        break;
    }
    case AtomicTestCases::OP_ATOMIC_STORE:
    {
        main += std::string("%id_loc              = OpAccessChain %uint32_input_ptr %id     %c_uint32_0\n"
                            "%x                   = OpLoad        %uint32           %id_loc\n"

                            "%input_data_var_loc  = OpAccessChain           %storage_buffer_${baseType}_ptr "
                            "%input_data_var                          %c_uint32_0 %x\n"
                            "%output_data_var_loc = OpUntypedAccessChainKHR %storage_buffer_untyped_ptr     "
                            "%output_buffer  %output_data_untyped_var %c_uint32_0 %x\n"

                            "%temp_data_var_loc   = OpLoad %${baseType} %input_data_var_loc\n"
                            "                     ${storeOp} %output_data_var_loc ${args} %temp_data_var_loc\n");
        break;
    }
    default:
    {
        DE_ASSERT(0);
        DE_FATAL("Unknown test case.");
        break;
    }
    }

    main += std::string("                     OpReturn\n"
                        "                     OpFunctionEnd\n");

    return main;
}

std::string createShaderMain(TYPE_PUNNING_TEST_CASE testCase)
{
    std::string main = std::string("%main       = OpFunction %void None %void_func\n"
                                   "%label_main = OpLabel\n");

    switch (testCase)
    {
    case TypePunningTestCases::LOAD_SAME_SIZE_TYPES:
    {
        main += std::string(
            "%id_loc              = OpAccessChain           %uint32_input_ptr                   %id                 "
            "%c_uint32_0\n"
            "%x                   = OpLoad                  %uint32                             %id_loc\n"

            "%input_data_var_loc  = OpUntypedAccessChainKHR %storage_buffer_untyped_ptr         %input_buffer       "
            "%input_data_untyped_var %c_uint32_0 %x\n"
            "%output_data_var_loc = OpAccessChain           %storage_buffer_${sameSizeType}_ptr %output_data_var       "
            "                     %c_uint32_0 %x\n"

            "%temp_data_var_loc   = ${loadOp}               %${sameSizeType}                    %input_data_var_loc "
            "${args}\n"
            "                       OpStore                 %output_data_var_loc                %temp_data_var_loc\n");
        break;
    }
    case TypePunningTestCases::LOAD_SCALAR_VECTOR:
    {
        main += std::string(
            "%input_data_var_loc  = OpUntypedAccessChainKHR %storage_buffer_untyped_ptr     %input_buffer       "
            "%input_data_untyped_var %c_uint32_0\n"
            "%output_data_var_loc = OpAccessChain           %storage_buffer_${otherVec}_ptr                     "
            "%output_data_var        %c_uint32_0\n"

            "%temp_data_var_loc   = ${loadOp}               %${otherVec}                    %input_data_var_loc "
            "${args}\n"
            "                       OpStore                 %output_data_var_loc            %temp_data_var_loc\n");
        break;
    }
    case TypePunningTestCases::LOAD_VECTOR_SCALAR:
    {
        main += std::string(
            "%input_data_var_loc  = OpUntypedAccessChainKHR %storage_buffer_untyped_ptr      %input_buffer       "
            "%input_data_untyped_var %c_uint32_0\n"
            "%output_data_var_loc = OpAccessChain           %storage_buffer_${otherType}_ptr                     "
            "%output_data_var        %c_uint32_0\n"

            "%temp_data_var_loc   = ${loadOp}               %${otherType}                    %input_data_var_loc "
            "${args}\n"
            "                       OpStore                 %output_data_var_loc             %temp_data_var_loc\n");
        break;
    }
    case TypePunningTestCases::COPY_FROM_SAME_SIZE_TYPES:
    {
        main +=
            std::string("%id_loc              = OpAccessChain           %uint32_input_ptr                   %id        "
                        "      %c_uint32_0\n"
                        "%x                   = OpLoad                  %uint32                             %id_loc\n"

                        "%input_data_var_loc  = OpUntypedAccessChainKHR %storage_buffer_untyped_ptr         "
                        "%input_buffer    %input_data_untyped_var %c_uint32_0 %x\n"
                        "%output_data_var_loc = OpAccessChain           %storage_buffer_${sameSizeType}_ptr "
                        "%output_data_var                         %c_uint32_0 %x\n"

                        "${copyOp}\n");
        break;
    }
    case TypePunningTestCases::STORE_SAME_SIZE_TYPES:
    {
        main += std::string(
            "%id_loc              = OpAccessChain           %uint32_input_ptr               %id                 "
            "%c_uint32_0\n"
            "%x                   = OpLoad                  %uint32                         %id_loc\n"

            "%input_data_var_loc  = OpAccessChain           %storage_buffer_${baseType}_ptr                     "
            "%input_data_var          %c_uint32_0 %x\n"
            "%output_data_var_loc = OpUntypedAccessChainKHR %storage_buffer_untyped_ptr     %output_buffer      "
            "%output_data_untyped_var %c_uint32_0 %x\n"

            "%temp_data_var_loc   = OpLoad                  %${baseType}                    %input_data_var_loc\n"
            "                     ${storeOp}                %output_data_var_loc   ${args}  %temp_data_var_loc\n");
        break;
    }
    case TypePunningTestCases::STORE_SCALAR_VECTOR:
    {
        main += std::string(
            "%input_data_var_loc  = OpAccessChain           %storage_buffer_${baseType}_ptr                     "
            "%input_data_var          %c_uint32_0\n"
            "%output_data_var_loc = OpUntypedAccessChainKHR %storage_buffer_untyped_ptr     %output_buffer      "
            "%output_data_untyped_var %c_uint32_0\n"

            "%temp_data_var_loc   = OpLoad                  %${baseType}                    %input_data_var_loc\n"
            "                       ${storeOp}              %output_data_var_loc  ${args}   %temp_data_var_loc\n");
        break;
    }
    case TypePunningTestCases::STORE_VECTOR_SCALAR:
    {
        main += std::string(
            "%input_data_var_loc  = OpAccessChain           %storage_buffer_${baseVec}_ptr                      "
            "%input_data_var          %c_uint32_0\n"
            "%output_data_var_loc = OpUntypedAccessChainKHR %storage_buffer_untyped_ptr    %output_buffer       "
            "%output_data_untyped_var %c_uint32_0\n"

            "%temp_data_var_loc   = OpLoad                  %${baseVec}                    %input_data_var_loc\n"
            "                       ${storeOp}              %output_data_var_loc  ${args}  %temp_data_var_loc\n");
        break;
    }
    case TypePunningTestCases::COPY_TO_SAME_SIZE_TYPES:
    {
        main += std::string("%id_loc              = OpAccessChain           %uint32_input_ptr               %id        "
                            "    %c_uint32_0\n"
                            "%x                   = OpLoad                  %uint32                         %id_loc\n"

                            "%input_data_var_loc  = OpAccessChain           %storage_buffer_${baseType}_ptr            "
                            "    %input_data_var          %c_uint32_0 %x\n"
                            "%output_data_var_loc = OpUntypedAccessChainKHR %storage_buffer_untyped_ptr     "
                            "%output_buffer %output_data_untyped_var %c_uint32_0 %x\n"

                            "${copyOp}\n");
        break;
    }
    case TypePunningTestCases::COPY_TO_SCALAR_VECTOR:
    {
        main += std::string("%input_data_var_loc  = OpAccessChain           %storage_buffer_${baseType}_ptr "
                            "%input_data_var %c_uint32_0\n"
                            "%output_data_var_loc = OpUntypedAccessChainKHR %storage_buffer_untyped_ptr     "
                            "%output_buffer  %output_data_untyped_var %c_uint32_0\n"

                            "${copyOp}\n");
        break;
    }
    case TypePunningTestCases::COPY_TO_VECTOR_SCALAR:
    {
        main += std::string("%input_data_var_loc  = OpAccessChain           %storage_buffer_${baseVec}_ptr "
                            "%input_data_var %c_uint32_0\n"
                            "%output_data_var_loc = OpUntypedAccessChainKHR %storage_buffer_untyped_ptr    "
                            "%output_buffer  %output_data_untyped_var %c_uint32_0\n"

                            "${copyOp}\n");
        break;
    }
    case TypePunningTestCases::COPY_FROM_SCALAR_VECTOR:
    {
        main += std::string("%input_data_var_loc  = OpUntypedAccessChainKHR %storage_buffer_untyped_ptr     "
                            "%input_buffer    %input_data_untyped_var %c_uint32_0\n"
                            "%output_data_var_loc = OpAccessChain           %storage_buffer_${otherVec}_ptr "
                            "%output_data_var %c_uint32_0\n"

                            "${copyOp}\n");
        break;
    }
    case TypePunningTestCases::COPY_FROM_VECTOR_SCALAR:
    {
        main += std::string("%input_data_var_loc  = OpUntypedAccessChainKHR %storage_buffer_untyped_ptr     "
                            "%input_buffer    %input_data_untyped_var %c_uint32_0\n"
                            "%output_data_var_loc = OpAccessChain           %storage_buffer_${otherType}_ptr "
                            "%output_data_var %c_uint32_0\n"

                            "${copyOp}\n");
        break;
    }
    case TypePunningTestCases::MULTIPLE_ACCESS_CHAINS:
    {
        main += std::string(
            "%input_data_var_loc  = OpUntypedAccessChainKHR %storage_buffer_untyped_ptr         %input_buffer        "
            "%input_data_untyped_var\n"
            "%data_var_loc        = OpUntypedAccessChainKHR %storage_buffer_untyped_ptr         %output_buffer       "
            "%input_data_var_loc\n"
            "%loaded_data         = OpLoad                  %output_buffer                      %data_var_loc\n"
            "%output_data_var_loc = OpAccessChain           %output_buffer_storage_buffer_ptr   %output_data_var\n"
            "                       OpStore                 %output_data_var_loc                %loaded_data\n");
        break;
    }
    case TypePunningTestCases::CUSTOM_STRUCT_TYPE:
    {
        main += std::string(
            "%input_data_var_loc  = OpUntypedAccessChainKHR %storage_buffer_untyped_ptr         %output_buffer       "
            "%input_data_untyped_var\n"
            "%loaded_input        = OpLoad                  %output_buffer                      %input_data_var_loc\n"
            "%output_data_var_loc = OpAccessChain           %output_buffer_storage_buffer_ptr   %output_data_var\n"
            "                       OpStore                 %output_data_var_loc                %loaded_input\n");
        break;
    }
    default:
    {
        DE_ASSERT(0);
        DE_FATAL("Unknown test case.");
        break;
    }
    }

    main += std::string("                     OpReturn\n"
                        "                     OpFunctionEnd\n");

    return main;
}

std::string createShaderMain(POINTER_TEST_CASE testCase)
{
    std::string main = std::string("%main       = OpFunction %void None %void_func\n"
                                   "%label_main = OpLabel\n");

    switch (testCase)
    {
    case PointerTestCases::OP_BITCAST_FROM_UNTYPED_PHYSICAL_STORAGE:
    {
        main += std::string(
            "%input_ptr  = OpAccessChain           %data_buffer_phys_ptr_ptr  %all_data_var %c_uint32_0\n"
            "%input      = OpLoad                  %data_buffer_phys_ptr      %input_ptr\n"
            "%input_loc  = OpUntypedAccessChainKHR %untyped_phys_ptr          %data_buffer  %input      %c_uint32_0\n"

            "%output_ptr = OpAccessChain            %data_buffer_phys_ptr_ptr %all_data_var %c_uint32_1\n"
            "%output     = OpLoad                   %data_buffer_phys_ptr     %output_ptr\n"
            "%output_loc = OpAccessChain            %${baseType}_phys_ptr     %output       %c_uint32_0\n"

            "%bitcasted     = OpBitcast %${baseType}_phys_ptr %input_loc\n"
            "%bitcasted_val = OpLoad    %${baseType}          %bitcasted      Aligned ${alignment}\n"
            "                 OpStore   %output_loc           %bitcasted_val  Aligned ${alignment}\n");
        break;
    }
    case PointerTestCases::OP_BITCAST_TO_UNTYPED_PHYSICAL_STORAGE:
    {
        main += std::string(
            "%input_ptr  = OpAccessChain           %data_buffer_phys_ptr_ptr  %all_data_var %c_uint32_0\n"
            "%input      = OpLoad                  %data_buffer_phys_ptr      %input_ptr\n"
            "%input_loc  = OpAccessChain           %${baseType}_phys_ptr      %input        %c_uint32_0\n"

            "%output_ptr = OpAccessChain            %data_buffer_phys_ptr_ptr %all_data_var %c_uint32_1\n"
            "%output     = OpLoad                   %data_buffer_phys_ptr     %output_ptr\n"
            "%output_loc = OpUntypedAccessChainKHR  %untyped_phys_ptr         %data_buffer  %output     %c_uint32_0\n"

            "%bitcasted     = OpBitcast %untyped_phys_ptr     %input_loc\n"
            "%bitcasted_val = OpLoad    %${baseType}          %bitcasted      Aligned ${alignment}\n"
            "                 OpStore   %output_loc           %bitcasted_val  Aligned ${alignment}\n");
        break;
    }
    case PointerTestCases::OP_SELECT_PHYSICAL_STORAGE:
    {
        main += std::string(
            "%input_0_ptr = OpAccessChain           %data_buffer_phys_ptr_ptr %all_data_var %c_uint32_0\n"
            "%input_0     = OpLoad                  %data_buffer_phys_ptr     %input_0_ptr\n"
            "%input_0_loc = OpUntypedAccessChainKHR %untyped_phys_ptr         %data_buffer  %input_0    %c_uint32_0\n"

            "%input_1_ptr = OpAccessChain           %data_buffer_phys_ptr_ptr %all_data_var %c_uint32_1\n"
            "%input_1     = OpLoad                  %data_buffer_phys_ptr     %input_1_ptr\n"
            "%input_1_loc = OpUntypedAccessChainKHR %untyped_phys_ptr         %data_buffer  %input_1    %c_uint32_0\n"

            "%output_ptr = OpAccessChain            %data_buffer_phys_ptr_ptr %all_data_var %c_uint32_2\n"
            "%output     = OpLoad                   %data_buffer_phys_ptr     %output_ptr\n"
            "%output_loc = OpUntypedAccessChainKHR  %untyped_phys_ptr         %data_buffer  %output     %c_uint32_0\n"

            "%push_const_loc   = OpAccessChain           %uint32_push_constant_ptr                    "
            "%push_constant_var        %c_uint32_0\n"
            "%condition_int    = OpLoad                  %uint32                         %push_const_loc\n"
            "%condition_bool   = OpIEqual                %bool            %condition_int %c_uint32_1\n"

            "%selected_phys_ptr = OpSelect %untyped_phys_ptr %condition_bool    %input_0_loc %input_1_loc\n"
            "%selected_val      = OpLoad   %${baseType}      %selected_phys_ptr Aligned ${alignment}\n"
            "                     OpStore  %output_loc       %selected_val      Aligned ${alignment}\n");
        break;
    }
    case PointerTestCases::OP_PHI_PHYSICAL_STORAGE:
    {
        main += std::string(
            "%input_0_ptr = OpAccessChain           %data_buffer_phys_ptr_ptr %all_data_var %c_uint32_0\n"
            "%input_0     = OpLoad                  %data_buffer_phys_ptr     %input_0_ptr\n"
            "%input_0_loc = OpUntypedAccessChainKHR %untyped_phys_ptr         %data_buffer  %input_0    %c_uint32_0\n"

            "%input_1_ptr = OpAccessChain           %data_buffer_phys_ptr_ptr %all_data_var %c_uint32_1\n"
            "%input_1     = OpLoad                  %data_buffer_phys_ptr     %input_1_ptr\n"
            "%input_1_loc = OpUntypedAccessChainKHR %untyped_phys_ptr         %data_buffer  %input_1    %c_uint32_0\n"

            "%output_ptr = OpAccessChain            %data_buffer_phys_ptr_ptr %all_data_var %c_uint32_2\n"
            "%output     = OpLoad                   %data_buffer_phys_ptr     %output_ptr\n"
            "%output_loc = OpUntypedAccessChainKHR  %untyped_phys_ptr         %data_buffer  %output     %c_uint32_0\n"

            "%push_const_loc   = OpAccessChain           %uint32_push_constant_ptr                    "
            "%push_constant_var        %c_uint32_0\n"
            "%condition_int    = OpLoad                  %uint32                         %push_const_loc\n"
            "%condition_bool   = OpIEqual                %bool            %condition_int %c_uint32_1\n"

            "                OpSelectionMerge       %end_label      None\n"
            "                OpBranchConditional    %condition_bool %take_input_0 %take_input_1\n"
            "%take_input_0 = OpLabel\n"
            "                OpBranch               %end_label\n"
            "%take_input_1 = OpLabel\n"
            "                OpBranch               %end_label\n"
            "%end_label    = OpLabel\n"

            "%selected_phys_ptr = OpPhi    %untyped_phys_ptr %input_0_loc %take_input_0 %input_1_loc %take_input_1\n"
            "%selected_val      = OpLoad   %${baseType}      %selected_phys_ptr Aligned ${alignment}\n"
            "                     OpStore  %output_loc       %selected_val      Aligned ${alignment}\n");
        break;
    }
    case PointerTestCases::OP_FUNCTION_CALL_PHYSICAL_STORAGE:
    {
        main += std::string(
            "%input_ptr  = OpAccessChain           %data_buffer_phys_ptr_ptr  %all_data_var %c_uint32_0\n"
            "%input      = OpLoad                  %data_buffer_phys_ptr      %input_ptr\n"
            "%input_loc  = OpUntypedAccessChainKHR %untyped_phys_ptr          %data_buffer  %input      %c_uint32_0\n"

            "%output_ptr = OpAccessChain            %data_buffer_phys_ptr_ptr %all_data_var %c_uint32_1\n"
            "%output     = OpLoad                   %data_buffer_phys_ptr     %output_ptr\n"
            "%output_loc = OpUntypedAccessChainKHR  %untyped_phys_ptr         %data_buffer  %output     %c_uint32_0\n"

            "%returned_phys_ptr = OpFunctionCall    %untyped_phys_ptr         %simple_function %input_loc\n"
            "%returned_val      = OpLoad            %${baseType}      %returned_phys_ptr Aligned ${alignment}\n"
            "                     OpStore           %output_loc       %returned_val      Aligned ${alignment}\n");
        break;
    }
    case PointerTestCases::OP_PTR_ACCESS_CHAIN_PHYSICAL_STORAGE:
    {
        main += std::string(
            "%id_loc     = OpAccessChain            %uint32_input_ptr          %id     %c_uint32_0\n"
            "%x          = OpLoad                   %uint32                    %id_loc\n"

            "%input_ptr  = OpAccessChain            %data_buffer_phys_ptr_ptr  %all_data_var %c_uint32_0\n"
            "%input      = OpLoad                   %data_buffer_phys_ptr      %input_ptr\n"
            "%input_loc  = OpUntypedAccessChainKHR  %untyped_phys_ptr          %data_buffer  %input      %c_uint32_0\n"

            "%output_ptr = OpAccessChain            %data_buffer_phys_ptr_ptr %all_data_var %c_uint32_1\n"
            "%output     = OpLoad                   %data_buffer_phys_ptr     %output_ptr\n"
            "%output_loc = OpUntypedAccessChainKHR  %untyped_phys_ptr         %data_buffer  %output      %c_uint32_0\n"

            "%input_loc_0 = OpUntypedAccessChainKHR      %untyped_phys_ptr    %data_buffer  %input_loc   %c_uint32_0   "
            "%c_uint32_0\n"
            "%input_loc_x = OpUntypedPtrAccessChainKHR   %untyped_phys_ptr    %data_buffer  %input_loc_0 %x\n"

            "%accessed_val = OpLoad            %${baseType}      %input_loc_x  Aligned ${alignment}\n"
            "                OpStore           %output_loc       %accessed_val Aligned ${alignment}\n");
        break;
    }
    case PointerTestCases::OP_SELECT_VARIABLE_PTR:
    {
        main += std::string(
            "%push_const_loc   = OpAccessChain           %uint32_push_constant_ptr                    "
            "%push_constant_var        %c_uint32_0\n"
            "%condition_int    = OpLoad                  %uint32                         %push_const_loc\n"
            "%condition_bool   = OpIEqual                %bool            %condition_int %c_uint32_1\n"
            "%input_loc_first  = OpUntypedAccessChainKHR %storage_buffer_untyped_ptr     %input_buffer_0    "
            "%input_data_0_untyped_var %c_uint32_0\n"
            "%input_loc_second = OpUntypedAccessChainKHR %storage_buffer_untyped_ptr     %input_buffer_1    "
            "%input_data_1_untyped_var %c_uint32_0\n"
            "%output_loc       = OpAccessChain           %${baseType}_storage_buffer_ptr                    "
            "%output_data_var          %c_uint32_0\n"

            "%selected_ptr     = OpSelect %storage_buffer_untyped_ptr %condition_bool %input_loc_first "
            "%input_loc_second\n"

            "%selected_ptr_loc = OpLoad  %${baseType} %selected_ptr\n"
            "                    OpStore %output_loc  %selected_ptr_loc\n");
        break;
    }
    case PointerTestCases::OP_PHI_VARIABLE_PTR:
    {
        main += std::string(
            "%push_const_loc   = OpAccessChain           %uint32_push_constant_ptr                    "
            "%push_constant_var        %c_uint32_0\n"
            "%condition_int    = OpLoad                  %uint32                         %push_const_loc\n"
            "%condition_bool   = OpIEqual                %bool            %condition_int %c_uint32_1\n"
            "%input_loc_first  = OpUntypedAccessChainKHR %storage_buffer_untyped_ptr     %input_buffer_0    "
            "%input_data_0_untyped_var %c_uint32_0\n"
            "%input_loc_second = OpUntypedAccessChainKHR %storage_buffer_untyped_ptr     %input_buffer_1    "
            "%input_data_1_untyped_var %c_uint32_0\n"
            "%output_loc       = OpAccessChain           %${baseType}_storage_buffer_ptr                    "
            "%output_data_var          %c_uint32_0\n"

            "                    OpSelectionMerge        %end_label                      None\n"
            "                    OpBranchConditional     %condition_bool                 %take_input_0      "
            "%take_input_1\n"
            "%take_input_0     = OpLabel\n"
            "                    OpBranch                %end_label\n"
            "%take_input_1     = OpLabel\n"
            "                    OpBranch                %end_label\n"
            "%end_label        = OpLabel\n"

            "%selected_ptr     = OpPhi                   %storage_buffer_untyped_ptr    %input_loc_first   "
            "%take_input_0              %input_loc_second   %take_input_1\n"
            "%selected_ptr_loc = OpLoad                  %${baseType}                   %selected_ptr\n"
            "                    OpStore                 %output_loc                    %selected_ptr_loc\n");
        break;
    }
    case PointerTestCases::OP_PTR_EQUAL_VARIABLE_PTR:
    {
        main += std::string(
            "${mainLogic}\n"
            "%output_loc       = OpAccessChain           %output_uint32_storage_buffer_ptr                    "
            "                    %output_data_var        %c_uint32_0\n"

            "%selected         = OpSelect                %uint32  %are_equal %c_uint32_1 %c_uint32_0\n"
            "                    OpStore                 %output_loc                     %selected\n");
        break;
    }
    case PointerTestCases::OP_PTR_NOT_EQUAL_VARIABLE_PTR:
    {
        main += std::string(
            "${mainLogic}\n"
            "%output_loc       = OpAccessChain           %output_uint32_storage_buffer_ptr                    "
            "                    %output_data_var        %c_uint32_0\n"

            "%selected         = OpSelect                %uint32  %are_equal %c_uint32_1 %c_uint32_0\n"
            "                    OpStore                 %output_loc                     %selected\n");
        break;
    }
    case PointerTestCases::OP_PTR_DIFF_VARIABLE_PTR:
    {
        main += std::string(
            "${mainLogic}\n"
            "%output_loc           = OpAccessChain              %uint32_storage_buffer_ptr                            "
            "%output_data_var        %c_uint32_0\n"

            "%ptr_diff_value       = OpPtrDiff                  %uint32                         %input_loc_second_ptr "
            "%input_loc_first_ptr\n"
            "                        OpStore                    %output_loc                     %ptr_diff_value\n");
        break;
    }
    case PointerTestCases::OP_PTR_ACCESS_CHAIN_VARIABLE_PTR:
    {
        main += std::string(
            "%id_loc          = OpAccessChain %uint32_input_ptr %id %c_uint32_0\n"
            "%x               = OpLoad %uint32 %id_loc\n"

            "%input_loc       = OpUntypedAccessChainKHR    %strided_storage_buffer_untyped_ptr  %input_buffer      "
            "                   %input_data_untyped_var    %c_uint32_0  %c_uint32_0\n"
            "%input_loc_ptr   = OpUntypedPtrAccessChainKHR %storage_buffer_untyped_ptr     %${baseType}      "
            "                   %input_loc                 %x\n"
            "%output_loc      = OpAccessChain              %${baseType}_storage_buffer_ptr                    "
            "                   %output_data_var           %c_uint32_0 %x\n"

            "%input_ptr_loc   = OpLoad                     %${baseType}                    %input_loc_ptr\n"
            "                   OpStore                    %output_loc                     %input_ptr_loc\n");
        break;
    }
    case PointerTestCases::OP_FUNCTION_CALL_VARIABLE_PTR:
    {
        main += std::string(
            "%input_array_loc  = OpUntypedAccessChainKHR %storage_buffer_untyped_ptr     %input_buffer      "
            "%input_data_untyped_var   %c_uint32_0\n"
            "%input_loc        = OpUntypedAccessChainKHR %storage_buffer_untyped_ptr     %array_32      "
            "%input_array_loc   %c_uint32_4\n"
            "%output_loc       = OpAccessChain           %${baseType}_storage_buffer_ptr                    "
            "%output_data_var          %c_uint32_0\n"

            "%returned_ptr     = OpFunctionCall          %storage_buffer_untyped_ptr     %simple_function   "
            "%input_loc\n"

            "%returned_ptr_loc = OpLoad                  %${baseType}                    %returned_ptr\n"
            "                    OpStore                 %output_loc                     %returned_ptr_loc\n");
        break;
    }
    case PointerTestCases::MULTIPLE_ACCESS_CHAINS_VARIABLE_PTR:
    {
        main +=
            std::string("%output_loc       = OpAccessChain          %other_type_storage_buffer_ptr "
                        "%output_data_var          %c_uint32_0\n"
                        "%input_array_loc  = OpUntypedAccessChainKHR %storage_buffer_untyped_ptr %input_buffer "
                        "%input_data_var %c_uint32_0\n"
                        "%elem_4th_first   = OpUntypedAccessChainKHR %storage_buffer_untyped_ptr "
                        "%array_first_32 %input_array_loc %c_uint32_4\n"
                        "%elem_8th_second  = OpUntypedAccessChainKHR %storage_buffer_untyped_ptr "
                        "%array_second_32 %elem_4th_first %c_uint32_8\n"
                        "%elem_loc         = OpLoad                  %${otherType}                %elem_8th_second\n"
                        "                    OpStore                 %output_loc                  %elem_loc\n");
        break;
    }
    case PointerTestCases::FUNCTION_VARIABLE_VARIABLE_PTR:
    {
        main += std::string(
            "%output_copy_function_var = OpVariable                 %storage_buffer_untyped_ptr_function_ptr Function\n"

            "%push_const_loc           = OpAccessChain           %uint32_push_constant_ptr                    "
            "%push_constant_var        %c_uint32_0\n"
            "%condition_int            = OpLoad                  %uint32                         %push_const_loc\n"
            "%condition_bool           = OpIEqual                %bool            %condition_int %c_uint32_1\n"
            "%input_loc_first          = OpUntypedAccessChainKHR    %storage_buffer_untyped_ptr              "
            "%input_buffer_0           %input_data_0_untyped_var %c_uint32_0\n"
            "%input_loc_second         = OpUntypedAccessChainKHR    %storage_buffer_untyped_ptr              "
            "%input_buffer_1           %input_data_1_untyped_var %c_uint32_0\n"
            "%output_loc               = OpAccessChain              %${baseType}_storage_buffer_ptr                    "
            "                %output_data_var          %c_uint32_0\n"

            "%selected_ptr             = OpSelect                   %storage_buffer_untyped_ptr              "
            "%condition_bool              %input_loc_first          %input_loc_second\n"

            "                            OpStore                    %output_copy_function_var                "
            "%selected_ptr\n"
            "%output_copy_loc_unty_ptr = OpLoad                     %storage_buffer_untyped_ptr              "
            "%output_copy_function_var\n"
            "%output_copy_loc          = OpLoad                     %${baseType}                             "
            "%output_copy_loc_unty_ptr\n"
            "                            OpStore                    %output_loc                              "
            "%output_copy_loc\n");
        break;
    }
    case PointerTestCases::PRIVATE_VARIABLE_VARIABLE_PTR:
    {
        main += std::string(
            "%push_const_loc           = OpAccessChain              %uint32_push_constant_ptr                    "
            "%push_constant_var        %c_uint32_0\n"
            "%condition_int            = OpLoad                     %uint32          %push_const_loc\n"
            "%condition_bool           = OpIEqual                   %bool            %condition_int %c_uint32_1\n"
            "%input_loc_first          = OpUntypedAccessChainKHR    %storage_buffer_untyped_ptr        "
            "      %input_buffer_0           %input_data_0_untyped_var %c_uint32_0\n"
            "%input_loc_second         = OpUntypedAccessChainKHR    %storage_buffer_untyped_ptr        "
            "      %input_buffer_1           %input_data_1_untyped_var %c_uint32_0\n"
            "%output_loc               = OpAccessChain              %${baseType}_storage_buffer_ptr    "
            "                                %output_data_var          %c_uint32_0\n"

            "%selected_ptr             = OpSelect                   %storage_buffer_untyped_ptr        "
            "      %condition_bool        %input_loc_first          %input_loc_second\n"

            "                            OpStore                    %output_copy_private_var           "
            "      %selected_ptr\n"
            "%output_copy_loc_unty_ptr = OpLoad                     %storage_buffer_untyped_ptr        "
            "      %output_copy_private_var\n"
            "%output_copy_loc          = OpLoad                     %${baseType}                       "
            "      %output_copy_loc_unty_ptr\n"
            "                            OpStore                    %output_loc                        "
            "      %output_copy_loc\n");
        break;
    }
    case PointerTestCases::WORKGROUP_MEMORY_VARIABLE_PTR:
    {
        main += std::string(
            "%id_loc          = OpAccessChain %uint32_input_ptr %id %c_uint32_0\n"
            "%x               = OpLoad        %uint32           %id_loc\n"

            "%input_loc       = OpAccessChain %${baseType}_storage_buffer_ptr %input_data_var %c_uint32_0 %x\n"
            "%input_elem      = OpLoad        %${baseType}                    %input_loc\n"

            "%shared_loc      = OpUntypedAccessChainKHR %workgroup_untyped_ptr %shared_buffer %workgroup_untyped_var "
            "                   %c_uint32_0 %x\n"
            "                   OpStore                 %shared_loc            %input_elem\n"

            "                   OpControlBarrier %c_uint32_2 %c_uint32_2 %c_uint32_264\n"

            "%output_elem     = OpLoad        %${baseType}                    %shared_loc\n"
            "%output_loc      = OpAccessChain %${baseType}_storage_buffer_ptr %output_data_var %c_uint32_0 %x\n"
            "                   OpStore       %output_loc                     %output_elem\n");
        break;
    }
    default:
    {
        DE_ASSERT(0);
        DE_FATAL("Unknown test case.");
        break;
    }
    }

    main += std::string("                OpReturn\n"
                        "                OpFunctionEnd\n");

    return main;
}

std::string createShaderMain(MEMORY_INTERPRETATION_TEST_CASE testCase, bool read)
{
    std::string main =
        std::string("%main               = OpFunction %void None %void_func\n"
                    "%label_main         = OpLabel\n"
                    "%gid                = OpLoad %vec3_uint32 %id\n"
                    "%gid_x              = OpCompositeExtract %uint32 %gid 0\n"
                    "%index_access       = OpAccessChain %uint32_storage_ptr %indices_var %c_uint32_0 %gid_x\n"
                    "%index              = OpLoad %uint32 %index_access\n");

    if (read)
    {
        switch (testCase)
        {
        case MemoryInterpretationTestCases::LARGE_ARRAY_STRIDE:
        {
            main += std::string("%in_access    = OpUntypedAccessChainKHR %untyped_ptr %large_array %in_var %index\n"
                                "%in_load      = OpLoad %uint32 %in_access\n");

            break;
        }
        case MemoryInterpretationTestCases::NON_ZERO_OFFSET:
        {
            main += std::string(
                "%in_access    = OpUntypedAccessChainKHR %untyped_ptr %test_array %in_var %index %c_uint32_1\n"
                "%in_load      = OpLoad %uint32 %in_access\n");
            break;
        }
        case MemoryInterpretationTestCases::MIXED_OFFSETS:
        {
            main +=
                std::string("                OpSelectionMerge %merge None\n"
                            "                OpSwitch %index %merge 0 %case_0 1 %case_1 2 %case_2 3 %case_3\n"
                            "\n"
                            "%case_0       = OpLabel\n"
                            "%in_access_0  = OpUntypedAccessChainKHR %untyped_ptr %test_struct %in_var %c_uint32_0\n"
                            "%in_load_0    = OpLoad %uint32 %in_access_0\n"
                            "                OpBranch %merge\n"
                            "\n"
                            "%case_1       = OpLabel\n"
                            "%in_access_1  = OpUntypedAccessChainKHR %untyped_ptr %test_struct %in_var %c_uint32_1\n"
                            "%in_load_1    = OpLoad %uint32 %in_access_1\n"
                            "                OpBranch %merge\n"
                            "\n"
                            "%case_2       = OpLabel\n"
                            "%in_access_2  = OpUntypedAccessChainKHR %untyped_ptr %test_struct %in_var %c_uint32_2\n"
                            "%in_load_2    = OpLoad %uint32 %in_access_2\n"
                            "                OpBranch %merge\n"
                            "\n"
                            "%case_3       = OpLabel\n"
                            "%in_access_3  = OpUntypedAccessChainKHR %untyped_ptr %test_struct %in_var %c_uint32_3\n"
                            "%in_load_3    = OpLoad %uint32 %in_access_3\n"
                            "                OpBranch %merge\n"
                            "\n"
                            "%merge        = OpLabel\n"
                            "%in_load      = OpPhi %uint32 %in_load_0 %case_0 %in_load_1 %case_1 %in_load_2 "
                            "%case_2 %in_load_3 %case_3 %c_uint32_0 %label_main\n");

            break;
        }
        case MemoryInterpretationTestCases::MULTIPLE_ACCESS_CHAINS:
        {
            main += std::string(
                "%in_access_1 = OpUntypedAccessChainKHR %untyped_ptr %type_1 %in_var %index\n"
                "%in_access_2 = OpUntypedAccessChainKHR %untyped_ptr %type_2 %in_access_1 %c_uint32_1 %index\n"
                "%in_access_3 = OpUntypedAccessChainKHR %untyped_ptr %type_3 %in_access_2 %index\n"
                "%in_load     = OpLoad %uint32 %in_access_3\n");

            break;
        }
        case MemoryInterpretationTestCases::SHORT2_NO_STORAGE_CAP:
        {
            main += std::string("%in_access   = OpUntypedAccessChainKHR %untyped_ptr %array %in_var %index\n"
                                "%load        = OpLoad %short2 %in_access\n"
                                "%in_load     = OpBitcast %uint32 %load\n");

            break;
        }
        case MemoryInterpretationTestCases::CHAR4_NO_STORAGE_CAP:
        {
            main += std::string("%in_access   = OpUntypedAccessChainKHR %untyped_ptr %array %in_var %index\n"
                                "%load        = OpLoad %uchar4 %in_access\n"
                                "%in_load     = OpBitcast %uint32 %load\n");

            break;
        }
        case MemoryInterpretationTestCases::CHAR2_16BIT_STORAGE_CAP:
        {
            main += std::string("%mul         = OpIMul %uint32 %index %c_uint32_2\n"
                                "%in_access   = OpUntypedAccessChainKHR %untyped_ptr %uchar2_array %in_var %mul\n"
                                "%load        = OpLoad %uchar2 %in_access\n"
                                "%in_load     = OpBitcast %ushort %load\n"
                                "%out_access  = OpAccessChain %ushort_storage_ptr %out_var %c_uint32_0 %gid_x\n"
                                "               OpStore %out_access %in_load\n");

            break;
        }
        case MemoryInterpretationTestCases::UNTYPED_FROM_TYPED_VAR:
        {
            main += std::string("%in_access    = OpUntypedAccessChainKHR %untyped_ptr %array %in_var %index\n"
                                "%in_load      = OpLoad %uint32 %in_access\n");

            break;
        }
        case MemoryInterpretationTestCases::UNTYPED_FROM_TYPED_ACCESS_CHAIN:
        {
            main += std::string("%typed_access = OpAccessChain %ptr_array_storage %in_var %c_uint32_0\n"
                                "%in_access    = OpUntypedAccessChainKHR %untyped_ptr %array %typed_access %index\n"
                                "%in_load      = OpLoad %uint32 %in_access\n");

            break;
        }
        default:
        {
            DE_ASSERT(0);
            DE_FATAL("Unknown test case.");
            break;
        }
        }

        if (testCase != MemoryInterpretationTestCases::CHAR2_16BIT_STORAGE_CAP)
        {
            main += std::string("%out_access   = OpAccessChain %uint32_storage_ptr %out_var %c_uint32_0 %gid_x\n"
                                "                OpStore %out_access %in_load\n");
        }
    }
    else
    {
        if (testCase != MemoryInterpretationTestCases::CHAR2_16BIT_STORAGE_CAP)
        {
            main += std::string("%in_access    = OpAccessChain %uint32_storage_ptr %in_var %c_uint32_0 %gid_x\n"
                                "%in_load      = OpLoad %uint32 %in_access\n");
        }

        switch (testCase)
        {
        case MemoryInterpretationTestCases::LARGE_ARRAY_STRIDE:
        {
            main += std::string("%out_access   = OpUntypedAccessChainKHR %untyped_ptr %large_array %out_var %index\n"
                                "                OpStore %out_access %in_load\n");

            break;
        }
        case MemoryInterpretationTestCases::NON_ZERO_OFFSET:
        {
            main += std::string(
                "%out_access   = OpUntypedAccessChainKHR %untyped_ptr %test_array %out_var %index %c_uint32_1\n"
                "                OpStore %out_access %in_load\n");

            break;
        }
        case MemoryInterpretationTestCases::MIXED_OFFSETS:
        {
            main +=
                std::string("                OpSelectionMerge %merge None\n"
                            "                OpSwitch %index %merge 0 %case_0 1 %case_1 2 %case_2 3 %case_3\n"
                            "\n"
                            "%case_0       = OpLabel\n"
                            "%out_access_0 = OpUntypedAccessChainKHR %untyped_ptr %test_struct %out_var %c_uint32_0\n"
                            "                OpStore %out_access_0 %in_load\n"
                            "                OpBranch %merge\n"
                            "\n"
                            "%case_1       = OpLabel\n"
                            "%out_access_1 = OpUntypedAccessChainKHR %untyped_ptr %test_struct %out_var %c_uint32_1\n"
                            "                OpStore %out_access_1 %in_load\n"
                            "                OpBranch %merge\n"
                            "\n"
                            "%case_2       = OpLabel\n"
                            "%out_access_2 = OpUntypedAccessChainKHR %untyped_ptr %test_struct %out_var %c_uint32_2\n"
                            "                OpStore %out_access_2 %in_load\n"
                            "                OpBranch %merge\n"
                            "\n"
                            "%case_3       = OpLabel\n"
                            "%out_access_3 = OpUntypedAccessChainKHR %untyped_ptr %test_struct %out_var %c_uint32_3\n"
                            "                OpStore %out_access_3 %in_load\n"
                            "                OpBranch %merge\n"
                            "\n"
                            "%merge        = OpLabel\n");

            break;
        }
        case MemoryInterpretationTestCases::MULTIPLE_ACCESS_CHAINS:
        {
            main += std::string(
                "%out_access_1 = OpUntypedAccessChainKHR %untyped_ptr %type_1 %out_var %index\n"
                "%out_access_2 = OpUntypedAccessChainKHR %untyped_ptr %type_2 %out_access_1 %c_uint32_1 %index\n"
                "%out_access_3 = OpUntypedAccessChainKHR %untyped_ptr %type_3 %out_access_2 %index\n"
                "                OpStore %out_access_3 %in_load\n");

            break;
        }
        case MemoryInterpretationTestCases::SHORT2_NO_STORAGE_CAP:
        {
            main += std::string("%out_access   = OpUntypedAccessChainKHR %untyped_ptr %array %out_var %index\n"
                                "%cast         = OpBitcast %short2 %in_load\n"
                                "                OpStore %out_access %cast\n");

            break;
        }
        case MemoryInterpretationTestCases::CHAR4_NO_STORAGE_CAP:
        {
            main += std::string("%out_access   = OpUntypedAccessChainKHR %untyped_ptr %array %out_var %index\n"
                                "%cast         = OpBitcast %uchar4 %in_load\n"
                                "                OpStore %out_access %cast\n");

            break;
        }
        case MemoryInterpretationTestCases::CHAR2_16BIT_STORAGE_CAP:
        {
            main += std::string("%in_access    = OpAccessChain %ushort_storage_ptr %in_var %c_uint32_0 %gid_x\n"
                                "%in_load      = OpLoad %ushort %in_access\n"
                                "%mul          = OpIMul %uint32 %index %c_uint32_2\n"
                                "%out_access   = OpUntypedAccessChainKHR %untyped_ptr %uchar2_array %out_var %mul\n"
                                "%cast         = OpBitcast %uchar2 %in_load\n"
                                "                OpStore %out_access %cast\n");

            break;
        }
        case MemoryInterpretationTestCases::UNTYPED_FROM_TYPED_VAR:
        {
            main += std::string("%out_access   = OpUntypedAccessChainKHR %untyped_ptr %array %out_var %index\n"
                                "                OpStore %out_access %in_load\n");

            break;
        }
        case MemoryInterpretationTestCases::UNTYPED_FROM_TYPED_ACCESS_CHAIN:
        {
            main += std::string("%typed_access = OpAccessChain %ptr_array_storage %out_var %c_uint32_0\n"
                                "%out_access   = OpUntypedAccessChainKHR %untyped_ptr %array %typed_access %index\n"
                                "                OpStore %out_access %in_load\n");

            break;
        }
        default:
        {
            DE_ASSERT(0);
            DE_FATAL("Unknown test case.");
            break;
        }
        }
    }

    main += std::string("                OpReturn\n"
                        "                OpFunctionEnd\n");

    return main;
}

std::string createShaderMain(BLOCK_ARRAY_TEST_CASE testCase, std::map<std::string, std::string> &specMap)
{
    std::string main = std::string("%main       = OpFunction %void None %void_func\n"
                                   "%label_main = OpLabel\n"
                                   "%value_var  = OpVariable %uint32_func_ptr Function %c_uint32_0\n"
                                   "%gid        = OpLoad %vec3_uint32 %id\n"
                                   "%gid_x      = OpCompositeExtract %uint32 %gid 0\n"
                                   "%index_gep  = OpAccessChain %uint32_storage_ptr %uni_var %c_uint32_0 %gid_x\n"
                                   "%index      = OpLoad %uint32 %index_gep\n"
                                   "%gid_x_p1   = OpIAdd %uint32 %gid_x %c_uint32_1\n"
                                   "%next_gid_x = OpUMod %uint32 %gid_x_p1 %c_uint32_4\n"
                                   "%less       = OpULessThanEqual %bool %gid_x %index\n");

    switch (testCase)
    {
    case BlockArrayTestCases::BASIC:
    {
        specMap["base_gep_0"] = "";
        specMap["base_gep_1"] = "";
        specMap["base_gep_2"] = "";
        specMap["base_gep_3"] = "";
        specMap["gep_0"]      = "OpUntypedAccessChainKHR %ptr_no_stride %block_array %in_var %gid_x %c_uint32_0 %index";
        specMap["gep_1"]      = "OpUntypedAccessChainKHR %ptr_no_stride %block_array %in_var %gid_x %c_uint32_0 %index";
        specMap["gep_2"]      = "OpUntypedAccessChainKHR %ptr_no_stride %block_array %in_var %gid_x %c_uint32_0 %index";
        specMap["gep_3"]      = "OpUntypedAccessChainKHR %ptr_no_stride %block_array %in_var %gid_x %c_uint32_0 %index";
        break;
    }
    case BlockArrayTestCases::REINTERPRET_BLOCK_NORMAL_ACCESS_CHAIN:
    {
        specMap["base_gep_0"] = "%base_gep_0 = OpAccessChain %block0_storage_ptr %in_var %gid_x";
        specMap["base_gep_1"] = "%base_gep_1 = OpAccessChain %block0_storage_ptr %in_var %gid_x";
        specMap["base_gep_2"] = "%base_gep_2 = OpAccessChain %block0_storage_ptr %in_var %gid_x";
        specMap["base_gep_3"] = "%base_gep_3 = OpAccessChain %block0_storage_ptr %in_var %gid_x";
        specMap["gep_0"] =
            "OpUntypedAccessChainKHR %ptr_no_stride %in_block_1 %base_gep_0 %c_uint32_0 %index %c_uint32_0";
        specMap["gep_1"] =
            "OpUntypedAccessChainKHR %ptr_no_stride %in_block_1 %base_gep_1 %c_uint32_0 %index %c_uint32_0";
        specMap["gep_2"] =
            "OpUntypedAccessChainKHR %ptr_no_stride %in_block_1 %base_gep_2 %c_uint32_0 %index %c_uint32_0";
        specMap["gep_3"] =
            "OpUntypedAccessChainKHR %ptr_no_stride %in_block_1 %base_gep_3 %c_uint32_0 %index %c_uint32_0";
        break;
    }
    case BlockArrayTestCases::REINTERPRET_BLOCK_NORMAL_PTR_ACCESS_CHAIN:
    {
        specMap["base_gep_0"] = std::string(
            "%base_gep_0  = OpAccessChain %block0_storage_ptr %in_var %gid_x\n"
            "%extra_gep_0 = OpUntypedAccessChainKHR %ptr_16_stride %in_block_1 %base_gep_0 %c_uint32_0 %c_uint32_0");
        specMap["base_gep_1"] = std::string(
            "%base_gep_1  = OpAccessChain %block0_storage_ptr %in_var %gid_x\n"
            "%extra_gep_1 = OpUntypedAccessChainKHR %ptr_16_stride %in_block_1 %base_gep_1 %c_uint32_0 %c_uint32_0");
        specMap["base_gep_2"] = std::string(
            "%base_gep_2  = OpAccessChain %block0_storage_ptr %in_var %gid_x\n"
            "%extra_gep_2 = OpUntypedAccessChainKHR %ptr_16_stride %in_block_1 %base_gep_2 %c_uint32_0 %c_uint32_0");
        specMap["base_gep_3"] = std::string(
            "%base_gep_3  = OpAccessChain %block0_storage_ptr %in_var %gid_x\n"
            "%extra_gep_3 = OpUntypedAccessChainKHR %ptr_16_stride %in_block_1 %base_gep_3 %c_uint32_0 %c_uint32_0");
        specMap["gep_0"] = "OpUntypedPtrAccessChainKHR %ptr_no_stride %int4_array %extra_gep_0 %index %c_uint32_0";
        specMap["gep_1"] = "OpUntypedPtrAccessChainKHR %ptr_no_stride %int4_array %extra_gep_1 %index %c_uint32_0";
        specMap["gep_2"] = "OpUntypedPtrAccessChainKHR %ptr_no_stride %int4_array %extra_gep_2 %index %c_uint32_0";
        specMap["gep_3"] = "OpUntypedPtrAccessChainKHR %ptr_no_stride %int4_array %extra_gep_3 %index %c_uint32_0";
        break;
    }
    case BlockArrayTestCases::REINTERPRET_BLOCK_UNTYPED_ACCESS_CHAIN:
    {
        specMap["base_gep_0"] = "%base_gep_0 = OpUntypedAccessChainKHR %ptr_no_stride %block_array %in_var %gid_x";
        specMap["base_gep_1"] = "%base_gep_1 = OpUntypedAccessChainKHR %ptr_no_stride %block_array %in_var %gid_x";
        specMap["base_gep_2"] = "%base_gep_2 = OpUntypedAccessChainKHR %ptr_no_stride %block_array %in_var %gid_x";
        specMap["base_gep_3"] = "%base_gep_3 = OpUntypedAccessChainKHR %ptr_no_stride %block_array %in_var %gid_x";
        specMap["gep_0"] =
            "OpUntypedAccessChainKHR %ptr_no_stride %in_block_1 %base_gep_0 %c_uint32_0 %index %c_uint32_0";
        specMap["gep_1"] =
            "OpUntypedAccessChainKHR %ptr_no_stride %in_block_1 %base_gep_1 %c_uint32_0 %index %c_uint32_0";
        specMap["gep_2"] =
            "OpUntypedAccessChainKHR %ptr_no_stride %in_block_1 %base_gep_2 %c_uint32_0 %index %c_uint32_0";
        specMap["gep_3"] =
            "OpUntypedAccessChainKHR %ptr_no_stride %in_block_1 %base_gep_3 %c_uint32_0 %index %c_uint32_0";
        break;
    }
    case BlockArrayTestCases::REINTERPRET_BLOCK_UNTYPED_PTR_ACCESS_CHAIN:
    {
        specMap["base_gep_0"] = std::string(
            "%base_gep_0  = OpUntypedAccessChainKHR %ptr_no_stride %block_array %in_var %gid_x\n"
            "%extra_gep_0 = OpUntypedAccessChainKHR %ptr_16_stride %in_block_1 %base_gep_0 %c_uint32_0 %c_uint32_0");
        specMap["base_gep_1"] = std::string(
            "%base_gep_1  = OpUntypedAccessChainKHR %ptr_no_stride %block_array %in_var %gid_x\n"
            "%extra_gep_1 = OpUntypedAccessChainKHR %ptr_16_stride %in_block_1 %base_gep_1 %c_uint32_0 %c_uint32_0");
        specMap["base_gep_2"] = std::string(
            "%base_gep_2  = OpUntypedAccessChainKHR %ptr_no_stride %block_array %in_var %gid_x\n"
            "%extra_gep_2 = OpUntypedAccessChainKHR %ptr_16_stride %in_block_1 %base_gep_2 %c_uint32_0 %c_uint32_0");
        specMap["base_gep_3"] = std::string(
            "%base_gep_3  = OpUntypedAccessChainKHR %ptr_no_stride %block_array %in_var %gid_x\n"
            "%extra_gep_3 = OpUntypedAccessChainKHR %ptr_16_stride %in_block_1 %base_gep_3 %c_uint32_0 %c_uint32_0");
        specMap["gep_0"] = "OpUntypedPtrAccessChainKHR %ptr_no_stride %int4_array %extra_gep_0 %index %c_uint32_0";
        specMap["gep_1"] = "OpUntypedPtrAccessChainKHR %ptr_no_stride %int4_array %extra_gep_1 %index %c_uint32_0";
        specMap["gep_2"] = "OpUntypedPtrAccessChainKHR %ptr_no_stride %int4_array %extra_gep_2 %index %c_uint32_0";
        specMap["gep_3"] = "OpUntypedPtrAccessChainKHR %ptr_no_stride %int4_array %extra_gep_3 %index %c_uint32_0";
        break;
    }
    case BlockArrayTestCases::SELECT_BLOCK_NORMAL_ACCESS_CHAIN:
    {
        specMap["base_gep_0"] =
            std::string("%base_gep_0a = OpAccessChain %block0_storage_ptr %in_var %gid_x\n"
                        "%base_gep_0b = OpAccessChain %block0_storage_ptr %in_var %next_gid_x\n"
                        "%sel_0       = OpSelect %block0_storage_ptr %less %base_gep_0a %base_gep_0b\n");
        specMap["base_gep_1"] =
            std::string("%base_gep_1a = OpAccessChain %block0_storage_ptr %in_var %gid_x\n"
                        "%base_gep_1b = OpAccessChain %block0_storage_ptr %in_var %next_gid_x\n"
                        "%sel_1       = OpSelect %block0_storage_ptr %less %base_gep_1a %base_gep_1b\n");
        specMap["base_gep_2"] =
            std::string("%base_gep_2a = OpAccessChain %block0_storage_ptr %in_var %gid_x\n"
                        "%base_gep_2b = OpAccessChain %block0_storage_ptr %in_var %next_gid_x\n"
                        "%sel_2       = OpSelect %block0_storage_ptr %less %base_gep_2a %base_gep_2b\n");
        specMap["base_gep_3"] =
            std::string("%base_gep_3a = OpAccessChain %block0_storage_ptr %in_var %gid_x\n"
                        "%base_gep_3b = OpAccessChain %block0_storage_ptr %in_var %next_gid_x\n"
                        "%sel_3       = OpSelect %block0_storage_ptr %less %base_gep_3a %base_gep_3b\n");
        specMap["gep_0"] = "OpUntypedAccessChainKHR %ptr_no_stride %in_block_1 %sel_0 %c_uint32_0 %index %c_uint32_0";
        specMap["gep_1"] = "OpUntypedAccessChainKHR %ptr_no_stride %in_block_1 %sel_1 %c_uint32_0 %index %c_uint32_0";
        specMap["gep_2"] = "OpUntypedAccessChainKHR %ptr_no_stride %in_block_1 %sel_2 %c_uint32_0 %index %c_uint32_0";
        specMap["gep_3"] = "OpUntypedAccessChainKHR %ptr_no_stride %in_block_1 %sel_3 %c_uint32_0 %index %c_uint32_0";
        break;
    }
    case BlockArrayTestCases::SELECT_BLOCK_NORMAL_PTR_ACCESS_CHAIN:
    {
        specMap["base_gep_0"] = std::string(
            "%base_gep_0a = OpAccessChain %block0_storage_ptr %in_var %gid_x\n"
            "%base_gep_0b = OpAccessChain %block0_storage_ptr %in_var %next_gid_x\n"
            "%sel_0       = OpSelect %block0_storage_ptr %less %base_gep_0a %base_gep_0b\n"
            "%extra_gep_0 = OpUntypedAccessChainKHR %ptr_16_stride %in_block_1 %sel_0 %c_uint32_0 %c_uint32_0\n");
        specMap["base_gep_1"] = std::string(
            "%base_gep_1a = OpAccessChain %block0_storage_ptr %in_var %gid_x\n"
            "%base_gep_1b = OpAccessChain %block0_storage_ptr %in_var %next_gid_x\n"
            "%sel_1       = OpSelect %block0_storage_ptr %less %base_gep_1a %base_gep_1b\n"
            "%extra_gep_1 = OpUntypedAccessChainKHR %ptr_16_stride %in_block_1 %sel_1 %c_uint32_0 %c_uint32_0\n");
        specMap["base_gep_2"] = std::string(
            "%base_gep_2a = OpAccessChain %block0_storage_ptr %in_var %gid_x\n"
            "%base_gep_2b = OpAccessChain %block0_storage_ptr %in_var %next_gid_x\n"
            "%sel_2       = OpSelect %block0_storage_ptr %less %base_gep_2a %base_gep_2b\n"
            "%extra_gep_2 = OpUntypedAccessChainKHR %ptr_16_stride %in_block_1 %sel_2 %c_uint32_0 %c_uint32_0\n");
        specMap["base_gep_3"] = std::string(
            "%base_gep_3a = OpAccessChain %block0_storage_ptr %in_var %gid_x\n"
            "%base_gep_3b = OpAccessChain %block0_storage_ptr %in_var %next_gid_x\n"
            "%sel_3       = OpSelect %block0_storage_ptr %less %base_gep_3a %base_gep_3b\n"
            "%extra_gep_3 = OpUntypedAccessChainKHR %ptr_16_stride %in_block_1 %sel_3 %c_uint32_0 %c_uint32_0\n");
        specMap["gep_0"] = "OpUntypedPtrAccessChainKHR %ptr_no_stride %int4_array %extra_gep_0 %index %c_uint32_0";
        specMap["gep_1"] = "OpUntypedPtrAccessChainKHR %ptr_no_stride %int4_array %extra_gep_1 %index %c_uint32_0";
        specMap["gep_2"] = "OpUntypedPtrAccessChainKHR %ptr_no_stride %int4_array %extra_gep_2 %index %c_uint32_0";
        specMap["gep_3"] = "OpUntypedPtrAccessChainKHR %ptr_no_stride %int4_array %extra_gep_3 %index %c_uint32_0";
        break;
    }
    case BlockArrayTestCases::SELECT_BLOCK_UNTYPED_ACCESS_CHAIN:
    {
        specMap["base_gep_0"] =
            std::string("%base_gep_0a = OpUntypedAccessChainKHR %ptr_no_stride %block_array %in_var %gid_x\n"
                        "%base_gep_0b = OpUntypedAccessChainKHR %ptr_no_stride %block_array %in_var %next_gid_x\n"
                        "%sel_0       = OpSelect %ptr_no_stride %less %base_gep_0a %base_gep_0b\n");
        specMap["base_gep_1"] =
            std::string("%base_gep_1a = OpUntypedAccessChainKHR %ptr_no_stride %block_array %in_var %gid_x\n"
                        "%base_gep_1b = OpUntypedAccessChainKHR %ptr_no_stride %block_array %in_var %next_gid_x\n"
                        "%sel_1       = OpSelect %ptr_no_stride %less %base_gep_1a %base_gep_1b\n");
        specMap["base_gep_2"] =
            std::string("%base_gep_2a = OpUntypedAccessChainKHR %ptr_no_stride %block_array %in_var %gid_x\n"
                        "%base_gep_2b = OpUntypedAccessChainKHR %ptr_no_stride %block_array %in_var %next_gid_x\n"
                        "%sel_2       = OpSelect %ptr_no_stride %less %base_gep_2a %base_gep_2b\n");
        specMap["base_gep_3"] =
            std::string("%base_gep_3a = OpUntypedAccessChainKHR %ptr_no_stride %block_array %in_var %gid_x\n"
                        "%base_gep_3b = OpUntypedAccessChainKHR %ptr_no_stride %block_array %in_var %next_gid_x\n"
                        "%sel_3       = OpSelect %ptr_no_stride %less %base_gep_3a %base_gep_3b\n");
        specMap["gep_0"] = "OpUntypedAccessChainKHR %ptr_no_stride %in_block_1 %sel_0 %c_uint32_0 %index %c_uint32_0";
        specMap["gep_1"] = "OpUntypedAccessChainKHR %ptr_no_stride %in_block_1 %sel_1 %c_uint32_0 %index %c_uint32_0";
        specMap["gep_2"] = "OpUntypedAccessChainKHR %ptr_no_stride %in_block_1 %sel_2 %c_uint32_0 %index %c_uint32_0";
        specMap["gep_3"] = "OpUntypedAccessChainKHR %ptr_no_stride %in_block_1 %sel_3 %c_uint32_0 %index %c_uint32_0";
        break;
    }
    case BlockArrayTestCases::SELECT_BLOCK_UNTYPED_PTR_ACCESS_CHAIN:
    {
        specMap["base_gep_0"] = std::string(
            "%base_gep_0a = OpUntypedAccessChainKHR %ptr_no_stride %block_array %in_var %gid_x\n"
            "%base_gep_0b = OpUntypedAccessChainKHR %ptr_no_stride %block_array %in_var %next_gid_x\n"
            "%sel_0       = OpSelect %ptr_no_stride %less %base_gep_0a %base_gep_0b\n"
            "%extra_gep_0 = OpUntypedAccessChainKHR %ptr_16_stride %in_block_1 %sel_0 %c_uint32_0 %c_uint32_0\n");
        specMap["base_gep_1"] = std::string(
            "%base_gep_1a = OpUntypedAccessChainKHR %ptr_no_stride %block_array %in_var %gid_x\n"
            "%base_gep_1b = OpUntypedAccessChainKHR %ptr_no_stride %block_array %in_var %next_gid_x\n"
            "%sel_1       = OpSelect %ptr_no_stride %less %base_gep_1a %base_gep_1b\n"
            "%extra_gep_1 = OpUntypedAccessChainKHR %ptr_16_stride %in_block_1 %sel_1 %c_uint32_0 %c_uint32_0\n");
        specMap["base_gep_2"] = std::string(
            "%base_gep_2a = OpUntypedAccessChainKHR %ptr_no_stride %block_array %in_var %gid_x\n"
            "%base_gep_2b = OpUntypedAccessChainKHR %ptr_no_stride %block_array %in_var %next_gid_x\n"
            "%sel_2       = OpSelect %ptr_no_stride %less %base_gep_2a %base_gep_2b\n"
            "%extra_gep_2 = OpUntypedAccessChainKHR %ptr_16_stride %in_block_1 %sel_2 %c_uint32_0 %c_uint32_0\n");
        specMap["base_gep_3"] = std::string(
            "%base_gep_3a = OpUntypedAccessChainKHR %ptr_no_stride %block_array %in_var %gid_x\n"
            "%base_gep_3b = OpUntypedAccessChainKHR %ptr_no_stride %block_array %in_var %next_gid_x\n"
            "%sel_3       = OpSelect %ptr_no_stride %less %base_gep_3a %base_gep_3b\n"
            "%extra_gep_3 = OpUntypedAccessChainKHR %ptr_16_stride %in_block_1 %sel_3 %c_uint32_0 %c_uint32_0\n");
        specMap["gep_0"] = "OpUntypedPtrAccessChainKHR %ptr_no_stride %int4_array %extra_gep_0 %index %c_uint32_0";
        specMap["gep_1"] = "OpUntypedPtrAccessChainKHR %ptr_no_stride %int4_array %extra_gep_1 %index %c_uint32_0";
        specMap["gep_2"] = "OpUntypedPtrAccessChainKHR %ptr_no_stride %int4_array %extra_gep_2 %index %c_uint32_0";
        specMap["gep_3"] = "OpUntypedPtrAccessChainKHR %ptr_no_stride %int4_array %extra_gep_3 %index %c_uint32_0";
        break;
    }
    default:
    {
        DE_ASSERT(0);
        DE_FATAL("Unknown test case.");
        break;
    }
    }

    main += std::string("OpSelectionMerge %label_end None\n"
                        "OpSwitch %gid_x %label_end 0 %label_0 1 %label_1 2 %label_2 3 %label_3\n"

                        "%label_0 = OpLabel\n"
                        "${base_gep_0}\n"
                        "%gep_0   = ${gep_0}\n"
                        "%value_0 = OpLoad %uint32 %gep_0\n"
                        "OpStore %value_var %value_0\n"
                        "OpBranch %label_end\n"

                        "%label_1 = OpLabel\n"
                        "${base_gep_1}\n"
                        "%gep_1   = ${gep_1}\n"
                        "%value_1 = OpLoad %uint32 %gep_1\n"
                        "OpStore %value_var %value_1\n"
                        "OpBranch %label_end\n"

                        "%label_2 = OpLabel\n"
                        "${base_gep_2}\n"
                        "%gep_2   = ${gep_2}\n"
                        "%value_2 = OpLoad %uint32 %gep_2\n"
                        "OpStore %value_var %value_2\n"
                        "OpBranch %label_end\n"

                        "%label_3 = OpLabel\n"
                        "${base_gep_3}\n"
                        "%gep_3   = ${gep_3}\n"
                        "%value_3 = OpLoad %uint32 %gep_3\n"
                        "OpStore %value_var %value_3\n"
                        "OpBranch %label_end\n"

                        "%label_end = OpLabel\n"
                        "%value = OpLoad %uint32 %value_var\n"
                        "%out_gep = OpAccessChain %uint32_storage_ptr %out_var %c_uint32_0 %gid_x\n"
                        "OpStore %out_gep %value\n"
                        "OpReturn\n"
                        "OpFunctionEnd\n");

    return main;
}

std::string createShaderMain(WORKGROUP_TEST_CASE testCase)
{
    std::string main = std::string("%main               = OpFunction %void None %void_func\n"
                                   "%label_main         = OpLabel\n");

    switch (testCase)
    {
    case WorkgroupTestCases::NOT_ALIASED:
    case WorkgroupTestCases::ALIASED:
    {
        main += std::string(
            /* Acesses */
            "%input_data_0_scalar_loc    = OpAccessChain           %${baseType}_storage_buffer_ptr      "
            "%input_data_0_var %c_uint32_1\n"
            "%input_data_0_vector_loc    = OpAccessChain           %vec4_${baseType}_storage_buffer_ptr "
            "%input_data_0_var %c_uint32_0\n"
            "%input_data_1_scalar_loc    = OpAccessChain           %${baseType}_storage_buffer_ptr      "
            "%input_data_1_var %c_uint32_1\n"
            "%input_data_1_vector_loc    = OpAccessChain           %vec4_${baseType}_storage_buffer_ptr "
            "%input_data_1_var %c_uint32_0\n"

            "%data_buffer_0_scalar_loc = OpUntypedAccessChainKHR %workgroup_untyped_ptr               %data_buffer     "
            "%data_buffer_0_untyped_var %c_uint32_1\n"
            "%data_buffer_0_vector_loc = OpUntypedAccessChainKHR %workgroup_untyped_ptr               %data_buffer     "
            "%data_buffer_0_untyped_var %c_uint32_0\n"
            "%data_buffer_1_scalar_loc = OpUntypedAccessChainKHR %workgroup_untyped_ptr               %data_buffer     "
            "%data_buffer_1_untyped_var %c_uint32_1\n"
            "%data_buffer_1_vector_loc = OpUntypedAccessChainKHR %workgroup_untyped_ptr               %data_buffer     "
            "%data_buffer_1_untyped_var %c_uint32_0\n"

            "%output_data_0_scalar_loc   = OpAccessChain         %${baseType}_storage_buffer_ptr       "
            "%output_data_0_var %c_uint32_1\n"
            "%output_data_0_vector_loc   = OpAccessChain         %vec4_${baseType}_storage_buffer_ptr  "
            "%output_data_0_var %c_uint32_0\n"
            "%output_data_1_scalar_loc   = OpAccessChain         %${baseType}_storage_buffer_ptr       "
            "%output_data_1_var %c_uint32_1\n"
            "%output_data_1_vector_loc   = OpAccessChain         %vec4_${baseType}_storage_buffer_ptr  "
            "%output_data_1_var %c_uint32_0\n"

            /* Writting to shared memory */
            "%input_data_0_scalar        = OpLoad  %${baseType}              %input_data_0_scalar_loc\n"
            "                              OpStore %data_buffer_0_scalar_loc %input_data_0_scalar\n"
            "%input_data_0_vector        = OpLoad  %vec4_${baseType}         %input_data_0_vector_loc\n"
            "                              OpStore %data_buffer_0_vector_loc %input_data_0_vector\n"
            "%input_data_1_scalar        = OpLoad  %${baseType}              %input_data_1_scalar_loc\n"
            "                              OpStore %data_buffer_1_scalar_loc %input_data_1_scalar\n"
            "%input_data_1_vector        = OpLoad  %vec4_${baseType}         %input_data_1_vector_loc\n"
            "                              OpStore %data_buffer_1_vector_loc %input_data_1_vector\n"

            /* Barriers */
            "                            OpMemoryBarrier         %c_uint32_1                          %c_uint32_264\n"
            "                            OpControlBarrier        %c_uint32_2                          %c_uint32_2      "
            "                           %c_uint32_264\n"

            /* Reading from shared memory */
            "%data_buffer_0_scalar     = OpLoad  %${baseType}              %data_buffer_0_scalar_loc\n"
            "                            OpStore %output_data_0_scalar_loc %data_buffer_0_scalar\n"
            "%data_buffer_0_vector     = OpLoad  %vec4_${baseType}         %data_buffer_0_vector_loc\n"
            "                            OpStore %output_data_0_vector_loc %data_buffer_0_vector\n"
            "%data_buffer_1_scalar     = OpLoad  %${baseType}              %data_buffer_1_scalar_loc\n"
            "                            OpStore %output_data_1_scalar_loc %data_buffer_1_scalar\n"
            "%data_buffer_1_vector     = OpLoad  %vec4_${baseType}         %data_buffer_1_vector_loc\n"
            "                            OpStore %output_data_1_vector_loc %data_buffer_1_vector\n");
        break;
    }
    default:
    {
        DE_ASSERT(0);
        DE_FATAL("Unknown test case.");
        break;
    }
    }

    main += std::string("                OpReturn\n"
                        "                OpFunctionEnd\n");

    return main;
}

std::string createShaderMain(COOPERATIVE_MATRIX_TEST_CASE testCase)
{
    std::string main = std::string("%main               = OpFunction %void None %void_func\n"
                                   "%label_main         = OpLabel\n");

    switch (testCase)
    {
    case CooperativeMatrixTestCases::BASIC_LOAD:
    {
        main += std::string("%output_loc    = OpAccessChain    %${baseType}_storage_buffer_ptr"
                            "                 %output_data_var %c_uint32_0 %c_uint32_0\n"

                            "%loaded_matrix = OpCooperativeMatrixLoadKHR %${baseType}_matrix "
                            "                 %input_data_untyped_var    %c_matrix_layout %stride\n"
                            "                 OpCooperativeMatrixStoreKHR %output_loc"
                            "                 %loaded_matrix              %c_matrix_layout %stride\n");
        break;
    }
    case CooperativeMatrixTestCases::BASIC_STORE:
    {
        main += std::string("%input_loc     = OpAccessChain   %${baseType}_storage_buffer_ptr"
                            "                 %input_data_var %c_uint32_0 %c_uint32_0\n"

                            "%loaded_matrix = OpCooperativeMatrixLoadKHR  %${baseType}_matrix"
                            "                 %input_loc                  %c_matrix_layout %stride\n"
                            "                 OpCooperativeMatrixStoreKHR %output_data_untyped_var"
                            "                 %loaded_matrix              %c_matrix_layout %stride\n");
        break;
    }
    case CooperativeMatrixTestCases::TYPE_PUNNING_LOAD:
    {
        main += std::string("%output_loc    = OpAccessChain    %${sameSizeType}_storage_buffer_ptr"
                            "                 %output_data_var %c_uint32_0 %c_uint32_0\n"

                            "%loaded_matrix = OpCooperativeMatrixLoadKHR  %${sameSizeType}_matrix"
                            "                 %input_data_untyped_var     %c_matrix_layout %stride\n"
                            "                 OpCooperativeMatrixStoreKHR %output_loc"
                            "                 %loaded_matrix              %c_matrix_layout %stride\n");
        break;
    }
    case CooperativeMatrixTestCases::TYPE_PUNNING_STORE:
    {
        main += std::string("%input_loc     = OpAccessChain   %${baseType}_storage_buffer_ptr"
                            "                 %input_data_var %c_uint32_0 %c_uint32_0\n"

                            "%loaded_matrix = OpCooperativeMatrixLoadKHR  %${baseType}_matrix"
                            "                 %input_loc                  %c_matrix_layout %stride\n"
                            "                 OpCooperativeMatrixStoreKHR %output_data_untyped_var"
                            "                 %loaded_matrix              %c_matrix_layout %stride\n");
        break;
    }
    case CooperativeMatrixTestCases::MIXED_LOAD:
    {
        main += std::string("%id_loc = OpAccessChain %uint32_input_ptr %id %c_uint32_0\n"
                            "%x      = OpLoad        %uint32           %id_loc\n"

                            "%input_loc  = OpUntypedAccessChainKHR %storage_buffer_untyped_ptr"
                            "              %input_buffer           %input_data_untyped_var %c_uint32_0 %x\n"
                            "%output_loc = OpAccessChain           %${baseType}_storage_buffer_ptr"
                            "              %output_data_var        %c_uint32_0 %x\n"

                            "%loaded_matrix = OpCooperativeMatrixLoadKHR  %${baseType}_matrix %input_loc"
                            "                 %c_matrix_layout            %stride             None\n"
                            "                 OpCooperativeMatrixStoreKHR %output_loc         %loaded_matrix"
                            "                 %c_matrix_layout            %stride             None\n");
        break;
    }
    case CooperativeMatrixTestCases::MIXED_STORE:
    {
        main += std::string("%id_loc = OpAccessChain %uint32_input_ptr %id %c_uint32_0\n"
                            "%x      = OpLoad        %uint32           %id_loc\n"

                            "%input_loc  = OpAccessChain           %${baseType}_storage_buffer_ptr"
                            "              %input_data_var         %c_uint32_0 %x\n"
                            "%output_loc = OpUntypedAccessChainKHR %storage_buffer_untyped_ptr"
                            "              %output_buffer          %output_data_untyped_var %c_uint32_0 %x\n"

                            "%loaded_matrix = OpCooperativeMatrixLoadKHR  %${baseType}_matrix %input_loc"
                            "                 %c_matrix_layout            %stride             None\n"
                            "                 OpCooperativeMatrixStoreKHR %output_loc         %loaded_matrix"
                            "                 %c_matrix_layout            %stride             None\n");
        break;
    }
    default:
    {
        DE_ASSERT(0);
        DE_FATAL("Unknown test case.");
        break;
    }
    }

    main += std::string("                OpReturn\n"
                        "                OpFunctionEnd\n");

    return main;
}

void addDescriptorArrayTests(tcu::TestCaseGroup *testGroup, MEMORY_MODEL_TYPE memModel)
{
    tcu::TestContext &testCtx = testGroup->getTestContext();

    const tcu::StringTemplate shaderHeader(createShaderHeader());

    const tcu::StringTemplate shaderAnnotations(createShaderAnnotations(BaseTestCases::DESCRIPTOR_ARRAY));

    const tcu::StringTemplate shaderVariables(createShaderVariables(BaseTestCases::DESCRIPTOR_ARRAY));

    const tcu::StringTemplate shaderFunctions(createShaderMain(BaseTestCases::DESCRIPTOR_ARRAY));

    for (uint32_t i = 0; i < DE_LENGTH_OF_ARRAY(BASE_DATA_TYPE_CASES); ++i)
    {
        std::string testName = toString(BASE_DATA_TYPE_CASES[i]);

        std::map<std::string, std::string> specMap;
        specMap["stride"]   = std::to_string(getSizeInBytes(BASE_DATA_TYPE_CASES[i]) * 4);
        specMap["offset0"]  = std::to_string(getSizeInBytes(BASE_DATA_TYPE_CASES[i]) * 0);
        specMap["offset1"]  = std::to_string(getSizeInBytes(BASE_DATA_TYPE_CASES[i]) * 1);
        specMap["offset2"]  = std::to_string(getSizeInBytes(BASE_DATA_TYPE_CASES[i]) * 2);
        specMap["offset3"]  = std::to_string(getSizeInBytes(BASE_DATA_TYPE_CASES[i]) * 3);
        specMap["baseType"] = toString(BASE_DATA_TYPE_CASES[i]);
        specMap["baseDecl"] = getDeclaration(BASE_DATA_TYPE_CASES[i]);

        std::string memModelOp;
        std::vector<const char *> spvExts;
        std::vector<const char *> spvCaps;
        ComputeShaderSpec spec;
        adjustSpecForUntypedPointers(spec, spvExts, spvCaps);
        adjustSpecForMemoryModel(memModel, spec, memModelOp, spvExts, spvCaps);
        adjustSpecForDataTypes(BASE_DATA_TYPE_CASES[i], spec, spvExts, spvCaps);
        adjustSpecForVariablePointers(spec, spvExts, spvCaps);

        specMap["memModelOp"]   = memModelOp;
        specMap["extensions"]   = toString(spvExts);
        specMap["capabilities"] = toString(spvCaps);

        std::string shaderVariablesStr = shaderVariables.specialize(specMap);
        if (BASE_DATA_TYPE_CASES[i] != DataTypes::UINT32)
        {
            shaderVariablesStr = "%uint32                    = OpTypeInt     32            0\n" + shaderVariablesStr;
        }

        const std::string shaderAsm = shaderHeader.specialize(specMap) + shaderAnnotations.specialize(specMap) +
                                      shaderVariablesStr + shaderFunctions.specialize(specMap);

        FilledBufferDesc desc;
        desc.dataType  = BASE_DATA_TYPE_CASES[i];
        desc.value     = 1;
        desc.elemCount = 4;
        desc.fillType  = FillingTypes::VALUE;
        desc.padding   = 0;

        Resource inputOutput = createFilledResource(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, desc);

        spec.assembly      = shaderAsm;
        spec.numWorkGroups = tcu::IVec3(1, 1, 1);
        spec.inputs.push_back(inputOutput);
        spec.outputs.push_back(inputOutput);
        spec.extensions.push_back("VK_KHR_storage_buffer_storage_class");

        testGroup->addChild(new SpvAsmComputeShaderCase(testCtx, testName.c_str(), spec));
    }
}

void addOpArrayLengthTests(tcu::TestCaseGroup *testGroup, MEMORY_MODEL_TYPE memModel)
{
    tcu::TestContext &testCtx = testGroup->getTestContext();

    const tcu::StringTemplate shaderHeader(createShaderHeader());

    const tcu::StringTemplate shaderAnnotations(createShaderAnnotations(BaseTestCases::ARRAY_LENGTH));

    const tcu::StringTemplate shaderVariables(createShaderVariables(BaseTestCases::ARRAY_LENGTH));

    const tcu::StringTemplate shaderFunctions(createShaderMain(BaseTestCases::ARRAY_LENGTH));

    for (uint32_t i = 0; i < DE_LENGTH_OF_ARRAY(BASE_DATA_TYPE_CASES); ++i)
    {
        std::string testName = toString(BASE_DATA_TYPE_CASES[i]);

        const uint32_t numWorkgroup = 16u;

        std::map<std::string, std::string> specMap;
        specMap["alignment"] = std::to_string(getSizeInBytes(BASE_DATA_TYPE_CASES[i]));
        specMap["baseType"]  = toString(BASE_DATA_TYPE_CASES[i]);
        specMap["baseDecl"]  = getDeclaration(BASE_DATA_TYPE_CASES[i]);

        std::string shaderVariablesStr = shaderVariables.specialize(specMap);
        if (BASE_DATA_TYPE_CASES[i] != DataTypes::UINT32)
        {
            shaderVariablesStr = "%uint32                    = OpTypeInt     32            0\n"
                                 "%c_uint32_1                = OpConstant    %uint32       1\n"
                                 "%uint32_storage_buffer_ptr = OpTypePointer StorageBuffer %uint32\n" +
                                 shaderVariablesStr;
        }

        std::string memModelOp;
        std::vector<const char *> spvExts;
        std::vector<const char *> spvCaps;
        ComputeShaderSpec spec;
        adjustSpecForUntypedPointers(spec, spvExts, spvCaps);
        adjustSpecForMemoryModel(memModel, spec, memModelOp, spvExts, spvCaps);
        adjustSpecForDataTypes(BASE_DATA_TYPE_CASES[i], spec, spvExts, spvCaps);

        specMap["memModelOp"]       = memModelOp;
        specMap["extensions"]       = toString(spvExts);
        specMap["capabilities"]     = toString(spvCaps);
        const std::string shaderAsm = shaderHeader.specialize(specMap) + shaderAnnotations.specialize(specMap) +
                                      shaderVariablesStr + shaderFunctions.specialize(specMap);

        FilledBufferDesc desc;
        desc.dataType  = BASE_DATA_TYPE_CASES[i];
        desc.value     = 1;
        desc.elemCount = numWorkgroup;
        desc.fillType  = FillingTypes::VALUE;
        desc.padding   = 0;
        Resource input = createFilledResource(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, desc);

        desc.value      = numWorkgroup;
        desc.elemCount  = 1;
        desc.dataType   = DataTypes::UINT32;
        Resource output = createFilledResource(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, desc);

        spec.assembly      = shaderAsm;
        spec.numWorkGroups = tcu::IVec3(numWorkgroup, 1, 1);
        spec.inputs.push_back(input);
        spec.outputs.push_back(output);
        spec.extensions.push_back("VK_KHR_storage_buffer_storage_class");

        testGroup->addChild(new SpvAsmComputeShaderCase(testCtx, testName.c_str(), spec));
    }
}

void addLoadTests(tcu::TestCaseGroup *testGroup, MEMORY_MODEL_TYPE memModel)
{
    tcu::TestContext &testCtx = testGroup->getTestContext();

    de::MovePtr<tcu::TestCaseGroup> uniformGroup(new tcu::TestCaseGroup(testCtx, "uniform", ""));
    de::MovePtr<tcu::TestCaseGroup> storageGroup(new tcu::TestCaseGroup(testCtx, "storage", ""));
    de::MovePtr<tcu::TestCaseGroup> pushConstantGroup(new tcu::TestCaseGroup(testCtx, "push_constant", ""));

    const tcu::StringTemplate shaderHeader(createShaderHeader());

    const tcu::StringTemplate shaderAnnotations(createShaderAnnotations(BaseTestCases::LOAD));

    const tcu::StringTemplate shaderVariables(createShaderVariables(BaseTestCases::LOAD));

    const tcu::StringTemplate shaderFunctions(createShaderMain(BaseTestCases::LOAD));

    for (uint32_t i = 0; i < DE_LENGTH_OF_ARRAY(BASE_DATA_TYPE_CASES); ++i)
    {
        for (uint32_t j = 0; j < DE_LENGTH_OF_ARRAY(LOAD_CONTAINER_TYPE_CASES); ++j)
        {
            std::string testName = toString(BASE_DATA_TYPE_CASES[i]);

            const uint32_t numWorkgroup = LOAD_CONTAINER_TYPE_CASES[j] == ContainerTypes::PUSH_CONSTANT ?
                                              Constants::pushConstArraySize :
                                              Constants::numThreads;

            std::map<std::string, std::string> specMap;
            if (LOAD_CONTAINER_TYPE_CASES[j] == ContainerTypes::UNIFORM)
            {
                specMap["alignment"] = std::to_string(Constants::uniformAlignment);
            }
            else
            {
                specMap["alignment"] = std::to_string(getSizeInBytes(BASE_DATA_TYPE_CASES[i]));
            }
            specMap["threadCount"]  = std::to_string(numWorkgroup);
            specMap["args"]         = LOAD_OPERATION_CASES[0].pArgs;
            specMap["baseType"]     = toString(BASE_DATA_TYPE_CASES[i]);
            specMap["loadOp"]       = LOAD_OPERATION_CASES[0].pOperation;
            specMap["baseDecl"]     = getDeclaration(BASE_DATA_TYPE_CASES[i]);
            specMap["storageClass"] = getStorageClass(LOAD_CONTAINER_TYPE_CASES[j]);
            specMap["storageDecorations"] =
                getResourceDecorations(LOAD_CONTAINER_TYPE_CASES[j], BASE_DATA_TYPE_CASES[i], numWorkgroup);

            std::string shaderVariablesStr = shaderVariables.specialize(specMap);
            if (BASE_DATA_TYPE_CASES[i] != DataTypes::UINT32)
            {
                shaderVariablesStr = "%uint32     = OpTypeInt  32      0\n"
                                     "%c_uint32_1 = OpConstant %uint32 1\n" +
                                     shaderVariablesStr;
            }

            std::string memModelOp;
            std::vector<const char *> spvExts;
            std::vector<const char *> spvCaps;
            ComputeShaderSpec spec;
            adjustSpecForUntypedPointers(spec, spvExts, spvCaps);
            adjustSpecForMemoryModel(memModel, spec, memModelOp, spvExts, spvCaps);
            adjustSpecForDataTypes(BASE_DATA_TYPE_CASES[i], spec, spvExts, spvCaps);

            specMap["memModelOp"]       = memModelOp;
            specMap["extensions"]       = toString(spvExts);
            specMap["capabilities"]     = toString(spvCaps);
            const std::string shaderAsm = shaderHeader.specialize(specMap) + shaderAnnotations.specialize(specMap) +
                                          shaderVariablesStr + shaderFunctions.specialize(specMap);

            FilledBufferDesc desc;
            desc.dataType  = BASE_DATA_TYPE_CASES[i];
            desc.elemCount = numWorkgroup;
            desc.fillType  = FillingTypes::RANDOM;
            desc.seed      = deStringHash(testGroup->getName());
            VkDescriptorType inpDescType;
            if (LOAD_CONTAINER_TYPE_CASES[j] == ContainerTypes::UNIFORM)
            {
                inpDescType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                desc.padding = Constants::uniformAlignment - getSizeInBytes(BASE_DATA_TYPE_CASES[i]);
            }
            else
            {
                inpDescType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                desc.padding = 0;
            }

            BufferSp inputBuffer    = createFilledBuffer(desc);
            Resource outputResource = createFilledResource(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, desc);

            if (LOAD_CONTAINER_TYPE_CASES[j] == ContainerTypes::PUSH_CONSTANT)
                spec.pushConstants = inputBuffer;
            else
                spec.inputs.push_back(Resource(inputBuffer, inpDescType));

            spec.assembly      = shaderAsm;
            spec.numWorkGroups = tcu::IVec3(numWorkgroup, 1, 1);
            spec.outputs.push_back(outputResource);
            spec.extensions.push_back("VK_KHR_storage_buffer_storage_class");

            if (LOAD_CONTAINER_TYPE_CASES[j] == ContainerTypes::UNIFORM)
            {
                uniformGroup->addChild(new SpvAsmComputeShaderCase(testCtx, testName.c_str(), spec));
            }
            else if (LOAD_CONTAINER_TYPE_CASES[j] == ContainerTypes::STORAGE_BUFFER)
            {
                storageGroup->addChild(new SpvAsmComputeShaderCase(testCtx, testName.c_str(), spec));
            }
            else // LOAD_CONTAINER_TYPE_CASES[j] == ContainerTypes::PUSH_CONSTANT
            {
                pushConstantGroup->addChild(new SpvAsmComputeShaderCase(testCtx, testName.c_str(), spec));
            }
        }
    }

    testGroup->addChild(uniformGroup.release());
    testGroup->addChild(storageGroup.release());
    testGroup->addChild(pushConstantGroup.release());
}

void addLoadAtomicTests(tcu::TestCaseGroup *testGroup, MEMORY_MODEL_TYPE memModel)
{
    tcu::TestContext &testCtx = testGroup->getTestContext();

    const tcu::StringTemplate shaderHeader(createShaderHeader());

    const tcu::StringTemplate shaderAnnotations(createShaderAnnotations(AtomicTestCases::OP_ATOMIC_LOAD));

    const tcu::StringTemplate shaderVariables(createShaderVariables(AtomicTestCases::OP_ATOMIC_LOAD));

    const tcu::StringTemplate shaderFunctions(createShaderMain(AtomicTestCases::OP_ATOMIC_LOAD));

    for (uint32_t i = 0; i < DE_LENGTH_OF_ARRAY(ATOMIC_DATA_TYPE_CASES); ++i)
    {
        std::string testName = toString(ATOMIC_DATA_TYPE_CASES[i]);

        const uint32_t numWorkgroup = Constants::numThreads;

        std::map<std::string, std::string> specMap;

        specMap["alignment"]   = std::to_string(getSizeInBytes(ATOMIC_DATA_TYPE_CASES[i]));
        specMap["threadCount"] = std::to_string(numWorkgroup);
        specMap["args"]        = LOAD_OPERATION_CASES[1].pArgs;
        specMap["baseType"]    = toString(ATOMIC_DATA_TYPE_CASES[i]);
        specMap["loadOp"]      = LOAD_OPERATION_CASES[1].pOperation;
        specMap["baseDecl"]    = getDeclaration(ATOMIC_DATA_TYPE_CASES[i]);

        std::string shaderVariablesStr = shaderVariables.specialize(specMap);
        if (ATOMIC_DATA_TYPE_CASES[i] != DataTypes::UINT32)
        {
            shaderVariablesStr = "%uint32     = OpTypeInt  32      0\n" + shaderVariablesStr;
        }

        std::string memModelOp;
        std::vector<const char *> spvExts;
        std::vector<const char *> spvCaps;
        ComputeShaderSpec spec;
        adjustSpecForUntypedPointers(spec, spvExts, spvCaps);
        adjustSpecForMemoryModel(memModel, spec, memModelOp, spvExts, spvCaps);
        adjustSpecForDataTypes(ATOMIC_DATA_TYPE_CASES[i], spec, spvExts, spvCaps);
        adjustSpecForAtomicOperations(ATOMIC_DATA_TYPE_CASES[i], spec, spvExts, spvCaps);

        specMap["memModelOp"]       = memModelOp;
        specMap["extensions"]       = toString(spvExts);
        specMap["capabilities"]     = toString(spvCaps);
        const std::string shaderAsm = shaderHeader.specialize(specMap) + shaderAnnotations.specialize(specMap) +
                                      shaderVariablesStr + shaderFunctions.specialize(specMap);

        FilledBufferDesc desc;
        desc.dataType  = ATOMIC_DATA_TYPE_CASES[i];
        desc.elemCount = numWorkgroup;
        desc.fillType  = FillingTypes::RANDOM;
        desc.seed      = deStringHash(testGroup->getName());
        desc.padding   = 0;

        Resource inputResource  = createFilledResource(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, desc);
        Resource outputResource = createFilledResource(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, desc);

        spec.inputs.push_back(inputResource);
        spec.outputs.push_back(outputResource);
        spec.assembly      = shaderAsm;
        spec.numWorkGroups = tcu::IVec3(numWorkgroup, 1, 1);
        spec.extensions.push_back("VK_KHR_storage_buffer_storage_class");

        testGroup->addChild(new SpvAsmComputeShaderCase(testCtx, testName.c_str(), spec));
    }
}

void addLoadMixedTypeTests(tcu::TestCaseGroup *testGroup, MEMORY_MODEL_TYPE memModel)
{
    tcu::TestContext &testCtx = testGroup->getTestContext();

    de::MovePtr<tcu::TestCaseGroup> uniformGroup(new tcu::TestCaseGroup(testCtx, "uniform", ""));
    de::MovePtr<tcu::TestCaseGroup> storageGroup(new tcu::TestCaseGroup(testCtx, "storage", ""));
    de::MovePtr<tcu::TestCaseGroup> pushConstantGroup(new tcu::TestCaseGroup(testCtx, "push_constant", ""));

    {
        const tcu::StringTemplate shaderHeader(createShaderHeader());

        const tcu::StringTemplate shaderAnnotations(
            createShaderAnnotations(TypePunningTestCases::LOAD_SAME_SIZE_TYPES));

        const tcu::StringTemplate shaderVariables(createShaderVariables(TypePunningTestCases::LOAD_SAME_SIZE_TYPES));

        const tcu::StringTemplate shaderFunctions(createShaderMain(TypePunningTestCases::LOAD_SAME_SIZE_TYPES));

        for (uint32_t i = 0; i < DE_LENGTH_OF_ARRAY(BASE_DATA_TYPE_CASES); ++i)
        {
            for (uint32_t j = 0; j < DE_LENGTH_OF_ARRAY(LOAD_CONTAINER_TYPE_CASES); ++j)
            {
                std::vector<DATA_TYPE> sameSizeTypes = getSameSizeBaseDataType(BASE_DATA_TYPE_CASES[i]);

                for (uint32_t l = 0; l < sameSizeTypes.size(); ++l)
                {
                    const DATA_TYPE dataType = sameSizeTypes[l];

                    std::string testName = toString(BASE_DATA_TYPE_CASES[i]) + std::string("_to_") + toString(dataType);

                    const uint32_t numWorkgroup = LOAD_CONTAINER_TYPE_CASES[j] == ContainerTypes::PUSH_CONSTANT ?
                                                      Constants::pushConstArraySize :
                                                      Constants::numThreads;
                    const uint32_t caseIndex    = static_cast<uint32_t>(dataType);
                    std::map<std::string, std::string> specMap;
                    if (LOAD_CONTAINER_TYPE_CASES[j] == ContainerTypes::UNIFORM)
                    {
                        specMap["alignment"] = std::to_string(Constants::uniformAlignment);
                    }
                    else
                    {
                        specMap["alignment"] = std::to_string(getSizeInBytes(BASE_DATA_TYPE_CASES[i]));
                    }
                    specMap["threadCount"]  = std::to_string(numWorkgroup);
                    specMap["args"]         = LOAD_OPERATION_CASES[0].pArgs;
                    specMap["baseType"]     = toString(BASE_DATA_TYPE_CASES[i]);
                    specMap["baseDecl"]     = getDeclaration(BASE_DATA_TYPE_CASES[i]);
                    specMap["sameSizeType"] = toString(BASE_DATA_TYPE_CASES[caseIndex]);
                    specMap["sameSizeDecl"] = getDeclaration(BASE_DATA_TYPE_CASES[caseIndex]);
                    specMap["loadOp"]       = LOAD_OPERATION_CASES[0].pOperation;
                    specMap["storageClass"] = getStorageClass(LOAD_CONTAINER_TYPE_CASES[j]);
                    specMap["otherCap"]     = getCapability(dataType);
                    specMap["storageDecorations"] =
                        getSameSizeResourceDecorations(LOAD_CONTAINER_TYPE_CASES[j], BASE_DATA_TYPE_CASES[i],
                                                       BASE_DATA_TYPE_CASES[caseIndex], numWorkgroup);

                    std::string shaderVariablesStr = shaderVariables.specialize(specMap);
                    if ((BASE_DATA_TYPE_CASES[i] != DataTypes::UINT32) && (dataType != DataTypes::UINT32))
                    {
                        shaderVariablesStr = "%uint32     = OpTypeInt 32 0\n"
                                             "%c_uint32_1 = OpConstant %uint32 1\n" +
                                             shaderVariablesStr;
                    }

                    std::string memModelOp;
                    std::vector<const char *> spvExts;
                    std::vector<const char *> spvCaps;
                    ComputeShaderSpec spec;
                    adjustSpecForUntypedPointers(spec, spvExts, spvCaps);
                    adjustSpecForMemoryModel(memModel, spec, memModelOp, spvExts, spvCaps);
                    adjustSpecForDataTypes(BASE_DATA_TYPE_CASES[i], spec, spvExts, spvCaps);
                    if (BASE_DATA_TYPE_CASES[i] != dataType)
                        adjustSpecForDataTypes(dataType, spec, spvExts, spvCaps);
                    adjustSpecForSmallContainerType(LOAD_CONTAINER_TYPE_CASES[j], BASE_DATA_TYPE_CASES[i], spec,
                                                    spvExts, spvCaps);
                    if ((getSizeInBytes(BASE_DATA_TYPE_CASES[i]) !=
                         getSizeInBytes(dataType)) ||                                     // diffrent size of data types
                        (LOAD_CONTAINER_TYPE_CASES[j] != ContainerTypes::STORAGE_BUFFER)) // diffrent starage types
                        adjustSpecForSmallContainerType(ContainerTypes::STORAGE_BUFFER, BASE_DATA_TYPE_CASES[i], spec,
                                                        spvExts, spvCaps);

                    specMap["memModelOp"]       = memModelOp;
                    specMap["extensions"]       = toString(spvExts);
                    specMap["capabilities"]     = toString(spvCaps);
                    const std::string shaderAsm = shaderHeader.specialize(specMap) +
                                                  shaderAnnotations.specialize(specMap) + shaderVariablesStr +
                                                  shaderFunctions.specialize(specMap);

                    FilledBufferDesc desc;
                    desc.dataType  = sameSizeTypes[l];
                    desc.elemCount = numWorkgroup;
                    desc.fillType  = FillingTypes::RANDOM;
                    desc.seed      = deStringHash(testGroup->getName());

                    VkDescriptorType inpDescType;
                    if (LOAD_CONTAINER_TYPE_CASES[j] == ContainerTypes::UNIFORM)
                    {
                        inpDescType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                        desc.padding = Constants::uniformAlignment - getSizeInBytes(BASE_DATA_TYPE_CASES[i]);
                    }
                    else
                    {
                        inpDescType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                        desc.padding = 0;
                    }

                    BufferSp inputBuffer    = createFilledBuffer(desc);
                    Resource outputResource = createFilledResource(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, desc);

                    if (LOAD_CONTAINER_TYPE_CASES[j] == ContainerTypes::PUSH_CONSTANT)
                        spec.pushConstants = inputBuffer;
                    else
                        spec.inputs.push_back(Resource(inputBuffer, inpDescType));

                    spec.assembly      = shaderAsm;
                    spec.numWorkGroups = tcu::IVec3(numWorkgroup, 1, 1);
                    spec.outputs.push_back(outputResource);
                    spec.extensions.push_back("VK_KHR_storage_buffer_storage_class");

                    if (LOAD_CONTAINER_TYPE_CASES[j] == ContainerTypes::UNIFORM)
                    {
                        uniformGroup->addChild(new SpvAsmComputeShaderCase(testCtx, testName.c_str(), spec));
                    }
                    else if (LOAD_CONTAINER_TYPE_CASES[j] == ContainerTypes::STORAGE_BUFFER)
                    {
                        storageGroup->addChild(new SpvAsmComputeShaderCase(testCtx, testName.c_str(), spec));
                    }
                    else // LOAD_CONTAINER_TYPE_CASES[j] == ContainerTypes::PUSH_CONSTANT
                    {
                        pushConstantGroup->addChild(new SpvAsmComputeShaderCase(testCtx, testName.c_str(), spec));
                    }
                }
            }
        }
    }

    {
        const tcu::StringTemplate shaderHeader(createShaderHeader());

        const tcu::StringTemplate shaderAnnotations(createShaderAnnotations(TypePunningTestCases::LOAD_SCALAR_VECTOR));

        const tcu::StringTemplate shaderVariables(createShaderVariables(TypePunningTestCases::LOAD_SCALAR_VECTOR));

        const tcu::StringTemplate shaderFunctions(createShaderMain(TypePunningTestCases::LOAD_SCALAR_VECTOR));

        for (uint32_t i = 0; i < DE_LENGTH_OF_ARRAY(BASE_DATA_TYPE_CASES); ++i)
        {
            for (uint32_t j = 0; j < DE_LENGTH_OF_ARRAY(LOAD_CONTAINER_TYPE_CASES); ++j)
            {
                std::vector<COMPOSITE_DATA_TYPE> sameSizeTypes = getSameSizeCompositeType(BASE_DATA_TYPE_CASES[i]);

                for (uint32_t l = 0; l < sameSizeTypes.size(); ++l)
                {
                    COMPOSITE_DATA_TYPE compositeType = sameSizeTypes[l];
                    const uint32_t otherIndex         = static_cast<uint32_t>(getCompositeBaseDataType(compositeType));

                    std::string testName =
                        toString(BASE_DATA_TYPE_CASES[i]) + std::string("_to_") + toString(compositeType);

                    std::map<std::string, std::string> specMap;
                    if (LOAD_CONTAINER_TYPE_CASES[j] == ContainerTypes::UNIFORM)
                    {
                        specMap["alignment"] = std::to_string(Constants::uniformAlignment);
                    }
                    else
                    {
                        specMap["alignment"] = std::to_string(getSizeInBytes(BASE_DATA_TYPE_CASES[i]));
                    }

                    specMap["args"]               = LOAD_OPERATION_CASES[0].pArgs;
                    specMap["loadOp"]             = LOAD_OPERATION_CASES[0].pOperation;
                    specMap["storageClass"]       = getStorageClass(LOAD_CONTAINER_TYPE_CASES[j]);
                    specMap["baseType"]           = toString(BASE_DATA_TYPE_CASES[i]);
                    specMap["otherType"]          = toString(getCompositeBaseDataType(compositeType));
                    specMap["baseDecl"]           = getDeclaration(BASE_DATA_TYPE_CASES[i]);
                    specMap["otherTypeDecl"]      = getDeclaration(BASE_DATA_TYPE_CASES[otherIndex]);
                    specMap["otherVec"]           = toString(compositeType);
                    specMap["otherVecDecl"]       = getDeclaration(compositeType);
                    specMap["otherCap"]           = getCapability(compositeType);
                    specMap["storageDecorations"] = getScalarVectorResourceDecorations(LOAD_CONTAINER_TYPE_CASES[j]);

                    if (sameSizeTypes[l] != CompositeDataTypes::VEC3_UINT32)
                        specMap["inputVec"] = "%vec3_uint32 = OpTypeVector %uint32 3";

                    std::string shaderVariablesStr = shaderVariables.specialize(specMap);
                    if ((BASE_DATA_TYPE_CASES[i] != DataTypes::UINT32) &&
                        (getCompositeBaseDataType(compositeType) != DataTypes::UINT32) &&
                        compositeType != CompositeDataTypes::VEC2_UINT32 &&
                        compositeType != CompositeDataTypes::VEC3_UINT32 &&
                        compositeType != CompositeDataTypes::VEC4_UINT32)
                    {
                        shaderVariablesStr = "%uint32      = OpTypeInt 32 0\n"
                                             "%c_uint32_1  = OpConstant %uint32 1\n" +
                                             shaderVariablesStr;
                    }

                    std::string memModelOp;
                    std::vector<const char *> spvExts;
                    std::vector<const char *> spvCaps;
                    ComputeShaderSpec spec;
                    adjustSpecForUntypedPointers(spec, spvExts, spvCaps);
                    adjustSpecForMemoryModel(memModel, spec, memModelOp, spvExts, spvCaps);
                    adjustSpecForDataTypes(BASE_DATA_TYPE_CASES[i], spec, spvExts, spvCaps);
                    if (BASE_DATA_TYPE_CASES[i] != getCompositeBaseDataType(compositeType))
                        adjustSpecForDataTypes(getCompositeBaseDataType(compositeType), spec, spvExts, spvCaps);
                    adjustSpecForSmallContainerType(LOAD_CONTAINER_TYPE_CASES[j], BASE_DATA_TYPE_CASES[i], spec,
                                                    spvExts, spvCaps);
                    if ((getSizeInBytes(BASE_DATA_TYPE_CASES[i]) !=
                         getSizeInBytes(getCompositeBaseDataType(compositeType))) ||      // diffrent size of data types
                        (LOAD_CONTAINER_TYPE_CASES[j] != ContainerTypes::STORAGE_BUFFER)) // diffrent starage types
                        adjustSpecForSmallContainerType(ContainerTypes::STORAGE_BUFFER,
                                                        getCompositeBaseDataType(compositeType), spec, spvExts,
                                                        spvCaps);

                    specMap["memModelOp"]       = memModelOp;
                    specMap["extensions"]       = toString(spvExts);
                    specMap["capabilities"]     = toString(spvCaps);
                    const std::string shaderAsm = shaderHeader.specialize(specMap) +
                                                  shaderAnnotations.specialize(specMap) + shaderVariablesStr +
                                                  shaderFunctions.specialize(specMap);

                    FilledBufferDesc desc;
                    desc.dataType  = BASE_DATA_TYPE_CASES[i];
                    desc.elemCount = 2;
                    desc.padding   = 0;
                    desc.fillType  = FillingTypes::VALUE;
                    desc.value     = 1;

                    BufferSp inputBuffer = createFilledBuffer(desc);

                    desc.elemCount          = 1;
                    Resource outputResource = createFilledResource(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, desc);

                    if (LOAD_CONTAINER_TYPE_CASES[j] == ContainerTypes::PUSH_CONSTANT)
                        spec.pushConstants = inputBuffer;
                    else
                    {
                        VkDescriptorType t = LOAD_CONTAINER_TYPE_CASES[j] == ContainerTypes::UNIFORM ?
                                                 VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER :
                                                 VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                        spec.inputs.push_back(Resource(inputBuffer, t));
                    }

                    spec.assembly      = shaderAsm;
                    spec.numWorkGroups = tcu::IVec3(1, 1, 1);
                    spec.outputs.push_back(outputResource);
                    spec.extensions.push_back("VK_KHR_storage_buffer_storage_class");

                    if (LOAD_CONTAINER_TYPE_CASES[j] == ContainerTypes::UNIFORM)
                    {
                        uniformGroup->addChild(new SpvAsmComputeShaderCase(testCtx, testName.c_str(), spec));
                    }
                    else if (LOAD_CONTAINER_TYPE_CASES[j] == ContainerTypes::STORAGE_BUFFER)
                    {
                        storageGroup->addChild(new SpvAsmComputeShaderCase(testCtx, testName.c_str(), spec));
                    }
                    else // LOAD_CONTAINER_TYPE_CASES[j] == ContainerTypes::PUSH_CONSTANT
                    {
                        pushConstantGroup->addChild(new SpvAsmComputeShaderCase(testCtx, testName.c_str(), spec));
                    }
                }
            }
        }
    }

    {
        const tcu::StringTemplate shaderHeader(createShaderHeader());

        const tcu::StringTemplate shaderAnnotations(createShaderAnnotations(TypePunningTestCases::LOAD_VECTOR_SCALAR));

        const tcu::StringTemplate shaderVariables(createShaderVariables(TypePunningTestCases::LOAD_VECTOR_SCALAR));

        const tcu::StringTemplate shaderFunctions(createShaderMain(TypePunningTestCases::LOAD_VECTOR_SCALAR));

        for (uint32_t i = 0; i < DE_LENGTH_OF_ARRAY(COMPOSITE_DATA_TYPE_CASES); ++i)
        {
            for (uint32_t j = 0; j < DE_LENGTH_OF_ARRAY(LOAD_CONTAINER_TYPE_CASES); ++j)
            {
                std::vector<DATA_TYPE> sameSizeTypes = getSameSizeBaseDataType(COMPOSITE_DATA_TYPE_CASES[i]);

                for (uint32_t l = 0; l < sameSizeTypes.size(); ++l)
                {
                    const DATA_TYPE dataType  = sameSizeTypes[l];
                    const uint32_t otherIndex = static_cast<uint32_t>(dataType);

                    std::string testName =
                        toString(COMPOSITE_DATA_TYPE_CASES[i]) + std::string("_to_") + toString(dataType);

                    std::map<std::string, std::string> specMap;
                    if (LOAD_CONTAINER_TYPE_CASES[j] == ContainerTypes::UNIFORM)
                    {
                        specMap["alignment"] = std::to_string(Constants::uniformAlignment);
                    }
                    else
                    {
                        specMap["alignment"] = std::to_string(getSizeInBytes(COMPOSITE_DATA_TYPE_CASES[i]));
                    }

                    specMap["args"]          = LOAD_OPERATION_CASES[0].pArgs;
                    specMap["loadOp"]        = LOAD_OPERATION_CASES[0].pOperation;
                    specMap["storageClass"]  = getStorageClass(LOAD_CONTAINER_TYPE_CASES[j]);
                    specMap["baseType"]      = toString(getCompositeBaseDataType(COMPOSITE_DATA_TYPE_CASES[i]));
                    specMap["otherType"]     = toString(dataType);
                    specMap["baseDecl"]      = getDeclaration(getCompositeBaseDataType(COMPOSITE_DATA_TYPE_CASES[i]));
                    specMap["otherTypeDecl"] = getDeclaration(BASE_DATA_TYPE_CASES[otherIndex]);
                    specMap["otherCap"]      = getCapability(dataType);
                    specMap["baseVec"]       = toString(COMPOSITE_DATA_TYPE_CASES[i]);
                    specMap["baseVecDecl"]   = getDeclaration(COMPOSITE_DATA_TYPE_CASES[i]);
                    specMap["storageDecorations"] = getScalarVectorResourceDecorations(LOAD_CONTAINER_TYPE_CASES[j]);

                    if (COMPOSITE_DATA_TYPE_CASES[i] != CompositeDataTypes::VEC3_UINT32)
                        specMap["inputVec"] = "%vec3_uint32 = OpTypeVector %uint32 3";

                    std::string shaderVariablesStr = shaderVariables.specialize(specMap);
                    if ((getCompositeBaseDataType(COMPOSITE_DATA_TYPE_CASES[i]) != DataTypes::UINT32) &&
                        (dataType != DataTypes::UINT32))
                    {
                        shaderVariablesStr = "%uint32      = OpTypeInt 32 0\n"
                                             "%c_uint32_1  = OpConstant %uint32 1\n" +
                                             shaderVariablesStr;
                    }

                    std::string memModelOp;
                    std::vector<const char *> spvExts;
                    std::vector<const char *> spvCaps;
                    ComputeShaderSpec spec;
                    adjustSpecForUntypedPointers(spec, spvExts, spvCaps);
                    adjustSpecForMemoryModel(memModel, spec, memModelOp, spvExts, spvCaps);
                    adjustSpecForDataTypes(getCompositeBaseDataType(COMPOSITE_DATA_TYPE_CASES[i]), spec, spvExts,
                                           spvCaps);
                    if (getCompositeBaseDataType(COMPOSITE_DATA_TYPE_CASES[i]) != dataType)
                        adjustSpecForDataTypes(dataType, spec, spvExts, spvCaps);
                    adjustSpecForSmallContainerType(LOAD_CONTAINER_TYPE_CASES[j],
                                                    getCompositeBaseDataType(COMPOSITE_DATA_TYPE_CASES[i]), spec,
                                                    spvExts, spvCaps);
                    if ((getSizeInBytes(getCompositeBaseDataType(COMPOSITE_DATA_TYPE_CASES[i])) !=
                         getSizeInBytes(dataType)) ||                                     // diffrent size of data types
                        (LOAD_CONTAINER_TYPE_CASES[j] != ContainerTypes::STORAGE_BUFFER)) // diffrent starage types
                        adjustSpecForSmallContainerType(ContainerTypes::STORAGE_BUFFER, dataType, spec, spvExts,
                                                        spvCaps);

                    specMap["memModelOp"]       = memModelOp;
                    specMap["extensions"]       = toString(spvExts);
                    specMap["capabilities"]     = toString(spvCaps);
                    const std::string shaderAsm = shaderHeader.specialize(specMap) +
                                                  shaderAnnotations.specialize(specMap) + shaderVariablesStr +
                                                  shaderFunctions.specialize(specMap);

                    FilledBufferDesc desc;
                    desc.dataType = getCompositeBaseDataType(COMPOSITE_DATA_TYPE_CASES[i]);
                    /* We use only first value to meet push constant requirements */;
                    desc.elemCount = getElementCount(COMPOSITE_DATA_TYPE_CASES[i]) * 2;
                    desc.padding   = 0;
                    desc.fillType  = FillingTypes::VALUE;
                    desc.value     = 1;

                    BufferSp inputBuffer = createFilledBuffer(desc);

                    desc.elemCount          = getElementCount(COMPOSITE_DATA_TYPE_CASES[i]);
                    Resource outputResource = createFilledResource(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, desc);

                    if (LOAD_CONTAINER_TYPE_CASES[j] == ContainerTypes::PUSH_CONSTANT)
                        spec.pushConstants = inputBuffer;
                    else
                    {
                        VkDescriptorType t = LOAD_CONTAINER_TYPE_CASES[j] == ContainerTypes::UNIFORM ?
                                                 VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER :
                                                 VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                        spec.inputs.push_back(Resource(inputBuffer, t));
                    }

                    spec.assembly      = shaderAsm;
                    spec.numWorkGroups = tcu::IVec3(1, 1, 1);
                    spec.outputs.push_back(outputResource);
                    spec.extensions.push_back("VK_KHR_storage_buffer_storage_class");

                    if (LOAD_CONTAINER_TYPE_CASES[j] == ContainerTypes::UNIFORM)
                    {
                        uniformGroup->addChild(new SpvAsmComputeShaderCase(testCtx, testName.c_str(), spec));
                    }
                    else if (LOAD_CONTAINER_TYPE_CASES[j] == ContainerTypes::STORAGE_BUFFER)
                    {
                        storageGroup->addChild(new SpvAsmComputeShaderCase(testCtx, testName.c_str(), spec));
                    }
                    else // LOAD_CONTAINER_TYPE_CASES[j] == ContainerTypes::PUSH_CONSTANT
                    {
                        pushConstantGroup->addChild(new SpvAsmComputeShaderCase(testCtx, testName.c_str(), spec));
                    }
                }
            }
        }
    }

    testGroup->addChild(uniformGroup.release());
    testGroup->addChild(storageGroup.release());
    testGroup->addChild(pushConstantGroup.release());
}

void addStoreTests(tcu::TestCaseGroup *testGroup, MEMORY_MODEL_TYPE memModel)
{
    tcu::TestContext &testCtx = testGroup->getTestContext();

    const tcu::StringTemplate shaderHeader(createShaderHeader());

    const tcu::StringTemplate shaderAnnotations(createShaderAnnotations(BaseTestCases::STORE));

    const tcu::StringTemplate shaderVariables(createShaderVariables(BaseTestCases::STORE));

    const tcu::StringTemplate shaderFunctions(createShaderMain(BaseTestCases::STORE));

    for (uint32_t i = 0; i < DE_LENGTH_OF_ARRAY(BASE_DATA_TYPE_CASES); ++i)
    {
        std::string testName = toString(BASE_DATA_TYPE_CASES[i]);

        std::map<std::string, std::string> specMap;
        specMap["alignment"]   = std::to_string(getSizeInBytes(BASE_DATA_TYPE_CASES[i]));
        specMap["args"]        = STORE_OPERATION_CASES[0].pArgs;
        specMap["baseDecl"]    = getDeclaration(BASE_DATA_TYPE_CASES[i]);
        specMap["baseType"]    = toString(BASE_DATA_TYPE_CASES[i]);
        specMap["storeOp"]     = STORE_OPERATION_CASES[0].pOperation;
        specMap["threadCount"] = std::to_string(Constants::numThreads);

        std::string shaderVariablesStr = shaderVariables.specialize(specMap);
        if (BASE_DATA_TYPE_CASES[i] != DataTypes::UINT32)
        {
            shaderVariablesStr = "%uint32     = OpTypeInt  32      0\n"
                                 "%c_uint32_1 = OpConstant %uint32 1\n" +
                                 shaderVariablesStr;
        }

        std::string memModelOp;
        std::vector<const char *> spvExts;
        std::vector<const char *> spvCaps;
        ComputeShaderSpec spec;
        adjustSpecForUntypedPointers(spec, spvExts, spvCaps);
        adjustSpecForMemoryModel(memModel, spec, memModelOp, spvExts, spvCaps);
        adjustSpecForDataTypes(BASE_DATA_TYPE_CASES[i], spec, spvExts, spvCaps);
        adjustSpecForSmallContainerType(ContainerTypes::STORAGE_BUFFER, BASE_DATA_TYPE_CASES[i], spec, spvExts,
                                        spvCaps);

        specMap["memModelOp"]       = memModelOp;
        specMap["extensions"]       = toString(spvExts);
        specMap["capabilities"]     = toString(spvCaps);
        const std::string shaderAsm = shaderHeader.specialize(specMap) + shaderAnnotations.specialize(specMap) +
                                      shaderVariablesStr + shaderFunctions.specialize(specMap);

        FilledBufferDesc desc;
        desc.dataType  = BASE_DATA_TYPE_CASES[i];
        desc.elemCount = Constants::numThreads;
        desc.padding   = 0;
        desc.fillType  = FillingTypes::RANDOM;
        desc.seed      = deStringHash(testGroup->getName());

        Resource inputOutputResource = createFilledResource(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, desc);

        spec.assembly      = shaderAsm;
        spec.numWorkGroups = tcu::IVec3(Constants::numThreads, 1, 1);
        spec.inputs.push_back(inputOutputResource);
        spec.outputs.push_back(inputOutputResource);
        spec.extensions.push_back("VK_KHR_storage_buffer_storage_class");

        testGroup->addChild(new SpvAsmComputeShaderCase(testCtx, testName.c_str(), spec));
    }
}

void addStoreAtomicTests(tcu::TestCaseGroup *testGroup, MEMORY_MODEL_TYPE memModel)
{
    tcu::TestContext &testCtx = testGroup->getTestContext();

    const tcu::StringTemplate shaderHeader(createShaderHeader());

    const tcu::StringTemplate shaderAnnotations(createShaderAnnotations(AtomicTestCases::OP_ATOMIC_STORE));

    const tcu::StringTemplate shaderVariables(createShaderVariables(AtomicTestCases::OP_ATOMIC_STORE));

    const tcu::StringTemplate shaderFunctions(createShaderMain(AtomicTestCases::OP_ATOMIC_STORE));

    for (uint32_t i = 0; i < DE_LENGTH_OF_ARRAY(ATOMIC_DATA_TYPE_CASES); ++i)
    {
        std::string testName = toString(ATOMIC_DATA_TYPE_CASES[i]);

        std::map<std::string, std::string> specMap;
        specMap["alignment"]   = std::to_string(getSizeInBytes(ATOMIC_DATA_TYPE_CASES[i]));
        specMap["args"]        = STORE_OPERATION_CASES[1].pArgs;
        specMap["baseDecl"]    = getDeclaration(ATOMIC_DATA_TYPE_CASES[i]);
        specMap["baseType"]    = toString(ATOMIC_DATA_TYPE_CASES[i]);
        specMap["storeOp"]     = STORE_OPERATION_CASES[1].pOperation;
        specMap["threadCount"] = std::to_string(Constants::numThreads);

        std::string shaderVariablesStr = shaderVariables.specialize(specMap);
        if (ATOMIC_DATA_TYPE_CASES[i] != DataTypes::UINT32)
        {
            shaderVariablesStr = "%uint32     = OpTypeInt  32      0\n" + shaderVariablesStr;
        }

        std::string memModelOp;
        std::vector<const char *> spvExts;
        std::vector<const char *> spvCaps;
        ComputeShaderSpec spec;
        adjustSpecForUntypedPointers(spec, spvExts, spvCaps);
        adjustSpecForMemoryModel(memModel, spec, memModelOp, spvExts, spvCaps);
        adjustSpecForDataTypes(ATOMIC_DATA_TYPE_CASES[i], spec, spvExts, spvCaps);
        adjustSpecForAtomicOperations(ATOMIC_DATA_TYPE_CASES[i], spec, spvExts, spvCaps);

        specMap["memModelOp"]       = memModelOp;
        specMap["extensions"]       = toString(spvExts);
        specMap["capabilities"]     = toString(spvCaps);
        const std::string shaderAsm = shaderHeader.specialize(specMap) + shaderAnnotations.specialize(specMap) +
                                      shaderVariablesStr + shaderFunctions.specialize(specMap);

        FilledBufferDesc desc;
        desc.dataType  = ATOMIC_DATA_TYPE_CASES[i];
        desc.elemCount = Constants::numThreads;
        desc.padding   = 0;
        desc.fillType  = FillingTypes::RANDOM;
        desc.seed      = deStringHash(testGroup->getName());

        Resource inputOutputResource = createFilledResource(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, desc);

        spec.assembly      = shaderAsm;
        spec.numWorkGroups = tcu::IVec3(Constants::numThreads, 1, 1);
        spec.inputs.push_back(inputOutputResource);
        spec.outputs.push_back(inputOutputResource);
        spec.extensions.push_back("VK_KHR_storage_buffer_storage_class");

        testGroup->addChild(new SpvAsmComputeShaderCase(testCtx, testName.c_str(), spec));
    }
}

void addStoreMixedTypeTests(tcu::TestCaseGroup *testGroup, MEMORY_MODEL_TYPE memModel)
{
    tcu::TestContext &testCtx = testGroup->getTestContext();

    {
        const tcu::StringTemplate shaderHeader(createShaderHeader());

        const tcu::StringTemplate shaderAnnotations(
            createShaderAnnotations(TypePunningTestCases::STORE_SAME_SIZE_TYPES));

        const tcu::StringTemplate shaderVariables(createShaderVariables(TypePunningTestCases::STORE_SAME_SIZE_TYPES));

        const tcu::StringTemplate shaderFunctions(createShaderMain(TypePunningTestCases::STORE_SAME_SIZE_TYPES));

        for (uint32_t i = 0; i < DE_LENGTH_OF_ARRAY(BASE_DATA_TYPE_CASES); ++i)
        {
            std::vector<DATA_TYPE> sameSizeTypes = getSameSizeBaseDataType(BASE_DATA_TYPE_CASES[i]);

            for (uint32_t k = 0; k < sameSizeTypes.size(); ++k)
            {
                const DATA_TYPE dataType  = sameSizeTypes[k];
                const uint32_t otherIndex = static_cast<uint32_t>(dataType);

                std::string testName = toString(BASE_DATA_TYPE_CASES[i]) + std::string("_to_") + toString(dataType);

                std::map<std::string, std::string> specMap;
                specMap["alignment"]    = std::to_string(getSizeInBytes(BASE_DATA_TYPE_CASES[i]));
                specMap["args"]         = STORE_OPERATION_CASES[0].pArgs;
                specMap["baseType"]     = toString(BASE_DATA_TYPE_CASES[i]);
                specMap["baseDecl"]     = getDeclaration(BASE_DATA_TYPE_CASES[i]);
                specMap["sameSizeType"] = toString(BASE_DATA_TYPE_CASES[otherIndex]);
                specMap["sameSizeDecl"] = getDeclaration(BASE_DATA_TYPE_CASES[otherIndex]);
                specMap["storeOp"]      = STORE_OPERATION_CASES[0].pOperation;
                specMap["threadCount"]  = std::to_string(Constants::numThreads);
                specMap["otherCap"]     = getCapability(dataType);

                std::string shaderVariablesStr = shaderVariables.specialize(specMap);
                if ((BASE_DATA_TYPE_CASES[i] != DataTypes::UINT32) && (dataType != DataTypes::UINT32))
                {
                    shaderVariablesStr = "%uint32     = OpTypeInt 32 0\n"
                                         "%c_uint32_1 = OpConstant %uint32 1\n" +
                                         shaderVariablesStr;
                }

                std::string memModelOp;
                std::vector<const char *> spvExts;
                std::vector<const char *> spvCaps;
                ComputeShaderSpec spec;
                adjustSpecForUntypedPointers(spec, spvExts, spvCaps);
                adjustSpecForMemoryModel(memModel, spec, memModelOp, spvExts, spvCaps);
                adjustSpecForDataTypes(BASE_DATA_TYPE_CASES[i], spec, spvExts, spvCaps);
                if (BASE_DATA_TYPE_CASES[i] != dataType)
                    adjustSpecForDataTypes(dataType, spec, spvExts, spvCaps);
                adjustSpecForSmallContainerType(ContainerTypes::STORAGE_BUFFER, BASE_DATA_TYPE_CASES[i], spec, spvExts,
                                                spvCaps);
                if (getSizeInBytes(BASE_DATA_TYPE_CASES[i]) != getSizeInBytes(dataType))
                    adjustSpecForSmallContainerType(ContainerTypes::STORAGE_BUFFER, dataType, spec, spvExts, spvCaps);

                specMap["memModelOp"]       = memModelOp;
                specMap["extensions"]       = toString(spvExts);
                specMap["capabilities"]     = toString(spvCaps);
                const std::string shaderAsm = shaderHeader.specialize(specMap) + shaderAnnotations.specialize(specMap) +
                                              shaderVariablesStr + shaderFunctions.specialize(specMap);

                FilledBufferDesc desc;
                desc.dataType  = BASE_DATA_TYPE_CASES[i];
                desc.elemCount = Constants::numThreads;
                desc.padding   = 0;
                desc.fillType  = FillingTypes::RANDOM;
                desc.seed      = deStringHash(testGroup->getName());

                Resource inputOutputResource = createFilledResource(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, desc);

                spec.assembly      = shaderAsm;
                spec.numWorkGroups = tcu::IVec3(Constants::numThreads, 1, 1);
                spec.inputs.push_back(inputOutputResource);
                spec.outputs.push_back(inputOutputResource);
                spec.extensions.push_back("VK_KHR_storage_buffer_storage_class");

                testGroup->addChild(new SpvAsmComputeShaderCase(testCtx, testName.c_str(), spec));
            }
        }
    }

    {
        const tcu::StringTemplate shaderHeader(createShaderHeader());

        const tcu::StringTemplate shaderAnnotations(createShaderAnnotations(TypePunningTestCases::STORE_SCALAR_VECTOR));

        const tcu::StringTemplate shaderVariables(createShaderVariables(TypePunningTestCases::STORE_SCALAR_VECTOR));

        const tcu::StringTemplate shaderFunctions(createShaderMain(TypePunningTestCases::STORE_SCALAR_VECTOR));

        for (uint32_t i = 0; i < DE_LENGTH_OF_ARRAY(BASE_DATA_TYPE_CASES); ++i)
        {
            std::vector<COMPOSITE_DATA_TYPE> sameSizeTypes = getSameSizeCompositeType(BASE_DATA_TYPE_CASES[i]);

            for (uint32_t k = 0; k < sameSizeTypes.size(); ++k)
            {
                const COMPOSITE_DATA_TYPE compositeType = sameSizeTypes[k];

                std::string testName =
                    toString(BASE_DATA_TYPE_CASES[i]) + std::string("_to_") + toString(compositeType);

                std::map<std::string, std::string> specMap;
                specMap["alignment"]     = std::to_string(getSizeInBytes(BASE_DATA_TYPE_CASES[i]));
                specMap["args"]          = STORE_OPERATION_CASES[0].pArgs;
                specMap["baseType"]      = toString(BASE_DATA_TYPE_CASES[i]);
                specMap["otherType"]     = toString(getCompositeBaseDataType(compositeType));
                specMap["baseDecl"]      = getDeclaration(BASE_DATA_TYPE_CASES[i]);
                specMap["otherTypeDecl"] = getDeclaration(getCompositeBaseDataType(compositeType));
                specMap["storeOp"]       = STORE_OPERATION_CASES[0].pOperation;
                specMap["otherVec"]      = toString(compositeType);
                specMap["otherVecDecl"]  = getDeclaration(compositeType);
                specMap["otherCap"]      = getCapability(compositeType);

                if (sameSizeTypes[k] != CompositeDataTypes::VEC3_UINT32)
                    specMap["inputVec"] = "%vec3_uint32 = OpTypeVector %uint32 3";

                std::string shaderVariablesStr = shaderVariables.specialize(specMap);
                if ((BASE_DATA_TYPE_CASES[i] != DataTypes::UINT32) &&
                    (getCompositeBaseDataType(compositeType) != DataTypes::UINT32) &&
                    compositeType != CompositeDataTypes::VEC2_UINT32 &&
                    compositeType != CompositeDataTypes::VEC3_UINT32 &&
                    compositeType != CompositeDataTypes::VEC4_UINT32)
                {
                    shaderVariablesStr = "%uint32      = OpTypeInt 32 0\n"
                                         "%c_uint32_1  = OpConstant %uint32 1\n" +
                                         shaderVariablesStr;
                }

                std::string memModelOp;
                std::vector<const char *> spvExts;
                std::vector<const char *> spvCaps;
                ComputeShaderSpec spec;
                adjustSpecForUntypedPointers(spec, spvExts, spvCaps);
                adjustSpecForMemoryModel(memModel, spec, memModelOp, spvExts, spvCaps);
                adjustSpecForDataTypes(BASE_DATA_TYPE_CASES[i], spec, spvExts, spvCaps);
                if (BASE_DATA_TYPE_CASES[i] != getCompositeBaseDataType(compositeType))
                    adjustSpecForDataTypes(getCompositeBaseDataType(compositeType), spec, spvExts, spvCaps);
                adjustSpecForSmallContainerType(ContainerTypes::STORAGE_BUFFER, BASE_DATA_TYPE_CASES[i], spec, spvExts,
                                                spvCaps);
                if (getSizeInBytes(BASE_DATA_TYPE_CASES[i]) != getSizeInBytes(getCompositeBaseDataType(compositeType)))
                    adjustSpecForSmallContainerType(ContainerTypes::STORAGE_BUFFER,
                                                    getCompositeBaseDataType(compositeType), spec, spvExts, spvCaps);

                specMap["memModelOp"]       = memModelOp;
                specMap["extensions"]       = toString(spvExts);
                specMap["capabilities"]     = toString(spvCaps);
                const std::string shaderAsm = shaderHeader.specialize(specMap) + shaderAnnotations.specialize(specMap) +
                                              shaderVariablesStr + shaderFunctions.specialize(specMap);

                FilledBufferDesc desc;
                desc.dataType  = BASE_DATA_TYPE_CASES[i];
                desc.elemCount = 1;
                desc.padding   = 0;
                desc.fillType  = FillingTypes::VALUE;
                desc.value     = 1;

                Resource inputOutputResource = createFilledResource(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, desc);

                spec.assembly      = shaderAsm;
                spec.numWorkGroups = tcu::IVec3(1, 1, 1);
                spec.inputs.push_back(inputOutputResource);
                spec.outputs.push_back(inputOutputResource);
                spec.extensions.push_back("VK_KHR_storage_buffer_storage_class");

                testGroup->addChild(new SpvAsmComputeShaderCase(testCtx, testName.c_str(), spec));
            }
        }
    }

    {
        const tcu::StringTemplate shaderHeader(createShaderHeader());

        const tcu::StringTemplate shaderAnnotations(createShaderAnnotations(TypePunningTestCases::STORE_VECTOR_SCALAR));

        const tcu::StringTemplate shaderVariables(createShaderVariables(TypePunningTestCases::STORE_VECTOR_SCALAR));

        const tcu::StringTemplate shaderFunctions(createShaderMain(TypePunningTestCases::STORE_VECTOR_SCALAR));

        for (uint32_t i = 0; i < DE_LENGTH_OF_ARRAY(COMPOSITE_DATA_TYPE_CASES); ++i)
        {
            std::vector<DATA_TYPE> sameSizeTypes = getSameSizeBaseDataType(COMPOSITE_DATA_TYPE_CASES[i]);

            for (uint32_t k = 0; k < sameSizeTypes.size(); ++k)
            {
                const DATA_TYPE dataType  = sameSizeTypes[k];
                const uint32_t otherIndex = static_cast<uint32_t>(dataType);

                std::string testName =
                    toString(COMPOSITE_DATA_TYPE_CASES[i]) + std::string("_to_") + toString(dataType);

                std::map<std::string, std::string> specMap;
                specMap["args"]          = STORE_OPERATION_CASES[0].pArgs;
                specMap["storeOp"]       = STORE_OPERATION_CASES[0].pOperation;
                specMap["baseType"]      = toString(getCompositeBaseDataType(COMPOSITE_DATA_TYPE_CASES[i]));
                specMap["otherType"]     = toString(dataType);
                specMap["baseDecl"]      = getDeclaration(getCompositeBaseDataType(COMPOSITE_DATA_TYPE_CASES[i]));
                specMap["otherTypeDecl"] = getDeclaration(BASE_DATA_TYPE_CASES[otherIndex]);
                specMap["otherCap"]      = getCapability(dataType);
                specMap["baseVec"]       = toString(COMPOSITE_DATA_TYPE_CASES[i]);
                specMap["baseVecDecl"]   = getDeclaration(COMPOSITE_DATA_TYPE_CASES[i]);

                if (COMPOSITE_DATA_TYPE_CASES[i] != CompositeDataTypes::VEC3_UINT32)
                    specMap["inputVec"] = "%vec3_uint32 = OpTypeVector %uint32 3";

                std::string shaderVariablesStr = shaderVariables.specialize(specMap);
                if ((getCompositeBaseDataType(COMPOSITE_DATA_TYPE_CASES[i]) != DataTypes::UINT32) &&
                    (dataType != DataTypes::UINT32))
                {
                    shaderVariablesStr = "%uint32      = OpTypeInt 32 0\n"
                                         "%c_uint32_1  = OpConstant %uint32 1\n" +
                                         shaderVariablesStr;
                }

                std::string memModelOp;
                std::vector<const char *> spvExts;
                std::vector<const char *> spvCaps;
                ComputeShaderSpec spec;
                adjustSpecForUntypedPointers(spec, spvExts, spvCaps);
                adjustSpecForMemoryModel(memModel, spec, memModelOp, spvExts, spvCaps);
                adjustSpecForDataTypes(getCompositeBaseDataType(COMPOSITE_DATA_TYPE_CASES[i]), spec, spvExts, spvCaps);
                if (getCompositeBaseDataType(COMPOSITE_DATA_TYPE_CASES[i]) != dataType)
                    adjustSpecForDataTypes(dataType, spec, spvExts, spvCaps);
                adjustSpecForSmallContainerType(ContainerTypes::STORAGE_BUFFER,
                                                getCompositeBaseDataType(COMPOSITE_DATA_TYPE_CASES[i]), spec, spvExts,
                                                spvCaps);
                if (getSizeInBytes(getCompositeBaseDataType(COMPOSITE_DATA_TYPE_CASES[i])) != getSizeInBytes(dataType))
                    adjustSpecForSmallContainerType(ContainerTypes::STORAGE_BUFFER, dataType, spec, spvExts, spvCaps);

                specMap["memModelOp"]       = memModelOp;
                specMap["extensions"]       = toString(spvExts);
                specMap["capabilities"]     = toString(spvCaps);
                const std::string shaderAsm = shaderHeader.specialize(specMap) + shaderAnnotations.specialize(specMap) +
                                              shaderVariablesStr + shaderFunctions.specialize(specMap);

                FilledBufferDesc desc;
                desc.dataType  = getCompositeBaseDataType(COMPOSITE_DATA_TYPE_CASES[i]);
                desc.elemCount = getElementCount(COMPOSITE_DATA_TYPE_CASES[i]);
                desc.fillType  = FillingTypes::VALUE;
                desc.value     = 1;
                desc.padding   = 0;

                Resource inputOutputResource = createFilledResource(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, desc);

                spec.assembly      = shaderAsm;
                spec.numWorkGroups = tcu::IVec3(1, 1, 1);
                spec.inputs.push_back(inputOutputResource);
                spec.outputs.push_back(inputOutputResource);
                spec.extensions.push_back("VK_KHR_storage_buffer_storage_class");

                testGroup->addChild(new SpvAsmComputeShaderCase(testCtx, testName.c_str(), spec));
            }
        }
    }
}

void addCopyTests(tcu::TestCaseGroup *testGroup, MEMORY_MODEL_TYPE memModel, bool fromUntyped)
{
    tcu::TestContext &testCtx = testGroup->getTestContext();

    const BASE_TEST_CASE testCase = fromUntyped ? BaseTestCases::COPY_FROM : BaseTestCases::COPY_TO;

    de::MovePtr<tcu::TestCaseGroup> objectGroup(new tcu::TestCaseGroup(testCtx, "op_copy_object", ""));
    de::MovePtr<tcu::TestCaseGroup> memoryGroup(new tcu::TestCaseGroup(testCtx, "op_copy_memory", ""));
    de::MovePtr<tcu::TestCaseGroup> memorySizedGroup(new tcu::TestCaseGroup(testCtx, "op_copy_memory_sized", ""));

    const tcu::StringTemplate shaderHeader(createShaderHeader());

    const tcu::StringTemplate shaderAnnotations(createShaderAnnotations(testCase));

    const tcu::StringTemplate shaderVariables(createShaderVariables(testCase));

    const tcu::StringTemplate shaderFunctions(createShaderMain(testCase));

    for (uint32_t i = 0; i < DE_LENGTH_OF_ARRAY(BASE_DATA_TYPE_CASES); ++i)
    {
        for (uint32_t j = 0; j < DE_LENGTH_OF_ARRAY(COPY_OPERATION_CASES); ++j)
        {
            std::string testName = toString(BASE_DATA_TYPE_CASES[i]);

            std::map<std::string, std::string> specMap;
            specMap["copyOp"]      = COPY_OPERATION_CASES[j].pCopyOp;
            specMap["alignment"]   = std::to_string(getSizeInBytes(BASE_DATA_TYPE_CASES[i]));
            specMap["baseDecl"]    = getDeclaration(BASE_DATA_TYPE_CASES[i]);
            specMap["baseType"]    = toString(BASE_DATA_TYPE_CASES[i]);
            specMap["threadCount"] = std::to_string(Constants::numThreads);
            specMap["size"]        = std::to_string(getSizeInBytes(BASE_DATA_TYPE_CASES[i]));
            specMap["copyType"]    = toString(BASE_DATA_TYPE_CASES[i]);

            const tcu::StringTemplate tempShaderFunctions = tcu::StringTemplate(shaderFunctions.specialize(specMap));
            std::string shaderVariablesStr                = shaderVariables.specialize(specMap);
            if (BASE_DATA_TYPE_CASES[i] != DataTypes::UINT32)
            {
                shaderVariablesStr = "%uint32     = OpTypeInt 32 0\n"
                                     "%c_uint32_1 = OpConstant %uint32 1\n" +
                                     shaderVariablesStr;
            }

            std::string memModelOp;
            std::vector<const char *> spvExts;
            std::vector<const char *> spvCaps;
            ComputeShaderSpec spec;
            adjustSpecForUntypedPointers(spec, spvExts, spvCaps);
            adjustSpecForMemoryModel(memModel, spec, memModelOp, spvExts, spvCaps);
            adjustSpecForDataTypes(BASE_DATA_TYPE_CASES[i], spec, spvExts, spvCaps);
            adjustSpecForSmallContainerType(ContainerTypes::STORAGE_BUFFER, BASE_DATA_TYPE_CASES[i], spec, spvExts,
                                            spvCaps);

            specMap["memModelOp"]       = memModelOp;
            specMap["extensions"]       = toString(spvExts);
            specMap["capabilities"]     = toString(spvCaps);
            const std::string shaderAsm = shaderHeader.specialize(specMap) + shaderAnnotations.specialize(specMap) +
                                          shaderVariablesStr + tempShaderFunctions.specialize(specMap);

            FilledBufferDesc desc;
            desc.dataType  = BASE_DATA_TYPE_CASES[i];
            desc.elemCount = Constants::numThreads;
            desc.padding   = 0;
            desc.fillType  = FillingTypes::RANDOM;
            desc.seed      = deStringHash(testGroup->getName());

            Resource inputOutputResource = createFilledResource(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, desc);

            spec.assembly      = shaderAsm;
            spec.numWorkGroups = tcu::IVec3(Constants::numThreads, 1, 1);
            spec.inputs.push_back(inputOutputResource);
            spec.outputs.push_back(inputOutputResource);
            spec.extensions.push_back("VK_KHR_storage_buffer_storage_class");

            if (COPY_OPERATION_CASES[j].type == CopyOperationTypes::COPY_OBJECT)
            {
                objectGroup->addChild(new SpvAsmComputeShaderCase(testCtx, testName.c_str(), spec));
            }
            else if (COPY_OPERATION_CASES[j].type == CopyOperationTypes::COPY_MEMORY)
            {
                memoryGroup->addChild(new SpvAsmComputeShaderCase(testCtx, testName.c_str(), spec));
            }
            else // COPY_OPERATION_CASES[j].type == CopyOperationTypes::COPY_MEMORY_SIZED
            {
                memorySizedGroup->addChild(new SpvAsmComputeShaderCase(testCtx, testName.c_str(), spec));
            }
        }
    }

    testGroup->addChild(objectGroup.release());
    testGroup->addChild(memoryGroup.release());
    testGroup->addChild(memorySizedGroup.release());
}

void addCopyFromUntypedMixedTypeTests(tcu::TestCaseGroup *testGroup, MEMORY_MODEL_TYPE memModel)
{
    tcu::TestContext &testCtx = testGroup->getTestContext();

    de::MovePtr<tcu::TestCaseGroup> memoryGroup(new tcu::TestCaseGroup(testCtx, "op_copy_memory", ""));
    de::MovePtr<tcu::TestCaseGroup> memorySizedGroup(new tcu::TestCaseGroup(testCtx, "op_copy_memory_sized", ""));

    {
        const tcu::StringTemplate shaderHeader(createShaderHeader());

        const tcu::StringTemplate shaderAnnotations(
            createShaderAnnotations(TypePunningTestCases::COPY_FROM_SAME_SIZE_TYPES));

        const tcu::StringTemplate shaderVariables(
            createShaderVariables(TypePunningTestCases::COPY_FROM_SAME_SIZE_TYPES));

        const tcu::StringTemplate shaderFunctions(createShaderMain(TypePunningTestCases::COPY_FROM_SAME_SIZE_TYPES));

        for (uint32_t i = 0; i < DE_LENGTH_OF_ARRAY(BASE_DATA_TYPE_CASES); ++i)
        {
            for (uint32_t j = 1; j < DE_LENGTH_OF_ARRAY(COPY_OPERATION_CASES); ++j)
            {
                std::vector<DATA_TYPE> sameSizeTypes = getSameSizeBaseDataType(BASE_DATA_TYPE_CASES[i]);

                for (uint32_t k = 0; k < sameSizeTypes.size(); ++k)
                {
                    const DATA_TYPE dataType  = sameSizeTypes[k];
                    const uint32_t otherIndex = static_cast<uint32_t>(dataType);

                    std::string testName =
                        std::string(toString(BASE_DATA_TYPE_CASES[i])) + "_to_" + toString(sameSizeTypes[k]);

                    std::map<std::string, std::string> specMap;
                    specMap["copyOp"]       = COPY_OPERATION_CASES[j].pCopyOp;
                    specMap["alignment"]    = std::to_string(getSizeInBytes(BASE_DATA_TYPE_CASES[i]));
                    specMap["baseDecl"]     = getDeclaration(BASE_DATA_TYPE_CASES[i]);
                    specMap["sameSizeDecl"] = getDeclaration(BASE_DATA_TYPE_CASES[otherIndex]);
                    specMap["baseType"]     = toString(BASE_DATA_TYPE_CASES[i]);
                    specMap["sameSizeType"] = toString(BASE_DATA_TYPE_CASES[otherIndex]);
                    specMap["threadCount"]  = std::to_string(Constants::numThreads);
                    specMap["size"]         = std::to_string(getSizeInBytes(BASE_DATA_TYPE_CASES[i]));
                    specMap["otherCap"]     = getCapability(dataType);
                    specMap["copyType"]     = toString(BASE_DATA_TYPE_CASES[i]);

                    const tcu::StringTemplate tempShaderFunctions =
                        tcu::StringTemplate(shaderFunctions.specialize(specMap));
                    std::string shaderVariablesStr = shaderVariables.specialize(specMap);
                    if ((BASE_DATA_TYPE_CASES[i] != DataTypes::UINT32) && (sameSizeTypes[k] != DataTypes::UINT32))
                    {
                        shaderVariablesStr = "%uint32     = OpTypeInt 32 0\n"
                                             "%c_uint32_1 = OpConstant %uint32 1\n" +
                                             shaderVariablesStr;
                    }

                    std::string memModelOp;
                    std::vector<const char *> spvExts;
                    std::vector<const char *> spvCaps;
                    ComputeShaderSpec spec;
                    adjustSpecForUntypedPointers(spec, spvExts, spvCaps);
                    adjustSpecForMemoryModel(memModel, spec, memModelOp, spvExts, spvCaps);
                    adjustSpecForDataTypes(BASE_DATA_TYPE_CASES[i], spec, spvExts, spvCaps);
                    if (BASE_DATA_TYPE_CASES[i] != dataType)
                        adjustSpecForDataTypes(dataType, spec, spvExts, spvCaps);
                    adjustSpecForSmallContainerType(ContainerTypes::STORAGE_BUFFER, BASE_DATA_TYPE_CASES[i], spec,
                                                    spvExts, spvCaps);

                    specMap["memModelOp"]       = memModelOp;
                    specMap["extensions"]       = toString(spvExts);
                    specMap["capabilities"]     = toString(spvCaps);
                    const std::string shaderAsm = shaderHeader.specialize(specMap) +
                                                  shaderAnnotations.specialize(specMap) + shaderVariablesStr +
                                                  tempShaderFunctions.specialize(specMap);

                    FilledBufferDesc desc;
                    desc.dataType  = BASE_DATA_TYPE_CASES[i];
                    desc.elemCount = Constants::numThreads;
                    desc.padding   = 0;
                    desc.fillType  = FillingTypes::RANDOM;
                    desc.seed      = deStringHash(testGroup->getName());

                    Resource inputOutputResource = createFilledResource(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, desc);

                    spec.assembly      = shaderAsm;
                    spec.numWorkGroups = tcu::IVec3(Constants::numThreads, 1, 1);
                    spec.inputs.push_back(inputOutputResource);
                    spec.outputs.push_back(inputOutputResource);
                    spec.extensions.push_back("VK_KHR_storage_buffer_storage_class");

                    if (COPY_OPERATION_CASES[j].type == CopyOperationTypes::COPY_MEMORY)
                    {
                        memoryGroup->addChild(new SpvAsmComputeShaderCase(testCtx, testName.c_str(), spec));
                    }
                    else if (COPY_OPERATION_CASES[j].type == CopyOperationTypes::COPY_MEMORY_SIZED)
                    {
                        memorySizedGroup->addChild(new SpvAsmComputeShaderCase(testCtx, testName.c_str(), spec));
                    }
                }
            }
        }
    }

    {
        const tcu::StringTemplate shaderHeader(createShaderHeader());

        const tcu::StringTemplate shaderAnnotations(
            createShaderAnnotations(TypePunningTestCases::COPY_FROM_SCALAR_VECTOR));

        const tcu::StringTemplate shaderVariables(createShaderVariables(TypePunningTestCases::COPY_FROM_SCALAR_VECTOR));

        const tcu::StringTemplate shaderFunctions(createShaderMain(TypePunningTestCases::COPY_FROM_SCALAR_VECTOR));

        for (uint32_t i = 0; i < DE_LENGTH_OF_ARRAY(BASE_DATA_TYPE_CASES); ++i)
        {
            for (uint32_t j = 1; j < DE_LENGTH_OF_ARRAY(COPY_OPERATION_CASES); ++j)
            {
                std::vector<COMPOSITE_DATA_TYPE> sameSizeTypes = getSameSizeCompositeType(BASE_DATA_TYPE_CASES[i]);

                for (uint32_t k = 0; k < sameSizeTypes.size(); ++k)
                {
                    const COMPOSITE_DATA_TYPE compositeType = sameSizeTypes[k];
                    const uint32_t otherIndex = static_cast<uint32_t>(getCompositeBaseDataType(compositeType));

                    std::string testName =
                        std::string(toString(BASE_DATA_TYPE_CASES[i])) + "_to_" + toString(compositeType);

                    std::map<std::string, std::string> specMap;
                    specMap["copyOp"]        = COPY_OPERATION_CASES[j].pCopyOp;
                    specMap["baseType"]      = toString(BASE_DATA_TYPE_CASES[i]);
                    specMap["baseDecl"]      = getDeclaration(BASE_DATA_TYPE_CASES[i]);
                    specMap["sameSizeDecl"]  = getDeclaration(BASE_DATA_TYPE_CASES[otherIndex]);
                    specMap["otherType"]     = toString(getCompositeBaseDataType(compositeType));
                    specMap["sameSizeType"]  = toString(BASE_DATA_TYPE_CASES[otherIndex]);
                    specMap["otherTypeDecl"] = getDeclaration(BASE_DATA_TYPE_CASES[otherIndex]);
                    specMap["otherVec"]      = toString(compositeType);
                    specMap["otherVecDecl"]  = getDeclaration(compositeType);
                    specMap["size"]          = std::to_string(getSizeInBytes(BASE_DATA_TYPE_CASES[i]));
                    specMap["otherCap"]      = getCapability(compositeType);
                    specMap["copyType"]      = toString(BASE_DATA_TYPE_CASES[i]);

                    if (compositeType != CompositeDataTypes::VEC3_UINT32)
                        specMap["inputVec"] = "%vec3_uint32 = OpTypeVector %uint32 3";

                    std::string shaderVariablesStr = shaderVariables.specialize(specMap);
                    if ((BASE_DATA_TYPE_CASES[i] != DataTypes::UINT32) &&
                        (getCompositeBaseDataType(compositeType) != DataTypes::UINT32) &&
                        compositeType != CompositeDataTypes::VEC2_UINT32 &&
                        compositeType != CompositeDataTypes::VEC3_UINT32 &&
                        compositeType != CompositeDataTypes::VEC4_UINT32)
                    {
                        shaderVariablesStr = "%uint32      = OpTypeInt 32 0\n"
                                             "%c_uint32_1  = OpConstant %uint32 1\n" +
                                             shaderVariablesStr;
                    }

                    std::string memModelOp;
                    std::vector<const char *> spvExts;
                    std::vector<const char *> spvCaps;
                    ComputeShaderSpec spec;
                    adjustSpecForUntypedPointers(spec, spvExts, spvCaps);
                    adjustSpecForMemoryModel(memModel, spec, memModelOp, spvExts, spvCaps);
                    adjustSpecForDataTypes(BASE_DATA_TYPE_CASES[i], spec, spvExts, spvCaps);
                    if (BASE_DATA_TYPE_CASES[i] != getCompositeBaseDataType(compositeType))
                        adjustSpecForDataTypes(getCompositeBaseDataType(compositeType), spec, spvExts, spvCaps);
                    adjustSpecForSmallContainerType(ContainerTypes::STORAGE_BUFFER, BASE_DATA_TYPE_CASES[i], spec,
                                                    spvExts, spvCaps);

                    specMap["memModelOp"]       = memModelOp;
                    specMap["extensions"]       = toString(spvExts);
                    specMap["capabilities"]     = toString(spvCaps);
                    const std::string shaderAsm = shaderHeader.specialize(specMap) +
                                                  shaderAnnotations.specialize(specMap) + shaderVariablesStr +
                                                  shaderFunctions.specialize(specMap);

                    FilledBufferDesc desc;
                    desc.dataType  = BASE_DATA_TYPE_CASES[i];
                    desc.elemCount = 1;
                    desc.fillType  = FillingTypes::VALUE;
                    desc.value     = 1;
                    desc.padding   = 0;

                    Resource inputOutputResource = createFilledResource(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, desc);

                    spec.assembly      = shaderAsm;
                    spec.numWorkGroups = tcu::IVec3(1, 1, 1);
                    spec.inputs.push_back(inputOutputResource);
                    spec.outputs.push_back(inputOutputResource);
                    spec.extensions.push_back("VK_KHR_storage_buffer_storage_class");

                    if (COPY_OPERATION_CASES[j].type == CopyOperationTypes::COPY_MEMORY)
                    {
                        memoryGroup->addChild(new SpvAsmComputeShaderCase(testCtx, testName.c_str(), spec));
                    }
                    else if (COPY_OPERATION_CASES[j].type == CopyOperationTypes::COPY_MEMORY_SIZED)
                    {
                        memorySizedGroup->addChild(new SpvAsmComputeShaderCase(testCtx, testName.c_str(), spec));
                    }
                }
            }
        }
    }

    {
        const tcu::StringTemplate shaderHeader(createShaderHeader());

        const tcu::StringTemplate shaderAnnotations(
            createShaderAnnotations(TypePunningTestCases::COPY_FROM_VECTOR_SCALAR));

        const tcu::StringTemplate shaderVariables(createShaderVariables(TypePunningTestCases::COPY_FROM_VECTOR_SCALAR));

        const tcu::StringTemplate shaderFunctions(createShaderMain(TypePunningTestCases::COPY_FROM_VECTOR_SCALAR));

        for (uint32_t i = 0; i < DE_LENGTH_OF_ARRAY(COMPOSITE_DATA_TYPE_CASES); ++i)
        {
            for (uint32_t j = 1; j < DE_LENGTH_OF_ARRAY(COPY_OPERATION_CASES); ++j)
            {
                std::vector<DATA_TYPE> sameSizeTypes = getSameSizeBaseDataType(COMPOSITE_DATA_TYPE_CASES[i]);

                for (uint32_t k = 0; k < sameSizeTypes.size(); ++k)
                {
                    const DATA_TYPE dataType  = sameSizeTypes[k];
                    const uint32_t otherIndex = static_cast<uint32_t>(dataType);

                    std::string testName =
                        std::string(toString(COMPOSITE_DATA_TYPE_CASES[i])) + "_to_" + toString(dataType);
                    std::string testDesc = "Test " + std::string(toString(COPY_OPERATION_CASES[j].type)) +
                                           " operation from untyped " + toString(COMPOSITE_DATA_TYPE_CASES[i]) +
                                           " to " + toString(dataType);

                    std::map<std::string, std::string> specMap;
                    specMap["copyOp"]        = COPY_OPERATION_CASES[j].pCopyOp;
                    specMap["baseType"]      = toString(getCompositeBaseDataType(COMPOSITE_DATA_TYPE_CASES[i]));
                    specMap["baseDecl"]      = getDeclaration(getCompositeBaseDataType(COMPOSITE_DATA_TYPE_CASES[i]));
                    specMap["otherType"]     = toString(dataType);
                    specMap["otherTypeDecl"] = getDeclaration(BASE_DATA_TYPE_CASES[otherIndex]);
                    specMap["baseVec"]       = toString(COMPOSITE_DATA_TYPE_CASES[i]);
                    specMap["baseVecDecl"]   = getDeclaration(COMPOSITE_DATA_TYPE_CASES[i]);
                    specMap["size"]          = std::to_string(getSizeInBytes(COMPOSITE_DATA_TYPE_CASES[i]));
                    specMap["otherCap"]      = getCapability(dataType);
                    specMap["copyType"]      = toString(COMPOSITE_DATA_TYPE_CASES[i]);

                    if (COMPOSITE_DATA_TYPE_CASES[i] != CompositeDataTypes::VEC3_UINT32)
                        specMap["inputVec"] = "%vec3_uint32 = OpTypeVector %uint32 3";

                    std::string shaderVariablesStr = shaderVariables.specialize(specMap);
                    if ((getCompositeBaseDataType(COMPOSITE_DATA_TYPE_CASES[i]) != DataTypes::UINT32) &&
                        (dataType != DataTypes::UINT32))
                    {
                        shaderVariablesStr = "%uint32      = OpTypeInt 32 0\n"
                                             "%c_uint32_1  = OpConstant %uint32 1\n" +
                                             shaderVariablesStr;
                    }

                    std::string memModelOp;
                    std::vector<const char *> spvExts;
                    std::vector<const char *> spvCaps;
                    ComputeShaderSpec spec;
                    adjustSpecForUntypedPointers(spec, spvExts, spvCaps);
                    adjustSpecForMemoryModel(memModel, spec, memModelOp, spvExts, spvCaps);
                    adjustSpecForDataTypes(getCompositeBaseDataType(COMPOSITE_DATA_TYPE_CASES[i]), spec, spvExts,
                                           spvCaps);
                    if (getCompositeBaseDataType(COMPOSITE_DATA_TYPE_CASES[i]) != dataType)
                        adjustSpecForDataTypes(dataType, spec, spvExts, spvCaps);
                    adjustSpecForSmallContainerType(ContainerTypes::STORAGE_BUFFER,
                                                    getCompositeBaseDataType(COMPOSITE_DATA_TYPE_CASES[i]), spec,
                                                    spvExts, spvCaps);

                    specMap["memModelOp"]       = memModelOp;
                    specMap["extensions"]       = toString(spvExts);
                    specMap["capabilities"]     = toString(spvCaps);
                    const std::string shaderAsm = shaderHeader.specialize(specMap) +
                                                  shaderAnnotations.specialize(specMap) + shaderVariablesStr +
                                                  shaderFunctions.specialize(specMap);

                    FilledBufferDesc desc;
                    desc.dataType  = getCompositeBaseDataType(COMPOSITE_DATA_TYPE_CASES[i]);
                    desc.elemCount = getElementCount(COMPOSITE_DATA_TYPE_CASES[i]);
                    desc.fillType  = FillingTypes::VALUE;
                    desc.value     = 1;
                    desc.padding   = 0;

                    Resource inputOutputResource = createFilledResource(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, desc);

                    spec.assembly      = shaderAsm;
                    spec.numWorkGroups = tcu::IVec3(1, 1, 1);
                    spec.inputs.push_back(inputOutputResource);
                    spec.outputs.push_back(inputOutputResource);
                    spec.extensions.push_back("VK_KHR_storage_buffer_storage_class");

                    if (COPY_OPERATION_CASES[j].type == CopyOperationTypes::COPY_MEMORY)
                    {
                        memoryGroup->addChild(new SpvAsmComputeShaderCase(testCtx, testName.c_str(), spec));
                    }
                    else if (COPY_OPERATION_CASES[j].type == CopyOperationTypes::COPY_MEMORY_SIZED)
                    {
                        memorySizedGroup->addChild(new SpvAsmComputeShaderCase(testCtx, testName.c_str(), spec));
                    }
                }
            }
        }
    }

    testGroup->addChild(memoryGroup.release());
    testGroup->addChild(memorySizedGroup.release());
}

void addCopyToUntypedMixedTypeTests(tcu::TestCaseGroup *testGroup, MEMORY_MODEL_TYPE memModel)
{
    tcu::TestContext &testCtx = testGroup->getTestContext();

    de::MovePtr<tcu::TestCaseGroup> memoryGroup(new tcu::TestCaseGroup(testCtx, "op_copy_memory", ""));
    de::MovePtr<tcu::TestCaseGroup> memorySizedGroup(new tcu::TestCaseGroup(testCtx, "op_copy_memory_sized", ""));

    {
        const tcu::StringTemplate shaderHeader(createShaderHeader());

        const tcu::StringTemplate shaderAnnotations(
            createShaderAnnotations(TypePunningTestCases::COPY_TO_SAME_SIZE_TYPES));

        const tcu::StringTemplate shaderVariables(createShaderVariables(TypePunningTestCases::COPY_TO_SAME_SIZE_TYPES));

        const tcu::StringTemplate shaderFunctions(createShaderMain(TypePunningTestCases::COPY_TO_SAME_SIZE_TYPES));

        for (uint32_t i = 0; i < DE_LENGTH_OF_ARRAY(BASE_DATA_TYPE_CASES); ++i)
        {
            for (uint32_t j = 1; j < DE_LENGTH_OF_ARRAY(COPY_OPERATION_CASES); ++j)
            {
                std::vector<DATA_TYPE> sameSizeTypes = getSameSizeBaseDataType(BASE_DATA_TYPE_CASES[i]);

                for (uint32_t k = 0; k < sameSizeTypes.size(); ++k)
                {
                    const DATA_TYPE dataType  = sameSizeTypes[k];
                    const uint32_t otherIndex = static_cast<uint32_t>(dataType);

                    std::string testName = std::string(toString(BASE_DATA_TYPE_CASES[i])) + "_to_" + toString(dataType);

                    std::map<std::string, std::string> specMap;
                    specMap["copyOp"]       = COPY_OPERATION_CASES[j].pCopyOp;
                    specMap["alignment"]    = std::to_string(getSizeInBytes(BASE_DATA_TYPE_CASES[i]));
                    specMap["baseDecl"]     = getDeclaration(BASE_DATA_TYPE_CASES[i]);
                    specMap["sameSizeDecl"] = getDeclaration(BASE_DATA_TYPE_CASES[otherIndex]);
                    specMap["baseType"]     = toString(BASE_DATA_TYPE_CASES[i]);
                    specMap["sameSizeType"] = toString(BASE_DATA_TYPE_CASES[otherIndex]);
                    specMap["threadCount"]  = std::to_string(Constants::numThreads);
                    specMap["size"]         = std::to_string(getSizeInBytes(BASE_DATA_TYPE_CASES[i]));
                    specMap["otherCap"]     = getCapability(dataType);
                    specMap["copyType"]     = toString(BASE_DATA_TYPE_CASES[i]);

                    const tcu::StringTemplate tempShaderFunctions =
                        tcu::StringTemplate(shaderFunctions.specialize(specMap));
                    std::string shaderVariablesStr = shaderVariables.specialize(specMap);
                    if ((BASE_DATA_TYPE_CASES[i] != DataTypes::UINT32) && (sameSizeTypes[k] != DataTypes::UINT32))
                    {
                        shaderVariablesStr = "%uint32 = OpTypeInt 32 0\n"
                                             "%c_uint32_1 = OpConstant %uint32 1\n" +
                                             shaderVariablesStr;
                    }

                    std::string memModelOp;
                    std::vector<const char *> spvExts;
                    std::vector<const char *> spvCaps;
                    ComputeShaderSpec spec;
                    adjustSpecForUntypedPointers(spec, spvExts, spvCaps);
                    adjustSpecForMemoryModel(memModel, spec, memModelOp, spvExts, spvCaps);
                    adjustSpecForDataTypes(BASE_DATA_TYPE_CASES[i], spec, spvExts, spvCaps);
                    if (BASE_DATA_TYPE_CASES[i] != dataType)
                        adjustSpecForDataTypes(dataType, spec, spvExts, spvCaps);
                    adjustSpecForSmallContainerType(ContainerTypes::STORAGE_BUFFER, BASE_DATA_TYPE_CASES[i], spec,
                                                    spvExts, spvCaps);

                    specMap["memModelOp"]       = memModelOp;
                    specMap["extensions"]       = toString(spvExts);
                    specMap["capabilities"]     = toString(spvCaps);
                    const std::string shaderAsm = shaderHeader.specialize(specMap) +
                                                  shaderAnnotations.specialize(specMap) + shaderVariablesStr +
                                                  tempShaderFunctions.specialize(specMap);

                    FilledBufferDesc desc;
                    desc.dataType  = BASE_DATA_TYPE_CASES[i];
                    desc.elemCount = Constants::numThreads;
                    desc.padding   = 0;
                    desc.fillType  = FillingTypes::RANDOM;
                    desc.seed      = deStringHash(testGroup->getName());

                    Resource inputOutputResource = createFilledResource(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, desc);

                    spec.assembly      = shaderAsm;
                    spec.numWorkGroups = tcu::IVec3(Constants::numThreads, 1, 1);
                    spec.inputs.push_back(inputOutputResource);
                    spec.outputs.push_back(inputOutputResource);
                    spec.extensions.push_back("VK_KHR_storage_buffer_storage_class");

                    if (COPY_OPERATION_CASES[j].type == CopyOperationTypes::COPY_MEMORY)
                    {
                        memoryGroup->addChild(new SpvAsmComputeShaderCase(testCtx, testName.c_str(), spec));
                    }
                    else if (COPY_OPERATION_CASES[j].type == CopyOperationTypes::COPY_MEMORY_SIZED)
                    {
                        memorySizedGroup->addChild(new SpvAsmComputeShaderCase(testCtx, testName.c_str(), spec));
                    }
                }
            }
        }
    }

    {
        const tcu::StringTemplate shaderHeader(createShaderHeader());

        const tcu::StringTemplate shaderAnnotations(
            createShaderAnnotations(TypePunningTestCases::COPY_TO_SCALAR_VECTOR));

        const tcu::StringTemplate shaderVariables(createShaderVariables(TypePunningTestCases::COPY_TO_SCALAR_VECTOR));

        const tcu::StringTemplate shaderFunctions(createShaderMain(TypePunningTestCases::COPY_TO_SCALAR_VECTOR));

        for (uint32_t i = 0; i < DE_LENGTH_OF_ARRAY(BASE_DATA_TYPE_CASES); ++i)
        {
            for (uint32_t j = 0; j < DE_LENGTH_OF_ARRAY(COPY_OPERATION_CASES); ++j)
            {
                std::vector<COMPOSITE_DATA_TYPE> sameSizeTypes = getSameSizeCompositeType(BASE_DATA_TYPE_CASES[i]);

                for (uint32_t k = 0; k < sameSizeTypes.size(); ++k)
                {
                    const COMPOSITE_DATA_TYPE compositeType = sameSizeTypes[k];

                    std::string testName =
                        std::string(toString(BASE_DATA_TYPE_CASES[i])) + "_to_" + toString(compositeType);

                    std::map<std::string, std::string> specMap;
                    specMap["copyOp"]        = COPY_OPERATION_CASES[j].pCopyOp;
                    specMap["baseType"]      = toString(BASE_DATA_TYPE_CASES[i]);
                    specMap["otherType"]     = toString(getCompositeBaseDataType(compositeType));
                    specMap["baseDecl"]      = getDeclaration(BASE_DATA_TYPE_CASES[i]);
                    specMap["otherTypeDecl"] = getDeclaration(getCompositeBaseDataType(compositeType));
                    specMap["otherVec"]      = toString(compositeType);
                    specMap["otherVecDecl"]  = getDeclaration(compositeType);
                    specMap["size"]          = std::to_string(getSizeInBytes(BASE_DATA_TYPE_CASES[i]));
                    specMap["otherCap"]      = getCapability(compositeType);
                    specMap["copyType"]      = toString(BASE_DATA_TYPE_CASES[i]);

                    if (sameSizeTypes[k] != CompositeDataTypes::VEC3_UINT32)
                        specMap["inputVec"] = "%vec3_uint32 = OpTypeVector %uint32 3";

                    const tcu::StringTemplate tempShaderFunctions =
                        tcu::StringTemplate(shaderFunctions.specialize(specMap));
                    std::string shaderVariablesStr = shaderVariables.specialize(specMap);
                    if ((BASE_DATA_TYPE_CASES[i] != DataTypes::UINT32) &&
                        (getCompositeBaseDataType(compositeType) != DataTypes::UINT32) &&
                        compositeType != CompositeDataTypes::VEC2_UINT32 &&
                        compositeType != CompositeDataTypes::VEC3_UINT32 &&
                        compositeType != CompositeDataTypes::VEC4_UINT32)
                    {
                        shaderVariablesStr = "%uint32      = OpTypeInt 32 0\n"
                                             "%c_uint32_1  = OpConstant %uint32 1\n" +
                                             shaderVariablesStr;
                    }

                    std::string memModelOp;
                    std::vector<const char *> spvExts;
                    std::vector<const char *> spvCaps;
                    ComputeShaderSpec spec;
                    adjustSpecForUntypedPointers(spec, spvExts, spvCaps);
                    adjustSpecForMemoryModel(memModel, spec, memModelOp, spvExts, spvCaps);
                    adjustSpecForDataTypes(BASE_DATA_TYPE_CASES[i], spec, spvExts, spvCaps);
                    if (BASE_DATA_TYPE_CASES[i] != getCompositeBaseDataType(compositeType))
                        adjustSpecForDataTypes(getCompositeBaseDataType(compositeType), spec, spvExts, spvCaps);
                    adjustSpecForSmallContainerType(ContainerTypes::STORAGE_BUFFER, BASE_DATA_TYPE_CASES[i], spec,
                                                    spvExts, spvCaps);

                    specMap["memModelOp"]       = memModelOp;
                    specMap["extensions"]       = toString(spvExts);
                    specMap["capabilities"]     = toString(spvCaps);
                    const std::string shaderAsm = shaderHeader.specialize(specMap) +
                                                  shaderAnnotations.specialize(specMap) + shaderVariablesStr +
                                                  tempShaderFunctions.specialize(specMap);

                    FilledBufferDesc desc;
                    desc.dataType  = BASE_DATA_TYPE_CASES[i];
                    desc.elemCount = 1;
                    desc.padding   = 0;
                    desc.fillType  = FillingTypes::VALUE;
                    desc.value     = 1;

                    Resource inputOutputResource = createFilledResource(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, desc);

                    spec.assembly      = shaderAsm;
                    spec.numWorkGroups = tcu::IVec3(1, 1, 1);
                    spec.inputs.push_back(inputOutputResource);
                    spec.outputs.push_back(inputOutputResource);
                    spec.extensions.push_back("VK_KHR_storage_buffer_storage_class");

                    if (COPY_OPERATION_CASES[j].type == CopyOperationTypes::COPY_MEMORY)
                    {
                        memoryGroup->addChild(new SpvAsmComputeShaderCase(testCtx, testName.c_str(), spec));
                    }
                    else if (COPY_OPERATION_CASES[j].type == CopyOperationTypes::COPY_MEMORY_SIZED)
                    {
                        memorySizedGroup->addChild(new SpvAsmComputeShaderCase(testCtx, testName.c_str(), spec));
                    }
                }
            }
        }
    }

    {
        const tcu::StringTemplate shaderHeader(createShaderHeader());

        const tcu::StringTemplate shaderAnnotations(
            createShaderAnnotations(TypePunningTestCases::COPY_TO_VECTOR_SCALAR));

        const tcu::StringTemplate shaderVariables(createShaderVariables(TypePunningTestCases::COPY_TO_VECTOR_SCALAR));

        const tcu::StringTemplate shaderFunctions(createShaderMain(TypePunningTestCases::COPY_TO_VECTOR_SCALAR));

        for (uint32_t i = 0; i < DE_LENGTH_OF_ARRAY(COMPOSITE_DATA_TYPE_CASES); ++i)
        {
            for (uint32_t j = 0; j < DE_LENGTH_OF_ARRAY(COPY_OPERATION_CASES); ++j)
            {
                std::vector<DATA_TYPE> sameSizeTypes = getSameSizeBaseDataType(COMPOSITE_DATA_TYPE_CASES[i]);

                for (uint32_t k = 0; k < sameSizeTypes.size(); ++k)
                {
                    const DATA_TYPE dataType  = sameSizeTypes[k];
                    const uint32_t otherIndex = static_cast<uint32_t>(dataType);

                    std::string testName =
                        std::string(toString(COMPOSITE_DATA_TYPE_CASES[i])) + "_to_" + toString(dataType);

                    std::map<std::string, std::string> specMap;
                    specMap["copyOp"]        = COPY_OPERATION_CASES[j].pCopyOp;
                    specMap["alignment"]     = std::to_string(getSizeInBytes(BASE_DATA_TYPE_CASES[i]));
                    specMap["baseType"]      = toString(getCompositeBaseDataType(COMPOSITE_DATA_TYPE_CASES[i]));
                    specMap["otherType"]     = toString(dataType);
                    specMap["baseDecl"]      = getDeclaration(getCompositeBaseDataType(COMPOSITE_DATA_TYPE_CASES[i]));
                    specMap["otherTypeDecl"] = getDeclaration(BASE_DATA_TYPE_CASES[otherIndex]);
                    specMap["baseVec"]       = toString(COMPOSITE_DATA_TYPE_CASES[i]);
                    specMap["baseVecDecl"]   = getDeclaration(COMPOSITE_DATA_TYPE_CASES[i]);
                    specMap["size"]          = std::to_string(getSizeInBytes(COMPOSITE_DATA_TYPE_CASES[i]));
                    specMap["otherCap"]      = getCapability(sameSizeTypes[k]);
                    specMap["copyType"]      = toString(COMPOSITE_DATA_TYPE_CASES[i]);

                    if (COMPOSITE_DATA_TYPE_CASES[i] != CompositeDataTypes::VEC3_UINT32)
                        specMap["inputVec"] = "%vec3_uint32 = OpTypeVector %uint32 3";

                    const tcu::StringTemplate tempShaderFunctions =
                        tcu::StringTemplate(shaderFunctions.specialize(specMap));
                    std::string shaderVariablesStr = shaderVariables.specialize(specMap);
                    if ((getCompositeBaseDataType(COMPOSITE_DATA_TYPE_CASES[i]) != DataTypes::UINT32) &&
                        (dataType != DataTypes::UINT32))
                    {
                        shaderVariablesStr = "%uint32      = OpTypeInt 32 0\n"
                                             "%c_uint32_1  = OpConstant %uint32 1\n" +
                                             shaderVariablesStr;
                    }

                    std::string memModelOp;
                    std::vector<const char *> spvExts;
                    std::vector<const char *> spvCaps;
                    ComputeShaderSpec spec;
                    adjustSpecForUntypedPointers(spec, spvExts, spvCaps);
                    adjustSpecForMemoryModel(memModel, spec, memModelOp, spvExts, spvCaps);
                    adjustSpecForDataTypes(getCompositeBaseDataType(COMPOSITE_DATA_TYPE_CASES[i]), spec, spvExts,
                                           spvCaps);
                    if (getCompositeBaseDataType(COMPOSITE_DATA_TYPE_CASES[i]) != dataType)
                        adjustSpecForDataTypes(dataType, spec, spvExts, spvCaps);
                    adjustSpecForSmallContainerType(ContainerTypes::STORAGE_BUFFER,
                                                    getCompositeBaseDataType(COMPOSITE_DATA_TYPE_CASES[i]), spec,
                                                    spvExts, spvCaps);

                    specMap["memModelOp"]       = memModelOp;
                    specMap["extensions"]       = toString(spvExts);
                    specMap["capabilities"]     = toString(spvCaps);
                    const std::string shaderAsm = shaderHeader.specialize(specMap) +
                                                  shaderAnnotations.specialize(specMap) + shaderVariablesStr +
                                                  tempShaderFunctions.specialize(specMap);

                    FilledBufferDesc desc;
                    desc.dataType  = getCompositeBaseDataType(COMPOSITE_DATA_TYPE_CASES[i]);
                    desc.elemCount = getElementCount(COMPOSITE_DATA_TYPE_CASES[i]);
                    desc.fillType  = FillingTypes::VALUE;
                    desc.value     = 1;
                    desc.padding   = 0;

                    Resource inputOutputResource = createFilledResource(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, desc);

                    spec.assembly      = shaderAsm;
                    spec.numWorkGroups = tcu::IVec3(1, 1, 1);
                    spec.inputs.push_back(inputOutputResource);
                    spec.outputs.push_back(inputOutputResource);
                    spec.extensions.push_back("VK_KHR_storage_buffer_storage_class");

                    if (COPY_OPERATION_CASES[j].type == CopyOperationTypes::COPY_MEMORY)
                    {
                        memoryGroup->addChild(new SpvAsmComputeShaderCase(testCtx, testName.c_str(), spec));
                    }
                    else if (COPY_OPERATION_CASES[j].type == CopyOperationTypes::COPY_MEMORY_SIZED)
                    {
                        memorySizedGroup->addChild(new SpvAsmComputeShaderCase(testCtx, testName.c_str(), spec));
                    }
                }
            }
        }
    }

    testGroup->addChild(memoryGroup.release());
    testGroup->addChild(memorySizedGroup.release());
}

void addAtomicAddTests(tcu::TestCaseGroup *testGroup, MEMORY_MODEL_TYPE memModel)
{
    tcu::TestContext &testCtx = testGroup->getTestContext();

    tcu::StringTemplate shaderHeader(createShaderHeader("%output_data_untyped_var"));

    tcu::StringTemplate shaderAnnotations(createShaderAnnotations(AtomicTestCases::OP_ATOMIC_ADD));

    tcu::StringTemplate shaderVariables(createShaderVariables(AtomicTestCases::OP_ATOMIC_ADD));

    tcu::StringTemplate shaderFunctions(createShaderMain(AtomicTestCases::OP_ATOMIC_ADD));

    for (uint32_t i = 0; i < DE_LENGTH_OF_ARRAY(ATOMIC_DATA_TYPE_CASES); ++i)
    {
        std::string testName = toString(ATOMIC_DATA_TYPE_CASES[i]);

        std::map<std::string, std::string> specMap;
        specMap["baseDecl"] = getDeclaration(ATOMIC_DATA_TYPE_CASES[i]);
        specMap["baseType"] = toString(ATOMIC_DATA_TYPE_CASES[i]);
        specMap["opType"]   = getAtomicAddOperator(ATOMIC_DATA_TYPE_CASES[i]);
        specMap["opValue"]  = std::to_string(16);

        const tcu::StringTemplate tempShaderFunctions = tcu::StringTemplate(shaderFunctions.specialize(specMap));

        std::string shaderVariablesStr = shaderVariables.specialize(specMap);
        if (ATOMIC_DATA_TYPE_CASES[i] != DataTypes::UINT32)
        {
            shaderVariablesStr = "%uint32     = OpTypeInt  32      0\n"
                                 "%c_uint32_1 = OpConstant %uint32 1\n" +
                                 shaderVariablesStr;
        }

        std::string memModelOp;
        std::vector<const char *> spvExts;
        std::vector<const char *> spvCaps;
        ComputeShaderSpec spec;
        adjustSpecForUntypedPointers(spec, spvExts, spvCaps);
        adjustSpecForMemoryModel(memModel, spec, memModelOp, spvExts, spvCaps);
        adjustSpecForDataTypes(ATOMIC_DATA_TYPE_CASES[i], spec, spvExts, spvCaps);
        adjustSpecForAtomicOperations(ATOMIC_DATA_TYPE_CASES[i], spec, spvExts, spvCaps);
        adjustSpecForAtomicAddOperations(ATOMIC_DATA_TYPE_CASES[i], spec, spvExts, spvCaps);

        specMap["memModelOp"]       = memModelOp;
        specMap["extensions"]       = toString(spvExts);
        specMap["capabilities"]     = toString(spvCaps);
        const std::string shaderAsm = shaderHeader.specialize(specMap) + shaderAnnotations.specialize(specMap) +
                                      shaderVariablesStr + tempShaderFunctions.specialize(specMap);

        AtomicResourceDesc desc;
        desc.dataType  = ATOMIC_DATA_TYPE_CASES[i];
        desc.elemCount = 1;

        AtomicOpDesc atomicDesc;
        atomicDesc.type      = OP_ATOMIC_ADD;
        atomicDesc.userData0 = 16;
        atomicDesc.elemIndex = 0;

        Resource output = createAtomicResource(desc, std::vector<AtomicOpDesc>({atomicDesc}));

        spec.assembly      = shaderAsm;
        spec.numWorkGroups = tcu::IVec3(1, 1, 1);
        spec.spirvVersion  = SPIRV_VERSION_1_4;
        spec.outputs.push_back(output);
        spec.extensions.push_back("VK_KHR_storage_buffer_storage_class");

        testGroup->addChild(new SpvAsmComputeShaderCase(testCtx, testName.c_str(), spec));
    }
}

void addAtomicSubtractTests(tcu::TestCaseGroup *testGroup, MEMORY_MODEL_TYPE memModel)
{
    tcu::TestContext &testCtx = testGroup->getTestContext();

    tcu::StringTemplate shaderHeader(createShaderHeader("%output_data_untyped_var"));

    tcu::StringTemplate shaderAnnotations(createShaderAnnotations(AtomicTestCases::OP_ATOMIC_SUB));

    tcu::StringTemplate shaderVariables(createShaderVariables(AtomicTestCases::OP_ATOMIC_SUB));

    tcu::StringTemplate shaderFunctions(createShaderMain(AtomicTestCases::OP_ATOMIC_SUB));

    for (uint32_t i = 0; i < DE_LENGTH_OF_ARRAY(ATOMIC_INT_DATA_TYPE_CASES); ++i)
    {
        std::string testName = toString(ATOMIC_INT_DATA_TYPE_CASES[i]);

        std::map<std::string, std::string> specMap;
        specMap["baseDecl"] = getDeclaration(ATOMIC_INT_DATA_TYPE_CASES[i]);
        specMap["baseType"] = toString(ATOMIC_INT_DATA_TYPE_CASES[i]);
        specMap["opType"]   = getAtomicSubtractOperator(ATOMIC_INT_DATA_TYPE_CASES[i]);
        specMap["opValue"]  = std::to_string(16);

        const tcu::StringTemplate tempShaderFunctions = tcu::StringTemplate(shaderFunctions.specialize(specMap));

        std::string shaderVariablesStr = shaderVariables.specialize(specMap);
        if (ATOMIC_INT_DATA_TYPE_CASES[i] != DataTypes::UINT32)
        {
            shaderVariablesStr = "%uint32     = OpTypeInt  32      0\n"
                                 "%c_uint32_1 = OpConstant %uint32 1\n" +
                                 shaderVariablesStr;
        }

        std::string memModelOp;
        std::vector<const char *> spvExts;
        std::vector<const char *> spvCaps;
        ComputeShaderSpec spec;
        adjustSpecForUntypedPointers(spec, spvExts, spvCaps);
        adjustSpecForMemoryModel(memModel, spec, memModelOp, spvExts, spvCaps);
        adjustSpecForDataTypes(ATOMIC_INT_DATA_TYPE_CASES[i], spec, spvExts, spvCaps);
        adjustSpecForAtomicOperations(ATOMIC_INT_DATA_TYPE_CASES[i], spec, spvExts, spvCaps);

        specMap["memModelOp"]       = memModelOp;
        specMap["extensions"]       = toString(spvExts);
        specMap["capabilities"]     = toString(spvCaps);
        const std::string shaderAsm = shaderHeader.specialize(specMap) + shaderAnnotations.specialize(specMap) +
                                      shaderVariablesStr + tempShaderFunctions.specialize(specMap);

        AtomicResourceDesc desc;
        desc.dataType  = ATOMIC_INT_DATA_TYPE_CASES[i];
        desc.elemCount = 1;

        AtomicOpDesc atomicDesc;
        atomicDesc.type      = OP_ATOMIC_SUBTRACT;
        atomicDesc.userData0 = 16;
        atomicDesc.elemIndex = 0;

        Resource output = createAtomicResource(desc, std::vector<AtomicOpDesc>({atomicDesc}));

        spec.assembly      = shaderAsm;
        spec.numWorkGroups = tcu::IVec3(1, 1, 1);
        spec.spirvVersion  = SPIRV_VERSION_1_4;
        spec.outputs.push_back(output);
        spec.extensions.push_back("VK_KHR_storage_buffer_storage_class");

        testGroup->addChild(new SpvAsmComputeShaderCase(testCtx, testName.c_str(), spec));
    }
}

void addAtomicIncrementDecrementTests(tcu::TestCaseGroup *testGroup, MEMORY_MODEL_TYPE memModel,
                                      AtomicTestCases testCase)
{
    DE_ASSERT((testCase == AtomicTestCases::OP_ATOMIC_INCREMENT) || (testCase == AtomicTestCases::OP_ATOMIC_DECREMENT));

    const AtomicOpType opType =
        testCase == AtomicTestCases::OP_ATOMIC_INCREMENT ? OP_ATOMIC_INCREMENT : OP_ATOMIC_DECREMENT;

    tcu::TestContext &testCtx = testGroup->getTestContext();

    tcu::StringTemplate shaderHeader(createShaderHeader("%output_data_untyped_var"));

    tcu::StringTemplate shaderAnnotations(createShaderAnnotations(testCase));

    tcu::StringTemplate shaderVariables(createShaderVariables(testCase));

    tcu::StringTemplate shaderFunctions(createShaderMain(testCase));

    for (uint32_t i = 0; i < DE_LENGTH_OF_ARRAY(ATOMIC_INT_DATA_TYPE_CASES); ++i)
    {
        const std::string testName = toString(ATOMIC_INT_DATA_TYPE_CASES[i]);
        const std::string opStr    = testCase == AtomicTestCases::OP_ATOMIC_INCREMENT ?
                                         getAtomicIncrementOperator(ATOMIC_INT_DATA_TYPE_CASES[i]) :
                                         getAtomicDecrementOperator(ATOMIC_INT_DATA_TYPE_CASES[i]);

        std::map<std::string, std::string> specMap;
        specMap["baseDecl"] = getDeclaration(ATOMIC_INT_DATA_TYPE_CASES[i]);
        specMap["baseType"] = toString(ATOMIC_INT_DATA_TYPE_CASES[i]);
        specMap["opType"]   = opStr;

        const tcu::StringTemplate tempShaderFunctions = tcu::StringTemplate(shaderFunctions.specialize(specMap));

        std::string shaderVariablesStr = shaderVariables.specialize(specMap);
        if (ATOMIC_INT_DATA_TYPE_CASES[i] != DataTypes::UINT32)
        {
            shaderVariablesStr = "%uint32     = OpTypeInt  32      0\n" + shaderVariablesStr;
        }

        std::string memModelOp;
        std::vector<const char *> spvExts;
        std::vector<const char *> spvCaps;
        ComputeShaderSpec spec;
        adjustSpecForUntypedPointers(spec, spvExts, spvCaps);
        adjustSpecForMemoryModel(memModel, spec, memModelOp, spvExts, spvCaps);
        adjustSpecForDataTypes(ATOMIC_INT_DATA_TYPE_CASES[i], spec, spvExts, spvCaps);
        adjustSpecForAtomicOperations(ATOMIC_INT_DATA_TYPE_CASES[i], spec, spvExts, spvCaps);

        specMap["memModelOp"]       = memModelOp;
        specMap["extensions"]       = toString(spvExts);
        specMap["capabilities"]     = toString(spvCaps);
        const std::string shaderAsm = shaderHeader.specialize(specMap) + shaderAnnotations.specialize(specMap) +
                                      shaderVariablesStr + tempShaderFunctions.specialize(specMap);

        AtomicResourceDesc desc;
        desc.dataType  = ATOMIC_INT_DATA_TYPE_CASES[i];
        desc.elemCount = 1;

        AtomicOpDesc atomicDesc;
        atomicDesc.type      = opType;
        atomicDesc.elemIndex = 0;

        Resource output = createAtomicResource(desc, std::vector<AtomicOpDesc>({atomicDesc}));

        spec.assembly      = shaderAsm;
        spec.numWorkGroups = tcu::IVec3(1, 1, 1);
        spec.spirvVersion  = SPIRV_VERSION_1_4;
        spec.outputs.push_back(output);
        spec.extensions.push_back("VK_KHR_storage_buffer_storage_class");

        testGroup->addChild(new SpvAsmComputeShaderCase(testCtx, testName.c_str(), spec));
    }
}

void addAtomicMinMaxTests(tcu::TestCaseGroup *testGroup, MEMORY_MODEL_TYPE memModel, AtomicTestCases testCase)
{
    DE_ASSERT((testCase == AtomicTestCases::OP_ATOMIC_MIN) || (testCase == AtomicTestCases::OP_ATOMIC_MAX));

    const AtomicOpType opType = testCase == AtomicTestCases::OP_ATOMIC_MIN ? OP_ATOMIC_MIN : OP_ATOMIC_MAX;

    tcu::TestContext &testCtx = testGroup->getTestContext();

    tcu::StringTemplate shaderHeader(createShaderHeader("%output_data_untyped_var"));

    tcu::StringTemplate shaderAnnotations(createShaderAnnotations(testCase));

    tcu::StringTemplate shaderVariables(createShaderVariables(testCase));

    tcu::StringTemplate shaderFunctions(createShaderMain(testCase));

    for (uint32_t i = 0; i < DE_LENGTH_OF_ARRAY(ATOMIC_DATA_TYPE_CASES); ++i)
    {
        const std::string testName = toString(ATOMIC_DATA_TYPE_CASES[i]);
        const std::string opStr    = testCase == AtomicTestCases::OP_ATOMIC_MIN ?
                                         getAtomicMinOperator(ATOMIC_DATA_TYPE_CASES[i]) :
                                         getAtomicMaxOperator(ATOMIC_DATA_TYPE_CASES[i]);

        std::map<std::string, std::string> specMap;
        specMap["baseDecl"] = getDeclaration(ATOMIC_DATA_TYPE_CASES[i]);
        specMap["baseType"] = toString(ATOMIC_DATA_TYPE_CASES[i]);
        specMap["opType"]   = opStr;
        specMap["opValue"]  = std::to_string(getSignedUnsignedMinMaxTestValue(ATOMIC_DATA_TYPE_CASES[i]));

        const tcu::StringTemplate tempShaderFunctions = tcu::StringTemplate(shaderFunctions.specialize(specMap));

        std::string shaderVariablesStr = shaderVariables.specialize(specMap);
        if (ATOMIC_DATA_TYPE_CASES[i] != DataTypes::UINT32)
        {
            shaderVariablesStr = "%uint32     = OpTypeInt  32      0\n"
                                 "%c_uint32_1 = OpConstant %uint32 1\n" +
                                 shaderVariablesStr;
        }

        std::string memModelOp;
        std::vector<const char *> spvExts;
        std::vector<const char *> spvCaps;
        ComputeShaderSpec spec;
        adjustSpecForUntypedPointers(spec, spvExts, spvCaps);
        adjustSpecForMemoryModel(memModel, spec, memModelOp, spvExts, spvCaps);
        adjustSpecForDataTypes(ATOMIC_DATA_TYPE_CASES[i], spec, spvExts, spvCaps);
        adjustSpecForAtomicOperations(ATOMIC_DATA_TYPE_CASES[i], spec, spvExts, spvCaps);
        adjustSpecForAtomicMinMaxOperations(ATOMIC_DATA_TYPE_CASES[i], spec, spvExts, spvCaps);

        specMap["memModelOp"]       = memModelOp;
        specMap["extensions"]       = toString(spvExts);
        specMap["capabilities"]     = toString(spvCaps);
        const std::string shaderAsm = shaderHeader.specialize(specMap) + shaderAnnotations.specialize(specMap) +
                                      shaderVariablesStr + tempShaderFunctions.specialize(specMap);

        AtomicResourceDesc desc;
        desc.dataType  = ATOMIC_DATA_TYPE_CASES[i];
        desc.elemCount = 1;

        AtomicOpDesc atomicDesc;
        atomicDesc.type      = opType;
        atomicDesc.elemIndex = 0;
        atomicDesc.userData0 = getSignedUnsignedMinMaxTestValue(ATOMIC_DATA_TYPE_CASES[i]);

        Resource output = createAtomicResource(desc, std::vector<AtomicOpDesc>({atomicDesc}));

        spec.assembly      = shaderAsm;
        spec.numWorkGroups = tcu::IVec3(1, 1, 1);
        spec.spirvVersion  = SPIRV_VERSION_1_4;
        spec.outputs.push_back(output);
        spec.extensions.push_back("VK_KHR_storage_buffer_storage_class");

        testGroup->addChild(new SpvAsmComputeShaderCase(testCtx, testName.c_str(), spec));
    }
}

void addAtomicBooleanTests(tcu::TestCaseGroup *testGroup, MEMORY_MODEL_TYPE memModel, AtomicTestCases testCase)
{
    DE_ASSERT((testCase == AtomicTestCases::OP_ATOMIC_AND) || (testCase == AtomicTestCases::OP_ATOMIC_OR) ||
              (testCase == AtomicTestCases::OP_ATOMIC_XOR));

    AtomicOpType opType{};

    const char *(*pAtomicOpFn)(DATA_TYPE) = nullptr;

    switch (testCase)
    {
    case vkt::SpirVAssembly::AtomicTestCases::OP_ATOMIC_AND:
    {
        opType      = OP_ATOMIC_AND;
        pAtomicOpFn = getAtomicAndOperator;

        break;
    }
    case vkt::SpirVAssembly::AtomicTestCases::OP_ATOMIC_OR:
    {
        opType      = OP_ATOMIC_OR;
        pAtomicOpFn = getAtomicOrOperator;

        break;
    }
    case vkt::SpirVAssembly::AtomicTestCases::OP_ATOMIC_XOR:
    {
        opType      = OP_ATOMIC_XOR;
        pAtomicOpFn = getAtomicXorOperator;

        break;
    }
    default:
    {
        DE_ASSERT(0);
        break;
    }
    }

    tcu::TestContext &testCtx = testGroup->getTestContext();

    tcu::StringTemplate shaderHeader(createShaderHeader("%output_data_untyped_var"));

    tcu::StringTemplate shaderAnnotations(createShaderAnnotations(testCase));

    tcu::StringTemplate shaderVariables(createShaderVariables(testCase));

    tcu::StringTemplate shaderFunctions(createShaderMain(testCase));

    for (uint32_t i = 0; i < DE_LENGTH_OF_ARRAY(ATOMIC_INT_DATA_TYPE_CASES); ++i)
    {
        const std::string testName = toString(ATOMIC_INT_DATA_TYPE_CASES[i]);

        DE_ASSERT(pAtomicOpFn != nullptr);

        std::map<std::string, std::string> specMap;
        specMap["baseDecl"] = getDeclaration(ATOMIC_INT_DATA_TYPE_CASES[i]);
        specMap["baseType"] = toString(ATOMIC_INT_DATA_TYPE_CASES[i]);
        specMap["opType"]   = (*pAtomicOpFn)(ATOMIC_INT_DATA_TYPE_CASES[i]);
        specMap["opValue"]  = std::to_string(1);

        const tcu::StringTemplate tempShaderFunctions = tcu::StringTemplate(shaderFunctions.specialize(specMap));

        std::string shaderVariablesStr = shaderVariables.specialize(specMap);
        if (ATOMIC_INT_DATA_TYPE_CASES[i] != DataTypes::UINT32)
        {
            shaderVariablesStr = "%uint32     = OpTypeInt  32      0\n"
                                 "%c_uint32_1 = OpConstant %uint32 1\n" +
                                 shaderVariablesStr;
        }

        std::string memModelOp;
        std::vector<const char *> spvExts;
        std::vector<const char *> spvCaps;
        ComputeShaderSpec spec;
        adjustSpecForUntypedPointers(spec, spvExts, spvCaps);
        adjustSpecForMemoryModel(memModel, spec, memModelOp, spvExts, spvCaps);
        adjustSpecForDataTypes(ATOMIC_INT_DATA_TYPE_CASES[i], spec, spvExts, spvCaps);
        adjustSpecForAtomicOperations(ATOMIC_INT_DATA_TYPE_CASES[i], spec, spvExts, spvCaps);

        specMap["memModelOp"]       = memModelOp;
        specMap["extensions"]       = toString(spvExts);
        specMap["capabilities"]     = toString(spvCaps);
        const std::string shaderAsm = shaderHeader.specialize(specMap) + shaderAnnotations.specialize(specMap) +
                                      shaderVariablesStr + tempShaderFunctions.specialize(specMap);

        AtomicResourceDesc desc;
        desc.dataType  = ATOMIC_INT_DATA_TYPE_CASES[i];
        desc.elemCount = 1;

        AtomicOpDesc atomicDesc;
        atomicDesc.type      = opType;
        atomicDesc.elemIndex = 0;
        atomicDesc.userData0 = 1;

        Resource output = createAtomicResource(desc, std::vector<AtomicOpDesc>({atomicDesc}));

        spec.assembly      = shaderAsm;
        spec.numWorkGroups = tcu::IVec3(1, 1, 1);
        spec.spirvVersion  = SPIRV_VERSION_1_4;
        spec.outputs.push_back(output);
        spec.extensions.push_back("VK_KHR_storage_buffer_storage_class");

        testGroup->addChild(new SpvAsmComputeShaderCase(testCtx, testName.c_str(), spec));
    }
}

void addAtomicExchangeTests(tcu::TestCaseGroup *testGroup, MEMORY_MODEL_TYPE memModel)
{
    tcu::TestContext &testCtx = testGroup->getTestContext();

    tcu::StringTemplate shaderHeader(createShaderHeader("%output_data_untyped_var"));

    tcu::StringTemplate shaderAnnotations(createShaderAnnotations(AtomicTestCases::OP_ATOMIC_EXCHANGE));

    tcu::StringTemplate shaderVariables(createShaderVariables(AtomicTestCases::OP_ATOMIC_EXCHANGE));

    tcu::StringTemplate shaderFunctions(createShaderMain(AtomicTestCases::OP_ATOMIC_EXCHANGE));

    for (uint32_t i = 0; i < DE_LENGTH_OF_ARRAY(ATOMIC_DATA_TYPE_CASES); ++i)
    {
        const std::string testName = toString(ATOMIC_DATA_TYPE_CASES[i]);

        std::map<std::string, std::string> specMap;
        specMap["baseDecl"] = getDeclaration(ATOMIC_DATA_TYPE_CASES[i]);
        specMap["baseType"] = toString(ATOMIC_DATA_TYPE_CASES[i]);
        specMap["opType"]   = getAtomicExchangeOperator(ATOMIC_DATA_TYPE_CASES[i]);
        specMap["opValue"]  = std::to_string(1);

        const tcu::StringTemplate tempShaderFunctions = tcu::StringTemplate(shaderFunctions.specialize(specMap));

        std::string shaderVariablesStr = shaderVariables.specialize(specMap);
        if (ATOMIC_DATA_TYPE_CASES[i] != DataTypes::UINT32)
        {
            shaderVariablesStr = "%uint32     = OpTypeInt  32      0\n"
                                 "%c_uint32_1 = OpConstant %uint32 1\n" +
                                 shaderVariablesStr;
        }

        std::string memModelOp;
        std::vector<const char *> spvExts;
        std::vector<const char *> spvCaps;
        ComputeShaderSpec spec;
        adjustSpecForUntypedPointers(spec, spvExts, spvCaps);
        adjustSpecForMemoryModel(memModel, spec, memModelOp, spvExts, spvCaps);
        adjustSpecForDataTypes(ATOMIC_DATA_TYPE_CASES[i], spec, spvExts, spvCaps);
        adjustSpecForAtomicOperations(ATOMIC_DATA_TYPE_CASES[i], spec, spvExts, spvCaps);

        specMap["memModelOp"]       = memModelOp;
        specMap["extensions"]       = toString(spvExts);
        specMap["capabilities"]     = toString(spvCaps);
        const std::string shaderAsm = shaderHeader.specialize(specMap) + shaderAnnotations.specialize(specMap) +
                                      shaderVariablesStr + tempShaderFunctions.specialize(specMap);

        AtomicResourceDesc desc;
        desc.dataType  = ATOMIC_DATA_TYPE_CASES[i];
        desc.elemCount = 1;

        AtomicOpDesc atomicDesc;
        atomicDesc.type      = OP_ATOMIC_EXCHANGE;
        atomicDesc.elemIndex = 0;
        atomicDesc.userData0 = 1;

        Resource output = createAtomicResource(desc, std::vector<AtomicOpDesc>({atomicDesc}));

        spec.assembly      = shaderAsm;
        spec.numWorkGroups = tcu::IVec3(1, 1, 1);
        spec.spirvVersion  = SPIRV_VERSION_1_4;
        spec.outputs.push_back(output);
        spec.extensions.push_back("VK_KHR_storage_buffer_storage_class");

        testGroup->addChild(new SpvAsmComputeShaderCase(testCtx, testName.c_str(), spec));
    }
}

void addAtomicCompareExchangeTests(tcu::TestCaseGroup *testGroup, MEMORY_MODEL_TYPE memModel)
{
    tcu::TestContext &testCtx = testGroup->getTestContext();

    de::MovePtr<tcu::TestCaseGroup> exchangedGroup(new tcu::TestCaseGroup(testCtx, "exchanged", ""));
    de::MovePtr<tcu::TestCaseGroup> notExchangedGroup(new tcu::TestCaseGroup(testCtx, "not_exchanged", ""));

    tcu::StringTemplate shaderHeader(createShaderHeader("%output_data_untyped_var"));

    tcu::StringTemplate shaderAnnotations(createShaderAnnotations(AtomicTestCases::OP_ATOMIC_COMPARE_EXCHANGE));

    tcu::StringTemplate shaderVariables(createShaderVariables(AtomicTestCases::OP_ATOMIC_COMPARE_EXCHANGE));

    tcu::StringTemplate shaderFunctions(createShaderMain(AtomicTestCases::OP_ATOMIC_COMPARE_EXCHANGE));

    for (uint32_t i = 0; i < DE_LENGTH_OF_ARRAY(ATOMIC_INT_DATA_TYPE_CASES); ++i)
    {
        for (uint32_t j = 0; j < 2; ++j)
        {
            const std::string testName = toString(ATOMIC_INT_DATA_TYPE_CASES[i]);

            std::map<std::string, std::string> specMap;
            specMap["baseDecl"]  = getDeclaration(ATOMIC_INT_DATA_TYPE_CASES[i]);
            specMap["baseType"]  = toString(ATOMIC_INT_DATA_TYPE_CASES[i]);
            specMap["opType"]    = getAtomicCompareExchangeOperator(ATOMIC_INT_DATA_TYPE_CASES[i]);
            specMap["compValue"] = std::to_string(j);
            specMap["opValue"]   = std::to_string(16);

            const tcu::StringTemplate tempShaderFunctions = tcu::StringTemplate(shaderFunctions.specialize(specMap));

            std::string shaderVariablesStr = shaderVariables.specialize(specMap);
            if (ATOMIC_INT_DATA_TYPE_CASES[i] != DataTypes::UINT32)
            {
                tcu::StringTemplate compTmp("%c_${baseType}_1 = OpConstant %${baseType} 1\n");
                std::string compStr = compTmp.specialize(specMap);

                shaderVariablesStr = "%uint32 = OpTypeInt 32 0\n" + shaderVariablesStr + compStr;
            }

            std::string memModelOp;
            std::vector<const char *> spvExts;
            std::vector<const char *> spvCaps;
            ComputeShaderSpec spec;
            adjustSpecForUntypedPointers(spec, spvExts, spvCaps);
            adjustSpecForMemoryModel(memModel, spec, memModelOp, spvExts, spvCaps);
            adjustSpecForDataTypes(ATOMIC_INT_DATA_TYPE_CASES[i], spec, spvExts, spvCaps);
            adjustSpecForAtomicOperations(ATOMIC_INT_DATA_TYPE_CASES[i], spec, spvExts, spvCaps);

            specMap["memModelOp"]       = memModelOp;
            specMap["extensions"]       = toString(spvExts);
            specMap["capabilities"]     = toString(spvCaps);
            const std::string shaderAsm = shaderHeader.specialize(specMap) + shaderAnnotations.specialize(specMap) +
                                          shaderVariablesStr + tempShaderFunctions.specialize(specMap);

            AtomicResourceDesc desc;
            desc.dataType  = ATOMIC_INT_DATA_TYPE_CASES[i];
            desc.elemCount = 1;

            AtomicOpDesc storeDesc;
            storeDesc.type      = OP_ATOMIC_STORE;
            storeDesc.elemIndex = 0;
            storeDesc.userData0 = 1;

            AtomicOpDesc compExDesc;
            compExDesc.type      = OP_ATOMIC_COMPARE_EXCHANGE;
            compExDesc.elemIndex = 0;
            compExDesc.userData0 = 16;
            compExDesc.userData1 = j;

            Resource output = createAtomicResource(desc, std::vector<AtomicOpDesc>({storeDesc, compExDesc}));

            spec.assembly      = shaderAsm;
            spec.numWorkGroups = tcu::IVec3(1, 1, 1);
            spec.spirvVersion  = SPIRV_VERSION_1_4;
            spec.outputs.push_back(output);
            spec.extensions.push_back("VK_KHR_storage_buffer_storage_class");

            if (j) // for 1 adding to exchange group
            {
                exchangedGroup->addChild(new SpvAsmComputeShaderCase(testCtx, testName.c_str(), spec));
            }
            else // for 0 adding to not exchange group
            {
                notExchangedGroup->addChild(new SpvAsmComputeShaderCase(testCtx, testName.c_str(), spec));
            }
        }
    }

    testGroup->addChild(exchangedGroup.release());
    testGroup->addChild(notExchangedGroup.release());
}

void addVariablePtrOpSelectTests(tcu::TestCaseGroup *testGroup, MEMORY_MODEL_TYPE memModel)
{
    tcu::TestContext &testCtx = testGroup->getTestContext();

    de::MovePtr<tcu::TestCaseGroup> firstGroup(new tcu::TestCaseGroup(testCtx, "first", ""));
    de::MovePtr<tcu::TestCaseGroup> secondGroup(new tcu::TestCaseGroup(testCtx, "second", ""));

    tcu::StringTemplate shaderHeader(
        createShaderHeader("%push_constant_var %input_data_0_untyped_var %input_data_1_untyped_var %output_data_var"));

    tcu::StringTemplate shaderAnnotations(createShaderAnnotations(PointerTestCases::OP_SELECT_VARIABLE_PTR));

    tcu::StringTemplate shaderVariables(createShaderVariables(PointerTestCases::OP_SELECT_VARIABLE_PTR));

    tcu::StringTemplate shaderFunctions(createShaderMain(PointerTestCases::OP_SELECT_VARIABLE_PTR));

    for (uint32_t i = 0; i < DE_LENGTH_OF_ARRAY(BASE_DATA_TYPE_CASES); ++i)
    {
        for (uint32_t j = 0; j < 2; ++j)
        {
            std::string testName = toString(BASE_DATA_TYPE_CASES[i]);

            std::map<std::string, std::string> specMap;
            specMap["baseDecl"] = getDeclaration(BASE_DATA_TYPE_CASES[i]);
            specMap["baseType"] = toString(BASE_DATA_TYPE_CASES[i]);

            std::string memModelOp;
            std::vector<const char *> spvExts;
            std::vector<const char *> spvCaps;
            ComputeShaderSpec spec;
            adjustSpecForUntypedPointers(spec, spvExts, spvCaps);
            adjustSpecForMemoryModel(memModel, spec, memModelOp, spvExts, spvCaps);
            adjustSpecForDataTypes(BASE_DATA_TYPE_CASES[i], spec, spvExts, spvCaps);
            adjustSpecForVariablePointers(spec, spvExts, spvCaps);

            specMap["memModelOp"]                         = memModelOp;
            specMap["extensions"]                         = toString(spvExts);
            specMap["capabilities"]                       = toString(spvCaps);
            const tcu::StringTemplate tempShaderFunctions = tcu::StringTemplate(shaderFunctions.specialize(specMap));

            std::string shaderVariablesStr = shaderVariables.specialize(specMap);
            if (BASE_DATA_TYPE_CASES[i] != DataTypes::UINT32)
            {
                shaderVariablesStr = "%uint32     = OpTypeInt 32 0\n" + shaderVariablesStr;
            }

            const std::string shaderAsm = shaderHeader.specialize(specMap) + shaderAnnotations.specialize(specMap) +
                                          shaderVariablesStr + tempShaderFunctions.specialize(specMap);

            FilledBufferDesc desc;
            desc.dataType   = BASE_DATA_TYPE_CASES[i];
            desc.elemCount  = 1;
            desc.padding    = 0;
            desc.fillType   = FillingTypes::VALUE;
            desc.value      = 1;
            Resource input0 = createFilledResource(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, desc);
            desc.value      = 0;
            Resource input1 = createFilledResource(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, desc);

            desc.fillType   = FillingTypes::VALUE;
            desc.value      = j ? 1.0 : 0.0;
            Resource output = createFilledResource(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, desc);
            spec.outputs.push_back(output);

            desc.dataType      = DataTypes::UINT32;
            desc.value         = j;
            BufferSp pushConst = createFilledBuffer(desc);

            spec.assembly      = shaderAsm;
            spec.numWorkGroups = tcu::IVec3(1, 1, 1);
            spec.spirvVersion  = SPIRV_VERSION_1_4;
            spec.pushConstants = pushConst;
            spec.inputs.push_back(input0);
            spec.inputs.push_back(input1);
            spec.extensions.push_back("VK_KHR_storage_buffer_storage_class");

            if (j)
            {
                firstGroup->addChild(new SpvAsmComputeShaderCase(testCtx, testName.c_str(), spec));
            }
            else
            {
                secondGroup->addChild(new SpvAsmComputeShaderCase(testCtx, testName.c_str(), spec));
            }
        }
    }

    testGroup->addChild(firstGroup.release());
    testGroup->addChild(secondGroup.release());
}

void addPhysicalStorageOpSelectTests(tcu::TestCaseGroup *testGroup, MEMORY_MODEL_TYPE memModel)
{
    tcu::TestContext &testCtx = testGroup->getTestContext();

    de::MovePtr<tcu::TestCaseGroup> firstGroup(new tcu::TestCaseGroup(testCtx, "first", ""));
    de::MovePtr<tcu::TestCaseGroup> secondGroup(new tcu::TestCaseGroup(testCtx, "second", ""));

    tcu::StringTemplate shaderHeader(createShaderHeader("%push_constant_var %all_data_var"));

    tcu::StringTemplate shaderAnnotations(createShaderAnnotations(PointerTestCases::OP_SELECT_PHYSICAL_STORAGE));

    tcu::StringTemplate shaderVariables(createShaderVariables(PointerTestCases::OP_SELECT_PHYSICAL_STORAGE));

    tcu::StringTemplate shaderFunctions(createShaderMain(PointerTestCases::OP_SELECT_PHYSICAL_STORAGE));

    for (uint32_t i = 0; i < DE_LENGTH_OF_ARRAY(BASE_DATA_TYPE_CASES); ++i)
    {
        for (uint32_t j = 0; j < 2; ++j)
        {
            std::string testName = toString(BASE_DATA_TYPE_CASES[i]);

            std::map<std::string, std::string> specMap;
            specMap["baseDecl"]  = getDeclaration(BASE_DATA_TYPE_CASES[i]);
            specMap["baseType"]  = toString(BASE_DATA_TYPE_CASES[i]);
            specMap["alignment"] = std::to_string(getSizeInBytes(BASE_DATA_TYPE_CASES[i]));

            std::string memModelOp;
            std::vector<const char *> spvExts;
            std::vector<const char *> spvCaps;
            ComputeShaderSpec spec;
            adjustSpecForUntypedPointers(spec, spvExts, spvCaps);
            adjustSpecForMemoryModel(memModel, spec, memModelOp, spvExts, spvCaps);
            adjustSpecForDataTypes(BASE_DATA_TYPE_CASES[i], spec, spvExts, spvCaps);
            adjustSpecForPhysicalStorageBuffer(memModel, spec, memModelOp, spvExts, spvCaps);

            specMap["memModelOp"]                         = memModelOp;
            specMap["extensions"]                         = toString(spvExts);
            specMap["capabilities"]                       = toString(spvCaps);
            const tcu::StringTemplate tempShaderFunctions = tcu::StringTemplate(shaderFunctions.specialize(specMap));

            std::string shaderVariablesStr = shaderVariables.specialize(specMap);
            if (BASE_DATA_TYPE_CASES[i] != DataTypes::UINT32)
            {
                shaderVariablesStr = "%uint32     = OpTypeInt  32      0\n" + shaderVariablesStr;
            }

            const std::string shaderAsm = shaderHeader.specialize(specMap) + shaderAnnotations.specialize(specMap) +
                                          shaderVariablesStr + tempShaderFunctions.specialize(specMap);

            FilledBufferDesc desc;
            desc.dataType   = BASE_DATA_TYPE_CASES[i];
            desc.elemCount  = 1;
            desc.padding    = 0;
            desc.fillType   = FillingTypes::VALUE;
            desc.value      = 1;
            Resource input0 = createFilledResource(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, desc);
            desc.value      = 0;
            Resource input1 = createFilledResource(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, desc);

            desc.fillType   = FillingTypes::VALUE;
            desc.value      = j ? 1.0 : 0.0;
            Resource output = createFilledResource(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, desc);
            spec.outputs.push_back(output);

            desc.dataType      = DataTypes::UINT32;
            desc.elemCount     = 1;
            desc.value         = j;
            BufferSp pushConst = createFilledBuffer(desc);

            spec.assembly              = shaderAsm;
            spec.numWorkGroups         = tcu::IVec3(1, 1, 1);
            spec.spirvVersion          = SPIRV_VERSION_1_4;
            spec.usesPhysStorageBuffer = true;
            spec.pushConstants         = pushConst;
            spec.inputs.push_back(input0);
            spec.inputs.push_back(input1);
            spec.extensions.push_back("VK_KHR_storage_buffer_storage_class");

            if (j)
            {
                firstGroup->addChild(new SpvAsmComputeShaderCase(testCtx, testName.c_str(), spec));
            }
            else
            {
                secondGroup->addChild(new SpvAsmComputeShaderCase(testCtx, testName.c_str(), spec));
            }
        }
    }

    testGroup->addChild(firstGroup.release());
    testGroup->addChild(secondGroup.release());
}

void addVariablePtrOpPhiTests(tcu::TestCaseGroup *testGroup, MEMORY_MODEL_TYPE memModel)
{
    tcu::TestContext &testCtx = testGroup->getTestContext();

    de::MovePtr<tcu::TestCaseGroup> firstGroup(new tcu::TestCaseGroup(testCtx, "first", ""));
    de::MovePtr<tcu::TestCaseGroup> secondGroup(new tcu::TestCaseGroup(testCtx, "second", ""));

    tcu::StringTemplate shaderHeader(
        createShaderHeader("%push_constant_var %input_data_0_untyped_var %input_data_1_untyped_var %output_data_var"));

    tcu::StringTemplate shaderAnnotations(createShaderAnnotations(PointerTestCases::OP_PHI_VARIABLE_PTR));

    tcu::StringTemplate shaderVariables(createShaderVariables(PointerTestCases::OP_PHI_VARIABLE_PTR));

    tcu::StringTemplate shaderFunctions(createShaderMain(PointerTestCases::OP_PHI_VARIABLE_PTR));

    for (uint32_t i = 0; i < DE_LENGTH_OF_ARRAY(BASE_DATA_TYPE_CASES); ++i)
    {
        for (uint32_t j = 0; j < 2; ++j)
        {
            std::string testName = toString(BASE_DATA_TYPE_CASES[i]);

            std::map<std::string, std::string> specMap;
            specMap["baseDecl"]  = getDeclaration(BASE_DATA_TYPE_CASES[i]);
            specMap["baseType"]  = toString(BASE_DATA_TYPE_CASES[i]);
            specMap["alignment"] = std::to_string(getSizeInBytes(BASE_DATA_TYPE_CASES[i]));

            std::string memModelOp;
            std::vector<const char *> spvExts;
            std::vector<const char *> spvCaps;
            ComputeShaderSpec spec;
            adjustSpecForUntypedPointers(spec, spvExts, spvCaps);
            adjustSpecForMemoryModel(memModel, spec, memModelOp, spvExts, spvCaps);
            adjustSpecForDataTypes(BASE_DATA_TYPE_CASES[i], spec, spvExts, spvCaps);
            adjustSpecForVariablePointers(spec, spvExts, spvCaps);

            specMap["memModelOp"]                         = memModelOp;
            specMap["extensions"]                         = toString(spvExts);
            specMap["capabilities"]                       = toString(spvCaps);
            const tcu::StringTemplate tempShaderFunctions = tcu::StringTemplate(shaderFunctions.specialize(specMap));

            std::string shaderVariablesStr = shaderVariables.specialize(specMap);
            if (BASE_DATA_TYPE_CASES[i] != DataTypes::UINT32)
            {
                shaderVariablesStr = "%uint32     = OpTypeInt  32      0\n" + shaderVariablesStr;
            }

            const std::string shaderAsm = shaderHeader.specialize(specMap) + shaderAnnotations.specialize(specMap) +
                                          shaderVariablesStr + tempShaderFunctions.specialize(specMap);

            FilledBufferDesc desc;
            desc.dataType   = BASE_DATA_TYPE_CASES[i];
            desc.elemCount  = 1;
            desc.padding    = 0;
            desc.fillType   = FillingTypes::VALUE;
            desc.value      = 1;
            Resource input0 = createFilledResource(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, desc);
            desc.value      = 0;
            Resource input1 = createFilledResource(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, desc);

            desc.fillType   = FillingTypes::VALUE;
            desc.value      = j ? 1.0 : 0.0;
            Resource output = createFilledResource(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, desc);
            spec.outputs.push_back(output);

            desc.dataType      = DataTypes::UINT32;
            desc.value         = j;
            BufferSp pushConst = createFilledBuffer(desc);

            spec.assembly      = shaderAsm;
            spec.numWorkGroups = tcu::IVec3(1, 1, 1);
            // After spir-v version 1.6 OpBranchConditional labels nust not be the same.
            spec.spirvVersion  = SPIRV_VERSION_1_4;
            spec.pushConstants = pushConst;
            spec.inputs.push_back(input0);
            spec.inputs.push_back(input1);
            spec.extensions.push_back("VK_KHR_storage_buffer_storage_class");

            if (j)
            {
                firstGroup->addChild(new SpvAsmComputeShaderCase(testCtx, testName.c_str(), spec));
            }
            else
            {
                secondGroup->addChild(new SpvAsmComputeShaderCase(testCtx, testName.c_str(), spec));
            }
        }
    }

    testGroup->addChild(firstGroup.release());
    testGroup->addChild(secondGroup.release());
}

void addPhysicalStorageOpPhiTests(tcu::TestCaseGroup *testGroup, MEMORY_MODEL_TYPE memModel)
{
    tcu::TestContext &testCtx = testGroup->getTestContext();

    de::MovePtr<tcu::TestCaseGroup> firstGroup(new tcu::TestCaseGroup(testCtx, "first", ""));
    de::MovePtr<tcu::TestCaseGroup> secondGroup(new tcu::TestCaseGroup(testCtx, "second", ""));

    tcu::StringTemplate shaderHeader(createShaderHeader("%push_constant_var %all_data_var"));

    tcu::StringTemplate shaderAnnotations(createShaderAnnotations(PointerTestCases::OP_PHI_PHYSICAL_STORAGE));

    tcu::StringTemplate shaderVariables(createShaderVariables(PointerTestCases::OP_PHI_PHYSICAL_STORAGE));

    tcu::StringTemplate shaderFunctions(createShaderMain(PointerTestCases::OP_PHI_PHYSICAL_STORAGE));

    for (uint32_t i = 0; i < DE_LENGTH_OF_ARRAY(BASE_DATA_TYPE_CASES); ++i)
    {
        for (uint32_t j = 0; j < 2; ++j)
        {
            std::string testName = toString(BASE_DATA_TYPE_CASES[i]);

            std::map<std::string, std::string> specMap;
            specMap["baseDecl"]  = getDeclaration(BASE_DATA_TYPE_CASES[i]);
            specMap["baseType"]  = toString(BASE_DATA_TYPE_CASES[i]);
            specMap["alignment"] = std::to_string(getSizeInBytes(BASE_DATA_TYPE_CASES[i]));

            std::string memModelOp;
            std::vector<const char *> spvExts;
            std::vector<const char *> spvCaps;
            ComputeShaderSpec spec;
            adjustSpecForUntypedPointers(spec, spvExts, spvCaps);
            adjustSpecForMemoryModel(memModel, spec, memModelOp, spvExts, spvCaps);
            adjustSpecForDataTypes(BASE_DATA_TYPE_CASES[i], spec, spvExts, spvCaps);
            adjustSpecForPhysicalStorageBuffer(memModel, spec, memModelOp, spvExts, spvCaps);

            specMap["memModelOp"]                         = memModelOp;
            specMap["extensions"]                         = toString(spvExts);
            specMap["capabilities"]                       = toString(spvCaps);
            const tcu::StringTemplate tempShaderFunctions = tcu::StringTemplate(shaderFunctions.specialize(specMap));

            std::string shaderVariablesStr = shaderVariables.specialize(specMap);
            if (BASE_DATA_TYPE_CASES[i] != DataTypes::UINT32)
            {
                shaderVariablesStr = "%uint32     = OpTypeInt  32      0\n" + shaderVariablesStr;
            }

            const std::string shaderAsm = shaderHeader.specialize(specMap) + shaderAnnotations.specialize(specMap) +
                                          shaderVariablesStr + tempShaderFunctions.specialize(specMap);

            FilledBufferDesc desc;
            desc.dataType   = BASE_DATA_TYPE_CASES[i];
            desc.elemCount  = 1;
            desc.padding    = 0;
            desc.fillType   = FillingTypes::VALUE;
            desc.value      = 1;
            Resource input0 = createFilledResource(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, desc);
            desc.value      = 0;
            Resource input1 = createFilledResource(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, desc);

            desc.fillType   = FillingTypes::VALUE;
            desc.value      = j ? 1.0 : 0.0;
            Resource output = createFilledResource(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, desc);
            spec.outputs.push_back(output);

            desc.dataType      = DataTypes::UINT32;
            desc.elemCount     = 1;
            desc.value         = j;
            BufferSp pushConst = createFilledBuffer(desc);

            spec.assembly      = shaderAsm;
            spec.numWorkGroups = tcu::IVec3(1, 1, 1);
            // After spir-v version 1.6 OpBranchConditional labels nust not be the same.
            spec.spirvVersion          = SPIRV_VERSION_1_4;
            spec.usesPhysStorageBuffer = true;
            spec.pushConstants         = pushConst;
            spec.inputs.push_back(input0);
            spec.inputs.push_back(input1);
            spec.extensions.push_back("VK_KHR_storage_buffer_storage_class");

            if (j)
            {
                firstGroup->addChild(new SpvAsmComputeShaderCase(testCtx, testName.c_str(), spec));
            }
            else
            {
                secondGroup->addChild(new SpvAsmComputeShaderCase(testCtx, testName.c_str(), spec));
            }
        }
    }

    testGroup->addChild(firstGroup.release());
    testGroup->addChild(secondGroup.release());
}

void addVariablePtrOpPtrEqualTests(tcu::TestCaseGroup *testGroup, MEMORY_MODEL_TYPE memModel)
{
    tcu::TestContext &testCtx = testGroup->getTestContext();

    de::MovePtr<tcu::TestCaseGroup> equalGroup(new tcu::TestCaseGroup(testCtx, "equal", ""));
    de::MovePtr<tcu::TestCaseGroup> notEqualGroup(new tcu::TestCaseGroup(testCtx, "not_equal", ""));

    tcu::StringTemplate shaderHeader(createShaderHeader("%input_data_var %output_data_var"));

    tcu::StringTemplate shaderAnnotations(createShaderAnnotations(PointerTestCases::OP_PTR_EQUAL_VARIABLE_PTR));

    tcu::StringTemplate shaderVariables(createShaderVariables(PointerTestCases::OP_PTR_EQUAL_VARIABLE_PTR));

    tcu::StringTemplate shaderFunctions(createShaderMain(PointerTestCases::OP_PTR_EQUAL_VARIABLE_PTR));

    // Equal - same buffer same index
    for (uint32_t i = 0; i < DE_LENGTH_OF_ARRAY(BASE_DATA_TYPE_CASES); ++i)
    {
        std::string testName = toString(BASE_DATA_TYPE_CASES[i]);

        std::map<std::string, std::string> specMap;
        specMap["baseDecl"]  = getDeclaration(BASE_DATA_TYPE_CASES[i]);
        specMap["baseType"]  = toString(BASE_DATA_TYPE_CASES[i]);
        specMap["alignment"] = std::to_string(getSizeInBytes(BASE_DATA_TYPE_CASES[i]));
        specMap["mainLogic"] =
            "%input_loc_first  = OpUntypedAccessChainKHR %storage_buffer_untyped_ptr     %input_buffer    "
            "%input_data_var %c_uint32_0 %c_uint32_2\n"
            "%input_loc_second = OpUntypedAccessChainKHR %storage_buffer_untyped_ptr     %input_buffer    "
            "%input_data_var %c_uint32_0 %c_uint32_2\n"
            "%are_equal        = OpPtrEqual              %bool                           %input_loc_first   "
            "%input_loc_second\n";

        std::string memModelOp;
        std::vector<const char *> spvExts;
        std::vector<const char *> spvCaps;
        ComputeShaderSpec spec;
        adjustSpecForUntypedPointers(spec, spvExts, spvCaps);
        adjustSpecForMemoryModel(memModel, spec, memModelOp, spvExts, spvCaps);
        adjustSpecForDataTypes(BASE_DATA_TYPE_CASES[i], spec, spvExts, spvCaps);
        adjustSpecForVariablePointers(spec, spvExts, spvCaps);

        specMap["memModelOp"]                         = memModelOp;
        specMap["extensions"]                         = toString(spvExts);
        specMap["capabilities"]                       = toString(spvCaps);
        const tcu::StringTemplate tempShaderFunctions = tcu::StringTemplate(shaderFunctions.specialize(specMap));

        std::string shaderVariablesStr = shaderVariables.specialize(specMap);
        if (BASE_DATA_TYPE_CASES[i] != DataTypes::UINT32)
        {
            shaderVariablesStr = "%uint32     = OpTypeInt  32      0\n" + shaderVariablesStr;
        }

        const std::string shaderAsm = shaderHeader.specialize(specMap) + shaderAnnotations.specialize(specMap) +
                                      shaderVariablesStr + tempShaderFunctions.specialize(specMap);

        FilledBufferDesc desc;
        desc.dataType  = BASE_DATA_TYPE_CASES[i];
        desc.elemCount = 32;
        desc.padding   = 0;
        desc.fillType  = FillingTypes::INCREMENTED;
        Resource input = createFilledResource(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, desc);

        desc.dataType   = DataTypes::UINT32;
        desc.elemCount  = 1;
        desc.padding    = 0;
        desc.fillType   = FillingTypes::VALUE;
        desc.value      = 1;
        Resource output = createFilledResource(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, desc);

        spec.assembly      = shaderAsm;
        spec.numWorkGroups = tcu::IVec3(1, 1, 1);
        spec.spirvVersion  = SPIRV_VERSION_1_4; // OpPtrEqual, OpPtrNotEqual and OpPtrDiff requires SPIR-V 1.4
        spec.inputs.push_back(input);
        spec.outputs.push_back(output);
        spec.extensions.push_back("VK_KHR_storage_buffer_storage_class");

        equalGroup->addChild(new SpvAsmComputeShaderCase(testCtx, testName.c_str(), spec));
    }

    // Equal - same byte offset indexed as different types
    for (uint32_t i = 0; i < DE_LENGTH_OF_ARRAY(BASE_DATA_TYPE_CASES); ++i)
    {
        std::string testName =
            toString(BASE_DATA_TYPE_CASES[i]) + std::string(getNameStrForVarPtrs(BASE_DATA_TYPE_CASES[i]));

        std::map<std::string, std::string> specMap;
        specMap["baseDecl"]              = getDeclaration(BASE_DATA_TYPE_CASES[i]);
        specMap["baseType"]              = toString(BASE_DATA_TYPE_CASES[i]);
        specMap["secondType"]            = getSecondTypeDefinitionForVarPtrs(BASE_DATA_TYPE_CASES[i]);
        specMap["secondArray"]           = getSecondArrayDefinitionForVarPtrs(BASE_DATA_TYPE_CASES[i]);
        specMap["alignment"]             = std::to_string(getSizeInBytes(BASE_DATA_TYPE_CASES[i]));
        specMap["secondArrayDecoration"] = getSecondArrayDecorationForVarPtrs(BASE_DATA_TYPE_CASES[i]);
        specMap["mainLogic"] = "%input_array_loc = OpUntypedAccessChainKHR %storage_buffer_untyped_ptr %input_buffer "
                               "%input_data_var %c_uint32_0\n"
                               "%input_loc_first = OpUntypedAccessChainKHR %storage_buffer_untyped_ptr "
                               "%array_first_32 %input_array_loc %c_uint32_4\n"
                               "%input_loc_second = OpUntypedAccessChainKHR %storage_buffer_untyped_ptr "
                               "%array_second_32 %input_array_loc " +
                               std::string(getSameByteIndexForVarPtrs(BASE_DATA_TYPE_CASES[i])) +
                               "%are_equal        = OpPtrEqual              %bool       "
                               "                    %input_loc_first   "
                               "%input_loc_second\n";

        std::string memModelOp;
        std::vector<const char *> spvExts;
        std::vector<const char *> spvCaps;
        ComputeShaderSpec spec;
        adjustSpecForUntypedPointers(spec, spvExts, spvCaps);
        adjustSpecForMemoryModel(memModel, spec, memModelOp, spvExts, spvCaps);
        adjustSpecForDataTypes(BASE_DATA_TYPE_CASES[i], spec, spvExts, spvCaps);
        adjustSpecForDataTypes(DataTypes::INT16, spec, spvExts, spvCaps);
        adjustSpecForVariablePointers(spec, spvExts, spvCaps);

        specMap["memModelOp"]                         = memModelOp;
        specMap["extensions"]                         = toString(spvExts);
        specMap["capabilities"]                       = toString(spvCaps);
        const tcu::StringTemplate tempShaderFunctions = tcu::StringTemplate(shaderFunctions.specialize(specMap));

        std::string shaderVariablesStr = shaderVariables.specialize(specMap);
        if (BASE_DATA_TYPE_CASES[i] != DataTypes::UINT32)
        {
            shaderVariablesStr = "%uint32     = OpTypeInt  32      0\n" + shaderVariablesStr;
        }

        const std::string shaderAsm = shaderHeader.specialize(specMap) + shaderAnnotations.specialize(specMap) +
                                      shaderVariablesStr + tempShaderFunctions.specialize(specMap);

        FilledBufferDesc desc;
        desc.dataType  = BASE_DATA_TYPE_CASES[i];
        desc.elemCount = 32;
        desc.padding   = 0;
        desc.fillType  = FillingTypes::INCREMENTED;
        Resource input = createFilledResource(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, desc);

        desc.dataType   = DataTypes::UINT32;
        desc.elemCount  = 1;
        desc.padding    = 0;
        desc.fillType   = FillingTypes::VALUE;
        desc.value      = 1;
        Resource output = createFilledResource(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, desc);

        spec.assembly      = shaderAsm;
        spec.numWorkGroups = tcu::IVec3(1, 1, 1);
        spec.spirvVersion  = SPIRV_VERSION_1_4; // OpPtrEqual, OpPtrNotEqual and OpPtrDiff requires SPIR-V 1.4
        spec.inputs.push_back(input);
        spec.outputs.push_back(output);
        spec.extensions.push_back("VK_KHR_storage_buffer_storage_class");

        equalGroup->addChild(new SpvAsmComputeShaderCase(testCtx, testName.c_str(), spec));
    }

    // Equal - typed and untyped pointer
    for (uint32_t i = 0; i < DE_LENGTH_OF_ARRAY(BASE_DATA_TYPE_CASES); ++i)
    {
        std::string testName = toString(BASE_DATA_TYPE_CASES[i]) + std::string("_typed_and_untyped");

        std::map<std::string, std::string> specMap;
        specMap["baseDecl"]  = getDeclaration(BASE_DATA_TYPE_CASES[i]);
        specMap["baseType"]  = toString(BASE_DATA_TYPE_CASES[i]);
        specMap["alignment"] = std::to_string(getSizeInBytes(BASE_DATA_TYPE_CASES[i]));
        specMap["mainLogic"] =
            "%input_loc_first  = OpUntypedAccessChainKHR %storage_buffer_untyped_ptr       %input_buffer    "
            "%input_data_var  %c_uint32_0 %c_uint32_2\n"
            "%input_loc_second = OpAccessChain           %" +
            std::string(toString(BASE_DATA_TYPE_CASES[i])) +
            "_storage_buffer_ptr                    "
            "%input_data_var %c_uint32_0 %c_uint32_2\n"
            "%are_equal        = OpPtrEqual              %bool                           %input_loc_first   "
            "%input_loc_second\n";

        std::string memModelOp;
        std::vector<const char *> spvExts;
        std::vector<const char *> spvCaps;
        ComputeShaderSpec spec;
        adjustSpecForUntypedPointers(spec, spvExts, spvCaps);
        adjustSpecForMemoryModel(memModel, spec, memModelOp, spvExts, spvCaps);
        adjustSpecForDataTypes(BASE_DATA_TYPE_CASES[i], spec, spvExts, spvCaps);
        adjustSpecForVariablePointers(spec, spvExts, spvCaps);

        specMap["memModelOp"]                         = memModelOp;
        specMap["extensions"]                         = toString(spvExts);
        specMap["capabilities"]                       = toString(spvCaps);
        const tcu::StringTemplate tempShaderFunctions = tcu::StringTemplate(shaderFunctions.specialize(specMap));

        std::string shaderVariablesStr = shaderVariables.specialize(specMap);
        if (BASE_DATA_TYPE_CASES[i] != DataTypes::UINT32)
        {
            shaderVariablesStr = "%uint32     = OpTypeInt  32      0\n" + shaderVariablesStr;
        }

        const std::string shaderAsm = shaderHeader.specialize(specMap) + shaderAnnotations.specialize(specMap) +
                                      shaderVariablesStr + tempShaderFunctions.specialize(specMap);

        FilledBufferDesc desc;
        desc.dataType  = BASE_DATA_TYPE_CASES[i];
        desc.elemCount = 32;
        desc.padding   = 0;
        desc.fillType  = FillingTypes::INCREMENTED;
        Resource input = createFilledResource(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, desc);

        desc.dataType   = DataTypes::UINT32;
        desc.elemCount  = 1;
        desc.padding    = 0;
        desc.fillType   = FillingTypes::VALUE;
        desc.value      = 1;
        Resource output = createFilledResource(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, desc);

        spec.assembly      = shaderAsm;
        spec.numWorkGroups = tcu::IVec3(1, 1, 1);
        spec.spirvVersion  = SPIRV_VERSION_1_4; // OpPtrEqual, OpPtrNotEqual and OpPtrDiff requires SPIR-V 1.4
        spec.inputs.push_back(input);
        spec.outputs.push_back(output);
        spec.extensions.push_back("VK_KHR_storage_buffer_storage_class");

        equalGroup->addChild(new SpvAsmComputeShaderCase(testCtx, testName.c_str(), spec));
    }

    // Not equal - same buffer different indices
    for (uint32_t i = 0; i < DE_LENGTH_OF_ARRAY(BASE_DATA_TYPE_CASES); ++i)
    {
        std::string testName = toString(BASE_DATA_TYPE_CASES[i]);

        std::map<std::string, std::string> specMap;
        specMap["baseDecl"]  = getDeclaration(BASE_DATA_TYPE_CASES[i]);
        specMap["baseType"]  = toString(BASE_DATA_TYPE_CASES[i]);
        specMap["alignment"] = std::to_string(getSizeInBytes(BASE_DATA_TYPE_CASES[i]));
        specMap["mainLogic"] =
            "%input_loc_first  = OpUntypedAccessChainKHR %storage_buffer_untyped_ptr     %input_buffer    "
            "%input_data_var %c_uint32_0 %c_uint32_2\n"
            "%input_loc_second = OpUntypedAccessChainKHR %storage_buffer_untyped_ptr     %input_buffer    "
            "%input_data_var %c_uint32_0 %c_uint32_4\n"
            "%are_equal        = OpPtrEqual              %bool                           %input_loc_first   "
            "%input_loc_second\n";

        std::string memModelOp;
        std::vector<const char *> spvExts;
        std::vector<const char *> spvCaps;
        ComputeShaderSpec spec;
        adjustSpecForUntypedPointers(spec, spvExts, spvCaps);
        adjustSpecForMemoryModel(memModel, spec, memModelOp, spvExts, spvCaps);
        adjustSpecForDataTypes(BASE_DATA_TYPE_CASES[i], spec, spvExts, spvCaps);
        adjustSpecForVariablePointers(spec, spvExts, spvCaps);

        specMap["memModelOp"]                         = memModelOp;
        specMap["extensions"]                         = toString(spvExts);
        specMap["capabilities"]                       = toString(spvCaps);
        const tcu::StringTemplate tempShaderFunctions = tcu::StringTemplate(shaderFunctions.specialize(specMap));

        std::string shaderVariablesStr = shaderVariables.specialize(specMap);
        if (BASE_DATA_TYPE_CASES[i] != DataTypes::UINT32)
        {
            shaderVariablesStr = "%uint32     = OpTypeInt  32      0\n" + shaderVariablesStr;
        }

        const std::string shaderAsm = shaderHeader.specialize(specMap) + shaderAnnotations.specialize(specMap) +
                                      shaderVariablesStr + tempShaderFunctions.specialize(specMap);

        FilledBufferDesc desc;
        desc.dataType  = BASE_DATA_TYPE_CASES[i];
        desc.elemCount = 32;
        desc.padding   = 0;
        desc.fillType  = FillingTypes::INCREMENTED;
        Resource input = createFilledResource(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, desc);

        desc.dataType   = DataTypes::UINT32;
        desc.elemCount  = 1;
        desc.padding    = 0;
        desc.fillType   = FillingTypes::VALUE;
        desc.value      = 0;
        Resource output = createFilledResource(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, desc);

        spec.assembly      = shaderAsm;
        spec.numWorkGroups = tcu::IVec3(1, 1, 1);
        spec.spirvVersion  = SPIRV_VERSION_1_4; // OpPtrEqual, OpPtrNotEqual and OpPtrDiff requires SPIR-V 1.4
        spec.inputs.push_back(input);
        spec.outputs.push_back(output);
        spec.extensions.push_back("VK_KHR_storage_buffer_storage_class");

        notEqualGroup->addChild(new SpvAsmComputeShaderCase(testCtx, testName.c_str(), spec));
    }

    // Not equal - same buffer different indices one typed one untyped pointer
    for (uint32_t i = 0; i < DE_LENGTH_OF_ARRAY(BASE_DATA_TYPE_CASES); ++i)
    {
        std::string testName = toString(BASE_DATA_TYPE_CASES[i]) + std::string("_typed_and_untyped");

        std::map<std::string, std::string> specMap;
        specMap["baseDecl"]  = getDeclaration(BASE_DATA_TYPE_CASES[i]);
        specMap["baseType"]  = toString(BASE_DATA_TYPE_CASES[i]);
        specMap["alignment"] = std::to_string(getSizeInBytes(BASE_DATA_TYPE_CASES[i]));
        specMap["mainLogic"] =
            "%input_loc_first  = OpUntypedAccessChainKHR %storage_buffer_untyped_ptr       %input_buffer    "
            "%input_data_var  %c_uint32_0 %c_uint32_2\n"
            "%input_loc_second = OpAccessChain           %" +
            std::string(toString(BASE_DATA_TYPE_CASES[i])) +
            "_storage_buffer_ptr                    "
            "%input_data_var %c_uint32_0 %c_uint32_4\n"
            "%are_equal        = OpPtrEqual              %bool                           %input_loc_first   "
            "%input_loc_second\n";

        std::string memModelOp;
        std::vector<const char *> spvExts;
        std::vector<const char *> spvCaps;
        ComputeShaderSpec spec;
        adjustSpecForUntypedPointers(spec, spvExts, spvCaps);
        adjustSpecForMemoryModel(memModel, spec, memModelOp, spvExts, spvCaps);
        adjustSpecForDataTypes(BASE_DATA_TYPE_CASES[i], spec, spvExts, spvCaps);
        adjustSpecForVariablePointers(spec, spvExts, spvCaps);

        specMap["memModelOp"]                         = memModelOp;
        specMap["extensions"]                         = toString(spvExts);
        specMap["capabilities"]                       = toString(spvCaps);
        const tcu::StringTemplate tempShaderFunctions = tcu::StringTemplate(shaderFunctions.specialize(specMap));

        std::string shaderVariablesStr = shaderVariables.specialize(specMap);
        if (BASE_DATA_TYPE_CASES[i] != DataTypes::UINT32)
        {
            shaderVariablesStr = "%uint32     = OpTypeInt  32      0\n" + shaderVariablesStr;
        }

        const std::string shaderAsm = shaderHeader.specialize(specMap) + shaderAnnotations.specialize(specMap) +
                                      shaderVariablesStr + tempShaderFunctions.specialize(specMap);

        FilledBufferDesc desc;
        desc.dataType  = BASE_DATA_TYPE_CASES[i];
        desc.elemCount = 32;
        desc.padding   = 0;
        desc.fillType  = FillingTypes::INCREMENTED;
        Resource input = createFilledResource(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, desc);

        desc.dataType   = DataTypes::UINT32;
        desc.elemCount  = 1;
        desc.padding    = 0;
        desc.fillType   = FillingTypes::VALUE;
        desc.value      = 0;
        Resource output = createFilledResource(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, desc);

        spec.assembly      = shaderAsm;
        spec.numWorkGroups = tcu::IVec3(1, 1, 1);
        spec.spirvVersion  = SPIRV_VERSION_1_4; // OpPtrEqual, OpPtrNotEqual and OpPtrDiff requires SPIR-V 1.4
        spec.inputs.push_back(input);
        spec.outputs.push_back(output);
        spec.extensions.push_back("VK_KHR_storage_buffer_storage_class");

        notEqualGroup->addChild(new SpvAsmComputeShaderCase(testCtx, testName.c_str(), spec));
    }

    // Not equal - comparsion to null pointers
    for (uint32_t i = 0; i < DE_LENGTH_OF_ARRAY(BASE_DATA_TYPE_CASES); ++i)
    {
        std::string testName = toString(BASE_DATA_TYPE_CASES[i]) + std::string("_null_ptr");

        std::map<std::string, std::string> specMap;
        specMap["baseDecl"]  = getDeclaration(BASE_DATA_TYPE_CASES[i]);
        specMap["baseType"]  = toString(BASE_DATA_TYPE_CASES[i]);
        specMap["alignment"] = std::to_string(getSizeInBytes(BASE_DATA_TYPE_CASES[i]));
        specMap["mainLogic"] =
            "%input_loc_first  = OpUntypedAccessChainKHR %storage_buffer_untyped_ptr     %input_buffer    "
            "%input_data_var %c_uint32_0 %c_uint32_2\n"
            "%are_equal        = OpPtrEqual              %bool                           %input_loc_first   "
            "%c_null_untyped_ptr\n";

        std::string memModelOp;
        std::vector<const char *> spvExts;
        std::vector<const char *> spvCaps;
        ComputeShaderSpec spec;
        adjustSpecForUntypedPointers(spec, spvExts, spvCaps);
        adjustSpecForMemoryModel(memModel, spec, memModelOp, spvExts, spvCaps);
        adjustSpecForDataTypes(BASE_DATA_TYPE_CASES[i], spec, spvExts, spvCaps);
        adjustSpecForVariablePointers(spec, spvExts, spvCaps);

        specMap["memModelOp"]                         = memModelOp;
        specMap["extensions"]                         = toString(spvExts);
        specMap["capabilities"]                       = toString(spvCaps);
        const tcu::StringTemplate tempShaderFunctions = tcu::StringTemplate(shaderFunctions.specialize(specMap));

        std::string shaderVariablesStr = shaderVariables.specialize(specMap);
        if (BASE_DATA_TYPE_CASES[i] != DataTypes::UINT32)
        {
            shaderVariablesStr = "%uint32     = OpTypeInt  32      0\n" + shaderVariablesStr;
        }

        const std::string shaderAsm = shaderHeader.specialize(specMap) + shaderAnnotations.specialize(specMap) +
                                      shaderVariablesStr + tempShaderFunctions.specialize(specMap);

        FilledBufferDesc desc;
        desc.dataType  = BASE_DATA_TYPE_CASES[i];
        desc.elemCount = 32;
        desc.padding   = 0;
        desc.fillType  = FillingTypes::INCREMENTED;
        Resource input = createFilledResource(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, desc);

        desc.dataType   = DataTypes::UINT32;
        desc.elemCount  = 1;
        desc.padding    = 0;
        desc.fillType   = FillingTypes::VALUE;
        desc.value      = 0;
        Resource output = createFilledResource(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, desc);

        spec.assembly      = shaderAsm;
        spec.numWorkGroups = tcu::IVec3(1, 1, 1);
        spec.spirvVersion  = SPIRV_VERSION_1_4; // OpPtrEqual, OpPtrNotEqual and OpPtrDiff requires SPIR-V 1.4
        spec.inputs.push_back(input);
        spec.outputs.push_back(output);
        spec.extensions.push_back("VK_KHR_storage_buffer_storage_class");

        notEqualGroup->addChild(new SpvAsmComputeShaderCase(testCtx, testName.c_str(), spec));
    }

    testGroup->addChild(equalGroup.release());
    testGroup->addChild(notEqualGroup.release());
}

void addVariablePtrOpPtrNotEqualTests(tcu::TestCaseGroup *testGroup, MEMORY_MODEL_TYPE memModel)
{
    tcu::TestContext &testCtx = testGroup->getTestContext();

    de::MovePtr<tcu::TestCaseGroup> equalGroup(new tcu::TestCaseGroup(testCtx, "equal", ""));
    de::MovePtr<tcu::TestCaseGroup> notEqualGroup(new tcu::TestCaseGroup(testCtx, "not_equal", ""));

    tcu::StringTemplate shaderHeader(createShaderHeader("%input_data_var %output_data_var"));

    tcu::StringTemplate shaderAnnotations(createShaderAnnotations(PointerTestCases::OP_PTR_NOT_EQUAL_VARIABLE_PTR));

    tcu::StringTemplate shaderVariables(createShaderVariables(PointerTestCases::OP_PTR_NOT_EQUAL_VARIABLE_PTR));

    tcu::StringTemplate shaderFunctions(createShaderMain(PointerTestCases::OP_PTR_NOT_EQUAL_VARIABLE_PTR));

    // Equal - same buffer same index
    for (uint32_t i = 0; i < DE_LENGTH_OF_ARRAY(BASE_DATA_TYPE_CASES); ++i)
    {
        std::string testName = toString(BASE_DATA_TYPE_CASES[i]);

        std::map<std::string, std::string> specMap;
        specMap["baseDecl"]  = getDeclaration(BASE_DATA_TYPE_CASES[i]);
        specMap["baseType"]  = toString(BASE_DATA_TYPE_CASES[i]);
        specMap["alignment"] = std::to_string(getSizeInBytes(BASE_DATA_TYPE_CASES[i]));
        specMap["mainLogic"] =
            "%input_loc_first  = OpUntypedAccessChainKHR %storage_buffer_untyped_ptr     %input_buffer    "
            "%input_data_var %c_uint32_0 %c_uint32_2\n"
            "%input_loc_second = OpUntypedAccessChainKHR %storage_buffer_untyped_ptr     %input_buffer    "
            "%input_data_var %c_uint32_0 %c_uint32_2\n"
            "%are_equal        = OpPtrNotEqual           %bool                           %input_loc_first   "
            "%input_loc_second\n";

        std::string memModelOp;
        std::vector<const char *> spvExts;
        std::vector<const char *> spvCaps;
        ComputeShaderSpec spec;
        adjustSpecForUntypedPointers(spec, spvExts, spvCaps);
        adjustSpecForMemoryModel(memModel, spec, memModelOp, spvExts, spvCaps);
        adjustSpecForDataTypes(BASE_DATA_TYPE_CASES[i], spec, spvExts, spvCaps);
        adjustSpecForVariablePointers(spec, spvExts, spvCaps);

        specMap["memModelOp"]                         = memModelOp;
        specMap["extensions"]                         = toString(spvExts);
        specMap["capabilities"]                       = toString(spvCaps);
        const tcu::StringTemplate tempShaderFunctions = tcu::StringTemplate(shaderFunctions.specialize(specMap));

        std::string shaderVariablesStr = shaderVariables.specialize(specMap);
        if (BASE_DATA_TYPE_CASES[i] != DataTypes::UINT32)
        {
            shaderVariablesStr = "%uint32 = OpTypeInt 32 0\n" + shaderVariablesStr;
        }

        const std::string shaderAsm = shaderHeader.specialize(specMap) + shaderAnnotations.specialize(specMap) +
                                      shaderVariablesStr + tempShaderFunctions.specialize(specMap);

        FilledBufferDesc desc;
        desc.dataType  = BASE_DATA_TYPE_CASES[i];
        desc.elemCount = 32;
        desc.padding   = 0;
        desc.fillType  = FillingTypes::INCREMENTED;
        Resource input = createFilledResource(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, desc);

        desc.dataType   = DataTypes::UINT32;
        desc.elemCount  = 1;
        desc.padding    = 0;
        desc.fillType   = FillingTypes::VALUE;
        desc.value      = 0;
        Resource output = createFilledResource(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, desc);

        spec.assembly      = shaderAsm;
        spec.numWorkGroups = tcu::IVec3(1, 1, 1);
        spec.spirvVersion  = SPIRV_VERSION_1_4; // OpPtrEqual, OpPtrNotEqual and OpPtrDiff requires SPIR-V 1.4
        spec.inputs.push_back(input);
        spec.outputs.push_back(output);
        spec.extensions.push_back("VK_KHR_storage_buffer_storage_class");

        equalGroup->addChild(new SpvAsmComputeShaderCase(testCtx, testName.c_str(), spec));
    }

    // Equal - same byte offset indexed as different types
    for (uint32_t i = 0; i < DE_LENGTH_OF_ARRAY(BASE_DATA_TYPE_CASES); ++i)
    {
        std::string testName =
            toString(BASE_DATA_TYPE_CASES[i]) + std::string(getNameStrForVarPtrs(BASE_DATA_TYPE_CASES[i]));

        std::map<std::string, std::string> specMap;
        specMap["baseDecl"]              = getDeclaration(BASE_DATA_TYPE_CASES[i]);
        specMap["baseType"]              = toString(BASE_DATA_TYPE_CASES[i]);
        specMap["secondType"]            = getSecondTypeDefinitionForVarPtrs(BASE_DATA_TYPE_CASES[i]);
        specMap["secondArray"]           = getSecondArrayDefinitionForVarPtrs(BASE_DATA_TYPE_CASES[i]);
        specMap["alignment"]             = std::to_string(getSizeInBytes(BASE_DATA_TYPE_CASES[i]));
        specMap["secondArrayDecoration"] = getSecondArrayDecorationForVarPtrs(BASE_DATA_TYPE_CASES[i]);
        specMap["mainLogic"] = "%input_array_loc = OpUntypedAccessChainKHR %storage_buffer_untyped_ptr %input_buffer "
                               "%input_data_var %c_uint32_0\n"
                               "%input_loc_first = OpUntypedAccessChainKHR %storage_buffer_untyped_ptr "
                               "%array_first_32 %input_array_loc %c_uint32_4\n"
                               "%input_loc_second = OpUntypedAccessChainKHR %storage_buffer_untyped_ptr "
                               "%array_second_32 %input_array_loc " +
                               std::string(getSameByteIndexForVarPtrs(BASE_DATA_TYPE_CASES[i])) +
                               "%are_equal        = OpPtrNotEqual           %bool       "
                               "                    %input_loc_first   "
                               "%input_loc_second\n";

        std::string memModelOp;
        std::vector<const char *> spvExts;
        std::vector<const char *> spvCaps;
        ComputeShaderSpec spec;
        adjustSpecForUntypedPointers(spec, spvExts, spvCaps);
        adjustSpecForMemoryModel(memModel, spec, memModelOp, spvExts, spvCaps);
        adjustSpecForDataTypes(BASE_DATA_TYPE_CASES[i], spec, spvExts, spvCaps);
        adjustSpecForDataTypes(DataTypes::INT16, spec, spvExts, spvCaps);
        adjustSpecForVariablePointers(spec, spvExts, spvCaps);

        specMap["memModelOp"]                         = memModelOp;
        specMap["extensions"]                         = toString(spvExts);
        specMap["capabilities"]                       = toString(spvCaps);
        const tcu::StringTemplate tempShaderFunctions = tcu::StringTemplate(shaderFunctions.specialize(specMap));

        std::string shaderVariablesStr = shaderVariables.specialize(specMap);
        if (BASE_DATA_TYPE_CASES[i] != DataTypes::UINT32)
        {
            shaderVariablesStr = "%uint32 = OpTypeInt 32 0\n" + shaderVariablesStr;
        }

        const std::string shaderAsm = shaderHeader.specialize(specMap) + shaderAnnotations.specialize(specMap) +
                                      shaderVariablesStr + tempShaderFunctions.specialize(specMap);

        FilledBufferDesc desc;
        desc.dataType  = BASE_DATA_TYPE_CASES[i];
        desc.elemCount = 32;
        desc.padding   = 0;
        desc.fillType  = FillingTypes::INCREMENTED;
        Resource input = createFilledResource(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, desc);

        desc.dataType   = DataTypes::UINT32;
        desc.elemCount  = 1;
        desc.padding    = 0;
        desc.fillType   = FillingTypes::VALUE;
        desc.value      = 0;
        Resource output = createFilledResource(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, desc);

        spec.assembly      = shaderAsm;
        spec.numWorkGroups = tcu::IVec3(1, 1, 1);
        spec.spirvVersion  = SPIRV_VERSION_1_4; // OpPtrEqual, OpPtrNotEqual and OpPtrDiff requires SPIR-V 1.4
        spec.inputs.push_back(input);
        spec.outputs.push_back(output);
        spec.extensions.push_back("VK_KHR_storage_buffer_storage_class");

        equalGroup->addChild(new SpvAsmComputeShaderCase(testCtx, testName.c_str(), spec));
    }

    // Equal - typed and untyped pointer
    for (uint32_t i = 0; i < DE_LENGTH_OF_ARRAY(BASE_DATA_TYPE_CASES); ++i)
    {
        std::string testName = toString(BASE_DATA_TYPE_CASES[i]) + std::string("_typed_and_untyped");

        std::map<std::string, std::string> specMap;
        specMap["baseDecl"]  = getDeclaration(BASE_DATA_TYPE_CASES[i]);
        specMap["baseType"]  = toString(BASE_DATA_TYPE_CASES[i]);
        specMap["alignment"] = std::to_string(getSizeInBytes(BASE_DATA_TYPE_CASES[i]));
        specMap["mainLogic"] =
            "%input_loc_first  = OpUntypedAccessChainKHR %storage_buffer_untyped_ptr       %input_buffer    "
            "%input_data_var  %c_uint32_0 %c_uint32_2\n"
            "%input_loc_second = OpAccessChain           %" +
            std::string(toString(BASE_DATA_TYPE_CASES[i])) +
            "_storage_buffer_ptr                    "
            "%input_data_var %c_uint32_0 %c_uint32_2\n"
            "%are_equal        = OpPtrNotEqual           %bool                           %input_loc_first   "
            "%input_loc_second\n";

        std::string memModelOp;
        std::vector<const char *> spvExts;
        std::vector<const char *> spvCaps;
        ComputeShaderSpec spec;
        adjustSpecForUntypedPointers(spec, spvExts, spvCaps);
        adjustSpecForMemoryModel(memModel, spec, memModelOp, spvExts, spvCaps);
        adjustSpecForDataTypes(BASE_DATA_TYPE_CASES[i], spec, spvExts, spvCaps);
        adjustSpecForVariablePointers(spec, spvExts, spvCaps);

        specMap["memModelOp"]                         = memModelOp;
        specMap["extensions"]                         = toString(spvExts);
        specMap["capabilities"]                       = toString(spvCaps);
        const tcu::StringTemplate tempShaderFunctions = tcu::StringTemplate(shaderFunctions.specialize(specMap));

        std::string shaderVariablesStr = shaderVariables.specialize(specMap);
        if (BASE_DATA_TYPE_CASES[i] != DataTypes::UINT32)
        {
            shaderVariablesStr = "%uint32 = OpTypeInt 32 0\n" + shaderVariablesStr;
        }

        const std::string shaderAsm = shaderHeader.specialize(specMap) + shaderAnnotations.specialize(specMap) +
                                      shaderVariablesStr + tempShaderFunctions.specialize(specMap);

        FilledBufferDesc desc;
        desc.dataType  = BASE_DATA_TYPE_CASES[i];
        desc.elemCount = 32;
        desc.padding   = 0;
        desc.fillType  = FillingTypes::INCREMENTED;
        Resource input = createFilledResource(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, desc);

        desc.dataType   = DataTypes::UINT32;
        desc.elemCount  = 1;
        desc.padding    = 0;
        desc.fillType   = FillingTypes::VALUE;
        desc.value      = 0;
        Resource output = createFilledResource(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, desc);

        spec.assembly      = shaderAsm;
        spec.numWorkGroups = tcu::IVec3(1, 1, 1);
        spec.spirvVersion  = SPIRV_VERSION_1_4; // OpPtrEqual, OpPtrNotEqual and OpPtrDiff requires SPIR-V 1.4
        spec.inputs.push_back(input);
        spec.outputs.push_back(output);
        spec.extensions.push_back("VK_KHR_storage_buffer_storage_class");

        equalGroup->addChild(new SpvAsmComputeShaderCase(testCtx, testName.c_str(), spec));
    }

    // Not equal - same buffer different indices
    for (uint32_t i = 0; i < DE_LENGTH_OF_ARRAY(BASE_DATA_TYPE_CASES); ++i)
    {
        std::string testName = toString(BASE_DATA_TYPE_CASES[i]);

        std::map<std::string, std::string> specMap;
        specMap["baseDecl"]  = getDeclaration(BASE_DATA_TYPE_CASES[i]);
        specMap["baseType"]  = toString(BASE_DATA_TYPE_CASES[i]);
        specMap["alignment"] = std::to_string(getSizeInBytes(BASE_DATA_TYPE_CASES[i]));
        specMap["mainLogic"] =
            "%input_loc_first  = OpUntypedAccessChainKHR %storage_buffer_untyped_ptr     %input_buffer    "
            "%input_data_var %c_uint32_0 %c_uint32_2\n"
            "%input_loc_second = OpUntypedAccessChainKHR %storage_buffer_untyped_ptr     %input_buffer    "
            "%input_data_var %c_uint32_0 %c_uint32_4\n"
            "%are_equal        = OpPtrNotEqual           %bool                           %input_loc_first   "
            "%input_loc_second\n";

        std::string memModelOp;
        std::vector<const char *> spvExts;
        std::vector<const char *> spvCaps;
        ComputeShaderSpec spec;
        adjustSpecForUntypedPointers(spec, spvExts, spvCaps);
        adjustSpecForMemoryModel(memModel, spec, memModelOp, spvExts, spvCaps);
        adjustSpecForDataTypes(BASE_DATA_TYPE_CASES[i], spec, spvExts, spvCaps);
        adjustSpecForVariablePointers(spec, spvExts, spvCaps);

        specMap["memModelOp"]                         = memModelOp;
        specMap["extensions"]                         = toString(spvExts);
        specMap["capabilities"]                       = toString(spvCaps);
        const tcu::StringTemplate tempShaderFunctions = tcu::StringTemplate(shaderFunctions.specialize(specMap));

        std::string shaderVariablesStr = shaderVariables.specialize(specMap);
        if (BASE_DATA_TYPE_CASES[i] != DataTypes::UINT32)
        {
            shaderVariablesStr = "%uint32 = OpTypeInt 32 0\n" + shaderVariablesStr;
        }

        const std::string shaderAsm = shaderHeader.specialize(specMap) + shaderAnnotations.specialize(specMap) +
                                      shaderVariablesStr + tempShaderFunctions.specialize(specMap);

        FilledBufferDesc desc;
        desc.dataType  = BASE_DATA_TYPE_CASES[i];
        desc.elemCount = 32;
        desc.padding   = 0;
        desc.fillType  = FillingTypes::INCREMENTED;
        Resource input = createFilledResource(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, desc);

        desc.dataType   = DataTypes::UINT32;
        desc.elemCount  = 1;
        desc.padding    = 0;
        desc.fillType   = FillingTypes::VALUE;
        desc.value      = 1;
        Resource output = createFilledResource(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, desc);

        spec.assembly      = shaderAsm;
        spec.numWorkGroups = tcu::IVec3(1, 1, 1);
        spec.spirvVersion  = SPIRV_VERSION_1_4; // OpPtrEqual, OpPtrNotEqual and OpPtrDiff requires SPIR-V 1.4
        spec.inputs.push_back(input);
        spec.outputs.push_back(output);
        spec.extensions.push_back("VK_KHR_storage_buffer_storage_class");

        notEqualGroup->addChild(new SpvAsmComputeShaderCase(testCtx, testName.c_str(), spec));
    }

    // Not equal - same buffer different indices one typed one untyped pointer
    for (uint32_t i = 0; i < DE_LENGTH_OF_ARRAY(BASE_DATA_TYPE_CASES); ++i)
    {
        std::string testName = toString(BASE_DATA_TYPE_CASES[i]) + std::string("_typed_and_untyped");

        std::map<std::string, std::string> specMap;
        specMap["baseDecl"]  = getDeclaration(BASE_DATA_TYPE_CASES[i]);
        specMap["baseType"]  = toString(BASE_DATA_TYPE_CASES[i]);
        specMap["alignment"] = std::to_string(getSizeInBytes(BASE_DATA_TYPE_CASES[i]));
        specMap["mainLogic"] =
            "%input_loc_first  = OpUntypedAccessChainKHR %storage_buffer_untyped_ptr       %input_buffer    "
            "%input_data_var  %c_uint32_0 %c_uint32_2\n"
            "%input_loc_second = OpAccessChain           %" +
            std::string(toString(BASE_DATA_TYPE_CASES[i])) +
            "_storage_buffer_ptr                    "
            "%input_data_var %c_uint32_0 %c_uint32_4\n"
            "%are_equal        = OpPtrNotEqual           %bool                           %input_loc_first   "
            "%input_loc_second\n";

        std::string memModelOp;
        std::vector<const char *> spvExts;
        std::vector<const char *> spvCaps;
        ComputeShaderSpec spec;
        adjustSpecForUntypedPointers(spec, spvExts, spvCaps);
        adjustSpecForMemoryModel(memModel, spec, memModelOp, spvExts, spvCaps);
        adjustSpecForDataTypes(BASE_DATA_TYPE_CASES[i], spec, spvExts, spvCaps);
        adjustSpecForVariablePointers(spec, spvExts, spvCaps);

        specMap["memModelOp"]                         = memModelOp;
        specMap["extensions"]                         = toString(spvExts);
        specMap["capabilities"]                       = toString(spvCaps);
        const tcu::StringTemplate tempShaderFunctions = tcu::StringTemplate(shaderFunctions.specialize(specMap));

        std::string shaderVariablesStr = shaderVariables.specialize(specMap);
        if (BASE_DATA_TYPE_CASES[i] != DataTypes::UINT32)
        {
            shaderVariablesStr = "%uint32 = OpTypeInt 32 0\n" + shaderVariablesStr;
        }

        const std::string shaderAsm = shaderHeader.specialize(specMap) + shaderAnnotations.specialize(specMap) +
                                      shaderVariablesStr + tempShaderFunctions.specialize(specMap);

        FilledBufferDesc desc;
        desc.dataType  = BASE_DATA_TYPE_CASES[i];
        desc.elemCount = 32;
        desc.padding   = 0;
        desc.fillType  = FillingTypes::INCREMENTED;
        Resource input = createFilledResource(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, desc);

        desc.dataType   = DataTypes::UINT32;
        desc.elemCount  = 1;
        desc.padding    = 0;
        desc.fillType   = FillingTypes::VALUE;
        desc.value      = 1;
        Resource output = createFilledResource(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, desc);

        spec.assembly      = shaderAsm;
        spec.numWorkGroups = tcu::IVec3(1, 1, 1);
        spec.spirvVersion  = SPIRV_VERSION_1_4; // OpPtrEqual, OpPtrNotEqual and OpPtrDiff requires SPIR-V 1.4
        spec.inputs.push_back(input);
        spec.outputs.push_back(output);
        spec.extensions.push_back("VK_KHR_storage_buffer_storage_class");

        notEqualGroup->addChild(new SpvAsmComputeShaderCase(testCtx, testName.c_str(), spec));
    }

    // Not equal - comparsion to null pointers
    for (uint32_t i = 0; i < DE_LENGTH_OF_ARRAY(BASE_DATA_TYPE_CASES); ++i)
    {
        std::string testName = toString(BASE_DATA_TYPE_CASES[i]) + std::string("_null_ptr");

        std::map<std::string, std::string> specMap;
        specMap["baseDecl"]  = getDeclaration(BASE_DATA_TYPE_CASES[i]);
        specMap["baseType"]  = toString(BASE_DATA_TYPE_CASES[i]);
        specMap["alignment"] = std::to_string(getSizeInBytes(BASE_DATA_TYPE_CASES[i]));
        specMap["mainLogic"] =
            "%input_loc_first  = OpUntypedAccessChainKHR %storage_buffer_untyped_ptr     %input_buffer    "
            "%input_data_var %c_uint32_0 %c_uint32_2\n"
            "%are_equal        = OpPtrNotEqual           %bool                           %input_loc_first   "
            "%c_null_untyped_ptr\n";

        std::string memModelOp;
        std::vector<const char *> spvExts;
        std::vector<const char *> spvCaps;
        ComputeShaderSpec spec;
        adjustSpecForUntypedPointers(spec, spvExts, spvCaps);
        adjustSpecForMemoryModel(memModel, spec, memModelOp, spvExts, spvCaps);
        adjustSpecForDataTypes(BASE_DATA_TYPE_CASES[i], spec, spvExts, spvCaps);
        adjustSpecForVariablePointers(spec, spvExts, spvCaps);

        specMap["memModelOp"]                         = memModelOp;
        specMap["extensions"]                         = toString(spvExts);
        specMap["capabilities"]                       = toString(spvCaps);
        const tcu::StringTemplate tempShaderFunctions = tcu::StringTemplate(shaderFunctions.specialize(specMap));

        std::string shaderVariablesStr = shaderVariables.specialize(specMap);
        if (BASE_DATA_TYPE_CASES[i] != DataTypes::UINT32)
        {
            shaderVariablesStr = "%uint32 = OpTypeInt 32 0\n" + shaderVariablesStr;
        }

        const std::string shaderAsm = shaderHeader.specialize(specMap) + shaderAnnotations.specialize(specMap) +
                                      shaderVariablesStr + tempShaderFunctions.specialize(specMap);

        FilledBufferDesc desc;
        desc.dataType  = BASE_DATA_TYPE_CASES[i];
        desc.elemCount = 32;
        desc.padding   = 0;
        desc.fillType  = FillingTypes::INCREMENTED;
        Resource input = createFilledResource(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, desc);

        desc.dataType   = DataTypes::UINT32;
        desc.elemCount  = 1;
        desc.padding    = 0;
        desc.fillType   = FillingTypes::VALUE;
        desc.value      = 1;
        Resource output = createFilledResource(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, desc);

        spec.assembly      = shaderAsm;
        spec.numWorkGroups = tcu::IVec3(1, 1, 1);
        spec.spirvVersion  = SPIRV_VERSION_1_4; // OpPtrEqual, OpPtrNotEqual and OpPtrDiff requires SPIR-V 1.4
        spec.inputs.push_back(input);
        spec.outputs.push_back(output);
        spec.extensions.push_back("VK_KHR_storage_buffer_storage_class");

        notEqualGroup->addChild(new SpvAsmComputeShaderCase(testCtx, testName.c_str(), spec));
    }

    testGroup->addChild(equalGroup.release());
    testGroup->addChild(notEqualGroup.release());
}

void addVariablePtrOpPtrDiffTests(tcu::TestCaseGroup *testGroup, MEMORY_MODEL_TYPE memModel)
{
    tcu::TestContext &testCtx = testGroup->getTestContext();

    tcu::StringTemplate shaderHeader(createShaderHeader("%input_data_var %output_data_var"));

    tcu::StringTemplate shaderAnnotations(createShaderAnnotations(PointerTestCases::OP_PTR_DIFF_VARIABLE_PTR));

    tcu::StringTemplate shaderVariables(createShaderVariables(PointerTestCases::OP_PTR_DIFF_VARIABLE_PTR));

    tcu::StringTemplate shaderFunctions(createShaderMain(PointerTestCases::OP_PTR_DIFF_VARIABLE_PTR));

    // Same types
    for (uint32_t i = 0; i < DE_LENGTH_OF_ARRAY(BASE_DATA_TYPE_CASES); ++i)
    {
        std::string testName = toString(BASE_DATA_TYPE_CASES[i]);

        std::map<std::string, std::string> specMap;
        specMap["baseDecl"]    = getDeclaration(BASE_DATA_TYPE_CASES[i]);
        specMap["baseType"]    = toString(BASE_DATA_TYPE_CASES[i]);
        specMap["threadCount"] = std::to_string(Constants::numThreads);
        specMap["alignment"]   = std::to_string(getSizeInBytes(BASE_DATA_TYPE_CASES[i]));
        specMap["mainLogic"] =
            "%input_loc_first_ptr  = OpUntypedAccessChainKHR    %storage_buffer_untyped_ptr     %input_buffer         "
            "                        %input_data_var            %c_uint32_0 %c_uint32_4\n"
            "%input_loc_second_ptr = OpUntypedAccessChainKHR    %storage_buffer_untyped_ptr     %input_buffer         "
            "                        %input_data_var            %c_uint32_0 %c_uint32_16\n";

        std::string memModelOp;
        std::vector<const char *> spvExts;
        std::vector<const char *> spvCaps;
        ComputeShaderSpec spec;
        adjustSpecForUntypedPointers(spec, spvExts, spvCaps);
        adjustSpecForMemoryModel(memModel, spec, memModelOp, spvExts, spvCaps);
        adjustSpecForDataTypes(BASE_DATA_TYPE_CASES[i], spec, spvExts, spvCaps);
        adjustSpecForVariablePointers(spec, spvExts, spvCaps);

        specMap["memModelOp"]                         = memModelOp;
        specMap["extensions"]                         = toString(spvExts);
        specMap["capabilities"]                       = toString(spvCaps);
        const tcu::StringTemplate tempShaderFunctions = tcu::StringTemplate(shaderFunctions.specialize(specMap));

        std::string shaderVariablesStr = shaderVariables.specialize(specMap);
        if (BASE_DATA_TYPE_CASES[i] != DataTypes::UINT32)
        {
            shaderVariablesStr = "%uint32     = OpTypeInt  32      0\n" + shaderVariablesStr;
        }

        const std::string shaderAsm = shaderHeader.specialize(specMap) + shaderAnnotations.specialize(specMap) +
                                      shaderVariablesStr + tempShaderFunctions.specialize(specMap);

        FilledBufferDesc desc;
        desc.dataType  = BASE_DATA_TYPE_CASES[i];
        desc.elemCount = 32;
        desc.padding   = 0;
        desc.fillType  = FillingTypes::RANDOM;
        desc.seed      = deStringHash(testGroup->getName());
        Resource input = createFilledResource(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, desc);

        desc.dataType   = DataTypes::UINT32;
        desc.elemCount  = 1;
        desc.fillType   = FillingTypes::VALUE;
        desc.value      = 12 * getSizeInBytes(BASE_DATA_TYPE_CASES[i]);
        Resource output = createFilledResource(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, desc);

        spec.assembly      = shaderAsm;
        spec.numWorkGroups = tcu::IVec3(1, 1, 1);
        spec.spirvVersion  = SPIRV_VERSION_1_4;
        spec.inputs.push_back(input);
        spec.outputs.push_back(output);
        spec.extensions.push_back("VK_KHR_storage_buffer_storage_class");

        testGroup->addChild(new SpvAsmComputeShaderCase(testCtx, testName.c_str(), spec));
    }

    // Different types
    for (uint32_t i = 0; i < DE_LENGTH_OF_ARRAY(BASE_DATA_TYPE_CASES); ++i)
    {
        std::string testName =
            toString(BASE_DATA_TYPE_CASES[i]) + std::string(getNameStrForVarPtrs(BASE_DATA_TYPE_CASES[i]));

        std::map<std::string, std::string> specMap;
        specMap["baseDecl"]              = getDeclaration(BASE_DATA_TYPE_CASES[i]);
        specMap["baseType"]              = toString(BASE_DATA_TYPE_CASES[i]);
        specMap["threadCount"]           = std::to_string(Constants::numThreads);
        specMap["secondType"]            = getSecondTypeDefinitionForVarPtrs(BASE_DATA_TYPE_CASES[i]);
        specMap["secondArray"]           = getSecondArrayDefinitionForVarPtrs(BASE_DATA_TYPE_CASES[i]);
        specMap["alignment"]             = std::to_string(getSizeInBytes(BASE_DATA_TYPE_CASES[i]));
        specMap["secondArrayDecoration"] = getSecondArrayDecorationForVarPtrs(BASE_DATA_TYPE_CASES[i]);
        specMap["mainLogic"] =
            "%input_array_loc      = OpUntypedAccessChainKHR %storage_buffer_untyped_ptr %input_buffer "
            "                        %input_data_var %c_uint32_0\n"
            "%input_loc_first_ptr  = OpUntypedAccessChainKHR    %storage_buffer_untyped_ptr %array_first_32  "
            "                        %input_array_loc           %c_uint32_4\n"
            "%input_loc_second_ptr = OpUntypedAccessChainKHR    %storage_buffer_untyped_ptr %array_second_32 "
            "                        %input_array_loc " +
            std::string(getSameByteIndexForVarPtrs(BASE_DATA_TYPE_CASES[i]));

        std::string memModelOp;
        std::vector<const char *> spvExts;
        std::vector<const char *> spvCaps;
        ComputeShaderSpec spec;
        adjustSpecForUntypedPointers(spec, spvExts, spvCaps);
        adjustSpecForMemoryModel(memModel, spec, memModelOp, spvExts, spvCaps);
        adjustSpecForDataTypes(BASE_DATA_TYPE_CASES[i], spec, spvExts, spvCaps);
        adjustSpecForDataTypes(DataTypes::INT16, spec, spvExts, spvCaps);
        adjustSpecForVariablePointers(spec, spvExts, spvCaps);

        specMap["memModelOp"]                         = memModelOp;
        specMap["extensions"]                         = toString(spvExts);
        specMap["capabilities"]                       = toString(spvCaps);
        const tcu::StringTemplate tempShaderFunctions = tcu::StringTemplate(shaderFunctions.specialize(specMap));

        std::string shaderVariablesStr = shaderVariables.specialize(specMap);
        if (BASE_DATA_TYPE_CASES[i] != DataTypes::UINT32)
        {
            shaderVariablesStr = "%uint32     = OpTypeInt  32      0\n" + shaderVariablesStr;
        }

        const std::string shaderAsm = shaderHeader.specialize(specMap) + shaderAnnotations.specialize(specMap) +
                                      shaderVariablesStr + tempShaderFunctions.specialize(specMap);

        FilledBufferDesc desc;
        desc.dataType  = BASE_DATA_TYPE_CASES[i];
        desc.elemCount = 32;
        desc.padding   = 0;
        desc.fillType  = FillingTypes::RANDOM;
        desc.seed      = deStringHash(testGroup->getName());
        Resource input = createFilledResource(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, desc);

        desc.dataType   = DataTypes::UINT32;
        desc.elemCount  = 1;
        desc.fillType   = FillingTypes::VALUE;
        desc.value      = 0 * getSizeInBytes(BASE_DATA_TYPE_CASES[i]);
        Resource output = createFilledResource(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, desc);

        spec.assembly      = shaderAsm;
        spec.numWorkGroups = tcu::IVec3(1, 1, 1);
        spec.spirvVersion  = SPIRV_VERSION_1_4;
        spec.inputs.push_back(input);
        spec.outputs.push_back(output);
        spec.extensions.push_back("VK_KHR_storage_buffer_storage_class");

        testGroup->addChild(new SpvAsmComputeShaderCase(testCtx, testName.c_str(), spec));
    }
}

void addVariablePtrOpFunctionCallTests(tcu::TestCaseGroup *testGroup, MEMORY_MODEL_TYPE memModel)
{
    tcu::TestContext &testCtx = testGroup->getTestContext();

    tcu::StringTemplate shaderHeader(createShaderHeader());

    tcu::StringTemplate shaderAnnotations(createShaderAnnotations(PointerTestCases::OP_FUNCTION_CALL_VARIABLE_PTR));

    tcu::StringTemplate shaderVariables(createShaderVariables(PointerTestCases::OP_FUNCTION_CALL_VARIABLE_PTR));

    std::string functions = createSimpleFunction(PointerTestCases::OP_FUNCTION_CALL_VARIABLE_PTR);
    functions += createShaderMain(PointerTestCases::OP_FUNCTION_CALL_VARIABLE_PTR);

    tcu::StringTemplate shaderFunctions(functions);

    for (uint32_t i = 0; i < DE_LENGTH_OF_ARRAY(BASE_DATA_TYPE_CASES); ++i)
    {
        std::string testName = toString(BASE_DATA_TYPE_CASES[i]);

        std::map<std::string, std::string> specMap;
        specMap["alignment"] = std::to_string(getSizeInBytes(BASE_DATA_TYPE_CASES[i]));
        specMap["baseDecl"]  = getDeclaration(BASE_DATA_TYPE_CASES[i]);
        specMap["baseType"]  = toString(BASE_DATA_TYPE_CASES[i]);

        std::string memModelOp;
        std::vector<const char *> spvExts;
        std::vector<const char *> spvCaps;
        ComputeShaderSpec spec;
        adjustSpecForUntypedPointers(spec, spvExts, spvCaps);
        adjustSpecForMemoryModel(memModel, spec, memModelOp, spvExts, spvCaps);
        adjustSpecForDataTypes(BASE_DATA_TYPE_CASES[i], spec, spvExts, spvCaps);
        adjustSpecForVariablePointers(spec, spvExts, spvCaps);

        specMap["memModelOp"]                         = memModelOp;
        specMap["extensions"]                         = toString(spvExts);
        specMap["capabilities"]                       = toString(spvCaps);
        const tcu::StringTemplate tempShaderFunctions = tcu::StringTemplate(shaderFunctions.specialize(specMap));

        std::string shaderVariablesStr = shaderVariables.specialize(specMap);
        if (BASE_DATA_TYPE_CASES[i] != DataTypes::UINT32)
        {
            shaderVariablesStr = "%uint32     = OpTypeInt  32      0\n" + shaderVariablesStr;
        }

        const std::string shaderAsm = shaderHeader.specialize(specMap) + shaderAnnotations.specialize(specMap) +
                                      shaderVariablesStr + tempShaderFunctions.specialize(specMap);

        FilledBufferDesc desc;
        desc.dataType  = BASE_DATA_TYPE_CASES[i];
        desc.elemCount = 32;
        desc.padding   = 0;
        desc.fillType  = FillingTypes::INCREMENTED;
        Resource input = createFilledResource(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, desc);

        desc.elemCount  = 1;
        desc.fillType   = FillingTypes::VALUE;
        desc.value      = 8;
        Resource output = createFilledResource(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, desc);

        spec.assembly      = shaderAsm;
        spec.numWorkGroups = tcu::IVec3(1, 1, 1);
        spec.inputs.push_back(input);
        spec.outputs.push_back(output);
        spec.extensions.push_back("VK_KHR_storage_buffer_storage_class");

        testGroup->addChild(new SpvAsmComputeShaderCase(testCtx, testName.c_str(), spec));
    }
}

void addPhysicalStorageOpFunctionCallTests(tcu::TestCaseGroup *testGroup, MEMORY_MODEL_TYPE memModel)
{
    tcu::TestContext &testCtx = testGroup->getTestContext();

    tcu::StringTemplate shaderHeader(createShaderHeader());

    tcu::StringTemplate shaderAnnotations(createShaderAnnotations(PointerTestCases::OP_FUNCTION_CALL_PHYSICAL_STORAGE));

    tcu::StringTemplate shaderVariables(createShaderVariables(PointerTestCases::OP_FUNCTION_CALL_PHYSICAL_STORAGE));

    std::string functions = createSimpleFunction(PointerTestCases::OP_FUNCTION_CALL_PHYSICAL_STORAGE);
    functions += createShaderMain(PointerTestCases::OP_FUNCTION_CALL_PHYSICAL_STORAGE);

    tcu::StringTemplate shaderFunctions(functions);

    for (uint32_t i = 0; i < DE_LENGTH_OF_ARRAY(BASE_DATA_TYPE_CASES); ++i)
    {
        std::string testName = toString(BASE_DATA_TYPE_CASES[i]);

        std::map<std::string, std::string> specMap;
        specMap["baseDecl"]  = getDeclaration(BASE_DATA_TYPE_CASES[i]);
        specMap["baseType"]  = toString(BASE_DATA_TYPE_CASES[i]);
        specMap["alignment"] = std::to_string(getSizeInBytes(BASE_DATA_TYPE_CASES[i]));

        std::string memModelOp;
        std::vector<const char *> spvExts;
        std::vector<const char *> spvCaps;
        ComputeShaderSpec spec;
        adjustSpecForUntypedPointers(spec, spvExts, spvCaps);
        adjustSpecForMemoryModel(memModel, spec, memModelOp, spvExts, spvCaps);
        adjustSpecForDataTypes(BASE_DATA_TYPE_CASES[i], spec, spvExts, spvCaps);
        adjustSpecForPhysicalStorageBuffer(memModel, spec, memModelOp, spvExts, spvCaps);

        specMap["memModelOp"]                         = memModelOp;
        specMap["extensions"]                         = toString(spvExts);
        specMap["capabilities"]                       = toString(spvCaps);
        const tcu::StringTemplate tempShaderFunctions = tcu::StringTemplate(shaderFunctions.specialize(specMap));

        std::string shaderVariablesStr = shaderVariables.specialize(specMap);
        if (BASE_DATA_TYPE_CASES[i] != DataTypes::UINT32)
        {
            shaderVariablesStr = "%uint32     = OpTypeInt  32      0\n" + shaderVariablesStr;
        }

        const std::string shaderAsm = shaderHeader.specialize(specMap) + shaderAnnotations.specialize(specMap) +
                                      shaderVariablesStr + tempShaderFunctions.specialize(specMap);

        FilledBufferDesc desc;
        desc.dataType  = BASE_DATA_TYPE_CASES[i];
        desc.elemCount = 1;
        desc.padding   = 0;
        desc.fillType  = FillingTypes::VALUE;
        desc.value     = 1.0;

        Resource input  = createFilledResource(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, desc);
        Resource output = createFilledResource(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, desc);

        spec.assembly              = shaderAsm;
        spec.numWorkGroups         = tcu::IVec3(1, 1, 1);
        spec.usesPhysStorageBuffer = true;
        spec.inputs.push_back(input);
        spec.outputs.push_back(output);
        spec.extensions.push_back("VK_KHR_storage_buffer_storage_class");

        testGroup->addChild(new SpvAsmComputeShaderCase(testCtx, testName.c_str(), spec));
    }
}

void addVariablePtrOpPtrAccessChain(tcu::TestCaseGroup *testGroup, MEMORY_MODEL_TYPE memModel)
{
    tcu::TestContext &testCtx = testGroup->getTestContext();

    tcu::StringTemplate shaderHeader(createShaderHeader());

    tcu::StringTemplate shaderAnnotations(createShaderAnnotations(PointerTestCases::OP_PTR_ACCESS_CHAIN_VARIABLE_PTR));

    tcu::StringTemplate shaderVariables(createShaderVariables(PointerTestCases::OP_PTR_ACCESS_CHAIN_VARIABLE_PTR));

    tcu::StringTemplate shaderFunctions(createShaderMain(PointerTestCases::OP_PTR_ACCESS_CHAIN_VARIABLE_PTR));

    for (uint32_t i = 0; i < DE_LENGTH_OF_ARRAY(BASE_DATA_TYPE_CASES); ++i)
    {
        std::string testName = toString(BASE_DATA_TYPE_CASES[i]);

        std::map<std::string, std::string> specMap;
        specMap["baseDecl"]    = getDeclaration(BASE_DATA_TYPE_CASES[i]);
        specMap["baseType"]    = toString(BASE_DATA_TYPE_CASES[i]);
        specMap["threadCount"] = std::to_string(Constants::numThreads);
        specMap["alignment"]   = std::to_string(getSizeInBytes(BASE_DATA_TYPE_CASES[i]));

        std::string memModelOp;
        std::vector<const char *> spvExts;
        std::vector<const char *> spvCaps;
        ComputeShaderSpec spec;
        adjustSpecForUntypedPointers(spec, spvExts, spvCaps);
        adjustSpecForMemoryModel(memModel, spec, memModelOp, spvExts, spvCaps);
        adjustSpecForDataTypes(BASE_DATA_TYPE_CASES[i], spec, spvExts, spvCaps);
        adjustSpecForVariablePointers(spec, spvExts, spvCaps);

        specMap["memModelOp"]                         = memModelOp;
        specMap["extensions"]                         = toString(spvExts);
        specMap["capabilities"]                       = toString(spvCaps);
        const tcu::StringTemplate tempShaderFunctions = tcu::StringTemplate(shaderFunctions.specialize(specMap));

        std::string shaderVariablesStr = shaderVariables.specialize(specMap);
        if (BASE_DATA_TYPE_CASES[i] != DataTypes::UINT32)
        {
            shaderVariablesStr = "%uint32     = OpTypeInt  32      0\n" + shaderVariablesStr;
        }

        const std::string shaderAsm = shaderHeader.specialize(specMap) + shaderAnnotations.specialize(specMap) +
                                      shaderVariablesStr + tempShaderFunctions.specialize(specMap);

        FilledBufferDesc desc;
        desc.dataType  = BASE_DATA_TYPE_CASES[i];
        desc.elemCount = Constants::numThreads;
        desc.padding   = 0;
        desc.fillType  = FillingTypes::RANDOM;
        desc.seed      = deStringHash(testGroup->getName());

        Resource input  = createFilledResource(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, desc);
        Resource output = createFilledResource(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, desc);

        spec.assembly      = shaderAsm;
        spec.numWorkGroups = tcu::IVec3(Constants::numThreads, 1, 1);
        spec.inputs.push_back(input);
        spec.outputs.push_back(output);
        spec.extensions.push_back("VK_KHR_storage_buffer_storage_class");

        testGroup->addChild(new SpvAsmComputeShaderCase(testCtx, testName.c_str(), spec));
    }
}

void addPhysicalStorageOpPtrAccessChainTests(tcu::TestCaseGroup *testGroup, MEMORY_MODEL_TYPE memModel)
{
    tcu::TestContext &testCtx = testGroup->getTestContext();

    tcu::StringTemplate shaderHeader(createShaderHeader());

    tcu::StringTemplate shaderAnnotations(
        createShaderAnnotations(PointerTestCases::OP_PTR_ACCESS_CHAIN_PHYSICAL_STORAGE));

    tcu::StringTemplate shaderVariables(createShaderVariables(PointerTestCases::OP_PTR_ACCESS_CHAIN_PHYSICAL_STORAGE));

    tcu::StringTemplate shaderFunctions(createShaderMain(PointerTestCases::OP_PTR_ACCESS_CHAIN_PHYSICAL_STORAGE));

    for (uint32_t i = 0; i < DE_LENGTH_OF_ARRAY(BASE_DATA_TYPE_CASES); ++i)
    {
        std::string testName = toString(BASE_DATA_TYPE_CASES[i]);

        std::map<std::string, std::string> specMap;
        specMap["baseDecl"]    = getDeclaration(BASE_DATA_TYPE_CASES[i]);
        specMap["baseType"]    = toString(BASE_DATA_TYPE_CASES[i]);
        specMap["threadCount"] = std::to_string(Constants::numThreads);
        specMap["alignment"]   = std::to_string(getSizeInBytes(BASE_DATA_TYPE_CASES[i]));

        std::string memModelOp;
        std::vector<const char *> spvExts;
        std::vector<const char *> spvCaps;
        ComputeShaderSpec spec;
        adjustSpecForUntypedPointers(spec, spvExts, spvCaps);
        adjustSpecForMemoryModel(memModel, spec, memModelOp, spvExts, spvCaps);
        adjustSpecForDataTypes(BASE_DATA_TYPE_CASES[i], spec, spvExts, spvCaps);
        adjustSpecForPhysicalStorageBuffer(memModel, spec, memModelOp, spvExts, spvCaps);

        specMap["memModelOp"]                         = memModelOp;
        specMap["extensions"]                         = toString(spvExts);
        specMap["capabilities"]                       = toString(spvCaps);
        const tcu::StringTemplate tempShaderFunctions = tcu::StringTemplate(shaderFunctions.specialize(specMap));

        std::string shaderVariablesStr = shaderVariables.specialize(specMap);
        if (BASE_DATA_TYPE_CASES[i] != DataTypes::UINT32)
        {
            shaderVariablesStr = "%uint32     = OpTypeInt  32      0\n" + shaderVariablesStr;
        }

        const std::string shaderAsm = shaderHeader.specialize(specMap) + shaderAnnotations.specialize(specMap) +
                                      shaderVariablesStr + tempShaderFunctions.specialize(specMap);

        FilledBufferDesc desc;
        desc.dataType  = BASE_DATA_TYPE_CASES[i];
        desc.elemCount = 1;
        desc.padding   = 0;
        desc.fillType  = FillingTypes::RANDOM;
        desc.seed      = deStringHash(testGroup->getName());

        Resource input  = createFilledResource(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, desc);
        Resource output = createFilledResource(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, desc);

        spec.assembly              = shaderAsm;
        spec.numWorkGroups         = tcu::IVec3(1, 1, 1);
        spec.usesPhysStorageBuffer = true;
        spec.inputs.push_back(input);
        spec.outputs.push_back(output);
        spec.extensions.push_back("VK_KHR_storage_buffer_storage_class");

        testGroup->addChild(new SpvAsmComputeShaderCase(testCtx, testName.c_str(), spec));
    }
}

void addVariablePtrFunctionVariableTests(tcu::TestCaseGroup *testGroup, MEMORY_MODEL_TYPE memModel)
{
    tcu::TestContext &testCtx = testGroup->getTestContext();

    tcu::StringTemplate shaderHeader(createShaderHeader());

    tcu::StringTemplate shaderAnnotations(createShaderAnnotations(PointerTestCases::FUNCTION_VARIABLE_VARIABLE_PTR));

    tcu::StringTemplate shaderVariables(createShaderVariables(PointerTestCases::FUNCTION_VARIABLE_VARIABLE_PTR));

    tcu::StringTemplate shaderFunctions(createShaderMain(PointerTestCases::FUNCTION_VARIABLE_VARIABLE_PTR));

    for (uint32_t i = 0; i < DE_LENGTH_OF_ARRAY(BASE_DATA_TYPE_CASES); ++i)
    {
        std::string testName = toString(BASE_DATA_TYPE_CASES[i]);

        std::map<std::string, std::string> specMap;
        specMap["baseDecl"] = getDeclaration(BASE_DATA_TYPE_CASES[i]);
        specMap["baseType"] = toString(BASE_DATA_TYPE_CASES[i]);

        std::string memModelOp;
        std::vector<const char *> spvExts;
        std::vector<const char *> spvCaps;
        ComputeShaderSpec spec;
        adjustSpecForUntypedPointers(spec, spvExts, spvCaps);
        adjustSpecForMemoryModel(memModel, spec, memModelOp, spvExts, spvCaps);
        adjustSpecForDataTypes(BASE_DATA_TYPE_CASES[i], spec, spvExts, spvCaps);
        adjustSpecForVariablePointers(spec, spvExts, spvCaps);

        specMap["memModelOp"]                         = memModelOp;
        specMap["extensions"]                         = toString(spvExts);
        specMap["capabilities"]                       = toString(spvCaps);
        const tcu::StringTemplate tempShaderFunctions = tcu::StringTemplate(shaderFunctions.specialize(specMap));

        std::string shaderVariablesStr = shaderVariables.specialize(specMap);
        if (BASE_DATA_TYPE_CASES[i] != DataTypes::UINT32)
        {
            shaderVariablesStr = "%uint32     = OpTypeInt  32      0\n" + shaderVariablesStr;
        }

        const std::string shaderAsm = shaderHeader.specialize(specMap) + shaderAnnotations.specialize(specMap) +
                                      shaderVariablesStr + tempShaderFunctions.specialize(specMap);

        FilledBufferDesc desc;
        desc.dataType  = BASE_DATA_TYPE_CASES[i];
        desc.elemCount = 1;
        desc.padding   = 0;
        desc.fillType  = FillingTypes::VALUE;
        desc.value     = 1.0;

        Resource input0 = createFilledResource(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, desc);
        Resource output = createFilledResource(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, desc);
        desc.value      = 0.0;
        Resource input1 = createFilledResource(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, desc);

        desc.dataType      = DataTypes::UINT32;
        desc.value         = 1u;
        BufferSp pushConst = createFilledBuffer(desc);

        spec.assembly      = shaderAsm;
        spec.numWorkGroups = tcu::IVec3(1, 1, 1);
        spec.pushConstants = pushConst;
        spec.inputs.push_back(input0);
        spec.inputs.push_back(input1);
        spec.outputs.push_back(output);
        spec.extensions.push_back("VK_KHR_storage_buffer_storage_class");

        testGroup->addChild(new SpvAsmComputeShaderCase(testCtx, testName.c_str(), spec));
    }
}

void addVariablePtrPrivateVariableTests(tcu::TestCaseGroup *testGroup, MEMORY_MODEL_TYPE memModel)
{
    tcu::TestContext &testCtx = testGroup->getTestContext();

    tcu::StringTemplate shaderHeader(createShaderHeader());

    tcu::StringTemplate shaderAnnotations(createShaderAnnotations(PointerTestCases::PRIVATE_VARIABLE_VARIABLE_PTR));

    tcu::StringTemplate shaderVariables(createShaderVariables(PointerTestCases::PRIVATE_VARIABLE_VARIABLE_PTR));

    tcu::StringTemplate shaderFunctions(createShaderMain(PointerTestCases::PRIVATE_VARIABLE_VARIABLE_PTR));

    for (uint32_t i = 0; i < DE_LENGTH_OF_ARRAY(BASE_DATA_TYPE_CASES); ++i)
    {
        std::string testName = toString(BASE_DATA_TYPE_CASES[i]);

        std::map<std::string, std::string> specMap;
        specMap["baseDecl"] = getDeclaration(BASE_DATA_TYPE_CASES[i]);
        specMap["baseType"] = toString(BASE_DATA_TYPE_CASES[i]);

        std::string memModelOp;
        std::vector<const char *> spvExts;
        std::vector<const char *> spvCaps;
        ComputeShaderSpec spec;
        adjustSpecForUntypedPointers(spec, spvExts, spvCaps);
        adjustSpecForMemoryModel(memModel, spec, memModelOp, spvExts, spvCaps);
        adjustSpecForDataTypes(BASE_DATA_TYPE_CASES[i], spec, spvExts, spvCaps);
        adjustSpecForVariablePointers(spec, spvExts, spvCaps);

        specMap["memModelOp"]                         = memModelOp;
        specMap["extensions"]                         = toString(spvExts);
        specMap["capabilities"]                       = toString(spvCaps);
        const tcu::StringTemplate tempShaderFunctions = tcu::StringTemplate(shaderFunctions.specialize(specMap));

        std::string shaderVariablesStr = shaderVariables.specialize(specMap);
        if (BASE_DATA_TYPE_CASES[i] != DataTypes::UINT32)
        {
            shaderVariablesStr = "%uint32     = OpTypeInt  32      0\n" + shaderVariablesStr;
        }

        const std::string shaderAsm = shaderHeader.specialize(specMap) + shaderAnnotations.specialize(specMap) +
                                      shaderVariablesStr + tempShaderFunctions.specialize(specMap);

        FilledBufferDesc desc;
        desc.dataType  = BASE_DATA_TYPE_CASES[i];
        desc.elemCount = 1;
        desc.padding   = 0;
        desc.fillType  = FillingTypes::VALUE;
        desc.value     = 1.0;

        Resource input0 = createFilledResource(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, desc);
        Resource output = createFilledResource(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, desc);
        desc.value      = 0.0;
        Resource input1 = createFilledResource(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, desc);

        desc.dataType      = DataTypes::UINT32;
        desc.value         = 1u;
        BufferSp pushConst = createFilledBuffer(desc);

        spec.assembly      = shaderAsm;
        spec.numWorkGroups = tcu::IVec3(1, 1, 1);
        spec.pushConstants = pushConst;
        spec.inputs.push_back(input0);
        spec.inputs.push_back(input1);
        spec.outputs.push_back(output);
        spec.extensions.push_back("VK_KHR_storage_buffer_storage_class");

        testGroup->addChild(new SpvAsmComputeShaderCase(testCtx, testName.c_str(), spec));
    }
}

void addStructAsTypeTests(tcu::TestCaseGroup *testGroup, MEMORY_MODEL_TYPE memModel)
{
    tcu::TestContext &testCtx = testGroup->getTestContext();

    tcu::StringTemplate shaderHeader(createShaderHeader());

    tcu::StringTemplate shaderAnnotations(createShaderAnnotations(TypePunningTestCases::CUSTOM_STRUCT_TYPE));

    tcu::StringTemplate shaderVariables(createShaderVariables(TypePunningTestCases::CUSTOM_STRUCT_TYPE));

    tcu::StringTemplate shaderFunctions(createShaderMain(TypePunningTestCases::CUSTOM_STRUCT_TYPE));

    {
        std::map<std::string, std::string> specMap;
        specMap["inputOffsets"] = "OpMemberDecorate %input_buffer 1 Offset 8\n";
        specMap["baseTypes"]    = "%int32         = OpTypeInt   32 1\n"
                                  "%float32       = OpTypeFloat 32\n"
                                  "%vec2_uint32   = OpTypeVector %uint32  2\n"
                                  "%vec2_float32  = OpTypeVector %float32 2\n"
                                  "%vec4_int32    = OpTypeVector %int32   4\n";
        specMap["inputLayout"]  = "%vec2_uint32 %vec2_float32";
        specMap["outputLayout"] = "%vec4_int32";

        std::string memModelOp;
        std::vector<const char *> spvExts;
        std::vector<const char *> spvCaps;
        ComputeShaderSpec spec;
        adjustSpecForUntypedPointers(spec, spvExts, spvCaps);
        adjustSpecForMemoryModel(memModel, spec, memModelOp, spvExts, spvCaps);

        specMap["memModelOp"]   = memModelOp;
        specMap["extensions"]   = toString(spvExts);
        specMap["capabilities"] = toString(spvCaps);

        const std::string shaderAsm = shaderHeader.specialize(specMap) + shaderAnnotations.specialize(specMap) +
                                      shaderVariables.specialize(specMap) + shaderFunctions.specialize(specMap);

        struct InputStruct
        {
            tcu::UVec2 vec2_uint32;
            tcu::Vec2 vec2_float32;
        } inputStruct;

        inputStruct.vec2_uint32[0]  = 0u;
        inputStruct.vec2_uint32[1]  = 1u;
        inputStruct.vec2_float32[0] = 1.0f;
        inputStruct.vec2_float32[1] = 1.0f;

        struct OutputStruct
        {
            tcu::UVec4 vec4_int32;
        } outputStruct;

        outputStruct.vec4_int32[0] = 0;
        outputStruct.vec4_int32[1] = 1;
        outputStruct.vec4_int32[2] = tcu::Float32(1.0f).bits();
        outputStruct.vec4_int32[3] = tcu::Float32(1.0f).bits();

        Resource inputResource =
            Resource(BufferSp(new Buffer<InputStruct>(std::vector<InputStruct>(1, inputStruct), 0)),
                     vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
        Resource outputResource =
            Resource(BufferSp(new Buffer<OutputStruct>(std::vector<OutputStruct>(1, outputStruct), 0)),
                     vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);

        spec.assembly      = shaderAsm;
        spec.numWorkGroups = tcu::IVec3(Constants::numThreads, 1, 1);
        spec.inputs.push_back(inputResource);
        spec.outputs.push_back(outputResource);
        spec.extensions.push_back("VK_KHR_storage_buffer_storage_class");

        testGroup->addChild(new SpvAsmComputeShaderCase(testCtx, "vec2_uint32_vec2_float32_to_vec4_int32", spec));
    }

    {
        std::map<std::string, std::string> specMap;
        specMap["outputOffsets"] = "OpMemberDecorate %output_buffer 1 Offset 1\n"
                                   "OpMemberDecorate %output_buffer 2 Offset 2\n"
                                   "OpMemberDecorate %output_buffer 3 Offset 3\n";
        specMap["baseTypes"]     = "%uint8         = OpTypeInt   8 0\n";
        specMap["inputLayout"]   = "%uint32";
        specMap["outputLayout"]  = "%uint8 %uint8 %uint8 %uint8";

        std::string memModelOp;
        std::vector<const char *> spvExts;
        std::vector<const char *> spvCaps;
        ComputeShaderSpec spec;
        adjustSpecForUntypedPointers(spec, spvExts, spvCaps);
        adjustSpecForMemoryModel(memModel, spec, memModelOp, spvExts, spvCaps);
        adjustSpecForDataTypes(DataTypes::UINT8, spec, spvExts, spvCaps);
        adjustSpecForSmallContainerType(ContainerTypes::STORAGE_BUFFER, DataTypes::UINT8, spec, spvExts, spvCaps);

        specMap["memModelOp"]   = memModelOp;
        specMap["extensions"]   = toString(spvExts);
        specMap["capabilities"] = toString(spvCaps);

        const std::string shaderAsm = shaderHeader.specialize(specMap) + shaderAnnotations.specialize(specMap) +
                                      shaderVariables.specialize(specMap) + shaderFunctions.specialize(specMap);

        struct InputStruct
        {
            uint32_t uint32;
        } inputStruct;

        inputStruct.uint32 = 0b00000001000000010000000100000001;

        struct OutputStruct
        {
            uint8_t uint8_0;
            uint8_t uint8_1;
            uint8_t uint8_2;
            uint8_t uint8_3;
        } outputStruct;

        outputStruct.uint8_0 = 1;
        outputStruct.uint8_1 = 1;
        outputStruct.uint8_2 = 1;
        outputStruct.uint8_3 = 1;

        Resource inputResource =
            Resource(BufferSp(new Buffer<InputStruct>(std::vector<InputStruct>(1, inputStruct), 0)),
                     vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
        Resource outputResource =
            Resource(BufferSp(new Buffer<OutputStruct>(std::vector<OutputStruct>(1, outputStruct), 0)),
                     vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);

        spec.assembly      = shaderAsm;
        spec.numWorkGroups = tcu::IVec3(Constants::numThreads, 1, 1);
        spec.inputs.push_back(inputResource);
        spec.outputs.push_back(outputResource);
        spec.extensions.push_back("VK_KHR_storage_buffer_storage_class");

        testGroup->addChild(new SpvAsmComputeShaderCase(testCtx, "uint32_to_uint8_uint8_uint8_uint8", spec));
    }

    {
        std::map<std::string, std::string> specMap;
        specMap["inputOffsets"]  = "OpMemberDecorate %input_buffer 1 Offset 16\n";
        specMap["outputOffsets"] = "OpMemberDecorate %output_buffer 1 Offset 2\n"
                                   "OpMemberDecorate %output_buffer 2 Offset 6\n"
                                   "OpMemberDecorate %output_buffer 3 Offset 8\n"
                                   "OpMemberDecorate %output_buffer 4 Offset 12\n";
        specMap["baseTypes"]     = "%int32         = OpTypeInt   32 1\n"
                                   "%float16       = OpTypeFloat 16\n"
                                   "%vec2_float16  = OpTypeVector %float16 2\n"
                                   "%vec4_float16  = OpTypeVector %float16 4\n"
                                   "%vec2_int32    = OpTypeVector %int32   2\n";
        specMap["inputLayout"]   = "%vec4_float16 %vec2_int32";
        specMap["outputLayout"]  = "%float16 %vec2_float16 %float16 %int32 %int32";

        std::string memModelOp;
        std::vector<const char *> spvExts;
        std::vector<const char *> spvCaps;
        ComputeShaderSpec spec;
        adjustSpecForUntypedPointers(spec, spvExts, spvCaps);
        adjustSpecForMemoryModel(memModel, spec, memModelOp, spvExts, spvCaps);
        adjustSpecForDataTypes(DataTypes::FLOAT16, spec, spvExts, spvCaps);
        adjustSpecForSmallContainerType(ContainerTypes::STORAGE_BUFFER, DataTypes::FLOAT16, spec, spvExts, spvCaps);

        specMap["memModelOp"]   = memModelOp;
        specMap["extensions"]   = toString(spvExts);
        specMap["capabilities"] = toString(spvCaps);

        const std::string shaderAsm = shaderHeader.specialize(specMap) + shaderAnnotations.specialize(specMap) +
                                      shaderVariables.specialize(specMap) + shaderFunctions.specialize(specMap);

        struct InputStruct
        {
            tcu::F16Vec4 vec4_float16;
            tcu::IVec2 vec2_int32;
        } inputStruct;

        inputStruct.vec4_float16[0] = tcu::Float16(1.0f);
        inputStruct.vec4_float16[1] = tcu::Float16(-100.0f);
        inputStruct.vec4_float16[2] = tcu::Float16(17.312f);
        inputStruct.vec4_float16[3] = tcu::Float16(-1.11f);
        inputStruct.vec2_int32[0]   = 1;
        inputStruct.vec2_int32[1]   = -1;

        struct OutputStruct
        {
            tcu::Float16 float16_0;
            tcu::F16Vec2 vec2_float16;
            tcu::Float16 float16_1;
            int32_t int32_0;
            int32_t int32_1;
        } outputStruct;

        outputStruct.float16_0       = tcu::Float16(1.0f);
        outputStruct.vec2_float16[0] = tcu::Float16(-100.0f);
        outputStruct.vec2_float16[1] = tcu::Float16(17.312f);
        outputStruct.float16_1       = tcu::Float16(-1.11f);
        outputStruct.int32_0         = 1;
        outputStruct.int32_1         = -1;

        Resource inputResource =
            Resource(BufferSp(new Buffer<InputStruct>(std::vector<InputStruct>(1, inputStruct), 0)),
                     vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
        Resource outputResource =
            Resource(BufferSp(new Buffer<OutputStruct>(std::vector<OutputStruct>(1, outputStruct), 0)),
                     vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);

        spec.assembly      = shaderAsm;
        spec.numWorkGroups = tcu::IVec3(Constants::numThreads, 1, 1);
        spec.inputs.push_back(inputResource);
        spec.outputs.push_back(outputResource);
        spec.extensions.push_back("VK_KHR_storage_buffer_storage_class");

        testGroup->addChild(new SpvAsmComputeShaderCase(
            testCtx, "vec4_float16_vec2_int32_to_float16_vec2_float16_float16_int32_int32", spec));
    }

    {
        std::map<std::string, std::string> specMap;
        specMap["inputOffsets"]  = "OpMemberDecorate %int32_struct 0 Offset 0\n"
                                   "OpMemberDecorate %int32_struct 1 Offset 4\n"
                                   "OpMemberDecorate %int32_struct 2 Offset 8\n"
                                   "OpMemberDecorate %int32_struct 3 Offset 16\n";
        specMap["outputOffsets"] = "OpMemberDecorate %output_buffer 1 Offset 8\n";
        specMap["baseTypes"]     = "%int32         = OpTypeInt   32 1\n"
                                   "%vec2_int32    = OpTypeVector %int32   2\n"
                                   "%int32_struct  = OpTypeStruct %int32 %int32 %int32 %int32";
        specMap["inputLayout"]   = "%int32_struct";
        specMap["outputLayout"]  = "%vec2_int32 %vec2_int32";

        std::string memModelOp;
        std::vector<const char *> spvExts;
        std::vector<const char *> spvCaps;
        ComputeShaderSpec spec;
        adjustSpecForUntypedPointers(spec, spvExts, spvCaps);
        adjustSpecForMemoryModel(memModel, spec, memModelOp, spvExts, spvCaps);

        specMap["memModelOp"]   = memModelOp;
        specMap["extensions"]   = toString(spvExts);
        specMap["capabilities"] = toString(spvCaps);

        const std::string shaderAsm = shaderHeader.specialize(specMap) + shaderAnnotations.specialize(specMap) +
                                      shaderVariables.specialize(specMap) + shaderFunctions.specialize(specMap);

        struct NestedStruct
        {
            int32_t int32_0;
            int32_t int32_1;
            int32_t int32_2;
            int32_t int32_3;
        };

        struct InputStruct
        {
            NestedStruct nested;
        } inputStruct;

        inputStruct.nested.int32_0 = 0;
        inputStruct.nested.int32_1 = 1;
        inputStruct.nested.int32_2 = -1;
        inputStruct.nested.int32_3 = INT32_MAX;

        struct OutputStruct
        {
            tcu::IVec2 vec2_int32_0;
            tcu::IVec2 vec2_int32_1;
        } outputStruct;

        outputStruct.vec2_int32_0[0] = 0;
        outputStruct.vec2_int32_0[1] = 1;
        outputStruct.vec2_int32_1[0] = -1;
        outputStruct.vec2_int32_1[1] = INT32_MAX;

        Resource inputResource =
            Resource(BufferSp(new Buffer<InputStruct>(std::vector<InputStruct>(1, inputStruct), 0)),
                     vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
        Resource outputResource =
            Resource(BufferSp(new Buffer<OutputStruct>(std::vector<OutputStruct>(1, outputStruct), 0)),
                     vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);

        spec.assembly      = shaderAsm;
        spec.numWorkGroups = tcu::IVec3(Constants::numThreads, 1, 1);
        spec.inputs.push_back(inputResource);
        spec.outputs.push_back(outputResource);
        spec.extensions.push_back("VK_KHR_storage_buffer_storage_class");

        testGroup->addChild(new SpvAsmComputeShaderCase(
            testCtx, "nested_struct_int32_int32_int32_int32_to_vec2_int32_vec2_int32", spec));
    }

    {
        std::map<std::string, std::string> specMap;
        specMap["inputOffsets"]  = "OpMemberDecorate %input_buffer 1 Offset 8\n"
                                   "OpMemberDecorate %input_buffer 2 Offset 16\n";
        specMap["outputOffsets"] = "OpMemberDecorate %vec4_int64_struct 0 Offset 0\n";
        specMap["baseTypes"]     = "%int64             = OpTypeInt    64 1\n"
                                   "%uint64            = OpTypeInt   64 0\n"
                                   "%float64           = OpTypeFloat 64\n"
                                   "%vec2_float64      = OpTypeVector %float64 2\n"
                                   "%vec4_int64        = OpTypeVector %int64   4\n"
                                   "%vec4_int64_struct = OpTypeStruct %vec4_int64";
        specMap["inputLayout"]   = "%int64 %uint64 %vec2_float64";
        specMap["outputLayout"]  = "%vec4_int64_struct";

        std::string memModelOp;
        std::vector<const char *> spvExts;
        std::vector<const char *> spvCaps;
        ComputeShaderSpec spec;
        adjustSpecForUntypedPointers(spec, spvExts, spvCaps);
        adjustSpecForMemoryModel(memModel, spec, memModelOp, spvExts, spvCaps);
        adjustSpecForDataTypes(DataTypes::INT64, spec, spvExts, spvCaps);
        adjustSpecForDataTypes(DataTypes::FLOAT64, spec, spvExts, spvCaps);

        specMap["memModelOp"]   = memModelOp;
        specMap["extensions"]   = toString(spvExts);
        specMap["capabilities"] = toString(spvCaps);

        const std::string shaderAsm = shaderHeader.specialize(specMap) + shaderAnnotations.specialize(specMap) +
                                      shaderVariables.specialize(specMap) + shaderFunctions.specialize(specMap);

        struct NestedStruct
        {
            tcu::I64Vec4 vec4_int64;
        };

        struct InputStruct
        {
            int64_t int64;
            uint64_t uint64;
            tcu::DVec2 vec2_float64;
        } inputStruct;

        inputStruct.int64           = INT64_MAX;
        inputStruct.uint64          = 1;
        inputStruct.vec2_float64[0] = 0.0f;
        inputStruct.vec2_float64[1] = -112.0f;

        struct OutputStruct
        {
            NestedStruct nested;
        } outputStruct;

        deMemcpy(&outputStruct, &inputStruct, sizeof(inputStruct));

        Resource inputResource =
            Resource(BufferSp(new Buffer<InputStruct>(std::vector<InputStruct>(1, inputStruct), 0)),
                     vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
        Resource outputResource =
            Resource(BufferSp(new Buffer<OutputStruct>(std::vector<OutputStruct>(1, outputStruct), 0)),
                     vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);

        spec.assembly      = shaderAsm;
        spec.numWorkGroups = tcu::IVec3(Constants::numThreads, 1, 1);
        spec.inputs.push_back(inputResource);
        spec.outputs.push_back(outputResource);
        spec.extensions.push_back("VK_KHR_storage_buffer_storage_class");

        testGroup->addChild(
            new SpvAsmComputeShaderCase(testCtx, "int64_uint64_vec2_float64_to_nested_struct_vec4_int64", spec));
    }

    {
        std::map<std::string, std::string> specMap;
        specMap["outputOffsets"] = "OpMemberDecorate %uint16_struct 0 Offset 0\n"
                                   "OpMemberDecorate %uint16_struct 1 Offset 2\n"
                                   "OpMemberDecorate %uint16_struct 2 Offset 4\n"
                                   "OpMemberDecorate %uint16_struct 3 Offset 6\n";
        specMap["baseTypes"]     = "%uint16        = OpTypeInt   16 0\n"
                                   "%uint64        = OpTypeInt   64 0\n"
                                   "%uint16_struct = OpTypeStruct %uint16 %uint16 %uint16 %uint16";
        specMap["inputLayout"]   = "%uint64";
        specMap["outputLayout"]  = "%uint16_struct";

        std::string memModelOp;
        std::vector<const char *> spvExts;
        std::vector<const char *> spvCaps;
        ComputeShaderSpec spec;
        adjustSpecForUntypedPointers(spec, spvExts, spvCaps);
        adjustSpecForMemoryModel(memModel, spec, memModelOp, spvExts, spvCaps);
        adjustSpecForDataTypes(DataTypes::INT64, spec, spvExts, spvCaps);
        adjustSpecForDataTypes(DataTypes::INT16, spec, spvExts, spvCaps);
        adjustSpecForSmallContainerType(ContainerTypes::STORAGE_BUFFER, DataTypes::UINT16, spec, spvExts, spvCaps);

        specMap["memModelOp"]   = memModelOp;
        specMap["extensions"]   = toString(spvExts);
        specMap["capabilities"] = toString(spvCaps);

        const std::string shaderAsm = shaderHeader.specialize(specMap) + shaderAnnotations.specialize(specMap) +
                                      shaderVariables.specialize(specMap) + shaderFunctions.specialize(specMap);

        struct NestedStruct
        {
            uint16_t uint16_0;
            uint16_t uint16_1;
            uint16_t uint16_2;
            uint16_t uint16_3;
        };

        struct InputStruct
        {
            uint64_t uint64;
        } inputStruct;

        inputStruct.uint64 = 0b0000000000000001000000000000000100000000000000010000000000000001;

        struct OutputStruct
        {
            NestedStruct nested;
        } outputStruct;

        outputStruct.nested.uint16_0 = 1;
        outputStruct.nested.uint16_1 = 1;
        outputStruct.nested.uint16_2 = 1;
        outputStruct.nested.uint16_3 = 1;

        Resource inputResource =
            Resource(BufferSp(new Buffer<InputStruct>(std::vector<InputStruct>(1, inputStruct), 0)),
                     vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
        Resource outputResource =
            Resource(BufferSp(new Buffer<OutputStruct>(std::vector<OutputStruct>(1, outputStruct), 0)),
                     vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);

        spec.assembly      = shaderAsm;
        spec.numWorkGroups = tcu::IVec3(Constants::numThreads, 1, 1);
        spec.inputs.push_back(inputResource);
        spec.outputs.push_back(outputResource);
        spec.extensions.push_back("VK_KHR_storage_buffer_storage_class");

        testGroup->addChild(
            new SpvAsmComputeShaderCase(testCtx, "uint64_to_nested_struct_uint16_uint16_uint16_uint16", spec));
    }
}

void addMemoryReinterpretationTests(tcu::TestCaseGroup *testGroup, MEMORY_MODEL_TYPE memModel, bool read)
{
    tcu::TestContext &testCtx = testGroup->getTestContext();

    tcu::StringTemplate shaderHeader(createShaderHeader());

    const uint32_t numWGs     = 4;
    const uint32_t bufferSize = 128;

    for (uint32_t i = 0; i < DE_ENUM_COUNT(MemoryInterpretationTestCases); ++i)
    {
        MemoryInterpretationTestCases testCase = static_cast<MemoryInterpretationTestCases>(i);

        std::string testName = toString(testCase);

        tcu::StringTemplate shaderAnnotations(createShaderAnnotations(testCase, read));

        tcu::StringTemplate shaderVariables(createShaderVariables(testCase, read));

        tcu::StringTemplate shaderFunctions(createShaderMain(testCase, read));

        std::map<std::string, std::string> specMap;
        std::string memModelOp;
        std::vector<const char *> spvExts;
        std::vector<const char *> spvCaps;
        ComputeShaderSpec spec;
        adjustSpecForUntypedPointers(spec, spvExts, spvCaps);
        adjustSpecForMemoryModel(memModel, spec, memModelOp, spvExts, spvCaps);
        adjustSpecForMemoryInterpretation(spec, spvExts, spvCaps, testCase);

        specMap["memModelOp"]   = memModelOp;
        specMap["extensions"]   = toString(spvExts);
        specMap["capabilities"] = toString(spvCaps);

        const std::string shaderAsm = shaderHeader.specialize(specMap) + shaderAnnotations.specialize(specMap) +
                                      shaderVariables.specialize(specMap) + shaderFunctions.specialize(specMap);

        const std::vector<uint32_t> offsets = getOffsets(testCase);
        DE_ASSERT(offsets.size() == numWGs);
        const std::vector<uint32_t> indices = getIndices(testCase);
        DE_ASSERT(indices.size() == numWGs);

        uint32_t magic      = 42;
        uint32_t inputSize  = read ? bufferSize : numWGs;
        uint32_t outputSize = read ? numWGs : bufferSize;
        std::vector<uint32_t> inputData(inputSize, 0);
        std::vector<uint32_t> outputData(outputSize, 0xffffffff);
        for (uint32_t o = 0; o < offsets.size(); ++o)
        {
            const uint32_t outputVal =
                testCase == MemoryInterpretationTestCases::CHAR2_16BIT_STORAGE_CAP ? 0xffff0000 | magic : magic;
            uint32_t inputIdx     = read ? offsets[o] : o;
            uint32_t outputIdx    = read ? o : offsets[o];
            inputData[inputIdx]   = magic;
            outputData[outputIdx] = outputVal;
            magic++;
        }

        Resource inputResource =
            Resource(BufferSp(new Buffer<uint32_t>(inputData, 0)), vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
        Resource indicesResource =
            Resource(BufferSp(new Buffer<uint32_t>(indices, 0)), vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
        Resource outputResource =
            Resource(BufferSp(new Buffer<uint32_t>(outputData, 0)), vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);

        spec.assembly      = shaderAsm;
        spec.numWorkGroups = tcu::IVec3(numWGs, 1, 1);
        spec.inputs.push_back(inputResource);
        spec.inputs.push_back(indicesResource);
        spec.outputs.push_back(outputResource);
        spec.extensions.push_back("VK_KHR_storage_buffer_storage_class");

        testGroup->addChild(new SpvAsmComputeShaderCase(testCtx, testName.c_str(), spec));
    }
}

void addBlockArrayTests(tcu::TestCaseGroup *testGroup, MEMORY_MODEL_TYPE memModel)
{
    tcu::TestContext &testCtx = testGroup->getTestContext();

    tcu::StringTemplate shaderHeader(createShaderHeader());

    tcu::StringTemplate shaderVariables(createShaderVariables(BlockArrayTestCases::BASIC));

    const uint32_t numWGs     = 4;
    const uint32_t bufferSize = 128;

    for (uint32_t i = 0; i < DE_ENUM_COUNT(BlockArrayTestCases); ++i)
    {
        BlockArrayTestCases testCase = static_cast<BlockArrayTestCases>(i);

        std::string testName = toString(testCase);

        tcu::StringTemplate shaderAnnotations(createShaderAnnotations(testCase));

        std::map<std::string, std::string> specMap;

        tcu::StringTemplate shaderFunctions(createShaderMain(testCase, specMap));

        std::string memModelOp;
        std::vector<const char *> spvExts;
        std::vector<const char *> spvCaps;
        ComputeShaderSpec spec;
        adjustSpecForUntypedPointers(spec, spvExts, spvCaps);
        adjustSpecForMemoryModel(memModel, spec, memModelOp, spvExts, spvCaps);
        adjustSpecForBlockArray(spec, spvExts, spvCaps, testCase);

        specMap["memModelOp"]    = memModelOp;
        specMap["extensions"]    = toString(spvExts);
        specMap["capabilities"]  = toString(spvCaps);
        specMap["threads"]       = "4";
        specMap["threads_const"] = "%c_uint32_4";

        const std::string shaderAsm = shaderHeader.specialize(specMap) + shaderAnnotations.specialize(specMap) +
                                      shaderVariables.specialize(specMap) + shaderFunctions.specialize(specMap);

        spec.numArrayInputs               = 4;
        std::vector<uint32_t> indicesData = {0, 4, 8, 12};
        std::vector<uint32_t> inputData0(bufferSize, 0);
        std::vector<uint32_t> inputData1(bufferSize, 0);
        std::vector<uint32_t> inputData2(bufferSize, 0);
        std::vector<uint32_t> inputData3(bufferSize, 0);
        if (testCase == BlockArrayTestCases::BASIC)
        {
            inputData0[0]  = 42;
            inputData1[4]  = 43;
            inputData2[8]  = 44;
            inputData3[12] = 45;
        }
        else
        {
            inputData0[0]  = 42;
            inputData1[16] = 43;
            inputData2[32] = 44;
            inputData3[48] = 45;
        }
        std::vector<uint32_t> outputData = {42, 43, 44, 45};

        Resource indicesResource =
            Resource(BufferSp(new Buffer<uint32_t>(indicesData, 0)), vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
        Resource inputResource0 =
            Resource(BufferSp(new Buffer<uint32_t>(inputData0, 0)), vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
        Resource inputResource1 =
            Resource(BufferSp(new Buffer<uint32_t>(inputData1, 0)), vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
        Resource inputResource2 =
            Resource(BufferSp(new Buffer<uint32_t>(inputData2, 0)), vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
        Resource inputResource3 =
            Resource(BufferSp(new Buffer<uint32_t>(inputData3, 0)), vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
        Resource outputResource =
            Resource(BufferSp(new Buffer<uint32_t>(outputData, 0)), vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);

        spec.assembly      = shaderAsm;
        spec.numWorkGroups = tcu::IVec3(numWGs, 1, 1);
        spec.inputs.push_back(indicesResource);
        spec.inputs.push_back(inputResource0);
        spec.inputs.push_back(inputResource1);
        spec.inputs.push_back(inputResource2);
        spec.inputs.push_back(inputResource3);
        spec.outputs.push_back(outputResource);

        testGroup->addChild(new SpvAsmComputeShaderCase(testCtx, testName.c_str(), spec));
    }
}

void addMultipleAccessChainTests(tcu::TestCaseGroup *testGroup, MEMORY_MODEL_TYPE memModel)
{
    tcu::TestContext &testCtx = testGroup->getTestContext();

    tcu::StringTemplate shaderHeader(createShaderHeader());

    tcu::StringTemplate shaderAnnotations(createShaderAnnotations(TypePunningTestCases::MULTIPLE_ACCESS_CHAINS));

    tcu::StringTemplate shaderVariables(createShaderVariables(TypePunningTestCases::MULTIPLE_ACCESS_CHAINS));

    tcu::StringTemplate shaderFunctions(createShaderMain(TypePunningTestCases::MULTIPLE_ACCESS_CHAINS));

    for (uint32_t i = 0; i < DE_LENGTH_OF_ARRAY(BASE_DATA_TYPE_CASES); ++i)
    {
        std::string testName = toString(BASE_DATA_TYPE_CASES[i]);

        std::map<std::string, std::string> specMap;
        specMap["baseDecl"] = getDeclaration(BASE_DATA_TYPE_CASES[i]);
        specMap["baseType"] = toString(BASE_DATA_TYPE_CASES[i]);
        specMap["size"]     = std::to_string(getSizeInBytes(BASE_DATA_TYPE_CASES[i]));

        std::string memModelOp;
        std::vector<const char *> spvExts;
        std::vector<const char *> spvCaps;
        ComputeShaderSpec spec;
        adjustSpecForUntypedPointers(spec, spvExts, spvCaps);
        adjustSpecForMemoryModel(memModel, spec, memModelOp, spvExts, spvCaps);
        adjustSpecForDataTypes(BASE_DATA_TYPE_CASES[i], spec, spvExts, spvCaps);
        adjustSpecForSmallContainerType(ContainerTypes::STORAGE_BUFFER, BASE_DATA_TYPE_CASES[i], spec, spvExts,
                                        spvCaps);

        specMap["memModelOp"]   = memModelOp;
        specMap["extensions"]   = toString(spvExts);
        specMap["capabilities"] = toString(spvCaps);

        std::string shaderVariablesStr = shaderVariables.specialize(specMap);
        if (BASE_DATA_TYPE_CASES[i] != DataTypes::UINT32)
        {
            shaderVariablesStr = "%uint32     = OpTypeInt  32      0\n"
                                 "%c_uint32_1 = OpConstant %uint32 1\n" +
                                 shaderVariablesStr;
        }

        const std::string shaderAsm = shaderHeader.specialize(specMap) + shaderAnnotations.specialize(specMap) +
                                      shaderVariablesStr + shaderFunctions.specialize(specMap);

        FilledBufferDesc desc;
        desc.dataType   = BASE_DATA_TYPE_CASES[i];
        desc.elemCount  = 2;
        desc.padding    = 0;
        desc.fillType   = FillingTypes::VALUE;
        desc.value      = 2;
        Resource input  = createFilledResource(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, desc);
        Resource output = createFilledResource(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, desc);

        spec.assembly      = shaderAsm;
        spec.numWorkGroups = tcu::IVec3(Constants::numThreads, 1, 1);
        spec.inputs.push_back(input);
        spec.outputs.push_back(output);
        spec.extensions.push_back("VK_KHR_storage_buffer_storage_class");

        testGroup->addChild(new SpvAsmComputeShaderCase(testCtx, testName.c_str(), spec));
    }
}

void addVariablePointersMultipleAccessChainTests(tcu::TestCaseGroup *testGroup, MEMORY_MODEL_TYPE memModel)
{
    tcu::TestContext &testCtx = testGroup->getTestContext();

    tcu::StringTemplate shaderHeader(createShaderHeader());

    tcu::StringTemplate shaderAnnotations(
        createShaderAnnotations(PointerTestCases::MULTIPLE_ACCESS_CHAINS_VARIABLE_PTR));

    tcu::StringTemplate shaderVariables(createShaderVariables(PointerTestCases::MULTIPLE_ACCESS_CHAINS_VARIABLE_PTR));

    tcu::StringTemplate shaderFunctions(createShaderMain(PointerTestCases::MULTIPLE_ACCESS_CHAINS_VARIABLE_PTR));

    for (uint32_t i = 0; i < DE_LENGTH_OF_ARRAY(BASE_DATA_TYPE_CASES); ++i)
    {
        std::string testName = toString(BASE_DATA_TYPE_CASES[i]);

        std::map<std::string, std::string> specMap;
        specMap["baseDecl"]    = getDeclaration(BASE_DATA_TYPE_CASES[i]);
        specMap["baseType"]    = toString(BASE_DATA_TYPE_CASES[i]);
        specMap["secondType"]  = getSecondTypeDefinitionForVarPtrs(BASE_DATA_TYPE_CASES[i]);
        specMap["otherType"]   = toString(getSecondTypeForVarPtrs(BASE_DATA_TYPE_CASES[i]));
        specMap["secondArray"] = getSecondArrayDefinitionForVarPtrs(BASE_DATA_TYPE_CASES[i]);
        specMap["alignment0"]  = std::to_string(getSizeInBytes(BASE_DATA_TYPE_CASES[i]));
        specMap["alignment1"]  = std::to_string(getSecondAlignmentForVarPtrs(BASE_DATA_TYPE_CASES[i]));
        specMap["elemNdx"]     = getSameByteIndexForVarPtrs(BASE_DATA_TYPE_CASES[i]);

        std::string memModelOp;
        std::vector<const char *> spvExts;
        std::vector<const char *> spvCaps;
        ComputeShaderSpec spec;
        adjustSpecForUntypedPointers(spec, spvExts, spvCaps);
        adjustSpecForMemoryModel(memModel, spec, memModelOp, spvExts, spvCaps);
        adjustSpecForDataTypes(BASE_DATA_TYPE_CASES[i], spec, spvExts, spvCaps);
        adjustSpecForDataTypes(DataTypes::INT16, spec, spvExts, spvCaps);
        adjustSpecForVariablePointers(spec, spvExts, spvCaps);

        specMap["memModelOp"]   = memModelOp;
        specMap["extensions"]   = toString(spvExts);
        specMap["capabilities"] = toString(spvCaps);

        std::string shaderVariablesStr = shaderVariables.specialize(specMap);
        if (BASE_DATA_TYPE_CASES[i] != DataTypes::UINT32)
        {
            shaderVariablesStr = "%uint32     = OpTypeInt  32      0\n" + shaderVariablesStr;
        }

        const std::string shaderAsm = shaderHeader.specialize(specMap) + shaderAnnotations.specialize(specMap) +
                                      shaderVariablesStr + shaderFunctions.specialize(specMap);

        FilledBufferDesc desc;
        desc.dataType  = BASE_DATA_TYPE_CASES[i];
        desc.elemCount = 32;
        desc.padding   = 0;
        desc.fillType  = FillingTypes::INCREMENTED;
        Resource input = createFilledResource(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, desc);

        std::vector<uint8_t> inputBytes(input.getByteSize());
        input.getBytes(inputBytes);

        const uint32_t byteOffset =                     // calculating byte offset returned bu OpUntypedAccessChainKHR
            4 * getSizeInBytes(BASE_DATA_TYPE_CASES[i]) // 4 elem offset in first array
            + 8 * getSizeInBytes(getSecondTypeForVarPtrs(BASE_DATA_TYPE_CASES[i])); // 8 elem offset in second array
        std::vector<uint8_t> outputBytes(inputBytes.begin() + byteOffset,
                                         inputBytes.begin() + byteOffset +
                                             getSecondAlignmentForVarPtrs(BASE_DATA_TYPE_CASES[i]));

        Resource output =
            Resource(BufferSp(new Buffer<uint8_t>(outputBytes, 0)), vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);

        spec.assembly      = shaderAsm;
        spec.numWorkGroups = tcu::IVec3(1, 1, 1);
        spec.inputs.push_back(input);
        spec.outputs.push_back(output);
        spec.extensions.push_back("VK_KHR_storage_buffer_storage_class");

        testGroup->addChild(new SpvAsmComputeShaderCase(testCtx, testName.c_str(), spec));
    }
}

void addPhysicalStorageOpBitcastTests(tcu::TestCaseGroup *testGroup, MEMORY_MODEL_TYPE memModel, bool fromUntyped)
{
    tcu::TestContext &testCtx = testGroup->getTestContext();

    const POINTER_TEST_CASE ptrTestType = fromUntyped ? PointerTestCases::OP_BITCAST_FROM_UNTYPED_PHYSICAL_STORAGE :
                                                        PointerTestCases::OP_BITCAST_TO_UNTYPED_PHYSICAL_STORAGE;

    tcu::StringTemplate shaderHeader(createShaderHeader());

    tcu::StringTemplate shaderAnnotations(createShaderAnnotations(ptrTestType));

    tcu::StringTemplate shaderVariables(createShaderVariables(ptrTestType));

    tcu::StringTemplate shaderFunctions(createShaderMain(ptrTestType));

    for (uint32_t i = 0; i < DE_LENGTH_OF_ARRAY(BASE_DATA_TYPE_CASES); ++i)
    {
        std::string testName = toString(BASE_DATA_TYPE_CASES[i]);

        std::map<std::string, std::string> specMap;
        specMap["baseDecl"]  = getDeclaration(BASE_DATA_TYPE_CASES[i]);
        specMap["baseType"]  = toString(BASE_DATA_TYPE_CASES[i]);
        specMap["alignment"] = std::to_string(getSizeInBytes(BASE_DATA_TYPE_CASES[i]));

        std::string memModelOp;
        std::vector<const char *> spvExts;
        std::vector<const char *> spvCaps;
        ComputeShaderSpec spec;
        adjustSpecForUntypedPointers(spec, spvExts, spvCaps);
        adjustSpecForMemoryModel(memModel, spec, memModelOp, spvExts, spvCaps);
        adjustSpecForDataTypes(BASE_DATA_TYPE_CASES[i], spec, spvExts, spvCaps);
        adjustSpecForPhysicalStorageBuffer(memModel, spec, memModelOp, spvExts, spvCaps);

        specMap["memModelOp"]                         = memModelOp;
        specMap["extensions"]                         = toString(spvExts);
        specMap["capabilities"]                       = toString(spvCaps);
        const tcu::StringTemplate tempShaderFunctions = tcu::StringTemplate(shaderFunctions.specialize(specMap));

        std::string shaderVariablesStr = shaderVariables.specialize(specMap);
        if (BASE_DATA_TYPE_CASES[i] != DataTypes::UINT32)
        {
            shaderVariablesStr = "%uint32     = OpTypeInt  32      0\n" + shaderVariablesStr;
        }

        const std::string shaderAsm = shaderHeader.specialize(specMap) + shaderAnnotations.specialize(specMap) +
                                      shaderVariablesStr + tempShaderFunctions.specialize(specMap);

        FilledBufferDesc desc;
        desc.dataType  = BASE_DATA_TYPE_CASES[i];
        desc.elemCount = 1;
        desc.padding   = 0;
        desc.fillType  = FillingTypes::VALUE;
        desc.value     = 1;

        Resource inputOutput = createFilledResource(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, desc);

        spec.assembly              = shaderAsm;
        spec.numWorkGroups         = tcu::IVec3(1, 1, 1);
        spec.usesPhysStorageBuffer = true;
        spec.inputs.push_back(inputOutput);
        spec.outputs.push_back(inputOutput);
        spec.extensions.push_back("VK_KHR_storage_buffer_storage_class");

        testGroup->addChild(new SpvAsmComputeShaderCase(testCtx, testName.c_str(), spec));
    }
}

void addVariablePointersWorkgroupMemoryTests(tcu::TestCaseGroup *testGroup, MEMORY_MODEL_TYPE memModel)
{
    tcu::TestContext &testCtx = testGroup->getTestContext();

    tcu::StringTemplate shaderHeader(createShaderHeader("%input_data_var %output_data_var %workgroup_untyped_var"));

    tcu::StringTemplate shaderAnnotations(createShaderAnnotations(PointerTestCases::WORKGROUP_MEMORY_VARIABLE_PTR));

    tcu::StringTemplate shaderVariables(createShaderVariables(PointerTestCases::WORKGROUP_MEMORY_VARIABLE_PTR));

    tcu::StringTemplate shaderFunctions(createShaderMain(PointerTestCases::WORKGROUP_MEMORY_VARIABLE_PTR));

    for (uint32_t i = 0; i < DE_LENGTH_OF_ARRAY(BASE_DATA_TYPE_CASES); ++i)
    {
        std::string testName = toString(BASE_DATA_TYPE_CASES[i]);

        std::map<std::string, std::string> specMap;
        specMap["baseDecl"]  = getDeclaration(BASE_DATA_TYPE_CASES[i]);
        specMap["baseType"]  = toString(BASE_DATA_TYPE_CASES[i]);
        specMap["alignment"] = std::to_string(getSizeInBytes(BASE_DATA_TYPE_CASES[i]));

        std::string memModelOp;
        std::vector<const char *> spvExts;
        std::vector<const char *> spvCaps;
        ComputeShaderSpec spec;
        adjustSpecForUntypedPointers(spec, spvExts, spvCaps);
        adjustSpecForMemoryModel(memModel, spec, memModelOp, spvExts, spvCaps);
        adjustSpecForDataTypes(BASE_DATA_TYPE_CASES[i], spec, spvExts, spvCaps);
        adjustSpecForVariablePointers(spec, spvExts, spvCaps);
        adjustSpecForWorkgroupMemoryExplicitLayout(BASE_DATA_TYPE_CASES[i], spec, spvExts, spvCaps);

        specMap["memModelOp"]   = memModelOp;
        specMap["extensions"]   = toString(spvExts);
        specMap["capabilities"] = toString(spvCaps);

        std::string shaderVariablesStr = shaderVariables.specialize(specMap);
        if (BASE_DATA_TYPE_CASES[i] != DataTypes::UINT32)
        {
            shaderVariablesStr = "%uint32     = OpTypeInt  32      0\n" + shaderVariablesStr;
        }

        const std::string shaderAsm = shaderHeader.specialize(specMap) + shaderAnnotations.specialize(specMap) +
                                      shaderVariablesStr + shaderFunctions.specialize(specMap);

        FilledBufferDesc desc;
        desc.dataType   = BASE_DATA_TYPE_CASES[i];
        desc.elemCount  = Constants::numThreads;
        desc.padding    = 0;
        desc.seed       = deStringHash(testGroup->getName());
        desc.fillType   = FillingTypes::RANDOM;
        Resource input  = createFilledResource(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, desc);
        Resource output = createFilledResource(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, desc);

        spec.assembly      = shaderAsm;
        spec.numWorkGroups = tcu::IVec3(Constants::numThreads, 1, 1);
        spec.spirvVersion  = SPIRV_VERSION_1_4; // workgroup memory explicit layout requires SPIR-V 1.4
        spec.inputs.push_back(input);
        spec.outputs.push_back(output);
        spec.extensions.push_back("VK_KHR_storage_buffer_storage_class");

        testGroup->addChild(new SpvAsmComputeShaderCase(testCtx, testName.c_str(), spec));
    }
}

void addWorkgroupMemoryExplicitLayoutInteractionTests(tcu::TestCaseGroup *testGroup, MEMORY_MODEL_TYPE memModel,
                                                      WORKGROUP_TEST_CASE testCase)
{
    tcu::TestContext &testCtx = testGroup->getTestContext();

    tcu::StringTemplate shaderHeader(
        createShaderHeader("%input_data_0_var %input_data_1_var %data_buffer_0_untyped_var %data_buffer_1_untyped_var "
                           "%output_data_0_var %output_data_1_var"));

    tcu::StringTemplate shaderAnnotations(createShaderAnnotations(testCase));

    tcu::StringTemplate shaderVariables(createShaderVariables(testCase));

    tcu::StringTemplate shaderFunctions(createShaderMain(testCase));

    for (uint32_t i = 0; i < DE_LENGTH_OF_ARRAY(BASE_DATA_TYPE_CASES); ++i)
    {
        std::string testName = toString(BASE_DATA_TYPE_CASES[i]);

        std::map<std::string, std::string> specMap;
        specMap["baseDecl"]  = getDeclaration(BASE_DATA_TYPE_CASES[i]);
        specMap["baseType"]  = toString(BASE_DATA_TYPE_CASES[i]);
        specMap["vecOffset"] = std::to_string(4 * getSizeInBytes(BASE_DATA_TYPE_CASES[i]));

        std::string memModelOp;
        std::vector<const char *> spvExts;
        std::vector<const char *> spvCaps;
        ComputeShaderSpec spec;
        adjustSpecForUntypedPointers(spec, spvExts, spvCaps);
        adjustSpecForMemoryModel(memModel, spec, memModelOp, spvExts, spvCaps);
        adjustSpecForDataTypes(BASE_DATA_TYPE_CASES[i], spec, spvExts, spvCaps);
        adjustSpecForWorkgroupMemoryExplicitLayout(BASE_DATA_TYPE_CASES[i], spec, spvExts, spvCaps);

        specMap["memModelOp"]   = memModelOp;
        specMap["extensions"]   = toString(spvExts);
        specMap["capabilities"] = toString(spvCaps);

        const tcu::StringTemplate tempShaderFunctions = tcu::StringTemplate(shaderFunctions.specialize(specMap));

        std::string shaderVariablesStr = shaderVariables.specialize(specMap);
        if (BASE_DATA_TYPE_CASES[i] != DataTypes::UINT32)
        {
            shaderVariablesStr = "%uint32     = OpTypeInt  32      0\n" + shaderVariablesStr;
        }

        const std::string shaderAsm = shaderHeader.specialize(specMap) + shaderAnnotations.specialize(specMap) +
                                      shaderVariablesStr + tempShaderFunctions.specialize(specMap);

        FilledBufferDesc desc;
        desc.dataType   = BASE_DATA_TYPE_CASES[i];
        desc.elemCount  = 5; // scalar + vec4
        desc.padding    = 0;
        desc.fillType   = FillingTypes::VALUE;
        desc.value      = 1;
        Resource input0 = createFilledResource(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, desc);

        desc.value      = 4;
        Resource input1 = createFilledResource(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, desc);

        spec.assembly      = shaderAsm;
        spec.numWorkGroups = tcu::IVec3(Constants::numThreads, 1, 1);
        spec.spirvVersion  = SPIRV_VERSION_1_4; // workgroup memory explicit layout requires SPIR-V 1.4
        spec.inputs.push_back(input0);
        spec.inputs.push_back(input1);
        if (testCase == WorkgroupTestCases::ALIASED)
        {
            spec.outputs.push_back(input1);
            spec.outputs.push_back(input1);
        }
        else // WorkgroupTestCases::NOT_ALIASED
        {
            spec.outputs.push_back(input0);
            spec.outputs.push_back(input1);
        }
        spec.extensions.push_back("VK_KHR_storage_buffer_storage_class");

        testGroup->addChild(new SpvAsmComputeShaderCase(testCtx, testName.c_str(), spec));
    }
}

struct MatrixSize
{
    uint32_t rows;
    uint32_t cols;
};

const char *getShaderInterfaces(COOPERATIVE_MATRIX_TEST_CASE testCase)
{
    static const char *const translateTable[DE_ENUM_COUNT(CooperativeMatrixTestCases)] = {
        "%input_data_untyped_var %output_data_var", // BASIC_LOAD
        "%input_data_var %output_data_untyped_var", // BASIC_STORE
        "%input_data_untyped_var %output_data_var", // TYPE_PUNNING_LOAD
        "%input_data_var %output_data_untyped_var", // TYPE_PUNNING_STORE
        "%input_data_untyped_var %output_data_var", // MIXED_LOAD
        "%input_data_var %output_data_untyped_var", // MIXED_STORE
    };

    return translateTable[DE_ENUM_INDEX(testCase)];
}

VkComponentTypeKHR getVkComponentType(DATA_TYPE type)
{
    static const VkComponentTypeKHR translateTable[DE_ENUM_COUNT(DataTypes)] = {
        VK_COMPONENT_TYPE_UINT8_KHR,   // UINT8
        VK_COMPONENT_TYPE_SINT8_KHR,   // INT8
        VK_COMPONENT_TYPE_UINT16_KHR,  // UINT16
        VK_COMPONENT_TYPE_SINT16_KHR,  // INT16
        VK_COMPONENT_TYPE_FLOAT16_KHR, // FLOAT16
        VK_COMPONENT_TYPE_UINT32_KHR,  // UINT32
        VK_COMPONENT_TYPE_SINT32_KHR,  // INT32
        VK_COMPONENT_TYPE_FLOAT32_KHR, // FLOAT32
        VK_COMPONENT_TYPE_UINT64_KHR,  // UINT64
        VK_COMPONENT_TYPE_SINT64_KHR,  // INT64
        VK_COMPONENT_TYPE_FLOAT64_KHR, // FLOAT64
    };

    return translateTable[DE_ENUM_INDEX(type)];
}

bool checkMatrixSupport(const InstanceInterface &instance, VkPhysicalDevice physicalDevice, MATRIX_TYPE matrixType,
                        DATA_TYPE dataType)
{
    uint32_t propsCnt = 0;
    std::vector<VkCooperativeMatrixPropertiesKHR> props;
    instance.getPhysicalDeviceCooperativeMatrixPropertiesKHR(physicalDevice, &propsCnt, nullptr);
    props.resize(propsCnt);
    for (size_t ndx = 0; ndx < props.size(); ndx++)
        props[ndx].sType = VK_STRUCTURE_TYPE_COOPERATIVE_MATRIX_PROPERTIES_KHR;
    instance.getPhysicalDeviceCooperativeMatrixPropertiesKHR(physicalDevice, &propsCnt, props.data());

    bool isSupported = false;
    for (size_t ndx = 0; ndx < props.size(); ndx++)
    {
        switch (matrixType)
        {
        case MatrixTypes::A:
        {
            if (getVkComponentType(dataType) == props[ndx].AType)
                isSupported = true;

            break;
        }
        case MatrixTypes::B:
        {
            if (getVkComponentType(dataType) == props[ndx].BType)
                isSupported = true;

            break;
        }
        case MatrixTypes::ACCUMULATOR:
        {
            if (getVkComponentType(dataType) == props[ndx].CType)
                isSupported = true;

            break;
        }
        default:
            break;
        }

        if (isSupported)
            break;
    }

    return isSupported;
}

MatrixSize getMatrixSize(const InstanceInterface &instance, VkPhysicalDevice physicalDevice, MATRIX_TYPE matrixType,
                         DATA_TYPE dataType)
{
    uint32_t propsCnt = 0;
    std::vector<VkCooperativeMatrixPropertiesKHR> props;
    instance.getPhysicalDeviceCooperativeMatrixPropertiesKHR(physicalDevice, &propsCnt, nullptr);
    props.resize(propsCnt);
    for (size_t ndx = 0; ndx < props.size(); ndx++)
        props[ndx].sType = VK_STRUCTURE_TYPE_COOPERATIVE_MATRIX_PROPERTIES_KHR;
    instance.getPhysicalDeviceCooperativeMatrixPropertiesKHR(physicalDevice, &propsCnt, props.data());

    bool isFound = false;
    MatrixSize size{0u, 0u};
    for (size_t ndx = 0; ndx < props.size(); ndx++)
    {
        switch (matrixType)
        {
        case MatrixTypes::A:
        {
            if (getVkComponentType(dataType) == props[ndx].AType)
            {
                size.rows = props[ndx].MSize;
                size.cols = props[ndx].KSize;
                isFound   = true;
            }

            break;
        }
        case MatrixTypes::B:
        {
            if (getVkComponentType(dataType) == props[ndx].BType)
            {
                size.rows = props[ndx].KSize;
                size.cols = props[ndx].NSize;
                isFound   = true;
            }

            break;
        }
        case MatrixTypes::ACCUMULATOR:
        {
            if (getVkComponentType(dataType) == props[ndx].CType)
            {
                size.rows = props[ndx].MSize;
                size.cols = props[ndx].NSize;
                isFound   = true;
            }

            break;
        }
        default:
            break;
        }

        if (isFound)
            break;
    }

    return size;
}

struct CooperativeMatrixInteractionTestParams
{
    COOPERATIVE_MATRIX_TEST_CASE testCase;
    MATRIX_LAYOUT matLayout;
    MATRIX_TYPE matType;
    DATA_TYPE dataType;
    DATA_TYPE sameSizeDataType;
    MEMORY_MODEL_TYPE memModel;
};

class CooperativeMatrixInteractionTestInstance : public TestInstance
{
public:
    CooperativeMatrixInteractionTestInstance(Context &ctx, const CooperativeMatrixInteractionTestParams &params)
        : TestInstance(ctx)
        , m_params(params)
    {
    }
    tcu::TestStatus iterate(void);

private:
    const CooperativeMatrixInteractionTestParams &m_params;
};

tcu::TestStatus CooperativeMatrixInteractionTestInstance::iterate(void)
{
    const InstanceInterface &ivk           = m_context.getInstanceInterface();
    const DeviceInterface &vk              = m_context.getDeviceInterface();
    const VkPhysicalDevice &physicalDevice = m_context.getPhysicalDevice();
    const VkDevice device                  = m_context.getDevice();
    const VkQueue queue                    = m_context.getUniversalQueue();
    const uint32_t queueNdx                = m_context.getUniversalQueueFamilyIndex();
    Allocator &allocator                   = m_context.getDefaultAllocator();

    MatrixSize matrixSize         = getMatrixSize(ivk, physicalDevice, m_params.matType, m_params.dataType);
    const VkDeviceSize bufferSize = matrixSize.rows * matrixSize.cols * getSizeInBytes(m_params.dataType);
    if (bufferSize == 0)
        TCU_THROW(NotSupportedError, "Cooperative matrix feature is not supported");

    // Gen input and expected data
    FilledBufferDesc desc;
    desc.dataType        = m_params.dataType;
    desc.elemCount       = 1;
    desc.padding         = 0;
    desc.fillType        = FillingTypes::VALUE;
    desc.value           = 1;
    Resource inputOutput = createFilledResource(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, desc);
    std::vector<uint8_t> expectedBytes;
    inputOutput.getBytes(expectedBytes);

    // Storage buffers
    const BufferWithMemory inputBuffer(vk, device, allocator,
                                       makeBufferCreateInfo(bufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT),
                                       MemoryRequirement::HostVisible);

    {
        const Allocation &alloc = inputBuffer.getAllocation();

        deMemcpy(alloc.getHostPtr(), expectedBytes.data(), expectedBytes.size());
        flushAlloc(vk, device, alloc);
        // No barrier needed, flushed memory is automatically visible
    }

    const BufferWithMemory outputBuffer(vk, device, allocator,
                                        makeBufferCreateInfo(bufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT),
                                        MemoryRequirement::HostVisible);

    // Descriptors
    const Unique<VkDescriptorSetLayout> descriptorSetLayout(
        DescriptorSetLayoutBuilder()
            .addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
            .addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
            .build(vk, device));

    const Unique<VkDescriptorPool> descriptorPool(
        DescriptorPoolBuilder()
            .addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2u)
            .build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u));

    const Unique<VkDescriptorSet> descriptorSet(makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout));

    const VkDescriptorBufferInfo inputBufferInfo(makeDescriptorBufferInfo(inputBuffer.get(), 0ull, bufferSize));
    const VkDescriptorBufferInfo outputBufferInfo(makeDescriptorBufferInfo(outputBuffer.get(), 0ull, bufferSize));

    DescriptorSetUpdateBuilder()
        .writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u),
                     VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &inputBufferInfo)
        .writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(1u),
                     VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &outputBufferInfo)
        .update(vk, device);

    // Pipeline
    const Unique<VkPipelineLayout> pipelineLayout(makePipelineLayout(vk, device, *descriptorSetLayout));

    const VkSpecializationMapEntry specializationMapEntries[] = {
        {
            0u, // uint32_t    constantID
            0u, // uint32_t    offset
            4   // size_t      size
        },
        {
            1u, // uint32_t    constantID
            4u, // uint32_t    offset
            4   // size_t      size
        },
    };

    const VkSpecializationInfo specializationInfo = {
        2u,                       // uint32_t                           mapEntryCount
        specializationMapEntries, // const VkSpecializationMapEntry*    pMapEntries
        sizeof(matrixSize),       // size_t                             dataSize
        &matrixSize               // const void*                        pData
    };

    BinaryCollection &binaries = m_context.getBinaryCollection();
    const Unique<VkShaderModule> shaderModule(createShaderModule(vk, device, binaries.get("compute")));

    const Unique<VkPipeline> computePipeline(
        makeComputePipeline(vk, device, *pipelineLayout, 0u, nullptr, *shaderModule, 0u, &specializationInfo));

    // Commands
    const Unique<VkCommandPool> cmdPool(makeCommandPool(vk, device, queueNdx));
    const Unique<VkCommandBuffer> cmdBuffer(
        allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

    // Reset the command buffer and begin recording.
    beginCommandBuffer(vk, *cmdBuffer);

    vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *computePipeline);
    vk.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayout, 0u, 1u, &descriptorSet.get(),
                             0u, nullptr);

    vk.cmdDispatch(*cmdBuffer, 1u, 1u, 1u);

    endCommandBuffer(vk, *cmdBuffer);
    submitCommandsAndWait(vk, device, queue, *cmdBuffer);

    // Retrive result from buffer
    const Allocation &outputBufferAllocation = outputBuffer.getAllocation();
    invalidateAlloc(vk, device, outputBufferAllocation);

    // Check result
    bool passed               = true;
    uint8_t *expectedBytesPtr = static_cast<uint8_t *>(outputBufferAllocation.getHostPtr());
    for (size_t ndx = 0; ndx < expectedBytes.size(); ++ndx)
    {
        if (expectedBytes[ndx] != expectedBytesPtr[ndx])
        {
            passed = false;
            break;
        }
    }

    return passed ? tcu::TestStatus::pass("Passed") : tcu::TestStatus::fail("Failed");
}

class CooperativeMatrixInteractionTestCase : public TestCase
{
public:
    CooperativeMatrixInteractionTestCase(tcu::TestContext &testCtx, const char *name,
                                         const CooperativeMatrixInteractionTestParams &params)
        : TestCase(testCtx, name)
        , m_params(params)
    {
    }

    void checkSupport(Context &context) const;
    void initPrograms(vk::SourceCollections &programCollection) const;
    TestInstance *createInstance(Context &ctx) const;

private:
    const CooperativeMatrixInteractionTestParams m_params;
};

void CooperativeMatrixInteractionTestCase::checkSupport(Context &context) const
{
    context.requireDeviceFunctionality("VK_KHR_shader_untyped_pointers");
    {
        const VkPhysicalDeviceShaderUntypedPointersFeaturesKHR &extensionFeatures =
            context.getShaderUntypedPointersFeatures();

        if (!extensionFeatures.shaderUntypedPointers)
            TCU_THROW(NotSupportedError, "Untyped pointers feature is not supported");
    }
    context.requireDeviceFunctionality("VK_KHR_cooperative_matrix");
    {
        const VkPhysicalDeviceCooperativeMatrixFeaturesKHR &extensionFeatures = context.getCooperativeMatrixFeatures();

        if (!extensionFeatures.cooperativeMatrix)
            TCU_THROW(NotSupportedError, "Cooperative matrix feature is not supported");
    }

    VkPhysicalDevice physicalDevice   = context.getPhysicalDevice();
    const InstanceInterface &instance = context.getInstanceInterface();

    DATA_TYPE dataType = m_params.dataType;
    if (m_params.testCase == CooperativeMatrixTestCases::TYPE_PUNNING_LOAD)
        dataType = m_params.sameSizeDataType;

    if (!checkMatrixSupport(instance, physicalDevice, m_params.matType, dataType))
        TCU_THROW(NotSupportedError,
                  std::string("Cooperative matrix not supported for requested params: matrix_type=") +
                      toString(m_params.matType) + ", data_type=" + toString(dataType));
}

void CooperativeMatrixInteractionTestCase::initPrograms(vk::SourceCollections &programCollection) const
{
    tcu::StringTemplate shaderHeader(createShaderHeader(getShaderInterfaces(m_params.testCase), "32 1 1"));
    tcu::StringTemplate shaderAnnotations(createShaderAnnotations(m_params.testCase));
    tcu::StringTemplate shaderVariables(createShaderVariables(m_params.testCase));
    tcu::StringTemplate shaderFunctions(createShaderMain(m_params.testCase));

    std::map<std::string, std::string> specMap;
    specMap["baseDecl"]     = getDeclaration(m_params.dataType);
    specMap["baseType"]     = toString(m_params.dataType);
    specMap["typeSize"]     = std::to_string(getSizeInBytes(m_params.dataType));
    specMap["matrixUse"]    = std::to_string(getMatrixBinaryUse(m_params.matType));
    specMap["matrixLayout"] = std::to_string(getMatrixBinaryLayout(m_params.matLayout));
    if (m_params.sameSizeDataType != DataTypes::_ENUM_COUNT)
    {
        specMap["sameSizeType"] = toString(m_params.sameSizeDataType);
        specMap["sameSizeDecl"] = getDeclaration(m_params.sameSizeDataType);
    }

    std::string memModelOp;
    std::vector<const char *> spvExts;
    std::vector<const char *> spvCaps;
    ComputeShaderSpec spec;
    adjustSpecForUntypedPointers(spec, spvExts, spvCaps);
    adjustSpecForMemoryModel(m_params.memModel, spec, memModelOp, spvExts, spvCaps);
    adjustSpecForDataTypes(m_params.dataType, spec, spvExts, spvCaps);
    if ((m_params.sameSizeDataType != DataTypes::_ENUM_COUNT) && (m_params.dataType != m_params.sameSizeDataType))
        adjustSpecForDataTypes(m_params.sameSizeDataType, spec, spvExts, spvCaps);
    adjustSpecForCooperativeMatrix(spec, spvExts, spvCaps);

    specMap["memModelOp"]                         = memModelOp;
    specMap["extensions"]                         = toString(spvExts);
    specMap["capabilities"]                       = toString(spvCaps);
    const tcu::StringTemplate tempShaderFunctions = tcu::StringTemplate(shaderFunctions.specialize(specMap));

    std::string shaderVariablesStr = shaderVariables.specialize(specMap);
    if ((m_params.dataType != DataTypes::UINT32) && (m_params.sameSizeDataType != DataTypes::UINT32))
    {
        shaderVariablesStr = "%uint32     = OpTypeInt  32      0\n" + shaderVariablesStr;
    }

    const std::string shaderAsm = shaderHeader.specialize(specMap) + shaderAnnotations.specialize(specMap) +
                                  shaderVariablesStr + tempShaderFunctions.specialize(specMap);

    programCollection.spirvAsmSources.add("compute")
        << shaderAsm.c_str()
        << SpirVAsmBuildOptions(programCollection.usedVulkanVersion,
                                SPIRV_VERSION_1_6); // cooperative matrices requires SPIR-V 1.6
}

TestInstance *CooperativeMatrixInteractionTestCase::createInstance(Context &ctx) const
{
    return new CooperativeMatrixInteractionTestInstance(ctx, m_params);
}

void addCooperativeMatrixInteractionBasicTests(tcu::TestCaseGroup *testGroup, MEMORY_MODEL_TYPE memModel)
{
    tcu::TestContext &testCtx = testGroup->getTestContext();

    // Load tests
    {
        de::MovePtr<tcu::TestCaseGroup> loadGroup(new tcu::TestCaseGroup(testCtx, "load", ""));

        for (uint32_t i = 0; i < DE_LENGTH_OF_ARRAY(MATRIX_USE_CASES); ++i)
        {
            de::MovePtr<tcu::TestCaseGroup> useCaseGroup(
                new tcu::TestCaseGroup(testCtx, toString(MATRIX_USE_CASES[i]), ""));

            for (uint32_t j = 0; j < DE_LENGTH_OF_ARRAY(MATRIX_LAYOUT_CASES); ++j)
            {
                de::MovePtr<tcu::TestCaseGroup> layoutGroup(
                    new tcu::TestCaseGroup(testCtx, toString(MATRIX_LAYOUT_CASES[j]), ""));

                for (uint32_t k = 0; k < DE_LENGTH_OF_ARRAY(BASE_DATA_TYPE_CASES); ++k)
                {
                    std::string testName = toString(BASE_DATA_TYPE_CASES[k]);

                    CooperativeMatrixInteractionTestParams params;
                    params.testCase         = CooperativeMatrixTestCases::BASIC_LOAD;
                    params.dataType         = BASE_DATA_TYPE_CASES[k];
                    params.sameSizeDataType = DataTypes::_ENUM_COUNT;
                    params.matLayout        = MATRIX_LAYOUT_CASES[j];
                    params.matType          = MATRIX_USE_CASES[i];
                    params.memModel         = memModel;

                    layoutGroup->addChild(new CooperativeMatrixInteractionTestCase(testCtx, testName.c_str(), params));
                }

                useCaseGroup->addChild(layoutGroup.release());
            }

            loadGroup->addChild(useCaseGroup.release());
        }

        testGroup->addChild(loadGroup.release());
    }

    // Store tests
    {
        de::MovePtr<tcu::TestCaseGroup> storeGroup(new tcu::TestCaseGroup(testCtx, "store", ""));

        for (uint32_t i = 0; i < DE_LENGTH_OF_ARRAY(MATRIX_USE_CASES); ++i)
        {
            de::MovePtr<tcu::TestCaseGroup> useCaseGroup(
                new tcu::TestCaseGroup(testCtx, toString(MATRIX_USE_CASES[i]), ""));

            for (uint32_t j = 0; j < DE_LENGTH_OF_ARRAY(MATRIX_LAYOUT_CASES); ++j)
            {
                de::MovePtr<tcu::TestCaseGroup> layoutGroup(
                    new tcu::TestCaseGroup(testCtx, toString(MATRIX_LAYOUT_CASES[j]), ""));

                for (uint32_t k = 0; k < DE_LENGTH_OF_ARRAY(BASE_DATA_TYPE_CASES); ++k)
                {
                    std::string testName = toString(BASE_DATA_TYPE_CASES[k]);

                    CooperativeMatrixInteractionTestParams params;
                    params.testCase         = CooperativeMatrixTestCases::BASIC_STORE;
                    params.dataType         = BASE_DATA_TYPE_CASES[k];
                    params.sameSizeDataType = DataTypes::_ENUM_COUNT;
                    params.matLayout        = MATRIX_LAYOUT_CASES[j];
                    params.matType          = MATRIX_USE_CASES[i];
                    params.memModel         = memModel;

                    layoutGroup->addChild(new CooperativeMatrixInteractionTestCase(testCtx, testName.c_str(), params));
                }

                useCaseGroup->addChild(layoutGroup.release());
            }

            storeGroup->addChild(useCaseGroup.release());
        }

        testGroup->addChild(storeGroup.release());
    }
}

void addCooperativeMatrixInteractionTypePunningTests(tcu::TestCaseGroup *testGroup, MEMORY_MODEL_TYPE memModel)
{
    tcu::TestContext &testCtx = testGroup->getTestContext();

    // Load tests
    {
        de::MovePtr<tcu::TestCaseGroup> loadGroup(new tcu::TestCaseGroup(testCtx, "load", ""));

        for (uint32_t i = 0; i < DE_LENGTH_OF_ARRAY(MATRIX_USE_CASES); ++i)
        {
            de::MovePtr<tcu::TestCaseGroup> useCaseGroup(
                new tcu::TestCaseGroup(testCtx, toString(MATRIX_USE_CASES[i]), ""));

            for (uint32_t j = 0; j < DE_LENGTH_OF_ARRAY(MATRIX_LAYOUT_CASES); ++j)
            {
                de::MovePtr<tcu::TestCaseGroup> layoutGroup(
                    new tcu::TestCaseGroup(testCtx, toString(MATRIX_LAYOUT_CASES[j]), ""));

                for (uint32_t k = 0; k < DE_LENGTH_OF_ARRAY(BASE_DATA_TYPE_CASES); ++k)
                {
                    std::vector<DATA_TYPE> sameSizeTypes = getSameSizeBaseDataType(BASE_DATA_TYPE_CASES[k]);

                    for (uint32_t l = 0; l < sameSizeTypes.size(); ++l)
                    {
                        std::string testName =
                            toString(BASE_DATA_TYPE_CASES[k]) + std::string("_to_") + toString(sameSizeTypes[l]);

                        CooperativeMatrixInteractionTestParams params;
                        params.testCase         = CooperativeMatrixTestCases::TYPE_PUNNING_LOAD;
                        params.dataType         = BASE_DATA_TYPE_CASES[k];
                        params.sameSizeDataType = sameSizeTypes[l];
                        params.matLayout        = MATRIX_LAYOUT_CASES[j];
                        params.matType          = MATRIX_USE_CASES[i];
                        params.memModel         = memModel;

                        layoutGroup->addChild(
                            new CooperativeMatrixInteractionTestCase(testCtx, testName.c_str(), params));
                    }
                }

                useCaseGroup->addChild(layoutGroup.release());
            }

            loadGroup->addChild(useCaseGroup.release());
        }

        testGroup->addChild(loadGroup.release());
    }

    // Store tests
    {
        de::MovePtr<tcu::TestCaseGroup> storeGroup(new tcu::TestCaseGroup(testCtx, "store", ""));

        for (uint32_t i = 0; i < DE_LENGTH_OF_ARRAY(MATRIX_USE_CASES); ++i)
        {
            de::MovePtr<tcu::TestCaseGroup> useCaseGroup(
                new tcu::TestCaseGroup(testCtx, toString(MATRIX_USE_CASES[i]), ""));

            for (uint32_t j = 0; j < DE_LENGTH_OF_ARRAY(MATRIX_LAYOUT_CASES); ++j)
            {
                de::MovePtr<tcu::TestCaseGroup> layoutGroup(
                    new tcu::TestCaseGroup(testCtx, toString(MATRIX_LAYOUT_CASES[j]), ""));

                for (uint32_t k = 0; k < DE_LENGTH_OF_ARRAY(BASE_DATA_TYPE_CASES); ++k)
                {
                    std::vector<DATA_TYPE> sameSizeTypes = getSameSizeBaseDataType(BASE_DATA_TYPE_CASES[k]);

                    for (uint32_t l = 0; l < sameSizeTypes.size(); ++l)
                    {
                        std::string testName =
                            toString(BASE_DATA_TYPE_CASES[k]) + std::string("_to_") + toString(sameSizeTypes[l]);

                        CooperativeMatrixInteractionTestParams params;
                        params.testCase         = CooperativeMatrixTestCases::TYPE_PUNNING_STORE;
                        params.dataType         = BASE_DATA_TYPE_CASES[k];
                        params.sameSizeDataType = sameSizeTypes[l];
                        params.matLayout        = MATRIX_LAYOUT_CASES[j];
                        params.matType          = MATRIX_USE_CASES[i];
                        params.memModel         = memModel;

                        layoutGroup->addChild(
                            new CooperativeMatrixInteractionTestCase(testCtx, testName.c_str(), params));
                    }
                }

                useCaseGroup->addChild(layoutGroup.release());
            }

            storeGroup->addChild(useCaseGroup.release());
        }

        testGroup->addChild(storeGroup.release());
    }
}

void addCooperativeMatrixInteractionMixedTests(tcu::TestCaseGroup *testGroup, MEMORY_MODEL_TYPE memModel)
{
    tcu::TestContext &testCtx = testGroup->getTestContext();

    // Load tests
    {
        de::MovePtr<tcu::TestCaseGroup> loadGroup(new tcu::TestCaseGroup(testCtx, "load", ""));

        for (uint32_t i = 0; i < DE_LENGTH_OF_ARRAY(MATRIX_USE_CASES); ++i)
        {
            de::MovePtr<tcu::TestCaseGroup> useCaseGroup(
                new tcu::TestCaseGroup(testCtx, toString(MATRIX_USE_CASES[i]), ""));

            for (uint32_t j = 0; j < DE_LENGTH_OF_ARRAY(MATRIX_LAYOUT_CASES); ++j)
            {
                de::MovePtr<tcu::TestCaseGroup> layoutGroup(
                    new tcu::TestCaseGroup(testCtx, toString(MATRIX_LAYOUT_CASES[j]), ""));

                for (uint32_t k = 0; k < DE_LENGTH_OF_ARRAY(BASE_DATA_TYPE_CASES); ++k)
                {
                    std::string testName = toString(BASE_DATA_TYPE_CASES[k]);

                    CooperativeMatrixInteractionTestParams params;
                    params.testCase         = CooperativeMatrixTestCases::MIXED_LOAD;
                    params.dataType         = BASE_DATA_TYPE_CASES[k];
                    params.sameSizeDataType = DataTypes::_ENUM_COUNT;
                    params.matLayout        = MATRIX_LAYOUT_CASES[j];
                    params.matType          = MATRIX_USE_CASES[i];
                    params.memModel         = memModel;

                    layoutGroup->addChild(new CooperativeMatrixInteractionTestCase(testCtx, testName.c_str(), params));
                }

                useCaseGroup->addChild(layoutGroup.release());
            }

            loadGroup->addChild(useCaseGroup.release());
        }

        testGroup->addChild(loadGroup.release());
    }

    // Store tests
    {
        de::MovePtr<tcu::TestCaseGroup> storeGroup(new tcu::TestCaseGroup(testCtx, "store", ""));

        for (uint32_t i = 0; i < DE_LENGTH_OF_ARRAY(MATRIX_USE_CASES); ++i)
        {
            de::MovePtr<tcu::TestCaseGroup> useCaseGroup(
                new tcu::TestCaseGroup(testCtx, toString(MATRIX_USE_CASES[i]), ""));

            for (uint32_t j = 0; j < DE_LENGTH_OF_ARRAY(MATRIX_LAYOUT_CASES); ++j)
            {
                de::MovePtr<tcu::TestCaseGroup> layoutGroup(
                    new tcu::TestCaseGroup(testCtx, toString(MATRIX_LAYOUT_CASES[j]), ""));

                for (uint32_t k = 0; k < DE_LENGTH_OF_ARRAY(BASE_DATA_TYPE_CASES); ++k)
                {
                    std::string testName = toString(BASE_DATA_TYPE_CASES[k]);

                    CooperativeMatrixInteractionTestParams params;
                    params.testCase         = CooperativeMatrixTestCases::MIXED_STORE;
                    params.dataType         = BASE_DATA_TYPE_CASES[k];
                    params.sameSizeDataType = DataTypes::_ENUM_COUNT;
                    params.matLayout        = MATRIX_LAYOUT_CASES[j];
                    params.matType          = MATRIX_USE_CASES[i];
                    params.memModel         = memModel;

                    layoutGroup->addChild(new CooperativeMatrixInteractionTestCase(testCtx, testName.c_str(), params));
                }

                useCaseGroup->addChild(layoutGroup.release());
            }

            storeGroup->addChild(useCaseGroup.release());
        }
    }
}

void addAtomicsTestGroup(tcu::TestCaseGroup *testGroup, MEMORY_MODEL_TYPE memModel)
{
    addTestGroup(testGroup, "load", addLoadAtomicTests, memModel);
    addTestGroup(testGroup, "store", addStoreAtomicTests, memModel);
    addTestGroup(testGroup, "add", addAtomicAddTests, memModel);
    addTestGroup(testGroup, "subtract", addAtomicSubtractTests, memModel);
    addTestGroup(testGroup, "increment", addAtomicIncrementDecrementTests, memModel,
                 AtomicTestCases::OP_ATOMIC_INCREMENT);
    addTestGroup(testGroup, "decrement", addAtomicIncrementDecrementTests, memModel,
                 AtomicTestCases::OP_ATOMIC_DECREMENT);
    addTestGroup(testGroup, "min", addAtomicMinMaxTests, memModel, AtomicTestCases::OP_ATOMIC_MIN);
    addTestGroup(testGroup, "max", addAtomicMinMaxTests, memModel, AtomicTestCases::OP_ATOMIC_MAX);
    addTestGroup(testGroup, "and", addAtomicBooleanTests, memModel, AtomicTestCases::OP_ATOMIC_AND);
    addTestGroup(testGroup, "or", addAtomicBooleanTests, memModel, AtomicTestCases::OP_ATOMIC_OR);
    addTestGroup(testGroup, "xor", addAtomicBooleanTests, memModel, AtomicTestCases::OP_ATOMIC_XOR);
    addTestGroup(testGroup, "exchange", addAtomicExchangeTests, memModel);
    addTestGroup(testGroup, "compare_exchange", addAtomicCompareExchangeTests, memModel);
}

void addPhysicalStorageOpBitcastTestGroup(tcu::TestCaseGroup *testGroup, MEMORY_MODEL_TYPE memModel)
{
    addTestGroup(testGroup, "from_untyped", addPhysicalStorageOpBitcastTests, memModel, true);
    addTestGroup(testGroup, "to_untyped", addPhysicalStorageOpBitcastTests, memModel, false);
}

void addCopyTestGroup(tcu::TestCaseGroup *testGroup, MEMORY_MODEL_TYPE memModel)
{
    addTestGroup(testGroup, "from_untyped", addCopyTests, memModel, true);
    addTestGroup(testGroup, "to_untyped", addCopyTests, memModel, false);
}

void addCopyMixedTypeTestGroup(tcu::TestCaseGroup *testGroup, MEMORY_MODEL_TYPE memModel)
{
    addTestGroup(testGroup, "from_untyped", addCopyFromUntypedMixedTypeTests, memModel);
    addTestGroup(testGroup, "to_untyped", addCopyToUntypedMixedTypeTests, memModel);
}

void addBasicUsecaseTestGroup(tcu::TestCaseGroup *testGroup, MEMORY_MODEL_TYPE memModel)
{
    addTestGroup(testGroup, "load", addLoadTests, memModel);
    addTestGroup(testGroup, "store", addStoreTests, memModel);
    addTestGroup(testGroup, "copy", addCopyTestGroup, memModel);
    addTestGroup(testGroup, "array_length", addOpArrayLengthTests, memModel);
    addTestGroup(testGroup, "atomics", addAtomicsTestGroup, memModel);
    addTestGroup(testGroup, "descriptor_array", addDescriptorArrayTests, memModel);
}

void addMemoryInterpretationTestGroup(tcu::TestCaseGroup *testGroup, MEMORY_MODEL_TYPE memModel)
{
    addTestGroup(testGroup, "read", addMemoryReinterpretationTests, memModel, true);
    addTestGroup(testGroup, "write", addMemoryReinterpretationTests, memModel, false);
}

void addDataReinterpretTestGroup(tcu::TestCaseGroup *testGroup, MEMORY_MODEL_TYPE memModel)
{
    addTestGroup(testGroup, "struct_as_type", addStructAsTypeTests, memModel);
    addTestGroup(testGroup, "multiple_access_chains", addMultipleAccessChainTests, memModel);
    addTestGroup(testGroup, "memory_interpretation", addMemoryInterpretationTestGroup, memModel);
}

void addTypePunningTestGroup(tcu::TestCaseGroup *testGroup, MEMORY_MODEL_TYPE memModel)
{
    addTestGroup(testGroup, "load", addLoadMixedTypeTests, memModel);
    addTestGroup(testGroup, "store", addStoreMixedTypeTests, memModel);
    addTestGroup(testGroup, "copy", addCopyMixedTypeTestGroup, memModel);
    addTestGroup(testGroup, "reinterpret", addDataReinterpretTestGroup, memModel);
}

void addPhysicalStorageBufferInteractionTestGroup(tcu::TestCaseGroup *testGroup, MEMORY_MODEL_TYPE memModel)
{
    addTestGroup(testGroup, "op_bitcast", addPhysicalStorageOpBitcastTestGroup, memModel);
    addTestGroup(testGroup, "op_select", addPhysicalStorageOpSelectTests, memModel);
    addTestGroup(testGroup, "op_phi", addPhysicalStorageOpPhiTests, memModel);
    addTestGroup(testGroup, "op_function_call", addPhysicalStorageOpFunctionCallTests, memModel);
    addTestGroup(testGroup, "op_ptr_access_chain", addPhysicalStorageOpPtrAccessChainTests, memModel);
}

void addVariablePointersInteractionTestGroup(tcu::TestCaseGroup *testGroup, MEMORY_MODEL_TYPE memModel)
{
    addTestGroup(testGroup, "op_select", addVariablePtrOpSelectTests, memModel);
    addTestGroup(testGroup, "op_ptr_equal", addVariablePtrOpPtrEqualTests, memModel);
    addTestGroup(testGroup, "op_ptr_not_equal", addVariablePtrOpPtrNotEqualTests, memModel);
    addTestGroup(testGroup, "op_ptr_diff", addVariablePtrOpPtrDiffTests, memModel);
    addTestGroup(testGroup, "op_phi", addVariablePtrOpPhiTests, memModel);
    addTestGroup(testGroup, "op_function_call", addVariablePtrOpFunctionCallTests, memModel);
    addTestGroup(testGroup, "op_ptr_access_chain", addVariablePtrOpPtrAccessChain, memModel);
    addTestGroup(testGroup, "function_variable", addVariablePtrFunctionVariableTests, memModel);
    addTestGroup(testGroup, "private_variable", addVariablePtrPrivateVariableTests, memModel);
    addTestGroup(testGroup, "multiple_access_chains", addVariablePointersMultipleAccessChainTests, memModel);
    addTestGroup(testGroup, "workgroup_memory", addVariablePointersWorkgroupMemoryTests, memModel);
}

void addWorkgroupMemoryExplicitLayoutInteractionTestGroup(tcu::TestCaseGroup *testGroup, MEMORY_MODEL_TYPE memModel)
{
    addTestGroup(testGroup, "aliased", addWorkgroupMemoryExplicitLayoutInteractionTests, memModel,
                 WorkgroupTestCases::ALIASED);
    addTestGroup(testGroup, "not_aliased", addWorkgroupMemoryExplicitLayoutInteractionTests, memModel,
                 WorkgroupTestCases::NOT_ALIASED);
}

void addCooperativeMatrixInteractionTestGroup(tcu::TestCaseGroup *testGroup, MEMORY_MODEL_TYPE memModel)
{
    addTestGroup(testGroup, "basic_usecase", addCooperativeMatrixInteractionBasicTests, memModel);
    addTestGroup(testGroup, "type_punning", addCooperativeMatrixInteractionTypePunningTests, memModel);
    addTestGroup(testGroup, "mixed", addCooperativeMatrixInteractionMixedTests, memModel);
}

void addBlockArrayTestGroup(tcu::TestCaseGroup *testGroup, MEMORY_MODEL_TYPE memModel)
{
    addTestGroup(testGroup, "block_array", addBlockArrayTests, memModel);
}

void addVulkanMemoryModelTestGroup(tcu::TestCaseGroup *testGroup)
{
    addTestGroup(testGroup, "basic_usecase", addBasicUsecaseTestGroup, MemoryModelTypes::VULKAN);
    addTestGroup(testGroup, "type_punning", addTypePunningTestGroup, MemoryModelTypes::VULKAN);
    addTestGroup(testGroup, "variable_pointers", addVariablePointersInteractionTestGroup, MemoryModelTypes::VULKAN);
    addTestGroup(testGroup, "physical_storage", addPhysicalStorageBufferInteractionTestGroup, MemoryModelTypes::VULKAN);
    addTestGroup(testGroup, "workgroup_memory_explicit_layout", addWorkgroupMemoryExplicitLayoutInteractionTestGroup,
                 MemoryModelTypes::VULKAN);
    addTestGroup(testGroup, "cooperative_matrix", addCooperativeMatrixInteractionTestGroup, MemoryModelTypes::VULKAN);
    addTestGroup(testGroup, "block_array", addBlockArrayTestGroup, MemoryModelTypes::VULKAN);
}

void addGLSLMemoryModelTestGroup(tcu::TestCaseGroup *testGroup)
{
    addTestGroup(testGroup, "basic_usecase", addBasicUsecaseTestGroup, MemoryModelTypes::GLSL);
    addTestGroup(testGroup, "type_punning", addTypePunningTestGroup, MemoryModelTypes::GLSL);
    addTestGroup(testGroup, "variable_pointers", addVariablePointersInteractionTestGroup, MemoryModelTypes::GLSL);
    addTestGroup(testGroup, "physical_storage", addPhysicalStorageBufferInteractionTestGroup, MemoryModelTypes::GLSL);
    addTestGroup(testGroup, "workgroup_memory_explicit_layout", addWorkgroupMemoryExplicitLayoutInteractionTestGroup,
                 MemoryModelTypes::GLSL);
    addTestGroup(testGroup, "block_array", addBlockArrayTestGroup, MemoryModelTypes::GLSL);
}

tcu::TestCaseGroup *createUntypedPointersTestGroup(tcu::TestContext &testCtx)
{
    de::MovePtr<tcu::TestCaseGroup> untypedPointerTestGroup(
        new tcu::TestCaseGroup(testCtx, "untyped_pointers", "Tests for SPV_KHR_untyped_pointers extension tests."));

    untypedPointerTestGroup->addChild(createTestGroup(testCtx, "vulkan_memory_model", addVulkanMemoryModelTestGroup));
    untypedPointerTestGroup->addChild(createTestGroup(testCtx, "glsl_memory_model", addGLSLMemoryModelTestGroup));

    return untypedPointerTestGroup.release();
}
} // namespace SpirVAssembly
} // namespace vkt
