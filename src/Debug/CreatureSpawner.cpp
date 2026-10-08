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
#include <SDL_mouse.h>
#include <fmt/format.h>
#include <glm/gtx/vec_swizzle.hpp>
#include <glm/trigonometric.hpp>
#include <imgui.h>

#include "3D/CreatureBody.h"
#include "3D/LandIslandInterface.h"
#include "Camera/Camera.h"
#include "Creature/CreatureDesires.h"
#include "Creature/CreatureFace.h"
#include "Creature/CreatureIdleMind.h"
#include "Creature/CreatureLayers.h"
#include "Creature/CreatureLocomotion.h"
#include "Creature/CreatureLook.h"
#include "Creature/CreatureMindTables.h"
#include "Creature/CreatureMorph.h"
#include "Creature/CreatureSkin.h"
#include "Creature/LeashRules.h"
#include "ECS/Archetypes/CreatureArchetype.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureDrawPose.h"
#include "ECS/Components/CreatureLeash.h"
#include "ECS/Components/CreatureLocomotion.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/CreatureNeeds.h"
#include "ECS/Components/CreatureSkin.h"
#include "ECS/Components/Transform.h"
#include "ECS/MobileDrawing.h"
#include "ECS/PlayerCreature.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CreatureHairSystemInterface.h"
#include "ECS/Systems/CreatureHandSystemInterface.h"
#include "ECS/Systems/CreatureLocomotionSystemInterface.h"
#include "ECS/Systems/CreatureMindSystemInterface.h"
#include "ECS/Systems/CreatureObjectActionSystemInterface.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/LeashSystemInterface.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "MiraclesModel.h"
#include "RoutePlanner/RouteFollower.h"

using namespace openblack;
using namespace openblack::debug::gui;
using namespace openblack::debug::creature_spawner;
using openblack::ecs::archetypes::CreatureArchetype;
using openblack::ecs::components::Creature;
using openblack::ecs::components::CreatureAnimation;
using openblack::ecs::components::CreatureDrawPose;
using openblack::ecs::components::CreatureLeash;
using openblack::ecs::components::CreatureLocomotion;
using openblack::ecs::components::CreatureMindState;
using openblack::ecs::components::CreatureMorph;
using openblack::ecs::components::CreatureNeeds;
using openblack::ecs::components::CreatureTattoos;
using openblack::ecs::components::Transform;

