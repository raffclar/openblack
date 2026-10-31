/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "ComputerGameHandCasts.h"

#include <cstring>

#include <algorithm>
#include <optional>
#include <string>

#include <LNDFile.h>
#include <spdlog/spdlog.h>

#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "ComputerMindCommon.h"
#include "Creature/CreatureGesture.h"
#include "ECS/Components/CreatureSpells.h"
#include "ECS/Components/SpellIcon.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/Registry.h"
#include "ECS/Systems/GestureSystemInterface.h"
#include "ECS/Systems/MagicSystemInterface.h"
#include "ECS/Systems/SpellIconSystemInterface.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/MagicTables.h"
#include "Magic/SpellRules.h"
#include "Sound/GameSoundEffects.h"
#include "Sound/SoundId.h"

using namespace openblack;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;
using namespace openblack::ecs::systems::computer_world;

namespace
{

/// Writes the miracle that the god cast to the log, with no spaces in its name. The check tools count these lines for
/// each god.
void LogCast(PlayerNames player, const GMagicEffectInfo& effect)
{
	const auto logger = spdlog::get("game");
	if (logger == nullptr)
	{
		return;
	}
	std::string name(effect.debugString.data(), strnlen(effect.debugString.data(), effect.debugString.size()));
	std::ranges::replace(name, ' ', '_');
	SPDLOG_LOGGER_INFO(logger, "Computer god {} casts {}", static_cast<int>(player), name);
}

} // namespace

GameHandCasts::GameHandCasts(PlayerNames player, ComputerPlayer& state)
    : _player(player)
    , _state(state)
{
}

bool GameHandCasts::IconHasSite(entt::entity icon) const
{
	auto& registry = Entities();
	const auto* component = registry.Valid(icon) ? registry.TryGet<const WorshipIcon>(icon) : nullptr;
	return component != nullptr && registry.Valid(component->site) && registry.AllOf<WorshipSite>(component->site);
}

bool GameHandCasts::IconCharging(entt::entity icon) const
{
	auto& registry = Entities();
	const auto* component = registry.Valid(icon) ? registry.TryGet<const WorshipIcon>(icon) : nullptr;
	return component != nullptr && component->charging;
}

bool GameHandCasts::StartChant(entt::entity icon, MagicType /*type*/)
{
	return Locator::spellIconSystem::has_value() && Locator::spellIconSystem::value().StartChargeForGod(icon, _player);
}

float GameHandCasts::ChantNeeded(entt::entity icon) const
{
	return Locator::spellIconSystem::has_value() ? Locator::spellIconSystem::value().ChantNeeded(icon) : 0.0f;
}

bool GameHandCasts::Angry(MagicType type) const
{
	return Locator::infoConstants::has_value() &&
	       magic::GetMagicEffectInfo(Locator::infoConstants::value(), type).perceivedPlayerDesire.at(0) ==
	           CreatureDesires::Anger;
}

bool GameHandCasts::Cast(MagicType type, entt::entity target, entt::entity icon, glm::vec3 hand)
{
	const auto targetCoords = CoordsOfThing(target);
	if (!targetCoords.has_value())
	{
		return false;
	}
	return CastFrom(type, target, *targetCoords, icon, hand, 1.0f);
}

bool GameHandCasts::CastAt(MagicType type, glm::vec3 place, entt::entity icon, glm::vec3 hand, float magnitude)
{
	if (!Locator::terrainSystem::has_value())
	{
		return false;
	}
	// The miracle goes on the land at the place
	map_coords::MapCoords coords {.x = map_coords::ToFixed(place.x), .z = map_coords::ToFixed(place.z), .altitude = 0.0f};
	coords.altitude = Locator::terrainSystem::value().GetAltitude(coords.x, coords.z);
	return CastFrom(type, entt::null, coords, icon, hand, magnitude);
}

bool GameHandCasts::UnderSpell(entt::entity creature) const
{
	auto& registry = Entities();
	if (creature == entt::null || !registry.Valid(creature))
	{
		return false;
	}
	const auto* spells = registry.TryGet<const CreatureSpells>(creature);
	return spells != nullptr && std::ranges::any_of(spells->spells.slots, [](const creature_spells::Slot& slot) {
		       return slot.phase != creature_spells::Phase::Off;
	       });
}

