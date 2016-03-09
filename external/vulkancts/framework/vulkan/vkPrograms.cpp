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
 * \brief Program utilities.
 *//*--------------------------------------------------------------------*/

#include "vkPrograms.hpp"
#include "vkGlslToSpirV.hpp"
#include "vkSpirVAsm.hpp"
#include "vkRefUtil.hpp"

#include "tcuTestLog.hpp"

#include "deArrayUtil.hpp"
#include "deMemory.h"

namespace vk
{

using std::string;
using std::vector;
using tcu::TestLog;

// ProgramBinary

ProgramBinary::ProgramBinary (ProgramFormat format, size_t binarySize, const deUint8* binary)
	: m_format	(format)
	, m_binary	(binary, binary+binarySize)
{
}

// Utils

ProgramBinary* buildProgram (const glu::ProgramSources& program, ProgramFormat binaryFormat, glu::ShaderProgramInfo* buildInfo)
{
	if (binaryFormat == PROGRAM_FORMAT_SPIRV)
	{
		vector<deUint8> binary;
		glslToSpirV(program, &binary, buildInfo);
		// \todo[2016-02-10 dekimir]: Enable this when all glsl tests can pass it.
		//validateSpirV(binary, &buildInfo->program.infoLog);
		return new ProgramBinary(binaryFormat, binary.size(), &binary[0]);
	}
	else
		TCU_THROW(NotSupportedError, "Unsupported program format");
}

ProgramBinary* assembleProgram (const SpirVAsmSource& program, SpirVProgramInfo* buildInfo)
{
	vector<deUint8> binary;
	assembleSpirV(&program, &binary, buildInfo);
#if defined(DE_DEBUG)
	buildInfo->compileOk &= validateSpirV(binary, &buildInfo->infoLog);
	if (!buildInfo->compileOk)
		TCU_FAIL("Failed to validate shader");
#endif
	return new ProgramBinary(PROGRAM_FORMAT_SPIRV, binary.size(), &binary[0]);
}

Move<VkShaderModule> createShaderModule (const DeviceInterface& deviceInterface, VkDevice device, const ProgramBinary& binary, VkShaderModuleCreateFlags flags)
{
	if (binary.getFormat() == PROGRAM_FORMAT_SPIRV)
	{
		const struct VkShaderModuleCreateInfo		shaderModuleInfo	=
		{
			VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
			DE_NULL,
			flags,
			(deUintptr)binary.getSize(),
			(const deUint32*)binary.getBinary(),
		};

		return createShaderModule(deviceInterface, device, &shaderModuleInfo);
	}
	else
		TCU_THROW(NotSupportedError, "Unsupported program format");
}

glu::ShaderType getGluShaderType (VkShaderStageFlagBits shaderStage)
{
	switch (shaderStage)
	{
		case VK_SHADER_STAGE_VERTEX_BIT:					return glu::SHADERTYPE_VERTEX;
		case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:		return glu::SHADERTYPE_TESSELLATION_CONTROL;
		case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT:	return glu::SHADERTYPE_TESSELLATION_EVALUATION;
		case VK_SHADER_STAGE_GEOMETRY_BIT:					return glu::SHADERTYPE_GEOMETRY;
		case VK_SHADER_STAGE_FRAGMENT_BIT:					return glu::SHADERTYPE_FRAGMENT;
		case VK_SHADER_STAGE_COMPUTE_BIT:					return glu::SHADERTYPE_COMPUTE;
		default:
			DE_FATAL("Unknown shader stage");
			return glu::SHADERTYPE_LAST;
	}
}

VkShaderStageFlagBits getVkShaderStage (glu::ShaderType shaderType)
{
	static const VkShaderStageFlagBits s_shaderStages[] =
	{
		VK_SHADER_STAGE_VERTEX_BIT,
		VK_SHADER_STAGE_FRAGMENT_BIT,
		VK_SHADER_STAGE_GEOMETRY_BIT,
		VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,
		VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,
		VK_SHADER_STAGE_COMPUTE_BIT
	};

	return de::getSizedArrayElement<glu::SHADERTYPE_LAST>(s_shaderStages, shaderType);
}

} // vk
