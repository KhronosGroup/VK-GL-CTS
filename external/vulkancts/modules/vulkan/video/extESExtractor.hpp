#ifndef _EXTESEXTRACTOR_HPP
#define _EXTESEXTRACTOR_HPP
/*
* Copyright (C) 2023 Igalia, S.L.
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*    http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

#include "esextractor.h"
#include "tcuTestLog.hpp"

class ESEDemuxer {
    ESExtractor *extractor{};
    ESEPacket* pkt{};
    ESEVideoCodec eVideoCodec{ESE_VIDEO_CODEC_UNKNOWN};

    tcu::TestLog& log;

public:
    ESEDemuxer(const std::string& filePath, tcu::TestLog& log_)
		: extractor(es_extractor_new(filePath.c_str(), "Alignment:NAL"))
		, log(log_)
	{
        eVideoCodec = es_extractor_video_codec(extractor);
        log << tcu::TestLog::Message << "ESEDemuxer found video codec: " << eVideoCodec << tcu::TestLog::EndMessage;
    }

    ~ESEDemuxer()
	{
        if (pkt) {
            es_extractor_clear_packet(pkt);
        }
        es_extractor_teardown(extractor);
    }

    ESEVideoCodec GetVideoCodec() {
        if (!extractor) {
            return ESE_VIDEO_CODEC_UNKNOWN;
        }
        return eVideoCodec;
    }

    bool Demux(deUint8 **ppVideo, deInt64 *pnVideoBytes) {
        if (!extractor) {
            return false;
        }

        *pnVideoBytes = 0;

        if (pkt) {
            es_extractor_clear_packet(pkt);
            pkt = nullptr;
        }

        int e = 0;
        e = es_extractor_read_packet(extractor, &pkt);

        if (e > ESE_RESULT_LAST_PACKET) {
            return false;
        }

        *ppVideo = pkt->data;
        *pnVideoBytes = static_cast<int>(pkt->data_size);

        return true;
    }
};

inline vk::VkVideoCodecOperationFlagBitsKHR EXExtractor2NvCodecId(ESEVideoCodec id) {
    switch (id) {
        case ESE_VIDEO_CODEC_H264       : return vk::VK_VIDEO_CODEC_OPERATION_DECODE_H264_BIT_KHR;
        case ESE_VIDEO_CODEC_H265       : return vk::VK_VIDEO_CODEC_OPERATION_DECODE_H265_BIT_KHR;
        default                     : /* assert(false); */ return vk::VkVideoCodecOperationFlagBitsKHR(0);
    }
}
#endif // _EXTESEXTRACTOR_HPP
