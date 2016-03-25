/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2016 The Khronos Group Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and/or associated documentation files (the
 * "Materials"), to deal in the Materials without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Materials, and to
 * permit persons to whom the Materials are furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice(s) and this permission notice shall be included
 * in all copies or substantial portions of the Materials.
 *
 * THE MATERIALS ARE PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * MATERIALS OR THE USE OR OTHER DEALINGS IN THE MATERIALS.
 *
 *//*!
 * \file
 * \brief Image Tests
 *//*--------------------------------------------------------------------*/

#include "vktImageTests.hpp"
#include "vktImageLoadStoreTests.hpp"
#include "vktImageQualifiersTests.hpp"
#include "vktImageSizeTests.hpp"
#include "vktTestGroupUtil.hpp"
#include "vktImageAtomicOperationTests.hpp"

namespace vkt
{
namespace image
{

namespace
{

void createChildren (tcu::TestCaseGroup* imageTests)
{
	tcu::TestContext&	testCtx		= imageTests->getTestContext();
	
	imageTests->addChild(createImageStoreTests(testCtx));
	imageTests->addChild(createImageLoadStoreTests(testCtx));
	imageTests->addChild(createImageFormatReinterpretTests(testCtx));
	imageTests->addChild(createImageQualifiersTests(testCtx));
	imageTests->addChild(createImageSizeTests(testCtx));
	imageTests->addChild(createImageAtomicOperationTests(testCtx));
}

} // anonymous

tcu::TestCaseGroup* createTests (tcu::TestContext& testCtx)
{
	return createTestGroup(testCtx, "image", "Image tests", createChildren);
}

} // image
} // vkt
