/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "CreatureMindSystem.h"

#include <cmath>
#include <cstring>

#include <algorithm>
#include <array>
#include <chrono>
#include <functional>
#include <iterator>
#include <limits>
#include <random>
#include <ranges>
#include <span>
#include <utility>
#include <vector>

#include <spdlog/spdlog.h>

#include "3D/CreatureBody.h"
#include "3D/DayNightClock.h"
#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "Camera/Camera.h"
#include "Creature/CreatureAttention.h"
#include "Creature/CreatureDesires.h"
#include "Creature/CreatureFace.h"
#include "Creature/CreatureFeedback.h"
#include "Creature/CreatureGiving.h"
#include "Creature/CreatureHelp.h"
#include "Creature/CreatureIdleMind.h"
#include "Creature/CreatureLayers.h"
#include "Creature/CreatureLocomotion.h"
#include "Creature/CreatureLook.h"
#include "Creature/CreatureObjectActions.h"
#include "Creature/CreaturePhysiology.h"
#include "Creature/CreaturePlanner.h"
#include "Creature/CreatureReach.h"
#include "Creature/CreatureRoute.h"
#include "Creature/CreatureScriptAgendas.h"
#include "Creature/CreatureThrow.h"
#include "CreatureMindSystemDetail.h"
#include "ECS/Archetypes/PotArchetype.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/CarriedByTornado.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureFight.h"
#include "ECS/Components/CreatureLocomotion.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/CreatureNeeds.h"
#include "ECS/Components/CreatureObjectAction.h"
#include "ECS/Components/HandGrab.h"
#include "ECS/Components/MapCellResident.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/ScriptControl.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/Villager.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CinematicDirectorSystemInterface.h"
#include "ECS/Systems/CreatureAnimationSystemInterface.h"
#include "ECS/Systems/CreatureFightSystemInterface.h"
#include "ECS/Systems/CreatureHandSystemInterface.h"
#include "ECS/Systems/CreatureHelpSystemInterface.h"
#include "ECS/Systems/CreatureLocomotionSystemInterface.h"
#include "ECS/Systems/CreatureObjectActionSystemInterface.h"
#include "ECS/Systems/CreaturePhysiologySystemInterface.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/MagicSystemInterface.h"
#include "ECS/Systems/ReactionSystemInterface.h"
#include "ECS/Systems/ResourceStoreSystemInterface.h"
#include "ECS/Systems/SkySystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "ObjectMeasures.h"
#include "VillagerPhysics.h"

using namespace openblack;
using namespace openblack::ecs::systems;
using namespace openblack::ecs::components;
namespace animations = openblack::creature_layers::animations;

namespace
{
/// Fainting from what its body lacks, the advisor says why first
void TellFainting(entt::entity creature)
{
	if (openblack::Locator::creatureHelpSystem::has_value())
	{
		openblack::Locator::creatureHelpSystem::value().Fainting(creature);
	}
}

/// The player's hand holds the creature
bool HeldByHand(entt::entity creature)
{
	return openblack::Locator::creatureHandSystem::has_value() &&
	       openblack::Locator::creatureHandSystem::value().GetCreature() == creature;
}
} // namespace
using creature_desires::Desire;

namespace openblack::ecs::systems::mind_detail
{
constexpr float k_TurnSeconds = std::chrono::duration<float>(TimeSystemInterface::k_TurnDuration).count();
constexpr float k_TurnsPerSecond = 1.0f / k_TurnSeconds;
/// A creature just put in the world takes its first few turns to stand up before it can pass out
constexpr uint32_t k_TurnsBeforePassingOut = 2;
/// A face told to by hand is held this long
constexpr float k_ToldFaceMs = 3000.0f;

/// A creature looks for food and water this far away at most
constexpr float k_FoodSearchDistance = 200.0f;
constexpr int32_t k_WaterSearchCells = 30;
/// Where it stands to drink is found this far at most from the water
constexpr float k_ShoreSearchDistance = 60.0f;
/// Food further than this from its mouth, for its size, has got away by the time it eats
constexpr float k_EatReach = 15.0f;
/// Having drunk it doesn't want water for a while, and having had a poo it doesn't want another for longer
constexpr float k_WaterSuppressSeconds = 20.0f;
constexpr float k_PooSuppressSeconds = 60.0f;
/// The rows of the game's creature action table that seeing to each need counts as
constexpr std::string_view k_EatAction = "EatAfterExamining";
constexpr std::string_view k_DrinkAction = "DrinkFromTheSea";
constexpr std::string_view k_PooAction = "Poo";
constexpr std::string_view k_PukeAction = "Puke";
constexpr std::string_view k_SleepAction = "SleepOnTheSpot";
constexpr std::string_view k_ExamineAction = "ExamineByPickingUp";
constexpr std::string_view k_ThrowAboutAction = "PracticeThrow";
constexpr std::string_view k_HurlAction = "Hurl";
constexpr std::string_view k_FightAction = "Fight";
/// The action a creature made to catch something carries out
constexpr std::string_view k_CatchAction = "Catch";
/// The action a creature given something from the hand carries out to take it
constexpr std::string_view k_TakeFromHandAction = "TakeObjectFromHand";
/// What a creature does with what it was given, by the game's action table
constexpr std::string_view k_ExamineGivenAction = "ExamineObjectInHand";
constexpr std::string_view k_EatGivenAction = "EatObjectInHand";
constexpr std::string_view k_ThrowGivenAction = "ThrowObjectInHand";
/// A creature looks for things to pick up this far away at most, and for homes and trees to hurl them at
constexpr float k_ObjectSearchDistance = 80.0f;
constexpr float k_HurlSearchDistance = 120.0f;
/// The creature wants the player's attention more the longer it is alone, fully after a minute, and from lack of
/// anything to do with the player, fully after two
constexpr float k_LonelySeconds = 60.0f;
constexpr float k_UninterestedSeconds = 120.0f;
/// The desire to show how it is grows with all its desires added up, fully at this much
constexpr float k_ManifestSum = 3.0f;

/// Just woken up, the eyelids droop and blink slowly; otherwise they are as they start
constexpr float k_SleepyOpenness = -0.7f;
constexpr int32_t k_SleepyBlinkIntervalMs = 2500;

/// How high above their feet creatures look at other things
constexpr float k_VillagerHeadHeight = 1.8f;
constexpr float k_AbodeLookHeight = 4.0f;
constexpr float k_TreeLookHeight = 6.0f;
constexpr float k_CitadelLookHeight = 30.0f;

float Clamp01(float value)
{
	return std::clamp(value, 0.0f, 1.0f);
}

/// The 17 per species values of a creature table row
template <typename Row>
float PerSpecies(const Row& row, size_t species)
{
	static_assert(sizeof(Row) == 17 * sizeof(float));
	std::array<float, 17> values {};
	std::memcpy(values.data(), &row, sizeof(values));
	return values.at(std::min(species, values.size() - 1));
}

/// How a species' desires start, from the game's creature tables
std::array<creature_desires::DesireSetup, creature_desires::k_DesireCount> SetupFor(CreatureType species)
{
	std::array<creature_desires::DesireSetup, creature_desires::k_DesireCount> setup {};
	if (!Locator::infoConstants::has_value())
	{
		return setup;
	}
	const auto& info = Locator::infoConstants::value();
	const auto row = creature::InfoRow(species);
	for (size_t d = 0; d < setup.size(); ++d)
	{
		const auto& initial = info.creatureInitialDesire.at(d);
		auto& desire = setup.at(d);
		desire.max = initial.field0x3c;
		desire.decayMin = initial.field0x40;
		desire.decayMax = initial.field0x44;
		desire.increaseSeconds = PerSpecies(info.creatureDesireForType.at(d), row);
		desire.weight = initial.field0x68;
		const std::array<uint32_t, creature_desires::k_MaxSources> sourceTypes {
		    initial.field0x0,  initial.field0x4,  initial.field0x8,  initial.field0xc,
		    initial.field0x10, initial.field0x14, initial.field0x18, initial.field0x1c,
		};
		for (const auto type : sourceTypes)
		{
			if (type >= creature_desires::k_NoSource)
			{
				continue;
			}
			desire.sources.push_back({
			    .type = type,
			    .value = PerSpecies(info.creatureInitialSource1.at(type), row),
			    .threshold = PerSpecies(info.creatureInitialSource2.at(type), row),
			    .multiplier = info.desireSourceTable.at(type).field0x8,
			    .clearedWhenSatisfied = info.desireSourceTable.at(type).field0x4 != 0,
			});
		}
	}
	return setup;
}

/// The desires each stage of growing up brings and takes away, from the game's tables
std::vector<creature_desires::PhaseDesires> Phases()
{
	std::vector<creature_desires::PhaseDesires> phases;
	if (!Locator::infoConstants::has_value())
	{
		return phases;
	}
	const auto valid = [](uint32_t desire) { return desire < creature_desires::k_DesireCount; };
	for (const auto& entry : Locator::infoConstants::value().creatureDevelopmentPhaseEntry)
	{
		auto& phase = phases.emplace_back();
		const std::array<uint32_t, 10> add {entry.field0x3c, entry.field0x40, entry.field0x44, entry.field0x48,
		                                    entry.field0x4c, entry.field0x50, entry.field0x54, entry.field0x58,
		                                    entry.field0x5c, entry.field0x60};
		const std::array<uint32_t, 4> remove {entry.field0x64, entry.field0x68, entry.field0x6c, entry.field0x70};
		for (const auto desire : add | std::views::filter(valid))
		{
			phase.add.push_back(static_cast<Desire>(desire));
		}
		for (const auto desire : remove | std::views::filter(valid))
		{
			phase.remove.push_back(static_cast<Desire>(desire));
		}
	}
	return phases;
}

/// The sources that follow the creature's state: its body's, and its mind's own
std::optional<float> ReadSource(uint32_t type, const creature_desires::Desires& desires, const CreatureMindState& mind,
                                const creature_physiology::Needs& needs, bool night)
{
	namespace sources = creature_desires::sources;
	if (const auto value = creature_physiology::SourceValue(type, needs, night))
	{
		return value;
	}
	switch (type)
	{
	case sources::k_AttentionFromLoneliness:
		return Clamp01(mind.secondsAlone / k_LonelySeconds);
	case sources::k_AttentionFromLackOfInteraction:
		return Clamp01(mind.secondsAlone / k_UninterestedSeconds);
	case sources::k_ManifestState:
		return Clamp01(desires.sum / k_ManifestSum);
	case sources::k_RunAwayFromPlayer:
		return Clamp01(-mind.attitudeToPlayer);
	case sources::k_AngerFromSadness:
	case sources::k_PlayFromSadness:
	case sources::k_TirednessFromSadness:
		return creature_desires::SourceValue(desires[Desire::Sadness], sources::k_Sadness).value_or(0.0f);
	default:
		return std::nullopt;
	}
}

/// Everything on the land a creature might look at
std::vector<creature_look::Candidate> GatherCandidates(ecs::Registry& registry)
{
	std::vector<creature_look::Candidate> candidates;
	const auto add = [&candidates](entt::entity entity, creature_look::Interest kind, const glm::vec3& point) {
		candidates.push_back({.id = entt::to_integral(entity), .kind = kind, .point = point});
	};
	registry.Each<const Creature, const Transform>([&](entt::entity entity, const Creature& creature, const Transform& at) {
		add(entity, creature_look::Interest::Creature,
		    at.position + glm::vec3(0.0f, creature_look::k_HeadHeight * ShownSize(creature), 0.0f));
	});
	registry.Each<const Villager, const Transform>([&](entt::entity entity, const Villager&, const Transform& at) {
		add(entity, creature_look::Interest::Villager, at.position + glm::vec3(0.0f, k_VillagerHeadHeight, 0.0f));
	});
	registry.Each<const Abode, const Transform>([&](entt::entity entity, const Abode&, const Transform& at) {
		add(entity, creature_look::Interest::Abode, at.position + glm::vec3(0.0f, k_AbodeLookHeight, 0.0f));
	});
	registry.Each<const Tree, const Transform>([&](entt::entity entity, const Tree&, const Transform& at) {
		add(entity, creature_look::Interest::Tree, at.position + glm::vec3(0.0f, k_TreeLookHeight, 0.0f));
	});
	registry.Each<const Temple, const Transform>([&](entt::entity entity, const Temple&, const Transform& at) {
		add(entity, creature_look::Interest::Citadel, at.position + glm::vec3(0.0f, k_CitadelLookHeight, 0.0f));
	});
	// A miracle's doves catch a creature's eye as much as anything; other animals less so
	registry.Each<const Animal, const Transform>([&](entt::entity entity, const Animal& animal, const Transform& at) {
		add(entity, animal.type == AnimalInfo::SpellDove ? creature_look::Interest::Dove : creature_look::Interest::Animal,
		    at.position);
	});
	return candidates;
}

void ApplyEyes(CreatureEyes* eyes, creature_mind::Eyes look)
{
	if (eyes == nullptr || look == creature_mind::Eyes::Unchanged)
	{
		return;
	}
	const bool sleepy = look == creature_mind::Eyes::Sleepy;
	eyes->mode = look == creature_mind::Eyes::Closed ? creature_eyes::Mode::Closed : creature_eyes::Mode::Calm;
	eyes->openness = sleepy ? k_SleepyOpenness : 0.0f;
	eyes->blink.intervalMs = sleepy ? k_SleepyBlinkIntervalMs : creature_eyes::k_BlinkIntervalMs;
}

/// Sends the creature where its mind wants it to go; whether it can't go there and gives up what it is doing
/// Turning to face a point as a script asks, or down the slope, unless it faces it already
void FaceForScript(entt::entity creature, const creature_mind::Movement& move)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* transform = registry.TryGet<const Transform>(creature);
	const auto* self = registry.TryGet<const CreatureLocomotion>(creature);
	if (transform == nullptr || self == nullptr)
	{
		return;
	}
	const glm::vec2 from {transform->position.x, transform->position.z};
	auto point = move.point;
	if (move.kind == creature_mind::Movement::Kind::FaceDownSlope)
	{
		point = creature_mind::DownSlope(from, [](glm::vec2 at) -> std::optional<float> {
			if (!Locator::terrainSystem::has_value() || !map_coords::InBounds(glm::vec3(at.x, 0.0f, at.y)))
			{
				return std::nullopt;
			}
			return Locator::terrainSystem::value().GetHeightAt(at);
		});
	}
	const auto offset = point - from;
	const auto turn = creature_locomotion::WrapAngle(creature_locomotion::HeadingOf(offset) - self->heading);
	// Down the slope it turns unless it stands on top of the place; to a script's point, unless it nearly faces it
	const bool turns = move.kind == creature_mind::Movement::Kind::FaceDownSlope
	                       ? glm::length(offset) >= creature_mind::k_OnTopOf
	                       : creature_mind::NeedsToTurnToFace(glm::length(offset), turn);
	if (turns)
	{
		Locator::creatureLocomotionSystem::value().TurnToFace(creature, point);
	}
}

