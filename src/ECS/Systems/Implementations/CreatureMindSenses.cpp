/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// What a creature sees and where it looks: what catches its eye, where its player and the hand are, and the things
// that it waits for

#define LOCATOR_IMPLEMENTATIONS

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

#include <glm/gtx/vec_swizzle.hpp>
#include <spdlog/spdlog.h>

#include "3D/CreatureBody.h"
#include "3D/DayNightClock.h"
#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "Camera/Camera.h"
#include "Common/GUtilsAngle.h"
#include "Common/GUtilsDistance.h"
#include "Common/GameRandom.h"
#include "Creature/CreatureActionAgendas.h"
#include "Creature/CreatureAnimation.h"
#include "Creature/CreatureAttention.h"
#include "Creature/CreatureDesireSources.h"
#include "Creature/CreatureDesires.h"
#include "Creature/CreatureDevelopment.h"
#include "Creature/CreatureEmergencies.h"
#include "Creature/CreatureFace.h"
#include "Creature/CreatureFeedback.h"
#include "Creature/CreatureFriendship.h"
#include "Creature/CreatureGiving.h"
#include "Creature/CreatureHelp.h"
#include "Creature/CreatureIdleMind.h"
#include "Creature/CreatureKindAgendas.h"
#include "Creature/CreatureKiss.h"
#include "Creature/CreatureLayers.h"
#include "Creature/CreatureLocomotion.h"
#include "Creature/CreatureLook.h"
#include "Creature/CreatureLostThing.h"
#include "Creature/CreatureMeals.h"
#include "Creature/CreatureMode.h"
#include "Creature/CreatureObjectActions.h"
#include "Creature/CreatureOddAgendas.h"
#include "Creature/CreaturePhysiology.h"
#include "Creature/CreaturePlanner.h"
#include "Creature/CreaturePlayerAttention.h"
#include "Creature/CreatureReach.h"
#include "Creature/CreatureRestAndHome.h"
#include "Creature/CreatureRoute.h"
#include "Creature/CreatureScriptAgendas.h"
#include "Creature/CreatureThrow.h"
#include "Creature/CreatureTownHelp.h"
#include "Creature/CreatureTurnOrder.h"
#include "CreatureMindSystem.h"
#include "CreatureMindSystemDetail.h"
#include "CreatureMindTownHelp.h"
#include "ECS/Archetypes/PotArchetype.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/CarriedByTornado.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureFight.h"
#include "ECS/Components/CreatureGestureDrawing.h"
#include "ECS/Components/CreatureHelp.h"
#include "ECS/Components/CreatureLeash.h"
#include "ECS/Components/CreatureLocomotion.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/CreatureNeeds.h"
#include "ECS/Components/CreatureObjectAction.h"
#include "ECS/Components/CreaturePartner.h"
#include "ECS/Components/HandGrab.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/LivingReaction.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/OneOffSpellSeed.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/ScriptControl.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WallHug.h"
#include "ECS/CreatureCarried.h"
#include "ECS/CreatureExplorationLand.h"
#include "ECS/CreatureHome.h"
#include "ECS/LearningMarker.h"
#include "ECS/Map.h"
#include "ECS/ObjectPhysics.h"
#include "ECS/PhysicsEntry.h"
#include "ECS/PlayerTowns.h"
#include "ECS/Reflection/BinaryArchive.h"
#include "ECS/Registry.h"
#include "ECS/ReportedCamera.h"
#include "ECS/ShownCreatureFaces.h"
#include "ECS/Systems/AnimalSystemInterface.h"
#include "ECS/Systems/CinematicDirectorSystemInterface.h"
#include "ECS/Systems/CreatureAnimationLogSystemInterface.h"
#include "ECS/Systems/CreatureAnimationSystemInterface.h"
#include "ECS/Systems/CreatureFightSystemInterface.h"
#include "ECS/Systems/CreatureHandSystemInterface.h"
#include "ECS/Systems/CreatureHelpSystemInterface.h"
#include "ECS/Systems/CreatureLocomotionSystemInterface.h"
#include "ECS/Systems/CreatureObjectActionSystemInterface.h"
#include "ECS/Systems/CreaturePhysiologySystemInterface.h"
#include "ECS/Systems/CreatureSkinSystemInterface.h"
#include "ECS/Systems/DynamicsSystemInterface.h"
#include "ECS/Systems/FireSystemInterface.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/LeashSystemInterface.h"
#include "ECS/Systems/LivingActionSystemInterface.h"
#include "ECS/Systems/MagicSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "ECS/Systems/PlaygroundTripSystemInterface.h"
#include "ECS/Systems/ReactionSystemInterface.h"
#include "ECS/Systems/ResourceStoreSystemInterface.h"
#include "ECS/Systems/SkyClockSystemInterface.h"
#include "ECS/Systems/TeleportSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "ECS/WorldObjects.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/MagicWorldInterface.h"
#include "MagicLiving.h"
#include "ObjectMeasures.h"
#include "Physics/Body.h"
#include "Physics/LivingRules.h"
#include "VillagerFire.h"

