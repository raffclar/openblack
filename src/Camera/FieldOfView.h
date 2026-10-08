/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <entt/entity/fwd.hpp>
#include <glm/vec3.hpp>

#include "Graphics/RegionOnScreen.h"

/// The scripts' "is it on the screen" tests: GAME_THING_FIELD_OF_VIEW 011 and POS_FIELD_OF_VIEW 012. Both go through
/// the drawn camera's world-to-clipping matrix (billboard::CameraFrame::clipMatrices.worldToClipping, as
/// ShadowMath::BlockVisible), the near plane (billboard::CameraFrame::nearZ) and the screen size.
namespace openblack::field_of_view
{

/// The drawn camera's screen tests are graphics::region_on_screen's (Graphics/RegionOnScreen.h: ScreenView,
/// PointOnScreen, SphereOnScreen)
using View = graphics::region_on_screen::ScreenView;

/// The main camera of openblack as the view (Locator::camera, Locator::windowing); false when there is none
[[nodiscard]] bool CurrentView(View& out);
/// POS_FIELD_OF_VIEW 012: inside the temple false (pending: openblack has no temple interior state, taken as
/// outside); else PointOnScreen
[[nodiscard]] bool PosInView(const glm::vec3& point);
/// GAME_THING_FIELD_OF_VIEW 011: nothing, or inside the temple: false. An Object: its 3D object's mesh (none -> false)
/// bounding sphere through SphereOnScreen. Any other thing with a position: the point (x, altitude + its height
/// offset, z) through PointOnScreen. (approximate) openblack's "Object" is an entity with a Mesh: its box centre
/// through the Transform and the box's half diagonal x the largest scale as the bounding radius (as Renderer.cpp's
/// culling); without a Mesh its Transform position (the land height plus the offset are already in it)
[[nodiscard]] bool ThingInView(entt::entity thing);
/// The object's mesh (none -> false), then its bounding sphere through SphereOnScreen. No temple test (ThingInView's
/// caller has it). Also false without a Mesh or without a view (openblack: no camera / window, the unit tests)
[[nodiscard]] bool ObjectOnScreen(entt::entity thing);

} // namespace openblack::field_of_view
