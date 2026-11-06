/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// How a creature learns from what the hand of its player shows it or gives it

#define LOCATOR_IMPLEMENTATIONS

#include <cmath>

#include <algorithm>
#include <array>
#include <chrono>
#include <functional>
#include <iterator>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <LHVM.h>
#include <fmt/format.h>
#include <glm/geometric.hpp>
#include <glm/gtx/vec_swizzle.hpp>
#include <spdlog/spdlog.h>

#include "3D/CreatureBody.h"
#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "Animals/AnimalRules.h"
#include "Camera/Camera.h"
#include "Common/GUtilsAngle.h"
#include "Common/GUtilsDistance.h"
#include "Common/GameRandom.h"
#include "Common/X87Vector.h"
#include "Creature/CreatureActionAgendas.h"
#include "Creature/CreatureActionHistory.h"
#include "Creature/CreatureActivity.h"
#include "Creature/CreatureAgendaArea.h"
#include "Creature/CreatureAttention.h"
#include "Creature/CreatureBringHome.h"
#include "Creature/CreatureDance.h"
#include "Creature/CreatureDecisionTree.h"
#include "Creature/CreatureDesireSources.h"
#include "Creature/CreatureDesires.h"
#include "Creature/CreatureEmoteAgendas.h"
#include "Creature/CreatureExplorationMap.h"
#include "Creature/CreatureFight.h"
#include "Creature/CreatureGiving.h"
#include "Creature/CreatureKindAgendas.h"
#include "Creature/CreatureLayers.h"
#include "Creature/CreatureLearning.h"
#include "Creature/CreatureMeals.h"
#include "Creature/CreatureMind.h"
#include "Creature/CreatureMindModel.h"
#include "Creature/CreatureMindTables.h"
#include "Creature/CreatureMode.h"
#include "Creature/CreatureMusicReaction.h"
#include "Creature/CreatureObjectActions.h"
#include "Creature/CreatureObjectBeliefs.h"
#include "Creature/CreaturePlanActions.h"
#include "Creature/CreaturePlanner.h"
#include "Creature/CreaturePlayerAttention.h"
#include "Creature/CreatureRestAndHome.h"
#include "Creature/CreatureRig.h"
#include "Creature/CreatureRoute.h"
#include "Creature/CreatureSacrifice.h"
#include "Creature/CreatureScriptAgendas.h"
#include "Creature/CreatureScriptPlay.h"
#include "Creature/CreatureSpellMind.h"
#include "Creature/CreatureTownCompassion.h"
#include "Creature/CreatureTownTheft.h"
#include "Creature/CreatureTraining.h"
#include "Creature/CreatureTurnOrder.h"
#include "Creature/CreatureWatching.h"
#include "Creature/LeashOrders.h"
#include "Creature/LeashRules.h"
#include "CreatureMindFootball.h"
#include "CreatureMindSystem.h"
#include "CreatureMindSystemDetail.h"
#include "CreatureMindTownHelp.h"
#include "ECS/BuildingSites.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/AnimatedStatic.h"
#include "ECS/Components/AtHome.h"
#include "ECS/Components/Ball.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureFight.h"
#include "ECS/Components/CreatureLeash.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/CreatureNeeds.h"
#include "ECS/Components/CreatureObjectAction.h"
#include "ECS/Components/CreaturePartner.h"
#include "ECS/Components/CreatureSpells.h"
#include "ECS/Components/Dance.h"
#include "ECS/Components/DeadTree.h"
#include "ECS/Components/Feature.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/FishFarm.h"
#include "ECS/Components/Flock.h"
#include "ECS/Components/Flowers.h"
#include "ECS/Components/Forest.h"
#include "ECS/Components/HandGrab.h"
#include "ECS/Components/LandForest.h"
#include "ECS/Components/LearningMarker.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/LivingReaction.h"
#include "ECS/Components/MagicFireBall.h"
#include "ECS/Components/MagicPile.h"
#include "ECS/Components/MagicShield.h"
#include "ECS/Components/MiracleImpression.h"
#include "ECS/Components/Mist.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/OneOffSpellSeed.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/Scaffold.h"
#include "ECS/Components/ScriptControl.h"
#include "ECS/Components/Spell.h"
#include "ECS/Components/SpellIcon.h"
#include "ECS/Components/StoragePit.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/TownDesire.h"
#include "ECS/Components/TownDesireFlag.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/VillageTotem.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/VillagerDeath.h"
#include "ECS/Components/WallHug.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/CreatureBodyFile.h"
#include "ECS/CreatureCarried.h"
#include "ECS/CreatureExplorationLand.h"
#include "ECS/CreatureHome.h"
#include "ECS/CreatureNaming.h"
#include "ECS/FixedKinds.h"
#include "ECS/LearningMarker.h"
#include "ECS/PlayerTowns.h"
#include "ECS/Registry.h"
#include "ECS/ReportedCamera.h"
#include "ECS/Systems/AnimalSystemInterface.h"
#include "ECS/Systems/CreatureFightSystemInterface.h"
#include "ECS/Systems/CreatureHandSystemInterface.h"
#include "ECS/Systems/CreatureHelpSystemInterface.h"
#include "ECS/Systems/CreatureLocomotionSystemInterface.h"
#include "ECS/Systems/CreatureObjectActionSystemInterface.h"
#include "ECS/Systems/FireSystemInterface.h"
#include "ECS/Systems/FishFarmSystemInterface.h"
#include "ECS/Systems/ForestSystemInterface.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/MagicSystemInterface.h"
#include "ECS/Systems/MusicMoodSystemInterface.h"
#include "ECS/Systems/PlayerProfileSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "ECS/Systems/SkyClockSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "ECS/Systems/WeatherSystemInterface.h"
#include "ECS/TempleConstruction.h"
#include "ECS/TempleHeartOf.h"
#include "ECS/ThingMapPlace.h"
#include "ECS/TownDesire.h"
#include "ECS/WallHugRules.h"
#include "ECS/WorldObjects.h"
#include "Graphics/Moon.h"
#include "Gui/TextDatabase.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/MagicTables.h"
#include "MagicLiving.h"
#include "ObjectMeasures.h"
#include "Resources/ResourcesInterface.h"
#include "Sound/MusicMood.h"

