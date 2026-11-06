/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// How a creature learns from the feedback of its player: a stroke or a slap teaches it what it did, and each turn it
// keeps the actions that it did in mind for the feedback to come

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

namespace openblack::ecs::systems::mind_detail
{
void TellHelp(entt::entity creature, const openblack::creature_help::Message& message)
{
	if (openblack::Locator::creatureHelpSystem::has_value())
	{
		openblack::Locator::creatureHelpSystem::value().Raise(creature, message);
	}
}
} // namespace openblack::ecs::systems::mind_detail

namespace
{
constexpr float k_TurnSeconds = std::chrono::duration<float>(TimeSystemInterface::k_TurnDuration).count();
constexpr float k_TurnsPerSecond = 1.0f / k_TurnSeconds;

/// Feedback teaches lessons only once the creature has grown up this far; before, it only stirs its feelings
constexpr uint32_t k_MinLessonPhase = 2;
/// A stroke holds back sadness this long
constexpr float k_StrokeSadnessSeconds = 90.0f;

constexpr float k_MimicStrokeBoost = 0.3f;

/// Forgiven for running away, the threshold of its wish to run raises this much
constexpr float k_RunAwayThresholdStep = 0.1f;
/// A thing to use is learnt about at most this strongly for poo
constexpr float k_PooUseFeedback = 0.5f;

/// A creature stroked for what it holds eats it as the action that eats a thing alive
constexpr std::string_view k_EatHeldAction = "EatAlive";
} // namespace

namespace openblack::ecs::systems::mind_detail
{
std::string ObjectName(const std::optional<creature_tree::Belief>& belief)
{
	return belief.has_value() ? fmt::format("{}s", creature_tree::BeliefName(belief->type)) : "that";
}
} // namespace openblack::ecs::systems::mind_detail

