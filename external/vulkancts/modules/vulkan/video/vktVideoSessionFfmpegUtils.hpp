#ifndef _VKTVIDEOSESSIONFFMPEGUTILS_HPP
#define _VKTVIDEOSESSIONFFMPEGUTILS_HPP
/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2021 The Khronos Group Inc.
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
 * \brief Video Session Ffmpeg Utils
 *//*--------------------------------------------------------------------*/

#include "deUniquePtr.hpp"
#include "deDefs.hpp"
#include "vkPlatform.hpp"
#include <vector>

namespace vkt
{
namespace video
{

class IfcFfmpegDemuxer
{
public:
    virtual bool demux(uint8_t **pData, int64_t *pSize) = 0;
    virtual ~IfcFfmpegDemuxer()
    {
    }
};

class IfcFfmpegFunctions
{
public:
    virtual IfcFfmpegDemuxer *createIfcFfmpegDemuxer(de::MovePtr<std::vector<uint8_t>> &data) = 0;
    virtual ~IfcFfmpegFunctions(){};
};

de::MovePtr<IfcFfmpegFunctions> createIfcFfmpegFunctions();

} // namespace video
} // namespace vkt

#endif // _VKTVIDEOSESSIONFFMPEGUTILS_HPP