using namespace openblack;
using namespace openblack::ecs::systems;
using namespace openblack::ecs::components;

using creature_desires::Desire;

namespace openblack::ecs::systems::mind_detail
{
constexpr float k_TurnSeconds = std::chrono::duration<float>(TimeSystemInterface::k_TurnDuration).count();
constexpr float k_TurnsPerSecond = 1.0f / k_TurnSeconds;

glm::vec3 LookAbout(const Creature& creature, const Transform& transform, const CreatureMindState& mind)
{
	if (creature_look::Watches(mind.look))
	{
		return mind.look.point;
	}
	return creature_look::PointAhead({
	    .position = transform.position,
	    .ahead = -(transform.rotation * glm::vec3(0.0f, 0.0f, 1.0f)),
	    .size = ShownSize(creature),
	});
}

/// The creature info values for the look of a creature that walks to a place
creature_look::WhileGoingInfo WhileGoingInfoOf(CreatureType species)
{
	if (!Locator::infoConstants::has_value())
	{
		return {};
	}
	const auto& creatures = Locator::infoConstants::value().creature;
	const auto row = creature::InfoRow(species);
	if (row >= creatures.size())
	{
		return {};
	}
	const auto& info = creatures.at(row);
	return {
	    .sweepHalfAngle = info.lookSweepHalfAngle,
	    .sweepAngleStep = info.lookSweepAngleStep,
	    .sweepNearest = info.lookSweepNearest,
	    .sweepFurthest = info.lookSweepFurthest,
	    .sweepDistanceStep = info.lookSweepDistanceStep,
	    .placeRange = info.lookAtPlaceRange,
	};
}

std::optional<glm::vec3> LookWhileGoing(entt::entity entity, const Creature& creature, const Transform& transform,
                                        CreatureMindState& mind, const glm::vec3& place)
{
	const auto info = WhileGoingInfoOf(creature.species);
	if (!mind.goingLook.has_value())
	{
		mind.goingLook = creature_look::StartWhileGoing(info);
	}
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* walk = registry.TryGet<const CreatureLocomotion>(entity);
	const auto ahead = -(transform.rotation * glm::vec3(0.0f, 0.0f, 1.0f));
	// The direction the body faces, as the game keeps it: a quarter turn on from the map's
	const auto heading =
	    walk != nullptr && walk->started ? walk->heading.Radians() : GameHeading::Of(ahead.x, ahead.z).Radians();
	const auto groundAt = [](glm::vec2 at) {
		return Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt(at) : 0.0f;
	};
	// The creature keeps the height of the mover. The ground below it comes from the land now.
	std::optional<glm::vec3> mover;
	if (mind.noticedMover.has_value())
	{
		const auto& noticed = *mind.noticedMover;
		mover = glm::vec3(noticed.at.x, groundAt(noticed.at) + noticed.height, noticed.at.y);
	}
	const auto point = creature_look::LookWhileGoing(
	    *mind.goingLook, info,
	    {
	        .position = glm::vec2(transform.position.x, transform.position.z),
	        .heading = heading,
	        .place = glm::vec2(place.x, place.z),
	        .watched = creature_look::Watches(mind.look) ? std::optional(mind.look.point) : std::nullopt,
	        .mover = mover,
	        .moverTurn = mind.noticedMover.has_value() ? mind.noticedMover->turn : 0,
	        .turn = mind.turn,
	        .turnsPerSecond = k_TurnsPerSecond,
	    });
	if (!point.has_value())
	{
		return std::nullopt;
	}
	return glm::vec3(point->at.x, point->height.value_or(groundAt(point->at)), point->at.y);
}

/// Where the creature's head looks for a step that gives it something to look at: the top of a thing, or its foot, a
/// point, or the camera. It doesn't look at a thing it holds, nor at one it is walking past within ten of it.
/// Where the hand nearest a creature is, of the player whose hand the creature reads (see ReportedCreaturePlayer). None
/// for a creature without a player, or when there is no hand.
std::optional<glm::vec3> NearestHand(const ecs::Registry& registry, entt::entity creature)
{
	const auto* body = registry.TryGet<const Creature>(creature);
	const auto* transform = registry.TryGet<const Transform>(creature);
	const auto player = openblack::ecs::ReportedCreaturePlayer(creature);
	if (body == nullptr || body->owner == PlayerNames::NEUTRAL || transform == nullptr || !player.has_value())
	{
		return std::nullopt;
	}
	return openblack::ecs::NearestHandOf(*player, transform->position);
}

float RunLookBackSeconds(CreatureType species)
{
	if (!Locator::infoConstants::has_value())
	{
		return 0.0f;
	}
	const auto& creatures = Locator::infoConstants::value().creature;
	const auto row = creature::InfoRow(species);
	return row < creatures.size() ? creatures.at(row).runLookBackSeconds : 0.0f;
}
float RunLookAheadSeconds(CreatureType species)
{
	if (!Locator::infoConstants::has_value())
	{
		return 0.0f;
	}
	const auto& creatures = Locator::infoConstants::value().creature;
	const auto row = creature::InfoRow(species);
	return row < creatures.size() ? creatures.at(row).runLookAheadSeconds : 0.0f;
}

std::optional<glm::vec3> GazePoint(const ecs::Registry& registry, entt::entity creature, const creature_mind::Gaze& gaze,
                                   bool moving)
{
	constexpr float k_NoLookWhileMovingWithin = 10.0f;
	if (gaze.camera)
	{
		const auto camera = openblack::ecs::ReportedCameraOfItsPlayer(creature);
		return camera.has_value() ? std::optional(camera->origin) : std::nullopt;
	}
	if (gaze.hand)
	{
		return NearestHand(registry, creature);
	}
	if (gaze.player)
	{
		// The hand when it moved much lately, else the camera
		const auto* mind = registry.TryGet<const CreatureMindState>(creature);
		if (mind != nullptr && mind->playerInterest.handMovement > creature_player_attention::k_WatchHandMovement)
		{
			return NearestHand(registry, creature);
		}
		const auto camera = openblack::ecs::ReportedCameraOfItsPlayer(creature);
		return camera.has_value() ? std::optional(camera->origin) : std::nullopt;
	}
	if (gaze.feet)
	{
		// The ground a few metres ahead of where the body faces
		const auto* self = registry.TryGet<const Transform>(creature);
		if (self == nullptr)
		{
			return std::nullopt;
		}
		const auto ahead = -(self->rotation * glm::vec3(0.0f, 0.0f, 1.0f));
		const auto flat = glm::vec2(ahead.x, ahead.z);
		const auto at = glm::vec2(self->position.x, self->position.z) +
		                (glm::length(flat) > 0.0f ? glm::normalize(flat) : glm::vec2(0.0f)) * creature_mind::k_FeetLookMetres;
		const auto ground =
		    Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt(at) : self->position.y;
		return glm::vec3 {at.x, ground, at.y};
	}
	if (!gaze.object.has_value() && gaze.point.has_value() && gaze.pointOnGround)
	{
		// The ground below the point. A creature that moves within a few metres of the point does not look at it.
		const glm::vec2 flat {gaze.point->x, gaze.point->z};
		const auto* self = registry.TryGet<const Transform>(creature);
		if (self == nullptr || (moving && gutils::GetDistanceInMetres(glm::vec2(self->position.x, self->position.z), flat) <
		                                      k_NoLookWhileMovingWithin))
		{
			return std::nullopt;
		}
		const auto ground =
		    Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt(flat) : gaze.point->y;
		return glm::vec3 {flat.x, ground, flat.y};
	}
	if (!gaze.object.has_value())
	{
		return gaze.point;
	}
	const auto thing = static_cast<entt::entity>(*gaze.object);
	const auto at = ecs::systems::object_measures::PositionOf(registry, thing);
	const auto* self = registry.TryGet<const Transform>(creature);
	if (!at.has_value() || self == nullptr || ecs::creature_carried::Carries(registry, creature, thing) ||
	    (moving && gutils::GetDistanceInMetres(glm::vec2(self->position.x, self->position.z), glm::vec2(at->x, at->z)) <
	                   k_NoLookWhileMovingWithin))
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

std::optional<glm::vec3> DependentLookPoint(const ecs::Registry& registry, entt::entity person)
{
	const auto at = registry.Valid(person) ? ecs::systems::object_measures::PositionOf(registry, person) : std::nullopt;
	if (!at.has_value())
	{
		return std::nullopt;
	}
	const glm::vec2 flat {at->x, at->z};
	const auto ground = Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt(flat) : at->y;
	return glm::vec3 {flat.x, ground + ecs::systems::object_measures::Height(registry, person), flat.y};
}

glm::vec3 FightLook(const ecs::Registry& registry, entt::entity entity, const Creature& creature, const Transform& transform,
                    const CreatureMindState& mind)
{
	using Stage = CreatureFighting::Stage;
	if (const auto* fighting = registry.TryGet<const CreatureFighting>(entity);
	    fighting != nullptr && registry.Valid(fighting->opponent) && IsLeadIn(fighting->stage) &&
	    fighting->stage != Stage::Taunt && fighting->stage != Stage::Point)
	{
		if (const auto top = GazePoint(registry, entity, {.object = entt::to_integral(fighting->opponent)}, /*moving=*/false);
		    top.has_value())
		{
			return *top;
		}
	}
	return LookAbout(creature, transform, mind);
}

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
	return ecs::InMapCells(registry, object) && !registry.AnyOf<InHand, HeldByCreature, InPhysics, CarriedByTornado>(object);
}

bool WaitedForMoving(const ecs::Registry& registry, const creature_mind::IdleMind& idle)
{
	if (idle.step >= idle.agenda.size() || idle.agenda[idle.step].kind != creature_mind::Step::Kind::WaitForStill ||
	    !idle.agenda[idle.step].object.has_value())
	{
		return false;
	}
	const auto object = static_cast<entt::entity>(*idle.agenda[idle.step].object);
	return registry.Valid(object) && registry.AllOf<Creature>(object) && Locator::creatureLocomotionSystem::has_value() &&
	       Locator::creatureLocomotionSystem::value().IsMoving(object);
}

bool WaitedForBusy(const ecs::Registry& registry, const creature_mind::IdleMind& idle)
{
	if (idle.step >= idle.agenda.size() || idle.agenda[idle.step].kind != creature_mind::Step::Kind::WaitForFree ||
	    !idle.agenda[idle.step].object.has_value())
	{
		return false;
	}
	const auto object = static_cast<entt::entity>(*idle.agenda[idle.step].object);
	if (!registry.Valid(object) || !registry.AllOf<Creature>(object))
	{
		return false;
	}
	const auto* body = registry.TryGet<const CreatureAnimation>(object);
	const bool playing = body != nullptr && creature_layers::IsPlaying(body->body);
	return playing ||
	       (Locator::creatureLocomotionSystem::has_value() && Locator::creatureLocomotionSystem::value().IsMoving(object));
}

std::optional<creature_mind::Senses::FollowedView> FollowedViewOf(const ecs::Registry& registry, const glm::vec3& from,
                                                                  const CreatureMindState& mind)
{
	const auto& idle = mind.idle;
	if (idle.step >= idle.agenda.size() || idle.agenda[idle.step].kind != creature_mind::Step::Kind::FollowWatching ||
	    !idle.agenda[idle.step].object.has_value())
	{
		return std::nullopt;
	}
	const auto villager = static_cast<entt::entity>(*idle.agenda[idle.step].object);
	if (!registry.Valid(villager) || !registry.AllOf<Transform>(villager))
	{
		return std::nullopt;
	}
	const auto at = glm::xz(registry.Get<const Transform>(villager).position);
	// The table distance between the two map positions
	creature_mind::Senses::FollowedView view {.distance = gutils::GetDistanceInMetres(at, glm::xz(from))};
	// Interesting while its state now teaches a skill the creature does not know
	const auto* action = registry.TryGet<const LivingAction>(villager);
	if (action != nullptr && mind.learnt.has_value() && Locator::infoConstants::has_value())
	{
		const auto state = action->states.at(static_cast<size_t>(LivingAction::Index::Top));
		const auto& table = Locator::infoConstants::value().villagerStateTable;
		view.interesting = state < table.size() && creature_mind::DoingSomethingInteresting(table.at(state).teachesSkill,
		                                                                                    mind.learnt->knowledge.skillsKnown);
	}
	return view;
}

bool StillObjectFlies(const ecs::Registry& registry, const creature_mind::IdleMind& idle)
{
	if (idle.step >= idle.agenda.size() || idle.agenda[idle.step].kind != creature_mind::Step::Kind::RenderImmobile ||
	    !idle.agenda[idle.step].object.has_value())
	{
		return false;
	}
	const auto object = static_cast<entt::entity>(*idle.agenda[idle.step].object);
	if (!registry.Valid(object))
	{
		return false;
	}
	if (const auto* action = registry.TryGet<const LivingAction>(object); action != nullptr && registry.AllOf<Villager>(object))
	{
		return action->states[static_cast<size_t>(LivingAction::Index::Top)] == static_cast<uint8_t>(VillagerStates::Flying);
	}
	const auto* animal = registry.TryGet<const Animal>(object);
	return animal != nullptr && animal->state == AnimalState::Flying;
}

void MakeStill(const ecs::Registry& registry, entt::entity thing)
{
	if (!registry.Valid(thing))
	{
		return;
	}
	if (registry.AllOf<Villager>(thing))
	{
		villager_fire::StartDyingWithoutDeath(thing);
	}
	else if (registry.AllOf<Animal>(thing) && Locator::animalSystem::has_value())
	{
		Locator::animalSystem::value().StartDyingWithoutDeath(thing);
	}
}

bool WatchedIsInTheAir(const ecs::Registry& registry, const creature_mind::IdleMind& idle)
{
	if (idle.step >= idle.agenda.size() || idle.agenda[idle.step].kind != creature_mind::Step::Kind::WatchFlying ||
	    !idle.agenda[idle.step].object.has_value() || !Locator::dynamicsSystem::has_value())
	{
		return false;
	}
	const auto object = static_cast<entt::entity>(*idle.agenda[idle.step].object);
	if (!registry.Valid(object) || !registry.AllOf<InPhysics>(object))
	{
		return false;
	}
	const auto* entry = Locator::dynamicsSystem::value().Find(object);
	if (entry == nullptr || entry->body == nullptr)
	{
		return false;
	}
	const auto centre = entry->body->Centre();
	const float land =
	    Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt(glm::vec2(centre.x, centre.z)) : 0.0f;
	return physics::living::IsActuallyInTheAir(entry->body->SpeedNow(), centre.y, land, entry->body->Radius());
}

/// The game turns that the hand stays in one place, of the player whose camera the creature reads: its own player
uint32_t HandStillTurnsOf(PlayerNames player)
{
	return openblack::ecs::PlayerHandStillTurnsOf(player);
}

std::optional<creature_mind::Senses::CameraView> CameraViewOf(const ecs::Registry& registry, entt::entity creature,
                                                              const creature_mind::IdleMind& idle)
{
	const auto camera = openblack::ecs::ReportedCameraOfItsPlayer(creature);
	if (idle.step >= idle.agenda.size() || idle.agenda[idle.step].kind != creature_mind::Step::Kind::WatchCamera ||
	    !camera.has_value())
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
	const auto eye = camera->origin;
	const glm::vec2 point {eye.x, eye.z};
	const auto offset = point - glm::vec2(transform->position.x, transform->position.z);
	// How far away the camera is: the game's table distance along the ground
	return creature_mind::Senses::CameraView {
	    .distance = gutils::GetDistance(transform->position, eye),
	    .groundDistance = glm::length(offset),
	    .point = point,
	    .turn = creature_locomotion::HeadingOf(offset).TurnFrom(self->heading),
	    .handStillTurns = HandStillTurnsOf(body->owner),
	};
}

std::pair<std::optional<glm::vec3>, bool> CameraFocusOf(const ecs::Registry& registry, entt::entity creature,
                                                        const creature_mind::IdleMind& idle)
{
	const auto camera = openblack::ecs::ReportedCameraOfItsPlayer(creature);
	if (idle.step >= idle.agenda.size() || idle.agenda[idle.step].kind != creature_mind::Step::Kind::FollowCamera ||
	    !camera.has_value())
	{
		return {std::nullopt, false};
	}
	const auto* body = registry.TryGet<const Creature>(creature);
	const auto* transform = registry.TryGet<const Transform>(creature);
	if (body == nullptr || body->owner == PlayerNames::NEUTRAL || transform == nullptr)
	{
		return {std::nullopt, false};
	}
	const auto focus = camera->focus;
	const auto from = map_coords::FromMetres(glm::vec2(transform->position.x, transform->position.z));
	const auto to = map_coords::FromMetres(glm::vec2(focus.x, focus.z));
	// The way the body faces: its heading, or the way its rotation faces before it walks
	const auto* walk = registry.TryGet<const CreatureLocomotion>(creature);
	const auto ahead = -(transform->rotation * glm::vec3(0.0f, 0.0f, 1.0f));
	const auto heading = walk != nullptr && walk->started ? walk->heading : GameHeading::Of(ahead.x, ahead.z);
	const auto facing = static_cast<int32_t>(gutils::ConvertScawenAngleToGameAngle(heading.Radians()));
	// The game takes the difference of the two angles with no wrap at a full turn
	const auto apart = std::abs(facing - static_cast<int32_t>(gutils::GetAngleFromXZ(from, to)));
	const bool inView =
	    apart < gutils::k_GameAngleCircle / creature_look::k_BodyViewParts || map_coords::Cell(from) == map_coords::Cell(to);
	return {focus, inView};
}

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
	    .turn = creature_locomotion::HeadingOf(offset).TurnFrom(self->heading),
	};
}

