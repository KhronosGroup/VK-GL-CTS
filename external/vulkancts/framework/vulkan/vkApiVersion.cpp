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

#include "vkApiVersion.hpp"
#include <vector>
#include <set>
#include <algorithm>

namespace vk
{

ApiVersion unpackVersion(uint32_t version)
{
    return ApiVersion(VK_API_VERSION_VARIANT(version), VK_API_VERSION_MAJOR(version), VK_API_VERSION_MINOR(version),
                      VK_API_VERSION_PATCH(version));
}

uint32_t pack(const ApiVersion &version)
{
    DE_ASSERT((version.variantNum & ~0x7) == 0);
    DE_ASSERT((version.majorNum & ~0x7F) == 0);
    DE_ASSERT((version.minorNum & ~0x3FF) == 0);
    DE_ASSERT((version.patchNum & ~0xFFF) == 0);

    return (version.variantNum << 29) | (version.majorNum << 22) | (version.minorNum << 12) | version.patchNum;
}

uint32_t apiVersionClearPatch(uint32_t version)
{
    return version & ~0xFFF;
}

// Direct acyclic graph of Vulkan API versions and its predecessors.
// At the moment it's linear ( 0.1.0.0 < 0.1.1.0 < 0.1.2.0 < 1.1.0.0 ).
// But with the introduction of Vulkan 1.3 it won't be, because Vulkan 1.2 will have 2 successors orthogonal to each other.
// Moreover - when in the future new Vulkan SC 1.1 version will be created - it's possible that
// it will have 2 predecessors : Vulkan SC 1.0 and Vulkan 1.3 ( or later version - it's just example )
// When it happens : two new predecessors will look like this:
//    { VK_MAKE_API_VERSION(1, 1, 1, 0), VK_MAKE_API_VERSION(1, 1, 0, 0) },
//    { VK_MAKE_API_VERSION(1, 1, 1, 0), VK_MAKE_API_VERSION(0, 1, 3, 0) },

const static std::vector<std::pair<uint32_t, uint32_t>> apiVersionPredecessors = {
    {VK_MAKE_API_VERSION(0, 1, 0, 0), 0},
    {VK_MAKE_API_VERSION(0, 1, 1, 0), VK_MAKE_API_VERSION(0, 1, 0, 0)},
    {VK_MAKE_API_VERSION(0, 1, 2, 0), VK_MAKE_API_VERSION(0, 1, 1, 0)},
    {VK_MAKE_API_VERSION(1, 1, 0, 0), VK_MAKE_API_VERSION(0, 1, 2, 0)},
    {VK_MAKE_API_VERSION(0, 1, 3, 0), VK_MAKE_API_VERSION(0, 1, 2, 0)},
};

bool isApiVersionEqual(uint32_t lhs, uint32_t rhs)
{
    uint32_t lhsp = apiVersionClearPatch(lhs);
    uint32_t rhsp = apiVersionClearPatch(rhs);
    return lhsp == rhsp;
}

bool isApiVersionPredecessor(uint32_t version, uint32_t predVersion)
{
    std::vector<uint32_t> versions;
    versions.push_back(apiVersionClearPatch(version));

    uint32_t p = apiVersionClearPatch(predVersion);

    while (!versions.empty())
    {
        uint32_t v = versions.back();
        versions.pop_back();

        for (auto it = begin(apiVersionPredecessors); it != end(apiVersionPredecessors); ++it)
        {
            if (it->first != v)
                continue;
            if (it->second == p)
                return true;
            versions.push_back(it->second);
        }
    }
    return false;
}

bool isApiVersionSupported(uint32_t yourVersion, uint32_t versionInQuestion)
{
    if (isApiVersionEqual(yourVersion, versionInQuestion))
        return true;
    return isApiVersionPredecessor(yourVersion, versionInQuestion);
}

uint32_t minVulkanAPIVersion(uint32_t lhs, uint32_t rhs)
{
    uint32_t lhsp = apiVersionClearPatch(lhs);
    uint32_t rhsp = apiVersionClearPatch(rhs);
    if (lhsp == rhsp)
        return de::min(lhs, rhs);
    if (isApiVersionPredecessor(rhs, lhs))
        return lhs;
    if (isApiVersionPredecessor(lhs, rhs))
        return rhs;
    // both versions are located in different DAG paths - we will return common predecessor
    static std::vector<uint32_t> commonPredecessors;
    if (commonPredecessors.empty())
    {
        std::set<uint32_t> pred;
        for (auto it = begin(apiVersionPredecessors); it != end(apiVersionPredecessors); ++it)
        {
            if (pred.find(it->second) != end(pred))
                commonPredecessors.push_back(it->second);
            pred.insert(it->second);
        }
        std::sort(begin(commonPredecessors), end(commonPredecessors),
                  [](uint32_t xlhs, uint32_t xrhs) { return isApiVersionPredecessor(xrhs, xlhs); });
    }
    for (auto it = begin(commonPredecessors); it != end(commonPredecessors); ++it)
        if (isApiVersionPredecessor(rhs, *it) && isApiVersionPredecessor(lhs, *it))
            return *it;
    return 0;
}

} // namespace vk
