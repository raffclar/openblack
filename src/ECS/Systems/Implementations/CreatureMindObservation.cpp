/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// How a creature learns from what it sees: it copies the deeds of its player, and it learns the skills of villagers
// and the miracles that it sees

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

namespace
{
constexpr float k_TurnSeconds = std::chrono::duration<float>(TimeSystemInterface::k_TurnDuration).count();
constexpr float k_TurnsPerSecond = 1.0f / k_TurnSeconds;

/// Noticing a deed it may copy adds this much to the first source of the desire behind the deed, and this much to its
/// wish to follow the player
constexpr float k_NoticedDeedSource = 0.1f;
constexpr float k_NoticedDeedFollow = 0.3f;
/// Starting to copy a deed on a thing teaches what the thing and its whole thing are worth with this feedback
constexpr float k_DeedSeenLesson = 0.5f;

/// A creature watches skills, miracles and the player's deeds this far away at most
constexpr float k_ViewDistance = 150.0f;
/// Copying the player's desire makes that desire dominant for this many seconds
constexpr float k_MimicDesireSeconds = 120.0f;
/// A creature that copies the player's desire stops when it thought this many times with no plan since its last plan
constexpr uint32_t k_MostThinksWithoutPlanToCopyDesire = 40;
/// After that, it does not follow the player for this many seconds
constexpr float k_NoFollowAfterCopySeconds = 40.0f;

/// A creature that sees a villager do a skill it does not know follows the villager to watch it
constexpr std::string_view k_FollowToWatchAction = "ExamineByFollowing";

/// How pressing the reaction a creature holds is, by the game's reaction table; 0 when it holds none
float ReactionPriorityOf(const ecs::Registry& registry, entt::entity creature)
{
	const auto* reaction = registry.TryGet<const LivingReaction>(creature);
	if (reaction == nullptr || reaction->type == Reaction::None || !Locator::infoConstants::has_value())
	{
		return 0.0f;
	}
	const auto& table = Locator::infoConstants::value().reaction;
	const auto row = static_cast<size_t>(reaction->type);
	return row < table.size() ? static_cast<float>(table.at(row).priority) : 0.0f;
}

/// The player of a thing that can belong to a player: a town, a villager or a building by its town, a creature or a
/// temple. A villager or a building has the player of the town that it belongs to. None for any other thing, or for a
/// villager or a building with no town.
std::optional<PlayerNames> PlayerOfThing(const ecs::Registry& registry, entt::entity thing,
                                         std::optional<entt::entity> belongsTo)
{
	if (!registry.Valid(thing))
	{
		return std::nullopt;
	}
	if (const auto* creature = registry.TryGet<const Creature>(thing))
	{
		return creature->owner;
	}
	if (const auto* temple = registry.TryGet<const Temple>(thing))
	{
		return temple->owner;
	}
	if (const auto* town = registry.TryGet<const Town>(thing))
	{
		return town->owner;
	}
	if (const auto owner = belongsTo; owner.has_value() && *owner != thing)
	{
		if (const auto* town = registry.TryGet<const Town>(*owner))
		{
			return town->owner;
		}
	}
	return std::nullopt;
}

/// The kind of a thing for copying the player
creature_watching::MimicKind MimicKindOf(const ecs::Registry& registry, entt::entity thing)
{
	using Class = creature_watching::MimicFacts::Class;
	creature_watching::MimicFacts facts;
	if (const auto* villager = registry.TryGet<const Villager>(thing))
	{
		facts.kind = Class::Villager;
		facts.child = villager->lifeStage == Villager::LifeStage::Child;
		facts.female = villager->sex == Villager::Sex::FEMALE;
	}
	else if (registry.AllOf<Animal>(thing))
	{
		const auto* info = ecs::world_objects::InfoOf(thing);
		facts.kind = Class::Animal;
		facts.nasty = info != nullptr && info->alignmentType == AlignmentType::AnimalNasty;
	}
	else if (registry.AllOf<Creature>(thing))
	{
		facts.kind = Class::Creature;
	}
	else if (registry.AnyOf<WorshipSite, WorshipAltar, VillageTotem, FishFarm, Temple, Town>(thing))
	{
		facts.kind = Class::Other;
	}
	else if (registry.AnyOf<Abode, Field>(thing))
	{
		facts.kind = Class::Building;
	}
	else if (registry.AllOf<Tree>(thing))
	{
		facts.kind = Class::Tree;
	}
	else if (registry.AnyOf<Rock, DeadTree>(thing))
	{
		facts.kind = Class::Rock;
	}
	else if (registry.AnyOf<MobileStatic, MobileObject, Ball, Pot>(thing))
	{
		facts.kind = Class::LooseObject;
	}
	return creature_watching::KindToCopy(facts);
}
} // namespace

