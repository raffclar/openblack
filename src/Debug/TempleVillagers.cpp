/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TempleVillagers.h"

#include <cmath>

#include <algorithm>
#include <array>
#include <numbers>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <fmt/format.h>
#include <glm/geometric.hpp>
#include <glm/gtx/vec_swizzle.hpp>

#include "3D/LandIslandInterface.h"
#include "3D/TempleInteriorInterface.h"
#include "Audio/AudioManagerInterface.h"
#include "Camera/Camera.h"
#include "ECS/Archetypes/VillagerArchetype.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/Villager.h"
#include "ECS/Registry.h"
#include "ECS/Systems/DynamicsSystemInterface.h"
#include "ECS/Systems/Implementations/VillagerWorship.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "ECS/Systems/VillageTotemSystemInterface.h"
#include "Editor/EditorEntities.h"
#include "InfoConstants.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::debug::gui;
using namespace openblack::temple_villagers;

namespace
{
/// The player whose temple the window looks at
constexpr auto k_Player = PlayerNames::PLAYER_ONE;
/// New villagers stand round their town's first building, this far from it, in metres
constexpr float k_SpawnRing = 6.0f;
/// The camera looks at a villager from no further than this, and at the temple from no further than that, in metres,
/// keeping its way of looking
constexpr float k_LookDistance = 45.0f;
constexpr float k_TempleLookDistance = 160.0f;

constexpr ImU32 k_WorshippingColour = IM_COL32(255, 210, 60, 255);
constexpr ImU32 k_OnTheWayColour = IM_COL32(80, 200, 255, 255);
constexpr ImU32 k_NearbyColour = IM_COL32(200, 200, 200, 200);
constexpr ImU32 k_SelectedColour = IM_COL32(255, 60, 60, 255);
constexpr ImU32 k_PlaceColour = IM_COL32(120, 255, 120, 255);
constexpr ImU32 k_InsideColour = IM_COL32(255, 140, 255, 255);
constexpr ImU32 k_FloorColour = IM_COL32(255, 255, 255, 70);

ImU32 ColourOf(Standing standing)
{
	switch (standing)
	{
	case Standing::Worshipping:
		return k_WorshippingColour;
	case Standing::OnTheWay:
		return k_OnTheWayColour;
	case Standing::Nearby:
		return k_NearbyColour;
	}
	return k_NearbyColour;
}

std::string_view StateName(VillagerStates state)
{
	const auto index = static_cast<size_t>(state);
	return index < k_VillagerStateStrings.size() ? k_VillagerStateStrings.at(index) : std::string_view("?");
}

std::string_view TribeName(Tribe tribe)
{
	const auto index = static_cast<size_t>(tribe);
	return tribe != Tribe::NONE && index < k_TribeStrs.size() ? k_TribeStrs.at(index) : std::string_view("NONE");
}

std::string_view JobName(VillagerNumber number)
{
	const auto index = static_cast<size_t>(number);
	return index < k_VillagerNumberStrs.size() ? k_VillagerNumberStrs.at(index) : std::string_view("?");
}

uint32_t Id(entt::entity entity)
{
	return static_cast<uint32_t>(entt::to_integral(entity));
}

/// Where a point of the world is on the screen, in the debug windows' units; none off the screen or behind the camera
std::optional<ImVec2> OnScreen(const glm::vec3& point)
{
	if (!Locator::camera::has_value())
	{
		return std::nullopt;
	}
	const auto size = ImGui::GetIO().DisplaySize;
	glm::vec3 screen;
	if (!Locator::camera::value().ProjectWorldToScreen(point, glm::vec4(0.0f, 0.0f, size.x, size.y), screen))
	{
		return std::nullopt;
	}
	return ImVec2(screen.x, screen.y);
}

glm::vec3 OnLand(glm::vec2 point)
{
	const float height = Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt(point) : 0.0f;
	return {point.x, height, point.y};
}

/// A line between two points of the world, where both are on the screen
void Line(ImDrawList& draw, const glm::vec3& from, const glm::vec3& to, ImU32 colour, float thickness = 1.5f)
{
	const auto a = OnScreen(from);
	const auto b = OnScreen(to);
	if (a.has_value() && b.has_value())
	{
		draw.AddLine(*a, *b, colour, thickness);
	}
}

void Button(bool enabled, const char* label, const auto& pressed)
{
	ImGui::BeginDisabled(!enabled);
	if (ImGui::Button(label))
	{
		pressed();
	}
	ImGui::EndDisabled();
}
} // namespace

