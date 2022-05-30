#ifndef _VKSENDIAN_HPP
#define _VKSENDIAN_HPP

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

#include <deInt32.h>

namespace vksc_server
{

	constexpr bool IsBigEndian ()
	{
		return (DE_ENDIANNESS) == (DE_BIG_ENDIAN);
	}

	constexpr u64 ReverseBytes64(u64 n)
	{
		return (
			((n & u64{0xFF00000000000000}) >> 56) |
			((n & u64{0x00FF000000000000}) >> 40) |
			((n & u64{0x0000FF0000000000}) >> 24) |
			((n & u64{0x000000FF00000000}) >> 8)  |
			((n & u64{0x00000000FF000000}) << 8)  |
			((n & u64{0x0000000000FF0000}) << 24) |
			((n & u64{0x000000000000FF00}) << 40) |
			((n & u64{0x00000000000000FF}) << 56)
		);
	}

#if DE_ENDIANNESS == DE_LITTLE_ENDIAN
	inline u16 HostToNetwork16 (u16 host) { return deReverseBytes16(host); }
	inline u32 HostToNetwork32 (u32 host) { return deReverseBytes32(host); }
	inline u64 HostToNetwork64 (u64 host) { return   ReverseBytes64(host); }
#elif DE_ENDIANNESS == DE_BIG_ENDIAN
	inline u16 HostToNetwork16 (u16 host) { return host; }
	inline u32 HostToNetwork32 (u32 host) { return host; }
	inline u64 HostToNetwork64 (u64 host) { return host; }
#endif

	inline u16 NetworkToHost16 (u16 net) { return HostToNetwork16(net); }
	inline u32 NetworkToHost32 (u32 net) { return HostToNetwork32(net); }
	inline u64 NetworkToHost64 (u64 net) { return HostToNetwork64(net); }

}

#endif // _VKSENDIAN_HPP
