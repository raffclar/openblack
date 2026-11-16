/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "DebugActs.h"

#include <cmath>

#include <algorithm>

#include "Common/DebugThingActs.h"
#include "Creature/BonusCreatures.h"
#include "Help/Spirits.h"
#include "PlayerSlots.h"

namespace openblack::debug_acts
{

using player_commands::DebugAdvisor;
using player_commands::DebugConsoleLine;
using player_commands::DebugWorld;

static_assert(k_AdvisorAnims == help::spirits::k_AnimSlots, "The acts take each animation of an advisor");

std::string_view NameOf(DebugAdvisor::Act act) noexcept
{
	switch (act)
	{
	case DebugAdvisor::Act::Out:
		return "out";
	case DebugAdvisor::Act::Appear:
		return "appear";
	case DebugAdvisor::Act::Home:
		return "home";
	case DebugAdvisor::Act::Vanish:
		return "vanish";
	case DebugAdvisor::Act::Fly:
		return "fly";
	case DebugAdvisor::Act::Cling:
		return "cling";
	case DebugAdvisor::Act::ScreenPoint:
		return "screen_point";
	case DebugAdvisor::Act::PointAt:
		return "point_at";
	case DebugAdvisor::Act::LookAt:
		return "look_at";
	case DebugAdvisor::Act::StopPointing:
		return "stop_pointing";
	case DebugAdvisor::Act::StopLooking:
		return "stop_looking";
	case DebugAdvisor::Act::PlayAnim:
		return "play_anim";
	case DebugAdvisor::Act::Feel:
		return "feel";
	}
	return "unknown";
}

std::string_view NameOf(DebugWorld::Act act) noexcept
{
	switch (act)
	{
	case DebugWorld::Act::TimeOfDay:
		return "time_of_day";
	case DebugWorld::Act::PlayerAlignment:
		return "player_alignment";
	case DebugWorld::Act::BonusCreature:
		return "bonus_creature";
	}
	return "unknown";
}

std::optional<std::string_view> AdvisorActRefusal(const DebugAdvisor& act) noexcept
{
	if (act.act > DebugAdvisor::Act::Feel)
	{
		return "the act is not an act of the advisors";
	}
	if (act.advisor > k_EvilAdvisor)
	{
		return "the advisor is not the good one, the evil one or both";
	}
	const bool placeFinite = std::isfinite(act.place.x) && std::isfinite(act.place.y);
	if (!placeFinite || act.place.x < 0.0f || act.place.x > 1.0f || act.place.y < 0.0f || act.place.y > 1.0f)
	{
		return "the place on the screen is not from 0 to 1";
	}
	if (!std::isfinite(act.position.x) || !std::isfinite(act.position.y) || !std::isfinite(act.position.z))
	{
		return "the position is not a number";
	}
	if (act.anim >= k_AdvisorAnims)
	{
		return "the animation is not an animation of the advisors";
	}
	if (!std::isfinite(act.speed) || act.speed < k_SlowestAnim || act.speed > k_FastestAnim)
	{
		return "the speed of the animation is not from 0.1 to 3";
	}
	if (act.emotion > k_LastEmotion)
	{
		return "the emotion is not an emotion of the advisors";
	}
	return std::nullopt;
}

std::optional<std::string_view> WorldActRefusal(const DebugWorld& act) noexcept
{
	if (!std::isfinite(act.value))
	{
		return "the value is not a number";
	}
	switch (act.act)
	{
	case DebugWorld::Act::TimeOfDay:
		if (act.value < 0.0f || act.value > k_HoursInDay)
		{
			return "the time of the day is not from 0 to 24 hours";
		}
		return std::nullopt;
	case DebugWorld::Act::PlayerAlignment:
		if (!player_slots::SlotOfPlayer(static_cast<PlayerNames>(act.player)).has_value())
		{
			return "the player is not a player of a slot";
		}
		if (act.value < -1.0f || act.value > 1.0f)
		{
			return "the alignment is not from -1 to 1";
		}
		return std::nullopt;
	case DebugWorld::Act::BonusCreature:
		if (act.index >= creature_swap::k_BonusCount)
		{
			return "the index is not a bonus creature";
		}
		if (act.value != 0.0f && act.value != 1.0f)
		{
			return "the value of a bonus creature is not 0 or 1";
		}
		return std::nullopt;
	}
	return "the act is not a change of the debug menus";
}

std::optional<std::string_view> ConsoleLineRefusal(const DebugConsoleLine& line) noexcept
{
	if (line.text.empty())
	{
		return "the line is empty";
	}
	if (line.text.size() > player_commands::k_MostConsoleLineBytes)
	{
		return "the line has more than 512 bytes";
	}
	if (std::ranges::find(line.text, '\0') != line.text.end())
	{
		return "the line has a zero byte";
	}
	return std::nullopt;
}

std::optional<std::string_view> RefusalOf(const player_commands::Action& action) noexcept
{
	if (const auto* advisor = std::get_if<DebugAdvisor>(&action))
	{
		return AdvisorActRefusal(*advisor);
	}
	if (const auto* world = std::get_if<DebugWorld>(&action))
	{
		return WorldActRefusal(*world);
	}
	if (const auto* line = std::get_if<DebugConsoleLine>(&action))
	{
		return ConsoleLineRefusal(*line);
	}
	// The acts of the Physics, Particles, Temple villagers and Path finding windows
	return debug_thing_acts::RefusalOf(action);
}

bool ApplyWorldAct(WorldTarget& target, const DebugWorld& act)
{
	if (WorldActRefusal(act).has_value())
	{
		return false;
	}
	switch (act.act)
	{
	case DebugWorld::Act::TimeOfDay:
		target.SetTimeOfDay(std::fmod(act.value, k_HoursInDay));
		break;
	case DebugWorld::Act::PlayerAlignment:
		target.SetPlayerAlignment(static_cast<PlayerNames>(act.player), act.value);
		break;
	case DebugWorld::Act::BonusCreature:
		target.SetBonusUnlocked(act.index, act.value == 1.0f);
		break;
	}
	return true;
}

} // namespace openblack::debug_acts
