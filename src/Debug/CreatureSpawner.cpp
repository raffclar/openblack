/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureSpawner.h"

#include <algorithm>
#include <array>
#include <functional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <SDL_events.h>
#include <fmt/format.h>
#include <glm/gtx/vec_swizzle.hpp>
#include <glm/trigonometric.hpp>

#include "3D/CreatureBody.h"
#include "3D/LandIslandInterface.h"
#include "Camera/Camera.h"
#include "Common/DeveloperToolsGate.h"
#include "Creature/CreatureDesires.h"
#include "Creature/CreatureFace.h"
#include "Creature/CreatureIdleMind.h"
#include "Creature/CreatureLayers.h"
#include "Creature/CreatureLocomotion.h"
#include "Creature/CreatureLook.h"
#include "Creature/CreatureMorph.h"
#include "Creature/CreatureSkin.h"
#include "Creature/LeashRules.h"
#include "ECS/Archetypes/CreatureArchetype.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureLocomotion.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/CreatureSkin.h"
#include "ECS/Components/Transform.h"
#include "ECS/CreatureReactionNotice.h"
#include "ECS/CreatureRemoval.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CreatureHairSystemInterface.h"
#include "ECS/Systems/CreatureLocomotionSystemInterface.h"
#include "ECS/Systems/CreatureMindSystemInterface.h"
#include "ECS/Systems/CreatureObjectActionSystemInterface.h"
#include "ECS/Systems/LeashSystemInterface.h"
#include "Editor/EditorCreatureActs.h"
#include "Editor/EditorCreatureEdits.h"
#include "Editor/EditorMindLoads.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Windowing/WindowingInterface.h"

using namespace openblack;
using namespace openblack::debug::gui;
using openblack::ecs::archetypes::CreatureArchetype;
using openblack::ecs::components::Creature;
using openblack::ecs::components::CreatureAnimation;
using openblack::ecs::components::CreatureLocomotion;
using openblack::ecs::components::CreatureMindState;
using openblack::ecs::components::CreatureMorph;
using openblack::ecs::components::CreatureTattoos;
using openblack::ecs::components::Transform;

namespace
{
// Indexed by species, from the cow
constexpr std::array<std::string_view, static_cast<size_t>(CreatureType::_COUNT) - 1> k_SpeciesNames {
    "Cow",        "Tiger", "Leopard", "Wolf", "Lion",     "Horse", "Tortoise", "Zebra",     "Brown Bear",
    "Polar Bear", "Sheep", "Chimp",   "Ogre", "Mandrill", "Rhino", "Gorilla",  "Giant Ape",
};

constexpr std::array<std::string_view, static_cast<size_t>(PlayerNames::_COUNT)> k_OwnerNames {
    "Player One", "Player Two", "Player Three", "Player Four", "Player Five", "Player Six", "Player Seven", "Neutral",
};

const ImVec4 k_PlacingColour {0.85f, 0.30f, 0.25f, 1.0f};
/// A right click picks out the thing to pick up or knock down nearest it, this close at most
constexpr float k_PickRadius = 12.0f;
const ImVec4 k_StartColour {0.25f, 0.60f, 0.30f, 1.0f};

const GCreatureInfo* SpeciesInfo(CreatureType species)
{
	if (!Locator::infoConstants::has_value())
	{
		return nullptr;
	}
	const auto& creatures = Locator::infoConstants::value().creature;
	const auto row = creature::InfoRow(species);
	return row < creatures.size() ? &creatures.at(row) : nullptr;
}

/// An animation's place in the creature spec and its name
std::string AnimationLabel(size_t animation)
{
	const auto name = creature_layers::animations::Name(animation);
	return name.empty() ? fmt::format("{}", animation) : fmt::format("{} {}", animation, name);
}

std::string_view MotionName(CreatureLocomotion::Motion motion)
{
	constexpr std::array<std::string_view, 7> k_Names {
	    "Standing", "Planning a route", "Confused", "Turning", "Stepping off", "Walking", "Pointing at the route"};
	return k_Names.at(static_cast<size_t>(motion));
}

std::string_view MoveResultName(ecs::systems::CreatureLocomotionSystemInterface::MoveResult result)
{
	using MoveResult = ecs::systems::CreatureLocomotionSystemInterface::MoveResult;
	switch (result)
	{
	case MoveResult::InvalidDestination:
		return "nowhere to stand there";
	case MoveResult::Busy:
		return "it can't walk";
	case MoveResult::Started:
	default:
		return "on its way";
	}
}

bool IsRightButton(const SDL_Event& event)
{
	return (event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEBUTTONUP) && event.button.button == SDL_BUTTON_RIGHT;
}
} // namespace

CreatureSpawner::CreatureSpawner() noexcept
    : Window("Creature Spawner", ImVec2(420.0f, 520.0f))
{
}

std::string_view CreatureSpawner::SpeciesName(CreatureType species) noexcept
{
	const auto index = static_cast<size_t>(species);
	return index >= 1 && index <= k_SpeciesNames.size() ? k_SpeciesNames.at(index - 1) : "Unknown";
}

void CreatureSpawner::Close() noexcept
{
	_placing = false;
	_placeAt.reset();
	_commanding = false;
	_commandAt.reset();
	Window::Close();
}

void CreatureSpawner::Select(entt::entity entity) noexcept
{
	_selected = entity;
	Open();
}

void CreatureSpawner::Draw() noexcept
{
	if (!ImGui::BeginTabBar("CreatureSpawnerTabs"))
	{
		return;
	}
	if (ImGui::BeginTabItem("Spawn"))
	{
		DrawSpawnMind();
		DrawSettings();
		ImGui::Separator();
		DrawPlacing();
		ImGui::EndTabItem();
	}
	if (ImGui::BeginTabItem("Creatures"))
	{
		DrawCreatures();
		ImGui::EndTabItem();
	}
	// Selecting a creature brings its tab to the front, once
	const bool justSelected = _selected.has_value() && _selected != _tabFor;
	_tabFor = _selected;
	if (_selected.has_value() &&
	    ImGui::BeginTabItem("Selected", nullptr, justSelected ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None))
	{
		DrawSelected();
		ImGui::EndTabItem();
	}
	ImGui::EndTabBar();
	DrawFileBrowser();
}

