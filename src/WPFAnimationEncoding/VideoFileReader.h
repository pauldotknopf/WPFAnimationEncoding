#pragma once

using namespace System;
using namespace System::Drawing;
using namespace System::Drawing::Imaging;

namespace WPFAnimationEncoding
{
	ref struct ReaderPrivateData;

	public ref class VideoFileReader : IDisposable
	{
	public:

		/// <summary>
		/// Frame width of the opened video file.
		/// </summary>
		///
		/// <exception cref="System::IO::IOException">Thrown if no video file was open.</exception>
		///
		property int Width
		{
			int get()
			{
				CheckIfVideoFileIsOpen();
				return m_width;
			}
		}

		/// <summary>
		/// Frame height of the opened video file.
		/// </summary>
		///
		/// <exception cref="System::IO::IOException">Thrown if no video file was open.</exception>
		///
		property int Height
		{
			int get()
			{
				CheckIfVideoFileIsOpen();
				return m_height;
			}
		}

		/// <summary>
		/// Frame rate of the opened video file.
		/// </summary>
		///
		/// <exception cref="System::IO::IOException">Thrown if no video file was open.</exception>
		///
		property int FrameRate
		{
			int get()
			{
				CheckIfVideoFileIsOpen();
				return m_frameRate;
			}
		}

		/// <summary>
		/// Number of video frames in the opened video file.
		/// </summary>
		///
		/// <remarks><para><note><b>Warning</b>: some video file formats may report different value
		/// from the actual number of video frames in the file (subject to fix/investigate).</note></para>
		/// </remarks>
		///
		/// <exception cref="System::IO::IOException">Thrown if no video file was open.</exception>
		///
		property Int64 FrameCount
		{
			Int64 get()
			{
				CheckIfVideoFileIsOpen();
				return m_framesCount;
			}
		}

		/// <summary>
		/// Name of codec used for encoding the opened video file.
		/// </summary>
		///
		/// <exception cref="System::IO::IOException">Thrown if no video file was open.</exception>
		///
		property String^ CodecName
		{
			String^ get()
			{
				CheckIfVideoFileIsOpen();
				return m_codecName;
			}
		}

		/// <summary>
		/// The property specifies if a video file is opened or not by this instance of the class.
		/// </summary>
		property bool IsOpen
		{
			bool get()
			{
				return (data != nullptr);
			}
		}

	protected:

		/// <summary>
		/// Object's finalizer.
		/// </summary>
		/// 
		!VideoFileReader()
		{
			Close();
		}

	public:

		/// <summary>
		/// Initializes a new instance of the <see cref="VideoFileReader"/> class.
		/// </summary>
		/// 
		VideoFileReader(void);

		/// <summary>
		/// Disposes the object and frees its resources.
		/// </summary>
		/// 
		~VideoFileReader()
		{
			this->!VideoFileReader();
			disposed = true;
		}

		/// <summary>
		/// Open video file with the specified name.
		/// </summary>
		///
		/// <param name="fileName">Video file name to open.</param>
		///
		/// <exception cref="System::IO::IOException">Cannot open video file with the specified name.</exception>
		/// <exception cref="VideoException">A error occurred while opening the video file. See exception message.</exception>
		///
		void Open(String^ fileName);

		/// <summary>
		/// Read next video frame of the currently opened video file.
		/// </summary>
		/// 
		/// <returns>Returns next video frame of the opened file or <see langword="null"/> if end of
		/// file was reached. The returned video frame has 24 bpp color format.</returns>
		/// 
		/// <exception cref="System::IO::IOException">Thrown if no video file was open.</exception>
		/// <exception cref="VideoException">A error occurred while reading next video frame. See exception message.</exception>
		/// 
		Bitmap^ ReadVideoFrame();

		/// <summary>
		/// Close currently opened video file if any.
		/// </summary>
		/// 
		void Close();

	private:

		int m_width;
		int m_height;
		int     m_frameRate;
		String^ m_codecName;
		Int64 m_framesCount;

	private:
		Bitmap^ DecodeVideoFrame();

		// Checks if video file was opened
		void CheckIfVideoFileIsOpen()
		{
			if (data == nullptr)
			{
				throw gcnew System::IO::IOException("Video file is not open, so can not access its properties.");
			}
		}

		// Check if the object was already disposed
		void CheckIfDisposed()
		{
			if (disposed)
			{
				throw gcnew System::ObjectDisposedException("The object was already disposed.");
			}
		}

	private:
		// private data of the class
		ReaderPrivateData^ data;
		bool disposed;
	};

}