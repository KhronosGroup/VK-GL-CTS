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
 */
/*!
 * \file
 * \brief Video decoding tests
 */
/*--------------------------------------------------------------------*/

#include <utility>
#include <memory>

#include "deDefs.h"
#include "deFilePath.hpp"
#include "deStringUtil.hpp"

#include "tcuTestLog.hpp"
#include "tcuCommandLine.hpp"

#include "vkDefs.hpp"
#include "vktVideoDecodeTests.hpp"
#include "vktVideoTestUtils.hpp"
#include "vkBarrierUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkImageUtil.hpp"

#include "vktVideoClipInfo.hpp"

#ifdef DE_BUILD_VIDEO
#include "vktVideoBaseDecodeUtils.hpp"
#include <VulkanH264Decoder.h>
#include <VulkanH265Decoder.h>
#include <VulkanAV1Decoder.h>
#endif

namespace vkt
{
namespace video
{
#ifdef DE_BUILD_VIDEO
// Set this to 1 to have the decoded YCbCr frames written to the
// filesystem in the YV12 format.
// Check the relevant sections to change the file name and so on...
static constexpr bool debug_WriteOutFramesToSingleFile = false;
// Default behaviour is to write all decoded frames to one
// file. Setting this to 1 will output each frame to its own file.
static constexpr bool debug_WriteOutFramesToSeparateFiles = false;
// If true, the dumped frames will contain the results of decoding
// with filmgrain as expected. When false, `apply_grain` is forced to
// zero, resulting in dumped frames having no post-processing
// applied. This is useful for debugging the filmgrain process. Only
// AV1 has been considered so far.
static constexpr bool debug_WriteOutFilmGrain = true;

static_assert(!(debug_WriteOutFramesToSingleFile && debug_WriteOutFramesToSeparateFiles));

static constexpr double kFilmgrainPSNRLowerBound = 28.0;
static constexpr double kFilmgrainPSNRUpperBound = 34.0;

FrameProcessor::FrameProcessor(std::shared_ptr<Demuxer> &&demuxer, std::shared_ptr<VideoBaseDecoder> decoder)
    : m_decoder(decoder)
    , m_demuxer(std::move(demuxer))
{
    // TOOD: The parser interface is most unfortunate, it's as close
    // as inside-out to our needs as you can get. Eventually integrate
    // it more cleanly into the CTS.
    createParser(m_demuxer->codecOperation(), m_decoder, m_parser, m_demuxer->framing());
}

void FrameProcessor::parseNextChunk()
{
    std::vector<uint8_t> demuxedPacket = m_demuxer->nextPacket();

    VkParserBitstreamPacket pkt;
    pkt.pByteStream     = demuxedPacket.data();
    pkt.nDataLength     = demuxedPacket.size();
    pkt.llPTS           = 0;                         // Irrelevant for CTS
    pkt.bEOS            = demuxedPacket.size() == 0; // true if this is an End-Of-Stream packet (flush everything)
    pkt.bPTSValid       = false;                     // Irrelevant for CTS
    pkt.bDiscontinuity  = false;                     // Irrelevant for CTS
    pkt.bPartialParsing = false;                     // false: parse entire packet, true: parse until next
    pkt.bEOP            = false;                     // Irrelevant for CTS
    pkt.pbSideData      = nullptr;                   // Irrelevant for CTS
    pkt.nSideDataLength = 0;                         // Irrelevant for CTS

    size_t parsedBytes       = 0;
    const bool parserSuccess = m_parser->ParseByteStream(&pkt, &parsedBytes);
    m_eos                    = demuxedPacket.size() == 0 || !parserSuccess;
}

int FrameProcessor::getNextFrame(DecodedFrame *pFrame)
{
    int32_t framesInQueue = m_decoder->GetVideoFrameBuffer()->DequeueDecodedPicture(pFrame);
    while (!framesInQueue && !m_eos)
    {
        parseNextChunk();
        framesInQueue = m_decoder->GetVideoFrameBuffer()->DequeueDecodedPicture(pFrame);
    }

    if (!framesInQueue && !m_eos)
    {
        return -1;
    }

    return framesInQueue;
}

void FrameProcessor::bufferFrames(int framesToDecode)
{
    // This loop is for the out-of-order submissions cases. First the
    // requisite frame information is gathered from the
    // parser<->decoder loop.
    // Then the command buffers are recorded in a random order w.r.t
    // original coding order. Queue submissions are always in coding
    // order.
    // NOTE: For this sequence to work, the frame buffer must have
    // enough decode surfaces for the GOP intended for decode,
    // otherwise picture allocation will fail pretty quickly! See
    // m_numDecodeSurfaces, m_maxDecodeFramesCount
    // NOTE: When requesting two frames to be buffered, it's only
    // guaranteed in the successful case you will get 2 *or more*
    // frames for decode.  This is due to the inter-frame references,
    // where the second frame, for example, may need further coded
    // frames as dependencies.
    DE_ASSERT(m_decoder->m_outOfOrderDecoding);
    do
    {
        parseNextChunk();
        size_t decodedFrames = m_decoder->GetVideoFrameBuffer()->GetDisplayedFrameCount();
        if (decodedFrames >= framesToDecode)
            break;
    } while (!m_eos);
    auto &cachedParams = m_decoder->m_cachedDecodeParams;
    TCU_CHECK_MSG(cachedParams.size() >= framesToDecode, "Unknown decoder failure");
}
#endif

namespace
{
using namespace vk;
using de::MovePtr;

enum TestType
{
    TEST_TYPE_H264_DECODE_I,   // Case 6
    TEST_TYPE_H264_DECODE_I_P, // Case 7
    TEST_TYPE_H264_DECODE_CLIP_A,
    TEST_TYPE_H264_DECODE_I_P_B_13,                    // Case 7a
    TEST_TYPE_H264_DECODE_I_P_NOT_MATCHING_ORDER,      // Case 8
    TEST_TYPE_H264_DECODE_I_P_B_13_NOT_MATCHING_ORDER, // Case 8a
    TEST_TYPE_H264_DECODE_QUERY_RESULT_WITH_STATUS,    // Case 9
    TEST_TYPE_H264_DECODE_INLINE_QUERY_RESULT_WITH_STATUS,
    TEST_TYPE_H264_DECODE_RESOURCES_WITHOUT_PROFILES,
    TEST_TYPE_H264_DECODE_RESOLUTION_CHANGE,       // Case 17
    TEST_TYPE_H264_DECODE_RESOLUTION_CHANGE_DPB,   // Case 18
    TEST_TYPE_H264_DECODE_INTERLEAVED,             // Case 21
    TEST_TYPE_H264_BOTH_DECODE_ENCODE_INTERLEAVED, // Case 23 TODO
    TEST_TYPE_H264_H265_DECODE_INTERLEAVED,        // Case 24

    // VK_KHR_video_maintenance2
    TEST_TYPE_H264_DECODE_INLINE_SESSION_PARAMS,
    TEST_TYPE_H264_DECODE_RELAXED_SESSION_PARAMS,

    TEST_TYPE_H265_DECODE_I,   // Case 15
    TEST_TYPE_H265_DECODE_I_P, // Case 16
    TEST_TYPE_H265_DECODE_CLIP_D,
    TEST_TYPE_H265_DECODE_I_P_NOT_MATCHING_ORDER,      // Case 16-2
    TEST_TYPE_H265_DECODE_I_P_B_13,                    // Case 16-3
    TEST_TYPE_H265_DECODE_I_P_B_13_NOT_MATCHING_ORDER, // Case 16-4
    TEST_TYPE_H265_DECODE_QUERY_RESULT_WITH_STATUS,
    TEST_TYPE_H265_DECODE_INLINE_QUERY_RESULT_WITH_STATUS,
    TEST_TYPE_H265_DECODE_RESOURCES_WITHOUT_PROFILES,
    TEST_TYPE_H265_DECODE_SLIST_A,
    TEST_TYPE_H265_DECODE_SLIST_B,

    // VK_KHR_video_maintenance2
    TEST_TYPE_H265_DECODE_INLINE_SESSION_PARAMS,
    TEST_TYPE_H265_DECODE_RELAXED_SESSION_PARAMS,

    TEST_TYPE_AV1_DECODE_I,
    TEST_TYPE_AV1_DECODE_I_P,
    TEST_TYPE_AV1_DECODE_I_P_NOT_MATCHING_ORDER,
    TEST_TYPE_AV1_DECODE_BASIC_8,
    TEST_TYPE_AV1_DECODE_BASIC_8_NOT_MATCHING_ORDER,
    TEST_TYPE_AV1_DECODE_ALLINTRA_8,
    TEST_TYPE_AV1_DECODE_ALLINTRA_NOSETUP_8,
    TEST_TYPE_AV1_DECODE_ALLINTRA_BC_8,
    TEST_TYPE_AV1_DECODE_CDFUPDATE_8,
    TEST_TYPE_AV1_DECODE_GLOBALMOTION_8,
    TEST_TYPE_AV1_DECODE_FILMGRAIN_8,
    TEST_TYPE_AV1_DECODE_SVCL1T2_8,
    TEST_TYPE_AV1_DECODE_SUPERRES_8,
    TEST_TYPE_AV1_DECODE_SIZEUP_8,

    TEST_TYPE_AV1_DECODE_BASIC_10,
    TEST_TYPE_AV1_DECODE_ORDERHINT_10,
    TEST_TYPE_AV1_DECODE_FORWARDKEYFRAME_10,
    TEST_TYPE_AV1_DECODE_LOSSLESS_10,
    TEST_TYPE_AV1_DECODE_LOOPFILTER_10,
    TEST_TYPE_AV1_DECODE_CDEF_10,
    TEST_TYPE_AV1_DECODE_ARGON_FILMGRAIN_10,
    TEST_TYPE_AV1_DECODE_ARGON_TEST_787,

    TEST_TYPE_AV1_DECODE_ARGON_SEQCHANGE_AFFINE_8,

    // VK_KHR_video_maintenance2
    TEST_TYPE_AV1_DECODE_INLINE_SESSION_PARAMS,
    TEST_TYPE_AV1_DECODE_RELAXED_SESSION_PARAMS,

    TEST_TYPE_LAST
};

enum TestCodec
{
    TEST_CODEC_H264,
    TEST_CODEC_H265,
    TEST_CODEC_AV1,

