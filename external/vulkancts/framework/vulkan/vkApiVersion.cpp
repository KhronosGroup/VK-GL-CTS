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

ApiVersion unpackVersion (deUint32 version)
{
	return ApiVersion(VK_API_VERSION_VARIANT(version),
					  VK_API_VERSION_MAJOR(version),
					  VK_API_VERSION_MINOR(version),
					  VK_API_VERSION_PATCH(version));
}

deUint32 pack (const ApiVersion& version)
{
	DE_ASSERT((version.variantNum & ~0x7)   == 0);
	DE_ASSERT((version.majorNum   & ~0x7F)  == 0);
	DE_ASSERT((version.minorNum   & ~0x3FF) == 0);
	DE_ASSERT((version.patchNum   & ~0xFFF) == 0);

	return (version.variantNum << 29) | (version.majorNum << 22) | (version.minorNum << 12) | version.patchNum;
}

deUint32 apiVersionClearPatch(deUint32 version)
{
	return version & ~0xFFF;
}

// Direct acyclic graph of Vulkan API versions and its predecessors.
// At the moment it's linear ( 0.1.0.0 < 0.1.1.0 < 0.1.2.0 < 1.1.0.0 ).
// But with the introduction of Vulkan 1.3 it won't be, because Vulkan 1.2 will have 2 successors orthogonal to each other.
// Moreover - when in the future new Vulkan SC 1.1 version will be created - it's possible that
// it will have 2 predecessors : Vulkan SC 1.0 and Vulkan 1.3 ( or later version - it's just example )
// When it happens : two new predecessors will look like this:
//	{ VK_MAKE_API_VERSION(1, 1, 1, 0), VK_MAKE_API_VERSION(1, 1, 0, 0) },
//	{ VK_MAKE_API_VERSION(1, 1, 1, 0), VK_MAKE_API_VERSION(0, 1, 3, 0) },

const static std::vector<std::pair<deUint32,deUint32>> apiVersionPredecessors =
{
	{ VK_MAKE_API_VERSION(0, 1, 0, 0), 0 },
	{ VK_MAKE_API_VERSION(0, 1, 1, 0), VK_MAKE_API_VERSION(0, 1, 0, 0) },
	{ VK_MAKE_API_VERSION(0, 1, 2, 0), VK_MAKE_API_VERSION(0, 1, 1, 0) },
	{ VK_MAKE_API_VERSION(1, 1, 0, 0), VK_MAKE_API_VERSION(0, 1, 2, 0) },
	{ VK_MAKE_API_VERSION(0, 1, 3, 0), VK_MAKE_API_VERSION(0, 1, 2, 0) },
};

bool isApiVersionEqual(deUint32 lhs, deUint32 rhs)
{
	deUint32 lhsp = apiVersionClearPatch(lhs);
	deUint32 rhsp = apiVersionClearPatch(rhs);
	return lhsp == rhsp;
}

bool isApiVersionPredecessor(deUint32 version, deUint32 predVersion)
{
	std::vector<deUint32> versions;
	versions.push_back(apiVersionClearPatch(version));

	deUint32 p = apiVersionClearPatch(predVersion);

	while (!versions.empty())
	{
		deUint32 v = versions.back();
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

bool isApiVersionSupported(deUint32 yourVersion, deUint32 versionInQuestion)
{
	if (isApiVersionEqual(yourVersion, versionInQuestion))
		return true;
	return isApiVersionPredecessor(yourVersion, versionInQuestion);
}

deUint32 minVulkanAPIVersion(deUint32 lhs, deUint32 rhs)
{
	deUint32 lhsp = apiVersionClearPatch(lhs);
	deUint32 rhsp = apiVersionClearPatch(rhs);
	if (lhsp == rhsp)
		return de::min(lhs, rhs);
	if (isApiVersionPredecessor(rhs, lhs))
		return lhs;
	if (isApiVersionPredecessor(lhs, rhs))
		return rhs;
	// both versions are located in different DAG paths - we will return common predecessor
	static std::vector<deUint32> commonPredecessors;
	if (commonPredecessors.empty())
	{
		std::set<deUint32> pred;
		for (auto it = begin(apiVersionPredecessors); it != end(apiVersionPredecessors); ++it)
		{
			if (pred.find(it->second) != end(pred))
				commonPredecessors.push_back(it->second);
			pred.insert(it->second);
		}
		std::sort(begin(commonPredecessors), end(commonPredecessors), [](deUint32 xlhs, deUint32 xrhs) { return isApiVersionPredecessor(xrhs, xlhs); });
	}
	for (auto it = begin(commonPredecessors); it != end(commonPredecessors); ++it)
		if (isApiVersionPredecessor(rhs, *it) && isApiVersionPredecessor(lhs, *it))
			return *it;
	return 0;
}

} // vk
