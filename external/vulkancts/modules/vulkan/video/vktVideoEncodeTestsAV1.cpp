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
#include "tcuResultCollector.hpp"
#include "vktTestCase.hpp"
#include "vktVideoClipInfo.hpp"
#include "vktVideoEncodeTests.hpp"
#include "vktVideoTestUtils.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <fstream>
#include <string>
#include <algorithm>
#include <filesystem>
#include <numeric>
#include <chrono>
#include <limits>
#include <map>
#include <sstream>

#ifdef DE_BUILD_VIDEO
#include "vktVideoBaseDecodeUtils.hpp"
#include <vulkan_video_encoder.h>
#endif

#include "ycbcr/vktYCbCrUtil.hpp"

namespace vkt
{
namespace video
{

#ifdef DE_BUILD_VIDEO
static uint32_t getMaxFrameCount();
#endif

namespace
{
using namespace vk;

using de::MovePtr;
using vkt::ycbcr::getYCbCrBitDepth;
using vkt::ycbcr::isXChromaSubsampled;
using vkt::ycbcr::isYChromaSubsampled;

#ifdef DE_BUILD_VIDEO
static constexpr double PSNR_THRESHOLD_LOWER_LIMIT = 50.0;
#endif

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
    TILING_4x4,
    TILING_MAX_SUPPORTED
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
    bool isEmptyRegion;
    bool isMidway;
    const char *subName;
};

enum Feedback2TestType
{
    FEEDBACK2_DISABLED,
    FEEDBACK2_PARTITION_COUNT_1,
    FEEDBACK2_PARTITION_COUNT_MAX,
    FEEDBACK2_PARTITION_COUNT_MAX_TRUNCATED,
    FEEDBACK2_INTRA_INTER_PIXELS,
    FEEDBACK2_INTRA_INTER_SKIP_PIXELS,
    FEEDBACK2_QP_AVERAGE_MIN_MAX,
    FEEDBACK2_QP_WITH_QUANTIZATION_EQUIV,
};

struct Feedback2Def
{
    Feedback2TestType type;
    const char *subName;
    const char *inputClipNamePrefix;
    bool enablePictureFeedback;
    bool enablePixelFeedback;
    bool enableSkippedPixelFeedback;
    bool enablePerPartitionFeedback;
    uint32_t maxPerPartitionFeedbackEntries; // 0 means use device max
    bool overrideQp;
    uint32_t qpI;
    uint32_t qpP;
    uint32_t qpB;
    bool validateMinQp;
    bool validateMaxQp;
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
    Feedback2Def feedback2;
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

struct Feedback2PartitionFeedback
{
    uint32_t index;
    bool hasStatus;
    VkQueryResultStatusKHR status;
    uint32_t offset;
    uint32_t size;
};

struct Feedback2FrameFeedback
{
    uint64_t frameIndex;
    bool hasStatus;
    VkQueryResultStatusKHR status;
    bool hasAvgQp;
    bool hasMinQp;
    bool hasMaxQp;
    int32_t avgQp;
    int32_t minQp;
    int32_t maxQp;
    bool hasIntraPixels;
    bool hasInterPixels;
    bool hasSkippedPixels;
    uint32_t intraPixels;
    uint32_t interPixels;
    uint32_t skippedPixels;
    bool hasPicturePartitionCount;
    uint32_t picturePartitionCount;
    bool hasBitstreamBufferOffset;
    bool hasBitstreamBytesWritten;
    uint32_t bitstreamBufferOffset;
    uint32_t bitstreamBytesWritten;
    std::vector<Feedback2PartitionFeedback> partitions;
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
        , m_hasFeedback2Config(definition.feedback2.type != FEEDBACK2_DISABLED)
        , m_feedback2Config{}
    {
        if (m_hasFeedback2Config)
            m_feedback2Config = definition.feedback2;
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
    bool m_hasFeedback2Config;
    Feedback2Def m_feedback2Config;

    bool validateFeedback2Output(tcu::ResultCollector &results);
    bool parseFeedback2Output(const std::string &path, std::map<uint64_t, Feedback2FrameFeedback> &frames);
    bool getParsedAv1Partitions(std::vector<Feedback2PartitionFeedback> &partitions, uint32_t &bitstreamDataLen);
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
    void validateCapabilities(const Context &context) const;

protected:
    TestRequirements m_requirements;
    TestDefinition m_definition;
    static VkExtent2D codedPictureAlignment;
    const MovePtr<VkVideoEncodeAV1CapabilitiesKHR> m_av1Capabilities;
    const MovePtr<VkVideoEncodeIntraRefreshCapabilitiesKHR> m_intraRefreshCapabilities;
    bool m_hasFeedback2Config;
    Feedback2Def m_feedback2Config;

private:
    void buildEncoderParams(std::vector<std::string> &params, const Feedback2Def *feedbackConfig) const;
    uint32_t computeIntraRefreshCycleDuration() const;
    void updateFeedback2Config(const Context &context, Feedback2Def &config) const;
    void validateFeedback2Capabilities(const VkVideoEncodeCapabilitiesKHR &encodeCapabilities,
                                       const VkVideoEncodeFeedback2CapabilitiesKHR &feedback2Capabilities,
                                       const VkPhysicalDeviceVideoEncodeFeedback2FeaturesKHR &feedback2Features,
                                       Feedback2Def &config) const;
    bool computeMaxTiling(uint32_t &tileColumns, uint32_t &tileRows) const;
};

VkExtent2D VideoTestCase::codedPictureAlignment = VkExtent2D({0, 0});

static void buildTestName(const TestDefinition &testDef, std::string &testName);

static void buildInputClipName(const tcu::TestContext &testCtx, const TestDefinition &testDef, std::string &clipName)
{
    auto &cmdLine   = testCtx.getCommandLine();
    auto archiveDir = cmdLine.getArchiveDir();
    clipName        = archiveDir + std::string("/vulkan/video/yuv/");

    if (testDef.feedback2.inputClipNamePrefix != nullptr)
        clipName += testDef.feedback2.inputClipNamePrefix;

    clipName += testDef.frameSize.baseClipName;
    clipName += std::to_string(testDef.frameSize.width) + "x" + std::to_string(testDef.frameSize.height);

    clipName += "_" + std::string(testDef.subsampling.subName);
    clipName += "_" + std::string(testDef.bitDepth.subName);
    clipName += ".yuv";
}

// Mirrors the "<width>x<height>_<bitDepth>_<subsampling>_<gop>_<frameCount>" case-group path built in
// createVideoEncodeTestsAV1, so dump filenames match the dEQP case they belong to.
static std::string buildClipDescriptor(const TestDefinition &testDef)
{
    return std::to_string(testDef.frameSize.width) + "x" + std::to_string(testDef.frameSize.height) + "_" +
           testDef.bitDepth.subName + "_" + testDef.subsampling.subName + "_" + testDef.gop.subName + "_" +
           std::to_string(testDef.gop.frameCount);
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
    }
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
    }
    return VK_VIDEO_COMPONENT_BIT_DEPTH_INVALID_KHR;
}

tcu::TestStatus VideoTestInstance::iterate(void)
{
    tcu::TestStatus status            = tcu::TestStatus::fail("Unable to encode any frames");
    tcu::VideoEncodeOutput dumpOutput = m_context.getTestContext().getCommandLine().getVideoDumpEncodeOutput();

#ifdef DE_BUILD_VIDEO
    int64_t frameInputOrderNum = 0;
    int64_t completedFrames    = 0;

    // Encode all frames
    int64_t totalFrames = m_encoder->GetNumberOfFrames();
    for (int64_t i = 0; i < totalFrames; ++i)
    {
        VkResult result = m_encoder->EncodeNextFrame(frameInputOrderNum);
        if (result != VK_SUCCESS)
        {
            if (m_hasFeedback2Config && result == VK_ERROR_DEVICE_LOST)
            {
                m_encoder = nullptr;
                TCU_THROW(NotSupportedError, "Feedback2 not supported by encoder");
            }
            status = tcu::TestStatus::fail("Failed to encode frame " + de::toString(i));
            break;
        }
        result = m_encoder->GetBitstream();
        if (result != VK_SUCCESS)
        {
            if (m_hasFeedback2Config && result == VK_ERROR_DEVICE_LOST)
            {
                m_encoder = nullptr;
                TCU_THROW(NotSupportedError, "Feedback2 not supported by encoder");
            }
            status = tcu::TestStatus::fail("Failed to get bitstream for frame " + de::toString(i));
            break;
        }
        ++completedFrames;
    }

    if (completedFrames == totalFrames)
    {
        if (VideoDevice::supportsCodecOperation(m_context, VK_VIDEO_CODEC_OPERATION_DECODE_AV1_BIT_KHR))
        {
            status = validateEncodedContent(
                VK_VIDEO_CODEC_OPERATION_ENCODE_AV1_BIT_KHR, STD_VIDEO_AV1_PROFILE_MAIN, m_outputClipFilename.c_str(),
                m_inputClipFilename.c_str(), m_definition.gop.frameCount, m_definition.frameSize.width,
                m_definition.frameSize.height, m_expectedOutputExtent,
                getChromaSubSampling(m_definition.subsampling.subsampling), getBitDepth(m_definition.bitDepth.depth),
                getBitDepth(m_definition.bitDepth.depth), PSNR_THRESHOLD_LOWER_LIMIT);
        }
        else
        {
            m_context.getTestContext().getLog()
                << tcu::TestLog::Message
                << "Skipping decode-back verification: device does not advertise the AV1 decode operation."
                << tcu::TestLog::EndMessage;
            status =
                tcu::TestStatus::pass("Encode succeeded; decode-back verification skipped (no AV1 decode support)");
        }
    }

    if (!status.isFail() && m_hasFeedback2Config)
    {
        tcu::ResultCollector results(m_context.getTestContext().getLog());
        if (!validateFeedback2Output(results))
            status = tcu::TestStatus::fail("Feedback2 validation failed");
        else if (results.getResult() != QP_TEST_RESULT_PASS)
            status = tcu::TestStatus(results.getResult(), results.getMessage());
    }
    m_encoder = nullptr;
#else
    DE_UNREF(m_definition);
    DE_UNREF(m_expectedOutputExtent);
    status = tcu::TestStatus::fail("Vulkan video is not supported on this platform");
#endif

    if (!(dumpOutput & tcu::DUMP_ENC_BITSTREAM))
        removeClip(m_outputClipFilename);
    if (m_hasFeedback2Config && !(dumpOutput & tcu::DUMP_ENC_BITSTREAM))
        removeClip(m_outputClipFilename + ".feedback.txt");

    return status;
}

VideoTestCase::VideoTestCase(tcu::TestContext &testCtx, const char *testName, const TestRequirements &requirements,
                             const TestDefinition &definition)
    : TestCase(testCtx, testName)
    , m_requirements(requirements)
    , m_definition(definition)
    , m_av1Capabilities(getVideoCapabilitiesExtensionAV1E())
    , m_intraRefreshCapabilities(getIntraRefreshCapabilities())
    , m_hasFeedback2Config(definition.feedback2.type != FEEDBACK2_DISABLED)
    , m_feedback2Config{}
{
    if (m_hasFeedback2Config)
        m_feedback2Config = definition.feedback2;
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
    TestDefinition effectiveDefinition = m_definition;
    std::stringstream ss;
    std::string deviceID;

    if (m_hasFeedback2Config)
        updateFeedback2Config(ctx, effectiveDefinition.feedback2);

    buildEncoderParams(encoderParams, m_hasFeedback2Config ? &effectiveDefinition.feedback2 : nullptr);

    std::string inputClipName("");
    buildInputClipName(getTestContext(), m_definition, inputClipName);

    std::string testName("");
    buildTestName(m_definition, testName);
    std::string clipDescriptor = buildClipDescriptor(m_definition) + "_" + testName;
    std::string encodePrefix   = util::getVideoCodecPathSegment(VK_VIDEO_CODEC_OPERATION_ENCODE_AV1_BIT_KHR);
    std::string outputClipName = util::getVideoDumpPath(true, clipDescriptor, encodePrefix, "ivf");

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

    if (m_testCtx.getCommandLine().getVideoLogPrint())
    {
        args.push_back("--verbose");

        std::cerr << "TEST ARGS: ";
        for (auto &arg : args)
            std::cerr << arg << " ";
        std::cerr << std::endl;
    }

    if (!checkClipFileExists(inputClipName))
    {
#ifdef DE_BUILD_VIDEO
        vkt::video::util::generateYCbCrFile(inputClipName, getMaxFrameCount(), m_definition.frameSize.width,
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
    VkResult result =
        CreateVulkanVideoEncoder(m_requirements.codecOperation, static_cast<int>(args.size()), args.data(), encoder);
    if (result == VK_ERROR_OUT_OF_DEVICE_MEMORY || result == VK_ERROR_OUT_OF_HOST_MEMORY)
    {
        // Some video encode tests require a very large amount of host-visible memory to run, and may fail on some platforms,
        // especially 32-bit platforms.
        TCU_THROW(NotSupportedError, "Out of memory");
    }
    else if (result != VK_SUCCESS)
    {
        if (m_hasFeedback2Config &&
            (result == VK_ERROR_FEATURE_NOT_PRESENT || result == VK_ERROR_EXTENSION_NOT_PRESENT))
        {
            throw tcu::NotSupportedError("Feedback2 not supported by encoder");
        }
        throw tcu::TestError("Failed to create VulkanVideoEncoder");
    }
#endif
    testInstance = new VideoTestInstance(ctx, inputClipName, outputClipName, expectedOutputExtent, effectiveDefinition);
#ifdef DE_BUILD_VIDEO
    testInstance->setEncoder(encoder);
#endif
    return testInstance;
}

void VideoTestCase::checkSupport(Context &ctx) const
{
    VideoDevice::checkSupport(ctx, m_requirements.codecOperation);

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

void VideoTestCase::validateCapabilities(const Context &context) const
{
    const VkVideoCodecOperationFlagBitsKHR videoCodecEncodeOperation = m_requirements.codecOperation;
    const VkVideoEncodeUsageFlagBitsKHR usageFlag                    = VK_VIDEO_ENCODE_USAGE_DEFAULT_KHR;
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

    Feedback2Def localFeedbackConfig;
    Feedback2Def *feedbackConfig = nullptr;
    if (m_hasFeedback2Config)
    {
        localFeedbackConfig = m_feedback2Config;
        feedbackConfig      = &localFeedbackConfig;
    }

    VkPhysicalDeviceVideoEncodeFeedback2FeaturesKHR feedback2Features = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VIDEO_ENCODE_FEEDBACK_2_FEATURES_KHR, // sType
        nullptr,                                                                // pNext
        VK_FALSE,                                                               // videoEncodeFeedback2
    };

    if (feedbackConfig != nullptr)
    {
        VkPhysicalDeviceFeatures2 features2 = {
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, // sType
            &feedback2Features,                           // pNext
            {},                                           // features
        };
        vki.getPhysicalDeviceFeatures2(physicalDevice, &features2);
    }

    const MovePtr<VkVideoEncodeCapabilitiesKHR> encodeCapabilities =
        getVideoEncodeCapabilities(m_av1Capabilities.get());
    if (m_requirements.useIntraRefresh)
        appendStructurePtrToVulkanChain((const void **)&encodeCapabilities->pNext, m_intraRefreshCapabilities.get());

    VkVideoEncodeFeedback2CapabilitiesKHR feedback2Capabilities = {
        VK_STRUCTURE_TYPE_VIDEO_ENCODE_FEEDBACK_2_CAPABILITIES_KHR, // sType
        nullptr,                                                    // pNext
        0u,                                                         // maxPerPartitionFeedbackEntries
        0u,                                                         // supportedPerPartitionEncodeFeedbackFlags
    };

    if (feedbackConfig != nullptr)
        appendStructurePtrToVulkanChain((const void **)&encodeCapabilities->pNext, &feedback2Capabilities);

    const MovePtr<VkVideoCapabilitiesKHR> videoCapabilities =
        getVideoCapabilities(vki, physicalDevice, videoEncodeProfile.get(), encodeCapabilities.get());

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

    uint32_t tileColumns = m_requirements.maxTileColumns;
    uint32_t tileRows    = m_requirements.maxTileRows;

    if (m_definition.tiling.tiling == TILING_MAX_SUPPORTED)
    {
        if (!computeMaxTiling(tileColumns, tileRows))
            throw tcu::NotSupportedError("Failed to compute max tile configuration");
    }

    if (tileColumns > 0 || tileRows > 0)
    {
        uint32_t minTileWidth  = (m_requirements.width + tileColumns - 1) / tileColumns;
        uint32_t minTileHeight = (m_requirements.height + tileRows - 1) / tileRows;

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

        if (tileColumns > m_av1Capabilities->maxTiles.width || tileRows > m_av1Capabilities->maxTiles.height)
        {
            throw tcu::NotSupportedError("Required tile columns/rows exceed supported maximum");
        }
    }

    MovePtr<std::vector<VkFormat>> supportedFormats =
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

        // Midway tests require maxIntraRefreshCycleDuration >= 4
        if (m_definition.intraRefresh.isMidway && m_intraRefreshCapabilities->maxIntraRefreshCycleDuration < 4)
            throw tcu::NotSupportedError("Midway tests require maxIntraRefreshCycleDuration >= 4");
    }

    if (feedbackConfig != nullptr)
        validateFeedback2Capabilities(*encodeCapabilities, feedback2Capabilities, feedback2Features, *feedbackConfig);
}

void VideoTestCase::updateFeedback2Config(const Context &context, Feedback2Def &config) const
{
    DE_ASSERT(m_hasFeedback2Config);

    const VkVideoCodecOperationFlagBitsKHR videoCodecEncodeOperation = m_requirements.codecOperation;
    const VkImageUsageFlags usageFlag                                = VK_VIDEO_ENCODE_USAGE_DEFAULT_KHR;

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

    const InstanceInterface &vki          = context.getInstanceInterface();
    const VkPhysicalDevice physicalDevice = context.getPhysicalDevice();

    VkPhysicalDeviceVideoEncodeFeedback2FeaturesKHR feedback2Features = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VIDEO_ENCODE_FEEDBACK_2_FEATURES_KHR, // sType
        nullptr,                                                                // pNext
        VK_FALSE,                                                               // videoEncodeFeedback2
    };

    VkPhysicalDeviceFeatures2 features2 = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, // sType
        &feedback2Features,                           // pNext
        {},                                           // features
    };
    vki.getPhysicalDeviceFeatures2(physicalDevice, &features2);

