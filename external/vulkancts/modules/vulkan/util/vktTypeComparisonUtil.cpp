/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2021 The Khronos Group Inc.
 * Copyright (c) 2021 Google LLC.
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
 * \brief Utility functions for generating comparison code for values with different types.
 *//*--------------------------------------------------------------------*/

#include "gluShaderUtil.hpp"
#include "gluVarTypeUtil.hpp"
#include <set>

namespace vkt
{
namespace typecomputil
{

const char* getCompareFuncForType (glu::DataType type)
{
	switch (type)
	{
		case glu::TYPE_FLOAT:
			return "bool compare_float    (highp float a, highp float b)  { return abs(a - b) < 0.05; }\n";
		case glu::TYPE_FLOAT_VEC2:
			return "bool compare_vec2     (highp vec2 a, highp vec2 b)    { return compare_float(a.x, b.x)&&compare_float(a.y, b.y); }\n";
		case glu::TYPE_FLOAT_VEC3:
			return "bool compare_vec3     (highp vec3 a, highp vec3 b)    { return compare_float(a.x, b.x)&&compare_float(a.y, b.y)&&compare_float(a.z, b.z); }\n";
		case glu::TYPE_FLOAT_VEC4:
			return "bool compare_vec4     (highp vec4 a, highp vec4 b)    { return compare_float(a.x, b.x)&&compare_float(a.y, b.y)&&compare_float(a.z, b.z)&&compare_float(a.w, b.w); }\n";
		case glu::TYPE_FLOAT_MAT2:
			return "bool compare_mat2     (highp mat2 a, highp mat2 b)    { return compare_vec2(a[0], b[0])&&compare_vec2(a[1], b[1]); }\n";
		case glu::TYPE_FLOAT_MAT2X3:
			return "bool compare_mat2x3   (highp mat2x3 a, highp mat2x3 b){ return compare_vec3(a[0], b[0])&&compare_vec3(a[1], b[1]); }\n";
		case glu::TYPE_FLOAT_MAT2X4:
			return "bool compare_mat2x4   (highp mat2x4 a, highp mat2x4 b){ return compare_vec4(a[0], b[0])&&compare_vec4(a[1], b[1]); }\n";
		case glu::TYPE_FLOAT_MAT3X2:
			return "bool compare_mat3x2   (highp mat3x2 a, highp mat3x2 b){ return compare_vec2(a[0], b[0])&&compare_vec2(a[1], b[1])&&compare_vec2(a[2], b[2]); }\n";
		case glu::TYPE_FLOAT_MAT3:
			return "bool compare_mat3     (highp mat3 a, highp mat3 b)    { return compare_vec3(a[0], b[0])&&compare_vec3(a[1], b[1])&&compare_vec3(a[2], b[2]); }\n";
		case glu::TYPE_FLOAT_MAT3X4:
			return "bool compare_mat3x4   (highp mat3x4 a, highp mat3x4 b){ return compare_vec4(a[0], b[0])&&compare_vec4(a[1], b[1])&&compare_vec4(a[2], b[2]); }\n";
		case glu::TYPE_FLOAT_MAT4X2:
			return "bool compare_mat4x2   (highp mat4x2 a, highp mat4x2 b){ return compare_vec2(a[0], b[0])&&compare_vec2(a[1], b[1])&&compare_vec2(a[2], b[2])&&compare_vec2(a[3], b[3]); }\n";
		case glu::TYPE_FLOAT_MAT4X3:
			return "bool compare_mat4x3   (highp mat4x3 a, highp mat4x3 b){ return compare_vec3(a[0], b[0])&&compare_vec3(a[1], b[1])&&compare_vec3(a[2], b[2])&&compare_vec3(a[3], b[3]); }\n";
		case glu::TYPE_FLOAT_MAT4:
			return "bool compare_mat4     (highp mat4 a, highp mat4 b)    { return compare_vec4(a[0], b[0])&&compare_vec4(a[1], b[1])&&compare_vec4(a[2], b[2])&&compare_vec4(a[3], b[3]); }\n";
		case glu::TYPE_INT:
			return "bool compare_int      (highp int a, highp int b)      { return a == b; }\n";
		case glu::TYPE_INT_VEC2:
			return "bool compare_ivec2    (highp ivec2 a, highp ivec2 b)  { return a == b; }\n";
		case glu::TYPE_INT_VEC3:
			return "bool compare_ivec3    (highp ivec3 a, highp ivec3 b)  { return a == b; }\n";
		case glu::TYPE_INT_VEC4:
			return "bool compare_ivec4    (highp ivec4 a, highp ivec4 b)  { return a == b; }\n";
		case glu::TYPE_UINT:
			return "bool compare_uint     (highp uint a, highp uint b)    { return a == b; }\n";
		case glu::TYPE_UINT_VEC2:
			return "bool compare_uvec2    (highp uvec2 a, highp uvec2 b)  { return a == b; }\n";
		case glu::TYPE_UINT_VEC3:
			return "bool compare_uvec3    (highp uvec3 a, highp uvec3 b)  { return a == b; }\n";
		case glu::TYPE_UINT_VEC4:
			return "bool compare_uvec4    (highp uvec4 a, highp uvec4 b)  { return a == b; }\n";
		case glu::TYPE_BOOL:
			return "bool compare_bool     (bool a, bool b)                { return a == b; }\n";
		case glu::TYPE_BOOL_VEC2:
			return "bool compare_bvec2    (bvec2 a, bvec2 b)              { return a == b; }\n";
		case glu::TYPE_BOOL_VEC3:
			return "bool compare_bvec3    (bvec3 a, bvec3 b)              { return a == b; }\n";
		case glu::TYPE_BOOL_VEC4:
			return "bool compare_bvec4    (bvec4 a, bvec4 b)              { return a == b; }\n";
		case glu::TYPE_FLOAT16:
			return "bool compare_float16_t(highp float a, highp float b)  { return abs(a - b) < 0.05; }\n";
		case glu::TYPE_FLOAT16_VEC2:
			return "bool compare_f16vec2  (highp vec2 a, highp vec2 b)    { return compare_float(a.x, b.x)&&compare_float(a.y, b.y); }\n";
		case glu::TYPE_FLOAT16_VEC3:
			return "bool compare_f16vec3  (highp vec3 a, highp vec3 b)    { return compare_float(a.x, b.x)&&compare_float(a.y, b.y)&&compare_float(a.z, b.z); }\n";
		case glu::TYPE_FLOAT16_VEC4:
			return "bool compare_f16vec4  (highp vec4 a, highp vec4 b)    { return compare_float(a.x, b.x)&&compare_float(a.y, b.y)&&compare_float(a.z, b.z)&&compare_float(a.w, b.w); }\n";
		case glu::TYPE_INT8:
			return "bool compare_int8_t   (highp int a, highp int b)      { return a == b; }\n";
		case glu::TYPE_INT8_VEC2:
			return "bool compare_i8vec2   (highp ivec2 a, highp ivec2 b)  { return a == b; }\n";
		case glu::TYPE_INT8_VEC3:
			return "bool compare_i8vec3   (highp ivec3 a, highp ivec3 b)  { return a == b; }\n";
		case glu::TYPE_INT8_VEC4:
			return "bool compare_i8vec4   (highp ivec4 a, highp ivec4 b)  { return a == b; }\n";
		case glu::TYPE_UINT8:
			return "bool compare_uint8_t  (highp uint a, highp uint b)    { return a == b; }\n";
		case glu::TYPE_UINT8_VEC2:
			return "bool compare_u8vec2   (highp uvec2 a, highp uvec2 b)  { return a == b; }\n";
		case glu::TYPE_UINT8_VEC3:
			return "bool compare_u8vec3   (highp uvec3 a, highp uvec3 b)  { return a == b; }\n";
		case glu::TYPE_UINT8_VEC4:
			return "bool compare_u8vec4   (highp uvec4 a, highp uvec4 b)  { return a == b; }\n";
		case glu::TYPE_INT16:
			return "bool compare_int16_t  (highp int a, highp int b)      { return a == b; }\n";
		case glu::TYPE_INT16_VEC2:
			return "bool compare_i16vec2  (highp ivec2 a, highp ivec2 b)  { return a == b; }\n";
		case glu::TYPE_INT16_VEC3:
			return "bool compare_i16vec3  (highp ivec3 a, highp ivec3 b)  { return a == b; }\n";
		case glu::TYPE_INT16_VEC4:
			return "bool compare_i16vec4  (highp ivec4 a, highp ivec4 b)  { return a == b; }\n";
		case glu::TYPE_UINT16:
			return "bool compare_uint16_t (highp uint a, highp uint b)    { return a == b; }\n";
		case glu::TYPE_UINT16_VEC2:
			return "bool compare_u16vec2  (highp uvec2 a, highp uvec2 b)  { return a == b; }\n";
		case glu::TYPE_UINT16_VEC3:
			return "bool compare_u16vec3  (highp uvec3 a, highp uvec3 b)  { return a == b; }\n";
		case glu::TYPE_UINT16_VEC4:
			return "bool compare_u16vec4  (highp uvec4 a, highp uvec4 b)  { return a == b; }\n";
		default:
			DE_ASSERT(false);
			return DE_NULL;
	}
}

void getCompareDependencies (std::set<glu::DataType> &compareFuncs, glu::DataType basicType)
{
	switch (basicType)
	{
		case glu::TYPE_FLOAT_VEC2:
		case glu::TYPE_FLOAT_VEC3:
		case glu::TYPE_FLOAT_VEC4:
		case glu::TYPE_FLOAT16_VEC2:
		case glu::TYPE_FLOAT16_VEC3:
		case glu::TYPE_FLOAT16_VEC4:
			compareFuncs.insert(glu::TYPE_FLOAT);
			compareFuncs.insert(basicType);
			break;

		case glu::TYPE_FLOAT_MAT2:
		case glu::TYPE_FLOAT_MAT2X3:
		case glu::TYPE_FLOAT_MAT2X4:
		case glu::TYPE_FLOAT_MAT3X2:
		case glu::TYPE_FLOAT_MAT3:
		case glu::TYPE_FLOAT_MAT3X4:
		case glu::TYPE_FLOAT_MAT4X2:
		case glu::TYPE_FLOAT_MAT4X3:
		case glu::TYPE_FLOAT_MAT4:
			compareFuncs.insert(glu::TYPE_FLOAT);
			compareFuncs.insert(glu::getDataTypeFloatVec(glu::getDataTypeMatrixNumRows(basicType)));
			compareFuncs.insert(basicType);
			break;

		default:
			compareFuncs.insert(basicType);
			break;
	}
}

void collectUniqueBasicTypes (std::set<glu::DataType> &basicTypes, const glu::VarType &type)
{
	if (type.isStructType())
	{
		for (const auto &iter: *type.getStructPtr())
			collectUniqueBasicTypes(basicTypes, iter.getType());
	}
	else if (type.isArrayType())
		collectUniqueBasicTypes(basicTypes, type.getElementType());
	else
	{
		DE_ASSERT(type.isBasicType());
		basicTypes.insert(type.getBasicType());
	}
}

glu::DataType getPromoteType (glu::DataType type)
{
	switch (type)
	{
		case glu::TYPE_UINT8:
			return glu::TYPE_UINT;
		case glu::TYPE_UINT8_VEC2:
			return glu::TYPE_UINT_VEC2;
		case glu::TYPE_UINT8_VEC3:
			return glu::TYPE_UINT_VEC3;
		case glu::TYPE_UINT8_VEC4:
			return glu::TYPE_UINT_VEC4;
		case glu::TYPE_INT8:
			return glu::TYPE_INT;
		case glu::TYPE_INT8_VEC2:
			return glu::TYPE_INT_VEC2;
		case glu::TYPE_INT8_VEC3:
			return glu::TYPE_INT_VEC3;
		case glu::TYPE_INT8_VEC4:
			return glu::TYPE_INT_VEC4;
		case glu::TYPE_UINT16:
			return glu::TYPE_UINT;
		case glu::TYPE_UINT16_VEC2:
			return glu::TYPE_UINT_VEC2;
		case glu::TYPE_UINT16_VEC3:
			return glu::TYPE_UINT_VEC3;
		case glu::TYPE_UINT16_VEC4:
			return glu::TYPE_UINT_VEC4;
		case glu::TYPE_INT16:
			return glu::TYPE_INT;
		case glu::TYPE_INT16_VEC2:
			return glu::TYPE_INT_VEC2;
		case glu::TYPE_INT16_VEC3:
			return glu::TYPE_INT_VEC3;
		case glu::TYPE_INT16_VEC4:
			return glu::TYPE_INT_VEC4;
		case glu::TYPE_FLOAT16:
			return glu::TYPE_FLOAT;
		case glu::TYPE_FLOAT16_VEC2:
			return glu::TYPE_FLOAT_VEC2;
		case glu::TYPE_FLOAT16_VEC3:
			return glu::TYPE_FLOAT_VEC3;
		case glu::TYPE_FLOAT16_VEC4:
			return glu::TYPE_FLOAT_VEC4;
		default:
			return type;
	}
}
} // typecomputil
} // vkt
