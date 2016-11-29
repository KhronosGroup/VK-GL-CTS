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
 * \file  gl4cSparseTextureTests.cpp
 * \brief Conformance tests for the GL_ARB_sparse_texture functionality.
 */ /*-------------------------------------------------------------------*/

#include "gl4cSparseTextureTests.hpp"
#include "gluContextInfo.hpp"
#include "gluDefs.hpp"
#include "glwEnums.hpp"
#include "glwFunctions.hpp"
#include "tcuTestLog.hpp"

#include <cmath>
#include <cstdlib>
#include <string.h>
#include <vector>

using namespace glw;
using namespace glu;

namespace gl4cts
{

typedef std::pair<GLint, GLint> IntPair;

/** Verifies last query error and generate proper log message
 *
 * @param funcName         Verified function name
 * @param target           Target for which texture is binded
 * @param pname            Parameter name
 * @param error            Generated error code
 * @param expectedError    Expected error code
 *
 * @return Returns true if queried value is as expected, returns false otherwise
 */
bool SparseTextureUtils::verifyQueryError(tcu::TestContext& testCtx, const char* funcName, GLint target, GLint pname,
										  GLint error, GLint expectedError)
{
	if (error != expectedError)
	{
		testCtx.getLog() << tcu::TestLog::Message << funcName << " return wrong error code"
						 << ", target: " << target << ", pname: " << pname << ", expected: " << expectedError
						 << ", returned: " << error << tcu::TestLog::EndMessage;

		return false;
	}

	return true;
}

/** Verifies last operation error and generate proper log message
 *
 * @param funcName         Verified function name
 * @param mesage           Error message
 * @param error            Generated error code
 * @param expectedError    Expected error code
 *
 * @return Returns true if queried value is as expected, returns false otherwise
 */
bool SparseTextureUtils::verifyError(tcu::TestContext& testCtx, const char* funcName, GLint error, GLint expectedError)
{
	if (error != expectedError)
	{
		testCtx.getLog() << tcu::TestLog::Message << funcName << " return wrong error code "
						 << ", expectedError: " << expectedError << ", returnedError: " << error
						 << tcu::TestLog::EndMessage;

		return false;
	}

	return true;
}

/** Get minimal depth value for target
 *
 * @param target   Texture target
 *
 * @return Returns depth value
 */
GLint SparseTextureUtils::getTargetDepth(GLint target)
{
	GLint depth;

	if (target == GL_TEXTURE_3D || target == GL_TEXTURE_2D_ARRAY || target == GL_TEXTURE_CUBE_MAP_ARRAY)
	{
		depth = 6;
	}
	else
		depth = 1;

	return depth;
}

/** Queries for virtual page sizes
 *
 * @param gl           GL functions
 * @param target       Texture target
 * @param format       Texture internal format
 * @param pageSizeX    Texture page size reference for X dimension
 * @param pageSizeY    Texture page size reference for X dimension
 * @param pageSizeZ    Texture page size reference for X dimension
 **/
void SparseTextureUtils::getTexturePageSizes(const glw::Functions& gl, glw::GLint target, glw::GLint format,
											 glw::GLint& pageSizeX, glw::GLint& pageSizeY, glw::GLint& pageSizeZ)
{
	gl.getInternalformativ(target, format, GL_VIRTUAL_PAGE_SIZE_X_ARB, sizeof(pageSizeX), &pageSizeX);
	GLU_EXPECT_NO_ERROR(gl.getError(), "getInternalformativ error occurred for GL_VIRTUAL_PAGE_SIZE_X_ARB");

	gl.getInternalformativ(target, format, GL_VIRTUAL_PAGE_SIZE_Y_ARB, sizeof(pageSizeY), &pageSizeY);
	GLU_EXPECT_NO_ERROR(gl.getError(), "getInternalformativ error occurred for GL_VIRTUAL_PAGE_SIZE_Y_ARB");

	gl.getInternalformativ(target, format, GL_VIRTUAL_PAGE_SIZE_Z_ARB, sizeof(pageSizeZ), &pageSizeZ);
	GLU_EXPECT_NO_ERROR(gl.getError(), "getInternalformativ error occurred for GL_VIRTUAL_PAGE_SIZE_Z_ARB");
}

/* Texture static fields */
const GLuint Texture::m_invalid_id = -1;

/** Bind texture to target
 *
 * @param gl       GL API functions
 * @param id       Id of texture
 * @param tex_type Type of texture
 **/
void Texture::Bind(const Functions& gl, GLuint id, GLenum target)
{
	gl.bindTexture(target, id);
	GLU_EXPECT_NO_ERROR(gl.getError(), "BindTexture");
}

/** Generate texture instance
 *
 * @param gl     GL functions
 * @param out_id Id of texture
 **/
void Texture::Generate(const Functions& gl, GLuint& out_id)
{
	GLuint id = m_invalid_id;

	gl.genTextures(1, &id);
	GLU_EXPECT_NO_ERROR(gl.getError(), "GenTextures");

	if (m_invalid_id == id)
	{
		TCU_FAIL("Invalid id");
	}

	out_id = id;
}

/** Delete texture instance
 *
 * @param gl    GL functions
 * @param id    Id of texture
 **/
void Texture::Delete(const Functions& gl, GLuint& id)
{
	gl.deleteTextures(1, &id);
	GLU_EXPECT_NO_ERROR(gl.getError(), "GenTextures");
}

/** Allocate storage for texture
 *
 * @param gl              GL functions
 * @param target          Texture target
 * @param levels          Number of levels
 * @param internal_format Internal format of texture
 * @param width           Width of texture
 * @param height          Height of texture
 * @param depth           Depth of texture
 **/
void Texture::Storage(const Functions& gl, GLenum target, GLsizei levels, GLenum internal_format, GLuint width,
					  GLuint height, GLuint depth)
{
	switch (target)
	{
	case GL_TEXTURE_1D:
		gl.texStorage1D(target, levels, internal_format, width);
		break;
	case GL_TEXTURE_1D_ARRAY:
	case GL_TEXTURE_2D:
	case GL_TEXTURE_RECTANGLE:
	case GL_TEXTURE_CUBE_MAP:
		gl.texStorage2D(target, levels, internal_format, width, height);
		break;
	case GL_TEXTURE_2D_MULTISAMPLE:
		gl.texStorage2DMultisample(target, levels, internal_format, width, height, GL_FALSE);
		break;
	case GL_TEXTURE_3D:
	case GL_TEXTURE_2D_ARRAY:
	case GL_TEXTURE_CUBE_MAP_ARRAY:
		gl.texStorage3D(target, levels, internal_format, width, height, depth);
		break;
	default:
		TCU_FAIL("Invliad enum");
		break;
	}
}

/** Set contents of texture
 *
 * @param gl              GL functions
 * @param target          Texture target
 * @param level           Mipmap level
 * @param x               X offset
 * @param y               Y offset
 * @param z               Z offset
 * @param width           Width of texture
 * @param height          Height of texture
 * @param depth           Depth of texture
 * @param format          Format of data
 * @param type            Type of data
 * @param pixels          Buffer with image data
 **/
void Texture::SubImage(const glw::Functions& gl, glw::GLenum target, glw::GLint level, glw::GLint x, glw::GLint y,
					   glw::GLint z, glw::GLsizei width, glw::GLsizei height, glw::GLsizei depth, glw::GLenum format,
					   glw::GLenum type, const glw::GLvoid* pixels)
{
	switch (target)
	{
	case GL_TEXTURE_1D:
		gl.texSubImage1D(target, level, x, width, format, type, pixels);
		break;
	case GL_TEXTURE_1D_ARRAY:
	case GL_TEXTURE_2D:
	case GL_TEXTURE_RECTANGLE:
		gl.texSubImage2D(target, level, x, y, width, height, format, type, pixels);
		break;
	case GL_TEXTURE_CUBE_MAP:
		gl.texSubImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, level, x, y, width, height, format, type, pixels);
		gl.texSubImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, level, x, y, width, height, format, type, pixels);
		gl.texSubImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Y, level, x, y, width, height, format, type, pixels);
		gl.texSubImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, level, x, y, width, height, format, type, pixels);
		gl.texSubImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, level, x, y, width, height, format, type, pixels);
		gl.texSubImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, level, x, y, width, height, format, type, pixels);
		break;
	case GL_TEXTURE_3D:
	case GL_TEXTURE_2D_ARRAY:
	case GL_TEXTURE_CUBE_MAP_ARRAY:
		gl.texSubImage3D(target, level, x, y, z, width, height, depth, format, type, pixels);
		break;
	default:
		TCU_FAIL("Invliad enum");
		break;
	}
}

/** Constructor.
 *
 *  @param context     Rendering context
 */
TextureParameterQueriesTestCase::TextureParameterQueriesTestCase(deqp::Context& context)
	: TestCase(
		  context, "TextureParameterQueries",
		  "Implements all glTexParameter* and glGetTexParameter* queries tests described in CTS_ARB_sparse_texture")
{
	/* Left blank intentionally */
}

/** Stub init method */
void TextureParameterQueriesTestCase::init()
{
	mSupportedTargets.push_back(GL_TEXTURE_2D);
	mSupportedTargets.push_back(GL_TEXTURE_2D_ARRAY);
	mSupportedTargets.push_back(GL_TEXTURE_CUBE_MAP);
	mSupportedTargets.push_back(GL_TEXTURE_CUBE_MAP_ARRAY);
	mSupportedTargets.push_back(GL_TEXTURE_3D);
	mSupportedTargets.push_back(GL_TEXTURE_RECTANGLE);

	if (!m_context.getContextInfo().isExtensionSupported("GL_ARB_sparse_texture2"))
	{
		mNotSupportedTargets.push_back(GL_TEXTURE_1D);
		mNotSupportedTargets.push_back(GL_TEXTURE_1D_ARRAY);
		mNotSupportedTargets.push_back(GL_TEXTURE_2D_MULTISAMPLE);
		mNotSupportedTargets.push_back(GL_TEXTURE_2D_MULTISAMPLE_ARRAY);
	}
	else
	{
		mNotSupportedTargets.push_back(GL_TEXTURE_1D);
		mNotSupportedTargets.push_back(GL_TEXTURE_1D_ARRAY);
	}
}

/** Executes test iteration.
 *
 *  @return Returns STOP when test has finished executing, CONTINUE if more iterations are needed.
 */
