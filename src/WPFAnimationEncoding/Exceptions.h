#pragma once
public ref class VideoException : System::Exception
{
public:
	VideoException(System::String^ message)
		: System::Exception(message) { };
};