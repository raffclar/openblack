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
#include <string>
#include <string_view>

#include <glm/vec3.hpp>

/// The rules of the temple's save game room: a wall of 21 framed pictures, one for each slot of the saved games, each
/// with a button to save, a button to load and a button to delete. The buttons show only while the camera is close to a
/// picture.
namespace openblack::temple::save_game_room
{

/// How many pictures the room's walls have, one for each slot
inline constexpr uint32_t k_Slots = 21;
/// The last slot that the player can save into
inline constexpr uint32_t k_LastPlayerSlot = 13;
/// How far in front of a picture the camera looks at it from, along the picture's frame
inline constexpr float k_LookDistance = 20.0f;
/// The longest name that the name box takes
inline constexpr size_t k_NameLength = 32;

/// The parts of the room's mesh that are controls
enum class Part : uint8_t
{
	/// The slot's picture, which a click looks at closer and, once close, loads or saves
	Picture,
	Save,
	Load,
	Delete,
};

/// A control of the room's mesh and its slot, from 0
struct Control
{
	Part part;
	uint32_t slot;

	bool operator==(const Control&) const = default;
};

/// The control that a submesh of the room's mesh is, by its name, if any: "LH_PICTURE_savegame_01" is the picture of
/// slot 0, "LH_BUTTON_SAVE00" its save button. The case of the letters is not important.
[[nodiscard]] std::optional<Control> ParseControl(std::string_view subMeshName);

/// What the room's controls depend on
struct View
{
	/// The camera is wholly close to the part it looks at
	bool closeUp {false};
	/// The camera was last sent to a picture rather than the scroll
	bool pictureLookedAt {false};
	/// The player is kept in this room, as at the end of the game, and cannot save
	bool keptInRoom {false};
	bool multiplayer {false};
};

/// Whether a control is drawn: the pictures always; while the camera is close to a picture, the save buttons of the
/// slots that the player can save into, and the load and delete buttons of the slots that have a save
[[nodiscard]] bool IsShown(Control control, const View& view, bool hasSave);

/// What a click on a control does
enum class Click : uint8_t
{
	Nothing,
	/// The camera goes to look at the picture from close by, and the picture is shown in high detail
	LookCloser,
	/// The player is asked whether to load the slot's save
	AskLoad,
	/// The player is asked whether to save over the slot's save, then for a name
	AskOverwrite,
	/// The player is asked for a name for a new save in the slot
	AskName,
	/// The player is asked whether to delete the slot's save
	AskDelete,
};
/// A click on a control. Close to a picture, a click on it loads the slot's save, or saves into an empty slot that the
/// player can save into. The save button saves over a save after a question, or into an empty slot.
[[nodiscard]] Click Clicked(Control control, const View& view, bool hasSave);

/// When the player pushes a button, the button moves back by this distance along the z axis of its frame, in the units
/// of the frame
inline constexpr float k_ButtonPushIn = 0.3f;
/// The distance and direction that a control moves in the mesh while the player holds the mouse button on it. A button
/// moves into the wall. A picture does not move.
[[nodiscard]] glm::vec3 HeldOffset(Part part, glm::vec3 frameZ);

/// The part of a texture that a picture shows, from 0 to 1
struct UvRect
{
	float u0;
	float v0;
	float u1;
	float v1;

	bool operator==(const UvRect&) const = default;
};
/// A picture with no save shows the empty picture: its right half for a player who is good or neither, its left half
/// for an evil player
[[nodiscard]] UvRect EmptyPicture(float alignment);

/// The help texts of the room's tooltips, by their numbers
inline constexpr uint32_t k_SaveText = 0xE9C;
inline constexpr uint32_t k_LoadText = 0xE9D;
inline constexpr uint32_t k_DeleteText = 0xE9E;
inline constexpr uint32_t k_ZoomInText = 0xE75;
inline constexpr uint32_t k_ZoomInNamedText = 0xEE6;
inline constexpr uint32_t k_ZoomOutText = 0xE8B;

/// The name of a save as a tooltip shows it: a name of 16 characters or more is cut to 13 characters and three dots
[[nodiscard]] std::u16string ToolTipName(std::u16string_view name);

/// What a picture's tooltip depends on
struct PictureToolTipInput
{
	uint32_t slot;
	bool hovered;
	/// How close the camera is to the part it looks at, from 0 to 1
	float zoom;
	bool pictureLookedAt;
	/// The slot of the picture shown in high detail
	std::optional<uint32_t> highDetail;
	/// A button before this picture, in the order of the mesh, has the cursor
	bool buttonHovered;
	/// The name of the slot's save, if it has one
	std::optional<std::u16string_view> saveName;
	bool multiplayer;
};
/// The tooltip that a picture chooses, if it chooses one
struct PictureToolTip
{
	uint32_t text;
	/// The name the tooltip shows, for the named zoom. A picture's tooltips have no arrows.
	std::u16string name;
};
/// The tooltip of a picture. Hovered from afar, or close while the camera looks at the scroll, it zooms in, with the
/// save's name if the slot has a named save. Hovered close up as the picture in high detail, it loads the slot's save,
/// or saves into an empty slot that the player can save into. Not hovered while the camera looks at it close up, it
/// zooms out.
[[nodiscard]] std::optional<PictureToolTip> PictureToolTipOf(const PictureToolTipInput& input);
/// Whether a picture's tooltip, as above, is one of its zooms in
[[nodiscard]] bool PictureZoomsIn(const PictureToolTipInput& input);

} // namespace openblack::temple::save_game_room
