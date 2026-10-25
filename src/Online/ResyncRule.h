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

namespace openblack::online
{

/// What a client does when the checksum of its world parts from the server's (user decisions MP-Q11 and MI-4): it shows
/// "Resynchronising..." and joins again quickly, up to 3 times. When the third rejoin does not put it back in step, the
/// client leaves the game and shows the out-of-sync message.
class ResyncRule
{
public:
	static constexpr uint32_t k_MostTries = 3;

	enum class State : uint8_t
	{
		InStep,
		/// A rejoin is in progress, or the client waits for the first checksum after it
		Resyncing,
		/// The tries did not work. The client leaves the game.
		OutOfSync,
	};

	enum class Action : uint8_t
	{
		None,
		/// Join the server again
		Rejoin,
		/// Leave the game and show the out-of-sync message
		GiveUp,
	};

	/// The checksums of a turn are not the same. Gives what the client does now.
	[[nodiscard]] Action Differ(uint32_t turn)
	{
		if (_state == State::OutOfSync)
		{
			return Action::None;
		}
		// While a rejoin is in progress, only the first compare after it counts
		if (_state == State::Resyncing && !_rejoined)
		{
			return Action::None;
		}
		if (_tries >= k_MostTries)
		{
			_state = State::OutOfSync;
			return Action::GiveUp;
		}
		++_tries;
		_state = State::Resyncing;
		_rejoined = false;
		_firstTurn = _firstTurn.value_or(turn);
		return Action::Rejoin;
	}

	/// The rejoin finished: the next compare tells whether it worked
	void Rejoined() { _rejoined = true; }

	/// The checksums of a turn are the same
	void Same()
	{
		if (_state == State::Resyncing && _rejoined)
		{
			_state = State::InStep;
			_tries = 0;
			_firstTurn.reset();
		}
	}

	[[nodiscard]] State GetState() const { return _state; }
	[[nodiscard]] uint32_t Tries() const { return _tries; }
	/// The first turn at which the worlds parted, while the client is not in step
	[[nodiscard]] std::optional<uint32_t> FirstTurn() const { return _firstTurn; }

private:
	State _state {State::InStep};
	uint32_t _tries {0};
	bool _rejoined {false};
	std::optional<uint32_t> _firstTurn;
};

} // namespace openblack::online
