/*-------------------------------------------------------------------------
 * OpenGL Conformance Test Suite
 * -----------------------------
 *
 * Copyright (c) 2014-2016 The Khronos Group Inc.
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
 * \brief
 *//*--------------------------------------------------------------------*/

#include "gl4cShaderAtomicCountersTests.hpp"
#include "gluContextInfo.hpp"
#include "gluPixelTransfer.hpp"
#include "glwEnums.hpp"
#include "glwFunctions.hpp"
#include "tcuImageCompare.hpp"
#include "tcuRenderTarget.hpp"
#include "tcuSurface.hpp"
#include "tcuVector.hpp"
#include <assert.h>
#include <cstdarg>
#include <map>

namespace gl4cts
{

using namespace glw;
using tcu::Vec4;
using tcu::UVec4;

namespace
{

static tcu::TestLog* currentLog;

void setOutput(tcu::TestLog& log)
{
	currentLog = &log;
}

void Output(const char* format, ...)
{
	va_list args;
	va_start(args, format);

	const int   MAX_OUTPUT_STRING_SIZE = 40000;
	static char temp[MAX_OUTPUT_STRING_SIZE];

	vsnprintf(temp, MAX_OUTPUT_STRING_SIZE - 1, format, args);
	temp[MAX_OUTPUT_STRING_SIZE - 1] = '\0';

	char* logLine = strtok(temp, "\n");
	while (logLine != NULL)
	{
		currentLog->writeMessage(logLine);
		logLine = strtok(NULL, "\n");
	}
	va_end(args);
}

class SACSubcaseBase : public deqp::SubcaseBase
{
public:
	virtual std::string Title()
	{
		return NL "";
	}

	virtual std::string Purpose()
	{
		return NL "";
	}

	virtual std::string Method()
	{
		return NL "";
	}

	virtual std::string PassCriteria()
	{
		return NL "";
	}

	virtual ~SACSubcaseBase()
	{
	}

	int getWindowWidth()
	{
		const tcu::RenderTarget& renderTarget = m_context.getRenderContext().getRenderTarget();
		return renderTarget.getWidth();
	}

	int getWindowHeight()
	{
		const tcu::RenderTarget& renderTarget = m_context.getRenderContext().getRenderTarget();
		return renderTarget.getHeight();
	}

	long ValidateReadBuffer(const Vec4& expected)
	{
		const tcu::RenderTarget& renderTarget = m_context.getRenderContext().getRenderTarget();
		int						 viewportW	= renderTarget.getWidth();
		int						 viewportH	= renderTarget.getHeight();
		tcu::Surface			 renderedFrame(viewportW, viewportH);
		tcu::Surface			 referenceFrame(viewportW, viewportH);

		glu::readPixels(m_context.getRenderContext(), 0, 0, renderedFrame.getAccess());

		for (int y = 0; y < viewportH; ++y)
		{
			for (int x = 0; x < viewportW; ++x)
			{
				referenceFrame.setPixel(
					x, y, tcu::RGBA(static_cast<int>(expected[0] * 255), static_cast<int>(expected[1] * 255),
									static_cast<int>(expected[2] * 255), static_cast<int>(expected[3] * 255)));
			}
		}
		tcu::TestLog& log = m_context.getTestContext().getLog();
		bool isOk = tcu::fuzzyCompare(log, "Result", "Image comparison result", referenceFrame, renderedFrame, 0.05f,
									  tcu::COMPARE_LOG_RESULT);
		return (isOk ? NO_ERROR : ERROR);
	}

	void LinkProgram(GLuint program)
	{
		glLinkProgram(program);
		GLsizei length;
		GLchar  log[1024];
		glGetProgramInfoLog(program, sizeof(log), &length, log);
		if (length > 1)
		{
			Output("Program Info Log:\n%s\n", log);
		}
	}

	GLuint CreateProgram(const char* src_vs, const char* src_tcs, const char* src_tes, const char* src_gs,
						 const char* src_fs, bool link)
	{
		const GLuint p = glCreateProgram();

		if (src_vs)
		{
			GLuint sh = glCreateShader(GL_VERTEX_SHADER);
			glAttachShader(p, sh);
			glDeleteShader(sh);
			glShaderSource(sh, 1, &src_vs, NULL);
			glCompileShader(sh);
		}
		if (src_tcs)
		{
			GLuint sh = glCreateShader(GL_TESS_CONTROL_SHADER);
			glAttachShader(p, sh);
			glDeleteShader(sh);
			glShaderSource(sh, 1, &src_tcs, NULL);
			glCompileShader(sh);
		}
		if (src_tes)
		{
			GLuint sh = glCreateShader(GL_TESS_EVALUATION_SHADER);
			glAttachShader(p, sh);
			glDeleteShader(sh);
			glShaderSource(sh, 1, &src_tes, NULL);
			glCompileShader(sh);
		}
		if (src_gs)
		{
			GLuint sh = glCreateShader(GL_GEOMETRY_SHADER);
			glAttachShader(p, sh);
			glDeleteShader(sh);
			glShaderSource(sh, 1, &src_gs, NULL);
			glCompileShader(sh);
		}
		if (src_fs)
		{
			GLuint sh = glCreateShader(GL_FRAGMENT_SHADER);
			glAttachShader(p, sh);
			glDeleteShader(sh);
			glShaderSource(sh, 1, &src_fs, NULL);
			glCompileShader(sh);
		}
		if (link)
		{
			LinkProgram(p);
		}
		return p;
	}

	bool CheckProgram(GLuint program)
	{
		GLint status;
		glGetProgramiv(program, GL_LINK_STATUS, &status);
		GLint length;
		glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
		if (length > 1)
		{
			std::vector<GLchar> log(length);
			glGetProgramInfoLog(program, length, NULL, &log[0]);
			Output("%s\n", &log[0]);
		}
		return status == GL_TRUE;
	}

	GLuint CreateShaderProgram(GLenum type, GLsizei count, const GLchar** strings)
	{
		GLuint program = glCreateShaderProgramv(type, count, strings);
		GLint  status  = GL_TRUE;
		glGetProgramiv(program, GL_LINK_STATUS, &status);
		if (status == GL_FALSE)
		{
			GLsizei length;
			GLchar  log[1024];
			glGetProgramInfoLog(program, sizeof(log), &length, log);
			if (length > 1)
			{
				Output("Program Info Log:\n%s\n", log);
			}
		}
		return program;
	}

	void CreateQuad(GLuint* vao, GLuint* vbo, GLuint* ebo)
	{
		assert(vao && vbo);

		// interleaved data (vertex, color0 (green), color1 (blue), color2 (red))
		const float v[] = {
			-1.0f, -1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f,  1.0f, 0.0f, 0.0f, 1.0f, -1.0f, 0.0f, 1.0f,
			0.0f,  0.0f,  0.0f, 1.0f, 1.0f, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f,  0.0f, 1.0f,
			1.0f,  0.0f,  0.0f, 1.0f, 1.0f, 0.0f, 1.0f, 0.0f,  0.0f, 0.0f, 1.0f, 1.0f, 0.0f,  0.0f,
		};
		glGenBuffers(1, vbo);
		glBindBuffer(GL_ARRAY_BUFFER, *vbo);
		glBufferData(GL_ARRAY_BUFFER, sizeof(v), v, GL_STATIC_DRAW);
		glBindBuffer(GL_ARRAY_BUFFER, 0);

		if (ebo)
		{
			std::vector<GLushort> index_data(4);
			for (int i = 0; i < 4; ++i)
			{
				index_data[i] = static_cast<GLushort>(i);
			}
			glGenBuffers(1, ebo);
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, *ebo);
			glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(GLushort) * 4, &index_data[0], GL_STATIC_DRAW);
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
		}

		glGenVertexArrays(1, vao);
		glBindVertexArray(*vao);
		glBindBuffer(GL_ARRAY_BUFFER, *vbo);
		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 11, 0);

		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 11, reinterpret_cast<void*>(sizeof(float) * 2));

		glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 11, reinterpret_cast<void*>(sizeof(float) * 5));

		glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 11, reinterpret_cast<void*>(sizeof(float) * 8));

		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glEnableVertexAttribArray(0);
		glEnableVertexAttribArray(1);
		glEnableVertexAttribArray(2);
		glEnableVertexAttribArray(3);
		if (ebo)
		{
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, *ebo);
		}
		glBindVertexArray(0);
	}

	void CreateTriangle(GLuint* vao, GLuint* vbo, GLuint* ebo)
	{
		assert(vao && vbo);

		// interleaved data (vertex, color0 (green), color1 (blue), color2 (red))
		const float v[] = {
			-1.0f, -1.0f, 0.0f, 1.0f, 0.0f, 0.0f,  0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 3.0f, -1.0f, 0.0f, 1.0f, 0.0f, 0.0f,
			0.0f,  1.0f,  1.0f, 0.0f, 0.0f, -1.0f, 3.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f,  1.0f, 0.0f, 0.0f,
		};
		glGenBuffers(1, vbo);
		glBindBuffer(GL_ARRAY_BUFFER, *vbo);
		glBufferData(GL_ARRAY_BUFFER, sizeof(v), v, GL_STATIC_DRAW);
		glBindBuffer(GL_ARRAY_BUFFER, 0);

		if (ebo)
		{
			std::vector<GLushort> index_data(3);
			for (int i = 0; i < 3; ++i)
			{
				index_data[i] = static_cast<GLushort>(i);
			}
			glGenBuffers(1, ebo);
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, *ebo);
			glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(GLushort) * 4, &index_data[0], GL_STATIC_DRAW);
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
		}

		glGenVertexArrays(1, vao);
		glBindVertexArray(*vao);
		glBindBuffer(GL_ARRAY_BUFFER, *vbo);
		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 11, 0);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 11, reinterpret_cast<void*>(sizeof(float) * 2));
		glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 11, reinterpret_cast<void*>(sizeof(float) * 5));
		glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 11, reinterpret_cast<void*>(sizeof(float) * 8));
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glEnableVertexAttribArray(0);
		glEnableVertexAttribArray(1);
		glEnableVertexAttribArray(2);
		glEnableVertexAttribArray(3);
		if (ebo)
		{
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, *ebo);
		}
		glBindVertexArray(0);
	}

	const char* GLenumToString(GLenum e)
	{
		switch (e)
		{
		case GL_ATOMIC_COUNTER_BUFFER_BINDING:
			return "GL_ATOMIC_COUNTER_BUFFER_BINDING";
		case GL_MAX_VERTEX_ATOMIC_COUNTER_BUFFERS:
			return "GL_MAX_VERTEX_ATOMIC_COUNTER_BUFFERS";
		case GL_MAX_TESS_CONTROL_ATOMIC_COUNTER_BUFFERS:
			return "GL_MAX_TESS_CONTROL_ATOMIC_COUNTER_BUFFERS";
		case GL_MAX_TESS_EVALUATION_ATOMIC_COUNTER_BUFFERS:
			return "GL_MAX_TESS_EVALUATION_ATOMIC_COUNTER_BUFFERS";
		case GL_MAX_GEOMETRY_ATOMIC_COUNTER_BUFFERS:
			return "GL_MAX_GEOMETRY_ATOMIC_COUNTER_BUFFERS";
		case GL_MAX_FRAGMENT_ATOMIC_COUNTER_BUFFERS:
			return "GL_MAX_FRAGMENT_ATOMIC_COUNTER_BUFFERS";
		case GL_MAX_COMBINED_ATOMIC_COUNTER_BUFFERS:
			return "GL_MAX_COMBINED_ATOMIC_COUNTER_BUFFERS";

		case GL_MAX_VERTEX_ATOMIC_COUNTERS:
			return "GL_MAX_VERTEX_ATOMIC_COUNTERS";
		case GL_MAX_TESS_CONTROL_ATOMIC_COUNTERS:
			return "GL_MAX_TESS_CONTROL_ATOMIC_COUNTERS";
		case GL_MAX_TESS_EVALUATION_ATOMIC_COUNTERS:
			return "GL_MAX_TESS_EVALUATION_ATOMIC_COUNTERS";
		case GL_MAX_GEOMETRY_ATOMIC_COUNTERS:
			return "GL_MAX_GEOMETRY_ATOMIC_COUNTERS";
		case GL_MAX_FRAGMENT_ATOMIC_COUNTERS:
			return "GL_MAX_FRAGMENT_ATOMIC_COUNTERS";
		case GL_MAX_COMBINED_ATOMIC_COUNTERS:
			return "GL_MAX_COMBINED_ATOMIC_COUNTERS";

		case GL_MAX_ATOMIC_COUNTER_BUFFER_SIZE:
			return "GL_MAX_ATOMIC_COUNTER_BUFFER_SIZE";
		case GL_MAX_ATOMIC_COUNTER_BUFFER_BINDINGS:
			return "GL_MAX_ATOMIC_COUNTER_BUFFER_BINDINGS";

		default:
			assert(0);
			break;
		}
		return NULL;
	}

	bool CheckMaxValue(GLenum e, GLint expected)
	{
		bool ok = true;

		GLint i;
		glGetIntegerv(e, &i);
		Output("%s = %d\n", GLenumToString(e), i);
		if (i < expected)
		{
			ok = false;
			Output("%s state is incorrect (GetIntegerv, is: %d, expected: %d)\n", GLenumToString(e), i, expected);
		}

		GLint64 i64;
		glGetInteger64v(e, &i64);
		if (i64 < static_cast<GLint64>(expected))
		{
			ok = false;
			Output("%s state is incorrect (GetInteger64v, is: %d, expected: %d)\n", GLenumToString(e),
				   static_cast<GLint>(i64), expected);
		}

		GLfloat f;
		glGetFloatv(e, &f);
		if (f < static_cast<GLfloat>(expected))
		{
			ok = false;
			Output("%s state is incorrect (GetFloatv, is: %f, expected: %d)\n", GLenumToString(e), f, expected);
		}

		GLdouble d;
		glGetDoublev(e, &d);
		if (d < static_cast<GLdouble>(expected))
		{
			ok = false;
			Output("%s state is incorrect (GetDoublev, is: %f, expected: %d)\n", GLenumToString(e), d, expected);
		}

		GLboolean b;
		glGetBooleanv(e, &b);

		return ok;
	}

	bool CheckGetCommands(GLenum e, GLint expected)
	{
		bool ok = true;

		GLint i;
		glGetIntegerv(e, &i);
		if (i != expected)
		{
			ok = false;
			Output("%s state is incorrect (GetIntegerv, is: %d, expected: %d)\n", GLenumToString(e), i, expected);
		}

		GLint64 i64;
		glGetInteger64v(e, &i64);
		if (i64 != static_cast<GLint64>(expected))
		{
			ok = false;
			Output("%s state is incorrect (GetInteger64v, is: %d, expected: %d)\n", GLenumToString(e),
				   static_cast<GLint>(i64), expected);
		}

		GLfloat f;
		glGetFloatv(e, &f);
		if (f != static_cast<GLfloat>(expected))
		{
			ok = false;
			Output("%s state is incorrect (GetFloatv, is: %f, expected: %d)\n", GLenumToString(e), f, expected);
		}

		GLdouble d;
		glGetDoublev(e, &d);
		if (d != static_cast<GLdouble>(expected))
		{
			ok = false;
			Output("%s state is incorrect (GetDoublev, is: %f, expected: %d)\n", GLenumToString(e), d, expected);
		}

		GLboolean b;
		glGetBooleanv(e, &b);
		if (b != (expected ? GL_TRUE : GL_FALSE))
		{
			ok = false;
			Output("%s state is incorrect (GetBooleanv, is: %d, expected: %d)\n", GLenumToString(e), b,
				   expected ? GL_TRUE : GL_FALSE);
		}

		return ok;
	}

	bool CheckBufferBindingState(GLuint index, GLint binding, GLint64 start, GLint64 size)
	{
		bool ok = true;

		GLint i;
		glGetIntegeri_v(GL_ATOMIC_COUNTER_BUFFER_BINDING, index, &i);
		if (i != binding)
		{
			ok = false;
			Output("GL_ATOMIC_COUNTER_BUFFER_BINDING state is incorrect (GetIntegeri_v, is: %d, expected: %d, index: "
				   "%u)\n",
				   i, binding, index);
		}

		GLint64 i64;
		glGetInteger64i_v(GL_ATOMIC_COUNTER_BUFFER_BINDING, index, &i64);
		if (i64 != static_cast<GLint64>(binding))
		{
			ok = false;
			Output("GL_ATOMIC_COUNTER_BUFFER_BINDING state is incorrect (GetInteger64i_v, is: %d, expected: %d, index: "
				   "%u)\n",
				   static_cast<GLint>(i64), binding, index);
		}

		GLfloat f;
		glGetFloati_v(GL_ATOMIC_COUNTER_BUFFER_BINDING, index, &f);
		if (f != static_cast<GLfloat>(binding))
		{
			ok = false;
			Output(
				"GL_ATOMIC_COUNTER_BUFFER_BINDING state is incorrect (GetFloati_v, is: %f, expected: %d, index: %u)\n",
				f, binding, index);
		}

		GLdouble d;
		glGetDoublei_v(GL_ATOMIC_COUNTER_BUFFER_BINDING, index, &d);
		if (d != static_cast<GLdouble>(binding))
		{
			ok = false;
			Output(
				"GL_ATOMIC_COUNTER_BUFFER_BINDING state is incorrect (GetDoublei_v, is: %f, expected: %d, index: %u)\n",
				d, binding, index);
		}

		GLboolean b;
		glGetBooleani_v(GL_ATOMIC_COUNTER_BUFFER_BINDING, index, &b);
		if (b != (binding ? GL_TRUE : GL_FALSE))
		{
			ok = false;
			Output("GL_ATOMIC_COUNTER_BUFFER_BINDING state is incorrect (GetBooleani_v, is: %d, expected: %d, index: "
				   "%u)\n",
				   b, binding ? GL_TRUE : GL_FALSE, index);
		}

		glGetInteger64i_v(GL_ATOMIC_COUNTER_BUFFER_START, index, &i64);
		if (i64 != start)
		{
			ok = false;
			Output("GL_ATOMIC_COUNTER_BUFFER_START state is incorrect (GetInteger64i_v, is: %d, expected: %d, index: "
				   "%u)\n",
				   static_cast<GLint>(i64), static_cast<GLint>(start), index);
		}
		glGetInteger64i_v(GL_ATOMIC_COUNTER_BUFFER_SIZE, index, &i64);
		if (i64 != size && i64 != 0)
		{
			ok = false;
			Output("GL_ATOMIC_COUNTER_BUFFER_SIZE state is incorrect (GetInteger64i_v, is: %d, expected: (%d or 0), "
				   "index: %u)\n",
				   static_cast<GLint>(i64), static_cast<GLint>(size), index);
		}

		return ok;
	}

	bool CheckUniform(GLuint prog, const GLchar* uniform_name, GLuint uniform_index, GLint uniform_type,
					  GLint uniform_size, GLint uniform_offset, GLint uniform_array_stride, GLuint buffer_index)
	{
		bool ok = true;

		GLuint index;
		glGetUniformIndices(prog, 1, &uniform_name, &index);
		if (index != uniform_index)
		{
			Output("Uniform: %s: Bad index returned by glGetUniformIndices.\n", uniform_name);
			ok = false;
		}

		const GLsizei uniform_length = static_cast<GLsizei>(strlen(uniform_name));

		GLsizei length;
		GLint   size;
		GLenum  type;
		GLchar  name[32];

		glGetActiveUniformName(prog, uniform_index, sizeof(name), &length, name);
		if (strcmp(name, uniform_name))
		{
			Output("Uniform: %s: Bad name returned by glGetActiveUniformName.\n", uniform_name);
			ok = false;
		}
		if (length != uniform_length)
		{
			Output("Uniform: %s: Length is %d should be %d.\n", uniform_name, length, uniform_length);
			ok = false;
		}

		glGetActiveUniform(prog, uniform_index, sizeof(name), &length, &size, &type, name);
		if (strcmp(name, uniform_name))
		{
			Output("Uniform: %s: Bad name returned by glGetActiveUniform.\n", uniform_name);
			ok = false;
		}
		if (length != uniform_length)
		{
			Output("Uniform: %s: Length is %d should be %d.\n", uniform_name, length, uniform_length);
			ok = false;
		}
		if (size != uniform_size)
		{
			Output("Uniform: %s: Size is %d should be %d.\n", uniform_name, size, uniform_size);
			ok = false;
		}
		if (type != static_cast<GLenum>(uniform_type))
		{
			Output("Uniform: %s: Type is %d should be %d.\n", uniform_name, type, uniform_type);
			ok = false;
		}

		GLint param;
		glGetActiveUniformsiv(prog, 1, &uniform_index, GL_UNIFORM_TYPE, &param);
		if (param != uniform_type)
		{
			Output("Uniform: %s: Type is %d should be %d.\n", uniform_name, param, uniform_type);
			ok = false;
		}
		glGetActiveUniformsiv(prog, 1, &uniform_index, GL_UNIFORM_SIZE, &param);
		if (param != uniform_size)
		{
			Output("Uniform: %s: GL_UNIFORM_SIZE is %d should be %d.\n", uniform_name, param, uniform_size);
			ok = false;
		}
		glGetActiveUniformsiv(prog, 1, &uniform_index, GL_UNIFORM_NAME_LENGTH, &param);
		if (param != (uniform_length + 1))
		{
			Output("Uniform: %s: GL_UNIFORM_NAME_LENGTH is %d should be %d.\n", uniform_name, param,
				   uniform_length + 1);
			ok = false;
		}
		glGetActiveUniformsiv(prog, 1, &uniform_index, GL_UNIFORM_BLOCK_INDEX, &param);
		if (param != -1)
		{
			Output("Uniform: %s: GL_UNIFORM_BLOCK_INDEX should be -1.\n", uniform_name);
			ok = false;
		}
		glGetActiveUniformsiv(prog, 1, &uniform_index, GL_UNIFORM_OFFSET, &param);
		if (param != uniform_offset)
		{
			Output("Uniform: %s: GL_UNIFORM_OFFSET is %d should be %d.\n", uniform_name, param, uniform_offset);
			ok = false;
		}
		glGetActiveUniformsiv(prog, 1, &uniform_index, GL_UNIFORM_ARRAY_STRIDE, &param);
		if (param != uniform_array_stride)
		{
			Output("Uniform: %s: GL_UNIFORM_ARRAY_STRIDE is %d should be %d.\n", uniform_name, param,
				   uniform_array_stride);
			ok = false;
		}
		glGetActiveUniformsiv(prog, 1, &uniform_index, GL_UNIFORM_MATRIX_STRIDE, &param);
		if (param != 0)
		{
			Output("Uniform: %s: GL_UNIFORM_MATRIX_STRIDE should be 0 is %d.\n", uniform_name, param);
			ok = false;
		}
		glGetActiveUniformsiv(prog, 1, &uniform_index, GL_UNIFORM_IS_ROW_MAJOR, &param);
		if (param != 0)
		{
			Output("Uniform: %s: GL_UNIFORM_IS_ROW_MAJOR should be 0 is %d.\n", uniform_name, param);
			ok = false;
		}
		glGetActiveUniformsiv(prog, 1, &uniform_index, GL_UNIFORM_ATOMIC_COUNTER_BUFFER_INDEX, &param);
		if (param != static_cast<GLint>(buffer_index))
		{
			Output("Uniform: %s: GL_UNIFORM_ATOMIC_COUNTER_BUFFER_INDEX is %d should be %d.\n", uniform_name, param,
				   buffer_index);
			ok = false;
		}

		return ok;
	}

	bool CheckCounterValues(GLuint size, GLuint* values, GLuint min_value)
	{
		std::sort(values, values + size);
		for (GLuint i = 0; i < size; ++i)
		{
			Output("%u\n", values[i]);
			if (values[i] != i + min_value)
			{
				Output("Counter value is %u should be %u.\n", values[i], i + min_value);
				return false;
			}
		}
		return true;
	}

	bool CheckFinalCounterValue(GLuint buffer, GLintptr offset, GLuint expected_value)
	{
		GLuint value;
		glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, buffer);
		glGetBufferSubData(GL_ATOMIC_COUNTER_BUFFER, offset, 4, &value);
		if (value != expected_value)
		{
			Output("Counter value is %u should be %u.\n", value, expected_value);
			return false;
		}
		return true;
	}
};

