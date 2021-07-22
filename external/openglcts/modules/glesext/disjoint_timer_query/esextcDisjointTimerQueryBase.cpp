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
 * \file  esextDisjointTimerQueryBase.cpp
 * \brief Base class for Timer query tests
 */ /*-------------------------------------------------------------------*/

#include "esextcDisjointTimerQueryBase.hpp"
#include "gluContextInfo.hpp"
#include "gluDefs.hpp"
#include "glwEnums.hpp"
#include "glwFunctions.hpp"
#include "tcuTestLog.hpp"
#include <vector>

namespace glcts
{

/** Constructor
 *
 *  @param context     Test context
 *  @param name        Test case's name
 *  @param description Test case's description
 **/
DisjointTimerQueryBase::DisjointTimerQueryBase (Context& context, const ExtParameters& extParams,
											    const char* name, const char* description)
	: TestCaseBase(context, extParams, name, description)
{
	glGenQueriesEXT				= (glGenQueriesEXTFunc)context.getRenderContext().getProcAddress("glGenQueriesEXT");
	glDeleteQueriesEXT			= (glDeleteQueriesEXTFunc)context.getRenderContext().getProcAddress("glDeleteQueriesEXT");
	glIsQueryEXT				= (glIsQueryEXTFunc)context.getRenderContext().getProcAddress("glIsQueryEXT");
	glBeginQueryEXT				= (glBeginQueryEXTFunc)context.getRenderContext().getProcAddress("glBeginQueryEXT");
	glEndQueryEXT				= (glEndQueryEXTFunc)context.getRenderContext().getProcAddress("glEndQueryEXT");
	glQueryCounterEXT			= (glQueryCounterEXTFunc)context.getRenderContext().getProcAddress("glQueryCounterEXT");
	glGetQueryivEXT				= (glGetQueryivEXTFunc)context.getRenderContext().getProcAddress("glGetQueryivEXT");
	glGetQueryObjectivEXT		= (glGetQueryObjectivEXTFunc)context.getRenderContext().getProcAddress("glGetQueryObjectivEXT");
	glGetQueryObjectuivEXT		= (glGetQueryObjectuivEXTFunc)context.getRenderContext().getProcAddress("glGetQueryObjectuivEXT");
	glGetQueryObjecti64vEXT		= (glGetQueryObjecti64vEXTFunc)context.getRenderContext().getProcAddress("glGetQueryObjecti64vEXT");
	glGetQueryObjectui64vEXT	= (glGetQueryObjectui64vEXTFunc)context.getRenderContext().getProcAddress("glGetQueryObjectui64vEXT");
	glGetInteger64vEXT			= (glGetInteger64vEXTFunc)context.getRenderContext().getProcAddress("glGetInteger64vEXT");
}

} // namespace glcts
