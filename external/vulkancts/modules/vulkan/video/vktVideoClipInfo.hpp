#ifndef _VKTVIDEOCLIPINFO_HPP
#define _VKTVIDEOCLIPINFO_HPP
/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2023 The Khronos Group Inc.
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
 * \brief Reference checksums for video decode validation
 *
 * See the <vulkan_data_dir>/vulkan/video/frame_checksums.py file for
 * instructions on generating the checksums for new tests.
 *--------------------------------------------------------------------*/
#include <array>
#include <vector>
#include <string>
#include <sstream>

#include "deDefs.hpp"
#include "vktVideoTestUtils.hpp"

#include "vktDemuxer.hpp"

namespace vkt
{
namespace video
{

enum ClipName
{
    CLIP_H264_DEC_A = 0,
    CLIP_H264_DEC_C,
    CLIP_H265_DEC_D,
    CLIP_H264_ENC_E,
    CLIP_H265_ENC_F,
    CLIP_H264_ENC_G,
    CLIP_H265_ENC_H,
    CLIP_H264_DEC_4K_26_IBP_MAIN,
    CLIP_H265_DEC_JELLY,
    CLIP_H265_DEC_ITU_SLIST_A,
    CLIP_H265_DEC_ITU_SLIST_B,

    CLIP_AV1_DEC_BASIC_8,
    CLIP_AV1_DEC_ALLINTRA_8,
    CLIP_AV1_DEC_ALLINTRA_INTRABC_8,
    CLIP_AV1_DEC_CDFUPDATE_8,
    CLIP_AV1_DEC_GLOBALMOTION_8,
    CLIP_AV1_DEC_FILMGRAIN_8,
    CLIP_AV1_DEC_SVCL1T2_8,
    CLIP_AV1_DEC_SUPERRES_8,
    CLIP_AV1_DEC_SIZEUP_8,

    CLIP_AV1_DEC_BASIC_10,
    CLIP_AV1_DEC_ORDERHINT_10,
    CLIP_AV1_DEC_FORWARDKEYFRAME_10,
    CLIP_AV1_DEC_LOSSLESS_10,
    CLIP_AV1_DEC_LOOPFILTER_10,
    CLIP_AV1_DEC_CDEF_10,
    CLIP_AV1_DEC_ARGON_FILMGRAIN_10,
    CLIP_AV1_DEC_ARGON_TEST_787,

    CLIP_LAST,
};
struct VideoProfileInfo
{
    VkVideoCodecOperationFlagBitsKHR codecOperation;
    VkVideoChromaSubsamplingFlagBitsKHR subsamplingFlags;
    VkVideoComponentBitDepthFlagBitsKHR lumaBitDepth;
    VkVideoComponentBitDepthFlagBitsKHR chromaBitDepth;
    int profileIDC;

    VideoProfileInfo(VkVideoCodecOperationFlagBitsKHR codecOp,
                     VkVideoChromaSubsamplingFlagBitsKHR subsampleFlags = VK_VIDEO_CHROMA_SUBSAMPLING_420_BIT_KHR,
                     VkVideoComponentBitDepthFlagBitsKHR lumaDepth      = VK_VIDEO_COMPONENT_BIT_DEPTH_8_BIT_KHR,
                     VkVideoComponentBitDepthFlagBitsKHR chromaDepth    = VK_VIDEO_COMPONENT_BIT_DEPTH_8_BIT_KHR,
                     int profile                                        = 0)
        : codecOperation(codecOp)
        , subsamplingFlags(subsampleFlags)
        , lumaBitDepth(lumaDepth)
        , chromaBitDepth(chromaDepth)
        , profileIDC(profile)
    {
    }
};

struct ClipInfo
{
    ClipName name;
    const char *filename;
    std::vector<VideoProfileInfo> sessionProfiles;
    ElementaryStreamFraming framing{ElementaryStreamFraming::UNKNOWN};
    uint32_t frameWidth{0};
    uint32_t frameHeight{0};
    uint32_t frameRate{0};
    int totalFrames{0};
    uint32_t framesInGOP{0};
    const char **frameChecksums{nullptr};
};

const ClipInfo *clipInfo(ClipName c);
const char *checksumForClipFrame(const ClipInfo *cinfo, int frameNumber);

} // namespace video
} // namespace vkt
#endif // _VKTVIDEOCLIPINFO_HPP
