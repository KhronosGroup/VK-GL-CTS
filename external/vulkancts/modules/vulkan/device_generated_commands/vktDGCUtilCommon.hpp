#ifndef _VKTDGCUTILCOMMON_HPP
#define _VKTDGCUTILCOMMON_HPP
/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2024 The Khronos Group Inc.
 * Copyright (c) 2024 Valve Corporation.
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
 * \brief Device Generated Commands Common (NV and EXT) Utility Code
 *//*--------------------------------------------------------------------*/
#include "vkDefs.hpp"
#include "deMemory.h"

#include <vector>

namespace vkt
{
namespace DGC
{

// Returns true if the two memory requirements structures are equal.
bool equalMemoryRequirements(const vk::VkMemoryRequirements &, const vk::VkMemoryRequirements &);

// Push back an element of any type onto an std::vector (of uint8_t, uint32_t, etc).
// This is helpful to push items into a pseudobuffer that should contain DGC data.
template <typename T, typename K>
void pushBackElement(std::vector<T> &out, const K &element)
{
    constexpr auto vecItemSize = sizeof(T);
    constexpr auto elementSize = sizeof(K);
    constexpr auto neededItems = (elementSize + vecItemSize - 1u) / vecItemSize;

    DE_ASSERT(neededItems > 0u);
    const auto prevSize = out.size();
    out.resize(prevSize + neededItems);
    const auto basePtr = &out.at(prevSize); // Important to take this after resizing, not before.
    deMemcpy(basePtr, &element, sizeof(element));
};

} // namespace DGC
} // namespace vkt

#endif // _VKTDGCUTILCOMMON_HPP
