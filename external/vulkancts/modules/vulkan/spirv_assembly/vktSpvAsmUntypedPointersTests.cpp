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

#include <algorithm>
#include <map>
#include <string>
#include <vector>

#define DE_ENUM_COUNT(enumClass) static_cast<deUint32>((enumClass::_ENUM_COUNT))
#define DE_ENUM_INDEX(enumVal)   static_cast<deUint32>((enumVal))

namespace vkt
{
namespace SpirVAssembly
{
namespace Constants
{
	constexpr deUint32	numThreads				= 64;
	constexpr deUint32	uniformAlignment		= 16;
}

enum class DataTypes : deUint8
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

enum class CompositeDataTypes : deUint8
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

enum class OperationTypes : deUint8
{
	NORMAL = 0,
	ATOMIC,
	_ENUM_COUNT,
};
using OPERATION_TYPE = OperationTypes;

enum class ContainerTypes : deUint8
{
	STORAGE_BUFFER = 0,
	UNIFORM,
	PUSH_CONSTANT,
	WORKGROUP,
	_ENUM_COUNT,
};
using CONTAINER_TYPE = ContainerTypes;

enum class MemoryModelTypes : deUint8
{
	GLSL = 0,
	VULKAN,
	_ENUM_COUNT,
};
using MEMORY_MODEL_TYPE = MemoryModelTypes;

enum class CopyOperationTypes : deUint8
{
	COPY_OBJECT = 0,
	COPY_MEMORY,
	COPY_MEMORY_SIZED,
	_ENUM_COUNT,
};
using COPY_OPERATION_TYPE = CopyOperationTypes;

enum class BaseTestCases : deUint8
{
	LOAD = 0,
	STORE,
	COPY_FROM,
	COPY_TO,
	ARRAY_LENGTH,
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

enum class AtomicTestCases : deUint8
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

enum class PointerTestCases : deUint8
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
	_ENUM_COUNT,
};
using POINTER_TEST_CASE = PointerTestCases;

enum class WorkgroupTestCases : deUint8
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
	const char*		pOperation;	// SPIR-V operation
	const char*		pArgs;		// Additional arguments
	OPERATION_TYPE	type;		// Operation type
};

struct CopyOperation
{
	const char*			pCopyOp;	// SPIR-V copy operation
	COPY_OPERATION_TYPE	type;		// Copy operation type
};

static const DATA_TYPE BASE_DATA_TYPE_CASES[] = {
	DataTypes::UINT8,
	DataTypes::INT8	,
	DataTypes::UINT16,
	DataTypes::INT16,
	DataTypes::FLOAT16,
	DataTypes::UINT32,
	DataTypes::INT32,
	DataTypes::FLOAT32,
	DataTypes::UINT64,
	DataTypes::INT64,
	DataTypes::FLOAT64,
};

static const DATA_TYPE INT_BASE_DATA_TYPE_CASES[] = {
	DataTypes::UINT8,
	DataTypes::INT8	,
	DataTypes::UINT16,
	DataTypes::INT16,
	DataTypes::UINT32,
	DataTypes::INT32,
	DataTypes::UINT64,
	DataTypes::INT64,
};

static const COMPOSITE_DATA_TYPE COMPOSITE_DATA_TYPE_CASES[] = {
	CompositeDataTypes::VEC2_UINT8,
	CompositeDataTypes::VEC3_UINT8,
	CompositeDataTypes::VEC4_UINT8,
	CompositeDataTypes::VEC2_INT8,
	CompositeDataTypes::VEC3_INT8,
	CompositeDataTypes::VEC4_INT8,
	CompositeDataTypes::VEC2_UINT16,
	CompositeDataTypes::VEC3_UINT16,
	CompositeDataTypes::VEC4_UINT16,
	CompositeDataTypes::VEC2_INT16,
	CompositeDataTypes::VEC3_INT16,
	CompositeDataTypes::VEC4_INT16,
	CompositeDataTypes::VEC2_FLOAT16,
	CompositeDataTypes::VEC3_FLOAT16,
	CompositeDataTypes::VEC4_FLOAT16,
	CompositeDataTypes::VEC2_UINT32,
	CompositeDataTypes::VEC3_UINT32,
	CompositeDataTypes::VEC4_UINT32,
	CompositeDataTypes::VEC2_INT32,
	CompositeDataTypes::VEC3_INT32,
	CompositeDataTypes::VEC4_INT32,
	CompositeDataTypes::VEC2_FLOAT32,
	CompositeDataTypes::VEC3_FLOAT32,
	CompositeDataTypes::VEC4_FLOAT32,
	CompositeDataTypes::VEC2_UINT64,
	CompositeDataTypes::VEC3_UINT64,
	CompositeDataTypes::VEC4_UINT64,
	CompositeDataTypes::VEC2_INT64,
	CompositeDataTypes::VEC3_INT64,
	CompositeDataTypes::VEC4_INT64,
	CompositeDataTypes::VEC2_FLOAT64,
	CompositeDataTypes::VEC3_FLOAT64,
	CompositeDataTypes::VEC4_FLOAT64,
};

static const CONTAINER_TYPE LOAD_CONTAINER_TYPE_CASES[] = {
	ContainerTypes::STORAGE_BUFFER,
	ContainerTypes::UNIFORM,
	ContainerTypes::PUSH_CONSTANT,
};

static const Operation LOAD_OPERATION_CASES[] = {
	{	"OpLoad",		"",							OperationTypes::NORMAL	},
	{	"OpAtomicLoad",	"%c_uint32_1 %c_uint32_0",	OperationTypes::ATOMIC	},
};

static const Operation STORE_OPERATION_CASES[] = {
	{	"OpStore",			"",							OperationTypes::NORMAL	},
	{	"OpAtomicStore",	"%c_uint32_1 %c_uint32_0",	OperationTypes::ATOMIC	},
};

static const CopyOperation COPY_OPERATION_CASES[] = {
	{	"%object_loc         = OpLoad       %${copyType} %input_data_var_loc\n"
		"%coppied_object_loc = OpCopyObject %${copyType} %object_loc\n"
		"                      OpStore %output_data_var_loc %coppied_object_loc\n",				CopyOperationTypes::COPY_OBJECT			},
	{	"OpCopyMemory          %output_data_var_loc %input_data_var_loc",						CopyOperationTypes::COPY_MEMORY			},
	{	"OpCopyMemorySized     %output_data_var_loc %input_data_var_loc %c_uint32_data_size",	CopyOperationTypes::COPY_MEMORY_SIZED	},
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

static deUint32 getSizeInBytes(DATA_TYPE type)
{
	static const deUint32 sizeTable[DE_ENUM_COUNT(DataTypes)] = {
		1,	// UINT8
		1,	// INT8
		2,	// UINT16
		2,	// INT16
		2,	// FLOAT16
		4,	// UINT32
		4,	// INT32
		4,	// FLOAT32
		8,	// UINT64
		8,	// INT64
		8,	// FLOAT64
	};

	return sizeTable[DE_ENUM_INDEX(type)];
}

static deUint32 getSizeInBytes(COMPOSITE_DATA_TYPE type)
{
	static const deUint32 sizeTable[DE_ENUM_COUNT(CompositeDataTypes)] = {
		2,	// VEC2_UINT8
		3,	// VEC3_UINT8
		4,	// VEC4_UINT8
		2,	// VEC2_INT8
		3,	// VEC3_INT8
		4,	// VEC4_INT8
		4,	// VEC2_UINT16
		6,	// VEC3_UINT16
		8,	// VEC4_UINT16
		4,	// VEC2_INT16
		6,	// VEC3_INT16
		8,	// VEC4_INT16
		4,	// VEC2_FLOAT16
		6,	// VEC3_FLOAT16
		8,	// VEC4_FLOAT16
		8,	// VEC2_UINT32
		12,	// VEC3_UINT32
		16,	// VEC4_UINT32
		8,	// VEC2_INT32
		12,	// VEC3_INT32
		16,	// VEC4_INT32
		8,	// VEC2_FLOAT32
		12,	// VEC3_FLOAT32
		16,	// VEC4_FLOAT32
		16,	// VEC2_UINT64
		24,	// VEC3_UINT64
		32,	// VEC4_UINT64
		16,	// VEC2_INT64
		24,	// VEC3_INT64
		32,	// VEC4_INT64
		16,	// VEC2_FLOAT64
		24,	// VEC3_FLOAT64
		32,	// VEC4_FLOAT64
	};

	return sizeTable[DE_ENUM_INDEX(type)];
}

static deUint32 getElementCount(COMPOSITE_DATA_TYPE type)
{
	static const deUint32 elemCountTable[DE_ENUM_COUNT(CompositeDataTypes)] = {
		2,	// VEC2_UINT8
		3,	// VEC3_UINT8
		4,	// VEC4_UINT8
		2,	// VEC2_INT8
		3,	// VEC3_INT8
		4,	// VEC4_INT8
		2,	// VEC2_UINT16
		3,	// VEC3_UINT16
		4,	// VEC4_UINT16
		2,	// VEC2_INT16
		3,	// VEC3_INT16
		4,	// VEC4_INT16
		2,	// VEC2_FLOAT16
		3,	// VEC3_FLOAT16
		4,	// VEC4_FLOAT16
		2,	// VEC2_UINT32
		3,	// VEC3_UINT32
		4,	// VEC4_UINT32
		2,	// VEC2_INT32
		3,	// VEC3_INT32
		4,	// VEC4_INT32
		2,	// VEC2_FLOAT32
		3,	// VEC3_FLOAT32
		4,	// VEC4_FLOAT32
		2,	// VEC2_UINT64
		3,	// VEC3_UINT64
		4,	// VEC4_UINT64
		2,	// VEC2_INT64
		3,	// VEC3_INT64
		4,	// VEC4_INT64
		2,	// VEC2_FLOAT64
		3,	// VEC3_FLOAT64
		4,	// VEC4_FLOAT64
	};

	return elemCountTable[DE_ENUM_INDEX(type)];
}

static DATA_TYPE getCompositeBaseDataType(COMPOSITE_DATA_TYPE type)
{
	static const DATA_TYPE typeTable[DE_ENUM_COUNT(CompositeDataTypes)] = {
		DataTypes::UINT8,	// VEC2_UINT8
		DataTypes::UINT8,	// VEC3_UINT8
		DataTypes::UINT8,	// VEC4_UINT8
		DataTypes::INT8,	// VEC2_INT8
		DataTypes::INT8,	// VEC3_INT8
		DataTypes::INT8,	// VEC4_INT8
		DataTypes::UINT16,	// VEC2_UINT16
		DataTypes::UINT16,	// VEC3_UINT16
		DataTypes::UINT16,	// VEC4_UINT16
		DataTypes::INT16,	// VEC2_INT16
		DataTypes::INT16,	// VEC3_INT16
		DataTypes::INT16,	// VEC4_INT16
		DataTypes::FLOAT16,	// VEC2_FLOAT16
		DataTypes::FLOAT16,	// VEC3_FLOAT16
		DataTypes::FLOAT16,	// VEC4_FLOAT16
		DataTypes::UINT32,	// VEC2_UINT32
		DataTypes::UINT32,	// VEC3_UINT32
		DataTypes::UINT32,	// VEC4_UINT32
		DataTypes::INT32,	// VEC2_INT32
		DataTypes::INT32,	// VEC3_INT32
		DataTypes::INT32,	// VEC4_INT32
		DataTypes::FLOAT32,	// VEC2_FLOAT32
		DataTypes::FLOAT32,	// VEC3_FLOAT32
		DataTypes::FLOAT32,	// VEC4_FLOAT32
		DataTypes::UINT64,	// VEC2_UINT64
		DataTypes::UINT64,	// VEC3_UINT64
		DataTypes::UINT64,	// VEC4_UINT64
		DataTypes::INT64,	// VEC2_INT64
		DataTypes::INT64,	// VEC3_INT64
		DataTypes::INT64,	// VEC4_INT64
		DataTypes::FLOAT64,	// VEC2_FLOAT64
		DataTypes::FLOAT64,	// VEC3_FLOAT64
		DataTypes::FLOAT64,	// VEC4_FLOAT64
	};

	return typeTable[DE_ENUM_INDEX(type)];
}

static std::vector<DATA_TYPE> getSameSizeBaseDataType(DATA_TYPE type)
{
	static const std::vector<DATA_TYPE> sameSizeDataTable[DE_ENUM_COUNT(DataTypes)] = {
		{	DataTypes::INT8							},	// UINT8
		{	DataTypes::UINT8						},	// INT8
		{	DataTypes::INT16,	DataTypes::FLOAT16	},	// UINT16
		{	DataTypes::UINT16,	DataTypes::FLOAT16	},	// INT16
		{	DataTypes::UINT16,	DataTypes::INT16	},	// FLOAT16
		{	DataTypes::INT32,	DataTypes::FLOAT32	},	// UINT32
		{	DataTypes::UINT32,	DataTypes::FLOAT32	},	// INT32
		{	DataTypes::UINT32,	DataTypes::INT32	},	// FLOAT32
		{	DataTypes::INT64,	DataTypes::FLOAT64	},	// UINT64
		{	DataTypes::UINT64,	DataTypes::FLOAT64	},	// INT64
		{	DataTypes::UINT64,	DataTypes::INT64	},	// FLOAT64
	};

	return sameSizeDataTable[DE_ENUM_INDEX(type)];
}

static std::vector<DATA_TYPE> getSameSizeBaseDataType(COMPOSITE_DATA_TYPE type)
{
	static const std::vector<DATA_TYPE> sameSizeDataTable[DE_ENUM_COUNT(CompositeDataTypes)] = {
		{	DataTypes::UINT16,	DataTypes::INT16,	DataTypes::FLOAT16	},	// VEC2_UINT8
		{},																	// VEC3_UINT8
		{	DataTypes::UINT32,	DataTypes::INT32,	DataTypes::FLOAT32	},	// VEC4_UINT8
		{	DataTypes::UINT16,	DataTypes::INT16,	DataTypes::FLOAT16	},	// VEC2_INT8
		{},																	// VEC3_INT8
		{	DataTypes::UINT32,	DataTypes::INT32,	DataTypes::FLOAT32	},	// VEC4_INT8
		{	DataTypes::UINT32,	DataTypes::INT32,	DataTypes::FLOAT32	},	// VEC2_UINT16
		{},																	// VEC3_UINT16
		{	DataTypes::UINT64,	DataTypes::INT64,	DataTypes::FLOAT64	},	// VEC4_UINT16
		{	DataTypes::UINT32,	DataTypes::INT32,	DataTypes::FLOAT32	},	// VEC2_INT16
		{},																	// VEC3_INT16
		{	DataTypes::UINT64,	DataTypes::INT64,	DataTypes::FLOAT64	},	// VEC4_INT16
		{	DataTypes::UINT32,	DataTypes::INT32,	DataTypes::FLOAT32	},	// VEC2_FLOAT16
		{},																	// VEC3_FLOAT16
		{	DataTypes::UINT64,	DataTypes::INT64,	DataTypes::FLOAT64	},	// VEC4_FLOAT16
		{	DataTypes::UINT64,	DataTypes::INT64,	DataTypes::FLOAT64	},	// VEC2_UINT32
		{},																	// VEC3_UINT32
		{},																	// VEC4_UINT32
		{	DataTypes::UINT64,	DataTypes::INT64,	DataTypes::FLOAT64	},	// VEC2_INT32
		{},																	// VEC3_INT32
		{},																	// VEC4_INT32
		{	DataTypes::UINT64,	DataTypes::INT64,	DataTypes::FLOAT64	},	// VEC2_FLOAT32
		{},																	// VEC3_FLOAT32
		{},																	// VEC4_FLOAT32
		{},																	// VEC2_UINT64
		{},																	// VEC3_UINT64
		{},																	// VEC4_UINT64
		{},																	// VEC2_INT64
		{},																	// VEC3_INT64
		{},																	// VEC4_INT64
		{},																	// VEC2_FLOAT64
		{},																	// VEC3_FLOAT64
		{},																	// VEC4_FLOAT64
	};

	return sameSizeDataTable[DE_ENUM_INDEX(type)];
}

static std::vector<COMPOSITE_DATA_TYPE> getSameSizeCompositeType(DATA_TYPE type)
{
	static const std::vector<COMPOSITE_DATA_TYPE> sameSizeDataTable[DE_ENUM_COUNT(DataTypes)] = {
		{},																												// UINT8
		{},																												// INT8
		{	CompositeDataTypes::VEC2_UINT8,		CompositeDataTypes::VEC2_INT8										},	// UINT16
		{	CompositeDataTypes::VEC2_UINT8,		CompositeDataTypes::VEC2_INT8										},	// INT16
		{	CompositeDataTypes::VEC2_UINT8,		CompositeDataTypes::VEC2_INT8										},	// FLOAT16
		{	CompositeDataTypes::VEC4_UINT8,		CompositeDataTypes::VEC4_INT8,
			CompositeDataTypes::VEC2_UINT16,	CompositeDataTypes::VEC2_INT16,	CompositeDataTypes::VEC2_FLOAT16	},	// UINT32
		{	CompositeDataTypes::VEC4_UINT8,		CompositeDataTypes::VEC4_INT8,
			CompositeDataTypes::VEC2_UINT16,	CompositeDataTypes::VEC2_INT16,	CompositeDataTypes::VEC2_FLOAT16	},	// INT32
		{	CompositeDataTypes::VEC4_UINT8,		CompositeDataTypes::VEC4_INT8,
			CompositeDataTypes::VEC2_UINT16,	CompositeDataTypes::VEC2_INT16,	CompositeDataTypes::VEC2_FLOAT16	},	// FLOAT32
		{	CompositeDataTypes::VEC4_UINT16,	CompositeDataTypes::VEC4_INT16,	CompositeDataTypes::VEC4_FLOAT16,
			CompositeDataTypes::VEC2_UINT32,	CompositeDataTypes::VEC2_INT32,	CompositeDataTypes::VEC2_FLOAT32	},	// UINT64
		{	CompositeDataTypes::VEC4_UINT16,	CompositeDataTypes::VEC4_INT16,	CompositeDataTypes::VEC4_FLOAT16,
			CompositeDataTypes::VEC2_UINT32,	CompositeDataTypes::VEC2_INT32,	CompositeDataTypes::VEC2_FLOAT32	},	// INT64
		{	CompositeDataTypes::VEC4_UINT16,	CompositeDataTypes::VEC4_INT16,	CompositeDataTypes::VEC4_FLOAT16,
			CompositeDataTypes::VEC2_UINT32,	CompositeDataTypes::VEC2_INT32,	CompositeDataTypes::VEC2_FLOAT32	},	// FLOAT64
	};

	return sameSizeDataTable[DE_ENUM_INDEX(type)];
}

deBool isReadOnly(CONTAINER_TYPE type)
{
	static const deBool translateTable[DE_ENUM_COUNT(ContainerTypes)] = {
		DE_FALSE,	// STORAGE_BUFFER,
		DE_TRUE,	// UNIFORM,
		DE_TRUE,	// PUSH_CONSTANT,
		DE_FALSE,	// WORKGROUP,
	};

	return translateTable[DE_ENUM_INDEX(type)];
}

const char* toString(DATA_TYPE type)
{
	static const char* const translateTable[DE_ENUM_COUNT(DataTypes)] = {
		"uint8",	// UINT8
		"int8",		// INT8
		"uint16",	// UINT16
		"int16",	// INT16
		"float16",	// FLOAT16
		"uint32",	// UINT32
		"int32",	// INT32
		"float32",	// FLOAT32
		"uint64",	// UINT64
		"int64",	// INT64
		"float64",	// FLOAT64
	};

	return translateTable[DE_ENUM_INDEX(type)];
}

const char* toString(COMPOSITE_DATA_TYPE type)
{
	static const char* const translateTable[DE_ENUM_COUNT(CompositeDataTypes)] = {
		"vec2_uint8",	// VEC2_UINT8
		"vec3_uint8",	// VEC3_UINT8
		"vec4_uint8",	// VEC4_UINT8
		"vec2_int8",	// VEC2_INT8
		"vec3_int8",	// VEC3_INT8
		"vec4_int8",	// VEC4_INT8
		"vec2_uint16",	// VEC2_UINT16
		"vec3_uint16",	// VEC3_UINT16
		"vec4_uint16",	// VEC4_UINT16
		"vec2_int16",	// VEC2_INT16
		"vec3_int16",	// VEC3_INT16
		"vec4_int16",	// VEC4_INT16
		"vec2_float16",	// VEC2_FLOAT16
		"vec3_float16",	// VEC3_FLOAT16
		"vec4_float16",	// VEC4_FLOAT16
		"vec2_uint32",	// VEC2_UINT32
		"vec3_uint32",	// VEC3_UINT32
		"vec4_uint32",	// VEC4_UINT32
		"vec2_int32",	// VEC2_INT32
		"vec3_int32",	// VEC3_INT32
		"vec4_int32",	// VEC4_INT32
		"vec2_float32",	// VEC2_FLOAT32
		"vec3_float32",	// VEC3_FLOAT32
		"vec4_float32",	// VEC4_FLOAT32
		"vec2_uint64",	// VEC2_UINT64
		"vec3_uint64",	// VEC3_UINT64
		"vec4_uint64",	// VEC4_UINT64
		"vec2_int64",	// VEC2_INT64
		"vec3_int64",	// VEC3_INT64
		"vec4_int64",	// VEC4_INT64
		"vec2_float64",	// VEC2_FLOAT64
		"vec3_float64",	// VEC3_FLOAT64
		"vec4_float64",	// VEC4_FLOAT64
	};

	return translateTable[DE_ENUM_INDEX(type)];
}

const char* toString(ATOMIC_TEST_CASE testCase)
{
	static const char* const translateTable[DE_ENUM_COUNT(AtomicTestCases)] = {
		"op_atomic_load",				// OP_ATOMIC_LOAD
		"op_atomic_store",				// OP_ATOMIC_STORE
		"op_atomic_exchange",			// OP_ATOMIC_EXCHANGE
		"op_atomic_compare_exchange",	// OP_ATOMIC_COMPARE_EXCHANGE
		"op_atomic_increment",			// OP_ATOMIC_INCREMENT
		"op_atomic_decrement",			// OP_ATOMIC_DECREMENT
		"op_atomic_add",				// OP_ATOMIC_ADD
		"op_atomic_sub",				// OP_ATOMIC_SUB
		"op_atomic_min",				// OP_ATOMIC_MIN
		"op_atomic_max",				// OP_ATOMIC_MAX
		"op_atomic_and",				// OP_ATOMIC_AND
		"op_atomic_or",					// OP_ATOMIC_OR
		"op_atomic_xor",				// OP_ATOMIC_XOR
	};

	return translateTable[DE_ENUM_INDEX(testCase)];
}

const char* toString(POINTER_TEST_CASE testCase)
{
	static const char* const translateTable[DE_ENUM_COUNT(PointerTestCases)] = {
		"op_bitcast_form_untyped",	// OP_BITCAST_FROM_UNTYPED_PHYSICAL_STORAGE
		"op_bitcast_to_untyped",	// OP_BITCAST_TO_UNTYPED_PHYSICAL_STORAGE
		"op_select",				// OP_SELECT_PHYSICAL_STORAGE
		"op_phi",					// OP_PHI_PHYSICAL_STORAGE
		"op_ptr_access_chain",		// OP_PTR_ACCESS_CHAIN_PHYSICAL_STORAGE
		"op_function_call",			// FUNCTION_PARAMETERS_PHYSICAL_STORAGE
		"op_select",				// OP_SELECT_VARIABLE_PTR
		"op_phi",					// OP_PHI_VARIABLE_PTR
		"op_ptr_access_chain",		// OP_PTR_ACCESS_CHAIN_VARIABLE_PTR
		"op_ptr_equal",				// OP_PTR_EQUAL_VARIABLE_PTR
		"op_ptr_not_equal",			// OP_PTR_NOT_EQUAL_VARIABLE_PTR
		"op_ptr_diff",				// OP_PTR_DIFF_VARIABLE_PTR
		"op_function_call",			// OP_FUNCTION_CALL_VARIABLE_PTR
		"function_variable",		// FUNCTION_VARIABLE_VARIABLE_PTR
		"private_variable",			// PRIVATE_VARIABLE_VARIABLE_PTR
		"multiple_access_chains",	// MULTIPLE_ACCESS_CHAINS_VARIABLE_PTR
	};

	return translateTable[DE_ENUM_INDEX(testCase)];
}

const char* toString(WORKGROUP_TEST_CASE testCase)
{
	static const char* const translateTable[DE_ENUM_COUNT(WorkgroupTestCases)] = {
		"aliased",		// ALIASED
		"not_aliased",	// NOT_ALIASED
	};

	return translateTable[DE_ENUM_INDEX(testCase)];
}

const char* toString(OPERATION_TYPE opType)
{
	static const char* const translateTable[DE_ENUM_COUNT(OperationTypes)] = {
		"normal",	// NORMAL
		"atomic",	// ATOMIC
	};

	return translateTable[DE_ENUM_INDEX(opType)];
}

const char* toString(CONTAINER_TYPE contType)
{
	static const char* const translateTable[DE_ENUM_COUNT(ContainerTypes)] = {
		"storage_buffer",	// STORAGE_BUFFER
		"uniform",			// UNIFORM
		"push_constant",	// PUSH_CONSTANT
		"workgroup",		// WORKGROUP
	};

	return translateTable[DE_ENUM_INDEX(contType)];
}

const char* toString(COPY_OPERATION_TYPE type)
{
	static const char* const translateTable[DE_ENUM_COUNT(CopyOperationTypes)] = {
		"copy_object",			// COPY_OBJECT
		"copy_memory",			// COPY_MEMORY
		"copy_memory_sized",	// COPY_MEMORY_SIZED
	};

	return translateTable[DE_ENUM_INDEX(type)];
}

const char* toString(MATRIX_TYPE type)
{
	static const char* const translateTable[DE_ENUM_COUNT(MatrixTypes)] = {
		"a",			// A
		"b",			// B
		"accumulator",	// ACCUMULATOR
	};

	return translateTable[DE_ENUM_INDEX(type)];
}

const char* toString(MATRIX_LAYOUT layout)
{
	static const char* const translateTable[DE_ENUM_COUNT(MatrixLayouts)] = {
		"row_major",	// ROW_MAJOR
		"col_major",	// COL_MAJOR
	};

	return translateTable[DE_ENUM_INDEX(layout)];
}

const char* getCapability(DATA_TYPE type)
{
	static const char* const translateTable[DE_ENUM_COUNT(DataTypes)] = {
		"OpCapability Int8",	// UINT8
		"OpCapability Int8",	// INT8
		"OpCapability Int16",	// UINT16
		"OpCapability Int16",	// INT16
		"OpCapability Float16",	// FLOAT16
		"",						// UINT32
		"",						// INT32
		"",						// FLOAT32
		"OpCapability Int64",	// UINT64
		"OpCapability Int64",	// INT64
		"OpCapability Float64",	// FLOAT64
	};

	return translateTable[DE_ENUM_INDEX(type)];
}

const char* getCapability(COMPOSITE_DATA_TYPE type)
{
	static const char* const translateTable[DE_ENUM_COUNT(CompositeDataTypes)] = {
		"OpCapability Int8",	// VEC2_UINT8
		"OpCapability Int8",	// VEC3_UINT8
		"OpCapability Int8",	// VEC4_UINT8
		"OpCapability Int8",	// VEC2_INT8
		"OpCapability Int8",	// VEC3_INT8
		"OpCapability Int8",	// VEC4_INT8
		"OpCapability Int16",	// VEC2_UINT16
		"OpCapability Int16",	// VEC3_UINT16
		"OpCapability Int16",	// VEC4_UINT16
		"OpCapability Int16",	// VEC2_INT16
		"OpCapability Int16",	// VEC3_INT16
		"OpCapability Int16",	// VEC4_INT16
		"OpCapability Float16",	// VEC2_FLOAT16
		"OpCapability Float16",	// VEC3_FLOAT16
		"OpCapability Float16",	// VEC4_FLOAT16
		"",						// VEC2_UINT32
		"",						// VEC3_UINT32
		"",						// VEC4_UINT32
		"",						// VEC2_INT32
		"",						// VEC3_INT32
		"",						// VEC4_INT32
		"",						// VEC2_FLOAT32
		"",						// VEC3_FLOAT32
		"",						// VEC4_FLOAT32
		"OpCapability Int64",	// VEC2_UINT64
		"OpCapability Int64",	// VEC3_UINT64
		"OpCapability Int64",	// VEC4_UINT64
		"OpCapability Int64",	// VEC2_INT64
		"OpCapability Int64",	// VEC3_INT64
		"OpCapability Int64",	// VEC4_INT64
		"OpCapability Float64",	// VEC2_FLOAT64
		"OpCapability Float64",	// VEC3_FLOAT64
		"OpCapability Float64",	// VEC4_FLOAT64
	};

	return translateTable[DE_ENUM_INDEX(type)];
}

const char* getDeclaration(DATA_TYPE type)
{
	static const char* const translateTable[DE_ENUM_COUNT(DataTypes)] = {
		"OpTypeInt    8 0",	// UINT8
		"OpTypeInt    8 1",	// INT8
		"OpTypeInt   16 0",	// UINT16
		"OpTypeInt   16 1",	// INT16
		"OpTypeFloat 16",	// FLOAT16
		"OpTypeInt   32 0",	// UINT32
		"OpTypeInt   32 1",	// INT32
		"OpTypeFloat 32",	// FLOAT32
		"OpTypeInt   64 0",	// UINT64
		"OpTypeInt   64 1",	// INT64
		"OpTypeFloat 64",	// FLOAT64
	};

	return translateTable[DE_ENUM_INDEX(type)];
}

const char* getDeclaration(COMPOSITE_DATA_TYPE type)
{
	static const char* const translateTable[DE_ENUM_COUNT(CompositeDataTypes)] = {
		"OpTypeVector %uint8   2",	// VEC2_UINT8
		"OpTypeVector %uint8   3",	// VEC3_UINT8
		"OpTypeVector %uint8   4",	// VEC4_UINT8
		"OpTypeVector %int8    2",	// VEC2_INT8
		"OpTypeVector %int8    3",	// VEC3_INT8
		"OpTypeVector %int8    4",	// VEC4_INT8
		"OpTypeVector %uint16  2",	// VEC2_UINT16
		"OpTypeVector %uint16  3",	// VEC3_UINT16
		"OpTypeVector %uint16  4",	// VEC4_UINT16
		"OpTypeVector %int16   2",	// VEC2_INT16
		"OpTypeVector %int16   3",	// VEC3_INT16
		"OpTypeVector %int16   4",	// VEC4_INT16
		"OpTypeVector %float16 2",	// VEC2_FLOAT16
		"OpTypeVector %float16 3",	// VEC3_FLOAT16
		"OpTypeVector %float16 4",	// VEC4_FLOAT16
		"OpTypeVector %uint32  2",	// VEC2_UINT32
		"OpTypeVector %uint32  3",	// VEC3_UINT32
		"OpTypeVector %uint32  4",	// VEC4_UINT32
		"OpTypeVector %int32   2",	// VEC2_INT32
		"OpTypeVector %int32   3",	// VEC3_INT32
		"OpTypeVector %int32   4",	// VEC4_INT32
		"OpTypeVector %float32 2",	// VEC2_FLOAT32
		"OpTypeVector %float32 3",	// VEC3_FLOAT32
		"OpTypeVector %float32 4",	// VEC4_FLOAT32
		"OpTypeVector %uint64  2",	// VEC2_UINT64
		"OpTypeVector %uint64  3",	// VEC3_UINT64
		"OpTypeVector %uint64  4",	// VEC4_UINT64
		"OpTypeVector %int64   2",	// VEC2_INT64
		"OpTypeVector %int64   3",	// VEC3_INT64
		"OpTypeVector %int64   4",	// VEC4_INT64
		"OpTypeVector %float64 2",	// VEC2_FLOAT64
		"OpTypeVector %float64 3",	// VEC3_FLOAT64
		"OpTypeVector %float64 4",	// VEC4_FLOAT64
	};

	return translateTable[DE_ENUM_INDEX(type)];
}

const char* getStorageClass(CONTAINER_TYPE type)
{
	static const char* const translateTable[DE_ENUM_COUNT(ContainerTypes)] = {
		"StorageBuffer",	// STORAGE_BUFFER
		"Uniform",			// UNIFORM
		"PushConstant",		// PUSH_CONSTANT
		"Workgroup",		// WORKGROUP
	};

	return translateTable[DE_ENUM_INDEX(type)];
}

const char* getAtomicAddOperator(DATA_TYPE type)
{
	static const char* const translateTable[DE_ENUM_COUNT(DataTypes)] = {
		"OpAtomicIAdd",		// UINT8
		"OpAtomicIAdd",		// INT8
		"OpAtomicIAdd",		// UINT16
		"OpAtomicIAdd",		// INT16
		"OpAtomicFAddEXT",	// FLOAT16
		"OpAtomicIAdd",		// UINT32
		"OpAtomicIAdd",		// INT32
		"OpAtomicFAddEXT",	// FLOAT32
		"OpAtomicIAdd",		// UINT64
		"OpAtomicIAdd",		// INT64
		"OpAtomicFAddEXT",	// FLOAT64
	};

	return translateTable[DE_ENUM_INDEX(type)];
}

const char* getAtomicSubtractOperator(DATA_TYPE type)
{
	static const char* const translateTable[DE_ENUM_COUNT(DataTypes)] = {
		"OpAtomicISub",	// UINT8
		"OpAtomicISub",	// INT8
		"OpAtomicISub",	// UINT16
		"OpAtomicISub",	// INT16
		"",				// FLOAT16
		"OpAtomicISub",	// UINT32
		"OpAtomicISub",	// INT32
		"",				// FLOAT32
		"OpAtomicISub",	// UINT64
		"OpAtomicISub",	// INT64
		"",				// FLOAT64
	};

	return translateTable[DE_ENUM_INDEX(type)];
}

const char* getAtomicIncrementOperator(DATA_TYPE type)
{
	static const char* const translateTable[DE_ENUM_COUNT(DataTypes)] = {
		"OpAtomicIIncrement",	// UINT8
		"OpAtomicIIncrement",	// INT8
		"OpAtomicIIncrement",	// UINT16
		"OpAtomicIIncrement",	// INT16
		"",						// FLOAT16
		"OpAtomicIIncrement",	// UINT32
		"OpAtomicIIncrement",	// INT32
		"",						// FLOAT32
		"OpAtomicIIncrement",	// UINT64
		"OpAtomicIIncrement",	// INT64
		"",						// FLOAT64
	};

	return translateTable[DE_ENUM_INDEX(type)];
}

const char* getAtomicDecrementOperator(DATA_TYPE type)
{
	static const char* const translateTable[DE_ENUM_COUNT(DataTypes)] = {
		"OpAtomicIDecrement",	// UINT8
		"OpAtomicIDecrement",	// INT8
		"OpAtomicIDecrement",	// UINT16
		"OpAtomicIDecrement",	// INT16
		"",						// FLOAT16
		"OpAtomicIDecrement",	// UINT32
		"OpAtomicIDecrement",	// INT32
		"",						// FLOAT32
		"OpAtomicIDecrement",	// UINT64
		"OpAtomicIDecrement",	// INT64
		"",						// FLOAT64
	};

	return translateTable[DE_ENUM_INDEX(type)];
}

const char* getAtomicMinOperator(DATA_TYPE type)
{
	static const char* const translateTable[DE_ENUM_COUNT(DataTypes)] = {
		"OpAtomicUMin",	// UINT8
		"OpAtomicSMin",	// INT8
		"OpAtomicUMin",	// UINT16
		"OpAtomicSMin",	// INT16
		"",				// FLOAT16
		"OpAtomicUMin",	// UINT32
		"OpAtomicSMin",	// INT32
		"",				// FLOAT32
		"OpAtomicUMin",	// UINT64
		"OpAtomicSMin",	// INT64
		"",				// FLOAT64
	};

	return translateTable[DE_ENUM_INDEX(type)];
}

const char* getAtomicMaxOperator(DATA_TYPE type)
{
	static const char* const translateTable[DE_ENUM_COUNT(DataTypes)] = {
		"OpAtomicUMax",	// UINT8
		"OpAtomicSMax",	// INT8
		"OpAtomicUMax",	// UINT16
		"OpAtomicSMax",	// INT16
		"",				// FLOAT16
		"OpAtomicUMax",	// UINT32
		"OpAtomicSMax",	// INT32
		"",				// FLOAT32
		"OpAtomicUMax",	// UINT64
		"OpAtomicSMax",	// INT64
		"",				// FLOAT64
	};

	return translateTable[DE_ENUM_INDEX(type)];
}

const char* getAtomicAndOperator(DATA_TYPE type)
{
	static const char* const translateTable[DE_ENUM_COUNT(DataTypes)] = {
		"OpAtomicAnd",	// UINT8
		"OpAtomicAnd",	// INT8
		"OpAtomicAnd",	// UINT16
		"OpAtomicAnd",	// INT16
		"",				// FLOAT16
		"OpAtomicAnd",	// UINT32
		"OpAtomicAnd",	// INT32
		"",				// FLOAT32
		"OpAtomicAnd",	// UINT64
		"OpAtomicAnd",	// INT64
		"",				// FLOAT64
	};

	return translateTable[DE_ENUM_INDEX(type)];
}
const char* getAtomicExchangeOperator(DATA_TYPE type)
{
	static const char* const translateTable[DE_ENUM_COUNT(DataTypes)] = {
		"OpAtomicExchange",	// UINT8
		"OpAtomicExchange",	// INT8
		"OpAtomicExchange",	// UINT16
		"OpAtomicExchange",	// INT16
		"OpAtomicExchange",	// FLOAT16
		"OpAtomicExchange",	// UINT32
		"OpAtomicExchange",	// INT32
		"OpAtomicExchange",	// FLOAT32
		"OpAtomicExchange",	// UINT64
		"OpAtomicExchange",	// INT64
		"OpAtomicExchange",	// FLOAT64
	};

	return translateTable[DE_ENUM_INDEX(type)];
}

const char* getAtomicCompareExchangeOperator(DATA_TYPE type)
{
	static const char* const translateTable[DE_ENUM_COUNT(DataTypes)] = {
		"OpAtomicCompareExchange",	// UINT8
		"OpAtomicCompareExchange",	// INT8
		"OpAtomicCompareExchange",	// UINT16
		"OpAtomicCompareExchange",	// INT16
		"",							// FLOAT16
		"OpAtomicCompareExchange",	// UINT32
		"OpAtomicCompareExchange",	// INT32
		"",							// FLOAT32
		"OpAtomicCompareExchange",	// UINT64
		"OpAtomicCompareExchange",	// INT64
		"",							// FLOAT64
	};

	return translateTable[DE_ENUM_INDEX(type)];
}

static deInt32 getSignedUnsignedMinMaxTestValue(DATA_TYPE type)
{
	static const deInt32 translateTable[DE_ENUM_COUNT(DataTypes)] = {
		 1,			// UINT8
		-1,			// INT8
		 1,			// UINT16
		-1,			// INT16
		INT32_MAX,	// FLOAT16
		 1,			// UINT32
		-1,			// INT32
		INT32_MAX,	// FLOAT32
		 1,			// UINT64
		-1,			// INT64
		INT32_MAX,	// FLOAT64
	};

	return translateTable[DE_ENUM_INDEX(type)];
}

std::string getResourceDecorations(CONTAINER_TYPE containerType, DATA_TYPE dataType, deUint32 numWorkgroup)
{
	std::string	decorations	=  "OpDecorate %array_";
				decorations	+= toString(dataType);
				decorations	+= "_";
				decorations	+= std::to_string(numWorkgroup);
				decorations	+= " ArrayStride ";
				decorations	+= containerType == ContainerTypes::UNIFORM ? std::to_string(16) : std::to_string(getSizeInBytes(dataType));
				decorations	+= "\n";

	if (containerType == ContainerTypes::PUSH_CONSTANT)
	{
		decorations += std::string(
			"OpDecorate %output_data_var DescriptorSet 0\n"
			"OpDecorate %output_data_var Binding       0\n"
		);
	}
	else
	{
		decorations += std::string(
			"OpDecorate %input_data_untyped_var DescriptorSet 0\n"
			"OpDecorate %input_data_untyped_var Binding       0\n"

			"OpDecorate %output_data_var        DescriptorSet 0\n"
			"OpDecorate %output_data_var        Binding       1\n"
		);
	}

	return decorations;
}

std::string getSameSizeResourceDecorations(CONTAINER_TYPE containerType, DATA_TYPE dataType1, DATA_TYPE dataType2, deUint32 numWorkgroup)
{
	std::string	decorations	=  "OpDecorate %array_";
				decorations	+= toString(dataType1);
				decorations	+= "_";
				decorations	+= std::to_string(numWorkgroup);
				decorations	+= " ArrayStride ";
				decorations	+= containerType == ContainerTypes::UNIFORM ? std::to_string(16) : std::to_string(getSizeInBytes(dataType1));
				decorations	+= "\n";

				decorations	+= "OpDecorate %array_";
				decorations	+= toString(dataType2);
				decorations += "_";
				decorations	+= std::to_string(numWorkgroup);
				decorations	+= " ArrayStride ";
				decorations	+= containerType == ContainerTypes::UNIFORM ? std::to_string(16) : std::to_string(getSizeInBytes(dataType1));
				decorations	+= "\n";

	if (containerType == ContainerTypes::PUSH_CONSTANT)
	{
		decorations += std::string(
			"OpDecorate %output_data_var DescriptorSet 0\n"
			"OpDecorate %output_data_var Binding       0\n"
		);
	}
	else
	{
		decorations += std::string(
			"OpDecorate %input_data_untyped_var DescriptorSet 0\n"
			"OpDecorate %input_data_untyped_var Binding       0\n"

			"OpDecorate %output_data_var        DescriptorSet 0\n"
			"OpDecorate %output_data_var        Binding       1\n"
		);
	}

	return decorations;
}

std::string getScalarVectorResourceDecorations(CONTAINER_TYPE containerType)
{
	std::string	decorations	= "";

	if (containerType == ContainerTypes::PUSH_CONSTANT)
	{
		decorations += std::string(
			"OpDecorate %output_data_var DescriptorSet 0\n"
			"OpDecorate %output_data_var Binding       0\n"
		);
	}
	else
	{
		decorations += std::string(
			"OpDecorate %input_data_untyped_var DescriptorSet 0\n"
			"OpDecorate %input_data_untyped_var Binding       0\n"

			"OpDecorate %output_data_var        DescriptorSet 0\n"
			"OpDecorate %output_data_var        Binding       1\n"
		);
	}

	return decorations;
}

static deUint32 getMatrixBinaryUse(MATRIX_TYPE type)
{
	return DE_ENUM_INDEX(type);
}

static deUint32 getMatrixBinaryLayout(MATRIX_LAYOUT layout)
{
	return DE_ENUM_INDEX(layout);
}

static void adjustSpecForMemoryModel(ComputeShaderSpec& spec, std::map<std::string, std::string>& specMap, MEMORY_MODEL_TYPE memModel)
{
	switch (memModel)
	{
	case MemoryModelTypes::VULKAN:
	{
		specMap["memModelCap"]	= "OpCapability VulkanMemoryModel\nOpCapability VulkanMemoryModelDeviceScopeKHR";
		specMap["memModelExt"]	= "OpExtension \"SPV_KHR_vulkan_memory_model\"";
		specMap["memModelOp"]	= "OpMemoryModel Logical Vulkan";

		spec.extensions.push_back("VK_KHR_vulkan_memory_model");

		break;
	}
	case MemoryModelTypes::GLSL:
	{
		specMap["memModelOp"]	= "OpMemoryModel Logical GLSL450";

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

static void adjustSpecForDataTypes(ComputeShaderSpec& spec, std::map<std::string, std::string>& specMap, DATA_TYPE dataType)
{
	switch (dataType)
	{
	case DataTypes::UINT8:
	case DataTypes::INT8:
	{
		specMap["baseTypeCap"]									= "OpCapability Int8";
		spec.requestedVulkanFeatures.extFloat16Int8.shaderInt8	= VK_TRUE;
		break;
	}
	case DataTypes::UINT16:
	case DataTypes::INT16:
	{
		specMap["baseTypeCap"]									= "OpCapability Int16";
		spec.requestedVulkanFeatures.coreFeatures.shaderInt16	= VK_TRUE;
		break;
	}
	case DataTypes::FLOAT16:
	{
		specMap["baseTypeCap"]										= "OpCapability Float16";
		spec.requestedVulkanFeatures.extFloat16Int8.shaderFloat16	= VK_TRUE;
		break;
	}
	case DataTypes::UINT32:
	case DataTypes::INT32:
	case DataTypes::FLOAT32:
		break;
	case DataTypes::FLOAT64:
	{
		specMap["baseTypeCap"]	= "OpCapability Float64";
		spec.requestedVulkanFeatures.coreFeatures.shaderFloat64	= VK_TRUE;
		break;
	}
	case DataTypes::UINT64:
	case DataTypes::INT64:
	{
		specMap["baseTypeCap"]	= "OpCapability Int64";
		spec.requestedVulkanFeatures.coreFeatures.shaderInt64	= VK_TRUE;
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

static void adjustSpecForAtomicOperations(ComputeShaderSpec& spec, std::map<std::string, std::string>& specMap, DATA_TYPE dataType)
{
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
		spec.extensions.push_back("VK_EXT_shader_atomic_float2");	// VK_EXT_shader_atomic_float2 requires VK_EXT_shader_atomic_float to be enabled
		spec.requestedVulkanFeatures.extShaderAtomicFloat2.shaderBufferFloat16Atomics	= VK_TRUE;
		break;
	}
	case DataTypes::FLOAT32:
	{
		spec.extensions.push_back("VK_EXT_shader_atomic_float");
		spec.requestedVulkanFeatures.extShaderAtomicFloat.shaderBufferFloat32Atomics	= VK_TRUE;
		break;
	}
	case DataTypes::FLOAT64:
	{
		spec.extensions.push_back("VK_EXT_shader_atomic_float");
		spec.requestedVulkanFeatures.extShaderAtomicFloat.shaderBufferFloat64Atomics	= VK_TRUE;
		break;
	}
	case DataTypes::UINT64:
	case DataTypes::INT64:
	{
		specMap["atomicCap"]	= "OpCapability Int64Atomics";
		spec.extensions.push_back("VK_KHR_shader_atomic_int64");
		spec.requestedVulkanFeatures.extShaderAtomicInt64.shaderBufferInt64Atomics	= VK_TRUE;
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

static void adjustSpecForAtomicAddOperations(ComputeShaderSpec& spec, std::map<std::string, std::string>& specMap, DATA_TYPE dataType)
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
		specMap["atomicAddExt"] = "OpExtension \"SPV_EXT_shader_atomic_float16_add\"";
		specMap["atomicAddCap"] = "OpCapability AtomicFloat16AddEXT";
		spec.requestedVulkanFeatures.extShaderAtomicFloat2.shaderBufferFloat16AtomicAdd	= VK_TRUE;
		break;
	}
	case DataTypes::FLOAT32:
	{
		specMap["atomicAddExt"] = "OpExtension \"SPV_EXT_shader_atomic_float_add\"";
		specMap["atomicAddCap"] = "OpCapability AtomicFloat32AddEXT";
		spec.requestedVulkanFeatures.extShaderAtomicFloat.shaderBufferFloat32AtomicAdd	= VK_TRUE;
		break;
	}
	case DataTypes::FLOAT64:
	{
		specMap["atomicAddExt"] = "OpExtension \"SPV_EXT_shader_atomic_float_add\"";
		specMap["atomicAddCap"] = "OpCapability AtomicFloat64AddEXT";
		spec.requestedVulkanFeatures.extShaderAtomicFloat.shaderBufferFloat64AtomicAdd	= VK_TRUE;
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

static void adjustSpecForUntypedPointers(ComputeShaderSpec& spec, std::map<std::string, std::string>& specMap)
{
	specMap["storageCap"]	= "OpCapability UntypedPointersKHR";
	spec.requestedVulkanFeatures.extShaderUntypedPointers.shaderUntypedPointers	= VK_TRUE;
}

static void adjustSpecForSmallContainerType(ComputeShaderSpec& spec, std::map<std::string, std::string>& specMap, CONTAINER_TYPE containerType, DATA_TYPE dataType)
{
	switch (dataType)
	{
	case DataTypes::UINT8:
	case DataTypes::INT8:
	{
		specMap["smallStorageExt"]	= "OpExtension \"SPV_KHR_8bit_storage\"";
		switch (containerType)
		{
		case ContainerTypes::STORAGE_BUFFER:
		{
			specMap["smallStorageCap"]											= "OpCapability StorageBuffer8BitAccess\n";
			spec.requestedVulkanFeatures.ext8BitStorage.storageBuffer8BitAccess	= VK_TRUE;
			break;
		}
		case ContainerTypes::UNIFORM:
		{
			specMap["smallStorageCap"]														= "OpCapability UniformAndStorageBuffer8BitAccess\n";
			spec.requestedVulkanFeatures.ext8BitStorage.uniformAndStorageBuffer8BitAccess	= VK_TRUE;
			break;
		}
		case ContainerTypes::PUSH_CONSTANT:
		{
			specMap["smallStorageCap"]											= "OpCapability StoragePushConstant8BitAccess\n";
			spec.requestedVulkanFeatures.ext8BitStorage.storagePushConstant8	= VK_TRUE;
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
		specMap["smallStorageExt"]	= "OpExtension \"SPV_KHR_16bit_storage\"";
		switch (containerType)
		{
		case ContainerTypes::STORAGE_BUFFER:
		{
			specMap["smallStorageCap"]												= "OpCapability StorageBuffer16BitAccess\n";
			spec.requestedVulkanFeatures.ext16BitStorage.storageBuffer16BitAccess	= VK_TRUE;
			break;
		}
		case ContainerTypes::UNIFORM:
		{
			specMap["smallStorageCap"]														= "OpCapability UniformAndStorageBuffer16BitAccess\n";
			spec.requestedVulkanFeatures.ext16BitStorage.uniformAndStorageBuffer16BitAccess	= VK_TRUE;
			break;
		}
		case ContainerTypes::PUSH_CONSTANT:
		{
			specMap["smallStorageCap"]											= "OpCapability StoragePushConstant16BitAccess\n";
			spec.requestedVulkanFeatures.ext16BitStorage.storagePushConstant16	= VK_TRUE;
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

static void adjustSpecForVariablePointers(ComputeShaderSpec& spec, std::map<std::string, std::string>& specMap)
{
	spec.requestedVulkanFeatures.extVariablePointers.variablePointers				= VK_TRUE;
	spec.requestedVulkanFeatures.extVariablePointers.variablePointersStorageBuffer	= VK_TRUE;
	spec.extensions.push_back("VK_KHR_variable_pointers");

	specMap["otherCap"]	= "OpCapability VariablePointersStorageBuffer\n"
						  "OpCapability VariablePointers";
	specMap["otherExt"]	= "OpExtension \"SPV_KHR_variable_pointers\"";
}

static void adjustSpecForPhysicalStorageBuffer(ComputeShaderSpec& spec, std::map<std::string, std::string>& specMap, MEMORY_MODEL_TYPE memModel)
{
	specMap["otherCap"]	= "OpCapability PhysicalStorageBufferAddresses";
	specMap["otherExt"]	= "OpExtension \"SPV_KHR_physical_storage_buffer\"";
	spec.extensions.push_back("VK_KHR_buffer_device_address");

	// Memory model spec needs to be overwritten.
	switch (memModel)
	{
	case MemoryModelTypes::VULKAN:
	{
		specMap["memModelOp"]	= "OpMemoryModel PhysicalStorageBuffer64 Vulkan";
		break;
	}
	case MemoryModelTypes::GLSL:
	{
		specMap["memModelOp"]	= "OpMemoryModel PhysicalStorageBuffer64 GLSL450";
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

static void adjustSpecForWorkgroupMemoryExplicitLayout(ComputeShaderSpec& spec, std::map<std::string, std::string>& specMap)
{
	specMap["otherCap"]	= "OpCapability WorkgroupMemoryExplicitLayoutKHR\n"
						  "OpCapability WorkgroupMemoryExplicitLayout8BitAccessKHR\n"
						  "OpCapability WorkgroupMemoryExplicitLayout16BitAccessKHR\n";
	specMap["otherExt"]	= "OpExtension \"SPV_KHR_workgroup_memory_explicit_layout\"";
	spec.extensions.push_back("VK_KHR_workgroup_memory_explicit_layout");
}

static void adjustSpecForCooperativeMatrix(ComputeShaderSpec& spec, std::map<std::string, std::string>& specMap)
{
	specMap["otherCap"]	= "OpCapability CooperativeMatrixKHR\n";
	specMap["otherExt"]	= "OpExtension \"SPV_KHR_cooperative_matrix\"";
	spec.extensions.push_back("VK_KHR_cooperative_matrix");
}

enum class FillingTypes : deUint8
{
	RANDOM,
	VALUE,
	_ENUM_COUNT
};
using FILLING_TYPE = FillingTypes;

struct FilledResourceDesc
{
	union
	{
		deUint32			seed;
		double				value;
	};
	vk::VkDescriptorType	descriptorType;
	deUint32				elemCount;
	deUint32				padding;
	DATA_TYPE				dataType;
	FILLING_TYPE			fillType;
};

struct AtomicResourceDesc
{
	DATA_TYPE	dataType;
	deUint32	elemCount;
};

static Resource createFilledResource(const FilledResourceDesc& desc)
{
	const DATA_TYPE type = desc.dataType;

	switch (type)
	{
	case DataTypes::UINT8:
	{
		if (desc.fillType == FillingTypes::VALUE)
		{
			const deUint8 converted = static_cast<deUint8>(desc.value);
			return Resource(BufferSp(new Buffer<deUint8>(std::vector<deUint8>(desc.elemCount, converted), desc.padding)), desc.descriptorType);
		}
		else
		{
			de::Random rnd(desc.seed);
			std::vector<deUint8> randoms;
			if (desc.elemCount <= 24)
			{
				randoms = getUint8s(rnd, 24);
				randoms.resize(desc.elemCount);
			}
			else
			{
				randoms = getUint8s(rnd, desc.elemCount);
			}
			return Resource(BufferSp(new Buffer<deUint8>(randoms, desc.padding)), desc.descriptorType);
		}
	}
	case DataTypes::INT8:
	{
		if (desc.fillType == FillingTypes::VALUE)
		{
			const deInt8 converted = static_cast<deInt8>(desc.value);
			return Resource(BufferSp(new Buffer<deInt8>(std::vector<deInt8>(desc.elemCount, converted), desc.padding)), desc.descriptorType);
		}
		else
		{
			de::Random rnd(desc.seed);
			std::vector<deInt8> randoms;
			if (desc.elemCount <= 24)
			{
				randoms = getInt8s(rnd, 24);
				randoms.resize(desc.elemCount);
			}
			else
			{
				randoms = getInt8s(rnd, desc.elemCount);
			}
			return Resource(BufferSp(new Buffer<deInt8>(randoms, desc.padding)), desc.descriptorType);
		}

	}
	case DataTypes::UINT16:
	{
		if (desc.fillType == FillingTypes::VALUE)
		{
			const deUint16 converted = static_cast<deUint16>(desc.value);
			return Resource(BufferSp(new Buffer<deUint16>(std::vector<deUint16>(desc.elemCount, converted), desc.padding)), desc.descriptorType);
		}
		else
		{
			de::Random rnd(desc.seed);
			std::vector<deUint16> randoms;
			if (desc.elemCount <= 24)
			{
				randoms = getUint16s(rnd, 24);
				randoms.resize(desc.elemCount);
			}
			else
			{
				randoms = getUint16s(rnd, desc.elemCount);
			}
			return Resource(BufferSp(new Buffer<deUint16>(randoms, desc.padding)), desc.descriptorType);
		}
	}
	case DataTypes::INT16:
	{
		if (desc.fillType == FillingTypes::VALUE)
		{
			const deInt16 converted = static_cast<deInt16>(desc.value);
			return Resource(BufferSp(new Buffer<deInt16>(std::vector<deInt16>(desc.elemCount, converted), desc.padding)), desc.descriptorType);
		}
		else
		{
			de::Random rnd(desc.seed);
			std::vector<deInt16> randoms;
			if (desc.elemCount <= 24)
			{
				randoms = getInt16s(rnd, 24);
				randoms.resize(desc.elemCount);
			}
			else
			{
				randoms = getInt16s(rnd, desc.elemCount);
			}
			return Resource(BufferSp(new Buffer<deInt16>(randoms, desc.padding)), desc.descriptorType);
		}
	}
	case DataTypes::FLOAT16:
	{
		if (desc.fillType == FillingTypes::VALUE)
		{
			const deFloat16 converted = static_cast<deFloat16>(desc.value);
			return Resource(BufferSp(new Buffer<deFloat16>(std::vector<deFloat16>(desc.elemCount, converted), desc.padding)), desc.descriptorType);
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
			return Resource(BufferSp(new Buffer<deFloat16>(randoms, desc.padding)), desc.descriptorType);
		}
	}
	case DataTypes::UINT32:
	{
		if (desc.fillType == FillingTypes::VALUE)
		{
			const deUint32 converted = static_cast<deUint32>(desc.value);
			return Resource(BufferSp(new Buffer<deUint32>(std::vector<deUint32>(desc.elemCount, converted), desc.padding)), desc.descriptorType);
		}
		else
		{
			de::Random rnd(desc.seed);
			std::vector<deUint32> randoms;
			if (desc.elemCount <= 24)
			{
				randoms = getUint32s(rnd, 24);
				randoms.resize(desc.elemCount);
			}
			else
			{
				randoms = getUint32s(rnd, desc.elemCount);
			}
			return Resource(BufferSp(new Buffer<deUint32>(randoms, desc.padding)), desc.descriptorType);
		}
	}
	case DataTypes::INT32:
	{
		if (desc.fillType == FillingTypes::VALUE)
		{
			const deInt32 converted = static_cast<deInt32>(desc.value);
			return Resource(BufferSp(new Buffer<deInt32>(std::vector<deInt32>(desc.elemCount, converted), desc.padding)), desc.descriptorType);
		}
		else
		{
			de::Random rnd(desc.seed);
			std::vector<deInt32> randoms;
			if (desc.elemCount <= 24)
			{
				randoms = getInt32s(rnd, 24);
				randoms.resize(desc.elemCount);
			}
			else
			{
				randoms = getInt32s(rnd, desc.elemCount);
			}
			return Resource(BufferSp(new Buffer<deInt32>(randoms, desc.padding)), desc.descriptorType);
		}
	}
	case DataTypes::FLOAT32:
	{
		if (desc.fillType == FillingTypes::VALUE)
		{
			const float converted = static_cast<float>(desc.value);
			return Resource(BufferSp(new Buffer<float>(std::vector<float>(desc.elemCount, converted), desc.padding)), desc.descriptorType);
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
			return Resource(BufferSp(new Buffer<float>(randoms, desc.padding)), desc.descriptorType);
		}
	}
	case DataTypes::UINT64:
	{
		if (desc.fillType == FillingTypes::VALUE)
		{
			const deUint64 converted = static_cast<deUint64>(desc.value);
			return Resource(BufferSp(new Buffer<deUint64>(std::vector<deUint64>(desc.elemCount, converted), desc.padding)), desc.descriptorType);
		}
		else
		{
			de::Random rnd(desc.seed);
			std::vector<deUint64> randoms;
			if (desc.elemCount <= 24)
			{
				randoms = getUint64s(rnd, 24);
				randoms.resize(desc.elemCount);
			}
			else
			{
				randoms = getUint64s(rnd, desc.elemCount);
			}
			return Resource(BufferSp(new Buffer<deUint64>(randoms, desc.padding)), desc.descriptorType);
		}
	}
	case DataTypes::INT64:
	{
		if (desc.fillType == FillingTypes::VALUE)
		{
			const deInt64 converted = static_cast<deInt64>(desc.value);
			return Resource(BufferSp(new Buffer<deInt64>(std::vector<deInt64>(desc.elemCount, converted), desc.padding)), desc.descriptorType);
		}
		else
		{
			de::Random rnd(desc.seed);
			std::vector<deInt64> randoms;
			if (desc.elemCount <= 24)
			{
				randoms = getInt64s(rnd, 24);
				randoms.resize(desc.elemCount);
			}
			else
			{
				randoms = getInt64s(rnd, desc.elemCount);
			}
			return Resource(BufferSp(new Buffer<deInt64>(randoms, desc.padding)), desc.descriptorType);
		}
	}
	case DataTypes::FLOAT64:
	{
		if (desc.fillType == FillingTypes::VALUE)
		{
			return Resource(BufferSp(new Buffer<double>(std::vector<double>(desc.elemCount, desc.value), desc.padding)), desc.descriptorType);
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
			return Resource(BufferSp(new Buffer<double>(randoms, desc.padding)), desc.descriptorType);
		}
	}
	default:
		DE_ASSERT(0);
		DE_FATAL("Unsupported data type");
		return Resource(BufferSp(new Buffer<deUint8>(std::vector<deUint8>(), desc.padding)), desc.descriptorType);
	}

	return Resource(BufferSp(new Buffer<deUint8>(std::vector<deUint8>(), desc.padding)), desc.descriptorType);
}

struct VectorResourceDesc
{
	std::vector<deUint8>	data;
	DATA_TYPE				dataType;
	vk::VkDescriptorType	descriptorType;
};

static Resource createResourceFromVector(const VectorResourceDesc& desc)
{
	const DATA_TYPE type = desc.dataType;

	switch (type)
	{
	case DataTypes::UINT8:
	{
		return Resource(BufferSp(new Buffer<deUint8>(std::vector<deUint8>(desc.data.data(), desc.data.data() + desc.data.size()))), desc.descriptorType);
	}
	case DataTypes::INT8:
	{
		return Resource(BufferSp(new Buffer<deInt8>(std::vector<deInt8>(desc.data.data(), desc.data.data() + desc.data.size()))), desc.descriptorType);
	}
	case DataTypes::UINT16:
	{
		return Resource(BufferSp(new Buffer<deUint16>(std::vector<deUint16>(desc.data.data(), desc.data.data() + desc.data.size()))), desc.descriptorType);
	}
	case DataTypes::INT16:
	{
		return Resource(BufferSp(new Buffer<deInt16>(std::vector<deInt16>(desc.data.data(), desc.data.data() + desc.data.size()))), desc.descriptorType);
	}
	case DataTypes::FLOAT16:
	{
		return Resource(BufferSp(new Buffer<deFloat16>(std::vector<deFloat16>(desc.data.data(), desc.data.data() + desc.data.size()))), desc.descriptorType);
	}
	case DataTypes::UINT32:
	{
		return Resource(BufferSp(new Buffer<deUint32>(std::vector<deUint32>(desc.data.data(), desc.data.data() + desc.data.size()))), desc.descriptorType);
	}
	case DataTypes::INT32:
	{
		return Resource(BufferSp(new Buffer<deInt32>(std::vector<deInt32>(desc.data.data(), desc.data.data() + desc.data.size()))), desc.descriptorType);
	}
	case DataTypes::FLOAT32:
	{
		return Resource(BufferSp(new Buffer<float>(std::vector<float>(desc.data.data(), desc.data.data() + desc.data.size()))), desc.descriptorType);
	}
	case DataTypes::UINT64:
	{
		return Resource(BufferSp(new Buffer<deUint64>(std::vector<deUint64>(desc.data.data(), desc.data.data() + desc.data.size()))), desc.descriptorType);
	}
	case DataTypes::INT64:
	{
		return Resource(BufferSp(new Buffer<deInt64>(std::vector<deInt64>(desc.data.data(), desc.data.data() + desc.data.size()))), desc.descriptorType);
	}
	case DataTypes::FLOAT64:
	{
		return Resource(BufferSp(new Buffer<double>(std::vector<double>(desc.data.data(), desc.data.data() + desc.data.size()))), desc.descriptorType);
	}
	default:
		DE_ASSERT(0);
		DE_FATAL("Unsupported data type");
		return Resource(BufferSp(new Buffer<deUint8>(std::vector<deUint8>())));
	}

	return Resource(BufferSp(new Buffer<deUint8>(std::vector<deUint8>())));
}

static Resource createAtomicResource(const AtomicResourceDesc& desc, const std::vector<AtomicOpDesc>& atomicOpDescs)
{
	const DATA_TYPE type = desc.dataType;

	switch (type)
	{
	case DataTypes::UINT8:
	{
		return Resource(BufferSp(new AtomicBuffer<deUint8>(std::vector<deUint8>(desc.elemCount), atomicOpDescs)));
	}
	case DataTypes::INT8:
	{
		return Resource(BufferSp(new AtomicBuffer<deInt8>(std::vector<deInt8>(desc.elemCount), atomicOpDescs)));
	}
	case DataTypes::UINT16:
	{
		return Resource(BufferSp(new AtomicBuffer<deUint16>(std::vector<deUint16>(desc.elemCount), atomicOpDescs)));
	}
	case DataTypes::INT16:
	{
		return Resource(BufferSp(new AtomicBuffer<deInt16>(std::vector<deInt16>(desc.elemCount), atomicOpDescs)));
	}
	case DataTypes::FLOAT16:
	{
		return Resource(BufferSp(new AtomicBuffer<deFloat16>(std::vector<deFloat16>(desc.elemCount), atomicOpDescs)));
	}
	case DataTypes::UINT32:
	{
		return Resource(BufferSp(new AtomicBuffer<deUint32>(std::vector<deUint32>(desc.elemCount), atomicOpDescs)));
	}
	case DataTypes::INT32:
	{
		return Resource(BufferSp(new AtomicBuffer<deInt32>(std::vector<deInt32>(desc.elemCount), atomicOpDescs)));
	}
	case DataTypes::FLOAT32:
	{
		return Resource(BufferSp(new AtomicBuffer<float>(std::vector<float>(desc.elemCount), atomicOpDescs)));
	}
	case DataTypes::UINT64:
	{
		return Resource(BufferSp(new AtomicBuffer<deUint64>(std::vector<deUint64>(desc.elemCount), atomicOpDescs)));
	}
	case DataTypes::INT64:
	{
		return Resource(BufferSp(new AtomicBuffer<deInt64>(std::vector<deInt64>(desc.elemCount), atomicOpDescs)));
	}
	case DataTypes::FLOAT64:
	{
		return Resource(BufferSp(new AtomicBuffer<double>(std::vector<double>(desc.elemCount), atomicOpDescs)));
	}
	default:
		DE_ASSERT(0);
		DE_FATAL("Unsupported data type");
		return Resource(BufferSp(new AtomicBuffer<deUint8>(std::vector<deUint8>(), atomicOpDescs)));
	}

	return Resource(BufferSp(new AtomicBuffer<deUint8>(std::vector<deUint8>(), atomicOpDescs)));
}

std::string createShaderHeader(const char* pInterfaces = "")
{
	std::string header = std::string(
		"OpCapability Shader\n"
		"OpCapability UntypedPointersKHR\n"
		"${storageCap:opt}\n"
		"${smallStorageCap:opt}\n"
		"${additionalStorageCap:opt}\n"
		"${baseTypeCap:opt}\n"
		"${atomicCap:opt}\n"
		"${atomicAddCap:opt}\n"
		"${otherCap:opt}\n"
		"${memModelCap:opt}\n"
		"${memModelExt:opt}\n"
		"${smallStorageExt:opt}\n"
		"OpExtension \"SPV_KHR_storage_buffer_storage_class\"\n"
		"OpExtension \"SPV_KHR_untyped_pointers\"\n"
		"${otherExt:opt}\n"
		"${atomicAddExt:opt}\n"
		"${memModelOp}\n"
		"OpEntryPoint GLCompute %main \"main\" %id "
	);

	header += pInterfaces;
	header += " \n"
			  "OpExecutionMode %main LocalSize 1 1 1\n";

	return header;
}

std::string createShaderAnnotations(BASE_TEST_CASE testCase)
{
	std::string annotations = std::string(
		"OpDecorate       %id            BuiltIn GlobalInvocationId\n"
	);

	switch (testCase)
	{
	case BaseTestCases::ARRAY_LENGTH:
	{
		annotations += std::string(
			"OpDecorate       %${baseType}_rta        ArrayStride   ${alignment}\n"

			"OpMemberDecorate %input_buffer           0             Offset       0\n"
			"OpDecorate       %input_buffer           Block\n"
			"OpDecorate       %input_data_untyped_var DescriptorSet 0\n"
			"OpDecorate       %input_data_untyped_var Binding       0\n"

			"OpMemberDecorate %output_buffer          0             Offset       0\n"
			"OpDecorate       %output_buffer          Block\n"
			"OpDecorate       %output_data_var        DescriptorSet 0\n"
			"OpDecorate       %output_data_var        Binding       1\n"
		);

		break;
	}
	case BaseTestCases::LOAD:
	{
		annotations += std::string(
			"OpMemberDecorate %input_buffer  0       Offset 0\n"
			"OpDecorate       %input_buffer  Block\n"

			"OpMemberDecorate %output_buffer 0       Offset 0\n"
			"OpDecorate       %output_buffer Block\n"

			"${storageDecorations}\n"
		);
		break;
	}
	case BaseTestCases::COPY_FROM:
	{
		annotations += std::string(
			"OpDecorate       %array_${baseType}_${threadCount}     ArrayStride ${alignment}\n"

			"OpMemberDecorate %input_buffer           0             Offset 0\n"
			"OpDecorate       %input_buffer           Block\n"
			"OpDecorate       %input_data_untyped_var DescriptorSet 0\n"
			"OpDecorate       %input_data_untyped_var Binding       0\n"

			"OpMemberDecorate %output_buffer          0             Offset 0\n"
			"OpDecorate       %output_buffer          Block\n"
			"OpDecorate       %output_data_var        DescriptorSet 0\n"
			"OpDecorate       %output_data_var        Binding       1\n"
		);
		break;
	}
	case BaseTestCases::STORE:
	case BaseTestCases::COPY_TO:
	{
		annotations += std::string(
			"OpDecorate %array_${baseType}_${threadCount} ArrayStride ${alignment}\n"

			"OpMemberDecorate %input_buffer            0             Offset 0\n"
			"OpDecorate       %input_buffer            Block\n"
			"OpDecorate       %input_data_var          DescriptorSet 0\n"
			"OpDecorate       %input_data_var          Binding       0\n"

			"OpMemberDecorate %output_buffer           0             Offset 0\n"
			"OpDecorate       %output_buffer           Block\n"
			"OpDecorate       %output_data_untyped_var DescriptorSet 0\n"
			"OpDecorate       %output_data_untyped_var Binding       1\n"
		);
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
	std::string annotations = std::string(
		"OpDecorate       %id            BuiltIn GlobalInvocationId\n"
	);

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
		annotations += std::string(
			"OpMemberDecorate %output_buffer           0             Offset 0\n"
			"OpDecorate       %output_buffer           Block\n"
			"OpDecorate       %output_data_untyped_var DescriptorSet 0\n"
			"OpDecorate       %output_data_untyped_var Binding       0\n"
		);
		break;
	}
	case AtomicTestCases::OP_ATOMIC_LOAD:
	{
		annotations += std::string(
			"OpDecorate       %array_${baseType}_${threadCount}     ArrayStride ${alignment}\n"

			"OpMemberDecorate %input_buffer           0             Offset 0\n"
			"OpDecorate       %input_buffer           Block\n"
			"OpDecorate       %input_data_untyped_var DescriptorSet 0\n"
			"OpDecorate       %input_data_untyped_var Binding       0\n"

			"OpMemberDecorate %output_buffer          0             Offset 0\n"
			"OpDecorate       %output_buffer          Block\n"
			"OpDecorate       %output_data_var        DescriptorSet 0\n"
			"OpDecorate       %output_data_var        Binding       1\n"
		);

		break;
	}
	case AtomicTestCases::OP_ATOMIC_STORE:
	{
		annotations += std::string(
			"OpDecorate       %array_${baseType}_${threadCount}      ArrayStride ${alignment}\n"

			"OpMemberDecorate %input_buffer            0             Offset 0\n"
			"OpDecorate       %input_buffer            Block\n"
			"OpDecorate       %input_data_var          DescriptorSet 0\n"
			"OpDecorate       %input_data_var          Binding       0\n"

			"OpMemberDecorate %output_buffer           0             Offset 0\n"
			"OpDecorate       %output_buffer           Block\n"
			"OpDecorate       %output_data_untyped_var DescriptorSet 0\n"
			"OpDecorate       %output_data_untyped_var Binding       1\n"
		);

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
	std::string annotations = std::string(
		"OpDecorate       %id            BuiltIn GlobalInvocationId\n"
	);

	switch (testCase)
	{
	case TypePunningTestCases::COPY_FROM_SAME_SIZE_TYPES:
	{
		annotations += std::string(
			"OpDecorate       %array_${baseType}_${threadCount}     ArrayStride ${alignment}\n"
			"OpDecorate       %array_${sameSizeType}_${threadCount} ArrayStride ${alignment}\n"

			"OpMemberDecorate %input_buffer           0             Offset 0\n"
			"OpDecorate       %input_buffer           Block\n"
			"OpDecorate       %input_data_untyped_var DescriptorSet 0\n"
			"OpDecorate       %input_data_untyped_var Binding       0\n"

			"OpMemberDecorate %output_buffer          0             Offset 0\n"
			"OpDecorate       %output_buffer          Block\n"
			"OpDecorate       %output_data_var        DescriptorSet 0\n"
			"OpDecorate       %output_data_var        Binding       1\n"
		);

		break;
	}
	case TypePunningTestCases::LOAD_SAME_SIZE_TYPES:
	{
		annotations += std::string(
			"OpMemberDecorate %input_buffer  0      Offset 0\n"
			"OpDecorate       %input_buffer  Block\n"

			"OpMemberDecorate %output_buffer 0       Offset 0\n"
			"OpDecorate       %output_buffer Block\n"

			"${storageDecorations}\n"
		);

		break;
	}
	case TypePunningTestCases::LOAD_SCALAR_VECTOR:
	case TypePunningTestCases::LOAD_VECTOR_SCALAR:
	{
		annotations += std::string(
			"OpMemberDecorate %input_buffer  0      Offset 0\n"
			"OpDecorate       %input_buffer  Block\n"

			"OpMemberDecorate %output_buffer 0       Offset 0\n"
			"OpDecorate       %output_buffer Block\n"

			"${storageDecorations}\n"
		);

		break;
	}
	case TypePunningTestCases::COPY_FROM_SCALAR_VECTOR:
	case TypePunningTestCases::COPY_FROM_VECTOR_SCALAR:
	{
		annotations += std::string(
			"OpMemberDecorate %input_buffer           0             Offset 0\n"
			"OpDecorate       %input_buffer           Block\n"
			"OpDecorate       %input_data_untyped_var DescriptorSet 0\n"
			"OpDecorate       %input_data_untyped_var Binding       0\n"

			"OpMemberDecorate %output_buffer          0             Offset 0\n"
			"OpDecorate       %output_buffer          Block\n"
			"OpDecorate       %output_data_var        DescriptorSet 0\n"
			"OpDecorate       %output_data_var        Binding       1\n"
		);

		break;
	}
	case TypePunningTestCases::COPY_TO_SAME_SIZE_TYPES:
	case TypePunningTestCases::STORE_SAME_SIZE_TYPES:
	{
		annotations += std::string(
			"OpDecorate %array_${baseType}_${threadCount}     ArrayStride ${alignment}\n"
			"OpDecorate %array_${sameSizeType}_${threadCount} ArrayStride ${alignment}\n"

			"OpMemberDecorate %input_buffer            0             Offset 0\n"
			"OpDecorate       %input_buffer            Block\n"
			"OpDecorate       %input_data_var          DescriptorSet 0\n"
			"OpDecorate       %input_data_var          Binding       0\n"

			"OpMemberDecorate %output_buffer           0             Offset 0\n"
			"OpDecorate       %output_buffer           Block\n"
			"OpDecorate       %output_data_untyped_var DescriptorSet 0\n"
			"OpDecorate       %output_data_untyped_var Binding       1\n"
		);
		break;
	}
	case TypePunningTestCases::COPY_TO_SCALAR_VECTOR:
	case TypePunningTestCases::COPY_TO_VECTOR_SCALAR:
	case TypePunningTestCases::STORE_SCALAR_VECTOR:
	case TypePunningTestCases::STORE_VECTOR_SCALAR:
	{
		annotations += std::string(
			"OpMemberDecorate %input_buffer            0             Offset 0\n"
			"OpDecorate       %input_buffer            Block\n"
			"OpDecorate       %input_data_var          DescriptorSet 0\n"
			"OpDecorate       %input_data_var          Binding       0\n"

			"OpMemberDecorate %output_buffer           0              Offset 0\n"
			"OpDecorate       %output_buffer           Block\n"
			"OpDecorate       %output_data_untyped_var DescriptorSet  0\n"
			"OpDecorate       %output_data_untyped_var Binding        1\n"
		);
		break;
	}
	case TypePunningTestCases::MULTIPLE_ACCESS_CHAINS:
	{
		annotations += std::string(
			"OpMemberDecorate %data_buffer              0             Offset 0\n"
			"OpMemberDecorate %data_buffer              1             Offset ${size}\n"

			"OpMemberDecorate %input_buffer             0             Offset 0\n"
			"OpDecorate       %input_buffer             Block\n"
			"OpDecorate       %input_data_untyped_var   DescriptorSet 0\n"
			"OpDecorate       %input_data_untyped_var   Binding       0\n"

			"OpMemberDecorate %output_buffer            0             Offset 0\n"
			"OpDecorate       %output_buffer            Block\n"
			"OpDecorate       %output_data_var          DescriptorSet 0\n"
			"OpDecorate       %output_data_var          Binding       1\n"
		);
		break;
	}
	case TypePunningTestCases::CUSTOM_STRUCT_TYPE:
	{
		annotations += std::string(
			"OpMemberDecorate %input_buffer    0             Offset 0\n"
			"${inputOffsets:opt}\n"
			"OpDecorate       %input_buffer            Block\n"
			"OpDecorate       %input_data_untyped_var  DescriptorSet 0\n"
			"OpDecorate       %input_data_untyped_var  Binding       0\n"


			"OpMemberDecorate %output_buffer   0             Offset 0\n"
			"${outputOffsets:opt}\n"
			"OpDecorate       %output_buffer   Block\n"
			"OpDecorate       %output_data_var DescriptorSet 0\n"
			"OpDecorate       %output_data_var Binding       1\n"
		);

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
	std::string annotations = std::string(
		"OpDecorate %id BuiltIn GlobalInvocationId\n"
	);

	switch (testCase)
	{
	case PointerTestCases::OP_FUNCTION_CALL_PHYSICAL_STORAGE:
	{
		annotations += std::string(
			"OpDecorate       %input_ptr                Restrict\n"

			"OpMemberDecorate %data_buffer              0             Offset 0\n"
			"OpDecorate       %data_buffer              Block\n"

			"OpMemberDecorate %input_buffer             0             Offset 0\n"
			"OpDecorate       %input_buffer             Block\n"
			"OpDecorate       %input_data_untyped_var   DescriptorSet 0\n"
			"OpDecorate       %input_data_untyped_var   Binding       0\n"

			"OpMemberDecorate %output_buffer            0             Offset 0\n"
			"OpDecorate       %output_buffer            Block\n"
			"OpDecorate       %output_data_var          DescriptorSet 0\n"
			"OpDecorate       %output_data_var          Binding       1\n"
		);
		break;
	}
	case PointerTestCases::OP_BITCAST_FROM_UNTYPED_PHYSICAL_STORAGE:
	{
		annotations += std::string(
			"OpMemberDecorate %data_buffer              0             Offset 0\n"
			"OpDecorate       %data_buffer              Block\n"

			"OpMemberDecorate %input_buffer             0             Offset 0\n"
			"OpDecorate       %input_buffer             Block\n"
			"OpDecorate       %input_data_untyped_var   DescriptorSet 0\n"
			"OpDecorate       %input_data_untyped_var   Binding       0\n"

			"OpMemberDecorate %output_buffer            0             Offset 0\n"
			"OpDecorate       %output_buffer            Block\n"
			"OpDecorate       %output_data_var          DescriptorSet 0\n"
			"OpDecorate       %output_data_var          Binding       1\n"
		);
		break;
	}
	case PointerTestCases::OP_BITCAST_TO_UNTYPED_PHYSICAL_STORAGE:
	{
		annotations += std::string(
			"OpMemberDecorate %data_buffer              0             Offset 0\n"
			"OpDecorate       %data_buffer              Block\n"

			"OpMemberDecorate %input_buffer             0             Offset 0\n"
			"OpDecorate       %input_buffer             Block\n"
			"OpDecorate       %input_data_var           DescriptorSet 0\n"
			"OpDecorate       %input_data_var           Binding       0\n"

			"OpMemberDecorate %output_buffer            0             Offset 0\n"
			"OpDecorate       %output_buffer            Block\n"
			"OpDecorate       %output_data_untyped_var  DescriptorSet 0\n"
			"OpDecorate       %output_data_untyped_var  Binding       1\n"
		);
		break;
	}
	case PointerTestCases::OP_PHI_PHYSICAL_STORAGE:
	case PointerTestCases::OP_SELECT_PHYSICAL_STORAGE:
	{
		annotations += std::string(
			"OpMemberDecorate %data_buffer              0             Offset 0\n"
			"OpDecorate       %data_buffer              Block\n"

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
			"OpDecorate       %output_data_var          Binding       2\n"
		);
		break;
	}
	case PointerTestCases::OP_PTR_ACCESS_CHAIN_PHYSICAL_STORAGE:
	{
		annotations += std::string(
			"OpDecorate       %array_${baseType}_${threadCount} ArrayStride ${alignment}\n"

			"OpMemberDecorate %data_buffer            0             Offset 0\n"
			"OpDecorate       %data_buffer            Block\n"

			"OpMemberDecorate %input_buffer           0             Offset 0\n"
			"OpDecorate       %input_buffer           Block\n"
			"OpDecorate       %input_data_untyped_var DescriptorSet 0\n"
			"OpDecorate       %input_data_untyped_var Binding       0\n"

			"OpMemberDecorate %output_buffer          0             Offset 0\n"
			"OpDecorate       %output_buffer          Block\n"
			"OpDecorate       %output_data_var        DescriptorSet 0\n"
			"OpDecorate       %output_data_var        Binding       1\n"
		);
		break;
	}
	case PointerTestCases::OP_SELECT_VARIABLE_PTR:
	case PointerTestCases::OP_PHI_VARIABLE_PTR:
	case PointerTestCases::OP_PTR_EQUAL_VARIABLE_PTR:
	case PointerTestCases::OP_PTR_NOT_EQUAL_VARIABLE_PTR:
	case PointerTestCases::FUNCTION_VARIABLE_VARIABLE_PTR:
	case PointerTestCases::PRIVATE_VARIABLE_VARIABLE_PTR:
	{
		annotations += std::string(
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
			"OpDecorate       %output_data_var          Binding       2\n"
		);
		break;
	}
	case PointerTestCases::OP_PTR_DIFF_VARIABLE_PTR:
	case PointerTestCases::OP_PTR_ACCESS_CHAIN_VARIABLE_PTR:
	{
		annotations += std::string(
			"OpDecorate       %array_${baseType}_${threadCount} ArrayStride ${alignment}\n"

			"OpMemberDecorate %input_buffer             0             Offset 0\n"
			"OpDecorate       %input_buffer             Block\n"
			"OpDecorate       %input_data_untyped_var   DescriptorSet 0\n"
			"OpDecorate       %input_data_untyped_var   Binding       0\n"

			"OpMemberDecorate %output_buffer            0             Offset 0\n"
			"OpDecorate       %output_buffer            Block\n"
			"OpDecorate       %output_data_var          DescriptorSet 0\n"
			"OpDecorate       %output_data_var          Binding       1\n"
		);
		break;
	}
	case PointerTestCases::MULTIPLE_ACCESS_CHAINS_VARIABLE_PTR:
	{
		annotations += std::string(
			"OpMemberDecorate %data_buffer            0             Offset 0\n"
			"OpMemberDecorate %data_buffer            1             Offset ${size}\n"

			"OpMemberDecorate %input_buffer           0             Offset 0\n"
			"OpDecorate       %input_buffer           Block\n"
			"OpDecorate       %input_data_untyped_var DescriptorSet 0\n"
			"OpDecorate       %input_data_untyped_var Binding       0\n"

			"OpMemberDecorate %output_buffer          0             Offset 0\n"
			"OpDecorate       %output_buffer          Block\n"
			"OpDecorate       %output_data_var        DescriptorSet 0\n"
			"OpDecorate       %output_data_var        Binding       1\n"
		);
		break;
	}
	case PointerTestCases::OP_FUNCTION_CALL_VARIABLE_PTR:
	{
		annotations += std::string(
			"OpMemberDecorate %input_buffer           0             Offset 0\n"
			"OpDecorate       %input_buffer           Block\n"
			"OpDecorate       %input_data_untyped_var DescriptorSet 0\n"
			"OpDecorate       %input_data_untyped_var Binding       0\n"

			"OpMemberDecorate %output_buffer          0             Offset 0\n"
			"OpDecorate       %output_buffer          Block\n"
			"OpDecorate       %output_data_var        DescriptorSet 0\n"
			"OpDecorate       %output_data_var        Binding       1\n"
		);
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

std::string createShaderAnnotations(WORKGROUP_TEST_CASE testCase)
{
	std::string annotations = std::string(
		"OpDecorate %id BuiltIn GlobalInvocationId\n"
	);

	switch (testCase)
	{
	case WorkgroupTestCases::ALIASED:
	{
		annotations += std::string(
			"OpMemberDecorate %input_buffer    0             Offset 0\n"
			"OpMemberDecorate %input_buffer    1             Offset ${vecOffset}\n"
			"OpDecorate       %input_buffer    Block\n"
			"OpDecorate       %input_data_var  DescriptorSet 0\n"
			"OpDecorate       %input_data_var  Binding       0\n"

			"OpMemberDecorate %output_buffer   0             Offset 0\n"
			"OpMemberDecorate %output_buffer   1             Offset ${vecOffset}\n"
			"OpDecorate       %output_buffer   Block\n"
			"OpDecorate       %output_data_var DescriptorSet 0\n"
			"OpDecorate       %output_data_var Binding       1\n"

			"OpMemberDecorate %data_buffer     0             Offset 0\n"
			"OpMemberDecorate %data_buffer     1             Offset ${vecOffset}\n"
			"OpDecorate       %data_buffer     Block\n"

			"OpDecorate       %data_buffer_0_untyped_var     Aliased\n"
			"OpDecorate       %data_buffer_1_untyped_var     Aliased\n"
		);
		break;
	}
	case WorkgroupTestCases::NOT_ALIASED:
	{
		annotations += std::string(
			"OpMemberDecorate %input_buffer    0             Offset 0\n"
			"OpMemberDecorate %input_buffer    1             Offset ${vecOffset}\n"
			"OpDecorate       %input_buffer    Block\n"
			"OpDecorate       %input_data_var  DescriptorSet 0\n"
			"OpDecorate       %input_data_var  Binding       0\n"

			"OpMemberDecorate %output_buffer   0             Offset 0\n"
			"OpMemberDecorate %output_buffer   1             Offset ${vecOffset}\n"
			"OpDecorate       %output_buffer   Block\n"
			"OpDecorate       %output_data_var DescriptorSet 0\n"
			"OpDecorate       %output_data_var Binding       1\n"

			"OpMemberDecorate %data_buffer     0             Offset 0\n"
			"OpMemberDecorate %data_buffer     1             Offset ${vecOffset}\n"
			"OpDecorate       %data_buffer     Block\n"
		);
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
	std::string annotations = std::string(
		"OpDecorate %id BuiltIn GlobalInvocationId\n"
	);

	switch (testCase)
	{
	case CooperativeMatrixTestCases::BASIC_LOAD:
	{
		annotations += std::string(
			"OpDecorate       %${baseType}_rta        ArrayStride   ${typeSize}\n"

			"OpMemberDecorate %input_buffer           0             Offset 0\n"
			"OpDecorate       %input_buffer           Block\n"
			"OpDecorate       %input_data_untyped_var DescriptorSet 0\n"
			"OpDecorate       %input_data_untyped_var Binding       0\n"

			"OpMemberDecorate %output_buffer          0             Offset 0\n"
			"OpDecorate       %output_buffer          Block\n"
			"OpDecorate       %output_data_var        DescriptorSet 0\n"
			"OpDecorate       %output_data_var        Binding       1\n"
		);
		break;
	}
	case CooperativeMatrixTestCases::BASIC_STORE:
	{
		annotations += std::string(
			"OpDecorate       %${baseType}_rta         ArrayStride   ${typeSize}\n"

			"OpMemberDecorate %input_buffer            0             Offset 0\n"
			"OpDecorate       %input_buffer            Block\n"
			"OpDecorate       %input_data_var          DescriptorSet 0\n"
			"OpDecorate       %input_data_var          Binding       0\n"

			"OpMemberDecorate %output_buffer           0             Offset 0\n"
			"OpDecorate       %output_buffer           Block\n"
			"OpDecorate       %output_data_untyped_var DescriptorSet 0\n"
			"OpDecorate       %output_data_untyped_var Binding       1\n"
		);
		break;
	}
	case CooperativeMatrixTestCases::TYPE_PUNNING_LOAD:
	{
		annotations += std::string(
			"OpDecorate       %${baseType}_rta        ArrayStride   ${typeSize}\n"
			"OpDecorate       %${sameSizeType}_rta    ArrayStride   ${typeSize}\n"

			"OpMemberDecorate %input_buffer           0             Offset 0\n"
			"OpDecorate       %input_buffer           Block\n"
			"OpDecorate       %input_data_untyped_var DescriptorSet 0\n"
			"OpDecorate       %input_data_untyped_var Binding       0\n"

			"OpMemberDecorate %output_buffer          0             Offset 0\n"
			"OpDecorate       %output_buffer          Block\n"
			"OpDecorate       %output_data_var        DescriptorSet 0\n"
			"OpDecorate       %output_data_var        Binding       1\n"
		);
		break;
	}
	case CooperativeMatrixTestCases::TYPE_PUNNING_STORE:
	{
		annotations += std::string(
			"OpDecorate       %${baseType}_rta         ArrayStride   ${typeSize}\n"
			"OpDecorate       %${sameSizeType}_rta     ArrayStride   ${typeSize}\n"

			"OpMemberDecorate %input_buffer            0             Offset 0\n"
			"OpDecorate       %input_buffer            Block\n"
			"OpDecorate       %input_data_var          DescriptorSet 0\n"
			"OpDecorate       %input_data_var          Binding       0\n"

			"OpMemberDecorate %output_buffer           0             Offset 0\n"
			"OpDecorate       %output_buffer           Block\n"
			"OpDecorate       %output_data_untyped_var DescriptorSet 0\n"
			"OpDecorate       %output_data_untyped_var Binding       1\n"
		);
		break;
	}
	case CooperativeMatrixTestCases::MIXED_LOAD:
	{
		annotations += std::string(
			"OpDecorate       %${baseType}_rta        ArrayStride   ${typeSize}\n"

			"OpMemberDecorate %input_buffer           0             Offset 0\n"
			"OpDecorate       %input_buffer           Block\n"
			"OpDecorate       %input_data_untyped_var DescriptorSet 0\n"
			"OpDecorate       %input_data_untyped_var Binding       0\n"

			"OpMemberDecorate %output_buffer          0             Offset 0\n"
			"OpDecorate       %output_buffer          Block\n"
			"OpDecorate       %output_data_var        DescriptorSet 0\n"
			"OpDecorate       %output_data_var        Binding       1\n"
		);
		break;
	}
	case CooperativeMatrixTestCases::MIXED_STORE:
	{
		annotations += std::string(
			"OpDecorate       %${baseType}_rta         ArrayStride   ${typeSize}\n"

			"OpMemberDecorate %input_buffer            0             Offset 0\n"
			"OpDecorate       %input_buffer            Block\n"
			"OpDecorate       %input_data_var          DescriptorSet 0\n"
			"OpDecorate       %input_data_var          Binding       0\n"

			"OpMemberDecorate %output_buffer           0             Offset 0\n"
			"OpDecorate       %output_buffer           Block\n"
			"OpDecorate       %output_data_untyped_var DescriptorSet 0\n"
			"OpDecorate       %output_data_untyped_var Binding       1\n"
		);
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
			"%input_data_untyped_var             = OpUntypedVariableKHR    %storage_buffer_untyped_ptr       StorageBuffer %input_buffer\n"
			"%output_data_var                    = OpVariable              %output_buffer_storage_buffer_ptr StorageBuffer\n"
		);

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
			"%input_data_untyped_var           = OpUntypedVariableKHR    %storage_buffer_untyped_ptr        StorageBuffer %input_buffer\n"
			"%output_data_var                  = OpVariable              %output_buffer_storage_buffer_ptr  StorageBuffer\n"
		);
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
			"%input_data_untyped_var           = OpUntypedVariableKHR    %storage_buffer_untyped_ptr       ${storageClass} %input_buffer\n"
			"%output_data_var                  = OpVariable              %output_buffer_storage_buffer_ptr StorageBuffer\n"
		);
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
			"%input_data_var                  = OpVariable              %input_buffer_storage_buffer_ptr StorageBuffer\n"
			"%output_data_untyped_var         = OpUntypedVariableKHR    %storage_buffer_untyped_ptr      StorageBuffer %output_buffer\n"
		);

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
			"%input_data_var                  = OpVariable              %input_buffer_storage_buffer_ptr StorageBuffer\n"
			"%output_data_untyped_var         = OpUntypedVariableKHR    %storage_buffer_untyped_ptr      StorageBuffer %output_buffer\n"
		);
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
		"%${baseType}         = ${baseDecl}\n"
	);

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
			"%output_data_untyped_var         = OpUntypedVariableKHR    %storage_buffer_untyped_ptr StorageBuffer %output_buffer\n"
		);
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
			"%output_data_untyped_var         = OpUntypedVariableKHR    %storage_buffer_untyped_ptr StorageBuffer %output_buffer\n"
		);
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
			"%output_data_untyped_var         = OpUntypedVariableKHR    %storage_buffer_untyped_ptr StorageBuffer %output_buffer\n"
		);
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
			"%input_data_untyped_var           = OpUntypedVariableKHR    %storage_buffer_untyped_ptr       StorageBuffer %input_buffer\n"
			"%output_data_var                  = OpVariable              %output_buffer_storage_buffer_ptr StorageBuffer\n"
		);

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
			"%input_data_var                  = OpVariable              %input_buffer_storage_buffer_ptr StorageBuffer\n"
			"%output_data_untyped_var         = OpUntypedVariableKHR    %storage_buffer_untyped_ptr      StorageBuffer %output_buffer\n"
		);

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
			"%input_data_untyped_var             = OpUntypedVariableKHR    %storage_buffer_untyped_ptr       StorageBuffer %input_buffer\n"
			"%output_data_var                    = OpVariable              %output_buffer_storage_buffer_ptr StorageBuffer\n"
		);
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
			"%input_data_untyped_var           = OpUntypedVariableKHR    %storage_buffer_untyped_ptr       StorageBuffer %input_buffer\n"
			"%output_data_var                  = OpVariable              %storage_buffer_output_buffer_ptr StorageBuffer\n"
		);
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
			"%input_buffer  = OpTypeStruct %${baseType}\n"
			"%output_buffer = OpTypeStruct %${otherVec}\n"

			/* Pointers */
			"%uint32_input_ptr                 = OpTypePointer           Input %uint32\n"
			"%vec3_uint32_input_ptr            = OpTypePointer           Input %vec3_uint32\n"
			"%storage_buffer_untyped_ptr       = OpTypeUntypedPointerKHR ${storageClass}\n"
			"%storage_buffer_${otherVec}_ptr   = OpTypePointer           StorageBuffer %${otherVec}\n"
			"%storage_buffer_output_buffer_ptr = OpTypePointer           StorageBuffer %output_buffer\n"

			/* Objects */
			"%id                               = OpVariable             %vec3_uint32_input_ptr            Input\n"
			"%input_data_untyped_var           = OpUntypedVariableKHR   %storage_buffer_untyped_ptr       ${storageClass} %input_buffer\n"
			"%output_data_var                  = OpVariable             %storage_buffer_output_buffer_ptr StorageBuffer\n"
		);
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
			"%input_data_untyped_var           = OpUntypedVariableKHR    %storage_buffer_untyped_ptr       StorageBuffer %input_buffer\n"
			"%output_data_var                  = OpVariable              %storage_buffer_output_buffer_ptr StorageBuffer\n"
		);
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
			"%input_buffer  = OpTypeStruct %${baseVec}\n"
			"%output_buffer = OpTypeStruct %${otherType}\n"

			/* Pointers */
			"%uint32_input_ptr                 = OpTypePointer           Input         %uint32\n"
			"%vec3_uint32_input_ptr            = OpTypePointer           Input         %vec3_uint32\n"
			"%storage_buffer_untyped_ptr       = OpTypeUntypedPointerKHR ${storageClass}\n"
			"%storage_buffer_${otherType}_ptr  = OpTypePointer           StorageBuffer %${otherType}\n"
			"%storage_buffer_output_buffer_ptr = OpTypePointer           StorageBuffer %output_buffer\n"

			/* Objects */
			"%id                               = OpVariable              %vec3_uint32_input_ptr            Input\n"
			"%input_data_untyped_var           = OpUntypedVariableKHR    %storage_buffer_untyped_ptr       ${storageClass} %input_buffer\n"
			"%output_data_var                  = OpVariable              %storage_buffer_output_buffer_ptr StorageBuffer\n"
		);
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
			"%input_data_untyped_var             = OpUntypedVariableKHR    %storage_buffer_untyped_ptr       ${storageClass} %input_buffer\n"
			"%output_data_var                    = OpVariable              %output_buffer_storage_buffer_ptr StorageBuffer\n"
		);
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
			"%input_data_var                     = OpVariable              %input_buffer_storage_buffer_ptr StorageBuffer\n"
			"%output_data_untyped_var            = OpUntypedVariableKHR    %storage_buffer_untyped_ptr      StorageBuffer %output_buffer\n"
		);
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
			"%input_data_var                     = OpVariable              %input_buffer_storage_buffer_ptr StorageBuffer\n"
			"%output_data_untyped_var            = OpUntypedVariableKHR    %storage_buffer_untyped_ptr      StorageBuffer %output_buffer\n"
		);
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
			"%input_data_var                  = OpVariable              %input_buffer_storage_buffer_ptr StorageBuffer\n"
			"%output_data_untyped_var         = OpUntypedVariableKHR    %storage_buffer_untyped_ptr      StorageBuffer %output_buffer\n"
		);
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
			"%input_data_var                  = OpVariable              %input_buffer_storage_buffer_ptr StorageBuffer\n"
			"%output_data_untyped_var         = OpUntypedVariableKHR    %storage_buffer_untyped_ptr      StorageBuffer %output_buffer\n"
		);
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
			"%input_data_var                  = OpVariable              %input_buffer_storage_buffer_ptr StorageBuffer\n"
			"%output_data_untyped_var         = OpUntypedVariableKHR    %storage_buffer_untyped_ptr      StorageBuffer %output_buffer\n"
		);
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
			"%input_data_var                  = OpVariable              %input_buffer_storage_buffer_ptr StorageBuffer\n"
			"%output_data_untyped_var         = OpUntypedVariableKHR    %storage_buffer_untyped_ptr      StorageBuffer %output_buffer\n"
		);
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
			"%input_data_untyped_var              = OpUntypedVariableKHR    %storage_buffer_untyped_ptr       StorageBuffer %input_buffer\n"
			"%output_data_var                     = OpVariable              %output_buffer_storage_buffer_ptr StorageBuffer\n"
			"%id                                  = OpVariable              %vec3_uint32_input_ptr            Input\n"
		);
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
			"%vec3_uint32_input_ptr            = OpTypePointer           Input                            %vec3_uint32\n"
			"%input_buffer_storage_buffer_ptr  = OpTypePointer           StorageBuffer                    %input_buffer\n"
			"%output_buffer_storage_buffer_ptr = OpTypePointer           StorageBuffer                    %output_buffer\n"
			"%storage_buffer_untyped_ptr       = OpTypeUntypedPointerKHR StorageBuffer\n"

			/* Objects */
			"%output_data_var                  = OpVariable              %output_buffer_storage_buffer_ptr StorageBuffer\n"
			"%input_data_untyped_var           = OpUntypedVariableKHR    %storage_buffer_untyped_ptr       StorageBuffer %input_buffer\n"
			"%id                               = OpVariable              %vec3_uint32_input_ptr            Input\n"
		);

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
		"%vec3_uint32  = OpTypeVector %uint32 3\n"

		/* Function types */
		"%void_func   = OpTypeFunction %void\n"

		/* Constants */
		"%c_uint32_0  = OpConstant %uint32 0\n"
		"%c_uint32_16 = OpConstant %uint32 16\n"
		"${boolConst:opt}\n"

		/* Pointers */
		"%uint32_input_ptr                 = OpTypePointer Input %uint32\n"
		"%vec3_uint32_input_ptr            = OpTypePointer Input %vec3_uint32\n"
	);

	switch (testCase)
	{
	case PointerTestCases::OP_PHI_PHYSICAL_STORAGE:
	case PointerTestCases::OP_SELECT_PHYSICAL_STORAGE:
	{
		variables += std::string(
			"%data_buffer                                                = OpTypeStruct            %${baseType}\n"
			"%${baseType}_physical_storage_buffer_ptr                    = OpTypePointer           PhysicalStorageBuffer                    %${baseType}\n"
			"%data_buffer_physical_storage_buffer_ptr                    = OpTypePointer           PhysicalStorageBuffer                    %data_buffer\n"
			"%data_buffer_physical_storage_buffer_ptr_storage_buffer_ptr = OpTypePointer           StorageBuffer                            %data_buffer_physical_storage_buffer_ptr\n"
			"%storage_buffer_untyped_ptr                                 = OpTypeUntypedPointerKHR StorageBuffer\n"
			"%physical_storage_buffer_untyped_ptr                        = OpTypeUntypedPointerKHR PhysicalStorageBuffer\n"

			"%input_buffer_0                                             = OpTypeStruct            %physical_storage_buffer_untyped_ptr\n"
			"%input_buffer_1                                             = OpTypeStruct            %physical_storage_buffer_untyped_ptr\n"
			"%output_buffer                                              = OpTypeStruct            %data_buffer_physical_storage_buffer_ptr\n"
			"%output_buffer_storage_buffer_ptr                           = OpTypePointer           StorageBuffer %output_buffer\n"

			"%input_data_0_untyped_var                                   = OpUntypedVariableKHR    %storage_buffer_untyped_ptr              StorageBuffer %input_buffer_0\n"
			"%input_data_1_untyped_var                                   = OpUntypedVariableKHR    %storage_buffer_untyped_ptr              StorageBuffer %input_buffer_1\n"
			"%output_data_var                                            = OpVariable              %output_buffer_storage_buffer_ptr        StorageBuffer\n"
		);

		break;
	}
	case PointerTestCases::OP_BITCAST_FROM_UNTYPED_PHYSICAL_STORAGE:
	{
		variables += std::string(
			"%data_buffer                                                = OpTypeStruct            %${baseType}\n"
			"%${baseType}_physical_storage_buffer_ptr                    = OpTypePointer           PhysicalStorageBuffer                    %${baseType}\n"
			"%data_buffer_physical_storage_buffer_ptr                    = OpTypePointer           PhysicalStorageBuffer                    %data_buffer\n"
			"%data_buffer_physical_storage_buffer_ptr_storage_buffer_ptr = OpTypePointer           StorageBuffer                            %data_buffer_physical_storage_buffer_ptr\n"
			"%storage_buffer_untyped_ptr                                 = OpTypeUntypedPointerKHR StorageBuffer\n"
			"%physical_storage_buffer_untyped_ptr                        = OpTypeUntypedPointerKHR PhysicalStorageBuffer\n"

			"%input_buffer                                               = OpTypeStruct            %physical_storage_buffer_untyped_ptr\n"
			"%output_buffer                                              = OpTypeStruct            %data_buffer_physical_storage_buffer_ptr\n"
			"%output_buffer_storage_buffer_ptr                           = OpTypePointer           StorageBuffer %output_buffer\n"

			"%input_data_untyped_var                                     = OpUntypedVariableKHR    %storage_buffer_untyped_ptr              StorageBuffer %input_buffer\n"
			"%output_data_var                                            = OpVariable              %output_buffer_storage_buffer_ptr        StorageBuffer\n"
		);
		break;
	}
	case PointerTestCases::OP_BITCAST_TO_UNTYPED_PHYSICAL_STORAGE:
	{
		variables += std::string(
			"%data_buffer                                                = OpTypeStruct            %${baseType}\n"
			"%${baseType}_physical_storage_buffer_ptr                    = OpTypePointer           PhysicalStorageBuffer %${baseType}\n"
			"%data_buffer_physical_storage_buffer_ptr                    = OpTypePointer           PhysicalStorageBuffer %data_buffer\n"
			"%data_buffer_physical_storage_buffer_ptr_storage_buffer_ptr = OpTypePointer           StorageBuffer         %data_buffer_physical_storage_buffer_ptr\n"
			"%storage_buffer_untyped_ptr                                 = OpTypeUntypedPointerKHR StorageBuffer\n"
			"%physical_storage_buffer_untyped_ptr                        = OpTypeUntypedPointerKHR PhysicalStorageBuffer\n"

			"%input_buffer                                               = OpTypeStruct            %data_buffer_physical_storage_buffer_ptr\n"
			"%output_buffer                                              = OpTypeStruct            %physical_storage_buffer_untyped_ptr\n"
			"%input_buffer_storage_buffer_ptr                            = OpTypePointer           StorageBuffer %input_buffer\n"

			"%input_data_var                                             = OpVariable              %input_buffer_storage_buffer_ptr         StorageBuffer\n"
			"%output_data_untyped_var                                    = OpUntypedVariableKHR    %storage_buffer_untyped_ptr              StorageBuffer %output_buffer\n"
		);
		break;
	}
	case PointerTestCases::OP_PTR_ACCESS_CHAIN_PHYSICAL_STORAGE:
	{
		variables += std::string(
			/* Contants */
			"%c_uint32_${threadCount}          = OpConstant  %uint32      ${threadCount}\n"

			/* Arrays */
			"%array_${baseType}_${threadCount} = OpTypeArray %${baseType} %c_uint32_${threadCount}\n"

			"%data_buffer                                                = OpTypeStruct            %array_${baseType}_${threadCount}\n"
			"%${baseType}_physical_storage_buffer_ptr                    = OpTypePointer           PhysicalStorageBuffer %${baseType}\n"
			"%data_buffer_physical_storage_buffer_ptr                    = OpTypePointer           PhysicalStorageBuffer %data_buffer\n"
			"%data_buffer_physical_storage_buffer_ptr_storage_buffer_ptr = OpTypePointer           StorageBuffer         %data_buffer_physical_storage_buffer_ptr\n"
			"%storage_buffer_untyped_ptr                                 = OpTypeUntypedPointerKHR StorageBuffer\n"
			"%physical_storage_buffer_untyped_ptr                        = OpTypeUntypedPointerKHR PhysicalStorageBuffer\n"

			"%input_buffer                                               = OpTypeStruct            %physical_storage_buffer_untyped_ptr\n"
			"%output_buffer                                              = OpTypeStruct            %data_buffer_physical_storage_buffer_ptr\n"
			"%output_buffer_storage_buffer_ptr                           = OpTypePointer           StorageBuffer %output_buffer\n"

			"%input_data_untyped_var                                     = OpUntypedVariableKHR    %storage_buffer_untyped_ptr              StorageBuffer %input_buffer\n"
			"%output_data_var                                            = OpVariable              %output_buffer_storage_buffer_ptr        StorageBuffer\n"
		);
		break;
	}
	case PointerTestCases::OP_FUNCTION_CALL_PHYSICAL_STORAGE:
	{
		variables += std::string(
			"%data_buffer                                                = OpTypeStruct            %${baseType}\n"
			"%${baseType}_physical_storage_buffer_ptr                    = OpTypePointer           PhysicalStorageBuffer %${baseType}\n"
			"%data_buffer_physical_storage_buffer_ptr                    = OpTypePointer           PhysicalStorageBuffer %data_buffer\n"
			"%data_buffer_physical_storage_buffer_ptr_storage_buffer_ptr = OpTypePointer           StorageBuffer         %data_buffer_physical_storage_buffer_ptr\n"
			"%storage_buffer_untyped_ptr                                 = OpTypeUntypedPointerKHR StorageBuffer\n"
			"%physical_storage_buffer_untyped_ptr                        = OpTypeUntypedPointerKHR PhysicalStorageBuffer\n"

			"%input_buffer                                               = OpTypeStruct            %physical_storage_buffer_untyped_ptr\n"
			"%output_buffer                                              = OpTypeStruct            %data_buffer_physical_storage_buffer_ptr\n"
			"%output_buffer_storage_buffer_ptr                           = OpTypePointer           StorageBuffer %output_buffer\n"

			"%input_data_untyped_var                                     = OpUntypedVariableKHR    %storage_buffer_untyped_ptr              StorageBuffer %input_buffer\n"
			"%output_data_var                                            = OpVariable              %output_buffer_storage_buffer_ptr        StorageBuffer\n"
		);
		break;
	}
	case PointerTestCases::OP_SELECT_VARIABLE_PTR:
	case PointerTestCases::OP_PHI_VARIABLE_PTR:
	case PointerTestCases::OP_PTR_EQUAL_VARIABLE_PTR:
	case PointerTestCases::OP_PTR_NOT_EQUAL_VARIABLE_PTR:
	{
		variables += std::string(
			/* Structs */
			"%input_buffer_0                   = OpTypeStruct %${baseType}\n"
			"%input_buffer_1                   = OpTypeStruct %${baseType}\n"
			"%output_buffer                    = OpTypeStruct %${baseType}\n"

			/* Pointers */
			"%${baseType}_storage_buffer_ptr   = OpTypePointer           StorageBuffer                     %${baseType}\n"
			"%output_buffer_storage_buffer_ptr = OpTypePointer           StorageBuffer                     %output_buffer\n"
			"%storage_buffer_untyped_ptr       = OpTypeUntypedPointerKHR StorageBuffer\n"

			/* Objects */
			"%input_data_0_untyped_var         = OpUntypedVariableKHR    %storage_buffer_untyped_ptr       StorageBuffer %input_buffer_0\n"
			"%input_data_1_untyped_var         = OpUntypedVariableKHR    %storage_buffer_untyped_ptr       StorageBuffer %input_buffer_1\n"
			"%output_data_var                  = OpVariable              %output_buffer_storage_buffer_ptr StorageBuffer\n"
		);
		break;
	}
	case PointerTestCases::OP_PTR_DIFF_VARIABLE_PTR:
	{
		variables += std::string(
			/* Contants */
			"%c_uint32_${threadCount}          = OpConstant  %uint32      ${threadCount}\n"

			/* Arrays */
			"%array_${baseType}_${threadCount} = OpTypeArray %${baseType} %c_uint32_${threadCount}\n"

			/* Struct */
			"%input_buffer                     = OpTypeStruct            %array_${baseType}_${threadCount}\n"
			"%output_buffer                    = OpTypeStruct            %uint32\n"

			/* Pointers */

			"%uint32_storage_buffer_ptr        = OpTypePointer           StorageBuffer                     %uint32\n"
			"%output_buffer_storage_buffer_ptr = OpTypePointer           StorageBuffer                     %output_buffer\n"
			"%storage_buffer_untyped_ptr       = OpTypeUntypedPointerKHR StorageBuffer\n"

			/* Objects */
			"%input_data_untyped_var           = OpUntypedVariableKHR    %storage_buffer_untyped_ptr       StorageBuffer %input_buffer\n"
			"%output_data_var                  = OpVariable              %output_buffer_storage_buffer_ptr StorageBuffer\n"
		);
		break;
	}
	case PointerTestCases::OP_PTR_ACCESS_CHAIN_VARIABLE_PTR:
	{
		variables += std::string(
			/* Contants */
			"%c_uint32_${threadCount}          = OpConstant  %uint32      ${threadCount}\n"

			/* Arrays */
			"%array_${baseType}_${threadCount} = OpTypeArray %${baseType} %c_uint32_${threadCount}\n"

			/* Struct */
			"%input_buffer                     = OpTypeStruct            %array_${baseType}_${threadCount}\n"
			"%output_buffer                    = OpTypeStruct            %array_${baseType}_${threadCount}\n"

			/* Pointers */
			"%${baseType}_storage_buffer_ptr   = OpTypePointer           StorageBuffer                     %${baseType}\n"
			"%output_buffer_storage_buffer_ptr = OpTypePointer           StorageBuffer                     %output_buffer\n"
			"%storage_buffer_untyped_ptr       = OpTypeUntypedPointerKHR StorageBuffer\n"

			/* Objects */
			"%input_data_untyped_var           = OpUntypedVariableKHR    %storage_buffer_untyped_ptr       StorageBuffer %input_buffer\n"
			"%output_data_var                  = OpVariable              %output_buffer_storage_buffer_ptr StorageBuffer\n"
		);
		break;
	}
	case PointerTestCases::OP_FUNCTION_CALL_VARIABLE_PTR:
	{
		variables += std::string(
			/* Struct */
			"%input_buffer                     = OpTypeStruct %${baseType}\n"
			"%output_buffer                    = OpTypeStruct %${baseType}\n"

			/* Pointers */
			"%${baseType}_storage_buffer_ptr   = OpTypePointer           StorageBuffer                     %${baseType}\n"
			"%output_buffer_storage_buffer_ptr = OpTypePointer           StorageBuffer                     %output_buffer\n"
			"%storage_buffer_untyped_ptr       = OpTypeUntypedPointerKHR StorageBuffer\n"

			/* Objects */
			"%input_data_untyped_var           = OpUntypedVariableKHR    %storage_buffer_untyped_ptr       StorageBuffer %input_buffer\n"
			"%output_data_var                  = OpVariable              %output_buffer_storage_buffer_ptr StorageBuffer\n"
		);
		break;
	}
	case PointerTestCases::MULTIPLE_ACCESS_CHAINS_VARIABLE_PTR:
	{
		variables += std::string(
			/* Base types */
			"%vec2_${baseType}                 = OpTypeVector %${baseType} 2\n"

			/* Struct */
			"%data_buffer                      = OpTypeStruct %${baseType} %${baseType}\n"
			"%input_buffer                     = OpTypeStruct %vec2_${baseType}\n"
			"%output_buffer                    = OpTypeStruct %data_buffer\n"

			/* Pointers */
			"%${baseType}_storage_buffer_ptr   = OpTypePointer           StorageBuffer                     %${baseType}\n"
			"%output_buffer_storage_buffer_ptr = OpTypePointer           StorageBuffer                     %output_buffer\n"
			"%storage_buffer_untyped_ptr       = OpTypeUntypedPointerKHR StorageBuffer\n"

			/* Objects */
			"%input_data_untyped_var           = OpUntypedVariableKHR    %storage_buffer_untyped_ptr       StorageBuffer %input_buffer\n"
			"%output_data_var                  = OpVariable              %output_buffer_storage_buffer_ptr StorageBuffer\n"
		);
		break;
	}
	case PointerTestCases::FUNCTION_VARIABLE_VARIABLE_PTR:
	{
		variables += std::string(
			/* Struct */
			"%input_buffer_0                          = OpTypeStruct %${baseType}\n"
			"%input_buffer_1                          = OpTypeStruct %${baseType}\n"
			"%output_buffer                           = OpTypeStruct %${baseType}\n"

			/* Pointers */
			"%${baseType}_storage_buffer_ptr          = OpTypePointer           StorageBuffer                     %${baseType}\n"
			"%output_buffer_storage_buffer_ptr        = OpTypePointer           StorageBuffer                     %output_buffer\n"
			"%storage_buffer_untyped_ptr              = OpTypeUntypedPointerKHR StorageBuffer\n"
			"%storage_buffer_untyped_ptr_function_ptr = OpTypePointer           Function                          %storage_buffer_untyped_ptr\n"

			/* Objects */
			"%input_data_0_untyped_var                = OpUntypedVariableKHR    %storage_buffer_untyped_ptr       StorageBuffer %input_buffer_0\n"
			"%input_data_1_untyped_var                = OpUntypedVariableKHR    %storage_buffer_untyped_ptr       StorageBuffer %input_buffer_1\n"
			"%output_data_var                         = OpVariable              %output_buffer_storage_buffer_ptr StorageBuffer\n"
		);
		break;
	}
	case PointerTestCases::PRIVATE_VARIABLE_VARIABLE_PTR:
	{
		variables += std::string(
			/* Struct */
			"%input_buffer_0                          = OpTypeStruct %${baseType}\n"
			"%input_buffer_1                          = OpTypeStruct %${baseType}\n"
			"%output_buffer                           = OpTypeStruct %${baseType}\n"

			/* Pointers */
			"%${baseType}_storage_buffer_ptr          = OpTypePointer           StorageBuffer                     %${baseType}\n"
			"%output_buffer_storage_buffer_ptr        = OpTypePointer           StorageBuffer                     %output_buffer\n"
			"%storage_buffer_untyped_ptr              = OpTypeUntypedPointerKHR StorageBuffer\n"
			"%storage_buffer_untyped_ptr_private_ptr  = OpTypePointer           Private                           %storage_buffer_untyped_ptr\n"

			/* Objects */
			"%input_data_0_untyped_var                = OpUntypedVariableKHR    %storage_buffer_untyped_ptr             StorageBuffer %input_buffer_0\n"
			"%input_data_1_untyped_var                = OpUntypedVariableKHR    %storage_buffer_untyped_ptr             StorageBuffer %input_buffer_1\n"
			"%output_data_var                         = OpVariable              %output_buffer_storage_buffer_ptr       StorageBuffer\n"
			"%output_copy_private_var                 = OpVariable              %storage_buffer_untyped_ptr_private_ptr Private\n"
		);
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
		"%id              = OpVariable           %vec3_uint32_input_ptr                            Input\n"
	);

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
		"%vec3_uint32_input_ptr = OpTypePointer Input %vec3_uint32\n"
	);

	switch (testCase)
	{
	case WorkgroupTestCases::NOT_ALIASED:
	case WorkgroupTestCases::ALIASED:
	{
		variables += std::string(
			/* Struct */
			"%input_buffer                        = OpTypeStruct            %vec4_${baseType} %${baseType}\n"
			"%output_buffer                       = OpTypeStruct            %vec4_${baseType} %${baseType}\n"
			"%data_buffer                         = OpTypeStruct            %vec4_${baseType} %${baseType}\n"

			/* Pointers */
			"%${baseType}_storage_buffer_ptr      = OpTypePointer           StorageBuffer     %${baseType}\n"
			"%vec4_${baseType}_storage_buffer_ptr = OpTypePointer           StorageBuffer     %vec4_${baseType}\n"
			"%${baseType}_workgroup_ptr           = OpTypePointer           Workgroup         %${baseType}\n"
			"%vec4_${baseType}_workgroup_ptr      = OpTypePointer           Workgroup         %vec4_${baseType}\n"
			"%input_buffer_storage_buffer_ptr     = OpTypePointer           StorageBuffer     %input_buffer\n"
			"%output_buffer_storage_buffer_ptr    = OpTypePointer           StorageBuffer     %output_buffer\n"
			"%workgroup_untyped_ptr               = OpTypeUntypedPointerKHR Workgroup\n"

			/* Objects */
			"%input_data_var                      = OpVariable              %input_buffer_storage_buffer_ptr  StorageBuffer\n"
			"%output_data_var                     = OpVariable              %output_buffer_storage_buffer_ptr StorageBuffer\n"
			"%data_buffer_0_untyped_var           = OpUntypedVariableKHR    %workgroup_untyped_ptr            Workgroup     %data_buffer\n"
			"%data_buffer_1_untyped_var           = OpUntypedVariableKHR    %workgroup_untyped_ptr            Workgroup     %data_buffer\n"
		);
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
		"%id              = OpVariable           %vec3_uint32_input_ptr                            Input\n"
	);

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
			"%c_matrix_rows          = OpConstant %uint32 1\n"
			"%c_matrix_cols          = OpConstant %uint32 1\n"
			"%c_uint32_2             = OpConstant %uint32 2\n"
			"%c_matrix_use           = OpConstant %uint32 ${matrixUse}\n"
			"%c_matrix_layout        = OpConstant %uint32 ${matrixLayout}\n"

			/* Cooperative matrix */
			"%${baseType}_matrix_1x1 = OpTypeCooperativeMatrixKHR %${baseType} %c_uint32_0 %c_matrix_rows %c_matrix_cols %c_matrix_use\n"

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
			"%input_data_untyped_var              = OpUntypedVariableKHR    %storage_buffer_untyped_ptr       StorageBuffer %input_buffer\n"
			"%output_data_var                     = OpVariable              %output_buffer_storage_buffer_ptr StorageBuffer\n"
			"%id                                  = OpVariable              %vec3_uint32_input_ptr            Input\n"
		);
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
			"%c_matrix_rows          = OpConstant %uint32 1\n"
			"%c_matrix_cols          = OpConstant %uint32 1\n"
			"%c_uint32_2             = OpConstant %uint32 2\n"
			"%c_matrix_use           = OpConstant %uint32 ${matrixUse}\n"
			"%c_matrix_layout        = OpConstant %uint32 ${matrixLayout}\n"

			/* Cooperative matrix */
			"%${baseType}_matrix_1x1 = OpTypeCooperativeMatrixKHR %${baseType} %c_uint32_0 %c_matrix_rows %c_matrix_cols %c_matrix_use\n"

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
			"%input_data_var                      = OpVariable              %input_buffer_storage_buffer_ptr StorageBuffer\n"
			"%output_data_untyped_var             = OpUntypedVariableKHR    %storage_buffer_untyped_ptr      StorageBuffer %output_buffer\n"
			"%id                                  = OpVariable              %vec3_uint32_input_ptr           Input\n"
		);
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
			"%c_matrix_rows          = OpConstant %uint32 1\n"
			"%c_matrix_cols          = OpConstant %uint32 1\n"
			"%c_uint32_2             = OpConstant %uint32 2\n"
			"%c_matrix_use           = OpConstant %uint32 ${matrixUse}\n"
			"%c_matrix_layout        = OpConstant %uint32 ${matrixLayout}\n"

			/* Cooperative matrix */
			"%${sameSizeType}_matrix_1x1 = OpTypeCooperativeMatrixKHR %${sameSizeType} %c_uint32_0 %c_matrix_rows %c_matrix_cols %c_matrix_use\n"

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
			"%input_data_untyped_var              = OpUntypedVariableKHR    %storage_buffer_untyped_ptr       StorageBuffer %input_buffer\n"
			"%output_data_var                     = OpVariable              %output_buffer_storage_buffer_ptr StorageBuffer\n"
			"%id                                  = OpVariable              %vec3_uint32_input_ptr            Input\n"
		);
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
			"%c_matrix_rows          = OpConstant %uint32 1\n"
			"%c_matrix_cols          = OpConstant %uint32 1\n"
			"%c_uint32_2             = OpConstant %uint32 2\n"
			"%c_matrix_use           = OpConstant %uint32 ${matrixUse}\n"
			"%c_matrix_layout        = OpConstant %uint32 ${matrixLayout}\n"

			/* Cooperative matrix */
			"%${baseType}_matrix_1x1 = OpTypeCooperativeMatrixKHR %${baseType} %c_uint32_0 %c_matrix_rows %c_matrix_cols %c_matrix_use\n"

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
			"%input_data_var                      = OpVariable              %input_buffer_storage_buffer_ptr StorageBuffer\n"
			"%output_data_untyped_var             = OpUntypedVariableKHR    %storage_buffer_untyped_ptr      StorageBuffer %output_buffer\n"
			"%id                                  = OpVariable              %vec3_uint32_input_ptr           Input\n"
		);
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
			"%c_matrix_rows          = OpConstant %uint32 2\n"
			"%c_matrix_cols          = OpConstant %uint32 2\n"
			"%c_uint32_2             = OpConstant %uint32 2\n"
			"%c_matrix_use           = OpConstant %uint32 ${matrixUse}\n"
			"%c_matrix_layout        = OpConstant %uint32 ${matrixLayout}\n"
			"%c_matrix_stride        = OpConstant %uint32 ${matrixStride}\n"

			/* Cooperative matrix */
			"%${baseType}_matrix_2x2 = OpTypeCooperativeMatrixKHR %${baseType} %c_uint32_0 %c_matrix_rows %c_matrix_cols %c_matrix_use\n"

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
			"%input_data_untyped_var              = OpUntypedVariableKHR    %storage_buffer_untyped_ptr       StorageBuffer %input_buffer\n"
			"%output_data_var                     = OpVariable              %output_buffer_storage_buffer_ptr StorageBuffer\n"
			"%id                                  = OpVariable              %vec3_uint32_input_ptr            Input\n"
		);
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
			"%c_matrix_rows          = OpConstant %uint32 2\n"
			"%c_matrix_cols          = OpConstant %uint32 2\n"
			"%c_uint32_2             = OpConstant %uint32 2\n"
			"%c_matrix_use           = OpConstant %uint32 ${matrixUse}\n"
			"%c_matrix_layout        = OpConstant %uint32 ${matrixLayout}\n"
			"%c_matrix_stride        = OpConstant %uint32 ${matrixStride}\n"

			/* Cooperative matrix */
			"%${baseType}_matrix_2x2 = OpTypeCooperativeMatrixKHR %${baseType} %c_uint32_0 %c_matrix_rows %c_matrix_cols %c_matrix_use\n"

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
			"%input_data_var                      = OpVariable              %input_buffer_storage_buffer_ptr StorageBuffer\n"
			"%output_data_untyped_var             = OpUntypedVariableKHR    %storage_buffer_untyped_ptr      StorageBuffer %output_buffer\n"
			"%id                                  = OpVariable              %vec3_uint32_input_ptr           Input\n"
		);
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
		function += std::string(
			"%simple_function_type  = OpTypeFunction      %physical_storage_buffer_untyped_ptr %physical_storage_buffer_untyped_ptr\n"
			"%simple_function       = OpFunction          %physical_storage_buffer_untyped_ptr None %simple_function_type\n"
			"%input_ptr             = OpFunctionParameter %physical_storage_buffer_untyped_ptr\n"
			"%label_simple_function = OpLabel\n"
			"                         OpReturnValue       %input_ptr\n"
			"                         OpFunctionEnd\n"
		);
	}
	else // opType == PointerTestCases::FUNCTION_PARAMETERS_VARIABLE_PTR
	{
		function += std::string(
			"%simple_function_type  = OpTypeFunction      %storage_buffer_untyped_ptr %storage_buffer_untyped_ptr\n"
			"%simple_function       = OpFunction          %storage_buffer_untyped_ptr None %simple_function_type\n"
			"%input_ptr             = OpFunctionParameter %storage_buffer_untyped_ptr\n"
			"%label_simple_function = OpLabel\n"
			"                         OpReturnValue       %input_ptr\n"
			"                         OpFunctionEnd\n"
		);
	}

	return function;
}

std::string createShaderMain(BASE_TEST_CASE testCase)
{
	std::string main = std::string(
		"%main               = OpFunction %void None %void_func\n"
		"%label_main         = OpLabel\n"
	);

	switch (testCase)
	{
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
			"%rta_elem            = OpUntypedAccessChainKHR %storage_buffer_untyped_ptr %input_buffer %input_data_untyped_var %c_uint32_0 %curr_ndx\n"
			"                       OpStore       %rta_elem                        %c_${baseType}_1\n"
			"                       OpBranch      %label_3\n"
			"%label_3             = OpLabel\n"
			"%new_ndx             = OpIAdd        %uint32   %curr_ndx %c_uint32_1\n"
			"                       OpStore       %ndx      %new_ndx\n"
			"                       OpBranch      %label_0\n"
			"%label_4             = OpLabel\n"

			"%runtime_size        = OpUntypedArrayLengthKHR %uint32                    %input_buffer    %input_data_untyped_var 0\n"
			"%array_size_loc      = OpAccessChain           %uint32_storage_buffer_ptr %output_data_var %c_uint32_0\n"

			"                       OpStore                 %array_size_loc            %runtime_size\n"
		);
		break;
	}
	case BaseTestCases::LOAD:
	{
		main += std::string(
			"%id_loc              = OpAccessChain           %uint32_input_ptr               %id                 %c_uint32_0\n"
			"%x                   = OpLoad                  %uint32                         %id_loc\n"

			"%input_data_var_loc  = OpUntypedAccessChainKHR %storage_buffer_untyped_ptr     %input_buffer       %input_data_untyped_var %c_uint32_0 %x\n"
			"%output_data_var_loc = OpAccessChain           %storage_buffer_${baseType}_ptr %output_data_var                            %c_uint32_0 %x\n"

			"%temp_data_var_loc   = ${loadOp}               %${baseType}                    %input_data_var_loc ${args}\n"
			"                       OpStore                 %output_data_var_loc            %temp_data_var_loc\n"
		);
		break;
	}
	case BaseTestCases::COPY_FROM:
	{
		main += std::string(
			"%id_loc              = OpAccessChain           %uint32_input_ptr               %id           %c_uint32_0\n"
			"%x                   = OpLoad                  %uint32                         %id_loc\n"

			"%input_data_var_loc  = OpUntypedAccessChainKHR %storage_buffer_untyped_ptr     %input_buffer %input_data_untyped_var %c_uint32_0 %x\n"
			"%output_data_var_loc = OpAccessChain           %storage_buffer_${baseType}_ptr               %output_data_var        %c_uint32_0 %x\n"

			"${copyOp}\n"
		);
		break;
	}
	case BaseTestCases::STORE:
	{
		main += std::string(
			"%id_loc              = OpAccessChain           %uint32_input_ptr               %id                 %c_uint32_0\n"
			"%x                   = OpLoad                  %uint32                         %id_loc\n"

			"%input_data_var_loc  = OpAccessChain           %storage_buffer_${baseType}_ptr                     %input_data_var          %c_uint32_0 %x\n"
			"%output_data_var_loc = OpUntypedAccessChainKHR %storage_buffer_untyped_ptr     %output_buffer      %output_data_untyped_var %c_uint32_0 %x\n"

			"%temp_data_var_loc   = OpLoad                  %${baseType}                    %input_data_var_loc\n"
			"                       ${storeOp}              %output_data_var_loc  ${args}   %temp_data_var_loc\n"
		);
		break;
	}
	case BaseTestCases::COPY_TO:
	{
		main += std::string(
			"%id_loc              = OpAccessChain           %uint32_input_ptr               %id            %c_uint32_0\n"
			"%x                   = OpLoad                  %uint32                         %id_loc\n"

			"%input_data_var_loc  = OpAccessChain           %storage_buffer_${baseType}_ptr                %input_data_var          %c_uint32_0 %x\n"
			"%output_data_var_loc = OpUntypedAccessChainKHR %storage_buffer_untyped_ptr     %output_buffer %output_data_untyped_var %c_uint32_0 %x\n"

			"${copyOp}\n"
		);
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
		"                     OpFunctionEnd\n"
	);

	return main;
}

std::string createShaderMain(ATOMIC_TEST_CASE testCase)
{
	std::string main = std::string(
		"%main       = OpFunction %void None %void_func\n"
		"%label_main = OpLabel\n"
	);

	switch (testCase)
	{
	case AtomicTestCases::OP_ATOMIC_INCREMENT:
	case AtomicTestCases::OP_ATOMIC_DECREMENT:
	{
		main += std::string(
			"%output_data_var_loc = OpUntypedAccessChainKHR %storage_buffer_untyped_ptr %output_buffer %output_data_untyped_var %c_uint32_0\n"
			"%return_val          = ${opType}               %${baseType}                               %output_data_var_loc     %c_uint32_1 %c_uint32_0\n"
		);
		break;
	}
	case AtomicTestCases::OP_ATOMIC_ADD:
	case AtomicTestCases::OP_ATOMIC_SUB:
	case AtomicTestCases::OP_ATOMIC_MIN:
	case AtomicTestCases::OP_ATOMIC_MAX:
	case AtomicTestCases::OP_ATOMIC_EXCHANGE:
	{
		main += std::string(
			"%output_data_var_loc = OpUntypedAccessChainKHR %storage_buffer_untyped_ptr %output_buffer %output_data_untyped_var %c_uint32_0\n"
			"%return_val          = ${opType}               %${baseType}                               %output_data_var_loc     %c_uint32_1 %c_uint32_0 %op_value\n"
		);
		break;
	}
	case AtomicTestCases::OP_ATOMIC_AND:
	case AtomicTestCases::OP_ATOMIC_OR:
	case AtomicTestCases::OP_ATOMIC_XOR:
	{
		main += std::string(
			"%output_data_var_loc = OpUntypedAccessChainKHR %storage_buffer_untyped_ptr %output_buffer %output_data_untyped_var %c_uint32_0\n"
			"%unused_id           = ${opMin}                %${baseType}                               %output_data_var_loc     %c_uint32_1 %c_uint32_0 %c_${baseType}_1\n"
			"%return_val          = ${opType}               %${baseType}                               %output_data_var_loc     %c_uint32_1 %c_uint32_0 %op_value\n"
		);
		break;
	}
	case AtomicTestCases::OP_ATOMIC_COMPARE_EXCHANGE:
	{
		main += std::string(
			"%output_data_var_loc = OpUntypedAccessChainKHR %storage_buffer_untyped_ptr %output_buffer %output_data_untyped_var %c_uint32_0\n"
			"%unused_id           = ${opMin}                %${baseType}                               %output_data_var_loc     %c_uint32_1 %c_uint32_0 %c_${baseType}_1\n"
			"%return_val          = ${opType}               %${baseType}                               %output_data_var_loc     %c_uint32_1 %c_uint32_0 %c_uint32_0 %op_value %comp\n"
		);
		break;
	}
	case AtomicTestCases::OP_ATOMIC_LOAD:
	{
		main += std::string(
			"%id_loc              = OpAccessChain %uint32_input_ptr %id     %c_uint32_0\n"
			"%x                   = OpLoad        %uint32           %id_loc\n"

			"%input_data_var_loc  = OpUntypedAccessChainKHR %storage_buffer_untyped_ptr     %input_buffer    %input_data_untyped_var %c_uint32_0 %x\n"
			"%output_data_var_loc = OpAccessChain           %storage_buffer_${baseType}_ptr %output_data_var                         %c_uint32_0 %x\n"

			"%temp_data_var_loc   = ${loadOp} %${baseType} %input_data_var_loc ${args}\n"
			"                       OpStore   %output_data_var_loc %temp_data_var_loc\n"
		);
		break;
	}
	case AtomicTestCases::OP_ATOMIC_STORE:
	{
		main += std::string(
			"%id_loc              = OpAccessChain %uint32_input_ptr %id     %c_uint32_0\n"
			"%x                   = OpLoad        %uint32           %id_loc\n"

			"%input_data_var_loc  = OpAccessChain           %storage_buffer_${baseType}_ptr %input_data_var                          %c_uint32_0 %x\n"
			"%output_data_var_loc = OpUntypedAccessChainKHR %storage_buffer_untyped_ptr     %output_buffer  %output_data_untyped_var %c_uint32_0 %x\n"

			"%temp_data_var_loc   = OpLoad %${baseType} %input_data_var_loc\n"
			"                     ${storeOp} %output_data_var_loc ${args} %temp_data_var_loc\n"
		);
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
		"                     OpFunctionEnd\n"
	);

	return main;
}

std::string createShaderMain(TYPE_PUNNING_TEST_CASE testCase)
{
	std::string main = std::string(
		"%main       = OpFunction %void None %void_func\n"
		"%label_main = OpLabel\n"
	);

	switch (testCase)
	{
	case TypePunningTestCases::LOAD_SAME_SIZE_TYPES:
	{
		main += std::string(
			"%id_loc              = OpAccessChain           %uint32_input_ptr                   %id                 %c_uint32_0\n"
			"%x                   = OpLoad                  %uint32                             %id_loc\n"

			"%input_data_var_loc  = OpUntypedAccessChainKHR %storage_buffer_untyped_ptr         %input_buffer       %input_data_untyped_var %c_uint32_0 %x\n"
			"%output_data_var_loc = OpAccessChain           %storage_buffer_${sameSizeType}_ptr %output_data_var                            %c_uint32_0 %x\n"

			"%temp_data_var_loc   = ${loadOp}               %${sameSizeType}                    %input_data_var_loc ${args}\n"
			"                       OpStore                 %output_data_var_loc                %temp_data_var_loc\n"
		);
		break;
	}
	case TypePunningTestCases::LOAD_SCALAR_VECTOR:
	{
		main += std::string(
			"%input_data_var_loc  = OpUntypedAccessChainKHR %storage_buffer_untyped_ptr     %input_buffer       %input_data_untyped_var %c_uint32_0\n"
			"%output_data_var_loc = OpAccessChain           %storage_buffer_${otherVec}_ptr                     %output_data_var        %c_uint32_0\n"

			"%temp_data_var_loc   = ${loadOp}               %${otherVec}                    %input_data_var_loc ${args}\n"
			"                       OpStore                 %output_data_var_loc            %temp_data_var_loc\n"
		);
		break;
	}
	case TypePunningTestCases::LOAD_VECTOR_SCALAR:
	{
		main += std::string(
			"%input_data_var_loc  = OpUntypedAccessChainKHR %storage_buffer_untyped_ptr      %input_buffer       %input_data_untyped_var %c_uint32_0\n"
			"%output_data_var_loc = OpAccessChain           %storage_buffer_${otherType}_ptr                     %output_data_var        %c_uint32_0\n"

			"%temp_data_var_loc   = ${loadOp}               %${otherType}                    %input_data_var_loc ${args}\n"
			"                       OpStore                 %output_data_var_loc             %temp_data_var_loc\n"
		);
		break;
	}
	case TypePunningTestCases::COPY_FROM_SAME_SIZE_TYPES:
	{
		main += std::string(
			"%id_loc              = OpAccessChain           %uint32_input_ptr                   %id              %c_uint32_0\n"
			"%x                   = OpLoad                  %uint32                             %id_loc\n"

			"%input_data_var_loc  = OpUntypedAccessChainKHR %storage_buffer_untyped_ptr         %input_buffer    %input_data_untyped_var %c_uint32_0 %x\n"
			"%output_data_var_loc = OpAccessChain           %storage_buffer_${sameSizeType}_ptr %output_data_var                         %c_uint32_0 %x\n"

			"${copyOp}\n"
		);
		break;
	}
	case TypePunningTestCases::STORE_SAME_SIZE_TYPES:
	{
		main += std::string(
			"%id_loc              = OpAccessChain           %uint32_input_ptr               %id                 %c_uint32_0\n"
			"%x                   = OpLoad                  %uint32                         %id_loc\n"

			"%input_data_var_loc  = OpAccessChain           %storage_buffer_${baseType}_ptr                     %input_data_var          %c_uint32_0 %x\n"
			"%output_data_var_loc = OpUntypedAccessChainKHR %storage_buffer_untyped_ptr     %output_buffer      %output_data_untyped_var %c_uint32_0 %x\n"

			"%temp_data_var_loc   = OpLoad                  %${baseType}                    %input_data_var_loc\n"
			"                     ${storeOp}                %output_data_var_loc   ${args}  %temp_data_var_loc\n"
		);
		break;
	}
	case TypePunningTestCases::STORE_SCALAR_VECTOR:
	{
		main += std::string(
			"%input_data_var_loc  = OpAccessChain           %storage_buffer_${baseType}_ptr                     %input_data_var          %c_uint32_0\n"
			"%output_data_var_loc = OpUntypedAccessChainKHR %storage_buffer_untyped_ptr     %output_buffer      %output_data_untyped_var %c_uint32_0\n"

			"%temp_data_var_loc   = OpLoad                  %${baseType}                    %input_data_var_loc\n"
			"                       ${storeOp}              %output_data_var_loc  ${args}   %temp_data_var_loc\n"
		);
		break;
	}
	case TypePunningTestCases::STORE_VECTOR_SCALAR:
	{
		main += std::string(
			"%input_data_var_loc  = OpAccessChain           %storage_buffer_${baseVec}_ptr                      %input_data_var          %c_uint32_0\n"
			"%output_data_var_loc = OpUntypedAccessChainKHR %storage_buffer_untyped_ptr    %output_buffer       %output_data_untyped_var %c_uint32_0\n"

			"%temp_data_var_loc   = OpLoad                  %${baseVec}                    %input_data_var_loc\n"
			"                       ${storeOp}              %output_data_var_loc  ${args}  %temp_data_var_loc\n"
		);
		break;
	}
	case TypePunningTestCases::COPY_TO_SAME_SIZE_TYPES:
	{
		main += std::string(
			"%id_loc              = OpAccessChain           %uint32_input_ptr               %id            %c_uint32_0\n"
			"%x                   = OpLoad                  %uint32                         %id_loc\n"

			"%input_data_var_loc  = OpAccessChain           %storage_buffer_${baseType}_ptr                %input_data_var          %c_uint32_0 %x\n"
			"%output_data_var_loc = OpUntypedAccessChainKHR %storage_buffer_untyped_ptr     %output_buffer %output_data_untyped_var %c_uint32_0 %x\n"

			"${copyOp}\n"
		);
		break;
	}
	case TypePunningTestCases::COPY_TO_SCALAR_VECTOR:
	{
		main += std::string(
			"%input_data_var_loc  = OpAccessChain           %storage_buffer_${baseType}_ptr %input_data_var %c_uint32_0\n"
			"%output_data_var_loc = OpUntypedAccessChainKHR %storage_buffer_untyped_ptr     %output_buffer  %output_data_untyped_var %c_uint32_0\n"

			"${copyOp}\n"
		);
		break;
	}
	case TypePunningTestCases::COPY_TO_VECTOR_SCALAR:
	{
		main += std::string(
			"%input_data_var_loc  = OpAccessChain           %storage_buffer_${baseVec}_ptr %input_data_var %c_uint32_0\n"
			"%output_data_var_loc = OpUntypedAccessChainKHR %storage_buffer_untyped_ptr    %output_buffer  %output_data_untyped_var %c_uint32_0\n"

			"${copyOp}\n"
		);
		break;
	}
	case TypePunningTestCases::COPY_FROM_SCALAR_VECTOR:
	{
		main += std::string(
			"%input_data_var_loc  = OpUntypedAccessChainKHR %storage_buffer_untyped_ptr     %input_buffer    %input_data_untyped_var %c_uint32_0\n"
			"%output_data_var_loc = OpAccessChain           %storage_buffer_${otherVec}_ptr %output_data_var %c_uint32_0\n"

			"${copyOp}\n"
		);
		break;
	}
	case TypePunningTestCases::COPY_FROM_VECTOR_SCALAR:
	{
		main += std::string(
			"%input_data_var_loc  = OpUntypedAccessChainKHR %storage_buffer_untyped_ptr     %input_buffer    %input_data_untyped_var %c_uint32_0\n"
			"%output_data_var_loc = OpAccessChain           %storage_buffer_${otherType}_ptr %output_data_var %c_uint32_0\n"

			"${copyOp}\n"
		);
		break;
	}
	case TypePunningTestCases::MULTIPLE_ACCESS_CHAINS:
	{
		main += std::string(
			"%input_data_var_loc  = OpUntypedAccessChainKHR %storage_buffer_untyped_ptr         %input_buffer        %input_data_untyped_var\n"
			"%data_var_loc        = OpUntypedAccessChainKHR %storage_buffer_untyped_ptr         %output_buffer       %input_data_var_loc\n"
			"%loaded_data         = OpLoad                  %output_buffer                      %data_var_loc\n"
			"%output_data_var_loc = OpAccessChain           %output_buffer_storage_buffer_ptr   %output_data_var\n"
			"                       OpStore                 %output_data_var_loc                %loaded_data\n"
		);
		break;
	}
	case TypePunningTestCases::CUSTOM_STRUCT_TYPE:
	{
		main += std::string(
			"%input_data_var_loc  = OpUntypedAccessChainKHR %storage_buffer_untyped_ptr         %output_buffer       %input_data_untyped_var\n"
			"%loaded_input        = OpLoad                  %output_buffer                      %input_data_var_loc\n"
			"%output_data_var_loc = OpAccessChain           %output_buffer_storage_buffer_ptr   %output_data_var\n"
			"                       OpStore                 %output_data_var_loc                %loaded_input\n"
		);
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
		"                     OpFunctionEnd\n"
	);

	return main;
}

std::string createShaderMain(POINTER_TEST_CASE testCase)
{
	std::string main = std::string(
		"%main       = OpFunction %void None %void_func\n"
		"%label_main = OpLabel\n"
	);

	switch (testCase)
	{
	case PointerTestCases::OP_BITCAST_FROM_UNTYPED_PHYSICAL_STORAGE:
	{
		main += std::string(
			"%input_ptr_ptr_loc  = OpUntypedAccessChainKHR %storage_buffer_untyped_ptr              %data_buffer           %input_data_untyped_var %c_uint32_0\n"
			"%output_ptr_ptr_loc = OpAccessChain           %data_buffer_physical_storage_buffer_ptr_storage_buffer_ptr     %output_data_var        %c_uint32_0\n"

			"%input_ptr_loc      = OpLoad                  %physical_storage_buffer_untyped_ptr     %input_ptr_ptr_loc\n"
			"%output_ptr_loc     = OpLoad                  %data_buffer_physical_storage_buffer_ptr %output_ptr_ptr_loc\n"

			"%input_loc          = OpLoad                  %physical_storage_buffer_untyped_ptr     %input_ptr_loc         Aligned        ${alignment}\n"
			"%output_loc         = OpAccessChain           %${baseType}_physical_storage_buffer_ptr %output_ptr_loc        %c_uint32_0\n"

			"%bitcasted          = OpBitcast               %${baseType}_physical_storage_buffer_ptr %input_loc\n"
			"%bitcasted_loc      = OpLoad                  %${baseType}                             %bitcasted             Aligned          ${alignment}\n"
			"                      OpStore                 %output_loc                              %bitcasted_loc         Aligned          ${alignment}\n"
		);
		break;
	}
	case PointerTestCases::OP_BITCAST_TO_UNTYPED_PHYSICAL_STORAGE:
	{
		main += std::string(
			"%input_ptr_ptr_loc  = OpAccessChain           %data_buffer_physical_storage_buffer_ptr_storage_buffer_ptr     %input_data_var          %c_uint32_0\n"
			"%output_ptr_ptr_loc = OpUntypedAccessChainKHR %storage_buffer_untyped_ptr              %data_buffer           %output_data_untyped_var %c_uint32_0\n"

			"%input_ptr_loc      = OpLoad                  %data_buffer_physical_storage_buffer_ptr %input_ptr_ptr_loc\n"
			"%output_ptr_loc     = OpLoad                  %physical_storage_buffer_untyped_ptr     %output_ptr_ptr_loc\n"

			"%input_loc          = OpAccessChain           %${baseType}_physical_storage_buffer_ptr %input_ptr_loc         %c_uint32_0\n"
			"%output_loc         = OpUntypedAccessChainKHR %physical_storage_buffer_untyped_ptr     %output_buffer         %output_ptr_loc %c_uint32_0\n"

			"%bitcasted          = OpBitcast               %physical_storage_buffer_untyped_ptr     %input_loc\n"
			"%bitcasted_loc      = OpLoad                  %${baseType}                             %bitcasted             Aligned          ${alignment}\n"
			"                      OpStore                 %output_loc                              %bitcasted_loc         Aligned          ${alignment}\n"
		);
		break;
	}
	case PointerTestCases::OP_SELECT_PHYSICAL_STORAGE:
	{
		main += std::string(
			"%input_ptr_ptr_loc_first  = OpUntypedAccessChainKHR %storage_buffer_untyped_ptr              %data_buffer              %input_data_0_untyped_var %c_uint32_0\n"
			"%input_ptr_ptr_loc_second = OpUntypedAccessChainKHR %storage_buffer_untyped_ptr              %data_buffer              %input_data_1_untyped_var %c_uint32_0\n"
			"%output_ptr_ptr_loc       = OpAccessChain           %data_buffer_physical_storage_buffer_ptr_storage_buffer_ptr        %output_data_var          %c_uint32_0\n"

			"%input_ptr_loc_first      = OpLoad                  %physical_storage_buffer_untyped_ptr     %input_ptr_ptr_loc_first\n"
			"%input_ptr_loc_second     = OpLoad                  %physical_storage_buffer_untyped_ptr     %input_ptr_ptr_loc_second\n"
			"%output_ptr_loc           = OpLoad                  %data_buffer_physical_storage_buffer_ptr %output_ptr_ptr_loc\n"

			"%input_loc_first          = OpLoad                  %physical_storage_buffer_untyped_ptr     %input_ptr_loc_first      Aligned          ${alignment}\n"
			"%input_loc_second         = OpLoad                  %physical_storage_buffer_untyped_ptr     %input_ptr_loc_second     Aligned          ${alignment}\n"
			"%output_loc               = OpAccessChain           %${baseType}_physical_storage_buffer_ptr %output_ptr_loc           %c_uint32_0\n"

			"%selected_ptr             = OpSelect                %physical_storage_buffer_untyped_ptr     ${condition}              %input_loc_first %input_loc_second\n"

			"%selected_ptr_loc         = OpLoad                  %${baseType}                             %selected_ptr             Aligned          ${alignment}\n"
			"                            OpStore                 %output_loc                              %selected_ptr_loc         Aligned          ${alignment}\n"
		);
		break;
	}
	case PointerTestCases::OP_PHI_PHYSICAL_STORAGE:
	{
		main += std::string(
			"%input_ptr_ptr_loc_first  = OpUntypedAccessChainKHR %storage_buffer_untyped_ptr              %data_buffer              %input_data_0_untyped_var %c_uint32_0\n"
			"%input_ptr_ptr_loc_second = OpUntypedAccessChainKHR %storage_buffer_untyped_ptr              %data_buffer              %input_data_1_untyped_var %c_uint32_0\n"
			"%output_ptr_ptr_loc       = OpAccessChain           %data_buffer_physical_storage_buffer_ptr_storage_buffer_ptr        %output_data_var          %c_uint32_0\n"

			"%input_ptr_loc_first      = OpLoad                  %physical_storage_buffer_untyped_ptr     %input_ptr_ptr_loc_first\n"
			"%input_ptr_loc_second     = OpLoad                  %physical_storage_buffer_untyped_ptr     %input_ptr_ptr_loc_second\n"
			"%output_ptr_loc           = OpLoad                  %data_buffer_physical_storage_buffer_ptr %output_ptr_ptr_loc\n"

			"%input_loc_first          = OpLoad                  %physical_storage_buffer_untyped_ptr     %input_ptr_loc_first      Aligned          ${alignment}\n"
			"%input_loc_second         = OpLoad                  %physical_storage_buffer_untyped_ptr     %input_ptr_loc_second     Aligned          ${alignment}\n"
			"%output_loc               = OpAccessChain           %${baseType}_physical_storage_buffer_ptr %output_ptr_loc           %c_uint32_0\n"

			"                            OpSelectionMerge        %end_label                               None\n"
			"                            OpBranchConditional     ${condition}                             %take_input_0             %take_input_1\n"
			"%take_input_0             = OpLabel\n"
			"                            OpBranch                %end_label\n"
			"%take_input_1             = OpLabel\n"
			"                            OpBranch                %end_label\n"
			"%end_label                = OpLabel\n"

			"%selected_ptr             = OpPhi                   %physical_storage_buffer_untyped_ptr   %input_loc_first            %take_input_0    %input_loc_second   %take_input_1\n"
			"%selected_ptr_loc         = OpLoad                  %${baseType}                           %selected_ptr               Aligned          ${alignment}\n"
			"                            OpStore                 %output_loc                            %selected_ptr_loc           Aligned          ${alignment}\n"
		);
		break;
	}
	case PointerTestCases::OP_FUNCTION_CALL_PHYSICAL_STORAGE:
	{
		main += std::string(
			"%input_ptr_ptr_loc    = OpUntypedAccessChainKHR    %storage_buffer_untyped_ptr              %data_buffer              %input_data_untyped_var %c_uint32_0\n"
			"%output_ptr_ptr_loc   = OpAccessChain              %data_buffer_physical_storage_buffer_ptr_storage_buffer_ptr        %output_data_var        %c_uint32_0\n"

			"%input_ptr_loc        = OpLoad                     %physical_storage_buffer_untyped_ptr     %input_ptr_ptr_loc\n"
			"%output_ptr_loc       = OpLoad                     %data_buffer_physical_storage_buffer_ptr %output_ptr_ptr_loc\n"

			"%input_loc            = OpLoad                     %physical_storage_buffer_untyped_ptr     %input_ptr_loc            Aligned                 ${alignment}\n"
			"%output_loc           = OpAccessChain              %${baseType}_physical_storage_buffer_ptr %output_ptr_loc           %c_uint32_0\n"

			"%returned_ptr         = OpFunctionCall             %physical_storage_buffer_untyped_ptr     %simple_function %input_loc\n"

			"%returned_ptr_loc     = OpLoad                     %${baseType}                             %returned_ptr             Aligned                 ${alignment}\n"
			"                        OpStore                    %output_loc                              %returned_ptr_loc         Aligned                 ${alignment}\n"
		);
		break;
	}
	case PointerTestCases::OP_PTR_ACCESS_CHAIN_PHYSICAL_STORAGE:
	{
		main += std::string(
			"%id_loc               = OpAccessChain              %uint32_input_ptr                                                  %id                     %c_uint32_0\n"
			"%x                    = OpLoad                     %uint32                                  %id_loc\n"

			"%input_ptr_ptr_loc    = OpUntypedAccessChainKHR    %storage_buffer_untyped_ptr              %data_buffer              %input_data_untyped_var %c_uint32_0\n"
			"%output_ptr_ptr_loc   = OpAccessChain              %data_buffer_physical_storage_buffer_ptr_storage_buffer_ptr        %output_data_var        %c_uint32_0\n"

			"%input_ptr_loc        = OpLoad                     %physical_storage_buffer_untyped_ptr     %input_ptr_ptr_loc\n"
			"%output_ptr_loc       = OpLoad                     %data_buffer_physical_storage_buffer_ptr %output_ptr_ptr_loc\n"

			"%input_loc            = OpLoad                     %physical_storage_buffer_untyped_ptr     %input_ptr_loc            Aligned                 ${alignment}\n"
			"%output_loc           = OpAccessChain              %${baseType}_physical_storage_buffer_ptr %output_ptr_loc           %c_uint32_0             %x\n"

			"%input_loc_0          = OpUntypedAccessChainKHR    %physical_storage_buffer_untyped_ptr     %data_buffer              %input_loc              %c_uint32_0   %c_uint32_0\n"
			"%input_loc_x          = OpUntypedPtrAccessChainKHR %physical_storage_buffer_untyped_ptr     %data_buffer              %input_loc_0            %x\n"

			"%input_loc_x_ptr     = OpLoad                      %${baseType}                             %input_loc_x              Aligned                 ${alignment}\n"
			"                       OpStore                     %output_loc                              %input_loc_x_ptr          Aligned                 ${alignment}\n"
		);
		break;
	}
	case PointerTestCases::OP_SELECT_VARIABLE_PTR:
	{
		main += std::string(
			"%input_loc_first  = OpUntypedAccessChainKHR %storage_buffer_untyped_ptr     %input_buffer_0    %input_data_0_untyped_var %c_uint32_0\n"
			"%input_loc_second = OpUntypedAccessChainKHR %storage_buffer_untyped_ptr     %input_buffer_1    %input_data_1_untyped_var %c_uint32_0\n"
			"%output_loc       = OpAccessChain           %${baseType}_storage_buffer_ptr                    %output_data_var          %c_uint32_0\n"

			"%selected_ptr     = OpSelect %storage_buffer_untyped_ptr ${condition} %input_loc_first %input_loc_second\n"

			"%selected_ptr_loc = OpLoad  %${baseType} %selected_ptr\n"
			"                    OpStore %output_loc  %selected_ptr_loc\n"
		);
		break;
	}
	case PointerTestCases::OP_PHI_VARIABLE_PTR:
	{
		main += std::string(
			"%input_loc_first  = OpUntypedAccessChainKHR %storage_buffer_untyped_ptr     %input_buffer_0    %input_data_0_untyped_var %c_uint32_0\n"
			"%input_loc_second = OpUntypedAccessChainKHR %storage_buffer_untyped_ptr     %input_buffer_1    %input_data_1_untyped_var %c_uint32_0\n"
			"%output_loc       = OpAccessChain           %${baseType}_storage_buffer_ptr                    %output_data_var          %c_uint32_0\n"

			"                    OpSelectionMerge        %end_label                      None\n"
			"                    OpBranchConditional     ${condition}                    %take_input_0      %take_input_1\n"
			"%take_input_0     = OpLabel\n"
			"                    OpBranch                %end_label\n"
			"%take_input_1     = OpLabel\n"
			"                    OpBranch                %end_label\n"
			"%end_label        = OpLabel\n"

			"%selected_ptr     = OpPhi                   %storage_buffer_untyped_ptr    %input_loc_first   %take_input_0              %input_loc_second   %take_input_1\n"
			"%selected_ptr_loc = OpLoad                  %${baseType}                   %selected_ptr\n"
			"                    OpStore                 %output_loc                    %selected_ptr_loc\n"
		);
		break;
	}
	case PointerTestCases::OP_PTR_EQUAL_VARIABLE_PTR:
	{
		main += std::string(
			"%input_loc_first  = OpUntypedAccessChainKHR %storage_buffer_untyped_ptr     %input_buffer_0    %input_data_0_untyped_var %c_uint32_0\n"
			"%input_loc_second = OpUntypedAccessChainKHR %storage_buffer_untyped_ptr     %input_buffer_1    %input_data_1_untyped_var %c_uint32_0\n"
			"%output_loc       = OpAccessChain           %${baseType}_storage_buffer_ptr                    %output_data_var          %c_uint32_0\n"

			"%are_equal        = OpPtrEqual              %bool                           %input_loc_first   %input_loc_second\n"
			"%selected_ptr     = OpSelect                %storage_buffer_untyped_ptr     %are_equal         %input_loc_first          %input_loc_second\n"
			"%selected_ptr_loc = OpLoad                  %${baseType}                    %selected_ptr\n"
			"                    OpStore                 %output_loc                     %selected_ptr_loc\n"
		);
		break;
	}
	case PointerTestCases::OP_PTR_NOT_EQUAL_VARIABLE_PTR:
	{
		main += std::string(
			"%input_loc_first  = OpUntypedAccessChainKHR %storage_buffer_untyped_ptr     %input_buffer_0    %input_data_0_untyped_var %c_uint32_0\n"
			"%input_loc_second = OpUntypedAccessChainKHR %storage_buffer_untyped_ptr     %input_buffer_1    %input_data_1_untyped_var %c_uint32_0\n"
			"%output_loc       = OpAccessChain           %${baseType}_storage_buffer_ptr                    %output_data_var          %c_uint32_0\n"

			"%are_not_equal    = OpPtrNotEqual           %bool                           %input_loc_first   %input_loc_second\n"
			"%selected_ptr     = OpSelect                %storage_buffer_untyped_ptr     %are_not_equal     %input_loc_first          %input_loc_second\n"
			"%selected_ptr_loc = OpLoad                  %${baseType}                    %selected_ptr\n"
			"                    OpStore                 %output_loc                     %selected_ptr_loc\n"
		);
		break;
	}
	case PointerTestCases::OP_PTR_DIFF_VARIABLE_PTR:
	{
		main += std::string(
			"%input_loc            = OpUntypedAccessChainKHR    %storage_buffer_untyped_ptr     %input_buffer         %input_data_untyped_var %c_uint32_0 %c_uint32_0\n"
			"%input_loc_first_ptr  = OpUntypedPtrAccessChainKHR %storage_buffer_untyped_ptr     %input_buffer         %input_loc              %c_uint32_0\n"
			"%input_loc_second_ptr = OpUntypedPtrAccessChainKHR %storage_buffer_untyped_ptr     %input_buffer         %input_loc              %c_uint32_16\n"
			"%output_loc           = OpAccessChain              %uint32_storage_buffer_ptr                            %output_data_var        %c_uint32_0\n"

			"%ptr_diff_value       = OpPtrDiff                  %uint32                         %input_loc_second_ptr %input_loc_first_ptr\n"
			"                        OpStore                    %output_loc                     %ptr_diff_value\n"
		);
		break;
	}
	case PointerTestCases::OP_PTR_ACCESS_CHAIN_VARIABLE_PTR:
	{
		main += std::string(
			"%id_loc          = OpAccessChain %uint32_input_ptr %id %c_uint32_0\n"
			"%x               = OpLoad %uint32 %id_loc\n"

			"%input_loc       = OpUntypedAccessChainKHR    %storage_buffer_untyped_ptr     %input_buffer      %input_data_untyped_var   %c_uint32_0 %c_uint32_0\n"
			"%input_loc_ptr   = OpUntypedPtrAccessChainKHR %storage_buffer_untyped_ptr     %input_buffer      %input_loc                %x\n"
			"%output_loc      = OpAccessChain              %${baseType}_storage_buffer_ptr                    %output_data_var          %c_uint32_0 %x\n"

			"%input_ptr_loc   = OpLoad                     %${baseType}                    %input_loc_ptr\n"
			"                   OpStore                    %output_loc                     %input_ptr_loc\n"
		);
		break;
	}
	case PointerTestCases::OP_FUNCTION_CALL_VARIABLE_PTR:
	{
		main += std::string(
			"%input_loc        = OpUntypedAccessChainKHR %storage_buffer_untyped_ptr     %input_buffer      %input_data_untyped_var   %c_uint32_0\n"
			"%output_loc       = OpAccessChain           %${baseType}_storage_buffer_ptr                    %output_data_var          %c_uint32_0\n"

			"%returned_ptr     = OpFunctionCall          %storage_buffer_untyped_ptr     %simple_function   %input_loc\n"

			"%returned_ptr_loc = OpLoad                  %${baseType}                    %returned_ptr\n"
			"                    OpStore                 %output_loc                     %returned_ptr_loc\n"
		);
		break;
	}
	case PointerTestCases::MULTIPLE_ACCESS_CHAINS_VARIABLE_PTR:
	{
		main += std::string(
			"%input_data_var_loc  = OpUntypedAccessChainKHR %storage_buffer_untyped_ptr         %input_buffer        %input_data_untyped_var\n"
			"%data_var_loc        = OpUntypedAccessChainKHR %storage_buffer_untyped_ptr         %output_buffer       %input_data_var_loc\n"
			"%loaded_data         = OpLoad                  %output_buffer                      %data_var_loc\n"
			"%output_data_var_loc = OpAccessChain           %output_buffer_storage_buffer_ptr   %output_data_var\n"
			"                       OpStore                 %output_data_var_loc                %loaded_data\n"
		);
		break;
	}
	case PointerTestCases::FUNCTION_VARIABLE_VARIABLE_PTR:
	{
		main += std::string(
			"%output_copy_function_var = OpVariable                 %storage_buffer_untyped_ptr_function_ptr Function\n"

			"%input_loc_first          = OpUntypedAccessChainKHR    %storage_buffer_untyped_ptr              %input_buffer_0           %input_data_0_untyped_var %c_uint32_0\n"
			"%input_loc_second         = OpUntypedAccessChainKHR    %storage_buffer_untyped_ptr              %input_buffer_1           %input_data_1_untyped_var %c_uint32_0\n"
			"%output_loc               = OpAccessChain              %${baseType}_storage_buffer_ptr                                    %output_data_var          %c_uint32_0\n"

			"%selected_ptr             = OpSelect                   %storage_buffer_untyped_ptr              ${condition}              %input_loc_first          %input_loc_second\n"

			"                            OpStore                    %output_copy_function_var                %selected_ptr\n"
			"%output_copy_loc_unty_ptr = OpLoad                     %storage_buffer_untyped_ptr              %output_copy_function_var\n"
			"%output_copy_loc_ptr      = OpLoad                     %${baseType}_storage_buffer_ptr          %output_copy_loc_unty_ptr\n"
			"%output_copy_loc          = OpLoad                     %${baseType}                             %output_copy_loc_ptr\n"
			"                            OpStore                    %output_loc                              %output_copy_loc\n"
		);
		break;
	}
	case PointerTestCases::PRIVATE_VARIABLE_VARIABLE_PTR:
	{
		main += std::string(
			"%input_loc_first          = OpUntypedAccessChainKHR    %storage_buffer_untyped_ptr              %input_buffer_0           %input_data_0_untyped_var %c_uint32_0\n"
			"%input_loc_second         = OpUntypedAccessChainKHR    %storage_buffer_untyped_ptr              %input_buffer_1           %input_data_1_untyped_var %c_uint32_0\n"
			"%output_loc               = OpAccessChain              %${baseType}_storage_buffer_ptr                                    %output_data_var          %c_uint32_0\n"

			"%selected_ptr             = OpSelect                   %storage_buffer_untyped_ptr              ${condition}              %input_loc_first          %input_loc_second\n"

			"                            OpStore                    %output_copy_private_var                 %selected_ptr\n"
			"%output_copy_loc_unty_ptr = OpLoad                     %storage_buffer_untyped_ptr              %output_copy_private_var\n"
			"%output_copy_loc_ptr      = OpLoad                     %${baseType}_storage_buffer_ptr          %output_copy_loc_unty_ptr\n"
			"%output_copy_loc          = OpLoad                     %${baseType}                             %output_copy_loc_ptr\n"
			"                            OpStore                    %output_loc                              %output_copy_loc\n"
		);
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
		"                OpReturn\n"
		"                OpFunctionEnd\n"
	);

	return main;
}

std::string createShaderMain(WORKGROUP_TEST_CASE testCase)
{
	std::string main = std::string(
		"%main               = OpFunction %void None %void_func\n"
		"%label_main         = OpLabel\n"
	);

	switch (testCase)
	{
	case WorkgroupTestCases::NOT_ALIASED:
	case WorkgroupTestCases::ALIASED:
	{
		main += std::string(
			/* Writting to shared memory */
			"%input_data_scalar_loc    = OpAccessChain           %${baseType}_storage_buffer_ptr      %input_data_var                             %c_uint32_1\n"
			"%input_data_vector_loc    = OpAccessChain           %vec4_${baseType}_storage_buffer_ptr %input_data_var                             %c_uint32_0\n"
			"%data_buffer_0_scalar_loc = OpUntypedAccessChainKHR %workgroup_untyped_ptr               %data_buffer     %data_buffer_0_untyped_var %c_uint32_1\n"
			"%data_buffer_0_vector_loc = OpUntypedAccessChainKHR %workgroup_untyped_ptr               %data_buffer     %data_buffer_0_untyped_var %c_uint32_0\n"
			"%output_data_scalar_loc   = OpAccessChain           %${baseType}_storage_buffer_ptr      %output_data_var                            %c_uint32_1\n"
			"%output_data_vector_loc   = OpAccessChain           %vec4_${baseType}_storage_buffer_ptr %output_data_var                            %c_uint32_0\n"

			"%input_data_scalar        = OpLoad                  %${baseType}                         %input_data_scalar_loc\n"
			"                            OpStore                 %data_buffer_0_scalar_loc            %input_data_scalar\n"
			"%input_data_vector        = OpLoad                  %vec4_${baseType}                    %input_data_vector_loc\n"
			"                            OpStore                 %data_buffer_0_vector_loc            %input_data_vector\n"

			/* Barriers */
			"                            OpMemoryBarrier         %c_uint32_1                          %c_uint32_264\n"
			"                            OpControlBarrier        %c_uint32_2                          %c_uint32_2                                 %c_uint32_264\n"

			/* Reading from shared memory */
			"%data_buffer_0_scalar     = OpLoad                  %${baseType}                         %data_buffer_0_scalar_loc\n"
			"                            OpStore                 %output_data_scalar_loc              %data_buffer_0_scalar\n"
			"%data_buffer_0_vector     = OpLoad                  %vec4_${baseType}                    %data_buffer_0_vector_loc\n"
			"                            OpStore                 %output_data_vector_loc              %data_buffer_0_vector\n"
		);
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
		"                OpReturn\n"
		"                OpFunctionEnd\n"
	);

	return main;
}

std::string createShaderMain(COOPERATIVE_MATRIX_TEST_CASE testCase)
{
	std::string main = std::string(
		"%main               = OpFunction %void None %void_func\n"
		"%label_main         = OpLabel\n"
	);

	switch (testCase)
	{
	case CooperativeMatrixTestCases::BASIC_LOAD:
	{
		main += std::string(
			"%output_loc    = OpAccessChain               %${baseType}_storage_buffer_ptr       %output_data_var                %c_uint32_0      %c_uint32_0\n"

			"%loaded_matrix = OpCooperativeMatrixLoadKHR  %${baseType}_matrix_1x1               %input_data_untyped_var         %c_matrix_layout\n"
			"                 OpCooperativeMatrixStoreKHR %output_loc                           %loaded_matrix                  %c_matrix_layout\n"
		);
		break;
	}
	case CooperativeMatrixTestCases::BASIC_STORE:
	{
		main += std::string(
			"%input_loc     = OpAccessChain               %${baseType}_storage_buffer_ptr       %input_data_var                 %c_uint32_0      %c_uint32_0\n"

			"%loaded_matrix = OpCooperativeMatrixLoadKHR  %${baseType}_matrix_1x1               %input_loc                      %c_matrix_layout\n"
			"                 OpCooperativeMatrixStoreKHR %output_data_untyped_var              %loaded_matrix                  %c_matrix_layout\n"
		);
		break;
	}
	case CooperativeMatrixTestCases::TYPE_PUNNING_LOAD:
	{
		main += std::string(
			"%output_loc    = OpAccessChain               %${sameSizeType}_storage_buffer_ptr   %output_data_var                %c_uint32_0      %c_uint32_0\n"

			"%loaded_matrix = OpCooperativeMatrixLoadKHR  %${sameSizeType}_matrix_1x1           %input_data_untyped_var         %c_matrix_layout\n"
			"                 OpCooperativeMatrixStoreKHR %output_loc                           %loaded_matrix                  %c_matrix_layout\n"
		);
		break;
	}
	case CooperativeMatrixTestCases::TYPE_PUNNING_STORE:
	{
		main += std::string(
			"%input_loc     = OpAccessChain               %${baseType}_storage_buffer_ptr       %input_data_var                 %c_uint32_0      %c_uint32_0\n"

			"%loaded_matrix = OpCooperativeMatrixLoadKHR  %${baseType}_matrix_1x1               %input_loc                      %c_matrix_layout\n"
			"                 OpCooperativeMatrixStoreKHR %output_data_untyped_var              %loaded_matrix                  %c_matrix_layout\n"
		);
		break;
	}
	case CooperativeMatrixTestCases::MIXED_LOAD:
	{
		main += std::string(
			"%id_loc        = OpAccessChain               %uint32_input_ptr                     %id                             %c_uint32_0\n"
			"%x             = OpLoad                      %uint32                               %id_loc\n"

			"%input_loc     = OpUntypedAccessChainKHR     %storage_buffer_untyped_ptr           %input_buffer                   %input_data_untyped_var %c_uint32_0 %x\n"
			"%output_loc    = OpAccessChain               %${baseType}_storage_buffer_ptr       %output_data_var                %c_uint32_0             %x\n"

			"%loaded_matrix = OpCooperativeMatrixLoadKHR  %${baseType}_matrix_2x2               %input_loc                      %c_matrix_layout        %c_matrix_stride None\n"
			"                 OpCooperativeMatrixStoreKHR %output_loc                           %loaded_matrix                  %c_matrix_layout        %c_matrix_stride None\n"
		);
		break;
	}
	case CooperativeMatrixTestCases::MIXED_STORE:
	{
		main += std::string(
			"%id_loc        = OpAccessChain               %uint32_input_ptr                     %id                             %c_uint32_0\n"
			"%x             = OpLoad                      %uint32                               %id_loc\n"

			"%input_loc     = OpAccessChain               %${baseType}_storage_buffer_ptr       %input_data_var                 %c_uint32_0              %x\n"
			"%output_loc    = OpUntypedAccessChainKHR     %storage_buffer_untyped_ptr           %output_buffer                  %output_data_untyped_var %c_uint32_0 %x\n"

			"%loaded_matrix = OpCooperativeMatrixLoadKHR  %${baseType}_matrix_2x2               %input_loc                      %c_matrix_layout         %c_matrix_stride None\n"
			"                 OpCooperativeMatrixStoreKHR %output_loc                           %loaded_matrix                  %c_matrix_layout         %c_matrix_stride None\n"
		);
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
		"                OpReturn\n"
		"                OpFunctionEnd\n"
	);

	return main;
}

void addOpArrayLengthTests(tcu::TestCaseGroup* testGroup, MEMORY_MODEL_TYPE memModel)
{
	tcu::TestContext& testCtx = testGroup->getTestContext();

	const tcu::StringTemplate shaderHeader(
		createShaderHeader()
	);

	const tcu::StringTemplate shaderAnnotations(
		createShaderAnnotations(BaseTestCases::ARRAY_LENGTH)
	);

	const tcu::StringTemplate shaderVariables(
		createShaderVariables(BaseTestCases::ARRAY_LENGTH)
	);

	const tcu::StringTemplate shaderFunctions(
		createShaderMain(BaseTestCases::ARRAY_LENGTH)
	);

	for (deUint32 i = 0; i < DE_LENGTH_OF_ARRAY(BASE_DATA_TYPE_CASES); ++i)
	{
		std::string testName	= toString(BASE_DATA_TYPE_CASES[i]);

		const deUint32 numWorkgroup	= 16u;

		std::map<std::string, std::string>	specMap;
		specMap["alignment"]	= std::to_string(getSizeInBytes(BASE_DATA_TYPE_CASES[i]));
		specMap["baseType"]		= toString(BASE_DATA_TYPE_CASES[i]);
		specMap["baseDecl"]		= getDeclaration(BASE_DATA_TYPE_CASES[i]);

		std::string shaderVariablesStr = shaderVariables.specialize(specMap);
		if (BASE_DATA_TYPE_CASES[i] != DataTypes::UINT32)
		{
			shaderVariablesStr	= "%uint32                    = OpTypeInt     32            0\n"
								  "%c_uint32_1                = OpConstant    %uint32       1\n"
								  "%uint32_storage_buffer_ptr = OpTypePointer StorageBuffer %uint32\n"
								+ shaderVariablesStr;
		}

		ComputeShaderSpec spec;
		adjustSpecForUntypedPointers(spec, specMap);
		adjustSpecForMemoryModel(spec, specMap, memModel);
		adjustSpecForDataTypes(spec, specMap, BASE_DATA_TYPE_CASES[i]);

		const std::string shaderAsm =
			shaderHeader.specialize(specMap) +
			shaderAnnotations.specialize(specMap) +
			shaderVariablesStr +
			shaderFunctions.specialize(specMap);

		FilledResourceDesc desc;
		desc.dataType		= BASE_DATA_TYPE_CASES[i];
		desc.value			= 1;
		desc.elemCount		= numWorkgroup;
		desc.fillType		= FillingTypes::VALUE;
		desc.descriptorType	= vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		desc.padding		= 0;

		Resource input		= createFilledResource(desc);
		desc.value			= numWorkgroup;
		desc.elemCount		= 1;
		desc.dataType		= DataTypes::UINT32;
		Resource output		= createFilledResource(desc);

		spec.assembly		= shaderAsm;
		spec.numWorkGroups	= tcu::IVec3(numWorkgroup, 1, 1);
		spec.inputs.push_back(input);
		spec.outputs.push_back(output);
		spec.extensions.push_back("VK_KHR_storage_buffer_storage_class");
		spec.extensions.push_back("VK_KHR_shader_untyped_pointers");

		testGroup->addChild(new SpvAsmComputeShaderCase(testCtx, testName.c_str(), spec));
	}
}

void addLoadTests(tcu::TestCaseGroup* testGroup, MEMORY_MODEL_TYPE memModel)
{
	tcu::TestContext& testCtx = testGroup->getTestContext();

	de::MovePtr<tcu::TestCaseGroup>	uniformGroup(new tcu::TestCaseGroup(testCtx, "uniform", ""));
	de::MovePtr<tcu::TestCaseGroup>	storageGroup(new tcu::TestCaseGroup(testCtx, "storage", ""));
	de::MovePtr<tcu::TestCaseGroup>	pushConstantGroup(new tcu::TestCaseGroup(testCtx, "push_constant", ""));

	const tcu::StringTemplate shaderHeader(
		createShaderHeader()
	);

	const tcu::StringTemplate shaderAnnotations(
		createShaderAnnotations(BaseTestCases::LOAD)
	);

	const tcu::StringTemplate shaderVariables(
		createShaderVariables(BaseTestCases::LOAD)
	);

	const tcu::StringTemplate shaderFunctions(
		createShaderMain(BaseTestCases::LOAD)
	);

	for (deUint32 i = 0; i < DE_LENGTH_OF_ARRAY(BASE_DATA_TYPE_CASES); ++i)
	{
		for (deUint32 j = 0; j < DE_LENGTH_OF_ARRAY(LOAD_CONTAINER_TYPE_CASES); ++j)
		{
			std::string testName	= toString(BASE_DATA_TYPE_CASES[i]);

			const deUint32 numWorkgroup	= LOAD_CONTAINER_TYPE_CASES[j] == ContainerTypes::PUSH_CONSTANT ? 128 / 8 : Constants::numThreads;

			std::map<std::string, std::string>	specMap;
			if (LOAD_CONTAINER_TYPE_CASES[j] == ContainerTypes::UNIFORM)
			{
				specMap["alignment"]	= std::to_string(Constants::uniformAlignment);
			}
			else
			{
				specMap["alignment"]	= std::to_string(getSizeInBytes(BASE_DATA_TYPE_CASES[i]));
			}
			specMap["threadCount"]			= std::to_string(numWorkgroup);
			specMap["args"]					= LOAD_OPERATION_CASES[0].pArgs;
			specMap["baseType"]				= toString(BASE_DATA_TYPE_CASES[i]);
			specMap["loadOp"]				= LOAD_OPERATION_CASES[0].pOperation;
			specMap["baseDecl"]				= getDeclaration(BASE_DATA_TYPE_CASES[i]);
			specMap["storageClass"]			= getStorageClass(LOAD_CONTAINER_TYPE_CASES[j]);
			specMap["storageDecorations"]	= getResourceDecorations(LOAD_CONTAINER_TYPE_CASES[j], BASE_DATA_TYPE_CASES[i], numWorkgroup);

			std::string shaderVariablesStr = shaderVariables.specialize(specMap);
			if (BASE_DATA_TYPE_CASES[i] != DataTypes::UINT32)
			{
				shaderVariablesStr	= "%uint32     = OpTypeInt  32      0\n"
									  "%c_uint32_1 = OpConstant %uint32 1\n"
									+ shaderVariablesStr;
			}

			ComputeShaderSpec spec;
			adjustSpecForUntypedPointers(spec, specMap);
			adjustSpecForMemoryModel(spec, specMap, memModel);
			adjustSpecForDataTypes(spec, specMap, BASE_DATA_TYPE_CASES[i]);

			const std::string shaderAsm =
				shaderHeader.specialize(specMap) +
				shaderAnnotations.specialize(specMap) +
				shaderVariablesStr +
				shaderFunctions.specialize(specMap);

			FilledResourceDesc desc;
			desc.dataType	= BASE_DATA_TYPE_CASES[i];
			desc.elemCount	= numWorkgroup;
			desc.fillType	= FillingTypes::RANDOM;
			desc.seed		= deStringHash(testGroup->getName());
			if (LOAD_CONTAINER_TYPE_CASES[j] == ContainerTypes::UNIFORM)
			{
				desc.descriptorType	= vk::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
				desc.padding		= Constants::uniformAlignment - getSizeInBytes(BASE_DATA_TYPE_CASES[i]);
			}
			else
			{
				desc.descriptorType	= vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
				desc.padding		= 0;
			}

			Resource inputResource	= createFilledResource(desc);
			desc.descriptorType		= vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			Resource outputResource	= createFilledResource(desc);

			if (LOAD_CONTAINER_TYPE_CASES[j] == ContainerTypes::PUSH_CONSTANT)
			{
				spec.pushConstants	= inputResource.getBuffer();
				spec.outputs.push_back(outputResource);
			}
			else
			{
				spec.inputs.push_back(inputResource);
				spec.outputs.push_back(outputResource);
			}
			spec.assembly		= shaderAsm;
			spec.numWorkGroups	= tcu::IVec3(numWorkgroup, 1, 1);
			spec.extensions.push_back("VK_KHR_storage_buffer_storage_class");
			spec.extensions.push_back("VK_KHR_shader_untyped_pointers");

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

void addLoadAtomicTests(tcu::TestCaseGroup* testGroup, MEMORY_MODEL_TYPE memModel)
{
	tcu::TestContext& testCtx = testGroup->getTestContext();

	const tcu::StringTemplate shaderHeader(
		createShaderHeader()
	);

	const tcu::StringTemplate shaderAnnotations(
		createShaderAnnotations(AtomicTestCases::OP_ATOMIC_LOAD)
	);

	const tcu::StringTemplate shaderVariables(
		createShaderVariables(AtomicTestCases::OP_ATOMIC_LOAD)
	);

	const tcu::StringTemplate shaderFunctions(
		createShaderMain(AtomicTestCases::OP_ATOMIC_LOAD)
	);

	for (deUint32 i = 0; i < DE_LENGTH_OF_ARRAY(BASE_DATA_TYPE_CASES); ++i)
	{
		std::string testName	= toString(BASE_DATA_TYPE_CASES[i]);

		const deUint32 numWorkgroup	= Constants::numThreads;

		std::map<std::string, std::string>	specMap;

		specMap["alignment"]	= std::to_string(getSizeInBytes(BASE_DATA_TYPE_CASES[i]));
		specMap["threadCount"]	= std::to_string(numWorkgroup);
		specMap["args"]			= LOAD_OPERATION_CASES[1].pArgs;
		specMap["baseType"]		= toString(BASE_DATA_TYPE_CASES[i]);
		specMap["loadOp"]		= LOAD_OPERATION_CASES[1].pOperation;
		specMap["baseDecl"]		= getDeclaration(BASE_DATA_TYPE_CASES[i]);

		std::string shaderVariablesStr = shaderVariables.specialize(specMap);
		if (BASE_DATA_TYPE_CASES[i] != DataTypes::UINT32)
		{
			shaderVariablesStr	= "%uint32     = OpTypeInt  32      0\n"
								+ shaderVariablesStr;
		}

		ComputeShaderSpec spec;
		adjustSpecForUntypedPointers(spec, specMap);
		adjustSpecForMemoryModel(spec, specMap, memModel);
		adjustSpecForDataTypes(spec, specMap, BASE_DATA_TYPE_CASES[i]);
		adjustSpecForAtomicOperations(spec, specMap, BASE_DATA_TYPE_CASES[i]);

		const std::string shaderAsm =
			shaderHeader.specialize(specMap) +
			shaderAnnotations.specialize(specMap) +
			shaderVariablesStr +
			shaderFunctions.specialize(specMap);

		FilledResourceDesc desc;
		desc.dataType		= BASE_DATA_TYPE_CASES[i];
		desc.elemCount		= numWorkgroup;
		desc.fillType		= FillingTypes::RANDOM;
		desc.seed			= deStringHash(testGroup->getName());
		desc.descriptorType	= vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		desc.padding		= 0;

		Resource inputResource	= createFilledResource(desc);
		desc.descriptorType		= vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		Resource outputResource	= createFilledResource(desc);

		spec.inputs.push_back(inputResource);
		spec.outputs.push_back(outputResource);
		spec.assembly		= shaderAsm;
		spec.numWorkGroups	= tcu::IVec3(numWorkgroup, 1, 1);
		spec.extensions.push_back("VK_KHR_storage_buffer_storage_class");
		spec.extensions.push_back("VK_KHR_shader_untyped_pointers");

		testGroup->addChild(new SpvAsmComputeShaderCase(testCtx, testName.c_str(), spec));
	}
}

void addLoadMixedTypeTests(tcu::TestCaseGroup* testGroup, MEMORY_MODEL_TYPE memModel)
{
	tcu::TestContext& testCtx = testGroup->getTestContext();

	de::MovePtr<tcu::TestCaseGroup>	uniformGroup(new tcu::TestCaseGroup(testCtx, "uniform", ""));
	de::MovePtr<tcu::TestCaseGroup>	storageGroup(new tcu::TestCaseGroup(testCtx, "storage", ""));
	de::MovePtr<tcu::TestCaseGroup>	pushConstantGroup(new tcu::TestCaseGroup(testCtx, "push_constant", ""));

	{
		const tcu::StringTemplate shaderHeader(
			createShaderHeader()
		);

		const tcu::StringTemplate shaderAnnotations(
			createShaderAnnotations(TypePunningTestCases::LOAD_SAME_SIZE_TYPES)
		);

		const tcu::StringTemplate shaderVariables(
			createShaderVariables(TypePunningTestCases::LOAD_SAME_SIZE_TYPES)
		);

		const tcu::StringTemplate shaderFunctions(
			createShaderMain(TypePunningTestCases::LOAD_SAME_SIZE_TYPES)
		);

		for (deUint32 i = 0; i < DE_LENGTH_OF_ARRAY(BASE_DATA_TYPE_CASES); ++i)
		{
			for (deUint32 j = 0; j < DE_LENGTH_OF_ARRAY(LOAD_CONTAINER_TYPE_CASES); ++j)
			{
				std::vector<DATA_TYPE> sameSizeTypes = getSameSizeBaseDataType(BASE_DATA_TYPE_CASES[i]);

				for (deUint32 l = 0; l < sameSizeTypes.size(); ++l)
				{
					const DATA_TYPE dataType	= sameSizeTypes[l];

					std::string testName	= toString(BASE_DATA_TYPE_CASES[i]) + std::string("_to_") + toString(dataType);

					const deUint32						numWorkgroup	= LOAD_CONTAINER_TYPE_CASES[j] == ContainerTypes::PUSH_CONSTANT ? 128 / 8 : Constants::numThreads;
					const deUint32						caseIndex		= static_cast<deUint32>(dataType);
					std::map<std::string, std::string>	specMap;
					if (LOAD_CONTAINER_TYPE_CASES[j] == ContainerTypes::UNIFORM)
					{
						specMap["alignment"]	= std::to_string(Constants::uniformAlignment);
					}
					else
					{
						specMap["alignment"]	= std::to_string(getSizeInBytes(BASE_DATA_TYPE_CASES[i]));
					}
					specMap["threadCount"]			= std::to_string(numWorkgroup);
					specMap["args"]					= LOAD_OPERATION_CASES[0].pArgs;
					specMap["baseType"]				= toString(BASE_DATA_TYPE_CASES[i]);
					specMap["baseDecl"]				= getDeclaration(BASE_DATA_TYPE_CASES[i]);
					specMap["sameSizeType"]			= toString(BASE_DATA_TYPE_CASES[caseIndex]);
					specMap["sameSizeDecl"]			= getDeclaration(BASE_DATA_TYPE_CASES[caseIndex]);
					specMap["loadOp"]				= LOAD_OPERATION_CASES[0].pOperation;
					specMap["storageClass"]			= getStorageClass(LOAD_CONTAINER_TYPE_CASES[j]);
					specMap["otherCap"]				= getCapability(dataType);
					specMap["storageDecorations"]	= getSameSizeResourceDecorations(LOAD_CONTAINER_TYPE_CASES[j], BASE_DATA_TYPE_CASES[i], BASE_DATA_TYPE_CASES[caseIndex], numWorkgroup);


					std::string shaderVariablesStr	= shaderVariables.specialize(specMap);
					if ((BASE_DATA_TYPE_CASES[i] != DataTypes::UINT32) && (dataType != DataTypes::UINT32))
					{
						shaderVariablesStr	= "%uint32     = OpTypeInt 32 0\n"
												"%c_uint32_1 = OpConstant %uint32 1\n"
											+ shaderVariablesStr;
					}

					ComputeShaderSpec spec;
					adjustSpecForUntypedPointers(spec, specMap);
					adjustSpecForMemoryModel(spec, specMap, memModel);
					adjustSpecForDataTypes(spec, specMap, BASE_DATA_TYPE_CASES[i]);

					const std::string shaderAsm =
						shaderHeader.specialize(specMap) +
						shaderAnnotations.specialize(specMap) +
						shaderVariablesStr +
						shaderFunctions.specialize(specMap);

					FilledResourceDesc desc;
					desc.dataType	= sameSizeTypes[l];
					desc.elemCount	= Constants::numThreads;
					desc.fillType	= FillingTypes::RANDOM;
					desc.seed		= deStringHash(testGroup->getName());
					if (LOAD_CONTAINER_TYPE_CASES[j] == ContainerTypes::UNIFORM)
					{
						desc.descriptorType	= vk::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
						desc.padding		= Constants::uniformAlignment - getSizeInBytes(BASE_DATA_TYPE_CASES[i]);
					}
					else
					{
						desc.descriptorType	= vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
						desc.padding		= 0;
					}

					Resource inputResource	= createFilledResource(desc);
					desc.descriptorType		= vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
					Resource outputResource	= createFilledResource(desc);

					if (LOAD_CONTAINER_TYPE_CASES[j] == ContainerTypes::PUSH_CONSTANT)
					{
						spec.pushConstants	= inputResource.getBuffer();
						spec.outputs.push_back(outputResource);
					}
					else
					{
						spec.inputs.push_back(inputResource);
						spec.outputs.push_back(outputResource);
					}
					spec.assembly		= shaderAsm;
					spec.numWorkGroups	= tcu::IVec3(Constants::numThreads, 1, 1);
					spec.extensions.push_back("VK_KHR_storage_buffer_storage_class");
					spec.extensions.push_back("VK_KHR_shader_untyped_pointers");

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
		const tcu::StringTemplate shaderHeader(
			createShaderHeader()
		);

		const tcu::StringTemplate shaderAnnotations(
			createShaderAnnotations(TypePunningTestCases::LOAD_SCALAR_VECTOR)
		);

		const tcu::StringTemplate shaderVariables(
			createShaderVariables(TypePunningTestCases::LOAD_SCALAR_VECTOR)
		);

		const tcu::StringTemplate shaderFunctions(
			createShaderMain(TypePunningTestCases::LOAD_SCALAR_VECTOR)
		);

		for (deUint32 i = 0; i < DE_LENGTH_OF_ARRAY(BASE_DATA_TYPE_CASES); ++i)
		{
			for (deUint32 j = 0; j < DE_LENGTH_OF_ARRAY(LOAD_CONTAINER_TYPE_CASES); ++j)
			{
				std::vector<COMPOSITE_DATA_TYPE> sameSizeTypes = getSameSizeCompositeType(BASE_DATA_TYPE_CASES[i]);

				for (deUint32 l = 0; l < sameSizeTypes.size(); ++l)
				{
					COMPOSITE_DATA_TYPE compositeType	= sameSizeTypes[l];
					const deUint32 otherIndex			= static_cast<deUint32>(getCompositeBaseDataType(compositeType));

					std::string testName	= toString(BASE_DATA_TYPE_CASES[i]) + std::string("_to_") + toString(compositeType);

					std::map<std::string, std::string>	specMap;
					if (LOAD_CONTAINER_TYPE_CASES[j] == ContainerTypes::UNIFORM)
					{
						specMap["alignment"]	= std::to_string(Constants::uniformAlignment);
					}
					else
					{
						specMap["alignment"]	= std::to_string(getSizeInBytes(BASE_DATA_TYPE_CASES[i]));
					}

					specMap["args"]					= LOAD_OPERATION_CASES[0].pArgs;
					specMap["loadOp"]				= LOAD_OPERATION_CASES[0].pOperation;
					specMap["storageClass"]			= getStorageClass(LOAD_CONTAINER_TYPE_CASES[j]);
					specMap["baseType"]				= toString(BASE_DATA_TYPE_CASES[i]);
					specMap["otherType"]			= toString(getCompositeBaseDataType(compositeType));
					specMap["baseDecl"]				= getDeclaration(BASE_DATA_TYPE_CASES[i]);
					specMap["otherTypeDecl"]		= getDeclaration(BASE_DATA_TYPE_CASES[otherIndex]);
					specMap["otherVec"]				= toString(compositeType);
					specMap["otherVecDecl"]			= getDeclaration(compositeType);
					specMap["otherCap"]				= getCapability(compositeType);
					specMap["storageDecorations"]	= getScalarVectorResourceDecorations(LOAD_CONTAINER_TYPE_CASES[j]);

					if (sameSizeTypes[l] != CompositeDataTypes::VEC3_UINT32)
						specMap["inputVec"]		= "%vec3_uint32 = OpTypeVector %uint32 3";

					std::string shaderVariablesStr = shaderVariables.specialize(specMap);
					if ((BASE_DATA_TYPE_CASES[i] != DataTypes::UINT32) &&
						(getCompositeBaseDataType(compositeType) != DataTypes::UINT32) &&
						compositeType != CompositeDataTypes::VEC2_UINT32 &&
						compositeType != CompositeDataTypes::VEC3_UINT32 &&
						compositeType != CompositeDataTypes::VEC4_UINT32)
					{
						shaderVariablesStr	= "%uint32      = OpTypeInt 32 0\n"
											  "%c_uint32_1  = OpConstant %uint32 1\n"
											+ shaderVariablesStr;
					}

					ComputeShaderSpec spec;
					adjustSpecForUntypedPointers(spec, specMap);
					adjustSpecForMemoryModel(spec, specMap, memModel);
					adjustSpecForDataTypes(spec, specMap, BASE_DATA_TYPE_CASES[i]);

					const std::string shaderAsm =
						shaderHeader.specialize(specMap) +
						shaderAnnotations.specialize(specMap) +
						shaderVariablesStr +
						shaderFunctions.specialize(specMap);

					FilledResourceDesc desc;
					desc.dataType	= BASE_DATA_TYPE_CASES[i];
					desc.elemCount	= 1;
					desc.fillType	= FillingTypes::VALUE;
					desc.value		= 1;
					if (LOAD_CONTAINER_TYPE_CASES[j] == ContainerTypes::UNIFORM)
					{
						desc.descriptorType	= vk::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
						desc.padding		= Constants::uniformAlignment - getSizeInBytes(BASE_DATA_TYPE_CASES[i]);
					}
					else
					{
						desc.descriptorType	= vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
						desc.padding		= 0;
					}

					Resource inputResource	= createFilledResource(desc);

					std::vector<deUint8>	outputData;
					if (LOAD_CONTAINER_TYPE_CASES[j] == ContainerTypes::UNIFORM)
					{
						const deUint32 alignedSize	= std::max(getSizeInBytes(compositeType), Constants::uniformAlignment);
						outputData.assign(alignedSize, 0xff);
						const deUint32 dataSize	= getSizeInBytes(getCompositeBaseDataType(compositeType));
						const deUint32 elemCnt	= getElementCount(compositeType);
						for (deUint32 ndx = 0; ndx < elemCnt * dataSize - 1; ++ndx)
						{
							outputData[ndx]	= 0;
						}
						outputData[elemCnt * dataSize - 1]	= 1;
					}
					else
					{
						const deUint32 sizeInBytes	= getSizeInBytes(compositeType);
						outputData.resize(sizeInBytes);
						for (deUint32 ndx = 0; ndx < sizeInBytes - 1; ++ndx)
						{
							outputData[ndx]	= 0;
						}
						outputData[sizeInBytes - 1]	= 1;
					}
					VectorResourceDesc outputDesc;
					outputDesc.descriptorType	= vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
					outputDesc.data				= outputData;
					outputDesc.dataType			= getCompositeBaseDataType(compositeType);
					Resource outputResource		= createResourceFromVector(outputDesc);

					if (LOAD_CONTAINER_TYPE_CASES[j] == ContainerTypes::PUSH_CONSTANT)
					{
						spec.pushConstants	= inputResource.getBuffer();
					}
					else
					{
						spec.inputs.push_back(inputResource);
					}
					spec.assembly		= shaderAsm;
					spec.numWorkGroups	= tcu::IVec3(1, 1, 1);
					spec.outputs.push_back(outputResource);
					spec.extensions.push_back("VK_KHR_storage_buffer_storage_class");
					spec.extensions.push_back("VK_KHR_shader_untyped_pointers");

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
		const tcu::StringTemplate shaderHeader(
			createShaderHeader()
		);

		const tcu::StringTemplate shaderAnnotations(
			createShaderAnnotations(TypePunningTestCases::LOAD_VECTOR_SCALAR)
		);

		const tcu::StringTemplate shaderVariables(
			createShaderVariables(TypePunningTestCases::LOAD_VECTOR_SCALAR)
		);

		const tcu::StringTemplate shaderFunctions(
			createShaderMain(TypePunningTestCases::LOAD_VECTOR_SCALAR)
		);

		for (deUint32 i = 0; i < DE_LENGTH_OF_ARRAY(COMPOSITE_DATA_TYPE_CASES); ++i)
		{
			for (deUint32 j = 0; j < DE_LENGTH_OF_ARRAY(LOAD_CONTAINER_TYPE_CASES); ++j)
			{
				std::vector<DATA_TYPE> sameSizeTypes = getSameSizeBaseDataType(COMPOSITE_DATA_TYPE_CASES[i]);

				for (deUint32 l = 0; l < sameSizeTypes.size(); ++l)
				{
					const DATA_TYPE	dataType	= sameSizeTypes[l];
					const deUint32	otherIndex	= static_cast<deUint32>(dataType);

					std::string testName	= toString(COMPOSITE_DATA_TYPE_CASES[i]) + std::string("_to_") + toString(dataType);

					std::map<std::string, std::string>	specMap;
					if (LOAD_CONTAINER_TYPE_CASES[j] == ContainerTypes::UNIFORM)
					{
						specMap["alignment"]	= std::to_string(Constants::uniformAlignment);
					}
					else
					{
						specMap["alignment"]	= std::to_string(getSizeInBytes(COMPOSITE_DATA_TYPE_CASES[i]));
					}

					specMap["args"]					= LOAD_OPERATION_CASES[0].pArgs;
					specMap["loadOp"]				= LOAD_OPERATION_CASES[0].pOperation;
					specMap["storageClass"]			= getStorageClass(LOAD_CONTAINER_TYPE_CASES[j]);
					specMap["baseType"]				= toString(getCompositeBaseDataType(COMPOSITE_DATA_TYPE_CASES[i]));
					specMap["otherType"]			= toString(dataType);
					specMap["baseDecl"]				= getDeclaration(getCompositeBaseDataType(COMPOSITE_DATA_TYPE_CASES[i]));
					specMap["otherTypeDecl"]		= getDeclaration(BASE_DATA_TYPE_CASES[otherIndex]);
					specMap["otherCap"]				= getCapability(dataType);
					specMap["baseVec"]				= toString(COMPOSITE_DATA_TYPE_CASES[i]);
					specMap["baseVecDecl"]			= getDeclaration(COMPOSITE_DATA_TYPE_CASES[i]);
					specMap["storageDecorations"]	= getScalarVectorResourceDecorations(LOAD_CONTAINER_TYPE_CASES[j]);

					if (COMPOSITE_DATA_TYPE_CASES[i] != CompositeDataTypes::VEC3_UINT32)
						specMap["inputVec"]		= "%vec3_uint32 = OpTypeVector %uint32 3";

					std::string shaderVariablesStr	= shaderVariables.specialize(specMap);
					if ((getCompositeBaseDataType(COMPOSITE_DATA_TYPE_CASES[i]) != DataTypes::UINT32) &&
						(dataType != DataTypes::UINT32))
					{
						shaderVariablesStr	= "%uint32      = OpTypeInt 32 0\n"
											  "%c_uint32_1  = OpConstant %uint32 1\n"
											+ shaderVariablesStr;
					}

					ComputeShaderSpec spec;
					adjustSpecForUntypedPointers(spec, specMap);
					adjustSpecForMemoryModel(spec, specMap, memModel);
					adjustSpecForDataTypes(spec, specMap, getCompositeBaseDataType(COMPOSITE_DATA_TYPE_CASES[i]));

					const std::string shaderAsm =
						shaderHeader.specialize(specMap) +
						shaderAnnotations.specialize(specMap) +
						shaderVariablesStr +
						shaderFunctions.specialize(specMap);

					std::vector<deUint8>	inputData;
					VectorResourceDesc inputDesc;
					if (LOAD_CONTAINER_TYPE_CASES[j] == ContainerTypes::UNIFORM)
					{
						const deUint32 alignedSize	= std::max(getSizeInBytes(COMPOSITE_DATA_TYPE_CASES[i]), Constants::uniformAlignment);
						inputData.assign(alignedSize, 0xff);
						const deUint32 dataSize	= getSizeInBytes(getCompositeBaseDataType(COMPOSITE_DATA_TYPE_CASES[i]));
						const deUint32 elemCnt	= getElementCount(COMPOSITE_DATA_TYPE_CASES[i]);
						for (deUint32 ndx = 0; ndx < elemCnt * dataSize - 1; ++ndx)
						{
							inputData[ndx]	= 0;
						}
						inputData[elemCnt * dataSize - 1]	= 1;
						inputDesc.descriptorType			= vk::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
					}
					else
					{
						const deUint32 sizeInBytes	= getSizeInBytes(COMPOSITE_DATA_TYPE_CASES[i]);
						inputData.resize(sizeInBytes);
						for (deUint32 ndx = 0; ndx < sizeInBytes - 1; ++ndx)
						{
							inputData[ndx]	= 0;
						}
						inputData[sizeInBytes - 1]	= 1;
						inputDesc.descriptorType	= vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
					}
					inputDesc.data				= inputData;
					inputDesc.dataType			= getCompositeBaseDataType(COMPOSITE_DATA_TYPE_CASES[i]);
					Resource inputResource		= createResourceFromVector(inputDesc);

					FilledResourceDesc outputDesc;
					outputDesc.dataType	= dataType;
					outputDesc.elemCount	= 1;
					outputDesc.fillType	= FillingTypes::VALUE;
					outputDesc.value		= 1;
					if (LOAD_CONTAINER_TYPE_CASES[j] == ContainerTypes::UNIFORM)
					{
						outputDesc.padding		= Constants::uniformAlignment - getSizeInBytes(COMPOSITE_DATA_TYPE_CASES[i]);
					}
					else
					{
						outputDesc.padding		= 0;
					}
					Resource outputResource	= createFilledResource(outputDesc);

					if (LOAD_CONTAINER_TYPE_CASES[j] == ContainerTypes::PUSH_CONSTANT)
					{
						spec.pushConstants	= inputResource.getBuffer();
					}
					else
					{
						spec.inputs.push_back(inputResource);
					}
					spec.assembly		= shaderAsm;
					spec.numWorkGroups	= tcu::IVec3(1, 1, 1);
					spec.outputs.push_back(outputResource);
					spec.extensions.push_back("VK_KHR_storage_buffer_storage_class");
					spec.extensions.push_back("VK_KHR_shader_untyped_pointers");

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

void addStoreTests(tcu::TestCaseGroup* testGroup, MEMORY_MODEL_TYPE memModel)
{
	tcu::TestContext& testCtx = testGroup->getTestContext();

	const tcu::StringTemplate shaderHeader(
		createShaderHeader()
	);

	const tcu::StringTemplate shaderAnnotations(
		createShaderAnnotations(BaseTestCases::STORE)
	);

	const tcu::StringTemplate shaderVariables(
		createShaderVariables(BaseTestCases::STORE)
	);

	const tcu::StringTemplate shaderFunctions(
		createShaderMain(BaseTestCases::STORE)
	);

	for (deUint32 i = 0; i < DE_LENGTH_OF_ARRAY(BASE_DATA_TYPE_CASES); ++i)
	{
		std::string testName	= toString(BASE_DATA_TYPE_CASES[i]);

		std::map<std::string, std::string>	specMap;
		specMap["alignment"]	= std::to_string(getSizeInBytes(BASE_DATA_TYPE_CASES[i]));
		specMap["args"]			= STORE_OPERATION_CASES[0].pArgs;
		specMap["baseDecl"]		= getDeclaration(BASE_DATA_TYPE_CASES[i]);
		specMap["baseType"]		= toString(BASE_DATA_TYPE_CASES[i]);
		specMap["storeOp"]		= STORE_OPERATION_CASES[0].pOperation;
		specMap["threadCount"]	= std::to_string(Constants::numThreads);
		specMap["storageCap"]	= "OpCapability UntypedPointersStorageBufferKHR\n";

		std::string shaderVariablesStr = shaderVariables.specialize(specMap);
		if (BASE_DATA_TYPE_CASES[i] != DataTypes::UINT32)
		{
			shaderVariablesStr	= "%uint32     = OpTypeInt  32      0\n"
								  "%c_uint32_1 = OpConstant %uint32 1\n"
								+ shaderVariablesStr;
		}

		ComputeShaderSpec spec;
		adjustSpecForUntypedPointers(spec, specMap);
		adjustSpecForMemoryModel(spec, specMap, memModel);
		adjustSpecForDataTypes(spec, specMap, BASE_DATA_TYPE_CASES[i]);

		const std::string shaderAsm =
			shaderHeader.specialize(specMap) +
			shaderAnnotations.specialize(specMap) +
			shaderVariablesStr +
			shaderFunctions.specialize(specMap);

		FilledResourceDesc desc;
		desc.dataType		= BASE_DATA_TYPE_CASES[i];
		desc.elemCount		= Constants::numThreads;
		desc.descriptorType	= vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		desc.padding		= 0;
		desc.fillType		= FillingTypes::RANDOM;
		desc.seed			= deStringHash(testGroup->getName());

		Resource inputOutputResource	= createFilledResource(desc);

		spec.assembly		= shaderAsm;
		spec.numWorkGroups	= tcu::IVec3(Constants::numThreads, 1, 1);
		spec.inputs.push_back(inputOutputResource);
		spec.outputs.push_back(inputOutputResource);
		spec.extensions.push_back("VK_KHR_storage_buffer_storage_class");
		spec.extensions.push_back("VK_KHR_shader_untyped_pointers");

		testGroup->addChild(new SpvAsmComputeShaderCase(testCtx, testName.c_str(), spec));
	}
}

void addStoreAtomicTests(tcu::TestCaseGroup* testGroup, MEMORY_MODEL_TYPE memModel)
{
	tcu::TestContext& testCtx = testGroup->getTestContext();

	const tcu::StringTemplate shaderHeader(
		createShaderHeader()
	);

	const tcu::StringTemplate shaderAnnotations(
		createShaderAnnotations(AtomicTestCases::OP_ATOMIC_STORE)
	);

	const tcu::StringTemplate shaderVariables(
		createShaderVariables(AtomicTestCases::OP_ATOMIC_STORE)
	);

	const tcu::StringTemplate shaderFunctions(
		createShaderMain(AtomicTestCases::OP_ATOMIC_STORE)
	);

	for (deUint32 i = 0; i < DE_LENGTH_OF_ARRAY(BASE_DATA_TYPE_CASES); ++i)
	{
		std::string testName	= toString(BASE_DATA_TYPE_CASES[i]);

		std::map<std::string, std::string>	specMap;
		specMap["alignment"]	= std::to_string(getSizeInBytes(BASE_DATA_TYPE_CASES[i]));
		specMap["args"]			= STORE_OPERATION_CASES[1].pArgs;
		specMap["baseDecl"]		= getDeclaration(BASE_DATA_TYPE_CASES[i]);
		specMap["baseType"]		= toString(BASE_DATA_TYPE_CASES[i]);
		specMap["storeOp"]		= STORE_OPERATION_CASES[1].pOperation;
		specMap["threadCount"]	= std::to_string(Constants::numThreads);
		specMap["storageCap"]	= "OpCapability UntypedPointersStorageBufferKHR\n";

		std::string shaderVariablesStr = shaderVariables.specialize(specMap);
		if (BASE_DATA_TYPE_CASES[i] != DataTypes::UINT32)
		{
			shaderVariablesStr	= "%uint32     = OpTypeInt  32      0\n"
								+ shaderVariablesStr;
		}

		ComputeShaderSpec spec;
		adjustSpecForUntypedPointers(spec, specMap);
		adjustSpecForMemoryModel(spec, specMap, memModel);
		adjustSpecForDataTypes(spec, specMap, BASE_DATA_TYPE_CASES[i]);
		adjustSpecForAtomicOperations(spec, specMap, BASE_DATA_TYPE_CASES[i]);

		const std::string shaderAsm =
			shaderHeader.specialize(specMap) +
			shaderAnnotations.specialize(specMap) +
			shaderVariablesStr +
			shaderFunctions.specialize(specMap);

		FilledResourceDesc desc;
		desc.dataType		= BASE_DATA_TYPE_CASES[i];
		desc.elemCount		= Constants::numThreads;
		desc.descriptorType	= vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		desc.padding		= 0;
		desc.fillType		= FillingTypes::RANDOM;
		desc.seed			= deStringHash(testGroup->getName());

		Resource inputOutputResource	= createFilledResource(desc);

		spec.assembly		= shaderAsm;
		spec.numWorkGroups	= tcu::IVec3(Constants::numThreads, 1, 1);
		spec.inputs.push_back(inputOutputResource);
		spec.outputs.push_back(inputOutputResource);
		spec.extensions.push_back("VK_KHR_storage_buffer_storage_class");
		spec.extensions.push_back("VK_KHR_shader_untyped_pointers");

		testGroup->addChild(new SpvAsmComputeShaderCase(testCtx, testName.c_str(), spec));
	}
}

void addStoreMixedTypeTests(tcu::TestCaseGroup* testGroup, MEMORY_MODEL_TYPE memModel)
{
	tcu::TestContext& testCtx = testGroup->getTestContext();

	{
		const tcu::StringTemplate shaderHeader(
			createShaderHeader()
		);

		const tcu::StringTemplate shaderAnnotations(
			createShaderAnnotations(TypePunningTestCases::STORE_SAME_SIZE_TYPES)
		);

		const tcu::StringTemplate shaderVariables(
			createShaderVariables(TypePunningTestCases::STORE_SAME_SIZE_TYPES)
		);

		const tcu::StringTemplate shaderFunctions(
			createShaderMain(TypePunningTestCases::STORE_SAME_SIZE_TYPES)
		);

		for (deUint32 i = 0; i < DE_LENGTH_OF_ARRAY(BASE_DATA_TYPE_CASES); ++i)
		{
			std::vector<DATA_TYPE> sameSizeTypes = getSameSizeBaseDataType(BASE_DATA_TYPE_CASES[i]);

			for (deUint32 k = 0; k < sameSizeTypes.size(); ++k)
			{
				const DATA_TYPE	dataType	= sameSizeTypes[k];
				const deUint32	otherIndex	= static_cast<deUint32>(dataType);

				std::string testName	= toString(BASE_DATA_TYPE_CASES[i]) + std::string("_to_") + toString(dataType);

				std::map<std::string, std::string>	specMap;
				specMap["alignment"]	= std::to_string(getSizeInBytes(BASE_DATA_TYPE_CASES[i]));
				specMap["args"]			= STORE_OPERATION_CASES[0].pArgs;
				specMap["baseType"]		= toString(BASE_DATA_TYPE_CASES[i]);
				specMap["baseDecl"]		= getDeclaration(BASE_DATA_TYPE_CASES[i]);
				specMap["sameSizeType"]	= toString(BASE_DATA_TYPE_CASES[otherIndex]);
				specMap["sameSizeDecl"]	= getDeclaration(BASE_DATA_TYPE_CASES[otherIndex]);
				specMap["storeOp"]		= STORE_OPERATION_CASES[0].pOperation;
				specMap["threadCount"]	= std::to_string(Constants::numThreads);
				specMap["otherCap"]		= getCapability(dataType);

				std::string shaderVariablesStr	= shaderVariables.specialize(specMap);
				if ((BASE_DATA_TYPE_CASES[i] != DataTypes::UINT32) && (dataType != DataTypes::UINT32))
				{
					shaderVariablesStr	= "%uint32     = OpTypeInt 32 0\n"
										  "%c_uint32_1 = OpConstant %uint32 1\n"
										+ shaderVariablesStr;
				}

				ComputeShaderSpec spec;
				adjustSpecForUntypedPointers(spec, specMap);
				adjustSpecForMemoryModel(spec, specMap, memModel);
				adjustSpecForDataTypes(spec, specMap, BASE_DATA_TYPE_CASES[i]);

				const std::string shaderAsm =
					shaderHeader.specialize(specMap) +
					shaderAnnotations.specialize(specMap) +
					shaderVariablesStr +
					shaderFunctions.specialize(specMap);

				FilledResourceDesc desc;
				desc.dataType		= BASE_DATA_TYPE_CASES[i];
				desc.elemCount		= Constants::numThreads;
				desc.descriptorType	= vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
				desc.padding		= 0;
				desc.fillType		= FillingTypes::RANDOM;
				desc.seed			= deStringHash(testGroup->getName());

				Resource inputOutputResource	= createFilledResource(desc);

				spec.assembly		= shaderAsm;
				spec.numWorkGroups	= tcu::IVec3(Constants::numThreads, 1, 1);
				spec.inputs.push_back(inputOutputResource);
				spec.outputs.push_back(inputOutputResource);
				spec.extensions.push_back("VK_KHR_storage_buffer_storage_class");
				spec.extensions.push_back("VK_KHR_shader_untyped_pointers");

				testGroup->addChild(new SpvAsmComputeShaderCase(testCtx, testName.c_str(), spec));
			}
		}
	}

	{
		const tcu::StringTemplate shaderHeader(
			createShaderHeader()
		);

		const tcu::StringTemplate shaderAnnotations(
			createShaderAnnotations(TypePunningTestCases::STORE_SCALAR_VECTOR)
		);

		const tcu::StringTemplate shaderVariables(
			createShaderVariables(TypePunningTestCases::STORE_SCALAR_VECTOR)
		);

		const tcu::StringTemplate shaderFunctions(
			createShaderMain(TypePunningTestCases::STORE_SCALAR_VECTOR)
		);

		for (deUint32 i = 0; i < DE_LENGTH_OF_ARRAY(BASE_DATA_TYPE_CASES); ++i)
		{
			std::vector<COMPOSITE_DATA_TYPE> sameSizeTypes = getSameSizeCompositeType(BASE_DATA_TYPE_CASES[i]);

			for (deUint32 k = 0; k < sameSizeTypes.size(); ++k)
			{
				const COMPOSITE_DATA_TYPE compositeType	= sameSizeTypes[k];

				std::string testName	= toString(BASE_DATA_TYPE_CASES[i]) + std::string("_to_") + toString(compositeType);

				std::map<std::string, std::string>	specMap;
				specMap["alignment"]		= std::to_string(getSizeInBytes(BASE_DATA_TYPE_CASES[i]));
				specMap["args"]				= STORE_OPERATION_CASES[0].pArgs;
				specMap["baseType"]			= toString(BASE_DATA_TYPE_CASES[i]);
				specMap["otherType"]		= toString(getCompositeBaseDataType(compositeType));
				specMap["baseDecl"]			= getDeclaration(BASE_DATA_TYPE_CASES[i]);
				specMap["otherTypeDecl"]	= getDeclaration(getCompositeBaseDataType(compositeType));
				specMap["storeOp"]			= STORE_OPERATION_CASES[0].pOperation;
				specMap["otherVec"]			= toString(compositeType);
				specMap["otherVecDecl"]		= getDeclaration(compositeType);
				specMap["otherCap"]			= getCapability(compositeType);

				if (sameSizeTypes[k] != CompositeDataTypes::VEC3_UINT32)
					specMap["inputVec"]	= "%vec3_uint32 = OpTypeVector %uint32 3";

				std::string shaderVariablesStr	= shaderVariables.specialize(specMap);
				if ((BASE_DATA_TYPE_CASES[i] != DataTypes::UINT32) &&
					(getCompositeBaseDataType(compositeType) != DataTypes::UINT32) &&
					compositeType != CompositeDataTypes::VEC2_UINT32 &&
					compositeType != CompositeDataTypes::VEC3_UINT32 &&
					compositeType != CompositeDataTypes::VEC4_UINT32)
				{
					shaderVariablesStr	= "%uint32      = OpTypeInt 32 0\n"
										  "%c_uint32_1  = OpConstant %uint32 1\n"
										+ shaderVariablesStr;
				}

				ComputeShaderSpec spec;
				adjustSpecForUntypedPointers(spec, specMap);
				adjustSpecForMemoryModel(spec, specMap, memModel);
				adjustSpecForDataTypes(spec, specMap, BASE_DATA_TYPE_CASES[i]);

				const std::string shaderAsm =
					shaderHeader.specialize(specMap) +
					shaderAnnotations.specialize(specMap) +
					shaderVariablesStr +
					shaderFunctions.specialize(specMap);

				FilledResourceDesc desc;
				desc.dataType		= BASE_DATA_TYPE_CASES[i];
				desc.elemCount		= 1;
				desc.descriptorType	= vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
				desc.padding		= 0;
				desc.fillType		= FillingTypes::VALUE;
				desc.value			= 1;

				Resource inputOutputResource = createFilledResource(desc);

				spec.assembly		= shaderAsm;
				spec.numWorkGroups	= tcu::IVec3(1, 1, 1);
				spec.inputs.push_back(inputOutputResource);
				spec.outputs.push_back(inputOutputResource);
				spec.extensions.push_back("VK_KHR_storage_buffer_storage_class");
				spec.extensions.push_back("VK_KHR_shader_untyped_pointers");

				testGroup->addChild(new SpvAsmComputeShaderCase(testCtx, testName.c_str(), spec));
			}
		}
	}

	{
		const tcu::StringTemplate shaderHeader(
			createShaderHeader()
		);

		const tcu::StringTemplate shaderAnnotations(
			createShaderAnnotations(TypePunningTestCases::STORE_VECTOR_SCALAR)
		);

		const tcu::StringTemplate shaderVariables(
			createShaderVariables(TypePunningTestCases::STORE_VECTOR_SCALAR)
		);

		const tcu::StringTemplate shaderFunctions(
			createShaderMain(TypePunningTestCases::STORE_VECTOR_SCALAR)
		);

		for (deUint32 i = 0; i < DE_LENGTH_OF_ARRAY(COMPOSITE_DATA_TYPE_CASES); ++i)
		{
			std::vector<DATA_TYPE> sameSizeTypes = getSameSizeBaseDataType(COMPOSITE_DATA_TYPE_CASES[i]);

			for (deUint32 k = 0; k < sameSizeTypes.size(); ++k)
			{
				const DATA_TYPE	dataType	= sameSizeTypes[k];
				const deUint32	otherIndex	= static_cast<deUint32>(dataType);

				std::string testName	= toString(COMPOSITE_DATA_TYPE_CASES[i]) + std::string("_to_") + toString(dataType);

				std::map<std::string, std::string>	specMap;
				specMap["args"]				= STORE_OPERATION_CASES[0].pArgs;
				specMap["storeOp"]			= STORE_OPERATION_CASES[0].pOperation;
				specMap["baseType"]			= toString(getCompositeBaseDataType(COMPOSITE_DATA_TYPE_CASES[i]));
				specMap["otherType"]		= toString(dataType);
				specMap["baseDecl"]			= getDeclaration(getCompositeBaseDataType(COMPOSITE_DATA_TYPE_CASES[i]));
				specMap["otherTypeDecl"]	= getDeclaration(BASE_DATA_TYPE_CASES[otherIndex]);
				specMap["otherCap"]			= getCapability(dataType);
				specMap["baseVec"]			= toString(COMPOSITE_DATA_TYPE_CASES[i]);
				specMap["baseVecDecl"]		= getDeclaration(COMPOSITE_DATA_TYPE_CASES[i]);

				if (COMPOSITE_DATA_TYPE_CASES[i] != CompositeDataTypes::VEC3_UINT32)
					specMap["inputVec"]		= "%vec3_uint32 = OpTypeVector %uint32 3";

				std::string shaderVariablesStr	= shaderVariables.specialize(specMap);
				if ((getCompositeBaseDataType(COMPOSITE_DATA_TYPE_CASES[i]) != DataTypes::UINT32) &&
					(dataType != DataTypes::UINT32))
				{
					shaderVariablesStr	= "%uint32      = OpTypeInt 32 0\n"
										  "%c_uint32_1  = OpConstant %uint32 1\n"
										+ shaderVariablesStr;
				}

				ComputeShaderSpec spec;
				adjustSpecForUntypedPointers(spec, specMap);
				adjustSpecForMemoryModel(spec, specMap, memModel);
				adjustSpecForDataTypes(spec, specMap, getCompositeBaseDataType(COMPOSITE_DATA_TYPE_CASES[i]));

				const std::string shaderAsm =
					shaderHeader.specialize(specMap) +
					shaderAnnotations.specialize(specMap) +
					shaderVariablesStr +
					shaderFunctions.specialize(specMap);

				FilledResourceDesc desc;
				desc.dataType		= getCompositeBaseDataType(COMPOSITE_DATA_TYPE_CASES[i]);
				desc.elemCount		= getElementCount(COMPOSITE_DATA_TYPE_CASES[i]);
				desc.fillType		= FillingTypes::VALUE;
				desc.value			= 1;
				desc.descriptorType	= vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
				desc.padding		= 0;

				Resource inputOutputResource = createFilledResource(desc);

				spec.assembly		= shaderAsm;
				spec.numWorkGroups	= tcu::IVec3(1, 1, 1);
				spec.inputs.push_back(inputOutputResource);
				spec.outputs.push_back(inputOutputResource);
				spec.extensions.push_back("VK_KHR_storage_buffer_storage_class");
				spec.extensions.push_back("VK_KHR_shader_untyped_pointers");

				testGroup->addChild(new SpvAsmComputeShaderCase(testCtx, testName.c_str(), spec));
			}
		}
	}
}

void addCopyTests(tcu::TestCaseGroup* testGroup, MEMORY_MODEL_TYPE memModel, deBool fromUntyped)
{
	tcu::TestContext& testCtx = testGroup->getTestContext();

	const BASE_TEST_CASE	testCase	= fromUntyped ? BaseTestCases::COPY_FROM : BaseTestCases::COPY_TO;

	de::MovePtr<tcu::TestCaseGroup>	objectGroup(new tcu::TestCaseGroup(testCtx, "op_copy_object", ""));
	de::MovePtr<tcu::TestCaseGroup>	memoryGroup(new tcu::TestCaseGroup(testCtx, "op_copy_memory", ""));
	de::MovePtr<tcu::TestCaseGroup>	memorySizedGroup(new tcu::TestCaseGroup(testCtx, "op_copy_memory_sized", ""));

	const tcu::StringTemplate shaderHeader(
		createShaderHeader()
	);

	const tcu::StringTemplate shaderAnnotations(
		createShaderAnnotations(testCase)
	);

	const tcu::StringTemplate shaderVariables(
		createShaderVariables(testCase)
	);

	const tcu::StringTemplate shaderFunctions(
		createShaderMain(testCase)
	);

	for (deUint32 i = 0; i < DE_LENGTH_OF_ARRAY(BASE_DATA_TYPE_CASES); ++i)
	{
		for (deUint32 j = 0; j < DE_LENGTH_OF_ARRAY(COPY_OPERATION_CASES); ++j)
		{
			std::string testName	= toString(BASE_DATA_TYPE_CASES[i]);

			std::map<std::string, std::string>	specMap;
			specMap["copyOp"]		= COPY_OPERATION_CASES[j].pCopyOp;
			specMap["alignment"]	= std::to_string(getSizeInBytes(BASE_DATA_TYPE_CASES[i]));
			specMap["baseDecl"]		= getDeclaration(BASE_DATA_TYPE_CASES[i]);
			specMap["baseType"]		= toString(BASE_DATA_TYPE_CASES[i]);
			specMap["threadCount"]	= std::to_string(Constants::numThreads);
			specMap["size"]			= std::to_string(getSizeInBytes(BASE_DATA_TYPE_CASES[i]));
			specMap["copyType"]		= toString(BASE_DATA_TYPE_CASES[i]);

			const tcu::StringTemplate tempShaderFunctions	= tcu::StringTemplate(shaderFunctions.specialize(specMap));
			std::string shaderVariablesStr					= shaderVariables.specialize(specMap);
			if (BASE_DATA_TYPE_CASES[i] != DataTypes::UINT32)
			{
				shaderVariablesStr	= "%uint32     = OpTypeInt 32 0\n"
									  "%c_uint32_1 = OpConstant %uint32 1\n"
									+ shaderVariablesStr;
			}

			ComputeShaderSpec spec;
			adjustSpecForUntypedPointers(spec, specMap);
			adjustSpecForMemoryModel(spec, specMap, memModel);
			adjustSpecForDataTypes(spec, specMap, BASE_DATA_TYPE_CASES[i]);
			adjustSpecForSmallContainerType(spec, specMap, ContainerTypes::STORAGE_BUFFER, BASE_DATA_TYPE_CASES[i]);

			const std::string shaderAsm =
				shaderHeader.specialize(specMap) +
				shaderAnnotations.specialize(specMap) +
				shaderVariablesStr +
				tempShaderFunctions.specialize(specMap);

			FilledResourceDesc desc;
			desc.dataType		= BASE_DATA_TYPE_CASES[i];
			desc.elemCount		= Constants::numThreads;
			desc.descriptorType	= vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			desc.padding		= 0;
			desc.fillType		= FillingTypes::RANDOM;
			desc.seed			= deStringHash(testGroup->getName());

			Resource inputOutputResource	= createFilledResource(desc);

			spec.assembly		= shaderAsm;
			spec.numWorkGroups	= tcu::IVec3(Constants::numThreads, 1, 1);
			spec.inputs.push_back(inputOutputResource);
			spec.outputs.push_back(inputOutputResource);
			spec.extensions.push_back("VK_KHR_storage_buffer_storage_class");
			spec.extensions.push_back("VK_KHR_shader_untyped_pointers");

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

void addCopyFromUntypedMixedTypeTests(tcu::TestCaseGroup* testGroup, MEMORY_MODEL_TYPE memModel)
{
	tcu::TestContext& testCtx = testGroup->getTestContext();

	de::MovePtr<tcu::TestCaseGroup>	memoryGroup(new tcu::TestCaseGroup(testCtx, "op_copy_memory", ""));
	de::MovePtr<tcu::TestCaseGroup>	memorySizedGroup(new tcu::TestCaseGroup(testCtx, "op_copy_memory_sized", ""));

	{
		const tcu::StringTemplate shaderHeader(
			createShaderHeader()
		);

		const tcu::StringTemplate shaderAnnotations(
			createShaderAnnotations(TypePunningTestCases::COPY_FROM_SAME_SIZE_TYPES)
		);

		const tcu::StringTemplate shaderVariables(
			createShaderVariables(TypePunningTestCases::COPY_FROM_SAME_SIZE_TYPES)
		);

		const tcu::StringTemplate shaderFunctions(
			createShaderMain(TypePunningTestCases::COPY_FROM_SAME_SIZE_TYPES)
		);

		for (deUint32 i = 0; i < DE_LENGTH_OF_ARRAY(BASE_DATA_TYPE_CASES); ++i)
		{
			for (deUint32 j = 1; j < DE_LENGTH_OF_ARRAY(COPY_OPERATION_CASES); ++j)
			{
				std::vector<DATA_TYPE> sameSizeTypes = getSameSizeBaseDataType(BASE_DATA_TYPE_CASES[i]);

				for (deUint32 k = 0; k < sameSizeTypes.size(); ++k)
				{
					const DATA_TYPE	dataType	= sameSizeTypes[k];
					const deUint32	otherIndex	= static_cast<deUint32>(dataType);

					std::string testName	= std::string(toString(BASE_DATA_TYPE_CASES[i])) + "_to_" + toString(sameSizeTypes[k]);

					std::map<std::string, std::string>	specMap;
					specMap["copyOp"]		= COPY_OPERATION_CASES[j].pCopyOp;
					specMap["alignment"]	= std::to_string(getSizeInBytes(BASE_DATA_TYPE_CASES[i]));
					specMap["baseDecl"]		= getDeclaration(BASE_DATA_TYPE_CASES[i]);
					specMap["sameSizeDecl"]	= getDeclaration(BASE_DATA_TYPE_CASES[otherIndex]);
					specMap["baseType"]		= toString(BASE_DATA_TYPE_CASES[i]);
					specMap["sameSizeType"]	= toString(BASE_DATA_TYPE_CASES[otherIndex]);
					specMap["threadCount"]	= std::to_string(Constants::numThreads);
					specMap["size"]			= std::to_string(getSizeInBytes(BASE_DATA_TYPE_CASES[i]));
					specMap["otherCap"]		= getCapability(dataType);
					specMap["copyType"]		= toString(BASE_DATA_TYPE_CASES[i]);

					const tcu::StringTemplate tempShaderFunctions = tcu::StringTemplate(shaderFunctions.specialize(specMap));
					std::string shaderVariablesStr = shaderVariables.specialize(specMap);
					if ((BASE_DATA_TYPE_CASES[i] != DataTypes::UINT32) && (sameSizeTypes[k] != DataTypes::UINT32))
					{
						shaderVariablesStr	= "%uint32     = OpTypeInt 32 0\n"
											  "%c_uint32_1 = OpConstant %uint32 1\n"
											+ shaderVariablesStr;
					}

					ComputeShaderSpec spec;
					adjustSpecForUntypedPointers(spec, specMap);
					adjustSpecForMemoryModel(spec, specMap, memModel);
					adjustSpecForDataTypes(spec, specMap, BASE_DATA_TYPE_CASES[i]);
					adjustSpecForSmallContainerType(spec, specMap, ContainerTypes::STORAGE_BUFFER, BASE_DATA_TYPE_CASES[i]);

					const std::string shaderAsm =
						shaderHeader.specialize(specMap) +
						shaderAnnotations.specialize(specMap) +
						shaderVariablesStr +
						tempShaderFunctions.specialize(specMap);

					FilledResourceDesc desc;
					desc.dataType		= BASE_DATA_TYPE_CASES[i];
					desc.elemCount		= Constants::numThreads;
					desc.descriptorType	= vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
					desc.padding		= 0;
					desc.fillType		= FillingTypes::RANDOM;
					desc.seed			= deStringHash(testGroup->getName());

					Resource inputOutputResource	= createFilledResource(desc);

					spec.assembly		= shaderAsm;
					spec.numWorkGroups	= tcu::IVec3(Constants::numThreads, 1, 1);
					spec.inputs.push_back(inputOutputResource);
					spec.outputs.push_back(inputOutputResource);
					spec.extensions.push_back("VK_KHR_storage_buffer_storage_class");
					spec.extensions.push_back("VK_KHR_shader_untyped_pointers");

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
		const tcu::StringTemplate shaderHeader(
			createShaderHeader()
		);

		const tcu::StringTemplate shaderAnnotations(
			createShaderAnnotations(TypePunningTestCases::COPY_FROM_SCALAR_VECTOR)
		);

		const tcu::StringTemplate shaderVariables(
			createShaderVariables(TypePunningTestCases::COPY_FROM_SCALAR_VECTOR)
		);

		const tcu::StringTemplate shaderFunctions(
			createShaderMain(TypePunningTestCases::COPY_FROM_SCALAR_VECTOR)
		);

		for (deUint32 i = 0; i < DE_LENGTH_OF_ARRAY(BASE_DATA_TYPE_CASES); ++i)
		{
			for (deUint32 j = 1; j < DE_LENGTH_OF_ARRAY(COPY_OPERATION_CASES); ++j)
			{
				std::vector<COMPOSITE_DATA_TYPE> sameSizeTypes = getSameSizeCompositeType(BASE_DATA_TYPE_CASES[i]);

				for (deUint32 k = 0; k < sameSizeTypes.size(); ++k)
				{
					const COMPOSITE_DATA_TYPE	compositeType	= sameSizeTypes[k];
					const deUint32				otherIndex		= static_cast<deUint32>(getCompositeBaseDataType(compositeType));

					std::string testName	= std::string(toString(BASE_DATA_TYPE_CASES[i])) + "_to_" + toString(compositeType);

					std::map<std::string, std::string>	specMap;
					specMap["copyOp"]			= COPY_OPERATION_CASES[j].pCopyOp;
					specMap["baseType"]			= toString(BASE_DATA_TYPE_CASES[i]);
					specMap["baseDecl"]			= getDeclaration(BASE_DATA_TYPE_CASES[i]);
					specMap["sameSizeDecl"]		= getDeclaration(BASE_DATA_TYPE_CASES[otherIndex]);
					specMap["otherType"]		= toString(getCompositeBaseDataType(compositeType));
					specMap["sameSizeType"]		= toString(BASE_DATA_TYPE_CASES[otherIndex]);
					specMap["otherTypeDecl"]	= getDeclaration(BASE_DATA_TYPE_CASES[otherIndex]);
					specMap["otherVec"]			= toString(compositeType);
					specMap["otherVecDecl"]		= getDeclaration(compositeType);
					specMap["size"]				= std::to_string(getSizeInBytes(BASE_DATA_TYPE_CASES[i]));
					specMap["otherCap"]			= getCapability(compositeType);
					specMap["copyType"]			= toString(BASE_DATA_TYPE_CASES[i]);

					if (compositeType != CompositeDataTypes::VEC3_UINT32)
						specMap["inputVec"]		= "%vec3_uint32 = OpTypeVector %uint32 3";

					std::string shaderVariablesStr = shaderVariables.specialize(specMap);
					if ((BASE_DATA_TYPE_CASES[i] != DataTypes::UINT32) &&
						(getCompositeBaseDataType(compositeType) != DataTypes::UINT32) &&
						compositeType != CompositeDataTypes::VEC2_UINT32 &&
						compositeType != CompositeDataTypes::VEC3_UINT32 &&
						compositeType != CompositeDataTypes::VEC4_UINT32)
					{
						shaderVariablesStr	= "%uint32      = OpTypeInt 32 0\n"
											  "%c_uint32_1  = OpConstant %uint32 1\n"
											+ shaderVariablesStr;
					}

					ComputeShaderSpec spec;
					adjustSpecForUntypedPointers(spec, specMap);
					adjustSpecForMemoryModel(spec, specMap, memModel);
					adjustSpecForDataTypes(spec, specMap, BASE_DATA_TYPE_CASES[i]);
					adjustSpecForSmallContainerType(spec, specMap, ContainerTypes::STORAGE_BUFFER, BASE_DATA_TYPE_CASES[i]);

					const std::string shaderAsm =
						shaderHeader.specialize(specMap) +
						shaderAnnotations.specialize(specMap) +
						shaderVariablesStr +
						shaderFunctions.specialize(specMap);

					FilledResourceDesc desc;
					desc.dataType		= BASE_DATA_TYPE_CASES[i];
					desc.elemCount		= 1;
					desc.fillType		= FillingTypes::VALUE;
					desc.value			= 1;
					desc.descriptorType	= vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
					desc.padding		= 0;

					Resource inputOutputResource	= createFilledResource(desc);

					spec.assembly		= shaderAsm;
					spec.numWorkGroups	= tcu::IVec3(1, 1, 1);
					spec.inputs.push_back(inputOutputResource);
					spec.outputs.push_back(inputOutputResource);
					spec.extensions.push_back("VK_KHR_storage_buffer_storage_class");
					spec.extensions.push_back("VK_KHR_shader_untyped_pointers");

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
		const tcu::StringTemplate shaderHeader(
			createShaderHeader()
		);

		const tcu::StringTemplate shaderAnnotations(
			createShaderAnnotations(TypePunningTestCases::COPY_FROM_VECTOR_SCALAR)
		);

		const tcu::StringTemplate shaderVariables(
			createShaderVariables(TypePunningTestCases::COPY_FROM_VECTOR_SCALAR)
		);

		const tcu::StringTemplate shaderFunctions(
			createShaderMain(TypePunningTestCases::COPY_FROM_VECTOR_SCALAR)
		);

		for (deUint32 i = 0; i < DE_LENGTH_OF_ARRAY(COMPOSITE_DATA_TYPE_CASES); ++i)
		{
			for (deUint32 j = 1; j < DE_LENGTH_OF_ARRAY(COPY_OPERATION_CASES); ++j)
			{
				std::vector<DATA_TYPE> sameSizeTypes = getSameSizeBaseDataType(COMPOSITE_DATA_TYPE_CASES[i]);

				for (deUint32 k = 0; k < sameSizeTypes.size(); ++k)
				{
					const DATA_TYPE	dataType	= sameSizeTypes[k];
					const deUint32	otherIndex	= static_cast<deUint32>(dataType);

					std::string testName	= std::string(toString(COMPOSITE_DATA_TYPE_CASES[i])) + "_to_" + toString(dataType);
					std::string testDesc	= "Test " + std::string(toString(COPY_OPERATION_CASES[j].type)) + " operation from untyped "
											+ toString(COMPOSITE_DATA_TYPE_CASES[i]) + " to " + toString(dataType);

					std::map<std::string, std::string>	specMap;
					specMap["copyOp"]			= COPY_OPERATION_CASES[j].pCopyOp;
					specMap["baseType"]			= toString(getCompositeBaseDataType(COMPOSITE_DATA_TYPE_CASES[i]));
					specMap["baseDecl"]			= getDeclaration(getCompositeBaseDataType(COMPOSITE_DATA_TYPE_CASES[i]));
					specMap["otherType"]		= toString(dataType);
					specMap["otherTypeDecl"]	= getDeclaration(BASE_DATA_TYPE_CASES[otherIndex]);
					specMap["baseVec"]			= toString(COMPOSITE_DATA_TYPE_CASES[i]);
					specMap["baseVecDecl"]		= getDeclaration(COMPOSITE_DATA_TYPE_CASES[i]);
					specMap["size"]				= std::to_string(getSizeInBytes(COMPOSITE_DATA_TYPE_CASES[i]));
					specMap["otherCap"]			= getCapability(dataType);
					specMap["copyType"]			= toString(COMPOSITE_DATA_TYPE_CASES[i]);

					if (COMPOSITE_DATA_TYPE_CASES[i] != CompositeDataTypes::VEC3_UINT32)
						specMap["inputVec"]		= "%vec3_uint32 = OpTypeVector %uint32 3";

					std::string shaderVariablesStr	= shaderVariables.specialize(specMap);
					if ((getCompositeBaseDataType(COMPOSITE_DATA_TYPE_CASES[i]) != DataTypes::UINT32) &&
						(dataType != DataTypes::UINT32))
					{
						shaderVariablesStr	= "%uint32      = OpTypeInt 32 0\n"
											  "%c_uint32_1  = OpConstant %uint32 1\n"
											+ shaderVariablesStr;
					}

					ComputeShaderSpec spec;
					adjustSpecForUntypedPointers(spec, specMap);
					adjustSpecForMemoryModel(spec, specMap, memModel);
					adjustSpecForDataTypes(spec, specMap, getCompositeBaseDataType(COMPOSITE_DATA_TYPE_CASES[i]));
					adjustSpecForSmallContainerType(spec, specMap, ContainerTypes::STORAGE_BUFFER, getCompositeBaseDataType(COMPOSITE_DATA_TYPE_CASES[i]));

					const std::string shaderAsm =
						shaderHeader.specialize(specMap) +
						shaderAnnotations.specialize(specMap) +
						shaderVariablesStr +
						shaderFunctions.specialize(specMap);

					FilledResourceDesc desc;
					desc.dataType		= getCompositeBaseDataType(COMPOSITE_DATA_TYPE_CASES[i]);
					desc.elemCount		= getElementCount(COMPOSITE_DATA_TYPE_CASES[i]);
					desc.fillType		= FillingTypes::VALUE;
					desc.value			= 1;
					desc.descriptorType	= vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
					desc.padding		= 0;

					Resource inputOutputResource = createFilledResource(desc);

					spec.assembly		= shaderAsm;
					spec.numWorkGroups	= tcu::IVec3(1, 1, 1);
					spec.inputs.push_back(inputOutputResource);
					spec.outputs.push_back(inputOutputResource);
					spec.extensions.push_back("VK_KHR_storage_buffer_storage_class");
					spec.extensions.push_back("VK_KHR_shader_untyped_pointers");

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

void addCopyToUntypedMixedTypeTests(tcu::TestCaseGroup* testGroup, MEMORY_MODEL_TYPE memModel)
{
	tcu::TestContext& testCtx = testGroup->getTestContext();

	de::MovePtr<tcu::TestCaseGroup>	memoryGroup(new tcu::TestCaseGroup(testCtx, "op_copy_memory", ""));
	de::MovePtr<tcu::TestCaseGroup>	memorySizedGroup(new tcu::TestCaseGroup(testCtx, "op_copy_memory_sized", ""));

	{
		const tcu::StringTemplate shaderHeader(
			createShaderHeader()
		);

		const tcu::StringTemplate shaderAnnotations(
			createShaderAnnotations(TypePunningTestCases::COPY_TO_SAME_SIZE_TYPES)
		);

		const tcu::StringTemplate shaderVariables(
			createShaderVariables(TypePunningTestCases::COPY_TO_SAME_SIZE_TYPES)
		);

		const tcu::StringTemplate shaderFunctions(
			createShaderMain(TypePunningTestCases::COPY_TO_SAME_SIZE_TYPES)
		);

		for (deUint32 i = 0; i < DE_LENGTH_OF_ARRAY(BASE_DATA_TYPE_CASES); ++i)
		{
			for (deUint32 j = 1; j < DE_LENGTH_OF_ARRAY(COPY_OPERATION_CASES); ++j)
			{
				std::vector<DATA_TYPE> sameSizeTypes = getSameSizeBaseDataType(BASE_DATA_TYPE_CASES[i]);

				for (deUint32 k = 0; k < sameSizeTypes.size(); ++k)
				{
					const DATA_TYPE	dataType	= sameSizeTypes[k];
					const deUint32	otherIndex	= static_cast<deUint32>(dataType);

					std::string testName	= std::string(toString(BASE_DATA_TYPE_CASES[i])) + "_to_" + toString(dataType);

					std::map<std::string, std::string>	specMap;
					specMap["copyOp"]			= COPY_OPERATION_CASES[j].pCopyOp;
					specMap["alignment"]		= std::to_string(getSizeInBytes(BASE_DATA_TYPE_CASES[i]));
					specMap["baseDecl"]			= getDeclaration(BASE_DATA_TYPE_CASES[i]);
					specMap["sameSizeDecl"]		= getDeclaration(BASE_DATA_TYPE_CASES[otherIndex]);
					specMap["baseType"]			= toString(BASE_DATA_TYPE_CASES[i]);
					specMap["sameSizeType"]		= toString(BASE_DATA_TYPE_CASES[otherIndex]);
					specMap["threadCount"]		= std::to_string(Constants::numThreads);
					specMap["size"]				= std::to_string(getSizeInBytes(BASE_DATA_TYPE_CASES[i]));
					specMap["otherCap"]			= getCapability(dataType);
					specMap["copyType"]			= toString(BASE_DATA_TYPE_CASES[i]);

					const tcu::StringTemplate tempShaderFunctions	= tcu::StringTemplate(shaderFunctions.specialize(specMap));
					std::string shaderVariablesStr	= shaderVariables.specialize(specMap);
					if ((BASE_DATA_TYPE_CASES[i] != DataTypes::UINT32) && (sameSizeTypes[k] != DataTypes::UINT32))
					{
						shaderVariablesStr	= "%uint32 = OpTypeInt 32 0\n"
											  "%c_uint32_1 = OpConstant %uint32 1\n"
											+ shaderVariablesStr;
					}

					ComputeShaderSpec spec;
					adjustSpecForUntypedPointers(spec, specMap);
					adjustSpecForMemoryModel(spec, specMap, memModel);
					adjustSpecForDataTypes(spec, specMap, BASE_DATA_TYPE_CASES[i]);
					adjustSpecForSmallContainerType(spec, specMap, ContainerTypes::STORAGE_BUFFER, BASE_DATA_TYPE_CASES[i]);

					const std::string shaderAsm =
						shaderHeader.specialize(specMap) +
						shaderAnnotations.specialize(specMap) +
						shaderVariablesStr +
						tempShaderFunctions.specialize(specMap);

					FilledResourceDesc desc;
					desc.dataType		= BASE_DATA_TYPE_CASES[i];
					desc.elemCount		= Constants::numThreads;
					desc.descriptorType	= vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
					desc.padding		= 0;
					desc.fillType		= FillingTypes::RANDOM;
					desc.seed			= deStringHash(testGroup->getName());

					Resource inputOutputResource	= createFilledResource(desc);

					spec.assembly		= shaderAsm;
					spec.numWorkGroups	= tcu::IVec3(Constants::numThreads, 1, 1);
					spec.inputs.push_back(inputOutputResource);
					spec.outputs.push_back(inputOutputResource);
					spec.extensions.push_back("VK_KHR_storage_buffer_storage_class");
					spec.extensions.push_back("VK_KHR_shader_untyped_pointers");

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
		const tcu::StringTemplate shaderHeader(
			createShaderHeader()
		);

		const tcu::StringTemplate shaderAnnotations(
			createShaderAnnotations(TypePunningTestCases::COPY_TO_SCALAR_VECTOR)
		);

		const tcu::StringTemplate shaderVariables(
			createShaderVariables(TypePunningTestCases::COPY_TO_SCALAR_VECTOR)
		);

		const tcu::StringTemplate shaderFunctions(
			createShaderMain(TypePunningTestCases::COPY_TO_SCALAR_VECTOR)
		);

		for (deUint32 i = 0; i < DE_LENGTH_OF_ARRAY(BASE_DATA_TYPE_CASES); ++i)
		{
			for (deUint32 j = 0; j < DE_LENGTH_OF_ARRAY(COPY_OPERATION_CASES); ++j)
			{
				std::vector<COMPOSITE_DATA_TYPE> sameSizeTypes = getSameSizeCompositeType(BASE_DATA_TYPE_CASES[i]);

				for (deUint32 k = 0; k < sameSizeTypes.size(); ++k)
				{
					const COMPOSITE_DATA_TYPE	compositeType	= sameSizeTypes[k];

					std::string testName	= std::string(toString(BASE_DATA_TYPE_CASES[i])) + "_to_" + toString(compositeType);

					std::map<std::string, std::string>	specMap;
					specMap["copyOp"]			= COPY_OPERATION_CASES[j].pCopyOp;
					specMap["baseType"]			= toString(BASE_DATA_TYPE_CASES[i]);
					specMap["otherType"]		= toString(getCompositeBaseDataType(compositeType));
					specMap["baseDecl"]			= getDeclaration(BASE_DATA_TYPE_CASES[i]);
					specMap["otherTypeDecl"]	= getDeclaration(getCompositeBaseDataType(compositeType));
					specMap["otherVec"]			= toString(compositeType);
					specMap["otherVecDecl"]		= getDeclaration(compositeType);
					specMap["size"]				= std::to_string(getSizeInBytes(BASE_DATA_TYPE_CASES[i]));
					specMap["otherCap"]			= getCapability(compositeType);
					specMap["copyType"]			= toString(BASE_DATA_TYPE_CASES[i]);

					if (sameSizeTypes[k] != CompositeDataTypes::VEC3_UINT32)
						specMap["inputVec"]		= "%vec3_uint32 = OpTypeVector %uint32 3";

					const tcu::StringTemplate tempShaderFunctions = tcu::StringTemplate(shaderFunctions.specialize(specMap));
					std::string shaderVariablesStr = shaderVariables.specialize(specMap);
					if ((BASE_DATA_TYPE_CASES[i] != DataTypes::UINT32) &&
						(getCompositeBaseDataType(compositeType) != DataTypes::UINT32) &&
						compositeType != CompositeDataTypes::VEC2_UINT32 &&
						compositeType != CompositeDataTypes::VEC3_UINT32 &&
						compositeType != CompositeDataTypes::VEC4_UINT32)
					{
						shaderVariablesStr	= "%uint32      = OpTypeInt 32 0\n"
											  "%c_uint32_1  = OpConstant %uint32 1\n"
											+ shaderVariablesStr;
					}

					ComputeShaderSpec spec;
					adjustSpecForUntypedPointers(spec, specMap);
					adjustSpecForMemoryModel(spec, specMap, memModel);
					adjustSpecForDataTypes(spec, specMap, BASE_DATA_TYPE_CASES[i]);
					adjustSpecForSmallContainerType(spec, specMap, ContainerTypes::STORAGE_BUFFER, BASE_DATA_TYPE_CASES[i]);

					const std::string shaderAsm =
						shaderHeader.specialize(specMap) +
						shaderAnnotations.specialize(specMap) +
						shaderVariablesStr +
						tempShaderFunctions.specialize(specMap);

					FilledResourceDesc desc;
					desc.dataType		= BASE_DATA_TYPE_CASES[i];
					desc.elemCount		= 1;
					desc.descriptorType	= vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
					desc.padding		= 0;
					desc.fillType		= FillingTypes::VALUE;
					desc.value			= 1;

					Resource inputOutputResource	= createFilledResource(desc);

					spec.assembly		= shaderAsm;
					spec.numWorkGroups	= tcu::IVec3(1, 1, 1);
					spec.inputs.push_back(inputOutputResource);
					spec.outputs.push_back(inputOutputResource);
					spec.extensions.push_back("VK_KHR_storage_buffer_storage_class");
					spec.extensions.push_back("VK_KHR_shader_untyped_pointers");

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
		const tcu::StringTemplate shaderHeader(
			createShaderHeader()
		);

		const tcu::StringTemplate shaderAnnotations(
			createShaderAnnotations(TypePunningTestCases::COPY_TO_VECTOR_SCALAR)
		);

		const tcu::StringTemplate shaderVariables(
			createShaderVariables(TypePunningTestCases::COPY_TO_VECTOR_SCALAR)
		);

		const tcu::StringTemplate shaderFunctions(
			createShaderMain(TypePunningTestCases::COPY_TO_VECTOR_SCALAR)
		);

		for (deUint32 i = 0; i < DE_LENGTH_OF_ARRAY(COMPOSITE_DATA_TYPE_CASES); ++i)
		{
			for (deUint32 j = 0; j < DE_LENGTH_OF_ARRAY(COPY_OPERATION_CASES); ++j)
			{
				std::vector<DATA_TYPE> sameSizeTypes = getSameSizeBaseDataType(COMPOSITE_DATA_TYPE_CASES[i]);

				for (deUint32 k = 0; k < sameSizeTypes.size(); ++k)
				{
					const DATA_TYPE	dataType	= sameSizeTypes[k];
					const deUint32	otherIndex	= static_cast<deUint32>(dataType);

					std::string testName	= std::string(toString(COMPOSITE_DATA_TYPE_CASES[i])) + "_to_" + toString(dataType);

					std::map<std::string, std::string>	specMap;
					specMap["copyOp"]			= COPY_OPERATION_CASES[j].pCopyOp;
					specMap["alignment"]		= std::to_string(getSizeInBytes(BASE_DATA_TYPE_CASES[i]));
					specMap["baseType"]			= toString(getCompositeBaseDataType(COMPOSITE_DATA_TYPE_CASES[i]));
					specMap["otherType"]		= toString(dataType);
					specMap["baseDecl"]			= getDeclaration(getCompositeBaseDataType(COMPOSITE_DATA_TYPE_CASES[i]));
					specMap["otherTypeDecl"]	= getDeclaration(BASE_DATA_TYPE_CASES[otherIndex]);
					specMap["baseVec"]			= toString(COMPOSITE_DATA_TYPE_CASES[i]);
					specMap["baseVecDecl"]		= getDeclaration(COMPOSITE_DATA_TYPE_CASES[i]);
					specMap["size"]				= std::to_string(getSizeInBytes(COMPOSITE_DATA_TYPE_CASES[i]));
					specMap["otherCap"]			= getCapability(sameSizeTypes[k]);
					specMap["copyType"]			= toString(COMPOSITE_DATA_TYPE_CASES[i]);

					if (COMPOSITE_DATA_TYPE_CASES[i] != CompositeDataTypes::VEC3_UINT32)
						specMap["inputVec"]		= "%vec3_uint32 = OpTypeVector %uint32 3";

					const tcu::StringTemplate tempShaderFunctions = tcu::StringTemplate(shaderFunctions.specialize(specMap));
					std::string shaderVariablesStr = shaderVariables.specialize(specMap);
					if ((getCompositeBaseDataType(COMPOSITE_DATA_TYPE_CASES[i]) != DataTypes::UINT32) &&
						(dataType != DataTypes::UINT32))
					{
						shaderVariablesStr	= "%uint32      = OpTypeInt 32 0\n"
											  "%c_uint32_1  = OpConstant %uint32 1\n"
											+ shaderVariablesStr;
					}

					ComputeShaderSpec spec;
					adjustSpecForUntypedPointers(spec, specMap);
					adjustSpecForMemoryModel(spec, specMap, memModel);
					adjustSpecForDataTypes(spec, specMap, getCompositeBaseDataType(COMPOSITE_DATA_TYPE_CASES[i]));
					adjustSpecForSmallContainerType(spec, specMap, ContainerTypes::STORAGE_BUFFER, getCompositeBaseDataType(COMPOSITE_DATA_TYPE_CASES[i]));

					const std::string shaderAsm =
						shaderHeader.specialize(specMap) +
						shaderAnnotations.specialize(specMap) +
						shaderVariablesStr +
						tempShaderFunctions.specialize(specMap);

					FilledResourceDesc desc;
					desc.dataType		= getCompositeBaseDataType(COMPOSITE_DATA_TYPE_CASES[i]);
					desc.elemCount		= getElementCount(COMPOSITE_DATA_TYPE_CASES[i]);
					desc.fillType		= FillingTypes::VALUE;
					desc.value			= 1;
					desc.descriptorType	= vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
					desc.padding		= 0;

					Resource inputOutputResource	= createFilledResource(desc);

					spec.assembly		= shaderAsm;
					spec.numWorkGroups	= tcu::IVec3(1, 1, 1);
					spec.inputs.push_back(inputOutputResource);
					spec.outputs.push_back(inputOutputResource);
					spec.extensions.push_back("VK_KHR_storage_buffer_storage_class");
					spec.extensions.push_back("VK_KHR_shader_untyped_pointers");

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

void addAtomicAddTests(tcu::TestCaseGroup* testGroup, MEMORY_MODEL_TYPE memModel)
{
	tcu::TestContext& testCtx = testGroup->getTestContext();

	tcu::StringTemplate shaderHeader(
		createShaderHeader("%output_data_untyped_var")
	);

	tcu::StringTemplate shaderAnnotations(
		createShaderAnnotations(AtomicTestCases::OP_ATOMIC_ADD)
	);

	tcu::StringTemplate shaderVariables(
		createShaderVariables(AtomicTestCases::OP_ATOMIC_ADD)
	);

	tcu::StringTemplate shaderFunctions(
		createShaderMain(AtomicTestCases::OP_ATOMIC_ADD)
	);

	for (deUint32 i = 0; i < DE_LENGTH_OF_ARRAY(BASE_DATA_TYPE_CASES); ++i)
	{
		std::string testName	= toString(BASE_DATA_TYPE_CASES[i]);

		std::map<std::string, std::string>	specMap;
		specMap["baseDecl"]		= getDeclaration(BASE_DATA_TYPE_CASES[i]);
		specMap["baseType"]		= toString(BASE_DATA_TYPE_CASES[i]);
		specMap["opType"]		= getAtomicAddOperator(BASE_DATA_TYPE_CASES[i]);
		specMap["opValue"]		= std::to_string(16);

		ComputeShaderSpec spec;
		adjustSpecForUntypedPointers(spec, specMap);
		adjustSpecForMemoryModel(spec, specMap, memModel);
		adjustSpecForDataTypes(spec, specMap, BASE_DATA_TYPE_CASES[i]);
		adjustSpecForAtomicOperations(spec, specMap, BASE_DATA_TYPE_CASES[i]);
		adjustSpecForAtomicAddOperations(spec, specMap, BASE_DATA_TYPE_CASES[i]);

		const tcu::StringTemplate tempShaderFunctions = tcu::StringTemplate(shaderFunctions.specialize(specMap));

		std::string shaderVariablesStr = shaderVariables.specialize(specMap);
		if (BASE_DATA_TYPE_CASES[i] != DataTypes::UINT32)
		{
			shaderVariablesStr	= "%uint32     = OpTypeInt  32      0\n"
								  "%c_uint32_1 = OpConstant %uint32 1\n"
								+ shaderVariablesStr;
		}

		const std::string shaderAsm =
			shaderHeader.specialize(specMap) +
			shaderAnnotations.specialize(specMap) +
			shaderVariablesStr +
			tempShaderFunctions.specialize(specMap);

		AtomicResourceDesc desc;
		desc.dataType	= BASE_DATA_TYPE_CASES[i];
		desc.elemCount	= 1;

		AtomicOpDesc atomicDesc;
		atomicDesc.type			= OP_ATOMIC_ADD;
		atomicDesc.userData0	= 16;
		atomicDesc.elemIndex	= 0;

		Resource output		= createAtomicResource(desc, std::vector<AtomicOpDesc>({ atomicDesc }));

		spec.assembly		= shaderAsm;
		spec.numWorkGroups	= tcu::IVec3(1, 1, 1);
		spec.spirvVersion	= SPIRV_VERSION_1_4;
		spec.outputs.push_back(output);
		spec.extensions.push_back("VK_KHR_storage_buffer_storage_class");
		spec.extensions.push_back("VK_KHR_shader_untyped_pointers");

		testGroup->addChild(new SpvAsmComputeShaderCase(testCtx, testName.c_str(), spec));
	}
}

void addAtomicSubtractTests(tcu::TestCaseGroup* testGroup, MEMORY_MODEL_TYPE memModel)
{
	tcu::TestContext& testCtx = testGroup->getTestContext();

	tcu::StringTemplate shaderHeader(
		createShaderHeader("%output_data_untyped_var")
	);

	tcu::StringTemplate shaderAnnotations(
		createShaderAnnotations(AtomicTestCases::OP_ATOMIC_SUB)
	);

	tcu::StringTemplate shaderVariables(
		createShaderVariables(AtomicTestCases::OP_ATOMIC_SUB)
	);

	tcu::StringTemplate shaderFunctions(
		createShaderMain(AtomicTestCases::OP_ATOMIC_SUB)
	);

	for (deUint32 i = 0; i < DE_LENGTH_OF_ARRAY(INT_BASE_DATA_TYPE_CASES); ++i)
	{
		std::string testName	= toString(INT_BASE_DATA_TYPE_CASES[i]);

		std::map<std::string, std::string>	specMap;
		specMap["baseDecl"]		= getDeclaration(INT_BASE_DATA_TYPE_CASES[i]);
		specMap["baseType"]		= toString(INT_BASE_DATA_TYPE_CASES[i]);
		specMap["opType"]		= getAtomicSubtractOperator(INT_BASE_DATA_TYPE_CASES[i]);
		specMap["opValue"]		= std::to_string(16);

		ComputeShaderSpec spec;
		adjustSpecForUntypedPointers(spec, specMap);
		adjustSpecForMemoryModel(spec, specMap, memModel);
		adjustSpecForDataTypes(spec, specMap, INT_BASE_DATA_TYPE_CASES[i]);
		adjustSpecForAtomicOperations(spec, specMap, INT_BASE_DATA_TYPE_CASES[i]);

		const tcu::StringTemplate tempShaderFunctions = tcu::StringTemplate(shaderFunctions.specialize(specMap));

		std::string shaderVariablesStr = shaderVariables.specialize(specMap);
		if (INT_BASE_DATA_TYPE_CASES[i] != DataTypes::UINT32)
		{
			shaderVariablesStr	= "%uint32     = OpTypeInt  32      0\n"
								  "%c_uint32_1 = OpConstant %uint32 1\n"
								+ shaderVariablesStr;
		}

		const std::string shaderAsm =
			shaderHeader.specialize(specMap) +
			shaderAnnotations.specialize(specMap) +
			shaderVariablesStr +
			tempShaderFunctions.specialize(specMap);

		AtomicResourceDesc desc;
		desc.dataType	= INT_BASE_DATA_TYPE_CASES[i];
		desc.elemCount	= 1;

		AtomicOpDesc atomicDesc;
		atomicDesc.type			= OP_ATOMIC_SUBTRACT;
		atomicDesc.userData0	= 16;
		atomicDesc.elemIndex	= 0;

		Resource output		= createAtomicResource(desc, std::vector<AtomicOpDesc>({ atomicDesc }));

		spec.assembly		= shaderAsm;
		spec.numWorkGroups	= tcu::IVec3(1, 1, 1);
		spec.spirvVersion	= SPIRV_VERSION_1_4;
		spec.outputs.push_back(output);
		spec.extensions.push_back("VK_KHR_storage_buffer_storage_class");
		spec.extensions.push_back("VK_KHR_shader_untyped_pointers");

		testGroup->addChild(new SpvAsmComputeShaderCase(testCtx, testName.c_str(), spec));
	}
}

void addAtomicIncrementDecrementTests(tcu::TestCaseGroup* testGroup, MEMORY_MODEL_TYPE memModel, AtomicTestCases testCase)
{
	DE_ASSERT((testCase == AtomicTestCases::OP_ATOMIC_INCREMENT) || (testCase == AtomicTestCases::OP_ATOMIC_DECREMENT));

	const AtomicOpType	opType	= testCase == AtomicTestCases::OP_ATOMIC_INCREMENT ? OP_ATOMIC_INCREMENT : OP_ATOMIC_DECREMENT;

	tcu::TestContext& testCtx	= testGroup->getTestContext();

	tcu::StringTemplate shaderHeader(
		createShaderHeader("%output_data_untyped_var")
	);

	tcu::StringTemplate shaderAnnotations(
		createShaderAnnotations(testCase)
	);

	tcu::StringTemplate shaderVariables(
		createShaderVariables(testCase)
	);

	tcu::StringTemplate shaderFunctions(
		createShaderMain(testCase)
	);

	for (deUint32 i = 0; i < DE_LENGTH_OF_ARRAY(INT_BASE_DATA_TYPE_CASES); ++i)
	{
		const std::string	testName	= toString(INT_BASE_DATA_TYPE_CASES[i]);
		const std::string	opStr		= testCase == AtomicTestCases::OP_ATOMIC_INCREMENT ?
										  getAtomicIncrementOperator(INT_BASE_DATA_TYPE_CASES[i]) : getAtomicDecrementOperator(INT_BASE_DATA_TYPE_CASES[i]);

		std::map<std::string, std::string>	specMap;
		specMap["baseDecl"]		= getDeclaration(INT_BASE_DATA_TYPE_CASES[i]);
		specMap["baseType"]		= toString(INT_BASE_DATA_TYPE_CASES[i]);
		specMap["opType"]		= opStr;

		ComputeShaderSpec spec;
		adjustSpecForUntypedPointers(spec, specMap);
		adjustSpecForMemoryModel(spec, specMap, memModel);
		adjustSpecForDataTypes(spec, specMap, INT_BASE_DATA_TYPE_CASES[i]);
		adjustSpecForAtomicOperations(spec, specMap, INT_BASE_DATA_TYPE_CASES[i]);

		const tcu::StringTemplate tempShaderFunctions = tcu::StringTemplate(shaderFunctions.specialize(specMap));

		std::string shaderVariablesStr = shaderVariables.specialize(specMap);
		if (INT_BASE_DATA_TYPE_CASES[i] != DataTypes::UINT32)
		{
			shaderVariablesStr	= "%uint32 = OpTypeInt 32 0\n"
								+ shaderVariablesStr;
		}

		const std::string shaderAsm =
			shaderHeader.specialize(specMap) +
			shaderAnnotations.specialize(specMap) +
			shaderVariablesStr +
			tempShaderFunctions.specialize(specMap);

		AtomicResourceDesc desc;
		desc.dataType	= INT_BASE_DATA_TYPE_CASES[i];
		desc.elemCount	= 1;

		AtomicOpDesc atomicDesc;
		atomicDesc.type			= opType;
		atomicDesc.elemIndex	= 0;

		Resource output		= createAtomicResource(desc, std::vector<AtomicOpDesc>({ atomicDesc }));

		spec.assembly		= shaderAsm;
		spec.numWorkGroups	= tcu::IVec3(1, 1, 1);
		spec.spirvVersion	= SPIRV_VERSION_1_4;
		spec.outputs.push_back(output);
		spec.extensions.push_back("VK_KHR_storage_buffer_storage_class");
		spec.extensions.push_back("VK_KHR_shader_untyped_pointers");

		testGroup->addChild(new SpvAsmComputeShaderCase(testCtx, testName.c_str(), spec));
	}
}

void addAtomicMinMaxTests(tcu::TestCaseGroup* testGroup, MEMORY_MODEL_TYPE memModel, AtomicTestCases testCase)
{
	DE_ASSERT((testCase == AtomicTestCases::OP_ATOMIC_MIN) || (testCase == AtomicTestCases::OP_ATOMIC_MAX));

	const AtomicOpType	opType	= testCase == AtomicTestCases::OP_ATOMIC_MIN ? OP_ATOMIC_MIN : OP_ATOMIC_MAX;

	tcu::TestContext& testCtx = testGroup->getTestContext();

	tcu::StringTemplate shaderHeader(
		createShaderHeader("%output_data_untyped_var")
	);

	tcu::StringTemplate shaderAnnotations(
		createShaderAnnotations(testCase)
	);

	tcu::StringTemplate shaderVariables(
		createShaderVariables(testCase)
	);

	tcu::StringTemplate shaderFunctions(
		createShaderMain(testCase)
	);

	for (deUint32 i = 0; i < DE_LENGTH_OF_ARRAY(INT_BASE_DATA_TYPE_CASES); ++i)
	{
		const std::string	testName	= toString(INT_BASE_DATA_TYPE_CASES[i]);
		const std::string	opStr		= testCase == AtomicTestCases::OP_ATOMIC_MIN ?
										  getAtomicMinOperator(INT_BASE_DATA_TYPE_CASES[i]) : getAtomicMaxOperator(INT_BASE_DATA_TYPE_CASES[i]);

		std::map<std::string, std::string>	specMap;
		specMap["baseDecl"]		= getDeclaration(INT_BASE_DATA_TYPE_CASES[i]);
		specMap["baseType"]		= toString(INT_BASE_DATA_TYPE_CASES[i]);
		specMap["opType"]		= opStr;
		specMap["opValue"]		= std::to_string(getSignedUnsignedMinMaxTestValue(INT_BASE_DATA_TYPE_CASES[i]));

		ComputeShaderSpec spec;
		adjustSpecForUntypedPointers(spec, specMap);
		adjustSpecForMemoryModel(spec, specMap, memModel);
		adjustSpecForDataTypes(spec, specMap, INT_BASE_DATA_TYPE_CASES[i]);
		adjustSpecForAtomicOperations(spec, specMap, INT_BASE_DATA_TYPE_CASES[i]);

		const tcu::StringTemplate tempShaderFunctions = tcu::StringTemplate(shaderFunctions.specialize(specMap));

		std::string shaderVariablesStr = shaderVariables.specialize(specMap);
		if (INT_BASE_DATA_TYPE_CASES[i] != DataTypes::UINT32)
		{
			shaderVariablesStr	= "%uint32     = OpTypeInt  32      0\n"
								  "%c_uint32_1 = OpConstant %uint32 1\n"
								+ shaderVariablesStr;
		}

		const std::string shaderAsm =
			shaderHeader.specialize(specMap) +
			shaderAnnotations.specialize(specMap) +
			shaderVariablesStr +
			tempShaderFunctions.specialize(specMap);

		AtomicResourceDesc desc;
		desc.dataType	= INT_BASE_DATA_TYPE_CASES[i];
		desc.elemCount	= 1;

		AtomicOpDesc atomicDesc;
		atomicDesc.type			= opType;
		atomicDesc.elemIndex	= 0;
		atomicDesc.userData0	= getSignedUnsignedMinMaxTestValue(INT_BASE_DATA_TYPE_CASES[i]);

		Resource output		= createAtomicResource(desc, std::vector<AtomicOpDesc>({ atomicDesc }));

		spec.assembly		= shaderAsm;
		spec.numWorkGroups	= tcu::IVec3(1, 1, 1);
		spec.spirvVersion	= SPIRV_VERSION_1_4;
		spec.outputs.push_back(output);
		spec.extensions.push_back("VK_KHR_storage_buffer_storage_class");
		spec.extensions.push_back("VK_KHR_shader_untyped_pointers");

		testGroup->addChild(new SpvAsmComputeShaderCase(testCtx, testName.c_str(), spec));
	}
}

void addAtomicBooleanTests(tcu::TestCaseGroup* testGroup, MEMORY_MODEL_TYPE memModel, AtomicTestCases testCase)
{
	DE_ASSERT((testCase == AtomicTestCases::OP_ATOMIC_AND) || (testCase == AtomicTestCases::OP_ATOMIC_OR) || (testCase == AtomicTestCases::OP_ATOMIC_XOR));

	AtomicOpType	opType{};

	switch (testCase)
	{
	case vkt::SpirVAssembly::AtomicTestCases::OP_ATOMIC_AND:
	{
		opType	= OP_ATOMIC_AND;
		break;
	}
	case vkt::SpirVAssembly::AtomicTestCases::OP_ATOMIC_OR:
	{
		opType	= OP_ATOMIC_OR;
		break;
	}
	case vkt::SpirVAssembly::AtomicTestCases::OP_ATOMIC_XOR:
	{
		opType	= OP_ATOMIC_XOR;
		break;
	}
	default:
	{
		DE_ASSERT(0);
		break;
	}
	}

	tcu::TestContext& testCtx = testGroup->getTestContext();

	tcu::StringTemplate shaderHeader(
		createShaderHeader("%output_data_untyped_var")
	);

	tcu::StringTemplate shaderAnnotations(
		createShaderAnnotations(testCase)
	);

	tcu::StringTemplate shaderVariables(
		createShaderVariables(testCase)
	);

	tcu::StringTemplate shaderFunctions(
		createShaderMain(testCase)
	);

	for (deUint32 i = 0; i < DE_LENGTH_OF_ARRAY(INT_BASE_DATA_TYPE_CASES); ++i)
	{
		const std::string	testName	= toString(INT_BASE_DATA_TYPE_CASES[i]);

		std::map<std::string, std::string>	specMap;
		specMap["baseDecl"]		= getDeclaration(INT_BASE_DATA_TYPE_CASES[i]);
		specMap["baseType"]		= toString(INT_BASE_DATA_TYPE_CASES[i]);
		specMap["opMin"]		= getAtomicMinOperator(INT_BASE_DATA_TYPE_CASES[i]);
		specMap["opType"]		= getAtomicAndOperator(INT_BASE_DATA_TYPE_CASES[i]);
		specMap["opValue"]		= std::to_string(1);

		ComputeShaderSpec spec;
		adjustSpecForUntypedPointers(spec, specMap);
		adjustSpecForMemoryModel(spec, specMap, memModel);
		adjustSpecForDataTypes(spec, specMap, INT_BASE_DATA_TYPE_CASES[i]);
		adjustSpecForAtomicOperations(spec, specMap, INT_BASE_DATA_TYPE_CASES[i]);

		const tcu::StringTemplate tempShaderFunctions = tcu::StringTemplate(shaderFunctions.specialize(specMap));

		std::string shaderVariablesStr = shaderVariables.specialize(specMap);
		if (INT_BASE_DATA_TYPE_CASES[i] != DataTypes::UINT32)
		{
			shaderVariablesStr	= "%uint32     = OpTypeInt  32      0\n"
								  "%c_uint32_1 = OpConstant %uint32 1\n"
								+ shaderVariablesStr;
		}

		const std::string shaderAsm =
			shaderHeader.specialize(specMap) +
			shaderAnnotations.specialize(specMap) +
			shaderVariablesStr +
			tempShaderFunctions.specialize(specMap);

		AtomicResourceDesc desc;
		desc.dataType	= INT_BASE_DATA_TYPE_CASES[i];
		desc.elemCount	= 1;

		AtomicOpDesc atomicDesc;
		atomicDesc.type			= opType;
		atomicDesc.elemIndex	= 0;
		atomicDesc.userData0	= 1;

		Resource output		= createAtomicResource(desc, std::vector<AtomicOpDesc>({ atomicDesc }));

		spec.assembly		= shaderAsm;
		spec.numWorkGroups	= tcu::IVec3(1, 1, 1);
		spec.spirvVersion	= SPIRV_VERSION_1_4;
		spec.outputs.push_back(output);
		spec.extensions.push_back("VK_KHR_storage_buffer_storage_class");
		spec.extensions.push_back("VK_KHR_shader_untyped_pointers");

		testGroup->addChild(new SpvAsmComputeShaderCase(testCtx, testName.c_str(), spec));
	}
}

void addAtomicExchangeTests(tcu::TestCaseGroup* testGroup, MEMORY_MODEL_TYPE memModel)
{
	tcu::TestContext& testCtx = testGroup->getTestContext();

	tcu::StringTemplate shaderHeader(
		createShaderHeader("%output_data_untyped_var")
	);

	tcu::StringTemplate shaderAnnotations(
		createShaderAnnotations(AtomicTestCases::OP_ATOMIC_EXCHANGE)
	);

	tcu::StringTemplate shaderVariables(
		createShaderVariables(AtomicTestCases::OP_ATOMIC_EXCHANGE)
	);

	tcu::StringTemplate shaderFunctions(
		createShaderMain(AtomicTestCases::OP_ATOMIC_EXCHANGE)
	);

	for (deUint32 i = 0; i < DE_LENGTH_OF_ARRAY(BASE_DATA_TYPE_CASES); ++i)
	{
		const std::string	testName	= toString(BASE_DATA_TYPE_CASES[i]);

		std::map<std::string, std::string>	specMap;
		specMap["baseDecl"]		= getDeclaration(BASE_DATA_TYPE_CASES[i]);
		specMap["baseType"]		= toString(BASE_DATA_TYPE_CASES[i]);
		specMap["opType"]		= getAtomicExchangeOperator(BASE_DATA_TYPE_CASES[i]);
		specMap["opValue"]		= std::to_string(1);

		ComputeShaderSpec spec;
		adjustSpecForUntypedPointers(spec, specMap);
		adjustSpecForMemoryModel(spec, specMap, memModel);
		adjustSpecForDataTypes(spec, specMap, BASE_DATA_TYPE_CASES[i]);
		adjustSpecForAtomicOperations(spec, specMap, BASE_DATA_TYPE_CASES[i]);

		const tcu::StringTemplate tempShaderFunctions = tcu::StringTemplate(shaderFunctions.specialize(specMap));

		std::string shaderVariablesStr = shaderVariables.specialize(specMap);
		if (BASE_DATA_TYPE_CASES[i] != DataTypes::UINT32)
		{
			shaderVariablesStr	= "%uint32     = OpTypeInt  32      0\n"
								  "%c_uint32_1 = OpConstant %uint32 1\n"
								+ shaderVariablesStr;
		}

		const std::string shaderAsm =
			shaderHeader.specialize(specMap) +
			shaderAnnotations.specialize(specMap) +
			shaderVariablesStr +
			tempShaderFunctions.specialize(specMap);

		AtomicResourceDesc desc;
		desc.dataType	= BASE_DATA_TYPE_CASES[i];
		desc.elemCount	= 1;

		AtomicOpDesc atomicDesc;
		atomicDesc.type			= OP_ATOMIC_EXCHANGE;
		atomicDesc.elemIndex	= 0;
		atomicDesc.userData0	= 1;

		Resource output		= createAtomicResource(desc, std::vector<AtomicOpDesc>({ atomicDesc }));

		spec.assembly		= shaderAsm;
		spec.numWorkGroups	= tcu::IVec3(1, 1, 1);
		spec.spirvVersion	= SPIRV_VERSION_1_4;
		spec.outputs.push_back(output);
		spec.extensions.push_back("VK_KHR_storage_buffer_storage_class");
		spec.extensions.push_back("VK_KHR_shader_untyped_pointers");

		testGroup->addChild(new SpvAsmComputeShaderCase(testCtx, testName.c_str(), spec));
	}
}

void addAtomicCompareExchangeTests(tcu::TestCaseGroup* testGroup, MEMORY_MODEL_TYPE memModel)
{
	tcu::TestContext& testCtx = testGroup->getTestContext();

	de::MovePtr<tcu::TestCaseGroup>	exchangedGroup	(new tcu::TestCaseGroup(testCtx, "exchanged", ""));
	de::MovePtr<tcu::TestCaseGroup>	notExchangedGroup	(new tcu::TestCaseGroup(testCtx, "not_exchanged", ""));

	tcu::StringTemplate shaderHeader(
		createShaderHeader("%output_data_untyped_var")
	);

	tcu::StringTemplate shaderAnnotations(
		createShaderAnnotations(AtomicTestCases::OP_ATOMIC_COMPARE_EXCHANGE)
	);

	tcu::StringTemplate shaderVariables(
		createShaderVariables(AtomicTestCases::OP_ATOMIC_COMPARE_EXCHANGE)
	);

	tcu::StringTemplate shaderFunctions(
		createShaderMain(AtomicTestCases::OP_ATOMIC_COMPARE_EXCHANGE)
	);

	for (deUint32 i = 0; i < DE_LENGTH_OF_ARRAY(INT_BASE_DATA_TYPE_CASES); ++i)
	{
		for(deUint32 j = 0; j < 2; ++j)
		{
			const std::string	testName	= toString(INT_BASE_DATA_TYPE_CASES[i]);

			std::map<std::string, std::string>	specMap;
			specMap["baseDecl"]		= getDeclaration(INT_BASE_DATA_TYPE_CASES[i]);
			specMap["baseType"]		= toString(INT_BASE_DATA_TYPE_CASES[i]);
			specMap["opMin"]		= getAtomicMinOperator(INT_BASE_DATA_TYPE_CASES[i]);
			specMap["opType"]		= getAtomicCompareExchangeOperator(INT_BASE_DATA_TYPE_CASES[i]);
			specMap["compValue"]	= std::to_string(j);
			specMap["opValue"]		= std::to_string(16);

			ComputeShaderSpec spec;
			adjustSpecForUntypedPointers(spec, specMap);
			adjustSpecForMemoryModel(spec, specMap, memModel);
			adjustSpecForDataTypes(spec, specMap, INT_BASE_DATA_TYPE_CASES[i]);
			adjustSpecForAtomicOperations(spec, specMap, INT_BASE_DATA_TYPE_CASES[i]);

			const tcu::StringTemplate tempShaderFunctions = tcu::StringTemplate(shaderFunctions.specialize(specMap));

			std::string shaderVariablesStr = shaderVariables.specialize(specMap);
			if (INT_BASE_DATA_TYPE_CASES[i] != DataTypes::UINT32)
			{
				tcu::StringTemplate	compTmp("%c_${baseType}_1 = OpConstant %${baseType} 1\n");
				std::string			compStr = compTmp.specialize(specMap);

				shaderVariablesStr	= "%uint32 = OpTypeInt 32 0\n"
									+ shaderVariablesStr
									+ compStr;
			}

			const std::string shaderAsm =
				shaderHeader.specialize(specMap) +
				shaderAnnotations.specialize(specMap) +
				shaderVariablesStr +
				tempShaderFunctions.specialize(specMap);

			AtomicResourceDesc desc;
			desc.dataType	= INT_BASE_DATA_TYPE_CASES[i];
			desc.elemCount	= 1;

			AtomicOpDesc minDesc;
			minDesc.type			= OP_ATOMIC_MIN;
			minDesc.elemIndex		= 0;
			minDesc.userData0		= 1;

			AtomicOpDesc compExDesc;
			compExDesc.type			= OP_ATOMIC_COMPARE_EXCHANGE;
			compExDesc.elemIndex	= 0;
			compExDesc.userData0	= 16;
			compExDesc.userData1	= j;

			Resource output		= createAtomicResource(desc, std::vector<AtomicOpDesc>({ minDesc, compExDesc }));

			spec.assembly		= shaderAsm;
			spec.numWorkGroups	= tcu::IVec3(1, 1, 1);
			spec.spirvVersion	= SPIRV_VERSION_1_4;
			spec.outputs.push_back(output);
			spec.extensions.push_back("VK_KHR_storage_buffer_storage_class");
			spec.extensions.push_back("VK_KHR_shader_untyped_pointers");

			if (j)	// for 1 adding to exchange group
			{
				exchangedGroup->addChild(new SpvAsmComputeShaderCase(testCtx, testName.c_str(), spec));
			}
			else	// for 0 adding to not exchange group
			{
				notExchangedGroup->addChild(new SpvAsmComputeShaderCase(testCtx, testName.c_str(), spec));
			}
		}
	}

	testGroup->addChild(exchangedGroup.release());
	testGroup->addChild(notExchangedGroup.release());
}

void addVariablePtrOpSelectTests(tcu::TestCaseGroup* testGroup, MEMORY_MODEL_TYPE memModel)
{
	tcu::TestContext& testCtx = testGroup->getTestContext();

	de::MovePtr<tcu::TestCaseGroup>	firstGroup(new tcu::TestCaseGroup(testCtx, "first", ""));
	de::MovePtr<tcu::TestCaseGroup>	secondGroup(new tcu::TestCaseGroup(testCtx, "second", ""));

	tcu::StringTemplate shaderHeader(
		createShaderHeader("%input_data_0_untyped_var %input_data_1_untyped_var %output_data_var")
	);

	tcu::StringTemplate shaderAnnotations(
		createShaderAnnotations(PointerTestCases::OP_SELECT_VARIABLE_PTR)
	);

	tcu::StringTemplate shaderVariables(
		createShaderVariables(PointerTestCases::OP_SELECT_VARIABLE_PTR)
	);

	tcu::StringTemplate shaderFunctions(
		createShaderMain(PointerTestCases::OP_SELECT_VARIABLE_PTR)
	);

	for (deUint32 i = 0; i < DE_LENGTH_OF_ARRAY(BASE_DATA_TYPE_CASES); ++i)
	{
		for (deUint32 j = 0; j < 2; ++j)
		{
			std::string testName	= toString(BASE_DATA_TYPE_CASES[i]);

			std::map<std::string, std::string>	specMap;
			if (j)
			{
				specMap["boolConst"]	= "%c_bool_true = OpConstantTrue %bool";
				specMap["condition"]	= "%c_bool_true";
			}
			else
			{
				specMap["boolConst"]	= "%c_bool_false = OpConstantFalse %bool";
				specMap["condition"]	= "%c_bool_false";
			}

			specMap["baseDecl"]	= getDeclaration(BASE_DATA_TYPE_CASES[i]);
			specMap["baseType"]	= toString(BASE_DATA_TYPE_CASES[i]);

			ComputeShaderSpec spec;
			adjustSpecForUntypedPointers(spec, specMap);
			adjustSpecForMemoryModel(spec, specMap, memModel);
			adjustSpecForDataTypes(spec, specMap, BASE_DATA_TYPE_CASES[i]);
			adjustSpecForVariablePointers(spec, specMap);

			const tcu::StringTemplate tempShaderFunctions = tcu::StringTemplate(shaderFunctions.specialize(specMap));

			std::string shaderVariablesStr = shaderVariables.specialize(specMap);
			if (BASE_DATA_TYPE_CASES[i] != DataTypes::UINT32)
			{
				shaderVariablesStr	= "%uint32     = OpTypeInt 32 0\n"
									  "%c_uint32_1 = OpConstant %uint32 1\n"
									+ shaderVariablesStr;
			}

			const std::string shaderAsm =
				shaderHeader.specialize(specMap) +
				shaderAnnotations.specialize(specMap) +
				shaderVariablesStr +
				tempShaderFunctions.specialize(specMap);

			FilledResourceDesc desc;
			desc.dataType		= BASE_DATA_TYPE_CASES[i];
			desc.elemCount		= 1;
			desc.descriptorType	= vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			desc.padding		= 0;
			desc.fillType		= FillingTypes::VALUE;
			desc.value			= 1;
			Resource input0		= createFilledResource(desc);
			desc.value			= 0;
			Resource input1		= createFilledResource(desc);

			if (j)
			{
				desc.fillType	= FillingTypes::VALUE;
				desc.value		= 1.0;
				Resource output	= createFilledResource(desc);
				spec.outputs.push_back(output);
			}
			else
			{
				desc.fillType	= FillingTypes::VALUE;
				desc.value		= 0.0;
				Resource output	= createFilledResource(desc);
				spec.outputs.push_back(output);
			}

			spec.assembly		= shaderAsm;
			spec.numWorkGroups	= tcu::IVec3(1, 1, 1);
			spec.spirvVersion	= SPIRV_VERSION_1_4;
			spec.inputs.push_back(input0);
			spec.inputs.push_back(input1);
			spec.extensions.push_back("VK_KHR_storage_buffer_storage_class");
			spec.extensions.push_back("VK_KHR_shader_untyped_pointers");

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

void addPhysicalStorageOpSelectTests(tcu::TestCaseGroup* testGroup, MEMORY_MODEL_TYPE memModel)
{
	tcu::TestContext& testCtx = testGroup->getTestContext();

	de::MovePtr<tcu::TestCaseGroup>	firstGroup(new tcu::TestCaseGroup(testCtx, "first", ""));
	de::MovePtr<tcu::TestCaseGroup>	secondGroup(new tcu::TestCaseGroup(testCtx, "second", ""));

	tcu::StringTemplate shaderHeader(
		createShaderHeader("%input_data_0_untyped_var %input_data_1_untyped_var %output_data_var")
	);

	tcu::StringTemplate shaderAnnotations(
		createShaderAnnotations(PointerTestCases::OP_SELECT_PHYSICAL_STORAGE)
	);

	tcu::StringTemplate shaderVariables(
		createShaderVariables(PointerTestCases::OP_SELECT_PHYSICAL_STORAGE)
	);

	tcu::StringTemplate shaderFunctions(
		createShaderMain(PointerTestCases::OP_SELECT_PHYSICAL_STORAGE)
	);

	for (deUint32 i = 0; i < DE_LENGTH_OF_ARRAY(BASE_DATA_TYPE_CASES); ++i)
	{
		for (deUint32 j = 0; j < 2; ++j)
		{
			std::string testName	= toString(BASE_DATA_TYPE_CASES[i]);

			std::map<std::string, std::string>	specMap;
			if (j)
			{
				specMap["boolConst"]	=  "%c_bool_true = OpConstantTrue %bool";
				specMap["condition"]	=  "%c_bool_true";
			}
			else
			{
				specMap["boolConst"]	=  "%c_bool_false = OpConstantFalse %bool";
				specMap["condition"]	=  "%c_bool_false";
			}

			specMap["baseDecl"]		= getDeclaration(BASE_DATA_TYPE_CASES[i]);
			specMap["baseType"]		= toString(BASE_DATA_TYPE_CASES[i]);
			specMap["alignment"]	= std::to_string(getSizeInBytes(BASE_DATA_TYPE_CASES[i]));

			ComputeShaderSpec spec;
			adjustSpecForUntypedPointers(spec, specMap);
			adjustSpecForMemoryModel(spec, specMap, memModel);
			adjustSpecForDataTypes(spec, specMap, BASE_DATA_TYPE_CASES[i]);
			adjustSpecForPhysicalStorageBuffer(spec, specMap, memModel);

			const tcu::StringTemplate tempShaderFunctions = tcu::StringTemplate(shaderFunctions.specialize(specMap));

			std::string shaderVariablesStr	= shaderVariables.specialize(specMap);
			if (BASE_DATA_TYPE_CASES[i] != DataTypes::UINT32)
			{
				shaderVariablesStr	= "%uint32     = OpTypeInt  32      0\n"
									  "%c_uint32_1 = OpConstant %uint32 1\n"
									+ shaderVariablesStr;
			}

			const std::string shaderAsm =
				shaderHeader.specialize(specMap) +
				shaderAnnotations.specialize(specMap) +
				shaderVariablesStr +
				tempShaderFunctions.specialize(specMap);

			FilledResourceDesc desc;
			desc.dataType		= BASE_DATA_TYPE_CASES[i];
			desc.elemCount		= 1;
			desc.descriptorType	= vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			desc.padding		= 0;
			desc.fillType		= FillingTypes::VALUE;
			desc.value			= 1;
			Resource input0		= createFilledResource(desc);
			desc.value			= 0;
			Resource input1		= createFilledResource(desc);

			if (j)
			{
				desc.fillType	= FillingTypes::VALUE;
				desc.value		= 1.0;
				Resource output	= createFilledResource(desc);
				spec.outputs.push_back(output);
			}
			else
			{
				desc.fillType	= FillingTypes::VALUE;
				desc.value		= 0.0;
				Resource output = createFilledResource(desc);
				spec.outputs.push_back(output);
			}

			spec.assembly		= shaderAsm;
			spec.numWorkGroups	= tcu::IVec3(1, 1, 1);
			spec.spirvVersion	= SPIRV_VERSION_1_4;
			spec.inputs.push_back(input0);
			spec.inputs.push_back(input1);
			spec.extensions.push_back("VK_KHR_storage_buffer_storage_class");
			spec.extensions.push_back("VK_KHR_shader_untyped_pointers");

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

void addVariablePtrOpPhiTests(tcu::TestCaseGroup* testGroup, MEMORY_MODEL_TYPE memModel)
{
	tcu::TestContext& testCtx = testGroup->getTestContext();

	de::MovePtr<tcu::TestCaseGroup>	firstGroup(new tcu::TestCaseGroup(testCtx, "first", ""));
	de::MovePtr<tcu::TestCaseGroup>	secondGroup(new tcu::TestCaseGroup(testCtx, "second", ""));

	tcu::StringTemplate shaderHeader(
		createShaderHeader("%input_data_0_untyped_var %input_data_1_untyped_var %output_data_var")
	);

	tcu::StringTemplate shaderAnnotations(
		createShaderAnnotations(PointerTestCases::OP_PHI_VARIABLE_PTR)
	);

	tcu::StringTemplate shaderVariables(
		createShaderVariables(PointerTestCases::OP_PHI_VARIABLE_PTR)
	);

	tcu::StringTemplate shaderFunctions(
		createShaderMain(PointerTestCases::OP_PHI_VARIABLE_PTR)
	);

	for (deUint32 i = 0; i < DE_LENGTH_OF_ARRAY(BASE_DATA_TYPE_CASES); ++i)
	{
		for (deUint32 j = 0; j < 2; ++j)
		{
			std::string testName	= toString(BASE_DATA_TYPE_CASES[i]);

			std::map<std::string, std::string>	specMap;
			if (j)
			{
				specMap["boolConst"]	=  "%c_bool_true = OpConstantTrue %bool";
				specMap["condition"]	=  "%c_bool_true";
			}
			else
			{
				specMap["boolConst"]	=  "%c_bool_false = OpConstantFalse %bool";
				specMap["condition"]	=  "%c_bool_false";
			}

			specMap["baseDecl"]		= getDeclaration(BASE_DATA_TYPE_CASES[i]);
			specMap["baseType"]		= toString(BASE_DATA_TYPE_CASES[i]);
			specMap["alignment"]	= std::to_string(getSizeInBytes(BASE_DATA_TYPE_CASES[i]));

			ComputeShaderSpec spec;
			adjustSpecForUntypedPointers(spec, specMap);
			adjustSpecForMemoryModel(spec, specMap, memModel);
			adjustSpecForDataTypes(spec, specMap, BASE_DATA_TYPE_CASES[i]);
			adjustSpecForVariablePointers(spec, specMap);

			const tcu::StringTemplate tempShaderFunctions = tcu::StringTemplate(shaderFunctions.specialize(specMap));

			std::string shaderVariablesStr = shaderVariables.specialize(specMap);
			if (BASE_DATA_TYPE_CASES[i] != DataTypes::UINT32)
			{
				shaderVariablesStr	= "%uint32     = OpTypeInt  32      0\n"
									  "%c_uint32_1 = OpConstant %uint32 1\n"
									+ shaderVariablesStr;
			}

			const std::string shaderAsm =
				shaderHeader.specialize(specMap) +
				shaderAnnotations.specialize(specMap) +
				shaderVariablesStr +
				tempShaderFunctions.specialize(specMap);

			FilledResourceDesc desc;
			desc.dataType		= BASE_DATA_TYPE_CASES[i];
			desc.elemCount		= 1;
			desc.descriptorType	= vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			desc.padding		= 0;
			desc.fillType		= FillingTypes::VALUE;
			desc.value			= 1;
			Resource input0		= createFilledResource(desc);
			desc.value			= 0;
			Resource input1		= createFilledResource(desc);

			if (j)
			{
				desc.fillType = FillingTypes::VALUE;
				desc.value = 1.0;
				Resource output = createFilledResource(desc);
				spec.outputs.push_back(output);
			}
			else
			{
				desc.fillType = FillingTypes::VALUE;
				desc.value = 0.0;
				Resource output = createFilledResource(desc);
				spec.outputs.push_back(output);
			}

			spec.assembly		= shaderAsm;
			spec.numWorkGroups	= tcu::IVec3(1, 1, 1);
			spec.spirvVersion	= SPIRV_VERSION_1_4;	// After spir-v version 1.6 OpBranchConditional labels nust not be the same.
			spec.inputs.push_back(input0);
			spec.inputs.push_back(input1);
			spec.extensions.push_back("VK_KHR_storage_buffer_storage_class");
			spec.extensions.push_back("VK_KHR_shader_untyped_pointers");

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

void addPhysicalStorageOpPhiTests(tcu::TestCaseGroup* testGroup, MEMORY_MODEL_TYPE memModel)
{
	tcu::TestContext& testCtx = testGroup->getTestContext();

	de::MovePtr<tcu::TestCaseGroup>	firstGroup(new tcu::TestCaseGroup(testCtx, "first", ""));
	de::MovePtr<tcu::TestCaseGroup>	secondGroup(new tcu::TestCaseGroup(testCtx, "second", ""));

	tcu::StringTemplate shaderHeader(
		createShaderHeader("%input_data_0_untyped_var %input_data_1_untyped_var %output_data_var")
	);

	tcu::StringTemplate shaderAnnotations(
		createShaderAnnotations(PointerTestCases::OP_PHI_PHYSICAL_STORAGE)
	);

	tcu::StringTemplate shaderVariables(
		createShaderVariables(PointerTestCases::OP_PHI_PHYSICAL_STORAGE)
	);

	tcu::StringTemplate shaderFunctions(
		createShaderMain(PointerTestCases::OP_PHI_PHYSICAL_STORAGE)
	);

	for (deUint32 i = 0; i < DE_LENGTH_OF_ARRAY(BASE_DATA_TYPE_CASES); ++i)
	{
		for (deUint32 j = 0; j < 2; ++j)
		{
			std::string testName	= toString(BASE_DATA_TYPE_CASES[i]);

			std::map<std::string, std::string>	specMap;
			if (j)
			{
				specMap["boolConst"]	=  "%c_bool_true = OpConstantTrue %bool";
				specMap["condition"]	=  "%c_bool_true";
			}
			else
			{
				specMap["boolConst"]	=  "%c_bool_false = OpConstantFalse %bool";
				specMap["condition"]	=  "%c_bool_false";
			}

			specMap["baseDecl"]		= getDeclaration(BASE_DATA_TYPE_CASES[i]);
			specMap["baseType"]		= toString(BASE_DATA_TYPE_CASES[i]);
			specMap["alignment"]	= std::to_string(getSizeInBytes(BASE_DATA_TYPE_CASES[i]));

			ComputeShaderSpec spec;
			adjustSpecForUntypedPointers(spec, specMap);
			adjustSpecForMemoryModel(spec, specMap, memModel);
			adjustSpecForDataTypes(spec, specMap, BASE_DATA_TYPE_CASES[i]);
			adjustSpecForPhysicalStorageBuffer(spec, specMap, memModel);

			const tcu::StringTemplate tempShaderFunctions = tcu::StringTemplate(shaderFunctions.specialize(specMap));

			std::string shaderVariablesStr = shaderVariables.specialize(specMap);
			if (BASE_DATA_TYPE_CASES[i] != DataTypes::UINT32)
			{
				shaderVariablesStr	= "%uint32     = OpTypeInt  32      0\n"
									  "%c_uint32_1 = OpConstant %uint32 1\n"
									+ shaderVariablesStr;
			}

			const std::string shaderAsm =
				shaderHeader.specialize(specMap) +
				shaderAnnotations.specialize(specMap) +
				shaderVariablesStr +
				tempShaderFunctions.specialize(specMap);

			FilledResourceDesc desc;
			desc.dataType		= BASE_DATA_TYPE_CASES[i];
			desc.elemCount		= 1;
			desc.descriptorType	= vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			desc.padding		= 0;
			desc.fillType		= FillingTypes::VALUE;
			desc.value			= 1;
			Resource input0		= createFilledResource(desc);
			desc.value			= 0;
			Resource input1		= createFilledResource(desc);

			if (j)
			{
				desc.fillType	= FillingTypes::VALUE;
				desc.value		= 1.0;
				Resource output	= createFilledResource(desc);
				spec.outputs.push_back(output);
			}
			else
			{
				desc.fillType	= FillingTypes::VALUE;
				desc.value		= 0.0;
				Resource output = createFilledResource(desc);
				spec.outputs.push_back(output);
			}

			spec.assembly		= shaderAsm;
			spec.numWorkGroups	= tcu::IVec3(1, 1, 1);
			spec.spirvVersion	= SPIRV_VERSION_1_4;	// After spir-v version 1.6 OpBranchConditional labels nust not be the same.
			spec.inputs.push_back(input0);
			spec.inputs.push_back(input1);
			spec.extensions.push_back("VK_KHR_storage_buffer_storage_class");
			spec.extensions.push_back("VK_KHR_shader_untyped_pointers");

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

void addVariablePtrOpPtrEqualTests(tcu::TestCaseGroup* testGroup, MEMORY_MODEL_TYPE memModel)
{
	tcu::TestContext& testCtx = testGroup->getTestContext();

	de::MovePtr<tcu::TestCaseGroup>	equalGroup(new tcu::TestCaseGroup(testCtx, "equal", ""));
	de::MovePtr<tcu::TestCaseGroup>	notEqualGroup(new tcu::TestCaseGroup(testCtx, "not_equal", ""));

	tcu::StringTemplate shaderHeader(
		createShaderHeader("%input_data_0_untyped_var %input_data_1_untyped_var %output_data_var")
	);

	tcu::StringTemplate shaderAnnotations(
		createShaderAnnotations(PointerTestCases::OP_PTR_EQUAL_VARIABLE_PTR)
	);

	tcu::StringTemplate shaderVariables(
		createShaderVariables(PointerTestCases::OP_PTR_EQUAL_VARIABLE_PTR)
	);

	tcu::StringTemplate shaderFunctions(
		createShaderMain(PointerTestCases::OP_PTR_EQUAL_VARIABLE_PTR)
	);

	for (deUint32 i = 0; i < DE_LENGTH_OF_ARRAY(BASE_DATA_TYPE_CASES); ++i)
	{
		for (deUint32 j = 0; j < 2; ++j)
		{
			std::string testName	= toString(BASE_DATA_TYPE_CASES[i]);

			std::map<std::string, std::string>	specMap;
			specMap["baseDecl"]		= getDeclaration(BASE_DATA_TYPE_CASES[i]);
			specMap["baseType"]		= toString(BASE_DATA_TYPE_CASES[i]);

			ComputeShaderSpec spec;
			adjustSpecForUntypedPointers(spec, specMap);
			adjustSpecForMemoryModel(spec, specMap, memModel);
			adjustSpecForDataTypes(spec, specMap, BASE_DATA_TYPE_CASES[i]);
			adjustSpecForVariablePointers(spec, specMap);

			const tcu::StringTemplate tempShaderFunctions	= tcu::StringTemplate(shaderFunctions.specialize(specMap));

			std::string shaderVariablesStr	= shaderVariables.specialize(specMap);
			if (BASE_DATA_TYPE_CASES[i] != DataTypes::UINT32)
			{
				shaderVariablesStr	= "%uint32     = OpTypeInt  32      0\n"
									  "%c_uint32_1 = OpConstant %uint32 1\n"
									+ shaderVariablesStr;
			}

			const std::string shaderAsm =
				shaderHeader.specialize(specMap) +
				shaderAnnotations.specialize(specMap) +
				shaderVariablesStr +
				tempShaderFunctions.specialize(specMap);

			FilledResourceDesc desc;
			desc.dataType		= BASE_DATA_TYPE_CASES[i];
			desc.elemCount		= 1;
			desc.descriptorType	= vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			desc.padding		= 0;
			desc.fillType		= FillingTypes::VALUE;
			desc.value			= 1.0;

			if (j)
			{
				Resource input0	= createFilledResource(desc);
				Resource input1	= createFilledResource(desc);
				Resource output	= createFilledResource(desc);
				spec.inputs.push_back(input0);
				spec.inputs.push_back(input1);
				spec.outputs.push_back(output);
			}
			else
			{
				Resource input0	= createFilledResource(desc);
				desc.value		= 0.0;
				Resource input1	= createFilledResource(desc);
				Resource output	= createFilledResource(desc);
				spec.inputs.push_back(input0);
				spec.inputs.push_back(input1);
				spec.outputs.push_back(output);
			}

			spec.assembly		= shaderAsm;
			spec.numWorkGroups	= tcu::IVec3(1, 1, 1);
			spec.spirvVersion	= SPIRV_VERSION_1_4;	// OpPtrEqual, OpPtrNotEqual and OpPtrDiff requires SPIR-V 1.4
			spec.extensions.push_back("VK_KHR_storage_buffer_storage_class");
			spec.extensions.push_back("VK_KHR_shader_untyped_pointers");

			if (j)
			{
				equalGroup->addChild(new SpvAsmComputeShaderCase(testCtx, testName.c_str(), spec));
			}
			else
			{
				notEqualGroup->addChild(new SpvAsmComputeShaderCase(testCtx, testName.c_str(), spec));
			}
		}
	}

	testGroup->addChild(equalGroup.release());
	testGroup->addChild(notEqualGroup.release());
}

void addVariablePtrOpPtrNotEqualTests(tcu::TestCaseGroup* testGroup, MEMORY_MODEL_TYPE memModel)
{
	tcu::TestContext& testCtx	= testGroup->getTestContext();

	de::MovePtr<tcu::TestCaseGroup>	equalGroup(new tcu::TestCaseGroup(testCtx, "equal", ""));
	de::MovePtr<tcu::TestCaseGroup>	notEqualGroup(new tcu::TestCaseGroup(testCtx, "not_equal", ""));

	tcu::StringTemplate shaderHeader(
		createShaderHeader("%input_data_0_untyped_var %input_data_1_untyped_var %output_data_var")
	);

	tcu::StringTemplate shaderAnnotations(
		createShaderAnnotations(PointerTestCases::OP_PTR_NOT_EQUAL_VARIABLE_PTR)
	);

	tcu::StringTemplate shaderVariables(
		createShaderVariables(PointerTestCases::OP_PTR_NOT_EQUAL_VARIABLE_PTR)
	);

	tcu::StringTemplate shaderFunctions(
		createShaderMain(PointerTestCases::OP_PTR_NOT_EQUAL_VARIABLE_PTR)
	);

	for (deUint32 i = 0; i < DE_LENGTH_OF_ARRAY(BASE_DATA_TYPE_CASES); ++i)
	{
		for (deUint32 j = 0; j < 2; ++j)
		{
			std::string testName	= toString(BASE_DATA_TYPE_CASES[i]);

			std::map<std::string, std::string>	specMap;
			specMap["baseDecl"]	= getDeclaration(BASE_DATA_TYPE_CASES[i]);
			specMap["baseType"]	= toString(BASE_DATA_TYPE_CASES[i]);

			ComputeShaderSpec spec;
			adjustSpecForUntypedPointers(spec, specMap);
			adjustSpecForMemoryModel(spec, specMap, memModel);
			adjustSpecForDataTypes(spec, specMap, BASE_DATA_TYPE_CASES[i]);
			adjustSpecForVariablePointers(spec, specMap);

			const tcu::StringTemplate tempShaderFunctions	= tcu::StringTemplate(shaderFunctions.specialize(specMap));

			std::string shaderVariablesStr = shaderVariables.specialize(specMap);
			if (BASE_DATA_TYPE_CASES[i] != DataTypes::UINT32)
			{
				shaderVariablesStr	= "%uint32 = OpTypeInt 32 0\n"
									  "%c_uint32_1 = OpConstant %uint32 1\n"
									+ shaderVariablesStr;
			}

			const std::string shaderAsm =
				shaderHeader.specialize(specMap) +
				shaderAnnotations.specialize(specMap) +
				shaderVariablesStr +
				tempShaderFunctions.specialize(specMap);

			FilledResourceDesc desc;
			desc.dataType		= BASE_DATA_TYPE_CASES[i];
			desc.elemCount		= 1;
			desc.descriptorType	= vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			desc.padding		= 0;
			desc.fillType		= FillingTypes::VALUE;
			desc.value			= 1.0;

			if (j)
			{
				Resource input0	= createFilledResource(desc);
				Resource input1	= createFilledResource(desc);
				Resource output	= createFilledResource(desc);
				spec.inputs.push_back(input0);
				spec.inputs.push_back(input1);
				spec.outputs.push_back(output);
			}
			else
			{
				Resource input1	= createFilledResource(desc);
				desc.value		= 0.0;
				Resource input0	= createFilledResource(desc);
				Resource output	= createFilledResource(desc);
				spec.inputs.push_back(input0);
				spec.inputs.push_back(input1);
				spec.outputs.push_back(output);
			}

			spec.assembly		= shaderAsm;
			spec.numWorkGroups	= tcu::IVec3(1, 1, 1);
			spec.spirvVersion	= SPIRV_VERSION_1_4;	// OpPtrEqual, OpPtrNotEqual and OpPtrDiff requires SPIR-V 1.4
			spec.extensions.push_back("VK_KHR_storage_buffer_storage_class");
			spec.extensions.push_back("VK_KHR_shader_untyped_pointers");

			if (j)
			{
				equalGroup->addChild(new SpvAsmComputeShaderCase(testCtx, testName.c_str(), spec));
			}
			else
			{
				notEqualGroup->addChild(new SpvAsmComputeShaderCase(testCtx, testName.c_str(), spec));
			}
		}
	}

	testGroup->addChild(equalGroup.release());
	testGroup->addChild(notEqualGroup.release());
}

void addVariablePtrOpPtrDiffTests(tcu::TestCaseGroup* testGroup, MEMORY_MODEL_TYPE memModel)
{
	tcu::TestContext& testCtx = testGroup->getTestContext();

	tcu::StringTemplate shaderHeader(
		createShaderHeader("%input_data_untyped_var %output_data_var")
	);

	tcu::StringTemplate shaderAnnotations(
		createShaderAnnotations(PointerTestCases::OP_PTR_DIFF_VARIABLE_PTR)
	);

	tcu::StringTemplate shaderVariables(
		createShaderVariables(PointerTestCases::OP_PTR_DIFF_VARIABLE_PTR)
	);

	tcu::StringTemplate shaderFunctions(
		createShaderMain(PointerTestCases::OP_PTR_DIFF_VARIABLE_PTR)
	);

	for (deUint32 i = 0; i < DE_LENGTH_OF_ARRAY(BASE_DATA_TYPE_CASES); ++i)
	{
		std::string testName	= toString(BASE_DATA_TYPE_CASES[i]);

		std::map<std::string, std::string>	specMap;
		specMap["baseDecl"]		= getDeclaration(BASE_DATA_TYPE_CASES[i]);
		specMap["baseType"]		= toString(BASE_DATA_TYPE_CASES[i]);
		specMap["threadCount"]	= std::to_string(Constants::numThreads);
		specMap["alignment"]	= std::to_string(getSizeInBytes(BASE_DATA_TYPE_CASES[i]));

		ComputeShaderSpec spec;
		adjustSpecForUntypedPointers(spec, specMap);
		adjustSpecForMemoryModel(spec, specMap, memModel);
		adjustSpecForDataTypes(spec, specMap, BASE_DATA_TYPE_CASES[i]);
		adjustSpecForVariablePointers(spec, specMap);

		const tcu::StringTemplate tempShaderFunctions = tcu::StringTemplate(shaderFunctions.specialize(specMap));

		std::string shaderVariablesStr = shaderVariables.specialize(specMap);
		if (BASE_DATA_TYPE_CASES[i] != DataTypes::UINT32)
		{
			shaderVariablesStr	= "%uint32     = OpTypeInt  32      0\n"
								  "%c_uint32_1 = OpConstant %uint32 1\n"
								+ shaderVariablesStr;
		}

		const std::string shaderAsm =
			shaderHeader.specialize(specMap) +
			shaderAnnotations.specialize(specMap) +
			shaderVariablesStr +
			tempShaderFunctions.specialize(specMap);

		FilledResourceDesc desc;
		desc.dataType		= BASE_DATA_TYPE_CASES[i];
		desc.elemCount		= Constants::numThreads;
		desc.descriptorType	= vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		desc.padding		= 0;
		desc.fillType		= FillingTypes::RANDOM;
		desc.seed			= deStringHash(testGroup->getName());

		Resource input		= createFilledResource(desc);
		desc.dataType		= DataTypes::UINT32;
		desc.elemCount		= 1;
		desc.fillType		= FillingTypes::VALUE;
		desc.value			= 16;
		Resource output = createFilledResource(desc);

		spec.assembly		= shaderAsm;
		spec.numWorkGroups	= tcu::IVec3(1, 1, 1);
		spec.spirvVersion	= SPIRV_VERSION_1_4;
		spec.inputs.push_back(input);
		spec.outputs.push_back(output);
		spec.extensions.push_back("VK_KHR_storage_buffer_storage_class");
		spec.extensions.push_back("VK_KHR_shader_untyped_pointers");

		testGroup->addChild(new SpvAsmComputeShaderCase(testCtx, testName.c_str(), spec));
	}
}

void addVariablePtrOpFunctionCallTests(tcu::TestCaseGroup* testGroup, MEMORY_MODEL_TYPE memModel)
{
	tcu::TestContext& testCtx = testGroup->getTestContext();

	tcu::StringTemplate shaderHeader(
		createShaderHeader()
	);

	tcu::StringTemplate shaderAnnotations(
		createShaderAnnotations(PointerTestCases::OP_FUNCTION_CALL_VARIABLE_PTR)
	);

	tcu::StringTemplate shaderVariables(
		createShaderVariables(PointerTestCases::OP_FUNCTION_CALL_VARIABLE_PTR)
	);

	std::string	functions	 = createSimpleFunction(PointerTestCases::OP_FUNCTION_CALL_VARIABLE_PTR);
				functions	+= createShaderMain(PointerTestCases::OP_FUNCTION_CALL_VARIABLE_PTR);

	tcu::StringTemplate shaderFunctions(
		functions
	);

	for (deUint32 i = 0; i < DE_LENGTH_OF_ARRAY(BASE_DATA_TYPE_CASES); ++i)
	{
		std::string testName	= toString(BASE_DATA_TYPE_CASES[i]);

		std::map<std::string, std::string>	specMap;
		specMap["baseDecl"]	= getDeclaration(BASE_DATA_TYPE_CASES[i]);
		specMap["baseType"]	= toString(BASE_DATA_TYPE_CASES[i]);

		ComputeShaderSpec spec;
		adjustSpecForUntypedPointers(spec, specMap);
		adjustSpecForMemoryModel(spec, specMap, memModel);
		adjustSpecForDataTypes(spec, specMap, BASE_DATA_TYPE_CASES[i]);
		adjustSpecForVariablePointers(spec, specMap);

		const tcu::StringTemplate tempShaderFunctions = tcu::StringTemplate(shaderFunctions.specialize(specMap));

		std::string shaderVariablesStr = shaderVariables.specialize(specMap);
		if (BASE_DATA_TYPE_CASES[i] != DataTypes::UINT32)
		{
			shaderVariablesStr	= "%uint32     = OpTypeInt  32      0\n"
								  "%c_uint32_1 = OpConstant %uint32 1\n"
								+ shaderVariablesStr;
		}

		const std::string shaderAsm =
			shaderHeader.specialize(specMap) +
			shaderAnnotations.specialize(specMap) +
			shaderVariablesStr +
			tempShaderFunctions.specialize(specMap);

		FilledResourceDesc desc;
		desc.dataType		= BASE_DATA_TYPE_CASES[i];
		desc.elemCount		= 1;
		desc.descriptorType	= vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		desc.padding		= 0;
		desc.fillType		= FillingTypes::VALUE;
		desc.value			= 1.0;

		Resource input		= createFilledResource(desc);
		Resource output		= createFilledResource(desc);

		spec.assembly = shaderAsm;
		spec.numWorkGroups = tcu::IVec3(1, 1, 1);
		spec.inputs.push_back(input);
		spec.outputs.push_back(output);
		spec.extensions.push_back("VK_KHR_storage_buffer_storage_class");
		spec.extensions.push_back("VK_KHR_shader_untyped_pointers");

		testGroup->addChild(new SpvAsmComputeShaderCase(testCtx, testName.c_str(), spec));
	}
}

void addPhysicalStorageOpFunctionCallTests(tcu::TestCaseGroup* testGroup, MEMORY_MODEL_TYPE memModel)
{
	tcu::TestContext& testCtx = testGroup->getTestContext();

	tcu::StringTemplate shaderHeader(
		createShaderHeader()
	);

	tcu::StringTemplate shaderAnnotations(
		createShaderAnnotations(PointerTestCases::OP_FUNCTION_CALL_PHYSICAL_STORAGE)
	);

	tcu::StringTemplate shaderVariables(
		createShaderVariables(PointerTestCases::OP_FUNCTION_CALL_PHYSICAL_STORAGE)
	);

	std::string	functions	=  createSimpleFunction(PointerTestCases::OP_FUNCTION_CALL_PHYSICAL_STORAGE);
				functions	+= createShaderMain(PointerTestCases::OP_FUNCTION_CALL_PHYSICAL_STORAGE);

	tcu::StringTemplate	shaderFunctions(
		functions
	);

	for (deUint32 i = 0; i < DE_LENGTH_OF_ARRAY(BASE_DATA_TYPE_CASES); ++i)
	{
		std::string testName	= toString(BASE_DATA_TYPE_CASES[i]);

		std::map<std::string, std::string>	specMap;
		specMap["baseDecl"]		= getDeclaration(BASE_DATA_TYPE_CASES[i]);
		specMap["baseType"]		= toString(BASE_DATA_TYPE_CASES[i]);
		specMap["alignment"]	= std::to_string(getSizeInBytes(BASE_DATA_TYPE_CASES[i]));

		ComputeShaderSpec	spec;
		adjustSpecForUntypedPointers(spec, specMap);
		adjustSpecForMemoryModel(spec, specMap, memModel);
		adjustSpecForDataTypes(spec, specMap, BASE_DATA_TYPE_CASES[i]);
		adjustSpecForPhysicalStorageBuffer(spec, specMap, memModel);

		const tcu::StringTemplate	tempShaderFunctions = tcu::StringTemplate(shaderFunctions.specialize(specMap));

		std::string shaderVariablesStr	= shaderVariables.specialize(specMap);
		if (BASE_DATA_TYPE_CASES[i] != DataTypes::UINT32)
		{
			shaderVariablesStr	= "%uint32     = OpTypeInt  32      0\n"
								  "%c_uint32_1 = OpConstant %uint32 1\n"
								+ shaderVariablesStr;
		}

		const std::string shaderAsm =
			shaderHeader.specialize(specMap) +
			shaderAnnotations.specialize(specMap) +
			shaderVariablesStr +
			tempShaderFunctions.specialize(specMap);

		FilledResourceDesc desc;
		desc.dataType		= BASE_DATA_TYPE_CASES[i];
		desc.elemCount		= 1;
		desc.descriptorType	= vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		desc.padding		= 0;
		desc.fillType		= FillingTypes::VALUE;
		desc.value			= 1.0;

		Resource input	= createFilledResource(desc);
		Resource output	= createFilledResource(desc);

		spec.assembly		= shaderAsm;
		spec.numWorkGroups	= tcu::IVec3(1, 1, 1);
		spec.inputs.push_back(input);
		spec.outputs.push_back(output);
		spec.extensions.push_back("VK_KHR_storage_buffer_storage_class");
		spec.extensions.push_back("VK_KHR_shader_untyped_pointers");

		testGroup->addChild(new SpvAsmComputeShaderCase(testCtx, testName.c_str(), spec));
	}
}

void addVariablePtrOpPtrAccessChain(tcu::TestCaseGroup* testGroup, MEMORY_MODEL_TYPE memModel)
{
	tcu::TestContext& testCtx = testGroup->getTestContext();

	tcu::StringTemplate shaderHeader(
		createShaderHeader()
	);

	tcu::StringTemplate shaderAnnotations(
		createShaderAnnotations(PointerTestCases::OP_PTR_ACCESS_CHAIN_VARIABLE_PTR)
	);

	tcu::StringTemplate shaderVariables(
		createShaderVariables(PointerTestCases::OP_PTR_ACCESS_CHAIN_VARIABLE_PTR)
	);

	tcu::StringTemplate shaderFunctions(
		createShaderMain(PointerTestCases::OP_PTR_ACCESS_CHAIN_VARIABLE_PTR)
	);

	for (deUint32 i = 0; i < DE_LENGTH_OF_ARRAY(BASE_DATA_TYPE_CASES); ++i)
	{
		std::string testName	= toString(BASE_DATA_TYPE_CASES[i]);

		std::map<std::string, std::string>	specMap;
		specMap["baseDecl"]		= getDeclaration(BASE_DATA_TYPE_CASES[i]);
		specMap["baseType"]		= toString(BASE_DATA_TYPE_CASES[i]);
		specMap["threadCount"]	= std::to_string(Constants::numThreads);
		specMap["alignment"]	= std::to_string(getSizeInBytes(BASE_DATA_TYPE_CASES[i]));

		ComputeShaderSpec spec;
		adjustSpecForUntypedPointers(spec, specMap);
		adjustSpecForMemoryModel(spec, specMap, memModel);
		adjustSpecForDataTypes(spec, specMap, BASE_DATA_TYPE_CASES[i]);
		adjustSpecForVariablePointers(spec, specMap);

		const tcu::StringTemplate tempShaderFunctions = tcu::StringTemplate(shaderFunctions.specialize(specMap));

		std::string shaderVariablesStr = shaderVariables.specialize(specMap);
		if (BASE_DATA_TYPE_CASES[i] != DataTypes::UINT32)
		{
			shaderVariablesStr	= "%uint32     = OpTypeInt  32      0\n"
								  "%c_uint32_1 = OpConstant %uint32 1\n"
								+ shaderVariablesStr;
		}

		const std::string shaderAsm =
			shaderHeader.specialize(specMap) +
			shaderAnnotations.specialize(specMap) +
			shaderVariablesStr +
			tempShaderFunctions.specialize(specMap);

		FilledResourceDesc desc;
		desc.dataType		= BASE_DATA_TYPE_CASES[i];
		desc.elemCount		= Constants::numThreads;
		desc.descriptorType	= vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		desc.padding		= 0;
		desc.fillType		= FillingTypes::RANDOM;
		desc.seed			= deStringHash(testGroup->getName());

		Resource input	= createFilledResource(desc);
		Resource output	= createFilledResource(desc);

		spec.assembly		= shaderAsm;
		spec.numWorkGroups	= tcu::IVec3(Constants::numThreads, 1, 1);
		spec.inputs.push_back(input);
		spec.outputs.push_back(output);
		spec.extensions.push_back("VK_KHR_storage_buffer_storage_class");
		spec.extensions.push_back("VK_KHR_shader_untyped_pointers");

		testGroup->addChild(new SpvAsmComputeShaderCase(testCtx, testName.c_str(), spec));
	}
}

void addPhysicalStorageOpPtrAccessChainTests(tcu::TestCaseGroup* testGroup, MEMORY_MODEL_TYPE memModel)
{
	tcu::TestContext& testCtx = testGroup->getTestContext();

	tcu::StringTemplate shaderHeader(
		createShaderHeader()
	);

	tcu::StringTemplate shaderAnnotations(
		createShaderAnnotations(PointerTestCases::OP_PTR_ACCESS_CHAIN_PHYSICAL_STORAGE)
	);

	tcu::StringTemplate shaderVariables(
		createShaderVariables(PointerTestCases::OP_PTR_ACCESS_CHAIN_PHYSICAL_STORAGE)
	);

	tcu::StringTemplate shaderFunctions(
		createShaderMain(PointerTestCases::OP_PTR_ACCESS_CHAIN_PHYSICAL_STORAGE)
	);

	for (deUint32 i = 0; i < DE_LENGTH_OF_ARRAY(BASE_DATA_TYPE_CASES); ++i)
	{
		std::string testName	= toString(BASE_DATA_TYPE_CASES[i]);

		std::map<std::string, std::string>	specMap;
		specMap["baseDecl"]		= getDeclaration(BASE_DATA_TYPE_CASES[i]);
		specMap["baseType"]		= toString(BASE_DATA_TYPE_CASES[i]);
		specMap["threadCount"]	= std::to_string(Constants::numThreads);
		specMap["alignment"]	= std::to_string(getSizeInBytes(BASE_DATA_TYPE_CASES[i]));

		ComputeShaderSpec spec;
		adjustSpecForUntypedPointers(spec, specMap);
		adjustSpecForMemoryModel(spec, specMap, memModel);
		adjustSpecForDataTypes(spec, specMap, BASE_DATA_TYPE_CASES[i]);
		adjustSpecForPhysicalStorageBuffer(spec, specMap, memModel);

		const tcu::StringTemplate tempShaderFunctions = tcu::StringTemplate(shaderFunctions.specialize(specMap));

		std::string shaderVariablesStr = shaderVariables.specialize(specMap);
		if (BASE_DATA_TYPE_CASES[i] != DataTypes::UINT32)
		{
			shaderVariablesStr	= "%uint32     = OpTypeInt  32      0\n"
								  "%c_uint32_1 = OpConstant %uint32 1\n"
								+ shaderVariablesStr;
		}

		const std::string shaderAsm =
			shaderHeader.specialize(specMap) +
			shaderAnnotations.specialize(specMap) +
			shaderVariablesStr +
			tempShaderFunctions.specialize(specMap);

		FilledResourceDesc desc;
		desc.dataType		= BASE_DATA_TYPE_CASES[i];
		desc.elemCount		= Constants::numThreads;
		desc.descriptorType	= vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		desc.padding		= 0;
		desc.fillType		= FillingTypes::RANDOM;
		desc.seed			= deStringHash(testGroup->getName());

		Resource input	= createFilledResource(desc);
		Resource output	= createFilledResource(desc);

		spec.assembly		= shaderAsm;
		spec.numWorkGroups	= tcu::IVec3(Constants::numThreads, 1, 1);
		spec.inputs.push_back(input);
		spec.outputs.push_back(output);
		spec.extensions.push_back("VK_KHR_storage_buffer_storage_class");
		spec.extensions.push_back("VK_KHR_shader_untyped_pointers");

		testGroup->addChild(new SpvAsmComputeShaderCase(testCtx, testName.c_str(), spec));
	}
}

void addVariablePtrFunctionVariableTests(tcu::TestCaseGroup* testGroup, MEMORY_MODEL_TYPE memModel)
{
	tcu::TestContext& testCtx	= testGroup->getTestContext();

	tcu::StringTemplate shaderHeader(
		createShaderHeader()
	);

	tcu::StringTemplate shaderAnnotations(
		createShaderAnnotations(PointerTestCases::FUNCTION_VARIABLE_VARIABLE_PTR)
	);

	tcu::StringTemplate shaderVariables(
		createShaderVariables(PointerTestCases::FUNCTION_VARIABLE_VARIABLE_PTR)
	);

	tcu::StringTemplate shaderFunctions(
		createShaderMain(PointerTestCases::FUNCTION_VARIABLE_VARIABLE_PTR)
	);

	for (deUint32 i = 0; i < DE_LENGTH_OF_ARRAY(BASE_DATA_TYPE_CASES); ++i)
	{
		std::string testName	= toString(BASE_DATA_TYPE_CASES[i]);

		std::map<std::string, std::string>	specMap;
		specMap["baseDecl"]		= getDeclaration(BASE_DATA_TYPE_CASES[i]);
		specMap["baseType"]		= toString(BASE_DATA_TYPE_CASES[i]);
		specMap["boolConst"]	= "%c_bool_true = OpConstantTrue %bool";
		specMap["condition"]	= "%c_bool_true";

		ComputeShaderSpec spec;
		adjustSpecForUntypedPointers(spec, specMap);
		adjustSpecForMemoryModel(spec, specMap, memModel);
		adjustSpecForDataTypes(spec, specMap, BASE_DATA_TYPE_CASES[i]);
		adjustSpecForVariablePointers(spec, specMap);

		const tcu::StringTemplate tempShaderFunctions = tcu::StringTemplate(shaderFunctions.specialize(specMap));

		std::string shaderVariablesStr = shaderVariables.specialize(specMap);
		if (BASE_DATA_TYPE_CASES[i] != DataTypes::UINT32)
		{
			shaderVariablesStr	= "%uint32     = OpTypeInt  32      0\n"
								  "%c_uint32_1 = OpConstant %uint32 1\n"
								+ shaderVariablesStr;
		}

		const std::string shaderAsm =
			shaderHeader.specialize(specMap) +
			shaderAnnotations.specialize(specMap) +
			shaderVariablesStr +
			tempShaderFunctions.specialize(specMap);

		FilledResourceDesc desc;
		desc.dataType		= BASE_DATA_TYPE_CASES[i];
		desc.elemCount		= Constants::numThreads;
		desc.descriptorType	= vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		desc.padding		= 0;
		desc.fillType		= FillingTypes::VALUE;
		desc.value			= 1.0;

		Resource input0		= createFilledResource(desc);
		Resource output		= createFilledResource(desc);
		desc.value			= 0.0;
		Resource input1		= createFilledResource(desc);

		spec.assembly		= shaderAsm;
		spec.numWorkGroups	= tcu::IVec3(1, 1, 1);
		spec.inputs.push_back(input0);
		spec.inputs.push_back(input1);
		spec.outputs.push_back(output);
		spec.extensions.push_back("VK_KHR_storage_buffer_storage_class");
		spec.extensions.push_back("VK_KHR_shader_untyped_pointers");

		testGroup->addChild(new SpvAsmComputeShaderCase(testCtx, testName.c_str(), spec));
	}
}

void addVariablePtrPrivateVariableTests(tcu::TestCaseGroup* testGroup, MEMORY_MODEL_TYPE memModel)
{
	tcu::TestContext& testCtx	= testGroup->getTestContext();

	tcu::StringTemplate shaderHeader(
		createShaderHeader()
	);

	tcu::StringTemplate shaderAnnotations(
		createShaderAnnotations(PointerTestCases::PRIVATE_VARIABLE_VARIABLE_PTR)
	);

	tcu::StringTemplate shaderVariables(
		createShaderVariables(PointerTestCases::PRIVATE_VARIABLE_VARIABLE_PTR)
	);

	tcu::StringTemplate shaderFunctions(
		createShaderMain(PointerTestCases::PRIVATE_VARIABLE_VARIABLE_PTR)
	);

	for (deUint32 i = 0; i < DE_LENGTH_OF_ARRAY(BASE_DATA_TYPE_CASES); ++i)
	{
		std::string testName	= toString(BASE_DATA_TYPE_CASES[i]);

		std::map<std::string, std::string>	specMap;
		specMap["baseDecl"]		= getDeclaration(BASE_DATA_TYPE_CASES[i]);
		specMap["baseType"]		= toString(BASE_DATA_TYPE_CASES[i]);
		specMap["boolConst"]	= "%c_bool_true = OpConstantTrue %bool";
		specMap["condition"]	= "%c_bool_true";

		ComputeShaderSpec spec;
		adjustSpecForUntypedPointers(spec, specMap);
		adjustSpecForMemoryModel(spec, specMap, memModel);
		adjustSpecForDataTypes(spec, specMap, BASE_DATA_TYPE_CASES[i]);
		adjustSpecForVariablePointers(spec, specMap);

		const tcu::StringTemplate tempShaderFunctions	= tcu::StringTemplate(shaderFunctions.specialize(specMap));

		std::string shaderVariablesStr	= shaderVariables.specialize(specMap);
		if (BASE_DATA_TYPE_CASES[i] != DataTypes::UINT32)
		{
			shaderVariablesStr	= "%uint32     = OpTypeInt  32      0\n"
								  "%c_uint32_1 = OpConstant %uint32 1\n"
								+ shaderVariablesStr;
		}

		const std::string shaderAsm =
			shaderHeader.specialize(specMap) +
			shaderAnnotations.specialize(specMap) +
			shaderVariablesStr +
			tempShaderFunctions.specialize(specMap);

		FilledResourceDesc desc;
		desc.dataType		= BASE_DATA_TYPE_CASES[i];
		desc.elemCount		= Constants::numThreads;
		desc.descriptorType	= vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		desc.padding		= 0;
		desc.fillType		= FillingTypes::VALUE;
		desc.value			= 1.0;

		Resource input0		= createFilledResource(desc);
		Resource output		= createFilledResource(desc);
		desc.value			= 0.0;
		Resource input1		= createFilledResource(desc);

		spec.assembly		= shaderAsm;
		spec.numWorkGroups	= tcu::IVec3(1, 1, 1);
		spec.inputs.push_back(input0);
		spec.inputs.push_back(input1);
		spec.outputs.push_back(output);
		spec.extensions.push_back("VK_KHR_storage_buffer_storage_class");
		spec.extensions.push_back("VK_KHR_shader_untyped_pointers");

		testGroup->addChild(new SpvAsmComputeShaderCase(testCtx, testName.c_str(), spec));
	}
}

void addStructAsTypeTests(tcu::TestCaseGroup* testGroup, MEMORY_MODEL_TYPE memModel)
{
	tcu::TestContext& testCtx = testGroup->getTestContext();

	tcu::StringTemplate shaderHeader(
		createShaderHeader()
	);

	tcu::StringTemplate shaderAnnotations(
		createShaderAnnotations(TypePunningTestCases::CUSTOM_STRUCT_TYPE)
	);

	tcu::StringTemplate shaderVariables(
		createShaderVariables(TypePunningTestCases::CUSTOM_STRUCT_TYPE)
	);

	tcu::StringTemplate shaderFunctions(
		createShaderMain(TypePunningTestCases::CUSTOM_STRUCT_TYPE)
	);

	{
		std::map<std::string, std::string>	specMap;
		specMap["inputOffsets"] =
			"OpMemberDecorate %input_buffer 1 Offset 8\n";
		specMap["baseTypes"] =
			"%int32         = OpTypeInt   32 1\n"
			"%float32       = OpTypeFloat 32\n"
			"%vec2_uint32   = OpTypeVector %uint32  2\n"
			"%vec2_float32  = OpTypeVector %float32 2\n"
			"%vec4_int32    = OpTypeVector %int32   4\n";
		specMap["inputLayout"] = "%vec2_uint32 %vec2_float32";
		specMap["outputLayout"] = "%vec4_int32";

		ComputeShaderSpec spec;
		adjustSpecForMemoryModel(spec, specMap, memModel);
		adjustSpecForUntypedPointers(spec, specMap);

		const std::string shaderAsm =
			shaderHeader.specialize(specMap) +
			shaderAnnotations.specialize(specMap) +
			shaderVariables.specialize(specMap) +
			shaderFunctions.specialize(specMap);

		struct InputStruct
		{
			tcu::UVec2	vec2_uint32;
			tcu::Vec2	vec2_float32;
		} inputStruct;

		inputStruct.vec2_float32[0]	= 1.0f;
		inputStruct.vec2_float32[1]	= 1.0f;
		inputStruct.vec2_uint32[0]	= 0u;
		inputStruct.vec2_uint32[1]	= 1u;

		struct OutputStruct
		{
			tcu::UVec4	vec4_int32;
		} outputStruct;

		outputStruct.vec4_int32[0]	= 1;
		outputStruct.vec4_int32[1]	= 1;
		outputStruct.vec4_int32[2]	= 0;
		outputStruct.vec4_int32[3]	= 1;

		Resource inputResource = Resource(BufferSp(new Buffer<InputStruct>(std::vector<InputStruct>(1, inputStruct), 0)), vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
		Resource outputResource = Resource(BufferSp(new Buffer<OutputStruct>(std::vector<OutputStruct>(1, outputStruct), 0)), vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);

		spec.assembly = shaderAsm;
		spec.numWorkGroups = tcu::IVec3(Constants::numThreads, 1, 1);
		spec.inputs.push_back(inputResource);
		spec.outputs.push_back(outputResource);
		spec.extensions.push_back("VK_KHR_storage_buffer_storage_class");
		spec.extensions.push_back("VK_KHR_shader_untyped_pointers");

		testGroup->addChild(new SpvAsmComputeShaderCase(testCtx, "vec2_uint32_vec2_float32_to_vec4_int32", spec));
	}

	{
		std::map<std::string, std::string>	specMap;
		specMap["outputOffsets"]	=
			"OpMemberDecorate %output_buffer 1 Offset 1\n"
			"OpMemberDecorate %output_buffer 2 Offset 2\n"
			"OpMemberDecorate %output_buffer 3 Offset 3\n";
		specMap["baseTypes"]		=
			"%uint8         = OpTypeInt   8 0\n";
		specMap["inputLayout"]		= "%uint32";
		specMap["outputLayout"]		= "%uint8 %uint8 %uint8 %uint8";

		ComputeShaderSpec spec;
		adjustSpecForMemoryModel(spec, specMap, memModel);
		adjustSpecForDataTypes(spec, specMap, DataTypes::UINT8);
		adjustSpecForUntypedPointers(spec, specMap);

		const std::string shaderAsm =
			shaderHeader.specialize(specMap) +
			shaderAnnotations.specialize(specMap) +
			shaderVariables.specialize(specMap) +
			shaderFunctions.specialize(specMap);

		struct InputStruct
		{
			deUint32 uint32;
		} inputStruct;

		inputStruct.uint32 = 0b00000001000000010000000100000001;

		struct OutputStruct
		{
			deUint8 uint8_0;
			deUint8 uint8_1;
			deUint8 uint8_2;
			deUint8 uint8_3;
		} outputStruct;

		outputStruct.uint8_0 = 1;
		outputStruct.uint8_1 = 1;
		outputStruct.uint8_2 = 1;
		outputStruct.uint8_3 = 1;

		Resource inputResource = Resource(BufferSp(new Buffer<InputStruct>(std::vector<InputStruct>(1, inputStruct), 0)), vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
		Resource outputResource = Resource(BufferSp(new Buffer<OutputStruct>(std::vector<OutputStruct>(1, outputStruct), 0)), vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);

		spec.assembly = shaderAsm;
		spec.numWorkGroups = tcu::IVec3(Constants::numThreads, 1, 1);
		spec.inputs.push_back(inputResource);
		spec.outputs.push_back(outputResource);
		spec.extensions.push_back("VK_KHR_storage_buffer_storage_class");
		spec.extensions.push_back("VK_KHR_shader_untyped_pointers");

		testGroup->addChild(new SpvAsmComputeShaderCase(testCtx, "uint32_to_uint8_uint8_uint8_uint8", spec));
	}

	{
		std::map<std::string, std::string>	specMap;
		specMap["inputOffsets"]	=
			"OpMemberDecorate %input_buffer 1 Offset 16\n";
		specMap["outputOffsets"] =
			"OpMemberDecorate %output_buffer 1 Offset 2\n"
			"OpMemberDecorate %output_buffer 2 Offset 6\n"
			"OpMemberDecorate %output_buffer 3 Offset 8\n"
			"OpMemberDecorate %output_buffer 4 Offset 12\n";
		specMap["baseTypes"]	=
			"%int32         = OpTypeInt   32 1\n"
			"%float16       = OpTypeFloat 16\n"
			"%vec2_float16  = OpTypeVector %float16 2\n"
			"%vec4_float16  = OpTypeVector %float16 4\n"
			"%vec2_int32    = OpTypeVector %int32   2\n";
		specMap["inputLayout"]	= "%vec4_float16 %vec2_int32";
		specMap["outputLayout"]	= "%float16 %vec2_float16 %float16 %int32 %int32";

		ComputeShaderSpec spec;
		adjustSpecForMemoryModel(spec, specMap, memModel);
		adjustSpecForUntypedPointers(spec, specMap);
		adjustSpecForDataTypes(spec, specMap, DataTypes::FLOAT16);

		const std::string shaderAsm =
			shaderHeader.specialize(specMap) +
			shaderAnnotations.specialize(specMap) +
			shaderVariables.specialize(specMap) +
			shaderFunctions.specialize(specMap);

		struct InputStruct
		{
			tcu::F16Vec4	vec4_float16;
			tcu::IVec2		vec2_int32;
		} inputStruct;

		inputStruct.vec4_float16[0]	= tcu::Float16(1.0f);
		inputStruct.vec4_float16[1]	= tcu::Float16(-100.0f);
		inputStruct.vec4_float16[2]	= tcu::Float16(17.312f);
		inputStruct.vec4_float16[3]	= tcu::Float16(-1.11f);
		inputStruct.vec2_int32[0]	= 1;
		inputStruct.vec2_int32[1]	= -1;

		struct OutputStruct
		{
			tcu::Float16	float16_0;
			tcu::F16Vec2	vec2_float16;
			tcu::Float16	float16_1;
			deInt32			int32_0;
			deInt32			int32_1;
		} outputStruct;

		outputStruct.float16_0			= tcu::Float16(1.0f);
		outputStruct.vec2_float16[0]	= tcu::Float16(-100.0f);
		outputStruct.vec2_float16[1]	= tcu::Float16(17.312f);
		outputStruct.float16_1			= tcu::Float16(-1.11f);
		outputStruct.int32_0			= 1;
		outputStruct.int32_1			= -1;

		Resource inputResource = Resource(BufferSp(new Buffer<InputStruct>(std::vector<InputStruct>(1, inputStruct), 0)), vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
		Resource outputResource = Resource(BufferSp(new Buffer<OutputStruct>(std::vector<OutputStruct>(1, outputStruct), 0)), vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);

		spec.assembly = shaderAsm;
		spec.numWorkGroups = tcu::IVec3(Constants::numThreads, 1, 1);
		spec.inputs.push_back(inputResource);
		spec.outputs.push_back(outputResource);
		spec.extensions.push_back("VK_KHR_storage_buffer_storage_class");
		spec.extensions.push_back("VK_KHR_shader_untyped_pointers");

		testGroup->addChild(new SpvAsmComputeShaderCase(testCtx, "vec4_float16_vec2_int32_to_float16_vec2_float16_float16_int32_int32", spec));
	}

	{
		std::map<std::string, std::string>	specMap;
		specMap["inputOffsets"]		=
			"OpMemberDecorate %int32_struct 0 Offset 0\n"
			"OpMemberDecorate %int32_struct 1 Offset 4\n"
			"OpMemberDecorate %int32_struct 2 Offset 8\n"
			"OpMemberDecorate %int32_struct 3 Offset 16\n";
		specMap["outputOffsets"]	=
			"OpMemberDecorate %output_buffer 1 Offset 8\n";
		specMap["baseTypes"]		=
			"%int32         = OpTypeInt   32 1\n"
			"%vec2_int32    = OpTypeVector %int32   2\n"
			"%int32_struct  = OpTypeStruct %int32 %int32 %int32 %int32";
		specMap["inputLayout"]		= "%int32_struct";
		specMap["outputLayout"]		= "%vec2_int32 %vec2_int32";

		ComputeShaderSpec spec;
		adjustSpecForMemoryModel(spec, specMap, memModel);
		adjustSpecForUntypedPointers(spec, specMap);

		const std::string shaderAsm =
			shaderHeader.specialize(specMap) +
			shaderAnnotations.specialize(specMap) +
			shaderVariables.specialize(specMap) +
			shaderFunctions.specialize(specMap);

		struct NestedStruct
		{
			deInt32 int32_0;
			deInt32 int32_1;
			deInt32 int32_2;
			deInt32 int32_3;
		};

		struct InputStruct
		{
			NestedStruct nested;
		} inputStruct;

		inputStruct.nested.int32_0	= 0;
		inputStruct.nested.int32_1	= 1;
		inputStruct.nested.int32_2	= -1;
		inputStruct.nested.int32_3	= INT32_MAX;

		struct OutputStruct
		{
			tcu::IVec2 vec2_int32_0;
			tcu::IVec2 vec2_int32_1;
		} outputStruct;

		outputStruct.vec2_int32_0[0]	= 0;
		outputStruct.vec2_int32_0[1]	= 1;
		outputStruct.vec2_int32_1[0]	= -1;
		outputStruct.vec2_int32_1[1]	= INT32_MAX;

		Resource inputResource = Resource(BufferSp(new Buffer<InputStruct>(std::vector<InputStruct>(1, inputStruct), 0)), vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
		Resource outputResource = Resource(BufferSp(new Buffer<OutputStruct>(std::vector<OutputStruct>(1, outputStruct), 0)), vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);

		spec.assembly = shaderAsm;
		spec.numWorkGroups = tcu::IVec3(Constants::numThreads, 1, 1);
		spec.inputs.push_back(inputResource);
		spec.outputs.push_back(outputResource);
		spec.extensions.push_back("VK_KHR_storage_buffer_storage_class");
		spec.extensions.push_back("VK_KHR_shader_untyped_pointers");

		testGroup->addChild(new SpvAsmComputeShaderCase(testCtx, "nested_struct_int32_int32_int32_int32_to_vec2_int32_vec2_int32", spec));
	}

	{
		std::map<std::string, std::string>	specMap;
		specMap["inputOffsets"]		=
			"OpMemberDecorate %input_buffer 1 Offset 8\n"
			"OpMemberDecorate %input_buffer 2 Offset 16\n";
		specMap["outputOffsets"]	=
			"OpMemberDecorate %vec4_int64_struct 0 Offset 0\n";
		specMap["baseTypes"]		=
			"%int64             = OpTypeInt    64 1\n"
			"%uint64            = OpTypeInt   64 0\n"
			"%float64           = OpTypeFloat 64\n"
			"%vec2_float64      = OpTypeVector %float64 2\n"
			"%vec4_int64        = OpTypeVector %int64   4\n"
			"%vec4_int64_struct = OpTypeStruct %vec4_int64";
		specMap["inputLayout"]		= "%int64 %uint64 %vec2_float64";
		specMap["outputLayout"]		= "%vec4_int64_struct";
		specMap["baseTypeCap"]		= "OpCapability Int64\n"
			"OpCapability Float64\n";

		ComputeShaderSpec spec;
		adjustSpecForMemoryModel(spec, specMap, memModel);
		adjustSpecForUntypedPointers(spec, specMap);


		const std::string shaderAsm =
			shaderHeader.specialize(specMap) +
			shaderAnnotations.specialize(specMap) +
			shaderVariables.specialize(specMap) +
			shaderFunctions.specialize(specMap);

		struct NestedStruct
		{
			tcu::I64Vec4	vec4_int64;
		};

		struct InputStruct
		{
			deInt64			int64;
			deUint64		uint64;
			tcu::DVec2		vec2_float64;
		} inputStruct;

		inputStruct.int64			= INT64_MAX;
		inputStruct.uint64			= 1;
		inputStruct.vec2_float64[0]	= 0.0f;
		inputStruct.vec2_float64[1]	= -112.0f;

		struct OutputStruct
		{
			NestedStruct	nested;
		} outputStruct;

		outputStruct.nested.vec4_int64[0]	= INT64_MAX;
		outputStruct.nested.vec4_int64[1]	= 1;
		outputStruct.nested.vec4_int64[2]	= 0;
		outputStruct.nested.vec4_int64[3]	= -112;


		Resource inputResource	= Resource(BufferSp(new Buffer<InputStruct>(std::vector<InputStruct>(1, inputStruct), 0)), vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
		Resource outputResource	= Resource(BufferSp(new Buffer<OutputStruct>(std::vector<OutputStruct>(1, outputStruct), 0)), vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);

		spec.assembly		= shaderAsm;
		spec.numWorkGroups	= tcu::IVec3(Constants::numThreads, 1, 1);
		spec.inputs.push_back(inputResource);
		spec.outputs.push_back(outputResource);
		spec.extensions.push_back("VK_KHR_storage_buffer_storage_class");
		spec.extensions.push_back("VK_KHR_shader_untyped_pointers");
		spec.requestedVulkanFeatures.coreFeatures.shaderFloat64	= VK_TRUE;
		spec.requestedVulkanFeatures.coreFeatures.shaderInt64	= VK_TRUE;

		testGroup->addChild(new SpvAsmComputeShaderCase(testCtx, "int64_uint64_vec2_float64_to_nested_struct_vec4_int64", spec));
	}

	{
		std::map<std::string, std::string>	specMap;
		specMap["outputOffsets"]	=
			"OpMemberDecorate %uint16_struct 0 Offset 0\n"
			"OpMemberDecorate %uint16_struct 1 Offset 2\n"
			"OpMemberDecorate %uint16_struct 2 Offset 4\n"
			"OpMemberDecorate %uint16_struct 3 Offset 6\n";
		specMap["baseTypes"]		=
			"%uint16        = OpTypeInt   16 0\n"
			"%uint64        = OpTypeInt   64 0\n"
			"%uint16_struct = OpTypeStruct %uint16 %uint16 %uint16 %uint16";
		specMap["inputLayout"]		= "%uint64";
		specMap["outputLayout"]		= "%uint16_struct";
		specMap["baseTypeCap"]		= "OpCapability Int64\n"
			"OpCapability Int16\n";

		ComputeShaderSpec spec;
		adjustSpecForMemoryModel(spec, specMap, memModel);
		adjustSpecForUntypedPointers(spec, specMap);

		const std::string shaderAsm =
			shaderHeader.specialize(specMap) +
			shaderAnnotations.specialize(specMap) +
			shaderVariables.specialize(specMap) +
			shaderFunctions.specialize(specMap);

		struct NestedStruct
		{
			deUint16	uint16_0;
			deUint16	uint16_1;
			deUint16	uint16_2;
			deUint16	uint16_3;
		};

		struct InputStruct
		{
			deUint64	uint64;
		} inputStruct;

		inputStruct.uint64	= 0b0000000000000001000000000000000100000000000000010000000000000001;

		struct OutputStruct
		{
			NestedStruct	nested;
		} outputStruct;

		outputStruct.nested.uint16_0	= 1;
		outputStruct.nested.uint16_1	= 1;
		outputStruct.nested.uint16_2	= 1;
		outputStruct.nested.uint16_3	= 1;


		Resource inputResource	= Resource(BufferSp(new Buffer<InputStruct>(std::vector<InputStruct>(1, inputStruct), 0)), vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
		Resource outputResource	= Resource(BufferSp(new Buffer<OutputStruct>(std::vector<OutputStruct>(1, outputStruct), 0)), vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);

		spec.assembly = shaderAsm;
		spec.numWorkGroups = tcu::IVec3(Constants::numThreads, 1, 1);
		spec.inputs.push_back(inputResource);
		spec.outputs.push_back(outputResource);
		spec.extensions.push_back("VK_KHR_storage_buffer_storage_class");
		spec.extensions.push_back("VK_KHR_shader_untyped_pointers");
		spec.requestedVulkanFeatures.coreFeatures.shaderInt16	= VK_TRUE;
		spec.requestedVulkanFeatures.coreFeatures.shaderInt64	= VK_TRUE;

		testGroup->addChild(new SpvAsmComputeShaderCase(testCtx, "uint64_to_nested_struct_uint16_uint16_uint16_uint16", spec));
	}
}

void addMultipleAccessChainTests(tcu::TestCaseGroup* testGroup, MEMORY_MODEL_TYPE memModel)
{
	tcu::TestContext& testCtx = testGroup->getTestContext();

	tcu::StringTemplate shaderHeader(
		createShaderHeader()
	);

	tcu::StringTemplate shaderAnnotations(
		createShaderAnnotations(TypePunningTestCases::MULTIPLE_ACCESS_CHAINS)
	);

	tcu::StringTemplate shaderVariables(
		createShaderVariables(TypePunningTestCases::MULTIPLE_ACCESS_CHAINS)
	);

	tcu::StringTemplate shaderFunctions(
		createShaderMain(TypePunningTestCases::MULTIPLE_ACCESS_CHAINS)
	);

	for (deUint32 i = 0; i < DE_LENGTH_OF_ARRAY(BASE_DATA_TYPE_CASES); ++i)
	{
		std::string testName	= toString(BASE_DATA_TYPE_CASES[i]);

		std::map<std::string, std::string>	specMap;
		specMap["baseDecl"]		= getDeclaration(BASE_DATA_TYPE_CASES[i]);
		specMap["baseType"]		= toString(BASE_DATA_TYPE_CASES[i]);
		specMap["size"]			= std::to_string(getSizeInBytes(BASE_DATA_TYPE_CASES[i]));

		ComputeShaderSpec spec;
		adjustSpecForUntypedPointers(spec, specMap);
		adjustSpecForMemoryModel(spec, specMap, memModel);
		adjustSpecForDataTypes(spec, specMap, BASE_DATA_TYPE_CASES[i]);

		std::string shaderVariablesStr = shaderVariables.specialize(specMap);
		if (BASE_DATA_TYPE_CASES[i] != DataTypes::UINT32)
		{
			shaderVariablesStr	= "%uint32     = OpTypeInt  32      0\n"
								  "%c_uint32_1 = OpConstant %uint32 1\n"
								+ shaderVariablesStr;
		}

		const std::string shaderAsm =
			shaderHeader.specialize(specMap) +
			shaderAnnotations.specialize(specMap) +
			shaderVariablesStr +
			shaderFunctions.specialize(specMap);

		FilledResourceDesc desc;
		desc.dataType			= BASE_DATA_TYPE_CASES[i];
		desc.elemCount			= 2;
		desc.descriptorType		= vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		desc.padding			= 0;
		desc.fillType			= FillingTypes::VALUE;
		desc.value				= 2;
		Resource input			= createFilledResource(desc);
		desc.value				= 2;
		Resource output			= createFilledResource(desc);

		spec.assembly		= shaderAsm;
		spec.numWorkGroups	= tcu::IVec3(Constants::numThreads, 1, 1);
		spec.inputs.push_back(input);
		spec.outputs.push_back(output);
		spec.extensions.push_back("VK_KHR_storage_buffer_storage_class");
		spec.extensions.push_back("VK_KHR_shader_untyped_pointers");

		testGroup->addChild(new SpvAsmComputeShaderCase(testCtx, testName.c_str(), spec));
	}
}

void addVariablePointersMultipleAccessChainTests(tcu::TestCaseGroup* testGroup, MEMORY_MODEL_TYPE memModel)
{
	tcu::TestContext& testCtx = testGroup->getTestContext();

	tcu::StringTemplate shaderHeader(
		createShaderHeader()
	);

	tcu::StringTemplate shaderAnnotations(
		createShaderAnnotations(PointerTestCases::MULTIPLE_ACCESS_CHAINS_VARIABLE_PTR)
	);

	tcu::StringTemplate shaderVariables(
		createShaderVariables(PointerTestCases::MULTIPLE_ACCESS_CHAINS_VARIABLE_PTR)
	);

	tcu::StringTemplate shaderFunctions(
		createShaderMain(PointerTestCases::MULTIPLE_ACCESS_CHAINS_VARIABLE_PTR)
	);

	for (deUint32 i = 0; i < DE_LENGTH_OF_ARRAY(BASE_DATA_TYPE_CASES); ++i)
	{
		std::string testName	= toString(BASE_DATA_TYPE_CASES[i]);

		std::map<std::string, std::string>	specMap;
		specMap["baseDecl"]	= getDeclaration(BASE_DATA_TYPE_CASES[i]);
		specMap["baseType"]	= toString(BASE_DATA_TYPE_CASES[i]);
		specMap["size"]		= std::to_string(getSizeInBytes(BASE_DATA_TYPE_CASES[i]));

		ComputeShaderSpec spec;
		adjustSpecForUntypedPointers(spec, specMap);
		adjustSpecForMemoryModel(spec, specMap, memModel);
		adjustSpecForDataTypes(spec, specMap, BASE_DATA_TYPE_CASES[i]);
		adjustSpecForVariablePointers(spec, specMap);

		std::string shaderVariablesStr = shaderVariables.specialize(specMap);
		if (BASE_DATA_TYPE_CASES[i] != DataTypes::UINT32)
		{
			shaderVariablesStr	= "%uint32     = OpTypeInt  32      0\n"
								  "%c_uint32_1 = OpConstant %uint32 1\n"
								+ shaderVariablesStr;
		}

		const std::string shaderAsm	=
			shaderHeader.specialize(specMap) +
			shaderAnnotations.specialize(specMap) +
			shaderVariablesStr +
			shaderFunctions.specialize(specMap);

		FilledResourceDesc desc;
		desc.dataType		= BASE_DATA_TYPE_CASES[i];
		desc.elemCount		= 2;
		desc.descriptorType	= vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		desc.padding		= 0;
		desc.fillType		= FillingTypes::VALUE;
		desc.value			= 2;
		Resource input		= createFilledResource(desc);
		desc.value			= 2;
		Resource output		= createFilledResource(desc);

		spec.assembly		= shaderAsm;
		spec.numWorkGroups	= tcu::IVec3(Constants::numThreads, 1, 1);
		spec.inputs.push_back(input);
		spec.outputs.push_back(output);
		spec.extensions.push_back("VK_KHR_storage_buffer_storage_class");
		spec.extensions.push_back("VK_KHR_shader_untyped_pointers");

		testGroup->addChild(new SpvAsmComputeShaderCase(testCtx, testName.c_str(), spec));
	}
}

void addPhysicalStorageOpBitcastTests(tcu::TestCaseGroup* testGroup, MEMORY_MODEL_TYPE memModel, deBool fromUntyped)
{
	tcu::TestContext& testCtx	= testGroup->getTestContext();

	const POINTER_TEST_CASE	ptrTestType	= fromUntyped ? PointerTestCases::OP_BITCAST_FROM_UNTYPED_PHYSICAL_STORAGE : PointerTestCases::OP_BITCAST_TO_UNTYPED_PHYSICAL_STORAGE;

	tcu::StringTemplate shaderHeader(
		createShaderHeader()
	);

	tcu::StringTemplate shaderAnnotations(
		createShaderAnnotations(ptrTestType)
	);

	tcu::StringTemplate shaderVariables(
		createShaderVariables(ptrTestType)
	);

	tcu::StringTemplate shaderFunctions(
		createShaderMain(ptrTestType)
	);

	for (deUint32 i = 0; i < DE_LENGTH_OF_ARRAY(BASE_DATA_TYPE_CASES); ++i)
	{
		std::string testName	= toString(BASE_DATA_TYPE_CASES[i]);

		std::map<std::string, std::string>	specMap;
		specMap["baseDecl"]		= getDeclaration(BASE_DATA_TYPE_CASES[i]);
		specMap["baseType"]		= toString(BASE_DATA_TYPE_CASES[i]);
		specMap["threadCount"]	= std::to_string(Constants::numThreads);
		specMap["alignment"]	= std::to_string(getSizeInBytes(BASE_DATA_TYPE_CASES[i]));

		ComputeShaderSpec spec;
		adjustSpecForUntypedPointers(spec, specMap);
		adjustSpecForMemoryModel(spec, specMap, memModel);
		adjustSpecForDataTypes(spec, specMap, BASE_DATA_TYPE_CASES[i]);
		adjustSpecForPhysicalStorageBuffer(spec, specMap, memModel);

		const tcu::StringTemplate tempShaderFunctions = tcu::StringTemplate(shaderFunctions.specialize(specMap));

		std::string shaderVariablesStr = shaderVariables.specialize(specMap);
		if (BASE_DATA_TYPE_CASES[i] != DataTypes::UINT32)
		{
			shaderVariablesStr	= "%uint32     = OpTypeInt  32      0\n"
								  "%c_uint32_1 = OpConstant %uint32 1\n"
								+ shaderVariablesStr;
		}

		const std::string shaderAsm =
			shaderHeader.specialize(specMap) +
			shaderAnnotations.specialize(specMap) +
			shaderVariablesStr +
			tempShaderFunctions.specialize(specMap);

		FilledResourceDesc desc;
		desc.dataType		= BASE_DATA_TYPE_CASES[i];
		desc.elemCount		= Constants::numThreads;
		desc.descriptorType	= vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		desc.padding		= 0;
		desc.fillType		= FillingTypes::VALUE;
		desc.value			= 1;

		Resource inputOutput	= createFilledResource(desc);

		spec.assembly		= shaderAsm;
		spec.numWorkGroups	= tcu::IVec3(Constants::numThreads, 1, 1);
		spec.inputs.push_back(inputOutput);
		spec.outputs.push_back(inputOutput);
		spec.extensions.push_back("VK_KHR_storage_buffer_storage_class");
		spec.extensions.push_back("VK_KHR_shader_untyped_pointers");

		testGroup->addChild(new SpvAsmComputeShaderCase(testCtx, testName.c_str(), spec));
	}
}

void addWorkgroupMemoryInteractionTests(tcu::TestCaseGroup* testGroup, MEMORY_MODEL_TYPE memModel, WORKGROUP_TEST_CASE testCase)
{
	tcu::TestContext& testCtx	= testGroup->getTestContext();

	tcu::StringTemplate shaderHeader(
		createShaderHeader("%input_data_var %data_buffer_0_untyped_var %data_buffer_1_untyped_var %output_data_var")
	);

	tcu::StringTemplate shaderAnnotations(
		createShaderAnnotations(testCase)
	);

	tcu::StringTemplate shaderVariables(
		createShaderVariables(testCase)
	);

	tcu::StringTemplate shaderFunctions(
		createShaderMain(testCase)
	);

	for (deUint32 i = 0; i < DE_LENGTH_OF_ARRAY(BASE_DATA_TYPE_CASES); ++i)
	{
		std::string testName = toString(BASE_DATA_TYPE_CASES[i]);

		std::map<std::string, std::string>	specMap;
		specMap["baseDecl"]		= getDeclaration(BASE_DATA_TYPE_CASES[i]);
		specMap["baseType"]		= toString(BASE_DATA_TYPE_CASES[i]);
		specMap["vecOffset"]	= std::to_string(4 * getSizeInBytes(BASE_DATA_TYPE_CASES[i]));

		ComputeShaderSpec spec;
		adjustSpecForUntypedPointers(spec, specMap);
		adjustSpecForMemoryModel(spec, specMap, memModel);
		adjustSpecForDataTypes(spec, specMap, BASE_DATA_TYPE_CASES[i]);
		adjustSpecForWorkgroupMemoryExplicitLayout(spec, specMap);

		const tcu::StringTemplate tempShaderFunctions = tcu::StringTemplate(shaderFunctions.specialize(specMap));

		std::string shaderVariablesStr = shaderVariables.specialize(specMap);
		if (BASE_DATA_TYPE_CASES[i] != DataTypes::UINT32)
		{
			shaderVariablesStr	= "%uint32     = OpTypeInt  32      0\n"
								+ shaderVariablesStr;
		}

		const std::string shaderAsm =
			shaderHeader.specialize(specMap) +
			shaderAnnotations.specialize(specMap) +
			shaderVariablesStr +
			tempShaderFunctions.specialize(specMap);

		FilledResourceDesc desc;
		desc.dataType		= BASE_DATA_TYPE_CASES[i];
		desc.elemCount		= 5;										// scalar + vec4
		desc.descriptorType	= vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		desc.padding		= 0;
		desc.fillType		= FillingTypes::VALUE;
		desc.value			= 1;

		Resource inputOutput	= createFilledResource(desc);

		spec.assembly		= shaderAsm;
		spec.numWorkGroups	= tcu::IVec3(Constants::numThreads, 1, 1);
		spec.spirvVersion	= SPIRV_VERSION_1_4;	// workgroup memory explicit layout requires SPIR-V 1.4
		spec.inputs.push_back(inputOutput);
		spec.outputs.push_back(inputOutput);
		spec.extensions.push_back("VK_KHR_storage_buffer_storage_class");
		spec.extensions.push_back("VK_KHR_shader_untyped_pointers");

		testGroup->addChild(new SpvAsmComputeShaderCase(testCtx, testName.c_str(), spec));
	}
}

void addCooperativeMatrixInteractionBasicTests(tcu::TestCaseGroup* testGroup, MEMORY_MODEL_TYPE memModel)
{
	tcu::TestContext& testCtx = testGroup->getTestContext();

	// Load tests
	{
		de::MovePtr<tcu::TestCaseGroup>	loadGroup(new tcu::TestCaseGroup(testCtx, "load", ""));

		tcu::StringTemplate shaderHeader(
			createShaderHeader("%input_data_untyped_var %output_data_var")
		);

		tcu::StringTemplate shaderAnnotations(
			createShaderAnnotations(CooperativeMatrixTestCases::BASIC_LOAD)
		);

		tcu::StringTemplate shaderVariables(
			createShaderVariables(CooperativeMatrixTestCases::BASIC_LOAD)
		);

		tcu::StringTemplate shaderFunctions(
			createShaderMain(CooperativeMatrixTestCases::BASIC_LOAD)
		);

		for (deUint32 i = 0; i < DE_LENGTH_OF_ARRAY(MATRIX_USE_CASES); ++i)
		{
			de::MovePtr<tcu::TestCaseGroup>	useCaseGroup(new tcu::TestCaseGroup(testCtx, toString(MATRIX_USE_CASES[i]), ""));

			for (deUint32 j = 0; j < DE_LENGTH_OF_ARRAY(MATRIX_LAYOUT_CASES); ++j)
			{
				de::MovePtr<tcu::TestCaseGroup>	layoutGroup(new tcu::TestCaseGroup(testCtx, toString(MATRIX_LAYOUT_CASES[j]), ""));

				for (deUint32 k = 0; k < DE_LENGTH_OF_ARRAY(BASE_DATA_TYPE_CASES); ++k)
				{
					std::string testName	= toString(BASE_DATA_TYPE_CASES[k]);

					std::map<std::string, std::string>	specMap;
					specMap["baseDecl"]		= getDeclaration(BASE_DATA_TYPE_CASES[k]);
					specMap["baseType"]		= toString(BASE_DATA_TYPE_CASES[k]);
					specMap["typeSize"]		= std::to_string(getSizeInBytes(BASE_DATA_TYPE_CASES[k]));
					specMap["matrixUse"]	= std::to_string(getMatrixBinaryUse(MATRIX_USE_CASES[i]));
					specMap["matrixLayout"]	= std::to_string(getMatrixBinaryLayout(MATRIX_LAYOUT_CASES[j]));

					ComputeShaderSpec spec;
					adjustSpecForUntypedPointers(spec, specMap);
					adjustSpecForMemoryModel(spec, specMap, memModel);
					adjustSpecForDataTypes(spec, specMap, BASE_DATA_TYPE_CASES[k]);
					adjustSpecForCooperativeMatrix(spec, specMap);

					const tcu::StringTemplate tempShaderFunctions	= tcu::StringTemplate(shaderFunctions.specialize(specMap));

					std::string shaderVariablesStr	= shaderVariables.specialize(specMap);
					if (BASE_DATA_TYPE_CASES[k] != DataTypes::UINT32)
					{
						shaderVariablesStr	= "%uint32     = OpTypeInt  32      0\n"
											+ shaderVariablesStr;
					}

					const std::string shaderAsm =
						shaderHeader.specialize(specMap) +
						shaderAnnotations.specialize(specMap) +
						shaderVariablesStr +
						tempShaderFunctions.specialize(specMap);

					FilledResourceDesc desc;
					desc.dataType		= BASE_DATA_TYPE_CASES[k];
					desc.elemCount		= 1;
					desc.descriptorType	= vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
					desc.padding		= 0;
					desc.fillType		= FillingTypes::VALUE;
					desc.value			= 1;

					Resource inputOutput	= createFilledResource(desc);

					spec.assembly		= shaderAsm;
					spec.numWorkGroups	= tcu::IVec3(1, 1, 1);
					spec.spirvVersion	= SPIRV_VERSION_1_6;	// cooperative matrices requires SPIR-V 1.6
					spec.inputs.push_back(inputOutput);
					spec.outputs.push_back(inputOutput);
					spec.extensions.push_back("VK_KHR_storage_buffer_storage_class");
					spec.extensions.push_back("VK_KHR_shader_untyped_pointers");

					layoutGroup->addChild(new SpvAsmComputeShaderCase(testCtx, testName.c_str(), spec));
				}

				useCaseGroup->addChild(layoutGroup.release());
			}

			loadGroup->addChild(useCaseGroup.release());
		}

		testGroup->addChild(loadGroup.release());
	}

	// Store tests
	{
		de::MovePtr<tcu::TestCaseGroup>	storeGroup(new tcu::TestCaseGroup(testCtx, "store", ""));

		tcu::StringTemplate shaderHeader(
			createShaderHeader("%input_data_var %output_data_untyped_var")
		);

		tcu::StringTemplate shaderAnnotations(
			createShaderAnnotations(CooperativeMatrixTestCases::BASIC_STORE)
		);

		tcu::StringTemplate shaderVariables(
			createShaderVariables(CooperativeMatrixTestCases::BASIC_STORE)
		);

		tcu::StringTemplate shaderFunctions(
			createShaderMain(CooperativeMatrixTestCases::BASIC_STORE)
		);

		for (deUint32 i = 0; i < DE_LENGTH_OF_ARRAY(MATRIX_USE_CASES); ++i)
		{
			de::MovePtr<tcu::TestCaseGroup>	useCaseGroup(new tcu::TestCaseGroup(testCtx, toString(MATRIX_USE_CASES[i]), ""));

			for (deUint32 j = 0; j < DE_LENGTH_OF_ARRAY(MATRIX_LAYOUT_CASES); ++j)
			{
				de::MovePtr<tcu::TestCaseGroup>	layoutGroup(new tcu::TestCaseGroup(testCtx, toString(MATRIX_LAYOUT_CASES[j]), ""));

				for (deUint32 k = 0; k < DE_LENGTH_OF_ARRAY(BASE_DATA_TYPE_CASES); ++k)
				{
					std::string testName	= toString(BASE_DATA_TYPE_CASES[k]);

					std::map<std::string, std::string>	specMap;
					specMap["baseDecl"]		= getDeclaration(BASE_DATA_TYPE_CASES[k]);
					specMap["baseType"]		= toString(BASE_DATA_TYPE_CASES[k]);
					specMap["typeSize"]		= std::to_string(getSizeInBytes(BASE_DATA_TYPE_CASES[k]));
					specMap["matrixUse"]	= std::to_string(getMatrixBinaryUse(MATRIX_USE_CASES[i]));
					specMap["matrixLayout"]	= std::to_string(getMatrixBinaryLayout(MATRIX_LAYOUT_CASES[j]));

					ComputeShaderSpec spec;
					adjustSpecForUntypedPointers(spec, specMap);
					adjustSpecForMemoryModel(spec, specMap, memModel);
					adjustSpecForDataTypes(spec, specMap, BASE_DATA_TYPE_CASES[k]);
					adjustSpecForCooperativeMatrix(spec, specMap);

					const tcu::StringTemplate tempShaderFunctions	= tcu::StringTemplate(shaderFunctions.specialize(specMap));

					std::string shaderVariablesStr	= shaderVariables.specialize(specMap);
					if (BASE_DATA_TYPE_CASES[k] != DataTypes::UINT32)
					{
						shaderVariablesStr	= "%uint32     = OpTypeInt  32      0\n"
											+ shaderVariablesStr;
					}

					const std::string shaderAsm =
						shaderHeader.specialize(specMap) +
						shaderAnnotations.specialize(specMap) +
						shaderVariablesStr +
						tempShaderFunctions.specialize(specMap);

					FilledResourceDesc desc;
					desc.dataType		= BASE_DATA_TYPE_CASES[k];
					desc.elemCount		= 1;
					desc.descriptorType	= vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
					desc.padding		= 0;
					desc.fillType		= FillingTypes::VALUE;
					desc.value			= 1;

					Resource inputOutput	= createFilledResource(desc);

					spec.assembly		= shaderAsm;
					spec.numWorkGroups	= tcu::IVec3(1, 1, 1);
					spec.spirvVersion	= SPIRV_VERSION_1_6;	// cooperative matrices requires SPIR-V 1.6
					spec.inputs.push_back(inputOutput);
					spec.outputs.push_back(inputOutput);
					spec.extensions.push_back("VK_KHR_storage_buffer_storage_class");
					spec.extensions.push_back("VK_KHR_shader_untyped_pointers");

					layoutGroup->addChild(new SpvAsmComputeShaderCase(testCtx, testName.c_str(), spec));
				}

				useCaseGroup->addChild(layoutGroup.release());
			}

			storeGroup->addChild(useCaseGroup.release());
		}

		testGroup->addChild(storeGroup.release());
	}
}


void addCooperativeMatrixInteractionTypePunningTests(tcu::TestCaseGroup* testGroup, MEMORY_MODEL_TYPE memModel)
{
	tcu::TestContext& testCtx = testGroup->getTestContext();

	// Load tests
	{
		de::MovePtr<tcu::TestCaseGroup>	loadGroup(new tcu::TestCaseGroup(testCtx, "load", ""));

		tcu::StringTemplate shaderHeader(
			createShaderHeader("%input_data_untyped_var %output_data_var")
		);

		tcu::StringTemplate shaderAnnotations(
			createShaderAnnotations(CooperativeMatrixTestCases::TYPE_PUNNING_LOAD)
		);

		tcu::StringTemplate shaderVariables(
			createShaderVariables(CooperativeMatrixTestCases::TYPE_PUNNING_LOAD)
		);

		tcu::StringTemplate shaderFunctions(
			createShaderMain(CooperativeMatrixTestCases::TYPE_PUNNING_LOAD)
		);

		for (deUint32 i = 0; i < DE_LENGTH_OF_ARRAY(MATRIX_USE_CASES); ++i)
		{
			de::MovePtr<tcu::TestCaseGroup>	useCaseGroup(new tcu::TestCaseGroup(testCtx, toString(MATRIX_USE_CASES[i]), ""));

			for (deUint32 j = 0; j < DE_LENGTH_OF_ARRAY(MATRIX_LAYOUT_CASES); ++j)
			{
				de::MovePtr<tcu::TestCaseGroup>	layoutGroup(new tcu::TestCaseGroup(testCtx, toString(MATRIX_LAYOUT_CASES[j]), ""));

				for (deUint32 k = 0; k < DE_LENGTH_OF_ARRAY(BASE_DATA_TYPE_CASES); ++k)
				{
					std::vector<DATA_TYPE> sameSizeTypes	= getSameSizeBaseDataType(BASE_DATA_TYPE_CASES[k]);

					for (deUint32 l = 0; l < sameSizeTypes.size(); ++l)
					{
						std::string testName	= toString(BASE_DATA_TYPE_CASES[k]) + std::string("_to_") + toString(sameSizeTypes[l]);

						std::map<std::string, std::string>	specMap;
						specMap["baseDecl"]		= getDeclaration(BASE_DATA_TYPE_CASES[k]);
						specMap["baseType"]		= toString(BASE_DATA_TYPE_CASES[k]);
						specMap["sameSizeType"]	= toString(sameSizeTypes[l]);
						specMap["sameSizeDecl"]	= getDeclaration(sameSizeTypes[l]);
						specMap["typeSize"]		= std::to_string(getSizeInBytes(BASE_DATA_TYPE_CASES[k]));
						specMap["matrixUse"]	= std::to_string(getMatrixBinaryUse(MATRIX_USE_CASES[i]));
						specMap["matrixLayout"]	= std::to_string(getMatrixBinaryLayout(MATRIX_LAYOUT_CASES[j]));

						ComputeShaderSpec spec;
						adjustSpecForUntypedPointers(spec, specMap);
						adjustSpecForMemoryModel(spec, specMap, memModel);
						adjustSpecForDataTypes(spec, specMap, BASE_DATA_TYPE_CASES[k]);
						adjustSpecForCooperativeMatrix(spec, specMap);

						const tcu::StringTemplate tempShaderFunctions	= tcu::StringTemplate(shaderFunctions.specialize(specMap));

						std::string shaderVariablesStr	= shaderVariables.specialize(specMap);
						if (BASE_DATA_TYPE_CASES[k] != DataTypes::UINT32)
						{
							shaderVariablesStr	= "%uint32     = OpTypeInt  32      0\n"
												+ shaderVariablesStr;
						}

						const std::string shaderAsm =
							shaderHeader.specialize(specMap) +
							shaderAnnotations.specialize(specMap) +
							shaderVariablesStr +
							tempShaderFunctions.specialize(specMap);

						FilledResourceDesc desc;
						desc.dataType		= BASE_DATA_TYPE_CASES[k];
						desc.elemCount		= 1;
						desc.descriptorType	= vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
						desc.padding		= 0;
						desc.fillType		= FillingTypes::VALUE;
						desc.value			= 1;
						Resource input		= createFilledResource(desc);

						desc.dataType		= sameSizeTypes[l];
						Resource output		= createFilledResource(desc);

						spec.assembly		= shaderAsm;
						spec.numWorkGroups	= tcu::IVec3(1, 1, 1);
						spec.spirvVersion	= SPIRV_VERSION_1_6;	// cooperative matrices requires SPIR-V 1.6
						spec.inputs.push_back(input);
						spec.outputs.push_back(output);
						spec.extensions.push_back("VK_KHR_storage_buffer_storage_class");
						spec.extensions.push_back("VK_KHR_shader_untyped_pointers");

						layoutGroup->addChild(new SpvAsmComputeShaderCase(testCtx, testName.c_str(), spec));
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
		de::MovePtr<tcu::TestCaseGroup>	storeGroup(new tcu::TestCaseGroup(testCtx, "store", ""));

		tcu::StringTemplate shaderHeader(
			createShaderHeader("%input_data_var %output_data_untyped_var")
		);

		tcu::StringTemplate shaderAnnotations(
			createShaderAnnotations(CooperativeMatrixTestCases::TYPE_PUNNING_STORE)
		);

		tcu::StringTemplate shaderVariables(
			createShaderVariables(CooperativeMatrixTestCases::TYPE_PUNNING_STORE)
		);

		tcu::StringTemplate shaderFunctions(
			createShaderMain(CooperativeMatrixTestCases::TYPE_PUNNING_STORE)
		);

		for (deUint32 i = 0; i < DE_LENGTH_OF_ARRAY(MATRIX_USE_CASES); ++i)
		{
			de::MovePtr<tcu::TestCaseGroup>	useCaseGroup(new tcu::TestCaseGroup(testCtx, toString(MATRIX_USE_CASES[i]), ""));

			for (deUint32 j = 0; j < DE_LENGTH_OF_ARRAY(MATRIX_LAYOUT_CASES); ++j)
			{
				de::MovePtr<tcu::TestCaseGroup>	layoutGroup(new tcu::TestCaseGroup(testCtx, toString(MATRIX_LAYOUT_CASES[j]), ""));

				for (deUint32 k = 0; k < DE_LENGTH_OF_ARRAY(BASE_DATA_TYPE_CASES); ++k)
				{
					std::vector<DATA_TYPE> sameSizeTypes = getSameSizeBaseDataType(BASE_DATA_TYPE_CASES[k]);

					for (deUint32 l = 0; l < sameSizeTypes.size(); ++l)
					{
						std::string testName	= toString(BASE_DATA_TYPE_CASES[k]) + std::string("_to_") + toString(sameSizeTypes[l]);

						std::map<std::string, std::string>	specMap;
						specMap["baseDecl"]		= getDeclaration(BASE_DATA_TYPE_CASES[k]);
						specMap["baseType"]		= toString(BASE_DATA_TYPE_CASES[k]);
						specMap["sameSizeType"]	= toString(sameSizeTypes[l]);
						specMap["sameSizeDecl"]	= getDeclaration(sameSizeTypes[l]);
						specMap["typeSize"]		= std::to_string(getSizeInBytes(BASE_DATA_TYPE_CASES[k]));
						specMap["matrixUse"]	= std::to_string(getMatrixBinaryUse(MATRIX_USE_CASES[i]));
						specMap["matrixLayout"]	= std::to_string(getMatrixBinaryLayout(MATRIX_LAYOUT_CASES[j]));

						ComputeShaderSpec spec;
						adjustSpecForUntypedPointers(spec, specMap);
						adjustSpecForMemoryModel(spec, specMap, memModel);
						adjustSpecForDataTypes(spec, specMap, BASE_DATA_TYPE_CASES[k]);
						adjustSpecForCooperativeMatrix(spec, specMap);

						const tcu::StringTemplate tempShaderFunctions	= tcu::StringTemplate(shaderFunctions.specialize(specMap));

						std::string shaderVariablesStr	= shaderVariables.specialize(specMap);
						if (BASE_DATA_TYPE_CASES[k] != DataTypes::UINT32)
						{
							shaderVariablesStr	= "%uint32     = OpTypeInt  32      0\n"
												+ shaderVariablesStr;
						}

						const std::string shaderAsm =
							shaderHeader.specialize(specMap) +
							shaderAnnotations.specialize(specMap) +
							shaderVariablesStr +
							tempShaderFunctions.specialize(specMap);

						FilledResourceDesc desc;
						desc.dataType		= BASE_DATA_TYPE_CASES[k];
						desc.elemCount		= 1;
						desc.descriptorType	= vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
						desc.padding		= 0;
						desc.fillType		= FillingTypes::VALUE;
						desc.value			= 1;
						Resource input		= createFilledResource(desc);

						desc.dataType		= sameSizeTypes[l];
						Resource output		= createFilledResource(desc);

						spec.assembly		= shaderAsm;
						spec.numWorkGroups	= tcu::IVec3(1, 1, 1);
						spec.spirvVersion	= SPIRV_VERSION_1_6;	// cooperative matrices requires SPIR-V 1.6
						spec.inputs.push_back(input);
						spec.outputs.push_back(output);
						spec.extensions.push_back("VK_KHR_storage_buffer_storage_class");
						spec.extensions.push_back("VK_KHR_shader_untyped_pointers");

						layoutGroup->addChild(new SpvAsmComputeShaderCase(testCtx, testName.c_str(), spec));
					}
				}

				useCaseGroup->addChild(layoutGroup.release());
			}

			storeGroup->addChild(useCaseGroup.release());
		}

		testGroup->addChild(storeGroup.release());
	}
}

void addCooperativeMatrixInteractionMixedTests(tcu::TestCaseGroup* testGroup, MEMORY_MODEL_TYPE memModel)
{
tcu::TestContext& testCtx = testGroup->getTestContext();

	// Load tests
	{
		de::MovePtr<tcu::TestCaseGroup>	loadGroup(new tcu::TestCaseGroup(testCtx, "load", ""));

		tcu::StringTemplate shaderHeader(
			createShaderHeader("%input_data_untyped_var %output_data_var")
		);

		tcu::StringTemplate shaderAnnotations(
			createShaderAnnotations(CooperativeMatrixTestCases::MIXED_LOAD)
		);

		tcu::StringTemplate shaderVariables(
			createShaderVariables(CooperativeMatrixTestCases::MIXED_LOAD)
		);

		tcu::StringTemplate shaderFunctions(
			createShaderMain(CooperativeMatrixTestCases::MIXED_LOAD)
		);

		for (deUint32 i = 0; i < DE_LENGTH_OF_ARRAY(MATRIX_USE_CASES); ++i)
		{
			de::MovePtr<tcu::TestCaseGroup>	useCaseGroup(new tcu::TestCaseGroup(testCtx, toString(MATRIX_USE_CASES[i]), ""));

			for (deUint32 j = 0; j < DE_LENGTH_OF_ARRAY(MATRIX_LAYOUT_CASES); ++j)
			{
				de::MovePtr<tcu::TestCaseGroup>	layoutGroup(new tcu::TestCaseGroup(testCtx, toString(MATRIX_LAYOUT_CASES[j]), ""));

				for (deUint32 k = 0; k < DE_LENGTH_OF_ARRAY(BASE_DATA_TYPE_CASES); ++k)
				{
					std::string testName	= toString(BASE_DATA_TYPE_CASES[k]);
					std::string testDesc	= "Test load operation from untyped pointer to cooperative matrix for "
											+ std::string(toString(BASE_DATA_TYPE_CASES[k])) + " base data type.";

					std::map<std::string, std::string>	specMap;
					specMap["baseDecl"]		= getDeclaration(BASE_DATA_TYPE_CASES[k]);
					specMap["baseType"]		= toString(BASE_DATA_TYPE_CASES[k]);
					specMap["typeSize"]		= std::to_string(getSizeInBytes(BASE_DATA_TYPE_CASES[k]));
					specMap["matrixStride"]	= std::to_string(getSizeInBytes(BASE_DATA_TYPE_CASES[k]) * 2);
					specMap["matrixUse"]	= std::to_string(getMatrixBinaryUse(MATRIX_USE_CASES[i]));
					specMap["matrixLayout"]	= std::to_string(getMatrixBinaryLayout(MATRIX_LAYOUT_CASES[j]));

					ComputeShaderSpec spec;
					adjustSpecForUntypedPointers(spec, specMap);
					adjustSpecForMemoryModel(spec, specMap, memModel);
					adjustSpecForDataTypes(spec, specMap, BASE_DATA_TYPE_CASES[k]);
					adjustSpecForCooperativeMatrix(spec, specMap);

					const tcu::StringTemplate tempShaderFunctions	= tcu::StringTemplate(shaderFunctions.specialize(specMap));

					std::string shaderVariablesStr	= shaderVariables.specialize(specMap);
					if (BASE_DATA_TYPE_CASES[k] != DataTypes::UINT32)
					{
						shaderVariablesStr	= "%uint32     = OpTypeInt  32      0\n"
											+ shaderVariablesStr;
					}

					const std::string shaderAsm =
						shaderHeader.specialize(specMap) +
						shaderAnnotations.specialize(specMap) +
						shaderVariablesStr +
						tempShaderFunctions.specialize(specMap);

					FilledResourceDesc desc;
					desc.dataType		= BASE_DATA_TYPE_CASES[k];
					desc.elemCount		= 4;
					desc.descriptorType	= vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
					desc.padding		= 0;
					desc.fillType		= FillingTypes::VALUE;
					desc.value			= 1;

					Resource inputOutput	= createFilledResource(desc);

					spec.assembly		= shaderAsm;
					spec.numWorkGroups	= tcu::IVec3(4, 1, 1);
					spec.spirvVersion	= SPIRV_VERSION_1_6;	// cooperative matrices requires SPIR-V 1.6
					spec.inputs.push_back(inputOutput);
					spec.outputs.push_back(inputOutput);
					spec.extensions.push_back("VK_KHR_storage_buffer_storage_class");
					spec.extensions.push_back("VK_KHR_shader_untyped_pointers");

					layoutGroup->addChild(new SpvAsmComputeShaderCase(testCtx, testName.c_str(), spec));
				}

				useCaseGroup->addChild(layoutGroup.release());
			}

			loadGroup->addChild(useCaseGroup.release());
		}

		testGroup->addChild(loadGroup.release());
	}

	// Store tests
	{
		de::MovePtr<tcu::TestCaseGroup>	storeGroup(new tcu::TestCaseGroup(testCtx, "store", ""));

		tcu::StringTemplate shaderHeader(
			createShaderHeader("%input_data_var %output_data_untyped_var")
		);

		tcu::StringTemplate shaderAnnotations(
			createShaderAnnotations(CooperativeMatrixTestCases::MIXED_STORE)
		);

		tcu::StringTemplate shaderVariables(
			createShaderVariables(CooperativeMatrixTestCases::MIXED_STORE)
		);

		tcu::StringTemplate shaderFunctions(
			createShaderMain(CooperativeMatrixTestCases::MIXED_STORE)
		);

		for (deUint32 i = 0; i < DE_LENGTH_OF_ARRAY(MATRIX_USE_CASES); ++i)
		{
			de::MovePtr<tcu::TestCaseGroup>	useCaseGroup(new tcu::TestCaseGroup(testCtx, toString(MATRIX_USE_CASES[i]), ""));

			for (deUint32 j = 0; j < DE_LENGTH_OF_ARRAY(MATRIX_LAYOUT_CASES); ++j)
			{
				de::MovePtr<tcu::TestCaseGroup>	layoutGroup(new tcu::TestCaseGroup(testCtx, toString(MATRIX_LAYOUT_CASES[j]), ""));

				for (deUint32 k = 0; k < DE_LENGTH_OF_ARRAY(BASE_DATA_TYPE_CASES); ++k)
				{
					std::string testName	= toString(BASE_DATA_TYPE_CASES[k]);
					std::string testDesc	= "Test store operation from untyped pointer to cooperative matrix for "
											+ std::string(toString(BASE_DATA_TYPE_CASES[i])) + " base data type.";

					std::map<std::string, std::string>	specMap;
					specMap["baseDecl"]		= getDeclaration(BASE_DATA_TYPE_CASES[k]);
					specMap["baseType"]		= toString(BASE_DATA_TYPE_CASES[k]);
					specMap["typeSize"]		= std::to_string(getSizeInBytes(BASE_DATA_TYPE_CASES[k]));
					specMap["matrixStride"]	= std::to_string(getSizeInBytes(BASE_DATA_TYPE_CASES[k]) * 2);
					specMap["matrixUse"]	= std::to_string(getMatrixBinaryUse(MATRIX_USE_CASES[i]));
					specMap["matrixLayout"]	= std::to_string(getMatrixBinaryLayout(MATRIX_LAYOUT_CASES[j]));

					ComputeShaderSpec spec;
					adjustSpecForUntypedPointers(spec, specMap);
					adjustSpecForMemoryModel(spec, specMap, memModel);
					adjustSpecForDataTypes(spec, specMap, BASE_DATA_TYPE_CASES[k]);
					adjustSpecForCooperativeMatrix(spec, specMap);

					const tcu::StringTemplate tempShaderFunctions	= tcu::StringTemplate(shaderFunctions.specialize(specMap));

					std::string shaderVariablesStr	= shaderVariables.specialize(specMap);
					if (BASE_DATA_TYPE_CASES[k] != DataTypes::UINT32)
					{
						shaderVariablesStr	= "%uint32     = OpTypeInt  32      0\n"
											+ shaderVariablesStr;
					}

					const std::string shaderAsm =
						shaderHeader.specialize(specMap) +
						shaderAnnotations.specialize(specMap) +
						shaderVariablesStr +
						tempShaderFunctions.specialize(specMap);

					FilledResourceDesc desc;
					desc.dataType		= BASE_DATA_TYPE_CASES[k];
					desc.elemCount		= 4;
					desc.descriptorType	= vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
					desc.padding		= 0;
					desc.fillType		= FillingTypes::VALUE;
					desc.value			= 1;

					Resource inputOutput	= createFilledResource(desc);

					spec.assembly		= shaderAsm;
					spec.numWorkGroups	= tcu::IVec3(4, 1, 1);
					spec.spirvVersion	= SPIRV_VERSION_1_6;	// cooperative matrices requires SPIR-V 1.6
					spec.inputs.push_back(inputOutput);
					spec.outputs.push_back(inputOutput);
					spec.extensions.push_back("VK_KHR_storage_buffer_storage_class");
					spec.extensions.push_back("VK_KHR_shader_untyped_pointers");

					layoutGroup->addChild(new SpvAsmComputeShaderCase(testCtx, testName.c_str(), spec));
				}

				useCaseGroup->addChild(layoutGroup.release());
			}

			storeGroup->addChild(useCaseGroup.release());
		}

		testGroup->addChild(storeGroup.release());
	}
}

void addAtomicsTestGroup(tcu::TestCaseGroup* testGroup, MEMORY_MODEL_TYPE memModel)
{
	addTestGroup(testGroup, "load", addLoadAtomicTests, memModel);
	addTestGroup(testGroup, "store", addStoreAtomicTests, memModel);
	addTestGroup(testGroup, "add", addAtomicAddTests, memModel);
	addTestGroup(testGroup, "subtract", addAtomicSubtractTests, memModel);
	addTestGroup(testGroup, "increment", addAtomicIncrementDecrementTests, memModel, AtomicTestCases::OP_ATOMIC_INCREMENT);
	addTestGroup(testGroup, "decrement", addAtomicIncrementDecrementTests, memModel, AtomicTestCases::OP_ATOMIC_DECREMENT);
	addTestGroup(testGroup, "min", addAtomicMinMaxTests, memModel, AtomicTestCases::OP_ATOMIC_MIN);
	addTestGroup(testGroup, "max", addAtomicMinMaxTests, memModel, AtomicTestCases::OP_ATOMIC_MAX);
	addTestGroup(testGroup, "and", addAtomicBooleanTests, memModel, AtomicTestCases::OP_ATOMIC_AND);
	addTestGroup(testGroup, "or", addAtomicBooleanTests, memModel, AtomicTestCases::OP_ATOMIC_OR);
	addTestGroup(testGroup, "xor", addAtomicBooleanTests, memModel, AtomicTestCases::OP_ATOMIC_XOR);
	addTestGroup(testGroup, "exchange", addAtomicExchangeTests, memModel);
	addTestGroup(testGroup, "compare_exchange", addAtomicCompareExchangeTests, memModel);
}

void addPhysicalStorageOpBitcastTestGroup(tcu::TestCaseGroup* testGroup, MEMORY_MODEL_TYPE memModel)
{
	addTestGroup(testGroup, "from_untyped", addPhysicalStorageOpBitcastTests, memModel, DE_TRUE);
	addTestGroup(testGroup, "to_untyped", addPhysicalStorageOpBitcastTests, memModel, DE_FALSE);
}

void addCopyTestGroup(tcu::TestCaseGroup* testGroup, MEMORY_MODEL_TYPE memModel)
{
	addTestGroup(testGroup, "from_untyped", addCopyTests, memModel, DE_TRUE);
	addTestGroup(testGroup, "to_untyped", addCopyTests, memModel, DE_FALSE);
}

void addCopyMixedTypeTestGroup(tcu::TestCaseGroup* testGroup, MEMORY_MODEL_TYPE memModel)
{
	addTestGroup(testGroup, "from_untyped", addCopyFromUntypedMixedTypeTests, memModel);
	addTestGroup(testGroup, "to_untyped", addCopyToUntypedMixedTypeTests, memModel);
}

void addBasicUsecaseTestGroup(tcu::TestCaseGroup* testGroup, MEMORY_MODEL_TYPE memModel)
{
	addTestGroup(testGroup, "load", addLoadTests, memModel);
	addTestGroup(testGroup, "store", addStoreTests, memModel);
	addTestGroup(testGroup, "copy", addCopyTestGroup, memModel);
	addTestGroup(testGroup, "array_length", addOpArrayLengthTests, memModel);
	addTestGroup(testGroup, "atomics", addAtomicsTestGroup, memModel);
}

void addDataReinterpretTestGroup(tcu::TestCaseGroup* testGroup, MEMORY_MODEL_TYPE memModel)
{
	addTestGroup(testGroup, "struct_as_type", addStructAsTypeTests, memModel);
	addTestGroup(testGroup, "multiple_access_chains", addMultipleAccessChainTests, memModel);
}

void addTypePunningTestGroup(tcu::TestCaseGroup* testGroup, MEMORY_MODEL_TYPE memModel)
{
	addTestGroup(testGroup, "load", addLoadMixedTypeTests, memModel);
	addTestGroup(testGroup, "store", addStoreMixedTypeTests, memModel);
	addTestGroup(testGroup, "copy", addCopyMixedTypeTestGroup, memModel);
	addTestGroup(testGroup, "reinterpret", addDataReinterpretTestGroup, memModel);
}

void addPhysicalStorageBufferInteractionTestGroup(tcu::TestCaseGroup* testGroup, MEMORY_MODEL_TYPE memModel)
{
	addTestGroup(testGroup, "op_bitcast", addPhysicalStorageOpBitcastTestGroup, memModel);
	addTestGroup(testGroup, "op_select", addPhysicalStorageOpSelectTests, memModel);
	addTestGroup(testGroup, "op_phi", addPhysicalStorageOpPhiTests, memModel);
	addTestGroup(testGroup, "op_function_call", addPhysicalStorageOpFunctionCallTests, memModel);
	addTestGroup(testGroup, "op_ptr_access_chain", addPhysicalStorageOpPtrAccessChainTests, memModel);
}

void addVariablePointersInteractionTestGroup(tcu::TestCaseGroup* testGroup, MEMORY_MODEL_TYPE memModel)
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
}

void addWorkgroupMemoryInteractionTestGroup(tcu::TestCaseGroup* testGroup, MEMORY_MODEL_TYPE memModel)
{
	addTestGroup(testGroup, "aliased", addWorkgroupMemoryInteractionTests, memModel, WorkgroupTestCases::ALIASED);
	addTestGroup(testGroup, "not_aliased", addWorkgroupMemoryInteractionTests, memModel, WorkgroupTestCases::NOT_ALIASED);
}

void addCooperativeMatrixInteractionTestGroup(tcu::TestCaseGroup* testGroup, MEMORY_MODEL_TYPE memModel)
{
	addTestGroup(testGroup, "basic_usecase", addCooperativeMatrixInteractionBasicTests, memModel);
	addTestGroup(testGroup, "type_punning", addCooperativeMatrixInteractionTypePunningTests, memModel);
	addTestGroup(testGroup, "mixed", addCooperativeMatrixInteractionMixedTests, memModel);
}

void addVulkanMemoryModelTestGroup(tcu::TestCaseGroup* testGroup)
{
	addTestGroup(testGroup, "basic_usecase", addBasicUsecaseTestGroup, MemoryModelTypes::VULKAN);
	addTestGroup(testGroup, "type_punning", addTypePunningTestGroup, MemoryModelTypes::VULKAN);
	addTestGroup(testGroup, "variable_pointers", addVariablePointersInteractionTestGroup, MemoryModelTypes::VULKAN);
	addTestGroup(testGroup, "physical_storage", addPhysicalStorageBufferInteractionTestGroup, MemoryModelTypes::VULKAN);
	addTestGroup(testGroup, "workgroup_memory", addWorkgroupMemoryInteractionTestGroup, MemoryModelTypes::VULKAN);
	addTestGroup(testGroup, "cooperative_matrix", addCooperativeMatrixInteractionTestGroup, MemoryModelTypes::VULKAN);
}

void addGLSLMemoryModelTestGroup(tcu::TestCaseGroup* testGroup)
{
	addTestGroup(testGroup, "basic_usecase", addBasicUsecaseTestGroup, MemoryModelTypes::GLSL);
	addTestGroup(testGroup, "type_punning", addTypePunningTestGroup, MemoryModelTypes::GLSL);
	addTestGroup(testGroup, "variable_pointers", addVariablePointersInteractionTestGroup, MemoryModelTypes::GLSL);
	addTestGroup(testGroup, "physical_storage", addPhysicalStorageBufferInteractionTestGroup, MemoryModelTypes::GLSL);
	addTestGroup(testGroup, "workgroup_memory", addWorkgroupMemoryInteractionTestGroup, MemoryModelTypes::GLSL);
	addTestGroup(testGroup, "cooperative_matrix", addCooperativeMatrixInteractionTestGroup, MemoryModelTypes::GLSL);
}

tcu::TestCaseGroup* createUntypedPointersTestGroup(tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> untypedPointerTestGroup(new tcu::TestCaseGroup(testCtx, "untyped_pointers", "Tests for SPV_KHR_untyped_pointers extension tests."));

	untypedPointerTestGroup->addChild(createTestGroup(testCtx, "vulkan_memory_model", addVulkanMemoryModelTestGroup));
	untypedPointerTestGroup->addChild(createTestGroup(testCtx, "glsl_memory_model", addGLSLMemoryModelTestGroup));

	return untypedPointerTestGroup.release();
}
} // SpirVAssembly
} // vkt
