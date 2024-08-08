#ifndef _VKAPIVERSION_HPP
#define _VKAPIVERSION_HPP
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
 * \brief Vulkan api version.
 *//*--------------------------------------------------------------------*/

#include "vkDefs.hpp"

#include <ostream>

namespace vk
{

struct ApiVersion
{
    uint32_t variantNum;
    uint32_t majorNum;
    uint32_t minorNum;
    uint32_t patchNum;

    ApiVersion(uint32_t variantNum_, uint32_t majorNum_, uint32_t minorNum_, uint32_t patchNum_)
        : variantNum(variantNum_)
        , majorNum(majorNum_)
        , minorNum(minorNum_)
        , patchNum(patchNum_)
    {
    }
};

ApiVersion unpackVersion(uint32_t version);
uint32_t pack(const ApiVersion &version);

bool isApiVersionEqual(uint32_t lhs, uint32_t rhs);
bool isApiVersionPredecessor(uint32_t version, uint32_t predVersion);
bool isApiVersionSupported(uint32_t yourVersion, uint32_t versionInQuestion);
uint32_t minVulkanAPIVersion(uint32_t lhs, uint32_t rhs);

inline std::ostream &operator<<(std::ostream &s, const ApiVersion &version)
{
    return s << version.majorNum << "." << version.minorNum << "." << version.patchNum;
}

} // namespace vk

#endif // _VKAPIVERSION_HPP
