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
 * \brief Program utilities.
 *//*--------------------------------------------------------------------*/

#include "vkPrograms.hpp"
#include "vkGlslToSpirV.hpp"
#include "vkSpirVAsm.hpp"
#include "vkRefUtil.hpp"

#include "tcuTestLog.hpp"

#include "deArrayUtil.hpp"
#include "deMemory.h"
#include "deInt32.h"

namespace vk
{

using std::string;
using std::vector;
using tcu::TestLog;

#if defined(DE_DEBUG) && defined(DEQP_HAVE_SPIRV_TOOLS)
#	define VALIDATE_BINARIES	true
#else
#	define VALIDATE_BINARIES	false
#endif

#define SPIRV_BINARY_ENDIANNESS DE_LITTLE_ENDIAN

// ProgramBinary

ProgramBinary::ProgramBinary (ProgramFormat format, size_t binarySize, const deUint8* binary)
	: m_format	(format)
	, m_binary	(binary, binary+binarySize)
{
}

// Utils

namespace
{

bool isNativeSpirVBinaryEndianness (void)
{
#if (DE_ENDIANNESS == SPIRV_BINARY_ENDIANNESS)
	return true;
#else
	return false;
#endif
}

bool isSaneSpirVBinary (const ProgramBinary& binary)
{
	const deUint32	spirvMagicWord	= 0x07230203;
	const deUint32	spirvMagicBytes	= isNativeSpirVBinaryEndianness()
									? spirvMagicWord
									: deReverseBytes32(spirvMagicWord);

	DE_ASSERT(binary.getFormat() == PROGRAM_FORMAT_SPIRV);

	if (binary.getSize() % sizeof(deUint32) != 0)
		return false;

	if (binary.getSize() < sizeof(deUint32))
		return false;

	if (*(const deUint32*)binary.getBinary() != spirvMagicBytes)
		return false;

	return true;
}

ProgramBinary* createProgramBinaryFromSpirV (const vector<deUint32>& binary)
{
	DE_ASSERT(!binary.empty());

	if (isNativeSpirVBinaryEndianness())
		return new ProgramBinary(PROGRAM_FORMAT_SPIRV, binary.size()*sizeof(deUint32), (const deUint8*)&binary[0]);
	else
		TCU_THROW(InternalError, "SPIR-V endianness translation not supported");
}

} // anonymous

ProgramBinary* buildProgram (const glu::ProgramSources& program, ProgramFormat binaryFormat, glu::ShaderProgramInfo* buildInfo)
{
	const bool	validateBinary	= VALIDATE_BINARIES;

	if (binaryFormat == PROGRAM_FORMAT_SPIRV)
	{
		vector<deUint32> binary;

		if (!compileGlslToSpirV(program, &binary, buildInfo))
			TCU_THROW(InternalError, "Compiling GLSL to SPIR-V failed");

		if (validateBinary)
		{
			std::ostringstream validationLog;

			if (!validateSpirV(binary.size(), &binary[0], &validationLog))
			{
				buildInfo->program.linkOk 	 = false;
				buildInfo->program.infoLog	+= "\n" + validationLog.str();

				TCU_THROW(InternalError, "Validation failed for compiled SPIR-V binary");
			}
		}

		return createProgramBinaryFromSpirV(binary);
	}
	else
		TCU_THROW(NotSupportedError, "Unsupported program format");
}

ProgramBinary* assembleProgram (const SpirVAsmSource& program, SpirVProgramInfo* buildInfo)
{
	const bool			validateBinary		= VALIDATE_BINARIES;
	vector<deUint32>	binary;

	if (!assembleSpirV(&program, &binary, buildInfo))
		TCU_THROW(InternalError, "Failed to assemble SPIR-V");

	if (validateBinary)
	{
		std::ostringstream	validationLog;

		if (!validateSpirV(binary.size(), &binary[0], &validationLog))
		{
			buildInfo->compileOk	 = false;
			buildInfo->infoLog		+= "\n" + validationLog.str();

			TCU_THROW(InternalError, "Validation failed for assembled SPIR-V binary");
		}
	}

	return createProgramBinaryFromSpirV(binary);
}

void disassembleProgram (const ProgramBinary& program, std::ostream* dst)
{
	if (program.getFormat() == PROGRAM_FORMAT_SPIRV)
	{
		TCU_CHECK_INTERNAL(isSaneSpirVBinary(program));

		if (isNativeSpirVBinaryEndianness())
			disassembleSpirV(program.getSize()/sizeof(deUint32), (const deUint32*)program.getBinary(), dst);
		else
			TCU_THROW(InternalError, "SPIR-V endianness translation not supported");
	}
	else
		TCU_THROW(NotSupportedError, "Unsupported program format");
}

bool validateProgram (const ProgramBinary& program, std::ostream* dst)
{
	if (program.getFormat() == PROGRAM_FORMAT_SPIRV)
	{
		if (!isSaneSpirVBinary(program))
		{
			*dst << "Binary doesn't look like SPIR-V at all";
			return false;
		}

		if (isNativeSpirVBinaryEndianness())
			return validateSpirV(program.getSize()/sizeof(deUint32), (const deUint32*)program.getBinary(), dst);
		else
			TCU_THROW(InternalError, "SPIR-V endianness translation not supported");
	}
	else
		TCU_THROW(NotSupportedError, "Unsupported program format");
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