void CreatureSpawner::UseSpeciesDefaults() noexcept
{
	if (!Locator::infoConstants::has_value())
	{
		return;
	}
	// The random spread of the body uses the synced random numbers of the game. In a network game the window must not
	// change the synced world of this computer only, so it shows the body of the species without the spread.
	const auto body =
	    developer_tools::NetworkSessionLive() ? CreatureArchetype::BaseBody(_species) : CreatureArchetype::StartBody(_species);
	_alignment = body.alignment;
	_fatness = body.fatness;
	_strength = body.strength;
	_scale = CreatureArchetype::StartScale(_species);
	_defaultsFor = _species;
}

void CreatureSpawner::DrawSettings() noexcept
{
	if (_defaultsFor != _species)
	{
		UseSpeciesDefaults();
	}

	ImGui::SeparatorText("Creature");

	if (ImGui::BeginCombo("Species", SpeciesName(_species).data()))
	{
		for (size_t i = 0; i < k_SpeciesNames.size(); ++i)
		{
			const auto species = static_cast<CreatureType>(i + 1);
			if (ImGui::Selectable(k_SpeciesNames.at(i).data(), species == _species))
			{
				_species = species;
			}
		}
		ImGui::EndCombo();
	}

	if (ImGui::BeginCombo("Owner", k_OwnerNames.at(static_cast<size_t>(_owner)).data()))
	{
		for (size_t i = 0; i < k_OwnerNames.size(); ++i)
		{
			const auto owner = static_cast<PlayerNames>(i);
			if (ImGui::Selectable(k_OwnerNames.at(i).data(), owner == _owner))
			{
				_owner = owner;
			}
		}
		ImGui::EndCombo();
	}

	ImGui::SeparatorText("Body");
	ImGui::SliderFloat("Alignment", &_alignment, -1.0f, 1.0f,
	                   _alignment < 0.0f ? "Evil %.2f" : (_alignment > 0.0f ? "Good %.2f" : "Neutral %.2f"));
	ImGui::SliderFloat("Fatness", &_fatness, 0.0f, 1.0f, "%.2f");
	ImGui::SliderFloat("Strength", &_strength, 0.0f, 1.0f, "%.2f");
	ImGui::SliderFloat("Size", &_scale, creature_morph::k_MinScale, creature_morph::k_MaxScale, "%.2f",
	                   ImGuiSliderFlags_Logarithmic);
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Creatures grow by themselves up to size %.1f", static_cast<double>(creature_morph::k_MaxGrownScale));
	}
	if (ImGui::Button("Species defaults"))
	{
		UseSpeciesDefaults();
	}
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("As a new creature of the species is: neutral, with its size, fatness and strength");
	}

	const auto* info = SpeciesInfo(_species);
	const auto morph = creature_morph::FromAttributes(
	    _alignment, _fatness, _strength, info != nullptr ? info->strength : creature_morph::k_UnknownSpeciesStrength);
	ImGui::Text("Evil-good %+.2f, thin-fat %+.2f, weak-strong %+.2f", static_cast<double>(morph.evilGood),
	            static_cast<double>(morph.thinFat), static_cast<double>(morph.weakStrong));
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("How far the body is pulled from its base mesh towards each axis' mesh, -1 to 1");
	}

	ImGui::SeparatorText("Facing");
	ImGui::Checkbox("Random", &_randomFacing);
	if (!_randomFacing)
	{
		ImGui::SliderFloat("Degrees", &_facingDegrees, 0.0f, 360.0f, "%.0f");
	}
}

bool CreatureSpawner::InNetworkGame() noexcept
{
	return developer_tools::NetworkSessionLive();
}

float CreatureSpawner::ShownValue(entt::entity entity, creature_edit_values::Value value, float now) const noexcept
{
	return _held.has_value() && _held->entity == entity && _held->value == value ? _held->amount : now;
}

void CreatureSpawner::FollowValueSlider(entt::entity entity, creature_edit_values::Value value, float amount, bool changed,
                                        bool released) noexcept
{
	if (changed)
	{
		_held = HeldValue {.entity = entity, .value = value, .amount = amount};
	}
	if (released && _held.has_value() && _held->entity == entity && _held->value == value)
	{
		editor::SendCreatureEdit(entity, value, _held->amount);
		_held.reset();
	}
}

