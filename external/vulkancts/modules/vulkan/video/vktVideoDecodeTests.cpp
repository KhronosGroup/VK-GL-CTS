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
 *	  http://www.apache.org/licenses/LICENSE-2.0
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

#include "vktVideoDecodeTests.hpp"
#include "vktVideoTestUtils.hpp"
#include "vkBarrierUtil.hpp"
#include "vkObjUtil.hpp"

#include "tcuFunctionLibrary.hpp"
#include "tcuPlatform.hpp"
#include "tcuTestLog.hpp"

#include "vkCmdUtil.hpp"
#include "vkDefs.hpp"
#include "vkImageWithMemory.hpp"
#include "tcuCommandLine.hpp"

#include "vktVideoClipInfo.hpp"

#include <deDefs.h>

#ifdef DE_BUILD_VIDEO
#include "extESExtractor.hpp"
#include "vktVideoBaseDecodeUtils.hpp"

#include "extNvidiaVideoParserIf.hpp"
// FIXME: The samples repo is missing this internal include from their H265 decoder
#include "nvVulkanh265ScalingList.h"
#include <VulkanH264Decoder.h>
#include <VulkanH265Decoder.h>

#include <utility>
#endif

namespace vkt
{
namespace video
{

// Set this to 1 to have the decoded YCbCr frames written to the
// filesystem in the YV12 format.
// Check the relevant sections to change the file name and so on...
#define FRAME_DUMP_DEBUG 0

namespace
{
using namespace vk;
using namespace std;

using de::MovePtr;

enum TestType
{
	TEST_TYPE_H264_DECODE_I, // Case 6
	TEST_TYPE_H264_DECODE_I_P, // Case 7
	TEST_TYPE_H264_DECODE_CLIP_A,
	TEST_TYPE_H264_DECODE_I_P_B_13, // Case 7a
	TEST_TYPE_H264_DECODE_I_P_NOT_MATCHING_ORDER, // Case 8
	TEST_TYPE_H264_DECODE_I_P_B_13_NOT_MATCHING_ORDER, // Case 8a
	TEST_TYPE_H264_DECODE_QUERY_RESULT_WITH_STATUS, // Case 9
	TEST_TYPE_H264_DECODE_RESOLUTION_CHANGE, // Case 17
	TEST_TYPE_H264_DECODE_RESOLUTION_CHANGE_DPB, // Case 18
	TEST_TYPE_H264_DECODE_INTERLEAVED, // Case 21
	TEST_TYPE_H264_BOTH_DECODE_ENCODE_INTERLEAVED, // Case 23 TODO
	TEST_TYPE_H264_H265_DECODE_INTERLEAVED, // Case 24

	TEST_TYPE_H265_DECODE_I, // Case 15
	TEST_TYPE_H265_DECODE_I_P, // Case 16
	TEST_TYPE_H265_DECODE_CLIP_D,
	TEST_TYPE_H265_DECODE_I_P_NOT_MATCHING_ORDER, // Case 16-2
	TEST_TYPE_H265_DECODE_I_P_B_13, // Case 16-3
	TEST_TYPE_H265_DECODE_I_P_B_13_NOT_MATCHING_ORDER, // Case 16-4

