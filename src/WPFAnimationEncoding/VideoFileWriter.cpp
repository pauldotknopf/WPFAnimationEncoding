#include "StdAfx.h"
#include "VideoFileWriter.h"
#include "libffmpeg.h"
#include "Exceptions.h"

namespace WPFAnimationEncoding {

	int video_codecs[] =
	{
		libffmpeg::CODEC_ID_H264,
		libffmpeg::CODEC_ID_WMV1,
		libffmpeg::CODEC_ID_WMV2,
		libffmpeg::CODEC_ID_MSMPEG4V2,
		libffmpeg::CODEC_ID_MSMPEG4V3,
		libffmpeg::CODEC_ID_H263P,
		libffmpeg::CODEC_ID_FLV1,
		libffmpeg::CODEC_ID_MPEG2VIDEO,
		libffmpeg::CODEC_ID_RAWVIDEO
	};

	int pixel_formats[] =
	{
		libffmpeg::PIX_FMT_YUV420P,
		libffmpeg::PIX_FMT_YUV420P,
		libffmpeg::PIX_FMT_YUV420P,
		libffmpeg::PIX_FMT_YUV420P,
		libffmpeg::PIX_FMT_YUV420P,
		libffmpeg::PIX_FMT_YUV420P,
		libffmpeg::PIX_FMT_YUV420P,
		libffmpeg::PIX_FMT_YUV420P,
		libffmpeg::PIX_FMT_BGR24,
	};

	int CODECS_COUNT(sizeof(video_codecs) / sizeof(libffmpeg::AVCodecID));

#pragma region Some private FFmpeg related stuff hidden out of header file

	static void write_video_frame(WriterPrivateData^ data);
	static void open_video(WriterPrivateData^ data);
	static void add_video_stream(WriterPrivateData^ data, int width, int height, int frameRate, int bitRate,
	enum libffmpeg::AVCodecID codec_id, enum libffmpeg::AVPixelFormat pixelFormat);

	// A structure to encapsulate all FFMPEG related private variable
	ref struct WriterPrivateData
	{
	public:
		libffmpeg::AVFormatContext*             FormatContext;
		libffmpeg::AVStream*                    VideoStream;
		libffmpeg::AVFrame*                             VideoFrame;
		struct libffmpeg::SwsContext*   ConvertContext;
		struct libffmpeg::SwsContext*   ConvertContextGrayscale;

		WriterPrivateData()
		{
			FormatContext = NULL;
			VideoStream = NULL;
			VideoFrame = NULL;
			ConvertContext = NULL;
			ConvertContextGrayscale = NULL;
		}
	};
#pragma endregion

	// Class constructor
	VideoFileWriter::VideoFileWriter(void) :
		data(nullptr), disposed(false)
	{
		libffmpeg::av_register_all();
		libffmpeg::avcodec_register_all();
	}

	void VideoFileWriter::Open(String^ fileName, int width, int height)
	{
		Open(fileName, width, height, 25);
	}

	void VideoFileWriter::Open(String^ fileName, int width, int height, int frameRate)
	{
		Open(fileName, width, height, frameRate, VideoCodec::Default);
	}

	void VideoFileWriter::Open(String^ fileName, int width, int height, int frameRate, VideoCodec codec)
	{
		Open(fileName, width, height, frameRate, codec, 400000);
	}

