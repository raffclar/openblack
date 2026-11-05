/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cctype>
#include <cmath>
#include <cstdint>
#include <ctime>

#include <algorithm>
#include <array>
#include <chrono>
#include <functional>
#include <iterator>
#include <limits>
#include <numbers>
#include <optional>
#include <ranges>
#include <sstream>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_set>
#include <utility>
#include <vector>

#include <LHVM.h>
#include <LHVMTypes.h>
#include <SaveSlots.h>
#include <entt/entity/entity.hpp>
#include <entt/entity/fwd.hpp>
#include <glm/geometric.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/matrix.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <spdlog/spdlog.h>

#include "3D/L3DMesh.h"
#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "3D/TempleInteriorInterface.h"
#include "Animals/AnimalMove.h"
#include "CHLApi.h"
#include "CHLApi/NativeHelpers.h"
#include "CHLApi/Natives.h"
#include "Camera/Camera.h"
#include "Common/GUtilsAngle.h"
#include "Common/GUtilsDistance.h"
#include "Common/PortableRotations.h"
#include "Creature/CreatureHandGlow.h"
#include "Creature/CreatureScriptPlay.h"
#include "Creature/LeashRules.h"
#include "Creature/TemplePen.h"
#include "ECS/Archetypes/CreatureArchetype.h"
#include "ECS/ChallengeRecords.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureFight.h"
#include "ECS/Components/CreatureLocomotion.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/CreatureNeeds.h"
#include "ECS/Components/CreatureSpells.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/Flock.h"
#include "ECS/Components/HandGrab.h"
#include "ECS/Components/Indestructible.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/OneOffSpellSeed.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/Player.h"
#include "ECS/Components/PuzzleGame.h"
#include "ECS/Components/ScriptControl.h"
#include "ECS/Components/ScriptHighlight.h"
#include "ECS/Components/Shark.h"
#include "ECS/Components/Spell.h"
#include "ECS/Components/SpellSeed.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/VillageTotem.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/VillagerDeath.h"
#include "ECS/Components/WallHug.h"
#include "ECS/Dances.h"
#include "ECS/Map.h"
#include "ECS/MazePuzzleRules.h"
#include "ECS/PlacedTurn.h"
#include "ECS/Reflection/BinaryArchive.h"
#include "ECS/Registry.h"
#include "ECS/ScriptContainerWalk.h"
#include "ECS/ScriptFlocks.h"
#include "ECS/ScriptSpotVisuals.h"
#include "ECS/Systems/AlignmentSystemInterface.h"
#include "ECS/Systems/AnimalSystemInterface.h"
#include "ECS/Systems/BoatLaunchSystemInterface.h"
#include "ECS/Systems/CameraHelpSystemInterface.h"
#include "ECS/Systems/CreatureCarryOverSystemInterface.h"
#include "ECS/Systems/CreatureFightSystemInterface.h"
#include "ECS/Systems/CreatureLocomotionSystemInterface.h"
#include "ECS/Systems/CreatureMindSystemInterface.h"
#include "ECS/Systems/CreatureObjectActionSystemInterface.h"
#include "ECS/Systems/CreaturePhysiologySystemInterface.h"
#include "ECS/Systems/DynamicsSystemInterface.h"
#include "ECS/Systems/EffectRequestSystemInterface.h"
#include "ECS/Systems/FightTranscriptSystemInterface.h"
#include "ECS/Systems/HandDemoSystemInterface.h"
#include "ECS/Systems/HelpProfileSystemInterface.h"
#include "ECS/Systems/HelpSpeechSystemInterface.h"
#include "ECS/Systems/Implementations/ObjectMeasures.h"
#include "ECS/Systems/Implementations/VillagerDisciples.h"
#include "ECS/Systems/Implementations/VillagerScript.h"
#include "ECS/Systems/LivingActionSystemInterface.h"
#include "ECS/Systems/ParticleRuleSystemInterface.h"
#include "ECS/Systems/PuzzleTotemSystemInterface.h"
#include "ECS/Systems/ReactionSystemInterface.h"
#include "ECS/Systems/ScriptHighlightSystemInterface.h"
#include "ECS/Systems/TurnCheerSystemInterface.h"
#include "ECS/Systems/TutorialSkipSystemInterface.h"
#include "ECS/Systems/VideoSystemInterface.h"
#include "ECS/Systems/VillageTotemSystemInterface.h"
#include "ECS/Systems/WeatherSystemInterface.h"
#include "ECS/Systems/WorshipSiteSystemInterface.h"
#include "ECS/TempleConstruction.h"
#include "ECS/ThingMapPlace.h"
#include "ECS/VillagerAge.h"
#include "ECS/VillagerScriptRules.h"
#include "ECS/WalkerPlacement.h"
#include "ECS/WallHugRules.h"
#include "ECS/WorldObjects.h"
#include "Enums.h"
#include "Gui/ScriptTexts.h"
#include "Help/Spirits.h"
#include "InfoConstants.h"
#include "Input/GameActionMapInterface.h"
#include "Locator.h"
#include "Physics/Body.h"
#include "Resources/ResourcesInterface.h"
#include "ScriptHeaders/ScriptEnums.h"
#include "ScriptHeaders/ScriptPropertyRules.h"
#include "Sound/GameMusic.h"

