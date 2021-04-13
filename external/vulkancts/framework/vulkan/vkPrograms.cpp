/*-------------------------------------------------------------------------
 * Vulkan CTS Framework
 * --------------------
 *
 * Copyright (c) 2019 Google Inc.
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

#include "spirv-tools/optimizer.hpp"

#include "qpInfo.h"

#include "vkPrograms.hpp"
#include "vkShaderToSpirV.hpp"
#include "vkSpirVAsm.hpp"
#include "vkRefUtil.hpp"

#include "deMutex.hpp"
#include "deFilePath.hpp"
#include "deArrayUtil.hpp"
#include "deMemory.h"
#include "deInt32.h"

#include "tcuCommandLine.hpp"

#include <map>

namespace vk
{

using std::string;
using std::vector;
using std::map;

#if defined(DE_DEBUG)
#	define VALIDATE_BINARIES	true
#else
#	define VALIDATE_BINARIES	false
#endif

#define SPIRV_BINARY_ENDIANNESS DE_LITTLE_ENDIAN

// ProgramBinary

ProgramBinary::ProgramBinary (ProgramFormat format, size_t binarySize, const deUint8* binary)
	: m_format	(format)
	, m_binary	(binary, binary+binarySize)
	, m_used	(false)
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

void optimizeCompiledBinary (vector<deUint32>& binary, int optimizationRecipe, const SpirvVersion spirvVersion)
{
	spv_target_env targetEnv = SPV_ENV_VULKAN_1_0;

	// Map SpirvVersion with spv_target_env:
	switch (spirvVersion)
	{
		case SPIRV_VERSION_1_0: targetEnv = SPV_ENV_VULKAN_1_0;	break;
		case SPIRV_VERSION_1_1:
		case SPIRV_VERSION_1_2:
		case SPIRV_VERSION_1_3: targetEnv = SPV_ENV_VULKAN_1_1;	break;
		case SPIRV_VERSION_1_4: targetEnv = SPV_ENV_VULKAN_1_1_SPIRV_1_4;	break;
		case SPIRV_VERSION_1_5: targetEnv = SPV_ENV_VULKAN_1_2;	break;
		default:
			TCU_THROW(InternalError, "Unexpected SPIR-V version requested");
	}

	spvtools::Optimizer optimizer(targetEnv);

	switch (optimizationRecipe)
	{
		case 1:
			optimizer.RegisterPerformancePasses();
			break;
		case 2:
			optimizer.RegisterSizePasses();
			break;
		default:
			TCU_THROW(InternalError, "Unknown optimization recipe requested");
	}

	spvtools::OptimizerOptions optimizer_options;
	optimizer_options.set_run_validator(false);
	const bool ok = optimizer.Run(binary.data(), binary.size(), &binary, optimizer_options);

	if (!ok)
		TCU_THROW(InternalError, "Optimizer call failed");
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

void validateCompiledBinary(const vector<deUint32>& binary, glu::ShaderProgramInfo* buildInfo, const SpirvValidatorOptions& options)
{
	std::ostringstream validationLog;

	if (!validateSpirV(binary.size(), &binary[0], &validationLog, options))
	{
		buildInfo->program.linkOk	 = false;
		buildInfo->program.infoLog	+= "\n" + validationLog.str();

		TCU_THROW(InternalError, "Validation failed for compiled SPIR-V binary");
	}
}

void validateCompiledBinary(const vector<deUint32>& binary, SpirVProgramInfo* buildInfo, const SpirvValidatorOptions& options)
{
	std::ostringstream validationLog;

	if (!validateSpirV(binary.size(), &binary[0], &validationLog, options))
	{
		buildInfo->compileOk = false;
		buildInfo->infoLog += "\n" + validationLog.str();

		TCU_THROW(InternalError, "Validation failed for compiled SPIR-V binary");
	}
}

de::Mutex							cacheFileMutex;
map<deUint32, vector<deUint32> >	cacheFileIndex;
bool								cacheFileFirstRun = true;

void shaderCacheFirstRunCheck (const char* shaderCacheFile, bool truncate)
{
	cacheFileMutex.lock();
	if (cacheFileFirstRun)
	{
		cacheFileFirstRun = false;
		if (truncate)
		{
			// Open file with "w" access to truncate it
			FILE* f = fopen(shaderCacheFile, "wb");
			if (f)
				fclose(f);
		}
		else
		{
			// Parse chunked shader cache file for hashes and offsets
			FILE* file = fopen(shaderCacheFile, "rb");
			int count = 0;
			if (file)
			{
				deUint32 chunksize	= 0;
				deUint32 hash		= 0;
				deUint32 offset		= 0;
				bool ok				= true;
				while (ok)
				{
					offset = (deUint32)ftell(file);
					if (ok) ok = fread(&chunksize, 1, 4, file)				== 4;
					if (ok) ok = fread(&hash, 1, 4, file)					== 4;
					if (ok) cacheFileIndex[hash].push_back(offset);
					if (ok) ok = fseek(file, offset + chunksize, SEEK_SET)	== 0;
					count++;
				}
				fclose(file);
			}
		}
	}
	cacheFileMutex.unlock();
}

std::string intToString (deUint32 integer)
{
	std::stringstream temp_sstream;

	temp_sstream << integer;

	return temp_sstream.str();
}

// 32-bit FNV-1 hash
deUint32 shadercacheHash (const char* str)
{
	deUint32 hash = 0x811c9dc5;
	deUint32 c;
	while ((c = (deUint32)*str++) != 0)
	{
		hash *= 16777619;
		hash ^= c;
	}
	return hash;
}

vk::ProgramBinary* shadercacheLoad (const std::string& shaderstring, const char* shaderCacheFilename)
{
	deUint32		hash		= shadercacheHash(shaderstring.c_str());
	deInt32			format;
	deInt32			length;
	deInt32			sourcelength;
	deUint32		i;
	deUint32		temp;
	deUint8*		bin			= 0;
	char*			source		= 0;
	deBool			ok			= true;
	deBool			diff		= true;
	cacheFileMutex.lock();

	if (cacheFileIndex.count(hash) == 0)
	{
		cacheFileMutex.unlock();
		return 0;
	}
	FILE*			file		= fopen(shaderCacheFilename, "rb");
	ok				= file											!= 0;

	for (i = 0; i < cacheFileIndex[hash].size(); i++)
	{
		if (ok) ok = fseek(file, cacheFileIndex[hash][i], SEEK_SET)	== 0;
		if (ok) ok = fread(&temp, 1, 4, file)						== 4; // Chunk size (skip)
		if (ok) ok = fread(&temp, 1, 4, file)						== 4; // Stored hash
		if (ok) ok = temp											== hash; // Double check
		if (ok) ok = fread(&format, 1, 4, file)						== 4;
		if (ok) ok = fread(&length, 1, 4, file)						== 4;
		if (ok) ok = length											> 0; // sanity check
		if (ok) bin = new deUint8[length];
		if (ok) ok = fread(bin, 1, length, file)					== (size_t)length;
		if (ok) ok = fread(&sourcelength, 1, 4, file)				== 4;
		if (ok && sourcelength > 0)
		{
			source = new char[sourcelength + 1];
			ok = fread(source, 1, sourcelength, file)				== (size_t)sourcelength;
			source[sourcelength] = 0;
			diff = shaderstring != std::string(source);
		}
		if (!ok || diff)
		{
			// Mismatch, but may still exist in cache if there were hash collisions
			delete[] source;
			delete[] bin;
		}
		else
		{
			delete[] source;
			if (file) fclose(file);
			cacheFileMutex.unlock();
			vk::ProgramBinary* res = new vk::ProgramBinary((vk::ProgramFormat)format, length, bin);
			delete[] bin;
			return res;
		}
	}
	if (file) fclose(file);
	cacheFileMutex.unlock();
	return 0;
}

void shadercacheSave (const vk::ProgramBinary* binary, const std::string& shaderstring, const char* shaderCacheFilename)
{
	if (binary == 0)
		return;
	deUint32			hash		= shadercacheHash(shaderstring.c_str());
	deInt32				format		= binary->getFormat();
	deUint32			length		= (deUint32)binary->getSize();
	deUint32			chunksize;
	deUint32			offset;
	const deUint8*		bin			= binary->getBinary();
	const de::FilePath	filePath	(shaderCacheFilename);

	cacheFileMutex.lock();

	if (cacheFileIndex[hash].size())
	{
		FILE*			file		= fopen(shaderCacheFilename, "rb");
		deBool			ok			= (file != 0);
		deBool			diff		= DE_TRUE;
		deInt32			sourcelength;
		deUint32		i;
		deUint32		temp;

		for (i = 0; i < cacheFileIndex[hash].size(); i++)
		{
			deUint32	cachedLength	= 0;

			if (ok) ok = fseek(file, cacheFileIndex[hash][i], SEEK_SET)	== 0;
			if (ok) ok = fread(&temp, 1, 4, file)						== 4; // Chunk size (skip)
			if (ok) ok = fread(&temp, 1, 4, file)						== 4; // Stored hash
			if (ok) ok = temp											== hash; // Double check
			if (ok) ok = fread(&temp, 1, 4, file)						== 4;
			if (ok) ok = fread(&cachedLength, 1, 4, file)				== 4;
			if (ok) ok = cachedLength									> 0; // sanity check
			if (ok) fseek(file, cachedLength, SEEK_CUR); // skip binary
			if (ok) ok = fread(&sourcelength, 1, 4, file)				== 4;

			if (ok && sourcelength > 0)
			{
				char* source;
				source	= new char[sourcelength + 1];
				ok		= fread(source, 1, sourcelength, file)			== (size_t)sourcelength;
				source[sourcelength] = 0;
				diff	= shaderstring != std::string(source);
				delete[] source;
			}

			if (ok && !diff)
			{
				// Already in cache (written by another thread, probably)
				fclose(file);
				cacheFileMutex.unlock();
				return;
			}
		}
		fclose(file);
	}

	if (!de::FilePath(filePath.getDirName()).exists())
		de::createDirectoryAndParents(filePath.getDirName().c_str());

	FILE*				file		= fopen(shaderCacheFilename, "ab");
	if (!file)
	{
		cacheFileMutex.unlock();
		return;
	}
	// Append mode starts writing from the end of the file,
	// but unless we do a seek, ftell returns 0.
	fseek(file, 0, SEEK_END);
	offset		= (deUint32)ftell(file);
	chunksize	= 4 + 4 + 4 + 4 + length + 4 + (deUint32)shaderstring.length();
	fwrite(&chunksize, 1, 4, file);
	fwrite(&hash, 1, 4, file);
	fwrite(&format, 1, 4, file);
	fwrite(&length, 1, 4, file);
	fwrite(bin, 1, length, file);
	length = (deUint32)shaderstring.length();
	fwrite(&length, 1, 4, file);
	fwrite(shaderstring.c_str(), 1, length, file);
	fclose(file);
	cacheFileIndex[hash].push_back(offset);

	cacheFileMutex.unlock();
}

// Insert any information that may affect compilation into the shader string.
void getCompileEnvironment (std::string& shaderstring)
{
	shaderstring += "GLSL:";
	shaderstring += qpGetReleaseGlslName();
	shaderstring += "\nSpir-v Tools:";
	shaderstring += qpGetReleaseSpirvToolsName();
	shaderstring += "\nSpir-v Headers:";
	shaderstring += qpGetReleaseSpirvHeadersName();
	shaderstring += "\n";
}

// Insert compilation options into the shader string.
void getBuildOptions (std::string& shaderstring, const ShaderBuildOptions& buildOptions, int optimizationRecipe)
{
	shaderstring += "Target Spir-V ";
	shaderstring += getSpirvVersionName(buildOptions.targetVersion);
	shaderstring += "\n";
	if (buildOptions.flags & ShaderBuildOptions::FLAG_ALLOW_RELAXED_OFFSETS)
		shaderstring += "Flag:Allow relaxed offsets\n";
	if (buildOptions.flags & ShaderBuildOptions::FLAG_USE_STORAGE_BUFFER_STORAGE_CLASS)
		shaderstring += "Flag:Use storage buffer storage class\n";
	if (optimizationRecipe != 0)
	{
		shaderstring += "Optimization recipe ";
		shaderstring += de::toString(optimizationRecipe);
		shaderstring += "\n";
	}
}

ProgramBinary* buildProgram (const GlslSource& program, glu::ShaderProgramInfo* buildInfo, const tcu::CommandLine& commandLine)
{
	const SpirvVersion	spirvVersion		= program.buildOptions.targetVersion;
	const bool			validateBinary		= VALIDATE_BINARIES;
	vector<deUint32>	binary;
	std::string			cachekey;
	std::string			shaderstring;
	vk::ProgramBinary*	res					= 0;
	const int			optimizationRecipe	= commandLine.getOptimizationRecipe();

	if (commandLine.isShadercacheEnabled())
	{
		shaderCacheFirstRunCheck(commandLine.getShaderCacheFilename(), commandLine.isShaderCacheTruncateEnabled());
		getCompileEnvironment(cachekey);
		getBuildOptions(cachekey, program.buildOptions, optimizationRecipe);

		for (int i = 0; i < glu::SHADERTYPE_LAST; i++)
		{
			if (!program.sources[i].empty())
			{
				cachekey += glu::getShaderTypeName((glu::ShaderType)i);

				for (std::vector<std::string>::const_iterator it = program.sources[i].begin(); it != program.sources[i].end(); ++it)
					shaderstring += *it;
			}
		}

		cachekey = cachekey + shaderstring;

		res = shadercacheLoad(cachekey, commandLine.getShaderCacheFilename());

		if (res)
		{
			buildInfo->program.infoLog		= "Loaded from cache";
			buildInfo->program.linkOk		= true;
			buildInfo->program.linkTimeUs	= 0;

			for (int shaderType = 0; shaderType < glu::SHADERTYPE_LAST; shaderType++)
			{
				if (!program.sources[shaderType].empty())
				{
					glu::ShaderInfo	shaderBuildInfo;

					shaderBuildInfo.type			= (glu::ShaderType)shaderType;
					shaderBuildInfo.source			= shaderstring;
					shaderBuildInfo.compileTimeUs	= 0;
					shaderBuildInfo.compileOk		= true;

					buildInfo->shaders.push_back(shaderBuildInfo);
				}
			}
		}
	}

	if (!res)
	{
		{
			vector<deUint32> nonStrippedBinary;

			if (!compileGlslToSpirV(program, &nonStrippedBinary, buildInfo))
				TCU_THROW(InternalError, "Compiling GLSL to SPIR-V failed");

			TCU_CHECK_INTERNAL(!nonStrippedBinary.empty());
			stripSpirVDebugInfo(nonStrippedBinary.size(), &nonStrippedBinary[0], &binary);
			TCU_CHECK_INTERNAL(!binary.empty());
		}

		if (optimizationRecipe != 0)
		{
			validateCompiledBinary(binary, buildInfo, program.buildOptions.getSpirvValidatorOptions());
			optimizeCompiledBinary(binary, optimizationRecipe, spirvVersion);
		}

		if (validateBinary)
		{
			validateCompiledBinary(binary, buildInfo, program.buildOptions.getSpirvValidatorOptions());
		}

		res = createProgramBinaryFromSpirV(binary);
		if (commandLine.isShadercacheEnabled())
			shadercacheSave(res, cachekey, commandLine.getShaderCacheFilename());
	}
	return res;
}

ProgramBinary* buildProgram (const HlslSource& program, glu::ShaderProgramInfo* buildInfo, const tcu::CommandLine& commandLine)
{
	const SpirvVersion	spirvVersion		= program.buildOptions.targetVersion;
	const bool			validateBinary		= VALIDATE_BINARIES;
	vector<deUint32>	binary;
	std::string			cachekey;
	std::string			shaderstring;
	vk::ProgramBinary*	res					= 0;
	const int			optimizationRecipe	= commandLine.getOptimizationRecipe();

	if (commandLine.isShadercacheEnabled())
	{
		shaderCacheFirstRunCheck(commandLine.getShaderCacheFilename(), commandLine.isShaderCacheTruncateEnabled());
		getCompileEnvironment(cachekey);
		getBuildOptions(cachekey, program.buildOptions, optimizationRecipe);

		for (int i = 0; i < glu::SHADERTYPE_LAST; i++)
		{
			if (!program.sources[i].empty())
			{
				cachekey += glu::getShaderTypeName((glu::ShaderType)i);

				for (std::vector<std::string>::const_iterator it = program.sources[i].begin(); it != program.sources[i].end(); ++it)
					shaderstring += *it;
			}
		}

		cachekey = cachekey + shaderstring;

		res = shadercacheLoad(cachekey, commandLine.getShaderCacheFilename());

		if (res)
		{
			buildInfo->program.infoLog		= "Loaded from cache";
			buildInfo->program.linkOk		= true;
			buildInfo->program.linkTimeUs	= 0;

			for (int shaderType = 0; shaderType < glu::SHADERTYPE_LAST; shaderType++)
			{
				if (!program.sources[shaderType].empty())
				{
					glu::ShaderInfo	shaderBuildInfo;

					shaderBuildInfo.type			= (glu::ShaderType)shaderType;
					shaderBuildInfo.source			= shaderstring;
					shaderBuildInfo.compileTimeUs	= 0;
					shaderBuildInfo.compileOk		= true;

					buildInfo->shaders.push_back(shaderBuildInfo);
				}
			}
		}
	}

	if (!res)
	{
		{
			vector<deUint32> nonStrippedBinary;

			if (!compileHlslToSpirV(program, &nonStrippedBinary, buildInfo))
				TCU_THROW(InternalError, "Compiling HLSL to SPIR-V failed");

			TCU_CHECK_INTERNAL(!nonStrippedBinary.empty());
			stripSpirVDebugInfo(nonStrippedBinary.size(), &nonStrippedBinary[0], &binary);
			TCU_CHECK_INTERNAL(!binary.empty());
		}

		if (optimizationRecipe != 0)
		{
			validateCompiledBinary(binary, buildInfo, program.buildOptions.getSpirvValidatorOptions());
			optimizeCompiledBinary(binary, optimizationRecipe, spirvVersion);
		}

		if (validateBinary)
		{
			validateCompiledBinary(binary, buildInfo, program.buildOptions.getSpirvValidatorOptions());
		}

		res = createProgramBinaryFromSpirV(binary);
		if (commandLine.isShadercacheEnabled())
			shadercacheSave(res, cachekey, commandLine.getShaderCacheFilename());
	}
	return res;
}

ProgramBinary* assembleProgram (const SpirVAsmSource& program, SpirVProgramInfo* buildInfo, const tcu::CommandLine& commandLine)
{
	const SpirvVersion	spirvVersion		= program.buildOptions.targetVersion;
	const bool			validateBinary		= VALIDATE_BINARIES;
	vector<deUint32>	binary;
	vk::ProgramBinary*	res					= 0;
	std::string			cachekey;
	const int			optimizationRecipe	= commandLine.isSpirvOptimizationEnabled() ? commandLine.getOptimizationRecipe() : 0;

	if (commandLine.isShadercacheEnabled())
	{
		shaderCacheFirstRunCheck(commandLine.getShaderCacheFilename(), commandLine.isShaderCacheTruncateEnabled());
		getCompileEnvironment(cachekey);
		cachekey += "Target Spir-V ";
		cachekey += getSpirvVersionName(spirvVersion);
		cachekey += "\n";
		if (optimizationRecipe != 0)
		{
			cachekey += "Optimization recipe ";
			cachekey += de::toString(optimizationRecipe);
			cachekey += "\n";
		}

		cachekey += program.source;

		res = shadercacheLoad(cachekey, commandLine.getShaderCacheFilename());

		if (res)
		{
			buildInfo->source			= program.source;
			buildInfo->compileOk		= true;
			buildInfo->compileTimeUs	= 0;
			buildInfo->infoLog			= "Loaded from cache";
		}
	}

	if (!res)
	{

		if (!assembleSpirV(&program, &binary, buildInfo, spirvVersion))
			TCU_THROW(InternalError, "Failed to assemble SPIR-V");

		if (optimizationRecipe != 0)
		{
			validateCompiledBinary(binary, buildInfo, program.buildOptions.getSpirvValidatorOptions());
			optimizeCompiledBinary(binary, optimizationRecipe, spirvVersion);
		}

		if (validateBinary)
		{
			validateCompiledBinary(binary, buildInfo, program.buildOptions.getSpirvValidatorOptions());
		}

		res = createProgramBinaryFromSpirV(binary);
		if (commandLine.isShadercacheEnabled())
			shadercacheSave(res, cachekey, commandLine.getShaderCacheFilename());
	}
	return res;
}

void disassembleProgram (const ProgramBinary& program, std::ostream* dst)
{
	if (program.getFormat() == PROGRAM_FORMAT_SPIRV)
	{
		TCU_CHECK_INTERNAL(isSaneSpirVBinary(program));

		if (isNativeSpirVBinaryEndianness())
			disassembleSpirV(program.getSize()/sizeof(deUint32), (const deUint32*)program.getBinary(), dst,
							 extractSpirvVersion(program));
		else
			TCU_THROW(InternalError, "SPIR-V endianness translation not supported");
	}
	else
		TCU_THROW(NotSupportedError, "Unsupported program format");
}

bool validateProgram (const ProgramBinary& program, std::ostream* dst, const SpirvValidatorOptions& options)
{
	if (program.getFormat() == PROGRAM_FORMAT_SPIRV)
	{
		if (!isSaneSpirVBinary(program))
		{
			*dst << "Binary doesn't look like SPIR-V at all";
			return false;
		}

		if (isNativeSpirVBinaryEndianness())
			return validateSpirV(program.getSize()/sizeof(deUint32), (const deUint32*)program.getBinary(), dst, options);
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

		binary.setUsed();

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
		VK_SHADER_STAGE_COMPUTE_BIT,
		VK_SHADER_STAGE_RAYGEN_BIT_NV,
		VK_SHADER_STAGE_ANY_HIT_BIT_NV,
		VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV,
		VK_SHADER_STAGE_MISS_BIT_NV,
		VK_SHADER_STAGE_INTERSECTION_BIT_NV,
		VK_SHADER_STAGE_CALLABLE_BIT_NV,
	};

	return de::getSizedArrayElement<glu::SHADERTYPE_LAST>(s_shaderStages, shaderType);
}

// Baseline version, to be used for shaders which don't specify a version
vk::SpirvVersion getBaselineSpirvVersion (const deUint32 /* vulkanVersion */)
{
	return vk::SPIRV_VERSION_1_0;
}