    TEST_CODEC_LAST
};

static const char *testTypeToStr(TestType type)
{
    const char *testName;
    switch (type)
    {
    case TEST_TYPE_H264_DECODE_I:
    case TEST_TYPE_H265_DECODE_I:
    case TEST_TYPE_AV1_DECODE_I:
        testName = "i";
        break;
    case TEST_TYPE_H264_DECODE_I_P:
    case TEST_TYPE_H265_DECODE_I_P:
    case TEST_TYPE_AV1_DECODE_I_P:
        testName = "i_p";
        break;
    case TEST_TYPE_H264_DECODE_CLIP_A:
        testName = "420_8bit_high_176x144_30frames";
        break;
    case TEST_TYPE_H264_DECODE_I_P_NOT_MATCHING_ORDER:
    case TEST_TYPE_H265_DECODE_I_P_NOT_MATCHING_ORDER:
    case TEST_TYPE_AV1_DECODE_I_P_NOT_MATCHING_ORDER:
        testName = "i_p_not_matching_order";
        break;
    case TEST_TYPE_H264_DECODE_I_P_B_13:
    case TEST_TYPE_H265_DECODE_I_P_B_13:
        testName = "i_p_b_13";
        break;
    case TEST_TYPE_H264_DECODE_I_P_B_13_NOT_MATCHING_ORDER:
    case TEST_TYPE_H265_DECODE_I_P_B_13_NOT_MATCHING_ORDER:
        testName = "i_p_b_13_not_matching_order";
        break;
    case TEST_TYPE_H264_DECODE_QUERY_RESULT_WITH_STATUS:
    case TEST_TYPE_H265_DECODE_QUERY_RESULT_WITH_STATUS:
        testName = "query_with_status";
        break;
    case TEST_TYPE_H264_DECODE_INLINE_QUERY_RESULT_WITH_STATUS:
    case TEST_TYPE_H265_DECODE_INLINE_QUERY_RESULT_WITH_STATUS:
        testName = "inline_query_with_status";
        break;
    case TEST_TYPE_H264_DECODE_RESOURCES_WITHOUT_PROFILES:
    case TEST_TYPE_H265_DECODE_RESOURCES_WITHOUT_PROFILES:
        testName = "resources_without_profiles";
        break;
    case TEST_TYPE_H264_DECODE_RESOLUTION_CHANGE:
        testName = "resolution_change";
        break;
    case TEST_TYPE_H264_DECODE_RESOLUTION_CHANGE_DPB:
        testName = "resolution_change_dpb";
        break;
    case TEST_TYPE_H264_DECODE_INTERLEAVED:
        testName = "interleaved";
        break;
    case TEST_TYPE_H264_H265_DECODE_INTERLEAVED:
        testName = "h265_interleaved";
        break;
    case TEST_TYPE_H265_DECODE_CLIP_D:
        testName = "420_8bit_main_176x144_30frames";
        break;
    case TEST_TYPE_H265_DECODE_SLIST_A:
        testName = "slist_a";
        break;
    case TEST_TYPE_H265_DECODE_SLIST_B:
        testName = "slist_b";
        break;
    case TEST_TYPE_AV1_DECODE_BASIC_8:
        testName = "basic_8";
        break;
    case TEST_TYPE_AV1_DECODE_BASIC_8_NOT_MATCHING_ORDER:
        testName = "basic_8_not_matching_order";
        break;
    case TEST_TYPE_AV1_DECODE_BASIC_10:
        testName = "basic_10";
        break;
    case TEST_TYPE_AV1_DECODE_ALLINTRA_8:
        testName = "allintra_8";
        break;
    case TEST_TYPE_AV1_DECODE_ALLINTRA_NOSETUP_8:
        testName = "allintra_nosetup_8";
        break;
    case TEST_TYPE_AV1_DECODE_ALLINTRA_BC_8:
        testName = "allintrabc_8";
        break;
    case TEST_TYPE_AV1_DECODE_CDFUPDATE_8:
        testName = "cdfupdate_8";
        break;
    case TEST_TYPE_AV1_DECODE_GLOBALMOTION_8:
        testName = "globalmotion_8";
        break;
    case TEST_TYPE_AV1_DECODE_FILMGRAIN_8:
        testName = "filmgrain_8";
        break;
    case TEST_TYPE_AV1_DECODE_SVCL1T2_8:
        testName = "svcl1t2_8";
        break;
    case TEST_TYPE_AV1_DECODE_SUPERRES_8:
        testName = "superres_8";
        break;
    case TEST_TYPE_AV1_DECODE_SIZEUP_8:
        testName = "sizeup_8";
        break;
    case TEST_TYPE_AV1_DECODE_ARGON_SEQCHANGE_AFFINE_8:
        testName = "argon_seqchange_affine_8";
        break;
    case TEST_TYPE_AV1_DECODE_ORDERHINT_10:
        testName = "orderhint_10";
        break;
    case TEST_TYPE_AV1_DECODE_FORWARDKEYFRAME_10:
        testName = "forwardkeyframe_10";
        break;
    case TEST_TYPE_AV1_DECODE_LOSSLESS_10:
        testName = "lossless_10";
        break;
    case TEST_TYPE_AV1_DECODE_LOOPFILTER_10:
        testName = "loopfilter_10";
        break;
    case TEST_TYPE_AV1_DECODE_CDEF_10:
        testName = "cdef_10";
        break;
    case TEST_TYPE_AV1_DECODE_ARGON_FILMGRAIN_10:
        testName = "argon_filmgrain_10_test1019";
        break;
    case TEST_TYPE_AV1_DECODE_ARGON_TEST_787:
        testName = "argon_test787";
        break;
    case TEST_TYPE_H264_DECODE_INLINE_SESSION_PARAMS:
    case TEST_TYPE_H265_DECODE_INLINE_SESSION_PARAMS:
    case TEST_TYPE_AV1_DECODE_INLINE_SESSION_PARAMS:
        testName = "inline_session_params";
        break;
    case TEST_TYPE_H264_DECODE_RELAXED_SESSION_PARAMS:
    case TEST_TYPE_H265_DECODE_RELAXED_SESSION_PARAMS:
    case TEST_TYPE_AV1_DECODE_RELAXED_SESSION_PARAMS:
        testName = "relaxed_session_params";
        break;
    default:
        TCU_THROW(InternalError, "Unknown TestType");
    }
    return testName;
}

enum TestCodec getTestCodec(const TestType testType)
{
    switch (testType)
    {
    case TEST_TYPE_H264_DECODE_I:
    case TEST_TYPE_H264_DECODE_I_P:
    case TEST_TYPE_H264_DECODE_CLIP_A:
    case TEST_TYPE_H264_DECODE_I_P_NOT_MATCHING_ORDER:
    case TEST_TYPE_H264_DECODE_I_P_B_13:
    case TEST_TYPE_H264_DECODE_I_P_B_13_NOT_MATCHING_ORDER:
    case TEST_TYPE_H264_DECODE_QUERY_RESULT_WITH_STATUS:
    case TEST_TYPE_H264_DECODE_INLINE_QUERY_RESULT_WITH_STATUS:
    case TEST_TYPE_H264_DECODE_RESOURCES_WITHOUT_PROFILES:
    case TEST_TYPE_H264_DECODE_RESOLUTION_CHANGE:
    case TEST_TYPE_H264_DECODE_RESOLUTION_CHANGE_DPB:
    case TEST_TYPE_H264_DECODE_INTERLEAVED:
    case TEST_TYPE_H264_H265_DECODE_INTERLEAVED:
    case TEST_TYPE_H264_DECODE_INLINE_SESSION_PARAMS:
    case TEST_TYPE_H264_DECODE_RELAXED_SESSION_PARAMS:
        return TEST_CODEC_H264;

    case TEST_TYPE_H265_DECODE_I:
    case TEST_TYPE_H265_DECODE_I_P:
    case TEST_TYPE_H265_DECODE_CLIP_D:
    case TEST_TYPE_H265_DECODE_I_P_NOT_MATCHING_ORDER:
    case TEST_TYPE_H265_DECODE_I_P_B_13:
    case TEST_TYPE_H265_DECODE_I_P_B_13_NOT_MATCHING_ORDER:
    case TEST_TYPE_H265_DECODE_QUERY_RESULT_WITH_STATUS:
    case TEST_TYPE_H265_DECODE_INLINE_QUERY_RESULT_WITH_STATUS:
    case TEST_TYPE_H265_DECODE_RESOURCES_WITHOUT_PROFILES:
    case TEST_TYPE_H265_DECODE_SLIST_A:
    case TEST_TYPE_H265_DECODE_SLIST_B:
    case TEST_TYPE_H265_DECODE_INLINE_SESSION_PARAMS:
    case TEST_TYPE_H265_DECODE_RELAXED_SESSION_PARAMS:
        return TEST_CODEC_H265;

    case TEST_TYPE_AV1_DECODE_I:
    case TEST_TYPE_AV1_DECODE_I_P:
    case TEST_TYPE_AV1_DECODE_I_P_NOT_MATCHING_ORDER:
    case TEST_TYPE_AV1_DECODE_BASIC_8:
    case TEST_TYPE_AV1_DECODE_BASIC_8_NOT_MATCHING_ORDER:
    case TEST_TYPE_AV1_DECODE_BASIC_10:
    case TEST_TYPE_AV1_DECODE_ALLINTRA_8:
    case TEST_TYPE_AV1_DECODE_ALLINTRA_NOSETUP_8:
    case TEST_TYPE_AV1_DECODE_ALLINTRA_BC_8:
    case TEST_TYPE_AV1_DECODE_CDFUPDATE_8:
    case TEST_TYPE_AV1_DECODE_GLOBALMOTION_8:
    case TEST_TYPE_AV1_DECODE_FILMGRAIN_8:
    case TEST_TYPE_AV1_DECODE_SVCL1T2_8:
    case TEST_TYPE_AV1_DECODE_SUPERRES_8:
    case TEST_TYPE_AV1_DECODE_SIZEUP_8:
    case TEST_TYPE_AV1_DECODE_ARGON_SEQCHANGE_AFFINE_8:
    case TEST_TYPE_AV1_DECODE_ORDERHINT_10:
    case TEST_TYPE_AV1_DECODE_FORWARDKEYFRAME_10:
    case TEST_TYPE_AV1_DECODE_LOSSLESS_10:
    case TEST_TYPE_AV1_DECODE_LOOPFILTER_10:
    case TEST_TYPE_AV1_DECODE_CDEF_10:
    case TEST_TYPE_AV1_DECODE_ARGON_FILMGRAIN_10:
    case TEST_TYPE_AV1_DECODE_ARGON_TEST_787:
    case TEST_TYPE_AV1_DECODE_INLINE_SESSION_PARAMS:
    case TEST_TYPE_AV1_DECODE_RELAXED_SESSION_PARAMS:
        return TEST_CODEC_AV1;

    default:
        TCU_THROW(InternalError, "Unknown TestType");
    }
}

enum DecoderOption : uint32_t
{
    // The default is to do nothing additional to ordinary playback.
    Default = 0,
    // All decode operations will have their status checked for success (Q2 2023: not all vendors support these)
    UseStatusQueries = 1 << 0,
    // Same as above, but tested with inline queries from the video_maintenance1 extension.
    UseInlineStatusQueries = 1 << 1,
    // Do not playback the clip in the "normal fashion", instead cached decode parameters for later process
    // this is primarily used to support out-of-order submission test cases.
    // It is a limited mode of operation, only able to cache 32 frames. This is ample to test out-of-order recording.
    CachedDecoding = 1 << 2,
    // When a parameter object changes the resolution of the test content, and the new video session would otherwise
    // still be compatible with the last session (for example, larger decode surfaces preceeding smaller decode surfaces,
    // a frame downsize), force the session to be recreated anyway.
    RecreateDPBImages = 1 << 3,
    // Test profile-less resources from the video_mainteance1 extension.
    ResourcesWithoutProfiles = 1 << 4,
    FilmGrainPresent         = 1 << 5,
    IntraOnlyDecoding        = 1 << 6,
    AnnexB                   = 1 << 7,

