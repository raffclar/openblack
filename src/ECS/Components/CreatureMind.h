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

#include "Creature/CreatureDesires.h"
#include "Creature/CreatureIdleMind.h"
#include "Creature/CreatureLook.h"
#include "Creature/CreatureMindModel.h"
#include "Creature/CreatureObjectBeliefs.h"
#include "Creature/CreaturePlanner.h"
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
	/// Whether the head turns to what it watches this turn
	bool lookingAbout {false};
	/// What a script has it keep its head turned to, whatever it is doing, until the script lets it go: a thing (looked
	/// at where it stands) or a point
	std::optional<creature_mind::Gaze> scriptFocus;

	/// Seconds since the player last paid it any attention
	float secondsAlone {0.0f};
	/// Game turns its player's camera was looking at it, which over the turns it has lived is the attention its god has
	/// paid it
	uint32_t turnsLookedAt {0};
	/// What it believes about the things it has come across, and how far its eyes have swept the cells about it
	creature_object_beliefs::Store beliefs;
	creature_object_beliefs::Sweep sweep {};
	/// The map cell it stood in when it last looked for the nearest temple, which it does again each time it goes into
	/// another cell, and the temple that was nearest then
	std::optional<glm::ivec2> beliefCell;
	entt::entity nearestTemple {entt::null};
	/// Seconds since the player last stroked or slapped it, and which
	std::optional<float> feedbackSeconds;
	bool feedbackWasStroke {false};
	/// How it feels about the player, which each stroke or slap nudges, and the average of what it has been given
	float attitudeToPlayer {0.0f};
	float averageFeedback {0.0f};
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
	/// The kinds of thing it has been given from the hand with no desire for them, which it looked over the first time
	std::set<uint32_t> givenKinds;
	/// The food it took from a pile and is picking up, and the pile, which gives the food up once it has picked it up
	entt::entity takenFood {entt::null};
	entt::entity takenFrom {entt::null};
	/// The agenda carrying out the plan, and the last agenda remembered for feedback, by the idle mind's count of them
	uint32_t planSerial {0};
	uint32_t agendaSeen {0};
	/// Turns its agenda's steps have run, counted on from step to step, which a walk up to something checks every fifty
	/// for having got stuck
	uint32_t stepTurns {0};
	/// Game turns the mind has thought, and the turn it last planned
	uint32_t turn {0};
	uint32_t plannedTurn {0};
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
	/// How often it has been told to hold things of each kind (by the game's belief types): it plays with what it holds
	/// only the first times
	std::map<uint32_t, uint32_t> heldKinds;
	/// Where the thing stood that the player did something to which the creature last set about copying (the land's
	/// corner until then): it takes no tree from near there to plant
	glm::vec2 copiedAt {0.0f};
	/// The kind for copying of that thing. A deed on no thing keeps the kind from before. None before the first deed.
	std::optional<creature_watching::MimicKind> copiedKind;
	/// It ate a magic mushroom, and becomes high once it is done with what it is doing
	bool highPending {false};
	/// The trees it planted others by most recently, by entity number, oldest first: it never takes one of them to plant
	std::vector<uint32_t> plantedBy;
};

} // namespace openblack::ecs::components