	TEST_TYPE_LAST
};

const char* getTestName(TestType type)
{
	const char* testName;
	switch (type)
	{
		case TEST_TYPE_H264_DECODE_I:
			testName = "h264_i";
			break;
		case TEST_TYPE_H264_DECODE_I_P:
			testName = "h264_i_p";
			break;
		case TEST_TYPE_H264_DECODE_CLIP_A:
			testName = "h264_420_8bit_high_176x144_30frames";
			break;
		case TEST_TYPE_H264_DECODE_I_P_NOT_MATCHING_ORDER:
			testName = "h264_i_p_not_matching_order";
			break;
		case TEST_TYPE_H264_DECODE_I_P_B_13:
			testName = "h264_i_p_b_13";
			break;
		case TEST_TYPE_H264_DECODE_I_P_B_13_NOT_MATCHING_ORDER:
			testName = "h264_i_p_b_13_not_matching_order";
			break;
		case TEST_TYPE_H264_DECODE_QUERY_RESULT_WITH_STATUS:
			testName = "h264_query_with_status";
			break;
		case TEST_TYPE_H264_DECODE_RESOLUTION_CHANGE:
			testName = "h264_resolution_change";
			break;
		case TEST_TYPE_H264_DECODE_RESOLUTION_CHANGE_DPB:
			testName = "h264_resolution_change_dpb";
			break;
		case TEST_TYPE_H264_DECODE_INTERLEAVED:
			testName = "h264_interleaved";
			break;
		case TEST_TYPE_H264_H265_DECODE_INTERLEAVED:
			testName = "h264_h265_interleaved";
			break;
		case TEST_TYPE_H265_DECODE_I:
			testName = "h265_i";
			break;
		case TEST_TYPE_H265_DECODE_I_P:
			testName = "h265_i_p";
			break;
		case TEST_TYPE_H265_DECODE_CLIP_D:
			testName = "h265_420_8bit_main_176x144_30frames";
			break;
		case TEST_TYPE_H265_DECODE_I_P_NOT_MATCHING_ORDER:
			testName = "h265_i_p_not_matching_order";
			break;
		case TEST_TYPE_H265_DECODE_I_P_B_13:
			testName = "h265_i_p_b_13";
			break;
		case TEST_TYPE_H265_DECODE_I_P_B_13_NOT_MATCHING_ORDER:
			testName = "h265_i_p_b_13_not_matching_order";
			break;
		default:
			TCU_THROW(InternalError, "Unknown TestType");
	}
	return testName;
}


enum DecoderOption : deUint32
{
	// The default is to do nothing additional to ordinary playback.
	Default			  = 0,
	// All decode operations will have their status checked for success (Q2 2023: not all vendors support these)
	UseStatusQueries  = 1 << 0,
	// Do not playback the clip in the "normal fashion", instead cached decode parameters for later process
	// this is primarily used to support out-of-order submission test cases, and per-GOP handling.
	CachedDecoding	  = 1 << 1,
	// When a parameter object changes the resolution of the test content, and the new video session would otherwise
	// still be compatible with the last session (for example, larger decode surfaces preceeding smaller decode surfaces,
	// a frame downsize), force the session to be recreated anyway.
	RecreateDPBImages = 1 << 2,
};
static const int ALL_FRAMES = 0;

struct BaseDecodeParam
{
	ClipName	  clip;
	int			  framesToCheck;
	DecoderOption decoderOptions;
};

struct DecodeTestParam
{
	TestType		type;
	BaseDecodeParam stream;

} g_DecodeTests[] = {
	{TEST_TYPE_H264_DECODE_I, {CLIP_A, 1, DecoderOption::Default}},
	{TEST_TYPE_H264_DECODE_I_P, {CLIP_A, 2, DecoderOption::Default}},
	{TEST_TYPE_H264_DECODE_I_P_NOT_MATCHING_ORDER, {CLIP_A, 2, DecoderOption::CachedDecoding}},
	{TEST_TYPE_H264_DECODE_CLIP_A, {CLIP_A, ALL_FRAMES, DecoderOption::Default}},
	{TEST_TYPE_H264_DECODE_I_P_B_13, {CLIP_H264_4K_26_IBP_MAIN, ALL_FRAMES, DecoderOption::Default}},
	{TEST_TYPE_H264_DECODE_I_P_B_13_NOT_MATCHING_ORDER, {CLIP_H264_4K_26_IBP_MAIN, ALL_FRAMES, DecoderOption::CachedDecoding}},
	{TEST_TYPE_H264_DECODE_QUERY_RESULT_WITH_STATUS, {CLIP_A, ALL_FRAMES, DecoderOption::UseStatusQueries}},
	{TEST_TYPE_H264_DECODE_RESOLUTION_CHANGE, {CLIP_C, ALL_FRAMES, DecoderOption::Default}},
	{TEST_TYPE_H264_DECODE_RESOLUTION_CHANGE_DPB, {CLIP_C, ALL_FRAMES, DecoderOption::RecreateDPBImages}},

	{TEST_TYPE_H265_DECODE_I, {CLIP_D, 1, DecoderOption::Default}},
	{TEST_TYPE_H265_DECODE_I_P, {CLIP_D, 2, DecoderOption::Default}},
	{TEST_TYPE_H265_DECODE_I_P_NOT_MATCHING_ORDER, {CLIP_D, 2, DecoderOption::CachedDecoding}},
	{TEST_TYPE_H265_DECODE_I_P_B_13, {CLIP_JELLY_HEVC, ALL_FRAMES, DecoderOption::Default}},
	{TEST_TYPE_H265_DECODE_I_P_B_13_NOT_MATCHING_ORDER, {CLIP_JELLY_HEVC, ALL_FRAMES, DecoderOption::CachedDecoding}},
	{TEST_TYPE_H265_DECODE_CLIP_D, {CLIP_D, ALL_FRAMES, DecoderOption::Default}},
};

struct InterleavingDecodeTestParams
{
	TestType	type;
	BaseDecodeParam streamA;
	BaseDecodeParam streamB;
} g_InterleavingTests[] = {
	{TEST_TYPE_H264_DECODE_INTERLEAVED, {CLIP_A, ALL_FRAMES, DecoderOption::CachedDecoding}, {CLIP_A, ALL_FRAMES, DecoderOption::CachedDecoding}},
	{TEST_TYPE_H264_H265_DECODE_INTERLEAVED, {CLIP_A, ALL_FRAMES, DecoderOption::CachedDecoding}, {CLIP_D, ALL_FRAMES, DecoderOption::CachedDecoding}},
};

class TestDefinition
{
public:
	static MovePtr<TestDefinition> create(DecodeTestParam params, deUint32 baseSeed)
	{
		return MovePtr<TestDefinition>(new TestDefinition(params, baseSeed));
	}

