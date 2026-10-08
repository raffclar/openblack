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

#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>

/// (openblack) A hash of the game state at the end of every turn, to prove that two runs of the same game (one thread
/// and two, before and after a refactor) play the same turns. The original has a smaller one for network games, a
/// checksum of the mobile objects sent each turn; this one is not that checksum (not ported) and is never shown to the
/// player.
///
/// Off unless OPENBLACK_STATE_HASH=<file> (or Enable() from a test): then each turn appends one line
/// "turn <n> <total> <part>=<hash> ..." to the file, every part in the order it was registered. Floats are hashed by
/// their bits: a different rounding is a different game.
namespace openblack::state_hash
{

/// FNV-1a, 64 bits
class Hasher
{
public:
	void Bytes(std::span<const std::byte> bytes) noexcept;
	void U32(uint32_t value) noexcept;
	void U64(uint64_t value) noexcept;
	void Float(float value) noexcept;
	void Vec3(const glm::vec3& value) noexcept;
	void Mat3(const glm::mat3& value) noexcept;
	[[nodiscard]] uint64_t Value() const noexcept { return _value; }

private:
	uint64_t _value {0xCBF29CE484222325ull};
};

using Part = std::function<void(Hasher&)>;

/// A part of the state, hashed each turn after the ones registered before it. The owners of a system add their
/// own (their components' game fields); the built-in ones are "random", "clock", "pools" and "transform".
void Register(std::string_view name, Part part);

struct TurnHash
{
	uint32_t turn {0};
	uint64_t total {0};
	std::vector<std::pair<std::string, uint64_t>> parts;
};

/// From OPENBLACK_STATE_HASH: whether the hash is taken (read once, at the first call)
[[nodiscard]] bool Enabled();
/// Tests: take the hashes in memory (and in `file` when not empty) whatever the environment says
void Enable(std::string file = {});
/// The end of a game turn (Game::GameLogicLoop): hash every part
void OnTurnEnd(uint32_t turn);
/// The hashes taken since Enable / the last Clear (tests)
[[nodiscard]] const std::vector<TurnHash>& Taken();
void Clear();

} // namespace openblack::state_hash
