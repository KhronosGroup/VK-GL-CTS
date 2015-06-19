/*-------------------------------------------------------------------------
 * drawElements Quality Program Vulkan Utilities
 * -----------------------------------------------
 *
 * Copyright 2015 The Android Open Source Project
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
 * \brief GLSL to SPIR-V.
 *//*--------------------------------------------------------------------*/

#include "vkGlslToSpirV.hpp"
#include "deArrayUtil.hpp"
#include "deMemory.h"
#include "qpDebugOut.h"

#if defined(DEQP_HAVE_GLSLANG)
#	include "deSingleton.h"
#	include "deMutex.hpp"

#	include "SPIRV/GlslangToSpv.h"
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
	ShInitialize();
}

void prepareGlslang (void)
{
	deInitSingleton(&s_glslangInitState, initGlslang, DE_NULL);
}

class SpvGenerator : public TCompiler
{
public:
	SpvGenerator (EShLanguage language, std::vector<deUint32>& dst, TInfoSink& infoSink)
		: TCompiler	(language, infoSink)
		, m_dst		(dst)
	{
	}

	bool compile (TIntermNode* root, int version = 0, EProfile profile = ENoProfile)
	{
		glslang::TIntermediate intermediate(getLanguage(), version, profile);
		intermediate.setTreeRoot(root);
		glslang::GlslangToSpv(intermediate, m_dst);
		return true;
	}

private:
	std::vector<deUint32>&	m_dst;
};

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
	builtin->maxComputeWorkGroupSizeX					= 1024;
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

void glslToSpirV (const glu::ProgramSources& program, std::vector<deUint8>& dst)
{
	TBuiltInResource	builtinRes;

	prepareGlslang();
	getDefaultBuiltInResources(&builtinRes);

	// \note Compiles only first found shader
	for (int shaderType = 0; shaderType < glu::SHADERTYPE_LAST; shaderType++)
	{
		if (!program.sources[shaderType].empty())
		{
			de::ScopedLock		compileLock	(s_glslangLock);
			const char* const	srcText		= program.sources[shaderType][0].c_str();
			const int			srcLen		= (int)program.sources[shaderType][0].size();
			vector<deUint32>	spvBlob;
			TInfoSink			infoSink;
			SpvGenerator		compiler	(getGlslangStage(glu::ShaderType(shaderType)), spvBlob, infoSink);
			const int			compileOk	= ShCompile(static_cast<ShHandle>(&compiler), &srcText, 1, &srcLen, EShOptNone, &builtinRes, 0);

			if (compileOk == 0)
			{
				// \todo [2015-06-19 pyry] Create special CompileException and pass error messages through that
				qpPrint(infoSink.info.c_str());
				TCU_FAIL("Failed to compile shader");
			}

			dst.resize(spvBlob.size() * sizeof(deUint32));
#if (DE_ENDIANNESS == DE_LITTLE_ENDIAN)
			deMemcpy(&dst[0], &spvBlob[0], dst.size());
#else
#	error "Big-endian not supported"
#endif
			return;
		}
	}

	TCU_THROW(InternalError, "Can't compile empty program");
}

#else // defined(DEQP_HAVE_GLSLANG)

void glslToSpirV (const glu::ProgramSources&, std::vector<deUint8>&)
{
	TCU_THROW(NotSupportedError, "GLSL to SPIR-V compilation not supported (DEQP_HAVE_GLSLANG not defined)");
}

#endif

} // vk