namespace
{
const ImVec4 k_PlacingColour {0.85f, 0.30f, 0.25f, 1.0f};
const ImVec4 k_StartColour {0.25f, 0.60f, 0.30f, 1.0f};
/// A click picks out the thing to pick up or knock down nearest it, this close at most
constexpr float k_PickRadius = 12.0f;
/// The player's creature section lists this many of its strongest desires
constexpr size_t k_TopDesires = 5;

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

std::string_view LookKindName(creature_look::Interest kind)
{
	constexpr std::array<std::string_view, 8> k_Names {"dove",     "citadel", "creature", "animal",
	                                                   "villager", "abode",   "tree",     "fixed object"};
	return k_Names.at(static_cast<size_t>(kind));
}

std::string_view MotionName(CreatureLocomotion::Motion motion)
{
	constexpr std::array<std::string_view, 6> k_Names {"Standing", "Planning a route", "Confused",
	                                                   "Turning",  "Stepping off",     "Walking"};
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

bool IsLeftClick(const SDL_Event& event)
{
	return (event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEBUTTONUP) && event.button.button == SDL_BUTTON_LEFT;
}

std::string ActionName(const creature_mind_tables::Tables* tables, uint32_t action)
{
	return tables != nullptr && action < tables->actions.size() ? tables->actions[action].name : fmt::format("#{}", action);
}

/// Where the player's hand is, on the land
std::optional<glm::vec3> HandOnLand()
{
	if (!Locator::handSystem::has_value())
	{
		return std::nullopt;
	}
	using Side = ecs::systems::HandSystemInterface::Side;
	const auto positions = Locator::handSystem::value().GetPlayerHandPositions();
	auto point = debug::miracles::HandPoint(positions.at(static_cast<size_t>(Side::Left)),
	                                        positions.at(static_cast<size_t>(Side::Right)));
	if (point.has_value() && Locator::terrainSystem::has_value())
	{
		*point = OnGround(*point, Locator::terrainSystem::value().GetHeightAt(glm::xz(*point)));
	}
	return point;
}
} // namespace

CreatureSpawner::CreatureSpawner() noexcept
    : Window("Creature Spawner", ImVec2(460.0f, 620.0f))
{
}

void CreatureSpawner::Close() noexcept
{
	_placing = false;
	_commanding = false;
	_clicked = false;
	Window::Close();
}

void CreatureSpawner::Draw() noexcept
{
	if (!Locator::entitiesRegistry::has_value())
	{
		ImGui::TextUnformatted("No land loaded");
		return;
	}
	DrawPlayersCreature();
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
}

void CreatureSpawner::DrawPlayersCreature() noexcept
{
	if (!ImGui::CollapsingHeader("Player's creature", ImGuiTreeNodeFlags_DefaultOpen))
	{
		return;
	}
	if (!Locator::leashSystem::has_value())
	{
		ImGui::TextUnformatted("No leash system, so no player's creature");
		return;
	}
	auto& leashes = Locator::leashSystem::value();
	auto& registry = Locator::entitiesRegistry::value();
	const auto& view = std::as_const(registry);
	const auto found = leashes.PlayersCreature(PlayerNames::PLAYER_ONE);
	const auto* creature = found.has_value() ? view.TryGet<const Creature>(*found) : nullptr;
	const auto* transform = found.has_value() ? view.TryGet<const Transform>(*found) : nullptr;
	if (creature == nullptr || transform == nullptr)
	{
		ImGui::TextUnformatted("Player one has no creature");
		return;
	}
	const auto entity = *found;
	const auto* pose = view.TryGet<const CreatureDrawPose>(entity);
	const auto* mind = view.TryGet<const CreatureMindState>(entity);
	const auto* leash = view.TryGet<const CreatureLeash>(entity);
	const auto* needs = view.TryGet<const CreatureNeeds>(entity);
	const auto summary = Summarise(
	    {
	        .entity = entity,
	        .species = creature->species,
	        .size = creature->size,
	        .alignment = creature->alignment,
	        .leashable = creature->leashable,
	        .position = transform->position,
	        .ownScale = transform->scale,
	        .poseScale = pose != nullptr ? pose->scale : std::nullopt,
	        .developmentPhase = mind != nullptr ? std::optional(mind->developmentPhase) : std::nullopt,
	        .home = leash != nullptr ? leash->home : std::nullopt,
	        .wornLeash = leash != nullptr && leash->worn.has_value() ? std::optional(leash->worn->type) : std::nullopt,
	        .knownLeashes = leash != nullptr ? leash->known : decltype(leash->known) {},
	        .desires = mind != nullptr && mind->desires.has_value() ? &*mind->desires : nullptr,
	        .plan = mind != nullptr ? mind->planner.current : std::nullopt,
	        .needs = needs != nullptr ? &needs->needs : nullptr,
	    },
	    k_TopDesires);

	ImGui::Text("%s, entity %u, at %.1f, %.1f, %.1f", SpeciesName(summary.species).data(), entt::to_integral(entity),
	            static_cast<double>(summary.position.x), static_cast<double>(summary.position.y),
	            static_cast<double>(summary.position.z));
	ImGui::SameLine();
	if (ImGui::SmallButton("Select"))
	{
		_selected = entity;
	}
	ImGui::Text("Size %.2f, drawn at %.3f%s", static_cast<double>(summary.size), static_cast<double>(summary.drawnScale.x),
	            pose != nullptr && pose->scale.has_value() ? " (in its pen)" : "");
	ImGui::Text("Alignment %+.2f", static_cast<double>(summary.alignment));
	if (summary.developmentPhase.has_value())
	{
		ImGui::Text("Stage of growing up %u of %d", *summary.developmentPhase, ecs::player_creature::k_LastDevelopmentStage);
	}
	if (summary.home.has_value())
	{
		ImGui::Text("Home at %.1f, %.1f", static_cast<double>(summary.home->x), static_cast<double>(summary.home->z));
	}
	else
	{
		ImGui::TextUnformatted("No home");
	}
	ImGui::Text("Leash: %s, knows %s, %s",
	            summary.wornLeash.has_value() ? creature_leash::Name(*summary.wornLeash) : "none worn",
	            summary.knownLeashes.c_str(), summary.leashable ? "leashable" : "not leashable");

	std::string desires;
	if (mind != nullptr && mind->desires.has_value())
	{
		for (const auto desire : summary.desires)
		{
			desires += fmt::format("{}{} {:.2f}", desires.empty() ? "" : ", ", creature_desires::Name(desire),
			                       (*mind->desires)[desire].value);
		}
	}
	ImGui::TextWrapped("Desires: %s", desires.empty() ? "none yet" : desires.c_str());
	if (summary.plan.has_value())
	{
		const auto* tables =
		    Locator::creatureMindSystem::has_value() ? Locator::creatureMindSystem::value().GetTables() : nullptr;
		ImGui::Text("Plan: %s for %s, priority %.1f", ActionName(tables, summary.plan->action).c_str(),
		            creature_desires::Name(summary.plan->desire).data(), static_cast<double>(summary.plan->priority));
	}
	else if (mind != nullptr)
	{
		ImGui::Text("Doing: %s", creature_mind::Name(mind->idle.activity).data());
	}
	if (summary.needs.has_value())
	{
		const auto& body = *summary.needs;
		ImGui::Text("Energy %.2f, exhaustion %.2f, thirst %.2f, poo %.2f, life %.2f, age %u", static_cast<double>(body.energy),
		            static_cast<double>(body.exhaustion), static_cast<double>(body.dehydration), static_cast<double>(body.poo),
		            static_cast<double>(body.life), body.age);
	}
	if (Locator::creatureHandSystem::has_value())
	{
		const auto& hand = Locator::creatureHandSystem::value();
		const auto held = hand.GetCreature();
		const auto under = hand.CreatureUnderHand();
		ImGui::Text("Hand: %s, under it %s",
		            held.has_value() ? fmt::format("holding creature {}", entt::to_integral(*held)).c_str()
		                             : "holding no creature",
		            under.has_value() ? fmt::format("creature {}", entt::to_integral(*under)).c_str() : "no creature");
	}

	if (ImGui::Button("Bring to the hand"))
	{
		if (const auto hand = HandOnLand())
		{
			if (auto* moved = Find<Transform>(registry, entity))
			{
				moved->position = *hand;
				ecs::NotifyTeleported(entity);
				_lastPlayers = fmt::format("Brought to {:.0f}, {:.0f}", hand->x, hand->z);
			}
		}
		else
		{
			_lastPlayers = "The hand is not on the land";
		}
	}
	ImGui::SameLine();
	ImGui::SetNextItemWidth(120.0f);
	ImGui::SliderInt("##phase", &_phase, 0, ecs::player_creature::k_LastDevelopmentStage);
	ImGui::SameLine();
	if (ImGui::Button("Set stage"))
	{
		const auto phase = ClampPhase(_phase, ecs::player_creature::k_LastDevelopmentStage);
		ecs::player_creature::SetDevelopmentStage(registry, entity, phase);
		_lastPlayers = fmt::format("Stage of growing up set to {}", phase);
	}
	ImGui::TextUnformatted("Knows");
	for (const auto type : creature_leash::k_Types)
	{
		ImGui::SameLine();
		bool known = leashes.Knows(entity, type);
		if (ImGui::Checkbox(fmt::format("{}##players", creature_leash::Name(type)).c_str(), &known))
		{
			leashes.SetKnown(entity, type, known);
		}
	}
	ImGui::SameLine();
	bool leashable = leashes.IsLeashable(entity);
	if (ImGui::Checkbox("Leashable##players", &leashable))
	{
		leashes.SetLeashable(entity, leashable);
	}
	if (!_lastPlayers.empty())
	{
		ImGui::TextUnformatted(_lastPlayers.c_str());
	}
}

void CreatureSpawner::UseSpeciesDefaults() noexcept
{
	if (!Locator::infoConstants::has_value())
	{
		return;
	}
	const auto body = CreatureArchetype::StartBody(_species);
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
		for (size_t i = 0; i < k_SpeciesCount; ++i)
		{
			const auto species = SpeciesAt(i);
			if (ImGui::Selectable(SpeciesName(species).data(), species == _species))
			{
				_species = species;
			}
		}
		ImGui::EndCombo();
	}