void CreatureMindSystem::CopyPlayersDeed(entt::entity creature, CreatureMindState& mind)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* tables = GetTables();
	if (tables == nullptr || !mind.learnt.has_value() || !mind.learnt->mimicry.has_value() ||
	    !registry.AllOf<Transform>(creature))
	{
		return;
	}
	auto& learnt = *mind.learnt;
	const auto& rule = tables->mimics.at(learnt.mimicry->rule);
	// Every copy is for the desire the player seemed to have
	if (rule.desire >= creature_desires::k_DesireCount)
	{
		return;
	}
	const auto desire = static_cast<Desire>(rule.desire);
	std::optional<entt::entity> players;
	if (learnt.mimicry->object.has_value())
	{
		const auto thing = static_cast<entt::entity>(*learnt.mimicry->object);
		if (registry.Valid(thing))
		{
			ComeAcross(creature, mind, thing);
			if (mind.beliefs.Find(thing) != nullptr)
			{
				// The player still holds the thing in the hand: it stops copying
				if (registry.AllOf<InHand>(thing))
				{
					learnt.mimicry.reset();
					return;
				}
				players = thing;
			}
		}
	}

	// It does what the player did, in three passes: to the player's thing, to the nearest thing of the same kind, and to
	// the thing it finds most useful to act on. It may do it each time it can; after each time, it stops by random(2).
	enum class Pass : uint8_t
	{
		PlayersThing,
		SameKind,
		MostUseful,
	};
	const auto* at = registry.TryGet<const Transform>(creature);
	const auto distanceWeight = tables->rules.at(static_cast<size_t>(desire)).distanceWeight;
	const auto choose = [&](Pass pass, Target target) -> std::optional<entt::entity> {
		const auto suits = [&](entt::entity thing) { return Accepts(registry, target, thing, creature); };
		switch (pass)
		{
		case Pass::PlayersThing:
			return players.has_value() && suits(*players) ? players : std::nullopt;
		case Pass::SameKind:
		{
			if (!mind.copiedKind.has_value())
			{
				return std::nullopt;
			}
			std::vector<entt::entity> things;
			std::vector<creature_watching::MimicCandidate> candidates;
			for (const auto& belief : mind.beliefs.InListOrder())
			{
				const auto thing = ActedThingOf(registry, belief.thing);
				if (!thing.has_value() || *thing == creature || !SuitsDesire(creature, mind, desire, *thing) || !suits(*thing))
				{
					continue;
				}
				things.push_back(*thing);
				candidates.push_back({
				    .kind = MimicKindOf(registry, *thing),
				    .nearness = mind_detail::Nearness(registry, creature, at->position, *thing, belief.place, distanceWeight),
				});
			}
			const auto nearest = creature_watching::NearestOfKind(candidates, *mind.copiedKind);
			return nearest.has_value() ? std::optional(things.at(*nearest)) : std::nullopt;
		}
		case Pass::MostUseful:
		{
			std::optional<creature_planner::ObjectCandidate> best;
			for (const auto& thing : BelievedThingsToActOn(creature, mind, desire, suits))
			{
				if (thing.usefulness > (best.has_value() ? best->usefulness : 0.0f))
				{
					best = thing;
				}
			}
			return best.has_value() ? std::optional(static_cast<entt::entity>(best->id)) : std::nullopt;
		}
		}
		return std::nullopt;
	};
	const auto copyActions =
	    creature_watching::CopyActionsOf(rule, learnt.mimicry->rule, learnt.mimicry->magic, tables->miracles);
	for (const auto pass : {Pass::PlayersThing, Pass::SameKind, Pass::MostUseful})
	{
		for (const auto action : copyActions)
		{
			const auto* executor = creature_plan_actions::For(tables->actions.at(action).name);
			// The creature copies a miracle only when it may cast that miracle now, as in its own plans
			if (executor == nullptr ||
			    (creature_plan_actions::IsCast(*executor) &&
			     !MayCast(creature, mind, action, IsPowerUpCast(tables->actions.at(action).name))) ||
			    !creature_watching::KnowsWhatItNeeds(learnt.knowledge, tables->actions.at(action).needs) ||
			    LeftAreaFor(registry, creature, mind, action))
			{
				continue;
			}
			// An action that needs no thing acts on the creature itself
			std::optional<entt::entity> object;
			if (executor->target != Target::None)
			{
				object = choose(pass, executor->target);
				if (!object.has_value())
				{
					continue;
				}
			}
			const auto whole = WholeOfForcedPlan(creature, mind, desire, object.value_or(creature));
			creature_plan_actions::Situation situation;
			if (!Adopt(creature, mind,
			           {.desire = desire,
			            .action = action,
			            .object = object.has_value() ? std::optional(entt::to_integral(*object)) : std::nullopt,
			            .about = whole.has_value() ? std::optional(entt::to_integral(*whole)) : std::nullopt,
			            // Copying counts as pressing as a desire at its strongest, with nothing learnt
			            .priority = creature_planner::Priority(1.0f, std::nullopt,
			                                                   creature_planner::ActionPriority(learnt.opinions.at(action)),
			                                                   creature_planner::k_DefaultUsefulness, std::nullopt)},
			           situation, true))
			{
				continue;
			}
			creature_mind_model::Think(learnt, fmt::format("I'm copying you: {}", tables->actions.at(action).name));
			SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Creature {} copies the player in pass {}: {} on {}",
			                    entt::to_integral(creature), static_cast<int>(pass), tables->actions.at(action).name,
			                    object.has_value() ? static_cast<int64_t>(entt::to_integral(*object)) : -1);
			if (Random(2) == 0)
			{
				return;
			}
		}
	}
}