	// Creates a video file with the specified name and properties
	void VideoFileWriter::Open(String^ fileName, int width, int height, int frameRate, VideoCodec codec, int bitRate)
	{
		CheckIfDisposed();

		// close previous file if any open
		Close();

		data = gcnew WriterPrivateData();
		bool success = false;

		// check width and height
		if (((width & 1) != 0) || ((height & 1) != 0))
		{
			throw gcnew ArgumentException("Video file resolution must be a multiple of two.");
		}

		// check video codec
		if (((int)codec < -1) || ((int)codec >= CODECS_COUNT))
		{
			throw gcnew ArgumentException("Invalid video codec is specified.");
		}

		m_width = width;
		m_height = height;
		m_codec = codec;
		m_frameRate = frameRate;
		m_bitRate = bitRate;

		// convert specified managed String to unmanaged string
		IntPtr ptr = System::Runtime::InteropServices::Marshal::StringToHGlobalUni(fileName);
		wchar_t* nativeFileNameUnicode = (wchar_t*)ptr.ToPointer();
		int utf8StringSize = WideCharToMultiByte(CP_UTF8, 0, nativeFileNameUnicode, -1, NULL, 0, NULL, NULL);
		char* nativeFileName = new char[utf8StringSize];
		WideCharToMultiByte(CP_UTF8, 0, nativeFileNameUnicode, -1, nativeFileName, utf8StringSize, NULL, NULL);

		try
		{
			// gues about destination file format from its file name
			libffmpeg::AVOutputFormat* outputFormat = libffmpeg::av_guess_format(NULL, nativeFileName, NULL);

			if (!outputFormat)
			{
				// gues about destination file format from its short name
				outputFormat = libffmpeg::av_guess_format("mpeg", NULL, NULL);

				if (!outputFormat)
				{
					throw gcnew VideoException("Cannot find suitable output format.");
				}
			}

			// prepare format context
			data->FormatContext = libffmpeg::avformat_alloc_context();

			if (!data->FormatContext)
			{
				throw gcnew VideoException("Cannot allocate format context.");
			}
			data->FormatContext->oformat = outputFormat;

			//// add video stream using the specified video codec
			//add_video_stream(data, width, height, frameRate, bitRate,
			//	(codec == VideoCodec::Default) ? outputFormat->video_codec : (libffmpeg::AVCodecID) video_codecs[(int)codec],
			//	(codec == VideoCodec::Default) ? libffmpeg::PIX_FMT_YUV420P : (libffmpeg::AVPixelFormat) pixel_formats[(int)codec]);
/*
			libffmpeg::CODEC_ID_H264,
				libffmpeg::CODEC_ID_WMV1,
				libffmpeg::CODEC_ID_WMV2,
				libffmpeg::CODEC_ID_MSMPEG4V2,
				libffmpeg::CODEC_ID_MSMPEG4V3,
				libffmpeg::CODEC_ID_H263P,
				libffmpeg::CODEC_ID_FLV1,
				libffmpeg::CODEC_ID_MPEG2VIDEO,
				libffmpeg::CODEC_ID_RAWVIDEO
		};

		int pixel_formats[] =
		{
			libffmpeg::PIX_FMT_YUV420P,
			libffmpeg::PIX_FMT_YUV420P,
			libffmpeg::PIX_FMT_YUV420P,
			libffmpeg::PIX_FMT_YUV420P,
			libffmpeg::PIX_FMT_YUV420P,
			libffmpeg::PIX_FMT_YUV420P,
			libffmpeg::PIX_FMT_YUV420P,
			libffmpeg::PIX_FMT_YUV420P,
			libffmpeg::PIX_FMT_BGR24,*/

			add_video_stream(data, width, height, frameRate, bitRate, libffmpeg::CODEC_ID_H264, libffmpeg::PIX_FMT_YUV420P);

			open_video(data);

			// open output file
			if (!(outputFormat->flags & AVFMT_NOFILE))
			{
				if (libffmpeg::avio_open(&data->FormatContext->pb, nativeFileName, AVIO_FLAG_WRITE) < 0)
				{
					throw gcnew System::IO::IOException("Cannot open the video file.");
				}
			}

			libffmpeg::avformat_write_header(data->FormatContext, NULL);

			success = true;
		}
		finally
		{
			System::Runtime::InteropServices::Marshal::FreeHGlobal(ptr);
			delete[] nativeFileName;

			if (!success)
			{
				Close();
			}
		}
	}

	// Close current video file
	void VideoFileWriter::Close()
	{
		if (data != nullptr)
		{
			if (data->FormatContext)
			{
				if (data->FormatContext->pb != NULL)
				{
					libffmpeg::av_write_trailer(data->FormatContext);
				}

				if (data->VideoStream)
				{
					libffmpeg::avcodec_close(data->VideoStream->codec);
				}

				if (data->VideoFrame)
				{
					libffmpeg::av_free(data->VideoFrame->data[0]);
					libffmpeg::av_free(data->VideoFrame);
				}

				for (unsigned int i = 0; i < data->FormatContext->nb_streams; i++)
				{
					libffmpeg::av_freep(&data->FormatContext->streams[i]->codec);
					libffmpeg::av_freep(&data->FormatContext->streams[i]);
				}

				if (data->FormatContext->pb != NULL)
				{
					libffmpeg::avio_close(data->FormatContext->pb);
				}

				libffmpeg::av_free(data->FormatContext);
			}

			if (data->ConvertContext != NULL)
			{
				libffmpeg::sws_freeContext(data->ConvertContext);
			}

			if (data->ConvertContextGrayscale != NULL)
			{
				libffmpeg::sws_freeContext(data->ConvertContextGrayscale);
			}

			data = nullptr;
		}

		m_width = 0;
		m_height = 0;
	}

