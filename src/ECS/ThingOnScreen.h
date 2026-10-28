/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <optional>

#include <entt/entity/entity.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "3D/ScreenPick.h"
#include "Enums.h"

/// Whether things show on the screen, as the scripts and the advisors' remarks ask it
namespace openblack::ecs::thing_on_screen
{

/// What a screen view is made from
struct ViewInputs
{
	/// The eye and the point that the camera looks at
	glm::vec3 eye {0.0f};
	glm::vec3 focus {0.0f};
	/// The lens across a 4:3 screen, in radians
	float lens {0.0f};
	/// How far the near plane is ahead of the eye
	float nearClip {1.0f};
	/// The screen's width over its height, and its size in pixels
	float aspect {1.0f};
	glm::vec2 resolution {0.0f};
};

/// The view of a camera with these inputs, as the camera itself makes its perspective: only the 4:3 middle of a wider
/// screen, so that what is in view is what a 4:3 screen would show
[[nodiscard]] screen_pick::View ViewOf(const ViewInputs& inputs);

/// The view things are looked for in: the view of the player as its last view report tells it (the turns read only
/// this), with the window of that player. None before the first view report or when the player has no window.
[[nodiscard]] std::optional<screen_pick::View> ScreenView(PlayerNames player);

/// Whether a thing shows on the screen: a thing with a model by its model's bounding sphere, anything else by the point
/// it stands at
[[nodiscard]] bool OnScreen(const screen_pick::View& view, entt::entity thing);

/// Whether the point where a thing stands shows on the screen, across the land as a map position holds it. The
/// model of the thing has no effect.
[[nodiscard]] bool PlaceOnScreen(const screen_pick::View& view, entt::entity thing);

} // namespace openblack::ecs::thing_on_screen
