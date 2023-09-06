#ifndef _VKSSTORE_HPP
#define _VKSSTORE_HPP

/*-------------------------------------------------------------------------
 * Vulkan CTS Framework
 * --------------------
 *
 * Copyright (c) 2021 The Khronos Group Inc.
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
 *-------------------------------------------------------------------------*/

#include "vksCommon.hpp"

#include <mutex>
#include <map>

namespace vksc_server
{

struct Store
{
	bool Get (const string& path, vector<u8>& content, bool removeAfter)
	{
		std::lock_guard<std::mutex> lock(FileMapMutex);

		auto it = FileMap.find(path);
		if (it != FileMap.end())
		{
			if (removeAfter)
			{
				content = std::move(it->second);
				FileMap.erase(it);
			}
			else
			{
				content = it->second;
			}
			return true;
		}
		return false;
	}

	bool Set (const string& uniqueFilename, const vector<u8>& content)
	{
		std::lock_guard<std::mutex> lock(FileMapMutex);
		FileMap[uniqueFilename] = std::move(content);
		return true;
	}

private:
	std::map<string, vector<u8>> FileMap;
	std::mutex FileMapMutex;
};

}

#endif // _VKSSTORE_HPP
