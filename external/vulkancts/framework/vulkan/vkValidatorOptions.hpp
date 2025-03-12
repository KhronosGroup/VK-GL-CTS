#ifndef _VKVALIDATOROPTIONS_HPP
#define _VKVALIDATOROPTIONS_HPP
/*-------------------------------------------------------------------------
 * Vulkan CTS Framework
 * --------------------
 *
 * Copyright (c) 2018 Google LLC
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
 * \brief SPIR-V validator options
 *//*--------------------------------------------------------------------*/

#include "vkDefs.hpp"

namespace vk
{

struct SpirvValidatorOptions
{
    enum BlockLayoutRules
    {
        // The default for the target Vulkan environment.
        kDefaultBlockLayout,
        // Don't check block layout
        kNoneBlockLayout,
        // VK_KHR_relaxed_block_layout
        kRelaxedBlockLayout,
        // VK_EXT_uniform_buffer_standard_layout
        kUniformStandardLayout,
        // VK_EXT_scalar_block_layout
        kScalarBlockLayout
    };

    enum Flags
    {
        FLAG_SPIRV_VALIDATOR_WORKGROUP_SCALAR_BLOCK_LAYOUT = (1u << 0),
        FLAG_SPIRV_VALIDATOR_ALLOW_LOCALSIZEID             = (1u << 1)
    };

    SpirvValidatorOptions(uint32_t the_vulkan_version = VK_MAKE_API_VERSION(0, 1, 0, 0),
                          BlockLayoutRules the_layout = kDefaultBlockLayout, bool allowSpirv14 = false,
                          uint32_t the_flags = 0)
        : vulkanVersion(the_vulkan_version)
        , blockLayout(the_layout)
        , supports_VK_KHR_spirv_1_4(allowSpirv14)
        , flags(the_flags)
    {
    }

    // The target Vulkan version.  This determines the SPIR-V environment rules to
    // be checked. The bit pattern is as produced by VK_MAKE_API_VERSION.
    uint32_t vulkanVersion;

    // The block layout rules to enforce.
    BlockLayoutRules blockLayout;

    // Does the device support VK_KHR_spirv_1_4?
    // (Camelcase would just be wrong here.)
    bool supports_VK_KHR_spirv_1_4;

    uint32_t flags;
};

} // namespace vk

#endif // _VKVALIDATOROPTIONS_HPP
