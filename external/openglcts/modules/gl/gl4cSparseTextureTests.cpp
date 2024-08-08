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
#include "gluStrUtil.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string.h>
#include <vector>

using namespace glw;
using namespace glu;

namespace gl4cts
{

std::vector<GLint> SparseTextureCommitmentTargets = {GL_TEXTURE_2D,       GL_TEXTURE_2D_ARRAY,
                                                     GL_TEXTURE_CUBE_MAP, GL_TEXTURE_CUBE_MAP_ARRAY,
                                                     GL_TEXTURE_3D,       GL_TEXTURE_RECTANGLE};

std::vector<GLint> SparseTextureCommitmentFormats = {
    GL_R8,         GL_R8_SNORM,   GL_R16,          GL_R16_SNORM,      GL_RG8,         GL_RG8_SNORM,
    GL_RG16,       GL_RG16_SNORM, GL_RGB565,       GL_RGBA8,          GL_RGBA8_SNORM, GL_RGB10_A2,
    GL_RGB10_A2UI, GL_RGBA16,     GL_RGBA16_SNORM, GL_R16F,           GL_RG16F,       GL_RGBA16F,
    GL_R32F,       GL_RG32F,      GL_RGBA32F,      GL_R11F_G11F_B10F, GL_RGB9_E5,     GL_R8I,
    GL_R8UI,       GL_R16I,       GL_R16UI,        GL_R32I,           GL_R32UI,       GL_RG8I,
    GL_RG8UI,      GL_RG16I,      GL_RG16UI,       GL_RG32I,          GL_RG32UI,      GL_RGBA8I,
    GL_RGBA8UI,    GL_RGBA16I,    GL_RGBA16UI,     GL_RGBA32I};

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
bool SparseTextureUtils::verifyQueryError(std::stringstream &log, const char *funcName, GLint target, GLint pname,
                                          GLint error, GLint expectedError)
{
    if (error != expectedError)
    {
        log << "QueryError [" << funcName << " return wrong error code"
            << ", target: " << target << ", pname: " << pname << ", expected: " << expectedError
            << ", returned: " << error << "] - ";

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
bool SparseTextureUtils::verifyError(std::stringstream &log, const char *funcName, GLint error, GLint expectedError)
{
    if (error != expectedError)
    {
        log << "Error [" << funcName << " return wrong error code "
            << ", expectedError: " << expectedError << ", returnedError: " << error << "] - ";

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

    if (target == GL_TEXTURE_3D || target == GL_TEXTURE_1D_ARRAY || target == GL_TEXTURE_2D_ARRAY ||
        target == GL_TEXTURE_2D_MULTISAMPLE_ARRAY || target == GL_TEXTURE_2D || target == GL_TEXTURE_RECTANGLE ||
        target == GL_TEXTURE_CUBE_MAP)
    {
        depth = 1;
    }
    else if (target == GL_TEXTURE_CUBE_MAP_ARRAY)
        depth = 6;
    else
        depth = 0;

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
void SparseTextureUtils::getTexturePageSizes(const glw::Functions &gl, glw::GLint target, glw::GLint format,
                                             glw::GLint &pageSizeX, glw::GLint &pageSizeY, glw::GLint &pageSizeZ)
{
    gl.getInternalformativ(target, format, GL_VIRTUAL_PAGE_SIZE_X_ARB, 1, &pageSizeX);
    GLU_EXPECT_NO_ERROR(gl.getError(), "getInternalformativ error occurred for GL_VIRTUAL_PAGE_SIZE_X_ARB");

    gl.getInternalformativ(target, format, GL_VIRTUAL_PAGE_SIZE_Y_ARB, 1, &pageSizeY);
    GLU_EXPECT_NO_ERROR(gl.getError(), "getInternalformativ error occurred for GL_VIRTUAL_PAGE_SIZE_Y_ARB");

    gl.getInternalformativ(target, format, GL_VIRTUAL_PAGE_SIZE_Z_ARB, 1, &pageSizeZ);
    GLU_EXPECT_NO_ERROR(gl.getError(), "getInternalformativ error occurred for GL_VIRTUAL_PAGE_SIZE_Z_ARB");
}

/** Calculate texture size for specific mipmap
 *
 * @param target  GL functions
 * @param state   Texture current state
 * @param level   Texture mipmap level
 * @param width   Texture output width
 * @param height  Texture output height
 * @param depth   Texture output depth
 **/
void SparseTextureUtils::getTextureLevelSize(GLint target, TextureState &state, GLint level, GLint &width,
                                             GLint &height, GLint &depth)
{
    width = state.width / (int)pow(2, level);
    if (target == GL_TEXTURE_1D || target == GL_TEXTURE_1D_ARRAY)
        height = 1;
    else
        height = state.height / (int)pow(2, level);

    if (target == GL_TEXTURE_3D)
        depth = state.depth / (int)pow(2, level);
    else if (target == GL_TEXTURE_1D_ARRAY || target == GL_TEXTURE_2D_ARRAY || target == GL_TEXTURE_CUBE_MAP_ARRAY ||
             target == GL_TEXTURE_2D_MULTISAMPLE_ARRAY)
        depth = state.depth;
    else
        depth = 1;
}

/** Returns texture target name if exist, otherwise hex numer
 *
 * @param target  Value of texture target
 **/
std::string SparseTextureUtils::getTextureTargetString(GLint target)
{
    auto targetName = glu::getTextureTargetName(target);

    if (targetName == nullptr)
    {
        switch (target)
        {
        case 0x8C18:
            return "texture_1d_array";
        case 0x84F5:
            return "texture_rectangle";
        case 0x8D41:
            return "renderbuffer";
        default:
            return "null";
        }
    }
    std::string name(targetName);
    removeGLPrefixAndLowerCase(name);

    return name;
}

/** Returns texture format name if exist, otherwise hex value
 *
 * @param format  Value of texture format
 **/
std::string SparseTextureUtils::getTextureFormatString(GLint format)
{
    auto formatName = glu::getTextureFormatName(format);

    if (formatName == nullptr)
    {
        switch (format)
        {
        default:
            return "null";
        }
    }
    std::string name(formatName);
    removeGLPrefixAndLowerCase(name);

    return name;
}

/** Removes GL_ prefix from texture name and lowercases
 *
 * @param name  Texture name to lowercase and remove GL_ prefix
 **/
void SparseTextureUtils::removeGLPrefixAndLowerCase(std::string &name)
{
    std::string remove("GL_");
    std::size_t ind = name.find(remove);
    if (ind != std::string::npos)
    {
        name.erase(ind, remove.length());
    }
    std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) { return std::tolower(c); });
}

/* Texture static fields */
const GLuint Texture::m_invalid_id = -1;

/** Bind texture to target
 *
 * @param gl       GL API functions
 * @param id       Id of texture
 * @param tex_type Type of texture
 **/
void Texture::Bind(const Functions &gl, GLuint id, GLenum target)
{
    gl.bindTexture(target, id);
    GLU_EXPECT_NO_ERROR(gl.getError(), "BindTexture");
}

/** Generate texture instance
 *
 * @param gl     GL functions
 * @param out_id Id of texture
 **/
void Texture::Generate(const Functions &gl, GLuint &out_id)
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
void Texture::Delete(const Functions &gl, GLuint &id)
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
void Texture::Storage(const Functions &gl, GLenum target, GLsizei levels, GLenum internal_format, GLuint width,
                      GLuint height, GLuint depth)
{
    switch (target)
    {
    case GL_TEXTURE_1D:
        gl.texStorage1D(target, levels, internal_format, width);
        break;
    case GL_TEXTURE_1D_ARRAY:
        gl.texStorage2D(target, levels, internal_format, width, depth);
        break;
    case GL_TEXTURE_2D:
    case GL_TEXTURE_RECTANGLE:
    case GL_TEXTURE_CUBE_MAP:
        gl.texStorage2D(target, levels, internal_format, width, height);
        break;
    case GL_TEXTURE_3D:
    case GL_TEXTURE_2D_ARRAY:
    case GL_TEXTURE_CUBE_MAP_ARRAY:
        gl.texStorage3D(target, levels, internal_format, width, height, depth);
        break;
    case GL_TEXTURE_2D_MULTISAMPLE:
        gl.texStorage2DMultisample(target, levels /* samples */, internal_format, width, height, GL_TRUE);
        break;
    case GL_TEXTURE_2D_MULTISAMPLE_ARRAY:
        gl.texStorage3DMultisample(target, levels /* samples */, internal_format, width, height, depth, GL_TRUE);
        break;
    default:
        TCU_FAIL("Invliad enum");
    }
}

/** Get texture data
 *
 * @param gl       GL functions
 * @param target   Texture target
 * @param format   Format of data
 * @param type     Type of data
 * @param out_data Buffer for data
 **/
void Texture::GetData(const glw::Functions &gl, glw::GLint level, glw::GLenum target, glw::GLenum format,
                      glw::GLenum type, glw::GLvoid *out_data)
{
    gl.getTexImage(target, level, format, type, out_data);
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
void Texture::SubImage(const glw::Functions &gl, glw::GLenum target, glw::GLint level, glw::GLint x, glw::GLint y,
                       glw::GLint z, glw::GLsizei width, glw::GLsizei height, glw::GLsizei depth, glw::GLenum format,
                       glw::GLenum type, const glw::GLvoid *pixels)
{
    switch (target)
    {
    case GL_TEXTURE_1D:
        gl.texSubImage1D(target, level, x, width, format, type, pixels);
        break;
    case GL_TEXTURE_1D_ARRAY:
        gl.texSubImage2D(target, level, x, y, width, depth, format, type, pixels);
        break;
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
    }
}

/** Constructor.
 *
 *  @param context     Rendering context
 */
TextureParameterQueriesTestCase::TextureParameterQueriesTestCase(deqp::Context &context, const char *name,
                                                                 const char *description, GLint supportedTarget,
                                                                 GLint notSupportedTarget)
    : TestCase(context, name, description)
    , mSupportedTarget(supportedTarget)
    , mNotSupportedTarget(notSupportedTarget)
{
    /* Left blank intentionally */
}

/** Executes test iteration.
 *
 *  @return Returns STOP when test has finished executing, CONTINUE if more iterations are needed.
 */
tcu::TestNode::IterateResult TextureParameterQueriesTestCase::iterate()
{
    if (!m_context.getContextInfo().isExtensionSupported("GL_ARB_sparse_texture"))
    {
        m_testCtx.setTestResult(QP_TEST_RESULT_NOT_SUPPORTED, "Not Supported");
        return STOP;
    }

    const Functions &gl = m_context.getRenderContext().getFunctions();

    bool result = true;
    GLuint texture;

    if (mSupportedTarget != GL_INVALID_VALUE)
    {
        mLog.str("");

        Texture::Generate(gl, texture);
        Texture::Bind(gl, texture, mSupportedTarget);

        result = testTextureSparseARB(gl, mSupportedTarget) && testVirtualPageSizeIndexARB(gl, mSupportedTarget) &&
                 testNumSparseLevelsARB(gl, mSupportedTarget);

        Texture::Delete(gl, texture);

        if (!result)
        {
            m_testCtx.getLog() << tcu::TestLog::Message << mLog.str() << "Fail [positive tests]"
                               << tcu::TestLog::EndMessage;
            m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Fail");
            return STOP;
        }
    }

    if (m_context.getContextInfo().isExtensionSupported("GL_ARB_sparse_texture2"))
    {
        m_testCtx.setTestResult(QP_TEST_RESULT_NOT_SUPPORTED, "Not Supported");
        return STOP;
    }
    else if (mNotSupportedTarget != GL_INVALID_VALUE)
    {
        mLog.str("");

        Texture::Generate(gl, texture);
        Texture::Bind(gl, texture, mNotSupportedTarget);

        result = testTextureSparseARB(gl, mNotSupportedTarget, GL_INVALID_VALUE);

        Texture::Delete(gl, texture);

        if (!result)
        {
            m_testCtx.getLog() << tcu::TestLog::Message << mLog.str() << "Fail [positive tests]"
                               << tcu::TestLog::EndMessage;
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
bool TextureParameterQueriesTestCase::testTextureSparseARB(const Functions &gl, GLint target, GLint expectedError)
{
    const GLint pname = GL_TEXTURE_SPARSE_ARB;

    bool result = true;

    GLint testValueInt;
    GLuint testValueUInt;
    GLfloat testValueFloat;

    mLog << "Testing TEXTURE_SPARSE_ARB for target: " << target << " - ";

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
            result = SparseTextureUtils::verifyQueryError(mLog, "glTexParameteri", target, pname, gl.getError(),
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
            result = SparseTextureUtils::verifyQueryError(mLog, "glTexParameterf", target, pname, gl.getError(),
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
            result = SparseTextureUtils::verifyQueryError(mLog, "glTexParameteriv", target, pname, gl.getError(),
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
            result = SparseTextureUtils::verifyQueryError(mLog, "glTexParameterfv", target, pname, gl.getError(),
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
            result = SparseTextureUtils::verifyQueryError(mLog, "glTexParameterIiv", target, pname, gl.getError(),
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
            result = SparseTextureUtils::verifyQueryError(mLog, "glTexParameterIuiv", target, pname, gl.getError(),
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
bool TextureParameterQueriesTestCase::testVirtualPageSizeIndexARB(const Functions &gl, GLint target,
                                                                  GLint expectedError)
{
    const GLint pname = GL_VIRTUAL_PAGE_SIZE_INDEX_ARB;

    bool result = true;

    GLint testValueInt;
    GLuint testValueUInt;
    GLfloat testValueFloat;

    mLog << "Testing VIRTUAL_PAGE_SIZE_INDEX_ARB for target: " << target << " - ";

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
            result = SparseTextureUtils::verifyQueryError(mLog, "glTexParameteri", target, pname, gl.getError(),
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
            result = SparseTextureUtils::verifyQueryError(mLog, "glTexParameterf", target, pname, gl.getError(),
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
            result = SparseTextureUtils::verifyQueryError(mLog, "glTexParameteriv", target, pname, gl.getError(),
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
            result = SparseTextureUtils::verifyQueryError(mLog, "glTexParameterfv", target, pname, gl.getError(),
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
            result = SparseTextureUtils::verifyQueryError(mLog, "glTexParameterIiv", target, pname, gl.getError(),
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
            result = SparseTextureUtils::verifyQueryError(mLog, "glTexParameterIuiv", target, pname, gl.getError(),
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
bool TextureParameterQueriesTestCase::testNumSparseLevelsARB(const Functions &gl, GLint target)
{
    const GLint pname = GL_NUM_SPARSE_LEVELS_ARB;

    bool result = true;

    GLint value_int;
    GLuint value_uint;
    GLfloat value_float;

    mLog << "Testing NUM_SPARSE_LEVELS_ARB for target: " << target << " - ";

    gl.getTexParameteriv(target, pname, &value_int);
    result = SparseTextureUtils::verifyError(mLog, "glGetTexParameteriv", gl.getError(), GL_NO_ERROR);

    if (result)
    {
        gl.getTexParameterfv(target, pname, &value_float);
        result = SparseTextureUtils::verifyError(mLog, "glGetTexParameterfv", gl.getError(), GL_NO_ERROR);

        if (result)
        {
            gl.getTexParameterIiv(target, pname, &value_int);
            result = SparseTextureUtils::verifyError(mLog, "glGetGexParameterIiv", gl.getError(), GL_NO_ERROR);

            if (result)
            {
                gl.getTexParameterIuiv(target, pname, &value_uint);
                result = SparseTextureUtils::verifyError(mLog, "getTexParameterIuiv", gl.getError(), GL_NO_ERROR);
            }
        }
    }

    return result;
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
bool TextureParameterQueriesTestCase::checkGetTexParameter(const Functions &gl, GLint target, GLint pname,
                                                           GLint expected)
{
    bool result = true;

    GLint value_int;
    GLuint value_uint;
    GLfloat value_float;

    mLog << "Testing GetTexParameter for target: " << target << " - ";

    gl.getTexParameteriv(target, pname, &value_int);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glGetTexParameteriv error occurred");
    if (value_int != expected)
    {
        mLog << "glGetTexParameteriv return wrong value"
             << ", target: " << target << ", pname: " << pname << ", expected: " << expected
             << ", returned: " << value_int << " - ";

        result = false;
    }

    gl.getTexParameterfv(target, pname, &value_float);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glGetTexParameterfv error occurred");
    if ((GLint)value_float != expected)
    {
        mLog << "glGetTexParameterfv return wrong value"
             << ", target: " << target << ", pname: " << pname << ", expected: " << expected
             << ", returned: " << (GLint)value_float << " - ";

        result = false;
    }

    gl.getTexParameterIiv(target, pname, &value_int);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glGetGexParameterIiv error occurred");
    if (value_int != expected)
    {
        mLog << "glGetGexParameterIiv return wrong value"
             << ", target: " << target << ", pname: " << pname << ", expected: " << expected
             << ", returned: " << value_int << " - ";

        result = false;
    }

    gl.getTexParameterIuiv(target, pname, &value_uint);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glGetGexParameterIui error occurred");
    if ((GLint)value_uint != expected)
    {
        mLog << "glGetGexParameterIui return wrong value"
             << ", target: " << target << ", pname: " << pname << ", expected: " << expected
             << ", returned: " << (GLint)value_uint << " - ";

        result = false;
    }

    return result;
}

/** Constructor.
 *
 *  @param context     Rendering context
 */
InternalFormatQueriesTestCase::InternalFormatQueriesTestCase(deqp::Context &context, const char *name,
                                                             const char *description, GLint target, GLint format)
    : TestCase(context, name, description)
    , mTarget(target)
    , mFormat(format)
{
    /* Left blank intentionally */
}

/** Executes test iteration.
 *
 *  @return Returns STOP when test has finished executing, CONTINUE if more iterations are needed.
 */
tcu::TestNode::IterateResult InternalFormatQueriesTestCase::iterate()
{
    if (!m_context.getContextInfo().isExtensionSupported("GL_ARB_sparse_texture"))
    {
        m_testCtx.setTestResult(QP_TEST_RESULT_NOT_SUPPORTED, "Not Supported");
        return STOP;
    }

    const Functions &gl = m_context.getRenderContext().getFunctions();

    bool result = true;

    mLog << "Testing getInternalformativ - ";

    GLint value;

    gl.getInternalformativ(mTarget, mFormat, GL_NUM_VIRTUAL_PAGE_SIZES_ARB, 1, &value);
    GLU_EXPECT_NO_ERROR(gl.getError(), "getInternalformativ error occurred for GL_NUM_VIRTUAL_PAGE_SIZES_ARB");
    if (value == 0)
    {
        mLog << "getInternalformativ for GL_NUM_VIRTUAL_PAGE_SIZES_ARB, target: " << mTarget << ", format: " << mFormat
             << " returns wrong value: " << value << " - ";

        result = false;
    }

    if (result)
    {
        GLint pageSizeX;
        GLint pageSizeY;
        GLint pageSizeZ;
        SparseTextureUtils::getTexturePageSizes(gl, mTarget, mFormat, pageSizeX, pageSizeY, pageSizeZ);
    }
    else
    {
        m_testCtx.getLog() << tcu::TestLog::Message << mLog.str() << "Fail" << tcu::TestLog::EndMessage;
        m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Fail");
        return STOP;
    }

    m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");

    return STOP;
}

/** Constructor.
 *
 *  @param context     Rendering context
 */
SimpleQueriesTestCase::SimpleQueriesTestCase(deqp::Context &context)
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
    if (!m_context.getContextInfo().isExtensionSupported("GL_ARB_sparse_texture"))
    {
        m_testCtx.setTestResult(QP_TEST_RESULT_NOT_SUPPORTED, "Not Supported");
        return STOP;
    }

    const Functions &gl = m_context.getRenderContext().getFunctions();

    testSipmleQueries(gl, GL_MAX_SPARSE_TEXTURE_SIZE_ARB);
    testSipmleQueries(gl, GL_MAX_SPARSE_3D_TEXTURE_SIZE_ARB);
    testSipmleQueries(gl, GL_MAX_SPARSE_ARRAY_TEXTURE_LAYERS_ARB);
    testSipmleQueries(gl, GL_SPARSE_TEXTURE_FULL_ARRAY_CUBE_MIPMAPS_ARB);

    m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
    return STOP;
}

void SimpleQueriesTestCase::testSipmleQueries(const Functions &gl, GLint pname)
{
    std::stringstream log;
    log << "Testing simple query for pname: " << pname << " - ";

    bool result = true;

    GLint value_int;
    GLint64 value_int64;
    GLfloat value_float;
    GLdouble value_double;
    GLboolean value_bool;

    gl.getIntegerv(pname, &value_int);
    result = SparseTextureUtils::verifyError(log, "getIntegerv", gl.getError(), GL_NO_ERROR);

    if (result)
    {
        gl.getInteger64v(pname, &value_int64);
        result = SparseTextureUtils::verifyError(log, "getInteger64v", gl.getError(), GL_NO_ERROR);

        if (result)
        {
            gl.getFloatv(pname, &value_float);
            result = SparseTextureUtils::verifyError(log, "getFloatv", gl.getError(), GL_NO_ERROR);

            if (result)
            {
                gl.getDoublev(pname, &value_double);
                result = SparseTextureUtils::verifyError(log, "getDoublev", gl.getError(), GL_NO_ERROR);

                if (result)
                {
                    gl.getBooleanv(pname, &value_bool);
                    result = SparseTextureUtils::verifyError(log, "getBooleanv", gl.getError(), GL_NO_ERROR);
                }
            }
        }
    }

    if (!result)
    {
        TCU_FAIL(log.str().c_str());
    }
}

/** Constructor.
 *
 *  @param context     Rendering context
 */
SparseTextureAllocationTestCase::SparseTextureAllocationTestCase(deqp::Context &context, const char *name,
                                                                 const char *description, GLint target,
                                                                 GLint fullArrayTarget, GLint format)
    : TestCase(context, name, description)
    , mTarget(target)
    , mFullArrayTarget(fullArrayTarget)
    , mFormat(format)
{
    /* Left blank intentionally */
}

/** Executes test iteration.
 *
 *  @return Returns STOP when test has finished executing, CONTINUE if more iterations are needed.
 */
tcu::TestNode::IterateResult SparseTextureAllocationTestCase::iterate()
{
    if (!m_context.getContextInfo().isExtensionSupported("GL_ARB_sparse_texture"))
    {
        m_testCtx.setTestResult(QP_TEST_RESULT_NOT_SUPPORTED, "Not Supported");
        return STOP;
    }

    const Functions &gl = m_context.getRenderContext().getFunctions();

    if (mTarget != GL_INVALID_VALUE)
    {
        mLog.str("");
        mLog << "Testing sparse texture allocation for target: " << mTarget << ", format: " << mFormat << " - ";

        bool result = positiveTesting(gl, mTarget, mFormat) && verifyTexParameterErrors(gl, mTarget, mFormat) &&
                      verifyTexStorageVirtualPageSizeIndexError(gl, mTarget, mFormat) &&
                      verifyTexStorageFullArrayCubeMipmapsError(gl, mTarget, mFormat) &&
                      verifyTexStorageInvalidValueErrors(gl, mTarget, mFormat);

        if (!result)
        {
            m_testCtx.getLog() << tcu::TestLog::Message << mLog.str() << "Fail" << tcu::TestLog::EndMessage;

            m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Fail");
            return STOP;
        }
    }

    if (mFullArrayTarget != GL_INVALID_VALUE)
    {
        mLog.str("");
        mLog << "Testing sparse texture allocation for target [full array]: " << mFullArrayTarget
             << ", format: " << mFormat << " - ";

        bool result = verifyTexStorageFullArrayCubeMipmapsError(gl, mFullArrayTarget, mFormat);

        if (!result)
        {
            m_testCtx.getLog() << tcu::TestLog::Message << mLog.str() << "Fail" << tcu::TestLog::EndMessage;

            m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Fail");
            return STOP;
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
 *
 * @return Returns true if no errors occurred, false otherwise.
 **/
bool SparseTextureAllocationTestCase::positiveTesting(const Functions &gl, GLint target, GLint format)
{
    mLog << "Positive Testing - ";

    GLuint texture;

    Texture::Generate(gl, texture);
    Texture::Bind(gl, texture, target);

    GLint pageSizeX;
    GLint pageSizeY;
    GLint pageSizeZ;
    GLint depth = SparseTextureUtils::getTargetDepth(target);
    SparseTextureUtils::getTexturePageSizes(gl, target, format, pageSizeX, pageSizeY, pageSizeZ);

    gl.texParameteri(target, GL_TEXTURE_SPARSE_ARB, GL_TRUE);
    if (!SparseTextureUtils::verifyError(mLog, "texParameteri", gl.getError(), GL_NO_ERROR))
    {
        Texture::Delete(gl, texture);
        return false;
    }

    //The <width> and <height> has to be equal for cube map textures
    if (target == GL_TEXTURE_CUBE_MAP || target == GL_TEXTURE_CUBE_MAP_ARRAY)
    {
        if (pageSizeX > pageSizeY)
            pageSizeY = pageSizeX;
        else if (pageSizeX < pageSizeY)
            pageSizeX = pageSizeY;
    }

    Texture::Storage(gl, target, 1, format, pageSizeX, pageSizeY, depth * pageSizeZ);
    if (!SparseTextureUtils::verifyError(mLog, "Texture::Storage", gl.getError(), GL_NO_ERROR))
    {
        Texture::Delete(gl, texture);
        return false;
    }

    Texture::Delete(gl, texture);
    return true;
}

/** Verifies if texParameter* generate proper errors for given target and internal format.
 *
 * @param gl           GL API functions
 * @param target       Target for which texture is binded
 * @param format       Texture internal format
 *
 * @return Returns true if errors are as expected, false otherwise.
 */
bool SparseTextureAllocationTestCase::verifyTexParameterErrors(const Functions &gl, GLint target, GLint format)
{
    mLog << "Verify TexParameter errors - ";

    bool result = true;

    GLuint texture;
    GLint depth;

    Texture::Generate(gl, texture);
    Texture::Bind(gl, texture, target);

    depth = SparseTextureUtils::getTargetDepth(target);

    Texture::Storage(gl, target, 1, format, 8, 8, depth);
    if (!SparseTextureUtils::verifyError(mLog, "TexStorage", gl.getError(), GL_NO_ERROR))
    {
        Texture::Delete(gl, texture);
        return false;
    }

    GLint immutableFormat;

    gl.getTexParameteriv(target, GL_TEXTURE_IMMUTABLE_FORMAT, &immutableFormat);
    if (!SparseTextureUtils::verifyQueryError(mLog, "getTexParameteriv", target, GL_TEXTURE_IMMUTABLE_FORMAT,
                                              gl.getError(), GL_NO_ERROR))
    {
        Texture::Delete(gl, texture);
        return false;
    }

    // Test error only if texture is immutable format, otherwise skip
    if (immutableFormat == GL_TRUE)
    {
        std::vector<IntPair> params;
        params.push_back(IntPair(GL_TEXTURE_SPARSE_ARB, GL_TRUE));
        params.push_back(IntPair(GL_VIRTUAL_PAGE_SIZE_INDEX_ARB, 1));

        for (std::vector<IntPair>::const_iterator iter = params.begin(); iter != params.end(); ++iter)
        {
            const IntPair &param = *iter;

            if (result)
            {
                gl.texParameteri(target, param.first, param.second);
                result = SparseTextureUtils::verifyQueryError(mLog, "glTexParameteri", target, param.first,
                                                              gl.getError(), GL_INVALID_OPERATION);
            }

            if (result)
            {
                gl.texParameterf(target, param.first, (GLfloat)param.second);
                result = SparseTextureUtils::verifyQueryError(mLog, "glTexParameterf", target, param.first,
                                                              gl.getError(), GL_INVALID_OPERATION);
            }

            if (result)
            {
                GLint value = param.second;
                gl.texParameteriv(target, param.first, &value);
                result = SparseTextureUtils::verifyQueryError(mLog, "glTexParameteriv", target, param.first,
                                                              gl.getError(), GL_INVALID_OPERATION);
            }

            if (result)
            {
                GLfloat value = (GLfloat)param.second;
                gl.texParameterfv(target, param.first, &value);
                result = SparseTextureUtils::verifyQueryError(mLog, "glTexParameterfv", target, param.first,
                                                              gl.getError(), GL_INVALID_OPERATION);
            }

            if (result)
            {
                GLint value = param.second;
                gl.texParameterIiv(target, param.first, &value);
                result = SparseTextureUtils::verifyQueryError(mLog, "glTexParameterIiv", target, param.first,
                                                              gl.getError(), GL_INVALID_OPERATION);
            }

            if (result)
            {
                GLuint value = param.second;
                gl.texParameterIuiv(target, param.first, &value);
                result = SparseTextureUtils::verifyQueryError(mLog, "glTexParameterIuiv", target, param.first,
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
bool SparseTextureAllocationTestCase::verifyTexStorageVirtualPageSizeIndexError(const Functions &gl, GLint target,
                                                                                GLint format)
{
    mLog << "Verify VirtualPageSizeIndex errors - ";

    GLuint texture;
    GLint depth;
    GLint numPageSizes;

    Texture::Generate(gl, texture);
    Texture::Bind(gl, texture, target);

    gl.texParameteri(target, GL_TEXTURE_SPARSE_ARB, GL_TRUE);
    if (!SparseTextureUtils::verifyQueryError(mLog, "texParameteri", target, GL_TEXTURE_SPARSE_ARB, gl.getError(),
                                              GL_NO_ERROR))
    {
        Texture::Delete(gl, texture);
        return false;
    }

    gl.getInternalformativ(target, format, GL_NUM_VIRTUAL_PAGE_SIZES_ARB, 1, &numPageSizes);
    if (!SparseTextureUtils::verifyQueryError(mLog, "getInternalformativ", target, GL_NUM_VIRTUAL_PAGE_SIZES_ARB,
                                              gl.getError(), GL_NO_ERROR))
    {
        Texture::Delete(gl, texture);
        return false;
    }

    numPageSizes += 1;
    gl.texParameteri(target, GL_VIRTUAL_PAGE_SIZE_INDEX_ARB, numPageSizes);
    if (!SparseTextureUtils::verifyQueryError(mLog, "texParameteri", target, GL_VIRTUAL_PAGE_SIZE_INDEX_ARB,
                                              gl.getError(), GL_NO_ERROR))
    {
        Texture::Delete(gl, texture);
        return false;
    }

    depth = SparseTextureUtils::getTargetDepth(target);

    Texture::Storage(gl, target, 1, format, 8, 8, depth);
    if (!SparseTextureUtils::verifyError(mLog, "TexStorage", gl.getError(), GL_INVALID_OPERATION))
    {
        Texture::Delete(gl, texture);
        return false;
    }

    Texture::Delete(gl, texture);
    return true;
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
bool SparseTextureAllocationTestCase::verifyTexStorageFullArrayCubeMipmapsError(const Functions &gl, GLint target,
                                                                                GLint format)
{
    mLog << "Verify FullArrayCubeMipmaps errors - ";

    bool result = true;

    GLuint texture;
    GLint depth;

    depth = SparseTextureUtils::getTargetDepth(target);

    GLboolean fullArrayCubeMipmaps;

    gl.getBooleanv(GL_SPARSE_TEXTURE_FULL_ARRAY_CUBE_MIPMAPS_ARB, &fullArrayCubeMipmaps);
    if (!SparseTextureUtils::verifyQueryError(
            mLog, "getBooleanv", target, GL_SPARSE_TEXTURE_FULL_ARRAY_CUBE_MIPMAPS_ARB, gl.getError(), GL_NO_ERROR))
        return false;

    if (fullArrayCubeMipmaps == GL_FALSE &&
        (target == GL_TEXTURE_2D_ARRAY || target == GL_TEXTURE_CUBE_MAP || target == GL_TEXTURE_CUBE_MAP_ARRAY))
    {
        Texture::Generate(gl, texture);
        Texture::Bind(gl, texture, target);

        GLint pageSizeX;
        GLint pageSizeY;
        GLint pageSizeZ;
        SparseTextureUtils::getTexturePageSizes(gl, target, format, pageSizeX, pageSizeY, pageSizeZ);

        gl.texParameteri(target, GL_TEXTURE_SPARSE_ARB, GL_TRUE);

        GLint levels = 4;
        GLint width  = pageSizeX * (int)pow(2, levels - 1);
        GLint height = pageSizeY * (int)pow(2, levels - 1);

        // Check 2 different cases:
        // 1) wrong width
        // 2) wrong height
        if (target == GL_TEXTURE_CUBE_MAP || target == GL_TEXTURE_CUBE_MAP_ARRAY)
        {
            GLint widthHeight = de::max(width, height);
            GLint pageSize    = de::max(pageSizeX, pageSizeY);
            Texture::Storage(gl, target, levels, format, widthHeight + pageSize, widthHeight + pageSize, depth);
            result =
                SparseTextureUtils::verifyError(mLog, "TexStorage [wrong width]", gl.getError(), GL_INVALID_OPERATION);
        }
        else
        {
            Texture::Storage(gl, target, levels, format, width + pageSizeX, height, depth);
            result =
                SparseTextureUtils::verifyError(mLog, "TexStorage [wrong width]", gl.getError(), GL_INVALID_OPERATION);

            if (result)
            {
                Texture::Storage(gl, target, levels, format, width, height + pageSizeY, depth);
                result = SparseTextureUtils::verifyError(mLog, "TexStorage [wrong height]", gl.getError(),
                                                         GL_INVALID_OPERATION);
            }
        }

        Texture::Delete(gl, texture);
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
bool SparseTextureAllocationTestCase::verifyTexStorageInvalidValueErrors(const Functions &gl, GLint target,
                                                                         GLint format)
{
    mLog << "Verify Invalid Value errors - ";

    GLuint texture;

    Texture::Generate(gl, texture);
    Texture::Bind(gl, texture, target);

    GLint pageSizeX;
    GLint pageSizeY;
    GLint pageSizeZ;
    SparseTextureUtils::getTexturePageSizes(gl, target, format, pageSizeX, pageSizeY, pageSizeZ);

    gl.texParameteri(target, GL_TEXTURE_SPARSE_ARB, GL_TRUE);

    GLint width  = pageSizeX;
    GLint height = pageSizeY;
    GLint depth  = SparseTextureUtils::getTargetDepth(target) * pageSizeZ;

    if (target == GL_TEXTURE_3D)
    {
        GLint max3DTextureSize;

        gl.getIntegerv(GL_MAX_SPARSE_3D_TEXTURE_SIZE_ARB, &max3DTextureSize);
        if (!SparseTextureUtils::verifyQueryError(mLog, "getIntegerv", target, GL_MAX_SPARSE_3D_TEXTURE_SIZE_ARB,
                                                  gl.getError(), GL_NO_ERROR))
        {
            Texture::Delete(gl, texture);
            return false;
        }

        // Check 3 different cases:
        // 1) wrong width
        // 2) wrong height
        // 3) wrong depth
        Texture::Storage(gl, target, 1, format, width + max3DTextureSize, height, depth);
        if (!SparseTextureUtils::verifyError(mLog, "TexStorage [GL_TEXTURE_3D wrong width]", gl.getError(),
                                             GL_INVALID_VALUE))
        {
            Texture::Delete(gl, texture);
            return false;
        }

        Texture::Storage(gl, target, 1, format, width, height + max3DTextureSize, depth);
        if (!SparseTextureUtils::verifyError(mLog, "TexStorage [GL_TEXTURE_3D wrong height]", gl.getError(),
                                             GL_INVALID_VALUE))
        {
            Texture::Delete(gl, texture);
            return false;
        }

        // Check for GL_NV_deep_texture3D support, if so we'll need to check
        // against the depth limit instead of the generic 3D texture size limit
        if (m_context.getContextInfo().isExtensionSupported("GL_NV_deep_texture3D"))
        {

            // Ensure that width and height are within the valid bounds for a
            // deep texture
            GLint maxTextureWidthHeight;
            gl.getIntegerv(GL_MAX_DEEP_3D_TEXTURE_DEPTH_NV, &maxTextureWidthHeight);

            if (width < maxTextureWidthHeight && height < maxTextureWidthHeight)
            {
                gl.getIntegerv(GL_MAX_DEEP_3D_TEXTURE_DEPTH_NV, &max3DTextureSize);
            }
        }

        Texture::Storage(gl, target, 1, format, width, height, depth + max3DTextureSize);
        if (!SparseTextureUtils::verifyError(mLog, "TexStorage [GL_TEXTURE_3D wrong depth]", gl.getError(),
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
        if (!SparseTextureUtils::verifyQueryError(mLog, "getIntegerv", target, GL_MAX_SPARSE_TEXTURE_SIZE_ARB,
                                                  gl.getError(), GL_NO_ERROR))
        {
            Texture::Delete(gl, texture);
            return false;
        }

        // Check 3 different cases:
        // 1) wrong width
        // 2) wrong height
        Texture::Storage(gl, target, 1, format, width + maxTextureSize, height, depth);
        if (!SparseTextureUtils::verifyError(mLog, "TexStorage [!GL_TEXTURE_3D wrong width]", gl.getError(),
                                             GL_INVALID_VALUE))
        {
            Texture::Delete(gl, texture);
            return false;
        }

        if (target != GL_TEXTURE_1D_ARRAY)
        {
            Texture::Storage(gl, target, 1, format, width, height + maxTextureSize, depth);
            if (!SparseTextureUtils::verifyError(mLog, "TexStorage [!GL_TEXTURE_3D wrong height]", gl.getError(),
                                                 GL_INVALID_VALUE))
            {
                Texture::Delete(gl, texture);
                return false;
            }
        }

        GLint maxArrayTextureLayers;

        gl.getIntegerv(GL_MAX_SPARSE_ARRAY_TEXTURE_LAYERS_ARB, &maxArrayTextureLayers);
        if (!SparseTextureUtils::verifyQueryError(mLog, "getIntegerv", target, GL_MAX_SPARSE_ARRAY_TEXTURE_LAYERS_ARB,
                                                  gl.getError(), GL_NO_ERROR))
        {
            Texture::Delete(gl, texture);
            return false;
        }

        if (target == GL_TEXTURE_1D_ARRAY || target == GL_TEXTURE_2D_ARRAY || target == GL_TEXTURE_CUBE_MAP_ARRAY)
        {
            Texture::Storage(gl, target, 1, format, width, height, depth + maxArrayTextureLayers);
            if (!SparseTextureUtils::verifyError(mLog, "TexStorage [ARRAY wrong depth]", gl.getError(),
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
            Texture::Storage(gl, target, 1, format, pageSizeX + 1, height, depth);
            if (!SparseTextureUtils::verifyError(mLog, "TexStorage [wrong width]", gl.getError(), GL_INVALID_VALUE))
            {
                Texture::Delete(gl, texture);
                return false;
            }
        }

        if (pageSizeY > 1)
        {
            Texture::Storage(gl, target, 1, format, width, pageSizeY + 1, depth);
            if (!SparseTextureUtils::verifyError(mLog, "TexStorage [wrong height]", gl.getError(), GL_INVALID_VALUE))
            {
                Texture::Delete(gl, texture);
                return false;
            }
        }

        if (pageSizeZ > 1)
        {
            Texture::Storage(gl, target, 1, format, width, height, pageSizeZ + 1);
            if (!SparseTextureUtils::verifyError(mLog, "TexStorage [wrong depth]", gl.getError(), GL_INVALID_VALUE))
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
 *  @param name        Test name
 *  @param description Test description
 */
SparseTextureCommitmentTestCase::SparseTextureCommitmentTestCase(deqp::Context &context, const char *name,
                                                                 const char *description, GLint target, GLint format)
    : TestCase(context, name, description)
    , mTarget(target)
    , mFormat(format)
    , mState()
{
    /* Left blank intentionally */
}

/** Executes test iteration.
 *
 *  @return Returns STOP when test has finished executing, CONTINUE if more iterations are needed.
 */
tcu::TestNode::IterateResult SparseTextureCommitmentTestCase::iterate()
{
    if (!m_context.getContextInfo().isExtensionSupported("GL_ARB_sparse_texture"))
    {
        m_testCtx.setTestResult(QP_TEST_RESULT_NOT_SUPPORTED, "Not Supported");
        return STOP;
    }

    if (caseAllowed(mTarget, mFormat))
    {
        const Functions &gl = m_context.getRenderContext().getFunctions();
        mLog.str("");
        mLog << "Testing sparse texture commitment for target: " << mTarget << ", format: " << mFormat << " - ";

        bool result = true;
        GLuint texture;

        //Checking if written data into not committed region generates no error
        sparseAllocateTexture(gl, mTarget, mFormat, texture, 3);
        for (int l = 0; l < mState.levels; ++l)
            writeDataToTexture(gl, mTarget, mFormat, texture, l);

        //Checking if written data into committed region is as expected
        for (int l = 0; l < mState.levels; ++l)
        {
            if (commitTexturePage(gl, mTarget, mFormat, texture, l))
            {
                writeDataToTexture(gl, mTarget, mFormat, texture, l);
                result = verifyTextureData(gl, mTarget, mFormat, texture, l);
            }

            if (!result)
                break;
        }

        Texture::Delete(gl, texture);

        //verify errors
        result = result && verifyInvalidOperationErrors(gl, mTarget, mFormat, texture);
        result = result && verifyInvalidValueErrors(gl, mTarget, mFormat, texture);

        if (!result)
        {
            m_testCtx.getLog() << tcu::TestLog::Message << mLog.str() << "Fail" << tcu::TestLog::EndMessage;

            m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Fail");
            return STOP;
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
void SparseTextureCommitmentTestCase::texPageCommitment(const glw::Functions &gl, glw::GLint target, glw::GLint format,
                                                        glw::GLuint &texture, GLint level, GLint xOffset, GLint yOffset,
                                                        GLint zOffset, GLint width, GLint height, GLint depth,
                                                        GLboolean commit)
{
    DE_UNREF(format);
    Texture::Bind(gl, texture, target);

    gl.texPageCommitmentARB(target, level, xOffset, yOffset, zOffset, width, height, depth, commit);
}

/** Check if specific combination of target and format is allowed
 *
 * @param target       Target for which texture is binded
 * @param format       Texture internal format
 *
 * @return Returns true if target/format combination is allowed, false otherwise.
 */
bool SparseTextureCommitmentTestCase::caseAllowed(GLint target, GLint format)
{
    DE_UNREF(target);
    DE_UNREF(format);
    return true;
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
bool SparseTextureCommitmentTestCase::prepareTexture(const Functions &gl, GLint target, GLint format, GLuint &texture)
{
    Texture::Generate(gl, texture);
    Texture::Bind(gl, texture, target);

    mState.minDepth = SparseTextureUtils::getTargetDepth(target);
    SparseTextureUtils::getTexturePageSizes(gl, target, format, mState.pageSizeX, mState.pageSizeY, mState.pageSizeZ);

    //The <width> and <height> has to be equal for cube map textures
    if (target == GL_TEXTURE_CUBE_MAP || target == GL_TEXTURE_CUBE_MAP_ARRAY)
    {
        if (mState.pageSizeX > mState.pageSizeY)
            mState.pageSizeY = mState.pageSizeX;
        else if (mState.pageSizeX < mState.pageSizeY)
            mState.pageSizeX = mState.pageSizeY;
    }

    mState.width  = 2 * mState.pageSizeX;
    mState.height = 2 * mState.pageSizeY;
    mState.depth  = 2 * mState.pageSizeZ * mState.minDepth;

    mState.format = glu::mapGLInternalFormat(format);

    return true;
}

/** Allocating sparse texture memory using texStorage* function
 *
 * @param gl           GL API functions
 * @param target       Target for which texture is binded
 * @param format       Texture internal format
 * @param texture      Texture object
 * @param levels       Texture mipmaps level
 *
 * @return Returns true if no error occurred, otherwise throws an exception.
 */
bool SparseTextureCommitmentTestCase::sparseAllocateTexture(const Functions &gl, GLint target, GLint format,
                                                            GLuint &texture, GLint levels)
{
    mLog << "Sparse Allocate [levels: " << levels << "] - ";

    prepareTexture(gl, target, format, texture);

    gl.texParameteri(target, GL_TEXTURE_SPARSE_ARB, GL_TRUE);
    GLU_EXPECT_NO_ERROR(gl.getError(), "texParameteri error occurred for GL_TEXTURE_SPARSE_ARB");

    // GL_TEXTURE_RECTANGLE can have only one level
    mState.levels = target == GL_TEXTURE_RECTANGLE ? 1 : levels;

    Texture::Storage(gl, target, mState.levels, format, mState.width, mState.height, mState.depth);
    GLU_EXPECT_NO_ERROR(gl.getError(), "TexStorage");

    return true;
}

/** Allocating texture memory using texStorage* function
 *
 * @param gl           GL API functions
 * @param target       Target for which texture is binded
 * @param format       Texture internal format
 * @param texture      Texture object
 * @param levels       Texture mipmaps level
 *
 * @return Returns true if no error occurred, otherwise throws an exception.
 */
bool SparseTextureCommitmentTestCase::allocateTexture(const Functions &gl, GLint target, GLint format, GLuint &texture,
                                                      GLint levels)
{
    mLog << "Allocate [levels: " << levels << "] - ";

    prepareTexture(gl, target, format, texture);

    //GL_TEXTURE_RECTANGLE can have only one level
    if (target != GL_TEXTURE_RECTANGLE)
        mState.levels = levels;
    else
        mState.levels = 1;

    Texture::Storage(gl, target, mState.levels, format, mState.width, mState.height, mState.depth);
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
bool SparseTextureCommitmentTestCase::writeDataToTexture(const Functions &gl, GLint target, GLint format,
                                                         GLuint &texture, GLint level)
{
    DE_UNREF(format);
    DE_UNREF(texture);

    mLog << "Fill texture [level: " << level << "] - ";

    if (level > mState.levels - 1)
        TCU_FAIL("Invalid level");

    TransferFormat transferFormat = glu::getTransferFormat(mState.format);

    GLint width;
    GLint height;
    GLint depth;
    SparseTextureUtils::getTextureLevelSize(target, mState, level, width, height, depth);

    if (width > 0 && height > 0 && depth >= mState.minDepth)
    {
        GLint texSize = width * height * depth * mState.format.getPixelSize();

        std::vector<GLubyte> vecData;
        vecData.resize(texSize);
        GLubyte *data = vecData.data();

        deMemset(data, 16 + 16 * level, texSize);

        Texture::SubImage(gl, target, level, 0, 0, 0, width, height, depth, transferFormat.format,
                          transferFormat.dataType, (GLvoid *)data);
        GLU_EXPECT_NO_ERROR(gl.getError(), "SubImage");
    }

    return true;
}

/** Verify if data stored in texture is as expected
 *
 * @param gl           GL API functions
 * @param target       Target for which texture is binded
 * @param format       Texture internal format
 * @param texture      Texture object
 * @param level        Texture mipmap level
 *
 * @return Returns true if data is as expected, false if not, throws an exception if error occurred.
 */
bool SparseTextureCommitmentTestCase::verifyTextureData(const Functions &gl, GLint target, GLint format,
                                                        GLuint &texture, GLint level)
{
    DE_UNREF(format);
    DE_UNREF(texture);

    mLog << "Verify Texture [level: " << level << "] - ";

    if (level > mState.levels - 1)
        TCU_FAIL("Invalid level");

    TransferFormat transferFormat = glu::getTransferFormat(mState.format);

    GLint width;
    GLint height;
    GLint depth;
    SparseTextureUtils::getTextureLevelSize(target, mState, level, width, height, depth);

    //Committed region is limited to 1/2 of width
    GLint widthCommitted = width / 2;

    if (widthCommitted == 0 || height == 0 || depth < mState.minDepth)
        return true;

    bool result = true;

    if (target != GL_TEXTURE_CUBE_MAP)
    {
        GLint texSize = width * height * depth * mState.format.getPixelSize();

        std::vector<GLubyte> vecExpData;
        std::vector<GLubyte> vecOutData;
        vecExpData.resize(texSize);
        vecOutData.resize(texSize);
        GLubyte *exp_data = vecExpData.data();
        GLubyte *out_data = vecOutData.data();

        deMemset(exp_data, 16 + 16 * level, texSize);
        deMemset(out_data, 255, texSize);

        Texture::GetData(gl, level, target, transferFormat.format, transferFormat.dataType, (GLvoid *)out_data);
        GLU_EXPECT_NO_ERROR(gl.getError(), "Texture::GetData");

        //Verify only committed region
        for (GLint x = 0; x < widthCommitted; ++x)
            for (GLint y = 0; y < height; ++y)
                for (GLint z = 0; z < depth; ++z)
                {
                    int pixelSize          = mState.format.getPixelSize();
                    GLubyte *dataRegion    = exp_data + ((x + y * width) * pixelSize);
                    GLubyte *outDataRegion = out_data + ((x + y * width) * pixelSize);
                    if (deMemCmp(dataRegion, outDataRegion, pixelSize) != 0)
                        result = false;
                }
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

        GLint texSize = width * height * mState.format.getPixelSize();

        std::vector<GLubyte> vecExpData;
        std::vector<GLubyte> vecOutData;
        vecExpData.resize(texSize);
        vecOutData.resize(texSize);
        GLubyte *exp_data = vecExpData.data();
        GLubyte *out_data = vecOutData.data();

        deMemset(exp_data, 16 + 16 * level, texSize);
        deMemset(out_data, 255, texSize);

        for (size_t i = 0; i < subTargets.size(); ++i)
        {
            GLint subTarget = subTargets[i];

            mLog << "Verify Subtarget [subtarget: " << subTarget << "] - ";

            deMemset(out_data, 255, texSize);

            Texture::GetData(gl, level, subTarget, transferFormat.format, transferFormat.dataType, (GLvoid *)out_data);
            GLU_EXPECT_NO_ERROR(gl.getError(), "Texture::GetData");

            //Verify only committed region
            for (GLint x = 0; x < widthCommitted; ++x)
                for (GLint y = 0; y < height; ++y)
                    for (GLint z = 0; z < depth; ++z)
                    {
                        int pixelSize          = mState.format.getPixelSize();
                        GLubyte *dataRegion    = exp_data + ((x + y * width) * pixelSize);
                        GLubyte *outDataRegion = out_data + ((x + y * width) * pixelSize);
                        if (deMemCmp(dataRegion, outDataRegion, pixelSize) != 0)
                            result = false;
                    }

            if (!result)
                break;
        }
    }

    return result;
}

/** Commit texture page using texPageCommitment function
 *
 * @param gl           GL API functions
 * @param target       Target for which texture is binded
 * @param format       Texture internal format
 * @param texture      Texture object
 * @param level        Texture mipmap level
 *
 * @return Returns true if commitment is done properly, false if commitment is not allowed or throws exception if error occurred.
 */
bool SparseTextureCommitmentTestCase::commitTexturePage(const Functions &gl, GLint target, GLint format,
                                                        GLuint &texture, GLint level)
{
    mLog << "Commit Region [level: " << level << "] - ";

    if (level > mState.levels - 1)
        TCU_FAIL("Invalid level");

    // Avoid not allowed commitments
    if (!isInPageSizesRange(target, level) || !isPageSizesMultiplication(target, level))
    {
        mLog << "Skip commitment [level: " << level << "] - ";
        return false;
    }

    GLint width;
    GLint height;
    GLint depth;
    SparseTextureUtils::getTextureLevelSize(target, mState, level, width, height, depth);

    if (target == GL_TEXTURE_CUBE_MAP)
        depth = 6 * depth;

    GLint widthCommitted = width / 2;

    Texture::Bind(gl, texture, target);
    texPageCommitment(gl, target, format, texture, level, 0, 0, 0, widthCommitted, height, depth, GL_TRUE);
    GLU_EXPECT_NO_ERROR(gl.getError(), "texPageCommitment");

    return true;
}

/** Check if current texture size for level is greater or equal page size in a corresponding direction
 *
 * @param target  Target for which texture is binded
 * @param level   Texture mipmap level
 *
 * @return Returns true if the texture size condition is fulfilled, false otherwise.
 */
bool SparseTextureCommitmentTestCase::isInPageSizesRange(GLint target, GLint level)
{
    GLint width;
    GLint height;
    GLint depth;
    SparseTextureUtils::getTextureLevelSize(target, mState, level, width, height, depth);

    if (target == GL_TEXTURE_CUBE_MAP)
        depth = 6 * depth;

    GLint widthCommitted = width / 2;
    if (widthCommitted >= mState.pageSizeX && height >= mState.pageSizeY &&
        (mState.minDepth == 0 || depth >= mState.pageSizeZ))
    {
        return true;
    }

    return false;
}

/** Check if current texture size for level is page size multiplication in a corresponding direction
 *
 * @param target  Target for which texture is binded
 * @param level   Texture mipmap level
 *
 * @return Returns true if the texture size condition is fulfilled, false otherwise.
 */
bool SparseTextureCommitmentTestCase::isPageSizesMultiplication(GLint target, GLint level)
{
    GLint width;
    GLint height;
    GLint depth;
    SparseTextureUtils::getTextureLevelSize(target, mState, level, width, height, depth);

    if (target == GL_TEXTURE_CUBE_MAP)
        depth = 6 * depth;

    GLint widthCommitted = width / 2;
    if ((widthCommitted % mState.pageSizeX) == 0 && (height % mState.pageSizeY) == 0 && (depth % mState.pageSizeZ) == 0)
    {
        return true;
    }

    return false;
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
bool SparseTextureCommitmentTestCase::verifyInvalidOperationErrors(const Functions &gl, GLint target, GLint format,
                                                                   GLuint &texture)
{
    mLog << "Verify INVALID_OPERATION Errors - ";

    bool result = true;

    // Case 1 - texture is not GL_TEXTURE_IMMUTABLE_FORMAT
    Texture::Generate(gl, texture);
    Texture::Bind(gl, texture, target);

    gl.texParameteri(target, GL_TEXTURE_SPARSE_ARB, GL_TRUE);
    GLU_EXPECT_NO_ERROR(gl.getError(), "texParameteri error occurred for GL_TEXTURE_SPARSE_ARB");

    GLint immutableFormat;

    gl.getTexParameteriv(target, GL_TEXTURE_IMMUTABLE_FORMAT, &immutableFormat);
    GLU_EXPECT_NO_ERROR(gl.getError(), "getTexParameteriv error occurred for GL_TEXTURE_IMMUTABLE_FORMAT");

    if (immutableFormat == GL_FALSE)
    {
        texPageCommitment(gl, target, format, texture, 0, 0, 0, 0, mState.pageSizeX, mState.pageSizeY, mState.pageSizeZ,
                          GL_TRUE);
        result = SparseTextureUtils::verifyError(mLog, "texPageCommitment [GL_TEXTURE_IMMUTABLE_FORMAT texture]",
                                                 gl.getError(), GL_INVALID_OPERATION);
        if (!result)
            goto verifing_invalid_operation_end;
    }

    Texture::Delete(gl, texture);

    // Case 2 - texture is not TEXTURE_SPARSE_ARB
    allocateTexture(gl, target, format, texture, 1);

    texPageCommitment(gl, target, format, texture, 0, 0, 0, 0, mState.pageSizeX, mState.pageSizeY, mState.pageSizeZ,
                      GL_TRUE);
    result = SparseTextureUtils::verifyError(mLog, "texPageCommitment [not TEXTURE_SPARSE_ARB texture]", gl.getError(),
                                             GL_INVALID_OPERATION);
    if (!result)
        goto verifing_invalid_operation_end;

    // Sparse allocate texture
    Texture::Delete(gl, texture);
    sparseAllocateTexture(gl, target, format, texture, 1);

    // Case 3 - commitment sizes greater than expected
    texPageCommitment(gl, target, format, texture, 0, 0, 0, 0, mState.width + mState.pageSizeX, mState.height,
                      mState.depth, GL_TRUE);
    result = SparseTextureUtils::verifyError(mLog, "texPageCommitment [commitment width greater than expected]",
                                             gl.getError(), GL_INVALID_OPERATION);
    if (!result)
        goto verifing_invalid_operation_end;

    texPageCommitment(gl, target, format, texture, 0, 0, 0, 0, mState.width, mState.height + mState.pageSizeY,
                      mState.depth, GL_TRUE);
    result = SparseTextureUtils::verifyError(mLog, "texPageCommitment [commitment height greater than expected]",
                                             gl.getError(), GL_INVALID_OPERATION);
    if (!result)
        goto verifing_invalid_operation_end;

    if (target == GL_TEXTURE_3D || target == GL_TEXTURE_2D_ARRAY || target == GL_TEXTURE_CUBE_MAP_ARRAY)
    {
        texPageCommitment(gl, target, format, texture, 0, 0, 0, 0, mState.width, mState.height,
                          mState.depth + mState.pageSizeZ, GL_TRUE);
        result = SparseTextureUtils::verifyError(mLog, "texPageCommitment [commitment depth greater than expected]",
                                                 gl.getError(), GL_INVALID_OPERATION);
        if (!result)
            goto verifing_invalid_operation_end;
    }

    // Case 4 - commitment sizes not multiple of corresponding page sizes
    if (mState.pageSizeX > 1)
    {
        texPageCommitment(gl, target, format, texture, 0, 0, 0, 0, 1, mState.pageSizeY, mState.pageSizeZ, GL_TRUE);
        result =
            SparseTextureUtils::verifyError(mLog, "texPageCommitment [commitment width not multiple of page sizes X]",
                                            gl.getError(), GL_INVALID_OPERATION);
        if (!result)
            goto verifing_invalid_operation_end;
    }

    if (mState.pageSizeY > 1)
    {
        texPageCommitment(gl, target, format, texture, 0, 0, 0, 0, mState.pageSizeX, 1, mState.pageSizeZ, GL_TRUE);
        result =
            SparseTextureUtils::verifyError(mLog, "texPageCommitment [commitment height not multiple of page sizes Y]",
                                            gl.getError(), GL_INVALID_OPERATION);
        if (!result)
            goto verifing_invalid_operation_end;
    }

    if (mState.pageSizeZ > 1)
    {
        if (target == GL_TEXTURE_3D || target == GL_TEXTURE_2D_ARRAY || target == GL_TEXTURE_CUBE_MAP_ARRAY)
        {
            texPageCommitment(gl, target, format, texture, 0, 0, 0, 0, mState.pageSizeX, mState.pageSizeY,
                              mState.minDepth, GL_TRUE);
            result = SparseTextureUtils::verifyError(
                mLog, "texPageCommitment [commitment depth not multiple of page sizes Z]", gl.getError(),
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
bool SparseTextureCommitmentTestCase::verifyInvalidValueErrors(const Functions &gl, GLint target, GLint format,
                                                               GLuint &texture)
{
    mLog << "Verify INVALID_VALUE Errors - ";

    bool result = true;

    sparseAllocateTexture(gl, target, format, texture, 1);

    // Case 1 - commitment offset not multiple of page size in corresponding dimension
    if (mState.pageSizeX > 1)
    {
        texPageCommitment(gl, target, format, texture, 0, 1, 0, 0, mState.pageSizeX, mState.pageSizeY, mState.pageSizeZ,
                          GL_TRUE);
        result =
            SparseTextureUtils::verifyError(mLog, "texPageCommitment [commitment offsetX not multiple of page size X]",
                                            gl.getError(), GL_INVALID_VALUE);
        if (!result)
            goto verifing_invalid_value_end;
    }
    if (mState.pageSizeY > 1)
    {
        texPageCommitment(gl, target, format, texture, 0, 0, 1, 0, mState.pageSizeX, mState.pageSizeY, mState.pageSizeZ,
                          GL_TRUE);
        result =
            SparseTextureUtils::verifyError(mLog, "texPageCommitment [commitment offsetY not multiple of page size Y]",
                                            gl.getError(), GL_INVALID_VALUE);
        if (!result)
            goto verifing_invalid_value_end;
    }
    if ((target == GL_TEXTURE_3D || target == GL_TEXTURE_2D_ARRAY || target == GL_TEXTURE_CUBE_MAP_ARRAY) &&
        (mState.minDepth % mState.pageSizeZ))
    {
        texPageCommitment(gl, target, format, texture, 0, 0, 0, mState.minDepth, mState.pageSizeX, mState.pageSizeY,
                          mState.pageSizeZ, GL_TRUE);
        result =
            SparseTextureUtils::verifyError(mLog, "texPageCommitment [commitment offsetZ not multiple of page size Z]",
                                            gl.getError(), GL_INVALID_VALUE);
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
SparseDSATextureCommitmentTestCase::SparseDSATextureCommitmentTestCase(deqp::Context &context, const char *name,
                                                                       const char *description, GLint target,
                                                                       GLint format)
    : SparseTextureCommitmentTestCase(context, name, description, target, format)
{
    /* Left blank intentionally */
}

/** Executes test iteration.
 *
 *  @return Returns STOP when test has finished executing, CONTINUE if more iterations are needed.
 */
tcu::TestNode::IterateResult SparseDSATextureCommitmentTestCase::iterate()
{
    if (!m_context.getContextInfo().isExtensionSupported("GL_ARB_sparse_texture"))
    {
        m_testCtx.setTestResult(QP_TEST_RESULT_NOT_SUPPORTED, "Not Supported");
        return STOP;
    }

    if (!m_context.getContextInfo().isExtensionSupported("GL_EXT_direct_state_access"))
    {
        m_testCtx.setTestResult(QP_TEST_RESULT_NOT_SUPPORTED, "GL_EXT_direct_state_access extension is not supported.");
        return STOP;
    }

    const Functions &gl = m_context.getRenderContext().getFunctions();

    bool result = true;
    GLuint texture;

    mLog.str("");
    mLog << "Testing DSA sparse texture commitment for target: " << mTarget << ", format: " << mFormat << " - ";

    //Checking if written data into committed region is as expected
    sparseAllocateTexture(gl, mTarget, mFormat, texture, 3);
    for (int l = 0; l < mState.levels; ++l)
    {
        if (commitTexturePage(gl, mTarget, mFormat, texture, l))
        {
            writeDataToTexture(gl, mTarget, mFormat, texture, l);
            result = verifyTextureData(gl, mTarget, mFormat, texture, l);
        }

        if (!result)
            break;
    }

    Texture::Delete(gl, texture);

    //verify errors
    result = result && verifyInvalidOperationErrors(gl, mTarget, mFormat, texture);
    result = result && verifyInvalidValueErrors(gl, mTarget, mFormat, texture);

    if (!result)
    {
        m_testCtx.getLog() << tcu::TestLog::Message << mLog.str() << "Fail" << tcu::TestLog::EndMessage;
        m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Fail");
        return STOP;
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
void SparseDSATextureCommitmentTestCase::texPageCommitment(const glw::Functions &gl, glw::GLint target,
                                                           glw::GLint format, glw::GLuint &texture, GLint level,
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
SparseTextureTests::SparseTextureTests(deqp::Context &context)
    : TestCaseGroup(context, "sparse_texture_tests", "Verify conformance of CTS_ARB_sparse_texture implementation")
{
}

/** Initializes the test group contents. */
void SparseTextureTests::init()
{
    addChild(new SimpleQueriesTestCase(m_context));

    addTextureParameterQueriesTestCase();
    addInternalFormatQueriesTestCase();
    addSparseTextureAllocationTestCase();
    addSparseTextureCommitmentTestCase();
    addSparseDSATextureCommitmentTestCase();
}

void SparseTextureTests::addTextureParameterQueriesTestCase()
{
    std::vector<GLint> supportedTargets;
    supportedTargets.push_back(GL_TEXTURE_2D);
    supportedTargets.push_back(GL_TEXTURE_2D_ARRAY);
    supportedTargets.push_back(GL_TEXTURE_CUBE_MAP);
    supportedTargets.push_back(GL_TEXTURE_CUBE_MAP_ARRAY);
    supportedTargets.push_back(GL_TEXTURE_3D);
    supportedTargets.push_back(GL_TEXTURE_RECTANGLE);

    std::vector<GLint> notSupportedTargets;
    notSupportedTargets.push_back(GL_TEXTURE_2D_MULTISAMPLE);
    notSupportedTargets.push_back(GL_TEXTURE_2D_MULTISAMPLE_ARRAY);

    const char *description =
        "Implements all glTexParameter* and glGetTexParameter* queries tests described in CTS_ARB_sparse_texture";

    for (std::vector<GLint>::iterator targetIter = supportedTargets.begin(); targetIter != supportedTargets.end();
         ++targetIter)
    {
        std::string name =
            std::string("TextureParameterQueries") + "_" + SparseTextureUtils::getTextureTargetString(*targetIter);
        addChild(
            new TextureParameterQueriesTestCase(m_context, name.c_str(), description, *targetIter, GL_INVALID_VALUE));
    }
    for (std::vector<GLint>::iterator targetIter = notSupportedTargets.begin(); targetIter != notSupportedTargets.end();
         ++targetIter)
    {
        std::string name =
            std::string("TextureParameterQueries") + "_" + SparseTextureUtils::getTextureTargetString(*targetIter);
        addChild(
            new TextureParameterQueriesTestCase(m_context, name.c_str(), description, GL_INVALID_VALUE, *targetIter));
    }
}

void SparseTextureTests::addInternalFormatQueriesTestCase()
{
    std::vector<GLint> supportedTargets;
    supportedTargets.push_back(GL_TEXTURE_2D);
    supportedTargets.push_back(GL_TEXTURE_2D_ARRAY);
    supportedTargets.push_back(GL_TEXTURE_3D);
    supportedTargets.push_back(GL_TEXTURE_CUBE_MAP);
    supportedTargets.push_back(GL_TEXTURE_CUBE_MAP_ARRAY);
    supportedTargets.push_back(GL_TEXTURE_RECTANGLE);

    std::vector<GLint> supportedInternalFormats;
    supportedInternalFormats.push_back(GL_R8);
    supportedInternalFormats.push_back(GL_R8_SNORM);
    supportedInternalFormats.push_back(GL_R16);
    supportedInternalFormats.push_back(GL_R16_SNORM);
    supportedInternalFormats.push_back(GL_RG8);
    supportedInternalFormats.push_back(GL_RG8_SNORM);
    supportedInternalFormats.push_back(GL_RG16);
    supportedInternalFormats.push_back(GL_RG16_SNORM);
    supportedInternalFormats.push_back(GL_RGB565);
    supportedInternalFormats.push_back(GL_RGBA8);
    supportedInternalFormats.push_back(GL_RGBA8_SNORM);
    supportedInternalFormats.push_back(GL_RGB10_A2);
    supportedInternalFormats.push_back(GL_RGB10_A2UI);
    supportedInternalFormats.push_back(GL_RGBA16);
    supportedInternalFormats.push_back(GL_RGBA16_SNORM);
    supportedInternalFormats.push_back(GL_R16F);
    supportedInternalFormats.push_back(GL_RG16F);
    supportedInternalFormats.push_back(GL_RGBA16F);
    supportedInternalFormats.push_back(GL_R32F);
    supportedInternalFormats.push_back(GL_RG32F);
    supportedInternalFormats.push_back(GL_RGBA32F);
    supportedInternalFormats.push_back(GL_R11F_G11F_B10F);
    supportedInternalFormats.push_back(GL_RGB9_E5);
    supportedInternalFormats.push_back(GL_R8I);
    supportedInternalFormats.push_back(GL_R8UI);
    supportedInternalFormats.push_back(GL_R16I);
    supportedInternalFormats.push_back(GL_R16UI);
    supportedInternalFormats.push_back(GL_R32I);
    supportedInternalFormats.push_back(GL_R32UI);
    supportedInternalFormats.push_back(GL_RG8I);
    supportedInternalFormats.push_back(GL_RG8UI);
    supportedInternalFormats.push_back(GL_RG16I);
    supportedInternalFormats.push_back(GL_RG16UI);
    supportedInternalFormats.push_back(GL_RG32I);
    supportedInternalFormats.push_back(GL_RG32UI);
    supportedInternalFormats.push_back(GL_RGBA8I);
    supportedInternalFormats.push_back(GL_RGBA8UI);
    supportedInternalFormats.push_back(GL_RGBA16I);
    supportedInternalFormats.push_back(GL_RGBA16UI);
    supportedInternalFormats.push_back(GL_RGBA32I);

    const char *description = "Implements GetInternalformat query tests described in CTS_ARB_sparse_texture";

    for (std::vector<GLint>::iterator formIter = supportedInternalFormats.begin();
         formIter != supportedInternalFormats.end(); ++formIter)
    {
        for (std::vector<GLint>::iterator targetIter = supportedTargets.begin(); targetIter != supportedTargets.end();
             ++targetIter)
        {
            std::string name = std::string("InternalFormatQueries") + "_" +
                               SparseTextureUtils::getTextureTargetString(*targetIter) + "_" +
                               SparseTextureUtils::getTextureFormatString(*formIter);
            addChild(new InternalFormatQueriesTestCase(m_context, name.c_str(), description, *targetIter, *formIter));
        }
    }
}

void SparseTextureTests::addSparseTextureAllocationTestCase()
{
    std::vector<GLint> supportedTargets;
    supportedTargets.push_back(GL_TEXTURE_2D);
    supportedTargets.push_back(GL_TEXTURE_2D_ARRAY);
    supportedTargets.push_back(GL_TEXTURE_CUBE_MAP);
    supportedTargets.push_back(GL_TEXTURE_CUBE_MAP_ARRAY);
    supportedTargets.push_back(GL_TEXTURE_3D);
    supportedTargets.push_back(GL_TEXTURE_RECTANGLE);

    std::vector<GLint> fullArrayTargets;
    fullArrayTargets.push_back(GL_TEXTURE_2D_ARRAY);
    fullArrayTargets.push_back(GL_TEXTURE_CUBE_MAP);
    fullArrayTargets.push_back(GL_TEXTURE_CUBE_MAP_ARRAY);

    std::vector<GLint> supportedInternalFormats;
    supportedInternalFormats.push_back(GL_R8);
    supportedInternalFormats.push_back(GL_R8_SNORM);
    supportedInternalFormats.push_back(GL_R16);
    supportedInternalFormats.push_back(GL_R16_SNORM);
    supportedInternalFormats.push_back(GL_RG8);
    supportedInternalFormats.push_back(GL_RG8_SNORM);
    supportedInternalFormats.push_back(GL_RG16);
    supportedInternalFormats.push_back(GL_RG16_SNORM);
    supportedInternalFormats.push_back(GL_RGB565);
    supportedInternalFormats.push_back(GL_RGBA8);
    supportedInternalFormats.push_back(GL_RGBA8_SNORM);
    supportedInternalFormats.push_back(GL_RGB10_A2);
    supportedInternalFormats.push_back(GL_RGB10_A2UI);
    supportedInternalFormats.push_back(GL_RGBA16);
    supportedInternalFormats.push_back(GL_RGBA16_SNORM);
    supportedInternalFormats.push_back(GL_R16F);
    supportedInternalFormats.push_back(GL_RG16F);
    supportedInternalFormats.push_back(GL_RGBA16F);
    supportedInternalFormats.push_back(GL_R32F);
    supportedInternalFormats.push_back(GL_RG32F);
    supportedInternalFormats.push_back(GL_RGBA32F);
    supportedInternalFormats.push_back(GL_R11F_G11F_B10F);
    supportedInternalFormats.push_back(GL_RGB9_E5);
    supportedInternalFormats.push_back(GL_R8I);
    supportedInternalFormats.push_back(GL_R8UI);
    supportedInternalFormats.push_back(GL_R16I);
    supportedInternalFormats.push_back(GL_R16UI);
    supportedInternalFormats.push_back(GL_R32I);
    supportedInternalFormats.push_back(GL_R32UI);
    supportedInternalFormats.push_back(GL_RG8I);
    supportedInternalFormats.push_back(GL_RG8UI);
    supportedInternalFormats.push_back(GL_RG16I);
    supportedInternalFormats.push_back(GL_RG16UI);
    supportedInternalFormats.push_back(GL_RG32I);
    supportedInternalFormats.push_back(GL_RG32UI);
    supportedInternalFormats.push_back(GL_RGBA8I);
    supportedInternalFormats.push_back(GL_RGBA8UI);
    supportedInternalFormats.push_back(GL_RGBA16I);
    supportedInternalFormats.push_back(GL_RGBA16UI);
    supportedInternalFormats.push_back(GL_RGBA32I);

    const char *description = "Verifies TexStorage* functionality added in CTS_ARB_sparse_texture";

    for (std::vector<GLint>::iterator formIter = supportedInternalFormats.begin();
         formIter != supportedInternalFormats.end(); ++formIter)
    {
        for (std::vector<GLint>::iterator targetIter = supportedTargets.begin(); targetIter != supportedTargets.end();
             ++targetIter)
        {
            std::string name = std::string("SparseTextureAllocation") + "_" +
                               SparseTextureUtils::getTextureTargetString(*targetIter) + "_" +
                               SparseTextureUtils::getTextureFormatString(*formIter);
            addChild(new SparseTextureAllocationTestCase(m_context, name.c_str(), description, *targetIter,
                                                         GL_INVALID_VALUE, *formIter));
        }
        for (std::vector<GLint>::iterator targetIter = fullArrayTargets.begin(); targetIter != fullArrayTargets.end();
             ++targetIter)
        {
            std::string name = std::string("SparseTextureAllocation") + "_fullArray_" +
                               SparseTextureUtils::getTextureTargetString(*targetIter) + "_" +
                               SparseTextureUtils::getTextureFormatString(*formIter);
            addChild(new SparseTextureAllocationTestCase(m_context, name.c_str(), description, GL_INVALID_VALUE,
                                                         *targetIter, *formIter));
        }
    }
}

void SparseTextureTests::addSparseTextureCommitmentTestCase()
{
    const char *description = "Verifies TexPageCommitmentARB functionality added in CTS_ARB_sparse_texture";

    for (size_t target = 0; target < SparseTextureCommitmentTargets.size(); target++)
    {
        for (size_t format = 0; format < SparseTextureCommitmentFormats.size(); format++)
        {
            std::string name = std::string("SparseTextureCommitment") + "_" +
                               SparseTextureUtils::getTextureTargetString(SparseTextureCommitmentTargets[target]) +
                               "_" + SparseTextureUtils::getTextureFormatString(SparseTextureCommitmentFormats[format]);
            addChild(new SparseTextureCommitmentTestCase(m_context, name.c_str(), description,
                                                         SparseTextureCommitmentTargets[target],
                                                         SparseTextureCommitmentFormats[format]));
        }
    }
}

void SparseTextureTests::addSparseDSATextureCommitmentTestCase()
{
    const char *description = "Verifies texturePageCommitmentEXT functionality added in CTS_ARB_sparse_texture";

    for (size_t target = 0; target < SparseTextureCommitmentTargets.size(); target++)
    {
        for (size_t format = 0; format < SparseTextureCommitmentFormats.size(); format++)
        {
            std::string name = std::string("SparseDSATextureCommitment") + "_" +
                               SparseTextureUtils::getTextureTargetString(SparseTextureCommitmentTargets[target]) +
                               "_" + SparseTextureUtils::getTextureFormatString(SparseTextureCommitmentFormats[format]);
            addChild(new SparseDSATextureCommitmentTestCase(m_context, name.c_str(), description,
                                                            SparseTextureCommitmentTargets[target],
                                                            SparseTextureCommitmentFormats[format]));
        }
    }
}

} // namespace gl4cts