class Buffer : public deqp::GLWrapper
{
public:
	Buffer()
		: size_(0)
		, usage_(GL_STATIC_DRAW)
		, access_(GL_READ_WRITE)
		, access_flags_(0)
		, mapped_(GL_FALSE)
		, map_pointer_(NULL)
		, map_offset_(0)
		, map_length_(0)
	{
		glGenBuffers(1, &name_);
		glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, name_);
		glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, 0);
	}
	~Buffer()
	{
		glDeleteBuffers(1, &name_);
	}

	GLuint name() const
	{
		return name_;
	}

	long Verify()
	{
		GLint   i;
		GLint64 i64;

		glGetBufferParameteri64v(GL_ATOMIC_COUNTER_BUFFER, GL_BUFFER_SIZE, &i64);
		if (i64 != size_)
		{
			Output("BUFFER_SIZE is %d should be %d.\n", static_cast<GLint>(i64), static_cast<GLint>(size_));
			return ERROR;
		}
		glGetBufferParameteriv(GL_ATOMIC_COUNTER_BUFFER, GL_BUFFER_USAGE, &i);
		if (i != static_cast<GLint>(usage_))
		{
			Output("BUFFER_USAGE is %d should be %d.\n", i, usage_);
			return ERROR;
		}
		glGetBufferParameteriv(GL_ATOMIC_COUNTER_BUFFER, GL_BUFFER_ACCESS, &i);
		if (i != static_cast<GLint>(access_))
		{
			Output("BUFFER_ACCESS is %d should be %d.\n", i, access_);
			return ERROR;
		}
		glGetBufferParameteriv(GL_ATOMIC_COUNTER_BUFFER, GL_BUFFER_ACCESS_FLAGS, &i);
		if (i != access_flags_)
		{
			Output("BUFFER_ACCESS_FLAGS is %d should be %d.\n", i, access_flags_);
			return ERROR;
		}
		glGetBufferParameteriv(GL_ATOMIC_COUNTER_BUFFER, GL_BUFFER_MAPPED, &i);
		if (i != mapped_)
		{
			Output("BUFFER_MAPPED is %d should be %d.\n", i, mapped_);
			return ERROR;
		}
		glGetBufferParameteri64v(GL_ATOMIC_COUNTER_BUFFER, GL_BUFFER_MAP_OFFSET, &i64);
		if (i64 != map_offset_)
		{
			Output("BUFFER_MAP_OFFSET is %d should be %d.\n", static_cast<GLint>(i64), static_cast<GLint>(map_offset_));
			return ERROR;
		}
		glGetBufferParameteri64v(GL_ATOMIC_COUNTER_BUFFER, GL_BUFFER_MAP_LENGTH, &i64);
		if (i64 != map_length_)
		{
			Output("BUFFER_MAP_LENGTH is %d should be %d.\n", static_cast<GLint>(i64), static_cast<GLint>(map_length_));
			return ERROR;
		}

		void* ptr;
		glGetBufferPointerv(GL_ATOMIC_COUNTER_BUFFER, GL_BUFFER_MAP_POINTER, &ptr);
		if (ptr != map_pointer_)
		{
			Output("BUFFER_MAP_POINTER is %p should be %p.\n", ptr, map_pointer_);
			return ERROR;
		}
		return NO_ERROR;
	}

	void Data(GLsizeiptr size, const void* data, GLenum usage)
	{
		size_  = size;
		usage_ = usage;
		glBufferData(GL_ATOMIC_COUNTER_BUFFER, size, data, usage);
	}

	void* MapRange(GLintptr offset, GLsizeiptr length, GLbitfield access)
	{
		assert(mapped_ == GL_FALSE);

		map_pointer_ = glMapBufferRange(GL_ATOMIC_COUNTER_BUFFER, offset, length, access);
		if (map_pointer_)
		{
			map_offset_   = offset;
			map_length_   = length;
			access_flags_ = access;
			if ((access & GL_MAP_WRITE_BIT) && (access & GL_MAP_READ_BIT))
				access_ = GL_READ_WRITE;
			else if (access & GL_MAP_READ_BIT)
				access_ = GL_READ_ONLY;
			else if (access & GL_MAP_WRITE_BIT)
				access_ = GL_WRITE_ONLY;
			mapped_		= GL_TRUE;
		}
		return map_pointer_;
	}

	void* Map(GLenum access)
	{
		assert(mapped_ == GL_FALSE);

		map_pointer_ = glMapBuffer(GL_ATOMIC_COUNTER_BUFFER, access);
		if (map_pointer_)
		{
			mapped_ = GL_TRUE;
			access_ = access;
			if (access == GL_READ_WRITE)
				access_flags_ = GL_MAP_WRITE_BIT | GL_MAP_READ_BIT;
			else if (access == GL_READ_ONLY)
				access_flags_ = GL_MAP_READ_BIT;
			else if (access == GL_WRITE_ONLY)
				access_flags_ = GL_MAP_WRITE_BIT;
			map_offset_		  = 0;
			map_length_		  = size_;
		}
		return map_pointer_;
	}
	GLboolean Unmap()
	{
		assert(mapped_ == GL_TRUE);

		if (glUnmapBuffer(GL_ATOMIC_COUNTER_BUFFER))
		{
			map_offset_   = 0;
			map_length_   = 0;
			map_pointer_  = 0;
			mapped_		  = GL_FALSE;
			access_flags_ = 0;
			access_		  = GL_READ_WRITE;
			return GL_TRUE;
		}
		return GL_FALSE;
	}

private:
	GLuint	name_;
	GLint64   size_;
	GLenum	usage_;
	GLenum	access_;
	GLint	 access_flags_;
	GLboolean mapped_;
	void*	 map_pointer_;
	GLint64   map_offset_;
	GLint64   map_length_;
};
}

class BasicBufferOperations : public SACSubcaseBase
{
	virtual std::string Title()
	{
		return NL "Atomic Counter Buffer - basic operations";
	}

	virtual std::string Purpose()
	{
		return NL
			"Verify that basic buffer operations work as expected with new buffer target." NL
			"Tested commands: BindBuffer, BufferData, BufferSubData, MapBuffer, MapBufferRange, UnmapBuffer and" NL
			"GetBufferSubData.";
	}

	virtual std::string Method()
	{
		return NL "";
	}

	virtual std::string PassCriteria()
	{
		return NL "";
	}

	GLuint buffer_;

	virtual long Setup()
	{
		buffer_ = 0;
		return NO_ERROR;
	}

	virtual long Run()
	{
		glGenBuffers(1, &buffer_);
		glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, buffer_);
		glBufferData(GL_ATOMIC_COUNTER_BUFFER, 8 * 4, NULL, GL_STATIC_DRAW);
		glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, 0);

		glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, buffer_);
		GLuint* ptr = static_cast<GLuint*>(glMapBuffer(GL_ATOMIC_COUNTER_BUFFER, GL_WRITE_ONLY));
		for (GLuint i = 0; i < 8; ++i)
			ptr[i]	= i;
		glUnmapBuffer(GL_ATOMIC_COUNTER_BUFFER);
		glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, 0);

		long   res = NO_ERROR;
		GLuint data[8];
		glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, buffer_);
		glGetBufferSubData(GL_ATOMIC_COUNTER_BUFFER, 0, sizeof(data), data);
		glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, 0);
		for (GLuint i = 0; i < 8; ++i)
		{
			if (data[i] != i)
			{
				Output("data[%u] is: %u should be: %u\n", i, data[i], i);
				res = ERROR;
			}
		}
		if (res != NO_ERROR)
			return res;

		glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, buffer_);
		ptr = static_cast<GLuint*>(glMapBufferRange(GL_ATOMIC_COUNTER_BUFFER, 0, 32, GL_MAP_WRITE_BIT));
		for (GLuint i = 0; i < 8; ++i)
			ptr[i]	= i * 2;
		glUnmapBuffer(GL_ATOMIC_COUNTER_BUFFER);
		glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, 0);

		glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, buffer_);
		ptr = static_cast<GLuint*>(glMapBufferRange(GL_ATOMIC_COUNTER_BUFFER, 0, 32, GL_MAP_READ_BIT));
		for (GLuint i = 0; i < 8; ++i)
		{
			if (ptr[i] != i * 2)
			{
				Output("data[%u] is: %u should be: %u\n", i, data[i], i * 2);
				res = ERROR;
			}
		}
		glUnmapBuffer(GL_ATOMIC_COUNTER_BUFFER);
		glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, 0);

		glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, buffer_);
		for (GLuint i = 0; i < 8; ++i)
			data[i]   = i * 3;
		glBufferSubData(GL_ATOMIC_COUNTER_BUFFER, 0, 32, data);
		for (GLuint i = 0; i < 8; ++i)
			data[i]   = 0;
		glGetBufferSubData(GL_ATOMIC_COUNTER_BUFFER, 0, sizeof(data), data);
		glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, 0);
		for (GLuint i = 0; i < 8; ++i)
		{
			if (data[i] != i * 3)
			{
				Output("data[%u] is: %u should be: %u\n", i, data[i], i * 3);
				res = ERROR;
			}
		}

		return res;
	}

	virtual long Cleanup()
	{
		glDeleteBuffers(1, &buffer_);
		return NO_ERROR;
	}
};

class BasicBufferState : public SACSubcaseBase
{
	virtual std::string Title()
	{
		return NL "Atomic Counter Buffer - state";
	}

	virtual std::string Purpose()
	{
		return NL "Verify that setting and getting buffer state works as expected for new buffer target.";
	}

	virtual std::string Method()
	{
		return NL "";
	}

	virtual std::string PassCriteria()
	{
		return NL "";
	}

	virtual long Run()
	{
		Buffer buffer;
		glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, buffer.name());

		if (buffer.Verify() != NO_ERROR)
			return ERROR;

		buffer.Data(100, NULL, GL_DYNAMIC_COPY);
		if (buffer.Verify() != NO_ERROR)
			return ERROR;

		buffer.MapRange(10, 50, GL_MAP_WRITE_BIT);
		if (buffer.Verify() != NO_ERROR)
			return ERROR;
		buffer.Unmap();
		if (buffer.Verify() != NO_ERROR)
			return ERROR;

		buffer.Map(GL_READ_ONLY);
		if (buffer.Verify() != NO_ERROR)
			return ERROR;
		buffer.Unmap();
		if (buffer.Verify() != NO_ERROR)
			return ERROR;

		return NO_ERROR;
	}
};

class BasicBufferBind : public SACSubcaseBase
{
	virtual std::string Title()
	{
		return NL "Atomic Counter Buffer - binding";
	}

	virtual std::string Purpose()
	{
		return NL "Verify that binding buffer objects to ATOMIC_COUNTER_BUFFER (indexed) target" NL
				  "works as expected. In particualr make sure that binding with BindBufferBase and BindBufferRange" NL
				  "also bind to generic binding point and deleting buffer that is currently bound unbinds it. Tested" NL
				  "commands: BindBuffer, BindBufferBase and BindBufferRange.";
	}

	virtual std::string Method()
	{
		return NL "";
	}

	virtual std::string PassCriteria()
	{
		return NL "";
	}

	GLuint buffer_;

	virtual long Setup()
	{
		buffer_ = 0;
		return NO_ERROR;
	}

	virtual long Run()
	{
		GLint bindings;
		glGetIntegerv(GL_MAX_ATOMIC_COUNTER_BUFFER_BINDINGS, &bindings);
		Output("MAX_ATOMIC_COUNTER_BUFFER_BINDINGS: %d\n", bindings);

		if (!CheckGetCommands(GL_ATOMIC_COUNTER_BUFFER_BINDING, 0))
			return ERROR;
		for (GLint index = 0; index < bindings; ++index)
		{
			if (!CheckBufferBindingState(static_cast<GLuint>(index), 0, 0, 0))
				return ERROR;
		}

		glGenBuffers(1, &buffer_);
		glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, buffer_);

		if (!CheckGetCommands(GL_ATOMIC_COUNTER_BUFFER_BINDING, static_cast<GLint>(buffer_)))
			return ERROR;
		for (GLint index = 0; index < bindings; ++index)
		{
			if (!CheckBufferBindingState(static_cast<GLuint>(index), 0, 0, 0))
				return ERROR;
		}

		long res = NO_ERROR;

		glBufferData(GL_ATOMIC_COUNTER_BUFFER, 1000, NULL, GL_DYNAMIC_COPY);
		glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, 0);

		glBindBufferBase(GL_ATOMIC_COUNTER_BUFFER, 1, buffer_);
		if (!CheckBufferBindingState(1, static_cast<GLint>(buffer_), 0, 1000))
			res = ERROR;
		if (!CheckGetCommands(GL_ATOMIC_COUNTER_BUFFER_BINDING, static_cast<GLint>(buffer_)))
			res = ERROR;

		glBindBufferBase(GL_ATOMIC_COUNTER_BUFFER, static_cast<GLuint>(bindings / 2), buffer_);
		if (!CheckBufferBindingState(static_cast<GLuint>(bindings / 2), static_cast<GLint>(buffer_), 0, 1000))
			res = ERROR;

		glBindBufferBase(GL_ATOMIC_COUNTER_BUFFER, static_cast<GLuint>(bindings - 1), buffer_);
		if (!CheckBufferBindingState(static_cast<GLuint>(bindings - 1), static_cast<GLint>(buffer_), 0, 1000))
			res = ERROR;
		glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, 0);

		glBindBufferRange(GL_ATOMIC_COUNTER_BUFFER, 1, buffer_, 8, 32);
		if (!CheckBufferBindingState(1, static_cast<GLint>(buffer_), 8, 32))
			res = ERROR;
		if (!CheckGetCommands(GL_ATOMIC_COUNTER_BUFFER_BINDING, static_cast<GLint>(buffer_)))
			res = ERROR;

		glBindBufferRange(GL_ATOMIC_COUNTER_BUFFER, static_cast<GLuint>(bindings / 2), buffer_, 512, 100);
		if (!CheckBufferBindingState(static_cast<GLuint>(bindings / 2), static_cast<GLint>(buffer_), 512, 100))
			res = ERROR;

		glBindBufferRange(GL_ATOMIC_COUNTER_BUFFER, static_cast<GLuint>(bindings - 1), buffer_, 12, 128);
		if (!CheckBufferBindingState(static_cast<GLuint>(bindings - 1), static_cast<GLint>(buffer_), 12, 128))
			res = ERROR;

		glDeleteBuffers(1, &buffer_);
		buffer_ = 0;

		GLint i;
		glGetIntegerv(GL_ATOMIC_COUNTER_BUFFER_BINDING, &i);
		if (i != 0)
		{
			Output("Generic binding point should be 0 after deleting bound buffer object.\n");
			res = ERROR;
		}
		for (GLint index = 0; index < bindings; ++index)
		{
			glGetIntegeri_v(GL_ATOMIC_COUNTER_BUFFER_BINDING, static_cast<GLuint>(index), &i);
			if (i != 0)
			{
				Output("Binding point %u should be 0 after deleting bound buffer object.\n", index);
				res = ERROR;
			}
		}

		return res;
	}

	virtual long Cleanup()
	{
		glDeleteBuffers(1, &buffer_);
		return NO_ERROR;
	}
};

class BasicProgramMax : public SACSubcaseBase
{
	virtual std::string Title()
	{
		return NL "Program - max values";
	}

	virtual std::string Purpose()
	{
		return NL "Verify all max values which deal with atomic counter buffers.";
	}

	virtual std::string Method()
	{
		return NL "";
	}

	virtual std::string PassCriteria()
	{
		return NL "";
	}

	virtual long Run()
	{
		if (!CheckMaxValue(GL_MAX_ATOMIC_COUNTER_BUFFER_BINDINGS, 1))
			return ERROR;
		if (!CheckMaxValue(GL_MAX_ATOMIC_COUNTER_BUFFER_SIZE, 32))
			return ERROR;
		if (!CheckMaxValue(GL_MAX_COMBINED_ATOMIC_COUNTER_BUFFERS, 1))
			return ERROR;
		if (!CheckMaxValue(GL_MAX_COMBINED_ATOMIC_COUNTERS, 8))
			return ERROR;
		if (!CheckMaxValue(GL_MAX_VERTEX_ATOMIC_COUNTER_BUFFERS, 0))
			return ERROR;
		if (!CheckMaxValue(GL_MAX_VERTEX_ATOMIC_COUNTERS, 0))
			return ERROR;
		if (!CheckMaxValue(GL_MAX_TESS_CONTROL_ATOMIC_COUNTER_BUFFERS, 0))
			return ERROR;
		if (!CheckMaxValue(GL_MAX_TESS_CONTROL_ATOMIC_COUNTERS, 0))
			return ERROR;
		if (!CheckMaxValue(GL_MAX_TESS_EVALUATION_ATOMIC_COUNTER_BUFFERS, 0))
			return ERROR;
		if (!CheckMaxValue(GL_MAX_TESS_EVALUATION_ATOMIC_COUNTERS, 0))
			return ERROR;
		if (!CheckMaxValue(GL_MAX_GEOMETRY_ATOMIC_COUNTER_BUFFERS, 0))
			return ERROR;
		if (!CheckMaxValue(GL_MAX_GEOMETRY_ATOMIC_COUNTERS, 0))
			return ERROR;
		if (!CheckMaxValue(GL_MAX_FRAGMENT_ATOMIC_COUNTER_BUFFERS, 1))
			return ERROR;
		if (!CheckMaxValue(GL_MAX_FRAGMENT_ATOMIC_COUNTERS, 8))
			return ERROR;
		return NO_ERROR;
	}
};

