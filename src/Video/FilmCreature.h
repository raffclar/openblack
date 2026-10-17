/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <optional>

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include "3D/FilmCameraPath.h"
#include "Creature/CreatureHandGlow.h"

/// The player's creature in the falling spell's film. The film shows a copy of the creature: its body, size, skin,
/// tattoos and alignment look. The copy holds nothing and never faints. It stands at the world's origin, facing as its
/// mesh does, and plays one action once from the start of the film, on the right side and at the pace of real time. A
/// camera path moves round it, and the renderer draws it over the film's picture. It glows white, then orange in a dark
/// red light that flickers, and the film does not show it for a time in the middle. The creature itself stays where it
/// is, with the game paused under the film.
namespace openblack::film_creature
{

/// The action the copy plays
constexpr size_t k_Action = 215;
/// The copy's pose moves on by this time before the action starts
constexpr int32_t k_FirstPoseMs = 1;
/// The film's time that the copy's pose counts from until the film's first frame. The first frame comes before it, so
/// in practice the pose counts from the start of the film.
constexpr int32_t k_StartMs = 100;
/// The film does not show the copy from this time to this time into the film, both included
constexpr int32_t k_HiddenFromMs = 19550;
constexpr int32_t k_HiddenToMs = 27350;
/// The copy's brightness rises from 0 to 255 up to the first time, and again from the first time to the second
constexpr int32_t k_FirstRiseMs = 19450;
constexpr int32_t k_SecondRiseEndMs = 27350;
/// The camera's position and the point it looks at are this much of the path's
constexpr float k_PathScale = 0.8f;
/// The camera sees this wide across, in radians, as on a 4:3 screen
constexpr float k_FieldOfView = 0.7853982f;
constexpr float k_ScreenAspect = 4.0f / 3.0f;
/// The light that the copy is lit by
constexpr glm::vec3 k_Light {0.0f, 0.0f, 1000.0f};

/// Moves the copy's pose on with the film's time: the time from the last frame, when the film has moved on
struct Clock
{
	int32_t lastMs {k_StartMs};
};
/// The milliseconds the pose moves on by at a time into the film, or none when the film has not moved on
[[nodiscard]] std::optional<int32_t> Step(Clock& clock, int32_t filmMs);

/// Whether the film shows the copy at a time into the film
[[nodiscard]] bool Shown(int32_t filmMs);
/// The copy's brightness at a time into the film, 0 to 255: it rises twice, and stays 0 after the second rise
[[nodiscard]] int32_t Brightness(int32_t filmMs);

/// The colours of the copy, 0xAARRGGBB: the colour that its light shades, and the colour added after
struct Look
{
	uint32_t colour {0};
	uint32_t added {0};
};
/// The copy's colours at a brightness. Three local random draws in this order give the flicker of the red, the green and
/// the blue: 0 to 31, 0 to 7 and 0 to 3. At brightness 0 it is black with white added; at 255 it is a dark red with
/// orange added.
[[nodiscard]] Look LookAt(int32_t brightness, uint32_t red, uint32_t green, uint32_t blue);

/// The copy has a glow on each hand, with a scale of 1, from the start. Each glow's power rises and falls by the film's
/// time. The left hand's glow rises from 1 to 3 in a second, falls back in two seconds, and rises again later. The right
/// hand's glow does the same a little after.
constexpr float k_GlowScale = 1.0f;
constexpr float k_StartGlowPower = 1.0f;
/// The power that the film gives a hand's glow at a time into the film, or none when the film does not change it then.
/// Only the left and the right hand have glows.
[[nodiscard]] std::optional<float> GlowPower(creature_hand_glow::Place hand, int32_t filmMs);

/// The camera's view at a point of its path: its axes from the point's frame, kept unit long, at the point's position
/// scaled by k_PathScale
[[nodiscard]] glm::mat4 View(const FilmCameraPath::Point& point);
/// The camera's projection: the world camera's depth, with k_FieldOfView across a 4:3 screen
[[nodiscard]] glm::mat4 Projection(const glm::mat4& worldProjection);

} // namespace openblack::film_creature
