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

#include <algorithm>
#include <optional>
#include <string>

/// The silence of the game server, as a client of a network game sees it (deviation row of the user's request of
/// 2026-11-19). While the client waits for the turns of the server and no message came for a short time, the hand, the
/// camera and the inputs that send commands stop, and red text at the top left shows the seconds of the silence. When
/// a message comes again, both stop at once. This is presentation and input only: no value goes into the synced world.
/// Pure, with no global state, tested with a fake clock.
namespace openblack::online::server_silence
{

/// The time of one turn of the server at the normal speed of the clock of the room
inline constexpr int64_t k_TurnMicroseconds = 100'000;

/// The silence shows after the time of this many turns. The server sends a turn each 100 ms, so in a normal game a
/// message comes each 100 ms. Four turns (400 ms) is longer than one resend of a lost packet of TCP (at least 200 ms
/// and the round trip), so a single lost packet does not show the text. It is also shorter than the time that a
/// player sees as a stop of the world.
inline constexpr int64_t k_TurnsBeforeShow = 4;

/// The silence after which the client closes the socket and tries to come back, also when the socket stays open. This
/// is the limit of the socket of the desktop, so the browser has the same limit. The client sends a ping each second, so
/// a server that runs answers in this time also while the clock of the room is paused.
inline constexpr int64_t k_LimitMicroseconds = 50'000'000;

/// The silence that shows the text, for the time of a turn of the clock of the room. A slow clock has longer turns, so
/// the silence must be longer. A fast clock keeps the time of four normal turns.
/// @param speed The time of a turn as a multiple of a tenth of a second (1 is normal)
[[nodiscard]] constexpr int64_t ShowAfter(float speed) noexcept
{
	const auto turn =
	    static_cast<int64_t>(static_cast<double>(k_TurnMicroseconds) * static_cast<double>(std::max(speed, 1.0f)));
	return k_TurnsBeforeShow * turn;
}

/// What the watch reads in each frame, after the poll of the socket
struct Facts
{
	/// Microseconds of a steady clock of this computer
	int64_t now {0};
	/// True while the client waits for the turns of the server: the game plays (or the client tries to come back), the
	/// clock of the room is not paused, no wait for the loads, and the server did not stop the turns
	bool waits {false};
	/// The time of the last message from the server, by the same clock. None before the first message.
	std::optional<int64_t> heardAt;
	/// The time of a turn as a multiple of a tenth of a second (1 is normal)
	float speed {1.0f};
};

/// The watch of the silence of the server. The silence counts from the last message, but not from a time before the
/// client started to wait for the turns. Thus a pause of the clock of the room, which sends no turns, does not show the
/// text when the pause ends.
class Watch
{
public:
	/// Once a frame. Gives true when the text starts or stops in this frame.
	bool Update(const Facts& facts) noexcept
	{
		const bool shownBefore = _shown.has_value();
		if (!facts.waits)
		{
			_waitsSince.reset();
			_silence = 0;
			_shown.reset();
			return shownBefore;
		}
		if (!_waitsSince.has_value())
		{
			_waitsSince = facts.now;
		}
		const auto from = facts.heardAt.has_value() ? std::max(*facts.heardAt, *_waitsSince) : *_waitsSince;
		_silence = std::max<int64_t>(facts.now - from, 0);
		if (_silence > ShowAfter(facts.speed))
		{
			_shown = _silence;
		}
		else
		{
			_shown.reset();
		}
		return shownBefore != _shown.has_value();
	}

	/// The microseconds of the silence while the text shows. None when it does not show.
	[[nodiscard]] std::optional<int64_t> Shown() const noexcept { return _shown; }
	/// The microseconds of the silence, also when it is too short to show. 0 when the client does not wait.
	[[nodiscard]] int64_t Silence() const noexcept { return _silence; }

private:
	std::optional<int64_t> _waitsSince;
	int64_t _silence {0};
	std::optional<int64_t> _shown;
};

/// The text of a silence: the seconds with one decimal and the letter s, such as "24.3s". The tenths are cut off, not
/// rounded, so the text never shows a time that did not yet go.
[[nodiscard]] std::u16string Text(int64_t microseconds);

/// What the hand of the player gets from the mouse
enum class HandEvent : uint8_t
{
	/// A button of the mouse goes down
	Press,
	/// A button of the mouse comes up
	Release,
	/// The mouse moves
	Move,
	Other,
};

/// What the game does with an event of the hand
enum class Admission : uint8_t
{
	/// The hand gets the event now
	Pass,
	/// The event does not go to the hand
	Drop,
	/// The hand gets the event when the silence ends
	Hold,
};

/// During a silence the hand gets no event. A press and a move do not go, so the hand does not move and sends no command.
/// A release waits until the silence ends, so that a button that went down before the silence does not stay down in
/// the hand.
[[nodiscard]] constexpr Admission Admit(HandEvent event, bool silent) noexcept
{
	if (!silent)
	{
		return Admission::Pass;
	}
	return event == HandEvent::Release ? Admission::Hold : Admission::Drop;
}

} // namespace openblack::online::server_silence
