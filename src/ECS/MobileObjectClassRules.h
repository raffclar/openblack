/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <chrono>

#include <glm/mat4x4.hpp>

namespace openblack::ecs
{
class Registry;
}

/// The rules of the loose objects that the game makes as a creed or as a crop (see mobile_object_record::Class)
namespace openblack::ecs::mobile_object_class
{

/// The glow of a creed object always has this power
constexpr float k_CreedGlowPower = 1.0f;

/// The matrix that the glow of a creed object draws the creed model with: the object's own matrix, with its axes
/// scaled by the scale of the glow
[[nodiscard]] glm::mat4 CreedGlowModel(const glm::mat4& objectModel, float glowScale);

/// The textures of the glows of the creed objects slide by the real time of a frame, at most once a frame of the
/// screen's refresh
void SlideCreedGlows(Registry& registry, std::chrono::milliseconds frameRealTime);

/// The size of a crop of a life: its life times the scale per life of its record
[[nodiscard]] float CropScale(float life, float scalePerLife);

/// Whether a crop set to a life goes from the world: a life of 0 or less removes it, also when the crop is protected
[[nodiscard]] constexpr bool CropGoes(float life)
{
	return life <= 0.0f;
}

} // namespace openblack::ecs::mobile_object_class