    const MovePtr<VkVideoEncodeAV1CapabilitiesKHR> av1Capabilities = getVideoCapabilitiesExtensionAV1E();

    VkVideoEncodeFeedback2CapabilitiesKHR feedback2Capabilities = {
        VK_STRUCTURE_TYPE_VIDEO_ENCODE_FEEDBACK_2_CAPABILITIES_KHR, // sType
        nullptr,                                                    // pNext
        0u,                                                         // maxPerPartitionFeedbackEntries
        0u,                                                         // supportedPerPartitionEncodeFeedbackFlags
    };
    appendStructurePtrToVulkanChain((const void **)&av1Capabilities->pNext, &feedback2Capabilities);

    const MovePtr<VkVideoEncodeCapabilitiesKHR> encodeCapabilities = getVideoEncodeCapabilities(av1Capabilities.get());

    const MovePtr<VkVideoCapabilitiesKHR> videoCapabilities =
        getVideoCapabilities(vki, physicalDevice, videoEncodeProfile.get(), encodeCapabilities.get());
    DE_UNREF(videoCapabilities);

    validateFeedback2Capabilities(*encodeCapabilities, feedback2Capabilities, feedback2Features, config);
}

void VideoTestCase::validateFeedback2Capabilities(
    const VkVideoEncodeCapabilitiesKHR &encodeCapabilities,
    const VkVideoEncodeFeedback2CapabilitiesKHR &feedback2Capabilities,
    const VkPhysicalDeviceVideoEncodeFeedback2FeaturesKHR &feedback2Features, Feedback2Def &config) const
{
    if (feedback2Features.videoEncodeFeedback2 == VK_FALSE)
        throw tcu::NotSupportedError("videoEncodeFeedback2 feature not supported");

    VkVideoEncodeFeedbackFlagsKHR requiredFeedbackFlags = 0u;
    if (config.enablePictureFeedback)
    {
        requiredFeedbackFlags |= VK_VIDEO_ENCODE_FEEDBACK_AVERAGE_QUANTIZATION_BIT_KHR;
        config.validateMinQp = ((encodeCapabilities.supportedEncodeFeedbackFlags &
                                 VK_VIDEO_ENCODE_FEEDBACK_MIN_QUANTIZATION_BIT_KHR) != 0);
        config.validateMaxQp = ((encodeCapabilities.supportedEncodeFeedbackFlags &
                                 VK_VIDEO_ENCODE_FEEDBACK_MAX_QUANTIZATION_BIT_KHR) != 0);
    }
    else
    {
        config.validateMinQp = false;
        config.validateMaxQp = false;
    }
    if (config.enablePixelFeedback)
    {
        requiredFeedbackFlags |=
            VK_VIDEO_ENCODE_FEEDBACK_INTRA_PIXELS_BIT_KHR | VK_VIDEO_ENCODE_FEEDBACK_INTER_PIXELS_BIT_KHR;
    }
    if (config.enableSkippedPixelFeedback)
    {
        requiredFeedbackFlags |= VK_VIDEO_ENCODE_FEEDBACK_SKIPPED_PIXELS_BIT_KHR;
    }
    if (config.enablePerPartitionFeedback)
    {
        requiredFeedbackFlags |= VK_VIDEO_ENCODE_FEEDBACK_PICTURE_PARTITION_COUNT_BIT_KHR;
    }

    if ((encodeCapabilities.supportedEncodeFeedbackFlags & requiredFeedbackFlags) != requiredFeedbackFlags)
        throw tcu::NotSupportedError("Required feedback2 flags not supported");

    if (config.enablePerPartitionFeedback)
    {
        if (feedback2Capabilities.maxPerPartitionFeedbackEntries == 0)
            throw tcu::NotSupportedError("maxPerPartitionFeedbackEntries is 0");

        const VkVideoEncodePerPartitionFeedbackFlagsKHR requiredPerPartitionFlags =
            VK_VIDEO_ENCODE_PER_PARTITION_FEEDBACK_STATUS_BIT_KHR |
            VK_VIDEO_ENCODE_PER_PARTITION_FEEDBACK_BITSTREAM_BUFFER_OFFSET_BIT_KHR |
            VK_VIDEO_ENCODE_PER_PARTITION_FEEDBACK_BITSTREAM_BYTES_WRITTEN_BIT_KHR;

        if ((feedback2Capabilities.supportedPerPartitionEncodeFeedbackFlags & requiredPerPartitionFlags) !=
            requiredPerPartitionFlags)
        {
            throw tcu::NotSupportedError("Required per-partition feedback flags not supported");
        }

        if (config.maxPerPartitionFeedbackEntries == 0)
        {
            config.maxPerPartitionFeedbackEntries = feedback2Capabilities.maxPerPartitionFeedbackEntries;
        }
        else if (config.maxPerPartitionFeedbackEntries > feedback2Capabilities.maxPerPartitionFeedbackEntries)
        {
            throw tcu::NotSupportedError("Requested maxPerPartitionFeedbackEntries exceeds capability");
        }
    }
}

bool VideoTestCase::computeMaxTiling(uint32_t &tileColumns, uint32_t &tileRows) const
{
    const uint32_t width  = m_definition.frameSize.width;
    const uint32_t height = m_definition.frameSize.height;
    const auto &caps      = *m_av1Capabilities;

    if (caps.minTileSize.width == 0 || caps.minTileSize.height == 0 || caps.maxTileSize.width == 0 ||
        caps.maxTileSize.height == 0)
    {
        return false;
    }

    auto ceilDiv = [](uint32_t value, uint32_t divisor) -> uint32_t { return (value + divisor - 1) / divisor; };

    uint32_t minCols = ceilDiv(width, caps.maxTileSize.width);
    uint32_t minRows = ceilDiv(height, caps.maxTileSize.height);

    uint32_t maxCols = width / caps.minTileSize.width;
    uint32_t maxRows = height / caps.minTileSize.height;

    if (maxCols == 0)
        maxCols = 1;
    if (maxRows == 0)
        maxRows = 1;

    maxCols = std::min(maxCols, caps.maxTiles.width);
    maxRows = std::min(maxRows, caps.maxTiles.height);

    minCols = std::max(minCols, 1u);
    minRows = std::max(minRows, 1u);

    auto highestPowerOfTwoLE = [](uint32_t value) -> uint32_t
    {
        if (value == 0)
            return 0;

        value |= value >> 1;
        value |= value >> 2;
        value |= value >> 4;
        value |= value >> 8;
        value |= value >> 16;
        return value - (value >> 1);
    };

    auto pickPowerOfTwoInRange = [&](uint32_t minValue, uint32_t maxValue) -> uint32_t
    {
        const uint32_t p2 = highestPowerOfTwoLE(maxValue);
        if (p2 < minValue)
            return 0;
        return p2;
    };

    if (maxCols < minCols || maxRows < minRows)
        return false;

    tileColumns = pickPowerOfTwoInRange(minCols, maxCols);
    tileRows    = pickPowerOfTwoInRange(minRows, maxRows);

    if (tileColumns == 0 || tileRows == 0)
        return false;

    if (m_testCtx.getCommandLine().getVideoLogPrint())
    {
        std::cerr << "AV1 tile caps: maxTiles=" << caps.maxTiles.width << "x" << caps.maxTiles.height
                  << " minTileSize=" << caps.minTileSize.width << "x" << caps.minTileSize.height
                  << " maxTileSize=" << caps.maxTileSize.width << "x" << caps.maxTileSize.height << " stdSyntaxFlags=0x"
                  << std::hex << caps.stdSyntaxFlags << std::dec << " computedRangeCols=[" << minCols << "," << maxCols
                  << "]"
                  << " computedRangeRows=[" << minRows << "," << maxRows << "]"
                  << " chosen=" << tileColumns << "x" << tileRows << std::endl;
    }
    return true;
}

VideoTestCase *createVideoTestCase(tcu::TestContext &testCtx, const char *testname,
                                   const TestRequirements &requirements, const TestDefinition &definition)
{
    VideoTestCase *testCase = new VideoTestCase(testCtx, testname, requirements, definition);
    return testCase;
}

uint32_t VideoTestCase::computeIntraRefreshCycleDuration() const
{
    // For empty-region tests, always use maxIntraRefreshCycleDuration
    if (m_definition.intraRefresh.isEmptyRegion)
        return m_intraRefreshCapabilities->maxIntraRefreshCycleDuration;

    // For midway tests, use a fixed cycle duration of 4 (as per test spec)
    if (m_definition.intraRefresh.isMidway)
        return 4;

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

void VideoTestCase::buildEncoderParams(std::vector<std::string> &params, const Feedback2Def *feedbackConfig) const
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
        params.push_back(de::toString(m_definition.gop.gopFrameCount));
        break;
    default:
        params.push_back("0");
        break;
    }

