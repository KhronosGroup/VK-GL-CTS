#ifndef _VKSCOMMON_HPP
#define _VKSCOMMON_HPP

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

#include <deDefs.h>
#include <string>
#include <vector>
#include <functional>
#include <cassert>
#include <stdexcept>
#include <future>
#include <algorithm>

namespace vksc_server
{

using msize = std::size_t;

using s32 = deInt32;

using u8  = deUint8;
using u16 = deUint16;
using u32 = deUint32;
using u64 = deUint64;

using std::string;
using std::vector;

template <typename R>
bool is_ready (const std::future<R>& f)
{
	return f.wait_for(std::chrono::seconds(1)) == std::future_status::ready;
}

template <typename T, typename PRED>
vector<T>& remove_erase_if (vector<T>& on, PRED pred)
{
	on.erase( std::remove_if(on.begin(), on.end(), pred),  on.end() );
	return on;
}

}; // vksc_server

#endif // _VKSCOMMON_HPP
