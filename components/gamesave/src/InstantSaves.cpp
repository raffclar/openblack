/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "InstantSaves.h"

#include <cstddef>

std::u16string openblack::gamesave::InstantSaveName(std::u16string_view text, uint32_t land)
{
	std::u16string name;
	bool numberWritten = false;
	for (size_t i = 0; i < text.size(); ++i)
	{
		if (text[i] != u'%' || i + 1 >= text.size())
		{
			name += text[i];
			continue;
		}
		if (text[i + 1] == u'%')
		{
			name += u'%';
			++i;
			continue;
		}
		// A number with a width and no figures after the point: "%3.0f"
		size_t at = i + 1;
		size_t width = 0;
		while (at < text.size() && text[at] >= u'0' && text[at] <= u'9')
		{
			width = (width * 10) + static_cast<size_t>(text[at] - u'0');
			++at;
		}
		if (numberWritten || at + 2 >= text.size() || text[at] != u'.' || text[at + 1] != u'0' || text[at + 2] != u'f')
		{
			name += text[i];
			continue;
		}
		const auto digits = std::to_string(land);
		if (digits.size() < width)
		{
			name.append(width - digits.size(), u' ');
		}
		for (const char digit : digits)
		{
			name += static_cast<char16_t>(digit);
		}
		numberWritten = true;
		i = at + 2;
	}
	return name;
}
