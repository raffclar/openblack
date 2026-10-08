/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "SpellCreature.h"

#include <algorithm>
#include <array>
#include <optional>
#include <utility>
#include <vector>

#include <glm/geometric.hpp>

#include "Audio/Audio.h"
#include "Audio/Game/Banks.h"
#include "Creature/CreatureSpells.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/CreatureSpells.h"
#include "ECS/Components/Spell.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CreatureLocomotionSystemInterface.h"
#include "ECS/Systems/LeashSystemInterface.h"
#include "GameClock.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/Core/Spell.h"
#include "Magic/Core/SpellEvent.h"
#include "Magic/MagicTables.h"
#include "SpellClasses.h"

using namespace openblack;
using namespace openblack::magic;
using namespace openblack::ecs::components;

namespace
{
/// A spell's duration that never runs out
constexpr float k_NoTimeLimit = -1.0f;

[[nodiscard]] float TurnsPerSecond()
{
	return 1.0f / game_clock::k_TurnSeconds;
}

/// Whether the entity is a spell still there to close down
[[nodiscard]] bool IsLiveSpell(entt::entity spell)
{
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	return spell != entt::null && registry.Valid(spell) && registry.AllOf<Spell>(spell);
}

/// The sound of the creature taking a spell, from the bank all creatures share
void PlaySound(entt::entity creature, int32_t soundAction)
{
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	const auto* transform = registry.TryGet<const Transform>(creature);
	if (transform == nullptr || !Locator::audio::has_value() || !Locator::audioState::has_value())
	{
		return;
	}
	const auto listener = audio::ListenerPoint();
	const float distance = listener ? glm::distance(*listener, transform->position) : 0.0f;
	audio::PlayAnimationEffect(audio::Owner::Thing(creature), distance, audio::AnimKey {0, 0, 0, 0, soundAction},
	                           audio::AnimAction::Play, audio::Bank(audio::SfxBank::Creature), true, 0.0f, 0.0f);
}

/// One event of a creature's spell, applied to the creature and passed on to the systems it concerns
void ApplyEvent(entt::entity entity, const creature_spells::TurnEvent& event)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto& lookup = std::as_const(registry);
	if (!lookup.AllOf<Creature, CreatureSpells>(entity))
	{
		return;
	}
	auto& creature = registry.Get<Creature>(entity);
	auto& component = registry.Get<CreatureSpells>(entity);
	auto* mind = lookup.AllOf<CreatureMindState>(entity) ? &registry.Get<CreatureMindState>(entity) : nullptr;

	creature_spells::BodyValues body {
	    .size = creature.size, .strength = creature.strength, .fatness = creature.fatness, .alignment = creature.alignment};
	creature_spells::SpellLook look {.freeze = component.freeze,
	                                 .fizz = component.fizz,
	                                 .invisible = component.invisible,
	                                 .pausedMind = component.pausedMind};
	std::optional<creature_spells::DesireView> view;
	if (mind != nullptr)
	{
		view = creature_spells::DesireView {.desires = mind->desires.has_value() ? &*mind->desires : nullptr,
		                                    .paused = mind->paused};
	}
	const auto effects =
	    creature_spells::Apply(event, component.spells[event.spell], body, look, view.has_value() ? &*view : nullptr);

	creature.size = body.size;
	creature.strength = body.strength;
	creature.fatness = body.fatness;
	creature.alignment = body.alignment;
	component.freeze = look.freeze;
	component.fizz = look.fizz;
	component.invisible = look.invisible;
	component.pausedMind = look.pausedMind;
	if (mind != nullptr && effects.pauseMind.has_value())
	{
		mind->paused = *effects.pauseMind;
	}
	if (effects.playbackScale.has_value() && lookup.AllOf<CreatureAnimation>(entity))
	{
		registry.Get<CreatureAnimation>(entity).playbackScale = *effects.playbackScale;
	}
	if (effects.stopLocomotion && Locator::creatureLocomotionSystem::has_value())
	{
		Locator::creatureLocomotionSystem::value().Stop(entity);
	}
	if (effects.leashWorks.has_value() && Locator::leashSystem::has_value())
	{
		Locator::leashSystem::value().SetWorks(entity, *effects.leashWorks);
	}
	if (effects.sound != 0)
	{
		PlaySound(entity, effects.sound);
	}
}

/// The base cast at the object's place; then, cast on a creature, the creature takes the spell on
int CreatureInitWithObject(entt::entity spell, entt::entity object, SpellCastData* castData, const psys::ProcessInfo& info)
{
	const int result = base::InitWithObject(spell, object, castData, info);
	if (result == 1)
	{
		spell_creature::Receive(object, spell);
	}
	return result;
}

