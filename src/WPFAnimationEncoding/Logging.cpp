#include "stdafx.h"
#include "Logging.h"
#include "libffmpeg.h"

#define LINE_SZ 1024

namespace WPFAnimationEncoding
{
	static void ErrorCallback(void* ptr, int level, const char* fmt, va_list vl)
	{
		char line[LINE_SZ];
		int printPrefix = 1;
		libffmpeg::av_log_format_line(ptr, level, fmt, vl, line, LINE_SZ, &printPrefix);
		System::Console::Write(gcnew System::String(line));
	}

	Logging::Logging()
	{
	}

	void Logging::Init()
	{
		libffmpeg::av_log_set_callback(ErrorCallback);
	}
}