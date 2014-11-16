#include "stdafx.h"
#include "VideoReEncoder.h"
#include "Exceptions.h"

namespace WPFAnimationEncoding
{
	struct InputOutputContext 
	{
		InputOutputContext() :
			formatContext(NULL),
			codecContext(NULL),
			videoStream(NULL),
			videoFrame(NULL)
		{

		}
		~InputOutputContext()
		{
			if (videoFrame != NULL)
			{
				libffmpeg::av_free(videoFrame);
				videoFrame = NULL;
			}
			if (codecContext != NULL)
			{
				libffmpeg::avcodec_close(codecContext);
				codecContext = NULL;
			}
			if (formatContext != NULL)
			{
				if (formatContext->oformat != NULL)
				{
					if (!(formatContext->oformat->flags & AVFMT_NOFILE))
						libffmpeg::avio_close(formatContext->pb);
					libffmpeg::avformat_free_context(formatContext);
				}
				else if (formatContext->iformat != NULL)
				{
					libffmpeg::avformat_close_input(&formatContext);
				}
				else
				{
					libffmpeg::avformat_free_context(formatContext);
				}
				formatContext = NULL;
			}
			// do i need to free this, even though I free the format context? probally not.
			videoStream = NULL;
		}
		libffmpeg::AVFormatContext *formatContext;
		libffmpeg::AVCodecContext *codecContext;
		libffmpeg::AVStream *videoStream;
		libffmpeg::AVCodec *codec;
		libffmpeg::AVFrame *videoFrame;
	};
	struct VideoReEncoderSession
	{
		VideoReEncoderSession() :
			convertContext(NULL)
		{
			packet.data = NULL;
			packet.size = 0;
		}
		~VideoReEncoderSession()
		{
			// free general stuff
			if (packet.data != NULL)
			{
				libffmpeg::av_free_packet(&packet);
			}

			if (convertContext != NULL)
			{
				convertContext = NULL;
				libffmpeg::sws_freeContext(convertContext);
			}
		}
		InputOutputContext input;
		InputOutputContext output;
		// general
		libffmpeg::SwsContext *convertContext;
		libffmpeg::AVPacket packet;
	};

	static void prepare_input(VideoReEncoderSession* session, char *fileName)
	{
		int result = libffmpeg::avformat_open_input(&session->input.formatContext, fileName, NULL, NULL);
		if (result != 0)
			throw gcnew VideoException("Input avformat_alloc_output_context2 error " + result);
		if (!session->input.formatContext)
			throw gcnew VideoException("Couldn't create input format context.");

		// retrieve stream information
		if (libffmpeg::avformat_find_stream_info(session->input.formatContext, NULL) < 0)
			throw gcnew VideoException("Cannot find stream information.");

		// search for the first video stream
		for (unsigned int i = 0; i < session->input.formatContext->nb_streams; i++)
		{
			if (session->input.formatContext->streams[i]->codec->codec_type == libffmpeg::AVMEDIA_TYPE_VIDEO)
			{
				// get the pointer to the codec context for the video stream
				session->input.codecContext = session->input.formatContext->streams[i]->codec;
				session->input.videoStream = session->input.formatContext->streams[i];
				break;
			}
		}
		if (session->input.videoStream == NULL)
			throw gcnew VideoException("Cannot find video stream in the specified file.");
		if (session->input.codecContext == NULL)
			throw gcnew VideoException("Cannot create the input video codec context.");

		// find decoder for the video stream
		session->input.codec = libffmpeg::avcodec_find_decoder(session->input.codecContext->codec_id);
		if (session->input.codec == NULL)
			throw gcnew VideoException("Cannot find codec to decode the video stream with");

		// open the codec
		if (libffmpeg::avcodec_open2(session->input.codecContext, session->input.codec, NULL) < 0)
			throw gcnew VideoException("Cannot open video codec.");
	}