namespace openblack::chlapi
{

using namespace openblack::ecs::archetypes;
using openblack::Locator;
using openblack::ecs::components::CreatureMindState;
using openblack::ecs::components::Transform;

/// A creature given a state plays what the script last gave it to play
void CreatureCarriesOutScriptPlay(entt::entity creature)
{
	auto& mind = Locator::entitiesRegistry::value().Get<ecs::components::CreatureMindState>(creature);
	if (Locator::creatureMindSystem::has_value())
	{
		Locator::creatureMindSystem::value().CarryOutForScript(creature, creature_script_play::Agenda(mind.scriptPlay));
	}
}

void SetScriptState() // 017 SET_SCRIPT_STATE
{
	const auto state = Pop().intVal;
	const auto object = PopObject();
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(object))
	{
		ObjectNoLongerValid();
		return;
	}
	// The script takes control of each member of a group. Each living member takes the state as one would on its own,
	// with no word of those that can't.
	if (IsScriptContainer(registry, object))
	{
		for (const auto member : ContainerMembers(registry, object))
		{
			TakeControlInLoop(registry, member);
			if (IsDirectableVillager(member))
			{
				auto& living = Locator::livingActionSystem::value();
				if (state >= 0 && state < static_cast<int32_t>(VillagerStates::_COUNT))
				{
					living.VillagerSetScriptState(member, static_cast<VillagerStates>(state));
				}
			}
			else if (registry.Valid(member) && registry.AllOf<ecs::components::CreatureMindState>(member))
			{
				CreatureCarriesOutScriptPlay(member);
			}
			else if (registry.Valid(member) && registry.AllOf<ecs::components::Animal>(member) &&
			         Locator::animalSystem::has_value() && state >= 0)
			{
				static_cast<void>(Locator::animalSystem::value().SetScriptState(member, static_cast<LivingStates>(state)));
			}
		}
		return;
	}
	if (IsDirectableVillager(object))
	{
		auto& living = Locator::livingActionSystem::value();
		if (state < 0 || state >= static_cast<int32_t>(VillagerStates::_COUNT) || !living.VillagerCanBeDirected(object))
		{
			ScriptMessage("Object not living for set state");
			return;
		}
		living.VillagerSetScriptState(object, static_cast<VillagerStates>(state));
		return;
	}
	if (registry.AllOf<ecs::components::CreatureMindState>(object))
	{
		// A creature takes no state: it starts playing what the script last gave it to play
		CreatureCarriesOutScriptPlay(object);
		return;
	}
	// An animal takes the state of its own table
	if (registry.AllOf<ecs::components::Animal>(object) && Locator::animalSystem::has_value())
	{
		if (state < 0 || !Locator::animalSystem::value().SetScriptState(object, static_cast<LivingStates>(state)))
		{
			if (const auto logger = spdlog::get("scripting"))
			{
				SPDLOG_LOGGER_WARN(logger, "SET_SCRIPT_STATE: animal state {} is not done", state);
			}
		}
		return;
	}
	ScriptMessage("Object not living for set state");
}

void SetScriptStatePos() // 018 SET_SCRIPT_STATE_POS
{
	const auto position = PopVec();
	const auto object = PopObject();
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(object))
	{
		ObjectNoLongerValid();
		return;
	}
	const auto place = glm::vec2(position.x, position.z);
	if (IsScriptContainer(registry, object))
	{
		// The script takes control of each member. Only a villager gets the place.
		for (const auto member : ContainerMembers(registry, object))
		{
			TakeControlInLoop(registry, member);
			if (IsDirectableVillager(member))
			{
				Locator::livingActionSystem::value().VillagerSetScriptPlace(member, place);
			}
		}
		return;
	}
	if (!IsDirectableVillager(object))
	{
		ScriptMessage("Object not villager for state data");
		return;
	}
	Locator::livingActionSystem::value().VillagerSetScriptPlace(object, place);
}

void SetScriptFloat() // 019 SET_SCRIPT_FLOAT
{
	const auto value = Popf();
	const auto object = PopObject();
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(object))
	{
		ObjectNoLongerValid();
		return;
	}
	if (IsScriptContainer(registry, object))
	{
		// The script takes control of each member. Only a villager gets the number.
		for (const auto member : ContainerMembers(registry, object))
		{
			TakeControlInLoop(registry, member);
			if (IsDirectableVillager(member))
			{
				Locator::livingActionSystem::value().VillagerSetScriptNumber(member, value);
			}
		}
		return;
	}
	if (!IsDirectableVillager(object))
	{
		ScriptMessage("Object not villager for state data");
		return;
	}
	Locator::livingActionSystem::value().VillagerSetScriptNumber(object, value);
}

void SetScriptUlong() // 020 SET_SCRIPT_ULONG
{
	// A count of -1 plays the clip without end
	const auto plays = Pop().uintVal;
	const auto animation = Pop().intVal;
	const auto object = PopObject();
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(object))
	{
		ObjectNoLongerValid();
		return;
	}
	// The two numbers serve the script state that the villager goes into next. They are a clip and its plays for a
	// clip, or the fewest and most turns at each stop for a walk round a place.
	const auto giveVillager = [animation, plays](entt::entity villager) {
		auto& living = Locator::livingActionSystem::value();
		living.VillagerSetScriptNumbers(villager, static_cast<uint32_t>(animation), plays);
		living.VillagerSetScriptAnimation(villager, static_cast<AnimId>(animation), plays);
	};
	if (IsScriptContainer(registry, object))
	{
		// The script takes control of each member. Only a villager gets the two numbers. A creature or an animal in the
		// group keeps the numbers that it had.
		for (const auto member : ContainerMembers(registry, object))
		{
			TakeControlInLoop(registry, member);
			if (IsDirectableVillager(member))
			{
				giveVillager(member);
			}
		}
		return;
	}
	if (IsDirectableVillager(object))
	{
		giveVillager(object);
		return;
	}
	if (auto* mind = registry.TryGet<ecs::components::CreatureMindState>(object))
	{
		// Kept for the creature until the script tells it to start
		mind->scriptPlay = {.animation = animation, .plays = plays};
		return;
	}
	ScriptMessage("setting the state of something neither a creature nor a villager");
}

/// Whether something is drowning, as the scripts ask: a villager while it is in its drowning state, anything else
/// while it is in the physics with its body's centre under the sea's level
static bool IsDrowning(entt::entity object)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(object))
	{
		return false;
	}
	if (registry.AllOf<ecs::components::Villager>(object))
	{
		const auto* action = registry.TryGet<const ecs::components::LivingAction>(object);
		return action != nullptr && Locator::livingActionSystem::has_value() &&
		       Locator::livingActionSystem::value().VillagerGetState(*action, ecs::components::LivingAction::Index::Top) ==
		           VillagerStates::Drowning;
	}
	if (!registry.AllOf<ecs::components::InPhysics>(object) || !Locator::dynamicsSystem::has_value())
	{
		return false;
	}
	const auto* entry = Locator::dynamicsSystem::value().Find(object);
	return entry != nullptr && entry->body != nullptr && entry->body->Centre().y < 0.0f;
}

/// Whether a town is wholly destroyed: none of its buildings stands
bool TownCompletelyDestroyed(const ecs::components::Town& town)
{
	const auto& registry = Locator::entitiesRegistry::value();
	std::vector<script::property_rules::TownBuilding> buildings;
	buildings.reserve(town.abodes.size());
	for (const auto abode : town.abodes)
	{
		if (!registry.Valid(abode))
		{
			continue;
		}
		const auto* progress = registry.TryGet<const ecs::components::BuildProgress>(abode);
		buildings.push_back({
		    .life = ecs::world_objects::LifeOf(abode),
		    .field = registry.AllOf<ecs::components::Field>(abode),
		    .built = progress != nullptr ? progress->built : 1.0f,
		});
	}
	return script::property_rules::TownCompletelyDestroyed(buildings);
}

