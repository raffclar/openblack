/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <string_view>

#include "ECS/Components/CreatureFight.h"

/// The names that the fight tab of the creature window shows for the stages of a fight and of a knock-out
namespace openblack::debug::creature_fight_names
{

/// The name of a stage of a fight. Each stage has a name, so the compiler warns when a new stage has no name.
[[nodiscard]] constexpr std::string_view StageName(ecs::components::CreatureFighting::Stage stage) noexcept
{
	using Stage = ecs::components::CreatureFighting::Stage;
	switch (stage)
	{
	case Stage::FaceOff:
		return "turning to face";
	case Stage::Taunt:
		return "taunting";
	case Stage::Point:
		return "pointing at the arena";
	case Stage::Approach:
		return "walking to its place";
	case Stage::Ready:
		return "waiting";
	case Stage::Square:
		return "squaring up";
	case Stage::Duel:
		return "duelling";
	case Stage::Celebrate:
		return "celebrating";
	case Stage::Respond:
		return "responding";
	case Stage::Stop:
		return "stopping";
	}
	return "unknown";
}

/// The name of a stage of a knock-out. Each stage has a name, so the compiler warns when a new stage has no name.
[[nodiscard]] constexpr std::string_view KnockedOutName(ecs::components::CreatureKnockedOut::Stage stage) noexcept
{
	using Stage = ecs::components::CreatureKnockedOut::Stage;
	switch (stage)
	{
	case Stage::Lying:
		return "lying out cold";
	case Stage::FadingOut:
		return "fading out";
	case Stage::FadingIn:
		return "fading in at home";
	case Stage::Waiting:
		return "lying at home";
	case Stage::WaitingForSpells:
		return "lying until its spells end";
	case Stage::Resting:
		return "resting";
	case Stage::GettingUp:
		return "getting up";
	}
	return "unknown";
}

} // namespace openblack::debug::creature_fight_names
