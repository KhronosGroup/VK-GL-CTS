#ifndef _GL4CTEXTUREBUFFERTESTS_HPP
#define _GL4CTEXTUREBUFFERTESTS_HPP

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
 * \file  gl4cTextureBufferTests.hpp
 * \brief Texture buffer Tests Suite Interface
 */ /*-------------------------------------------------------------------*/

#include "glcTestCase.hpp"

namespace gl4cts
{

class TextureBufferTestBase : public deqp::TestCase
{
public:
    TextureBufferTestBase(deqp::Context &context, const char *test_name, const char *test_description);
    virtual ~TextureBufferTestBase();

    tcu::TestNode::IterateResult iterate(void);
    virtual void test();
    virtual void clean();

protected:
    glw::GLuint m_tbo;
    glw::GLuint m_tboTexture;
    glw::GLuint m_bufferRange;
};

class TexBufferTest : public TextureBufferTestBase
{
public:
    TexBufferTest(deqp::Context &context, const char *test_name, const char *test_description);
    ~TexBufferTest() = default;

    void test() override;
};

class TextureBufferTest : public TextureBufferTestBase
{
public:
    TextureBufferTest(deqp::Context &context, const char *test_name, const char *test_description);
    ~TextureBufferTest() = default;

    void test() override;
};

class TexBufferRangeTest : public TextureBufferTestBase
{
public:
    TexBufferRangeTest(deqp::Context &context, const char *test_name, const char *test_description);
    ~TexBufferRangeTest() = default;

    void test() override;
};

class TextureBufferRangeTest : public TextureBufferTestBase
{
public:
    TextureBufferRangeTest(deqp::Context &context, const char *test_name, const char *test_description);
    ~TextureBufferRangeTest() = default;

    void test() override;
};

class TextureBufferTests : public deqp::TestCaseGroup
{
public:
    TextureBufferTests(deqp::Context &context);
    ~TextureBufferTests() = default;

    void init(void);

private:
    TextureBufferTests(const TextureBufferTests &other);
    TextureBufferTests &operator=(const TextureBufferTests &other);
};

} /* namespace gl4cts */

#endif // _GL4CTEXTUREBUFFERTESTS_HPP