class BasicProgramQuery : public SACSubcaseBase
{
	virtual std::string Title()
	{
		return NL "Program - atomic counters queries";
	}

	virtual std::string Purpose()
	{
		return NL "Get all the information from the program object about atomic counters." NL
				  "Verify that all informations are correct. Tested commands: glGetActiveAtomicCounterBufferiv," NL
				  "GetProgramiv and GetUniform* with new enums.";
	}

	virtual std::string Method()
	{
		return NL "";
	}

	virtual std::string PassCriteria()
	{
		return NL "";
	}

	GLuint counter_buffer_;
	GLuint vao_, vbo_;
	GLuint prog_;

	virtual long Setup()
	{
		counter_buffer_ = 0;
		vao_ = vbo_ = 0;
		prog_		= 0;
		return NO_ERROR;
	}

	virtual long Run()
	{
		// create program
		const char* glsl_vs = "#version 420 core" NL "layout(location = 0) in vec4 i_vertex;" NL "void main() {" NL
							  "  gl_Position = i_vertex;" NL "}";
		const char* glsl_fs =
			"#version 420 core" NL "layout(location = 0, index = 0)  out vec4 o_color;" NL
			"layout(binding = 0, offset = 0)  uniform atomic_uint ac_counter0;" NL
			"layout(binding = 0, offset = 4)  uniform atomic_uint ac_counter1;" NL
			"layout(binding = 0)              uniform atomic_uint ac_counter2;" NL
			"layout(binding = 0)              uniform atomic_uint ac_counter67[2];" NL
			"layout(binding = 0)              uniform atomic_uint ac_counter3;" NL
			"layout(binding = 0)              uniform atomic_uint ac_counter4;" NL
			"layout(binding = 0)              uniform atomic_uint ac_counter5;" NL "void main() {" NL "  uint c = 0;" NL
			"  c += atomicCounterIncrement(ac_counter0);" NL "  c += atomicCounterIncrement(ac_counter1);" NL
			"  c += atomicCounterIncrement(ac_counter2);" NL "  c += atomicCounterIncrement(ac_counter3);" NL
			"  c += atomicCounterIncrement(ac_counter4);" NL "  c += atomicCounterIncrement(ac_counter5);" NL
			"  c += atomicCounterIncrement(ac_counter67[0]);" NL "  c += atomicCounterIncrement(ac_counter67[1]);" NL
			"  if (c > 10u) o_color = vec4(0.0, 1.0, 0.0, 1.0);" NL "  else o_color = vec4(1.0, 0.0, 0.0, 1.0);" NL "}";

		prog_ = CreateProgram(glsl_vs, NULL, NULL, NULL, glsl_fs, true);

		// get active buffers
		GLuint active_buffers;
		glGetProgramiv(prog_, GL_ACTIVE_ATOMIC_COUNTER_BUFFERS, reinterpret_cast<GLint*>(&active_buffers));
		if (active_buffers != 1)
		{
			Output("GL_ACTIVE_ATOMIC_COUNTER_BUFFERS is %u should be %d.\n", active_buffers, 1);
			return ERROR;
		}
		GLint buffers_binding_index;
		glGetActiveAtomicCounterBufferiv(prog_, 0, GL_ATOMIC_COUNTER_BUFFER_BINDING, &buffers_binding_index);

		GLint i;
		glGetActiveAtomicCounterBufferiv(prog_, buffers_binding_index, GL_ATOMIC_COUNTER_BUFFER_DATA_SIZE, &i);
		if (i < 32)
		{
			Output("GL_ATOMIC_COUNTER_BUFFER_DATA_SIZE is %d should be at least %d.\n", i, 32);
			return ERROR;
		}
		glGetActiveAtomicCounterBufferiv(prog_, buffers_binding_index, GL_ATOMIC_COUNTER_BUFFER_ACTIVE_ATOMIC_COUNTERS,
										 &i);
		if (i != 7)
		{
			Output("GL_ATOMIC_COUNTER_BUFFER_ACTIVE_ATOMIC_COUNTERS is %d should be %d.\n", i, 8);
			return ERROR;
		}
		GLint indices[7] = { -1, -1, -1, -1, -1, -1, -1 };
		glGetActiveAtomicCounterBufferiv(prog_, buffers_binding_index,
										 GL_ATOMIC_COUNTER_BUFFER_ACTIVE_ATOMIC_COUNTER_INDICES, indices);
		Output("GL_ATOMIC_COUNTER_BUFFER_ACTIVE_ATOMIC_COUNTER_INDICES:\n");
		for (i = 0; i < 7; ++i)
		{
			Output("%d ", indices[i]);
			if (indices[i] == -1)
			{
				Output("Index -1 found!\n");
				return ERROR;
			}
		}
		Output("\n");

		glGetActiveAtomicCounterBufferiv(prog_, buffers_binding_index,
										 GL_ATOMIC_COUNTER_BUFFER_REFERENCED_BY_VERTEX_SHADER, &i);
		if (i != GL_FALSE)
		{
			Output("GL_ATOMIC_COUNTER_BUFFER_REFERENCED_BY_VERTEX_SHADER is %d should be %d.\n", i, 0);
			return ERROR;
		}
		glGetActiveAtomicCounterBufferiv(prog_, buffers_binding_index,
										 GL_ATOMIC_COUNTER_BUFFER_REFERENCED_BY_TESS_CONTROL_SHADER, &i);
		if (i != GL_FALSE)
		{
			Output("GL_ATOMIC_COUNTER_BUFFER_REFERENCED_BY_TESS_CONTROL_SHADER is %d should be %d.\n", i, 0);
			return ERROR;
		}
		glGetActiveAtomicCounterBufferiv(prog_, buffers_binding_index,
										 GL_ATOMIC_COUNTER_BUFFER_REFERENCED_BY_TESS_EVALUATION_SHADER, &i);
		if (i != GL_FALSE)
		{
			Output("GL_ATOMIC_COUNTER_BUFFER_REFERENCED_BY_TESS_EVALUATION_SHADER is %d should be %d.\n", i, 0);
			return ERROR;
		}
		glGetActiveAtomicCounterBufferiv(prog_, buffers_binding_index,
										 GL_ATOMIC_COUNTER_BUFFER_REFERENCED_BY_GEOMETRY_SHADER, &i);
		if (i != GL_FALSE)
		{
			Output("GL_ATOMIC_COUNTER_BUFFER_REFERENCED_BY_GEOMETRY_SHADER is %d should be %d.\n", i, 0);
			return ERROR;
		}
		glGetActiveAtomicCounterBufferiv(prog_, buffers_binding_index,
										 GL_ATOMIC_COUNTER_BUFFER_REFERENCED_BY_FRAGMENT_SHADER, &i);
		if (i != GL_TRUE)
		{
			Output("GL_ATOMIC_COUNTER_BUFFER_REFERENCED_BY_FRAGMENT_SHADER is %d should be %d.\n", i, 1);
			return ERROR;
		}

		// get active uniforms
		std::map<std::string, GLuint> uniforms_name_index;
		GLuint active_uniforms;
		glGetProgramiv(prog_, GL_ACTIVE_UNIFORMS, reinterpret_cast<GLint*>(&active_uniforms));
		if (active_uniforms != 7)
		{
			Output("GL_ACTIVE_UNIFORMS is %u should be %d.\n", active_uniforms, 8);
			return ERROR;
		}
		for (GLuint index = 0; index < active_uniforms; ++index)
		{
			GLchar name[32];
			glGetActiveUniformName(prog_, index, sizeof(name), NULL, name);
			uniforms_name_index.insert(std::make_pair(name, index));
		}

		if (!CheckUniform(prog_, "ac_counter0", uniforms_name_index["ac_counter0"], GL_UNSIGNED_INT_ATOMIC_COUNTER, 1,
						  0, 0, buffers_binding_index))
			return ERROR;
		if (!CheckUniform(prog_, "ac_counter1", uniforms_name_index["ac_counter1"], GL_UNSIGNED_INT_ATOMIC_COUNTER, 1,
						  4, 0, buffers_binding_index))
			return ERROR;
		if (!CheckUniform(prog_, "ac_counter2", uniforms_name_index["ac_counter2"], GL_UNSIGNED_INT_ATOMIC_COUNTER, 1,
						  8, 0, buffers_binding_index))
			return ERROR;
		if (!CheckUniform(prog_, "ac_counter3", uniforms_name_index["ac_counter3"], GL_UNSIGNED_INT_ATOMIC_COUNTER, 1,
						  20, 0, buffers_binding_index))
			return ERROR;
		if (!CheckUniform(prog_, "ac_counter4", uniforms_name_index["ac_counter4"], GL_UNSIGNED_INT_ATOMIC_COUNTER, 1,
						  24, 0, buffers_binding_index))
			return ERROR;
		if (!CheckUniform(prog_, "ac_counter5", uniforms_name_index["ac_counter5"], GL_UNSIGNED_INT_ATOMIC_COUNTER, 1,
						  28, 0, buffers_binding_index))
			return ERROR;
		if (!CheckUniform(prog_, "ac_counter67[0]", uniforms_name_index["ac_counter67[0]"],
						  GL_UNSIGNED_INT_ATOMIC_COUNTER, 2, 12, 4, buffers_binding_index))
			return ERROR;

		// create atomic counter buffer
		glGenBuffers(1, &counter_buffer_);
		glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, counter_buffer_);
		const unsigned int data[7] = { 20, 20, 20, 20, 20, 20, 20 };
		glBufferData(GL_ATOMIC_COUNTER_BUFFER, sizeof(data), data, GL_DYNAMIC_DRAW);
		glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, 0);

		// create geometry
		CreateQuad(&vao_, &vbo_, NULL);

		// draw
		glBindBufferBase(GL_ATOMIC_COUNTER_BUFFER, 0, counter_buffer_);
		glUseProgram(prog_);
		glBindVertexArray(vao_);
		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

		if (ValidateReadBuffer(Vec4(0, 1, 0, 1)) != NO_ERROR)
		{
			Output("Render target color should be (0.0, 1.0, 0.0, 1.0).\n");
			return ERROR;
		}
		return NO_ERROR;
	}

	virtual long Cleanup()
	{
		glDeleteBuffers(1, &counter_buffer_);
		glDeleteVertexArrays(1, &vao_);
		glDeleteBuffers(1, &vbo_);
		glDeleteProgram(prog_);
		glUseProgram(0);
		return NO_ERROR;
	}
};

class BasicUsageSimple : public SACSubcaseBase
{
	virtual std::string Title()
	{
		return NL "Simple Use Case";
	}

	virtual std::string Purpose()
	{
		return NL "Verify that simple usage of atomic counters work as expected." NL
				  "In FS value returned from atomicCounterIncrement is converted to color.";
	}

	virtual std::string Method()
	{
		return NL "";
	}

	virtual std::string PassCriteria()
	{
		return NL "";
	}

	GLuint counter_buffer_;
	GLuint vao_, vbo_;
	GLuint prog_;

	virtual long Setup()
	{
		counter_buffer_ = 0;
		vao_ = vbo_ = 0;
		prog_		= 0;
		return NO_ERROR;
	}

	virtual long Run()
	{
		// create program
		const char* src_vs = "#version 420 core" NL "layout(location = 0) in vec4 i_vertex;" NL "void main() {" NL
							 "  gl_Position = i_vertex;" NL "}";
		const char* src_fs = "#version 420 core" NL "layout(location = 0) out vec4 o_color;" NL
							 "layout(binding = 0, offset = 0) uniform atomic_uint ac_counter;" NL "void main() {" NL
							 "  uint c = atomicCounterIncrement(ac_counter);" NL
							 "  float r = float(c / 40u) / 255.0;" NL "  o_color = vec4(r, 0.0, 0.0, 1.0);" NL "}";
		prog_ = CreateProgram(src_vs, NULL, NULL, NULL, src_fs, true);

		// create atomic counter buffer
		glGenBuffers(1, &counter_buffer_);
		glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, counter_buffer_);
		glBufferData(GL_ATOMIC_COUNTER_BUFFER, 4, NULL, GL_DYNAMIC_COPY);
		glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, 0);

		// create geometry
		CreateQuad(&vao_, &vbo_, NULL);

		// clear counter buffer (set to 0)
		glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, counter_buffer_);
		unsigned int* ptr = static_cast<unsigned int*>(
			glMapBufferRange(GL_ATOMIC_COUNTER_BUFFER, 0, 4, GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT));
		*ptr = 0;
		glUnmapBuffer(GL_ATOMIC_COUNTER_BUFFER);

		// draw
		glBindBufferBase(GL_ATOMIC_COUNTER_BUFFER, 0, counter_buffer_);
		glUseProgram(prog_);
		glBindVertexArray(vao_);
		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

		ValidateReadBuffer(Vec4(1, 0, 0, 1));

		return NO_ERROR;
	}

	virtual long Cleanup()
	{
		glDeleteBuffers(1, &counter_buffer_);
		glDeleteVertexArrays(1, &vao_);
		glDeleteBuffers(1, &vbo_);
		glDeleteProgram(prog_);
		glUseProgram(0);

		return NO_ERROR;
	}
};

class BasicUsageFS : public SACSubcaseBase
{
	virtual std::string Title()
	{
		return NL "Atomic Counters usage in the Fragment Shader stage";
	}

	virtual std::string Purpose()
	{
		return NL "Verify that atomic counters work as expected in the Fragment Shader stage." NL
				  "In particular make sure that values returned by GLSL built-in functions" NL
				  "atomicCounterIncrement and atomicCounterDecrement are unique in every shader invocation." NL
				  "Also make sure that the final values in atomic counter buffer objects are as expected.";
	}

	virtual std::string Method()
	{
		return NL "";
	}

	virtual std::string PassCriteria()
	{
		return NL "";
	}

	GLuint counter_buffer_;
	GLuint vao_, vbo_;
	GLuint prog_;
	GLuint fbo_, rt_[2];

	virtual long Setup()
	{
		counter_buffer_ = 0;
		vao_ = vbo_ = 0;
		prog_		= 0;
		fbo_ = rt_[0] = rt_[1] = 0;
		return NO_ERROR;
	}

	virtual long Run()
	{
		// create program
		const char* src_vs = "#version 420 core" NL "layout(location = 0) in vec4 i_vertex;" NL "void main() {" NL
							 "  gl_Position = i_vertex;" NL "}";

		const char* src_fs = "#version 420 core" NL "layout(location = 0) out uvec4 o_color[2];" NL
							 "layout(binding = 0, offset = 0) uniform atomic_uint ac_counter_inc;" NL
							 "layout(binding = 0, offset = 4) uniform atomic_uint ac_counter_dec;" NL "void main() {" NL
							 "  o_color[0] = uvec4(atomicCounterIncrement(ac_counter_inc));" NL
							 "  o_color[1] = uvec4(atomicCounterDecrement(ac_counter_dec));" NL "}";
		prog_ = CreateProgram(src_vs, NULL, NULL, NULL, src_fs, true);

		// create atomic counter buffer
		glGenBuffers(1, &counter_buffer_);
		glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, counter_buffer_);
		glBufferData(GL_ATOMIC_COUNTER_BUFFER, 8, NULL, GL_DYNAMIC_COPY);
		glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, 0);

		// create render targets
		const int s = 8;
		glGenTextures(2, rt_);

		for (int i = 0; i < 2; ++i)
		{
			glBindTexture(GL_TEXTURE_2D, rt_[i]);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_R32UI, s, s, 0, GL_RED_INTEGER, GL_UNSIGNED_INT, NULL);
			glBindTexture(GL_TEXTURE_2D, 0);
		}

		// create fbo
		glGenFramebuffers(1, &fbo_);
		glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
		glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, rt_[0], 0);
		glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, rt_[1], 0);
		const GLenum draw_buffers[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
		glDrawBuffers(2, draw_buffers);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		// create geometry
		CreateQuad(&vao_, &vbo_, NULL);

		// init counter buffer
		glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, counter_buffer_);
		unsigned int* ptr = static_cast<unsigned int*>(
			glMapBufferRange(GL_ATOMIC_COUNTER_BUFFER, 0, 8, GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT));
		*ptr++ = 0;
		*ptr++ = 80;
		glUnmapBuffer(GL_ATOMIC_COUNTER_BUFFER);

		// draw
		glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
		glViewport(0, 0, s, s);
		glBindBufferBase(GL_ATOMIC_COUNTER_BUFFER, 0, counter_buffer_);
		glUseProgram(prog_);
		glBindVertexArray(vao_);
		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

		// validate
		GLuint data[s * s];
		glReadBuffer(GL_COLOR_ATTACHMENT0);
		glReadPixels(0, 0, s, s, GL_RED_INTEGER, GL_UNSIGNED_INT, data);
		if (!CheckCounterValues(s * s, data, 0))
			return ERROR;

		glReadBuffer(GL_COLOR_ATTACHMENT1);
		glReadPixels(0, 0, s, s, GL_RED_INTEGER, GL_UNSIGNED_INT, data);
		if (!CheckCounterValues(s * s, data, 16))
			return ERROR;

		if (!CheckFinalCounterValue(counter_buffer_, 0, 64))
			return ERROR;
		if (!CheckFinalCounterValue(counter_buffer_, 4, 16))
			return ERROR;

		return NO_ERROR;
	}

	virtual long Cleanup()
	{
		glDeleteFramebuffers(1, &fbo_);
		glDeleteTextures(2, rt_);
		glViewport(0, 0, getWindowWidth(), getWindowHeight());
		glDeleteBuffers(1, &counter_buffer_);
		glDeleteVertexArrays(1, &vao_);
		glDeleteBuffers(1, &vbo_);
		glDeleteProgram(prog_);
		glUseProgram(0);
		return NO_ERROR;
	}
};

class BasicUsageVS : public SACSubcaseBase
{
	virtual std::string Title()
	{
		return NL "Atomic Counters usage in the Vertex Shader stage";
	}

	virtual std::string Purpose()
	{
		return NL "Verify that atomic counters work as expected in the Vertex Shader stage." NL
				  "In particular make sure that values returned by GLSL built-in functions" NL
				  "atomicCounterIncrement and atomicCounterDecrement are unique in every shader invocation." NL
				  "Also make sure that the final values in atomic counter buffer objects are as expected.";
	}

	virtual std::string Method()
	{
		return NL "";
	}

	virtual std::string PassCriteria()
	{
		return NL "";
	}

	GLuint counter_buffer_[2];
	GLuint xfb_buffer_[2];
	GLuint array_buffer_;
	GLuint vao_;
	GLuint prog_;

	virtual long Setup()
	{
		counter_buffer_[0] = counter_buffer_[1] = 0;
		xfb_buffer_[0] = xfb_buffer_[1] = 0;
		array_buffer_					= 0;
		vao_							= 0;
		prog_							= 0;
		return NO_ERROR;
	}

