#include "stdafx.h"
#include "VideoReEncoder.h"
#include "Exceptions.h"

namespace WPFAnimationEncoding
{
	struct VideoReEncoderSession
	{
		VideoReEncoderSession() : 
			inputFormatContext(NULL), 
			inputCodecContext(NULL), 
			inputVideoStream(NULL),
			outputFormatContext(NULL),
			codec(NULL),
			videoFrame(NULL),
			convertContext(NULL)
		{
			packet.data = NULL;
			packet.size = 0;
		}
		// input
		libffmpeg::AVFormatContext *inputFormatContext;
		libffmpeg::AVCodecContext *inputCodecContext;
		libffmpeg::AVStream *inputVideoStream;
		// output
		libffmpeg::AVFormatContext *outputFormatContext;
		// general
		libffmpeg::AVCodec *codec;
		libffmpeg::AVFrame *videoFrame;
		libffmpeg::SwsContext *convertContext;
		libffmpeg::AVPacket packet;
	};

	static void open_video(VideoReEncoderSession* session, char* fileName)
	{
		int result = libffmpeg::avformat_open_input(&session->inputFormatContext, fileName, NULL, NULL);
		if (result != 0)
			throw gcnew VideoException("Input avformat_alloc_output_context2 error " + result);
		if (!session->inputFormatContext)
			throw gcnew VideoException("Couldn't create input format context.");

		// retrieve stream information
		if (libffmpeg::avformat_find_stream_info(session->inputFormatContext, NULL) < 0)
			throw gcnew VideoException("Cannot find stream information.");
	}

	static void find_input_video_stream(VideoReEncoderSession* session)
	{
		// search for the first video stream
		for (unsigned int i = 0; i < session->inputFormatContext->nb_streams; i++)
		{
			if (session->inputFormatContext->streams[i]->codec->codec_type == libffmpeg::AVMEDIA_TYPE_VIDEO)
			{
				// get the pointer to the codec context for the video stream
				session->inputCodecContext = session->inputFormatContext->streams[i]->codec;
				session->inputVideoStream = session->inputFormatContext->streams[i];
				break;
			}
		}
		if (session->inputVideoStream == NULL)
			throw gcnew VideoException("Cannot find video stream in the specified file.");
		if (session->inputCodecContext == NULL)
			throw gcnew VideoException("Cannot create the input video codec context.");
	}

	static void open_input_video_codec()
	{

	}

	// Class constructor
	VideoReEncoder::VideoReEncoder(void) :
		disposed(false)
	{
		libffmpeg::av_register_all();
	}

