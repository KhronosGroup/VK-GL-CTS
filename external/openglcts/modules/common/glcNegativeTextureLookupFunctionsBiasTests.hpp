#ifndef _GLCNEGATIVETEXTURELOOKUPFUNCTIONSBIASTESTS_HPP
#define _GLCNEGATIVETEXTURELOOKUPFUNCTIONSBIASTESTS_HPP

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
 * \file  glcNegativeTextureLookupFunctionsBiasTests.hpp
 * \brief Negative texture lookup functions bias tests Suite Interface
 */ /*-------------------------------------------------------------------*/

#include "glcTestCase.hpp"

namespace glcts
{

class NegativeTextureLookupFunctionsBiasTests : public deqp::TestCaseGroup
{
public:
    NegativeTextureLookupFunctionsBiasTests(deqp::Context &context);
    ~NegativeTextureLookupFunctionsBiasTests() = default;

    void init(void);

private:
    NegativeTextureLookupFunctionsBiasTests(const NegativeTextureLookupFunctionsBiasTests &other);
    NegativeTextureLookupFunctionsBiasTests &operator=(const NegativeTextureLookupFunctionsBiasTests &other);
};

class NegativeTextureLookupFunctionsBiasTest : public deqp::TestCase
{
public:
    NegativeTextureLookupFunctionsBiasTest(deqp::Context &context, const std::string &test_name,
                                           const std::string &test_description, const std::string vertex_shader_txt,
                                           bool texture_shadow_lod_required     = false,
                                           bool texture_cube_map_array_required = false,
                                           bool sparse_texture2_required        = false);
    virtual ~NegativeTextureLookupFunctionsBiasTest() = default;

    tcu::TestNode::IterateResult iterate(void);
    virtual bool test();

protected:
    std::string m_vertex_shader_txt;
    bool m_texture_shadow_lod_required;
    bool m_texture_cube_map_array_required;
    bool m_sparse_texture2_required;
};

} /* namespace glcts */
#endif // _GLCNEGATIVETEXTURELOOKUPFUNCTIONSBIASTESTS_HPP
