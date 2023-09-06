#ifndef _VKSSERIALIZER_HPP
#define _VKSSERIALIZER_HPP

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
#include "vksEndian.hpp"

#include <cstring>
#include <map>
#include <set>

namespace vksc_server
{

struct ToWrite
{
	typedef const void* VoidPointer;
	static void SerializeData (vector<u8>& buffer, msize& pos, VoidPointer data, msize size)
	{
		if (size == 0) return;
		if ((pos + size) > buffer.size()) buffer.resize(pos + size);
		memmove(buffer.data() + pos, data, size);
		pos += size;
	}
};

struct ToRead
{
	typedef void* VoidPointer;
	static void SerializeData (vector<u8>& buffer, msize& pos, VoidPointer data, msize size)
	{
		if (size == 0) return;

		if ( (pos + size) > buffer.size() ) throw std::runtime_error("SerializeData::ToRead unexpected end");
		memmove(data, buffer.data() + pos, size);
		pos += size;
	}
};

template <typename TYPE>
class Serializer
{
	vector<u8>& Data;
	msize Pos{};

public:
			Serializer			(vector<u8>& data) : Data(data) {}
	void	SerializeRawData	(typename TYPE::VoidPointer data, msize size) { TYPE::SerializeData(Data, Pos, data, size); }
	template <typename T>
	void	SerializeObject		(T& obj) { obj.Serialize(*this); }

	void	Serialize			() { }
	template <typename ARG>
	void	Serialize			(ARG&& arg) { SerializeItem(*this, arg); }
	template <typename ARG, typename... ARGs>
	void	Serialize			(ARG&& first, ARGs&&... args)	{ SerializeItem(*this, first); Serialize(args...); }
};

template <typename TYPE>
inline void SerializeItem (TYPE& serializer, u8& val)
{
	serializer.SerializeRawData(&val, sizeof(u8));
}

inline void SerializeItem (Serializer<ToRead>& serializer, u32& val)
{
	u32 nval;
	serializer.SerializeRawData(&nval, sizeof(u32));
	val = NetworkToHost32(nval);
}

inline void SerializeItem (Serializer<ToWrite>& serializer, u32& val)
{
	u32 nval = HostToNetwork32(val);
	serializer.SerializeRawData(&nval, sizeof(u32));
}

inline void SerializeItem (Serializer<ToRead>& serializer, u64& val)
{
	u64 nval;
	serializer.SerializeRawData(&nval, sizeof(u64));
	val = NetworkToHost64(nval);
}

inline void SerializeItem (Serializer<ToWrite>& serializer, u64& val)
{
	u64 nval = HostToNetwork64(val);
	serializer.SerializeRawData(&nval, sizeof(u64));
}

inline void SerializeItem (Serializer<ToRead>& serializer, s32& val)
{
	u32 nval;
	serializer.SerializeRawData(&nval, sizeof(u32));
	val = static_cast<s32>(NetworkToHost32(nval));
}

inline void SerializeItem (Serializer<ToWrite>& serializer, s32& val)
{
	u32 nval = HostToNetwork32(static_cast<u32>(val));
	serializer.SerializeRawData(&nval, sizeof(u32));
}

inline void SerializeItem (Serializer<ToRead>& serializer, bool& v)
{
	u8 byte;
	serializer.Serialize(byte);
	if (byte == 0) v = false;
	else if (byte == 1) v = true;
	else throw std::runtime_error("SerializeItem(Serializer<ToRead>, bool) invalid bool value");
}

inline void SerializeItem (Serializer<ToWrite>& serializer, bool& v)
{
	u8 byte = v?1:0;
	serializer.Serialize(byte);
}

inline void SerializeSize (Serializer<ToWrite>& serializer, msize size)
{
	if (size > std::numeric_limits<u32>::max()) throw std::runtime_error("length of a container is too big");
	u32 size32 = (u32)size;
	serializer.Serialize(size32);
}

inline void SerializeSize (Serializer<ToRead>& serializer, msize& size)
{
	u32 size32;
	serializer.Serialize(size32);
	size = size32;
}

inline void SerializeItem (Serializer<ToRead>& serializer, string& str)
{
	msize size;
	SerializeSize(serializer, size);

	vector<char> v(size);
	serializer.SerializeRawData(v.data(), v.size());
	str.assign(v.begin(), v.end());
}

inline void SerializeItem (Serializer<ToWrite>& serializer, const string& str)
{
	SerializeSize(serializer, str.size());
	serializer.SerializeRawData(str.data(), str.size());
}

template <typename T>
inline void SerializeItem(Serializer<ToWrite>& serializer, std::vector<T>& v)
{
	msize ms = v.size();
	SerializeSize(serializer, ms);

	for (msize i{}; i < v.size(); ++i)
	{
		serializer.Serialize(v[i]);
	}
}

template <typename T>
inline void SerializeItem(Serializer<ToRead>& serializer, std::vector<T>& v)
{
	msize size;
	SerializeSize(serializer, size);
	v.clear();
	for (msize i{}; i < size; ++i)
	{
		T item;
		SerializeItem(serializer, item);
		v.push_back(std::move(item));
	}
}

template <>
inline void SerializeItem (Serializer<ToWrite>& serializer, std::vector<u8>& v)
{
	SerializeSize(serializer, v.size());
	serializer.SerializeRawData(v.data(), v.size());
}

template <>
inline void SerializeItem (Serializer<ToRead>& serializer, std::vector<u8>& v)
{
	msize size;
	SerializeSize(serializer, size);

	v.clear();
	v.resize(size);
	serializer.SerializeRawData(v.data(), v.size());
}

template <typename K, typename V>
inline void SerializeItem (Serializer<ToRead>& serializer, std::map<K, V>& v)
{
	msize size;
	SerializeSize(serializer, size);
	v.clear();
	for (msize i{}; i < size; ++i)
	{
		std::pair<K, V> p;
		serializer.Serialize(p.first, p.second);
		v.insert(std::move(p));
	}
}

template <typename K, typename V>
inline void SerializeItem (Serializer<ToWrite>& serializer, std::map<K, V>& v)
{
	SerializeSize(serializer, v.size());
	for (auto& p : v)
	{
		serializer.Serialize(p.first, p.second);
	}
}

template <typename T>
inline void SerializeItem (Serializer<ToRead>& serializer, std::set<T>& v)
{
	msize size;
	SerializeSize(serializer, size);
	v.clear();
	for (msize i{}; i < size; ++i)
	{
		T item;
		SerializeItem(serializer, item);
		v.insert(std::move(item));
	}
}

template <typename T>
inline void SerializeItem (Serializer<ToWrite>& serializer, std::set<T>& v)
{
	SerializeSize(serializer, v.size());
	for (auto& i : v) SerializeItem(serializer, i);
}

template <typename T>
inline vector<u8> Serialize(T& data)
{
	vector<u8> result;
	Serializer<ToWrite>{result}.SerializeObject(data);
	return result;
}

template <typename T>
inline T Deserialize(vector<u8>& buffer)
{
	T result;
	Serializer<ToRead>{buffer}.SerializeObject(result);
	return result;
}

}

#endif // _VKSSERIALIZER_HPP
