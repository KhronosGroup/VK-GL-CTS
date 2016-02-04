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
 * \file  vktSparseResourcesTests.cpp
 * \brief Sparse Resources Tests
 *//*--------------------------------------------------------------------*/

#include "vktSparseResourcesTests.hpp"
#include "vktSparseResourcesBufferSparseBinding.hpp"
#include "vktSparseResourcesImageSparseBinding.hpp"
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

	return sparseTests.release();
}

} // sparse
} // vkt
