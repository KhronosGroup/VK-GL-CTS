#ifndef _GL3CTRANSFORMFEEDBACK3TESTS_HPP
#define _GL3CTRANSFORMFEEDBACK3TESTS_HPP
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
 * \file  gl3cTransformFeedback3Tests.hpp
 * \brief Conformance tests for ARB_transform_feedback3 functionality.
 */ /*-------------------------------------------------------------------*/

#include "glcTestCase.hpp"
#include "glwDefs.hpp"
#include "glwEnums.hpp"
#include "tcuDefs.hpp"

#include <map>

namespace glu
{
class ShaderProgram;
}

namespace gl3cts
{

// Base class for below test cases
class TransformFeedbackBaseTestCase : public deqp::TestCase
{
public:
	TransformFeedbackBaseTestCase(deqp::Context& context, const char* name, const char* desc)
		: TestCase(context, name, desc), m_testSupported(false), m_vao(0), m_vbo(0), m_program(0)
	{
	}

	void deinit() override;
	void init() override;

	bool createTransformBuffer(const int size, const glw::GLint buffer, const glw::GLint index);

	void createVertexBuffers(const glw::GLuint& program, std::vector<glw::GLfloat>& verts);
	void releaseVertexBuffers();

	bool readBuffer(const int size, const glw::GLint buffer, std::vector<char>& data);
	bool compareArrays(const glw::GLfloat* const d1, const int s1, const glw::GLfloat* d2, int s2);

	void buildTransformFeedbackProgram(const char* vsSource, const char* gsSource, const char* fsSource);

	/* Returns the number of transform feedback varyings. */
	virtual glw::GLsizei varyingsCount() = 0;

	/* Returns the array of transform feedback varying names. */
	virtual const char** varyings() = 0;

	/* Returns the transform feedback buffer mode. */
	virtual glw::GLenum bufferMode() { return GL_INTERLEAVED_ATTRIBS; }

protected:
	std::map<std::string, std::string> specializationMap;

	bool m_testSupported;

	glw::GLuint m_vao;
	glw::GLuint m_vbo;

	glw::GLuint	m_program;

	static const glw::GLchar* m_shader_vert;
	static const glw::GLchar* m_shader_frag;
};

/*
Specification:
* For implementations that support more than one vertex stream verify that
two streams writing to the same buffer object works. Do this by creating
one transform feedback buffer and creating two transform feedback
targets from this buffer. Then record the varyings such that stream 0
writes to the buffer and then has a gl_SkipCompontent4, move to the next
buffer with gl_NextBuffer and then do gl_SkipComponents4 and record the
stream 1 varying. This will interleave two streams into the same
transform feedback buffer. Verify the data in the buffer is correct for
both stream 0 and stream 1 data.

Procedure:
Three points are drawn to one interleaved XFB buffer. Two varyings are
captured with skipping components after (1st varying) and before (2nd varying)
varying definition. Values are therefore captured tightly to a buffer.
*/

class TransformFeedbackMultipleStreamsTestCase : public TransformFeedbackBaseTestCase
{
public:
	/* Public methods */
	TransformFeedbackMultipleStreamsTestCase(deqp::Context& context);

	void init() override;

	tcu::TestNode::IterateResult iterate() override;

	glw::GLsizei varyingsCount() override { return 5; }

	const char** varyings() override;

private:
	/* Private members */
	static const glw::GLchar* m_shader_mult_streams_vert;
	static const glw::GLchar* m_shader_mult_streams_geom;
};

/*
Specification:
* Create a single transform feedback buffer object with multiple tranform
feedback buffer targets. Capture multiple varyings from the shader, but
use gl_SkipComponents1, gl_SkipComponents2, gl_SkipComponents3 and
gl_SkipComponents4 to leave holes in the buffer with undefined data. The
transform feedback buffer should be verified to make sure the undefined
areas of data are unmodified after draw is called. This can be done by
initializing the buffer with known data before doing transform feedback.

Procedure:
Several gl_SkipComponents are used. Every one atleast once and also multiple
skips successively. Buffer is filled with predefined values to check
immutability.
*/

class TransformFeedbackSkipComponentsTestCase : public TransformFeedbackBaseTestCase
{
public:
	/* Public methods */
	TransformFeedbackSkipComponentsTestCase(deqp::Context& context);

	tcu::TestNode::IterateResult iterate() override;

	glw::GLsizei varyingsCount() override { return 10; }

	const char** varyings() override;
};

/*
Specification:
* Create multiple transform feedback buffer objects and use a mixture of
gl_NextBuffer and gl_SkipComponents1-4 to make sure the primitive data
is written to the correct transform feedback buffer object, and to the
correct location within the buffer.

Procedure:
Four XFBs are filled with and different combinations of gl_SkipComponents
before actual values.
*/

class TransformFeedbackSkipMultipleBuffersTestCase : public TransformFeedbackBaseTestCase
{
public:
	/* Public methods */
	TransformFeedbackSkipMultipleBuffersTestCase(deqp::Context& context);

	tcu::TestNode::IterateResult iterate() override;

	glw::GLsizei varyingsCount() override { return 11; }

	const char** varyings() override;
};

/** Test group which encapsulates all conformance tests */
class TransformFeedback3Tests : public deqp::TestCaseGroup
{
public:
	/* Public methods */
	TransformFeedback3Tests(deqp::Context& context);

	void init();

private:
	TransformFeedback3Tests(const TransformFeedback3Tests& other);
	TransformFeedback3Tests& operator=(const TransformFeedback3Tests& other);
};

} // namespace gl3cts

#endif // _GL3CTRANSFORMFEEDBACK3TESTS_HPP
