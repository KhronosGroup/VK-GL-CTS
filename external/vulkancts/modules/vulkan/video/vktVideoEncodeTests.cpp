/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2024 The Khronos Group Inc.
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
 * \brief Video Encoding Session tests
 */
/*--------------------------------------------------------------------*/

#include "tcuCommandLine.hpp"
#include "vktVideoTestUtils.hpp"
#include "vktVideoEncodeTests.hpp"
#include "vktVideoTestUtils.hpp"
#include "vktTestCase.hpp"

#ifdef DE_BUILD_VIDEO
#include "vktVideoBaseDecodeUtils.hpp"
#endif

#include "tcuTextureUtil.hpp"
#include "tcuVectorUtil.hpp"
#include "tcuTestLog.hpp"
#include "tcuPlatform.hpp"
#include "tcuFunctionLibrary.hpp"
#include "tcuSurface.hpp"

#include "tcuTexture.hpp"
#include "tcuVector.hpp"
#include "tcuPixelFormat.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuImageCompare.hpp"

#include "vkDefs.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkImageWithMemory.hpp"
#include "vkImageUtil.hpp"
#include "vkBarrierUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkTypeUtil.hpp"

#include "vktVideoClipInfo.hpp"
#include "ycbcr/vktYCbCrUtil.hpp"

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <string>
#include <algorithm>
#include <cmath>

#ifndef VK_MAX_NUM_IMAGE_PLANES_KHR
#define VK_MAX_NUM_IMAGE_PLANES_KHR 4
#endif

namespace vkt
{
namespace video
{
namespace
{
using namespace vk;
using namespace std;

using de::MovePtr;
using vkt::ycbcr::MultiPlaneImageData;

template <typename T>
std::tuple<T, T> refs(T a, T b)
{
    return std::make_tuple(a, b);
}

typedef de::SharedPtr<vk::Unique<vk::VkSemaphore>> SemaphoreSp;

enum TestType
{
    TEST_TYPE_H264_ENCODE_I,      // Encode one I frame
    TEST_TYPE_H264_ENCODE_RC_VBR, // Encode one I frame with enabled variable rate control, maximum QP value equal to 42
    TEST_TYPE_H264_ENCODE_RC_CBR, // Encode one I frame with enabled constant rate control, maximum QP value equal to 42
    TEST_TYPE_H264_ENCODE_RC_DISABLE,    // Encode one I frame with disabled rate control, constant QP value equal to 28
    TEST_TYPE_H264_ENCODE_QUALITY_LEVEL, // Encode one I frame with quality level set to 0
    TEST_TYPE_H264_ENCODE_QM_DELTA_RC_VBR, // Encode three I frame with enabled quantization map with enabled variable rate control, maximum QP value equal to 42
    TEST_TYPE_H264_ENCODE_QM_DELTA_RC_CBR, // Encode three I frame with enabled quantization map with enabled constant rate control, maximum QP value equal to 42
    TEST_TYPE_H264_ENCODE_QM_DELTA_RC_DISABLE, // Encode three I frame with enabled quantization map with delta values and disabled rate control
    TEST_TYPE_H264_ENCODE_QM_DELTA, // Encode one I frame with enabled quantization map with delta values
    TEST_TYPE_H264_ENCODE_QM_EMPHASIS_CBR, // Encode one I frame with enabled quantization map with emphasis values and enabled constant rate control
    TEST_TYPE_H264_ENCODE_QM_EMPHASIS_VBR, // Encode one I frame with enabled quantization map with emphasis values and enabled variable rate control
    TEST_TYPE_H264_ENCODE_USAGE, // Encode one I frame with non-default encode usage setup
    TEST_TYPE_H264_ENCODE_I_P, // Encode one I frame and one P frame, recording and submission order match encode order
    TEST_TYPE_H264_ENCODE_I_P_NOT_MATCHING_ORDER, // Encode one I frame, one P frame, recording and submission order not matching encoding order
    TEST_TYPE_H264_I_P_B_13, // Encode two 13-frame GOPs, both I, P, and B frames recording and submission order match encode order
    TEST_TYPE_H264_ENCODE_QUERY_RESULT_WITH_STATUS, // Encode one I frame, one P frame with status query reported successfully for both frames. Recording and submission order match encode order
    TEST_TYPE_H264_ENCODE_INLINE_QUERY, // VK_KHR_video_maintenance1 required test: Encode one I frame with inline without vkCmdBegin/EndQuery
    TEST_TYPE_H264_ENCODE_RESOURCES_WITHOUT_PROFILES, // VK_KHR_video_maintenance1 required test: Encode one I frame with DPB resources defined without passing an encode profile
    TEST_TYPE_H264_ENCODE_RESOLUTION_CHANGE_DPB, // Encode one I frame and one P frame with session created with a smaller resolution than extracted frame

    // New intra refresh test types for H264
    TEST_TYPE_H264_ENCODE_INTRA_REFRESH_PICTURE_PARTITION,
    TEST_TYPE_H264_ENCODE_INTRA_REFRESH_ANY_BLOCK_BASED,
    TEST_TYPE_H264_ENCODE_INTRA_REFRESH_ROW_BASED,
    TEST_TYPE_H264_ENCODE_INTRA_REFRESH_COLUMN_BASED,

    // Empty-region intra refresh test types for H264
    TEST_TYPE_H264_ENCODE_INTRA_REFRESH_ANY_BLOCK_BASED_EMPTY_REGION,
    TEST_TYPE_H264_ENCODE_INTRA_REFRESH_ROW_BASED_EMPTY_REGION,
    TEST_TYPE_H264_ENCODE_INTRA_REFRESH_COLUMN_BASED_EMPTY_REGION,

    // Mid-way intra refresh test types for H264
    TEST_TYPE_H264_ENCODE_INTRA_REFRESH_PICTURE_PARTITION_MIDWAY,
    TEST_TYPE_H264_ENCODE_INTRA_REFRESH_ANY_BLOCK_BASED_MIDWAY,
    TEST_TYPE_H264_ENCODE_INTRA_REFRESH_ROW_BASED_MIDWAY,
    TEST_TYPE_H264_ENCODE_INTRA_REFRESH_COLUMN_BASED_MIDWAY,

    TEST_TYPE_H265_ENCODE_I,
    TEST_TYPE_H265_ENCODE_RC_VBR,
    TEST_TYPE_H265_ENCODE_RC_CBR,
    TEST_TYPE_H265_ENCODE_RC_DISABLE,
    TEST_TYPE_H265_ENCODE_QUALITY_LEVEL,
    TEST_TYPE_H265_ENCODE_QM_DELTA_RC_VBR,
    TEST_TYPE_H265_ENCODE_QM_DELTA_RC_CBR,
    TEST_TYPE_H265_ENCODE_QM_DELTA_RC_DISABLE,
    TEST_TYPE_H265_ENCODE_QM_DELTA,
    TEST_TYPE_H265_ENCODE_QM_EMPHASIS_CBR,
    TEST_TYPE_H265_ENCODE_QM_EMPHASIS_VBR,
    TEST_TYPE_H265_ENCODE_USAGE,
    TEST_TYPE_H265_ENCODE_I_P,
    TEST_TYPE_H265_ENCODE_I_P_NOT_MATCHING_ORDER,
    TEST_TYPE_H265_I_P_B_13,
    TEST_TYPE_H265_ENCODE_QUERY_RESULT_WITH_STATUS,
    TEST_TYPE_H265_ENCODE_INLINE_QUERY,
    TEST_TYPE_H265_ENCODE_RESOURCES_WITHOUT_PROFILES,
    TEST_TYPE_H265_ENCODE_RESOLUTION_CHANGE_DPB,

    // New intra refresh test types for H265
    TEST_TYPE_H265_ENCODE_INTRA_REFRESH_PICTURE_PARTITION,
    TEST_TYPE_H265_ENCODE_INTRA_REFRESH_ANY_BLOCK_BASED,
    TEST_TYPE_H265_ENCODE_INTRA_REFRESH_ROW_BASED,
    TEST_TYPE_H265_ENCODE_INTRA_REFRESH_COLUMN_BASED,

    // Empty-region intra refresh test types for H265
    TEST_TYPE_H265_ENCODE_INTRA_REFRESH_ANY_BLOCK_BASED_EMPTY_REGION,
    TEST_TYPE_H265_ENCODE_INTRA_REFRESH_ROW_BASED_EMPTY_REGION,
    TEST_TYPE_H265_ENCODE_INTRA_REFRESH_COLUMN_BASED_EMPTY_REGION,

    // Mid-way intra refresh test types for H265
    TEST_TYPE_H265_ENCODE_INTRA_REFRESH_PICTURE_PARTITION_MIDWAY,
    TEST_TYPE_H265_ENCODE_INTRA_REFRESH_ANY_BLOCK_BASED_MIDWAY,
    TEST_TYPE_H265_ENCODE_INTRA_REFRESH_ROW_BASED_MIDWAY,
    TEST_TYPE_H265_ENCODE_INTRA_REFRESH_COLUMN_BASED_MIDWAY,

    TEST_TYPE_LAST
};

enum TestCodec
{
    TEST_CODEC_H264,
    TEST_CODEC_H265,