void CreatureSpawner::DrawSelected() noexcept
{
	if (!_selected.has_value() || !Locator::entitiesRegistry::has_value())
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	auto* creature = registry.TryGet<Creature>(*_selected);
	auto* transform = registry.TryGet<Transform>(*_selected);
	if (creature == nullptr || transform == nullptr)
	{
		_selected.reset();
		return;
	}

	ImGui::SeparatorText("Selected creature");
	ImGui::Text("%s at %.0f, %.0f", SpeciesName(creature->species).data(), static_cast<double>(transform->position.x),
	            static_cast<double>(transform->position.z));
	const bool network = InNetworkGame();
	const auto entity = *_selected;
	using creature_edit_values::Value;
	// In a network game each slider sends its amount as a command of the turns when the player lets go of it
	const auto slider = [this, network, entity](const char* label, Value value, float& field, float low, float high,
	                                            ImGuiSliderFlags flags) {
		if (!network)
		{
			return ImGui::SliderFloat(label, &field, low, high, "%.2f", flags);
		}
		auto amount = ShownValue(entity, value, field);
		const bool changed = ImGui::SliderFloat(label, &amount, low, high, "%.2f", flags);
		FollowValueSlider(entity, value, amount, changed, ImGui::IsItemDeactivatedAfterEdit());
		return false;
	};
	slider("Its alignment", Value::Alignment, creature->alignment, -1.0f, 1.0f, ImGuiSliderFlags_None);
	slider("Its fatness", Value::Fatness, creature->fatness, 0.0f, 1.0f, ImGuiSliderFlags_None);
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Its body follows its fatness by at most %.2f a game turn",
		                  static_cast<double>(creature_morph::k_MaxFatnessStep));
	}
	if (slider("Its strength", Value::Strength, creature->strength, 0.0f, 1.0f, ImGuiSliderFlags_None))
	{
		// Set here, as a script sets it, the body shows it
		CreatureArchetype::ShowStrength(entity);
	}
	if (slider("Its size", Value::Size, creature->size, creature_morph::k_MinScale, creature_morph::k_MaxScale,
	           ImGuiSliderFlags_Logarithmic))
	{
		creature->size = creature_morph::ClampScale(creature->size);
		transform->scale = glm::vec3(CreatureArchetype::DrawnScale(creature->species, creature->size));
		registry.SetDirty();
	}
	if (auto* morph = registry.TryGet<CreatureMorph>(*_selected); morph != nullptr)
	{
		ImGui::Text("Drawn: evil-good %+.2f, thin-fat %+.2f, weak-strong %+.2f", static_cast<double>(morph->drawn.evilGood),
		            static_cast<double>(morph->drawn.thinFat), static_cast<double>(morph->drawn.weakStrong));
		const auto skinWeight = creature_skin::BlendWeight(morph->drawn.evilGood);
		ImGui::Text("Skin %u of %u towards %s", static_cast<uint32_t>(skinWeight),
		            static_cast<uint32_t>(creature_skin::k_MaxWeight), morph->drawn.evilGood < 0.0f ? "evil" : "good");
		ImGui::Text("Fatness shown %.2f", static_cast<double>(morph->shownFatness));
		ImGui::SameLine();
		if (ImGui::SmallButton("Show now"))
		{
			static_cast<void>(editor::DoCreatureAct(editor::ActOf(entity, creature_edit_acts::Act::ShowFatnessNow)));
		}
	}
	if (ImGui::Button("Deselect"))
	{
		_selected.reset();
	}
	if (!_selected.has_value() || !ImGui::BeginTabBar("SelectedCreatureTabs"))
	{
		return;
	}
	const std::array<std::pair<const char*, void (CreatureSpawner::*)(entt::entity) noexcept>, 9> k_Tabs {{
	    {"Looks", &CreatureSpawner::DrawAppearance},
	    {"Audio", &CreatureSpawner::DrawAudio},
	    {"Movement", &CreatureSpawner::DrawMovement},
	    {"Hands", &CreatureSpawner::DrawHands},
	    {"Mind", &CreatureSpawner::DrawMind},
	    {"Learning", &CreatureSpawner::DrawLearning},
	    {"Body", &CreatureSpawner::DrawBody},
	    {"Leash", &CreatureSpawner::DrawLeash},
	    {"Fight", &CreatureSpawner::DrawFight},
	}};
	for (const auto& [label, draw] : k_Tabs)
	{
		if (ImGui::BeginTabItem(label))
		{
			// In a network game each change of a tab goes as a command of the turns
			(this->*draw)(entity);
			ImGui::EndTabItem();
		}
	}
	ImGui::EndTabBar();
}

void CreatureSpawner::DrawMovement(entt::entity entity) noexcept
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* self = registry.TryGet<CreatureLocomotion>(entity);
	if (self == nullptr || !Locator::creatureLocomotionSystem::has_value())
	{
		return;
	}

	ImGui::SeparatorText("Movement");
	ImGui::Text("%s%s", MotionName(self->motion).data(), self->failed ? ", the last move failed" : "");
	ImGui::Text("Speed %.1f of walk %.1f, run %.1f units/s", static_cast<double>(self->speed),
	            static_cast<double>(self->speeds.walk), static_cast<double>(self->speeds.run));
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Asked for %.2f of its top speed, %.1f units/s", static_cast<double>(self->fraction),
		                  static_cast<double>(creature_locomotion::TargetSpeed(self->fraction, self->speeds.run)));
	}
	ImGui::Text("Heading %.0f degrees, radius %.1f, scale %.3f", static_cast<double>(glm::degrees(self->heading.Radians())),
	            static_cast<double>(self->radius), static_cast<double>(self->scale));
	if (const auto& route = self->follower.GetRoute(); !route.legs.empty())
	{
		ImGui::Text("Route of %zu legs, on leg %zu, %.1f units left", route.legs.size(), route.segment + 1,
		            static_cast<double>(route.RemainingTotal()));
	}
	if (self->follower.IsSearching())
	{
		ImGui::Text("Searching (%zu searches), %zu circles met", self->follower.GetSearchCount(),
		            self->follower.GetObstacles().All().size());
	}
	std::string legs;
	for (const auto& track : self->tracks)
	{
		legs += fmt::format("{}{} {:.2f}", legs.empty() ? "" : ", ", AnimationLabel(track.animation), track.weight);
	}
	ImGui::TextWrapped("Legs: %s", legs.empty() ? "as the body plays" : legs.c_str());

	const auto colour = _commanding ? k_PlacingColour : k_StartColour;
	ImGui::PushStyleColor(ImGuiCol_Button, colour);
	if (ImGui::Button(_commanding ? "Stop commanding" : "Command it", ImVec2(-1.0f, 0.0f)))
	{
		_commanding = !_commanding;
		_commandAt.reset();
		if (_commanding)
		{
			_placing = false;
			_placeAt.reset();
		}
	}
	ImGui::PopStyleColor();
	if (_commanding)
	{
		ImGui::TextWrapped("Right click on the land:");
		auto order = static_cast<int>(_order);
		ImGui::RadioButton("Walk here", &order, static_cast<int>(Order::Walk));
		ImGui::SameLine();
		ImGui::RadioButton("Run here", &order, static_cast<int>(Order::Run));
		ImGui::SameLine();
		ImGui::RadioButton("Flee from", &order, static_cast<int>(Order::Flee));
		ImGui::SameLine();
		ImGui::RadioButton("Face", &order, static_cast<int>(Order::Face));
		ImGui::RadioButton("Pick up", &order, static_cast<int>(Order::PickUp));
		ImGui::SameLine();
		ImGui::RadioButton("Throw at", &order, static_cast<int>(Order::Throw));
		ImGui::SameLine();
		ImGui::RadioButton("Knock down", &order, static_cast<int>(Order::Destroy));
		ImGui::SameLine();
		ImGui::RadioButton("Point at", &order, static_cast<int>(Order::Point));
		_order = static_cast<Order>(order);
		if (!_lastOrder.empty())
		{
			ImGui::TextUnformatted(_lastOrder.c_str());
		}
	}
	using creature_edit_acts::Act;
	if (ImGui::Button("Stop"))
	{
		static_cast<void>(editor::DoCreatureAct(editor::ActOf(entity, Act::StopMoving)));
	}
	ImGui::SameLine();
	if (ImGui::Button("Face the camera"))
	{
		// The editor gives the place of its camera, so that each computer turns the creature to the same place
		auto act = editor::ActOf(entity, Act::Face);
		act.position = Locator::camera::value().GetOrigin();
		static_cast<void>(editor::DoCreatureAct(act));
	}
	ImGui::SameLine();
	ImGui::Checkbox("Show route", &_showRoute);
	if (_showRoute)
	{
		DrawRoute(entity);
	}
}

