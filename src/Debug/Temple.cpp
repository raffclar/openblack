/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Temple.h"

#include <algorithm>
#include <array>
#include <optional>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <utility>

#include <fmt/format.h>

#include "3D/TempleInteriorInterface.h"
#include "ECS/Components/Temple.h"
#include "ECS/Systems/AlignmentSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "ECS/Systems/TempleExteriorSystemInterface.h"
#include "Locator.h"
#include "Magic/Core/Players.h"
#include "TempleAlignmentModel.h"
#include "Worship/Citadel.h"

using namespace openblack;
using namespace debug::gui;

TempleInterior::TempleInterior() noexcept
    : Window("Citadel", ImVec2(360.0f, 420.0f))
{
}

const std::array<std::tuple<TempleRoom, std::string_view>, 7> k_Lookup {
    std::make_tuple(TempleRoom::Challenge, "Challenge Room"),   //
    std::make_tuple(TempleRoom::CreatureCave, "Creature Cave"), //
    std::make_tuple(TempleRoom::Credits, "Credits Room"),       //
    std::make_tuple(TempleRoom::Main, "Hall"),                  //
    std::make_tuple(TempleRoom::Multi, "Multiplayer Room"),     //
    std::make_tuple(TempleRoom::Options, "Options Room"),       //
    std::make_tuple(TempleRoom::SaveGame, "Save Game Room"),    //
};

void TempleInterior::Draw() noexcept
{
	if (Locator::temple::has_value())
	{
		auto& temple = Locator::temple::value();
		const auto inactive = !temple.Active();

		if (ImGui::Button(inactive ? "Enter temple" : "Exit temple"))
		{
			if (inactive)
			{
				temple.Activate();
			}
			else
			{
				temple.Deactivate();
			}
		}

		// Outside, each room's button takes the player into the temple at that room; inside, it goes to the room
		for (auto [room, name] : k_Lookup)
		{
			if (ImGui::Button(fmt::format("{}", name).c_str()))
			{
				if (inactive)
				{
					temple.Activate(static_cast<TempleRoom>(room));
				}
				else
				{
					temple.EnterRoom(room);
				}
			}
		}
	}
	else
	{
		ImGui::Text("No temple");
	}

	DrawAlignment();
}

void TempleInterior::DrawAlignment() noexcept
{
	ImGui::Separator();
	ImGui::TextUnformatted("Alignment");
	if (!Locator::playerSystem::has_value() || !Locator::alignmentSystem::has_value())
	{
		ImGui::TextUnformatted("No players");
		return;
	}
	const auto players = temple_alignment::PlayersInGame(&magic::players::EntityOf);
	if (players.empty())
	{
		ImGui::TextUnformatted("No players");
		return;
	}
	if (std::ranges::find(players, _player) == players.end())
	{
		_player = players.front();
	}
	if (ImGui::BeginCombo("Player", k_PlayerNamesStrs.at(static_cast<size_t>(_player)).data()))
	{
		for (const auto player : players)
		{
			if (ImGui::Selectable(k_PlayerNamesStrs.at(static_cast<size_t>(player)).data(), player == _player))
			{
				_player = player;
			}
		}
		ImGui::EndCombo();
	}

	// Only a move of the slider or a press of a button writes the alignment
	auto& alignment = Locator::alignmentSystem::value();
	float value = alignment.GetPlayerAlignment(_player);
	if (ImGui::SliderFloat("Evil to good", &value, -1.0f, 1.0f, "%.3f"))
	{
		temple_alignment::SetAlignment(alignment, _player, value);
	}
	for (const auto& [label, preset] : {std::pair {"Evil", -1.0f}, std::pair {"Neutral", 0.0f}, std::pair {"Good", 1.0f}})
	{
		if (preset != -1.0f)
		{
			ImGui::SameLine();
		}
		if (ImGui::Button(label))
		{
			temple_alignment::SetAlignment(alignment, _player, preset);
		}
	}
	ImGui::Text("Alignment %.3f", alignment.GetPlayerAlignment(_player));

	// The player's temple outside, which takes the alignment a step a turn
	const auto heart = Locator::entitiesRegistry::has_value() ? worship::citadel::HeartOf(worship::citadel::Of(_player))
	                                                          : entt::entity {entt::null};
	const auto look = heart != entt::null && Locator::templeExteriorSystem::has_value()
	                      ? Locator::templeExteriorSystem::value().GetLook(heart)
	                      : std::nullopt;
	if (!look)
	{
		ImGui::TextUnformatted("No temple outside for this player");
		return;
	}
	const float targetNow = temple_alignment::AlignmentTargetNow(alignment, _player);
	ImGui::TextUnformatted(temple_alignment::LookStatus(*look, targetNow).c_str());
	ImGui::ProgressBar(look->alignment, ImVec2(-1.0f, 0.0f), "evil to good");
	if (ImGui::Button("Snap the outside now"))
	{
		temple_alignment::SnapOutside(alignment, Locator::templeExteriorSystem::value(), heart, _player);
	}
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Without it the outside follows a step a turn, as the original's");
	}
}

void TempleInterior::Update() noexcept {}

void TempleInterior::ProcessEventOpen([[maybe_unused]] const SDL_Event& event) noexcept {}

void TempleInterior::ProcessEventAlways([[maybe_unused]] const SDL_Event& event) noexcept {}