	TestDefinition(DecodeTestParam params, deUint32 baseSeed)
		: m_params(params)
		, m_info(clipInfo(params.stream.clip))
		, m_hash(baseSeed)
	{
		m_profile = VkVideoCoreProfile(m_info->profile.codecOperation, m_info->profile.subsamplingFlags, m_info->profile.lumaBitDepth, m_info->profile.chromaBitDepth, m_info->profile.profileIDC);
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

	const char* getClipFilename() const
	{
		return m_info->filename;
	}

	const ClipInfo* getClipInfo() const
	{
		return m_info;
	};

	VkVideoCodecOperationFlagBitsKHR getCodecOperation() const
	{
		return m_profile.GetCodecType();
	}
	const VkVideoCoreProfile* getProfile() const
	{
		return &m_profile;
	}

	int framesToCheck() const
	{
		return m_params.stream.framesToCheck;
	}

	bool hasOption(DecoderOption o) const
	{
		return (m_params.stream.decoderOptions & o) != 0;
	}

	int getParamaterUpdateHackRequirement() const
	{
		return m_pictureParameterUpdateTriggerHack;
	}

	VideoDevice::VideoDeviceFlags requiredDeviceFlags() const
	{
		return VideoDevice::VIDEO_DEVICE_FLAG_REQUIRE_SYNC2_OR_NOT_SUPPORTED |
			   (hasOption(DecoderOption::UseStatusQueries) ? VideoDevice::VIDEO_DEVICE_FLAG_QUERY_WITH_STATUS_FOR_DECODE_SUPPORT : VideoDevice::VIDEO_DEVICE_FLAG_NONE);
	}

	const VkExtensionProperties* extensionProperties() const
	{
		static const VkExtensionProperties h264StdExtensionVersion = {
			VK_STD_VULKAN_VIDEO_CODEC_H264_DECODE_EXTENSION_NAME, VK_STD_VULKAN_VIDEO_CODEC_H264_DECODE_SPEC_VERSION};
		static const VkExtensionProperties h265StdExtensionVersion = {
			VK_STD_VULKAN_VIDEO_CODEC_H265_DECODE_EXTENSION_NAME, VK_STD_VULKAN_VIDEO_CODEC_H265_DECODE_SPEC_VERSION};

		switch (m_profile.GetCodecType())
		{
			case VK_VIDEO_CODEC_OPERATION_DECODE_H264_BIT_KHR:
				return &h264StdExtensionVersion;
			case VK_VIDEO_CODEC_OPERATION_DECODE_H265_BIT_KHR:
				return &h265StdExtensionVersion;
			default:
				tcu::die("Unsupported video codec %s\n", util::codecToName(m_profile.GetCodecType()));
				break;
		}

		TCU_THROW(InternalError, "Unsupported codec");
	};

	void updateHash(deUint32 baseHash)
	{
		m_hash = deUint32Hash(baseHash);
	}

private:
	DecodeTestParam	   m_params;
	const ClipInfo*	   m_info{};
	deUint32		   m_hash{};
	VkVideoCoreProfile m_profile;
	// The 1-based count of parameter set updates after which to force a parameter object release.
	// This is required due to the design of the NVIDIA decode-client API. It sends parameter updates and expects constructed parameter
	// objects back synchronously, before the next video session is created in a following BeginSequence call.
	int				   m_pictureParameterUpdateTriggerHack{0}; // Zero is "off"
};


// Vulkan video is not supported on android platform
// all external libraries, helper functions and test instances has been excluded
#ifdef DE_BUILD_VIDEO
using VkVideoParser = VkSharedBaseObj<VulkanVideoDecodeParser>;

void createParser(const TestDefinition* params, VideoBaseDecoder* decoder, VkSharedBaseObj<VulkanVideoDecodeParser>& parser)
{
	VkVideoCapabilitiesKHR		 videoCaps{};
	VkVideoDecodeCapabilitiesKHR videoDecodeCaps{};
	util::getVideoDecodeCapabilities(*decoder->m_deviceContext, decoder->m_profile, videoCaps, videoDecodeCaps);

	const VkParserInitDecodeParameters pdParams = {
		NV_VULKAN_VIDEO_PARSER_API_VERSION,
		dynamic_cast<VkParserVideoDecodeClient*>(decoder),
		static_cast<deUint32>(2 * 1024 * 1024), // 2MiB is the default bitstream buffer size
		static_cast<deUint32>(videoCaps.minBitstreamBufferOffsetAlignment),
		static_cast<deUint32>(videoCaps.minBitstreamBufferSizeAlignment),
		0,
		0,
		nullptr,
		true,
	};

	if (videoLoggingEnabled())
	{
		tcu::print("Creating a parser with offset alignment=%d and size alignment=%d\n",
				   static_cast<deUint32>(videoCaps.minBitstreamBufferOffsetAlignment),
				   static_cast<deUint32>(videoCaps.minBitstreamBufferSizeAlignment));
	}

	const VkExtensionProperties* pStdExtensionVersion = params->extensionProperties();
	DE_ASSERT(pStdExtensionVersion);

	switch (params->getCodecOperation())
	{
		case VK_VIDEO_CODEC_OPERATION_DECODE_H264_BIT_KHR:
		{
			if (strcmp(pStdExtensionVersion->extensionName, VK_STD_VULKAN_VIDEO_CODEC_H264_DECODE_EXTENSION_NAME) || pStdExtensionVersion->specVersion != VK_STD_VULKAN_VIDEO_CODEC_H264_DECODE_SPEC_VERSION)
			{
				tcu::die("The requested decoder h.264 Codec STD version is NOT supported. The supported decoder h.264 Codec STD version is version %d of %s\n",
						 VK_STD_VULKAN_VIDEO_CODEC_H264_DECODE_SPEC_VERSION,
						 VK_STD_VULKAN_VIDEO_CODEC_H264_DECODE_EXTENSION_NAME);
			}
			VkSharedBaseObj<VulkanH264Decoder> nvVideoH264DecodeParser(new VulkanH264Decoder(params->getCodecOperation()));
			parser = nvVideoH264DecodeParser;
			break;
		}
		case VK_VIDEO_CODEC_OPERATION_DECODE_H265_BIT_KHR:
		{
			if (strcmp(pStdExtensionVersion->extensionName, VK_STD_VULKAN_VIDEO_CODEC_H265_DECODE_EXTENSION_NAME) || pStdExtensionVersion->specVersion != VK_STD_VULKAN_VIDEO_CODEC_H265_DECODE_SPEC_VERSION)
			{
				tcu::die("The requested decoder h.265 Codec STD version is NOT supported. The supported decoder h.265 Codec STD version is version %d of %s\n",
						 VK_STD_VULKAN_VIDEO_CODEC_H265_DECODE_SPEC_VERSION,
						 VK_STD_VULKAN_VIDEO_CODEC_H265_DECODE_EXTENSION_NAME);
			}
			VkSharedBaseObj<VulkanH265Decoder> nvVideoH265DecodeParser(new VulkanH265Decoder(params->getCodecOperation()));
			parser = nvVideoH265DecodeParser;
			break;
		}
		default:
			TCU_FAIL("Unsupported codec type!");
	}

	VK_CHECK(parser->Initialize(&pdParams));
}

static MovePtr<VideoBaseDecoder> decoderFromTestDefinition(DeviceContext* devctx, const TestDefinition& test)
{
	VkSharedBaseObj<VulkanVideoFrameBuffer> vkVideoFrameBuffer;
	VK_CHECK(VulkanVideoFrameBuffer::Create(devctx,
											test.hasOption(DecoderOption::UseStatusQueries),
											vkVideoFrameBuffer));

	VideoBaseDecoder::Parameters params;
	params.profile							 = test.getProfile();
	params.context							 = devctx;
	params.framebuffer						 = vkVideoFrameBuffer;
	params.framesToCheck					 = test.framesToCheck();
	params.queryDecodeStatus				 = test.hasOption(DecoderOption::UseStatusQueries);
	params.outOfOrderDecoding				 = test.hasOption(DecoderOption::CachedDecoding);
	params.alwaysRecreateDPB				 = test.hasOption(DecoderOption::RecreateDPBImages);
	params.pictureParameterUpdateTriggerHack = test.getParamaterUpdateHackRequirement();

	return MovePtr<VideoBaseDecoder>(new VideoBaseDecoder(std::move(params)));
}

class FrameProcessor
{
public:
	static const int DECODER_QUEUE_SIZE = 6;