void CreatureSpawner::DrawRoute(entt::entity entity) noexcept
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* self = registry.TryGet<const CreatureLocomotion>(entity);
	if (self == nullptr || self->follower.GetRoute().legs.empty() || !Locator::terrainSystem::has_value())
	{
		return;
	}
	const auto& land = Locator::terrainSystem::value();
	const auto& camera = Locator::camera::value();
	const auto display = ImGui::GetIO().DisplaySize;
	const glm::vec4 viewport {0.0f, 0.0f, display.x, display.y};
	auto* drawList = ImGui::GetBackgroundDrawList();
	const auto project = [&](glm::vec2 point) -> std::optional<ImVec2> {
		glm::vec3 screen;
		// A little above the ground, so the line isn't hidden in it
		const glm::vec3 world {point.x, land.GetHeightAt(point) + 0.5f, point.y};
		if (!camera.ProjectWorldToScreen(world, viewport, screen))
		{
			return std::nullopt;
		}
		return ImVec2(screen.x, screen.y);
	};
	// Gone over already in grey, still to go in yellow
	const auto& route = self->follower.GetRoute();
	const auto stepAngle = glm::radians(5.0f);
	std::vector<glm::vec2> points;
	for (size_t leg = 0; leg < route.legs.size(); ++leg)
	{
		const auto colour = leg < route.segment ? IM_COL32(160, 160, 160, 200) : IM_COL32(255, 220, 60, 255);
		const auto legPoints =
		    creature_route::Route {.legs = {route.legs[leg]}, .segment = 0, .travelled = 0.0f}.Points(stepAngle);
		for (size_t i = 1; i < legPoints.size(); ++i)
		{
			const auto from = project(legPoints[i - 1]);
			const auto to = project(legPoints[i]);
			if (from && to)
			{
				drawList->AddLine(*from, *to, colour, 2.5f);
			}
		}
		points.insert(points.end(), legPoints.begin(), legPoints.end());
	}
	if (const auto end = project(points.back()))
	{
		drawList->AddCircle(*end, 6.0f, IM_COL32(255, 120, 40, 255), 16, 2.0f);
	}
	if (self->destination.has_value())
	{
		if (const auto destination = project(*self->destination))
		{
			drawList->AddCircleFilled(*destination, 3.0f, IM_COL32(255, 80, 40, 255));
		}
	}
}

