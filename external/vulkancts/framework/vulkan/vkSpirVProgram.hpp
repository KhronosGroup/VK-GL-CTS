#ifndef _VKSPIRVPROGRAM_HPP
#define _VKSPIRVPROGRAM_HPP
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
 * \brief SPIR-V program and binary info.
 *//*--------------------------------------------------------------------*/

#include "vkDefs.hpp"
#include "tcuTestLog.hpp"

#include <string>
#include <sstream>

namespace vk
{

struct SpirVAsmSource
{
	template<typename T>
	SpirVAsmSource& operator<<(const T& val)
	{
		program << val;
		return *this;
	}
	std::ostringstream program;
};

struct SpirVProgramInfo
{
	SpirVProgramInfo()
		: source		(DE_NULL)
		, compileTimeUs	(0)
		, compileOk		(false)
	{
	}

	const SpirVAsmSource*	source;
	std::string				infoLog;
	deUint64				compileTimeUs;
	bool					compileOk;
};

tcu::TestLog&	operator<<			(tcu::TestLog& log, const SpirVProgramInfo& shaderInfo);
tcu::TestLog&	operator<<			(tcu::TestLog& log, const SpirVAsmSource& program);

}

#endif // _VKSPIRVPROGRAM_HPP
