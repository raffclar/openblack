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
#include <span>
#include <string>
#include <string_view>

/// The rules of the temple's challenge room: its wall of 21 framed pictures, one for each challenge the player has
/// started in the order they started, each with a button to replay its reminder while the challenge isn't done
namespace openblack::temple::challenge_room
{

/// How many pictures the room's walls have room for
inline constexpr uint32_t k_Places = 21;
/// How far in front of a picture the camera looks at it from, along the picture's frame
inline constexpr float k_LookDistance = 25.0f;

/// The parts of the room's mesh that show a challenge
enum class Part : uint8_t
{
	/// The challenge's picture, which a click looks at closer and, once close, replays
	Picture,
	/// The frame round every picture
	Frame,
	/// The silver frame of a challenge, or the gold one of a quest
	SilverFrame,
	GoldFrame,
	/// The button under a challenge that isn't done, which replays its reminder
	ReplayButton,
};

/// A part of the room's mesh and the place of the challenge it shows, from 0
struct Control
{
	Part part;
	uint32_t place;

	bool operator==(const Control&) const = default;
};

/// The part a submesh of the room's mesh is, by its name ("LH_Picture_Challenge_01" is the first picture), if any
[[nodiscard]] std::optional<Control> ParseControl(std::string_view subMeshName);

/// What the room shows of a challenge
struct Challenge
{
	bool done;
	bool quest;
};

/// Whether a part is drawn, for the challenges in the order they started: the picture and frames of each challenge,
/// its silver or gold frame, and its button while it isn't done
[[nodiscard]] bool IsShown(Control control, std::span<const Challenge> challenges);

/// What a click on a part does
enum class Click : uint8_t
{
	Nothing,
	/// The camera goes to look at the picture from close by, and the picture is shown in high detail
	LookCloser,
	/// The screen fades to white and the player leaves the temple to see the challenge's reminder again
	Replay,
};
/// A click on a part, with the camera wholly close to a picture or not. Close up, any picture clicked replays its
/// challenge, as its button does, unless the challenge is done.
[[nodiscard]] Click Clicked(Control control, bool closeUp, std::span<const Challenge> challenges);

/// How long a challenge took, or has taken so far, as the room's close look tells it
struct Duration
{
	uint32_t hours;
	uint32_t minutes;
	uint32_t seconds;
};
/// From the game turn the challenge started, the one it was done on if it is, the game turn now and the length of a
/// turn in milliseconds
[[nodiscard]] Duration TimeTaken(uint32_t startedTurn, std::optional<uint32_t> doneTurn, uint32_t turnNow,
                                 uint32_t millisecondsPerTurn);

/// The second line of the room's close look at a picture: "Time taken" and the time, two spaces, "Success" and the
/// percentage, its fraction dropped
[[nodiscard]] std::u16string DetailsLine(std::u16string_view timeTaken, Duration duration, std::u16string_view success,
                                         float successShare);

/// The close look's two lines are drawn in the font's 30 pixel size, 10 pixels down from the top of the screen and
/// centred on it, in black, and again in yellow two pixels right and down
inline constexpr float k_TextSize = 30.0f;
inline constexpr float k_TextTop = 10.0f;
inline constexpr float k_ShadowOffset = 2.0f;

/// How much the close look's words show, from 0 to 255, as the camera comes close to the picture, from 0 to 1
[[nodiscard]] uint8_t TextAlpha(float closeness);

} // namespace openblack::temple::challenge_room