// Max supported versions for each Vulkan version, without requiring a Vulkan extension.
vk::SpirvVersion getMaxSpirvVersionForVulkan (const deUint32 vulkanVersion)
{
	vk::SpirvVersion	result			= vk::SPIRV_VERSION_LAST;

	deUint32 vulkanVersionMajorMinor = VK_MAKE_VERSION(VK_API_VERSION_MAJOR(vulkanVersion), VK_API_VERSION_MINOR(vulkanVersion), 0);
	if (vulkanVersionMajorMinor == VK_API_VERSION_1_0)
		result = vk::SPIRV_VERSION_1_0;
	else if (vulkanVersionMajorMinor == VK_API_VERSION_1_1)
		result = vk::SPIRV_VERSION_1_3;
	else if (vulkanVersionMajorMinor >= VK_API_VERSION_1_2)
		result = vk::SPIRV_VERSION_1_5;

	DE_ASSERT(result < vk::SPIRV_VERSION_LAST);

	return result;
}

vk::SpirvVersion getMaxSpirvVersionForAsm (const deUint32 vulkanVersion)
{
	return getMaxSpirvVersionForVulkan(vulkanVersion);
}

vk::SpirvVersion getMaxSpirvVersionForGlsl (const deUint32 vulkanVersion)
{
	return getMaxSpirvVersionForVulkan(vulkanVersion);
}

