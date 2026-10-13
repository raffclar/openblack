/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include <algorithm>
#include <array>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include <MindFile.h>
#include <spdlog/spdlog.h>

#include "3D/CreatureBody.h"
#include "Common/GameRandom.h"
#include "Creature/CreatureMindFileBody.h"
#include "Creature/CreatureMindTables.h"
#include "Creature/CreatureRig.h"
#include "Creature/CreatureSpells.h"
#include "Creature/CreatureSwap.h"
#include "Creature/CreatureWatching.h"
#include "CreatureMindSystem.h"
#include "ECS/Archetypes/CreatureArchetype.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureLeash.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/CreatureSpells.h"
#include "ECS/Components/CreatureSwap.h"
#include "ECS/Components/Transform.h"
#include "ECS/CreatureBodyFile.h"
#include "ECS/Registry.h"
#include "ECS/Systems/LeashSystemInterface.h"
#include "ECS/Systems/ParticleSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::systems;
using namespace openblack::ecs::components;

namespace
{
/// The action a creature plays as its mind leaves for the other body
constexpr size_t k_SwapAnimation = 109;
/// The beam between the two creatures lasts this many turns
constexpr int k_SwapBeamTurns = 70;
/// The action of the game's table a creature carries out while it swaps
constexpr std::string_view k_SwapAction = "SwapMindWithOtherCreature";

/// A body's size and alignment as they were before any spell on it changed them
creature_swap::Shape OwnShape(const ecs::Registry& registry, entt::entity creature)
{
	const auto& body = registry.Get<const Creature>(creature);
	const creature_spells::SavedBody now {.size = body.size, .strength = body.strength, .alignment = body.alignment};
	const auto* spells = registry.TryGet<const CreatureSpells>(creature);
	const auto saved = spells != nullptr ? creature_spells::ValuesToSave(spells->spells, now) : now;
	return {.size = saved.size, .alignment = saved.alignment};
}

/// Whether no spell is on the creature any more
bool SpellFree(const ecs::Registry& registry, entt::entity creature)
{
	const auto* spells = registry.Valid(creature) ? registry.TryGet<const CreatureSpells>(creature) : nullptr;
	return spells == nullptr || std::ranges::all_of(spells->spells.slots,
	                                                [](const auto& slot) { return slot.phase == creature_spells::Phase::Off; });
}

/// The player whose colour the creature's beam takes: its own, or the local player's when it has none
/// A point of a creature's body: one of its bones, drawn at random, where it was last posed; its place when it has no pose
glm::vec3 RandomBodyPoint(const ecs::Registry& registry, entt::entity creature)
{
	const auto& transform = registry.Get<const Transform>(creature);
	const auto* animation = registry.TryGet<const CreatureAnimation>(creature);
	const auto bones = animation != nullptr ? static_cast<uint32_t>(animation->boneMatrices.size()) : 0u;
	const auto bone = Locator::gameRandom::value().GameRand(bones);
	if (animation == nullptr || bone >= bones)
	{
		return transform.position;
	}
	const auto placement = creature::PlacementMatrix(transform.position, transform.rotation, transform.scale);
	return glm::vec3(creature::PosedBone(bone, animation->boneMatrices, placement)[3]);
}

/// Every third game turn of the swap, while the other is still a creature and its beam of light still runs, a beam runs
/// from a random point of this body to a random point of the other's
void FireSwapBeam(const ecs::Registry& registry, entt::entity creature, const CreatureSwap& swap)
{
	if (!Locator::particleSystem::has_value() || !Locator::time::has_value() || !Locator::gameRandom::has_value())
	{
		return;
	}
	auto& particles = Locator::particleSystem::value();
	if (!creature_swap::IsBeamTurn(Locator::time::value().GetTurn()) || !registry.Valid(swap.other) ||
	    !registry.AllOf<Creature, Transform>(swap.other) || swap.beam == ParticleSystemInterface::k_NoEffect ||
	    !particles.IsRunning(swap.beam))
	{
		return;
	}
	const auto from = RandomBodyPoint(registry, creature);
	const auto to = RandomBodyPoint(registry, swap.other);
	particles.AddPlasma(swap.beam,
	                    creature_swap::Beam(from, to, Locator::gameRandom::value().GameRand(creature_swap::k_BeamAlphaDraws)));
}

int BeamPlayer(PlayerNames owner)
{
	if (owner != PlayerNames::NEUTRAL)
	{
		return static_cast<int>(owner);
	}
	return static_cast<int>(Locator::playerSystem::has_value() ? Locator::playerSystem::value().GetLocalPlayer()
	                                                           : PlayerNames::PLAYER_ONE);
}
} // namespace

