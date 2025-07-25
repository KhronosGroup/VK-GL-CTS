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
 * \brief AV1 Video Encoding Session tests
 */
/*--------------------------------------------------------------------*/
#include "vktTestCase.hpp"
#include "vktVideoClipInfo.hpp"
#include "vktVideoEncodeTests.hpp"
#include "vktVideoTestUtils.hpp"

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <string>
#include <algorithm>
#include <filesystem>
#include <chrono>

#ifdef DE_BUILD_VIDEO
#include <vulkan_video_encoder.h>
#endif

#include "ycbcr/vktYCbCrUtil.hpp"

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
using vkt::ycbcr::getYCbCrBitDepth;
using vkt::ycbcr::getYCbCrFormatChannelCount;
using vkt::ycbcr::isXChromaSubsampled;
using vkt::ycbcr::isYChromaSubsampled;

#define PSNR_THRESHOLD_LOWER_LIMIT 50.0

bool checkClipFileExists(const std::string &clipName);
void removeClip(const std::string &clipName);

enum BitDepth
{
    BIT_DEPTH_8  = 8,
    BIT_DEPTH_10 = 10,
    BIT_DEPTH_12 = 12
};

enum ChromaSubsampling
{
    CHROMA_SS_400 = 400,
    CHROMA_SS_420 = 420,
    CHROMA_SS_422 = 422,
    CHROMA_SS_444 = 444
};

enum GOPStruct
{
    GOP_I,
    GOP_I_P,
    GOP_I_P_B,
    GOP_IDR_P_B
};

enum Ordering
{
    ORDERED,
    UNORDERED
};

enum ResolutionChange
{
    RESOLUTION_NO_CHANGE,
    RESOLUTION_TO_LARGER,
    RESOLUTION_TO_SMALLER
};

enum QIndex
{
    QINDEX_NONE = 1,
    QINDEX_64   = 64,
    QINDEX_128  = 128,
    QINDEX_192  = 192,
    QINDEX_255  = 255,
};

enum Tiling
{
    TILING_1_TILE,
    TILING_1x2,
    TILING_4x4
};

enum Superblock
{
    SUPERBLOCK_64x64   = 64,
    SUPERBLOCK_128x128 = 128,
};

enum RateControl
{
    RC_DEFAULT  = 0,
    RC_DISABLED = 1,
    RC_CBR      = 2,
    RC_VBR      = 4
};

enum LoopFilter
{
    LF_OFF,
    LF_ON,
};

enum LoopRestore
{
    LR_OFF,
    LR_ON,
};

enum CDEF
{
    CDEF_OFF,
    CDEF_ON,
};

enum DpbMode
{
    DPB_MODE_DEFAULT,
    DPB_MODE_SEPARATE,
    DPB_MODE_LAYERED,
};

enum IntraRefreshMode
{
    IR_OFF,
    IR_PICTURE_PARTITION,
    IR_ROW_BASED,
    IR_COLUMN_BASED,
    IR_ANY_BLOCK_BASED
};

struct FrameSizeDef
{
    const char *baseClipName;
    uint32_t width;
    uint32_t height;
};

struct BitDepthDef
{
    enum BitDepth depth;
    const char *subName;
};

struct ChromaSubsamplingDef
{
    enum ChromaSubsampling subsampling;
    const char *subName;
};

struct GOPDef
{
    uint32_t frameCount;
    enum GOPStruct gop;
    bool open;
    uint32_t gopFrameCount;
    uint32_t consecutiveBFrames;
    const char *subName;
};

struct OrderingDef
{
    enum Ordering order;
    const char *subName;
};

struct ResolutionChangeDef
{
    enum ResolutionChange resolutionChange;
    const char *subName;
};

struct QuantizationDef
{
    uint32_t qIndex;
    const char *subName;
};

struct TilingDef
{
    enum Tiling tiling;
    const char *subName;
};

struct SuperblockDef
{
    enum Superblock superblock;
    const char *subName;
};

struct RateControlDef
{
    enum RateControl rc;
    const char *subName;
};

struct LoopFilterDef
{
    enum LoopFilter lf;
    const char *subName;
};

struct LoopRestoreDef
{
    enum LoopRestore lr;
    const char *subName;
};

struct CDEFDef
{
    enum CDEF cdef;
    const char *subName;
};

struct DpbModeDef
{
    enum DpbMode mode;
    const char *subName;
};

struct IntraRefreshDef
{
    enum IntraRefreshMode mode;
    const char *subName;
};

struct TestDefinition
{
    const FrameSizeDef &frameSize;
    const BitDepthDef &bitDepth;
    const ChromaSubsamplingDef &subsampling;
    const GOPDef &gop;
    const OrderingDef &ordering;
    const ResolutionChangeDef &resolutionChange;
    const QuantizationDef &quantization;
    const TilingDef &tiling;
    const SuperblockDef &superblock;
    const RateControlDef &rateControl;
    const LoopFilterDef &loopFilter;
    const LoopRestoreDef &loopRestore;
    const CDEFDef &cdef;
    const DpbModeDef &dpbMode;
    const IntraRefreshDef &intraRefresh;
};

