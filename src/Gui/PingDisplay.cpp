/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "PingDisplay.h"

#include <algorithm>
#include <array>
#include <limits>

#include "MultiplayerFrontEndRules.h"
#include "ScreenFit.h"

using namespace openblack::gui;

std::optional<int32_t> ping_display::Milliseconds(std::optional<uint64_t> roundTripMicroseconds) noexcept
{
	if (!roundTripMicroseconds.has_value())
	{
		return std::nullopt;
	}
	constexpr uint64_t k_Largest = std::numeric_limits<int32_t>::max();
	return static_cast<int32_t>(std::min(*roundTripMicroseconds / 1000, k_Largest));
}

std::optional<std::u16string> ping_display::Text(std::optional<uint64_t> roundTripMicroseconds)
{
	const auto milliseconds = Milliseconds(roundTripMicroseconds);
	if (!milliseconds.has_value())
	{
		return std::nullopt;
	}
	const std::array<multiplayer::TextArgument, 1> arguments = {int64_t {*milliseconds}};
	return multiplayer::FormatText(k_Format, arguments);
}

std::u16string ping_display::AfterOwnName(std::u16string line, const std::optional<std::u16string>& ping)
{
	if (ping.has_value())
	{
		line += u' ';
		line += *ping;
	}
	return line;
}

glm::vec2 ping_display::Origin(glm::u16vec2 resolution) noexcept
{
	return -FrontEndSpace(resolution.x, resolution.y).offset;
}