/// A creature has its body from the time that it is made. A script that reads or sets a value of the body before the
/// first turn of the creature works on the start values of its species, and the value that it sets stays.
static void StartCreatureBody(entt::entity object)
{
	if (Locator::entitiesRegistry::value().AllOf<ecs::components::Creature>(object) &&
	    Locator::creaturePhysiologySystem::has_value())
	{
		Locator::creaturePhysiologySystem::value().StartBody(object);
	}
}

/// A creature's body as the scripts read and set it, none for anything else
static creature_physiology::Needs* CreatureNeedsOf(entt::entity object)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.AllOf<ecs::components::Creature>(object))
	{
		return nullptr;
	}
	StartCreatureBody(object);
	auto* needs = registry.TryGet<ecs::components::CreatureNeeds>(object);
	return needs != nullptr ? &needs->needs : nullptr;
}

/// The need of a creature a property is, none for the properties that aren't
static std::optional<script::property_rules::CreatureNeed> NeedOf(script::ObjectPropertyType prop)
{
	using script::ObjectPropertyType;
	using script::property_rules::CreatureNeed;
	switch (prop)
	{
	case ObjectPropertyType::CreatureWarmth:
		return CreatureNeed::Warmth;
	case ObjectPropertyType::CreatureEnergy:
		return CreatureNeed::Energy;
	case ObjectPropertyType::CreatureItchiness:
		return CreatureNeed::Itchiness;
	case ObjectPropertyType::CreatureAmountOfPoo:
		return CreatureNeed::Poo;
	case ObjectPropertyType::CreatureExhaustion:
		return CreatureNeed::Exhaustion;
	case ObjectPropertyType::CreatureDehydration:
		return CreatureNeed::Dehydration;
	default:
		return std::nullopt;
	}
}

static float& NeedValue(creature_physiology::Needs& needs, script::property_rules::CreatureNeed need)
{
	using script::property_rules::CreatureNeed;
	switch (need)
	{
	case CreatureNeed::Warmth:
		return needs.warmth;
	case CreatureNeed::Energy:
		return needs.energy;
	case CreatureNeed::Itchiness:
		return needs.itchiness;
	case CreatureNeed::Poo:
		return needs.poo;
	case CreatureNeed::Exhaustion:
		return needs.exhaustion;
	case CreatureNeed::Dehydration:
	default:
		return needs.dehydration;
	}
}

/// Whether a thing is one of the world's objects, which have a life, angles and a height; a town isn't
bool IsWorldObject(entt::entity object)
{
	return !Locator::entitiesRegistry::value().AllOf<ecs::components::Town>(object);
}

/// Whether a living thing: its angle is kept by its walking, not by how it was placed
bool IsLivingThing(entt::entity object)
{
	return Locator::entitiesRegistry::value()
	    .AnyOf<ecs::components::Villager, ecs::components::Creature, ecs::components::Animal>(object);
}

/// The way a living thing faces, as the scripts take it: radians from +x towards +z
float LivingAngleOf(entt::entity object, const Transform& transform)
{
	const auto& registry = Locator::entitiesRegistry::value();
	if (const auto* walk = registry.TryGet<const ecs::components::WallHug>(object); walk != nullptr)
	{
		return walk->yAngle;
	}
	if (const auto* animal = registry.TryGet<const ecs::components::Animal>(object); animal != nullptr)
	{
		return animal->heading;
	}
	if (const auto* locomotion = registry.TryGet<const ecs::components::CreatureLocomotion>(object);
	    locomotion != nullptr && locomotion->started)
	{
		return script::property_rules::CreatureHeadingToLivingAngle(locomotion->heading);
	}
	// Standing still, a living thing is turned as it is drawn: its model faces the other way round, a quarter turn on
	return script::property_rules::CreatureHeadingToLivingAngle(
	    GameHeading(script::property_rules::PlacedAngles(transform.rotation).y));
}

/// Turns a living thing to face a way, given as the scripts take it: its walk, if it has one, faces that way from now
void TurnLivingThing(entt::entity object, Transform& transform, float angle)
{
	auto& registry = Locator::entitiesRegistry::value();
	const bool turned = angle != LivingAngleOf(object, transform);
	const auto heading = script::property_rules::LivingAngleToCreatureHeading(angle);
	if (auto* walk = registry.TryGet<ecs::components::WallHug>(object); walk != nullptr)
	{
		walk->yAngle = angle;
		walk->gameAngle = static_cast<uint16_t>(gutils::ConvertAngle3DToGame(angle));
	}
	if (auto* animal = registry.TryGet<ecs::components::Animal>(object); animal != nullptr)
	{
		animal->heading = angle;
		animal->previousHeading = angle;
	}
	if (auto* locomotion = registry.TryGet<ecs::components::CreatureLocomotion>(object); locomotion != nullptr)
	{
		locomotion->heading = heading;
		locomotion->targetHeading = heading;
		locomotion->fromHeading = heading;
		locomotion->toHeading = heading;
	}
	transform.rotation = glm::mat3(portable_math::EulerAngleY(heading.Yaw()));
	registry.SetDirty();
	// A thing that turns comes out of the map's cells and goes in again, at the front of the list of its cell
	if (turned)
	{
		ecs::thing_map_place::Reinsert(object);
	}
}

/// Turns a thing that is not living to the given angles and keeps them. A thing in the map's cells comes out of them and
/// goes in again: always for a turn of a lean, and for a turn about the upright only when the angle about y changes.
void TurnPlacedThing(entt::entity object, const glm::vec3& angles, bool lean)
{
	auto& registry = Locator::entitiesRegistry::value();
	const bool turned = lean || angles.y != ecs::placed_turn::AnglesOf(registry, object).y;
	ecs::placed_turn::SetAngles(registry, object, angles);
	if (turned)
	{
		ecs::thing_map_place::Reinsert(object);
	}
}

/// Whether a thing may lean, so that its angles across and forward are its own; the others always stand upright
static bool CanLean(entt::entity object)
{
	return ecs::placed_turn::CanLean(Locator::entitiesRegistry::value(), object);
}

