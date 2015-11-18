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
 * \brief GLSL to SPIR-V.
 *//*--------------------------------------------------------------------*/

#include "vkGlslToSpirV.hpp"
#include "deArrayUtil.hpp"
#include "deMemory.h"
#include "deClock.h"
#include "qpDebugOut.h"

#if defined(DEQP_HAVE_GLSLANG)
#	include "deSingleton.h"
#	include "deMutex.hpp"

#	include "SPIRV/GlslangToSpv.h"
#	include "SPIRV/disassemble.h"
#	include "SPIRV/doc.h"
#	include "glslang/Include/InfoSink.h"
#	include "glslang/Include/ShHandle.h"
#	include "glslang/MachineIndependent/localintermediate.h"
#	include "glslang/Public/ShaderLang.h"

#endif

namespace vk
{

using std::string;
using std::vector;

#if defined(DEQP_HAVE_GLSLANG)

namespace
{

EShLanguage getGlslangStage (glu::ShaderType type)
{
	static const EShLanguage stageMap[] =
	{
		EShLangVertex,
		EShLangFragment,
		EShLangGeometry,
		EShLangTessControl,
		EShLangTessEvaluation,
		EShLangCompute,
	};
	return de::getSizedArrayElement<glu::SHADERTYPE_LAST>(stageMap, type);
}

static volatile deSingletonState	s_glslangInitState	= DE_SINGLETON_STATE_NOT_INITIALIZED;
static de::Mutex					s_glslangLock;

void initGlslang (void*)
{
	// Main compiler
	glslang::InitializeProcess();

	// SPIR-V disassembly
	spv::Parameterize();
}

void prepareGlslang (void)
{
	deInitSingleton(&s_glslangInitState, initGlslang, DE_NULL);
}

// \todo [2015-06-19 pyry] Specialize these per GLSL version

// Fail compilation if more members are added to TLimits or TBuiltInResource
struct LimitsSizeHelper_s			{ bool m0, m1, m2, m3, m4, m5, m6, m7, m8; };
struct BuiltInResourceSizeHelper_s	{ int m[83]; LimitsSizeHelper_s l; };

DE_STATIC_ASSERT(sizeof(TLimits)			== sizeof(LimitsSizeHelper_s));
DE_STATIC_ASSERT(sizeof(TBuiltInResource)	== sizeof(BuiltInResourceSizeHelper_s));

void getDefaultLimits (TLimits* limits)
{
	limits->nonInductiveForLoops					= true;
	limits->whileLoops								= true;
	limits->doWhileLoops							= true;
	limits->generalUniformIndexing					= true;
	limits->generalAttributeMatrixVectorIndexing	= true;
	limits->generalVaryingIndexing					= true;
	limits->generalSamplerIndexing					= true;
	limits->generalVariableIndexing					= true;
	limits->generalConstantMatrixVectorIndexing		= true;
}

void getDefaultBuiltInResources (TBuiltInResource* builtin)
{
	getDefaultLimits(&builtin->limits);

	builtin->maxLights									= 32;
	builtin->maxClipPlanes								= 6;
	builtin->maxTextureUnits							= 32;
	builtin->maxTextureCoords							= 32;
	builtin->maxVertexAttribs							= 64;
	builtin->maxVertexUniformComponents					= 4096;
	builtin->maxVaryingFloats							= 64;
	builtin->maxVertexTextureImageUnits					= 32;
	builtin->maxCombinedTextureImageUnits				= 80;
	builtin->maxTextureImageUnits						= 32;
	builtin->maxFragmentUniformComponents				= 4096;
	builtin->maxDrawBuffers								= 32;
	builtin->maxVertexUniformVectors					= 128;
	builtin->maxVaryingVectors							= 8;
	builtin->maxFragmentUniformVectors					= 16;
	builtin->maxVertexOutputVectors						= 16;
	builtin->maxFragmentInputVectors					= 15;
	builtin->minProgramTexelOffset						= -8;
	builtin->maxProgramTexelOffset						= 7;
	builtin->maxClipDistances							= 8;
	builtin->maxComputeWorkGroupCountX					= 65535;
	builtin->maxComputeWorkGroupCountY					= 65535;
	builtin->maxComputeWorkGroupCountZ					= 65535;
	builtin->maxComputeWorkGroupSizeX					= 1024;
	builtin->maxComputeWorkGroupSizeY					= 1024;
	builtin->maxComputeWorkGroupSizeZ					= 64;
	builtin->maxComputeUniformComponents				= 1024;
	builtin->maxComputeTextureImageUnits				= 16;
	builtin->maxComputeImageUniforms					= 8;
	builtin->maxComputeAtomicCounters					= 8;
	builtin->maxComputeAtomicCounterBuffers				= 1;
	builtin->maxVaryingComponents						= 60;
	builtin->maxVertexOutputComponents					= 64;
	builtin->maxGeometryInputComponents					= 64;
	builtin->maxGeometryOutputComponents				= 128;
	builtin->maxFragmentInputComponents					= 128;
	builtin->maxImageUnits								= 8;
	builtin->maxCombinedImageUnitsAndFragmentOutputs	= 8;
	builtin->maxCombinedShaderOutputResources			= 8;
	builtin->maxImageSamples							= 0;
	builtin->maxVertexImageUniforms						= 0;
	builtin->maxTessControlImageUniforms				= 0;
	builtin->maxTessEvaluationImageUniforms				= 0;
	builtin->maxGeometryImageUniforms					= 0;
	builtin->maxFragmentImageUniforms					= 8;
	builtin->maxCombinedImageUniforms					= 8;
	builtin->maxGeometryTextureImageUnits				= 16;
	builtin->maxGeometryOutputVertices					= 256;
	builtin->maxGeometryTotalOutputComponents			= 1024;
	builtin->maxGeometryUniformComponents				= 1024;
	builtin->maxGeometryVaryingComponents				= 64;
	builtin->maxTessControlInputComponents				= 128;
	builtin->maxTessControlOutputComponents				= 128;
	builtin->maxTessControlTextureImageUnits			= 16;
	builtin->maxTessControlUniformComponents			= 1024;
	builtin->maxTessControlTotalOutputComponents		= 4096;
	builtin->maxTessEvaluationInputComponents			= 128;
	builtin->maxTessEvaluationOutputComponents			= 128;
	builtin->maxTessEvaluationTextureImageUnits			= 16;
	builtin->maxTessEvaluationUniformComponents			= 1024;
	builtin->maxTessPatchComponents						= 120;
	builtin->maxPatchVertices							= 32;
	builtin->maxTessGenLevel							= 64;
	builtin->maxViewports								= 16;
	builtin->maxVertexAtomicCounters					= 0;
	builtin->maxTessControlAtomicCounters				= 0;
	builtin->maxTessEvaluationAtomicCounters			= 0;
	builtin->maxGeometryAtomicCounters					= 0;
	builtin->maxFragmentAtomicCounters					= 8;
	builtin->maxCombinedAtomicCounters					= 8;
	builtin->maxAtomicCounterBindings					= 1;
	builtin->maxVertexAtomicCounterBuffers				= 0;
	builtin->maxTessControlAtomicCounterBuffers			= 0;
	builtin->maxTessEvaluationAtomicCounterBuffers		= 0;
	builtin->maxGeometryAtomicCounterBuffers			= 0;
	builtin->maxFragmentAtomicCounterBuffers			= 1;
	builtin->maxCombinedAtomicCounterBuffers			= 1;
	builtin->maxAtomicCounterBufferSize					= 16384;
	builtin->maxTransformFeedbackBuffers				= 4;
	builtin->maxTransformFeedbackInterleavedComponents	= 64;
	builtin->maxCullDistances							= 8;
	builtin->maxCombinedClipAndCullDistances			= 8;
	builtin->maxSamples									= 4;
};

} // anonymous

void glslToSpirV (const glu::ProgramSources& program, std::vector<deUint8>* dst, glu::ShaderProgramInfo* buildInfo)
{
	TBuiltInResource	builtinRes;

	prepareGlslang();
	getDefaultBuiltInResources(&builtinRes);

	// \note Compiles only first found shader
	for (int shaderType = 0; shaderType < glu::SHADERTYPE_LAST; shaderType++)
	{
		if (!program.sources[shaderType].empty())
		{
			const de::ScopedLock	compileLock			(s_glslangLock);
			const std::string&		srcText				= program.sources[shaderType][0];
			const char*				srcPtrs[]			= { srcText.c_str() };
			const int				srcLengths[]		= { (int)srcText.size() };
			vector<deUint32>		spvBlob;
			const EShLanguage		shaderStage			= getGlslangStage(glu::ShaderType(shaderType));
			glslang::TShader		shader				(shaderStage);
			glslang::TProgram		program;

			shader.setStrings(srcPtrs, DE_LENGTH_OF_ARRAY(srcPtrs));
			program.addShader(&shader);

			{
				const deUint64	compileStartTime	= deGetMicroseconds();
				const int		compileRes			= shader.parse(&builtinRes, 110, false, EShMsgSpvRules);
				glu::ShaderInfo	shaderBuildInfo;

				shaderBuildInfo.type			= (glu::ShaderType)shaderType;
				shaderBuildInfo.source			= srcText;
				shaderBuildInfo.infoLog			= shader.getInfoLog(); // \todo [2015-07-13 pyry] Include debug log?
				shaderBuildInfo.compileTimeUs	= deGetMicroseconds()-compileStartTime;
				shaderBuildInfo.compileOk		= (compileRes != 0);

				buildInfo->shaders.push_back(shaderBuildInfo);

				if (compileRes == 0)
					TCU_FAIL("Failed to compile shader");
			}

			{
				const deUint64	linkStartTime	= deGetMicroseconds();
				const int		linkRes			= program.link(EShMsgDefault);

				buildInfo->program.infoLog		= program.getInfoLog(); // \todo [2015-11-05 scygan] Include debug log?
				buildInfo->program.linkOk		= (linkRes != 0);
				buildInfo->program.linkTimeUs	= deGetMicroseconds()-linkStartTime;

				if (linkRes == 0)
					TCU_FAIL("Failed to link shader");
			}

			{
				const glslang::TIntermediate* const	intermediate	= program.getIntermediate(shaderStage);
				glslang::GlslangToSpv(*intermediate, spvBlob);
			}

			dst->resize(spvBlob.size() * sizeof(deUint32));
#if (DE_ENDIANNESS == DE_LITTLE_ENDIAN)
			deMemcpy(&(*dst)[0], &spvBlob[0], dst->size());
#else
#	error "Big-endian not supported"
#endif

			return;
		}
	}

	TCU_THROW(InternalError, "Can't compile empty program");
}

void disassembleSpirV (size_t binarySize, const deUint8* binary, std::ostream* dst)
{
	std::vector<deUint32>	binForDisasm	(binarySize/4);

	DE_ASSERT(binarySize%4 == 0);

#if (DE_ENDIANNESS == DE_LITTLE_ENDIAN)
	deMemcpy(&binForDisasm[0], binary, binarySize);
#else
#	error "Big-endian not supported"
#endif

	spv::Disassemble(*dst, binForDisasm);
}

#else // defined(DEQP_HAVE_GLSLANG)

void glslToSpirV (const glu::ProgramSources&, std::vector<deUint8>*, glu::ShaderProgramInfo*)
{
	TCU_THROW(NotSupportedError, "GLSL to SPIR-V compilation not supported (DEQP_HAVE_GLSLANG not defined)");
}

void disassembleSpirV (size_t, const deUint8*, std::ostream*)
{
	TCU_THROW(NotSupportedError, "SPIR-V disassembling not supported (DEQP_HAVE_GLSLANG not defined)");
}

#endif

} // vk
