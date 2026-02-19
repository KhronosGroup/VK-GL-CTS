/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2025 The Khronos Group Inc.
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
 * \brief Video Profiles Validation tests
 *//*--------------------------------------------------------------------*/

#include "vktVideoProfilesValidationTests.hpp"
#include "deStringUtil.hpp"
#include "vkDefs.hpp"
#include "vktVideoTestUtils.hpp"

namespace vkt
{
namespace video
{
namespace
{
using namespace vk;
using namespace std;

union CodecCaps
{
    VkVideoDecodeH264CapabilitiesKHR h264Dec;
    VkVideoDecodeH265CapabilitiesKHR h265Dec;
    VkVideoDecodeAV1CapabilitiesKHR av1Dec;
    VkVideoDecodeVP9CapabilitiesKHR vp9Dec;

    VkVideoEncodeH264CapabilitiesKHR h264Enc;
    VkVideoEncodeH265CapabilitiesKHR h265Enc;
    VkVideoEncodeAV1CapabilitiesKHR av1Enc;
};

struct TestParams
{
    union
    {
        VkVideoDecodeH264ProfileInfoKHR h264Dec;
        VkVideoDecodeH265ProfileInfoKHR h265Dec;
        VkVideoDecodeAV1ProfileInfoKHR av1Dec;
        VkVideoDecodeVP9ProfileInfoKHR vp9Dec;

        VkVideoEncodeH264ProfileInfoKHR h264Enc;
        VkVideoEncodeH265ProfileInfoKHR h265Enc;
        VkVideoEncodeAV1ProfileInfoKHR av1Enc;
    } codecProfile;
    VkVideoProfileInfoKHR profile;
    VkVideoProfileListInfoKHR profileList;

    CodecCaps codecCaps;
    VkVideoCapabilitiesKHR videoCaps;
    VkVideoDecodeCapabilitiesKHR decodeCaps;
    VkVideoEncodeCapabilitiesKHR encodeCaps;

    bool isEncode() const
    {
        return (profile.videoCodecOperation == VK_VIDEO_CODEC_OPERATION_ENCODE_H264_BIT_KHR ||
                profile.videoCodecOperation == VK_VIDEO_CODEC_OPERATION_ENCODE_H265_BIT_KHR ||
                profile.videoCodecOperation == VK_VIDEO_CODEC_OPERATION_ENCODE_AV1_BIT_KHR);
    }

    bool isDecode() const
    {
        return (profile.videoCodecOperation == VK_VIDEO_CODEC_OPERATION_DECODE_H264_BIT_KHR ||
                profile.videoCodecOperation == VK_VIDEO_CODEC_OPERATION_DECODE_H265_BIT_KHR ||
                profile.videoCodecOperation == VK_VIDEO_CODEC_OPERATION_DECODE_AV1_BIT_KHR ||
                profile.videoCodecOperation == VK_VIDEO_CODEC_OPERATION_DECODE_VP9_BIT_KHR);
    }
};

auto getComponentBitdepth = [](VkVideoComponentBitDepthFlagsKHR flags)
{
    switch (flags)
    {
    case VK_VIDEO_COMPONENT_BIT_DEPTH_8_BIT_KHR:
        return 8;
    case VK_VIDEO_COMPONENT_BIT_DEPTH_10_BIT_KHR:
        return 10;
    case VK_VIDEO_COMPONENT_BIT_DEPTH_12_BIT_KHR:
        return 12;
    default:
        TCU_THROW(InternalError, "unknown component bit depth");
    }
};

typedef de::SharedPtr<TestParams> SharedTestParams;

class VideoProfilesValidationTestInstance : public VideoBaseTestInstance
{
public:
    VideoProfilesValidationTestInstance(Context &context, const SharedTestParams &params);
    ~VideoProfilesValidationTestInstance(void);

    tcu::TestStatus iterate(void);

protected:
    SharedTestParams m_params;
    DeviceContext m_deviceContext;