/// How tall an object stands to the scripts: a creature by its size, anything else by its model as it is scaled, none
/// without a model
float ScriptHeightOf(entt::entity object)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (const auto* creature = registry.TryGet<const ecs::components::Creature>(object); creature != nullptr)
	{
		// As tall as its body is shown, kept within the sizes it can be drawn at
		return script::property_rules::CreatureHeight(ecs::components::ShownSize(*creature));
	}
	const auto* transform = registry.TryGet<const Transform>(object);
	const auto* mesh = registry.TryGet<const ecs::components::Mesh>(object);
	if (transform == nullptr || mesh == nullptr || !Locator::resources::has_value())
	{
		return 0.0f;
	}
	const auto& meshes = Locator::resources::value().GetMeshes();
	if (!meshes.Contains(mesh->id))
	{
		return 0.0f;
	}
	return meshes.Handle(mesh->id)->GetBoundingBox().Size().y * transform->scale.y;
}

/// The town a totem stands in: the town of the town centre it stands on
static entt::entity TownOfTotem(entt::entity totem)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* totemComponent = registry.TryGet<const ecs::components::VillageTotem>(totem);
	const auto* abode = totemComponent != nullptr && registry.Valid(totemComponent->townCentre)
	                        ? registry.TryGet<const ecs::components::Abode>(totemComponent->townCentre)
	                        : nullptr;
	if (abode == nullptr)
	{
		return entt::null;
	}
	const auto& towns = registry.Context().towns;
	const auto found = towns.find(abode->townId);
	return found != towns.end() && registry.Valid(found->second) ? found->second : entt::null;
}

/// The script asked for a property the thing doesn't have
static void CannotGetProperty(script::ObjectPropertyType prop)
{
	SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Cannot Get Property {}", static_cast<int>(prop));
	Pushf(0.0f);
}

static void CannotSetProperty(script::ObjectPropertyType prop)
{
	SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Cannot Set Property {}", static_cast<int>(prop));
}