tcu::TestNode::IterateResult TextureParameterQueriesTestCase::iterate()
{
	const Functions& gl = m_context.getRenderContext().getFunctions();

	bool result = true;

	GLuint texture;

	//Iterate through supported targets

	for (std::vector<glw::GLint>::const_iterator iter = mSupportedTargets.begin(); iter != mSupportedTargets.end();
		 ++iter)
	{
		const GLint& target = *iter;

		Texture::Generate(gl, texture);
		Texture::Bind(gl, texture, target);

		result = testTextureSparseARB(gl, target) && testVirtualPageSizeIndexARB(gl, target) &&
				 testNumSparseLevelsARB(gl, target);

		Texture::Delete(gl, texture);

		if (!result)
		{
			m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Fail on positive tests");
			return STOP;
		}
	}

	//Iterate through not supported targets
	for (std::vector<glw::GLint>::const_iterator iter = mNotSupportedTargets.begin();
		 iter != mNotSupportedTargets.end(); ++iter)
	{
		const GLint& target = *iter;

		Texture::Generate(gl, texture);
		Texture::Bind(gl, texture, target);

		result = testTextureSparseARB(gl, target, GL_INVALID_VALUE);

		Texture::Delete(gl, texture);

		if (!result)
		{
			m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Fail on negative tests");
			return STOP;
		}
	}

	m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
	return STOP;
}

/** Testing texParameter* functions for binded texture and GL_TEXTURE_SPARSE_ARB parameter name
 *
 * @param gl               GL API functions
 * @param target           Target for which texture is binded
 * @param expectedError    Expected error code (default value GL_NO_ERROR)
 *
 * @return Returns true if queried value is as expected, returns false otherwise
 */
bool TextureParameterQueriesTestCase::testTextureSparseARB(const Functions& gl, GLint target, GLint expectedError)
{
	const GLint pname = GL_TEXTURE_SPARSE_ARB;

	bool result = true;

	GLint   testValueInt;
	GLuint  testValueUInt;
	GLfloat testValueFloat;

	m_testCtx.getLog() << tcu::TestLog::Message << "Testing TEXTURE_SPARSE_ARB for target: " << target
					   << ", expected error: " << expectedError << tcu::TestLog::EndMessage;

	//Check getTexParameter* default value
	if (expectedError == GL_NO_ERROR)
		result = checkGetTexParameter(gl, target, pname, GL_FALSE);

	//Check getTexParameter* for manually set values
	if (result)
	{
		//Query to set parameter
		gl.texParameteri(target, pname, GL_TRUE);
		if (expectedError == GL_NO_ERROR)
		{
			GLU_EXPECT_NO_ERROR(gl.getError(), "glTexParameteri error occurred.");
			result = checkGetTexParameter(gl, target, pname, GL_TRUE);

			//If no error verification reset TEXTURE_SPARSE_ARB value
			gl.texParameteri(target, pname, GL_FALSE);
			GLU_EXPECT_NO_ERROR(gl.getError(), "glTexParameteri error occurred.");
		}
		else
			result = SparseTextureUtils::verifyQueryError(m_testCtx, "glTexParameteri", target, pname, gl.getError(),
														  expectedError);
	}

	if (result)
	{
		gl.texParameterf(target, pname, GL_TRUE);
		if (expectedError == GL_NO_ERROR)
		{
			GLU_EXPECT_NO_ERROR(gl.getError(), "glTexParameterf error occurred.");
			result = checkGetTexParameter(gl, target, pname, GL_TRUE);

			gl.texParameteri(target, pname, GL_FALSE);
			GLU_EXPECT_NO_ERROR(gl.getError(), "glTexParameteri error occurred.");
		}
		else
			result = SparseTextureUtils::verifyQueryError(m_testCtx, "glTexParameterf", target, pname, gl.getError(),
														  expectedError);
	}

	if (result)
	{
		testValueInt = GL_TRUE;
		gl.texParameteriv(target, pname, &testValueInt);
		if (expectedError == GL_NO_ERROR)
		{
			GLU_EXPECT_NO_ERROR(gl.getError(), "glTexParameteriv error occurred.");
			result = checkGetTexParameter(gl, target, pname, GL_TRUE);

			gl.texParameteri(target, pname, GL_FALSE);
			GLU_EXPECT_NO_ERROR(gl.getError(), "glTexParameteri error occurred.");
		}
		else
			result = SparseTextureUtils::verifyQueryError(m_testCtx, "glTexParameteriv", target, pname, gl.getError(),
														  expectedError);
	}

	if (result)
	{
		testValueFloat = (GLfloat)GL_TRUE;
		gl.texParameterfv(target, pname, &testValueFloat);
		if (expectedError == GL_NO_ERROR)
		{
			GLU_EXPECT_NO_ERROR(gl.getError(), "glTexParameterfv error occurred.");
			result = checkGetTexParameter(gl, target, pname, GL_TRUE);

			gl.texParameteri(target, pname, GL_FALSE);
			GLU_EXPECT_NO_ERROR(gl.getError(), "glTexParameteri error occurred.");
		}
		else
			result = SparseTextureUtils::verifyQueryError(m_testCtx, "glTexParameterfv", target, pname, gl.getError(),
														  expectedError);
	}

	if (result)
	{
		testValueInt = GL_TRUE;
		gl.texParameterIiv(target, pname, &testValueInt);
		if (expectedError == GL_NO_ERROR)
		{
			GLU_EXPECT_NO_ERROR(gl.getError(), "glTexParameterIiv error occurred.");
			result = checkGetTexParameter(gl, target, pname, GL_TRUE);

			gl.texParameteri(target, pname, GL_FALSE);
			GLU_EXPECT_NO_ERROR(gl.getError(), "glTexParameteri error occurred.");
		}
		else
			result = SparseTextureUtils::verifyQueryError(m_testCtx, "glTexParameterIiv", target, pname, gl.getError(),
														  expectedError);
	}

	if (result)
	{
		testValueUInt = GL_TRUE;
		gl.texParameterIuiv(target, pname, &testValueUInt);
		if (expectedError == GL_NO_ERROR)
		{
			GLU_EXPECT_NO_ERROR(gl.getError(), "glTexParameterIuiv error occurred.");
			result = checkGetTexParameter(gl, target, pname, GL_TRUE);

			gl.texParameteri(target, pname, GL_FALSE);
			GLU_EXPECT_NO_ERROR(gl.getError(), "glTexParameteri error occurred.");
		}
		else
			result = SparseTextureUtils::verifyQueryError(m_testCtx, "glTexParameterIuiv", target, pname, gl.getError(),
														  expectedError);
	}

	return result;
}

/** Testing texParameter* functions for binded texture and GL_VIRTUAL_PAGE_SIZE_INDEX_ARB parameter name
 *
 * @param gl               GL API functions
 * @param target           Target for which texture is binded
 * @param expectedError    Expected error code (default value GL_NO_ERROR)
 *
 * @return Returns true if queried value is as expected, returns false otherwise
 */
bool TextureParameterQueriesTestCase::testVirtualPageSizeIndexARB(const Functions& gl, GLint target,
																  GLint expectedError)
{
	const GLint pname = GL_VIRTUAL_PAGE_SIZE_INDEX_ARB;

	bool result = true;

	GLint   testValueInt;
	GLuint  testValueUInt;
	GLfloat testValueFloat;

	m_testCtx.getLog() << tcu::TestLog::Message << "Testing VIRTUAL_PAGE_SIZE_INDEX_ARB for target: " << target
					   << ", expected error: " << expectedError << tcu::TestLog::EndMessage;

	//Check getTexParameter* default value
	if (expectedError == GL_NO_ERROR)
		result = checkGetTexParameter(gl, target, pname, 0);

	//Check getTexParameter* for manually set values
	if (result)
	{
		gl.texParameteri(target, pname, 1);
		if (expectedError == GL_NO_ERROR)
		{
			GLU_EXPECT_NO_ERROR(gl.getError(), "glTexParameteri error occurred");
			result = checkGetTexParameter(gl, target, pname, 1);

			//If no error verification reset TEXTURE_SPARSE_ARB value
			gl.texParameteri(target, pname, 0);
			GLU_EXPECT_NO_ERROR(gl.getError(), "glTexParameteri error occurred");
		}
		else
			result = SparseTextureUtils::verifyQueryError(m_testCtx, "glTexParameteri", target, pname, gl.getError(),
														  expectedError);
	}

	if (result)
	{
		gl.texParameterf(target, pname, 2.0f);
		if (expectedError == GL_NO_ERROR)
		{
			GLU_EXPECT_NO_ERROR(gl.getError(), "glTexParameterf error occurred");
			result = checkGetTexParameter(gl, target, pname, 2);

			gl.texParameteri(target, pname, 0);
			GLU_EXPECT_NO_ERROR(gl.getError(), "glTexParameteri error occurred");
		}
		else
			result = SparseTextureUtils::verifyQueryError(m_testCtx, "glTexParameterf", target, pname, gl.getError(),
														  expectedError);
	}

	if (result)
	{
		testValueInt = 8;
		gl.texParameteriv(target, pname, &testValueInt);
		if (expectedError == GL_NO_ERROR)
		{
			GLU_EXPECT_NO_ERROR(gl.getError(), "glTexParameteriv error occurred");
			result = checkGetTexParameter(gl, target, pname, 8);

			gl.texParameteri(target, pname, 0);
			GLU_EXPECT_NO_ERROR(gl.getError(), "glTexParameteri error occurred");
		}
		else
			result = SparseTextureUtils::verifyQueryError(m_testCtx, "glTexParameteriv", target, pname, gl.getError(),
														  expectedError);
	}

	if (result)
	{
		testValueFloat = 10.0f;
		gl.texParameterfv(target, pname, &testValueFloat);
		if (expectedError == GL_NO_ERROR)
		{
			GLU_EXPECT_NO_ERROR(gl.getError(), "glTexParameterfv error occurred");
			result = checkGetTexParameter(gl, target, pname, 10);

			gl.texParameteri(target, pname, 0);
			GLU_EXPECT_NO_ERROR(gl.getError(), "glTexParameteri error occurred");
		}
		else
			result = SparseTextureUtils::verifyQueryError(m_testCtx, "glTexParameterfv", target, pname, gl.getError(),
														  expectedError);
	}

	if (result)
	{
		testValueInt = 6;
		gl.texParameterIiv(target, pname, &testValueInt);
		if (expectedError == GL_NO_ERROR)
		{
			GLU_EXPECT_NO_ERROR(gl.getError(), "glTexParameterIiv error occurred");
			result = checkGetTexParameter(gl, target, pname, 6);

			gl.texParameteri(target, pname, 0);
			GLU_EXPECT_NO_ERROR(gl.getError(), "glTexParameteri error occurred");
		}
		else
			result = SparseTextureUtils::verifyQueryError(m_testCtx, "glTexParameterIiv", target, pname, gl.getError(),
														  expectedError);
	}

	if (result)
	{
		testValueUInt = 16;
		gl.texParameterIuiv(target, pname, &testValueUInt);
		if (expectedError == GL_NO_ERROR)
		{
			GLU_EXPECT_NO_ERROR(gl.getError(), "glTexParameterIuiv error occurred");
			result = checkGetTexParameter(gl, target, pname, 16);

			gl.texParameteri(target, pname, 0);
			GLU_EXPECT_NO_ERROR(gl.getError(), "glTexParameteri error occurred");
		}
		else
			result = SparseTextureUtils::verifyQueryError(m_testCtx, "glTexParameterIuiv", target, pname, gl.getError(),
														  expectedError);
	}

	return result;
}