    TEST_CODEC_LAST
};

const char *getTestName(const TestType testType)
{
    switch (testType)
    {
    case TEST_TYPE_H264_ENCODE_I:
    case TEST_TYPE_H265_ENCODE_I:
        return "i";
    case TEST_TYPE_H264_ENCODE_RC_VBR:
    case TEST_TYPE_H265_ENCODE_RC_VBR:
        return "rc_vbr";
    case TEST_TYPE_H264_ENCODE_RC_CBR:
    case TEST_TYPE_H265_ENCODE_RC_CBR:
        return "rc_cbr";
    case TEST_TYPE_H264_ENCODE_RC_DISABLE:
    case TEST_TYPE_H265_ENCODE_RC_DISABLE:
        return "rc_disable";
    case TEST_TYPE_H264_ENCODE_QUALITY_LEVEL:
    case TEST_TYPE_H265_ENCODE_QUALITY_LEVEL:
        return "quality_level";
    case TEST_TYPE_H264_ENCODE_QM_DELTA_RC_VBR:
    case TEST_TYPE_H265_ENCODE_QM_DELTA_RC_VBR:
        return "quantization_map_delta_rc_vbr";
    case TEST_TYPE_H264_ENCODE_QM_DELTA_RC_CBR:
    case TEST_TYPE_H265_ENCODE_QM_DELTA_RC_CBR:
        return "quantization_map_delta_rc_cbr";
    case TEST_TYPE_H264_ENCODE_QM_DELTA_RC_DISABLE:
    case TEST_TYPE_H265_ENCODE_QM_DELTA_RC_DISABLE:
        return "quantization_map_delta_rc_disable";
    case TEST_TYPE_H264_ENCODE_QM_DELTA:
    case TEST_TYPE_H265_ENCODE_QM_DELTA:
        return "quantization_map_delta";
    case TEST_TYPE_H264_ENCODE_QM_EMPHASIS_CBR:
    case TEST_TYPE_H265_ENCODE_QM_EMPHASIS_CBR:
        return "quantization_map_emphasis_cbr";
    case TEST_TYPE_H264_ENCODE_QM_EMPHASIS_VBR:
    case TEST_TYPE_H265_ENCODE_QM_EMPHASIS_VBR:
        return "quantization_map_emphasis_vbr";
    case TEST_TYPE_H264_ENCODE_USAGE:
    case TEST_TYPE_H265_ENCODE_USAGE:
        return "usage";
    case TEST_TYPE_H264_ENCODE_I_P:
    case TEST_TYPE_H265_ENCODE_I_P:
        return "i_p";
    case TEST_TYPE_H264_ENCODE_I_P_NOT_MATCHING_ORDER:
    case TEST_TYPE_H265_ENCODE_I_P_NOT_MATCHING_ORDER:
        return "i_p_not_matching_order";
    case TEST_TYPE_H264_I_P_B_13:
    case TEST_TYPE_H265_I_P_B_13:
        return "i_p_b_13";
    case TEST_TYPE_H264_ENCODE_RESOLUTION_CHANGE_DPB:
    case TEST_TYPE_H265_ENCODE_RESOLUTION_CHANGE_DPB:
        return "resolution_change_dpb";
    case TEST_TYPE_H264_ENCODE_QUERY_RESULT_WITH_STATUS:
    case TEST_TYPE_H265_ENCODE_QUERY_RESULT_WITH_STATUS:
        return "query_with_status";
    case TEST_TYPE_H264_ENCODE_INLINE_QUERY:
    case TEST_TYPE_H265_ENCODE_INLINE_QUERY:
        return "inline_query";
    case TEST_TYPE_H264_ENCODE_RESOURCES_WITHOUT_PROFILES:
    case TEST_TYPE_H265_ENCODE_RESOURCES_WITHOUT_PROFILES:
        return "resources_without_profiles";
    case TEST_TYPE_H264_ENCODE_INTRA_REFRESH_PICTURE_PARTITION:
    case TEST_TYPE_H265_ENCODE_INTRA_REFRESH_PICTURE_PARTITION:
        return "intra_refresh_picture_partition";
    case TEST_TYPE_H264_ENCODE_INTRA_REFRESH_ANY_BLOCK_BASED:
    case TEST_TYPE_H265_ENCODE_INTRA_REFRESH_ANY_BLOCK_BASED:
        return "intra_refresh_any_block_based";
    case TEST_TYPE_H264_ENCODE_INTRA_REFRESH_ROW_BASED:
    case TEST_TYPE_H265_ENCODE_INTRA_REFRESH_ROW_BASED:
        return "intra_refresh_row_based";
    case TEST_TYPE_H264_ENCODE_INTRA_REFRESH_COLUMN_BASED:
    case TEST_TYPE_H265_ENCODE_INTRA_REFRESH_COLUMN_BASED:
        return "intra_refresh_column_based";
    case TEST_TYPE_H264_ENCODE_INTRA_REFRESH_ANY_BLOCK_BASED_EMPTY_REGION:
    case TEST_TYPE_H265_ENCODE_INTRA_REFRESH_ANY_BLOCK_BASED_EMPTY_REGION:
        return "intra_refresh_any_block_based_empty_region";
    case TEST_TYPE_H264_ENCODE_INTRA_REFRESH_ROW_BASED_EMPTY_REGION:
    case TEST_TYPE_H265_ENCODE_INTRA_REFRESH_ROW_BASED_EMPTY_REGION:
        return "intra_refresh_row_based_empty_region";
    case TEST_TYPE_H264_ENCODE_INTRA_REFRESH_COLUMN_BASED_EMPTY_REGION:
    case TEST_TYPE_H265_ENCODE_INTRA_REFRESH_COLUMN_BASED_EMPTY_REGION:
        return "intra_refresh_column_based_empty_region";
    case TEST_TYPE_H264_ENCODE_INTRA_REFRESH_PICTURE_PARTITION_MIDWAY:
    case TEST_TYPE_H265_ENCODE_INTRA_REFRESH_PICTURE_PARTITION_MIDWAY:
        return "intra_refresh_picture_partition_midway";
    case TEST_TYPE_H264_ENCODE_INTRA_REFRESH_ANY_BLOCK_BASED_MIDWAY:
    case TEST_TYPE_H265_ENCODE_INTRA_REFRESH_ANY_BLOCK_BASED_MIDWAY:
        return "intra_refresh_any_block_based_midway";
    case TEST_TYPE_H264_ENCODE_INTRA_REFRESH_ROW_BASED_MIDWAY:
    case TEST_TYPE_H265_ENCODE_INTRA_REFRESH_ROW_BASED_MIDWAY:
        return "intra_refresh_row_based_midway";
    case TEST_TYPE_H264_ENCODE_INTRA_REFRESH_COLUMN_BASED_MIDWAY:
    case TEST_TYPE_H265_ENCODE_INTRA_REFRESH_COLUMN_BASED_MIDWAY:
        return "intra_refresh_column_based_midway";
    default:
        TCU_THROW(InternalError, "Unknown TestType");
    }
}

enum TestCodec getTestCodec(const TestType testType)
{
    switch (testType)
    {
    case TEST_TYPE_H264_ENCODE_I:
    case TEST_TYPE_H264_ENCODE_RC_VBR:
    case TEST_TYPE_H264_ENCODE_RC_CBR:
    case TEST_TYPE_H264_ENCODE_RC_DISABLE:
    case TEST_TYPE_H264_ENCODE_QUALITY_LEVEL:
    case TEST_TYPE_H264_ENCODE_USAGE:
    case TEST_TYPE_H264_ENCODE_I_P:
    case TEST_TYPE_H264_ENCODE_I_P_NOT_MATCHING_ORDER:
    case TEST_TYPE_H264_I_P_B_13:
    case TEST_TYPE_H264_ENCODE_RESOLUTION_CHANGE_DPB:
    case TEST_TYPE_H264_ENCODE_QUERY_RESULT_WITH_STATUS:
    case TEST_TYPE_H264_ENCODE_INLINE_QUERY:
    case TEST_TYPE_H264_ENCODE_RESOURCES_WITHOUT_PROFILES:
    case TEST_TYPE_H264_ENCODE_QM_DELTA_RC_VBR:
    case TEST_TYPE_H264_ENCODE_QM_DELTA_RC_CBR:
    case TEST_TYPE_H264_ENCODE_QM_DELTA_RC_DISABLE:
    case TEST_TYPE_H264_ENCODE_QM_DELTA:
    case TEST_TYPE_H264_ENCODE_QM_EMPHASIS_CBR:
    case TEST_TYPE_H264_ENCODE_QM_EMPHASIS_VBR:
    case TEST_TYPE_H264_ENCODE_INTRA_REFRESH_PICTURE_PARTITION:
    case TEST_TYPE_H264_ENCODE_INTRA_REFRESH_ANY_BLOCK_BASED:
    case TEST_TYPE_H264_ENCODE_INTRA_REFRESH_ROW_BASED:
    case TEST_TYPE_H264_ENCODE_INTRA_REFRESH_COLUMN_BASED:
    case TEST_TYPE_H264_ENCODE_INTRA_REFRESH_ANY_BLOCK_BASED_EMPTY_REGION:
    case TEST_TYPE_H264_ENCODE_INTRA_REFRESH_ROW_BASED_EMPTY_REGION:
    case TEST_TYPE_H264_ENCODE_INTRA_REFRESH_COLUMN_BASED_EMPTY_REGION:
    case TEST_TYPE_H264_ENCODE_INTRA_REFRESH_PICTURE_PARTITION_MIDWAY:
    case TEST_TYPE_H264_ENCODE_INTRA_REFRESH_ANY_BLOCK_BASED_MIDWAY:
    case TEST_TYPE_H264_ENCODE_INTRA_REFRESH_ROW_BASED_MIDWAY:
    case TEST_TYPE_H264_ENCODE_INTRA_REFRESH_COLUMN_BASED_MIDWAY:
        return TEST_CODEC_H264;
    case TEST_TYPE_H265_ENCODE_I:
    case TEST_TYPE_H265_ENCODE_RC_VBR:
    case TEST_TYPE_H265_ENCODE_RC_CBR:
    case TEST_TYPE_H265_ENCODE_RC_DISABLE:
    case TEST_TYPE_H265_ENCODE_QUALITY_LEVEL:
    case TEST_TYPE_H265_ENCODE_USAGE:
    case TEST_TYPE_H265_ENCODE_I_P:
    case TEST_TYPE_H265_ENCODE_I_P_NOT_MATCHING_ORDER:
    case TEST_TYPE_H265_I_P_B_13:
    case TEST_TYPE_H265_ENCODE_RESOLUTION_CHANGE_DPB:
    case TEST_TYPE_H265_ENCODE_QUERY_RESULT_WITH_STATUS:
    case TEST_TYPE_H265_ENCODE_INLINE_QUERY:
    case TEST_TYPE_H265_ENCODE_RESOURCES_WITHOUT_PROFILES:
    case TEST_TYPE_H265_ENCODE_QM_DELTA_RC_VBR:
    case TEST_TYPE_H265_ENCODE_QM_DELTA_RC_CBR:
    case TEST_TYPE_H265_ENCODE_QM_DELTA_RC_DISABLE:
    case TEST_TYPE_H265_ENCODE_QM_DELTA:
    case TEST_TYPE_H265_ENCODE_QM_EMPHASIS_CBR:
    case TEST_TYPE_H265_ENCODE_QM_EMPHASIS_VBR:
    case TEST_TYPE_H265_ENCODE_INTRA_REFRESH_PICTURE_PARTITION:
    case TEST_TYPE_H265_ENCODE_INTRA_REFRESH_ANY_BLOCK_BASED:
    case TEST_TYPE_H265_ENCODE_INTRA_REFRESH_ROW_BASED:
    case TEST_TYPE_H265_ENCODE_INTRA_REFRESH_COLUMN_BASED:
    case TEST_TYPE_H265_ENCODE_INTRA_REFRESH_ANY_BLOCK_BASED_EMPTY_REGION:
    case TEST_TYPE_H265_ENCODE_INTRA_REFRESH_ROW_BASED_EMPTY_REGION:
    case TEST_TYPE_H265_ENCODE_INTRA_REFRESH_COLUMN_BASED_EMPTY_REGION:
    case TEST_TYPE_H265_ENCODE_INTRA_REFRESH_PICTURE_PARTITION_MIDWAY:
    case TEST_TYPE_H265_ENCODE_INTRA_REFRESH_ANY_BLOCK_BASED_MIDWAY:
    case TEST_TYPE_H265_ENCODE_INTRA_REFRESH_ROW_BASED_MIDWAY:
    case TEST_TYPE_H265_ENCODE_INTRA_REFRESH_COLUMN_BASED_MIDWAY:
        return TEST_CODEC_H265;
    default:
        TCU_THROW(InternalError, "Unknown TestType");
    }
}

enum FrameType
{
    IDR_FRAME,
    I_FRAME,
    P_FRAME,
    B_FRAME
};

enum QuantizationMap
{
    QM_DELTA,
    QM_EMPHASIS
};

enum Option : uint32_t
{
    // The default is to do nothing additional to ordinary encode.
    Default = 0,
    UseStatusQueries =
        1
        << 0, // All encode operations will have their status checked for success (Q2 2023: not all vendors support these)
    UseVariableBitrateControl    = 1 << 1,
    UseConstantBitrateControl    = 1 << 2,
    SwapOrder                    = 1 << 3,
    DisableRateControl           = 1 << 4, // const QP
    ResolutionChange             = 1 << 5,
    UseQualityLevel              = 1 << 6,
    UseEncodeUsage               = 1 << 7,
    UseInlineQueries             = 1 << 8,  // Inline queries from the video_mainteance1 extension.
    ResourcesWithoutProfiles     = 1 << 9,  // Test profile-less resources from the video_mainteance1 extension.
    UseDeltaMap                  = 1 << 10, // VK_KHR_video_encode_quantization_map
    UseEmphasisMap               = 1 << 11, // VK_KHR_video_encode_quantization_map
    IntraRefreshPicturePartition = 1 << 12, // Per picture partition intra refresh mode
    IntraRefreshBlockBased       = 1 << 13, // Block-based intra refresh mode
    IntraRefreshBlockRow         = 1 << 14, // Block row-based intra refresh mode
    IntraRefreshBlockColumn      = 1 << 15, // Block column-based intra refresh mode
    IntraRefreshEmptyRegion      = 1 << 16, // Empty region intra refresh (uses maxIntraRefreshCycleDuration)
    IntraRefreshMidway           = 1 << 17, // Start new intra refresh cycle mid-way through previous one
};

#define INTRA_REFRESH_ENCODE_TEST_PATTERN(testType, clipName, option)                                         \
    {                                                                                                         \
        testType, clipName, 1, {IDR_FRAME, P_FRAME, P_FRAME, P_FRAME, P_FRAME, P_FRAME, P_FRAME, P_FRAME,     \
                                P_FRAME,   P_FRAME, P_FRAME, P_FRAME, P_FRAME, P_FRAME, P_FRAME, P_FRAME},    \
            {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15},                                           \
            {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15}, 4, {1, 0},                                \
            {refs(0, 0), refs(1, 0), refs(1, 0), refs(1, 0), refs(1, 0), refs(1, 0), refs(1, 0), refs(1, 0),  \
             refs(1, 0), refs(1, 0), refs(1, 0), refs(1, 0), refs(1, 0), refs(1, 0), refs(1, 0), refs(1, 0)}, \
            {{}, {0}, {1}, {2}, {3}, {4}, {5}, {6}, {7}, {8}, {9}, {10}, {11}, {12}, {13}, {14}},             \
            {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15},                                           \
            {refs<std::vector<uint8_t>>({}, {}),   refs<std::vector<uint8_t>>({0}, {}),                       \
             refs<std::vector<uint8_t>>({1}, {}),  refs<std::vector<uint8_t>>({2}, {}),                       \
             refs<std::vector<uint8_t>>({3}, {}),  refs<std::vector<uint8_t>>({4}, {}),                       \
             refs<std::vector<uint8_t>>({5}, {}),  refs<std::vector<uint8_t>>({6}, {}),                       \
             refs<std::vector<uint8_t>>({7}, {}),  refs<std::vector<uint8_t>>({8}, {}),                       \
             refs<std::vector<uint8_t>>({9}, {}),  refs<std::vector<uint8_t>>({10}, {}),                      \
             refs<std::vector<uint8_t>>({11}, {}), refs<std::vector<uint8_t>>({12}, {}),                      \
             refs<std::vector<uint8_t>>({13}, {}), refs<std::vector<uint8_t>>({14}, {})},                     \
            static_cast<Option>(option)                                                                       \
    }

#define INTRA_REFRESH_MIDWAY_TEST_PATTERN(testType, clipName, option)                             \
    {                                                                                             \
        testType, clipName, 1, {IDR_FRAME, P_FRAME, P_FRAME, P_FRAME, P_FRAME, P_FRAME, P_FRAME}, \
            {0, 1, 2, 3, 4, 5, 6}, {0, 1, 2, 3, 4, 5, 6}, 2, {1, 0},                              \
            {refs(0, 0), refs(1, 0), refs(1, 0), refs(1, 0), refs(1, 0), refs(1, 0), refs(1, 0)}, \
            {{}, {0}, {1}, {0}, {1}, {0}, {1}}, {0, 1, 0, 1, 0, 1, 0},                            \
            {refs<std::vector<uint8_t>>({}, {}),  refs<std::vector<uint8_t>>({0}, {}),            \
             refs<std::vector<uint8_t>>({1}, {}), refs<std::vector<uint8_t>>({0}, {}),            \
             refs<std::vector<uint8_t>>({1}, {}), refs<std::vector<uint8_t>>({0}, {}),            \
             refs<std::vector<uint8_t>>({1}, {})},                                                \
            static_cast<Option>(option | Option::IntraRefreshMidway)                              \
    }

struct EncodeTestParam
{
    TestType type;
    ClipName clip;
    uint32_t gops;
    std::vector<FrameType> encodePattern;
    std::vector<uint32_t> frameIdx;
    std::vector<uint32_t> FrameNum;
    uint8_t spsMaxRefFrames;                                   // Sequence parameter set maximum reference frames.
    std::tuple<uint8_t, uint8_t> ppsNumActiveRefs;             // Picture parameter set number of active references
    std::vector<std::tuple<uint8_t, uint8_t>> shNumActiveRefs; // Slice header number of active references,
    std::vector<std::vector<uint8_t>> refSlots;                // index of dpbImageVideoReferenceSlots
    std::vector<int8_t> curSlot;                               // index of dpbImageVideoReferenceSlots
    std::vector<std::tuple<std::vector<uint8_t>, std::vector<uint8_t>>>
        frameReferences; // index of dpbImageVideoReferenceSlots
    Option encoderOptions;
} g_EncodeTests[] = {
    {TEST_TYPE_H264_ENCODE_I,
     CLIP_H264_ENC_E,
     1,
     {IDR_FRAME},
     /* frameIdx */ {0},
     /* FrameNum */ {0},
     /* spsMaxRefFrames */ 1,
     /* ppsNumActiveRefs */ {0, 0},
     /* shNumActiveRefs */ {refs(0, 0)},
     /* refSlots */ {{}},
     /* curSlot */ {0},
     /* frameReferences */ {refs<std::vector<uint8_t>>({}, {})},
     /* encoderOptions */ Option::Default},
    {TEST_TYPE_H264_ENCODE_RC_VBR,
     CLIP_H264_ENC_E,
     1,
     {IDR_FRAME},
     /* frameIdx */ {0, 1},
     /* FrameNum */ {0, 1},
     /* spsMaxRefFrames */ 2,
     /* ppsNumActiveRefs */ {0, 0},
     /* shNumActiveRefs */ {refs(0, 0), refs(1, 0)},
     /* refSlots */ {{}, {0}},
     /* curSlot */ {0, 1},
     /* frameReferences */ {refs<std::vector<uint8_t>>({}, {}), refs<std::vector<uint8_t>>({0}, {})},
     /* encoderOptions */ Option::UseVariableBitrateControl},
    {TEST_TYPE_H264_ENCODE_RC_CBR,
     CLIP_H264_ENC_E,
     1,
     {IDR_FRAME},
     /* frameIdx */ {0},
     /* FrameNum */ {0},
     /* spsMaxRefFrames */ 1,
     /* ppsNumActiveRefs */ {0, 0},
     /* shNumActiveRefs */ {refs(0, 0)},
     /* refSlots */ {{}},
     /* curSlot */ {0},
     /* frameReferences */ {refs<std::vector<uint8_t>>({}, {})},
     /* encoderOptions */ Option::UseConstantBitrateControl},
    {TEST_TYPE_H264_ENCODE_RC_DISABLE,
     CLIP_H264_ENC_E,
     1,
     {IDR_FRAME, P_FRAME},
     /* frameIdx */ {0, 1},
     /* FrameNum */ {0, 1},
     /* spsMaxRefFrames */ 2,
     /* ppsNumActiveRefs */ {0, 0},
     /* shNumActiveRefs */ {refs(0, 0), refs(1, 0)},
     /* refSlots */ {{}, {0}},
     /* curSlot */ {0, 1},
     /* frameReferences */ {refs<std::vector<uint8_t>>({}, {}), refs<std::vector<uint8_t>>({0}, {})},
     /* encoderOptions */ Option::DisableRateControl},
    {TEST_TYPE_H264_ENCODE_QUALITY_LEVEL,
     CLIP_H264_ENC_E,
     1,
     {IDR_FRAME},
     /* frameIdx */ {0},
     /* FrameNum */ {0},
     /* spsMaxRefFrames */ 1,
     /* ppsNumActiveRefs */ {0, 0},
     /* shNumActiveRefs */ {refs(0, 0)},
     /* refSlots */ {{}},
     /* curSlot */ {0},
     /* frameReferences */ {refs<std::vector<uint8_t>>({}, {})},
     /* encoderOptions */ Option::UseQualityLevel},
    {TEST_TYPE_H264_ENCODE_QM_DELTA_RC_VBR,
     CLIP_H264_ENC_E,
     3,
     {IDR_FRAME},
     /* frameIdx */ {0},
     /* FrameNum */ {0},
     /* spsMaxRefFrames */ 1,
     /* ppsNumActiveRefs */ {0, 0},
     /* shNumActiveRefs */ {refs(0, 0)},
     /* refSlots */ {{}},
     /* curSlot */ {0},
     /* frameReferences */ {refs<std::vector<uint8_t>>({}, {})},
     /* encoderOptions */ static_cast<Option>(Option::UseDeltaMap | Option::UseVariableBitrateControl)},
    {TEST_TYPE_H264_ENCODE_QM_DELTA_RC_CBR,
     CLIP_H264_ENC_E,
     3,
     {IDR_FRAME},
     /* frameIdx */ {0},
     /* FrameNum */ {0},
     /* spsMaxRefFrames */ 1,
     /* ppsNumActiveRefs */ {0, 0},
     /* shNumActiveRefs */ {refs(0, 0)},
     /* refSlots */ {{}},
     /* curSlot */ {0},
     /* frameReferences */ {refs<std::vector<uint8_t>>({}, {})},
     /* encoderOptions */ static_cast<Option>(Option::UseDeltaMap | Option::UseConstantBitrateControl)},
    {TEST_TYPE_H264_ENCODE_QM_DELTA_RC_DISABLE,
     CLIP_H264_ENC_E,
     3,
     {IDR_FRAME},
     /* frameIdx */ {0},
     /* FrameNum */ {0},
     /* spsMaxRefFrames */ 1,
     /* ppsNumActiveRefs */ {0, 0},
     /* shNumActiveRefs */ {refs(0, 0)},
     /* refSlots */ {{}},
     /* curSlot */ {0},
     /* frameReferences */ {refs<std::vector<uint8_t>>({}, {})},
     /* encoderOptions */ static_cast<Option>(Option::UseDeltaMap | Option::DisableRateControl)},
    {TEST_TYPE_H264_ENCODE_QM_DELTA,
     CLIP_H264_ENC_E,
     3,
     {IDR_FRAME},
     /* frameIdx */ {0},
     /* FrameNum */ {0},
     /* spsMaxRefFrames */ 1,
     /* ppsNumActiveRefs */ {0, 0},
     /* shNumActiveRefs */ {refs(0, 0)},
     /* refSlots */ {{}},
     /* curSlot */ {0},
     /* frameReferences */ {refs<std::vector<uint8_t>>({}, {})},
     /* encoderOptions */ Option::UseDeltaMap},
    {TEST_TYPE_H264_ENCODE_QM_EMPHASIS_CBR,
     CLIP_H264_ENC_E,
     2,
     {IDR_FRAME},
     /* frameIdx */ {0},
     /* FrameNum */ {0},
     /* spsMaxRefFrames */ 1,
     /* ppsNumActiveRefs */ {0, 0},
     /* shNumActiveRefs */ {refs(0, 0)},
     /* refSlots */ {{}},
     /* curSlot */ {0},
     /* frameReferences */ {refs<std::vector<uint8_t>>({}, {})},
     /* encoderOptions */ static_cast<Option>(Option::UseEmphasisMap | Option::UseConstantBitrateControl)},
    {TEST_TYPE_H264_ENCODE_QM_EMPHASIS_VBR,
     CLIP_H264_ENC_E,
     2,
     {IDR_FRAME},
     /* frameIdx */ {0},
     /* FrameNum */ {0},
     /* spsMaxRefFrames */ 1,
     /* ppsNumActiveRefs */ {0, 0},
     /* shNumActiveRefs */ {refs(0, 0)},
     /* refSlots */ {{}},
     /* curSlot */ {0},
     /* frameReferences */ {refs<std::vector<uint8_t>>({}, {})},
     /* encoderOptions */ static_cast<Option>(Option::UseEmphasisMap | Option::UseVariableBitrateControl)},
    {TEST_TYPE_H264_ENCODE_USAGE,
     CLIP_H264_ENC_E,
     1,
     {IDR_FRAME},
     /* frameIdx */ {0},
     /* FrameNum */ {0},
     /* spsMaxRefFrames */ 1,
     /* ppsNumActiveRefs */ {0, 0},
     /* shNumActiveRefs */ {refs(0, 0)},
     /* refSlots */ {{}},
     /* curSlot */ {0},
     /* frameReferences */ {refs<std::vector<uint8_t>>({}, {})},
     /* encoderOptions */ Option::UseEncodeUsage},
    {TEST_TYPE_H264_ENCODE_I_P,
     CLIP_H264_ENC_E,
     1,
     {IDR_FRAME, P_FRAME},
     /* frameIdx */ {0, 1},
     /* FrameNum */ {0, 1},
     /* spsMaxRefFrames */ 2,
     /* ppsNumActiveRefs */ {0, 0},
     /* shNumActiveRefs */ {refs(0, 0), refs(1, 0)},
     /* refSlots */ {{}, {0}},
     /* curSlot */ {0, 1},
     /* frameReferences */ {refs<std::vector<uint8_t>>({}, {}), refs<std::vector<uint8_t>>({0}, {})},
     /* encoderOptions */ Option::Default},
    {TEST_TYPE_H264_ENCODE_I_P_NOT_MATCHING_ORDER,
     CLIP_H264_ENC_E,
     1,
     {IDR_FRAME, P_FRAME},
     /* frameIdx */ {0, 1},
     /* FrameNum */ {0, 1},
     /* spsMaxRefFrames */ 2,
     /* ppsNumActiveRefs */ {0, 0},
     /* shNumActiveRefs */ {refs(0, 0), refs(1, 0)},
     /* refSlots */ {{}, {0}},
     /* curSlot */ {0, 1},
     /* frameReferences */ {refs<std::vector<uint8_t>>({}, {}), refs<std::vector<uint8_t>>({0}, {})},
     /* encoderOptions */ Option::SwapOrder},
    {TEST_TYPE_H264_ENCODE_QUERY_RESULT_WITH_STATUS,
     CLIP_H264_ENC_E,
     1,
     {IDR_FRAME, P_FRAME},
     /* frameIdx */ {0, 1},
     /* FrameNum */ {0, 1},
     /* spsMaxRefFrames */ 2,
     /* ppsNumActiveRefs */ {0, 0},
     /* shNumActiveRefs */ {refs(0, 0), refs(1, 0)},
     /* refSlots */ {{}, {0}},
     /* curSlot */ {0, 1},
     /* frameReferences */ {refs<std::vector<uint8_t>>({}, {}), refs<std::vector<uint8_t>>({0}, {})},
     /* encoderOptions */ Option::UseStatusQueries},
    {TEST_TYPE_H264_ENCODE_INLINE_QUERY,
     CLIP_H264_ENC_E,
     1,
     {IDR_FRAME},
     /* frameIdx */ {0},
     /* FrameNum */ {0},
     /* spsMaxRefFrames */ 1,
     /* ppsNumActiveRefs */ {0, 0},
     /* shNumActiveRefs */ {refs(0, 0)},
     /* refSlots */ {{}},
     /* curSlot */ {0},
     /* frameReferences */ {refs<std::vector<uint8_t>>({}, {})},
     /* encoderOptions */ Option::UseInlineQueries},
    {TEST_TYPE_H264_ENCODE_RESOURCES_WITHOUT_PROFILES,
     CLIP_H264_ENC_E,
     1,
     {IDR_FRAME, P_FRAME},
     /* frameIdx */ {0, 1},
     /* FrameNum */ {0, 1},
     /* spsMaxRefFrames */ 2,
     /* ppsNumActiveRefs */ {0, 0},
     /* shNumActiveRefs */ {refs(0, 0), refs(1, 0)},
     /* refSlots */ {{}, {0}},
     /* curSlot */ {0, 1},
     /* frameReferences */ {refs<std::vector<uint8_t>>({}, {}), refs<std::vector<uint8_t>>({0}, {})},
     /* encoderOptions */ Option::ResourcesWithoutProfiles},
    {TEST_TYPE_H264_ENCODE_RESOLUTION_CHANGE_DPB,
     CLIP_H264_ENC_G,
     2,
     {IDR_FRAME, P_FRAME},
     /* frameIdx */ {0, 1},
     /* FrameNum */ {0, 1},
     /* spsMaxRefFrames */ 2,
     /* ppsNumActiveRefs */ {0, 0},
     /* shNumActiveRefs */ {refs(0, 0), refs(1, 0)},
     /* refSlots */ {{}, {0}},
     /* curSlot */ {0, 1},
     /* frameReferences */ {refs<std::vector<uint8_t>>({}, {}), refs<std::vector<uint8_t>>({0}, {})},
     /* encoderOptions */ Option::ResolutionChange},
    {TEST_TYPE_H264_I_P_B_13,
     CLIP_H264_ENC_E,
     2,
     {IDR_FRAME, P_FRAME, B_FRAME, B_FRAME, P_FRAME, B_FRAME, B_FRAME, P_FRAME, B_FRAME, B_FRAME, P_FRAME, B_FRAME,
      B_FRAME, P_FRAME},
     /* frameIdx */ {0, 3, 1, 2, 6, 4, 5, 9, 7, 8, 12, 10, 11, 13},
     /* frameNum */ {0, 1, 2, 2, 2, 3, 3, 3, 4, 4, 4, 5, 5, 5},
     /* spsMaxRefFrames */ 4,
     /* ppsNumActiveRefs */ {2, 2},
     /* shNumActiveRefs */
     {refs(0, 0), refs(1, 0), refs(2, 2), refs(2, 2), refs(2, 0), refs(2, 2), refs(2, 2), refs(2, 0), refs(2, 2),
      refs(2, 2), refs(2, 0), refs(2, 2), refs(2, 2), refs(2, 0)},
     /* refSlots */
     {{},
      {0},
      {0, 1},
      {0, 1},
      {0, 1},
      {0, 1, 2},
      {0, 1, 2},
      {0, 1, 2},
      {0, 1, 2, 3},
      {0, 1, 2, 3},
      {0, 1, 2, 3},
      {1, 2, 3, 4},
      {1, 2, 3, 4},
      {1, 2, 3, 4}},
     /* curSlot */ {0, 1, 2, 2, 2, 3, 3, 3, 4, 4, 4, 5, 5, 5},
     /* frameReferences */
     {refs<std::vector<uint8_t>>({}, {}), refs<std::vector<uint8_t>>({0}, {}),
      refs<std::vector<uint8_t>>({0, 1}, {1, 0}), refs<std::vector<uint8_t>>({0, 1}, {1, 0}),
      refs<std::vector<uint8_t>>({1, 0}, {}), refs<std::vector<uint8_t>>({1, 0}, {2, 1}),
      refs<std::vector<uint8_t>>({1, 0}, {2, 1}), refs<std::vector<uint8_t>>({2, 1}, {}),
      refs<std::vector<uint8_t>>({2, 1}, {3, 2}), refs<std::vector<uint8_t>>({2, 1}, {3, 2}),
      refs<std::vector<uint8_t>>({3, 2}, {}), refs<std::vector<uint8_t>>({3, 2}, {4, 3}),
      refs<std::vector<uint8_t>>({3, 2}, {4, 3}), refs<std::vector<uint8_t>>({4, 3}, {})},
     /* encoderOptions */ Option::Default},
    INTRA_REFRESH_ENCODE_TEST_PATTERN(TEST_TYPE_H264_ENCODE_INTRA_REFRESH_PICTURE_PARTITION, CLIP_H264_ENC_E,
                                      Option::IntraRefreshPicturePartition),
    INTRA_REFRESH_ENCODE_TEST_PATTERN(TEST_TYPE_H264_ENCODE_INTRA_REFRESH_ANY_BLOCK_BASED, CLIP_H264_ENC_E,
                                      Option::IntraRefreshBlockBased),
    INTRA_REFRESH_ENCODE_TEST_PATTERN(TEST_TYPE_H264_ENCODE_INTRA_REFRESH_ROW_BASED, CLIP_H264_ENC_E,
                                      Option::IntraRefreshBlockRow),
    INTRA_REFRESH_ENCODE_TEST_PATTERN(TEST_TYPE_H264_ENCODE_INTRA_REFRESH_COLUMN_BASED, CLIP_H264_ENC_E,
                                      Option::IntraRefreshBlockColumn),
    {TEST_TYPE_H264_ENCODE_INTRA_REFRESH_ANY_BLOCK_BASED_EMPTY_REGION,
     CLIP_H264_ENC_E,
     1,
     {IDR_FRAME, P_FRAME},
     /* frameIdx */ {0, 1},
     /* FrameNum */ {0, 1},
     /* spsMaxRefFrames */ 2,
     /* ppsNumActiveRefs */ {1, 0},
     /* shNumActiveRefs */ {refs(0, 0), refs(1, 0)},
     /* refSlots */ {{}, {0}},
     /* curSlot */ {0, 1},
     /* frameReferences */ {refs<std::vector<uint8_t>>({}, {}), refs<std::vector<uint8_t>>({0}, {})},
     /* encoderOptions */ static_cast<Option>(Option::IntraRefreshBlockBased | Option::IntraRefreshEmptyRegion)},
    {TEST_TYPE_H264_ENCODE_INTRA_REFRESH_ROW_BASED_EMPTY_REGION,
     CLIP_H264_ENC_E,
     1,
     {IDR_FRAME, P_FRAME},
     /* frameIdx */ {0, 1},
     /* FrameNum */ {0, 1},
     /* spsMaxRefFrames */ 2,
     /* ppsNumActiveRefs */ {1, 0},
     /* shNumActiveRefs */ {refs(0, 0), refs(1, 0)},
     /* refSlots */ {{}, {0}},
     /* curSlot */ {0, 1},
     /* frameReferences */ {refs<std::vector<uint8_t>>({}, {}), refs<std::vector<uint8_t>>({0}, {})},
     /* encoderOptions */ static_cast<Option>(Option::IntraRefreshBlockRow | Option::IntraRefreshEmptyRegion)},
    {TEST_TYPE_H264_ENCODE_INTRA_REFRESH_COLUMN_BASED_EMPTY_REGION,
     CLIP_H264_ENC_E,
     1,
     {IDR_FRAME, P_FRAME},
     /* frameIdx */ {0, 1},
     /* FrameNum */ {0, 1},
     /* spsMaxRefFrames */ 2,
     /* ppsNumActiveRefs */ {1, 0},
     /* shNumActiveRefs */ {refs(0, 0), refs(1, 0)},
     /* refSlots */ {{}, {0}},
     /* curSlot */ {0, 1},
     /* frameReferences */ {refs<std::vector<uint8_t>>({}, {}), refs<std::vector<uint8_t>>({0}, {})},
     /* encoderOptions */ static_cast<Option>(Option::IntraRefreshBlockColumn | Option::IntraRefreshEmptyRegion)},
    {TEST_TYPE_H265_ENCODE_I,
     CLIP_H265_ENC_F,
     1,
     {IDR_FRAME},
     /* frameIdx */ {0},
     /* FrameNum */ {0},
     /* spsMaxRefFrames */ 1,
     /* ppsNumActiveRefs */ {0, 0},
     /* shNumActiveRefs */ {refs(0, 0)},
     /* refSlots */ {{}},
     /* curSlot */ {0},
     /* frameReferences */ {refs<std::vector<uint8_t>>({}, {})},
     /* encoderOptions */ Option::Default},
    {TEST_TYPE_H265_ENCODE_RC_VBR,
     CLIP_H265_ENC_F,
     1,
     {IDR_FRAME},
     /* frameIdx */ {0, 1},
     /* FrameNum */ {0, 1},
     /* spsMaxRefFrames */ 2,
     /* ppsNumActiveRefs */ {0, 0},
     /* shNumActiveRefs */ {refs(0, 0), refs(1, 0)},
     /* refSlots */ {{}, {0}},
     /* curSlot */ {0, 1},
     /* frameReferences */ {refs<std::vector<uint8_t>>({}, {}), refs<std::vector<uint8_t>>({0}, {})},
     /* encoderOptions */ Option::UseVariableBitrateControl},
    {TEST_TYPE_H265_ENCODE_RC_CBR,
     CLIP_H265_ENC_F,
     1,
     {IDR_FRAME},
     /* frameIdx */ {0},
     /* FrameNum */ {0},
     /* spsMaxRefFrames */ 1,
     /* ppsNumActiveRefs */ {0, 0},
     /* shNumActiveRefs */ {refs(0, 0)},
     /* refSlots */ {{}},
     /* curSlot */ {0},
     /* frameReferences */ {refs<std::vector<uint8_t>>({}, {})},
     /* encoderOptions */ Option::UseConstantBitrateControl},
    {TEST_TYPE_H265_ENCODE_RC_DISABLE,
     CLIP_H265_ENC_F,
     1,
     {IDR_FRAME, P_FRAME},
     /* frameIdx */ {0, 1},
     /* FrameNum */ {0, 1},
     /* spsMaxRefFrames */ 2,
     /* ppsNumActiveRefs */ {0, 0},
     /* shNumActiveRefs */ {refs(0, 0), refs(1, 0)},
     /* refSlots */ {{}, {0}},
     /* curSlot */ {0, 1},
     /* frameReferences */ {refs<std::vector<uint8_t>>({}, {}), refs<std::vector<uint8_t>>({0}, {})},
     /* encoderOptions */ Option::DisableRateControl},
    {TEST_TYPE_H265_ENCODE_QUALITY_LEVEL,
     CLIP_H265_ENC_F,
     1,
     {IDR_FRAME},
     /* frameIdx */ {0},
     /* FrameNum */ {0},
     /* spsMaxRefFrames */ 1,
     /* ppsNumActiveRefs */ {0, 0},
     /* shNumActiveRefs */ {refs(0, 0)},
     /* refSlots */ {{}},
     /* curSlot */ {0},
     /* frameReferences */ {refs<std::vector<uint8_t>>({}, {})},
     /* encoderOptions */ Option::UseQualityLevel},
    {TEST_TYPE_H265_ENCODE_QM_DELTA_RC_VBR,
     CLIP_H265_ENC_F,
     3,
     {IDR_FRAME},
     /* frameIdx */ {0},
     /* FrameNum */ {0},
     /* spsMaxRefFrames */ 1,
     /* ppsNumActiveRefs */ {0, 0},
     /* shNumActiveRefs */ {refs(0, 0)},
     /* refSlots */ {{}},
     /* curSlot */ {0},
     /* frameReferences */ {refs<std::vector<uint8_t>>({}, {})},
     /* encoderOptions */ static_cast<Option>(Option::UseDeltaMap | Option::UseVariableBitrateControl)},
    {TEST_TYPE_H265_ENCODE_QM_DELTA_RC_CBR,
     CLIP_H265_ENC_F,
     3,
     {IDR_FRAME},
     /* frameIdx */ {0},
     /* FrameNum */ {0},
     /* spsMaxRefFrames */ 1,
     /* ppsNumActiveRefs */ {0, 0},
     /* shNumActiveRefs */ {refs(0, 0)},
     /* refSlots */ {{}},
     /* curSlot */ {0},
     /* frameReferences */ {refs<std::vector<uint8_t>>({}, {})},
     /* encoderOptions */ static_cast<Option>(Option::UseDeltaMap | Option::UseConstantBitrateControl)},
    {TEST_TYPE_H265_ENCODE_QM_DELTA_RC_DISABLE,
     CLIP_H265_ENC_F,
     3,
     {IDR_FRAME},
     /* frameIdx */ {0},
     /* FrameNum */ {0},
     /* spsMaxRefFrames */ 1,
     /* ppsNumActiveRefs */ {0, 0},
     /* shNumActiveRefs */ {refs(0, 0)},
     /* refSlots */ {{}},
     /* curSlot */ {0},
     /* frameReferences */ {refs<std::vector<uint8_t>>({}, {})},
     /* encoderOptions */ static_cast<Option>(Option::UseDeltaMap | Option::DisableRateControl)},
    {TEST_TYPE_H265_ENCODE_QM_DELTA,
     CLIP_H265_ENC_F,
     3,
     {IDR_FRAME},
     /* frameIdx */ {0},
     /* FrameNum */ {0},
     /* spsMaxRefFrames */ 1,
     /* ppsNumActiveRefs */ {0, 0},
     /* shNumActiveRefs */ {refs(0, 0)},
     /* refSlots */ {{}},
     /* curSlot */ {0},
     /* frameReferences */ {refs<std::vector<uint8_t>>({}, {})},
     /* encoderOptions */ Option::UseDeltaMap},
    {TEST_TYPE_H265_ENCODE_QM_EMPHASIS_CBR,
     CLIP_H265_ENC_F,
     2,
     {IDR_FRAME},
     /* frameIdx */ {0},
     /* FrameNum */ {0},
     /* spsMaxRefFrames */ 1,
     /* ppsNumActiveRefs */ {0, 0},
     /* shNumActiveRefs */ {refs(0, 0)},
     /* refSlots */ {{}},
     /* curSlot */ {0},
     /* frameReferences */ {refs<std::vector<uint8_t>>({}, {})},
     /* encoderOptions */ static_cast<Option>(Option::UseEmphasisMap | Option::UseConstantBitrateControl)},
    {TEST_TYPE_H265_ENCODE_QM_EMPHASIS_VBR,
     CLIP_H265_ENC_F,
     2,
     {IDR_FRAME},
     /* frameIdx */ {0},
     /* FrameNum */ {0},
     /* spsMaxRefFrames */ 1,
     /* ppsNumActiveRefs */ {0, 0},
     /* shNumActiveRefs */ {refs(0, 0)},
     /* refSlots */ {{}},
     /* curSlot */ {0},
     /* frameReferences */ {refs<std::vector<uint8_t>>({}, {})},
     /* encoderOptions */ static_cast<Option>(Option::UseEmphasisMap | Option::UseVariableBitrateControl)},
    {TEST_TYPE_H265_ENCODE_USAGE,
     CLIP_H265_ENC_F,
     1,
     {IDR_FRAME},
     /* frameIdx */ {0},
     /* FrameNum */ {0},
     /* spsMaxRefFrames */ 1,
     /* ppsNumActiveRefs */ {0, 0},
     /* shNumActiveRefs */ {refs(0, 0)},
     /* refSlots */ {{}},
     /* curSlot */ {0},
     /* frameReferences */ {refs<std::vector<uint8_t>>({}, {})},
     /* encoderOptions */ Option::UseEncodeUsage},
    {TEST_TYPE_H265_ENCODE_I_P,
     CLIP_H265_ENC_F,
     1,
     {IDR_FRAME, P_FRAME},
     /* frameIdx */ {0, 1},
     /* FrameNum */ {0, 1},
     /* spsMaxRefFrames */ 2,
     /* ppsNumActiveRefs */ {0, 0},
     /* shNumActiveRefs */ {refs(0, 0), refs(1, 0)},
     /* refSlots */ {{}, {0}},
     /* curSlot */ {0, 1},
     /* frameReferences */ {refs<std::vector<uint8_t>>({}, {}), refs<std::vector<uint8_t>>({0}, {})},
     /* encoderOptions */ Option::Default},
    {TEST_TYPE_H265_ENCODE_I_P_NOT_MATCHING_ORDER,
     CLIP_H265_ENC_F,
     1,
     {IDR_FRAME, P_FRAME},
     /* frameIdx */ {0, 1},
     /* FrameNum */ {0, 1},
     /* spsMaxRefFrames */ 2,
     /* ppsNumActiveRefs */ {0, 0},
     /* shNumActiveRefs */ {refs(0, 0), refs(1, 0)},
     /* refSlots */ {{}, {0}},
     /* curSlot */ {0, 1},
     /* frameReferences */ {refs<std::vector<uint8_t>>({}, {}), refs<std::vector<uint8_t>>({0}, {})},
     /* encoderOptions */ Option::SwapOrder},
    {TEST_TYPE_H265_ENCODE_QUERY_RESULT_WITH_STATUS,
     CLIP_H265_ENC_F,
     1,
     {IDR_FRAME, P_FRAME},
     /* frameIdx */ {0, 1},
     /* FrameNum */ {0, 1},
     /* spsMaxRefFrames */ 2,
     /* ppsNumActiveRefs */ {0, 0},
     /* shNumActiveRefs */ {refs(0, 0), refs(1, 0)},
     /* refSlots */ {{}, {0}},
     /* curSlot */ {0, 1},
     /* frameReferences */ {refs<std::vector<uint8_t>>({}, {}), refs<std::vector<uint8_t>>({0}, {})},
     /* encoderOptions */ Option::UseStatusQueries},
    {TEST_TYPE_H265_ENCODE_INLINE_QUERY,
     CLIP_H265_ENC_F,
     1,
     {IDR_FRAME},
     /* frameIdx */ {0},
     /* FrameNum */ {0},
     /* spsMaxRefFrames */ 1,
     /* ppsNumActiveRefs */ {0, 0},
     /* shNumActiveRefs */ {refs(0, 0)},
     /* refSlots */ {{}},
     /* curSlot */ {0},
     /* frameReferences */ {refs<std::vector<uint8_t>>({}, {})},
     /* encoderOptions */ Option::UseInlineQueries},
    {TEST_TYPE_H265_ENCODE_RESOURCES_WITHOUT_PROFILES,
     CLIP_H265_ENC_F,
     1,
     {IDR_FRAME, P_FRAME},
     /* frameIdx */ {0, 1},
     /* FrameNum */ {0, 1},
     /* spsMaxRefFrames */ 2,
     /* ppsNumActiveRefs */ {0, 0},
     /* shNumActiveRefs */ {refs(0, 0), refs(1, 0)},
     /* refSlots */ {{}, {0}},
     /* curSlot */ {0, 1},
     /* frameReferences */ {refs<std::vector<uint8_t>>({}, {}), refs<std::vector<uint8_t>>({0}, {})},
     /* encoderOptions */ Option::ResourcesWithoutProfiles},
    {TEST_TYPE_H265_ENCODE_RESOLUTION_CHANGE_DPB,
     CLIP_H265_ENC_H,
     2,
     {IDR_FRAME, P_FRAME},
     /* frameIdx */ {0, 1},
     /* FrameNum */ {0, 1},
     /* spsMaxRefFrames */ 2,
     /* ppsNumActiveRefs */ {0, 0},
     /* shNumActiveRefs */ {refs(0, 0), refs(1, 0)},
     /* refSlots */ {{}, {0}},
     /* curSlot */ {0, 1},
     /* frameReferences */ {refs<std::vector<uint8_t>>({}, {}), refs<std::vector<uint8_t>>({0}, {})},
     /* encoderOptions */ Option::ResolutionChange},
    {TEST_TYPE_H265_I_P_B_13,
     CLIP_H265_ENC_F,
     2,
     {IDR_FRAME, P_FRAME, B_FRAME, B_FRAME, P_FRAME, B_FRAME, B_FRAME, P_FRAME, B_FRAME, B_FRAME, P_FRAME, B_FRAME,
      B_FRAME, P_FRAME},
     /* frameIdx */ {0, 3, 1, 2, 6, 4, 5, 9, 7, 8, 12, 10, 11, 13},
     /* frameNum */ {0, 1, 2, 2, 2, 3, 3, 3, 4, 4, 4, 5, 5, 5},
     /* spsMaxRefFrames */ 2,
     /* ppsNumActiveRefs */ {1, 1},
     /* shNumActiveRefs */
     {refs(0, 0), refs(1, 0), refs(1, 1), refs(1, 1), refs(1, 0), refs(1, 1), refs(1, 1), refs(1, 0), refs(1, 1),
      refs(1, 1), refs(1, 0), refs(1, 1), refs(1, 1), refs(1, 0)},
     /* refSlots */
     {{},
      {0},
      {0, 1},
      {0, 1},
      {0, 1},
      {0, 1, 2},
      {0, 1, 2},
      {0, 1, 2},
      {0, 1, 2, 3},
      {0, 1, 2, 3},
      {0, 1, 2, 3},
      {1, 2, 3, 4},
      {1, 2, 3, 4},
      {1, 2, 3, 4}},
     /* curSlot */ {0, 1, 2, 2, 2, 3, 3, 3, 4, 4, 4, 5, 5, 5},
     /* frameReferences */
     {refs<std::vector<uint8_t>>({}, {}), refs<std::vector<uint8_t>>({0}, {}),
      refs<std::vector<uint8_t>>({0, 1}, {1, 0}), refs<std::vector<uint8_t>>({0, 1}, {1, 0}),
      refs<std::vector<uint8_t>>({1, 0}, {}), refs<std::vector<uint8_t>>({1, 0}, {2, 1}),
      refs<std::vector<uint8_t>>({1, 0}, {2, 1}), refs<std::vector<uint8_t>>({2, 1}, {}),
      refs<std::vector<uint8_t>>({2, 1}, {3, 2}), refs<std::vector<uint8_t>>({2, 1}, {3, 2}),
      refs<std::vector<uint8_t>>({3, 2}, {}), refs<std::vector<uint8_t>>({3, 2}, {4, 3}),
      refs<std::vector<uint8_t>>({3, 2}, {4, 3}), refs<std::vector<uint8_t>>({4, 3}, {})},
     /* encoderOptions */ Option::Default},
    INTRA_REFRESH_ENCODE_TEST_PATTERN(TEST_TYPE_H265_ENCODE_INTRA_REFRESH_PICTURE_PARTITION, CLIP_H265_ENC_F,
                                      Option::IntraRefreshPicturePartition),
    INTRA_REFRESH_ENCODE_TEST_PATTERN(TEST_TYPE_H265_ENCODE_INTRA_REFRESH_ANY_BLOCK_BASED, CLIP_H265_ENC_F,
                                      Option::IntraRefreshBlockBased),
    INTRA_REFRESH_ENCODE_TEST_PATTERN(TEST_TYPE_H265_ENCODE_INTRA_REFRESH_ROW_BASED, CLIP_H265_ENC_F,
                                      Option::IntraRefreshBlockRow),
    INTRA_REFRESH_ENCODE_TEST_PATTERN(TEST_TYPE_H265_ENCODE_INTRA_REFRESH_COLUMN_BASED, CLIP_H265_ENC_F,
                                      Option::IntraRefreshBlockColumn),
    {TEST_TYPE_H265_ENCODE_INTRA_REFRESH_ANY_BLOCK_BASED_EMPTY_REGION,
     CLIP_H265_ENC_F,
     1,
     {IDR_FRAME, P_FRAME},
     /* frameIdx */ {0, 1},
     /* FrameNum */ {0, 1},
     /* spsMaxRefFrames */ 2,
     /* ppsNumActiveRefs */ {1, 0},
     /* shNumActiveRefs */ {refs(0, 0), refs(1, 0)},
     /* refSlots */ {{}, {0}},
     /* curSlot */ {0, 1},
     /* frameReferences */ {refs<std::vector<uint8_t>>({}, {}), refs<std::vector<uint8_t>>({0}, {})},
     /* encoderOptions */ static_cast<Option>(Option::IntraRefreshBlockBased | Option::IntraRefreshEmptyRegion)},
    {TEST_TYPE_H265_ENCODE_INTRA_REFRESH_ROW_BASED_EMPTY_REGION,
     CLIP_H265_ENC_F,
     1,
     {IDR_FRAME, P_FRAME},
     /* frameIdx */ {0, 1},
     /* FrameNum */ {0, 1},
     /* spsMaxRefFrames */ 2,
     /* ppsNumActiveRefs */ {1, 0},
     /* shNumActiveRefs */ {refs(0, 0), refs(1, 0)},
     /* refSlots */ {{}, {0}},
     /* curSlot */ {0, 1},
     /* frameReferences */ {refs<std::vector<uint8_t>>({}, {}), refs<std::vector<uint8_t>>({0}, {})},
     /* encoderOptions */ static_cast<Option>(Option::IntraRefreshBlockRow | Option::IntraRefreshEmptyRegion)},
    {TEST_TYPE_H265_ENCODE_INTRA_REFRESH_COLUMN_BASED_EMPTY_REGION,
     CLIP_H265_ENC_F,
     1,
     {IDR_FRAME, P_FRAME},
     /* frameIdx */ {0, 1},
     /* FrameNum */ {0, 1},
     /* spsMaxRefFrames */ 2,
     /* ppsNumActiveRefs */ {1, 0},
     /* shNumActiveRefs */ {refs(0, 0), refs(1, 0)},
     /* refSlots */ {{}, {0}},
     /* curSlot */ {0, 1},
     /* frameReferences */ {refs<std::vector<uint8_t>>({}, {}), refs<std::vector<uint8_t>>({0}, {})},
     /* encoderOptions */ static_cast<Option>(Option::IntraRefreshBlockColumn | Option::IntraRefreshEmptyRegion)},

    // Mid-way intra refresh tests for H264
    INTRA_REFRESH_MIDWAY_TEST_PATTERN(TEST_TYPE_H264_ENCODE_INTRA_REFRESH_PICTURE_PARTITION_MIDWAY, CLIP_H264_ENC_E,
                                      Option::IntraRefreshPicturePartition),
    INTRA_REFRESH_MIDWAY_TEST_PATTERN(TEST_TYPE_H264_ENCODE_INTRA_REFRESH_ANY_BLOCK_BASED_MIDWAY, CLIP_H264_ENC_E,
                                      Option::IntraRefreshBlockBased),
    INTRA_REFRESH_MIDWAY_TEST_PATTERN(TEST_TYPE_H264_ENCODE_INTRA_REFRESH_ROW_BASED_MIDWAY, CLIP_H264_ENC_E,
                                      Option::IntraRefreshBlockRow),
    INTRA_REFRESH_MIDWAY_TEST_PATTERN(TEST_TYPE_H264_ENCODE_INTRA_REFRESH_COLUMN_BASED_MIDWAY, CLIP_H264_ENC_E,
                                      Option::IntraRefreshBlockColumn),

    // Mid-way intra refresh tests for H265
    INTRA_REFRESH_MIDWAY_TEST_PATTERN(TEST_TYPE_H265_ENCODE_INTRA_REFRESH_PICTURE_PARTITION_MIDWAY, CLIP_H265_ENC_F,
                                      Option::IntraRefreshPicturePartition),
    INTRA_REFRESH_MIDWAY_TEST_PATTERN(TEST_TYPE_H265_ENCODE_INTRA_REFRESH_ANY_BLOCK_BASED_MIDWAY, CLIP_H265_ENC_F,
                                      Option::IntraRefreshBlockBased),
    INTRA_REFRESH_MIDWAY_TEST_PATTERN(TEST_TYPE_H265_ENCODE_INTRA_REFRESH_ROW_BASED_MIDWAY, CLIP_H265_ENC_F,
                                      Option::IntraRefreshBlockRow),
    INTRA_REFRESH_MIDWAY_TEST_PATTERN(TEST_TYPE_H265_ENCODE_INTRA_REFRESH_COLUMN_BASED_MIDWAY, CLIP_H265_ENC_F,
                                      Option::IntraRefreshBlockColumn)};

class TestDefinition
{
public:
    static MovePtr<TestDefinition> create(EncodeTestParam params, bool layeredSrc, bool generalLayout)
    {
        return MovePtr<TestDefinition>(new TestDefinition(params, layeredSrc, generalLayout));
    }

