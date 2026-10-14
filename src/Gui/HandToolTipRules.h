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
#include <vector>

#include "Gui/ToolTips.h"

/// The tooltips that the hand shows for what it can do: with nothing under it, over a thing that it can take or tap,
/// while it holds a thing, and over a building that wants wood. Each turn the hand submits the tooltips of one of these
/// rules in their order. Two tooltips of the same priority then take turns, one for each display time.
namespace openblack::gui::hand_tooltip
{

/// "Pick Up", over a thing that the hand can take
constexpr uint32_t k_PickUp = 0;
/// "Throw", while the hand holds a thing
constexpr uint32_t k_Throw = 1;
/// "Pitch" and "Rotate", at the places of the screen where a drag of the land tilts or turns the camera
constexpr uint32_t k_Pitch = 3;
constexpr uint32_t k_Rotate = 4;
/// "Tap", over a thing that a tap of the Action button works on
constexpr uint32_t k_Tap = 7;
/// "Move", over the land
constexpr uint32_t k_Move = 11;
/// "Catch", over a thing that flies, rolls or slides
constexpr uint32_t k_Catch = 13;
/// "Activate", over a scroll that a script has not started; "Reminder" over a scroll that it started
constexpr uint32_t k_Activate = 16;
constexpr uint32_t k_Reminder = 45;
/// "Give", while the hand holds a thing that a creature can take from it
constexpr uint32_t k_Give = 20;
/// "Zoom Out", with the camera near the land and pointed down
constexpr uint32_t k_ZoomOut = 24;
/// "Supply", while the hand holds a thing that the store under it takes
constexpr uint32_t k_Supply = 27;
/// "Drop" and "Plant", what the hand does when it lets go of a thing; a tree is planted
constexpr uint32_t k_Drop = 123;
constexpr uint32_t k_Plant = 124;
/// "Building Completed: %3.0f%%" and "Wood Required: %3.0f", over a building that is not built or not repaired
constexpr uint32_t k_BuildingCompleted = 129;
constexpr uint32_t k_WoodRequired = 136;
/// "Tap To Break" over a big rock, and "Tap To Open" over a reward chest
constexpr uint32_t k_TapToBreak = 132;
constexpr uint32_t k_TapToOpen = 133;

/// A tooltip that the hand submits: its words, the button of its mouse and its arrows
struct Offer
{
	uint32_t index;
	ToolTipAction action {ToolTipAction::None};
	uint32_t arrows {ToolTipArrows::k_None};

	[[nodiscard]] bool operator==(const Offer&) const = default;
};

/// The camera's state, as it shows in the tooltip of the hand over the land
struct CameraFacts
{
	/// The camera hints at the place of the cursor, as camera_drag::tricon
	uint32_t tricons {0};
	/// The camera controls that the scripts let the player use, as camera_drag::feature
	uint32_t features {0};
	/// The distance from the camera to the land at the middle of the screen, none when that line touches no land
	std::optional<float> centreDistance;
	/// How steeply the camera looks down, in radians
	float pitch {0.0f};
};

/// The camera hint bits and feature bits that these rules read. They have the same values as camera_drag's.
constexpr uint32_t k_RotateHint = 0x01;
constexpr uint32_t k_PitchHint = 0x02;
constexpr uint32_t k_PitchFeature = 0x01;
constexpr uint32_t k_RotateFeature = 0x02;
constexpr uint32_t k_ZoomFeature = 0x04;
/// The camera offers "Zoom Out" when the land at the middle of the screen is nearer than this, and when the camera looks
/// down more steeply than k_ZoomOutPitch
constexpr float k_ZoomOutDistance = 15.0f;
constexpr float k_ZoomOutPitch = 0.55f;

/// What the empty hand offers over the land, or over a thing that it can do nothing with:
/// - "Pitch" where a drag tilts the camera;
/// - "Rotate" where a drag turns it;
/// - "Zoom Out" when the camera is near the land and looks down steeply;
/// - "Move" in all other places.
/// The scripts can stop each camera control, and then the hand does not offer it.
[[nodiscard]] Offer OverNothing(const CameraFacts& camera);

/// What the empty hand offers over a thing that it can take: "Catch" while the thing flies, rolls or slides; else "Pick
/// Up", and after it the thing's tap tooltip, if a tap works on the thing. The two take turns.
[[nodiscard]] std::vector<Offer> OverTakeable(bool moving, std::optional<uint32_t> tapToolTip);

/// What a tap of a thing does, by the kind of thing
enum class TapKind : uint8_t
{
	/// A building of a town: the people come out
	Knock,
	/// A big rock breaks
	Break,
	/// The player's reward chest opens
	Open,
};

/// The tooltip of a tap, by the Action button
[[nodiscard]] Offer TapOffer(TapKind kind);

/// The tooltip over a scroll that a script put in the world: "Activate" before the script starts it, "Reminder" after
[[nodiscard]] Offer ScrollOffer(bool started);

/// What the hand can do with the thing it holds, at the thing under the pointer
enum class HeldUse : uint8_t
{
	/// Let go of it or throw it
	None,
	/// Give it to the creature under the pointer
	Give,
	/// Put it into the store under the pointer
	Supply,
};

/// What the hand offers while it holds a thing. Outside the player's influence it offers nothing of its own: the hand
/// then shows the tooltips that it shows over the land. Over a creature that can take the thing, "Give"; over a store
/// of its resource, "Supply". In all other places "Drop" (or "Plant" for a tree) and "Throw", which take turns.
[[nodiscard]] std::optional<std::vector<Offer>> Holding(bool handInInfluence, HeldUse use, bool tree);

/// Whether the hand shows "Wood Required" and "Building Completed" over a building. The hand shows them over a
/// building that is not fully built or not fully repaired. The building must have no player or the local player, and
/// the scripts must let the player tilt the camera. The influence of the hand has no effect.
[[nodiscard]] bool OffersBuildingToolTips(bool whole, bool ownOrNoPlayer, uint32_t features);

/// The words and number of a building's "Wood Required" tooltip. With a site that takes wood, it shows the wood that
/// the site wants, at least zero. With no such site, it shows the words and number of "Building Completed".
struct BuildingWords
{
	uint32_t words;
	double number;
};
[[nodiscard]] BuildingWords WoodRequired(std::optional<float> woodNeeded, double completed);

/// The percentage that "Building Completed" shows: of the repair for a built building, else of the building's build
[[nodiscard]] double BuildingCompleted(bool built, float builtShare, float life);

} // namespace openblack::gui::hand_tooltip
