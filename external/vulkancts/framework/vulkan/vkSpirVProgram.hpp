#ifndef _VKSPIRVPROGRAM_HPP
#define _VKSPIRVPROGRAM_HPP
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
 * \brief SPIR-V program and binary info.
 *//*--------------------------------------------------------------------*/

#include "vkDefs.hpp"
#include "tcuTestLog.hpp"
#include "deStringUtil.hpp"

#include <string>

namespace vk
{

struct SpirVAsmSource
{
	SpirVAsmSource (void)
	{
	}

	SpirVAsmSource (const std::string& source_)
		: source(source_)
	{
	}

	std::string		source;
};

struct SpirVProgramInfo
{
	SpirVProgramInfo (void)
		: compileTimeUs	(0)
		, compileOk		(false)
	{
	}

	std::string		source;
	std::string		infoLog;
	deUint64		compileTimeUs;
	bool			compileOk;
};

tcu::TestLog&	operator<<		(tcu::TestLog& log, const SpirVProgramInfo& shaderInfo);
tcu::TestLog&	operator<<		(tcu::TestLog& log, const SpirVAsmSource& program);

// Helper for constructing SpirVAsmSource
template<typename T>
SpirVAsmSource& operator<< (SpirVAsmSource& src, const T& val)
{
	src.source += de::toString(val);
	return src;
}

}

#endif // _VKSPIRVPROGRAM_HPP