void CreatureSpawner::DrawMind(entt::entity entity) noexcept
{
	namespace animations = creature_layers::animations;
	auto& registry = Locator::entitiesRegistry::value();
	auto* mind = registry.TryGet<CreatureMindState>(entity);
	auto* animation = registry.TryGet<CreatureAnimation>(entity);
	if (mind == nullptr || animation == nullptr || !Locator::creatureMindSystem::has_value())
	{
		return;
	}

	ImGui::SeparatorText("Mind");
	// In a network game each change of the tab goes as a command of the turns
	const bool network = InNetworkGame();
	using creature_edit_acts::Act;
	const auto act = [](const player_commands::EditorCreatureAct& edit) { static_cast<void>(editor::DoCreatureAct(edit)); };
	bool paused = mind->paused;
	if (ImGui::Checkbox("Pause mind", &paused))
	{
		act(editor::SwitchOf(entity, Act::PauseMind, paused));
	}
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("The mind leaves the body alone; the buttons below still work");
	}
	bool notice = ecs::creature_reaction_notice::TakesNotice(registry, entity);
	if (ImGui::Checkbox("Takes notice of reactions", &notice))
	{
		act(editor::SwitchOf(entity, Act::TakesNotice, notice));
	}
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Debug only. A creature that a script makes takes no notice until a script or a swap tells "
		                  "it to: it reacts to nothing near it and catches nothing");
	}
	if (Locator::infoConstants::has_value())
	{
		const auto& phases = Locator::infoConstants::value().creatureDevelopmentPhaseEntry;
		using creature_edit_values::Value;
		const auto now = static_cast<float>(std::min<size_t>(mind->developmentPhase, phases.size() - 1));
		auto phase = static_cast<int>(network ? ShownValue(entity, Value::GrownUp, now) : now);
		const bool changed = ImGui::SliderInt("Grown up", &phase, 0, static_cast<int>(phases.size()) - 1,
		                                      phases.at(static_cast<size_t>(phase)).name.data());
		if (network)
		{
			FollowValueSlider(entity, Value::GrownUp, static_cast<float>(phase), changed, ImGui::IsItemDeactivatedAfterEdit());
		}
		else if (changed)
		{
			mind->developmentPhase = static_cast<uint32_t>(phase);
		}
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("Each stage of growing up brings desires and takes some away");
		}
	}
	const auto& idle = mind->idle;
	ImGui::Text("%s, step %zu of %zu", creature_mind::Name(idle.activity).data(), std::min(idle.step + 1, idle.agenda.size()),
	            idle.agenda.size());
	if (idle.step < idle.agenda.size())
	{
		const auto& step = idle.agenda[idle.step];
		switch (step.kind)
		{
		case creature_mind::Step::Kind::Wait:
			ImGui::Text("Waiting %.1f of %.1f s", static_cast<double>(idle.stepSeconds), static_cast<double>(step.seconds));
			break;
		case creature_mind::Step::Kind::Action:
			ImGui::Text("Action %s%s", AnimationLabel(step.animation).c_str(), step.sleepyEyes ? ", sleepy eyes" : "");
			break;
		case creature_mind::Step::Kind::Static:
			ImGui::Text("%s %.1f of %.1f s", AnimationLabel(step.sequence[1]).c_str(), static_cast<double>(idle.stepSeconds),
			            static_cast<double>(step.seconds));
			break;
		case creature_mind::Step::Kind::Move:
			ImGui::Text("Moving for %.1f s", static_cast<double>(idle.stepSeconds));
			break;
		case creature_mind::Step::Kind::Object:
			break;
		case creature_mind::Step::Kind::Cast:
			ImGui::Text("Casting for %.1f of %.1f s", static_cast<double>(idle.stepSeconds), static_cast<double>(step.seconds));
			break;
		case creature_mind::Step::Kind::Gesture:
			ImGui::Text("Drawing a gesture for %.1f s", static_cast<double>(idle.stepSeconds));
			break;
		case creature_mind::Step::Kind::WaitInMap:
			ImGui::Text("Waiting for a thing to land for %.1f s", static_cast<double>(idle.stepSeconds));
			break;
		case creature_mind::Step::Kind::Douse:
			ImGui::Text("Dousing a fire");
			break;
		case creature_mind::Step::Kind::CoolDown:
			ImGui::Text("Putting out its own fire");
			break;
		case creature_mind::Step::Kind::Shatter:
			ImGui::Text("Breaking a rock in two");
			break;
		case creature_mind::Step::Kind::Repair:
			ImGui::Text("Mending a building");
			break;
		case creature_mind::Step::Kind::UseUp:
			ImGui::Text("Using a thing up");
			break;
		case creature_mind::Step::Kind::UseUpHeld:
			ImGui::Text("Using up what it holds");
			break;
		case creature_mind::Step::Kind::ActOnItself:
			ImGui::Text("Acting on itself from now on");
			break;
		case creature_mind::Step::Kind::Teleport:
			ImGui::Text("Asking a teleport stone for a jump");
			break;
		case creature_mind::Step::Kind::Transported:
			ImGui::Text("Carried by a teleport stone for %.1f s", static_cast<double>(idle.stepSeconds));
			break;
		case creature_mind::Step::Kind::StopTeleportControl:
			ImGui::Text("Free of the teleport stone");
			break;
		case creature_mind::Step::Kind::Empty:
			ImGui::Text("Nothing to do");
			break;
		case creature_mind::Step::Kind::WatchFlying:
			ImGui::Text("Watching a thing fly for %.1f s", static_cast<double>(idle.stepSeconds));
			break;
		case creature_mind::Step::Kind::SwapMind:
			ImGui::Text("Swapping minds for %.1f s", static_cast<double>(idle.stepSeconds));
			break;
		case creature_mind::Step::Kind::Overlay:
			ImGui::Text("%s on top of its body", AnimationLabel(step.animation).c_str());
			break;
		case creature_mind::Step::Kind::Dance:
			ImGui::Text("Dancing with villagers, stage %u", static_cast<unsigned>(step.dance.kind));
			break;
		default:
			break;
		}
	}
	ImGui::Text("Last desire shown: %s", idle.shown.has_value() ? creature_desires::Name(*idle.shown).data() : "none");

	ImGui::SeparatorText("Body");
	const auto& body = animation->body;
	ImGui::Text("Playing %s, %.0f ms%s", AnimationLabel(creature_layers::CurrentAnimation(body)).c_str(),
	            static_cast<double>(body.timeMs), body.mirrored ? ", mirrored" : "");
	const auto& face = animation->face;
	ImGui::Text("Face %s%s%s, %.0f ms in", face.current ? AnimationLabel(*face.current).c_str() : "none",
	            face.wanted != face.current ? " -> " : "",
	            face.wanted != face.current ? (face.wanted ? AnimationLabel(*face.wanted).c_str() : "relaxing") : "",
	            static_cast<double>(face.timeMs));
	if (face.wanted.has_value())
	{
		ImGui::Text("  pulled for %s, held %.1f s more", creature_face::Name(face.cue).data(),
		            static_cast<double>(face.remainingMs) / 1000.0);
	}
	ImGui::Text("  next face in %.1f s, variety %u, attitude to the player %+.2f",
	            static_cast<double>(std::max(idle.faceSeconds, 0.0f)), idle.faceVariety,
	            static_cast<double>(mind->attitudeToPlayer));
	ImGui::Text("Gesture %s", animation->gesture.animation ? AnimationLabel(*animation->gesture.animation).c_str() : "none");
	if (mind->look.id.has_value() && animation->lookAt.has_value())
	{
		ImGui::Text("Looks at entity %u, of interest %.2f, for %.1f s", *mind->look.id,
		            static_cast<double>(mind->look.interest), static_cast<double>(mind->look.watchedTurns) / 10.0);
	}
	else
	{
		ImGui::Text("Looks %s", animation->lookAt.has_value() ? "ahead" : "nowhere in particular");
	}
	ImGui::Text("Head turned %+.0f degrees left, %+.0f up", static_cast<double>(glm::degrees(animation->yaw.angle)),
	            static_cast<double>(glm::degrees(animation->pitch.angle)));
	const auto pairs =
	    std::ranges::count_if(animation->mirror, [index = uint32_t {0}](uint32_t mirror) mutable { return mirror != index++; });
	ImGui::Text("%td bones have a mirror bone", pairs);

	ImGui::SeparatorText("Tell it");
	const bool busy = creature_layers::IsPlaying(body);
	if (ImGui::BeginCombo("Action", AnimationLabel(animations::k_FirstAction + _action).c_str()))
	{
		for (size_t i = 0; i < animations::k_ActionCount; ++i)
		{
			if (ImGui::Selectable(AnimationLabel(animations::k_FirstAction + i).c_str(), i == _action))
			{
				_action = i;
			}
		}
		ImGui::EndCombo();
	}
	ImGui::SameLine();
	ImGui::BeginDisabled(busy);
	if (ImGui::Button("Play"))
	{
		act(editor::IndexOf(entity, Act::PlayAction, _action));
	}
	ImGui::EndDisabled();
	if (ImGui::BeginCombo("Gesture", AnimationLabel(animations::k_FirstGesture + _gesture).c_str()))
	{
		for (size_t i = 0; i < animations::k_GestureCount; ++i)
		{
			if (ImGui::Selectable(AnimationLabel(animations::k_FirstGesture + i).c_str(), i == _gesture))
			{
				_gesture = i;
			}
		}
		ImGui::EndCombo();
	}
	ImGui::SameLine();
	ImGui::BeginDisabled(animation->gesture.animation.has_value());
	if (ImGui::Button("Gesture"))
	{
		act(editor::IndexOf(entity, Act::PlayGesture, _gesture));
	}
	ImGui::EndDisabled();
	ImGui::TextUnformatted("Face");
	for (size_t i = 0; i < animations::k_FaceCount; ++i)
	{
		if (i % 6 != 0)
		{
			ImGui::SameLine();
		}
		if (ImGui::SmallButton(AnimationLabel(animations::k_FirstFace + i).c_str()))
		{
			act(editor::IndexOf(entity, Act::PullFace, i));
		}
	}
	ImGui::TextUnformatted("Feel");
	ImGui::SetItemTooltip("Pulls the face the creature's mind would pull for a feeling or what it is doing");
	constexpr std::array k_Feelings {
	    creature_face::Cue::Idle,        creature_face::Cue::AttitudeToPlayer,
	    creature_face::Cue::Curiosity,   creature_face::Cue::Anger,
	    creature_face::Cue::Fear,        creature_face::Cue::Compassion,
	    creature_face::Cue::Playfulness, creature_face::Cue::Smile,
	    creature_face::Cue::Grimace,     creature_face::Cue::Amazed,
	    creature_face::Cue::Puzzled,     creature_face::Cue::Frightened,
	    creature_face::Cue::Sad,         creature_face::Cue::Exhausted,
	};
	for (size_t i = 0; i < k_Feelings.size(); ++i)
	{
		if (i % 5 != 0)
		{
			ImGui::SameLine();
		}
		if (ImGui::SmallButton(std::string(creature_face::Name(k_Feelings.at(i))).c_str()))
		{
			act(editor::IndexOf(entity, Act::ShowFeeling, static_cast<size_t>(k_Feelings.at(i))));
		}
	}
	ImGui::BeginDisabled(busy);
	if (ImGui::Button("Sit down"))
	{
		act(editor::ActOf(entity, Act::SitDown));
	}
	ImGui::EndDisabled();
	ImGui::SameLine();
	ImGui::BeginDisabled(!creature_layers::IsLooping(body));
	if (ImGui::Button("Stand up"))
	{
		act(editor::ActOf(entity, Act::StandUp));
	}
	ImGui::EndDisabled();
	ImGui::SameLine();
	if (ImGui::Button("Stroke"))
	{
		act({.creature = entity, .act = static_cast<uint8_t>(Act::Feedback), .amount = 0.5f});
	}
	ImGui::SameLine();
	if (ImGui::Button("Slap"))
	{
		act({.creature = entity, .act = static_cast<uint8_t>(Act::Feedback), .amount = -0.5f});
	}

	ImGui::SeparatorText("Desires");
	ImGui::Text("Turns unwatched %u, turns since the player dealt with it %u", mind->attention.turnsIgnored,
	            mind->attention.turnsSinceInteraction);
	if (!mind->desires.has_value())
	{
		return;
	}
	ImGui::Text("All desires add up to %.2f", static_cast<double>(mind->desires->sum));
	// Strongest first
	std::array<creature_desires::Desire, creature_desires::k_DesireCount> order {};
	for (size_t i = 0; i < order.size(); ++i)
	{
		order.at(i) = static_cast<creature_desires::Desire>(i);
	}
	std::ranges::stable_sort(order, std::greater {},
	                         [&desires = *mind->desires](creature_desires::Desire desire) { return desires[desire].value; });
	for (const auto desire : order)
	{
		const auto& state = (*mind->desires)[desire];
		if (!state.activated || (state.sources.empty() && state.value <= 0.0f))
		{
			continue;
		}
		const auto label = fmt::format("{:.2f} / {:.2f}{}", state.value, state.max, state.suppressedTurns > 0 ? " held" : "");
		ImGui::ProgressBar(state.max > 0.0f ? state.value / state.max : 0.0f, ImVec2(160.0f, 0.0f), label.c_str());
		ImGui::SameLine();
		ImGui::TextUnformatted(creature_desires::Name(desire).data());
		if (ImGui::IsItemHovered() && !state.sources.empty())
		{
			std::string sources;
			for (const auto& source : state.sources)
			{
				sources += fmt::format("source {}: {:.2f} past {:.2f}\n", source.type, source.value, source.threshold);
			}
			ImGui::SetTooltip("%s", sources.c_str());
		}
	}
}

