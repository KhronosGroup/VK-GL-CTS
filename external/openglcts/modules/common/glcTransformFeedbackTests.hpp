#ifndef _GLCTRANSFORMFEEDBACKTESTS_HPP
#define _GLCTRANSFORMFEEDBACKTESTS_HPP
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
 * \file  glcTransformFeedbackTests.hpp
 * \brief Conformance tests for the transform_feedback2 functionality.
 */ /*-------------------------------------------------------------------*/

#include "glcTestCase.hpp"
#include "glwDefs.hpp"
#include "tcuDefs.hpp"

#include <map>

namespace glu
{
class ShaderProgram;
}

namespace glcts
{

/*
Specification:

      Using the Basic Outline above, enable each of the following features and
      permutations and make sure they operate as expected.

    * Create and bind a user transform feedback object with
      GenTransformFeedbacks and BindTransformFeedback and ensure the test
      runs correctly. Delete the user transform buffer object.

    * Create multiple user transform feedback objects and configure different
      state in each object. The state tested should be the following:

          TRANSFORM_FEEDBACK_BUFFER_BINDING
          TRANSFORM_FEEDBACK_BUFFER_START
          TRANSFORM_FEEDBACK_BUFFER_SIZE

    * Draw a subset of the primitives for the test, call
      PauseTransformFeedback, draw other primitives not part of the test,
      call ResumeTransformFeedback and continue with the remaining primitives.
      The feedback buffer should only contain primitives drawn while the
      transform feedback object is not paused.

      Query the transform feedback state for TRANSFORM_FEEDBACK_BUFFER_PAUSED
      and TRANSFORM_FEEDBACK_BUFFER_ACTIVE to verify the state is reflected
      correctly.

Procedure:

    Draw and query state

Notes:
*/
class TransformFeedbackStatesTestCase : public deqp::TestCase
{
public:
    /* Public methods */
    TransformFeedbackStatesTestCase(deqp::Context &context);
    ~TransformFeedbackStatesTestCase();

    void deinit();
    void init();
    tcu::TestNode::IterateResult iterate();

private:
    /* Private methods */
    bool draw_simple2(glw::GLuint program, glw::GLenum primitivetype, glw::GLint vertexcount, bool pauseresume);
    void buildTransformFeedbackProgram(const char *vsSource, const char *fsSource);

    /* Private members */
    static const glw::GLchar *m_shader_vert;
    static const glw::GLchar *m_shader_frag;

    glw::GLuint m_program;

    std::map<std::string, std::string> specializationMap;

    glw::GLuint m_vao;

    glw::GLuint m_buffers[2];
    glw::GLuint m_tf_id;
    glw::GLuint m_queries[2];

    bool m_isContextES;
    bool m_testSupported;
};

/** Test group which encapsulates all conformance tests */
class TransformFeedbackTests : public deqp::TestCaseGroup
{
public:
    /* Public methods */
    TransformFeedbackTests(deqp::Context &context);

    void init();

private:
    TransformFeedbackTests(const TransformFeedbackTests &other);
    TransformFeedbackTests &operator=(const TransformFeedbackTests &other);
};

} // namespace glcts

#endif // _GLCTRANSFORMFEEDBACKTESTS_HPP