    tcu::TestStatus tryCreateVideoSession(VkFormat imageFormat, VkFormat dpbFormat);
    tcu::TestStatus validateProfileCompatibility();
    tcu::TestStatus validateVideoFormatsWithProfile();
    tcu::TestStatus validateWithFormatProperties(VkVideoFormatPropertiesKHR video_fmt, VkImageUsageFlags usage);
    tcu::TestStatus validateWithImageFormatProperties(VkVideoFormatPropertiesKHR video_fmt, VkImageUsageFlags usage);

private:
    bool validateProfileCodec();
    bool videoMaintenance1Support;
};

VideoProfilesValidationTestInstance::VideoProfilesValidationTestInstance(Context &context,
                                                                         const SharedTestParams &params)
    : VideoBaseTestInstance(context)
    , m_params(params)
    , m_deviceContext(&m_context, &m_videoDevice)
{
    VkQueueFlags queueFlags = VK_QUEUE_TRANSFER_BIT;

    if (m_params->isDecode())
        queueFlags |= VK_QUEUE_VIDEO_DECODE_BIT_KHR;
    else
        queueFlags |= VK_QUEUE_VIDEO_ENCODE_BIT_KHR;

    VideoDevice::VideoDeviceFlags flags = VideoDevice::VIDEO_DEVICE_FLAG_REQUIRE_SYNC2_OR_NOT_SUPPORTED;
    if (m_params->profile.videoCodecOperation == VK_VIDEO_CODEC_OPERATION_DECODE_VP9_BIT_KHR)
        flags |= VideoDevice::VIDEO_DEVICE_FLAG_REQUIRE_DECODE_VP9;

    videoMaintenance1Support = context.isDeviceFunctionalitySupported("VK_KHR_video_maintenance1");

    if (videoMaintenance1Support)
        flags |= VideoDevice::VIDEO_DEVICE_FLAG_REQUIRE_MAINTENANCE_1;

    VkDevice device = getDeviceSupportingQueue(queueFlags, m_params->profile.videoCodecOperation, flags);

    m_deviceContext.updateDevice(
        m_context.getPhysicalDevice(), device,
        m_params->isDecode() ?
            getDeviceQueue(m_context.getDeviceInterface(), device, m_videoDevice.getQueueFamilyIndexDecode(), 0) :
            nullptr,
        m_params->isEncode() ?
            getDeviceQueue(m_context.getDeviceInterface(), device, m_videoDevice.getQueueFamilyIndexEncode(), 0) :
            nullptr,
        getDeviceQueue(m_context.getDeviceInterface(), device, m_videoDevice.getQueueFamilyIndexTransfer(), 0));
}

VideoProfilesValidationTestInstance::~VideoProfilesValidationTestInstance(void)
{
}

tcu::TestStatus VideoProfilesValidationTestInstance::tryCreateVideoSession(VkFormat imageFormat, VkFormat dpbFormat)
{
    const DeviceInterface &vkd = m_deviceContext.getDeviceDriver();
    VkDevice device            = m_deviceContext.device;
    auto queueFamilyIndex =
        m_params->isEncode() ? m_deviceContext.encodeQueueFamilyIdx() : m_deviceContext.decodeQueueFamilyIdx();

    static const VkExtensionProperties h264DecodeStdExtensionVersion = {
        VK_STD_VULKAN_VIDEO_CODEC_H264_DECODE_EXTENSION_NAME, VK_STD_VULKAN_VIDEO_CODEC_H264_DECODE_SPEC_VERSION};
    static const VkExtensionProperties h265DecodeStdExtensionVersion = {
        VK_STD_VULKAN_VIDEO_CODEC_H265_DECODE_EXTENSION_NAME, VK_STD_VULKAN_VIDEO_CODEC_H265_DECODE_SPEC_VERSION};
    static const VkExtensionProperties av1DecodeStdExtensionVersion = {
        VK_STD_VULKAN_VIDEO_CODEC_AV1_DECODE_EXTENSION_NAME, VK_STD_VULKAN_VIDEO_CODEC_AV1_DECODE_SPEC_VERSION};
    static const VkExtensionProperties vp9DecodeStdExtensionVersion = {
        VK_STD_VULKAN_VIDEO_CODEC_VP9_DECODE_EXTENSION_NAME, VK_STD_VULKAN_VIDEO_CODEC_VP9_DECODE_SPEC_VERSION};
    static const VkExtensionProperties h264EncodeStdExtensionVersion = {
        VK_STD_VULKAN_VIDEO_CODEC_H264_ENCODE_EXTENSION_NAME, VK_STD_VULKAN_VIDEO_CODEC_H264_ENCODE_SPEC_VERSION};
    static const VkExtensionProperties h265EncodeStdExtensionVersion = {
        VK_STD_VULKAN_VIDEO_CODEC_H265_ENCODE_EXTENSION_NAME, VK_STD_VULKAN_VIDEO_CODEC_H265_ENCODE_SPEC_VERSION};
    static const VkExtensionProperties av1EncodeStdExtensionVersion = {
        VK_STD_VULKAN_VIDEO_CODEC_AV1_ENCODE_EXTENSION_NAME, VK_STD_VULKAN_VIDEO_CODEC_AV1_ENCODE_SPEC_VERSION};

    VkVideoSessionCreateInfoKHR createInfo = initVulkanStructure();

    createInfo.pVideoProfile              = &m_params->profile;
    createInfo.queueFamilyIndex           = queueFamilyIndex;
    createInfo.pictureFormat              = imageFormat;
    createInfo.maxCodedExtent             = m_params->videoCaps.maxCodedExtent;
    createInfo.maxDpbSlots                = m_params->videoCaps.maxDpbSlots;
    createInfo.maxActiveReferencePictures = m_params->videoCaps.maxActiveReferencePictures;
    createInfo.referencePictureFormat     = dpbFormat;

    switch (m_params->profile.videoCodecOperation)
    {
    case VK_VIDEO_CODEC_OPERATION_DECODE_H264_BIT_KHR:
        createInfo.pStdHeaderVersion = &h264DecodeStdExtensionVersion;
        break;
    case VK_VIDEO_CODEC_OPERATION_DECODE_H265_BIT_KHR:
        createInfo.pStdHeaderVersion = &h265DecodeStdExtensionVersion;
        break;
    case VK_VIDEO_CODEC_OPERATION_DECODE_AV1_BIT_KHR:
        createInfo.pStdHeaderVersion = &av1DecodeStdExtensionVersion;
        break;
    case VK_VIDEO_CODEC_OPERATION_DECODE_VP9_BIT_KHR:
        createInfo.pStdHeaderVersion = &vp9DecodeStdExtensionVersion;
        break;
    case VK_VIDEO_CODEC_OPERATION_ENCODE_H264_BIT_KHR:
        createInfo.pStdHeaderVersion = &h264EncodeStdExtensionVersion;
        break;
    case VK_VIDEO_CODEC_OPERATION_ENCODE_H265_BIT_KHR:
        createInfo.pStdHeaderVersion = &h265EncodeStdExtensionVersion;
        break;
    case VK_VIDEO_CODEC_OPERATION_ENCODE_AV1_BIT_KHR:
        createInfo.pStdHeaderVersion = &av1EncodeStdExtensionVersion;
        break;
    default:
        DE_ASSERT(0);
    }

    VkVideoSessionKHR videoSession;

    VkResult result = vkd.createVideoSessionKHR(device, &createInfo, NULL, &videoSession);
    if (result != VK_SUCCESS)
    {
        ostringstream failMsg;
        failMsg << "Failed to create a video session with " << result;
        return tcu::TestStatus::fail(failMsg.str());
    }

    vkd.destroyVideoSessionKHR(device, videoSession, nullptr);

    return tcu::TestStatus::pass("OK");
}

bool VideoProfilesValidationTestInstance::validateProfileCodec()
{
    uint8_t chromaSubsampling = VK_VIDEO_CHROMA_SUBSAMPLING_MONOCHROME_BIT_KHR |
                                VK_VIDEO_CHROMA_SUBSAMPLING_420_BIT_KHR | VK_VIDEO_CHROMA_SUBSAMPLING_422_BIT_KHR |
                                VK_VIDEO_CHROMA_SUBSAMPLING_444_BIT_KHR;

    uint8_t bitDepth = VK_VIDEO_COMPONENT_BIT_DEPTH_8_BIT_KHR | VK_VIDEO_COMPONENT_BIT_DEPTH_10_BIT_KHR |
                       VK_VIDEO_COMPONENT_BIT_DEPTH_12_BIT_KHR;
    bool shouldSameBitdepth = false;

    switch (m_params->profile.videoCodecOperation)
    {
    // H.264 profile compatibility validation
    case VK_VIDEO_CODEC_OPERATION_DECODE_H264_BIT_KHR:
    case VK_VIDEO_CODEC_OPERATION_ENCODE_H264_BIT_KHR:
    {
        StdVideoH264ProfileIdc profileIdc = m_params->isDecode() ? m_params->codecProfile.h264Dec.stdProfileIdc :
                                                                   m_params->codecProfile.h264Enc.stdProfileIdc;

        switch (profileIdc)
        {
        case STD_VIDEO_H264_PROFILE_IDC_BASELINE:
        case STD_VIDEO_H264_PROFILE_IDC_MAIN:
            // These profiles only support 8-bit, 4:2:0
            chromaSubsampling = VK_VIDEO_CHROMA_SUBSAMPLING_420_BIT_KHR;
            bitDepth          = VK_VIDEO_COMPONENT_BIT_DEPTH_8_BIT_KHR;
            break;
        case STD_VIDEO_H264_PROFILE_IDC_HIGH:
            // only support monochrome/8-bit, 4:2:0
            chromaSubsampling =
                VK_VIDEO_CHROMA_SUBSAMPLING_MONOCHROME_BIT_KHR | VK_VIDEO_CHROMA_SUBSAMPLING_420_BIT_KHR;
            bitDepth = VK_VIDEO_COMPONENT_BIT_DEPTH_8_BIT_KHR;
            break;
        default:
            break;
        }
        break;
    }
    // H.265 profile compatibility validation
    case VK_VIDEO_CODEC_OPERATION_DECODE_H265_BIT_KHR:
    case VK_VIDEO_CODEC_OPERATION_ENCODE_H265_BIT_KHR:
    {
        StdVideoH265ProfileIdc profileIdc = m_params->isDecode() ? m_params->codecProfile.h265Dec.stdProfileIdc :
                                                                   m_params->codecProfile.h265Enc.stdProfileIdc;

        switch (profileIdc)
        {
        case STD_VIDEO_H265_PROFILE_IDC_MAIN:
        case STD_VIDEO_H265_PROFILE_IDC_MAIN_STILL_PICTURE:
            // Main profile: 8-bit, 4:2:0 only
            chromaSubsampling = VK_VIDEO_CHROMA_SUBSAMPLING_420_BIT_KHR;
            bitDepth          = VK_VIDEO_COMPONENT_BIT_DEPTH_8_BIT_KHR;
            break;
        case STD_VIDEO_H265_PROFILE_IDC_MAIN_10:
            // Main 10 profile: 8/10-bit, 4:2:0 only
            chromaSubsampling = VK_VIDEO_CHROMA_SUBSAMPLING_420_BIT_KHR;
            bitDepth          = VK_VIDEO_COMPONENT_BIT_DEPTH_8_BIT_KHR | VK_VIDEO_COMPONENT_BIT_DEPTH_10_BIT_KHR;
            break;
        default:
            break;
        }
        break;
    }
    // AV1 profile compatibility validation
    case VK_VIDEO_CODEC_OPERATION_DECODE_AV1_BIT_KHR:
    case VK_VIDEO_CODEC_OPERATION_ENCODE_AV1_BIT_KHR:
    {
        StdVideoAV1Profile profileIdc =
            m_params->isDecode() ? m_params->codecProfile.av1Dec.stdProfile : m_params->codecProfile.av1Enc.stdProfile;

        if ((m_params->profile.chromaSubsampling != VK_VIDEO_CHROMA_SUBSAMPLING_MONOCHROME_BIT_KHR) &&
            m_params->profile.lumaBitDepth != m_params->profile.chromaBitDepth)
        {
            shouldSameBitdepth = true;
            break;
        }

        switch (profileIdc)
        {
        case STD_VIDEO_AV1_PROFILE_MAIN:
            // Main profile: 8-bit or 10-bit, monochrome/4:2:0 only
            chromaSubsampling =
                VK_VIDEO_CHROMA_SUBSAMPLING_MONOCHROME_BIT_KHR | VK_VIDEO_CHROMA_SUBSAMPLING_420_BIT_KHR;
            bitDepth = VK_VIDEO_COMPONENT_BIT_DEPTH_8_BIT_KHR | VK_VIDEO_COMPONENT_BIT_DEPTH_10_BIT_KHR;
            break;
        case STD_VIDEO_AV1_PROFILE_HIGH:
            // High profile: 8-bit or 10-bit, monochrome/4:2:0 or 4:4:4
            chromaSubsampling = VK_VIDEO_CHROMA_SUBSAMPLING_MONOCHROME_BIT_KHR |
                                VK_VIDEO_CHROMA_SUBSAMPLING_420_BIT_KHR | VK_VIDEO_CHROMA_SUBSAMPLING_444_BIT_KHR;
            bitDepth = VK_VIDEO_COMPONENT_BIT_DEPTH_8_BIT_KHR | VK_VIDEO_COMPONENT_BIT_DEPTH_10_BIT_KHR;
            break;
        default:
            break;
        }
        break;
    }
    // VP9 profile compatibility validation
    case VK_VIDEO_CODEC_OPERATION_DECODE_VP9_BIT_KHR:
    {
        if (m_params->profile.lumaBitDepth != m_params->profile.chromaBitDepth)
        {
            shouldSameBitdepth = true;
            break;
        }

        switch (m_params->codecProfile.vp9Dec.stdProfile)
        {
        case STD_VIDEO_VP9_PROFILE_0:
        case STD_VIDEO_VP9_PROFILE_2:
            // Should only support 4:2:0
            chromaSubsampling = VK_VIDEO_CHROMA_SUBSAMPLING_420_BIT_KHR;
            break;
        case STD_VIDEO_VP9_PROFILE_1:
        case STD_VIDEO_VP9_PROFILE_3:
            // Should support 4:2:2 or 4:4:4
            chromaSubsampling = VK_VIDEO_CHROMA_SUBSAMPLING_422_BIT_KHR | VK_VIDEO_CHROMA_SUBSAMPLING_444_BIT_KHR;
            break;
        default:
            TCU_THROW(InternalError, "unknown vp9 profile");
        }

        switch (m_params->codecProfile.vp9Dec.stdProfile)
        {
        case STD_VIDEO_VP9_PROFILE_0:
        case STD_VIDEO_VP9_PROFILE_1:
            // 8-bit only
            bitDepth = VK_VIDEO_COMPONENT_BIT_DEPTH_8_BIT_KHR;
            break;
        case STD_VIDEO_VP9_PROFILE_2:
        case STD_VIDEO_VP9_PROFILE_3:
            // 10-bit or 12-bit (not 8-bit)
            bitDepth = VK_VIDEO_COMPONENT_BIT_DEPTH_10_BIT_KHR | VK_VIDEO_COMPONENT_BIT_DEPTH_12_BIT_KHR;
            break;
        default:
            TCU_THROW(InternalError, "unknown vp9 profile");
        }
        break;
    }
    default:
        DE_ASSERT(0);
    }

    if (shouldSameBitdepth)
        return false;

    if (!(m_params->profile.chromaSubsampling & chromaSubsampling))
        return false;

    if (!(m_params->profile.lumaBitDepth & bitDepth) || !(m_params->profile.chromaBitDepth & bitDepth))
        return false;

    return true;
}

tcu::TestStatus VideoProfilesValidationTestInstance::validateWithFormatProperties(VkVideoFormatPropertiesKHR video_fmt,
                                                                                  VkImageUsageFlags usage)
{
    const InstanceInterface &vk = m_context.getInstanceInterface();
    const VkPhysicalDevice phys = m_context.getPhysicalDevice();

    VkFormatProperties2 formatProperties2 = initVulkanStructure();
    vk.getPhysicalDeviceFormatProperties2(phys, video_fmt.format, &formatProperties2);

    VkFormatFeatureFlags features = 0;

    if (m_params->isDecode())
    {
        if (usage & VK_IMAGE_USAGE_VIDEO_DECODE_DST_BIT_KHR)
            features |= VK_FORMAT_FEATURE_VIDEO_DECODE_OUTPUT_BIT_KHR;

        if (usage & VK_IMAGE_USAGE_VIDEO_DECODE_DPB_BIT_KHR)
            features |= VK_FORMAT_FEATURE_VIDEO_DECODE_DPB_BIT_KHR;
    }
    else
    {
        if (usage & VK_IMAGE_USAGE_VIDEO_ENCODE_SRC_BIT_KHR)
            features |= VK_FORMAT_FEATURE_VIDEO_ENCODE_INPUT_BIT_KHR;

        if (usage & VK_IMAGE_USAGE_VIDEO_ENCODE_DPB_BIT_KHR)
            features |= VK_FORMAT_FEATURE_VIDEO_ENCODE_DPB_BIT_KHR;
    }

    if (video_fmt.imageTiling == VK_IMAGE_TILING_LINEAR)
    {
        if ((formatProperties2.formatProperties.linearTilingFeatures & features) == 0)
            return tcu::TestStatus::fail("Not aligned with linear features");
    }
    else if (video_fmt.imageTiling == VK_IMAGE_TILING_OPTIMAL)
    {
        if ((formatProperties2.formatProperties.optimalTilingFeatures & features) == 0)
            return tcu::TestStatus::fail("Not aligned with optimal features");
    }

    return tcu::TestStatus::pass("OK");
}

std::vector<uint64_t> getDrmFormatModifier(Context &context, VkFormat format)
{
    const InstanceInterface &vki = context.getInstanceInterface();
    const VkPhysicalDevice phys  = context.getPhysicalDevice();
    std::vector<uint64_t> drmModifiers;

    VkDrmFormatModifierPropertiesList2EXT drmFormatProperties = initVulkanStructure();
    VkFormatProperties2 formatProperties2                     = initVulkanStructure(&drmFormatProperties);
    vki.getPhysicalDeviceFormatProperties2(phys, format, &formatProperties2);

    if (drmFormatProperties.drmFormatModifierCount == 0)
        TCU_THROW(NotSupportedError, "No DRM format modifier found for " + de::toString(format));

    std::vector<VkDrmFormatModifierProperties2EXT> drmFormatModifiers(drmFormatProperties.drmFormatModifierCount);
    drmFormatProperties.pDrmFormatModifierProperties = drmFormatModifiers.data();
    vki.getPhysicalDeviceFormatProperties2(phys, format, &formatProperties2);

    for (const auto modifier : drmFormatModifiers)
        drmModifiers.push_back(modifier.drmFormatModifier);

    return drmModifiers;
}

tcu::TestStatus VideoProfilesValidationTestInstance::validateWithImageFormatProperties(
    VkVideoFormatPropertiesKHR video_fmt, VkImageUsageFlags usage)
{
    const InstanceInterface &vk = m_context.getInstanceInterface();
    const VkPhysicalDevice phys = m_context.getPhysicalDevice();

    VkImageCreateFlags requiredCreateFlags = 0;
    VkImageUsageFlags requiredUsageFlags   = 0;
    if (m_params->isDecode())
    {
        if (usage & VK_IMAGE_USAGE_VIDEO_DECODE_DPB_BIT_KHR)
            requiredUsageFlags |= VK_IMAGE_USAGE_VIDEO_DECODE_DPB_BIT_KHR;

        if (usage & VK_IMAGE_USAGE_VIDEO_DECODE_DST_BIT_KHR)
        {
            requiredUsageFlags |= VK_IMAGE_USAGE_VIDEO_DECODE_DST_BIT_KHR;
            requiredUsageFlags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
            requiredUsageFlags |= VK_IMAGE_USAGE_SAMPLED_BIT;

            if (isYCbCrFormat(video_fmt.format))
                requiredCreateFlags |= (VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT | VK_IMAGE_CREATE_EXTENDED_USAGE_BIT);
        }
    }
    else
    {
        if (usage & VK_IMAGE_USAGE_VIDEO_ENCODE_DPB_BIT_KHR)
            requiredUsageFlags |= VK_IMAGE_USAGE_VIDEO_ENCODE_DPB_BIT_KHR;

        if (usage & VK_IMAGE_USAGE_VIDEO_ENCODE_SRC_BIT_KHR)
        {
            requiredUsageFlags |= VK_IMAGE_USAGE_VIDEO_ENCODE_SRC_BIT_KHR;
            requiredUsageFlags |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;

            if (isYCbCrFormat(video_fmt.format))
                requiredCreateFlags |= (VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT | VK_IMAGE_CREATE_EXTENDED_USAGE_BIT);
        }
    }

    bool enoughImageCreateFlags = (video_fmt.imageCreateFlags & requiredCreateFlags) == requiredCreateFlags;
    bool enoughImageUsageFlags  = (video_fmt.imageUsageFlags & requiredUsageFlags) == requiredUsageFlags;

    std::vector<uint64_t> drmModifiers;
    uint32_t nCnt                = 1;
    bool foundCompatibleModifier = false;

    if (video_fmt.imageTiling == VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT)
    {
        drmModifiers = getDrmFormatModifier(m_context, video_fmt.format);
        nCnt         = (uint32_t)drmModifiers.size();

        if (nCnt == 0)
            return tcu::TestStatus::fail("The number of DRM modifier is 0 even though it is exposed to be supported.");
    }

    for (uint32_t i = 0; i < nCnt; i++)
    {
        VkPhysicalDeviceImageDrmFormatModifierInfoEXT imageFormatModifierInfo = initVulkanStructure();
        VkImageFormatListCreateInfo imageFormatListInfo                       = initVulkanStructure();
        imageFormatListInfo.viewFormatCount                                   = 1;
        imageFormatListInfo.pViewFormats                                      = &video_fmt.format;

        if (drmModifiers.size() > 0)
        {
            imageFormatModifierInfo.drmFormatModifier = drmModifiers[i];
            imageFormatListInfo.pNext                 = &imageFormatModifierInfo;
            m_params->profileList.pNext               = &imageFormatListInfo;
        }

        VkPhysicalDeviceImageFormatInfo2 imageFormatInfo2 = initVulkanStructure(&m_params->profileList);
        imageFormatInfo2.format                           = video_fmt.format;
        imageFormatInfo2.type                             = video_fmt.imageType;
        imageFormatInfo2.tiling                           = video_fmt.imageTiling;
        imageFormatInfo2.usage                            = requiredUsageFlags;
        imageFormatInfo2.flags                            = requiredCreateFlags;
        VkImageFormatProperties2 imageFormatProperties2   = initVulkanStructure();

        VkResult result = vk.getPhysicalDeviceImageFormatProperties2(phys, &imageFormatInfo2, &imageFormatProperties2);
        if (result != VK_SUCCESS)
        {
            // Modifier is not compatible
            if (video_fmt.imageTiling == VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT &&
                result == VK_ERROR_FORMAT_NOT_SUPPORTED)
                continue;

            ostringstream failMsg;

            failMsg << "Failed query call to vkGetPhysicalDeviceImageFormatPropertiesKHR with " << result;
            return tcu::TestStatus::fail(failMsg.str());
        }

        // Verify that the queried video format has necessary image usages.
        // vkGetPhysicalDeviceImageFormatProperties2 returns success with the required image usages,
        // that means the queried video format should have the same.
        if (!enoughImageUsageFlags)
        {
            ostringstream warnMsg;

            warnMsg << "The video format doesn't have enough image usage flags necessary even though it supports via "
                       "vkGetPhysicalDeviceImageFormatPropertiesKHR";
            return tcu::TestStatus(QP_TEST_RESULT_CAPABILITY_WARNING, warnMsg.str());
        }

        // Verify that the queried video format has necessary image create flags.
        // vkGetPhysicalDeviceImageFormatProperties2 returns success with the required image create flags,
        // that means the queried video format should have the same.
        if (!enoughImageCreateFlags)
        {
            ostringstream warnMsg;

            warnMsg << "The video format doesn't have enough image create flags necessary even though it supports via "
                       "vkGetPhysicalDeviceImageFormatPropertiesKHR";
            return tcu::TestStatus(QP_TEST_RESULT_CAPABILITY_WARNING, warnMsg.str());
        }

        foundCompatibleModifier = true;
    }

    if (video_fmt.imageTiling == VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT && !foundCompatibleModifier)
        return tcu::TestStatus::fail("No compatible modifier found for the video format");

    return tcu::TestStatus::pass("OK");
}

tcu::TestStatus VideoProfilesValidationTestInstance::validateVideoFormatsWithProfile()
{
    tcu::TestStatus ret{QP_TEST_RESULT_PASS, "OK"};

    const InstanceInterface &vk  = m_context.getInstanceInterface();
    const VkPhysicalDevice phys  = m_context.getPhysicalDevice();
    uint32_t numVideoFormatInfos = 0;
    bool separateDpbAndOutputOnly =
        !(m_params->decodeCaps.flags & VK_VIDEO_DECODE_CAPABILITY_DPB_AND_OUTPUT_COINCIDE_BIT_KHR);
    bool coincideDpbAndOutputOnly =
        !(m_params->decodeCaps.flags & VK_VIDEO_DECODE_CAPABILITY_DPB_AND_OUTPUT_DISTINCT_BIT_KHR);
    VkImageUsageFlags dpbAndOutputCoincideUsage =
        VK_IMAGE_USAGE_VIDEO_DECODE_DPB_BIT_KHR | VK_IMAGE_USAGE_VIDEO_DECODE_DST_BIT_KHR;

    std::vector<VkImageUsageFlags> usageFlags;

    if (m_params->isDecode())
    {
        usageFlags.push_back(VK_IMAGE_USAGE_VIDEO_DECODE_DPB_BIT_KHR);
        usageFlags.push_back(VK_IMAGE_USAGE_VIDEO_DECODE_DST_BIT_KHR);
        if (!separateDpbAndOutputOnly)
            usageFlags.push_back(VK_IMAGE_USAGE_VIDEO_DECODE_DPB_BIT_KHR | VK_IMAGE_USAGE_VIDEO_DECODE_DST_BIT_KHR);
    }
    else
    {
        usageFlags.push_back(VK_IMAGE_USAGE_VIDEO_ENCODE_DPB_BIT_KHR);
        usageFlags.push_back(VK_IMAGE_USAGE_VIDEO_ENCODE_SRC_BIT_KHR);
    }

    std::vector<VkFormat> imageFormats;
    std::vector<VkFormat> dpbImageFormats;

    for (VkImageUsageFlags usage : usageFlags)
    {
        VkPhysicalDeviceVideoFormatInfoKHR formatInfo = initVulkanStructure(&m_params->profileList);
        formatInfo.imageUsage                         = usage;
        m_params->profileList                         = initVulkanStructure();
        m_params->profileList.profileCount            = 1;
        m_params->profileList.pProfiles               = &m_params->profile;

        VkResult result =
            vk.getPhysicalDeviceVideoFormatPropertiesKHR(phys, &formatInfo, &numVideoFormatInfos, nullptr);

        if (result != VK_SUCCESS)
        {
            ostringstream failMsg;

            failMsg << "Failed query call to vkGetPhysicalDeviceVideoFormatPropertiesKHR with " << result;
            return tcu::TestStatus::fail(failMsg.str());
        }

        // Return fail if the number of supported formats is 0.
        if (numVideoFormatInfos == 0)
            return tcu::TestStatus::fail(
                "The number of supported formats is 0 even though the profile is exposed to be supported.");

        std::vector<VkVideoFormatPropertiesKHR> formats;
        formats.resize(numVideoFormatInfos);
        for (auto &fmt : formats)
            fmt = initVulkanStructure();

        result = vk.getPhysicalDeviceVideoFormatPropertiesKHR(phys, &formatInfo, &numVideoFormatInfos, formats.data());

        if (result != VK_SUCCESS)
        {
            ostringstream failMsg;

            failMsg << "Failed query call to vkGetPhysicalDeviceVideoFormatPropertiesKHR with " << result;
            return tcu::TestStatus::fail(failMsg.str());
        }

        for (auto &fmt : formats)
        {
            // Validate if bit depth is compatible with the queried format.
            if (isYCbCrFormat(fmt.format))
            {
                const tcu::UVec4 formatBitdepth(ycbcr::getYCbCrBitDepth(fmt.format));
                unsigned lumaBitDepth   = getComponentBitdepth(m_params->profile.lumaBitDepth);
                unsigned chromaBitDepth = getComponentBitdepth(m_params->profile.chromaBitDepth);
                if (lumaBitDepth != formatBitdepth.x() || chromaBitDepth != formatBitdepth.y())
                {
                    ostringstream failMsg;

                    failMsg << "The format " << fmt.format << " (" << formatBitdepth.x() << ", " << formatBitdepth.y()
                            << ") should be compatible with bit depth: (" << lumaBitDepth << ", " << chromaBitDepth
                            << ")";
                    return tcu::TestStatus::fail(failMsg.str());
                }
            }

            // Validate if the queried image usages includes requested image usages.
            if (!(fmt.imageUsageFlags & usage))
                return tcu::TestStatus::fail("The queried video format doesn't have any image usage requested");

            // Validate if the queried image usage is aligned with the queried video capability.
            if (m_params->isDecode() && separateDpbAndOutputOnly)
            {
                if ((fmt.imageUsageFlags & dpbAndOutputCoincideUsage) == dpbAndOutputCoincideUsage)
                {
                    ostringstream failMsg;

                    failMsg
                        << "Image Usage should be supported separately for DECODE_DPB and DECODE_DST since the video "
                           "capability doesn't expose VK_VIDEO_DECODE_CAPABILITY_DPB_AND_OUTPUT_COINCIDE_BIT_KHR. ";
                    return tcu::TestStatus::fail(failMsg.str());
                }
            }

            if (m_params->isDecode() && coincideDpbAndOutputOnly)
            {
                if ((fmt.imageUsageFlags & dpbAndOutputCoincideUsage) != dpbAndOutputCoincideUsage)
                {
                    ostringstream failMsg;

                    failMsg << "Image Usage should not be supported for DECODE_DPB or DECODE_DST separately since the "
                               "video "
                               "capability doesn't expose VK_VIDEO_DECODE_CAPABILITY_DPB_AND_OUTPUT_DISTINCT_BIT_KHR. ";
                    return tcu::TestStatus::fail(failMsg.str());
                }
            }

            // Validate if the queried video format is aligned with VkFormatProperties2 from getPhysicalDeviceFormat.
            ret = validateWithFormatProperties(fmt, usage);
            if (ret.getCode() != QP_TEST_RESULT_PASS)
                return ret;

            // Validate if the queried video format is aligned with VkImageFormatProperties2 from getPhysicalDeviceImageFormat.
            ret = validateWithImageFormatProperties(fmt, usage);
            if (ret.getCode() != QP_TEST_RESULT_PASS)
                return ret;

            if (fmt.imageUsageFlags & VK_IMAGE_USAGE_VIDEO_DECODE_DPB_BIT_KHR ||
                fmt.imageUsageFlags & VK_IMAGE_USAGE_VIDEO_ENCODE_DPB_BIT_KHR)
            {
                dpbImageFormats.push_back(fmt.format);
            }

            if (fmt.imageUsageFlags & VK_IMAGE_USAGE_VIDEO_DECODE_DST_BIT_KHR ||
                fmt.imageUsageFlags & VK_IMAGE_USAGE_VIDEO_ENCODE_SRC_BIT_KHR)
            {
                imageFormats.push_back(fmt.format);
            }
        }
    }

    /* Make a set of unique pairs for dpb format and img format */
    std::set<std::pair<VkFormat, VkFormat>> formatSet;
    for (VkFormat imgFmt : imageFormats)
        for (VkFormat dpbFmt : dpbImageFormats)
            formatSet.insert({imgFmt, dpbFmt});

    if (formatSet.size() == 0)
        return tcu::TestStatus::fail("No supported video formats");

    for (const auto &fmts : formatSet)
    {
        ret = tryCreateVideoSession(fmts.first, fmts.second);
        if (ret.isFail())
            return ret;
    }

    return tcu::TestStatus::pass("OK");
}

tcu::TestStatus VideoProfilesValidationTestInstance::iterate(void)
{
    const InstanceInterface &vk           = m_context.getInstanceInterface();
    const VkPhysicalDevice physicalDevice = m_context.getPhysicalDevice();

    VkResult result =
        vk.getPhysicalDeviceVideoCapabilitiesKHR(physicalDevice, &m_params->profile, &m_params->videoCaps);

    bool validCodec = validateProfileCodec();

    if (result == vk::VK_SUCCESS && !validCodec)
    {
        return tcu::TestStatus::fail(
            "Video capability query should return an error when an invalid video profile is specified");
    }
    else if (result != VK_SUCCESS)
    {
        switch (result)
        {
        case VK_ERROR_VIDEO_PICTURE_LAYOUT_NOT_SUPPORTED_KHR:
            TCU_THROW(NotSupportedError, "video picture layout not supported");
        case VK_ERROR_VIDEO_PROFILE_FORMAT_NOT_SUPPORTED_KHR:
            TCU_THROW(NotSupportedError, "video profile format not supported");
        case VK_ERROR_VIDEO_PROFILE_OPERATION_NOT_SUPPORTED_KHR:
            TCU_THROW(NotSupportedError, "video profile operation not supported");
        case VK_ERROR_VIDEO_PROFILE_CODEC_NOT_SUPPORTED_KHR:
            TCU_THROW(NotSupportedError, "video profile codec not supported");
        default:
        {
            ostringstream failMsg;
            failMsg << "Failed query call to vkGetPhysicalDeviceVideoCapabilitiesKHR with " << result;
            return tcu::TestStatus::fail(failMsg.str());
        }
        }
    }

    return validateVideoFormatsWithProfile();
}

class VideoProfilesValidationTestCase : public TestCase
{
public:
    VideoProfilesValidationTestCase(tcu::TestContext &context, const char *name, const SharedTestParams &params);
    ~VideoProfilesValidationTestCase(void);

