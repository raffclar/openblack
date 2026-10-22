/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// A read-only file system for the web build that holds the user's install of Black & White. The page gives the files
// of the folder that the user picks, and each thread of the game reads slices of them in place, with no copy. The
// names match without letter case, as on Windows.

#include <cerrno>
#include <cstdlib>

#include <algorithm>
#include <atomic>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include <emscripten.h>
#include <emscripten/wasmfs.h>
#include <fcntl.h>

// The headers of the WebAssembly file system of Emscripten, for a back end of our own
#include <backend.h>
#include <file.h>
#include <wasmfs.h>

#include "FileSystem/UserFolderIndex.h"
#include "WebPlatform.h"

// The page posts the folder to each worker thread of the game (see web/page/pre.js). A worker can read a part of a file
// at once with FileReaderSync, which the page's main thread does not have.
// clang-format off
EM_JS(char*, openblack_user_folder_list, (), {
	const folder = globalThis.openblackWeb && globalThis.openblackWeb.folder;
	return folder ? stringToNewUTF8(folder.list) : 0;
});

EM_JS(double, openblack_user_folder_read, (uint32_t file, uint8_t* buffer, double length, double offset), {
	try {
		const blob = globalThis.openblackWeb.folder.files[file];
		const bytes = new FileReaderSync().readAsArrayBuffer(blob.slice(offset, offset + length));
		HEAPU8.set(new Uint8Array(bytes), buffer);
		return bytes.byteLength;
	} catch (error) {
		console.error('openblack: the game cannot read a file of the game folder:', error);
		return -1;
	}
});
EM_JS_DEPS(openblack_user_folder, "$stringToNewUTF8");
// clang-format on

namespace
{

using openblack::filesystem::LowerName;
using openblack::filesystem::UserFolderIndex;

/// A file of the user's folder. The file is read-only.
class UserFolderFile final: public wasmfs::DataFile
{
public:
	UserFolderFile(wasmfs::backend_t backend, uint32_t file, uint64_t size)
	    : wasmfs::DataFile(S_IRUGO, backend)
	    , _file(file)
	    , _size(static_cast<off_t>(size))
	{
	}

private:
	int open(wasmfs::oflags_t flags) override { return (flags & O_ACCMODE) == O_RDONLY ? 0 : -EROFS; }
	int close() override
	{
		_block.clear();
		_block.shrink_to_fit();
		return 0;
	}
	ssize_t read(uint8_t* buffer, size_t length, off_t offset) override
	{
		if (offset >= _size)
		{
			return 0;
		}
		const auto count = std::min<off_t>(static_cast<off_t>(length), _size - offset);
		// Each read of the page takes a long time, whatever its size, and the game reads many files in small parts. Thus
		// a small read gets its bytes from a block of the file that the last read kept.
		if (count >= k_BlockSize)
		{
			return ReadFromPage(buffer, count, offset);
		}
		const bool inBlock = offset >= _blockStart && offset + count <= _blockStart + static_cast<off_t>(_block.size());
		if (!inBlock)
		{
			const off_t start = offset - offset % k_BlockSize;
			const off_t end = std::min<off_t>(_size, std::max<off_t>(start + k_BlockSize, offset + count));
			_block.resize(static_cast<size_t>(end - start));
			if (ReadFromPage(_block.data(), end - start, start) != end - start)
			{
				_block.clear();
				return -EIO;
			}
			_blockStart = start;
		}
		std::copy_n(_block.begin() + (offset - _blockStart), count, buffer);
		return static_cast<ssize_t>(count);
	}
	ssize_t ReadFromPage(uint8_t* buffer, off_t count, off_t offset) const
	{
		const auto done = openblack_user_folder_read(_file, buffer, static_cast<double>(count), static_cast<double>(offset));
		return done < 0 ? -EIO : static_cast<ssize_t>(done);
	}
	ssize_t write(const uint8_t*, size_t, off_t) override { return -EROFS; }
	int setSize(off_t) override { return -EROFS; }
	int flush() override { return 0; }
	off_t getSize() override { return _size; }

	/// The size of the block that a small read keeps
	static constexpr off_t k_BlockSize = 1024 * 1024;