	virtual long Run()
	{

		GLint p1, p2;
		glGetIntegerv(GL_MAX_VERTEX_ATOMIC_COUNTER_BUFFERS, &p1);
		glGetIntegerv(GL_MAX_VERTEX_ATOMIC_COUNTERS, &p2);
		if (p1 < 2 || p2 < 2)
		{
			return NO_ERROR;
		}

		// create program
		const char* src_vs =
			"#version 420 core" NL "layout(location = 0) in uint i_zero;" NL "out uint o_atomic_inc;" NL
			"out uint o_atomic_dec;" NL "layout(binding = 0, offset = 0) uniform atomic_uint ac_counter_inc;" NL
			"layout(binding = 1, offset = 0) uniform atomic_uint ac_counter_dec;" NL "void main() {" NL
			"  o_atomic_inc = i_zero + atomicCounterIncrement(ac_counter_inc);" NL
			"  o_atomic_dec = i_zero + atomicCounterDecrement(ac_counter_dec);" NL "}";
		prog_				   = CreateProgram(src_vs, NULL, NULL, NULL, NULL, false);
		const char* xfb_var[2] = { "o_atomic_inc", "o_atomic_dec" };
		glTransformFeedbackVaryings(prog_, 2, xfb_var, GL_SEPARATE_ATTRIBS);
		LinkProgram(prog_);

		// create array buffer
		const unsigned int array_buffer_data[32] = { 0 };
		glGenBuffers(1, &array_buffer_);
		glBindBuffer(GL_ARRAY_BUFFER, array_buffer_);
		glBufferData(GL_ARRAY_BUFFER, sizeof(array_buffer_data), array_buffer_data, GL_STATIC_DRAW);
		glBindBuffer(GL_ARRAY_BUFFER, 0);

		// create atomic counter buffers
		glGenBuffers(2, counter_buffer_);
		glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, counter_buffer_[0]);
		glBufferData(GL_ATOMIC_COUNTER_BUFFER, 4, NULL, GL_DYNAMIC_COPY);
		glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, counter_buffer_[1]);
		glBufferData(GL_ATOMIC_COUNTER_BUFFER, 4, NULL, GL_DYNAMIC_COPY);
		glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, 0);

		// create transform feedback buffers
		glGenBuffers(2, xfb_buffer_);
		glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, xfb_buffer_[0]);
		glBufferData(GL_TRANSFORM_FEEDBACK_BUFFER, 1000, NULL, GL_STREAM_COPY);
		glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, xfb_buffer_[1]);
		glBufferData(GL_TRANSFORM_FEEDBACK_BUFFER, 1000, NULL, GL_STREAM_COPY);
		glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, 0);

		// init counter buffers
		glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, counter_buffer_[0]);
		unsigned int* ptr = static_cast<unsigned int*>(
			glMapBufferRange(GL_ATOMIC_COUNTER_BUFFER, 0, 4, GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT));
		*ptr = 7;
		glUnmapBuffer(GL_ATOMIC_COUNTER_BUFFER);

		glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, counter_buffer_[1]);
		ptr = static_cast<unsigned int*>(
			glMapBufferRange(GL_ATOMIC_COUNTER_BUFFER, 0, 4, GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT));
		*ptr = 77;
		glUnmapBuffer(GL_ATOMIC_COUNTER_BUFFER);

		// create vertex array object
		glGenVertexArrays(1, &vao_);
		glBindVertexArray(vao_);
		glBindBuffer(GL_ARRAY_BUFFER, array_buffer_);
		glVertexAttribIPointer(0, 1, GL_UNSIGNED_INT, 0, 0);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glEnableVertexAttribArray(0);
		glBindVertexArray(0);

		// draw
		glEnable(GL_RASTERIZER_DISCARD);
		glBindBufferBase(GL_ATOMIC_COUNTER_BUFFER, 0, counter_buffer_[0]);
		glBindBufferBase(GL_ATOMIC_COUNTER_BUFFER, 1, counter_buffer_[1]);
		glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, xfb_buffer_[0]);
		glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 1, xfb_buffer_[1]);
		glUseProgram(prog_);
		glBindVertexArray(vao_);
		glBeginTransformFeedback(GL_POINTS);
		glDrawArrays(GL_POINTS, 0, 32);
		glEndTransformFeedback();
		glDisable(GL_RASTERIZER_DISCARD);

		// validate
		GLuint data[32];
		glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, xfb_buffer_[0]);
		glGetBufferSubData(GL_TRANSFORM_FEEDBACK_BUFFER, 0, sizeof(data), data);
		if (!CheckCounterValues(32, data, 7))
			return ERROR;

		glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, xfb_buffer_[1]);
		glGetBufferSubData(GL_TRANSFORM_FEEDBACK_BUFFER, 0, sizeof(data), data);
		if (!CheckCounterValues(32, data, 45))
			return ERROR;

		if (!CheckFinalCounterValue(counter_buffer_[0], 0, 39))
			return ERROR;
		if (!CheckFinalCounterValue(counter_buffer_[1], 0, 45))
			return ERROR;

		return NO_ERROR;
	}

	virtual long Cleanup()
	{
		glDeleteBuffers(2, counter_buffer_);
		glDeleteBuffers(2, xfb_buffer_);
		glDeleteBuffers(1, &array_buffer_);
		glDeleteVertexArrays(1, &vao_);
		glDeleteProgram(prog_);
		glUseProgram(0);
		return NO_ERROR;
	}
};

class BasicUsageGS : public SACSubcaseBase
{
	virtual std::string Title()
	{
		return NL "Atomic Counters usage in the Geometry Shader stage";
	}

	virtual std::string Purpose()
	{
		return NL "Verify that atomic counters work as expected in the Geometry Shader stage." NL
				  "In particular make sure that values returned by GLSL built-in functions" NL
				  "atomicCounterIncrement and atomicCounterDecrement are unique in every shader invocation." NL
				  "Also make sure that the final values in atomic counter buffer objects are as expected.";
	}

	virtual std::string Method()
	{
		return NL "";
	}

	virtual std::string PassCriteria()
	{
		return NL "";
	}

	GLuint counter_buffer_;
	GLuint xfb_buffer_[2];
	GLuint array_buffer_;
	GLuint vao_;
	GLuint prog_;

	virtual long Setup()
	{
		counter_buffer_ = 0;
		xfb_buffer_[0] = xfb_buffer_[1] = 0;
		array_buffer_					= 0;
		vao_							= 0;
		prog_							= 0;
		return NO_ERROR;
	}

	virtual long Run()
	{

		GLint p1, p2;
		glGetIntegerv(GL_MAX_GEOMETRY_ATOMIC_COUNTER_BUFFERS, &p1);
		glGetIntegerv(GL_MAX_GEOMETRY_ATOMIC_COUNTERS, &p2);
		if (p1 < 1 || p2 < 2)
		{
			return NO_ERROR;
		}

		// create program
		const char* glsl_vs = "#version 420 core" NL "layout(location = 0) in uint i_zero;" NL "out uint vs_zero;" NL
							  "void main() {" NL "  vs_zero = i_zero;" NL "}";
		const char* glsl_gs =
			"#version 420 core" NL "layout(points) in;" NL "in uint vs_zero[];" NL
			"layout(points, max_vertices = 1) out;" NL "out uint o_atomic_inc;" NL "out uint o_atomic_dec;" NL
			"layout(binding = 0, offset = 8) uniform atomic_uint ac_counter_inc;" NL
			"layout(binding = 0, offset = 16) uniform atomic_uint ac_counter_dec;" NL "void main() {" NL
			"  o_atomic_inc = vs_zero[0] + atomicCounterIncrement(ac_counter_inc);" NL
			"  o_atomic_dec = vs_zero[0] + atomicCounterDecrement(ac_counter_dec);" NL "  EmitVertex();" NL "}";
		prog_				   = CreateProgram(glsl_vs, NULL, NULL, glsl_gs, NULL, false);
		const char* xfb_var[2] = { "o_atomic_inc", "o_atomic_dec" };
		glTransformFeedbackVaryings(prog_, 2, xfb_var, GL_SEPARATE_ATTRIBS);
		LinkProgram(prog_);

		// create array buffer
		const unsigned int array_buffer_data[32] = { 0 };
		glGenBuffers(1, &array_buffer_);
		glBindBuffer(GL_ARRAY_BUFFER, array_buffer_);
		glBufferData(GL_ARRAY_BUFFER, sizeof(array_buffer_data), array_buffer_data, GL_STATIC_DRAW);
		glBindBuffer(GL_ARRAY_BUFFER, 0);

		// create atomic counter buffer
		glGenBuffers(1, &counter_buffer_);
		glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, counter_buffer_);
		glBufferData(GL_ATOMIC_COUNTER_BUFFER, 32, NULL, GL_DYNAMIC_COPY);
		glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, 0);

		// create transform feedback buffers
		glGenBuffers(2, xfb_buffer_);
		glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, xfb_buffer_[0]);
		glBufferData(GL_TRANSFORM_FEEDBACK_BUFFER, 1000, NULL, GL_STREAM_COPY);
		glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, xfb_buffer_[1]);
		glBufferData(GL_TRANSFORM_FEEDBACK_BUFFER, 1000, NULL, GL_STREAM_COPY);
		glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, 0);

		// create vertex array object
		glGenVertexArrays(1, &vao_);
		glBindVertexArray(vao_);
		glBindBuffer(GL_ARRAY_BUFFER, array_buffer_);
		glVertexAttribIPointer(0, 1, GL_UNSIGNED_INT, 0, 0);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glEnableVertexAttribArray(0);
		glBindVertexArray(0);

		// init counter buffer
		glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, counter_buffer_);
		unsigned int* ptr = static_cast<unsigned int*>(
			glMapBufferRange(GL_ATOMIC_COUNTER_BUFFER, 0, 32, GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT));
		*(ptr + 2) = 17;
		*(ptr + 4) = 100;
		glUnmapBuffer(GL_ATOMIC_COUNTER_BUFFER);

		// draw
		glEnable(GL_RASTERIZER_DISCARD);
		glBindBufferBase(GL_ATOMIC_COUNTER_BUFFER, 0, counter_buffer_);
		glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, xfb_buffer_[0]);
		glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 1, xfb_buffer_[1]);
		glUseProgram(prog_);
		glBindVertexArray(vao_);
		glBeginTransformFeedback(GL_POINTS);
		glDrawArrays(GL_POINTS, 0, 32);
		glEndTransformFeedback();
		glDisable(GL_RASTERIZER_DISCARD);

		// validate
		GLuint data[32];
		glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, xfb_buffer_[0]);
		glGetBufferSubData(GL_TRANSFORM_FEEDBACK_BUFFER, 0, sizeof(data), data);
		if (!CheckCounterValues(32, data, 17))
			return ERROR;

		glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, xfb_buffer_[1]);
		glGetBufferSubData(GL_TRANSFORM_FEEDBACK_BUFFER, 0, sizeof(data), data);
		if (!CheckCounterValues(32, data, 68))
			return ERROR;

		if (!CheckFinalCounterValue(counter_buffer_, 8, 49))
			return ERROR;
		if (!CheckFinalCounterValue(counter_buffer_, 16, 68))
			return ERROR;

		return NO_ERROR;
	}

	virtual long Cleanup()
	{
		glDeleteBuffers(1, &counter_buffer_);
		glDeleteBuffers(2, xfb_buffer_);
		glDeleteBuffers(1, &array_buffer_);
		glDeleteVertexArrays(1, &vao_);
		glDeleteProgram(prog_);
		glUseProgram(0);
		return NO_ERROR;
	}
};

class BasicUsageTES : public SACSubcaseBase
{
	virtual std::string Title()
	{
		return NL "Atomic Counters usage in the Tessellation Evaluation Shader stage";
	}

	virtual std::string Purpose()
	{
		return NL "Verify that atomic counters work as expected in the Tessellation Evaluation Shader stage." NL
				  "In particular make sure that values returned by GLSL built-in functions" NL
				  "atomicCounterIncrement and atomicCounterDecrement are unique in every shader invocation." NL
				  "Also make sure that the final values in atomic counter buffer objects are as expected.";
	}

	virtual std::string Method()
	{
		return NL "";
	}

	virtual std::string PassCriteria()
	{
		return NL "";
	}

	GLuint counter_buffer_;
	GLuint xfb_buffer_[2];
	GLuint array_buffer_;
	GLuint vao_;
	GLuint prog_;

	virtual long Setup()
	{
		counter_buffer_ = 0;
		xfb_buffer_[0] = xfb_buffer_[1] = 0;
		array_buffer_					= 0;
		vao_							= 0;
		prog_							= 0;
		return NO_ERROR;
	}

	virtual long Run()
	{

		GLint p1, p2;
		glGetIntegerv(GL_MAX_TESS_EVALUATION_ATOMIC_COUNTER_BUFFERS, &p1);
		glGetIntegerv(GL_MAX_TESS_EVALUATION_ATOMIC_COUNTERS, &p2);
		if (p1 < 1 || p2 < 1)
		{
			return NO_ERROR;
		}

		const char* glsl_vs = "#version 420 core" NL "layout(location = 0) in uint i_zero;" NL "out uint vs_zero;" NL
							  "void main() {" NL "  vs_zero = i_zero;" NL "}";
		const char* glsl_tes =
			"#version 420 core" NL "layout(triangles, equal_spacing, ccw) in;" NL "in uint vs_zero[];" NL
			"out uint o_atomic_inc;" NL "out uint o_atomic_dec;" NL
			"layout(binding = 0, offset = 128) uniform atomic_uint ac_counter[2];" NL "void main() {" NL
			"  o_atomic_inc = vs_zero[0] + vs_zero[31] + atomicCounterIncrement(ac_counter[0]);" NL
			"  o_atomic_dec = vs_zero[0] + vs_zero[31] + atomicCounterDecrement(ac_counter[1]);" NL "}";
		// programs
		prog_				   = CreateProgram(glsl_vs, NULL, glsl_tes, NULL, NULL, false);
		const char* xfb_var[2] = { "o_atomic_inc", "o_atomic_dec" };
		glTransformFeedbackVaryings(prog_, 2, xfb_var, GL_SEPARATE_ATTRIBS);
		LinkProgram(prog_);

		// create array buffer
		const unsigned int array_buffer_data[32] = { 0 };
		glGenBuffers(1, &array_buffer_);
		glBindBuffer(GL_ARRAY_BUFFER, array_buffer_);
		glBufferData(GL_ARRAY_BUFFER, sizeof(array_buffer_data), array_buffer_data, GL_STATIC_DRAW);
		glBindBuffer(GL_ARRAY_BUFFER, 0);

		// create atomic counter buffer
		glGenBuffers(1, &counter_buffer_);
		glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, counter_buffer_);
		glBufferData(GL_ATOMIC_COUNTER_BUFFER, 200, NULL, GL_DYNAMIC_READ);
		glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, 0);

		// create transform feedback buffers
		glGenBuffers(2, xfb_buffer_);
		glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, xfb_buffer_[0]);
		glBufferData(GL_TRANSFORM_FEEDBACK_BUFFER, 1000, NULL, GL_STREAM_COPY);
		glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, xfb_buffer_[1]);
		glBufferData(GL_TRANSFORM_FEEDBACK_BUFFER, 1000, NULL, GL_STREAM_COPY);
		glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, 0);

		// create vertex array object
		glGenVertexArrays(1, &vao_);
		glBindVertexArray(vao_);
		glBindBuffer(GL_ARRAY_BUFFER, array_buffer_);
		glVertexAttribIPointer(0, 1, GL_UNSIGNED_INT, 0, 0);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glEnableVertexAttribArray(0);
		glBindVertexArray(0);

		// init counter buffer
		glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, counter_buffer_);
		unsigned int* ptr = static_cast<unsigned int*>(
			glMapBufferRange(GL_ATOMIC_COUNTER_BUFFER, 0, 180, GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT));
		*(ptr + 32) = 100000;
		*(ptr + 33) = 111;
		glUnmapBuffer(GL_ATOMIC_COUNTER_BUFFER);

		// draw
		glEnable(GL_RASTERIZER_DISCARD);
		glBindBufferBase(GL_ATOMIC_COUNTER_BUFFER, 0, counter_buffer_);
		glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, xfb_buffer_[0]);
		glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 1, xfb_buffer_[1]);
		glUseProgram(prog_);
		glBindVertexArray(vao_);
		glPatchParameteri(GL_PATCH_VERTICES, 32);
		glBeginTransformFeedback(GL_TRIANGLES);
		glDrawArrays(GL_PATCHES, 0, 32);
		glEndTransformFeedback();
		glPatchParameteri(GL_PATCH_VERTICES, 3);
		glDisable(GL_RASTERIZER_DISCARD);

		// validate
		GLuint data[3];
		glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, xfb_buffer_[0]);
		glGetBufferSubData(GL_TRANSFORM_FEEDBACK_BUFFER, 0, sizeof(data), data);
		if (!CheckCounterValues(3, data, 100000))
			return ERROR;

		glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, xfb_buffer_[1]);
		glGetBufferSubData(GL_TRANSFORM_FEEDBACK_BUFFER, 0, sizeof(data), data);
		if (!CheckCounterValues(3, data, 108))
			return ERROR;

		if (!CheckFinalCounterValue(counter_buffer_, 128, 100003))
			return ERROR;
		if (!CheckFinalCounterValue(counter_buffer_, 132, 108))
			return ERROR;

		return NO_ERROR;
	}

	virtual long Cleanup()
	{
		glDeleteBuffers(1, &counter_buffer_);
		glDeleteBuffers(2, xfb_buffer_);
		glDeleteBuffers(1, &array_buffer_);
		glDeleteVertexArrays(1, &vao_);
		glDeleteProgram(prog_);
		glUseProgram(0);
		return NO_ERROR;
	}
};

class AdvancedUsageMultiStage : public SACSubcaseBase
{
	virtual std::string Title()
	{
		return NL "Same atomic counter accessed from multiple shader stages";
	}

	virtual std::string Purpose()
	{
		return NL "Same atomic counter is incremented (decremented) from two shader stages (VS and FS)." NL
				  "Verify that this scenario works as expected. In particular ensure that all generated values are "
				  "unique and" NL "final value in atomic counter buffer objects are as expected.";
	}

	virtual std::string Method()
	{
		return NL "";
	}

	virtual std::string PassCriteria()
	{
		return NL "";
	}

	GLuint counter_buffer_;
	GLuint xfb_buffer_[2];
	GLuint vao_, vbo_;
	GLuint prog_;
	GLuint fbo_, rt_[2];

	virtual long Setup()
	{
		counter_buffer_ = 0;
		xfb_buffer_[0] = xfb_buffer_[1] = 0;
		vao_ = vbo_ = 0;
		prog_		= 0;
		fbo_ = rt_[0] = rt_[1] = 0;
		return NO_ERROR;
	}

	virtual long Run()
	{

		GLint p1, p2, p3;
		glGetIntegerv(GL_MAX_VERTEX_ATOMIC_COUNTER_BUFFERS, &p1);
		glGetIntegerv(GL_MAX_VERTEX_ATOMIC_COUNTERS, &p2);
		glGetIntegerv(GL_MAX_FRAGMENT_ATOMIC_COUNTER_BUFFERS, &p3);
		if (p1 < 8 || p2 < 2 || p3 < 8)
		{
			return NO_ERROR;
		}

		// create program
		const char* src_vs =
			"#version 420 core" NL "layout(location = 0) in vec4 i_vertex;" NL "out uint o_atomic_inc;" NL
			"out uint o_atomic_dec;" NL "layout(binding = 1, offset = 16) uniform atomic_uint ac_counter_inc;" NL
			"layout(binding = 7, offset = 128) uniform atomic_uint ac_counter_dec;" NL "void main() {" NL
			"  gl_Position = i_vertex;" NL "  o_atomic_inc = atomicCounterIncrement(ac_counter_inc);" NL
			"  o_atomic_dec = atomicCounterDecrement(ac_counter_dec);" NL "}";
		const char* src_fs = "#version 420 core" NL "layout(location = 0) out uvec4 o_color[2];" NL
							 "layout(binding = 1, offset = 16) uniform atomic_uint ac_counter_inc;" NL
							 "layout(binding = 7, offset = 128) uniform atomic_uint ac_counter_dec;" NL
							 "void main() {" NL "  o_color[0] = uvec4(atomicCounterIncrement(ac_counter_inc));" NL
							 "  o_color[1] = uvec4(atomicCounterDecrement(ac_counter_dec));" NL "}";
		prog_				   = CreateProgram(src_vs, NULL, NULL, NULL, src_fs, false);
		const char* xfb_var[2] = { "o_atomic_inc", "o_atomic_dec" };
		glTransformFeedbackVaryings(prog_, 2, xfb_var, GL_SEPARATE_ATTRIBS);
		LinkProgram(prog_);

		// create atomic counter buffer
		std::vector<GLuint> init_data(256, 100);
		glGenBuffers(1, &counter_buffer_);
		glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, counter_buffer_);
		glBufferData(GL_ATOMIC_COUNTER_BUFFER, (GLsizeiptr)(init_data.size() * sizeof(GLuint)), &init_data[0],
					 GL_DYNAMIC_COPY);
		glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, 0);

		// create transform feedback buffers
		glGenBuffers(2, xfb_buffer_);
		glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, xfb_buffer_[0]);
		glBufferData(GL_TRANSFORM_FEEDBACK_BUFFER, 1000, NULL, GL_STREAM_COPY);
		glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, xfb_buffer_[1]);
		glBufferData(GL_TRANSFORM_FEEDBACK_BUFFER, 1000, NULL, GL_STREAM_COPY);
		glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, 0);

		// create render targets
		const int s = 8;
		glGenTextures(2, rt_);
		for (int i = 0; i < 2; ++i)
		{
			glBindTexture(GL_TEXTURE_2D, rt_[i]);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_R32UI, s, s, 0, GL_RED_INTEGER, GL_UNSIGNED_INT, NULL);
			glBindTexture(GL_TEXTURE_2D, 0);
		}

		// create fbo
		glGenFramebuffers(1, &fbo_);
		glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
		glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, rt_[0], 0);
		glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, rt_[1], 0);
		const GLenum draw_buffers[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
		glDrawBuffers(2, draw_buffers);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		// create geometry
		CreateTriangle(&vao_, &vbo_, NULL);

		// draw
		glBindBufferRange(GL_ATOMIC_COUNTER_BUFFER, 1, counter_buffer_, 16, 32);
		glBindBufferBase(GL_ATOMIC_COUNTER_BUFFER, 7, counter_buffer_);
		glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, xfb_buffer_[0]);
		glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 1, xfb_buffer_[1]);
		glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
		glViewport(0, 0, s, s);
		glUseProgram(prog_);
		glBindVertexArray(vao_);
		glBeginTransformFeedback(GL_TRIANGLES);
		glDrawArrays(GL_TRIANGLES, 0, 3);
		glEndTransformFeedback();

		// validate
		GLuint data[s * s + 3];
		glReadBuffer(GL_COLOR_ATTACHMENT0);
		glReadPixels(0, 0, s, s, GL_RED_INTEGER, GL_UNSIGNED_INT, data);
		glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, xfb_buffer_[0]);
		glGetBufferSubData(GL_TRANSFORM_FEEDBACK_BUFFER, 0, 3 * sizeof(GLuint), &data[s * s]);
		if (!CheckCounterValues(s * s + 3, data, 100))
			return ERROR;

		glReadBuffer(GL_COLOR_ATTACHMENT1);
		glReadPixels(0, 0, s, s, GL_RED_INTEGER, GL_UNSIGNED_INT, data);
		glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, xfb_buffer_[1]);
		glGetBufferSubData(GL_TRANSFORM_FEEDBACK_BUFFER, 0, 3 * sizeof(GLuint), &data[s * s]);
		if (!CheckCounterValues(s * s + 3, data, 33))
			return ERROR;

		if (!CheckFinalCounterValue(counter_buffer_, 32, 167))
			return ERROR;
		if (!CheckFinalCounterValue(counter_buffer_, 128, 33))
			return ERROR;

		return NO_ERROR;
	}

	virtual long Cleanup()
	{
		glDeleteFramebuffers(1, &fbo_);
		glDeleteTextures(2, rt_);
		glViewport(0, 0, getWindowWidth(), getWindowHeight());
		glDeleteBuffers(1, &counter_buffer_);
		glDeleteBuffers(2, xfb_buffer_);
		glDeleteVertexArrays(1, &vao_);
		glDeleteBuffers(1, &vbo_);
		glDeleteProgram(prog_);
		glUseProgram(0);
		return NO_ERROR;
	}
};

