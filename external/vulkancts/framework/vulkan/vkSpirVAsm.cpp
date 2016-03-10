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
 * \brief SPIR-V assembly to binary.
 *//*--------------------------------------------------------------------*/

#include "vkSpirVAsm.hpp"
#include "vkSpirVProgram.hpp"
#include "deClock.h"

#include <algorithm>

#if defined(DEQP_HAVE_SPIRV_TOOLS)
#	include "spirv-tools/libspirv.h"
#endif

namespace vk
{

using std::string;
using std::vector;

#if defined(DEQP_HAVE_SPIRV_TOOLS)

bool assembleSpirV (const SpirVAsmSource* program, std::vector<deUint32>* dst, SpirVProgramInfo* buildInfo)
{
	const spv_context	context		= spvContextCreate();
	spv_binary			binary		= DE_NULL;
	spv_diagnostic		diagnostic	= DE_NULL;

	if (!context)
		throw std::bad_alloc();

	try
	{
		const std::string&	spvSource			= program->source;
		const deUint64		compileStartTime	= deGetMicroseconds();
		const spv_result_t	compileOk			= spvTextToBinary(context, spvSource.c_str(), spvSource.size(), &binary, &diagnostic);

		buildInfo->source			= spvSource;
		buildInfo->infoLog			= diagnostic? diagnostic->error : ""; // \todo [2015-07-13 pyry] Include debug log?
		buildInfo->compileTimeUs	= deGetMicroseconds() - compileStartTime;
		buildInfo->compileOk		= (compileOk == SPV_SUCCESS);

		dst->resize(binary->wordCount);
		std::copy(&binary->code[0], &binary->code[0] + binary->wordCount, dst->begin());

		spvBinaryDestroy(binary);
		spvDiagnosticDestroy(diagnostic);
		spvContextDestroy(context);

		return compileOk == SPV_SUCCESS;
	}
	catch (...)
	{
		spvBinaryDestroy(binary);
		spvDiagnosticDestroy(diagnostic);
		spvContextDestroy(context);

		throw;
	}
}

void disassembleSpirV (size_t binarySizeInWords, const deUint32* binary, std::ostream* dst)
{
	const spv_context	context		= spvContextCreate();
	spv_text			text		= DE_NULL;
	spv_diagnostic		diagnostic	= DE_NULL;

	if (!context)
		throw std::bad_alloc();

	try
	{
		const spv_result_t	result	= spvBinaryToText(context, binary, binarySizeInWords, 0, &text, &diagnostic);

		if (result != SPV_SUCCESS)
			TCU_THROW(InternalError, "Disassembling SPIR-V failed");

		*dst << text->str;

		spvTextDestroy(text);
		spvDiagnosticDestroy(diagnostic);
		spvContextDestroy(context);
	}
	catch (...)
	{
		spvTextDestroy(text);
		spvDiagnosticDestroy(diagnostic);
		spvContextDestroy(context);

		throw;
	}
}

bool validateSpirV (size_t binarySizeInWords, const deUint32* binary, std::ostream* infoLog)
{
	const spv_context	context		= spvContextCreate();
	spv_diagnostic		diagnostic	= DE_NULL;

	try
	{
		spv_const_binary_t	cbinary		= { binary, binarySizeInWords };
		const spv_result_t	valid		= spvValidate(context, &cbinary, &diagnostic);

		if (diagnostic)
			*infoLog << diagnostic->error;

		spvDiagnosticDestroy(diagnostic);
		spvContextDestroy(context);

		return valid == SPV_SUCCESS;
	}
	catch (...)
	{
		spvDiagnosticDestroy(diagnostic);
		spvContextDestroy(context);

		throw;
	}
}

#else // defined(DEQP_HAVE_SPIRV_TOOLS)

bool assembleSpirV (const SpirVAsmSource*, std::vector<deUint32>*, SpirVProgramInfo*)
{
	TCU_THROW(NotSupportedError, "SPIR-V assembly not supported (DEQP_HAVE_SPIRV_TOOLS not defined)");
}

void disassembleSpirV (size_t, const deUint32*, std::ostream*)
{
	TCU_THROW(NotSupportedError, "SPIR-V disassembling not supported (DEQP_HAVE_SPIRV_TOOLS not defined)");
}

bool validateSpirV (size_t, const deUint32*, std::ostream*)
{
	TCU_THROW(NotSupportedError, "SPIR-V validation not supported (DEQP_HAVE_SPIRV_TOOLS not defined)");
}

#endif

} // vk