TempleVillagers::TempleVillagers() noexcept
    : Window(std::string(k_TempleVillagersWindow), ImVec2(560.0f, 720.0f))
{
}

void TempleVillagers::UpdateAlways() noexcept
{
	if (!Locator::entitiesRegistry::has_value())
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	// Those made last frame set off for their town's worship site as the game sends a villager
	for (const auto villager : std::exchange(_toSend, {}))
	{
		if (registry.Valid(villager) && registry.AllOf<ecs::components::LivingAction>(villager))
		{
			ecs::villager_worship::CheckWorshipActivity(registry.Get<ecs::components::LivingAction>(villager), false);
		}
	}
	std::erase_if(_spawned, [&registry](entt::entity villager) { return !registry.Valid(villager); });
	if (_selected.has_value() && !registry.Valid(*_selected))
	{
		_selected.reset();
		_follow = false;
	}
	// A step runs until its turn has gone, then the game is paused again
	if (_stepUntil.has_value() && Locator::time::has_value())
	{
		auto& time = Locator::time::value();
		if (time.GetTurn() >= *_stepUntil)
		{
			time.SetPaused(true);
			_stepUntil.reset();
		}
	}
	if (_follow && _selected.has_value())
	{
		if (const auto* transform = registry.TryGet<const ecs::components::Transform>(*_selected))
		{
			LookAt(transform->position, k_LookDistance);
		}
	}
}

void TempleVillagers::LookAt(const glm::vec3& point, float distance) noexcept
{
	if (!Locator::camera::has_value())
	{
		return;
	}
	auto& camera = Locator::camera::value();
	auto away = camera.GetOrigin() - camera.GetFocus();
	const float length = glm::length(away);
	away = length > 0.0f ? away * (std::min(length, distance) / length) : glm::vec3(0.0f, 30.0f, -30.0f);
	camera.Place(point + away, point);
}

void TempleVillagers::Draw() noexcept
{
	const auto survey = Take(k_Player, _radius, _floors);
	if (survey.temple == entt::null)
	{
		ImGui::TextWrapped("The player has no temple on this land. The testbed scenario debug.temple_villagers makes one "
		                   "with a town that worships at it.");
		return;
	}
	DrawTemple(survey);
	DrawRun();
	DrawSites(survey);
	DrawTowns(survey);
	DrawSpawn(survey);
	DrawVillagers(survey);
	DrawSelected(survey);
	DrawInside(survey);
	if (_marks)
	{
		DrawMarks(survey);
	}
}

void TempleVillagers::DrawTemple(const Survey& survey) noexcept
{
	ImGui::Text("Temple %u at (%.1f, %.1f, %.1f)", Id(survey.temple), static_cast<double>(survey.templePosition.x),
	            static_cast<double>(survey.templePosition.y), static_cast<double>(survey.templePosition.z));
	ImGui::SameLine();
	if (ImGui::SmallButton("Look at temple"))
	{
		_follow = false;
		LookAt(survey.templePosition, k_TempleLookDistance);
	}
	if (Locator::temple::has_value())
	{
		auto& temple = Locator::temple::value();
		ImGui::SameLine();
		if (ImGui::SmallButton(temple.Active() ? "Leave temple" : "Enter temple"))
		{
			if (temple.Active())
			{
				temple.Deactivate();
			}
			else
			{
				temple.Activate();
			}
		}
	}
	ImGui::Checkbox("Mark in the world", &_marks);
	ImGui::SameLine();
	ImGui::Checkbox("Site places", &_sitePlaces);
	ImGui::SameLine();
	ImGui::Checkbox("Walks", &_walks);
	ImGui::SameLine();
	ImGui::Checkbox("Temple floors", &_floors);
}

void TempleVillagers::DrawRun() noexcept
{
	if (!Locator::time::has_value())
	{
		return;
	}
	auto& time = Locator::time::value();
	ImGui::Text("Turn %u%s", time.GetTurn(), time.IsPaused() ? ", paused" : "");
	ImGui::SameLine();
	if (ImGui::SmallButton(time.IsPaused() ? "Resume" : "Pause"))
	{
		_stepUntil.reset();
		time.SetPaused(!time.IsPaused());
	}
	ImGui::SameLine();
	if (ImGui::SmallButton("Step a turn"))
	{
		_stepUntil = time.GetTurn() + 1;
		time.SetPaused(false);
	}
	ImGui::SameLine();
	if (ImGui::SmallButton("Step ten turns"))
	{
		_stepUntil = time.GetTurn() + 10;
		time.SetPaused(false);
	}
}