    UseInlineSessionParams    = 1 << 8,
    ResetCodecNoSessionParams = 1 << 9,
};

static const int ALL_FRAMES = 0;

struct BaseDecodeParam
{
    ClipName clip;
    int framesToCheck;
    DecoderOption decoderOptions;
};

struct DecodeTestParam
{
    TestType type;
    BaseDecodeParam stream;

} g_DecodeTests[] = {
    {TEST_TYPE_H264_DECODE_I, {CLIP_H264_DEC_A, 1, DecoderOption::Default}},
    {TEST_TYPE_H264_DECODE_I_P, {CLIP_H264_DEC_A, 2, DecoderOption::Default}},
    {TEST_TYPE_H264_DECODE_I_P_NOT_MATCHING_ORDER, {CLIP_H264_DEC_A, 2, DecoderOption::CachedDecoding}},
    {TEST_TYPE_H264_DECODE_CLIP_A, {CLIP_H264_DEC_A, ALL_FRAMES, DecoderOption::Default}},
    {TEST_TYPE_H264_DECODE_I_P_B_13, {CLIP_H264_DEC_4K_26_IBP_MAIN, ALL_FRAMES, DecoderOption::Default}},
    {TEST_TYPE_H264_DECODE_I_P_B_13_NOT_MATCHING_ORDER,
     {CLIP_H264_DEC_4K_26_IBP_MAIN, ALL_FRAMES, DecoderOption::CachedDecoding}},
    {TEST_TYPE_H264_DECODE_QUERY_RESULT_WITH_STATUS, {CLIP_H264_DEC_A, ALL_FRAMES, DecoderOption::UseStatusQueries}},
    {TEST_TYPE_H264_DECODE_INLINE_QUERY_RESULT_WITH_STATUS,
     {CLIP_H264_DEC_A, ALL_FRAMES,
      (DecoderOption)(DecoderOption::UseStatusQueries | DecoderOption::UseInlineStatusQueries)}},
    {TEST_TYPE_H264_DECODE_RESOURCES_WITHOUT_PROFILES,
     {CLIP_H264_DEC_A, ALL_FRAMES, DecoderOption::ResourcesWithoutProfiles}},
    {TEST_TYPE_H264_DECODE_RESOLUTION_CHANGE, {CLIP_H264_DEC_C, ALL_FRAMES, DecoderOption::Default}},
    {TEST_TYPE_H264_DECODE_RESOLUTION_CHANGE_DPB, {CLIP_H264_DEC_C, ALL_FRAMES, DecoderOption::RecreateDPBImages}},
    {TEST_TYPE_H264_DECODE_INLINE_SESSION_PARAMS, {CLIP_H264_DEC_A, 1, DecoderOption::UseInlineSessionParams}},
    {TEST_TYPE_H264_DECODE_RELAXED_SESSION_PARAMS, {CLIP_H264_DEC_A, 1, DecoderOption::ResetCodecNoSessionParams}},

    {TEST_TYPE_H265_DECODE_I, {CLIP_H265_DEC_D, 1, DecoderOption::Default}},
    {TEST_TYPE_H265_DECODE_I_P, {CLIP_H265_DEC_D, 2, DecoderOption::Default}},
    {TEST_TYPE_H265_DECODE_I_P_NOT_MATCHING_ORDER, {CLIP_H265_DEC_D, 2, DecoderOption::CachedDecoding}},
    {TEST_TYPE_H265_DECODE_I_P_B_13, {CLIP_H265_DEC_JELLY, ALL_FRAMES, DecoderOption::Default}},
    {TEST_TYPE_H265_DECODE_I_P_B_13_NOT_MATCHING_ORDER,
     {CLIP_H265_DEC_JELLY, ALL_FRAMES, DecoderOption::CachedDecoding}},
    {TEST_TYPE_H265_DECODE_CLIP_D, {CLIP_H265_DEC_D, ALL_FRAMES, DecoderOption::Default}},
    {TEST_TYPE_H265_DECODE_QUERY_RESULT_WITH_STATUS, {CLIP_H265_DEC_D, ALL_FRAMES, DecoderOption::UseStatusQueries}},
    {TEST_TYPE_H265_DECODE_INLINE_QUERY_RESULT_WITH_STATUS,
     {CLIP_H265_DEC_D, ALL_FRAMES,
      (DecoderOption)(DecoderOption::UseStatusQueries | DecoderOption::UseInlineStatusQueries)}},
    {TEST_TYPE_H265_DECODE_RESOURCES_WITHOUT_PROFILES,
     {CLIP_H265_DEC_D, ALL_FRAMES, DecoderOption::ResourcesWithoutProfiles}},
    {TEST_TYPE_H265_DECODE_SLIST_A, {CLIP_H265_DEC_ITU_SLIST_A, 28, DecoderOption::Default}},
    {TEST_TYPE_H265_DECODE_SLIST_B, {CLIP_H265_DEC_ITU_SLIST_B, 28, DecoderOption::Default}},
    {TEST_TYPE_H265_DECODE_INLINE_SESSION_PARAMS, {CLIP_H265_DEC_D, 1, DecoderOption::UseInlineSessionParams}},
    {TEST_TYPE_H265_DECODE_RELAXED_SESSION_PARAMS, {CLIP_H265_DEC_D, 1, DecoderOption::ResetCodecNoSessionParams}},

    {TEST_TYPE_AV1_DECODE_I, {CLIP_AV1_DEC_BASIC_8, 1, DecoderOption::Default}},
    {TEST_TYPE_AV1_DECODE_I_P, {CLIP_AV1_DEC_BASIC_8, 2, DecoderOption::Default}},
    {TEST_TYPE_AV1_DECODE_I_P_NOT_MATCHING_ORDER, {CLIP_AV1_DEC_BASIC_8, 2, DecoderOption::CachedDecoding}},
    {TEST_TYPE_AV1_DECODE_BASIC_8, {CLIP_AV1_DEC_BASIC_8, ALL_FRAMES, DecoderOption::Default}},
    {TEST_TYPE_AV1_DECODE_BASIC_8_NOT_MATCHING_ORDER, {CLIP_AV1_DEC_BASIC_8, 24, DecoderOption::CachedDecoding}},
    {TEST_TYPE_AV1_DECODE_ALLINTRA_8, {CLIP_AV1_DEC_ALLINTRA_8, ALL_FRAMES, DecoderOption::Default}},
    {TEST_TYPE_AV1_DECODE_ALLINTRA_NOSETUP_8, {CLIP_AV1_DEC_ALLINTRA_8, ALL_FRAMES, DecoderOption::IntraOnlyDecoding}},
    {TEST_TYPE_AV1_DECODE_ALLINTRA_BC_8, {CLIP_AV1_DEC_ALLINTRA_INTRABC_8, ALL_FRAMES, DecoderOption::Default}},
    {TEST_TYPE_AV1_DECODE_CDFUPDATE_8, {CLIP_AV1_DEC_CDFUPDATE_8, ALL_FRAMES, DecoderOption::Default}},
    {TEST_TYPE_AV1_DECODE_GLOBALMOTION_8, {CLIP_AV1_DEC_GLOBALMOTION_8, ALL_FRAMES, DecoderOption::Default}},
    {TEST_TYPE_AV1_DECODE_FILMGRAIN_8, {CLIP_AV1_DEC_FILMGRAIN_8, ALL_FRAMES, DecoderOption::FilmGrainPresent}},
    {TEST_TYPE_AV1_DECODE_SVCL1T2_8, {CLIP_AV1_DEC_SVCL1T2_8, ALL_FRAMES, DecoderOption::Default}},
    {TEST_TYPE_AV1_DECODE_SUPERRES_8, {CLIP_AV1_DEC_SUPERRES_8, ALL_FRAMES, DecoderOption::Default}},
    {TEST_TYPE_AV1_DECODE_SIZEUP_8, {CLIP_AV1_DEC_SIZEUP_8, ALL_FRAMES, DecoderOption::Default}},
    {TEST_TYPE_AV1_DECODE_BASIC_10, {CLIP_AV1_DEC_BASIC_10, ALL_FRAMES, DecoderOption::Default}},
    {TEST_TYPE_AV1_DECODE_ORDERHINT_10, {CLIP_AV1_DEC_ORDERHINT_10, ALL_FRAMES, DecoderOption::Default}},
    {TEST_TYPE_AV1_DECODE_FORWARDKEYFRAME_10, {CLIP_AV1_DEC_FORWARDKEYFRAME_10, ALL_FRAMES, DecoderOption::Default}},
    {TEST_TYPE_AV1_DECODE_LOSSLESS_10, {CLIP_AV1_DEC_LOSSLESS_10, ALL_FRAMES, DecoderOption::Default}},
    {TEST_TYPE_AV1_DECODE_LOOPFILTER_10, {CLIP_AV1_DEC_LOOPFILTER_10, ALL_FRAMES, DecoderOption::Default}},
    {TEST_TYPE_AV1_DECODE_CDEF_10, {CLIP_AV1_DEC_CDEF_10, ALL_FRAMES, DecoderOption::Default}},
    {TEST_TYPE_AV1_DECODE_INLINE_SESSION_PARAMS, {CLIP_AV1_DEC_BASIC_8, 1, DecoderOption::UseInlineSessionParams}},
    {TEST_TYPE_AV1_DECODE_RELAXED_SESSION_PARAMS, {CLIP_AV1_DEC_BASIC_8, 1, DecoderOption::ResetCodecNoSessionParams}},

    // TODO: Did not have sufficient implementations to find out why this is failing.
    // {TEST_TYPE_AV1_DECODE_ARGON_SEQCHANGE_AFFINE_8, {CLIP_ARGON_SEQCHANGE_AFFINE_8, 4, DecoderOption::AnnexB}},

    // TODO: Frames after the first hit asserts in the parser. First frame decodes correctly.
    // TODO: Filmgrain option not set here, because the first frame doesn't have it enabled.
    {TEST_TYPE_AV1_DECODE_ARGON_FILMGRAIN_10,
     {CLIP_AV1_DEC_ARGON_FILMGRAIN_10, 1, (DecoderOption)(DecoderOption::AnnexB)}},

    // TODO: Did not have sufficient implementations to find out why this is failing.
    //{TEST_TYPE_AV1_DECODE_ARGON_TEST_787, {CLIP_ARGON_TEST_787, 2, DecoderOption::AnnexB}},
};

struct InterleavingDecodeTestParams
{
    TestType type;
    BaseDecodeParam streamA;
    BaseDecodeParam streamB;
} g_InterleavingTests[] = {
    {TEST_TYPE_H264_DECODE_INTERLEAVED,
     {CLIP_H264_DEC_A, ALL_FRAMES, DecoderOption::CachedDecoding},
     {CLIP_H264_DEC_A, ALL_FRAMES, DecoderOption::CachedDecoding}},
    {TEST_TYPE_H264_H265_DECODE_INTERLEAVED,
     {CLIP_H264_DEC_A, ALL_FRAMES, DecoderOption::CachedDecoding},
     {CLIP_H265_DEC_D, ALL_FRAMES, DecoderOption::CachedDecoding}},
};

class TestDefinition
{
public:
    static MovePtr<TestDefinition> create(DecodeTestParam params, uint32_t baseSeed, bool layeredDpb)
    {
        return MovePtr<TestDefinition>(new TestDefinition(params, baseSeed, layeredDpb));
    }

    TestDefinition(DecodeTestParam params, uint32_t baseSeed, bool layeredDpb)
        : m_params(params)
        , m_info(clipInfo(params.stream.clip))
        , m_hash(baseSeed)
        , m_isLayeredDpb(layeredDpb)
    {
        for (const auto &profile : m_info->sessionProfiles)
        {
            m_profiles.push_back(VkVideoCoreProfile(profile.codecOperation, profile.subsamplingFlags,
                                                    profile.lumaBitDepth, profile.chromaBitDepth, profile.profileIDC,
                                                    params.stream.decoderOptions & DecoderOption::FilmGrainPresent));
        }

        if (m_params.stream.framesToCheck == ALL_FRAMES)
        {
            m_params.stream.framesToCheck = m_info->totalFrames;
        }

        if (params.type == TEST_TYPE_H264_DECODE_RESOLUTION_CHANGE_DPB)
        {
            m_pictureParameterUpdateTriggerHack = 3;
        }
    }