	static void prepate_output(VideoReEncoderSession* session, char *fileName)
	{
		// make sure we opened the input first
		if (!session->input.formatContext)
			throw gcnew VideoException("You have to prepare the input before preparing the output.");

		// open the output file
		int result = libffmpeg::avformat_alloc_output_context2(&session->output.formatContext, NULL, NULL, fileName);
		if (result != 0)
			throw gcnew VideoException("Output avformat_alloc_output_context2 error " + result);
		if (!session->output.formatContext)
			throw gcnew VideoException("Couldn't create output format context.");

		// copy all the stream information from the input file, to the output file
		for (int i = 0; i < session->input.formatContext->nb_streams; i++)
		{
			if (i <= 1){
				// create the new stream
				libffmpeg::AVStream *outStream = libffmpeg::avformat_new_stream(session->output.formatContext, NULL);
				if (!outStream)
					throw gcnew VideoException("Failed allocating output stream");

				// cope all the information about the stream from the input, to the output
				result = libffmpeg::avcodec_copy_context(session->output.formatContext->streams[i]->codec,
					session->input.formatContext->streams[i]->codec);
				if (result < 0)
					throw gcnew VideoException("Copying stream context failed. " + result);
			}
		}

		// init muxer and write output file header
		if (!(session->output.formatContext->oformat->flags & AVFMT_NOFILE)) {
			result = libffmpeg::avio_open(&session->output.formatContext->pb, fileName, AVIO_FLAG_WRITE);
			if (result < 0)
				throw gcnew VideoException("Could not open output file. " + result);
		}
		result = libffmpeg::avformat_write_header(session->output.formatContext, NULL);
		if (result < 0)
			throw gcnew VideoException("Error occurred when opening output file. " + result);
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
			prepare_input(&session, nativeInputFileName);

			prepate_output(&session, nativeOutputFileName);

			// prepare scaling context to convert RGB image to video format
			session.convertContext = libffmpeg::sws_getContext(session.input.codecContext->width, session.input.codecContext->height, session.input.codecContext->pix_fmt,
				session.input.codecContext->width, session.input.codecContext->height, libffmpeg::PIX_FMT_BGR24, SWS_BICUBIC, NULL, NULL, NULL);
			if (session.convertContext == NULL)
				throw gcnew VideoException("Cannot initialize frames conversion context.");

			bool hasFrame = true;

			while (hasFrame) {
				hasFrame = libffmpeg::av_read_frame(session.input.formatContext, &session.packet) == 0;
				if (hasFrame) {

					//type = data->FormatContext->streams[packet.stream_index]->codec->codec_type;

					/* remux this frame without reencoding */
					session.packet.dts = libffmpeg::av_rescale_q_rnd(session.packet.dts,
						session.input.formatContext->streams[session.packet.stream_index]->time_base,
						session.output.formatContext->streams[session.packet.stream_index]->time_base,
						(libffmpeg::AVRounding)(libffmpeg::AV_ROUND_NEAR_INF | libffmpeg::AV_ROUND_PASS_MINMAX));

					session.packet.pts = libffmpeg::av_rescale_q_rnd(session.packet.pts,
						session.input.formatContext->streams[session.packet.stream_index]->time_base,
						session.output.formatContext->streams[session.packet.stream_index]->time_base,
						(libffmpeg::AVRounding)(libffmpeg::AV_ROUND_NEAR_INF | libffmpeg::AV_ROUND_PASS_MINMAX));

					result = libffmpeg::av_interleaved_write_frame(session.output.formatContext, &session.packet);

					if (result < 0)
						throw gcnew VideoException("av_interleaved_write_frame error. " + result);

					av_free_packet(&session.packet);

				}
			}

			libffmpeg::av_write_trailer(session.output.formatContext);
		}
		finally
		{
			System::Runtime::InteropServices::Marshal::FreeHGlobal(nativeInputFileNamePointer);
			System::Runtime::InteropServices::Marshal::FreeHGlobal(nativeOutputFileNamePointer);
		}
	}
}