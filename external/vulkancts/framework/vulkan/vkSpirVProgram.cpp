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
 * The Materials are Confidential Information as defined by the
 * Khronos Membership Agreement until designated non-confidential by
 * Khronos, at which point this condition clause shall be removed.
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
 * \brief Spirv program and binary info.
 *//*--------------------------------------------------------------------*/

#include "vkSpirVProgram.hpp"

#include "tcuTestLog.hpp"

namespace vk
{

tcu::TestLog& operator<< (tcu::TestLog& log, const SpirVProgramInfo& shaderInfo)
{
	log << tcu::TestLog::ShaderProgram(shaderInfo.compileOk , shaderInfo.infoLog) << tcu::TestLog::EndShaderProgram;

	// Write statistics
	log << tcu::TestLog::Float(	"SpirVAssemblyTime",
								"SpirV assembly time",
								"ms", QP_KEY_TAG_TIME, (float)shaderInfo.compileTimeUs / 1000.0f);
	return log;
}

tcu::TestLog& operator<< (tcu::TestLog& log, const SpirVAsmSource& source)
{
	log << tcu::TestLog::KernelSource(source.program.str());

	return log;
}

} // vk