void CreatureMindSystem::LearnFromFeedback(entt::entity creature, CreatureMindState& mind, float feedback)
{
	namespace sources = creature_desires::sources;
	if (!mind.desires.has_value() || !mind.learnt.has_value())
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	const auto* tables = GetTables();
	auto& desires = *mind.desires;
	auto& learnt = *mind.learnt;
	const auto contextIndex = creature_learning::BestContext(learnt.contexts);
	auto* context = contextIndex.has_value() ? &learnt.contexts.at(*contextIndex) : nullptr;
	const auto* info = context != nullptr && tables != nullptr && context->action < tables->actions.size()
	                       ? &tables->actions[context->action]
	                       : nullptr;

	// Stroked for holding something while doing an action that allows it, it learns to eat from the hand
	if (feedback > 0.0f && mind.developmentPhase > 0 && info != nullptr && info->eatWhenStroked &&
	    Locator::creatureObjectActionSystem::has_value())
	{
		if (const auto held = Locator::creatureObjectActionSystem::value().GetCarried(creature);
		    held.has_value() && FoodValueOf(*held).has_value())
		{
			creature_learning::MakeFullyDominant(desires, Desire::Hunger, mind_detail::DesireFloorOf(creature));
			if (const auto belief = BeliefOf(registry, *held, creature); belief.has_value() && tables != nullptr)
			{
				creature_mind_model::LearnSpread(learnt, TreeKind::ActOn, Desire::Hunger, *belief, 1.0f, tables->dependencies,
				                                 tables->attributes);
			}
			creature_mind_model::Think(learnt, "I've learnt to eat what I'm given");
			// It must eat the thing alive. The feedback for this meal goes to eating that thing alive. The meal is not a
			// forced plan, but the lesson that it does again after the meal is a forced plan.
			static_cast<void>(ForcePlan(creature, {.desire = Desire::Hunger,
			                                       .action = k_EatHeldAction,
			                                       .object = *held,
			                                       .agenda = creature_mind::EatHeld(),
			                                       .activity = creature_mind::Activity::Eat,
			                                       .forced = false,
			                                       .fightBodyEndsAgain = true}));
			// When the meal is done, it does again what it was stroked for. This replaces a thing that waits.
			mind.followUp = creature_follow_up::FollowUp::DoLessonAgain;
			return;
		}
	}
	// Stroked for running away from the player, it is forgiven and wants to run away less
	if (feedback > 0.0f && context != nullptr && context->desire == Desire::RunAwayFromPlayer)
	{
		auto& runAway = desires[Desire::RunAwayFromPlayer];
		creature_learning::MakeLeastDominant(desires, Desire::RunAwayFromPlayer, creature_learning::k_LeastDominantFactor,
		                                     mind_detail::DesireFloorOf(creature));
		for (auto& source : runAway.sources)
		{
			if (source.type == sources::k_RunAwayFromPlayer)
			{
				source.value = 0.0f;
				source.threshold = std::min(source.threshold + k_RunAwayThresholdStep, creature_learning::k_MaxThreshold);
			}
		}
		Abandon(mind);
		return;
	}

	// A slap stops it when it copies its player, and makes it show how it feels when it completes its current work
	if (feedback < 0.0f)
	{
		creature_watching::SlappedWhileMimicking(learnt.mimicry);
		mind.followUp = creature_follow_up::FollowUp::ShowSlapFeeling;
	}
	// With nothing it did lately to credit the feedback to, it learns nothing from it
	if (context == nullptr)
	{
		return;
	}
	const auto strength = creature_learning::Strengthened(feedback);
	if (feedback > 0.0f)
	{
		if (learnt.mimicry.has_value() && tables != nullptr)
		{
			creature_watching::StrokedWhileMimicking(learnt.mimicry, tables->mimics,
			                                         [this](uint32_t range) { return Random(range); });
			creature_desires::ChangeSource(desires, k_FollowPlayerSource, k_MimicStrokeBoost);
		}
		creature_desires::Suppress(desires, Desire::Sadness, k_StrokeSadnessSeconds, k_TurnsPerSecond);
	}
	// Slapped, it gives up the newest thing that it did and wants that desire least of all, young or not. This needs a
	// plan that it could carry out: a desire about a whole thing needs that whole thing.
	const auto giveUpAfterSlap = [&]() {
		if (feedback > 0.0f || learnt.contexts.empty() || tables == nullptr)
		{
			return;
		}
		const auto& newest = learnt.contexts.back();
		if (tables->rules.at(static_cast<size_t>(newest.desire)).aboutSomething && !newest.activityBelief.has_value())
		{
			return;
		}
		creature_learning::MakeLeastDominant(desires, newest.desire, creature_learning::k_LeastDominantFactor,
		                                     mind_detail::DesireFloorOf(creature));
		for (auto& state : desires.desires)
		{
			state.suppressedTurns = 0;
		}
		// This slap also clears the mark that the hand held it
		mind.heldSinceRestart = false;
		Abandon(mind);
	};

	if (info == nullptr || mind.developmentPhase < k_MinLessonPhase)
	{
		// Nothing to learn from yet: stroking makes it playful, showy and kind; slapping angry and fearful
		const auto amount = 0.5f * std::abs(feedback);
		constexpr std::array k_Stroked {sources::k_PlayFromWatchingPlayer, sources::k_ManifestState,
		                                sources::k_CompassionFromWatchingPlayer};
		constexpr std::array k_Slapped {sources::k_AngerFromDamage, sources::k_FearFromDamage};
		const auto pushed = feedback > 0.0f ? std::span<const uint32_t>(k_Stroked) : std::span<const uint32_t>(k_Slapped);
		for (const auto type : pushed)
		{
			creature_desires::ChangeSource(desires, type, amount);
		}
		TellHelp(creature, creature_help::TooYoungToLearn(feedback));
		mind.followUp = creature_follow_up::AfterYoungFeedback(mind.followUp, feedback, Random(2));
		giveUpAfterSlap();
		// A young creature that is stroked also does its action again, as an older one does
		if (feedback > 0.0f)
		{
			DoLessonAgain(creature, mind);
		}
		return;
	}

	const auto desire = context->desire;
	const auto d = static_cast<size_t>(desire);
	const auto& texts = tables->texts.at(d);
	context->credited = strength;
	// It is taught: the player has dealt with it
	mind.attention.turnsSinceInteraction = 0;
	if (tables->rules.at(d).learnable)
	{
		creature_learning::LearnDesireLesson(desires, desire, creature_learning::DominantSource(desires[desire]), strength,
		                                     tables->rules, tables->dependencies, tables->thresholdBounds);
		creature_mind_model::Think(learnt, creature_learning::DesireLessonText(strength, texts.desire));
		// What the desire was about teaches what such things are worth as its point; what it acted on, when that was
		// something else and not itself, what such things are worth acting on (soiling only a little either way)
		const auto lessons = creature_learning::ThingLessonsFor(*context, entt::to_integral(creature));
		if (lessons.activity)
		{
			creature_mind_model::LearnSpread(learnt, TreeKind::Activity, desire, *context->activityBelief, strength,
			                                 tables->dependencies, tables->attributes);
		}
		if (lessons.actedOn)
		{
			const auto acted = desire == Desire::Poo ? std::clamp(strength, -k_PooUseFeedback, k_PooUseFeedback) : strength;
			creature_mind_model::LearnSpread(learnt, TreeKind::ActOn, desire, *context->belief, acted, tables->dependencies,
			                                 tables->attributes);
		}
		if (context->belief.has_value())
		{
			creature_mind_model::Think(
			    learnt, creature_learning::ObjectLessonText(strength, ObjectName(context->belief), texts.trying));
		}
		// The lesson changed its opinion of doing it to such things when what it knows of them has split up
		const auto belowRoot = [&learnt, desire](TreeKind kind, const creature_tree::Belief& belief) {
			const auto& tree = learnt.trees.at(static_cast<size_t>(kind)).at(static_cast<size_t>(desire));
			const auto leaf = creature_tree::LeafOf(tree, belief);
			return leaf.has_value() && *leaf != 0;
		};
		const bool opinionChanged = (lessons.activity && belowRoot(TreeKind::Activity, *context->activityBelief)) ||
		                            (lessons.actedOn && belowRoot(TreeKind::ActOn, *context->belief));
		auto& opinion = learnt.opinions.at(context->action);
		opinion = creature_learning::OpinionAfter(opinion, strength);
		creature_mind_model::Think(
		    learnt, creature_learning::ActionLessonText(strength, info->name, texts.trying, ObjectName(context->belief)));
		TellHelp(creature, creature_help::FeedbackLesson(strength, opinionChanged, desire, context->source));
	}

	if (feedback <= 0.0f)
	{
		giveUpAfterSlap();
		return;
	}
	// Stroked for something it did to a thing, it must do it again to something like it
	DoLessonAgain(creature, mind);
}