bool CreatureMindSystem::ContinueKeptPlanWith(entt::entity creature, CreatureMindState& mind, entt::entity gift)
{
	const auto* tables = GetTables();
	const auto& kept = mind.keptPlan;
	if (tables == nullptr || !kept.has_value() || kept->action >= tables->actions.size())
	{
		return false;
	}
	const auto& action = tables->actions.at(kept->action);
	const auto* executor = creature_plan_actions::For(action.name);
	// Only the actions that a creature can do here can continue
	if (executor == nullptr)
	{
		return false;
	}
	const auto& registry = Locator::entitiesRegistry::value();
	const bool holdsGift = ecs::creature_carried::Carries(registry, creature, gift);
	const auto whole = kept->about.has_value() ? kept->about : kept->activityObject;
	const bool needsThingToUse = creature_plan_actions::NeedsThingToUse(action.name);
	const auto use = executor->use != Target::None ? executor->use : Target::Pickable;
	const creature_giving::KeptPlanFacts facts {
	    .complete = kept->action != 0 && kept->object.has_value(),
	    .missesWhole = tables->rules.at(static_cast<size_t>(kept->desire)).aboutSomething && !whole.has_value(),
	    .missesThingToUse = needsThingToUse && !kept->instrument.has_value(),
	    .actsOnThing = action.actsOnThing,
	    .needsThingToUse = needsThingToUse,
	    .giftSuitsActOn = !(action.notOnWhatItHolds && holdsGift) &&
	                      (executor->target == Target::None || Accepts(registry, executor->target, gift, creature)),
	    .giftSuitsUse =
	        !NoWayLately(registry, mind, gift) &&
	        !BelievedPlaceOf(registry, mind, gift)
	             .transform([&](const map_coords::MapCoords& place) { return OutsideConfinedArea(registry, creature, place); })
	             .value_or(false) &&
	        LetsCreatureAct(registry, gift) && Accepts(registry, use, gift, creature),
	};
	const auto chosen = creature_giving::UseForKeptPlan(facts);
	if (chosen == creature_giving::KeptPlanUse::None)
	{
		return false;
	}
	const auto toEntity = [](std::optional<uint32_t> number) {
		return number.has_value() ? std::optional(static_cast<entt::entity>(*number)) : std::nullopt;
	};
	SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Creature {} continues {} with the thing it was given ({}) as the thing {}",
	                    entt::to_integral(creature), action.name, entt::to_integral(gift),
	                    chosen == creature_giving::KeptPlanUse::ActOn ? "to act on" : "to use");
	const bool actsOnGift = chosen == creature_giving::KeptPlanUse::ActOn;
	return ForceActivity(creature, {
	                                   .desire = kept->desire,
	                                   .activityObject = toEntity(whole),
	                                   .action = action.name,
	                                   .actionObject = actsOnGift ? gift : static_cast<entt::entity>(*kept->object),
	                                   .instrument = actsOnGift ? toEntity(kept->instrument) : std::optional(gift),
	                                   .ordered = true,
	                                   .fightBodyEndsAgain = true,
	                               });
}

void CreatureMindSystem::ProcessMimicry(entt::entity creature, CreatureMindState& mind)
{
	const auto* tables = GetTables();
	if (tables == nullptr || !mind.learnt.has_value() || !mind.learnt->mimicry.has_value() || !mind.desires.has_value())
	{
		return;
	}
	auto& mimicry = mind.learnt->mimicry;
	if (!creature_watching::StageContinues(*mimicry, mind.turn, k_TurnsPerSecond))
	{
		creature_watching::NextMimicStage(mimicry, tables->mimics, mind.turn, [this](uint32_t range) { return Random(range); });
		if (mimicry.has_value())
		{
			const auto& next = tables->mimics.at(mimicry->rule);
			SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Creature {} copies the player's {}: stage {}",
			                    entt::to_integral(creature), next.name, creature_watching::Name(mimicry->stage));
			if (mimicry->stage == creature_watching::MimicStage::CopyDesire && next.desire < tables->texts.size())
			{
				creature_mind_model::Think(mind.learnt.value(),
				                           fmt::format("I want what you wanted: {}", tables->texts.at(next.desire).desire));
			}
		}
		return;
	}
	const auto& rule = tables->mimics.at(mimicry->rule);
	auto& registry = Locator::entitiesRegistry::value();
	switch (mimicry->stage)
	{
	case creature_watching::MimicStage::Notice:
	{
		// The creature shows the player that it saw the deed, if the deed had a thing
		const auto object = mimicry->object.has_value() ? static_cast<entt::entity>(*mimicry->object) : entt::null;
		if (registry.Valid(object) && rule.noticeAction < tables->actions.size())
		{
			ComeAcross(creature, mind, object);
			ForceActivity(creature, {.desire = Desire::Curiosity,
			                         .action = tables->actions.at(rule.noticeAction).name,
			                         .actionObject = object,
			                         .ordered = true,
			                         .forced = false,
			                         .fightBodyEndsAgain = true});
		}
		break;
	}
	case creature_watching::MimicStage::CopyAction:
		CopyPlayersDeed(creature, mind);
		break;
	case creature_watching::MimicStage::CopyDesire:
	{
		if (rule.desire >= creature_desires::k_DesireCount)
		{
			break;
		}
		// It comes to want what the player seemed to want. Its hunger and tiredness stay as they were.
		auto& desires = *mind.desires;
		const auto hunger = desires[Desire::Hunger].value;
		const auto tiredness = desires[Desire::Tiredness].value;
		auto* spells = registry.TryGet<CreatureSpells>(creature);
		if (spells == nullptr)
		{
			spells = &registry.Assign<CreatureSpells>(creature);
		}
		spells->cheat = creature_spell_mind::SetCheatDominant(
		    desires, static_cast<Desire>(rule.desire),
		    {.all = false, .seconds = k_MimicDesireSeconds, .floor = mind_detail::DesireFloorOf(creature)}, k_TurnsPerSecond);
		mind_detail::TakeWrittenSources(creature, mind);
		desires[Desire::FollowPlayerDesire].suppressedTurns = 0;
		// A creature that stood with nothing to do for too long stops copying, and does not follow the player for a time
		if (mind.thinksWithoutPlan > k_MostThinksWithoutPlanToCopyDesire)
		{
			mimicry.reset();
			creature_desires::Suppress(desires, Desire::FollowPlayerDesire, k_NoFollowAfterCopySeconds, k_TurnsPerSecond);
		}
		desires[Desire::Hunger].value = std::clamp(hunger, 0.0f, 1.0f);
		desires[Desire::Tiredness].value = std::clamp(tiredness, 0.0f, 1.0f);
		break;
	}
	}
}