void GetProperty() // 021 GET_PROPERTY
{
	const auto object = PopObject();
	const auto prop = static_cast<script::ObjectPropertyType>(Pop().intVal);
	auto& registry = Locator::entitiesRegistry::value();
	if (object == entt::null || !registry.Valid(object))
	{
		ObjectNoLongerValid();
		Pushf(0.0f);
		return;
	}
	// A town, a flock or a dance gives 0 for each property but its player. A script reads its members one by one.
	if (IsScriptContainer(registry, object) && prop != script::ObjectPropertyType::Player)
	{
		Pushf(0.0f);
		return;
	}
	if (const auto need = NeedOf(prop); need.has_value())
	{
		auto* needs = CreatureNeedsOf(object);
		if (needs == nullptr)
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Object not a creature");
			Pushf(0.0f);
			return;
		}
		Pushf(NeedValue(*needs, *need));
		return;
	}
	switch (prop)
	{
	case script::ObjectPropertyType::CreatureFightHealth:
		if (!registry.AllOf<ecs::components::Creature>(object))
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Object not a creature");
			Pushf(0.0f);
			return;
		}
		{
			const auto health =
			    Locator::creatureFightSystem::has_value() ? Locator::creatureFightSystem::value().GetFightHealth(object) : 1.0f;
			if (Locator::fightTranscript::has_value())
			{
				Locator::fightTranscript::value().ScriptRead(object, "FIGHTHEALTH", health);
			}
			Pushf(health);
			return;
		}
	case script::ObjectPropertyType::Flying:
		// In the physics, thrown, dropped or knocked and not yet at rest
		Pushb(registry.AllOf<ecs::components::InPhysics>(object));
		return;
	case script::ObjectPropertyType::Drowning:
		Pushb(IsDrowning(object));
		return;
	case script::ObjectPropertyType::Scale:
		// A creature answers the size its body is shown at
		if (const auto* creature = registry.TryGet<const ecs::components::Creature>(object); creature != nullptr)
		{
			Pushf(ecs::components::ShownSize(*creature));
			return;
		}
		if (const auto* transform = Locator::entitiesRegistry::value().TryGet<const Transform>(object); transform != nullptr)
		{
			Pushf(transform->scale.x);
			return;
		}
		Pushf(0.0f);
		return;
	case script::ObjectPropertyType::Speed:
		// A thing in the physics gives the speed of its flight
		if (registry.AllOf<ecs::components::InPhysics>(object) && Locator::dynamicsSystem::has_value())
		{
			if (const auto* entry = Locator::dynamicsSystem::value().Find(object); entry != nullptr && entry->body != nullptr)
			{
				Pushf(glm::length(entry->body->velocity));
				return;
			}
		}
		// A weather thing gives the speed of its storm
		if (Locator::weatherSystem::has_value())
		{
			if (const auto stormSpeed = Locator::weatherSystem::value().WeatherThingSpeed(object))
			{
				Pushf(*stormSpeed);
				return;
			}
		}
		// An animal gives its walk speed, metres a turn
		if (registry.AllOf<ecs::components::Animal>(object))
		{
			Pushf(Locator::animalSystem::has_value() ? Locator::animalSystem::value().SpeedForScript(object) : 0.0f);
			return;
		}
		if (const auto* wallHug = Locator::entitiesRegistry::value().TryGet<const ecs::components::WallHug>(object);
		    wallHug != nullptr)
		{
			Pushf(ecs::villager_script_rules::WalkSpeedToScriptSpeed(wallHug->speed));
			return;
		}
		Pushf(0.0f);
		return;
	case script::ObjectPropertyType::Age:
		// Its age in years; only living things have one
		if (const auto* person = registry.TryGet<const ecs::components::Villager>(object); person != nullptr)
		{
			Pushf(static_cast<float>(ecs::villager_age::AgeNow(*person)));
			return;
		}
		// A creature has the age of its body
		if (const auto* needs = CreatureNeedsOf(object); needs != nullptr)
		{
			Pushf(static_cast<float>(needs->age));
			return;
		}
		// TODO(opening): the ages of animals; other things have none ("not used on non living objects")
		NotImplemented(static_cast<int32_t>(prop));
		Pushf(0.0f);
		return;
	case script::ObjectPropertyType::InHand:
		// Held in a hand, as a number
		Pushf(registry.AllOf<ecs::components::InHand>(object) ? 1.0f : 0.0f);
		return;
	case script::ObjectPropertyType::Player:
	{
		const auto* town = registry.TryGet<const ecs::components::Town>(object);
		Pushf(script::property_rules::PlayerProperty(ecs::world_objects::PlayerOf(object),
		                                             town != nullptr && TownCompletelyDestroyed(*town)));
		return;
	}
	case script::ObjectPropertyType::BuiltPercentage:
		// How much of it is built, 0 to 1; anything that isn't built is whole
		if (const auto* progress = registry.TryGet<const ecs::components::BuildProgress>(object); progress != nullptr)
		{
			Pushf(progress->built);
			return;
		}
		Pushf(1.0f);
		return;
	case script::ObjectPropertyType::XPos:
	case script::ObjectPropertyType::YPos:
	case script::ObjectPropertyType::ZPos:
	{
		const auto* transform = registry.TryGet<const Transform>(object);
		if (transform == nullptr)
		{
			Pushf(0.0f);
			return;
		}
		const auto& at = transform->position;
		if (prop == script::ObjectPropertyType::YPos)
		{
			// How high above the land it is
			const float land =
			    Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt(glm::vec2(at.x, at.z)) : 0.0f;
			Pushf(at.y - land);
			return;
		}
		// Across the land, as a map position holds it
		Pushf(map_coords::Quantise(prop == script::ObjectPropertyType::XPos ? at.x : at.z));
		return;
	}
	case script::ObjectPropertyType::Health:
		if (!IsWorldObject(object))
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Object not an object");
			CannotGetProperty(prop);
			return;
		}
		StartCreatureBody(object);
		Pushf(ecs::world_objects::LifeOf(object));
		return;
	case script::ObjectPropertyType::Moving:
	{
		// A creature moves while it is on its way somewhere, from being given where to go until it stops
		if (const auto* locomotion = registry.TryGet<const ecs::components::CreatureLocomotion>(object); locomotion != nullptr)
		{
			Pushb(locomotion->destination.has_value() && !locomotion->facingOnly &&
			      locomotion->motion != ecs::components::CreatureLocomotion::Motion::Standing);
			return;
		}
		// An animal moved across the land in its last turn
		if (const auto* animal = registry.TryGet<const ecs::components::Animal>(object); animal != nullptr)
		{
			Pushb(script::property_rules::MovedAcross(animal->previousPosition, animal->position));
			return;
		}
		// Things that aren't in the map's cells never move
		if (!IsWorldObject(object))
		{
			Pushb(false);
			return;
		}
		// TODO(script-natives): a villager moved in its last turn, and any other thing has moved from where it was made or
		// came to rest from the physics: openblack keeps neither position
		NotImplemented(static_cast<int32_t>(prop));
		Pushb(false);
		return;
	}
	case script::ObjectPropertyType::Death:
	{
		// What a dead villager died of; every other thing (and a living villager) answers none
		const auto* death = registry.TryGet<const ecs::components::VillagerDeath>(object);
		Pushf(death != nullptr && death->reason.has_value() ? static_cast<float>(*death->reason) : 0.0f);
		return;
	}
	case script::ObjectPropertyType::Angle:
	case script::ObjectPropertyType::XAngle:
	case script::ObjectPropertyType::ZAngle:
	{
		const auto* transform = registry.TryGet<const Transform>(object);
		if (!IsWorldObject(object) || transform == nullptr)
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Object not an object");
			CannotGetProperty(prop);
			return;
		}
		if (prop != script::ObjectPropertyType::Angle)
		{
			// The thing's kept angles, as it got them. A thing that does not lean has no lean.
			const auto angles = ecs::placed_turn::AnglesOf(registry, object);
			Pushf(script::property_rules::AngleToScript(prop == script::ObjectPropertyType::XAngle ? angles.x : angles.z));
			return;
		}
		if (IsLivingThing(object))
		{
			Pushf(script::property_rules::AngleToScript(LivingAngleOf(object, *transform)));
			return;
		}
		Pushf(script::property_rules::AngleToScript(ecs::placed_turn::AnglesOf(registry, object).y));
		return;
	}
	case script::ObjectPropertyType::Strength:
	{
		if (const auto* creature = registry.TryGet<const ecs::components::Creature>(object); creature != nullptr)
		{
			Pushf(creature->strength);
			return;
		}
		// A miracle's globe or seed: what scales the prayer power and the time of what it casts
		if (const auto* globe = registry.TryGet<const ecs::components::OneOffSpellSeed>(object); globe != nullptr)
		{
			Pushf(globe->multiplier);
			return;
		}
		if (const auto* seed = registry.TryGet<const ecs::components::SpellSeed>(object); seed != nullptr)
		{
			Pushf(seed->castMultiplier);
			return;
		}
		// A miracle that is cast: what multiplies the strength its prayer power gives
		if (const auto* spell = registry.TryGet<const ecs::components::Spell>(object); spell != nullptr)
		{
			Pushf(spell->chants.strengthMultiplier);
			return;
		}
		// Anything else answers its life
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Script-Did you want the strength of this?");
		Pushf(ecs::world_objects::LifeOf(object));
		return;
	}
	case script::ObjectPropertyType::Alignment:
	{
		if (const auto* creature = registry.TryGet<const ecs::components::Creature>(object); creature != nullptr)
		{
			Pushf(creature->alignment);
			return;
		}
		// Anything else its player's, none without a player
		const auto player = ecs::world_objects::PlayerOf(object);
		Pushf(player.has_value() && Locator::alignmentSystem::has_value()
		          ? Locator::alignmentSystem::value().GetPlayerAlignment(*player)
		          : 0.0f);
		return;
	}
	case script::ObjectPropertyType::Height:
		// A puzzle totem answers its height, a whole number
		if (const auto* totem = registry.TryGet<const ecs::components::PuzzleTotem>(object); totem != nullptr)
		{
			Pushf(static_cast<float>(totem->height));
			return;
		}
		// A town's totem answers the share of its people it was last set to send to worship
		if (const auto* totem = registry.TryGet<const ecs::components::VillageTotem>(object); totem != nullptr)
		{
			Pushf(totem->ease.target);
			return;
		}
		Pushf(IsWorldObject(object) ? ScriptHeightOf(object) : 0.0f);
		return;
	case script::ObjectPropertyType::MaxHeight:
		// A puzzle totem keeps a greatest height. Nothing else keeps one.
		if (const auto* totem = registry.TryGet<const ecs::components::PuzzleTotem>(object); totem != nullptr)
		{
			Pushf(static_cast<float>(totem->highest));
			return;
		}
		Pushf(0.0f);
		return;
	case script::ObjectPropertyType::CreatureMinSize:
	case script::ObjectPropertyType::CreatureMaxSize:
	{
		const auto* spells = registry.TryGet<const ecs::components::CreatureSpells>(object);
		const auto* creature = registry.TryGet<const ecs::components::Creature>(object);
		if (spells == nullptr || creature == nullptr)
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Object not a creature");
			CannotGetProperty(prop);
			return;
		}
		// What the size spells would take it to now: in a fight they go by its size
		const auto inFight =
		    registry.AllOf<ecs::components::CreatureFighting>(object) ? std::optional(creature->size) : std::nullopt;
		Pushf(prop == script::ObjectPropertyType::CreatureMinSize
		          ? creature_spells::SmallestSizeNow(spells->smallestSize, inFight)
		          : creature_spells::LargestSizeNow(spells->largestSize, inFight));
		return;
	}
	case script::ObjectPropertyType::CreatureFatness:
		if (const auto* creature = registry.TryGet<const ecs::components::Creature>(object); creature != nullptr)
		{
			Pushf(creature->fatness);
			return;
		}
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Object not a creature");
		CannotGetProperty(prop);
		return;
	case script::ObjectPropertyType::CreatureCanGoThroughVortex:
		// 1 when a script has let it through, else 0
		if (const auto* creature = registry.TryGet<const ecs::components::Creature>(object); creature != nullptr)
		{
			Pushf(creature->mayGoThroughVortex ? 1.0f : 0.0f);
			return;
		}
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Object not a creature");
		CannotGetProperty(prop);
		return;
	default:
		// TODO(Daniels118): the other properties
		NotImplemented(static_cast<int32_t>(prop));
		Pushf(0.0f);
		return;
	}
}

