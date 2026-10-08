/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ConsciencesModel.h"

#include <cctype>
#include <charconv>

#include <algorithm>

#include <fmt/format.h>

using namespace openblack;
using namespace openblack::debug;
using namespace openblack::debug::consciences;

namespace
{
bool EqualIgnoringCase(char a, char b)
{
	return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
}
} // namespace

std::string_view consciences::ControlStateName(help::spirits::ControlState state)
{
	using help::spirits::ControlState;
	switch (state)
	{
	case ControlState::Home:
		return "at home";
	case ControlState::GoingHome:
		return "going home";
	case ControlState::Out:
		return "out";
	case ControlState::Clinging:
		return "clinging";
	}
	return "unknown";
}

std::string consciences::DudeStateName(uint32_t state)
{
	namespace dude_state = help::spirits::dude_state;
	switch (state)
	{
	case dude_state::k_Hover:
		return "hovering";
	case dude_state::k_Point:
		return "about to point";
	case dude_state::k_PointHoldL:
		return "pointing left";
	case dude_state::k_PointHoldR:
		return "pointing right";
	case dude_state::k_PointIntroL:
		return "starting to point left";
	case dude_state::k_PointIntroR:
		return "starting to point right";
	case dude_state::k_PointOutroL:
		return "stopping pointing left";
	case dude_state::k_PointOutroR:
		return "stopping pointing right";
	case dude_state::k_Avoid:
		return "avoiding";
	case dude_state::k_FlyToAnim:
		return "flying to a clip";
	case dude_state::k_Cling:
		return "clinging";
	case dude_state::k_ClingArrive:
		return "arriving at the edge";
	case dude_state::k_ClingLeave:
		return "leaving the edge";
	case dude_state::k_ScriptedAnim:
		return "playing a clip";
	default:
		return fmt::format("state {}", state);
	}
}

std::string_view consciences::SetModeName(uint32_t mode)
{
	switch (mode)
	{
	case 0:
		return "its texts";
	case 1:
		return "one text at random";
	case 2:
		return "the thing's texts";
	case 3:
		return "a script";
	default:
		return "unknown";
	}
}

bool consciences::MatchesSearch(const SetRow& row, std::string_view search)
{
	if (search.empty())
	{
		return true;
	}
	uint32_t number = 0;
	const auto* end = search.data() + search.size();
	if (const auto [last, error] = std::from_chars(search.data(), end, number); error == std::errc() && last == end)
	{
		return row.set == number;
	}
	return !std::ranges::search(row.script, search, EqualIgnoringCase).empty();
}

std::vector<Clip> consciences::ClipsOf(std::span<const std::string> animNames)
{
	std::vector<Clip> clips;
	for (size_t slot = k_StopClipSlot + 1; slot < animNames.size(); ++slot)
	{
		if (!animNames[slot].empty())
		{
			clips.push_back({.slot = static_cast<uint32_t>(slot), .name = animNames[slot]});
		}
	}
	return clips;
}

glm::ivec2 consciences::ClipSpot(int dude, glm::ivec2 screen)
{
	const int32_t x = dude == help::spirits::k_GoodDude ? screen.x / 4 : (screen.x * 3) / 4;
	return {x, screen.y / 2};
}

std::string_view consciences::OverrideName(help::SpeakerOverride override)
{
	using help::SpeakerOverride;
	switch (override)
	{
	case SpeakerOverride::None:
		return "as the game says";
	case SpeakerOverride::SilenceGood:
		return "good spirit silent";
	case SpeakerOverride::SilenceEvil:
		return "evil spirit silent";
	case SpeakerOverride::ForceGood:
		return "good spirit says everything";
	case SpeakerOverride::ForceEvil:
		return "evil spirit says everything";
	}
	return "unknown";
}
