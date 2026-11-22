/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "LoadIslandMenu.h"

#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

#include <imgui.h>
#include <spdlog/spdlog.h>

#include "Common/DeveloperToolsGate.h"
#include "ECS/Systems/NetworkGameSystemInterface.h"
#include "Game.h"
#include "Level.h"
#include "Locator.h"
#include "Online/NetworkLand.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;

namespace
{

/// In a network game: the game server starts the room again on the land, on the testbed or on the map for 32 gods
void AskServerForLand(const game_socket::EditorLand& land) noexcept
{
	const bool sent = Locator::networkGame::has_value() && Locator::networkGame::value().SendEditorLand(land);
	if (auto logger = spdlog::get("game"))
	{
		const auto name = land.testbed       ? std::string("the testbed")
		                  : land.godsRingMap ? std::string("the map for 32 gods")
		                                     : land.land;
		// The test tools find the words "The Load Island menu asks the game server"
		SPDLOG_LOGGER_INFO(logger, "The Load Island menu asks the game server for {}: {}", name,
		                   sent ? "the request went" : "the request did not go");
	}
}

/// The levels of one kind, in the order of their names
std::vector<const Level*> SortedByName(std::vector<const Level*> levels)
{
	std::ranges::sort(levels, [](const Level* level1, const Level* level2) { return level1->GetName() < level2->GetName(); });
	return levels;
}

} // namespace

developer_tools::LandMenuAct debug::gui::CurrentLandMenuAct() noexcept
{
	return developer_tools::LandMenuActOf(developer_tools::NetworkSessionLive(), developer_tools::LocalPlayerHasEditorClaim());
}

void debug::gui::DrawLoadIslandItems(developer_tools::LandMenuAct act) noexcept
{
	if (act == developer_tools::LandMenuAct::Closed)
	{
		return;
	}
	const bool asksServer = act == developer_tools::LandMenuAct::AsksServer;
	auto& game = *Game::Instance();
	std::vector<const Level*> campaigns;
	std::vector<const Level*> playgrounds;
	Locator::resources::value().GetLevels().Each([&campaigns, &playgrounds](entt::id_type /*id*/, const Level& level) {
		(level.GetType() == Level::LandType::Campaign ? campaigns : playgrounds).emplace_back(&level);
	});
	campaigns = SortedByName(std::move(campaigns));
	playgrounds = SortedByName(std::move(playgrounds));

	const auto menuItem = [&game, asksServer](const Level& level) {
		if (ImGui::MenuItem(level.GetName().c_str(), nullptr, false, level.IsValid()))
		{
			if (asksServer)
			{
				AskServerForLand(
				    {.land = network_land::WireName(level.GetScriptPath()), .testbed = false, .godsRingMap = false});
			}
			else
			{
				game.LoadFromLandMenu(level.GetScriptPath());
			}
		}
		if (!level.GetDescription().empty() && ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("%s", level.GetDescription().c_str());
		}
	};

	if (ImGui::BeginMenu("Story Islands"))
	{
		for (const auto* level : campaigns)
		{
			menuItem(*level);
		}
		ImGui::EndMenu();
	}
	if (ImGui::BeginMenu("Playground Islands"))
	{
		for (const auto* level : playgrounds)
		{
			menuItem(*level);
		}
		ImGui::EndMenu();
	}
	ImGui::Separator();
	// A build for players has no testbed
	if (ImGui::MenuItem("Creature Testbed", nullptr, false, developer_tools::k_Built))
	{
		if (asksServer)
		{
			AskServerForLand({.land = {}, .testbed = true, .godsRingMap = false});
		}
		else
		{
			game.LoadTestbed();
		}
	}
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("A flat plane over the whole map with a lake north of the middle, for trying out how "
		                  "creatures move, are animated, drink and wade, and what the water reflects");
	}
	if (ImGui::MenuItem("Map for 32 Gods"))
	{
		if (asksServer)
		{
			AskServerForLand({.land = {}, .testbed = false, .godsRingMap = true});
		}
		else
		{
			game.LoadGodsRingMap();
		}
	}
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip(asksServer ? "The network map for 32 gods for the whole room: a round island with a citadel and a "
		                               "village in each of its 32 slots"
		                             : "The network map for 32 gods on this computer alone: a round island with a citadel "
		                               "and a village in each of its 32 slots");
	}
}