void CreatureMindSystem::LearnTurn()
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* tables = GetTables();
	if (tables == nullptr)
	{
		return;
	}
	std::vector<entt::entity> creatures;
	registry.Each<const Creature, CreatureMindState>(
	    [&creatures](entt::entity entity, const Creature&, CreatureMindState&) { creatures.push_back(entity); });
	for (const auto creature : creatures)
	{
		LearnCreatureTurn(creature);
	}
}

void CreatureMindSystem::LearnCreatureTurn(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* tables = GetTables();
	if (tables == nullptr || !registry.Valid(creature) || !registry.AllOf<Creature>(creature))
	{
		return;
	}
	auto* mind = registry.TryGet<CreatureMindState>(creature);
	if (mind == nullptr || !mind->learnt.has_value() || !mind->desires.has_value())
	{
		return;
	}
	auto& learnt = *mind->learnt;
	// What the player showed it on a leash: anger or compassion towards such things
	for (const auto& shown : mind->leash.shown)
	{
		// It comes across the thing first, so the lesson has the thing as it is now. The lesson is about the whole
		// thing that the thing is part of (the town of a villager); a creature is its own whole thing. A thing that is
		// part of nothing teaches nothing.
		const auto thing = static_cast<entt::entity>(shown.object);
		ComeAcross(creature, *mind, thing);
		const auto whole = registry.AllOf<Creature>(thing) ? std::optional(thing) : WholeOfShownThing(creature, *mind, thing);
		const auto wholeBelief = whole.has_value() ? BeliefOf(registry, *whole, creature) : std::nullopt;
		const auto belief = BeliefOf(registry, thing, creature);
		if (!wholeBelief.has_value() || !belief.has_value())
		{
			continue;
		}
		for (const auto& lesson : shown.lessons)
		{
			creature_mind_model::LearnSpread(learnt, TreeKind::Activity, lesson.desire, *wholeBelief, lesson.change,
			                                 tables->dependencies, tables->attributes);
			creature_mind_model::Think(
			    learnt, creature_learning::ObjectLessonText(lesson.change, ObjectName(belief),
			                                                tables->texts.at(static_cast<size_t>(lesson.desire)).trying));
		}
	}
	mind->leash.shown.clear();
	// Leashed to another creature, it comes to find it nicer or nastier, and learns about creatures like it
	for (const auto& change : mind->leash.attitudes)
	{
		auto& attitude = creature_learning::AttitudeTo(learnt.creatures, change.creature);
		const auto lessons = creature_learning::ChangeHowNice(attitude, change.change);
		// It comes across the other creature first, so the lessons have that creature as it is now
		ComeAcross(creature, *mind, static_cast<entt::entity>(change.creature));
		if (const auto belief = BeliefOf(registry, static_cast<entt::entity>(change.creature), creature))
		{
			for (const auto& [desire, amount] : lessons)
			{
				creature_mind_model::LearnSpread(learnt, TreeKind::Activity, desire, *belief, amount, tables->dependencies,
				                                 tables->attributes);
			}
		}
	}
	mind->leash.attitudes.clear();
}