class AdvancedUsageDrawUpdateDraw : public SACSubcaseBase
{
	virtual std::string Title()
	{
		return NL "Update via Draw Call and update via MapBufferRange";
	}

	virtual std::string Purpose()
	{
		return NL "1. Create atomic counter buffers and init them with start values." NL
				  "2. Increment (decrement) buffer values in the shader." NL
				  "3. Map buffers with MapBufferRange command. Increment (decrement) buffer values manually." NL
				  "4. Unmap buffers with UnmapBuffer command." NL
				  "5. Again increment (decrement) buffer values in the shader." NL
				  "Verify that this scenario works as expected and final values in the buffer objects are correct.";
	}

	virtual std::string Method()
	{
		return NL "";
	}

	virtual std::string PassCriteria()
	{
		return NL "";
	}

	GLuint counter_buffer_;
	GLuint vao_, vbo_;
	GLuint prog_, prog2_;
	GLuint fbo_, rt_[2];

	virtual long Setup()
	{
		counter_buffer_ = 0;
		vao_ = vbo_ = 0;
		prog_		= 0;
		fbo_ = rt_[0] = rt_[1] = 0;
		return NO_ERROR;
	}

	virtual long Run()
	{
		// create program
		const char* src_vs = "#version 420 core" NL "layout(location = 0) in vec4 i_vertex;" NL "void main() {" NL
							 "  gl_Position = i_vertex;" NL "}";
		const char* src_fs = "#version 420 core" NL "layout(location = 0) out uvec4 o_color[2];" NL
							 "layout(binding = 0) uniform atomic_uint ac_counter[2];" NL "void main() {" NL
							 "  o_color[0] = uvec4(atomicCounterIncrement(ac_counter[0]));" NL
							 "  o_color[1] = uvec4(atomicCounterDecrement(ac_counter[1]));" NL "}";
		const char* src_fs2 = "#version 420 core" NL "layout(location = 0) out uvec4 o_color[2];" NL
							  "layout(binding = 0) uniform atomic_uint ac_counter[2];" NL "void main() {" NL
							  "  o_color[0] = uvec4(atomicCounter(ac_counter[0]));" NL
							  "  o_color[1] = uvec4(atomicCounter(ac_counter[1]));" NL "}";
		prog_  = CreateProgram(src_vs, NULL, NULL, NULL, src_fs, true);
		prog2_ = CreateProgram(src_vs, NULL, NULL, NULL, src_fs2, true);

		// create atomic counter buffer
		glGenBuffers(1, &counter_buffer_);
		glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, counter_buffer_);
		glBufferData(GL_ATOMIC_COUNTER_BUFFER, 8, NULL, GL_DYNAMIC_COPY);
		glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, 0);

		// create render targets
		const int s = 8;
		glGenTextures(2, rt_);

		for (int i = 0; i < 2; ++i)
		{
			glBindTexture(GL_TEXTURE_2D, rt_[i]);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_R32UI, s, s, 0, GL_RED_INTEGER, GL_UNSIGNED_INT, NULL);
			glBindTexture(GL_TEXTURE_2D, 0);
		}

		// create fbo
		glGenFramebuffers(1, &fbo_);
		glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
		glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, rt_[0], 0);
		glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, rt_[1], 0);
		const GLenum draw_buffers[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
		glDrawBuffers(2, draw_buffers);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		// create geometry
		CreateQuad(&vao_, &vbo_, NULL);

		// init counter buffer
		glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, counter_buffer_);
		unsigned int* ptr = static_cast<unsigned int*>(
			glMapBufferRange(GL_ATOMIC_COUNTER_BUFFER, 0, 8, GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT));
		*ptr++ = 256;
		*ptr++ = 256;
		glUnmapBuffer(GL_ATOMIC_COUNTER_BUFFER);

		// draw
		glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
		glViewport(0, 0, s, s);
		glBindBufferBase(GL_ATOMIC_COUNTER_BUFFER, 0, counter_buffer_);
		glUseProgram(prog_);
		glBindVertexArray(vao_);
		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

		// update counter buffer
		glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, counter_buffer_);
		ptr = static_cast<unsigned int*>(
			glMapBufferRange(GL_ATOMIC_COUNTER_BUFFER, 0, 8, GL_MAP_WRITE_BIT | GL_MAP_READ_BIT));
		*ptr++ += 512;
		*ptr++ += 1024;
		glUnmapBuffer(GL_ATOMIC_COUNTER_BUFFER);

		// draw
		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
		glMemoryBarrier(GL_ATOMIC_COUNTER_BARRIER_BIT);

		// draw
		glUseProgram(prog2_);
		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

		// validate
		GLuint data[s * s];
		glReadBuffer(GL_COLOR_ATTACHMENT0);
		glReadPixels(0, 0, s, s, GL_RED_INTEGER, GL_UNSIGNED_INT, data);
		for (int i = 0; i < s * s; ++i)
		{
			if (data[i] != 896)
			{
				Output("Counter value is %u should be %u.\n", data[i], 896);
				return ERROR;
			}
		}

		glReadBuffer(GL_COLOR_ATTACHMENT1);
		glReadPixels(0, 0, s, s, GL_RED_INTEGER, GL_UNSIGNED_INT, data);
		for (int i = 0; i < s * s; ++i)
		{
			if (data[i] != 1152)
			{
				Output("Counter value is %u should be %u.\n", data[i], 896);
				return ERROR;
			}
		}

		if (!CheckFinalCounterValue(counter_buffer_, 0, 896))
			return ERROR;
		if (!CheckFinalCounterValue(counter_buffer_, 4, 1152))
			return ERROR;

		return NO_ERROR;
	}

	virtual long Cleanup()
	{
		glDeleteFramebuffers(1, &fbo_);
		glDeleteTextures(2, rt_);
		glViewport(0, 0, getWindowWidth(), getWindowHeight());
		glDeleteBuffers(1, &counter_buffer_);
		glDeleteVertexArrays(1, &vao_);
		glDeleteBuffers(1, &vbo_);
		glDeleteProgram(prog_);
		glDeleteProgram(prog2_);
		glUseProgram(0);
		return NO_ERROR;
	}
};

class AdvancedUsageManyCounters : public SACSubcaseBase
{
	virtual std::string Title()
	{
		return NL "Large atomic counters array indexed with uniforms";
	}

	virtual std::string Purpose()
	{
		return NL "Verify that large atomic counters array works as expected when indexed with dynamically uniform "
				  "expressions." NL
				  "Built-ins tested: atomicCounterIncrement, atomicCounterDecrement and atomicCounter.";
	}

	virtual std::string Method()
	{
		return NL "";
	}

	virtual std::string PassCriteria()
	{
		return NL "";
	}

	GLuint counter_buffer_;
	GLuint vao_, vbo_;
	GLuint prog_;
	GLuint fbo_, rt_[8];

	virtual long Setup()
	{
		counter_buffer_ = 0;
		vao_ = vbo_ = 0;
		prog_		= 0;
		fbo_		= 0;
		memset(rt_, 0, sizeof(rt_));
		return NO_ERROR;
	}

	virtual long Run()
	{
		// create program
		const char* src_vs = "#version 420 core" NL "layout(location = 0) in vec4 i_vertex;" NL "void main() {" NL
							 "  gl_Position = i_vertex;" NL "}";
		const char* src_fs =
			"#version 420 core" NL "layout(location = 0) out uvec4 o_color[8];" NL
			"uniform int u_active_counters[8];" NL "layout(binding = 0) uniform atomic_uint ac_counter[8];" NL
			"void main() {" NL "  o_color[0] = uvec4(atomicCounterIncrement(ac_counter[u_active_counters[0]]));" NL
			"  o_color[1] = uvec4(atomicCounterDecrement(ac_counter[u_active_counters[1]]));" NL
			"  o_color[2] = uvec4(atomicCounter(ac_counter[u_active_counters[2]]));" NL
			"  o_color[3] = uvec4(atomicCounterIncrement(ac_counter[u_active_counters[3]]));" NL
			"  o_color[4] = uvec4(atomicCounterDecrement(ac_counter[u_active_counters[4]]));" NL
			"  o_color[5] = uvec4(atomicCounter(ac_counter[u_active_counters[5]]));" NL
			"  o_color[6] = uvec4(atomicCounterIncrement(ac_counter[u_active_counters[6]]));" NL
			"  o_color[7] = uvec4(atomicCounterIncrement(ac_counter[u_active_counters[7]]));" NL "}";
		prog_ = CreateProgram(src_vs, NULL, NULL, NULL, src_fs, true);

		// create atomic counter buffer
		std::vector<GLuint> init_data(8, 1000);
		glGenBuffers(1, &counter_buffer_);
		glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, counter_buffer_);
		glBufferData(GL_ATOMIC_COUNTER_BUFFER, 32, &init_data[0], GL_DYNAMIC_COPY);
		glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, 0);

		// create render targets
		const int s = 8;
		glGenTextures(8, rt_);

		for (int i = 0; i < 8; ++i)
		{
			glBindTexture(GL_TEXTURE_2D, rt_[i]);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_R32UI, s, s, 0, GL_RED_INTEGER, GL_UNSIGNED_INT, NULL);
			glBindTexture(GL_TEXTURE_2D, 0);
		}

		// create fbo
		glGenFramebuffers(1, &fbo_);
		glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
		GLenum draw_buffers[8];
		for (int i = 0; i < 8; ++i)
		{
			glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, rt_[i], 0);
			draw_buffers[i] = GL_COLOR_ATTACHMENT0 + i;
		};
		glDrawBuffers(8, draw_buffers);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		// create geometry
		CreateTriangle(&vao_, &vbo_, NULL);

		// set uniforms
		glUseProgram(prog_);
		glUniform1i(glGetUniformLocation(prog_, "u_active_counters[0]"), 5);
		glUniform1i(glGetUniformLocation(prog_, "u_active_counters[1]"), 2);
		glUniform1i(glGetUniformLocation(prog_, "u_active_counters[2]"), 7);
		glUniform1i(glGetUniformLocation(prog_, "u_active_counters[3]"), 3);
		glUniform1i(glGetUniformLocation(prog_, "u_active_counters[4]"), 0);
		glUniform1i(glGetUniformLocation(prog_, "u_active_counters[5]"), 4);
		glUniform1i(glGetUniformLocation(prog_, "u_active_counters[6]"), 6);
		glUniform1i(glGetUniformLocation(prog_, "u_active_counters[7]"), 1);

		// draw
		glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
		glViewport(0, 0, s, s);
		glBindBufferBase(GL_ATOMIC_COUNTER_BUFFER, 0, counter_buffer_);
		glBindVertexArray(vao_);
		glDrawArrays(GL_TRIANGLES, 0, 3);

		// validate
		GLuint data[s * s];
		glReadBuffer(GL_COLOR_ATTACHMENT0);
		glReadPixels(0, 0, s, s, GL_RED_INTEGER, GL_UNSIGNED_INT, data);
		if (!CheckCounterValues(s * s, data, 1000))
			return ERROR;
		if (!CheckFinalCounterValue(counter_buffer_, 20, 1064))
			return ERROR;

		glReadBuffer(GL_COLOR_ATTACHMENT1);
		glReadPixels(0, 0, s, s, GL_RED_INTEGER, GL_UNSIGNED_INT, data);
		if (!CheckCounterValues(s * s, data, 1000 - 64))
			return ERROR;
		if (!CheckFinalCounterValue(counter_buffer_, 8, 1000 - 64))
			return ERROR;

		glReadBuffer(GL_COLOR_ATTACHMENT2);
		glReadPixels(0, 0, s, s, GL_RED_INTEGER, GL_UNSIGNED_INT, data);
		for (int i = 0; i < s * s; ++i)
			if (data[i] != 1000)
				return ERROR;
		if (!CheckFinalCounterValue(counter_buffer_, 28, 1000))
			return ERROR;

		glReadBuffer(GL_COLOR_ATTACHMENT3);
		glReadPixels(0, 0, s, s, GL_RED_INTEGER, GL_UNSIGNED_INT, data);
		if (!CheckCounterValues(s * s, data, 1000))
			return ERROR;
		if (!CheckFinalCounterValue(counter_buffer_, 12, 1064))
			return ERROR;

		glReadBuffer(GL_COLOR_ATTACHMENT4);
		glReadPixels(0, 0, s, s, GL_RED_INTEGER, GL_UNSIGNED_INT, data);
		if (!CheckCounterValues(s * s, data, 1000 - 64))
			return ERROR;
		if (!CheckFinalCounterValue(counter_buffer_, 0, 1000 - 64))
			return ERROR;

		glReadBuffer(GL_COLOR_ATTACHMENT5);
		glReadPixels(0, 0, s, s, GL_RED_INTEGER, GL_UNSIGNED_INT, data);
		for (int i = 0; i < s * s; ++i)
			if (data[i] != 1000)
				return ERROR;
		if (!CheckFinalCounterValue(counter_buffer_, 16, 1000))
			return ERROR;

		glReadBuffer(GL_COLOR_ATTACHMENT6);
		glReadPixels(0, 0, s, s, GL_RED_INTEGER, GL_UNSIGNED_INT, data);
		if (!CheckCounterValues(s * s, data, 1000))
			return ERROR;
		if (!CheckFinalCounterValue(counter_buffer_, 24, 1064))
			return ERROR;

		glReadBuffer(GL_COLOR_ATTACHMENT7);
		glReadPixels(0, 0, s, s, GL_RED_INTEGER, GL_UNSIGNED_INT, data);
		if (!CheckCounterValues(s * s, data, 1000))
			return ERROR;
		if (!CheckFinalCounterValue(counter_buffer_, 4, 1064))
			return ERROR;

		return NO_ERROR;
	}

	virtual long Cleanup()
	{
		glDeleteFramebuffers(1, &fbo_);
		glDeleteTextures(8, rt_);
		glViewport(0, 0, getWindowWidth(), getWindowHeight());
		glDeleteBuffers(1, &counter_buffer_);
		glDeleteVertexArrays(1, &vao_);
		glDeleteBuffers(1, &vbo_);
		glDeleteProgram(prog_);
		glUseProgram(0);
		return NO_ERROR;
	}
};

class AdvancedUsageSwitchPrograms : public SACSubcaseBase
{
	virtual std::string Title()
	{
		return NL "Switching several program objects with different atomic counters with different bindings";
	}

	virtual std::string Purpose()
	{
		return NL "Verify that each program upadate atomic counter buffer object in appropriate binding point.";
	}

	virtual std::string Method()
	{
		return NL "";
	}

	virtual std::string PassCriteria()
	{
		return NL "";
	}

	GLuint counter_buffer_[8];
	GLuint xfb_buffer_;
	GLuint vao_, vbo_;
	GLuint prog_[8];
	GLuint fbo_, rt_;

	std::string GenVSSrc(int binding, int offset)
	{
		std::ostringstream os;
		os << "#version 420 core" NL "layout(location = 0) in vec4 i_vertex;" NL "out uvec4 o_atomic_value;" NL
			  "layout(binding = "
		   << binding << ", offset = " << offset
		   << ") uniform atomic_uint ac_counter_vs;" NL "void main() {" NL "  gl_Position = i_vertex;" NL
			  "  o_atomic_value = uvec4(atomicCounterIncrement(ac_counter_vs));" NL "}";
		return os.str();
	}
	std::string GenFSSrc(int binding, int offset)
	{
		std::ostringstream os;
		os << "#version 420 core" NL "layout(location = 0) out uvec4 o_color;" NL "layout(binding = " << binding
		   << ", offset = " << offset << ") uniform atomic_uint ac_counter_fs;" NL "void main() {" NL
										 "  o_color = uvec4(atomicCounterIncrement(ac_counter_fs));" NL "}";
		return os.str();
	}

	virtual long Setup()
	{
		memset(counter_buffer_, 0, sizeof(counter_buffer_));
		xfb_buffer_ = 0;
		vao_ = vbo_ = 0;
		memset(prog_, 0, sizeof(prog_));
		fbo_ = rt_ = 0;
		return NO_ERROR;
	}

	virtual long Run()
	{

		GLint p1, p2, p3;
		glGetIntegerv(GL_MAX_VERTEX_ATOMIC_COUNTER_BUFFERS, &p1);
		glGetIntegerv(GL_MAX_VERTEX_ATOMIC_COUNTERS, &p2);
		glGetIntegerv(GL_MAX_FRAGMENT_ATOMIC_COUNTER_BUFFERS, &p3);
		if (p1 < 8 || p2 < 1 || p3 < 8)
		{
			return NO_ERROR;
		}

		// create programs
		for (int i = 0; i < 8; ++i)
		{
			std::string vs_str  = GenVSSrc(i, i * 8);
			std::string fs_str  = GenFSSrc(7 - i, 128 + i * 16);
			const char* src_vs  = vs_str.c_str();
			const char* src_fs  = fs_str.c_str();
			prog_[i]			= CreateProgram(src_vs, NULL, NULL, NULL, src_fs, false);
			const char* xfb_var = "o_atomic_value";
			glTransformFeedbackVaryings(prog_[i], 1, &xfb_var, GL_SEPARATE_ATTRIBS);
			LinkProgram(prog_[i]);
		}

		// create atomic counter buffers
		glGenBuffers(8, counter_buffer_);
		for (int i = 0; i < 8; ++i)
		{
			std::vector<GLuint> init_data(256);
			glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, counter_buffer_[i]);
			glBufferData(GL_ATOMIC_COUNTER_BUFFER, (GLsizeiptr)(init_data.size() * sizeof(GLuint)), &init_data[0],
						 GL_DYNAMIC_COPY);
		}
		glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, 0);

		// create transform feedback buffer
		glGenBuffers(1, &xfb_buffer_);
		glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, xfb_buffer_);
		glBufferData(GL_TRANSFORM_FEEDBACK_BUFFER, 1000, NULL, GL_STREAM_COPY);
		glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, 0);

		// create render target
		const int s = 8;
		glGenTextures(1, &rt_);
		glBindTexture(GL_TEXTURE_2D, rt_);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_R32UI, s, s, 0, GL_RED_INTEGER, GL_UNSIGNED_INT, NULL);
		glBindTexture(GL_TEXTURE_2D, 0);

		// create fbo
		glGenFramebuffers(1, &fbo_);
		glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
		glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, rt_, 0);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		// create geometry
		CreateTriangle(&vao_, &vbo_, NULL);

		// draw
		for (GLuint i = 0; i < 8; ++i)
		{
			glBindBufferBase(GL_ATOMIC_COUNTER_BUFFER, i, counter_buffer_[i]);
		}
		glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, xfb_buffer_);
		glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
		glViewport(0, 0, s, s);
		glBindVertexArray(vao_);

		for (int i = 0; i < 8; ++i)
		{
			glUseProgram(prog_[i]);
			glBeginTransformFeedback(GL_TRIANGLES);
			glDrawArrays(GL_TRIANGLES, 0, 3);
			glEndTransformFeedback();
			glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);

			if (!CheckFinalCounterValue(counter_buffer_[i], i * 8, 3))
				return ERROR;
			if (!CheckFinalCounterValue(counter_buffer_[7 - i], 128 + i * 16, 64))
				return ERROR;
		}
		return NO_ERROR;
	}

	virtual long Cleanup()
	{
		glDeleteFramebuffers(1, &fbo_);
		glDeleteTextures(1, &rt_);
		glViewport(0, 0, getWindowWidth(), getWindowHeight());
		glDeleteBuffers(8, counter_buffer_);
		glDeleteBuffers(1, &xfb_buffer_);
		glDeleteVertexArrays(1, &vao_);
		glDeleteBuffers(1, &vbo_);
		for (int i = 0; i < 8; ++i)
			glDeleteProgram(prog_[i]);
		glUseProgram(0);
		return NO_ERROR;
	}
};

