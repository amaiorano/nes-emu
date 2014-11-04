#include "FileStream.h"
#include <cstdarg>

void FileStream::Printf(const char* format, ...)
{
	static char buffer[2048];
	va_list args;
	va_start( args, format );
	int bytesWritten = _vsnprintf(buffer, sizeof(buffer), format, args);
	va_end(args);

	assert(bytesWritten < sizeof(buffer));

	fwrite(buffer, bytesWritten, 1, m_file);
}
