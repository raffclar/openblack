/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <optional>
#include <span>
#include <utility>
#include <vector>

#include <GameSocket.h>

/// The files of the script tabs of the editor in a network game: a compiled script program, the source of a script and the
/// text of a land script. The file is only on the computer of the editor, so its bytes go to the game server in parts on
/// the game socket, and the server sends them to every computer. Then a command of the turns uses the file by the hash of
/// its bytes, and each computer uses the same file at the same turn.
namespace openblack::editor::script_files
{

using Kind = game_socket::EditorScriptFile::Kind;

/// The most files that one store keeps. The game server sends each file again to each player who comes back.
inline constexpr size_t k_MostKeptFiles = 16;
/// The most bytes of all the files that one store keeps
inline constexpr size_t k_MostKeptBytes = 32 * 1024 * 1024;

/// The frames that send a file, in order. Each part but the last holds game_socket::k_MostEditorScriptPartBytes. None
/// when the file is empty or larger than game_socket::k_MostEditorScriptFileBytes.
[[nodiscard]] std::vector<game_socket::EditorScriptFile> SplitFile(Kind kind, std::span<const uint8_t> bytes);

/// Puts the parts of one file together again. The parts of a file come in order on one game socket, and one file comes
/// after the other.
class PartAssembly
{
public:
	/// What a part did
	enum class Step : uint8_t
	{
		/// The file needs more parts
		Waiting,
		/// The part was the last, and the bytes have the hash of the file. Take() gives the file.
		Done,
		/// The part does not continue the file, or the bytes do not have the hash: the assembly forgets the file
		Bad,
	};

	/// A whole file
	struct File
	{
		Kind kind {Kind::Program};
		uint64_t hash {0};
		std::vector<uint8_t> bytes;
	};

	[[nodiscard]] Step Add(const game_socket::EditorScriptFile& part);
	/// The file after the step Done. The assembly then waits for the first part of the next file.
	[[nodiscard]] std::optional<File> Take();

private:
	std::optional<game_socket::EditorScriptFile> _first;
	std::vector<uint8_t> _bytes;
	uint32_t _next {0};
	bool _done {false};
};

/// The files of one network game, in the order that they came, by the hash of their bytes. The game server and each client
/// keep one.
class FileStore
{
public:
	/// What an add did
	enum class Added : uint8_t
	{
		/// The store keeps the new file
		New,
		/// The store kept a file of the same kind with the same hash before
		Known,
		/// The store keeps the most files or the most bytes, or the file is empty: it does not keep the new file
		Refused,
	};

	using File = PartAssembly::File;

	/// Keeps a file. Gives what the add did and the hash of the bytes.
	[[nodiscard]] std::pair<Added, uint64_t> Add(Kind kind, std::vector<uint8_t> bytes);
	/// The bytes of the file of the kind with the hash, or none when the store does not keep it
	[[nodiscard]] std::span<const uint8_t> Find(Kind kind, uint64_t hash) const noexcept;
	/// The files in the order that they came
	[[nodiscard]] const std::vector<File>& Files() const noexcept { return _files; }
	void Clear() noexcept
	{
		_files.clear();
		_bytes = 0;
	}

private:
	std::vector<File> _files;
	size_t _bytes {0};
};

} // namespace openblack::editor::script_files