void TempleVillagers::DrawSites(const Survey& survey) noexcept
{
	if (!ImGui::CollapsingHeader(fmt::format("Worship sites ({})###sites", survey.sites.size()).c_str(),
	                             ImGuiTreeNodeFlags_DefaultOpen))
	{
		return;
	}
	if (!ImGui::BeginTable("sites", 7, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit))
	{
		return;
	}
	for (const auto* heading : {"Site", "Tribe", "Place", "Built", "Dancers", "Worshippers", "Food"})
	{
		ImGui::TableSetupColumn(heading);
	}
	ImGui::TableHeadersRow();
	for (const auto& site : survey.sites)
	{
		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::PushID(static_cast<int>(Id(site.site)));
		if (ImGui::SmallButton(fmt::format("{}", Id(site.site)).c_str()))
		{
			_follow = false;
			if (const auto& centre = site.places.at(static_cast<size_t>(SitePlace::Centre)); centre.has_value())
			{
				LookAt(OnLand(*centre), k_LookDistance);
			}
		}
		ImGui::PopID();
		ImGui::TableNextColumn();
		ImGui::TextUnformatted(TribeName(site.tribe).data());
		ImGui::TableNextColumn();
		ImGui::Text("%u", site.place);
		ImGui::TableNextColumn();
		ImGui::TextUnformatted(site.built ? "yes" : "no");
		ImGui::TableNextColumn();
		ImGui::Text("%u", site.dancers);
		ImGui::TableNextColumn();
		ImGui::Text("%zu", site.worshippers.size());
		ImGui::TableNextColumn();
		ImGui::Text("%u", site.foodInPot);
	}
	ImGui::EndTable();
}

void TempleVillagers::DrawTowns(const Survey& survey) noexcept
{
	if (!ImGui::CollapsingHeader(fmt::format("Towns worshipping here ({})###towns", survey.towns.size()).c_str(),
	                             ImGuiTreeNodeFlags_DefaultOpen))
	{
		return;
	}
	for (const auto& town : survey.towns)
	{
		ImGui::PushID(static_cast<int>(Id(town.town)));
		ImGui::Text("Town %u: %u people, %u worshipping, %u on the way, wants %d more", town.id, town.population,
		            town.worshipping, town.onTheWay, town.needed);
		// The share is set as the town's totem sets it, which sends people or calls them back
		float share = town.share;
		ImGui::SetNextItemWidth(200.0f);
		ImGui::SliderFloat("Worship share", &share, 0.0f, 1.0f, "%.2f");
		if (ImGui::IsItemDeactivatedAfterEdit() && Locator::villageTotemSystem::has_value())
		{
			Locator::villageTotemSystem::value().SetTownShare(town.town, share);
		}
		ImGui::SameLine();
		if (ImGui::SmallButton("Send one"))
		{
			ecs::villager_worship::AdjustWorshippers(town.town, 1, true, false);
		}
		ImGui::SameLine();
		if (ImGui::SmallButton("Call one back"))
		{
			ecs::villager_worship::AdjustWorshippers(town.town, -1, true, false);
		}
		ImGui::PopID();
	}
}