    TestDefinition(EncodeTestParam params, bool layeredSrc, bool generalLayout)
        : m_params(params)
        , m_isLayeredSrc(layeredSrc)
        , m_generalLayout(generalLayout)
        , m_info(clipInfo(params.clip))
    {
        VideoProfileInfo profile = m_info->sessionProfiles[0];
        m_profile = VkVideoCoreProfile(profile.codecOperation, profile.subsamplingFlags, profile.lumaBitDepth,
                                       profile.chromaBitDepth, profile.profileIDC);
    }

    TestType getTestType() const
    {
        return m_params.type;
    }

    bool isLayered() const
    {
        return m_isLayeredSrc;
    }

    bool usesGeneralLayout() const
    {
        return m_generalLayout;
    }

    const char *getClipFilename() const
    {
        return m_info->filename;
    }

    std::string getClipFilePath() const
    {
        std::vector<std::string> resourcePathComponents = {"vulkan", "video", m_info->filename};
        de::FilePath resourcePath                       = de::FilePath::join(resourcePathComponents);
        return resourcePath.getPath();
    }

    uint32_t getClipWidth() const
    {
        return m_info->frameWidth;
    }

    uint32_t getClipHeight() const
    {
        return m_info->frameHeight;
    }

    uint32_t getClipFrameRate() const
    {
        return m_info->frameRate;
    }

    uint32_t getClipTotalFrames() const
    {
        return m_info->totalFrames;
    }

    VkVideoCodecOperationFlagBitsKHR getCodecOperation() const
    {
        return m_profile.GetCodecType();
    }

    void *getDecodeProfileExtension() const
    {
        if (m_profile.IsH264())
        {
            const VkVideoDecodeH264ProfileInfoKHR *videoProfileExtention = m_profile.GetDecodeH264Profile();
            return reinterpret_cast<void *>(const_cast<VkVideoDecodeH264ProfileInfoKHR *>(videoProfileExtention));
        }
        if (m_profile.IsH265())
        {
            const VkVideoDecodeH265ProfileInfoKHR *videoProfileExtention = m_profile.GetDecodeH265Profile();
            return reinterpret_cast<void *>(const_cast<VkVideoDecodeH265ProfileInfoKHR *>(videoProfileExtention));
        }
        TCU_THROW(InternalError, "Unsupported codec");
    }

    void *getEncodeProfileExtension() const
    {
        if (m_profile.IsH264())
        {
            const VkVideoEncodeH264ProfileInfoKHR *videoProfileExtention = m_profile.GetEncodeH264Profile();
            return reinterpret_cast<void *>(const_cast<VkVideoEncodeH264ProfileInfoKHR *>(videoProfileExtention));
        }
        if (m_profile.IsH265())
        {
            const VkVideoEncodeH265ProfileInfoKHR *videoProfileExtention = m_profile.GetEncodeH265Profile();
            return reinterpret_cast<void *>(const_cast<VkVideoEncodeH265ProfileInfoKHR *>(videoProfileExtention));
        }
        TCU_THROW(InternalError, "Unsupported codec");
    }

    const VkVideoCoreProfile *getProfile() const
    {
        return &m_profile;
    }

    uint32_t gopCount() const
    {
        return m_params.gops;
    }

    uint32_t gopFrameCount() const
    {
        return static_cast<uint32_t>(m_params.encodePattern.size());
    }

    int gopReferenceFrameCount() const
    {
        int count = 0;
        for (const auto &frame : m_params.encodePattern)
        {
            if (frame != B_FRAME)
            {
                count++;
            }
        }
        return count;
    }

    int gopCycles() const
    {
        int gopNum = 0;

        for (auto &frame : m_params.encodePattern)
            if (frame == IDR_FRAME || frame == I_FRAME)
                gopNum++;

        DE_ASSERT(gopNum);

        return gopNum;
    }

    bool patternContain(FrameType type) const
    {
        return std::find(m_params.encodePattern.begin(), m_params.encodePattern.end(), type) !=
               m_params.encodePattern.end();
    }

    uint32_t frameIdx(uint32_t Idx) const
    {
        return m_params.frameIdx[Idx];
    }

    FrameType frameType(uint32_t Idx) const
    {
        return m_params.encodePattern[Idx];
    }

    uint8_t maxNumRefs() const
    {
        return m_params.spsMaxRefFrames;
    }

    uint8_t ppsActiveRefs0() const
    {
        return std::get<0>(m_params.ppsNumActiveRefs);
    }

    uint8_t ppsActiveRefs1() const
    {
        return std::get<1>(m_params.ppsNumActiveRefs);
    }

    uint8_t shActiveRefs0(uint32_t Idx) const
    {
        return std::get<0>(m_params.shNumActiveRefs[Idx]);
    }

    uint8_t shActiveRefs1(uint32_t Idx) const
    {
        return std::get<1>(m_params.shNumActiveRefs[Idx]);
    }

    std::vector<uint8_t> ref0(uint32_t Idx) const
    {
        std::tuple<std::vector<uint8_t>, std::vector<uint8_t>> ref = m_params.frameReferences[Idx];
        return std::get<0>(ref);
    }

    std::vector<uint8_t> ref1(uint32_t Idx) const
    {
        std::tuple<std::vector<uint8_t>, std::vector<uint8_t>> ref = m_params.frameReferences[Idx];
        return std::get<1>(ref);
    }

    std::vector<uint8_t> refSlots(uint32_t Idx) const
    {
        std::vector<uint8_t> refs = m_params.refSlots[Idx];
        return refs;
    }

    uint8_t refsCount(uint32_t Idx) const
    {
        return static_cast<uint8_t>(m_params.refSlots[Idx].size());
    }

    int8_t curSlot(uint32_t Idx) const
    {
        return m_params.curSlot[Idx];
    }

    uint32_t frameNumber(uint32_t Idx) const
    {
        return m_params.FrameNum[Idx];
    }

    uint32_t getConsecutiveBFrameCount(void) const
    {
        uint32_t maxConsecutiveBFrameCount     = 0;
        uint32_t currentConsecutiveBFrameCount = 0;

        for (const auto &frame : m_params.encodePattern)
        {
            if (frame == B_FRAME)
            {
                currentConsecutiveBFrameCount++;
            }
            else
            {
                if (currentConsecutiveBFrameCount > maxConsecutiveBFrameCount)
                {
                    maxConsecutiveBFrameCount = currentConsecutiveBFrameCount;
                }
                currentConsecutiveBFrameCount = 0;
            }
        }

        return maxConsecutiveBFrameCount;
    }

    size_t framesToCheck() const
    {
        return m_params.encodePattern.size() * m_params.gops;
    }

    bool hasOption(Option o) const
    {
        return (m_params.encoderOptions & o) != 0;
    }

    VideoDevice::VideoDeviceFlags requiredDeviceFlags() const
    {
        switch (m_profile.GetCodecType())
        {
        case VK_VIDEO_CODEC_OPERATION_ENCODE_H264_BIT_KHR:
        case VK_VIDEO_CODEC_OPERATION_ENCODE_H265_BIT_KHR:
        case VK_VIDEO_CODEC_OPERATION_ENCODE_AV1_BIT_KHR:
        {
            VideoDevice::VideoDeviceFlags flags = VideoDevice::VIDEO_DEVICE_FLAG_REQUIRE_SYNC2_OR_NOT_SUPPORTED;

            if (hasOption(Option::UseStatusQueries))
                flags |= VideoDevice::VIDEO_DEVICE_FLAG_QUERY_WITH_STATUS_FOR_ENCODE_SUPPORT;

            if (hasOption(Option::UseInlineQueries) || hasOption(Option::ResourcesWithoutProfiles))
                flags |= VideoDevice::VIDEO_DEVICE_FLAG_REQUIRE_MAINTENANCE_1;

            if (hasOption(Option::UseDeltaMap) || hasOption(Option::UseEmphasisMap))
                flags |= VideoDevice::VIDEO_DEVICE_FLAG_REQUIRE_QUANTIZATION_MAP;

            if (hasOption(Option::IntraRefreshPicturePartition) || hasOption(Option::IntraRefreshBlockBased) ||
                hasOption(Option::IntraRefreshBlockRow) || hasOption(Option::IntraRefreshBlockColumn))
            {
                flags |= VideoDevice::VIDEO_DEVICE_FLAG_REQUIRE_INTRA_REFRESH;
            }

            return flags;
        }
        default:
            tcu::die("Unsupported video codec %s\n", util::codecToName(m_profile.GetCodecType()));
            break;
        }

        TCU_THROW(InternalError, "Unsupported codec");
    }

    const VkExtensionProperties *extensionProperties() const
    {
        static const VkExtensionProperties h264StdExtensionVersion = {
            VK_STD_VULKAN_VIDEO_CODEC_H264_ENCODE_EXTENSION_NAME, VK_STD_VULKAN_VIDEO_CODEC_H264_ENCODE_SPEC_VERSION};
        static const VkExtensionProperties h265StdExtensionVersion = {
            VK_STD_VULKAN_VIDEO_CODEC_H265_ENCODE_EXTENSION_NAME, VK_STD_VULKAN_VIDEO_CODEC_H265_ENCODE_SPEC_VERSION};

        switch (m_profile.GetCodecType())
        {
        case VK_VIDEO_CODEC_OPERATION_ENCODE_H264_BIT_KHR:
            return &h264StdExtensionVersion;
        case VK_VIDEO_CODEC_OPERATION_ENCODE_H265_BIT_KHR:
            return &h265StdExtensionVersion;
        default:
            tcu::die("Unsupported video codec %s\n", util::codecToName(m_profile.GetCodecType()));
            break;
        }

        TCU_THROW(InternalError, "Unsupported codec");
    };

private:
    EncodeTestParam m_params;
    bool m_isLayeredSrc;
    bool m_generalLayout;
    const ClipInfo *m_info{};
    VkVideoCoreProfile m_profile;
};

struct bytestreamWriteWithStatus
{
    uint32_t bitstreamOffset;
    uint32_t bitstreamWrite;
    VkQueryResultStatusKHR status;
};

bool processQueryPoolResults(const DeviceInterface &vk, const VkDevice device, VkQueryPool encodeQueryPool,
                             uint32_t firstQueryId, uint32_t queryCount, VkDeviceSize &bitstreamBufferOffset,
                             VkDeviceSize &minBitstreamBufferOffsetAlignment, const bool queryStatus)
{
    bytestreamWriteWithStatus queryResultWithStatus;
    deMemset(&queryResultWithStatus, 0xFF, sizeof(queryResultWithStatus));

    if (vk.getQueryPoolResults(device, encodeQueryPool, firstQueryId, queryCount, sizeof(queryResultWithStatus),
                               &queryResultWithStatus, sizeof(queryResultWithStatus),
                               VK_QUERY_RESULT_WITH_STATUS_BIT_KHR | VK_QUERY_RESULT_WAIT_BIT) == VK_SUCCESS)
    {
        bitstreamBufferOffset += queryResultWithStatus.bitstreamWrite;

        // Align buffer offset after adding written data
        bitstreamBufferOffset = deAlign64(bitstreamBufferOffset, minBitstreamBufferOffsetAlignment);

        if (queryStatus && queryResultWithStatus.status != VK_QUERY_RESULT_STATUS_COMPLETE_KHR)
        {
            return false;
        }
    }
    return true;
}

StdVideoH264PictureType getH264PictureType(const FrameType frameType)
{
    switch (frameType)
    {
    case IDR_FRAME:
        return STD_VIDEO_H264_PICTURE_TYPE_IDR;
    case I_FRAME:
        return STD_VIDEO_H264_PICTURE_TYPE_I;
    case P_FRAME:
        return STD_VIDEO_H264_PICTURE_TYPE_P;
    case B_FRAME:
        return STD_VIDEO_H264_PICTURE_TYPE_B;
    default:
        return {};
    }
}

StdVideoH264SliceType getH264SliceType(const FrameType frameType)
{
    switch (frameType)
    {
    case IDR_FRAME:
    case I_FRAME:
        return STD_VIDEO_H264_SLICE_TYPE_I;
    case P_FRAME:
        return STD_VIDEO_H264_SLICE_TYPE_P;
    case B_FRAME:
        return STD_VIDEO_H264_SLICE_TYPE_B;
    default:
        return {};
    }
}

StdVideoH265PictureType getH265PictureType(const FrameType frameType)
{
    switch (frameType)
    {
    case IDR_FRAME:
        return STD_VIDEO_H265_PICTURE_TYPE_IDR;
    case I_FRAME:
        return STD_VIDEO_H265_PICTURE_TYPE_I;
    case P_FRAME:
        return STD_VIDEO_H265_PICTURE_TYPE_P;
    case B_FRAME:
        return STD_VIDEO_H265_PICTURE_TYPE_B;
    default:
        return {};
    }
}

StdVideoH265SliceType getH265SliceType(const FrameType frameType)
{
    switch (frameType)
    {
    case IDR_FRAME:
    case I_FRAME:
        return STD_VIDEO_H265_SLICE_TYPE_I;
    case P_FRAME:
        return STD_VIDEO_H265_SLICE_TYPE_P;
    case B_FRAME:
        return STD_VIDEO_H265_SLICE_TYPE_B;
    default:
        return {};
    }
}

VkVideoCodecOperationFlagBitsKHR getCodecDecodeOperationFromEncode(VkVideoCodecOperationFlagBitsKHR encodeOperation)
{
    switch (encodeOperation)
    {
    case VK_VIDEO_CODEC_OPERATION_ENCODE_H264_BIT_KHR:
        return VK_VIDEO_CODEC_OPERATION_DECODE_H264_BIT_KHR;
    case VK_VIDEO_CODEC_OPERATION_ENCODE_H265_BIT_KHR:
        return VK_VIDEO_CODEC_OPERATION_DECODE_H265_BIT_KHR;
    default:
        return VK_VIDEO_CODEC_OPERATION_NONE_KHR;
    }
}

template <typename T>
void fillBuffer(const DeviceInterface &vk, const VkDevice device, Allocation &bufferAlloc, const std::vector<T> &data,
                VkDeviceSize nonCoherentAtomSize, VkDeviceSize mappedSize, VkDeviceSize dataOffset = 0)
{
    VkDeviceSize dataSize    = data.size() * sizeof(T);
    VkDeviceSize roundedSize = ((dataSize + nonCoherentAtomSize - 1) / nonCoherentAtomSize) * nonCoherentAtomSize;

    VkDeviceSize flushSize;
    if (dataOffset + roundedSize > mappedSize)
    {
        flushSize = VK_WHOLE_SIZE;
    }
    else
    {
        flushSize = roundedSize;
    }

    const VkMappedMemoryRange memRange = {
        VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE, //  VkStructureType sType;
        nullptr,                               //  const void* pNext;
        bufferAlloc.getMemory(),               //  VkDeviceMemory memory;
        bufferAlloc.getOffset() + dataOffset,  //  VkDeviceSize offset;
        flushSize                              //  VkDeviceSize size;
    };

    T *hostPtr = static_cast<T *>(bufferAlloc.getHostPtr());
    memcpy(hostPtr + dataOffset, data.data(), data.size() * sizeof(T));

    VK_CHECK(vk.flushMappedMemoryRanges(device, 1u, &memRange));
}

template <typename T>
vector<T> createQuantizationPatternImage(VkExtent2D quantizationMapExtent, T leftSideQp, T rightSideQp)
{
    size_t totalPixels = quantizationMapExtent.width * quantizationMapExtent.height;
    vector<T> quantizationMap(totalPixels);

    uint32_t midPoint = quantizationMapExtent.width / 2;

    for (uint32_t y = 0; y < quantizationMapExtent.height; ++y)
    {
        for (uint32_t x = 0; x < quantizationMapExtent.width; ++x)
        {
            if (x < midPoint)
            {
                quantizationMap[y * quantizationMapExtent.width + x] = leftSideQp;
            }
            else
            {
                quantizationMap[y * quantizationMapExtent.width + x] = rightSideQp;
            }
        }
    }

    return quantizationMap;
}

void copyBufferToImage(const DeviceInterface &vk, VkDevice device, VkQueue queue, uint32_t queueFamilyIndex,
                       const VkBuffer &buffer, VkDeviceSize bufferSize, const VkExtent2D &imageSize,
                       uint32_t arrayLayers, VkImage destImage, bool generalLayout)
{
    Move<VkCommandPool> cmdPool = createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, queueFamilyIndex);
    Move<VkCommandBuffer> cmdBuffer = allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    Move<VkFence> fence             = createFence(vk, device);
    VkImageLayout destImageLayout =
        generalLayout ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_VIDEO_ENCODE_QUANTIZATION_MAP_KHR;
    VkPipelineStageFlags destImageDstStageFlags = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    VkAccessFlags finalAccessMask               = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;

