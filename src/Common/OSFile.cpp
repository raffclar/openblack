/* OpenBlack - A reimplementation of Lionhead's Black & White.
 *
 * OpenBlack is the legal property of its developers, whose names
 * can be found in the AUTHORS.md file distributed with this source
 * distribution.
 *
 * OpenBlack is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 3
 * of the License, or (at your option) any later version.
 *
 * OpenBlack is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with OpenBlack. If not, see <http://www.gnu.org/licenses/>.
 */

#include <Common/OSFile.h>
#include <cassert>
#include <sstream>
#include <stdexcept>

using namespace OpenBlack;

#if FILE_API == FILE_API_POSIX
OSFile::OSFile()
{
	mHandle = -1;
}

OSFile::~OSFile()
{
	if (mHandle != -1)
		close(mHandle);
}

void OSFile::Open(char const* filename, LH_FILE_MODE mode)
{
	assert(mHandle == -1);

	int handle = -1;
	if (mode == LH_FILE_MODE::Read)
		handle = open(filename, O_RDONLY);
	else if (mode == LH_FILE_MODE::Create)
		handle = open(filename, O_CREAT | O_EXCL | O_RDWR, 0600);

	if (handle == -1)
	{
		std::ostringstream os;
		os << "Failed to open '" << filename << "' for reading.";
		throw std::runtime_error(os.str());
	}

	mHandle = handle;
}

void OSFile::Close()
{
	assert(mHandle != -1);

	close(mHandle);
	mHandle = -1;
}

size_t OSFile::Read(void* data, size_t size)
{
	assert(mHandle != -1);

	ssize_t readb;

	readb = read(mHandle, data, size);
	if (readb == -1)
		throw std::runtime_error("A read operation on a file failed.");

	return (size_t)read;
}

void OSFile::Seek(size_t position, LH_SEEK_MODE mode)
{
	assert(mHandle != -1);

	int seekMode;
	switch (mode)
	{
	case LH_SEEK_MODE::Set:
		seekMode = SEEK_SET;
		break;
	case LH_SEEK_MODE::Current:
		seekMode = SEEK_CUR;
		break;
	case LH_SEEK_MODE::End:
		seekMode = SEEK_END;
		break;
	default:
		throw std::runtime_error("Invalid seek mode.");
		break;
	}

	if (lseek(mHandle, position, seekMode) == -1)
		throw std::runtime_error("A seek operation on a file failed.");
}

size_t OSFile::Position()
{
	assert(mHandle != -1);

	off_t value = lseek(mHandle, 0, SEEK_CUR);

	if (value == -1)
		throw std::runtime_error("A query operation on a file failed.");

	return value;
}

size_t OSFile::Size()
{
	assert(mHandle != -1);

	struct stat info;
	if (fstat(mHandle, &info) != 0)
		throw std::runtime_error("A query operation on a file failed.");

	// should be fine, no files are over 4GB..
	if (info.st_size > SIZE_MAX)
		throw std::runtime_error("Files greater that 4GB are not supported.");

	return info.st_size;
}

/* static */

bool OSFile::Delete(const char* filename)
{
	return !unlink(filename);
}

bool OSFile::Rename(const char* srcfile, const char* dstfile)
{
	return !rename(srcfile, dstfile);
}

bool OSFile::Exists(const char* filename)
{
	return !access(filename, F_OK);
}
#endif

#if FILE_API == FILE_API_WIN32
OSFile::OSFile()
{
	mHandle = INVALID_HANDLE_VALUE;
}

OSFile::~OSFile()
{
	if (mHandle != INVALID_HANDLE_VALUE)
		CloseHandle(mHandle);
}