bool Move(entt::entity creature, const creature_mind::Commands& commands)
{
	if (!Locator::creatureLocomotionSystem::has_value())
	{
		return false;
	}
	auto& locomotion = Locator::creatureLocomotionSystem::value();
	if (commands.stopMoving)
	{
		locomotion.Stop(creature);
	}
	if (!commands.move.has_value())
	{
		return false;
	}
	using Kind = creature_mind::Movement::Kind;
	using Pace = CreatureLocomotionSystemInterface::Pace;
	const auto& move = *commands.move;
	const auto pace = move.run ? Pace::Run : Pace::Walk;
	const auto object = move.object.has_value() ? std::optional(static_cast<entt::entity>(*move.object)) : std::nullopt;
	switch (move.kind)
	{
	case Kind::ToPoint:
	case Kind::Nearby:
		if (locomotion.MoveTo(creature, move.point, pace, move.minDistance, move.maxDistance) ==
		        CreatureLocomotionSystemInterface::MoveResult::InvalidDestination &&
		    move.giveUpIfUnreachable)
		{
			return true;
		}
		break;
	case Kind::ToObject:
		if (object.has_value())
		{
			locomotion.MoveToObject(creature, *object, pace, move.maxDistance);
		}
		break;
	case Kind::Follow:
		if (object.has_value())
		{
			locomotion.Follow(creature, *object, move.maxDistance, pace);
		}
		break;
	case Kind::FleeFrom:
		locomotion.FleeFrom(creature, move.point);
		break;
	case Kind::TurnToFace:
		locomotion.TurnToFace(creature, move.point);
		break;
	case Kind::FacePoint:
	case Kind::FaceDownSlope:
		FaceForScript(creature, move);
		break;
	case Kind::GoNearObject:
	case Kind::GetAwayFromObject:
	case Kind::TurnToFaceObject:
	case Kind::ToThrowPosition:
		// Overseen turn by turn by the mind as it casts (StartSubMove)
		break;
	}
	return false;
}

bool IsSubMove(creature_mind::Movement::Kind kind)
{
	using Kind = creature_mind::Movement::Kind;
	return kind == Kind::GoNearObject || kind == Kind::GetAwayFromObject || kind == Kind::TurnToFaceObject ||
	       kind == Kind::ToThrowPosition;
}

/// Where the creature's head looks for a step that gives it something to look at: the top of a thing, or its foot, a
/// point, or the camera. It doesn't look at a thing it holds, nor at one it is walking past within ten of it.
/// Where the player's hand nearest a creature is, none for a creature without a player or when there is no hand
std::optional<glm::vec3> NearestHand(const ecs::Registry& registry, entt::entity creature)
{
	const auto* body = registry.TryGet<const Creature>(creature);
	const auto* transform = registry.TryGet<const Transform>(creature);
	if (body == nullptr || body->owner == PlayerNames::NEUTRAL || transform == nullptr || !Locator::handSystem::has_value())
	{
		return std::nullopt;
	}
	std::optional<glm::vec3> nearest;
	for (const auto& hand : Locator::handSystem::value().GetPlayerHandPositions())
	{
		if (hand.has_value() &&
		    (!nearest.has_value() || glm::distance(*hand, transform->position) < glm::distance(*nearest, transform->position)))
		{
			nearest = hand;
		}
	}
	return nearest;
}

std::optional<glm::vec3> GazePoint(const ecs::Registry& registry, entt::entity creature, const creature_mind::Gaze& gaze,
                                   bool moving)
{
	constexpr float k_NoLookWhileMovingWithin = 10.0f;
	if (gaze.camera)
	{
		return Locator::camera::has_value() ? std::optional(Locator::camera::value().GetOrigin()) : std::nullopt;
	}
	if (gaze.hand)
	{
		return NearestHand(registry, creature);
	}
	if (!gaze.object.has_value())
	{
		return gaze.point;
	}
	const auto thing = static_cast<entt::entity>(*gaze.object);
	const auto at = ecs::systems::object_measures::PositionOf(registry, thing);
	const auto* self = registry.TryGet<const Transform>(creature);
	const auto* held = registry.TryGet<const HeldByCreature>(thing);
	if (!at.has_value() || self == nullptr || (held != nullptr && held->creature == creature) ||
	    (moving &&
	     glm::distance(glm::vec2(self->position.x, self->position.z), glm::vec2(at->x, at->z)) < k_NoLookWhileMovingWithin))
	{
		return std::nullopt;
	}
	if (gaze.itself)
	{
		return *at;
	}
	const glm::vec2 flat {at->x, at->z};
	const auto ground = Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt(flat) : at->y;
	if (gaze.bottom)
	{
		return glm::vec3 {flat.x, ground, flat.y};
	}
	return glm::vec3 {flat.x, at->y + ecs::systems::object_measures::Height(registry, thing), flat.y};
}

/// Where a script has the creature keep its head turned: a thing where it stands, or a point. The focus is let go once
/// the thing is gone or the script has let the creature go.
std::optional<glm::vec3> ScriptFocusPoint(const ecs::Registry& registry, entt::entity creature, CreatureMindState& mind)
{
	if (!mind.scriptFocus.has_value())
	{
		return std::nullopt;
	}
	if (!registry.AllOf<ScriptControlled>(creature))
	{
		mind.scriptFocus.reset();
		return std::nullopt;
	}
	if (!mind.scriptFocus->object.has_value())
	{
		return mind.scriptFocus->point;
	}
	const auto at = ecs::systems::object_measures::PositionOf(registry, static_cast<entt::entity>(*mind.scriptFocus->object));
	if (!at.has_value())
	{
		mind.scriptFocus.reset();
	}
	return at;
}

/// The rest of the agenda is no use: it gives it up
void GiveUp(creature_mind::IdleMind& idle)
{
	idle.step = idle.agenda.size();
	idle.stepStarted = false;
	idle.gaveUp = true;
}

/// Whether the thing the creature's step waits for is back on the map: out of any hand, flight or tornado. None when the
/// thing is gone, and when the step waits for nothing.
std::optional<bool> WaitedForInMap(const ecs::Registry& registry, const creature_mind::IdleMind& idle)
{
	if (idle.step >= idle.agenda.size() || idle.agenda[idle.step].kind != creature_mind::Step::Kind::WaitInMap ||
	    !idle.agenda[idle.step].object.has_value())
	{
		return std::nullopt;
	}
	const auto object = static_cast<entt::entity>(*idle.agenda[idle.step].object);
	if (!registry.Valid(object))
	{
		return std::nullopt;
	}
	return registry.AllOf<MapCellResident>(object) &&
	       !registry.AnyOf<InHand, HeldByCreature, InPhysics, CarriedByTornado>(object);
}

/// Whether the thing a step watches flying is still flying: moved by the physics and really up in the air; none when it
/// is gone or the step watches nothing flying
std::optional<bool> WatchedFlying(const ecs::Registry& registry, const creature_mind::IdleMind& idle)
{
	if (idle.step >= idle.agenda.size() || idle.agenda[idle.step].kind != creature_mind::Step::Kind::WatchFlying ||
	    !idle.agenda[idle.step].object.has_value())
	{
		return std::nullopt;
	}
	const auto object = static_cast<entt::entity>(*idle.agenda[idle.step].object);
	if (!registry.Valid(object))
	{
		return std::nullopt;
	}
	return registry.AllOf<InPhysics>(object) && ecs::villager_physics::IsActuallyInTheAir(object);
}

