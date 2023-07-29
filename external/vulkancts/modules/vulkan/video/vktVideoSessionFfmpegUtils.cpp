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
 * \brief Video Session Ffmpeg Utils
 *//*--------------------------------------------------------------------*/

#include "vktVideoSessionFfmpegUtils.hpp"
#include "vkDefs.hpp"
#include "tcuPlatform.hpp"
#include "tcuFunctionLibrary.hpp"

#include "extFFmpegDemuxer.h"
#include "deMemory.h"

#if (DE_OS == DE_OS_WIN32)
#define FFMPEG_AVCODEC_LIBRARY_NAME "avcodec-58.dll"
#define FFMPEG_AVFORMAT_LIBRARY_NAME "avformat-58.dll"
#define FFMPEG_AVUTIL_LIBRARY_NAME "avutil-56.dll"
#else
#define FFMPEG_AVCODEC_LIBRARY_NAME "libavcodec.so"
#define FFMPEG_AVFORMAT_LIBRARY_NAME "libavformat.so"
#define FFMPEG_AVUTIL_LIBRARY_NAME "libavutil.so"
#endif

namespace vkt
{
namespace video
{

class DataProviderImpl : public FFmpegDemuxer::DataProvider
{
public:
    DataProviderImpl(de::MovePtr<std::vector<uint8_t>> &data);

    virtual ~DataProviderImpl(){};
    virtual int GetData(uint8_t *pBuf, int nBuf);
    virtual size_t GetDataSize(void)
    {
        return m_data->size();
    };

    de::MovePtr<std::vector<uint8_t>> m_data;
    size_t m_used;
};

DataProviderImpl::DataProviderImpl(de::MovePtr<std::vector<uint8_t>> &data) : m_data(data), m_used(0)
{
}

int DataProviderImpl::GetData(uint8_t *pBuf, int nBuf)
{
    if (nBuf <= 0 || pBuf == DE_NULL)
        return 0;

    const size_t left      = m_data->size() - m_used;
    const size_t requested = static_cast<size_t>(nBuf);
    const size_t size      = requested < left ? requested : left;

    if (size > 0)
    {
        const std::vector<uint8_t> &data = *m_data;

        deMemcpy(pBuf, &data[m_used], size);

        m_used += size;
    }

    return static_cast<int>(size);
}

class FfmpegDemuxerImpl : public IfcFfmpegDemuxer
{
public:
    FfmpegDemuxerImpl(de::MovePtr<std::vector<uint8_t>> &data, FFMpegAPI &api);
    virtual ~FfmpegDemuxerImpl(){};
    virtual bool demux(uint8_t **pData, int64_t *pSize);

protected:
    DataProviderImpl m_dataProvider;
    de::MovePtr<FFmpegDemuxer> m_FFmpegDemuxer;
};

FfmpegDemuxerImpl::FfmpegDemuxerImpl(de::MovePtr<std::vector<uint8_t>> &data, FFMpegAPI &api)
    : m_dataProvider(data)
    , m_FFmpegDemuxer(new FFmpegDemuxer(&m_dataProvider, &api))
{
}

bool FfmpegDemuxerImpl::demux(uint8_t **pData, int64_t *pSize)
{
    int size    = 0;
    bool result = m_FFmpegDemuxer->Demux((uint8_t **)pData, &size);

    if (pSize != DE_NULL)
        *pSize = static_cast<int64_t>(size);

    return result;
}

class FfmpegFunctionsImpl : public IfcFfmpegFunctions
{
public:
    FfmpegFunctionsImpl();
    virtual IfcFfmpegDemuxer *createIfcFfmpegDemuxer(de::MovePtr<std::vector<uint8_t>> &data);

protected:
    tcu::DynamicFunctionLibrary m_functions_avcodec;
    tcu::DynamicFunctionLibrary m_functions_avformat;
    tcu::DynamicFunctionLibrary m_functions_avutil;

