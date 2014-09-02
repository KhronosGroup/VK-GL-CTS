/*-------------------------------------------------------------------------
 * drawElements Quality Program EGL Module
 * ---------------------------------------
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
 * \brief glw::FunctionLoader using eglGetProcAddress() and tcu::Library.
 *//*--------------------------------------------------------------------*/

#include "egluGLFunctionLoader.hpp"
#include "egluHeaderWrapper.hpp"

namespace eglu
{

GLFunctionLoader::GLFunctionLoader (const tcu::FunctionLibrary* library)
	: m_library(library)
{
}

glw::GenericFuncType GLFunctionLoader::get (const char* name) const
{
	glw::GenericFuncType func = (glw::GenericFuncType)m_library->getFunction(name);

	if (!func)
		return (glw::GenericFuncType)eglGetProcAddress(name);
	else
		return func;
}

} // eglu