/// Its player's camera as a creature watching it sees it, only while its step watches the camera; none for a creature
/// without a player
std::optional<creature_mind::Senses::CameraView> CameraViewOf(const ecs::Registry& registry, entt::entity creature,
                                                              const creature_mind::IdleMind& idle)
{
	if (idle.step >= idle.agenda.size() || idle.agenda[idle.step].kind != creature_mind::Step::Kind::WatchCamera ||
	    !Locator::camera::has_value())
	{
		return std::nullopt;
	}
	const auto* body = registry.TryGet<const Creature>(creature);
	const auto* transform = registry.TryGet<const Transform>(creature);
	const auto* self = registry.TryGet<const CreatureLocomotion>(creature);
	if (body == nullptr || body->owner == PlayerNames::NEUTRAL || transform == nullptr || self == nullptr)
	{
		return std::nullopt;
	}
	const auto eye = Locator::camera::value().GetOrigin();
	const glm::vec2 point {eye.x, eye.z};
	const auto offset = point - glm::vec2(transform->position.x, transform->position.z);
	return creature_mind::Senses::CameraView {
	    .distance = glm::distance(eye, transform->position),
	    .groundDistance = glm::length(offset),
	    .point = point,
	    .turn = creature_locomotion::WrapAngle(creature_locomotion::HeadingOf(offset) - self->heading),
	};
}

/// The player's hand as a creature watching it sees it, only while its step watches the hand; none for a creature
/// without a player
std::optional<creature_mind::Senses::CameraView> HandViewOf(const ecs::Registry& registry, entt::entity creature,
                                                            const creature_mind::IdleMind& idle)
{
	if (idle.step >= idle.agenda.size() || idle.agenda[idle.step].kind != creature_mind::Step::Kind::WatchHand)
	{
		return std::nullopt;
	}
	const auto hand = NearestHand(registry, creature);
	const auto* transform = registry.TryGet<const Transform>(creature);
	const auto* self = registry.TryGet<const CreatureLocomotion>(creature);
	if (!hand.has_value() || transform == nullptr || self == nullptr)
	{
		return std::nullopt;
	}
	const glm::vec2 point {hand->x, hand->z};
	const auto offset = point - glm::vec2(transform->position.x, transform->position.z);
	return creature_mind::Senses::CameraView {
	    .distance = glm::distance(*hand, transform->position),
	    .groundDistance = glm::length(offset),
	    .point = point,
	    .turn = creature_locomotion::WrapAngle(creature_locomotion::HeadingOf(offset) - self->heading),
	};
}

/// Lets go of the miracle the creature holds, as it stops what it was doing
void ReleaseCast(entt::entity creature)
{
	if (Locator::magicSystem::has_value())
	{
		Locator::magicSystem::value().ReleaseCreatureCast(creature);
	}
}

void Apply(const creature_mind::Commands& commands, CreatureAnimation& animation, CreatureEyes* eyes)
{
	if (commands.endSit)
	{
		animation.body = creature_layers::EndLoop(animation.body);
	}
	if (commands.playOnce.has_value())
	{
		if (auto body = creature_layers::PlayOnce(animation.body, *commands.playOnce, commands.mirrored))
		{
			animation.body = *body;
		}
	}
	if (commands.startSequence.has_value())
	{
		const auto& sequence = *commands.startSequence;
		if (auto body = creature_layers::PlaySequence(animation.body, sequence[0], sequence[1], sequence[2], commands.holdLoop,
		                                              commands.mirrored))
		{
			animation.body = *body;
		}
	}
	if (commands.playOverlay.has_value())
	{
		if (auto gesture = creature_layers::PlayGesture(animation.gesture, *commands.playOverlay))
		{
			animation.gesture = *gesture;
		}
	}
	if (commands.relaxFace)
	{
		animation.face = creature_layers::RelaxFace(animation.face);
	}
	if (commands.face.has_value())
	{
		animation.face = creature_layers::PullFace(animation.face, creature_face::AnimationOf(commands.face->face),
		                                           commands.face->milliseconds, commands.face->cue);
	}
	ApplyEyes(eyes, commands.eyes);
}

/// What, besides why, picks the faces the creature pulls
creature_face::Feelings FeelingsOf(const CreatureMindState& mind)
{
	return {.variety = mind.idle.faceVariety, .attitudeToPlayer = mind.attitudeToPlayer, .idlePick = 0};
}

bool IsNight()
{
	return Locator::skySystem::has_value() && Locator::skySystem::value().GetClock().IsVisualNight();
}

float DesireValue(const creature_desires::Desires& desires, Desire desire)
{
	const auto& state = desires[desire];
	return state.activated ? state.value : 0.0f;
}

/// What something is worth to eat, if anything
std::optional<float> FoodValueOf(entt::entity entity)
{
	return Locator::creatureObjectActionSystem::has_value() ? Locator::creatureObjectActionSystem::value().FoodValueOf(entity)
	                                                        : std::nullopt;
}

/// The food within reach of a point the creature would most like to eat, and where it is: the nearest, unless it has
/// learnt to prefer some kinds of food (weigh says how useful each is, 0.1 for food nothing is known about); food it has
/// learnt is worse than that it leaves alone
std::optional<std::pair<entt::entity, glm::vec2>> NearestFood(ecs::Registry& registry, glm::vec2 from,
                                                              const std::function<float(entt::entity)>& weigh)
{
	constexpr float k_FoodDistanceWeight = 0.05f;
	std::optional<std::pair<entt::entity, glm::vec2>> nearest;
	float best = 0.0f;
	const auto consider = [&](entt::entity entity, const Transform& at) {
		const glm::vec2 point {at.position.x, at.position.z};
		const auto distance = glm::distance(point, from);
		if (distance > k_FoodSearchDistance || registry.AllOf<HeldByCreature>(entity) || !FoodValueOf(entity).has_value())
		{
			return;
		}
		const auto usefulness = weigh ? weigh(entity) : creature_planner::k_DefaultUsefulness;
		if (usefulness < creature_planner::k_DefaultUsefulness)
		{
			return;
		}
		const auto score = usefulness * creature_planner::DistancePriority(false, distance, k_FoodDistanceWeight) +
		                   1e-6f * (k_FoodSearchDistance - distance);
		if (score > best)
		{
			best = score;
			nearest = {entity, point};
		}
	};
	registry.Each<const Villager, const Transform>(
	    [&](entt::entity entity, const Villager&, const Transform& at) { consider(entity, at); });
	registry.Each<const MobileObject, const Transform>(
	    [&](entt::entity entity, const MobileObject&, const Transform& at) { consider(entity, at); });
	registry.Each<const Pot, const Transform>(
	    [&](entt::entity entity, const Pot&, const Transform& at) { consider(entity, at); });
	return nearest;
}

/// The nearest water to a point, searching the cells round it: shallow water it can wade into or open sea, and where it
/// can stand at its edge
std::optional<creature_mind::Wants::WaterSpot> NearestWater(glm::vec2 from)
{
	if (!Locator::creatureLocomotionSystem::has_value() || !Locator::terrainSystem::has_value())
	{
		return std::nullopt;
	}
	const auto& land = Locator::creatureLocomotionSystem::value().GetWalkableLand();
	const auto& terrain = Locator::terrainSystem::value();
	const auto cellX = static_cast<int32_t>(std::floor(from.x / creature_route::k_CellSize));
	const auto cellZ = static_cast<int32_t>(std::floor(from.y / creature_route::k_CellSize));
	std::optional<glm::vec2> nearest;
	float best = std::numeric_limits<float>::max();
	for (int32_t x = cellX - k_WaterSearchCells; x <= cellX + k_WaterSearchCells; ++x)
	{
		for (int32_t z = cellZ - k_WaterSearchCells; z <= cellZ + k_WaterSearchCells; ++z)
		{
			if (x < 0 || z < 0 || x >= creature_route::k_CellsPerSide || z >= creature_route::k_CellsPerSide)
			{
				continue;
			}
			const auto centre = (glm::vec2(static_cast<float>(x), static_cast<float>(z)) + 0.5f) * creature_route::k_CellSize;
			const auto ground = land.At(x, z);
			const bool sea = ground == creature_route::Ground::Blocked && terrain.GetHeightAt(centre) <= 0.0f;
			if (ground != creature_route::Ground::Water && !sea)
			{
				continue;
			}
			const auto distance = glm::distance(centre, from);
			if (distance < best)
			{
				best = distance;
				nearest = centre;
			}
		}
	}
	if (!nearest.has_value())
	{
		return std::nullopt;
	}
	// It stands at the edge of the water on its own side, or in the shallows
	const auto shore = land.NearestValid(*nearest, creature_route::k_DestinationClearance, k_ShoreSearchDistance);
	if (!shore.has_value())
	{
		return std::nullopt;
	}
	return creature_mind::Wants::WaterSpot {.shore = *shore, .water = *nearest};
}

/// The nearest thing a creature could pick up, and where it is
std::optional<std::pair<entt::entity, glm::vec2>> NearestObject(ecs::Registry& registry, entt::entity creature, glm::vec2 from)
{
	if (!Locator::creatureObjectActionSystem::has_value())
	{
		return std::nullopt;
	}
	const auto& hands = Locator::creatureObjectActionSystem::value();
	std::optional<std::pair<entt::entity, glm::vec2>> nearest;
	float best = k_ObjectSearchDistance;
	registry.Each<const MobileObject, const Transform>([&](entt::entity entity, const MobileObject&, const Transform& at) {
		const glm::vec2 point {at.position.x, at.position.z};
		const auto distance = glm::distance(point, from);
		if (distance <= best && !registry.AllOf<HeldByCreature>(entity) && hands.CanPickUp(creature, entity))
		{
			best = distance;
			nearest = {entity, point};
		}
	});
	return nearest;
}

/// The nearest home or tree, to hurl something at
std::optional<glm::vec2> NearestHurlTarget(ecs::Registry& registry, glm::vec2 from)
{
	std::optional<glm::vec2> nearest;
	float best = k_HurlSearchDistance;
	const auto consider = [&](const Transform& at) {
		const glm::vec2 point {at.position.x, at.position.z};
		const auto distance = glm::distance(point, from);
		if (distance <= best)
		{
			best = distance;
			nearest = point;
		}
	};
	registry.Each<const Abode, const Transform>([&](const Abode&, const Transform& at) { consider(at); });
	registry.Each<const Tree, const Transform>([&](const Tree&, const Transform& at) { consider(at); });
	return nearest;
}

/// The needs the mind might see to now, and the food and water at hand for them
creature_mind::Wants WantsOf(ecs::Registry& registry, entt::entity creature, const creature_desires::Desires& desires,
                             glm::vec2 position, const std::function<float(entt::entity)>& weighFood)
{
	creature_mind::Wants wants {
	    .hunger = DesireValue(desires, Desire::Hunger),
	    .tiredness = DesireValue(desires, Desire::Tiredness),
	    .poo = DesireValue(desires, Desire::Poo),
	    .water = DesireValue(desires, Desire::Water),
	    .curiosity = DesireValue(desires, Desire::Curiosity),
	    .play = DesireValue(desires, Desire::Play),
	    .anger = DesireValue(desires, Desire::Anger),
	    .holding = registry.AllOf<CreatureHeldObject>(creature),
	};
	if (!wants.holding && std::max({wants.curiosity, wants.play, wants.anger}) >= creature_mind::k_ActOnDesire)
	{
		if (const auto object = NearestObject(registry, creature, position))
		{
			wants.object = entt::to_integral(object->first);
			wants.objectPoint = object->second;
		}
		if (wants.anger >= creature_mind::k_ActOnDesire)
		{
			wants.hurlTarget = NearestHurlTarget(registry, position);
		}
	}
	if (wants.hunger >= creature_mind::k_ActOnNeed)
	{
		if (const auto food = NearestFood(registry, position, weighFood))
		{
			wants.food = entt::to_integral(food->first);
			wants.foodPoint = food->second;
		}
	}
	if (wants.water >= creature_mind::k_ActOnNeed)
	{
		wants.waterSpot = NearestWater(position);
	}
	return wants;
}

