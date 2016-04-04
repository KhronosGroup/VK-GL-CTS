#ifndef _VKGLSLTOSPIRV_HPP
#define _VKGLSLTOSPIRV_HPP
/*-------------------------------------------------------------------------
 * Vulkan CTS Framework
 * --------------------
 *
 * Copyright (c) 2015 Google Inc.
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
 * \brief GLSL to SPIR-V.
 *//*--------------------------------------------------------------------*/

#include "vkDefs.hpp"
#include "vkPrograms.hpp"
#include "gluShaderProgram.hpp"

#include <ostream>

namespace vk
{

//! Compile GLSL program to SPIR-V. Will fail with NotSupportedError if compiler is not available.
void	glslToSpirV			(const glu::ProgramSources& src, std::vector<deUint8>* dst, glu::ShaderProgramInfo* buildInfo);

//! Disassemble SPIR-V binary
void	disassembleSpirV	(size_t binarySize, const deUint8* binary, std::ostream* dst);

} // vk

#endif // _VKGLSLTOSPIRV_HPP