struct TestRequirements
{
    std::vector<std::string> extensions;
    bool requireBFrames;
    bool useVariableBitrate;
    bool useConstantBitrate;
    uint32_t bitDepth;
    uint32_t subSampling;
    VkVideoCodecOperationFlagBitsKHR codecOperation;
    uint32_t width;
    uint32_t height;
    VkVideoEncodeAV1SuperblockSizeFlagsKHR superblockSizes;
    uint32_t maxTileColumns;
    uint32_t maxTileRows;
    bool useDpbArray;
    bool isXSubsampled;
    bool isYSubsampled;
    tcu::UVec4 colorDepth;
    bool useIntraRefresh;
    VkVideoEncodeIntraRefreshModeFlagsKHR intraRefreshMode;
};
class VideoTestInstance : public VideoBaseTestInstance
{
public:
    VideoTestInstance(Context &context, const std::string &inputClipFilename, const std::string &outputClipFilename,
                      const VkExtent2D expectedOutputExtent, const TestDefinition &definition)
        : VideoBaseTestInstance(context)
        , m_inputClipFilename(inputClipFilename)
        , m_outputClipFilename(outputClipFilename)
        , m_expectedOutputExtent(expectedOutputExtent)
        , m_definition(definition)
    {
    }
    virtual tcu::TestStatus iterate(void);
#ifdef DE_BUILD_VIDEO
    void setEncoder(VkSharedBaseObj<VulkanVideoEncoder> &encoder)
    {
        m_encoder = encoder;
    }
#endif

private:
#ifdef DE_BUILD_VIDEO
    VkSharedBaseObj<VulkanVideoEncoder> m_encoder;
#endif
    std::string m_inputClipFilename;
    std::string m_outputClipFilename;

    // Output resolution may be different from input resolution if
    // overriding happened (e.g, due to codedPictureAlignment not being {8,8}).
    VkExtent2D m_expectedOutputExtent;

    TestDefinition m_definition;
};

class VideoTestCase : public TestCase
{
public:
    VideoTestCase(tcu::TestContext &testCtx, const char *testName, const TestRequirements &requirements,
                  const TestDefinition &definition);
    virtual ~VideoTestCase(void);
    TestInstance *createInstance(Context &ctx) const override;
    void checkSupport(Context &ctx) const override;
    void addRequirement(const std::string &requirement);
    void validateCapabilities(Context &context) const;

protected:
    TestRequirements m_requirements;
    TestDefinition m_definition;
    static VkExtent2D codedPictureAlignment;
    const MovePtr<VkVideoEncodeAV1CapabilitiesKHR> m_av1Capabilities;
    const MovePtr<VkVideoEncodeIntraRefreshCapabilitiesKHR> m_intraRefreshCapabilities;

private:
    void buildEncoderParams(std::vector<std::string> &params) const;
    uint32_t computeIntraRefreshCycleDuration() const;
};

VkExtent2D VideoTestCase::codedPictureAlignment = VkExtent2D({0, 0});

static void buildTestName(const TestDefinition &testDef, std::string &testName);

static void buildClipName(tcu::TestContext &testCtx, const TestDefinition &testDef, std::string &clipName, bool output)
{
    auto &cmdLine   = testCtx.getCommandLine();
    auto archiveDir = cmdLine.getArchiveDir();
    clipName        = archiveDir + std::string("/vulkan/video/yuv/");

    clipName += testDef.frameSize.baseClipName;
    clipName += std::to_string(testDef.frameSize.width) + "x" + std::to_string(testDef.frameSize.height);

    clipName += "_" + std::string(testDef.subsampling.subName);
    clipName += "_" + std::string(testDef.bitDepth.subName);

    if (output)
    {
        clipName += "_" + std::string(testDef.gop.subName);
        clipName += "_" + std::to_string(testDef.gop.frameCount);
        std::string testName("");
        buildTestName(testDef, testName);
        clipName += "_" + testName;
        clipName += ".ivf";
    }
    else
        clipName += ".yuv";
}

VkVideoChromaSubsamplingFlagsKHR getChromaSubSampling(enum ChromaSubsampling subSampling)
{
    switch (subSampling)
    {
    case CHROMA_SS_400:
        return VK_VIDEO_CHROMA_SUBSAMPLING_MONOCHROME_BIT_KHR;
    case CHROMA_SS_420:
        return VK_VIDEO_CHROMA_SUBSAMPLING_420_BIT_KHR;
    case CHROMA_SS_422:
        return VK_VIDEO_CHROMA_SUBSAMPLING_422_BIT_KHR;
    case CHROMA_SS_444:
        return VK_VIDEO_CHROMA_SUBSAMPLING_444_BIT_KHR;
    };
    return VK_VIDEO_CHROMA_SUBSAMPLING_INVALID_KHR;
}

VkVideoComponentBitDepthFlagBitsKHR getBitDepth(enum BitDepth bitDepth)
{
    switch (bitDepth)
    {
    case BIT_DEPTH_8:
        return VK_VIDEO_COMPONENT_BIT_DEPTH_8_BIT_KHR;
    case BIT_DEPTH_10:
        return VK_VIDEO_COMPONENT_BIT_DEPTH_10_BIT_KHR;
    case BIT_DEPTH_12:
        return VK_VIDEO_COMPONENT_BIT_DEPTH_12_BIT_KHR;
    };
    return VK_VIDEO_COMPONENT_BIT_DEPTH_INVALID_KHR;
}