	if (ImGui::BeginCombo("Owner", OwnerName(_owner).data()))
	{
		for (size_t i = 0; i < static_cast<size_t>(PlayerNames::_COUNT); ++i)
		{
			const auto owner = static_cast<PlayerNames>(i);
			if (ImGui::Selectable(OwnerName(owner).data(), owner == _owner))
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

void CreatureSpawner::DrawSelected() noexcept
{
	if (!_selected.has_value())
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	auto* creature = Find<Creature>(registry, *_selected);
	auto* transform = Find<Transform>(registry, *_selected);
	if (creature == nullptr || transform == nullptr)
	{
		_selected.reset();
		return;
	}

	ImGui::SeparatorText("Selected creature");
	ImGui::Text("%s at %.0f, %.0f", SpeciesName(creature->species).data(), static_cast<double>(transform->position.x),
	            static_cast<double>(transform->position.z));
	ImGui::SliderFloat("Its alignment", &creature->alignment, -1.0f, 1.0f, "%.2f");
	ImGui::SliderFloat("Its fatness", &creature->fatness, 0.0f, 1.0f, "%.2f");
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Its body follows its fatness by at most %.2f a game turn",
		                  static_cast<double>(creature_morph::k_MaxFatnessStep));
	}
	ImGui::SliderFloat("Its strength", &creature->strength, 0.0f, 1.0f, "%.2f");
	if (ImGui::SliderFloat("Its size", &creature->size, creature_morph::k_MinScale, creature_morph::k_MaxScale, "%.2f",
	                       ImGuiSliderFlags_Logarithmic))
	{
		creature->size = creature_morph::ClampScale(creature->size);
		transform->scale = glm::vec3(CreatureArchetype::DrawnScale(creature->species, creature->size));
		registry.SetDirty();
	}
	if (auto* morph = Find<CreatureMorph>(registry, *_selected); morph != nullptr)
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
			morph->shownFatness = creature->fatness;
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
	const auto entity = *_selected;
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
			(this->*draw)(entity);
			ImGui::EndTabItem();
		}
	}
	ImGui::EndTabBar();
}

