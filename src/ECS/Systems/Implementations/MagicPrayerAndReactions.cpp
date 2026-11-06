/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The prayer power that pays for the seeds, the cast state of the hand, and how the world reacts to a miracle.

#define LOCATOR_IMPLEMENTATIONS

#include <algorithm>
#include <chrono>
#include <optional>

#include <glm/gtx/euler_angles.hpp>

#include "3D/MapCoords.h"
#include "Creature/PerceivedDesires.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/Player.h"
#include "ECS/Components/PrayerPower.h"
#include "ECS/Components/Spell.h"
#include "ECS/Components/SpellSeed.h"
#include "ECS/Components/Transform.h"
#include "ECS/CreatureSight.h"
#include "ECS/Map.h"
#include "ECS/Registry.h"
#include "ECS/Statistics.h"
#include "ECS/Systems/ForestSystemInterface.h"
#include "ECS/Systems/ReactionSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "ECS/Systems/WorshipSiteSystemInterface.h"
#include "ECS/ThingMapPlace.h"
#include "ECS/WorldObjects.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/MagicTables.h"
#include "Magic/SpellSeedRules.h"
#include "MagicSystem.h"
#include "Particles/ParticleMiracleMaths.h"
#include "Sound/SoundRequestInterface.h"

using namespace openblack;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;

namespace
{
constexpr float k_TurnSeconds = std::chrono::duration<float>(TimeSystemInterface::k_TurnDuration).count();
constexpr auto k_TurnMilliseconds = static_cast<uint32_t>(TimeSystemInterface::k_TurnDuration.count());

ecs::Registry& EntityRegistry()
{
	return Locator::entitiesRegistry::value();
}
} // namespace

PrayerPower* MagicSystem::PrayerOf(PlayerNames player) const
{
	auto& registry = EntityRegistry();
	PrayerPower* found = nullptr;
	registry.Each<const Player, PrayerPower>([&](entt::entity, const Player& component, PrayerPower& prayer) {
		if (component.name == player)
		{
			found = &prayer;
		}
	});
	return found;
}

float MagicSystem::DrawForSeed(const SpellSeed& seed, float amount)
{
	if (seed.worshipSite != entt::null && EntityRegistry().Valid(seed.worshipSite) && Locator::worshipSiteSystem::has_value())
	{
		return Locator::worshipSiteSystem::value().UseCreateChants(seed.worshipSite, amount);
	}
	auto* store = PrayerOf(seed.player);
	return store != nullptr ? magic::ChargeSeed(*store, amount) : 0.0f;
}

void MagicSystem::ReturnFromSeed(const SpellSeed& seed, float amount)
{
	if (seed.worshipSite != entt::null && EntityRegistry().Valid(seed.worshipSite) && Locator::worshipSiteSystem::has_value())
	{
		if (amount > 0.0f)
		{
			Locator::worshipSiteSystem::value().ReturnChants(seed.worshipSite, amount);
		}
		return;
	}
	if (auto* store = PrayerOf(seed.player))
	{
		magic::ReturnPrayer(*store, amount);
	}
}

float WorshipSiteSpellCaster::MaintainSpell(float amount)
{
	return Locator::worshipSiteSystem::has_value() ? Locator::worshipSiteSystem::value().MaintainSpell(_site, amount) : 0.0f;
}

void WorshipSiteSpellCaster::OnChantsSpent(float chants, magic::ChantCharge charge)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!Locator::worshipSiteSystem::has_value() || _spell == entt::null || !registry.Valid(_spell) ||
	    !registry.AllOf<Spell>(_spell))
	{
		return;
	}
	auto& spell = registry.Get<Spell>(_spell);
	// The motes fly from the icon that made the seed; without it there are none
	const auto icon = spell.caster.icon;
	if (icon == entt::null || !registry.Valid(icon) || !registry.AllOf<Transform>(icon))
	{
		return;
	}
	const bool upkeep = charge == magic::ChantCharge::PerTurn;
	float& spent = upkeep ? spell.upkeepMotes : spell.eventMotes;
	spent = chants + spent;
	if (!(magic::k_ChantsPerMote < spent))
	{
		return;
	}
	spent = 0.0f;
	// Both ends stand at their places on the map, as high above the land as they are
	const auto onMap = [](glm::vec3 point) {
		if (!Locator::terrainSystem::has_value())
		{
			return point;
		}
		const auto& land = Locator::terrainSystem::value();
		return map_coords::ToWorld(land, map_coords::HeldPlace(land, point));
	};
	const auto to = upkeep ? spell.castPosition : spell.position;
	Locator::worshipSiteSystem::value().AddMote(_site, onMap(registry.Get<const Transform>(icon).position), onMap(to));
}