bool GameHandCasts::CastFrom(MagicType type, entt::entity target, const map_coords::MapCoords& targetCoords, entt::entity icon,
                             glm::vec3 hand, float magnitude)
{
	if (!Locator::infoConstants::has_value() || !Locator::magicSystem::has_value())
	{
		return false;
	}
	// The miracle must be charged full
	if (icon != entt::null && ChantNeeded(icon) > 0.0f)
	{
		return false;
	}
	// A food or wood miracle lands only on land
	if (type == MagicType::Food || type == MagicType::Wood)
	{
		const auto* cell = Locator::terrainSystem::value().FindCell(glm::u16vec2(map_coords::Cell(targetCoords)));
		if (cell == nullptr || cell->properties.hasWater != 0)
		{
			return false;
		}
	}
	const auto& info = Locator::infoConstants::value();
	const auto& effect = magic::GetMagicEffectInfo(info, type);
	const map_coords::MapCoords handCoords {
	    .x = map_coords::ToFixed(hand.x), .z = map_coords::ToFixed(hand.z), .altitude = hand.y};
	const auto from = glm::vec3(map_coords::ToMetres(handCoords.x), handCoords.altitude, map_coords::ToMetres(handCoords.z));
	const auto at =
	    glm::vec3(map_coords::ToMetres(targetCoords.x), targetCoords.altitude, map_coords::ToMetres(targetCoords.z));
	const magic::SpellCastData cast {.magnitude = magnitude,
	                                 .chants = effect.initialChants,
	                                 .duration = effect.timerWhenComputerPlayerCasting,
	                                 .maxObjectsToCreate = -1};
	const particles::ProcessInfo process {.handPosition = from,
	                                      .cameraForward = at - from,
	                                      .direction = glm::vec3(0.0f),
	                                      .power = 1.0f,
	                                      .enabled = true,
	                                      .spin = 0.0f};
	const auto seed = magic::FindFirstSpellSeedForMagicType(info, type);
	const bool onObject = target != entt::null && seed.has_value() && magic::GetSpellSeedInfo(info, *seed).castOnObject != 0;
	auto& magicSystem = Locator::magicSystem::value();
	const auto spell = onObject ? magicSystem.CastOnObject(type, _player, target, cast, process)
	                            : magicSystem.CastAtPoint(type, _player, at, cast, process);
	if (spell == entt::null)
	{
		return false;
	}
	if (icon != entt::null && Locator::spellIconSystem::has_value())
	{
		Locator::spellIconSystem::value().TakeCastCharge(icon);
	}
	_state.casts.insert(_state.casts.begin(), spell);
	LogCast(_player, effect);
	return true;
}

float GameHandCasts::CastSeconds(MagicType type) const
{
	return Locator::infoConstants::has_value()
	           ? magic::GetMagicEffectInfo(Locator::infoConstants::value(), type).computerCastDuration
	           : 0.0f;
}

uint32_t GameHandCasts::SeedGesture(MagicType type, int32_t stage) const
{
	if (!Locator::infoConstants::has_value())
	{
		return 0;
	}
	const auto& info = Locator::infoConstants::value();
	const auto seed = magic::FindFirstSpellSeedForMagicType(info, type);
	if (!seed.has_value())
	{
		return 0;
	}
	const auto& seedInfo = magic::GetSpellSeedInfo(info, *seed);
	return static_cast<uint32_t>(stage == 0 ? seedInfo.selectionGesture : seedInfo.gesture);
}

float GameHandCasts::GestureLength(uint32_t gesture) const
{
	const auto symbol = Locator::gestureSystem::has_value()
	                        ? Locator::gestureSystem::value().GetSymbol(static_cast<GestureType>(gesture))
	                        : std::nullopt;
	return symbol.has_value() ? symbol->length : 0.0f;
}

glm::vec3 GameHandCasts::GesturePoint(uint32_t gesture, float share) const
{
	const auto symbol = Locator::gestureSystem::has_value()
	                        ? Locator::gestureSystem::value().GetSymbol(static_cast<GestureType>(gesture))
	                        : std::nullopt;
	return symbol.has_value() ? creature_gesture::SamplePath(symbol->points, share) : glm::vec3(0.0f);
}

void GameHandCasts::ShowGesture(uint32_t gesture, glm::vec3 corner, float size)
{
	if (gesture == 0)
	{
		return;
	}
	if (Locator::gestureSystem::has_value())
	{
		Locator::gestureSystem::value().ShowGestureOnLand(static_cast<GestureType>(gesture), corner, size);
	}
	// The sound of a recognised gesture plays at the god's hand
	audio::PlayGameSoundEffect(static_cast<entt::id_type>(audio::SoundId::G_SpellGestureRecognise), corner);
}