    const VkCommandBufferBeginInfo cmdBufferBeginInfo = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, // VkStructureType sType;
        nullptr,                                     // const void* pNext;
        VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT, // VkCommandBufferUsageFlags flags;
        (const VkCommandBufferInheritanceInfo *)nullptr,
    };

    const VkBufferImageCopy copyRegion = {
        0,                                              // VkDeviceSize                    bufferOffset
        0,                                              // uint32_t                        bufferRowLength
        0,                                              // uint32_t                        bufferImageHeight
        {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, arrayLayers}, // VkImageSubresourceLayers        imageSubresource
        {0, 0, 0},                                      // VkOffset3D                    imageOffset
        {imageSize.width, imageSize.height, 1}          // VkExtent3D                    imageExtent
    };

    // Barriers for copying buffer to image
    const VkBufferMemoryBarrier preBufferBarrier =
        makeBufferMemoryBarrier(VK_ACCESS_HOST_WRITE_BIT,    // VkAccessFlags srcAccessMask;
                                VK_ACCESS_TRANSFER_READ_BIT, // VkAccessFlags dstAccessMask;
                                buffer,                      // VkBuffer buffer;
                                0u,                          // VkDeviceSize offset;
                                bufferSize                   // VkDeviceSize size;
        );

    const VkImageSubresourceRange subresourceRange{
        // VkImageSubresourceRange subresourceRange;
        VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspect;
        0u,                        // uint32_t baseMipLevel;
        1u,                        // uint32_t mipLevels;
        0u,                        // uint32_t baseArraySlice;
        arrayLayers                // uint32_t arraySize;
    };

    const VkImageMemoryBarrier preImageBarrier =
        makeImageMemoryBarrier(0u,                                   // VkAccessFlags srcAccessMask;
                               VK_ACCESS_TRANSFER_WRITE_BIT,         // VkAccessFlags dstAccessMask;
                               VK_IMAGE_LAYOUT_UNDEFINED,            // VkImageLayout oldLayout;
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, // VkImageLayout newLayout;
                               destImage,                            // VkImage image;
                               subresourceRange                      // VkImageSubresourceRange subresourceRange;
        );

    const VkImageMemoryBarrier postImageBarrier =
        makeImageMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT,         // VkAccessFlags srcAccessMask;
                               finalAccessMask,                      // VkAccessFlags dstAccessMask;
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, // VkImageLayout oldLayout;
                               destImageLayout,                      // VkImageLayout newLayout;
                               destImage,                            // VkImage image;
                               subresourceRange                      // VkImageSubresourceRange subresourceRange;
        );

    VK_CHECK(vk.beginCommandBuffer(*cmdBuffer, &cmdBufferBeginInfo));
    vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, (VkDependencyFlags)0,
                          0, (const VkMemoryBarrier *)nullptr, 1, &preBufferBarrier, 1, &preImageBarrier);
    vk.cmdCopyBufferToImage(*cmdBuffer, buffer, destImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1u, &copyRegion);
    vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, destImageDstStageFlags, (VkDependencyFlags)0, 0,
                          (const VkMemoryBarrier *)nullptr, 0, (const VkBufferMemoryBarrier *)nullptr, 1,
                          &postImageBarrier);
    VK_CHECK(vk.endCommandBuffer(*cmdBuffer));

    const VkPipelineStageFlags pipelineStageFlags = VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT;

    const VkSubmitInfo submitInfo = {
        VK_STRUCTURE_TYPE_SUBMIT_INFO, // VkStructureType sType;
        nullptr,                       // const void* pNext;
        0u,                            // uint32_t waitSemaphoreCount;
        nullptr,                       // const VkSemaphore* pWaitSemaphores;
        &pipelineStageFlags,           // const VkPipelineStageFlags* pWaitDstStageMask;
        1u,                            // uint32_t commandBufferCount;
        &cmdBuffer.get(),              // const VkCommandBuffer* pCommandBuffers;
        0u,                            // uint32_t signalSemaphoreCount;
        nullptr                        // const VkSemaphore* pSignalSemaphores;
    };

    try
    {
        VK_CHECK(vk.queueSubmit(queue, 1, &submitInfo, *fence));
        VK_CHECK(vk.waitForFences(device, 1, &fence.get(), true, ~(0ull)));
    }
    catch (...)
    {
        VK_CHECK(vk.deviceWaitIdle(device));
        throw;
    }
}

VkVideoPictureResourceInfoKHR makeVideoPictureResource(const VkExtent2D &codedExtent, uint32_t baseArrayLayer,
                                                       const VkImageView imageView, const void *pNext = nullptr)
{
    const VkVideoPictureResourceInfoKHR videoPictureResource = {
        VK_STRUCTURE_TYPE_VIDEO_PICTURE_RESOURCE_INFO_KHR, //  VkStructureType sType;
        pNext,                                             //  const void* pNext;
        {0, 0},                                            //  VkOffset2D codedOffset;
        codedExtent,                                       //  VkExtent2D codedExtent;
        baseArrayLayer,                                    //  uint32_t baseArrayLayer;
        imageView,                                         //  VkImageView imageViewBinding;
    };

    return videoPictureResource;
}

VkVideoReferenceSlotInfoKHR makeVideoReferenceSlot(int32_t slotIndex,
                                                   const VkVideoPictureResourceInfoKHR *pPictureResource,
                                                   const void *pNext = nullptr)
{
    const VkVideoReferenceSlotInfoKHR videoReferenceSlotKHR = {
        VK_STRUCTURE_TYPE_VIDEO_REFERENCE_SLOT_INFO_KHR, //  VkStructureType sType;
        pNext,                                           //  const void* pNext;
        slotIndex,                                       //  int32_t slotIndex;
        pPictureResource,                                //  const VkVideoPictureResourceInfoKHR* pPictureResource;
    };

    return videoReferenceSlotKHR;
}

// Vulkan video is not supported on android platform
// all external libraries, helper functions and test instances has been excluded
#ifdef DE_BUILD_VIDEO

#endif // DE_BUILD_VIDEO

class VideoEncodeTestInstance : public VideoBaseTestInstance
{
public:
    VideoEncodeTestInstance(Context &context, const TestDefinition *testDefinition);
    ~VideoEncodeTestInstance(void);

    tcu::TestStatus iterate(void);

protected:
    Move<VkQueryPool> createEncodeVideoQueries(const DeviceInterface &videoDeviceDriver, VkDevice device,
                                               uint32_t numQueries, const VkVideoProfileInfoKHR *pVideoProfile);

    VkFormat checkImageFormat(VkImageUsageFlags flags, const VkVideoProfileListInfoKHR *videoProfileList,
                              const VkFormat requiredFormat);

    bool checkQueryResultSupport(void);

    void printBuffer(const DeviceInterface &videoDeviceDriver, VkDevice device, const BufferWithMemory &buffer,
                     VkDeviceSize bufferSize);

    VkFormat getResultImageFormat(void);

    const TestDefinition *m_testDefinition;

private:
    // Test configuration
    VkVideoCodecOperationFlagBitsKHR m_videoCodecEncodeOperation;
    VkVideoCodecOperationFlagBitsKHR m_videoCodecDecodeOperation;
    uint32_t m_gopCount;
    uint32_t m_gopFrameCount;
    uint32_t m_dpbSlots;
    VkExtent2D m_codedExtent;
    bool m_layeredSrc;

    // Feature flags
    bool m_queryStatus;
    bool m_useInlineQueries;
    bool m_resourcesWithoutProfiles;
    bool m_resolutionChange;
    bool m_swapOrder;
    bool m_useVariableBitrate;
    bool m_useConstantBitrate;
    bool m_customEncodeUsage;
    bool m_useQualityLevel;
    bool m_useDeltaMap;
    bool m_useEmphasisMap;
    bool m_disableRateControl;
    bool m_activeRateControl;

    // QP values
    int32_t m_constQp;
    int32_t m_maxQpValue;
    int32_t m_minQpValue;
    float m_minEmphasisQpValue;
    float m_maxEmphasisQpValue;
    int32_t m_minQpDelta;
    int32_t m_maxQpDelta;

    // Device and interfaces
    const InstanceInterface *m_vki;
    VkPhysicalDevice m_physicalDevice;
    VkDevice m_videoEncodeDevice;
    const DeviceInterface *m_videoDeviceDriver;
    uint32_t m_encodeQueueFamilyIndex;
    uint32_t m_decodeQueueFamilyIndex;
    uint32_t m_transferQueueFamilyIndex;
    VkQueue m_encodeQueue;
    VkQueue m_decodeQueue;
    VkQueue m_transferQueue;

    // Formats
    VkFormat m_imageFormat;
    VkFormat m_dpbImageFormat;

    // Profiles and capabilities
    MovePtr<VkVideoEncodeUsageInfoKHR> m_encodeUsageInfo;
    MovePtr<VkVideoProfileInfoKHR> m_videoEncodeProfile;
    MovePtr<VkVideoProfileInfoKHR> m_videoDecodeProfile;
    MovePtr<VkVideoProfileListInfoKHR> m_videoEncodeProfileList;
    MovePtr<VkVideoEncodeCapabilitiesKHR> m_videoEncodeCapabilities;
    MovePtr<VkVideoCapabilitiesKHR> m_videoCapabilities;
    MovePtr<VkVideoEncodeH264CapabilitiesKHR> m_videoH264CapabilitiesExtension;
    MovePtr<VkVideoEncodeH265CapabilitiesKHR> m_videoH265CapabilitiesExtension;
    MovePtr<VkVideoEncodeH264QuantizationMapCapabilitiesKHR> m_H264QuantizationMapCapabilities;
    MovePtr<VkVideoEncodeH265QuantizationMapCapabilitiesKHR> m_H265QuantizationMapCapabilities;

    // Buffer management
    VkDeviceSize m_bitstreamBufferOffset;
    VkDeviceSize m_minBitstreamBufferOffsetAlignment;
    VkDeviceSize m_nonCoherentAtomSize;
    void initializeTestParameters(void);
    void setupDeviceAndQueues(void);
    void queryAndValidateCapabilities(void);

    // Video session
    Move<VkVideoSessionKHR> m_videoEncodeSession;
    vector<AllocationPtr> m_encodeAllocation;
    void createVideoSession(void);

    // Quantization map resources
    uint8_t m_quantizationMapCount;
    VkExtent2D m_quantizationMapExtent;
    VkExtent2D m_quantizationMapTexelSize;
    std::vector<std::unique_ptr<const ImageWithMemory>> m_quantizationMapImages;
    std::vector<std::unique_ptr<const Move<VkImageView>>> m_quantizationMapImageViews;
    void setupQuantizationMapResources(void);

    // Session parameters
    uint32_t m_qualityLevel;
    std::vector<Move<VkVideoSessionParametersKHR>> m_videoEncodeSessionParameters;
    void setupSessionParameters(void);

    // DPB resources
    bool m_separateReferenceImages;
    std::vector<std::unique_ptr<const ImageWithMemory>> m_dpbImages;
    std::vector<std::unique_ptr<const Move<VkImageView>>> m_dpbImageViews;
    std::vector<std::unique_ptr<const VkVideoPictureResourceInfoKHR>> m_dpbPictureResources;
    std::vector<VkVideoReferenceSlotInfoKHR> m_dpbImageVideoReferenceSlots;
    std::vector<MovePtr<StdVideoEncodeH264ReferenceInfo>> m_H264refInfos;
    std::vector<MovePtr<StdVideoEncodeH265ReferenceInfo>> m_H265refInfos;
    std::vector<MovePtr<VkVideoEncodeH264DpbSlotInfoKHR>> m_H264dpbSlotInfos;
    std::vector<MovePtr<VkVideoEncodeH265DpbSlotInfoKHR>> m_H265dpbSlotInfos;
    void prepareDPBResources(void);

    // Source image resources
    std::vector<std::unique_ptr<const ImageWithMemory>> m_imageVector;
    std::vector<std::unique_ptr<const Move<VkImageView>>> m_imageViewVector;
    std::vector<std::unique_ptr<const VkVideoPictureResourceInfoKHR>> m_imagePictureResourceVector;
    void prepareInputImages(void);
    VkExtent2D currentCodedExtent(uint32_t frame);

    // Session headers
    std::vector<std::vector<uint8_t>> m_headersData;
    void getSessionParametersHeaders(void);

    // Rate Control
    VkVideoEncodeRateControlModeFlagBitsKHR m_rateControlMode;
    de::MovePtr<VkVideoEncodeH264RateControlLayerInfoKHR> m_videoEncodeH264RateControlLayerInfo;
    de::MovePtr<VkVideoEncodeH265RateControlLayerInfoKHR> m_videoEncodeH265RateControlLayerInfo;
    de::MovePtr<VkVideoEncodeRateControlLayerInfoKHR> m_videoEncodeRateControlLayerInfo;
    VkVideoEncodeH264RateControlInfoKHR m_videoEncodeH264RateControlInfo;
    VkVideoEncodeH265RateControlInfoKHR m_videoEncodeH265RateControlInfo;
    de::MovePtr<VkVideoEncodeRateControlInfoKHR> m_videoEncodeRateControlInfo;
    void setupRateControl(void);

    // Command buffers
    Move<VkCommandPool> m_encodeCmdPool;
    Move<VkCommandBuffer> m_firstEncodeCmdBuffer;
    Move<VkCommandBuffer> m_secondEncodeCmdBuffer;
    void setupCommandBuffers(void);

    // Encode buffer
    VkDeviceSize m_encodeBufferSize;
    VkDeviceSize m_encodeFrameBufferSizeAligned;
    std::unique_ptr<BufferWithMemory> m_encodeBuffer;
    Move<VkQueryPool> m_encodeQueryPool;
    void prepareEncodeBuffer(void);

    // Input video frames
    std::vector<de::MovePtr<std::vector<uint8_t>>> m_inVector;
    void loadVideoFrames(void);

    // Frame encoding
    uint32_t m_queryId;
    void encodeFrames(void);
    void encodeFrame(uint16_t gopIdx, uint32_t nalIdx, VkBuffer encodeBuffer, VkDeviceSize encodeFrameBufferSizeAligned,
                     Move<VkQueryPool> &encodeQueryPool);

    // Swap Order Submission
    void handleSwapOrderSubmission(Move<VkQueryPool> &encodeQueryPool);

    // Verify Encoded Bitstream
    tcu::TestStatus verifyEncodedBitstream(const BufferWithMemory &encodeBuffer, VkDeviceSize encodeBufferSize);

    // Dump output of encoding tests.
    tcu::VideoEncodeOutput m_dumpOutput;

    // Intra refresh capabilities and parameters
    bool m_useIntraRefresh;
    VkVideoEncodeIntraRefreshModeFlagBitsKHR m_intraRefreshMode;
    uint32_t m_intraRefreshRegionCount;
    bool m_intraRefreshEmptyRegion;
    bool m_intraRefreshMidway;
    uint32_t m_intraRefreshCycleDuration;
    MovePtr<VkVideoEncodeIntraRefreshCapabilitiesKHR> m_videoEncodeIntraRefreshCapabilities;
    void queryIntraRefreshCapabilities(void);

    MovePtr<VkVideoEncodeIntraRefreshInfoKHR> createIntraRefreshInfo(uint32_t nalIdx);
    std::vector<MovePtr<VkVideoReferenceIntraRefreshInfoKHR>> m_referenceIntraRefreshInfos;
    void updateReferenceSlotsForIntraRefresh(uint32_t nalIdx, VkVideoReferenceSlotInfoKHR *referenceSlots,
                                             uint8_t refsCount);
    VkVideoEncodeFlagsKHR getEncodeFlags(uint32_t nalIdx);
    uint32_t getIntraRefreshIndex(uint32_t nalIdx) const;

    uint32_t calculateTotalFramesFromClipData(const std::vector<uint8_t> &clip, uint32_t width, uint32_t height);
};

VideoEncodeTestInstance::VideoEncodeTestInstance(Context &context, const TestDefinition *testDefinition)
    : VideoBaseTestInstance(context)
    , m_testDefinition(testDefinition)
{
}

VideoEncodeTestInstance::~VideoEncodeTestInstance(void)
{
}

Move<VkQueryPool> VideoEncodeTestInstance::createEncodeVideoQueries(const DeviceInterface &videoDeviceDriver,
                                                                    VkDevice device, uint32_t numQueries,
                                                                    const VkVideoProfileInfoKHR *pVideoProfile)
{

    VkQueryPoolVideoEncodeFeedbackCreateInfoKHR encodeFeedbackQueryType = {
        VK_STRUCTURE_TYPE_QUERY_POOL_VIDEO_ENCODE_FEEDBACK_CREATE_INFO_KHR, //  VkStructureType sType;
        pVideoProfile,                                                      //  const void* pNext;
        VK_VIDEO_ENCODE_FEEDBACK_BITSTREAM_BUFFER_OFFSET_BIT_KHR |
            VK_VIDEO_ENCODE_FEEDBACK_BITSTREAM_BYTES_WRITTEN_BIT_KHR, //  VkVideoEncodeFeedbackFlagsKHR encodeFeedbackFlags;
    };

    const VkQueryPoolCreateInfo queryPoolCreateInfo = {
        VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,            //  VkStructureType sType;
        static_cast<const void *>(&encodeFeedbackQueryType), //  const void* pNext;
        0,                                                   //  VkQueryPoolCreateFlags flags;
        VK_QUERY_TYPE_VIDEO_ENCODE_FEEDBACK_KHR,             //  VkQueryType queryType;
        numQueries,                                          //  uint32_t queryCount;
        0,                                                   //  VkQueryPipelineStatisticFlags pipelineStatistics;
    };

    return createQueryPool(videoDeviceDriver, device, &queryPoolCreateInfo);
}

VkFormat VideoEncodeTestInstance::checkImageFormat(VkImageUsageFlags flags,
                                                   const VkVideoProfileListInfoKHR *videoProfileList,
                                                   const VkFormat requiredFormat)
{
    const InstanceInterface &vki               = m_context.getInstanceInterface();
    const VkPhysicalDevice physicalDevice      = m_context.getPhysicalDevice();
    MovePtr<vector<VkFormat>> supportedFormats = getSupportedFormats(vki, physicalDevice, flags, videoProfileList);

    if (!supportedFormats || supportedFormats->empty())
        TCU_THROW(NotSupportedError, "No supported picture formats");

    for (const auto &supportedFormat : *supportedFormats)
        if (supportedFormat == requiredFormat)
            return requiredFormat;

    TCU_THROW(NotSupportedError, "Failed to find required picture format");
}

bool VideoEncodeTestInstance::checkQueryResultSupport(void)
{
    uint32_t count = 0;
    auto &vkif     = m_context.getInstanceInterface();
    vkif.getPhysicalDeviceQueueFamilyProperties2(m_context.getPhysicalDevice(), &count, nullptr);
    std::vector<VkQueueFamilyProperties2> queues(count);
    std::vector<VkQueueFamilyVideoPropertiesKHR> videoQueues(count);
    std::vector<VkQueueFamilyQueryResultStatusPropertiesKHR> queryResultStatus(count);

    for (std::vector<VkQueueFamilyProperties2>::size_type i = 0; i < queues.size(); i++)
    {
        queues[i].sType            = VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2;
        videoQueues[i].sType       = VK_STRUCTURE_TYPE_QUEUE_FAMILY_VIDEO_PROPERTIES_KHR;
        queues[i].pNext            = &videoQueues[i];
        queryResultStatus[i].sType = VK_STRUCTURE_TYPE_QUEUE_FAMILY_QUERY_RESULT_STATUS_PROPERTIES_KHR;
        videoQueues[i].pNext       = &queryResultStatus[i];
    }
    vkif.getPhysicalDeviceQueueFamilyProperties2(m_context.getPhysicalDevice(), &count, queues.data());

    for (auto &property : queryResultStatus)
    {
        if (property.queryResultStatusSupport)
            return true;
    }

    return false;
}

bool saveBufferAsFile(const BufferWithMemory &buffer, VkDeviceSize bufferSize, const string &outputFileName)
{
    auto &bufferAlloc  = buffer.getAllocation();
    const auto dataPtr = reinterpret_cast<uint8_t *>(bufferAlloc.getHostPtr());
    ofstream outFile(outputFileName, ios::binary | ios::out);

    if (!outFile.is_open())
    {
        cerr << "Error: Unable to open output file '" << outputFileName << "'." << endl;
        return false;
    }

    outFile.write(reinterpret_cast<char *>(dataPtr), static_cast<std::streamsize>(bufferSize));
    outFile.close();

    return true;
}

void VideoEncodeTestInstance::initializeTestParameters()
{
    // Set up codec operations
    m_videoCodecEncodeOperation = m_testDefinition->getCodecOperation();
    m_videoCodecDecodeOperation = getCodecDecodeOperationFromEncode(m_videoCodecEncodeOperation);

    // Set up GOP parameters
    m_gopCount      = m_testDefinition->gopCount();
    m_gopFrameCount = m_testDefinition->gopFrameCount();
    m_dpbSlots      = m_testDefinition->gopReferenceFrameCount();
    m_codedExtent   = {m_testDefinition->getClipWidth(), m_testDefinition->getClipHeight()};

    // Set whether it uses src image array
    m_layeredSrc = m_testDefinition->isLayered();

    // Set up feature flags
    m_queryStatus              = m_testDefinition->hasOption(Option::UseStatusQueries);
    m_useInlineQueries         = m_testDefinition->hasOption(Option::UseInlineQueries);
    m_resourcesWithoutProfiles = m_testDefinition->hasOption(Option::ResourcesWithoutProfiles);
    m_resolutionChange         = m_testDefinition->hasOption(Option::ResolutionChange);
    m_swapOrder                = m_testDefinition->hasOption(Option::SwapOrder);
    m_useVariableBitrate       = m_testDefinition->hasOption(Option::UseVariableBitrateControl);
    m_useConstantBitrate       = m_testDefinition->hasOption(Option::UseConstantBitrateControl);
    m_customEncodeUsage        = m_testDefinition->hasOption(Option::UseEncodeUsage);
    m_useQualityLevel          = m_testDefinition->hasOption(Option::UseQualityLevel);
    m_useDeltaMap              = m_testDefinition->hasOption(Option::UseDeltaMap);
    m_useEmphasisMap           = m_testDefinition->hasOption(Option::UseEmphasisMap);
    m_disableRateControl       = m_testDefinition->hasOption(Option::DisableRateControl);
    m_activeRateControl        = m_useVariableBitrate || m_useConstantBitrate;

    // Set up QP values
    m_constQp            = 28;
    m_maxQpValue         = m_disableRateControl || m_activeRateControl ? 42 : 51;
    m_minQpValue         = 0;
    m_minEmphasisQpValue = 0.0f;
    m_maxEmphasisQpValue = 1.0f;
    m_minQpDelta         = 0;
    m_maxQpDelta         = 0;

    // Initialize buffer offsets
    m_bitstreamBufferOffset = 0u;

    // Set up encode usage info
    m_encodeUsageInfo = getEncodeUsageInfo(
        m_testDefinition->getEncodeProfileExtension(),
        m_customEncodeUsage ? VK_VIDEO_ENCODE_USAGE_STREAMING_BIT_KHR : VK_VIDEO_ENCODE_USAGE_DEFAULT_KHR,
        m_customEncodeUsage ? VK_VIDEO_ENCODE_CONTENT_DESKTOP_BIT_KHR : VK_VIDEO_ENCODE_CONTENT_DEFAULT_KHR,
        m_customEncodeUsage ? VK_VIDEO_ENCODE_TUNING_MODE_HIGH_QUALITY_KHR : VK_VIDEO_ENCODE_TUNING_MODE_DEFAULT_KHR);

    // Create encode and decode profiles
    m_videoEncodeProfile = getVideoProfile(m_videoCodecEncodeOperation, m_encodeUsageInfo.get());
    m_videoDecodeProfile = getVideoProfile(m_videoCodecDecodeOperation, m_testDefinition->getDecodeProfileExtension());

    // Create profile list for encode
    m_videoEncodeProfileList = getVideoProfileList(m_videoEncodeProfile.get(), 1);

    // Check query support if needed
    if (m_queryStatus && !checkQueryResultSupport())
        TCU_THROW(NotSupportedError, "Implementation does not support query status");

    // Set up quality level
    m_qualityLevel = 0;

    // Dump mode for debugging
    m_dumpOutput = m_context.getTestContext().getCommandLine().getVideoDumpEncodeOutput();

    // Initialize intra refresh parameters
    if (m_testDefinition->hasOption(Option::IntraRefreshPicturePartition))
        m_intraRefreshMode = VK_VIDEO_ENCODE_INTRA_REFRESH_MODE_PER_PICTURE_PARTITION_BIT_KHR;
    else if (m_testDefinition->hasOption(Option::IntraRefreshBlockBased))
        m_intraRefreshMode = VK_VIDEO_ENCODE_INTRA_REFRESH_MODE_BLOCK_BASED_BIT_KHR;
    else if (m_testDefinition->hasOption(Option::IntraRefreshBlockRow))
        m_intraRefreshMode = VK_VIDEO_ENCODE_INTRA_REFRESH_MODE_BLOCK_ROW_BASED_BIT_KHR;
    else if (m_testDefinition->hasOption(Option::IntraRefreshBlockColumn))
        m_intraRefreshMode = VK_VIDEO_ENCODE_INTRA_REFRESH_MODE_BLOCK_COLUMN_BASED_BIT_KHR;
    else
        m_intraRefreshMode = VK_VIDEO_ENCODE_INTRA_REFRESH_MODE_NONE_KHR;

    m_intraRefreshEmptyRegion   = m_testDefinition->hasOption(Option::IntraRefreshEmptyRegion);
    m_intraRefreshMidway        = m_testDefinition->hasOption(Option::IntraRefreshMidway);
    m_useIntraRefresh           = m_intraRefreshMode != VK_VIDEO_ENCODE_INTRA_REFRESH_MODE_NONE_KHR;
    m_intraRefreshRegionCount   = 0;
    m_intraRefreshCycleDuration = 0;
}