std::optional<creature_player_attention::FollowHandView> FollowHandViewOf(const ecs::Registry& registry, entt::entity creature,
                                                                          const CreatureMindState& mind, bool bodyPlaying)
{
	const auto& idle = mind.idle;
	if (idle.step >= idle.agenda.size() || idle.agenda[idle.step].kind != creature_mind::Step::Kind::FollowHand)
	{
		return std::nullopt;
	}
	const auto* body = registry.TryGet<const Creature>(creature);
	const auto* transform = registry.TryGet<const Transform>(creature);
	const auto* self = registry.TryGet<const CreatureLocomotion>(creature);
	auto hand = NearestHand(registry, creature);
	if (!hand.has_value())
	{
		hand = mind.playerInterest.lastHand;
	}
	creature_player_attention::FollowHandView view;
	if (body == nullptr || body->owner == PlayerNames::NEUTRAL || !hand.has_value() || transform == nullptr || self == nullptr)
	{
		return view;
	}
	view.hasPlayer = true;
	const map_coords::MapCoords handAt {.x = map_coords::ToFixed(hand->x), .z = map_coords::ToFixed(hand->z)};
	const map_coords::MapCoords at {.x = map_coords::ToFixed(transform->position.x),
	                                .z = map_coords::ToFixed(transform->position.z)};
	view.handInMap = map_coords::InBounds(handAt);
	view.hand = {hand->x, hand->z};
	view.distance = gutils::GetDistanceInMetres(at, handAt);
	view.reach = creature_player_attention::FollowHandReach(object_measures::TwoDRadius(registry, creature));
	view.bodyPlaying = bodyPlaying;
	view.directionClose = creature_player_attention::IsDirectionClose(at, handAt, self->heading.GameAngle());
	return view;
}