    TestType getTestType() const
    {
        return m_params.type;
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

    const ClipInfo *getClipInfo() const
    {
        return m_info;
    };

    VkVideoCodecOperationFlagBitsKHR getCodecOperation(int session) const
    {
        return m_profiles[session].GetCodecType();
    }

    const VkVideoCoreProfile *getProfile(int session) const
    {
        return &m_profiles[session];
    }

    const std::string getTestName() const
    {
        std::stringstream oss;
        oss << testTypeToStr(getTestType());
        if (isLayered())
            oss << "_layered_dpb";
        else
            oss << "_separated_dpb";

        return oss.str();
    }

    int framesToCheck() const
    {
        return m_params.stream.framesToCheck;
    }

    bool hasOption(DecoderOption o) const
    {
        return (m_params.stream.decoderOptions & o) != 0;
    }

    bool isLayered() const
    {
        return m_isLayeredDpb;
    }

    int getParamaterUpdateHackRequirement() const
    {
        return m_pictureParameterUpdateTriggerHack;
    }

    VideoDevice::VideoDeviceFlags requiredDeviceFlags() const
    {
        VideoDevice::VideoDeviceFlags flags = VideoDevice::VIDEO_DEVICE_FLAG_REQUIRE_SYNC2_OR_NOT_SUPPORTED;

        if (hasOption(DecoderOption::UseStatusQueries))
            flags |= VideoDevice::VIDEO_DEVICE_FLAG_QUERY_WITH_STATUS_FOR_DECODE_SUPPORT;

        if (hasOption(DecoderOption::UseInlineStatusQueries) || hasOption(DecoderOption::ResourcesWithoutProfiles))
            flags |= VideoDevice::VIDEO_DEVICE_FLAG_REQUIRE_MAINTENANCE_1;

        return flags;
    }

    const VkExtensionProperties *extensionProperties(int session) const
    {
        static const VkExtensionProperties h264StdExtensionVersion = {
            VK_STD_VULKAN_VIDEO_CODEC_H264_DECODE_EXTENSION_NAME, VK_STD_VULKAN_VIDEO_CODEC_H264_DECODE_SPEC_VERSION};
        static const VkExtensionProperties h265StdExtensionVersion = {
            VK_STD_VULKAN_VIDEO_CODEC_H265_DECODE_EXTENSION_NAME, VK_STD_VULKAN_VIDEO_CODEC_H265_DECODE_SPEC_VERSION};
        static const VkExtensionProperties av1StdExtensionVersion = {
            VK_STD_VULKAN_VIDEO_CODEC_AV1_DECODE_EXTENSION_NAME, VK_STD_VULKAN_VIDEO_CODEC_AV1_DECODE_SPEC_VERSION};

        switch (m_profiles[session].GetCodecType())
        {
        case VK_VIDEO_CODEC_OPERATION_DECODE_H264_BIT_KHR:
            return &h264StdExtensionVersion;
        case VK_VIDEO_CODEC_OPERATION_DECODE_H265_BIT_KHR:
            return &h265StdExtensionVersion;
        case VK_VIDEO_CODEC_OPERATION_DECODE_AV1_BIT_KHR:
            return &av1StdExtensionVersion;
        default:
            tcu::die("Unsupported video codec %s\n", util::codecToName(m_profiles[session].GetCodecType()));
            break;
        }

        TCU_THROW(InternalError, "Unsupported codec");
    };

    void updateHash(uint32_t baseHash)
    {
        m_hash = deUint32Hash(baseHash);
    }

private:
    DecodeTestParam m_params;
    const ClipInfo *m_info{};
    uint32_t m_hash{};
    bool m_isLayeredDpb{};
    std::vector<VkVideoCoreProfile> m_profiles;
    // The 1-based count of parameter set updates after which to force
    // a parameter object release.  This is required due to the design
    // of the NVIDIA decode-client API. It sends parameter updates and
    // expects constructed parameter objects back synchronously,
    // before the next video session is created in a following
    // BeginSequence call.
    int m_pictureParameterUpdateTriggerHack{0}; // Zero is "off"
};

// Vulkan video is not supported on android platform
// all external libraries, helper functions and test instances has been excluded
#ifdef DE_BUILD_VIDEO

static std::shared_ptr<VideoBaseDecoder> decoderFromTestDefinition(DeviceContext *devctx, const TestDefinition &test,
                                                                   bool forceDisableFilmGrain = false)
{
    VkSharedBaseObj<VulkanVideoFrameBuffer> vkVideoFrameBuffer;
    VK_CHECK(VulkanVideoFrameBuffer::Create(devctx, test.hasOption(DecoderOption::UseStatusQueries),
                                            test.hasOption(DecoderOption::ResourcesWithoutProfiles),
                                            vkVideoFrameBuffer));

    VideoBaseDecoder::Parameters params;
    params.profile                           = test.getProfile(0);
    params.context                           = devctx;
    params.framebuffer                       = vkVideoFrameBuffer;
    params.framesToCheck                     = test.framesToCheck();
    params.layeredDpb                        = test.isLayered();
    params.queryDecodeStatus                 = test.hasOption(DecoderOption::UseStatusQueries);
    params.useInlineQueries                  = test.hasOption(DecoderOption::UseInlineStatusQueries);
    params.useInlineSessionParams            = test.hasOption(DecoderOption::UseInlineSessionParams);
    params.resetCodecNoSessionParams         = test.hasOption(DecoderOption::ResetCodecNoSessionParams);
    params.resourcesWithoutProfiles          = test.hasOption(DecoderOption::ResourcesWithoutProfiles);
    params.outOfOrderDecoding                = test.hasOption(DecoderOption::CachedDecoding);
    params.alwaysRecreateDPB                 = test.hasOption(DecoderOption::RecreateDPBImages);
    params.intraOnlyDecoding                 = test.hasOption(DecoderOption::IntraOnlyDecoding);
    params.pictureParameterUpdateTriggerHack = test.getParamaterUpdateHackRequirement();
    params.forceDisableFilmGrain             = forceDisableFilmGrain;

    return std::make_shared<VideoBaseDecoder>(std::move(params));
}

struct DownloadedFrame
{
    std::vector<uint8_t> luma;
    std::vector<uint8_t> cb;
    std::vector<uint8_t> cr;

    std::string checksum() const
    {
        MD5Digest digest;
        MD5Context ctx{};
        MD5Init(&ctx);
        MD5Update(&ctx, luma.data(), luma.size());
        MD5Update(&ctx, cb.data(), cb.size());
        MD5Update(&ctx, cr.data(), cr.size());
        MD5Final(&digest, &ctx);
        return MD5DigestToBase16(digest);
    }
};

inline uint16_t roru16(uint16_t x, uint16_t n)
{
    return n == 0 ? x : (x >> n) | (x << (-n & 15));
}

static void copyAllPlanesToBuffers(const DeviceDriver &vkd, const DecodedFrame &frame, const VkExtent2D &imageExtent,
                                   const PlanarFormatDescription &planarDescription, VkCommandBuffer cmdbuf,
                                   std::vector<std::unique_ptr<BufferWithMemory>> &planeBuffers)
{
    for (uint32_t planeNdx = 0; planeNdx < planarDescription.numPlanes; planeNdx++)
    {
        uint32_t width = imageExtent.width;
        if (planeNdx > 0)
            width = (width + 1) & ~1;

        width /= planarDescription.planes[planeNdx].widthDivisor;
        uint32_t height                    = imageExtent.height / planarDescription.planes[planeNdx].heightDivisor;
        VkExtent3D planeExtent             = {width, height, 1u};
        const VkImageAspectFlagBits aspect = getPlaneAspect(planeNdx);
        {

            const VkImageMemoryBarrier preCopyBarrier = {
                VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                nullptr,
                0u,
                VK_ACCESS_TRANSFER_READ_BIT,
                VK_IMAGE_LAYOUT_GENERAL,
                VK_IMAGE_LAYOUT_GENERAL,
                VK_QUEUE_FAMILY_IGNORED,
                VK_QUEUE_FAMILY_IGNORED,
                frame.outputImageView->GetImageResource()->GetImage(),
                {(VkImageAspectFlags)aspect, 0u, 1u, frame.imageLayerIndex, 1u}};

            vkd.cmdPipelineBarrier(cmdbuf, (VkPipelineStageFlags)VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                                   (VkPipelineStageFlags)VK_PIPELINE_STAGE_TRANSFER_BIT, (VkDependencyFlags)0u, 0u,
                                   nullptr, 0u, nullptr, 1u, &preCopyBarrier);
        }
        {
            const VkBufferImageCopy copy = {0u, // bufferOffset
                                            0u, // bufferRowLength
                                            0u, // bufferImageHeight
                                            {(VkImageAspectFlags)aspect, 0u, frame.imageLayerIndex, 1u},
                                            makeOffset3D(0u, 0u, 0u),
                                            planeExtent};
            vkd.cmdCopyImageToBuffer(cmdbuf, frame.outputImageView->GetImageResource()->GetImage(),
                                     VK_IMAGE_LAYOUT_GENERAL, planeBuffers[planeNdx]->get(), 1u, &copy);
        }
        {
            const VkBufferMemoryBarrier postCopyBarrier = {VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
                                                           nullptr,
                                                           VK_ACCESS_TRANSFER_WRITE_BIT,
                                                           VK_ACCESS_HOST_READ_BIT,
                                                           VK_QUEUE_FAMILY_IGNORED,
                                                           VK_QUEUE_FAMILY_IGNORED,
                                                           planeBuffers[planeNdx]->get(),
                                                           0u,
                                                           VK_WHOLE_SIZE};

            vkd.cmdPipelineBarrier(cmdbuf, (VkPipelineStageFlags)VK_PIPELINE_STAGE_TRANSFER_BIT,
                                   (VkPipelineStageFlags)VK_PIPELINE_STAGE_HOST_BIT, (VkDependencyFlags)0u, 0u, nullptr,
                                   1u, &postCopyBarrier, 0u, nullptr);
        }
    }
}

DownloadedFrame getDecodedImage(DeviceContext &devctx, VkImageLayout originalLayout, const DecodedFrame &frame)
{
    auto &vkd                     = devctx.getDeviceDriver();
    auto device                   = devctx.device;
    auto queueFamilyIndexDecode   = devctx.decodeQueueFamilyIdx();
    auto queueFamilyIndexTransfer = devctx.transferQueueFamilyIdx();
    const VkExtent2D imageExtent{(uint32_t)frame.displayWidth, (uint32_t)frame.displayHeight};
    const VkImage image         = frame.outputImageView->GetImageResource()->GetImage();
    const VkFormat format       = frame.outputImageView->GetImageResource()->GetImageCreateInfo().format;
    const VkQueue queueDecode   = getDeviceQueue(vkd, device, queueFamilyIndexDecode, 0u);
    const VkQueue queueTransfer = getDeviceQueue(vkd, device, queueFamilyIndexTransfer, 0u);

    PlanarFormatDescription planarDescription = getPlanarFormatDescription(format);
    DE_ASSERT(planarDescription.numPlanes == 2 || planarDescription.numPlanes == 3);
    const VkImageSubresourceRange imageSubresourceRange =
        makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, frame.imageLayerIndex, 1);