class AdvancedUsageUBO : public SACSubcaseBase
{
	virtual std::string Title()
	{
		return NL "Atomic Counters used to access Uniform Buffer Objects";
	}

	virtual std::string Purpose()
	{
		return NL "Atomic counters are used to access UBOs. In that way each shader invocation can access UBO at "
				  "unique offset." NL
				  "This scenario is a base for some practical algorithms. Verify that it works as expected.";
	}

	virtual std::string Method()
	{
		return NL "";
	}

	virtual std::string PassCriteria()
	{
		return NL "";
	}

	GLuint counter_buffer_;
	GLuint uniform_buffer_;
	GLuint vao_, vbo_;
	GLuint prog_;
	GLuint fbo_, rt_;

	virtual long Setup()
	{
		counter_buffer_ = 0;
		uniform_buffer_ = 0;
		vao_ = vbo_ = 0;
		prog_		= 0;
		fbo_ = rt_ = 0;
		return NO_ERROR;
	}

	virtual long Run()
	{
		// create program
		const char* src_vs = "#version 420 core" NL "layout(location = 0) in vec4 i_vertex;" NL "void main() {" NL
							 "  gl_Position = i_vertex;" NL "}";
		const char* src_fs =
			"#version 420 core" NL "layout(location = 0) out uvec4 o_color;" NL
			"layout(binding = 0, offset = 0) uniform atomic_uint ac_counter;" NL "layout(std140) uniform Data {" NL
			"  uint index[256];" NL "} ub_data;" NL "void main() {" NL
			"  o_color = uvec4(ub_data.index[atomicCounterIncrement(ac_counter)]);" NL "}";
		prog_ = CreateProgram(src_vs, NULL, NULL, NULL, src_fs, true);
		glUniformBlockBinding(prog_, glGetUniformBlockIndex(prog_, "Data"), 1);

		// create atomic counter buffer
		const unsigned int z = 0;
		glGenBuffers(1, &counter_buffer_);
		glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, counter_buffer_);
		glBufferData(GL_ATOMIC_COUNTER_BUFFER, sizeof(z), &z, GL_DYNAMIC_COPY);
		glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, 0);

		// create uniform buffer
		std::vector<UVec4> init_data(256);
		for (GLuint i	= 0; i < 256; ++i)
			init_data[i] = UVec4(i);
		glGenBuffers(1, &uniform_buffer_);
		glBindBuffer(GL_UNIFORM_BUFFER, uniform_buffer_);
		glBufferData(GL_UNIFORM_BUFFER, (GLsizeiptr)(sizeof(UVec4) * init_data.size()), &init_data[0], GL_DYNAMIC_COPY);
		glBindBuffer(GL_UNIFORM_BUFFER, 0);

		// create render targets
		const int s = 16;
		glGenTextures(1, &rt_);
		glBindTexture(GL_TEXTURE_2D, rt_);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_R32UI, s, s, 0, GL_RED_INTEGER, GL_UNSIGNED_INT, NULL);
		glBindTexture(GL_TEXTURE_2D, 0);

		// create fbo
		glGenFramebuffers(1, &fbo_);
		glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
		glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, rt_, 0);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		// create geometry
		CreateQuad(&vao_, &vbo_, NULL);

		// draw
		glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
		glViewport(0, 0, s, s);
		glBindBufferBase(GL_ATOMIC_COUNTER_BUFFER, 0, counter_buffer_);
		glBindBufferBase(GL_UNIFORM_BUFFER, 1, uniform_buffer_);
		glUseProgram(prog_);
		glBindVertexArray(vao_);
		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

		// validate
		GLuint data[s * s];
		glReadBuffer(GL_COLOR_ATTACHMENT0);
		glReadPixels(0, 0, s, s, GL_RED_INTEGER, GL_UNSIGNED_INT, data);
		if (!CheckCounterValues(s * s, data, 0))
			return ERROR;
		if (!CheckFinalCounterValue(counter_buffer_, 0, 256))
			return ERROR;

		return NO_ERROR;
	}

	virtual long Cleanup()
	{
		glDeleteFramebuffers(1, &fbo_);
		glDeleteTextures(1, &rt_);
		glViewport(0, 0, getWindowWidth(), getWindowHeight());
		glDeleteBuffers(1, &counter_buffer_);
		glDeleteBuffers(1, &uniform_buffer_);
		glDeleteVertexArrays(1, &vao_);
		glDeleteBuffers(1, &vbo_);
		glDeleteProgram(prog_);
		glUseProgram(0);
		return NO_ERROR;
	}
};

class AdvancedUsageTBO : public SACSubcaseBase
{
	virtual std::string Title()
	{
		return NL "Atomic Counters used to access Texture Buffer Objects";
	}

	virtual std::string Purpose()
	{
		return NL "Atomic counters are used to access TBOs. In that way each shader invocation can access TBO at "
				  "unique offset." NL
				  "This scenario is a base for some practical algorithms. Verify that it works as expected.";
	}

	virtual std::string Method()
	{
		return NL "";
	}

	virtual std::string PassCriteria()
	{
		return NL "";
	}

	GLuint counter_buffer_;
	GLuint buffer_;
	GLuint texture_;
	GLuint vao_, vbo_;
	GLuint prog_;
	GLuint fbo_, rt_;

	virtual long Setup()
	{
		counter_buffer_ = 0;
		buffer_			= 0;
		texture_		= 0;
		vao_ = vbo_ = 0;
		prog_		= 0;
		fbo_ = rt_ = 0;
		return NO_ERROR;
	}

	virtual long Run()
	{
		// create program
		const char* src_vs = "#version 420 core" NL "layout(location = 0) in vec4 i_vertex;" NL "void main() {" NL
							 "  gl_Position = i_vertex;" NL "}";
		const char* src_fs =
			"#version 420 core" NL "layout(location = 0) out uvec4 o_color;" NL
			"layout(binding = 0, offset = 0) uniform atomic_uint ac_counter;" NL "uniform usamplerBuffer s_buffer;" NL
			"void main() {" NL "  o_color = uvec4(texelFetch(s_buffer, int(atomicCounterIncrement(ac_counter))).r);" NL
			"}";
		prog_ = CreateProgram(src_vs, NULL, NULL, NULL, src_fs, true);

		// create atomic counter buffer
		const unsigned int z = 0;
		glGenBuffers(1, &counter_buffer_);
		glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, counter_buffer_);
		glBufferData(GL_ATOMIC_COUNTER_BUFFER, sizeof(z), &z, GL_DYNAMIC_COPY);
		glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, 0);

		// create buffer
		std::vector<GLuint> init_data(256);
		for (GLuint i	= 0; i < 256; ++i)
			init_data[i] = i;
		glGenBuffers(1, &buffer_);
		glBindBuffer(GL_ARRAY_BUFFER, buffer_);
		glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(sizeof(GLuint) * init_data.size()), &init_data[0], GL_DYNAMIC_COPY);
		glBindBuffer(GL_ARRAY_BUFFER, 0);

		// create texture
		glGenTextures(1, &texture_);
		glBindTexture(GL_TEXTURE_BUFFER, texture_);
		glTexBuffer(GL_TEXTURE_BUFFER, GL_R32UI, buffer_);

		// create render targets
		const int s = 16;
		glGenTextures(1, &rt_);
		glBindTexture(GL_TEXTURE_2D, rt_);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_R32UI, s, s, 0, GL_RED_INTEGER, GL_UNSIGNED_INT, NULL);
		glBindTexture(GL_TEXTURE_2D, 0);

		// create fbo
		glGenFramebuffers(1, &fbo_);
		glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
		glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, rt_, 0);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		// create geometry
		CreateQuad(&vao_, &vbo_, NULL);

		// draw
		glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
		glViewport(0, 0, s, s);
		glBindBufferBase(GL_ATOMIC_COUNTER_BUFFER, 0, counter_buffer_);
		glUseProgram(prog_);
		glBindVertexArray(vao_);
		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

		// validate
		GLuint data[s * s];
		glReadBuffer(GL_COLOR_ATTACHMENT0);
		glReadPixels(0, 0, s, s, GL_RED_INTEGER, GL_UNSIGNED_INT, data);
		if (!CheckCounterValues(s * s, data, 0))
			return ERROR;
		if (!CheckFinalCounterValue(counter_buffer_, 0, 256))
			return ERROR;

		return NO_ERROR;
	}

	virtual long Cleanup()
	{
		glDeleteFramebuffers(1, &fbo_);
		glDeleteTextures(1, &rt_);
		glDeleteTextures(1, &texture_);
		glViewport(0, 0, getWindowWidth(), getWindowHeight());
		glDeleteBuffers(1, &counter_buffer_);
		glDeleteBuffers(1, &buffer_);
		glDeleteVertexArrays(1, &vao_);
		glDeleteBuffers(1, &vbo_);
		glDeleteProgram(prog_);
		glUseProgram(0);
		return NO_ERROR;
	}
};

class NegativeAPI : public SACSubcaseBase
{
	virtual std::string Title()
	{
		return NL "GetActiveAtomicCounterBufferiv";
	}

	virtual std::string Purpose()
	{
		return NL "Verify errors reported by GetActiveAtomicCounterBufferiv command.";
	}

	virtual std::string Method()
	{
		return NL "";
	}

	virtual std::string PassCriteria()
	{
		return NL "";
	}

	GLuint prog_;
	GLuint buffer;

	virtual long Setup()
	{
		prog_ = 0;
		return NO_ERROR;
	}

	virtual long Run()
	{
		// create program
		const char* glsl_vs = "#version 420 core" NL "layout(location = 0) in vec4 i_vertex;" NL "void main() {" NL
							  "  gl_Position = i_vertex;" NL "}";
		const char* glsl_fs = "#version 420 core" NL "layout(location = 0) out uvec4 o_color[4];" NL
							  "layout(binding = 0) uniform atomic_uint ac_counter0;" NL "void main() {" NL
							  "  o_color[0] = uvec4(atomicCounterIncrement(ac_counter0));" NL "}";
		prog_ = CreateProgram(glsl_vs, NULL, NULL, NULL, glsl_fs, true);

		GLint i;
		long  error = NO_ERROR;
		glGetActiveAtomicCounterBufferiv(prog_, 1, GL_ATOMIC_COUNTER_BUFFER_BINDING, &i);
		if (glGetError() != GL_INVALID_VALUE)
		{
			Output("glGetActiveAtomicCounterBufferiv should generate INAVLID_VALUE when"
				   " index is greater than or equal GL_ACTIVE_ATOMIC_COUNTER_BUFFERS.\n");
			error = ERROR;
		}
		glGetActiveAtomicCounterBufferiv(prog_, 7, GL_ATOMIC_COUNTER_BUFFER_BINDING, &i);
		if (glGetError() != GL_INVALID_VALUE)
		{
			Output("glGetActiveAtomicCounterBufferiv should generate INAVLID_VALUE when"
				   " index is greater than or equal GL_ACTIVE_ATOMIC_COUNTER_BUFFERS.\n");
			error = ERROR;
		}
		GLint res;
		glGetIntegerv(GL_MAX_ATOMIC_COUNTER_BUFFER_BINDINGS, &res);
		glGenBuffers(1, &buffer);
		glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, buffer);
		glBindBufferBase(GL_ATOMIC_COUNTER_BUFFER, res, buffer);
		if (glGetError() != GL_INVALID_VALUE)
		{
			Output("glBindBufferBase should generate INVALID_VALUE when"
				   " index is greater than or equal GL_MAX_ATOMIC_COUNTER_BUFFER_BINDINGS.\n");
			error = ERROR;
		}
		glBindBufferRange(GL_ATOMIC_COUNTER_BUFFER, res, buffer, 0, 4);
		if (glGetError() != GL_INVALID_VALUE)
		{
			Output("glBindBufferRange should generate INVALID_VALUE when"
				   " index is greater than or equal GL_MAX_ATOMIC_COUNTER_BUFFER_BINDINGS.\n");
			error = ERROR;
		}
		glBindBufferRange(GL_ATOMIC_COUNTER_BUFFER, res - 1, buffer, 3, 4);
		if (glGetError() != GL_INVALID_VALUE)
		{
			Output("glBindBufferRange should generate INVALID_VALUE when"
				   " <offset> is not a multiple of four\n");
			error = ERROR;
		}
		return error;
	}

	virtual long Cleanup()
	{
		glDeleteProgram(prog_);
		glDeleteBuffers(1, &buffer);
		return NO_ERROR;
	}
};

class NegativeGLSL : public SACSubcaseBase
{
	virtual std::string Title()
	{
		return NL "GLSL errors";
	}

	virtual std::string Purpose()
	{
		return NL "Verify that two different atomic counter uniforms with same binding "
				  "cannot share same offset value.";
	}

	virtual std::string Method()
	{
		return NL "";
	}

	virtual std::string PassCriteria()
	{
		return NL "";
	}

	GLuint prog_;

	virtual long Setup()
	{
		prog_ = 0;
		return NO_ERROR;
	}

	virtual long Run()
	{
		// create program
		const char* glsl_vs = "#version 420 core" NL "layout(location = 0) in vec4 i_vertex;" NL "void main() {" NL
							  "  gl_Position = i_vertex;" NL "}";
		const char* glsl_fs = "#version 420 core" NL "layout(location = 0) out uvec4 o_color[4];" NL
							  "layout(binding = 0, offset = 4) uniform atomic_uint ac_counter0;" NL
							  "layout(binding = 0, offset = 4) uniform atomic_uint ac_counter2;" NL "void main() {" NL
							  "  o_color[0] = uvec4(atomicCounterIncrement(ac_counter0));" NL
							  "  o_color[2] = uvec4(atomicCounterIncrement(ac_counter2));" NL "}";
		prog_ = CreateProgram(glsl_vs, NULL, NULL, NULL, glsl_fs, false);

		GLint status;
		glLinkProgram(prog_);
		glGetProgramiv(prog_, GL_LINK_STATUS, &status);
		if (status == GL_TRUE)
		{
			Output("Link should fail because ac_counter0 and ac_counter2 uses same binding and same offset.\n");
			return ERROR;
		}
		return NO_ERROR;
	}

	virtual long Cleanup()
	{
		glDeleteProgram(prog_);
		return NO_ERROR;
	}
};

class NegativeUBO : public SACSubcaseBase
{
	virtual std::string Title()
	{
		return NL "GLSL errors";
	}

	virtual std::string Purpose()
	{
		return NL "Verify that atomic counters cannot be declared in uniform block.";
	}

	virtual std::string Method()
	{
		return NL "";
	}

	virtual std::string PassCriteria()
	{
		return NL "";
	}

	GLuint prog_;

	virtual long Setup()
	{
		prog_ = 0;
		return NO_ERROR;
	}

	virtual long Run()
	{

		// create program
		const char* glsl_vs = "#version 430 core" NL "layout(location = 0) in vec4 i_vertex;" NL "void main() {" NL
							  "  gl_Position = i_vertex;" NL "}";

		const char* glsl_fs1 =
			"#version 430 core" NL "layout(location = 0) out uvec4 o_color[4];" NL "uniform Block {" NL
			"  layout(binding = 0, offset = 0) uniform atomic_uint ac_counter0;" NL "};" NL "void main() {" NL
			"  o_color[0] = uvec4(atomicCounterIncrement(ac_counter0));" NL "}";

		prog_ = glCreateProgram();

		GLuint sh = glCreateShader(GL_VERTEX_SHADER);
		glAttachShader(prog_, sh);
		glShaderSource(sh, 1, &glsl_vs, NULL);
		glCompileShader(sh);
		GLint status_comp;
		glGetShaderiv(sh, GL_COMPILE_STATUS, &status_comp);
		if (status_comp != GL_TRUE)
		{
			Output("Unexpected error during vertex shader compilation.");
			return ERROR;
		}
		glDeleteShader(sh);

		sh = glCreateShader(GL_FRAGMENT_SHADER);
		glAttachShader(prog_, sh);
		glShaderSource(sh, 1, &glsl_fs1, NULL);
		glCompileShader(sh);
		glGetShaderiv(sh, GL_COMPILE_STATUS, &status_comp);
		glDeleteShader(sh);

		GLint status;
		glLinkProgram(prog_);
		glGetProgramiv(prog_, GL_LINK_STATUS, &status);
		if (status_comp == GL_TRUE && status == GL_TRUE)
		{
			Output("Expected error during fragment shader compilation or linking.");
			return ERROR;
		}
		return NO_ERROR;
	}

	virtual long Cleanup()
	{
		glDeleteProgram(prog_);
		return NO_ERROR;
	}
};

class NegativeSSBO : public SACSubcaseBase
{
	virtual std::string Title()
	{
		return NL "GLSL errors";
	}

	virtual std::string Purpose()
	{
		return NL "Verify that atomic counters cannot be declared in the buffer block.";
	}

	virtual std::string Method()
	{
		return NL "";
	}

	virtual std::string PassCriteria()
	{
		return NL "";
	}

	GLuint prog_;

	virtual long Setup()
	{
		prog_ = 0;
		return NO_ERROR;
	}

	virtual long Run()
	{

		if (!m_context.getContextInfo().isExtensionSupported("GL_ARB_shader_storage_buffer_object"))
		{
			OutputNotSupported("GL_ARB_shader_storage_buffer_object not supported");
			return NO_ERROR;
		}

		// create program
		const char* glsl_vs = "#version 420 core" NL "layout(location = 0) in vec4 i_vertex;" NL "void main() {" NL
							  "  gl_Position = i_vertex;" NL "}";

		const char* glsl_fs1 = "#version 420 core" NL "#extension GL_ARB_shader_storage_buffer_object: require" NL
							   "layout(location = 0) out uvec4 o_color[4];" NL "layout(binding = 0) buffer Buffer {" NL
							   "  layout(binding = 0, offset = 16) uniform atomic_uint ac_counter0;" NL "};" NL
							   "void main() {" NL "  o_color[0] = uvec4(atomicCounterIncrement(ac_counter0));" NL "}";

		prog_ = glCreateProgram();

		GLuint sh = glCreateShader(GL_VERTEX_SHADER);
		glAttachShader(prog_, sh);
		glShaderSource(sh, 1, &glsl_vs, NULL);
		glCompileShader(sh);
		GLint status_comp;
		glGetShaderiv(sh, GL_COMPILE_STATUS, &status_comp);
		if (status_comp != GL_TRUE)
		{
			Output("Unexpected error during vertex shader compilation.");
			return ERROR;
		}
		glDeleteShader(sh);

		sh = glCreateShader(GL_FRAGMENT_SHADER);
		glAttachShader(prog_, sh);
		glShaderSource(sh, 1, &glsl_fs1, NULL);
		glCompileShader(sh);
		glGetShaderiv(sh, GL_COMPILE_STATUS, &status_comp);
		glDeleteShader(sh);

		GLint status;
		glLinkProgram(prog_);
		glGetProgramiv(prog_, GL_LINK_STATUS, &status);
		if (status_comp == GL_TRUE && status == GL_TRUE)
		{
			Output("Expected error during fragment shader compilation or linking.");
			return ERROR;
		}
		return NO_ERROR;
	}