void CreatureMindSystem::SeeSkill(const glm::vec3& point, size_t skill)
{
	auto& registry = Locator::entitiesRegistry::value();
	std::vector<entt::entity> watching;
	registry.Each<const Creature, const CreatureMindState, const Transform>(
	    [&](entt::entity entity, const Creature&, const CreatureMindState& mind, const Transform& at) {
		    if (mind.learnt.has_value() && glm::distance(at.position, point) <= k_ViewDistance)
		    {
			    watching.push_back(entity);
		    }
	    });
	for (const auto entity : watching)
	{
		WatchSkill(entity, skill);
	}
}

namespace
{
/// The game's turn now
uint32_t GameTurn()
{
	return Locator::time::has_value() ? static_cast<uint32_t>(Locator::time::value().GetTurn()) : 0;
}
} // namespace

glm::vec3 mind_detail::HeadPoint(const ecs::Registry& registry, entt::entity creature)
{
	const auto& transform = registry.Get<const Transform>(creature);
	const auto& body = registry.Get<const Creature>(creature);
	const auto fallback = transform.position + glm::vec3(0.0f, ShownSize(body) * creature_mode::k_HeightPerSize, 0.0f);
	const auto* animation = registry.TryGet<const CreatureAnimation>(creature);
	const auto& rigs = Locator::resources::value().GetCreatureRigs();
	const auto rigId = creature::GetRigId(body.species);
	if (animation == nullptr || animation->boneMatrices.empty() || !rigs.Contains(rigId))
	{
		return fallback;
	}
	const auto& points = rigs.Handle(rigId)->actionPoints;
	if (!points.has_value() || points->head >= animation->boneMatrices.size())
	{
		return fallback;
	}
	const auto pose = creature::TurnPose(animation->newerTurnPose, animation->boneMatrices,
	                                     creature::PlacementMatrix(transform.position, transform.rotation, transform.scale));
	return glm::vec3(creature::PosedBone(points->head, pose.bones, pose.placement)[3]);
}

void CreatureMindSystem::WatchSkill(entt::entity creature, size_t skill)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* tables = GetTables();
	auto* mind = registry.Valid(creature) ? registry.TryGet<CreatureMindState>(creature) : nullptr;
	// A mind stilled, as by the freeze spell, learns nothing
	if (tables == nullptr || mind == nullptr || !mind->learnt.has_value() || mind->paused)
	{
		return;
	}
	// The clock of a skill runs on the game's turns
	const auto turn = GameTurn();
	const auto progress = creature_watching::SeeSkill(mind->learnt->knowledge, skill, tables->skills, mind->developmentPhase,
	                                                  turn, k_TurnsPerSecond);
	SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Creature {} sees skill {} (turn {}): event {}, meter {}",
	                    entt::to_integral(creature), skill, turn,
	                    progress.event.has_value() ? static_cast<int>(*progress.event) : -1, progress.meter.value_or(-1.0f));
	if (progress.event == creature_watching::LearningEvent::Learnt)
	{
		creature_mind_model::Think(*mind->learnt, fmt::format("I've learnt to {}", tables->skills.at(skill).name));
	}
	ShowLearningMeter(creature, *mind, progress.meter);
	// The advisor tells of it: too young to learn it, learnt it now, or still learning it
	if (progress.event.has_value())
	{
		using creature_help::Kind;
		using creature_watching::LearningEvent;
		const auto kind = *progress.event == LearningEvent::TooYoung       ? Kind::CantLearnNormalActionYet
		                  : *progress.event == LearningEvent::NearlyLearnt ? Kind::NearlyLearntNormalAction
		                                                                   : Kind::LessonNormalAction;
		TellHelp(creature, {.kind = kind, .parameter = static_cast<uint32_t>(skill)});
	}
}

void CreatureMindSystem::WatchVillager(entt::entity creature, CreatureMindState& mind, entt::entity villager)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* person = registry.TryGet<const Villager>(villager);
	if (person == nullptr)
	{
		return;
	}
	const creature_watching::Watcher watcher {
	    .phase = mind.developmentPhase,
	    .scriptControlled = registry.AllOf<ScriptControlled>(creature),
	    .planDesire = mind.planner.current.has_value() ? std::optional(mind.planner.current->desire) : std::nullopt,
	};
	if (!creature_watching::WatchesVillagers(watcher))
	{
		return;
	}
	WatchVillagerSkill(creature, mind, villager);
	// A villager of another player's village, or of a village of no player, makes it want to impress more
	const auto* town = registry.Valid(person->town) ? registry.TryGet<const Town>(person->town) : nullptr;
	const auto* body = registry.TryGet<const Creature>(creature);
	if (town != nullptr && (town->owner == PlayerNames::NEUTRAL || body == nullptr || town->owner != body->owner))
	{
		mind.impressFromSeeing = creature_desire_sources::ImpressAfterSighting(mind.impressFromSeeing);
	}
}

