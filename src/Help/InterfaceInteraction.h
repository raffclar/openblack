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

/// The script's interface interaction level (CHL 063 SET_INTERFACE_INTERACTION, SCRIPT_INTERFACE_LEVEL): how much of
/// the interface the player keeps while a tutorial script teaches one gesture. Each level writes:
/// - the script's level (always, also for an invalid one);
/// - the interface flags (Input/InterfaceActive.h): = 0 (0, 6, 7: the interface is active again), |= 6 or |= 4;
/// - camera_help::EnableCameraFeatures(F, -1) (Camera/CameraHelp.h), and for 1, 2 and 10 the auto-pitch
///   camera_help::SetAutoPitch(0.448799, 15.0, true);
/// - the hand's reach (min(R, 1800)): the hand keeps it (HandSystemInterface::SetHandReach), this only asks for it
///   (LevelHandReach);
/// - the two control map switches.
namespace openblack::help::interface_interaction
{

/// The hand's largest reach (its starting value and the bound of every reach). The hand owns the reach and its bound;
/// these are the values the levels pass
constexpr float k_MaxHandReach = 1800.0f;
/// JUST_GRAB's reach
constexpr float k_JustGrabHandReach = 75.0f;
/// The auto-pitch arguments for levels 1, 2 and 10
constexpr float k_TutorialAutoPitchAngle = 0.448799f;
constexpr float k_TutorialAutoPitchDistance = 15.0f;

/// Above 15 (unsigned) and 9: "Unexpected interaction = %d", only the level written
void Set(int32_t level);
/// The script's level. (pending) no reader found besides the save; 0 at start (inferred)
[[nodiscard]] int32_t GetLevel();

/// The reach R a level asks for (the hand keeps R <= 1800 ? R : 1800): 75 for JUST_GRAB, 1800 for the other levels
/// that set it, nullopt for JUST_HAND_MOVE (8), JUST_HAND_INTERACTION (12) and the invalid levels, which leave the
/// reach as it is. Set hands it to HandSystemInterface::SetHandReach, whose readers are the hand's: the interface's
/// hand update, the hand's movement and its distance from the view (clamped to [2, reach])
[[nodiscard]] std::optional<float> LevelHandReach(int32_t level);

/// Whether the control map allows the camera moves (on at start)
[[nodiscard]] bool CameraMovesAllowed();
/// Whether the control map allows the realm zooms (on at start)
[[nodiscard]] bool RealmZoomsAllowed();
/// The game's key handling: the LH_KEY 2..15 block (KB_1..KB_TAB: openblack's camera bookmark keys) needs both
/// switches
[[nodiscard]] bool KeyShortcutsEnabled();
/// Asked by the control map before an action counts: true = the bindable action is blocked. Camera moves not allowed:
/// actions 3..19 but 5; camera moves allowed and realm zooms not: 17..19.
/// (inferred) with openblack's BindableActionMap bit order (Input/GameActionMapInterface.h) that is every camera
/// move but TALK, and ZOOM_TO_TEMPLE / ZOOM_TO_CREATURE / ZOOM_TO_REALM. (pending) GameActionMap does not ask it yet
[[nodiscard]] bool IsActionBlocked(int32_t action);

/// The initial state: level 0, both switches on (the interface flags, the camera features and the hand's reach have
/// their own Reset / initial values)
void Reset();

} // namespace openblack::help::interface_interaction
