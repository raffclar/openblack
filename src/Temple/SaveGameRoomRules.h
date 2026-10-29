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

#include <array>
#include <functional>
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

/// What the room's list of saved games and their small pictures come from. Two equal stamps tell that the game did not
/// save a game, delete a game or write the picture of a slot between them, for the same player on the same land.
struct SavedGamesStamp
{
	/// The count of the saves, the deletes and the slot pictures that the game started to write
	uint64_t changes {0};
	/// The player's folder in the profiles' folder, empty for none
	std::string player;
	uint32_t land {0};

	[[nodiscard]] bool operator==(const SavedGamesStamp&) const = default;
};

/// Whether the room reads its list and its small pictures again: at the first read, and when the saved games changed
/// after the last read
[[nodiscard]] bool ReadsSavedGamesAgain(const std::optional<SavedGamesStamp>& lastRead, const SavedGamesStamp& now);

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

/// The pictures that the room reads again after a save into a slot. The room always reads the small pictures again. It
/// reads the large picture again only when the picture of the slot shows in high detail.
struct PicturesToRead
{
	bool smallPictures;
	bool largePicture;

	bool operator==(const PicturesToRead&) const = default;
};
[[nodiscard]] PicturesToRead PicturesAfterSave(uint32_t slot, std::optional<uint32_t> highDetail);

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

/// The first slot that has a label of its own when it has no save
inline constexpr uint32_t k_FirstLabelSlot = 14;
/// The help texts of the labels of the slots from 14 when they have no save: "Previous Land", three times
/// "Auto Save Land: %3.0f", "Next Land", "Quick Save" and "Auto Save On Exit"
inline constexpr std::array<uint32_t, k_Slots - k_FirstLabelSlot> k_LabelTexts {6688, 6690, 6690, 6690, 6689, 6661, 6691};

/// The date and the name of the save in a slot
struct SaveWords
{
	std::u16string_view date;
	std::u16string_view name;
};
/// The words that the room writes on the screen while the camera looks closely at the picture in high detail
struct CloseLookWords
{
	/// The line at the top of the screen: the date and the time of the slot's save. A slot with no save has none.
	std::optional<std::u16string> title;
	/// The words below the title: the name of the slot's save, or the label of a slot with no save
	std::u16string details;

	bool operator==(const CloseLookWords&) const = default;
};
/// A help text by its number
using HelpTextFn = std::function<std::u16string(uint32_t number)>;
/// The words of the close look at a slot's picture. A slot with a save gives its date and its name. A slot from 14
/// with no save gives its label, with the land's number for "%3.0f". Other slots with no save give no words.
[[nodiscard]] std::optional<CloseLookWords> CloseLookWordsOf(uint32_t slot, std::optional<SaveWords> save, uint32_t landNumber,
                                                             const HelpTextFn& helpText);

/// The details are black and two pixels down first, then yellow
inline constexpr float k_DetailsShadowDrop = 2.0f;
/// The left and the right of the details' lines on the screen
struct DetailsSpan
{
	float left;
	float right;

	bool operator==(const DetailsSpan&) const = default;
};
/// The middle half of a screen, from its left side and its width in pixels: from a quarter of the width, rounded down,
/// to three times that quarter
[[nodiscard]] DetailsSpan DetailsSpanOf(int32_t left, int32_t width);
/// Where a line of the details starts across the screen, so that the line is centred in the span
[[nodiscard]] float DetailsLineLeft(const DetailsSpan& span, float lineWidth);

} // namespace openblack::temple::save_game_room
