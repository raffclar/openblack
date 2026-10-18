/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ComputerPlayerStart.h"

#include <algorithm>

#include "Creature/CreatureMindFileBody.h"
#include "ECS/PlayerMiracles.h"

namespace openblack::computer_player
{

void SwitchOn(ecs::components::ComputerPlayer& player, bool on)
{
	player.switchedOn = on;
	if (on)
	{
		player.marked = true;
	}
}

bool DueToStart(const ecs::components::ComputerPlayer& player)
{
	return player.switchedOn && !player.started;
}

bool GetsCreature(const StartFacts& facts)
{
	return facts.temple && facts.pen && !facts.creature && facts.skirmish && !facts.network;
}

bool TakesSize(const ecs::components::ComputerPlayer& player, const StartFacts& facts)
{
	return !facts.local && facts.creature && player.creatureLike.has_value() && facts.localCreature;
}

bool LearnsMiracles(const ecs::components::ComputerPlayer& player, const StartFacts& facts)
{
	return !facts.local && facts.creature && player.marked;
}

CreatureType SpeciesFor(uint32_t roll)
{
	return creature_mind_body::SpeciesFromRow(roll).value_or(CreatureType::GiantApe);
}

float SizeLike(float localShownSize, float addedShare)
{
	// The game adds and multiplies at double precision and keeps the result as a float
	return static_cast<float>((static_cast<double>(addedShare) + static_cast<double>(k_SmallestShareOfSize)) *
	                          static_cast<double>(localShownSize));
}

std::vector<MagicType> MiraclesToLearn(const ecs::components::Player::Miracles& miracles, bool temple)
{
	std::vector<MagicType> learnt;
	if (!temple)
	{
		return learnt;
	}
	for (size_t index = 1; index < ecs::components::Player::k_MagicTypeCount; ++index)
	{
		const auto type = static_cast<MagicType>(index);
		if (player_miracles::IsMagicTypeEnabled(miracles, type) ||
		    std::ranges::find(k_AlwaysKnown, type) != k_AlwaysKnown.end())
		{
			learnt.push_back(type);
		}
	}
	return learnt;
}

namespace
{
StartFacts FactsOf(const World& world, PlayerNames player)
{
	const auto local = world.LocalPlayer();
	return {
	    .local = player == local,
	    .temple = world.HasTemple(player),
	    .pen = world.Pen(player).has_value(),
	    .creature = world.Creature(player).has_value(),
	    .skirmish = world.Skirmish(),
	    .network = false,
	    .localCreature = world.Creature(local).has_value(),
	};
}
} // namespace

void Start(World& world, PlayerNames player, ecs::components::ComputerPlayer& state)
{
	state.started = true;
	// A player with a temple but no creature gets a creature of a random species at the pen of the temple
	if (GetsCreature(FactsOf(world, player)))
	{
		const auto species = SpeciesFor(world.GameRand(k_SpeciesChoices));
		world.MakeCreature(player, species, *world.Pen(player));
	}
	// The creature that the player has now, also a creature made above, gets its size and its miracles
	const auto facts = FactsOf(world, player);
	const auto creature = world.Creature(player);
	if (!creature.has_value())
	{
		return;
	}
	if (TakesSize(state, facts))
	{
		const auto localSize = world.ShownSize(*world.Creature(world.LocalPlayer()));
		world.SetSize(*creature, SizeLike(localSize, world.GameFloatRand(k_MostAddedShare)));
	}
	if (LearnsMiracles(state, facts))
	{
		for (const auto miracle : MiraclesToLearn(world.Miracles(player), facts.temple))
		{
			world.Teach(*creature, miracle);
		}
	}
}

} // namespace openblack::computer_player