SpirvVersion extractSpirvVersion (const ProgramBinary& binary)
{
	DE_STATIC_ASSERT(SPIRV_VERSION_1_5 + 1 == SPIRV_VERSION_LAST);

	if (binary.getFormat() != PROGRAM_FORMAT_SPIRV)
		TCU_THROW(InternalError, "Binary is not in SPIR-V format");

	if (!isSaneSpirVBinary(binary) || binary.getSize() < sizeof(SpirvBinaryHeader))
		TCU_THROW(InternalError, "Invalid SPIR-V header format");

	const deUint32				spirvBinaryVersion10	= 0x00010000;
	const deUint32				spirvBinaryVersion11	= 0x00010100;
	const deUint32				spirvBinaryVersion12	= 0x00010200;
	const deUint32				spirvBinaryVersion13	= 0x00010300;
	const deUint32				spirvBinaryVersion14	= 0x00010400;
	const deUint32				spirvBinaryVersion15	= 0x00010500;
	const SpirvBinaryHeader*	header					= reinterpret_cast<const SpirvBinaryHeader*>(binary.getBinary());
	const deUint32				spirvVersion			= isNativeSpirVBinaryEndianness()
														? header->version
														: deReverseBytes32(header->version);
	SpirvVersion				result					= SPIRV_VERSION_LAST;

	switch (spirvVersion)
	{
		case spirvBinaryVersion10:	result = SPIRV_VERSION_1_0; break; //!< SPIR-V 1.0
		case spirvBinaryVersion11:	result = SPIRV_VERSION_1_1; break; //!< SPIR-V 1.1
		case spirvBinaryVersion12:	result = SPIRV_VERSION_1_2; break; //!< SPIR-V 1.2
		case spirvBinaryVersion13:	result = SPIRV_VERSION_1_3; break; //!< SPIR-V 1.3
		case spirvBinaryVersion14:	result = SPIRV_VERSION_1_4; break; //!< SPIR-V 1.4
		case spirvBinaryVersion15:	result = SPIRV_VERSION_1_5; break; //!< SPIR-V 1.5
		default:					TCU_THROW(InternalError, "Unknown SPIR-V version detected in binary");
	}

	return result;
}

