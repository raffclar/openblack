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

#include <chrono>
#include <span>
#include <string_view>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "Gui/ScriptTexts.h"

namespace openblack::ecs::systems
{

/// What the scripts do to the picture for their cut scenes: fading it to a colour and back, and the cinema bars
class CinematicDirectorSystemInterface
{
public:
	virtual ~CinematicDirectorSystemInterface() = default;

	/// Fades the picture from clear to a colour over whole seconds, at once for none
	virtual void FadeTo(uint8_t red, uint8_t green, uint8_t blue, int8_t seconds) = 0;
	/// Fades the picture from the colour back to clear over whole seconds, at once for none
	virtual void FadeBackToNormal(int8_t seconds) = 0;
	/// As the story's scripts start a new game: the picture is black from the first frame until they fade it in
	virtual void StartStory() = 0;
	[[nodiscard]] virtual bool IsFadeFinished() const = 0;
	/// The fade's colour over the picture, 0xAARRGGBB, nothing for an alpha of 0
	[[nodiscard]] virtual uint32_t GetFadeColour() const = 0;

	/// Slides the cinema bars in or out. Bringing them in hides the game's dialogs, and while a script task has them
	/// in, `owner` being its number, the player's interface is put away. 0 is for the game's own bars.
	virtual void SetWideScreen(bool on, uint32_t owner) = 0;
	/// The bars all the way in at once, as a full-screen video brings them; they then slide out as usual
	virtual void SnapWideScreen() = 0;
	[[nodiscard]] virtual bool IsWideScreenOn() const = 0;
	/// The script task that brought the bars in, 0 for none
	[[nodiscard]] virtual uint32_t GetWideScreenOwner() const = 0;
	[[nodiscard]] virtual bool IsWideScreenTransitionFinished() const = 0;
	/// How far in the bars are, 0 for none and 1 for a 16:9 picture
	[[nodiscard]] virtual float GetWideScreenFraction() const = 0;
	/// Whether the player has their interface: the hand and its actions are put away while a script has the bars in
	[[nodiscard]] virtual bool IsInterfaceActive() const = 0;
	/// The player's interface is put away or given back whatever the bars are, as a hand demonstration does
	virtual void SetInterfaceActive(bool active) = 0;
	/// Whether the game's dialogs were told to hide since this was last asked
	[[nodiscard]] virtual bool TakeHideDialogs() = 0;

	/// A script's close clipping: the camera draws things right up to itself, for close shots
	virtual void SetCloseClipping(bool close) = 0;
	[[nodiscard]] virtual bool IsCloseClipping() const = 0;

	/// Writes words across the screen in a box, its corner and size shares of the screen's width and height and its
	/// lines' height a share of the screen's height, in the colour last set, fading in over `fadeSeconds` or shown at
	/// once for none
	virtual void WriteText(std::u16string_view words, glm::vec2 corner, glm::vec2 extent, float size, float fadeSeconds) = 0;
	/// The colour the texts written from now on are drawn in, black until a script sets one; it lasts from land to land
	virtual void SetTextColour(glm::u8vec3 colour) = 0;
	/// Fades every text on the screen out over `seconds`, or takes them away at once for none
	virtual void FadeAllTexts(float seconds) = 0;
	/// Moves the texts' fades on by the game's time of a frame, taking away those that have faded out
	virtual void UpdateTexts(int32_t milliseconds) = 0;
	/// The texts on the screen, the newest first, in the order they are drawn
	[[nodiscard]] virtual std::span<const gui::script_text::ScreenText> GetTexts() const = 0;

	/// As a new land opens: no fade, no bars, no close clipping, and the texts are taken away
	virtual void Reset() = 0;
	/// Moves the fade on by a game turn
	virtual void ProcessTurn() = 0;
	/// Slides the bars on by the game time of the frame, which stops while the game is paused
	virtual void Update(std::chrono::duration<float, std::milli> gameTime) = 0;
};

} // namespace openblack::ecs::systems
