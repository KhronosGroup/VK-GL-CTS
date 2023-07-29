#ifndef _EXTFFMPEGDEMUXER_H
#define _EXTFFMPEGDEMUXER_H
/*
 * Copyright 2017-2018 NVIDIA Corporation.  All rights reserved.
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

#if ((DE_COMPILER == DE_COMPILER_CLANG || DE_COMPILER == DE_COMPILER_GCC))
#define DISABLE_DEPRECATIONS
#endif

#ifdef DISABLE_DEPRECATIONS
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#pragma GCC diagnostic ignored "-Wconversion"
#endif

extern "C"
{
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#include <libavcodec/avcodec.h>
}

#ifdef DISABLE_DEPRECATIONS
#pragma GCC diagnostic pop
#endif

#include <deDefs.h>

#include <iostream>
#include <tcuDefs.hpp>

#define LOG(X) std::ostream(0)

inline bool check(int e, int iLine, const char *szFile)
{
    if (e < 0)
    {
        LOG(ERROR) << "General error " << e << " at line " << iLine << " in file " << szFile;
        return false;
    }
    return true;
}

#define ck(call) check(call, __LINE__, __FILE__)

typedef void *(*pFFMpeg_av_malloc)(size_t size);

typedef void (*pFFMpeg_av_freep)(void *ptr);

typedef void (*pFFMpeg_av_init_packet)(AVPacket *pkt);

typedef void (*pFFMpeg_av_packet_unref)(AVPacket *pkt);

typedef int (*pFFMpeg_av_bsf_init)(AVBSFContext *ctx);

typedef int (*pFFMpeg_av_bsf_send_packet)(AVBSFContext *ctx, AVPacket *pkt);

typedef int (*pFFMpeg_av_bsf_receive_packet)(AVBSFContext *ctx, AVPacket *pkt);

typedef const AVBitStreamFilter *(*pFFMpeg_av_bsf_get_by_name)(const char *name);

typedef int (*pFFMpeg_av_bsf_alloc)(const AVBitStreamFilter *filter, AVBSFContext **ctx);

typedef AVIOContext *(*pFFMpeg_avio_alloc_context)(unsigned char *buffer, int buffer_size, int write_flag, void *opaque,
                                                   int (*read_packet)(void *opaque, uint8_t *buf, int buf_size),
                                                   int (*write_packet)(void *opaque, uint8_t *buf, int buf_size),
                                                   int64_t (*seek)(void *opaque, int64_t offset, int whence));

typedef int (*pFFMpeg_av_find_best_stream)(AVFormatContext *ic, enum AVMediaType type, int wanted_stream_nb,
                                           int related_stream, const AVCodec **decoder_ret, int flags);

typedef int (*pFFMpeg_av_read_frame)(AVFormatContext *s, AVPacket *pkt);

typedef AVFormatContext *(*pFFMpeg_avformat_alloc_context)(void);

typedef int (*pFFMpeg_avformat_network_init)(void);

typedef int (*pFFMpeg_avformat_find_stream_info)(AVFormatContext *ic, AVDictionary **options);

typedef int (*pFFMpeg_avformat_open_input)(AVFormatContext **ps, const char *url, const AVInputFormat *fmt,
                                           AVDictionary **options);

typedef void (*pFFMpeg_avformat_close_input)(AVFormatContext **s);

typedef struct
{
    pFFMpeg_av_malloc av_malloc;
    pFFMpeg_av_freep av_freep;
    pFFMpeg_av_init_packet av_init_packet;
    pFFMpeg_av_packet_unref av_packet_unref;
    pFFMpeg_av_bsf_init av_bsf_init;
    pFFMpeg_av_bsf_send_packet av_bsf_send_packet;
    pFFMpeg_av_bsf_receive_packet av_bsf_receive_packet;
    pFFMpeg_av_bsf_get_by_name av_bsf_get_by_name;
    pFFMpeg_av_bsf_alloc av_bsf_alloc;
    pFFMpeg_avio_alloc_context avio_alloc_context;
    pFFMpeg_av_find_best_stream av_find_best_stream;
    pFFMpeg_av_read_frame av_read_frame;
    pFFMpeg_avformat_alloc_context avformat_alloc_context;
    pFFMpeg_avformat_network_init avformat_network_init;
    pFFMpeg_avformat_find_stream_info avformat_find_stream_info;
    pFFMpeg_avformat_open_input avformat_open_input;
    pFFMpeg_avformat_close_input avformat_close_input;
} FFMpegAPI;

class FFmpegDemuxer
{
private:
    AVFormatContext *fmtc = NULL;
    FFMpegAPI *api        = NULL;
    AVIOContext *avioc    = NULL;
    AVPacket pkt, pktFiltered;
    AVBSFContext *bsfc = NULL;

    int iVideoStream;
    bool bMp4;
    AVCodecID eVideoCodec;
    int nWidth, nHeight, nBitDepth;

    AVPixelFormat format;
    /**
     * Codec-specific bitstream restrictions that the stream conforms to.
     */
    int profile;
    int level;

    /**
     * Video only. The aspect ratio (width / height) which a single pixel
     * should have when displayed.
     *
     * When the aspect ratio is unknown / undefined, the numerator should be
     * set to 0 (the denominator may have any value).
     */
    AVRational sample_aspect_ratio;

    /**
     * Video only. The order of the fields in interlaced video.
     */
    enum AVFieldOrder field_order;

    /**
     * Video only. Additional colorspace characteristics.
     */
    enum AVColorRange color_range;
    enum AVColorPrimaries color_primaries;
    enum AVColorTransferCharacteristic color_trc;
    enum AVColorSpace color_space;
    enum AVChromaLocation chroma_location;

