/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <map>
#include <memory>
#include <optional>
#include <set>
#include <vector>

#include <MindFile.h>
#include <entt/entity/entity.hpp>
#include <glm/vec2.hpp>

#include "Creature/CreatureDesireSources.h"
#include "Creature/CreatureDesires.h"
#include "Creature/CreatureEmoteAgendas.h"
#include "Creature/CreatureFollowUp.h"
#include "Creature/CreatureIdleMind.h"
#include "Creature/CreatureLook.h"
#include "Creature/CreatureMindModel.h"
#include "Creature/CreatureObjectBeliefs.h"
#include "Creature/CreaturePlanner.h"
#include "Creature/CreaturePlayerAttention.h"
#include "Creature/CreatureScriptPlay.h"
#include "Creature/CreatureTownCompassion.h"
#include "Creature/LeashRules.h"
#include "Creature/PerceivedDesires.h"
#include "Enums.h"

namespace openblack::ecs::components
{

/// What a creature wants, what it is doing about it, and what it looks at
struct CreatureMindState
{
	/// The last stage of growing up
	static constexpr uint32_t k_FullyGrownUp = 13;

	/// The desires, set up from the species' tables when the mind first thinks
	std::optional<creature_desires::Desires> desires;
	creature_mind::IdleMind idle {};
	/// What a script last gave it to play, kept until a script tells it to start
	creature_script_play::Request scriptPlay {};
	creature_look::Target look {};
	/// The look while it walks to a place, started from the creature info at the first walk
	std::optional<creature_look::WhileGoing> goingLook;
	/// The last moving thing that it noticed as it came across things, for the look while it walks. The turn is a turn of
	/// this mind.
	std::optional<creature_look::NoticedMover> noticedMover;
	/// Whether the head turns to what it watches this turn
	bool lookingAbout {false};
	/// What a script has it keep its head turned to, whatever it is doing, until the script lets it go: a thing (looked
	/// at where it stands) or a point
	std::optional<creature_mind::Gaze> scriptFocus;

	/// The personality it was born with, set with the desires
	creature_desire_sources::InnateTraits innate {};
	/// How much attention the player pays it, and how interesting it finds the player
	creature_desire_sources::Attention attention {};
	creature_desire_sources::PlayerInterest playerInterest {};
	/// How fast the player's hand moves, which follows the hand by a part each turn
	creature_player_attention::HandSpeed handSpeed {};
	/// It has seen villages of other players, or of no player: a source of wanting to impress
	float impressFromSeeing {0.0f};
	/// Game turns its player's camera was looking at it, which over the turns it has lived is the attention its god has
	/// paid it
	uint32_t turnsLookedAt {0};
	/// What it believes about the things it has come across, and how far its eyes have swept the cells about it
	creature_object_beliefs::Store beliefs;
	creature_object_beliefs::Sweep sweep {};
	/// The town and the temple that it came across last as it looked far about it
	entt::entity lastTown {entt::null};
	entt::entity nearestTemple {entt::null};
	/// Seconds since the player last stroked or slapped it, and which. A new mind counts from the time it was made, as if
	/// it had feedback then.
	std::optional<float> feedbackSeconds {0.0f};
	bool feedbackWasStroke {false};
	/// How it feels about the player, which each stroke or slap nudges
	float attitudeToPlayer {0.0f};
	/// What it thinks its player wants, from what it has seen the player do
	creature_perceived_desires::PerceivedDesires perceivedDesires {};
	/// The last feedback the player gave it as the hand let go, from -1 (slapped) to 1 (stroked), and what it was doing
	/// then, for it to learn from
	struct Feedback
	{
		float value;
		creature_mind::Activity activity;
	};
	std::optional<Feedback> lastFeedback;
	/// How strongly the player last rewarded (above 0) or punished (below 0) it: the feedback the hand let go with when
	/// it was more than slight, or a blow from something its own player threw. Scripts read it, and one sets it back to 0
	/// to wait for the next.
	float interactionMagnitude {0.0f};
	/// How many times it has carried out each action of the game's table to its end, by the action's number, for the
	/// scripts to read; and the action of what the idle mind is carrying out, by its count of agendas
	std::map<uint32_t, uint32_t> actionCounts;
	struct ActionUnderway
	{
		uint32_t serial;
		uint32_t action;
	};
	std::optional<ActionUnderway> underway;
	/// The action it last carried out to its end, by its number, which the towns watching it judge it by
	std::optional<uint32_t> lastActionDone;

