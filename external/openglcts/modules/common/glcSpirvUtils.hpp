#ifndef _GLCSPIRVUTILS_HPP
#define _GLCSPIRVUTILS_HPP
/*-------------------------------------------------------------------------
 * OpenGL Conformance Test Suite
 * -----------------------------
 *
 * Copyright (c) 2017-2019 The Khronos Group Inc.
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
 * \file  glcSpirvUtils.hpp
 * \brief Utility functions for using Glslang and Spirv-tools to work with
 *  SPIR-V shaders.
 */ /*-------------------------------------------------------------------*/
#include "glcContext.hpp"
#include "gluShaderProgram.hpp"
#include <map>
#include <vector>

namespace glc
{
typedef std::map<std::string, std::vector<std::string> > SpirVMapping;

namespace spirvUtils
{

enum SpirvVersion
{
	SPIRV_VERSION_1_0	= 0,	//!< SPIR-V 1.0
	SPIRV_VERSION_1_1	= 1,	//!< SPIR-V 1.1
	SPIRV_VERSION_1_2	= 2,	//!< SPIR-V 1.2
	SPIRV_VERSION_1_3	= 3,	//!< SPIR-V 1.3

	SPIRV_VERSION_LAST
};

void checkGlSpirvSupported(deqp::Context& m_context);

glu::ShaderBinary makeSpirV(tcu::TestLog& log, glu::ShaderSource source, SpirvVersion version = SPIRV_VERSION_1_0);

void spirvAssemble(glu::ShaderBinaryDataType& dst, const std::string& src);
void spirvDisassemble(std::string& dst, const glu::ShaderBinaryDataType& src);
bool spirvValidate(glu::ShaderBinaryDataType& dst, bool throwOnError);

bool verifyMappings(std::string glslSource, std::string spirVSource, SpirVMapping& mappings, bool anyOf);
}
}

#endif // _GLCSPIRVUTILS_HPP