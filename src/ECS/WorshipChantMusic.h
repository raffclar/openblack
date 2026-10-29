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

#include <glm/vec3.hpp>

#include "Sound/GameMusic.h"

/// The chant of the worshippers at a worship site, which the camera hears near the site
namespace openblack::ecs::worship_chant_music
{

/// The camera hears the sites of the temple nearest to it, nearer than this
inline constexpr float k_TempleFurthest = 150.0f;
/// Of those, it hears the site whose dance is nearest to it, nearer than this
inline constexpr float k_SiteFurthest = 100.0f;
/// And only while it is less than this above or below the land under the dance
inline constexpr float k_Highest = 100.0f;

/// The temple nearest the camera, nearer than k_TempleFurthest: the first of equally near ones, in the order given
[[nodiscard]] std::optional<size_t> NearestTemple(std::span<const glm::vec3> temples, glm::vec3 camera);

/// A site of a temple, as the camera hears it
struct Site
{
	/// The middle of its dance
	glm::vec3 dance {0.0f};
	/// How many dance it
	uint32_t dancers {0};
};
/// The site heard among a temple's sites by their places round it: one with dancers, its dance nearest the camera and
/// nearer than k_SiteFurthest, the first of equally near ones
[[nodiscard]] std::optional<size_t> NearestSite(std::span<const std::optional<Site>> sites, glm::vec3 camera);

/// Whether the camera is near enough the land under the dance in height to hear it
[[nodiscard]] bool NearInHeight(float camera, float groundAtDance);

/// The chant the camera hears, and where its dance is: none when no site is heard
[[nodiscard]] std::optional<audio::GameMusic::TurnInputs::CreatureDance> Heard(glm::vec3 camera);

} // namespace openblack::ecs::worship_chant_music