std::optional<creature_player_attention::WatchPlayerView>
WatchPlayerViewOf(const ecs::Registry& registry, entt::entity creature, const CreatureMindState& mind, bool bodyPlaying)
{
	const auto& idle = mind.idle;
	if (idle.step >= idle.agenda.size() || idle.agenda[idle.step].kind != creature_mind::Step::Kind::WatchPlayer)
	{
		return std::nullopt;
	}
	const auto* body = registry.TryGet<const Creature>(creature);
	const auto* transform = registry.TryGet<const Transform>(creature);
	const auto* self = registry.TryGet<const CreatureLocomotion>(creature);
	creature_player_attention::WatchPlayerView view {
	    // The game has a wait for orders of fifteen seconds, but no input of the player starts it. Thus the creature never
	    // waits, and the watch ends at once, as in the game.
	    .waitingForOrders = false,
	    .hasPlayer = body != nullptr && body->owner != PlayerNames::NEUTRAL,
	    .bodyPlaying = bodyPlaying,
	    .handMovement = mind.playerInterest.handMovement,
	};
	if (transform == nullptr || self == nullptr)
	{
		return view;
	}
	const auto targetOf = [transform, self](glm::vec3 point) {
		const auto offset = glm::vec2(point.x, point.z) - glm::vec2(transform->position.x, transform->position.z);
		// The distance is the length along the ground
		return creature_player_attention::WatchPlayerView::Target {
		    .point = {point.x, point.z},
		    .distance = glm::length(offset),
		    .turn = creature_locomotion::HeadingOf(offset).TurnFrom(self->heading),
		};
	};
	if (const auto hand = NearestHand(registry, creature); hand.has_value())
	{
		view.hand = targetOf(*hand);
	}
	if (const auto camera = openblack::ecs::ReportedCameraOfItsPlayer(creature))
	{
		view.camera = targetOf(camera->origin);
	}
	return view;
}
} // namespace openblack::ecs::systems::mind_detail

using namespace openblack::ecs::systems::mind_detail;
