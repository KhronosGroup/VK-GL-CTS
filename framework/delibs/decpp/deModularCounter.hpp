#ifndef _DEMODULARCOUNTER_HPP
#define _DEMODULARCOUNTER_HPP
/*-------------------------------------------------------------------------
 * drawElements C++ Base Library
 * -----------------------------
 *
 * Copyright 2023 Valve Corporation.
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
 * \brief Modular counter helper class.
 *//*--------------------------------------------------------------------*/

#include <type_traits>
#include <cstdint>

namespace de
{

template <typename T>
class ModularCounter
{
	static_assert(std::is_unsigned<T>::value, "Invalid underlying type");

public:
	typedef T value_type;

	explicit ModularCounter (T period, T initialValue = T{0})
		: m_period(period), m_value(initialValue)
	{
	}

	ModularCounter& operator++	()			{ m_value = ((m_value + T{1}) % m_period); return *this; }
	ModularCounter& operator--	()			{ m_value = ((m_value - T{1}) % m_period); return *this; }
	ModularCounter  operator++	(int)		{ ModularCounter ret(*this); ++(*this); return ret; }
	ModularCounter  operator--	(int)		{ ModularCounter ret(*this); --(*this); return ret; }
	operator		T			(void)		{ return m_value; }

protected:
	const T	m_period;
	T		m_value;
};

using ModCounter64 = ModularCounter<uint64_t>;
using ModCounter32 = ModularCounter<uint32_t>;

}

#endif // _DEMODULARCOUNTER_HPP