void CreatureMindSystem::WatchVillagerSkill(entt::entity creature, CreatureMindState& mind, entt::entity villager)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* action = registry.TryGet<const LivingAction>(villager);
	if (!mind.learnt.has_value() || !Locator::infoConstants::has_value() || action == nullptr)
	{
		return;
	}
	// How far a walking villager still has to go: the table distance between the two map positions
	float toGoal = 0.0f;
	if (const auto* walk = registry.TryGet<const WallHug>(villager); walk != nullptr)
	{
		toGoal = gutils::GetDistanceInMetres(glm::vec2(glm::xz(registry.Get<const Transform>(villager).position)),
		                                     wall_hug::ToPoint(walk->goalPlace));
	}
	const auto state = creature_watching::StateToLearnFrom({
	    .state = action->states.at(static_cast<size_t>(LivingAction::Index::Top)),
	    .finalState = action->states.at(static_cast<size_t>(LivingAction::Index::Final)),
	    .distanceToGoal = toGoal,
	});
	const auto& table = Locator::infoConstants::value().villagerStateTable;
	if (state >= table.size())
	{
		return;
	}
	const auto skill = table.at(state).teachesSkill;
	const auto& known = mind.learnt->knowledge.skillsKnown;
	if (skill >= known.size() || known.at(skill))
	{
		return;
	}
	// It takes in what the villager does, and goes after it to see more
	WatchSkill(creature, skill);
	ForceActivity(creature, {.desire = creature_desires::Desire::Curiosity,
	                         .action = k_FollowToWatchAction,
	                         .actionObject = villager,
	                         .forced = false,
	                         .fightBodyEndsAgain = true});
}

void CreatureMindSystem::ShowLearningMeter(entt::entity creature, CreatureMindState& mind, std::optional<float> share)
{
	const auto turn = GameTurn();
	if (!share.has_value() || !creature_watching::MeterShows(mind.lastMeterTurn, turn))
	{
		return;
	}
	mind.lastMeterTurn = turn;
	auto& registry = Locator::entitiesRegistry::value();
	const auto* body = registry.TryGet<const Creature>(creature);
	const auto* transform = registry.TryGet<const Transform>(creature);
	if (body == nullptr || transform == nullptr || !Locator::resources::has_value())
	{
		return;
	}
	const auto marker = registry.Create();
	registry.Assign<LearningMarker>(marker, ecs::learning_marker::Start(HeadPoint(registry, creature),
	                                                                    ShownSize(*body) * creature_mode::k_HeightPerSize,
	                                                                    *share, turn));
}

void CreatureMindSystem::SeeMiracle(const glm::vec3& point, size_t miracle)
{
	auto& registry = Locator::entitiesRegistry::value();
	registry.Each<const Creature, CreatureMindState, const Transform>(
	    [&](entt::entity entity, const Creature& creature, CreatureMindState& mind, const Transform& at) {
		    if (glm::distance(at.position, point) <= k_ViewDistance)
		    {
			    CountMiracleSighting(entity, creature, mind, miracle);
		    }
	    });
}

void CreatureMindSystem::CountMiracleSighting(entt::entity entity, const Creature& creature, CreatureMindState& mind,
                                              size_t miracle)
{
	const auto* tables = GetTables();
	// A mind stilled, as by the freeze spell, learns nothing
	if (tables == nullptr || !mind.learnt.has_value() || mind.paused)
	{
		return;
	}
	const auto progress = creature_watching::SeeMiracle(
	    mind.learnt->knowledge, miracle, tables->miracles, mind.developmentPhase, mind.turn, mind.leash.miracleSightingWeight,
	    creature_mind_tables::MiracleMultiplier(creature::InfoRow(creature.species)));
	if (progress.learnt && !progress.ignored)
	{
		creature_mind_model::Think(*mind.learnt, fmt::format("I've learnt the miracle {}", tables->miracles.at(miracle).name));
	}
	ShowLearningMeter(entity, mind, progress.meter);
	// The advisor tells of what the sighting showed
	if (progress.event.has_value())
	{
		using creature_help::Kind;
		using creature_watching::LearningEvent;
		const auto kind = *progress.event == LearningEvent::TooYoung       ? Kind::CantLearnMagicActionYet
		                  : *progress.event == LearningEvent::NearlyLearnt ? Kind::NearlyLearntMagicAction
		                                                                   : Kind::LessonMagicAction;
		TellHelp(entity, {.kind = kind, .parameter = static_cast<uint32_t>(miracle)});
	}
}

void CreatureMindSystem::PlayerDid(size_t deed, const glm::vec3& point, std::optional<entt::entity> object,
                                   std::optional<PlayerNames> player)
{
	NoticeDeed(deed, point, object, player, 0);
}

void CreatureMindSystem::PlayerCastDeed(size_t deed, const glm::vec3& point, std::optional<entt::entity> object,
                                        std::optional<PlayerNames> player, MagicType magic)
{
	NoticeDeed(deed, point, object, player, static_cast<uint32_t>(magic));
}