using namespace openblack;
using namespace openblack::ecs::systems;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems::mind_detail;
using creature_desires::Desire;
using creature_mind_model::TreeKind;
using creature_plan_actions::Target;

std::optional<entt::entity> CreatureMindSystem::WholeOfShownThing(entt::entity creature, CreatureMindState& mind,
                                                                  entt::entity thing)
{
	const auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(thing))
	{
		return std::nullopt;
	}
	ComeAcross(creature, mind, thing);
	// A creature is part of no whole thing
	if (registry.AllOf<Creature>(thing))
	{
		return std::nullopt;
	}
	// A temple shown is its heart, a part of the temple
	if (registry.AllOf<Temple>(thing))
	{
		if (const auto heart = ecs::temple_heart::Of(registry, thing); heart != entt::null)
		{
			ComeAcross(creature, mind, heart);
		}
		return thing;
	}
	const auto whole = BelongsTo(thing);
	if (!whole.has_value() || *whole == thing || !registry.Valid(*whole))
	{
		return std::nullopt;
	}
	ComeAcross(creature, mind, *whole);
	return whole;
}

std::optional<entt::entity> CreatureMindSystem::WholeOfForcedPlan(entt::entity creature, CreatureMindState& mind, Desire desire,
                                                                  entt::entity thing)
{
	if (auto whole = WholeOfShownThing(creature, mind, thing))
	{
		return whole;
	}
	// A desire about something, with no whole thing, is about the nearest town
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* tables = GetTables();
	const auto* at = registry.TryGet<const Transform>(creature);
	if (tables == nullptr || at == nullptr || !tables->rules.at(static_cast<size_t>(desire)).aboutSomething)
	{
		return std::nullopt;
	}
	const auto town = mind_detail::NearestTown(registry, at->position);
	if (town == entt::null)
	{
		return std::nullopt;
	}
	ComeAcross(creature, mind, town);
	return town;
}

std::optional<CreatureMindSystem::ChosenActivity>
CreatureMindSystem::ShownActivity(entt::entity creature, CreatureMindState& mind, Desire desire, entt::entity thing)
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* tables = GetTables();
	const auto* at = registry.TryGet<const Transform>(creature);
	const auto whole = WholeOfShownThing(creature, mind, thing);
	const auto* belief = whole.has_value() ? mind.beliefs.Find(*whole) : nullptr;
	const auto kind = belief != nullptr ? creature_activity::WholeOf(belief->kind) : std::nullopt;
	if (tables == nullptr || at == nullptr || !kind.has_value())
	{
		return std::nullopt;
	}
	const auto facts = WholeFactsOf(registry, creature, *whole, *kind, *tables, LookedAtByItsPlayer(creature));
	if (!creature_activity::CanBeAbout(desire, facts, mind.mayAttackOwnPlayer))
	{
		return std::nullopt;
	}
	const auto usefulness = ActivityUsefulness(creature, mind, desire, *whole) *
	                        mind_detail::Nearness(registry, creature, at->position, *whole, belief->place,
	                                              tables->rules.at(static_cast<size_t>(desire)).distanceWeight);
	return ChosenActivity {.whole = *whole, .kind = *kind, .usefulness = usefulness};
}

