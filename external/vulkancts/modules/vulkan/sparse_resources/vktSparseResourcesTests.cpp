/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2016 The Khronos Group Inc.
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
 * \file  vktSparseResourcesTests.cpp
 * \brief Sparse Resources Tests
 *//*--------------------------------------------------------------------*/

#include "vktSparseResourcesTests.hpp"
#include "vktSparseResourcesBufferSparseBinding.hpp"
#include "vktSparseResourcesImageSparseBinding.hpp"
#include "vktSparseResourcesBufferSparseResidency.hpp"
#include "vktSparseResourcesImageSparseResidency.hpp"
#include "vktSparseResourcesMipmapSparseResidency.hpp"
#include "vktSparseResourcesBufferMemoryAliasing.hpp"
#include "deUniquePtr.hpp"

namespace vkt
{
namespace sparse
{

tcu::TestCaseGroup* createTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> sparseTests (new tcu::TestCaseGroup(testCtx, "sparse_resources", "Sparse Resources Tests"));

	sparseTests->addChild(createBufferSparseBindingTests(testCtx));
	sparseTests->addChild(createImageSparseBindingTests(testCtx));
	sparseTests->addChild(createBufferSparseResidencyTests(testCtx));
	sparseTests->addChild(createImageSparseResidencyTests(testCtx));
	sparseTests->addChild(createMipmapSparseResidencyTests(testCtx));
	sparseTests->addChild(createBufferSparseMemoryAliasingTests(testCtx));

	return sparseTests.release();
}

} // sparse
} // vkt