tcu::TestStatus VideoTestInstance::iterate(void)
{
    tcu::TestStatus status = tcu::TestStatus::fail("Unable to encode any frames");
#ifdef DE_BUILD_VIDEO
    int64_t frameNumEncoded = 0;

    // Encode all frames
    int64_t totalFrames = m_encoder->GetNumberOfFrames();
    for (int64_t i = 0; i < totalFrames; ++i)
    {
        VkResult result = m_encoder->EncodeNextFrame(frameNumEncoded);
        if (result != VK_SUCCESS)
        {
            status = tcu::TestStatus::fail("Failed to encode frame " + de::toString(i));
            break;
        }
        result = m_encoder->GetBitstream();
        if (result != VK_SUCCESS)
        {
            status = tcu::TestStatus::fail("Failed to get bitstream for frame " + de::toString(i));
            break;
        }
    }

    if (frameNumEncoded + 1 == totalFrames)
    {
        status = validateEncodedContent(
            VK_VIDEO_CODEC_OPERATION_ENCODE_AV1_BIT_KHR, STD_VIDEO_AV1_PROFILE_MAIN, m_outputClipFilename.c_str(),
            m_inputClipFilename.c_str(), m_definition.gop.frameCount, m_definition.frameSize.width,
            m_definition.frameSize.height, m_expectedOutputExtent,
            getChromaSubSampling(m_definition.subsampling.subsampling), getBitDepth(m_definition.bitDepth.depth),
            getBitDepth(m_definition.bitDepth.depth), PSNR_THRESHOLD_LOWER_LIMIT);
    }
    m_encoder = nullptr;
#else
    DE_UNREF(m_definition);
    DE_UNREF(m_expectedOutputExtent);
    status = tcu::TestStatus::fail("Vulkan video is not supported on this platform");
#endif
#if STREAM_DUMP_DEBUG == 0
    removeClip(m_outputClipFilename);
#endif

    return status;
}

VideoTestCase::VideoTestCase(tcu::TestContext &testCtx, const char *testName, const TestRequirements &requirements,
                             const TestDefinition &definition)
    : TestCase(testCtx, testName)
    , m_requirements(requirements)
    , m_definition(definition)
    , m_av1Capabilities(getVideoCapabilitiesExtensionAV1E())
    , m_intraRefreshCapabilities(getIntraRefreshCapabilities())
{
}

VideoTestCase::~VideoTestCase(void)
{
}

TestInstance *VideoTestCase::createInstance(Context &ctx) const
{
#ifdef DE_BUILD_VIDEO
    VkSharedBaseObj<VulkanVideoEncoder> encoder;
#endif
    VideoTestInstance *testInstance;
    std::vector<const char *> args;
    std::vector<std::string> encoderParams;
    std::stringstream ss;
    std::string deviceID;

    buildEncoderParams(encoderParams);

    std::string inputClipName("");
    buildClipName(getTestContext(), m_definition, inputClipName, false);

    std::string outputClipName("");
    buildClipName(getTestContext(), m_definition, outputClipName, true);

    args.push_back("vk-gl-cts"); //args needs the appname as a first argument
    args.push_back("-i");
    args.push_back(inputClipName.c_str());
    args.push_back("-o");
    args.push_back(outputClipName.c_str());

    args.push_back("--deviceID");
    ss << std::hex << getPhysicalDeviceProperties(ctx.getInstanceInterface(), ctx.getPhysicalDevice()).deviceID;
    deviceID = ss.str();
    args.push_back(deviceID.c_str());

    for (const auto &param : encoderParams)
    {
        args.push_back(param.c_str());
    }
#if STREAM_DUMP_DEBUG
    std::cerr << "TEST ARGS: ";
    for (auto &arg : args)
        std::cerr << arg << " ";
    std::cerr << endl;
#endif
    if (!checkClipFileExists(inputClipName))
    {
#ifdef DE_BUILD_VIDEO
        vkt::video::util::generateYCbCrFile(inputClipName, m_definition.gop.frameCount, m_definition.frameSize.width,
                                            m_definition.frameSize.height, m_definition.subsampling.subsampling,
                                            m_definition.bitDepth.depth);
#endif
    }

    VkExtent2D expectedOutputExtent = {m_definition.frameSize.width, m_definition.frameSize.height};
    if (codedPictureAlignment.width != 8 || codedPictureAlignment.height != 8)
    {
        const auto w = de::roundUp(m_requirements.width, 8U);
        const auto h = de::roundUp(m_requirements.height, 8U);

        expectedOutputExtent.width  = de::roundUp(w, codedPictureAlignment.width);
        expectedOutputExtent.height = de::roundUp(h, codedPictureAlignment.height);
    }
#ifdef DE_BUILD_VIDEO
    VkResult result = CreateVulkanVideoEncoder(m_requirements.codecOperation, static_cast<int>(args.size()),
                                               const_cast<char **>(args.data()), encoder);
    if (result != VK_SUCCESS)
    {
        throw tcu::TestError("Failed to create VulkanVideoEncoder");
    }
#endif
    testInstance = new VideoTestInstance(ctx, inputClipName, outputClipName, expectedOutputExtent, m_definition);
#ifdef DE_BUILD_VIDEO
    testInstance->setEncoder(encoder);
#endif
    return testInstance;
}

void VideoTestCase::checkSupport(Context &ctx) const
{
    for (const auto &extension : m_requirements.extensions)
    {
        if (!ctx.isDeviceFunctionalitySupported(extension.c_str()))
        {
            throw tcu::NotSupportedError("Required extension " + extension + " not supported");
        }
    }

    try
    {
        validateCapabilities(ctx);
    }
    catch (const tcu::NotSupportedError &e)
    {
        throw tcu::NotSupportedError(std::string("Capability check failed: ") + e.what());
    }
}

