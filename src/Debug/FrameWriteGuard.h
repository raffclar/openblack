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
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <unordered_map>
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
	/// With the fields: the hash of each field of each entity, by "component.field" and then by the entity
	std::unordered_map<std::string, std::unordered_map<uint32_t, uint64_t>> rows;
	/// With the fields: the hash of each value that each system saves, in the order of its save, by the name of its part
	std::unordered_map<std::string, std::vector<uint64_t>> systemValues;
};

/// The first entity whose field differs between two samples, none when no row differs
[[nodiscard]] std::optional<uint32_t> ChangedEntity(const Sample& before, const Sample& after, const std::string& part);

/// The numbers (from 0, in the order of the save) of the values of a system part that differ between two samples. A
/// value that only one sample has also differs.
[[nodiscard]] std::vector<size_t> ChangedValues(const Sample& before, const Sample& after, const std::string& part);

/// The name that a sample gives to the count of synced random numbers, when it changes
inline constexpr std::string_view k_SyncedDrawsPart = "synced random draws";

/// The start of the name that a sample gives to the order of the rows of a component
inline constexpr std::string_view k_OrderPart = "order of ";

/// The start of the name that a sample gives to the saved state of a system
inline constexpr std::string_view k_SystemPart = "system ";

/// The parts that differ between two samples, in the order of the second sample, and the synced draws when their count
/// changed. A part that only one sample has also differs.
[[nodiscard]] std::vector<std::string> ChangedParts(const Sample& before, const Sample& after);

/// Whether the guard tells the changes of a frame step. A step that loads a land makes a new synced world. That is not a
/// frame step that writes in the world of a game, so the guard does not look at it.
[[nodiscard]] constexpr bool LooksAtStep(uint32_t landLoadsBefore, uint32_t landLoadsAfter)
{
	return landLoadsBefore == landLoadsAfter;
}

/// A tool for the frame-rate audit. It finds the frame steps that change the synced world.
///
/// The synced world must change only in the game turns, so that each computer of a network game has the same world at
/// any frame rate. The guard takes a sample of the synced state before and after a frame step. It writes one log line
/// the first time that a step changes a part, with the turn.
class Guard
{
public:
	/// byField: the guard also tells the field of each component that changed. This is slower. systems: the guard also
	/// looks at the saved state of each system, which the turns can read outside the registry.
	explicit Guard(bool byField, bool systems = false);
	~Guard();
	Guard(const Guard&) = delete;
	Guard& operator=(const Guard&) = delete;

	/// Takes the sample before a frame step. The sample after the last step serves when nothing ran between them.
	void Before();
	/// A step that the guard does not look at ran: the next step takes a new sample before it
	void Forget() { _last.reset(); }
	/// Takes the sample after the frame step and tells the parts that the step changed for the first time
	void After(std::string_view step, uint32_t turn);
	/// Takes the sample after a step of the tools, which can change the synced world, as the testbed runner does in the
	/// debug windows. The guard does not tell the parts. It tells only the turn of the change, once for each turn, so
	/// that a tool can know the changes that the next steps make from it.
	void AfterTool(std::string_view step, uint32_t turn);

	/// Each step and each part that the step changed, as the guard found them
	[[nodiscard]] const std::set<std::pair<std::string, std::string>>& Found() const { return _found; }

private:
	[[nodiscard]] Sample Take() const;

	std::unique_ptr<entt::meta_ctx> _context;
	bool _byField;
	bool _systems;
	Sample _before;
	std::optional<Sample> _last;
	std::set<std::pair<std::string, std::string>> _found;
	/// The turns at which a step of the tools changed the synced world
	std::set<uint32_t> _toolTurns;
};

} // namespace openblack::frame_write_guard
