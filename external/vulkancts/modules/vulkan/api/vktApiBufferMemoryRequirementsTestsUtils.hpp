#ifndef _VKTAPIBUFFERMEMORYREQUIREMENTSTESTSUTILS_HPP
#define _VKTAPIBUFFERMEMORYREQUIREMENTSTESTSUTILS_HPP
/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2021 The Khronos Group Inc.
 * Copyright (c) 2016 The Android Open Source Project
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
 * \brief Utilities for vktApiMemoryRequirementsTests.
 *//*--------------------------------------------------------------------*/

#include "deDefs.h"
#include "deSharedPtr.hpp"

#include <initializer_list>
#include <set>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace vkt
{
namespace api
{
namespace u
{

template<class> struct tc;
template<class Key, class... Ignored>
struct tc<std::tuple<Key, Ignored...>> {
	typedef std::tuple<Key, Ignored...> T;
	bool operator()(const T& l, const T& r) const
		{ return std::get<0>(l) < std::get<0>(r); }
};

template<class Flag, class Bit, class... Ignored>
struct BitsSet : public std::set<std::tuple<Bit, Ignored...>, tc<std::tuple<Bit, Ignored...>>>
{
	typedef Bit	bit_type;
	typedef Flag flag_type;
	typedef std::tuple<Bit, Ignored...> value_type;
	typedef std::set<value_type, tc<value_type>> base;
	typedef typename base::const_iterator const_iterator;
	BitsSet(std::initializer_list<value_type> list) : base(list) {}
	BitsSet(BitsSet&& other) : base(std::forward<BitsSet>(other)) {}
	BitsSet(const BitsSet& other) : base(other) {}
	BitsSet() = default;
	BitsSet& operator=(const BitsSet& other) {
		base::operator=(other);
		return *this;
	}
	BitsSet& operator=(BitsSet&& other) {
		base::operator=(std::forward<BitsSet>(other));
		return *this;
	}
	operator Flag() const {
		Flag flag = static_cast<Flag>(0);
		for (const auto& bit : *this)
			flag |= std::get<0>(bit);
		return flag;
	}
	Flag operator()() const {
		return static_cast<Flag>(*this);
	}
	bool contains(const Bit& bit) const {
		for (const auto& myBit : *this)
			if (bit == std::get<0>(myBit)) return true;
		return false;
	}
	bool any(std::initializer_list<Bit> bits) const {
		for (auto i = bits.begin(); i != bits.end(); ++i)
			if (contains(*i)) return true;
		return false;
	}
	bool all(std::initializer_list<Bit> bits) const {
		for (auto i = bits.begin(); i != bits.end(); ++i)
			if (!contains(*i)) return false;
		return true;
	}
	bool contains(const value_type& bit) const {
		return contains(std::get<0>(bit));
	}
	const_iterator find(const Bit& bit) const {
		auto end = std::end(*this);
		for (auto i = std::begin(*this); i != end; ++i)
			if (bit == std::get<0>(*i))
				return i;
		return end;
	}
	const_iterator find(const value_type& bit) const {
		return find(std::get<0>(bit));
	}
	const value_type& get(const Bit& bit) const {
		auto search = find(bit);
		DE_ASSERT(search != std::end(*this));
		return *search;
	}
	static Bit extract(const value_type& bit) {
		return std::get<0>(bit);
	}
	template<size_t Index, class TypeAt>
	BitsSet select(const TypeAt& typeAtIndex) const {
		static_assert(std::is_same<TypeAt, typename std::tuple_element<Index, value_type>::type>::value, "");
		BitsSet result;
		for (const auto& bit : *this) {
			if (typeAtIndex == std::get<Index>(bit))
					result.insert(bit);
		}
		return result;
	}
	de::SharedPtr<BitsSet> makeShared() const {
		return de::SharedPtr<BitsSet>(new BitsSet(*this));
	}
	static de::SharedPtr<BitsSet> makeShared(const value_type& bit) {
		return de::SharedPtr<BitsSet>(new BitsSet({bit}));
	}
	static de::SharedPtr<BitsSet> makeShared(BitsSet&& src) {
		return de::SharedPtr<BitsSet>(new BitsSet(std::move(src)));
	}
};

template<class Flag, class Bits, class... Ignored>
std::vector<Flag> mergeFlags
(
	const std::vector<Flag>&							flags1,
	const std::vector<BitsSet<Flag, Bits, Ignored...>>&	flags2
)
{
	std::vector<Flag>	result;
	if (!flags1.empty() && !flags2.empty()) {
		for (const auto& flag1 : flags1) {
			for (const auto& flag2 : flags2)
				result.emplace_back(flag1 | flag2);
		}
	}
	else if (flags2.empty()) {
		result = flags1;
	}
	else if (flags1.empty()) {
		for (const auto& flag2 : flags2)
			result.emplace_back(flag2);
	}
	return result;
}

template<class Flag, class Bits, class... Ignored>
void mergeFlags
(
	std::vector<BitsSet<Flag, Bits, Ignored...>>&		inout,
	const std::vector<BitsSet<Flag, Bits, Ignored...>>&	flags
)
{
	if (inout.empty())
		inout.insert(inout.end(), flags.begin(), flags.end());
	else {
		for (auto& bits1: inout) {
			for (const auto& bits2 : flags)
				bits1.insert(bits2.begin(), bits2.end());
		}
	}
}

template<class Flag, class Bit, class... Ignored>
void combine
(
	std::vector<BitsSet<Flag, Bit, Ignored...>>&	result,
	const BitsSet<Flag, Bit, Ignored...>&			bits,
	std::vector<Flag>&								hints
)
{
	const Flag flag = bits();
	if (bits.empty() || hints.end() != std::find(hints.begin(), hints.end(), flag)) return;
	hints.emplace_back(flag);
	result.emplace_back(bits);
	for (deUint32 b = 0; b < bits.size(); ++b) {
		BitsSet<Flag, Bit, Ignored...>	tmp(bits);
		tmp.erase(std::next(tmp.begin(), b));
		combine(result, tmp, hints);
	}
}

} // u
} // api
} // vkt

#endif // _VKTAPIBUFFERMEMORYREQUIREMENTSTESTSUTILS_HPP
