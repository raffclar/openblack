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

#include <glm/vec2.hpp>

#include "Help/HelpTextDisplay.h"

namespace openblack::gui
{
class TextDatabase;
}

namespace openblack::help
{
class DialogueText;
class AdvisorVoices;
} // namespace openblack::help

namespace openblack::ecs::systems
{

/// The dialogue the scripts show and have spoken: the help texts at the bottom of the screen, the advisors' and the
/// narrators' voices saying them, and the player clicking them on
class HelpTextSystemInterface
{
public:
	/// What a frame gives the dialogue
	struct Frame
	{
		/// The frame's game time and real time, in milliseconds
		uint32_t gameMs {0};
		uint32_t realMs {0};
		/// Inside the temple the texts go by real time
		bool inTemple {false};
		/// The player clicked this frame
		bool click {false};
		/// The key that cuts a text short is held: the keypad's Enter
		bool skipKey {false};
	};

	virtual ~HelpTextSystemInterface() = default;

	/// Starts the dialogue with the game's help texts, for a screen of a height, which sets the size of the text
	virtual void Start(const gui::TextDatabase& texts, int screenHeight) = 0;
	[[nodiscard]] virtual bool IsStarted() const = 0;
	/// No texts and no voices, as a new land opens
	virtual void Reset() = 0;

	/// Once a frame: the voices start and stop, the player's click is taken and the newest text slides in
	virtual void Update(const Frame& frame) = 0;

	/// A script shows a help text and has it said; false for an invalid text, which shows the first
	virtual bool RunText(bool singleLine, uint32_t text, int32_t withInteraction) = 0;
	/// The text the scripts last showed has been read
	[[nodiscard]] virtual bool IsTextRead() const = 0;
	/// Every text is taken away
	virtual void ClearAllText() = 0;
	/// The texts are taken away and their box closed
	virtual void CloseDialogue() = 0;
	/// Both advisors are cut short in what they say
	virtual void InterruptAdvisors() = 0;
	/// Who says a help text, a narrator of the help texts' script; a number past the last gives the first text's
	[[nodiscard]] virtual int32_t GetNarrator(uint32_t text) const = 0;

	/// The box and words to draw this frame for a screen with cinema bars of `barPixels`, nothing before the start
	[[nodiscard]] virtual const help::TextFrame* Layout(glm::ivec2 screen, int barPixels,
	                                                    const help::WidthFn& widthFn) const = 0;

	/// The advisors' voices
	[[nodiscard]] virtual help::AdvisorVoices& GetVoices() = 0;
	[[nodiscard]] virtual const help::AdvisorVoices& GetVoices() const = 0;
	/// The texts shown, nothing before the start
	[[nodiscard]] virtual const help::DialogueText* GetDialogue() const = 0;
};

} // namespace openblack::ecs::systems
