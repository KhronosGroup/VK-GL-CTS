#ifndef _GL4CCLEARTEXIMAGEANDSUBIMAGETESTS_HPP
#define _GL4CCLEARTEXIMAGEANDSUBIMAGETESTS_HPP

/*-------------------------------------------------------------------------
 * OpenGL Conformance Test Suite
 * -----------------------------
 *
 * Copyright (c) 2025 The Khronos Group Inc.
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
 * \file  gl4cClearTexImageAndSubImageTests.hpp
 * \brief Clear Texture Image Tests Suite Interface
 */ /*-------------------------------------------------------------------*/

#include "glcTestCase.hpp"
#include "tcuPixelFormat.hpp"
#include "tcuTexture.hpp"

#include "gluTextureUtil.hpp"

#include <functional>

namespace gl4cts
{

class ClearTexAndSubImageTest : public deqp::TestCase
{
public:
    struct TestOptions
    {
        bool clearTexImage;
        bool clearSubTexImage;
        bool dimensionZero;
        bool clearWithRed;
        bool clearWithRg;
        bool clearWithRgba;
        bool clearWithRgb;
    };
    ClearTexAndSubImageTest(deqp::Context &context, const char *test_name, const char *test_description,
                            glw::GLenum format, glw::GLenum internalFormat, glw::GLenum type, size_t pixelSize,
                            glw::GLint texLevel, TestOptions testOptions);
    ~ClearTexAndSubImageTest() = default;

    tcu::TestNode::IterateResult iterate(void);
    template <typename type>
    bool test();

private:
    void createTexture();
    void deleteTexture();
    template <typename type>
    void fillTexture();
    template <typename type>
    void clearTexture();
    template <typename type>
    void clearImageTexture();
    template <typename type>
    void clearSubImageTexture();

    template <typename type>
    bool verifyResults();
    template <typename type>
    bool verifyClearImageResults();
    template <typename type>
    bool verifyClearSubImageResults();

    glw::GLuint m_texture;
    glw::GLuint m_width;
    glw::GLuint m_height;
    glw::GLenum m_format;
    glw::GLenum m_internalFormat;
    glw::GLuint m_pixelSize;
    glw::GLenum m_type;
    glw::GLint m_texLevel;

    TestOptions m_testOptions;
};

class ClearTexAndSubImageNegativeTest : public deqp::TestCase
{
public:
    ClearTexAndSubImageNegativeTest(deqp::Context &context, const char *test_name, const char *test_description,
                                    std::function<bool(deqp::Context &)> &testFunc);
    ~ClearTexAndSubImageNegativeTest() = default;

    tcu::TestNode::IterateResult iterate(void);

private:
    std::function<bool(deqp::Context &)> &m_testFunc;
};

class ClearTextureImageTestCases : public deqp::TestCaseGroup
{
public:
    /* Public methods */
    ClearTextureImageTestCases(deqp::Context &context);
    ~ClearTextureImageTestCases() = default;

    void init(void);

private:
    /* Private methods */
    ClearTextureImageTestCases(const ClearTextureImageTestCases &other);
    ClearTextureImageTestCases &operator=(const ClearTextureImageTestCases &other);
};
} /* namespace gl4cts */

#endif // _GL4CCLEARTEXIMAGEANDSUBIMAGETESTS_HPP