static void SetPropertyOf(entt::entity object, script::ObjectPropertyType prop, float value);

void SetProperty() // 022 SET_PROPERTY
{
	const auto value = Popf();
	const auto object = PopObject();
	const auto prop = static_cast<script::ObjectPropertyType>(Pop().intVal);
	auto& registry = Locator::entitiesRegistry::value();
	if (object == entt::null || !registry.Valid(object))
	{
		ObjectNoLongerValid();
		return;
	}
	// A town, a flock or a dance gives the property to each of its members. Each member takes it as if the script set it
	// on that member. The script does not take control of the members. When a script sets a health of 0 on a town, each
	// villager of the town has no life. Then each villager that no script controls dies at its next check.
	if (IsScriptContainer(registry, object))
	{
		for (const auto member : ContainerMembers(registry, object))
		{
			if (registry.Valid(member))
			{
				SetPropertyOf(member, prop, value);
			}
		}
		return;
	}
	SetPropertyOf(object, prop, value);
}

/// One property of one thing, as a script sets it on that thing
static void SetPropertyOf(entt::entity object, script::ObjectPropertyType prop, float value)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (const auto need = NeedOf(prop); need.has_value())
	{
		auto* needs = CreatureNeedsOf(object);
		if (needs == nullptr)
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Object not a creature");
			return;
		}
		NeedValue(*needs, *need) = script::property_rules::SetNeed(*need, value);
		return;
	}
	if (prop == script::ObjectPropertyType::YPos)
	{
		// A highlight keeps the height it is given, whatever stands under it
		if (registry.AllOf<ecs::components::ScriptHighlight>(object))
		{
			Locator::scriptHighlightSystem::value().SetDrawHeight(object, value);
		}
		// How high above the land it is: it is drawn there at once
		if (auto* transform = registry.TryGet<Transform>(object); transform != nullptr)
		{
			const auto& at = transform->position;
			const float land =
			    Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt(glm::vec2(at.x, at.z)) : 0.0f;
			transform->position.y = land + value;
		}
		return;
	}
	switch (prop)
	{
	case script::ObjectPropertyType::CreatureFightHealth:
		// Kept as given, even outside 0 to 1, until a fight starts it afresh
		if (!registry.AllOf<ecs::components::Creature>(object))
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Object not a creature");
			CannotSetProperty(prop);
			return;
		}
		if (Locator::creatureFightSystem::has_value())
		{
			Locator::creatureFightSystem::value().SetFightHealth(object, value);
		}
		return;
	case script::ObjectPropertyType::Health:
		if (!IsWorldObject(object))
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Object not an object");
			CannotSetProperty(prop);
			return;
		}
		StartCreatureBody(object);
		// A thing a script holds in a cutscene, or made indestructible, can't be brought to nearly no life
		ecs::world_objects::SetLife(
		    object, value,
		    script::property_rules::CanSetLife(value, ecs::world_objects::HeldDuringCutscene(object),
		                                       registry.AllOf<ecs::components::Indestructible>(object)));
		return;
	case script::ObjectPropertyType::Angle:
	case script::ObjectPropertyType::XAngle:
	case script::ObjectPropertyType::ZAngle:
	{
		auto* transform = registry.TryGet<Transform>(object);
		if (!IsWorldObject(object) || transform == nullptr)
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Object not an object");
			CannotSetProperty(prop);
			return;
		}
		const float radians = script::property_rules::AngleFromScript(value);
		// Only its turn is its own: a lean asked for changes nothing
		if (!CanLean(object) && prop != script::ObjectPropertyType::Angle)
		{
			return;
		}
		if (IsLivingThing(object))
		{
			// Unless it already faces that way
			if (radians != LivingAngleOf(object, *transform))
			{
				TurnLivingThing(object, *transform, radians);
			}
			return;
		}
		// The other angles stay as the thing has them
		auto angles = ecs::placed_turn::AnglesOf(registry, object);
		(prop == script::ObjectPropertyType::Angle    ? angles.y
		 : prop == script::ObjectPropertyType::XAngle ? angles.x
		                                              : angles.z) = radians;
		TurnPlacedThing(object, angles, prop != script::ObjectPropertyType::Angle);
		return;
	}
	case script::ObjectPropertyType::Strength:
		if (auto* creature = registry.TryGet<ecs::components::Creature>(object); creature != nullptr)
		{
			// Its body shows it
			creature->strength = value;
			ecs::archetypes::CreatureArchetype::ShowStrength(object);
			return;
		}
		if (auto* globe = registry.TryGet<ecs::components::OneOffSpellSeed>(object); globe != nullptr)
		{
			globe->multiplier = value;
			return;
		}
		if (auto* seed = registry.TryGet<ecs::components::SpellSeed>(object); seed != nullptr)
		{
			seed->castMultiplier = value;
			return;
		}
		// A miracle that is cast: the value multiplies each strength that it gets after this
		if (auto* spell = registry.TryGet<ecs::components::Spell>(object); spell != nullptr)
		{
			spell->chants.strengthMultiplier = value;
			return;
		}
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Cannot set strength of this");
		CannotSetProperty(prop);
		return;
	case script::ObjectPropertyType::Alignment:
		if (auto* creature = registry.TryGet<ecs::components::Creature>(object); creature != nullptr)
		{
			// Its body takes its looks at once
			creature->alignment = value;
			ecs::archetypes::CreatureArchetype::TakeLooks(registry, object);
			return;
		}
		// Only a creature (or a reward, which openblack doesn't keep an alignment for) takes one
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "UNEXPECTED Alignment change");
		return;
	case script::ObjectPropertyType::Height:
		if (registry.AllOf<ecs::components::VillageTotem>(object))
		{
			// A totem sets its town's share of people at worship
			if (const auto town = TownOfTotem(object); town != entt::null && Locator::villageTotemSystem::has_value())
			{
				Locator::villageTotemSystem::value().SetTownShare(town, value);
			}
			// TODO(script-natives): a totem with no town takes the share itself
			return;
		}
		if (auto* totem = registry.TryGet<ecs::components::PuzzleTotem>(object); totem != nullptr)
		{
			// A puzzle totem takes the height, made whole towards zero, up to its greatest height, and stands where that
			// height puts it
			const auto height = ecs::maze_puzzle::TotemHeight(value, totem->highest);
			if (Locator::puzzleTotemSystem::has_value())
			{
				Locator::puzzleTotemSystem::value().SetHeight(object, height);
			}
			else
			{
				totem->height = height;
			}
			return;
		}
		if (auto* creature = registry.TryGet<ecs::components::Creature>(object); creature != nullptr)
		{
			// It grows or shrinks to that height at once
			creature->size = script::property_rules::CreatureSizeForHeight(value);
			if (auto* transform = registry.TryGet<Transform>(object); transform != nullptr)
			{
				transform->scale = glm::vec3(ecs::archetypes::CreatureArchetype::DrawnScale(creature->species, creature->size));
				registry.SetDirty();
			}
		}
		// Nothing else takes a height
		return;
	case script::ObjectPropertyType::MaxHeight:
		// Nothing keeps a greatest height
		return;
	case script::ObjectPropertyType::CreatureMinSize:
	case script::ObjectPropertyType::CreatureMaxSize:
		if (auto* spells = registry.TryGet<ecs::components::CreatureSpells>(object); spells != nullptr)
		{
			// The size spells take it no further
			(prop == script::ObjectPropertyType::CreatureMinSize ? spells->smallestSize : spells->largestSize) = value;
			return;
		}
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Object not a creature");
		CannotSetProperty(prop);
		return;
	case script::ObjectPropertyType::CreatureCanGoThroughVortex:
		// Any value but 0 lets it through
		if (auto* creature = registry.TryGet<ecs::components::Creature>(object); creature != nullptr)
		{
			creature->mayGoThroughVortex = value != 0.0f;
			return;
		}
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Object not a creature");
		CannotSetProperty(prop);
		return;
	case script::ObjectPropertyType::CreatureFatness:
		if (auto* creature = registry.TryGet<ecs::components::Creature>(object); creature != nullptr)
		{
			creature->fatness = value;
			return;
		}
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Object not a creature");
		CannotSetProperty(prop);
		return;
	case script::ObjectPropertyType::Scale:
		// A creature grows or shrinks to that size at once, its body shown at it (within the sizes a creature can be
		// drawn at) unless it is in its pen
		if (auto* creature = registry.TryGet<ecs::components::Creature>(object); creature != nullptr)
		{
			creature->size = value;
			if (auto* transform = registry.TryGet<Transform>(object); transform != nullptr)
			{
				transform->scale = glm::vec3(
				    ecs::archetypes::CreatureArchetype::DrawnScale(creature->species, ecs::components::ShownSize(*creature)));
				registry.SetDirty();
			}
			return;
		}
		// A thing that holds a visual of a script gives the size to its visual and keeps its own size
		if (const auto effect = ecs::script_spot_visuals::EffectOf(registry, object); effect.has_value())
		{
			if (Locator::effectRequestSystem::has_value())
			{
				Locator::effectRequestSystem::value().SetMagnitude(*effect, value);
			}
			return;
		}
		// The thing is drawn at this size, the same along every axis. A thing in the map's cells that changes its size
		// comes out of them and goes in again.
		if (auto* transform = registry.TryGet<Transform>(object); transform != nullptr)
		{
			const bool resized = transform->scale.x != value;
			transform->scale = glm::vec3(value);
			registry.SetDirty();
			if (resized)
			{
				ecs::thing_map_place::Reinsert(object);
			}
		}
		return;
	case script::ObjectPropertyType::Age:
		// Only living things take an age, in whole years
		if (registry.AllOf<ecs::components::Villager>(object))
		{
			Locator::livingActionSystem::value().VillagerSetAge(object, static_cast<uint32_t>(static_cast<int32_t>(value)));
			return;
		}
		// A creature's body takes the age as it is, and nothing else changes
		if (auto* needs = CreatureNeedsOf(object); needs != nullptr)
		{
			needs->age = static_cast<uint32_t>(static_cast<int32_t>(value));
			return;
		}
		// TODO(opening): the ages of animals; other things take none
		NotImplemented(static_cast<int32_t>(prop));
		return;
	case script::ObjectPropertyType::Speed:
		// A creature is asked to go at that share of its top speed, from none to all of it, until it is next sent
		// somewhere at a pace of its own
		if (auto* locomotion = registry.TryGet<ecs::components::CreatureLocomotion>(object); locomotion != nullptr)
		{
			locomotion->fraction = script::property_rules::CreatureSpeedFraction(value);
			return;
		}
		// A weather thing's storm travels at it, metres a second
		if (Locator::weatherSystem::has_value() && Locator::weatherSystem::value().SetWeatherThingSpeed(object, value))
		{
			return;
		}
		// An animal walks at it, metres a turn
		if (registry.AllOf<ecs::components::Animal>(object))
		{
			if (Locator::animalSystem::has_value())
			{
				Locator::animalSystem::value().SetSpeedFromScript(object, value);
			}
			return;
		}
		// Metres a turn
		if (auto* wallHug = registry.TryGet<ecs::components::WallHug>(object); wallHug != nullptr)
		{
			wallHug->speed = ecs::villager_script_rules::ScriptSpeedToWalkSpeed(value);
		}
		return;
	case script::ObjectPropertyType::BuiltPercentage:
		// A building or a feature is built that far, finished at all of it
		ecs::construction::SetBuilt(object, value);
		return;
	default:
		// TODO(Daniels118): the other properties
		NotImplemented(static_cast<int32_t>(prop));
		return;
	}
}