/// The least a creature's desires are lessened to by its actions, by its species
float DesireFloorOf(entt::entity creature)
{
	const auto* body = Locator::entitiesRegistry::value().TryGet<const Creature>(creature);
	if (body == nullptr || !Locator::infoConstants::has_value())
	{
		return 0.0f;
	}
	const auto& species = Locator::infoConstants::value().creature;
	const auto row = creature::InfoRow(body->species);
	return row < species.size() ? species.at(row).desireFloor : 0.0f;
}

/// Having done an action, the desire it satisfies is less, by the game's action table
void Lessen(entt::entity creature, creature_desires::Desires& desires, std::string_view action)
{
	if (!Locator::infoConstants::has_value())
	{
		return;
	}
	const auto& actions = Locator::infoConstants::value().creatureAction;
	const auto found = std::ranges::find_if(actions, [action](const auto& row) {
		return std::string_view(row.name.data(), strnlen(row.name.data(), row.name.size())) == action;
	});
	if (found != actions.end() && found->desire < creature_desires::k_DesireCount)
	{
		creature_desires::LessenAfterAction(desires[static_cast<Desire>(found->desire)], found->desireMultiplier,
		                                    DesireFloorOf(creature));
	}
}

/// Having done an action, its body pays for it
void BodyPaysFor(entt::entity creature, std::string_view action)
{
	if (Locator::creaturePhysiologySystem::has_value())
	{
		Locator::creaturePhysiologySystem::value().FinishAction(creature, action);
	}
}

void Satisfied(entt::entity creature, creature_desires::Desires& desires, std::string_view action)
{
	Lessen(creature, desires, action);
	BodyPaysFor(creature, action);
}

/// What a step did to the body
void TakeEffect(entt::entity creature, const creature_mind::Commands& commands, creature_desires::Desires& desires)
{
	using creature_mind::Effect;
	if (commands.effect == Effect::None || !Locator::creaturePhysiologySystem::has_value())
	{
		return;
	}
	auto& physiology = Locator::creaturePhysiologySystem::value();
	switch (commands.effect)
	{
	case Effect::Eat:
		// It took its nourishment as it ate what it held
		Satisfied(creature, desires, k_EatAction);
		break;
	case Effect::Examined:
		Satisfied(creature, desires, k_ExamineAction);
		break;
	case Effect::ThrewAbout:
		Satisfied(creature, desires, k_ThrowAboutAction);
		break;
	case Effect::Hurled:
		Satisfied(creature, desires, k_HurlAction);
		break;
	case Effect::Drink:
		physiology.Drink(creature);
		Satisfied(creature, desires, k_DrinkAction);
		creature_desires::Suppress(desires, Desire::Water, k_WaterSuppressSeconds, k_TurnsPerSecond);
		break;
	case Effect::Poo:
		physiology.Poo(creature);
		Satisfied(creature, desires, k_PooAction);
		desires[Desire::Poo].value = 0.0f;
		creature_desires::Suppress(desires, Desire::Poo, k_PooSuppressSeconds, k_TurnsPerSecond);
		break;
	case Effect::Puke:
		physiology.Puke(creature);
		Satisfied(creature, desires, k_PukeAction);
		break;
	case Effect::Slept:
		Satisfied(creature, desires, k_SleepAction);
		break;
	case Effect::CameRound:
		physiology.WakeFromFaint(creature);
		break;
	case Effect::Completed:
	case Effect::Observed:
	case Effect::Inspected:
	case Effect::TookFromPile:
	case Effect::TookFromHand:
	case Effect::None:
		break;
	}
}

/// A step saying the plan's action counts as done there, before the step that finishes it, sees to its desire
void CountCompleted(entt::entity creature, CreatureMindState& mind, const creature_mind::Commands& commands,
                    const creature_mind_tables::Tables* tables)
{
	if (commands.effect != creature_mind::Effect::Completed || !mind.planActive || !mind.planner.current.has_value() ||
	    tables == nullptr || !mind.desires.has_value() || mind.planner.current->action >= tables->actions.size())
	{
		return;
	}
	Satisfied(creature, *mind.desires, tables->actions[mind.planner.current->action].name);
}

/// The town nearest a point along the ground, of any player or none
entt::entity NearestTown(const ecs::Registry& registry, glm::vec3 point)
{
	entt::entity nearest = entt::null;
	auto best = std::numeric_limits<float>::max();
	registry.Each<const Town, const Transform>([&](entt::entity entity, const Town&, const Transform& at) {
		const float distance = glm::distance(glm::vec2(at.position.x, at.position.z), glm::vec2(point.x, point.z));
		if (distance < best)
		{
			best = distance;
			nearest = entity;
		}
	});
	return nearest;
}

/// The food a creature fishing brings out of the sea: a handful at its feet of a thousand for each unit of its size,
/// drawn half as big again as the creature, belonging to the nearest town. Nothing comes out of a fish farm.
std::optional<entt::entity> FoodFromTheSea(ecs::Registry& registry, entt::entity creature)
{
	constexpr float k_FoodPerSize = 1000.0f;
	constexpr float k_ScalePerSize = 1.5f;
	const auto* transform = registry.TryGet<const Transform>(creature);
	if (transform == nullptr)
	{
		return std::nullopt;
	}
	const auto* body = registry.TryGet<const Creature>(creature);
	const float size = body != nullptr ? body->size : 1.0f;
	const auto amount = static_cast<int32_t>(size * k_FoodPerSize);
	const auto food = ecs::archetypes::PotArchetype::Create(transform->position, 0.0f, PotInfo::WheatInHand, amount,
	                                                        NearestTown(registry, transform->position));
	if (food == entt::null)
	{
		return std::nullopt;
	}
	if (auto* foodTransform = registry.TryGet<Transform>(food))
	{
		foodTransform->scale = glm::vec3(size * k_ScalePerSize);
	}
	return food;
}

/// The food a creature takes from a pile: a handful at its feet of a thousand for each unit of its size, no more than the
/// pile holds, drawn three and three quarter times the creature's size, belonging to the nearest town. None from a pile
/// with no food.
// TODO(creature-actions-3): the game puts the handful a little short of the creature's reach towards the pile, by a
// measure of its body not ported yet; here it appears at its feet, as fished food does
std::optional<entt::entity> FoodFromPile(ecs::Registry& registry, entt::entity creature, entt::entity pile)
{
	constexpr float k_FoodPerSize = 1000.0f;
	constexpr float k_ScalePerSize = 3.75f;
	const auto* transform = registry.TryGet<const Transform>(creature);
	if (transform == nullptr || !registry.Valid(pile) || !Locator::resourceStoreSystem::has_value())
	{
		return std::nullopt;
	}
	const auto* body = registry.TryGet<const Creature>(creature);
	const float size = body != nullptr ? body->size : 1.0f;
	const auto held = Locator::resourceStoreSystem::value().GetResource(pile, ResourceType::Food);
	const auto amount = std::min(static_cast<uint32_t>(size * k_FoodPerSize), held);
	// The handful appears on the ground beside the creature, most of its reach towards the pile, at the creature's own
	// height
	glm::vec3 at = transform->position;
	const auto* pileTransform = registry.TryGet<const Transform>(pile);
	const auto reach = Locator::creatureObjectActionSystem::has_value()
	                       ? Locator::creatureObjectActionSystem::value().PickUpStraightReach(creature)
	                       : std::nullopt;
	if (pileTransform != nullptr && reach.has_value())
	{
		const auto point = creature_reach::HandfulPoint({transform->position.x, transform->position.z},
		                                                {pileTransform->position.x, pileTransform->position.z}, *reach);
		at = {point.x, transform->position.y, point.y};
	}
	const auto food = ecs::archetypes::PotArchetype::Create(at, 0.0f, PotInfo::WheatInHand, static_cast<int32_t>(amount),
	                                                        NearestTown(registry, transform->position));
	if (food == entt::null)
	{
		return std::nullopt;
	}
	if (auto* foodTransform = registry.TryGet<Transform>(food))
	{
		foodTransform->scale = glm::vec3(size * k_ScalePerSize);
	}
	return food;
}

/// Tells the creature's hands what to do with a thing
void Order(ecs::Registry& registry, entt::entity creature, const creature_mind::Commands& commands)
{
	if (!commands.object.has_value() || !Locator::creatureObjectActionSystem::has_value())
	{
		return;
	}
	auto& hands = Locator::creatureObjectActionSystem::value();
	const auto& order = *commands.object;
	const auto object = order.object.has_value() ? std::optional(static_cast<entt::entity>(*order.object)) : std::nullopt;
	const auto ground = Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt(order.point) : 0.0f;
	const glm::vec3 point {order.point.x, ground, order.point.y};
	using Kind = creature_mind::ObjectOrder::Kind;
	switch (order.kind)
	{
	case Kind::PickUp:
		if (object.has_value() && registry.Valid(*object))
		{
			hands.PickUp(creature, *object);
		}
		else
		{
			hands.Cancel(creature);
		}
		break;
	case Kind::PutDown:
		hands.PutDown(creature);
		break;
	case Kind::Discard:
		if (order.animation != 0)
		{
			hands.DiscardWith(creature, order.animation);
		}
		else
		{
			hands.Discard(creature);
		}
		break;
	case Kind::Eat:
		hands.EatHeld(creature);
		break;
	case Kind::Keep:
		hands.Keep(creature, order.animation);
		break;
	case Kind::Throw:
	case Kind::ThrowNearby:
		// At a point in the air when it has a height, as the player's camera; otherwise at the ground there
		hands.Throw(creature, {point.x, order.pointHeight.value_or(point.y), point.z});
		break;
	case Kind::TakeFromHand:
		if (!object.has_value() || !registry.Valid(*object) || !hands.TakeFromHand(creature, *object, order.mirrored))
		{
			hands.Cancel(creature);
		}
		break;
	case Kind::Destroy:
		if (object.has_value() && registry.Valid(*object))
		{
			hands.Destroy(creature, *object);
		}
		else
		{
			hands.Cancel(creature);
		}
		break;
	case Kind::PointAt:
		if (object.has_value() || order.seconds.has_value() || order.pointHeight.has_value())
		{
			// At a thing, wherever it is now and goes; or at a point in the air, as the camera
			const auto* at =
			    object.has_value() && registry.Valid(*object) ? registry.TryGet<const Transform>(*object) : nullptr;
			if (object.has_value() && at == nullptr)
			{
				hands.Cancel(creature);
				break;
			}
			const auto aim = at != nullptr ? at->position : glm::vec3 {point.x, order.pointHeight.value_or(point.y), point.z};
			hands.PointAtFor(creature, aim, order.seconds.value_or(creature_object_actions::k_PointSeconds), object);
			break;
		}
		hands.PointAt(creature, point);
		break;
	case Kind::Catch:
		if (!object.has_value() || !registry.Valid(*object) || !hands.Catch(creature, *object))
		{
			hands.Cancel(creature);
		}
		break;
	case Kind::ThrowInStore:
	{
		// At the store's own place on the ground, taking as long to fly there as it would take to fall the distance
		// along the ground
		const auto* store = object.has_value() && registry.Valid(*object) ? registry.TryGet<const Transform>(*object) : nullptr;
		const auto* self = registry.TryGet<const Transform>(creature);
		if (store == nullptr || self == nullptr)
		{
			hands.Cancel(creature);
			break;
		}
		const glm::vec2 at {store->position.x, store->position.z};
		const auto height = Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt(at) : 0.0f;
		const auto flight = creature_throw::FlightTime(glm::distance(at, glm::vec2(self->position.x, self->position.z)));
		hands.ThrowTaking(creature, {at.x, height, at.y}, flight);
		break;
	}
	case Kind::TakeFromPile:
		if (const auto food = object.has_value() ? FoodFromPile(registry, creature, *object) : std::nullopt; food.has_value())
		{
			if (auto* mind = registry.TryGet<CreatureMindState>(creature))
			{
				mind->takenFood = *food;
				mind->takenFrom = *object;
			}
			hands.PickUp(creature, *food);
		}
		else
		{
			hands.Cancel(creature);
		}
		break;
	case Kind::FishFromSea:
		if (const auto food = FoodFromTheSea(registry, creature); food.has_value())
		{
			hands.PickUp(creature, *food);
		}
		else
		{
			hands.Cancel(creature);
		}
		break;
	}
}

