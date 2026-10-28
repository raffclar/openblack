/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <functional>
#include <optional>
#include <string>
#include <vector>

#include <glm/vec2.hpp>

#include "Common/Zoomer.h"
#include "Dialog.h"

namespace openblack::gui
{

class StaticText;

/// The message that a setup box shows in front of it, with one OK button.
///
/// The message is an opaque box of 500 by 200 in the middle of the dialog space. The box is tinted white, so its fill
/// is the grey of the opaque cell of the front end atlas. Its text is in the top part of the box, broken to fit. The
/// text starts at the big size and gets smaller while it is 125 or more high, but not smaller than the small size. The
/// OK button is below the text, at the middle size.
///
/// While the message shows, the box behind it holds: it gets no input, and over half a second it dims to a quarter.
/// OK, Return or Escape closes the message. Then the box behind is back at once.
///
/// The two opacities are whole steps of 1/255, cut down, as the game draws them.
class SetupMessage
{
public:
	static constexpr DialogRect k_Box {.min = {150, 200}, .max = {650, 400}};
	static constexpr DialogRect k_Text {.min = {200, 220}, .max = {600, 350}};
	static constexpr DialogRect k_Ok {.min = {325, 338}, .max = {475, 370}};
	/// The time in which the box behind dims
	static constexpr float k_FadeSeconds = 0.5f;
	/// How much the box behind dims when the message shows fully
	static constexpr float k_Dims = 0.75f;

	SetupMessage();
	~SetupMessage();
	SetupMessage(const SetupMessage&) = delete;
	SetupMessage& operator=(const SetupMessage&) = delete;
	SetupMessage(SetupMessage&&) = delete;
	SetupMessage& operator=(SetupMessage&&) = delete;

	/// Shows the message with this text and this label on its button
	void Show(std::u16string text, std::u16string okLabel);
	/// Removes the message at once, with no answer
	void Close();
	[[nodiscard]] bool IsShown() const noexcept { return _dialog.has_value(); }

	/// The opacity of the box behind the message, from the opacity that the box has of its own
	[[nodiscard]] float BehindAlpha(float boxAlpha) const;
	/// The opacity of the message
	[[nodiscard]] float MessageAlpha() const;

	/// The opacity of a box behind a message in 255ths: (box - message x 0.75) x 255, with no fraction, from 0 to 255
	[[nodiscard]] static int BehindSteps(float boxAlpha, float messageAlpha) noexcept;
	/// The opacity of a message in 255ths: its fade x 255, with no fraction, from 0 to 255
	[[nodiscard]] static int MessageSteps(float messageAlpha) noexcept;

	// Input at points of the dialog space, while the message shows
	void MouseMove(glm::ivec2 point);
	void MouseDown(glm::ivec2 point);
	/// True when the OK button acted. The message then closes and onAnswer runs.
	bool MouseUp(glm::ivec2 point);
	/// Return and Escape press OK. True when the key closed the message.
	bool KeyDown(int key);
	/// Whether the mouse button is down on the OK button
	[[nodiscard]] bool IsHolding() const noexcept { return _dialog.has_value() && _dialog->IsHolding(); }

	void Update(float deltaSeconds);
	/// The message in front of all of the box. Inactive, nothing lights up.
	void Draw(const DialogPainter& painter, bool active) const;

	/// The OK button, for tools that press it by name
	[[nodiscard]] std::vector<Dialog::NamedControl> GetNamedControls() const;

	/// Runs when the player closes the message with OK, Return or Escape
	std::function<void()> onAnswer;

private:
	void Answer();

	std::optional<Dialog> _dialog;
	StaticText* _text {nullptr};
	/// Set by the OK button. The message closes after its dialog has finished with the click.
	bool _answered {false};
	Zoomer _fade;
};

} // namespace openblack::gui
