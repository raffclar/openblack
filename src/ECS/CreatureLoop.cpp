/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureLoop.h"

#include <cstdio>
#include <cstdlib>

#include <chrono>

#include "Creature/LeashKeys.h"
#include "Creature/LocalPlayer.h"
#include "ECS/PlayerCreature.h"
#include "ECS/Systems/CreatureAnimationSystemInterface.h"
#include "ECS/Systems/CreatureAudioSystemInterface.h"
#include "ECS/Systems/CreatureFightSystemInterface.h"
#include "ECS/Systems/CreatureHairSystemInterface.h"
#include "ECS/Systems/CreatureLocomotionSystemInterface.h"
#include "ECS/Systems/CreatureMindSystemInterface.h"
#include "ECS/Systems/CreatureObjectActionSystemInterface.h"
#include "ECS/Systems/CreaturePhysiologySystemInterface.h"
#include "ECS/Systems/CreatureSkinSystemInterface.h"
#include "ECS/Systems/FootprintSystemInterface.h"
#include "ECS/Systems/LeashSystemInterface.h"
#include "Input/GameActionMapInterface.h"
#include "Locator.h"
#include "Magic/Spells/SpellCreature.h"
#include "Profiler.h"

namespace openblack::ecs::creature_loop
{
namespace
{
/// A creature service the loop calls. Release builds have no entt assert, so a missing one stops the game with its
/// name rather than a null dereference.
template <typename Slot>
auto& Required(const char* slot)
{
	if (!Slot::has_value())
	{
		std::fprintf(stderr, "ecs::creature_loop: no %s in the locator\n", slot);
		std::abort();
	}
	return Slot::value();
}
} // namespace

void ProcessTurn(Profiler& profiler)
{
	// each creature's home follows its player's temple's pen while the temple stands
	player_creature::FollowTemplePens();
	// in the pen it is drawn smaller
	player_creature::ShrinkInPens();
	{
		auto stage = profiler.BeginScoped(Profiler::Stage::CreaturePhysiologyUpdate);
		Required<Locator::creaturePhysiologySystem>("Locator::creaturePhysiologySystem").ProcessTurn();
	}
	Required<Locator::creatureAnimationSystem>("Locator::creatureAnimationSystem").ProcessTurn();
	Required<Locator::creatureSkinSystem>("Locator::creatureSkinSystem").ProcessTurn();
	{
		auto stage = profiler.BeginScoped(Profiler::Stage::CreatureLeashUpdate);
		Required<Locator::leashSystem>("Locator::leashSystem").ProcessTurn();
	}
	auto& mind = Required<Locator::creatureMindSystem>("Locator::creatureMindSystem");
	{
		auto stage = profiler.BeginScoped(Profiler::Stage::CreatureMindUpdate);
		mind.ProcessTurn();
	}
	{
		auto stage = profiler.BeginScoped(Profiler::Stage::CreaturePlannerUpdate);
		mind.PlanTurn();
	}
	{
		auto stage = profiler.BeginScoped(Profiler::Stage::CreatureLearningUpdate);
		mind.LearnTurn();
	}
	{
		auto stage = profiler.BeginScoped(Profiler::Stage::CreatureLocomotionUpdate);
		Required<Locator::creatureLocomotionSystem>("Locator::creatureLocomotionSystem").ProcessTurn();
	}
	{
		auto stage = profiler.BeginScoped(Profiler::Stage::CreatureObjectActionUpdate);
		Required<Locator::creatureObjectActionSystem>("Locator::creatureObjectActionSystem").ProcessTurn();
	}
	{
		auto stage = profiler.BeginScoped(Profiler::Stage::CreatureCombatUpdate);
		Required<Locator::creatureFightSystem>("Locator::creatureFightSystem").ProcessTurn();
	}
	// the miracles the creatures have taken on ease in, hold and wear off
	magic::spell_creature::ProcessTurn();
}

void UpdateFrame(float turnFraction, uint32_t frameGameMs, Profiler& profiler)
{
	auto stage = profiler.BeginScoped(Profiler::Stage::CreatureFrame);
	const auto gameTime = std::chrono::duration<float, std::milli>(static_cast<float>(frameGameMs));
	Required<Locator::creatureFightSystem>("Locator::creatureFightSystem")
	    .Update(turnFraction, static_cast<float>(frameGameMs));
	Required<Locator::creatureLocomotionSystem>("Locator::creatureLocomotionSystem").Update(turnFraction);
	Required<Locator::creaturePhysiologySystem>("Locator::creaturePhysiologySystem")
	    .Update(static_cast<float>(frameGameMs) * 0.001f);
	// what the creatures act on sets the animations they play, which the animation then poses
	auto& objectActions = Required<Locator::creatureObjectActionSystem>("Locator::creatureObjectActionSystem");
	objectActions.UpdateDraw(turnFraction);
	Required<Locator::creatureAnimationSystem>("Locator::creatureAnimationSystem").Update(gameTime);
	// what they hold rides in the hand where the body was just posed
	objectActions.UpdateHeldDraw();
	Required<Locator::creatureHairSystem>("Locator::creatureHairSystem").Update(gameTime);
	Required<Locator::creatureAudioSystem>("Locator::creatureAudioSystem").Update(gameTime);
	Required<Locator::footprintSystem>("Locator::footprintSystem").Update(gameTime);
	Required<Locator::creatureSkinSystem>("Locator::creatureSkinSystem").Update();
}

void UpdateLeash(float frameGameSeconds, Profiler& profiler)
{
	auto stage = profiler.BeginScoped(Profiler::Stage::CreatureLeashFrame);
	Required<Locator::leashSystem>("Locator::leashSystem").Update(frameGameSeconds);
}

void OnLoadMap()
{
	Required<Locator::footprintSystem>("Locator::footprintSystem").Reset();
}

void ProcessLeashKeys(const input::GameActionInterface& actions)
{
	const auto key = creature_leash::PressedKey(
	    [&actions](input::BindableActionMap action) { return actions.Get(action) && actions.GetChanged(action); });
	if (!key.has_value() || !Locator::leashSystem::has_value())
	{
		return;
	}
	auto& leash = Locator::leashSystem::value();
	const auto player = openblack::creature::LocalPlayer();
	const auto led = leash.PlayersCreature(player);
	if (!led.has_value() || !leash.IsLeashable(*led))
	{
		return;
	}
	leash.PressKey(player, *key);
}

void ReleaseLeashHeldInHand(PlayerNames player)
{
	if (!Locator::leashSystem::has_value())
	{
		return;
	}
	auto& leash = Locator::leashSystem::value();
	const auto creature = leash.PlayersCreature(player);
	if (!creature.has_value() || !leash.IsLeashed(*creature) || leash.TiedTo(*creature).has_value())
	{
		return;
	}
	(void)leash.TakeOffHeldLeash(player);
}
} // namespace openblack::ecs::creature_loop