    virtual TestInstance *createInstance(Context &context) const;
    virtual void checkSupport(Context &context) const;

private:
    SharedTestParams m_params;
};

VideoProfilesValidationTestCase::VideoProfilesValidationTestCase(tcu::TestContext &context, const char *name,
                                                                 const SharedTestParams &params)
    : vkt::TestCase(context, name)
    , m_params(params)
{
}

VideoProfilesValidationTestCase::~VideoProfilesValidationTestCase(void)
{
}

void VideoProfilesValidationTestCase::checkSupport(Context &context) const
{
    context.requireDeviceFunctionality("VK_KHR_video_queue");

    if (context.isDeviceFunctionalitySupported("VK_KHR_video_maintenance1"))
        context.requireDeviceFunctionality("VK_KHR_video_maintenance1");

    switch (m_params->profile.videoCodecOperation)
    {
    case VK_VIDEO_CODEC_OPERATION_DECODE_H264_BIT_KHR:
        context.requireDeviceFunctionality("VK_KHR_video_decode_queue");
        context.requireDeviceFunctionality("VK_KHR_video_decode_h264");
        break;
    case VK_VIDEO_CODEC_OPERATION_DECODE_H265_BIT_KHR:
        context.requireDeviceFunctionality("VK_KHR_video_decode_queue");
        context.requireDeviceFunctionality("VK_KHR_video_decode_h265");
        break;
    case VK_VIDEO_CODEC_OPERATION_DECODE_AV1_BIT_KHR:
        context.requireDeviceFunctionality("VK_KHR_video_decode_queue");
        context.requireDeviceFunctionality("VK_KHR_video_decode_av1");
        break;
    case VK_VIDEO_CODEC_OPERATION_DECODE_VP9_BIT_KHR:
        context.requireDeviceFunctionality("VK_KHR_video_decode_queue");
        context.requireDeviceFunctionality("VK_KHR_video_decode_vp9");
        break;
    case VK_VIDEO_CODEC_OPERATION_ENCODE_H264_BIT_KHR:
        context.requireDeviceFunctionality("VK_KHR_video_encode_queue");
        context.requireDeviceFunctionality("VK_KHR_video_encode_h264");
        break;
    case VK_VIDEO_CODEC_OPERATION_ENCODE_H265_BIT_KHR:
        context.requireDeviceFunctionality("VK_KHR_video_encode_queue");
        context.requireDeviceFunctionality("VK_KHR_video_encode_h265");
        break;
    case VK_VIDEO_CODEC_OPERATION_ENCODE_AV1_BIT_KHR:
        context.requireDeviceFunctionality("VK_KHR_video_encode_queue");
        context.requireDeviceFunctionality("VK_KHR_video_encode_av1");
        break;
    default:
        TCU_THROW(NotSupportedError, "Unknown TestType");
    }
}

TestInstance *VideoProfilesValidationTestCase::createInstance(Context &context) const
{
    switch (m_params->profile.videoCodecOperation)
    {
    case VK_VIDEO_CODEC_OPERATION_DECODE_H264_BIT_KHR:
    case VK_VIDEO_CODEC_OPERATION_DECODE_H265_BIT_KHR:
    case VK_VIDEO_CODEC_OPERATION_DECODE_AV1_BIT_KHR:
    case VK_VIDEO_CODEC_OPERATION_DECODE_VP9_BIT_KHR:
    case VK_VIDEO_CODEC_OPERATION_ENCODE_H264_BIT_KHR:
    case VK_VIDEO_CODEC_OPERATION_ENCODE_H265_BIT_KHR:
    case VK_VIDEO_CODEC_OPERATION_ENCODE_AV1_BIT_KHR:
        return new VideoProfilesValidationTestInstance(context, m_params);
    default:
        TCU_THROW(NotSupportedError, "Unknown TestType");
    }
}

static std::string getH264ProfileTestName(const TestParams &params, bool encode)
{
    std::stringstream ss;
    StdVideoH264ProfileIdc profileIdc =
        encode ? params.codecProfile.h264Enc.stdProfileIdc : params.codecProfile.h264Dec.stdProfileIdc;

    switch (profileIdc)
    {
    case STD_VIDEO_H264_PROFILE_IDC_BASELINE:
        ss << "baseline";
        break;
    case STD_VIDEO_H264_PROFILE_IDC_MAIN:
        ss << "main";
        break;
    case STD_VIDEO_H264_PROFILE_IDC_HIGH:
        ss << "high";
        break;
    case STD_VIDEO_H264_PROFILE_IDC_HIGH_444_PREDICTIVE:
        ss << "high_444_predictive";
        break;
    default:
        TCU_THROW(NotSupportedError, "Unknown H264 Profile");
    }

    if (!encode)
    {
        switch (params.codecProfile.h264Dec.pictureLayout)
        {
        case VK_VIDEO_DECODE_H264_PICTURE_LAYOUT_PROGRESSIVE_KHR:
            ss << "_progressive";
            break;
        case VK_VIDEO_DECODE_H264_PICTURE_LAYOUT_INTERLACED_INTERLEAVED_LINES_BIT_KHR:
            ss << "_interlaced_interleaved_lines";
            break;
        case VK_VIDEO_DECODE_H264_PICTURE_LAYOUT_INTERLACED_SEPARATE_PLANES_BIT_KHR:
            ss << "_interlaced_separate_planes";
            break;
        default:
            TCU_THROW(NotSupportedError, "Unknown H264 Picture Layout");
        }
    }

    return ss.str();
}

static std::string getH265ProfileTestName(const TestParams &params, bool encode)
{
    std::stringstream ss;
    StdVideoH265ProfileIdc profileIdc =
        encode ? params.codecProfile.h265Enc.stdProfileIdc : params.codecProfile.h265Dec.stdProfileIdc;

    switch (profileIdc)
    {
    case STD_VIDEO_H265_PROFILE_IDC_MAIN:
        ss << "main";
        break;
    case STD_VIDEO_H265_PROFILE_IDC_MAIN_10:
        ss << "main_10";
        break;
    case STD_VIDEO_H265_PROFILE_IDC_MAIN_STILL_PICTURE:
        ss << "main_still_pic";
        break;
    case STD_VIDEO_H265_PROFILE_IDC_FORMAT_RANGE_EXTENSIONS:
        ss << "format_range_ext";
        break;
    case STD_VIDEO_H265_PROFILE_IDC_SCC_EXTENSIONS:
        ss << "scc_ext";
        break;
    default:
        TCU_THROW(NotSupportedError, "Unknown H265 Profile");
    }

    return ss.str();
}

static std::string getAV1ProfileTestName(const TestParams &params, bool encode)
{
    std::stringstream ss;
    StdVideoAV1Profile profile = encode ? params.codecProfile.av1Enc.stdProfile : params.codecProfile.av1Dec.stdProfile;

    switch (profile)
    {
    case STD_VIDEO_AV1_PROFILE_MAIN:
        ss << "main";
        break;
    case STD_VIDEO_AV1_PROFILE_HIGH:
        ss << "high";
        break;
    case STD_VIDEO_AV1_PROFILE_PROFESSIONAL:
        ss << "professional";
        break;
    default:
        TCU_THROW(NotSupportedError, "Unknown AV1 Profile");
    }

    if (!encode)
    {
        if (params.codecProfile.av1Dec.filmGrainSupport)
            ss << "_with_filmgrain";
        else
            ss << "_without_filmgrain";
    }

    return ss.str();
}

static std::string getVP9ProfileTestName(const TestParams &params)
{
    std::stringstream ss;
    StdVideoVP9Profile profile = params.codecProfile.vp9Dec.stdProfile;

    switch (profile)
    {
    case STD_VIDEO_VP9_PROFILE_0:
        ss << "profile0";
        break;
    case STD_VIDEO_VP9_PROFILE_1:
        ss << "profile1";
        break;
    case STD_VIDEO_VP9_PROFILE_2:
        ss << "profile2";
        break;
    case STD_VIDEO_VP9_PROFILE_3:
        ss << "profile3";
        break;
    default:
        TCU_THROW(NotSupportedError, "Unknown VP9 Profile");
    }

    return ss.str();
}

std::string getTestName(const TestParams &params)
{
    std::stringstream ss;
    switch (params.profile.videoCodecOperation)
    {
    case VK_VIDEO_CODEC_OPERATION_DECODE_H264_BIT_KHR:
        ss << getH264ProfileTestName(params, false);
        break;
    case VK_VIDEO_CODEC_OPERATION_DECODE_H265_BIT_KHR:
        ss << getH265ProfileTestName(params, false);
        break;
    case VK_VIDEO_CODEC_OPERATION_DECODE_AV1_BIT_KHR:
        ss << getAV1ProfileTestName(params, false);
        break;
    case VK_VIDEO_CODEC_OPERATION_DECODE_VP9_BIT_KHR:
        ss << getVP9ProfileTestName(params);
        break;
    case VK_VIDEO_CODEC_OPERATION_ENCODE_H264_BIT_KHR:
        ss << getH264ProfileTestName(params, true);
        break;
    case VK_VIDEO_CODEC_OPERATION_ENCODE_H265_BIT_KHR:
        ss << getH265ProfileTestName(params, true);
        break;
    case VK_VIDEO_CODEC_OPERATION_ENCODE_AV1_BIT_KHR:
        ss << getAV1ProfileTestName(params, true);
        break;
    default:
        TCU_THROW(InternalError, "unsupported codec");
    }

    switch (params.profile.chromaSubsampling)
    {
    case VK_VIDEO_CHROMA_SUBSAMPLING_MONOCHROME_BIT_KHR:
        ss << "_monochrome";
        break;
    case VK_VIDEO_CHROMA_SUBSAMPLING_420_BIT_KHR:
        ss << "_420";
        break;
    case VK_VIDEO_CHROMA_SUBSAMPLING_422_BIT_KHR:
        ss << "_422";
        break;
    case VK_VIDEO_CHROMA_SUBSAMPLING_444_BIT_KHR:
        ss << "_444";
        break;
    default:
        TCU_THROW(InternalError, "invalid subsampling");
    }

    // Not strictly required, but used to reduce the amount of combinations tested.
    //DE_ASSERT(params.profile.lumaBitDepth == params.profile.chromaBitDepth);

    switch (params.profile.lumaBitDepth)
    {
    case VK_VIDEO_COMPONENT_BIT_DEPTH_8_BIT_KHR:
        ss << "_luma_8bit";
        break;
    case VK_VIDEO_COMPONENT_BIT_DEPTH_10_BIT_KHR:
        ss << "_luma_10bit";
        break;
    case VK_VIDEO_COMPONENT_BIT_DEPTH_12_BIT_KHR:
        ss << "_luma_12bit";
        break;
    default:
        TCU_THROW(InternalError, "invalid bitdepth");
    }

    switch (params.profile.chromaBitDepth)
    {
    case VK_VIDEO_COMPONENT_BIT_DEPTH_8_BIT_KHR:
        ss << "_chroma_8bit";
        break;
    case VK_VIDEO_COMPONENT_BIT_DEPTH_10_BIT_KHR:
        ss << "_chroma_10bit";
        break;
    case VK_VIDEO_COMPONENT_BIT_DEPTH_12_BIT_KHR:
        ss << "_chroma_12bit";
        break;
    default:
        TCU_THROW(InternalError, "invalid bitdepth");
    }

    return de::toLower(ss.str());
}

} // namespace

static void setProfiles(SharedTestParams &params, VkVideoCodecOperationFlagBitsKHR codec,
                        VkVideoChromaSubsamplingFlagsKHR subsampling, VkVideoComponentBitDepthFlagsKHR lumaBitDepth,
                        VkVideoComponentBitDepthFlagsKHR chromaBitDepth)
{
    params->profile.videoCodecOperation = codec;
    params->profile.chromaSubsampling   = subsampling;
    params->profile.lumaBitDepth        = lumaBitDepth;
    params->profile.chromaBitDepth      = chromaBitDepth;
}

tcu::TestCaseGroup *createVideoProfilesValidationTests(tcu::TestContext &testCtx)
{
    de::MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(testCtx, "profiles"));