void CreatureSpawner::DrawMovement(entt::entity entity) noexcept
{
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	const auto* self = registry.TryGet<const CreatureLocomotion>(entity);
	if (self == nullptr || !Locator::creatureLocomotionSystem::has_value())
	{
		return;
	}
	auto& locomotion = Locator::creatureLocomotionSystem::value();

	ImGui::SeparatorText("Movement");
	ImGui::Text("%s%s", MotionName(self->motion).data(), self->failed ? ", the last move failed" : "");
	ImGui::Text("Speed %.1f of walk %.1f, run %.1f units/s", static_cast<double>(self->speed),
	            static_cast<double>(self->speeds.walk), static_cast<double>(self->speeds.run));
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Asked for %.2f of its top speed, %.1f units/s", static_cast<double>(self->fraction),
		                  static_cast<double>(creature_locomotion::TargetSpeed(self->fraction, self->speeds.run)));
	}
	ImGui::Text("Heading %.0f degrees, radius %.1f, scale %.3f", static_cast<double>(glm::degrees(self->heading)),
	            static_cast<double>(self->radius), static_cast<double>(self->scale));
	if (self->follower != nullptr)
	{
		const auto& follower = *self->follower;
		if (follower.HasRoute())
		{
			ImGui::Text("Route of %zu legs ahead, %.1f units left", follower.GetRouteAhead().size(),
			            static_cast<double>(follower.RemainingLength()));
		}
		else if (!self->routeReady && self->destination.has_value())
		{
			ImGui::Text("Planning, %d plans so far", follower.GetPlanCount());
		}
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
		_clicked = false;
		if (_commanding)
		{
			_placing = false;
		}
	}
	ImGui::PopStyleColor();
	if (_commanding)
	{
		ImGui::TextWrapped("Left click on the land:");
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
	if (ImGui::Button("Stop"))
	{
		locomotion.Stop(entity);
	}
	ImGui::SameLine();
	if (ImGui::Button("Face the camera") && Locator::camera::has_value())
	{
		locomotion.TurnToFace(entity, glm::xz(Locator::camera::value().GetOrigin()));
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
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	const auto* self = registry.TryGet<const CreatureLocomotion>(entity);
	const auto* transform = registry.TryGet<const Transform>(entity);
	if (self == nullptr || transform == nullptr || self->follower == nullptr || !self->follower->HasRoute() ||
	    !Locator::terrainSystem::has_value() || !Locator::camera::has_value())
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
	// From where it stands along each leg of the route still to go
	auto from = project(glm::xz(transform->position));
	for (const auto& node : self->follower->GetRouteAhead())
	{
		const auto to = project({node.to.x, node.to.z});
		if (from && to)
		{
			drawList->AddLine(*from, *to, IM_COL32(255, 220, 60, 255), 2.5f);
		}
		from = to;
	}
	if (from)
	{
		drawList->AddCircle(*from, 6.0f, IM_COL32(255, 120, 40, 255), 16, 2.0f);
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
	auto* mind = Find<CreatureMindState>(registry, entity);
	const auto* animation = std::as_const(registry).TryGet<const CreatureAnimation>(entity);
	if (mind == nullptr || animation == nullptr || !Locator::creatureMindSystem::has_value())
	{
		return;
	}
	auto& minds = Locator::creatureMindSystem::value();

	ImGui::SeparatorText("Mind");
	ImGui::Checkbox("Pause mind", &mind->paused);
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("The mind leaves the body alone; the buttons below still work");
	}
	if (Locator::infoConstants::has_value())
	{
		const auto& phases = Locator::infoConstants::value().creatureDevelopmentPhaseEntry;
		auto phase = static_cast<int>(std::min<size_t>(mind->developmentPhase, phases.size() - 1));
		if (ImGui::SliderInt("Grown up", &phase, 0, static_cast<int>(phases.size()) - 1,
		                     phases.at(static_cast<size_t>(phase)).name.data()))
		{
			ecs::player_creature::SetDevelopmentStage(registry, entity, phase);
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
		}
	}
	ImGui::Text("Shows a desire again in %.0f s%s%s", static_cast<double>(idle.showDesireSeconds),
	            idle.shown.has_value() ? ", last " : "",
	            idle.shown.has_value() ? creature_desires::Name(*idle.shown).data() : "");

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
		ImGui::Text("Looks at a %s (entity %u) for %.1f s", LookKindName(mind->look.kind).data(), *mind->look.id,
		            static_cast<double>(mind->look.watchedTurns) / 10.0);
	}
	else
	{
		ImGui::Text("Looks %s", animation->lookAt.has_value() ? "ahead" : "nowhere in particular");
	}
	ImGui::Text("Head turned %+.0f degrees left, %+.0f up", static_cast<double>(glm::degrees(animation->yaw.angle)),
	            static_cast<double>(glm::degrees(animation->pitch.angle)));

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
		// Unmirrored: tossing the game's coin outside the turn would change its random numbers
		minds.PlayAction(entity, animations::k_FirstAction + _action, false);
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
		minds.PlayGesture(entity, animations::k_FirstGesture + _gesture);
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
			minds.PullFace(entity, animations::k_FirstFace + i);
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
			minds.ShowFeeling(entity, k_Feelings.at(i));
		}
	}
	ImGui::BeginDisabled(busy);
	if (ImGui::Button("Sit down"))
	{
		minds.SitDown(entity);
	}
	ImGui::EndDisabled();
	ImGui::SameLine();
	ImGui::BeginDisabled(!creature_layers::IsLooping(body));
	if (ImGui::Button("Stand up"))
	{
		minds.StandUp(entity);
	}
	ImGui::EndDisabled();
	ImGui::SameLine();
	if (ImGui::Button("Stroke"))
	{
		minds.ReceiveFeedback(entity, 0.5f);
	}
	ImGui::SameLine();
	if (ImGui::Button("Slap"))
	{
		minds.ReceiveFeedback(entity, -0.5f);
	}

	ImGui::SeparatorText("Desires");
	ImGui::Text("Alone %.0f s", static_cast<double>(mind->secondsAlone));
	if (!mind->desires.has_value())
	{
		return;
	}
	const auto& desires = *mind->desires;
	ImGui::Text("All desires add up to %.2f", static_cast<double>(desires.sum));
	for (const auto desire : DesiresByStrength(desires))
	{
		const auto& state = desires[desire];
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
		_clicked = false;
		if (_placing)
		{
			_commanding = false;
		}
	}
	ImGui::PopStyleColor(2);
	if (_placing)
	{
		ImGui::TextWrapped("Left click on the land to place a creature there");
	}
}