/** Testing getTexParameter* functions for binded texture and GL_NUM_SPARSE_LEVELS_ARB parameter name
 *
 * @param gl               GL API functions
 * @param target           Target for which texture is binded
 * @param expectedError    Expected error code (default value GL_NO_ERROR)
 *
 * @return Returns true if no error code was generated, throws exception otherwise
 */
bool TextureParameterQueriesTestCase::testNumSparseLevelsARB(const Functions& gl, GLint target)
{
	const GLint pname = GL_NUM_SPARSE_LEVELS_ARB;

	GLint   value_int;
	GLuint  value_uint;
	GLfloat value_float;

	m_testCtx.getLog() << tcu::TestLog::Message << "Testing NUM_SPARSE_LEVELS_ARB for target: " << target
					   << tcu::TestLog::EndMessage;

	gl.getTexParameteriv(target, pname, &value_int);
	GLU_EXPECT_NO_ERROR(gl.getError(), "glGetTexParameteriv error occurred");

	gl.getTexParameterfv(target, pname, &value_float);
	GLU_EXPECT_NO_ERROR(gl.getError(), "glGetTexParameterfv error occurred");

	gl.getTexParameterIiv(target, pname, &value_int);
	GLU_EXPECT_NO_ERROR(gl.getError(), "glGetGexParameterIiv error occurred");

	gl.getTexParameterIuiv(target, pname, &value_uint);
	GLU_EXPECT_NO_ERROR(gl.getError(), "glGetGexParameterIui error occurred");

	return true;
}

/** Checking if getTexParameter* for binded texture returns value as expected
 *
 * @param gl           GL API functions
 * @param target       Target for which texture is binded
 * @param pname        Parameter name
 * @param expected     Expected value (int because function is designed to query only int and boolean parameters)
 *
 * @return Returns true if queried value is as expected, returns false otherwise
 */
bool TextureParameterQueriesTestCase::checkGetTexParameter(const Functions& gl, GLint target, GLint pname,
														   GLint expected)
{
	bool result = true;

	GLint   value_int;
	GLuint  value_uint;
	GLfloat value_float;

	gl.getTexParameteriv(target, pname, &value_int);
	GLU_EXPECT_NO_ERROR(gl.getError(), "glGetTexParameteriv error occurred");
	if (value_int != expected)
	{
		m_testCtx.getLog() << tcu::TestLog::Message << "glGetTexParameteriv return wrong value"
						   << ", target: " << target << ", pname: " << pname << ", expected: " << expected
						   << ", returned: " << value_int << tcu::TestLog::EndMessage;

		result = false;
	}

	gl.getTexParameterfv(target, pname, &value_float);
	GLU_EXPECT_NO_ERROR(gl.getError(), "glGetTexParameterfv error occurred");
	if ((GLint)value_float != expected)
	{
		m_testCtx.getLog() << tcu::TestLog::Message << "glGetTexParameterfv return wrong value"
						   << ", target: " << target << ", pname: " << pname << ", expected: " << expected
						   << ", returned: " << (GLint)value_float << tcu::TestLog::EndMessage;

		result = false;
	}

	gl.getTexParameterIiv(target, pname, &value_int);
	GLU_EXPECT_NO_ERROR(gl.getError(), "glGetGexParameterIiv error occurred");
	if (value_int != expected)
	{
		m_testCtx.getLog() << tcu::TestLog::Message << "glGetGexParameterIiv return wrong value"
						   << ", target: " << target << ", pname: " << pname << ", expected: " << expected
						   << ", returned: " << value_int << tcu::TestLog::EndMessage;

		result = false;
	}

	gl.getTexParameterIuiv(target, pname, &value_uint);
	GLU_EXPECT_NO_ERROR(gl.getError(), "glGetGexParameterIui error occurred");
	if ((GLint)value_uint != expected)
	{
		m_testCtx.getLog() << tcu::TestLog::Message << "glGetGexParameterIui return wrong value"
						   << ", target: " << target << ", pname: " << pname << ", expected: " << expected
						   << ", returned: " << (GLint)value_uint << tcu::TestLog::EndMessage;

		result = false;
	}

	return result;
}

/** Constructor.
 *
 *  @param context     Rendering context
 */
InternalFormatQueriesTestCase::InternalFormatQueriesTestCase(deqp::Context& context)
	: TestCase(context, "InternalFormatQueries",
			   "Implements GetInternalformat query tests described in CTS_ARB_sparse_texture")
{
	/* Left blank intentionally */
}

/** Stub init method */
void InternalFormatQueriesTestCase::init()
{
	mSupportedTargets.push_back(GL_TEXTURE_1D);
	mSupportedTargets.push_back(GL_TEXTURE_1D_ARRAY);
	mSupportedTargets.push_back(GL_TEXTURE_2D);
	mSupportedTargets.push_back(GL_TEXTURE_2D_ARRAY);
	mSupportedTargets.push_back(GL_TEXTURE_3D);
	mSupportedTargets.push_back(GL_TEXTURE_CUBE_MAP);
	mSupportedTargets.push_back(GL_TEXTURE_CUBE_MAP_ARRAY);
	mSupportedTargets.push_back(GL_TEXTURE_RECTANGLE);
	mSupportedTargets.push_back(GL_TEXTURE_BUFFER);
	mSupportedTargets.push_back(GL_RENDERBUFFER);
	mSupportedTargets.push_back(GL_TEXTURE_2D_MULTISAMPLE);
	mSupportedTargets.push_back(GL_TEXTURE_2D_MULTISAMPLE_ARRAY);

	mSupportedInternalFormats.push_back(GL_R8);
	mSupportedInternalFormats.push_back(GL_R8_SNORM);
	mSupportedInternalFormats.push_back(GL_R16);
	mSupportedInternalFormats.push_back(GL_R16_SNORM);
	mSupportedInternalFormats.push_back(GL_RG8);
	mSupportedInternalFormats.push_back(GL_RG8_SNORM);
	mSupportedInternalFormats.push_back(GL_RG16);
	mSupportedInternalFormats.push_back(GL_RG16_SNORM);
	mSupportedInternalFormats.push_back(GL_RGB565);
	mSupportedInternalFormats.push_back(GL_RGBA8);
	mSupportedInternalFormats.push_back(GL_RGBA8_SNORM);
	mSupportedInternalFormats.push_back(GL_RGB10_A2);
	mSupportedInternalFormats.push_back(GL_RGB10_A2UI);
	mSupportedInternalFormats.push_back(GL_RGBA16);
	mSupportedInternalFormats.push_back(GL_RGBA16_SNORM);
	mSupportedInternalFormats.push_back(GL_R16F);
	mSupportedInternalFormats.push_back(GL_RG16F);
	mSupportedInternalFormats.push_back(GL_RGBA16F);
	mSupportedInternalFormats.push_back(GL_R32F);
	mSupportedInternalFormats.push_back(GL_RG32F);
	mSupportedInternalFormats.push_back(GL_RGBA32F);
	mSupportedInternalFormats.push_back(GL_R11F_G11F_B10F);
	mSupportedInternalFormats.push_back(GL_RGB9_E5);
	mSupportedInternalFormats.push_back(GL_R8I);
	mSupportedInternalFormats.push_back(GL_R8UI);
	mSupportedInternalFormats.push_back(GL_R16I);
	mSupportedInternalFormats.push_back(GL_R16UI);
	mSupportedInternalFormats.push_back(GL_R32I);
	mSupportedInternalFormats.push_back(GL_R32UI);
	mSupportedInternalFormats.push_back(GL_RG8I);
	mSupportedInternalFormats.push_back(GL_RG8UI);
	mSupportedInternalFormats.push_back(GL_RG16I);
	mSupportedInternalFormats.push_back(GL_RG16UI);
	mSupportedInternalFormats.push_back(GL_RG32I);
	mSupportedInternalFormats.push_back(GL_RG32UI);
	mSupportedInternalFormats.push_back(GL_RGBA8I);
	mSupportedInternalFormats.push_back(GL_RGBA8UI);
	mSupportedInternalFormats.push_back(GL_RGBA16I);
	mSupportedInternalFormats.push_back(GL_RGBA16UI);
	mSupportedInternalFormats.push_back(GL_RGBA32I);
}

/** Executes test iteration.
 *
 *  @return Returns STOP when test has finished executing, CONTINUE if more iterations are needed.
 */
tcu::TestNode::IterateResult InternalFormatQueriesTestCase::iterate()
{
	const Functions& gl = m_context.getRenderContext().getFunctions();

	bool result = true;

	m_testCtx.getLog() << tcu::TestLog::Message << "Testing getInternalformativ" << tcu::TestLog::EndMessage;

	for (std::vector<glw::GLint>::const_iterator iter = mSupportedTargets.begin(); iter != mSupportedTargets.end();
		 ++iter)
	{
		const GLint& target = *iter;

		for (std::vector<glw::GLint>::const_iterator formIter = mSupportedInternalFormats.begin();
			 formIter != mSupportedInternalFormats.end(); ++formIter)
		{
			const GLint& format = *formIter;
			GLint		 value;

			gl.getInternalformativ(target, format, GL_NUM_VIRTUAL_PAGE_SIZES_ARB, sizeof(value), &value);
			GLU_EXPECT_NO_ERROR(gl.getError(), "getInternalformativ error occurred for GL_NUM_VIRTUAL_PAGE_SIZES_ARB");
			if (value == 0)
			{
				m_testCtx.getLog() << tcu::TestLog::Message
								   << "getInternalformativ for GL_NUM_VIRTUAL_PAGE_SIZES_ARB, target: " << target
								   << ", format: " << format << " returns wrong value: " << value
								   << tcu::TestLog::EndMessage;

				result = false;
			}

			if (result)
			{
				GLint pageSizeX;
				GLint pageSizeY;
				GLint pageSizeZ;
				SparseTextureUtils::getTexturePageSizes(gl, target, format, pageSizeX, pageSizeY, pageSizeZ);
			}
			else
			{
				m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Fail");
				return STOP;
			}
		}
	}

	m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");

	return STOP;
}