    std::vector<VkVideoCodecOperationFlagBitsKHR> codecs = {
        VK_VIDEO_CODEC_OPERATION_DECODE_H264_BIT_KHR, VK_VIDEO_CODEC_OPERATION_DECODE_H265_BIT_KHR,
        VK_VIDEO_CODEC_OPERATION_DECODE_AV1_BIT_KHR,  VK_VIDEO_CODEC_OPERATION_DECODE_VP9_BIT_KHR,
        VK_VIDEO_CODEC_OPERATION_ENCODE_H264_BIT_KHR, VK_VIDEO_CODEC_OPERATION_ENCODE_H265_BIT_KHR,
        VK_VIDEO_CODEC_OPERATION_ENCODE_AV1_BIT_KHR};

    std::vector<VkImageUsageFlagBits> usageFlags = {
        VK_IMAGE_USAGE_VIDEO_DECODE_DST_BIT_KHR, VK_IMAGE_USAGE_VIDEO_DECODE_DPB_BIT_KHR,
        VK_IMAGE_USAGE_VIDEO_ENCODE_SRC_BIT_KHR, VK_IMAGE_USAGE_VIDEO_ENCODE_DPB_BIT_KHR};

    std::vector<VkVideoChromaSubsamplingFlagsKHR> subsamplingFlags = {
        VK_VIDEO_CHROMA_SUBSAMPLING_MONOCHROME_BIT_KHR,
        VK_VIDEO_CHROMA_SUBSAMPLING_420_BIT_KHR,
        VK_VIDEO_CHROMA_SUBSAMPLING_422_BIT_KHR,
        VK_VIDEO_CHROMA_SUBSAMPLING_444_BIT_KHR,
    };

