/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VillagerLabels.h"

#include <charconv>
#include <cmath>

#include <algorithm>
#include <ranges>
#include <span>
#include <string>

namespace openblack::gui::villager_labels
{

namespace
{
/// Whole pixels, cut towards zero, as the game turns the box's and the pointer's corners into pixels
float Truncated(float value)
{
	return std::trunc(value);
}

/// A colour channel taken a share (out of 256) of the way to white, rounding down
uint8_t TowardsWhite(uint8_t channel, uint32_t share)
{
	return static_cast<uint8_t>(channel + ((share * (255u - channel)) >> 8u));
}

std::u16string Widen(std::string_view text)
{
	return {text.begin(), text.end()};
}

/// The printf conversions the labels use: %s, %d, %.Nf and %%
std::u16string Format(std::u16string_view format, std::span<const std::u16string> arguments)
{
	std::u16string out;
	size_t next = 0;
	for (size_t i = 0; i < format.size(); ++i)
	{
		const auto c = format[i];
		if (c != u'%' || i + 1 >= format.size())
		{
			out += c;
			continue;
		}
		// Skip the flags, width and precision up to the conversion
		size_t j = i + 1;
		while (j < format.size() && (format[j] == u'.' || (format[j] >= u'0' && format[j] <= u'9')))
		{
			++j;
		}
		if (j >= format.size())
		{
			break;
		}
		if (format[j] == u'%')
		{
			out += u'%';
		}
		else if (next < arguments.size())
		{
			out += arguments[next++];
		}
		i = j;
	}
	return out;
}

/// The decimals a "%.Nf" conversion asks for, 6 when it gives none
int DecimalsOf(std::u16string_view conversion)
{
	const auto dot = conversion.find(u'.');
	if (dot == std::u16string_view::npos)
	{
		return 6;
	}
	int decimals = 0;
	for (size_t i = dot + 1; i < conversion.size() && conversion[i] >= u'0' && conversion[i] <= u'9'; ++i)
	{
		decimals = (decimals * 10) + (conversion[i] - u'0');
	}
	return decimals;
}
} // namespace

float StepNameFade(float fade, bool shown, float milliseconds)
{
	const float target = shown ? 1.0f : 0.0f;
	const float step = milliseconds * k_NameFadePerMillisecond;
	return fade < target ? std::min(fade + step, target) : std::max(fade - step, target);
}

Choice Choose(const Who& who)
{
	// A named villager's speech bubble takes the place of its label
	if (who.named && who.speaking)
	{
		return {};
	}
	if (who.detailsForAll || who.detailsForThis)
	{
		return {.kind = Kind::Details, .size = 1.0f, .withName = who.named && who.nameAllowed};
	}
	if (who.named && who.nameAllowed && who.nameFade > k_SmallestSize)
	{
		return {.kind = Kind::Name, .size = std::min(who.nameFade, 1.0f), .withName = true};
	}
	return {};
}

std::u16string FormatFixed(double value, int decimals)
{
	// The exact decimal expansion of the value, then rounded half away from zero at the decimals asked for
	constexpr int k_ExactDigits = 60;
	std::array<char, 512> buffer {};
	const auto magnitude = std::abs(value);
	const auto [end, error] = std::to_chars(buffer.data(), buffer.data() + buffer.size(), magnitude, std::chars_format::fixed,
	                                        decimals + k_ExactDigits);
	if (error != std::errc())
	{
		return u"";
	}
	std::string digits(buffer.data(), end);
	const auto dot = digits.find('.');
	const bool roundUp = digits[dot + 1 + static_cast<size_t>(decimals)] >= '5';
	digits.resize(decimals > 0 ? dot + 1 + static_cast<size_t>(decimals) : dot);
	if (roundUp)
	{
		// Carry the one up through the digits, past the point
		auto i = static_cast<int>(digits.size()) - 1;
		for (; i >= 0; --i)
		{
			if (digits[static_cast<size_t>(i)] == '.')
			{
				continue;
			}
			if (digits[static_cast<size_t>(i)] == '9')
			{
				digits[static_cast<size_t>(i)] = '0';
				continue;
			}
			++digits[static_cast<size_t>(i)];
			break;
		}
		if (i < 0)
		{
			digits.insert(digits.begin(), '1');
		}
	}
	// The sign is kept even when the number rounds to nothing, as the runtime writes "-0"
	return Widen((std::signbit(value) ? "-" : "") + digits);
}

std::u16string DetailsText(const Details& details, const DetailWords& words)
{
	std::u16string text;
	if (details.name.has_value())
	{
		text += *details.name;
		text += u'\n';
	}
	// Life and food as whole percents of the single precision values
	const auto percent = [](float value) { return FormatFixed(static_cast<double>(value) * 100.0, 0); };
	const std::array<std::u16string, 7> arguments {
	    std::u16string(details.doing), std::u16string(words.age), Widen(std::to_string(details.age)),
	    std::u16string(words.life),    percent(details.life),     std::u16string(words.food),
	    percent(details.food)};
	text += Format(u"%s\n%s%d %s%.0f%% %s%.0f%%", arguments);
	if (details.pregnant)
	{
		text += words.pregnant;
	}
	if (details.poisoned)
	{
		text += words.poisoned;
	}
	return text;
}

std::u16string DebugDetailsText(const Details& details, std::u16string_view format)
{
	std::u16string text;
	if (details.name.has_value())
	{
		text += *details.name;
		text += u'\n';
	}
	text += details.doing;
	text += u'\n';
	// The age, then the life and food as they are, each at the decimals its conversion asks for
	std::vector<std::u16string> arguments;
	const std::array<double, 2> values {static_cast<double>(details.life), static_cast<double>(details.food)};
	size_t nextValue = 0;
	for (size_t i = 0; i < format.size(); ++i)
	{
		if (format[i] != u'%' || i + 1 >= format.size())
		{
			continue;
		}
		size_t j = i + 1;
		while (j < format.size() && (format[j] == u'.' || (format[j] >= u'0' && format[j] <= u'9')))
		{
			++j;
		}
		if (j >= format.size())
		{
			break;
		}
		if (format[j] == u'd')
		{
			arguments.push_back(Widen(std::to_string(details.age)));
		}
		else if (format[j] == u'f' && nextValue < values.size())
		{
			arguments.push_back(FormatFixed(values.at(nextValue++), DecimalsOf(format.substr(i, j - i))));
		}
		i = j;
	}
	text += Format(format, arguments);
	if (details.pregnant)
	{
		text += u" P";
	}
	if (details.poisoned)
	{
		text += u" S";
	}
	return text;
}

uint8_t PlayerColourIndex(uint8_t player, int32_t landNumber)
{
	// The story's lands give their computer players colours of their own
	switch (landNumber)
	{
	case 1:
	case 4:
		return player == 1 ? 6 : player;
	case 2:
		return player == 1 ? 4 : (player == 2 ? 5 : player);
	case 3:
		return player == 1 ? 5 : player;
	case 5:
		return player == 1 ? 6 : player;
	default:
		return player;
	}
}

glm::u8vec3 LabelColour(std::optional<glm::u8vec3> player, bool lighterForWoman)
{
	constexpr uint32_t k_WomanShare = 145;
	constexpr uint32_t k_HalfShare = 128;
	constexpr uint8_t k_AllButBlack = 10;
	auto colour = player.value_or(glm::u8vec3(255));
	if (lighterForWoman)
	{
		colour = {TowardsWhite(colour.r, k_WomanShare), TowardsWhite(colour.g, k_WomanShare),
		          TowardsWhite(colour.b, k_WomanShare)};
	}
	if (colour.r < k_AllButBlack && colour.g < k_AllButBlack && colour.b < k_AllButBlack)
	{
		colour = glm::u8vec3(255);
	}
	return {TowardsWhite(colour.r, k_HalfShare), TowardsWhite(colour.g, k_HalfShare), TowardsWhite(colour.b, k_HalfShare)};
}

float PixelScale(glm::vec2 screen)
{
	return std::min(screen.x, screen.y * 4.0f / 3.0f) / k_ReferenceWidth;
}

bool Board::Add(const Request& request, float screenWidth, const WidthFn& width, float pixelScale)
{
	if (request.size < k_SmallestSize)
	{
		return false;
	}
	const float size = std::min(request.size, 1.0f);
	Label label {
	    .pixel = glm::vec2(request.pixel), .distance = request.distance, .pixelScale = pixelScale, .colour = request.colour};
	// Every size in pixels, at the label's size and the screen's scale
	const float pixels = size * pixelScale;

	// Its lines: at the line breaks, the last of them taking the rest
	const auto text = std::u16string_view(request.text).substr(0, k_MaxCharacters);
	size_t start = 0;
	while (label.lines.size() + 1 < k_MaxLines)
	{
		const auto lineBreak = text.find(u'\n', start);
		if (lineBreak == std::u16string_view::npos)
		{
			break;
		}
		label.lines.emplace_back(text.substr(start, lineBreak - start));
		start = lineBreak + 1;
	}
	label.lines.emplace_back(text.substr(start));

	label.lineHeight = pixels * k_LineHeight;
	label.inset = pixels * k_Inset;
	float boxWidth = 0.0f;
	for (const auto& line : label.lines)
	{
		boxWidth = std::max(boxWidth, width(line, label.lineHeight) + (pixels * k_ExtraWidth));
	}
	// Wider than the screen it is cut to half the screen
	if (screenWidth < boxWidth)
	{
		boxWidth = std::floor(screenWidth / 2.0f);
	}
	label.left = std::max(label.pixel.x - (boxWidth * 0.5f), 0.0f);
	label.right = boxWidth + label.left;
	if (screenWidth < label.right)
	{
		label.right = screenWidth;
		label.left = screenWidth - boxWidth;
	}
	label.bottom = label.pixel.y - (pixelScale * k_RaisedBy);
	label.top = label.bottom - ((pixels * k_ExtraHeight) + (static_cast<float>(label.lines.size()) * label.lineHeight));

	// Up over every label it covers, looking again from the leftmost each time it moves
	for (size_t i = 0; i < _labels.size() && _labels[i].left < label.right;)
	{
		const auto& other = _labels[i];
		const bool covers = other.right > label.left && other.top <= label.bottom && other.bottom >= label.top;
		if (!covers)
		{
			++i;
			continue;
		}
		const float height = label.bottom - label.top;
		label.bottom = other.top - pixelScale;
		label.top = label.bottom - height;
		if (other.top < 0.0f)
		{
			break;
		}
		i = 0;
	}
	if (label.top < 0.0f)
	{
		return false;
	}
	label.boxAlpha = static_cast<uint8_t>(size * k_BoxAlpha);
	label.wordsAlpha = static_cast<uint8_t>(size * k_WordsAlpha);
	const auto at = std::ranges::find_if(_labels, [&label](const Label& other) { return other.left >= label.left; });
	_labels.insert(at, std::move(label));
	return true;
}

std::vector<const Label*> Board::DrawOrder() const
{
	std::vector<const Label*> order;
	order.reserve(_labels.size());
	for (const auto& label : _labels)
	{
		order.push_back(&label);
	}
	std::ranges::stable_sort(order, std::ranges::greater {}, &Label::distance);
	return order;
}

std::vector<Quad> BoxShape(const Label& label)
{
	// The corners are rounded by a quarter of the box's height, no more than half its width or height; each corner
	// point is then cut to a whole pixel
	const float width = label.right - label.left;
	const float height = label.bottom - label.top;
	const float radius = std::min({height * 0.25f, width * 0.5f, height * 0.5f});
	const std::array<float, 4> xs {Truncated(label.left), Truncated(label.left + radius), Truncated(label.right - radius),
	                               Truncated(label.right)};
	const std::array<float, 4> ys {Truncated(label.top), Truncated(label.top + radius), Truncated(label.bottom - radius),
	                               Truncated(label.bottom)};
	constexpr std::array<float, 4> k_Us {0.0f, 0.125f, 0.125f, 0.25f};
	constexpr std::array<float, 4> k_Vs {0.0f, 0.125f, 0.125f, 0.25f};
	std::vector<Quad> quads;
	quads.reserve(9);
	for (size_t row = 0; row < 3; ++row)
	{
		for (size_t column = 0; column < 3; ++column)
		{
			const auto corner = [&](size_t c, size_t r) {
				return Corner {.position = {xs.at(c), ys.at(r)}, .uv = {k_Us.at(c), k_Vs.at(r)}};
			};
			quads.push_back(
			    {corner(column, row), corner(column + 1, row), corner(column + 1, row + 1), corner(column, row + 1)});
		}
	}
	return quads;
}

std::array<Quad, 2> PointerShape(const Label& label)
{
	const auto quad = [](float x0, float y0, float x1, float y1, glm::vec2 uv0, glm::vec2 uv1) {
		return Quad {Corner {.position = {x0, y0}, .uv = uv0}, Corner {.position = {x1, y0}, .uv = {uv1.x, uv0.y}},
		             Corner {.position = {x1, y1}, .uv = uv1}, Corner {.position = {x0, y1}, .uv = {uv0.x, uv1.y}}};
	};
	const float x = label.pixel.x;
	const float y = label.pixel.y;
	const float scale = label.pixelScale;
	const float headTop = Truncated(y - (scale * k_HeadHeightPixels));
	// The stalk stretches one row of the texture's pointer tile; the head is the tile's tip
	return {quad(Truncated(x - (scale * k_StalkHalfWidth)), Truncated(label.bottom), Truncated(x + (scale * k_StalkHalfWidth)),
	             headTop, {0.53125f, 0.05f}, {0.59375f, 0.05f}),
	        quad(Truncated(x - (scale * k_HeadHalfWidth)), headTop, Truncated(x + (scale * k_HeadHalfWidth)), Truncated(y),
	             {0.5f, 0.05f}, {0.625f, 0.125f})};
}

} // namespace openblack::gui::villager_labels
