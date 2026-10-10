/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "HandDemo.h"

#include <algorithm>

namespace openblack::hand_demo
{

namespace
{
constexpr uint32_t k_HundredthsPerTurn = 100;
/// The height of a 16:9 picture across the screen's width
constexpr float k_WideScreenShape = 0.5625f;
} // namespace

uint32_t GameHundredths(uint32_t turn, float turnFraction)
{
	const auto into =
	    std::min(static_cast<uint32_t>(turnFraction * static_cast<float>(k_HundredthsPerTurn)), k_HundredthsPerTurn - 1);
	return (turn * k_HundredthsPerTurn) + into;
}

glm::ivec2 CursorPixel(std::span<const float, 2> cursor, glm::ivec2 screen, float wideScreenFraction)
{
	const auto width = static_cast<float>(screen.x);
	const auto height = static_cast<float>(screen.y);
	const auto bar = static_cast<int>((height - (k_WideScreenShape * width)) * wideScreenFraction) / 2;
	const auto barF = static_cast<float>(bar);
	return {static_cast<int>((width * cursor[0]) + 0.5f),
	        static_cast<int>(((height - (2.0f * barF)) * cursor[1]) + barF + 0.5f)};
}

Playback::Playback(std::span<const hnd::HNDRecord> records, uint32_t gameTime, std::vector<Played>& played)
    : _records(records)
{
	if (_records.empty())
	{
		return;
	}
	// The first record plays at once, and the time starts from it
	bool triggerReached = false;
	Play(1, triggerReached, played);
	_firstRecordTime = _records.front().time;
	_startTime = gameTime;
}

void Playback::Play(size_t recordsRead, bool& triggerReached, std::vector<Played>& played)
{
	const auto& record = _records[_next];
	// No camera hints at either end of the recording, counted from the first record read in this go
	const bool nearAnEnd = recordsRead < k_HintlessRecords || _records.size() - recordsRead < k_HintlessRecords;
	played.push_back({.record = &record, .hints = nearAnEnd ? 0u : record.hints});
	if (record.trigger != 0)
	{
		triggerReached = true;
	}
	++_next;
}

bool Playback::Advance(uint32_t gameTime, bool pauseOnTrigger, bool& triggerReached, std::vector<Played>& played)
{
	if (Ended())
	{
		return false;
	}
	const size_t recordsRead = _next + 1;
	while (!Ended())
	{
		const auto& record = _records[_next];
		const auto due = static_cast<int64_t>(record.time) - _firstRecordTime;
		const auto now = static_cast<int64_t>(gameTime) - _startTime;
		if (due > now)
		{
			return true;
		}
		if (pauseOnTrigger && triggerReached)
		{
			// Held: the clock is moved on so that this record stays due and the rest keep their pace
			_startTime = _firstRecordTime - static_cast<int64_t>(record.time) + static_cast<int64_t>(gameTime);
			return true;
		}
		Play(recordsRead, triggerReached, played);
	}
	return false;
}

} // namespace openblack::hand_demo