	// start the reencoding of the video file
	void VideoReEncoder::StartReEncoding(String^ inputFileName, String^ outputFileName, VideoReEncodeCallback^ callback)
	{
		// http://ffmpeg.org/doxygen/trunk/doc_2examples_2transcoding_8c-example.html

		IntPtr nativeInputFileNamePointer = System::Runtime::InteropServices::Marshal::StringToHGlobalAnsi(inputFileName);
		char* nativeInputFileName = static_cast<char*>(nativeInputFileNamePointer.ToPointer());

		IntPtr nativeOutputFileNamePointer = System::Runtime::InteropServices::Marshal::StringToHGlobalAnsi(outputFileName);
		char* nativeOutputFileName = static_cast<char*>(nativeOutputFileNamePointer.ToPointer());

		int result = -1;
		VideoReEncoderSession session;

		try
		{
			open_video(&session, nativeInputFileName);

			find_input_video_stream(&session);

			// find decoder for the video stream
			session.codec = libffmpeg::avcodec_find_decoder(session.inputCodecContext->codec_id);
			if (session.codec == NULL)
				throw gcnew VideoException("Cannot find codec to decode the video stream with");

			// open the codec
			if (libffmpeg::avcodec_open2(session.inputCodecContext, session.codec, NULL) < 0)
				throw gcnew VideoException("Cannot open video codec.");

			// allocate video frame
			session.videoFrame = libffmpeg::avcodec_alloc_frame();
			if (session.videoFrame == NULL)
				throw gcnew VideoException("Couldn't allocate the video frame.");

			// prepare scaling context to convert RGB image to video format
			session.convertContext = libffmpeg::sws_getContext(session.inputCodecContext->width, session.inputCodecContext->height, session.inputCodecContext->pix_fmt,
				session.inputCodecContext->width, session.inputCodecContext->height, libffmpeg::PIX_FMT_BGR24, SWS_BICUBIC, NULL, NULL, NULL);
			if (session.convertContext == NULL)
				throw gcnew VideoException("Cannot initialize frames conversion context.");

			// open the output file
			result = libffmpeg::avformat_alloc_output_context2(&session.outputFormatContext, NULL, NULL, nativeOutputFileName);
			if (result != 0)
				throw gcnew VideoException("Output avformat_alloc_output_context2 error " + result);
			if (!session.inputFormatContext)
				throw gcnew VideoException("Couldn't create output format context.");

			// copy all the stream information from the input file, to the output file
			for (int i = 0; i < session.inputFormatContext->nb_streams; i++)
			{
				if (i <= 1){
					// create the new stream
					libffmpeg::AVStream *outStream = libffmpeg::avformat_new_stream(session.outputFormatContext, NULL);
					if (!outStream)
						throw gcnew VideoException("Failed allocating output stream");

					// cope all the information about the stream from the input, to the output
					result = libffmpeg::avcodec_copy_context(session.outputFormatContext->streams[i]->codec,
						session.inputFormatContext->streams[i]->codec);
					if (result < 0)
						throw gcnew VideoException("Copying stream context failed. " + result);
				}
			}

			// init muxer and write output file header
			if (!(session.outputFormatContext->oformat->flags & AVFMT_NOFILE)) {
				result = libffmpeg::avio_open(&session.outputFormatContext->pb, nativeOutputFileName, AVIO_FLAG_WRITE);
				if (result < 0)
					throw gcnew VideoException("Could not open output file. " + result);
			}
			result = libffmpeg::avformat_write_header(session.outputFormatContext, NULL);
			if (result < 0)
				throw gcnew VideoException("Error occurred when opening output file. " + result);

			bool hasFrame = true;

			while (hasFrame) {
				hasFrame = libffmpeg::av_read_frame(session.inputFormatContext, &session.packet) == 0;
				if (hasFrame) {

					//type = data->FormatContext->streams[packet.stream_index]->codec->codec_type;

					/* remux this frame without reencoding */
					session.packet.dts = libffmpeg::av_rescale_q_rnd(session.packet.dts,
						session.inputFormatContext->streams[session.packet.stream_index]->time_base,
						session.outputFormatContext->streams[session.packet.stream_index]->time_base,
						(libffmpeg::AVRounding)(libffmpeg::AV_ROUND_NEAR_INF | libffmpeg::AV_ROUND_PASS_MINMAX));

					session.packet.pts = libffmpeg::av_rescale_q_rnd(session.packet.pts,
						session.inputFormatContext->streams[session.packet.stream_index]->time_base,
						session.outputFormatContext->streams[session.packet.stream_index]->time_base,
						(libffmpeg::AVRounding)(libffmpeg::AV_ROUND_NEAR_INF | libffmpeg::AV_ROUND_PASS_MINMAX));

					result = libffmpeg::av_interleaved_write_frame(session.outputFormatContext, &session.packet);

					if (result < 0)
						throw gcnew VideoException("av_interleaved_write_frame error. " + result);

					av_free_packet(&session.packet);

				}
			}

			libffmpeg::av_write_trailer(session.outputFormatContext);
		}
		finally
		{
			System::Runtime::InteropServices::Marshal::FreeHGlobal(nativeInputFileNamePointer);
			System::Runtime::InteropServices::Marshal::FreeHGlobal(nativeOutputFileNamePointer);

			// free general stuff
			if (session.packet.data != NULL)
				av_free_packet(&session.packet);
			if (session.videoFrame != NULL)
				libffmpeg::av_free(session.videoFrame);
			if (session.inputCodecContext != NULL)
				libffmpeg::avcodec_close(session.inputCodecContext);
			if (session.convertContext != NULL)
				libffmpeg::sws_freeContext(session.convertContext);

			// close the output file
			if (session.outputFormatContext != NULL) {
				if (session.outputFormatContext && !(session.outputFormatContext->oformat->flags & AVFMT_NOFILE))
					libffmpeg::avio_close(session.outputFormatContext->pb);
				libffmpeg::avformat_free_context(session.outputFormatContext);
			}

			// close the input file
			if (session.inputFormatContext != NULL)
				libffmpeg::avformat_close_input(&session.inputFormatContext);
		}
	}
}