void VideoTestCase::validateCapabilities(Context &context) const
{
    const VkVideoCodecOperationFlagBitsKHR videoCodecEncodeOperation = m_requirements.codecOperation;
    const VkImageUsageFlags usageFlag                                = VK_VIDEO_ENCODE_USAGE_DEFAULT_KHR;
    const VkImageUsageFlags imageFlag                                = VK_IMAGE_USAGE_VIDEO_ENCODE_SRC_BIT_KHR;

    const VkVideoEncodeAV1ProfileInfoKHR encodeProfileInfo = {
        VK_STRUCTURE_TYPE_VIDEO_ENCODE_AV1_PROFILE_INFO_KHR, // sType
        nullptr,                                             // pNext
        STD_VIDEO_AV1_PROFILE_MAIN                           // stdProfile
    };

    de::MovePtr<VkVideoEncodeAV1ProfileInfoKHR> encodeProfile =
        de::MovePtr<VkVideoEncodeAV1ProfileInfoKHR>(new VkVideoEncodeAV1ProfileInfoKHR(encodeProfileInfo));

    const MovePtr<VkVideoEncodeUsageInfoKHR> encodeUsageInfo = getEncodeUsageInfo(
        encodeProfile.get(), usageFlag, VK_VIDEO_ENCODE_CONTENT_DEFAULT_KHR, VK_VIDEO_ENCODE_TUNING_MODE_DEFAULT_KHR);

    const MovePtr<VkVideoProfileInfoKHR> videoEncodeProfile =
        getVideoProfile(videoCodecEncodeOperation, encodeUsageInfo.get(), m_requirements.subSampling,
                        m_requirements.bitDepth, m_requirements.bitDepth);

    const MovePtr<VkVideoProfileListInfoKHR> videoEncodeProfileList = getVideoProfileList(videoEncodeProfile.get(), 1);

    const InstanceInterface &vki          = context.getInstanceInterface();
    const VkPhysicalDevice physicalDevice = context.getPhysicalDevice();

    const MovePtr<VkVideoEncodeCapabilitiesKHR> encodeCapabilities =
        getVideoEncodeCapabilities(m_av1Capabilities.get());

    void *headStruct = encodeCapabilities.get();
    if (m_requirements.useIntraRefresh)
        appendStructurePtrToVulkanChain((const void **)&headStruct, m_intraRefreshCapabilities.get());

    const MovePtr<VkVideoCapabilitiesKHR> videoCapabilities =
        getVideoCapabilities(vki, physicalDevice, videoEncodeProfile.get(), headStruct);

    if (m_requirements.requireBFrames)
    {
        if (m_av1Capabilities->maxBidirectionalCompoundReferenceCount == 0)
            throw tcu::NotSupportedError("B frames encoding not supported for AV1");
    }

    if (m_requirements.useVariableBitrate &&
        !(encodeCapabilities->rateControlModes & VK_VIDEO_ENCODE_RATE_CONTROL_MODE_VBR_BIT_KHR))
    {
        throw tcu::NotSupportedError("Variable bitrate not supported");
    }

    if (m_requirements.useConstantBitrate &&
        !(encodeCapabilities->rateControlModes & VK_VIDEO_ENCODE_RATE_CONTROL_MODE_CBR_BIT_KHR))
    {
        throw tcu::NotSupportedError("Constant bitrate not supported");
    }

    if (!(m_av1Capabilities->superblockSizes & m_requirements.superblockSizes))
    {
        throw tcu::NotSupportedError("Required superblock size not supported");
    }

    if (m_requirements.width > videoCapabilities->maxCodedExtent.width ||
        m_requirements.height > videoCapabilities->maxCodedExtent.height)
    {
        throw tcu::NotSupportedError("Required dimensions exceed maxCodedExtent");
    }

    if (m_requirements.width < videoCapabilities->minCodedExtent.width ||
        m_requirements.height < videoCapabilities->minCodedExtent.height)
    {
        throw tcu::NotSupportedError("Required dimensions are smaller than minCodedExtent");
    }

    if (!m_requirements.useDpbArray &&
        (videoCapabilities->flags & VK_VIDEO_CAPABILITY_SEPARATE_REFERENCE_IMAGES_BIT_KHR) == 0)
    {
        throw tcu::NotSupportedError("Separate DPB images not supported");
    }

    if (m_requirements.maxTileColumns > 0 || m_requirements.maxTileRows > 0)
    {
        uint32_t minTileWidth =
            (m_requirements.width + m_requirements.maxTileColumns - 1) / m_requirements.maxTileColumns;
        uint32_t minTileHeight = (m_requirements.height + m_requirements.maxTileRows - 1) / m_requirements.maxTileRows;

        if (minTileWidth < m_av1Capabilities->minTileSize.width ||
            minTileHeight < m_av1Capabilities->minTileSize.height)
        {
            throw tcu::NotSupportedError("Required tile dimensions are smaller than minTileSize");
        }

        if (m_requirements.width > m_av1Capabilities->maxTiles.width * m_av1Capabilities->maxTileSize.width ||
            m_requirements.height > m_av1Capabilities->maxTiles.height * m_av1Capabilities->maxTileSize.height)
        {
            throw tcu::NotSupportedError("Required dimensions exceed maximum possible tiled area");
        }

        if (m_requirements.maxTileColumns > m_av1Capabilities->maxTiles.width ||
            m_requirements.maxTileRows > m_av1Capabilities->maxTiles.height)
        {
            throw tcu::NotSupportedError("Required tile columns/rows exceed supported maximum");
        }
    }

    MovePtr<vector<VkFormat>> supportedFormats =
        getSupportedFormats(vki, physicalDevice, imageFlag, videoEncodeProfileList.get());

    if (!supportedFormats || supportedFormats->empty())
        TCU_THROW(NotSupportedError, "No supported picture formats");

    bool formatFound = false;
    for (const auto &supportedFormat : *supportedFormats)
    {
        if (isXChromaSubsampled(supportedFormat) != m_requirements.isXSubsampled ||
            isYChromaSubsampled(supportedFormat) != m_requirements.isYSubsampled)
        {
            continue;
        }

        tcu::UVec4 formatColorDepth = getYCbCrBitDepth(supportedFormat);
        if (formatColorDepth != m_requirements.colorDepth)
        {
            continue;
        }

        // TODO nessery ?
        // uint32_t channelCount = getYCbCrFormatChannelCount(supportedFormat);
        // if (channelCount < 3) // Assuming we need at least 3 channels (Y, Cb, Cr)
        // {
        //     continue;
        // }

        formatFound = true;
        break;
    }

    if (!formatFound)
        TCU_THROW(NotSupportedError,
                  "No supported format found matching the required chroma subsampling and color depth");

    codedPictureAlignment = m_av1Capabilities->codedPictureAlignment;

    if (m_requirements.useIntraRefresh)
    {
        if (!context.isDeviceFunctionalitySupported("VK_KHR_video_encode_intra_refresh"))
            throw tcu::NotSupportedError("VK_KHR_video_encode_intra_refresh not supported");

        if ((m_intraRefreshCapabilities->intraRefreshModes & m_requirements.intraRefreshMode) == 0)
            throw tcu::NotSupportedError("Required intra-refresh mode not supported");
    }
}

