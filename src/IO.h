#pragma once

// Input/Output utilities inspired largely by .NET's System.IO

#include <string>

namespace IO
{
	namespace Path
	{
		typedef std::string string;

		const char DirectorySeparatorChar = '\\';
		const char AltDirectorySeparatorChar = '/';
		const char DirectorySeparatorChars[] = {DirectorySeparatorChar, AltDirectorySeparatorChar, 0};

		const char ExtensionSeparatorChar = '.';

		string GetDirectoryName(const string& path);
		string GetFileName(const string& path);
		string GetFileNameWithoutExtension(const string& path);
		string Combine(const string& path1, const string& path2);
		string ChangeExtension(const string& path, const string& extension);

	} // namespace Path
} // namespace IO


// Inline implementation
namespace IO
{
	namespace Path
	{
		inline string GetDirectoryName(const string& path)
		{
			size_t pos = path.find_last_of(DirectorySeparatorChars);
			if (pos != std::string::npos)
			{
				return path.substr(0, pos);
			}
			return "";
		}

		inline string GetFileName(const string& path)
		{
			size_t pos = path.find_last_of(DirectorySeparatorChars);
			if (pos != std::string::npos)
			{
				return path.substr(pos + 1);
			}
			return path;
		}

		inline string GetFileNameWithoutExtension(const string& path)
		{
			string filename = GetFileName(path);
			size_t pos = filename.find_last_of(ExtensionSeparatorChar);
			if (pos != std::string::npos)
			{
				return filename.substr(0, pos);
			}
			return filename;
		}

		inline string Combine(const string& path1, const string& path2)
		{
			if (path1.empty()) return path2;
			if (path2.empty()) return path1;
			return path1 + DirectorySeparatorChar + path2;
		}

		inline string ChangeExtension(const string& path, const string& extension)
		{
			const string& d = GetDirectoryName(path);
			const string& f = GetFileNameWithoutExtension(path);
			const string& e = (extension[0] != ExtensionSeparatorChar)? ("." + extension) : extension;
			return Combine(d, f + e);
		}

	} // namespace Path
} // namespace IO
