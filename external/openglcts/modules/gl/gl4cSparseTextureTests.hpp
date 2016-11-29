#ifndef _GL4CSPARSETEXTURETESTS_HPP
#define _GL4CSPARSETEXTURETESTS_HPP
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
 */ /*!
 * \file
 * \brief
 */ /*-------------------------------------------------------------------*/

/**
 */ /*!
 * \file  gl4cSparseTextureTests.hpp
 * \brief Conformance tests for the GL_ARB_sparse_texture functionality.
 */ /*-------------------------------------------------------------------*/
#include "glcTestCase.hpp"
#include "glwDefs.hpp"
#include "glwEnums.hpp"
#include "tcuDefs.hpp"
#include <vector>

#include "gluTextureUtil.hpp"
#include "tcuTextureUtil.hpp"

namespace gl4cts
{
class SparseTextureUtils
{
public:
	static bool verifyQueryError(tcu::TestContext& testCtx, const char* funcName, glw::GLint target, glw::GLint pname,
								 glw::GLint error, glw::GLint expectedError);

	static bool verifyError(tcu::TestContext& testCtx, const char* funcName, glw::GLint error,
							glw::GLint expectedError);

	static glw::GLint getTargetDepth(glw::GLint target);

	static void getTexturePageSizes(const glw::Functions& gl, glw::GLint target, glw::GLint format,
									glw::GLint& pageSizeX, glw::GLint& pageSizeY, glw::GLint& pageSizeZ);
};

/** Represents texture static helper
 **/
class Texture
{
public:
	/* Public static routines */
	/* Functionality */
	static void Bind(const glw::Functions& gl, glw::GLuint id, glw::GLenum target);

	static void Generate(const glw::Functions& gl, glw::GLuint& out_id);

	static void Delete(const glw::Functions& gl, glw::GLuint& id);

	static void Storage(const glw::Functions& gl, glw::GLenum target, glw::GLsizei levels, glw::GLenum internal_format,
						glw::GLuint width, glw::GLuint height, glw::GLuint depth);

	static void SubImage(const glw::Functions& gl, glw::GLenum target, glw::GLint level, glw::GLint x, glw::GLint y,
						 glw::GLint z, glw::GLsizei width, glw::GLsizei height, glw::GLsizei depth, glw::GLenum format,
						 glw::GLenum type, const glw::GLvoid* pixels);

	/* Public fields */
	glw::GLuint m_id;

	/* Public constants */
	static const glw::GLuint m_invalid_id;
};

/** Test verifies TexParameter{if}{v}, TexParameterI{u}v, GetTexParameter{if}v
 * and GetTexParameterIi{u}v queries for <pname>:
 *   - TEXTURE_SPARSE_ARB,
 *   - VIRTUAL_PAGE_SIZE_INDEX_ARB.
 * Test verifies also GetTexParameter{if}v and GetTexParameterIi{u}v queries for <pname>:
 *   - NUM_SPARSE_LEVELS_ARB
 **/
class TextureParameterQueriesTestCase : public deqp::TestCase
{
public:
	/* Public methods */
	TextureParameterQueriesTestCase(deqp::Context& context);

	void						 init();
	tcu::TestNode::IterateResult iterate();

private:
	/* Private members */
	std::vector<glw::GLint> mSupportedTargets;
	std::vector<glw::GLint> mNotSupportedTargets;

	/* Private methods */
	bool testTextureSparseARB(const glw::Functions& gl, glw::GLint target, glw::GLint expectedError = GL_NO_ERROR);
	bool testVirtualPageSizeIndexARB(const glw::Functions& gl, glw::GLint target,
									 glw::GLint expectedError = GL_NO_ERROR);
	bool testNumSparseLevelsARB(const glw::Functions& gl, glw::GLint target);

	bool checkGetTexParameter(const glw::Functions& gl, glw::GLint target, glw::GLint pname, glw::GLint expected);
};

/** Test verifies GetInternalformativ query for formats from Table 8.12 and <pname>:
 *   - NUM_VIRTUAL_PAGE_SIZES_ARB,
 *   - VIRTUAL_PAGE_SIZE_X_ARB,
 *   - VIRTUAL_PAGE_SIZE_Y_ARB,
 *   - VIRTUAL_PAGE_SIZE_Z_ARB.
 **/
class InternalFormatQueriesTestCase : public deqp::TestCase
{
public:
	/* Public methods */
	InternalFormatQueriesTestCase(deqp::Context& context);

	void						 init();
	tcu::TestNode::IterateResult iterate();

private:
	/* Private methods */
	std::vector<glw::GLint> mSupportedTargets;
	std::vector<glw::GLint> mSupportedInternalFormats;

	/* Private members */
};

/** Test verifies GetIntegerv, GetFloatv, GetDoublev, GetInteger64v,
 * and GetBooleanv queries for <pname>:
 *   - MAX_SPARSE_TEXTURE_SIZE_ARB,
 *   - MAX_SPARSE_3D_TEXTURE_SIZE_ARB,
 *   - MAX_SPARSE_ARRAY_TEXTURE_LAYERS_ARB,
 *   - SPARSE_TEXTURE_FULL_ARRAY_CUBE_MIPMAPS_ARB.
 **/
class SimpleQueriesTestCase : public deqp::TestCase
{
public:
	/* Public methods */
	SimpleQueriesTestCase(deqp::Context& context);

