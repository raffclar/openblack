/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <functional>
#include <optional>
#include <random>
#include <span>
#include <utility>
#include <vector>

#include "Common/RandomNumberManager.h"
#include "Creature/CreatureActivity.h"
#include "Creature/CreatureIdleMind.h"
#include "Creature/CreatureMindModel.h"
#include "Creature/CreatureMindTables.h"
#include "Creature/CreatureObjectBeliefs.h"
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
	void PlayerCastDeed(size_t deed, const glm::vec3& point, std::optional<entt::entity> object,
	                    std::optional<PlayerNames> player, MagicType magic) override;
	[[nodiscard]] const creature_mind_tables::Tables* GetTables() override;
	[[nodiscard]] std::optional<creature_tree::Belief> BeliefAbout(entt::entity creature, entt::entity thing) const override;

	bool PlayAction(entt::entity creature, size_t animation) override;
	bool PlayGesture(entt::entity creature, size_t animation) override;
	void PullFace(entt::entity creature, size_t animation) override;
	std::optional<creature_face::Request> ShowFeeling(entt::entity creature, creature_face::Cue cue) override;
	bool SitDown(entt::entity creature) override;
	void StandUp(entt::entity creature) override;
	void ReceiveFeedback(entt::entity creature, float feedback) override;
	void HandTookHold(entt::entity creature) override;
	void UpdateAttitudeFromFeedback(entt::entity creature, float feedback) override;
	[[nodiscard]] float GetAverageFeedback() const override { return _averageFeedback; }
	void NoteLeashOrder(entt::entity creature) override;
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
	void FightStarted(entt::entity creature, entt::entity opponent, bool planned) override;
	void AbandonAction(entt::entity creature) override;
	bool ForceDesireOn(entt::entity creature, creature_desires::Desire desire, entt::entity thing) override;
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
	void ClearAfterPlayed(entt::entity creature) override;
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
	void ForceFinish(entt::entity creature) override;
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
	/// The mind that a mind file loads over: a fresh mind of the species with its starting lesson, or the creature's own
	/// mind, which a swap of minds keeps
	enum class FileBase : uint8_t
	{
		Fresh,
		Own,
	};
	/// Takes up a mind file waiting to be loaded
	void TakeUpFile(entt::entity creature, components::CreatureMindState& mind, FileBase base = FileBase::Fresh);
	/// Gives the creature the mind that a file loaded
	void ApplyLoaded(entt::entity creature, components::CreatureMindState& mind, creature_mind_model::LoadedMind loaded,
	                 const creaturemind::MindFileData& file);
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
	/// The creature stops the plan with no success, then starts the agenda of the same plan again from its first step
	void StartPlanAgain(entt::entity creature, components::CreatureMindState& mind);
	/// The turn rules that come after the hand lets go and before the creature decides: the desires of its body before
	/// the first stage, the plan that starts again after a hold, and the copy timeout
	void TurnBeforeDecision(entt::entity creature, const components::Creature& body, components::CreatureMindState& mind,
	                        bool scripted);
	/// The emergencies that come after the turn rules and before the creature decides: low life, a fire on its body, and
	/// a fire in the nearest town
	void StepEmergencies(entt::entity creature, const components::Creature& body, components::CreatureMindState& mind,
	                     bool scripted);
	/// The creature heals a thing at once, as a stroke heals it
	void Heal(entt::entity creature, entt::entity thing);
	/// Copying the player, the creature does what the player did: to the player's thing, else to the nearest thing of the
	/// same kind, else to the most useful thing (CreatureMindLearning.cpp)
	void CopyPlayersDeed(entt::entity creature, components::CreatureMindState& mind);
	/// Copying the player, with no plan to carry out, the creature does the action of the copy stage again, or goes on to
	/// the next stage (CreatureMindLearning.cpp)
	void ProcessMimicry(entt::entity creature, components::CreatureMindState& mind);
	/// A script makes a creature pick a fight with another: it asks, and they fight if the other agrees; either way the
	/// script's order is carried out
	bool FightForScript(entt::entity creature, entt::entity opponent);
	/// Learns what feedback teaches, from what the creature did lately
	void LearnFromFeedback(entt::entity creature, components::CreatureMindState& mind, float feedback);
	/// The creature has taken a thing the hand gave it: its desires choose what it does with it
	void UseGift(entt::entity creature, entt::entity object);
	/// The creature continues the plan that it kept with the thing that it took from the hand, if the thing suits
	/// the plan; returns whether it did (CreatureMindLearning.cpp)
	bool ContinueKeptPlanWith(entt::entity creature, components::CreatureMindState& mind, entt::entity gift);
	/// Held by the player's hand, it shows what it wants once its body is free
	void ShowDesireOnHold(entt::entity creature, components::CreatureMindState& mind, components::CreatureAnimation& animation);
	/// Plan the desires of one creature for this turn, a maximum of one time each turn. The planner examines the
	/// desires for this turn, or all the desires, or only one desire. For only one desire, the creature starts the plan
	/// immediately, if there is a plan. When adopt is false, the planner only finds the plans and the creature does not
	/// start a plan.
	void PlanCreature(entt::entity creature, components::CreatureMindState& mind, bool everyDesire = false,
	                  std::optional<creature_desires::Desire> only = std::nullopt, bool adopt = true);
	/// The first desire that has a plan for a thing that the creature holds, found as for a thing on the leash. The
	/// search uses a copy of the mind, so that the current plan does not change (CreatureMindLearning.cpp)
	[[nodiscard]] std::optional<creature_desires::Desire>
	DesireForThingInHand(entt::entity creature, components::CreatureMindState& mind, entt::entity thing);
	/// The creature lets go of the thing that it holds because the next step needs empty hands: it puts the thing down,
	/// or it tosses the thing away (CreatureMindLearning.cpp)
	void LetGoOfHeld(entt::entity creature, components::CreatureMindState& mind, creature_mind::LetGo how);
	/// Uniform random numbers from 0 to n - 1, and from 0 to 1
	uint32_t Random(uint32_t range);
	float Chance();
	/// What an agenda needs to know of the creature itself: the camera's position, whether it has a player, whether a
	/// script controls it, its radius, and a chance for what varies
	void Situate(entt::entity creature, creature_plan_actions::Situation& situation);
	/// What the plan actions with its player need: its attitude to the player, and the player's hand
	void SituatePlayer(entt::entity creature, creature_plan_actions::Situation& situation);
	/// What the emotes and the sleep and sit places need to know of where the creature is
	void SituateEmotes(entt::entity creature, creature_plan_actions::Situation& situation);
	/// The clear place where the creature sleeps on the spot or sits down by a thing; none for other actions, or when
	/// there is no clear place
	[[nodiscard]] std::optional<glm::vec2> EmoteClearArea(entt::entity creature,
	                                                      const creature_plan_actions::Executor& executor, glm::vec2 point,
	                                                      const creature_plan_actions::Situation& situation);
	/// The places of the creature's map of the land that the action uses: where it goes or looks, and the sea
	static void SituateExploration(entt::entity creature, const creature_plan_actions::Executor& executor,
	                               creature_plan_actions::Situation& situation);
	/// The creature carried out an action to its end on this game turn
	static void NoteDone(components::CreatureMindState& mind, uint32_t action);
	/// An agenda that no plan gave ends: its action was done just now, and the other actions of its desire get older
	void EndUnderway(components::CreatureMindState& mind);
	/// Where its home and its player's temple are, and whether another creature is near enough to frighten it
	void SituateHome(entt::entity creature, creature_plan_actions::Situation& situation);
	/// The agenda to go into its temple makes the creature believe in the temple's heart
	void BelieveInTempleToEnter(entt::entity creature, const creature_plan_actions::Executor& executor,
	                            const creature_plan_actions::Situation& situation);
	/// The places that the creature uses when it hangs around at home: its home and its player's temple, both on the
	/// ground, the radius of the area that holds it, and its height
	[[nodiscard]] static creature_mind::HangAroundPlaces HangAroundPlacesOf(entt::entity creature);

	/// Plans an activity in place of what the creature was doing: it stops walking and gets up out of a pose first, while
	/// an action it is playing plays on and the new agenda waits for it
	bool Replan(entt::entity creature, creature_mind::Activity activity, std::vector<creature_mind::Step> agenda);
	/// The creature sees a miracle it reacted to, and learns from it
	void WatchMiracle(entt::entity creature, size_t miracle);
	/// The creature counts one sighting of the miracle, and the advisor tells what the sighting showed
	void CountMiracleSighting(entt::entity entity, const components::Creature& creature, components::CreatureMindState& mind,
	                          size_t miracle);
	/// A creature grown up enough casts the one-shot globe that it was given. Whether it does.
	bool CastGivenGlobe(entt::entity creature, components::CreatureMindState& mind, entt::entity globe);
	/// The creature watches a skill being done, and learns from it
	void WatchSkill(entt::entity creature, size_t skill);
	/// The creature sees again a villager it believes in: grown up enough and free to, it learns from what the villager
	/// does when that teaches a skill it does not know, and goes after the villager to watch it. A villager of another
	/// player's village, or of a village of no player, also makes it want to impress more.
	void WatchVillager(entt::entity creature, components::CreatureMindState& mind, entt::entity villager);
	/// Seeing a thing again that it believes in stirs some of its desire sources
	void StirSourcesOnSighting(entt::entity creature, components::CreatureMindState& mind,
	                           creature_object_beliefs::Belief& belief);
	/// The skill a villager seen again shows the creature, if it does not know it yet
	void WatchVillagerSkill(entt::entity creature, components::CreatureMindState& mind, entt::entity villager);
	/// A sighting that moves the learning meter shows it over the creature's head, at most every so many turns
	void ShowLearningMeter(entt::entity creature, components::CreatureMindState& mind, std::optional<float> share);
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
	[[nodiscard]] std::optional<entt::entity> WholeOfShownThing(entt::entity creature, components::CreatureMindState& mind,
	                                                            entt::entity thing);
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
	/// A creature puts the water of five buckets on a thing (CreatureMindFire.cpp). Return true if the thing is not on
	/// fire after this. Only the plain "put out fire" action has this step. The water miracle at a fire does not have it.
	bool Douse(entt::entity creature, entt::entity object);
	/// A stomp that ends the agenda of a creature (CreatureMindStomp.cpp). When the thing has no life after it, the
	/// creature counts the kill, and the action sees to its desire.
	void StompedToDeath(entt::entity creature, components::CreatureMindState& mind, entt::entity thing);
	// Its dances with the villagers of a town (CreatureMindDance.cpp)
	/// The creature's dance as its mind sees it, while a dance step is next or under way. It notes where each dancer
	/// stands, to tell next turn which dancers move.
	[[nodiscard]] static std::optional<creature_mind::Senses::DanceView> DanceViewOf(entt::entity creature,
	                                                                                 const creature_mind::IdleMind& idle);
	/// Carries out a stage of a dance with villagers that the mind ordered
	static void CarryOutDance(entt::entity creature, creature_mind::IdleMind& idle, const creature_mind::DanceOrder& order);
	/// The creature's dance stops when the creature stops dancing in it, as when it gives up its agenda
	static void KeepDanceWithAgenda(entt::entity creature, const creature_mind::IdleMind& idle);
	/// The clear place of a dance with a villager, about the villager; none when there is none
	[[nodiscard]] static std::optional<glm::vec2> DanceAreaFor(entt::entity creature, glm::vec2 villager);
	/// A dance with a villager is about to be planned: the creature notes the dance as one to impress, and the agenda
	/// learns the clear place about the villager and what the dance's row of the table asks for
	static void SituateDance(entt::entity creature, const creature_plan_actions::Executor& executor, glm::vec2 villager,
	                         creature_plan_actions::Situation& situation);
	/// The creature starts to make a dance that it notes: it notes the turn and the row of the dance
	static void NoteImpressiveDance(entt::entity creature, uint32_t type);
	/// Whether the creature may choose a dance of a row of the dance table now
	[[nodiscard]] static bool MayDanceAgain(entt::entity creature, uint32_t type);

	// What it believes about the things about it (CreatureMindBeliefs.cpp)
	/// It comes across a thing: its belief about it is brought up to date, or made, and a new one leads it on to what the
	/// thing belongs to. If the thing moves and is not the creature itself, it becomes the last moving thing that the
	/// creature noticed.
	void ComeAcross(entt::entity creature, components::CreatureMindState& mind, entt::entity thing);
	/// Where it believes a thing is: where it last came across it; nothing when it has never come across it
	[[nodiscard]] static std::optional<glm::vec3> BelievedPlace(const components::CreatureMindState& mind, entt::entity thing);
	/// It forgets what it believed about things gone from the world, and its last try that fizzled when the thing of
	/// that try goes
	void ForgetGoneThings(entt::entity creature, components::CreatureMindState& mind);
	/// Gone into another map cell, it comes across the temple nearest it if that is another than the last
	void NoteNearestTemple(entt::entity creature, components::CreatureMindState& mind, const components::Transform& transform);
	/// Its eyes sweep this turn's cells about it, and it comes across everything in those it can see
	void SweepEyes(entt::entity entity, const components::Creature& creature, components::CreatureMindState& mind,
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
	/// The miracle that a creature casts to impress, none when it knows none. It notes the turn of the cast for every
	/// creature.
	std::optional<uint32_t> PickImpressiveMiracle(entt::entity creature);
	std::optional<creature_plan_actions::CastInfo> CastInfoFor(entt::entity creature, uint32_t action,
	                                                           std::optional<entt::entity> target);
	/// How often the creature has seen a miracle and how often it needs to have seen it to cast it
	[[nodiscard]] std::optional<std::pair<float, float>> MiracleSightings(entt::entity creature, MagicType type);
	/// A fizzled try, shown as it next chooses what to do
	void ShowFizzle(entt::entity creature, components::CreatureMindState& mind);
	/// The creature tries a miracle at a place, as its casting pose's loop begins. A try short of the sightings that it
	/// needs fizzles. A miracle that it cannot cast there frustrates it. Return true if it cast the miracle.
	bool TryMiracleAt(entt::entity creature, MagicType type, glm::vec3 place);
	/// The part of a try that is the same for a thing and for a place. Return true if the creature can continue.
	bool StartTry(entt::entity creature, MagicType type, std::optional<entt::entity> target);
	/// The creature could not cast the miracle: it is frustrated and wants food above all
	void FrustrateAfterCast(entt::entity creature, MagicType type);
	/// Does the thing that waited for the creature to complete its work
	void DoFollowUp(entt::entity creature, components::CreatureMindState& mind, creature_follow_up::FollowUp followUp);
	/// The first turn of the route search stopped far short of the destination. The creature clears one of the things
	/// that the search met, then it walks on to the destination. Returns whether it starts to clear the way
	/// (CreatureMindClearWay.cpp).
	bool ClearWay(entt::entity creature, components::CreatureMindState& mind, glm::vec2 destination,
	              std::span<const uint32_t> met);
	/// The creature cleared its way and walks on to the destination
	void WalkOn(entt::entity creature, const components::CreatureMindState& mind);
	/// Each turn: the mark of a creature that clears its way stays while the plan that clears it runs. The movement of
	/// the creature gets the mark.
	void StepClearingWay(entt::entity creature, components::CreatureMindState& mind);
	/// The creature finds no way to where it goes: it makes a sound of refusal
	void RefuseNoRoute(entt::entity creature);
	/// A run away from a thing (none for the player's hand) stopped. Far enough from it, the run sees to the desire.
	void EndRunAway(entt::entity creature, components::CreatureMindState& mind, std::optional<uint32_t> object);
	/// A stroke, or the end of the meal of the lesson to eat what it holds: the creature must do again the action that
	/// feedback goes to now, to the most useful thing of the same kind. This can be the same thing again.
	void DoLessonAgain(entt::entity creature, components::CreatureMindState& mind);
	/// A player did a deed, maybe with a miracle (0 for none), that creatures can copy (CreatureMindLearning.cpp)
	void NoticeDeed(size_t deed, const glm::vec3& point, std::optional<entt::entity> object, std::optional<PlayerNames> player,
	                uint32_t magic);
	/// When it starts to copy a deed on a thing, the creature learns that the thing and its whole thing are good for the
	/// desire that the player seemed to have, as half a stroke would teach it (CreatureMindLearning.cpp)
	void LearnFromDeedSeen(entt::entity creature, components::CreatureMindState& mind, size_t deed, entt::entity thing,
	                       uint32_t magic);
	/// The creature chose to copy the player: it copies the deed that it saw last again (CreatureMindLearning.cpp)
	void CopyObservedDeedAgain(entt::entity creature, components::CreatureMindState& mind);
	/// It is made to be high, as after eating a magic mushroom
	void BecomeHigh(entt::entity creature);
	/// Going near, getting away from or turning to face an object begins, and each turn it goes on
	void StartSubMove(entt::entity creature, const creature_mind::Movement& movement, float seconds);
	void StepSubMove(entt::entity creature, bool animating);
	bool GoNear(entt::entity creature, components::CreatureCasting& casting, bool reissue);
	creature_mind::SubMove SubMoveOf(entt::entity creature);
	/// The minds choose at random, apart from the game's own random numbers
	RandomStreamSource _random {RandomStream::CreatureMind};
	/// The average of the feedback given to all creatures
	float _averageFeedback {0.0f};
	/// The game's tables for the minds, taken once the game's data is loaded
	std::optional<creature_mind_tables::Tables> _tables;
};

} // namespace openblack::ecs::systems
