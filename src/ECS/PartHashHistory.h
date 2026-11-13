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

#include <deque>
#include <memory>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace openblack::synced_state
{

/// The hash of each part of the synced world for the last turns. When the world of a network game parts from the
/// server's world at a turn, the log compares the parts of that turn on the two computers, to name the part that
/// differs. The names of the parts are the same from turn to turn, so the history keeps one list of names for many turns.
class PartHashHistory
{
public:
	using Parts = std::vector<std::pair<std::string, uint64_t>>;

	/// The turns that the history keeps. A client compares the checksum of a turn when the server's checksum of the
	/// turn comes, and comes back to the server some turns after that.
	static constexpr size_t k_KeptTurns = 600;

	/// Keeps the parts of a turn. A turn that is not after the last kept turn removes the kept turns from that turn on,
	/// as a world that loads the server's state goes on from another turn.
	void Keep(uint32_t turn, std::span<const std::pair<std::string, uint64_t>> parts);
	/// The parts of a turn, empty when the history does not keep the turn
	[[nodiscard]] Parts Of(uint32_t turn) const;
	[[nodiscard]] size_t Turns() const { return _entries.size(); }
	void Clear() { _entries.clear(); }

private:
	struct Entry
	{
		uint32_t turn {0};
		std::shared_ptr<const std::vector<std::string>> names;
		std::vector<uint64_t> values;
	};
	std::deque<Entry> _entries;
	/// The names of the parts of the last kept turn
	std::shared_ptr<const std::vector<std::string>> _names;
};

} // namespace openblack::synced_state
