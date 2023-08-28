/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2021 The Khronos Group Inc.
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
 *//*!
 * \file
 * \brief Video Encoding and Decoding tests
 *//*--------------------------------------------------------------------*/

#include "vktVideoTests.hpp"
#include "vktVideoCapabilitiesTests.hpp"
#include "vktVideoDecodeTests.hpp"
#include "synchronization/vktSynchronizationTests.hpp"

#include "deUniquePtr.hpp"

namespace vkt
{
namespace video
{

using namespace vk;

tcu::TestCaseGroup*	createTests (tcu::TestContext& testCtx, const std::string& name)
{
	de::MovePtr<tcu::TestCaseGroup>	group	(new tcu::TestCaseGroup(testCtx, name.c_str(), "Video encoding and decoding tests"));

	group->addChild(createVideoCapabilitiesTests(testCtx));
	group->addChild(createVideoDecodeTests(testCtx));

	{
		de::MovePtr<tcu::TestCaseGroup>	syncGroup(new tcu::TestCaseGroup(testCtx, "synchronization", ""));

		syncGroup->addChild(createSynchronizationTests(testCtx, "encode_h264", VK_VIDEO_CODEC_OPERATION_ENCODE_H264_BIT_EXT));
		syncGroup->addChild(createSynchronizationTests(testCtx, "encode_h265", VK_VIDEO_CODEC_OPERATION_ENCODE_H265_BIT_EXT));
		syncGroup->addChild(createSynchronizationTests(testCtx, "decode_h264", VK_VIDEO_CODEC_OPERATION_DECODE_H264_BIT_KHR));
		syncGroup->addChild(createSynchronizationTests(testCtx, "decode_h265", VK_VIDEO_CODEC_OPERATION_DECODE_H265_BIT_KHR));

		group->addChild(syncGroup.release());
	}

	{
		de::MovePtr<tcu::TestCaseGroup>	syncGroup(new tcu::TestCaseGroup(testCtx, "synchronization2", ""));

		syncGroup->addChild(createSynchronizationTests(testCtx, "encode_h264", VK_VIDEO_CODEC_OPERATION_ENCODE_H264_BIT_EXT));
		syncGroup->addChild(createSynchronizationTests(testCtx, "encode_h265", VK_VIDEO_CODEC_OPERATION_ENCODE_H265_BIT_EXT));
		syncGroup->addChild(createSynchronizationTests(testCtx, "decode_h264", VK_VIDEO_CODEC_OPERATION_DECODE_H264_BIT_KHR));
		syncGroup->addChild(createSynchronizationTests(testCtx, "decode_h265", VK_VIDEO_CODEC_OPERATION_DECODE_H265_BIT_KHR));

		group->addChild(syncGroup.release());
	}

	return group.release();
}

}	// video
}	// vkt
