/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "DebugMagicWeatherActRuns.h"

#include <tuple>
#include <utility>

#include "Common/DebugActRuns.h"
#include "Common/DebugMagicWeatherActs.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/Player.h"
#include "ECS/Components/PrayerPower.h"
#include "ECS/Components/Sky.h"
#include "ECS/Registry.h"
#include "ECS/Systems/MagicSystemInterface.h"
#include "ECS/Systems/PlayerCommandSystemInterface.h"
#include "ECS/Systems/SkyClockSystemInterface.h"
#include "ECS/Systems/SnowSystemInterface.h"
#include "ECS/Systems/WeatherSystemInterface.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/MagicTables.h"
#include "Magic/SpellRules.h"
#include "Particles/ParticleSpellLink.h"

namespace openblack::debug_acts
{

namespace
{

/// The prayer power of a player, none when the player has none
ecs::components::PrayerPower* PrayerOf(PlayerNames player)
{
	ecs::components::PrayerPower* found = nullptr;
	Locator::entitiesRegistry::value().Each<const ecs::components::Player, ecs::components::PrayerPower>(
	    [player, &found](entt::entity, const ecs::components::Player& owner, ecs::components::PrayerPower& prayer) {
		    if (owner.name == player && found == nullptr)
		    {
			    found = &prayer;
		    }
	    });
	return found;
}

/// The miracles of the game for the acts of the Magic window
class GameMagic final: public MagicTarget
{
public:
	GameMagic(ecs::systems::MagicSystemInterface& magic, const InfoConstants& info)
	    : _magic(magic)
	    , _info(info)
	{
	}

	void SetIgnoreInfluence(bool ignore) override { _magic.SetIgnoreInfluence(ignore); }

	[[nodiscard]] bool HasSeed(MagicType type) const override
	{
		return magic::FindFirstSpellSeedForMagicType(_info, type).has_value();
	}

	void PlaceDispenser(glm::vec3 position, MagicType type) override { _magic.CreateDispenser(position, type, 0.0f); }

	void PlaceBubble(glm::vec3 position, MagicType type) override { _magic.CreateOneOffSeedFor(position, type); }

	void GiveToHand(PlayerNames player, MagicType type) override
	{
		const auto seed = *magic::FindFirstSpellSeedForMagicType(_info, type);
		const auto step = magic::GetPowerUpGesture(magic::GetSpellSeedInfo(_info, seed), type);
		_magic.GiveSeedToHand(player, seed, step.level, 1.0f);
	}

	void SummonFromWorship(PlayerNames player, MagicType type) override
	{
		const auto seed = *magic::FindFirstSpellSeedForMagicType(_info, type);
		const auto step = magic::GetPowerUpGesture(magic::GetSpellSeedInfo(_info, seed), type);
		_magic.SummonSeed(player, seed, step.level);
	}

	void CastAtPoint(PlayerNames player, MagicType type, glm::vec3 hand, glm::vec3 target, glm::vec3 forward) override
	{
		_magic.CastAtPoint(type, player, target, CastData(type), Process(hand, forward));
	}

	[[nodiscard]] bool IsCreature(entt::entity entity) const override
	{
		const auto& registry = Locator::entitiesRegistry::value();
		return registry.Valid(entity) && registry.AllOf<ecs::components::Creature>(entity);
	}

	void CastOnCreature(PlayerNames player, MagicType type, entt::entity creature, glm::vec3 hand, glm::vec3 forward) override
	{
		_magic.CastOnObject(type, player, creature, CastData(type), Process(hand, forward));
	}

	void DropSeed(PlayerNames player) override { _magic.DiscardHeldSeedOf(player); }

	bool SetPrayerPower(PlayerNames player, float chants) override
	{
		auto* prayer = PrayerOf(player);
		if (prayer != nullptr)
		{
			prayer->chants = chants;
		}
		return prayer != nullptr;
	}

	bool SetInfinitePrayer(PlayerNames player, bool infinite) override
	{
		auto* prayer = PrayerOf(player);
		if (prayer != nullptr)
		{
			prayer->infinite = infinite;
		}
		return prayer != nullptr;
	}

private:
	/// The cast of the first seed of the miracle at the plain strength, with the radius of the normal cost
	[[nodiscard]] magic::SpellCastData CastData(MagicType type) const
	{
		const auto seed = magic::FindFirstSpellSeedForMagicType(_info, type).value_or(SpellSeedType::None);
		float size = 1.0f;
		if (const auto* radius = magic::GetMagicInfoAs<GMagicRadiusSpellInfo>(_info, type))
		{
			size = radius->radiusForNormalCost;
		}
		return magic::SeedCastData(_info, type, seed, 1.0f, size);
	}

	[[nodiscard]] static particles::ProcessInfo Process(glm::vec3 hand, glm::vec3 forward)
	{
		return {.handPosition = hand, .cameraForward = forward, .direction = glm::vec3(0.0f, -k_CastDownSpeed, 0.0f)};
	}

