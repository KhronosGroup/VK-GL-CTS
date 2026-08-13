/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2025 LunarG, Inc.
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
 * \brief Shader Object Independent Sets Tests
 *//*--------------------------------------------------------------------*/

#include "vktShaderObjectIndependentSetsTests.hpp"
#include "vktIndependentSetsUtil.hpp"

namespace vkt
{
namespace ShaderObject
{

using namespace vk;

tcu::TestCaseGroup *createShaderObjectIndependentSetsTests(tcu::TestContext &testCtx)
{
    const auto groupName = "m11_independent_sets";

    const std::vector<PipelineConstructionType> constructionTypes{
        PIPELINE_CONSTRUCTION_TYPE_SHADER_OBJECT_UNLINKED_SPIRV,
        PIPELINE_CONSTRUCTION_TYPE_SHADER_OBJECT_UNLINKED_BINARY,
        PIPELINE_CONSTRUCTION_TYPE_SHADER_OBJECT_LINKED_SPIRV,
        PIPELINE_CONSTRUCTION_TYPE_SHADER_OBJECT_LINKED_BINARY,
    };

    return IndependentSets::createRandomTests(testCtx, groupName, constructionTypes);
}

} // namespace ShaderObject
} // namespace vkt
