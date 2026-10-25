/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <entt/meta/fwd.hpp>

#include "ECS/SyncedState.h"

namespace openblack::frame_write_guard
{

/// The synced state at one moment: the hash of each part, and the count of synced random numbers drawn
struct Sample
{
	synced_state::Hash hash;
	uint64_t syncedDraws {0};
};

/// The name that a sample gives to the count of synced random numbers, when it changes
inline constexpr std::string_view k_SyncedDrawsPart = "synced random draws";

/// The parts that differ between two samples, in the order of the second sample, and the synced draws when their count
/// changed. A part that only one sample has also differs.
[[nodiscard]] std::vector<std::string> ChangedParts(const Sample& before, const Sample& after);

/// A tool for the frame-rate audit. It finds the frame steps that change the synced world.
///
/// The synced world must change only in the game turns, so that each computer of a network game has the same world at
/// any frame rate. The guard takes a sample of the synced state before and after a frame step. It writes one log line
/// the first time that a step changes a part, with the turn.
class Guard
{
public:
	/// byField: the guard also tells the field of each component that changed. This is slower.
	explicit Guard(bool byField);
	~Guard();
	Guard(const Guard&) = delete;
	Guard& operator=(const Guard&) = delete;

	/// Takes the sample before a frame step
	void Before();
	/// Takes the sample after the frame step and tells the parts that the step changed for the first time
	void After(std::string_view step, uint32_t turn);

	/// Each step and each part that the step changed, as the guard found them
	[[nodiscard]] const std::set<std::pair<std::string, std::string>>& Found() const { return _found; }

private:
	[[nodiscard]] Sample Take() const;

	std::unique_ptr<entt::meta_ctx> _context;
	bool _byField;
	Sample _before;
	std::set<std::pair<std::string, std::string>> _found;
};

} // namespace openblack::frame_write_guard