	FrameProcessor(DeviceContext* devctx, const TestDefinition* params, VideoBaseDecoder* decoder, tcu::TestLog& log)
		: m_devctx(devctx)
		, m_demuxer(params->getClipFilename(), log)
		, m_decoder(decoder)
		, m_frameData(DECODER_QUEUE_SIZE)
		, m_frameDataIdx(0)
	{
		createParser(params, m_decoder, m_parser);
		for (auto& frame : m_frameData)
			frame.Reset();
	}

	void parseNextChunk()
	{
		deUint8*				pData		   = 0;
		deInt64					size		   = 0;
		bool					demuxerSuccess = m_demuxer.Demux(&pData, &size);

		VkParserBitstreamPacket pkt;
		pkt.pByteStream			 = pData; // Ptr to byte stream data decode/display event
		pkt.nDataLength			 = size; // Data length for this packet
		pkt.llPTS				 = 0; // Presentation Time Stamp for this packet (clock rate specified at initialization)
		pkt.bEOS				 = !demuxerSuccess; // true if this is an End-Of-Stream packet (flush everything)
		pkt.bPTSValid			 = false; // true if llPTS is valid (also used to detect frame boundaries for VC1 SP/MP)
		pkt.bDiscontinuity		 = false; // true if DecMFT is signalling a discontinuity
		pkt.bPartialParsing		 = 0; // 0: parse entire packet, 1: parse until next
		pkt.bEOP				 = false; // true if the packet in pByteStream is exactly one frame
		pkt.pbSideData			 = nullptr; // Auxiliary encryption information
		pkt.nSideDataLength		 = 0; // Auxiliary encrypton information length

		size_t	   parsedBytes	 = 0;
		const bool parserSuccess = m_parser->ParseByteStream(&pkt, &parsedBytes);
		if (videoLoggingEnabled())
			std::cout << "Parsed " << parsedBytes << " bytes from bitstream" << std::endl;

		m_videoStreamHasEnded = !(demuxerSuccess && parserSuccess);
	}

	int getNextFrame(DecodedFrame* pFrame)
	{
		// The below call to DequeueDecodedPicture allows returning the next frame without parsing of the stream.
		// Parsing is only done when there are no more frames in the queue.
		int32_t framesInQueue = m_decoder->GetVideoFrameBuffer()->DequeueDecodedPicture(pFrame);

		// Loop until a frame (or more) is parsed and added to the queue.
		while ((framesInQueue == 0) && !m_videoStreamHasEnded)
		{
			parseNextChunk();
			framesInQueue		= m_decoder->GetVideoFrameBuffer()->DequeueDecodedPicture(pFrame);
		}

		if ((framesInQueue == 0) && m_videoStreamHasEnded)
		{
			return -1;
		}

		return framesInQueue;
	}

	const DecodedFrame* decodeFrame()
	{
		auto&		  vk				= m_devctx->getDeviceDriver();
		auto		  device			= m_devctx->device;
		DecodedFrame* pLastDecodedFrame = &m_frameData[m_frameDataIdx];

		// Make sure the frame complete fence signaled (video frame is processed) before returning the frame.
		if (pLastDecodedFrame->frameCompleteFence != VK_NULL_HANDLE)
		{
			VK_CHECK(vk.waitForFences(device, 1, &pLastDecodedFrame->frameCompleteFence, true, TIMEOUT_100ms));
			VK_CHECK(vk.getFenceStatus(device, pLastDecodedFrame->frameCompleteFence));
		}

		m_decoder->ReleaseDisplayedFrame(pLastDecodedFrame);
		pLastDecodedFrame->Reset();

		TCU_CHECK_MSG(getNextFrame(pLastDecodedFrame) > 0, "Unexpected decode result");
		TCU_CHECK_MSG(pLastDecodedFrame, "Unexpected decode result");

		if (videoLoggingEnabled())
			std::cout << "<= Wait on picIdx: " << pLastDecodedFrame->pictureIndex
					  << "\t\tdisplayWidth: " << pLastDecodedFrame->displayWidth
					  << "\t\tdisplayHeight: " << pLastDecodedFrame->displayHeight
					  << "\t\tdisplayOrder: " << pLastDecodedFrame->displayOrder
					  << "\tdecodeOrder: " << pLastDecodedFrame->decodeOrder
					  << "\ttimestamp " << pLastDecodedFrame->timestamp
					  << "\tdstImageView " << (pLastDecodedFrame->outputImageView ? pLastDecodedFrame->outputImageView->GetImageResource()->GetImage() : VK_NULL_HANDLE)
					  << std::endl;

		m_frameDataIdx = (m_frameDataIdx + 1) % m_frameData.size();
		return pLastDecodedFrame;
	}

	void bufferFrames(int framesToDecode)
	{
		// This loop is for the out-of-order submissions cases. First all the frame information is gathered from the parser<->decoder loop
		// then the command buffers are recorded in a random order, as well as the queue submissions, depending on the configuration of
		// the test.
		// NOTE: For this sequence to work, the frame buffer must have enough decode surfaces for the GOP intended for decode, otherwise
		// picture allocation will fail pretty quickly! See m_numDecodeSurfaces, m_maxDecodeFramesCount
		// The previous CTS cases were not actually randomizing the queue submission order (despite claiming too!)
		DE_ASSERT(m_decoder->m_outOfOrderDecoding);
		do
		{
			parseNextChunk();
			size_t decodedFrames	= m_decoder->GetVideoFrameBuffer()->GetDisplayedFrameCount();
			if (decodedFrames == framesToDecode)
				break;
		}
		while (!m_videoStreamHasEnded);
		DE_ASSERT(m_decoder->m_cachedDecodeParams.size() == framesToDecode);
	}

	int getBufferedDisplayCount() const { return m_decoder->GetVideoFrameBuffer()->GetDisplayedFrameCount(); }
private:
	DeviceContext* m_devctx;
	ESEDemuxer m_demuxer;
	VkVideoParser m_parser;
	VideoBaseDecoder* m_decoder;

