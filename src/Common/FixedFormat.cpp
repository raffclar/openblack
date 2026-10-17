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

#include <algorithm>
#include <format>
#include <optional>
#include <span>

namespace openblack::fixed_format
{

namespace
{
/// The digits of a value of no sign with `precision` digits after the point, as the game's C library makes them, with
/// no point and no zeros at the front: the 17 first digits of the value, cut after the precision and then rounded up
/// when the next digit is 5 or more
std::string RoundedDigits(double value, int precision)
{
	constexpr int k_Significant = 17;
	// A value that is not finite is written as zero. The game's texts do not show such a value.
	if (value == 0.0 || !std::isfinite(value))
	{
		return "0";
	}
	const auto text = std::format("{:.16e}", value);
	std::string significant;
	significant.push_back(text[0]);
	significant.append(text, 2, k_Significant - 1);
	const auto exponent = std::stoi(text.substr(text.find('e') + 1));
	const int kept = exponent + 1 + precision;
	if (kept < 0)
	{
		return "0";
	}
	// A place at the front for the carry of the rounding
	std::string digits = "0";
	for (int i = 0; i < kept; ++i)
	{
		digits.push_back(i < k_Significant ? significant[static_cast<size_t>(i)] : '0');
	}
	if (kept < k_Significant && significant[static_cast<size_t>(kept)] >= '5')
	{
		auto at = digits.size() - 1;
		while (digits[at] == '9')
		{
			digits[at] = '0';
			--at;
		}
		++digits[at];
	}
	const auto first = digits.find_first_not_of('0');
	return first == std::string::npos ? "0" : digits.substr(first);
}
} // namespace

std::string Fixed(double value, int width, int precision)
{
	precision = std::clamp(precision, 0, 9);
	// An exact zero has no sign, but a value under zero that rounds to zero keeps its sign
	const bool negative = value < 0.0;
	auto digits = RoundedDigits(std::abs(value), precision);
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
	return WithNumbers(text, std::span<const double>(&value, 1));
}

std::u16string WithNumbers(std::u16string_view text, std::span<const double> values)
{
	std::u16string written;
	written.reserve(text.size() + 8);
	size_t used = 0;
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
		const double value = values.empty() ? 0.0 : values[std::min(used, values.size() - 1)];
		++used;
		for (const char c : Fixed(value, width, precision.value_or(6)))
		{
			written.push_back(static_cast<char16_t>(c));
		}
		i = at;
	}
	return written;
}

} // namespace openblack::fixed_format
