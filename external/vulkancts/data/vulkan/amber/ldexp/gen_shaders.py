#!/usr/bin/env python
#
# Copyright (c) 2025 The Khronos Group Inc.
# Copyright (c) 2025 Valve Corporation.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
import sys

TEMPLATE_NAME = 'template.glsl'
with open(TEMPLATE_NAME, 'r') as stream:
    template = stream.read()

COMPONENT_COUNTS = [1, 2, 4]

FLOAT16 = ['float16_t', 'f16vec2', 'f16vec3', 'f16vec4']
FLOAT32 = ['float32_t', 'f32vec2', 'f32vec3', 'f32vec4']
FLOAT64 = ['float64_t', 'f64vec2', 'f64vec3', 'f64vec4']

INT8  = ['int8_t', 'i8vec2', 'i8vec3', 'i8vec4']
INT16 = ['int16_t', 'i16vec2', 'i16vec3', 'i16vec4']
INT32 = ['int32_t', 'i32vec2', 'i32vec3', 'i32vec4']
INT64 = ['int64_t', 'i64vec2', 'i64vec3', 'i64vec4']

for component_count in COMPONENT_COUNTS:
    type_index = component_count - 1
    for significand_type in (FLOAT16[type_index], FLOAT32[type_index], FLOAT64[type_index]):
        for exponent_type in (INT8[type_index], INT16[type_index], INT32[type_index], INT64[type_index]):
            shader_code = template.format(significand_type=significand_type, exponent_type=exponent_type)
            file_name = 'ldexp_{significand_type}_{exponent_type}.glsl'.format(significand_type=significand_type, exponent_type=exponent_type)
            with open(file_name, 'w') as stream:
                stream.write(shader_code)

sys.exit()