/** Constructor.
 *
 *  @param context     Rendering context
 */
SimpleQueriesTestCase::SimpleQueriesTestCase(deqp::Context& context)
	: TestCase(context, "SimpleQueries", "Implements Get* queries tests described in CTS_ARB_sparse_texture")
{
	/* Left blank intentionally */
}

/** Executes test iteration.
 *
 *  @return Returns STOP when test has finished executing, CONTINUE if more iterations are needed.
 */
tcu::TestNode::IterateResult SimpleQueriesTestCase::iterate()
{
	const Functions& gl = m_context.getRenderContext().getFunctions();

	testSipmleQueries(gl, GL_MAX_SPARSE_TEXTURE_SIZE_ARB);
	testSipmleQueries(gl, GL_MAX_SPARSE_3D_TEXTURE_SIZE_ARB);
	testSipmleQueries(gl, GL_MAX_SPARSE_ARRAY_TEXTURE_LAYERS_ARB);
	testSipmleQueries(gl, GL_SPARSE_TEXTURE_FULL_ARRAY_CUBE_MIPMAPS_ARB);

	m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
	return STOP;
}

void SimpleQueriesTestCase::testSipmleQueries(const Functions& gl, GLint pname)
{
	m_testCtx.getLog() << tcu::TestLog::Message << "Testing simple queries for pname: " << pname
					   << tcu::TestLog::EndMessage;

	GLint	 value_int;
	GLint64   value_int64;
	GLfloat   value_float;
	GLdouble  value_double;
	GLboolean value_bool;

	gl.getIntegerv(pname, &value_int);
	GLU_EXPECT_NO_ERROR(gl.getError(), "getIntegerv error occurred");

	gl.getInteger64v(pname, &value_int64);
	GLU_EXPECT_NO_ERROR(gl.getError(), "getInteger64v error occurred");

	gl.getFloatv(pname, &value_float);
	GLU_EXPECT_NO_ERROR(gl.getError(), "getFloatv error occurred");

	gl.getDoublev(pname, &value_double);
	GLU_EXPECT_NO_ERROR(gl.getError(), "getDoublev error occurred");

	gl.getBooleanv(pname, &value_bool);
	GLU_EXPECT_NO_ERROR(gl.getError(), "getBooleanv error occurred");
}

/** Constructor.
 *
 *  @param context     Rendering context
 */
SparseTextureAllocationTestCase::SparseTextureAllocationTestCase(deqp::Context& context)
	: TestCase(context, "SparseTextureAllocation", "Verifies TexStorage* functionality added in CTS_ARB_sparse_texture")
{
	/* Left blank intentionally */
}

/** Constructor.
 *
 *  @param context     Rendering context
 */
SparseTextureAllocationTestCase::SparseTextureAllocationTestCase(deqp::Context& context, const char* name,
																 const char* description)
	: TestCase(context, name, description)
{
	/* Left blank intentionally */
}

/** Initializes the test group contents. */
void SparseTextureAllocationTestCase::init()
{
	mSupportedTargets.push_back(GL_TEXTURE_2D);
	mSupportedTargets.push_back(GL_TEXTURE_2D_ARRAY);
	mSupportedTargets.push_back(GL_TEXTURE_CUBE_MAP);
	mSupportedTargets.push_back(GL_TEXTURE_CUBE_MAP_ARRAY);
	mSupportedTargets.push_back(GL_TEXTURE_3D);
	mSupportedTargets.push_back(GL_TEXTURE_RECTANGLE);

	mFullArrayTargets.push_back(GL_TEXTURE_1D_ARRAY);
	mFullArrayTargets.push_back(GL_TEXTURE_2D_ARRAY);
	mFullArrayTargets.push_back(GL_TEXTURE_CUBE_MAP);
	mFullArrayTargets.push_back(GL_TEXTURE_CUBE_MAP_ARRAY);

	mSupportedInternalFormats.push_back(GL_R8);
	mSupportedInternalFormats.push_back(GL_R8_SNORM);
	mSupportedInternalFormats.push_back(GL_R16);
	mSupportedInternalFormats.push_back(GL_R16_SNORM);
	mSupportedInternalFormats.push_back(GL_RG8);
	mSupportedInternalFormats.push_back(GL_RG8_SNORM);
	mSupportedInternalFormats.push_back(GL_RG16);
	mSupportedInternalFormats.push_back(GL_RG16_SNORM);
	mSupportedInternalFormats.push_back(GL_RGB565);
	mSupportedInternalFormats.push_back(GL_RGBA8);
	mSupportedInternalFormats.push_back(GL_RGBA8_SNORM);
	mSupportedInternalFormats.push_back(GL_RGB10_A2);
	mSupportedInternalFormats.push_back(GL_RGB10_A2UI);
	mSupportedInternalFormats.push_back(GL_RGBA16);
	mSupportedInternalFormats.push_back(GL_RGBA16_SNORM);
	mSupportedInternalFormats.push_back(GL_R16F);
	mSupportedInternalFormats.push_back(GL_RG16F);
	mSupportedInternalFormats.push_back(GL_RGBA16F);
	mSupportedInternalFormats.push_back(GL_R32F);
	mSupportedInternalFormats.push_back(GL_RG32F);
	mSupportedInternalFormats.push_back(GL_RGBA32F);
	mSupportedInternalFormats.push_back(GL_R11F_G11F_B10F);
	mSupportedInternalFormats.push_back(GL_RGB9_E5);
	mSupportedInternalFormats.push_back(GL_R8I);
	mSupportedInternalFormats.push_back(GL_R8UI);
	mSupportedInternalFormats.push_back(GL_R16I);
	mSupportedInternalFormats.push_back(GL_R16UI);
	mSupportedInternalFormats.push_back(GL_R32I);
	mSupportedInternalFormats.push_back(GL_R32UI);
	mSupportedInternalFormats.push_back(GL_RG8I);
	mSupportedInternalFormats.push_back(GL_RG8UI);
	mSupportedInternalFormats.push_back(GL_RG16I);
	mSupportedInternalFormats.push_back(GL_RG16UI);
	mSupportedInternalFormats.push_back(GL_RG32I);
	mSupportedInternalFormats.push_back(GL_RG32UI);
	mSupportedInternalFormats.push_back(GL_RGBA8I);
	mSupportedInternalFormats.push_back(GL_RGBA8UI);
	mSupportedInternalFormats.push_back(GL_RGBA16I);
	mSupportedInternalFormats.push_back(GL_RGBA16UI);
	mSupportedInternalFormats.push_back(GL_RGBA32I);
}

/** Executes test iteration.
 *
 *  @return Returns STOP when test has finished executing, CONTINUE if more iterations are needed.
 */
tcu::TestNode::IterateResult SparseTextureAllocationTestCase::iterate()
{
	const Functions& gl = m_context.getRenderContext().getFunctions();

	bool result = true;

	for (std::vector<glw::GLint>::const_iterator iter = mSupportedTargets.begin(); iter != mSupportedTargets.end();
		 ++iter)
	{
		const GLint& target = *iter;

		for (std::vector<glw::GLint>::const_iterator formIter = mSupportedInternalFormats.begin();
			 formIter != mSupportedInternalFormats.end(); ++formIter)
		{
			const GLint& format = *formIter;

			m_testCtx.getLog() << tcu::TestLog::Message << "Testing sparse texture allocation for target: " << target
							   << ", format: " << format << tcu::TestLog::EndMessage;

			positiveTesting(gl, target, format);

			result = verifyTexParameterErrors(gl, target, format) &&
					 verifyTexStorageVirtualPageSizeIndexError(gl, target, format) &&
					 verifyTexStorageFullArrayCubeMipmapsError(gl, target, format) &&
					 verifyTexStorageInvalidValueErrors(gl, target, format);

			if (!result)
			{
				m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Fail");
				return STOP;
			}
		}
	}

	for (std::vector<glw::GLint>::const_iterator iter = mFullArrayTargets.begin(); iter != mFullArrayTargets.end();
		 ++iter)
	{
		const GLint& target = *iter;

		for (std::vector<glw::GLint>::const_iterator formIter = mSupportedInternalFormats.begin();
			 formIter != mSupportedInternalFormats.end(); ++formIter)
		{
			const GLint& format = *formIter;

			m_testCtx.getLog() << tcu::TestLog::Message << "Testing sparse texture allocation for target: " << target
							   << ", format: " << format << tcu::TestLog::EndMessage;

			result = verifyTexStorageFullArrayCubeMipmapsError(gl, target, format) &&
					 verifyTexStorageInvalidValueErrors(gl, target, format);

			if (!result)
			{
				m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Fail");
				return STOP;
			}
		}
	}

	m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
	return STOP;
}

/** Testing if texStorage* functionality added in ARB_sparse_texture extension works properly for given target and internal format
 *
 * @param gl           GL API functions
 * @param target       Target for which texture is binded
 * @param format       Texture internal format
 **/
void SparseTextureAllocationTestCase::positiveTesting(const Functions& gl, GLint target, GLint format)
{
	GLuint texture;

	Texture::Generate(gl, texture);
	Texture::Bind(gl, texture, target);

	GLint pageSizeX;
	GLint pageSizeY;
	GLint pageSizeZ;
	GLint depth = SparseTextureUtils::getTargetDepth(target);
	SparseTextureUtils::getTexturePageSizes(gl, target, format, pageSizeX, pageSizeY, pageSizeZ);

	gl.texParameteri(target, GL_TEXTURE_SPARSE_ARB, GL_TRUE);
	GLU_EXPECT_NO_ERROR(gl.getError(), "texParameteri error occurred for GL_TEXTURE_SPARSE_ARB");

	//The <width> and <height> has to be equal for cube map textures
	if (target == GL_TEXTURE_CUBE_MAP || target == GL_TEXTURE_CUBE_MAP_ARRAY)
	{
		if (pageSizeX > pageSizeY)
			pageSizeY = pageSizeX;
		else if (pageSizeX < pageSizeY)
			pageSizeX = pageSizeY;
	}

	Texture::Storage(gl, target, 1, format, pageSizeX, pageSizeY, depth * pageSizeZ);
	GLU_EXPECT_NO_ERROR(gl.getError(), "TexStorage");

	Texture::Delete(gl, texture);
}

