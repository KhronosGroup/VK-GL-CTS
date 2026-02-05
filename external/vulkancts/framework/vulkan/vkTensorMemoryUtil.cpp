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

#include "vkTensorMemoryUtil.hpp"

namespace vk
{

const TensorStrides getTensorStrides(const TensorDimensions &dimensions, size_t formatSize, size_t multiplier)
{
    DE_ASSERT(dimensions.size() > 0);
    TensorStrides strides{};

    for (size_t dim_idx = 0; dim_idx < (dimensions.size() - 1); ++dim_idx)
    {
        strides.push_back(multiplier * std::accumulate(dimensions.cbegin() + dim_idx + 1, dimensions.cend(), formatSize,
                                                       std::multiplies<size_t>()));
    }

    strides.push_back(formatSize);

    return strides;
}

} // namespace vk