	// Writes new video frame to the opened video file
	void VideoFileWriter::WriteVideoFrame(Bitmap^ frame)
	{
		WriteVideoFrame(frame, 0);
	}

	// Writes new video frame to the opened video file
	void VideoFileWriter::WriteVideoFrame(Bitmap^ frame, int frameNumber)
	{
		CheckIfDisposed();

		if (data == nullptr)
		{
			throw gcnew System::IO::IOException("A video file was not opened yet.");
		}

		if ((frame->PixelFormat != PixelFormat::Format24bppRgb) &&
			(frame->PixelFormat != PixelFormat::Format32bppArgb) &&
			(frame->PixelFormat != PixelFormat::Format32bppPArgb) &&
			(frame->PixelFormat != PixelFormat::Format32bppRgb) &&
			(frame->PixelFormat != PixelFormat::Format8bppIndexed))
		{
			throw gcnew ArgumentException("The provided bitmap must be 24 or 32 bpp color image or 8 bpp grayscale image.");
		}

		if ((frame->Width != m_width) || (frame->Height != m_height))
		{
			throw gcnew ArgumentException("Bitmap size must be of the same as video size, which was specified on opening video file.");
		}

		// lock the bitmap
		BitmapData^ bitmapData = frame->LockBits(System::Drawing::Rectangle(0, 0, m_width, m_height),
			ImageLockMode::ReadOnly,
			(frame->PixelFormat == PixelFormat::Format8bppIndexed) ? PixelFormat::Format8bppIndexed : PixelFormat::Format24bppRgb);

		libffmpeg::uint8_t* ptr = reinterpret_cast<libffmpeg::uint8_t*>(static_cast<void*>(bitmapData->Scan0));

		libffmpeg::uint8_t* srcData[4] = { ptr, NULL, NULL, NULL };
		int srcLinesize[4] = { bitmapData->Stride, 0, 0, 0 };

		// convert source image to the format of the video file
		if (frame->PixelFormat == PixelFormat::Format8bppIndexed)
		{
			libffmpeg::sws_scale(data->ConvertContextGrayscale, srcData, srcLinesize, 0, m_height, data->VideoFrame->data, data->VideoFrame->linesize);
		}
		else
		{
			libffmpeg::sws_scale(data->ConvertContext, srcData, srcLinesize, 0, m_height, data->VideoFrame->data, data->VideoFrame->linesize);
		}

		frame->UnlockBits(bitmapData);

		data->VideoFrame->pts = static_cast<libffmpeg::int64_t>(frameNumber);

		// write the converted frame to the video file
		write_video_frame(data);
	}

#pragma region Private methods
	// Writes video frame to opened video file
	void write_video_frame(WriterPrivateData^ data)
	{
		libffmpeg::AVCodecContext* codecContext = data->VideoStream->codec;
		int out_size, ret = 0;
		int gotFrame;

		if (data->FormatContext->oformat->flags & AVFMT_RAWPICTURE)
		{
			Console::WriteLine("raw picture must be written");
		}
		else
		{
			// encode the image
			libffmpeg::AVPacket packet;
			libffmpeg::av_init_packet(&packet);
			packet.data = NULL;
			packet.size = 0;
			
			ret = libffmpeg::avcodec_encode_video2(codecContext, &packet, data->VideoFrame, &gotFrame);

			if (gotFrame)
			{
				if (ret < 0)
				{
					throw gcnew VideoException("Error encoding video. " + ret);
				}

				packet.pts = libffmpeg::av_rescale_q(data->VideoFrame->pts, codecContext->time_base, data->VideoStream->time_base);
				packet.dts = packet.pts;

				packet.stream_index = data->VideoStream->index;

				// write the compressed frame to the media file
				ret = libffmpeg::av_interleaved_write_frame(data->FormatContext, &packet);
			}

			libffmpeg::av_free_packet(&packet);
		}

		if (ret != 0)
		{
			throw gcnew VideoException("Error while writing video frame.");
		}
	}

