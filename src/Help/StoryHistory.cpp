/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "StoryHistory.h"

#include <algorithm>

namespace openblack::help
{

void StoryHistory::Add(const StoryLine& line)
{
	_lines.at(_next) = line;
	_next = (_next + 1) % k_Capacity;
	_count = std::min(_count + 1, k_Capacity);
}

std::optional<StoryLine> StoryHistory::Back(int32_t linesBack) const
{
	if (linesBack < 0 || static_cast<size_t>(linesBack) >= _count)
	{
		return std::nullopt;
	}
	// The newest line is the line before the next free place
	const auto index = (_next + k_Capacity - 1 - static_cast<size_t>(linesBack)) % k_Capacity;
	return _lines.at(index);
}

void StoryHistory::Clear()
{
	_next = 0;
	_count = 0;
}

std::string_view StoryCaption(int32_t narrator)
{
	switch (narrator)
	{
	case 2: // The good advisor
		return "HELP_TEXT_DIALOG_ADDITION_131";
	case 3: // The evil advisor
		return "HELP_TEXT_DIALOG_ADDITION_128";
	case 4: // A woman
		return "HELP_TEXT_DIALOG_ADDITION_18";
	case 5: // A man
		return "HELP_TEXT_DIALOG_ADDITION_17";
	case 6: // The ogre
		return "HELP_TEXT_DIALOG_ADDITION_133";
	case 7: // Khazar
		return "HELP_TEXT_DIALOG_ADDITION_79";
	case 8: // Lethys
		return "HELP_TEXT_DIALOG_ADDITION_82";
	case 9: // Nemesis
		return "HELP_TEXT_DIALOG_ADDITION_95";
	case 10: // A boy
		return "HELP_TEXT_DIALOG_ADDITION_16";
	case 11: // The big voice
		return "HELP_TEXT_DIALOG_ADDITION_15";
	case 12: // The trainer
		return "HELP_TEXT_DIALOG_ADDITION_134";
	default: // All the other narrators: "This was said:"
		return "HELP_TEXT_DIALOG_ADDITION_135";
	}
}

int32_t ClampStoryStart(int32_t start, size_t count)
{
	// The history has no line before the oldest line. An empty history starts at the newest line.
	const auto last = static_cast<int32_t>(count) - 1;
	return std::max(std::min(start, last), 0);
}

} // namespace openblack::help