bool CreatureMindSystem::SwapMinds(entt::entity from, entt::entity to)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto ready = [&registry](entt::entity creature) {
		return creature != entt::null && registry.Valid(creature) &&
		       registry.AllOf<Creature, CreatureMindState, Transform>(creature);
	};
	if (from == to || !ready(from) || !ready(to))
	{
		return false;
	}
	for (const auto creature : {from, to})
	{
		SetUpMind(creature, registry.Get<const Creature>(creature), registry.Get<CreatureMindState>(creature));
	}

	// The leashes come off both first
	if (Locator::leashSystem::has_value())
	{
		auto& leashes = Locator::leashSystem::value();
		for (const auto creature : {from, to})
		{
			if (registry.Get<const Creature>(creature).owner != PlayerNames::NEUTRAL && leashes.IsLeashed(creature))
			{
				leashes.TakeOff(creature);
			}
		}
	}

	// What each body keeps for itself: its name, size and alignment as they are now. Each mind takes the other body's
	// size and alignment, as they were before any spell, to grow into over the swap.
	struct Kept
	{
		std::u16string name;
		float size;
		float alignment;
		float pendingAlignment;
		creature_swap::Shape own;
	};
	const auto keep = [&registry](entt::entity creature) {
		const auto& body = registry.Get<const Creature>(creature);
		return Kept {.name = body.name,
		             .size = body.size,
		             .alignment = body.alignment,
		             .pendingAlignment = body.pendingAlignment,
		             .own = OwnShape(registry, creature)};
	};
	const auto keptFrom = keep(from);
	const auto keptTo = keep(to);
	const auto speciesFrom = registry.Get<const Creature>(from).species;
	const auto speciesTo = registry.Get<const Creature>(to).species;

	auto fileFrom = SaveMind(from);
	auto fileTo = SaveMind(to);
	if (!fileFrom.has_value() || !fileTo.has_value())
	{
		SPDLOG_LOGGER_WARN(spdlog::get("game"), "Creatures {} and {} can't swap minds: one has no mind yet",
		                   entt::to_integral(from), entt::to_integral(to));
		return false;
	}

	// Each takes up the other's mind file at once, mind and body, then has its own name, size and alignment back
	const auto takeUp = [this, &registry](entt::entity creature, creaturemind::MindFileData file, const Kept& kept) {
		auto shared = std::make_shared<const creaturemind::MindFileData>(std::move(file));
		auto& mind = registry.Get<CreatureMindState>(creature);
		mind.pendingFile = shared;
		TakeUpFile(creature, mind);
		if (auto body = creature_mind_body::FromMindFile(*shared); body.has_value())
		{
			body->size = kept.size;
			body->alignment = kept.alignment;
			ecs::creature_body_file::Apply(registry, creature, *body);
		}
		auto& self = registry.Get<Creature>(creature);
		self.name = kept.name;
		self.size = kept.size;
		self.alignment = kept.alignment;
		self.pendingAlignment = kept.pendingAlignment;
	};
	takeUp(from, std::move(*fileTo), keptFrom);
	takeUp(to, std::move(*fileFrom), keptTo);

	registry.AssignOrReplace<CreatureSwap>(from, CreatureSwap {.other = to, .target = keptTo.own});
	registry.AssignOrReplace<CreatureSwap>(to, CreatureSwap {.other = from, .target = keptFrom.own});

	// The mind that came from the first creature is as far towards learning each miracle as it was, for its new species
	if (const auto* tables = GetTables(); tables != nullptr)
	{
		auto& mind = registry.Get<CreatureMindState>(to);
		if (mind.learnt.has_value())
		{
			auto& seen = mind.learnt->knowledge.miraclesSeen;
			const auto multiplierBefore = creature_mind_tables::MiracleMultiplier(creature::InfoRow(speciesFrom));
			const auto multiplierAfter = creature_mind_tables::MiracleMultiplier(creature::InfoRow(speciesTo));
			for (size_t miracle = 1; miracle < std::min(seen.size(), tables->miracles.size()); ++miracle)
			{
				const auto timesToSee = tables->miracles[miracle].timesToSee;
				seen[miracle].count = creature_swap::RescaledSightings(
				    seen[miracle].count, creature_watching::TimesNeeded(timesToSee, multiplierBefore),
				    creature_watching::TimesNeeded(timesToSee, multiplierAfter));
			}
		}
	}

	// Both carry out the swap
	for (const auto& [creature, other] : std::array {std::pair {from, to}, std::pair {to, from}})
	{
		ForcePlan(creature, {.desire = creature_desires::Desire::ObeyPlayer,
		                     .action = k_SwapAction,
		                     .object = other,
		                     .agenda = std::vector {creature_mind::Step {.kind = creature_mind::Step::Kind::SwapMind,
		                                                                 .animation = k_SwapAnimation}}});
	}

	// The owners swap: the player now has the second creature, which they can lead as they did the first
	const auto ownerFrom = registry.Get<const Creature>(from).owner;
	const auto ownerTo = registry.Get<const Creature>(to).owner;
	if (ownerFrom != ownerTo)
	{
		if (Locator::leashSystem::has_value())
		{
			auto& leashes = Locator::leashSystem::value();
			leashes.SetOwner(from, ownerTo);
			leashes.SetOwner(to, ownerFrom);
			leashes.ClaimOnArrival(to);
			leashes.ClaimOnArrival(from);
		}
		else
		{
			registry.Get<Creature>(from).owner = ownerTo;
			registry.Get<Creature>(to).owner = ownerFrom;
		}
		if (Locator::playerSystem::has_value())
		{
			for (const auto creature : {from, to})
			{
				if (registry.Get<const Creature>(creature).owner != PlayerNames::NEUTRAL)
				{
					Locator::playerSystem::value().AddCreature(creature);
				}
			}
		}
	}
	// The leashes a player has taught go with the creature they lead
	auto* leashFrom = registry.TryGet<CreatureLeash>(from);
	auto* leashTo = registry.TryGet<CreatureLeash>(to);
	if (leashFrom != nullptr && leashTo != nullptr)
	{
		std::swap(leashFrom->known, leashTo->known);
	}

	SPDLOG_LOGGER_INFO(spdlog::get("game"), "Creatures {} and {} swapped minds", entt::to_integral(from),
	                   entt::to_integral(to));
	return true;
}