void TempleVillagers::DrawSpawn(const Survey& survey) noexcept
{
	if (!ImGui::CollapsingHeader("Make villagers", ImGuiTreeNodeFlags_DefaultOpen))
	{
		return;
	}
	if (survey.towns.empty())
	{
		ImGui::TextWrapped("None of the player's towns worships at this temple: villagers made now would have nowhere to "
		                   "worship.");
		return;
	}
	_town = std::min(_town, survey.towns.size() - 1);
	if (ImGui::BeginCombo("Town", fmt::format("Town {}", survey.towns.at(_town).id).c_str()))
	{
		for (size_t i = 0; i < survey.towns.size(); ++i)
		{
			if (ImGui::Selectable(fmt::format("Town {}", survey.towns.at(i).id).c_str(), i == _town))
			{
				_town = i;
			}
		}
		ImGui::EndCombo();
	}
	if (ImGui::BeginCombo("Tribe", TribeName(_tribe).data()))
	{
		for (size_t i = 0; i < static_cast<size_t>(Tribe::_COUNT); ++i)
		{
			const auto tribe = static_cast<Tribe>(i);
			if (ImGui::Selectable(TribeName(tribe).data(), tribe == _tribe))
			{
				_tribe = tribe;
			}
		}
		ImGui::EndCombo();
	}
	if (ImGui::BeginCombo("Job", JobName(_number).data()))
	{
		for (size_t i = 0; i < static_cast<size_t>(VillagerNumber::_COUNT); ++i)
		{
			const auto number = static_cast<VillagerNumber>(i);
			if (ImGui::Selectable(JobName(number).data(), number == _number))
			{
				_number = number;
			}
		}
		ImGui::EndCombo();
	}
	ImGui::SliderInt("Age", &_age, 1, 80);
	ImGui::SliderInt("How many", &_count, 1, 32);
	ImGui::Checkbox("Send them to worship", &_sendOnSpawn);
	if (ImGui::Button("Make"))
	{
		Spawn(survey.towns.at(_town));
	}
	ImGui::SameLine();
	Button(!_spawned.empty(), fmt::format("Take away the {} made here", _spawned.size()).c_str(), [this] { ClearSpawned(); });
}

void TempleVillagers::Spawn(const temple_villagers::Town& town) noexcept
{
	if (!town.home.has_value() || !Locator::infoConstants::has_value())
	{
		return;
	}
	VillagerInfo type;
	try
	{
		type = GVillagerInfo::Find(_tribe, _number);
	}
	catch (const std::runtime_error&)
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	const auto home = glm::xz(*town.home);
	for (int i = 0; i < _count; ++i)
	{
		const float angle = 2.0f * std::numbers::pi_v<float> * static_cast<float>(i) / static_cast<float>(_count);
		const auto at = OnLand(home + k_SpawnRing * glm::vec2(std::cos(angle), std::sin(angle)));
		const auto villager =
		    ecs::archetypes::VillagerArchetype::Create(*town.home, at, type, static_cast<uint32_t>(std::max(_age, 1)));
		// It joins the town nearest its home, which is the town asked for
		if (auto* person = registry.TryGet<ecs::components::Villager>(villager); person != nullptr && person->town != town.town)
		{
			person->town = town.town;
		}
		_spawned.push_back(villager);
		if (_sendOnSpawn)
		{
			_toSend.push_back(villager);
		}
	}
}

void TempleVillagers::ClearSpawned() noexcept
{
	auto& registry = Locator::entitiesRegistry::value();
	for (const auto villager : std::exchange(_spawned, {}))
	{
		if (registry.Valid(villager))
		{
			// It leaves its site and the dance as a worshipper leaving its town does, then the towns forget it
			ecs::villager_worship::LeaveTown(villager);
			ecs::villager_worship::Forget(villager);
			// Out of the physics and silent first, then out of the world as the game takes things out
			if (Locator::dynamicsSystem::has_value())
			{
				Locator::dynamicsSystem::value().RemoveObject(villager, false, false);
			}
			if (Locator::audio::has_value())
			{
				Locator::audio::value().StopOwnedSounds(villager);
			}
			editor::Remove(villager);
		}
	}
	_toSend.clear();
}

