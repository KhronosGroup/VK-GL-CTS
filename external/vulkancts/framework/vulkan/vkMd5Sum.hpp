#ifndef _VKMD5SUM_HPP
#define _VKMD5SUM_HPP
/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2023 The Khronos Group Inc.
 * Copyright (c) 2023 The SQLite Project.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *	  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *//*!
 * \file
 * \brief Utilities for calculating MD5 checksums.
 *
 * This file was modified from Chromium,
 * https://chromium.googlesource.com/chromium/src/base/+/7ef85b701132474f71e6369f081a2fb84582ee88/md5.h
 *
 * This code implements the MD5 message-digest algorithm.
 * The algorithm is due to Ron Rivest.  This code was
 * written by Colin Plumb in 1993, no copyright is claimed.
 * This code is in the public domain; do with it what you wish.
 *
 * Equivalent code is available from RSA Data Security, Inc.
 * This code has been tested against that, and is equivalent,
 * except that you don't need to include two pages of legalese
 * with every copy.
 *
 * To compute the message digest of a chunk of bytes, declare an
 * MD5Context structure, pass it to MD5Init, call MD5Update as
 * needed on buffers full of bytes, and then call MD5Final, which
 * will fill a supplied 16-byte array with the digest.
 *--------------------------------------------------------------------*/

#include <cstdint>
#include <deDefs.h>
#include <string>

namespace vk
{

// The output of an MD5 operation.
struct MD5Digest
{
	unsigned char a[16];
};

// Used for storing intermediate data during an MD5 computation. Callers
// should not access the data.
typedef char MD5Context[88];

void MD5Sum(const void* data, std::size_t length, MD5Digest* digest);

// Converts a digest into human-readable hexadecimal.
std::string MD5DigestToBase16(const MD5Digest& digest);

// Helper for doing the common case of MD5Sum followed by MD5DigestToBase16.
std::string MD5SumBase16(const void* data, std::size_t length);

} // namespace vk

#endif // _VKMD5SUM_HPP