	virtual long Cleanup()
	{
		glDeleteProgram(prog_);
		return NO_ERROR;
	}
};

class NegativeUniform : public SACSubcaseBase
{
	virtual std::string Title()
	{
		return NL "GLSL errors";
	}

	virtual std::string Purpose()
	{
		return NL "Verify that atomicCounterIncrement/atomicCounterDecrement "
				  "cannot be used on normal uniform.";
	}

	virtual std::string Method()
	{
		return NL "";
	}

	virtual std::string PassCriteria()
	{
		return NL "";
	}

	GLuint prog_;

	virtual long Setup()
	{
		prog_ = 0;
		return NO_ERROR;
	}

	virtual long Run()
	{

		// create program
		const char* glsl_vs = "#version 430 core" NL "layout(location = 0) in vec4 i_vertex;" NL "void main() {" NL
							  "  gl_Position = i_vertex;" NL "}";

		const char* glsl_fs1 =
			"#version 430 core" NL "layout(location = 0) out uvec4 o_color[4];" NL "uniform uint ac_counter0;" NL
			"void main() {" NL "  o_color[0] = uvec4(atomicCounterIncrement(ac_counter0));" NL "}";

		prog_ = glCreateProgram();

		GLuint sh = glCreateShader(GL_VERTEX_SHADER);
		glAttachShader(prog_, sh);
		glShaderSource(sh, 1, &glsl_vs, NULL);
		glCompileShader(sh);
		GLint status_comp;
		glGetShaderiv(sh, GL_COMPILE_STATUS, &status_comp);
		if (status_comp != GL_TRUE)
		{
			Output("Unexpected error during vertex shader compilation.");
			return ERROR;
		}
		glDeleteShader(sh);

		sh = glCreateShader(GL_FRAGMENT_SHADER);
		glAttachShader(prog_, sh);
		glShaderSource(sh, 1, &glsl_fs1, NULL);
		glCompileShader(sh);
		glGetShaderiv(sh, GL_COMPILE_STATUS, &status_comp);
		glDeleteShader(sh);

		GLint status;
		glLinkProgram(prog_);
		glGetProgramiv(prog_, GL_LINK_STATUS, &status);
		if (status_comp == GL_TRUE && status == GL_TRUE)
		{
			Output("Expected error during fragment shader compilation or linking.");
			return ERROR;
		}
		return NO_ERROR;
	}

	virtual long Cleanup()
	{
		glDeleteProgram(prog_);
		return NO_ERROR;
	}
};

class NegativeArray : public SACSubcaseBase
{
	virtual std::string Title()
	{
		return NL "GLSL errors";
	}

	virtual std::string Purpose()
	{
		return NL "Verify that atomicCounterIncrement/atomicCounterDecrement "
				  "cannot be used on array of atomic counters.";
	}

	virtual std::string Method()
	{
		return NL "";
	}

	virtual std::string PassCriteria()
	{
		return NL "";
	}

	GLuint prog_;

	virtual long Setup()
	{
		prog_ = 0;
		return NO_ERROR;
	}

	virtual long Run()
	{

		// create program
		const char* glsl_vs = "#version 430 core" NL "layout(location = 0) in vec4 i_vertex;" NL "void main() {" NL
							  "  gl_Position = i_vertex;" NL "}";

		const char* glsl_fs1 = "#version 430 core" NL "layout(location = 0) out uvec4 o_color[4];" NL
							   "layout(binding = 0, offset = 0) uniform atomic_uint ac_counter0[3];" NL
							   "void main() {" NL "  o_color[0] = uvec4(atomicCounterIncrement(ac_counter0));" NL
							   "  o_color[1] = uvec4(atomicCounterDecrement(ac_counter0));" NL "}";

		prog_ = glCreateProgram();

		GLuint sh = glCreateShader(GL_VERTEX_SHADER);
		glAttachShader(prog_, sh);
		glShaderSource(sh, 1, &glsl_vs, NULL);
		glCompileShader(sh);
		GLint status_comp;
		glGetShaderiv(sh, GL_COMPILE_STATUS, &status_comp);
		if (status_comp != GL_TRUE)
		{
			Output("Unexpected error during vertex shader compilation.");
			return ERROR;
		}
		glDeleteShader(sh);

		sh = glCreateShader(GL_FRAGMENT_SHADER);
		glAttachShader(prog_, sh);
		glShaderSource(sh, 1, &glsl_fs1, NULL);
		glCompileShader(sh);
		glGetShaderiv(sh, GL_COMPILE_STATUS, &status_comp);
		glDeleteShader(sh);

		GLint status;
		glLinkProgram(prog_);
		glGetProgramiv(prog_, GL_LINK_STATUS, &status);
		if (status_comp == GL_TRUE && status == GL_TRUE)
		{
			Output("Expected error during fragment shader compilation or linking.");
			return ERROR;
		}
		return NO_ERROR;
	}

	virtual long Cleanup()
	{
		glDeleteProgram(prog_);
		return NO_ERROR;
	}
};

class BasicUsageNoOffset : public SACSubcaseBase
{
	virtual std::string Title()
	{
		return NL "Atomic Counters usage with no offset";
	}

	virtual std::string Purpose()
	{
		return NL "Verify that atomic counters work as expected in the Fragment Shader when declared with no offset "
				  "qualifier in layout." NL "In particular make sure that values returned by GLSL built-in functions" NL
				  "atomicCounterIncrement and atomicCounterDecrement are unique in every shader invocation." NL
				  "Also make sure that the final values in atomic counter buffer objects are as expected.";
	}

	virtual std::string Method()
	{
		return NL "";
	}

	virtual std::string PassCriteria()
	{
		return NL "";
	}

	GLuint counter_buffer_;
	GLuint vao_, vbo_;
	GLuint prog_;
	GLuint fbo_, rt_[2];

	virtual long Setup()
	{
		counter_buffer_ = 0;
		vao_ = vbo_ = 0;
		prog_		= 0;
		fbo_ = rt_[0] = rt_[1] = 0;
		return NO_ERROR;
	}

	virtual long Run()
	{
		// create program
		const char* src_vs = "#version 420 core" NL "layout(location = 0) in vec4 i_vertex;" NL "void main() {" NL
							 "  gl_Position = i_vertex;" NL "}";
		const char* src_fs = "#version 420 core" NL "layout(location = 0) out uvec4 o_color[2];" NL
							 "layout(binding = 0) uniform atomic_uint ac_counter_inc;" NL
							 "layout(binding = 0) uniform atomic_uint ac_counter_dec;" NL "void main() {" NL
							 "  o_color[0] = uvec4(atomicCounterIncrement(ac_counter_inc));" NL
							 "  o_color[1] = uvec4(atomicCounterDecrement(ac_counter_dec));" NL "}";
		prog_ = CreateProgram(src_vs, NULL, NULL, NULL, src_fs, true);

		// create atomic counter buffer
		glGenBuffers(1, &counter_buffer_);
		glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, counter_buffer_);
		glBufferData(GL_ATOMIC_COUNTER_BUFFER, 8, NULL, GL_DYNAMIC_COPY);
		glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, 0);

		// create render targets
		const int s = 8;
		glGenTextures(2, rt_);

		for (int i = 0; i < 2; ++i)
		{
			glBindTexture(GL_TEXTURE_2D, rt_[i]);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_R32UI, s, s, 0, GL_RED_INTEGER, GL_UNSIGNED_INT, NULL);
			glBindTexture(GL_TEXTURE_2D, 0);
		}

		// create fbo
		glGenFramebuffers(1, &fbo_);
		glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
		glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, rt_[0], 0);
		glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, rt_[1], 0);
		const GLenum draw_buffers[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
		glDrawBuffers(2, draw_buffers);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		// create geometry
		CreateQuad(&vao_, &vbo_, NULL);

		// init counter buffer
		glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, counter_buffer_);
		unsigned int* ptr = static_cast<unsigned int*>(
			glMapBufferRange(GL_ATOMIC_COUNTER_BUFFER, 0, 8, GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT));
		*ptr++ = 0;
		*ptr++ = 80;
		glUnmapBuffer(GL_ATOMIC_COUNTER_BUFFER);

		// draw
		glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
		glViewport(0, 0, s, s);
		glBindBufferBase(GL_ATOMIC_COUNTER_BUFFER, 0, counter_buffer_);
		glUseProgram(prog_);
		glBindVertexArray(vao_);
		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

		// validate
		GLuint data[s * s];
		glReadBuffer(GL_COLOR_ATTACHMENT0);
		glReadPixels(0, 0, s, s, GL_RED_INTEGER, GL_UNSIGNED_INT, data);
		if (!CheckCounterValues(s * s, data, 0))
			return ERROR;

		glReadBuffer(GL_COLOR_ATTACHMENT1);
		glReadPixels(0, 0, s, s, GL_RED_INTEGER, GL_UNSIGNED_INT, data);
		if (!CheckCounterValues(s * s, data, 16))
			return ERROR;

		if (!CheckFinalCounterValue(counter_buffer_, 0, 64))
			return ERROR;
		if (!CheckFinalCounterValue(counter_buffer_, 4, 16))
			return ERROR;

		return NO_ERROR;
	}

	virtual long Cleanup()
	{
		glDeleteFramebuffers(1, &fbo_);
		glDeleteTextures(2, rt_);
		glViewport(0, 0, getWindowWidth(), getWindowHeight());
		glDeleteBuffers(1, &counter_buffer_);
		glDeleteVertexArrays(1, &vao_);
		glDeleteBuffers(1, &vbo_);
		glDeleteProgram(prog_);
		glUseProgram(0);
		return NO_ERROR;
	}
};

class BasicUsageCS : public SACSubcaseBase
{
public:
	virtual std::string Title()
	{
		return NL "Atomic Counters usage in the Compute Shader stage";
	}

	virtual std::string Purpose()
	{
		return NL "Verify that atomic counters work as expected in the Compute Shader stage." NL
				  "In particular make sure that values returned by GLSL built-in functions" NL
				  "atomicCounterIncrement and atomicCounterDecrement are unique in every shader invocation." NL
				  "Also make sure that the final values in atomic counter buffer objects are as expected.";
	}

	virtual std::string Method()
	{
		return NL "";
	}

	virtual std::string PassCriteria()
	{
		return NL "";
	}

	GLuint counter_buffer_;
	GLuint prog_;
	GLuint m_buffer;

	virtual long Setup()
	{
		counter_buffer_ = 0;
		prog_			= 0;
		m_buffer		= 0;
		return NO_ERROR;
	}

	GLuint CreateComputeProgram(const std::string& cs)
	{
		const GLuint p = glCreateProgram();

		const char* const kGLSLVer = "#version 420 core \n"
									 "#extension GL_ARB_compute_shader: require \n"
									 "#extension GL_ARB_shader_storage_buffer_object: require \n";

		if (!cs.empty())
		{
			const GLuint sh = glCreateShader(GL_COMPUTE_SHADER);
			glAttachShader(p, sh);
			glDeleteShader(sh);
			const char* const src[2] = { kGLSLVer, cs.c_str() };
			glShaderSource(sh, 2, src, NULL);
			glCompileShader(sh);
		}

		return p;
	}

	bool CheckProgram(GLuint program, bool* compile_error = NULL)
	{
		GLint compile_status = GL_TRUE;
		GLint status;
		glGetProgramiv(program, GL_LINK_STATUS, &status);

		if (status == GL_FALSE)
		{
			GLint attached_shaders;
			glGetProgramiv(program, GL_ATTACHED_SHADERS, &attached_shaders);

			if (attached_shaders > 0)
			{
				std::vector<GLuint> shaders(attached_shaders);
				glGetAttachedShaders(program, attached_shaders, NULL, &shaders[0]);

				for (GLint i = 0; i < attached_shaders; ++i)
				{
					GLenum type;
					glGetShaderiv(shaders[i], GL_SHADER_TYPE, reinterpret_cast<GLint*>(&type));
					switch (type)
					{
					case GL_VERTEX_SHADER:
						Output("*** Vertex Shader ***\n");
						break;
					case GL_TESS_CONTROL_SHADER:
						Output("*** Tessellation Control Shader ***\n");
						break;
					case GL_TESS_EVALUATION_SHADER:
						Output("*** Tessellation Evaluation Shader ***\n");
						break;
					case GL_GEOMETRY_SHADER:
						Output("*** Geometry Shader ***\n");
						break;
					case GL_FRAGMENT_SHADER:
						Output("*** Fragment Shader ***\n");
						break;
					case GL_COMPUTE_SHADER:
						Output("*** Compute Shader ***\n");
						break;
					default:
						Output("*** Unknown Shader ***\n");
						break;
					}

					GLint res;
					glGetShaderiv(shaders[i], GL_COMPILE_STATUS, &res);
					if (res != GL_TRUE)
						compile_status = res;

					// shader source
					GLint length;
					glGetShaderiv(shaders[i], GL_SHADER_SOURCE_LENGTH, &length);
					if (length > 0)
					{
						std::vector<GLchar> source(length);
						glGetShaderSource(shaders[i], length, NULL, &source[0]);
						Output("%s\n", &source[0]);
					}

					// shader info log
					glGetShaderiv(shaders[i], GL_INFO_LOG_LENGTH, &length);
					if (length > 0)
					{
						std::vector<GLchar> log(length);
						glGetShaderInfoLog(shaders[i], length, NULL, &log[0]);
						Output("%s\n", &log[0]);
					}
				}
			}

			// program info log
			GLint length;
			glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
			if (length > 0)
			{
				std::vector<GLchar> log(length);
				glGetProgramInfoLog(program, length, NULL, &log[0]);
				Output("%s\n", &log[0]);
			}
		}

		if (compile_error)
			*compile_error = (compile_status == GL_TRUE ? false : true);
		if (compile_status != GL_TRUE)
			return false;
		return status == GL_TRUE ? true : false;
	}

	virtual long Run()
	{

		if (!m_context.getContextInfo().isExtensionSupported("GL_ARB_compute_shader") ||
			!m_context.getContextInfo().isExtensionSupported("GL_ARB_shader_storage_buffer_object"))
		{
			OutputNotSupported("GL_ARB_shader_storage_buffer_object or GL_ARB_compute_shader not supported");
			return NO_ERROR;
		}

		// create program
		const char* const glsl_cs =
			NL "layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;" NL
			   "layout(binding = 0, offset = 0) uniform atomic_uint ac_counter_inc;" NL
			   "layout(binding = 0, offset = 4) uniform atomic_uint ac_counter_dec;" NL
			   "layout(std430) buffer Output {" NL "  uint data_inc[256];" NL "  uint data_dec[256];" NL "} g_out;" NL
			   "void main() {" NL "  uint offset = 32 * gl_GlobalInvocationID.y + gl_GlobalInvocationID.x;" NL
			   "  g_out.data_inc[offset] = atomicCounterIncrement(ac_counter_inc);" NL
			   "  g_out.data_dec[offset] = atomicCounterDecrement(ac_counter_dec);" NL "}";
		prog_ = CreateComputeProgram(glsl_cs);
		glLinkProgram(prog_);
		if (!CheckProgram(prog_))
			return ERROR;

		// create atomic counter buffer
		glGenBuffers(1, &counter_buffer_);
		glBindBufferBase(GL_ATOMIC_COUNTER_BUFFER, 0, counter_buffer_);
		glBufferData(GL_ATOMIC_COUNTER_BUFFER, 8, NULL, GL_DYNAMIC_COPY);
		glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, 0);

		glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, counter_buffer_);
		unsigned int* ptr = static_cast<unsigned int*>(
			glMapBufferRange(GL_ATOMIC_COUNTER_BUFFER, 0, 8, GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT));
		*ptr++ = 0;
		*ptr++ = 256;
		glUnmapBuffer(GL_ATOMIC_COUNTER_BUFFER);

		glGenBuffers(1, &m_buffer);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, m_buffer);
		glBufferData(GL_SHADER_STORAGE_BUFFER, 512 * sizeof(GLuint), NULL, GL_DYNAMIC_DRAW);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

		glUseProgram(prog_);
		glDispatchCompute(4, 1, 1);

		GLuint data[512];
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_buffer);
		glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);
		glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, 512 * sizeof(GLuint), &data[0]);

		std::sort(data, data + 512);
		for (int i = 0; i < 512; i += 2)
		{
			if (data[i] != data[i + 1])
			{
				Output("Pair of values should be equal, got: %d, %d\n", data[i], data[i + 1]);
				return ERROR;
			}
			if (i < 510 && data[i] == data[i + 2])
			{
				Output("Too many same values found: %d, index: %d\n", data[i], i);
				return ERROR;
			}
		}

		return NO_ERROR;
	}

	virtual long Cleanup()
	{
		glDeleteBuffers(1, &counter_buffer_);
		glDeleteBuffers(1, &m_buffer);
		glDeleteProgram(prog_);
		glUseProgram(0);
		return NO_ERROR;
	}
};

class AdvancedManyDrawCalls : public SACSubcaseBase
{
	virtual std::string Title()
	{
		return NL "Atomic Counters usage in multiple draw calls";
	}

	virtual std::string Purpose()
	{
		return NL "Verify atomic counters behaviour across multiple draw calls.";
	}

	virtual std::string Method()
	{
		return NL "";
	}

	virtual std::string PassCriteria()
	{
		return NL "";
	}

	GLuint counter_buffer_;
	GLuint vao_, vbo_;
	GLuint prog_;
	GLuint fbo_, rt_[2];

	virtual long Setup()
	{
		counter_buffer_ = 0;
		vao_ = vbo_ = 0;
		prog_		= 0;
		fbo_ = rt_[0] = rt_[1] = 0;
		return NO_ERROR;
	}

	virtual long Run()
	{
		// create program
		const char* src_vs = "#version 420 core" NL "layout(location = 0) in vec4 i_vertex;" NL "void main() {" NL
							 "  gl_Position = i_vertex;" NL "}";
		const char* src_fs = "#version 420 core" NL "layout(location = 0) out uvec4 o_color[2];" NL
							 "layout(binding = 0, offset = 0) uniform atomic_uint ac_counter_inc;" NL
							 "layout(binding = 0, offset = 4) uniform atomic_uint ac_counter_dec;" NL "void main() {" NL
							 "  o_color[0] = uvec4(atomicCounterIncrement(ac_counter_inc));" NL
							 "  o_color[1] = uvec4(atomicCounterDecrement(ac_counter_dec));" NL "}";
		prog_ = CreateProgram(src_vs, NULL, NULL, NULL, src_fs, true);

		// create atomic counter buffer
		glGenBuffers(1, &counter_buffer_);
		glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, counter_buffer_);
		glBufferData(GL_ATOMIC_COUNTER_BUFFER, 8, NULL, GL_DYNAMIC_COPY);
		glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, 0);

		// create render targets
		const int s = 8;
		glGenTextures(2, rt_);

		for (int i = 0; i < 2; ++i)
		{
			glBindTexture(GL_TEXTURE_2D, rt_[i]);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_R32UI, s, s, 0, GL_RED_INTEGER, GL_UNSIGNED_INT, NULL);
			glBindTexture(GL_TEXTURE_2D, 0);
		}

		// create fbo
		glGenFramebuffers(1, &fbo_);
		glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
		glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, rt_[0], 0);
		glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, rt_[1], 0);
		const GLenum draw_buffers[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
		glDrawBuffers(2, draw_buffers);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		// create geometry
		CreateQuad(&vao_, &vbo_, NULL);

		// init counter buffer
		glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, counter_buffer_);
		unsigned int* ptr = static_cast<unsigned int*>(
			glMapBufferRange(GL_ATOMIC_COUNTER_BUFFER, 0, 8, GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT));
		*ptr++ = 0;
		*ptr++ = 256;
		glUnmapBuffer(GL_ATOMIC_COUNTER_BUFFER);

		// draw
		glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
		glViewport(0, 0, s, s);
		glBindBufferBase(GL_ATOMIC_COUNTER_BUFFER, 0, counter_buffer_);
		glUseProgram(prog_);
		glBindVertexArray(vao_);

		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
		glMemoryBarrier(GL_ATOMIC_COUNTER_BARRIER_BIT);
		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
		glMemoryBarrier(GL_ATOMIC_COUNTER_BARRIER_BIT);
		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
		glMemoryBarrier(GL_ATOMIC_COUNTER_BARRIER_BIT);
		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

		// validate
		GLuint data[s * s];
		glReadBuffer(GL_COLOR_ATTACHMENT0);
		glReadPixels(0, 0, s, s, GL_RED_INTEGER, GL_UNSIGNED_INT, data);
		if (!CheckCounterValues(s * s, data, s * s * 3))
			return ERROR;

		glReadBuffer(GL_COLOR_ATTACHMENT1);
		glReadPixels(0, 0, s, s, GL_RED_INTEGER, GL_UNSIGNED_INT, data);
		if (!CheckCounterValues(s * s, data, 0))
			return ERROR;

		if (!CheckFinalCounterValue(counter_buffer_, 0, 256))
			return ERROR;
		if (!CheckFinalCounterValue(counter_buffer_, 4, 0))
			return ERROR;

		return NO_ERROR;
	}

	virtual long Cleanup()
	{
		glDeleteFramebuffers(1, &fbo_);
		glDeleteTextures(2, rt_);
		glViewport(0, 0, getWindowWidth(), getWindowHeight());
		glDeleteBuffers(1, &counter_buffer_);
		glDeleteVertexArrays(1, &vao_);
		glDeleteBuffers(1, &vbo_);
		glDeleteProgram(prog_);
		glUseProgram(0);
		return NO_ERROR;
	}
};

