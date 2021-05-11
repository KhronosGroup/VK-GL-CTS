#ifndef _ESEXTCDISJOINTTIMERQUERYBASE_HPP
#define _ESEXTCDISJOINTTIMERQUERYBASE_HPP
/*-------------------------------------------------------------------------
 * OpenGL Conformance Test Suite
 * -----------------------------
 *
 * Copyright (c) 2021 The Khronos Group Inc.
 * Copyright (c) 2021 Google LLC
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

/*!
 * \file  esextcDisjointTimerQueryBase.hpp
 * \brief Base class for timer query tests
 */ /*-------------------------------------------------------------------*/

#include "../esextcTestCaseBase.hpp"
#include "glw.h"

namespace glcts
{

class DisjointTimerQueryBase : public TestCaseBase
{
public:
	DisjointTimerQueryBase			(Context& context, const ExtParameters& extParams, const char* name,
									 const char* description);

	virtual ~DisjointTimerQueryBase	()
	{
	}

protected:
	typedef void		(*glGenQueriesEXTFunc)(GLsizei n, GLuint* ids);
	typedef void		(*glDeleteQueriesEXTFunc)(GLsizei n, const GLuint* ids);
	typedef GLboolean	(*glIsQueryEXTFunc)(GLuint id);
	typedef void		(*glBeginQueryEXTFunc)(GLenum target, GLuint id);
	typedef void		(*glEndQueryEXTFunc)(GLenum target);
	typedef void		(*glQueryCounterEXTFunc)(GLuint id, GLenum target);
	typedef void		(*glGetQueryivEXTFunc)(GLenum target, GLenum pname, GLint* params);
	typedef void		(*glGetQueryObjectivEXTFunc)(GLuint id, GLenum pname, GLint* params);
	typedef void		(*glGetQueryObjectuivEXTFunc)(GLuint id, GLenum pname, GLuint* params);
	typedef void		(*glGetQueryObjecti64vEXTFunc)(GLuint id, GLenum pname, GLint64* params);
	typedef void		(*glGetQueryObjectui64vEXTFunc)(GLuint id, GLenum pname, GLuint64* params);
	typedef void		(*glGetInteger64vEXTFunc)(GLenum pname, GLint64* data);

	glGenQueriesEXTFunc				glGenQueriesEXT;
	glDeleteQueriesEXTFunc			glDeleteQueriesEXT;
	glIsQueryEXTFunc				glIsQueryEXT;
	glBeginQueryEXTFunc				glBeginQueryEXT;
	glEndQueryEXTFunc				glEndQueryEXT;
	glQueryCounterEXTFunc			glQueryCounterEXT;
	glGetQueryivEXTFunc				glGetQueryivEXT;
	glGetQueryObjectivEXTFunc		glGetQueryObjectivEXT;
	glGetQueryObjectuivEXTFunc		glGetQueryObjectuivEXT;
	glGetQueryObjecti64vEXTFunc		glGetQueryObjecti64vEXT;
	glGetQueryObjectui64vEXTFunc	glGetQueryObjectui64vEXT;
	glGetInteger64vEXTFunc			glGetInteger64vEXT;
};

} // namespace glcts

#endif // _ESEXTCDISJOINTTIMERQUERYBASE_HPP
