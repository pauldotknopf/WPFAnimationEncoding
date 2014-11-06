#pragma once

#include "libffmpeg.h"

using namespace System;
using namespace System::Drawing;
using namespace System::Drawing::Imaging;

namespace WPFAnimationEncoding
{
	public delegate Bitmap^ VideoReEncodeCallback(Bitmap^ image, bool% cancel);

	ref struct ReaderPrivateData;

	public ref class VideoReEncoder
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
		!VideoReEncoder()
		{
			Close();
		}

	public:

		/// <summary>
		/// Initializes a new instance of the <see cref="VideoFileReader"/> class.
		/// </summary>
		/// 
		VideoReEncoder(void);

		/// <summary>
		/// Disposes the object and frees its resources.
		/// </summary>
		/// 
		~VideoReEncoder()
		{
			this->!VideoReEncoder();
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
		/// Start re encoding the video to a new destination.
		/// </summary>
		/// 
		/// <exception cref="System::IO::IOException">Thrown if no video file was open.</exception>
		/// <exception cref="VideoException">A error occurred while reading next video frame. See exception message.</exception>
		/// 
		void StartReEncoding(String^ fileName, VideoReEncodeCallback^ callback);

		/// <summary>
		/// Close currently opened video file if any.
		/// </summary>
		/// 
		void Close();

	private:

		int m_width;
		int m_height;

	private:
		//Bitmap^ DecodeVideoFrame();

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