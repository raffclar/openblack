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

#include "ECS/Systems/SavedStateInterface.h"
#include "Enums.h"
#include "Help/GuidanceRules.h"
#include "Help/VillageVoices.h"

namespace openblack::ecs::systems
{

/// The guidance of the player at this machine (see guidance): when each kind may be heard again, and the advisors'
/// remarks on what the player does to towns and buildings, how their alignment drifts and the full moon. A remark is
/// said through the help's messages, so a story script holding the dialogue keeps it, and is then simply lost.
class GuidanceSystemInterface: public SavedStateInterface
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

		/// Gives the values that a saved game keeps to a visitor
		template <typename Self, typename Visitor>
		static void VisitSaved(Self& self, Visitor&& visit)
		{
			visit(self.kind, self.text, self.turn, self.started);
		}
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
	/// An advisors' remark of a kind, said when RemarkNow lets it: one of the kind's texts, picked at random
	virtual void RemarkIfNow(guidance::Kind kind) = 0;

	/// Harm done to a town by a player, applied by a creature or by anything else
	virtual void TownAttacked(entt::entity town, PlayerNames attacker, bool byCreature) = 0;
	/// Harm done to a town: sometimes its people cry out at the town, in a voice that tells what harmed it. The town must
	/// have people, and the guidance kind of the cry must wait no longer.
	virtual void TownAttackCry(entt::entity town, const guidance::TownAttackVoice& voice) = 0;
	/// A building broken by a blow of a player's (none for a blow of nobody's), with this share of its model left
	virtual void BuildingBroken(entt::entity building, std::optional<PlayerNames> player, float remaining) = 0;
	/// Each turn, before the player's alignment moves: this turn's change to it, times how fast it moves
	virtual void AlignmentTurn(float change) = 0;
	/// A remark said at most once a land, at its chance
	virtual void SayOneOff(guidance::OneOff oneOff) = 0;
	/// The player here killed a villager: the advisors speak of it when the villager is on screen
	virtual void KilledVillager(entt::entity villager) = 0;
	/// A villager of the player here died of chanting: the advisors warn of the worshippers that die
	virtual void WorshipperDied() = 0;
	/// Another player took a villager of the player here: the advisors warn of the loss when the villager died near the
	/// point under the cursor
	virtual void LosingVillager(entt::entity villager) = 0;
	/// A town of the player here has few adults left: the advisors warn of it when the town has people and is near the
	/// point under the cursor
	virtual void LowOnPeople(entt::entity town) = 0;
	/// For the testbed: every kind counts as heard as long ago as can be
	virtual void EndWaits() = 0;

	[[nodiscard]] virtual const std::vector<Remark>& GetRemarks() const = 0;
	/// Turns until the moon is looked at again
	[[nodiscard]] virtual int32_t GetMoonWait() const = 0;
};

} // namespace openblack::ecs::systems