/// The pile a creature took food from gives up as much as the food it picked up holds
void GiveUpTakenFood(ecs::Registry& registry, CreatureMindState& mind)
{
	const auto food = std::exchange(mind.takenFood, entt::null);
	const auto pile = std::exchange(mind.takenFrom, entt::null);
	const auto* pot = registry.Valid(food) ? registry.TryGet<const Pot>(food) : nullptr;
	if (pot == nullptr || !registry.Valid(pile) || !Locator::resourceStoreSystem::has_value())
	{
		return;
	}
	static_cast<void>(Locator::resourceStoreSystem::value().RemoveResource(pile, ResourceType::Food, pot->amount));
}

creature_mind::HandsState HandsOf(entt::entity creature)
{
	if (!Locator::creatureObjectActionSystem::has_value())
	{
		return creature_mind::HandsState::Idle;
	}
	using State = CreatureObjectActionSystemInterface::State;
	switch (Locator::creatureObjectActionSystem::value().GetState(creature))
	{
	case State::Busy:
		return creature_mind::HandsState::Busy;
	case State::Done:
		return creature_mind::HandsState::Done;
	case State::Failed:
		return creature_mind::HandsState::Failed;
	case State::Idle:
		break;
	}
	return creature_mind::HandsState::Idle;
}
} // namespace openblack::ecs::systems::mind_detail

using namespace openblack::ecs::systems::mind_detail;

void CreatureMindSystem::ProcessTurn()
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto random = [this](uint32_t range) {
		return range == 0 ? 0u : std::uniform_int_distribution<uint32_t>(0, range - 1)(_random);
	};
	std::optional<std::vector<creature_look::Candidate>> candidates;
	const bool night = IsNight();
	// The creatures passing out this turn, which are knocked out once every mind has thought
	std::vector<entt::entity> passingOut;
	// Where the player's camera is and looks, which tells whether it is looking at the player's creatures
	struct View
	{
		glm::vec3 origin;
		glm::vec3 focus;
	};
	const auto view =
	    Locator::camera::has_value()
	        ? std::optional(View {.origin = Locator::camera::value().GetOrigin(), .focus = Locator::camera::value().GetFocus()})
	        : std::nullopt;

	// Whether the cinema bars are in for a cut scene, and which script task brought them
	const bool barsIn =
	    Locator::cinematicDirectorSystem::has_value() && Locator::cinematicDirectorSystem::value().IsWideScreenOn();
	const auto barsOwner = barsIn ? Locator::cinematicDirectorSystem::value().GetWideScreenOwner() : 0u;
	const auto* lookTables = GetTables();
	const auto cutSceneLook = lookTables != nullptr
	                              ? creature_mind_tables::FindAction(*lookTables, creature_mind::k_CutSceneLookAction)
	                              : std::nullopt;

	registry.Each<const Creature, CreatureMindState, CreatureAnimation, const Transform>(
	    [&](entt::entity entity, const Creature& creature, CreatureMindState& mind, CreatureAnimation& animation,
	        const Transform& transform) {
		    SetUpMind(entity, creature, mind);
		    if (mind.pendingFile != nullptr)
		    {
			    TakeUpFile(entity, mind);
		    }
		    for (const auto& teaching : std::exchange(mind.pendingTeaching, {}))
		    {
			    SetKnowsAction(entity, teaching.type, teaching.action, teaching.knows);
		    }
		    ++mind.turn;
		    // What it thinks its player wants fades
		    creature_perceived_desires::Fade(mind.perceivedDesires);
		    if (mind.learnt.has_value())
		    {
			    creature_learning::Age(mind.learnt->contexts, k_TurnSeconds);
			    for (auto& turns : mind.learnt->turnsSinceDone)
			    {
				    turns = std::min(turns + 1, creature_planner::k_NoveltyTurns);
			    }
		    }
		    if (mind.desiresPhase != mind.developmentPhase)
		    {
			    creature_desires::ActivateForPhase(*mind.desires, Phases(), mind.developmentPhase);
			    mind.desiresPhase = mind.developmentPhase;
		    }
		    mind.secondsAlone += k_TurnSeconds;
		    // What it believed about things now gone is forgotten, and going into another cell it notes the nearest temple
		    ForgetGoneThings(mind);
		    NoteNearestTemple(mind, transform);
		    const bool held = HeldByHand(entity);
		    // Only a creature's own player pays it attention, and not while the hand holds it; nor do its eyes sweep about
		    // it then
		    if (!held)
		    {
			    if (creature.owner == PlayerNames::PLAYER_ONE && view.has_value() &&
			        creature_attention::IsLookingAt(view->origin, view->focus, transform.position))
			    {
				    ++mind.turnsLookedAt;
			    }
			    SweepEyes(creature, mind, animation, transform);
		    }
		    if (mind.feedbackSeconds.has_value())
		    {
			    *mind.feedbackSeconds += k_TurnSeconds;
		    }
		    // Held by the player's hand its mind stands still, but for showing what it wants once it has turned to the
		    // player
		    if (held)
		    {
			    if (mind.showDesireOnHold)
			    {
				    ShowDesireOnHold(entity, mind, animation);
			    }
			    return;
		    }
		    auto* needs = registry.TryGet<CreatureNeeds>(entity);
		    // Under a script's control its desires stand still and it chooses nothing for itself: it only carries out
		    // what it is given, and stands about once that is done
		    const bool scripted = registry.AllOf<ScriptControlled>(entity);
		    if (!scripted)
		    {
			    const auto body = needs != nullptr ? needs->needs : creature_physiology::Needs {};
			    creature_desires::UpdateSources(*mind.desires,
			                                    [&mind, &body, night](uint32_t type, const creature_desires::Desires& desires) {
				                                    return ReadSource(type, desires, mind, body, night);
			                                    });
			    creature_desires::UpdateDesires(*mind.desires, k_TurnsPerSecond);
		    }

		    auto* eyes = registry.TryGet<CreatureEyes>(entity);
		    // Paused, fighting or knocked out, the mind leaves the body alone
		    if (mind.paused || registry.AnyOf<CreatureFighting, CreatureKnockedOut>(entity))
		    {
			    return;
		    }
		    // Exhausted, starved or out of life, it passes out as a fight's loser does: it lies where it fell, then a
		    // player's creature is carried to its pen to come round. A creature just put in the world stands up first.
		    const bool fainting = needs != nullptr && needs->faint.has_value();
		    if (fainting && Locator::creatureFightSystem::has_value())
		    {
			    if (mind.turn > k_TurnsBeforePassingOut)
			    {
				    TellFainting(entity);
				    needs->faint.reset();
				    passingOut.push_back(entity);
			    }
			    return;
		    }
		    // Following the leash to the hand, it is the leash's; passing out (above) lets go of it
		    if (mind.leash.obeying)
		    {
			    return;
		    }
		    // Without the fights' knock-outs, it drops where it stands and comes round there
		    if (fainting && !creature_mind::IsUnconscious(mind.idle))
		    {
			    if (Locator::creatureLocomotionSystem::has_value())
			    {
				    Locator::creatureLocomotionSystem::value().Stop(entity);
			    }
			    // As with any change of plan, a pose ends and an action plays out before it drops
			    animation.body = creature_layers::EndLoop(animation.body);
			    TellFainting(entity);
			    creature_mind::Plan(mind.idle, creature_mind::Activity::Faint, creature_mind::Faint());
			    needs->faint.reset();
		    }
		    // Free to choose what to do next, it weighs every desire's plans first; the idle policy only chooses when
		    // none is pressing enough
		    if (!scripted && !mind.planActive && mind.idle.step >= mind.idle.agenda.size() &&
		        !(needs != nullptr && needs->faint.has_value()))
		    {
			    // A try at a miracle that fizzled is what it shows first
			    ShowFizzle(entity, mind);
			    if (mind.idle.step >= mind.idle.agenda.size())
			    {
				    PlanCreature(entity, mind, true);
			    }
		    }
		    // A script's cut scene makes every creature free to think for itself look at the camera, again and again
		    // until the cut scene is over; what it is doing then plays out to its end
		    if (!scripted && cutSceneLook.has_value())
		    {
			    const bool looking =
			        mind.planActive && mind.planner.current.has_value() && mind.planner.current->action == *cutSceneLook;
			    if (creature_mind::StartsLookAtCameraInCutScene(barsIn, barsOwner, looking))
			    {
				    ForcePlan(entity, {.desire = creature_desires::Desire::ObeyPlayer,
				                       .action = creature_mind::k_CutSceneLookAction,
				                       .object = entity});
			    }
		    }
		    const bool moving =
		        Locator::creatureLocomotionSystem::has_value() && Locator::creatureLocomotionSystem::value().IsMoving(entity);
		    if (mind.idle.step < mind.idle.agenda.size())
		    {
			    ++mind.stepTurns;
		    }
		    StepSubMove(entity, creature_layers::IsPlaying(animation.body));
		    // A catch takes up the body until it is over, but the plan to catch is carried out as soon as the catch starts
		    const bool catching = Locator::creatureObjectActionSystem::has_value() &&
		                          Locator::creatureObjectActionSystem::value().IsCatching(entity);
		    const bool catchStep = mind.idle.stepStarted && mind.idle.step < mind.idle.agenda.size() &&
		                           mind.idle.agenda[mind.idle.step].kind == creature_mind::Step::Kind::Object &&
		                           mind.idle.agenda[mind.idle.step].order.kind == creature_mind::ObjectOrder::Kind::Catch;
		    const creature_mind::Senses senses {
		        .seconds = k_TurnSeconds,
		        .bodyBusy = creature_layers::IsPlaying(animation.body) || moving || catching,
		        .bodyLooping = creature_layers::IsLooping(animation.body),
		        .moving = moving,
		        .subMove = SubMoveOf(entity),
		        .position = glm::vec2(transform.position.x, transform.position.z),
		        .strongest = creature_desires::StrongestShowable(*mind.desires, creature_mind::k_MinDesireShown),
		        .feedbackSeconds = mind.feedbackSeconds,
		        .feedbackWasStroke = mind.feedbackWasStroke,
		        // Food and water are only looked for when it is free to choose what to do next
		        .wants = mind.idle.step >= mind.idle.agenda.size()
		                     ? WantsOf(registry, entity, *mind.desires, glm::vec2(transform.position.x, transform.position.z),
		                               [&registry, &mind, entity](entt::entity food) {
			                               return FoodUsefulness(registry, mind, entity, food);
		                               })
		                     : creature_mind::Wants {},
		        .rested = needs != nullptr && needs->rested,
		        .hands = catchStep && catching ? creature_mind::HandsState::Done : HandsOf(entity),
		        .objectInMap = WaitedForInMap(registry, mind.idle),
		        .objectFlying = WatchedFlying(registry, mind.idle),
		        .feelings = FeelingsOf(mind),
		        .choosesNext = !scripted,
		        .camera = CameraViewOf(registry, entity, mind.idle),
		        .hand = HandViewOf(registry, entity, mind.idle),
		        .swap = SwapViewOf(entity),
		        .overlayPlaying = animation.gesture.animation.has_value(),
		    };
		    const auto commands = creature_mind::Think(mind.idle, senses, random);
		    Apply(commands, animation, eyes);
		    if (Move(entity, commands))
		    {
			    GiveUp(mind.idle);
		    }
		    if (commands.move.has_value() && IsSubMove(commands.move->kind) && mind.idle.step < mind.idle.agenda.size())
		    {
			    StartSubMove(entity, *commands.move, mind.idle.agenda[mind.idle.step].seconds);
		    }
		    if (commands.releaseCast)
		    {
			    ReleaseCast(entity);
		    }
		    // The miracle is tried as the casting pose's loop begins; the rest of the agenda is no use without it
		    if (commands.cast.has_value() && commands.cast->object.has_value() &&
		        !TryMiracle(entity, static_cast<MagicType>(commands.cast->magicType),
		                    static_cast<entt::entity>(*commands.cast->object)))
		    {
			    mind.idle.step = mind.idle.agenda.size();
			    mind.idle.stepStarted = false;
			    mind.idle.gaveUp = true;
		    }
		    Order(registry, entity, commands);
		    StepSwap(entity, commands);
		    if (commands.douse.has_value())
		    {
			    if (Douse(entity, static_cast<entt::entity>(*commands.douse)))
			    {
				    mind.desireSeenTo = true;
			    }
		    }
		    TakeEffect(entity, commands, *mind.desires);
		    // Once it has picked up the food it took from a pile, the pile gives that much up
		    if (commands.effect == creature_mind::Effect::TookFromPile)
		    {
			    GiveUpTakenFood(registry, mind);
		    }
		    if (commands.effect == creature_mind::Effect::Observed && commands.effectObject.has_value())
		    {
			    const auto thing = static_cast<entt::entity>(*commands.effectObject);
			    if (const auto belief = BeliefOf(registry, thing, entity))
			    {
				    mind.observedKinds.insert(belief->type);
			    }
			    else if (registry.AllOf<Animal>(thing))
			    {
				    // Animals are their own kind of thing to a creature, though nothing else it knows of them is kept yet
				    mind.observedKinds.insert(creature_tree::belief_types::k_Animal);
			    }
		    }
		    // Having looked another creature over, it notes when, and waits a while before it does again
		    if (commands.effect == creature_mind::Effect::Inspected && commands.effectObject.has_value())
		    {
			    if (auto* inspected = mind.beliefs.Find(static_cast<entt::entity>(*commands.effectObject));
			        inspected != nullptr && Locator::reactionSystem::has_value())
			    {
				    inspected->inspected = Locator::reactionSystem::value().CurrentTurn();
			    }
		    }
		    CountCompleted(entity, mind, commands, GetTables());
		    // Taking in what a thing is does nothing for the desire the plan serves
		    if (commands.effect != creature_mind::Effect::None && commands.effect != creature_mind::Effect::Observed &&
		        commands.effect != creature_mind::Effect::Inspected && commands.effect != creature_mind::Effect::TookFromPile &&
		        commands.effect != creature_mind::Effect::TookFromHand)
		    {
			    mind.satisfiedByEffect = true;
		    }
		    FollowAgenda(entity, mind);
		    // Once it has taken what the hand gave it, its desires choose what to do with it
		    if (commands.effect == creature_mind::Effect::TookFromHand && commands.effectObject.has_value())
		    {
			    UseGift(entity, static_cast<entt::entity>(*commands.effectObject));
		    }
		    if (needs != nullptr)
		    {
			    needs->rest = creature_mind::IsUnconscious(mind.idle) ? CreatureNeeds::Rest::Unconscious
			                  : creature_mind::IsAsleep(mind.idle)    ? CreatureNeeds::Rest::Asleep
			                                                          : CreatureNeeds::Rest::Awake;
		    }

		    // Looking about, the head turns to the most interesting thing in sight, or ahead
		    mind.lookingAbout = commands.lookAbout;
		    const auto focus = ScriptFocusPoint(registry, entity, mind);
		    if (focus.has_value())
		    {
			    // A script's focus holds the head whatever the creature is doing
			    animation.lookAt = focus;
		    }
		    else if (commands.gaze.has_value())
		    {
			    animation.lookAt = GazePoint(registry, entity, *commands.gaze, moving);
		    }
		    else if (commands.lookAbout)
		    {
			    if (!candidates.has_value())
			    {
				    candidates = GatherCandidates(registry);
			    }
			    const auto self = entt::to_integral(entity);
			    std::vector<creature_look::Candidate> others;
			    others.reserve(candidates->size());
			    std::ranges::copy_if(*candidates, std::back_inserter(others),
			                         [self](const creature_look::Candidate& candidate) { return candidate.id != self; });
			    const creature_look::Viewer viewer {
			        .position = transform.position,
			        .ahead = -(transform.rotation * glm::vec3(0.0f, 0.0f, 1.0f)),
			        .size = ShownSize(creature),
			    };
			    mind.look = creature_look::LookAbout(mind.look, others, viewer, k_TurnsPerSecond);
			    animation.lookAt = mind.look.id.has_value() ? mind.look.point : creature_look::PointAhead(viewer);
		    }
		    else
		    {
			    animation.lookAt.reset();
		    }
		    if (eyes != nullptr)
		    {
			    eyes->lookAt = animation.lookAt;
		    }
	    });
	for (const auto entity : passingOut)
	{
		Locator::creatureFightSystem::value().KnockOut(entity);
	}
}

