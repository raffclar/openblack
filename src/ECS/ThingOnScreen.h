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

#include "3D/ScreenPick.h"

/// Whether things show on the screen, as the scripts and the advisors' remarks ask it
namespace openblack::ecs::thing_on_screen
{

/// The view things are looked for in: the camera as it is now and the window's size, only the 4:3 middle of a wider
/// screen so that what is in view is what a 4:3 screen would show; none without a window
[[nodiscard]] std::optional<screen_pick::View> ScreenView();

/// Whether a thing shows on the screen: a thing with a model by its model's bounding sphere, anything else by the point
/// it stands at
[[nodiscard]] bool OnScreen(const screen_pick::View& view, entt::entity thing);

} // namespace openblack::ecs::thing_on_screen