void VideoEncodeTestInstance::setupDeviceAndQueues()
{
    // Get instance interface and physical device
    m_vki            = &m_context.getInstanceInterface();
    m_physicalDevice = m_context.getPhysicalDevice();

    // Get formats
    m_imageFormat    = checkImageFormat(VK_IMAGE_USAGE_VIDEO_ENCODE_SRC_BIT_KHR, m_videoEncodeProfileList.get(),
                                        VK_FORMAT_G8_B8R8_2PLANE_420_UNORM);
    m_dpbImageFormat = checkImageFormat(VK_IMAGE_USAGE_VIDEO_ENCODE_DPB_BIT_KHR, m_videoEncodeProfileList.get(),
                                        VK_FORMAT_G8_B8R8_2PLANE_420_UNORM);

    // Get video device
    const VideoDevice::VideoDeviceFlags videoDeviceFlags = m_testDefinition->requiredDeviceFlags();
    m_videoEncodeDevice =
        getDeviceSupportingQueue(VK_QUEUE_VIDEO_ENCODE_BIT_KHR | VK_QUEUE_VIDEO_DECODE_BIT_KHR | VK_QUEUE_TRANSFER_BIT,
                                 m_videoCodecEncodeOperation | m_videoCodecDecodeOperation, videoDeviceFlags);
    m_videoDeviceDriver = &getDeviceDriver();

    // Get non-coherent atom size for memory alignment
    m_nonCoherentAtomSize = m_context.getDeviceProperties().limits.nonCoherentAtomSize;

    // Get queue family indices and queues
    m_encodeQueueFamilyIndex   = getQueueFamilyIndexEncode();
    m_decodeQueueFamilyIndex   = getQueueFamilyIndexDecode();
    m_transferQueueFamilyIndex = getQueueFamilyIndexTransfer();

    m_encodeQueue   = getDeviceQueue(*m_videoDeviceDriver, m_videoEncodeDevice, m_encodeQueueFamilyIndex, 0u);
    m_decodeQueue   = getDeviceQueue(*m_videoDeviceDriver, m_videoEncodeDevice, m_decodeQueueFamilyIndex, 0u);
    m_transferQueue = getDeviceQueue(*m_videoDeviceDriver, m_videoEncodeDevice, m_transferQueueFamilyIndex, 0u);
}

void VideoEncodeTestInstance::queryAndValidateCapabilities()
{
    // Get quantization map capabilities
    m_H264QuantizationMapCapabilities = getVideoEncodeH264QuantizationMapCapabilities();
    m_H265QuantizationMapCapabilities = getVideoEncodeH265QuantizationMapCapabilities();

    // Get codec capabilities
    const bool quantizationMapEnabled = m_useDeltaMap | m_useEmphasisMap;
    m_videoH264CapabilitiesExtension =
        getVideoCapabilitiesExtensionH264E(quantizationMapEnabled ? m_H264QuantizationMapCapabilities.get() : nullptr);
    m_videoH265CapabilitiesExtension =
        getVideoCapabilitiesExtensionH265E(quantizationMapEnabled ? m_H265QuantizationMapCapabilities.get() : nullptr);

    // Get capabilities extension based on codec
    void *videoCapabilitiesExtensionPtr = NULL;
    if (m_testDefinition->getProfile()->IsH264())
        videoCapabilitiesExtensionPtr = static_cast<void *>(m_videoH264CapabilitiesExtension.get());
    else if (m_testDefinition->getProfile()->IsH265())
        videoCapabilitiesExtensionPtr = static_cast<void *>(m_videoH265CapabilitiesExtension.get());
    DE_ASSERT(videoCapabilitiesExtensionPtr);

    // Get encode capabilities
    m_videoEncodeCapabilities = getVideoEncodeCapabilities(videoCapabilitiesExtensionPtr);

    if (m_useIntraRefresh)
    {
        m_videoEncodeIntraRefreshCapabilities = getIntraRefreshCapabilities();
        appendStructurePtrToVulkanChain((const void **)&m_videoEncodeCapabilities->pNext,
                                        m_videoEncodeIntraRefreshCapabilities.get());
    }

    m_videoCapabilities =
        getVideoCapabilities(*m_vki, m_physicalDevice, m_videoEncodeProfile.get(), m_videoEncodeCapabilities.get());
    m_minBitstreamBufferOffsetAlignment = m_videoCapabilities->minBitstreamBufferOffsetAlignment;

    if (m_useIntraRefresh)
    {
        // @FIXME: For now the GOP size can't be larger than available DPB slots due to limitations
        //         in DPB slot management.
        TCU_CHECK_AND_THROW(InternalError, m_videoCapabilities->maxDpbSlots >= m_gopFrameCount,
                            "Maximum DPB slots must be greater than or equal to GOP frame count");
    }

    TCU_CHECK_AND_THROW(InternalError,
                        m_videoEncodeCapabilities->supportedEncodeFeedbackFlags &
                            VK_VIDEO_ENCODE_FEEDBACK_BITSTREAM_BYTES_WRITTEN_BIT_KHR,
                        "Implementation must support bitstream bytes written feedback");

    // Check intra-refresh capabilities
    queryIntraRefreshCapabilities();

    // Check for required features
    if (m_useDeltaMap)
    {
        if (!(m_videoEncodeCapabilities->flags & VK_VIDEO_ENCODE_CAPABILITY_QUANTIZATION_DELTA_MAP_BIT_KHR))
            TCU_THROW(NotSupportedError, "Implementation does not support quantization delta map");

        if (m_testDefinition->getProfile()->IsH264())
        {
            m_minQpDelta = m_H264QuantizationMapCapabilities->minQpDelta;
            m_maxQpDelta = m_H264QuantizationMapCapabilities->maxQpDelta;
        }
        else if (m_testDefinition->getProfile()->IsH265())
        {
            m_minQpDelta = m_H265QuantizationMapCapabilities->minQpDelta;
            m_maxQpDelta = m_H265QuantizationMapCapabilities->maxQpDelta;
        }
    }

    if (m_useEmphasisMap)
    {
        if (!(m_videoEncodeCapabilities->flags & VK_VIDEO_ENCODE_CAPABILITY_EMPHASIS_MAP_BIT_KHR))
            TCU_THROW(NotSupportedError, "Implementation does not support emphasis map");
    }

    // Check support for P and B frames
    if (m_testDefinition->getProfile()->IsH264())
    {
        bool minPReferenceCount  = m_videoH264CapabilitiesExtension->maxPPictureL0ReferenceCount > 0;
        bool minBReferenceCount  = m_videoH264CapabilitiesExtension->maxBPictureL0ReferenceCount > 0;
        bool minL1ReferenceCount = m_videoH264CapabilitiesExtension->maxL1ReferenceCount > 0;

        if (m_testDefinition->patternContain(P_FRAME) && !minPReferenceCount)
            TCU_THROW(NotSupportedError, "Implementation does not support H264 P frames encoding");
        else if (m_testDefinition->patternContain(B_FRAME) && !minBReferenceCount && !minL1ReferenceCount)
            TCU_THROW(NotSupportedError, "Implementation does not support H264 B frames encoding");
    }
    else if (m_testDefinition->getProfile()->IsH265())
    {
        bool minPReferenceCount  = m_videoH265CapabilitiesExtension->maxPPictureL0ReferenceCount > 0;
        bool minBReferenceCount  = m_videoH265CapabilitiesExtension->maxBPictureL0ReferenceCount > 0;
        bool minL1ReferenceCount = m_videoH265CapabilitiesExtension->maxL1ReferenceCount > 0;

        if (m_testDefinition->patternContain(P_FRAME) && !minPReferenceCount)
            TCU_THROW(NotSupportedError, "Implementation does not support H265 P frames encoding");
        else if (m_testDefinition->patternContain(B_FRAME) && !minBReferenceCount && !minL1ReferenceCount)
            TCU_THROW(NotSupportedError, "Implementation does not support H265 B frames encoding");
    }

    // Check support for bitrate control
    if (m_useVariableBitrate)
    {
        if ((m_videoEncodeCapabilities->rateControlModes & VK_VIDEO_ENCODE_RATE_CONTROL_MODE_VBR_BIT_KHR) == 0)
            TCU_THROW(NotSupportedError, "Implementation does not support variable bitrate control");

        TCU_CHECK_AND_THROW(InternalError, m_videoEncodeCapabilities->maxBitrate > 0,
                            "Maximum bitrate must be greater than zero for variable bitrate");
    }
    else if (m_useConstantBitrate)
    {
        if ((m_videoEncodeCapabilities->rateControlModes & VK_VIDEO_ENCODE_RATE_CONTROL_MODE_CBR_BIT_KHR) == 0)
            TCU_THROW(NotSupportedError, "Implementation does not support constant bitrate control");

        TCU_CHECK_AND_THROW(InternalError, m_videoEncodeCapabilities->maxBitrate > 0,
                            "Maximum bitrate must be greater than zero for constant bitrate");
    }

    // Verify DPB slots support
    TCU_CHECK_AND_THROW(InternalError, m_videoCapabilities->maxDpbSlots >= m_dpbSlots,
                        "Maximum DPB slots must be greater than or equal to requested DPB slots");

    // Verify that chosen quality level is satisfied
    TCU_CHECK_AND_THROW(InternalError, m_qualityLevel < m_videoEncodeCapabilities->maxQualityLevels,
                        "Quality level must be less than maximum quality levels");
}

void VideoEncodeTestInstance::createVideoSession(void)
{
    // Set session creation flags based on requirements
    VkVideoSessionCreateFlagsKHR videoSessionFlags = 0;
    if (m_useInlineQueries)
        videoSessionFlags |= VK_VIDEO_SESSION_CREATE_INLINE_QUERIES_BIT_KHR;
    if (m_useDeltaMap)
        videoSessionFlags |= VK_VIDEO_SESSION_CREATE_ALLOW_ENCODE_QUANTIZATION_DELTA_MAP_BIT_KHR;
    if (m_useEmphasisMap)
        videoSessionFlags |= VK_VIDEO_SESSION_CREATE_ALLOW_ENCODE_EMPHASIS_MAP_BIT_KHR;

    // Create video session info structure
    const MovePtr<VkVideoSessionCreateInfoKHR> videoEncodeSessionCreateInfo = getVideoSessionCreateInfo(
        m_encodeQueueFamilyIndex, videoSessionFlags, m_videoEncodeProfile.get(), m_codedExtent, m_imageFormat,
        m_dpbImageFormat, m_dpbSlots, m_videoCapabilities->maxActiveReferencePictures);

    // Create intra refresh create info if needed
    VkVideoEncodeSessionIntraRefreshCreateInfoKHR intraRefreshCreateInfo;
    if (m_useIntraRefresh)
    {
        intraRefreshCreateInfo.sType = VK_STRUCTURE_TYPE_VIDEO_ENCODE_SESSION_INTRA_REFRESH_CREATE_INFO_KHR;
        intraRefreshCreateInfo.pNext = VK_NULL_HANDLE;

        // Set the intra refresh mode based on the test
        intraRefreshCreateInfo.intraRefreshMode = m_intraRefreshMode;

        appendStructurePtrToVulkanChain(&videoEncodeSessionCreateInfo.get()->pNext, &intraRefreshCreateInfo);
    }

    // Create the video session
    m_videoEncodeSession =
        createVideoSessionKHR(*m_videoDeviceDriver, m_videoEncodeDevice, videoEncodeSessionCreateInfo.get());

    // Bind memory to the video session
    m_encodeAllocation =
        getAndBindVideoSessionMemory(*m_videoDeviceDriver, m_videoEncodeDevice, *m_videoEncodeSession, getAllocator());
}

void VideoEncodeTestInstance::setupQuantizationMapResources(void)
{
    m_quantizationMapCount     = m_useDeltaMap ? 3 : 2;
    m_quantizationMapExtent    = {0, 0};
    m_quantizationMapTexelSize = {0, 0};

    if (!m_useDeltaMap && !m_useEmphasisMap)
        return;

    VkFormat quantizationImageFormat      = VK_FORMAT_R8_SNORM;
    VkImageTiling quantizationImageTiling = VK_IMAGE_TILING_OPTIMAL;

    // Query quantization map capabilities
    uint32_t videoFormatPropertiesCount = 0u;

    VkImageUsageFlags quantizationImageUsageFlags =
        (m_useDeltaMap ? VK_IMAGE_USAGE_VIDEO_ENCODE_QUANTIZATION_DELTA_MAP_BIT_KHR :
                         VK_IMAGE_USAGE_VIDEO_ENCODE_EMPHASIS_MAP_BIT_KHR) |
        VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    const VkPhysicalDeviceVideoFormatInfoKHR videoFormatInfo = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VIDEO_FORMAT_INFO_KHR, //  VkStructureType sType;
        m_videoEncodeProfileList.get(),                          //  const void* pNext;
        quantizationImageUsageFlags,                             //  VkImageUsageFlags imageUsage;
    };

    VkVideoFormatPropertiesKHR videoFormatPropertiesKHR = {};
    videoFormatPropertiesKHR.sType                      = VK_STRUCTURE_TYPE_VIDEO_FORMAT_PROPERTIES_KHR;
    videoFormatPropertiesKHR.pNext                      = nullptr;

    VkVideoFormatQuantizationMapPropertiesKHR quantizationMapPropertiesKHR = {};
    quantizationMapPropertiesKHR.sType = VK_STRUCTURE_TYPE_VIDEO_FORMAT_QUANTIZATION_MAP_PROPERTIES_KHR;
    quantizationMapPropertiesKHR.pNext = nullptr;

    VkVideoFormatH265QuantizationMapPropertiesKHR H265QuantizationMapFormatProperty = {};
    H265QuantizationMapFormatProperty.sType = VK_STRUCTURE_TYPE_VIDEO_FORMAT_H265_QUANTIZATION_MAP_PROPERTIES_KHR;
    H265QuantizationMapFormatProperty.pNext = nullptr;

    vector<VkVideoFormatPropertiesKHR> videoFormatProperties;
    vector<VkVideoFormatQuantizationMapPropertiesKHR> quantizationMapProperties;
    vector<VkVideoFormatH265QuantizationMapPropertiesKHR> H265quantizationMapFormatProperties;
    de::MovePtr<vector<VkFormat>> result;

    VK_CHECK(m_vki->getPhysicalDeviceVideoFormatPropertiesKHR(m_physicalDevice, &videoFormatInfo,
                                                              &videoFormatPropertiesCount, nullptr));

    videoFormatProperties.resize(videoFormatPropertiesCount, videoFormatPropertiesKHR);
    quantizationMapProperties.resize(videoFormatPropertiesCount, quantizationMapPropertiesKHR);
    H265quantizationMapFormatProperties.resize(videoFormatPropertiesCount, H265QuantizationMapFormatProperty);

    for (uint32_t i = 0; i < videoFormatPropertiesCount; ++i)
    {
        videoFormatProperties[i].pNext = &quantizationMapProperties[i];
        if (m_testDefinition->getProfile()->IsH265())
            quantizationMapProperties[i].pNext = &H265quantizationMapFormatProperties[i];
    }

    VK_CHECK(m_vki->getPhysicalDeviceVideoFormatPropertiesKHR(
        m_physicalDevice, &videoFormatInfo, &videoFormatPropertiesCount, videoFormatProperties.data()));

    // Pick first available quantization map format and properties
    quantizationImageFormat    = videoFormatProperties[0].format;
    quantizationImageTiling    = videoFormatProperties[0].imageTiling;
    m_quantizationMapTexelSize = quantizationMapProperties[0].quantizationMapTexelSize;

    DE_ASSERT(m_quantizationMapTexelSize.width > 0 && m_quantizationMapTexelSize.height > 0);

    m_quantizationMapExtent = {static_cast<uint32_t>(std::ceil(static_cast<float>(m_codedExtent.width) /
                                                               static_cast<float>(m_quantizationMapTexelSize.width))),
                               static_cast<uint32_t>(std::ceil(static_cast<float>(m_codedExtent.height) /
                                                               static_cast<float>(m_quantizationMapTexelSize.height)))};

    const VkImageUsageFlags quantizationMapImageUsage =
        (m_useDeltaMap ? VK_IMAGE_USAGE_VIDEO_ENCODE_QUANTIZATION_DELTA_MAP_BIT_KHR :
                         VK_IMAGE_USAGE_VIDEO_ENCODE_EMPHASIS_MAP_BIT_KHR) |
        VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    const VkImageCreateInfo quantizationMapImageCreateInfo = makeImageCreateInfo(
        quantizationImageFormat, m_quantizationMapExtent, 0, &m_encodeQueueFamilyIndex, quantizationMapImageUsage,
        m_videoEncodeProfileList.get(), 1U, VK_IMAGE_LAYOUT_UNDEFINED, quantizationImageTiling);

    const vector<uint32_t> transaferQueueFamilyIndices(1u, m_transferQueueFamilyIndex);

    const VkBufferUsageFlags quantizationMapBufferUsageFlags = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    const VkDeviceSize quantizationMapBufferSize =
        getBufferSize(quantizationImageFormat, m_quantizationMapExtent.width, m_quantizationMapExtent.height);

    const VkBufferCreateInfo quantizationMapBufferCreateInfo = makeBufferCreateInfo(
        quantizationMapBufferSize, quantizationMapBufferUsageFlags, transaferQueueFamilyIndices, 0, nullptr);

    BufferWithMemory quantizationMapBuffer(*m_videoDeviceDriver, m_videoEncodeDevice, getAllocator(),
                                           quantizationMapBufferCreateInfo,
                                           MemoryRequirement::Local | MemoryRequirement::HostVisible);

    Allocation &quantizationMapBufferAlloc = quantizationMapBuffer.getAllocation();
    void *quantizationMapBufferHostPtr     = quantizationMapBufferAlloc.getHostPtr();

    // Calculate QP values for each image sides, the type of values is based on the quantization map format and adnotated by the index
    auto calculateMapValues = [this](auto idx, QuantizationMap mapType) -> auto
    {
        using T          = decltype(idx);
        T leftSideValue  = T{0};
        T rightSideValue = T{0};

        if (mapType == QM_DELTA)
        {
            // Quantization map provided, constant Qp set to 26
            if (idx == 0)
            {
                leftSideValue = rightSideValue = static_cast<T>(std::max(m_minQpValue - m_constQp, m_minQpDelta));
            }
            // Quantization map provided, constant Qp set to 26
            else if (idx == 1)
            {
                leftSideValue = rightSideValue = static_cast<T>(std::min(m_maxQpValue - m_constQp, m_maxQpDelta));
            }
            // Only third frame will receive different quantization values for both sides
            else if (idx == 2)
            {
                leftSideValue  = static_cast<T>(std::max(m_minQpValue - m_constQp, m_minQpDelta));
                rightSideValue = static_cast<T>(std::min(m_maxQpValue - m_constQp, m_maxQpDelta));
            }
        }
        else if (mapType == QM_EMPHASIS)
        {
            // Only second frame will receive different quantization values for both sides
            if (idx == 1)
            {
                if constexpr (std::is_same_v<T, uint8_t>)
                {
                    leftSideValue  = static_cast<T>(m_minEmphasisQpValue * 255.0f);
                    rightSideValue = static_cast<T>(m_maxEmphasisQpValue * 255.0f);
                }
                else
                {
                    leftSideValue  = static_cast<T>(m_minEmphasisQpValue);
                    rightSideValue = static_cast<T>(m_maxEmphasisQpValue);
                }
            }
        }

        return std::make_tuple(leftSideValue, rightSideValue);
    };

    // Create quantization map image
    auto processQuantizationMapImage = [&](auto leftSideQp, auto rightSideQp)
    {
        using T = decltype(leftSideQp);

        auto quantizationMapImageData =
            createQuantizationPatternImage<T>(m_quantizationMapExtent, leftSideQp, rightSideQp);

        std::unique_ptr<const ImageWithMemory> quantizationMapImage(
            new ImageWithMemory(*m_videoDeviceDriver, m_videoEncodeDevice, getAllocator(),
                                quantizationMapImageCreateInfo, MemoryRequirement::Any));
        std::unique_ptr<const Move<VkImageView>> quantizationMapImageView(new Move<VkImageView>(
            makeImageView(*m_videoDeviceDriver, m_videoEncodeDevice, quantizationMapImage->get(), VK_IMAGE_VIEW_TYPE_2D,
                          quantizationImageFormat, makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1))));

        deMemset(quantizationMapBufferHostPtr, 0x00, static_cast<size_t>(quantizationMapBufferSize));
        flushAlloc(*m_videoDeviceDriver, m_videoEncodeDevice, quantizationMapBufferAlloc);

        fillBuffer(*m_videoDeviceDriver, m_videoEncodeDevice, quantizationMapBufferAlloc, quantizationMapImageData,
                   m_nonCoherentAtomSize, quantizationMapBufferSize);

        copyBufferToImage(*m_videoDeviceDriver, m_videoEncodeDevice, m_transferQueue, m_transferQueueFamilyIndex,
                          *quantizationMapBuffer, quantizationMapBufferSize, m_quantizationMapExtent, 1,
                          quantizationMapImage->get(), m_testDefinition->usesGeneralLayout());

        m_quantizationMapImages.push_back(std::move(quantizationMapImage));
        m_quantizationMapImageViews.push_back(std::move(quantizationMapImageView));
    };

    for (uint32_t qmIdx = 0; qmIdx < m_quantizationMapCount; ++qmIdx)
    {
        switch (quantizationImageFormat)
        {
        case VK_FORMAT_R8_UNORM:
        {
            auto [leftSideQp, rightSideQp] = calculateMapValues(uint8_t(qmIdx), QM_EMPHASIS);
            processQuantizationMapImage(leftSideQp, rightSideQp);
            break;
        }
        case VK_FORMAT_R8_SINT:
        {
            auto [leftSideQp, rightSideQp] = calculateMapValues(int8_t(qmIdx), QM_DELTA);
            processQuantizationMapImage(leftSideQp, rightSideQp);
            break;
        }
        case VK_FORMAT_R16_SINT:
        {
            auto [leftSideQp, rightSideQp] = calculateMapValues(int16_t(qmIdx), QM_DELTA);
            processQuantizationMapImage(leftSideQp, rightSideQp);
            break;
        }
        case VK_FORMAT_R32_SINT:
        {
            auto [leftSideQp, rightSideQp] = calculateMapValues(int32_t(qmIdx), QM_DELTA);
            processQuantizationMapImage(leftSideQp, rightSideQp);
            break;
        }
        default:
            TCU_THROW(NotSupportedError, "Unsupported quantization map format");
        }
    }
}

void VideoEncodeTestInstance::setupSessionParameters()
{
    const auto videoEncodeQualityLevelInfo = getVideoEncodeQualityLevelInfo(m_qualityLevel, nullptr);
    MovePtr<VkVideoEncodeQuantizationMapSessionParametersCreateInfoKHR> quantizationMapSessionParametersInfo =
        getVideoEncodeH264QuantizationMapParameters(m_quantizationMapTexelSize);

    std::vector<MovePtr<StdVideoH264SequenceParameterSet>> stdVideoH264SequenceParameterSets;
    std::vector<MovePtr<StdVideoH264PictureParameterSet>> stdVideoH264PictureParameterSets;
    std::vector<MovePtr<VkVideoEncodeH264SessionParametersAddInfoKHR>> encodeH264SessionParametersAddInfoKHRs;
    std::vector<MovePtr<VkVideoEncodeH264SessionParametersCreateInfoKHR>> H264sessionParametersCreateInfos;

    std::vector<MovePtr<StdVideoH265ProfileTierLevel>> stdVideoH265ProfileTierLevels;
    std::vector<MovePtr<StdVideoH265DecPicBufMgr>> stdVideoH265DecPicBufMgrs;
    std::vector<MovePtr<StdVideoH265VideoParameterSet>> stdVideoH265VideoParameterSets;
    std::vector<MovePtr<StdVideoH265SequenceParameterSetVui>> stdVideoH265SequenceParameterSetVuis;
    std::vector<MovePtr<StdVideoH265SequenceParameterSet>> stdVideoH265SequenceParameterSets;
    std::vector<MovePtr<StdVideoH265PictureParameterSet>> stdVideoH265PictureParameterSets;
    std::vector<MovePtr<VkVideoEncodeH265SessionParametersAddInfoKHR>> encodeH265SessionParametersAddInfoKHRs;
    std::vector<MovePtr<VkVideoEncodeH265SessionParametersCreateInfoKHR>> H265sessionParametersCreateInfos;

    std::vector<MovePtr<VkVideoSessionParametersCreateInfoKHR>> videoEncodeSessionParametersCreateInfos;

    for (int i = 0; i < (m_resolutionChange ? 2 : 1); ++i)
    {
        // Second videoEncodeSessionParameters is being created with half the size
        uint32_t extentWidth  = i == 0 ? m_codedExtent.width : m_codedExtent.width / 2;
        uint32_t extentHeight = i == 0 ? m_codedExtent.height : m_codedExtent.height / 2;

        stdVideoH264SequenceParameterSets.push_back(getStdVideoH264EncodeSequenceParameterSet(
            extentWidth, extentHeight, m_testDefinition->maxNumRefs(), nullptr));
        stdVideoH264PictureParameterSets.push_back(getStdVideoH264EncodePictureParameterSet(
            m_testDefinition->ppsActiveRefs0(), m_testDefinition->ppsActiveRefs1()));
        encodeH264SessionParametersAddInfoKHRs.push_back(createVideoEncodeH264SessionParametersAddInfoKHR(
            1u, stdVideoH264SequenceParameterSets.back().get(), 1u, stdVideoH264PictureParameterSets.back().get()));

        H264sessionParametersCreateInfos.push_back(createVideoEncodeH264SessionParametersCreateInfoKHR(
            static_cast<const void *>(m_useQualityLevel ?
                                          videoEncodeQualityLevelInfo.get() :
                                          ((m_useDeltaMap || m_useEmphasisMap) ?
                                               static_cast<const void *>(quantizationMapSessionParametersInfo.get()) :
                                               nullptr)),
            1u, 1u, encodeH264SessionParametersAddInfoKHRs.back().get()));

        stdVideoH265ProfileTierLevels.push_back(
            getStdVideoH265ProfileTierLevel(STD_VIDEO_H265_PROFILE_IDC_MAIN, STD_VIDEO_H265_LEVEL_IDC_6_2));
        stdVideoH265DecPicBufMgrs.push_back(getStdVideoH265DecPicBufMgr());
        stdVideoH265VideoParameterSets.push_back(getStdVideoH265VideoParameterSet(
            stdVideoH265DecPicBufMgrs.back().get(), stdVideoH265ProfileTierLevels.back().get()));
        stdVideoH265SequenceParameterSetVuis.push_back(
            getStdVideoH265SequenceParameterSetVui(m_testDefinition->getClipFrameRate()));
        stdVideoH265SequenceParameterSets.push_back(getStdVideoH265SequenceParameterSet(
            extentWidth, extentHeight, m_videoH265CapabilitiesExtension->ctbSizes,
            m_videoH265CapabilitiesExtension->transformBlockSizes, stdVideoH265DecPicBufMgrs.back().get(),
            stdVideoH265ProfileTierLevels.back().get(), stdVideoH265SequenceParameterSetVuis.back().get()));
        stdVideoH265PictureParameterSets.push_back(
            getStdVideoH265PictureParameterSet(m_videoH265CapabilitiesExtension.get()));
        encodeH265SessionParametersAddInfoKHRs.push_back(getVideoEncodeH265SessionParametersAddInfoKHR(
            1u, stdVideoH265VideoParameterSets.back().get(), 1u, stdVideoH265SequenceParameterSets.back().get(), 1u,
            stdVideoH265PictureParameterSets.back().get()));
        H265sessionParametersCreateInfos.push_back(getVideoEncodeH265SessionParametersCreateInfoKHR(
            static_cast<const void *>(m_useQualityLevel ?
                                          videoEncodeQualityLevelInfo.get() :
                                          ((m_useDeltaMap || m_useEmphasisMap) ?
                                               static_cast<const void *>(quantizationMapSessionParametersInfo.get()) :
                                               nullptr)),
            1u, 1u, 1u, encodeH265SessionParametersAddInfoKHRs.back().get()));

        const void *sessionParametersCreateInfoPtr = nullptr;

        if (m_testDefinition->getProfile()->IsH264())
            sessionParametersCreateInfoPtr = static_cast<const void *>(H264sessionParametersCreateInfos.back().get());
        else if (m_testDefinition->getProfile()->IsH265())
            sessionParametersCreateInfoPtr = static_cast<const void *>(H265sessionParametersCreateInfos.back().get());
        DE_ASSERT(sessionParametersCreateInfoPtr);

        const VkVideoSessionParametersCreateFlagsKHR videoSessionParametersFlags =
            (m_useDeltaMap || m_useEmphasisMap) ?
                static_cast<VkVideoSessionParametersCreateFlagsKHR>(
                    VK_VIDEO_SESSION_PARAMETERS_CREATE_QUANTIZATION_MAP_COMPATIBLE_BIT_KHR) :
                static_cast<VkVideoSessionParametersCreateFlagsKHR>(0);

        videoEncodeSessionParametersCreateInfos.push_back(getVideoSessionParametersCreateInfoKHR(
            sessionParametersCreateInfoPtr, videoSessionParametersFlags, *m_videoEncodeSession));
        m_videoEncodeSessionParameters.push_back(createVideoSessionParametersKHR(
            *m_videoDeviceDriver, m_videoEncodeDevice, videoEncodeSessionParametersCreateInfos.back().get()));
    }
}

