/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// What a creature wants: how the desires of a species start, how their sources and values change each turn, and how
// an action that a creature does satisfies a desire

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

namespace
{
/// The player's hand holds the creature
bool HeldByHand(entt::entity creature)
{
	return openblack::Locator::creatureHandSystem::has_value() &&
	       openblack::Locator::creatureHandSystem::value().IsLocked(creature);
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

/// The desire to show how it is grows with all its desires added up, fully at this much
constexpr float k_ManifestSum = 3.0f;

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
		desire.max = initial.initialMax;
		desire.decayMin = initial.decayMin;
		desire.decayMax = initial.decayMax;
		desire.increaseSeconds = PerSpecies(info.creatureDesireForType.at(d), row);
		desire.weight = initial.initialWeight;
		static_assert(std::tuple_size_v<decltype(initial.sources)> == creature_desires::k_MaxSources);
		for (const auto type : initial.sources)
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
			    .multiplier = table.multiplier,
			    .clearedWhenSatisfied = table.clearedWhenSatisfied != 0,
			});
			desire.spreads.push_back({.value = table.valueSpread, .threshold = table.thresholdSpread});
		}
	}
	return setup;
}

creature_desires::DesireSetup::Spread SourceSpreadOf(uint32_t type)
{
	if (!Locator::infoConstants::has_value())
	{
		return {};
	}
	const auto& table = Locator::infoConstants::value().desireSourceTable;
	if (type >= table.size())
	{
		return {};
	}
	return {.value = table.at(type).valueSpread, .threshold = table.at(type).thresholdSpread};
}

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
		body.at(d) = info.creatureInitialDesire.at(d).bodyDesire != 0;
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
		for (const auto desire : entry.desiresOn | std::views::filter(valid))
		{
			phase.add.push_back(static_cast<Desire>(desire));
		}
		for (const auto desire : entry.desiresOff | std::views::filter(valid))
		{
			phase.remove.push_back(static_cast<Desire>(desire));
		}
	}
	return phases;
}