void CreatureSpawner::DrawPlacing() noexcept
{
	const auto colour = _placing ? k_PlacingColour : k_StartColour;
	ImGui::PushStyleColor(ImGuiCol_Button, colour);
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(colour.x * 1.15f, colour.y * 1.15f, colour.z * 1.15f, 1.0f));
	if (ImGui::Button(_placing ? "Stop Placing Creatures" : "Start Placing Creatures", ImVec2(-1.0f, 0.0f)))
	{
		_placing = !_placing;
		_placeAt.reset();
		if (_placing)
		{
			_commanding = false;
			_commandAt.reset();
		}
	}
	ImGui::PopStyleColor(2);
	if (_placing)
	{
		ImGui::TextWrapped("Right click on the land to place a creature there");
	}
}

void CreatureSpawner::DrawCreatures() noexcept
{
	if (!Locator::entitiesRegistry::has_value())
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();

	std::vector<entt::entity> creatures;
	registry.Each<const Creature>([&creatures](entt::entity entity, const Creature&) { creatures.push_back(entity); });

	ImGui::SeparatorText("On the land");
	DrawSharedSettings();
	ImGui::Text("%zu creature%s", creatures.size(), creatures.size() == 1 ? "" : "s");
	if (_selected.has_value() && std::ranges::find(creatures, *_selected) == creatures.end())
	{
		_selected.reset();
	}
	ImGui::SameLine();
	ImGui::BeginDisabled(creatures.empty());
	if (ImGui::Button("Remove all"))
	{
		// Each goes through the game's removal, which lets go of its leash, fights, hand and lists
		for (const auto creature : creatures)
		{
			ecs::creature_removal::RemoveFromGame(creature);
		}
		creatures.clear();
		_selected.reset();
	}
	ImGui::EndDisabled();

	std::optional<entt::entity> remove;
	if (ImGui::BeginTable("Creatures", 5, ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingStretchProp,
	                      ImVec2(0.0f, 0.0f)))
	{
		ImGui::TableSetupColumn("Species");
		ImGui::TableSetupColumn("Owner");
		ImGui::TableSetupColumn("Where");
		ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed);
		ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed);
		ImGui::TableHeadersRow();
		for (const auto entity : creatures)
		{
			const auto& creature = registry.Get<Creature>(entity);
			const auto& transform = registry.Get<Transform>(entity);
			ImGui::PushID(static_cast<int>(entt::to_integral(entity)));
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::TextUnformatted(SpeciesName(creature.species).data());
			ImGui::TableNextColumn();
			ImGui::TextUnformatted(k_OwnerNames.at(static_cast<size_t>(creature.owner)).data());
			ImGui::TableNextColumn();
			ImGui::Text("%.0f, %.0f", static_cast<double>(transform.position.x), static_cast<double>(transform.position.z));
			ImGui::TableNextColumn();
			const bool selected = _selected == entity;
			if (ImGui::SmallButton(selected ? "Selected" : "Select"))
			{
				_selected = entity;
			}
			ImGui::TableNextColumn();
			if (ImGui::SmallButton("Remove"))
			{
				remove = entity;
			}
			ImGui::PopID();
		}
		ImGui::EndTable();
	}
	if (remove.has_value())
	{
		if (_selected == *remove)
		{
			_selected.reset();
		}
		ecs::creature_removal::RemoveFromGame(*remove);
	}
}