    std::vector<VkVideoDecodeH264PictureLayoutFlagBitsKHR> h264PicLayouts = {
        VK_VIDEO_DECODE_H264_PICTURE_LAYOUT_PROGRESSIVE_KHR,
        VK_VIDEO_DECODE_H264_PICTURE_LAYOUT_INTERLACED_INTERLEAVED_LINES_BIT_KHR,
        VK_VIDEO_DECODE_H264_PICTURE_LAYOUT_INTERLACED_SEPARATE_PLANES_BIT_KHR,
    };

    std::vector<StdVideoH264ProfileIdc> h264ProfileIdc = {
        STD_VIDEO_H264_PROFILE_IDC_BASELINE,
        STD_VIDEO_H264_PROFILE_IDC_MAIN,
        STD_VIDEO_H264_PROFILE_IDC_HIGH,
        STD_VIDEO_H264_PROFILE_IDC_HIGH_444_PREDICTIVE,
    };

    std::vector<StdVideoH265ProfileIdc> h265ProfileIdc = {
        STD_VIDEO_H265_PROFILE_IDC_MAIN,
        STD_VIDEO_H265_PROFILE_IDC_MAIN_10,
        STD_VIDEO_H265_PROFILE_IDC_MAIN_STILL_PICTURE,
        STD_VIDEO_H265_PROFILE_IDC_FORMAT_RANGE_EXTENSIONS,
        STD_VIDEO_H265_PROFILE_IDC_SCC_EXTENSIONS,
    };