	ecs::systems::MagicSystemInterface& _magic;
	const InfoConstants& _info;
};

/// The weather and the snow of the game for the acts of the Weather window
class GameWeather final: public WeatherTarget
{
public:
	explicit GameWeather(ecs::systems::WeatherSystemInterface& weather)
	    : _weather(weather)
	{
	}

	void ClearStorms() override { _weather.ClearStorms(); }
	void SetStormCreation(bool enabled) override { _weather.SetStormCreationEnabled(enabled); }
	void SetClimates(bool enabled) override { _weather.SetClimateSystemEnabled(enabled); }
	void ForceStorm(const ecs::systems::ForcedStorm& storm) override { _weather.ForceStorm(storm); }
	void StrikeLightning(bool bolt) override { _weather.StrikeLightning(bolt); }

	void MeltSnow() override
	{
		if (Locator::snowSystem::has_value())
		{
			Locator::snowSystem::value().Reset();
		}
	}

	[[nodiscard]] bool IsStorm(entt::entity entity) const override { return _weather.StormLives(entity); }
	void EndStorm(entt::entity storm) override { _weather.EndStorm(storm); }

private:
	ecs::systems::WeatherSystemInterface& _weather;
};

/// The clock of the day and the moon of the game for the acts of the Moon window
class GameSky final: public SkyTarget
{
public:
	void SetScriptHour(float hour) override
	{
		if (Locator::skyClockSystem::has_value())
		{
			Locator::skyClockSystem::value().GetClock().SetScriptTime(hour);
		}
	}

	void SetClockRuns(bool runs) override
	{
		if (Locator::skyClockSystem::has_value())
		{
			Locator::skyClockSystem::value().GetClock().SetRunning(runs);
		}
	}

	void SetMoonDate(std::optional<int64_t> date) override
	{
		if (!Locator::skyClockSystem::has_value() || !Locator::entitiesRegistry::has_value())
		{
			return;
		}
		auto* moon = Locator::entitiesRegistry::value().TryGet<ecs::components::Moon>(
		    Locator::skyClockSystem::value().GetEntities().moon);
		if (moon != nullptr)
		{
			moon->dateOverride = date;
		}
	}
};

void Submit(player_commands::Action action)
{
	if (Locator::playerCommandSystem::has_value())
	{
		Locator::playerCommandSystem::value().Submit(std::move(action));
	}
}

} // namespace

std::optional<std::string_view> RunMagicAct(PlayerNames player, const player_commands::DebugMagic& act)
{
	if (!Locator::magicSystem::has_value() || !Locator::infoConstants::has_value())
	{
		return "the game has no miracles";
	}
	GameMagic target(Locator::magicSystem::value(), Locator::infoConstants::value());
	return ApplyMagicAct(target, player, act);
}

debug::town_miracles::Outcome RunTownMiracle(const player_commands::DebugTownMiracle& act)
{
	if (const auto refusal = TownMiracleRefusal(act))
	{
		return {.error = std::string(*refusal)};
	}
	if (!Locator::entitiesRegistry::has_value())
	{
		return {.error = "no land is open"};
	}
	const auto& registry = Locator::entitiesRegistry::value();
	const auto& towns = registry.Context().towns;
	const auto town = towns.find(act.town);
	if (town == towns.end() || !registry.Valid(town->second))
	{
		return {.error = "the number is not a town of the land"};
	}
	return debug::town_miracles::Apply({.town = town->second,
	                                    .type = static_cast<MagicType>(act.magic),
	                                    .give = act.give,
	                                    .withPlain = act.withPlain,
	                                    .allLevels = act.allLevels,
	                                    .openSite = act.openSite,
	                                    .anyLand = act.anyLand});
}

std::optional<std::string_view> RunWeatherAct(const player_commands::DebugWeather& act)
{
	if (!Locator::weatherSystem::has_value())
	{
		return "the island has no weather";
	}
	GameWeather target(Locator::weatherSystem::value());
	return ApplyWeatherAct(target, act);
}

std::optional<std::string_view> RunSkyAct(const player_commands::DebugSky& act)
{
	GameSky target;
	return ApplySkyAct(target, act);
}

void Do(const player_commands::DebugMagic& act)
{
	if (SendsCommands())
	{
		Submit(act);
		return;
	}
	std::ignore = RunMagicAct(PlayerNames::PLAYER_ONE, act);
}

void Do(const player_commands::DebugWeather& act)
{
	if (SendsCommands())
	{
		Submit(act);
		return;
	}
	std::ignore = RunWeatherAct(act);
}

void Do(const player_commands::DebugSky& act)
{
	if (SendsCommands())
	{
		Submit(act);
		return;
	}
	std::ignore = RunSkyAct(act);
}

std::optional<debug::town_miracles::Outcome> Do(const player_commands::DebugTownMiracle& act)
{
	if (SendsCommands())
	{
		Submit(act);
		return std::nullopt;
	}
	return RunTownMiracle(act);
}

} // namespace openblack::debug_acts
