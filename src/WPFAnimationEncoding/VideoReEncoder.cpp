#include "stdafx.h"
#include "VideoReEncoder.h"
#include "Exceptions.h"

namespace WPFAnimationEncoding
{
	// A structure to encapsulate all FFMPEG related private variable
	ref struct ReaderPrivateData
	{
	public:
		libffmpeg::AVFormatContext*             FormatContext;
		libffmpeg::AVStream*                    VideoStream;
		libffmpeg::AVCodecContext*              CodecContext;
		libffmpeg::AVFrame*                             VideoFrame;
		struct libffmpeg::SwsContext*   ConvertContext;

		libffmpeg::AVPacket* Packet;
		int BytesRemaining;

		ReaderPrivateData()
		{
			FormatContext = NULL;
			VideoStream = NULL;
			CodecContext = NULL;
			VideoFrame = NULL;
			ConvertContext = NULL;

			Packet = NULL;
			BytesRemaining = 0;
		}
	};

	// Class constructor
	VideoReEncoder::VideoReEncoder(void) :
		data(nullptr), disposed(false)
	{
		libffmpeg::av_register_all();
	}

	//#pragma managed(push, off)
	static libffmpeg::AVFormatContext* open_file(char* fileName)
	{
		libffmpeg::AVFormatContext* formatContext = NULL;

		if (libffmpeg::avformat_open_input(&formatContext, fileName, NULL, NULL) != 0)
		{
			return NULL;
		}
		return formatContext;
	}
	//#pragma managed(pop)

	// Opens the specified video file
	void VideoReEncoder::Open(String^ fileName)
	{
		CheckIfDisposed();

		// close previous file if any was open
		Close();

		data = gcnew ReaderPrivateData();
		data->Packet = new libffmpeg::AVPacket();
		data->Packet->data = NULL;

		bool success = false;

		IntPtr ptr = System::Runtime::InteropServices::Marshal::StringToHGlobalAnsi(fileName);
		char* nativeFileName = static_cast<char*>(ptr.ToPointer());

		try
		{
			// open the specified video file
			data->FormatContext = open_file(nativeFileName);
			if (data->FormatContext == NULL)
			{
				throw gcnew System::IO::IOException("Cannot open the video file.");
			}

			// retrieve stream information
			if (libffmpeg::avformat_find_stream_info(data->FormatContext, NULL) < 0)
			{
				throw gcnew VideoException("Cannot find stream information.");
			}

			// search for the first video stream
			for (unsigned int i = 0; i < data->FormatContext->nb_streams; i++)
			{
				if (data->FormatContext->streams[i]->codec->codec_type == libffmpeg::AVMEDIA_TYPE_VIDEO)
				{
					// get the pointer to the codec context for the video stream
					data->CodecContext = data->FormatContext->streams[i]->codec;
					data->VideoStream = data->FormatContext->streams[i];
					break;
				}
			}
			if (data->VideoStream == NULL)
			{
				throw gcnew VideoException("Cannot find video stream in the specified file.");
			}

			// find decoder for the video stream
			libffmpeg::AVCodec* codec = libffmpeg::avcodec_find_decoder(data->CodecContext->codec_id);
			if (codec == NULL)
			{
				throw gcnew VideoException("Cannot find codec to decode the video stream.");
			}

			// open the codec
			if (libffmpeg::avcodec_open2(data->CodecContext, codec, NULL) < 0)
			{
				throw gcnew VideoException("Cannot open video codec.");
			}

			// allocate video frame
			data->VideoFrame = libffmpeg::avcodec_alloc_frame();

			// prepare scaling context to convert RGB image to video format
			data->ConvertContext = libffmpeg::sws_getContext(data->CodecContext->width, data->CodecContext->height, data->CodecContext->pix_fmt,
				data->CodecContext->width, data->CodecContext->height, libffmpeg::PIX_FMT_BGR24,
				SWS_BICUBIC, NULL, NULL, NULL);

			if (data->ConvertContext == NULL)
			{
				throw gcnew VideoException("Cannot initialize frames conversion context.");
			}

			// get some properties of the video file
			m_width = data->CodecContext->width;
			m_height = data->CodecContext->height;

			success = true;
		}
		finally
		{
			System::Runtime::InteropServices::Marshal::FreeHGlobal(ptr);

			if (!success)
			{
				Close();
			}
		}
	}

