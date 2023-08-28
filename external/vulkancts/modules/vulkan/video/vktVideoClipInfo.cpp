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
 *//*!
 * \file
 * \brief Reference checksums for video decode validation
 *
 * See the <vulkan_data_dir>/vulkan/video/frame_checksums.py file for
 * instructions on generating the checksums for new tests.
 *--------------------------------------------------------------------*/

#include "vktVideoClipInfo.hpp"
#include "vktVideoTestUtils.hpp"


namespace vkt {
namespace video
{

static const char* clipA[30] =
{
	"7fec73fbcd485785b0c5f14776628bd6",
	"4e635dfa94b770ee983540746f416270",
	"a8bbb2ae42f5edebe7891ff803be7c4f",
	"3389478c7a0011551274bb68ce6deb5f",
	"63aa1d861304729dfec950f486a15a0b",
	"04980dcedb06d21d0c7dc37653d3a458",
	"2a0162386ebbf3eea6f613042cb2a617",
	"a29121c412ae6bda174254039a5b6086",
	"bfdeb27a7a83b250a8311a4468c3e4bf",
	"cee21548a5235a8eea878dd695ef8f50",
	"3b8cc378c3843c7c5918e9213e9e4f81",
	"d11322bbaec122fcc7f2d8ed6b455110",
	"ffe3f13784f7689735d2b8c96bcd2d0b",
	"0253711c4523988d4b9736238f5e5d01",
	"d3968c4560c31815443f857ddb0082a3",
	"74b0ae5f72a6676e4679b30f06598277",
	"7d49ad952d2600aefe20e0a6611e91bf",
	"32d86ff0d1dfb493adb2d90e973025fb",
	"a0f52f968b800bd8b67c2a44b5394690",
	"0a54a47ed72ecead9f7a43f39a562fca",
	"c1effee5f75f5c59c71a3765e199861c",
	"75f020c1da96c2d5da31483c16a1e4ed",
	"66cfa534ced4f1b18b6b04cc0a41331d",
	"3648527cf7b9b7366759c5ab8f1aab20",
	"1c7c47a07c5bb38dfe2a4e9e2cf5d860",
	"2e2bc3c723a731b4df4eb32a0b36293d",
	"6e647c760a0304d97abf291a2b9e66be",
	"5a9fe2e1bdf31ad0d256aeeecbe35ae7",
	"3e825fb6fb5361151fb7c1347523d6d0",
	"2870bd63631f4b787a1b084974cd519c",
};

static const char* clipB[30] =
{
	"a5221c5caa575aadb9c6c1e968ab6c29",
	"5214efd7d98f6c4dcab897bb1310743b",
	"6c7ab991e68a285711731911d88ed121",
	"43a583ed805e254a5473ae515eea2e93",
	"1a9fb658f449d71648b6f2cdca1aa34d",
	"377be6813960071259167f7305894469",
	"cf3ab21b6b6570f63a2b8eff7ee04879",
	"596a68452693b2e56df40e30275a353e",
	"e785d42901a1196690bd0f3700011a90",
	"88a00463d768d796e106408ebcb6721e",
	"bda7da34a619ef6d9cd08ab03fa72e3a",
	"bebc1624c3cdebcfc3016c33955f4923",
	"e209eac5c302e23252570b74973f94bf",
	"4ff0ba2f72cb6d8f0229dc12e2cfbbcf",
	"18ac88f74d7df142046079d733779035",
	"147bb36e92f6ff87e348c48f38b47dab",
	"9e09197f76ba89c70c2b183a167a6696",
	"867c16867c26bc0c58f16c5ebeff6eb2",
	"b8efc23369b7f2ea19e2b21ce97368db",
	"5b911f588984740ef772f5845f3a5805",
	"27f0f7fe39718799061e0e532bfbed14",
	"60020402bc2b645b703af5156f52ee9d",
	"d73e71817bf776b62b4d1e81793d659f",
	"e873082290fbcfddebd784fc2edaab9f",
	"2f4e293b00c145f1f5515920d2fd5705",
	"be907f259eca15e8914bc08861f3c76f",
	"a18eb46de4c7dff7915bc65673d2fff9",
	"615315bca5a50fabad86745e070b3c8f",
	"2a2aa07f61aa590eb9a10a07c85dfe04",
	"119fd37851af0cac45fda60d7dc1ccd8",
};

static const char* clipC[30] =
{
	"2086be0ea6f35e68a9f34ea51a359ef0", // 352x288 reference checksums
	"9e045b8e14c7f635fff4a4e56a359ff7",
	"b9aace50035d92d2b2b0cbc5049ccea1",
	"eda08231bb604567d72d850b9aaa8658",
	"b5b9ec2c1a231f595c6c747a93926287",
	"81f77e981e65926f04ed9483f838ce1f",
	"8fb63cb865f410e5d003a6691ef6d998",
	"0417e3ce4d51670e8cf5e1a1a8b64b0d",
	"5c7cee8e1e72b3acb71dd09f51e3d4e4",
	"966368b1970e59dbd14d5e492691c25c",
	"8afd1bb08c05651dacc5bf1a72fb1001",
	"bd9fe310e70fc55fbc0b43895aa14eb1",
	"057435492eb70ebf1ffcec5b423828f7",
	"6df3ab9e8f590e8e21b88e5401361d49",
	"531730c9a1ce3076c5f1a30609efcf17",
	"87823f36c56ecd539c955cfe3e44d097", // 176x144 reference checksums
	"65399b5342b3ee7bba9f51074523ae16",
	"fd3c5efbc237e24c3242aaa35124b77c",
	"56dd0b57ba4b436605f236ae7ad8535c",
	"c1694a837158581a282493ffb7778d1a",
	"64417694aeccbe36abb77cd5689083ea",
	"08bba16075a5d1d36fadac3ca3a71d06",
	"adb982cd7240ab8d97924631455a5d88",
	"822bd35904dabe46e9238aae77c5e85f",
	"1da1fc85653b894fc6f4cf2ddd41b7f6",
	"80e283c058324a21cace4c8ed1f0a178",
	"b5169864eb21620a1a4a0f988c01fefa",
	"cf38cf96287f0cd13fdbdc2b25cfd0d1",
	"95b71dc4a6a7925cfb1ad6c76e547b95",
	"c2146da8b89f068e00d7de837a02b89c",
};

static const char* h264_4k_26_ibp_main[26] =
{
	"2cd5ad413430098893a617ab46a7b885",
	"0b82fc7501897a834470f68e0647c96b",
	"5099dbd788ec90ad9010bbcc4ca3a813",
	"10ffaf41904c779627ab6ae9ebe5bf9f",
	"7d4aca0319da143a09d03b3b13d5ba23",
	"34f6ab5b1b676c189950f0b2f80d9431",
	"e812f06035d77030cf56eaa65632783c",
	"f8dbeb21b35a5fedcfd171a8207f836b",
	"b0c5a18f31c92e9778d1dcdca4b781d3",
	"bb89ff68261f0626eb8699dc3e000278",
	"aeb5064995db3a5cdae6283c67a2949e",
	"425dfb76d5961994a6dff267dc0fc5a6",
	"17800cb3d7598c6284f544069653c594",
	"bb193efca8e1a2f6bcb9f523c815468f",
	"a8eb65b204edc25a9dada32889717218",
	"715e7ec00e756ca3c85722f9464aeba3",
	"496e1e2c97a8cbfabb7b179838e3384f",
	"102445bf67c88ecb761f3263b8ab672b",
	"d9a3dbe8b05a4d7fd04a4564771b3dc0",
	"8fe9190578592d6a551f47f19926bb21",
	"854519810e75eb9501e896ec66b83d59",
	"ccd65246951a2b77dce66e11faa97811",
	"cde30e5d581267834ae91f4af77d7fac",
	"e144d9fe07470ae7be48362ab8817cc1",
	"4ef58adc2006d4456a0145102efe83b3",
	"ebfb7d5301c287e09a6473402fdcd010",
};

static const char* jellyfishHEVC[26] =
{
	"131ecbc5c6ebac63c3b25721cb5cb2b2",
	"0a9da70fd9959874e74542c7768d9e08",
	"2f09ce3f5d243d84b12f53d2574b8e9c",
	"909d939a5d09650e49703b7e78beddfe",
	"483cf425d719b961173db2f1a1c7f282",
	"b3d1e44427723fb80d1229e335e2cacd",
	"5c5f38b8ab0f19602c0af8fc4a7b37a1",
	"7e3e579e923a0f5d07b75fa18e8be8ce",
	"ad2dc11df6f46a011d0dd923c0001dc5",
	"f490093b4e2dfee8fe13b2e41310607f",
	"982511b0c457e643b4f77bfd64db55eb",
	"f985dc3bd5b366edf0a7541af3fa3a72",
	"6493f4a899a3450fe8f2dfb77ab97812",
	"04dec3a5a5ce2bd480593a708885a0c2",
	"0afbf90e7e45c04f566eac2aecb19e52",
	"e1016a026c40490db627cdc51ef7b600",
	"869d25ef36cc75bc2729ab4a563cd1a0",
	"ba0f91e20161af480776c24790538153",
	"5896ccae5cdc7abbee7d064f11bc55bf",
	"80565c134c28c8413cb8ee6217af15fd",
	"e1acab18892d012cf14d6acc69f660b5",
	"c05cf136e5f464daa2e03484d855913e",
	"20f32820fa8deb9e971f56778bc7ba96",
	"e9bbe21340ed0c828a6f2eb5c2b204e6",
	"27c5e8099070dfcbae31ba7f9ddd97e5",
	"87be085bd498c3f97e9bc55beed79683",
};

static auto		H264_420_8BIT_HIGH_PROFILE = VideoProfileInfo{VK_VIDEO_CODEC_OPERATION_DECODE_H264_BIT_KHR,
														  VK_VIDEO_CHROMA_SUBSAMPLING_420_BIT_KHR,
														  VK_VIDEO_COMPONENT_BIT_DEPTH_8_BIT_KHR,
														  VK_VIDEO_COMPONENT_BIT_DEPTH_8_BIT_KHR,
														  STD_VIDEO_H264_PROFILE_IDC_HIGH};
static auto		H264_420_8BIT_MAIN_PROFILE = VideoProfileInfo{VK_VIDEO_CODEC_OPERATION_DECODE_H264_BIT_KHR,
														  VK_VIDEO_CHROMA_SUBSAMPLING_420_BIT_KHR,
														  VK_VIDEO_COMPONENT_BIT_DEPTH_8_BIT_KHR,
														  VK_VIDEO_COMPONENT_BIT_DEPTH_8_BIT_KHR,
														  STD_VIDEO_H264_PROFILE_IDC_MAIN};
static auto		H265_420_8BIT_MAIN_PROFILE = VideoProfileInfo{VK_VIDEO_CODEC_OPERATION_DECODE_H265_BIT_KHR,
														  VK_VIDEO_CHROMA_SUBSAMPLING_420_BIT_KHR,
														  VK_VIDEO_COMPONENT_BIT_DEPTH_8_BIT_KHR,
														  VK_VIDEO_COMPONENT_BIT_DEPTH_8_BIT_KHR,
														  STD_VIDEO_H265_PROFILE_IDC_MAIN};

static ClipInfo Clips[] =
{
	{
		CLIP_A,
		"vulkan/video/clip-a.h264",
		H264_420_8BIT_HIGH_PROFILE,
		30,
		10,
		3,
		clipA,
	},
	{
		CLIP_B,
		"vulkan/video/clip-b.h264",
		H264_420_8BIT_HIGH_PROFILE,
		30,
		10,
		3,
		clipB,
	},
	{
		CLIP_C,
		"vulkan/video/clip-c.h264",
		H264_420_8BIT_HIGH_PROFILE,
		30,
		10,
		3,
		clipC,
	},
	{
		CLIP_D,
		"vulkan/video/clip-d.h265",
		H265_420_8BIT_MAIN_PROFILE,
		30,
		10,
		3,
		clipA,// same as clip A
	},
	{
		CLIP_H264_4K_26_IBP_MAIN,
		"vulkan/video/4k_26_ibp_main.h264",
		H264_420_8BIT_MAIN_PROFILE,
		26,
		13,
		2,
		h264_4k_26_ibp_main,
	},
	{
		CLIP_JELLY_HEVC,
		"vulkan/video/jellyfish-250-mbps-4k-uhd-GOB-IPB13.h265",
		H265_420_8BIT_MAIN_PROFILE,
		26,
		26,
		1,
		jellyfishHEVC,
	},
};

const ClipInfo* clipInfo(ClipName c)
{
	return &Clips[c];
}

const char* checksumForClipFrame(const ClipInfo* cinfo, int frameNumber)
{
	DE_ASSERT(frameNumber >= 0 && frameNumber < cinfo->totalFrames);
	return cinfo->frameChecksums[frameNumber];
}


} // namespace video
} // namespace vkt
