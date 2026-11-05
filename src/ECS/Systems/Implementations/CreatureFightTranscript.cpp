/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>

#include <array>
#include <optional>
#include <string_view>
#include <vector>

#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/vec_swizzle.hpp>

#include "CreatureFightSystem.h"
#include "CreatureFightSystemDetail.h"
#include "ECS/Archetypes/CreatureArchetype.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureFight.h"
#include "ECS/Components/CreatureNeeds.h"
#include "ECS/CreatureHome.h"
#include "ECS/CreaturePlacement.h"
#include "ECS/CreatureScars.h"
#include "ECS/Map.h"
#include "ECS/PosedModel.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CreatureAnimationSystemInterface.h"
#include "ECS/Systems/FightTranscriptNote.h"
#include "ECS/ThingCircles.h"
#include "ECS/ThingMapPlace.h"
#include "ECS/ThingModel.h"
#include "ECS/WorldObjects.h"
#include "Locator.h"
#include "Sound/SoundRequestInterface.h"

using namespace openblack;
using namespace openblack::ecs::systems;
using namespace openblack::ecs::components;
namespace fight = openblack::creature_fight;
using namespace openblack::ecs::systems::fight_detail;

namespace openblack::ecs::systems::fight_detail
{
std::string_view Name(CreatureFighting::Stage stage)
{
	static constexpr std::array<std::string_view, 10> k_Names {"face_off", "taunt", "point",     "approach", "ready",
	                                                           "square",   "duel",  "celebrate", "respond",  "stop"};
	const auto index = static_cast<size_t>(stage);
	return index < k_Names.size() ? k_Names.at(index) : std::string_view("unknown");
}

std::string_view Name(CreatureKnockedOut::Stage stage)
{
	static constexpr std::array<std::string_view, 7> k_Names {
	    "lying", "fading_out", "fading_in", "waiting", "waiting_for_spells", "resting", "getting_up"};
	const auto index = static_cast<size_t>(stage);
	return index < k_Names.size() ? k_Names.at(index) : std::string_view("unknown");
}

fight_transcript::Fields Levels(const ecs::Registry& registry, entt::entity creature)
{
	fight_transcript::Fields fields {{"creature", fight_transcript::Id(creature)}};
	if (const auto* fighting = registry.TryGet<const CreatureFighting>(creature))
	{
		fields.emplace_back("health", fighting->fighter.health);
		fields.emplace_back("stamina", fighting->fighter.stamina);
		fields.emplace_back("control", fight::Name(fighting->fighter.control));
		fields.emplace_back("tendency", fighting->fighter.tendency);
		fields.emplace_back("queue", static_cast<uint32_t>(fighting->fighter.queue.Size()));
	}
	if (const auto* needs = registry.TryGet<const CreatureNeeds>(creature))
	{
		fields.emplace_back("life", needs->needs.life);
		fields.emplace_back("exhaustion", needs->needs.exhaustion);
		fields.emplace_back("energy", needs->needs.energy);
	}
	if (const auto* body = registry.TryGet<const Creature>(creature))
	{
		fields.emplace_back("strength", body->strength);
		fields.emplace_back("size", ShownSize(*body));
		fields.emplace_back("alignment", body->alignment);
	}
	return fields;
}

fight_transcript::Fields LevelChange(entt::entity creature, std::string_view level, float before, float after,
                                     std::string_view why)
{
	return {{"creature", fight_transcript::Id(creature)},
	        {"level", level},
	        {"before", before},
	        {"after", after},
	        {"change", after - before},
	        {"why", why}};
}
} // namespace openblack::ecs::systems::fight_detail

void CreatureFightSystem::NoteActions()
{
	if (!fight_transcript::Recording())
	{
		return;
	}
	std::vector<entt::entity> fighters;
	Locator::entitiesRegistry::value().Each<const CreatureFighting>(
	    [&fighters](entt::entity entity, const CreatureFighting& /*fighting*/) { fighters.push_back(entity); });
	for (const auto entity : fighters)
	{
		NoteAction(entity);
	}
}

void CreatureFightSystem::NoteAction(entt::entity entity)
{
	if (!fight_transcript::Recording())
	{
		return;
	}
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* found = registry.Valid(entity) ? registry.TryGet<const CreatureFighting>(entity) : nullptr;
	if (found == nullptr)
	{
		return;
	}
	const auto& fighting = *found;
	const auto& fighter = fighting.fighter;
	const auto seen = _seen.find(entity);
	if (seen != _seen.end() && seen->second.state == fighter.state && seen->second.animation == fighter.animation)
	{
		return;
	}
	_seen[entity] = {.state = fighter.state, .animation = fighter.animation};
	std::optional<uint64_t> cause;
	if (const auto known = _actionCause.find(entity); known != _actionCause.end())
	{
		cause = known->second;
		_actionCause.erase(known);
	}
	Locator::fightTranscript::value().Note(
	    entity, "action",
	    {{"creature", fight_transcript::Id(entity)},
	     {"state", fight::Name(fighter.state)},
	     {"animation", static_cast<uint64_t>(fighter.animation)},
	     {"fight_action", fight::Name(fight::FightActionOf(fighter.animation))},
	     {"speed", fighter.speed},
	     {"special", fighter.special},
	     {"mirrored", fighter.mirrored},
	     {"spell", fighter.spell.has_value() ? static_cast<int64_t>(*fighter.spell) : int64_t {-1}},
	     {"queue", static_cast<uint32_t>(fighter.queue.Size())},
	     {"stage", Name(fighting.stage)}},
	    cause);
}

void CreatureFightSystem::NoteStage(entt::entity creature, CreatureFighting::Stage from, CreatureFighting::Stage to)
{
	if (from == to)
	{
		return;
	}
	fight_transcript::Note(creature, "stage", [&] {
		const auto* fighting = Locator::entitiesRegistry::value().TryGet<const CreatureFighting>(creature);
		return fight_transcript::Fields {{"creature", fight_transcript::Id(creature)},
		                                 {"from", Name(from)},
		                                 {"to", Name(to)},
		                                 {"stage_seconds", fighting != nullptr ? fighting->stageSeconds : 0.0f}};
	});
}

void CreatureFightSystem::NoteFightOver(entt::entity creature, std::string_view reason)
{
	if (!fight_transcript::Recording())
	{
		return;
	}
	auto& transcript = Locator::fightTranscript::value();
	const auto fight = transcript.GetRecorder().FightOf(entt::to_integral(creature));
	const auto* found = fight.has_value() ? transcript.GetRecorder().Find(*fight) : nullptr;
	// Only the first reason counts, as each side of a fight ends it
	if (found == nullptr || found->ended.has_value())
	{
		return;
	}
	transcript.Note(creature, "fight_over", {{"creature", fight_transcript::Id(creature)}, {"reason", reason}}, std::nullopt);
	transcript.End(creature);
}