	std::vector<DecodedFrame> m_frameData;
	size_t m_frameDataIdx{};

	bool m_videoStreamHasEnded{false};
};

de::MovePtr<vkt::ycbcr::MultiPlaneImageData> getDecodedImage(DeviceContext&		 devctx,
															 VkImageLayout		 layout,
															 const DecodedFrame* frame)
{
	auto&									 vkd					  = devctx.getDeviceDriver();
	auto									 device					  = devctx.device;
	auto									 queueFamilyIndexDecode	  = devctx.decodeQueueFamilyIdx();
	auto									 queueFamilyIndexTransfer = devctx.transferQueueFamilyIdx();
	const VkExtent2D						 imageExtent{(deUint32)frame->displayWidth, (deUint32)frame->displayHeight};
	const VkImage							 image	= frame->outputImageView->GetImageResource()->GetImage();
	const VkFormat							 format = frame->outputImageView->GetImageResource()->GetImageCreateInfo().format;

	MovePtr<vkt::ycbcr::MultiPlaneImageData> multiPlaneImageData(new vkt::ycbcr::MultiPlaneImageData(format, tcu::UVec2(imageExtent.width, imageExtent.height)));
	const VkQueue							 queueDecode				   = getDeviceQueue(vkd, device, queueFamilyIndexDecode, 0u);
	const VkQueue							 queueTransfer				   = getDeviceQueue(vkd, device, queueFamilyIndexTransfer, 0u);
	const VkImageSubresourceRange			 imageSubresourceRange		   = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1);
	const VkImageMemoryBarrier2KHR			 imageBarrierDecode			   = makeImageMemoryBarrier2(VK_PIPELINE_STAGE_2_VIDEO_DECODE_BIT_KHR,
																				 VK_ACCESS_2_VIDEO_DECODE_WRITE_BIT_KHR,
																				 VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT_KHR,
																				 VK_ACCESS_NONE_KHR,
																				 layout,
																				 VK_IMAGE_LAYOUT_GENERAL,
																				 image,
																				 imageSubresourceRange);
	const VkImageMemoryBarrier2KHR			 imageBarrierOwnershipDecode   = makeImageMemoryBarrier2(VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT_KHR,
																						 VK_ACCESS_NONE_KHR,
																						 VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT_KHR,
																						 VK_ACCESS_NONE_KHR,
																						 VK_IMAGE_LAYOUT_GENERAL,
																						 VK_IMAGE_LAYOUT_GENERAL,
																						 image,
																						 imageSubresourceRange,
																						 queueFamilyIndexDecode,
																						 queueFamilyIndexTransfer);
	const VkImageMemoryBarrier2KHR			 imageBarrierOwnershipTransfer = makeImageMemoryBarrier2(VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT_KHR,
																							 VK_ACCESS_NONE_KHR,
																							 VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT_KHR,
																							 VK_ACCESS_NONE_KHR,
																							 VK_IMAGE_LAYOUT_GENERAL,
																							 VK_IMAGE_LAYOUT_GENERAL,
																							 image,
																							 imageSubresourceRange,
																							 queueFamilyIndexDecode,
																							 queueFamilyIndexTransfer);
	const VkImageMemoryBarrier2KHR			 imageBarrierTransfer		   = makeImageMemoryBarrier2(VK_PIPELINE_STAGE_2_TRANSFER_BIT_KHR,
																					 VK_ACCESS_2_TRANSFER_READ_BIT_KHR,
																					 VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT_KHR,
																					 VK_ACCESS_NONE_KHR,
																					 VK_IMAGE_LAYOUT_GENERAL,
																					 VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
																					 image,
																					 imageSubresourceRange);
	const Move<VkCommandPool>				 cmdDecodePool(makeCommandPool(vkd, device, queueFamilyIndexDecode));
	const Move<VkCommandBuffer>				 cmdDecodeBuffer(allocateCommandBuffer(vkd, device, *cmdDecodePool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));
	const Move<VkCommandPool>				 cmdTransferPool(makeCommandPool(vkd, device, queueFamilyIndexTransfer));
	const Move<VkCommandBuffer>				 cmdTransferBuffer(allocateCommandBuffer(vkd, device, *cmdTransferPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));
	Move<VkSemaphore>						 semaphore		  = createSemaphore(vkd, device);
	Move<VkFence>							 decodeFence	  = createFence(vkd, device);
	Move<VkFence>							 transferFence	  = createFence(vkd, device);
	VkFence									 fences[]		  = {*decodeFence, *transferFence};
	const VkPipelineStageFlags				 waitDstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	VkSubmitInfo							 decodeSubmitInfo{
		VK_STRUCTURE_TYPE_SUBMIT_INFO, // VkStructureType                              sType;
		DE_NULL, // const void*                                  pNext;
		0u, // deUint32                                             waitSemaphoreCount;
		DE_NULL, // const VkSemaphore*                   pWaitSemaphores;
		DE_NULL, // const VkPipelineStageFlags*  pWaitDstStageMask;
		1u, // deUint32                                             commandBufferCount;
		&*cmdDecodeBuffer, // const VkCommandBuffer*               pCommandBuffers;
		1u, // deUint32                                             signalSemaphoreCount;
		&*semaphore, // const VkSemaphore*                   pSignalSemaphores;
	};
	if (frame->frameCompleteSemaphore != VK_NULL_HANDLE)
	{
		decodeSubmitInfo.waitSemaphoreCount = 1;
		decodeSubmitInfo.pWaitSemaphores	= &frame->frameCompleteSemaphore;
		decodeSubmitInfo.pWaitDstStageMask	= &waitDstStageMask;
	}
	const VkSubmitInfo transferSubmitInfo{
		VK_STRUCTURE_TYPE_SUBMIT_INFO, // VkStructureType                              sType;
		DE_NULL, // const void*                                  pNext;
		1u, // deUint32                                             waitSemaphoreCount;
		&*semaphore, // const VkSemaphore*                   pWaitSemaphores;
		&waitDstStageMask, // const VkPipelineStageFlags*  pWaitDstStageMask;
		1u, // deUint32                                             commandBufferCount;
		&*cmdTransferBuffer, // const VkCommandBuffer*               pCommandBuffers;
		0u, // deUint32                                             signalSemaphoreCount;
		DE_NULL, // const VkSemaphore*                   pSignalSemaphores;
	};

