/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2024 The Khronos Group Inc.
 * Copyright (c) 2024 Valve Corporation.
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
 * \brief Device Generated Commands Tests
 *//*--------------------------------------------------------------------*/

#include "vktDGCTests.hpp"

#include "vktDGCComputeGetInfoTests.hpp"
#include "vktDGCPropertyTests.hpp"
#include "vktDGCComputeSmokeTests.hpp"
#include "vktDGCComputeLayoutTests.hpp"
#include "vktDGCComputeMiscTests.hpp"
#include "vktDGCComputePreprocessTests.hpp"
#include "vktDGCComputeSubgroupTests.hpp"
#include "vktDGCComputeConditionalTests.hpp"

#include "vktDGCComputeGetInfoTestsExt.hpp"
#include "vktDGCPropertyTestsExt.hpp"
#include "vktDGCComputeSmokeTestsExt.hpp"
#include "vktDGCComputeLayoutTestsExt.hpp"
#include "vktDGCComputeMiscTestsExt.hpp"
#include "vktDGCComputePreprocessTestsExt.hpp"
#include "vktDGCComputeSubgroupTestsExt.hpp"
#include "vktDGCComputeConditionalTestsExt.hpp"

#include "vktDGCGraphicsDrawTestsExt.hpp"
#include "vktDGCGraphicsDrawCountTestsExt.hpp"
#include "vktDGCGraphicsConditionalTestsExt.hpp"
#include "vktDGCGraphicsMeshTestsExt.hpp"
#include "vktDGCGraphicsMiscTestsExt.hpp"
#include "vktDGCGraphicsXfbTestsExt.hpp"
#include "vktDGCGraphicsTessStateTestsExt.hpp"

#include "vktDGCRayTracingTestsExt.hpp"

#include "deUniquePtr.hpp"

namespace vkt
{
namespace DGC
{

namespace
{
using GroupPtr = de::MovePtr<tcu::TestCaseGroup>;
}

tcu::TestCaseGroup *createTests(tcu::TestContext &testCtx, const std::string &name)
{
    GroupPtr mainGroup(new tcu::TestCaseGroup(testCtx, name.c_str()));

    GroupPtr nvGroup(new tcu::TestCaseGroup(testCtx, "nv"));
    GroupPtr extGroup(new tcu::TestCaseGroup(testCtx, "ext"));

    GroupPtr nvComputeGroup(new tcu::TestCaseGroup(testCtx, "compute"));
    GroupPtr nvMiscGroup(new tcu::TestCaseGroup(testCtx, "misc"));

    GroupPtr extComputeGroup(new tcu::TestCaseGroup(testCtx, "compute"));
    GroupPtr extMiscGroup(new tcu::TestCaseGroup(testCtx, "misc"));
    GroupPtr extGraphicsGroup(new tcu::TestCaseGroup(testCtx, "graphics"));

    nvComputeGroup->addChild(createDGCComputeGetInfoTests(testCtx));
    nvComputeGroup->addChild(createDGCComputeSmokeTests(testCtx));
    nvComputeGroup->addChild(createDGCComputeLayoutTests(testCtx));
    nvComputeGroup->addChild(createDGCComputeMiscTests(testCtx));
    nvComputeGroup->addChild(createDGCComputePreprocessTests(testCtx));
    nvComputeGroup->addChild(createDGCComputeSubgroupTests(testCtx));
    nvComputeGroup->addChild(createDGCComputeConditionalTests(testCtx));

    nvMiscGroup->addChild(createDGCPropertyTests(testCtx));

    nvGroup->addChild(nvComputeGroup.release());
    nvGroup->addChild(nvMiscGroup.release());

    extComputeGroup->addChild(createDGCComputeGetInfoTestsExt(testCtx));
    extComputeGroup->addChild(createDGCComputeSmokeTestsExt(testCtx));
    extComputeGroup->addChild(createDGCComputeLayoutTestsExt(testCtx));
    extComputeGroup->addChild(createDGCComputeMiscTestsExt(testCtx));
    extComputeGroup->addChild(createDGCComputePreprocessTestsExt(testCtx));
    extComputeGroup->addChild(createDGCComputeSubgroupTestsExt(testCtx));
    extComputeGroup->addChild(createDGCComputeConditionalTestsExt(testCtx));

    extMiscGroup->addChild(createDGCPropertyTestsExt(testCtx));

    extGraphicsGroup->addChild(createDGCGraphicsDrawTestsExt(testCtx));
    extGraphicsGroup->addChild(createDGCGraphicsDrawCountTestsExt(testCtx));
    extGraphicsGroup->addChild(createDGCGraphicsConditionalTestsExt(testCtx));
    extGraphicsGroup->addChild(createDGCGraphicsMeshTestsExt(testCtx));
    extGraphicsGroup->addChild(createDGCGraphicsMiscTestsExt(testCtx));
    extGraphicsGroup->addChild(createDGCGraphicsXfbTestsExt(testCtx));
    extGraphicsGroup->addChild(createDGCGraphicsTessStateTestsExt(testCtx));

    extGroup->addChild(extComputeGroup.release());
    extGroup->addChild(extMiscGroup.release());
    extGroup->addChild(extGraphicsGroup.release());
    extGroup->addChild(createDGCRayTracingTestsExt(testCtx));

    mainGroup->addChild(nvGroup.release());
    mainGroup->addChild(extGroup.release());

    return mainGroup.release();
}

} // namespace DGC
} // namespace vkt