void CreatureMindSystem::NoticeDeed(size_t deed, const glm::vec3& point, std::optional<entt::entity> object,
                                    std::optional<PlayerNames> player, uint32_t magic)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* tables = GetTables();
	if (tables == nullptr || deed >= tables->mimics.size())
	{
		return;
	}
	std::vector<entt::entity> mayCopy;
	registry.Each<const Creature, CreatureMindState, const Transform>([&](entt::entity entity, const Creature& creature,
	                                                                      CreatureMindState& mind, const Transform& at) {
		if (!mind.learnt.has_value() || (player.has_value() && (creature.owner != *player || !creature.leashable)))
		{
			return;
		}
		// It must see where the thing is, or the point when there is no thing, and be near enough to it
		const glm::vec2 here(at.position.x, at.position.z);
		const auto seen =
		    object.has_value() ? PointOf(registry, *object).value_or(glm::vec2(point.x, point.z)) : glm::vec2(point.x, point.z);
		const auto* animation = registry.TryGet<const CreatureAnimation>(entity);
		const auto from = map_coords::FromMetres(here);
		const auto to = map_coords::FromMetres(seen);
		const auto looking = LookingAngle(at, animation != nullptr ? animation->lookAt : std::nullopt);
		const bool inView = creature_object_beliefs::CanSee(looking, gutils::GetAngleFromXZ(from, to),
		                                                    map_coords::Cell(from) == map_coords::Cell(to));
		const bool nearEnough = creature_watching::WithinCopyReach(gutils::GetDistanceInMetres(to, from),
		                                                           creature_object_beliefs::SweepCells(ShownSize(creature)));
		const auto* leashes = registry.TryGet<const CreatureLeash>(entity);
		const creature_watching::MimicConditions conditions {
		    .phase = mind.developmentPhase,
		    .learningLeashInHand = mind.leash.learningInHand,
		    .reactionPriority = ReactionPriorityOf(registry, entity),
		    .canSee = inView && nearEnough,
		    .heldByHand = Locator::creatureHandSystem::has_value() && Locator::creatureHandSystem::value().IsLocked(entity),
		    .leashWorks = leashes == nullptr || !leashes->worn.has_value() || leashes->worn->works,
		    .humanPlayer =
		        !Locator::playerSystem::has_value() || !Locator::playerSystem::value().IsComputerPlayer(creature.owner),
		};
		if (creature_watching::MayStartMimicry(mind.learnt->mimicry, deed, tables->mimics, conditions))
		{
			mayCopy.push_back(entity);
		}
	});
	const auto target = object.has_value() ? std::optional(entt::to_integral(*object)) : std::nullopt;
	const auto random = [this](uint32_t range) { return Random(range); };
	for (const auto entity : mayCopy)
	{
		auto* mind = registry.TryGet<CreatureMindState>(entity);
		const auto* creature = registry.TryGet<const Creature>(entity);
		if (mind == nullptr || creature == nullptr || !mind->learnt.has_value())
		{
			continue;
		}
		// A deed done with a miracle that the creature has not seen often enough counts as a sighting of the miracle. With
		// more than one sighting still needed it stops there; with one, it goes on to copy one time in two.
		if (magic != 0 && magic < tables->miracles.size() && magic < mind->learnt->knowledge.miraclesSeen.size())
		{
			const auto step = creature_watching::CastDeedGate(
			    mind->learnt->knowledge.miraclesSeen.at(magic).count, tables->miracles.at(magic).timesToSee,
			    creature_mind_tables::MiracleMultiplier(creature::InfoRow(creature->species)));
			if (step != creature_watching::CastDeedStep::Copy)
			{
				CountMiracleSighting(entity, *creature, *mind, magic);
				if (step == creature_watching::CastDeedStep::CountAndStop || Random(2) == 0)
				{
					continue;
				}
			}
		}
		if (object.has_value() && registry.Valid(*object))
		{
			LearnFromDeedSeen(entity, *mind, deed, *object, magic);
		}
		creature_watching::BeginMimicry(mind->learnt->mimicry, deed, tables->mimics, target, mind->turn, random);
		mind->learnt->mimicry->magic = magic;
		mind->observedDeed = creature_watching::ObservedDeed {.rule = deed, .object = target, .magic = magic};
		// It forgets the trees that it planted others by
		mind->plantedBy.clear();
		// When the player is angry with a thing of the player, the creature that copies the player can be angry with the
		// things of its player from now on
		if (object.has_value() && tables->mimics.at(deed).desire == static_cast<uint32_t>(Desire::Anger) &&
		    PlayerOfThing(registry, *object, BelongsTo(*object)) == creature->owner)
		{
			mind->mayAttackOwnPlayer = true;
		}
		if (const auto copied = object.has_value() ? PointOf(registry, *object) : std::nullopt)
		{
			mind->copiedAt = *copied;
		}
		if (object.has_value() && registry.Valid(*object))
		{
			mind->copiedKind = MimicKindOf(registry, *object);
		}
		// Seeing the deed stirs the first source of the desire behind it, and its wish to follow the player
		const auto desire = tables->mimics.at(deed).desire;
		if (mind->desires.has_value() && desire < creature_desires::k_DesireCount)
		{
			const auto& sources = (*mind->desires)[static_cast<Desire>(desire)].sources;
			if (!sources.empty())
			{
				creature_desires::ChangeSource(*mind->desires, sources.front().type, k_NoticedDeedSource);
			}
			creature_desires::ChangeSource(*mind->desires, k_FollowPlayerSource, k_NoticedDeedFollow);
		}
		// It knows of the miracle of the deed from now on, and the advisor tells of it when this is new
		auto& known = mind->learnt->knowledge.miraclesKnown;
		if (magic != 0 && magic < known.size() && !known.at(magic))
		{
			known.at(magic) = true;
			TellHelp(entity, {.kind = creature_help::Kind::LessonMagicAction, .parameter = magic});
		}
		// What it was doing stops as a failure. With no plan, it then notices the deed (see ProcessMimicry).
		{
			creature_mind_model::Think(*mind->learnt, fmt::format("I saw you: {}", tables->mimics.at(deed).name));
			// The failure stops a fight that the creature is in, for both creatures. After a fight that is over, the body
			// ends its fight again and the life pays again.
			if (registry.AllOf<CreatureFighting>(entity) && Locator::creatureFightSystem::has_value())
			{
				Locator::creatureFightSystem::value().CallOffForPlan(entity);
			}
			if (Locator::creatureObjectActionSystem::has_value() &&
			    Locator::creatureObjectActionSystem::value().GetState(entity) ==
			        CreatureObjectActionSystemInterface::State::Busy)
			{
				Locator::creatureObjectActionSystem::value().StopForPlan(entity, true);
			}
			Abandon(entity, *mind);
			if (Locator::magicSystem::has_value())
			{
				Locator::magicSystem::value().ReleaseCreatureCast(entity);
			}
			creature_mind::Plan(mind->idle, creature_mind::Activity::None, {});
		}
	}
}