VideoTestCase *createVideoTestCase(tcu::TestContext &testCtx, const char *testname,
                                   const TestRequirements &requirements, const TestDefinition &definition)
{
    VideoTestCase *testCase = new VideoTestCase(testCtx, testname, requirements, definition);
    return testCase;
}

uint32_t VideoTestCase::computeIntraRefreshCycleDuration() const
{
    VkExtent2D minCodingBlockSize;
    if (m_av1Capabilities->superblockSizes & VK_VIDEO_ENCODE_AV1_SUPERBLOCK_SIZE_64_BIT_KHR)
        minCodingBlockSize = {64, 64};
    else if (m_av1Capabilities->superblockSizes & VK_VIDEO_ENCODE_AV1_SUPERBLOCK_SIZE_128_BIT_KHR)
        minCodingBlockSize = {128, 128};
    else
        throw std::runtime_error("No supported superblock size for AV1");

    VkExtent2D codedExtentInMinCodingBlocks;
    codedExtentInMinCodingBlocks.width =
        (m_definition.frameSize.width + minCodingBlockSize.width - 1) / minCodingBlockSize.width;
    codedExtentInMinCodingBlocks.height =
        (m_definition.frameSize.height + minCodingBlockSize.height - 1) / minCodingBlockSize.height;

    uint32_t maxPicturePartitions = 0;
    switch (m_definition.intraRefresh.mode)
    {
    case IR_PICTURE_PARTITION:
    {
        uint32_t maxCodecPartitions    = m_av1Capabilities->maxTiles.width * m_av1Capabilities->maxTiles.height;
        uint32_t maxPartitionsInBlocks = codedExtentInMinCodingBlocks.width * codedExtentInMinCodingBlocks.height;
        maxPicturePartitions           = std::min(maxCodecPartitions, maxPartitionsInBlocks);
        break;
    }
    case IR_ROW_BASED:
        maxPicturePartitions = codedExtentInMinCodingBlocks.height;
        break;
    case IR_COLUMN_BASED:
        maxPicturePartitions = codedExtentInMinCodingBlocks.width;
        break;
    case IR_ANY_BLOCK_BASED:
        maxPicturePartitions = codedExtentInMinCodingBlocks.width * codedExtentInMinCodingBlocks.height;
        break;
    case IR_OFF:
        return 0;
    default:
        throw std::runtime_error("Invalid intra-refresh mode");
    }

    return std::min(m_intraRefreshCapabilities->maxIntraRefreshCycleDuration, maxPicturePartitions);
}

void VideoTestCase::buildEncoderParams(std::vector<std::string> &params) const
{
    params.push_back("--codec");
    params.push_back("av1");

    params.push_back("--numFrames");
    params.push_back(de::toString(m_definition.gop.frameCount));

    params.push_back("--inputWidth");
    params.push_back(de::toString(m_definition.frameSize.width));
    params.push_back("--inputHeight");
    params.push_back(de::toString(m_definition.frameSize.height));

    params.push_back("--idrPeriod");
    switch (m_definition.gop.gop)
    {
    case GOP_IDR_P_B:
        params.push_back("30");
        break;
    default:
        params.push_back("0");
        break;
    }

    switch (m_definition.tiling.tiling)
    {
    case TILING_1x2:
    {
        uint32_t tileWidthInSbs, tileHeightInSbs;
        if (m_definition.superblock.superblock)
        {
            tileWidthInSbs = static_cast<uint32_t>(
                std::ceil((double)m_definition.frameSize.width / (double)m_definition.superblock.superblock));
            tileHeightInSbs = static_cast<uint32_t>(
                std::ceil((double)m_definition.frameSize.height / (double)m_definition.superblock.superblock));
        }
        else
        {
            TCU_THROW(NotSupportedError, "superblock should not be null");
        }
        params.push_back("--tiles");
        params.push_back("--params");
        params.push_back("0");
        params.push_back("1");
        params.push_back(std::to_string(tileWidthInSbs - 1));
        params.push_back("2");
        params.push_back(std::to_string((tileHeightInSbs / 2) - 1));
        params.push_back(std::to_string((tileHeightInSbs - tileHeightInSbs / 2) - 1));
        params.push_back("0");
        break;
    }
    case TILING_4x4:
        params.push_back("--tiles");
        params.push_back("--params");
        params.push_back("1");
        params.push_back("4");
        params.push_back("4");
        params.push_back("0");
        break;
    default:
        break;
    }

    params.push_back("--inputChromaSubsampling");
    params.push_back(std::to_string(m_definition.subsampling.subsampling).c_str());

    params.push_back("--inputBpp");
    params.push_back(std::to_string(m_definition.bitDepth.depth));

    params.push_back("--consecutiveBFrameCount");
    params.push_back(de::toString(m_definition.gop.consecutiveBFrames));

    params.push_back("--gopFrameCount");
    params.push_back(de::toString(m_definition.gop.frameCount));

    params.push_back("--qpI");
    params.push_back(de::toString(m_definition.quantization.qIndex));

    params.push_back("--qpP");
    params.push_back(de::toString(m_definition.quantization.qIndex));

    params.push_back("--qpB");
    params.push_back(de::toString(m_definition.quantization.qIndex));

    params.push_back("--rateControlMode");
    params.push_back(de::toString(m_definition.rateControl.rc));

    if (m_definition.loopFilter.lf == LF_ON)
        params.push_back("--lf");

    if (m_definition.loopRestore.lr == LR_ON)
        params.push_back("--lr");

    if (m_definition.loopRestore.lr == LR_ON)
        params.push_back("--cdef");

    switch (m_definition.dpbMode.mode)
    {
    case DPB_MODE_SEPARATE:
        params.push_back("--dpbMode");
        params.push_back("separate");
        break;
    case DPB_MODE_LAYERED:
        params.push_back("--dpbMode");
        params.push_back("layered");
        break;
    default:
        break;
    }

    if (m_definition.ordering.order == UNORDERED)
        params.push_back("--testOutOfOrderRecording");

    if (m_definition.intraRefresh.mode != IR_OFF)
    {
        uint32_t intraRefreshCycleDuration = 0;
        intraRefreshCycleDuration          = computeIntraRefreshCycleDuration();
        DE_ASSERT(intraRefreshCycleDuration > 0);

        switch (m_definition.intraRefresh.mode)
        {
        case IR_PICTURE_PARTITION:
            params.push_back("--intraRefreshMode");
            params.push_back("picpartition");
            break;
        case IR_ROW_BASED:
            params.push_back("--intraRefreshMode");
            params.push_back("blockrows");
            break;
        case IR_COLUMN_BASED:
            params.push_back("--intraRefreshMode");
            params.push_back("blockcolumns");
            break;
        case IR_ANY_BLOCK_BASED:
            params.push_back("--intraRefreshMode");
            params.push_back("blocks");
            break;
        default:
            break;
        }

        params.push_back("--intraRefreshCycleDuration");
        params.push_back(std::to_string(intraRefreshCycleDuration));
    }
}

