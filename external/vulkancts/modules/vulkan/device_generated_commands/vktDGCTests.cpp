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

#include "deUniquePtr.hpp"

namespace vkt
{
namespace DGC
{

namespace
{
using GroupPtr = de::MovePtr<tcu::TestCaseGroup>;
}

tcu::TestCaseGroup*	createTests	(tcu::TestContext& testCtx, const std::string& name)
{
	GroupPtr mainGroup		(new tcu::TestCaseGroup(testCtx, name.c_str()));
	GroupPtr nvGroup		(new tcu::TestCaseGroup(testCtx, "nv"));
	GroupPtr computeGroup	(new tcu::TestCaseGroup(testCtx, "compute"));
	GroupPtr miscGroup		(new tcu::TestCaseGroup(testCtx, "misc"));

	computeGroup->addChild(createDGCComputeGetInfoTests(testCtx));
	computeGroup->addChild(createDGCComputeSmokeTests(testCtx));
	computeGroup->addChild(createDGCComputeLayoutTests(testCtx));
	computeGroup->addChild(createDGCComputeMiscTests(testCtx));
	computeGroup->addChild(createDGCComputePreprocessTests(testCtx));
	computeGroup->addChild(createDGCComputeSubgroupTests(testCtx));
	computeGroup->addChild(createDGCComputeConditionalTests(testCtx));

	miscGroup->addChild(createDGCPropertyTests(testCtx));

	nvGroup->addChild(computeGroup.release());
	nvGroup->addChild(miscGroup.release());
	mainGroup->addChild(nvGroup.release());

	return mainGroup.release();
}

} // DGC
} // vkt
