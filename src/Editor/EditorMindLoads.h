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

#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

#include <entt/entity/entity.hpp>

#include "Commands/PlayerCommands.h"

namespace openblack::creaturemind
{
struct MindFileData;
}

/// The mind files of the creature tools of the editor in a network game. The mind file is only on the computer of the
/// editor, so the bytes go to the game server on the game socket, and the server sends them to every computer. Then a
/// command of the turns (player_commands::EditorMindLoad) uses the file by the hash of its bytes, and each computer loads
/// the same mind at the same turn.
namespace openblack::editor::mind_loads
{

/// The most mind files that one store keeps. The game server refuses a new file when it keeps this many, because it sends
/// each file again to each player who comes back.
inline constexpr size_t k_MostKeptFiles = 32;

/// The hash of the bytes of a mind file: FNV-1a over all the bytes. Each computer gets the same hash from the same bytes.
[[nodiscard]] uint64_t HashOf(std::span<const uint8_t> bytes) noexcept;

/// The mind files of one network game, in the order that they came, by the hash of their bytes. The game server and each
/// client keep one.
class MindFileStore
{
public:
	/// What an add did
	enum class Added : uint8_t
	{
		/// The store keeps the new file
		New,
		/// The store kept a file with the same hash before
		Known,
		/// The store keeps the most files, or the file is empty: it does not keep the new file
		Refused,
	};

	/// Keeps a file. Gives what the add did and the hash of the bytes.
	[[nodiscard]] std::pair<Added, uint64_t> Add(std::vector<uint8_t> bytes);
	/// The bytes of the file with the hash, or none when the store does not keep it
	[[nodiscard]] std::span<const uint8_t> Find(uint64_t hash) const noexcept;
	/// The files in the order that they came
	[[nodiscard]] const std::vector<std::pair<uint64_t, std::vector<uint8_t>>>& Files() const noexcept { return _files; }
	void Clear() noexcept { _files.clear(); }

private:
	std::vector<std::pair<uint64_t, std::vector<uint8_t>>> _files;
};

/// Reads the bytes of a mind file as the creature tools read a file that they open. None when the bytes do not read.
[[nodiscard]] std::shared_ptr<const creaturemind::MindFileData> ReadMind(std::span<const uint8_t> bytes);

/// Why the world does not take the command, or none when it takes it. kept tells whether the store of the computer keeps
/// the file of the command. The rules keep no state.
[[nodiscard]] std::optional<std::string_view> MindLoadRefusal(const player_commands::EditorMindLoad& load, bool kept) noexcept;

/// What the world of a computer gives to a mind load at the turn. The game gives the world of the Locator, and the tests
/// give fakes.
struct MindLoadWorld
{
	/// True when the thing is a creature that has a mind
	std::function<bool(entt::entity)> isCreature;
	/// Makes a new creature of the editor. Gives entt::null when the world refused it.
	std::function<entt::entity(const player_commands::EditorMindLoad&)> spawn;
	/// The creature takes the mind of the file
	std::function<void(entt::entity, std::shared_ptr<const creaturemind::MindFileData>)> loadMind;
};

/// What a mind load did at the turn
struct MindLoadResult
{
	/// The creature that took the mind, or entt::null when the world refused the command
	entt::entity creature {entt::null};
	/// Why the world refused the command. Empty when the creature took the mind.
	std::string_view refusal;
};

/// Does a mind load at the turn, as each computer of a network game does it. file is the bytes of the file with the hash
/// of the command, from the store of this computer, or none.
[[nodiscard]] MindLoadResult ApplyMindLoad(const player_commands::EditorMindLoad& load, std::span<const uint8_t> file,
                                           const MindLoadWorld& world);

/// Does a mind load at the turn in the world of the Locator
[[nodiscard]] MindLoadResult RunMindLoad(const player_commands::EditorMindLoad& load, std::span<const uint8_t> file);

/// Sends a mind file and its command: first the bytes on the game socket, then the command of the turns. The hash of the
/// command comes from the bytes. False when the game socket did not take the bytes: then the command does not go.
[[nodiscard]] bool SendMindLoad(std::span<const uint8_t> file, player_commands::EditorMindLoad load);

} // namespace openblack::editor::mind_loads