void CreatureMindSystem::DoLessonAgain(entt::entity creature, CreatureMindState& mind)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* tables = GetTables();
	const auto* transform = registry.TryGet<const Transform>(creature);
	if (tables == nullptr || transform == nullptr || !mind.learnt.has_value() || !mind.desires.has_value())
	{
		return;
	}
	const auto& learnt = *mind.learnt;
	const auto index = creature_learning::BestContext(learnt.contexts);
	if (!index.has_value())
	{
		return;
	}
	const auto context = learnt.contexts.at(*index);
	SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Creature {} looks to do its lesson again: action {} on {}",
	                    entt::to_integral(creature), context.action, context.object.value_or(0));
	// It needs a thing that it acted on that is not itself. The thing can be gone, as a thing that it ate is: the kind of
	// thing stays known. It does nothing while it copies the player.
	if (!creature_follow_up::MayDoAgain(context.object, entt::to_integral(creature), learnt.mimicry.has_value()) ||
	    context.action >= tables->actions.size())
	{
		return;
	}
	const auto actedOn = static_cast<entt::entity>(*context.object);
	const auto action = creature_follow_up::ActionToDoAgain(tables->actions.at(context.action).name);
	const auto actionIndex = creature_mind_tables::FindAction(*tables, action);
	const auto* executor = creature_plan_actions::For(action);
	if (!actionIndex.has_value() || executor == nullptr)
	{
		return;
	}
	const glm::vec2 position {transform->position.x, transform->position.z};
	creature_plan_actions::Situation situation;
	Situate(creature, situation);
	const auto desire = context.desire;
	// It finds the most useful thing of the same kind as the thing it acted on, as the planner finds a thing for an
	// action. An action that acts on nothing acts on the creature itself. An action that acts on a thing but has no test
	// of its own takes any thing of that kind.
	const bool actsOnThing = tables->actions.at(*actionIndex).actsOnThing;
	std::vector<creature_planner::ObjectCandidate> sameKind;
	if (actsOnThing)
	{
		const auto* known = mind.beliefs.Find(actedOn);
		if (known == nullptr && !context.belief.has_value())
		{
			return;
		}
		const auto kind = known != nullptr ? known->kind : context.belief->type;
		sameKind = BelievedThingsToActOn(creature, mind, desire, [&](entt::entity candidate) {
			const auto* belief = mind.beliefs.Find(candidate);
			return creature_follow_up::SuitsDoAgain({
			    .sameKind = belief != nullptr && belief->kind == kind,
			    .passesTest = executor->target == Target::None
			                      ? std::nullopt
			                      : std::optional(Accepts(registry, executor->target, candidate, creature)),
			    .notWhatItHolds = executor->notWhatItHolds,
			    .heldByItself = ecs::creature_carried::Carries(registry, creature, candidate),
			});
		});
	}
	const auto chosen = creature_follow_up::ThingToDoAgainTo(actsOnThing, entt::to_integral(creature), sameKind);
	if (!chosen.has_value())
	{
		return;
	}
	const auto object = static_cast<entt::entity>(*chosen);
	// It must be able to do the action now and know what the action needs. It does not do an action again that the area
	// that holds it refused.
	if (!creature_plan_actions::Allowed(*executor, situation) ||
	    !creature_watching::KnowsWhatItNeeds(learnt.knowledge, tables->actions.at(*actionIndex).needs) ||
	    LeftAreaFor(registry, creature, mind, static_cast<uint32_t>(*actionIndex)))
	{
		return;
	}
	const auto whole = WholeOfForcedPlan(creature, mind, desire, object);
	std::optional<entt::entity> instrument;
	if (executor->use != Target::None)
	{
		const auto used = UsedFor(creature, executor->use, position,
		                          tables->rules.at(static_cast<size_t>(desire)).distanceWeight, entt::to_integral(object));
		if (!used.has_value())
		{
			return;
		}
		instrument = static_cast<entt::entity>(used->id);
	}
	SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Creature {} does its action again: {} on {}", entt::to_integral(creature), action,
	                    entt::to_integral(object));
	static_cast<void>(ForcePlan(creature, {.desire = desire,
	                                       .action = action,
	                                       .object = object,
	                                       .instrument = instrument,
	                                       .activityObject = whole,
	                                       .ordered = true,
	                                       .fightBodyEndsAgain = true}));
}

