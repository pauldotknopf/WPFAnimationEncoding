#pragma once

#include "libffmpeg.h"

using namespace System;
using namespace System::Drawing;
using namespace System::Drawing::Imaging;

namespace WPFAnimationEncoding
{
	public delegate Bitmap^ VideoReEncodeCallback(Bitmap^ image, bool% cancel);

	public ref class VideoReEncoder
	{
	public:

	protected:

		/// <summary>
		/// Object's finalizer.
		/// </summary>
		!VideoReEncoder() { }

	public:

		/// <summary>
		/// Initializes a new instance of the <see cref="VideoFileReader"/> class.
		/// </summary>
		VideoReEncoder(void);

		/// <summary>
		/// Disposes the object and frees its resources.
		/// </summary>
		~VideoReEncoder()
		{
			this->!VideoReEncoder();
			disposed = true;
		}

		/// <summary>
		/// Start re encoding the video to a new destination.
		/// </summary>
		void StartReEncoding(String^ inputFileName, String^ outputFileName, VideoReEncodeCallback^ callback);

	private:
		
		// Check if the object was already disposed
		void CheckIfDisposed()
		{
			if (disposed)
			{
				throw gcnew System::ObjectDisposedException("The object was already disposed.");
			}
		}

	private:
		bool disposed;
	};
}