/** Verifies if texParameter* generate proper errors for given target and internal format.
 *
 * @param gl           GL API functions
 * @param target       Target for which texture is binded
 * @param format       Texture internal format
 *
 * @return Returns true if errors are as expected, false otherwise.
 */
bool SparseTextureAllocationTestCase::verifyTexParameterErrors(const Functions& gl, GLint target, GLint format)
{
	bool result = true;

	GLuint texture;
	GLint  depth;

	Texture::Generate(gl, texture);
	Texture::Bind(gl, texture, target);

	depth = SparseTextureUtils::getTargetDepth(target);

	Texture::Storage(gl, target, 1, format, 8, 8, depth);
	GLU_EXPECT_NO_ERROR(gl.getError(), "TexStorage");

	GLint immutableFormat;

	gl.getTexParameteriv(target, GL_TEXTURE_IMMUTABLE_FORMAT, &immutableFormat);
	GLU_EXPECT_NO_ERROR(gl.getError(), "getTexParameteriv error occurred for GL_TEXTURE_IMMUTABLE_FORMAT");

	// Test error only if texture is immutable format, otherwise skip
	if (immutableFormat == GL_TRUE)
	{
		std::vector<IntPair> params;
		params.push_back(IntPair(GL_TEXTURE_SPARSE_ARB, GL_TRUE));
		params.push_back(IntPair(GL_VIRTUAL_PAGE_SIZE_INDEX_ARB, 1));

		for (std::vector<IntPair>::const_iterator iter = params.begin(); iter != params.end(); ++iter)
		{
			const IntPair& param = *iter;

			if (result)
			{
				gl.texParameteri(target, param.first, param.second);
				result = SparseTextureUtils::verifyQueryError(m_testCtx, "glTexParameteri", target, param.first,
															  gl.getError(), GL_INVALID_OPERATION);
			}

			if (result)
			{
				gl.texParameterf(target, param.first, (GLfloat)param.second);
				result = SparseTextureUtils::verifyQueryError(m_testCtx, "glTexParameterf", target, param.first,
															  gl.getError(), GL_INVALID_OPERATION);
			}

			if (result)
			{
				GLint value = param.second;
				gl.texParameteriv(target, param.first, &value);
				result = SparseTextureUtils::verifyQueryError(m_testCtx, "glTexParameteriv", target, param.first,
															  gl.getError(), GL_INVALID_OPERATION);
			}

			if (result)
			{
				GLfloat value = (GLfloat)param.second;
				gl.texParameterfv(target, param.first, &value);
				result = SparseTextureUtils::verifyQueryError(m_testCtx, "glTexParameterfv", target, param.first,
															  gl.getError(), GL_INVALID_OPERATION);
			}

			if (result)
			{
				GLint value = param.second;
				gl.texParameterIiv(target, param.first, &value);
				result = SparseTextureUtils::verifyQueryError(m_testCtx, "glTexParameterIiv", target, param.first,
															  gl.getError(), GL_INVALID_OPERATION);
			}

			if (result)
			{
				GLuint value = param.second;
				gl.texParameterIuiv(target, param.first, &value);
				result = SparseTextureUtils::verifyQueryError(m_testCtx, "glTexParameterIuiv", target, param.first,
															  gl.getError(), GL_INVALID_OPERATION);
			}
		}
	}

	Texture::Delete(gl, texture);

	return result;
}

/** Verifies if texStorage* generate proper error for given target and internal format when
 *  VIRTUAL_PAGE_SIZE_INDEX_ARB value is greater than NUM_VIRTUAL_PAGE_SIZES_ARB.
 *
 * @param gl           GL API functions
 * @param target       Target for which texture is binded
 * @param format       Texture internal format
 *
 * @return Returns true if errors are as expected, false otherwise.
 */
bool SparseTextureAllocationTestCase::verifyTexStorageVirtualPageSizeIndexError(const Functions& gl, GLint target,
																				GLint format)
{
	bool result = true;

	GLuint texture;
	GLint  depth;
	GLint  numPageSizes;

	Texture::Generate(gl, texture);
	Texture::Bind(gl, texture, target);

	gl.texParameteri(target, GL_TEXTURE_SPARSE_ARB, GL_TRUE);
	GLU_EXPECT_NO_ERROR(gl.getError(), "texParameteri error occurred for GL_TEXTURE_SPARSE_ARB");

	gl.getInternalformativ(target, format, GL_NUM_VIRTUAL_PAGE_SIZES_ARB, sizeof(numPageSizes), &numPageSizes);
	GLU_EXPECT_NO_ERROR(gl.getError(), "getTexParameteriv error occurred for GL_NUM_VIRTUAL_PAGE_SIZES_ARB");

	numPageSizes += 1;
	gl.texParameteri(target, GL_VIRTUAL_PAGE_SIZE_INDEX_ARB, numPageSizes);
	GLU_EXPECT_NO_ERROR(gl.getError(), "texParameteri error occurred for GL_VIRTUAL_PAGE_SIZE_INDEX_ARB");

	depth = SparseTextureUtils::getTargetDepth(target);

	Texture::Storage(gl, target, 1, format, 8, 8, depth);
	result = SparseTextureUtils::verifyError(m_testCtx, "TexStorage", gl.getError(), GL_INVALID_OPERATION);

	Texture::Delete(gl, texture);

	return result;
}

/** Verifies if texStorage* generate proper errors for given target and internal format and
 *  SPARSE_TEXTURE_FULL_ARRAY_CUBE_MIPMAPS_ARB value set to FALSE.
 *
 * @param gl           GL API functions
 * @param target       Target for which texture is binded
 * @param format       Texture internal format
 *
 * @return Returns true if errors are as expected, false otherwise.
 */
bool SparseTextureAllocationTestCase::verifyTexStorageFullArrayCubeMipmapsError(const Functions& gl, GLint target,
																				GLint format)
{
	bool result = true;

	GLuint texture;
	GLint  depth;

	depth = SparseTextureUtils::getTargetDepth(target);

	GLboolean fullArrayCubeMipmaps;

	gl.getBooleanv(GL_SPARSE_TEXTURE_FULL_ARRAY_CUBE_MIPMAPS_ARB, &fullArrayCubeMipmaps);
	GLU_EXPECT_NO_ERROR(gl.getError(), "getBooleanv error occurred for GL_SPARSE_TEXTURE_FULL_ARRAY_CUBE_MIPMAPS_ARB");

	if (fullArrayCubeMipmaps == GL_FALSE)
	{
		if (target != GL_TEXTURE_1D_ARRAY && target != GL_TEXTURE_2D_ARRAY && target != GL_TEXTURE_CUBE_MAP &&
			target != GL_TEXTURE_CUBE_MAP_ARRAY)
		{
			// Case 1: test GL_TEXTURE_SPARSE_ARB
			Texture::Generate(gl, texture);
			Texture::Bind(gl, texture, target);

			gl.texParameteri(target, GL_TEXTURE_SPARSE_ARB, GL_TRUE);
			GLU_EXPECT_NO_ERROR(gl.getError(), "texParameteri error occurred for GL_TEXTURE_SPARSE_ARB");

			Texture::Storage(gl, target, 1, format, 8, 8, depth);
			result =
				SparseTextureUtils::verifyError(m_testCtx, "TexStorage [case 1]", gl.getError(), GL_INVALID_OPERATION);

			Texture::Delete(gl, texture);

			// Case 2: test wrong texture size
			if (result)
			{
				Texture::Generate(gl, texture);
				Texture::Bind(gl, texture, target);

				GLint pageSizeX;
				GLint pageSizeY;
				GLint pageSizeZ;
				SparseTextureUtils::getTexturePageSizes(gl, target, format, pageSizeX, pageSizeY, pageSizeZ);

				GLint levels = 4;
				GLint width  = pageSizeX * (int)pow(2, levels - 1);
				GLint height = pageSizeY * (int)pow(2, levels - 1);

				// Check 2 different cases:
				// 1) wrong width
				// 2) wrong height
				Texture::Storage(gl, target, levels, format, width + pageSizeX, height, depth);
				result = SparseTextureUtils::verifyError(m_testCtx, "TexStorage [case 2 wrong width]", gl.getError(),
														 GL_INVALID_OPERATION);

				if (result)
				{
					Texture::Storage(gl, target, levels, format, width, height + pageSizeY, depth);
					result = SparseTextureUtils::verifyError(m_testCtx, "TexStorage [case 2 wrong height]",
															 gl.getError(), GL_INVALID_OPERATION);
				}

				Texture::Delete(gl, texture);
			}
		}
		else
		{
			// Case 3: test full array mipmaps targets
			Texture::Generate(gl, texture);
			Texture::Bind(gl, texture, target);

			if (target == GL_TEXTURE_1D_ARRAY)
				Texture::Storage(gl, target, 1, format, 8, depth, 0);
			else
				Texture::Storage(gl, target, 1, format, 8, 8, depth);

			result =
				SparseTextureUtils::verifyError(m_testCtx, "TexStorage [case 3]", gl.getError(), GL_INVALID_OPERATION);

			Texture::Delete(gl, texture);
		}
	}

	return result;
}

/** Verifies if texStorage* generate proper errors for given target and internal format when
 *  texture size are set greater than allowed.
 *
 * @param gl           GL API functions
 * @param target       Target for which texture is binded
 * @param format       Texture internal format
 *
 * @return Returns true if errors are as expected, false otherwise.
 */