void GetPosition() // 023 GET_POSITION
{
	// No object, or one that has gone, finds no transform: it is at the origin
	const auto objId = entt::to_integral(PopObject());

	glm::vec3 position(0.0f);
	if (objId != 0)
	{
		auto& registry = Locator::entitiesRegistry::value();
		auto* transform = registry.TryGet<Transform>(static_cast<entt::entity>(objId));
		const auto object = static_cast<entt::entity>(objId);
		if (ecs::script_flocks::IsFlock(registry, object))
		{
			// Where its leader is, or its own place on the ground when it has nobody
			const auto leader = ecs::script_flocks::Leader(registry, object);
			const auto* leaderTransform = leader != entt::null ? registry.TryGet<Transform>(leader) : nullptr;
			if (leaderTransform != nullptr)
			{
				position = leaderTransform->position;
			}
			else
			{
				const auto& place = registry.Get<const ecs::components::Flock>(object).place;
				const auto xz = map_coords::ToMetres(place);
				position = {xz.x, Locator::terrainSystem::value().GetHeightAt(xz) + place.altitude, xz.y};
			}
		}
		else if (const auto* shark = registry.TryGet<const ecs::components::Shark>(static_cast<entt::entity>(objId)))
		{
			// Where it is this turn, not where it is drawn on the way there
			position = shark->position;
		}
		else if (const auto* wallHug = registry.TryGet<const ecs::components::WallHug>(object);
		         wallHug != nullptr && transform != nullptr && !registry.AllOf<ecs::components::Creature>(object) &&
		         ecs::villager_script_rules::WithinAStepOfGoalPlace(glm::vec2(transform->position.x, transform->position.z),
		                                                            wallHug->goalPlace, wallHug->speed))
		{
			// A walker within a turn's step of where it is walking to is seen there, at the height of its goal above the
			// land. An animal keeps the height of its goal with its move.
			const auto goal = ecs::wall_hug::ToPoint(wallHug->goalPlace);
			const auto* animal = registry.TryGet<const ecs::components::Animal>(object);
			const float height = animal != nullptr ? animal->goalHeight : wallHug->goalHeight;
			position = {goal.x, Locator::terrainSystem::value().GetHeightAt(goal) + height, goal.y};
		}
		else if (transform != nullptr)
		{
			position = transform->position;
		}
	}

	PushVec(position);
}