bool validateTestDefinition(const TestDefinition &testDef)
{
    // Here we check for invalid or unsupported combinations of test
    // parameters.

    // Not supported by vendors
    if (testDef.subsampling.subsampling != CHROMA_SS_420)
        return false;

    // Not supported by vendors
    if (testDef.bitDepth.depth == BIT_DEPTH_12)
        return false;

    // Superblocks: only 64x64 supported for now
    if (testDef.superblock.superblock != SUPERBLOCK_64x64)
        return false;

    // Resolution change: only 64x64 supported for now
    if (testDef.resolutionChange.resolutionChange != RESOLUTION_NO_CHANGE)
        return false;

    // ordering out of order only supported with I_P_B and 3 B-Frames
    if (testDef.ordering.order == UNORDERED && testDef.gop.gop != GOP_IDR_P_B && testDef.gop.consecutiveBFrames != 3)
        return false;

    // The Qindex test should be performed only when rate control is disabled
    if (testDef.quantization.qIndex != QINDEX_NONE && testDef.rateControl.rc != RC_DISABLED)
        return false;

    // The nested combination of tests should be performed only with 720x780
    if (testDef.frameSize.width != 720 && testDef.frameSize.height != 480 &&
        (testDef.ordering.order != ORDERED || testDef.resolutionChange.resolutionChange != RESOLUTION_NO_CHANGE ||
         testDef.quantization.qIndex != QINDEX_NONE || testDef.superblock.superblock != SUPERBLOCK_64x64 ||
         testDef.rateControl.rc != RC_DEFAULT || testDef.loopFilter.lf != LF_OFF || testDef.loopRestore.lr != LR_OFF ||
         testDef.cdef.cdef != CDEF_OFF || testDef.dpbMode.mode != DPB_MODE_SEPARATE))
        return false;

    // Test only GOP_I_P_B in the case of resolution different from 720x780
    if (testDef.frameSize.width != 720 && testDef.frameSize.height != 480 && (testDef.gop.gop != GOP_I_P_B))
        return false;

    // Remove TILING_1x2 from 7680x4320 resolution as it is not supported by the AV1 specification
    // See MAX_TILE_WIDTH in https://aomediacodec.github.io/av1-spec/av1-spec.pdf
    if (testDef.frameSize.width == 7680 && testDef.frameSize.height == 4320 && (testDef.tiling.tiling == TILING_1x2))
        return false;

    // Intra-refresh is only supported with P frames, not with B frames
    if (testDef.intraRefresh.mode != IR_OFF && testDef.gop.gop != GOP_I_P)
        return false;

    return true;
}

bool checkClipFileExists(const std::string &clipName)
{
    std::ifstream f(clipName.c_str());
    return f.good();
}

void removeClip(const std::string &clipName)
{
    try
    {
        std::filesystem::remove(clipName);
    }
    catch (const std::filesystem::filesystem_error &e)
    {
        std::cerr << "Error deleting file: " << e.what() << std::endl;
    }
}

inline void addSubName(std::ostringstream &s, const char *subName)
{
    if (strlen(subName) != 0)
    {
        if (s.str() != "")
            s << "_";
        s << subName;
    }
}

