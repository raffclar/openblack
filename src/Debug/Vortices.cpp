/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Vortices.h"

#include <utility>
#include <vector>

#include <SDL_events.h>
#include <SDL_mouse.h>
#include <fmt/format.h>
#include <imgui.h>

#include "3D/LandIslandInterface.h"
#include "ECS/Components/LandscapeVortex.h"
#include "ECS/Registry.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/ScriptStateInterface.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/Vortex.h"
#include "Locator.h"
#include "MiraclesModel.h"
#include "VorticesModel.h"

using namespace openblack;
using namespace openblack::debug::gui;
using namespace openblack::debug::vortices;

namespace
{
/// A left button press or release
bool IsLeftClick(const SDL_Event& event)
{
	return (event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEBUTTONUP) && event.button.button == SDL_BUTTON_LEFT;
}

/// The vortices on the land
std::vector<VortexAt> ReadVortices(const ecs::Registry& registry)
{
	std::vector<VortexAt> vortices;
	registry.Each<const ecs::components::LandscapeVortex>(
	    [&vortices](entt::entity entity, const ecs::components::LandscapeVortex& vortex) {
		    vortices.push_back({.entity = entity, .type = vortex.type, .state = vortex.state, .position = vortex.position});
	    });
	return vortices;
}
} // namespace

Vortices::Vortices() noexcept
    : Window("Vortices", ImVec2(480.0f, 420.0f))
{
}

void Vortices::Draw() noexcept
{
	if (!Locator::entitiesRegistry::has_value() || !Locator::scriptState::has_value())
	{
		ImGui::TextUnformatted("No vortices: no land loaded");
		return;
	}
	DrawList();
	DrawCreate();
	DrawExit();
	if (!_last.empty())
	{
		ImGui::Separator();
		ImGui::TextUnformatted(_last.c_str());
	}
}

void Vortices::DrawList() noexcept
{
	ImGui::SeparatorText("On the land");
	const auto vortices = ReadVortices(std::as_const(Locator::entitiesRegistry::value()));
	const auto rows = Rows(vortices);
	if (rows.empty())
	{
		ImGui::TextDisabled("No vortex on the land");
		return;
	}
	auto fadeOut = entt::entity {entt::null};
	auto remove = entt::entity {entt::null};
	constexpr auto k_Flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit;
	if (ImGui::BeginTable("Vortices", 5, k_Flags))
	{
		ImGui::TableSetupColumn("Entity");
		ImGui::TableSetupColumn("Type");
		ImGui::TableSetupColumn("Position");
		ImGui::TableSetupColumn("State");
		ImGui::TableSetupColumn("");
		ImGui::TableHeadersRow();
		for (const auto& row : rows)
		{
			ImGui::PushID(static_cast<int>(entt::to_integral(row.entity)));
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::Text("%u", entt::to_integral(row.entity));
			ImGui::TableNextColumn();
			ImGui::TextUnformatted(row.type.data(), row.type.data() + row.type.size());
			ImGui::TableNextColumn();
			ImGui::Text("(%.0f, %.0f, %.0f)", row.position.x, row.position.y, row.position.z);
			ImGui::TableNextColumn();
			ImGui::TextUnformatted(row.state.data(), row.state.data() + row.state.size());
			ImGui::TableNextColumn();
			ImGui::BeginDisabled(!row.canFadeOut);
			if (ImGui::SmallButton("Fade out"))
			{
				fadeOut = row.entity;
			}
			ImGui::EndDisabled();
			ImGui::SameLine();
			if (ImGui::SmallButton("Delete"))
			{
				remove = row.entity;
			}
			ImGui::PopID();
		}
		ImGui::EndTable();
	}

	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	if (fadeOut != entt::null && registry.Valid(fadeOut))
	{
		// as the script's fade out: it goes once the fade is over
		ecs::vortex::StartFadeOut(fadeOut);
		_last = fmt::format("Vortex {} fades out", entt::to_integral(fadeOut));
	}
	if (remove != entt::null && registry.Valid(remove))
	{
		// as a vortex whose fade out is over
		ecs::vortex::OnDeleted(remove);
		ecs::ToBeDeleted(remove);
		_last = fmt::format("Vortex {} deleted", entt::to_integral(remove));
	}
}

