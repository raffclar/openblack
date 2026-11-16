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
#include <string_view>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

#include "Commands/PlayerCommands.h"
#include "ECS/Systems/WeatherSystemInterface.h"
#include "Enums.h"

/// The acts of the Magic, the Weather and the Moon debug windows on the world: the miracles, the dispensers, the bubbles,
/// the prayer power, the miracles of the towns, the storms, the snow, the climates, the clock of the day and the date of
/// the moon. In a network game each act goes as a command of the turns, and each computer does it at the turn. In a game
/// of this computer alone the act occurs at once.
namespace openblack::debug_acts
{

/// The limits of a storm of the Weather window, as its sliders give them
inline constexpr int8_t k_ColdestStorm = -30;
inline constexpr int8_t k_HottestStorm = 45;
inline constexpr int8_t k_MostPercent = 100;
inline constexpr int8_t k_MostWind = 100;
inline constexpr float k_ShortestStorm = 10.0f;
inline constexpr float k_LongestStorm = 3600.0f;
inline constexpr float k_ShortestFade = 0.1f;
inline constexpr float k_LongestFade = 60.0f;
inline constexpr float k_LongestFlashWait = 120.0f;
inline constexpr float k_HighestCloud = 500.0f;
inline constexpr float k_FastestRain = 5.0f;
/// The height above the land of a bubble that the Magic window puts down, and of the hand that casts a miracle
inline constexpr float k_BubbleHeight = 3.0f;
inline constexpr float k_CastHeight = 15.0f;
/// The speed of a miracle that the Magic window casts down on the land
inline constexpr float k_CastDownSpeed = 10.0f;

[[nodiscard]] std::string_view NameOf(player_commands::DebugMagic::Act act) noexcept;
[[nodiscard]] std::string_view NameOf(player_commands::DebugWeather::Act act) noexcept;
[[nodiscard]] std::string_view NameOf(player_commands::DebugSky::Act act) noexcept;

/// Why no computer does the act, none when the values are in their limits. The game server refuses an act with a
/// refusal, and each computer checks the act again at the turn.
[[nodiscard]] std::optional<std::string_view> MagicActRefusal(const player_commands::DebugMagic& act) noexcept;
[[nodiscard]] std::optional<std::string_view> TownMiracleRefusal(const player_commands::DebugTownMiracle& act) noexcept;
[[nodiscard]] std::optional<std::string_view> WeatherActRefusal(const player_commands::DebugWeather& act) noexcept;
[[nodiscard]] std::optional<std::string_view> SkyActRefusal(const player_commands::DebugSky& act) noexcept;

/// The parts of the world that the Running tab of the Magic window changes. The game gives its systems, and the tests
/// give a fake.
class MagicTarget
{
public:
	virtual ~MagicTarget() = default;
	virtual void SetIgnoreInfluence(bool ignore) = 0;
	/// Whether the miracle has a seed that a hand can hold
	[[nodiscard]] virtual bool HasSeed(MagicType type) const = 0;
	virtual void PlaceDispenser(glm::vec3 position, MagicType type) = 0;
	virtual void PlaceBubble(glm::vec3 position, MagicType type) = 0;
	virtual void GiveToHand(PlayerNames player, MagicType type) = 0;
	virtual void SummonFromWorship(PlayerNames player, MagicType type) = 0;
	/// The hand of the player is at the position and throws the miracle down
	virtual void CastAtPoint(PlayerNames player, MagicType type, glm::vec3 hand, glm::vec3 target, glm::vec3 forward) = 0;
	[[nodiscard]] virtual bool IsCreature(entt::entity entity) const = 0;
	virtual void CastOnCreature(PlayerNames player, MagicType type, entt::entity creature, glm::vec3 hand,
	                            glm::vec3 forward) = 0;
	virtual void DropSeed(PlayerNames player) = 0;
	/// Returns false when the player has no prayer power
	virtual bool SetPrayerPower(PlayerNames player, float chants) = 0;
	virtual bool SetInfinitePrayer(PlayerNames player, bool infinite) = 0;
};

/// Does an act of the Magic window for the player. Returns why nothing changed, none when the act occurred.
std::optional<std::string_view> ApplyMagicAct(MagicTarget& target, PlayerNames player, const player_commands::DebugMagic& act);

/// The parts of the world that the Weather window changes
class WeatherTarget
{
public:
	virtual ~WeatherTarget() = default;
	virtual void ClearStorms() = 0;
	virtual void SetStormCreation(bool enabled) = 0;
	virtual void SetClimates(bool enabled) = 0;
	virtual void ForceStorm(const ecs::systems::ForcedStorm& storm) = 0;
	virtual void StrikeLightning(bool bolt) = 0;
	virtual void MeltSnow() = 0;
	/// Whether the entity is a storm that still lives
	[[nodiscard]] virtual bool IsStorm(entt::entity entity) const = 0;
	virtual void EndStorm(entt::entity storm) = 0;
};

/// The storm of an act of the Weather window
[[nodiscard]] ecs::systems::ForcedStorm ForcedStormOf(const player_commands::DebugWeather& act) noexcept;

/// Does an act of the Weather window. Returns why nothing changed, none when the act occurred.
std::optional<std::string_view> ApplyWeatherAct(WeatherTarget& target, const player_commands::DebugWeather& act);

/// The parts of the world that the Moon window changes
class SkyTarget
{
public:
	virtual ~SkyTarget() = default;
	virtual void SetScriptHour(float hour) = 0;
	virtual void SetClockRuns(bool runs) = 0;
	/// The date that the moon takes its phase from. None gives the date of the computer again.
	virtual void SetMoonDate(std::optional<int64_t> date) = 0;
};

/// Does an act of the Moon window. Returns why nothing changed, none when the act occurred.
std::optional<std::string_view> ApplySkyAct(SkyTarget& target, const player_commands::DebugSky& act);

} // namespace openblack::debug_acts
