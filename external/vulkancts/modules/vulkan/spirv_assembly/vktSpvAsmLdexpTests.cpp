/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2025 The Khronos Group Inc.
 * Copyright (c) 2025 Valve Corporation.
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
 * \brief SPIR-V tests for the ldexp operation.
 *//*--------------------------------------------------------------------*/

#include "vktSpvAsmLdexpTests.hpp"
#include "vktAmberTestCase.hpp"

#include "deUniquePtr.hpp"

namespace vkt
{
namespace SpirVAssembly
{

tcu::TestCaseGroup *createLdexpGroup(tcu::TestContext &testCtx)
{
    de::MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(testCtx, "ldexp"));

    struct LdexpCase
    {
        const char *testName;
        const std::vector<std::string> testRequirements;
    };

    const LdexpCase caseList[] = {
        {"ldexp_f16vec2_i16vec2",
         {"Float16Int8Features.shaderFloat16", "Features.shaderInt16", "Storage16BitFeatures.storageBuffer16BitAccess",
          "Storage16BitFeatures.uniformAndStorageBuffer16BitAccess"}},
        {"ldexp_f16vec2_i32vec2",
         {"Float16Int8Features.shaderFloat16", "Storage16BitFeatures.storageBuffer16BitAccess",
          "Storage16BitFeatures.uniformAndStorageBuffer16BitAccess"}},
        {"ldexp_f16vec2_i64vec2",
         {"Float16Int8Features.shaderFloat16", "Features.shaderInt64", "Storage16BitFeatures.storageBuffer16BitAccess",
          "Storage16BitFeatures.uniformAndStorageBuffer16BitAccess"}},
        {"ldexp_f16vec2_i8vec2",
         {"Float16Int8Features.shaderFloat16", "Float16Int8Features.shaderInt8",
          "Storage16BitFeatures.storageBuffer16BitAccess", "Storage16BitFeatures.uniformAndStorageBuffer16BitAccess",
          "Storage8BitFeatures.uniformAndStorageBuffer8BitAccess"}},
        {"ldexp_f16vec4_i16vec4",
         {"Float16Int8Features.shaderFloat16", "Features.shaderInt16", "Storage16BitFeatures.storageBuffer16BitAccess",
          "Storage16BitFeatures.uniformAndStorageBuffer16BitAccess"}},
        {"ldexp_f16vec4_i32vec4",
         {"Float16Int8Features.shaderFloat16", "Storage16BitFeatures.storageBuffer16BitAccess",
          "Storage16BitFeatures.uniformAndStorageBuffer16BitAccess"}},
        {"ldexp_f16vec4_i64vec4",
         {"Float16Int8Features.shaderFloat16", "Features.shaderInt64", "Storage16BitFeatures.storageBuffer16BitAccess",
          "Storage16BitFeatures.uniformAndStorageBuffer16BitAccess"}},
        {"ldexp_f16vec4_i8vec4",
         {"Float16Int8Features.shaderFloat16", "Float16Int8Features.shaderInt8",
          "Storage16BitFeatures.storageBuffer16BitAccess", "Storage16BitFeatures.uniformAndStorageBuffer16BitAccess",
          "Storage8BitFeatures.uniformAndStorageBuffer8BitAccess"}},
        {"ldexp_f32vec2_i16vec2",
         {"Features.shaderInt16", "Storage16BitFeatures.storageBuffer16BitAccess",
          "Storage16BitFeatures.uniformAndStorageBuffer16BitAccess"}},
        {"ldexp_f32vec2_i32vec2", {}},
        {"ldexp_f32vec2_i64vec2", {"Features.shaderInt64"}},
        {"ldexp_f32vec2_i8vec2",
         {"Float16Int8Features.shaderInt8", "Storage8BitFeatures.uniformAndStorageBuffer8BitAccess"}},
        {"ldexp_f32vec4_i16vec4",
         {"Features.shaderInt16", "Storage16BitFeatures.storageBuffer16BitAccess",
          "Storage16BitFeatures.uniformAndStorageBuffer16BitAccess"}},
        {"ldexp_f32vec4_i32vec4", {}},
        {"ldexp_f32vec4_i64vec4", {"Features.shaderInt64"}},
        {"ldexp_f32vec4_i8vec4",
         {"Float16Int8Features.shaderInt8", "Storage8BitFeatures.uniformAndStorageBuffer8BitAccess"}},
        {"ldexp_f64vec2_i16vec2",
         {"Features.shaderFloat64", "Features.shaderInt16", "Storage16BitFeatures.storageBuffer16BitAccess",
          "Storage16BitFeatures.uniformAndStorageBuffer16BitAccess"}},
        {"ldexp_f64vec2_i32vec2", {"Features.shaderFloat64"}},
        {"ldexp_f64vec2_i64vec2", {"Features.shaderFloat64", "Features.shaderInt64"}},
        {"ldexp_f64vec2_i8vec2",
         {"Features.shaderFloat64", "Float16Int8Features.shaderInt8",
          "Storage8BitFeatures.uniformAndStorageBuffer8BitAccess"}},
        {"ldexp_f64vec4_i16vec4",
         {"Features.shaderFloat64", "Features.shaderInt16", "Storage16BitFeatures.storageBuffer16BitAccess",
          "Storage16BitFeatures.uniformAndStorageBuffer16BitAccess"}},
        {"ldexp_f64vec4_i32vec4", {"Features.shaderFloat64"}},
        {"ldexp_f64vec4_i64vec4", {"Features.shaderFloat64", "Features.shaderInt64"}},
        {"ldexp_f64vec4_i8vec4",
         {"Features.shaderFloat64", "Float16Int8Features.shaderInt8",
          "Storage8BitFeatures.uniformAndStorageBuffer8BitAccess"}},
        {"ldexp_float16_int16",
         {"Float16Int8Features.shaderFloat16", "Features.shaderInt16", "Storage16BitFeatures.storageBuffer16BitAccess",
          "Storage16BitFeatures.uniformAndStorageBuffer16BitAccess"}},
        {"ldexp_float16_int32",
         {"Float16Int8Features.shaderFloat16", "Storage16BitFeatures.storageBuffer16BitAccess",
          "Storage16BitFeatures.uniformAndStorageBuffer16BitAccess"}},
        {"ldexp_float16_int64",
         {"Float16Int8Features.shaderFloat16", "Features.shaderInt64", "Storage16BitFeatures.storageBuffer16BitAccess",
          "Storage16BitFeatures.uniformAndStorageBuffer16BitAccess"}},
        {"ldexp_float16_int8",
         {"Float16Int8Features.shaderFloat16", "Float16Int8Features.shaderInt8",
          "Storage16BitFeatures.storageBuffer16BitAccess", "Storage16BitFeatures.uniformAndStorageBuffer16BitAccess",
          "Storage8BitFeatures.uniformAndStorageBuffer8BitAccess"}},
        {"ldexp_float32_int16",
         {"Features.shaderInt16", "Storage16BitFeatures.storageBuffer16BitAccess",
          "Storage16BitFeatures.uniformAndStorageBuffer16BitAccess"}},
        {"ldexp_float32_int32", {}},
        {"ldexp_float32_int64", {"Features.shaderInt64"}},
        {"ldexp_float32_int8",
         {"Float16Int8Features.shaderInt8", "Storage8BitFeatures.uniformAndStorageBuffer8BitAccess"}},
        {"ldexp_float64_int16",
         {"Features.shaderFloat64", "Features.shaderInt16", "Storage16BitFeatures.storageBuffer16BitAccess",
          "Storage16BitFeatures.uniformAndStorageBuffer16BitAccess"}},
        {"ldexp_float64_int32", {"Features.shaderFloat64"}},
        {"ldexp_float64_int64", {"Features.shaderFloat64", "Features.shaderInt64"}},
        {"ldexp_float64_int8",
         {"Features.shaderFloat64", "Float16Int8Features.shaderInt8",
          "Storage8BitFeatures.uniformAndStorageBuffer8BitAccess"}},
    };

    const auto category = "ldexp"; // Subdirectory in the amber test case base data dir.

    const auto makeFileName = [](const char *testName) { return std::string(testName) + ".amber"; };

    for (const auto &ldexpCase : caseList)
    {
        const auto fileName = makeFileName(ldexpCase.testName);
        group->addChild(cts_amber::createAmberTestCase(testCtx, ldexpCase.testName, category, fileName,
                                                       ldexpCase.testRequirements));
    }

    return group.release();
}

} // namespace SpirVAssembly
} // namespace vkt
