/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureNaming.h"

#include "ECS/Systems/HelpTextSystemInterface.h"
#include "ECS/Systems/PlayerProfileSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "Gui/TextDatabase.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs;

creature_names::Owner creature_naming::OwnerOf(PlayerNames owner)
{
	if (owner == PlayerNames::NEUTRAL)
	{
		return creature_names::Owner::Nobody;
	}
	const bool computer = Locator::playerSystem::has_value() ? Locator::playerSystem::value().IsComputerPlayer(owner)
	                                                         : owner != PlayerNames::PLAYER_ONE;
	return computer ? creature_names::Owner::Computer : creature_names::Owner::Player;
}

std::u16string creature_naming::HelpWords(uint32_t text)
{
	return Locator::helpTextSystem::has_value() ? Locator::helpTextSystem::value().GetWords(text) : std::u16string {};
}

std::optional<std::u16string> creature_naming::ProfileCreatureName()
{
	if (!Locator::playerProfileSystem::has_value())
	{
		return std::nullopt;
	}
	const auto profile = Locator::playerProfileSystem::value().GetCurrentProfile();
	if (!profile.has_value())
	{
		return std::nullopt;
	}
	return gui::ToUtf16(profile->creatureName);
}

std::u16string creature_naming::NewName(PlayerNames owner, size_t creatures)
{
	const auto profileName = ProfileCreatureName();
	return creature_names::NewName(OwnerOf(owner), creatures,
	                               profileName.has_value() ? std::optional<std::u16string_view>(*profileName) : std::nullopt,
	                               HelpWords);
}