class NegativeArithmetic : public SACSubcaseBase
{
	virtual std::string Title()
	{
		return NL "GLSL errors";
	}

	virtual std::string Purpose()
	{
		return NL "Verify that standard arithmetic operations \n"
				  "cannot be performed on atomic counters.";
	}

	virtual std::string Method()
	{
		return NL "";
	}

	virtual std::string PassCriteria()
	{
		return NL "";
	}

	GLuint prog_;

	virtual long Setup()
	{
		prog_ = 0;
		return NO_ERROR;
	}

	virtual long Run()
	{

		// create program
		const char* glsl_vs = "#version 420 core" NL "layout(location = 0) in vec4 i_vertex;" NL "void main() {" NL
							  "  gl_Position = i_vertex;" NL "}";

		const char* glsl_fs1 = "#version 420 core" NL "layout(location = 0) out uvec4 o_color[4];" NL
							   "layout(binding = 0, offset = 0) uniform atomic_uint ac_counter;" NL "void main() {" NL
							   "  o_color[0] = ac_counter++;" NL "}";

		prog_ = glCreateProgram();

		GLuint sh = glCreateShader(GL_VERTEX_SHADER);
		glAttachShader(prog_, sh);
		glShaderSource(sh, 1, &glsl_vs, NULL);
		glCompileShader(sh);
		GLint status_comp;
		glGetShaderiv(sh, GL_COMPILE_STATUS, &status_comp);
		if (status_comp != GL_TRUE)
		{
			Output("Unexpected error during vertex shader compilation.");
			return ERROR;
		}
		glDeleteShader(sh);

		sh = glCreateShader(GL_FRAGMENT_SHADER);
		glAttachShader(prog_, sh);
		glShaderSource(sh, 1, &glsl_fs1, NULL);
		glCompileShader(sh);
		glGetShaderiv(sh, GL_COMPILE_STATUS, &status_comp);
		glDeleteShader(sh);

		GLint status;
		glLinkProgram(prog_);
		glGetProgramiv(prog_, GL_LINK_STATUS, &status);
		if (status_comp == GL_TRUE && status == GL_TRUE)
		{
			Output("Expected error during fragment shader compilation or linking.");
			return ERROR;
		}
		return NO_ERROR;
	}

	virtual long Cleanup()
	{
		glDeleteProgram(prog_);
		return NO_ERROR;
	}
};

class AdvancedManyDrawCalls2 : public SACSubcaseBase
{

	GLuint m_acbo, m_ssbo;
	GLuint m_vao;
	GLuint m_ppo, m_vsp, m_fsp;

	virtual long Setup()
	{
		glGenBuffers(1, &m_acbo);
		glGenBuffers(1, &m_ssbo);
		glGenVertexArrays(1, &m_vao);
		glGenProgramPipelines(1, &m_ppo);
		m_vsp = m_fsp = 0;
		return NO_ERROR;
	}

	virtual long Cleanup()
	{
		glDeleteBuffers(1, &m_acbo);
		glDeleteBuffers(1, &m_ssbo);
		glDeleteVertexArrays(1, &m_vao);
		glDeleteProgramPipelines(1, &m_ppo);
		glDeleteProgram(m_vsp);
		glDeleteProgram(m_fsp);
		return NO_ERROR;
	}

	virtual long Run()
	{

		if (!m_context.getContextInfo().isExtensionSupported("GL_ARB_shader_storage_buffer_object"))
		{
			OutputNotSupported("GL_ARB_shader_storage_buffer_object not supported");
			return NO_ERROR;
		}

		const char* const glsl_vs = "#version 420 core" NL "out gl_PerVertex {" NL "  vec4 gl_Position;" NL "};" NL
									"void main() {" NL "  gl_Position = vec4(0, 0, 0, 1);" NL "}";
		const char* const glsl_fs =
			"#version 420 core" NL "#extension GL_ARB_shader_storage_buffer_object : require" NL
			"layout(binding = 0) uniform atomic_uint g_counter;" NL "layout(std430, binding = 0) buffer Output {" NL
			"  uint g_output[];" NL "};" NL "void main() {" NL "  uint c = atomicCounterIncrement(g_counter);" NL
			"  g_output[c] = c;" NL "}";

		m_vsp = glCreateShaderProgramv(GL_VERTEX_SHADER, 1, &glsl_vs);
		m_fsp = glCreateShaderProgramv(GL_FRAGMENT_SHADER, 1, &glsl_fs);
		if (!CheckProgram(m_vsp) || !CheckProgram(m_fsp))
			return ERROR;

		glUseProgramStages(m_ppo, GL_VERTEX_SHADER_BIT, m_vsp);
		glUseProgramStages(m_ppo, GL_FRAGMENT_SHADER_BIT, m_fsp);

		glBindBufferBase(GL_ATOMIC_COUNTER_BUFFER, 0, m_acbo);
		{
			GLuint data = 0;
			glBufferData(GL_ATOMIC_COUNTER_BUFFER, 4, &data, GL_DYNAMIC_COPY);
		}

		{
			std::vector<GLuint> data(1000, 0xffff);
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, m_ssbo);
			glBufferData(GL_SHADER_STORAGE_BUFFER, (GLsizeiptr)(data.size() * 4), &data[0], GL_DYNAMIC_READ);
		}

		// draw
		glViewport(0, 0, 1, 1);
		glBindProgramPipeline(m_ppo);
		glBindVertexArray(m_vao);
		for (int i = 0; i < 100; ++i)
		{
			glDrawArrays(GL_POINTS, 0, 1);
		}

		glViewport(0, 0, getWindowWidth(), getWindowHeight());

		long status = NO_ERROR;

		{
			GLuint* data;

			glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, m_acbo);
			glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);
			data = static_cast<GLuint*>(glMapBufferRange(GL_ATOMIC_COUNTER_BUFFER, 0, 4, GL_MAP_READ_BIT));
			if (data[0] != 100)
			{
				status = ERROR;
				Output("AC buffer content is %u, sholud be 100.\n", data[0]);
			}
			glUnmapBuffer(GL_ATOMIC_COUNTER_BUFFER);
			glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, 0);

			glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_ssbo);
			glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);
			data = static_cast<GLuint*>(glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, 100 * 4, GL_MAP_READ_BIT));
			std::sort(data, data + 100);
			for (GLuint i = 0; i < 100; ++i)
			{
				if (data[i] != i)
				{
					status = ERROR;
					Output("data[%u] is %u, should be %u.\n", i, data[i], i);
				}
			}
			glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
			glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
		}

		return status;
	}
};

class AdvancedUsageMultipleComputeDispatches : public SACSubcaseBase
{
	GLuint m_acbo, m_ssbo;
	GLuint m_ppo, m_csp;

	virtual long Setup()
	{
		glGenBuffers(1, &m_acbo);
		glGenBuffers(1, &m_ssbo);
		glGenProgramPipelines(1, &m_ppo);
		m_csp = 0;
		return NO_ERROR;
	}

	virtual long Cleanup()
	{
		glDeleteBuffers(1, &m_acbo);
		glDeleteBuffers(1, &m_ssbo);
		glDeleteProgramPipelines(1, &m_ppo);
		glDeleteProgram(m_csp);
		return NO_ERROR;
	}

	virtual long Run()
	{

		if (!m_context.getContextInfo().isExtensionSupported("GL_ARB_compute_shader") ||
			!m_context.getContextInfo().isExtensionSupported("GL_ARB_shader_storage_buffer_object"))
		{
			Output("GL_ARB_compute_shader or GL_ARB_shader_storage_buffer_object not supported, skipping test\n");
			return NO_ERROR;
		}

		// create program
		const char* const glsl_cs =
			"#version 420 core" NL "#extension GL_ARB_compute_shader : require" NL
			"#extension GL_ARB_shader_storage_buffer_object : require" NL "layout(local_size_x = 1) in;" NL
			"layout(binding = 0) uniform atomic_uint g_counter;" NL "layout(std430, binding = 0) buffer Output {" NL
			"  uint g_output[];" NL "};" NL "void main() {" NL "  const uint c = atomicCounterIncrement(g_counter);" NL
			"  g_output[c] = c;" NL "}";

		m_csp = glCreateShaderProgramv(GL_COMPUTE_SHADER, 1, &glsl_cs);
		if (!CheckProgram(m_csp))
			return ERROR;
		glUseProgramStages(m_ppo, GL_COMPUTE_SHADER_BIT, m_csp);

		glBindBufferBase(GL_ATOMIC_COUNTER_BUFFER, 0, m_acbo);
		{
			GLuint data = 0;
			glBufferData(GL_ATOMIC_COUNTER_BUFFER, 4, &data, GL_DYNAMIC_COPY);
		}

		{
			std::vector<GLuint> data(1000, 0xffff);
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, m_ssbo);
			glBufferData(GL_SHADER_STORAGE_BUFFER, (GLsizeiptr)(data.size() * 4), &data[0], GL_DYNAMIC_READ);
		}

		glBindProgramPipeline(m_ppo);
		for (int i = 0; i < 100; ++i)
		{
			glDispatchCompute(1, 1, 1);
		}

		long status = NO_ERROR;

		{
			GLuint* data;

			glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, m_acbo);
			glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);
			data = static_cast<GLuint*>(glMapBufferRange(GL_ATOMIC_COUNTER_BUFFER, 0, 4, GL_MAP_READ_BIT));
			if (data[0] != 100)
			{
				status = ERROR;
				Output("AC buffer content is %u, sholud be 100.\n", data[0]);
			}
			glUnmapBuffer(GL_ATOMIC_COUNTER_BUFFER);
			glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, 0);

			glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_ssbo);
			glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);
			data = static_cast<GLuint*>(glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, 100 * 4, GL_MAP_READ_BIT));
			std::sort(data, data + 100);
			for (GLuint i = 0; i < 100; ++i)
			{
				if (data[i] != i)
				{
					status = ERROR;
					Output("data[%u] is %u, should be %u.\n", i, data[i], i);
				}
			}
			glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
			glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
		}

		return status;
	}
};

class BasicGLSLBuiltIn : public BasicUsageCS
{
public:
	virtual std::string Title()
	{
		return NL "gl_Max* Check";
	}

	virtual std::string Purpose()
	{
		return NL "Verify that gl_Max*Counters and gl_Max*Bindings exist in glsl and their values are no lower" NL
				  "than minimum required by the spec and are no different from their GL_MAX_* counterparts.";
	}

	GLuint prog_;
	GLuint m_buffer;

	virtual long Setup()
	{
		prog_	= 0;
		m_buffer = 0;
		return NO_ERROR;
	}

	virtual long Run()
	{
		if (!m_context.getContextInfo().isExtensionSupported("GL_ARB_compute_shader") ||
			!m_context.getContextInfo().isExtensionSupported("GL_ARB_shader_storage_buffer_object"))
		{
			Output("GL_ARB_compute_shader or GL_ARB_shader_storage_buffer_object not supported, skipping test\n");
			return NO_ERROR;
		}

		// create program
		const char* const glsl_cs = NL
			"layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;" NL "layout(std430) buffer Output {" NL
			"  uint data;" NL "} g_out;" NL "uniform int m_vac;" NL "uniform int m_fac;" NL "uniform int m_csac;" NL
			"uniform int m_cac;" NL "uniform int m_abuf;" NL "uniform int m_tcac;" NL "uniform int m_teac;" NL
			"uniform int m_gac;" NL "void main() {" NL "  uint res = 1u;" NL
			"  if (gl_MaxVertexAtomicCounters < 0 || gl_MaxVertexAtomicCounters != m_vac)" NL "     res = res * 2u;" NL
			"  if (gl_MaxFragmentAtomicCounters < 0 || gl_MaxFragmentAtomicCounters != m_fac)" NL
			"     res = res * 3u;" NL
			"  if (gl_MaxComputeAtomicCounters < 8 || gl_MaxComputeAtomicCounters != m_csac)" NL
			"     res = res * 5u;" NL
			"  if (gl_MaxCombinedAtomicCounters < 8 || gl_MaxCombinedAtomicCounters != m_cac)" NL
			"     res = res * 7u;" NL
			"  if (gl_MaxAtomicCounterBindings < 1 || gl_MaxAtomicCounterBindings != m_abuf)" NL
			"     res = res * 11u;" NL
			"  if (gl_MaxTessControlAtomicCounters < 0 || gl_MaxTessControlAtomicCounters != m_tcac)" NL
			"     res = res * 13u;" NL
			"  if (gl_MaxTessEvaluationAtomicCounters < 0 || gl_MaxTessEvaluationAtomicCounters != m_teac)" NL
			"     res = res * 17u;" NL
			"  if (gl_MaxGeometryAtomicCounters < 0 || gl_MaxGeometryAtomicCounters != m_gac)" NL
			"     res = res * 19u;" NL "  g_out.data = res;" NL "}";

		prog_ = CreateComputeProgram(glsl_cs);
		glLinkProgram(prog_);
		if (!CheckProgram(prog_))
			return ERROR;

		glUseProgram(prog_);
		int m_vac;
		int m_fac;
		int m_csac;
		int m_cac;
		int m_abuf;
		int m_tcac;
		int m_teac;
		int m_gac;
		glGetIntegerv(GL_MAX_VERTEX_ATOMIC_COUNTERS, &m_vac);
		glUniform1i(glGetUniformLocation(prog_, "m_vac"), m_vac);
		glGetIntegerv(GL_MAX_FRAGMENT_ATOMIC_COUNTERS, &m_fac);
		glUniform1i(glGetUniformLocation(prog_, "m_fac"), m_fac);
		glGetIntegerv(GL_MAX_COMPUTE_ATOMIC_COUNTERS, &m_csac);
		glUniform1i(glGetUniformLocation(prog_, "m_csac"), m_csac);
		glGetIntegerv(GL_MAX_COMBINED_ATOMIC_COUNTERS, &m_cac);
		glUniform1i(glGetUniformLocation(prog_, "m_cac"), m_cac);
		glGetIntegerv(GL_MAX_ATOMIC_COUNTER_BUFFER_BINDINGS, &m_abuf);
		glUniform1i(glGetUniformLocation(prog_, "m_abuf"), m_abuf);
		glGetIntegerv(GL_MAX_TESS_CONTROL_ATOMIC_COUNTERS, &m_tcac);
		glUniform1i(glGetUniformLocation(prog_, "m_tcac"), m_tcac);
		glGetIntegerv(GL_MAX_TESS_EVALUATION_ATOMIC_COUNTERS, &m_teac);
		glUniform1i(glGetUniformLocation(prog_, "m_teac"), m_teac);
		glGetIntegerv(GL_MAX_GEOMETRY_ATOMIC_COUNTERS, &m_gac);
		glUniform1i(glGetUniformLocation(prog_, "m_gac"), m_gac);

		glGenBuffers(1, &m_buffer);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, m_buffer);
		glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(GLuint), NULL, GL_DYNAMIC_DRAW);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

		glDispatchCompute(1, 1, 1);

		long	error = NO_ERROR;
		GLuint* data;
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_buffer);
		glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);
		data = static_cast<GLuint*>(glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, sizeof(GLuint), GL_MAP_READ_BIT));
		if (data[0] != 1u)
		{
			Output("Expected 1, got: %d", data[0]);
			error = ERROR;
		}
		glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

		return error;
	}

	virtual long Cleanup()
	{
		glDeleteBuffers(1, &m_buffer);
		glDeleteProgram(prog_);
		glUseProgram(0);
		return NO_ERROR;
	}
};

ShaderAtomicCountersTests::ShaderAtomicCountersTests(deqp::Context& context)
	: TestCaseGroup(context, "shader_atomic_counters", "")
{
}

ShaderAtomicCountersTests::~ShaderAtomicCountersTests(void)
{
}

void ShaderAtomicCountersTests::init()
{
	using namespace deqp;
	setOutput(m_context.getTestContext().getLog());
	addChild(
		new TestSubcase(m_context, "advanced-usage-many-counters", TestSubcase::Create<AdvancedUsageManyCounters>));
	addChild(new TestSubcase(m_context, "basic-buffer-operations", TestSubcase::Create<BasicBufferOperations>));
	addChild(new TestSubcase(m_context, "basic-buffer-state", TestSubcase::Create<BasicBufferState>));
	addChild(new TestSubcase(m_context, "basic-buffer-bind", TestSubcase::Create<BasicBufferBind>));
	addChild(new TestSubcase(m_context, "basic-program-max", TestSubcase::Create<BasicProgramMax>));
	addChild(new TestSubcase(m_context, "basic-program-query", TestSubcase::Create<BasicProgramQuery>));
	addChild(new TestSubcase(m_context, "basic-usage-simple", TestSubcase::Create<BasicUsageSimple>));
	addChild(new TestSubcase(m_context, "basic-usage-no-offset", TestSubcase::Create<BasicUsageNoOffset>));
	addChild(new TestSubcase(m_context, "basic-usage-fs", TestSubcase::Create<BasicUsageFS>));
	addChild(new TestSubcase(m_context, "basic-usage-vs", TestSubcase::Create<BasicUsageVS>));
	addChild(new TestSubcase(m_context, "basic-usage-gs", TestSubcase::Create<BasicUsageGS>));
	addChild(new TestSubcase(m_context, "basic-usage-tes", TestSubcase::Create<BasicUsageTES>));
	addChild(new TestSubcase(m_context, "basic-usage-cs", TestSubcase::Create<BasicUsageCS>));
	addChild(new TestSubcase(m_context, "basic-glsl-built-in", TestSubcase::Create<BasicGLSLBuiltIn>));
	addChild(new TestSubcase(m_context, "advanced-usage-multi-stage", TestSubcase::Create<AdvancedUsageMultiStage>));
	addChild(new TestSubcase(m_context, "advanced-usage-draw-update-draw",
							 TestSubcase::Create<AdvancedUsageDrawUpdateDraw>));
	addChild(
		new TestSubcase(m_context, "advanced-usage-switch-programs", TestSubcase::Create<AdvancedUsageSwitchPrograms>));
	addChild(new TestSubcase(m_context, "advanced-usage-ubo", TestSubcase::Create<AdvancedUsageUBO>));
	addChild(new TestSubcase(m_context, "advanced-usage-tbo", TestSubcase::Create<AdvancedUsageTBO>));
	addChild(new TestSubcase(m_context, "advanced-usage-many-draw-calls", TestSubcase::Create<AdvancedManyDrawCalls>));
	addChild(
		new TestSubcase(m_context, "advanced-usage-many-draw-calls2", TestSubcase::Create<AdvancedManyDrawCalls2>));
	addChild(new TestSubcase(m_context, "advanced-usage-many-dispatches",
							 TestSubcase::Create<AdvancedUsageMultipleComputeDispatches>));
	addChild(new TestSubcase(m_context, "negative-api", TestSubcase::Create<NegativeAPI>));
	addChild(new TestSubcase(m_context, "negative-glsl", TestSubcase::Create<NegativeGLSL>));
	addChild(new TestSubcase(m_context, "negative-ssbo", TestSubcase::Create<NegativeSSBO>));
	addChild(new TestSubcase(m_context, "negative-ubo", TestSubcase::Create<NegativeUBO>));
	addChild(new TestSubcase(m_context, "negative-uniform", TestSubcase::Create<NegativeUniform>));
	addChild(new TestSubcase(m_context, "negative-array", TestSubcase::Create<NegativeArray>));
	addChild(new TestSubcase(m_context, "negative-arithmetic", TestSubcase::Create<NegativeArithmetic>));
}

} // namespace gl4cts