void VideoEncodeTestInstance::prepareDPBResources(void)
{
    const VkImageUsageFlags dpbImageUsage = VK_IMAGE_USAGE_VIDEO_ENCODE_DPB_BIT_KHR;

    // Check if implementation supports separate reference images
    m_separateReferenceImages =
        m_videoCapabilities.get()->flags & VK_VIDEO_CAPABILITY_SEPARATE_REFERENCE_IMAGES_BIT_KHR;

    const VkImageCreateInfo dpbImageCreateInfo =
        makeImageCreateInfo(m_imageFormat, m_codedExtent, 0, &m_encodeQueueFamilyIndex, dpbImageUsage,
                            m_videoEncodeProfileList.get(), m_separateReferenceImages ? 1 : m_dpbSlots);
    const VkImageViewType dpbImageViewType =
        m_separateReferenceImages ? VK_IMAGE_VIEW_TYPE_2D : VK_IMAGE_VIEW_TYPE_2D_ARRAY;

    // Create DPB images
    for (uint8_t i = 0; i < (m_separateReferenceImages ? m_dpbSlots : 1); ++i)
    {
        std::unique_ptr<ImageWithMemory> dpbImage(new ImageWithMemory(
            *m_videoDeviceDriver, m_videoEncodeDevice, getAllocator(), dpbImageCreateInfo, MemoryRequirement::Any));
        m_dpbImages.push_back(std::move(dpbImage));
    }

    // Create reference info structures
    for (uint8_t i = 0, j = 0; i < m_gopFrameCount; ++i)
    {
        if (m_testDefinition->frameType(i) == B_FRAME)
            continue;

        m_H264refInfos.push_back(getStdVideoEncodeH264ReferenceInfo(getH264PictureType(m_testDefinition->frameType(i)),
                                                                    m_testDefinition->frameNumber(i),
                                                                    m_testDefinition->frameIdx(i) * 2));
        m_H265refInfos.push_back(getStdVideoEncodeH265ReferenceInfo(getH265PictureType(m_testDefinition->frameType(i)),
                                                                    m_testDefinition->frameIdx(i)));

        m_H264dpbSlotInfos.push_back(getVideoEncodeH264DpbSlotInfo(m_H264refInfos[j].get()));
        m_H265dpbSlotInfos.push_back(getVideoEncodeH265DpbSlotInfo(m_H265refInfos[j].get()));

        j++;
    }

    // Create picture resources and reference slots
    for (uint8_t i = 0, j = 0; i < m_gopFrameCount; ++i)
    {
        if (m_testDefinition->frameType(i) == B_FRAME)
            continue;

        const VkImageSubresourceRange dpbImageSubresourceRange = {
            VK_IMAGE_ASPECT_COLOR_BIT, //  VkImageAspectFlags aspectMask;
            0,                         //  uint32_t baseMipLevel;
            1,                         //  uint32_t levelCount;
            m_separateReferenceImages ? static_cast<uint32_t>(0) :
                                        static_cast<uint32_t>(j), //  uint32_t baseArrayLayer;
            1,                                                    //  uint32_t layerCount;
        };

        std::unique_ptr<Move<VkImageView>> dpbImageView(new Move<VkImageView>(makeImageView(
            *m_videoDeviceDriver, m_videoEncodeDevice, m_dpbImages[m_separateReferenceImages ? j : 0]->get(),
            dpbImageViewType, m_imageFormat, dpbImageSubresourceRange)));
        std::unique_ptr<VkVideoPictureResourceInfoKHR> dpbPictureResource(
            new VkVideoPictureResourceInfoKHR(makeVideoPictureResource(m_codedExtent, 0, dpbImageView->get())));

        m_dpbImageViews.push_back(std::move(dpbImageView));
        m_dpbPictureResources.push_back(std::move(dpbPictureResource));

        const void *dpbSlotInfoPtr = nullptr;

        if (m_testDefinition->getProfile()->IsH264())
            dpbSlotInfoPtr = static_cast<const void *>(m_H264dpbSlotInfos[j].get());
        else if (m_testDefinition->getProfile()->IsH265())
            dpbSlotInfoPtr = static_cast<const void *>(m_H265dpbSlotInfos[j].get());
        DE_ASSERT(dpbSlotInfoPtr);

        m_dpbImageVideoReferenceSlots.push_back(
            makeVideoReferenceSlot(-1, m_dpbPictureResources[j].get(), dpbSlotInfoPtr));

        j++;
    }

    // Ensure m_dpbImageVideoReferenceSlots has enough entries for all possible slot indices
    // Fill remaining slots with properly initialized but inactive slots
    while (m_dpbImageVideoReferenceSlots.size() < m_dpbSlots)
    {
        // Create a dummy slot with proper sType initialization
        VkVideoReferenceSlotInfoKHR dummySlot = {
            VK_STRUCTURE_TYPE_VIDEO_REFERENCE_SLOT_INFO_KHR, // sType
            nullptr,                                         // pNext
            -1,                                              // slotIndex (inactive)
            nullptr                                          // pPictureResource
        };
        m_dpbImageVideoReferenceSlots.push_back(dummySlot);
    }
}

VkExtent2D VideoEncodeTestInstance::currentCodedExtent(uint32_t frame)
{
    VkExtent2D currentCodedExtent = m_codedExtent;

    // For resolution_change_dpb tests, it changes from frame 2.
    if (m_resolutionChange && (frame > 1))
    {
        currentCodedExtent.width /= 2;
        currentCodedExtent.height /= 2;
    }

    if (currentCodedExtent.width > m_videoCapabilities->maxCodedExtent.width ||
        currentCodedExtent.height > m_videoCapabilities->maxCodedExtent.height)
    {
        TCU_THROW(NotSupportedError, "Required dimensions exceed maxCodedExtent");
    }

    if (currentCodedExtent.width < m_videoCapabilities->minCodedExtent.width ||
        currentCodedExtent.height < m_videoCapabilities->minCodedExtent.height)
    {
        TCU_THROW(NotSupportedError, "Required dimensions are smaller than minCodedExtent");
    }

    return currentCodedExtent;
}

void VideoEncodeTestInstance::prepareInputImages(void)
{
    const VkImageUsageFlags imageUsage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_VIDEO_ENCODE_SRC_BIT_KHR;

    uint32_t framesToProcess = m_gopCount * m_gopFrameCount;

    for (uint32_t i = 0; i < (m_layeredSrc ? 1 : framesToProcess); ++i)
    {
        VkExtent2D codedExtent = currentCodedExtent(i);

        const VkImageCreateInfo imageCreateInfo = makeImageCreateInfo(
            m_imageFormat, codedExtent,
            m_resourcesWithoutProfiles ? VK_IMAGE_CREATE_VIDEO_PROFILE_INDEPENDENT_BIT_KHR : 0,
            &m_transferQueueFamilyIndex, imageUsage,
            m_resourcesWithoutProfiles ? nullptr : m_videoEncodeProfileList.get(), m_layeredSrc ? framesToProcess : 1);

        std::unique_ptr<const ImageWithMemory> image(new ImageWithMemory(
            *m_videoDeviceDriver, m_videoEncodeDevice, getAllocator(), imageCreateInfo, MemoryRequirement::Any));

        m_imageVector.push_back(std::move(image));

        for (uint32_t j = 0; j < (m_layeredSrc ? framesToProcess : 1); ++j)
        {
            codedExtent = m_layeredSrc ? currentCodedExtent(j) : codedExtent;

            std::unique_ptr<const Move<VkImageView>> imageView(new Move<VkImageView>(
                makeImageView(*m_videoDeviceDriver, m_videoEncodeDevice, m_imageVector[m_layeredSrc ? 0 : i]->get(),
                              m_layeredSrc ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D, m_imageFormat,
                              makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, m_layeredSrc ? j : 0, 1))));

            std::unique_ptr<const VkVideoPictureResourceInfoKHR> imagePictureResource(
                new VkVideoPictureResourceInfoKHR(makeVideoPictureResource(codedExtent, 0, **imageView)));

            m_imageViewVector.push_back(std::move(imageView));
            m_imagePictureResourceVector.push_back(std::move(imagePictureResource));
        }
    }
}

void VideoEncodeTestInstance::loadVideoFrames(void)
{
    de::MovePtr<vector<uint8_t>> clip = loadVideoData(m_testDefinition->getClipFilePath());

    m_inVector.clear();

    // Get the available frame count from the clip info, but if it is zero then calculate it.
    uint32_t availableFrames = m_testDefinition->getClipTotalFrames();
    if (availableFrames == 0)
        availableFrames = calculateTotalFramesFromClipData(*clip, m_codedExtent.width, m_codedExtent.height);

    // Log the available frame count
    m_context.getTestContext().getLog() << tcu::TestLog::Message << "Available frames in clip: " << availableFrames
                                        << tcu::TestLog::EndMessage;

    // FIXME: Adjust gopFrameCount if needed (for intra refresh tests). An issue has been detected
    // where the DPB slots are not being used correctly by the test definition.
    if (m_useIntraRefresh)
    {
        // Limit gopFrameCount to available frames
        m_gopFrameCount = std::min(m_gopFrameCount, availableFrames);

        m_context.getTestContext().getLog()
            << tcu::TestLog::Message << "Final frame count for intra refresh: " << m_gopFrameCount
            << tcu::TestLog::EndMessage;
    }

    // Limit the number of frames to process based on availableFrames
    uint32_t framesToProcess = std::min(m_gopCount * m_gopFrameCount, availableFrames);

    for (uint32_t i = 0; i < framesToProcess; ++i)
    {
        uint32_t gopIdx = i / m_gopFrameCount;

        uint32_t extentWidth  = m_codedExtent.width;
        uint32_t extentHeight = m_codedExtent.height;

        bool half_size = false;

        if (m_resolutionChange && gopIdx == 1)
        {
            extentWidth /= 2;
            extentHeight /= 2;
            half_size = true;
        }

        MovePtr<MultiPlaneImageData> multiPlaneImageData(
            new MultiPlaneImageData(m_imageFormat, tcu::UVec2(extentWidth, extentHeight)));
        vkt::ycbcr::extractI420Frame(*clip, i, m_codedExtent.width, m_codedExtent.height, multiPlaneImageData.get(),
                                     half_size);

        // Save NV12 Multiplanar frame to YUV 420p 8 bits
        de::MovePtr<std::vector<uint8_t>> in =
            vkt::ycbcr::YCbCrConvUtil<uint8_t>::MultiPlanarNV12toI420(multiPlaneImageData.get());

        if (m_dumpOutput & tcu::DUMP_ENC_YUV)
        {
            std::string filename = "in_" + std::to_string(i) + ".yuv";
            vkt::ycbcr::YCbCrContent<uint8_t>::save(*in, filename);
        }

        vkt::ycbcr::uploadImage(*m_videoDeviceDriver, m_videoEncodeDevice, m_transferQueueFamilyIndex, getAllocator(),
                                m_layeredSrc ? m_imageVector[0]->get() : m_imageVector[i]->get(), *multiPlaneImageData,
                                0, VK_IMAGE_LAYOUT_GENERAL, m_layeredSrc ? i : 0);

        m_inVector.push_back(std::move(in));
    }
}

void VideoEncodeTestInstance::getSessionParametersHeaders()
{
    VkVideoEncodeSessionParametersFeedbackInfoKHR videoEncodeSessionParametersFeedbackInfo = {
        VK_STRUCTURE_TYPE_VIDEO_ENCODE_SESSION_PARAMETERS_FEEDBACK_INFO_KHR, //  VkStructureType sType;
        nullptr,                                                             //  void* pNext;
        false,                                                               //  VkBool32 hasOverrides;
    };

    const VkVideoEncodeH264SessionParametersGetInfoKHR videoEncodeH264SessionParametersGetInfo = {
        VK_STRUCTURE_TYPE_VIDEO_ENCODE_H264_SESSION_PARAMETERS_GET_INFO_KHR, //  VkStructureType sType;
        nullptr,                                                             //  const void* pNext;
        true,                                                                //  VkBool32 writeStdSPS;
        true,                                                                //  VkBool32 writeStdPPS;
        0,                                                                   //  uint32_t stdSPSId;
        0,                                                                   //  uint32_t stdPPSId;
    };

    const VkVideoEncodeH265SessionParametersGetInfoKHR videoEncodeH265SessionParametersGetInfo = {
        VK_STRUCTURE_TYPE_VIDEO_ENCODE_H265_SESSION_PARAMETERS_GET_INFO_KHR, //  VkStructureType sType;
        nullptr,                                                             //  const void* pNext;
        true,                                                                //  VkBool32 writeStdVPS;
        true,                                                                //  VkBool32 writeStdSPS;
        true,                                                                //  VkBool32 writeStdPPS;
        0,                                                                   //  uint32_t stdVPSId;
        0,                                                                   //  uint32_t stdSPSId;
        0,                                                                   //  uint32_t stdPPSId;
    };

    const void *videoEncodeSessionParametersGetInfoPtr = nullptr;

    if (m_testDefinition->getProfile()->IsH264())
        videoEncodeSessionParametersGetInfoPtr = static_cast<const void *>(&videoEncodeH264SessionParametersGetInfo);
    else if (m_testDefinition->getProfile()->IsH265())
        videoEncodeSessionParametersGetInfoPtr = static_cast<const void *>(&videoEncodeH265SessionParametersGetInfo);
    DE_ASSERT(videoEncodeSessionParametersGetInfoPtr);

    m_headersData.clear();

    for (int i = 0; i < (m_resolutionChange ? 2 : 1); ++i)
    {
        const VkVideoEncodeSessionParametersGetInfoKHR videoEncodeSessionParametersGetInfo = {
            VK_STRUCTURE_TYPE_VIDEO_ENCODE_SESSION_PARAMETERS_GET_INFO_KHR, // VkStructureType sType;
            videoEncodeSessionParametersGetInfoPtr,                         // const void* pNext;
            m_videoEncodeSessionParameters[i].get(), // VkVideoSessionParametersKHR videoSessionParameters;
        };

        std::vector<uint8_t> headerData;

        size_t requiredHeaderSize = 0;
        VK_CHECK(m_videoDeviceDriver->getEncodedVideoSessionParametersKHR(
            m_videoEncodeDevice, &videoEncodeSessionParametersGetInfo, &videoEncodeSessionParametersFeedbackInfo,
            &requiredHeaderSize, nullptr));

        TCU_CHECK_AND_THROW(InternalError, requiredHeaderSize != 0, "Required header size must be non-zero");

        headerData.resize(requiredHeaderSize);
        VK_CHECK(m_videoDeviceDriver->getEncodedVideoSessionParametersKHR(
            m_videoEncodeDevice, &videoEncodeSessionParametersGetInfo, &videoEncodeSessionParametersFeedbackInfo,
            &requiredHeaderSize, headerData.data()));

        m_headersData.push_back(std::move(headerData));
    }
}

void VideoEncodeTestInstance::setupRateControl()
{
    m_videoEncodeH264RateControlLayerInfo =
        getVideoEncodeH264RateControlLayerInfo(true, 0, 0, 0, true, m_maxQpValue, m_maxQpValue, m_maxQpValue);
    m_videoEncodeH265RateControlLayerInfo =
        getVideoEncodeH265RateControlLayerInfo(true, 0, 0, 0, true, m_maxQpValue, m_maxQpValue, m_maxQpValue);

    const void *videoEncodeRateControlLayerInfoPtr = nullptr;

    if (m_testDefinition->getProfile()->IsH264())
        videoEncodeRateControlLayerInfoPtr = static_cast<const void *>(m_videoEncodeH264RateControlLayerInfo.get());
    else if (m_testDefinition->getProfile()->IsH265())
        videoEncodeRateControlLayerInfoPtr = static_cast<const void *>(m_videoEncodeH265RateControlLayerInfo.get());
    DE_ASSERT(videoEncodeRateControlLayerInfoPtr);

    if (m_disableRateControl)
    {
        m_rateControlMode = VK_VIDEO_ENCODE_RATE_CONTROL_MODE_DISABLED_BIT_KHR;
    }
    else if (m_activeRateControl)
    {
        if (m_useVariableBitrate)
            m_rateControlMode = VK_VIDEO_ENCODE_RATE_CONTROL_MODE_VBR_BIT_KHR;
        else
            m_rateControlMode = VK_VIDEO_ENCODE_RATE_CONTROL_MODE_CBR_BIT_KHR;
    }
    else
    {
        m_rateControlMode = VK_VIDEO_ENCODE_RATE_CONTROL_MODE_DEFAULT_KHR;
    }

    m_videoEncodeRateControlLayerInfo = getVideoEncodeRateControlLayerInfo(
        videoEncodeRateControlLayerInfoPtr, m_rateControlMode, m_testDefinition->getClipFrameRate());

    const VkVideoEncodeH264RateControlInfoKHR videoEncodeH264RateControlInfo = {
        VK_STRUCTURE_TYPE_VIDEO_ENCODE_H264_RATE_CONTROL_INFO_KHR, //  VkStructureType sType;
        nullptr,                                                   //  const void* pNext;
        VK_VIDEO_ENCODE_H264_RATE_CONTROL_REGULAR_GOP_BIT_KHR,     //  VkVideoEncodeH264RateControlFlagsKHR flags;
        m_gopFrameCount,                                           //  uint32_t gopFrameCount;
        m_gopFrameCount,                                           //  uint32_t idrPeriod;
        m_testDefinition->getConsecutiveBFrameCount(),             //  uint32_t consecutiveBFrameCount;
        1,                                                         //  uint32_t temporalLayerCount;
    };
    m_videoEncodeH264RateControlInfo = videoEncodeH264RateControlInfo;

    const VkVideoEncodeH265RateControlInfoKHR videoEncodeH265RateControlInfo = {
        VK_STRUCTURE_TYPE_VIDEO_ENCODE_H265_RATE_CONTROL_INFO_KHR, //  VkStructureType sType;
        nullptr,                                                   //  const void* pNext;
        VK_VIDEO_ENCODE_H265_RATE_CONTROL_REGULAR_GOP_BIT_KHR,     //  VkVideoEncodeH265RateControlFlagsKHR flags;
        m_gopFrameCount,                                           //  uint32_t gopFrameCount;
        m_gopFrameCount,                                           //  uint32_t idrPeriod;
        m_testDefinition->getConsecutiveBFrameCount(),             //  uint32_t consecutiveBFrameCount;
        (m_useConstantBitrate || m_useVariableBitrate) ? 1U : 0,   //  uint32_t subLayerCount;
    };
    m_videoEncodeH265RateControlInfo = videoEncodeH265RateControlInfo;

    const void *videoEncodeRateControlInfoPtr = nullptr;

    if (m_testDefinition->getProfile()->IsH264())
        videoEncodeRateControlInfoPtr = static_cast<const void *>(&m_videoEncodeH264RateControlInfo);
    else if (m_testDefinition->getProfile()->IsH265())
        videoEncodeRateControlInfoPtr = static_cast<const void *>(&m_videoEncodeH265RateControlInfo);
    DE_ASSERT(videoEncodeRateControlInfoPtr);

    m_videoEncodeRateControlInfo = getVideoEncodeRateControlInfo(
        m_disableRateControl ? nullptr : videoEncodeRateControlInfoPtr, m_rateControlMode,
        (m_useConstantBitrate || m_useVariableBitrate) ? m_videoEncodeRateControlLayerInfo.get() : nullptr);
}

