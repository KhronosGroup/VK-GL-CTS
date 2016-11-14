#ifndef _GL4CSPARSETEXTURE2TESTS_HPP
#define _GL4CSPARSETEXTURE2TESTS_HPP
/*-------------------------------------------------------------------------
 * OpenGL Conformance Test Suite
 * -----------------------------
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
 * \file
 * \brief
 *//*--------------------------------------------------------------------*/

/**
 */ /*!
 * \file  gl4cSparseTexture2Tests.hpp
 * \brief Conformance tests for the GL_ARB_sparse_texture2 functionality.
 */ /*--------------------------------------------------------------------*/
#include "glcTestCase.hpp"
#include "glwDefs.hpp"
#include "tcuDefs.hpp"
#include <map>
#include <vector>

#include "gluTextureUtil.hpp"
#include "tcuTextureUtil.hpp"

#include "gl4cSparseTextureTests.hpp"

namespace gl4cts
{

struct PageSizeStruct
{
	glw::GLint xSize;
	glw::GLint ySize;
	glw::GLint zSize;

	PageSizeStruct() : xSize(0), ySize(0), zSize(0)
	{
	}
	PageSizeStruct(glw::GLint x, glw::GLint y, glw::GLint z) : xSize(x), ySize(y), zSize(z)
	{
	}
};

typedef std::pair<glw::GLint, PageSizeStruct> PageSizePair;

/** Test verifies if values returned by GetInternalFormat* query matches Standard Virtual Page Sizes for <pname>:
 *   - VIRTUAL_PAGE_SIZE_X_ARB,
 *   - VIRTUAL_PAGE_SIZE_Y_ARB,
 *   - VIRTUAL_PAGE_SIZE_Z_ARB.
 **/
class StandardPageSizesTestCase : public deqp::TestCase
{
public:
	/* Public methods */
	StandardPageSizesTestCase(deqp::Context& context);

	void						 init();
	tcu::TestNode::IterateResult iterate();

private:
	/* Private methods */
	std::vector<glw::GLint> mSupportedTargets;
	std::map<glw::GLint, PageSizeStruct> mStandardVirtualPageSizesTable;

	/* Private members */
};

/** Test verifies glTexStorage* functionality added by ARB_sparse_texture2 extension
 **/
class SparseTexture2AllocationTestCase : public SparseTextureAllocationTestCase
{
public:
	/* Public methods */
	SparseTexture2AllocationTestCase(deqp::Context& context);

	virtual void init();
	//virtual tcu::TestNode::IterateResult iterate();

private:
	/* Private methods */
};

/** Test group which encapsulates all sparse texture conformance tests */
class SparseTexture2Tests : public deqp::TestCaseGroup
{
public:
	/* Public methods */
	SparseTexture2Tests(deqp::Context& context);

	void init();

private:
	SparseTexture2Tests(const SparseTexture2Tests& other);
	SparseTexture2Tests& operator=(const SparseTexture2Tests& other);
};

} /* glcts namespace */

#endif // _GL4CSPARSETEXTURE2TESTS_HPP
