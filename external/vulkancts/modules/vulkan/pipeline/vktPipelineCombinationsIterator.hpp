#ifndef _VKTPIPELINECOMBINATIONSITERATOR_HPP
#define _VKTPIPELINECOMBINATIONSITERATOR_HPP
/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2015 The Khronos Group Inc.
 * Copyright (c) 2015 Imagination Technologies Ltd.
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
    CombinationsIterator(uint32_t numItems, uint32_t combinationSize);
    virtual ~CombinationsIterator(void)
    {
    }
    bool hasNext(void) const;
    T next(void);
    void reset(void);

protected:
    virtual T getCombinationValue(const std::vector<uint32_t> &combination) = 0;

private:
    static uint32_t factorial(uint32_t x);
    uint32_t m_numItems;

    uint32_t m_combinationIndex;
    uint32_t m_combinationSize;
    uint32_t m_combinationCount;

    std::vector<uint32_t> m_combination;
};

static uint32_t seriesProduct(uint32_t first, uint32_t last)
{
    uint32_t result = 1;

    for (uint32_t i = first; i <= last; i++)
        result *= i;

    return result;
}

template <typename T>
CombinationsIterator<T>::CombinationsIterator(uint32_t numItems, uint32_t combinationSize)
    : m_numItems(numItems)
    , m_combinationSize(combinationSize)
{
    DE_ASSERT(m_combinationSize > 0);
    DE_ASSERT(m_combinationSize <= m_numItems);

    m_combinationCount = seriesProduct(numItems - combinationSize + 1, numItems) / seriesProduct(1, combinationSize);

    m_combination.resize(m_combinationSize);
    reset();
}

template <typename T>
bool CombinationsIterator<T>::hasNext(void) const
{
    return m_combinationIndex < m_combinationCount;
}

template <typename T>
T CombinationsIterator<T>::next(void)
{
    DE_ASSERT(m_combinationIndex < m_combinationCount);

    if (m_combinationIndex > 0)
    {
        for (int combinationItemNdx = (int)m_combinationSize - 1; combinationItemNdx >= 0; combinationItemNdx--)
        {
            if ((m_combination[combinationItemNdx] + 1 < m_numItems) &&
                ((combinationItemNdx == (int)m_combinationSize - 1) ||
                 (m_combination[combinationItemNdx + 1] > m_combination[combinationItemNdx] + 1)))
            {
                m_combination[combinationItemNdx]++;

                for (uint32_t resetNdx = combinationItemNdx + 1; resetNdx < m_combinationSize; resetNdx++)
                    m_combination[resetNdx] = m_combination[resetNdx - 1] + 1;

                break;
            }
        }
    }

    m_combinationIndex++;

    return getCombinationValue(m_combination);
}

template <typename T>
void CombinationsIterator<T>::reset(void)
{
    // Set up first combination
    for (uint32_t itemNdx = 0; itemNdx < m_combinationSize; itemNdx++)
        m_combination[itemNdx] = itemNdx;

    m_combinationIndex = 0;
}

template <typename T>
uint32_t CombinationsIterator<T>::factorial(uint32_t x)
{
    uint32_t result = 1;

    for (uint32_t value = x; value > 1; value--)
        result *= value;

    return result;
}

} // namespace pipeline
} // namespace vkt

#endif // _VKTPIPELINECOMBINATIONSITERATOR_HPP
