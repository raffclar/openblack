/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureStatusPanel.h"

#include <cmath>

#include <algorithm>
#include <string>

using namespace openblack;
using namespace openblack::creature_panel;

namespace
{
/// The box sits a fortieth of the screen in from its left (and from the top while following a creature), and is a
/// quarter of the screen wide
constexpr int k_BoxInsets = 40;
constexpr int k_BoxWidths = 4;
/// The rows are a sixteenth of the screen's height apart
constexpr int k_RowsPerScreen = 16;
} // namespace

float creature_panel::Clamp01(float value)
{
	if (!(value > 0.0f))
	{
		return 0.0f;
	}
	if (!(value < 1.0f))
	{
		return 1.0f;
	}
	return value;
}

float creature_panel::ClampReward(float reward)
{
	if (!(reward > -1.0f))
	{
		return -1.0f;
	}
	if (!(reward < 1.0f))
	{
		return 1.0f;
	}
	return reward;
}

Values creature_panel::FromNeeds(const creature_physiology::Needs& needs, std::optional<float> reward)
{
	return {
	    .damage = Clamp01(1.0f - needs.life),
	    .hunger = Clamp01(1.0f - needs.energy),
	    .tiredness = Clamp01(needs.exhaustion),
	    .reward = reward.has_value() ? std::optional(ClampReward(*reward)) : std::nullopt,
	};
}

int creature_panel::Percent(float value)
{
	return static_cast<int>(value * 100.0f);
}

std::u16string creature_panel::FormatNumber(std::u16string_view format, int number)
{
	std::u16string text;
	for (size_t i = 0; i < format.size(); ++i)
	{
		if (format[i] == u'%' && i + 1 < format.size())
		{
			if (format[i + 1] == u'd')
			{
				for (const auto digit : std::to_string(number))
				{
					text.push_back(static_cast<char16_t>(digit));
				}
				++i;
				continue;
			}
			if (format[i + 1] == u'%')
			{
				text.push_back(u'%');
				++i;
				continue;
			}
		}
		text.push_back(format[i]);
	}
	return text;
}

std::u16string creature_panel::FormatPercent(float value)
{
	return FormatNumber(u"%d%%", Percent(value));
}

RewardKind creature_panel::Classify(float reward)
{
	if (reward < -k_NoRewardBelow)
	{
		return RewardKind::Bad;
	}
	if (reward > k_NoRewardBelow)
	{
		return RewardKind::Good;
	}
	return RewardKind::None;
}

std::u16string creature_panel::FormatReward(float reward, const RewardTexts& texts)
{
	switch (Classify(reward))
	{
	case RewardKind::Bad:
		return FormatNumber(texts.bad, Percent(-reward));
	case RewardKind::Good:
		return FormatNumber(texts.good, Percent(reward));
	case RewardKind::None:
	default:
		return std::u16string(texts.none) + u" 0%";
	}
}

float creature_panel::Fill(Row row, const Values& values)
{
	switch (row)
	{
	case Row::Damage:
		return values.damage;
	case Row::Hunger:
		return values.hunger;
	case Row::Tiredness:
		return values.tiredness;
	case Row::Reward:
	default:
		return (values.reward.value_or(0.0f) * 0.5f) + 0.5f;
	}
}

glm::vec4 creature_panel::BarColour(Row row, float fill)
{
	if (row != Row::Reward)
	{
		return {1.0f, 1.0f, 0.0f, 1.0f};
	}
	return fill < 0.5f ? glm::vec4(1.0f, 0.0f, 0.0f, 1.0f) : glm::vec4(0.0f, 1.0f, 0.0f, 1.0f);
}

std::u16string creature_panel::FormatValue(Row row, const Values& values, const RewardTexts& texts)
{
	return row == Row::Reward ? FormatReward(values.reward.value_or(0.0f), texts) : FormatPercent(Fill(row, values));
}

float creature_panel::LabelColumnWidth(std::span<const float> labelWidths, float size)
{
	const auto widest = labelWidths.empty() ? 0.0f : *std::ranges::max_element(labelWidths);
	return widest + static_cast<float>(static_cast<int>(size) / 2);
}

int creature_panel::RowPitch(glm::u16vec2 resolution)
{
	return resolution.y / k_RowsPerScreen;
}

Layout creature_panel::Compute(glm::u16vec2 resolution, bool withReward, float labelColumnWidth)
{
	// Whole pixels throughout, from shares of the screen. The width is a 4:3 screen's as tall as this one (or this
	// one's, when narrower): on a wider screen the panel keeps its 4:3 size against the left edge.
	const int screenHeight = resolution.y;
	const int width = std::min<int>(resolution.x, (screenHeight * 4) / 3);
	const int size = RowPitch(resolution);
	Layout layout;
	layout.size = static_cast<float>(size);
	layout.textSize = static_cast<float>(size / 2);
	const int x0 = width / k_BoxInsets;
	const int x1 = (width / k_BoxWidths) + x0;
	// With the reward a quarter of the way down; following a creature near the top, three quarters as high
	const int y0 = withReward ? screenHeight / 4 : screenHeight / k_BoxInsets;
	const float heightShare = withReward ? 1.0f : 0.75f;
	const int y1 = static_cast<int>((static_cast<float>(screenHeight / 3) * heightShare) + static_cast<float>(y0));
	layout.box = {.min = {static_cast<float>(x0), static_cast<float>(y0)},
	              .max = {static_cast<float>(x1), static_cast<float>(y1)}};

	layout.rowCount = withReward ? k_RowCount : k_RowCount - 1;
	const auto gap = static_cast<float>(size / 4);
	const auto left = static_cast<float>(static_cast<int>(static_cast<float>(x0) + labelColumnWidth));
	const auto right = static_cast<float>(static_cast<int>(left + labelColumnWidth));
	for (size_t i = 0; i < layout.rowCount; ++i)
	{
		const auto row = static_cast<Row>(i);
		// A third of a row more before the reward
		int y = y0 + ((2 * size) / 3) + (static_cast<int>(i) * size);
		if (row == Row::Reward)
		{
			y += size / 3;
		}
		const auto top = static_cast<float>(y);
		const Rect bar {.min = {left, top}, .max = {right, top + layout.textSize}};
		layout.rows.at(i) = {
		    .row = row,
		    .bar = bar,
		    .labelRight = {bar.min.x - gap, top},
		    .valueLeft = {bar.max.x + gap, top},
		};
	}
	return layout;
}

BarFill creature_panel::FillOf(const Rect& bar, float fill, bool fromMiddle)
{
	// The fill runs inside the frame, from its left edge or its middle to how full the bar is, in whole pixels
	const float left = bar.min.x + k_BarInset;
	const float right = bar.max.x - k_BarInset;
	const float end = std::clamp(std::trunc(left + ((right - left) * Clamp01(fill))), left, right);
	BarFill result {.from = left, .to = end, .top = bar.min.y + k_BarInset, .bottom = bar.max.y - k_BarInset};
	if (fromMiddle)
	{
		const float middle = std::trunc(left + ((right - left) * 0.5f));
		result.from = middle;
		result.middle = middle;
	}
	return result;
}
