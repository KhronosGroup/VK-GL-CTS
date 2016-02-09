#ifndef _VKTPIPELINEUNIQUERANDOMITERATOR_HPP
#define _VKTPIPELINEUNIQUERANDOMITERATOR_HPP
/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2015 The Khronos Group Inc.
 * Copyright (c) 2015 Imagination Technologies Ltd.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and/or associated documentation files (the
 * "Materials"), to deal in the Materials without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Materials, and to
 * permit persons to whom the Materials are furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice(s) and this permission notice shall be included
 * in all copies or substantial portions of the Materials.
 *
 * THE MATERIALS ARE PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * MATERIALS OR THE USE OR OTHER DEALINGS IN THE MATERIALS.
 *
 *//*!
 * \file
 * \brief Iterator over a unique sequence of items
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"
#include "deRandom.hpp"
#include <set>
#include <vector>

namespace vkt
{
namespace pipeline
{

template <typename T>
class UniqueRandomIterator
{
public:
							UniqueRandomIterator	(deUint32 numItems, deUint32 numValues, int seed);
	virtual					~UniqueRandomIterator	(void) {}
	bool					hasNext					(void) const;
	T						next					(void);
	void					reset					(void);

protected:
	virtual T				getIndexedValue			(deUint32 index) = 0;

private:
	std::vector<deUint32>	m_indices;
	size_t					m_currentIndex;
};

template <typename T>
UniqueRandomIterator<T>::UniqueRandomIterator (deUint32 numItems, deUint32 numValues, int seed)
{
	de::Random rnd(seed);

	DE_ASSERT(numItems <= numValues);

	if (numItems == numValues)
	{
		// Fast way to populate the index sequence
		m_indices = std::vector<deUint32>(numItems);

		for (deUint32 itemNdx = 0; itemNdx < numItems; itemNdx++)
			m_indices[itemNdx] = itemNdx;
	}
	else
	{
		std::set<deUint32> uniqueIndices;

		// Populate set with "numItems" unique values between 0 and numValues - 1
		while (uniqueIndices.size() < numItems)
			uniqueIndices.insert(rnd.getUint32() % numValues);

		// Copy set into index sequence
		m_indices = std::vector<deUint32>(uniqueIndices.begin(), uniqueIndices.end());
	}

	// Scramble the indices
	rnd.shuffle(m_indices.begin(), m_indices.end());

	reset();
}

template <typename T>
bool UniqueRandomIterator<T>::hasNext (void) const
{
	return m_currentIndex < m_indices.size();
}

template <typename T>
T UniqueRandomIterator<T>::next (void)
{
	DE_ASSERT(m_currentIndex < m_indices.size());

	return getIndexedValue(m_indices[m_currentIndex++]);
}

template <typename T>
void UniqueRandomIterator<T>::reset (void)
{
	m_currentIndex = 0;
}

} // pipeline
} // vkt

#endif // _VKTPIPELINEUNIQUERANDOMITERATOR_HPP