MagicSystemInterface::HandCastState MagicSystem::GetHandCastState() const
{
	HandCastState state {.state = _input.state,
	                     .holding = _actionDown,
	                     .velocity = _handVelocity,
	                     .throwSpeed = particles::maths::HandThrow(_handVelocity).speed,
	                     .pour = magic::PourPoseAt(_pour, 1.0f),
	                     .pointValid = HandPointValid()};
	if (Held().has_value() && EntityRegistry().Valid(*Held()) && Locator::infoConstants::has_value())
	{
		const auto& seed = EntityRegistry().Get<const SpellSeed>(*Held());
		state.ready = seed.ready;
		const float delay = Info().spellSystem.delayBeforeSeedActive;
		state.readyIn = seed.ready || magic::SeedReadyAfter(seed.turnsInHand, k_TurnMilliseconds, delay)
		                    ? 0.0f
		                    : std::max(delay - magic::SeedSecondsInHand(seed.turnsInHand, k_TurnMilliseconds), 0.0f);
		state.origin = seed.origin;
		state.powerUp = seed.powerUp;
		state.chantStore = seed.chantStore;
		state.storedChants = seed.storedChants;
	}
	if (_circle.has_value())
	{
		state.circleCentre = _circle->centre;
		state.circleRadius = _circle->radius;
		state.circleSecondsLeft =
		    std::max(static_cast<float>(magic::k_CircleTurns) - static_cast<float>(_turn - _circle->turn), 0.0f) *
		    k_TurnSeconds;
	}
	return state;
}

void MagicSystem::Empathise(MagicType type, PlayerNames player, glm::vec3 point)
{
	auto* mind = ecs::creature_sight::MindSeeing(player, point);
	if (mind == nullptr)
	{
		return;
	}
	const auto& effect = magic::GetMagicEffectInfo(Info(), type);
	for (const auto desire : effect.perceivedPlayerDesire)
	{
		creature_perceived_desires::Increase(mind->perceivedDesires, static_cast<size_t>(desire), 1.0f);
	}
	creature_perceived_desires::IncreaseTown(mind->perceivedDesires, static_cast<size_t>(effect.townDesireBeingHelped), 1.0f);
}

void MagicSystem::RecordCast(MagicType type, PlayerNames player, glm::vec3 point)
{
	EntityRegistry().Each<components::Player>([&](entt::entity, components::Player& record) {
		if (record.name == player)
		{
			record.lastCast = components::Player::Cast {.position = point, .type = type, .turn = _turn};
		}
	});
	player_statistics::CountCast(EntityRegistry(), player, type);
}

void MagicSystem::ReactToSpell(Spell& spell, bool onCast)
{
	const auto& effect = magic::GetMagicEffectInfo(Info(), spell.magicType);
	if (effect.reactionType == Reaction::None || !Locator::reactionSystem::has_value())
	{
		return;
	}
	// A reaction that has ended lets the miracle's next event make another
	if (spell.reaction != 0 && !Locator::reactionSystem::value().Find(spell.reaction).has_value())
	{
		spell.reaction = 0;
	}
	if (spell.reaction != 0)
	{
		return;
	}
	const auto found = std::ranges::find_if(
	    _spells, [&spell](entt::entity e) { return EntityRegistry().Valid(e) && EntityRegistry().TryGet<Spell>(e) == &spell; });
	if (found == _spells.end())
	{
		return;
	}
	const auto entity = *found;
	spell.reaction = Locator::reactionSystem::value().Create({
	    .initiator = entity,
	    .type = effect.reactionType,
	    .player = spell.caster.player,
	    .position = spell.position,
	    .impressiveValue = effect.impressiveValue,
	    .power = 1.0f,
	    .strength = magic::spells::StrengthOf(*this, spell),
	    .magicType = spell.magicType,
	    .casterCreature = spell.caster.kind == SpellCaster::Kind::Creature ? spell.caster.entity : entt::null,
	    .onCast = onCast,
	    .balance = ecs::systems::ReactionSystemInterface::Balance::Miracles,
	});
}