/// The base close-down; then no creature's spell points at it any more, so it is not closed down again
void CreatureCloseDown(entt::entity spell)
{
	base::CloseDown(spell);
	auto& registry = Locator::entitiesRegistry::value();
	std::vector<entt::entity> holders;
	std::as_const(registry).Each<const CreatureSpells>([&](entt::entity entity, const CreatureSpells& component) {
		const auto& spells = component.spells;
		const bool inSlot =
		    std::ranges::any_of(spells.slots, [spell](const creature_spells::Slot& slot) { return slot.miracle == spell; });
		const bool waiting = std::ranges::any_of(
		    spells.waiting, [spell](const creature_spells::Waiting& queued) { return queued.miracle == spell; });
		if (inSlot || waiting)
		{
			holders.push_back(entity);
		}
	});
	for (const auto entity : holders)
	{
		auto& spells = registry.Get<CreatureSpells>(entity).spells;
		for (auto& slot : spells.slots)
		{
			if (slot.miracle == spell)
			{
				slot.miracle = entt::null;
			}
		}
		for (auto& queued : spells.waiting)
		{
			if (queued.miracle == spell)
			{
				queued.miracle = entt::null;
			}
		}
	}
}
} // namespace

void spell_creature::Receive(entt::entity creature, entt::entity spell)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto& lookup = std::as_const(registry);
	if (!registry.Valid(creature) || !lookup.AllOf<Creature>(creature) || !IsLiveSpell(spell))
	{
		return;
	}
	const auto which = creature_spells::SpellOf(lookup.Get<const Spell>(spell).magicType);
	if (!which.has_value())
	{
		return;
	}
	auto& component = lookup.AllOf<CreatureSpells>(creature) ? registry.Get<CreatureSpells>(creature)
	                                                         : registry.AssignState<CreatureSpells>(creature);
	auto& miracle = registry.Get<Spell>(spell);
	// The creature holds the spell for the miracle's time, made longer by the caster's tribal power; the miracle itself
	// then runs until the creature lets it go
	const float seconds = miracle.duration > 0.0f ? miracle.duration * GetTribalPower(spell) : miracle.duration;
	const auto result =
	    creature_spells::Receive(component.spells, *which, creature_spells::TurnsOf(seconds, TurnsPerSecond()), spell);
	miracle.duration = k_NoTimeLimit;
	if (result.replaced != spell && IsLiveSpell(result.replaced))
	{
		magic::CloseDown(result.replaced);
	}
}

void spell_creature::ProcessTurn()
{
	if (!Locator::entitiesRegistry::has_value())
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	const auto& lookup = std::as_const(registry);

	// Each spell's start and finish times, from its table
	std::array<creature_spells::Timing, creature_spells::k_SpellCount> timings {};
	if (Locator::infoConstants::has_value())
	{
		const auto& info = Locator::infoConstants::value();
		for (size_t i = 0; i < timings.size(); ++i)
		{
			const auto type = static_cast<MagicType>(static_cast<size_t>(MagicType::CreatureSpellFreeze) + i);
			if (const auto* row = GetMagicInfoAs<GMagicCreatureSpellInfo>(info, type))
			{
				timings.at(i) = {.startSeconds = row->startTransitionDuration, .finishSeconds = row->finishTransitionDuration};
			}
		}
	}

	std::vector<entt::entity> creatures;
	lookup.Each<const CreatureSpells>([&](entt::entity entity, const CreatureSpells&) { creatures.push_back(entity); });
	std::ranges::sort(creatures, {}, [](entt::entity entity) { return entt::to_integral(entity); });
	for (const auto entity : creatures)
	{
		if (!lookup.AllOf<CreatureSpells>(entity))
		{
			continue;
		}
		const auto turn = creature_spells::Step(registry.Get<CreatureSpells>(entity).spells, timings, TurnsPerSecond());
		for (const auto& event : turn.events)
		{
			ApplyEvent(entity, event);
		}
		for (const auto miracle : turn.ended)
		{
			if (IsLiveSpell(miracle))
			{
				magic::CloseDown(miracle);
			}
		}
	}
}

void openblack::magic::RegisterCreatureSpell()
{
	// The plain spell's operations but for the cast on an object and the close-down, so that a creature miracle cast at
	// a place does as the plain spell does
	const SpellOps ops {.initWithPos = base::InitWithPos,
	                    .initWithObject = CreatureInitWithObject,
	                    .process = base::Process,
	                    .spellEvent = spell_event::SpellEvent,
	                    .costToMaintain = base::CalculateCostToMaintain,
	                    .closeDown = CreatureCloseDown,
	                    .toBeDeleted = nullptr,
	                    .hasEnoughChantsForRecast = base::HasEnoughChantsAndLifeForRecast,
	                    .particleType = base::GetParticleType};
	RegisterOps(SpellClass::Creature, ops);
}