void Vortices::DrawCreate() noexcept
{
	ImGui::SeparatorText("Create under the hand");
	auto chosen = static_cast<int>(_type);
	for (const auto type : k_Types)
	{
		const auto name = std::string(TypeName(type));
		ImGui::RadioButton(name.c_str(), &chosen, static_cast<int>(type));
		ImGui::SameLine();
	}
	ImGui::NewLine();
	_type = static_cast<VortexType>(chosen);
	if (ImGui::Button("Create now"))
	{
		CreateAtHand();
	}
	ImGui::SameLine();
	if (!_atClick)
	{
		if (ImGui::Button("At the next click on the land"))
		{
			_atClick = true;
			_last = "Click on the land to place it";
		}
	}
	else if (ImGui::Button("Cancel the click"))
	{
		_atClick = false;
		_last.clear();
	}
}

void Vortices::DrawExit() noexcept
{
	ImGui::SeparatorText("The way to the next land");
	ImGui::TextWrapped("Opens the vortex a land's script opens to leave the land, at the script's place. Going through it "
	                   "to the next land is not ported yet.");
	for (const auto& exit : k_ExitVortices)
	{
		const auto label = fmt::format("Land {} ({:.0f}, {:.0f})", exit.land, exit.position.x, exit.position.z);
		if (ImGui::Button(label.c_str()))
		{
			CreateAt(k_ExitVortexType, exit.position);
		}
		ImGui::SameLine();
	}
	ImGui::NewLine();
}

void Vortices::CreateAt(VortexType type, glm::vec3 point) noexcept
{
	const auto made = ecs::vortex::Create(point, type);
	_last = CreatedMessage(type, point, made);
}

void Vortices::CreateAtHand() noexcept
{
	if (const auto hand = HandLandPoint())
	{
		CreateAt(_type, *hand);
	}
	else
	{
		_last = "The hand is not on the land";
	}
}

std::optional<glm::vec3> Vortices::HandLandPoint() noexcept
{
	if (!Locator::handSystem::has_value())
	{
		return std::nullopt;
	}
	using Side = ecs::systems::HandSystemInterface::Side;
	const auto positions = Locator::handSystem::value().GetPlayerHandPositions();
	auto point =
	    miracles::HandPoint(positions.at(static_cast<size_t>(Side::Left)), positions.at(static_cast<size_t>(Side::Right)));
	if (point.has_value() && Locator::terrainSystem::has_value())
	{
		point->y = Locator::terrainSystem::value().GetHeightAt({point->x, point->z});
	}
	return point;
}

void Vortices::Update() noexcept
{
	if (_clicked)
	{
		_clicked = false;
		_atClick = false;
		if (Locator::entitiesRegistry::has_value() && Locator::scriptState::has_value())
		{
			CreateAtHand();
		}
	}
}

bool Vortices::TakesEvent(const SDL_Event& event) const noexcept
{
	return IsLeftClick(event) &&
	       _leftCapture.Takes(event.type == SDL_MOUSEBUTTONDOWN, _atClick, ImGui::GetIO().WantCaptureMouse);
}

void Vortices::ProcessEventOpen(const SDL_Event& event) noexcept
{
	if (!TakesEvent(event))
	{
		return;
	}
	const bool press = event.type == SDL_MOUSEBUTTONDOWN;
	_leftCapture.Seen(press);
	if (press)
	{
		_clicked = true;
	}
}

void Vortices::ProcessEventAlways([[maybe_unused]] const SDL_Event& event) noexcept {}