void TempleVillagers::DrawVillagers(const Survey& survey) noexcept
{
	if (!ImGui::CollapsingHeader(fmt::format("Villagers ({})###villagers", survey.villagers.size()).c_str(),
	                             ImGuiTreeNodeFlags_DefaultOpen))
	{
		return;
	}
	ImGui::SetNextItemWidth(160.0f);
	ImGui::SliderFloat("Nearby within (m)", &_radius, 0.0f, 300.0f, "%.0f");
	ImGui::Checkbox("Worshipping", &_showWorshipping);
	ImGui::SameLine();
	ImGui::Checkbox("On the way", &_showOnTheWay);
	ImGui::SameLine();
	ImGui::Checkbox("Nearby", &_showNearby);
	const auto shown = [this](Standing standing) {
		return (standing == Standing::Worshipping && _showWorshipping) || (standing == Standing::OnTheWay && _showOnTheWay) ||
		       (standing == Standing::Nearby && _showNearby);
	};
	constexpr auto k_Flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY |
	                         ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_Resizable;
	if (!ImGui::BeginTable("villagers", 7, k_Flags, ImVec2(0.0f, 220.0f)))
	{
		return;
	}
	ImGui::TableSetupScrollFreeze(0, 1);
	for (const auto* heading : {"Id", "Standing", "Who", "Doing", "Turns", "Away", "Going to"})
	{
		ImGui::TableSetupColumn(heading);
	}
	ImGui::TableHeadersRow();
	for (const auto& villager : survey.villagers)
	{
		if (!shown(villager.standing))
		{
			continue;
		}
		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::PushID(static_cast<int>(Id(villager.villager)));
		if (ImGui::Selectable(fmt::format("{}", Id(villager.villager)).c_str(), _selected == villager.villager,
		                      ImGuiSelectableFlags_SpanAllColumns))
		{
			_selected = villager.villager;
		}
		ImGui::PopID();
		ImGui::TableNextColumn();
		ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(ColourOf(villager.standing)), "%s", Name(villager.standing).data());
		ImGui::TableNextColumn();
		ImGui::Text("%s %s%s", TribeName(villager.tribe).data(), JobName(villager.number).data(),
		            villager.child ? " (child)" : "");
		ImGui::TableNextColumn();
		ImGui::TextUnformatted(StateName(villager.final).data());
		ImGui::TableNextColumn();
		ImGui::Text("%u", villager.turnsInState);
		ImGui::TableNextColumn();
		ImGui::Text("%.0f m", static_cast<double>(villager.distance));
		ImGui::TableNextColumn();
		if (villager.walk.has_value() && !villager.walk->moveState.empty() && villager.walk->moveState != "arrived")
		{
			ImGui::Text("(%.0f, %.0f) %s", static_cast<double>(villager.walk->goal.x),
			            static_cast<double>(villager.walk->goal.y), villager.walk->moveState.data());
		}
		else
		{
			ImGui::TextUnformatted("-");
		}
	}
	ImGui::EndTable();
}

void TempleVillagers::DrawSelected(const Survey& survey) noexcept
{
	if (!_selected.has_value())
	{
		return;
	}
	const auto it = std::ranges::find(survey.villagers, *_selected, &Villager::villager);
	if (it == survey.villagers.end())
	{
		ImGui::Text("Villager %u is no longer around the temple", Id(*_selected));
		ImGui::SameLine();
		if (ImGui::SmallButton("Forget it"))
		{
			_selected.reset();
			_follow = false;
		}
		return;
	}
	const auto& villager = *it;
	ImGui::SeparatorText(fmt::format("Villager {}", Id(villager.villager)).c_str());
	ImGui::Text("%s %s %s, %u years old, life %.2f, town %u, site %u", TribeName(villager.tribe).data(),
	            villager.male ? "man" : "woman", JobName(villager.number).data(), villager.age,
	            static_cast<double>(villager.life), Id(villager.town), Id(villager.site));
	ImGui::Text("State: %s (top %s), %u turns", StateName(villager.final).data(), StateName(villager.top).data(),
	            villager.turnsInState);
	ImGui::Text("At (%.1f, %.1f, %.1f), %.1f m from the temple", static_cast<double>(villager.position.x),
	            static_cast<double>(villager.position.y), static_cast<double>(villager.position.z),
	            static_cast<double>(villager.distance));
	if (villager.walk.has_value())
	{
		const auto& walk = *villager.walk;
		ImGui::Text("Walk: %s to (%.1f, %.1f) at %.2f m/s", walk.moveState.empty() ? "standing" : walk.moveState.data(),
		            static_cast<double>(walk.goal.x), static_cast<double>(walk.goal.y), static_cast<double>(walk.speed));
		if (walk.stepGoal.has_value())
		{
			ImGui::Text("Next step to (%.1f, %.1f)", static_cast<double>(walk.stepGoal->x),
			            static_cast<double>(walk.stepGoal->y));
		}
		if (walk.circleCentre.has_value())
		{
			ImGui::Text("Going round a circle at (%.1f, %.1f), %.1f m across", static_cast<double>(walk.circleCentre->x),
			            static_cast<double>(walk.circleCentre->y), static_cast<double>(walk.circleRadius * 2.0f));
		}
	}
	if (ImGui::SmallButton("Look at it"))
	{
		_follow = false;
		LookAt(villager.position, k_LookDistance);
	}
	ImGui::SameLine();
	ImGui::Checkbox("Follow it", &_follow);
	ImGui::SameLine();
	if (ImGui::SmallButton("Send to worship") && Locator::entitiesRegistry::has_value())
	{
		auto& registry = Locator::entitiesRegistry::value();
		if (auto* action = registry.TryGet<ecs::components::LivingAction>(villager.villager))
		{
			ecs::villager_worship::CheckWorshipActivity(*action, false);
		}
	}
	ImGui::SameLine();
	if (ImGui::SmallButton("Deselect"))
	{
		_selected.reset();
		_follow = false;
	}
}

