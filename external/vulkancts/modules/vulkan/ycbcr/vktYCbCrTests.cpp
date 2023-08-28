/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2017 Google Inc.
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
 * \brief YCbCr Conversion Tests
 *//*--------------------------------------------------------------------*/

#include "vktYCbCrTests.hpp"
#include "vktTestGroupUtil.hpp"
#include "vktYCbCrFormatTests.hpp"
#include "vktYCbCrFilteringTests.hpp"
#include "vktYCbCrViewTests.hpp"
#include "vktYCbCrImageQueryTests.hpp"
#include "vktYCbCrConversionTests.hpp"
#include "vktYCbCrCopyTests.hpp"
#include "vktYCbCrStorageImageWriteTests.hpp"
#include "vktYCbCrImageOffsetTests.hpp"

namespace vkt
{
namespace ycbcr
{

namespace
{

void populateTestGroup (tcu::TestCaseGroup* ycbcrTests)
{
	tcu::TestContext&	testCtx		= ycbcrTests->getTestContext();

	ycbcrTests->addChild(createFormatTests(testCtx));
	ycbcrTests->addChild(createFilteringTests(testCtx));
	ycbcrTests->addChild(createViewTests(testCtx));
	ycbcrTests->addChild(createImageQueryTests(testCtx));
	ycbcrTests->addChild(createConversionTests(testCtx));
	ycbcrTests->addChild(createCopyTests(testCtx));
	ycbcrTests->addChild(createDimensionsCopyTests(testCtx));
	ycbcrTests->addChild(createStorageImageWriteTests(testCtx));
	ycbcrTests->addChild(createImageOffsetTests(testCtx));
}

} // anonymous

tcu::TestCaseGroup* createTests (tcu::TestContext& testCtx, const std::string& name)
{
	return createTestGroup(testCtx, name.c_str(), "YCbCr Conversion Tests", populateTestGroup);
}

} // ycbcr
} // vkt
