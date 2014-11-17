#include "stdafx.h"
#include "VideoReEncoder.h"
#include "Exceptions.h"

#define LINE_SZ 1024

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
			inputConvertContext(NULL),
			outputConvertContext(NULL),
			outputGrayscaleContext(NULL)
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

			if (inputConvertContext != NULL)
			{
				libffmpeg::sws_freeContext(inputConvertContext);
				inputConvertContext = NULL;
			}

			if (outputConvertContext != NULL)
			{
				libffmpeg::sws_freeContext(outputConvertContext);
				outputConvertContext = NULL;
			}

			if (outputGrayscaleContext != NULL)
			{
				libffmpeg::sws_freeContext(outputGrayscaleContext);
				outputGrayscaleContext = NULL;
			}
		}
		InputOutputContext input;
		InputOutputContext output;
		libffmpeg::AVPacket packet;
		libffmpeg::SwsContext *inputConvertContext;
		libffmpeg::SwsContext *outputConvertContext;
		libffmpeg::SwsContext *outputGrayscaleContext;
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

		session->input.videoFrame = libffmpeg::avcodec_alloc_frame();
		if (!session->input.videoFrame)
			throw gcnew VideoException("Couldn't create the input video frame.");
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
			if (i == session->input.videoStream->index)
			{
				libffmpeg::AVCodec* codec = libffmpeg::avcodec_find_encoder(session->input.codecContext->codec_id);
				libffmpeg::AVStream* outStream = libffmpeg::avformat_new_stream(session->output.formatContext, codec);
				if (!outStream)
					throw gcnew VideoException("Failed allocating output stream");

				session->output.codec = codec;
				session->output.codecContext = outStream->codec;

				// put sample parameters
				session->output.codecContext->bit_rate = 400000000;
				session->output.codecContext->width = session->input.codecContext->width;
				session->output.codecContext->height = session->input.codecContext->height;

				// time base: this is the fundamental unit of time (in seconds) in terms
				// of which frame timestamps are represented. for fixed-fps content,
				// timebase should be 1/framerate and timestamp increments should be
				// identically 1.
				session->output.codecContext->time_base.den = session->input.videoStream->r_frame_rate.num;
				session->output.codecContext->time_base.num = session->input.videoStream->r_frame_rate.den;

				Console::WriteLine(session->output.codecContext->time_base.den);
				Console::WriteLine(session->output.codecContext->time_base.num);

				session->output.codecContext->gop_size = 12; // emit one intra frame every twelve frames at most
				session->output.codecContext->pix_fmt = session->input.codecContext->pix_fmt;

				if (session->output.codecContext->codec_id == libffmpeg::CODEC_ID_MPEG1VIDEO)
				{
					// Needed to avoid using macroblocks in which some coeffs overflow.
					// This does not happen with normal video, it just happens here as
					// the motion of the chroma plane does not match the luma plane.
					session->output.codecContext->mb_decision = 2;
				}

				// some formats want stream headers to be separate
				if (session->output.formatContext->oformat->flags & AVFMT_GLOBALHEADER)
				{
					session->output.codecContext->flags |= CODEC_FLAG_GLOBAL_HEADER;
				}

				session->output.videoStream = outStream;
			}
			else
			{
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

		// some formats want stream headers to be separate
		if (session->output.formatContext->oformat->flags & AVFMT_GLOBALHEADER)
		{
			session->output.codecContext->flags |= CODEC_FLAG_GLOBAL_HEADER;
		}

		// init muxer and write output file header
		if (!(session->output.formatContext->oformat->flags & AVFMT_NOFILE)) {
			result = libffmpeg::avio_open(&session->output.formatContext->pb, fileName, AVIO_FLAG_WRITE);
			if (result < 0)
				throw gcnew VideoException("Could not open output file. " + result);
		}

		// init the file
		result = libffmpeg::avformat_write_header(session->output.formatContext, NULL);
		if (result < 0)
			throw gcnew VideoException("Error occurred when opening output file. " + result);

		// open the codec
		result = libffmpeg::avcodec_open2(session->output.codecContext, session->output.codec, NULL);
		if (result < 0)
			throw gcnew VideoException("Cannot open video codec.");

		session->output.videoFrame = libffmpeg::avcodec_alloc_frame();
		session->output.videoFrame->format = session->output.codecContext->pix_fmt;
		session->output.videoFrame->width = session->output.codecContext->width;
		session->output.videoFrame->height = session->output.codecContext->height;

		result = libffmpeg::av_image_alloc(session->output.videoFrame->data, session->output.videoFrame->linesize, session->output.videoFrame->width, session->output.videoFrame->height, session->output.codecContext->pix_fmt, 32);
		if (result < 0)
			throw gcnew VideoException("Couldn't allocate frame for encoded video");

		//// create the video frame
		//session->output.videoFrame = libffmpeg::avcodec_alloc_frame();
		//if (!session->output.videoFrame)
		//	throw gcnew VideoException("Couldn't create the input video frame.");

		//int bufferSize = libffmpeg::avpicture_get_size(session->output.codecContext->pix_fmt, session->output.codecContext->width, session->output.codecContext->height);
		//void * picture_buf = libffmpeg::av_malloc(bufferSize);
		//if (!picture_buf)
		//	throw gcnew VideoException("Couldn't allocate the output frame buffer.");

		//libffmpeg::avpicture_fill((libffmpeg::AVPicture *) session->output.videoFrame, (libffmpeg::uint8_t *) picture_buf, session->output.codecContext->pix_fmt, session->output.codecContext->width, session->output.codecContext->height);
	}

	static Bitmap^ decode_frame(VideoReEncoderSession* session)
	{
		int frameFinished;
		Bitmap^ bitmap = nullptr;

		int bytesDecoded;
		int bytesRemaining = session->packet.size;
		bool exit = false;

		// work on the current packet until we have decoded all of it
		while (bytesRemaining > 0)
		{
			// decode the next chunk of data
			bytesDecoded = libffmpeg::avcodec_decode_video2(session->input.codecContext, session->input.videoFrame, &frameFinished, &session->packet);

			// was there an error?
			if (bytesDecoded < 0)
			{
				throw gcnew VideoException("Error while decoding frame.");
			}

			bytesRemaining -= bytesDecoded;

			if (frameFinished)
			{
				bitmap = gcnew Bitmap(session->input.codecContext->width, session->input.codecContext->height, PixelFormat::Format24bppRgb);

				// lock the bitmap
				BitmapData^ bitmapData = bitmap->LockBits(System::Drawing::Rectangle(0, 0, session->input.codecContext->width, session->input.codecContext->height),
					ImageLockMode::ReadOnly, PixelFormat::Format24bppRgb);

				libffmpeg::uint8_t* ptr = reinterpret_cast<libffmpeg::uint8_t*>(static_cast<void*>(bitmapData->Scan0));

				libffmpeg::uint8_t* srcData[4] = { ptr, NULL, NULL, NULL };
				int srcLinesize[4] = { bitmapData->Stride, 0, 0, 0 };

				// convert video frame to the RGB bitmap
				libffmpeg::sws_scale(session->inputConvertContext, session->input.videoFrame->data, session->input.videoFrame->linesize, 0,
					session->input.codecContext->height, srcData, srcLinesize);

				bitmap->UnlockBits(bitmapData);

				return bitmap;
			}
		}

		return bitmap;
	}

	static bool encode_frame(VideoReEncoderSession* session, Bitmap^ bitmap, libffmpeg::AVPacket* packet)
	{
		if ((bitmap->PixelFormat != PixelFormat::Format24bppRgb) &&
			(bitmap->PixelFormat != PixelFormat::Format32bppArgb) &&
			(bitmap->PixelFormat != PixelFormat::Format32bppPArgb) &&
			(bitmap->PixelFormat != PixelFormat::Format32bppRgb) &&
			(bitmap->PixelFormat != PixelFormat::Format8bppIndexed))
			throw gcnew ArgumentException("The provided bitmap must be 24 or 32 bpp color image or 8 bpp grayscale image.");

		if ((session->output.codecContext->width != bitmap->Width) || (session->output.codecContext->height != bitmap->Height))
			throw gcnew ArgumentException("Bitmap size must be of the same as video size ");

		// lock the bitmap
		BitmapData^ bitmapData = bitmap->LockBits(System::Drawing::Rectangle(0, 0, bitmap->Width, bitmap->Height),
			ImageLockMode::ReadOnly,
			(bitmap->PixelFormat == PixelFormat::Format8bppIndexed) ? PixelFormat::Format8bppIndexed : PixelFormat::Format24bppRgb);

		libffmpeg::uint8_t* ptr = reinterpret_cast<libffmpeg::uint8_t*>(static_cast<void*>(bitmapData->Scan0));

		libffmpeg::uint8_t* srcData[4] = { ptr, NULL, NULL, NULL };
		int srcLinesize[4] = { bitmapData->Stride, 0, 0, 0 };

		// convert source image to the format of the video file
		if (bitmap->PixelFormat == PixelFormat::Format8bppIndexed)
		{
			libffmpeg::sws_scale(session->outputGrayscaleContext, srcData, srcLinesize, 0, bitmap->Height, session->output.videoFrame->data, session->output.videoFrame->linesize);
		}
		else
		{
			libffmpeg::sws_scale(session->outputConvertContext, srcData, srcLinesize, 0, bitmap->Height, session->output.videoFrame->data, session->output.videoFrame->linesize);
		}

		bitmap->UnlockBits(bitmapData);

		int gotFrame;
		
		int result = libffmpeg::avcodec_encode_video2(session->output.codecContext, packet, session->output.videoFrame, &gotFrame);
		if (result < 0)
			throw gcnew VideoException("Error encoding video. " + result);

		if (gotFrame)
		{
			return true;
		}

		return false;
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

			session.inputConvertContext = libffmpeg::sws_getContext(session.input.codecContext->width, session.input.codecContext->height, session.input.codecContext->pix_fmt,
				session.input.codecContext->width, session.input.codecContext->height, libffmpeg::PIX_FMT_BGR24, SWS_BICUBIC, NULL, NULL, NULL);
			if (session.inputConvertContext == NULL)
				throw gcnew VideoException("Cannot initialize input frames conversion context.");

			session.outputConvertContext = libffmpeg::sws_getContext(session.output.codecContext->width, session.output.codecContext->height, libffmpeg::PIX_FMT_BGR24,
				session.output.codecContext->width, session.output.codecContext->height, session.output.codecContext->pix_fmt,
				SWS_BICUBIC, NULL, NULL, NULL);
			if (session.outputConvertContext == NULL)
				throw gcnew VideoException("Cannot initialize output frames conversion context.");

			session.outputGrayscaleContext = libffmpeg::sws_getContext(session.output.codecContext->width, session.output.codecContext->height, libffmpeg::PIX_FMT_GRAY8,
				session.output.codecContext->width, session.output.codecContext->height, session.output.codecContext->pix_fmt,
				SWS_BICUBIC, NULL, NULL, NULL);
			if (session.outputGrayscaleContext == NULL)
				throw gcnew VideoException("Cannot initialize output grayscale frames conversion context.");

			unsigned long packetNumber = 0;

			bool hasFrame = true;
			bool cancel = false;
			while (hasFrame && !cancel) {
				hasFrame = libffmpeg::av_read_frame(session.input.formatContext, &session.packet) == 0;
				if (hasFrame) {

					if (session.packet.stream_index == session.input.videoStream->index)
					{
						// this packet is video!
						Bitmap^ decodedBitmap = decode_frame(&session);
						if (decodedBitmap != nullptr)
						{
							Bitmap^ newBitmap = callback(decodedBitmap, cancel);
							if (newBitmap == nullptr)
								throw gcnew Exception("You must return a bitmap to re-encode!");

							libffmpeg::AVPacket newPacket;
							libffmpeg::av_init_packet(&newPacket);
							newPacket.data = NULL;
							newPacket.size = 0;
							session.output.videoFrame->pts = packetNumber;

							if (encode_frame(&session, newBitmap, &newPacket))
							{
								newPacket.dts = libffmpeg::av_rescale_q_rnd(newPacket.dts,
									session.output.codecContext->time_base,
									session.output.formatContext->streams[session.packet.stream_index]->time_base,
									(libffmpeg::AVRounding)(libffmpeg::AV_ROUND_NEAR_INF | libffmpeg::AV_ROUND_PASS_MINMAX));

								newPacket.pts = libffmpeg::av_rescale_q_rnd(newPacket.pts,
									session.output.codecContext->time_base,
									session.output.formatContext->streams[session.packet.stream_index]->time_base,
									(libffmpeg::AVRounding)(libffmpeg::AV_ROUND_NEAR_INF | libffmpeg::AV_ROUND_PASS_MINMAX));

								newPacket.stream_index = session.packet.stream_index;

								libffmpeg::log_packet(session.output.formatContext, &newPacket);

								result = libffmpeg::av_interleaved_write_frame(session.output.formatContext, &newPacket);

								if (result < 0)
									throw gcnew VideoException("av_interleaved_write_frame error. " + result);
							}

							packetNumber++;

							libffmpeg::av_free_packet(&newPacket);
						}
					}
					else
					{
						// this packet is not the video we are interested in (probally audio).
						// just remux it!

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
					}

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