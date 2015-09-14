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

namespace
{
static volatile deSingletonState	s_spirvInitState	= DE_SINGLETON_STATE_NOT_INITIALIZED;
static			spv_opcode_table	s_spirvOpcodeTable;
static			spv_operand_table	s_spirvOperandTable;
static			spv_ext_inst_table	s_spirvExtInstTable;

void initSpirVTools (void*)
{
	if (spvOpcodeTableGet(&s_spirvOpcodeTable) != SPV_SUCCESS)
			 TCU_THROW(InternalError, "Cannot get opcode table for assembly");

	if (spvOperandTableGet(&s_spirvOperandTable) != SPV_SUCCESS)
			 TCU_THROW(InternalError, "Cannot get operand table for assembly");

	if (spvExtInstTableGet(&s_spirvExtInstTable) != SPV_SUCCESS)
			 TCU_THROW(InternalError, "Cannot get external instruction table for assembly");)
}

void prepareSpirvTools (void)
{
	deInitSingleton(&s_spirvInitState, initSpirVTools, DE_NULL);
}

} // anonymous

void assembleSpirV (const SpirVAsmSource* program, std::vector<deUint8>* dst, SpirVProgramInfo* buildInfo)
{
	prepareSpirvTools();

	const std::string&	spvSource			= program->program.str();
	spv_binary			binary				= DE_NULL;
	spv_diagnostic		diagnostic			= DE_NULL;
	const deUint64		compileStartTime	= deGetMicroseconds();
	const spv_result_t	compileOk			= spvTextToBinary(spvSource.c_str(), spvSource.size(), s_spirvOpcodeTable, s_spirvOperandTable, s_spirvExtInstTable, &binary, &diagnostic);

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
	return;
}

#else // defined(DEQP_HAVE_SPIRV_TOOLS)

void assembleSpirV (const SpirVAsmSource*, std::vector<deUint8>*, SpirVProgramInfo*)
{
	TCU_THROW(NotSupportedError, "SPIR-V assembly not supported (DEQP_HAVE_SPIRV_TOOLS not defined)");
}

#endif

} // vk
