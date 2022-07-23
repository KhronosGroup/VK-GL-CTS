#ifndef _VKSPROTOCOL_HPP
#define _VKSPROTOCOL_HPP

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

#include "vksSerializerVKSC.hpp"
#include "vksStructsVKSC.hpp"

namespace vksc_server
{

struct CompileShaderRequest
{
	SourceVariant source;
	string commandLine{};

	static constexpr u32 Type () { return 0; }

	template <typename TYPE>
	void Serialize (Serializer<TYPE>& archive) { archive.SerializeObject(source); archive.Serialize(commandLine); }
};

struct CompileShaderResponse
{
	bool status{};
	vector<u8> binary;

	static constexpr u32 Type() { return 1; }

	template <typename TYPE>
	void Serialize (Serializer<TYPE>& archive) { archive.Serialize(status, binary); }
};

struct StoreContentRequest
{
	string name;
	vector<u8> data;

	static constexpr u32 Type() { return 2; }

	template <typename TYPE>
	void Serialize (Serializer<TYPE>& archive) { archive.Serialize(name, data); }
};

struct StoreContentResponse
{
	bool status{};

	static constexpr u32 Type() { return 3; }

	template <typename TYPE>
	void Serialize (Serializer<TYPE>& archive) { archive.Serialize(status); }
};

struct AppendRequest
{
	string fileName;
	vector<u8> data;
	bool clear{};

	static constexpr u32 Type() { return 4; }

	template <typename TYPE>
	void Serialize (Serializer<TYPE>& archive) { archive.Serialize(fileName, data, clear); }
};

struct GetContentRequest
{
	string path;
	bool physicalFile{};
	bool removeAfter{};

	static constexpr u32 Type() { return 5; }

	template <typename TYPE>
	void Serialize (Serializer<TYPE>& archive) { archive.Serialize(path, physicalFile, removeAfter); }
};

struct GetContentResponse
{
	bool status{};
	vector<u8> data;

	static constexpr u32 Type() { return 6; }

	template <typename TYPE>
	void Serialize (Serializer<TYPE>& archive) { archive.Serialize(status, data); }
};

struct CreateCacheRequest
{
	VulkanPipelineCacheInput	input;
	s32							caseFraction;
	static constexpr u32 Type() { return 7; }

	template <typename TYPE>
	void Serialize (Serializer<TYPE>& archive)
	{
		archive.SerializeObject(input);
		archive.Serialize(caseFraction);
	}
};

struct CreateCacheResponse
{
	bool status{};
	vector<u8>							binary;
	static constexpr u32 Type() { return 8; }

	template <typename TYPE>
	void Serialize (Serializer<TYPE>& archive) { archive.Serialize(status, binary); }
};

struct LogRequest
{
	s32 type;
	string message;

	static constexpr u32 Type() { return 9; }

	template <typename TYPE>
	void Serialize (Serializer<TYPE>& archive) { archive.Serialize(type, message); }
};

}

#endif // _VKSPROTOCOL_HPP
