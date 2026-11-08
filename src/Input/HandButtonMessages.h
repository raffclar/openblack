/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <algorithm>
#include <array>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace openblack::input::hand_messages
{

/// The two buttons of the hand, in the order that the game sends their messages: Action (the right button), then Move
/// (the left button)
enum class Button : uint8_t
{
	Action,
	Move,
};

inline constexpr size_t k_ButtonCount = 2;

/// A message of a button of the hand
enum class Message : uint8_t
{
	Double,
	Down,
	Up,
};

struct Sent
{
	Button button;
	Message message;

	[[nodiscard]] constexpr bool operator==(const Sent&) const noexcept = default;
};

/// A mouse input of the frame that changes the buttons of the hand
struct Input
{
	enum class Kind : uint8_t
	{
		Press,
		DoublePress,
		LetGo,
		/// A move of the pointer. It tells which buttons are down.
		Motion,
	};

	Kind kind;
	Button button {Button::Action};
	/// For a motion: the buttons that are down
	bool actionDown {false};
	bool moveDown {false};
};

/// A one-press action of the mouse that acts in this frame. The game sends no message of the hand's buttons in this frame.
struct OnePressAction
{
	/// The button of the hand whose press the action takes. The middle button and the wheel take no press of the hand.
	std::optional<Button> button;
};

/// Two presses of a button that the game finds from the held button, closer than this time, make a double click
inline constexpr uint32_t k_HeldDoubleClickMilliseconds = 250;

/// The game's rule for the messages of the hand's buttons. The game keeps each button's press, let go and double click
/// as one mark each, and sends the messages once a frame, after the moves of the pointer.
/// - While Alt is held, the game sends no message. The marks stay.
/// - When a one-press action of the mouse acts, the game sends no message in that frame. The action takes the mark of
///   its press. The other marks stay for the next frame.
/// - On the first frame without Alt, the game sends the messages from the marks. For each button it sends the double
///   click, the press and the let go, in this sequence. The Action button comes first.
/// - A press removes the let go mark of its button. A double click does not.
/// - A button that is down when the hand thinks that it is up sends its press on the next frame. Two such presses in
///   less time than k_HeldDoubleClickMilliseconds also send a double click on the frame after the second press.
/// While no message waits, each press and let go goes to the hand as it comes.
class HandButtonMessages
{
public:
	/// True when the messages of this frame come from the marks: Alt is held, or a message waits from an earlier frame
	[[nodiscard]] constexpr bool Waits(bool altHeld) const noexcept
	{
		return altHeld || std::ranges::any_of(_buttons, [](const State& state) {
			       return state.marks.Any() || state.doublePending || state.handDown != state.down;
		       });
	}

	/// True when the hand thinks that the button is down
	[[nodiscard]] constexpr bool HandDown(Button button) const noexcept { return At(button).handDown; }

	/// The inputs of a frame, after all the moves of the pointer. It gives no value when the inputs go to the hand as
	/// they come. Else it gives the messages to send at the end of the frame. There are no messages while Alt is held,
	/// and none in a frame where a one-press action of the mouse acts.
	[[nodiscard]] std::optional<std::vector<Sent>> Frame(std::span<const Input> inputs, bool altHeld, uint32_t milliseconds,
	                                                     std::optional<OnePressAction> onePress = std::nullopt)
	{
		if (!onePress.has_value() && !Waits(altHeld))
		{
			for (const auto& input : inputs)
			{
				Follow(input);
			}
			return std::nullopt;
		}
		for (const auto& input : inputs)
		{
			Mark(input);
		}
		if (altHeld)
		{
			return std::vector<Sent> {};
		}
		if (onePress.has_value())
		{
			if (onePress->button.has_value())
			{
				At(*onePress->button).marks.press = false;
			}
			return std::vector<Sent> {};
		}
		std::vector<Sent> sent;
		for (const auto button : {Button::Action, Button::Move})
		{
			Send(button, milliseconds, sent);
		}
		return sent;
	}

private:
	struct Marks
	{
		bool press {false};
		bool letGo {false};
		bool doublePress {false};

		[[nodiscard]] constexpr bool Any() const noexcept { return press || letGo || doublePress; }
	};

	struct State
	{
		/// The button is down on the mouse
		bool down {false};
		/// The hand got the press of the button and not its let go
		bool handDown {false};
		/// The hand got the press from a mark. Else the press came from the held button, and the let go comes when the
		/// button is up.
		bool pressFromMark {true};
		Marks marks;
		bool doublePending {false};
		/// The time of the last press that came from the held button. It has no value before the first such press.
		std::optional<uint32_t> heldPressTime;
	};

	[[nodiscard]] constexpr State& At(Button button) noexcept { return _buttons.at(static_cast<size_t>(button)); }
	[[nodiscard]] constexpr const State& At(Button button) const noexcept { return _buttons.at(static_cast<size_t>(button)); }

	/// An input that goes to the hand as it comes
	constexpr void Follow(const Input& input) noexcept
	{
		if (input.kind == Input::Kind::Motion)
		{
			At(Button::Action).down = At(Button::Action).handDown = input.actionDown;
			At(Button::Move).down = At(Button::Move).handDown = input.moveDown;
			return;
		}
		auto& state = At(input.button);
		state.down = state.handDown = input.kind != Input::Kind::LetGo;
		state.pressFromMark = true;
	}

	/// An input that the game keeps as a mark until it sends the messages
	constexpr void Mark(const Input& input) noexcept
	{
		if (input.kind == Input::Kind::Motion)
		{
			At(Button::Action).down = input.actionDown;
			At(Button::Move).down = input.moveDown;
			return;
		}
		auto& state = At(input.button);
		switch (input.kind)
		{
		case Input::Kind::Press:
			state.marks.press = true;
			state.marks.letGo = false;
			state.down = true;
			break;
		case Input::Kind::DoublePress:
			state.marks.doublePress = true;
			state.down = true;
			break;
		case Input::Kind::LetGo:
			state.marks.letGo = true;
			state.down = false;
			break;
		case Input::Kind::Motion:
			break;
		}
	}

	/// The messages of one button from its marks. The marks go after it.
	void Send(Button button, uint32_t milliseconds, std::vector<Sent>& sent)
	{
		auto& state = At(button);
		const auto marks = std::exchange(state.marks, {});
		// The double click
		if (std::exchange(state.doublePending, false))
		{
			sent.push_back({button, Message::Double});
		}
		else if (marks.doublePress)
		{
			state.handDown = true;
			sent.push_back({button, Message::Double});
		}
		// The press, from its mark, or from the held button
		if (!state.handDown && marks.press)
		{
			state.handDown = true;
			state.pressFromMark = true;
			sent.push_back({button, Message::Down});
		}
		else if (!state.handDown && state.down)
		{
			if (state.heldPressTime.has_value() && milliseconds - *state.heldPressTime < k_HeldDoubleClickMilliseconds)
			{
				state.doublePending = true;
			}
			state.heldPressTime = milliseconds;
			state.handDown = true;
			state.pressFromMark = false;
			sent.push_back({button, Message::Down});
		}
		// The let go, from its mark after a press from a mark, else from the button that is up
		if (state.handDown && (state.pressFromMark ? marks.letGo : !state.down))
		{
			state.handDown = false;
			sent.push_back({button, Message::Up});
		}
	}

	std::array<State, k_ButtonCount> _buttons {};
};

} // namespace openblack::input::hand_messages