	/// How far the creature has grown up, which decides the desires it has and which of its body's needs it feels, and
	/// the stage they were last set for. Until the game's story moves it on, a creature starts fully grown up.
	uint32_t developmentPhase {k_FullyGrownUp};
	std::optional<uint32_t> desiresPhase;
	/// A lesson script of growing up is running for it, from when the script says so until it says it is over. The
	/// game's help about the creature waits meanwhile, except the help about its stage.
	bool inDevelopmentScript {false};

	/// While paused, the mind leaves the body alone to be posed by hand
	bool paused {false};
	/// The creature can be angry with the things of its own player. At first it cannot. It can after it starts to copy
	/// the player in an angry action on a thing of the player.
	bool mayAttackOwnPlayer {false};
	/// The player's hand took hold of it: once it has turned to the player it shows what it wants, and the help says it
	bool showDesireOnHold {false};
	/// The hand held it since the last time a plan started again after a hold
	bool heldSinceRestart {false};
	/// The feedback of the last release of the hand, which reaches the mind after the turns that are left
	std::optional<float> releaseFeedback;
	uint32_t releaseFeedbackTurns {0};
	/// A plan of its own ran since it last stopped what it did, and the turns of the copy timeout with no such plan
	bool ownPlanSinceStop {false};
	uint32_t turnsWithoutOwnPlan {0};
	/// An order forced the plan. A forced plan counts at its end only when the order said so.
	bool planForced {false};
	bool forcedPlanCounts {false};
	/// The leash leads the plan: an order on the leash, or a walk back into its area while the leash is on. A forced
	/// plan that the leash leads does not stop the creature from taking up reactions. The mark goes when the creature
	/// stops what it does, or when the leash comes off.
	bool ledByLeash {false};
	/// An emergency started, and the creature did not stop what it does since then. It does not react to things then.
	bool emergencyRunning {false};

