#ifndef _VKSHADERPROGRAM_HPP
#define _VKSHADERPROGRAM_HPP
/*-------------------------------------------------------------------------
 * Vulkan CTS Framework
 * --------------------
 *
 * Copyright (c) 2017 Google Inc.
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
 * \brief Shader (GLSL/HLSL) source program.
 *//*--------------------------------------------------------------------*/

#include "vkDefs.hpp"
#include "gluShaderProgram.hpp"
#include "vkValidatorOptions.hpp"

#include <string>

namespace tcu
{
class TestLog;
} // namespace tcu

namespace vk
{

struct ShaderBuildOptions
{
    enum Flags
    {
        FLAG_USE_STORAGE_BUFFER_STORAGE_CLASS = (1u << 0),
        FLAG_ALLOW_RELAXED_OFFSETS            = (1u << 1), // allow block offsets to follow VK_KHR_relaxed_block_layout
        FLAG_ALLOW_SCALAR_OFFSETS             = (1u << 2), // allow block offsets to follow VK_EXT_scalar_block_layout
        FLAG_ALLOW_STD430_UBOS = (1u << 3), // allow block offsets to follow VK_EXT_uniform_buffer_standard_layout
        FLAG_ALLOW_WORKGROUP_SCALAR_OFFSETS =
            (1u
             << 4), // allow scalar block offsets for Workgroup memory, part of VK_KHR_workgroup_memory_explicit_layout
    };

    uint32_t vulkanVersion;
    SpirvVersion targetVersion;
    uint32_t flags;
    bool supports_VK_KHR_spirv_1_4;

    ShaderBuildOptions(uint32_t vulkanVersion_, SpirvVersion targetVersion_, uint32_t flags_, bool allowSpirv14 = false)
        : vulkanVersion(vulkanVersion_)
        , targetVersion(targetVersion_)
        , flags(flags_)
        , supports_VK_KHR_spirv_1_4(allowSpirv14)
    {
    }

    ShaderBuildOptions(void)
        : vulkanVersion(VK_MAKE_API_VERSION(0, 1, 0, 0))
        , targetVersion(SPIRV_VERSION_1_0)
        , flags(0u)
        , supports_VK_KHR_spirv_1_4(false)
    {
    }

    SpirvValidatorOptions getSpirvValidatorOptions() const
    {
        SpirvValidatorOptions::BlockLayoutRules rules = SpirvValidatorOptions::kDefaultBlockLayout;
        uint32_t validator_flags                      = 0u;

        if (flags & FLAG_ALLOW_SCALAR_OFFSETS)
        {
            rules = SpirvValidatorOptions::kScalarBlockLayout;
        }
        else if (flags & FLAG_ALLOW_STD430_UBOS)
        {
            rules = SpirvValidatorOptions::kUniformStandardLayout;
        }
        else if (flags & FLAG_ALLOW_RELAXED_OFFSETS)
        {
            rules = SpirvValidatorOptions::kRelaxedBlockLayout;
        }

        if (flags & FLAG_ALLOW_WORKGROUP_SCALAR_OFFSETS)
        {
            validator_flags |= SpirvValidatorOptions::FLAG_SPIRV_VALIDATOR_WORKGROUP_SCALAR_BLOCK_LAYOUT;
        }

        return SpirvValidatorOptions(vulkanVersion, rules, supports_VK_KHR_spirv_1_4, validator_flags);
    }
};

enum ShaderLanguage
{
    SHADER_LANGUAGE_GLSL = 0,
    SHADER_LANGUAGE_HLSL,

    SHADER_LANGUAGE_LAST
};

struct GlslSource
{
    static const ShaderLanguage shaderLanguage = SHADER_LANGUAGE_GLSL;
    std::vector<std::string> sources[glu::SHADERTYPE_LAST];
    ShaderBuildOptions buildOptions;

    GlslSource &operator<<(const glu::ShaderSource &shaderSource);
    GlslSource &operator<<(const ShaderBuildOptions &buildOptions_);
};

struct HlslSource
{
    static const ShaderLanguage shaderLanguage = SHADER_LANGUAGE_HLSL;
    std::vector<std::string> sources[glu::SHADERTYPE_LAST];
    ShaderBuildOptions buildOptions;

    HlslSource &operator<<(const glu::ShaderSource &shaderSource);
    HlslSource &operator<<(const ShaderBuildOptions &buildOptions_);
};

tcu::TestLog &operator<<(tcu::TestLog &log, const GlslSource &shaderSource);
tcu::TestLog &operator<<(tcu::TestLog &log, const HlslSource &shaderSource);

} // namespace vk

#endif // _VKSHADERPROGRAM_HPP