void CreatureSpawner::DrawSharedSettings() noexcept
{
	if (Locator::creatureHairSystem::has_value())
	{
		auto& hair = Locator::creatureHairSystem::value();
		bool shown = hair.IsShown();
		if (ImGui::Checkbox("Show hair", &shown))
		{
			hair.SetShown(shown);
		}
	}
	DrawAudioSettings();
	DrawFootprintSettings();
}

void CreatureSpawner::Update() noexcept
{
	if (_placeAt.has_value())
	{
		Spawn(*_placeAt);
		_placeAt.reset();
	}
	if (_commandAt.has_value())
	{
		Command(*_commandAt);
		_commandAt.reset();
	}
}

std::optional<entt::entity> CreatureSpawner::NearestToOrder(glm::vec2 point) const noexcept
{
	auto& hands = Locator::creatureObjectActionSystem::value();
	auto& registry = Locator::entitiesRegistry::value();
	const bool pickUp = _order == Order::PickUp;
	std::optional<entt::entity> nearest;
	float best = k_PickRadius;
	registry.Each<const Transform>([&](entt::entity entity, const Transform& at) {
		const auto distance = glm::distance(glm::xz(at.position), point);
		if (distance < best && entity != *_selected &&
		    (pickUp ? hands.CanPickUp(*_selected, entity) : hands.CanDestroy(entity)))
		{
			best = distance;
			nearest = entity;
		}
	});
	return nearest;
}

void CreatureSpawner::SendCommand(glm::vec3 point) noexcept
{
	using creature_edit_acts::Act;
	player_commands::EditorCreatureAct act {.creature = *_selected, .position = point};
	switch (_order)
	{
	case Order::Walk:
	case Order::Run:
		act.act = static_cast<uint8_t>(Act::GoTo);
		act.args[0] = _order == Order::Run ? 1 : 0;
		break;
	case Order::Flee:
		act.act = static_cast<uint8_t>(Act::FleeFrom);
		break;
	case Order::Face:
		act.act = static_cast<uint8_t>(Act::Face);
		break;
	case Order::PickUp:
	case Order::Destroy:
	{
		// The editor finds the thing, so that each computer acts on the same thing
		const auto nearest = NearestToOrder(glm::xz(point));
		if (!nearest.has_value())
		{
			_lastOrder = fmt::format("Nothing to {} there", _order == Order::PickUp ? "pick up" : "knock down");
			return;
		}
		act.act = static_cast<uint8_t>(_order == Order::PickUp ? Act::PickUp : Act::KnockDown);
		act.other = *nearest;
		break;
	}
	case Order::Throw:
		act.act = static_cast<uint8_t>(Act::ThrowAt);
		break;
	case Order::Point:
		act.act = static_cast<uint8_t>(Act::PointAt);
		break;
	}
	static_cast<void>(editor::DoCreatureAct(act));
	_lastOrder = fmt::format("Sent the order to {:.0f}, {:.0f}: it comes at the next turn", point.x, point.z);
}