void SetPosition() // 024 SET_POSITION
{
	auto position = PopVec();
	// No object, or one that has gone, finds no transform: nothing moves
	const auto objId = entt::to_integral(PopObject());

	if (objId != 0)
	{
		auto& registry = Locator::entitiesRegistry::value();
		const auto object = static_cast<entt::entity>(objId);
		// Any thing that is in the hand or flying stays where it is
		const bool inHand = registry.Valid(object) && registry.AllOf<ecs::components::InHand>(object);
		const bool flying = registry.Valid(object) && registry.AllOf<ecs::components::InPhysics>(object);
		if (inHand)
		{
			ScriptMessage("Setting object in hand to a position");
		}
		if (flying)
		{
			ScriptMessage("Trying to set position - Object is flying");
		}
		if (inHand || flying)
		{
			return;
		}
		// A marker is not in the map. It goes to the map position of the point and keeps the height of the point above
		// the land.
		if (registry.Valid(object) && registry.AllOf<ecs::components::ScriptMarker>(object))
		{
			const auto& island = Locator::terrainSystem::value();
			registry.Get<Transform>(object).position = map_coords::ToWorld(island, map_coords::FromWorld(island, position));
			registry.SetDirty();
			return;
		}
		// A thing out of the map's cells stays where it is, as a thing that a creature holds
		if (registry.Valid(object) && Locator::entitiesMap::has_value() && !Locator::entitiesMap::value().InMap(object))
		{
			ScriptMessage("Trying to set position - Object not in map");
			return;
		}
		// The thing goes to the map position of the point, on the land there
		const auto& island = Locator::terrainSystem::value();
		const auto place = map_coords::FromMetres({position.x, position.z});
		const auto placed = map_coords::ToMetres(place);
		position = {placed.x, island.GetHeightAt(placed), placed.y};
		if (IsDirectableVillager(object))
		{
			// Its walk takes up the new place, and one under the script's control stops there and waits for it
			ecs::walker_placement::Place(registry, object, position);
			ecs::thing_map_place::Keep(registry, object, place);
			if (Locator::entitiesMap::has_value())
			{
				Locator::entitiesMap::value().Moved(object);
			}
			if (registry.AllOf<ecs::components::ScriptControlled>(object))
			{
				ecs::walker_placement::Stop(registry, object);
				Locator::livingActionSystem::value().VillagerSetScriptState(object, VillagerStates::InScript);
			}
			registry.SetDirty();
			return;
		}
		// A creature goes there at once, without a walk. Its walk stops, and the plan continues from the new place.
		if (registry.AllOf<ecs::components::Creature>(object) && Locator::creatureLocomotionSystem::has_value())
		{
			Locator::creatureLocomotionSystem::value().PlaceWithoutWalking(object, position);
			ecs::thing_map_place::Keep(registry, object, place);
			if (Locator::entitiesMap::has_value())
			{
				Locator::entitiesMap::value().Moved(object);
			}
			return;
		}
		ecs::thing_map_place::Put(registry, object, place, position.y);
		// The place where the thing was before is now the new place. An animal under the script's control goes back to
		// its script state.
		if (registry.AllOf<ecs::components::Animal>(object))
		{
			ecs::walker_placement::Place(registry, object, position);
			if (registry.AllOf<ecs::components::ScriptControlled>(object) && Locator::animalSystem::has_value())
			{
				Locator::animalSystem::value().SetScriptState(object, LivingStates::LivingInScript);
			}
			registry.SetDirty();
		}
	}
}

void GetDistance() // 025 GET_DISTANCE
{
	const auto p1 = PopVec();
	const auto p0 = PopVec();
	Pushf(ecs::villager_script_rules::ScriptDistance(p0, p1));
}

} // namespace openblack::chlapi
