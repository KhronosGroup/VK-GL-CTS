#ifndef _VKGLSLTOSPIRV_HPP
#define _VKGLSLTOSPIRV_HPP
/*-------------------------------------------------------------------------
 * Vulkan CTS Framework
 * --------------------
 *
 * Copyright (c) 2015 Google Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and/or associated documentation files (the
 * "Materials"), to deal in the Materials without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Materials, and to
 * permit persons to whom the Materials are furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice(s) and this permission notice shall be
 * included in all copies or substantial portions of the Materials.
 *
 * THE MATERIALS ARE PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * MATERIALS OR THE USE OR OTHER DEALINGS IN THE MATERIALS.
 *
 *//*!
 * \file
 * \brief GLSL to SPIR-V.
 *//*--------------------------------------------------------------------*/

#include "vkDefs.hpp"
#include "vkPrograms.hpp"
#include "gluShaderProgram.hpp"

#include <ostream>

namespace vk
{

/*--------------------------------------------------------------------*//*!
 * \brief Compile GLSL program to SPIR-V binary
 * \param src
 * \param dst
 * \param buildInfo
 * \return True if compilation and linking succeeded, false otherwise
 *
 * If deqp was built without glslang (and thus compiler is not available)
 * tcu::NotSupportedError will be thrown instead.
 *
 * \note No linking is currently supported so src may contain source
 *       for only one shader stage.
 *//*--------------------------------------------------------------------*/
bool	compileGlslToSpirV		(const glu::ProgramSources& src, std::vector<deUint32>* dst, glu::ShaderProgramInfo* buildInfo);

} // vk

#endif // _VKGLSLTOSPIRV_HPP