public:
    class DataProvider
    {
    public:
        virtual ~DataProvider()
        {
        }
        virtual int GetData(uint8_t *pBuf, int nBuf) = 0;
    };

private:
    FFmpegDemuxer(AVFormatContext *fmtc_, FFMpegAPI *api_) : fmtc(fmtc_), api(api_)
    {
        if (!fmtc)
        {
            LOG(ERROR) << "No AVFormatContext provided.";
            return;
        }

        LOG(INFO) << "Media format: " << fmtc->iformat->long_name << " (" << fmtc->iformat->name << ")";

        ck(api->avformat_find_stream_info(fmtc, NULL));
        iVideoStream = api->av_find_best_stream(fmtc, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
        if (iVideoStream < 0)
        {
            LOG(ERROR) << "FFmpeg error: " << __FILE__ << " " << __LINE__ << " "
                       << "Could not find stream in input file";
            return;
        }

        //fmtc->streams[iVideoStream]->need_parsing = AVSTREAM_PARSE_NONE;
        eVideoCodec = fmtc->streams[iVideoStream]->codecpar->codec_id;
        nWidth      = fmtc->streams[iVideoStream]->codecpar->width;
        nHeight     = fmtc->streams[iVideoStream]->codecpar->height;
        format      = (AVPixelFormat)fmtc->streams[iVideoStream]->codecpar->format;
        nBitDepth   = 8;
        if (fmtc->streams[iVideoStream]->codecpar->format == AV_PIX_FMT_YUV420P10LE)
            nBitDepth = 10;
        if (fmtc->streams[iVideoStream]->codecpar->format == AV_PIX_FMT_YUV420P12LE)
            nBitDepth = 12;

        bMp4 = (!strcmp(fmtc->iformat->long_name, "QuickTime / MOV") ||
                !strcmp(fmtc->iformat->long_name, "FLV (Flash Video)") ||
                !strcmp(fmtc->iformat->long_name, "Matroska / WebM"));

        /**
         * Codec-specific bitstream restrictions that the stream conforms to.
         */
        profile = fmtc->streams[iVideoStream]->codecpar->profile;
        level   = fmtc->streams[iVideoStream]->codecpar->level;

        /**
         * Video only. The aspect ratio (width / height) which a single pixel
         * should have when displayed.
         *
         * When the aspect ratio is unknown / undefined, the numerator should be
         * set to 0 (the denominator may have any value).
         */
        sample_aspect_ratio = fmtc->streams[iVideoStream]->codecpar->sample_aspect_ratio;

        /**
         * Video only. The order of the fields in interlaced video.
         */
        field_order = fmtc->streams[iVideoStream]->codecpar->field_order;

        /**
         * Video only. Additional colorspace characteristics.
         */
        color_range     = fmtc->streams[iVideoStream]->codecpar->color_range;
        color_primaries = fmtc->streams[iVideoStream]->codecpar->color_primaries;
        color_trc       = fmtc->streams[iVideoStream]->codecpar->color_trc;
        color_space     = fmtc->streams[iVideoStream]->codecpar->color_space;
        chroma_location = fmtc->streams[iVideoStream]->codecpar->chroma_location;

        api->av_init_packet(&pkt);
        pkt.data = NULL;
        pkt.size = 0;
        api->av_init_packet(&pktFiltered);
        pktFiltered.data = NULL;
        pktFiltered.size = 0;

        if (bMp4)
        {
            const AVBitStreamFilter *bsf = NULL;

            if (eVideoCodec == AV_CODEC_ID_H264)
            {
                bsf = api->av_bsf_get_by_name("h264_mp4toannexb");
            }
            else if (eVideoCodec == AV_CODEC_ID_HEVC)
            {
                bsf = api->av_bsf_get_by_name("hevc_mp4toannexb");
            }

            if (!bsf)
            {
                LOG(ERROR) << "FFmpeg error: " << __FILE__ << " " << __LINE__ << " "
                           << "av_bsf_get_by_name(): " << eVideoCodec << " failed";
                return;
            }
            ck(api->av_bsf_alloc(bsf, &bsfc));
            bsfc->par_in = fmtc->streams[iVideoStream]->codecpar;
            ck(api->av_bsf_init(bsfc));
        }
    }

    AVFormatContext *CreateFormatContext(DataProvider *pDataProvider, FFMpegAPI *api_)
    {
        api = api_;

        AVFormatContext *ctx = NULL;
        if (!(ctx = api->avformat_alloc_context()))
        {
            LOG(ERROR) << "FFmpeg error: " << __FILE__ << " " << __LINE__;
            return NULL;
        }

        uint8_t *avioc_buffer = NULL;
        int avioc_buffer_size = 8 * 1024 * 1024;
        avioc_buffer          = (uint8_t *)api->av_malloc(avioc_buffer_size);
        if (!avioc_buffer)
        {
            LOG(ERROR) << "FFmpeg error: " << __FILE__ << " " << __LINE__;
            return NULL;
        }
        avioc = api->avio_alloc_context(avioc_buffer, avioc_buffer_size, 0, pDataProvider, &ReadPacket, NULL, NULL);
        if (!avioc)
        {
            LOG(ERROR) << "FFmpeg error: " << __FILE__ << " " << __LINE__;
            return NULL;
        }
        ctx->pb = avioc;

        ck(api->avformat_open_input(&ctx, NULL, NULL, NULL));
        return ctx;
    }

    AVFormatContext *CreateFormatContext(const char *szFilePath, FFMpegAPI *api_)
    {
        api = api_;

        api->avformat_network_init();

        AVFormatContext *ctx = NULL;
        ck(api->avformat_open_input(&ctx, szFilePath, NULL, NULL));
        return ctx;
    }

public:
    FFmpegDemuxer(const char *szFilePath, FFMpegAPI *api_) : FFmpegDemuxer(CreateFormatContext(szFilePath, api_), api_)
    {
    }
    FFmpegDemuxer(DataProvider *pDataProvider, FFMpegAPI *api_)
        : FFmpegDemuxer(CreateFormatContext(pDataProvider, api_), api_)
    {
    }
    ~FFmpegDemuxer()
    {
        if (pkt.data)
        {
            api->av_packet_unref(&pkt);
        }
        if (pktFiltered.data)
        {
            api->av_packet_unref(&pktFiltered);
        }

        api->avformat_close_input(&fmtc);
        if (avioc)
        {
            api->av_freep(&avioc->buffer);
            api->av_freep(&avioc);
        }
    }
    AVCodecID GetVideoCodec()
    {
        return eVideoCodec;
    }
    int GetWidth()
    {
        return nWidth;
    }
    int GetHeight()
    {
        return nHeight;
    }
    int GetBitDepth()
    {
        return nBitDepth;
    }
    int GetFrameSize()
    {
        return nBitDepth == 8 ? nWidth * nHeight * 3 / 2 : nWidth * nHeight * 3;
    }
    bool Demux(uint8_t **ppVideo, int *pnVideoBytes)
    {
        if (!fmtc)
        {
            return false;
        }

        *pnVideoBytes = 0;

        if (pkt.data)
        {
            api->av_packet_unref(&pkt);
        }

        int e = 0;
        while ((e = api->av_read_frame(fmtc, &pkt)) >= 0 && pkt.stream_index != iVideoStream)
        {
            api->av_packet_unref(&pkt);
        }
        if (e < 0)
        {
            return false;
        }

        if (bMp4)
        {
            if (pktFiltered.data)
            {
                api->av_packet_unref(&pktFiltered);
            }
            ck(api->av_bsf_send_packet(bsfc, &pkt));
            ck(api->av_bsf_receive_packet(bsfc, &pktFiltered));
            *ppVideo      = pktFiltered.data;
            *pnVideoBytes = pktFiltered.size;
        }
        else
        {
            *ppVideo      = pkt.data;
            *pnVideoBytes = pkt.size;
        }

        return true;
    }

    static int ReadPacket(void *opaque, uint8_t *pBuf, int nBuf)
    {
        return ((DataProvider *)opaque)->GetData(pBuf, nBuf);
    }

    void DumpStreamParameters()
    {

        LOG(INFO) << "Width: " << nWidth << std::endl;
        LOG(INFO) << "Height: " << nHeight << std::endl;
        LOG(INFO) << "BitDepth: " << nBitDepth << std::endl;
        LOG(INFO) << "Profile: " << profile << std::endl;
        LOG(INFO) << "Level: " << level << std::endl;
        LOG(INFO) << "Aspect Ration: " << (float)sample_aspect_ratio.num / (float)sample_aspect_ratio.den << std::endl;

        static const char *FieldOrder[] = {
            "UNKNOWN",
            "PROGRESSIVE",
            "TT: Top coded_first, top displayed first",
            "BB: Bottom coded first, bottom displayed first",
            "TB: Top coded first, bottom displayed first",
            "BT: Bottom coded first, top displayed first",
        };
        LOG(INFO) << "Field Order: " << FieldOrder[field_order] << std::endl;

        static const char *ColorRange[] = {
            "UNSPECIFIED",
            "MPEG: the normal 219*2^(n-8) MPEG YUV ranges",
            "JPEG: the normal     2^n-1   JPEG YUV ranges",
            "NB: Not part of ABI",
        };
        LOG(INFO) << "Color Range: " << ColorRange[color_range] << std::endl;

        static const char *ColorPrimaries[] = {
            "RESERVED0",
            "BT709: also ITU-R BT1361 / IEC 61966-2-4 / SMPTE RP177 Annex B",
            "UNSPECIFIED",
            "RESERVED",
            "BT470M: also FCC Title 47 Code of Federal Regulations 73.682 (a)(20)",

            "BT470BG: also ITU-R BT601-6 625 / ITU-R BT1358 625 / ITU-R BT1700 625 PAL & SECAM",
            "SMPTE170M: also ITU-R BT601-6 525 / ITU-R BT1358 525 / ITU-R BT1700 NTSC",
            "SMPTE240M: also ITU-R BT601-6 525 / ITU-R BT1358 525 / ITU-R BT1700 NTSC",
            "FILM: colour filters using Illuminant C",
            "BT2020: ITU-R BT2020",
            "SMPTE428: SMPTE ST 428-1 (CIE 1931 XYZ)",
            "SMPTE431: SMPTE ST 431-2 (2011) / DCI P3",
            "SMPTE432: SMPTE ST 432-1 (2010) / P3 D65 / Display P3",
            "JEDEC_P22: JEDEC P22 phosphors",
            "NB: Not part of ABI",
        };
        LOG(INFO) << "Color Primaries: " << ColorPrimaries[color_primaries] << std::endl;

        static const char *ColorTransferCharacteristic[] = {
            "RESERVED0",
            "BT709: also ITU-R BT1361",
            "UNSPECIFIED",
            "RESERVED",
            "GAMMA22:  also ITU-R BT470M / ITU-R BT1700 625 PAL & SECAM",
            "GAMMA28:  also ITU-R BT470BG",
            "SMPTE170M:  also ITU-R BT601-6 525 or 625 / ITU-R BT1358 525 or 625 / ITU-R BT1700 NTSC",
            "SMPTE240M",
            "LINEAR:  Linear transfer characteristics",
            "LOG: Logarithmic transfer characteristic (100:1 range)",
            "LOG_SQRT: Logarithmic transfer characteristic (100 * Sqrt(10) : 1 range)",
            "IEC61966_2_4: IEC 61966-2-4",
            "BT1361_ECG: ITU-R BT1361 Extended Colour Gamut",
            "IEC61966_2_1: IEC 61966-2-1 (sRGB or sYCC)",
            "BT2020_10: ITU-R BT2020 for 10-bit system",
            "BT2020_12: ITU-R BT2020 for 12-bit system",
            "SMPTE2084: SMPTE ST 2084 for 10-, 12-, 14- and 16-bit systems",
            "SMPTE428:  SMPTE ST 428-1",
            "ARIB_STD_B67:  ARIB STD-B67, known as Hybrid log-gamma",
            "NB: Not part of ABI",
        };
        LOG(INFO) << "Color Transfer Characteristic: " << ColorTransferCharacteristic[color_trc] << std::endl;

        static const char *ColorSpace[] = {
            "RGB:   order of coefficients is actually GBR, also IEC 61966-2-1 (sRGB)",
            "BT709:   also ITU-R BT1361 / IEC 61966-2-4 xvYCC709 / SMPTE RP177 Annex B",
            "UNSPECIFIED",
            "RESERVED",
            "FCC:  FCC Title 47 Code of Federal Regulations 73.682 (a)(20)",
            ("BT470BG:  also ITU-R BT601-6 625 / ITU-R BT1358 625 / ITU-R BT1700 625 PAL & SECAM / IEC 61966-2-4 "
             "xvYCC601"),
            "SMPTE170M:  also ITU-R BT601-6 525 / ITU-R BT1358 525 / ITU-R BT1700 NTSC",
            "SMPTE240M:  functionally identical to above",
            "YCGCO:  Used by Dirac / VC-2 and H.264 FRext, see ITU-T SG16",
            "BT2020_NCL:  ITU-R BT2020 non-constant luminance system",
            "BT2020_CL:  ITU-R BT2020 constant luminance system",
            "SMPTE2085:  SMPTE 2085, Y'D'zD'x",
            "CHROMA_DERIVED_NCL:  Chromaticity-derived non-constant luminance system",
            "CHROMA_DERIVED_CL:  Chromaticity-derived constant luminance system",
            "ICTCP:  ITU-R BT.2100-0, ICtCp",
            "NB:  Not part of ABI",
        };
        LOG(INFO) << "Color Space: " << ColorSpace[color_space] << std::endl;

        static const char *ChromaLocation[] = {
            "UNSPECIFIED",
            "LEFT: MPEG-2/4 4:2:0, H.264 default for 4:2:0",
            "CENTER: MPEG-1 4:2:0, JPEG 4:2:0, H.263 4:2:0",
            "TOPLEFT: ITU-R 601, SMPTE 274M 296M S314M(DV 4:1:1), mpeg2 4:2:2",
            "TOP",
            "BOTTOMLEFT",
            "BOTTOM",
            "NB:Not part of ABI",
        };
        LOG(INFO) << "Chroma Location: " << ChromaLocation[chroma_location] << std::endl;
    }
};

