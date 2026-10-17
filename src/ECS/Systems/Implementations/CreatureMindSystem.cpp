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
#include "Creature/CreatureAttention.h"
#include "Creature/CreatureDesireSources.h"
#include "Creature/CreatureDesires.h"
#include "Creature/CreatureEmergencies.h"
#include "Creature/CreatureFace.h"
#include "Creature/CreatureFeedback.h"
#include "Creature/CreatureFriendship.h"
#include "Creature/CreatureGiving.h"
#include "Creature/CreatureHelp.h"
#include "Creature/CreatureIdleMind.h"
#include "Creature/CreatureKindAgendas.h"
#include "Creature/CreatureLayers.h"
#include "Creature/CreatureLocomotion.h"
#include "Creature/CreatureLook.h"
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
#include "ECS/Components/CreatureLocomotion.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/CreatureNeeds.h"
#include "ECS/Components/CreatureObjectAction.h"
#include "ECS/Components/CreaturePartner.h"
#include "ECS/Components/HandGrab.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/LivingReaction.h"
#include "ECS/Components/MapCellResident.h"
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
#include "ECS/CreatureExplorationLand.h"
#include "ECS/CreatureHome.h"
#include "ECS/Map.h"
#include "ECS/ObjectPhysics.h"
#include "ECS/PhysicsEntry.h"
#include "ECS/Registry.h"
#include "ECS/Systems/AnimalSystemInterface.h"
#include "ECS/Systems/CinematicDirectorSystemInterface.h"
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
#include "ECS/Systems/LivingActionSystemInterface.h"
#include "ECS/Systems/MagicSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "ECS/Systems/ReactionSystemInterface.h"
#include "ECS/Systems/ResourceStoreSystemInterface.h"
#include "ECS/Systems/SkySystemInterface.h"
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

/// The action of the hands that an order of the mind starts, for the orders that walk to a thing
openblack::creature_object_actions::Kind HandsKindOf(openblack::creature_mind::ObjectOrder::Kind order)
{
	using Order = openblack::creature_mind::ObjectOrder::Kind;
	using openblack::creature_object_actions::Kind;
	switch (order)
	{
	case Order::PickUp:
		return Kind::PickUp;
	case Order::Destroy:
		return Kind::Destroy;
	case Order::Kick:
		return Kind::Kick;
	default:
		return Kind::Keep;
	}
}
} // namespace
using creature_desires::Desire;

namespace openblack::ecs::systems::mind_detail
{
constexpr float k_TurnSeconds = std::chrono::duration<float>(TimeSystemInterface::k_TurnDuration).count();
constexpr float k_TurnsPerSecond = 1.0f / k_TurnSeconds;
/// The whole game turns in a second, as the game counts them
constexpr auto k_WholeTurnsPerSecond = static_cast<uint32_t>(1000 / TimeSystemInterface::k_TurnDuration.count());
/// A creature just put in the world takes its first few turns to stand up before it can pass out
constexpr uint32_t k_TurnsBeforePassingOut = 2;
/// A face told to by hand is held this long
constexpr float k_ToldFaceMs = 3000.0f;

/// A creature looks for food and water this far away at most
constexpr float k_FoodSearchDistance = 200.0f;
/// Food further than this from its mouth, for its size, has got away by the time it eats
constexpr float k_EatReach = 15.0f;
/// Having drunk, it does not want water for a while
constexpr float k_WaterSuppressSeconds = 20.0f;
/// The rows of the game's creature action table that seeing to each need counts as
constexpr std::string_view k_EatAction = "EatAfterExamining";
constexpr std::string_view k_DrinkAction = "DrinkFromTheSea";
constexpr std::string_view k_PooAction = "Poo";
constexpr std::string_view k_FartAction = "Fart";
constexpr std::string_view k_PukeAction = "Puke";
constexpr std::string_view k_SleepAction = "SleepOnTheSpot";
constexpr std::string_view k_ExamineAction = "ExamineByPickingUp";
constexpr std::string_view k_ThrowAboutAction = "PracticeThrow";
constexpr std::string_view k_HurlAction = "Hurl";
constexpr std::string_view k_FightAction = "Fight";
/// The action of a creature that obeys another creature, as the other creature in a fight does
constexpr std::string_view k_ObeyCreatureAction = "ObeyCreature";
/// The action a creature made to catch something carries out
constexpr std::string_view k_CatchAction = "Catch";
/// The action a creature given something from the hand carries out to take it
constexpr std::string_view k_TakeFromHandAction = "TakeObjectFromHand";
/// What a creature does with what it was given, by the game's action table
constexpr std::string_view k_ExamineGivenAction = "ExamineObjectInHand";
constexpr std::string_view k_EatGivenAction = "EatObjectInHand";
constexpr std::string_view k_ThrowGivenAction = "ThrowObjectInHand";
/// The desire to show how it is grows with all its desires added up, fully at this much
constexpr float k_ManifestSum = 3.0f;

/// Just woken up, the eyelids droop and blink slowly; otherwise they are as they start
constexpr float k_SleepyOpenness = -0.7f;
constexpr int32_t k_SleepyBlinkIntervalMs = 2500;

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
			const auto& table = info.desireSourceTable.at(type);
			desire.sources.push_back({
			    .type = type,
			    .value = PerSpecies(info.creatureInitialSource1.at(type), row),
			    .threshold = PerSpecies(info.creatureInitialSource2.at(type), row),
			    .multiplier = table.field0x8,
			    .clearedWhenSatisfied = table.field0x4 != 0,
			});
			desire.spreads.push_back({.value = table.field0xc, .threshold = table.field0x10});
		}
	}
	return setup;
}

/// The desires of the body, from the game's table of desires
std::array<bool, creature_desires::k_DesireCount> BodyDesires()
{
	std::array<bool, creature_desires::k_DesireCount> body {};
	if (!Locator::infoConstants::has_value())
	{
		return body;
	}
	const auto& info = Locator::infoConstants::value();
	for (size_t d = 0; d < body.size(); ++d)
	{
		body.at(d) = info.creatureInitialDesire.at(d).field0x34 != 0;
	}
	return body;
}

creature_desire_sources::InnateTraits InnateTraitsOf(CreatureType species)
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
	const auto& traits = info.innateTraits;
	return {
	    .kindness = traits.at(0),
	    .aggression = traits.at(1),
	    .lethargy = traits.at(2),
	    .friendliness = traits.at(3),
	    .communicativeness = traits.at(4),
	    .sixth = traits.at(5),
	    .objectsLookWeight = info.lookAtObjectsWeight,
	    .mountainsLookWeight = info.lookAtMountainsWeight,
	    .waterLookWeight = info.lookAtWaterWeight,
	};
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

/// How interesting the creature finds its player this turn. The local player's hand is the player's; a creature of
/// another player, or of none, finds nothing of interest here.
float PlayerInterestNow(const Creature& creature, CreatureMindState& mind)
{
	if (!Locator::handSystem::has_value() || !Locator::playerSystem::has_value() ||
	    creature.owner != Locator::playerSystem::value().GetLocalPlayer())
	{
		return creature_desire_sources::InterestNow(mind.playerInterest, std::nullopt, std::nullopt);
	}
	const auto& registry = Locator::entitiesRegistry::value();
	constexpr auto k_Hand = static_cast<size_t>(HandSystemInterface::Side::Left);
	const auto handEntity = Locator::handSystem::value().GetPlayerHands().at(k_Hand);
	// Off the land the hand has no place: it counts as where it last was
	auto hand = Locator::handSystem::value().GetPlayerHandPositions().at(k_Hand);
	if (!hand.has_value())
	{
		hand = mind.playerInterest.lastHand;
	}
	std::optional<float> heldRadius;
	if (const auto* grab = registry.Valid(handEntity) ? registry.TryGet<const HandGrab>(handEntity) : nullptr;
	    grab != nullptr && registry.Valid(grab->object) &&
	    (grab->state == HandGrab::State::Holding || grab->state == HandGrab::State::ReadyToThrow))
	{
		heldRadius = object_measures::TwoDRadius(registry, grab->object);
	}
	if (!hand.has_value())
	{
		return 0.0f;
	}
	return creature_desire_sources::InterestNow(mind.playerInterest, hand, heldRadius);
}

/// What scripts set the creature's sources to since its last turn: the trait or count behind each source follows
void TakeWrittenSources(entt::entity entity, CreatureMindState& mind)
{
	if (!mind.desires.has_value() || mind.desires->written.empty())
	{
		return;
	}
	auto* body = Locator::entitiesRegistry::value().TryGet<Creature>(entity);
	const creature_desire_sources::Written written {
	    .innate = &mind.innate,
	    .attention = &mind.attention,
	    .interest = &mind.playerInterest,
	    .mushroomsEaten = body != nullptr ? &body->magicMushroomsEaten : nullptr,
	    .turnsPerSecond = k_WholeTurnsPerSecond,
	};
	for (const auto& [type, value] : std::exchange(mind.desires->written, {}))
	{
		creature_desire_sources::Write(type, value, written);
	}
}

