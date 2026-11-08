/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <concepts>
#include <cstdint>

#include <chrono>

namespace openblack
{

/// A sleep of a small number of microseconds that keeps its time correct over many calls, as the game's own sleep does.
/// The system sleeps only in whole milliseconds and often sleeps too long. Thus each call adds its time to a debt. The
/// sleep starts only when the debt minus the last oversleep is more than a millisecond. The next calls subtract the
/// time that the system slept too long.
class SleepDebt
{
public:
	/// Sleeps `time` as a part of the debt. `sleep` sleeps a number of milliseconds and gives the microseconds that it
	/// slept.
	template <typename Sleeper>
	    requires std::invocable<Sleeper&, std::chrono::milliseconds>
	void Sleep(std::chrono::microseconds time, Sleeper&& sleep)
	{
		const auto debt = _debt + static_cast<int32_t>(time.count());
		const auto due = debt - _oversleep;
		if (due <= k_Microseconds)
		{
			// The oversleep stays for the next call
			_debt = debt;
			return;
		}
		const auto slept = static_cast<int32_t>(sleep(std::chrono::milliseconds(due / k_Microseconds)).count());
		_oversleep = slept - due;
		_debt = due % k_Microseconds;
	}

	[[nodiscard]] int32_t Debt() const noexcept { return _debt; }
	[[nodiscard]] int32_t Oversleep() const noexcept { return _oversleep; }

private:
	static constexpr int32_t k_Microseconds = 1000;

	/// The microseconds asked for and not slept
	int32_t _debt {0};
	/// The microseconds that the last sleep slept more than it had to
	int32_t _oversleep {0};
};

} // namespace openblack
