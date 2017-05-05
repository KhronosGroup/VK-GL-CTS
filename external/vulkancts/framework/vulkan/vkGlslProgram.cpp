/*-------------------------------------------------------------------------
 * Vulkan CTS Framework
 * --------------------
 *
 * Copyright (c) 2017 Google Inc.
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
 * \brief GLSL source program.
 *//*--------------------------------------------------------------------*/

#include "vkGlslProgram.hpp"

#include "tcuTestLog.hpp"

namespace vk
{

tcu::TestLog& operator<< (tcu::TestLog& log, const GlslSource& glslSource)
{
	log << tcu::TestLog::ShaderProgram(false, "(Source only)");

	try
	{
		for (int shaderType = 0; shaderType < glu::SHADERTYPE_LAST; shaderType++)
		{
			for (size_t shaderNdx = 0; shaderNdx < glslSource.sources[shaderType].size(); shaderNdx++)
			{
				log << tcu::TestLog::Shader(glu::getLogShaderType((glu::ShaderType)shaderType),
											glslSource.sources[shaderType][shaderNdx],
											false, "");
			}
		}
	}
	catch (...)
	{
		log << tcu::TestLog::EndShaderProgram;
		throw;
	}

	log << tcu::TestLog::EndShaderProgram;

	return log;
}

} // vk