/// What the sources that follow the creature's personality, attention, home and meals read
creature_desire_sources::State SourceStateOf(entt::entity entity, const Creature& creature, const CreatureMindState& mind,
                                             uint32_t age)
{
	const auto& registry = Locator::entitiesRegistry::value();
	creature_desire_sources::State state {
	    .innate = &mind.innate,
	    .attention = &mind.attention,
	    .interest = mind.playerInterest.value,
	    .turnsPerSecond = k_WholeTurnsPerSecond,
	    .computerPlayer = Locator::playerSystem::has_value() && Locator::playerSystem::value().IsComputerPlayer(creature.owner),
	    .age = age,
	    .mushroomsEaten = creature.magicMushroomsEaten,
	    .impressFromSeeing = mind.impressFromSeeing,
	};
	const auto* transform = registry.TryGet<const Transform>(entity);
	// TODO(creature-hang-around): the game keeps the place where a creature without a temple was made as its home;
	// without a home here, the sources of going home keep their own values
	if (const auto home = ecs::creature_home::HomeOf(registry, entity); home.has_value() && transform != nullptr)
	{
		state.homeDistance = glm::distance(glm::xz(transform->position), glm::xz(*home));
	}
	if (Locator::infoConstants::has_value())
	{
		const auto& phases = Locator::infoConstants::value().creatureDevelopmentPhaseEntry;
		state.goHomeDistance = phases.at(std::min<size_t>(mind.developmentPhase, phases.size() - 1)).goHomeDistance;
	}
	return state;
}

/// The sources that follow the creature's state: its body's, and its mind's own
std::optional<float> ReadSource(uint32_t type, const creature_desires::Desires& desires, const CreatureMindState& mind,
                                const creature_physiology::Needs& needs, bool night,
                                const creature_desire_sources::State& state)
{
	namespace sources = creature_desires::sources;
	if (const auto value = creature_physiology::SourceValue(type, needs, night))
	{
		return value;
	}
	if (const auto value = creature_desire_sources::Read(type, state))
	{
		return value;
	}
	switch (type)
	{
	case sources::k_ManifestState:
		return Clamp01(desires.sum / k_ManifestSum);
	case sources::k_RunAwayFromPlayer:
		return Clamp01(-mind.attitudeToPlayer);
	case sources::k_AngerFromSadness:
	case sources::k_HungerFromSadness:
	case sources::k_TirednessFromSadness:
		return creature_desires::SourceValue(desires[Desire::Sadness], sources::k_Sadness).value_or(0.0f);
	case sources::k_MissedFriends:
		return mind.learnt.has_value() ? creature_friendship::MissedFriends(mind.learnt->creatures) : 0.0f;
	default:
		return std::nullopt;
	}
}

/// One turn of looking about: the head turns to what the creature watches, or ahead
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

/// The look of a creature that walks to a place in this turn, or none near the place
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
	const auto heading = walk != nullptr && walk->started ? walk->heading : std::atan2(ahead.x, -ahead.z);
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

void ApplyEyes(CreatureEyes* eyes, creature_mind::Eyes look)
{
	if (eyes == nullptr || look == creature_mind::Eyes::Unchanged)
	{
		return;
	}
	// Just woken up, only the lids and the blinking change
	if (look == creature_mind::Eyes::Sleepy)
	{
		eyes->openness = k_SleepyOpenness;
		eyes->blink.intervalMs = k_SleepyBlinkIntervalMs;
		return;
	}
	// The other looks change only how the eyes look: the lids and the blinking stay as they were
	eyes->mode = look == creature_mind::Eyes::Closed   ? creature_eyes::Mode::Closed
	             : look == creature_mind::Eyes::Stoned ? creature_eyes::Mode::Stoned
	             : look == creature_mind::Eyes::Wide   ? creature_eyes::Mode::Wide
	                                                   : creature_eyes::Mode::Calm;
}

