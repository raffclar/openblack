/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include <span>
#include <vector>

#include <GameSocket.h>

/// The world file that Load world file of the editor sends to the game server in parts (editor.world)
namespace openblack::online::editor_world
{

/// The parts of the editor.world frames of a world file, in order. Empty when the file is empty or larger than the limit.
[[nodiscard]] std::vector<game_socket::EditorWorld> Split(std::span<const uint8_t> file);
/// The bytes of a world file on this computer. None when the file does not open.
[[nodiscard]] std::optional<std::vector<uint8_t>> ReadFile(const std::string& path);
/// Writes the bytes into a world file on this computer. False when the file was not written.
[[nodiscard]] bool WriteFile(const std::string& path, std::span<const uint8_t> bytes);

/// Puts the parts of one world file together, in the order that they come. A part 0 starts a new file.
class Upload
{
public:
	enum class Added : uint8_t
	{
		/// More parts must come
		More,
		/// The last part came: Take gives the whole file
		Done,
		/// The part is not the next part of the file, or the file is larger than its size. The upload is empty again.
		Broken,
	};

	[[nodiscard]] Added Add(const game_socket::EditorWorld& part);
	/// The whole file after Done. The upload is empty again.
	[[nodiscard]] std::vector<uint8_t> Take();
	void Clear();

private:
	std::vector<uint8_t> _bytes;
	uint32_t _next {0};
	uint32_t _count {0};
	uint32_t _size {0};
	bool _done {false};
};

} // namespace openblack::online::editor_world
