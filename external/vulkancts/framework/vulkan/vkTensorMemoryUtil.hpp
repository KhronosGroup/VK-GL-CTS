#ifndef _VKTENSORMEMORYUTIL_HPP
#define _VKTENSORMEMORYUTIL_HPP
/*-------------------------------------------------------------------------
 * Vulkan CTS Framework
 * --------------------
 *
 * Copyright (c) 2025 ARM Ltd.
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
 * \brief Tensor memory utility classes
 */
/*--------------------------------------------------------------------*/

#include "vkDefs.hpp"

#include <limits>
#include <numeric>
#include <iostream>
#include <stddef.h>
#include <stdint.h>
#include <vector>

namespace vk
{

using TensorDimensions = std::vector<int64_t>;
using TensorStrides    = std::vector<int64_t>;

const TensorStrides getTensorStrides(const TensorDimensions &dimensions, size_t formatSize, size_t multiplier = 1);

template <typename T>
class StridedMemoryUtils
{
public:
    StridedMemoryUtils() = default;

    StridedMemoryUtils(const TensorDimensions &dimensions, const TensorStrides &strides, void *memoryPtr)
        : m_dimensions{dimensions}
        , m_strides{strides.empty() ? getTensorStrides(m_dimensions, sizeof(T)) : strides}
        , m_memoryPtr{static_cast<T *>(memoryPtr)}
        , m_memorySize{0}
        , m_packedStrides{getTensorStrides(m_dimensions, 1)}
        , m_elementCount{std::accumulate(m_dimensions.cbegin(), m_dimensions.cend(), 1u, std::multiplies<size_t>())}
    {
        const size_t dimensionCount = m_dimensions.size();

        for (size_t dimension = 0; dimension < dimensionCount - 1; ++dimension)
        {
            m_memorySize += static_cast<uint64_t>(m_strides[dimension]) * (m_dimensions[dimension] - 1);
        }

        m_memorySize += static_cast<uint64_t>(m_strides[dimensionCount - 1]) * m_dimensions[dimensionCount - 1];
    }

    StridedMemoryUtils(const TensorDimensions &dimensions, const TensorStrides &strides)
        : StridedMemoryUtils(dimensions, strides, nullptr)
    {
        m_data.resize(static_cast<size_t>(m_memorySize / m_strides[m_dimensions.size() - 1]));
        m_memoryPtr = m_data.data();
    }

    T &operator[](size_t index)
    {
        return m_memoryPtr[getElementOffset(index)];
    }

    const T &operator[](size_t index) const
    {
        return m_memoryPtr[getElementOffset(index)];
    }

    T &at(size_t index)
    {
        return m_memoryPtr[getElementOffset(index)];
    }

    T &at(const std::vector<uint64_t> &coordinates)
    {
        return m_memoryPtr[getElementOffset(coordinates)];
    }

    const T &at(const std::vector<uint64_t> &coordinates) const
    {
        return m_memoryPtr[getElementOffset(coordinates)];
    }

    void clear()
    {
        deMemset(m_memoryPtr, 0, static_cast<size_t>(m_memorySize));
    }

    void clear(uint8_t clearValue)
    {
        deMemset(m_memoryPtr, clearValue, static_cast<size_t>(m_memorySize));
    }

    void fill()
    {
        for (size_t element_idx = 0; element_idx < m_elementCount; ++element_idx)
        {
            m_memoryPtr[getElementOffset(element_idx)] = static_cast<T>(element_idx);
        }
    }

    void fill(T startingValue)
    {
        for (size_t element_idx = 0; element_idx < m_elementCount; ++element_idx)
        {
            m_memoryPtr[getElementOffset(element_idx)] = static_cast<T>(startingValue + static_cast<T>(element_idx));
        }
    }

    size_t elementCount() const
    {
        return std::accumulate(m_dimensions.cbegin(), m_dimensions.cend(), 1, std::multiplies<uint32_t>());
    }

    uint64_t memorySize() const
    {
        return m_memorySize;
    }

    T *data() const
    {
        return m_memoryPtr;
    }

    const TensorDimensions shape() const
    {
        return m_dimensions;
    }

private:
    TensorDimensions m_dimensions;
    TensorStrides m_strides;
    T *m_memoryPtr;

    uint64_t m_memorySize;
    TensorStrides m_packedStrides;
    size_t m_elementCount;

    std::vector<T> m_data;

    uint64_t getElementOffset(const std::vector<uint64_t> &coordinates) const
    {
        uint64_t byteOffset = 0;
        for (size_t dim_idx = 0; dim_idx < m_dimensions.size(); ++dim_idx)
        {
            byteOffset += static_cast<uint64_t>(coordinates[dim_idx]) * static_cast<uint64_t>(m_strides[dim_idx]);
        }

        return byteOffset / sizeof(T);
    }

    uint64_t getElementOffset(uint64_t index) const
    {
        return getElementOffset(getCoordinates(index));
    }

    std::vector<uint64_t> getCoordinates(uint64_t index) const
    {
        std::vector<uint64_t> coordinates{};

        // Convert 1D index to n-dimensional coordinates
        for (auto stride : m_packedStrides)
        {
            coordinates.push_back(index / stride);
            index = index % stride;
        }

        return coordinates;
    }
};

} // namespace vk

#endif // _VKTENSORMEMORYUTIL_HPP