void CreatureSpawner::Command(glm::vec2 screenCoord) noexcept
{
	if (!_selected.has_value() || !Locator::creatureLocomotionSystem::has_value())
	{
		return;
	}
	const auto hit = Locator::camera::value().RaycastScreenCoordToLand(screenCoord, false);
	if (!hit.has_value())
	{
		return;
	}
	using Pace = ecs::systems::CreatureLocomotionSystemInterface::Pace;
	auto& locomotion = Locator::creatureLocomotionSystem::value();
	const auto point = glm::xz(hit->position);
	if (InNetworkGame())
	{
		SendCommand(hit->position);
		return;
	}
	switch (_order)
	{
	case Order::Walk:
	case Order::Run:
	{
		const auto result = locomotion.MoveTo(*_selected, point, _order == Order::Run ? Pace::Run : Pace::Walk, 0.0f, 1.0f);
		_lastOrder = fmt::format("To {:.0f}, {:.0f}: {}", point.x, point.y, MoveResultName(result));
		break;
	}
	case Order::Flee:
		_lastOrder = fmt::format("Away from {:.0f}, {:.0f}: {}", point.x, point.y,
		                         MoveResultName(locomotion.FleeFrom(*_selected, point, Pace::Run)));
		break;
	case Order::Face:
		_lastOrder = fmt::format("Facing {:.0f}, {:.0f}: {}", point.x, point.y,
		                         locomotion.TurnToFace(*_selected, point) ? "turning" : "can't");
		break;
	case Order::PickUp:
	case Order::Destroy:
	{
		// What is nearest where the land was clicked
		auto& hands = Locator::creatureObjectActionSystem::value();
		const bool pickUp = _order == Order::PickUp;
		const auto nearest = NearestToOrder(point);
		if (!nearest.has_value())
		{
			_lastOrder = fmt::format("Nothing to {} there", pickUp ? "pick up" : "knock down");
			break;
		}
		const auto started = pickUp ? hands.PickUp(*_selected, *nearest) : hands.Destroy(*_selected, *nearest);
		_lastOrder = fmt::format("{} entity {}: {}", pickUp ? "Picking up" : "Knocking down", entt::to_integral(*nearest),
		                         started ? "started" : "can't");
		break;
	}
	case Order::Throw:
		_lastOrder =
		    fmt::format("Throwing at {:.0f}, {:.0f}: {}", point.x, point.y,
		                Locator::creatureObjectActionSystem::value().Throw(*_selected, hit->position) ? "started" : "can't");
		break;
	case Order::Point:
		_lastOrder =
		    fmt::format("Pointing at {:.0f}, {:.0f}: {}", point.x, point.y,
		                Locator::creatureObjectActionSystem::value().PointAt(*_selected, hit->position) ? "started" : "can't");
		break;
	}
}

void CreatureSpawner::Spawn(glm::vec2 screenCoord) noexcept
{
	if (!Locator::terrainSystem::has_value() || !Locator::dynamicsSystem::has_value())
	{
		return;
	}
	const auto hit = Locator::camera::value().RaycastScreenCoordToLand(screenCoord, false);
	if (!hit.has_value())
	{
		return;
	}
	SpawnAt(hit->position);
}

entt::entity CreatureSpawner::SpawnAt(const glm::vec3& position) noexcept
{
	const auto degrees = _randomFacing ? std::uniform_real_distribution(0.0f, 360.0f)(_random) : _facingDegrees;
	return SpawnAt(position, glm::radians(degrees));
}

entt::entity CreatureSpawner::SpawnAt(glm::vec3 position, float yawRadians) noexcept
{
	// In a network game each computer makes the creature at the next turn. A mind file goes to every computer first, and
	// the new creature takes the mind of the file at that turn.
	if (InNetworkGame())
	{
		const player_commands::EditorCreatureSpawn spawn {.species = static_cast<uint8_t>(_species),
		                                                  .owner = static_cast<uint8_t>(_owner),
		                                                  .position = position,
		                                                  .radians = yawRadians,
		                                                  .scale = _scale,
		                                                  .alignment = _alignment,
		                                                  .fatness = _fatness,
		                                                  .strength = _strength,
		                                                  .hasTattoos = _spawnTattoos.has_value(),
		                                                  .tattoos = _spawnTattoos.value_or(creature_tattoo::Slots {})};
		if (_spawnMind == nullptr || _spawnMindBytes.empty())
		{
			editor::SendCreatureSpawn(spawn);
		}
		else if (!editor::mind_loads::SendMindLoad(_spawnMindBytes,
		                                           {.use = player_commands::EditorMindLoad::Use::Spawn, .spawn = spawn}))
		{
			_lastMindFile = "The game socket is not open: the mind file did not go";
		}
		return entt::null;
	}
	const auto entity = CreatureArchetype::Create(position, _owner, _species, 0, yawRadians, _scale,
	                                              {.alignment = _alignment, .fatness = _fatness, .strength = _strength});
	auto& registry = Locator::entitiesRegistry::value();
	if (Locator::leashSystem::has_value())
	{
		// Made for trying things out, it knows every leash, as creatures made by the original's debug tools do
		for (const auto type : creature_leash::k_Types)
		{
			Locator::leashSystem::value().SetKnown(entity, type, true);
		}
	}
	if (_spawnMind != nullptr && Locator::creatureMindSystem::has_value())
	{
		// The mind is taken up on the creature's first turn of thought
		Locator::creatureMindSystem::value().LoadMind(entity, _spawnMind);
	}
	if (auto* tattoos = registry.TryGet<CreatureTattoos>(entity); tattoos != nullptr && _spawnTattoos.has_value())
	{
		tattoos->slots = *_spawnTattoos;
		++tattoos->revision;
	}
	return entity;
}

bool CreatureSpawner::TakesEvent(const SDL_Event& event) const noexcept
{
	// While placing or commanding, a right click on the land is the window's rather than the hand's
	return (_placing || (_commanding && _selected.has_value())) && IsRightButton(event) && !ImGui::GetIO().WantCaptureMouse;
}

void CreatureSpawner::ProcessEventOpen(const SDL_Event& event) noexcept
{
	if (!TakesEvent(event) || event.type != SDL_MOUSEBUTTONDOWN || !Locator::windowing::has_value())
	{
		return;
	}
	const auto size = static_cast<glm::vec2>(Locator::windowing::value().GetSize());
	const auto at = glm::vec2(static_cast<float>(event.button.x), static_cast<float>(event.button.y)) / size;
	if (_placing)
	{
		_placeAt = at;
	}
	else
	{
		_commandAt = at;
	}
}

void CreatureSpawner::ProcessEventAlways([[maybe_unused]] const SDL_Event& event) noexcept {}
