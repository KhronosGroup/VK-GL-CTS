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
 * \brief SPIR-V assembly to binary.
 *//*--------------------------------------------------------------------*/

#include "vkSpirVAsm.hpp"
#include "vkSpirVProgram.hpp"
#include "deArrayUtil.hpp"
#include "deMemory.h"
#include "deClock.h"
#include "qpDebugOut.h"

#if defined(DEQP_HAVE_SPIRV_TOOLS)
#	include "deSingleton.h"

#	include "libspirv/libspirv.h"
#endif

namespace vk
{

using std::string;
using std::vector;

#if defined(DEQP_HAVE_SPIRV_TOOLS)


void assembleSpirV (const SpirVAsmSource* program, std::vector<deUint8>* dst, SpirVProgramInfo* buildInfo)
{
	spv_context context = spvContextCreate();

	const std::string&	spvSource			= program->program.str();
	spv_binary			binary				= DE_NULL;
	spv_diagnostic		diagnostic			= DE_NULL;
	const deUint64		compileStartTime	= deGetMicroseconds();
	const spv_result_t	compileOk			= spvTextToBinary(context, spvSource.c_str(), spvSource.size(), &binary, &diagnostic);

	{
		buildInfo->source			= program;
		buildInfo->infoLog			= diagnostic? diagnostic->error : ""; // \todo [2015-07-13 pyry] Include debug log?
		buildInfo->compileTimeUs	= deGetMicroseconds() - compileStartTime;
		buildInfo->compileOk		= (compileOk == SPV_SUCCESS);
	}

	if (compileOk != SPV_SUCCESS)
		TCU_FAIL("Failed to compile shader");

	dst->resize((int)binary->wordCount * sizeof(deUint32));
#if (DE_ENDIANNESS == DE_LITTLE_ENDIAN)
	deMemcpy(&(*dst)[0], &binary->code[0], dst->size());
#else
#	error "Big-endian not supported"
#endif
	spvBinaryDestroy(binary);
	spvDiagnosticDestroy(diagnostic);
	spvContextDestroy(context);
	return;
}

#else // defined(DEQP_HAVE_SPIRV_TOOLS)

void assembleSpirV (const SpirVAsmSource*, std::vector<deUint8>*, SpirVProgramInfo*)
{
	TCU_THROW(NotSupportedError, "SPIR-V assembly not supported (DEQP_HAVE_SPIRV_TOOLS not defined)");
}

#endif

} // vk