    auto appendTileParams = [&](uint32_t tileCols, uint32_t tileRows)
    {
        params.push_back("--tiles");
        params.push_back("--params");
        params.push_back("1");
        params.push_back(std::to_string(tileCols));
        params.push_back(std::to_string(tileRows));

        params.push_back("0");
    };

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
        appendTileParams(4u, 4u);
        break;
    case TILING_MAX_SUPPORTED:
    {
        uint32_t tileCols = 1;
        uint32_t tileRows = 1;
        if (!computeMaxTiling(tileCols, tileRows))
        {
            tileCols = 1;
            tileRows = 1;
        }

        if (tileCols > 1 || tileRows > 1)
            appendTileParams(tileCols, tileRows);
        break;
    }
    default:
        break;
    }

    params.push_back("--inputChromaSubsampling");
    params.push_back(std::to_string(m_definition.subsampling.subsampling));

    params.push_back("--inputBpp");
    params.push_back(std::to_string(m_definition.bitDepth.depth));

    params.push_back("--consecutiveBFrameCount");
    params.push_back(de::toString(m_definition.gop.consecutiveBFrames));

    if (!m_definition.gop.open)
    {
        params.push_back("--gopFrameCount");
        params.push_back(de::toString(m_definition.gop.gopFrameCount));
        params.push_back("--closedGop");
    }

    uint32_t qpI = m_definition.quantization.qIndex;
    uint32_t qpP = m_definition.quantization.qIndex;
    uint32_t qpB = m_definition.quantization.qIndex;
    if (feedbackConfig != nullptr && feedbackConfig->overrideQp)
    {
        qpI = feedbackConfig->qpI;
        qpP = feedbackConfig->qpP;
        qpB = feedbackConfig->qpB;
    }

    params.push_back("--qpI");
    params.push_back(de::toString(qpI));

    params.push_back("--qpP");
    params.push_back(de::toString(qpP));

    params.push_back("--qpB");
    params.push_back(de::toString(qpB));

    params.push_back("--rateControlMode");
    params.push_back(de::toString(m_definition.rateControl.rc));

    if (m_definition.loopFilter.lf == LF_ON)
        params.push_back("--lf");

    if (m_definition.loopRestore.lr == LR_ON)
        params.push_back("--lr");

    if (m_definition.cdef.cdef == CDEF_ON)
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
        const uint32_t intraRefreshCycleDuration = computeIntraRefreshCycleDuration();

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

        // Add midway parameter if this is a midway test
        if (m_definition.intraRefresh.isMidway)
        {
            // For midway tests, set the restart index to 2 (restart after 2 frames of the 4-frame cycle)
            const uint32_t intraRefreshMidwayIndex = 2;

            params.push_back("--testIntraRefreshMidway");
            params.push_back(std::to_string(intraRefreshMidwayIndex));
        }
    }

    if (feedbackConfig != nullptr)
    {
        if (feedbackConfig->enablePictureFeedback)
            params.push_back("--pictureFeedback");
        if (feedbackConfig->enablePixelFeedback)
            params.push_back("--pixelCountFeedback");
        if (feedbackConfig->enableSkippedPixelFeedback)
            params.push_back("--skippedPixelCountFeedback");
        if (feedbackConfig->enablePerPartitionFeedback)
        {
            params.push_back("--enablePerPartitionFeedback");
            params.push_back("--maxPerPartitionFeedbackEntries");
            params.push_back(de::toString(feedbackConfig->maxPerPartitionFeedbackEntries));
        }
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

    // Checks specific to feedback2 tests
    if (testDef.feedback2.type != FEEDBACK2_DISABLED)
    {
        // Feedback2 tests should only be performed with 720x480 8-bit 4:2:0.
        if (testDef.frameSize.width != 720 || testDef.frameSize.height != 480 ||
            testDef.bitDepth.depth != BIT_DEPTH_8 || testDef.subsampling.subsampling != CHROMA_SS_420)
        {
            return false;
        }

        // Feedback2 tests should not combine unrelated features.
        if (testDef.ordering.order != ORDERED || testDef.resolutionChange.resolutionChange != RESOLUTION_NO_CHANGE ||
            testDef.superblock.superblock != SUPERBLOCK_64x64 || testDef.loopFilter.lf != LF_OFF ||
            testDef.loopRestore.lr != LR_OFF || testDef.cdef.cdef != CDEF_OFF ||
            testDef.dpbMode.mode != DPB_MODE_LAYERED || testDef.intraRefresh.mode != IR_OFF || testDef.gop.open)
        {
            return false;
        }

        switch (testDef.feedback2.type)
        {
        case FEEDBACK2_PARTITION_COUNT_1:
            if (testDef.gop.gop != GOP_I || testDef.gop.frameCount != 1 || testDef.tiling.tiling != TILING_1_TILE ||
                strcmp(testDef.gop.subName, "i") != 0 || testDef.quantization.qIndex != QINDEX_NONE ||
                testDef.rateControl.rc != RC_DEFAULT)
            {
                return false;
            }
            break;
        case FEEDBACK2_PARTITION_COUNT_MAX:
        case FEEDBACK2_PARTITION_COUNT_MAX_TRUNCATED:
            if (testDef.gop.gop != GOP_I || testDef.gop.frameCount != 1 || strcmp(testDef.gop.subName, "i") != 0 ||
                testDef.tiling.tiling != TILING_MAX_SUPPORTED || testDef.quantization.qIndex != QINDEX_NONE ||
                testDef.rateControl.rc != RC_DEFAULT)
            {
                return false;
            }
            break;
        case FEEDBACK2_INTRA_INTER_PIXELS:
        case FEEDBACK2_INTRA_INTER_SKIP_PIXELS:
            if (testDef.gop.gop != GOP_I_P || testDef.gop.frameCount != 2 || testDef.tiling.tiling != TILING_1_TILE ||
                strcmp(testDef.gop.subName, "i_p") != 0 || testDef.quantization.qIndex != QINDEX_NONE ||
                testDef.rateControl.rc != RC_DEFAULT)
            {
                return false;
            }
            break;
        case FEEDBACK2_QP_AVERAGE_MIN_MAX:
        case FEEDBACK2_QP_WITH_QUANTIZATION_EQUIV:
            if (testDef.gop.gop != GOP_I_P || testDef.gop.frameCount != 2 || testDef.tiling.tiling != TILING_1_TILE ||
                strcmp(testDef.gop.subName, "i_p") != 0 || testDef.quantization.qIndex != QINDEX_64 ||
                testDef.rateControl.rc != RC_DISABLED)
            {
                return false;
            }
            break;
        default:
            return false;
        }
        return true;
    }

    // Short GOPs and max-supported tiling are only instantiated to cover feedback2 AV1 tests.
    if ((testDef.gop.frameCount == 1 && strcmp(testDef.gop.subName, "i") == 0) ||
        (testDef.gop.frameCount == 2 && strcmp(testDef.gop.subName, "i_p") == 0) ||
        testDef.tiling.tiling == TILING_MAX_SUPPORTED)
    {
        return false;
    }

    // Checks specific to intra-refresh tests
    if (testDef.intraRefresh.mode != IR_OFF)
    {
        // Intra-refresh tests should only be performed with 352x288 resolution
        if (testDef.frameSize.width != 352 || testDef.frameSize.height != 288)
            return false;

        // Intra-refresh tests should not combine the rest of parameters
        if (testDef.ordering.order != ORDERED || testDef.resolutionChange.resolutionChange != RESOLUTION_NO_CHANGE ||
            testDef.quantization.qIndex != QINDEX_NONE || testDef.superblock.superblock != SUPERBLOCK_64x64 ||
            testDef.rateControl.rc != RC_DEFAULT || testDef.loopFilter.lf != LF_OFF ||
            testDef.loopRestore.lr != LR_OFF || testDef.cdef.cdef != CDEF_OFF)
        {
            return false;
        }

        // Intra-refresh is only supported with P frames, not with B frames
        if (testDef.gop.gop != GOP_I_P)
            return false;

        // Empty-region tests should only have 2 frames (IDR + P), use i_p_empty_region GOP, and no tiling
        if (testDef.intraRefresh.isEmptyRegion &&
            (testDef.gop.frameCount != 2 || strcmp(testDef.gop.subName, "i_p_empty_region") != 0 ||
             testDef.tiling.tiling != TILING_1_TILE))
        {
            return false;
        }

        // Only empty-region intra-refresh tests should use the empty-region GOP
        if (!testDef.intraRefresh.isEmptyRegion && strcmp(testDef.gop.subName, "i_p_empty_region") == 0)
            return false;

        // Midway tests should have 7 frames (1 IDR + 6 P) and use i_p_midway GOP
        if (testDef.intraRefresh.isMidway &&
            (testDef.gop.frameCount != 7 || strcmp(testDef.gop.subName, "i_p_midway") != 0))
        {
            return false;
        }

        // Only midway intra-refresh tests should use the midway GOP
        if (!testDef.intraRefresh.isMidway && strcmp(testDef.gop.subName, "i_p_midway") == 0)
            return false;

        return true;
    }

    // The nested combination of tests should be performed only with 720x480
    if (testDef.frameSize.width != 720 && testDef.frameSize.height != 480 &&
        (testDef.ordering.order != ORDERED || testDef.resolutionChange.resolutionChange != RESOLUTION_NO_CHANGE ||
         testDef.quantization.qIndex != QINDEX_NONE || testDef.superblock.superblock != SUPERBLOCK_64x64 ||
         testDef.rateControl.rc != RC_DEFAULT || testDef.loopFilter.lf != LF_OFF || testDef.loopRestore.lr != LR_OFF ||
         testDef.cdef.cdef != CDEF_OFF))
    {
        return false;
    }

    // Test only GOP_I_P in the case of resolution different from 720x480
    if (testDef.frameSize.width != 720 && testDef.frameSize.height != 480 && (testDef.gop.gop != GOP_I_P))
        return false;

    // Remove TILING_1x2 from 7680x4320 resolution as it is not supported by the AV1 specification
    // See MAX_TILE_WIDTH in https://aomediacodec.github.io/av1-spec/av1-spec.pdf
    if (testDef.frameSize.width == 7680 && testDef.frameSize.height == 4320 && (testDef.tiling.tiling == TILING_1x2))
        return false;

    // Non-intra-refresh tests should not use intra-refresh midway or empty region GOPs
    if (testDef.intraRefresh.mode == IR_OFF &&
        (strcmp(testDef.gop.subName, "i_p_midway") == 0 || strcmp(testDef.gop.subName, "i_p_empty_region") == 0))
    {
        return false;
    }

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

bool parseU32(const std::string &value, uint32_t &out)
{
    char *end            = nullptr;
    unsigned long parsed = std::strtoul(value.c_str(), &end, 10);
    if (end == nullptr || *end != '\0')
        return false;
    if (parsed > std::numeric_limits<uint32_t>::max())
        return false;
    out = static_cast<uint32_t>(parsed);
    return true;
}

bool parseI32(const std::string &value, int32_t &out)
{
    char *end   = nullptr;
    long parsed = std::strtol(value.c_str(), &end, 10);
    if (end == nullptr || *end != '\0')
        return false;
    if (parsed < std::numeric_limits<int32_t>::min() || parsed > std::numeric_limits<int32_t>::max())
        return false;
    out = static_cast<int32_t>(parsed);
    return true;
}

bool VideoTestInstance::getParsedAv1Partitions(std::vector<Feedback2PartitionFeedback> &partitions,
                                               uint32_t &bitstreamDataLen)
{
#ifdef DE_BUILD_VIDEO
    partitions.clear();
    bitstreamDataLen = 0u;

    VideoDevice::VideoDeviceFlags videoDeviceFlags = VideoDevice::VIDEO_DEVICE_FLAG_REQUIRE_SYNC2_OR_NOT_SUPPORTED;
    const VkPhysicalDevice physicalDevice          = m_context.getPhysicalDevice();
    const VkDevice videoDevice                     = getDeviceSupportingQueue(
        VK_QUEUE_VIDEO_ENCODE_BIT_KHR | VK_QUEUE_VIDEO_DECODE_BIT_KHR | VK_QUEUE_TRANSFER_BIT,
        VK_VIDEO_CODEC_OPERATION_ENCODE_AV1_BIT_KHR | VK_VIDEO_CODEC_OPERATION_DECODE_AV1_BIT_KHR, videoDeviceFlags);
    const DeviceInterface &videoDeviceDriver = getDeviceDriver();

    const uint32_t encodeQueueFamilyIndex   = getQueueFamilyIndexEncode();
    const uint32_t decodeQueueFamilyIndex   = getQueueFamilyIndexDecode();
    const uint32_t transferQueueFamilyIndex = getQueueFamilyIndexTransfer();

    const VkQueue encodeQueue   = getDeviceQueue(videoDeviceDriver, videoDevice, encodeQueueFamilyIndex, 0u);
    const VkQueue decodeQueue   = getDeviceQueue(videoDeviceDriver, videoDevice, decodeQueueFamilyIndex, 0u);
    const VkQueue transferQueue = getDeviceQueue(videoDeviceDriver, videoDevice, transferQueueFamilyIndex, 0u);

    DeviceContext deviceContext(&m_context, &m_videoDevice, physicalDevice, videoDevice, decodeQueue, encodeQueue,
                                transferQueue);

    const auto decodeProfile = VkVideoCoreProfile(
        VK_VIDEO_CODEC_OPERATION_DECODE_AV1_BIT_KHR, getChromaSubSampling(m_definition.subsampling.subsampling),
        getBitDepth(m_definition.bitDepth.depth), getBitDepth(m_definition.bitDepth.depth), STD_VIDEO_AV1_PROFILE_MAIN);

    auto basicDecoder = createBasicDecoder(&deviceContext, &decodeProfile, 1, false);

    Demuxer::Params demuxParams = {};
    demuxParams.data            = std::make_unique<BufferedReader>(m_outputClipFilename.c_str());
    demuxParams.codecOperation  = VK_VIDEO_CODEC_OPERATION_DECODE_AV1_BIT_KHR;
    demuxParams.framing         = ElementaryStreamFraming::IVF;
    auto demuxer                = Demuxer::create(std::move(demuxParams));

    VkVideoParser parser;
    createParser(demuxer->codecOperation(), basicDecoder, parser, demuxer->framing());

    FrameProcessor processor(std::move(demuxer), basicDecoder);
    DecodedFrame frame;
    if (processor.getNextFrame(&frame) <= 0)
    {
        m_context.getTestContext().getLog()
            << tcu::TestLog::Message << "Failed to decode encoded AV1 bitstream for partition validation"
            << tcu::TestLog::EndMessage;
        return false;
    }
    processor.releaseFrame(&frame);

    if (basicDecoder->m_cachedDecodeParams.empty() || !basicDecoder->m_cachedDecodeParams.front())
    {
        m_context.getTestContext().getLog()
            << tcu::TestLog::Message << "Missing cached AV1 decode parameters for partition validation"
            << tcu::TestLog::EndMessage;
        return false;
    }

    const auto &av1PictureData = basicDecoder->m_cachedDecodeParams.front()->av1PicParams;
    const size_t parserDataLen = basicDecoder->m_cachedDecodeParams.front()->pictureParams.bitstreamDataLen;
    const uint32_t tileCount   = av1PictureData.khr_info.tileCount;
    const uint32_t maxTileOffsets =
        static_cast<uint32_t>(sizeof(av1PictureData.tileOffsets) / sizeof(av1PictureData.tileOffsets[0]));
    const uint32_t maxTileSizes =
        static_cast<uint32_t>(sizeof(av1PictureData.tileSizes) / sizeof(av1PictureData.tileSizes[0]));
    if (tileCount > maxTileOffsets || tileCount > maxTileSizes)
    {
        m_context.getTestContext().getLog()
            << tcu::TestLog::Message << "Parsed AV1 tile count exceeds parser storage limits"
            << tcu::TestLog::EndMessage;
        return false;
    }
    if (parserDataLen > static_cast<size_t>(std::numeric_limits<uint32_t>::max()))
    {
        m_context.getTestContext().getLog()
            << tcu::TestLog::Message << "Parsed AV1 bitstream size exceeds CTS limits" << tcu::TestLog::EndMessage;
        return false;
    }
    bitstreamDataLen = static_cast<uint32_t>(parserDataLen);

    partitions.reserve(tileCount);
    for (uint32_t tileNdx = 0; tileNdx < tileCount; ++tileNdx)
    {
        Feedback2PartitionFeedback partition{};
        partition.index     = tileNdx;
        partition.hasStatus = false;
        partition.status    = VK_QUERY_RESULT_STATUS_NOT_READY_KHR;
        partition.offset    = av1PictureData.tileOffsets[tileNdx];
        partition.size      = av1PictureData.tileSizes[tileNdx];
        partitions.push_back(partition);
    }

    return true;
#else
    DE_UNREF(partitions);
    DE_UNREF(bitstreamDataLen);
    return false;
#endif
}

bool VideoTestInstance::parseFeedback2Output(const std::string &path,
                                             std::map<uint64_t, Feedback2FrameFeedback> &frames)
{
    std::ifstream file(path.c_str());
    if (!file.is_open())
    {
        m_context.getTestContext().getLog()
            << tcu::TestLog::Message << "Failed to open feedback file: " << path << tcu::TestLog::EndMessage;
        return false;
    }

    std::string line;
    Feedback2FrameFeedback *currentFrame = nullptr;

    while (std::getline(file, line))
    {
        if (line.empty())
            continue;

        std::istringstream ss(line);
        std::string token;
        if (!(ss >> token))
            continue;

        if (token == "frame")
        {
            uint64_t frameIndex = 0;
            if (!(ss >> frameIndex))
            {
                m_context.getTestContext().getLog()
                    << tcu::TestLog::Message << "Malformed frame line in feedback file: " << line
                    << tcu::TestLog::EndMessage;
                return false;
            }

            auto it = frames.find(frameIndex);
            if (it == frames.end())
            {
                Feedback2FrameFeedback frame{};
                frame.frameIndex = frameIndex;
                it               = frames.emplace(frameIndex, frame).first;
            }

            currentFrame = &it->second;

            std::string kv;
            while (ss >> kv)
            {
                const size_t pos = kv.find('=');
                if (pos == std::string::npos)
                    continue;

                const std::string key = kv.substr(0, pos);
                const std::string val = kv.substr(pos + 1);

                if (key == "avgqp")
                {
                    int32_t v = 0;
                    if (!parseI32(val, v))
                        return false;
                    currentFrame->hasAvgQp = true;
                    currentFrame->avgQp    = v;
                }
                else if (key == "minqp")
                {
                    int32_t v = 0;
                    if (!parseI32(val, v))
                        return false;
                    currentFrame->hasMinQp = true;
                    currentFrame->minQp    = v;
                }
                else if (key == "maxqp")
                {
                    int32_t v = 0;
                    if (!parseI32(val, v))
                        return false;
                    currentFrame->hasMaxQp = true;
                    currentFrame->maxQp    = v;
                }
                else if (key == "intra")
                {
                    uint32_t v = 0;
                    if (!parseU32(val, v))
                        return false;
                    currentFrame->hasIntraPixels = true;
                    currentFrame->intraPixels    = v;
                }
                else if (key == "inter")
                {
                    uint32_t v = 0;
                    if (!parseU32(val, v))
                        return false;
                    currentFrame->hasInterPixels = true;
                    currentFrame->interPixels    = v;
                }
                else if (key == "skipped")
                {
                    uint32_t v = 0;
                    if (!parseU32(val, v))
                        return false;
                    currentFrame->hasSkippedPixels = true;
                    currentFrame->skippedPixels    = v;
                }
                else if (key == "pic_partition_count")
                {
                    uint32_t v = 0;
                    if (!parseU32(val, v))
                        return false;
                    currentFrame->hasPicturePartitionCount = true;
                    currentFrame->picturePartitionCount    = v;
                }
                else if (key == "bs_offset")
                {
                    uint32_t v = 0;
                    if (!parseU32(val, v))
                        return false;
                    currentFrame->hasBitstreamBufferOffset = true;
                    currentFrame->bitstreamBufferOffset    = v;
                }
                else if (key == "bs_size")
                {
                    uint32_t v = 0;
                    if (!parseU32(val, v))
                        return false;
                    currentFrame->hasBitstreamBytesWritten = true;
                    currentFrame->bitstreamBytesWritten    = v;
                }
                else if (key == "status")
                {
                    int32_t v = 0;
                    if (!parseI32(val, v))
                        return false;
                    currentFrame->hasStatus = true;
                    currentFrame->status    = static_cast<VkQueryResultStatusKHR>(v);
                }
            }
        }
        else if (token == "partition")
        {
            if (currentFrame == nullptr)
            {
                m_context.getTestContext().getLog()
                    << tcu::TestLog::Message << "Partition line without frame context: " << line
                    << tcu::TestLog::EndMessage;
                return false;
            }

            uint32_t index = 0;
            if (!(ss >> index))
            {
                m_context.getTestContext().getLog()
                    << tcu::TestLog::Message << "Malformed partition line in feedback file: " << line
                    << tcu::TestLog::EndMessage;
                return false;
            }

            Feedback2PartitionFeedback partition{};
            partition.index = index;

            bool hasStatus = false;
            bool hasOffset = false;
            bool hasSize   = false;

            std::string kv;
            while (ss >> kv)
            {
                const size_t pos = kv.find('=');
                if (pos == std::string::npos)
                    continue;

                const std::string key = kv.substr(0, pos);
                const std::string val = kv.substr(pos + 1);

                if (key == "offset")
                {
                    uint32_t v = 0;
                    if (!parseU32(val, v))
                        return false;
                    partition.offset = v;
                    hasOffset        = true;
                }
                else if (key == "size")
                {
                    uint32_t v = 0;
                    if (!parseU32(val, v))
                        return false;
                    partition.size = v;
                    hasSize        = true;
                }
                else if (key == "status")
                {
                    int32_t v = 0;
                    if (!parseI32(val, v))
                        return false;
                    partition.status = static_cast<VkQueryResultStatusKHR>(v);
                    hasStatus        = true;
                }
            }

            if (!hasOffset || !hasSize)
            {
                m_context.getTestContext().getLog()
                    << tcu::TestLog::Message << "Partition line missing offset/size: " << line
                    << tcu::TestLog::EndMessage;
                return false;
            }

            partition.hasStatus = hasStatus;
            currentFrame->partitions.push_back(partition);
        }
    }

    return !frames.empty();
}

bool VideoTestInstance::validateFeedback2Output(tcu::ResultCollector &results)
{
    const std::string feedbackPath = m_outputClipFilename + ".feedback.txt";
    std::map<uint64_t, Feedback2FrameFeedback> frames;
    if (!parseFeedback2Output(feedbackPath, frames))
        return false;

    auto getFrame = [&](uint64_t index) -> const Feedback2FrameFeedback *
    {
        auto it = frames.find(index);
        return (it != frames.end()) ? &it->second : nullptr;
    };

    const uint32_t superblockSize = m_definition.superblock.superblock;
    const auto alignUp            = [](uint32_t value, uint32_t align) { return (value + align - 1) / align * align; };

    const uint64_t minPixels64 =
        static_cast<uint64_t>(m_definition.frameSize.width) * static_cast<uint64_t>(m_definition.frameSize.height);
    const uint32_t alignedWidth  = alignUp(m_definition.frameSize.width, superblockSize);
    const uint32_t alignedHeight = alignUp(m_definition.frameSize.height, superblockSize);
    const uint64_t maxPixels64   = static_cast<uint64_t>(alignedWidth) * static_cast<uint64_t>(alignedHeight);
    const uint32_t minPixels     = static_cast<uint32_t>(
        std::min<uint64_t>(minPixels64, static_cast<uint64_t>(std::numeric_limits<uint32_t>::max())));
    const uint32_t maxPixels = static_cast<uint32_t>(
        std::min<uint64_t>(maxPixels64, static_cast<uint64_t>(std::numeric_limits<uint32_t>::max())));

    switch (m_feedback2Config.type)
    {
    case FEEDBACK2_PARTITION_COUNT_1:
    case FEEDBACK2_PARTITION_COUNT_MAX:
    case FEEDBACK2_PARTITION_COUNT_MAX_TRUNCATED:
    {
        const Feedback2FrameFeedback *frame = getFrame(0);
        if (frame == nullptr)
            return false;

        if (!frame->hasStatus || frame->status != VK_QUERY_RESULT_STATUS_COMPLETE_KHR)
            return false;
        if (!frame->hasPicturePartitionCount || frame->picturePartitionCount == 0u)
            return false;

        if (!frame->hasBitstreamBytesWritten || frame->bitstreamBytesWritten == 0u)
            return false;

        std::vector<Feedback2PartitionFeedback> parsedPartitions;
        uint32_t parserBitstreamBytes = 0u;
        if (!getParsedAv1Partitions(parsedPartitions, parserBitstreamBytes))
            return false;
        const uint32_t parsedCount = static_cast<uint32_t>(parsedPartitions.size());
        if (parsedCount != frame->picturePartitionCount)
            return false;
        if (parserBitstreamBytes < frame->bitstreamBytesWritten)
            return false;

        const uint32_t reservedEntries  = (m_feedback2Config.maxPerPartitionFeedbackEntries == 0u) ?
                                              frame->picturePartitionCount :
                                              m_feedback2Config.maxPerPartitionFeedbackEntries;
        const uint32_t expectedReported = std::min(frame->picturePartitionCount, reservedEntries);
        const uint32_t actualReported   = static_cast<uint32_t>(frame->partitions.size());
        if (actualReported != expectedReported)
        {
            std::ostringstream msg;
            msg << "expected " << expectedReported << " per-partition feedback entries, got " << actualReported
                << ". Validating available entries only.";
            results.addResult(QP_TEST_RESULT_QUALITY_WARNING, msg.str());
        }
        const uint32_t entriesToValidate = std::min(expectedReported, actualReported);
        if (entriesToValidate == 0u)
        {
            results.addResult(QP_TEST_RESULT_QUALITY_WARNING,
                              "no per-partition feedback entries available to validate");
            return true;
        }

        const uint32_t parserToFeedbackOffsetBias = parserBitstreamBytes - frame->bitstreamBytesWritten;

        for (uint32_t partitionNdx = 0; partitionNdx < entriesToValidate; ++partitionNdx)
        {
            const auto &feedbackPartition = frame->partitions[partitionNdx];
            if (feedbackPartition.index != partitionNdx)
                return false;
            if (!feedbackPartition.hasStatus || feedbackPartition.status != VK_QUERY_RESULT_STATUS_COMPLETE_KHR)
                return false;

            const auto &parsedPartition = parsedPartitions[partitionNdx];
            if (parsedPartition.offset < parserToFeedbackOffsetBias)
                return false;

            if (feedbackPartition.offset != parsedPartition.offset - parserToFeedbackOffsetBias)
                return false;
            if (feedbackPartition.size != parsedPartition.size)
                return false;
        }

        return true;
    }
    case FEEDBACK2_INTRA_INTER_PIXELS:
    {
        // Per VK_KHR_video_encode_feedback2: the captured intra/inter pixel counts always
        // include padding pixels beyond the requested coded extent that are part of the
        // complete coding blocks. For AV1 profiles that support SKIPPED_PIXELS_BIT_KHR, the
        // implementation may either include skipped pixels in the intra/inter counts ("both")
        // or exclude them from both ("neither"); the convention is consistent across frames.
        // We detect the convention from the keyframe (frame 0): since it has no inter blocks,
        // intra(frame0) == totalPadded iff the implementation uses the include-skipped
        // convention. Conservation across the two identical input frames is only assertable
        // under the include-skipped convention.
        const Feedback2FrameFeedback *frame0 = getFrame(0);
        const Feedback2FrameFeedback *frame1 = getFrame(1);
        if (frame0 == nullptr || frame1 == nullptr)
            return false;
        if (!frame0->hasIntraPixels || !frame0->hasInterPixels)
            return false;
        if (!frame1->hasIntraPixels || !frame1->hasInterPixels)
            return false;

        // Keyframe contains only intra blocks; interPixels must be 0 under either convention.
        if (frame0->interPixels != 0u)
            return false;

        const uint64_t totalPadded = static_cast<uint64_t>(maxPixels);
        const uint64_t frame0Sum =
            static_cast<uint64_t>(frame0->intraPixels) + static_cast<uint64_t>(frame0->interPixels);
        const uint64_t frame1Sum =
            static_cast<uint64_t>(frame1->intraPixels) + static_cast<uint64_t>(frame1->interPixels);

        // intra+inter cannot exceed the coded extent padded to entire coding blocks.
        if (frame0Sum > totalPadded || frame1Sum > totalPadded)
            return false;

        const bool includeSkipped = (frame0Sum == totalPadded);
        if (includeSkipped)
        {
            // Total pixel count is conserved across the two identical input frames.
            if (frame1Sum != totalPadded)
                return false;
        }

        return true;
    }
    case FEEDBACK2_INTRA_INTER_SKIP_PIXELS:
    {
        // AV1 with SKIPPED pixel feedback enabled. Per VK_KHR_video_encode_feedback2, when
        // SKIPPED_PIXELS_BIT_KHR is supported by an AV1 profile, the implementation may
        // either include skipped pixels in the intra/inter counts or exclude them from both,
        // consistently across frames. The keyframe defines two candidate totals:
        //   totalPixelCount1 = intraPixelCount(frame0)                    (include-skipped)
        //   totalPixelCount2 = intraPixelCount(frame0) + skippedPixelCount(frame0)  (exclude-skipped)
        // For the two identical input frames, the inter frame's pixel counts must match the
        // keyframe's totals under whichever convention the implementation chose.
        const Feedback2FrameFeedback *frame0 = getFrame(0);
        const Feedback2FrameFeedback *frame1 = getFrame(1);
        if (frame0 == nullptr || frame1 == nullptr)
            return false;

        const auto u32Max  = static_cast<uint64_t>(std::numeric_limits<uint32_t>::max());
        const auto inRange = [&](uint64_t v) { return v >= minPixels && v <= maxPixels; };

        // Keyframe: interPixelCount must be 0. Either totalPixelCount1 or totalPixelCount2
        // must fall within [minPixels, maxPixels]. skippedPixelCount is not required to be zero.
        if (!frame0->hasIntraPixels || !frame0->hasInterPixels || !frame0->hasSkippedPixels)
            return false;
        if (frame0->interPixels != 0u)
            return false;

        const uint64_t totalPixelCount1 = static_cast<uint64_t>(frame0->intraPixels);
        const uint64_t totalPixelCount2 = totalPixelCount1 + static_cast<uint64_t>(frame0->skippedPixels);
        if (totalPixelCount2 > u32Max)
            return false;
        if (!inRange(totalPixelCount1) && !inRange(totalPixelCount2))
            return false;

        // Inter frame: assert cross-frame conservation under one of the two conventions.
        //   include-skipped: intra(frame1) + inter(frame1) == totalPixelCount1
        //   exclude-skipped: intra(frame1) + inter(frame1) + skipped(frame1) == totalPixelCount2
        // intraPixelCount must be < 10% of the matched keyframe total. skippedPixelCount must
        // be <= interPixelCount. The source clip has two identical frames, so a low skip/inter
        // ratio is suspicious; warn (do not fail) when skippedPixels/interPixels <= 50%.
        if (!frame1->hasIntraPixels || !frame1->hasInterPixels || !frame1->hasSkippedPixels)
            return false;

        const uint64_t frame1Sum1 =
            static_cast<uint64_t>(frame1->intraPixels) + static_cast<uint64_t>(frame1->interPixels);
        const uint64_t frame1Sum2 = frame1Sum1 + static_cast<uint64_t>(frame1->skippedPixels);
        if (frame1Sum2 > u32Max)
            return false;
        const bool matchNoSkip   = (frame1Sum1 == totalPixelCount1);
        const bool matchWithSkip = (frame1Sum2 == totalPixelCount2);
        if (!matchNoSkip && !matchWithSkip)
            return false;

        const uint64_t referenceTotal = matchNoSkip ? totalPixelCount1 : totalPixelCount2;
        if (referenceTotal == 0u)
            return false;
        if (static_cast<uint64_t>(frame1->intraPixels) * 10u >= referenceTotal)
            return false;

        if (frame1->skippedPixels > frame1->interPixels)
            return false;

        if (frame1->interPixels > 0u &&
            static_cast<uint64_t>(frame1->skippedPixels) * 2u <= static_cast<uint64_t>(frame1->interPixels))
        {
            std::ostringstream msg;
            msg << "low skip ratio on inter frame (skippedPixels=" << frame1->skippedPixels
                << ", interPixels=" << frame1->interPixels
                << "); identical input frames are expected to produce a high proportion of skip pixels.";
            results.addResult(QP_TEST_RESULT_QUALITY_WARNING, msg.str());
        }

        return true;
    }
    case FEEDBACK2_QP_AVERAGE_MIN_MAX:
    case FEEDBACK2_QP_WITH_QUANTIZATION_EQUIV:
    {
        const Feedback2FrameFeedback *frame0 = getFrame(0);
        const Feedback2FrameFeedback *frame1 = getFrame(1);
        if (frame0 == nullptr || frame1 == nullptr)
            return false;

        const int32_t qpMin = 0;
        const int32_t qpMax = 255;
        auto validateQp     = [&](const Feedback2FrameFeedback &frame, int32_t expectedQp) -> bool
        {
            if (!frame.hasAvgQp)
                return false;
            if (m_feedback2Config.validateMinQp && !frame.hasMinQp)
                return false;
            if (m_feedback2Config.validateMaxQp && !frame.hasMaxQp)
                return false;
            if (m_feedback2Config.validateMinQp && frame.minQp < qpMin)
                return false;
            if (m_feedback2Config.validateMaxQp && frame.maxQp > qpMax)
                return false;
            if (m_feedback2Config.validateMinQp && frame.minQp > frame.avgQp)
                return false;
            if (m_feedback2Config.validateMaxQp && frame.avgQp > frame.maxQp)
                return false;

            return (frame.avgQp == expectedQp);
        };

        if (!validateQp(*frame0, static_cast<int32_t>(m_feedback2Config.qpI)))
            return false;
        if (!validateQp(*frame1, static_cast<int32_t>(m_feedback2Config.qpP)))
            return false;

        return true;
    }
    default:
        return false;
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
    if (testDef.feedback2.type != FEEDBACK2_DISABLED)
    {
        testName = testDef.feedback2.subName;
        return;
    }

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
    if (testDef.feedback2.type != FEEDBACK2_DISABLED)
        requirements.extensions.push_back("VK_KHR_video_encode_feedback2");

    requirements.codecOperation = VK_VIDEO_CODEC_OPERATION_ENCODE_AV1_BIT_KHR;

    requirements.width  = testDef.frameSize.width;
    requirements.height = testDef.frameSize.height;

    requirements.bitDepth    = getBitDepth(testDef.bitDepth.depth);
    requirements.subSampling = getChromaSubSampling(testDef.subsampling.subsampling);

    requirements.requireBFrames = (testDef.gop.gop == GOP_I_P_B || testDef.gop.gop == GOP_IDR_P_B);

    requirements.useVariableBitrate = (testDef.rateControl.rc == RC_VBR);
    requirements.useConstantBitrate = (testDef.rateControl.rc == RC_CBR);

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
    case TILING_MAX_SUPPORTED:
        requirements.maxTileColumns = 0;
        requirements.maxTileRows    = 0;
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
    {1, GOP_I, false, 1, 0, "i"},
    {2, GOP_I_P, false, 2, 0, "i_p"},
    {15, GOP_I, false, 1, 0, "i"},
    {15, GOP_I_P, false, 15, 0, "i_p"},
    {15, GOP_I_P, true, 15, 0, "i_p_open"},
    {15, GOP_I_P_B, false, 13, 3, "i_p_b3_13"},
    {15, GOP_IDR_P_B, false, 13, 3, "idr_p_b3_13"},
    {2, GOP_I_P, false, 2, 0, "i_p_empty_region"}, // Special GOP for empty-region tests
    {7, GOP_I_P, false, 2, 0, "i_p_midway"},       // Special GOP for midway tests (1 IDR + 6 P frames)
};

#ifdef DE_BUILD_VIDEO
// Finds the max frameCount from gopTests.
// TODO: When we update to C++20 this can be made constexpr.
static uint32_t getMaxFrameCount()
{
    return std::transform_reduce(
        gopTests.begin(), gopTests.end(), 1, [](uint32_t a, uint32_t b) { return std::max(a, b); },
        [](const GOPDef &gop) { return gop.frameCount; });
}
#endif

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
    {TILING_MAX_SUPPORTED, "tiling_max"},
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
    {IR_OFF, false, false, ""},
    {IR_PICTURE_PARTITION, false, false, "intra_refresh_picture_partition"},
    {IR_ROW_BASED, false, false, "intra_refresh_row_based"},
    {IR_COLUMN_BASED, false, false, "intra_refresh_column_based"},
    {IR_ANY_BLOCK_BASED, false, false, "intra_refresh_any_block_based"},
    {IR_ROW_BASED, true, false, "intra_refresh_row_based_empty_region"},
    {IR_COLUMN_BASED, true, false, "intra_refresh_column_based_empty_region"},
    {IR_ANY_BLOCK_BASED, true, false, "intra_refresh_any_block_based_empty_region"},
    {IR_PICTURE_PARTITION, false, true, "intra_refresh_picture_partition_midway"},
    {IR_ROW_BASED, false, true, "intra_refresh_row_based_midway"},
    {IR_COLUMN_BASED, false, true, "intra_refresh_column_based_midway"},
    {IR_ANY_BLOCK_BASED, false, true, "intra_refresh_any_block_based_midway"},
};

static const Feedback2Def feedback2Disabled = {
    FEEDBACK2_DISABLED, "", nullptr, false, false, false, false, 0u, false, 0u, 0u, 0u, false, false,
};
static const std::vector<Feedback2Def> feedback2Tests = {
    {FEEDBACK2_PARTITION_COUNT_1, "feedback2_partition_count_1", nullptr, false, false, false, true, 1u, false, 0u, 0u,
     0u, false, false},
    {FEEDBACK2_PARTITION_COUNT_MAX, "feedback2_partition_count_max", nullptr, false, false, false, true, 0u, false, 0u,
     0u, 0u, false, false},
    {FEEDBACK2_PARTITION_COUNT_MAX_TRUNCATED, "feedback2_partition_count_max_truncated", nullptr, false, false, false,
     true, 1u, false, 0u, 0u, 0u, false, false},
    {FEEDBACK2_INTRA_INTER_PIXELS, "feedback2_intra_inter_pixels", "identical_2f_", false, true, false, false, 0u,
     false, 0u, 0u, 0u, false, false},
    {FEEDBACK2_INTRA_INTER_SKIP_PIXELS, "feedback2_intra_inter_skip_pixels", "identical_2f_", false, true, true, false,
     0u, false, 0u, 0u, 0u, false, false},
    {FEEDBACK2_QP_AVERAGE_MIN_MAX, "feedback2_qp_average_min_max", nullptr, true, false, false, false, 0u, true, 64u,
     64u, 64u, false, false},
    {FEEDBACK2_QP_WITH_QUANTIZATION_EQUIV, "feedback2_qp_with_quantization_equiv", nullptr, true, false, false, false,
     0u, true, 64u, 128u, 128u, false, false},
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
                                                                    intraRefreshTest, feedback2Disabled};
                                                                auto testCase =
                                                                    createVideoEncodeTestAV1(testCtx, testDef);
                                                                if (testCase != nullptr)
                                                                    gopGroup->addChild(testCase);

                                                                for (const auto &feedback2Test : feedback2Tests)
                                                                {
                                                                    const TestDefinition feedback2TestDef = {
                                                                        frameSizeTest,    bitDepthTest,
                                                                        subsamplingTest,  gopTest,
                                                                        orderingTest,     resolutionChangeTest,
                                                                        quantizationTest, tilingTest,
                                                                        superblockTest,   rateControlTest,
                                                                        lfTest,           lrTest,
                                                                        cdefTest,         dpbModeTest,
                                                                        intraRefreshTest, feedback2Test};

                                                                    auto feedback2Case = createVideoEncodeTestAV1(
                                                                        testCtx, feedback2TestDef);
                                                                    if (feedback2Case != nullptr)
                                                                        gopGroup->addChild(feedback2Case);
                                                                }
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
