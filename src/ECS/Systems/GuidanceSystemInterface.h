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
#include <vector>

#include <entt/entity/entity.hpp>

#include "Enums.h"
#include "Help/GuidanceRules.h"

namespace openblack::ecs::systems
{

/// The guidance of the player at this machine (see guidance): when each kind may be heard again, and the advisors'
/// remarks on what the player does to towns and buildings, how their alignment drifts and the full moon. A remark is
/// said through the help's messages, so a story script holding the dialogue keeps it, and is then simply lost.
class GuidanceSystemInterface
{
public:
	/// A remark the advisors were asked to say, for the inspector
	struct Remark
	{
		guidance::Kind kind;
		uint32_t text;
		uint32_t turn;
		/// False when the dialogue was held by a story script
		bool started;
	};

	virtual ~GuidanceSystemInterface() = default;

	/// A new land: each kind is taken as last heard a random while before the land's first turn
	virtual void Reset() = 0;
	/// Once a game turn: the first turn's stamps, and a look at the moon at night
	virtual void ProcessTurn() = 0;

	/// Whether a kind of guidance may be heard now, drawing its wait (the help level, the first land and the wait since
	/// it was last heard)
	virtual bool PlayNow(guidance::Kind kind) = 0;
	/// The same for an advisors' remark: the wait of any remark first, then its own kind's
	virtual bool RemarkNow(guidance::Kind kind) = 0;

	/// Harm done to a town by a player, applied by a creature or by anything else
	virtual void TownAttacked(entt::entity town, PlayerNames attacker, bool byCreature) = 0;
	/// A building broken by a blow of a player's (none for a blow of nobody's), with this share of its model left
	virtual void BuildingBroken(entt::entity building, std::optional<PlayerNames> player, float remaining) = 0;
	/// Each turn, before the player's alignment moves: this turn's change to it, times how fast it moves
	virtual void AlignmentTurn(float change) = 0;
	/// A remark said at most once a land, at its chance
	virtual void SayOneOff(guidance::OneOff oneOff) = 0;
	/// For the testbed: every kind counts as heard as long ago as can be
	virtual void EndWaits() = 0;

	[[nodiscard]] virtual const std::vector<Remark>& GetRemarks() const = 0;
	/// Turns until the moon is looked at again
	[[nodiscard]] virtual int32_t GetMoonWait() const = 0;
};

} // namespace openblack::ecs::systems