void VideoEncodeTestInstance::setupCommandBuffers()
{
    m_encodeCmdPool         = makeCommandPool(*m_videoDeviceDriver, m_videoEncodeDevice, m_encodeQueueFamilyIndex);
    m_firstEncodeCmdBuffer  = allocateCommandBuffer(*m_videoDeviceDriver, m_videoEncodeDevice, *m_encodeCmdPool,
                                                    VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    m_secondEncodeCmdBuffer = allocateCommandBuffer(*m_videoDeviceDriver, m_videoEncodeDevice, *m_encodeCmdPool,
                                                    VK_COMMAND_BUFFER_LEVEL_PRIMARY);
}

void VideoEncodeTestInstance::encodeFrames(void)
{
    // Pre fill buffer with SPS and PPS header
    fillBuffer(*m_videoDeviceDriver, m_videoEncodeDevice, m_encodeBuffer.get()->getAllocation(), m_headersData[0],
               m_nonCoherentAtomSize, m_encodeBufferSize, m_bitstreamBufferOffset);
    // Move offset to accommodate header data
    m_bitstreamBufferOffset = deAlign64(m_bitstreamBufferOffset + m_headersData[0].size(),
                                        m_videoCapabilities->minBitstreamBufferOffsetAlignment);

    m_queryId = 0;

    for (uint16_t GOPIdx = 0; GOPIdx < m_gopCount; ++GOPIdx)
    {
        uint32_t emptyRefSlotIdx = m_swapOrder ? 1 : 0;

        if (m_resolutionChange && GOPIdx == 1)
        {
            // Pre fill buffer with new SPS/PPS/VPS header
            fillBuffer(*m_videoDeviceDriver, m_videoEncodeDevice, m_encodeBuffer.get()->getAllocation(),
                       m_headersData[1], m_nonCoherentAtomSize, m_encodeBufferSize, m_bitstreamBufferOffset);
            m_bitstreamBufferOffset =
                deAlign64(m_bitstreamBufferOffset + m_headersData[1].size(), m_minBitstreamBufferOffsetAlignment);
        }

        // Use the adjusted m_gopFrameCount instead of the original pattern size
        for (uint32_t NALIdx = emptyRefSlotIdx; NALIdx < m_gopFrameCount; (m_swapOrder ? --NALIdx : ++NALIdx))
        {
            encodeFrame(GOPIdx, NALIdx, m_encodeBuffer.get()->get(), m_encodeFrameBufferSizeAligned, m_encodeQueryPool);

            if (!(m_testDefinition->frameType(NALIdx) == B_FRAME)) // Update reference slots for non-B-frames
            {
                if (m_swapOrder)
                    emptyRefSlotIdx--;
                else
                    emptyRefSlotIdx++;
            }
        }
    }
}

void VideoEncodeTestInstance::encodeFrame(uint16_t gopIdx, uint32_t nalIdx, VkBuffer encodeBuffer,
                                          VkDeviceSize encodeFrameBufferSizeAligned, Move<VkQueryPool> &encodeQueryPool)
{
    // End coding
    const VkVideoEndCodingInfoKHR videoEndCodingInfo = {
        VK_STRUCTURE_TYPE_VIDEO_END_CODING_INFO_KHR, //  VkStructureType sType;
        nullptr,                                     //  const void* pNext;
        0u,                                          //  VkVideoEndCodingFlagsKHR flags;
    };

    std::vector<de::MovePtr<StdVideoEncodeH264SliceHeader>> stdVideoEncodeH264SliceHeaders;
    std::vector<de::MovePtr<VkVideoEncodeH264NaluSliceInfoKHR>> videoEncodeH264NaluSlices;
    std::vector<de::MovePtr<StdVideoEncodeH264ReferenceListsInfo>> videoEncodeH264ReferenceListInfos;
    std::vector<de::MovePtr<StdVideoEncodeH264PictureInfo>> H264pictureInfos;
    std::vector<de::MovePtr<VkVideoEncodeH264PictureInfoKHR>> videoEncodeH264PictureInfo;

    std::vector<de::MovePtr<StdVideoEncodeH265SliceSegmentHeader>> stdVideoEncodeH265SliceSegmentHeaders;
    std::vector<de::MovePtr<StdVideoH265ShortTermRefPicSet>> stdVideoH265ShortTermRefPicSets;
    std::vector<de::MovePtr<VkVideoEncodeH265NaluSliceSegmentInfoKHR>> videoEncodeH265NaluSliceSegments;
    std::vector<de::MovePtr<StdVideoEncodeH265ReferenceListsInfo>> videoEncodeH265ReferenceListInfos;
    std::vector<de::MovePtr<StdVideoEncodeH265PictureInfo>> H265pictureInfos;
    std::vector<de::MovePtr<VkVideoEncodeH265PictureInfoKHR>> videoEncodeH265PictureInfos;
    std::vector<de::MovePtr<VkVideoEncodeInfoKHR>> m_videoEncodeFrameInfos;

    // Storage for contiguous slice arrays (persistent for this frame)
    std::vector<VkVideoEncodeH264NaluSliceInfoKHR> h264SliceArray;
    std::vector<VkVideoEncodeH265NaluSliceSegmentInfoKHR> h265SliceSegmentArray;

    VkCommandBuffer encodeCmdBuffer = (nalIdx == 1 && m_swapOrder) ? *m_secondEncodeCmdBuffer : *m_firstEncodeCmdBuffer;

    // Reset dpb slots list.
    for (uint32_t dpb = 0; dpb < std::min(m_dpbSlots, m_gopFrameCount); dpb++)
    {
        if (dpb < m_dpbImageVideoReferenceSlots.size())
            m_dpbImageVideoReferenceSlots[dpb].slotIndex = -1;
    }

    beginCommandBuffer(*m_videoDeviceDriver, encodeCmdBuffer, 0u);

    m_videoDeviceDriver->cmdResetQueryPool(encodeCmdBuffer, encodeQueryPool.get(), 0, 2);

    StdVideoH264PictureType stdVideoH264PictureType = getH264PictureType(m_testDefinition->frameType(nalIdx));
    StdVideoH265PictureType stdVideoH265PictureType = getH265PictureType(m_testDefinition->frameType(nalIdx));

    StdVideoH264SliceType stdVideoH264SliceType = getH264SliceType(m_testDefinition->frameType(nalIdx));
    StdVideoH265SliceType stdVideoH265SliceType = getH265SliceType(m_testDefinition->frameType(nalIdx));

    uint32_t refsPool = 0;

    uint8_t H264RefPicList0[STD_VIDEO_H264_MAX_NUM_LIST_REF];
    uint8_t H265RefPicList0[STD_VIDEO_H265_MAX_NUM_LIST_REF];

    std::fill(H264RefPicList0, H264RefPicList0 + STD_VIDEO_H264_MAX_NUM_LIST_REF, STD_VIDEO_H264_NO_REFERENCE_PICTURE);
    std::fill(H265RefPicList0, H265RefPicList0 + STD_VIDEO_H265_MAX_NUM_LIST_REF, STD_VIDEO_H265_NO_REFERENCE_PICTURE);

    uint8_t numL0 = 0;
    uint8_t numL1 = 0;

    bool pType = stdVideoH264PictureType == STD_VIDEO_H264_PICTURE_TYPE_P ||
                 stdVideoH265PictureType == STD_VIDEO_H265_PICTURE_TYPE_P;
    bool bType = stdVideoH264PictureType == STD_VIDEO_H264_PICTURE_TYPE_B ||
                 stdVideoH265PictureType == STD_VIDEO_H265_PICTURE_TYPE_B;

    if (pType)
    {
        refsPool = 1;

        std::vector<uint8_t> list0 = m_testDefinition->ref0(nalIdx);
        for (auto idx : list0)
        {
            H264RefPicList0[numL0]   = idx;
            H265RefPicList0[numL0++] = idx;
        }
    }

    uint8_t H264RefPicList1[STD_VIDEO_H264_MAX_NUM_LIST_REF];
    uint8_t H265RefPicList1[STD_VIDEO_H265_MAX_NUM_LIST_REF];

    std::fill(H264RefPicList1, H264RefPicList1 + STD_VIDEO_H264_MAX_NUM_LIST_REF, STD_VIDEO_H264_NO_REFERENCE_PICTURE);
    std::fill(H265RefPicList1, H265RefPicList1 + STD_VIDEO_H265_MAX_NUM_LIST_REF, STD_VIDEO_H265_NO_REFERENCE_PICTURE);

    if (bType)
    {
        refsPool = 2;

        std::vector<uint8_t> list0 = m_testDefinition->ref0(nalIdx);
        for (auto idx : list0)
        {
            H264RefPicList0[numL0]   = idx;
            H265RefPicList0[numL0++] = idx;
        }

        std::vector<uint8_t> list1 = m_testDefinition->ref1(nalIdx);
        for (auto idx : list1)
        {
            H264RefPicList1[numL1]   = idx;
            H265RefPicList1[numL1++] = idx;
        }
    }

    int32_t startRefSlot    = refsPool == 0 ? -1 : m_testDefinition->refSlots(nalIdx)[0];
    int32_t startRefSlotIdx = m_separateReferenceImages && startRefSlot > -1 ? startRefSlot : 0;

    VkVideoReferenceSlotInfoKHR *referenceSlots;
    std::vector<VkVideoReferenceSlotInfoKHR> usedReferenceSlots;
    uint8_t refsCount = 0;

    if (pType || bType)
    {
        std::vector<uint32_t> tmpSlotIds;
        for (int32_t s = 0; s < numL0; s++)
            tmpSlotIds.push_back(H264RefPicList0[s]);
        for (int32_t s = 0; s < numL1; s++)
            tmpSlotIds.push_back(H264RefPicList1[s]);

        // Sort and remove redundant ids
        sort(tmpSlotIds.begin(), tmpSlotIds.end());
        tmpSlotIds.erase(unique(tmpSlotIds.begin(), tmpSlotIds.end()), tmpSlotIds.end());

        for (auto idx : tmpSlotIds)
        {
            m_dpbImageVideoReferenceSlots[idx].slotIndex = idx;
            usedReferenceSlots.push_back(m_dpbImageVideoReferenceSlots[idx]);
        }
        referenceSlots = &usedReferenceSlots[0];
        refsCount      = (uint8_t)usedReferenceSlots.size();
    }
    else
    {
        referenceSlots = &m_dpbImageVideoReferenceSlots[startRefSlotIdx];
        refsCount      = m_testDefinition->refsCount(nalIdx);
    }

    de::MovePtr<VkVideoBeginCodingInfoKHR> videoBeginCodingFrameInfoKHR = getVideoBeginCodingInfo(
        *m_videoEncodeSession,
        m_resolutionChange ? m_videoEncodeSessionParameters[gopIdx].get() : m_videoEncodeSessionParameters[0].get(),
        m_dpbSlots, &m_dpbImageVideoReferenceSlots[0],
        ((m_activeRateControl || m_disableRateControl) && (nalIdx > 0 || gopIdx > 0)) ?
            m_videoEncodeRateControlInfo.get() :
            nullptr);

    m_videoDeviceDriver->cmdBeginVideoCodingKHR(encodeCmdBuffer, videoBeginCodingFrameInfoKHR.get());

    de::MovePtr<VkVideoCodingControlInfoKHR> resetVideoEncodingControl =
        getVideoCodingControlInfo(VK_VIDEO_CODING_CONTROL_RESET_BIT_KHR);

    if (nalIdx == 0)
    {
        m_videoDeviceDriver->cmdControlVideoCodingKHR(encodeCmdBuffer, resetVideoEncodingControl.get());
        const auto videoEncodeQualityLevelInfo = getVideoEncodeQualityLevelInfo(m_qualityLevel, nullptr);

        if (m_disableRateControl || m_activeRateControl)
        {
            de::MovePtr<VkVideoCodingControlInfoKHR> videoRateConstrolInfo = getVideoCodingControlInfo(
                VK_VIDEO_CODING_CONTROL_ENCODE_RATE_CONTROL_BIT_KHR, m_videoEncodeRateControlInfo.get());
            m_videoDeviceDriver->cmdControlVideoCodingKHR(encodeCmdBuffer, videoRateConstrolInfo.get());
        }
        if (m_useQualityLevel)
        {
            de::MovePtr<VkVideoCodingControlInfoKHR> videoQualityControlInfo = getVideoCodingControlInfo(
                VK_VIDEO_CODING_CONTROL_ENCODE_QUALITY_LEVEL_BIT_KHR, videoEncodeQualityLevelInfo.get());
            m_videoDeviceDriver->cmdControlVideoCodingKHR(encodeCmdBuffer, videoQualityControlInfo.get());
        }
    }

    // Determine number of slices needed for H.264
    uint32_t numSlices = 1;
    if (m_useIntraRefresh && m_intraRefreshMode == VK_VIDEO_ENCODE_INTRA_REFRESH_MODE_PER_PICTURE_PARTITION_BIT_KHR &&
        nalIdx > 0 && nalIdx <= m_intraRefreshRegionCount)
    {
        numSlices = m_intraRefreshCycleDuration;
        // Validate that the number of slices doesn't exceed codec capabilities
        if (m_testDefinition->getProfile()->IsH264() && m_videoH264CapabilitiesExtension &&
            numSlices > m_videoH264CapabilitiesExtension->maxSliceCount)
        {
            TCU_THROW(NotSupportedError, "Intra refresh cycle duration exceeds maximum H.264 slice count");
        }
    }

    // Create the required number of slices for H.264
    for (uint32_t sliceIdx = 0; sliceIdx < numSlices; ++sliceIdx)
    {
        // For intra refresh per-picture partition mode, only the slice corresponding to intraRefreshIndex should be I-type
        StdVideoH264SliceType currentSliceType = stdVideoH264SliceType;
        if (m_useIntraRefresh &&
            m_intraRefreshMode == VK_VIDEO_ENCODE_INTRA_REFRESH_MODE_PER_PICTURE_PARTITION_BIT_KHR && nalIdx > 0 &&
            nalIdx <= m_intraRefreshRegionCount)
        {
            uint32_t intraRefreshIndex = getIntraRefreshIndex(nalIdx);
            if (sliceIdx == intraRefreshIndex)
                currentSliceType = STD_VIDEO_H264_SLICE_TYPE_I;
        }

        bool h264ActiveOverrideFlag =
            (currentSliceType != STD_VIDEO_H264_SLICE_TYPE_I) &&
            ((m_testDefinition->ppsActiveRefs0() != m_testDefinition->shActiveRefs0(nalIdx)) ||
             (m_testDefinition->ppsActiveRefs1() != m_testDefinition->shActiveRefs1(nalIdx)));

        stdVideoEncodeH264SliceHeaders.push_back(
            getStdVideoEncodeH264SliceHeader(currentSliceType, h264ActiveOverrideFlag));
        videoEncodeH264NaluSlices.push_back(getVideoEncodeH264NaluSlice(
            stdVideoEncodeH264SliceHeaders.back().get(),
            (m_rateControlMode == VK_VIDEO_ENCODE_RATE_CONTROL_MODE_DISABLED_BIT_KHR) ? m_constQp : 0));

        h264SliceArray.push_back(*videoEncodeH264NaluSlices.back().get());
    }

    videoEncodeH264ReferenceListInfos.push_back(
        getVideoEncodeH264ReferenceListsInfo(H264RefPicList0, H264RefPicList1, numL0, numL1));
    H264pictureInfos.push_back(getStdVideoEncodeH264PictureInfo(
        getH264PictureType(m_testDefinition->frameType(nalIdx)), m_testDefinition->frameNumber(nalIdx),
        m_testDefinition->frameIdx(nalIdx) * 2, gopIdx,
        nalIdx > 0 ? videoEncodeH264ReferenceListInfos.back().get() : nullptr));

    // Create H.264 picture info with all slices
    videoEncodeH264PictureInfo.push_back(
        getVideoEncodeH264PictureInfo(H264pictureInfos.back().get(), numSlices, h264SliceArray.data()));

    // Determine number of slice segments needed for H.265
    uint32_t numSliceSegments = 1;
    if (m_useIntraRefresh && m_intraRefreshMode == VK_VIDEO_ENCODE_INTRA_REFRESH_MODE_PER_PICTURE_PARTITION_BIT_KHR &&
        nalIdx > 0 && nalIdx <= m_intraRefreshRegionCount)
    {
        numSliceSegments = m_intraRefreshCycleDuration;
        // Validate that the number of slice segments doesn't exceed codec capabilities
        if (m_testDefinition->getProfile()->IsH265() && m_videoH265CapabilitiesExtension &&
            numSliceSegments > m_videoH265CapabilitiesExtension->maxSliceSegmentCount)
        {
            TCU_THROW(NotSupportedError, "Intra refresh cycle duration exceeds maximum H.265 slice segment count");
        }
    }

    // Create the required number of slice segments for H.265
    for (uint32_t sliceIdx = 0; sliceIdx < numSliceSegments; ++sliceIdx)
    {
        // For intra refresh per-picture partition mode, only the slice corresponding to intraRefreshIndex should be I-type
        StdVideoH265SliceType currentSliceType = stdVideoH265SliceType;
        if (m_useIntraRefresh &&
            m_intraRefreshMode == VK_VIDEO_ENCODE_INTRA_REFRESH_MODE_PER_PICTURE_PARTITION_BIT_KHR && nalIdx > 0 &&
            nalIdx <= m_intraRefreshRegionCount)
        {
            uint32_t intraRefreshIndex = getIntraRefreshIndex(nalIdx);
            if (sliceIdx == intraRefreshIndex)
                currentSliceType = STD_VIDEO_H265_SLICE_TYPE_I;
        }

        stdVideoEncodeH265SliceSegmentHeaders.push_back(getStdVideoEncodeH265SliceSegmentHeader(currentSliceType));
        videoEncodeH265NaluSliceSegments.push_back(getVideoEncodeH265NaluSliceSegment(
            stdVideoEncodeH265SliceSegmentHeaders.back().get(),
            (m_rateControlMode == VK_VIDEO_ENCODE_RATE_CONTROL_MODE_DISABLED_BIT_KHR) ? m_constQp : 0));

        h265SliceSegmentArray.push_back(*videoEncodeH265NaluSliceSegments.back().get());
    }

    videoEncodeH265ReferenceListInfos.push_back(getVideoEncodeH265ReferenceListsInfo(H265RefPicList0, H265RefPicList1));
    stdVideoH265ShortTermRefPicSets.push_back(getStdVideoH265ShortTermRefPicSet(
        getH265PictureType(m_testDefinition->frameType(nalIdx)), m_testDefinition->frameIdx(nalIdx),
        m_testDefinition->getConsecutiveBFrameCount()));
    H265pictureInfos.push_back(getStdVideoEncodeH265PictureInfo(
        getH265PictureType(m_testDefinition->frameType(nalIdx)), m_testDefinition->frameIdx(nalIdx),
        nalIdx > 0 ? videoEncodeH265ReferenceListInfos.back().get() : nullptr,
        stdVideoH265ShortTermRefPicSets.back().get()));

    // Create picture info with all slice segments
    videoEncodeH265PictureInfos.push_back(
        getVideoEncodeH265PictureInfo(H265pictureInfos.back().get(), numSliceSegments, h265SliceSegmentArray.data()));

    const void *videoEncodePictureInfoPtr = nullptr;

    if (m_testDefinition->getProfile()->IsH264())
        videoEncodePictureInfoPtr = static_cast<const void *>(videoEncodeH264PictureInfo.back().get());
    else if (m_testDefinition->getProfile()->IsH265())
        videoEncodePictureInfoPtr = static_cast<const void *>(videoEncodeH265PictureInfos.back().get());
    DE_ASSERT(videoEncodePictureInfoPtr);

    VkVideoReferenceSlotInfoKHR *setupReferenceSlotPtr = nullptr;

    int8_t curSlotIdx                = m_testDefinition->curSlot(nalIdx);
    setupReferenceSlotPtr            = &m_dpbImageVideoReferenceSlots[curSlotIdx];
    setupReferenceSlotPtr->slotIndex = curSlotIdx;

    uint32_t srcPictureResourceIdx = (gopIdx * m_gopFrameCount) + m_testDefinition->frameIdx(nalIdx);

    VkDeviceSize dstBufferOffset;

    // Due to the invert command order dstBufferOffset for P frame is unknown during the recording, set offset to a "safe" values
    if (m_swapOrder)
    {
        if (nalIdx == 0)
            dstBufferOffset = deAlign64(256, m_minBitstreamBufferOffsetAlignment);
        else
            dstBufferOffset = deAlign64(encodeFrameBufferSizeAligned + 256, m_minBitstreamBufferOffsetAlignment);
    }
    else
    {
        dstBufferOffset = m_bitstreamBufferOffset;
    }

    // Set up the pNext chain for various features
    VkBaseInStructure *pStruct = (VkBaseInStructure *)videoEncodePictureInfoPtr;

    de::MovePtr<VkVideoInlineQueryInfoKHR> inlineQueryInfo;
    if (m_useInlineQueries)
    {
        inlineQueryInfo = getVideoInlineQueryInfo(encodeQueryPool.get(), m_queryId, 1, nullptr);
        appendStructurePtrToVulkanChain((const void **)&pStruct->pNext, inlineQueryInfo.get());
    }

    de::MovePtr<VkVideoEncodeQuantizationMapInfoKHR> quantizationMapInfo;
    if (m_useDeltaMap || m_useEmphasisMap)
    {
        quantizationMapInfo = getQuantizationMapInfo(
            m_quantizationMapImageViews[gopIdx % m_quantizationMapCount]->get(), m_quantizationMapExtent);
        appendStructurePtrToVulkanChain((const void **)&pStruct->pNext, quantizationMapInfo.get());
    }

    MovePtr<VkVideoEncodeIntraRefreshInfoKHR> intraRefreshInfo;
    if (m_useIntraRefresh)
    {
        intraRefreshInfo = createIntraRefreshInfo(nalIdx);
        updateReferenceSlotsForIntraRefresh(nalIdx, referenceSlots, refsCount);
        appendStructurePtrToVulkanChain((const void **)&pStruct->pNext, intraRefreshInfo.get());
    }

    // Get encode flags for the current frame
    VkVideoEncodeFlagsKHR encodeFlags = getEncodeFlags(nalIdx);

    m_videoEncodeFrameInfos.push_back(
        getVideoEncodeInfo(videoEncodePictureInfoPtr, encodeFlags, encodeBuffer, dstBufferOffset,
                           (*m_imagePictureResourceVector[srcPictureResourceIdx]), setupReferenceSlotPtr, refsCount,
                           (refsPool == 0) ? nullptr : referenceSlots));

    if (!m_useInlineQueries)
        m_videoDeviceDriver->cmdBeginQuery(encodeCmdBuffer, encodeQueryPool.get(), m_queryId, 0);

    m_videoDeviceDriver->cmdEncodeVideoKHR(encodeCmdBuffer, m_videoEncodeFrameInfos.back().get());

    if (!m_useInlineQueries)
        m_videoDeviceDriver->cmdEndQuery(encodeCmdBuffer, encodeQueryPool.get(), m_queryId);
    m_videoDeviceDriver->cmdEndVideoCodingKHR(encodeCmdBuffer, &videoEndCodingInfo);

    endCommandBuffer(*m_videoDeviceDriver, encodeCmdBuffer);

    if (!m_swapOrder)
    {
        submitCommandsAndWait(*m_videoDeviceDriver, m_videoEncodeDevice, m_encodeQueue, encodeCmdBuffer);

        if (!processQueryPoolResults(*m_videoDeviceDriver, m_videoEncodeDevice, encodeQueryPool.get(), m_queryId, 1,
                                     m_bitstreamBufferOffset, m_minBitstreamBufferOffsetAlignment, m_queryStatus))
            throw tcu::TestStatus::fail("Unexpected query result status");
    }
}

void VideoEncodeTestInstance::handleSwapOrderSubmission(Move<VkQueryPool> &encodeQueryPool)
{
    Move<VkSemaphore> frameEncodedSemaphore     = createSemaphore(*m_videoDeviceDriver, m_videoEncodeDevice);
    const VkPipelineStageFlags waitDstStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;

    const auto firstCommandFence =
        submitCommands(*m_videoDeviceDriver, m_videoEncodeDevice, m_encodeQueue, *m_firstEncodeCmdBuffer, false, 1U, 0,
                       nullptr, nullptr, 1, &frameEncodedSemaphore.get());
    waitForFence(*m_videoDeviceDriver, m_videoEncodeDevice, *firstCommandFence);

    if (!processQueryPoolResults(*m_videoDeviceDriver, m_videoEncodeDevice, encodeQueryPool.get(), m_queryId, 1,
                                 m_bitstreamBufferOffset, m_minBitstreamBufferOffsetAlignment, m_queryStatus))
        throw tcu::TestStatus::fail("Unexpected query result status");

    const auto secondCommandFence =
        submitCommands(*m_videoDeviceDriver, m_videoEncodeDevice, m_encodeQueue, *m_secondEncodeCmdBuffer, false, 1U, 1,
                       &frameEncodedSemaphore.get(), &waitDstStageMask);
    waitForFence(*m_videoDeviceDriver, m_videoEncodeDevice, *secondCommandFence);

    if (!processQueryPoolResults(*m_videoDeviceDriver, m_videoEncodeDevice, encodeQueryPool.get(), m_queryId, 1,
                                 m_bitstreamBufferOffset, m_minBitstreamBufferOffsetAlignment, m_queryStatus))
        throw tcu::TestStatus::fail("Unexpected query result status");
}

tcu::TestStatus VideoEncodeTestInstance::verifyEncodedBitstream(const BufferWithMemory &encodeBuffer,
                                                                VkDeviceSize encodeBufferSize)
{
    if (m_dumpOutput & tcu::DUMP_ENC_BITSTREAM)
    {
        auto outputFileName = string("out_") + getTestName(m_testDefinition->getTestType());

        if (m_testDefinition->getProfile()->IsH264())
            outputFileName += ".h264";
        else if (m_testDefinition->getProfile()->IsH265())
            outputFileName += ".h265";

        saveBufferAsFile(encodeBuffer, encodeBufferSize, outputFileName);
    }

    // Vulkan video is not supported on android platform
    // all external libraries, helper functions and test instances has been excluded
#ifdef DE_BUILD_VIDEO
    DeviceContext deviceContext(&m_context, &m_videoDevice, m_physicalDevice, m_videoEncodeDevice, m_decodeQueue,
                                m_encodeQueue, m_transferQueue);

    const Unique<VkCommandPool> decodeCmdPool(
        makeCommandPool(*m_videoDeviceDriver, m_videoEncodeDevice, m_decodeQueueFamilyIndex));
    const Unique<VkCommandBuffer> decodeCmdBuffer(allocateCommandBuffer(
        *m_videoDeviceDriver, m_videoEncodeDevice, *decodeCmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

    uint32_t H264profileIdc = STD_VIDEO_H264_PROFILE_IDC_MAIN;
    uint32_t H265profileIdc = STD_VIDEO_H265_PROFILE_IDC_MAIN;

    uint32_t profileIdc = 0;

    if (m_testDefinition->getProfile()->IsH264())
        profileIdc = H264profileIdc;
    else if (m_testDefinition->getProfile()->IsH265())
        profileIdc = H265profileIdc;
    DE_ASSERT(profileIdc);

    auto decodeProfile =
        VkVideoCoreProfile(m_videoCodecDecodeOperation, VK_VIDEO_CHROMA_SUBSAMPLING_420_BIT_KHR,
                           VK_VIDEO_COMPONENT_BIT_DEPTH_8_BIT_KHR, VK_VIDEO_COMPONENT_BIT_DEPTH_8_BIT_KHR, profileIdc);

    // Use the actual frame count processed rather than the pattern definition
    uint32_t actualFramesToCheck = m_gopCount * m_gopFrameCount;

    auto basicDecoder = createBasicDecoder(&deviceContext, &decodeProfile, actualFramesToCheck, m_resolutionChange);

    Demuxer::Params demuxParams = {};
    demuxParams.data            = std::make_unique<BufferedReader>(
        static_cast<const char *>(encodeBuffer.getAllocation().getHostPtr()), encodeBufferSize);
    demuxParams.codecOperation = m_videoCodecDecodeOperation;
    demuxParams.framing        = ElementaryStreamFraming::H26X_BYTE_STREAM;
    auto demuxer               = Demuxer::create(std::move(demuxParams));
    VkVideoParser parser;
    // TODO: Check for decoder extension support before attempting validation!
    createParser(demuxer->codecOperation(), basicDecoder, parser, demuxer->framing());

    FrameProcessor processor(std::move(demuxer), basicDecoder);
    std::vector<int> incorrectFrames;
    std::vector<int> correctFrames;
    std::vector<double> psnrDiff;

    // Log how many frames we expect to process
    m_context.getTestContext().getLog() << tcu::TestLog::Message << "Expecting to verify " << actualFramesToCheck
                                        << " frames" << tcu::TestLog::EndMessage;

    for (uint32_t NALIdx = 0; NALIdx < actualFramesToCheck; NALIdx++)
    {
        DecodedFrame frame;
        const auto gotFrame = processor.getNextFrame(&frame);
        TCU_CHECK_AND_THROW(
            InternalError, gotFrame > 0,
            "Expected more frames from the bitstream. Most likely an internal CTS bug, or maybe an invalid bitstream");

        VkImageLayout layout;
        if (m_testDefinition->usesGeneralLayout())
            layout = VK_IMAGE_LAYOUT_GENERAL;
        else
            layout = basicDecoder->dpbAndOutputCoincide() ? VK_IMAGE_LAYOUT_VIDEO_DECODE_DPB_KHR :
                                                            VK_IMAGE_LAYOUT_VIDEO_DECODE_DST_KHR;

        auto resultImage = getDecodedImageFromContext(deviceContext, layout, &frame);
        processor.releaseFrame(&frame);
        de::MovePtr<std::vector<uint8_t>> out =
            vkt::ycbcr::YCbCrConvUtil<uint8_t>::MultiPlanarNV12toI420(resultImage.get());

        if (m_dumpOutput & tcu::DUMP_ENC_YUV)
        {
            const string outputFileName = "out_" + std::to_string(NALIdx) + ".yuv";
            vkt::ycbcr::YCbCrContent<uint8_t>::save(*out, outputFileName);
        }

        // Quantization maps verification
        if (m_useDeltaMap || m_useEmphasisMap)
        {
            double d = util::calculatePSNRdifference(*m_inVector[NALIdx], *out, m_codedExtent, m_quantizationMapExtent,
                                                     m_quantizationMapTexelSize);

            psnrDiff.push_back(d);

            if (m_useEmphasisMap && NALIdx == 1)
            {
                if (psnrDiff[1] <= psnrDiff[0])
                    return tcu::TestStatus::fail(
                        "PSNR difference for the second frame is not greater than for the first frame");
            }
            else if (m_useDeltaMap && NALIdx == 2)
            {
                if (psnrDiff[2] > 0)
                    return tcu::TestStatus::fail(
                        "PSNR value for left half of the frame is lower than for the right half");
            }
        }

        double higherPsnrThreshold     = 30.0;
        double lowerPsnrThreshold      = 20.0;
        double criticalPsnrThreshold   = 10;
        double psnrThresholdLowerLimit = m_disableRateControl ? lowerPsnrThreshold : higherPsnrThreshold;
        string failMessage;

        double psnr = util::PSNR(*m_inVector[NALIdx], *out);

        // Quality checks
        if (psnr < psnrThresholdLowerLimit)
        {
            double difference = psnrThresholdLowerLimit - psnr;

            if ((m_useDeltaMap || m_useEmphasisMap) && NALIdx == 1)
            {
                // When testing quantization map, the PSNR of the secont image is expected to be low
                break;
            }
            if (psnr > criticalPsnrThreshold)
            {
                failMessage = "Frame " + std::to_string(NALIdx) + " with PSNR " + std::to_string(psnr) + " is " +
                              std::to_string(difference) + " points below the lower threshold";
                return tcu::TestStatus(QP_TEST_RESULT_QUALITY_WARNING, failMessage);
            }
            else
            {
                failMessage = "Frame " + std::to_string(NALIdx) + " with PSNR " + std::to_string(psnr) + " is " +
                              std::to_string(difference) + " points below the critical threshold";
                return tcu::TestStatus::fail(failMessage);
            }
        }
    }

    const string passMessage = std::to_string(actualFramesToCheck) + " correctly encoded frames";
    return tcu::TestStatus::pass(passMessage);
#else
    DE_UNREF(encodeBuffer);
    DE_UNREF(encodeBufferSize);
    TCU_THROW(NotSupportedError, "Vulkan video is not supported on android platform");
#endif
}

void VideoEncodeTestInstance::prepareEncodeBuffer(void)
{
    const vector<uint32_t> encodeQueueFamilyIndices(1u, m_encodeQueueFamilyIndex);

    const VkBufferUsageFlags encodeBufferUsageFlags =
        VK_BUFFER_USAGE_VIDEO_ENCODE_DST_BIT_KHR | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    const VkDeviceSize encodeFrameBufferSize = getBufferSize(m_imageFormat, m_codedExtent.width, m_codedExtent.height);
    m_encodeFrameBufferSizeAligned =
        deAlign64(encodeFrameBufferSize, m_videoCapabilities->minBitstreamBufferSizeAlignment);
    m_encodeBufferSize = m_encodeFrameBufferSizeAligned * m_gopFrameCount * m_gopCount;

    const VkBufferCreateInfo encodeBufferCreateInfo = makeBufferCreateInfo(
        m_encodeBufferSize, encodeBufferUsageFlags, encodeQueueFamilyIndices, 0, m_videoEncodeProfileList.get());

    m_encodeBuffer = std::make_unique<BufferWithMemory>(*m_videoDeviceDriver, m_videoEncodeDevice, getAllocator(),
                                                        encodeBufferCreateInfo,
                                                        MemoryRequirement::Local | MemoryRequirement::HostVisible);

    Allocation &encodeBufferAlloc = m_encodeBuffer.get()->getAllocation();
    void *encodeBufferHostPtr     = encodeBufferAlloc.getHostPtr();

    m_encodeQueryPool =
        createEncodeVideoQueries(*m_videoDeviceDriver, m_videoEncodeDevice, 2, m_videoEncodeProfile.get());

    deMemset(encodeBufferHostPtr, 0x00, static_cast<size_t>(m_encodeBufferSize));
    flushAlloc(*m_videoDeviceDriver, m_videoEncodeDevice, encodeBufferAlloc);
}

void VideoEncodeTestInstance::queryIntraRefreshCapabilities(void)
{
    if (!m_useIntraRefresh)
        return;

    // Check if the requested intra refresh mode is supported
    VkVideoEncodeIntraRefreshModeFlagsKHR supportedModes = m_videoEncodeIntraRefreshCapabilities->intraRefreshModes;

    if (!(supportedModes & m_intraRefreshMode))
    {
        const char *modeStr = "unknown";
        switch (m_intraRefreshMode)
        {
        case VK_VIDEO_ENCODE_INTRA_REFRESH_MODE_PER_PICTURE_PARTITION_BIT_KHR:
            modeStr = "Per-picture partition";
            break;
        case VK_VIDEO_ENCODE_INTRA_REFRESH_MODE_BLOCK_BASED_BIT_KHR:
            modeStr = "Block-based";
            break;
        case VK_VIDEO_ENCODE_INTRA_REFRESH_MODE_BLOCK_ROW_BASED_BIT_KHR:
            modeStr = "Block row-based";
            break;
        case VK_VIDEO_ENCODE_INTRA_REFRESH_MODE_BLOCK_COLUMN_BASED_BIT_KHR:
            modeStr = "Block column-based";
            break;
        case VK_VIDEO_ENCODE_INTRA_REFRESH_MODE_NONE_KHR:
            modeStr = "None";
            break;
        default:
            break;
        }
        TCU_THROW(NotSupportedError, (std::string(modeStr) + " intra refresh mode not supported").c_str());
    }

    // Calculate intraRefreshRegionCount based on the mode and codec
    VkExtent2D minCodingBlockSize           = {};
    VkExtent2D codedExtentInMinCodingBlocks = {};
    uint32_t maxCodecPartitions             = 0;
    uint32_t maxPartitionsInBlocks          = 0;

    if (m_testDefinition->getProfile()->IsH264())
    {
        // H.264: min coding block size is 16x16
        minCodingBlockSize = {16, 16};

        // Calculate coded extent in min coding blocks
        codedExtentInMinCodingBlocks.width =
            (m_codedExtent.width + minCodingBlockSize.width - 1) / minCodingBlockSize.width;
        codedExtentInMinCodingBlocks.height =
            (m_codedExtent.height + minCodingBlockSize.height - 1) / minCodingBlockSize.height;

        // Get max slice count
        maxCodecPartitions = m_videoH264CapabilitiesExtension->maxSliceCount;

        // Calculate max partitions in blocks based on ROW_UNALIGNED_SLICE capability
        if (m_videoH264CapabilitiesExtension->flags & VK_VIDEO_ENCODE_H264_CAPABILITY_ROW_UNALIGNED_SLICE_BIT_KHR)
            maxPartitionsInBlocks = codedExtentInMinCodingBlocks.width * codedExtentInMinCodingBlocks.height;
        else
            maxPartitionsInBlocks = codedExtentInMinCodingBlocks.height;
    }
    else if (m_testDefinition->getProfile()->IsH265())
    {
        if ((m_videoH265CapabilitiesExtension->ctbSizes & VK_VIDEO_ENCODE_H265_CTB_SIZE_16_BIT_KHR) != 0)
        {
            minCodingBlockSize = {16, 16};
        }
        else if ((m_videoH265CapabilitiesExtension->ctbSizes & VK_VIDEO_ENCODE_H265_CTB_SIZE_32_BIT_KHR) != 0)
        {
            minCodingBlockSize = {32, 32};
        }
        else
        {
            TCU_CHECK_AND_THROW(
                InternalError,
                (m_videoH265CapabilitiesExtension->ctbSizes & VK_VIDEO_ENCODE_H265_CTB_SIZE_64_BIT_KHR) != 0,
                "H.265 CTB size 64 must be supported");
            minCodingBlockSize = {64, 64};
        }

        // Calculate coded extent in min coding blocks
        codedExtentInMinCodingBlocks.width =
            (m_codedExtent.width + minCodingBlockSize.width - 1) / minCodingBlockSize.width;
        codedExtentInMinCodingBlocks.height =
            (m_codedExtent.height + minCodingBlockSize.height - 1) / minCodingBlockSize.height;

        // Get max slice segment count
        maxCodecPartitions = m_videoH265CapabilitiesExtension->maxSliceSegmentCount;

        // Calculate max partitions in blocks based on ROW_UNALIGNED_SLICE_SEGMENT capability
        if (m_videoH265CapabilitiesExtension->flags &
            VK_VIDEO_ENCODE_H265_CAPABILITY_ROW_UNALIGNED_SLICE_SEGMENT_BIT_KHR)
        {
            maxPartitionsInBlocks = codedExtentInMinCodingBlocks.width * codedExtentInMinCodingBlocks.height;
        }
        else
        {
            maxPartitionsInBlocks = codedExtentInMinCodingBlocks.height;
        }
    }

    uint32_t maxPicturePartitions = 0;

    switch (m_intraRefreshMode)
    {
    case VK_VIDEO_ENCODE_INTRA_REFRESH_MODE_PER_PICTURE_PARTITION_BIT_KHR:
        maxPicturePartitions = std::min(maxCodecPartitions, maxPartitionsInBlocks);
        break;
    case VK_VIDEO_ENCODE_INTRA_REFRESH_MODE_BLOCK_ROW_BASED_BIT_KHR:
        maxPicturePartitions = codedExtentInMinCodingBlocks.height;
        break;
    case VK_VIDEO_ENCODE_INTRA_REFRESH_MODE_BLOCK_COLUMN_BASED_BIT_KHR:
        maxPicturePartitions = codedExtentInMinCodingBlocks.width;
        break;
    case VK_VIDEO_ENCODE_INTRA_REFRESH_MODE_BLOCK_BASED_BIT_KHR:
        maxPicturePartitions = codedExtentInMinCodingBlocks.width * codedExtentInMinCodingBlocks.height;
        break;
    default:
        maxPicturePartitions = 0;
        break;
    }

    // Calculate intraRefreshRegionCount and intraRefreshCycleDuration
    m_intraRefreshRegionCount =
        std::min(m_videoEncodeIntraRefreshCapabilities->maxIntraRefreshCycleDuration, maxPicturePartitions);

    // For per-picture partition mode, further limit based on rectangular region constraints
    if (m_intraRefreshMode == VK_VIDEO_ENCODE_INTRA_REFRESH_MODE_PER_PICTURE_PARTITION_BIT_KHR &&
        !m_videoEncodeIntraRefreshCapabilities->nonRectangularIntraRefreshRegions)
    {
        uint32_t maxRectangularPartitions = 0;
        if (m_testDefinition->getProfile()->IsH264())
        {
            // H.264: Limited by macroblock rows (16x16)
            uint32_t mbHeight        = 16;
            maxRectangularPartitions = (m_codedExtent.height + mbHeight - 1) / mbHeight;
        }
        else if (m_testDefinition->getProfile()->IsH265())
        {
            // H.265: Limited by CTU rows (assume 64x64 CTU)
            uint32_t ctuHeight       = 64;
            maxRectangularPartitions = (m_codedExtent.height + ctuHeight - 1) / ctuHeight;
        }

        if (maxRectangularPartitions > 0)
        {
            m_intraRefreshRegionCount = std::min(m_intraRefreshRegionCount, maxRectangularPartitions);
        }
    }

    // For empty-region tests, use maxIntraRefreshCycleDuration to create empty region
    if (m_intraRefreshEmptyRegion)
    {
        m_intraRefreshCycleDuration = m_videoEncodeIntraRefreshCapabilities->maxIntraRefreshCycleDuration;
        m_intraRefreshRegionCount   = 1; // Only one frame with empty intra refresh
    }
    else if (m_intraRefreshMidway)
    {
        // For mid-way tests, set cycle duration to 4 and region count to 6 (to cover all 7 frames - 1 IDR)
        m_intraRefreshCycleDuration = 4;
        m_intraRefreshRegionCount   = 6; // Frames 1-6 (after IDR frame 0)
        // Ensure the implementation supports at least cycle duration of 4
        if (m_videoEncodeIntraRefreshCapabilities->maxIntraRefreshCycleDuration < 4)
        {
            TCU_THROW(NotSupportedError,
                      "Implementation does not support intra refresh cycle duration of 4 or greater");
        }
    }
    else
    {
        m_intraRefreshCycleDuration = m_intraRefreshRegionCount;
        // For basic intra-refresh tests, the GOP frame count is clamped to the cycle duration plus one IDR frame.
        m_gopFrameCount = std::min(m_gopFrameCount, m_intraRefreshCycleDuration + 1);
    }
}

MovePtr<VkVideoEncodeIntraRefreshInfoKHR> VideoEncodeTestInstance::createIntraRefreshInfo(uint32_t nalIdx)
{
    if (!m_useIntraRefresh || nalIdx == 0)
        return MovePtr<VkVideoEncodeIntraRefreshInfoKHR>(); // Return empty MovePtr instead of nullptr

    // For normal intra refresh tests, check region count
    if (!m_intraRefreshEmptyRegion && !m_intraRefreshMidway && nalIdx > m_intraRefreshRegionCount)
        return MovePtr<VkVideoEncodeIntraRefreshInfoKHR>();

    // For midway tests, check that we're within the 6 frames that have intra refresh
    if (m_intraRefreshMidway && nalIdx > 6)
        return MovePtr<VkVideoEncodeIntraRefreshInfoKHR>();

    MovePtr<VkVideoEncodeIntraRefreshInfoKHR> intraRefreshInfo(new VkVideoEncodeIntraRefreshInfoKHR());
    intraRefreshInfo->sType                     = VK_STRUCTURE_TYPE_VIDEO_ENCODE_INTRA_REFRESH_INFO_KHR;
    intraRefreshInfo->pNext                     = nullptr;
    intraRefreshInfo->intraRefreshCycleDuration = m_intraRefreshCycleDuration;
    intraRefreshInfo->intraRefreshIndex         = getIntraRefreshIndex(nalIdx);

    return intraRefreshInfo;
}

void VideoEncodeTestInstance::updateReferenceSlotsForIntraRefresh(uint32_t nalIdx,
                                                                  VkVideoReferenceSlotInfoKHR *referenceSlots,
                                                                  uint8_t refsCount)
{
    if (!m_useIntraRefresh || nalIdx <= 1 || nalIdx > m_intraRefreshRegionCount)
        return;

    // dirtyIntraRefreshRegions = intraRefreshCycleDuration - intraRefreshIndex
    uint32_t currentIntraRefreshIndex = getIntraRefreshIndex(nalIdx);

    // Only frames after the first intra refresh frame need reference intra refresh info
    MovePtr<VkVideoReferenceIntraRefreshInfoKHR> referenceIntraRefreshInfo(new VkVideoReferenceIntraRefreshInfoKHR());
    referenceIntraRefreshInfo->sType                    = VK_STRUCTURE_TYPE_VIDEO_REFERENCE_INTRA_REFRESH_INFO_KHR;
    referenceIntraRefreshInfo->pNext                    = nullptr;
    referenceIntraRefreshInfo->dirtyIntraRefreshRegions = m_intraRefreshCycleDuration - currentIntraRefreshIndex;

    // Add the reference intra refresh info to the immediately preceding reference frame
    if (refsCount > 0)
    {
        VkVideoReferenceSlotInfoKHR &refSlot = referenceSlots[0];

        // Save the original pNext
        const void *origPNext = refSlot.pNext;

        // Update pNext to include referenceIntraRefreshInfo
        referenceIntraRefreshInfo->pNext = origPNext;

        // Store the pointer in the reference slot
        refSlot.pNext = referenceIntraRefreshInfo.get();

        // Store the MovePtr for cleanup later
        m_referenceIntraRefreshInfos.push_back(std::move(referenceIntraRefreshInfo));
    }
}

// Updates encode flags to include intra refresh if needed
VkVideoEncodeFlagsKHR VideoEncodeTestInstance::getEncodeFlags(uint32_t nalIdx)
{
    VkVideoEncodeFlagsKHR encodeFlags = 0;

    if (m_useDeltaMap)
        encodeFlags |= VK_VIDEO_ENCODE_WITH_QUANTIZATION_DELTA_MAP_BIT_KHR;
    else if (m_useEmphasisMap)
        encodeFlags |= VK_VIDEO_ENCODE_WITH_EMPHASIS_MAP_BIT_KHR;

    if (m_useIntraRefresh && nalIdx > 0 && nalIdx <= m_intraRefreshRegionCount)
        encodeFlags |= VK_VIDEO_ENCODE_INTRA_REFRESH_BIT_KHR;

    return encodeFlags;
}

uint32_t VideoEncodeTestInstance::getIntraRefreshIndex(uint32_t nalIdx) const
{
    if (m_intraRefreshMidway)
    {
        // For mid-way tests:
        // - Frames 1-2: first cycle (indices 0, 1)
        // - Frame 3: start new cycle (index 0)
        // - Frames 4-6: continue new cycle (indices 1, 2, 3)
        if (nalIdx <= 2)
            return nalIdx - 1; // Index 0, 1
        else if (nalIdx == 3)
            return 0; // Start new cycle
        else
            return nalIdx - 3; // Index 1, 2, 3 for frames 4, 5, 6
    }
    else
    {
        // For normal intra refresh tests
        return nalIdx - 1; // Index 0 is the first intra refresh frame (after IDR)
    }
}

uint32_t VideoEncodeTestInstance::calculateTotalFramesFromClipData(const std::vector<uint8_t> &clip, uint32_t width,
                                                                   uint32_t height)
{
    // Calculate frame size in bytes for YUV 4:2:0 format
    size_t frameSize = width * height * 3 / 2; // Y: width*height, U/V: width*height/4 each
    DE_ASSERT(frameSize > 0);
    // Calculate the maximum number of complete frames in the clip
    size_t maxFrames = static_cast<uint32_t>(clip.size() / frameSize);
    DE_ASSERT(maxFrames <= UINT32_MAX);

    return static_cast<uint32_t>(maxFrames);
}

tcu::TestStatus VideoEncodeTestInstance::iterate(void)
{
    initializeTestParameters();
    setupDeviceAndQueues();
    queryAndValidateCapabilities();
    createVideoSession();
    setupQuantizationMapResources();
    setupSessionParameters();
    prepareDPBResources();
    prepareInputImages();
    prepareEncodeBuffer();
    loadVideoFrames();
    setupRateControl();
    getSessionParametersHeaders();
    setupCommandBuffers();
    encodeFrames();
    if (m_swapOrder)
        handleSwapOrderSubmission(m_encodeQueryPool);
    return verifyEncodedBitstream(*m_encodeBuffer.get(), m_encodeBufferSize);
}

class VideoEncodeTestCase : public TestCase
{
public:
    VideoEncodeTestCase(tcu::TestContext &context, const char *name, MovePtr<TestDefinition> testDefinition);
    ~VideoEncodeTestCase(void);

    virtual TestInstance *createInstance(Context &context) const;
    virtual void checkSupport(Context &context) const;

private:
    MovePtr<TestDefinition> m_testDefinition;
};

VideoEncodeTestCase::VideoEncodeTestCase(tcu::TestContext &context, const char *name,
                                         MovePtr<TestDefinition> testDefinition)
    : vkt::TestCase(context, name)
    , m_testDefinition(testDefinition)
{
}

VideoEncodeTestCase::~VideoEncodeTestCase(void)
{
}

void VideoEncodeTestCase::checkSupport(Context &context) const
{
    context.requireDeviceFunctionality("VK_KHR_video_queue");
    context.requireDeviceFunctionality("VK_KHR_synchronization2");
    context.requireDeviceFunctionality("VK_KHR_video_encode_queue");

    switch (m_testDefinition->getTestType())
    {
    case TEST_TYPE_H264_ENCODE_I:
    case TEST_TYPE_H264_ENCODE_RC_VBR:
    case TEST_TYPE_H264_ENCODE_RC_CBR:
    case TEST_TYPE_H264_ENCODE_RC_DISABLE:
    case TEST_TYPE_H264_ENCODE_QUALITY_LEVEL:
    case TEST_TYPE_H264_ENCODE_USAGE:
    case TEST_TYPE_H264_ENCODE_I_P:
    case TEST_TYPE_H264_ENCODE_I_P_NOT_MATCHING_ORDER:
    case TEST_TYPE_H264_I_P_B_13:
    case TEST_TYPE_H264_ENCODE_RESOLUTION_CHANGE_DPB:
    case TEST_TYPE_H264_ENCODE_QUERY_RESULT_WITH_STATUS:
        context.requireDeviceFunctionality("VK_KHR_video_encode_h264");
        break;
    case TEST_TYPE_H264_ENCODE_INLINE_QUERY:
    case TEST_TYPE_H264_ENCODE_RESOURCES_WITHOUT_PROFILES:
        context.requireDeviceFunctionality("VK_KHR_video_encode_h264");
        context.requireDeviceFunctionality("VK_KHR_video_maintenance1");
        break;
    case TEST_TYPE_H264_ENCODE_QM_DELTA_RC_DISABLE:
    case TEST_TYPE_H264_ENCODE_QM_DELTA_RC_VBR:
    case TEST_TYPE_H264_ENCODE_QM_DELTA_RC_CBR:
    case TEST_TYPE_H264_ENCODE_QM_DELTA:
    case TEST_TYPE_H264_ENCODE_QM_EMPHASIS_CBR:
    case TEST_TYPE_H264_ENCODE_QM_EMPHASIS_VBR:
        context.requireDeviceFunctionality("VK_KHR_video_encode_h264");
        context.requireDeviceFunctionality("VK_KHR_video_encode_quantization_map");
        break;
    case TEST_TYPE_H264_ENCODE_INTRA_REFRESH_PICTURE_PARTITION:
    case TEST_TYPE_H264_ENCODE_INTRA_REFRESH_ANY_BLOCK_BASED:
    case TEST_TYPE_H264_ENCODE_INTRA_REFRESH_ROW_BASED:
    case TEST_TYPE_H264_ENCODE_INTRA_REFRESH_COLUMN_BASED:
    case TEST_TYPE_H264_ENCODE_INTRA_REFRESH_ANY_BLOCK_BASED_EMPTY_REGION:
    case TEST_TYPE_H264_ENCODE_INTRA_REFRESH_ROW_BASED_EMPTY_REGION:
    case TEST_TYPE_H264_ENCODE_INTRA_REFRESH_COLUMN_BASED_EMPTY_REGION:
    case TEST_TYPE_H264_ENCODE_INTRA_REFRESH_PICTURE_PARTITION_MIDWAY:
    case TEST_TYPE_H264_ENCODE_INTRA_REFRESH_ANY_BLOCK_BASED_MIDWAY:
    case TEST_TYPE_H264_ENCODE_INTRA_REFRESH_ROW_BASED_MIDWAY:
    case TEST_TYPE_H264_ENCODE_INTRA_REFRESH_COLUMN_BASED_MIDWAY:
        context.requireDeviceFunctionality("VK_KHR_video_encode_h264");
        context.requireDeviceFunctionality("VK_KHR_video_encode_intra_refresh");
        break;
    case TEST_TYPE_H265_ENCODE_I:
    case TEST_TYPE_H265_ENCODE_RC_VBR:
    case TEST_TYPE_H265_ENCODE_RC_CBR:
    case TEST_TYPE_H265_ENCODE_RC_DISABLE:
    case TEST_TYPE_H265_ENCODE_QUALITY_LEVEL:
    case TEST_TYPE_H265_ENCODE_USAGE:
    case TEST_TYPE_H265_ENCODE_I_P:
    case TEST_TYPE_H265_ENCODE_I_P_NOT_MATCHING_ORDER:
    case TEST_TYPE_H265_I_P_B_13:
    case TEST_TYPE_H265_ENCODE_RESOLUTION_CHANGE_DPB:
    case TEST_TYPE_H265_ENCODE_QUERY_RESULT_WITH_STATUS:
        context.requireDeviceFunctionality("VK_KHR_video_encode_h265");
        break;
    case TEST_TYPE_H265_ENCODE_INLINE_QUERY:
    case TEST_TYPE_H265_ENCODE_RESOURCES_WITHOUT_PROFILES:
        context.requireDeviceFunctionality("VK_KHR_video_encode_h265");
        context.requireDeviceFunctionality("VK_KHR_video_maintenance1");
        break;
    case TEST_TYPE_H265_ENCODE_QM_DELTA_RC_DISABLE:
    case TEST_TYPE_H265_ENCODE_QM_DELTA_RC_VBR:
    case TEST_TYPE_H265_ENCODE_QM_DELTA_RC_CBR:
    case TEST_TYPE_H265_ENCODE_QM_DELTA:
    case TEST_TYPE_H265_ENCODE_QM_EMPHASIS_CBR:
    case TEST_TYPE_H265_ENCODE_QM_EMPHASIS_VBR:
        context.requireDeviceFunctionality("VK_KHR_video_encode_h265");
        context.requireDeviceFunctionality("VK_KHR_video_encode_quantization_map");
        break;
    case TEST_TYPE_H265_ENCODE_INTRA_REFRESH_PICTURE_PARTITION:
    case TEST_TYPE_H265_ENCODE_INTRA_REFRESH_ANY_BLOCK_BASED:
    case TEST_TYPE_H265_ENCODE_INTRA_REFRESH_ROW_BASED:
    case TEST_TYPE_H265_ENCODE_INTRA_REFRESH_COLUMN_BASED:
    case TEST_TYPE_H265_ENCODE_INTRA_REFRESH_ANY_BLOCK_BASED_EMPTY_REGION:
    case TEST_TYPE_H265_ENCODE_INTRA_REFRESH_ROW_BASED_EMPTY_REGION:
    case TEST_TYPE_H265_ENCODE_INTRA_REFRESH_COLUMN_BASED_EMPTY_REGION:
    case TEST_TYPE_H265_ENCODE_INTRA_REFRESH_PICTURE_PARTITION_MIDWAY:
    case TEST_TYPE_H265_ENCODE_INTRA_REFRESH_ANY_BLOCK_BASED_MIDWAY:
    case TEST_TYPE_H265_ENCODE_INTRA_REFRESH_ROW_BASED_MIDWAY:
    case TEST_TYPE_H265_ENCODE_INTRA_REFRESH_COLUMN_BASED_MIDWAY:
        context.requireDeviceFunctionality("VK_KHR_video_encode_h265");
        context.requireDeviceFunctionality("VK_KHR_video_encode_intra_refresh");
        break;
    default:
        TCU_THROW(InternalError, "Unknown TestType");
    }

    if (m_testDefinition->usesGeneralLayout() == VK_IMAGE_LAYOUT_GENERAL)
    {
        context.requireDeviceFunctionality("VK_KHR_unified_image_layouts");
        if (!context.getUnifiedImageLayoutsFeatures().unifiedImageLayoutsVideo)
        {
            TCU_THROW(NotSupportedError, "unifiedImageLayoutsVideo");
        }
    }
}

TestInstance *VideoEncodeTestCase::createInstance(Context &context) const
{
#ifdef DE_BUILD_VIDEO
    return new VideoEncodeTestInstance(context, m_testDefinition.get());
#else
    // Vulkan video is not supported on android platform
    DE_UNREF(context);
    return nullptr;
#endif
}

} // namespace

tcu::TestCaseGroup *createVideoEncodeTests(tcu::TestContext &testCtx)
{
    MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(testCtx, "encode", "Video encoding session tests"));

    MovePtr<tcu::TestCaseGroup> h264Group(new tcu::TestCaseGroup(testCtx, "h264", "H.264 video codec"));
    MovePtr<tcu::TestCaseGroup> h265Group(new tcu::TestCaseGroup(testCtx, "h265", "H.265 video codec"));

    for (bool layeredSrc : {true, false})
    {
        for (bool generalLayout : {true, false})
        {
            for (const auto &encodeTest : g_EncodeTests)
            {
                auto defn = TestDefinition::create(encodeTest, layeredSrc, generalLayout);

                std::string testName = std::string(getTestName(defn->getTestType())) +
                                       std::string(layeredSrc ? "_layered_src" : "_separated_src") +
                                       std::string(generalLayout ? "_general_layout" : "_video_layout");
                auto testCodec = getTestCodec(defn->getTestType());

                if (testCodec == TEST_CODEC_H264)
                    h264Group->addChild(new VideoEncodeTestCase(testCtx, testName.c_str(), defn));
                else if (testCodec == TEST_CODEC_H265)
                    h265Group->addChild(new VideoEncodeTestCase(testCtx, testName.c_str(), defn));
                else
                {
                    TCU_THROW(InternalError, "Unknown Video Codec");
                }
            }
        }
    }

    group->addChild(h264Group.release());
    group->addChild(h265Group.release());
    group->addChild(createVideoEncodeTestsAV1(testCtx));

    return group.release();
}
} // namespace video
} // namespace vkt