void CreatureSpawner::DrawCreatures() noexcept
{
	auto& registry = Locator::entitiesRegistry::value();

	std::vector<entt::entity> creatures;
	std::as_const(registry).Each<const Creature>(
	    [&creatures](entt::entity entity, const Creature&) { creatures.push_back(entity); });

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
		registry.Destroy(creatures.begin(), creatures.end());
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
			const auto& creature = std::as_const(registry).Get<const Creature>(entity);
			const auto* transform = std::as_const(registry).TryGet<const Transform>(entity);
			ImGui::PushID(static_cast<int>(entt::to_integral(entity)));
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::TextUnformatted(SpeciesName(creature.species).data());
			ImGui::TableNextColumn();
			ImGui::TextUnformatted(OwnerName(creature.owner).data());
			ImGui::TableNextColumn();
			if (transform != nullptr)
			{
				ImGui::Text("%.0f, %.0f", static_cast<double>(transform->position.x),
				            static_cast<double>(transform->position.z));
			}
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
		registry.Destroy(*remove);
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
	if (!_clicked)
	{
		return;
	}
	_clicked = false;
	if (_placing)
	{
		Spawn();
	}
	else if (_commanding)
	{
		Command();
	}
}

void CreatureSpawner::Command() noexcept
{
	if (!_selected.has_value() || !Locator::creatureLocomotionSystem::has_value() || !Locator::camera::has_value() ||
	    !Locator::entitiesRegistry::has_value())
	{
		return;
	}
	const auto hit = Locator::camera::value().RaycastMouseToLand(false);
	if (!hit.has_value())
	{
		_lastOrder = "No land under the click";
		return;
	}
	using Pace = ecs::systems::CreatureLocomotionSystemInterface::Pace;
	auto& locomotion = Locator::creatureLocomotionSystem::value();
	const auto point = glm::xz(hit->position);
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
		                         MoveResultName(locomotion.FleeFrom(*_selected, point)));
		break;
	case Order::Face:
		_lastOrder = fmt::format("Facing {:.0f}, {:.0f}: {}", point.x, point.y,
		                         locomotion.TurnToFace(*_selected, point) ? "turning" : "can't");
		break;
	case Order::PickUp:
	case Order::Destroy:
	{
		if (!Locator::creatureObjectActionSystem::has_value())
		{
			break;
		}
		// What is nearest where the land was clicked
		auto& hands = Locator::creatureObjectActionSystem::value();
		const bool pickUp = _order == Order::PickUp;
		std::vector<debug::miracles::ThingAt> things;
		std::as_const(Locator::entitiesRegistry::value()).Each<const Transform>([&](entt::entity entity, const Transform& at) {
			if (entity != *_selected && (pickUp ? hands.CanPickUp(entity) : hands.CanDestroy(entity)))
			{
				things.push_back({.entity = entity, .position = at.position});
			}
		});
		const auto nearest = debug::miracles::NearestThing(things, hit->position, k_PickRadius);
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
		if (Locator::creatureObjectActionSystem::has_value())
		{
			_lastOrder = fmt::format("Throwing at {:.0f}, {:.0f}: {}", point.x, point.y,
			                         Locator::creatureObjectActionSystem::value().Throw(*_selected, hit->position) ? "started"
			                                                                                                       : "can't");
		}
		break;
	case Order::Point:
		if (Locator::creatureObjectActionSystem::has_value())
		{
			_lastOrder = fmt::format("Pointing at {:.0f}, {:.0f}: {}", point.x, point.y,
			                         Locator::creatureObjectActionSystem::value().PointAt(*_selected, hit->position) ? "started"
			                                                                                                         : "can't");
		}
		break;
	}
}

