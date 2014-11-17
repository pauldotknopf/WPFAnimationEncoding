#pragma once

#include "Stdafx.h"

namespace libffmpeg
{
	extern "C"
	{
		// disable warnings about badly formed documentation from FFmpeg, which we don't need at all
		#pragma warning(disable:4635) 
		// disable warning about conversion int64 to int32
		#pragma warning(disable:4244) 

		#include "libavformat\avformat.h"
		#include "libavformat\avio.h"
		#include "libavcodec\avcodec.h"
		#include "libswscale\swscale.h"
		#include "libavutil\imgutils.h"

		#include "libavutil\timestamp.h"
		#undef PixelFormat
	}

	#undef av_ts2str
	#define av_ts2str(ts) av_ts_make_string(new char[AV_TS_MAX_STRING_SIZE], ts)

	#undef av_ts2timestr
	#define av_ts2timestr(ts, tb) av_ts_make_time_string(new char[AV_TS_MAX_STRING_SIZE], ts, tb)

	static void log_packet(const AVFormatContext *fmt_ctx, const AVPacket *pkt)
	{
		AVRational *time_base = &fmt_ctx->streams[pkt->stream_index]->time_base;

		char * param1 = av_ts2str(pkt->pts);
		char * param2 = av_ts2timestr(pkt->pts, time_base);
		char * param3 = av_ts2str(pkt->dts);
		char * param4 = av_ts2timestr(pkt->dts, time_base);
		char * param5 = av_ts2str(pkt->duration);
		char * param6 = av_ts2timestr(pkt->duration, time_base);

		printf("pts:%s pts_time:%s dts:%s dts_time:%s duration:%s duration_time:%s stream_index:%d\n",
			param1, param2,
			param3, param4,
			param5, param6,
			pkt->stream_index);

		delete param1;
		delete param2;
		delete param3;
		delete param4;
		delete param5;
		delete param6;
	}
}