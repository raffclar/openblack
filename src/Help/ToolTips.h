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

// The help system's tooltips: the one tooltip shown next to the hand (submitted, forced, its lifetime per game turn
// inside help::Process, the text builder) and its InputPromptIcon (help::input_prompt). What to submit each turn comes from the
// hand's state (the hand's tooltip functions), registered with SetStateSubmitter. Wiki:
// docs/bw1-notes/hand-and-interface.md, "Tooltips".
namespace openblack::help::tooltips
{

/// The texts this module accepts: 0xE73 (HELP_TEXT_TOOLTIP_01) .. 0xE73 + 0xA9
constexpr uint32_t k_First = 0xE73;
constexpr uint32_t k_Count = 0xAA;

/// What the builder reads from the object for a text (here the caller passes it): up to two numbers in the text's
/// order (0xEF9: the food, then the wood), `outOf` appended as "%s/%d" (0xEFC: the places), and `text`, the text the
/// builder writes instead (0xECC -> 0xEE0 when the believers are negative)
struct Numbers
{
	std::array<double, 2> values {};
	size_t count {0};
	std::optional<int32_t> outOf;
	std::optional<uint32_t> text;
};

/// `action` is the BINDABLE_ACTION whose mouse button or key the icon shows (-1: none); `value` fills the text's
/// number when it has one (the original's builder reads it from the object; here the caller passes it). Submitting
/// the current text keeps it alive this turn.
void Submit(uint32_t text, int32_t action, uint32_t align, bool force, std::optional<float> value = std::nullopt);
/// The same with the builder's numbers (Numbers)
void Submit(uint32_t text, int32_t action, uint32_t align, bool force, const Numbers& numbers);
/// Submit(text, -1, 0, 1), and the value for the text's number
void Force(uint32_t text, float value);

/// The hand's part of the builder (from the interface's hand state): called once per turn before the lifetime, it
/// submits what the hand shows
void SetStateSubmitter(std::function<void()> submitter);

/// TOOLTIP_LEVEL (from the profile, default 2): 0 none, 1 only priorities >= 0.9, 2 all with the fade-out of the low
/// ones, 3 all and no fade-in
void SetLevel(int32_t level);
[[nodiscard]] int32_t Level();

/// Once per game turn, and per temple turn while the game is paused inside the citadel (from help::Process): the
/// builder, then the lifetime and the InputPromptIcon (created, kept or deleted; input_prompt::Frame fades it and
/// Renderer::DrawKeyOrMouse draws it)
void ProcessTurn();
/// Every frame: while paused outside the citadel the InputPromptIcon is deleted
void Frame();

/// The interface reset / a new land (help system reset): no tooltip, timers and show counts kept
void Reset();
/// The middle step of HelpSystem::ResetIcons: only the tooltip's InputPromptIcon is deleted
void ResetIcon();

} // namespace openblack::help::tooltips
