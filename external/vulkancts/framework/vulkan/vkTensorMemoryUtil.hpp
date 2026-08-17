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
#include "tcuTestCase.hpp"

#include <limits>
#include <numeric>
#include <iostream>
#include <sstream>
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

const TensorStrides getTensorStrides(const TensorDimensions &dimensions, size_t formatSize, int64_t multiplier = 1);

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
        , m_elementCount{static_cast<size_t>(std::accumulate(m_dimensions.cbegin(), m_dimensions.cend(),
                                                             static_cast<int64_t>(1u), std::multiplies<int64_t>()))}
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
        constexpr bool isFloat = std::is_same_v<T, tcu::Float32> || std::is_same_v<T, tcu::Float16> ||
                                 std::is_same_v<T, tcu::BrainFloat16> || std::is_same_v<T, tcu::FloatE5M2> ||
                                 std::is_same_v<T, tcu::FloatE4M3>;
        if constexpr (isFloat)
        {
            // Smallest whole number N where N + 0.5 is not representable due to precision
            static constexpr uint64_t halfValuesPrecisionLimit = static_cast<uint64_t>(1) << T::MANTISSA_BITS;

            // Element index where this number N is first to be stored
            static constexpr uint64_t elementIdxHalfValueLimit = static_cast<uint64_t>(halfValuesPrecisionLimit * 2);

            // Bit pattern of this number N
            static constexpr uint64_t upperBase = (T::MANTISSA_BITS + T::EXPONENT_BIAS) << T::MANTISSA_BITS;

            // Bit pattern for maximum normal value of the type
            static constexpr uint64_t maxNormalExponentBitPattern =
                (1 << T::EXPONENT_BITS) - (!std::is_same_v<T, tcu::FloatE4M3> ? 2 : 1);
            static constexpr uint64_t maxNormalMantissaBitPattern =
                (1 << T::MANTISSA_BITS) - (!std::is_same_v<T, tcu::FloatE4M3> ? 1 : 2);
            static constexpr uint64_t maxNormalBitPattern =
                (maxNormalExponentBitPattern << T::MANTISSA_BITS) | maxNormalMantissaBitPattern;

            // Element index wrap to not go above maximum normal value of the type
            static constexpr uint64_t wrap = elementIdxHalfValueLimit + maxNormalBitPattern - upperBase + 1;

            for (uint64_t elementIdx = 0; elementIdx < m_elementCount; ++elementIdx)
            {
                // Wrapped index
                const uint64_t wrapped = elementIdx % wrap;

                if (wrapped < elementIdxHalfValueLimit)
                {
                    // Fill with sequential values increasing by 0.5 until we run out of precision
                    m_memoryPtr[getElementOffset(elementIdx)] = T(static_cast<double>(wrapped) * 0.5);
                }
                else
                {
                    // Then to next representable number
                    const uint64_t floatBitPattern = wrapped - elementIdxHalfValueLimit + upperBase;
                    const int exponentUnbiased =
                        static_cast<int>(floatBitPattern >> T::MANTISSA_BITS) - T::EXPONENT_BIAS;
                    const typename T::StorageType mantissaBitPattern =
                        static_cast<typename T::StorageType>(floatBitPattern & ((1 << T::MANTISSA_BITS) - 1));
                    m_memoryPtr[getElementOffset(elementIdx)] =
                        T::constructBits(1, exponentUnbiased, mantissaBitPattern);
                }
            }
        }
        else
        {
            for (size_t elementIdx = 0; elementIdx < m_elementCount; ++elementIdx)
            {
                m_memoryPtr[getElementOffset(elementIdx)] = static_cast<T>(elementIdx);
            }
        }
    }

    void fill(T startingValue)
    {
        for (size_t elementIdx = 0; elementIdx < m_elementCount; ++elementIdx)
        {
            m_memoryPtr[getElementOffset(elementIdx)] = static_cast<T>(startingValue + static_cast<T>(elementIdx));
        }
    }

#ifndef CTS_USES_VULKANSC
    void fill(T startingValue, const std::vector<vk::DataGraphConstantSparsityHint> &sparsityInfo)
    {
        for (size_t elementIdx = 0; elementIdx < m_elementCount; ++elementIdx)
        {
            bool isZero            = false;
            const auto coordinates = getCoordinates(elementIdx);
            for (const auto sparseInfo : sparsityInfo)
            {
                if ((coordinates.at(sparseInfo.dimension) % sparseInfo.groupSize) < sparseInfo.zeroCount)
                {
                    // at least one sparsity hint requires a 0
                    isZero = true;
                    break;
                }
            }
            m_memoryPtr[getElementOffset(elementIdx)] =
                isZero ? static_cast<T>(0) : static_cast<T>(startingValue + static_cast<T>(elementIdx));
        }
    }
#endif

    size_t elementCount() const
    {
        return static_cast<size_t>(std::accumulate(m_dimensions.cbegin(), m_dimensions.cend(), static_cast<int64_t>(1),
                                                   std::multiplies<int64_t>()));
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

template <typename T>
tcu::TestStatus compareStridedMemory(const StridedMemoryUtils<T> &first, const StridedMemoryUtils<T> &second,
                                     const double eps = .01)
{
    constexpr bool isFloat = std::is_same_v<T, tcu::Float32> || std::is_same_v<T, tcu::Float16> ||
                             std::is_same_v<T, tcu::BrainFloat16> || std::is_same_v<T, tcu::FloatE5M2> ||
                             std::is_same_v<T, tcu::FloatE4M3>;

    const size_t elementCount = first.elementCount();
    DE_ASSERT(elementCount == second.elementCount());

    if constexpr (isFloat)
    {
        for (size_t i = 0; i < elementCount; ++i)
        {
            const double value    = first[i].asDouble();
            const double expected = second[i].asDouble();

            if ((first[i].isNaN() && !second[i].isNaN()) || (!first[i].isNaN() && second[i].isNaN()))
            {
                std::ostringstream msg;
                msg << "Only one of the values are NaN at index " << i << ": first = " << value
                    << ", second = " << expected;
                return tcu::TestStatus::fail(msg.str());
            }

            const double error = std::abs(value - expected);
            if (error > eps)
            {
                std::ostringstream msg;
                msg << "Error between first and second buffer is too large at index " << i << ": Error: " << error
                    << " > " << eps << ", first = " << value << ", second = " << expected;
                return tcu::TestStatus::fail(msg.str());
            }
        }
    }
    else
    {
        for (size_t i = 0; i < elementCount; ++i)
        {
            if (first[i] != second[i])
            {
                std::ostringstream msg;
                msg << "Comparison failed at index " << i << ": tensor = " << int(first[i])
                    << ", buffer = " << int(second[i]);
                return tcu::TestStatus::fail(msg.str());
            }
        }
    }

    return tcu::TestStatus::pass("Pass");
}

} // namespace vk

#endif // _VKTENSORMEMORYUTIL_HPP