bool SparseTextureAllocationTestCase::verifyTexStorageInvalidValueErrors(const Functions& gl, GLint target,
																		 GLint format)
{
	GLuint texture;
	GLint  pageSizeX;
	GLint  pageSizeY;
	GLint  pageSizeZ;
	SparseTextureUtils::getTexturePageSizes(gl, target, format, pageSizeX, pageSizeY, pageSizeZ);

	Texture::Generate(gl, texture);
	Texture::Bind(gl, texture, target);

	GLint width  = pageSizeX;
	GLint height = pageSizeY;
	GLint depth  = SparseTextureUtils::getTargetDepth(target) * pageSizeZ;

	if (target == GL_TEXTURE_3D)
	{
		GLint max3DTextureSize;

		gl.getIntegerv(GL_MAX_SPARSE_3D_TEXTURE_SIZE_ARB, &max3DTextureSize);
		GLU_EXPECT_NO_ERROR(gl.getError(), "getIntegeriv error occurred for GL_MAX_SPARSE_3D_TEXTURE_SIZE_ARB");

		// Check 3 different cases:
		// 1) wrong width
		// 2) wrong height
		// 3) wrong depth
		Texture::Storage(gl, target, 1, format, width + max3DTextureSize, height, depth);
		if (!SparseTextureUtils::verifyError(m_testCtx, "TexStorage [GL_TEXTURE_3D wrong width]", gl.getError(),
											 GL_INVALID_VALUE))
		{
			Texture::Delete(gl, texture);
			return false;
		}

		Texture::Storage(gl, target, 1, format, width, height + max3DTextureSize, depth);
		if (!SparseTextureUtils::verifyError(m_testCtx, "TexStorage [GL_TEXTURE_3D wrong height]", gl.getError(),
											 GL_INVALID_VALUE))
		{
			Texture::Delete(gl, texture);
			return false;
		}

		Texture::Storage(gl, target, 1, format, width, height, depth + max3DTextureSize);
		if (!SparseTextureUtils::verifyError(m_testCtx, "TexStorage [GL_TEXTURE_3D wrong depth]", gl.getError(),
											 GL_INVALID_VALUE))
		{
			Texture::Delete(gl, texture);
			return false;
		}
	}
	else
	{
		GLint maxTextureSize;

		gl.getIntegerv(GL_MAX_SPARSE_TEXTURE_SIZE_ARB, &maxTextureSize);
		GLU_EXPECT_NO_ERROR(gl.getError(), "getIntegeriv error occurred for GL_MAX_SPARSE_TEXTURE_SIZE_ARB");

		// Check 3 different cases:
		// 1) wrong width
		// 2) wrong height
		Texture::Storage(gl, target, 1, format, width + maxTextureSize, height, depth);
		if (!SparseTextureUtils::verifyError(m_testCtx, "TexStorage [!GL_TEXTURE_3D wrong width]", gl.getError(),
											 GL_INVALID_VALUE))
		{
			Texture::Delete(gl, texture);
			return false;
		}

		Texture::Storage(gl, target, 1, format, width, height + maxTextureSize, depth);
		if (!SparseTextureUtils::verifyError(m_testCtx, "TexStorage [!GL_TEXTURE_3D wrong height]", gl.getError(),
											 GL_INVALID_VALUE))
		{
			Texture::Delete(gl, texture);
			return false;
		}

		GLint maxArrayTextureLayers;

		gl.getIntegerv(GL_MAX_SPARSE_ARRAY_TEXTURE_LAYERS_ARB, &maxArrayTextureLayers);
		GLU_EXPECT_NO_ERROR(gl.getError(), "getIntegeriv error occurred for GL_MAX_SPARSE_ARRAY_TEXTURE_LAYERS_ARB");

		if (target == GL_TEXTURE_1D_ARRAY)
		{
			Texture::Storage(gl, target, 1, format, width, height + maxArrayTextureLayers, 0);
			if (!SparseTextureUtils::verifyError(m_testCtx, "TexStorage [ARRAY wrong height]", gl.getError(),
												 GL_INVALID_VALUE))
			{
				Texture::Delete(gl, texture);
				return false;
			}
		}
		else if ((target == GL_TEXTURE_2D_ARRAY || target == GL_TEXTURE_CUBE_MAP_ARRAY))
		{
			Texture::Storage(gl, target, 1, format, width, height, depth + maxArrayTextureLayers);
			if (!SparseTextureUtils::verifyError(m_testCtx, "TexStorage [ARRAY wrong depth]", gl.getError(),
												 GL_INVALID_VALUE))
			{
				Texture::Delete(gl, texture);
				return false;
			}
		}
	}

	if (!m_context.getContextInfo().isExtensionSupported("GL_ARB_sparse_texture2"))
	{
		if (pageSizeX > 1)
		{
			Texture::Storage(gl, target, 1, format, 1, height, depth);
			if (!SparseTextureUtils::verifyError(m_testCtx, "TexStorage [wrong width]", gl.getError(),
												 GL_INVALID_VALUE))
			{
				Texture::Delete(gl, texture);
				return false;
			}
		}

		if (pageSizeY > 1)
		{
			Texture::Storage(gl, target, 1, format, width, 1, depth);
			if (!SparseTextureUtils::verifyError(m_testCtx, "TexStorage [wrong height]", gl.getError(),
												 GL_INVALID_VALUE))
			{
				Texture::Delete(gl, texture);
				return false;
			}
		}

		if (pageSizeZ > 1)
		{
			Texture::Storage(gl, target, 1, format, width, height, SparseTextureUtils::getTargetDepth(target));
			if (!SparseTextureUtils::verifyError(m_testCtx, "TexStorage [wrong depth]", gl.getError(),
												 GL_INVALID_VALUE))
			{
				Texture::Delete(gl, texture);
				return false;
			}
		}
	}

	Texture::Delete(gl, texture);
	return true;
}

/** Constructor.
 *
 *  @param context     Rendering context
 */
SparseTextureCommitmentTestCase::SparseTextureCommitmentTestCase(deqp::Context& context)
	: TestCase(context, "SparseTextureCommitment",
			   "Verifies TexPageCommitmentARB functionality added in CTS_ARB_sparse_texture")
	, mWidthStored(0)
	, mHeightStored(0)
	, mDepthStored(0)
{
	/* Left blank intentionally */
}

/** Constructor.
 *
 *  @param context     Rendering context
 *  @param name        Test name
 *  @param description Test description
 */
SparseTextureCommitmentTestCase::SparseTextureCommitmentTestCase(deqp::Context& context, const char* name,
																 const char* description)
	: TestCase(context, name, description), mWidthStored(0), mHeightStored(0), mDepthStored(0)
{
	/* Left blank intentionally */
}

/** Initializes the test group contents. */
void SparseTextureCommitmentTestCase::init()
{
	mSupportedTargets.push_back(GL_TEXTURE_2D);
	mSupportedTargets.push_back(GL_TEXTURE_2D_ARRAY);
	mSupportedTargets.push_back(GL_TEXTURE_CUBE_MAP);
	mSupportedTargets.push_back(GL_TEXTURE_CUBE_MAP_ARRAY);
	mSupportedTargets.push_back(GL_TEXTURE_3D);
	mSupportedTargets.push_back(GL_TEXTURE_RECTANGLE);

	mSupportedInternalFormats.push_back(GL_R8);
	mSupportedInternalFormats.push_back(GL_R8_SNORM);
	mSupportedInternalFormats.push_back(GL_R16);
	mSupportedInternalFormats.push_back(GL_R16_SNORM);
	mSupportedInternalFormats.push_back(GL_RG8);
	mSupportedInternalFormats.push_back(GL_RG8_SNORM);
	mSupportedInternalFormats.push_back(GL_RG16);
	mSupportedInternalFormats.push_back(GL_RG16_SNORM);
	mSupportedInternalFormats.push_back(GL_RGB565);
	mSupportedInternalFormats.push_back(GL_RGBA8);
	mSupportedInternalFormats.push_back(GL_RGBA8_SNORM);
	mSupportedInternalFormats.push_back(GL_RGB10_A2);
	mSupportedInternalFormats.push_back(GL_RGB10_A2UI);
	mSupportedInternalFormats.push_back(GL_RGBA16);
	mSupportedInternalFormats.push_back(GL_RGBA16_SNORM);
	mSupportedInternalFormats.push_back(GL_R16F);
	mSupportedInternalFormats.push_back(GL_RG16F);
	mSupportedInternalFormats.push_back(GL_RGBA16F);
	mSupportedInternalFormats.push_back(GL_R32F);
	mSupportedInternalFormats.push_back(GL_RG32F);
	mSupportedInternalFormats.push_back(GL_RGBA32F);
	mSupportedInternalFormats.push_back(GL_R11F_G11F_B10F);
	mSupportedInternalFormats.push_back(GL_RGB9_E5);
	mSupportedInternalFormats.push_back(GL_R8I);
	mSupportedInternalFormats.push_back(GL_R8UI);
	mSupportedInternalFormats.push_back(GL_R16I);
	mSupportedInternalFormats.push_back(GL_R16UI);
	mSupportedInternalFormats.push_back(GL_R32I);
	mSupportedInternalFormats.push_back(GL_R32UI);
	mSupportedInternalFormats.push_back(GL_RG8I);
	mSupportedInternalFormats.push_back(GL_RG8UI);
	mSupportedInternalFormats.push_back(GL_RG16I);
	mSupportedInternalFormats.push_back(GL_RG16UI);
	mSupportedInternalFormats.push_back(GL_RG32I);
	mSupportedInternalFormats.push_back(GL_RG32UI);
	mSupportedInternalFormats.push_back(GL_RGBA8I);
	mSupportedInternalFormats.push_back(GL_RGBA8UI);
	mSupportedInternalFormats.push_back(GL_RGBA16I);
	mSupportedInternalFormats.push_back(GL_RGBA16UI);
	mSupportedInternalFormats.push_back(GL_RGBA32I);
}

/** Executes test iteration.
 *
 *  @return Returns STOP when test has finished executing, CONTINUE if more iterations are needed.
 */
