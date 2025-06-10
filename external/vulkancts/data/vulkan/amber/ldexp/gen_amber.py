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
import re
import math

# These are fixed.
significands = [
    -1.0, -1.0, -1.0, -1.0,
    -1.0, -1.0, -1.0, -1.0,
     0.5,  0.5,  0.5,  0.5,
     0.5,  0.5,  0.5,  0.5,
    1.25, 1.25, 1.25, 1.25,
    1.25, 1.25, 1.25, 1.25,
     2.0,  2.0,  2.0,  2.0,
     2.0,  2.0,  2.0,  2.0,
]

initial_results = [
    .0, .0, .0, .0, .0, .0, .0, .0,
    .0, .0, .0, .0, .0, .0, .0, .0,
    .0, .0, .0, .0, .0, .0, .0, .0,
    .0, .0, .0, .0, .0, .0, .0, .0,
]

fixed_exponents = [1, 0, -1, -2]
optional_exponents = [-14, -126, -1022]
target_exponent_count = 8
exponent_replicas = len(set(significands)) # To pair each exponent with reach significand.

def get_component_count_from_type_name(type_name):
    if 'vec' in type_name:
        return int(type_name[-1])
    return 1

def get_bits_from_type_name(type_name):
    match_obj = re.search(r'^[a-z]*?([0-9]+).*', type_name)
    if match_obj is not None:
        return int(match_obj.group(1))
    return 0

def min_exponent(bit_count):
    return {16: -14, 32: -126, 64: -1022}[bit_count]

def bits_min(bit_count):
    return -(2**(bit_count - 1))

def get_amber_float_type(bit_count):
    return {16: 'float16', 32: 'float', 64: 'double'}[bit_count]

def get_amber_int_type(bit_count):
    return 'int' + str(bit_count)

def read_full_file(file_name):
    with open(file_name, 'r') as stream:
        contents = stream.read()
    return contents

def get_features(asm):
    caps = re.findall(r'OpCapability \w+', asm)
    caps = [x.split()[1] for x in caps]
    caps = [x for x in caps if x != 'Shader']
    cap_to_feature = {
        'Float16': ['Float16Int8Features.shaderFloat16'],
        'Float64': ['shaderFloat64'],
        'Int16': ['shaderInt16'],
        'Int64': ['shaderInt64'],
        'Int8': ['Float16Int8Features.shaderInt8'],
        'StorageBuffer16BitAccess': ['Storage16BitFeatures.storageBuffer16BitAccess', 'Storage16BitFeatures.uniformAndStorageBuffer16BitAccess'],
        'UniformAndStorageBuffer8BitAccess': ['Storage8BitFeatures.uniformAndStorageBuffer8BitAccess'],
    }
    features = []
    for c in caps:
        cap_features = cap_to_feature[c]
        for f in cap_features:
            features.append('DEVICE_FEATURE %s' % (f,))
    return '\n'.join(features)

amber_template = read_full_file('template.amber')

for arg_idx in range(1, len(sys.argv), 1):
    spirv_asm = read_full_file(sys.argv[arg_idx])
    device_features = get_features(spirv_asm)

    file_name = sys.argv[arg_idx]
    simplified_name = file_name.replace('_t', '')
    match_obj = re.match(r'^ldexp_(.*?)_(.*?).glsl.spv.spvasm', simplified_name)

    if match_obj is None:
        print('%s does not match the expected file name' % (file_name, ), file=sys.stderr)
        continue

    significand_type = match_obj.group(1)
    exponent_type = match_obj.group(2)

    component_count = get_component_count_from_type_name(significand_type)
    inv_count = int(len(significands) / component_count)

    significand_bits = get_bits_from_type_name(significand_type)
    exponent_bits = get_bits_from_type_name(exponent_type)

    if significand_bits == 0 or exponent_bits == 0:
        print('Unknown bits in significand or exponent: (%s, %s)' % (significand_bits, exponent_bits), file=sys.stderr)
        continue

    smallest_int_exponent = bits_min(exponent_bits) # For the integer exponent operand in the ldexp call.
    smallest_float_exponent = min_exponent(significand_bits) # According to the exponent bits in the float type.

    # Try exponents in the limits.
    used_exponents = [x for x in fixed_exponents]
    for opt_exp in optional_exponents:
        if opt_exp > smallest_int_exponent and opt_exp > smallest_float_exponent:
            used_exponents.append(opt_exp)

    # Try really small exponents, only taking into account the integer exponent operand.
    # Results when using these exponents may be flushed to zero according to the spec.
    # At the same time, the upper part of these numbers is all zeros except in the most significant bit.
    # If they get truncated by mistake, it results in a positive exponent.
    while len(used_exponents) < target_exponent_count:
        missing = target_exponent_count - len(used_exponents)
        used_exponents.append(smallest_int_exponent + missing + 2) # +2 to make them non-obvious.

    # Repeat exponents multipe times to combine them with each significand.
    replica = [x for x in used_exponents]
    for replica_idx in range(0, exponent_replicas - 1):
        used_exponents.extend(replica)

    amber_float_type = get_amber_float_type(significand_bits)
    amber_int_type = get_amber_int_type(exponent_bits)

    expects = []
    for (idx, significand) in enumerate(significands):
        exponent = used_exponents[idx]
        result = math.ldexp(significand, exponent)
        offset = int((significand_bits * idx) / 8) # in bytes
        expect = 'EXPECT results IDX %s TOLERANCE .0001 EQ %s\n' % (offset, result)
        expects.append(expect)
    expects_str = ''.join(expects)

    amber_contents = amber_template.format(device_features=device_features,
                                           spirv_asm=spirv_asm,
                                           significands='\n'.join(str(x) for x in significands),
                                           exponents='\n'.join(str(x) for x in used_exponents),
                                           initial_results='\n'.join(str(x) for x in initial_results),
                                           count=inv_count,
                                           amber_float_type=amber_float_type,
                                           amber_int_type=amber_int_type,
                                           expects=expects_str)

    output_file_name = 'ldexp_%s_%s.amber' % (significand_type, exponent_type)
    with open(output_file_name, 'w') as stream:
        stream.write(amber_contents)