/// How interesting the creature finds its player this turn: the hand of the player that owns the creature, where the
/// reports of that player put it. A creature of a player with no hand, or of no player, finds nothing of interest here.
float PlayerInterestNow(const Creature& creature, CreatureMindState& mind)
{
	// A creature with no player has no hand to read. Every god has a hand (see ecs::PlayerHandsOf).
	if (creature.owner == PlayerNames::NEUTRAL)
	{
		return creature_desire_sources::InterestNow(mind.playerInterest, std::nullopt, std::nullopt);
	}
	const auto& registry = Locator::entitiesRegistry::value();
	constexpr auto k_Hand = static_cast<size_t>(HandSystemInterface::Side::Left);
	const auto hands =
	    Locator::handSystem::has_value() ? Locator::handSystem::value().GetHandsOf(creature.owner) : std::nullopt;
	const auto handEntity = hands.has_value() ? hands->at(k_Hand) : entt::null;
	// Off the land the reported hand has no place: it counts as where it last was
	auto hand = openblack::ecs::PlayerHandsOf(creature.owner).at(k_Hand);
	if (!hand.has_value())
	{
		hand = mind.playerInterest.lastHand;
	}
	std::optional<float> heldRadius;
	if (const auto* grab = registry.Valid(handEntity) ? registry.TryGet<const HandGrab>(handEntity) : nullptr;
	    grab != nullptr && registry.Valid(grab->object) &&
	    (grab->state == HandGrab::State::Holding || grab->state == HandGrab::State::ReadyToThrow ||
	     grab->state == HandGrab::State::LettingGo))
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
		state.homeDistance = gutils::GetDistanceInMetres(glm::xz(transform->position), glm::xz(*home));
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

bool IsNight()
{
	return Locator::skyClockSystem::has_value() && Locator::skyClockSystem::value().GetClock().IsVisualNight();
}

float DesireValue(const creature_desires::Desires& desires, Desire desire)
{
	return desires.IsActivated(desire) ? desires[desire].value : 0.0f;
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
		creature_desires::LessenAfterAction(desires, static_cast<Desire>(found->desire), found->desireMultiplier,
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
} // namespace openblack::ecs::systems::mind_detail

using namespace openblack::ecs::systems::mind_detail;

void CreatureMindSystem::MindTurn(entt::entity entity)
{
	auto& registry = Locator::entitiesRegistry::value();
	std::erase(_thinking, entity);
	if (!Thinks(registry, entity))
	{
		return;
	}
	const auto& creature = registry.Get<const Creature>(entity);
	auto& mind = registry.Get<CreatureMindState>(entity);
	auto& animation = registry.Get<CreatureAnimation>(entity);
	const auto& transform = registry.Get<const Transform>(entity);
	// Where the player's camera is and looks, which tells whether it is looking at the player's creatures
	// Where the camera of the creature's own player is and looks, which tells whether that player looks at it
	struct View
	{
		glm::vec3 origin;
		glm::vec3 focus;
	};
	const auto reported = openblack::ecs::ReportedCameraOfItsPlayer(entity);
	const auto view =
	    reported.has_value() ? std::optional(View {.origin = reported->origin, .focus = reported->focus}) : std::nullopt;
	SetUpMind(entity, creature, mind);
	if (mind.pendingFile != nullptr)
	{
		TakeUpFile(entity, mind);
	}
	for (const auto& teaching : std::exchange(mind.pendingTeaching, {}))
	{
		if (teaching.distinction)
		{
			LearnDistinction(entity, teaching.desire, teaching.attribute, teaching.value);
			continue;
		}
		if (teaching.everything)
		{
			// A value of 2 omits nothing, 0 omits the skills and 1 omits the miracles
			const uint32_t excluded = !teaching.everySkill ? 0u : !teaching.everyMiracle ? 1u : 2u;
			LearnEverythingExcluding(entity, excluded);
			continue;
		}
		SetKnowsAction(entity, teaching.type, teaching.action, teaching.knows);
	}
	++mind.turn;
	// What scripts set its sources to since the last turn lasts: what each source follows takes the value
	TakeWrittenSources(entity, mind);
	// How interesting it finds its player follows the hand, also while the hand holds it
	creature_desire_sources::UpdateInterest(mind.playerInterest, PlayerInterestNow(creature, mind));
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
	// On the Gods' Playground every desire counts as activated
	mind.desires->everyDesireActivated =
	    Locator::playgroundTripSystem::has_value() && Locator::playgroundTripSystem::value().InfluenceEverywhere();
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
		const bool lookedAt =
		    view.has_value() && creature_attention::IsLookingAt(view->origin, view->focus, transform.position);
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
	// Under a script's control its desires stand still and it chooses nothing for itself: it only carries out
	// what it is given, and stands about once that is done
	const bool scripted = registry.AllOf<ScriptControlled>(entity);
	TurnBeforeDecision(entity, creature, mind, scripted);
	StepEmergencies(entity, creature, mind, scripted);
	_thinking.push_back(entity);
}

void CreatureMindSystem::DesiresTurn(entt::entity entity)
{
	auto& registry = Locator::entitiesRegistry::value();
	std::erase(_acting, entity);
	// A mind that the hand holds, or that does not think, stands still
	if (std::erase(_thinking, entity) == 0 || !Thinks(registry, entity))
	{
		return;
	}
	const auto& creature = registry.Get<const Creature>(entity);
	auto& mind = registry.Get<CreatureMindState>(entity);
	auto& animation = registry.Get<CreatureAnimation>(entity);
	const auto& transform = registry.Get<const Transform>(entity);
	const bool night = IsNight();
	auto* needs = registry.TryGet<CreatureNeeds>(entity);
	const bool scripted = registry.AllOf<ScriptControlled>(entity);
	// The asker of a joint action can make its mind stand still: its desires and its feelings for other
	// creatures stand still, and it chooses nothing, but it carries out the orders of the asker
	const bool demobilised = registry.AllOf<CreatureDemobilised>(entity);
	// The other creature in a fight obeys the creature that started it: its desires stand still from the order to stand
	// still until the fight is over, or until it wins by a knock-out
	const auto* fighting = registry.TryGet<const CreatureFighting>(entity);
	const bool obeysInFight = creature_fight::ObeysInFight({
	    .fighting = fighting != nullptr,
	    .startedIt = fighting != nullptr && fighting->madeArena,
	    .won = fighting != nullptr && fighting->stage == CreatureFighting::Stage::Celebrate,
	    .onlyAsked = fighting != nullptr && fighting->part == CreatureFighting::Part::Asked,
	    .mindFree = fighting != nullptr && fighting->mindFree,
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
		creature_desires::UpdateSources(*mind.desires,
		                                [&mind, &body, night, &state](uint32_t type, const creature_desires::Desires& desires) {
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
	// Exhausted, starved or out of life while it is in a fight, it passes out: its fight is called off for both, and
	// nobody wins
	if (!mind.paused && Locator::creatureFightSystem::has_value() && fighting != nullptr && needs != nullptr &&
	    needs->faint.has_value() && mind.turn > k_TurnsBeforePassingOut)
	{
		TellFainting(entity);
		needs->faint.reset();
		Locator::creatureFightSystem::value().CallOffForPlan(entity);
		Locator::creatureFightSystem::value().KnockOut(entity);
		return;
	}
	// Paused, fighting or knocked out, the mind leaves the body alone. In a fight, the steps of the fight
	// choose the point that the head looks at. The other creature of a fight continues with what it did until the
	// first creature tells it to obey.
	const bool fightMovesBody = fighting != nullptr && fighting->part == CreatureFighting::Part::Obeys;
	if (mind.paused || fightMovesBody || registry.AllOf<CreatureKnockedOut>(entity))
	{
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
			Locator::creatureFightSystem::value().KnockOut(entity);
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
	_acting.push_back(entity);
}

void CreatureMindSystem::ChangeDesireSource(entt::entity creature, uint32_t type, float amount)
{
	auto* mind = Locator::entitiesRegistry::value().TryGet<CreatureMindState>(creature);
	if (mind != nullptr && mind->desires.has_value())
	{
		creature_desires::ChangeSource(*mind->desires, type, amount);
	}
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
