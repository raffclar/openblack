/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "DebugMagicWeatherActs.h"

#include <cmath>

#include <initializer_list>

#include "Magic/MagicTables.h"

namespace openblack::debug_acts
{

using player_commands::DebugMagic;
using player_commands::DebugSky;
using player_commands::DebugTownMiracle;
using player_commands::DebugWeather;

namespace
{

[[nodiscard]] bool IsFinite(glm::vec3 value) noexcept
{
	return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

/// Whether the number is a miracle that the Magic window gives: not the first type, which is no miracle
[[nodiscard]] bool IsMiracle(uint8_t magic) noexcept
{
	return magic != 0 && magic < magic::k_MagicTypeCount;
}

[[nodiscard]] bool InRange(float value, float least, float most) noexcept
{
	return std::isfinite(value) && value >= least && value <= most;
}

[[nodiscard]] bool InRange(int value, int least, int most) noexcept
{
	return value >= least && value <= most;
}

} // namespace

std::string_view NameOf(DebugMagic::Act act) noexcept
{
	switch (act)
	{
	case DebugMagic::Act::IgnoreInfluence:
		return "ignore_influence";
	case DebugMagic::Act::PlaceDispenser:
		return "place_dispenser";
	case DebugMagic::Act::PlaceBubble:
		return "place_bubble";
	case DebugMagic::Act::GiveToHand:
		return "give_to_hand";
	case DebugMagic::Act::SummonFromWorship:
		return "summon_from_worship";
	case DebugMagic::Act::CastAtPoint:
		return "cast_at_point";
	case DebugMagic::Act::CastOnCreature:
		return "cast_on_creature";
	case DebugMagic::Act::DropSeed:
		return "drop_seed";
	case DebugMagic::Act::PrayerPower:
		return "prayer_power";
	case DebugMagic::Act::InfinitePrayer:
		return "infinite_prayer";
	}
	return "unknown";
}

std::string_view NameOf(DebugWeather::Act act) noexcept
{
	switch (act)
	{
	case DebugWeather::Act::Preset:
		return "preset";
	case DebugWeather::Act::ForceStorm:
		return "force_storm";
	case DebugWeather::Act::ClearStorms:
		return "clear_storms";
	case DebugWeather::Act::Bolt:
		return "bolt";
	case DebugWeather::Act::Thunder:
		return "thunder";
	case DebugWeather::Act::MeltSnow:
		return "melt_snow";
	case DebugWeather::Act::Climates:
		return "climates";
	case DebugWeather::Act::ClimateStorms:
		return "climate_storms";
	case DebugWeather::Act::EndStorm:
		return "end_storm";
	}
	return "unknown";
}

std::string_view NameOf(DebugSky::Act act) noexcept
{
	switch (act)
	{
	case DebugSky::Act::ScriptHour:
		return "script_hour";
	case DebugSky::Act::ClockRuns:
		return "clock_runs";
	case DebugSky::Act::MoonDate:
		return "moon_date";
	case DebugSky::Act::ComputerDate:
		return "computer_date";
	case DebugSky::Act::Reset:
		return "reset";
	}
	return "unknown";
}

std::optional<std::string_view> MagicActRefusal(const DebugMagic& act) noexcept
{
	if (act.act > DebugMagic::Act::InfinitePrayer)
	{
		return "the act is not an act of the Magic window";
	}
	if (!IsFinite(act.position) || !IsFinite(act.forward) || !std::isfinite(act.value))
	{
		return "a value is not a number";
	}
	switch (act.act)
	{
	case DebugMagic::Act::PlaceDispenser:
	case DebugMagic::Act::PlaceBubble:
	case DebugMagic::Act::GiveToHand:
	case DebugMagic::Act::SummonFromWorship:
	case DebugMagic::Act::CastAtPoint:
	case DebugMagic::Act::CastOnCreature:
		if (!IsMiracle(act.magic))
		{
			return "the miracle is not a miracle";
		}
		return std::nullopt;
	default:
		return std::nullopt;
	}
}

std::optional<std::string_view> TownMiracleRefusal(const DebugTownMiracle& act) noexcept
{
	if (!IsMiracle(act.magic))
	{
		return "the miracle is not a miracle";
	}
	return std::nullopt;
}

std::optional<std::string_view> WeatherActRefusal(const DebugWeather& act) noexcept
{
	if (act.act > DebugWeather::Act::EndStorm)
	{
		return "the act is not an act of the Weather window";
	}
	if (act.act != DebugWeather::Act::Preset && act.act != DebugWeather::Act::ForceStorm)
	{
		return std::nullopt;
	}
	if (!InRange(act.temperature, k_ColdestStorm, k_HottestStorm))
	{
		return "the temperature is not from -30 to 45";
	}
	if (!InRange(act.rain, 0, k_MostPercent) || !InRange(act.snow, 0, k_MostPercent) ||
	    !InRange(act.overcast, 0, k_MostPercent))
	{
		return "the rain, the snow or the cloud is not from 0 to 100";
	}
	if (!InRange(act.windX, -k_MostWind, k_MostWind) || !InRange(act.windZ, -k_MostWind, k_MostWind))
	{
		return "the wind is not from -100 to 100";
	}
	if (!InRange(act.seconds, k_ShortestStorm, k_LongestStorm) || !InRange(act.fadeSeconds, k_ShortestFade, k_LongestFade))
	{
		return "the time of the storm or of its fade is not in the limits of the window";
	}
	for (const auto wait : {act.thunderWait.x, act.thunderWait.y, act.boltWait.x, act.boltWait.y})
	{
		if (!InRange(wait, 0.0f, k_LongestFlashWait))
		{
			return "the time between the flashes is not from 0 to 120 seconds";
		}
	}
	if (!InRange(act.cloudHeight, 0.0f, k_HighestCloud) || !InRange(act.rainSpeed, 0.0f, k_FastestRain))
	{
		return "the height of the clouds or the speed of the rain is not in the limits of the window";
	}
	return std::nullopt;
}

std::optional<std::string_view> SkyActRefusal(const DebugSky& act) noexcept
{
	if (act.act > DebugSky::Act::Reset)
	{
		return "the act is not an act of the Moon window";
	}
	if (act.act == DebugSky::Act::ScriptHour && !InRange(act.hour, 0.0f, 24.0f))
	{
		return "the hour is not from 0 to 24";
	}
	return std::nullopt;
}

std::optional<std::string_view> ApplyMagicAct(MagicTarget& target, PlayerNames player, const DebugMagic& act)
{
	if (const auto refusal = MagicActRefusal(act))
	{
		return refusal;
	}
	const auto type = static_cast<MagicType>(act.magic);
	const glm::vec3 hand = act.position + glm::vec3(0.0f, k_CastHeight, 0.0f);
	switch (act.act)
	{
	case DebugMagic::Act::IgnoreInfluence:
		target.SetIgnoreInfluence(act.on);
		break;
	case DebugMagic::Act::PlaceDispenser:
		target.PlaceDispenser(act.position, type);
		break;
	case DebugMagic::Act::PlaceBubble:
		target.PlaceBubble(act.position + glm::vec3(0.0f, k_BubbleHeight, 0.0f), type);
		break;
	case DebugMagic::Act::GiveToHand:
	case DebugMagic::Act::SummonFromWorship:
		if (!target.HasSeed(type))
		{
			return "the miracle has no seed";
		}
		if (act.act == DebugMagic::Act::GiveToHand)
		{
			target.GiveToHand(player, type);
		}
		else
		{
			target.SummonFromWorship(player, type);
		}
		break;
	case DebugMagic::Act::CastAtPoint:
		target.CastAtPoint(player, type, hand, act.position, act.forward);
		break;
	case DebugMagic::Act::CastOnCreature:
		if (!target.IsCreature(act.creature))
		{
			return "the thing is not a creature";
		}
		target.CastOnCreature(player, type, act.creature, hand, act.forward);
		break;
	case DebugMagic::Act::DropSeed:
		target.DropSeed(player);
		break;
	case DebugMagic::Act::PrayerPower:
		if (!target.SetPrayerPower(player, act.value))
		{
			return "the player has no prayer power";
		}
		break;
	case DebugMagic::Act::InfinitePrayer:
		if (!target.SetInfinitePrayer(player, act.on))
		{
			return "the player has no prayer power";
		}
		break;
	}
	return std::nullopt;
}

ecs::systems::ForcedStorm ForcedStormOf(const DebugWeather& act) noexcept
{
	ecs::systems::ForcedStorm storm;
	storm.effect = {.temperature = act.temperature,
	                .rain = act.rain,
	                .snow = act.snow,
	                .overcast = act.overcast,
	                .windX = act.windX,
	                .windZ = act.windZ};
	storm.seconds = act.seconds;
	storm.fadeSeconds = act.fadeSeconds;
	storm.thunderWait = act.thunderWait;
	storm.boltWait = act.boltWait;
	storm.cloudHeight = act.cloudHeight;
	storm.rainSpeed = act.rainSpeed;
	return storm;
}

std::optional<std::string_view> ApplyWeatherAct(WeatherTarget& target, const DebugWeather& act)
{
	if (const auto refusal = WeatherActRefusal(act))
	{
		return refusal;
	}
	switch (act.act)
	{
	case DebugWeather::Act::Preset:
		// The preset becomes the weather of the island: the other storms clear and the climates stop the new storms
		target.ClearStorms();
		target.SetStormCreation(false);
		if (act.rain != 0 || act.snow != 0)
		{
			target.ForceStorm(ForcedStormOf(act));
		}
		break;
	case DebugWeather::Act::ForceStorm:
		target.ForceStorm(ForcedStormOf(act));
		break;
	case DebugWeather::Act::ClearStorms:
		target.ClearStorms();
		break;
	case DebugWeather::Act::Bolt:
		target.StrikeLightning(true);
		break;
	case DebugWeather::Act::Thunder:
		target.StrikeLightning(false);
		break;
	case DebugWeather::Act::MeltSnow:
		target.MeltSnow();
		break;
	case DebugWeather::Act::Climates:
		target.SetClimates(act.on);
		break;
	case DebugWeather::Act::ClimateStorms:
		target.SetStormCreation(act.on);
		break;
	case DebugWeather::Act::EndStorm:
		if (!target.IsStorm(act.storm))
		{
			return "the thing is not a storm";
		}
		target.EndStorm(act.storm);
		break;
	}
	return std::nullopt;
}

std::optional<std::string_view> ApplySkyAct(SkyTarget& target, const DebugSky& act)
{
	if (const auto refusal = SkyActRefusal(act))
	{
		return refusal;
	}
	switch (act.act)
	{
	case DebugSky::Act::ScriptHour:
		target.SetScriptHour(act.hour);
		break;
	case DebugSky::Act::ClockRuns:
		target.SetClockRuns(act.on);
		break;
	case DebugSky::Act::MoonDate:
		target.SetMoonDate(act.date);
		break;
	case DebugSky::Act::ComputerDate:
		target.SetMoonDate(std::nullopt);
		break;
	case DebugSky::Act::Reset:
		target.SetMoonDate(std::nullopt);
		target.SetClockRuns(true);
		break;
	}
	return std::nullopt;
}

} // namespace openblack::debug_acts
