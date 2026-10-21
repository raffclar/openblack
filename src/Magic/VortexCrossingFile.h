/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <filesystem>
#include <fstream>
#include <ios>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_set>

#include <entt/entity/entity.hpp>

// The crossing file and the vortex that writes it. The last vortex that started to lead away from the land owns the file.
// The file stays open while that vortex is in the world. When the vortex goes, the file closes. This is also true when
// the land changes and the land clears all its things.

namespace openblack::vortex
{

class CrossingFile
{
public:
	/// Empty the file at the path and give it to the vortex. Every vortex that starts to lead away empties the file.
	/// Returns false when the file does not open.
	bool Start(entt::entity owner, const std::filesystem::path& path)
	{
		Close();
		_file.open(path, std::ios::out | std::ios::trunc | std::ios::binary);
		if (!_file.is_open())
		{
			return false;
		}
		_owner = owner;
		_path = path;
		_contents = std::string {};
		return true;
	}

	/// Open the file at the path again for the vortex, as after a load. The lines in the file stay, and the next text goes
	/// after them. The file does not know the things written before. Returns false when the file is not there.
	bool Resume(entt::entity owner, const std::filesystem::path& path)
	{
		Close();
		_file.open(path, std::ios::in | std::ios::out | std::ios::binary);
		if (!_file.is_open())
		{
			return false;
		}
		// The file keeps what the disk has, so that a save does not read the disk again
		_contents = std::string(std::istreambuf_iterator<char>(_file), std::istreambuf_iterator<char>());
		_file.clear();
		_file.seekp(0, std::ios::end);
		_owner = owner;
		_path = path;
		if (!_file.good())
		{
			_contents.reset();
		}
		return true;
	}

	/// Add the text to the end of the file. The text goes to the disk at once.
	void Write(std::string_view text)
	{
		if (!_file.is_open())
		{
			return;
		}
		_file.write(text.data(), static_cast<std::streamsize>(text.size()));
		_file.flush();
		// After a failed write, what the disk has is not known
		if (_file.good() && _contents.has_value())
		{
			_contents->append(text);
		}
		else
		{
			_contents.reset();
		}
	}

	/// Record the thing as written. Returns false when the file is closed or the file has the thing already.
	[[nodiscard]] bool MarkWritten(entt::entity thing) { return _file.is_open() && _written.insert(thing).second; }

	/// Close the file when the vortex that goes is its owner
	void OwnerGone(entt::entity vortex)
	{
		if (vortex == _owner)
		{
			Close();
		}
	}

	void Close()
	{
		_file.close();
		_owner = entt::null;
		_written.clear();
	}

	[[nodiscard]] bool IsOpen() const { return _file.is_open(); }
	[[nodiscard]] entt::entity Owner() const { return _owner; }

	/// What the open file at the path has on the disk: the text since Start, or the file as Resume found it and the
	/// text after it. Nothing when the file is closed, when the path is another path, or after a failed write.
	[[nodiscard]] std::optional<std::string_view> Contents(const std::filesystem::path& path) const
	{
		if (!_file.is_open() || !_contents.has_value() || path != _path)
		{
			return std::nullopt;
		}
		return std::string_view(*_contents);
	}

private:
	std::fstream _file;
	std::filesystem::path _path;
	std::optional<std::string> _contents;
	entt::entity _owner {entt::null};
	/// The things in the file. The file has each thing one time only. A static thing and the same static things at its
	/// position count as one.
	std::unordered_set<entt::entity> _written;
};

} // namespace openblack::vortex
