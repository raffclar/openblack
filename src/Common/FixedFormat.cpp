/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "FixedFormat.h"

#include <cmath>
#include <cstdint>

#include <algorithm>
#include <optional>

namespace openblack::fixed_format
{

std::string Fixed(double value, int width, int precision)
{
	precision = std::clamp(precision, 0, 9);
	const bool negative = std::signbit(value);
	double scale = 1.0;
	for (int i = 0; i < precision; ++i)
	{
		scale *= 10.0;
	}
	const auto scaled = static_cast<uint64_t>(std::floor(std::abs(value) * scale + 0.5));
	auto digits = std::to_string(scaled);
	if (precision > 0)
	{
		if (digits.size() <= static_cast<size_t>(precision))
		{
			digits.insert(0, static_cast<size_t>(precision) + 1 - digits.size(), '0');
		}
		digits.insert(digits.size() - static_cast<size_t>(precision), 1, '.');
	}
	if (negative)
	{
		digits.insert(0, 1, '-');
	}
	if (static_cast<int>(digits.size()) < width)
	{
		digits.insert(0, static_cast<size_t>(width) - digits.size(), ' ');
	}
	return digits;
}

std::u16string WithNumber(std::u16string_view text, double value)
{
	std::u16string written;
	written.reserve(text.size() + 8);
	for (size_t i = 0; i < text.size(); ++i)
	{
		if (text[i] != u'%' || i + 1 >= text.size())
		{
			written.push_back(text[i]);
			continue;
		}
		if (text[i + 1] == u'%')
		{
			written.push_back(u'%');
			++i;
			continue;
		}
		// %[width][.digits]f
		size_t at = i + 1;
		int width = 0;
		while (at < text.size() && text[at] >= u'0' && text[at] <= u'9')
		{
			width = width * 10 + (text[at] - u'0');
			++at;
		}
		std::optional<int> precision;
		if (at < text.size() && text[at] == u'.')
		{
			++at;
			precision = 0;
			while (at < text.size() && text[at] >= u'0' && text[at] <= u'9')
			{
				*precision = *precision * 10 + (text[at] - u'0');
				++at;
			}
		}
		if (at >= text.size() || text[at] != u'f')
		{
			written.push_back(text[i]);
			continue;
		}
		for (const char c : Fixed(value, width, precision.value_or(6)))
		{
			written.push_back(static_cast<char16_t>(c));
		}
		i = at;
	}
	return written;
}

} // namespace openblack::fixed_format
