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
#include "deArrayUtil.hpp"
#include "deMemory.h"

namespace vk
{

using std::string;
using std::vector;

// ProgramBinary

ProgramBinary::ProgramBinary (ProgramFormat format, size_t binarySize, const deUint8* binary)
	: m_format	(format)
	, m_binary	(binary, binary+binarySize)
{
}

// Utils

namespace
{

// \todo [2015-05-12 pyry] All of this is just placeholder until we have proper SPIR-V binary support

enum { GLSL_BINARY_MAGIC = 0x610d510a };

struct BinaryHeader
{
	deUint32	magic;
	deUint32	sourceLength[glu::SHADERTYPE_LAST]; // Length per stage, 0 means that not supplied
};

DE_STATIC_ASSERT(sizeof(BinaryHeader) == sizeof(deUint32)*7);

size_t computeSrcArrayTotalLength (const vector<string>& sources)
{
	size_t total = 0;
	for (vector<string>::const_iterator i = sources.begin(); i != sources.end(); ++i)
		total += i->length();
	return total;
}

size_t computeAggregatedSrcLength (const glu::ProgramSources& sources)
{
	size_t total = 0;
	for (int type = 0; type < glu::SHADERTYPE_LAST; type++)
		total += computeSrcArrayTotalLength(sources.sources[type]);
	return total;
}

void encodeGLSLBinary (const glu::ProgramSources& sources, vector<deUint8>& dst)
{
	const size_t	binarySize		= sizeof(BinaryHeader) + computeAggregatedSrcLength(sources);

	dst.resize(binarySize);

	{
		BinaryHeader* const	hdr	= (BinaryHeader*)&dst[0];
		hdr->magic = GLSL_BINARY_MAGIC;

		for (int type = 0; type < glu::SHADERTYPE_LAST; type++)
			hdr->sourceLength[type] = (deUint32)computeSrcArrayTotalLength(sources.sources[type]);
	}

	{
		size_t curOffset = sizeof(BinaryHeader);
		for (int type = 0; type < glu::SHADERTYPE_LAST; type++)
		{
			for (vector<string>::const_iterator srcIter = sources.sources[type].begin();
				 srcIter != sources.sources[type].end();
				 ++srcIter)
			{
				if (!srcIter->empty())
				{
					deMemcpy(&dst[curOffset], srcIter->c_str(), (int)srcIter->length());
					curOffset += srcIter->length();
				}
			}
		}
	}
}

void decodeGLSLBinary (size_t binarySize, const deUint8* binary, glu::ProgramSources& dst)
{
	const BinaryHeader*	hdr	= (const BinaryHeader*)binary;

	if (binarySize < sizeof(BinaryHeader) || hdr->magic != GLSL_BINARY_MAGIC)
		TCU_THROW(Exception, "Invalid GLSL program binary");

	{
		size_t curOffset = sizeof(BinaryHeader);

		for (int type = 0; type < glu::SHADERTYPE_LAST; type++)
		{
			if (hdr->sourceLength[type] > 0)
			{
				if (curOffset+hdr->sourceLength[type] > binarySize)
					TCU_THROW(Exception, "Incomplete GLSL program binary");

				dst.sources[type].resize(1);
				dst.sources[type][0] = std::string((const char*)&binary[curOffset], hdr->sourceLength[type]);

				curOffset += hdr->sourceLength[type];
			}
			else
				dst.sources[type].clear();
		}
	}
}

VkShaderStage getShaderStage (glu::ShaderType type)
{
	static const VkShaderStage stageMap[] =
	{
		VK_SHADER_STAGE_VERTEX,
		VK_SHADER_STAGE_FRAGMENT,
		VK_SHADER_STAGE_GEOMETRY,
		VK_SHADER_STAGE_TESS_CONTROL,
		VK_SHADER_STAGE_TESS_EVALUATION,
		VK_SHADER_STAGE_COMPUTE,
	};
	return de::getSizedArrayElement<glu::SHADERTYPE_LAST>(stageMap, type);
}

} // anonymous

ProgramBinary* buildProgram (const glu::ProgramSources& program, ProgramFormat binaryFormat)
{
	if (binaryFormat == PROGRAM_FORMAT_GLSL_SOURCE)
	{
		vector<deUint8> binary;
		encodeGLSLBinary(program, binary);
		return new ProgramBinary(binaryFormat, binary.size(), &binary[0]);
	}
	else if (binaryFormat == PROGRAM_FORMAT_SPIRV)
	{
		vector<deUint8> binary;
		glslToSpirV(program, binary);
		return new ProgramBinary(binaryFormat, binary.size(), &binary[0]);
	}
	else
		TCU_THROW(NotSupportedError, "Unsupported program format");
}

Move<VkShaderT> createShader (const DeviceInterface& deviceInterface, VkDevice device, const ProgramBinary& binary, VkShaderCreateFlags flags)
{
	if (binary.getFormat() == PROGRAM_FORMAT_GLSL_SOURCE)
	{
		// HACK: just concatenate everything
		glu::ProgramSources	sources;
		std::string			concatenated;

		decodeGLSLBinary(binary.getSize(), binary.getBinary(), sources);

		for (int shaderType = 0; shaderType < glu::SHADERTYPE_LAST; shaderType++)
		{
			for (size_t ndx = 0; ndx < sources.sources[shaderType].size(); ++ndx)
				concatenated += sources.sources[shaderType][ndx];
		}

		{
			const struct VkShaderCreateInfo		shaderInfo	=
			{
				VK_STRUCTURE_TYPE_SHADER_CREATE_INFO,	//	VkStructureType		sType;
				DE_NULL,								//	const void*			pNext;
				(deUintptr)concatenated.size(),			//	deUintptr			codeSize;
				concatenated.c_str(),					//	const void*			pCode;
				flags,									//	VkShaderCreateFlags	flags;
			};

			return createShader(deviceInterface, device, &shaderInfo);
		}
	}
	else if (binary.getFormat() == PROGRAM_FORMAT_SPIRV)
	{
		const struct VkShaderCreateInfo		shaderInfo	=
		{
			VK_STRUCTURE_TYPE_SHADER_CREATE_INFO,	//	VkStructureType		sType;
			DE_NULL,								//	const void*			pNext;
			(deUintptr)binary.getSize(),			//	deUintptr			codeSize;
			binary.getBinary(),						//	const void*			pCode;
			flags,									//	VkShaderCreateFlags	flags;
		};

		return createShader(deviceInterface, device, &shaderInfo);
	}
	else
		TCU_THROW(NotSupportedError, "Unsupported program format");
}

} // vk
