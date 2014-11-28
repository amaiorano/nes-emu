#include "FileStream.h"
#include <cstdio>
#include <cstdarg>

void FileStream::Printf(const char* format, ...)
{
	static char buffer[2048];
	va_list args;
	va_start( args, format );
	int bytesWritten = vsnprintf(buffer, sizeof(buffer), format, args);
	va_end(args);

	assert(bytesWritten < sizeof(buffer));

	fwrite(buffer, bytesWritten, 1, m_file);
}