    FFMpegAPI m_FFmpegApi;
};

// The following DLLs should be same folder as deqp-vk.exe: avcodec-59.dll avdevice-59.dll avfilter-8.dll avformat-59.dll avutil-57.dll swresample-4.dll swscale-6.dll
FfmpegFunctionsImpl::FfmpegFunctionsImpl()
    : m_functions_avcodec(FFMPEG_AVCODEC_LIBRARY_NAME)
    , m_functions_avformat(FFMPEG_AVFORMAT_LIBRARY_NAME)
    , m_functions_avutil(FFMPEG_AVUTIL_LIBRARY_NAME)
{
    m_FFmpegApi = {
        (pFFMpeg_av_malloc)m_functions_avutil.getFunction("av_malloc"), //  pFFMpeg_av_malloc av_malloc;
        (pFFMpeg_av_freep)m_functions_avutil.getFunction("av_freep"),   //  pFFMpeg_av_freep av_freep;
        (pFFMpeg_av_init_packet)m_functions_avcodec.getFunction(
            "av_init_packet"), //  pFFMpeg_av_init_packet av_init_packet;
        (pFFMpeg_av_packet_unref)m_functions_avcodec.getFunction(
            "av_packet_unref"), //  pFFMpeg_av_packet_unref av_packet_unref;
        (pFFMpeg_av_bsf_init)m_functions_avcodec.getFunction("av_bsf_init"), //  pFFMpeg_av_bsf_init av_bsf_init;
        (pFFMpeg_av_bsf_send_packet)m_functions_avcodec.getFunction(
            "av_bsf_send_packet"), //  pFFMpeg_av_bsf_send_packet av_bsf_send_packet;
        (pFFMpeg_av_bsf_receive_packet)m_functions_avcodec.getFunction(
            "av_bsf_receive_packet"), //  pFFMpeg_av_bsf_receive_packet av_bsf_receive_packet;
        (pFFMpeg_av_bsf_get_by_name)m_functions_avcodec.getFunction(
            "av_bsf_get_by_name"), //  pFFMpeg_av_bsf_get_by_name av_bsf_get_by_name;
        (pFFMpeg_av_bsf_alloc)m_functions_avcodec.getFunction("av_bsf_alloc"), //  pFFMpeg_av_bsf_alloc av_bsf_alloc;
        (pFFMpeg_avio_alloc_context)m_functions_avformat.getFunction(
            "avio_alloc_context"), //  pFFMpeg_avio_alloc_context avio_alloc_context;
        (pFFMpeg_av_find_best_stream)m_functions_avformat.getFunction(
            "av_find_best_stream"), //  pFFMpeg_av_find_best_stream av_find_best_stream;
        (pFFMpeg_av_read_frame)m_functions_avformat.getFunction(
            "av_read_frame"), //  pFFMpeg_av_read_frame av_read_frame;
        (pFFMpeg_avformat_alloc_context)m_functions_avformat.getFunction(
            "avformat_alloc_context"), //  pFFMpeg_avformat_alloc_context avformat_alloc_context;
        (pFFMpeg_avformat_network_init)m_functions_avformat.getFunction(
            "avformat_network_init"), //  pFFMpeg_avformat_network_init avformat_network_init;
        (pFFMpeg_avformat_find_stream_info)m_functions_avformat.getFunction(
            "avformat_find_stream_info"), //  pFFMpeg_avformat_find_stream_info avformat_find_stream_info;
        (pFFMpeg_avformat_open_input)m_functions_avformat.getFunction(
            "avformat_open_input"), //  pFFMpeg_avformat_open_input avformat_open_input;
        (pFFMpeg_avformat_close_input)m_functions_avformat.getFunction(
            "avformat_close_input"), //  pFFMpeg_avformat_close_input avformat_close_input;
    };

    for (size_t i = 0; i < sizeof(m_FFmpegApi) / sizeof(void *); i++)
    {
        const void *p = (void *)((void **)&m_FFmpegApi)[i];

        DE_ASSERT(p != DE_NULL);

        DE_UNREF(p);
    }
}

IfcFfmpegDemuxer *FfmpegFunctionsImpl::createIfcFfmpegDemuxer(de::MovePtr<std::vector<uint8_t>> &data)
{
    return new FfmpegDemuxerImpl(data, m_FFmpegApi);
}

de::MovePtr<IfcFfmpegFunctions> createIfcFfmpegFunctions()
{
    return de::MovePtr<IfcFfmpegFunctions>(new FfmpegFunctionsImpl());
}

} // namespace video
} // namespace vkt
