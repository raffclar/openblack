/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "LandGenerator.h"

#include <algorithm>
#include <random>
#include <vector>

#include <SaveSlots.h>
#include <fmt/format.h>
#include <spdlog/spdlog.h>

#include "Common/DeveloperToolsGate.h"
#include "ECS/Systems/LandSystemInterface.h"
#include "ECS/Systems/SaveGameSystemInterface.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::editor;

namespace
{
/// Whether the generator can load a land on this computer now. Only a player who can open the editor uses it, and
/// never in a network game (see developer_tools::LandGeneratorTabOf).
[[nodiscard]] bool LoadsHere()
{
	const auto tab = developer_tools::CurrentLandGeneratorTab();
	if (tab != developer_tools::LandGeneratorTab::Shown)
	{
		SPDLOG_LOGGER_WARN(spdlog::get("game"), "The game refuses the land generator: the tab is {}",
		                   developer_tools::NameOf(tab));
		return false;
	}
	return true;
}
} // namespace

std::optional<uint32_t> land_generator::FreeSlot(std::span<const uint32_t> usedSlots)
{
	for (uint32_t slot = 0; slot <= gamesave::k_LastPlayerSlot; ++slot)
	{
		if (std::ranges::find(usedSlots, slot) == usedSlots.end())
		{
			return slot;
		}
	}
	return std::nullopt;
}

bool land_generator::IsOn(const Session& session, std::string_view landPath)
{
	return session.returnSlot.has_value() && generated_land::ChoicesOf(landPath).has_value();
}

uint64_t land_generator::RandomSeed()
{
	std::random_device device;
	std::uniform_int_distribution<uint64_t> seeds;
	return seeds(device);
}

bool land_generator::Start(Session& session, uint64_t seed)
{
	if (!LoadsHere() || !Locator::saveGameSystem::has_value() || !Locator::landSystem::has_value())
	{
		session.status = "The land generator is not available in this game";
		return false;
	}
	auto& saves = Locator::saveGameSystem::value();
	// The generator never writes over a save: it takes the first slot of the player's own saves that is empty
	std::vector<uint32_t> used;
	for (const auto& slot : saves.Slots())
	{
		used.push_back(slot.slot);
	}
	const auto slot = FreeSlot(used);
	if (!slot.has_value())
	{
		session.status = "Each save slot has a save. Delete a save in the save game room, then try again.";
		return false;
	}
	if (!saves.Save(*slot, k_SaveName))
	{
		session.status = fmt::format("The game did not save into slot {}, so the land generator did not start", *slot);
		return false;
	}
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "The land generator saved the game into slot {}", *slot);
	session.returnSlot = slot;
	session.choices = {.seed = seed};
	return Generate(session);
}

bool land_generator::Generate(Session& session)
{
	if (!LoadsHere() || !Locator::landSystem::has_value())
	{
		session.status = "The land generator is not available in this game";
		return false;
	}
	session.choices = generated_land::Clamped(session.choices);
	if (!Locator::landSystem::value().LoadGeneratedLand(session.choices))
	{
		session.status = "The land did not load. The game folder must have the landscapes of the story.";
		return false;
	}
	session.status = fmt::format("The land of seed {} loaded", session.choices.seed);
	return true;
}

bool land_generator::Exit(Session& session)
{
	const auto slot = session.returnSlot;
	if (!slot.has_value() || !Locator::saveGameSystem::has_value())
	{
		return false;
	}
	// The generator stays on after a failed load, so that the player can try again
	if (!Locator::saveGameSystem::value().Load(*slot))
	{
		session.status = fmt::format("The save in slot {} did not load. It stays in the save game room.", *slot);
		SPDLOG_LOGGER_WARN(spdlog::get("game"), "The land generator did not load the save of slot {}", *slot);
		return false;
	}
	session.returnSlot.reset();
	session.status.clear();
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "The land generator stopped and loaded the save of slot {}", *slot);
	return true;
}