	uint32_t _file;
	off_t _size;
	std::vector<uint8_t> _block;
	off_t _blockStart {0};
};

/// A folder of the user's folder. It finds its entries with any letter case and gives them with their names on the disk.
class UserFolderDirectory final: public wasmfs::Directory
{
public:
	struct Child
	{
		std::string name;
		std::shared_ptr<wasmfs::File> file;
	};

	explicit UserFolderDirectory(wasmfs::backend_t backend)
	    : wasmfs::Directory(S_IRUGO | S_IXUGO, backend)
	{
	}

	void Add(std::string key, Child child) { _children.emplace(std::move(key), std::move(child)); }

private:
	std::shared_ptr<wasmfs::File> getChild(const std::string& name) override
	{
		const auto found = _children.find(LowerName(name));
		return found == _children.end() ? nullptr : found->second.file;
	}
	std::shared_ptr<wasmfs::DataFile> insertDataFile(const std::string&, mode_t) override { return nullptr; }
	std::shared_ptr<wasmfs::Directory> insertDirectory(const std::string&, mode_t) override { return nullptr; }
	std::shared_ptr<wasmfs::Symlink> insertSymlink(const std::string&, const std::string&) override { return nullptr; }
	int insertMove(const std::string&, std::shared_ptr<wasmfs::File>) override { return -EROFS; }
	int removeChild(const std::string&) override { return -EROFS; }
	ssize_t getNumEntries() override { return static_cast<ssize_t>(_children.size()); }
	MaybeEntries getEntries() override
	{
		std::vector<Entry> entries;
		entries.reserve(_children.size());
		for (const auto& [key, child] : _children)
		{
			entries.push_back({.name = child.name, .kind = child.file->kind, .ino = child.file->getIno()});
		}
		return {entries};
	}
	std::string getName(std::shared_ptr<wasmfs::File> file) override
	{
		for (const auto& [key, child] : _children)
		{
			if (child.file == file)
			{
				return child.name;
			}
		}
		return "";
	}
	// The folder keeps one object for each entry, so the file system does not need a cache of its own
	bool maintainsFileIdentity() override { return true; }

	std::map<std::string, Child> _children;
};

/// The back end that holds the tree of the user's folder. It gives its root folder once, when the folder is put in the
/// file system.
class UserFolderBackend final: public wasmfs::Backend
{
public:
	explicit UserFolderBackend(UserFolderIndex index)
	    : _index(std::move(index))
	{
	}

	std::shared_ptr<wasmfs::DataFile> createFile(mode_t) override { return nullptr; }
	std::shared_ptr<wasmfs::Directory> createDirectory(mode_t) override
	{
		if (_given)
		{
			return nullptr;
		}
		_given = true;
		return MakeFolder(0);
	}
	std::shared_ptr<wasmfs::Symlink> createSymlink(std::string) override { return nullptr; }

private:
	std::shared_ptr<UserFolderDirectory> MakeFolder(uint32_t node)
	{
		auto folder = std::make_shared<UserFolderDirectory>(this);
		for (const auto& [key, child] : _index.Nodes()[node].children)
		{
			const auto& entry = _index.Nodes()[child];
			std::shared_ptr<wasmfs::File> file;
			if (entry.file.has_value())
			{
				file = std::make_shared<UserFolderFile>(this, *entry.file, entry.size);
			}
			else
			{
				file = MakeFolder(child);
			}
			file->locked().setParent(folder);
			folder->Add(key, {.name = entry.name, .file = std::move(file)});
		}
		return folder;
	}

	UserFolderIndex _index;
	bool _given {false};
};

} // namespace

bool openblack::web::MountUserFolder(std::string_view path)
{
	const std::unique_ptr<char, decltype(&std::free)> list(openblack_user_folder_list(), &std::free);
	if (list == nullptr)
	{
		return false;
	}
	auto index = UserFolderIndex::Parse(list.get());
	if (index.Nodes()[0].children.empty())
	{
		return false;
	}
	const auto backend = wasmfs::wasmFS.addBackend(std::make_unique<UserFolderBackend>(std::move(index)));
	// The public header of the file system names the same back end with a type of its own
	return wasmfs_create_directory(std::string(path).c_str(), S_IRUGO | S_IXUGO, reinterpret_cast<::backend_t>(backend)) == 0;
}
