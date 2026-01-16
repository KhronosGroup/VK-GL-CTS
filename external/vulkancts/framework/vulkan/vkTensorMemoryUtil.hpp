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
#include "tcuFloat.hpp"

#include <limits>
#include <numeric>
#include <iostream>
#include <stddef.h>
#include <stdint.h>
#include <vector>

#ifndef CTS_USES_VULKANSC
#include "vkDataGraphUtil.hpp"
#endif

namespace vk
{

using TensorDimensions = std::vector<int64_t>;
using TensorStrides    = std::vector<int64_t>;

const TensorStrides getTensorStrides(const TensorDimensions &dimensions, size_t formatSize, size_t multiplier = 1);

struct Float16
{
    tcu::Float16 value;

    Float16() = default;

    // Copy constructor
    Float16(const Float16 &other) = default;

    template <typename T>
    Float16(T val) : value(static_cast<float>(val), tcu::ROUND_TO_EVEN)
    {
    }

    // Copy assignment operator
    Float16 &operator=(const Float16 &rhs) = default;

    // Assignment from float
    Float16 &operator=(float rhs)
    {
        value = tcu::Float16(rhs, tcu::ROUND_TO_EVEN);
        return *this;
    }

    // Implicitly convert back to float
    operator float() const
    {
        return value.asFloat();
    }

    // Comparison operators
    friend bool operator==(Float16 lhs, Float16 rhs)
    {
        return float(lhs) == float(rhs);
    }
    friend bool operator!=(Float16 lhs, Float16 rhs)
    {
        return !(lhs == rhs);
    }
    friend bool operator<(Float16 lhs, Float16 rhs)
    {
        return float(lhs) < float(rhs);
    }
    friend bool operator<=(Float16 lhs, Float16 rhs)
    {
        return float(lhs) <= float(rhs);
    }
    friend bool operator>(Float16 lhs, Float16 rhs)
    {
        return float(lhs) > float(rhs);
    }
    friend bool operator>=(Float16 lhs, Float16 rhs)
    {
        return float(lhs) >= float(rhs);
    }

    // Arithmetic operators
    friend Float16 operator+(Float16 lhs, Float16 rhs)
    {
        return Float16(float(lhs) + float(rhs));
    }
    friend Float16 operator-(Float16 lhs, Float16 rhs)
    {
        return Float16(float(lhs) - float(rhs));
    }
    friend Float16 operator*(Float16 lhs, Float16 rhs)
    {
        return Float16(float(lhs) * float(rhs));
    }
    friend Float16 operator/(Float16 lhs, Float16 rhs)
    {
        return Float16(float(lhs) / float(rhs));
    }

    // Compound-assignment
    Float16 &operator+=(const Float16 &rhs)
    {
        *this = *this + rhs;
        return *this;
    }
    Float16 &operator-=(const Float16 &rhs)
    {
        *this = *this - rhs;
        return *this;
    }
    Float16 &operator*=(const Float16 &rhs)
    {
        *this = *this * rhs;
        return *this;
    }
    Float16 &operator/=(const Float16 &rhs)
    {
        *this = *this / rhs;
        return *this;
    }

    // Stream-out as float
    friend std::ostream &operator<<(std::ostream &os, const Float16 &v)
    {
        os << v.value.asFloat();
        return os;
    }
};

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

#ifndef CTS_USES_VULKANSC
    void fill(T startingValue, const std::vector<vk::DataGraphConstantSparsityHint> &sparsityInfo)
    {
        for (size_t element_idx = 0; element_idx < m_elementCount; ++element_idx)
        {
            bool isZero            = false;
            const auto coordinates = getCoordinates(element_idx);
            for (const auto sparseInfo : sparsityInfo)
            {
                if ((coordinates.at(sparseInfo.dimension) % sparseInfo.groupSize) < sparseInfo.zeroCount)
                {
                    // at least one sparsity hint requires a 0
                    isZero = true;
                    break;
                }
            }
            m_memoryPtr[getElementOffset(element_idx)] =
                isZero ? static_cast<T>(0) : static_cast<T>(startingValue + static_cast<T>(element_idx));
        }
    }
#endif

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
