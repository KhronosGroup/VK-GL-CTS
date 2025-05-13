#ifndef _GLCTEXTURESTORAGETESTS_HPP
#define _GLCTEXTURESTORAGETESTS_HPP
/*-------------------------------------------------------------------------
 * OpenGL Conformance Test Suite
 * -----------------------------
 *
 * Copyright (c) 2024 The Khronos Group Inc.
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
 * \file  glcTextureStorageTests.hpp
 * \brief Conformance tests for textureStorage operations.
 */ /*-------------------------------------------------------------------*/

#include "glcTestCase.hpp"
#include "glwDefs.hpp"
#include "tcuDefs.hpp"

namespace glcts
{

/*
2.3 Verify that compressed texture data can be loaded into a new
    (i.e., recently created) texture, or updated in an existing texture

In a manner similar to test 2.2, verify that compressed texture
    data (e.g., RGTC and BPTC for OpenGL; and EAC/ETC2 for OpenGL ES)
    for each API can be loaded into a texture created by TexStorage.
*/
class TextureStorageCompressedDataTestCase : public deqp::TestCase
{
public:
    /* Public methods */
    TextureStorageCompressedDataTestCase(deqp::Context &context);

    void deinit();
    void init();
    tcu::TestNode::IterateResult iterate();

protected:
    bool iterate_gl();
    bool iterate_gles();

private:
    /* Private methods */

    bool m_isContextES;
    bool m_testSupported;

    glw::GLuint m_texture2D, m_textureCubeMap, m_texture3D, m_texture2DArray;
    const int m_textureSize2D = 512;
    const int m_textureSize3D = 64;
    int m_maxTexturePixels;
    int m_textureLevels2D;
    int m_textureLevels3D;

    std::vector<glw::GLfloat> m_texData;
};

/** Test group which encapsulates all conformance tests */
class TextureStorageTests : public deqp::TestCaseGroup
{
public:
    /* Public methods */
    TextureStorageTests(deqp::Context &context);

    void init();

private:
    TextureStorageTests(const TextureStorageTests &other);
    TextureStorageTests &operator=(const TextureStorageTests &other);
};

} // namespace glcts

#endif // _GLCTEXTURESTORAGETESTS_HPP