void OSFile::Open(char const* filename, LH_FILE_MODE mode)
{
	assert(mHandle == INVALID_HANDLE_VALUE);

	HANDLE handle = INVALID_HANDLE_VALUE;
	if (mode == LH_FILE_MODE::Read)
		handle = CreateFileA(filename, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
	else if (mode == LH_FILE_MODE::Create)
		handle = CreateFileA(filename, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, CREATE_ALWAYS, 0, 0);

	if (handle == INVALID_HANDLE_VALUE)
	{
		std::ostringstream os;
		os << "Failed to open '" << filename << "' for reading.";
		throw std::runtime_error(os.str());
	}

	mHandle = handle;
}

void OSFile::Close()
{
	assert(mHandle != INVALID_HANDLE_VALUE);

	CloseHandle(mHandle);
	mHandle = INVALID_HANDLE_VALUE;
}

size_t OSFile::Read(void* data, size_t size)
{
	assert(mHandle != INVALID_HANDLE_VALUE);

	DWORD read;

	if (!ReadFile(mHandle, data, size, &read, NULL))
		throw std::runtime_error("A read operation on a file failed.");

	return read;
}

void OSFile::Seek(size_t position, LH_SEEK_MODE mode)
{
	assert(mHandle != INVALID_HANDLE_VALUE);

	int seekMode;
	switch (mode)
	{
	case LH_SEEK_MODE::Set:
		seekMode = SEEK_SET;
		break;
	case LH_SEEK_MODE::Current:
		seekMode = SEEK_CUR;
		break;
	case LH_SEEK_MODE::End:
		seekMode = SEEK_END;
		break;
	default:
		throw std::runtime_error("Invalid seek mode.");
		break;
	}

	if (SetFilePointer(mHandle, position, NULL, seekMode) == INVALID_SET_FILE_POINTER)
		if (GetLastError() != NO_ERROR)
			throw std::runtime_error("A seek operation on a file failed.");
}

size_t OSFile::Position()
{
	assert(mHandle != INVALID_HANDLE_VALUE);

	DWORD value = SetFilePointer(mHandle, 0, NULL, SEEK_CUR);

	if (value == INVALID_SET_FILE_POINTER && GetLastError() != NO_ERROR)
		throw std::runtime_error("A query operation on a file failed.");

	return value;
}

size_t OSFile::Size()
{
	assert(mHandle != INVALID_HANDLE_VALUE);

	BY_HANDLE_FILE_INFORMATION info;

	if (!GetFileInformationByHandle(mHandle, &info))
		throw std::runtime_error("A query operation on a file failed.");

	// should be fine, no files are over 4GB..
	if (info.nFileSizeHigh != 0)
		throw std::runtime_error("Files greater that 4GB are not supported.");

	return info.nFileSizeLow;
}

/* static */

bool OSFile::Delete(const char* filename)
{
	return DeleteFileA(filename);
}

bool OSFile::Rename(const char* srcfile, const char* dstfile)
{
	return MoveFileA(srcfile, dstfile);
}

bool OSFile::Exists(const char* filename)
{
	DWORD result = GetFileAttributesA(filename);
	DWORD error  = GetLastError();
	return !(result == INVALID_FILE_ATTRIBUTES && (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND));
}

#endif

/* Non platform specific */

// Lookup is strcmp until it finds it :)

// todo: this sucks shit, I shouldn't read the entire file into a buffer
bool OSFile::GetSegment(const char* segmentName, OpenBlack::LHSegment* segment, bool allocateMemory)
{
	Seek(0, LH_SEEK_MODE::Set);
	size_t fileSize = this->Size();

	void* buffer = malloc(fileSize);
	Read(buffer, fileSize);

	char* segmentFull = new char[32];
	memset(segmentFull, 0, 32);
	strcpy(segmentFull, segmentName);

	char* cur = (char*)buffer;
	while (strncmp(cur, segmentFull, 32)) cur++; // todo: check this doesn't exceed total file length

	memcpy(segment, cur, 36);
	segment->SegmentData = malloc(segment->SegmentSize);
	memcpy(segment->SegmentData, cur + 36, segment->SegmentSize);

	free(buffer);

	return true;
}

std::string OSFile::ReadString()
{
	std::string str;
	char c;

	do
	{
		Read(&c, 1);
		str.push_back(c);
	} while (c != '\0');

	return str;
}

char* OSFile::ReadAll(const char* filename, size_t* sizeOut)
{
	OSFile* file = new OSFile;
	file->Open(filename, LH_FILE_MODE::Read);

	size_t fileSize = file->Size();

	char* buffer = new char[fileSize + 1];

	file->Read(buffer, fileSize);

	file->Close();

	if (sizeOut != 0)
		*sizeOut = fileSize + 1;

	buffer[fileSize] = 0;

	return buffer;
}