    std::vector<StdVideoAV1Profile> av1Profile = {
        STD_VIDEO_AV1_PROFILE_MAIN,
        STD_VIDEO_AV1_PROFILE_HIGH,
        STD_VIDEO_AV1_PROFILE_PROFESSIONAL,
    };

    std::vector<StdVideoVP9Profile> vp9Profile = {
        STD_VIDEO_VP9_PROFILE_0,
        STD_VIDEO_VP9_PROFILE_1,
        STD_VIDEO_VP9_PROFILE_2,
        STD_VIDEO_VP9_PROFILE_3,
    };

    std::vector<bool> filmGrainSupport = {false, true};

    std::vector<VkVideoComponentBitDepthFlagsKHR> bitdepthFlags = {VK_VIDEO_COMPONENT_BIT_DEPTH_8_BIT_KHR,
                                                                   VK_VIDEO_COMPONENT_BIT_DEPTH_10_BIT_KHR,
                                                                   VK_VIDEO_COMPONENT_BIT_DEPTH_12_BIT_KHR};

    de::MovePtr<tcu::TestCaseGroup> decGroup(new tcu::TestCaseGroup(testCtx, "decode"));
    de::MovePtr<tcu::TestCaseGroup> encGroup(new tcu::TestCaseGroup(testCtx, "encode"));
    de::MovePtr<tcu::TestCaseGroup> h264DecGroup(new tcu::TestCaseGroup(testCtx, "h264", "H.264 video decoder"));
    de::MovePtr<tcu::TestCaseGroup> h265DecGroup(new tcu::TestCaseGroup(testCtx, "h265", "H.265 video decoder"));
    de::MovePtr<tcu::TestCaseGroup> av1DecGroup(new tcu::TestCaseGroup(testCtx, "av1", "AV1 video decoder"));
    de::MovePtr<tcu::TestCaseGroup> vp9DecGroup(new tcu::TestCaseGroup(testCtx, "vp9", "VP9 video decoder"));
    de::MovePtr<tcu::TestCaseGroup> h264EncGroup(new tcu::TestCaseGroup(testCtx, "h264", "H.264 video encoder"));
    de::MovePtr<tcu::TestCaseGroup> h265EncGroup(new tcu::TestCaseGroup(testCtx, "h265", "H.265 video encoder"));
    de::MovePtr<tcu::TestCaseGroup> av1EncGroup(new tcu::TestCaseGroup(testCtx, "av1", "AV1 video encoder"));