void buildTestName(const TestDefinition &testDef, std::string &testName)
{
    std::ostringstream s;

    addSubName(s, testDef.ordering.subName);
    addSubName(s, testDef.resolutionChange.subName);
    addSubName(s, testDef.quantization.subName);
    addSubName(s, testDef.tiling.subName);
    addSubName(s, testDef.superblock.subName);
    addSubName(s, testDef.rateControl.subName);
    addSubName(s, testDef.loopFilter.subName);
    addSubName(s, testDef.loopRestore.subName);
    addSubName(s, testDef.cdef.subName);
    addSubName(s, testDef.dpbMode.subName);
    addSubName(s, testDef.intraRefresh.subName);

    testName = s.str();
    if (testName == "")
        testName = "default";
}

void buildTestRequirements(const TestDefinition &testDef, TestRequirements &requirements)
{
    requirements.extensions.push_back("VK_KHR_video_queue");
    requirements.extensions.push_back("VK_KHR_video_encode_queue");
    requirements.extensions.push_back("VK_KHR_video_encode_av1");

    requirements.codecOperation = VK_VIDEO_CODEC_OPERATION_ENCODE_AV1_BIT_KHR;

    requirements.width  = testDef.frameSize.width;
    requirements.height = testDef.frameSize.height;

    requirements.bitDepth    = getBitDepth(testDef.bitDepth.depth);
    requirements.subSampling = getChromaSubSampling(testDef.subsampling.subsampling);

    requirements.requireBFrames = (testDef.gop.gop == GOP_I_P_B || testDef.gop.gop == GOP_IDR_P_B);

    requirements.useVariableBitrate = (testDef.quantization.qIndex != 0);
    requirements.useConstantBitrate = false;

    requirements.superblockSizes = (testDef.superblock.superblock == SUPERBLOCK_64x64) ?
                                       VK_VIDEO_ENCODE_AV1_SUPERBLOCK_SIZE_64_BIT_KHR :
                                       VK_VIDEO_ENCODE_AV1_SUPERBLOCK_SIZE_128_BIT_KHR;

    requirements.useDpbArray = (testDef.dpbMode.mode == DPB_MODE_LAYERED);

    switch (testDef.tiling.tiling)
    {
    case TILING_1_TILE:
        requirements.maxTileColumns = 1;
        requirements.maxTileRows    = 1;
        break;
    case TILING_1x2:
        requirements.maxTileColumns = 1;
        requirements.maxTileRows    = 2;
        break;
    case TILING_4x4:
        requirements.maxTileColumns = 4;
        requirements.maxTileRows    = 4;
        break;
    }

    switch (testDef.subsampling.subsampling)
    {
    case CHROMA_SS_400:
        requirements.isXSubsampled = false;
        requirements.isYSubsampled = false;
        break;
    case CHROMA_SS_420:
        requirements.isXSubsampled = true;
        requirements.isYSubsampled = true;
        break;
    case CHROMA_SS_422:
        requirements.isXSubsampled = true;
        requirements.isYSubsampled = false;
        break;
    case CHROMA_SS_444:
        requirements.isXSubsampled = false;
        requirements.isYSubsampled = false;
        break;
    }

    switch (testDef.bitDepth.depth)
    {
    case BIT_DEPTH_8:
        requirements.colorDepth = tcu::UVec4(8, 8, 8, 0);
        break;
    case BIT_DEPTH_10:
        requirements.colorDepth = tcu::UVec4(10, 10, 10, 0);
        break;
    case BIT_DEPTH_12:
        requirements.colorDepth = tcu::UVec4(12, 12, 12, 0);
        break;
    }

    if (testDef.intraRefresh.mode != IR_OFF)
    {
        requirements.extensions.push_back("VK_KHR_video_encode_intra_refresh");
        requirements.useIntraRefresh = true;

        switch (testDef.intraRefresh.mode)
        {
        case IR_PICTURE_PARTITION:
            requirements.intraRefreshMode = VK_VIDEO_ENCODE_INTRA_REFRESH_MODE_PER_PICTURE_PARTITION_BIT_KHR;
            break;
        case IR_ROW_BASED:
            requirements.intraRefreshMode = VK_VIDEO_ENCODE_INTRA_REFRESH_MODE_BLOCK_ROW_BASED_BIT_KHR;
            break;
        case IR_COLUMN_BASED:
            requirements.intraRefreshMode = VK_VIDEO_ENCODE_INTRA_REFRESH_MODE_BLOCK_COLUMN_BASED_BIT_KHR;
            break;
        case IR_ANY_BLOCK_BASED:
            requirements.intraRefreshMode = VK_VIDEO_ENCODE_INTRA_REFRESH_MODE_BLOCK_BASED_BIT_KHR;
            break;
        default:
            requirements.intraRefreshMode = 0;
            break;
        }
    }
    else
    {
        requirements.useIntraRefresh  = false;
        requirements.intraRefreshMode = 0;
    }
}

} // namespace

VideoTestCase *createVideoEncodeTestAV1(tcu::TestContext &testCtx, const TestDefinition &testDef)
{
    // Discard invalid or unsupported combinations
    if (!validateTestDefinition(testDef))
        return nullptr;

    std::string testName("");
    buildTestName(testDef, testName);

    TestRequirements requirements;
    buildTestRequirements(testDef, requirements);

    auto testCase = createVideoTestCase(testCtx, testName.c_str(), requirements, testDef);

    return testCase;
}

// Test definitions

static const std::vector<FrameSizeDef> frameSizeTests = {
    {"", 128, 128},   {"", 176, 144},   {"", 352, 288},   {"", 720, 480},
    {"", 1920, 1080}, {"", 3840, 2160}, {"", 7680, 4320},
};

static const std::vector<BitDepthDef> bitDepthTests = {
    {BIT_DEPTH_8, "8le"},
    {BIT_DEPTH_10, "10le"},
    {BIT_DEPTH_12, "12le"},
};