std::string getSpirvVersionName (const SpirvVersion spirvVersion)
{
	DE_STATIC_ASSERT(SPIRV_VERSION_1_5 + 1 == SPIRV_VERSION_LAST);
	DE_ASSERT(spirvVersion < SPIRV_VERSION_LAST);

	std::string result;

	switch (spirvVersion)
	{
		case SPIRV_VERSION_1_0: result = "1.0"; break; //!< SPIR-V 1.0
		case SPIRV_VERSION_1_1: result = "1.1"; break; //!< SPIR-V 1.1
		case SPIRV_VERSION_1_2: result = "1.2"; break; //!< SPIR-V 1.2
		case SPIRV_VERSION_1_3: result = "1.3"; break; //!< SPIR-V 1.3
		case SPIRV_VERSION_1_4: result = "1.4"; break; //!< SPIR-V 1.4
		case SPIRV_VERSION_1_5: result = "1.5"; break; //!< SPIR-V 1.5
		default:				result = "Unknown";
	}

	return result;
}

SpirvVersion& operator++(SpirvVersion& spirvVersion)
{
	if (spirvVersion == SPIRV_VERSION_LAST)
		spirvVersion = SPIRV_VERSION_1_0;
	else
		spirvVersion = static_cast<SpirvVersion>(static_cast<deUint32>(spirvVersion) + 1);

	return spirvVersion;
}

} // vk
