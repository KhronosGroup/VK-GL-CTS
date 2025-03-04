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

#ifndef STREAM_DUMP_DEBUG
#define STREAM_DUMP_DEBUG 0
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
    UseVariableBitrateControl = 1 << 1,
    UseConstantBitrateControl = 1 << 2,
    SwapOrder                 = 1 << 3,
    DisableRateControl        = 1 << 4, // const QP
    ResolutionChange          = 1 << 5,
    UseQualityLevel           = 1 << 6,
    UseEncodeUsage            = 1 << 7,
    UseInlineQueries          = 1 << 8,  // Inline queries from the video_mainteance1 extension.
    ResourcesWithoutProfiles  = 1 << 9,  // Test profile-less resources from the video_mainteance1 extension.
    UseDeltaMap               = 1 << 10, // VK_KHR_video_encode_quantization_map
    UseEmphasisMap            = 1 << 11, // VK_KHR_video_encode_quantization_map
};

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
     /* curSlot */ {0, 1, -1, -1, 2, -1, -1, 3, -1, -1, 4, -1, -1, 5},
     /* frameReferences */
     {refs<std::vector<uint8_t>>({}, {}), refs<std::vector<uint8_t>>({0}, {}),
      refs<std::vector<uint8_t>>({0, 1}, {1, 0}), refs<std::vector<uint8_t>>({0, 1}, {1, 0}),
      refs<std::vector<uint8_t>>({1, 0}, {}), refs<std::vector<uint8_t>>({1, 0}, {2, 1}),
      refs<std::vector<uint8_t>>({1, 0}, {2, 1}), refs<std::vector<uint8_t>>({2, 1}, {}),
      refs<std::vector<uint8_t>>({2, 1}, {3, 2}), refs<std::vector<uint8_t>>({2, 1}, {3, 2}),
      refs<std::vector<uint8_t>>({3, 2}, {}), refs<std::vector<uint8_t>>({3, 2}, {4, 3}),
      refs<std::vector<uint8_t>>({3, 2}, {4, 3}), refs<std::vector<uint8_t>>({4, 3}, {})},
     /* encoderOptions */ Option::Default},
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
     /* curSlot */ {0, 1, -1, -1, 2, -1, -1, 3, -1, -1, 4, -1, -1, 5},
     /* frameReferences */
     {refs<std::vector<uint8_t>>({}, {}), refs<std::vector<uint8_t>>({0}, {}),
      refs<std::vector<uint8_t>>({0, 1}, {1, 0}), refs<std::vector<uint8_t>>({0, 1}, {1, 0}),
      refs<std::vector<uint8_t>>({1, 0}, {}), refs<std::vector<uint8_t>>({1, 0}, {2, 1}),
      refs<std::vector<uint8_t>>({1, 0}, {2, 1}), refs<std::vector<uint8_t>>({2, 1}, {}),
      refs<std::vector<uint8_t>>({2, 1}, {3, 2}), refs<std::vector<uint8_t>>({2, 1}, {3, 2}),
      refs<std::vector<uint8_t>>({3, 2}, {}), refs<std::vector<uint8_t>>({3, 2}, {4, 3}),
      refs<std::vector<uint8_t>>({3, 2}, {4, 3}), refs<std::vector<uint8_t>>({4, 3}, {})},
     /* encoderOptions */ Option::Default},
};

class TestDefinition
{
public:
    static MovePtr<TestDefinition> create(EncodeTestParam params)
    {
        return MovePtr<TestDefinition>(new TestDefinition(params));
    }

    TestDefinition(EncodeTestParam params) : m_params(params), m_info(clipInfo(params.clip))
    {
        VideoProfileInfo profile = m_info->sessionProfiles[0];
        m_profile = VkVideoCoreProfile(profile.codecOperation, profile.subsamplingFlags, profile.lumaBitDepth,
                                       profile.chromaBitDepth, profile.profileIDC);
    }

    TestType getTestType() const
    {
        return m_params.type;
    }