static const std::vector<ChromaSubsamplingDef> subsamplingTests = {
    {CHROMA_SS_400, "400"},
    {CHROMA_SS_420, "420"},
    {CHROMA_SS_422, "422"},
    {CHROMA_SS_444, "444"},
};

static const std::vector<GOPDef> gopTests = {
    {15, GOP_I, false, 1, 0, "i"},
    {15, GOP_I_P, false, 2, 0, "i_p"},
    {15, GOP_I_P, true, 2, 0, "i_p_open"},
    {15, GOP_I_P_B, false, 13, 3, "i_p_b3_13"},
    {15, GOP_IDR_P_B, false, 13, 3, "idr_p_b3_13"},
};

static const std::vector<OrderingDef> orderingTests = {
    {ORDERED, ""},
    {UNORDERED, "unordered"},
};

static const std::vector<ResolutionChangeDef> resolutionChangeTests = {
    {RESOLUTION_NO_CHANGE, ""},
    {RESOLUTION_TO_LARGER, "res_to_larger"},
    {RESOLUTION_TO_SMALLER, "res_to_smaller"},
};

static const std::vector<QuantizationDef> quantizationTests = {
    {QINDEX_NONE, ""},         {QINDEX_64, "qindex64"},   {QINDEX_128, "qindex128"},
    {QINDEX_192, "qindex192"}, {QINDEX_255, "qindex255"},
};

static const std::vector<TilingDef> tilingTests = {
    {TILING_1_TILE, ""},
    {TILING_1x2, "tiling_1x2"},
    {TILING_4x4, "tiling_4x4"},
};

static const std::vector<SuperblockDef> superblockTests = {
    {SUPERBLOCK_64x64, ""},
    {SUPERBLOCK_128x128, "superblocks_128x128"},
};

static const std::vector<RateControlDef> rateControlTests = {
    {RC_DEFAULT, ""},
    {RC_DISABLED, "rc_disabled"},
    {RC_CBR, "rc_cbr"},
    {RC_VBR, "rc_vbr"},
};

static const std::vector<LoopFilterDef> lfTests = {
    {LF_OFF, ""},
    {LF_ON, "lf"},
};

static const std::vector<LoopRestoreDef> lrTests = {
    {LR_OFF, ""},
    {LR_ON, "lr"},
};

static const std::vector<CDEFDef> cdefTests = {
    {CDEF_OFF, ""},
    {CDEF_ON, "cdef"},
};

static const std::vector<DpbModeDef> dpbModeTests = {
    {DPB_MODE_SEPARATE, ""},
    {DPB_MODE_LAYERED, "layered_dpb"},
};

static const std::vector<IntraRefreshDef> intraRefreshTests = {
    {IR_OFF, ""},
    {IR_PICTURE_PARTITION, "intra_refresh_picture_partition"},
    {IR_ROW_BASED, "intra_refresh_row_based"},
    {IR_COLUMN_BASED, "intra_refresh_column_based"},
    {IR_ANY_BLOCK_BASED, "intra_refresh_any_block_based"},
};

tcu::TestCaseGroup *createVideoEncodeTestsAV1(tcu::TestContext &testCtx)
{
    MovePtr<tcu::TestCaseGroup> av1group(new tcu::TestCaseGroup(testCtx, "av1", "AV1 video codec"));
    std::ostringstream s;
    std::string groupName;

    // Combine all tests types into a TestDefinition struct
    for (const auto &frameSizeTest : frameSizeTests)
        for (const auto &bitDepthTest : bitDepthTests)
            for (const auto &subsamplingTest : subsamplingTests)
            {
                s.str("");
                s << frameSizeTest.width << "x" << frameSizeTest.height;
                s << "_" << bitDepthTest.subName;
                s << "_" << subsamplingTest.subName;
                groupName = s.str();
                MovePtr<tcu::TestCaseGroup> resGroup(new tcu::TestCaseGroup(testCtx, groupName.c_str()));

                for (const auto &gopTest : gopTests)
                {
                    s.str("");
                    s << gopTest.subName << "_" << gopTest.frameCount;
                    groupName = s.str();
                    MovePtr<tcu::TestCaseGroup> gopGroup(new tcu::TestCaseGroup(testCtx, groupName.c_str()));
                    for (const auto &tilingTest : tilingTests)
                    {
                        for (const auto &orderingTest : orderingTests)
                            for (const auto &resolutionChangeTest : resolutionChangeTests)
                                for (const auto &quantizationTest : quantizationTests)
                                    for (const auto &superblockTest : superblockTests)
                                        for (const auto &rateControlTest : rateControlTests)
                                            for (const auto &lfTest : lfTests)
                                                for (const auto &lrTest : lrTests)
                                                    for (const auto &cdefTest : cdefTests)
                                                        for (const auto &dpbModeTest : dpbModeTests)
                                                            for (const auto &intraRefreshTest : intraRefreshTests)
                                                            {
                                                                TestDefinition testDef = {
                                                                    frameSizeTest,    bitDepthTest,
                                                                    subsamplingTest,  gopTest,
                                                                    orderingTest,     resolutionChangeTest,
                                                                    quantizationTest, tilingTest,
                                                                    superblockTest,   rateControlTest,
                                                                    lfTest,           lrTest,
                                                                    cdefTest,         dpbModeTest,
                                                                    intraRefreshTest};
                                                                auto testCase =
                                                                    createVideoEncodeTestAV1(testCtx, testDef);
                                                                if (testCase != nullptr)
                                                                    gopGroup->addChild(testCase);
                                                            }
                    }
                    resGroup->addChild(gopGroup.release());
                }
                av1group->addChild(resGroup.release());
            }
    return av1group.release();
}
} // namespace video
} // namespace vkt
