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
 * The Materials are Confidential Information as defined by the
 * Khronos Membership Agreement until designated non-confidential by Khronos,
 * at which point this condition clause shall be removed.
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
 * \brief Iterator over combinations of items without repetition
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
class CombinationsIterator
{
public:
							CombinationsIterator	(deUint32 numItems, deUint32 combinationSize);
	virtual					~CombinationsIterator	(void) {}
	bool					hasNext					(void) const;
	T						next					(void);
	void					reset					(void);

protected:
	virtual T				getCombinationValue		(const std::vector<deUint32>& combination) = 0;

private:
	static deUint32			factorial				(deUint32 x);
	deUint32				m_numItems;

	deUint32				m_combinationIndex;
	deUint32				m_combinationSize;
	deUint32				m_combinationCount;

	std::vector<deUint32>	m_combination;
};

static deUint32 seriesProduct (deUint32 first, deUint32 last)
{
	deUint32 result = 1;

	for (deUint32 i = first; i <= last; i++)
		result *= i;

	return result;
}

template <typename T>
CombinationsIterator<T>::CombinationsIterator (deUint32 numItems, deUint32 combinationSize)
	: m_numItems		(numItems)
	, m_combinationSize	(combinationSize)
{
	DE_ASSERT(m_combinationSize > 0);
	DE_ASSERT(m_combinationSize <= m_numItems);

	m_combinationCount	= seriesProduct(numItems - combinationSize + 1, numItems) / seriesProduct(1, combinationSize);

	m_combination.resize(m_combinationSize);
	reset();
}

template <typename T>
bool CombinationsIterator<T>::hasNext (void) const
{
	return m_combinationIndex < m_combinationCount;
}

template <typename T>
T CombinationsIterator<T>::next (void)
{
	DE_ASSERT(m_combinationIndex < m_combinationCount);

	if (m_combinationIndex > 0)
	{
		for (int combinationItemNdx = (int)m_combinationSize - 1; combinationItemNdx >= 0; combinationItemNdx--)
		{
			if ((m_combination[combinationItemNdx] + 1 < m_numItems) && ((combinationItemNdx == m_combinationSize - 1) || (m_combination[combinationItemNdx + 1] > m_combination[combinationItemNdx] + 1)))
			{
				m_combination[combinationItemNdx]++;

				for (deUint32 resetNdx = combinationItemNdx + 1; resetNdx < m_combinationSize; resetNdx++)
					m_combination[resetNdx] = m_combination[resetNdx - 1] + 1;

				break;
			}
		}
	}

	m_combinationIndex++;

	return getCombinationValue(m_combination);
}

template <typename T>
void CombinationsIterator<T>::reset (void)
{
	// Set up first combination
	for (deUint32 itemNdx = 0; itemNdx < m_combinationSize; itemNdx++)
		m_combination[itemNdx] = itemNdx;

	m_combinationIndex = 0;
}

template <typename T>
deUint32 CombinationsIterator<T>::factorial (deUint32 x)
{
	deUint32 result = 1;

	for (deUint32 value = x; value > 1; value--)
		result *= value;

	return result;
}

} // pipeline
} // vkt

#endif // _VKTPIPELINEUNIQUERANDOMITERATOR_HPP
