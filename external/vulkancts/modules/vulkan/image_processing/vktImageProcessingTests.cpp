/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2025 The Khronos Group Inc.
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
 * \brief Image Processing Tests
 *//*--------------------------------------------------------------------*/

#include "vktImageProcessingTests.hpp"
#include "vktImageProcessingApiTests.hpp"
#include "vktImageProcessingBlockMatchingTests.hpp"
#include "vktTestGroupUtil.hpp"

#include "vkPipelineConstructionUtil.hpp"

#include <string>

using namespace vk;
namespace vkt
{
namespace ImageProcessing
{

namespace
{
using GroupPtr = de::MovePtr<tcu::TestCaseGroup>;

void createChildren(tcu::TestCaseGroup *imageProcessingTests)
{
    tcu::TestContext &testCtx = imageProcessingTests->getTestContext();

    {
        GroupPtr graphicsGroup(new tcu::TestCaseGroup(testCtx, "graphics"));

        const struct
        {
            PipelineConstructionType constructionType;
            const char *name;
        } constructionTypes[] = {{PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC, "monolithic"},
                                 {PIPELINE_CONSTRUCTION_TYPE_FAST_LINKED_LIBRARY, "fast_lib"},
                                 {PIPELINE_CONSTRUCTION_TYPE_SHADER_OBJECT_UNLINKED_SPIRV, "shader_objects"}};

        for (const auto &pipelineConstructionType : constructionTypes)
        {
            GroupPtr gfxPipelineGroup(new tcu::TestCaseGroup(testCtx, pipelineConstructionType.name));
            gfxPipelineGroup->addChild(
                createImageProcessingBlockMatchingGraphicsTests(testCtx, pipelineConstructionType.constructionType));
            graphicsGroup->addChild(gfxPipelineGroup.release());
        }

        imageProcessingTests->addChild(graphicsGroup.release());
    }

    {
        imageProcessingTests->addChild(createImageProcessingApiTests(testCtx));
    }

    // Compute tests
    {
        GroupPtr computeGroup(new tcu::TestCaseGroup(testCtx, "compute"));
        computeGroup->addChild(createImageProcessingBlockMatchingComputeTests(testCtx));
        imageProcessingTests->addChild(computeGroup.release());
    }
}

} // namespace

tcu::TestCaseGroup *createTests(tcu::TestContext &testCtx, const std::string &name)
{
    return createTestGroup(testCtx, name.c_str(), createChildren);
}

} // namespace ImageProcessing
} // namespace vkt
