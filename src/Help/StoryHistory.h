/*******************************************************************************
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

#include <array>
#include <optional>
#include <string_view>

namespace openblack::help
{

/// A help text that a script showed, as the story history keeps it
struct StoryLine
{
	/// The number of the help text
	uint32_t text {0};
	/// How the player continues past the text: 0 by the time, 1 by a click, 2 never by a click
	int32_t interaction {0};
	/// The number that the script wrote into the text. The library shows the text without it.
	float number {0.0f};
	/// The narrator of the text, from the help texts
	int32_t narrator {0};

	/// Gives the values that a saved game keeps to a visitor
	template <typename Self, typename Visitor>
	static void VisitSaved(Self& self, Visitor&& visit)
	{
		visit(self.text, self.interaction, self.number, self.narrator);
	}
};

/// The story history: the last 1024 help texts that the scripts showed, which the library's last scroll shows.
/// A new line pushes out the oldest line when the history is full.
class StoryHistory
{
public:
	static constexpr size_t k_Capacity = 1024;

	void Add(const StoryLine& line);
	/// The line some lines back from the newest line, 0 for the newest line. None past the oldest line.
	[[nodiscard]] std::optional<StoryLine> Back(int32_t linesBack) const;
	[[nodiscard]] size_t Count() const noexcept { return _count; }
	void Clear();

private:
	std::array<StoryLine, k_Capacity> _lines {};
	/// Where the next line goes
	size_t _next {0};
	size_t _count {0};

public:
	/// Gives the values that a saved game keeps to a visitor
	template <typename Self, typename Visitor>
	static void VisitSaved(Self& self, Visitor&& visit)
	{
		visit(self._lines, self._next, self._count);
	}
};

/// The buttons of the story history in the library turn the first line that it shows
enum class StoryTurn
{
	/// One line further back
	Previous,
	/// One line nearer to the newest line
	Next,
	/// Back to the newest line
	Latest,
};

/// The text that comes before a line in the library: who said it, by the narrator of the line
[[nodiscard]] std::string_view StoryCaption(int32_t narrator);

/// The first line that the library shows, kept from 0 (the newest line) to the oldest line
[[nodiscard]] int32_t ClampStoryStart(int32_t start, size_t count);

} // namespace openblack::help
