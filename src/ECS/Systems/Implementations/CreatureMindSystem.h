/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <functional>
#include <optional>
#include <random>
#include <span>
#include <utility>
#include <vector>

#include "Common/RandomNumberManager.h"
#include "Creature/CreatureActivity.h"
#include "Creature/CreatureIdleMind.h"
#include "Creature/CreatureMindTables.h"
#include "Creature/CreaturePlanActions.h"
#include "Creature/CreaturePlanner.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureCasting.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/Transform.h"
#include "ECS/Systems/CreatureMindSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::systems
{

class CreatureMindSystem final: public CreatureMindSystemInterface
{
public:
	void ProcessTurn() override;
	void PlanTurn() override;
	void LearnTurn() override;

	void LoadMind(entt::entity creature, std::shared_ptr<const creaturemind::MindFileData> mind) override;
	[[nodiscard]] std::optional<creaturemind::MindFileData> SaveMind(entt::entity creature) const override;
	void ClearLearning(entt::entity creature) override;
	void SeeSkill(const glm::vec3& point, size_t skill) override;
	void SeeMiracle(const glm::vec3& point, size_t miracle) override;
	void PlayerDid(size_t deed, const glm::vec3& point, std::optional<entt::entity> object,
	               std::optional<PlayerNames> player) override;
	[[nodiscard]] const creature_mind_tables::Tables* GetTables() override;

	bool PlayAction(entt::entity creature, size_t animation) override;
	bool PlayGesture(entt::entity creature, size_t animation) override;
	void PullFace(entt::entity creature, size_t animation) override;
	std::optional<creature_face::Request> ShowFeeling(entt::entity creature, creature_face::Cue cue) override;
	bool SitDown(entt::entity creature) override;
	void StandUp(entt::entity creature) override;
	void ReceiveFeedback(entt::entity creature, float feedback) override;
	void HandTookHold(entt::entity creature) override;
	void UpdateAttitudeFromFeedback(entt::entity creature, float feedback) override;
	void ChangeDesireSource(entt::entity creature, uint32_t type, float amount) override;
	bool ForceAction(entt::entity creature, size_t animation, bool mirrored, std::optional<creature_face::Request> face,
	                 float interruptsAfter) override;
	bool Sleep(entt::entity creature) override;
	bool Eat(entt::entity creature, std::optional<entt::entity> food) override;
	bool Drink(entt::entity creature) override;
	bool Poo(entt::entity creature) override;
	bool Puke(entt::entity creature) override;
	bool Faint(entt::entity creature) override;
	void Wake(entt::entity creature) override;
	void FoughtFight(entt::entity creature, bool won) override;
	void AbandonAction(entt::entity creature) override;
	void ForceCatch(entt::entity creature, entt::entity object) override;
	bool TakeFromHand(entt::entity creature, entt::entity object, bool leftHand, glm::vec2 camera) override;
	[[nodiscard]] bool IsTakingFromHand(entt::entity creature) const override;
	[[nodiscard]] bool IsLyingDead(entt::entity creature) override;
	bool ForceActivity(entt::entity creature, const ForcedActivity& plan) override;
	void ReactToFire(entt::entity creature, entt::entity burning) override;
	void ReactToNastyMagic(entt::entity creature, const glm::vec3& point, std::optional<size_t> learn) override;
	void ReactToNiceMagic(entt::entity creature, const glm::vec3& point, std::optional<size_t> learn) override;
	void TakeUpReaction(entt::entity creature, entt::entity thing, const creature_reaction::Setup& setup) override;
	[[nodiscard]] bool NoticeCreatureToInspect(entt::entity creature, entt::entity other, uint32_t turn,
	                                           uint32_t waitTurns) override;
	bool TryMiracle(entt::entity creature, MagicType type, entt::entity target) override;
	void KnowMiracle(entt::entity creature, size_t miracle) override;
	void SetKnowsAction(entt::entity creature, CreatureActionLearningType type, uint32_t action, bool knows) override;
	bool TellCast(entt::entity creature, MagicType type, entt::entity target) override;
	bool ForcePlan(entt::entity creature, const ForcedPlan& plan) override;
	void AteMagicMushroom(entt::entity creature) override;
	bool SwapMinds(entt::entity from, entt::entity to) override;
	std::optional<creature_desires::Desire> ForcePlanOn(entt::entity creature, entt::entity object) override;
	bool CarryOutForScript(entt::entity creature, std::vector<creature_mind::Step> agenda) override;
	void SetScriptFocus(entt::entity creature, std::optional<creature_mind::Gaze> focus) override;
	bool ScriptDoAction(entt::entity creature, uint32_t action, entt::entity target, std::optional<entt::entity> with) override;
	[[nodiscard]] bool HasPlayed(entt::entity creature) const override;
	[[nodiscard]] float GetInteractionMagnitude(entt::entity creature) const override;
	void ClearInteractionMagnitude(entt::entity creature) override;
	[[nodiscard]] uint32_t GetActionCount(entt::entity creature, uint32_t action) const override;
	void SetOnlyDesire(entt::entity creature, creature_desires::Desire desire, float seconds) override;
	void ClearOnlyDesire(entt::entity creature) override;
	void MakeDesireDominant(entt::entity creature, creature_desires::Desire desire, float seconds, bool needsToo) override;
	void EndDominantDesire(entt::entity creature) override;
	[[nodiscard]] std::optional<creature_desires::Desire> DominantDesire(entt::entity creature) const override;
	void ChangeDesires(entt::entity creature, const std::function<void(creature_desires::Desires&)>& change) override;
	void SetAgendaPriority(entt::entity creature, float priority) override;
	void MoveToStage(entt::entity creature, uint32_t stage) override;
	bool PointOutHighlight(entt::entity creature) override;

	/// What a thing belongs to, as a creature sees it: a building's, villager's or field's town, a tree's forest (or else
	/// the nearest forest with trees), an animal's flock, a creature itself; nothing for anything else
	[[nodiscard]] static std::optional<entt::entity> BelongsTo(entt::entity thing);

private:
	/// Gives a creature its desires and what it has learnt, if it has none yet: the first time its mind thinks, or
	/// sooner when it is told to do something before then
	void SetUpMind(entt::entity creature, const components::Creature& body, components::CreatureMindState& mind);
	/// Sets up what a creature has learnt the first time its mind thinks, from its mind file when it has one
	void SetUpLearning(entt::entity creature, components::CreatureMindState& mind);
	/// Takes up a mind file waiting to be loaded
	void TakeUpFile(entt::entity creature, components::CreatureMindState& mind);
	/// How a creature's swap of minds stands this turn: whether the spells on both creatures have gone and whether its
	/// body has taken on its new shape
	[[nodiscard]] static creature_mind::Senses::SwapView SwapViewOf(entt::entity creature);
	/// What the idle mind told a creature carrying out a swap to do this turn
	void StepSwap(entt::entity creature, const creature_mind::Commands& commands);
	/// Remembers what the idle mind has started, for feedback to be credited to, and finishes plans that are done
	void FollowAgenda(entt::entity creature, components::CreatureMindState& mind);
	/// Carries out a plan in place of what the creature was doing; returns whether it could
	bool Adopt(entt::entity creature, components::CreatureMindState& mind, const creature_planner::Plan& plan,
	           const creature_plan_actions::Situation& situation);
	/// Gives up the plan carried out, if any
	static void Abandon(components::CreatureMindState& mind);
	/// Copying the player, the creature does what the player did: to the player's thing, else to the nearest thing of the
	/// same kind, else to the most useful thing (CreatureMindLearning.cpp)
	void CopyPlayersDeed(entt::entity creature, components::CreatureMindState& mind);
	/// A script makes a creature pick a fight with another: it asks, and they fight if the other agrees; either way the
	/// script's order is carried out
	bool FightForScript(entt::entity creature, entt::entity opponent);
	/// Learns what feedback teaches, from what the creature did lately
	void LearnFromFeedback(entt::entity creature, components::CreatureMindState& mind, float feedback);
	/// The creature has taken a thing the hand gave it: its desires choose what it does with it
	void UseGift(entt::entity creature, entt::entity object);
	/// Held by the player's hand, it shows what it wants once its body is free
	void ShowDesireOnHold(entt::entity creature, components::CreatureMindState& mind, components::CreatureAnimation& animation);
	/// Plan the desires of one creature for this turn, a maximum of one time each turn. The planner examines the
	/// desires for this turn, or all the desires, or only one desire. For only one desire, the creature starts the plan
	/// immediately, if there is a plan.
	void PlanCreature(entt::entity creature, components::CreatureMindState& mind, bool everyDesire = false,
	                  std::optional<creature_desires::Desire> only = std::nullopt);
	/// Uniform random numbers from 0 to n - 1, and from 0 to 1
	uint32_t Random(uint32_t range);
	float Chance();
	/// What an agenda needs to know of the creature itself: the camera's position, whether it has a player, whether a
	/// script controls it, its radius, and a chance for what varies
	void Situate(entt::entity creature, creature_plan_actions::Situation& situation);

	/// Plans an activity in place of what the creature was doing: it stops walking and gets up out of a pose first, while
	/// an action it is playing plays on and the new agenda waits for it
	bool Replan(entt::entity creature, creature_mind::Activity activity, std::vector<creature_mind::Step> agenda);
	/// The creature sees a miracle it reacted to, and learns from it
	void WatchMiracle(entt::entity creature, size_t miracle);
	/// The creature watches a skill being done, and learns from it
	void WatchSkill(entt::entity creature, size_t skill);
	/// How useful the creature has learnt a thing is for a desire, when it may act on it at all: nothing when it is kept
	/// within an area and the thing lies outside it
	[[nodiscard]] float ActivityUsefulness(entt::entity creature, const components::CreatureMindState& mind,
	                                       creature_desires::Desire desire, entt::entity object) const;
	/// How useful a thing the creature believes in is to do an action to for a desire: by what it has learnt of acting
	/// on such things, by how far it is from the creature (or from where its player's leash is tied), and for curiosity a
	/// hundredth as much for a kind it has had a good look at; nothing when it is kept within an area and the thing lies
	/// outside it, or it has no belief about the thing
	[[nodiscard]] float ObjectToActOnUsefulness(entt::entity creature, const components::CreatureMindState& mind,
	                                            creature_desires::Desire desire, entt::entity object);
	/// The whole thing that a desire about something is about, and its usefulness with its distance included
	struct ChosenActivity
	{
		entt::entity whole {entt::null};
		creature_activity::Whole kind {creature_activity::Whole::Town};
		float usefulness {0.0f};
	};
	/// Find the whole thing that a desire is about. Examine the whole things that the creature has beliefs about, the
	/// newest belief first. The desire must be able to be about the thing. Take the first thing that is worth the most.
	/// There is no result when no thing is worth more than nothing.
	[[nodiscard]] std::optional<ChosenActivity> ChooseActivity(entt::entity creature, const components::CreatureMindState& mind,
	                                                           creature_desires::Desire desire);
	/// The parts of the whole thing that the creature can act on for the desire, the newest belief first, each with its
	/// worth. The part of a creature is the creature.
	[[nodiscard]] std::vector<creature_planner::ObjectCandidate> PartsToActOn(entt::entity creature,
	                                                                          const components::CreatureMindState& mind,
	                                                                          creature_desires::Desire desire,
	                                                                          const ChosenActivity& activity);
	/// Whether a thing that the creature has a belief about suits a desire that is not about a whole thing. The thing
	/// must let a creature act on it. A hungry creature does not eat a thing of a town that it feels much compassion for.
	/// The creature comes across that town when it examines the rule.
	[[nodiscard]] bool SuitsDesire(entt::entity creature, components::CreatureMindState& mind, creature_desires::Desire desire,
	                               entt::entity thing);
	/// The things that the creature has beliefs about and that an action of a desire can act on, each with its worth.
	/// The function examines the two lists of beliefs in their order: the whole things first, then the other things,
	/// each list with the newest belief first.
	[[nodiscard]] std::vector<creature_planner::ObjectCandidate>
	BelievedThingsToActOn(entt::entity creature, components::CreatureMindState& mind, creature_desires::Desire desire,
	                      const std::function<bool(entt::entity)>& suitsAction);
	/// The whole thing that a thing shown to the creature is part of. The creature comes across the thing and the whole
	/// thing. A creature is part of no whole thing.
	[[nodiscard]] std::optional<entt::entity> WholeOfShownThing(components::CreatureMindState& mind, entt::entity thing);
	/// The whole thing that a plan that the creature must do is about: the whole thing that the plan's thing is part
	/// of. With no such whole thing, a desire about something is about the nearest town.
	[[nodiscard]] std::optional<entt::entity> WholeOfForcedPlan(entt::entity creature, components::CreatureMindState& mind,
	                                                            creature_desires::Desire desire, entt::entity thing);
	/// The whole thing that a desire is about when the leash shows the creature a thing: the whole thing that the shown
	/// thing is part of, when the desire can be about it
	[[nodiscard]] std::optional<ChosenActivity> ShownActivity(entt::entity creature, components::CreatureMindState& mind,
	                                                          creature_desires::Desire desire, entt::entity thing);
	/// The parts that a plan can act on when the leash shows a thing: the shown thing when it suits the desire and is of
	/// use, else the parts of the whole thing that do, the newest belief first
	[[nodiscard]] std::vector<creature_planner::ObjectCandidate> ShownParts(entt::entity creature,
	                                                                        components::CreatureMindState& mind,
	                                                                        creature_desires::Desire desire, entt::entity thing,
	                                                                        const ChosenActivity& activity);
	/// The things that a plan for a desire not about a whole thing can act on when the leash shows a thing: the shown
	/// thing when it suits the desire and is of use, else the parts of its whole thing that do, the newest belief first
	[[nodiscard]] std::vector<creature_planner::ObjectCandidate> ShownThingsWithoutWhole(entt::entity creature,
	                                                                                     components::CreatureMindState& mind,
	                                                                                     creature_desires::Desire desire,
	                                                                                     entt::entity thing);
	/// The desire that a creature most wants for a thing shown on the leash, of the desires not yet tried; none when no
	/// desire is left
	[[nodiscard]] std::optional<creature_desires::Desire> DesireForShownThing(entt::entity creature,
	                                                                          components::CreatureMindState& mind,
	                                                                          entt::entity thing,
	                                                                          std::span<const creature_desires::Desire> tried);
	/// What a thing is worth to a creature as the goal of a plan for a desire, and what that worth counts for in the
	/// plan's priority. With a whole thing that the desire is about, the thing is a part of that whole thing.
	[[nodiscard]] creature_planner::ObjectCandidate GoalWorth(entt::entity creature, const components::CreatureMindState& mind,
	                                                          creature_desires::Desire desire, entt::entity thing,
	                                                          const std::optional<ChosenActivity>& activity);
	/// Find the desire that a creature most wants to do about a different creature. The function does not examine the
	/// desires in the list of excluded desires. There is no result when no desire has a value.
	[[nodiscard]] std::optional<creature_desires::Desire>
	DesireAboutCreature(entt::entity creature, const components::CreatureMindState& mind, entt::entity other,
	                    std::span<const creature_desires::Desire> leftOut);
	/// Stop the current action as a failure. Make the desire the strongest desire. Plan the desire for the thing only.
	/// Return true if the creature found a plan.
	bool ForceDesireAbout(entt::entity creature, components::CreatureMindState& mind, creature_desires::Desire desire,
	                      entt::entity thing);
	/// Find the desires of a creature and of the creature that it watches. The creature can stop the watch and act on
	/// a desire. Return true if it did.
	bool WeighWatchedCreature(entt::entity creature, components::CreatureMindState& mind, entt::entity other);
	/// A creature douses a thing with water, five times a bucket's worth, as it does after casting the water miracle at
	/// a fire (CreatureMindFire.cpp); whether the thing is no longer on fire
	bool Douse(entt::entity creature, entt::entity object);

	// What it believes about the things about it (CreatureMindBeliefs.cpp)
	/// It comes across a thing: its belief about it is brought up to date, or made, and a new one leads it on to what the
	/// thing belongs to
	void ComeAcross(components::CreatureMindState& mind, entt::entity thing);
	/// Where it believes a thing is: where it last came across it; nothing when it has never come across it
	[[nodiscard]] static std::optional<glm::vec3> BelievedPlace(const components::CreatureMindState& mind, entt::entity thing);
	/// It forgets what it believed about things gone from the world
	void ForgetGoneThings(components::CreatureMindState& mind);
	/// Gone into another map cell, it comes across the temple nearest it if that is another than the last
	void NoteNearestTemple(components::CreatureMindState& mind, const components::Transform& transform);
	/// Its eyes sweep this turn's cells about it, and it comes across everything in those it can see
	void SweepEyes(const components::Creature& creature, components::CreatureMindState& mind,
	               const components::CreatureAnimation& animation, const components::Transform& transform);
	/// The agenda for an action of the game's table on an object, and what carries it out; none when it can't be made
	[[nodiscard]] std::optional<std::pair<const creature_plan_actions::Executor*, std::vector<creature_mind::Step>>>
	PlanAgenda(entt::entity creature, uint32_t action, std::optional<uint32_t> object,
	           const creature_plan_actions::Situation& situation);

	// Casting miracles (CreatureMindCasting.cpp)
	/// The miracle an action of the game's table casts, if any
	static std::optional<uint32_t> CastMagicOf(uint32_t action);
	/// Whether the creature may try the miracle an action casts: seen often enough, able to pay for it, and for a
	/// power-up grown up enough
	bool MayCast(entt::entity creature, const components::CreatureMindState& mind, uint32_t action, bool powerUp);
	/// What the creature casts for an action, the gesture it draws first and its height
	std::optional<creature_plan_actions::CastInfo> CastInfoFor(entt::entity creature, uint32_t action);
	/// How often the creature has seen a miracle and how often it needs to have seen it to cast it
	[[nodiscard]] std::optional<std::pair<float, float>> MiracleSightings(entt::entity creature, MagicType type);
	/// A fizzled try, shown as it next chooses what to do
	void ShowFizzle(entt::entity creature, components::CreatureMindState& mind);
	/// It is made to be high, as after eating a magic mushroom
	void BecomeHigh(entt::entity creature);
	/// Going near, getting away from or turning to face an object begins, and each turn it goes on
	void StartSubMove(entt::entity creature, const creature_mind::Movement& movement, float seconds);
	void StepSubMove(entt::entity creature, bool animating);
	bool GoNear(entt::entity creature, components::CreatureCasting& casting, bool reissue);
	creature_mind::SubMove SubMoveOf(entt::entity creature);
	/// The minds choose at random, apart from the game's own random numbers
	RandomStreamSource _random {RandomStream::CreatureMind};
	/// The game's tables for the minds, taken once the game's data is loaded
	std::optional<creature_mind_tables::Tables> _tables;
};

} // namespace openblack::ecs::systems