std::vector<creature_planner::ObjectCandidate> CreatureMindSystem::ShownParts(entt::entity creature, CreatureMindState& mind,
                                                                              Desire desire, entt::entity thing,
                                                                              const ChosenActivity& activity)
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto members = [&] {
		std::vector<uint32_t> found;
		for (const auto& belief : mind.beliefs.PartsOf(activity.whole))
		{
			const auto part = ActedThingOf(registry, belief.thing);
			if (part.has_value() && *part != creature && registry.Valid(*part))
			{
				found.push_back(entt::to_integral(*part));
			}
		}
		return found;
	};
	const auto found = creature_planner::ShownThingsToActOn(entt::to_integral(thing), members, [&](uint32_t id) {
		const auto part = static_cast<entt::entity>(id);
		return SuitsDesire(creature, mind, desire, part) && ObjectToActOnUsefulness(creature, mind, desire, part) > 0.0f;
	});
	std::vector<creature_planner::ObjectCandidate> parts;
	std::ranges::transform(found, std::back_inserter(parts), [&](uint32_t id) {
		return GoalWorth(creature, mind, desire, static_cast<entt::entity>(id), activity);
	});
	return parts;
}

std::vector<creature_planner::ObjectCandidate>
CreatureMindSystem::ShownThingsWithoutWhole(entt::entity creature, CreatureMindState& mind, Desire desire, entt::entity thing)
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto suits = [&](uint32_t id) {
		const auto found = static_cast<entt::entity>(id);
		return registry.Valid(found) && found != creature && SuitsDesire(creature, mind, desire, found) &&
		       ObjectToActOnUsefulness(creature, mind, desire, found) > 0.0f;
	};
	const auto members = [&] {
		std::vector<uint32_t> found;
		if (const auto whole = WholeOfShownThing(creature, mind, thing))
		{
			for (const auto& belief : mind.beliefs.PartsOf(*whole))
			{
				if (const auto part = ActedThingOf(registry, belief.thing))
				{
					found.push_back(entt::to_integral(*part));
				}
			}
		}
		return found;
	};
	const auto found = creature_planner::ShownThingsToActOn(entt::to_integral(thing), members, suits);
	std::vector<creature_planner::ObjectCandidate> parts;
	std::ranges::transform(found, std::back_inserter(parts), [&](uint32_t id) {
		return GoalWorth(creature, mind, desire, static_cast<entt::entity>(id), std::nullopt);
	});
	return parts;
}

std::optional<Desire> CreatureMindSystem::DesireForShownThing(entt::entity creature, CreatureMindState& mind,
                                                              entt::entity thing, std::span<const Desire> tried)
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* tables = GetTables();
	const auto* at = registry.TryGet<const Transform>(creature);
	if (tables == nullptr || at == nullptr || !mind.desires.has_value() || !registry.AllOf<Creature>(creature))
	{
		return std::nullopt;
	}
	// The whole thing that the shown thing is part of, when there is one
	const auto whole = WholeOfShownThing(creature, mind, thing);
	const auto* wholeBelief = whole.has_value() ? mind.beliefs.Find(*whole) : nullptr;
	const auto kind = wholeBelief != nullptr ? creature_activity::WholeOf(wholeBelief->kind) : std::nullopt;
	std::vector<creature_planner::DesireOnThing> aboutWhole;
	std::vector<creature_planner::DesireOnThing> withoutWhole;
	for (size_t d = 0; d < creature_desires::k_DesireCount; ++d)
	{
		const auto desire = static_cast<Desire>(d);
		const auto& state = mind.desires->desires.at(d);
		if (!mind.desires->IsActivated(d) || state.suppressedTurns != 0 || std::ranges::find(tried, desire) != tried.end())
		{
			continue;
		}
		const auto& rule = tables->rules.at(d);
		if (rule.aboutSomething)
		{
			if (!kind.has_value() ||
			    !creature_activity::CanBeAbout(
			        desire, WholeFactsOf(registry, creature, *whole, *kind, *tables, LookedAtByItsPlayer(creature)),
			        mind.mayAttackOwnPlayer))
			{
				continue;
			}
			const auto nearness =
			    mind_detail::Nearness(registry, creature, at->position, *whole, wholeBelief->place, rule.distanceWeight);
			aboutWhole.push_back({.desire = desire,
			                      .strength = state.value,
			                      .usefulness = ActivityUsefulness(creature, mind, desire, *whole) * nearness});
		}
		else if (creature_planner::TestsThingActedOn(desire))
		{
			withoutWhole.push_back({.desire = desire, .strength = state.value});
		}
	}
	return creature_planner::DesireForShownThing(aboutWhole, withoutWhole);
}

