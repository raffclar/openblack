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
#include <string_view>
#include <utility>
#include <vector>

/// The rules of the debug window that shows the hash of a land's synced state, to tell whether two runs stay the same.
/// This is developer tooling: it only reads the world.
namespace openblack::state_hash
{

/// The hash of the synced state at a turn, and the hash of each of its parts
struct Snapshot
{
	uint32_t turn {0};
	uint64_t hash {0};
	std::vector<std::pair<std::string, uint64_t>> parts;
};

/// A part whose hash is not the same in the two snapshots, or that only one of them has
struct Difference
{
	std::string part;
	std::optional<uint64_t> pinned;
	std::optional<uint64_t> current;
};

enum class Match : uint8_t
{
	/// No snapshot is pinned to compare with
	NothingPinned,
	Same,
	Different,
};

/// The hash as 16 hex digits, as the run records and the inspector give it
[[nodiscard]] std::string HexOf(uint64_t hash);

/// The parts that differ, by their names in order
[[nodiscard]] std::vector<Difference> Differences(const Snapshot& pinned, const Snapshot& current);

/// Whether the current snapshot has the pinned hash. The turns are not compared: a snapshot of another turn can only be
/// the same when the world did not change.
[[nodiscard]] Match Compare(const std::optional<Snapshot>& pinned, const Snapshot& current);

/// A line to copy into a note or a check: the turn and the hash
[[nodiscard]] std::string Line(const Snapshot& snapshot);

/// The snapshot from a line that Line gave. None for other text.
[[nodiscard]] std::optional<Snapshot> FromLine(std::string_view text);

/// Whether a snapshot is due at this turn: with every turns, at each turn that is a multiple of it that was not taken
/// yet; 0 for never
[[nodiscard]] bool Due(uint32_t every, uint32_t turn, std::optional<uint32_t> lastTurn);

} // namespace openblack::state_hash
