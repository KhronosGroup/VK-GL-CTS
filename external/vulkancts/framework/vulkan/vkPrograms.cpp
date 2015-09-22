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
		return new ProgramBinary(binaryFormat, binary.size(), &binary[0]);
	}
	else
		TCU_THROW(NotSupportedError, "Unsupported program format");
}

ProgramBinary* assembleProgram (const SpirVAsmSource& program, SpirVProgramInfo* buildInfo)
{
	vector<deUint8> binary;
	assembleSpirV(&program, &binary, buildInfo);
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
			(deUintptr)binary.getSize(),
			binary.getBinary(),
			flags,
		};

		return createShaderModule(deviceInterface, device, &shaderModuleInfo);
	}
	else
		TCU_THROW(NotSupportedError, "Unsupported program format");
}

glu::ShaderType getGluShaderType (VkShaderStage shaderStage)
{
	static const glu::ShaderType s_shaderTypes[] =
	{
		glu::SHADERTYPE_VERTEX,
		glu::SHADERTYPE_TESSELLATION_CONTROL,
		glu::SHADERTYPE_TESSELLATION_EVALUATION,
		glu::SHADERTYPE_GEOMETRY,
		glu::SHADERTYPE_FRAGMENT,
		glu::SHADERTYPE_COMPUTE
	};

	return de::getSizedArrayElement<VK_SHADER_STAGE_LAST>(s_shaderTypes, shaderStage);
}

VkShaderStage getVkShaderStage (glu::ShaderType shaderType)
{
	static const VkShaderStage s_shaderStages[] =
	{
		VK_SHADER_STAGE_VERTEX,
		VK_SHADER_STAGE_FRAGMENT,
		VK_SHADER_STAGE_GEOMETRY,
		VK_SHADER_STAGE_TESS_CONTROL,
		VK_SHADER_STAGE_TESS_EVALUATION,
		VK_SHADER_STAGE_COMPUTE
	};

	return de::getSizedArrayElement<glu::SHADERTYPE_LAST>(s_shaderStages, shaderType);
}

} // vk