	DEBUGLOG(std::cout << "getDecodedImage: " << image << " " << layout << std::endl);

	beginCommandBuffer(vkd, *cmdDecodeBuffer, 0u);
	cmdPipelineImageMemoryBarrier2(vkd, *cmdDecodeBuffer, &imageBarrierDecode);
	cmdPipelineImageMemoryBarrier2(vkd, *cmdDecodeBuffer, &imageBarrierOwnershipDecode);
	endCommandBuffer(vkd, *cmdDecodeBuffer);

	beginCommandBuffer(vkd, *cmdTransferBuffer, 0u);
	cmdPipelineImageMemoryBarrier2(vkd, *cmdTransferBuffer, &imageBarrierOwnershipTransfer);
	cmdPipelineImageMemoryBarrier2(vkd, *cmdTransferBuffer, &imageBarrierTransfer);
	endCommandBuffer(vkd, *cmdTransferBuffer);

	VK_CHECK(vkd.queueSubmit(queueDecode, 1u, &decodeSubmitInfo, *decodeFence));
	VK_CHECK(vkd.queueSubmit(queueTransfer, 1u, &transferSubmitInfo, *transferFence));

	VK_CHECK(vkd.waitForFences(device, DE_LENGTH_OF_ARRAY(fences), fences, DE_TRUE, ~0ull));