bool CreatureMindSystem::PlayAction(entt::entity creature, size_t animation)
{
	auto* body = Locator::entitiesRegistry::value().TryGet<CreatureAnimation>(creature);
	if (body == nullptr)
	{
		return false;
	}
	const auto played = creature_layers::PlayOnce(body->body, animation, std::bernoulli_distribution(0.5)(_random));
	if (played.has_value())
	{
		body->body = *played;
	}
	return played.has_value();
}

bool CreatureMindSystem::PlayGesture(entt::entity creature, size_t animation)
{
	auto* body = Locator::entitiesRegistry::value().TryGet<CreatureAnimation>(creature);
	if (body == nullptr)
	{
		return false;
	}
	const auto played = creature_layers::PlayGesture(body->gesture, animation);
	if (played.has_value())
	{
		body->gesture = *played;
	}
	return played.has_value();
}

void CreatureMindSystem::PullFace(entt::entity creature, size_t animation)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (auto* body = registry.TryGet<CreatureAnimation>(creature))
	{
		body->face = creature_layers::PullFace(body->face, animation, k_ToldFaceMs, creature_face::Cue::Told);
	}
}

std::optional<creature_face::Request> CreatureMindSystem::ShowFeeling(entt::entity creature, creature_face::Cue cue)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* body = registry.TryGet<CreatureAnimation>(creature);
	auto* mind = registry.TryGet<CreatureMindState>(creature);
	if (body == nullptr || mind == nullptr)
	{
		return std::nullopt;
	}
	const auto random = [this](uint32_t range) {
		return range == 0 ? 0u : std::uniform_int_distribution<uint32_t>(0, range - 1)(_random);
	};
	const creature_mind::Senses senses {.feelings = FeelingsOf(*mind)};
	const auto face = creature_mind::PullFace(mind->idle, cue, senses, random);
	if (face.has_value())
	{
		body->face = creature_layers::PullFace(body->face, creature_face::AnimationOf(face->face), face->milliseconds, cue);
	}
	return face;
}

bool CreatureMindSystem::SitDown(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* body = registry.TryGet<CreatureAnimation>(creature);
	auto* mind = registry.TryGet<CreatureMindState>(creature);
	if (body == nullptr || mind == nullptr || creature_layers::IsPlaying(body->body))
	{
		return false;
	}
	// A paused mind won't get it up again: it sits until told to stand
	if (mind->paused)
	{
		body->body =
		    *creature_layers::PlaySequence(body->body, animations::k_StartSit, animations::k_Sit, animations::k_EndSit);
		return true;
	}
	const auto random = [this](uint32_t range) {
		return range == 0 ? 0u : std::uniform_int_distribution<uint32_t>(0, range - 1)(_random);
	};
	Abandon(*mind);
	creature_mind::Plan(mind->idle, creature_mind::Activity::Told, {creature_mind::SitDown(random)});
	return true;
}

void CreatureMindSystem::UpdateAttitudeFromFeedback(entt::entity creature, float feedback)
{
	auto* mind = Locator::entitiesRegistry::value().TryGet<CreatureMindState>(creature);
	if (mind == nullptr)
	{
		return;
	}
	// Good feedback shows it its player wants compassion, bad anger, and it warms or cools to the player
	creature_perceived_desires::Increase(
	    mind->perceivedDesires, static_cast<size_t>(feedback > 0.0f ? Desire::Compassion : Desire::Anger), std::abs(feedback));
	// The scripts read how strongly it was last rewarded or punished
	mind->interactionMagnitude = feedback;
	mind->attitudeToPlayer = creature_feedback::AttitudeAfter(mind->attitudeToPlayer, feedback);
	mind->averageFeedback = creature_feedback::AverageAfter(mind->averageFeedback, feedback);
}

void CreatureMindSystem::ChangeDesireSource(entt::entity creature, uint32_t type, float amount)
{
	auto* mind = Locator::entitiesRegistry::value().TryGet<CreatureMindState>(creature);
	if (mind != nullptr && mind->desires.has_value())
	{
		creature_desires::ChangeSource(*mind->desires, type, amount);
	}
}

void CreatureMindSystem::HandTookHold(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* mind = registry.TryGet<CreatureMindState>(creature);
	if (mind == nullptr)
	{
		return;
	}
	// It stops and turns to face the player's eye
	if (Locator::creatureLocomotionSystem::has_value() && Locator::camera::has_value())
	{
		const auto eye = Locator::camera::value().GetOrigin();
		Locator::creatureLocomotionSystem::value().TurnToFace(creature, glm::vec2(eye.x, eye.z));
	}
	mind->showDesireOnHold = true;
}