void TempleVillagers::DrawInside(const Survey& survey) noexcept
{
	if (!ImGui::CollapsingHeader(fmt::format("Inside the temple ({})###inside", survey.inside.walkers.size()).c_str()))
	{
		return;
	}
	if (!survey.inside.active)
	{
		ImGui::TextWrapped("The player is outside. Going in brings the site's worshippers in with them, to walk about "
		                   "the temple's floors until the player leaves.");
		return;
	}
	if (!ImGui::BeginTable("inside", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY,
	                       ImVec2(0.0f, 160.0f)))
	{
		return;
	}
	for (const auto* heading : {"Came in as", "Doing", "For", "Going to"})
	{
		ImGui::TableSetupColumn(heading);
	}
	ImGui::TableHeadersRow();
	for (const auto& walker : survey.inside.walkers)
	{
		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::Text("%u", Id(walker.villager));
		ImGui::TableNextColumn();
		ImGui::TextUnformatted(walker.walking ? "walking" : "standing");
		ImGui::TableNextColumn();
		if (walker.walking)
		{
			ImGui::Text("%.1f s", static_cast<double>(walker.timer));
		}
		else
		{
			ImGui::Text("%.1f of %.1f s", static_cast<double>(walker.timer), static_cast<double>(walker.pause));
		}
		ImGui::TableNextColumn();
		ImGui::Text("(%.1f, %.1f, %.1f)", static_cast<double>(walker.target.x), static_cast<double>(walker.target.y),
		            static_cast<double>(walker.target.z));
	}
	ImGui::EndTable();
}

void TempleVillagers::DrawMarks(const Survey& survey) const noexcept
{
	auto& draw = *ImGui::GetBackgroundDrawList();
	if (survey.inside.active)
	{
		// Inside, the floors and those walking them
		for (size_t i = 0; i + 1 < survey.inside.floorEdges.size(); i += 2)
		{
			Line(draw, survey.inside.floorEdges[i], survey.inside.floorEdges[i + 1], k_FloorColour, 1.0f);
		}
		for (const auto& walker : survey.inside.walkers)
		{
			if (_walks && walker.walking)
			{
				Line(draw, walker.position, walker.target, k_InsideColour);
			}
			if (const auto at = OnScreen(walker.position))
			{
				draw.AddCircle(*at, 6.0f, k_InsideColour, 0, 2.0f);
			}
		}
		return;
	}
	if (_sitePlaces)
	{
		for (const auto& site : survey.sites)
		{
			for (size_t i = 0; i < site.places.size(); ++i)
			{
				if (!site.places.at(i).has_value())
				{
					continue;
				}
				if (const auto at = OnScreen(OnLand(*site.places.at(i))))
				{
					draw.AddRect(ImVec2(at->x - 4.0f, at->y - 4.0f), ImVec2(at->x + 4.0f, at->y + 4.0f), k_PlaceColour);
					draw.AddText(ImVec2(at->x + 6.0f, at->y - 7.0f), k_PlaceColour, Name(static_cast<SitePlace>(i)).data());
				}
			}
		}
	}
	for (const auto& villager : survey.villagers)
	{
		const bool selected = _selected == villager.villager;
		const auto colour = selected ? k_SelectedColour : ColourOf(villager.standing);
		if (_walks && villager.walk.has_value() && !villager.walk->moveState.empty() && villager.walk->moveState != "arrived")
		{
			const auto goal = OnLand(villager.walk->goal);
			if (villager.walk->stepGoal.has_value())
			{
				const auto step = OnLand(*villager.walk->stepGoal);
				Line(draw, villager.position, step, colour);
				Line(draw, step, goal, colour, 1.0f);
			}
			else
			{
				Line(draw, villager.position, goal, colour);
			}
		}
		if (const auto at = OnScreen(villager.position))
		{
			draw.AddCircle(*at, selected ? 10.0f : 6.0f, colour, 0, selected ? 3.0f : 2.0f);
			if (selected)
			{
				draw.AddText(ImVec2(at->x + 12.0f, at->y - 7.0f), colour,
				             fmt::format("{} {}", Id(villager.villager), StateName(villager.final)).c_str());
			}
		}
	}
}
