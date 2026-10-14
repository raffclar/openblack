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

#include <entt/entity/entity.hpp>

#include "Gui/HandToolTipRules.h"

/// The facts of the world that the rules of the hand's tooltips read: the camera, the thing under the pointer and the
/// thing in the hand
namespace openblack::gui::hand_tooltip
{

/// The camera controls that the scripts let the player use now
[[nodiscard]] uint32_t CameraFeatures();

/// The state of the camera now
[[nodiscard]] CameraFacts CameraNow();

/// The two tooltips of a building that is not whole, which take turns
struct BuildingToolTips
{
	BuildingWords wood;
	double completed;
};

/// The tooltips of a building that is not fully built or not fully repaired, none for other things
[[nodiscard]] std::optional<BuildingToolTips> BuildingOf(entt::entity object);

/// Whether the hand can do something with a thing at all. A building with nothing built yet is not such a thing.
[[nodiscard]] bool IsInteractable(entt::entity object);

/// Whether a thing flies, rolls or slides above the land
[[nodiscard]] bool IsMoving(entt::entity object);

/// The tooltip of a tap on a thing that the hand cannot take, none when a tap does nothing to it
[[nodiscard]] std::optional<Offer> TapOf(entt::entity object);

/// The tooltip of a tap on a thing that the hand can take, none when a tap does nothing to it
[[nodiscard]] std::optional<uint32_t> TakeableTapOf(entt::entity object);

/// The tooltip over a scroll that a script put in the world, none for other things
[[nodiscard]] std::optional<Offer> ScrollOf(entt::entity object);

/// What the hand can do with the thing that it holds at the thing under the pointer
[[nodiscard]] HeldUse UseAt(entt::entity held, std::optional<entt::entity> target);

} // namespace openblack::gui::hand_tooltip