tcu::TestNode::IterateResult SparseTextureCommitmentTestCase::iterate()
{
	const Functions& gl = m_context.getRenderContext().getFunctions();

	bool result = true;

	GLuint texture;

	for (std::vector<glw::GLint>::const_iterator iter = mSupportedTargets.begin(); iter != mSupportedTargets.end();
		 ++iter)
	{
		const GLint& target = *iter;

		for (std::vector<glw::GLint>::const_iterator formIter = mSupportedInternalFormats.begin();
			 formIter != mSupportedInternalFormats.end(); ++formIter)
		{
			const GLint& format = *formIter;

			m_testCtx.getLog() << tcu::TestLog::Message << "Testing sparse texture commitment for target: " << target
							   << ", format: " << format << tcu::TestLog::EndMessage;

			//Checking if written data into not committed region generates no error
			sparseAllocateTexture(gl, target, format, texture);
			writeDataToTexture(gl, target, format, texture);

			//Checking if written data into committed region is as expected
			commitTexturePage(gl, target, format, texture);
			writeDataToTexture(gl, target, format, texture);
			result = verifyTextureData(gl, target, format, texture);

			Texture::Delete(gl, texture);

			//verify errors
			result = verifyInvalidOperationErrors(gl, target, format, texture) &&
					 verifyInvalidValueErrors(gl, target, format, texture);

			if (!result)
			{
				m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Fail");
				return STOP;
			}
		}
	}

	m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
	return STOP;
}

/** Bind texPageCommitmentARB function
 *
 * @param gl           GL API functions
 * @param target       Target for which texture is binded
 * @param format       Texture internal format
 * @param texture      Texture object
 * @param xOffset      Texture commitment x offset
 * @param yOffset      Texture commitment y offset
 * @param zOffset      Texture commitment z offset
 * @param width        Texture commitment width
 * @param height       Texture commitment height
 * @param depth        Texture commitment depth
 * @param commit       Commit or de-commit indicator
 **/
void SparseTextureCommitmentTestCase::texPageCommitment(const glw::Functions& gl, glw::GLint target, glw::GLint format,
														glw::GLuint& texture, GLint level, GLint xOffset, GLint yOffset,
														GLint zOffset, GLint width, GLint height, GLint depth,
														GLboolean commit)
{
	DE_UNREF(format);
	Texture::Bind(gl, texture, target);

	gl.texPageCommitmentARB(target, level, xOffset, yOffset, zOffset, width, height, depth, commit);
}

/** Preparing texture
 *
 * @param gl           GL API functions
 * @param target       Target for which texture is binded
 * @param format       Texture internal format
 * @param texture      Texture object
 *
 * @return Returns true if no error occurred, otherwise throws an exception.
 */
bool SparseTextureCommitmentTestCase::prepareTexture(const Functions& gl, GLint target, GLint format, GLuint& texture)
{
	Texture::Generate(gl, texture);
	Texture::Bind(gl, texture, target);

	GLint pageSizeX;
	GLint pageSizeY;
	GLint pageSizeZ;
	GLint minDepth = SparseTextureUtils::getTargetDepth(target);
	SparseTextureUtils::getTexturePageSizes(gl, target, format, pageSizeX, pageSizeY, pageSizeZ);

	//The <width> and <height> has to be equal for cube map textures
	if (target == GL_TEXTURE_CUBE_MAP || target == GL_TEXTURE_CUBE_MAP_ARRAY)
	{
		if (pageSizeX > pageSizeY)
			pageSizeY = pageSizeX;
		else if (pageSizeX < pageSizeY)
			pageSizeX = pageSizeY;
	}

	mWidthStored  = 2 * pageSizeX;
	mHeightStored = 2 * pageSizeY;
	mDepthStored  = minDepth * pageSizeZ;

	mTextureFormatStored = glu::mapGLInternalFormat(format);

	return true;
}

/** Allocating sparse texture memory using texStorage* function
 *
 * @param gl           GL API functions
 * @param target       Target for which texture is binded
 * @param format       Texture internal format
 * @param texture      Texture object
 *
 * @return Returns true if no error occurred, otherwise throws an exception.
 */
bool SparseTextureCommitmentTestCase::sparseAllocateTexture(const Functions& gl, GLint target, GLint format,
															GLuint& texture)
{
	prepareTexture(gl, target, format, texture);

	gl.texParameteri(target, GL_TEXTURE_SPARSE_ARB, GL_TRUE);
	GLU_EXPECT_NO_ERROR(gl.getError(), "texParameteri error occurred for GL_TEXTURE_SPARSE_ARB");

	Texture::Storage(gl, target, 1, format, mWidthStored, mHeightStored, mDepthStored);
	GLU_EXPECT_NO_ERROR(gl.getError(), "TexStorage");

	return true;
}

/** Allocating texture memory using texStorage* function
 *
 * @param gl           GL API functions
 * @param target       Target for which texture is binded
 * @param format       Texture internal format
 * @param texture      Texture object
 *
 * @return Returns true if no error occurred, otherwise throws an exception.
 */
bool SparseTextureCommitmentTestCase::allocateTexture(const Functions& gl, GLint target, GLint format, GLuint& texture)
{
	prepareTexture(gl, target, format, texture);

	Texture::Storage(gl, target, 1, format, mWidthStored, mHeightStored, mDepthStored);
	GLU_EXPECT_NO_ERROR(gl.getError(), "TexStorage");

	return true;
}

/** Writing data to generated texture
 *
 * @param gl           GL API functions
 * @param target       Target for which texture is binded
 * @param format       Texture internal format
 * @param texture      Texture object
 *
 * @return Returns true if no error occurred, otherwise throws an exception.
 */
bool SparseTextureCommitmentTestCase::writeDataToTexture(const Functions& gl, GLint target, GLint format,
														 GLuint& texture)
{
	DE_UNREF(format);
	DE_UNREF(texture);

	TransferFormat transferFormat = glu::getTransferFormat(mTextureFormatStored);

	GLuint texSize = mWidthStored * mHeightStored * mDepthStored;

	GLubyte* data = (GLubyte*)malloc(texSize * mTextureFormatStored.getPixelSize());

	deMemset(data, 128, texSize * mTextureFormatStored.getPixelSize());

	Texture::SubImage(gl, target, 0, 0, 0, 0, mWidthStored, mHeightStored, mDepthStored, transferFormat.format,
					  transferFormat.dataType, (GLvoid*)data);

	GLU_EXPECT_NO_ERROR(gl.getError(), "SubImage");

	free(data);

	return true;
}

/** Verify if data stored in texture is as expected
 *
 * @param gl           GL API functions
 * @param target       Target for which texture is binded
 * @param format       Texture internal format
 * @param texture      Texture object
 *
 * @return Returns true if data is as expected, false if not, throws an exception if error occurred.
 */
bool SparseTextureCommitmentTestCase::verifyTextureData(const Functions& gl, GLint target, GLint format,
														GLuint& texture)
{
	DE_UNREF(format);
	DE_UNREF(texture);

	TransferFormat transferFormat = glu::getTransferFormat(mTextureFormatStored);

	GLuint texSize = mWidthStored * mHeightStored * mDepthStored;

	GLubyte* data	 = (GLubyte*)malloc(texSize * mTextureFormatStored.getPixelSize());
	GLubyte* out_data = (GLubyte*)malloc(texSize * mTextureFormatStored.getPixelSize());

	deMemset(data, 128, texSize * mTextureFormatStored.getPixelSize());

	bool result = true;

	GLint error = GL_NO_ERROR;

	if (target != GL_TEXTURE_CUBE_MAP)
	{
		deMemset(out_data, 255, texSize * mTextureFormatStored.getPixelSize());

		gl.getTexImage(target, 0, transferFormat.format, transferFormat.dataType, (GLvoid*)out_data);
		error = gl.getError();

		if (deMemCmp(out_data, data, texSize * mTextureFormatStored.getPixelSize()) != 0)
			result = false;
	}
	else
	{
		std::vector<GLint> subTargets;

		subTargets.push_back(GL_TEXTURE_CUBE_MAP_POSITIVE_X);
		subTargets.push_back(GL_TEXTURE_CUBE_MAP_NEGATIVE_X);
		subTargets.push_back(GL_TEXTURE_CUBE_MAP_POSITIVE_Y);
		subTargets.push_back(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y);
		subTargets.push_back(GL_TEXTURE_CUBE_MAP_POSITIVE_Z);
		subTargets.push_back(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z);

		for (std::vector<glw::GLint>::const_iterator iter = subTargets.begin(); iter != subTargets.end(); ++iter)
		{
			const GLint& subTarget = *iter;

			m_testCtx.getLog() << tcu::TestLog::Message << "Testing subTarget: " << subTarget
							   << tcu::TestLog::EndMessage;

			deMemset(out_data, 255, texSize * mTextureFormatStored.getPixelSize());

			gl.getTexImage(subTarget, 0, transferFormat.format, transferFormat.dataType, (GLvoid*)out_data);
			error = gl.getError();

			if (deMemCmp(out_data, data, texSize * mTextureFormatStored.getPixelSize()) != 0)
				result = false;

			if (!result || error != GL_NO_ERROR)
				break;
		}
	}

	GLU_EXPECT_NO_ERROR(error, "glGetTexImage");

	free(data);
	free(out_data);

	return result;
}

/** Commit texture page using texPageCommitment function
 *
 * @param gl           GL API functions
 * @param target       Target for which texture is binded
 * @param format       Texture internal format
 * @param texture      Texture object
 *
 * @return Returns true if no error occurred, otherwise throws an exception.
 */
bool SparseTextureCommitmentTestCase::commitTexturePage(const Functions& gl, GLint target, GLint format,
														GLuint& texture)
{
	Texture::Bind(gl, texture, target);

	texPageCommitment(gl, target, format, texture, 0, 0, 0, 0, mWidthStored, mHeightStored, mDepthStored, GL_TRUE);
	GLU_EXPECT_NO_ERROR(gl.getError(), "texPageCommitment");

	return true;
}

/** Verifies if gltexPageCommitment generates INVALID_OPERATION error in expected use cases
 *
 * @param gl           GL API functions
 * @param target       Target for which texture is binded
 * @param format       Texture internal format
 * @param texture      Texture object
 *
 * @return Returns true if no error occurred, otherwise throws an exception.
 */