    const char *getClipFilename() const
    {
        return m_info->filename;
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
                       uint32_t arrayLayers, VkImage destImage)
{
    Move<VkCommandPool> cmdPool = createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, queueFamilyIndex);
    Move<VkCommandBuffer> cmdBuffer = allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    Move<VkFence> fence             = createFence(vk, device);
    VkImageLayout destImageLayout   = VK_IMAGE_LAYOUT_VIDEO_ENCODE_QUANTIZATION_MAP_KHR;
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

#if STREAM_DUMP_DEBUG
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

#endif

tcu::TestStatus VideoEncodeTestInstance::iterate(void)
{
    const VkVideoCodecOperationFlagBitsKHR videoCodecEncodeOperation = m_testDefinition->getCodecOperation();
    const VkVideoCodecOperationFlagBitsKHR videoCodecDecodeOperation =
        getCodecDecodeOperationFromEncode(videoCodecEncodeOperation);

    const uint32_t gopCount      = m_testDefinition->gopCount();
    const uint32_t gopFrameCount = m_testDefinition->gopFrameCount();
    const uint32_t dpbSlots      = m_testDefinition->gopReferenceFrameCount();

    const bool queryStatus              = m_testDefinition->hasOption(Option::UseStatusQueries);
    const bool useInlineQueries         = m_testDefinition->hasOption(Option::UseInlineQueries);
    const bool resourcesWithoutProfiles = m_testDefinition->hasOption(Option::ResourcesWithoutProfiles);
    const bool resolutionChange         = m_testDefinition->hasOption(Option::ResolutionChange);
    const bool swapOrder                = m_testDefinition->hasOption(Option::SwapOrder);
    const bool useVariableBitrate       = m_testDefinition->hasOption(Option::UseVariableBitrateControl);
    const bool useConstantBitrate       = m_testDefinition->hasOption(Option::UseConstantBitrateControl);
    const bool customEncodeUsage        = m_testDefinition->hasOption(Option::UseEncodeUsage);
    const bool useQualityLevel          = m_testDefinition->hasOption(Option::UseQualityLevel);
    const bool useDeltaMap              = m_testDefinition->hasOption(Option::UseDeltaMap);
    const bool useEmphasisMap           = m_testDefinition->hasOption(Option::UseEmphasisMap);
    const bool disableRateControl       = m_testDefinition->hasOption(Option::DisableRateControl);

    const bool activeRateControl = useVariableBitrate || useConstantBitrate;

    const int32_t constQp          = 28;
    const int32_t maxQpValue       = disableRateControl || activeRateControl ? 42 : 51;
    const int32_t minQpValue       = 0;
    const float minEmphasisQpValue = 0.0f;
    const float maxEmphasisQpValue = 1.0f;
    int32_t minQpDelta             = 0;
    int32_t maxQpDelta             = 0;

    const VkExtent2D codedExtent = {m_testDefinition->getClipWidth(), m_testDefinition->getClipHeight()};

    const MovePtr<VkVideoEncodeUsageInfoKHR> encodeUsageInfo = getEncodeUsageInfo(
        m_testDefinition->getEncodeProfileExtension(),
        customEncodeUsage ? VK_VIDEO_ENCODE_USAGE_STREAMING_BIT_KHR : VK_VIDEO_ENCODE_USAGE_DEFAULT_KHR,
        customEncodeUsage ? VK_VIDEO_ENCODE_CONTENT_DESKTOP_BIT_KHR : VK_VIDEO_ENCODE_CONTENT_DEFAULT_KHR,
        customEncodeUsage ? VK_VIDEO_ENCODE_TUNING_MODE_HIGH_QUALITY_KHR : VK_VIDEO_ENCODE_TUNING_MODE_DEFAULT_KHR);

    const MovePtr<VkVideoProfileInfoKHR> videoEncodeProfile =
        getVideoProfile(videoCodecEncodeOperation, encodeUsageInfo.get());
    const MovePtr<VkVideoProfileInfoKHR> videoDecodeProfile =
        getVideoProfile(videoCodecDecodeOperation, m_testDefinition->getDecodeProfileExtension());

    const MovePtr<VkVideoProfileListInfoKHR> videoEncodeProfileList = getVideoProfileList(videoEncodeProfile.get(), 1);

    const VkFormat imageFormat = checkImageFormat(VK_IMAGE_USAGE_VIDEO_ENCODE_SRC_BIT_KHR, videoEncodeProfileList.get(),
                                                  VK_FORMAT_G8_B8R8_2PLANE_420_UNORM);
    const VkFormat dpbImageFormat = checkImageFormat(VK_IMAGE_USAGE_VIDEO_ENCODE_DPB_BIT_KHR,
                                                     videoEncodeProfileList.get(), VK_FORMAT_G8_B8R8_2PLANE_420_UNORM);

    const VideoDevice::VideoDeviceFlags videoDeviceFlags = m_testDefinition->requiredDeviceFlags();

    if (queryStatus && !checkQueryResultSupport())
        TCU_THROW(NotSupportedError, "Implementation does not support query status");

    const InstanceInterface &vki          = m_context.getInstanceInterface();
    const VkPhysicalDevice physicalDevice = m_context.getPhysicalDevice();
    const VkDevice videoDevice =
        getDeviceSupportingQueue(VK_QUEUE_VIDEO_ENCODE_BIT_KHR | VK_QUEUE_VIDEO_DECODE_BIT_KHR | VK_QUEUE_TRANSFER_BIT,
                                 videoCodecEncodeOperation | videoCodecDecodeOperation, videoDeviceFlags);
    const DeviceInterface &videoDeviceDriver = getDeviceDriver();

    const uint32_t encodeQueueFamilyIndex   = getQueueFamilyIndexEncode();
    const uint32_t decodeQueueFamilyIndex   = getQueueFamilyIndexDecode();
    const uint32_t transferQueueFamilyIndex = getQueueFamilyIndexTransfer();

    const VkQueue encodeQueue   = getDeviceQueue(videoDeviceDriver, videoDevice, encodeQueueFamilyIndex, 0u);
    const VkQueue decodeQueue   = getDeviceQueue(videoDeviceDriver, videoDevice, decodeQueueFamilyIndex, 0u);
    const VkQueue transferQueue = getDeviceQueue(videoDeviceDriver, videoDevice, transferQueueFamilyIndex, 0u);

    const MovePtr<VkVideoEncodeH264QuantizationMapCapabilitiesKHR> H264QuantizationMapCapabilities =
        getVideoEncodeH264QuantizationMapCapabilities();
    const MovePtr<VkVideoEncodeH265QuantizationMapCapabilitiesKHR> H265QuantizationMapCapabilities =
        getVideoEncodeH265QuantizationMapCapabilities();

    const MovePtr<VkVideoEncodeH264CapabilitiesKHR> videoH264CapabilitiesExtension =
        getVideoCapabilitiesExtensionH264E(H264QuantizationMapCapabilities.get());
    const MovePtr<VkVideoEncodeH265CapabilitiesKHR> videoH265CapabilitiesExtension =
        getVideoCapabilitiesExtensionH265E(H265QuantizationMapCapabilities.get());

    void *videoCapabilitiesExtensionPtr = NULL;

    if (m_testDefinition->getProfile()->IsH264())
    {
        videoCapabilitiesExtensionPtr = static_cast<void *>(videoH264CapabilitiesExtension.get());
    }
    else if (m_testDefinition->getProfile()->IsH265())
    {
        videoCapabilitiesExtensionPtr = static_cast<void *>(videoH265CapabilitiesExtension.get());
    }
    DE_ASSERT(videoCapabilitiesExtensionPtr);

    const MovePtr<VkVideoEncodeCapabilitiesKHR> videoEncodeCapabilities =
        getVideoEncodeCapabilities(videoCapabilitiesExtensionPtr);
    const MovePtr<VkVideoCapabilitiesKHR> videoCapabilities =
        getVideoCapabilities(vki, physicalDevice, videoEncodeProfile.get(), videoEncodeCapabilities.get());

    DE_ASSERT(videoEncodeCapabilities->supportedEncodeFeedbackFlags &
              VK_VIDEO_ENCODE_FEEDBACK_BITSTREAM_BYTES_WRITTEN_BIT_KHR);

    if (useDeltaMap)
    {
        if (!(videoEncodeCapabilities->flags & VK_VIDEO_ENCODE_CAPABILITY_QUANTIZATION_DELTA_MAP_BIT_KHR))
        {
            TCU_THROW(NotSupportedError, "Implementation does not support quantization delta map");
        }
    }
    if (useEmphasisMap)
    {
        if (!(videoEncodeCapabilities->flags & VK_VIDEO_ENCODE_CAPABILITY_EMPHASIS_MAP_BIT_KHR))
        {
            TCU_THROW(NotSupportedError, "Implementation does not support emphasis map");
        }
    }

    if (m_testDefinition->getProfile()->IsH264())
    {
        minQpDelta = H264QuantizationMapCapabilities->minQpDelta;
        maxQpDelta = H264QuantizationMapCapabilities->maxQpDelta;
    }
    else if (m_testDefinition->getProfile()->IsH265())
    {
        minQpDelta = H265QuantizationMapCapabilities->minQpDelta;
        maxQpDelta = H265QuantizationMapCapabilities->maxQpDelta;
    }

    // Check support for P and B frames
    if (m_testDefinition->getProfile()->IsH264())
    {
        bool minPReferenceCount  = videoH264CapabilitiesExtension->maxPPictureL0ReferenceCount > 0;
        bool minBReferenceCount  = videoH264CapabilitiesExtension->maxBPictureL0ReferenceCount > 0;
        bool minL1ReferenceCount = videoH264CapabilitiesExtension->maxL1ReferenceCount > 0;

        if (m_testDefinition->patternContain(P_FRAME) && !minPReferenceCount)
        {
            TCU_THROW(NotSupportedError, "Implementation does not support H264 P frames encoding");
        }
        else if (m_testDefinition->patternContain(B_FRAME) && !minBReferenceCount && !minL1ReferenceCount)
        {
            TCU_THROW(NotSupportedError, "Implementation does not support H264 B frames encoding");
        }
    }
    else if (m_testDefinition->getProfile()->IsH265())
    {
        bool minPReferenceCount  = videoH265CapabilitiesExtension->maxPPictureL0ReferenceCount > 0;
        bool minBReferenceCount  = videoH265CapabilitiesExtension->maxBPictureL0ReferenceCount > 0;
        bool minL1ReferenceCount = videoH265CapabilitiesExtension->maxL1ReferenceCount > 0;

        if (m_testDefinition->patternContain(P_FRAME) && !minPReferenceCount)
        {
            TCU_THROW(NotSupportedError, "Implementation does not support H265 P frames encoding");
        }
        else if (m_testDefinition->patternContain(B_FRAME) && !minBReferenceCount && !minL1ReferenceCount)
        {
            TCU_THROW(NotSupportedError, "Implementation does not support H265 B frames encoding");
        }
    }

    // Check support for bitrate control
    if (m_testDefinition->hasOption(Option::UseVariableBitrateControl))
    {
        if ((videoEncodeCapabilities->rateControlModes & VK_VIDEO_ENCODE_RATE_CONTROL_MODE_VBR_BIT_KHR) == 0)
            TCU_THROW(NotSupportedError, "Implementation does not support variable bitrate control");

        DE_ASSERT(videoEncodeCapabilities->maxBitrate > 0);
    }
    else if (m_testDefinition->hasOption(Option::UseConstantBitrateControl))
    {
        if ((videoEncodeCapabilities->rateControlModes & VK_VIDEO_ENCODE_RATE_CONTROL_MODE_CBR_BIT_KHR) == 0)
            TCU_THROW(NotSupportedError, "Implementation does not support constant bitrate control");

        DE_ASSERT(videoEncodeCapabilities->maxBitrate > 0);
    }

    VkDeviceSize bitstreamBufferOffset             = 0u;
    VkDeviceSize minBitstreamBufferOffsetAlignment = videoCapabilities->minBitstreamBufferOffsetAlignment;
    VkDeviceSize nonCoherentAtomSize               = m_context.getDeviceProperties().limits.nonCoherentAtomSize;

    Allocator &allocator = getAllocator();

    DE_ASSERT(videoCapabilities->maxDpbSlots >= dpbSlots);

    VkVideoSessionCreateFlagsKHR videoSessionFlags = 0;

    if (useInlineQueries)
        videoSessionFlags |= VK_VIDEO_SESSION_CREATE_INLINE_QUERIES_BIT_KHR;
    if (useDeltaMap)
        videoSessionFlags |= VK_VIDEO_SESSION_CREATE_ALLOW_ENCODE_QUANTIZATION_DELTA_MAP_BIT_KHR;
    if (useEmphasisMap)
        videoSessionFlags |= VK_VIDEO_SESSION_CREATE_ALLOW_ENCODE_EMPHASIS_MAP_BIT_KHR;

    const MovePtr<VkVideoSessionCreateInfoKHR> videoEncodeSessionCreateInfo =
        getVideoSessionCreateInfo(encodeQueueFamilyIndex, videoSessionFlags, videoEncodeProfile.get(), codedExtent,
                                  imageFormat, dpbImageFormat, dpbSlots, videoCapabilities->maxActiveReferencePictures);

    const Move<VkVideoSessionKHR> videoEncodeSession =
        createVideoSessionKHR(videoDeviceDriver, videoDevice, videoEncodeSessionCreateInfo.get());
    const vector<AllocationPtr> encodeAllocation =
        getAndBindVideoSessionMemory(videoDeviceDriver, videoDevice, *videoEncodeSession, allocator);

    uint8_t quantizationMapCount          = useDeltaMap ? 3 : 2;
    VkFormat quantizationImageFormat      = VK_FORMAT_R8_SNORM;
    VkImageTiling quantizationImageTiling = VK_IMAGE_TILING_OPTIMAL;
    VkExtent2D quantizationMapExtent      = {0, 0};
    VkExtent2D quantizationMapTexelSize   = {0, 0};

    std::vector<std::unique_ptr<const ImageWithMemory>> quantizationMapImages;
    std::vector<std::unique_ptr<const Move<VkImageView>>> quantizationMapImageViews;

    if (useDeltaMap || useEmphasisMap)
    {
        // Query quantization map capabilities
        uint32_t videoFormatPropertiesCount = 0u;

        VkImageUsageFlags quantizationImageUsageFlags =
            (useDeltaMap ? VK_IMAGE_USAGE_VIDEO_ENCODE_QUANTIZATION_DELTA_MAP_BIT_KHR :
                           VK_IMAGE_USAGE_VIDEO_ENCODE_EMPHASIS_MAP_BIT_KHR) |
            VK_IMAGE_USAGE_TRANSFER_DST_BIT;

        const VkPhysicalDeviceVideoFormatInfoKHR videoFormatInfo = {
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VIDEO_FORMAT_INFO_KHR, //  VkStructureType sType;
            videoEncodeProfileList.get(),                            //  const void* pNext;
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

        VK_CHECK(vki.getPhysicalDeviceVideoFormatPropertiesKHR(physicalDevice, &videoFormatInfo,
                                                               &videoFormatPropertiesCount, nullptr));

        videoFormatProperties.resize(videoFormatPropertiesCount, videoFormatPropertiesKHR);
        quantizationMapProperties.resize(videoFormatPropertiesCount, quantizationMapPropertiesKHR);
        H265quantizationMapFormatProperties.resize(videoFormatPropertiesCount, H265QuantizationMapFormatProperty);

        for (uint32_t i = 0; i < videoFormatPropertiesCount; ++i)
        {
            videoFormatProperties[i].pNext = &quantizationMapProperties[i];
            if (m_testDefinition->getProfile()->IsH265())
            {
                quantizationMapProperties[i].pNext = &H265quantizationMapFormatProperties[i];
            }
        }

        VK_CHECK(vki.getPhysicalDeviceVideoFormatPropertiesKHR(
            physicalDevice, &videoFormatInfo, &videoFormatPropertiesCount, videoFormatProperties.data()));

        // Pick first available quantization map format and properties
        quantizationImageFormat  = videoFormatProperties[0].format;
        quantizationImageTiling  = videoFormatProperties[0].imageTiling;
        quantizationMapTexelSize = quantizationMapProperties[0].quantizationMapTexelSize;

        DE_ASSERT(quantizationMapTexelSize.width > 0 && quantizationMapTexelSize.height > 0);

        quantizationMapExtent = {static_cast<uint32_t>(std::ceil(static_cast<float>(codedExtent.width) /
                                                                 static_cast<float>(quantizationMapTexelSize.width))),
                                 static_cast<uint32_t>(std::ceil(static_cast<float>(codedExtent.height) /
                                                                 static_cast<float>(quantizationMapTexelSize.height)))};

        const VkImageUsageFlags quantizationMapImageUsage =
            (useDeltaMap ? VK_IMAGE_USAGE_VIDEO_ENCODE_QUANTIZATION_DELTA_MAP_BIT_KHR :
                           VK_IMAGE_USAGE_VIDEO_ENCODE_EMPHASIS_MAP_BIT_KHR) |
            VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        const VkImageCreateInfo quantizationMapImageCreateInfo = makeImageCreateInfo(
            quantizationImageFormat, quantizationMapExtent, 0, &encodeQueueFamilyIndex, quantizationMapImageUsage,
            videoEncodeProfileList.get(), 1U, VK_IMAGE_LAYOUT_UNDEFINED, quantizationImageTiling);

        const vector<uint32_t> transaferQueueFamilyIndices(1u, transferQueueFamilyIndex);

        const VkBufferUsageFlags quantizationMapBufferUsageFlags = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        const VkDeviceSize quantizationMapBufferSize =
            getBufferSize(quantizationImageFormat, quantizationMapExtent.width, quantizationMapExtent.height);

        const VkBufferCreateInfo quantizationMapBufferCreateInfo = makeBufferCreateInfo(
            quantizationMapBufferSize, quantizationMapBufferUsageFlags, transaferQueueFamilyIndices, 0, nullptr);

        BufferWithMemory quantizationMapBuffer(videoDeviceDriver, videoDevice, getAllocator(),
                                               quantizationMapBufferCreateInfo,
                                               MemoryRequirement::Local | MemoryRequirement::HostVisible);

        Allocation &quantizationMapBufferAlloc = quantizationMapBuffer.getAllocation();
        void *quantizationMapBufferHostPtr     = quantizationMapBufferAlloc.getHostPtr();

        // Calculate QP values for each image sides, the type of values is based on the quantization map format and adnotated by the index
        auto calculateMapValues = [minQpValue, constQp, minQpDelta, maxQpValue, maxQpDelta, minEmphasisQpValue,
                                   maxEmphasisQpValue](auto idx, QuantizationMap mapType) -> auto
        {
            using T          = decltype(idx);
            T leftSideValue  = T{0};
            T rightSideValue = T{0};

            if (mapType == QM_DELTA)
            {
                // Quantization map provided, constant Qp set to 26
                if (idx == 0)
                {
                    leftSideValue = rightSideValue = static_cast<T>(std::max(minQpValue - constQp, minQpDelta));
                }
                // Quantization map provided, constant Qp set to 26
                else if (idx == 1)
                {
                    leftSideValue = rightSideValue = static_cast<T>(std::min(maxQpValue - constQp, maxQpDelta));
                }
                // Only third frame will receive different quantization values for both sides
                else if (idx == 2)
                {
                    leftSideValue  = static_cast<T>(std::max(minQpValue - constQp, minQpDelta));
                    rightSideValue = static_cast<T>(std::min(maxQpValue - constQp, maxQpDelta));
                }
            }
            else if (mapType == QM_EMPHASIS)
            {
                // Only second frame will receive different quantization values for both sides
                if (idx == 1)
                {
                    if constexpr (std::is_same_v<T, uint8_t>)
                    {
                        leftSideValue  = static_cast<T>(minEmphasisQpValue * 255.0f);
                        rightSideValue = static_cast<T>(maxEmphasisQpValue * 255.0f);
                    }
                    else
                    {
                        leftSideValue  = static_cast<T>(minEmphasisQpValue);
                        rightSideValue = static_cast<T>(maxEmphasisQpValue);
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
                createQuantizationPatternImage<T>(quantizationMapExtent, leftSideQp, rightSideQp);

            std::unique_ptr<const ImageWithMemory> quantizationMapImage(
                new ImageWithMemory(videoDeviceDriver, videoDevice, getAllocator(), quantizationMapImageCreateInfo,
                                    MemoryRequirement::Any));
            std::unique_ptr<const Move<VkImageView>> quantizationMapImageView(new Move<VkImageView>(makeImageView(
                videoDeviceDriver, videoDevice, quantizationMapImage->get(), VK_IMAGE_VIEW_TYPE_2D,
                quantizationImageFormat, makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1))));

            deMemset(quantizationMapBufferHostPtr, 0x00, static_cast<size_t>(quantizationMapBufferSize));
            flushAlloc(videoDeviceDriver, videoDevice, quantizationMapBufferAlloc);

            fillBuffer(videoDeviceDriver, videoDevice, quantizationMapBufferAlloc, quantizationMapImageData,
                       nonCoherentAtomSize, quantizationMapBufferSize);

            copyBufferToImage(videoDeviceDriver, videoDevice, transferQueue, transferQueueFamilyIndex,
                              *quantizationMapBuffer, quantizationMapBufferSize, quantizationMapExtent, 1,
                              quantizationMapImage->get());

            quantizationMapImages.push_back(std::move(quantizationMapImage));
            quantizationMapImageViews.push_back(std::move(quantizationMapImageView));
        };

        for (uint32_t qmIdx = 0; qmIdx < quantizationMapCount; ++qmIdx)
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
    } // if (useDeltaMap || useEmphasisMap)

    // Must be smaller than the maxQualityLevels capabilities limit supported by the specified video profile
    uint32_t qualityLevel = 0;
    DE_ASSERT(qualityLevel < videoEncodeCapabilities->maxQualityLevels);

    const MovePtr<VkVideoEncodeQualityLevelInfoKHR> videoEncodeQualityLevelInfo =
        getVideoEncodeQualityLevelInfo(qualityLevel, nullptr);

    const MovePtr<VkVideoEncodeQuantizationMapSessionParametersCreateInfoKHR> quantizationMapSessionParametersInfo =
        getVideoEncodeH264QuantizationMapParameters(quantizationMapTexelSize);

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
    std::vector<Move<VkVideoSessionParametersKHR>> videoEncodeSessionParameters;

    for (int i = 0; i < (resolutionChange ? 2 : 1); ++i)
    {
        // Second videoEncodeSessionParameters is being created with half the size
        uint32_t extentWidth  = i == 0 ? codedExtent.width : codedExtent.width / 2;
        uint32_t extentHeight = i == 0 ? codedExtent.height : codedExtent.height / 2;

        stdVideoH264SequenceParameterSets.push_back(getStdVideoH264EncodeSequenceParameterSet(
            extentWidth, extentHeight, m_testDefinition->maxNumRefs(), nullptr));
        stdVideoH264PictureParameterSets.push_back(getStdVideoH264EncodePictureParameterSet(
            m_testDefinition->ppsActiveRefs0(), m_testDefinition->ppsActiveRefs1()));
        encodeH264SessionParametersAddInfoKHRs.push_back(createVideoEncodeH264SessionParametersAddInfoKHR(
            1u, stdVideoH264SequenceParameterSets.back().get(), 1u, stdVideoH264PictureParameterSets.back().get()));

        H264sessionParametersCreateInfos.push_back(createVideoEncodeH264SessionParametersCreateInfoKHR(
            static_cast<const void *>(useQualityLevel ?
                                          videoEncodeQualityLevelInfo.get() :
                                          ((useDeltaMap || useEmphasisMap) ?
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
            extentWidth, extentHeight, videoH265CapabilitiesExtension->ctbSizes,
            videoH265CapabilitiesExtension->transformBlockSizes, stdVideoH265DecPicBufMgrs.back().get(),
            stdVideoH265ProfileTierLevels.back().get(), stdVideoH265SequenceParameterSetVuis.back().get()));
        stdVideoH265PictureParameterSets.push_back(
            getStdVideoH265PictureParameterSet(videoH265CapabilitiesExtension.get()));
        encodeH265SessionParametersAddInfoKHRs.push_back(getVideoEncodeH265SessionParametersAddInfoKHR(
            1u, stdVideoH265VideoParameterSets.back().get(), 1u, stdVideoH265SequenceParameterSets.back().get(), 1u,
            stdVideoH265PictureParameterSets.back().get()));
        H265sessionParametersCreateInfos.push_back(getVideoEncodeH265SessionParametersCreateInfoKHR(
            static_cast<const void *>(useQualityLevel ?
                                          videoEncodeQualityLevelInfo.get() :
                                          ((useDeltaMap || useEmphasisMap) ?
                                               static_cast<const void *>(quantizationMapSessionParametersInfo.get()) :
                                               nullptr)),
            1u, 1u, 1u, encodeH265SessionParametersAddInfoKHRs.back().get()));

        const void *sessionParametersCreateInfoPtr = nullptr;

        if (m_testDefinition->getProfile()->IsH264())
        {
            sessionParametersCreateInfoPtr = static_cast<const void *>(H264sessionParametersCreateInfos.back().get());
        }
        else if (m_testDefinition->getProfile()->IsH265())
        {
            sessionParametersCreateInfoPtr = static_cast<const void *>(H265sessionParametersCreateInfos.back().get());
        }
        DE_ASSERT(sessionParametersCreateInfoPtr);

        const VkVideoSessionParametersCreateFlagsKHR videoSessionParametersFlags =
            (useDeltaMap || useEmphasisMap) ?
                static_cast<VkVideoSessionParametersCreateFlagsKHR>(
                    VK_VIDEO_SESSION_PARAMETERS_CREATE_QUANTIZATION_MAP_COMPATIBLE_BIT_KHR) :
                static_cast<VkVideoSessionParametersCreateFlagsKHR>(0);

        videoEncodeSessionParametersCreateInfos.push_back(getVideoSessionParametersCreateInfoKHR(
            sessionParametersCreateInfoPtr, videoSessionParametersFlags, *videoEncodeSession));
        videoEncodeSessionParameters.push_back(createVideoSessionParametersKHR(
            videoDeviceDriver, videoDevice, videoEncodeSessionParametersCreateInfos.back().get()));
    }

    const VkImageUsageFlags dpbImageUsage = VK_IMAGE_USAGE_VIDEO_ENCODE_DPB_BIT_KHR;
    // If the implementation does not support individual images for DPB and so must use arrays
    const bool separateReferenceImages =
        videoCapabilities.get()->flags & VK_VIDEO_CAPABILITY_SEPARATE_REFERENCE_IMAGES_BIT_KHR;
    const VkImageCreateInfo dpbImageCreateInfo =
        makeImageCreateInfo(imageFormat, codedExtent, 0, &encodeQueueFamilyIndex, dpbImageUsage,
                            videoEncodeProfileList.get(), separateReferenceImages ? 1 : dpbSlots);
    const VkImageViewType dpbImageViewType =
        separateReferenceImages ? VK_IMAGE_VIEW_TYPE_2D : VK_IMAGE_VIEW_TYPE_2D_ARRAY;

    std::vector<std::unique_ptr<const ImageWithMemory>> dpbImages;

    for (uint8_t i = 0; i < (separateReferenceImages ? dpbSlots : 1); ++i)
    {
        std::unique_ptr<ImageWithMemory> dpbImage(new ImageWithMemory(videoDeviceDriver, videoDevice, getAllocator(),
                                                                      dpbImageCreateInfo, MemoryRequirement::Any));
        dpbImages.push_back(std::move(dpbImage));
    }

    std::vector<MovePtr<StdVideoEncodeH264ReferenceInfo>> H264refInfos;
    std::vector<MovePtr<StdVideoEncodeH265ReferenceInfo>> H265refInfos;

    std::vector<MovePtr<VkVideoEncodeH264DpbSlotInfoKHR>> H264dpbSlotInfos;
    std::vector<MovePtr<VkVideoEncodeH265DpbSlotInfoKHR>> H265dpbSlotInfos;

    for (uint8_t i = 0, j = 0; i < gopFrameCount; ++i)
    {
        if (m_testDefinition->frameType(i) == B_FRAME)
            continue;

        H264refInfos.push_back(getStdVideoEncodeH264ReferenceInfo(getH264PictureType(m_testDefinition->frameType(i)),
                                                                  m_testDefinition->frameNumber(i),
                                                                  m_testDefinition->frameIdx(i) * 2));
        H265refInfos.push_back(getStdVideoEncodeH265ReferenceInfo(getH265PictureType(m_testDefinition->frameType(i)),
                                                                  m_testDefinition->frameIdx(i)));

        H264dpbSlotInfos.push_back(getVideoEncodeH264DpbSlotInfo(H264refInfos[j].get()));
        H265dpbSlotInfos.push_back(getVideoEncodeH265DpbSlotInfo(H265refInfos[j].get()));

        j++;
    }

    std::vector<std::unique_ptr<const Move<VkImageView>>> dpbImageViews;
    std::vector<std::unique_ptr<const VkVideoPictureResourceInfoKHR>> dpbPictureResources;
    std::vector<VkVideoReferenceSlotInfoKHR> dpbImageVideoReferenceSlots;

    for (uint8_t i = 0, j = 0; i < gopFrameCount; ++i)
    {
        if (m_testDefinition->frameType(i) == B_FRAME)
            continue;

        const VkImageSubresourceRange dpbImageSubresourceRange = {
            VK_IMAGE_ASPECT_COLOR_BIT, //  VkImageAspectFlags aspectMask;
            0,                         //  uint32_t baseMipLevel;
            1,                         //  uint32_t levelCount;
            separateReferenceImages ? static_cast<uint32_t>(0) : static_cast<uint32_t>(j), //  uint32_t baseArrayLayer;
            1,                                                                             //  uint32_t layerCount;
        };

        std::unique_ptr<Move<VkImageView>> dpbImageView(new Move<VkImageView>(
            makeImageView(videoDeviceDriver, videoDevice, dpbImages[separateReferenceImages ? j : 0]->get(),
                          dpbImageViewType, imageFormat, dpbImageSubresourceRange)));
        std::unique_ptr<VkVideoPictureResourceInfoKHR> dpbPictureResource(
            new VkVideoPictureResourceInfoKHR(makeVideoPictureResource(codedExtent, 0, dpbImageView->get())));

        dpbImageViews.push_back(std::move(dpbImageView));
        dpbPictureResources.push_back(std::move(dpbPictureResource));

        const void *dpbSlotInfoPtr = nullptr;

        if (m_testDefinition->getProfile()->IsH264())
        {
            dpbSlotInfoPtr = static_cast<const void *>(H264dpbSlotInfos[j].get());
        }
        else if (m_testDefinition->getProfile()->IsH265())
        {
            dpbSlotInfoPtr = static_cast<const void *>(H265dpbSlotInfos[j].get());
        }
        DE_ASSERT(dpbSlotInfoPtr);

        dpbImageVideoReferenceSlots.push_back(
            makeVideoReferenceSlot(swapOrder ? j : -1, dpbPictureResources[j].get(), dpbSlotInfoPtr));

        j++;
    }

    const VkImageUsageFlags imageUsage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_VIDEO_ENCODE_SRC_BIT_KHR;

    std::vector<std::unique_ptr<const ImageWithMemory>> imageVector;
    std::vector<std::unique_ptr<const Move<VkImageView>>> imageViewVector;
    std::vector<std::unique_ptr<const VkVideoPictureResourceInfoKHR>> imagePictureResourceVector;

    for (uint32_t i = 0; i < gopCount; ++i)
    {
        for (uint32_t j = 0; j < gopFrameCount; ++j)
        {
            VkExtent2D currentCodedExtent = codedExtent;
            if (resolutionChange && i == 1)
            {
                currentCodedExtent.width /= 2;
                currentCodedExtent.height /= 2;
            }

            if (currentCodedExtent.width > videoCapabilities->maxCodedExtent.width ||
                currentCodedExtent.height > videoCapabilities->maxCodedExtent.height)
            {
                TCU_THROW(NotSupportedError, "Required dimensions exceed maxCodedExtent");
            }

            if (currentCodedExtent.width < videoCapabilities->minCodedExtent.width ||
                currentCodedExtent.height < videoCapabilities->minCodedExtent.height)
            {
                TCU_THROW(NotSupportedError, "Required dimensions are smaller than minCodedExtent");
            }

            const VkImageCreateInfo imageCreateInfo =
                makeImageCreateInfo(imageFormat, currentCodedExtent,
                                    resourcesWithoutProfiles ? VK_IMAGE_CREATE_VIDEO_PROFILE_INDEPENDENT_BIT_KHR : 0,
                                    &transferQueueFamilyIndex, imageUsage,
                                    resourcesWithoutProfiles ? nullptr : videoEncodeProfileList.get());

            std::unique_ptr<const ImageWithMemory> image(new ImageWithMemory(
                videoDeviceDriver, videoDevice, getAllocator(), imageCreateInfo, MemoryRequirement::Any));
            std::unique_ptr<const Move<VkImageView>> imageView(new Move<VkImageView>(
                makeImageView(videoDeviceDriver, videoDevice, image->get(), VK_IMAGE_VIEW_TYPE_2D, imageFormat,
                              makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1))));
            std::unique_ptr<const VkVideoPictureResourceInfoKHR> imagePictureResource(
                new VkVideoPictureResourceInfoKHR(makeVideoPictureResource(currentCodedExtent, 0, **imageView)));

            imageVector.push_back(std::move(image));
            imageViewVector.push_back(std::move(imageView));
            imagePictureResourceVector.push_back(std::move(imagePictureResource));
        }
    }

    const vector<uint32_t> encodeQueueFamilyIndices(1u, encodeQueueFamilyIndex);

    const VkBufferUsageFlags encodeBufferUsageFlags =
        VK_BUFFER_USAGE_VIDEO_ENCODE_DST_BIT_KHR | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    const VkDeviceSize encodeFrameBufferSize = getBufferSize(imageFormat, codedExtent.width, codedExtent.height);
    const VkDeviceSize encodeFrameBufferSizeAligned =
        deAlign64(encodeFrameBufferSize, videoCapabilities->minBitstreamBufferSizeAlignment);
    const VkDeviceSize encodeBufferSize = encodeFrameBufferSizeAligned * gopFrameCount * gopCount;

    const VkBufferCreateInfo encodeBufferCreateInfo = makeBufferCreateInfo(
        encodeBufferSize, encodeBufferUsageFlags, encodeQueueFamilyIndices, 0, videoEncodeProfileList.get());

    BufferWithMemory encodeBuffer(videoDeviceDriver, videoDevice, getAllocator(), encodeBufferCreateInfo,
                                  MemoryRequirement::Local | MemoryRequirement::HostVisible);

    Allocation &encodeBufferAlloc = encodeBuffer.getAllocation();
    void *encodeBufferHostPtr     = encodeBufferAlloc.getHostPtr();

    Move<VkQueryPool> encodeQueryPool =
        createEncodeVideoQueries(videoDeviceDriver, videoDevice, 2, videoEncodeProfile.get());

    deMemset(encodeBufferHostPtr, 0x00, static_cast<size_t>(encodeBufferSize));
    flushAlloc(videoDeviceDriver, videoDevice, encodeBufferAlloc);

    de::MovePtr<vector<uint8_t>> clip = loadVideoData(m_testDefinition->getClipFilename());

    std::vector<de::MovePtr<std::vector<uint8_t>>> inVector;

    for (uint32_t i = 0; i < gopCount; ++i)
    {
        for (uint32_t j = 0; j < gopFrameCount; ++j)
        {
            uint32_t index = i * gopFrameCount + j;

            uint32_t extentWidth  = codedExtent.width;
            uint32_t extentHeight = codedExtent.height;

            bool half_size = false;

            if (resolutionChange && i == 1)
            {
                extentWidth /= 2;
                extentHeight /= 2;
                half_size = true;
            }

            MovePtr<MultiPlaneImageData> multiPlaneImageData(
                new MultiPlaneImageData(imageFormat, tcu::UVec2(extentWidth, extentHeight)));
            vkt::ycbcr::extractI420Frame(*clip, index, codedExtent.width, codedExtent.height, multiPlaneImageData.get(),
                                         half_size);

            // Save NV12 Multiplanar frame to YUV 420p 8 bits
            de::MovePtr<std::vector<uint8_t>> in =
                vkt::ycbcr::YCbCrConvUtil<uint8_t>::MultiPlanarNV12toI420(multiPlaneImageData.get());

#if STREAM_DUMP_DEBUG
            std::string filename = "in_" + std::to_string(index) + ".yuv";
            vkt::ycbcr::YCbCrContent<uint8_t>::save(*in, filename);
#endif

            vkt::ycbcr::uploadImage(videoDeviceDriver, videoDevice, transferQueueFamilyIndex, allocator,
                                    *(*imageVector[index]), *multiPlaneImageData, 0, VK_IMAGE_LAYOUT_GENERAL);

            inVector.push_back(std::move(in));
        }
    }

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
    {
        videoEncodeSessionParametersGetInfoPtr = static_cast<const void *>(&videoEncodeH264SessionParametersGetInfo);
    }
    else if (m_testDefinition->getProfile()->IsH265())
    {
        videoEncodeSessionParametersGetInfoPtr = static_cast<const void *>(&videoEncodeH265SessionParametersGetInfo);
    }
    DE_ASSERT(videoEncodeSessionParametersGetInfoPtr);

    std::vector<std::vector<uint8_t>> headersData;

    for (int i = 0; i < (resolutionChange ? 2 : 1); ++i)
    {
        const VkVideoEncodeSessionParametersGetInfoKHR videoEncodeSessionParametersGetInfo = {
            VK_STRUCTURE_TYPE_VIDEO_ENCODE_SESSION_PARAMETERS_GET_INFO_KHR, // VkStructureType sType;
            videoEncodeSessionParametersGetInfoPtr,                         // const void* pNext;
            videoEncodeSessionParameters[i].get(), // VkVideoSessionParametersKHR videoSessionParameters;
        };

        std::vector<uint8_t> headerData;

        size_t requiredHeaderSize = 0;
        VK_CHECK(videoDeviceDriver.getEncodedVideoSessionParametersKHR(
            videoDevice, &videoEncodeSessionParametersGetInfo, &videoEncodeSessionParametersFeedbackInfo,
            &requiredHeaderSize, nullptr));

        DE_ASSERT(requiredHeaderSize != 0);

        headerData.resize(requiredHeaderSize);
        VK_CHECK(videoDeviceDriver.getEncodedVideoSessionParametersKHR(
            videoDevice, &videoEncodeSessionParametersGetInfo, &videoEncodeSessionParametersFeedbackInfo,
            &requiredHeaderSize, headerData.data()));

        headersData.push_back(std::move(headerData));
    }

    // Pre fill buffer with SPS and PPS header
    fillBuffer(videoDeviceDriver, videoDevice, encodeBufferAlloc, headersData[0], nonCoherentAtomSize, encodeBufferSize,
               bitstreamBufferOffset);

    // Move offset to accommodate header data
    bitstreamBufferOffset =
        deAlign64(bitstreamBufferOffset + headersData[0].size(), videoCapabilities->minBitstreamBufferOffsetAlignment);

    const Unique<VkCommandPool> encodeCmdPool(makeCommandPool(videoDeviceDriver, videoDevice, encodeQueueFamilyIndex));
    const Unique<VkCommandBuffer> firstEncodeCmdBuffer(
        allocateCommandBuffer(videoDeviceDriver, videoDevice, *encodeCmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));
    const Unique<VkCommandBuffer> secondEncodeCmdBuffer(
        allocateCommandBuffer(videoDeviceDriver, videoDevice, *encodeCmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

    // Rate control
    const de::MovePtr<VkVideoEncodeH264RateControlLayerInfoKHR> videoEncodeH264RateControlLayerInfo =
        getVideoEncodeH264RateControlLayerInfo(true, 0, 0, 0, true, maxQpValue, maxQpValue, maxQpValue);
    const de::MovePtr<VkVideoEncodeH265RateControlLayerInfoKHR> videoEncodeH265RateControlLayerInfo =
        getVideoEncodeH265RateControlLayerInfo(true, 0, 0, 0, true, maxQpValue, maxQpValue, maxQpValue);

    const void *videoEncodeRateControlLayerInfoPtr = nullptr;

    if (m_testDefinition->getProfile()->IsH264())
    {
        videoEncodeRateControlLayerInfoPtr = static_cast<const void *>(videoEncodeH264RateControlLayerInfo.get());
    }
    else if (m_testDefinition->getProfile()->IsH265())
    {
        videoEncodeRateControlLayerInfoPtr = static_cast<const void *>(videoEncodeH265RateControlLayerInfo.get());
    }
    DE_ASSERT(videoEncodeRateControlLayerInfoPtr);

    const VkVideoEncodeRateControlModeFlagBitsKHR rateControlMode =
        disableRateControl ? VK_VIDEO_ENCODE_RATE_CONTROL_MODE_DISABLED_BIT_KHR :
                             (activeRateControl ? (useVariableBitrate ? VK_VIDEO_ENCODE_RATE_CONTROL_MODE_VBR_BIT_KHR :
                                                                        VK_VIDEO_ENCODE_RATE_CONTROL_MODE_CBR_BIT_KHR) :
                                                  VK_VIDEO_ENCODE_RATE_CONTROL_MODE_DEFAULT_KHR);

    const de::MovePtr<VkVideoEncodeRateControlLayerInfoKHR> videoEncodeRateControlLayerInfo =
        getVideoEncodeRateControlLayerInfo(videoEncodeRateControlLayerInfoPtr, rateControlMode,
                                           m_testDefinition->getClipFrameRate());

    const VkVideoEncodeH264RateControlInfoKHR videoEncodeH264RateControlInfo = {
        VK_STRUCTURE_TYPE_VIDEO_ENCODE_H264_RATE_CONTROL_INFO_KHR, //  VkStructureType sType;
        nullptr,                                                   //  const void* pNext;
        VK_VIDEO_ENCODE_H264_RATE_CONTROL_REGULAR_GOP_BIT_KHR,     //  VkVideoEncodeH264RateControlFlagsKHR flags;
        m_testDefinition->gopFrameCount(),                         //  uint32_t gopFrameCount;
        m_testDefinition->gopFrameCount(),                         //  uint32_t idrPeriod;
        m_testDefinition->getConsecutiveBFrameCount(),             //  uint32_t consecutiveBFrameCount;
        1,                                                         //  uint32_t temporalLayerCount;
    };

    const VkVideoEncodeH265RateControlInfoKHR videoEncodeH265RateControlInfo = {
        VK_STRUCTURE_TYPE_VIDEO_ENCODE_H265_RATE_CONTROL_INFO_KHR, //  VkStructureType sType;
        nullptr,                                                   //  const void* pNext;
        VK_VIDEO_ENCODE_H265_RATE_CONTROL_REGULAR_GOP_BIT_KHR,     //  VkVideoEncodeH265RateControlFlagsKHR flags;
        m_testDefinition->gopFrameCount(),                         //  uint32_t gopFrameCount;
        m_testDefinition->gopFrameCount(),                         //  uint32_t idrPeriod;
        m_testDefinition->getConsecutiveBFrameCount(),             //  uint32_t consecutiveBFrameCount;
        (useConstantBitrate || useVariableBitrate) ? 1U : 0,       //  uint32_t subLayerCount;
    };

    const void *videoEncodeRateControlInfoPtr = nullptr;

    if (m_testDefinition->getProfile()->IsH264())
    {
        videoEncodeRateControlInfoPtr = static_cast<const void *>(&videoEncodeH264RateControlInfo);
    }
    else if (m_testDefinition->getProfile()->IsH265())
    {
        videoEncodeRateControlInfoPtr = static_cast<const void *>(&videoEncodeH265RateControlInfo);
    }
    DE_ASSERT(videoEncodeRateControlInfoPtr);

    const de::MovePtr<VkVideoEncodeRateControlInfoKHR> videoEncodeRateControlInfo = getVideoEncodeRateControlInfo(
        disableRateControl ? nullptr : videoEncodeRateControlInfoPtr, rateControlMode,
        (useConstantBitrate || useVariableBitrate) ? videoEncodeRateControlLayerInfo.get() : nullptr);
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

    std::vector<de::MovePtr<VkVideoEncodeInfoKHR>> videoEncodeFrameInfos;
    uint32_t queryId = 0;

    for (uint16_t GOPIdx = 0; GOPIdx < gopCount; ++GOPIdx)
    {
        uint32_t emptyRefSlotIdx = swapOrder ? 1 : 0;

        if (resolutionChange && GOPIdx == 1)
        {
            // Pre fill buffer with new SPS/PPS/VPS header
            fillBuffer(videoDeviceDriver, videoDevice, encodeBufferAlloc, headersData[1], nonCoherentAtomSize,
                       encodeBufferSize, bitstreamBufferOffset);
            bitstreamBufferOffset =
                deAlign64(bitstreamBufferOffset + headersData[1].size(), minBitstreamBufferOffsetAlignment);
        }

        for (uint32_t NALIdx = emptyRefSlotIdx; NALIdx < gopFrameCount; (swapOrder ? --NALIdx : ++NALIdx))
        {
            VkCommandBuffer encodeCmdBuffer =
                (NALIdx == 1 && swapOrder) ? *secondEncodeCmdBuffer : *firstEncodeCmdBuffer;

            beginCommandBuffer(videoDeviceDriver, encodeCmdBuffer, 0u);

            videoDeviceDriver.cmdResetQueryPool(encodeCmdBuffer, encodeQueryPool.get(), 0, 2);

            de::MovePtr<VkVideoBeginCodingInfoKHR> videoBeginCodingFrameInfoKHR = getVideoBeginCodingInfo(
                *videoEncodeSession,
                resolutionChange ? videoEncodeSessionParameters[GOPIdx].get() : videoEncodeSessionParameters[0].get(),
                dpbSlots, &dpbImageVideoReferenceSlots[0],
                (activeRateControl && NALIdx > 0) ? videoEncodeRateControlInfo.get() : nullptr);

            videoDeviceDriver.cmdBeginVideoCodingKHR(encodeCmdBuffer, videoBeginCodingFrameInfoKHR.get());

            de::MovePtr<VkVideoCodingControlInfoKHR> resetVideoEncodingControl =
                getVideoCodingControlInfo(VK_VIDEO_CODING_CONTROL_RESET_BIT_KHR);

            if (NALIdx == 0)
            {
                videoDeviceDriver.cmdControlVideoCodingKHR(encodeCmdBuffer, resetVideoEncodingControl.get());

                if (disableRateControl || activeRateControl)
                {
                    de::MovePtr<VkVideoCodingControlInfoKHR> videoRateConstrolInfo = getVideoCodingControlInfo(
                        VK_VIDEO_CODING_CONTROL_ENCODE_RATE_CONTROL_BIT_KHR, videoEncodeRateControlInfo.get());
                    videoDeviceDriver.cmdControlVideoCodingKHR(encodeCmdBuffer, videoRateConstrolInfo.get());
                }
                if (useQualityLevel)
                {
                    de::MovePtr<VkVideoCodingControlInfoKHR> videoQualityControlInfo = getVideoCodingControlInfo(
                        VK_VIDEO_CODING_CONTROL_ENCODE_QUALITY_LEVEL_BIT_KHR, videoEncodeQualityLevelInfo.get());
                    videoDeviceDriver.cmdControlVideoCodingKHR(encodeCmdBuffer, videoQualityControlInfo.get());
                }
            }

            StdVideoH264PictureType stdVideoH264PictureType = getH264PictureType(m_testDefinition->frameType(NALIdx));
            StdVideoH265PictureType stdVideoH265PictureType = getH265PictureType(m_testDefinition->frameType(NALIdx));

            StdVideoH264SliceType stdVideoH264SliceType = getH264SliceType(m_testDefinition->frameType(NALIdx));
            StdVideoH265SliceType stdVideoH265SliceType = getH265SliceType(m_testDefinition->frameType(NALIdx));

            uint32_t refsPool = 0;

            uint8_t H264RefPicList0[STD_VIDEO_H264_MAX_NUM_LIST_REF];
            uint8_t H265RefPicList0[STD_VIDEO_H265_MAX_NUM_LIST_REF];

            std::fill(H264RefPicList0, H264RefPicList0 + STD_VIDEO_H264_MAX_NUM_LIST_REF,
                      STD_VIDEO_H264_NO_REFERENCE_PICTURE);
            std::fill(H265RefPicList0, H265RefPicList0 + STD_VIDEO_H265_MAX_NUM_LIST_REF,
                      STD_VIDEO_H265_NO_REFERENCE_PICTURE);

            uint8_t numL0 = 0;
            uint8_t numL1 = 0;

            bool pType = stdVideoH264PictureType == STD_VIDEO_H264_PICTURE_TYPE_P ||
                         stdVideoH265PictureType == STD_VIDEO_H265_PICTURE_TYPE_P;
            bool bType = stdVideoH264PictureType == STD_VIDEO_H264_PICTURE_TYPE_B ||
                         stdVideoH265PictureType == STD_VIDEO_H265_PICTURE_TYPE_B;

            if (pType)
            {
                refsPool = 1;

                std::vector<uint8_t> list0 = m_testDefinition->ref0(NALIdx);
                for (auto idx : list0)
                {
                    H264RefPicList0[numL0]   = idx;
                    H265RefPicList0[numL0++] = idx;
                }
            }

            uint8_t H264RefPicList1[STD_VIDEO_H264_MAX_NUM_LIST_REF];
            uint8_t H265RefPicList1[STD_VIDEO_H265_MAX_NUM_LIST_REF];

            std::fill(H264RefPicList1, H264RefPicList1 + STD_VIDEO_H264_MAX_NUM_LIST_REF,
                      STD_VIDEO_H264_NO_REFERENCE_PICTURE);
            std::fill(H265RefPicList1, H265RefPicList1 + STD_VIDEO_H265_MAX_NUM_LIST_REF,
                      STD_VIDEO_H265_NO_REFERENCE_PICTURE);

            if (bType)
            {
                refsPool = 2;

                std::vector<uint8_t> list0 = m_testDefinition->ref0(NALIdx);
                for (auto idx : list0)
                {
                    H264RefPicList0[numL0]   = idx;
                    H265RefPicList0[numL0++] = idx;
                }

                std::vector<uint8_t> list1 = m_testDefinition->ref1(NALIdx);
                for (auto idx : list1)
                {
                    H264RefPicList1[numL1]   = idx;
                    H265RefPicList1[numL1++] = idx;
                }
            }

            bool h264ActiveOverrideFlag =
                (stdVideoH264SliceType != STD_VIDEO_H264_SLICE_TYPE_I) &&
                ((m_testDefinition->ppsActiveRefs0() != m_testDefinition->shActiveRefs0(NALIdx)) ||
                 (m_testDefinition->ppsActiveRefs1() != m_testDefinition->shActiveRefs1(NALIdx)));

            stdVideoEncodeH264SliceHeaders.push_back(
                getStdVideoEncodeH264SliceHeader(stdVideoH264SliceType, h264ActiveOverrideFlag));
            videoEncodeH264NaluSlices.push_back(getVideoEncodeH264NaluSlice(
                stdVideoEncodeH264SliceHeaders.back().get(),
                (rateControlMode == VK_VIDEO_ENCODE_RATE_CONTROL_MODE_DISABLED_BIT_KHR) ? constQp : 0));
            videoEncodeH264ReferenceListInfos.push_back(
                getVideoEncodeH264ReferenceListsInfo(H264RefPicList0, H264RefPicList1, numL0, numL1));
            H264pictureInfos.push_back(getStdVideoEncodeH264PictureInfo(
                getH264PictureType(m_testDefinition->frameType(NALIdx)), m_testDefinition->frameNumber(NALIdx),
                m_testDefinition->frameIdx(NALIdx) * 2, GOPIdx,
                NALIdx > 0 ? videoEncodeH264ReferenceListInfos.back().get() : nullptr));
            videoEncodeH264PictureInfo.push_back(
                getVideoEncodeH264PictureInfo(H264pictureInfos.back().get(), videoEncodeH264NaluSlices.back().get()));

            stdVideoEncodeH265SliceSegmentHeaders.push_back(
                getStdVideoEncodeH265SliceSegmentHeader(stdVideoH265SliceType));
            videoEncodeH265NaluSliceSegments.push_back(getVideoEncodeH265NaluSliceSegment(
                stdVideoEncodeH265SliceSegmentHeaders.back().get(),
                (rateControlMode == VK_VIDEO_ENCODE_RATE_CONTROL_MODE_DISABLED_BIT_KHR) ? constQp : 0));
            videoEncodeH265ReferenceListInfos.push_back(
                getVideoEncodeH265ReferenceListsInfo(H265RefPicList0, H265RefPicList1));
            stdVideoH265ShortTermRefPicSets.push_back(getStdVideoH265ShortTermRefPicSet(
                getH265PictureType(m_testDefinition->frameType(NALIdx)), m_testDefinition->frameIdx(NALIdx),
                m_testDefinition->getConsecutiveBFrameCount()));
            H265pictureInfos.push_back(getStdVideoEncodeH265PictureInfo(
                getH265PictureType(m_testDefinition->frameType(NALIdx)), m_testDefinition->frameIdx(NALIdx),
                NALIdx > 0 ? videoEncodeH265ReferenceListInfos.back().get() : nullptr,
                stdVideoH265ShortTermRefPicSets.back().get()));
            videoEncodeH265PictureInfos.push_back(getVideoEncodeH265PictureInfo(
                H265pictureInfos.back().get(), videoEncodeH265NaluSliceSegments.back().get()));

            const void *videoEncodePictureInfoPtr = nullptr;

            if (m_testDefinition->getProfile()->IsH264())
            {
                videoEncodePictureInfoPtr = static_cast<const void *>(videoEncodeH264PictureInfo.back().get());
            }
            else if (m_testDefinition->getProfile()->IsH265())
            {
                videoEncodePictureInfoPtr = static_cast<const void *>(videoEncodeH265PictureInfos.back().get());
            }
            DE_ASSERT(videoEncodePictureInfoPtr);

            VkVideoReferenceSlotInfoKHR *setupReferenceSlotPtr = nullptr;

            int8_t curSlotIdx = m_testDefinition->curSlot(NALIdx);
            if (!bType)
            {
                setupReferenceSlotPtr            = &dpbImageVideoReferenceSlots[curSlotIdx];
                setupReferenceSlotPtr->slotIndex = curSlotIdx;
            }

            int32_t startRefSlot = refsPool == 0 ? -1 : m_testDefinition->refSlots(NALIdx)[0];
            VkVideoReferenceSlotInfoKHR *referenceSlots =
                &dpbImageVideoReferenceSlots[separateReferenceImages && startRefSlot > -1 ? startRefSlot : 0];
            uint8_t refsCount              = m_testDefinition->refsCount(NALIdx);
            uint32_t srcPictureResourceIdx = (GOPIdx * gopFrameCount) + m_testDefinition->frameIdx(NALIdx);

            VkDeviceSize dstBufferOffset;

            // Due to the invert command order dstBufferOffset for P frame is unknown during the recording, set offset to a "safe" values
            if (swapOrder)
            {
                if (NALIdx == 0)
                {
                    dstBufferOffset = deAlign64(256, minBitstreamBufferOffsetAlignment);
                }
                else
                {
                    dstBufferOffset = deAlign64(encodeFrameBufferSizeAligned + 256, minBitstreamBufferOffsetAlignment);
                }
            }
            else
            {
                dstBufferOffset = bitstreamBufferOffset;
            }

            de::MovePtr<VkVideoInlineQueryInfoKHR> inlineQueryInfo =
                getVideoInlineQueryInfo(encodeQueryPool.get(), queryId, 1, nullptr);

            de::MovePtr<VkVideoEncodeQuantizationMapInfoKHR> quantizationMapInfo;

            if (useInlineQueries)
            {
                VkBaseInStructure *pStruct = (VkBaseInStructure *)videoEncodePictureInfoPtr;
                while (pStruct->pNext)
                    pStruct = (VkBaseInStructure *)pStruct->pNext;
                pStruct->pNext = (VkBaseInStructure *)inlineQueryInfo.get();
            }
            else if (useDeltaMap || useEmphasisMap)
            {
                VkBaseInStructure *pStruct = (VkBaseInStructure *)videoEncodePictureInfoPtr;
                quantizationMapInfo        = getQuantizationMapInfo(
                    quantizationMapImageViews[GOPIdx % quantizationMapCount]->get(), quantizationMapExtent);
                while (pStruct->pNext)
                    pStruct = (VkBaseInStructure *)pStruct->pNext;
                pStruct->pNext = (VkBaseInStructure *)quantizationMapInfo.get();
            }

            const VkVideoEncodeFlagsKHR encodeFlags =
                (useDeltaMap || useEmphasisMap) ?
                    (useDeltaMap ?
                         static_cast<VkVideoEncodeFlagsKHR>(VK_VIDEO_ENCODE_WITH_QUANTIZATION_DELTA_MAP_BIT_KHR) :
                         static_cast<VkVideoEncodeFlagsKHR>(VK_VIDEO_ENCODE_WITH_EMPHASIS_MAP_BIT_KHR)) :
                    static_cast<VkVideoEncodeFlagsKHR>(0);

            videoEncodeFrameInfos.push_back(
                getVideoEncodeInfo(videoEncodePictureInfoPtr, encodeFlags, *encodeBuffer, dstBufferOffset,
                                   (*imagePictureResourceVector[srcPictureResourceIdx]), setupReferenceSlotPtr,
                                   refsCount, (refsPool == 0) ? nullptr : referenceSlots));

            if (!useInlineQueries)
                videoDeviceDriver.cmdBeginQuery(encodeCmdBuffer, encodeQueryPool.get(), queryId, 0);

            videoDeviceDriver.cmdEncodeVideoKHR(encodeCmdBuffer, videoEncodeFrameInfos.back().get());

            if (!useInlineQueries)
                videoDeviceDriver.cmdEndQuery(encodeCmdBuffer, encodeQueryPool.get(), queryId);
            videoDeviceDriver.cmdEndVideoCodingKHR(encodeCmdBuffer, &videoEndCodingInfo);

            endCommandBuffer(videoDeviceDriver, encodeCmdBuffer);

            if (!swapOrder)
            {
                submitCommandsAndWait(videoDeviceDriver, videoDevice, encodeQueue, encodeCmdBuffer);

                if (!processQueryPoolResults(videoDeviceDriver, videoDevice, encodeQueryPool.get(), queryId, 1,
                                             bitstreamBufferOffset, minBitstreamBufferOffsetAlignment, queryStatus))
                    return tcu::TestStatus::fail("Unexpected query result status");
            }

            if (!bType)
            {
                if (swapOrder)
                    emptyRefSlotIdx--;
                else
                    emptyRefSlotIdx++;
            }
        }
    }

    if (swapOrder)
    {
        Move<VkSemaphore> frameEncodedSemaphore     = createSemaphore(videoDeviceDriver, videoDevice);
        const VkPipelineStageFlags waitDstStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;

        const auto firstCommandFence =
            submitCommands(videoDeviceDriver, videoDevice, encodeQueue, *firstEncodeCmdBuffer, false, 1U, 0, nullptr,
                           nullptr, 1, &frameEncodedSemaphore.get());
        waitForFence(videoDeviceDriver, videoDevice, *firstCommandFence);

        if (!processQueryPoolResults(videoDeviceDriver, videoDevice, encodeQueryPool.get(), queryId, 1,
                                     bitstreamBufferOffset, minBitstreamBufferOffsetAlignment, queryStatus))
            return tcu::TestStatus::fail("Unexpected query result status");

        const auto secondCommandFence =
            submitCommands(videoDeviceDriver, videoDevice, encodeQueue, *secondEncodeCmdBuffer, false, 1U, 1,
                           &frameEncodedSemaphore.get(), &waitDstStageMask);
        waitForFence(videoDeviceDriver, videoDevice, *secondCommandFence);

        if (!processQueryPoolResults(videoDeviceDriver, videoDevice, encodeQueryPool.get(), queryId, 1,
                                     bitstreamBufferOffset, minBitstreamBufferOffsetAlignment, queryStatus))
            return tcu::TestStatus::fail("Unexpected query result status");
    }

#if STREAM_DUMP_DEBUG
    if (m_testDefinition->getProfile()->IsH264())
    {
        saveBufferAsFile(encodeBuffer, encodeBufferSize, "out.h264");
    }
    else if (m_testDefinition->getProfile()->IsH265())
    {
        saveBufferAsFile(encodeBuffer, encodeBufferSize, "out.h265");
    }
#endif

// Vulkan video is not supported on android platform
// all external libraries, helper functions and test instances has been excluded
#ifdef DE_BUILD_VIDEO
    DeviceContext deviceContext(&m_context, &m_videoDevice, physicalDevice, videoDevice, decodeQueue, encodeQueue,
                                transferQueue);

    const Unique<VkCommandPool> decodeCmdPool(makeCommandPool(videoDeviceDriver, videoDevice, decodeQueueFamilyIndex));
    const Unique<VkCommandBuffer> decodeCmdBuffer(
        allocateCommandBuffer(videoDeviceDriver, videoDevice, *decodeCmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

    uint32_t H264profileIdc = STD_VIDEO_H264_PROFILE_IDC_MAIN;
    uint32_t H265profileIdc = STD_VIDEO_H265_PROFILE_IDC_MAIN;

    uint32_t profileIdc = 0;

    if (m_testDefinition->getProfile()->IsH264())
    {
        profileIdc = H264profileIdc;
    }
    else if (m_testDefinition->getProfile()->IsH265())
    {
        profileIdc = H265profileIdc;
    }
    DE_ASSERT(profileIdc);

    auto decodeProfile =
        VkVideoCoreProfile(videoCodecDecodeOperation, VK_VIDEO_CHROMA_SUBSAMPLING_420_BIT_KHR,
                           VK_VIDEO_COMPONENT_BIT_DEPTH_8_BIT_KHR, VK_VIDEO_COMPONENT_BIT_DEPTH_8_BIT_KHR, profileIdc);
    auto basicDecoder =
        createBasicDecoder(&deviceContext, &decodeProfile, m_testDefinition->framesToCheck(), resolutionChange);

    Demuxer::Params demuxParams = {};
    demuxParams.data            = std::make_unique<BufferedReader>(
        static_cast<const char *>(encodeBuffer.getAllocation().getHostPtr()), encodeBufferSize);
    demuxParams.codecOperation = videoCodecDecodeOperation;
    demuxParams.framing        = ElementaryStreamFraming::H26X_BYTE_STREAM;
    auto demuxer               = Demuxer::create(std::move(demuxParams));
    VkVideoParser parser;
    // TODO: Check for decoder extension support before attempting validation!
    createParser(demuxer->codecOperation(), basicDecoder, parser, demuxer->framing());

    FrameProcessor processor(std::move(demuxer), basicDecoder);
    std::vector<int> incorrectFrames;
    std::vector<int> correctFrames;
    std::vector<double> psnrDiff;

    for (int NALIdx = 0; NALIdx < m_testDefinition->framesToCheck(); NALIdx++)
    {
        DecodedFrame frame;
        TCU_CHECK_AND_THROW(
            InternalError, processor.getNextFrame(&frame) > 0,
            "Expected more frames from the bitstream. Most likely an internal CTS bug, or maybe an invalid bitstream");

        auto resultImage =
            getDecodedImageFromContext(deviceContext,
                                       basicDecoder->dpbAndOutputCoincide() ? VK_IMAGE_LAYOUT_VIDEO_DECODE_DPB_KHR :
                                                                              VK_IMAGE_LAYOUT_VIDEO_DECODE_DST_KHR,
                                       &frame);
        de::MovePtr<std::vector<uint8_t>> out =
            vkt::ycbcr::YCbCrConvUtil<uint8_t>::MultiPlanarNV12toI420(resultImage.get());

#if STREAM_DUMP_DEBUG
        const string outputFileName = "out_" + std::to_string(NALIdx) + ".yuv";
        vkt::ycbcr::YCbCrContent<uint8_t>::save(*out, outputFileName);
#endif
        // Quantization maps verification
        if (useDeltaMap || useEmphasisMap)
        {
            double d = util::calculatePSNRdifference(*inVector[NALIdx], *out, codedExtent, quantizationMapExtent,
                                                     quantizationMapTexelSize);

            psnrDiff.push_back(d);

            if (useEmphasisMap && NALIdx == 1)
            {
                if (psnrDiff[1] <= psnrDiff[0])
                    return tcu::TestStatus::fail(
                        "PSNR difference for the second frame is not greater than for the first frame");
            }
            else if (useDeltaMap && NALIdx == 2)
            {
                if (psnrDiff[2] > 0)
                    return tcu::TestStatus::fail(
                        "PSNR value for left half of the frame is lower than for the right half");
            }
        }

        double higherPsnrThreshold     = 30.0;
        double lowerPsnrThreshold      = 20.0;
        double criticalPsnrThreshold   = 10;
        double psnrThresholdLowerLimit = disableRateControl ? lowerPsnrThreshold : higherPsnrThreshold;
        string failMessage;

        double psnr = util::PSNR(*inVector[NALIdx], *out);

        // Quality checks
        if (psnr < psnrThresholdLowerLimit)
        {
            double difference = psnrThresholdLowerLimit - psnr;

            if ((useDeltaMap || useEmphasisMap) && NALIdx == 1)
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
    const string passMessage = std::to_string(m_testDefinition->framesToCheck()) + " correctly encoded frames";
    return tcu::TestStatus::pass(passMessage);

#else
    DE_UNREF(transferQueue);
    DE_UNREF(decodeQueue);
    TCU_THROW(NotSupportedError, "Vulkan video is not supported on android platform");
#endif
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
    default:
        TCU_THROW(InternalError, "Unknown TestType");
    }
}

TestInstance *VideoEncodeTestCase::createInstance(Context &context) const
{
#ifdef DE_BUILD_VIDEO
    return new VideoEncodeTestInstance(context, m_testDefinition.get());
#else
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

    for (const auto &encodeTest : g_EncodeTests)
    {
        auto defn = TestDefinition::create(encodeTest);

        const char *testName = getTestName(defn->getTestType());
        auto testCodec       = getTestCodec(defn->getTestType());

        if (testCodec == TEST_CODEC_H264)
            h264Group->addChild(new VideoEncodeTestCase(testCtx, testName, defn));
        else if (testCodec == TEST_CODEC_H265)
            h265Group->addChild(new VideoEncodeTestCase(testCtx, testName, defn));
        else
        {
            TCU_THROW(InternalError, "Unknown Video Codec");
        }
    }

    group->addChild(h264Group.release());
    group->addChild(h265Group.release());
    group->addChild(createVideoEncodeTestsAV1(testCtx));

    return group.release();
}
} // namespace video
} // namespace vkt