void CreatureSpawner::Spawn() noexcept
{
	if (!Locator::terrainSystem::has_value() || !Locator::landPickSystem::has_value() || !Locator::camera::has_value())
	{
		return;
	}
	const auto hit = Locator::camera::value().RaycastMouseToLand(false);
	if (!hit.has_value())
	{
		return;
	}
	SpawnAt(hit->position);
}

entt::entity CreatureSpawner::SpawnAt(const glm::vec3& position) noexcept
{
	const auto degrees = _randomFacing ? RandomFacingDegrees(_random) : _facingDegrees;
	// A mind from the cache is taken up on the creature's first turn of thought
	const auto entity = CreatureArchetype::Create(position, _owner, _species, _spawnMind, glm::radians(degrees), _scale,
	                                              {.alignment = _alignment, .fatness = _fatness, .strength = _strength});
	if (Locator::leashSystem::has_value())
	{
		// Made for trying things out, it knows every leash, as creatures made by the original's debug tools do
		for (const auto type : creature_leash::k_Types)
		{
			Locator::leashSystem::value().SetKnown(entity, type, true);
		}
	}
	auto& registry = Locator::entitiesRegistry::value();
	if (auto* tattoos = Find<CreatureTattoos>(registry, entity); tattoos != nullptr && _spawnTattoos.has_value())
	{
		tattoos->slots = *_spawnTattoos;
		++tattoos->revision;
	}
	return entity;
}

bool CreatureSpawner::TakesEvent(const SDL_Event& event) const noexcept
{
	// While placing or commanding, a left click on the land is the window's rather than the hand's
	return IsLeftClick(event) &&
	       _leftCapture.Takes(event.type == SDL_MOUSEBUTTONDOWN, _placing || (_commanding && _selected.has_value()),
	                          ImGui::GetIO().WantCaptureMouse);
}

void CreatureSpawner::ProcessEventOpen(const SDL_Event& event) noexcept
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

void CreatureSpawner::ProcessEventAlways([[maybe_unused]] const SDL_Event& event) noexcept {}