	tcu::TestNode::IterateResult iterate();

private:
	/* Private methods */
	/* Private members */
	void testSipmleQueries(const glw::Functions& gl, glw::GLint pname);
};

/** Test verifies glTexStorage* functionality added by ARB_sparse_texture extension
 **/
class SparseTextureAllocationTestCase : public deqp::TestCase
{
public:
	/* Public methods */
	SparseTextureAllocationTestCase(deqp::Context& context);

	SparseTextureAllocationTestCase(deqp::Context& context, const char* name, const char* description);

	virtual void						 init();
	virtual tcu::TestNode::IterateResult iterate();

protected:
	/* Protected methods */
	std::vector<glw::GLint> mSupportedTargets;
	std::vector<glw::GLint> mFullArrayTargets;
	std::vector<glw::GLint> mSupportedInternalFormats;

	/* Protected members */
	void positiveTesting(const glw::Functions& gl, glw::GLint target, glw::GLint format);
	bool verifyTexParameterErrors(const glw::Functions& gl, glw::GLint target, glw::GLint format);
	bool verifyTexStorageVirtualPageSizeIndexError(const glw::Functions& gl, glw::GLint target, glw::GLint format);
	bool verifyTexStorageFullArrayCubeMipmapsError(const glw::Functions& gl, glw::GLint target, glw::GLint format);
	bool verifyTexStorageInvalidValueErrors(const glw::Functions& gl, glw::GLint target, glw::GLint format);
};

/** Test verifies glTexPageCommitmentARB functionality added by ARB_sparse_texture extension
 **/
class SparseTextureCommitmentTestCase : public deqp::TestCase
{
public:
	/* Public methods */
	SparseTextureCommitmentTestCase(deqp::Context& context);

	SparseTextureCommitmentTestCase(deqp::Context& context, const char* name, const char* description);

	virtual void						 init();
	virtual tcu::TestNode::IterateResult iterate();

protected:
	/* Protected members */
	std::vector<glw::GLint> mSupportedTargets;
	std::vector<glw::GLint> mSupportedInternalFormats;

	glw::GLint		   mWidthStored;
	glw::GLint		   mHeightStored;
	glw::GLint		   mDepthStored;
	tcu::TextureFormat mTextureFormatStored;

	/* Protected methods */
	void texPageCommitment(const glw::Functions& gl, glw::GLint target, glw::GLint format, glw::GLuint& texture,
						   glw::GLint level, glw::GLint xOffset, glw::GLint yOffset, glw::GLint zOffset,
						   glw::GLint width, glw::GLint height, glw::GLint depth, glw::GLboolean committ);

	bool prepareTexture(const glw::Functions& gl, glw::GLint target, glw::GLint format, glw::GLuint& texture);
	bool sparseAllocateTexture(const glw::Functions& gl, glw::GLint target, glw::GLint format, glw::GLuint& texture);
	bool allocateTexture(const glw::Functions& gl, glw::GLint target, glw::GLint format, glw::GLuint& texture);
	bool writeDataToTexture(const glw::Functions& gl, glw::GLint target, glw::GLint format, glw::GLuint& texture);
	bool verifyTextureData(const glw::Functions& gl, glw::GLint target, glw::GLint format, glw::GLuint& texture);
	bool commitTexturePage(const glw::Functions& gl, glw::GLint target, glw::GLint format, glw::GLuint& texture);

	bool verifyInvalidOperationErrors(const glw::Functions& gl, glw::GLint target, glw::GLint format,
									  glw::GLuint& texture);
	bool verifyInvalidValueErrors(const glw::Functions& gl, glw::GLint target, glw::GLint format, glw::GLuint& texture);
};

/** Test verifies glTexturePageCommitmentEXT functionality added by ARB_sparse_texture extension
 **/
class SparseDSATextureCommitmentTestCase : public SparseTextureCommitmentTestCase
{
public:
	/* Public methods */
	SparseDSATextureCommitmentTestCase(deqp::Context& context);

	virtual tcu::TestNode::IterateResult iterate();

private:
	/* Private methods */
	virtual void texPageCommitment(const glw::Functions& gl, glw::GLint target, glw::GLint format, glw::GLuint& texture,
								   glw::GLint level, glw::GLint xOffset, glw::GLint yOffset, glw::GLint zOffset,
								   glw::GLint width, glw::GLint height, glw::GLint depth, glw::GLboolean committ);
};

/** Test group which encapsulates all sparse texture conformance tests */
class SparseTextureTests : public deqp::TestCaseGroup
{
public:
	/* Public methods */
	SparseTextureTests(deqp::Context& context);

	void init();

private:
	SparseTextureTests(const SparseTextureTests& other);
	SparseTextureTests& operator=(const SparseTextureTests& other);
};

} /* glcts namespace */

#endif // _GL4CSPARSETEXTURETESTS_HPP