    if (videoLoggingEnabled())
    {
        tcu::print(";;; getDecodedImage layer=%d arrayLayers=%d\n", frame.imageLayerIndex,
                   frame.outputImageView->GetImageResource()->GetImageCreateInfo().arrayLayers);
    }

    const Move<VkCommandPool> cmdDecodePool(makeCommandPool(vkd, device, queueFamilyIndexDecode));
    const Move<VkCommandBuffer> cmdDecodeBuffer(
        allocateCommandBuffer(vkd, device, *cmdDecodePool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));
    const Move<VkCommandPool> cmdTransferPool(makeCommandPool(vkd, device, queueFamilyIndexTransfer));
    const Move<VkCommandBuffer> cmdTransferBuffer(
        allocateCommandBuffer(vkd, device, *cmdTransferPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

    if (frame.frameCompleteFence != VK_NULL_HANDLE)
        VK_CHECK(vkd.waitForFences(device, 1, &frame.frameCompleteFence, VK_FALSE, ~(uint64_t)0u));

    auto computePlaneSize = [](const VkExtent2D extent, const PlanarFormatDescription &desc, int plane)
    {
        uint32_t w = extent.width;
        uint32_t h = extent.height;

        if (plane > 0)
        {
            w = (w + 1) /
                desc.planes[plane]
                    .widthDivisor; // This is what libaom does, but probably not the h/w - there's ambiguity about what to do for non-even dimensions imo
            h = (h + 1) / desc.planes[plane].heightDivisor;
            return w * h * desc.planes[plane].elementSizeBytes;
        }

        return w * h * desc.planes[plane].elementSizeBytes;
    };

    // Create a buffer to hold each planes' samples.
    std::vector<std::unique_ptr<BufferWithMemory>> planeBuffers;
    planeBuffers.reserve(planarDescription.numPlanes);
    for (int plane = 0; plane < planarDescription.numPlanes; plane++)
    {
        const VkBufferCreateInfo bufferInfo = {
            VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            nullptr,
            (VkBufferCreateFlags)0u,
            computePlaneSize(imageExtent, planarDescription, plane),
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_SHARING_MODE_EXCLUSIVE,
            0u,
            nullptr,
        };
        planeBuffers.emplace_back(new BufferWithMemory(vkd, device, devctx.allocator(), bufferInfo,
                                                       MemoryRequirement::HostVisible | MemoryRequirement::Any));
    }

    Move<VkFence> decodeFence                   = createFence(vkd, device);
    Move<VkFence> transferFence                 = createFence(vkd, device);
    Move<VkSemaphore> semaphore                 = createSemaphore(vkd, device);
    const VkPipelineStageFlags waitDstStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;

    // First release the image from the decode queue to the xfer queue, transitioning it to a friendly format for copying.
    const VkImageMemoryBarrier2KHR barrierDecodeRelease = makeImageMemoryBarrier2(
        VK_PIPELINE_STAGE_2_VIDEO_DECODE_BIT_KHR, VK_ACCESS_2_VIDEO_DECODE_WRITE_BIT_KHR, VK_PIPELINE_STAGE_2_NONE,
        0, // ignored for release operations
        originalLayout, VK_IMAGE_LAYOUT_GENERAL, image, imageSubresourceRange, queueFamilyIndexDecode,
        queueFamilyIndexTransfer);

    // And acquire it on the transfer queue
    const VkImageMemoryBarrier2KHR barrierTransferAcquire =
        makeImageMemoryBarrier2(VK_PIPELINE_STAGE_2_NONE,
                                0, // ignored for acquire operations
                                VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_MEMORY_READ_BIT, originalLayout,
                                VK_IMAGE_LAYOUT_GENERAL, // must match decode release
                                image,                   // must match decode release
                                imageSubresourceRange,   // must match decode release
                                queueFamilyIndexDecode,  // must match decode release
                                queueFamilyIndexTransfer);

    beginCommandBuffer(vkd, *cmdDecodeBuffer, 0u);
    cmdPipelineImageMemoryBarrier2(vkd, *cmdDecodeBuffer, &barrierDecodeRelease);
    endCommandBuffer(vkd, *cmdDecodeBuffer);

    bool haveSemaphore = frame.frameCompleteSemaphore != VK_NULL_HANDLE;
    const VkSubmitInfo decodeSubmitInfo{
        VK_STRUCTURE_TYPE_SUBMIT_INFO,
        nullptr,
        haveSemaphore ? 1u : 0u,
        haveSemaphore ? &frame.frameCompleteSemaphore : nullptr,
        haveSemaphore ? &waitDstStageMask : nullptr,
        1u,
        &*cmdDecodeBuffer,
        1u,
        &*semaphore,
    };
    VK_CHECK(vkd.queueSubmit(queueDecode, 1u, &decodeSubmitInfo, *decodeFence));
    VK_CHECK(vkd.waitForFences(device, 1, &*decodeFence, true, ~0ull));

    beginCommandBuffer(vkd, *cmdTransferBuffer, 0u);
    cmdPipelineImageMemoryBarrier2(vkd, *cmdTransferBuffer, &barrierTransferAcquire);
    copyAllPlanesToBuffers(vkd, frame, imageExtent, planarDescription, *cmdTransferBuffer, planeBuffers);
    endCommandBuffer(vkd, *cmdTransferBuffer);

    const VkSubmitInfo transferSubmitInfo{
        VK_STRUCTURE_TYPE_SUBMIT_INFO, // VkStructureType                              sType;
        nullptr,                       // const void*                                  pNext;
        1u,                            // uint32_t                                             waitSemaphoreCount;
        &*semaphore,                   // const VkSemaphore*                   pWaitSemaphores;
        &waitDstStageMask,             // const VkPipelineStageFlags*  pWaitDstStageMask;
        1u,                            // uint32_t                                             commandBufferCount;
        &*cmdTransferBuffer,           // const VkCommandBuffer*               pCommandBuffers;
        1u,                            // uint32_t                                             signalSemaphoreCount;
        &*semaphore,                   // const VkSemaphore*                   pSignalSemaphores;
    };
    VK_CHECK(vkd.queueSubmit(queueTransfer, 1u, &transferSubmitInfo, *transferFence));
    VK_CHECK(vkd.waitForFences(device, 1, &*transferFence, true, ~0ull));

    DownloadedFrame downloadedFrame;
    { // download the data from the buffers into the host buffers
        tcu::UVec4 channelDepths = ycbcr::getYCbCrBitDepth(format);
        DE_ASSERT(channelDepths.x() == channelDepths.y() &&
                  channelDepths.y() == channelDepths.z()); // Sanity for interface mismatch
        int bitDepth = channelDepths.x();
        DE_ASSERT(bitDepth == 8 || bitDepth == 10 || bitDepth == 12 || bitDepth == 16);
        TCU_CHECK_AND_THROW(InternalError, bitDepth != 16, "16-bit samples have not been tested yet");

        bool highBitDepth = bitDepth > 8;

        // Luma first
        {
            invalidateMappedMemoryRange(vkd, device, planeBuffers[0]->getAllocation().getMemory(), 0u, VK_WHOLE_SIZE);
            VkDeviceSize planeSize = computePlaneSize(imageExtent, planarDescription, 0);
            downloadedFrame.luma.resize(planeSize);
            if (highBitDepth && bitDepth != 16) // 16-bit can take the straight memcpy case below, no undefined bits.
            {
                const uint16_t *const samples = (uint16_t *)planeBuffers[0]->getAllocation().getHostPtr();
                uint16_t *const outputSamples = (uint16_t *)downloadedFrame.luma.data();

                //int shift = 16 - bitDepth;
                if (bitDepth == 10)
                {
                    for (VkDeviceSize sampleIdx = 0; sampleIdx < (imageExtent.width * imageExtent.height); sampleIdx++)
                    {
                        uint16_t sample          = samples[sampleIdx];
                        outputSamples[sampleIdx] = roru16(sample, 6);
                    }
                }
                else if (bitDepth == 12)
                {
                    for (VkDeviceSize sampleIdx = 0; sampleIdx < planeSize; sampleIdx++)
                    {
                        uint16_t sample                 = samples[sampleIdx];
                        downloadedFrame.luma[sampleIdx] = roru16(sample, 4);
                    }
                }
            }
            else
            {
                DE_ASSERT(bitDepth == 8);
                deMemcpy(downloadedFrame.luma.data(), planeBuffers[0]->getAllocation().getHostPtr(), planeSize);
            }
        }
        if (planarDescription.numPlanes == 2)
        {
            invalidateMappedMemoryRange(vkd, device, planeBuffers[1]->getAllocation().getMemory(), 0u, VK_WHOLE_SIZE);
            // Interleaved formats, deinterleave for MD5 comparisons (matches most other tool's notion of "raw YUV format")
            //   this is a very slow operation, and accounts for ~80% of the decode test runtime.
            //   it might be better to use reference checksums in the original hardware format, rather than xforming each time
            //   but that makes comparison to software references more difficult...
            VkDeviceSize planeSize = computePlaneSize(imageExtent, planarDescription, 1);
            VkDeviceSize numWords  = planeSize / sizeof(uint16_t);
            uint16_t *wordSamples  = (uint16_t *)planeBuffers[1]->getAllocation().getHostPtr();
            downloadedFrame.cb.resize(numWords);
            downloadedFrame.cr.resize(numWords);
            if (bitDepth == 8)
            {
                for (int i = 0; i < numWords; i++)
                {
                    downloadedFrame.cb[i] = wordSamples[i] & 0xFF;
                    downloadedFrame.cr[i] = (wordSamples[i] >> 8) & 0xFF;
                }
            }
            else
            {
                DE_ASSERT(bitDepth == 10 || bitDepth == 12 || bitDepth == 16);
                uint16_t *sampleWordsCb16 = (uint16_t *)downloadedFrame.cb.data();
                uint16_t *sampleWordsCr16 = (uint16_t *)downloadedFrame.cr.data();
                for (int i = 0; i < numWords / 2; i++)
                {
                    sampleWordsCb16[i] = roru16(wordSamples[2 * i], 16 - bitDepth);
                    sampleWordsCr16[i] = roru16(wordSamples[2 * i + 1], 16 - bitDepth);
                }
            }
        }
        else if (planarDescription.numPlanes == 3)
        {
            invalidateMappedMemoryRange(vkd, device, planeBuffers[1]->getAllocation().getMemory(), 0u, VK_WHOLE_SIZE);

            // Happy case, not deinterleaving, just straight memcpying
            uint32_t evenWidth = (imageExtent.width + 1) & ~1;

            // Not sure, but don't think chroma planes can be subsampled differently.
            DE_ASSERT(planarDescription.planes[1].widthDivisor == planarDescription.planes[2].widthDivisor);
            DE_ASSERT(planarDescription.planes[1].heightDivisor == planarDescription.planes[2].heightDivisor);
            DE_ASSERT(planarDescription.planes[1].elementSizeBytes == planarDescription.planes[2].elementSizeBytes);

            uint32_t width  = evenWidth / planarDescription.planes[1].widthDivisor;
            uint32_t height = imageExtent.height / planarDescription.planes[1].heightDivisor;

            VkDeviceSize cbPlaneSize = width * height * planarDescription.planes[1].elementSizeBytes;
            downloadedFrame.cb.resize(cbPlaneSize);
            deMemcpy(downloadedFrame.cb.data(), planeBuffers[1]->getAllocation().getHostPtr(),
                     downloadedFrame.cb.size());

            invalidateMappedMemoryRange(vkd, device, planeBuffers[2]->getAllocation().getMemory(), 0u, VK_WHOLE_SIZE);
            downloadedFrame.cr.resize(cbPlaneSize);
            deMemcpy(downloadedFrame.cr.data(), planeBuffers[2]->getAllocation().getHostPtr(),
                     downloadedFrame.cr.size());
        }
    }

    // We're nearly there, the pain is almost over, release the image
    // from the xfer queue, give it back to the decode queue, and
    // transition it back to the original layout.
    const VkImageMemoryBarrier2KHR barrierTransferRelease = makeImageMemoryBarrier2(
        VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_MEMORY_WRITE_BIT, VK_PIPELINE_STAGE_2_NONE,
        0, // ignored for release
        VK_IMAGE_LAYOUT_GENERAL, originalLayout, image, imageSubresourceRange, queueFamilyIndexTransfer,
        queueFamilyIndexDecode);

    const VkImageMemoryBarrier2KHR barrierDecodeAcquire = makeImageMemoryBarrier2(
        VK_PIPELINE_STAGE_2_NONE,
        0, // ignored for acquire
        VK_PIPELINE_STAGE_2_VIDEO_DECODE_BIT_KHR, VK_ACCESS_2_VIDEO_DECODE_WRITE_BIT_KHR, VK_IMAGE_LAYOUT_GENERAL,
        originalLayout, image, imageSubresourceRange, queueFamilyIndexTransfer, queueFamilyIndexDecode);

    vkd.resetCommandBuffer(*cmdTransferBuffer, 0u);
    vkd.resetCommandBuffer(*cmdDecodeBuffer, 0u);

    vkd.resetFences(device, 1, &*transferFence);
    vkd.resetFences(device, 1, &*decodeFence);

    beginCommandBuffer(vkd, *cmdTransferBuffer, 0u);
    cmdPipelineImageMemoryBarrier2(vkd, *cmdTransferBuffer, &barrierTransferRelease);
    endCommandBuffer(vkd, *cmdTransferBuffer);

    const VkSubmitInfo transferSubmitInfo2{
        VK_STRUCTURE_TYPE_SUBMIT_INFO, // VkStructureType                              sType;
        nullptr,                       // const void*                                  pNext;
        1u,                            // uint32_t                                             waitSemaphoreCount;
        &*semaphore,                   // const VkSemaphore*                   pWaitSemaphores;
        &waitDstStageMask,             // const VkPipelineStageFlags*  pWaitDstStageMask;
        1u,                            // uint32_t                                             commandBufferCount;
        &*cmdTransferBuffer,           // const VkCommandBuffer*               pCommandBuffers;
        1u,                            // uint32_t                                             signalSemaphoreCount;
        &*semaphore,                   // const VkSemaphore*                   pSignalSemaphores;
    };
    VK_CHECK(vkd.queueSubmit(queueTransfer, 1u, &transferSubmitInfo2, *transferFence));
    VK_CHECK(vkd.waitForFences(device, 1, &*transferFence, true, ~0ull));

    beginCommandBuffer(vkd, *cmdDecodeBuffer, 0u);
    cmdPipelineImageMemoryBarrier2(vkd, *cmdDecodeBuffer, &barrierDecodeAcquire);
    endCommandBuffer(vkd, *cmdDecodeBuffer);

    const VkSubmitInfo decodeSubmitInfo2{
        VK_STRUCTURE_TYPE_SUBMIT_INFO, // VkStructureType                              sType;
        nullptr,                       // const void*                                  pNext;
        1u,                            // uint32_t                                             waitSemaphoreCount;
        &*semaphore,                   // const VkSemaphore*                   pWaitSemaphores;
        &waitDstStageMask,             // const VkPipelineStageFlags*  pWaitDstStageMask;
        1u,                            // uint32_t                                             commandBufferCount;
        &*cmdDecodeBuffer,             // const VkCommandBuffer*               pCommandBuffers;
        0u,                            // uint32_t                                             signalSemaphoreCount;
        nullptr,                       // const VkSemaphore*                   pSignalSemaphores;
    };
    VK_CHECK(vkd.queueSubmit(queueDecode, 1u, &decodeSubmitInfo2, *decodeFence));
    VK_CHECK(vkd.waitForFences(device, 1, &*decodeFence, true, ~0ull));

    return downloadedFrame;
}

class VideoDecodeTestInstance : public VideoBaseTestInstance
{
public:
    VideoDecodeTestInstance(Context &context, const TestDefinition *testDefinition);
    tcu::TestStatus iterate(void);

protected:
    const TestDefinition *m_testDefinition;
    static_assert(sizeof(DeviceContext) < 128, "DeviceContext has grown bigger than expected!");
    DeviceContext m_deviceContext;
};

class InterleavingDecodeTestInstance : public VideoBaseTestInstance
{
public:
    InterleavingDecodeTestInstance(Context &context, const std::vector<MovePtr<TestDefinition>> &testDefinitions);
    tcu::TestStatus iterate(void);

protected:
    const std::vector<MovePtr<TestDefinition>> &m_testDefinitions;
    DeviceContext m_deviceContext;
    static_assert(sizeof(DeviceContext) < 128, "DeviceContext has grown bigger than expected!");
};

InterleavingDecodeTestInstance::InterleavingDecodeTestInstance(
    Context &context, const std::vector<MovePtr<TestDefinition>> &testDefinitions)
    : VideoBaseTestInstance(context)
    , m_testDefinitions(std::move(testDefinitions))
    , m_deviceContext(&m_context, &m_videoDevice)
{
    int requiredCodecs                                = VK_VIDEO_CODEC_OPERATION_NONE_KHR;
    VideoDevice::VideoDeviceFlags requiredDeviceFlags = VideoDevice::VideoDeviceFlagBits::VIDEO_DEVICE_FLAG_NONE;
    for (const auto &test : m_testDefinitions)
    {
        VkVideoCodecOperationFlagBitsKHR testBits = test->getCodecOperation(0);
        requiredCodecs |= testBits;
        requiredDeviceFlags |= test->requiredDeviceFlags();
    }
    VkDevice device = getDeviceSupportingQueue(VK_QUEUE_VIDEO_DECODE_BIT_KHR | VK_QUEUE_TRANSFER_BIT, requiredCodecs,
                                               requiredDeviceFlags);

    m_deviceContext.updateDevice(
        m_context.getPhysicalDevice(), device,
        getDeviceQueue(m_context.getDeviceInterface(), device, m_videoDevice.getQueueFamilyIndexDecode(), 0), nullptr,
        getDeviceQueue(m_context.getDeviceInterface(), device, m_videoDevice.getQueueFamilyIndexTransfer(), 0));
}

VideoDecodeTestInstance::VideoDecodeTestInstance(Context &context, const TestDefinition *testDefinition)
    : VideoBaseTestInstance(context)
    , m_testDefinition(testDefinition)
    , m_deviceContext(&m_context, &m_videoDevice)
{
    VkDevice device =
        getDeviceSupportingQueue(VK_QUEUE_VIDEO_DECODE_BIT_KHR | VK_QUEUE_TRANSFER_BIT,
                                 m_testDefinition->getCodecOperation(0), m_testDefinition->requiredDeviceFlags());

    m_deviceContext.updateDevice(
        m_context.getPhysicalDevice(), device,
        getDeviceQueue(m_context.getDeviceInterface(), device, m_videoDevice.getQueueFamilyIndexDecode(), 0), nullptr,
        getDeviceQueue(m_context.getDeviceInterface(), device, m_videoDevice.getQueueFamilyIndexTransfer(), 0));
}

static std::unique_ptr<FrameProcessor> createProcessor(const TestDefinition *td, DeviceContext *dctx,
                                                       bool forceDisableFilmGrain = false)
{
    Demuxer::Params demuxParams = {};
    demuxParams.data            = std::make_unique<BufferedReader>(td->getClipFilePath());
    demuxParams.codecOperation  = td->getCodecOperation(0);
    demuxParams.framing         = td->getClipInfo()->framing;

    auto demuxer = Demuxer::create(std::move(demuxParams));

    std::shared_ptr<VideoBaseDecoder> decoder = decoderFromTestDefinition(dctx, *td, forceDisableFilmGrain);

    return std::make_unique<FrameProcessor>(std::move(demuxer), decoder);
}

tcu::TestStatus VideoDecodeTestInstance::iterate()
{
    bool filmGrainPresent                     = m_testDefinition->hasOption(DecoderOption::FilmGrainPresent);
    std::unique_ptr<FrameProcessor> processor = createProcessor(m_testDefinition, &m_deviceContext);
    std::unique_ptr<FrameProcessor> processorWithoutFilmGrain;
    if (filmGrainPresent)
    {
        processorWithoutFilmGrain = createProcessor(m_testDefinition, &m_deviceContext, true);
    }

    std::vector<int> incorrectFrames;
    std::vector<int> correctFrames;

    if (m_testDefinition->hasOption(DecoderOption::CachedDecoding))
    {
        processor->decodeFrameOutOfOrder(m_testDefinition->framesToCheck());
        if (processorWithoutFilmGrain)
            processorWithoutFilmGrain->decodeFrameOutOfOrder(m_testDefinition->framesToCheck());
    }

    bool hasSeparateOutputImages = !processor->m_decoder->dpbAndOutputCoincide() || filmGrainPresent;

    std::FILE *debug_OutputFileHandle;

    auto openTemporaryFile = [](const std::string &basename, std::FILE **handle)
    {
        DE_ASSERT(handle != nullptr);
        *handle = std::fopen(basename.c_str(), "wb");
        DE_ASSERT(*handle != nullptr);
    };

    if (debug_WriteOutFramesToSingleFile)
    {
        openTemporaryFile("output.yuv", &debug_OutputFileHandle);
    }

    bool throwFilmGrainQualityWarning = false;
    for (int frameNumber = 0; frameNumber < m_testDefinition->framesToCheck(); frameNumber++)
    {
        DecodedFrame frame;
        TCU_CHECK_AND_THROW(
            InternalError, processor->getNextFrame(&frame) > 0,
            "Expected more frames from the bitstream. Most likely an internal CTS bug, or maybe an invalid bitstream");
        DecodedFrame frameWithoutFilmgGrain;
        if (processorWithoutFilmGrain)
        {
            TCU_CHECK_AND_THROW(InternalError, processorWithoutFilmGrain->getNextFrame(&frameWithoutFilmgGrain) > 0,
                                "Expected more frames from the bitstream. Most likely an internal CTS bug, or maybe an "
                                "invalid bitstream");
        }

        if (debug_WriteOutFramesToSeparateFiles)
        {
            std::stringstream oss;
            oss << "output" << frame.displayOrder << "_" << frame.displayWidth << "_" << frame.displayHeight << ".yuv";
            openTemporaryFile(oss.str(), &debug_OutputFileHandle);
        }

        DownloadedFrame downloadedFrame = getDecodedImage(
            m_deviceContext,
            hasSeparateOutputImages ? VK_IMAGE_LAYOUT_VIDEO_DECODE_DST_KHR : VK_IMAGE_LAYOUT_VIDEO_DECODE_DPB_KHR,
            frame);

        DownloadedFrame downloadedFrameWithoutFilmGrain;
        if (processorWithoutFilmGrain)
        {
            downloadedFrameWithoutFilmGrain =
                getDecodedImage(m_deviceContext,
                                !processor->m_decoder->dpbAndOutputCoincide() ? VK_IMAGE_LAYOUT_VIDEO_DECODE_DST_KHR :
                                                                                VK_IMAGE_LAYOUT_VIDEO_DECODE_DPB_KHR,
                                frameWithoutFilmgGrain);
        }

        if (debug_WriteOutFramesToSingleFile || debug_WriteOutFramesToSeparateFiles)
        {
            DownloadedFrame &frameToWriteOut =
                debug_WriteOutFilmGrain ? downloadedFrame : downloadedFrameWithoutFilmGrain;
            fwrite(frameToWriteOut.luma.data(), 1, frameToWriteOut.luma.size(), debug_OutputFileHandle);
            fwrite(frameToWriteOut.cb.data(), 1, frameToWriteOut.cb.size(), debug_OutputFileHandle);
            fwrite(frameToWriteOut.cr.data(), 1, frameToWriteOut.cr.size(), debug_OutputFileHandle);
            fflush(debug_OutputFileHandle);
        }

        if (debug_WriteOutFramesToSeparateFiles)
        {
            std::fclose(debug_OutputFileHandle);
        }

        std::string checksum       = checksumForClipFrame(m_testDefinition->getClipInfo(), frameNumber);
        std::string actualChecksum = downloadedFrame.checksum();

        double psnr = 0.0;
        if (processorWithoutFilmGrain)
        {
            static const double numPlanes = 3.0;
            util::PSNR(downloadedFrame.luma, downloadedFrameWithoutFilmGrain.luma);
            psnr += util::PSNR(downloadedFrame.cb, downloadedFrameWithoutFilmGrain.cb);
            psnr += util::PSNR(downloadedFrame.cr, downloadedFrameWithoutFilmGrain.cr);
            psnr /= numPlanes;
        }

        if (actualChecksum == checksum)
        {
            correctFrames.push_back(frameNumber);
        }
        else
        {
            if (processorWithoutFilmGrain)
            {
                if (!de::inRange(psnr, kFilmgrainPSNRLowerBound, kFilmgrainPSNRUpperBound))
                {
                    incorrectFrames.push_back(frameNumber);
                    throwFilmGrainQualityWarning = true;
                }
                else
                {
                    correctFrames.push_back(frameNumber);
                }
            }
            else
            {
                incorrectFrames.push_back(frameNumber);
            }
        }
    }

    if (debug_WriteOutFramesToSingleFile)
    {
        fclose(debug_OutputFileHandle);
    }

    if (!correctFrames.empty() && correctFrames.size() == m_testDefinition->framesToCheck())
    {
        std::stringstream oss;
        oss << m_testDefinition->framesToCheck() << " correctly decoded frames";
        return tcu::TestStatus::pass(oss.str());
    }
    else
    {
        std::stringstream ss;
        ss << correctFrames.size() << " out of " << m_testDefinition->framesToCheck() << " frames rendered correctly (";
        if (correctFrames.size() < incorrectFrames.size())
        {
            ss << "correct frames: ";
            for (int i : correctFrames)
                ss << i << " ";
        }
        else
        {
            ss << "incorrect frames: ";
            for (int i : incorrectFrames)
                ss << i << " ";
        }
        ss << "\b)";

        if (throwFilmGrainQualityWarning)
        {
            TCU_THROW(QualityWarning, std::string("Potentially non-standard filmgrain synthesis process: ") + ss.str());
        }

        return tcu::TestStatus::fail(ss.str());
    }
}

tcu::TestStatus InterleavingDecodeTestInstance::iterate(void)
{
    std::vector<std::unique_ptr<FrameProcessor>> processors;
    for (int i = 0; i < m_testDefinitions.size(); i++)
    {
        processors.push_back(createProcessor(&*m_testDefinitions[i], &m_deviceContext));
    }

    // First cache up all the decoded frames from the various decode sessions
    for (int i = 0; i < m_testDefinitions.size(); i++)
    {
        const auto &test = m_testDefinitions[i];
        auto &processor  = processors[i];
        processor->bufferFrames(test->framesToCheck());
        DE_ASSERT(processor->getBufferedDisplayCount() == test->framesToCheck());
    }

    auto interleaveCacheSize    = processors[0]->m_decoder->m_cachedDecodeParams.size();
    auto firstStreamDecodeQueue = processors[0]->m_decoder->m_deviceContext->decodeQueue;

    size_t totalFrames = 0;
    for (auto &processor : processors)
    {
        auto &decoder = processor->m_decoder;
        DE_ASSERT(decoder->m_cachedDecodeParams.size() == interleaveCacheSize);
        DE_ASSERT(decoder->m_deviceContext->decodeQueue == firstStreamDecodeQueue);
        totalFrames += decoder->m_cachedDecodeParams.size();
    }

    DE_UNREF(firstStreamDecodeQueue);

    // Interleave command buffer recording
    for (int i = 0; i < interleaveCacheSize; i++)
    {
        for (auto &processor : processors)
        {
            auto &decoder = processor->m_decoder;
            decoder->WaitForFrameFences(decoder->m_cachedDecodeParams[i]);
            decoder->ApplyPictureParameters(decoder->m_cachedDecodeParams[i]);
            decoder->RecordCommandBuffer(decoder->m_cachedDecodeParams[i]);
        }
    }

    // Interleave submissions
    for (int i = 0; i < interleaveCacheSize; i++)
    {
        for (int idx = 0; idx < processors.size(); idx++)
        {
            auto &decoder = processors[idx]->m_decoder;
            auto &test    = m_testDefinitions[idx];
            decoder->SubmitQueue(decoder->m_cachedDecodeParams[i]);
            if (test->hasOption(DecoderOption::UseStatusQueries))
            {
                decoder->QueryDecodeResults(decoder->m_cachedDecodeParams[i]);
            }
        }
    }

    struct InterleavedDecodeResults
    {
        std::vector<int> correctFrames;
        std::vector<int> incorrectFrames;
    };
    std::vector<InterleavedDecodeResults> results(m_testDefinitions.size());

    for (int i = 0; i < m_testDefinitions.size(); i++)
    {
        auto &test      = m_testDefinitions[i];
        auto &processor = processors[i];
        bool hasSeparateOutputImages =
            processor->m_decoder->dpbAndOutputCoincide() && !test->hasOption(DecoderOption::FilmGrainPresent);
        for (int frameNumber = 0; frameNumber < m_testDefinitions[i]->framesToCheck(); frameNumber++)
        {
            DecodedFrame frame;
            TCU_CHECK_AND_THROW(InternalError, processor->getNextFrame(&frame) > 0,
                                "Expected more frames from the bitstream. Most likely an internal CTS bug, or maybe an "
                                "invalid bitstream");

            if (videoLoggingEnabled())
            {
                std::cout << "Frame decoded: picIdx" << static_cast<uint32_t>(frame.pictureIndex) << " "
                          << frame.displayWidth << "x" << frame.displayHeight << " "
                          << "decode/display " << frame.decodeOrder << "/" << frame.displayOrder << " "
                          << "dstImageView " << frame.outputImageView->GetImageView() << std::endl;
            }

            DownloadedFrame downloadedFrame = getDecodedImage(
                m_deviceContext,
                hasSeparateOutputImages ? VK_IMAGE_LAYOUT_VIDEO_DECODE_DPB_KHR : VK_IMAGE_LAYOUT_VIDEO_DECODE_DST_KHR,
                frame);

            std::string expectedChecksum = checksumForClipFrame(test->getClipInfo(), frameNumber);
            std::string actualChecksum   = downloadedFrame.checksum();
            if (actualChecksum == expectedChecksum)
            {
                results[i].correctFrames.push_back(frameNumber);
            }
            else
            {
                results[i].incorrectFrames.push_back(frameNumber);
            }
        }
    }

    bool allTestsPassed  = true;
    int totalFramesCheck = 0;
    for (const auto &res : results)
    {
        if (!res.incorrectFrames.empty())
            allTestsPassed = false;
        totalFramesCheck += (res.correctFrames.size() + res.incorrectFrames.size());
    }
    DE_ASSERT(totalFramesCheck == totalFrames);
    DE_UNREF(totalFramesCheck);

    if (allTestsPassed)
        return tcu::TestStatus::pass(de::toString(totalFrames) + " correctly decoded frames");
    else
    {
        stringstream ss;
        ss << "Interleaving failure: ";
        for (int i = 0; i < results.size(); i++)
        {
            const auto &result = results[i];
            if (!result.incorrectFrames.empty())
            {
                ss << " (stream #" << i << " incorrect frames: ";
                for (int frame : result.incorrectFrames)
                    ss << frame << " ";
                ss << "\b)";
            }
        }
        return tcu::TestStatus::fail(ss.str());
    }
}

#endif // #ifdef DE_BUILD_VIDEO

class VideoDecodeTestCase : public vkt::TestCase
{
public:
    VideoDecodeTestCase(tcu::TestContext &context, const char *name, MovePtr<TestDefinition> testDefinition)
        : vkt::TestCase(context, name)
        , m_testDefinition(testDefinition)
    {
    }

    TestInstance *createInstance(Context &context) const override;
    void checkSupport(Context &context) const override;

private:
    MovePtr<TestDefinition> m_testDefinition;
};

class InterleavingDecodeTestCase : public vkt::TestCase
{
public:
    InterleavingDecodeTestCase(tcu::TestContext &context, const char *name,
                               std::vector<MovePtr<TestDefinition>> &&testDefinitions)
        : vkt::TestCase(context, name)
        , m_testDefinitions(std::move(testDefinitions))
    {
    }

    TestInstance *createInstance(Context &context) const override
    {
#ifdef DE_BUILD_VIDEO
        return new InterleavingDecodeTestInstance(context, m_testDefinitions);
#endif
        DE_UNREF(context);
        return nullptr;
    }
    void checkSupport(Context &context) const override;

private:
    std::vector<MovePtr<TestDefinition>> m_testDefinitions;
};

TestInstance *VideoDecodeTestCase::createInstance(Context &context) const
{
#ifdef DE_BUILD_VIDEO
    return new VideoDecodeTestInstance(context, m_testDefinition.get());
#endif

#ifndef DE_BUILD_VIDEO
    DE_UNREF(context);
    return nullptr;
#endif
}

void VideoDecodeTestCase::checkSupport(Context &context) const
{
    context.requireDeviceFunctionality("VK_KHR_video_queue");
    context.requireDeviceFunctionality("VK_KHR_video_decode_queue");
    context.requireDeviceFunctionality("VK_KHR_synchronization2");

    switch (m_testDefinition->getTestType())
    {
    case TEST_TYPE_H264_DECODE_I:
    case TEST_TYPE_H264_DECODE_I_P:
    case TEST_TYPE_H264_DECODE_CLIP_A:
    case TEST_TYPE_H264_DECODE_I_P_NOT_MATCHING_ORDER:
    case TEST_TYPE_H264_DECODE_I_P_B_13:
    case TEST_TYPE_H264_DECODE_I_P_B_13_NOT_MATCHING_ORDER:
    case TEST_TYPE_H264_DECODE_QUERY_RESULT_WITH_STATUS:
    case TEST_TYPE_H264_DECODE_RESOLUTION_CHANGE:
    case TEST_TYPE_H264_DECODE_RESOLUTION_CHANGE_DPB:
    {
        context.requireDeviceFunctionality("VK_KHR_video_decode_h264");
        break;
    }
    case TEST_TYPE_H264_DECODE_INLINE_QUERY_RESULT_WITH_STATUS:
    case TEST_TYPE_H264_DECODE_RESOURCES_WITHOUT_PROFILES:
    {
        context.requireDeviceFunctionality("VK_KHR_video_decode_h264");
        context.requireDeviceFunctionality("VK_KHR_video_maintenance1");
        break;
    }
    case TEST_TYPE_H264_DECODE_INLINE_SESSION_PARAMS:
    case TEST_TYPE_H264_DECODE_RELAXED_SESSION_PARAMS:
    {
        context.requireDeviceFunctionality("VK_KHR_video_decode_h264");
        context.requireDeviceFunctionality("VK_KHR_video_maintenance2");
        break;
    }

    case TEST_TYPE_H265_DECODE_I:
    case TEST_TYPE_H265_DECODE_I_P:
    case TEST_TYPE_H265_DECODE_CLIP_D:
    case TEST_TYPE_H265_DECODE_I_P_NOT_MATCHING_ORDER:
    case TEST_TYPE_H265_DECODE_I_P_B_13:
    case TEST_TYPE_H265_DECODE_I_P_B_13_NOT_MATCHING_ORDER:
    case TEST_TYPE_H265_DECODE_QUERY_RESULT_WITH_STATUS:
    case TEST_TYPE_H265_DECODE_SLIST_A:
    case TEST_TYPE_H265_DECODE_SLIST_B:
    {
        context.requireDeviceFunctionality("VK_KHR_video_decode_h265");
        break;
    }
    case TEST_TYPE_H265_DECODE_INLINE_QUERY_RESULT_WITH_STATUS:
    case TEST_TYPE_H265_DECODE_RESOURCES_WITHOUT_PROFILES:
    {
        context.requireDeviceFunctionality("VK_KHR_video_decode_h265");
        context.requireDeviceFunctionality("VK_KHR_video_maintenance1");
        break;
    }
    case TEST_TYPE_H265_DECODE_INLINE_SESSION_PARAMS:
    case TEST_TYPE_H265_DECODE_RELAXED_SESSION_PARAMS:
    {
        context.requireDeviceFunctionality("VK_KHR_video_decode_h265");
        context.requireDeviceFunctionality("VK_KHR_video_maintenance2");
        break;
    }

    case TEST_TYPE_AV1_DECODE_I:
    case TEST_TYPE_AV1_DECODE_I_P:
    case TEST_TYPE_AV1_DECODE_I_P_NOT_MATCHING_ORDER:
    case TEST_TYPE_AV1_DECODE_BASIC_8:
    case TEST_TYPE_AV1_DECODE_BASIC_8_NOT_MATCHING_ORDER:
    case TEST_TYPE_AV1_DECODE_BASIC_10:
    case TEST_TYPE_AV1_DECODE_ALLINTRA_8:
    case TEST_TYPE_AV1_DECODE_ALLINTRA_NOSETUP_8:
    case TEST_TYPE_AV1_DECODE_ALLINTRA_BC_8:
    case TEST_TYPE_AV1_DECODE_GLOBALMOTION_8:
    case TEST_TYPE_AV1_DECODE_CDFUPDATE_8:
    case TEST_TYPE_AV1_DECODE_FILMGRAIN_8:
    case TEST_TYPE_AV1_DECODE_SVCL1T2_8:
    case TEST_TYPE_AV1_DECODE_SUPERRES_8:
    case TEST_TYPE_AV1_DECODE_SIZEUP_8:
    case TEST_TYPE_AV1_DECODE_ARGON_SEQCHANGE_AFFINE_8:
    case TEST_TYPE_AV1_DECODE_ORDERHINT_10:
    case TEST_TYPE_AV1_DECODE_FORWARDKEYFRAME_10:
    case TEST_TYPE_AV1_DECODE_LOSSLESS_10:
    case TEST_TYPE_AV1_DECODE_LOOPFILTER_10:
    case TEST_TYPE_AV1_DECODE_CDEF_10:
    case TEST_TYPE_AV1_DECODE_ARGON_FILMGRAIN_10:
    case TEST_TYPE_AV1_DECODE_ARGON_TEST_787:
    {
        context.requireDeviceFunctionality("VK_KHR_video_decode_av1");
        break;
    }
    case TEST_TYPE_AV1_DECODE_INLINE_SESSION_PARAMS:
    case TEST_TYPE_AV1_DECODE_RELAXED_SESSION_PARAMS:
    {
        context.requireDeviceFunctionality("VK_KHR_video_decode_av1");
        context.requireDeviceFunctionality("VK_KHR_video_maintenance2");
        break;
    }

    default:
        TCU_THROW(InternalError, "Unknown TestType");
    }

    const VkExtensionProperties *extensionProperties = m_testDefinition->extensionProperties(0);
    switch (m_testDefinition->getCodecOperation(0))
    {
    case VK_VIDEO_CODEC_OPERATION_DECODE_H264_BIT_KHR:
        if (strcmp(extensionProperties->extensionName, VK_STD_VULKAN_VIDEO_CODEC_H264_DECODE_EXTENSION_NAME) ||
            extensionProperties->specVersion != VK_STD_VULKAN_VIDEO_CODEC_H264_DECODE_SPEC_VERSION)
        {
            tcu::die("The requested decoder h.264 Codec STD version is NOT supported. The supported decoder h.264 "
                     "Codec STD version is version %d of %s\n",
                     VK_STD_VULKAN_VIDEO_CODEC_H264_DECODE_SPEC_VERSION,
                     VK_STD_VULKAN_VIDEO_CODEC_H264_DECODE_EXTENSION_NAME);
        }
        break;
    case VK_VIDEO_CODEC_OPERATION_DECODE_H265_BIT_KHR:
        if (strcmp(extensionProperties->extensionName, VK_STD_VULKAN_VIDEO_CODEC_H265_DECODE_EXTENSION_NAME) ||
            extensionProperties->specVersion != VK_STD_VULKAN_VIDEO_CODEC_H265_DECODE_SPEC_VERSION)
        {
            tcu::die("The requested decoder h.265 Codec STD version is NOT supported. The supported decoder h.265 "
                     "Codec STD version is version %d of %s\n",
                     VK_STD_VULKAN_VIDEO_CODEC_H265_DECODE_SPEC_VERSION,
                     VK_STD_VULKAN_VIDEO_CODEC_H265_DECODE_EXTENSION_NAME);
        }
        break;
    case VK_VIDEO_CODEC_OPERATION_DECODE_AV1_BIT_KHR:
        if (strcmp(extensionProperties->extensionName, VK_STD_VULKAN_VIDEO_CODEC_AV1_DECODE_EXTENSION_NAME) ||
            extensionProperties->specVersion != VK_STD_VULKAN_VIDEO_CODEC_AV1_DECODE_SPEC_VERSION)
        {
            tcu::die("The requested AV1 Codec STD version is NOT supported. The supported AV1 STD version is version "
                     "%d of %s\n",
                     VK_STD_VULKAN_VIDEO_CODEC_AV1_DECODE_SPEC_VERSION,
                     VK_STD_VULKAN_VIDEO_CODEC_AV1_DECODE_EXTENSION_NAME);
        }
        break;
    default:
        TCU_FAIL("Unsupported codec type");
    }
}

void InterleavingDecodeTestCase::checkSupport(Context &context) const
{
    context.requireDeviceFunctionality("VK_KHR_video_queue");
    context.requireDeviceFunctionality("VK_KHR_video_decode_queue");
    context.requireDeviceFunctionality("VK_KHR_synchronization2");

#ifdef DE_DEBUG
    DE_ASSERT(!m_testDefinitions.empty());
    TestType firstType = m_testDefinitions[0]->getTestType();
    for (const auto &test : m_testDefinitions)
        DE_ASSERT(test->getTestType() == firstType);
#endif
    switch (m_testDefinitions[0]->getTestType())
    {
    case TEST_TYPE_H264_DECODE_INTERLEAVED:
    {
        context.requireDeviceFunctionality("VK_KHR_video_decode_h264");
        break;
    }
    case TEST_TYPE_H264_H265_DECODE_INTERLEAVED:
    {
        context.requireDeviceFunctionality("VK_KHR_video_decode_h264");
        context.requireDeviceFunctionality("VK_KHR_video_decode_h265");
        break;
    }
    default:
        TCU_THROW(InternalError, "Unknown interleaving test type");
    }
}

} // namespace

tcu::TestCaseGroup *createVideoDecodeTests(tcu::TestContext &testCtx)
{
    const uint32_t baseSeed = static_cast<uint32_t>(testCtx.getCommandLine().getBaseSeed());
    MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(testCtx, "decode"));
    MovePtr<tcu::TestCaseGroup> h264Group(new tcu::TestCaseGroup(testCtx, "h264", "H.264 video codec"));
    MovePtr<tcu::TestCaseGroup> h265Group(new tcu::TestCaseGroup(testCtx, "h265", "H.265 video codec"));
    MovePtr<tcu::TestCaseGroup> av1Group(new tcu::TestCaseGroup(testCtx, "av1", "AV1 video codec"));

