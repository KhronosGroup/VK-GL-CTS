#ifndef VKSHADERTOSPIRV_SLANG_H
#define VKSHADERTOSPIRV_SLANG_H

#define NOMINMAX
#include <windows.h>

#include <vector>
#include <string>
#include "gluShaderProgram.hpp"
#include "vkPrograms.hpp"

namespace Slang
{
	// from vkShaderToSpirV.cpp
	std::string getShaderStageSource(const std::vector<std::string>* sources, const vk::ShaderBuildOptions buildOptions,
		glu::ShaderType shaderType);

	bool compileShaderToSpirV(
		const std::vector<std::string> *sources,
		const vk::ShaderBuildOptions &buildOptions,
		const vk::ShaderLanguage shaderLanguage,
		std::vector<uint32_t> *dst,
		glu::ShaderProgramInfo *buildInfo);

} // namespace Slang

#endif // VKSHADERTOSPIRV_SLANG_H