void CreatureMindSystem::ShowDesireOnHold(entt::entity creature, CreatureMindState& mind, CreatureAnimation& animation)
{
	const bool turning =
	    Locator::creatureLocomotionSystem::has_value() && Locator::creatureLocomotionSystem::value().IsMoving(creature);
	if (turning || creature_layers::IsPlaying(animation.body))
	{
		return;
	}
	mind.showDesireOnHold = false;
	std::optional<Desire> credited;
	if (mind.learnt.has_value())
	{
		if (const auto best = creature_learning::BestContext(mind.learnt->contexts); best.has_value())
		{
			credited = mind.learnt->contexts.at(*best).desire;
		}
	}
	const auto planned =
	    mind.planActive && mind.planner.current.has_value() ? std::optional(mind.planner.current->desire) : std::nullopt;
	const auto desire = creature_help::DesireShownOnHold(credited, planned);
	if (!desire.has_value())
	{
		return;
	}
	PlayAction(creature, *creature_desires::EmoteFor(*desire));
	// The help says what it wants, even though the hand is on it
	if (Locator::creatureHelpSystem::has_value())
	{
		Locator::creatureHelpSystem::value().Raise(
		    creature,
		    {.kind = creature_help::Kind::CurrentDesire, .parameter = static_cast<uint32_t>(*desire), .forced = true});
	}
}

void CreatureMindSystem::ReceiveFeedback(entt::entity creature, float feedback)
{
	namespace sources = creature_desires::sources;
	auto& registry = Locator::entitiesRegistry::value();
	auto* mind = registry.TryGet<CreatureMindState>(creature);
	if (mind == nullptr)
	{
		return;
	}
	mind->secondsAlone = 0.0f;
	mind->lastFeedback = CreatureMindState::Feedback {.value = feedback, .activity = mind->idle.activity};
	// Let go of, however slightly it was stroked or slapped, it stops whatever it was doing: it stops walking, puts down
	// what it was doing with a thing, drops its agenda and gets up out of a sitting or lying pose. An action it is playing,
	// such as reeling from the last slap, plays on.
	if (Locator::creatureObjectActionSystem::has_value())
	{
		Locator::creatureObjectActionSystem::value().Cancel(creature);
	}
	if (Locator::creatureLocomotionSystem::has_value())
	{
		Locator::creatureLocomotionSystem::value().Stop(creature);
	}
	mind->idle.agenda.clear();
	mind->idle.step = 0;
	mind->idle.stepStarted = false;
	if (auto* body = registry.TryGet<CreatureAnimation>(creature);
	    body != nullptr && body->body.kind == creature_layers::BodyAction::Kind::Sequence)
	{
		body->body = creature_layers::EndLoop(body->body);
	}
	// Too slight to count, it only looks at the player
	if (std::abs(feedback) <= creature_feedback::k_SlightFeedback)
	{
		if (Locator::creatureLocomotionSystem::has_value() && Locator::camera::has_value())
		{
			const auto eye = Locator::camera::value().GetOrigin();
			Locator::creatureLocomotionSystem::value().TurnToFace(creature, glm::vec2(eye.x, eye.z));
		}
		return;
	}
	UpdateAttitudeFromFeedback(creature, feedback);
	mind->feedbackSeconds = 0.0f;
	mind->feedbackWasStroke = feedback > 0.0f;
	LearnFromFeedback(creature, *mind, feedback);
	// It shows how it feels as soon as it is free, unless it is doing something again as it was stroked for
	mind->idle.showDesireSeconds = 0.0f;
	if (!mind->planActive && mind->idle.activity != creature_mind::Activity::ShowDesire)
	{
		mind->idle.agenda.resize(std::min(mind->idle.agenda.size(), mind->idle.step + (mind->idle.stepStarted ? 1 : 0)));
	}
}

bool CreatureMindSystem::ForceAction(entt::entity creature, size_t animation, bool mirrored,
                                     std::optional<creature_face::Request> face, float interruptsAfter)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* body = registry.TryGet<CreatureAnimation>(creature);
	auto* mind = registry.TryGet<CreatureMindState>(creature);
	if (body == nullptr || mind == nullptr || creature_mind::IsUnconscious(mind->idle))
	{
		return false;
	}
	// The face is pulled even when the body is too far into something to take the action
	if (face.has_value())
	{
		body->face =
		    creature_layers::PullFace(body->face, creature_face::AnimationOf(face->face), face->milliseconds, face->cue);
	}
	// Not while it is only just into something
	if (Locator::creatureObjectActionSystem::has_value())
	{
		if (const auto progress = Locator::creatureObjectActionSystem::value().GetProgress(creature);
		    progress.has_value() && *progress < interruptsAfter)
		{
			return false;
		}
	}
	if (creature_layers::IsPlaying(body->body) && Locator::creatureAnimationSystem::has_value())
	{
		const auto duration = Locator::creatureAnimationSystem::value().AnimationDuration(
		    creature, creature_layers::CurrentAnimation(body->body));
		if (duration.has_value() && *duration > 0.0f && body->body.timeMs / *duration < interruptsAfter)
		{
			return false;
		}
	}
	const auto played = creature_layers::PlayOnce(creature_layers::BodyAction {}, animation, mirrored);
	if (!played.has_value())
	{
		return false;
	}
	if (Locator::creatureObjectActionSystem::has_value())
	{
		Locator::creatureObjectActionSystem::value().Cancel(creature);
	}
	if (Locator::creatureLocomotionSystem::has_value())
	{
		Locator::creatureLocomotionSystem::value().Stop(creature);
	}
	body->body = *played;
	// Only the body is taken: the mind goes on with what it was doing once the action has played, as its agenda waits for
	// a busy body
	return true;
}

bool CreatureMindSystem::Replan(entt::entity creature, creature_mind::Activity activity,
                                std::vector<creature_mind::Step> agenda)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* mind = registry.TryGet<CreatureMindState>(creature);
	auto* body = registry.TryGet<CreatureAnimation>(creature);
	if (mind == nullptr || body == nullptr || agenda.empty())
	{
		return false;
	}
	// It stops walking, unless a catch it has started is turning it to face what it catches: changing what it does
	// leaves a catch to its body
	// TODO(physics): whether the game's stopping cuts short the turn a catch starts with wasn't traced
	const bool catching =
	    Locator::creatureObjectActionSystem::has_value() && Locator::creatureObjectActionSystem::value().IsCatching(creature);
	if (Locator::creatureLocomotionSystem::has_value() && !catching)
	{
		Locator::creatureLocomotionSystem::value().Stop(creature);
	}
	// A sitting or lying pose goes on to its end, but an action it is playing, such as reeling from a slap, plays on: the
	// new agenda waits for the body to be free. It lets go of any miracle it holds.
	body->body = creature_layers::EndLoop(body->body);
	ReleaseCast(creature);
	if (auto* eyes = registry.TryGet<CreatureEyes>(creature))
	{
		ApplyEyes(eyes, creature_mind::Eyes::Normal);
	}
	Abandon(*mind);
	creature_mind::Plan(mind->idle, activity, std::move(agenda));
	return true;
}

bool CreatureMindSystem::Sleep(entt::entity creature)
{
	return Replan(creature, creature_mind::Activity::Sleep, creature_mind::Sleep([this](uint32_t range) {
		              return range == 0 ? 0u : std::uniform_int_distribution<uint32_t>(0, range - 1)(_random);
	              }));
}

bool CreatureMindSystem::Eat(entt::entity creature, std::optional<entt::entity> food)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* transform = registry.TryGet<const Transform>(creature);
	if (!food.has_value() && transform != nullptr)
	{
		if (const auto nearest = NearestFood(registry, glm::vec2(transform->position.x, transform->position.z), {}))
		{
			food = nearest->first;
		}
	}
	if (!food.has_value() || !FoodValueOf(*food).has_value())
	{
		return false;
	}
	return Replan(creature, creature_mind::Activity::Eat, creature_mind::Eat(entt::to_integral(*food)));
}

bool CreatureMindSystem::Drink(entt::entity creature)
{
	const auto* transform = Locator::entitiesRegistry::value().TryGet<const Transform>(creature);
	if (transform == nullptr)
	{
		return false;
	}
	const auto water = NearestWater(glm::vec2(transform->position.x, transform->position.z));
	return water.has_value() &&
	       Replan(creature, creature_mind::Activity::Drink, creature_mind::Drink(water->shore, water->water));
}

bool CreatureMindSystem::Poo(entt::entity creature)
{
	return Replan(creature, creature_mind::Activity::Poo, creature_mind::Poo([this](uint32_t range) {
		              return range == 0 ? 0u : std::uniform_int_distribution<uint32_t>(0, range - 1)(_random);
	              }));
}

bool CreatureMindSystem::Puke(entt::entity creature)
{
	return Replan(creature, creature_mind::Activity::Puke, creature_mind::Puke());
}

bool CreatureMindSystem::Faint(entt::entity creature)
{
	return Replan(creature, creature_mind::Activity::Faint, creature_mind::Faint());
}

void CreatureMindSystem::Wake(entt::entity creature)
{
	if (auto* mind = Locator::entitiesRegistry::value().TryGet<CreatureMindState>(creature))
	{
		mind->idle.wakeWanted = true;
	}
}

void CreatureMindSystem::StandUp(entt::entity creature)
{
	if (auto* body = Locator::entitiesRegistry::value().TryGet<CreatureAnimation>(creature))
	{
		body->body = creature_layers::EndLoop(body->body);
	}
}

void CreatureMindSystem::FoughtFight(entt::entity creature, [[maybe_unused]] bool won)
{
	// Winning or losing, the fight is done. The game also has the loser learn from losing, which isn't known.
	auto* mind = Locator::entitiesRegistry::value().TryGet<CreatureMindState>(creature);
	if (mind != nullptr && mind->desires.has_value())
	{
		Satisfied(creature, *mind->desires, k_FightAction);
	}
}

void CreatureMindSystem::ForceCatch(entt::entity creature, entt::entity object)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* mind = registry.Valid(creature) ? registry.TryGet<CreatureMindState>(creature) : nullptr;
	const auto* tables = GetTables();
	if (mind == nullptr || tables == nullptr)
	{
		return;
	}
	// What it was doing stops as a failure
	Abandon(*mind);
	ReleaseCast(creature);
	creature_mind::Step step {.kind = creature_mind::Step::Kind::Object};
	step.order = {.kind = creature_mind::ObjectOrder::Kind::Catch, .object = entt::to_integral(object)};
	if (!Replan(creature, creature_mind::Activity::Planned, {step}))
	{
		return;
	}
	if (const auto action = creature_mind_tables::FindAction(*tables, k_CatchAction))
	{
		mind->planner.current = creature_planner::Plan {.desire = Desire::Play,
		                                                .action = *action,
		                                                .object = entt::to_integral(object),
		                                                .goalUsefulness = creature_planner::k_ForcedScore,
		                                                .actionPriority = creature_planner::k_ForcedScore,
		                                                .priority = creature_planner::k_ForcedScore};
		mind->planActive = true;
		mind->planSerial = mind->idle.serial;
		mind->agendaSeen = mind->idle.serial;
		mind->satisfiedByEffect = false;
		mind->desireSeenTo = false;
	}
	// Having decided to play, the desires that go against it are held back
	if (mind->desires.has_value())
	{
		creature_learning::SuppressOpposed(*mind->desires, Desire::Play, tables->dependencies, k_TurnsPerSecond);
	}
}

