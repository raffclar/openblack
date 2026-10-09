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

#include <array>
#include <optional>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

// Where a player's worship sites stand around the temple, and when a town may have one: pure rules, free of the game

namespace openblack::ecs::worship_site
{

/// The places for worship sites around a temple
inline constexpr uint32_t k_Places = 6;
/// How far round each place is from the last: a seventh of a turn, as the game rounds it
inline constexpr float k_PlaceSpacing = 0.8975979f;
/// The points of the worship site's model: where a site's place is judged from, and where its altar stands
inline constexpr uint32_t k_PlacePoint = 9;
inline constexpr uint32_t k_AltarPoint = 8;
/// The land on which no worship sites are made for towns: the first land, the tutorial's
inline constexpr int32_t k_LandWithoutSites = 1;

/// Which way a site in a place faces: the temple's facing, and a seventh of a turn more for each place round
[[nodiscard]] float PlaceFacing(float templeFacing, uint32_t place);

/// A point of a model on the land, its model set down at a spot and turned to face some way (in radians): across the
/// land only, as the game measures the places
[[nodiscard]] glm::vec2 TurnedPoint(glm::vec2 spot, float facing, glm::vec3 point);

/// Of the free places around a temple, the one whose site would stand nearest a spot on the land, judged by where the
/// model's place point would be. The game turns the point only by the place's own share of a turn, not by the way the
/// temple faces, so a temple set down turned has its places judged as if it faced the first way. None when every place
/// is taken.
[[nodiscard]] std::optional<uint32_t> NearestFreePlace(const std::array<bool, k_Places>& taken, glm::vec2 temple,
                                                       glm::vec3 placePoint, glm::vec2 spot);

/// Whether a town may have a worship site made for it: never on the first land, nor once a script has stopped it, nor
/// while nobody lives in it
[[nodiscard]] bool MayHaveSite(int32_t landNumber, bool stoppedByScript, uint32_t population);

} // namespace openblack::ecs::worship_site