	// Close current video file
	void VideoReEncoder::Close()
	{
		if (data != nullptr)
		{
			if (data->VideoFrame != NULL)
			{
				libffmpeg::av_free(data->VideoFrame);
			}

			if (data->CodecContext != NULL)
			{
				libffmpeg::avcodec_close(data->CodecContext);
			}

			if (data->FormatContext != NULL)
			{
				libffmpeg::AVFormatContext* context = data->FormatContext;
				libffmpeg::avformat_close_input(&context);
			}

			if (data->ConvertContext != NULL)
			{
				libffmpeg::sws_freeContext(data->ConvertContext);
			}

			if (data->Packet->data != NULL)
			{
				libffmpeg::av_free_packet(data->Packet);
			}

			data = nullptr;
		}
	}

	// start the reencoding of the video file
	void VideoReEncoder::StartReEncoding(String^ fileName, VideoReEncodeCallback^ callback)
	{
		// http://ffmpeg.org/doxygen/trunk/doc_2examples_2transcoding_8c-example.html

		int result;
		IntPtr ptr = System::Runtime::InteropServices::Marshal::StringToHGlobalAnsi(fileName);
		char* nativeFileName = static_cast<char*>(ptr.ToPointer());

		try
		{
			libffmpeg::AVFormatContext *outputFormat = NULL;
			libffmpeg::AVStream *outStream;

			int result = libffmpeg::avformat_alloc_output_context2(&outputFormat, NULL, NULL, nativeFileName);

			if (!outputFormat) 
				throw gcnew VideoException("Could not create output context.");
			if (result < 0)
				throw gcnew VideoException("avformat_alloc_output_context2 error. " + result);

			for (int i = 0; i < data->FormatContext->nb_streams; i++)
			{
				outStream = avformat_new_stream(outputFormat, NULL);

				if (!outStream) {
					throw gcnew VideoException("Failed allocating output stream");
				}

				result = libffmpeg::avcodec_copy_context(outputFormat->streams[i]->codec,
					data->FormatContext->streams[i]->codec);

				if (result < 0) {
					throw gcnew VideoException("Copying stream context failed. " + result);
				}

				if (!(outputFormat->oformat->flags & AVFMT_NOFILE)) {
					result = libffmpeg::avio_open(&outputFormat->pb, nativeFileName, AVIO_FLAG_WRITE);
					if (result < 0) {
						throw gcnew VideoException("Could not open output file. " + result);
					}
				}

				/* init muxer, write output file header */
				result = libffmpeg::avformat_write_header(outputFormat, NULL);
				if (result < 0) {
					throw gcnew VideoException("Error occurred when opening output file. " + result);
				}
			}

			enum libffmpeg::AVMediaType type;
			unsigned int stream_index;
			libffmpeg::AVPacket packet;
			packet.data = NULL;
			packet.size = 0;

			while (1) {
				result = libffmpeg::av_read_frame(data->FormatContext, &packet);
				if (result < 0)
					break;
				
				stream_index = packet.stream_index;
				type = data->FormatContext->streams[packet.stream_index]->codec->codec_type;
			
				/* remux this frame without reencoding */
				packet.dts = libffmpeg::av_rescale_q_rnd(packet.dts,
					data->FormatContext->streams[stream_index]->time_base,
					outputFormat->streams[stream_index]->time_base,
					(libffmpeg::AVRounding)(libffmpeg::AV_ROUND_NEAR_INF | libffmpeg::AV_ROUND_PASS_MINMAX));
				packet.pts = libffmpeg::av_rescale_q_rnd(packet.pts,
					data->FormatContext->streams[stream_index]->time_base,
					outputFormat->streams[stream_index]->time_base,
					(libffmpeg::AVRounding)(libffmpeg::AV_ROUND_NEAR_INF | libffmpeg::AV_ROUND_PASS_MINMAX));
				result = libffmpeg::av_interleaved_write_frame(outputFormat, &packet);
				if (result < 0)
					throw gcnew VideoException("av_interleaved_write_frame error. " + result);

				av_free_packet(&packet);
			}

			libffmpeg::av_write_trailer(outputFormat);

			av_free_packet(&packet);
			if (outputFormat && !(outputFormat->oformat->flags & AVFMT_NOFILE))
				libffmpeg::avio_close(outputFormat->pb);
			libffmpeg::avformat_free_context(outputFormat);
		}
		finally
		{
			System::Runtime::InteropServices::Marshal::FreeHGlobal(ptr);
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