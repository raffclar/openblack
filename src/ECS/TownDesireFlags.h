/*******************************************************************************
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

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include "3D/FieldCrop.h"
#include "Enums.h"

/// The flags over a town's store, one for each of some of the things the town wants. Each flies higher the more its town
/// wants that thing, easing up and down to it, and turns to face the camera. It shows only while its town's store
/// stands built and the town wants the thing at least a little.
namespace openblack::ecs::town_desire_flags
{

/// The store's model holds the places of the flags among its special points, from this one on: a flag's place in the
/// town's info adds to it
inline constexpr uint32_t k_FirstFlagPoint = 12;
/// How far up a flag flies when its town wants its thing wholly, in metres
inline constexpr float k_RiseHeight = 4.4f;
/// A flag that has just come out grows to its whole size over this share of the room above its first showing
inline constexpr float k_GrowShare = 0.2f;
/// How much snow a flag can carry, of 256: less than a field's crop
inline constexpr float k_SnowCap = 45.0f;
/// The hand's tooltip over a flag ("Need"), among the game's tooltips. The words shown are the flag's own.
inline constexpr uint32_t k_NeedToolTip = 21;

/// The store's special point a flag of a place in the town's info flies at; none for a desire that has no flag
[[nodiscard]] std::optional<uint32_t> PointOf(int32_t place);

/// Whether a flag shows: its store stands built and the flag is up at least as far as its desire first shows
[[nodiscard]] bool Shows(bool storeWorking, float level, float showsAfter);

/// A flag's level and how fast it moves, after a frame drawn: one not showing is put straight at its town's desire, one
/// showing eases towards its desire (never past wholly wanted), arriving at rest a second later as each frame sets off
/// again
[[nodiscard]] field_crop::Settle Step(field_crop::Settle level, bool shown, float desire, float seconds);

/// How far over its place a flag of a level is drawn
[[nodiscard]] float Rise(float level);

/// The size a flag of a level is drawn at, from nothing as it first shows to its whole size a little higher
[[nodiscard]] float DrawnScale(float level, float showsAfter);

/// The turn about the upright, in the game's sense, that has a flag at a place face the camera
[[nodiscard]] float FacingAngle(glm::vec3 flag, glm::vec3 camera);

/// Whether a point, in clip space, is in view: the flags hold still while they aren't drawn
[[nodiscard]] bool InView(glm::vec4 clip);

/// The words the hand shows over a flag of a desire, among the game's tooltips; none for a desire without its own words
[[nodiscard]] std::optional<uint32_t> ToolTipWords(TownDesireInfo desire);

/// The share the hand shows over a flag, in hundredths: its town's desire, never more than wholly wanted
[[nodiscard]] float NeedPercent(float desire);

} // namespace openblack::ecs::town_desire_flags