bool CreatureMindSystem::TakeFromHand(entt::entity creature, entt::entity object, bool leftHand, glm::vec2 camera)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* mind = registry.Valid(creature) ? registry.TryGet<CreatureMindState>(creature) : nullptr;
	const auto* tables = GetTables();
	if (mind == nullptr || tables == nullptr || !mind->desires.has_value() || !registry.Valid(object))
	{
		return false;
	}
	const auto action = creature_mind_tables::FindAction(*tables, k_TakeFromHandAction);
	if (!action.has_value())
	{
		return false;
	}
	// It drops what it was doing at once, its body's action ended there, as a failure
	if (Locator::creatureObjectActionSystem::has_value() &&
	    Locator::creatureObjectActionSystem::value().GetState(creature) == CreatureObjectActionSystemInterface::State::Busy)
	{
		Locator::creatureObjectActionSystem::value().Cancel(creature);
	}
	Abandon(*mind);
	ReleaseCast(creature);
	if (!Replan(creature, creature_mind::Activity::Planned,
	            creature_mind::TakeFromHand(entt::to_integral(object), camera, leftHand)))
	{
		return false;
	}
	// Curious about what it is given, the plan is on the creature itself
	const auto self = entt::to_integral(creature);
	mind->planner.current = creature_planner::Plan {.desire = Desire::Curiosity,
	                                                .action = *action,
	                                                .object = self,
	                                                .goalUsefulness = creature_planner::k_ForcedScore,
	                                                .actionPriority = creature_planner::k_ForcedScore,
	                                                .priority = creature_planner::k_ForcedScore};
	mind->planActive = true;
	mind->planSerial = mind->idle.serial;
	mind->agendaSeen = mind->idle.serial;
	mind->satisfiedByEffect = false;
	mind->desireSeenTo = false;
	creature_learning::SuppressOpposed(*mind->desires, Desire::Curiosity, tables->dependencies, k_TurnsPerSecond);
	return true;
}

bool CreatureMindSystem::IsTakingFromHand(entt::entity creature) const
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* mind = registry.Valid(creature) ? registry.TryGet<const CreatureMindState>(creature) : nullptr;
	if (mind == nullptr || mind->idle.step >= mind->idle.agenda.size())
	{
		return false;
	}
	// The take is still ahead of it, or under way
	return std::ranges::any_of(mind->idle.agenda.begin() + static_cast<std::ptrdiff_t>(mind->idle.step),
	                           mind->idle.agenda.end(), [](const creature_mind::Step& step) {
		                           return step.kind == creature_mind::Step::Kind::Object &&
		                                  step.order.kind == creature_mind::ObjectOrder::Kind::TakeFromHand;
	                           });
}

void CreatureMindSystem::UseGift(entt::entity creature, entt::entity object)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* mind = registry.Valid(creature) ? registry.TryGet<CreatureMindState>(creature) : nullptr;
	if (mind == nullptr || !mind->desires.has_value() || !registry.Valid(object) ||
	    !Locator::creatureObjectActionSystem::has_value() ||
	    Locator::creatureObjectActionSystem::value().GetHeld(creature) != object)
	{
		return;
	}
	// TODO(creature-giving): the game first carries on the plan the creature had before it was given the thing, when that
	// plan's action takes a thing of this sort (see bw1-decomp creature/creature_giving.md section 4); not done yet
	const auto& desires = *mind->desires;
	const auto weigh = [&](Desire desire) {
		return creature_giving::Weighed {.desire = desires[desire].value,
		                                 .usefulness = ActivityUsefulness(creature, *mind, desire, object)};
	};
	const auto belief = BeliefOf(registry, object, creature);
	const auto kind = belief.has_value() ? belief->type : creature_tree::belief_types::k_Other;
	// TODO(creature-giving): the game also keeps which kinds it has learnt it can't eat; openblack has no such memory yet
	const auto choice = creature_giving::ChooseUse({
	    .curiosity = weigh(Desire::Curiosity),
	    .hunger = weigh(Desire::Hunger),
	    .playWithPlayer = weigh(Desire::PlayWithPlayer),
	    .cantEatKind = false,
	    .givenKindBefore = mind->givenKinds.contains(kind),
	});
	if (choice.remembersKind)
	{
		mind->givenKinds.insert(kind);
	}
	const auto number = entt::to_integral(object);
	switch (choice.use)
	{
	case creature_giving::Use::Examine:
		ForcePlan(creature, {.desire = Desire::Curiosity,
		                     .action = k_ExamineGivenAction,
		                     .object = object,
		                     .agenda = creature_mind::ExamineGiven(number)});
		break;
	case creature_giving::Use::Eat:
		ForcePlan(
		    creature,
		    {.desire = Desire::Hunger, .action = k_EatGivenAction, .object = object, .agenda = creature_mind::EatGiven()});
		break;
	case creature_giving::Use::Throw:
	{
		const auto eye = Locator::camera::has_value() ? Locator::camera::value().GetOrigin() : glm::vec3(0.0f);
		ForcePlan(creature, {.desire = Desire::PlayWithPlayer,
		                     .action = k_ThrowGivenAction,
		                     .object = object,
		                     .agenda = creature_mind::ThrowGiven(eye)});
		break;
	}
	case creature_giving::Use::AsLed:
		ForcePlanOn(creature, object);
		break;
	}
}

bool CreatureMindSystem::ForceActivity(entt::entity creature, const ForcedActivity& forced)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* mind = registry.Valid(creature) ? registry.TryGet<CreatureMindState>(creature) : nullptr;
	auto* body = mind != nullptr ? registry.TryGet<CreatureAnimation>(creature) : nullptr;
	const auto* tables = GetTables();
	if (mind == nullptr || body == nullptr || tables == nullptr || !mind->learnt.has_value() || !mind->desires.has_value())
	{
		return false;
	}
	const auto action = creature_mind_tables::FindAction(*tables, forced.action);
	if (!action.has_value())
	{
		return false;
	}
	// What it was doing stops as a failure, whatever comes of the new plan; a reaction it was in has been ended by
	// whatever forces the plan
	if (Locator::creatureObjectActionSystem::has_value() &&
	    Locator::creatureObjectActionSystem::value().GetState(creature) == CreatureObjectActionSystemInterface::State::Busy)
	{
		Locator::creatureObjectActionSystem::value().Cancel(creature);
	}
	Abandon(*mind);
	ReleaseCast(creature);
	const auto toNumber = [](std::optional<entt::entity> entity) {
		return entity.has_value() ? std::optional(entt::to_integral(*entity)) : std::nullopt;
	};
	const auto actionObject = forced.actionObject != entt::null ? toNumber(forced.actionObject) : std::nullopt;
	creature_plan_actions::Situation situation {
	    .instrument = toNumber(forced.instrument),
	    .handFull = registry.AllOf<CreatureHeldObject>(creature),
	};
	Situate(creature, situation);
	auto built = PlanAgenda(creature, *action, actionObject, situation);
	SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Creature {} is made to {} on {}: {}", entt::to_integral(creature), forced.action,
	                    actionObject.value_or(0), built.has_value() ? "planned" : "can't be planned");
	if (!built.has_value())
	{
		// The plan couldn't be made: it is left doing nothing, to decide afresh, once a pose it was in has ended and an
		// action it was playing has played out
		if (Locator::creatureLocomotionSystem::has_value())
		{
			Locator::creatureLocomotionSystem::value().Stop(creature);
		}
		body->body = creature_layers::EndLoop(body->body);
		creature_mind::Plan(mind->idle, creature_mind::Activity::None, {});
		return false;
	}
	auto& [executor, agenda] = *built;
	if (!Replan(creature, executor->activity, std::move(agenda)))
	{
		return false;
	}
	mind->planner.current = creature_planner::Plan {
	    .desire = forced.desire,
	    .action = *action,
	    .object = actionObject,
	    .goalUsefulness = creature_planner::k_ForcedScore,
	    .actionPriority = creature_planner::k_ForcedScore,
	    .priority = creature_planner::k_ForcedScore,
	    .activityObject = toNumber(forced.activityObject),
	    .instrument = situation.instrument,
	};
	mind->planActive = true;
	mind->planSerial = mind->idle.serial;
	mind->agendaSeen = mind->idle.serial;
	mind->satisfiedByEffect = false;
	mind->desireSeenTo = false;
	// It is remembered for the player's feedback, unless nothing says why it wants what the plan serves
	auto& desire = (*mind->desires)[forced.desire];
	if (const auto source = creature_learning::PlanSource(desire); source.has_value())
	{
		const auto& info = tables->actions[*action];
		creature_learning::Remember(mind->learnt->contexts,
		                            {
		                                .action = *action,
		                                .desire = forced.desire,
		                                .object = actionObject,
		                                .belief = actionObject.has_value()
		                                              ? BeliefOf(registry, static_cast<entt::entity>(*actionObject), creature)
		                                              : std::nullopt,
		                                .learnable = info.learnable,
		                                .windowSeconds = info.learningWindowSeconds,
		                                .source = *source,
		                            });
	}
	creature_learning::SuppressOpposed(*mind->desires, forced.desire, tables->dependencies, k_TurnsPerSecond);
	return true;
}

void CreatureMindSystem::AbandonAction(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (auto* mind = registry.Valid(creature) ? registry.TryGet<CreatureMindState>(creature) : nullptr)
	{
		Abandon(*mind);
	}
	ReleaseCast(creature);
}

void CreatureMindSystem::ChangeDesires(entt::entity creature, const std::function<void(creature_desires::Desires&)>& change)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* mind = registry.Valid(creature) ? registry.TryGet<CreatureMindState>(creature) : nullptr;
	const auto* body = mind != nullptr ? registry.TryGet<const Creature>(creature) : nullptr;
	if (body == nullptr)
	{
		return;
	}
	SetUpMind(creature, *body, *mind);
	if (!mind->desires.has_value())
	{
		return;
	}
	// Its stage of growing up decides which desires are active before the script has its say, not after
	if (mind->desiresPhase != mind->developmentPhase)
	{
		creature_desires::ActivateForPhase(*mind->desires, Phases(), mind->developmentPhase);
		mind->desiresPhase = mind->developmentPhase;
	}
	change(*mind->desires);
}

void CreatureMindSystem::SetAgendaPriority(entt::entity creature, float priority)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* mind = registry.Valid(creature) ? registry.TryGet<CreatureMindState>(creature) : nullptr;
	// It stands only until the next plan is carried out, which brings its own
	if (mind != nullptr && mind->planner.current.has_value())
	{
		mind->planner.current->priority = priority;
	}
}

void CreatureMindSystem::MoveToStage(entt::entity creature, uint32_t stage)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* mind = registry.Valid(creature) ? registry.TryGet<CreatureMindState>(creature) : nullptr;
	if (mind == nullptr)
	{
		return;
	}
	mind->developmentPhase = stage;
	// Its desires are set for the stage at once, even for the stage it is at already; a mind yet to think sets them up
	// for the stage when it first does
	if (mind->desires.has_value())
	{
		creature_desires::ActivateForPhase(*mind->desires, Phases(), stage);
		mind->desiresPhase = stage;
	}
	else
	{
		mind->desiresPhase.reset();
	}
	Abandon(*mind);
}