inline vk::VkVideoCodecOperationFlagBitsKHR FFmpeg2NvCodecId(AVCodecID id)
{
    switch (id)
    {
    case AV_CODEC_ID_MPEG1VIDEO: /* assert(false); */
        return vk::VkVideoCodecOperationFlagBitsKHR(0);
    case AV_CODEC_ID_MPEG2VIDEO: /* assert(false); */
        return vk::VkVideoCodecOperationFlagBitsKHR(0);
    case AV_CODEC_ID_MPEG4: /* assert(false); */
        return vk::VkVideoCodecOperationFlagBitsKHR(0);
    case AV_CODEC_ID_VC1: /* assert(false); */
        return vk::VkVideoCodecOperationFlagBitsKHR(0);
    case AV_CODEC_ID_H264:
        return vk::VK_VIDEO_CODEC_OPERATION_DECODE_H264_BIT_KHR;
    case AV_CODEC_ID_HEVC:
        return vk::VK_VIDEO_CODEC_OPERATION_DECODE_H265_BIT_KHR;
    case AV_CODEC_ID_VP8: /* assert(false); */
        return vk::VkVideoCodecOperationFlagBitsKHR(0);
#ifdef VK_EXT_video_decode_vp9
    case AV_CODEC_ID_VP9:
        return VK_VIDEO_CODEC_OPERATION_DECODE_VP9_BIT_KHR;
#endif                      // VK_EXT_video_decode_vp9
    case AV_CODEC_ID_MJPEG: /* assert(false); */
        return vk::VkVideoCodecOperationFlagBitsKHR(0);
    default: /* assert(false); */
        return vk::VkVideoCodecOperationFlagBitsKHR(0);
    }
}
#endif /* _EXTFFMPEGDEMUXER_H */
