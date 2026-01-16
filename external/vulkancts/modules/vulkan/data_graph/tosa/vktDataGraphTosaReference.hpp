#ifndef _VKTDATAGRAPHTOSAREFERENCE_HPP
#define _VKTDATAGRAPHTOSAREFERENCE_HPP
/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2024-2025 ARM Ltd.
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
 * \brief Tosa minimal reference implementation
 */
/*--------------------------------------------------------------------*/

#include "vktDataGraphTosaUtil.hpp"
#include <cstdint>

namespace vkt
{
namespace dataGraph
{

#define MAXPOOL_LAYOUT_COUNT 4

class TosaReferenceImplementation
{
public:
    template <typename inOutType>
    static void add(const tensor::StridedMemoryUtils<inOutType> &inputData1,
                    const tensor::StridedMemoryUtils<inOutType> &inputData2,
                    tensor::StridedMemoryUtils<inOutType> &outputData)
    {
        for (size_t i = 0; i < outputData.elementCount(); i++)
        {
            outputData[i] = inputData1[i] + inputData2[i];
        }
    }

    template <typename inOutType>
    static void sub(const tensor::StridedMemoryUtils<inOutType> &inputData1,
                    const tensor::StridedMemoryUtils<inOutType> &inputData2,
                    tensor::StridedMemoryUtils<inOutType> &outputData)
    {
        for (size_t i = 0; i < outputData.elementCount(); i++)
        {
            outputData[i] = inputData1[i] - inputData2[i];
        }
    }

    template <typename inType, typename outType>
    static void vector_cast(const tensor::StridedMemoryUtils<inType> &inputData,
                            tensor::StridedMemoryUtils<outType> &outputData)
    {
        DE_TEST_ASSERT(inputData.elementCount() == outputData.elementCount());
        for (size_t i = 0; i < inputData.elementCount(); i++)
        {
            outputData[i] = static_cast<outType>(inputData[i]);
        }
    }

    static std::vector<uint64_t> indexToCoordinates(std::vector<int64_t> shape, uint64_t index)
    {
        std::vector<uint64_t> coords(shape.size());
        for (uint64_t i = 0; i < shape.size(); i++)
        {
            size_t backIdx  = static_cast<size_t>((shape.size() - 1) - i);
            coords[backIdx] = index % static_cast<uint64_t>(shape[backIdx]);
            index /= static_cast<uint64_t>(shape[backIdx]);
        }
        return coords;
    };

    template <typename inOutType>
    static void maxpool2d(const tensor::StridedMemoryUtils<inOutType> &inTensor, uint32_t poolHeight,
                          uint32_t poolWidth, tensor::StridedMemoryUtils<inOutType> &outTensor)
    {
        for (uint32_t outIndex = 0; outIndex < outTensor.elementCount(); outIndex++)
        {
            const auto outCoords = indexToCoordinates(outTensor.shape(), outIndex);

            const auto n  = outCoords[0];
            const auto oh = outCoords[1];
            const auto ow = outCoords[2];
            const auto c  = outCoords[3];

            inOutType max_pool = std::numeric_limits<inOutType>::lowest();

            for (uint32_t i = 0; i < poolHeight; i++)
            {
                for (uint32_t j = 0; j < poolWidth; j++)
                {
                    uint64_t ph = oh * poolHeight + i;
                    uint64_t pw = ow * poolWidth + j;

                    max_pool = std::max(max_pool, inTensor.at({n, ph, pw, c}));
                }
            }

            outTensor.at(outCoords) = max_pool;
        }
    };

    template <typename inType, typename weightType, typename outType>
    static void conv2d(const tensor::StridedMemoryUtils<inType> &inTensor,
                       const tensor::StridedMemoryUtils<weightType> &weightTensor,
                       const tensor::StridedMemoryUtils<outType> &biasTensor,
                       tensor::StridedMemoryUtils<outType> &outTensor, const std::vector<int> &padding,
                       const std::array<int32_t, 2> &stride, const std::array<int32_t, 2> &dilation, inType tensorZP,
                       weightType weightZP)
    {
        const uint64_t inHeight   = static_cast<uint64_t>(inTensor.shape()[1]);
        const uint64_t inWidth    = static_cast<uint64_t>(inTensor.shape()[2]);
        const uint64_t inChannels = static_cast<uint64_t>(inTensor.shape()[3]);

        const uint64_t kernelHeight = static_cast<uint64_t>(weightTensor.shape()[1]);
        const uint64_t kernelWidth  = static_cast<uint64_t>(weightTensor.shape()[2]);

        for (uint32_t outIndex = 0; outIndex < outTensor.elementCount(); outIndex++)
        {
            const auto outCoords = indexToCoordinates(outTensor.shape(), outIndex);

            const auto n  = outCoords[0];
            const auto oh = outCoords[1];
            const auto ow = outCoords[2];
            const auto oc = outCoords[3];

            outType acc = 0;
            uint64_t h  = oh * stride[0] - padding[0];
            uint64_t w  = ow * stride[1] - padding[2];

            for (uint64_t kh = 0; kh < kernelHeight; ++kh)
            {
                for (uint64_t kw = 0; kw < kernelWidth; ++kw)
                {
                    for (uint64_t ic = 0; ic < inChannels; ++ic)
                    {
                        uint64_t ih = h + kh * dilation[0];
                        uint64_t iw = w + kw * dilation[1];

                        if (ih < inHeight && iw < inWidth)
                        {
                            outType value  = static_cast<outType>(inTensor.at({n, ih, iw, ic}));
                            outType weight = static_cast<outType>(weightTensor.at({oc, kh, kw, ic}));

                            value -= static_cast<outType>(tensorZP);
                            weight -= static_cast<outType>(weightZP);
                            acc += value * weight;
                        }
                    }
                }
            }

            outTensor.at(outCoords) = acc + biasTensor.at({oc});
        }
    };
};

} // namespace dataGraph
} // namespace vkt

#endif // _VKTDATAGRAPHTOSAREFERENCE_HPP