bool SparseTextureCommitmentTestCase::verifyInvalidOperationErrors(const Functions& gl, GLint target, GLint format,
																   GLuint& texture)
{
	bool result = true;

	GLint pageSizeX;
	GLint pageSizeY;
	GLint pageSizeZ;
	GLint minDepth = SparseTextureUtils::getTargetDepth(target);
	SparseTextureUtils::getTexturePageSizes(gl, target, format, pageSizeX, pageSizeY, pageSizeZ);

	// Case 1 - texture is not GL_TEXTURE_IMMUTABLE_FORMAT
	Texture::Generate(gl, texture);
	Texture::Bind(gl, texture, target);

	GLint immutableFormat;

	gl.getTexParameteriv(target, GL_TEXTURE_IMMUTABLE_FORMAT, &immutableFormat);
	GLU_EXPECT_NO_ERROR(gl.getError(), "getTexParameteriv error occurred for GL_TEXTURE_IMMUTABLE_FORMAT");

	if (immutableFormat == GL_FALSE)
	{
		texPageCommitment(gl, target, format, texture, 0, 0, 0, 0, mWidthStored, mHeightStored, mDepthStored, GL_TRUE);
		result =
			SparseTextureUtils::verifyError(m_testCtx, "texPageCommitment [A1]", gl.getError(), GL_INVALID_OPERATION);
		if (!result)
			goto verifing_invalid_operation_end;
	}

	// Case 2 - texture is not TEXTURE_SPARSE_ARB
	allocateTexture(gl, target, format, texture);

	texPageCommitment(gl, target, format, texture, 0, 0, 0, 0, mWidthStored, mHeightStored, mDepthStored, GL_TRUE);
	result = SparseTextureUtils::verifyError(m_testCtx, "texPageCommitment [A2]", gl.getError(), GL_INVALID_OPERATION);
	if (!result)
		goto verifing_invalid_operation_end;

	// Sparse allocate texture
	Texture::Delete(gl, texture);
	sparseAllocateTexture(gl, target, format, texture);

	// Case 3 - commitment sizes greater than expected
	texPageCommitment(gl, target, format, texture, 0, 0, 0, 0, mWidthStored + pageSizeX, mHeightStored, mDepthStored,
					  GL_TRUE);
	result = SparseTextureUtils::verifyError(m_testCtx, "texPageCommitment [A3]", gl.getError(), GL_INVALID_OPERATION);
	if (!result)
		goto verifing_invalid_operation_end;

	texPageCommitment(gl, target, format, texture, 0, 0, 0, 0, mWidthStored, mHeightStored + pageSizeY, mDepthStored,
					  GL_TRUE);
	result = SparseTextureUtils::verifyError(m_testCtx, "texPageCommitment [A4]", gl.getError(), GL_INVALID_OPERATION);
	if (!result)
		goto verifing_invalid_operation_end;

	if (target == GL_TEXTURE_3D || target == GL_TEXTURE_2D_ARRAY || target == GL_TEXTURE_CUBE_MAP_ARRAY)
	{
		texPageCommitment(gl, target, format, texture, 0, 0, 0, 0, mWidthStored, mHeightStored,
						  mDepthStored + pageSizeZ, GL_TRUE);
		result =
			SparseTextureUtils::verifyError(m_testCtx, "texPageCommitment [A5]", gl.getError(), GL_INVALID_OPERATION);
		if (!result)
			goto verifing_invalid_operation_end;
	}

	// Case 4 - commitment sizes not multiple of corresponding page sizes
	if (pageSizeX > 1)
	{
		texPageCommitment(gl, target, format, texture, 0, 0, 0, 0, 1, mHeightStored, mDepthStored, GL_TRUE);
		result =
			SparseTextureUtils::verifyError(m_testCtx, "texPageCommitment [A6]", gl.getError(), GL_INVALID_OPERATION);
		if (!result)
			goto verifing_invalid_operation_end;
	}

	if (pageSizeY > 1)
	{
		texPageCommitment(gl, target, format, texture, 0, 0, 0, 0, mWidthStored, 1, mDepthStored, GL_TRUE);
		result =
			SparseTextureUtils::verifyError(m_testCtx, "texPageCommitment [A7]", gl.getError(), GL_INVALID_OPERATION);
		if (!result)
			goto verifing_invalid_operation_end;
	}

	if (pageSizeZ > 1)
	{
		if (target == GL_TEXTURE_3D || target == GL_TEXTURE_2D_ARRAY || target == GL_TEXTURE_CUBE_MAP_ARRAY)
		{
			texPageCommitment(gl, target, format, texture, 0, 0, 0, 0, mWidthStored, mHeightStored, minDepth, GL_TRUE);
			result = SparseTextureUtils::verifyError(m_testCtx, "texPageCommitment [A8]", gl.getError(),
													 GL_INVALID_OPERATION);
			if (!result)
				goto verifing_invalid_operation_end;
		}
	}

verifing_invalid_operation_end:

	Texture::Delete(gl, texture);

	return result;
}

/** Verifies if texPageCommitment generates INVALID_VALUE error in expected use cases
 *
 * @param gl           GL API functions
 * @param target       Target for which texture is binded
 * @param format       Texture internal format
 * @param texture      Texture object
 *
 * @return Returns true if no error occurred, otherwise throws an exception.
 */
bool SparseTextureCommitmentTestCase::verifyInvalidValueErrors(const Functions& gl, GLint target, GLint format,
															   GLuint& texture)
{
	bool result = true;

	GLint pageSizeX;
	GLint pageSizeY;
	GLint pageSizeZ;
	GLint minDepth = SparseTextureUtils::getTargetDepth(target);
	SparseTextureUtils::getTexturePageSizes(gl, target, format, pageSizeX, pageSizeY, pageSizeZ);

	sparseAllocateTexture(gl, target, format, texture);

	// Case 1 - commitment sizes greater than expected
	texPageCommitment(gl, target, format, texture, 0, 1, 0, 0, mWidthStored + pageSizeX, mHeightStored, minDepth,
					  GL_TRUE);
	result = SparseTextureUtils::verifyError(m_testCtx, "texPageCommitment [B1]", gl.getError(), GL_INVALID_VALUE);
	if (!result)
		goto verifing_invalid_value_end;

	texPageCommitment(gl, target, format, texture, 0, 0, 1, 0, mWidthStored, mHeightStored + pageSizeY, minDepth,
					  GL_TRUE);
	result = SparseTextureUtils::verifyError(m_testCtx, "texPageCommitment [B2]", gl.getError(), GL_INVALID_VALUE);
	if (!result)
		goto verifing_invalid_value_end;

	if (target == GL_TEXTURE_3D || target == GL_TEXTURE_2D_ARRAY || target == GL_TEXTURE_CUBE_MAP_ARRAY)
	{
		texPageCommitment(gl, target, format, texture, 0, 0, 0, minDepth, mWidthStored, mHeightStored,
						  mDepthStored + pageSizeZ, GL_TRUE);
		result = SparseTextureUtils::verifyError(m_testCtx, "texPageCommitment [B3]", gl.getError(), GL_INVALID_VALUE);
		if (!result)
			goto verifing_invalid_value_end;
	}

verifing_invalid_value_end:

	Texture::Delete(gl, texture);

	return result;
}

/** Constructor.
 *
 *  @param context     Rendering context
 */
SparseDSATextureCommitmentTestCase::SparseDSATextureCommitmentTestCase(deqp::Context& context)
	: SparseTextureCommitmentTestCase(context, "SparseDSATextureCommitment",
									  "Verifies texturePageCommitmentEXT functionality added in CTS_ARB_sparse_texture")
{
	/* Left blank intentionally */
}

/** Executes test iteration.
 *
 *  @return Returns STOP when test has finished executing, CONTINUE if more iterations are needed.
 */
tcu::TestNode::IterateResult SparseDSATextureCommitmentTestCase::iterate()
{
	if (!m_context.getContextInfo().isExtensionSupported("GL_EXT_direct_state_access"))
	{
		m_testCtx.setTestResult(QP_TEST_RESULT_NOT_SUPPORTED, "GL_EXT_direct_state_access extension is not supported.");
		return STOP;
	}

	const Functions& gl = m_context.getRenderContext().getFunctions();

	bool result = true;

	GLuint texture;

	for (std::vector<glw::GLint>::const_iterator iter = mSupportedTargets.begin(); iter != mSupportedTargets.end();
		 ++iter)
	{
		const GLint& target = *iter;

		for (std::vector<glw::GLint>::const_iterator formIter = mSupportedInternalFormats.begin();
			 formIter != mSupportedInternalFormats.end(); ++formIter)
		{
			const GLint& format = *formIter;

			m_testCtx.getLog() << tcu::TestLog::Message
							   << "Testing DSA sparse texture commitment for target: " << target
							   << ", format: " << format << tcu::TestLog::EndMessage;

			//Checking if written data into committed region is as expected
			sparseAllocateTexture(gl, target, format, texture);
			commitTexturePage(gl, target, format, texture);
			writeDataToTexture(gl, target, format, texture);
			result = verifyTextureData(gl, target, format, texture);

			Texture::Delete(gl, texture);

			//verify errors
			result = verifyInvalidOperationErrors(gl, target, format, texture) &&
					 verifyInvalidValueErrors(gl, target, format, texture);

			if (!result)
			{
				m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Fail");
				return STOP;
			}
		}
	}

	m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
	return STOP;
}

/** Bind DSA texturePageCommitmentEXT function
 *
 * @param gl           GL API functions
 * @param target       Target for which texture is binded
 * @param format       Texture internal format
 * @param texture      Texture object
 * @param xOffset      Texture commitment x offset
 * @param yOffset      Texture commitment y offset
 * @param zOffset      Texture commitment z offset
 * @param width        Texture commitment width
 * @param height       Texture commitment height
 * @param depth        Texture commitment depth
 * @param commit       Commit or de-commit indicator
 **/
void SparseDSATextureCommitmentTestCase::texPageCommitment(const glw::Functions& gl, glw::GLint target,
														   glw::GLint format, glw::GLuint& texture, GLint level,
														   GLint xOffset, GLint yOffset, GLint zOffset, GLint width,
														   GLint height, GLint depth, GLboolean commit)
{
	DE_UNREF(target);
	DE_UNREF(format);
	gl.texturePageCommitmentEXT(texture, level, xOffset, yOffset, zOffset, width, height, depth, commit);
}

/** Constructor.
 *
 *  @param context Rendering context.
 */
SparseTextureTests::SparseTextureTests(deqp::Context& context)
	: TestCaseGroup(context, "sparse_texture_tests", "Verify conformance of CTS_ARB_sparse_texture implementation")
{
}

/** Initializes the test group contents. */
void SparseTextureTests::init()
{
	addChild(new TextureParameterQueriesTestCase(m_context));
	addChild(new InternalFormatQueriesTestCase(m_context));
	addChild(new SimpleQueriesTestCase(m_context));
	addChild(new SparseTextureAllocationTestCase(m_context));
	addChild(new SparseTextureCommitmentTestCase(m_context));
	addChild(new SparseDSATextureCommitmentTestCase(m_context));
}

} /* gl4cts namespace */
