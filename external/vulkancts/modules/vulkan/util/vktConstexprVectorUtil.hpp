#ifndef _VKTCONSTEXPRVECTORUTIL_HPP
#define _VKTCONSTEXPRVECTORUTIL_HPP
/*------------------------------------------------------------------------
 * Vulkan CTS Framework
 * ------------------------
 *
 * Copyright (c) 2020 The Khronos Group Inc.
 * Copyright (c) 2020 Advanced Micro Devices, Inc.
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
 */
/*!
 * \file
 * \brief Compile time friendly dynamic sized array with maximum capacity
 */
/*--------------------------------------------------------------------*/

#include <cstddef>
#include <array>

namespace vkt
{
/*--------------------------------------------------------------------*//*!
 * \brief Constexpr compatable vector with checked maximum capacity
 *
 * \note Unlike std::array, size() and max_size() are different values
 *       This makes behaviour more similar to that of std::vector.
 *//*--------------------------------------------------------------------*/
template<typename _t, size_t CAPACITY>
class ConstexprVector
{
public:
	using value_type = _t;
	using size_type = ::std::size_t;
	using difference_type = ::std::ptrdiff_t;
	using const_reference = const value_type&;
	using const_pointer = const value_type*;
	using const_iterator = const value_type*;

	inline constexpr ConstexprVector() noexcept : values{}, count{0} {};

	/*--------------------------------------------------------------------*//*!
	 * MSVC v140 chokes on this if it is a raw variadic template list.
	 * By providing a single argument lead for type deduction it seems to fix
	 * things. Marking constructor as explicit since this effectively becomes
	 * a single argument constructor.
	 *//*--------------------------------------------------------------------*/
	template<typename _arg_t, typename... _args_t>
	inline constexpr explicit ConstexprVector(const _arg_t& arg1, const _args_t&... args) noexcept :
		values{arg1, args...},
		count{sizeof...(_args_t) + 1}
	{
		static_assert((sizeof...(_args_t) + 1) <= CAPACITY, "Not enough capacity to store values");
	}

	inline constexpr const_reference at(size_type pos) const noexcept { return values[pos]; }
	inline constexpr const_reference operator[](size_type pos) const noexcept { return values[pos]; }
	inline constexpr const_reference front() const noexcept { return values[0]; }
	inline constexpr const_reference back() const noexcept { return values[count - 1];	}
	inline constexpr const_pointer data() const noexcept { return values; }
	inline constexpr const_iterator begin() const noexcept { return &values[0]; }
	inline constexpr const_iterator cbegin() const noexcept { return &values[0]; }
	inline constexpr const_iterator end() const noexcept { return &values[count]; }
	inline constexpr const_iterator cend() const noexcept { return &values[count]; }
	inline constexpr bool empty() const noexcept { return count == 0; }
	inline constexpr size_type size() const noexcept { return count; }
	inline constexpr size_type max_size() const noexcept { return CAPACITY; }

private:
	value_type values[CAPACITY];
	size_type count;
};

} // namespace vkt

#endif // _VKTCONSTEXPRVECTORUTIL_HPP
