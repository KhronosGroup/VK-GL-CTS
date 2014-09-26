/*-------------------------------------------------------------------------
 * drawElements Quality Program OpenGL ES 3.1 Module
 * -------------------------------------------------
 *
 * Copyright 2014 The Android Open Source Project
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
 *//*!
 * \file
 * \brief Negative Vertex Array API tests.
 *//*--------------------------------------------------------------------*/

#include "es31fNegativeVertexArrayApiTests.hpp"

#include "gluCallLogWrapper.hpp"
#include "gluContextInfo.hpp"
#include "gluShaderProgram.hpp"

#include "glwDefs.hpp"
#include "glwEnums.hpp"

namespace deqp
{
namespace gles31
{
namespace Functional
{
namespace NegativeTestShared
{

using tcu::TestLog;
using glu::CallLogWrapper;
using namespace glw;

static const char* vertexShaderSource		=	"#version 300 es\n"
												"void main (void)\n"
												"{\n"
												"	gl_Position = vec4(0.0);\n"
												"}\n\0";

static const char* fragmentShaderSource		=	"#version 300 es\n"
												"layout(location = 0) out mediump vec4 fragColor;"
												"void main (void)\n"
												"{\n"
												"	fragColor = vec4(0.0);\n"
												"}\n\0";

void vertex_attribf (NegativeTestContext& ctx)
{
	ctx.beginSection("GL_INVALID_VALUE is generated if index is greater than or equal to GL_MAX_VERTEX_ATTRIBS.");
	int maxVertexAttribs = ctx.getInteger(GL_MAX_VERTEX_ATTRIBS);
	ctx.glVertexAttrib1f(maxVertexAttribs, 0.0f);
	ctx.expectError(GL_INVALID_VALUE);
	ctx.glVertexAttrib2f(maxVertexAttribs, 0.0f, 0.0f);
	ctx.expectError(GL_INVALID_VALUE);
	ctx.glVertexAttrib3f(maxVertexAttribs, 0.0f, 0.0f, 0.0f);
	ctx.expectError(GL_INVALID_VALUE);
	ctx.glVertexAttrib4f(maxVertexAttribs, 0.0f, 0.0f, 0.0f, 0.0f);
	ctx.expectError(GL_INVALID_VALUE);
	ctx.endSection();
}

void vertex_attribfv (NegativeTestContext& ctx)
{
	ctx.beginSection("GL_INVALID_VALUE is generated if index is greater than or equal to GL_MAX_VERTEX_ATTRIBS.");
	int maxVertexAttribs = ctx.getInteger(GL_MAX_VERTEX_ATTRIBS);
	float v[4] = {0.0f};
	ctx.glVertexAttrib1fv(maxVertexAttribs, &v[0]);
	ctx.expectError(GL_INVALID_VALUE);
	ctx.glVertexAttrib2fv(maxVertexAttribs, &v[0]);
	ctx.expectError(GL_INVALID_VALUE);
	ctx.glVertexAttrib3fv(maxVertexAttribs, &v[0]);
	ctx.expectError(GL_INVALID_VALUE);
	ctx.glVertexAttrib4fv(maxVertexAttribs, &v[0]);
	ctx.expectError(GL_INVALID_VALUE);
	ctx.endSection();
}

void vertex_attribi4 (NegativeTestContext& ctx)
{
	int maxVertexAttribs	= ctx.getInteger(GL_MAX_VERTEX_ATTRIBS);
	GLint valInt			= 0;
	GLuint valUint			= 0;

	ctx.beginSection("GL_INVALID_VALUE is generated if index is greater than or equal to GL_MAX_VERTEX_ATTRIBS.");
	ctx.glVertexAttribI4i(maxVertexAttribs, valInt, valInt, valInt, valInt);
	ctx.expectError(GL_INVALID_VALUE);
	ctx.glVertexAttribI4ui(maxVertexAttribs, valUint, valUint, valUint, valUint);
	ctx.expectError(GL_INVALID_VALUE);
	ctx.endSection();
}

void vertex_attribi4v (NegativeTestContext& ctx)
{
	int maxVertexAttribs	= ctx.getInteger(GL_MAX_VERTEX_ATTRIBS);
	GLint valInt[4]			= { 0 };
	GLuint valUint[4]		= { 0 };

	ctx.beginSection("GL_INVALID_VALUE is generated if index is greater than or equal to GL_MAX_VERTEX_ATTRIBS.");
	ctx.glVertexAttribI4iv(maxVertexAttribs, &valInt[0]);
	ctx.expectError(GL_INVALID_VALUE);
	ctx.glVertexAttribI4uiv(maxVertexAttribs, &valUint[0]);
	ctx.expectError(GL_INVALID_VALUE);
	ctx.endSection();
}

void vertex_attrib_pointer (NegativeTestContext& ctx)
{
	ctx.beginSection("GL_INVALID_ENUM is generated if type is not an accepted value.");
	ctx.glVertexAttribPointer(0, 1, 0, GL_TRUE, 0, 0);
	ctx.expectError(GL_INVALID_ENUM);
	ctx.endSection();

	ctx.beginSection("GL_INVALID_VALUE is generated if index is greater than or equal to GL_MAX_VERTEX_ATTRIBS.");
	int maxVertexAttribs = ctx.getInteger(GL_MAX_VERTEX_ATTRIBS);
	ctx.glVertexAttribPointer(maxVertexAttribs, 1, GL_BYTE, GL_TRUE, 0, 0);
	ctx.expectError(GL_INVALID_VALUE);
	ctx.endSection();

	ctx.beginSection("GL_INVALID_VALUE is generated if size is not 1, 2, 3, or 4.");
	ctx.glVertexAttribPointer(0, 0, GL_BYTE, GL_TRUE, 0, 0);
	ctx.expectError(GL_INVALID_VALUE);
	ctx.endSection();

	ctx.beginSection("GL_INVALID_VALUE is generated if stride is negative.");
	ctx.glVertexAttribPointer(0, 1, GL_BYTE, GL_TRUE, -1, 0);
	ctx.expectError(GL_INVALID_VALUE);
	ctx.endSection();

	ctx.beginSection("GL_INVALID_OPERATION is generated if type is GL_INT_2_10_10_10_REV or GL_UNSIGNED_INT_2_10_10_10_REV and size is not 4.");
	ctx.glVertexAttribPointer(0, 2, GL_INT_2_10_10_10_REV, GL_TRUE, 0, 0);
	ctx.expectError(GL_INVALID_OPERATION);
	ctx.glVertexAttribPointer(0, 2, GL_UNSIGNED_INT_2_10_10_10_REV, GL_TRUE, 0, 0);
	ctx.expectError(GL_INVALID_OPERATION);
	ctx.glVertexAttribPointer(0, 4, GL_INT_2_10_10_10_REV, GL_TRUE, 0, 0);
	ctx.expectError(GL_NO_ERROR);
	ctx.glVertexAttribPointer(0, 4, GL_UNSIGNED_INT_2_10_10_10_REV, GL_TRUE, 0, 0);
	ctx.expectError(GL_NO_ERROR);
	ctx.endSection();

	ctx.beginSection("GL_INVALID_OPERATION is generated a non-zero vertex array object is bound, zero is bound to the GL_ARRAY_BUFFER buffer object binding point and the pointer argument is not NULL.");
	GLuint vao = 0;
	GLbyte offset = 1;
	ctx.glGenVertexArrays(1, &vao);
	ctx.glBindVertexArray(vao);
	ctx.glBindBuffer(GL_ARRAY_BUFFER, 0);
	ctx.expectError(GL_NO_ERROR);

	ctx.glVertexAttribPointer(0, 1, GL_BYTE, GL_TRUE, 0, &offset);
	ctx.expectError(GL_INVALID_OPERATION);

	ctx.glBindVertexArray(0);
	ctx.glDeleteVertexArrays(1, &vao);
	ctx.expectError(GL_NO_ERROR);
	ctx.endSection();
}

void vertex_attrib_i_pointer (NegativeTestContext& ctx)
{
	ctx.beginSection("GL_INVALID_ENUM is generated if type is not an accepted value.");
	ctx.glVertexAttribIPointer(0, 1, 0, 0, 0);
	ctx.expectError(GL_INVALID_ENUM);
	ctx.glVertexAttribIPointer(0, 4, GL_INT_2_10_10_10_REV, 0, 0);
	ctx.expectError(GL_INVALID_ENUM);
	ctx.glVertexAttribIPointer(0, 4, GL_UNSIGNED_INT_2_10_10_10_REV, 0, 0);
	ctx.expectError(GL_INVALID_ENUM);
	ctx.endSection();

	ctx.beginSection("GL_INVALID_VALUE is generated if index is greater than or equal to GL_MAX_VERTEX_ATTRIBS.");
	int maxVertexAttribs = ctx.getInteger(GL_MAX_VERTEX_ATTRIBS);
	ctx.glVertexAttribIPointer(maxVertexAttribs, 1, GL_BYTE, 0, 0);
	ctx.expectError(GL_INVALID_VALUE);
	ctx.endSection();

	ctx.beginSection("GL_INVALID_VALUE is generated if size is not 1, 2, 3, or 4.");
	ctx.glVertexAttribIPointer(0, 0, GL_BYTE, 0, 0);
	ctx.expectError(GL_INVALID_VALUE);
	ctx.endSection();

	ctx.beginSection("GL_INVALID_VALUE is generated if stride is negative.");
	ctx.glVertexAttribIPointer(0, 1, GL_BYTE, -1, 0);
	ctx.expectError(GL_INVALID_VALUE);
	ctx.endSection();

	ctx.beginSection("GL_INVALID_OPERATION is generated a non-zero vertex array object is bound, zero is bound to the GL_ARRAY_BUFFER buffer object binding point and the pointer argument is not NULL.");
	GLuint vao = 0;
	GLbyte offset = 1;
	ctx.glGenVertexArrays(1, &vao);
	ctx.glBindVertexArray(vao);
	ctx.glBindBuffer(GL_ARRAY_BUFFER, 0);
	ctx.expectError(GL_NO_ERROR);

	ctx.glVertexAttribIPointer(0, 1, GL_BYTE, 0, &offset);
	ctx.expectError(GL_INVALID_OPERATION);

	ctx.glBindVertexArray(0);
	ctx.glDeleteVertexArrays(1, &vao);
	ctx.expectError(GL_NO_ERROR);
	ctx.endSection();
}

void enable_vertex_attrib_array (NegativeTestContext& ctx)
{
	ctx.beginSection("GL_INVALID_VALUE is generated if index is greater than or equal to GL_MAX_VERTEX_ATTRIBS.");
	int maxVertexAttribs = ctx.getInteger(GL_MAX_VERTEX_ATTRIBS);
	ctx.glEnableVertexAttribArray(maxVertexAttribs);
	ctx.expectError(GL_INVALID_VALUE);
	ctx.endSection();
}

void disable_vertex_attrib_array (NegativeTestContext& ctx)
{
	ctx.beginSection("GL_INVALID_VALUE is generated if index is greater than or equal to GL_MAX_VERTEX_ATTRIBS.");
	int maxVertexAttribs = ctx.getInteger(GL_MAX_VERTEX_ATTRIBS);
	ctx.glDisableVertexAttribArray(maxVertexAttribs);
	ctx.expectError(GL_INVALID_VALUE);
	ctx.endSection();
}

void gen_vertex_arrays (NegativeTestContext& ctx)
{
	ctx.beginSection("GL_INVALID_VALUE is generated if n is negative.");
	GLuint arrays = 0;
	ctx.glGenVertexArrays(-1, &arrays);
	ctx.expectError(GL_INVALID_VALUE);
	ctx.endSection();
}

void bind_vertex_array (NegativeTestContext& ctx)
{
	ctx.beginSection("GL_INVALID_OPERATION is generated if array is not zero or the name of an existing vertex array object.");
	ctx.glBindVertexArray(-1);
	ctx.expectError(GL_INVALID_OPERATION);
	ctx.endSection();
}

void delete_vertex_arrays (NegativeTestContext& ctx)
{
	ctx.beginSection("GL_INVALID_VALUE is generated if n is negative.");
	ctx.glDeleteVertexArrays(-1, 0);
	ctx.expectError(GL_INVALID_VALUE);
	ctx.endSection();
}

void vertex_attrib_divisor (NegativeTestContext& ctx)
{
	ctx.beginSection("GL_INVALID_VALUE is generated if index is greater than or equal to GL_MAX_VERTEX_ATTRIBS.");
	int maxVertexAttribs = ctx.getInteger(GL_MAX_VERTEX_ATTRIBS);
	ctx.glVertexAttribDivisor(maxVertexAttribs, 0);
	ctx.expectError(GL_INVALID_VALUE);
	ctx.endSection();
}

void draw_arrays (NegativeTestContext& ctx)
{
	glu::ShaderProgram program(ctx.getRenderContext(), glu::makeVtxFragSources(vertexShaderSource, fragmentShaderSource));
	ctx.glUseProgram(program.getProgram());
	GLuint fbo = 0;

	ctx.beginSection("GL_INVALID_ENUM is generated if mode is not an accepted value.");
	ctx.glDrawArrays(-1, 0, 1);
	ctx.expectError(GL_INVALID_ENUM);
	ctx.endSection();

	ctx.beginSection("GL_INVALID_VALUE is generated if count is negative.");
	ctx.glDrawArrays(GL_POINTS, 0, -1);
	ctx.expectError(GL_INVALID_VALUE);
	ctx.endSection();

	ctx.beginSection("GL_INVALID_FRAMEBUFFER_OPERATION is generated if the currently bound framebuffer is not framebuffer complete.");
	ctx.glGenFramebuffers(1, &fbo);
	ctx.glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	ctx.glCheckFramebufferStatus(GL_FRAMEBUFFER);
	ctx.glDrawArrays(GL_POINTS, 0, 1);
	ctx.expectError(GL_INVALID_FRAMEBUFFER_OPERATION);
	ctx.glBindFramebuffer(GL_FRAMEBUFFER, 0);
	ctx.glDeleteFramebuffers(1, &fbo);
	ctx.endSection();

	ctx.glUseProgram(0);
}

void draw_arrays_invalid_program (NegativeTestContext& ctx)
{
	ctx.glUseProgram(0);
	GLuint fbo = 0;

	ctx.beginSection("GL_INVALID_ENUM is generated if mode is not an accepted value.");
	ctx.glDrawArrays(-1, 0, 1);
	ctx.expectError(GL_INVALID_ENUM);
	ctx.endSection();

	ctx.beginSection("GL_INVALID_VALUE is generated if count is negative.");
	ctx.glDrawArrays(GL_POINTS, 0, -1);
	ctx.expectError(GL_INVALID_VALUE);
	ctx.endSection();

	ctx.beginSection("GL_INVALID_FRAMEBUFFER_OPERATION is generated if the currently bound framebuffer is not framebuffer complete.");
	ctx.glGenFramebuffers(1, &fbo);
	ctx.glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	ctx.glCheckFramebufferStatus(GL_FRAMEBUFFER);
	ctx.glDrawArrays(GL_POINTS, 0, 1);
	ctx.expectError(GL_INVALID_FRAMEBUFFER_OPERATION);
	ctx.glBindFramebuffer(GL_FRAMEBUFFER, 0);
	ctx.glDeleteFramebuffers(1, &fbo);
	ctx.endSection();
}

void draw_arrays_incomplete_primitive (NegativeTestContext& ctx)
{
	glu::ShaderProgram program(ctx.getRenderContext(), glu::makeVtxFragSources(vertexShaderSource, fragmentShaderSource));
	ctx.glUseProgram(program.getProgram());
	GLuint fbo = 0;

	ctx.beginSection("GL_INVALID_ENUM is generated if mode is not an accepted value.");
	ctx.glDrawArrays(-1, 0, 1);
	ctx.expectError(GL_INVALID_ENUM);
	ctx.endSection();

	ctx.beginSection("GL_INVALID_VALUE is generated if count is negative.");
	ctx.glDrawArrays(GL_TRIANGLES, 0, -1);
	ctx.expectError(GL_INVALID_VALUE);
	ctx.endSection();

	ctx.beginSection("GL_INVALID_FRAMEBUFFER_OPERATION is generated if the currently bound framebuffer is not framebuffer complete.");
	ctx.glGenFramebuffers(1, &fbo);
	ctx.glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	ctx.glCheckFramebufferStatus(GL_FRAMEBUFFER);
	ctx.glDrawArrays(GL_TRIANGLES, 0, 1);
	ctx.expectError(GL_INVALID_FRAMEBUFFER_OPERATION);
	ctx.glBindFramebuffer(GL_FRAMEBUFFER, 0);
	ctx.glDeleteFramebuffers(1, &fbo);
	ctx.endSection();

	ctx.glUseProgram(0);
}

void draw_elements (NegativeTestContext& ctx)
{
	glu::ShaderProgram program(ctx.getRenderContext(), glu::makeVtxFragSources(vertexShaderSource, fragmentShaderSource));
	ctx.glUseProgram(program.getProgram());
	GLuint fbo = 0;
	GLuint buf = 0;
	GLuint tfID = 0;
	GLfloat vertices[1];

	ctx.beginSection("GL_INVALID_ENUM is generated if mode is not an accepted value.");
	ctx.glDrawElements(-1, 1, GL_UNSIGNED_BYTE, vertices);
	ctx.expectError(GL_INVALID_ENUM);
	ctx.endSection();

	ctx.beginSection("GL_INVALID_ENUM is generated if type is not one of the accepted values.");
	ctx.glDrawElements(GL_POINTS, 1, -1, vertices);
	ctx.expectError(GL_INVALID_ENUM);
	ctx.glDrawElements(GL_POINTS, 1, GL_FLOAT, vertices);
	ctx.expectError(GL_INVALID_ENUM);
	ctx.endSection();

	ctx.beginSection("GL_INVALID_VALUE is generated if count is negative.");
	ctx.glDrawElements(GL_POINTS, -1, GL_UNSIGNED_BYTE, vertices);
	ctx.expectError(GL_INVALID_VALUE);
	ctx.endSection();

	ctx.beginSection("GL_INVALID_FRAMEBUFFER_OPERATION is generated if the currently bound framebuffer is not framebuffer complete.");
	ctx.glGenFramebuffers(1, &fbo);
	ctx.glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	ctx.glCheckFramebufferStatus(GL_FRAMEBUFFER);
	ctx.glDrawElements(GL_POINTS, 1, GL_UNSIGNED_BYTE, vertices);
	ctx.expectError(GL_INVALID_FRAMEBUFFER_OPERATION);
	ctx.glBindFramebuffer(GL_FRAMEBUFFER, 0);
	ctx.glDeleteFramebuffers(1, &fbo);
	ctx.endSection();

	if (!ctx.getContextInfo().isExtensionSupported("GL_EXT_geometry_shader")) // GL_EXT_geometry_shader removes error
	{
		ctx.beginSection("GL_INVALID_OPERATION is generated if transform feedback is active and not paused.");
		const char* tfVarying		= "gl_Position";

		ctx.glGenBuffers				(1, &buf);
		ctx.glGenTransformFeedbacks		(1, &tfID);

		ctx.glUseProgram				(program.getProgram());
		ctx.glTransformFeedbackVaryings	(program.getProgram(), 1, &tfVarying, GL_INTERLEAVED_ATTRIBS);
		ctx.glLinkProgram				(program.getProgram());
		ctx.glBindTransformFeedback		(GL_TRANSFORM_FEEDBACK, tfID);
		ctx.glBindBuffer				(GL_TRANSFORM_FEEDBACK_BUFFER, buf);
		ctx.glBufferData				(GL_TRANSFORM_FEEDBACK_BUFFER, 32, DE_NULL, GL_DYNAMIC_DRAW);
		ctx.glBindBufferBase			(GL_TRANSFORM_FEEDBACK_BUFFER, 0, buf);
		ctx.glBeginTransformFeedback	(GL_POINTS);
		ctx.expectError				(GL_NO_ERROR);

		ctx.glDrawElements				(GL_POINTS, 1, GL_UNSIGNED_BYTE, vertices);
		ctx.expectError				(GL_INVALID_OPERATION);

		ctx.glPauseTransformFeedback();
		ctx.glDrawElements				(GL_POINTS, 1, GL_UNSIGNED_BYTE, vertices);
		ctx.expectError				(GL_NO_ERROR);

		ctx.glEndTransformFeedback		();
		ctx.glDeleteBuffers				(1, &buf);
		ctx.glDeleteTransformFeedbacks	(1, &tfID);
		ctx.expectError				(GL_NO_ERROR);
		ctx.endSection();
	}

	ctx.glUseProgram(0);
}

void draw_elements_invalid_program (NegativeTestContext& ctx)
{
	ctx.glUseProgram(0);
	GLuint fbo = 0;
	GLfloat vertices[1];

	ctx.beginSection("GL_INVALID_ENUM is generated if mode is not an accepted value.");
	ctx.glDrawElements(-1, 1, GL_UNSIGNED_BYTE, vertices);
	ctx.expectError(GL_INVALID_ENUM);
	ctx.endSection();

	ctx.beginSection("GL_INVALID_ENUM is generated if type is not one of the accepted values.");
	ctx.glDrawElements(GL_POINTS, 1, -1, vertices);
	ctx.expectError(GL_INVALID_ENUM);
	ctx.glDrawElements(GL_POINTS, 1, GL_FLOAT, vertices);
	ctx.expectError(GL_INVALID_ENUM);
	ctx.endSection();

	ctx.beginSection("GL_INVALID_VALUE is generated if count is negative.");
	ctx.glDrawElements(GL_POINTS, -1, GL_UNSIGNED_BYTE, vertices);
	ctx.expectError(GL_INVALID_VALUE);
	ctx.endSection();

	ctx.beginSection("GL_INVALID_FRAMEBUFFER_OPERATION is generated if the currently bound framebuffer is not framebuffer complete.");
	ctx.glGenFramebuffers(1, &fbo);
	ctx.glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	ctx.glCheckFramebufferStatus(GL_FRAMEBUFFER);
	ctx.glDrawElements(GL_POINTS, 1, GL_UNSIGNED_BYTE, vertices);
	ctx.expectError(GL_INVALID_FRAMEBUFFER_OPERATION);
	ctx.glBindFramebuffer(GL_FRAMEBUFFER, 0);
	ctx.glDeleteFramebuffers(1, &fbo);
	ctx.endSection();
}

void draw_elements_incomplete_primitive (NegativeTestContext& ctx)
{
	glu::ShaderProgram program(ctx.getRenderContext(), glu::makeVtxFragSources(vertexShaderSource, fragmentShaderSource));
	ctx.glUseProgram(program.getProgram());
	GLuint fbo = 0;
	GLuint buf = 0;
	GLuint tfID = 0;
	GLfloat vertices[1];

	ctx.beginSection("GL_INVALID_ENUM is generated if mode is not an accepted value.");
	ctx.glDrawElements(-1, 1, GL_UNSIGNED_BYTE, vertices);
	ctx.expectError(GL_INVALID_ENUM);
	ctx.endSection();

	ctx.beginSection("GL_INVALID_ENUM is generated if type is not one of the accepted values.");
	ctx.glDrawElements(GL_TRIANGLES, 1, -1, vertices);
	ctx.expectError(GL_INVALID_ENUM);
	ctx.glDrawElements(GL_TRIANGLES, 1, GL_FLOAT, vertices);
	ctx.expectError(GL_INVALID_ENUM);
	ctx.endSection();

	ctx.beginSection("GL_INVALID_VALUE is generated if count is negative.");
	ctx.glDrawElements(GL_TRIANGLES, -1, GL_UNSIGNED_BYTE, vertices);
	ctx.expectError(GL_INVALID_VALUE);
	ctx.endSection();

	ctx.beginSection("GL_INVALID_FRAMEBUFFER_OPERATION is generated if the currently bound framebuffer is not framebuffer complete.");
	ctx.glGenFramebuffers(1, &fbo);
	ctx.glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	ctx.glCheckFramebufferStatus(GL_FRAMEBUFFER);
	ctx.glDrawElements(GL_TRIANGLES, 1, GL_UNSIGNED_BYTE, vertices);
	ctx.expectError(GL_INVALID_FRAMEBUFFER_OPERATION);
	ctx.glBindFramebuffer(GL_FRAMEBUFFER, 0);
	ctx.glDeleteFramebuffers(1, &fbo);
	ctx.endSection();

	if (!ctx.getContextInfo().isExtensionSupported("GL_EXT_geometry_shader")) // GL_EXT_geometry_shader removes error
	{
		ctx.beginSection("GL_INVALID_OPERATION is generated if transform feedback is active and not paused.");
		const char* tfVarying		= "gl_Position";

		ctx.glGenBuffers				(1, &buf);
		ctx.glGenTransformFeedbacks		(1, &tfID);

		ctx.glUseProgram				(program.getProgram());
		ctx.glTransformFeedbackVaryings	(program.getProgram(), 1, &tfVarying, GL_INTERLEAVED_ATTRIBS);
		ctx.glLinkProgram				(program.getProgram());
		ctx.glBindTransformFeedback		(GL_TRANSFORM_FEEDBACK, tfID);
		ctx.glBindBuffer				(GL_TRANSFORM_FEEDBACK_BUFFER, buf);
		ctx.glBufferData				(GL_TRANSFORM_FEEDBACK_BUFFER, 32, DE_NULL, GL_DYNAMIC_DRAW);
		ctx.glBindBufferBase			(GL_TRANSFORM_FEEDBACK_BUFFER, 0, buf);
		ctx.glBeginTransformFeedback	(GL_TRIANGLES);
		ctx.expectError					(GL_NO_ERROR);

		ctx.glDrawElements				(GL_TRIANGLES, 1, GL_UNSIGNED_BYTE, vertices);
		ctx.expectError					(GL_INVALID_OPERATION);

		ctx.glPauseTransformFeedback	();
		ctx.glDrawElements				(GL_TRIANGLES, 1, GL_UNSIGNED_BYTE, vertices);
		ctx.expectError					(GL_NO_ERROR);

		ctx.glEndTransformFeedback		();
		ctx.glDeleteBuffers				(1, &buf);
		ctx.glDeleteTransformFeedbacks	(1, &tfID);
		ctx.expectError					(GL_NO_ERROR);
		ctx.endSection					();
	}

	ctx.glUseProgram(0);
}

void draw_arrays_instanced (NegativeTestContext& ctx)
{
	glu::ShaderProgram program(ctx.getRenderContext(), glu::makeVtxFragSources(vertexShaderSource, fragmentShaderSource));
	ctx.glUseProgram(program.getProgram());
	GLuint fbo = 0;
	ctx.glVertexAttribDivisor(0, 1);
	ctx.expectError(GL_NO_ERROR);

	ctx.beginSection("GL_INVALID_ENUM is generated if mode is not an accepted value.");
	ctx.glDrawArraysInstanced(-1, 0, 1, 1);
	ctx.expectError(GL_INVALID_ENUM);
	ctx.endSection();

	ctx.beginSection("GL_INVALID_VALUE is generated if count or primcount are negative.");
	ctx.glDrawArraysInstanced(GL_POINTS, 0, -1, 1);
	ctx.expectError(GL_INVALID_VALUE);
	ctx.glDrawArraysInstanced(GL_POINTS, 0, 1, -1);
	ctx.expectError(GL_INVALID_VALUE);
	ctx.endSection();

	ctx.beginSection("GL_INVALID_FRAMEBUFFER_OPERATION is generated if the currently bound framebuffer is not framebuffer complete.");
	ctx.glGenFramebuffers(1, &fbo);
	ctx.glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	ctx.glCheckFramebufferStatus(GL_FRAMEBUFFER);
	ctx.glDrawArraysInstanced(GL_POINTS, 0, 1, 1);
	ctx.expectError(GL_INVALID_FRAMEBUFFER_OPERATION);
	ctx.glBindFramebuffer(GL_FRAMEBUFFER, 0);
	ctx.glDeleteFramebuffers(1, &fbo);
	ctx.endSection();

	ctx.glUseProgram(0);
}

void draw_arrays_instanced_invalid_program (NegativeTestContext& ctx)
{
	ctx.glUseProgram(0);
	GLuint fbo = 0;
	ctx.glVertexAttribDivisor(0, 1);
	ctx.expectError(GL_NO_ERROR);

	ctx.beginSection("GL_INVALID_ENUM is generated if mode is not an accepted value.");
	ctx.glDrawArraysInstanced(-1, 0, 1, 1);
	ctx.expectError(GL_INVALID_ENUM);
	ctx.endSection();

	ctx.beginSection("GL_INVALID_VALUE is generated if count or primcount are negative.");
	ctx.glDrawArraysInstanced(GL_POINTS, 0, -1, 1);
	ctx.expectError(GL_INVALID_VALUE);
	ctx.glDrawArraysInstanced(GL_POINTS, 0, 1, -1);
	ctx.expectError(GL_INVALID_VALUE);
	ctx.endSection();

	ctx.beginSection("GL_INVALID_FRAMEBUFFER_OPERATION is generated if the currently bound framebuffer is not framebuffer complete.");
	ctx.glGenFramebuffers(1, &fbo);
	ctx.glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	ctx.glCheckFramebufferStatus(GL_FRAMEBUFFER);
	ctx.glDrawArraysInstanced(GL_POINTS, 0, 1, 1);
	ctx.expectError(GL_INVALID_FRAMEBUFFER_OPERATION);
	ctx.glBindFramebuffer(GL_FRAMEBUFFER, 0);
	ctx.glDeleteFramebuffers(1, &fbo);
	ctx.endSection();
}

void draw_arrays_instanced_incomplete_primitive (NegativeTestContext& ctx)
{
	glu::ShaderProgram program(ctx.getRenderContext(), glu::makeVtxFragSources(vertexShaderSource, fragmentShaderSource));
	ctx.glUseProgram(program.getProgram());
	GLuint fbo = 0;
	ctx.glVertexAttribDivisor(0, 1);
	ctx.expectError(GL_NO_ERROR);

	ctx.beginSection("GL_INVALID_ENUM is generated if mode is not an accepted value.");
	ctx.glDrawArraysInstanced(-1, 0, 1, 1);
	ctx.expectError(GL_INVALID_ENUM);
	ctx.endSection();

	ctx.beginSection("GL_INVALID_VALUE is generated if count or primcount are negative.");
	ctx.glDrawArraysInstanced(GL_TRIANGLES, 0, -1, 1);
	ctx.expectError(GL_INVALID_VALUE);
	ctx.glDrawArraysInstanced(GL_TRIANGLES, 0, 1, -1);
	ctx.expectError(GL_INVALID_VALUE);
	ctx.endSection();

	ctx.beginSection("GL_INVALID_FRAMEBUFFER_OPERATION is generated if the currently bound framebuffer is not framebuffer complete.");
	ctx.glGenFramebuffers(1, &fbo);
	ctx.glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	ctx.glCheckFramebufferStatus(GL_FRAMEBUFFER);
	ctx.glDrawArraysInstanced(GL_TRIANGLES, 0, 1, 1);
	ctx.expectError(GL_INVALID_FRAMEBUFFER_OPERATION);
	ctx.glBindFramebuffer(GL_FRAMEBUFFER, 0);
	ctx.glDeleteFramebuffers(1, &fbo);
	ctx.endSection();

	ctx.glUseProgram(0);
}

void draw_elements_instanced (NegativeTestContext& ctx)
{
	glu::ShaderProgram program(ctx.getRenderContext(), glu::makeVtxFragSources(vertexShaderSource, fragmentShaderSource));
	ctx.glUseProgram(program.getProgram());
	GLuint fbo = 0;
	GLuint buf = 0;
	GLuint tfID = 0;
	GLfloat vertices[1];
	ctx.glVertexAttribDivisor(0, 1);
	ctx.expectError(GL_NO_ERROR);

	ctx.beginSection("GL_INVALID_ENUM is generated if mode is not an accepted value.");
	ctx.glDrawElementsInstanced(-1, 1, GL_UNSIGNED_BYTE, vertices, 1);
	ctx.expectError(GL_INVALID_ENUM);
	ctx.endSection();

	ctx.beginSection("GL_INVALID_ENUM is generated if type is not one of the accepted values.");
	ctx.glDrawElementsInstanced(GL_POINTS, 1, -1, vertices, 1);
	ctx.expectError(GL_INVALID_ENUM);
	ctx.glDrawElementsInstanced(GL_POINTS, 1, GL_FLOAT, vertices, 1);
	ctx.expectError(GL_INVALID_ENUM);
	ctx.endSection();

	ctx.beginSection("GL_INVALID_VALUE is generated if count or primcount are negative.");
	ctx.glDrawElementsInstanced(GL_POINTS, -1, GL_UNSIGNED_BYTE, vertices, 1);
	ctx.expectError(GL_INVALID_VALUE);
	ctx.glDrawElementsInstanced(GL_POINTS, 11, GL_UNSIGNED_BYTE, vertices, -1);
	ctx.expectError(GL_INVALID_VALUE);
	ctx.endSection();

	ctx.beginSection("GL_INVALID_FRAMEBUFFER_OPERATION is generated if the currently bound framebuffer is not framebuffer complete.");
	ctx.glGenFramebuffers(1, &fbo);
	ctx.glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	ctx.glCheckFramebufferStatus(GL_FRAMEBUFFER);
	ctx.glDrawElementsInstanced(GL_POINTS, 1, GL_UNSIGNED_BYTE, vertices, 1);
	ctx.expectError(GL_INVALID_FRAMEBUFFER_OPERATION);
	ctx.glBindFramebuffer(GL_FRAMEBUFFER, 0);
	ctx.glDeleteFramebuffers(1, &fbo);
	ctx.endSection();

	if (!ctx.getContextInfo().isExtensionSupported("GL_EXT_geometry_shader")) // GL_EXT_geometry_shader removes error
	{
		ctx.beginSection("GL_INVALID_OPERATION is generated if transform feedback is active and not paused.");
		const char* tfVarying		= "gl_Position";

		ctx.glGenBuffers				(1, &buf);
		ctx.glGenTransformFeedbacks		(1, &tfID);

		ctx.glUseProgram				(program.getProgram());
		ctx.glTransformFeedbackVaryings	(program.getProgram(), 1, &tfVarying, GL_INTERLEAVED_ATTRIBS);
		ctx.glLinkProgram				(program.getProgram());
		ctx.glBindTransformFeedback		(GL_TRANSFORM_FEEDBACK, tfID);
		ctx.glBindBuffer				(GL_TRANSFORM_FEEDBACK_BUFFER, buf);
		ctx.glBufferData				(GL_TRANSFORM_FEEDBACK_BUFFER, 32, DE_NULL, GL_DYNAMIC_DRAW);
		ctx.glBindBufferBase			(GL_TRANSFORM_FEEDBACK_BUFFER, 0, buf);
		ctx.glBeginTransformFeedback	(GL_POINTS);
		ctx.expectError				(GL_NO_ERROR);

		ctx.glDrawElementsInstanced		(GL_POINTS, 1, GL_UNSIGNED_BYTE, vertices, 1);
		ctx.expectError				(GL_INVALID_OPERATION);

		ctx.glPauseTransformFeedback();
		ctx.glDrawElementsInstanced		(GL_POINTS, 1, GL_UNSIGNED_BYTE, vertices, 1);
		ctx.expectError				(GL_NO_ERROR);

		ctx.glEndTransformFeedback		();
		ctx.glDeleteBuffers				(1, &buf);
		ctx.glDeleteTransformFeedbacks	(1, &tfID);
		ctx.expectError				(GL_NO_ERROR);
		ctx.endSection();
	}

	ctx.glUseProgram(0);
}

void draw_elements_instanced_invalid_program (NegativeTestContext& ctx)
{
	ctx.glUseProgram(0);
	GLuint fbo = 0;
	GLfloat vertices[1];
	ctx.glVertexAttribDivisor(0, 1);
	ctx.expectError(GL_NO_ERROR);

	ctx.beginSection("GL_INVALID_ENUM is generated if mode is not an accepted value.");
	ctx.glDrawElementsInstanced(-1, 1, GL_UNSIGNED_BYTE, vertices, 1);
	ctx.expectError(GL_INVALID_ENUM);
	ctx.endSection();

	ctx.beginSection("GL_INVALID_ENUM is generated if type is not one of the accepted values.");
	ctx.glDrawElementsInstanced(GL_POINTS, 1, -1, vertices, 1);
	ctx.expectError(GL_INVALID_ENUM);
	ctx.glDrawElementsInstanced(GL_POINTS, 1, GL_FLOAT, vertices, 1);
	ctx.expectError(GL_INVALID_ENUM);
	ctx.endSection();

	ctx.beginSection("GL_INVALID_VALUE is generated if count or primcount are negative.");
	ctx.glDrawElementsInstanced(GL_POINTS, -1, GL_UNSIGNED_BYTE, vertices, 1);
	ctx.expectError(GL_INVALID_VALUE);
	ctx.glDrawElementsInstanced(GL_POINTS, 11, GL_UNSIGNED_BYTE, vertices, -1);
	ctx.expectError(GL_INVALID_VALUE);
	ctx.endSection();

	ctx.beginSection("GL_INVALID_FRAMEBUFFER_OPERATION is generated if the currently bound framebuffer is not framebuffer complete.");
	ctx.glGenFramebuffers(1, &fbo);
	ctx.glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	ctx.glCheckFramebufferStatus(GL_FRAMEBUFFER);
	ctx.glDrawElementsInstanced(GL_POINTS, 1, GL_UNSIGNED_BYTE, vertices, 1);
	ctx.expectError(GL_INVALID_FRAMEBUFFER_OPERATION);
	ctx.glBindFramebuffer(GL_FRAMEBUFFER, 0);
	ctx.glDeleteFramebuffers(1, &fbo);
	ctx.endSection();
}

void draw_elements_instanced_incomplete_primitive (NegativeTestContext& ctx)
{
	glu::ShaderProgram program(ctx.getRenderContext(), glu::makeVtxFragSources(vertexShaderSource, fragmentShaderSource));
	ctx.glUseProgram(program.getProgram());
	GLuint fbo = 0;
	GLuint buf = 0;
	GLuint tfID = 0;
	GLfloat vertices[1];
	ctx.glVertexAttribDivisor(0, 1);
	ctx.expectError(GL_NO_ERROR);

	ctx.beginSection("GL_INVALID_ENUM is generated if mode is not an accepted value.");
	ctx.glDrawElementsInstanced(-1, 1, GL_UNSIGNED_BYTE, vertices, 1);
	ctx.expectError(GL_INVALID_ENUM);
	ctx.endSection();

	ctx.beginSection("GL_INVALID_ENUM is generated if type is not one of the accepted values.");
	ctx.glDrawElementsInstanced(GL_TRIANGLES, 1, -1, vertices, 1);
	ctx.expectError(GL_INVALID_ENUM);
	ctx.glDrawElementsInstanced(GL_TRIANGLES, 1, GL_FLOAT, vertices, 1);
	ctx.expectError(GL_INVALID_ENUM);
	ctx.endSection();

	ctx.beginSection("GL_INVALID_VALUE is generated if count or primcount are negative.");
	ctx.glDrawElementsInstanced(GL_TRIANGLES, -1, GL_UNSIGNED_BYTE, vertices, 1);
	ctx.expectError(GL_INVALID_VALUE);
	ctx.glDrawElementsInstanced(GL_TRIANGLES, 11, GL_UNSIGNED_BYTE, vertices, -1);
	ctx.expectError(GL_INVALID_VALUE);
	ctx.endSection();

	ctx.beginSection("GL_INVALID_FRAMEBUFFER_OPERATION is generated if the currently bound framebuffer is not framebuffer complete.");
	ctx.glGenFramebuffers(1, &fbo);
	ctx.glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	ctx.glCheckFramebufferStatus(GL_FRAMEBUFFER);
	ctx.glDrawElementsInstanced(GL_TRIANGLES, 1, GL_UNSIGNED_BYTE, vertices, 1);
	ctx.expectError(GL_INVALID_FRAMEBUFFER_OPERATION);
	ctx.glBindFramebuffer(GL_FRAMEBUFFER, 0);
	ctx.glDeleteFramebuffers(1, &fbo);
	ctx.endSection();

	if (!ctx.getContextInfo().isExtensionSupported("GL_EXT_geometry_shader")) // GL_EXT_geometry_shader removes error
	{
		ctx.beginSection("GL_INVALID_OPERATION is generated if transform feedback is active and not paused.");
		const char* tfVarying		= "gl_Position";

		ctx.glGenBuffers				(1, &buf);
		ctx.glGenTransformFeedbacks		(1, &tfID);

		ctx.glUseProgram				(program.getProgram());
		ctx.glTransformFeedbackVaryings	(program.getProgram(), 1, &tfVarying, GL_INTERLEAVED_ATTRIBS);
		ctx.glLinkProgram				(program.getProgram());
		ctx.glBindTransformFeedback		(GL_TRANSFORM_FEEDBACK, tfID);
		ctx.glBindBuffer				(GL_TRANSFORM_FEEDBACK_BUFFER, buf);
		ctx.glBufferData				(GL_TRANSFORM_FEEDBACK_BUFFER, 32, DE_NULL, GL_DYNAMIC_DRAW);
		ctx.glBindBufferBase			(GL_TRANSFORM_FEEDBACK_BUFFER, 0, buf);
		ctx.glBeginTransformFeedback	(GL_TRIANGLES);
		ctx.expectError					(GL_NO_ERROR);

		ctx.glDrawElementsInstanced		(GL_TRIANGLES, 1, GL_UNSIGNED_BYTE, vertices, 1);
		ctx.expectError					(GL_INVALID_OPERATION);

		ctx.glPauseTransformFeedback();
		ctx.glDrawElementsInstanced		(GL_TRIANGLES, 1, GL_UNSIGNED_BYTE, vertices, 1);
		ctx.expectError					(GL_NO_ERROR);

		ctx.glEndTransformFeedback		();
		ctx.glDeleteBuffers				(1, &buf);
		ctx.glDeleteTransformFeedbacks	(1, &tfID);
		ctx.expectError					(GL_NO_ERROR);
		ctx.endSection();
	}

	ctx.glUseProgram(0);
}

void draw_range_elements (NegativeTestContext& ctx)
{
	glu::ShaderProgram program(ctx.getRenderContext(), glu::makeVtxFragSources(vertexShaderSource, fragmentShaderSource));
	ctx.glUseProgram(program.getProgram());
	GLuint fbo = 0;
	GLuint buf = 0;
	GLuint tfID = 0;
	GLfloat vertices[1];

	ctx.beginSection("GL_INVALID_ENUM is generated if mode is not an accepted value.");
	ctx.glDrawRangeElements(-1, 0, 1, 1, GL_UNSIGNED_BYTE, vertices);
	ctx.expectError(GL_INVALID_ENUM);
	ctx.endSection();

	ctx.beginSection("GL_INVALID_ENUM is generated if type is not one of the accepted values.");
	ctx.glDrawRangeElements(GL_POINTS, 0, 1, 1, -1, vertices);
	ctx.expectError(GL_INVALID_ENUM);
	ctx.glDrawRangeElements(GL_POINTS, 0, 1, 1, GL_FLOAT, vertices);
	ctx.expectError(GL_INVALID_ENUM);
	ctx.endSection();

	ctx.beginSection("GL_INVALID_VALUE is generated if count is negative.");
	ctx.glDrawRangeElements(GL_POINTS, 0, 1, -1, GL_UNSIGNED_BYTE, vertices);
	ctx.expectError(GL_INVALID_VALUE);
	ctx.endSection();

	ctx.beginSection("GL_INVALID_VALUE is generated if end < start.");
	ctx.glDrawRangeElements(GL_POINTS, 1, 0, 1, GL_UNSIGNED_BYTE, vertices);
	ctx.expectError(GL_INVALID_VALUE);
	ctx.endSection();

	ctx.beginSection("GL_INVALID_FRAMEBUFFER_OPERATION is generated if the currently bound framebuffer is not framebuffer complete.");
	ctx.glGenFramebuffers(1, &fbo);
	ctx.glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	ctx.glCheckFramebufferStatus(GL_FRAMEBUFFER);
	ctx.glDrawRangeElements(GL_POINTS, 0, 1, 1, GL_UNSIGNED_BYTE, vertices);
	ctx.expectError(GL_INVALID_FRAMEBUFFER_OPERATION);
	ctx.glBindFramebuffer(GL_FRAMEBUFFER, 0);
	ctx.glDeleteFramebuffers(1, &fbo);
	ctx.endSection();

	if (!ctx.getContextInfo().isExtensionSupported("GL_EXT_geometry_shader")) // GL_EXT_geometry_shader removes error
	{
		ctx.beginSection("GL_INVALID_OPERATION is generated if transform feedback is active and not paused.");
		const char* tfVarying		= "gl_Position";

		ctx.glGenBuffers				(1, &buf);
		ctx.glGenTransformFeedbacks		(1, &tfID);

		ctx.glUseProgram				(program.getProgram());
		ctx.glTransformFeedbackVaryings	(program.getProgram(), 1, &tfVarying, GL_INTERLEAVED_ATTRIBS);
		ctx.glLinkProgram				(program.getProgram());
		ctx.glBindTransformFeedback		(GL_TRANSFORM_FEEDBACK, tfID);
		ctx.glBindBuffer				(GL_TRANSFORM_FEEDBACK_BUFFER, buf);
		ctx.glBufferData				(GL_TRANSFORM_FEEDBACK_BUFFER, 32, DE_NULL, GL_DYNAMIC_DRAW);
		ctx.glBindBufferBase			(GL_TRANSFORM_FEEDBACK_BUFFER, 0, buf);
		ctx.glBeginTransformFeedback	(GL_POINTS);
		ctx.expectError					(GL_NO_ERROR);

		ctx.glDrawRangeElements			(GL_POINTS, 0, 1, 1, GL_UNSIGNED_BYTE, vertices);
		ctx.expectError					(GL_INVALID_OPERATION);

		ctx.glPauseTransformFeedback();
		ctx.glDrawRangeElements			(GL_POINTS, 0, 1, 1, GL_UNSIGNED_BYTE, vertices);
		ctx.expectError					(GL_NO_ERROR);

		ctx.glEndTransformFeedback		();
		ctx.glDeleteBuffers				(1, &buf);
		ctx.glDeleteTransformFeedbacks	(1, &tfID);
		ctx.expectError					(GL_NO_ERROR);
		ctx.endSection();
	}

	ctx.glUseProgram(0);
}

void draw_range_elements_invalid_program (NegativeTestContext& ctx)
{
	ctx.glUseProgram(0);
	GLuint fbo = 0;
	GLfloat vertices[1];

	ctx.beginSection("GL_INVALID_ENUM is generated if mode is not an accepted value.");
	ctx.glDrawRangeElements(-1, 0, 1, 1, GL_UNSIGNED_BYTE, vertices);
	ctx.expectError(GL_INVALID_ENUM);
	ctx.endSection();

	ctx.beginSection("GL_INVALID_ENUM is generated if type is not one of the accepted values.");
	ctx.glDrawRangeElements(GL_POINTS, 0, 1, 1, -1, vertices);
	ctx.expectError(GL_INVALID_ENUM);
	ctx.glDrawRangeElements(GL_POINTS, 0, 1, 1, GL_FLOAT, vertices);
	ctx.expectError(GL_INVALID_ENUM);
	ctx.endSection();

	ctx.beginSection("GL_INVALID_VALUE is generated if count is negative.");
	ctx.glDrawRangeElements(GL_POINTS, 0, 1, -1, GL_UNSIGNED_BYTE, vertices);
	ctx.expectError(GL_INVALID_VALUE);
	ctx.endSection();

	ctx.beginSection("GL_INVALID_VALUE is generated if end < start.");
	ctx.glDrawRangeElements(GL_POINTS, 1, 0, 1, GL_UNSIGNED_BYTE, vertices);
	ctx.expectError(GL_INVALID_VALUE);
	ctx.endSection();

	ctx.beginSection("GL_INVALID_FRAMEBUFFER_OPERATION is generated if the currently bound framebuffer is not framebuffer complete.");
	ctx.glGenFramebuffers(1, &fbo);
	ctx.glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	ctx.glCheckFramebufferStatus(GL_FRAMEBUFFER);
	ctx.glDrawRangeElements(GL_POINTS, 0, 1, 1, GL_UNSIGNED_BYTE, vertices);
	ctx.expectError(GL_INVALID_FRAMEBUFFER_OPERATION);
	ctx.glBindFramebuffer(GL_FRAMEBUFFER, 0);
	ctx.glDeleteFramebuffers(1, &fbo);
	ctx.endSection();
}

void draw_range_elements_incomplete_primitive (NegativeTestContext& ctx)
{
	glu::ShaderProgram program(ctx.getRenderContext(), glu::makeVtxFragSources(vertexShaderSource, fragmentShaderSource));
	ctx.glUseProgram(program.getProgram());
	GLuint fbo = 0;
	GLuint buf = 0;
	GLuint tfID = 0;
	GLfloat vertices[1];

	ctx.beginSection("GL_INVALID_ENUM is generated if mode is not an accepted value.");
	ctx.glDrawRangeElements(-1, 0, 1, 1, GL_UNSIGNED_BYTE, vertices);
	ctx.expectError(GL_INVALID_ENUM);
	ctx.endSection();

	ctx.beginSection("GL_INVALID_ENUM is generated if type is not one of the accepted values.");
	ctx.glDrawRangeElements(GL_TRIANGLES, 0, 1, 1, -1, vertices);
	ctx.expectError(GL_INVALID_ENUM);
	ctx.glDrawRangeElements(GL_TRIANGLES, 0, 1, 1, GL_FLOAT, vertices);
	ctx.expectError(GL_INVALID_ENUM);
	ctx.endSection();

	ctx.beginSection("GL_INVALID_VALUE is generated if count is negative.");
	ctx.glDrawRangeElements(GL_TRIANGLES, 0, 1, -1, GL_UNSIGNED_BYTE, vertices);
	ctx.expectError(GL_INVALID_VALUE);
	ctx.endSection();

	ctx.beginSection("GL_INVALID_VALUE is generated if end < start.");
	ctx.glDrawRangeElements(GL_TRIANGLES, 1, 0, 1, GL_UNSIGNED_BYTE, vertices);
	ctx.expectError(GL_INVALID_VALUE);
	ctx.endSection();

	ctx.beginSection("GL_INVALID_FRAMEBUFFER_OPERATION is generated if the currently bound framebuffer is not framebuffer complete.");
	ctx.glGenFramebuffers(1, &fbo);
	ctx.glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	ctx.glCheckFramebufferStatus(GL_FRAMEBUFFER);
	ctx.glDrawRangeElements(GL_TRIANGLES, 0, 1, 1, GL_UNSIGNED_BYTE, vertices);
	ctx.expectError(GL_INVALID_FRAMEBUFFER_OPERATION);
	ctx.glBindFramebuffer(GL_FRAMEBUFFER, 0);
	ctx.glDeleteFramebuffers(1, &fbo);
	ctx.endSection();

	if (!ctx.getContextInfo().isExtensionSupported("GL_EXT_geometry_shader")) // GL_EXT_geometry_shader removes error
	{
		ctx.beginSection("GL_INVALID_OPERATION is generated if transform feedback is active and not paused.");
		const char* tfVarying		= "gl_Position";

		ctx.glGenBuffers				(1, &buf);
		ctx.glGenTransformFeedbacks		(1, &tfID);

		ctx.glUseProgram				(program.getProgram());
		ctx.glTransformFeedbackVaryings	(program.getProgram(), 1, &tfVarying, GL_INTERLEAVED_ATTRIBS);
		ctx.glLinkProgram				(program.getProgram());
		ctx.glBindTransformFeedback		(GL_TRANSFORM_FEEDBACK, tfID);
		ctx.glBindBuffer				(GL_TRANSFORM_FEEDBACK_BUFFER, buf);
		ctx.glBufferData				(GL_TRANSFORM_FEEDBACK_BUFFER, 32, DE_NULL, GL_DYNAMIC_DRAW);
		ctx.glBindBufferBase			(GL_TRANSFORM_FEEDBACK_BUFFER, 0, buf);
		ctx.glBeginTransformFeedback	(GL_TRIANGLES);
		ctx.expectError				(GL_NO_ERROR);

		ctx.glDrawRangeElements			(GL_TRIANGLES, 0, 1, 1, GL_UNSIGNED_BYTE, vertices);
		ctx.expectError				(GL_INVALID_OPERATION);

		ctx.glPauseTransformFeedback();
		ctx.glDrawRangeElements			(GL_TRIANGLES, 0, 1, 1, GL_UNSIGNED_BYTE, vertices);
		ctx.expectError				(GL_NO_ERROR);

		ctx.glEndTransformFeedback		();
		ctx.glDeleteBuffers				(1, &buf);
		ctx.glDeleteTransformFeedbacks	(1, &tfID);
		ctx.expectError				(GL_NO_ERROR);
		ctx.endSection();
	}

	ctx.glUseProgram(0);
}

std::vector<FunctionContainer> getNegativeVertexArrayApiTestFunctions ()
{
	FunctionContainer funcs[] =
	{
		{vertex_attribf,								"vertex_attribf",								"Invalid glVertexAttrib{1234}f() usage"		},
		{vertex_attribfv,								"vertex_attribfv",								"Invalid glVertexAttrib{1234}fv() usage"	},
		{vertex_attribi4,								"vertex_attribi4",								"Invalid glVertexAttribI4{i|ui}f() usage"	},
		{vertex_attribi4v,								"vertex_attribi4v",								"Invalid glVertexAttribI4{i|ui}fv() usage"	},
		{vertex_attrib_pointer,							"vertex_attrib_pointer",						"Invalid glVertexAttribPointer() usage"		},
		{vertex_attrib_i_pointer,						"vertex_attrib_i_pointer",						"Invalid glVertexAttribPointer() usage"		},
		{enable_vertex_attrib_array,					"enable_vertex_attrib_array",					"Invalid glEnableVertexAttribArray() usage"	},
		{disable_vertex_attrib_array,					"disable_vertex_attrib_array",					"Invalid glDisableVertexAttribArray() usage"},
		{gen_vertex_arrays,								"gen_vertex_arrays",							"Invalid glGenVertexArrays() usage"			},
		{bind_vertex_array,								"bind_vertex_array",							"Invalid glBindVertexArray() usage"			},
		{delete_vertex_arrays,							"delete_vertex_arrays",							"Invalid glDeleteVertexArrays() usage"		},
		{vertex_attrib_divisor,							"vertex_attrib_divisor",						"Invalid glVertexAttribDivisor() usage"		},
		{draw_arrays,									"draw_arrays",									"Invalid glDrawArrays() usage"				},
		{draw_arrays_invalid_program,					"draw_arrays_invalid_program",					"Invalid glDrawArrays() usage"				},
		{draw_arrays_incomplete_primitive,				"draw_arrays_incomplete_primitive",				"Invalid glDrawArrays() usage"				},
		{draw_elements,									"draw_elements",								"Invalid glDrawElements() usage"			},
		{draw_elements_invalid_program,					"draw_elements_invalid_program",				"Invalid glDrawElements() usage"			},
		{draw_elements_incomplete_primitive,			"draw_elements_incomplete_primitive",			"Invalid glDrawElements() usage"			},
		{draw_arrays_instanced,							"draw_arrays_instanced",						"Invalid glDrawArraysInstanced() usage"		},
		{draw_arrays_instanced_invalid_program,			"draw_arrays_instanced_invalid_program",		"Invalid glDrawArraysInstanced() usage"		},
		{draw_arrays_instanced_incomplete_primitive,	"draw_arrays_instanced_incomplete_primitive",	"Invalid glDrawArraysInstanced() usage"		},
		{draw_elements_instanced,						"draw_elements_instanced",						"Invalid glDrawElementsInstanced() usage"	},
		{draw_elements_instanced_invalid_program,		"draw_elements_instanced_invalid_program",		"Invalid glDrawElementsInstanced() usage"	},
		{draw_elements_instanced_incomplete_primitive,	"draw_elements_instanced_incomplete_primitive",	"Invalid glDrawElementsInstanced() usage"	},
		{draw_range_elements,							"draw_range_elements",							"Invalid glDrawRangeElements() usage"		},
		{draw_range_elements_invalid_program,			"draw_range_elements_invalid_program",			"Invalid glDrawRangeElements() usage"		},
		{draw_range_elements_incomplete_primitive,		"draw_range_elements_incomplete_primitive",		"Invalid glDrawRangeElements() usage"		},
	};

	return std::vector<FunctionContainer>(DE_ARRAY_BEGIN(funcs), DE_ARRAY_END(funcs));
}

} // NegativeTestShared
} // Functional
} // gles31
} // deqp