void CreatureMindSystem::RememberAction(entt::entity creature, CreatureMindState& mind, creature_learning::Context context)
{
	const auto* tables = GetTables();
	if (!mind.learnt.has_value())
	{
		return;
	}
	auto& learnt = *mind.learnt;
	const auto forgotten = creature_learning::Remember(learnt.contexts, std::move(context));
	if (!forgotten.has_value() || tables == nullptr)
	{
		return;
	}
	SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Creature {} forgets action {} (ordered {}, done {})", entt::to_integral(creature),
	                    forgotten->action, forgotten->ordered, forgotten->done);
	// An action that the player made it do teaches approval when the creature forgets it, as a small stroke would
	// teach it about the whole thing and the thing that it acted on. It learns nothing about the desire or the action.
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* body = registry.TryGet<const Creature>(creature);
	if (body == nullptr || !Locator::infoConstants::has_value())
	{
		return;
	}
	const auto& species = Locator::infoConstants::value().creature;
	const auto row = creature::InfoRow(body->species);
	const auto desire = forgotten->desire;
	const auto approval = creature_learning::ForgottenOrderApproval(
	    *forgotten, tables->rules.at(static_cast<size_t>(desire)), row < species.size() ? species.at(row).orderApproval : 0.0f);
	if (!approval.has_value())
	{
		return;
	}
	const auto lessons = creature_learning::ThingLessonsFor(*forgotten, entt::to_integral(creature));
	const auto teach = [&](TreeKind kind, const creature_tree::Belief& belief, float feedback) {
		creature_mind_model::LearnSpread(learnt, kind, desire, belief, feedback, tables->dependencies, tables->attributes);
	};
	if (lessons.activity)
	{
		teach(TreeKind::Activity, *forgotten->activityBelief, *approval);
	}
	if (lessons.actedOn)
	{
		teach(TreeKind::ActOn, *forgotten->belief,
		      desire == Desire::Poo ? std::clamp(*approval, -k_PooUseFeedback, k_PooUseFeedback) : *approval);
	}
	SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Creature {} forgets action {} that it was made to do and approves it by {}",
	                    entt::to_integral(creature), forgotten->action, *approval);
}

void CreatureMindSystem::MarkNewestActionDone(entt::entity creature, CreatureMindState& mind)
{
	if (mind.learnt.has_value())
	{
		creature_learning::MarkNewestDone(mind.learnt->contexts, LookedAtByItsPlayer(creature));
	}
}

void CreatureMindSystem::NoteStepSeen(entt::entity creature, CreatureMindState& mind)
{
	// The creature does not show the player what it wants yet, so only the steps at or after the main step count
	const auto& idle = mind.idle;
	if (!mind.learnt.has_value() || !idle.movedOn.has_value() ||
	    !creature_mind::AtOrPastMainStep(idle.agenda, *idle.movedOn, false) || !LookedAtByItsPlayer(creature))
	{
		return;
	}
	creature_learning::MarkNewestSeen(mind.learnt->contexts);
}

bool CreatureMindSystem::LookedAtByItsPlayer(entt::entity creature)
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* transform = registry.TryGet<const Transform>(creature);
	const auto camera = openblack::ecs::ReportedCameraOfItsPlayer(creature);
	if (transform == nullptr || !camera.has_value())
	{
		return false;
	}
	return creature_attention::IsLookingAt(camera->origin, camera->focus, transform->position);
}
