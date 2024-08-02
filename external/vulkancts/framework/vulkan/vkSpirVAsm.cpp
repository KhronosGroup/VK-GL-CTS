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
 * \brief SPIR-V assembly to binary.
 *//*--------------------------------------------------------------------*/

#include "vkSpirVAsm.hpp"
#include "vkSpirVProgram.hpp"
#include "deClock.h"

#include <algorithm>

#include "spirv-tools/libspirv.h"

namespace vk
{

using std::string;
using std::vector;

// Returns the SPIRV-Tools target environment enum for the given dEQP Spirv validator options object.
// Do this here instead of as a method on SpirvValidatorOptions because only this file has access to
// the SPIRV-Tools headers.
static spv_target_env getSpirvToolsEnvForValidatorOptions(SpirvValidatorOptions opts)
{
    const bool allow_1_4 = opts.supports_VK_KHR_spirv_1_4;
    switch (opts.vulkanVersion)
    {
    case VK_MAKE_API_VERSION(0, 1, 0, 0):
        return SPV_ENV_VULKAN_1_0;
    case VK_MAKE_API_VERSION(0, 1, 1, 0):
        return allow_1_4 ? SPV_ENV_VULKAN_1_1_SPIRV_1_4 : SPV_ENV_VULKAN_1_1;
    case VK_MAKE_API_VERSION(0, 1, 2, 0):
        return SPV_ENV_VULKAN_1_2;
    case VK_MAKE_API_VERSION(1, 1, 0, 0):
        return SPV_ENV_VULKAN_1_2;
    case VK_MAKE_API_VERSION(0, 1, 3, 0):
        return SPV_ENV_VULKAN_1_3;
    default:
        break;
    }
    TCU_THROW(InternalError, "Unexpected Vulkan Version version requested");
    return SPV_ENV_VULKAN_1_0;
}

static spv_target_env mapTargetSpvEnvironment(SpirvVersion spirvVersion)
{
    spv_target_env result = SPV_ENV_UNIVERSAL_1_0;

    switch (spirvVersion)
    {
    case SPIRV_VERSION_1_0:
        result = SPV_ENV_UNIVERSAL_1_0;
        break; //!< SPIR-V 1.0
    case SPIRV_VERSION_1_1:
        result = SPV_ENV_UNIVERSAL_1_1;
        break; //!< SPIR-V 1.1
    case SPIRV_VERSION_1_2:
        result = SPV_ENV_UNIVERSAL_1_2;
        break; //!< SPIR-V 1.2
    case SPIRV_VERSION_1_3:
        result = SPV_ENV_UNIVERSAL_1_3;
        break; //!< SPIR-V 1.3
    case SPIRV_VERSION_1_4:
        result = SPV_ENV_UNIVERSAL_1_4;
        break; //!< SPIR-V 1.4
    case SPIRV_VERSION_1_5:
        result = SPV_ENV_UNIVERSAL_1_5;
        break; //!< SPIR-V 1.5
    case SPIRV_VERSION_1_6:
        result = SPV_ENV_UNIVERSAL_1_6;
        break; //!< SPIR-V 1.6
    default:
        TCU_THROW(InternalError, "Unknown SPIR-V version");
    }

    return result;
}

bool assembleSpirV(const SpirVAsmSource *program, std::vector<uint32_t> *dst, SpirVProgramInfo *buildInfo,
                   SpirvVersion spirvVersion)
{
    const spv_context context = spvContextCreate(mapTargetSpvEnvironment(spirvVersion));
    spv_binary binary         = nullptr;
    spv_diagnostic diagnostic = nullptr;

    if (!context)
        throw std::bad_alloc();

    try
    {
        const std::string &spvSource    = program->source;
        const uint64_t compileStartTime = deGetMicroseconds();
        const uint32_t options          = SPV_TEXT_TO_BINARY_OPTION_PRESERVE_NUMERIC_IDS;
        const spv_result_t compileOk =
            spvTextToBinaryWithOptions(context, spvSource.c_str(), spvSource.size(), options, &binary, &diagnostic);

        buildInfo->source        = spvSource;
        buildInfo->infoLog       = diagnostic ? diagnostic->error : ""; // \todo [2015-07-13 pyry] Include debug log?
        buildInfo->compileTimeUs = deGetMicroseconds() - compileStartTime;
        buildInfo->compileOk     = (compileOk == SPV_SUCCESS);

        if (buildInfo->compileOk)
        {
            DE_ASSERT(binary->wordCount > 0);
            dst->resize(binary->wordCount);
            std::copy(&binary->code[0], &binary->code[0] + binary->wordCount, dst->begin());
        }

        spvBinaryDestroy(binary);
        spvDiagnosticDestroy(diagnostic);
        spvContextDestroy(context);

        return compileOk == SPV_SUCCESS;
    }
    catch (...)
    {
        spvBinaryDestroy(binary);
        spvDiagnosticDestroy(diagnostic);
        spvContextDestroy(context);

        throw;
    }
}

void disassembleSpirV(size_t binarySizeInWords, const uint32_t *binary, std::ostream *dst, SpirvVersion spirvVersion)
{
    const spv_context context = spvContextCreate(mapTargetSpvEnvironment(spirvVersion));
    spv_text text             = nullptr;
    spv_diagnostic diagnostic = nullptr;

    if (!context)
        throw std::bad_alloc();

    try
    {
        const spv_result_t result = spvBinaryToText(context, binary, binarySizeInWords, 0, &text, &diagnostic);

        if (result != SPV_SUCCESS)
            TCU_THROW(InternalError, "Disassembling SPIR-V failed");

        *dst << text->str;

        spvTextDestroy(text);
        spvDiagnosticDestroy(diagnostic);
        spvContextDestroy(context);
    }
    catch (...)
    {
        spvTextDestroy(text);
        spvDiagnosticDestroy(diagnostic);
        spvContextDestroy(context);

        throw;
    }
}

bool validateSpirV(size_t binarySizeInWords, const uint32_t *binary, std::ostream *infoLog,
                   const SpirvValidatorOptions &val_options)
{
    const spv_context context     = spvContextCreate(getSpirvToolsEnvForValidatorOptions(val_options));
    spv_diagnostic diagnostic     = nullptr;
    spv_validator_options options = nullptr;
    spv_text disasmText           = nullptr;

    if (!context)
        throw std::bad_alloc();

    try
    {
        spv_const_binary_t cbinary = {binary, binarySizeInWords};

        options = spvValidatorOptionsCreate();

        if (options == nullptr)
            throw std::bad_alloc();

        switch (val_options.blockLayout)
        {
        case SpirvValidatorOptions::kDefaultBlockLayout:
            break;
        case SpirvValidatorOptions::kNoneBlockLayout:
            spvValidatorOptionsSetSkipBlockLayout(options, true);
            break;
        case SpirvValidatorOptions::kRelaxedBlockLayout:
            spvValidatorOptionsSetRelaxBlockLayout(options, true);
            break;
        case SpirvValidatorOptions::kUniformStandardLayout:
            spvValidatorOptionsSetUniformBufferStandardLayout(options, true);
            break;
        case SpirvValidatorOptions::kScalarBlockLayout:
            spvValidatorOptionsSetScalarBlockLayout(options, true);
            break;
        }

        if (val_options.flags & SpirvValidatorOptions::FLAG_SPIRV_VALIDATOR_WORKGROUP_SCALAR_BLOCK_LAYOUT)
        {
            spvValidatorOptionsSetWorkgroupScalarBlockLayout(options, true);
        }

        if (val_options.flags & SpirvValidatorOptions::FLAG_SPIRV_VALIDATOR_ALLOW_LOCALSIZEID)
            spvValidatorOptionsSetAllowLocalSizeId(options, true);

        const spv_result_t valid = spvValidateWithOptions(context, options, &cbinary, &diagnostic);
        const bool passed        = (valid == SPV_SUCCESS);

        *infoLog << "Validation " << (passed ? "PASSED: " : "FAILED: ");

        if (diagnostic && diagnostic->error)
        {
            // Print the diagnostic whether validation passes or fails.
            // In theory we could get a warning even in the pass case, but there are no cases
            // like that now.
            *infoLog << diagnostic->error << "\n";

            const uint32_t disasmOptions = SPV_BINARY_TO_TEXT_OPTION_FRIENDLY_NAMES | SPV_BINARY_TO_TEXT_OPTION_INDENT;
            const spv_result_t disasmResult =
                spvBinaryToText(context, binary, binarySizeInWords, disasmOptions, &disasmText, nullptr);

            if (disasmResult != SPV_SUCCESS)
                *infoLog << "Disassembly failed with code: " << de::toString(disasmResult) << "\n";

            if (disasmText != nullptr)
                *infoLog << disasmText->str << "\n";
        }

        spvTextDestroy(disasmText);
        spvValidatorOptionsDestroy(options);
        spvDiagnosticDestroy(diagnostic);
        spvContextDestroy(context);

        return passed;
    }
    catch (...)
    {
        spvTextDestroy(disasmText);
        spvValidatorOptionsDestroy(options);
        spvDiagnosticDestroy(diagnostic);
        spvContextDestroy(context);

        throw;
    }
}

} // namespace vk