void CreatureMindSystem::DanceAfterMusic(entt::entity creature, CreatureMindState& mind)
{
	constexpr std::string_view k_DanceWithVillagers = "DanceWithVillagers";
	constexpr uint32_t k_VillagerDanceLots = 8;
	auto& registry = Locator::entitiesRegistry::value();
	const auto* tables = GetTables();
	// After a track that it likes very much, one time in eight it looks for villagers to impress with a dance: the most
	// useful thing that it believes in for the dance
	if (tables != nullptr && creature_music::MayDanceWithVillagers(mind.followUpValue) && Random(k_VillagerDanceLots) == 0)
	{
		const auto action = creature_mind_tables::FindAction(*tables, k_DanceWithVillagers);
		const auto* executor = creature_plan_actions::For(k_DanceWithVillagers);
		if (action.has_value() && executor != nullptr)
		{
			const bool actsOnThing = tables->actions.at(*action).actsOnThing;
			std::vector<creature_planner::ObjectCandidate> candidates;
			if (actsOnThing)
			{
				candidates = BelievedThingsToActOn(creature, mind, Desire::Impress, [&](entt::entity candidate) {
					return candidate != creature &&
					       (executor->target == Target::None || Accepts(registry, executor->target, candidate, creature));
				});
			}
			if (const auto chosen = creature_follow_up::ThingToDoAgainTo(actsOnThing, entt::to_integral(creature), candidates);
			    chosen.has_value())
			{
				const auto object = static_cast<entt::entity>(*chosen);
				static_cast<void>(
				    ForcePlan(creature, {.desire = Desire::Impress,
				                         .action = k_DanceWithVillagers,
				                         .object = object,
				                         .activityObject = WholeOfForcedPlan(creature, mind, Desire::Impress, object),
				                         .ordered = true,
				                         .fightBodyEndsAgain = true}));
				return;
			}
		}
	}
	static_cast<void>(ForcePlan(creature, {.desire = Desire::IdleWithPlayer,
	                                       .action = "PracticeDance",
	                                       .object = creature,
	                                       .ordered = true,
	                                       .fightBodyEndsAgain = true}));
}

void CreatureMindSystem::LearnFromDeedSeen(entt::entity creature, CreatureMindState& mind, size_t deed, entt::entity thing,
                                           uint32_t magic)
{
	const auto* tables = GetTables();
	if (tables == nullptr || !mind.learnt.has_value() || deed >= tables->mimics.size())
	{
		return;
	}
	const auto& rule = tables->mimics.at(deed);
	const auto actions = creature_watching::CopyActionsOf(rule, deed, magic, tables->miracles);
	if (actions.empty() || rule.desire >= creature_desires::k_DesireCount)
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	const auto desire = static_cast<Desire>(rule.desire);
	ComeAcross(creature, mind, thing);
	// A creature is its own whole thing
	const auto whole = registry.AllOf<Creature>(thing) ? std::optional(thing) : WholeOfShownThing(creature, mind, thing);
	const creature_learning::Context context {
	    .action = actions.front(),
	    .desire = desire,
	    .object = entt::to_integral(thing),
	    .belief = ActedOnBeliefOf(registry, thing, creature),
	    .activity = whole.transform([](entt::entity entity) { return entt::to_integral(entity); }),
	    .activityBelief = whole.has_value() ? BeliefOf(registry, *whole, creature) : std::nullopt,
	};
	// It learns nothing from a deed that it could not plan to do itself: on a thing it knows nothing of, or for a desire
	// about a whole thing when the thing is part of none
	if (!context.belief.has_value() || (tables->rules.at(rule.desire).aboutSomething && !context.activityBelief.has_value()))
	{
		return;
	}
	auto& learnt = *mind.learnt;
	const auto lessons = creature_learning::ThingLessonsFor(context, entt::to_integral(creature));
	const auto teach = [&](creature_mind_model::TreeKind kind, const creature_tree::Belief& belief) {
		creature_mind_model::LearnSpread(learnt, kind, desire, belief, k_DeedSeenLesson, tables->dependencies,
		                                 tables->attributes);
	};
	if (lessons.activity)
	{
		teach(creature_mind_model::TreeKind::Activity, *context.activityBelief);
	}
	if (lessons.actedOn)
	{
		teach(creature_mind_model::TreeKind::ActOn, *context.belief);
	}
	creature_mind_model::Think(learnt, creature_learning::ObjectLessonText(k_DeedSeenLesson, ObjectName(context.belief),
	                                                                       tables->texts.at(rule.desire).trying));
}

void CreatureMindSystem::CopyObservedDeedAgain([[maybe_unused]] entt::entity creature, CreatureMindState& mind)
{
	const auto* tables = GetTables();
	if (tables == nullptr || !mind.learnt.has_value())
	{
		return;
	}
	// A creature that never started to copy a deed has nothing to copy
	if (!mind.observedDeed.has_value())
	{
		SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Creature {} wants to copy the player but saw no deed to copy",
		                    entt::to_integral(creature));
		return;
	}
	auto observed = *mind.observedDeed;
	// The thing that the player did it to can be gone
	auto& registry = Locator::entitiesRegistry::value();
	if (observed.object.has_value() && !registry.Valid(static_cast<entt::entity>(*observed.object)))
	{
		observed.object.reset();
		mind.observedDeed->object.reset();
	}
	auto copy = creature_watching::CopyObservedDeedAgain(observed, tables->mimics, mind.turn,
	                                                     [this](uint32_t range) { return Random(range); });
	if (!copy.has_value())
	{
		return;
	}
	mind.learnt->mimicry = copy;
	if (observed.object.has_value())
	{
		const auto thing = static_cast<entt::entity>(*observed.object);
		if (const auto at = PointOf(registry, thing))
		{
			mind.copiedAt = *at;
		}
		mind.copiedKind = MimicKindOf(registry, thing);
	}
	SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Creature {} copies the player's {} again", entt::to_integral(creature),
	                    tables->mimics.at(observed.rule).name);
	creature_mind_model::Think(*mind.learnt, fmt::format("I'll copy you again: {}", tables->mimics.at(observed.rule).name));
}