    for (bool layeredDpb : {true, false})
    {
        for (const auto &decodeTest : g_DecodeTests)
        {
            auto defn                  = TestDefinition::create(decodeTest, baseSeed, layeredDpb);
            const std::string testName = defn->getTestName();
            uint32_t rngSeed           = baseSeed ^ deStringHash(testName.c_str());
            defn->updateHash(rngSeed);

            auto testCodec = getTestCodec(decodeTest.type);
            if (testCodec == TEST_CODEC_H264)
                h264Group->addChild(new VideoDecodeTestCase(testCtx, testName.c_str(), defn));
            else if (testCodec == TEST_CODEC_H265)
                h265Group->addChild(new VideoDecodeTestCase(testCtx, testName.c_str(), defn));
            else
                av1Group->addChild(new VideoDecodeTestCase(testCtx, testName.c_str(), defn));
        }
        for (const auto &interleavingTest : g_InterleavingTests)
        {
            std::vector<MovePtr<TestDefinition>> defns;
            DecodeTestParam streamA{interleavingTest.type, interleavingTest.streamA};
            DecodeTestParam streamB{interleavingTest.type, interleavingTest.streamB};
            auto defnA                 = TestDefinition::create(streamA, baseSeed, layeredDpb);
            auto defnB                 = TestDefinition::create(streamB, baseSeed, layeredDpb);
            const std::string testName = defnA->getTestName();
            defns.push_back(std::move(defnA));
            defns.push_back(std::move(defnB));

            auto testCodec = getTestCodec(interleavingTest.type);
            if (testCodec == TEST_CODEC_H264)
                h264Group->addChild(new InterleavingDecodeTestCase(testCtx, testName.c_str(), std::move(defns)));
            else if (testCodec == TEST_CODEC_H265)
                h265Group->addChild(new InterleavingDecodeTestCase(testCtx, testName.c_str(), std::move(defns)));
            else
                av1Group->addChild(new InterleavingDecodeTestCase(testCtx, testName.c_str(), std::move(defns)));
        }
    } // layered true / false

    group->addChild(h264Group.release());
    group->addChild(h265Group.release());
    group->addChild(av1Group.release());

    return group.release();
}

} // namespace video
} // namespace vkt