	vkt::ycbcr::downloadImage(vkd, device, queueFamilyIndexTransfer, devctx.allocator(), image, multiPlaneImageData.get(), 0, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

	const VkImageMemoryBarrier2KHR imageBarrierTransfer2 = makeImageMemoryBarrier2(VK_PIPELINE_STAGE_2_TRANSFER_BIT_KHR,
																				   VK_ACCESS_2_TRANSFER_WRITE_BIT_KHR,
																				   VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT_KHR,
																				   VK_ACCESS_NONE_KHR,
																				   VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
																				   layout,
																				   image,
																				   imageSubresourceRange);

	vkd.resetCommandBuffer(*cmdTransferBuffer, 0u);
	vkd.resetFences(device, 1, &*transferFence);
	beginCommandBuffer(vkd, *cmdTransferBuffer, 0u);
	cmdPipelineImageMemoryBarrier2(vkd, *cmdTransferBuffer, &imageBarrierTransfer2);
	endCommandBuffer(vkd, *cmdTransferBuffer);

	const VkSubmitInfo transferSubmitInfo2{
		VK_STRUCTURE_TYPE_SUBMIT_INFO, // VkStructureType                              sType;
		DE_NULL, // const void*                                  pNext;
		0u, // deUint32                                             waitSemaphoreCount;
		DE_NULL, // const VkSemaphore*                   pWaitSemaphores;
		DE_NULL, // const VkPipelineStageFlags*  pWaitDstStageMask;
		1u, // deUint32                                             commandBufferCount;
		&*cmdTransferBuffer, // const VkCommandBuffer*               pCommandBuffers;
		0u, // deUint32                                             signalSemaphoreCount;
		DE_NULL, // const VkSemaphore*                   pSignalSemaphores;
	};

	VK_CHECK(vkd.queueSubmit(queueTransfer, 1u, &transferSubmitInfo2, *transferFence));
	VK_CHECK(vkd.waitForFences(device, 1, &*transferFence, DE_TRUE, ~0ull));

	return multiPlaneImageData;
}


class VideoDecodeTestInstance : public VideoBaseTestInstance
{
public:
	VideoDecodeTestInstance(Context& context, const TestDefinition* testDefinition);
	tcu::TestStatus iterate(void);

protected:
	const TestDefinition*	  m_testDefinition;
	MovePtr<VideoBaseDecoder> m_decoder{};
	static_assert(sizeof(DeviceContext) < 128, "DeviceContext has grown bigger than expected!");
	DeviceContext m_deviceContext;
};

class InterleavingDecodeTestInstance : public VideoBaseTestInstance
{
public:
	InterleavingDecodeTestInstance(Context& context, const std::vector<MovePtr<TestDefinition>>& testDefinitions);
	tcu::TestStatus iterate(void);

protected:
	const std::vector<MovePtr<TestDefinition>>& m_testDefinitions;
	std::vector<MovePtr<VideoBaseDecoder>>		m_decoders{};
	static_assert(sizeof(DeviceContext) < 128, "DeviceContext has grown bigger than expected!");
	DeviceContext m_deviceContext;
};

InterleavingDecodeTestInstance::InterleavingDecodeTestInstance(Context& context, const std::vector<MovePtr<TestDefinition>>& testDefinitions)
	: VideoBaseTestInstance(context), m_testDefinitions(std::move(testDefinitions))
{
	int							  requiredCodecs	  = VK_VIDEO_CODEC_OPERATION_NONE_KHR;
	VideoDevice::VideoDeviceFlags requiredDeviceFlags = VideoDevice::VideoDeviceFlagBits::VIDEO_DEVICE_FLAG_NONE;
	for (const auto& test : m_testDefinitions)
	{
		VkVideoCodecOperationFlagBitsKHR testBits = test->getCodecOperation();
		requiredCodecs |= testBits;
		requiredDeviceFlags |= test->requiredDeviceFlags();
	}
	VkDevice device			= getDeviceSupportingQueue(VK_QUEUE_VIDEO_DECODE_BIT_KHR | VK_QUEUE_TRANSFER_BIT, requiredCodecs, requiredDeviceFlags);

	m_deviceContext.context = &m_context;
	m_deviceContext.device	= device;
	m_deviceContext.phys	= m_context.getPhysicalDevice();
	m_deviceContext.vd		= &m_videoDevice;
	// TODO: Support for multiple queues / multithreading
	m_deviceContext.transferQueue =
		getDeviceQueue(m_context.getDeviceInterface(), device, m_videoDevice.getQueueFamilyIndexTransfer(), 0);
	m_deviceContext.decodeQueue =
		getDeviceQueue(m_context.getDeviceInterface(), device, m_videoDevice.getQueueFamilyIndexDecode(), 0);

	for (const auto& test : m_testDefinitions)
		m_decoders.push_back(decoderFromTestDefinition(&m_deviceContext, *test));
}

VideoDecodeTestInstance::VideoDecodeTestInstance(Context& context, const TestDefinition* testDefinition)
	: VideoBaseTestInstance(context), m_testDefinition(testDefinition)
{
	VkDevice device			= getDeviceSupportingQueue(VK_QUEUE_VIDEO_DECODE_BIT_KHR | VK_QUEUE_TRANSFER_BIT,
											   m_testDefinition->getCodecOperation(),
											   m_testDefinition->requiredDeviceFlags());

	m_deviceContext.context = &m_context;
	m_deviceContext.device	= device;
	m_deviceContext.phys	= m_context.getPhysicalDevice();
	m_deviceContext.vd		= &m_videoDevice;
	// TODO: Support for multiple queues / multithreading
	m_deviceContext.transferQueue =
		getDeviceQueue(m_context.getDeviceInterface(), device, m_videoDevice.getQueueFamilyIndexTransfer(), 0);
	m_deviceContext.decodeQueue =
		getDeviceQueue(m_context.getDeviceInterface(), device, m_videoDevice.getQueueFamilyIndexDecode(), 0);

	m_decoder = decoderFromTestDefinition(&m_deviceContext, *m_testDefinition);
}

tcu::TestStatus VideoDecodeTestInstance::iterate()
{
#if FRAME_DUMP_DEBUG
#ifdef _WIN32
	FILE* output = fopen("C:\\output.yuv", "wb");
#else
	FILE* output = fopen("/tmp/output.yuv", "wb");
#endif
#endif

	FrameProcessor	 processor(&m_deviceContext, m_testDefinition, m_decoder.get(), m_context.getTestContext().getLog());
	std::vector<int> incorrectFrames;
	std::vector<int> correctFrames;

	if (m_testDefinition->hasOption(DecoderOption::CachedDecoding))
	{
		processor.bufferFrames(m_testDefinition->framesToCheck());
		m_decoder->decodeFramesOutOfOrder();
	}

	for (int frameNumber = 0; frameNumber < m_testDefinition->framesToCheck(); frameNumber++)
	{
		const DecodedFrame* decodedFrame = processor.decodeFrame();
		TCU_CHECK_MSG(decodedFrame, "Decoder did not produce the expected amount of frames");
		auto resultImage = getDecodedImage(m_deviceContext, m_decoder->dpbAndOutputCoincide() ? VK_IMAGE_LAYOUT_VIDEO_DECODE_DPB_KHR : VK_IMAGE_LAYOUT_VIDEO_DECODE_DST_KHR, decodedFrame);

#if FRAME_DUMP_DEBUG
		auto bytes = semiplanarToYV12(*resultImage);
		fwrite(bytes.data(), 1, bytes.size(), output);
#endif
		std::string checksum = checksumForClipFrame(m_testDefinition->getClipInfo(), frameNumber);
		if (imageMatchesReferenceChecksum(*resultImage, checksum))
		{
			correctFrames.push_back(frameNumber);
		}
		else
		{
			incorrectFrames.push_back(frameNumber);
		}
	}

#if FRAME_DUMP_DEBUG
	fclose(output);
#endif
	if (!correctFrames.empty() && correctFrames.size() == m_testDefinition->framesToCheck())
		return tcu::TestStatus::pass(de::toString(m_testDefinition->framesToCheck()) + " correctly decoded frames");
	else
	{
		stringstream ss;
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
		return tcu::TestStatus::fail(ss.str());
	}
}

tcu::TestStatus InterleavingDecodeTestInstance::iterate(void)
{
	DE_ASSERT(m_testDefinitions.size() == m_decoders.size());
	DE_ASSERT(m_decoders.size() > 1);

	std::vector<MovePtr<FrameProcessor>> processors;
	for (int i = 0; i < m_testDefinitions.size(); i++)
	{
		processors.push_back(MovePtr<FrameProcessor>(new FrameProcessor(&m_deviceContext, m_testDefinitions[i].get(), m_decoders[i].get(), m_context.getTestContext().getLog())));
	}

#if FRAME_DUMP_DEBUG
#ifdef _WIN32
	FILE* output = fopen("C:\\output.yuv", "wb");
#else
	FILE* output = fopen("/tmp/output.yuv", "wb");
#endif
#endif

	// First cache up all the decoded frames from the various decode sessions
	for (int i = 0; i < m_testDefinitions.size(); i++)
	{
		const auto& test	  = m_testDefinitions[i];
		auto&		processor = processors[i];
		processor->bufferFrames(test->framesToCheck());
		DE_ASSERT(processor->getBufferedDisplayCount() == test->framesToCheck());
	}

	auto interleaveCacheSize	= m_decoders[0]->m_cachedDecodeParams.size();
	auto firstStreamDecodeQueue = m_decoders[0]->m_deviceContext->decodeQueue;

	size_t	 totalFrames			= 0;
	for (auto& decoder : m_decoders)
	{
		DE_ASSERT(decoder->m_cachedDecodeParams.size() == interleaveCacheSize);
		DE_ASSERT(decoder->m_deviceContext->decodeQueue == firstStreamDecodeQueue);
		totalFrames += decoder->m_cachedDecodeParams.size();
	}

	DE_UNREF(firstStreamDecodeQueue);

	// Interleave command buffer recording
	for (int i = 0; i < interleaveCacheSize; i++)
	{
		for (auto& decoder : m_decoders)
		{
			decoder->WaitForFrameFences(decoder->m_cachedDecodeParams[i]);
			decoder->ApplyPictureParameters(decoder->m_cachedDecodeParams[i]);
			decoder->RecordCommandBuffer(decoder->m_cachedDecodeParams[i]);
		}
	}

	// Interleave submissions
	for (int i = 0; i < interleaveCacheSize; i++)
	{
		for (int decoderIdx = 0; decoderIdx < m_decoders.size(); decoderIdx++)
		{
			auto& decoder = m_decoders[decoderIdx];
			auto& test	  = m_testDefinitions[decoderIdx];
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
		auto& test		= m_testDefinitions[i];
		auto& decoder	= m_decoders[i];
		auto& processor = processors[i];
		for (int frameNumber = 0; frameNumber < m_testDefinitions[i]->framesToCheck(); frameNumber++)
		{
			const DecodedFrame* frame		= processor->decodeFrame();
			auto				resultImage = getDecodedImage(m_deviceContext, decoder->dpbAndOutputCoincide() ? VK_IMAGE_LAYOUT_VIDEO_DECODE_DPB_KHR : VK_IMAGE_LAYOUT_VIDEO_DECODE_DST_KHR, frame);
#if FRAME_DUMP_DEBUG
			auto bytes = semiplanarToYV12(*resultImage);
			fwrite(bytes.data(), 1, bytes.size(), output);
#endif
			auto				checksum	= checksumForClipFrame(test->getClipInfo(), frameNumber);
			if (imageMatchesReferenceChecksum(*resultImage, checksum))
			{
				results[i].correctFrames.push_back(frameNumber);
			}
			else
			{
				results[i].incorrectFrames.push_back(frameNumber);
			}
		}
	}

#if FRAME_DUMP_DEBUG
	fclose(output);
#endif

	bool allTestsPassed	  = true;
	int	 totalFramesCheck = 0;
	for (const auto& res : results)
	{
		if (!res.incorrectFrames.empty())
			allTestsPassed = false;
		totalFramesCheck += (res.correctFrames.size() + res.incorrectFrames.size());
	}
	DE_ASSERT(totalFramesCheck == totalFrames);

	if (allTestsPassed)
		return tcu::TestStatus::pass(de::toString(totalFrames) + " correctly decoded frames");
	else
	{
		stringstream ss;
		ss << "Interleaving failure: ";
		for (int i = 0; i < results.size(); i++)
		{
			const auto& result = results[i];
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
	VideoDecodeTestCase(tcu::TestContext& context, const char* name, const char* desc, MovePtr<TestDefinition> testDefinition)
		: vkt::TestCase(context, name, desc), m_testDefinition(testDefinition)
	{
	}

	TestInstance* createInstance(Context& context) const override;
	void		  checkSupport(Context& context) const override;

private:
	MovePtr<TestDefinition> m_testDefinition;
};

class InterleavingDecodeTestCase : public vkt::TestCase
{
public:
	InterleavingDecodeTestCase(tcu::TestContext& context, const char* name, const char* desc, std::vector<MovePtr<TestDefinition>>&& testDefinitions)
		: vkt::TestCase(context, name, desc), m_testDefinitions(std::move(testDefinitions))
	{
	}

	TestInstance* createInstance(Context& context) const override
	{
#ifdef DE_BUILD_VIDEO
		return new InterleavingDecodeTestInstance(context, m_testDefinitions);
#endif
		DE_UNREF(context);
		return nullptr;
	}
	void checkSupport(Context& context) const override;

private:
	std::vector<MovePtr<TestDefinition>> m_testDefinitions;
};

TestInstance* VideoDecodeTestCase::createInstance(Context& context) const
{
#ifdef DE_BUILD_VIDEO
	return new VideoDecodeTestInstance(context, m_testDefinition.get());
#endif

#ifndef DE_BUILD_VIDEO
	DE_UNREF(context);
	return nullptr;
#endif
}

void VideoDecodeTestCase::checkSupport(Context& context) const
{
	context.requireDeviceFunctionality("VK_KHR_video_queue");
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
		case TEST_TYPE_H265_DECODE_I:
		case TEST_TYPE_H265_DECODE_I_P:
		case TEST_TYPE_H265_DECODE_CLIP_D:
		case TEST_TYPE_H265_DECODE_I_P_NOT_MATCHING_ORDER:
		case TEST_TYPE_H265_DECODE_I_P_B_13:
		case TEST_TYPE_H265_DECODE_I_P_B_13_NOT_MATCHING_ORDER:
		{
			context.requireDeviceFunctionality("VK_KHR_video_decode_h265");
			break;
		}
		default:
			TCU_THROW(InternalError, "Unknown TestType");
	}
}

void InterleavingDecodeTestCase::checkSupport(Context& context) const
{
	context.requireDeviceFunctionality("VK_KHR_video_queue");
	context.requireDeviceFunctionality("VK_KHR_synchronization2");

#ifdef DE_DEBUG
	DE_ASSERT(!m_testDefinitions.empty());
	TestType firstType = m_testDefinitions[0]->getTestType();
	for (const auto& test : m_testDefinitions)
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

tcu::TestCaseGroup* createVideoDecodeTests(tcu::TestContext& testCtx)
{
	const deUint32				baseSeed = static_cast<deUint32>(testCtx.getCommandLine().getBaseSeed());
	MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(testCtx, "decode", "Video decoding session tests"));

	for (const auto& decodeTest : g_DecodeTests)
	{
		auto		defn	 = TestDefinition::create(decodeTest, baseSeed);

		const char* testName = getTestName(defn->getTestType());
		deUint32	rngSeed	 = baseSeed ^ deStringHash(testName);
		defn->updateHash(rngSeed);
		group->addChild(new VideoDecodeTestCase(testCtx, testName, "", defn));
	}

	for (const auto& interleavingTest : g_InterleavingTests)
	{
		const char*							 testName = getTestName(interleavingTest.type);
		std::vector<MovePtr<TestDefinition>> defns;
		DecodeTestParam						 streamA{interleavingTest.type, interleavingTest.streamA};
		defns.push_back(TestDefinition::create(streamA, baseSeed));
		DecodeTestParam streamB{interleavingTest.type, interleavingTest.streamB};
		defns.push_back(TestDefinition::create(streamB, baseSeed));
		group->addChild(new InterleavingDecodeTestCase(testCtx, testName, "", std::move(defns)));
	}

	return group.release();
}

} // namespace video
} // namespace vkt
