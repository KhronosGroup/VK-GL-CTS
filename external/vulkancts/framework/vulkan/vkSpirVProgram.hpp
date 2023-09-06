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
#include "deStringUtil.hpp"
#include "vkValidatorOptions.hpp"

#include <string>

namespace tcu
{
class TestLog;
} // tcu

namespace vk
{

struct SpirVAsmBuildOptions
{
	deUint32		vulkanVersion;
	SpirvVersion	targetVersion;
	bool			supports_VK_KHR_spirv_1_4;
	bool			supports_VK_KHR_maintenance4;

	SpirVAsmBuildOptions (deUint32 vulkanVersion_, SpirvVersion targetVersion_, bool allowSpirv14 = false, bool allowMaintenance4 = false)
		: vulkanVersion					(vulkanVersion_)
		, targetVersion					(targetVersion_)
		, supports_VK_KHR_spirv_1_4		(allowSpirv14)
		, supports_VK_KHR_maintenance4	(allowMaintenance4)
	{}

	SpirVAsmBuildOptions (void)
		: vulkanVersion					(VK_MAKE_API_VERSION(0, 1, 0, 0))
		, targetVersion					(SPIRV_VERSION_1_0)
		, supports_VK_KHR_spirv_1_4		(false)
		, supports_VK_KHR_maintenance4	(false)
	{}

	SpirvValidatorOptions getSpirvValidatorOptions() const
	{
		SpirvValidatorOptions result(vulkanVersion);
		result.supports_VK_KHR_spirv_1_4 = supports_VK_KHR_spirv_1_4;
		if (supports_VK_KHR_maintenance4)
			result.flags = result.flags | SpirvValidatorOptions::FLAG_SPIRV_VALIDATOR_ALLOW_LOCALSIZEID;
		return result;
	}
};

struct SpirVAsmSource
{
	SpirVAsmSource (void)
	{
	}

	SpirVAsmSource (const std::string& source_)
		: source(source_)
	{
	}

	SpirVAsmSource& operator<< (const SpirVAsmBuildOptions& buildOptions_)
	{
		buildOptions = buildOptions_;
		return *this;
	}

	SpirVAsmBuildOptions	buildOptions;
	std::string				source;
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
