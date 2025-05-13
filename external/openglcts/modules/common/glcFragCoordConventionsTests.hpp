#ifndef _GLCFRAGCOORDCONVENTIONSTESTS_HPP
#define _GLCFRAGCOORDCONVENTIONSTESTS_HPP
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
 * \file  glcFragCoordConventionsTests.hpp
 * \brief Conformance tests fragment coord conventions operations.
 */ /*-------------------------------------------------------------------*/

#include "glcTestCase.hpp"
#include "glwDefs.hpp"
#include "tcuDefs.hpp"

#include <map>
#include <memory>

namespace glu
{
class ShaderProgram;
}

namespace glcts
{

struct EpsilonRec
{
    float color[4];
    float zero;
};

struct TestParams
{
    int index;              // Element index to be tested in test case
    int useFBO;             // Set up separate FBO for the case
    int useCull;            // Enable culling test variation
    int scissorTest;        // Enable scissor test variation
    int useMultisample;     // Use multisample FBO for the case
    int gatherSamples;      // Fill reference colors array instead of comparison
    int overrideCheckIndex; // Override "index" for color check index if !=-1
};

/*
SPECIFICATION:

3.4      Sample position not affected
Purpose: Verify that sample positions are not affected when Frag Coord
         Convention is changed.
Method:  Repeat case 2.3 drawing on multisample buffer. Modify default fragment
         shader to influence output color with sample position of the chosen
         sample id.
 */
class FragCoordConventionsMultisampleTestCase : public deqp::TestCase
{
public:
    /* Public methods */
    FragCoordConventionsMultisampleTestCase(deqp::Context &context);

    void deinit();
    void init();
    tcu::TestNode::IterateResult iterate();

private:
    bool doQuadCase(const TestParams &);
    bool drawQuad(const TestParams &, int, int);
    bool checkColor(const int x, const int y, const glw::GLuint reference);
    bool gatherColor(const int x, const int y, glw::GLuint *reference);

    void getBufferBits(glw::GLint colorBits[4]);
    void initEpsilon();
    glw::GLfloat calcEpsilon(long bits);

private:
    /* Private members */
    std::vector<std::unique_ptr<glu::ShaderProgram>> m_programs;

    std::map<std::string, std::string> specializationMap;

    glw::GLuint m_vao;
    glw::GLuint m_vbo;

    bool m_isContextES;
    bool m_testSupported;

    EpsilonRec m_eps;
};

/** Test group which encapsulates all conformance tests */
class FragCoordConventionsTests : public deqp::TestCaseGroup
{
public:
    /* Public methods */
    FragCoordConventionsTests(deqp::Context &context);

    void init();

private:
    FragCoordConventionsTests(const FragCoordConventionsTests &other);
    FragCoordConventionsTests &operator=(const FragCoordConventionsTests &other);
};

} // namespace glcts

#endif // _GLCFRAGCOORDCONVENTIONSTESTS_HPP
