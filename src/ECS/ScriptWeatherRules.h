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
#include <span>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "ECS/Components/Weather.h"

/// How a script's weather commands change the storm a weather thing brought
namespace openblack::ecs::script_weather
{

/// A script's amount, 0 to 1, as the storm keeps it in whole percent, cut towards nought (and wrapping past a byte as
/// the storm's bytes do)
[[nodiscard]] int8_t Percent(float amount);

/// The weather the storm brings: the temperature it pulls towards, its rain, snow and cloud from 0 to 1, and how fast its
/// rain falls
void SetWeather(components::Storm& storm, float temperature, float rainfall, float snowfall, float overcast, float fallSpeed);
/// How many seconds the storm lasts and how many it takes to come in and to clear, counted from when it was made
void SetTimeFade(components::Storm& storm, float lastsFor, float fadeSeconds);
/// Its own clouds: how many (cut to a whole number), how dark from 0 to 1 and how high
void SetClouds(components::Storm& storm, float count, float shade, float height);
/// The least and most seconds between its flashes of sheet lightning with thunder, and between its forks of lightning
/// striking the ground; a most of 0 has none
void SetLightning(components::Storm& storm, float sheetMin, float sheetMax, float forkMin, float forkMax);
/// How far from its middle it is at full strength and how far it reaches
void SetInnerOuter(components::Storm& storm, float inner, float outer);

/// The waits to its next flash and next fork, drawn afresh whenever a script changes the storm: the fork first, then the
/// flash, each from its least up to its most
template <typename GameFloatRand>
void DrawLightningWaits(components::Storm& storm, GameFloatRand&& gameFloatRand)
{
	storm.boltTimer = gameFloatRand(storm.boltWait.y - storm.boltWait.x) + storm.boltWait.x;
	storm.thunderTimer = gameFloatRand(storm.thunderWait.y - storm.thunderWait.x) + storm.thunderWait.x;
}

/// The wind a weather thing's storm is blown along by, in the wind's bytes
struct Wind
{
	int8_t x {0};
	int8_t z {0};
};

/// A climate's say in that wind: where its middle is, how far it is at full strength and how far it reaches, and its own
/// wind
struct ClimateWind
{
	glm::vec2 centre;
	float innerRadius;
	float outerRadius;
	float windX;
	float windZ;
};

/// How much of a climate's wind blows at a point: all of it within its inner radius, none past its outer, and in between
/// less the further out; none when the point is outside it
[[nodiscard]] std::optional<float> ClimateShare(const ClimateWind& climate, glm::vec2 point);

/// The wind that blows a weather thing's storm: the air's wind where it is, then the global climate's wind, then a share
/// of each climate's wind that reaches it (the global climate's again among them), in the order given, each part cut
/// to a whole number and kept to a byte, the sum held within a byte
[[nodiscard]] Wind DriftWind(Wind air, const ClimateWind& global, std::span<const ClimateWind> climates, glm::vec2 point);

/// A turn of the wind on a storm the wind blows: a hundredth of the wind's bytes
[[nodiscard]] glm::vec3 Drift(glm::vec3 position, Wind wind);

} // namespace openblack::ecs::script_weather
