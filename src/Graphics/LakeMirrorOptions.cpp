/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "LakeMirrorOptions.h"

#include <charconv>

#include <system_error>

using namespace openblack::graphics;

std::optional<lake_mirrors::Options> lake_mirrors::ParseOptions(std::string_view words)
{
	Options options;
	while (!words.empty())
	{
		const auto comma = words.find(',');
		const auto word = words.substr(0, comma);
		words = comma == std::string_view::npos ? std::string_view {} : words.substr(comma + 1);
		const auto equals = word.find('=');
		const auto name = word.substr(0, equals);
		std::optional<float> value;
		if (equals != std::string_view::npos)
		{
			const auto text = word.substr(equals + 1);
			float parsed = 0.0f;
			const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), parsed);
			if (error != std::errc {} || end != text.data() + text.size())
			{
				return std::nullopt;
			}
			value = parsed;
		}
		if (word.empty())
		{
			continue;
		}
		if (word == "off")
		{
			options.mirrors = false;
		}
		else if (word == "no-floor")
		{
			options.floor = false;
		}
		else if (word == "no-rects")
		{
			options.rects = false;
		}
		else if (word == "occlusion")
		{
			options.occlusion = true;
		}
		else if (word == "no-share")
		{
			options.shareLayers = false;
		}
		else if (name == "half" && value.has_value())
		{
			options.halfSizeBelow = *value;
		}
		else if (name == "join" && value.has_value())
		{
			options.joinWithin = *value;
		}
		else if (name == "alternate" && value.has_value())
		{
			options.everySecondFrameBelow = *value;
		}
		else
		{
			return std::nullopt;
		}
	}
	return options;
}