	/// What it has learnt and remembers, set up with the desires (from its mind file when it has one)
	std::optional<creature_mind_model::Learnt> learnt;
	/// The plans it weighs, and the one it carries out
	creature_planner::PlannerState planner {};
	/// Which of a town's desires its compassion helps with
	creature_town_compassion::State townCompassion {};
	/// The agenda carries out the planner's plan; the step that ends it took down the desire already
	bool planActive {false};
	bool satisfiedByEffect {false};
	/// A step of the plan saw to what its desire wanted, as putting out a fire does once it is out: the desire is less
	/// when the plan ends, even for an action that doesn't lessen it by itself
	bool desireSeenTo {false};
	/// The kinds of thing (as its beliefs sort them) it has had a good look at, which it is far less curious about since
	std::set<uint32_t> observedKinds;
	/// The plan that it did when it last started a reaction, or when the hand last gave it a thing while it had no
	/// reaction. When it takes a thing from the hand, it continues this plan with the thing if the thing suits the plan.
	std::optional<creature_planner::Plan> keptPlan;
	/// The sorts of object (by the object type of their info) that it brought up after it ate one. It does not eat
	/// these again.
	std::set<uint32_t> refusedFoodSorts;
	/// The food it took from a pile and is picking up, and the pile, which gives the food up once it has picked it up
	entt::entity takenFood {entt::null};
	entt::entity takenFrom {entt::null};
	/// The agenda carrying out the plan, and the last agenda remembered for feedback, by the idle mind's count of them
	uint32_t planSerial {0};
	uint32_t agendaSeen {0};
	/// Turns its agenda's steps have run, counted on from step to step, which a walk up to something checks every fifty
	/// for having got stuck
	uint32_t stepTurns {0};
	/// The turns that the agenda (by the idle mind's count of them) has run steps for
	uint32_t agendaTurns {0};
	uint32_t agendaTurnsSerial {0};
	/// The agenda (by the idle mind's count of them) that the creature last set its kept speed for
	uint32_t paceSerial {0};
	/// The actions of the desire of the plan that it carries out, in the order of the list that the plan came from; and
	/// for each desire, the list that its last plan came from
	std::vector<uint32_t> planActions;
	std::array<std::vector<uint32_t>, creature_desires::k_DesireCount> plannedActions {};
	/// The game turn that it last carried out each action to its end, 0 for never
	std::vector<uint32_t> lastDoneTurn;
	/// Running away, it looks back at what it runs from in turns
	creature_emote::RunLookBack runLookBack {};
	/// Game turns the mind has thought, and the turn it last planned
	uint32_t turn {0};
	uint32_t plannedTurn {0};
	/// The turn the learning meter last showed over it
	uint32_t lastMeterTurn {0};
	/// The action that gets the feedback for an agenda that no plan gave, when it is not the action of the agenda's
	/// activity, and the number of that agenda
	std::optional<uint32_t> creditedAction;
	uint32_t creditedSerial {0};
	/// The times it thought and had no plan to carry out since its last agenda started, and that agenda's number
	uint32_t thinksWithoutPlan {0};
	uint32_t thinksWithoutPlanSerial {0};
	/// The trainer of the debug tools: each thing it does to something, once done, it is stroked for if the thing is of
	/// this kind (by the game's belief types) and slapped for otherwise
	std::optional<uint32_t> trainer;
	/// A mind file to take up at the next turn, from the creature's mind resource or the debug tools; whether the
	/// resource has been looked at
	std::shared_ptr<const creaturemind::MindFileData> pendingFile;
	bool resourceChecked {false};
	/// What scripts taught it or made it forget before its learning was set up or while a mind file waits, kept to be
	/// applied in order over what it learns from the file
	struct Teaching
	{
		CreatureActionLearningType type {CreatureActionLearningType::Normal};
		uint32_t action {0};
		bool knows {false};
	};
	std::vector<Teaching> pendingTeaching;

	/// What the leash tells the mind: the desire it forces, whether the creature is following it to the hand, and what
	/// the player has shown it on the leash
	creature_leash::MindHooks leash {};
	/// A count for each sort of object (by the object type of its info). The count goes up by one each time that the
	/// creature plays with a thing that it holds. The creature plays only while the count is less than two. A gift from
	/// the hand with no desire for it sets a count of zero to one, and the creature looks over that first gift.
	std::map<uint32_t, uint32_t> heldSortCounts;
	/// Where the thing stood that the player did something to which the creature last set about copying (the land's
	/// corner until then): it takes no tree from near there to plant
	glm::vec2 copiedAt {0.0f};
	/// The kind for copying of that thing. A deed on no thing keeps the kind from before. None before the first deed.
	std::optional<creature_watching::MimicKind> copiedKind;
	/// The one thing that it does when it completes its current work, in place of the next decision
	std::optional<creature_follow_up::FollowUp> followUp;
	/// A number that the thing that waits uses: after a custom music track, ten times the track's mood
	int32_t followUpValue {0};
	/// The trees it planted others by most recently, by entity number, oldest first: it never takes one of them to plant
	std::vector<uint32_t> plantedBy;
	/// The last deed of the player that it started to copy. It copies this deed again when it chooses to copy the player
	/// for its desire to follow the player. None until it starts to copy a deed.
	std::optional<creature_watching::ObservedDeed> observedDeed;
	/// While the creature clears a thing out of its way: the serial of the plan that clears it. None at other times.
	std::optional<uint32_t> clearingWaySerial;
	/// Where the creature walks on to after it clears a thing out of its way
	glm::vec2 walkOnTo {0.0f};
};

} // namespace openblack::ecs::components