    for (VkVideoCodecOperationFlagBitsKHR codec : codecs)
        for (VkVideoChromaSubsamplingFlagsKHR subsampling : subsamplingFlags)
            for (VkVideoComponentBitDepthFlagsKHR lumaBitdepth : bitdepthFlags)
                for (VkVideoComponentBitDepthFlagsKHR chromaBitdepth : bitdepthFlags)
                {
                    /* Just ignore the chroma bit depth for monochrome */
                    if (subsampling == VK_VIDEO_CHROMA_SUBSAMPLING_MONOCHROME_BIT_KHR && lumaBitdepth != chromaBitdepth)
                        continue;

                    switch (codec)
                    {
                    case VK_VIDEO_CODEC_OPERATION_DECODE_H264_BIT_KHR:
                        for (VkVideoDecodeH264PictureLayoutFlagBitsKHR picLayout : h264PicLayouts)
                            for (StdVideoH264ProfileIdc profile : h264ProfileIdc)
                            {
                                SharedTestParams params(new TestParams());
                                params->codecProfile.h264Dec               = initVulkanStructure();
                                params->codecProfile.h264Dec.pictureLayout = picLayout;
                                params->codecProfile.h264Dec.stdProfileIdc = profile;
                                params->profile           = initVulkanStructure(&params->codecProfile.h264Dec);
                                params->codecCaps.h264Dec = initVulkanStructure();
                                params->decodeCaps        = initVulkanStructure(&params->codecCaps.h264Dec);
                                params->videoCaps         = initVulkanStructure(&params->decodeCaps);

                                setProfiles(params, codec, subsampling, lumaBitdepth, chromaBitdepth);
                                std::string testName = getTestName(*params);

                                h264DecGroup->addChild(
                                    new VideoProfilesValidationTestCase(testCtx, testName.c_str(), params));
                            }
                        break;
                    case VK_VIDEO_CODEC_OPERATION_DECODE_H265_BIT_KHR:
                        for (StdVideoH265ProfileIdc profile : h265ProfileIdc)
                        {
                            SharedTestParams params(new TestParams());
                            params->codecProfile.h265Dec               = initVulkanStructure();
                            params->codecProfile.h265Dec.stdProfileIdc = profile;
                            params->profile           = initVulkanStructure(&params->codecProfile.h265Dec);
                            params->codecCaps.h265Dec = initVulkanStructure();
                            params->decodeCaps        = initVulkanStructure(&params->codecCaps.h265Dec);
                            params->videoCaps         = initVulkanStructure(&params->decodeCaps);

                            setProfiles(params, codec, subsampling, lumaBitdepth, chromaBitdepth);
                            std::string testName = getTestName(*params);

                            h265DecGroup->addChild(
                                new VideoProfilesValidationTestCase(testCtx, testName.c_str(), params));
                        }
                        break;
                    case VK_VIDEO_CODEC_OPERATION_DECODE_AV1_BIT_KHR:
                        for (bool fg : filmGrainSupport)
                            for (StdVideoAV1Profile profile : av1Profile)
                            {
                                SharedTestParams params(new TestParams());
                                params->codecProfile.av1Dec                  = initVulkanStructure();
                                params->codecProfile.av1Dec.stdProfile       = profile;
                                params->codecProfile.av1Dec.filmGrainSupport = fg;
                                params->profile          = initVulkanStructure(&params->codecProfile.av1Dec);
                                params->codecCaps.av1Dec = initVulkanStructure();
                                params->decodeCaps       = initVulkanStructure(&params->codecCaps.av1Dec);
                                params->videoCaps        = initVulkanStructure(&params->decodeCaps);

                                setProfiles(params, codec, subsampling, lumaBitdepth, chromaBitdepth);
                                std::string testName = getTestName(*params);

                                av1DecGroup->addChild(
                                    new VideoProfilesValidationTestCase(testCtx, testName.c_str(), params));
                            }
                        break;
                    case VK_VIDEO_CODEC_OPERATION_DECODE_VP9_BIT_KHR:
                        for (StdVideoVP9Profile profile : vp9Profile)
                        {
                            SharedTestParams params(new TestParams());
                            params->codecProfile.vp9Dec            = initVulkanStructure();
                            params->codecProfile.vp9Dec.stdProfile = profile;
                            params->profile                        = initVulkanStructure(&params->codecProfile.vp9Dec);
                            params->codecCaps.vp9Dec               = initVulkanStructure();
                            params->decodeCaps                     = initVulkanStructure(&params->codecCaps.vp9Dec);
                            params->videoCaps                      = initVulkanStructure(&params->decodeCaps);

                            setProfiles(params, codec, subsampling, lumaBitdepth, chromaBitdepth);
                            std::string testName = getTestName(*params);

                            vp9DecGroup->addChild(
                                new VideoProfilesValidationTestCase(testCtx, testName.c_str(), params));
                        }
                        break;
                    case VK_VIDEO_CODEC_OPERATION_ENCODE_H264_BIT_KHR:
                        for (StdVideoH264ProfileIdc profile : h264ProfileIdc)
                        {
                            SharedTestParams params(new TestParams());
                            params->codecProfile.h264Enc               = initVulkanStructure();
                            params->codecProfile.h264Enc.stdProfileIdc = profile;
                            params->profile           = initVulkanStructure(&params->codecProfile.h264Enc);
                            params->codecCaps.h264Enc = initVulkanStructure();
                            params->encodeCaps        = initVulkanStructure(&params->codecCaps.h264Enc);
                            params->videoCaps         = initVulkanStructure(&params->encodeCaps);

                            setProfiles(params, codec, subsampling, lumaBitdepth, chromaBitdepth);
                            std::string testName = getTestName(*params);

                            h264EncGroup->addChild(
                                new VideoProfilesValidationTestCase(testCtx, testName.c_str(), params));
                        }
                        break;
                    case VK_VIDEO_CODEC_OPERATION_ENCODE_H265_BIT_KHR:
                        for (StdVideoH265ProfileIdc profile : h265ProfileIdc)
                        {
                            SharedTestParams params(new TestParams());
                            params->codecProfile.h265Enc               = initVulkanStructure();
                            params->codecProfile.h265Enc.stdProfileIdc = profile;
                            params->profile           = initVulkanStructure(&params->codecProfile.h265Enc);
                            params->codecCaps.h265Enc = initVulkanStructure();
                            params->encodeCaps        = initVulkanStructure(&params->codecCaps.h265Enc);
                            params->videoCaps         = initVulkanStructure(&params->encodeCaps);

                            setProfiles(params, codec, subsampling, lumaBitdepth, chromaBitdepth);
                            std::string testName = getTestName(*params);

                            h265EncGroup->addChild(
                                new VideoProfilesValidationTestCase(testCtx, testName.c_str(), params));
                        }
                        break;
                    case VK_VIDEO_CODEC_OPERATION_ENCODE_AV1_BIT_KHR:
                        for (StdVideoAV1Profile profile : av1Profile)
                        {
                            SharedTestParams params(new TestParams());
                            params->codecProfile.av1Enc            = initVulkanStructure();
                            params->codecProfile.av1Enc.stdProfile = profile;
                            params->profile                        = initVulkanStructure(&params->codecProfile.av1Enc);
                            params->codecCaps.av1Enc               = initVulkanStructure();
                            params->encodeCaps                     = initVulkanStructure(&params->codecCaps.av1Enc);
                            params->videoCaps                      = initVulkanStructure(&params->encodeCaps);

                            setProfiles(params, codec, subsampling, lumaBitdepth, chromaBitdepth);
                            std::string testName = getTestName(*params);

                            av1EncGroup->addChild(
                                new VideoProfilesValidationTestCase(testCtx, testName.c_str(), params));
                        }
                        break;
                    default:
                        TCU_THROW(InternalError, "unsupported codec");
                    }
                }

    decGroup->addChild(h264DecGroup.release());
    encGroup->addChild(h264EncGroup.release());
    decGroup->addChild(h265DecGroup.release());
    encGroup->addChild(h265EncGroup.release());
    decGroup->addChild(av1DecGroup.release());
    encGroup->addChild(av1EncGroup.release());
    decGroup->addChild(vp9DecGroup.release());

    group->addChild(decGroup.release());
    group->addChild(encGroup.release());

    return group.release();
}

} // namespace video
} // namespace vkt