std::optional<creature_desires::Desire> CreatureMindSystem::ForcePlanOn(entt::entity creature, entt::entity object,
                                                                        bool stopsWalk)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* mind = registry.Valid(creature) ? registry.TryGet<CreatureMindState>(creature) : nullptr;
	if (mind == nullptr || !mind->desires.has_value() || !mind->learnt.has_value() || !registry.Valid(object))
	{
		return std::nullopt;
	}
	// What it was doing is given up, so that the thing is planned for at once, whatever it was doing
	Abandon(*mind);
	creature_mind::Plan(mind->idle, creature_mind::Activity::None, {});
	ComeAcross(creature, *mind, object);
	// It finds the desire that it most wants for the shown thing, and plans that desire alone. With no plan, it leaves
	// that desire out and tries the next desire.
	std::vector<Desire> tried;
	for (size_t attempt = 0; attempt < creature_planner::k_ShownThingTries; ++attempt)
	{
		const auto desire = DesireForShownThing(creature, *mind, object, tried);
		if (!desire.has_value())
		{
			break;
		}
		mind->leash.actOn = {entt::to_integral(object)};
		mind->plannedTurn = mind->turn + 1;
		// The plan that it makes is one that the player made it make
		_orderedPlanFor = creature;
		if (!stopsWalk)
		{
			_walkContinuesFor = creature;
		}
		PlanCreature(creature, *mind, desire);
		_orderedPlanFor.reset();
		_walkContinuesFor.reset();
		mind->leash.actOn.clear();
		if (mind->planActive && mind->planner.current.has_value() && mind->planner.current->desire == *desire)
		{
			// This plan is a forced plan
			mind->planner.current->priority = std::numeric_limits<float>::max();
			mind->planForced = true;
			SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Creature {} acts on the shown thing {} for desire {} after {} tries",
			                    entt::to_integral(creature), entt::to_integral(object), static_cast<int>(*desire), attempt + 1);
			return desire;
		}
		tried.push_back(*desire);
	}
	return std::nullopt;
}

std::optional<Desire> CreatureMindSystem::DesireForThingInHand(entt::entity creature, CreatureMindState& mind,
                                                               entt::entity thing)
{
	if (!mind.desires.has_value() || !mind.learnt.has_value())
	{
		return std::nullopt;
	}
	// The creature comes across the thing, as when the leash shows it the thing
	ComeAcross(creature, mind, thing);
	// It looks for a plan in a copy of its mind. Thus the plan that it does now does not change.
	auto trial = mind;
	std::vector<Desire> tried;
	for (size_t attempt = 0; attempt < creature_planner::k_ShownThingTries; ++attempt)
	{
		const auto desire = DesireForShownThing(creature, trial, thing, tried);
		if (!desire.has_value())
		{
			break;
		}
		trial.leash.actOn = {entt::to_integral(thing)};
		trial.plannedTurn = trial.turn + 1;
		PlanCreature(creature, trial, desire, false);
		trial.leash.actOn.clear();
		if (trial.planner.best.at(static_cast<size_t>(*desire)).has_value())
		{
			return desire;
		}
		tried.push_back(*desire);
	}
	return std::nullopt;
}

bool CreatureMindSystem::LetGoOfHeld(entt::entity creature, CreatureMindState& mind, creature_mind::LetGo how)
{
	// A let-go that does not start leaves the hands to be checked again on the next turn
	if (!Locator::creatureObjectActionSystem::has_value())
	{
		mind.idle.handsChecked.reset();
		return false;
	}
	auto& hands = Locator::creatureObjectActionSystem::value();
	const auto held = hands.GetHeld(creature);
	if (!held.has_value())
	{
		mind.idle.handsChecked.reset();
		return false;
	}
	const auto& registry = Locator::entitiesRegistry::value();
	bool gently = how == creature_mind::LetGo::PutDown;
	std::optional<Desire> use;
	if (!gently)
	{
		// It does not look for a plan for a creature that it holds
		const bool heldIsCreature = registry.AllOf<Creature>(*held);
		if (!heldIsCreature)
		{
			use = DesireForThingInHand(creature, mind, *held);
		}
		gently = creature_mind::PutsDownGently(heldIsCreature, use);
	}
	const bool started = gently ? hands.PutDown(creature) : hands.Discard(creature);
	if (!started)
	{
		mind.idle.handsChecked.reset();
	}
	SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Creature {} lets go of {} before a step that needs empty hands: {} ({}), {}",
	                    entt::to_integral(creature), entt::to_integral(*held), gently ? "puts it down" : "tosses it away",
	                    use.has_value() ? static_cast<int>(*use) : -1, started ? "started" : "can't");
	return started;
}
