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
#include <array>
#include <optional>

#include <bgfx/bgfx.h>

#include "Visual/DeviceRestoreRequest.h"

/// The recovery from a lost graphics device. The graphics library makes the device again with all the textures,
/// buffers, shaders and render targets that it has, and each one keeps its handle. Only the pictures that the GPU drew
/// into render targets are gone, thus the renderer draws again the pictures that it keeps from one frame to the next.
namespace openblack::graphics::device_restore
{

/// Whether the game continues after a fatal error of the graphics library. It continues only after a lost device,
/// because the library makes the device again. All other fatal errors stop the game.
[[nodiscard]] constexpr bool ContinuesAfter(bgfx::Fatal::Enum code) noexcept
{
	return code == bgfx::Fatal::DeviceLost;
}

/// Finds each new restore of the device one time, from the count of restores that the graphics library gives
class Watch
{
public:
	/// True one time for each change of the count
	[[nodiscard]] constexpr bool Saw(uint32_t count) noexcept
	{
		if (count == _seen)
		{
			return false;
		}
		_seen = count;
		return true;
	}

private:
	uint32_t _seen {0};
};

/// The longest time that the renderer can wait for a new device after a loss
inline constexpr uint32_t k_LongestWaitMs = 10'000;
/// The number of restores in a short time after which the recovery stops: each new device is lost again at once
inline constexpr uint32_t k_RestoresInBurst = 3;
/// The time in which k_RestoresInBurst restores are a burst
inline constexpr uint32_t k_BurstMs = 30'000;

enum class Recovery : uint8_t
{
	Continues,
	Failed,
};

/// Finds a recovery from lost devices that does not succeed. The recovery fails when the renderer waits too long for a
/// new device, or when each new device is lost again soon after the restore.
class RecoveryWatch
{
public:
	/// Give the state of the renderer at each frame.
	/// - waiting: the renderer has no device and waits to make one again
	/// - restores: the count of restores that the graphics library gives
	/// - nowMs: the time of the frame, in milliseconds
	[[nodiscard]] constexpr Recovery Step(bool waiting, uint32_t restores, uint32_t nowMs) noexcept
	{
		if (!waiting)
		{
			_waitStart.reset();
		}
		else if (!_waitStart.has_value())
		{
			_waitStart = nowMs;
		}
		else if (nowMs - *_waitStart >= k_LongestWaitMs)
		{
			return Recovery::Failed;
		}

		if (restores != _restoresSeen)
		{
			const uint32_t newRestores = restores - _restoresSeen;
			_restoresSeen = restores;
			for (uint32_t i = 0; i < std::min(newRestores, k_RestoresInBurst); ++i)
			{
				_restoreTimes[_nextTime] = nowMs;
				_nextTime = (_nextTime + 1) % k_RestoresInBurst;
				_timesKept = std::min(_timesKept + 1, k_RestoresInBurst);
			}
			// The oldest kept time is the next slot to write
			if (_timesKept == k_RestoresInBurst && nowMs - _restoreTimes[_nextTime] < k_BurstMs)
			{
				return Recovery::Failed;
			}
		}
		return Recovery::Continues;
	}

private:
	std::optional<uint32_t> _waitStart;
	uint32_t _restoresSeen {0};
	std::array<uint32_t, k_RestoresInBurst> _restoreTimes {};
	uint32_t _nextTime {0};
	uint32_t _timesKept {0};
};

} // namespace openblack::graphics::device_restore