creature_mind::Senses::SwapView CreatureMindSystem::SwapViewOf(entt::entity creature)
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* swap = registry.TryGet<const CreatureSwap>(creature);
	const auto* body = registry.TryGet<const Creature>(creature);
	if (swap == nullptr || body == nullptr)
	{
		return {.spellsGone = SpellFree(registry, creature), .settled = true};
	}
	return {.spellsGone = SpellFree(registry, creature) && SpellFree(registry, swap->other),
	        .settled = creature_swap::IsSettled({.size = body->size, .alignment = body->alignment}, swap->target)};
}

void CreatureMindSystem::StepSwap(entt::entity creature, const creature_mind::Commands& commands)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (commands.finishSpells)
	{
		// Every spell on it is brought to its end
		if (auto* spells = registry.TryGet<CreatureSpells>(creature); spells != nullptr)
		{
			static_cast<void>(creature_spells::TryFinishAll(spells->spells));
		}
	}
	if (commands.startSwapBeam && Locator::particleSystem::has_value())
	{
		const auto& body = registry.Get<const Creature>(creature);
		auto& particles = Locator::particleSystem::value();
		auto* swap = registry.TryGet<CreatureSwap>(creature);
		// A beam of an earlier swap still about it goes first
		if (swap != nullptr && swap->beam != ParticleSystemInterface::k_NoEffect)
		{
			particles.CloseDown(swap->beam);
		}
		const auto beam =
		    particles.StartSpotVisual(SpotVisualType::MagicBeamCreatureSwap, registry.Get<const Transform>(creature).position,
		                              k_SwapBeamTurns, entt::null, 1.0f);
		particles.SetPlayer(beam, BeamPlayer(body.owner));
		if (swap != nullptr)
		{
			swap->beam = beam;
		}
	}
	if (commands.easeSwap)
	{
		const auto* swap = registry.TryGet<const CreatureSwap>(creature);
		auto& body = registry.Get<Creature>(creature);
		if (swap == nullptr)
		{
			return;
		}
		const auto shape = creature_swap::Ease({.size = body.size, .alignment = body.alignment}, swap->target);
		body.size = shape.size;
		body.alignment = shape.alignment;
		if (auto* transform = registry.TryGet<Transform>(creature))
		{
			transform->scale = glm::vec3(archetypes::CreatureArchetype::DrawnScale(body.species, ShownSize(body)));
		}
		FireSwapBeam(registry, creature, *swap);
	}
}