/// The creature stops what it does: its eyes go calm, blink as usual, and its lids open by how rested it is
void RestEyes(const ecs::Registry& registry, entt::entity creature, CreatureEyes* eyes)
{
	if (eyes == nullptr)
	{
		return;
	}
	const auto* needs = registry.TryGet<const CreatureNeeds>(creature);
	eyes->mode = creature_eyes::Mode::Calm;
	eyes->blink.intervalMs = creature_eyes::k_BlinkIntervalMs;
	eyes->openness = creature_eyes::RestedOpenness(needs != nullptr ? needs->needs.exhaustion : 0.0f);
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

/// A new agenda means that the creature stopped what it did. The creature of a player that is not the computer then
/// goes back to its walk speed. Other creatures keep the speed that they got last.
void KeepPaceWithAgenda(entt::entity creature, CreatureMindState& mind)
{
	if (mind.paceSerial == mind.idle.serial)
	{
		return;
	}
	mind.paceSerial = mind.idle.serial;
	const auto* body = Locator::entitiesRegistry::value().TryGet<const Creature>(creature);
	if (body == nullptr || !Locator::creatureLocomotionSystem::has_value())
	{
		return;
	}
	const bool computer = Locator::playerSystem::has_value() && Locator::playerSystem::value().IsComputerPlayer(body->owner);
	if (!creature_emergencies::KeepsRunSpeed(body->owner != PlayerNames::NEUTRAL, computer))
	{
		Locator::creatureLocomotionSystem::value().ResetPace(creature);
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
	// A walk that does not ask for the run speed keeps the speed that the creature got last
	const auto pace = move.run ? Pace::Run : Pace::Keep;
	const auto object = move.object.has_value() ? std::optional(static_cast<entt::entity>(*move.object)) : std::nullopt;
	switch (move.kind)
	{
	case Kind::ToPoint:
	case Kind::Nearby:
	{
		const auto result = move.extraSpeed.has_value()
		                        ? locomotion.LeadTo(creature, move.point, *move.extraSpeed, move.maxDistance)
		                        : locomotion.MoveTo(creature, move.point, pace, move.minDistance, move.maxDistance);
		if (result == CreatureLocomotionSystemInterface::MoveResult::InvalidDestination && move.giveUpIfUnreachable)
		{
			return true;
		}
		break;
	}
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
	case Kind::TurnToFaceAwayFromObject:
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
	       kind == Kind::TurnToFaceAwayFromObject || kind == Kind::ToThrowPosition;
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

/// Running away, the seconds that a creature of a species looks back at what it runs from, and the seconds that it then
/// looks about
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
		return Locator::camera::has_value() ? std::optional(Locator::camera::value().GetOrigin()) : std::nullopt;
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
		return Locator::camera::has_value() ? std::optional(Locator::camera::value().GetOrigin()) : std::nullopt;
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
		if (self == nullptr ||
		    (moving && glm::distance(glm::vec2(self->position.x, self->position.z), flat) < k_NoLookWhileMovingWithin))
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

/// The point that the head of a creature in a fight or knocked out looks at, each turn. Before the duel, it looks at the
/// top of its opponent, but not while it taunts or points at the arena. Then, in the duel and after it, it looks at what
/// it watches, or ahead. In the duel, the fight stance turns the head to the head of the opponent, and the moves keep the
/// head straight.
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

/// The creature heals itself: its life becomes full, and its wounds and blood age as if much time went by
void HealItself(ecs::Registry& registry, entt::entity creature)
{
	if (auto* needs = registry.TryGet<CreatureNeeds>(creature))
	{
		needs->needs.life = 1.0f;
	}
	if (Locator::creatureSkinSystem::has_value())
	{
		Locator::creatureSkinSystem::value().Heal(creature, creature_mind::k_HealItselfCounts);
	}
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

/// Whether the thing a step watches fly is still really in the air: in the physics, and moving fast or clear of the land
/// The villager a step follows to watch, as the creature sees it, while that step is next or under way
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
	creature_mind::Senses::FollowedView view {.distance = glm::distance(at, glm::xz(from))};
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

/// Whether the living object of the step that makes a thing still is in its state of flying
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

/// A living thing starts to die at once, without the work of a death: a villager or an animal
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
	return physics::living::IsActuallyInTheAir(entry->body->Speed(), centre.y, land, entry->body->Radius());
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
	    .handStillTurns = Locator::handSystem::has_value() ? Locator::handSystem::value().GetHandStillTurns() : 0,
	};
}

/// The point of focus of the camera of a creature's player, on the land, while its step follows the camera; and whether
/// the point is less than a sixth of a turn from the way its body faces, or in its own map cell. None for a creature
/// without a player.
std::pair<std::optional<glm::vec3>, bool> CameraFocusOf(const ecs::Registry& registry, entt::entity creature,
                                                        const creature_mind::IdleMind& idle)
{
	if (idle.step >= idle.agenda.size() || idle.agenda[idle.step].kind != creature_mind::Step::Kind::FollowCamera ||
	    !Locator::camera::has_value())
	{
		return {std::nullopt, false};
	}
	const auto* body = registry.TryGet<const Creature>(creature);
	const auto* transform = registry.TryGet<const Transform>(creature);
	if (body == nullptr || body->owner == PlayerNames::NEUTRAL || transform == nullptr)
	{
		return {std::nullopt, false};
	}
	const auto focus = Locator::camera::value().GetFocus();
	const auto from = map_coords::FromMetres(glm::vec2(transform->position.x, transform->position.z));
	const auto to = map_coords::FromMetres(glm::vec2(focus.x, focus.z));
	// The way the body faces, as the game turns it: a quarter turn on from the map's
	const auto ahead = -(transform->rotation * glm::vec3(0.0f, 0.0f, 1.0f));
	const auto facing = static_cast<int32_t>(gutils::ConvertScawenAngleToGameAngle(
	    static_cast<float>(std::atan2(static_cast<double>(ahead.x), -static_cast<double>(ahead.z)))));
	// The game takes the difference of the two angles with no wrap at a full turn
	const auto apart = std::abs(facing - static_cast<int32_t>(gutils::GetAngleFromXZ(from, to)));
	const bool inView =
	    apart < gutils::k_GameAngleCircle / creature_look::k_BodyViewParts || map_coords::Cell(from) == map_coords::Cell(to);
	return {focus, inView};
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

/// The player's hand as a creature that follows the hand sees it, only while its step follows the hand. If the hand is off
/// the land, the last position of the hand counts.
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
	view.directionClose = creature_player_attention::IsDirectionClose(
	    at, handAt, creature_player_attention::GameAngleOf(creature_locomotion::DirectionOf(self->heading)));
	return view;
}

/// The player as a creature that watches it sees it, only while its step watches the player
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
		return creature_player_attention::WatchPlayerView::Target {
		    .point = {point.x, point.z},
		    .distance = glm::distance(point, transform->position),
		    .turn = creature_locomotion::WrapAngle(creature_locomotion::HeadingOf(offset) - self->heading),
		};
	};
	if (const auto hand = NearestHand(registry, creature); hand.has_value())
	{
		view.hand = targetOf(*hand);
	}
	if (Locator::camera::has_value())
	{
		view.camera = targetOf(Locator::camera::value().GetOrigin());
	}
	return view;
}

/// Lets go of the miracle the creature holds, as it stops what it was doing
void ReleaseCast(entt::entity creature)
{
	if (Locator::magicSystem::has_value())
	{
		Locator::magicSystem::value().ReleaseCreatureCast(creature);
	}
}

/// Whether the creature's species has the clip. A body that is not loaded yet counts as one that has every clip.
bool HasClip(entt::entity creature, const CreatureAnimation& animation, size_t clip)
{
	if (!Locator::creatureAnimationSystem::has_value() || animation.skeleton.Empty())
	{
		return true;
	}
	return Locator::creatureAnimationSystem::value().AnimationDuration(creature, clip).has_value();
}

/// Whether the body does not have a clip that the next step plays as it starts. A step that started has no test.
bool ClipMissing(entt::entity creature, const CreatureAnimation& animation, const creature_mind::IdleMind& mind,
                 std::optional<size_t> communication)
{
	if (mind.stepStarted || mind.step >= mind.agenda.size())
	{
		return false;
	}
	return creature_mind::LacksClipToStart(mind.agenda[mind.step], communication,
	                                       [creature, &animation](size_t clip) { return HasClip(creature, animation, clip); });
}

void Apply(const creature_mind::Commands& commands, CreatureAnimation& animation, CreatureEyes* eyes)
{
	if (commands.endSit || commands.endStaticLoop)
	{
		animation.body = creature_layers::EndLoop(animation.body);
	}
	if (commands.endDance)
	{
		animation.body = creature_layers::EndDance(animation.body);
	}
	if (commands.startDance.has_value())
	{
		if (auto body = creature_layers::PlayDance(animation.body, *commands.startDance))
		{
			animation.body = *body;
		}
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
	if (eyes != nullptr && commands.blinkIntervalMs.has_value())
	{
		eyes->blink.intervalMs = *commands.blinkIntervalMs;
	}
}

/// What, besides why, picks the faces the creature pulls
creature_face::Feelings FeelingsOf(const CreatureMindState& mind)
{
	return {.variety = mind.idle.faceVariety, .attitudeToPlayer = mind.attitudeToPlayer, .idlePick = 0};
}

/// What the creature shows when it communicates its state now
creature_desires::Communication Communication(const CreatureMindState& mind)
{
	if (!mind.desires.has_value())
	{
		return {};
	}
	const auto turnsSinceFeedback =
	    mind.feedbackSeconds.has_value()
	        ? std::optional(static_cast<uint32_t>(std::lround(*mind.feedbackSeconds / k_TurnSeconds)))
	        : std::nullopt;
	return creature_desires::Communicate(*mind.desires, turnsSinceFeedback,
	                                     static_cast<uint32_t>(std::lround(k_TurnsPerSecond)), mind.attitudeToPlayer);
}

/// After the creature communicates its state, its desire to show its state cannot grow for a time, and the plan found
/// for that desire is forgotten
void Communicated(CreatureMindState& mind)
{
	if (mind.desires.has_value())
	{
		creature_desires::Suppress(*mind.desires, Desire::ManifestState, creature_desires::k_CommunicateSuppressSeconds,
		                           k_TurnsPerSecond);
	}
	mind.planner.best.at(static_cast<size_t>(Desire::ManifestState)).reset();
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

/// The nearest food that no creature holds, in the search distance from a point, and its position
std::optional<std::pair<entt::entity, glm::vec2>> NearestFood(ecs::Registry& registry, glm::vec2 from)
{
	std::optional<std::pair<entt::entity, glm::vec2>> nearest;
	float best = k_FoodSearchDistance;
	const auto consider = [&](entt::entity entity, const Transform& at) {
		const glm::vec2 point {at.position.x, at.position.z};
		const auto distance = glm::distance(point, from);
		if (distance <= best && !registry.AllOf<HeldByCreature>(entity) && FoodValueOf(entity).has_value())
		{
			best = distance;
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

/// What a step did to the body. A meal of a plan decreases the desire of the plan only when the plan ends, as all
/// actions of plans do.
void TakeEffect(entt::entity creature, const creature_mind::Commands& commands, creature_desires::Desires& desires,
                bool planned)
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
		// The meal decreases the hunger when the creature swallows it. A meal from no plan also decreases the desire by
		// the value in the action table.
		if (!planned)
		{
			Satisfied(creature, desires, k_EatAction);
		}
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
		// At the end of the squat, the bowels are empty. The lumps dropped during the squat stopped the desire to poo
		physiology.EmptyBowels(creature);
		Satisfied(creature, desires, k_PooAction);
		break;
	case Effect::Fart:
		// At the end of the squat, the bowels are empty. The action table sets how much the desire to poo decreases
		physiology.EmptyBowels(creature);
		Satisfied(creature, desires, k_FartAction);
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
	case Effect::Revived:
		magic_living::SetLife(creature, creature_odd::k_StrangeReviveLife);
		break;
	case Effect::RanAway:
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

/// Put a new handful for a creature into the map at once, as the game does with each new pot. A creature cannot pick
/// up a thing that is not in the map.
void PutOnTheMap(entt::entity food)
{
	if (Locator::entitiesMap::has_value())
	{
		Locator::entitiesMap::value().Refile(food);
	}
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
	PutOnTheMap(food);
	return food;
}

/// The resource a pot of a kind holds, by the game's pot table
ResourceType ResourceTypeOf(PotInfo kind)
{
	const auto row = static_cast<size_t>(kind);
	return Locator::infoConstants::has_value() && row < Locator::infoConstants::value().pot.size()
	           ? Locator::infoConstants::value().pot.at(row).resourceType
	           : ResourceType::Food;
}

/// The food that a creature takes from a thing (a pile, the food pile of a storage pit, a field or a tree). It is a
/// handful of wheat or fruit, of 1000 for each unit of the size of the creature, but not more than the store of food of
/// the thing. Its scale is 3.75 times the size of the creature, and it belongs to the nearest town. A field and a tree
/// have no store of food, thus their handful holds no food.
std::optional<entt::entity> FoodFromPile(ecs::Registry& registry, entt::entity creature, entt::entity pile,
                                         creature_mind::ObjectOrder::Handful handful)
{
	constexpr float k_ScalePerSize = 3.75f;
	const auto* transform = registry.TryGet<const Transform>(creature);
	if (transform == nullptr || !registry.Valid(pile) || !Locator::resourceStoreSystem::has_value())
	{
		return std::nullopt;
	}
	const auto* body = registry.TryGet<const Creature>(creature);
	const float size = body != nullptr ? body->size : 1.0f;
	const auto kind = creature_town_help::HandfulPot(handful);
	const auto held = Locator::resourceStoreSystem::value().GetResource(pile, ResourceTypeOf(kind));
	const auto amount = creature_meals::HandfulAmount(size, held);
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
	// Make the handful also when it holds no food, as from a field or a tree
	const auto food = ecs::archetypes::PotArchetype::CreateEmpty(at, 0.0f, kind);
	if (food == entt::null)
	{
		return std::nullopt;
	}
	auto& pot = registry.Get<Pot>(food);
	pot.amount = amount;
	pot.town = NearestTown(registry, transform->position);
	if (auto* foodTransform = registry.TryGet<Transform>(food))
	{
		foodTransform->scale = glm::vec3(size * k_ScalePerSize);
	}
	PutOnTheMap(food);
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
		// At the place of the object as the throw starts, when the order says so. The place of a thing that a creature
		// holds is the place of that creature.
		if (order.atObject && object.has_value() && registry.Valid(*object))
		{
			const auto* held = registry.TryGet<const HeldByCreature>(*object);
			const auto place = held != nullptr && registry.Valid(held->creature) ? held->creature : *object;
			if (const auto* at = registry.TryGet<const Transform>(place))
			{
				hands.Throw(creature, at->position);
				break;
			}
		}
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
	case Kind::Kick:
		if (!object.has_value() || !registry.Valid(*object) || !hands.Kick(creature, *object))
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
		if (const auto food = object.has_value() ? FoodFromPile(registry, creature, *object, order.handful) : std::nullopt;
		    food.has_value())
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

/// Remove the food of the handful from the pile that the creature took it from. Only a pile loses food: a field or a
/// tree loses nothing.
void GiveUpTakenFood(ecs::Registry& registry, CreatureMindState& mind)
{
	const auto food = std::exchange(mind.takenFood, entt::null);
	const auto pile = std::exchange(mind.takenFrom, entt::null);
	const auto* pot = registry.Valid(food) ? registry.TryGet<const Pot>(food) : nullptr;
	if (pot == nullptr || !registry.Valid(pile) || !registry.AllOf<Pot>(pile) || !Locator::resourceStoreSystem::has_value())
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
/// The creature's plan has it fight another creature: whether the other creature agreed and the fight started
bool StartPlannedFight(entt::entity creature, entt::entity opponent)
{
	using Result = CreatureFightSystemInterface::StartResult;
	return Locator::creatureFightSystem::has_value() &&
	       Locator::creatureFightSystem::value().StartPlannedFight(creature, opponent) == Result::Started;
}
} // namespace openblack::ecs::systems::mind_detail

using namespace openblack::ecs::systems::mind_detail;

void CreatureMindSystem::ProcessTurn()
{
	auto& registry = Locator::entitiesRegistry::value();
	// The feedback of a release reaches each mind before the minds think, when its wait is over
	std::vector<std::pair<entt::entity, float>> arrived;
	registry.Each<CreatureMindState>([&arrived](entt::entity entity, CreatureMindState& mind) {
		if (!mind.releaseFeedback.has_value())
		{
			return;
		}
		if (mind.releaseFeedbackTurns > 0)
		{
			--mind.releaseFeedbackTurns;
			return;
		}
		arrived.emplace_back(entity, *std::exchange(mind.releaseFeedback, std::nullopt));
	});
	for (const auto& [entity, feedback] : arrived)
	{
		ReceiveFeedback(entity, feedback);
	}
	const auto random = [this](uint32_t range) {
		return range == 0 ? 0u : std::uniform_int_distribution<uint32_t>(0, range - 1)(_random);
	};
	// The variety of faces moves on by the random numbers of the game
	const auto gameRandom = [&random](uint32_t range) {
		return Locator::gameRandom::has_value() ? Locator::gameRandom::value().GameRand(range) : random(range);
	};
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
		    // What scripts set its sources to since the last turn lasts: what each source follows takes the value
		    TakeWrittenSources(entity, mind);
		    // How interesting it finds its player follows the hand, also while the hand holds it. The speed of the hand
		    // follows the movement of the hand in the turn.
		    const auto handBefore = mind.playerInterest.lastHand;
		    creature_desire_sources::UpdateInterest(mind.playerInterest, PlayerInterestNow(creature, mind));
		    const auto handMovement = handBefore.has_value() && mind.playerInterest.lastHand.has_value()
		                                  ? *mind.playerInterest.lastHand - *handBefore
		                                  : glm::vec3(0.0f);
		    creature_player_attention::UpdateHandSpeed(mind.handSpeed, handMovement, k_TurnsPerSecond);
		    // What it thinks its player wants fades
		    creature_perceived_desires::Fade(mind.perceivedDesires);
		    if (mind.learnt.has_value())
		    {
			    creature_learning::Age(mind.learnt->contexts, k_TurnSeconds);
		    }
		    if (mind.desiresPhase != mind.developmentPhase)
		    {
			    creature_desires::ActivateForPhase(*mind.desires, Phases(), mind.developmentPhase);
			    mind.desiresPhase = mind.developmentPhase;
		    }
		    // What it believed about things now gone is forgotten
		    ForgetGoneThings(entity, mind);
		    const bool held = HeldByHand(entity);
		    // Only a creature's own player pays it attention, and not while the hand holds it; nor do its eyes sweep about
		    // it then
		    if (!held)
		    {
			    // As it moves, the mountains and the water that it finds on its map can catch its eye. Then it notes the
			    // region of the land where it stands, for its map of the places it went to.
			    LookAtLand(entity, creature, mind, transform);
			    LookFarAbout(entity, mind, transform);
			    ecs::creature_exploration_land::Visit(entity, {transform.position.x, transform.position.z});
			    const bool lookedAt = creature.owner == PlayerNames::PLAYER_ONE && view.has_value() &&
			                          creature_attention::IsLookingAt(view->origin, view->focus, transform.position);
			    creature_desire_sources::CountTurn(mind.attention, lookedAt);
			    if (lookedAt)
			    {
				    ++mind.turnsLookedAt;
			    }
			    SweepEyes(entity, creature, mind, animation, transform);
		    }
		    if (mind.feedbackSeconds.has_value())
		    {
			    *mind.feedbackSeconds += k_TurnSeconds;
		    }
		    // Held by the player's hand its mind stands still, but for showing what it wants once it has turned to the
		    // player
		    if (const auto heldTurn = creature_turn_order::WhileHeld(held, mind.showDesireOnHold);
		        heldTurn != creature_turn_order::HeldTurn::Think)
		    {
			    mind.heldSinceRestart = true;
			    if (heldTurn == creature_turn_order::HeldTurn::ShowDesire)
			    {
				    ShowDesireOnHold(entity, mind, animation);
			    }
			    return;
		    }
		    auto* needs = registry.TryGet<CreatureNeeds>(entity);
		    // Under a script's control its desires stand still and it chooses nothing for itself: it only carries out
		    // what it is given, and stands about once that is done
		    const bool scripted = registry.AllOf<ScriptControlled>(entity);
		    // The asker of a joint action can make its mind stand still: its desires and its feelings for other
		    // creatures stand still, and it chooses nothing, but it carries out the orders of the asker
		    const bool demobilised = registry.AllOf<CreatureDemobilised>(entity);
		    TurnBeforeDecision(entity, creature, mind, scripted);
		    StepEmergencies(entity, creature, mind, scripted);
		    // The other creature in a fight obeys the creature that started it: its desires stand still until the fight
		    // is over, or until it wins by a knock-out
		    const auto* fighting = registry.TryGet<const CreatureFighting>(entity);
		    const bool obeysInFight = creature_fight::ObeysInFight({
		        .fighting = fighting != nullptr,
		        .startedIt = fighting != nullptr && fighting->madeArena,
		        .won = fighting != nullptr && fighting->stage == CreatureFighting::Stage::Celebrate,
		    });
		    // How impressive it finds the other creatures, and the attention they gave it, fade each turn it thinks
		    if (!scripted && !demobilised && mind.learnt.has_value())
		    {
			    creature_friendship::Fade(mind.learnt->creatures);
		    }
		    if (!scripted && !obeysInFight && !demobilised)
		    {
			    const auto body = needs != nullptr ? needs->needs : creature_physiology::Needs {};
			    const auto state = SourceStateOf(entity, creature, mind, body.age);
			    creature_desires::UpdateSources(
			        *mind.desires, [&mind, &body, night, &state](uint32_t type, const creature_desires::Desires& desires) {
				        return ReadSource(type, desires, mind, body, night, state);
			        });
			    creature_desires::UpdateDesires(*mind.desires, k_TurnsPerSecond);
		    }

		    // After a knock-out, it takes up reactions again once it lies out cold no more and the agenda it had at the
		    // knock-out is over
		    if (const auto* after = registry.TryGet<const CreatureAfterFight>(entity);
		        after != nullptr && !registry.AllOf<CreatureKnockedOut>(entity) &&
		        (mind.idle.serial != after->agenda || mind.idle.step >= mind.idle.agenda.size()))
		    {
			    registry.Remove<CreatureAfterFight>(entity);
		    }
		    auto* eyes = registry.TryGet<CreatureEyes>(entity);
		    // Paused, fighting or knocked out, the mind leaves the body alone. In a fight, the steps of the fight
		    // choose the point that the head looks at.
		    if (mind.paused || registry.AnyOf<CreatureFighting, CreatureKnockedOut>(entity))
		    {
			    // Exhausted, starved or out of life while it fights, it passes out: its fight is called off for both,
			    // and nobody wins
			    if (!mind.paused && Locator::creatureFightSystem::has_value() && registry.AllOf<CreatureFighting>(entity) &&
			        needs != nullptr && needs->faint.has_value() && mind.turn > k_TurnsBeforePassingOut)
			    {
				    TellFainting(entity);
				    needs->faint.reset();
				    Locator::creatureFightSystem::value().CallOff(entity);
				    passingOut.push_back(entity);
				    return;
			    }
			    // A paused body still draws a gesture that it was told to draw
			    if (const auto* drawing = registry.TryGet<const CreatureGestureDrawing>(entity);
			        mind.paused && drawing != nullptr && !drawing->inDance)
			    {
				    StepGesture(entity);
			    }
			    if (!mind.paused)
			    {
				    animation.lookAt = FightLook(registry, entity, creature, transform, mind);
				    if (eyes != nullptr)
				    {
					    eyes->lookAt = animation.lookAt;
				    }
			    }
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
		    // Without the fights' knock-outs, it drops where it stands and comes round there
		    if (fainting && !creature_mind::IsUnconscious(mind.idle))
		    {
			    if (Locator::creatureLocomotionSystem::has_value())
			    {
				    Locator::creatureLocomotionSystem::value().Stop(entity);
			    }
			    // As with any change of plan, a pose ends and an action plays out before it drops
			    animation.body = creature_layers::EndDance(creature_layers::EndLoop(animation.body));
			    TellFainting(entity);
			    creature_mind::Plan(mind.idle, creature_mind::Activity::Faint, creature_mind::Faint(ShownSize(creature)));
			    needs->faint.reset();
		    }
		    // When the creature is free to choose, it weighs the plans of every desire. When no plan is ready, it does
		    // nothing.
		    if (!scripted && !demobilised && !mind.planActive && mind.idle.step >= mind.idle.agenda.size() &&
		        !(needs != nullptr && needs->faint.has_value()))
		    {
			    // A try at a miracle that fizzled is what it shows first
			    ShowFizzle(entity, mind);
			    if (mind.idle.step >= mind.idle.agenda.size())
			    {
				    PlanCreature(entity, mind, true);
			    }
			    // It counts the times it thinks and still has no plan, from the start of its last agenda
			    if (mind.thinksWithoutPlanSerial != mind.idle.serial)
			    {
				    mind.thinksWithoutPlanSerial = mind.idle.serial;
				    mind.thinksWithoutPlan = 0;
			    }
			    if (!mind.planActive && mind.idle.step >= mind.idle.agenda.size())
			    {
				    ++mind.thinksWithoutPlan;
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
				                       .object = entity,
				                       .ordered = true});
			    }
		    }
		    // When the creature examines a different creature, it finds the desires of the two creatures each turn.
		    // It does not do this on the first turn.
		    if (!scripted && mind.idle.stepStarted && mind.idle.step < mind.idle.agenda.size() &&
		        mind.idle.agenda[mind.idle.step].kind == creature_mind::Step::Kind::Inspect &&
		        mind.idle.agenda[mind.idle.step].object.has_value())
		    {
			    WeighWatchedCreature(entity, mind, static_cast<entt::entity>(*mind.idle.agenda[mind.idle.step].object));
		    }
		    // Shut in, the creature clears a thing out of its way and walks on. With nothing to clear, its walk ends where
		    // it stands. When the search finds no route at all, the creature gives up what it does and makes a sound of
		    // refusal. It also leaves the thing that it went to alone for a time.
		    StepClearingWay(entity, mind);
		    if (mind.idle.step < mind.idle.agenda.size())
		    {
			    const auto& step = mind.idle.agenda[mind.idle.step];
			    auto* walk = registry.TryGet<CreatureLocomotion>(entity);
			    const bool walkToPoint = step.kind == creature_mind::Step::Kind::Move && !step.movement.object.has_value() &&
			                             step.movement.giveUpIfUnreachable &&
			                             (step.movement.kind == creature_mind::Movement::Kind::ToPoint ||
			                              step.movement.kind == creature_mind::Movement::Kind::Nearby);
			    const bool walkToObject = (step.kind == creature_mind::Step::Kind::Move ||
			                               (step.kind == creature_mind::Step::Kind::FollowWatching && mind.idle.followMoved)) &&
			                              step.movement.object.has_value();
			    // Any walk that stops on the first turn of its search can clear the way, one time for each stop. This
			    // includes the walk of the hands to a thing that they pick up, stomp on or kick.
			    const bool handsStep = step.kind == creature_mind::Step::Kind::Object;
			    const bool anyWalk = step.kind == creature_mind::Step::Kind::Move || walkToObject || handsStep;
			    std::optional<CreatureLocomotion::GaveUp> gaveUp;
			    if (walk != nullptr && mind.idle.stepStarted && anyWalk)
			    {
				    gaveUp = std::exchange(walk->gaveUp, std::nullopt);
			    }
			    if (gaveUp.has_value() && ClearWay(entity, mind, gaveUp->destination, gaveUp->met))
			    {
				    // The new plan stops what the hands did
				    if (handsStep && Locator::creatureObjectActionSystem::has_value())
				    {
					    Locator::creatureObjectActionSystem::value().Cancel(entity);
				    }
				    StepClearingWay(entity, mind);
			    }
			    else if (walk != nullptr && walk->failed && mind.idle.stepStarted && (walkToPoint || walkToObject))
			    {
				    if (const auto object = step.movement.object)
				    {
					    mind.beliefs.MarkBlocked(static_cast<entt::entity>(*object), mind.turn);
				    }
				    Abandon(mind);
				    mind.idle.step = mind.idle.agenda.size();
				    mind.idle.stepStarted = false;
				    mind.idle.gaveUp = true;
				    RefuseNoRoute(entity);
			    }
			    // After a pick-up, a stomp or a kick that found no way to its thing, the creature does not choose the thing
			    // again for a time
			    else if (step.kind == creature_mind::Step::Kind::Object && mind.idle.stepStarted &&
			             Locator::creatureObjectActionSystem::has_value() &&
			             HandsOf(entity) == creature_mind::HandsState::Failed)
			    {
				    const auto failure = Locator::creatureObjectActionSystem::value().GetFailure(entity);
				    if (step.order.object.has_value() &&
				        creature_object_actions::LeavesThingAlone(HandsKindOf(step.order.kind), failure))
				    {
					    mind.beliefs.MarkBlocked(static_cast<entt::entity>(*step.order.object), mind.turn);
				    }
				    if (creature_object_actions::RefusesAloud(failure))
				    {
					    RefuseNoRoute(entity);
				    }
			    }
		    }
		    const bool moving =
		        Locator::creatureLocomotionSystem::has_value() && Locator::creatureLocomotionSystem::value().IsMoving(entity);
		    if (mind.idle.step < mind.idle.agenda.size())
		    {
			    ++mind.stepTurns;
		    }
		    StepSubMove(entity, creature_layers::IsPlaying(animation.body));
		    // A drawing that a step started continues each turn; a dance continues its own only on the turns of its move
		    if (const auto* drawing = registry.TryGet<const CreatureGestureDrawing>(entity);
		        drawing != nullptr && !drawing->inDance)
		    {
			    StepGesture(entity);
		    }
		    // A catch takes up the body until it is over, but the plan to catch is carried out as soon as the catch starts
		    const bool catching = Locator::creatureObjectActionSystem::has_value() &&
		                          Locator::creatureObjectActionSystem::value().IsCatching(entity);
		    const bool catchStep = mind.idle.stepStarted && mind.idle.step < mind.idle.agenda.size() &&
		                           mind.idle.agenda[mind.idle.step].kind == creature_mind::Step::Kind::Object &&
		                           mind.idle.agenda[mind.idle.step].order.kind == creature_mind::ObjectOrder::Kind::Catch;
		    // As a step that communicates its state starts, the creature finds what it feels at that time
		    const bool communicates = !mind.idle.stepStarted && mind.idle.step < mind.idle.agenda.size() &&
		                              mind.idle.agenda[mind.idle.step].kind == creature_mind::Step::Kind::Communicate;
		    const auto communication = communicates ? Communication(mind) : creature_desires::Communication {};
		    const auto inHand = Locator::creatureObjectActionSystem::has_value()
		                            ? Locator::creatureObjectActionSystem::value().GetHeld(entity)
		                            : std::nullopt;
		    const auto [cameraFocus, cameraFocusAhead] = CameraFocusOf(registry, entity, mind.idle);
		    const creature_mind::Senses senses {
		        .seconds = k_TurnSeconds,
		        .bodyBusy = creature_layers::IsPlaying(animation.body) || moving || catching,
		        .bodyLooping = creature_layers::IsLooping(animation.body),
		        .clipMissing = ClipMissing(entity, animation, mind.idle, communication.animation),
		        .bodyDancing = creature_layers::IsDancing(animation.body),
		        .moving = moving,
		        .subMove = SubMoveOf(entity),
		        .position = glm::vec2(transform.position.x, transform.position.z),
		        .communication = communication,
		        .rested = needs != nullptr && needs->rested,
		        .better = needs != nullptr && creature_mind::IsBetter(needs->needs.life, needs->needs.exhaustion),
		        .hands = catchStep && catching ? creature_mind::HandsState::Done : HandsOf(entity),
		        .held = inHand.has_value() ? std::optional(entt::to_integral(*inHand)) : std::nullopt,
		        .objectInMap = WaitedForInMap(registry, mind.idle),
		        .objectInTheAir = WatchedIsInTheAir(registry, mind.idle),
		        .objectFlying = StillObjectFlies(registry, mind.idle),
		        .feelings = FeelingsOf(mind),
		        .camera = CameraViewOf(registry, entity, mind.idle),
		        .hand = HandViewOf(registry, entity, mind.idle),
		        .swap = SwapViewOf(entity),
		        .overlayPlaying = animation.gesture.animation.has_value(),
		        .bodyPlaying = creature_layers::IsPlaying(animation.body),
		        .dance = DanceViewOf(entity, mind.idle),
		        .followed = FollowedViewOf(registry, transform.position, mind),
		        .cameraFocus = cameraFocus,
		        .cameraFocusAhead = cameraFocusAhead,
		        .followHand = FollowHandViewOf(registry, entity, mind, creature_layers::IsPlaying(animation.body)),
		        .watchPlayer = WatchPlayerViewOf(registry, entity, mind, creature_layers::IsPlaying(animation.body)),
		        .fighting = registry.AllOf<CreatureFighting>(entity),
		        .totemShare = mind_town_help::TotemShareOf(registry, mind.idle),
		    };
		    const auto commands = creature_mind::Think(mind.idle, senses, random, gameRandom);
		    NoteStepSeen(entity, mind);
		    if (commands.communicated.has_value())
		    {
			    SPDLOG_LOGGER_DEBUG(
			        spdlog::get("game"), "Creature {} shows its state: desire {}, animation {}", entt::to_integral(entity),
			        commands.communicated->desire.has_value() ? static_cast<int>(*commands.communicated->desire) : -1,
			        commands.communicated->animation.has_value() ? static_cast<int64_t>(*commands.communicated->animation)
			                                                     : -1);
			    Communicated(mind);
		    }
		    if (commands.dance.has_value())
		    {
			    CarryOutDance(entity, mind.idle, *commands.dance);
		    }
		    KeepDanceWithAgenda(entity, mind.idle);
		    KeepPaceWithAgenda(entity, mind);
		    if (commands.restEyes)
		    {
			    RestEyes(registry, entity, eyes);
		    }
		    Apply(commands, animation, eyes);
		    if (Move(entity, commands))
		    {
			    GiveUp(mind.idle);
		    }
		    if (commands.move.has_value() && IsSubMove(commands.move->kind) && mind.idle.step < mind.idle.agenda.size())
		    {
			    StartSubMove(entity, *commands.move, mind.idle.agenda[mind.idle.step].seconds);
		    }
		    if (commands.gesture.has_value() && !StartGesture(entity, *commands.gesture, commands.gestureInDance) &&
		        commands.gestureInDance)
		    {
			    StepGesture(entity);
		    }
		    if (commands.endPoint)
		    {
			    EndDanceGesturePoint(entity);
		    }
		    if (commands.releaseCast)
		    {
			    ReleaseCast(entity);
		    }
		    // The miracle is tried as the casting pose's loop begins; the rest of the agenda is no use without it
		    if (commands.cast.has_value() &&
		        ((commands.cast->place.has_value() &&
		          !TryMiracleAt(entity, static_cast<MagicType>(commands.cast->magicType), *commands.cast->place)) ||
		         (!commands.cast->place.has_value() && commands.cast->object.has_value() &&
		          !TryMiracle(entity, static_cast<MagicType>(commands.cast->magicType),
		                      static_cast<entt::entity>(*commands.cast->object)))))
		    {
			    mind.idle.step = mind.idle.agenda.size();
			    mind.idle.stepStarted = false;
			    mind.idle.gaveUp = true;
		    }
		    Order(registry, entity, commands);
		    // It asks the other creature to fight. If the other creature does not agree, the rest of the agenda is no use.
		    if (commands.startFight.has_value() && !StartPlannedFight(entity, static_cast<entt::entity>(*commands.startFight)))
		    {
			    mind.idle.step = mind.idle.agenda.size();
			    mind.idle.stepStarted = false;
			    mind.idle.gaveUp = true;
		    }
		    // A joint action: when the other creature refuses to be its partner, the rest of the agenda is no use
		    if (commands.requestPartner.has_value() &&
		        !RequestPartner(entity, static_cast<entt::entity>(commands.requestPartner->creature),
		                        commands.requestPartner->kind))
		    {
			    GiveUp(mind.idle);
		    }
		    if (commands.order.has_value() && commands.order->step != nullptr)
		    {
			    TakeOrder(static_cast<entt::entity>(commands.order->partner), entity, *commands.order->step);
		    }
		    if (commands.demobilise.has_value())
		    {
			    SetDemobilised(static_cast<entt::entity>(commands.demobilise->first), commands.demobilise->second);
		    }
		    if (commands.letGo.has_value())
		    {
			    LetGoOfHeld(entity, mind, *commands.letGo);
		    }
		    StepSwap(entity, commands);
		    // A rock broken in two at once; anything else can't be, and the rest of the agenda is no use
		    if (commands.shatter.has_value())
		    {
			    const auto rock = static_cast<entt::entity>(*commands.shatter);
			    if (registry.Valid(rock) && object_physics::IsRock(rock) && Locator::dynamicsSystem::has_value())
			    {
				    object_physics::SplitRock(Locator::dynamicsSystem::value(), rock);
			    }
			    else
			    {
				    GiveUp(mind.idle);
			    }
		    }
		    // A building mended by a fifth of its life, and the tree used up mending it gone. When the building gets all of
		    // its life again, the end of the plan can decrease its desire.
		    if (commands.repair.has_value() && registry.Valid(static_cast<entt::entity>(*commands.repair)))
		    {
			    const auto repaired = static_cast<entt::entity>(*commands.repair);
			    static_cast<void>(ecs::world_objects::IncreaseLife(repaired, creature_mind::k_RepairLife));
			    if (creature_mind::RepairedToFullLife(ecs::world_objects::LifeOf(repaired)))
			    {
				    mind.desireSeenTo = true;
			    }
		    }
		    if (commands.seesToDesire)
		    {
			    mind.desireSeenTo = true;
		    }
		    if (commands.heal.has_value())
		    {
			    Heal(entity, static_cast<entt::entity>(*commands.heal));
		    }
		    mind_town_help::Apply(registry, commands);
		    if (commands.renderImmobile.has_value())
		    {
			    MakeStill(registry, static_cast<entt::entity>(*commands.renderImmobile));
		    }
		    if (commands.useUp.has_value() && registry.Valid(static_cast<entt::entity>(*commands.useUp)))
		    {
			    ecs::world_objects::Remove(static_cast<entt::entity>(*commands.useUp));
		    }
		    // What its hand holds goes, and its hand is empty
		    if (commands.useUpHeld && Locator::creatureObjectActionSystem::has_value())
		    {
			    if (const auto usedUpHeld = Locator::creatureObjectActionSystem::value().GetHeld(entity))
			    {
				    mind.idle.usedUpHeld = entt::to_integral(*usedUpHeld);
				    ecs::world_objects::Remove(*usedUpHeld);
			    }
		    }
		    if (commands.restSitting && Locator::creaturePhysiologySystem::has_value())
		    {
			    Locator::creaturePhysiologySystem::value().SitTurn(entity);
		    }
		    // Its own fire stops: it cools to the temperature of the air
		    if (commands.coolDown && Locator::fireSystem::has_value())
		    {
			    Locator::fireSystem::value().PutOut(entity);
		    }
		    if (commands.healItself)
		    {
			    HealItself(registry, entity);
		    }
		    // It faints, and those near it react as to a death
		    if (commands.fainted && Locator::reactionSystem::has_value())
		    {
			    Locator::reactionSystem::value().Create(
			        {.initiator = entity, .type = Reaction::ReactToDeath, .position = transform.position});
		    }
		    if (commands.effect == creature_mind::Effect::RanAway)
		    {
			    EndRunAway(entity, mind, commands.effectObject);
		    }
		    if (commands.douse.has_value())
		    {
			    if (Douse(entity, static_cast<entt::entity>(*commands.douse)))
			    {
				    mind.desireSeenTo = true;
			    }
		    }
		    TakeEffect(entity, commands, *mind.desires, mind.planActive);
		    // After this step, the plan acts on the creature, not on the thing that it chose
		    if (commands.actOnItself && mind.planner.current.has_value())
		    {
			    mind.planner.current->object = entt::to_integral(entity);
		    }
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
		    // A stomp that ends the agenda and kills the thing counts the kill and sees to the desire of the action
		    if (commands.effect == creature_mind::Effect::Stomped && commands.lastStep && commands.effectObject.has_value())
		    {
			    StompedToDeath(entity, mind, static_cast<entt::entity>(*commands.effectObject));
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
		    const bool plannedMeal = commands.effect == creature_mind::Effect::Eat && mind.planActive;
		    if (commands.effect != creature_mind::Effect::None && commands.effect != creature_mind::Effect::Observed &&
		        commands.effect != creature_mind::Effect::Inspected && commands.effect != creature_mind::Effect::TookFromPile &&
		        commands.effect != creature_mind::Effect::TookFromHand && commands.effect != creature_mind::Effect::Stomped &&
		        !plannedMeal)
		    {
			    mind.satisfiedByEffect = true;
		    }
		    FollowAgenda(entity, mind);
		    // The turns that the agenda runs steps for
		    if (mind.agendaTurnsSerial != mind.idle.serial)
		    {
			    mind.agendaTurnsSerial = mind.idle.serial;
			    mind.agendaTurns = 0;
		    }
		    if (mind.idle.step < mind.idle.agenda.size())
		    {
			    ++mind.agendaTurns;
		    }
		    // Once it has taken what the hand gave it, its desires choose what to do with it
		    if (commands.effect == creature_mind::Effect::TookFromHand && commands.effectObject.has_value())
		    {
			    UseGift(entity, static_cast<entt::entity>(*commands.effectObject));
		    }
		    if (needs != nullptr)
		    {
			    needs->rest = creature_mind::IsUnconscious(mind.idle) ? CreatureNeeds::Rest::Unconscious
			                  : creature_mind::IsAsleep(mind.idle)    ? CreatureNeeds::Rest::Asleep
			                  : creature_mind::IsResting(mind.idle)   ? CreatureNeeds::Rest::Resting
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
		    else if (commands.gaze.has_value() && commands.gaze->runningAway)
		    {
			    // Running away, it looks back at what it runs from for a time, then looks about for a time
			    const bool back =
			        creature_emote::RunLooksBack(mind.runLookBack, k_TurnsPerSecond, RunLookBackSeconds(creature.species),
			                                     RunLookAheadSeconds(creature.species));
			    auto from = *commands.gaze;
			    from.runningAway = false;
			    const auto point = back ? GazePoint(registry, entity, from, moving) : std::nullopt;
			    animation.lookAt = point.has_value() ? point : LookAbout(creature, transform, mind);
		    }
		    else if (commands.gaze.has_value() && commands.gaze->whileGoing)
		    {
			    // On the way to a place: the head looks at the place, at what it watches, or from side to side. Without
			    // a point of its own, the look uses the top of the thing that it goes to, or else the place that the
			    // creature walks to.
			    const auto about = LookAbout(creature, transform, mind);
			    auto place = commands.gaze->point;
			    if (!place.has_value() && commands.gaze->object.has_value())
			    {
				    place = GazePoint(registry, entity, {.object = commands.gaze->object}, false);
			    }
			    if (const auto* walk = registry.TryGet<const CreatureLocomotion>(entity);
			        !place.has_value() && walk != nullptr && walk->destination.has_value())
			    {
				    place = glm::vec3(walk->destination->x, 0.0f, walk->destination->y);
			    }
			    animation.lookAt =
			        place.has_value() ? LookWhileGoing(entity, creature, transform, mind, *place).value_or(about) : about;
		    }
		    else
		    {
			    // A step with nothing to look at, or whose thing is gone, looks about. The pose of the body decides if
			    // the head can turn to the point.
			    const auto point =
			        commands.gaze.has_value() ? GazePoint(registry, entity, *commands.gaze, moving) : std::nullopt;
			    animation.lookAt = point.has_value() ? *point : LookAbout(creature, transform, mind);
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
	// A joint action stops for both creatures when either stops what it does for it
	KeepJointActions();
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
	// The variety of faces moves on by the random numbers of the game
	const auto gameRandom = [&random](uint32_t range) {
		return Locator::gameRandom::has_value() ? Locator::gameRandom::value().GameRand(range) : random(range);
	};
	const creature_mind::Senses senses {.feelings = FeelingsOf(*mind)};
	const auto face = creature_mind::PullFace(mind->idle, cue, senses, random, gameRandom);
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
	// The average of the feedback is one for all creatures
	_averageFeedback = creature_feedback::AverageAfter(_averageFeedback, feedback);
}

void CreatureMindSystem::TurnBeforeDecision(entt::entity creature, const Creature& body, CreatureMindState& mind, bool scripted)
{
	// Before the first stage, the desires of its body stay at 0
	if (mind.desires.has_value())
	{
		creature_turn_order::HoldBodyDesires(*mind.desires, BodyDesires(), mind.developmentPhase);
	}
	const auto planHasSteps = [&mind]() {
		return mind.planActive && mind.planner.current.has_value() && mind.idle.serial == mind.planSerial &&
		       mind.idle.step < mind.idle.agenda.size();
	};
	// After a hold, the first plan that has steps to do starts again from its first step
	if (creature_turn_order::RestartsAfterHold(mind.heldSinceRestart, planHasSteps()))
	{
		mind.heldSinceRestart = false;
		StartPlanAgain(creature, mind);
	}
	if (planHasSteps() && creature_turn_order::OwnPlanRuns(true, mind.planner.current->desire))
	{
		mind.ownPlanSinceStop = true;
	}
	// The copy timeout applies only to the creature of a player that is not the computer, and not under a script
	const bool computer = Locator::playerSystem::has_value() && Locator::playerSystem::value().IsComputerPlayer(body.owner);
	if (body.owner == PlayerNames::NEUTRAL || computer || scripted)
	{
		return;
	}
	if (creature_turn_order::StepCopyTimeout(mind.turnsWithoutOwnPlan, mind.ownPlanSinceStop, k_WholeTurnsPerSecond))
	{
		// It stops copying the player, the plan is no longer forced, and it does not follow the player for a time
		if (mind.learnt.has_value())
		{
			mind.learnt->mimicry.reset();
		}
		mind.planForced = false;
		if (mind.desires.has_value())
		{
			creature_desires::Suppress(*mind.desires, Desire::FollowPlayerDesire,
			                           creature_turn_order::k_NoFollowAfterCopyTimeoutSeconds, k_TurnsPerSecond);
		}
	}
}

void CreatureMindSystem::StartPlanAgain(entt::entity creature, CreatureMindState& mind)
{
	const auto plan = *mind.planner.current;
	// The context of the plan stays as it was for the player's feedback
	const bool contextRunning =
	    mind.learnt.has_value() && !mind.learnt->contexts.empty() && mind.learnt->contexts.back().running;
	// A catch is the body's own and carries on
	if (Locator::creatureObjectActionSystem::has_value() &&
	    Locator::creatureObjectActionSystem::value().GetState(creature) == CreatureObjectActionSystemInterface::State::Busy &&
	    !Locator::creatureObjectActionSystem::value().IsCatching(creature))
	{
		Locator::creatureObjectActionSystem::value().Cancel(creature);
	}
	creature_plan_actions::Situation situation {.instrument = plan.instrument, .activity = plan.activityObject};
	Situate(creature, situation);
	auto built = PlanAgenda(creature, plan.action, plan.object, situation);
	if (!built.has_value())
	{
		// With no agenda, the plan stops and the creature decides again
		SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Creature {} has no agenda to start {} again after the hand let go",
		                    entt::to_integral(creature), plan.action);
		Abandon(mind);
		return;
	}
	auto& [executor, agenda] = *built;
	if (!Replan(creature, executor->activity, std::move(agenda)))
	{
		SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Creature {} cannot start {} again after the hand let go",
		                    entt::to_integral(creature), plan.action);
		return;
	}
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "Creature {} starts {} again after the hand let go", entt::to_integral(creature),
	                   plan.action);
	mind.planner.current = plan;
	mind.planActive = true;
	mind.planSerial = mind.idle.serial;
	mind.agendaSeen = mind.idle.serial;
	mind.satisfiedByEffect = false;
	mind.desireSeenTo = false;
	if (contextRunning)
	{
		mind.learnt->contexts.back().running = true;
	}
}

void CreatureMindSystem::NoteLeashOrder(entt::entity creature)
{
	if (auto* mind = Locator::entitiesRegistry::value().TryGet<CreatureMindState>(creature))
	{
		mind->attention.turnsSinceInteraction = 0;
	}
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

void CreatureMindSystem::SendReleaseFeedback(entt::entity creature, float feedback)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* mind = registry.TryGet<CreatureMindState>(creature);
	if (mind == nullptr)
	{
		return;
	}
	// The feedback of an earlier release arrives first
	if (const auto earlier = std::exchange(mind->releaseFeedback, std::nullopt); earlier.has_value())
	{
		ReceiveFeedback(creature, *earlier);
		mind = registry.TryGet<CreatureMindState>(creature);
		if (mind == nullptr)
		{
			return;
		}
	}
	mind->releaseFeedback = feedback;
	mind->releaseFeedbackTurns = creature_feedback::k_ReleaseFeedbackTurns;
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
	if (auto* body = registry.TryGet<CreatureAnimation>(creature); body != nullptr)
	{
		body->body = creature_layers::EndDance(body->body);
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
	RestEyes(registry, creature, registry.TryGet<CreatureEyes>(creature));
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
		if (const auto nearest = NearestFood(registry, glm::vec2(transform->position.x, transform->position.z)))
		{
			food = nearest->first;
		}
	}
	if (!food.has_value() || !FoodValueOf(*food).has_value())
	{
		return false;
	}
	const bool holdingEatable =
	    Locator::creatureObjectActionSystem::has_value() && Locator::creatureObjectActionSystem::value().CanEatHeld(creature);
	return Replan(creature, creature_mind::Activity::Eat, creature_mind::Eat(entt::to_integral(*food), holdingEatable));
}

bool CreatureMindSystem::Drink(entt::entity creature)
{
	const auto* transform = Locator::entitiesRegistry::value().TryGet<const Transform>(creature);
	if (transform == nullptr)
	{
		return false;
	}
	const auto* body = Locator::entitiesRegistry::value().TryGet<const Creature>(creature);
	const auto place = DrinkPlaceOf(creature);
	return body != nullptr && place.has_value() &&
	       Replan(creature, creature_mind::Activity::Drink,
	              creature_drink::Drink(*place, creature_mode::CreatureHeight(ShownSize(*body))));
}

bool CreatureMindSystem::Poo(entt::entity creature)
{
	return Replan(
	    creature, creature_mind::Activity::Poo,
	    creature_mind::Poo(
	        [this](uint32_t range) { return range == 0 ? 0u : std::uniform_int_distribution<uint32_t>(0, range - 1)(_random); },
	        std::nullopt));
}

bool CreatureMindSystem::Puke(entt::entity creature)
{
	return Replan(creature, creature_mind::Activity::Puke, creature_mind::Puke());
}

bool CreatureMindSystem::Faint(entt::entity creature)
{
	// It faints as a fight's loser does: it lies where it fell, then a player's creature is carried to its pen to come
	// round
	auto& registry = Locator::entitiesRegistry::value();
	if (Locator::creatureFightSystem::has_value() && registry.Valid(creature))
	{
		TellFainting(creature);
		Locator::creatureFightSystem::value().ForceFaint(creature);
		return registry.AllOf<CreatureKnockedOut>(creature);
	}
	const auto* body = registry.TryGet<const Creature>(creature);
	return Replan(creature, creature_mind::Activity::Faint, creature_mind::Faint(body != nullptr ? ShownSize(*body) : 1.0f));
}

void CreatureMindSystem::Heal(entt::entity creature, entt::entity thing)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(thing))
	{
		return;
	}
	// The thing takes a full heal from the creature, which moves the creature's alignment as a heal does
	if (Locator::magicSystem::has_value())
	{
		magic::EffectValues values;
		values[magic::EffectKind::Heal] = 1.0f;
		const auto* body = registry.TryGet<const Creature>(creature);
		Locator::magicSystem::value().ApplyEffectToObject(
		    thing, values,
		    magic::EffectSource {.player = body != nullptr ? body->owner : PlayerNames::NEUTRAL,
		                         .casterCreature = creature,
		                         .appliedBy = creature});
	}
	if (!registry.Valid(thing) || !magic_living::LifeOf(thing).has_value())
	{
		return;
	}
	// A living thing gets all its life back. A villager then goes home, and an animal wanders off.
	SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Creature {} heals {} from life {}", entt::to_integral(creature),
	                    entt::to_integral(thing), magic_living::LifeOf(thing).value_or(0.0f));
	magic_living::SetLife(thing, 1.0f);
	if (auto* action = registry.TryGet<LivingAction>(thing);
	    action != nullptr && registry.AllOf<Villager>(thing) && Locator::livingActionSystem::has_value())
	{
		static_cast<void>(Locator::livingActionSystem::value().VillagerChangeTopState(*action, VillagerStates::GoHome));
	}
	else if (registry.AllOf<Animal>(thing) && Locator::animalSystem::has_value())
	{
		Locator::animalSystem::value().StartWandering(thing);
	}
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

void CreatureMindSystem::FightStarted(entt::entity creature, entt::entity opponent, bool planned)
{
	auto& registry = Locator::entitiesRegistry::value();
	// Each creature's agenda waits for the fight to end. The fight has started already, so the step that starts it is
	// marked as started.
	const auto fightWith = [&](entt::entity self, entt::entity other, const ForcedPlan& plan) {
		if (registry.AllOf<ScriptControlled>(self) || !ForcePlan(self, plan))
		{
			return;
		}
		if (auto* mind = registry.TryGet<CreatureMindState>(self); mind != nullptr && !mind->idle.agenda.empty())
		{
			mind->idle.stepStarted = mind->idle.agenda.front().kind == creature_mind::Step::Kind::Fight &&
			                         mind->idle.agenda.front().object == std::optional(entt::to_integral(other));
		}
	};
	const auto steps = [](entt::entity other) {
		return std::vector {creature_mind::Step {.kind = creature_mind::Step::Kind::Fight, .object = entt::to_integral(other)}};
	};
	// A creature told to fight, by the leash, a script or the debug tools, fights until the fight ends: nothing it wants
	// stops the fight.
	// TODO(creature-fight-mind): the leash makes the plan from the desires of the creature for the other creature. Until
	// the leash does that for creatures, the plan is to obey the player.
	if (!planned)
	{
		// A fight forced on a creature, like any forced action, makes every free creature agree to joint actions from
		// now on
		registry.Context().partnersAlwaysAgree = true;
		fightWith(creature, opponent,
		          {.desire = Desire::ObeyPlayer, .action = k_FightAction, .object = opponent, .agenda = steps(opponent)});
	}
	// The other creature stops what it does and obeys the first creature until the fight ends
	fightWith(opponent, creature,
	          {.desire = Desire::ObeyCreature, .action = k_ObeyCreatureAction, .object = creature, .agenda = steps(creature)});
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
	SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Creature {} is made to catch {}", entt::to_integral(creature),
	                    entt::to_integral(object));
	// It comes across the thing that it catches, and what it was doing stops as a failure
	ComeAcross(creature, *mind, object);
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
		                                                .goalUsefulness = creature_planner::k_CatchOrGiftScore,
		                                                .actionPriority = creature_planner::k_CatchOrGiftScore,
		                                                .priority = creature_planner::k_CatchOrGiftScore};
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
	// The action that its body plays continues at two times the usual speed
	if (auto* body = registry.TryGet<CreatureAnimation>(creature))
	{
		body->body = creature_layers::Hurry(body->body);
	}
	// Without a reaction, it keeps the plan that it does now. After it takes the thing, it can continue that
	// plan. With a reaction, it keeps the plan from the start of the reaction.
	if (const auto* reaction = registry.TryGet<const LivingReaction>(creature); reaction == nullptr || reaction->reaction == 0)
	{
		mind->keptPlan = mind->planner.current;
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
	                                                .goalUsefulness = creature_planner::k_CatchOrGiftScore,
	                                                .actionPriority = creature_planner::k_CatchOrGiftScore,
	                                                .priority = creature_planner::k_CatchOrGiftScore};
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
	// Grown up enough, it casts a one-shot globe that it was given
	if (registry.AllOf<OneOffSpellSeed>(object) && CastGivenGlobe(creature, *mind, object))
	{
		return;
	}
	// It comes across what it was given, then weighs it as a thing to act on for each desire it could serve; held, it
	// counts as near as can be
	ComeAcross(creature, *mind, object);
	if (ContinueKeptPlanWith(creature, *mind, object))
	{
		return;
	}
	const auto& desires = *mind->desires;
	const auto weigh = [&](Desire desire) {
		return creature_giving::Weighed {.desire = desires[desire].value,
		                                 .usefulness = ObjectToActOnUsefulness(creature, *mind, desire, object)};
	};
	// It remembers the sorts of object that it was given or played with, and that it learnt it cannot eat, by the object
	// type of their info
	const auto sort = mind_detail::ObjectSortOf(registry, object);
	const auto choice = creature_giving::ChooseUse({
	    .curiosity = weigh(Desire::Curiosity),
	    .hunger = weigh(Desire::Hunger),
	    .playWithPlayer = weigh(Desire::PlayWithPlayer),
	    .cantEatKind = sort.has_value() && mind->refusedFoodSorts.contains(*sort),
	    .givenKindBefore = sort.has_value() && mind->heldSortCounts.contains(*sort),
	});
	if (choice.remembersKind && sort.has_value())
	{
		mind->heldSortCounts[*sort] = 1;
	}
	const auto number = entt::to_integral(object);
	// The plan is about what the thing belongs to, if anything
	const auto owner = BelongsTo(object);
	switch (choice.use)
	{
	case creature_giving::Use::Examine:
		ForcePlan(creature, {.desire = Desire::Curiosity,
		                     .action = k_ExamineGivenAction,
		                     .object = object,
		                     .agenda = creature_mind::ExamineGiven(number),
		                     .activityObject = owner,
		                     .ordered = true});
		break;
	case creature_giving::Use::Eat:
		ForcePlan(creature, {.desire = Desire::Hunger,
		                     .action = k_EatGivenAction,
		                     .object = object,
		                     .agenda = creature_mind::EatGiven(),
		                     .activityObject = owner,
		                     .ordered = true});
		break;
	case creature_giving::Use::Throw:
	{
		const auto eye = Locator::camera::has_value() ? Locator::camera::value().GetOrigin() : glm::vec3(0.0f);
		ForcePlan(creature, {.desire = Desire::PlayWithPlayer,
		                     .action = k_ThrowGivenAction,
		                     .object = object,
		                     .agenda = creature_mind::ThrowGiven(eye),
		                     .activityObject = owner,
		                     .ordered = true});
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
	if (forced.comeAcross && registry.Valid(forced.actionObject))
	{
		ComeAcross(creature, *mind, forced.actionObject);
	}
	// A forced plan first stops the reaction that the creature has
	EndReaction(creature);
	// The current action stops as a failure. This occurs also when the creature finds no new plan.
	// A catch is the body's own and carries on
	if (Locator::creatureObjectActionSystem::has_value() &&
	    Locator::creatureObjectActionSystem::value().GetState(creature) == CreatureObjectActionSystemInterface::State::Busy &&
	    !Locator::creatureObjectActionSystem::value().IsCatching(creature))
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
	    .activity = toNumber(forced.activityObject),
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
	mind->planForced = true;
	mind->planSerial = mind->idle.serial;
	mind->agendaSeen = mind->idle.serial;
	mind->satisfiedByEffect = false;
	mind->desireSeenTo = false;
	// It is remembered for the player's feedback, unless nothing says why it wants what the plan serves
	auto& desire = (*mind->desires)[forced.desire];
	if (const auto source = creature_learning::PlanSource(desire); source.has_value())
	{
		const auto& info = tables->actions[*action];
		RememberAction(creature, *mind,
		               {
		                   .action = *action,
		                   .desire = forced.desire,
		                   .object = actionObject,
		                   .belief = actionObject.has_value()
		                                 ? ActedOnBeliefOf(registry, static_cast<entt::entity>(*actionObject), creature)
		                                 : std::nullopt,
		                   .activity = toNumber(forced.activityObject),
		                   .activityBelief = forced.activityObject.has_value()
		                                         ? BeliefOf(registry, *forced.activityObject, creature)
		                                         : std::nullopt,
		                   .learnable = info.learnable,
		                   .windowSeconds = info.learningWindowSeconds,
		                   .source = *source,
		                   .ordered = forced.ordered,
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
		// The action stops as a failure, and its agenda ends with it. The creature then chooses a new plan.
		mind->idle.step = mind->idle.agenda.size();
		mind->idle.stepStarted = false;
	}
	ReleaseCast(creature);
}

bool CreatureMindSystem::ForceDesireOn(entt::entity creature, creature_desires::Desire desire, entt::entity thing)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* mind = registry.Valid(creature) ? registry.TryGet<CreatureMindState>(creature) : nullptr;
	return mind != nullptr && registry.Valid(thing) && ForceDesireAbout(creature, *mind, desire, thing);
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
