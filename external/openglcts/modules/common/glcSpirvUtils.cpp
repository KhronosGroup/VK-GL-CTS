/*-------------------------------------------------------------------------
 * OpenGL Conformance Test Suite
 * -----------------------------
 *
 * Copyright (c) 2017-2019 The Khronos Group Inc.
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
 */ /*!
 * \file  glcSpirvUtils.cpp
 * \brief Utility functions for using Glslang and Spirv-tools to work with
 *  SPIR-V shaders.
 */ /*-------------------------------------------------------------------*/

#include "glcSpirvUtils.hpp"
#include "deArrayUtil.hpp"
#include "deSingleton.h"
#include "deStringUtil.hpp"
#include "gluContextInfo.hpp"
#include "tcuTestLog.hpp"

#include "SPIRV/GlslangToSpv.h"
#include "SPIRV/disassemble.h"
#include "SPIRV/doc.h"
#include "glslang/MachineIndependent/localintermediate.h"
#include "glslang/Public/ShaderLang.h"

#include "spirv-tools/libspirv.hpp"
#include "spirv-tools/optimizer.hpp"

using namespace glu;

namespace glc
{

namespace spirvUtils
{

void checkGlSpirvSupported(deqp::Context& m_context)
{
	bool is_at_least_gl_46 = (glu::contextSupports(m_context.getRenderContext().getType(), glu::ApiType::core(4, 6)));
	bool is_arb_gl_spirv   = m_context.getContextInfo().isExtensionSupported("GL_ARB_gl_spirv");

	if ((!is_at_least_gl_46) && (!is_arb_gl_spirv))
		TCU_THROW(NotSupportedError, "GL 4.6 or GL_ARB_gl_spirv is not supported");
}

EShLanguage getGlslangStage(glu::ShaderType type)
{
	static const EShLanguage stageMap[] = {
		EShLangVertex, EShLangFragment, EShLangGeometry, EShLangTessControl, EShLangTessEvaluation, EShLangCompute,
		EShLangRayGen, EShLangAnyHit, EShLangClosestHit, EShLangMiss, EShLangIntersect, EShLangCallable, EShLangTaskNV,
		EShLangMeshNV
	};

	return de::getSizedArrayElement<glu::SHADERTYPE_LAST>(stageMap, type);
}

static volatile deSingletonState s_glslangInitState = DE_SINGLETON_STATE_NOT_INITIALIZED;

void initGlslang(void*)
{
	// Main compiler
	glslang::InitializeProcess();

	// SPIR-V disassembly
	spv::Parameterize();
}

void prepareGlslang(void)
{
	deInitSingleton(&s_glslangInitState, initGlslang, DE_NULL);
}

void getDefaultLimits(TLimits* limits)
{
	limits->nonInductiveForLoops				 = true;
	limits->whileLoops							 = true;
	limits->doWhileLoops						 = true;
	limits->generalUniformIndexing				 = true;
	limits->generalAttributeMatrixVectorIndexing = true;
	limits->generalVaryingIndexing				 = true;
	limits->generalSamplerIndexing				 = true;
	limits->generalVariableIndexing				 = true;
	limits->generalConstantMatrixVectorIndexing  = true;
}

void getDefaultBuiltInResources(TBuiltInResource* builtin)
{
	getDefaultLimits(&builtin->limits);

	builtin->maxLights								   = 32;
	builtin->maxClipPlanes							   = 6;
	builtin->maxTextureUnits						   = 32;
	builtin->maxTextureCoords						   = 32;
	builtin->maxVertexAttribs						   = 64;
	builtin->maxVertexUniformComponents				   = 4096;
	builtin->maxVaryingFloats						   = 64;
	builtin->maxVertexTextureImageUnits				   = 32;
	builtin->maxCombinedTextureImageUnits			   = 80;
	builtin->maxTextureImageUnits					   = 32;
	builtin->maxFragmentUniformComponents			   = 4096;
	builtin->maxDrawBuffers							   = 32;
	builtin->maxVertexUniformVectors				   = 128;
	builtin->maxVaryingVectors						   = 8;
	builtin->maxFragmentUniformVectors				   = 16;
	builtin->maxVertexOutputVectors					   = 16;
	builtin->maxFragmentInputVectors				   = 15;
	builtin->minProgramTexelOffset					   = -8;
	builtin->maxProgramTexelOffset					   = 7;
	builtin->maxClipDistances						   = 8;
	builtin->maxComputeWorkGroupCountX				   = 65535;
	builtin->maxComputeWorkGroupCountY				   = 65535;
	builtin->maxComputeWorkGroupCountZ				   = 65535;
	builtin->maxComputeWorkGroupSizeX				   = 1024;
	builtin->maxComputeWorkGroupSizeY				   = 1024;
	builtin->maxComputeWorkGroupSizeZ				   = 64;
	builtin->maxComputeUniformComponents			   = 1024;
	builtin->maxComputeTextureImageUnits			   = 16;
	builtin->maxComputeImageUniforms				   = 8;
	builtin->maxComputeAtomicCounters				   = 8;
	builtin->maxComputeAtomicCounterBuffers			   = 1;
	builtin->maxVaryingComponents					   = 60;
	builtin->maxVertexOutputComponents				   = 64;
	builtin->maxGeometryInputComponents				   = 64;
	builtin->maxGeometryOutputComponents			   = 128;
	builtin->maxFragmentInputComponents				   = 128;
	builtin->maxImageUnits							   = 8;
	builtin->maxCombinedImageUnitsAndFragmentOutputs   = 8;
	builtin->maxCombinedShaderOutputResources		   = 8;
	builtin->maxImageSamples						   = 0;
	builtin->maxVertexImageUniforms					   = 0;
	builtin->maxTessControlImageUniforms			   = 0;
	builtin->maxTessEvaluationImageUniforms			   = 0;
	builtin->maxGeometryImageUniforms				   = 0;
	builtin->maxFragmentImageUniforms				   = 8;
	builtin->maxCombinedImageUniforms				   = 8;
	builtin->maxGeometryTextureImageUnits			   = 16;
	builtin->maxGeometryOutputVertices				   = 256;
	builtin->maxGeometryTotalOutputComponents		   = 1024;
	builtin->maxGeometryUniformComponents			   = 1024;
	builtin->maxGeometryVaryingComponents			   = 64;
	builtin->maxTessControlInputComponents			   = 128;
	builtin->maxTessControlOutputComponents			   = 128;
	builtin->maxTessControlTextureImageUnits		   = 16;
	builtin->maxTessControlUniformComponents		   = 1024;
	builtin->maxTessControlTotalOutputComponents	   = 4096;
	builtin->maxTessEvaluationInputComponents		   = 128;
	builtin->maxTessEvaluationOutputComponents		   = 128;
	builtin->maxTessEvaluationTextureImageUnits		   = 16;
	builtin->maxTessEvaluationUniformComponents		   = 1024;
	builtin->maxTessPatchComponents					   = 120;
	builtin->maxPatchVertices						   = 32;
	builtin->maxTessGenLevel						   = 64;
	builtin->maxViewports							   = 16;
	builtin->maxVertexAtomicCounters				   = 0;
	builtin->maxTessControlAtomicCounters			   = 0;
	builtin->maxTessEvaluationAtomicCounters		   = 0;
	builtin->maxGeometryAtomicCounters				   = 0;
	builtin->maxFragmentAtomicCounters				   = 8;
	builtin->maxCombinedAtomicCounters				   = 8;
	builtin->maxAtomicCounterBindings				   = 1;
	builtin->maxVertexAtomicCounterBuffers			   = 0;
	builtin->maxTessControlAtomicCounterBuffers		   = 0;
	builtin->maxTessEvaluationAtomicCounterBuffers	 = 0;
	builtin->maxGeometryAtomicCounterBuffers		   = 0;
	builtin->maxFragmentAtomicCounterBuffers		   = 1;
	builtin->maxCombinedAtomicCounterBuffers		   = 1;
	builtin->maxAtomicCounterBufferSize				   = 16384;
	builtin->maxTransformFeedbackBuffers			   = 4;
	builtin->maxTransformFeedbackInterleavedComponents = 64;
	builtin->maxCullDistances						   = 8;
	builtin->maxCombinedClipAndCullDistances		   = 8;
	builtin->maxSamples								   = 4;
	builtin->maxMeshOutputVerticesNV				   = 256;
	builtin->maxMeshOutputPrimitivesNV				   = 256;
	builtin->maxMeshWorkGroupSizeX_NV				   = 32;
	builtin->maxMeshWorkGroupSizeY_NV				   = 1;
	builtin->maxMeshWorkGroupSizeZ_NV				   = 1;
	builtin->maxTaskWorkGroupSizeX_NV				   = 32;
	builtin->maxTaskWorkGroupSizeY_NV				   = 1;
	builtin->maxTaskWorkGroupSizeZ_NV				   = 1;
	builtin->maxMeshViewCountNV						   = 4;
	builtin->maxDualSourceDrawBuffersEXT			   = 1;
};

glslang::EShTargetLanguageVersion getSpirvTargetVersion(SpirvVersion version)
{
    switch(version)
    {
    default:
        DE_FATAL("unhandled SPIRV target version");
        // fall-through
    case SPIRV_VERSION_1_0:
        return glslang::EShTargetSpv_1_0;
    case SPIRV_VERSION_1_1:
        return glslang::EShTargetSpv_1_1;
    case SPIRV_VERSION_1_2:
        return glslang::EShTargetSpv_1_2;
    case SPIRV_VERSION_1_3:
        return glslang::EShTargetSpv_1_3;
    }
}

bool compileGlslToSpirV(tcu::TestLog& log, std::string source, glu::ShaderType type, ShaderBinaryDataType* dst, SpirvVersion version)
{
	TBuiltInResource builtinRes;

	prepareGlslang();
	getDefaultBuiltInResources(&builtinRes);

	const EShLanguage shaderStage = getGlslangStage(type);

	glslang::TShader  shader(shaderStage);
	glslang::TProgram program;

	const char* src[] = { source.c_str() };

	shader.setStrings(src, 1);
	shader.setEnvTarget(glslang::EshTargetSpv, getSpirvTargetVersion(version));
	program.addShader(&shader);

	const int compileRes = shader.parse(&builtinRes, 100, false, EShMsgSpvRules);
	if (compileRes != 0)
	{
		const int linkRes = program.link(EShMsgSpvRules);

		if (linkRes != 0)
		{
			const glslang::TIntermediate* const intermediate = program.getIntermediate(shaderStage);
			glslang::GlslangToSpv(*intermediate, *dst);

			return true;
		}
		else
		{
			log << tcu::TestLog::Message << "Program linking error:\n"
				<< program.getInfoLog() << "\n"
				<< "Source:\n"
				<< source << "\n"
				<< tcu::TestLog::EndMessage;
		}
	}
	else
	{
		log << tcu::TestLog::Message << "Shader compilation error:\n"
			<< shader.getInfoLog() << "\n"
			<< "Source:\n"
			<< source << "\n"
			<< tcu::TestLog::EndMessage;
	}

	return false;
}

void consumer(spv_message_level_t, const char*, const spv_position_t&, const char* m)
{
	std::cerr << "error: " << m << std::endl;
}

void spirvAssemble(ShaderBinaryDataType& dst, const std::string& src)
{
	spvtools::SpirvTools core(SPV_ENV_OPENGL_4_5);

	core.SetMessageConsumer(consumer);

	if (!core.Assemble(src, &dst))
		TCU_THROW(InternalError, "Failed to assemble Spir-V source.");
}

void spirvDisassemble(std::string& dst, const ShaderBinaryDataType& src)
{
	spvtools::SpirvTools core(SPV_ENV_OPENGL_4_5);

	core.SetMessageConsumer(consumer);

	if (!core.Disassemble(src, &dst))
		TCU_THROW(InternalError, "Failed to disassemble Spir-V module.");
}

bool spirvValidate(ShaderBinaryDataType& dst, bool throwOnError)
{
	spvtools::SpirvTools core(SPV_ENV_OPENGL_4_5);

	if (throwOnError)
		core.SetMessageConsumer(consumer);

	if (!core.Validate(dst))
	{
		if (throwOnError)
			TCU_THROW(InternalError, "Failed to validate Spir-V module.");
		return false;
	}

	return true;
}

ShaderBinary makeSpirV(tcu::TestLog& log, ShaderSource source, SpirvVersion version)
{
	ShaderBinary binary;

	if (!spirvUtils::compileGlslToSpirV(log, source.source, source.shaderType, &binary.binary, version))
		TCU_THROW(InternalError, "Failed to convert GLSL to Spir-V");

	binary << source.shaderType << "main";

	return binary;
}

/** Verifying if GLSL to SpirV mapping was performed correctly
 *
 * @param glslSource       GLSL shader template
 * @param spirVSource      SpirV disassembled source
 * @param mappings         Glsl to SpirV mappings vector
 * @param anyOf            any occurence indicator
 *
 * @return true if GLSL code occurs as many times as all of SpirV code for each mapping if anyOf is false
 *         or true if SpirV code occurs at least once if GLSL code found, false otherwise.
 **/
bool verifyMappings(std::string glslSource, std::string spirVSource, SpirVMapping& mappings, bool anyOf)
{
	std::vector<std::string> spirVSourceLines = de::splitString(spirVSource, '\n');

	// Iterate through all glsl functions
	for (SpirVMapping::iterator it = mappings.begin(); it != mappings.end(); it++)
	{
		int glslCodeCount  = 0;
		int spirVCodeCount = 0;

		// To avoid finding functions with similar names (ie. "cos", "acos", "cosh")
		// add characteristic characters that delimits finding results
		std::string glslCode = it->first;

		// Count GLSL code occurrences in GLSL source
		size_t codePosition = glslSource.find(glslCode);
		while (codePosition != std::string::npos)
		{
			glslCodeCount++;
			codePosition = glslSource.find(glslCode, codePosition + 1);
		}

		if (glslCodeCount > 0)
		{
			// Count all SpirV code variants occurrences in SpirV source
			for (int s = 0; s < (signed)it->second.size(); ++s)
			{
				std::vector<std::string> spirVCodes = de::splitString(it->second[s], ' ');

				for (int v = 0; v < (signed)spirVSourceLines.size(); ++v)
				{
					std::vector<std::string> spirVLineCodes = de::splitString(spirVSourceLines[v], ' ');

					bool matchAll = true;
					for (int j = 0; j < (signed)spirVCodes.size(); ++j)
					{
						bool match = false;
						for (int i = 0; i < (signed)spirVLineCodes.size(); ++i)
						{
							if (spirVLineCodes[i] == spirVCodes[j])
								match = true;
						}

						matchAll = matchAll && match;
					}

					if (matchAll)
						spirVCodeCount++;
				}
			}

			// Check if both counts match
			if (anyOf && (glslCodeCount > 0 && spirVCodeCount == 0))
				return false;
			else if (!anyOf && glslCodeCount != spirVCodeCount)
				return false;
		}
	}

	return true;
}

} // namespace spirvUtils

} // namespace glc