	// Allocate picture of the specified format and size
	static libffmpeg::AVFrame* alloc_picture(enum libffmpeg::AVPixelFormat pix_fmt, int width, int height)
	{
		libffmpeg::AVFrame* picture;
		/*void* picture_buf;
		int size;*/

		picture = libffmpeg::avcodec_alloc_frame();
		picture->format = pix_fmt;
		picture->width = width;
		picture->height = height;

		int result = libffmpeg::av_image_alloc(picture->data, picture->linesize, picture->width, picture->height, pix_fmt, 32);
		if (result < 0)
		{
			throw gcnew VideoException("Couldn't allocate frame for encoded video");
		}

		return picture;
	}

	// Create new video stream and configure it
	void add_video_stream(WriterPrivateData^ data, int width, int height, int frameRate, int bitRate,
	enum libffmpeg::AVCodecID codecId, enum libffmpeg::AVPixelFormat pixelFormat)
	{
		libffmpeg::AVCodecContext* codecContex;
		libffmpeg::AVCodec *codec = libffmpeg::avcodec_find_encoder(libffmpeg::AV_CODEC_ID_H264);

		// create new stream
		data->VideoStream = libffmpeg::avformat_new_stream(data->FormatContext, codec);
		if (!data->VideoStream)
		{
			throw gcnew VideoException("Failed creating new video stream.");
		}

		codecContex = data->VideoStream->codec;
		//codecContex->codec_id = libffmpeg::AV_CODEC_ID_H264;
		//codecContex->codec_type = libffmpeg::AVMEDIA_TYPE_VIDEO;

		// put sample parameters
		codecContex->bit_rate = bitRate;
		codecContex->width = width;
		codecContex->height = height;

		// time base: this is the fundamental unit of time (in seconds) in terms
		// of which frame timestamps are represented. for fixed-fps content,
		// timebase should be 1/framerate and timestamp increments should be
		// identically 1.
		codecContex->time_base.den = frameRate;
		codecContex->time_base.num = 1;

		codecContex->gop_size = 12; // emit one intra frame every twelve frames at most
		codecContex->pix_fmt = pixelFormat;

		if (codecContex->codec_id == libffmpeg::CODEC_ID_MPEG1VIDEO)
		{
			// Needed to avoid using macroblocks in which some coeffs overflow.
			// This does not happen with normal video, it just happens here as
			// the motion of the chroma plane does not match the luma plane.
			codecContex->mb_decision = 2;
		}

		// some formats want stream headers to be separate
		if (data->FormatContext->oformat->flags & AVFMT_GLOBALHEADER)
		{
			codecContex->flags |= CODEC_FLAG_GLOBAL_HEADER;
		}
	}

	// Open video codec and prepare out buffer and picture
	void open_video(WriterPrivateData^ data)
	{
		libffmpeg::AVCodecContext* codecContext = data->VideoStream->codec;
		libffmpeg::AVCodec* codec = libffmpeg::avcodec_find_encoder(libffmpeg::AV_CODEC_ID_H264);

		libffmpeg::AVCodec c = (*codec);
		const char * name = c.name;
		if (!codec)
		{
			throw gcnew VideoException("Cannot find video codec.");
		}

		// open the codec 
		int result = libffmpeg::avcodec_open2(codecContext, codec, NULL);
		if (result < 0)
		{
			throw gcnew VideoException("Cannot open video codec.");
		}

		// allocate the encoded raw picture
		data->VideoFrame = alloc_picture(codecContext->pix_fmt, codecContext->width, codecContext->height);

		if (!data->VideoFrame)
		{
			throw gcnew VideoException("Cannot allocate video picture.");
		}

		// prepare scaling context to convert RGB image to video format
		data->ConvertContext = libffmpeg::sws_getContext(codecContext->width, codecContext->height, libffmpeg::PIX_FMT_BGR24,
			codecContext->width, codecContext->height, codecContext->pix_fmt,
			SWS_BICUBIC, NULL, NULL, NULL);
		// prepare scaling context to convert grayscale image to video format
		data->ConvertContextGrayscale = libffmpeg::sws_getContext(codecContext->width, codecContext->height, libffmpeg::PIX_FMT_GRAY8,
			codecContext->width, codecContext->height, codecContext->pix_fmt,
			SWS_BICUBIC, NULL, NULL, NULL);

		if ((data->ConvertContext == NULL) || (data->ConvertContextGrayscale == NULL))
		{
			throw gcnew VideoException("Cannot initialize frames conversion context.");
		}
	}
#pragma endregion

}