#include "stdafx.h"
#include "VideoReEncoder.h"
#include "Exceptions.h"

namespace WPFAnimationEncoding
{
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
		// input
		libffmpeg::AVFormatContext *inputFormatContext = NULL;
		libffmpeg::AVCodecContext *inputCodecContext = NULL;
		libffmpeg::AVStream *inputVideoStream = NULL;
		// output
		libffmpeg::AVFormatContext *outputFormatContext = NULL;
		// general
		libffmpeg::AVCodec *codec = NULL;
		libffmpeg::AVFrame *videoFrame = NULL;
		libffmpeg::SwsContext *convertContext = NULL;
		libffmpeg::AVPacket packet;
		packet.data = NULL;
		packet.size = 0;

		try
		{
			// open the input file
			int result = libffmpeg::avformat_open_input(&inputFormatContext, nativeInputFileName, NULL, NULL);
			if (result != 0)
				throw gcnew VideoException("Input avformat_alloc_output_context2 error " + result);
			if (!inputFormatContext)
				throw gcnew VideoException("Couldn't create input format context.");

			// retrieve stream information
			if (libffmpeg::avformat_find_stream_info(inputFormatContext, NULL) < 0)
				throw gcnew VideoException("Cannot find stream information.");

			// search for the first video stream
			for (unsigned int i = 0; i < inputFormatContext->nb_streams; i++)
			{
				if (inputFormatContext->streams[i]->codec->codec_type == libffmpeg::AVMEDIA_TYPE_VIDEO)
				{
					// get the pointer to the codec context for the video stream
					inputCodecContext = inputFormatContext->streams[i]->codec;
					inputVideoStream = inputFormatContext->streams[i];
					break;
				}
			}
			if (inputVideoStream == NULL)
				throw gcnew VideoException("Cannot find video stream in the specified file.");
			if (inputCodecContext == NULL)
				throw gcnew VideoException("Cannot create the input video codec context.");

			// find decoder for the video stream
			codec = libffmpeg::avcodec_find_decoder(inputCodecContext->codec_id);
			if (codec == NULL)
				throw gcnew VideoException("Cannot find codec to decode the video stream with");

			// open the codec
			if (libffmpeg::avcodec_open2(inputCodecContext, codec, NULL) < 0)
				throw gcnew VideoException("Cannot open video codec.");

			// allocate video frame
			videoFrame = libffmpeg::avcodec_alloc_frame();
			if (videoFrame == NULL)
				throw gcnew VideoException("Couldn't allocate the video frame.");

			// prepare scaling context to convert RGB image to video format
			convertContext = libffmpeg::sws_getContext(inputCodecContext->width, inputCodecContext->height, inputCodecContext->pix_fmt,
				inputCodecContext->width, inputCodecContext->height, libffmpeg::PIX_FMT_BGR24, SWS_BICUBIC, NULL, NULL, NULL);
			if (convertContext == NULL)
				throw gcnew VideoException("Cannot initialize frames conversion context.");

			// open the output file
			result = libffmpeg::avformat_alloc_output_context2(&outputFormatContext, NULL, NULL, nativeOutputFileName);
			if (result != 0)
				throw gcnew VideoException("Output avformat_alloc_output_context2 error " + result);
			if (!inputFormatContext)
				throw gcnew VideoException("Couldn't create output format context.");

			// copy all the stream information from the input file, to the output file
			for (int i = 0; i < inputFormatContext->nb_streams; i++)
			{
				// create the new stream
				libffmpeg::AVStream *outStream = libffmpeg::avformat_new_stream(outputFormatContext, NULL);
				if (!outStream)
					throw gcnew VideoException("Failed allocating output stream");

				// cope all the information about the stream from the input, to the output
				result = libffmpeg::avcodec_copy_context(outputFormatContext->streams[i]->codec,
					inputFormatContext->streams[i]->codec);
				if (result < 0)
					throw gcnew VideoException("Copying stream context failed. " + result);
			}

			// init muxer and write output file header
			if (!(outputFormatContext->oformat->flags & AVFMT_NOFILE)) {
				result = libffmpeg::avio_open(&outputFormatContext->pb, nativeOutputFileName, AVIO_FLAG_WRITE);
				if (result < 0)
					throw gcnew VideoException("Could not open output file. " + result);
			}
			result = libffmpeg::avformat_write_header(outputFormatContext, NULL);
			if (result < 0)
				throw gcnew VideoException("Error occurred when opening output file. " + result);

			bool hasFrame = true;

			while (hasFrame) {
				hasFrame = libffmpeg::av_read_frame(inputFormatContext, &packet) == 0;
				if (hasFrame) {

					//type = data->FormatContext->streams[packet.stream_index]->codec->codec_type;

					/* remux this frame without reencoding */
					packet.dts = libffmpeg::av_rescale_q_rnd(packet.dts,
						inputFormatContext->streams[packet.stream_index]->time_base,
						outputFormatContext->streams[packet.stream_index]->time_base,
						(libffmpeg::AVRounding)(libffmpeg::AV_ROUND_NEAR_INF | libffmpeg::AV_ROUND_PASS_MINMAX));

					packet.pts = libffmpeg::av_rescale_q_rnd(packet.pts,
						inputFormatContext->streams[packet.stream_index]->time_base,
						outputFormatContext->streams[packet.stream_index]->time_base,
						(libffmpeg::AVRounding)(libffmpeg::AV_ROUND_NEAR_INF | libffmpeg::AV_ROUND_PASS_MINMAX));

					result = libffmpeg::av_interleaved_write_frame(outputFormatContext, &packet);

					if (result < 0)
						throw gcnew VideoException("av_interleaved_write_frame error. " + result);

					av_free_packet(&packet);

				}
			}

			libffmpeg::av_write_trailer(outputFormatContext);
		}
		finally
		{
			System::Runtime::InteropServices::Marshal::FreeHGlobal(nativeInputFileNamePointer);
			System::Runtime::InteropServices::Marshal::FreeHGlobal(nativeOutputFileNamePointer);

			// free general stuff
			if (packet.data != NULL)
				av_free_packet(&packet);
			if (videoFrame != NULL)
				libffmpeg::av_free(videoFrame);
			if (inputCodecContext != NULL)
				libffmpeg::avcodec_close(inputCodecContext);
			if (convertContext != NULL)
				libffmpeg::sws_freeContext(convertContext);

			// close the output file
			if (outputFormatContext != NULL) {
				if (outputFormatContext && !(outputFormatContext->oformat->flags & AVFMT_NOFILE))
					libffmpeg::avio_close(outputFormatContext->pb);
				libffmpeg::avformat_free_context(outputFormatContext);
			}

			// close the input file
			if (inputFormatContext != NULL)
				libffmpeg::avformat_close_input(&inputFormatContext);


			//\int result = -1;
			//// input
			//libffmpeg::AVFormatContext *inputFormatContext = NULL;
			//libffmpeg::AVCodecContext *inputCodecContext = NULL;
			//libffmpeg::AVStream *inputVideoStream = NULL;
			//// output
			//libffmpeg::AVFormatContext *outputFormatContext = NULL;
			//// general
			//libffmpeg::AVCodec *codec = NULL;
			//libffmpeg::AVFrame *videoFrame = NULL;
			//libffmpeg::SwsContext *convertContext = NULL;
			//libffmpeg::AVPacket packet;
			//packet.data = NULL;
			//packet.size = 0;

		}
	}

	//// Read next video frame of the current video file
	//Bitmap^ VideoReEncoder::ReadVideoFrame()
	//{
	//	CheckIfDisposed();

	//	if (data == nullptr)
	//	{
	//		throw gcnew System::IO::IOException("Cannot read video frames since video file is not open.");
	//	}

	//	int frameFinished;
	//	Bitmap^ bitmap = nullptr;

	//	int bytesDecoded;
	//	bool exit = false;

	//	while (true)
	//	{
	//		// work on the current packet until we have decoded all of it
	//		while (data->BytesRemaining > 0)
	//		{
	//			// decode the next chunk of data
	//			bytesDecoded = libffmpeg::avcodec_decode_video2(data->CodecContext, data->VideoFrame, &frameFinished, data->Packet);

	//			// was there an error?
	//			if (bytesDecoded < 0)
	//			{
	//				throw gcnew VideoException("Error while decoding frame.");
	//			}

	//			data->BytesRemaining -= bytesDecoded;

	//			// did we finish the current frame? Then we can return
	//			if (frameFinished)
	//			{
	//				return DecodeVideoFrame();
	//			}
	//		}

	//		// read the next packet, skipping all packets that aren't
	//		// for this stream
	//		do
	//		{
	//			// free old packet if any
	//			if (data->Packet->data != NULL)
	//			{
	//				libffmpeg::av_free_packet(data->Packet);
	//				data->Packet->data = NULL;
	//			}

	//			// read new packet
	//			if (libffmpeg::av_read_frame(data->FormatContext, data->Packet) < 0)
	//			{
	//				exit = true;
	//				break;
	//			}
	//		} while (data->Packet->stream_index != data->VideoStream->index);

	//		// exit ?
	//		if (exit)
	//			break;

	//		data->BytesRemaining = data->Packet->size;
	//	}

	//	// decode the rest of the last frame
	//	bytesDecoded = libffmpeg::avcodec_decode_video2(
	//		data->CodecContext, data->VideoFrame, &frameFinished, data->Packet);

	//	// free last packet
	//	if (data->Packet->data != NULL)
	//	{
	//		libffmpeg::av_free_packet(data->Packet);
	//		data->Packet->data = NULL;
	//	}

	//	// is there a frame
	//	if (frameFinished)
	//	{
	//		bitmap = DecodeVideoFrame();
	//	}

	//	return bitmap;
	//}

	//// Decodes video frame into managed Bitmap
	//Bitmap^ VideoReEncoder::DecodeVideoFrame()
	//{
	//	Bitmap^ bitmap = gcnew Bitmap(data->CodecContext->width, data->CodecContext->height, PixelFormat::Format24bppRgb);

	//	// lock the bitmap
	//	BitmapData^ bitmapData = bitmap->LockBits(System::Drawing::Rectangle(0, 0, data->CodecContext->width, data->CodecContext->height),
	//		ImageLockMode::ReadOnly, PixelFormat::Format24bppRgb);

	//	libffmpeg::uint8_t* ptr = reinterpret_cast<libffmpeg::uint8_t*>(static_cast<void*>(bitmapData->Scan0));

	//	libffmpeg::uint8_t* srcData[4] = { ptr, NULL, NULL, NULL };
	//	int srcLinesize[4] = { bitmapData->Stride, 0, 0, 0 };

	//	// convert video frame to the RGB bitmap
	//	libffmpeg::sws_scale(data->ConvertContext, data->VideoFrame->data, data->VideoFrame->linesize, 0,
	//		data->CodecContext->height, srcData, srcLinesize);

	//	bitmap->UnlockBits(bitmapData);

	//	return bitmap;
	//}
}